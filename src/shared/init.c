
/*
 *@@sourcefile init.c:
 *      this file contains the XWorkplace initialization code
 *      which runs at Desktop startup.
 *
 *      This code used to be in kernel.c and has been moved
 *      to this file with V0.9.16.
 *
 *@@header "shared\init.h"
 */

/*
 *      Copyright (C) 1997-2001 Ulrich M�ller.
 *      This file is part of the XWorkplace source package.
 *      XWorkplace is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published
 *      by the Free Software Foundation, in version 2 as it comes in the
 *      "COPYING" file of the XWorkplace main distribution.
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 */

#pragma strings(readonly)

/*
 *  Suggested #include order:
 *  1)  os2.h
 *  2)  C library headers
 *  3)  setup.h (code generation and debugging options)
 *  4)  headers in helpers\
 *  5)  at least one SOM implementation header (*.ih)
 *  6)  dlgids.h, headers in shared\ (as needed)
 *  7)  headers in implementation dirs (e.g. filesys\, as needed)
 *  8)  #pragma hdrstop and then more SOM headers which crash with precompiled headers
 */

#define INCL_DOSEXCEPTIONS
#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#define INCL_DOSQUEUES
#define INCL_DOSERRORS

#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WINMESSAGEMGR
#define INCL_WINTIMER
#define INCL_WINSYS
#define INCL_WINPOINTERS
#define INCL_WINMENUS
#define INCL_WINDIALOGS
#define INCL_WINSTATICS
#define INCL_WINBUTTONS
#define INCL_WINPROGRAMLIST     // needed for PROGDETAILS, wppgm.h
#define INCL_WINSWITCHLIST
#define INCL_WINSHELLDATA
#define INCL_WINSTDFILE
#include <os2.h>
// C library headers
#include <stdio.h>              // needed for except.h
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h
#include <io.h>                 // access etc.
#include <fcntl.h>
#include <sys\stat.h>

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\apps.h"               // application helpers
#include "helpers\comctl.h"             // common controls (window procs)
#include "helpers\dialog.h"             // dialog helpers
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\procstat.h"           // DosQProcStat handling
#include "helpers\standards.h"          // some standard macros
#include "helpers\stringh.h"            // string helper routines
#include "helpers\threads.h"            // thread helpers
#include "helpers\winh.h"               // PM helper routines
#include "helpers\wphandle.h"           // file-system object handles
#include "helpers\xstring.h"            // extended string helpers

// SOM headers which don't crash with prec. header files
#include "xfstart.ih"
#include "xfldr.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "xwpapi.h"                     // public XWorkplace definitions

#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\helppanels.h"          // all XWorkplace help panel IDs
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

#include "filesys\filedlg.h"            // replacement file dialog implementation
#include "filesys\refresh.h"            // folder auto-refresh
#include "filesys\program.h"            // program implementation
#include "filesys\xthreads.h"           // extra XWorkplace threads

#include "media\media.h"                // XWorkplace multimedia support

#include "security\xwpsecty.h"          // XWorkplace Security

#include "startshut\archives.h"         // WPSArcO declarations
#include "startshut\shutdown.h"         // XWorkplace eXtended Shutdown

// headers in /hook
#include "hook\xwphook.h"

// other SOM headers
// #include <wpdesk.h>                     // WPDesktop

#pragma hdrstop

/* ******************************************************************
 *
 *   Private prototypes
 *
 ********************************************************************/

MRESULT EXPENTRY fnwpAPIObject(HWND hwndObject, ULONG msg, MPARAM mp1, MPARAM mp2);

MRESULT EXPENTRY fnwpThread1Object(HWND hwndObject, ULONG msg, MPARAM mp1, MPARAM mp2);

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

extern KERNELGLOBALS    G_KernelGlobals;            // kernel.c

static THREADINFO       G_tiSentinel = {0};

/* ******************************************************************
 *
 *   XWorkplace initialization
 *
 ********************************************************************/

/*
 *@@ G_apszXFolderKeys:
 *      XFolder INI keys to be copied when upgrading
 *      from XFolder to XWorkplace.
 */

const char **G_appszXFolderKeys[]
        = {
                &INIKEY_GLOBALSETTINGS  , // "GlobalSettings"
                &INIKEY_ACCELERATORS    , // "Accelerators"
#ifndef __NOFOLDERCONTENTS__
                &INIKEY_FAVORITEFOLDERS , // "FavoriteFolders"
#endif
#ifndef __NOQUICKOPEN__
                &INIKEY_QUICKOPENFOLDERS, // "QuickOpenFolders"
#endif
                &INIKEY_WNDPOSSTARTUP   , // "WndPosStartup"
                &INIKEY_WNDPOSNAMECLASH , // "WndPosNameClash"
                &INIKEY_NAMECLASHFOCUS  , // "NameClashLastFocus"
#ifndef __NOCFGSTATUSBARS__
                &INIKEY_STATUSBARFONT   , // "SB_Font"
                &INIKEY_SBTEXTNONESEL   , // "SB_NoneSelected"
                &INIKEY_SBTEXT_WPOBJECT , // "SB_WPObject"
                &INIKEY_SBTEXT_WPPROGRAM, // "SB_WPProgram"
                &INIKEY_SBTEXT_WPFILESYSTEM, // "SB_WPDataFile"
                &INIKEY_SBTEXT_WPURL       , // "SB_WPUrl"
                &INIKEY_SBTEXT_WPDISK   , // "SB_WPDisk"
                &INIKEY_SBTEXT_WPFOLDER , // "SB_WPFolder"
                &INIKEY_SBTEXTMULTISEL  , // "SB_MultiSelected"
                &INIKEY_SB_LASTCLASS    , // "SB_LastClass"
#endif
                &INIKEY_DLGFONT         , // "DialogFont"
                &INIKEY_BOOTMGR         , // "RebootTo"
                &INIKEY_AUTOCLOSE        // "AutoClose"
          };

