
/*
 * xwphook.h:
 *      header for both xwphook.c and xwpdaemon.c. This is also
 *      included from a number of sources for XFLDR.DLL which need
 *      to interface (configure) the daemon.
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

#ifndef XWPHOOK_HEADER_INCLUDED
    #define XWPHOOK_HEADER_INCLUDED

    /* ******************************************************************
     *                                                                  *
     *   Shared mem/semaphore IDs                                       *
     *                                                                  *
     ********************************************************************/

    #define IDSHMEM_DAEMON          "\\SHAREMEM\\XWORKPLC\\DAEMON.DAT"
            // DAEMONSHARED structure

    /* ******************************************************************
     *                                                                  *
     *   OS2.INI applications and keys                                  *
     *                                                                  *
     ********************************************************************/

    #define INIAPP_XWPHOOK          "XWorkplace:Hook"   // added V0.9.0
    #define INIKEY_HOOK_HOTKEYS     "Hotkeys"           // added V0.9.0
    #define INIKEY_HOOK_CONFIG      "Config"            // added V0.9.0
    #define INIKEY_HOOK_PGMGCONFIG  "PageMageConfig"    // V0.9.2 (2000-02-25) [umoeller]

    /* ******************************************************************
     *                                                                  *
     *   Structures                                                     *
     *                                                                  *
     ********************************************************************/

    #define MAX_STICKYS         64
    #define MAX_WINDOWS         256
    #define TEXTLEN             30

    // flags for HOOKCONFIG.usScrollMode
    #define SM_LINEWISE         0
    #define SM_AMPLIFIED        1

    /*
     *@@ PAGEMAGECONFIG:
     *      PageMage configuration data.
     *      This is stored in a global variable
     *      in xwpdaemn.c and only visible to
     *      the daemon (and thus to the PageMage
     *      code), but not to the hook.
     *
     *@@added V0.9.2 (2000-02-25) [umoeller]
     */

    typedef struct _PAGEMAGECONFIG
    {
        /* Misc 1 */
        POINTL       ptlMaxDesktops;
        POINTL       ptlStartDesktop;
        INT          iEdgeBoundary;
        ULONG        ulSleepTime;
        BOOL         bRepositionMouse;
        BOOL         bClickActivate;

        /* Misc 2 */
        BOOL         _bHoldWPS;          // make WPS Desktop sticky
        // CHAR         szWPSName[TEXTLEN];
        LONG         lPriority;
        LONG         lPriorityZero;
        BOOL         bRecoverOnShutdown;
        BOOL         bSwitchToFocus;

        /* Display */
        BOOL         bShowTitlebar;
        BOOL         bStartMin;
        BOOL         bFlash;
        ULONG        ulFlashDelay;
        BOOL         bFloatToTop;
        BOOL         bShowWindows;
        BOOL         bShowWindowText;
        BOOL         fPreserveProportions;

        /* Sticky */
        CHAR         aszSticky[MAX_STICKYS][TEXTLEN];
        SHORT        usStickyTextNum;
        HWND         hwndSticky2[MAX_STICKYS];
        SHORT        usSticky2Num;

        /*  Colors 1 */
        LONG         lcNormal;
        LONG         lcCurrent;
        LONG         lcDivider;
        CHAR         szNormal[20];
        CHAR         szCurrent[20];
        CHAR         szDivider[20];

        /* Colors 2 */
        LONG         lcNormalApp;
        LONG         lcCurrentApp;
        LONG         lcAppBorder;
        CHAR         szNormalApp[20];
        CHAR         szCurrentApp[20];
        CHAR         szAppBorder[20];

        /* Panning */
        BOOL         bPanAtTop;
        BOOL         bPanAtBottom;
        BOOL         bPanAtLeft;
        BOOL         bPanAtRight;
        BOOL         bWrapAround;

        /* Keyboard */
        ULONG        ulKeyShift;
        BOOL         bReturnKeystrokes;
        BOOL         bHotkeyGrabFocus;
    } PAGEMAGECONFIG, *PPAGEMAGECONFIG;

    /*
     *@@ HOOKCONFIG:
     *      configuration data for the hook and the daemon.
     *
     *      This is stored within the HOOKDATA structure
     *      (statically in the hook DLL) so that both
     *      the daemon and the hook have access to this.
     *
     *      A mirror of this structure is put into OS2.INI
     *      which gets loaded by the XWorkplace settings
     *      objects in XFLDR.DLL to configure the hook.
     *      This gets (re)loaded by the daemon when XFLDR.DLL
     *      posts XDM_HOOKCONFIG to fnwpDaemonObject.
     *
     *      So this is seen by the hook and the daemon;
     *      XFLDR.DLL only writes this back to OS2.INI and
     *      notifies the daemon to reload this.
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

        BOOL            fSlidingBring2Top;
                // bring windows to top or preserve Z-order
        BOOL            fSlidingIgnoreDesktop;
                // ignore Desktop windows
        BOOL            fSlidingIgnoreSeamless;
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

        // Global object hotkeys enabled:
        // this can be disabled even if any hotkeys are defined
        // because the hotkeys themselves are stored separately
        // in shared memory
        BOOL            fGlobalHotkeys;

        // PageMage configuration
        BOOL            fFloat,
                        fSlidingIgnorePageMage;        // on sliding focus

        // Sliding menus
        BOOL            fSlidingMenus;
                // enabled?
        ULONG           ulSubmenuDelay;
                // delay in ms; 0 = off
        BOOL            fMenuImmediateHilite;
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
     *      instead, since the hook does NOT see this
     *      structure.
     */

    typedef struct _DAEMONSHARED
    {
        HWND        hwndDaemonObject;
                // daemon object window (fnwpDaemonObject, xwpdaemn.c);
                // this is set by the daemon after it has created the object window,
                // so if this is != NULLHANDLE, the daemon is running
        HWND        hwndThread1Object;
                // XFLDR.DLL thread-1 object window (krn_fnwpThread1Object, shared\kernel.c);
                // this is set by krnInitializeXWorkplace before starting the daemon
                // and after the WPS re-initializes
        BOOL        fAllHooksInstalled;
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

    #define XDM_PAGEMAGECONFIG      (WM_USER + 1)

    #define XDM_HOOKINSTALL         (WM_USER + 2)

    #define XDM_STARTSTOPPAGEMAGE   (WM_USER + 3)

    #define XDM_DESKTOPREADY        (WM_USER + 4)

    #define XDM_HOTKEYPRESSED       (WM_USER + 5)

    #define XDM_HOTKEYSCHANGED      (WM_USER + 6)

    #define XDM_SLIDINGFOCUS        (WM_USER + 7)

    #define XDM_SLIDINGMENU         (WM_USER + 8)

    #define XDM_HOTCORNER           (WM_USER + 9)

#endif



