
/*
 *@@sourcefile center.h:
 *      public header file for the XCenter.
 *      This contains all declarations which are needed by
 *      all parts of the XCenter and to implement widget
 *      plugin DLLs.
 *
 *@@include #include "shared\center.h"
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

#ifndef CENTER_HEADER_INCLUDED
    #define CENTER_HEADER_INCLUDED

    /* ******************************************************************
     *
     *   Public definitions
     *
     ********************************************************************/

    // PM window class name for XCenter client (needed by XWPDaemon)
    #define WNDCLASS_XCENTER_CLIENT     "XWPCenterClient"

    // position flags
    #define XCENTER_BOTTOM          0
    #define XCENTER_TOP             1

    // widget class flags
    #define WGTF_SIZEABLE               0x0001
    #define WGTF_NOUSERCREATE           0x0002
    #define WGTF_UNIQUEPERXCENTER       0x0004
    #define WGTF_UNIQUEGLOBAL          (0x0008 + 0x0004)

    /*
     *@@ XCENTERGLOBALS:
     *      global data for a running XCenter instance.
     *      A pointer to this structure exists in
     *      each XCENTERWIDGETVIEW instance so that
     *      the widgets can access some of the global
     *      data.
     *
     *      In this structure, an XCenter instance
     *      passes a number of variables to its member
     *      widgets so that they can access some things
     *      more quickly for convenience. This structure
     *      is fully initialized at the time the widgets
     *      are created.
     *
     *      "Globals" isn't really a good name since
     *      one of these structures is created for
     *      each open XCenter (and there can be several),
     *      but this is definitely more global than the
     *      widget-specific structures.
     */

    typedef struct _XCENTERGLOBALS
    {
        HAB                 hab;                // anchor block of frame and client
        HWND                hwndFrame,          // XCenter frame window
                            hwndClient;         // client (child of XCenter frame)

        ULONG               cyClient;           // height of client (same as height
                                                // of all widgets!)
        ULONG               ulSpacing;          // spacing between widgets; this is
                                                // WinQuerySysValue(HWND_DESKTOP, SV_CXDLGFRAME);

        PVOID               pCountrySettings;   // country settings; this points to
                                                // a COUNTRYSETTINGS structure (prfh.h)

        ULONG               cxMiniIcon;         // system mini-icon size

        LONG                lcol3DDark,
                            lcol3DLight;        // system colors for 3D frames (RGB!)
    } XCENTERGLOBALS, *PXCENTERGLOBALS;

    // forward declaration
    typedef struct _XCENTERWIDGETVIEW *PXCENTERWIDGETVIEW;

    /*
     *@@ WIDGETSETTINGSDLGDATA:
     *      a bunch of data passed to a "show settings
     *      dialog" function (WGTSHOWSETTINGSDLG), if
     *      the widget has specified such a thing.
     *
     *      XCenter widget settings dialogs basically
     *      work as follows:
     *
     *      If a widget class supports settings dialogs,
     *      it must specify this in its XCENTERWIDGETCLASS.
     *
     *@@added V0.9.7 (2000-12-07) [umoeller]
     */

    typedef struct _WIDGETSETTINGSDLGDATA
    {
        HWND                    hwndOwner;
                    // proposed owner for settings dialog

        const char              *pcszSetupString;
                    // present setup string (do not change)

        const LHANDLE           hSettings;
                    // widget setting handle; this must
                    // be passed to ctrSetSetupString;
                    // DO NOT CHANGE

        PXCENTERGLOBALS         pGlobals;
                    // if != NULL, currently open XCenter
                    // for which widget data is being changed

        PXCENTERWIDGETVIEW      pView;
                    // if != NULL, currently open view
                    // of the widget

        PVOID                   pUser;
                    // some room for additional data the
                    // settings dialog might want

    } WIDGETSETTINGSDLGDATA, *PWIDGETSETTINGSDLGDATA;

    typedef VOID EXPENTRY WGTSHOWSETTINGSDLG(PWIDGETSETTINGSDLGDATA);
    typedef WGTSHOWSETTINGSDLG *PWGTSHOWSETTINGSDLG;

    typedef VOID EXPENTRY WGTSETUPSTRINGCHANGED(PXCENTERWIDGETVIEW, const char*);
    typedef WGTSETUPSTRINGCHANGED *PWGTSETUPSTRINGCHANGED;

    /*
     *@@ XCENTERWIDGETCLASS:
     *      describes one widget class which can be
     *      used in an XCenter. Such a class can either
     *      be internal or in a widget plugin DLL.
     */

    typedef struct _XCENTERWIDGETCLASS
    {
        const char      *pcszPMClass;       // PM window class of this widget
                                            // (can be shared among several widgets)

        ULONG           ulExtra;
                // additional identifiers the class might need if the
                // same PM window class is used for several widget classes

        const char      *pcszWidgetClass;   // internal widget class name

        const char      *pcszClassTitle;    // widget class title (shown to user)

        ULONG           ulClassFlags;
                // WGTF_* flags; any combination of the following:
                // -- WGTF_SIZEABLE: widget window can be resized.
                // -- WGTF_NOUSERCREATE: do not show this class in
                //    the "add widget" menu, and do not allow creating
                //    instances of this in the XCenter settings notebook.
                // -- WGTF_UNIQUEPERXCENTER: only one widget of this class
                //    should be created per XCenter.
                // -- WGTF_UNIQUEGLOBAL: only one widget of this class
                //    should be created in all XCenters on the system.
                //    This includes WGTF_UNIQUEPERXCENTER.

        PWGTSHOWSETTINGSDLG pShowSettingsDlg;
                // if the widget supports a settings dialog,
                // it must set this func pointer to a procedure
                // that will show that dialog. If this is != NULL,
                // the "Properties" menu item and the button in
                // the widgets list of the XCenter settings notebook
                // will be enabled.

    } XCENTERWIDGETCLASS, *PXCENTERWIDGETCLASS;

    /*
     *@@ XCENTERWIDGETSETTING:
     *      describes one widget to be created. One instance
     *      of this is created for each widget that the
     *      user has configured for a widget.
     *
     *      An array of these is returned by XCenter::xwpQueryWidgets.
     */

    typedef struct _XCENTERWIDGETSETTING
    {
        PSZ                 pszWidgetClass;   // widget class name;
                    // we cannot use the binary PXCENTERWIDGETCLASS pointer
                    // here because these structures are dynamically unloaded...
                    // use ctrpFindClass to find the XCENTERWIDGETCLASS ptr.
        PSZ                 pszSetupString;
                    // widget-class-specific setup string; can be NULL

            // Note: both pointers are assumed to be allocated
            // using malloc() or strdup() and are automatically
            // freed.

    } XCENTERWIDGETSETTING, *PXCENTERWIDGETSETTING;

    /*
     *@@ XCENTERWIDGETVIEW:
     *      structure for describing a single XCenter
     *      widget, i.e. a rectangular area shown in
     *      the XCenter client area.
     *
     *      Each XCenter widget is a separate window,
     *      being a child window of the XCenter client,
     *      which in turn is a child of the XCenter frame.
     *
     *      This structure is created once for each widget
     *      and passed to each widget in mp1 of WM_CREATE.
     *      After WM_CREATE, the XCenter stores a pointer
     *      to this structure in the QWL_USER window word
     *      of the widget.
     *      The widget can allocate another widget-specific
     *      buffer and store its pointer into the pUser
     *      field.
     *
     *      This structure is automatically freed by the
     *      XCenter when the widget is destroyed. However,
     *      if you allocated more memory for pUser, it is
     *      your own responsibility to free that on WM_DESTROY.
     *      General rule: The widget must clean up what it
     *      allocated itself.
     */

    typedef struct _XCENTERWIDGETVIEW
    {
        /*
         *  Informational fields:
         *      all these are meant for reading only
         *      and set up by the XCenter for the widget.
         */

        HWND        hwndWidget;
                // window handle of this widget; this is valid
                // only after WM_CREATE

        HAB         habWidget;
                // widget's anchor block (copied for convenience)

        PFNWP       pfnwpDefWidgetProc;
                // address of default widget window procedure. The
                // widget's own window proc must pass all unprocessed
                // messages (and a few more) to this instead of
                // WinDefWindowProc. See ctrDefWidgetProc (in
                // src/shared/center.c) for details.

        const XCENTERGLOBALS *pGlobals;
                // ptr to client/frame window data (do not change)

        PXCENTERWIDGETCLASS pWidgetClass;
                // widget class this widget was created from;
                // this ptr is only valid during WM_CREATE and
                // always NULL afterwards

        const char  *pcszWidgetClass;
                // internal name of the widget's class; this
                // is the same as pWidgetClass->pcszWidgetClass,
                // but this is valid after WM_CREATE.
                // DO NOT CHANGE it, the XCenter relies on this!

        const char  *pcszSetupString;
                // class-specific setup string. This field
                // is only valid during WM_CREATE and then holds
                // the setup string which was last stored with
                // the widget. The pointer can be NULL if no
                // setup string exists. After WM_CREATE, the
                // pointer is set to NULL always.

        LONG        xCurrent,
                // current X coordinate (do not change);
                // this is relative to the XCenter client
                // and only valid _after_ WM_CREATE.
                    cxCurrent,
                // current width; same rules.
                    cyCurrent;
                // current height; same rules.

        /*
         *  Setup fields:
         *      all these should be set up by the widget
         *      (normally during WM_CREATE). The XCenter
         *      reads these fields to find out more about
         *      the widget's wishes.
         *
         *      All these fields are initialized to safe
         *      defaults, which are probably not suitable
         *      for most widgets though.
         */

        LONG        cxWanted,
                // desired width in pixels. On WM_CREATE, the widget should
                // write its desired width here. If it sets this
                // to -1, it will get all remaining space on the
                // XCenter; if several widgets request this, the
                // available remaining space will be distributed
                // among the requesting widgets.
                // This defaults to 20.
                    cyWanted,
                // desired height in pixels. On WM_CREATE, the widget should
                // write its desired height here. The XCenter will
                // go thru all widgets after their creation and
                // set its client height to the largest height of all
                // widgets, and all widgets will be resized to that
                // largest height.
                // This defaults to 0.
                    cxMinimum;
                // absolute minimum width this widget will accept;
                // this is only important if it's resizeable.
                // This defaults to 20.

        BOOL        fSizeable;
                // if TRUE, the widget is sizeable with the mouse.
                // This is initially set to TRUE if the widget's
                // class has the WGTF_SIZEABLE flag, but can be
                // changed by the widget at any time.

        HWND        hwndContextMenu;
                // default context menu for this widget.
                // The XCenter loads a the same standard context menu
                // for each widget, which is displayed on WM_CONTEXTMENU
                // in ctrDefWidgetProc. Normally, the widget needs not
                // do anything to have the context menu displayed;
                // ctrDefWidgetProc takes care of this. However, you may
                // safely manipulate this context menu if you want to
                // insert/remove items.
                // The menu in here gets destroyed using WinDestroyWindow
                // when the widget window is destroyed.
                // This is only valid after WM_CREATE.

        PWGTSETUPSTRINGCHANGED pSetupStringChanged;
                // if the widget supports parsing setup strings
                // even after the widget has been created, it
                // must set this func pointer to a procedure
                // that will scan the setup string and update
                // the widget's display. This procedure gets
                // called when a widget's setup string is _changed_
                // for any reason, most importantly during a
                // call to ctrSetSetupString. To implement
                // working settings dialogs, you'll have to
                // implement this.
                // This defaults to NULL.

        const char  *pcszHelpLibrary;
        ULONG       ulHelpPanelID;
                // if these two are specified, the XCenter will
                // enable the "Help" item in the widget's context
                // menu and display the specified help panel when
                // the menu item is selected. Both default to NULL.

        /*
         *  Additional fields:
         */

        PVOID       pUser;
                // user data allocated by window class; this is
                // initially NULL, but you can use this for your
                // own data (which you must clean up yourself).
    } XCENTERWIDGETVIEW;

    /* ******************************************************************
     *
     *   Public widget helpers (exported from XFLDR.DLL)
     *
     ********************************************************************/

    #define SETUP_SEPARATOR     "øøø"

    PSZ APIENTRY ctrScanSetupString(const char *pcszSetupString,
                                    const char *pcszKeyword);
    typedef PSZ APIENTRY CTRSCANSETUPSTRING(const char *pcszSetupString,
                                            const char *pcszKeyword);
    typedef CTRSCANSETUPSTRING *PCTRSCANSETUPSTRING;

    LONG APIENTRY ctrParseColorString(const char *p);
    typedef LONG APIENTRY CTRPARSECOLORSTRING(const char *p);
    typedef CTRPARSECOLORSTRING *PCTRPARSECOLORSTRING;

    VOID APIENTRY ctrFreeSetupValue(PSZ p);
    typedef VOID APIENTRY CTRFREESETUPVALUE(PSZ p);
    typedef CTRFREESETUPVALUE *PCTRFREESETUPVALUE;

    BOOL APIENTRY ctrSetSetupString(LHANDLE hSetting, const char *pcszNewSetupString);
    typedef BOOL APIENTRY CTRSETSETUPSTRING(LHANDLE hSetting, const char *pcszNewSetupString);
    typedef CTRSETSETUPSTRING *PCTRSETSETUPSTRING;

    #ifdef PRFH_HEADER_INCLUDED
        BOOL ctrDisplayHelp(PXCENTERGLOBALS pGlobals,
                            const char *pcszHelpFile,
                            ULONG ulHelpPanelID);
        typedef BOOL CTRDISPLAYHELP(PXCENTERGLOBALS pGlobals,
                            const char *pcszHelpFile,
                            ULONG ulHelpPanelID);
        typedef CTRDISPLAYHELP *PCTRDISPLAYHELP;
    #endif

    MRESULT EXPENTRY ctrDefWidgetProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
                // a pointer to this is in XCENTERWIDGETVIEW

    /* ******************************************************************
     *
     *   Messages for XCenter client
     *
     ********************************************************************/

    #define XCM_SETWIDGETSIZE           WM_USER

    #define XCM_REFORMATALL             (WM_USER + 1)

    #define XCM_SAVESETUP               (WM_USER + 2)

    /* ******************************************************************
     *
     *   Widget Plugin DLL Exports
     *
     ********************************************************************/

    typedef ULONG EXPENTRY FNWGTINITMODULE(HAB hab,
                                           HMODULE hmodXFLDR,
                                           PXCENTERWIDGETCLASS *ppaClasses,
                                           PSZ pszErrorMsg);
    typedef FNWGTINITMODULE *PFNWGTINITMODULE;

    typedef VOID EXPENTRY FNWGTUNINITMODULE(VOID);
    typedef FNWGTUNINITMODULE *PFNWGTUNINITMODULE;
#endif

