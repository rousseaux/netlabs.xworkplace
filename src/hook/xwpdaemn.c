
/*
 *@@sourcefile xwpdaemn.c:
 *      this has the code for the XWorkplace Daemon process
 *      (XWPDAEMN.EXE), which is an invisible PM program with
 *      a single object window running in the background. This
 *      doesnot appear in the window list, but is only seen
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
 *      2.  Keeping track of WPS restarts, since the daemon
 *          keeps running even while the WPS restarts. Shared
 *          memory is used for communication.
 *
 *          The interaction between XWPDAEMN.EXE and XFLDR.DLL works
 *          as follows:
 *
 *          a) When the WPS is started for the first time (after
 *             a reboot), krnInitializeXWorkplace (XFLDR.DLL)
 *             allocates a DAEMONSHARED structure (xwphook.h)
 *             as a block of shared memory.
 *
 *          b) krnInitializeXWorkplace then starts XWPDAEMN.EXE.
 *
 *          c) The daemon then requests access to that block of
 *             shared memory, creates the daemon object window
 *             (fnwpDaemonObject) and sets DAEMONSHARED.fDaemonReady
 *             to TRUE so that krnInitializeXWorkplace can continue
 *             WPS startup. XDM_HOOKINSTALL is then sent to the
 *             object window to install the hook (if the hook is
 *             enabled in XWPSetup).
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
 *             realizes that the DAEMONSHARED structure is still
 *             in use and will thus find out that the WPS is not
 *             started for the first time. Instead, access to the
 *             existing DAEMONSHARED structure is requested (so
 *             we have a reference count of two again).
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
 *      3.  Removeable drives monitoring. I have put this into
 *          the daemon because we're dealing with DevIOCtl's
 *          here, which might not be a good idea to do in the
 *          PMSHELL.EXE process.
 *
 *      Both the hook and the daemon are all new with V0.9.0.
 *
 *@@added V0.9.0 [umoeller]
 *@@header "hook\xwphook.h"
 */

/*
 *      Copyright (C) 1999 Ulrich M”ller.
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, in version 2 as it comes in the COPYING
 *      file of the XFolder main distribution.
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 */

#define INCL_DOS
#define INCL_DOSDEVICES
#define INCL_DOSERRORS
#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WINPOINTERS
#define INCL_WINTIMER
#define INCL_WINSHELLDATA
#define INCL_WINWORKPLACE
#include <os2.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "hook\xwphook.h"               // hook and daemon definitions

#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel

#include "setup.h"                      // code generation and debugging options

/* ******************************************************************
 *                                                                  *
 *   Global variables                                               *
 *                                                                  *
 ********************************************************************/

HAB             habDaemon;
HMQ             hmqDaemon;

// pointer to hook data in hook dll;
// this includes HOOKCONFIG
PHOOKDATA       pHookData = NULL;
// pointer to shared-memory daemon globals
PDAEMONSHARED   pDaemonShared = NULL;

// sliding focus data
ULONG           ulSlidingFocusTimer = 0;    // timer ID for delayed sliding focus
HWND            hwndToActivate = 0;         // window to activate
                                            // (valid while timer is running)
BOOL            fIsSeamless = FALSE;        // hwndToActive is seamless Win-OS/2 window
                                            // (valid while timer is running)
// drive monitor
ULONG           ulMonitorTimer = 0;

// if the following is #define'd, we implement a
// system hook. If not, we implement only a "local"
// hook for our own msg queue. This is healthier for
// debugging. ;-)
#define GLOBAL

#define IOCTL_CDROMDISK             0x80
#define CDROMDISK_DEVICESTATUS      0x60
#define CDROMDISK_GETUPC            0x79

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
 */

VOID LoadHotkeysForHook(VOID)
{
    ULONG   ulSizeOfData = 0;
    // get size of data for pszApp/pszKey
    if (PrfQueryProfileSize(HINI_USER,
                            INIAPP_XWPHOOK,
                            INIKEY_HOOK_HOTKEYS,
                            &ulSizeOfData))
    {
        PGLOBALHOTKEY pHotkeys = (PGLOBALHOTKEY)malloc(ulSizeOfData);
        if (PrfQueryProfileData(HINI_USER,
                                INIAPP_XWPHOOK,
                                INIKEY_HOOK_HOTKEYS,
                                pHotkeys,
                                &ulSizeOfData))
        {
            // hotkeys successfully loaded:
            // calc no. of items in array
            ULONG cHotkeys = ulSizeOfData / sizeof(GLOBALHOTKEY);
            // notify hook
            hookSetGlobalHotkeys(pHotkeys, cHotkeys);
        }

        // we can safely free the hotkeys, because
        // the hook has copied them to shared memory
        free(pHotkeys);
    }
}

