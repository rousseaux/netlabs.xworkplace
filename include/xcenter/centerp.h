
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
    #define WNDCLASS_WIDGET_DTPBUTTON   "XWPCenterXButtonWidget"
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
         *      is passed to the widgets in each XCENTERWIDGETVIEW
         *      structure.
         */

        typedef struct _XCENTERWINDATA
        {
            USHORT              cbSize;             // size of struct (req. by PM)
            XCenter             *somSelf;           // XCenter instance

            USEITEM             UseItem;            // use item; immediately followed by view item
            VIEWITEM            ViewItem;           // view item

            XCENTERGLOBALS      Globals;            // public data; a ptr to this is stored in
                                                    // each created XCENTERWIDGETVIEW

            LINKLIST            llWidgetsLeft;      // linked list of PXCENTERWIDGETVIEW pointers;
                                                    // list is not auto-free (ctrpCreateXCenterView)

            PFNWP               pfnwpFrameOrig;     // original frame window proc (subclassed)

            LONG                yFrame;             // current frame y pos
            ULONG               cxFrame,            // always screen width
                                cyFrame;            // XCenter frame height (even if hidden)

            HWND                hwndContextMenu;    // if != NULLHANDLE, a context menu is showing

            BOOL                fShowingSettingsDlg; // if TRUE, a widget settings dlg is showing

            ULONG               ulStartTime;        // for animation 1 (TIMERID_UNFOLDFRAME)
            ULONG               ulWidgetsShown;     // for animation 2 (TIMERID_SHOWWIDGETS)
            ULONG               idTimerAutohide;    // if != 0, TIMERID_AUTOHIDE_START is running
            BOOL                fFrameAutoHidden;   // if TRUE, frame is currently sunk
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

            PXCENTERWIDGETVIEW      pViewData;
                        // if != NULL, ptr to open widget view

            ULONG                   ulIndex;
                        // index of the widget for which settings dlg is
                        // shown
        } WGTSETTINGSTEMP, *PWGTSETTINGSTEMP;

        VOID ctrpShowSettingsDlg(PXCENTERWINDATA pXCenterData,
                                 PXCENTERWIDGETVIEW pViewData);

        VOID ctrpDrawEmphasis(PXCENTERWINDATA pXCenterData,
                              HWND hwnd,
                              BOOL fRemove,
                              HPS hpsPre);

        VOID ctrpReformatFrame(PXCENTERWINDATA pXCenterData);

    #endif // LINKLIST_HEADER_INCLUDED

    VOID ctrpReformatFrameHWND(HWND hwnd);

    #ifdef SOM_XCenter_h
        VOID ctrpLoadClasses(VOID);

        VOID ctrpFreeClasses(VOID);

        PXCENTERWIDGETCLASS ctrpFindClass(XCenter *somSelf,
                                          const char *pcszWidgetClass);

        ULONG ctrpQueryWidgetIndexFromHWND(XCenter *somSelf,
                                           HWND hwnd);

        VOID ctrpAddWidget(XCenter *somSelf,
                           PXCENTERWIDGETSETTING pSetting,
                           PULONG pulNewItemCount);

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

        ULONG ctrpQuerySetup(XCenter *somSelf,
                             PSZ pszSetupString,
                             ULONG cbSetupString);

        BOOL ctrpModifyPopupMenu(XCenter *somSelf,
                                HWND hwndMenu);

        HWND ctrpCreateXCenterView(XCenter *somSelf,
                                   HAB hab,
                                   ULONG ulView);
    #endif // SOM_XCenter_h

    /* ******************************************************************
     *
     *   XCenter notebook callbacks (notebook.c)
     *
     ********************************************************************/

    #ifdef NOTEBOOK_HEADER_INCLUDED
        VOID ctrpViewInitPage(PCREATENOTEBOOKPAGE pcnbp,
                             ULONG flFlags);

        MRESULT ctrpViewItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                   USHORT usItemID, USHORT usNotifyCode,
                                   ULONG ulExtra);

        VOID ctrpWidgetsInitPage(PCREATENOTEBOOKPAGE pcnbp,
                                 ULONG flFlags);

        MRESULT ctrpWidgetsItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                       USHORT usItemID, USHORT usNotifyCode,
                                       ULONG ulExtra);
    #endif

#endif

