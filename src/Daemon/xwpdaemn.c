
/*
 *@@sourcefile xwpdaemn.c:
 *      this has the code for the XWorkplace Daemon process
 *      (XWPDAEMN.EXE). This is an invisible PM process which
 *      does not appear in the window list, but is only seen
 *      by the XWorkplace main DLL (XFLDR.DLL).
 *
 *      The daemon is started automatically by XFLDR.DLL upon
 *      first WPS startup (details follow). Currently, the
 *      daemon is responsible for the following:
 *
 *      1.  Installation and configuration of the XWorkplace
 *          PM hook (XWPHOOK.DLL, xwphook.c).
 *
 *          Since XWPHOOK.DLL may only be interfaced by one
 *          process, it is the daemon's responsibility to
 *          configure the hook. When any data changes which
 *          is of importance to the hook, XFLDR.DLL writes
 *          that data to OS2.INI and then posts a message to
 *          the daemon's object window (fnwpDaemonObject).
 *          The daemon then reads the new data and notifies
 *          the hook. See xwphook.c for details.
 *
 *          Note: If you wish to configure the hook or daemon,
 *          NEVER call the hook or the daemon directly.
 *          I have created proper interfaces for everything
 *          you can think of in config/hookintf.c, which
 *          also has the new notebook pages in the "Mouse" and
 *          "Keyboard" objects.
 *
 *      2.  Keeping track of WPS restarts, since the daemon
 *          keeps running even while the WPS restarts. Shared
 *          memory is used for communication.
 *
 *          The interaction between XWPDAEMN.EXE and XFLDR.DLL works
 *          as follows:
 *
 *          a) After system startup, PM starts the program specified
 *             in RUNWORKPLACE in CONFIG.SYS, which should be the
 *             WPS (PMSHELL.EXE).
 *
 *          b) When the WPS is started for the first time (after
 *             a reboot), krnInitializeXWorkplace (XFLDR.DLL)
 *             checks for whether a DAEMONSHARED structure (xwphook.h)
 *             has already been allocated as a block of shared memory.
 *
 *             At system startup, this is not the case. As a result,
 *             krnInitializeXWorkplace allocates that block and then
 *             starts XWPDAEMN.EXE using WinStartApp.
 *
 *          c) The daemon then requests access to that block of
 *             shared memory, creates the daemon object window
 *             (fnwpDaemonObject) and posts T1M_DAEMONREADY to
 *             krn_fnwpThread1Object in XFLDR.DLL.
 *
 *             krn_fnwpThread1Object will then send XDM_HOOKINSTALL
 *             to the daemon object window to install the hook (if
 *             the hook is enabled in XWPSetup). If PageMage has
 *             been enabled also, XDM_STARTSTOPPAGEMAGE will also
 *             be sent to start PageMage (see below).
 *
 *             After the Desktop has been opened, XFLDR.DLL will
 *             post XDM_DESKTOPREADY to the Daemon because both
 *             the hook and PageMage will need the window handle
 *             of the currently active Desktop.
 *
 *             This situation persists while the WPS is running
 *             (unless the hook is explicitly disabled by the user or
 *             the daemon gets killed). If any configuration data
 *             changes, XFLDR.DLL notifies the daemon by posting
 *             messages to the daemon's object window (fnwpDaemonObject),
 *             which in turn notifies the hook.
 *
 *          d) If the WPS is restarted for any reason (because
 *             of a crash or explicitly by the user), the daemon
 *             (and thus the hook) is not terminated, but stays
 *             active. Since OS/2 maintains a reference count
 *             for shared memory, the DAEMONSHARED structure
 *             is not freed, because the daemon is still using
 *             it.
 *
 *          e) When the WPS is re-initializing, krnInitializeXWorkplace
 *             this time realizes that the DAEMONSHARED structure is
 *             still in use and will thus find out that the WPS is not
 *             started for the first time. Instead, access to the
 *             existing DAEMONSHARED structure is requested (so
 *             we have a reference count of two again).
 *
 *             XFLDR.DLL then posts the new Desktop window handle with
 *             XDM_DESKTOPREADY so the daemon data can be updated.
 *
 *             After the Desktop is up, if DAEMONSHARED.fProcessStartupFolder
 *             is TRUE (which has been set to this by XShutdown if the
 *             respective checkbox was checked), the XWorkplace
 *             startup folder is processed again.
 *
 *          In short, at any time after the WPS is starting for
 *          the first time, at least one process is using the
 *          DAEMONSHARED structure: either the WPS process (with
 *          XFLDR.DLL) or XWPDAEMON.EXE or both. The structure
 *          only gets freed if both processes are killed, which
 *          is not probable under normal circumstances.
 *
 *          This new approach has several advantages:
 *
 *          a)  The XWorkplace startup folder finally works all the
 *              time, which was not the case with versions < 0.9.0.
 *
 *          b)  The hook does not get re-installed at every WPS
 *              restart, which saves PM resources. Apparently, system
 *              hooks can only be installed a limited number of times
 *              before the system hangs (I get around 10-15 tries).
 *
 *          c)  While programming on the hook, I can simply restart
 *              the daemon to have the hook reloaded, instead of
 *              having to restart the WPS all the time.
 *
 *      3.  The PageMage window for virtual desktops. See pgmg_control.c
 *          for details.
 *
 *      4.  Removeable drives monitoring. I have put this into
 *          the daemon because we're dealing with DevIOCtl's
 *          here, which might not be a good idea to do in the
 *          PMSHELL.EXE process.
 *
 *      Both the hook and the daemon are all new with V0.9.0.
 *
 *@@added V0.9.0 [umoeller]
 *@@header "hook\xwphook.h"
 *@@header "hook\xwpdaemn.h"
 */

/*
 *      Copyright (C) 1999-2000 Ulrich M”ller.
 *      Copyright (C) 1993-1999 Roman Stangl.
 *      Copyright (C) 1995-1999 Carlos Ugarte.
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, in version 2 as it comes in the COPYING
 *      file of the XWorkplace main distribution.
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 */

#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#define INCL_DOSEXCEPTIONS
#define INCL_DOSDEVICES
#define INCL_DOSERRORS

#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WINPOINTERS
#define INCL_WINMENUS
#define INCL_WINTIMER
#define INCL_WINMESSAGEMGR
#define INCL_WININPUT
#define INCL_WINSHELLDATA
#define INCL_WINSYS
#define INCL_WINWORKPLACE
#include <os2.h>

#include <stdio.h>
#include <time.h>
#include <setjmp.h>

#include "setup.h"                      // code generation and debugging options

#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\threads.h"
// #include "helpers\winh.h"               // PM helper routines

#include "hook\xwphook.h"               // hook and daemon definitions
#include "hook\hook_private.h"          // private hook and daemon definitions
#include "hook\xwpdaemn.h"              // PageMage and daemon declarations

#include "bldlevel.h"

#include "shared\center.h"              // public XCenter interfaces
#include "shared\kernel.h"              // XWorkplace Kernel

/* ******************************************************************
 *                                                                  *
 *   Global variables                                               *
 *                                                                  *
 ********************************************************************/

ULONG           G_pidDaemon = NULLHANDLE;
            // process ID of XWPDAEMN.EXE
HAB             G_habDaemon;
HMQ             G_hmqDaemon;
            // anchor block and HMQ of thread 1

// pointer to hook data in hook dll;
// this includes HOOKCONFIG
PHOOKDATA       G_pHookData = NULL;
// pointer to shared-memory daemon globals
PDAEMONSHARED   G_pDaemonShared = NULL;

// daemon icon from resources
HPOINTER        G_hptrDaemon = NULLHANDLE;