/*
 *@@ WaitForApp:
 *      waits for the specified application to terminate.
 *
 *      Returns:
 *
 *      -1: Error starting app (happ was zero, msg box displayed).
 *
 *      Other: Return code of the application.
 *
 *@@added V0.9.9 (2001-03-07) [umoeller]
 */

ULONG WaitForApp(const char *pcszTitle,
                 HAPP happ)
{
    ULONG   ulrc = -1;

    if (!happ)
    {
        // error:
        PSZ apsz[] = {(PSZ)pcszTitle};
        cmnMessageBoxMsgExt(NULLHANDLE,
                            121,       // xwp
                            apsz,
                            1,
                            206,       // cannot start %1
                            MB_OK);
    }
    else
    {
        // app started:
        // enter a modal message loop until we get the
        // WM_APPTERMINATENOTIFY for happ. Then we
        // know the app is done.
        HAB     hab = WinQueryAnchorBlock(G_KernelGlobals.hwndThread1Object);
        QMSG    qmsg;
        // ULONG   ulXFixReturnCode = 0;
        while (WinGetMsg(hab, &qmsg, NULLHANDLE, 0, 0))
        {
            if (    (qmsg.msg == WM_APPTERMINATENOTIFY)
                 && (qmsg.hwnd == G_KernelGlobals.hwndThread1Object)
                 && (qmsg.mp1 == (MPARAM)happ)
               )
            {
                // xfix has terminated:
                // get xfix return code from mp2... this is:
                // -- 0: everything's OK, continue.
                // -- 1: handle section was rewritten, restart Desktop
                //       now.
                ulrc = (ULONG)qmsg.mp2;
                // do not dispatch this
                break;
            }

            WinDispatchMsg(hab, &qmsg);
        }
    }

    return (ulrc);
}

/*
        LTEXT           "You have held down the Shift key while the WPS is i"
                        "nitializing.", -1, 5, 182, 210, 8, DT_WORDBREAK
                        PRESPARAMS PP_FONTNAMESIZE, "8.Helv"
        LTEXT           "You can now selectively disable certain XWorkplace "
                        "features in case these don't work right.", -1, 5,
                        164, 210, 17, DT_WORDBREAK
                        PRESPARAMS PP_FONTNAMESIZE, "8.Helv"
        LTEXT           "Note that most of these changes will only affect th"
                        "is one WPS session. To permanently disable a featur"
                        "e, open the respective XWorkplace settings object a"
                        "fter the WPS has started.", -1, 5, 146, 210, 20,
                        DT_WORDBREAK
                        PRESPARAMS PP_FONTNAMESIZE, "8.Helv"
*/

CONTROLDEF
#ifndef __NOBOOTLOGO__
    SkipBootLogoCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,    // "Skip ~boot logo once",
                            ID_XFDI_PANIC_SKIPBOOTLOGO,
                            -1,
                            -1),
#endif
    SkipStartupFolderCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,    // "Skip XWorkplace S~tartup folder once",
                            ID_XFDI_PANIC_SKIPXFLDSTARTUP,
                            -1,
                            -1),
#ifndef __NOQUICKOPEN__
    SkipQuickOpenCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,    // "Skip ""~Quick open"" folders once",
                            ID_XFDI_PANIC_SKIPQUICKOPEN,
                            -1,
                            -1),
#endif
    SkipArchivingCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,    // "Skip WPS ~archiving once",
                            ID_XFDI_PANIC_NOARCHIVING,
                            -1,
                            -1),
    DisableFeaturesCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,    // "Permanently ~disable all features",
                            ID_XFDI_PANIC_DISABLEFEATURES,
                            -1,
                            -1),
#ifndef __NOICONREPLACEMENTS__
    DisableReplIconsCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,    // "Permanently disable ~icon replacements",
                            ID_XFDI_PANIC_DISABLEREPLICONS,
                            -1,
                            -1),
#endif
    RemoveHotkeysCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,    // "~Remove all object hotkeys",
                            ID_XFDI_PANIC_REMOVEHOTKEYS,
                            -1,
                            -1),
    DisablePageMageCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,    // "Disable ~PageMage",
                            ID_XFDI_PANIC_DISABLEPAGEMAGE,
                            -1,
                            -1),
    DisableMultimediaCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,    // "Disable ~multimedia",
                            ID_XFDI_PANIC_DISABLEMULTIMEDIA,
                            -1,
                            -1),
    ContinueButton = CONTROLDEF_DEFPUSHBUTTON(
                            LOAD_STRING,
                            ID_XFDI_PANIC_CONTINUE,
                            100,
                            30),
    ContinueText = CONTROLDEF_TEXT(
                            LOAD_STRING,
                            ID_XFDI_PANIC_CONTINUE_TXT,
                            -1,
                            -1),
    XFixButton = CONTROLDEF_PUSHBUTTON(
                            LOAD_STRING,
                            ID_XFDI_PANIC_XFIX,
                            100,
                            30),
    XFixText = CONTROLDEF_TEXT(
                            LOAD_STRING,
                            ID_XFDI_PANIC_XFIX_TXT,
                            -1,
                            -1),
    CmdButton = CONTROLDEF_PUSHBUTTON(
                            LOAD_STRING,
                            ID_XFDI_PANIC_CMD,
                            100,
                            30),
    CmdText = CONTROLDEF_TEXT(
                            LOAD_STRING,
                            ID_XFDI_PANIC_CMD_TXT,
                            -1,
                            -1),
    ShutdownButton = CONTROLDEF_PUSHBUTTON(
                            LOAD_STRING,
                            ID_XFDI_PANIC_SHUTDOWN,
                            100,
                            30),
    ShutdownText = CONTROLDEF_TEXT(
                            LOAD_STRING,
                            ID_XFDI_PANIC_SHUTDOWN_TXT,
                            -1,
                            -1);

