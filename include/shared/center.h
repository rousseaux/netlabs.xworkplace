
/*
 *@@sourcefile center.h:
 *      public header file for the XCenter.
 *      This contains all declarations which are needed by
 *      all parts of the XCenter and to implement widget
 *      plugin DLLs.
 *
 *      WARNING: The XCenter is still work in progress. The
 *      definitions in this file are still subject to change.
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
    #define WC_XCENTER_CLIENT     "XWPCenterClient"

    // button types (src\xcenter\w_objbutton.c)
    #define BTF_OBJBUTTON       1
    #define BTF_XBUTTON         2

    // position flags
    #define XCENTER_BOTTOM          0
    #define XCENTER_TOP             1

    // display style
    #define XCS_FLATBUTTONS         0x0001
    #define XCS_SUNKBORDERS         0x0002

    /*
     *@@ XCENTERGLOBALS:
     *      global data for a running XCenter instance.
     *      A pointer to this structure exists in each
     *      XCENTERWIDGET instance so that the widgets
     *      can access some of the global data.
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

        PVOID               pCountrySettings;
                    // country settings; this points to a COUNTRYSETTINGS
                    // structure (prfh.h)

        ULONG               cyTallestWidget;
                    // height of client (same as height of all widgets!)

        ULONG               cxMiniIcon;
                    // system mini-icon size (for convenience); either 16 or 20

        LONG                lcol3DDark,
                            lcol3DLight;
                    // system colors for 3D frames (for convenience; RGB!)

        // the following are the width settings from the second "View"
        // settings page;
        // a widget may or may not want to consider these.
        ULONG               flDisplayStyle;
                    // XCenter display style flags;
                    // a widget may or may not want to consider these.
                    // These flags can be:
                    // -- XCS_FLATBUTTONS: paint buttons flat. If not set,
                    //      paint them raised.
                    // -- XCS_SUNKBORDERS: paint static controls (e.g. CPU meter)
                    //      with a "sunk" 3D frame. If not set, do not.

        ULONG               ul3DBorderWidth;
                    // 3D border width
        ULONG               ulBorderSpacing;
                    // border spacing (added to 3D border width)
        ULONG               ulSpacing;
                    // spacing between widgets

    } XCENTERGLOBALS, *PXCENTERGLOBALS;

    // forward declaration
    typedef struct _XCENTERWIDGET *PXCENTERWIDGET;

    /*
     *@@ WIDGETSETTINGSDLGDATA:
     *      a bunch of data passed to a "show settings
     *      dialog" function (WGTSHOWSETTINGSDLG), if
     *      the widget has specified such a thing.
     *
     *      XCenter widget settings dialogs basically
     *      work as follows:
     *
     *      1.  You must a function that displays a modal
     *          dialog. This function must have the following
     *          prototype:
     *
     +              VOID EXPENTRY ShowSettingsDlg(PWIDGETSETTINGSDLGDATA pData)
     *
     *      2.  Store that function in the
     *          XCENTERWIDGETCLASS.pShowSettingsDlg field for
     *          your widget class.
     *
     *          This will enable the "Properties" menu item
     *          for the widget.
     *
     *      3.  Your function gets called when the user selects
     *          "Properties". What that function does, doesn't
     *          matter... it should however display a modal
     *          dialog and update the widget's settings string
     *          and call ctrSetSetupString with the "hSettings"
     *          handle that was given to it in the
     *          WIDGETSETTINGSDLGDATA structure. This will give
     *          the widget the new settings.
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
                    // for which widget data is being changed.
                    // If NULL, the XCenter isn't currently
                    // open.

        PXCENTERWIDGET          pView;
                    // if != NULL, currently open view
                    // of the widget. If NULL, the widget
                    // isn't currently open.

        PVOID                   pUser;
                    // some room for additional data the
                    // settings dialog might want

    } WIDGETSETTINGSDLGDATA, *PWIDGETSETTINGSDLGDATA;

    typedef VOID EXPENTRY WGTSHOWSETTINGSDLG(PWIDGETSETTINGSDLGDATA);
    typedef WGTSHOWSETTINGSDLG *PWGTSHOWSETTINGSDLG;

    // widget class flags
    #define WGTF_SIZEABLE               0x0001
    #define WGTF_NOUSERCREATE           0x0002
    #define WGTF_UNIQUEPERXCENTER       0x0004
    #define WGTF_UNIQUEGLOBAL          (0x0008 + 0x0004)
    #define WGTF_TOOLTIP                0x0010

    /*
     *@@ XCENTERWIDGETCLASS:
     *      describes one widget class which can be
     *      used in an XCenter. Such a class can either
     *      be internal or in a widget plugin DLL.
     */

    typedef struct _XCENTERWIDGETCLASS
    {
        const char      *pcszPMClass;
                // PM window class name of this widget class (can be shared among
                // several widget classes). A plugin DLL is responsible for
                // registering this window class when it's loaded.

        ULONG           ulExtra;
                // additional identifiers the class might need if the
                // same PM window class is used for several widget classes.
                // This is not used by the XCenter, but you can access it
                // during WM_CREATE so you can differentiate between several
                // widget classes in the same window proc. You must copy this
                // to your private widget data then.

        const char      *pcszWidgetClass;
                // internal widget class name; this is used to identify
                // the class. This must be unique on the system.

        const char      *pcszClassTitle;
                // widget class title (shown to user in "Add widget" popup
                // menu).

        ULONG           ulClassFlags;
                // WGTF_* flags; any combination of the following:
                // -- WGTF_SIZEABLE: widget window can be resized with
                //    the mouse by the user.
                // -- WGTF_NOUSERCREATE: do not show this class in
                //    the "add widget" menu, and do not allow creating
                //    instances of this in the XCenter settings notebook.
                // -- WGTF_UNIQUEPERXCENTER: only one widget of this class
                //    should be created per XCenter.
                // -- WGTF_UNIQUEGLOBAL: only one widget of this class
                //    should be created in all XCenters on the system.
                //    This includes WGTF_UNIQUEPERXCENTER.
                // -- WGTF_TOOLTIP: if set, the widget has a tooltip
                //    and will receive WM_CONTROL messages with the
                //    TTN_NEEDTEXT notification code (see helpers\comctl.h).
                //    The window ID of the tooltip control is ID_XCENTER_TOOLTIP.

        PWGTSHOWSETTINGSDLG pShowSettingsDlg;
                // if the widget supports a settings dialog,
                // it must set this func pointer to a procedure
                // that will show that dialog. If this is != NULL,
                // the "Properties" menu item and the button in
                // the widgets list of the XCenter settings notebook
                // will be enabled. See WIDGETSETTINGSDLGDATA for
                // details about how to implement widget settings dialogs.

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
     *@@ XCENTERWIDGET:
     *      public structure to hold data for an XCenter
     *      widget, i.e. a PM window in the XCenter client
     *      area.
     *
     *      Each XCenter widget is a separate window,
     *      being a child window of the XCenter client,
     *      which in turn is a child of the XCenter frame.
     *
     *      This structure is created once for each widget
     *      and passed to each widget in mp1 of WM_CREATE.
     *      The first thing a widget must do on WM_CREATE
     *      is to store a pointer to this structure in its
     *      QWL_USER window word.
     *
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

    typedef struct _XCENTERWIDGET
    {
        /*
         *  Informational fields:
         *      all these are meant for reading only
         *      and set up by the XCenter for the widget.
         */

        HWND        hwndWidget;
                // window handle of this widget; this is valid
                // only _after_ WM_CREATE.

        HAB         habWidget;
                // widget's anchor block (copied for convenience).

        PFNWP       pfnwpDefWidgetProc;
                // address of default widget window procedure. This
                // always points to ctrDefWidgetProc (in
                // src/shared/center.c). The widget's own window
                // proc must pass all unprocessed messages (and a
                // few more) to this instead of WinDefWindowProc.
                // See ctrDefWidgetProc for details.

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
        ULONG       ulClassFlags;
                // class flags copied from XCENTERWIDGETCLASS.

        const char  *pcszSetupString;
                // class-specific setup string. This field
                // is only valid during WM_CREATE and then holds
                // the setup string which was last stored with
                // the widget. The pointer can be NULL if no
                // setup string exists. After WM_CREATE, the
                // pointer is set to NULL always.

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
                // own data (which you must clean up yourself on
                // WM_DESTROY).
    } XCENTERWIDGET;

    /* ******************************************************************
     *
     *   Public widget helpers (exported from XFLDR.DLL)
     *
     ********************************************************************/

    // #define SETUP_SEPARATOR     "øøø"

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
                // a pointer to this is in XCENTERWIDGET

    /* ******************************************************************
     *
     *   WM_CONTROL notification codes _from_ XCenter client
     *
     ********************************************************************/

    #define ID_XCENTER_CLIENT           7000
    #define ID_XCENTER_TOOLTIP          7001

    /*
     *@@ XN_QUERYSIZE:
     *      notification code for WM_CONTROL sent from
     *      the XCenter to a widget when it needs to
     *      know its desired size. This comes in once
     *      after WM_CREATE and may come in again later
     *      if the user changes XCenter view settings.
     *
     *      Parameters:
     *
     *      -- SHORT1FROMMP(mp1): ID, always ID_XCENTER_CLIENT.
     *
     *      -- SHORT2FROMMP(mp1): notify code (XN_QUERYSIZE).
     *
     *      -- PSIZEL mp2: pointer to a SIZEL structure in which
     *                     the widget must fill in its desired
     *                     size.
     *
     *      The widget must return TRUE if it has put its
     *      desired size into the SIZEL structure. Otherwise
     *      the XCenter will assume some dumb default for
     *      the widget size.
     *
     *      After all widgets have been created, the XCenter
     *      (and all widgets) are resized to have the largest
     *      cy requested. As a result, your window proc cannot
     *      assume that it will always have the size it
     *      requested.
     *
     *@@added V0.9.7 (2000-12-14) [umoeller]
     */

    #define XN_QUERYSIZE                1

    /*
     *@@ XN_SETUPCHANGED:
     *      notification code for WM_CONTROL sent from
     *      the XCenter to a widget when its setup string
     *      has changed.
     *
     *      Parameters:
     *
     *      -- SHORT1FROMMP(mp1): ID, always ID_XCENTER_CLIENT.
     *
     *      -- SHORT2FROMMP(mp1): notify code (XN_SETUPCHANGED).
     *
     *      -- const char* mp2: pointer to a new zero-termianted
     *                          setup string.
     *
     *      The widget must return TRUE if it has processed
     *      the setup string successfully.
     *
     *      This gets sent to an open widget when
     *      ctrSetSetupString has been invoked on it to allow
     *      it to update its display. This normally happens
     *      when its settings dialog saves a new setup string.
     *
     *@@added V0.9.7 (2000-12-13) [umoeller]
     */

    #define XN_SETUPCHANGED             2

    /*
     *@@ XN_OBJECTDESTROYED:
     *      notification code for WM_CONTROL posted (!)
     *      to a widget if it has registered itself with
     *      XFldObject::xwpSetWidgetNotify.
     *
     *      This is a last chance for the widget to clean
     *      up itself when an object that it relies on gets
     *      destroyed (deleted or made dormant). This message
     *      is posted from XFldObject::wpUnInitData.
     *
     *      Parameters:
     *
     *      -- SHORT1FROMMP(mp1): ID, always ID_XCENTER_CLIENT.
     *
     *      -- SHORT2FROMMP(mp1): notify code (XN_OBJECTDESTROYED).
     *
     *      -- WPObject* mp2: SOM object pointer of object
     *                        being destroyed. NOTE: This pointer
     *                        is no longer valid. Do not invoke
     *                        any SOM methods on it.
     *
     *@@added V0.9.7 (2001-01-03) [umoeller]
     */

    #define XN_OBJECTDESTROYED          3

    /* ******************************************************************
     *
     *   Public messages _to_ XCenter client
     *
     ********************************************************************/

    /*
     *@@ XCM_SETWIDGETSIZE:
     *      this msg can be posted to the client
     *      from a widget if it wants to change
     *      its size, e.g. because its display
     *      has changed and it needs more room.
     *
     *      Parameters:
     *
     *      -- HWND mp1: widget's window.
     *
     *      -- ULONG mp2: the new width that the
     *         widget wants to have.
     *
     *      Note: _Post_, do not send this message
     *      to the client. This causes excessive
     *      redraw of possibly all widgets.
     */

    #define XCM_SETWIDGETSIZE           WM_USER

    // formatting flags
    #define XFMF_DISPLAYSTYLECHANGED    0x0002
    #define XFMF_GETWIDGETSIZES         0x0001
    #define XFMF_RECALCHEIGHT           0x0004
    #define XFMF_REPOSITIONWIDGETS      0x0008
    #define XFMF_SHOWWIDGETS            0x0010
    #define XFMF_RESURFACE              0x0020

    /*
     *@@ XCM_REFORMAT:
     *      reformats the XCenter and all widgets. This
     *      gets posted by ctrDefWidgetProc when a widget
     *      gets destroyed, but can be posted by anyone.
     *
     *      Parameters:
     *
     *      -- HWND mp1: reformat flags. Any combination
     *         of the following:
     *
     *          --  XFMF_GETWIDGETSIZES: ask each widget for its
     *              desired size.
     *
     *          --  XFMF_DISPLAYSTYLECHANGED: display style has
     *              changed, repaint everything.
     *
     *          --  XFMF_RECALCHEIGHT: recalculate the XCenter's
     *              height, e.g. if a widget has been added or
     *              removed or vertically resized.
     *
     *          --  XFMF_REPOSITIONWIDGETS: reposition all widgets.
     *              This is necessary if a widget's horizontal size
     *              has changed.
     *
     *          --  XFMF_SHOWWIDGETS: set WS_VISIBLE on all widgets.
     *
     *          --  XFMF_RESURFACE: resurface XCenter to HWND_TOP.
     *
     *          Even if you specify 0, the XCenter will be re-shown
     *          if it is currently auto-hidden.
     *
     *      -- mp2: reserved, must be 0.
     *
     *      NOTE: This msg must be POSTED, not sent, to the XCenter
     *      client. It causes excessive redraw.
     */

    #define XCM_REFORMAT                (WM_USER + 1)

    /*
     *@@ XCM_SAVESETUP:
     *      this msg can be sent (!) to the client
     *      by a widget if its settings have been
     *      changed and it wants these settings to
     *      be saved with the XCenter instance data.
     *
     *      This is useful when fonts or colors have
     *      been dropped on the widget and no settings
     *      dialog is currently open (and ctrSetSetupString
     *      therefore won't work).
     *
     *      Parameters:
     *
     *      -- HWND mp1: widget's window.
     *
     *      -- const char* mp2: zero-terminated setup string.
     *
     *@@added V0.9.7 (2000-12-04) [umoeller]
     */

    #define XCM_SAVESETUP               (WM_USER + 2)

    // WM_USER + 3 and WM_USER + 4 are reserved

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