// sliding focus data
ULONG           G_ulSlidingFocusTimer = 0;    // timer ID for delayed sliding focus

// sliding menu data
ULONG           G_ulSlidingMenuTimer = 0;
MPARAM          G_SlidingMenuMp1Saved = 0;
            // MP1 of WM_MOUSEMOVE posted back to hook; added for checking

// drive monitor
ULONG           G_ulMonitorTimer = 0;

// PageMage
HWNDLIST        G_MainWindowList[MAX_WINDOWS] = {0};
            // array of window data managed by PageMage
USHORT          G_usWindowCount = 0;
            // count of array items currently in use
HMTX            G_hmtxWindowList = 0;
            // mutex sem protecting that array

HWND            G_hwndPageMageClient = NULLHANDLE;

CHAR            G_szFacename[PGMG_TEXTLEN] = "";

POINTL          G_ptlCurrPos = {0};
POINTL          G_ptlPgmgClientSize = {0};
POINTL          G_ptlEachDesktop = {0};
SWP             G_swpPgmgFrame = {0};

BOOL            G_bConfigChanged = FALSE;

THREADINFO      G_tiMoveThread = {0};

#define IOCTL_CDROMDISK             0x80
#define CDROMDISK_DEVICESTATUS      0x60
#define CDROMDISK_GETUPC            0x79

const char *WNDCLASS_DAEMONOBJECT = "XWPDaemonObject";

/* ******************************************************************
 *                                                                  *
 *   Drive monitoring                                               *
 *                                                                  *
 ********************************************************************/

int CheckRemoveableDrive(void)
{
    APIRET  arc;
    HFILE   hFile;
    ULONG   ulTemp;

    arc = DosOpen("X:",   // <--- CD drive
                  &hFile,
                  &ulTemp,
                  0,
                  FILE_NORMAL,
                  OPEN_ACTION_FAIL_IF_NEW
                         | OPEN_ACTION_OPEN_IF_EXISTS,
                  OPEN_FLAGS_DASD
                         | OPEN_FLAGS_FAIL_ON_ERROR
                         | OPEN_ACCESS_READONLY
                         | OPEN_SHARE_DENYNONE,
                  NULL);
    if (arc == NO_ERROR)
    {
        UCHAR   achUnit[4] = { 'C', 'D', '0', '1' };
        struct
        {
            UCHAR   uchCtlAdr;
            UCHAR   achUPC[7];
            UCHAR   uchReserved;
            UCHAR   uchFrame;
        } Result;

        ULONG ulDeviceStatus = 0;

        ulTemp = 0;

        arc = DosDevIOCtl(hFile,
                          IOCTL_CDROMDISK,
                          CDROMDISK_GETUPC,
                          achUnit, sizeof(achUnit), NULL,
                          &Result, sizeof(Result), &ulTemp );
        if (arc == NO_ERROR)
        {
            CHAR szUPC[100];
            sprintf(szUPC,
                    "UPC: %02x%02x%02x%02x%02x%02x%02x",
                    Result.achUPC[0], Result.achUPC[1], Result.achUPC[2],
                    Result.achUPC[3], Result.achUPC[4], Result.achUPC[5], Result.achUPC[6]);
            _Pmpf(("New ADR: 0x%lX", Result.uchCtlAdr));
            _Pmpf(("New UPC: %s", szUPC));
        }
        else
            _Pmpf(("DosDevIOCtl rc: %d", arc));

        arc = DosDevIOCtl(hFile,
                          IOCTL_CDROMDISK,
                          CDROMDISK_DEVICESTATUS,
                          achUnit, sizeof(achUnit), NULL,
                          &ulDeviceStatus, sizeof(ulDeviceStatus), &ulTemp);
        if (arc == NO_ERROR)
        {
            _Pmpf(("ulStatus: 0x%lX", ulDeviceStatus));
            _Pmpf(("  CDDA support:  %s", (ulDeviceStatus & (1<<30)) ? "yes" : "no"));
            _Pmpf(("  Playing audio: %s", (ulDeviceStatus & (1<<12)) ? "yes" : "no"));
            _Pmpf(("  Disk present:  %s", (ulDeviceStatus & (1<<11)) ? "yes" : "no"));
            _Pmpf(("  Door open:     %s", (ulDeviceStatus & (1    )) ? "yes" : "no"));
        }
        else
            _Pmpf(("DosDevIOCtl rc: %d", arc));

        DosClose(hFile);
    }
    else
        _Pmpf(("Open failed !\n"));

    return 0;
}

/* ******************************************************************
 *                                                                  *
 *   Exception hooks (except.c)                                     *
 *                                                                  *
 ********************************************************************/

/*
 *@@ dmnExceptOpenLogFile:
 *      hook function for the exception handlers (except.c).
 *
 *@@added V0.9.4 (2000-07-11) [umoeller]
 */

FILE* _System dmnExceptOpenLogFile(VOID)
{
    CHAR        szFileName[CCHMAXPATH];
    FILE        *file;
    DATETIME    DT;

    sprintf(szFileName, "%c:\\%s", doshQueryBootDrive(), XFOLDER_DMNCRASHLOG);
    file = fopen(szFileName, "a");

    if (file)
    {
        DosGetDateTime(&DT);
        fprintf(file, "\nXWorkplace Daemon trap message -- Date: %04d-%02d-%02d, Time: %02d:%02d:%02d\n",
            DT.year, DT.month, DT.day,
            DT.hours, DT.minutes, DT.seconds);
        fprintf(file, "------------------------------------------------------------------\n"
                      "\nAn internal error occurred in the XWorkplace Daemon (XWPDAEMN.EXE).\n"
                      "Please contact the author so that this error may be removed\n"
                      "in future XWorkplace versions. A contact address may be\n"
                      "obtained from the XWorkplace User Guide. Please supply\n"
                      "this file (?:\\" XFOLDER_DMNCRASHLOG " with your e-mail and describe as\n"
                      "exactly as possible the conditions under which the error\n"
                      "occured.\n"
                      "\nRunning XWorkplace version: V" BLDLEVEL_VERSION " built " __DATE__ "\n");

    }
    return (file);
}

/*
 *@@ dmnExceptExplain:
 *      hook function for the exception handlers (except.c).
 *
 *@@added V0.9.4 (2000-07-11) [umoeller]
 */

VOID _System dmnExceptExplain(FILE *file,      // in: logfile from fopen()
                              PTIB ptib)       // in: thread info block
{
    // nothing so far
}

/*
 *@@ dmnExceptError:
 *      hook function for the exception handlers (except.c).
 *
 *@@added V0.9.4 (2000-07-11) [umoeller]
 */

VOID APIENTRY dmnExceptError(const char *pcszFile,
                             ULONG ulLine,
                             const char *pcszFunction,
                             APIRET arc)     // in: DosSetExceptionHandler error code
{
    CHAR        szFileName[CCHMAXPATH];
    FILE        *file;
    DATETIME    DT;

    sprintf(szFileName, "%c:\\%s", doshQueryBootDrive(), XFOLDER_DMNCRASHLOG);
    file = fopen(szFileName, "a");

    if (file)
    {
        DosGetDateTime(&DT);
        fprintf(file, "\nXWorkplace Daemon trap message -- Date: %04d-%02d-%02d, Time: %02d:%02d:%02d\n",
            DT.year, DT.month, DT.day,
            DT.hours, DT.minutes, DT.seconds);

        fprintf(file,
                "%s line %d (%s): TRY_* macro failed to install exception handler (APIRET %d)",
                pcszFile, ulLine, pcszFunction,
                arc);
        fclose(file);
    }
}

/* ******************************************************************
 *                                                                  *
 *   Hook interface                                                 *
 *                                                                  *
 ********************************************************************/

