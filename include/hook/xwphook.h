
/*
 * xwphook.h:
 *      header for both xwphook.c and xwpdaemon.c. This is also
 *      included from a number of sources for XFLDR.DLL which need
 *      to interface (configure) the daemon.
 */

/*
 *      Copyright (C) 1999-2002 Ulrich M”ller.
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
     *   OS2.INI applications and keys                                  *
     *                                                                  *
     ********************************************************************/

    #define INIAPP_XWPHOOK              "XWorkplace:Hook"   // added V0.9.0
    #define INIKEY_HOOK_HOTKEYS         "Hotkeys"           // added V0.9.0
    #define INIKEY_HOOK_CONFIG          "Config"            // added V0.9.0
    #define INIKEY_HOOK_PGMGCONFIG      "XPagerConfig"    // V0.9.2 (2000-02-25) [umoeller]
    #define INIKEY_HOOK_PGMGWINPOS      "XPagerWinPos"    // V0.9.4 (2000-08-08) [umoeller]
    #define INIKEY_HOOK_FUNCTIONKEYS    "FuncKeys"          // added V0.9.3 (2000-04-19) [umoeller]

    /* ******************************************************************
     *                                                                  *
     *   Structures                                                     *
     *                                                                  *
     ********************************************************************/

    // do not change the following, or this will break
    // binary compatibility of the XPager OS2.INI data
    #define MAX_STICKYS         64
    // #define MAX_WINDOWS         256
    #define PGMG_TEXTLEN        30

#ifndef __NOPAGER__

    /*
     *@@ PAGERCONFIG:
     *      XPager configuration data.
     *      This is stored within the HOOKDATA structure
     *      (statically in the hook DLL) so that both
     *      the daemon and the hook have access to this.
     *
     *@@added V0.9.2 (2000-02-25) [umoeller]
     */

    typedef struct _PAGERCONFIG
    {
        /* Desktops */
        POINTL       ptlMaxDesktops;
                // no. of virtual Desktops (x and y)
        POINTL       ptlStartDesktop;
                // initial desktop at startup (always (1, 2))

        /* Display */
        BOOL         fShowTitlebar;
                // if TRUE, XPager has a titlebar
        BOOL         _fStartMinimized;
                // start minimized?
        BOOL         fPreserveProportions;
                // preserve proportions of XPager win when resizing?

        BOOL         fStayOnTop;
                // stay on top
        BOOL         fFlash;
        ULONG        ulFlashDelay;      // in milliseconds
                // "flash" (temporarily show)

        BOOL         fMirrorWindows;
                // show windows in XPager?

        BOOL         fShowWindowText;
                // show window titles in XPager?

        BOOL         fClick2Activate;
                // allow activate/lower by mouse clicks?

        BOOL         fRecoverOnShutdown;
                // if TRUE, windows are restored when XPager is exited

        /* Sticky */
        CHAR         aszSticky[MAX_STICKYS][PGMG_TEXTLEN];
        SHORT        usStickyTextNum;
        HWND         hwndSticky2[MAX_STICKYS];
        SHORT        usSticky2Num;

        /*  Colors */
        LONG         lcNormal;
        LONG         lcCurrent;
        LONG         lcDivider;

        LONG         lcNormalApp;
        LONG         lcCurrentApp;
        LONG         lcAppBorder;

        LONG         lcTxtNormalApp;
        LONG         lcTxtCurrentApp;

        /* Panning */
        BOOL         bPanAtTop;             // @@@ Martin, we can't comment this
        BOOL         bPanAtBottom;          // out, because this is stored in binary
        BOOL         bPanAtLeft;            // in OS2.INI.
        BOOL         bPanAtRight;
        BOOL         bWrapAround;

        /* Keyboard */
        BOOL         fEnableArrowHotkeys;
        ULONG        ulKeyShift;        // KC_* values
        /* BOOL         bReturnKeystrokes;
        BOOL         bHotkeyGrabFocus; */
    } PAGERCONFIG, *PPAGERCONFIG;

