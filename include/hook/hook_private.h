
/*
 * hook_private.h:
 *      private declarations for the hook and the daemon.
 *      These are not seen by XFLDR.DLL.
 *
 *@@include #define INCL_WINMESSAGEMGR
 *@@include #include <os2.h>
 *@@include #include xwphook.h
 */

/*
 *      Copyright (C) 1999-2000 Ulrich M”ller.
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, in version 2 as it comes in the COPYING
 *      file of the XWorkplace main distribution.
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 */

#ifndef HOOK_PRIVATE_HEADER_INCLUDED
    #define HOOK_PRIVATE_HEADER_INCLUDED

    /* ******************************************************************
     *                                                                  *
     *   Shared mem/semaphore IDs                                       *
     *                                                                  *
     ********************************************************************/

    #define SHMEM_HOTKEYS         "\\SHAREMEM\\XWORKPLC\\HOTKEYS.DAT"
    #define SHMEM_FUNCTIONKEYS    "\\SHAREMEM\\XWORKPLC\\FUNCKEYS.DAT"
                // added V0.9.3 (2000-04-20) [umoeller]

    #define IDMUTEX_ONEINSTANCE     "\\SEM32\\XWORKPLC\\ONEINST.MTX"
    #define TIMEOUT_HMTX_HOTKEYS    6000
    // #define SEM_TIMEOUT             4000

    // #define IDMUTEX_PGMG_WINLIST    "\\SEM32\\XWORKPLC\\PGMGWINS.MTX"
    // #define TIMEOUT_PGMG_WINLIST    6000

    // timer IDs for fnwpDaemonObject
    #define TIMERID_SLIDINGFOCUS        1
    #define TIMERID_SLIDINGMENU         2
    #define TIMERID_MONITORDRIVE        3
    #define TIMERID_AUTOHIDEMOUSE       4
    #define TIMERID_AUTOSCROLL          5
    #define TIMERID_MOVINGPTR           6

    /* ******************************************************************
     *                                                                  *
     *   Structures                                                     *
     *                                                                  *
     ********************************************************************/

    /*
     *@@ SCROLLDATA:
     *      substructure for storing scroll data
     *      for MB3-scroll processing. Two instances
     *      of this exist in HOOKDATA, one for the
     *      vertical, one for the horizontal scroll bar
     *      of the window which was under the mouse when
     *      MB3 was pressed down.
     *
     *@@added V0.9.2 (2000-02-25) [umoeller]
     */

    typedef struct _SCROLLDATA
    {
        HWND        hwndScrollBar;
                // actual scroll bar window or NULLHANDLE if none
        HWND        hwndScrollLastOwner;
                // WinQueryWindow(hwndScrollBar, QW_OWNER);
                // this gets recalculated with every mouse move
                // V0.9.3 (2000-04-30) [umoeller]

        // cached data used while MB3 is down; V0.9.1 (99-12-03)
        // LONG        lScrollBarSize;
                // size (height or width) of the scroll bar (in window coordinates);
                // calculated when MB3 is depressed; the size of the buttons
                // has already been subtracted
                // V0.9.2 (2000-03-23) [umoeller]: removed this from SCROLLDATA
                // because this might change while the user holds down MB3, so this
                // needs to be recalculated with every WM_MOUSEMOVE
        SHORT       sMB3InitialScreenMousePos,
                // mouse position when MB3 was depressed (in screen coordinates)
                // V0.9.3 (2000-04-30) [umoeller]: changed to screen
                    sMB3InitialThumbPosUnits,
                // scroll bar thumb position when MB3 was depressed (in scroll bar units)
                    sCurrentThumbPosUnits;
                // current scroll bar thumb position (in scroll bar units); this gets updated
                // when the mouse is moved while MB3 is depressed

        BOOL        fPostSBEndScroll;
                // this gets set to TRUE only if MB3 has been depressed and
                // the mouse has been moved and scroller messages have been
                // posted already;
                // this enforces a SB_ENDSCROLL when WM_BUTTON3UP comes
                // in later
    } SCROLLDATA, *PSCROLLDATA;

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
        // damon/hook configuration data shared by daemon and the hook;
        // this gets loaded from OS2.INI
        HOOKCONFIG  HookConfig;

        // PageMage configuration data shared by daemon and the hook;
        // this gets loaded from OS2.INI also
        PAGEMAGECONFIG PageMageConfig;

        BOOL        fSendMsgHooked,     // send-message hook installed?
                    fLockupHooked,      // lockup hook installed?
                    fInputHooked,       // input hook installed?
                    fPreAccelHooked;    // pre-accelerator table hook installed?

        HWND        hwndDaemonObject;
                // wnd used for notification of events;
                // this is the daemon object window
                // (fnwpDaemonObject) as passed to hookInit
                // and is the same as XWPGLOBALSHARED.hwndDaemonObject.
                // This receives lots of messages from the hook for
                // further processing.

        HAB         habDaemonObject;
                // anchor block of hwndDaemonObject; cached for speed

        HWND        hwndPageMageClient;
                // PageMage client window, created by pgmcCreateMainControlWnd
        HWND        hwndPageMageFrame;
                // PageMage frame window, created by pgmcCreateMainControlWnd
        HWND        hwndPageMageMoveThread;
                // PageMage move thread (fnwpMoveThread)

        HMODULE     hmodDLL;
                // XWPHOOK.DLL module handle

        HWND        hwndPMDesktop,
                // desktop window handle (WinQueryDesktopWindow)
                    hwndWPSDesktop,
                // WPS desktop frame window (this only gets set by the daemon later!!)
                    hwndWindowList;
                // window list handle
        ULONG       pidPM;
                // process ID of first PMSHELL.EXE V0.9.7 (2001-01-21) [umoeller]

        HWND        hwndLockupFrame;
                // current lockup window, if any

        // screen dimensions
        // (we use LONG's because otherwise we'd have to typecast
        // when calculating coordinates which might be off-screen;
        // V0.9.2 (2000-02-23) [umoeller])
        LONG        lCXScreen,
                    lCYScreen;

        HWND        hwndActivatedByUs;
                // this gets set to a window which was activated by the
                // daemon with sliding focus

        // MB3 scrolling data; added V0.9.1 (99-12-03)
        HWND        hwndCurrentlyScrolling;
                // this is != NULLHANDLE if MB3 has been depressed
                // over a window with scroll bars and reset to
                // NULLHANDLE once MB3 is released. This allows us
                // to simulate capturing the mouse without actually
                // capturing it, which isn't such a good idea in
                // a hook, apparently (changed V0.9.3 (2000-04-30) [umoeller])

        SCROLLDATA  SDXHorz,
                    SDYVert;

        BOOL        bAutoScroll;
                // this is TRUE if auto scrolling has been requested.
                // Set by the hook, and used by XDM_BEGINSCROLL
                // V0.9.9 (2001-03-20) [lafaix]

        // auto-hide mouse pointer; added V0.9.1 (99-12-03)
        ULONG       idAutoHideTimer;
                // if != NULL, auto-hide timer is running
        BOOL        fMousePointerHidden;
                // TRUE if mouse pointer has been hidden

        // PageMage
        BOOL        fDisablePgmgSwitching;
        BOOL        fDisableMouseSwitch;
                // TRUE if processing a mouse switch request.  Focus
                // changes are disable during this.
                /// V0.9.9 (2001-03-14) [lafaix]

        // sliding menus
        HWND        hwndMenuUnderMouse;
        SHORT       sMenuItemIndexUnderMouse;
                // V0.9.12 (2001-05-24) [lafaix]: changed meaning (from identity to index)
        MPARAM      mpDelayedSlidingMenuMp1;

        // auto-hide mouse pointer state backup; added V0.9.9 (2001-03-21) [lafaix]
        BOOL        fOldAutoHideMouse;

        // click watches V0.9.14 (2001-08-21) [umoeller]
        BOOL        fClickWatches;

    } HOOKDATA, *PHOOKDATA;

    // special key for WM_MOUSEMOVE with delayed sliding menus
    #define         HT_DELAYEDSLIDINGMENU   (HT_NORMAL+2)

    /* ******************************************************************
     *                                                                  *
     *   PageMage definitions needed by the hook                        *
     *                                                                  *
     ********************************************************************/

    #define PGMG_INVALIDATECLIENT   (WM_USER + 300)
    #define PGMG_ZAPPO              (WM_USER + 301)
    // #define PGMG_LOCKUP             (WM_USER + 302)
                // removed, this did nothing V0.9.7 (2001-01-18) [umoeller]
    #define PGMG_WNDCHANGE          (WM_USER + 303)
    #define PGMG_WNDRESCAN          (WM_USER + 304)
    #define PGMG_CHANGEACTIVE       (WM_USER + 305)
    #define PGMG_LOWERWINDOW        (WM_USER + 306)
    #define PGMG_WINLISTFULL        (WM_USER + 307)

    #define PGOM_CLICK2ACTIVATE     (WM_USER + 321)
    #define PGOM_CLICK2LOWER        (WM_USER + 322)
    #define PGOM_HOOKKEY            (WM_USER + 323)
    #define PGOM_FOCUSCHANGE        (WM_USER + 324)
    #define PGOM_MOUSESWITCH        (WM_USER + 325)

    /* ******************************************************************
     *
     *   Hook DLL prototypes
     *
     ********************************************************************/

    PHOOKDATA EXPENTRY hookInit(HWND hwndDaemonObject);

    BOOL EXPENTRY hookKill(VOID);

    APIRET EXPENTRY hookSetGlobalHotkeys(PGLOBALHOTKEY pNewHotkeys,
                                         ULONG cNewHotkeys,
                                         PFUNCTIONKEY pNewFunctionKeys,
                                         ULONG cNewFunctionKeys);

    /* ******************************************************************
     *
     *   Internal prototypes
     *
     ********************************************************************/

    VOID _Optlink StopMB3Scrolling(BOOL fSuccessPostMsgs);

    VOID _Optlink WMButton_SystemMenuContext(HWND hwnd);

    BOOL _Optlink WMMouseMove_MB3Scroll(HWND hwnd);

    BOOL _Optlink WMMouseMove(PQMSG pqmsg,
                              PBOOL pfRestartAutoHide);

    VOID _Optlink WMMouseMove_AutoHideMouse(VOID);

    BOOL _Optlink WMChar_Main(PQMSG pqmsg);

    extern HOOKDATA        G_HookData;
    extern PGLOBALHOTKEY   G_paGlobalHotkeys;
    extern ULONG           G_cGlobalHotkeys;
    extern PFUNCTIONKEY    G_paFunctionKeys;
    extern ULONG           G_cFunctionKeys;
    extern HMTX            G_hmtxGlobalHotkeys;

    extern HWND    G_hwndUnderMouse;
    extern HWND    G_hwndLastFrameUnderMouse;
    extern HWND    G_hwndLastSubframeUnderMouse;
    extern POINTS  G_ptsMousePosWin;
    extern POINTL  G_ptlMousePosDesktop;
    extern HWND    G_hwndRootMenu;
#endif