DLGHITEM dlgPanic[] =
    {
        START_TABLE,            // root table, required
#ifndef __NOBOOTLOGO__
            START_ROW(0),
                CONTROL_DEF(&SkipBootLogoCB),
#endif
            START_ROW(0),
                CONTROL_DEF(&SkipStartupFolderCB),
#ifndef __NOQUICKOPEN__
            START_ROW(0),
                CONTROL_DEF(&SkipQuickOpenCB),
#endif
            START_ROW(0),
                CONTROL_DEF(&SkipArchivingCB),
            START_ROW(0),
                CONTROL_DEF(&DisableFeaturesCB ),
#ifndef __NOICONREPLACEMENTS__
            START_ROW(0),
                CONTROL_DEF(&DisableReplIconsCB),
#endif
            START_ROW(0),
                CONTROL_DEF(&RemoveHotkeysCB),
            START_ROW(0),
                CONTROL_DEF(&DisablePageMageCB),
            START_ROW(0),
                CONTROL_DEF(&DisableMultimediaCB),
            START_ROW(ROW_VALIGN_CENTER),
                CONTROL_DEF(&ContinueButton),
                CONTROL_DEF(&ContinueText),
            START_ROW(ROW_VALIGN_CENTER),
                CONTROL_DEF(&XFixButton),
                CONTROL_DEF(&XFixText),
            START_ROW(ROW_VALIGN_CENTER),
                CONTROL_DEF(&CmdButton),
                CONTROL_DEF(&CmdText),
            START_ROW(ROW_VALIGN_CENTER),
                CONTROL_DEF(&ShutdownButton),
                CONTROL_DEF(&ShutdownText),
        END_TABLE
    };

/*
 *@@ ShowPanicDlg:
 *
 *@@added V0.9.16 (2001-10-08) [umoeller]
 */

VOID ShowPanicDlg(VOID)
{
    BOOL    fRepeat = FALSE;

    while (    (doshQueryShiftState())
            || (fRepeat)       // set to TRUE after xfix V0.9.7 (2001-01-24) [umoeller]
          )
    {
        // shift pressed: show "panic" dialog
        ULONG   ulrc = 0;
        APIRET  arc;
        PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
        HWND hwndPanic;

        fRepeat = FALSE;

        cmnLoadDialogStrings(dlgPanic,
                             ARRAYITEMCOUNT(dlgPanic));

        if (!(arc = dlghCreateDlg(&hwndPanic,
                                  NULLHANDLE,
                                  FCF_TITLEBAR | FCF_SYSMENU | FCF_DLGBORDER | FCF_NOBYTEALIGN,
                                  WinDefDlgProc,
                                  cmnGetString(ID_XFDI_PANIC_TITLE),
                                  dlgPanic,
                                  ARRAYITEMCOUNT(dlgPanic),
                                  NULL,
                                  cmnQueryDefaultFont())))
        {
            winhCenterWindow(hwndPanic);

            // disable items which are irrelevant
#ifndef __NOBOOTLOGO__
            winhEnableDlgItem(hwndPanic, ID_XFDI_PANIC_SKIPBOOTLOGO,

                             cmnIsFeatureEnabled(BootLogo));
#endif
            winhEnableDlgItem(hwndPanic, ID_XFDI_PANIC_NOARCHIVING,
                             pGlobalSettings->fReplaceArchiving);
#ifndef __NOICONREPLACEMENTS__
            winhEnableDlgItem(hwndPanic, ID_XFDI_PANIC_DISABLEREPLICONS,
                              cmnIsFeatureEnabled(IconReplacements));
#endif
            winhEnableDlgItem(hwndPanic, ID_XFDI_PANIC_DISABLEPAGEMAGE,
                             pGlobalSettings->fEnablePageMage);
            winhEnableDlgItem(hwndPanic, ID_XFDI_PANIC_DISABLEMULTIMEDIA,
                             (xmmQueryStatus() == MMSTAT_WORKING));

            ulrc = WinProcessDlg(hwndPanic);

            switch (ulrc)
            {
                case ID_XFDI_PANIC_CONTINUE:        // continue
                {
#ifndef __NOBOOTLOGO__
                    if (winhIsDlgItemChecked(hwndPanic, ID_XFDI_PANIC_SKIPBOOTLOGO))
                        G_KernelGlobals.ulPanicFlags |= SUF_SKIPBOOTLOGO;
#endif
                    if (winhIsDlgItemChecked(hwndPanic, ID_XFDI_PANIC_SKIPXFLDSTARTUP))
                        G_KernelGlobals.ulPanicFlags |= SUF_SKIPXFLDSTARTUP;
#ifndef __NOQUICKOPEN__
                    if (winhIsDlgItemChecked(hwndPanic, ID_XFDI_PANIC_SKIPQUICKOPEN))
                        G_KernelGlobals.ulPanicFlags |= SUF_SKIPQUICKOPEN;
#endif
                    if (winhIsDlgItemChecked(hwndPanic, ID_XFDI_PANIC_NOARCHIVING))
                    {
                        PARCHIVINGSETTINGS pArcSettings = arcQuerySettings();
                        // disable "check archives" flag
                        pArcSettings->ulArcFlags &= ~ARCF_ENABLED;
                    }
#ifndef __NOICONREPLACEMENTS__
                    if (winhIsDlgItemChecked(hwndPanic, ID_XFDI_PANIC_DISABLEREPLICONS))
                    {
                        GLOBALSETTINGS *pGlobalSettings2 = cmnLockGlobalSettings(__FILE__, __LINE__, __FUNCTION__);
                        if (pGlobalSettings2)
                        {
                            pGlobalSettings2->__fIconReplacements = FALSE;
                            cmnUnlockGlobalSettings();
                            cmnStoreGlobalSettings();
                        }
                    }
#endif
                    if (winhIsDlgItemChecked(hwndPanic, ID_XFDI_PANIC_DISABLEPAGEMAGE))
                    {
                        GLOBALSETTINGS *pGlobalSettings2 = cmnLockGlobalSettings(__FILE__, __LINE__, __FUNCTION__);
                        if (pGlobalSettings2)
                        {
                            pGlobalSettings2->fEnablePageMage = FALSE;  // @@todo
                            cmnUnlockGlobalSettings();
                            cmnStoreGlobalSettings();
                        }
                    }
                    if (winhIsDlgItemChecked(hwndPanic, ID_XFDI_PANIC_DISABLEMULTIMEDIA))
                    {
                        xmmDisable();
                    }
                    if (winhIsDlgItemChecked(hwndPanic, ID_XFDI_PANIC_DISABLEFEATURES))
                    {
                        cmnLoadGlobalSettings(TRUE);        // reset defaults
                        cmnStoreGlobalSettings();
                    }
                    if (winhIsDlgItemChecked(hwndPanic, ID_XFDI_PANIC_REMOVEHOTKEYS))
                        PrfWriteProfileData(HINI_USER,
                                            INIAPP_XWPHOOK,
                                            INIKEY_HOOK_HOTKEYS,
                                            0, 0);      // delete INI key
                break; }

                case ID_XFDI_PANIC_XFIX:      // run xfix:
                {
                    CHAR        szXfix[CCHMAXPATH];
                    PROGDETAILS pd = {0};
                    HAPP        happXFix;
                    cmnQueryXWPBasePath(szXfix);
                    strcat(szXfix, "\\bin\\xfix.exe");

                    pd.Length = sizeof(pd);
                    pd.progt.progc = PROG_PM;
                    pd.progt.fbVisible = SHE_VISIBLE;
                    pd.pszExecutable = szXfix;
                    happXFix = appStartApp(G_KernelGlobals.hwndThread1Object,
                                           &pd,
                                           0); // V0.9.14

                    if (WaitForApp(szXfix,
                                   happXFix)
                        == 1)
                    {
                        // handle section changed:
                        cmnMessageBoxMsg(NULLHANDLE,
                                         121,       // xwp
                                         205,       // restart wps now.
                                         MB_OK);
                        DosExit(EXIT_PROCESS, 0);
                    }

                    fRepeat = TRUE;
                break; }

                case ID_XFDI_PANIC_CMD:         // run cmd.exe
                {
                    HAPP happCmd;
                    PROGDETAILS pd = {0};
                    pd.Length = sizeof(pd);
                    pd.progt.progc = PROG_WINDOWABLEVIO;
                    pd.progt.fbVisible = SHE_VISIBLE;
                    pd.pszExecutable = "*";        // use OS2_SHELL
                    happCmd = appStartApp(G_KernelGlobals.hwndThread1Object,
                                          &pd,
                                          0); // V0.9.14
                    WaitForApp(getenv("OS2_SHELL"),
                               happCmd);
                break; }

                case ID_XFDI_PANIC_SHUTDOWN:        // shutdown
                    // "Shutdown" pressed:
                    WinShutdownSystem(WinQueryAnchorBlock(HWND_DESKTOP),
                                      WinQueryWindowULong(HWND_DESKTOP, QWL_HMQ));
                    while (TRUE)
                        DosSleep(1000);
            }

            WinDestroyWindow(hwndPanic);
        }
    }
}

