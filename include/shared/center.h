
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
 *      If you are looking at this file from the "toolkit\shared"
 *      directory of a binary XWorkplace release, this is an
 *      exact copy of the file in "include\shared" from the
 *      XWorkplace sources.
 *
 *@@include #include "shared\center.h"
 */

/*
 *      Copyright (C) 2000-2001 Ulrich M”ller.
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

    // position flags (XCENTERGLOBALS.ulPosition)
    #define XCENTER_BOTTOM          0
    #define XCENTER_TOP             1

    // display style (XCENTERGLOBALS.flDisplayStyle)
    #define XCS_FLATBUTTONS         0x0001
    #define XCS_SUNKBORDERS         0x0002
    #define XCS_SIZINGBARS          0x0004
    #define XCS_ALL3DBORDERS        0x0008

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
     *      are created, but values may change while the
     *      XCenter is open.
     *
     *      "Globals" isn't really a good name since
     *      one of these structures is created for
     *      each open XCenter (and there can be several),
     *      but this is definitely more global than the
     *      widget-specific structures.
     */

    typedef struct _XCENTERGLOBALS
    {
        HAB                 hab;
                    // anchor block of frame and client (constant)
        HWND                hwndFrame,
                    // XCenter frame window (constant)
                            hwndClient;
                    // client (child of XCenter frame) (constant)

        PVOID               pCountrySettings;
                    // country settings; this points to a COUNTRYSETTINGS
                    // structure (prfh.h) (constant)

        ULONG               cyInnerClient;
                    // height of inner client (same as height of all widgets!).
                    // This is normally == cyWidgetmax unless the user has
                    // resized the XCenter, but it will always be >= cyWidgetMax.
                    // The "inner client" plus the 3D border width plus the
                    // border spacing make up the full height of the client.

                    // This can change while the XCenter is open.
                    // This was changed with V0.9.9 (2001-03-09) [umoeller];
                    // this field still always has the client height, but this
                    // is no longer neccessarily the same as the tallest
                    // widget (see cyWidgetMax below).

        ULONG               cxMiniIcon;
                    // system mini-icon size (for convenience); either 16 or 20
                    // (constant)

        LONG                lcol3DDark,
                            lcol3DLight;
                    // system colors for 3D frames (for convenience; RGB!) (constant)

        // the following are the width settings from the second "View"
        // settings page;
        // a widget may or may not want to consider these.
        ULONG               flDisplayStyle;
                    // XCenter display style flags;
                    // a widget may or may not want to consider these.
                    // Can be changed by the user while the XCenter is open.
                    // These flags can be any combination of the following:
                    // -- XCS_FLATBUTTONS: paint buttons flat. If not set,
                    //      paint them raised.
                    // -- XCS_SUNKBORDERS: paint static controls (e.g. CPU meter)
                    //      with a "sunk" 3D frame. If not set, do not.
                    // -- XCS_SIZINGBARS: XCenter should automatically paint
                    //      sizing bars for sizeable widgets.
                    // -- XCS_ALL3DBORDERS: XCenter should draw all four 3D
                    //      borders around itself. If not set, it will only
                    //      draw one border (the one towards the screen).

        ULONG               ulPosition;
                    // XCenter position on screen, if a widget cares...
                    // Can be changed by the user while the XCenter is open.
                    // This is _one_ of the following:
                    // -- XCENTER_BOTTOM
                    // -- XCENTER_TOP
                    // Left and right are not yet supported.

        ULONG               ul3DBorderWidth;
                    // 3D border width; can change
        ULONG               ulBorderSpacing;
                    // border spacing (added to 3D border width); can change
        ULONG               ulSpacing;
                    // spacing between widgets; can change

        /*
         *      The following fields have been added with
         *      releases later than V0.9.7. Since they have
         *      been added at the bottom, the structure is
         *      still backward-compatible with old plugin
         *      binaries... but you cannot use these new
         *      fields unless you also require the corresponding
         *      XCenter revision by using the "version" export @3.
         */

        PVOID               pvXTimerSet;
                    // XCenter timer set, which can be used by widgets
                    // to start XTimers instead of regular PM timers.
                    // This was added V0.9.9 (2001-03-07) [umoeller].
                    // See src\helpers\timer.c for an introduction.
                    // This pointer is really an XTIMERSET pointer but
                    // has been declared as PVOID to avoid having to
                    // #include include\helpers\timer.h all the time.
                    // This is constant while the XCenter is open.

        ULONG               cyWidgetMax;
                    // height of tallest widget == minimum height of client
                    // V0.9.9 (2001-03-09) [umoeller]
                    // This may change while the XCenter is open.

    } XCENTERGLOBALS, *PXCENTERGLOBALS;

    // forward declaration
    typedef struct _XCENTERWIDGET *PXCENTERWIDGET;

    BOOL APIENTRY ctrSetSetupString(LHANDLE hSetting, const char *pcszNewSetupString);
    typedef BOOL APIENTRY CTRSETSETUPSTRING(LHANDLE hSetting, const char *pcszNewSetupString);
    typedef CTRSETSETUPSTRING *PCTRSETSETUPSTRING;

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
     *          The address of the ctrSetSetupString helper is
     *          given to you in this structure so that you
     *          won't have to import it from XFLDR.DLL.
     *
     *      If a widget class supports settings dialogs,
     *      it must specify this in its XCENTERWIDGETCLASS.
     *
     *@@added V0.9.7 (2000-12-07) [umoeller]
     *@@changed V0.9.9 (2001-02-06) [umoeller]: added pctrSetSetupString
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

        /*
         *      The following fields have been added with
         *      releases later than V0.9.7. Since they have
         *      been added at the bottom, the structure is
         *      still backward-compatible with old plugin
         *      binaries... but you cannot use these new
         *      fields unless you also require the corresponding
         *      XCenter revision by using the "version" export @3.
         */

        PCTRSETSETUPSTRING      pctrSetSetupString;
                    // ptr to ctrSetSetupString function in
                    // src\shared\center.c; this must be
                    // called by the settings dialog to
                    // change the widget's setup string.
                    // This pointer has been added with V0.9.9
                    // to allow using settings dialog without
                    // having to import this from XFLDR.DLL.

    } WIDGETSETTINGSDLGDATA, *PWIDGETSETTINGSDLGDATA;

    typedef VOID EXPENTRY WGTSHOWSETTINGSDLG(PWIDGETSETTINGSDLGDATA);
    typedef WGTSHOWSETTINGSDLG *PWGTSHOWSETTINGSDLG;

    // widget class flags
    #define WGTF_SIZEABLE               0x0001
    #define WGTF_NOUSERCREATE           0x0002
    #define WGTF_UNIQUEPERXCENTER       0x0004
    #define WGTF_UNIQUEGLOBAL          (0x0008 + 0x0004)
    #define WGTF_TOOLTIP                0x0010
    #define WGTF_TOOLTIP_AT_MOUSE      (0x0020 + 0x0010)
    #define WGTF_TRANSPARENT            0x0040
    #define WGTF_CONTAINER              0x0080
    #define WGTF_NONFOCUSTRAVERSABLE    0x0100

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
                // the class. This must be unique on the system and should
                // not contain any spaces (e.g.: "MySampleClass").

        const char      *pcszClassTitle;
                // widget class title (shown to user in "Add widget" popup
                // menu). Example: "Sample widget".

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
                // -- WGTF_TOOLTIP_AT_MOUSE: like WGTF_TOOPTIP, but the
                //    tooltip is not centered above the widget, but put
                //    at the mouse position instead.
                //    This includes WGTF_TOOLTIP.

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
                // NOTE: If your widget is "greedy", i.e. wants
                // all remaining size on the XCenter bar (by
                // returning -1 for cx with WM_CONTROL and
                //

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

    // ctrSetSetupString has been defined above

    BOOL APIENTRY ctrDisplayHelp(PXCENTERGLOBALS pGlobals,
                        const char *pcszHelpFile,
                        ULONG ulHelpPanelID);
    typedef BOOL APIENTRY CTRDISPLAYHELP(PXCENTERGLOBALS pGlobals,
                        const char *pcszHelpFile,
                        ULONG ulHelpPanelID);
    typedef CTRDISPLAYHELP *PCTRDISPLAYHELP;

    VOID APIENTRY ctrShowContextMenu(PXCENTERWIDGET pWidget, HWND hwndContextMenu);
    typedef VOID APIENTRY CTRSHOWCONTEXTMENU(PXCENTERWIDGET pWidget, HWND hwndContextMenu);
    typedef CTRSHOWCONTEXTMENU *PCTRSHOWCONTEXTMENU;

    VOID APIENTRY ctrPaintStaticWidgetBorder(HPS hps, PXCENTERWIDGET pWidget);
    typedef VOID APIENTRY CTRPAINTSTATICWIDGETBORDER(HPS hps, PXCENTERWIDGET pWidget);
    typedef CTRPAINTSTATICWIDGETBORDER *PCTRPAINTSTATICWIDGETBORDER;

    MRESULT EXPENTRY ctrDefWidgetProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
                // a pointer to this is in XCENTERWIDGET if the widget
                // is a non-container widget

    MRESULT EXPENTRY ctrDefContainerWidgetProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
                // a pointer to this is in XCENTERWIDGET if the widget
                // is a container widget

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
     *      As a special case, a widget can put -1 into the
     *      cx field of the SIZEL structure. It will then
     *      receive all remaining space on the XCenter bar
     *      ("greedy" widgets). This is what the window list
     *      widget does, for example.
     *
     *      If several widgets request to be "greedy", the
     *      remaining space on the XCenter bar is evenly
     *      distributed among the greedy widgets.
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

    /*
     *@@ XN_QUERYSETUP:
     *      notification code for WM_CONTROL sent to a widget
     *      when the sender needs to know the widget's setup string.
     *
     *      Parameters:
     *
     *      -- SHORT1FROMMP(mp1): ID, always ID_XCENTER_CLIENT.
     *
     *      -- SHORT2FROMMP(mp1): notify code (XN_QUERYSETUP).
     *
     *      -- char *mp2: buffer into which the setup string is
     *                    copied.  It can be NULL, in which case
     *                    nothing is copied.  Otherwise, it is
     *                    expected to contain enough room for the
     *                    whole setup string.
     *
     *      The widget must return the minimum required size needed
     *      to store the setup string (even if mp2 is NULL).
     *
     *@@added V0.9.9 (2001-03-01) [lafaix]
     */

    #define XN_QUERYSETUP               4

    // flags for XN_BEGINANIMATE and XN_ENDANIMATE
    #define XAF_SHOW                    1
    #define XAF_HIDE                    2

    /*
     *@@ XN_BEGINANIMATE:
     *      notification code for WM_CONTROL sent to a widget from
     *      an XCenter when it is about to begin animating.
     *
     *      Parameters:
     *
     *      -- SHORT1FROMMP(mp1): ID, always ID_XCENTER_CLIENT.
     *
     *      -- SHORT2FROMMP(mp1): notify code (XN_BEGINANIMATE).
     *
     *      -- ULONG mp2: XAF_SHOW if the parent initiate a 'show' animation,
     *                    XAF_HIDE if the parent initiate a 'hide' animation.
     *
     *      This notification is sent regardless of whether the XCenter
     *      actually does animation.  If it does no animation,
     *      XN_ENDANIMATE immediately follows XN_BEGINANIMATE.
     *
     *      An active widget can react to this message to start or
     *      stop doing something.  For example, a gauge widget can
     *      choose to stop running when the container is hidden, to
     *      save CPU cycles.
     *
     *@added V0.9.9 (2001-03-01) [lafaix]
     */

    #define XN_BEGINANIMATE             5

    /*
     *@@ XN_ENDANIMATE:
     *      notification code for WM_CONTROL sent to a widget from
     *      an XCenter when it has ended animating.
     *
     *      Parameters:
     *
     *      -- SHORT1FROMMP(mp1): ID, always ID_XCENTER_CLIENT.
     *
     *      -- SHORT2FROMMP(mp1): notify code (XN_ENDANIMATE).
     *
     *      -- ULONG mp2: XAF_SHOW if the parent ended a 'show' animation,
     *                    XAF_HIDE if the parent ended a 'hide' animation.
     *
     *      This notification is sent regardless of whether the XCenter
     *      actually does animation.  If it does no animation,
     *      XN_ENDANIMATE immediately follows XN_BEGINANIMATE.
     *
     *      An active widget can react to this message to start or
     *      stop doing something.  For example, a gauge widget can
     *      choose to stop running when the container is hidden, to
     *      save CPU cycles.
     *
     *@added V0.9.9 (2001-03-01) [lafaix]
     */

    #define XN_ENDANIMATE               6

    /*
     *@@ XN_QUERYWIDGETCOUNT:
     *      notification code for WM_CONTROL sent from the XCenter
     *      to a widget when it needs to know how many elements a
     *      container-widget contains.
     *
     *      Parameters:
     *
     *      -- SHORT1FROMMP(mp1): ID, always ID_XCENTER_CLIENT.
     *
     *      -- SHORT2FROMMP(mp1): notify code (XN_QUERYWIDGETCOUNT).
     *
     *      -- mp2: reserved, must be 0.
     *
     *      The widgets count must only include first level elements.
     *      That is, if a container contains other containers, the
     *      elements in those sub-containers should not be included.
     *
     *      The widget must return TRUE if it has put its count in the
     *      ULONG.  Otherwise, the XCenter will assume some dumb default
     *      for the count.
     *
     *@@added V0.9.9 (2001-02-23) [lafaix]
     */

    #define XN_QUERYWIDGETCOUNT         7

    /*
     *@@ XN_QUERYWIDGET:
     *      notification code for WM_CONTROL sent from the XCenter
     *      to a widget when it needs to know the widget present at
     *      a given position.
     *
     *      Parameters:
     *
     *      -- SHORT1FROMMP(mp1): ID, always ID_XCENTER_CLIENT.
     *
     *      -- SHORT2FROMMP(mp1): notify code (XN_QUERYWIDGET).
     *
     *      -- ULONG mp2: widget index (0 is the first widget).
     *
     *      The widget must return 0 if no widget exists at that index.
     *      Otherwise it must return a pointer to the corresponding
     *      XCENTERWIDGET structure.
     *
     *@@added V0.9.9 (2001-02-23) [lafaix]
     */

    #define XN_QUERYWIDGET              8

    // structure needed for XN_INSERTWIDGET
    typedef struct _WIDGETINFO
    {
        SHORT          sOffset;
                   // either WGT_END or the 0-based offset
        PXCENTERWIDGET pWidget;
                   // the widget to be inserted
    } WIDGETINFO, *PWIDGETINFO;

    // flags and return values for XN_INSERTWIDGET:
    #define WGT_END                     (-1)
    #define WGT_ERROR                   (-1)

   /*
     *@@ XN_INSERTWIDGET:
     *      notification code for WM_CONTROL sent from the XCenter
     *      to a widget when it needs to add a widget at a specified
     *      offset to a container-widget.
     *
     *      Parameters:
     *
     *      -- SHORT1FROMMP(mp1): ID, always ID_XCENTER_CLIENT.
     *
     *      -- SHORT2FROMMP(mp1): notify code (XN_INSERTWIDGET).
     *
     *      -- PWIDGETINFO mp2: a pointer to a WIDGETINFO structure
     *                          that details the insertion.
     *
     *      The widget must return WGT_ERROR if the insertion failed.
     *      Otherwise it must return the inserted widget's offset.
     *
     *@@added V0.9.9 (2001-02-23) [lafaix]
     */

    #define XN_INSERTWIDGET             9

    /*
     *@@ XN_DELETEWIDGET:
     *      notification code for WM_CONTROL sent from the XCenter
     *      to a widget when it needs to remove a widget at a specified
     *      offset.
     *
     *      Parameters:
     *
     *      -- SHORT1FROMMP(mp1): ID, always ID_XCENTER_CLIENT.
     *
     *      -- SHORT2FROMMP(mp1): notify code (XN_INSERTWIDGET).
     *
     *      -- SHORT mp2: the to be removed widget's offset.
     *
     *      The widget must return the count of remaining widgets.
     *
     *@@added V0.9.9 (2001-02-23) [lafaix]
     */

    #define XN_DELETEWIDGET             10

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

    // init-module export (ordinal 1)
    // WARNING: THIS PROTOTYPE HAS CHANGED WITH V0.9.9
    // IF QUERY-VERSION IS EXPORTED (@3, see below) THE NEW
    // PROTOTYPE IS USED
    typedef ULONG EXPENTRY FNWGTINITMODULE_OLD(HAB hab,
                                               HMODULE hmodXFLDR,
                                               PXCENTERWIDGETCLASS *ppaClasses,
                                               PSZ pszErrorMsg);
    typedef FNWGTINITMODULE_OLD *PFNWGTINITMODULE_OLD;

    typedef ULONG EXPENTRY FNWGTINITMODULE_099(HAB hab,
                                               HMODULE hmodPlugin,
                                               HMODULE hmodXFLDR,
                                               PXCENTERWIDGETCLASS *ppaClasses,
                                               PSZ pszErrorMsg);
    typedef FNWGTINITMODULE_099 *PFNWGTINITMODULE_099;

    // un-init-module export (ordinal 2)
    typedef VOID EXPENTRY FNWGTUNINITMODULE(VOID);
    typedef FNWGTUNINITMODULE *PFNWGTUNINITMODULE;

    // query version (ordinal 3; added V0.9.9 (2001-02-01) [umoeller])
    // IF QUERY-VERSION IS EXPORTED,THE NEW PROTOTYPE FOR
    // INIT_MODULE (above) WILL BE USED
    typedef VOID EXPENTRY FNWGTQUERYVERSION(PULONG pulMajor,
                                            PULONG pulMinor,
                                            PULONG pulRevision);
    typedef FNWGTQUERYVERSION *PFNWGTQUERYVERSION;
#endif