/*
 *@@ dmnStartPageMage:
 *      starts PageMage by calling pgmwScanAllWindows
 *      and pgmcCreateMainControlWnd and starting
 *      fntMoveQueueThread.
 *
 *      This gets called when XDM_STARTSTOPPAGEMAGE is
 *      received by fnwpDaemonObject.
 *
 *      Call dmnKillPageMage to stop PageMage again.
 *
 *      This function does not affect the operation of
 *      the daemon in general; it is only the PageMage
 *      which is affected. However, the hooks will
 *      notice this and process more messages.
 *
 *@@added V0.9.2 (2000-02-22) [umoeller]
 *@@changed V0.9.3 (2000-05-21) [umoeller]: fixed startup problems, added new thread flags
 */

BOOL dmnStartPageMage(VOID)
{
    BOOL brc = FALSE;

#ifdef __PAGEMAGE__

    if (G_pHookData)
        if (    (G_pHookData->fInputHooked)
             && (G_pHookData->fPreAccelHooked)
           )
        {
            // _Pmpf(("  Creating main window"));
            brc = pgmcCreateMainControlWnd();
               // this sets the global window handles;
               // the hook sees this and will start processing
               // PageMage messages
            // _Pmpf(("      returned %d", brc));

            // _Pmpf(("dmnStartPageMage: calling pgmwScanAllWindows"));
            pgmwScanAllWindows();

            // _Pmpf(("  starting Move thread"));
            thrCreate(&G_tiMoveThread,
                      fntMoveQueueThread,
                      NULL, // running flag
                      THRF_WAIT | THRF_PMMSGQUEUE,    // PM msgq
                      0);
                // this creates the PageMage object window
        }
#endif

    return (brc);
}

/*
 *@@ dmnKillPageMage:
 *      destroys the PageMage control window and
 *      stops the additional PageMage threads.
 *      This is the reverse to dmnStartPageMage and
 *      calls pgmmRecoverAllWindows in turn.
 *
 *      This gets called when XDM_STARTSTOPPAGEMAGE is
 *      received by fnwpDaemonObject.
 *
 *      This function does not affect the operation of
 *      the daemon in general; it is only the PageMage
 *      which is affected. However, the hooks will
 *      notice this and process fewer messages.
 *
 *@@added V0.9.2 (2000-02-22) [umoeller]
 *@@changed V0.9.3 (2000-04-25) [umoeller]: adjusted for new T1M_PAGEMAGECLOSED
 */

VOID dmnKillPageMage(BOOL fNotifyKernel)    // in: if TRUE, we post T1M_PAGEMAGECLOSED (TRUE) to the kernel
{
#ifdef __PAGEMAGE__
    if (G_pHookData->hwndPageMageFrame)
    {
        // PageMage running:
        ULONG   ulRequest;
        // save page mage frame
        HWND    hwndPageMageFrame = G_pHookData->hwndPageMageFrame;

        // stop move thread
        WinPostMsg(G_pHookData->hwndPageMageMoveThread, WM_QUIT, 0, 0);
        /* ulRequest = PGMGQENCODE(PGMGQ_QUIT, 0, 0);
        DosWriteQueue(G_hqPageMage, ulRequest, 0, NULL, 0);
        thrFree(&G_ptiMoveThread); */

        if (G_pHookData->PageMageConfig.fRecoverOnShutdown)
            pgmmRecoverAllWindows();

        // set global window handles to NULLHANDLE;
        // the hook sees this and will stop processing
        // PageMage messages
        G_pHookData->hwndPageMageClient = NULLHANDLE;
        G_pHookData->hwndPageMageFrame = NULLHANDLE;

        // then destroy the PageMage window
        WinDestroyWindow(hwndPageMageFrame);

        // notify kernel that PageMage has been closed
        // so that the global settings can be updated
        WinPostMsg(G_pDaemonShared->hwndThread1Object,
                   T1M_PAGEMAGECLOSED,
                   (MPARAM)fNotifyKernel,       // if TRUE, PageMage will be disabled
                   0);
    }
#endif
}

/* ******************************************************************
 *                                                                  *
 *   Hook interface                                                 *
 *                                                                  *
 ********************************************************************/

/*
 *@@ LoadHotkeysForHook:
 *      this loads the global object hotkeys from
 *      OS2.INI and passes them to the hook by
 *      calling hookSetGlobalHotkeys, which will
 *      then allocate shared memory for the hotkeys.
 *
 *      This gets called from main() when the daemon
 *      is (re)started and from fnwpDaemonObject
 *      when the hotkeys have changed.
 *
 *@@changed V0.9.3 (2000-04-20) [umoeller]: added function keys support
 */

VOID LoadHotkeysForHook(VOID)
{
    PGLOBALHOTKEY pHotkeys = NULL;
    ULONG cHotkeys = 0;
    PFUNCTIONKEY pFunctionKeys = NULL;
    ULONG cFunctionKeys = 0;

    ULONG   ulSizeOfData = 0;

    // _Pmpf(("LoadHotkeysForHook, pHookData: 0x%lX", G_pHookData));

    // 1) hotkeys

    // get size of data for pszApp/pszKey
    if (PrfQueryProfileSize(HINI_USER,
                            INIAPP_XWPHOOK,
                            INIKEY_HOOK_HOTKEYS,
                            &ulSizeOfData))
    {
        pHotkeys = (PGLOBALHOTKEY)malloc(ulSizeOfData);
        if (PrfQueryProfileData(HINI_USER,
                                INIAPP_XWPHOOK,
                                INIKEY_HOOK_HOTKEYS,
                                pHotkeys,
                                &ulSizeOfData))
            // hotkeys successfully loaded:
            // calc no. of items in array
            cHotkeys = ulSizeOfData / sizeof(GLOBALHOTKEY);
    }

    // 2) function keys

    // get size of data for pszApp/pszKey
    ulSizeOfData = 0;
    if (PrfQueryProfileSize(HINI_USER,
                            INIAPP_XWPHOOK,
                            INIKEY_HOOK_FUNCTIONKEYS,
                            &ulSizeOfData))
    {
        pFunctionKeys = (PFUNCTIONKEY)malloc(ulSizeOfData);
        if (PrfQueryProfileData(HINI_USER,
                                INIAPP_XWPHOOK,
                                INIKEY_HOOK_FUNCTIONKEYS,
                                pFunctionKeys,
                                &ulSizeOfData))
        {
            // hotkeys successfully loaded:
            // calc no. of items in array
            cFunctionKeys = ulSizeOfData / sizeof(FUNCTIONKEY);
            // _Pmpf(("  Got %d function keys", cFunctionKeys));
            // _Pmpf(("  function key %d is 0x%lX", 1, pFunctionKeys[1].ucScanCode));
        }
    }

    // 3) notify hook
    hookSetGlobalHotkeys(pHotkeys,
                         cHotkeys,
                         pFunctionKeys,
                         cFunctionKeys);

    // we can safely free the hotkeys, because
    // the hook has copied them to shared memory
    if (pHotkeys)
        free(pHotkeys);
    if (pFunctionKeys)
        free(pFunctionKeys);
}

/*
 *@@ LoadHookConfig:
 *      this (re)loads the configuration data
 *      from OS2.INI directly into the shared
 *      global data segment of XWPHOOK.DLL.
 *
 *      This has all the configuration except
 *      for the object hotkeys.
 *
 *      This gets called from main() when the daemon
 *      is (re)started and from fnwpDaemonObject
 *      upon receiving XDM_HOOKCONFIG when the
 *      config data has changed.
 */