/*
 *@@ ShowStartupDlgs:
 *      this gets called from initMain
 *      to show dialogs while the WPS is starting up.
 *
 *      If XWorkplace was just installed, we'll show
 *      an introductory page and offer to convert
 *      XFolder settings, if found.
 *
 *      If XWorkplace has just been installed, we show
 *      an introductory message that "Shift" will show
 *      the "panic" dialog.
 *
 *      If "Shift" is currently pressed, we'll show the
 *      "Panic" dialog.
 *
 *@@added V0.9.1 (99-12-18) [umoeller]
 */

VOID ShowStartupDlgs(VOID)
{
    ULONG   cbData = 0;

    // check if XWorkplace was just installed
    if (PrfQueryProfileInt(HINI_USER,
                           (PSZ)INIAPP_XWORKPLACE,
                           (PSZ)INIKEY_JUSTINSTALLED,
                           0x123) != 0x123)
    {
        // yes: explain the "Panic" feature
        cmnMessageBoxMsg(HWND_DESKTOP,
                         121,       // "XWorkplace"
                         159,       // "press shift for panic"
                         MB_OK);
    }

    /*
     * convert XFolder settings
     *
     */

    if (PrfQueryProfileSize(HINI_USER,
                            (PSZ)INIAPP_XWORKPLACE,
                            (PSZ)INIKEY_GLOBALSETTINGS,
                            &cbData)
            == FALSE)
    {
        // XWorkplace keys do _not_ exist:
        // check if we have old XFolder settings
        if (PrfQueryProfileSize(HINI_USER,
                                (PSZ)INIAPP_OLDXFOLDER,
                                (PSZ)INIKEY_GLOBALSETTINGS,
                                &cbData))
        {
            if (cmnMessageBoxMsg(HWND_DESKTOP,
                                 121,       // "XWorkplace"
                                 160,       // "convert?"
                                 MB_YESNO)
                    == MBID_YES)
            {
                // yes, convert:
                // copy keys from "XFolder" to "XWorkplace"
                ULONG   ul;
                for (ul = 0;
                     ul < sizeof(G_appszXFolderKeys) / sizeof(G_appszXFolderKeys[0]);
                     ul++)
                {
                    prfhCopyKey(HINI_USER,
                                INIAPP_OLDXFOLDER,      // source
                                *G_appszXFolderKeys[ul],
                                HINI_USER,
                                INIAPP_XWORKPLACE);
                }

                // reload
                cmnLoadGlobalSettings(FALSE);
            }
        }
    }

    /*
     * "Panic" dlg
     *
     */

    ShowPanicDlg();

#ifndef __ALWAYSSUBCLASS__
    if (getenv("XWP_NO_SUBCLASSING"))
    {
        // V0.9.3 (2000-04-26) [umoeller]
        GLOBALSETTINGS *pGlobalSettings2 = cmnLockGlobalSettings(__FILE__, __LINE__, __FUNCTION__);
        if (pGlobalSettings2)
        {
            pGlobalSettings2->__fNoSubclassing = TRUE;
            cmnUnlockGlobalSettings();
            // _Pmpf(("ShowStartupDlgs: disabled subclassing"));
        }
    }
#endif
}