/*
 *@@ LoadHookConfig:
 *      this reloads the configuration data
 *      from OS2.INI directly into the shared
 *      global data segment of XWPHOOK.DLL.
 *
 *      This has all the configuration except
 *      for the object hotkeys.
 *
 *      This gets called from main() when the daemon
 *      is (re)started and from fnwpDaemonObject
 *      when the config data has changed.
 */

BOOL LoadHookConfig(VOID)
{
    BOOL brc = FALSE;
    if (pHookData)
    {
        ULONG cb =  sizeof(HOOKCONFIG);

        // safe defaults
        memset(&pHookData->HookConfig, 0, sizeof(HOOKCONFIG));

        // overwrite from INI, if found
        brc = PrfQueryProfileData(HINI_USER,
                                  INIAPP_XWPHOOK,
                                  INIKEY_HOOK_CONFIG,
                                  &pHookData->HookConfig,
                                  &cb);
    }

    return (brc);
}

/*
 *@@ InstallHook:
 *      this installs the hooks in XWPHOOK.DLL by calling
 *      hookInit in XWPHOOK.DLL and then loads the configuration
 *      by calling LoadHotkeysForHook and LoadHookConfig.
 */

VOID InstallHook(VOID)
{
    #ifndef GLOBAL
        pHookData = hookInit(pDaemonShared->hwndDaemonObject,
                             // for debugging purposes, we'll
                             // set this to our own msg queue only
                             hmqInit);
    #else
        pHookData = hookInit(pDaemonShared->hwndDaemonObject,
                             // if GLOBAL has been #define'd, we
                             // pass NULLHANDLE, meaning "install
                             // system hook"
                             NULLHANDLE);
    #endif

    _Pmpf(("hookInit called, pHookData: 0x%lX", pHookData));

    if (pHookData)
        if (    (pHookData->fInputHooked)
             && (pHookData->fPreAccelHooked)
           )
        {
            // success:
            pDaemonShared->fHookInstalled = TRUE;
            // load hotkeys list from OS2.INI
            LoadHotkeysForHook();
            // load config from OS2.INI
            LoadHookConfig();
        }
}

/*
 *@@ DeinstallHook:
 *      this removes the hooks by calling hookKill.
 */