BOOL LoadHookConfig(BOOL fHook,         // in: reload hook settings
                    BOOL fPageMage)     // in: reload PageMage settings
{
    BOOL brc = FALSE;

    // _Pmpf(("XWPDAEMON: LoadHookConfig, pHookData: 0x%lX", G_pHookData));
    // _Pmpf(("    fHook: %d", fHook));
    // _Pmpf(("    fPageMage: %d", fPageMage));

    if (G_pHookData)
    {
        ULONG cb =  sizeof(HOOKCONFIG);

        if (fHook)
        {
            // safe defaults
            memset(&G_pHookData->HookConfig, 0, sizeof(HOOKCONFIG));
            // overwrite from INI, if found
            brc = PrfQueryProfileData(HINI_USER,
                                      INIAPP_XWPHOOK,
                                      INIKEY_HOOK_CONFIG,
                                      &G_pHookData->HookConfig,
                                      &cb);
        }

#ifdef __PAGEMAGE__
        if (fPageMage)
        {
            // safe defaults
            pgmsSetDefaults();
            // overwrite from INI, if found
            pgmsLoadSettings(0);
            // in any case, write them back to OS2.INI because
            // otherwise XWPScreen doesn't work right
            pgmsSaveSettings();
        }
#endif
    }

    return (brc);
}

/*
 *@@ InstallHook:
 *      this installs the hooks in XWPHOOK.DLL by calling
 *      hookInit in XWPHOOK.DLL and then loads the configuration
 *      by calling LoadHotkeysForHook and LoadHookConfig.
 *
 *      This gets called when fnwpDaemonObject receives XDM_HOOKINSTALL.
 *
 *      At this point, if this is the first call, only the daemon
 *      object window has been created in main(). PageMage
 *      doesn't exist yet and will only be started later
 *      when a separate XDM_STARTSTOPPAGEMAGE message is
 *      received by fnwpDaemonObject.
 */

VOID InstallHook(VOID)
{
    if (!G_pDaemonShared->fAllHooksInstalled)
    {
        /* HMTX hmtx;
        if (NO_ERROR == DosCreateMutexSem(NULL,  // unnamed
                                          &hmtx,
                                          DC_SEM_SHARED,
                                                // shared mutex; the hook requests
                                                // this, so this is needed by every
                                                // PM process
                                          FALSE))    // unowned
        { */
            // install hook
            G_pHookData = hookInit(G_pDaemonShared->hwndDaemonObject);

            // _Pmpf(("XWPDAEMON: hookInit called, pHookData: 0x%lX", G_pHookData));

            if (G_pHookData)
                if (    (G_pHookData->fInputHooked)
                     && (G_pHookData->fPreAccelHooked)
                   )
                {
                    // success:
                    G_pDaemonShared->fAllHooksInstalled = TRUE;
                    // load hotkeys list from OS2.INI
                    LoadHotkeysForHook();
                    // load config from OS2.INI
                    LoadHookConfig(TRUE,
                                   TRUE);
                }
        // }
    }
}

/*
 *@@ DeinstallHook:
 *      this removes the hooks by calling hookKill
 *      in XWPHOOK.DLL and also calls dmnKillPageMage.
 *
 *      This gets called when fnwpDaemonObject receives
 *      XDM_HOOKINSTALL. Also, this gets called on the
 *      daemon exit list in case the daemon crashes.
 *
 *@@changed V0.9.4 (2000-07-14) [umoeller]: timers kept running after this; fixed
 */

VOID DeinstallHook(VOID)
{
    if (G_pDaemonShared->fAllHooksInstalled)
    {
        // if mouse pointer is currently hidden,
        // show it now (otherwise it'll be lost...)
        if (G_pHookData)
            if (G_pHookData->fMousePointerHidden)
                WinShowPointer(HWND_DESKTOP, TRUE);

        dmnKillPageMage(FALSE);
        hookKill();
        G_pDaemonShared->fAllHooksInstalled = FALSE;

        // stop timers which might still be running V0.9.4 (2000-07-14) [umoeller]
        WinStopTimer(G_habDaemon,
                     G_pDaemonShared->hwndDaemonObject,
                     TIMERID_SLIDINGFOCUS);
        WinStopTimer(G_habDaemon,
                     G_pDaemonShared->hwndDaemonObject,
                     TIMERID_SLIDINGMENU);
        WinStopTimer(G_habDaemon,
                     G_pDaemonShared->hwndDaemonObject,
                     TIMERID_AUTOHIDEMOUSE);

        // _Pmpf(("XWPDAEMON: hookKilled called"));
    }
    G_pHookData = NULL;
}

/* ******************************************************************
 *                                                                  *
 *   Hook message processing                                        *
 *                                                                  *
 ********************************************************************/

/*
 *@@ ProcessSlidingFocus:
 *      this gets called from fnwpDaemonObject to
 *      implement the "sliding focus" feature.
 *
 *      This gets called in two situations:
 *
 *      -- If sliding focus delay is off, directly
 *         when the daemon receives XDM_SLIDINGFOCUS
 *         from the hook, that is, if the mouse has
 *         moved over a new frame window which should
 *         be activated.
 *
 *      -- If we have a delay of sliding focus,
 *         fnwpDaemonObject starts a timer on receiving
 *         XDM_SLIDINGFOCUS and calls this func only
 *         after the timer has elapsed.
 *
 *      This was initially based on code from
 *      ProgramCommander/2, but I got a few bugs fixed
 *      now.
 *
 *@@changed V0.9.3 (2000-05-23) [umoeller]: fixed MDI-subframes problem (VIEW.EXE)
 *@@changed V0.9.4 (2000-06-12) [umoeller]: fixed Win-OS/2 handling, which broke with 0.9.3
 *@@changed V0.9.7 (2000-12-08) [umoeller]: added "ignore XCenter"
 */

