
/*
 * hook_private.h:
 *      private declarations for the hook and the daemon.
 *      These are not seen by XFLDR.DLL.
 *
 *@@include #define INCL_WINMESSAGEMGR
 *@@include #include <os2.h>
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

    #define IDSHMEM_HOTKEYS         "\\SHAREMEM\\XWORKPLC\\HOTKEYS.DAT"

    #define IDMUTEX_ONEINSTANCE     "\\SEM32\\XWORKPLC\\ONEINST.SEM"
    #define SEM_TIMEOUT             4000

    #define PAGEMAGE_MUTEX          "\\SEM32\\PAGEMAGE"
    #define PAGEMAGE_WNDLSTMTX      "\\SEM32\\PGMG_WNDLSTMTX"
    #define PAGEMAGE_WNDLSTEV       "\\SEM32\\PGMG_WNDLSTEV"
    #define PAGEMAGE_WNDQUEUE       "\\QUEUES\\PAGEMAGE"

    // timer IDs for fnwpDaemonObject
    #define TIMERID_SLIDINGFOCUS        1
    #define TIMERID_SLIDINGMENU         2
    #define TIMERID_MONITORDRIVE        3
    #define TIMERID_AUTOHIDEMOUSE       4

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
                // actual scroll bar window
        HWND        hwndScrollOwner;
                // WinQueryWindow(hwndScrollBar, QW_OWNER)
        SHORT       usScrollBarID;
                // scroll bar of hwndScrollBarsOwner or NULLHANDLE if none

        // cached data used while MB3 is down; V0.9.1 (99-12-03)
        // LONG        lScrollBarSize;
                // size (height or width) of the scroll bar (in window coordinates);
                // calculated when MB3 is depressed; the size of the buttons
                // has already been subtracted
                // V0.9.2 (2000-03-23) [umoeller]: removed this from SCROLLDATA
                // because this might change while the user holds down MB3, so this
                // needs to be recalculated with every WM_MOUSEMOVE
        SHORT       sMB3InitialMousePos,
                // mouse position when MB3 was depressed (in window coordinates)
                    sMB3InitialThumbPos,
                // scroll bar thumb position when MB3 was depressed (in scroll bar units)
                    sCurrentThumbPos;
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
        BOOL        fSendMsgHooked,     // send-message hook installed?
                    fLockupHooked,      // lockup hook installed?
                    fInputHooked,       // input hook installed?
                    fPreAccelHooked;    // pre-accelerator table hook installed?

        HWND        hwndDaemonObject;
                // wnd used for notification of events;
                // this is the daemon object window
                // (fnwpDaemonObject) as passed to hookInit
                // and is the same as DAEMONSHARED.hwndDaemonObject.
                // This receives lots of messages from the hook for
                // further processing.

        HAB         habDaemonObject;
                // anchor block of hwndDaemonObject; cached for speed

        HWND        hwndPageMageClient;
                // PageMage client window, created by pgmcCreateMainControlWnd
        HWND        hwndPageMageFrame;
                // PageMage frame window, created by pgmcCreateMainControlWnd

        HMODULE     hmodDLL;
                // XWPHOOK.DLL module handle

        HWND        hwndPMDesktop,
                // desktop window handle (WinQueryDesktopWindow)
                    hwndWPSDesktop,
                // WPS desktop frame window (this only gets set by the daemon later!!)
                    hwndWindowList;
                // window list handle

        HWND        hwndLockupFrame;
                // current lockup window, if any

        // screen dimensions
        // (we use LONG's because otherwise we'd have to typecast
        // when calculating coordinates which might be off-screen;
        // V0.9.2 (2000-02-23) [umoeller])
        LONG        lCXScreen,
                    lCYScreen;

        // damon/hook configuration data shared by daemon and the hook;
        // this gets loaded from OS2.INI
        HOOKCONFIG  HookConfig;

        HWND        hwndActivatedByUs;
                // this gets set to a window which was activated by the
                // daemon with sliding focus

        // MB3 scrolling data; added V0.9.1 (99-12-03)
        BOOL        fCurrentlyMB3Scrolling;
                // this is TRUE only while MB3 is down dragging;
                // we will scroll the window contents then

        SCROLLDATA  SDXHorz,
                    SDYVert;

        // auto-hide mouse pointer; added V0.9.1 (99-12-03)
        ULONG       idAutoHideTimer;
                // if != NULL, auto-hide timer is running
        BOOL        fMousePointerHidden;
                // TRUE if mouse pointer has been hidden

        // PageMage
        BOOL        fDisableSwitching;

        // sliding menus
        HWND        hwndMenuUnderMouse;
        SHORT       sMenuItemUnderMouse;
        MPARAM      mpDelayedSlidingMenuMp1;
    } HOOKDATA, *PHOOKDATA;

    // special key for WM_MOUSEMOVE with delayed sliding menus
    #define         HT_DELAYEDSLIDINGMENU   (HT_NORMAL+2)

    /* ******************************************************************
     *                                                                  *
     *   PageMage definitions needed by the hook                        *
     *                                                                  *
     ********************************************************************/

    #define PGMG_INVALIDATECLIENT   (WM_USER + 100)
    #define PGMG_ZAPPO              (WM_USER + 101)
    #define PGMG_LOCKUP             (WM_USER + 102)
    #define PGMG_WNDCHANGE          (WM_USER + 103)
    #define PGMG_WNDRESCAN          (WM_USER + 104)
    #define PGMG_CHANGEACTIVE       (WM_USER + 105)
    #define PGMG_LOWERWINDOW        (WM_USER + 106)
    #define PGMG_PASSTHRU           (WM_USER + 107)

    #define PGMGQ_QUIT              0
    #define PGMGQ_MOUSEMOVE         1
    #define PGMGQ_CLICK2ACTIVATE    2
    #define PGMGQ_CLICK2LOWER       3
    #define PGMGQ_KEY               4
    #define PGMGQ_HOOKKEY           5
    #define PGMGQ_FOCUSCHANGE       6

    #define PGMGQENCODE(msg, parm1, parm2) ((msg << 28) | (parm1 << 14) | (parm2))
    #define MSGFROMPGMGQ(qmsg) (qmsg >> 28)
    #define PARM1FROMPGMGQ(qmsg) ((qmsg & 0x0FFFFFFF) >> 14)
    #define PARM2FROMPGMGQ(qmsg) (qmsg & 0x00003FFF)

    /* ******************************************************************
     *                                                                  *
     *   Hook DLL prototypes                                            *
     *                                                                  *
     ********************************************************************/

    PHOOKDATA EXPENTRY hookInit(HWND hwndDaemonObject);

    BOOL EXPENTRY hookKill(VOID);

    APIRET EXPENTRY hookSetGlobalHotkeys(PGLOBALHOTKEY pNewHotkeys,
                                         ULONG cNewHotkeys);


#endif