/*
 *@@ ReplaceWheelWatcher:
 *      blocks out the standard WPS "WheelWatcher" thread
 *      (which usually does the DosFindNotify* stuff)
 *      and creates a new thread in XWP instead.
 *
 *      Gets called _after_ the panic dialog, but _before_
 *      the additional XWP threads are started.
 *
 *@@added V0.9.9 (2001-01-31) [umoeller]
 *@@changed V0.9.10 (2001-04-08) [umoeller]: added exception handling
 */

VOID ReplaceWheelWatcher(FILE *DumpFile)
{
    APIRET      arc = NO_ERROR;

    TRY_LOUD(excpt1)        // V0.9.10 (2001-04-08) [umoeller]
    {
        HQUEUE      hqWheelWatcher = NULLHANDLE;

        if (DumpFile)
        {
            PQPROCSTAT16 pInfo;

            fprintf(DumpFile,
                    "\nEntering " __FUNCTION__":\n");

            if (!(arc = prc16GetInfo(&pInfo)))
            {
                // find WPS entry in process info
                PQPROCESS16 pProcess;
                if (pProcess = prc16FindProcessFromPID(pInfo,
                                                       G_KernelGlobals.pidWPS))
                {
                    // we now have the process info for the second PMSHELL.EXE...
                    ULONG       ul;
                    PQTHREAD16  pThread;

                    fprintf(DumpFile,
                            "  Running WPS threads at this point:\n");

                    for (ul = 0, pThread = (PQTHREAD16)PTR(pProcess->ulThreadList, 0);
                         ul < pProcess->usThreads;
                         ul++, pThread++ )
                    {
                        // CHAR    sz[100];
                        HENUM   henum;
                        HWND    hwndThis;
                        fprintf(DumpFile,
                                "    Thread %02d has priority 0x%04lX\n",
                                pThread->usTID,
                                pThread->ulPriority);

                        henum = WinBeginEnumWindows(HWND_OBJECT);
                        while (hwndThis = WinGetNextWindow(henum))
                        {
                            PID pid;
                            TID tid;
                            if (    (WinQueryWindowProcess(hwndThis, &pid, &tid))
                                 && (pid == G_KernelGlobals.pidWPS)
                                 && (tid == pThread->usTID)
                               )
                            {
                                CHAR szClass[100];
                                WinQueryClassName(hwndThis, sizeof(szClass), szClass);
                                fprintf(DumpFile,
                                        "        object wnd 0x%lX (%s)\n",
                                        hwndThis,
                                        szClass);
                            }
                        }
                        WinEndEnumWindows(henum);
                    } // end for (ul = 0, pThread =...
                }

                prc16FreeInfo(pInfo);       // V0.9.10 (2001-04-08) [umoeller]
            }
            else
            {
                fprintf(DumpFile,
                        "  !!! Cannot get WPS thread info, prc16GetInfo returned %d.\n", arc);
                cmnLog(__FILE__, __LINE__, __FUNCTION__,
                       "prc16GetInfo returned %d.", arc);
            }
        }

        // now lock out the WheelWatcher thread...
        // that thread is started AFTER us, and it attempts to
        // create a CP queue of the below name. If it cannot do
        // that, it will simply exit. So... by creating a queue
        // of the same name, the WheelWatcher will get an error
        // later, and exit.
        arc = DosCreateQueue(&hqWheelWatcher,
                             QUE_FIFO,
                             "\\QUEUES\\FILESYS\\NOTIFY");
        if (DumpFile)
            fprintf(DumpFile,
                    "  Created HQUEUE 0x%lX (DosCreateQueue returned %d)\n",
                    hqWheelWatcher,
                    arc);

        if (arc == NO_ERROR)
        {
            // we got the queue: then our assumption was valid
            // that we are indeed running _before_ WheelWatcher here...
            // create our own thread instead
            thrCreate(&G_tiSentinel,
                      refr_fntSentinel,
                      NULL,
                      "NotifySentinel",
                      THRF_WAIT,
                      0);           // no data here

            if (DumpFile)
                fprintf(DumpFile,
                        "  Started XWP Sentinel thread, TID: %d\n",
                        G_tiSentinel.tid);

            G_KernelGlobals.fAutoRefreshReplaced = TRUE;
        }
    }
    CATCH(excpt1) {} END_CATCH();
}

/*
 *@@ CheckDesktopValid:
 *      called from InitMain to check if <WP_DESKTOP>
 *      can be found on the system. The dull WPS
 *      "Cannot find desktop" dialog comes up _after_
 *      initMain completes, so we have a chance of
 *      fixing the desktop beforehand.
 *
 *@@added V0.9.16 (2001-09-29) [umoeller]
 */

VOID CheckDesktopValid(FILE *DumpFile)
{
    HOBJECT hobjDesktop = 0;
    ULONG cb = sizeof(hobjDesktop);

    fprintf(DumpFile,
            "\nEntering " __FUNCTION__":\n");

    if (!PrfQueryProfileData(HINI_USER,
                             (PSZ)WPINIAPP_LOCATION,      // "PM_Workplace:Location"
                             (PSZ)WPOBJID_DESKTOP,        // "<WP_DESKTOP>"
                             &hobjDesktop,
                             &cb))
        fprintf(DumpFile,
                "  ERROR: Cannot find <WP_DESKTOP> in PM_Workplace:Location in OS2.INI\n");
    else
        if (!hobjDesktop)
            fprintf(DumpFile,
                    "  ERROR: <WP_DESKTOP> in PM_Workplace:Location in OS2.INI has a null handle\n");
        else
        {
            // OK, check if that handle is valid.
            CHAR szDesktopPath[2*CCHMAXPATH];
            APIRET arc;
            if (arc = wphQueryPathFromHandle(HINI_USER,
                                             HINI_SYSTEM,
                                             hobjDesktop,
                                             szDesktopPath,
                                             sizeof(szDesktopPath)))
                fprintf(DumpFile,
                        "  ERROR %d resolving <WP_DESKTOP> handle 0x%lX\n",
                        hobjDesktop);
            else
            {
                ULONG ulAttr;

                fprintf(DumpFile,
                        "  <WP_DESKTOP> handle 0x%lX points to \"%s\"\n",
                        hobjDesktop,
                        szDesktopPath);
                if (arc = doshQueryPathAttr(szDesktopPath,
                                            &ulAttr))
                    fprintf(DumpFile,
                            "  ERROR: doshQueryPathAttr returned %d\n",
                            arc);
                else
                    if (0 == (ulAttr & FILE_DIRECTORY))
                        fprintf(DumpFile,
                                "  ERROR: Desktop path \"%s\" is not a directory.\n",
                                arc);
            }
        }
}