VOID ProcessSlidingFocus(HWND hwndFrameInBetween, // in: != NULLHANDLE if hook has detected another frame
                                                  // under the mouse before the desktop frame window was
                                                  // reached
                         HWND hwnd2Activate)     // in: highest parent of hwndUnderMouse, child of desktop
{
    HWND    hwndCurrentlyActive = NULLHANDLE,
            hwndClient = NULLHANDLE;
    BOOL    fIsSeamless = FALSE;

    // rule out desktops, if "ignore desktop" is on
    if (G_pHookData->HookConfig.fSlidingIgnoreDesktop)
        if (hwnd2Activate == G_pHookData->hwndPMDesktop)
            // target is PM desktop:
            return;
        else if (hwnd2Activate == G_pHookData->hwndWPSDesktop)
            // target is WPS desktop:
            return;

    // rule out PageMage, if "ignore PageMage" is on
    if (G_pHookData->HookConfig.fSlidingIgnorePageMage)
        if (hwnd2Activate == G_pHookData->hwndPageMageFrame)
            // target is PM desktop:
            return;

    // V0.9.7 (2000-12-08) [umoeller]:
    // rule out XCenter, if "ignore XCenter" is on
    hwndClient = WinWindowFromID(hwnd2Activate, FID_CLIENT);
            // also needed below for seamless
    if (G_pHookData->HookConfig.fSlidingIgnoreXCenter)
    {
        // Is-XCenter check: the window must have an FID_CLIENT
        // whose class name is WC_XCENTER_CLIENT
        if (hwndClient)
        {
            CHAR szClass[100];
            WinQueryClassName(hwndClient, sizeof(szClass), szClass);
            if (strcmp(szClass, WC_XCENTER_CLIENT) == 0)
                // target is XCenter:
                return;
        }
    }

    // always ignore window list
    if (hwnd2Activate == G_pHookData->hwndWindowList)
        return;

    if (WinQueryWindowULong(hwnd2Activate, QWL_STYLE) & WS_DISABLED)
        // window is disabled: ignore
        return;

    hwndCurrentlyActive = WinQueryActiveWindow(HWND_DESKTOP);

    // and stop if currently active window is the window list too
    if (hwndCurrentlyActive == G_pHookData->hwndWindowList)
        return;

    // handle seamless Win-OS/2 windows: those have a frame
    // and an FID_CLIENT of the SeamlessClass class
    if (hwndClient)     // queried above
    {
        CHAR szClassName[200];
        WinQueryClassName(hwndClient, sizeof(szClassName), szClassName);
        // _Pmpf(("ProcessSlidingFocus: hwndClient 0x%lX", hwndClient));
        // _Pmpf(("  class name: %s", szClassName));
        if (strcmp(szClassName, "SeamlessClass") == 0)
            fIsSeamless = TRUE;
    }

    // rule out redundant processing:
    // we might come here if the mouse has moved
    // across several desktop windows while the
    // sliding-focus delay has not elapsed yet
    // so that the active window really hasn't changed

    // bring-to-top mode
    // or window to activate is seamless Win-OS/2 window
    // and should be brought to the top?
    if (    (G_pHookData->HookConfig.fSlidingBring2Top)
         || (   (fIsSeamless)
             && (!G_pHookData->HookConfig.fSlidingIgnoreSeamless)
            )
       )
    {
        if (hwndCurrentlyActive != hwnd2Activate)
        {
            WinSetActiveWindow(HWND_DESKTOP, hwnd2Activate);
            G_pHookData->hwndActivatedByUs = hwnd2Activate;
        }
    }
    else
    {
        // no bring-to-top == preserve-Z-order mode:
        if (!fIsSeamless)
        {
            HWND hwndNewFocus = NULLHANDLE;

            // not seamless Win-OS/2 window:
            HWND    hwndTitlebar = NULLHANDLE;

            /*
             * step 1: handle subframes
             *
             */

            // let's check if we have an MDI frame, that is,
            // another frame below the subframe:
            HWND    hwndFocusSubwin = NULLHANDLE;

            if (hwndFrameInBetween)
                // hook has already detected another sub-frame under
                // the mouse: use that subframe's client
                hwndFocusSubwin = WinWindowFromID(hwndFrameInBetween, FID_CLIENT);

            if (!hwndFocusSubwin)
                // not found: try if maybe the main frame has saved
                // the focus of a subclient
                hwndFocusSubwin = WinQueryWindowULong(hwnd2Activate,
                                                      QWL_HWNDFOCUSSAVE);
                // typical cases of this are:
                // 1) a dialog window with controls, one of which had the focus;
                // 2) a plain frame window with a client which had the focus;
                // 3) complex case: a main frame window with several frame subwindows.
                //    Best example is VIEW.EXE, which has the following hierachy:
                //      main frame -- main client
                //                      +--- another view frame with a sub-client
                //                      +--- another view frame with a sub-client
                //    hwndFocusSubwin then has one of the sub-clients, so we better
                //    check:

            if ((hwndFocusSubwin) && (WinIsWindowVisible(hwndFocusSubwin)))
            {
                // frame window has control or client with focus stored:

                CHAR    szFocusSaveClass[200] = "";

                // from the hwndFocusSubwin, climb up the parents tree until
                // we reach the main frame. If we find another frame between
                // hwndFocusSubwin and the main frame, activate that one as well.
                HWND    hwndTemp = hwndFocusSubwin,
                        hwndMDIFrame = NULLHANDLE;
                                // this receives the "in-between" frame
                while (     (hwndTemp)
                         && (hwndTemp != hwnd2Activate)
                      )
                {
                    WinQueryClassName(hwndTemp,
                                      sizeof(szFocusSaveClass),
                                      szFocusSaveClass);
                    if (strcmp(szFocusSaveClass, "#1") == 0)
                    {
                        // found another frame in between:
                        hwndMDIFrame = hwndTemp;
                        break;
                    }

                    hwndTemp = WinQueryWindow(hwndTemp, QW_PARENT);
                }

                if (hwndMDIFrame)
                {
                    // yes, we found another frame in between:
                    // make that active as well
                    hwndTitlebar = WinWindowFromID(hwndMDIFrame, FID_TITLEBAR);
                    if (hwndTitlebar)
                    {
                        WinPostMsg(hwndMDIFrame,
                                   WM_ACTIVATE,
                                   MPFROMSHORT(TRUE),
                                   MPFROMHWND(hwndTitlebar));
                    }
                }

                hwndNewFocus = hwndFocusSubwin;
            }
            else
            {
                // frame window has no saved focus:
                // try if we can activate the client
                hwndClient = WinWindowFromID(hwnd2Activate,
                                             FID_CLIENT);
                if (hwndClient)
                {
                    // _Pmpf(("        giving focus 2 client"));
                    hwndNewFocus = hwndClient;
                }
                else
                {
                    // _Pmpf(("        no client, giving focus 2 frame"));
                    // we don't even have a client:
                    // activate the frame directly
                    hwndNewFocus = hwnd2Activate;
                }

            }

            // now give the new window the focus
            if (hwndNewFocus)
            {
                WinFocusChange(HWND_DESKTOP,
                               hwndNewFocus,
                               FC_NOSETACTIVE);
            }

            // now handle main frame

            if (hwndCurrentlyActive != hwnd2Activate)
            {
                // deactivate old window
                WinPostMsg(hwndCurrentlyActive,
                           WM_ACTIVATE,
                           MPFROMSHORT(FALSE),      // deactivate
                           MPFROMHWND(hwndNewFocus));

                // activate new window
                hwndTitlebar = WinWindowFromID(hwnd2Activate, FID_TITLEBAR);
                if (hwndTitlebar)
                {
                    WinPostMsg(hwnd2Activate,
                               WM_ACTIVATE,
                               MPFROMSHORT(TRUE),
                               MPFROMHWND(hwndTitlebar));
                }
            }

            // store activated window in hook data
            // so that the hook knows that we activated
            // this window
            G_pHookData->hwndActivatedByUs = hwnd2Activate;
        }
    }
}

/* ******************************************************************
 *                                                                  *
 *   Daemon object window                                           *
 *                                                                  *
 ********************************************************************/

/*
 *@@ fnwpDaemonObject:
 *      object window created in main(). This runs on thread 1.
 *
 *      This reacts to XDM_* messages (see xwphook.h)
 *      which are either posted from XWPHOOK.DLL for
 *      hotkeys, sliding focus, screen corner objects,
 *      or from XFLDR.DLL for daemon/hook configuration.
 *
 *@@changed V0.9.3 (2000-04-12) [umoeller]: added exception handling
 *@@changed V0.9.5 (2000-09-20) [pr]: fixed sliding focus bug
 */

