
/*
 * xwphook.h:
 *      header for both xwphook.c and xwpdaemon.c. This is also
 *      included from a number of sources from filesys\ which need
 *      to interface the daemon.
 */

/*
 *      Copyright (C) 1999 Ulrich M”ller.
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, in version 2 as it comes in the COPYING
 *      file of the XWorkplace main distribution.
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 */

#ifndef XWPHOOK_HEADER_INCLUDED
    #define XWPHOOK_HEADER_INCLUDED

    /* ******************************************************************
     *                                                                  *
     *   Shared mem/semaphore IDs                                       *
     *                                                                  *
     ********************************************************************/

    #define IDSHMEM_DAEMON          "\\SHAREMEM\\XWORKPLC\\DAEMON.DAT"
            // DAEMONSHARED structure
    #define IDSHMEM_HOTKEYS         "\\SHAREMEM\\XWORKPLC\\HOTKEYS.DAT"
    #define IDMUTEX_ONEINSTANCE     "\\SEM32\\XWORKPLC\\ONEINST.SEM"
    #define SEM_TIMEOUT             4000

    // timer IDs for fnwpDaemonObject
    #define TIMERID_SLIDINGFOCUS        1
    #define TIMERID_MONITORDRIVE        2
    #define TIMERID_AUTOHIDEMOUSE       3

    /* ******************************************************************
     *                                                                  *
     *   OS2.INI applications and keys                                  *
     *                                                                  *
     ********************************************************************/

    #define INIAPP_XWPHOOK          "XWorkplace:Hook"   // added V0.9.0
    #define INIKEY_HOOK_HOTKEYS     "Hotkeys"           // added V0.9.0
    #define INIKEY_HOOK_CONFIG      "Config"            // added V0.9.0

    /* ******************************************************************
     *                                                                  *
     *   Structures                                                     *
     *                                                                  *
     ********************************************************************/

    // flags for HOOKCONFIG.usScrollMode
    #define SM_LINEWISE         0
    #define SM_AMPLIFIED        1

    /*
     *@@ HOOKCONFIG:
     *      configuration data for the hook and the daemon.
     *
     *      This is stored within the HOOKDATA structure
     *      (statically in the hook DLL) so that both
     *      the daemon and the hook have access to this.
     *
     *      A mirror of this structure is put into OS2.INI.
     *      This gets (re)loaded by the daemon when
     *      XDM_HOOKCONFIG is received by fnwpDaemonObject.
     *
     *      For every item, the safe default value is null
     *      so the structure can be zeroed to disable
     *      everything.
     *
     *      Note that the object hotkey _definitions_ are
     *      not part of this structure. Those are set using
     *      XDM_HOTKEYSCHANGED instead. However, object
     *      hotkeys are globally enabled in here (fGlobalHotkeys).
     */

    typedef struct _HOOKCONFIG
    {
        // Sliding focus:

        BOOL            fSlidingFocus;
                // enabled?
        ULONG           ulSlidingFocusDelay;
                // delay in ms; 0 = off

        BOOL            fBring2Top;
                // bring windows to top or preserve Z-order
        BOOL            fIgnoreDesktop;
                // ignore Desktop windows
        BOOL            fIgnoreSeamless;
                // TRUE: ignore seamless Win-OS/2 windows
                // FALSE: always bring them to top

        // Screen corner objects:

        HOBJECT         ahobjHotCornerObjects[4];
                            // Indices:
                            //      0 = lower left,
                            //      1 = top left,
                            //      2 = lower right,
                            //      3 = top right.
                            // If any item is NULLHANDLE, it means the
                            // corner is inactive (no function defined).
                            // If the hiword of the item is 0xFFFF, this
                            // means a special function has been defined.
                            // Currently the following exist:
                            //      0xFFFF0000 = show window list;
                            //      0xFFFF0001 = show Desktop's context menu.
                            // Otherwise (> 0 and < 0xFFFF0000), we have
                            // a "real" object handle, and a regular WPS
                            // object is to be opened.

        BYTE            bMonitorDrives[30];     // array of 1-byte BOOLs; if any of these
                // is "1", the corresponding drive letter
                // will be monitored for media change
                // (index 1 = A, index 2 = B, index 3 = C, ...).
                // Index 0 is unused to match logical drive numbers.

        // More mouse mappings: V0.9.1 (99-12-03)

        BOOL            fChordWinList;
                // show window list on mb1+2 chord
        BOOL            fSysMenuMB2TitleBar;
                // show system menu on mb2 title-bar click

        // Mouse-button-3 scrolling: V0.9.1 (99-12-03)

        BOOL            fMB3Scroll;
                // scroll window contents on MB3Drag
        BOOL            fMB3ScrollReverse;
                // reverse scrolling
        USHORT          usScrollMode;
                // one of the following:
                //  -- SM_LINEWISE (0): scroll fixed, line-wise
                //  -- SM_AMPLIFIED (1): scroll amplified, relative to window size
        USHORT          usMB3ScrollMin;
                // minimum pixels that mouse must be moved;
                // 0 means 1, 1 means 2, ...
        SHORT           sAmplification;
                // amplification (-9 thru +10)
                // the amplification in percent is calculated like this:
                //      percent = 100 + (sAmplification * 10)
                // so we get:
                //      0       -->  100%
                //      2       -->  120%
                //     10       -->  200%
                //     -2       -->  80%
                //     -9       -->  10%

        // Auto-hide mouse pointer: V0.9.1 (99-12-03)
        BOOL            fAutoHideMouse;
        ULONG           ulAutoHideDelay;
                // delay in seconds; 0 means 1 second, 2 means 3, ...

        // Global object hotkeys:
        BOOL            fGlobalHotkeys;

    } HOOKCONFIG, *PHOOKCONFIG;

    /*
     *@@ DAEMONSHARED:
     *      block of shared memory which is used for
     *      communication between the XWorkplace daemon
     *      and XFLDR.DLL (kernel.c).
     *
     *      This is allocated upon initial WPS startup
     *      by krnInitializeXWorkplace and then requested
     *      by the daemon. See xwpdaemn.c for details.
     *
     *      Since the daemon keeps this block requested,
     *      it can be re-accessed by XFLDR.DLL upon
     *      WPS restarts and be used for storing data
     *      in between WPS session restarts.
     *
     *      The data in this structure is not stored
     *      anywhere. This structure must only be modified
     *      when either the daemon or the WPS is started.
     *      For hook configuration, HOOKCONFIG is used
     *      instead.
     */

    typedef struct _DAEMONSHARED
    {
        HWND        hwndDaemonObject;
                // daemon object window (fnwpDaemonObject, hook\xwphook.c);
                // this is set by the daemon after it has created the object window,
                // so if this is != NULLHANDLE, the daemon is running
        HWND        hwndThread1Object;
                // XFLDR.DLL thread-1 object window (krn_fnwpThread1Object, shared\kernel.c);
                // this is set by krnInitializeXWorkplace before starting the daemon
                // and after the WPS re-initializes
        BOOL        fHookInstalled;
                // TRUE if hook is currently installed;
                // dynamically changed by the daemon upon XDM_HOOKINSTALL
        ULONG       ulWPSStartupCount;
                // WPS startup count maintained by krnInitializeXWorkplace:
                // 1 at first WPS startup, 2 at next, ...
        BOOL        fProcessStartupFolder;
                // TRUE if startup folder should be processed;
                // set by krnInitializeXWorkplace and XShutdown (upon WPS restart)
    } DAEMONSHARED, *PDAEMONSHARED;

    /*
     *@@ HOOKDATA:
     *      global hook data structure. Only one instance
     *      of this is in the shared data segment of
     *      XWPHOOK.DLL, and a pointer to that structure
     *      is returned to the daemon which initially loads
     *      that DLL by hookInit and then stored by the
     *      daemon. As a result, this structure is shared
     *      between the hook and the daemon, and both can
     *      access it at any time.
     *
     *      This is statically initialized to 0 when the hook
     *      DLL is loaded. hookInit will then set up most
     *      fields in here.
     *
     *      This contains setup data (the state of the
     *      hook), some data which needs to be cached,
     *      as well as the HOOKCONFIG structure which
     *      is used to configure the hook and the daemon.
     *      That sub-structure gets (re)loaded from OS2.INI
     *      upon daemon startup and when XDM_HOOKCONFIG is
     *      received by fnwpDaemonObject.
     */

    typedef struct _HOOKDATA
    {
        BOOL        fInputHooked,       // input hook installed?
                    fPreAccelHooked;    // pre-accelerator table hook installed?

        HWND        hwndDaemonObject;   // wnd used for notification of events
                                        // as passed to hookInit; this is the
                                        // object window of the daemon
                                        // (fnwpDaemonObject)
        HAB         habDaemonObject;    // anchor block of hwndNotify

        HMODULE     hmodDLL;            // XWPHOOK.DLL module handle

        HWND        hwndPMDesktop,
                // desktop window handle (WinQueryDesktopWindow)
                    hwndWPSDesktop,
                // WPS desktop frame window (this only gets set by the daemon later!!)
                    hwndWindowList;
                // window list handle

        // screen dimensions
        ULONG       ulCXScreen,
                    ulCYScreen;

        // damon/hook configuration data shared by daemon and the hook;
        // this gets loaded from OS2.INI
        HOOKCONFIG  HookConfig;

        HWND        hwndActivatedByUs;
                // this gets set to a window which was activated by the
                // daemon with sliding focus

        // MB3 scrolling data; added V0.9.1 (99-12-03)
        BOOL        fMB3Scrolling;
                // this is TRUE only while MB3 is down dragging;
                // we will scroll the window contents then

        HWND        hwndScrollBarsOwner;

        HWND        hwndVertScrollBar;
        SHORT       usVertScrollBarID;
                // vertical scroll bar of hwndScrollBarsOwner or NULLHANDLE if none

        HWND        hwndHorzScrollBar;
        SHORT       usHorzScrollBarID;
                // horizontal scroll bar of hwndScrollBarsOwner or NULLHANDLE if none

        // cached data used while MB3 is down; V0.9.1 (99-12-03)
        SHORT       sMB3InitialMouseXPos,
                    sMB3InitialMouseYPos;
        SHORT       sMB3InitialXThumbPos,
                    sMB3InitialYThumbPos;
        SHORT       sCurrentThumbXPos,
                    sCurrentThumbYPos;

        BOOL        fPostVertSBEndScroll,
                    fPostHorzSBEndScroll;
                // these are TRUE if MB3 has been depressed and moved and
                // scroller messages have been posted; this requests
                // a SB_ENDSCROLL upon WM_BUTTON3UP

        // auto-hide mouse pointer; added V0.9.1 (99-12-03)
        ULONG       idAutoHideTimer;
                // if != NULL, auto-hide timer is running
        BOOL        fMousePointerHidden;
                // TRUE if mouse pointer has been hidden
    } HOOKDATA, *PHOOKDATA;

    /*
     *@@ GLOBALHOTKEY:
     *      single XWorkplace object hotkey definition.
     *      Arrays of this are allocated in shared memory and
     *      used by xwphook.c, xwpdaemn.c, and also XFldObject
     *      for hotkey manipulation and detection.
     */

    typedef struct _GLOBALHOTKEY
    {
        USHORT  usFlags;
                        // Keyboard control codes:
                        // SHORT1FROMMP(mp1) of WM_CHAR, filtered.
                        // KC_CTRL, KC_ALT, KC_SHIFT work always,
                        // no matter if we're in a PM or VIO session.
        UCHAR   ucScanCode;
                        // Hardware scan code:
                        // CHAR4FROMMP(mp1) of WM_CHAR.
                        // As opposed to what we do with folder hotkeys,
                        // this must be stored also, because we must use
                        // the scan code for WM_CHAR processing in the hook
                        // to identify hotkeys. We cannot use usKeyCode
                        // because that's different in VIO sessions, while
                        // this one is always the same.
                        // Even if any of Ctrl, Alt, Shift are pressed, this
                        // has the scan code of the additional key which was
                        // pressed.
        USHORT  usKeyCode;
                        // key code:
                        // if KC_VIRTUALKEY is set in usFlags, this has usvk,
                        // otherwise usch from WM_CHAR.
                        // This is only used to be able to display the hotkey
                        // in the hotkey configuration dialogs; we do _not_ use
                        // this to check WM_CHAR messages in the hook, because
                        // this is different between PM and VIO sessions.
        ULONG   ulHandle;
                        // handle to post to thread-1 object window (kernel.c);
                        // this is normally a HOBJECT
    } GLOBALHOTKEY, *PGLOBALHOTKEY;

    /* ******************************************************************
     *                                                                  *
     *   Messages                                                       *
     *                                                                  *
     ********************************************************************/

    #define XDM_HOOKCONFIG          (WM_USER)

    #define XDM_HOOKINSTALL         (WM_USER + 1)

    #define XDM_DESKTOPREADY        (WM_USER + 2)

    #define XDM_HOTKEYPRESSED       (WM_USER + 3)

    #define XDM_HOTKEYSCHANGED      (WM_USER + 4)

    #define XDM_SLIDINGFOCUS        (WM_USER + 5)

    #define XDM_HOTCORNER           (WM_USER + 6)

    /* ******************************************************************
     *                                                                  *
     *   Prototypes                                                     *
     *                                                                  *
     ********************************************************************/

    PHOOKDATA EXPENTRY hookInit(HWND hwndDaemonObject,
                                HMQ hmq);

    BOOL EXPENTRY hookKill(VOID);

    APIRET EXPENTRY hookSetGlobalHotkeys(PGLOBALHOTKEY pNewHotkeys,
                                         ULONG cNewHotkeys);

#endif