/*
 *@@ initMain:
 *      this gets called from M_XFldObject::wpclsInitData
 *      when the WPS is initializing. See remarks there.
 *
 *      From what I've checked, this function always gets
 *      called on thread 1 of PMSHELL.EXE.
 *
 *      As said there, at this point we own the computer
 *      all alone. Roughly speaking, the WPS has the following
 *      status:
 *
 *      --  The SOM kernel appears to be fully initialized.
 *
 *      --  The WPObject class object (_WPObject) is _being_
 *          initialized. The SOM kernel initializes a class
 *          object when the first instance (object) of a
 *          class is being created. I have no idea which
 *          object gets initialized first by the WPS, but
 *          maybe it's even the Desktop.
 *          Or maybe the class object is initialized
 *          explicitly by PMSHELL.EXE, while it is processing
 *          the WPS class list from OS2.INI, who knows.
 *
 *      --  A number of WPS threads are already running... I
 *          can count 12 here at the time this function is called.
 *          But they won't interfere with anything we're doing here,
 *          so we can suspend the boot process for as long as we
 *          want to (e.g. for the "panic" dialogs).
 *
 *      So what we're doing here is the following (this is a
 *      bit complex):
 *
 *      a) Initialize XWorkplace's globals: the GLOBALSETTINGS,
 *         the KERNELGLOBALS, and such.
 *
 *      b) Create the Thread-1 object window (fnwpThread1Object)
 *         and API object window (fnwpAPIObject).
 *
 *      c) If the "Shift" key is pressed, show the "Panic" dialog
 *         (new with V0.9.0). In that case, we pause the WPS
 *         bootup simply by not returning from this function
 *         until the dialog is dismissed.
 *
 *      d) Hack out the WPS folder auto-refresh threads, if enabled,
 *         and start the Sentinel thread (see ReplaceWheelWatcher).
 *
 *      e) Call xthrStartThreads to have the additional XWorkplace
 *         threads started. The Speedy thread will then display the
 *         boot logo, if allowed.
 *
 *      f) Start the XWorkplace daemon (XWPDAEMN.EXE, xwpdaemn.c),
 *         which will register the XWorkplace hook (XWPHOOK.DLL,
 *         xwphook.c, all new with V0.9.0). See xwpdaemon.c for details.
 *
 *      g) Finally, we call arcCheckIfBackupNeeded (archives.c)
 *         to enable Desktop archiving, if necessary. The WPS will
 *         then archive the Desktop, after we return from this
 *         function (also new with V0.9.0).
 *
 *@@changed V0.9.0 [umoeller]: renamed from xthrInitializeThreads
 *@@changed V0.9.0 [umoeller]: added dialog for shift key during Desktop startup
 *@@changed V0.9.0 [umoeller]: added XWorkplace daemon/hook
 *@@changed V0.9.0 [umoeller]: added Desktop archiving
 *@@changed V0.9.1 (99-12-19) [umoeller]: added NumLock at startup
 *@@changed V0.9.3 (2000-04-27) [umoeller]: added PM error windows
 *@@changed V0.9.5 (2000-08-10) [umoeller]: added XWPSHELL interface
 *@@changed V0.9.7 (2000-12-13) [umoeller]: moved config.sys path composition here
 *@@changed V0.9.7 (2000-12-17) [umoeller]: got crashes if archiving displayed a msg box; moved archiving up
 *@@changed V0.9.9 (2001-03-23) [umoeller]: added API object window
 *@@changed V0.9.14 (2001-08-21) [umoeller]: finally added setting for writing startup log
 *@@changed V0.9.16 (2001-09-26) [umoeller]: renamed from krnInitializeXWorkplace
 *@@changed V0.9.16 (2001-09-29) [umoeller]: added CheckDesktopValid()
 */