MRESULT EXPENTRY fnwpDaemonObject(HWND hwndObject, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc;

    // window to be activated (sliding focus)
    static HWND     S_hwndUnderMouse = NULLHANDLE;
    static HWND     S_hwnd2Activate = NULLHANDLE;

    TRY_LOUD(excpt1)
    {
        switch (msg)
        {
            /*
             *@@ XDM_HOOKINSTALL:
             *      this must be sent while the daemon
             *      is running to install or deinstall
             *      the hook. This does not affect operation
             *      of the daemon, which will keep running.
             *
             *      This is used by the XWPSetup "Features"
             *      page and by XFLDR.DLL after main() has
             *      posted T1M_DAEMONREADY to the thread-1
             *      object window.
             *
             *      Parameters:
             *      -- BOOL mp1: if TRUE, the hook will be installed;
             *                   if FALSE, the hook will be deinstalled.
             *
             *      Return value:
             *      -- BOOL TRUE if the hook is now installed,
             *              FALSE if it is not.
             */

            case XDM_HOOKINSTALL:
                // _Pmpf(("fnwpDaemonObject: got XDM_HOOKINSTALL (%d)", mp1));
                if (mp1)
                    // install the hook:
                    InstallHook();
                        // this sets the global pHookData pointer
                        // to the HOOKDATA in the DLL
                else
                    DeinstallHook();

                mrc = (MPARAM)(G_pDaemonShared->fAllHooksInstalled);
            break;

            /*
             *@@ XDM_DESKTOPREADY:
             *      this gets posted from fnwpFileObject after
             *      the WPS desktop frame has been opened
             *      so that the daemon/hook knows about the
             *      HWND of the WPS desktop. This is necessary
             *      for the sliding focus feature and for
             *      PageMage.
             *
             *      Parameters:
             *      -- HWND mp1: desktop frame HWND.
             *      -- mp2: unused, always 0.
             *
             *@@changed V0.9.4 (2000-08-08) [umoeller]: fixed problems after WPS restart
             */

            case XDM_DESKTOPREADY:
                // _Pmpf(("fnwpDaemonObject: got XDM_DESKTOPREADY (hwnd 0x%lX)", mp1));
                // _Pmpf(("    G_pHookData is: 0x%lX", G_pHookData));
                if (G_pHookData)
                {
                    G_pHookData->hwndWPSDesktop = (HWND)mp1;
#ifdef __PAGEMAGE__
                    // give PageMage a chance to recognize the Desktop
                    // V0.9.4 (2000-08-08) [umoeller]
                    pgmwWindowListAdd(G_pHookData->hwndWPSDesktop);
#endif
                }
            break;

            /*
             *@@ XDM_HOOKCONFIG:
             *      this gets _sent_ from XFLDR.DLL when
             *      daemon/hook settings have changed.
             *      This causes HOOKDATA.HOOKCONFIG to be
             *      updated from OS2.INI.
             *
             *      Parameters: none.
             *
             *      Return value:
             *      -- BOOL TRUE if successful.
             */

            case XDM_HOOKCONFIG:
                // _Pmpf(("fnwpDaemonObject: got XDM_HOOKCONFIG"));
                // load config from OS2.INI
                mrc = (MPARAM)LoadHookConfig(TRUE,      // hook
                                             FALSE);    // PageMage

            break;

#ifdef __PAGEMAGE__
            /*
             *@@ XDM_STARTSTOPPAGEMAGE:
             *      starts or stops PageMage.
             *
             *      If (mp1 == TRUE), dmnStartPageMage is called.
             *      If (mp1 == FALSE), dmnKillPageMage is called.
             *
             *@@added V0.9.2 (2000-02-21) [umoeller]
             */

            case XDM_STARTSTOPPAGEMAGE:
                // _Pmpf(("fnwpDaemonObject: got XDM_STARTSTOPPAGEMAGE (%d)", mp1));
                if (mp1)
                    // install the hook:
                    dmnStartPageMage();
                        // this sets the global pHookData pointer
                        // to the HOOKDATA in the DLL
                else
                    dmnKillPageMage(FALSE); // no notify

                mrc = (MPARAM)(G_pHookData->hwndPageMageFrame != NULLHANDLE);
            break;

            /*
             *@@ XDM_PAGEMAGECONFIG:
             *      this gets _sent_ from XFLDR.DLL when
             *      PageMage settings have changed.
             *      This causes the global PAGEMAGECONFIG
             *      data to be updated from OS2.INI.
             *
             *      Parameters:
             *      -- ULONG mp1: any of the following flags:
             *          -- PGMGCFG_REPAINT: repaint PageMage client
             *          -- PGMGCFG_REFORMAT: reformat whole window (e.g.
             *                  because Desktops have changed)
             *          -- PGMGCFG_ZAPPO: reformat title bar.
             *          -- PGMGCFG_STICKIES: sticky windows have changed,
             *                  rescan window list.
             *
             *      Return value:
             *      -- BOOL TRUE if successful.
             */

            case XDM_PAGEMAGECONFIG:
                // _Pmpf(("fnwpDaemonObject: got XDM_PAGEMAGECONFIG"));
                // load config from OS2.INI
                mrc = (MPARAM)pgmsLoadSettings((ULONG)mp1);
            break;
#endif

            /*
             *@@ XDM_HOTKEYPRESSED:
             *      message posted by the hook when
             *      a global hotkey is pressed. We forward
             *      this message on to the XWorkplace thread-1
             *      object window so that the object can be
             *      started on thread 1 of the WPS. If we used
             *      WinOpenObject here, the object would be
             *      running on a thread other than thread 1,
             *      which is not really desirable.
             *
             *      Parameters:
             *      -- ULONG mp1: hotkey identifier, probably WPS object handle.
             */

            case XDM_HOTKEYPRESSED:
                if (mp1)
                {
                    // _Pmpf(("Forwarding 0x%lX to 0x%lX",
                    //         mp1,
                    //         G_pDaemonShared->hwndThread1Object));
                    // forward msg (cross-process post)
                    WinPostMsg(G_pDaemonShared->hwndThread1Object,
                               T1M_OPENOBJECTFROMHANDLE,
                               mp1,
                               mp2);
                }
            break;

            /*
             *@@ XDM_HOTKEYSCHANGED:
             *      message posted by hifSetObjectHotkeys
             *      or hifSetFunctionKeys (XFLDR.DLL) when the
             *      list of global object hotkeys or the list
             *      of function keys has changed.
             *
             *      This is posted after the new lists have been
             *      written to OS2.INI so we can safely re-read
             *      this now and notify the hook of the change.
             *
             *      This is NOT posted at startup because we then
             *      read the lists automatically (XDM_HOOKINSTALL,
             *      InstallHook).
             *
             *      Parameters: none.
             *
             *@@changed V0.9.3 (2000-04-20) [umoeller]: this is now also posted for function keys.
             */

            case XDM_HOTKEYSCHANGED:
                LoadHotkeysForHook();
            break;

            /*
             *@@ XDM_SLIDINGFOCUS:
             *      message posted by the hook when the mouse
             *      pointer has moved to a new frame window.
             *
             *      Parameters:
             *      -- HWND mp1: window which was under the mouse
             *              (qmsg.hwnd in hook)
             *      -- HWND mp2: highest parent of that window,
             *              child of PM desktop; this is probably
             *              a WC_FRAME, but doesn't have to be.
             *              If this is NULLHANDLE, the daemon is
             *              telling us to stop any timers which
             *              might be running.
             */

            case XDM_SLIDINGFOCUS:
                S_hwndUnderMouse = (HWND)mp1;
                S_hwnd2Activate = (HWND)mp2;
                // DosBeep(10000, 10);
                if (G_pHookData->HookConfig.ulSlidingFocusDelay)
                {
                    // delayed sliding focus:
                    if (G_ulSlidingFocusTimer)
                    {
                        WinStopTimer(G_habDaemon,
                                     hwndObject,
                                     TIMERID_SLIDINGFOCUS);
                        G_ulSlidingFocusTimer = NULLHANDLE;
                    }

                    if (S_hwnd2Activate)
                        G_ulSlidingFocusTimer = WinStartTimer(G_habDaemon,
                                                              hwndObject,
                                                              TIMERID_SLIDINGFOCUS,
                                                              G_pHookData->HookConfig.ulSlidingFocusDelay);
                    // else: the daemon is telling us
                    // to stop timers which are running...
                }
                else
                    // immediately
                    // V0.9.5 (2000-09-20)[pr] sliding focus bug
                    if (S_hwnd2Activate)
                        ProcessSlidingFocus(S_hwndUnderMouse,
                                            S_hwnd2Activate);
            break;

            /*
             *@@ XDM_SLIDINGMENU:
             *      message posted by the hook when the mouse
             *      pointer has moved to a new menu item.
             *
             *      Parameters:
             *      -- mp1: mp1 of WM_MOUSEMOVE which was processed.
             *      -- mp2: unused.
             *
             *@@added V0.9.2 (2000-02-26) [umoeller]
             */

            case XDM_SLIDINGMENU:
                G_SlidingMenuMp1Saved = mp1;

                // delayed sliding menu:
                if (G_ulSlidingMenuTimer)
                    WinStopTimer(G_habDaemon,
                                 hwndObject,
                                 TIMERID_SLIDINGMENU);
                G_ulSlidingMenuTimer = WinStartTimer(G_habDaemon,
                                                     hwndObject,
                                                     TIMERID_SLIDINGMENU,
                                                     G_pHookData->HookConfig.ulSubmenuDelay);
            break;

            /*
             *@@ XDM_HOTCORNER:
             *      message posted by the hook when the mouse
             *      has reached one of the four corners of the
             *      screen ("hot corners"). The daemon then
             *      determines whether to open any object.
             *
             *      Parameters:
             *      -- BYTE mp1: corner reached;
             *                  1 = lower left corner,
             *                  2 = top left corner,
             *                  3 = lower right corner,
             *                  4 = top right corner.
             *              Borders added V0.9.4 (2000-06-12) [umoeller]:
             *                  5 = top border,
             *                  6 = left border,
             *                  7 = right border,
             *                  8 = bottom border.
             *      -- mp2: unused, always 0.
             */

            case XDM_HOTCORNER:
                if (mp1)
                {
                    // create array index: mp1 minus one.
                    LONG lIndex = ((LONG)mp1)-1;

                    if ((lIndex >= 0) && (lIndex <= 7))
                        if (G_pHookData->HookConfig.ahobjHotCornerObjects[lIndex] != NULLHANDLE)
                            // hot-corner object defined:
                            // check if it's PageMage
                            if (G_pHookData->HookConfig.ahobjHotCornerObjects[lIndex]
                                    == 0xFFFF0002)
                            {
                                // yes: bring up PageMage window
                                WinSetWindowPos(G_pHookData->hwndPageMageFrame,
                                                HWND_TOP,
                                                0, 0, 0, 0,
                                                SWP_ZORDER | SWP_SHOW | SWP_RESTORE);
#ifdef __PAGEMAGE__
                                // start or restart timer for flashing
                                // fixed V0.9.4 (2000-07-10) [umoeller]
                                if (G_pHookData->PageMageConfig.fFlash)
                                    pgmgcStartFlashTimer();
#endif
                            }
                            else
                                // no: let XFLDR.DLL thread-1 object handle this
                                WinPostMsg(G_pDaemonShared->hwndThread1Object,
                                           T1M_OPENOBJECTFROMHANDLE,
                                           // pass HOBJECT or special function (0xFFFF000x)
                                           (MPARAM)(G_pHookData->HookConfig.ahobjHotCornerObjects[lIndex]),
                                           // pass mp1
                                           (MPARAM)(lIndex + 1));

                }
            break;

            /*
             *@@ XDM_WMCHORDWINLIST:
             *      posted from the hook when WM_CHORD has been
             *      received and the window list should be displayed.
             *      We offload this to the daemon to process this
             *      asynchronously.
             *
             *      Parameters: none.
             *
             *@@added V0.9.4 (2000-06-14) [umoeller]
             */

            case XDM_WMCHORDWINLIST:
            {
                POINTL  ptlMouse;       // mouse coordinates
                SWP     WinListPos;     // position of window list window
                LONG WinListX, WinListY; // new ordinates of window list window
                // LONG DesktopCX, DesktopCY; // width and height of screen
                // get mouse coordinates (absolute coordinates)
                WinQueryPointerPos(HWND_DESKTOP, &ptlMouse);
                // get position of window list window
                WinQueryWindowPos(G_pHookData->hwndWindowList,
                                  &WinListPos);
                // calculate window list position (mouse pointer is center)
                WinListX = ptlMouse.x - (WinListPos.cx / 2);
                if (WinListX < 0)
                    WinListX = 0;
                WinListY = ptlMouse.y - (WinListPos.cy / 2);
                if (WinListY < 0)
                    WinListY = 0;
                if (WinListX + WinListPos.cx > G_pHookData->lCXScreen)
                    WinListX = G_pHookData->lCXScreen - WinListPos.cx;
                if (WinListY + WinListPos.cy > G_pHookData->lCYScreen)
                    WinListY = G_pHookData->lCYScreen - WinListPos.cy;
                // set window list window to calculated position
                WinSetWindowPos(G_pHookData->hwndWindowList, HWND_TOP,
                                WinListX, WinListY, 0, 0,
                                SWP_MOVE | SWP_SHOW | SWP_ZORDER);
                // now make it the active window
                WinSetActiveWindow(HWND_DESKTOP,
                                   G_pHookData->hwndWindowList);
            break; }

            /*
             *@@ XDM_PGMGWINLISTFULL:
             *
             *@@added V0.9.4 (2000-08-09) [umoeller]
             */

            case XDM_PGMGWINLISTFULL:
                /* winhDebugBox(NULLHANDLE,
                             "XWorkplace Daemon",
                             "The PageMage window list is full."); */
            break;

            /*
             * WM_TIMER:
             *
             */

            case WM_TIMER:
                switch ((ULONG)mp1)
                {
                    /*
                     * TIMERID_SLIDINGFOCUS:
                     *
                     */

                    case TIMERID_SLIDINGFOCUS:
                        WinStopTimer(G_habDaemon,
                                     hwndObject,
                                     (ULONG)mp1);   // timer ID
                        G_ulSlidingFocusTimer = NULLHANDLE;

                        ProcessSlidingFocus(S_hwndUnderMouse,
                                            S_hwnd2Activate);
                    break;

                    /*
                     * TIMERID_SLIDINGMENU:
                     *      started from XDM_SLIDINGMENU.
                     */

                    case TIMERID_SLIDINGMENU:
                        WinStopTimer(G_habDaemon,
                                     hwndObject,
                                     (ULONG)mp1);   // timer ID
                        G_ulSlidingMenuTimer = NULLHANDLE;

                        // post a special WM_MOUSEMOVE message;
                        // see WMMouseMove_SlidingMenus in xwphook.c
                        // for how this works
                        WinPostMsg(G_pHookData->hwndMenuUnderMouse,
                                   WM_MOUSEMOVE,
                                   G_SlidingMenuMp1Saved,
                                        // MP1 which was saved in XDM_SLIDINGMENU
                                        // to identify this msg in the hook
                                   MPFROM2SHORT(HT_DELAYEDSLIDINGMENU,
                                                KC_NONE));
                    break;

                    /*
                     * TIMERID_MONITORDRIVE:
                     *
                     */

                    case TIMERID_MONITORDRIVE:
                        CheckRemoveableDrive();
                    break;

                    /*
                     * TIMERID_AUTOHIDEMOUSE:
                     *
                     */

                    case TIMERID_AUTOHIDEMOUSE:
                        WinStopTimer(G_habDaemon,
                                     hwndObject,
                                     (ULONG)mp1);   // timer ID

                        G_pHookData->idAutoHideTimer = NULLHANDLE;

                        WinShowPointer(HWND_DESKTOP, FALSE);
                        G_pHookData->fMousePointerHidden = TRUE;
                    break;
                }
            break;

            default:
                mrc = WinDefWindowProc(hwndObject, msg, mp1, mp2);
        }
    }
    CATCH(excpt1)
    {
        mrc = 0;
    } END_CATCH();

    return (mrc);
}