#endif

    // flags for HOOKCONFIG.usScrollMode
    #define SM_LINEWISE         0
    #define SM_AMPLIFIED        1

    // flags for HOOKCONFIG.ulAutoHideFlags
    #define AHF_IGNOREMENUS     0x00000001L
    #define AHF_IGNOREBUTTONS   0x00000002L

    // flags for HOOKCONFIG.ulAutoMoveFlags
    #define AMF_ALWAYSMOVE      0x00000001L
    #define AMF_IGNORENOBUTTON  0x00000002L
    #define AMF_ANIMATE         0x00000004L

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
     *      If settings are added to this structure, they
     *      must be added to the bottom in order not to
     *      break binary compatibility between XWP versions.
     *
     *      Note that the object hotkey _definitions_ are
     *      not part of this structure. Those are set using
     *      XDM_HOTKEYSCHANGED instead. However, object
     *      hotkeys are globally enabled in here (fGlobalHotkeys).
     */

    typedef struct _HOOKCONFIG
    {
        // Sliding focus:

        BOOL            __fSlidingFocus;
                // enabled?
        ULONG           __ulSlidingFocusDelay;
                // delay in ms; 0 = off

        BOOL            __fSlidingBring2Top;
                // bring windows to top or preserve Z-order
        BOOL            __fSlidingIgnoreDesktop;
                // ignore Desktop windows
        BOOL            __fSlidingIgnoreSeamless;
                // TRUE: ignore seamless Win-OS/2 windows
                // FALSE: always bring them to top

        // Screen corner objects:
        HOBJECT         ahobjDummy[4];      // was four screen corner objects;
                                            // we extended the array to 8 items
                                            // so the array had to be moved to the
                                            // bottom in order not to break binary
                                            // compatibility

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
        BOOL            __fAutoHideMouse;
        ULONG           __ulAutoHideDelay;
                // delay in seconds; 0 means 1 second, 2 means 3, ...

        // Global object hotkeys enabled:
        // this can be disabled even if any hotkeys are defined
        // because the hotkeys themselves are stored separately
        // in shared memory
        BOOL            __fGlobalHotkeys;

        // XPager configuration
        BOOL            fRemoved1, // _fXPagerStayOnTop,
                        __fSlidingIgnoreXPager;
                                // on sliding focus

        // Sliding menus
        BOOL            fSlidingMenus;
                // enabled?
        ULONG           ulSubmenuDelay;
                // delay in ms; 0 = off
        BOOL            fMenuImmediateHilite;

        // Mouse-button-3 single-clicks to MB1 double-clicks
        // V0.9.4 (2000-06-12) [umoeller]
        BOOL            fMB3Click2MB1DblClk;

        // Screen corner objects:
        // moved the array down here (there's a dummy above)
        // V0.9.4 (2000-06-12) [umoeller]
        HOBJECT         ahobjHotCornerObjects[8];
                            // Indices:
                            //      0 = lower left corner,
                            //      1 = top left corner,
                            //      2 = lower right corner,
                            //      3 = top right corner;
                            //   borders added V0.9.4 (2000-06-12) [umoeller]:
                            //      4 = top border,
                            //      5 = left border,
                            //      6 = right border,
                            //      7 = bottom border.
                  // V0.9.18 (2002-02-12) [pr]
                  #define SCREENCORNER_MIN            0
                  #define SCREENCORNER_BOTTOMLEFT     0
                  #define SCREENCORNER_TOPLEFT        1
                  #define SCREENCORNER_BOTTOMRIGHT    2
                  #define SCREENCORNER_TOPRIGHT       3
                  #define SCREENCORNER_TOP            4
                  #define SCREENCORNER_LEFT           5
                  #define SCREENCORNER_RIGHT          6
                  #define SCREENCORNER_BOTTOM         7
                  #define SCREENCORNER_MAX            7
                            // If any item is NULLHANDLE, it means the
                            // corner is inactive (no function defined).
                            // If the hiword of the item is 0xFFFF, this
                            // means a special function has been defined.
                            // Currently the following exist:
                            //      0xFFFF0000 = show window list;
                            //      0xFFFF0001 = show Desktop's context menu.
                            //      0xFFFF0002 = show XPager.
                            //      (2001-01-26) [lafaix]
                            //      0xFFFF0003 = up one screen
                            //      0xFFFF0004 = right one screen
                            //      0xFFFF0005 = down one screen
                            //      0xFFFF0006 = left one screen
                            //      [lafaix] end
                            //      0xFFFF0007 = screen wrap [pr]
                            // Otherwise (> 0 and < 0xFFFF0000), we have
                            // a "real" object handle, and a regular WPS
                            // object is to be opened.

        // special treatment for conditional-cascade submenus when
        // using sliding menus (V0.9.6 (2000-10-27) [umoeller])
        BOOL            fConditionalCascadeSensitive;

        // more XPager configuration V0.9.7 (2000-12-08) [umoeller]
        BOOL            __fSlidingIgnoreXCenter;
                            // on sliding focus

        // screen corner objects sensitivity; in percent of the
        // adjacents borders.  0 = off, 50 = borders objects disabled
        // V0.9.9 (2001-03-15) [lafaix]
        ULONG           ulCornerSensitivity;

        // Mouse-button-3 autoscroll and push to bottom features
        BOOL            fMB3AutoScroll;
        BOOL            fMB3Push2Bottom;

        // Auto hide and automatic pointer movement options
        // V0.9.14 (2001-08-02) [lafaix]
        ULONG           __ulAutoHideFlags;
        BOOL            __fAutoMoveMouse;
        ULONG           __ulAutoMoveFlags;
        ULONG           __ulAutoMoveDelay;            // V0.9.14 (2001-08-21) [umoeller]
    } HOOKCONFIG, *PHOOKCONFIG;

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
                        // Only the following flags will be set:
                        // --   KC_CTRL
                        // --   KC_ALT
                        // --   KC_SHIFT
                        // --   KC_VIRTUALKEY
                        // --   KC_INVALIDCOMP: special flag used if the
                        //      scan code represents one of the user-defined
                        //      function keys in the XWPKeyboard object.
                        // KC_CTRL, KC_ALT, KC_SHIFT work always,
                        // no matter if we're in a PM or VIO session.
                        // However, for some reason, KC_VIRTUALKEY is
                        // never set in VIO sessions. We still store it
                        // in this structure though to be able to display
                        // the hotkey in the configuration pages.
                        // The hook will filter that out since the scan
                        // code is good enough to identify the key.
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
                        // this is normally the HOBJECT of the object to be
                        // opened.
    } GLOBALHOTKEY, *PGLOBALHOTKEY;

    /*
     *@@ FUNCTIONKEY:
     *      XWorkplace function key description.
     *      An array of these is returned by
     *      hifQueryFunctionKeys().
     *
     *@@added V0.9.3 (2000-04-19) [umoeller]
     */

    typedef struct _FUNCTIONKEY
    {
        UCHAR       ucScanCode;         // hardware scan code;
                                        // CHAR4FROMMP(mp1) of WM_CHAR
        CHAR        szDescription[30];  // key description (e.g. "Win left")
        BOOL        fModifier;          // TRUE if the scan code represents
                                        // a modifier key which can be pressed
                                        // together with another key, similar
                                        // to Ctrl or Alt or Del; this will
                                        // allow us to do things like "WinLeft + C"
    } FUNCTIONKEY, *PFUNCTIONKEY;

    /* ******************************************************************
     *                                                                  *
     *   Messages                                                       *
     *                                                                  *
     ********************************************************************/

    #define XDM_HOOKINSTALL         (WM_USER + 400)

    #define XDM_DESKTOPREADY        (WM_USER + 401)

    #define XDM_HOOKCONFIG          (WM_USER + 402)