VOID initMain(VOID)
{
    FILE        *DumpFile = NULL;

    static BOOL fInitialized = FALSE;

    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    // check if we're called for the first time,
    // because we better initialize this only once
    if (!fInitialized)
    {
        // moved this here from xthrStartThreads
        PTIB        ptib;
        PPIB        ppib;

        CHAR        szDumpFile[CCHMAXPATH];

        fInitialized = TRUE;

        if (pGlobalSettings->fWriteXWPStartupLog)       // V0.9.14 (2001-08-21) [umoeller]
        {
            sprintf(szDumpFile, "%c:\\xwpstart.log", doshQueryBootDrive());

            if (DumpFile = fopen(szDumpFile, "w"))
            {
                DATETIME dt;
                DosGetDateTime(&dt);
                fprintf(DumpFile,
                        "\nXWorkplace startup log -- Date: %04d-%02d-%02d, Time: %02d:%02d:%02d\n",
                        dt.year, dt.month, dt.day,
                        dt.hours, dt.minutes, dt.seconds);
                fprintf(DumpFile,
                        "----------------------------------------------------------\n");

                fprintf(DumpFile,
                        "\nEntering " __FUNCTION__":\n");
            }
        }

        // zero KERNELGLOBALS
        memset(&G_KernelGlobals, 0, sizeof(KERNELGLOBALS));

        // moved this here from xthrStartThreads V0.9.9 (2001-01-31) [umoeller]
        if (DosGetInfoBlocks(&ptib, &ppib) == NO_ERROR)
        {
            if (ppib)
                G_KernelGlobals.pidWPS = ppib->pib_ulpid;
            if (ptib && ptib->tib_ptib2)
                G_KernelGlobals.tidWorkplaceThread = ptib->tib_ptib2->tib2_ultid;

            if (DumpFile)
                fprintf(DumpFile,
                        "\n" __FUNCTION__ ": PID 0x%lX, TID 0x%lX\n",
                        G_KernelGlobals.pidWPS,
                        G_KernelGlobals.tidWorkplaceThread);
        }

        #ifdef __XWPMEMDEBUG__
            // set global memory error callback
            G_pMemdLogFunc = krnMemoryError;
        #endif

        // store Desktop startup time
        DosGetDateTime(&G_KernelGlobals.StartupDateTime);

        // get PM system error windows V0.9.3 (2000-04-28) [umoeller]
        winhFindPMErrorWindows(&G_KernelGlobals.hwndHardError,
                               &G_KernelGlobals.hwndSysError);

        // initialize awake-objects list (which holds
        // plain WPObject* pointers)
        // G_KernelGlobals.pllAwakeObjects = lstCreate(FALSE);   // no auto-free items
                                        // moved to xthreads.c V0.9.9 (2001-04-04) [umoeller]

        // register exception hooks for /helpers/except.c
        excRegisterHooks(krnExceptOpenLogFile,
                         krnExceptExplainXFolder,
                         krnExceptError,
                         !pGlobalSettings->fNoExcptBeeps);

        // create thread-1 object window
        WinRegisterClass(WinQueryAnchorBlock(HWND_DESKTOP),
                         (PSZ)WNDCLASS_THREAD1OBJECT,    // class name
                         (PFNWP)fnwpThread1Object,   // Window procedure
                         0,                  // class style
                         0);                 // extra window words
        G_KernelGlobals.hwndThread1Object
            = winhCreateObjectWindow(WNDCLASS_THREAD1OBJECT, // class name
                                     NULL);        // create params

        if (DumpFile)
            fprintf(DumpFile,
                    "XWorkplace thread-1 object window created, HWND 0x%lX\n",
                    G_KernelGlobals.hwndThread1Object);

        // store HAB of WPS thread 1 V0.9.9 (2001-04-04) [umoeller]
        G_habThread1 = WinQueryAnchorBlock(G_KernelGlobals.hwndThread1Object);

        // create API object window V0.9.9 (2001-03-23) [umoeller]
        WinRegisterClass(WinQueryAnchorBlock(HWND_DESKTOP),
                         (PSZ)WNDCLASS_APIOBJECT,    // class name
                         (PFNWP)fnwpAPIObject,   // Window procedure
                         0,                  // class style
                         0);                 // extra window words
        G_KernelGlobals.hwndAPIObject
            = winhCreateObjectWindow(WNDCLASS_APIOBJECT, // class name
                                     NULL);        // create params

        if (DumpFile)
            fprintf(DumpFile,
                    "XWorkplace API object window created, HWND 0x%lX\n",
                    G_KernelGlobals.hwndAPIObject);

        // if shift is pressed, show "Panic" dialog
        // V0.9.7 (2001-01-24) [umoeller]: moved this behind creation
        // of thread-1 window... we need this for starting xfix from
        // the "panic" dlg
        ShowStartupDlgs();

        // check if "replace folder refresh" is enabled...
        if (krnReplaceRefreshEnabled())
        {
            // yes: kick out WPS wheel watcher thread,
            // start our own one instead
            ReplaceWheelWatcher(DumpFile);
        }
    }

    /*
     *  enable NumLock at startup
     *      V0.9.1 (99-12-19) [umoeller]
     */

    if (pGlobalSettings->fNumLockStartup)
        winhSetNumLock(TRUE);

    // go check if the desktop is valid
    CheckDesktopValid(DumpFile);

    // initialize multimedia V0.9.3 (2000-04-25) [umoeller]
    xmmInit(DumpFile);
            // moved this down V0.9.9 (2001-01-31) [umoeller]

    /*
     *  initialize threads
     *
     */

    xthrStartThreads(DumpFile);

    /*
     *  check Desktop archiving (V0.9.0)
     *      moved this up V0.9.7 (2000-12-17) [umoeller];
     *      we get crashes if a msg box is displayed otherwise
     */

    if (pGlobalSettings->fReplaceArchiving)
        // check whether we need a WPS backup (archives.c)
        arcCheckIfBackupNeeded(G_KernelGlobals.hwndThread1Object,
                               T1M_DESTROYARCHIVESTATUS);

    /*
     *  start XWorkplace daemon (XWPDAEMN.EXE)
     *
     */

    {
        // check for the XWPGLOBALSHARED structure, which
        // is used for communication between the daemon
        // and XFLDR.DLL (see src/Daemon/xwpdaemn.c).
        // We take advantage of the fact that OS/2 keeps
        // reference of the processes which allocate or
        // request access to a block of shared memory.
        // The XWPGLOBALSHARED struct is allocated here
        // (just below) and requested by the daemon.
        //
        // -- If requesting the shared memory works at this point,
        //    this means that the daemon is still running!
        //    This happens after a Desktop restart. We'll then
        //    skip the rest.
        //
        // -- If requesting the shared memory fails, this means
        //    that the daemon is _not_ running (the WPS is started
        //    for the first time). We then allocate the shared
        //    memory and start the daemon, which in turn requests
        //    this shared memory block. Note that this also happens
        //    if the daemon stopped for some reason (crash, kill)
        //    and the user then restarts the WPS.

        PXWPGLOBALSHARED pXwpGlobalShared = 0;
        APIRET arc = DosGetNamedSharedMem((PVOID*)&pXwpGlobalShared,
                                          SHMEM_XWPGLOBAL,
                                          PAG_READ | PAG_WRITE);

        if (DumpFile)
            fprintf(DumpFile,
                    "\nAttempted to access " SHMEM_XWPGLOBAL ", DosGetNamedSharedMem returned %d\n",
                    arc);

        if (arc != NO_ERROR)
        {
            // BOOL    fDaemonStarted = FALSE;

            // shared mem does not exist:
            // --> daemon not running; probably first WPS
            // startup, so we allocate the shared mem now and
            // start the XWorkplace daemon

            if (DumpFile)
                fprintf(DumpFile, "--> XWPDAEMN not running, starting now.\n");

            arc = DosAllocSharedMem((PVOID*)&pXwpGlobalShared,
                                    SHMEM_XWPGLOBAL,
                                    sizeof(XWPGLOBALSHARED), // rounded up to 4KB
                                    PAG_COMMIT | PAG_READ | PAG_WRITE);

            if (DumpFile)
                fprintf(DumpFile, "  DosAllocSharedMem returned %d\n", arc);

            if (arc == NO_ERROR)
            {
                CHAR    szDir[CCHMAXPATH],
                        szExe[CCHMAXPATH];

                // shared mem successfully allocated:
                memset(pXwpGlobalShared, 0, sizeof(XWPGLOBALSHARED));
                // store the thread-1 object window, which
                // gets messages from the daemon
                pXwpGlobalShared->hwndThread1Object = G_KernelGlobals.hwndThread1Object;
                pXwpGlobalShared->hwndAPIObject = G_KernelGlobals.hwndAPIObject;
                        // V0.9.9 (2001-03-23) [umoeller]
                pXwpGlobalShared->ulWPSStartupCount = 1;
                // at the first Desktop start, always process startup folder
                pXwpGlobalShared->fProcessStartupFolder = TRUE;

                // now start the daemon;
                // we need to specify XWorkplace's "BIN"
                // subdir as the working dir, because otherwise
                // the daemon won't find XWPHOOK.DLL.

                // compose paths
                if (cmnQueryXWPBasePath(szDir))
                {
                    // path found:
                    PROGDETAILS pd;
                    // working dir: append bin
                    strcat(szDir, "\\bin");
                    // exe: append bin\xwpdaemon.exe
                    sprintf(szExe,
                            "%s\\xwpdaemn.exe",
                            szDir);
                    memset(&pd, 0, sizeof(pd));
                    pd.Length = sizeof(PROGDETAILS);
                    pd.progt.progc = PROG_PM;
                    pd.progt.fbVisible = SHE_VISIBLE;
                    pd.pszTitle = "XWorkplace Daemon";
                    pd.pszExecutable = szExe;
                    pd.pszParameters = "-D";
                    pd.pszStartupDir = szDir;
                    pd.pszEnvironment = "WORKPLACE\0\0";

                    G_KernelGlobals.happDaemon = WinStartApp(G_KernelGlobals.hwndThread1Object, // hwndNotify,
                                                             &pd,
                                                             "-D", // params; otherwise the daemon
                                                                   // displays a msg box
                                                             NULL,
                                                             0);// no SAF_INSTALLEDCMDLINE,
                    if (DumpFile)
                        fprintf(DumpFile,
                                "  WinStartApp for %s returned HAPP 0x%lX\n",
                                pd.pszExecutable,
                                G_KernelGlobals.happDaemon);

                    /* if (!G_KernelGlobals.happDaemon)
                        // success:
                        fDaemonStarted = TRUE; */
                }
            } // end if DosAllocSharedMem

        } // end if DosGetNamedSharedMem
        else
        {
            // shared memory block already exists:
            // this means the daemon is already running
            // and we have a Desktop restart

            if (DumpFile)
                fprintf(DumpFile, "--> XWPDAEMN already running, refreshing.\n");

            // store new thread-1 object wnd
            pXwpGlobalShared->hwndThread1Object = G_KernelGlobals.hwndThread1Object;
            pXwpGlobalShared->hwndAPIObject = G_KernelGlobals.hwndAPIObject;
                        // V0.9.9 (2001-03-23) [umoeller]

            // increase Desktop startup count
            pXwpGlobalShared->ulWPSStartupCount++;

            if (pXwpGlobalShared->hwndDaemonObject)
            {
                WinSendMsg(pXwpGlobalShared->hwndDaemonObject,
                           XDM_HOOKCONFIG,
                           0, 0);
                WinSendMsg(pXwpGlobalShared->hwndDaemonObject,
                           XDM_HOTKEYSCHANGED,
                           0, 0);
                    // cross-process post, synchronously:
                    // this returns only after the hook has been re-initialized
            }
            // we leave the "reuse startup folder" flag alone,
            // because this was already set by XShutdown before
            // the last Desktop restart
        }

        G_KernelGlobals.pXwpGlobalShared = pXwpGlobalShared;
    }

    /*
     *  interface XWPSHELL.EXE
     *
     */

    {
        PXWPSHELLSHARED pXWPShellShared = 0;
        APIRET arc = DosGetNamedSharedMem((PVOID*)&pXWPShellShared,
                                          SHMEM_XWPSHELL,
                                          PAG_READ | PAG_WRITE);
        if (DumpFile)
            fprintf(DumpFile,
                    "\nAttempted to access " SHMEM_XWPSHELL ", DosGetNamedSharedMem returned %d\n",
                    arc);

        if (arc == NO_ERROR)
        {
            // shared memory exists:
            // this means that XWPSHELL.EXE is running...
            // store this in KERNELGLOBALS
            G_KernelGlobals.pXWPShellShared = pXWPShellShared;

            // set flag that WPS termination will not provoke
            // logon; this is in case WPS crashes or user
            // restarts WPS. Only "Logoff" desktop menu item
            // will clear that flag.
            pXWPShellShared->fNoLogonButRestart = TRUE;

            if (DumpFile)
                fprintf(DumpFile, "--> XWPSHELL running, refreshed; enabling multi-user mode.\n");
        }
        else
            if (DumpFile)
                fprintf(DumpFile, "--> XWPSHELL not running, going into single-user mode.\n");
    }

    if (DumpFile)
    {
        fprintf(DumpFile,
                        "\nLeaving " __FUNCTION__", closing log.\n");
        fclose(DumpFile);
    }

    // After this, startup continues normally...
    // XWorkplace comes in again after the desktop has been fully populated.
    // Our XFldDesktop::wpPopulate then posts FIM_DESKTOPPOPULATED to the
    // File thread, which will do things like the startup folder and such.
    // This will then start the startup thread (fntStartup), which finally
    // checks for whether XWP was just installed; if so, T1M_WELCOME is
    // posted to the thread-1 object window.

}