/* ******************************************************************
 *                                                                  *
 *   main()                                                         *
 *                                                                  *
 ********************************************************************/

/*
 *@@ DaemonExitList:
 *      this is registered with DosExitList() in main,
 *      in case we crash, or the program is killed.
 *
 *      We call hookKill then.
 */

VOID APIENTRY DaemonExitList(ULONG ulCode)
{
    // de-install the hook
    DeinstallHook();
    if (G_pDaemonShared)
    {
        G_pDaemonShared->hwndDaemonObject = NULLHANDLE;
    }

    // and exit (we must not use return here)
    DosExitList(EXLST_EXIT, (PFNEXITLIST)DaemonExitList);
}

/*
 *@@ main:
 *      program entry point.
 *
 *      This first checks for the "-D" parameter, which
 *      is a security measure so that this program does
 *      not get started by curious XWorkplace users.
 *
 *      Then, we access the DAEMONSHARED structure which
 *      has been allocated by krnInitializeXWorkplace
 *      (filesys\kernel.c).
 *
 *      We then create our object window (fnwpDaemonObject),
 *      to which the hook will post messages.
 *
 *      This does not install the hooks yet, but posts
 *      T1M_DAEMONREADY to pDaemonShared->hwndThread1Object only.
 *      To actually install the hook, XFLDR.DLL then posts
 *      XDM_HOOKINSTALL to the daemon object window.
 */