VOID DeinstallHook(VOID)
{
    if (pDaemonShared->fHookInstalled)
    {
        hookKill();
        _Pmpf(("hookKilled called"));
        pDaemonShared->fHookInstalled = FALSE;
    }
    pHookData = NULL;
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
 *      At this point, the global hwndToActivate
 *      variable has been set to the desktop window
 *      which is to be activated.
 */

VOID ProcessSlidingFocus(VOID)
{
    HWND    hwndPreviousActive;

    // rule out desktops, if "ignore desktop" is on
    if (pHookData->HookConfig.fIgnoreDesktop)
        if (hwndToActivate == pHookData->hwndPMDesktop)
            // target is PM desktop:
            return;
        else if (hwndToActivate == pHookData->hwndWPSDesktop)
            // target is WPS desktop:
            return;

    // always ignore window list
    if (hwndToActivate == pHookData->hwndWindowList)
        return;

    if (WinQueryWindowULong(hwndToActivate, QWL_STYLE) & WS_DISABLED)
        // window is disabled: ignore
        return;

    hwndPreviousActive = WinQueryActiveWindow(HWND_DESKTOP);

    // and stop if currently active window is the window list too
    if (hwndPreviousActive == pHookData->hwndWindowList)
        return;

    // rule out redundant processing:
    // we might come here if the mouse has moved
    // across several desktop windows while the
    // sliding-focus delay has not elapsed yet
    // so that the active window really hasn't changed
    if (hwndPreviousActive != hwndToActivate)
    {
        if (    (pHookData->HookConfig.fBring2Top)
             || (   (fIsSeamless)
                 && (!pHookData->HookConfig.fIgnoreSeamless)
                )
           )
        {
            // bring-to-top mode
            // or window to activate is seamless Win-OS/2 window
            // and should be brought to the top:
            WinSetActiveWindow(HWND_DESKTOP, hwndToActivate);
            pHookData->hwndActivatedByUs = hwndToActivate;
        }
        else
        {
            // preserve-Z-order mode:

            if (!fIsSeamless)
            {
                // not seamless Win-OS/2 window:
                HWND    hwndFocusSave = WinQueryWindowULong(hwndToActivate,
                                                            QWL_HWNDFOCUSSAVE);
                HWND    hwndNewFocus = NULLHANDLE,
                        hwndTitlebar;

                if (hwndFocusSave)
                    // frame window has control with focus:
                    hwndNewFocus = hwndFocusSave;
                else
                {
                    // frame window has no saved focus:
                    // try if we can activate the client
                    HWND hwndClient = WinWindowFromID(hwndToActivate,
                                                      FID_CLIENT);
                    if (hwndClient)
                        hwndNewFocus = hwndClient;
                    else
                        // we don't even have a client:
                        // activate the frame directly
                        hwndNewFocus = hwndToActivate;
                }

                WinFocusChange(HWND_DESKTOP,
                               hwndNewFocus,
                               FC_NOSETACTIVE);
                // deactivate old window
                WinPostMsg(hwndPreviousActive,
                           WM_ACTIVATE,
                           MPFROMSHORT(FALSE),      // deactivate
                           MPFROMHWND(hwndNewFocus));
                // activate new window
                hwndTitlebar = WinWindowFromID(hwndToActivate, FID_TITLEBAR);
                if (hwndTitlebar)
                {
                    WinPostMsg(hwndToActivate,
                               WM_ACTIVATE,
                               MPFROMSHORT(TRUE),
                               MPFROMHWND(hwndTitlebar));
                    // store activated window in hook data
                    // so that the hook knows that we activated
                    // this window
                }
                pHookData->hwndActivatedByUs = hwndToActivate;
            }
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
 *      object window created in main().
 *
 *      This reacts to XDM_* messages (see xwphook.h)
 *      which are either posted from XWPHOOK.DLL for
 *      hotkeys, sliding focus, screen corner objects,
 *      or from XFLDR.DLL for daemon/hook configuration.
 */

MRESULT EXPENTRY fnwpDaemonObject(HWND hwndObject, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc;

    switch (msg)
    {
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
            // load config from OS2.INI
            mrc = (MPARAM)LoadHookConfig();
        break;

        /*
         *@@ XDM_HOOKINSTALL:
         *      this must be sent while the daemon
         *      is running to install or deinstall
         *      the hook. This does not affect operation
         *      of the daemon, which will keep running.
         *
         *      This is used by the XWPSetup "Features"
         *      page.
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
            if (mp1)
            {
                // install the hook:
                InstallHook();
                    // this sets the global pHookData pointer
                    // to the HOOKDATA in the DLL
            }
            else
                DeinstallHook();

            mrc = (MPARAM)(pDaemonShared->fHookInstalled);
        break;

        /*
         *@@ XDM_DESKTOPREADY:
         *      this gets posted from XFLDR.DLL after
         *      the WPS desktop frame has been opened
         *      so that the daemon/hook knows about the
         *      HWND of the WPS desktop. This is necessary
         *      for the sliding focus feature.
         *
         *      Parameters:
         *      -- HWND mp1: desktop frame HWND.
         *      -- mp2: unused, always 0.
         */

        case XDM_DESKTOPREADY:
            if (pHookData)
                pHookData->hwndWPSDesktop = (HWND)mp1;
        break;

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
                _Pmpf(("Forwarding 0x%lX to 0x%lX",
                        mp1,
                        pDaemonShared->hwndThread1Object));
                // forward msg (cross-process post)
                WinPostMsg(pDaemonShared->hwndThread1Object,
                           T1M_OPENOBJECTFROMHANDLE,
                           mp1,
                           mp2);
            }
        break;

        /*
         *@@ XDM_HOTKEYSCHANGED:
         *      message posted by XFLDR.DLL when the
         *      list of global object hotkeys has changed.
         *      This is posted after the new list has been
         *      written to OS2.INI so we can safely re-read
         *      this now and notify the hook of the change.
         *
         *      Parameters: none.
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
         *      -- HWND mp1: new desktop window handle
         *              (child of HWND_DESKTOP)
         *      -- BOOL mp2: TRUE if mp1 is a seamless Win-OS/2
         *              window
         */

        case XDM_SLIDINGFOCUS:
            hwndToActivate = (HWND)mp1;
            fIsSeamless = (BOOL)mp2;
            if (pHookData->HookConfig.ulSlidingFocusDelay)
            {
                // delayed sliding focus:
                if (ulSlidingFocusTimer)
                    WinStopTimer(habDaemon,
                                 hwndObject,
                                 TIMERID_SLIDINGFOCUS);
                ulSlidingFocusTimer = WinStartTimer(habDaemon,
                                                    hwndObject,
                                                    TIMERID_SLIDINGFOCUS,
                                                    pHookData->HookConfig.ulSlidingFocusDelay);
            }
            else
                ProcessSlidingFocus();
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
         *                  1 = lower left,
         *                  2 = top left,
         *                  3 = lower right,
         *                  4 = top right.
         *      -- mp2: unused, always 0.
         */

        case XDM_HOTCORNER:
            if (mp1)
            {
                // create array index
                LONG lIndex = ((LONG)mp1)-1;

                if ((lIndex >= 0) && (lIndex <= 3))
                    if (pHookData->HookConfig.ahobjHotCornerObjects[lIndex] != NULLHANDLE)
                        // hot-corner object defined:
                        WinPostMsg(pDaemonShared->hwndThread1Object,
                                   T1M_OPENOBJECTFROMHANDLE,
                                   (MPARAM)(pHookData->HookConfig.ahobjHotCornerObjects[lIndex]),
                                   (MPARAM)(lIndex + 1));

            }
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
                    WinStopTimer(habDaemon,
                                 hwndObject,
                                 (ULONG)mp1);   // timer ID
                    ulSlidingFocusTimer = NULLHANDLE;

                    ProcessSlidingFocus();
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
                    WinStopTimer(habDaemon,
                                 hwndObject,
                                 (ULONG)mp1);   // timer ID

                    pHookData->idAutoHideTimer = NULLHANDLE;

                    WinShowPointer(HWND_DESKTOP, FALSE);
                    pHookData->fMousePointerHidden = TRUE;
                break;
            }
        break;

        default:
            mrc = WinDefWindowProc(hwndObject, msg, mp1, mp2);
    }

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
    // if mouse pointer is currently hidden,
    // show it now (otherwise it'll be lost...)
    if (pHookData)
        if (pHookData->fMousePointerHidden)
            WinShowPointer(HWND_DESKTOP, TRUE);

    // de-install the hook
    DeinstallHook();
    if (pDaemonShared)
    {
        pDaemonShared->hwndDaemonObject = NULLHANDLE;
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
 *      to which the hook will post messages. Note that
 *      to actually install the hook, you must post
 *      XDM_HOOKINSTALL to the daemon object window.
 */

int main(int argc, char *argv[])
{
    habDaemon = WinInitialize(0);
    if (habDaemon)
    {
        hmqDaemon = WinCreateMsgQueue(habDaemon, 0);
        if (hmqDaemon)
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
                                      TRUE)
                     == NO_ERROR)
                {
                    // semaphore successfully created: this means
                    // that no other instance of XWPDAEMN.EXE is
                    // running

                    // access the shared memory allocated by
                    // XFLDR.DLL
                    APIRET arc = DosGetNamedSharedMem((PVOID*)&pDaemonShared,
                                                      IDSHMEM_DAEMON,
                                                      PAG_READ | PAG_WRITE);
                    if (arc == NO_ERROR)
                    {
                        // OK:
                        PSZ     pszInitClass = "XWorkplaceDaemonClass";

                        // install exit list
                        DosExitList(EXLST_ADD,
                                    DaemonExitList);

                        // create the object window
                        WinRegisterClass(habDaemon,
                                         pszInitClass,
                                         (PFNWP)fnwpDaemonObject,    // Window procedure
                                         0,                  // class style
                                         0);                 // extra window words
                        pDaemonShared->hwndDaemonObject
                                       = WinCreateWindow(HWND_OBJECT,
                                                         pszInitClass,
                                                         "XWorkplace PM Daemon",
                                                         0,           // style
                                                         0,0,0,0,     // position
                                                         0,           // owner
                                                         HWND_BOTTOM, // z-order
                                                         0x1000, // window id
                                                         NULL,        // create params
                                                         NULL);       // pres params

                        _Pmpf(("hwndDaemonObject: 0x%lX", pDaemonShared->hwndDaemonObject));

                        if (pDaemonShared->hwndDaemonObject)
                        {
                            QMSG    qmsg;

                            /* ulMonitorTimer = WinStartTimer(habDaemon,
                                                           pDaemonShared->hwndDaemonObject,
                                                           TIMERID_MONITORDRIVE,
                                                           2000); */

                            // OK: post msg to XFLDR.DLL thread-1 object window
                            // that we're ready, which will in turn send XDM_HOOKINSTALL
                            WinPostMsg(pDaemonShared->hwndThread1Object,
                                       T1M_DAEMONREADY,
                                       0, 0);

                            // standard PM message loop
                            while (WinGetMsg(habDaemon, &qmsg, 0, 0, 0))
                                WinDispatchMsg(habDaemon, &qmsg);

                            // then kill the hook again
                            DeinstallHook();
                        }

                        WinDestroyObject(pDaemonShared->hwndDaemonObject);
                        pDaemonShared->hwndDaemonObject = NULLHANDLE;
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

            WinDestroyMsgQueue(hmqDaemon);
        }
        WinTerminate(habDaemon);
        return (0);
    }
    return (99);
}