#ifndef __NOPAGER__
    #define XDM_STARTSTOPPAGER   (WM_USER + 403)

    #define XDM_PAGERCONFIG      (WM_USER + 404)
        // flags for XDM_PAGERCONFIG:
        #define PGMGCFG_REPAINT     0x0001  // causes PGMG_INVALIDATECLIENT
        #define PGMGCFG_REFORMAT    0x0002  // causes pgmcSetPgmgFramePos
        #define PGMGCFG_ZAPPO       0x0004  // reformats the XPager title bar
        #define PGMGCFG_STICKIES    0x0008  // sticky windows have changed, rescan winlist
#endif

    #define XDM_HOTKEYPRESSED       (WM_USER + 405)

    #define XDM_HOTKEYSCHANGED      (WM_USER + 406)

    #define XDM_FUNCTIONKEYSCHANGED (WM_USER + 407)

#ifndef __NOSLIDINGFOCUS__
    #define XDM_SLIDINGFOCUS        (WM_USER + 408)
#endif

    #define XDM_SLIDINGMENU         (WM_USER + 409)

    #define XDM_HOTCORNER           (WM_USER + 410)

    #define XDM_WMCHORDWINLIST      (WM_USER + 411)

    // #define XDM_PGMGWINLISTFULL     (WM_USER + 412)

    // added V0.9.9 (2001-03-18) [lafaix]
    #define XDM_BEGINSCROLL         (WM_USER + 413)
    #define XDM_SETPOINTER          (WM_USER + 414)
    #define XDM_ENDSCROLL           (WM_USER + 415)

    // added V0.9.12 (2001-05-12) [umoeller]
    #define XDM_RECOVERWINDOWS      (WM_USER + 416)

    #define XDM_ADDDISKWATCH        (WM_USER + 417)

    /*
     *@@ ADDDISKWATCH:
     *      struct used with XDM_ADDDISKWATCH.
     *
     *@@added V0.9.14 (2001-08-01) [umoeller]
     */

    typedef struct _ADDDISKWATCH
    {
        ULONG           ulLogicalDrive;         // disk to be monitored
        HWND            hwndNotify;             // window to be notified on change
        ULONG           ulMessage;              // message to be posted to window
    } ADDDISKWATCH, *PADDDISKWATCH;

    #define XDM_REMOVEDISKWATCH     (WM_USER + 418)

    #define XDM_QUERYDISKS          (WM_USER + 419)

    #define XDM_ADDCLICKWATCH       (WM_USER + 420)

    #define XDM_MOUSECLICKED        (WM_USER + 421)

/*
   #define WM_BUTTON1UP               0x0072
   #define WM_BUTTON1DBLCLK           0x0073
   #define WM_BUTTON2DOWN             0x0074
   #define WM_BUTTON2UP               0x0075
   #define WM_BUTTON2DBLCLK           0x0076
      #define WM_BUTTON3DOWN          0x0077
      #define WM_BUTTON3UP            0x0078
      #define WM_BUTTON3DBLCLK        0x0079
*/

#ifndef __NOMOVEMENT2FEATURES__
    #define XDM_MOVEPTRTOBUTTON     (WM_USER + 422)
#endif

    #define XDM_DISABLEHOTKEYSTEMP  (WM_USER + 423)

#endif



