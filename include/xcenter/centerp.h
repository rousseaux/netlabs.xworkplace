
/*
 *@@sourcefile centerp.h:
 *      private header file for the XCenter.
 *      See src\shared\center.c for an introduction to
 *      the XCenter.
 *
 *@@include #include "helpers\linklist.h"
 *@@include #include "config\center.h"
 *@@include #include "config\centerp.h"
 */

/*
 *      Copyright (C) 2000 Ulrich M”ller.
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

#ifndef CENTERP_HEADER_INCLUDED
    #define CENTERP_HEADER_INCLUDED

    #ifndef CENTER_HEADER_INCLUDED
        #error shared\center.h must be included before config\centerp.h.
    #endif

    // PM window class names for built-in widgets
    #define WNDCLASS_WIDGET_OBJBUTTON   "XWPCenterObjButtonWidget"
    #define WNDCLASS_WIDGET_PULSE       "XWPCenterPulseWidget"

    MRESULT EXPENTRY fnwpObjButtonWidget(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
    MRESULT EXPENTRY fnwpPulseWidget(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
    MRESULT EXPENTRY fnwpXButtonWidget(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);

    // undocumented window style for "topmost"
    #ifndef WS_TOPMOST
        #define WS_TOPMOST  0x00200000L
    #endif

    #define TIMERID_UNFOLDFRAME     1
    #define TIMERID_SHOWWIDGETS     2
    #define TIMERID_AUTOHIDE_START  3
    #define TIMERID_AUTOHIDE_RUN    4

    #define MAX_UNFOLDFRAME         500     // ms total unfold time
    #define MAX_AUTOHIDE            500     // ms total autohide animation time

    #ifdef LINKLIST_HEADER_INCLUDED

        /*
         *@@ WIDGETVIEWSTATE:
         *      private structure stored in the XCenter's
         *      XCENTERWINDATA.llWidgets list for each
         *      open widget.
         *
         *      This contains the XCENTERWIDGET structure
         *      that the widget is allowed to see. A
         *      direct pointer into that structure is
         *      passed to the widget on WM_CREATE, while
         *      we can use this to cache additional data
         *      that the widget itself should not see.
         *
         *@@added V0.9.7 (2000-12-14) [umoeller]
         */

        typedef struct _WIDGETVIEWSTATE
        {
            XCENTERWIDGET   Widget;
                        // widget's public data; this must be
                        // the first member of this structure,
                        // or WM_DESTROY in ctrDefWidgetProc
                        // won't work, which relies on these
                        // two structs having the same address

            SIZEL           szlWanted;
                        // the widget's desired size (as was
                        // last queried using WM_CONTROL with
                        // XN_QUERYSIZE)

            LONG            xCurrent;
                        // widget's current position; y is fixed currently
            LONG            xSizingBar;
                        // position of sizing bar to draw after this
                        // widget, if Widget.fSizeable == TRUE;
                        // otherwise this is 0
            SIZEL           szlCurrent;
                        // widget's current width and height;
                        // the width is normally the same as
                        // szlWanted.cx, except when the widget
                        // is sizeable

        } WIDGETVIEWSTATE, *PWIDGETVIEWSTATE;

        /*
         *@@ XCENTERWINDATA:
         *      general view-specific data for the XCenter.
         *      This is stored in QWL_USER of both the
         *      XCenter frame (fnwpXCenterMainFrame) and
         *      the client (fnwpXCenterMainClient).
         *
         *      This is allocated via wpAllocMem in
         *      ctrpCreateXCenterView and destroyed on
         *      WM_DESTROY in fnwpXCenterMainClient.
         *
         *      This structure is private and not seen
         *      by the widgets. However, this contains
         *      the XCENTERGLOBALS member, whose pointer
         *      is passed to the widgets in each XCENTERWIDGET
         *      structure.
         */

        typedef struct _XCENTERWINDATA
        {
            USHORT              cbSize;             // size of struct (req. by PM)
            XCenter             *somSelf;           // XCenter instance

            TID                 tidXCenter;         // thread ID of XCenter GUI thread
            HEV                 hevRunning;         // event sem posted once XCenter is created

            USEITEM             UseItem;            // use item; immediately followed by view item
            VIEWITEM            ViewItem;           // view item

            XCENTERGLOBALS      Globals;            // public data; a ptr to this is stored in
                                                    // each created XCENTERWIDGET

            LINKLIST            llWidgets;          // linked list of PXCENTERWIDGETVIEW pointers;
                                                    // list is not auto-free (ctrpCreateXCenterView)

            PFNWP               pfnwpFrameOrig;     // original frame window proc (subclassed)

                    // NOTE: the view settings (border width, spacing) are in XCENTERGLOBALS.

            LONG                yFrame;             // current frame y pos
            ULONG               cxFrame,            // always screen width
                                cyFrame;            // XCenter frame height (even if hidden)

            HWND                hwndContextMenu;    // if != NULLHANDLE, a context menu is showing

            BOOL                fShowingSettingsDlg; // if TRUE, a widget settings dlg is showing

            ULONG               ulStartTime;        // for animation 1 (TIMERID_UNFOLDFRAME)
            ULONG               ulWidgetsShown;     // for animation 2 (TIMERID_SHOWWIDGETS)

            ULONG               idTimerAutohideStart; // if != 0, TIMERID_AUTOHIDE_START is running
            ULONG               idTimerAutohideRun; // if != 0, TIMERID_AUTOHIDE_RUN is running

            BOOL                fFrameFullyShown;   // FALSE while the "unfold" animation is still
                                                    // running; always TRUE afterwards.

            BOOL                fFrameAutoHidden;   // if TRUE, frame is currently auto-hidden;
                                                    // this changes frequently

            HWND                hwndTooltip;        // tooltip control (comctl.c)

        } XCENTERWINDATA, *PXCENTERWINDATA;

        /*
         *@@ WGTSETTINGSTEMP:
         *      temporaray structure for ctrpShowSettingsDlg.
         *      This is needed to store a bunch of temporary
         *      data while a settings dialog is open to
         *      allow ctrSetSetupString to work.
         *
         *@@added V0.9.7 (2000-12-07) [umoeller]
         */

        typedef struct _WGTSETTINGSTEMP
        {
            PXCENTERWINDATA         pXCenterData;
                        // if != NULL, ptr to win data of open XCenter

            PXCENTERWIDGETSETTING   pSetting;
                        // ptr to internal settings list item

            PXCENTERWIDGET          pWidget;
                        // if != NULL, ptr to open widget view

            ULONG                   ulIndex;
                        // index of the widget for which settings dlg is
                        // shown
        } WGTSETTINGSTEMP, *PWGTSETTINGSTEMP;
    #endif // LINKLIST_HEADER_INCLUDED

    /* ******************************************************************
     *
     *   Desktop workarea resizing
     *
     ********************************************************************/

    APIRET ctrpDesktopWorkareaSupported(VOID);

    /* ******************************************************************
     *
     *   Internal widgets management
     *
     ********************************************************************/

    #ifdef LINKLIST_HEADER_INCLUDED
        VOID ctrpShowSettingsDlg(PXCENTERWINDATA pXCenterData,
                                 PXCENTERWIDGET pWidget);

        VOID ctrpDrawEmphasis(PXCENTERWINDATA pXCenterData,
                              HWND hwnd,
                              BOOL fRemove,
                              HPS hpsPre);

        VOID ctrpReformat(PXCENTERWINDATA pXCenterData,
                          ULONG ulFlags);

    #endif // LINKLIST_HEADER_INCLUDED

    /* ******************************************************************
     *
     *   XCenter widget class management
     *
     ********************************************************************/

    VOID ctrpLoadClasses(VOID);

    VOID ctrpFreeClasses(VOID);

    PXCENTERWIDGETCLASS ctrpFindClass(const char *pcszWidgetClass);

    /* ******************************************************************
     *
     *   XCenter view implementation
     *
     ********************************************************************/

    #ifdef SOM_XCenter_h

        ULONG ctrpQueryWidgetIndexFromHWND(XCenter *somSelf,
                                           HWND hwnd);

        BOOL ctrpRemoveWidget(XCenter *somSelf,
                              ULONG ulIndex);

        BOOL ctrpMoveWidget(XCenter *somSelf,
                            ULONG ulIndex2Move,
                            ULONG ulBeforeIndex);

        VOID ctrpFreeWidgets(XCenter *somSelf);

        PVOID ctrpQueryWidgets(XCenter *somSelf,
                               PULONG pulCount);

        VOID ctrpFreeWidgetsBuf(PVOID pBuf,
                                ULONG ulCount);

        BOOL ctrpInsertWidget(XCenter *somSelf,
                              ULONG ulBeforeIndex,
                              const char *pcszWidgetClass,
                              const char *pcszSetupString);

        PSZ ctrpStuffSettings(XCenter *somSelf,
                              PULONG pcbSettingsArray);

        ULONG ctrpUnstuffSettings(XCenter *somSelf);

        #ifdef LINKLIST_HEADER_INCLUDED
            PLINKLIST ctrpQuerySettingsList(XCenter *somSelf);
        #endif

        BOOL ctrpSetPriority(XCenter *somSelf,
                             ULONG ulClass,
                             LONG lDelta);

        ULONG ctrpQuerySetup(XCenter *somSelf,
                             PSZ pszSetupString,
                             ULONG cbSetupString);

        BOOL ctrpModifyPopupMenu(XCenter *somSelf,
                                HWND hwndMenu);

        HWND ctrpCreateXCenterView(XCenter *somSelf,
                                   HAB hab,
                                   ULONG ulView,
                                   PVOID *ppvOpenView);
    #endif // SOM_XCenter_h

    /* ******************************************************************
     *
     *   Private messages _to_ XCenter client
     *
     ********************************************************************/

    /*
     *@@ XCM_CREATEWIDGET:
     *      creates a widget window. This must be _sent_
     *      to the client... it's an internal message
     *      and may only be sent after the widget setting
     *      has been set up.
     *
     *      Parameters:
     *
     *      -- PXCENTERWIDGETSETTING mp1: setting for
     *              widget to be created.
     *
     *      -- ULONG mp2: widget index.
     *
     *@@added V0.9.7 (2001-01-03) [umoeller]
     */

    #define XCM_CREATEWIDGET            (WM_USER + 3)

    /*
     *@@ XCM_DESTROYWIDGET:
     *      destroys a widget window. This is an internal
     *      message and must only be sent after a widget
     *      setting has already been destroyed.
     *
     *      NEVER send this yourself.
     *      Use XCenter::xwpRemoveWidget instead.
     *
     *      Parameters:
     *
     *      -- HWND mp1: widget window to be destroyed.
     *
     *      -- mp2: reserved, must be 0.
     *
     *@@added V0.9.7 (2001-01-03) [umoeller]
     */

    #define XCM_DESTROYWIDGET           (WM_USER + 4)

    /* ******************************************************************
     *
     *   XCenter notebook callbacks (notebook.c)
     *
     ********************************************************************/

    #ifdef NOTEBOOK_HEADER_INCLUDED
        VOID ctrpView1InitPage(PCREATENOTEBOOKPAGE pcnbp,
                               ULONG flFlags);

        MRESULT ctrpView1ItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                    USHORT usItemID, USHORT usNotifyCode,
                                    ULONG ulExtra);

        VOID ctrpView2InitPage(PCREATENOTEBOOKPAGE pcnbp,
                               ULONG flFlags);

        MRESULT ctrpView2ItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                    USHORT usItemID, USHORT usNotifyCode,
                                    ULONG ulExtra);

        VOID ctrpWidgetsInitPage(PCREATENOTEBOOKPAGE pcnbp,
                                 ULONG flFlags);

        MRESULT ctrpWidgetsItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                       USHORT usItemID, USHORT usNotifyCode,
                                       ULONG ulExtra);
    #endif

#endif