int main(int argc, char *argv[])
{
    G_habDaemon = WinInitialize(0);
    if (G_habDaemon)
    {
        G_hmqDaemon = WinCreateMsgQueue(G_habDaemon, 0);
        if (G_hmqDaemon)
        {
            // get our process ID
            PTIB    ptib;
            PPIB    ppib;
            if (DosGetInfoBlocks(&ptib, &ppib) == NO_ERROR)
                G_pidDaemon = ppib->pib_ulpid;

            // set up exception handlers
            excRegisterHooks(dmnExceptOpenLogFile,
                             dmnExceptExplain,
                             dmnExceptError,
                             TRUE);     // beeps

            TRY_LOUD(excpt1)
            {
                // check security dummy parameter "-D"
                if (    (argc == 2)
                     && (strcmp(argv[1], "-D") == 0)
                   )
                {
                    HMTX    hmtx;
                    // check the daemon one-instance semaphore, which
                    // we create just for testing that the daemon is
                    // started only once
                    if (DosCreateMutexSem(IDMUTEX_ONEINSTANCE,
                                          &hmtx,
                                          DC_SEM_SHARED,
                                          TRUE)     // owned!!
                         == NO_ERROR)
                    {
                        // semaphore successfully created: this means
                        // that no other instance of XWPDAEMN.EXE is
                        // running

                        // access the shared memory allocated by
                        // XFLDR.DLL; this should always work:
                        // a) if the daemon has been started during first
                        //    WPS startup, krnInitializeXWorkplace has
                        //    allocated the memory before starting the
                        //    daemon;
                        // b) at any time later (if the daemon is restarted
                        //    manually), we have no problem either
                        APIRET arc = DosGetNamedSharedMem((PVOID*)&G_pDaemonShared,
                                                          SHMEM_DAEMON,
                                                          PAG_READ | PAG_WRITE);
                        if (arc == NO_ERROR)
                        {
                            // OK:
                            // install exit list
                            DosExitList(EXLST_ADD,
                                        DaemonExitList);

                            arc = DosCreateMutexSem(IDMUTEX_PGMG_WINLIST,
                                                    &G_hmtxWindowList,
                                                    DC_SEM_SHARED, // unnamed, but shared
                                                    FALSE);

                            /* arc = DosCreateEventSem(PAGEMAGE_WNDLSTEV,
                                                    &G_hevWindowList,
                                                    0,
                                                    FALSE); */

                            /* arc = DosCreateQueue(&G_hqPageMage,
                                                 QUE_FIFO | QUE_NOCONVERT_ADDRESS,
                                                 PAGEMAGE_WNDQUEUE); */

                            G_hptrDaemon = WinLoadPointer(HWND_DESKTOP,
                                                          NULLHANDLE,
                                                          1);

                            // create the object window
                            WinRegisterClass(G_habDaemon,
                                             (PSZ)WNDCLASS_DAEMONOBJECT,
                                             (PFNWP)fnwpDaemonObject,
                                             0,                  // class style
                                             0);                 // extra window words
                            G_pDaemonShared->hwndDaemonObject
                                = WinCreateWindow(HWND_OBJECT,
                                                  (PSZ)WNDCLASS_DAEMONOBJECT,
                                                  (PSZ)"",
                                                  0,
                                                  0,0,0,0,
                                                  0,
                                                  HWND_BOTTOM,
                                                  0,
                                                  NULL,
                                                  NULL);

                            // _Pmpf(("hwndDaemonObject: 0x%lX", G_pDaemonShared->hwndDaemonObject));

                            if (G_pDaemonShared->hwndDaemonObject)
                            {
                                QMSG    qmsg;

                                // OK: post msg to XFLDR.DLL thread-1 object window
                                // that we're ready, which will in turn send
                                // XDM_HOOKINSTALL
                                WinPostMsg(G_pDaemonShared->hwndThread1Object,
                                           T1M_DAEMONREADY,
                                           0, 0);

                                // standard PM message loop

                                // _Pmpf(("Entering msg queue"));

                                while (WinGetMsg(G_habDaemon, &qmsg, 0, 0, 0))
                                    WinDispatchMsg(G_habDaemon, &qmsg);

                                // _Pmpf(("Exited msg queue, WM_QUIT found"));

                                // then kill the hook again
                                DeinstallHook();
                                // _Pmpf(("main: DeinstallHook() returned"));
                            }

                            WinDestroyWindow(G_pDaemonShared->hwndDaemonObject);
                            G_pDaemonShared->hwndDaemonObject = NULLHANDLE;
                            // _Pmpf(("main: hwndDaemonObject destroyed"));
                        } // end if DosGetNamedSharedMem
                        else
                            WinMessageBox(HWND_DESKTOP, HWND_DESKTOP,
                                          "The XWorkplace Daemon failed to access the data block shared with "
                                          "the Workplace process. The daemon is terminated.",
                                          "XWorkplace Daemon",
                                          0,
                                          MB_MOVEABLE | MB_CANCEL | MB_ICONHAND);
                    } // end DosCreateMutexSem...
                    else
                    {
                        // mutex creation failed: another instance of
                        // XWPDAEMN.EXE is running, so complain
                        WinMessageBox(HWND_DESKTOP, HWND_DESKTOP,
                                      "Another instance of XWPDAEMN.EXE is already running. "
                                      "This instance will terminate now.",
                                      "XWorkplace Daemon",
                                      0,
                                      MB_MOVEABLE | MB_CANCEL | MB_ICONHAND);
                    }
                } // end if argc...
                else
                    WinMessageBox(HWND_DESKTOP, HWND_DESKTOP,
                                  "Hi there. Thanks for your interest in the XWorkplace Daemon, "
                                  "but this program is not intended to be started manually, "
                                  "but only automatically by XWorkplace when the WPS starts up.",
                                  "XWorkplace Daemon",
                                  0,
                                  MB_MOVEABLE | MB_CANCEL | MB_ICONHAND);

            }
            CATCH(excpt1) {} END_CATCH();

            WinDestroyMsgQueue(G_hmqDaemon);
        }
        WinTerminate(G_habDaemon);

        return (0);
    }
    return (99);
}

