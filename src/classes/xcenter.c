
/*
 *@@sourcefile xcenter.c:
 *      This file contains SOM code for the following XWorkplace classes:
 *
 *      --  XCenter: a WarpCenter replacement.
 *
 *      This class is fairly functional with V0.9.7 now.
 *      See src\shared\center.c for an introduction.
 *
 *      Installation of this class is completely optional.
 *
 *@@added V0.9.4 (2000-06-11) [umoeller]
 *@@somclass XCenter xctr_
 *@@somclass M_XCenter xctrM_
 */

/*
 *      Copyright (C) 2000 Ulrich M�ller.
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

/*
 *@@todo:
 *
 */

/*
 *  This file was generated by the SOM Compiler and Emitter Framework.
 *  Generated using:
 *      SOM Emitter emitctm: 2.41
 */

#ifndef SOM_Module_xcenter_Source
#define SOM_Module_xcenter_Source
#endif
#define XCenter_Class_Source
#define M_XCenter_Class_Source

/*
 *  Suggested #include order:
 *  1)  os2.h
 *  2)  C library headers
 *  3)  setup.h (code generation and debugging options)
 *  4)  headers in helpers\
 *  5)  at least one SOM implementation header (*.ih)
 *  6)  dlgids.h, headers in shared\ (as needed)
 *  7)  headers in implementation dirs (e.g. filesys\, as needed)
 *  8)  #pragma hdrstop and then more SOM headers which crash with precompiled headers
 */

#define INCL_DOSPROCESS
#define INCL_DOSEXCEPTIONS
#define INCL_DOSSEMAPHORES

#define INCL_WINWINDOWMGR
#define INCL_WINMESSAGEMGR
#define INCL_WINMENUS           // for menu helpers
#define INCL_WINDIALOGS
#define INCL_WINBUTTONS         // for button/check box helpers
#include <os2.h>

// C library headers
#include <stdio.h>              // needed for except.h
#include <setjmp.h>

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\except.h"             // exception handling
#include "helpers\winh.h"               // PM helper routines

// SOM headers which don't crash with prec. header files
#include "xcenter.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling

#include "shared\center.h"              // public XCenter interfaces
#include "xcenter\centerp.h"            // private XCenter implementation

// other SOM headers

#pragma hdrstop

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

const char *G_pcszXCenter = "XCenter";

/* ******************************************************************
 *
 *   XCenter instance methods
 *
 ********************************************************************/

/*
 *@@ xwpAddXCenterPages:
 *
 */

SOM_Scope ULONG  SOMLINK xctr_xwpAddXCenterPages(XCenter *somSelf,
                                                 HWND hwndNotebook)
{
    PCREATENOTEBOOKPAGE pcnbp;
    PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();

    /* XCenterData *somThis = XCenterGetData(somSelf); */
    XCenterMethodDebug("XCenter","xctr_xwpAddXCenterPages");

    // add the "Widgets" page
    pcnbp = malloc(sizeof(CREATENOTEBOOKPAGE));
    memset(pcnbp, 0, sizeof(CREATENOTEBOOKPAGE));
    pcnbp->somSelf = somSelf;
    pcnbp->hwndNotebook = hwndNotebook;
    pcnbp->hmod = cmnQueryNLSModuleHandle(FALSE);
    pcnbp->usPageStyleFlags = BKA_MAJOR;
    pcnbp->pszName = "Wi~dgets";   // ###
    pcnbp->ulDlgID = ID_XFD_CONTAINERPAGE;
    pcnbp->ulDefaultHelpPanel  = ID_XSH_XCENTER_WIDGETS;
    pcnbp->ulPageID = SP_XCENTER_WIDGETS;
    pcnbp->pampControlFlags = G_pampGenericCnrPage;
    pcnbp->cControlFlags = G_cGenericCnrPage;
    pcnbp->pfncbInitPage    = ctrpWidgetsInitPage;
    pcnbp->pfncbItemChanged = ctrpWidgetsItemChanged;
    ntbInsertPage(pcnbp);

    // add the "View" page on top
    pcnbp = malloc(sizeof(CREATENOTEBOOKPAGE));
    memset(pcnbp, 0, sizeof(CREATENOTEBOOKPAGE));
    pcnbp->somSelf = somSelf;
    pcnbp->hwndNotebook = hwndNotebook;
    pcnbp->hmod = cmnQueryNLSModuleHandle(FALSE);
    pcnbp->usPageStyleFlags = BKA_MAJOR;
    pcnbp->pszName = pNLSStrings->pszViewPage;
    pcnbp->ulDlgID = ID_CRD_SETTINGS_VIEW;
    pcnbp->ulDefaultHelpPanel  = ID_XSH_XCENTER_VIEW;
    pcnbp->ulPageID = SP_XCENTER_VIEW;
    pcnbp->pfncbInitPage    = ctrpViewInitPage;
    pcnbp->pfncbItemChanged = ctrpViewItemChanged;
    return (ntbInsertPage(pcnbp));
}

/*
 *@@ xwpQueryWidgets:
 *      returns an array of XWPWIDGETSETTING structures
 *      describing the widgets in this XCenter instance.
 *
 *      *pulCount receives the array item count (_not_
 *      the array size). Use XCenter::xwpFreeWidgetsBuf
 *      to free the memory allocated by this method.
 *
 *      If no widgets are present, NULL is returned, and
 *      *pulCount is set to 0.
 *
 *@@added V0.9.7 (2000-12-08) [umoeller]
 */

SOM_Scope PVOID  SOMLINK xctr_xwpQueryWidgets(XCenter *somSelf,
                                              PULONG pulCount)
{
    // XCenterData *somThis = XCenterGetData(somSelf);
    XCenterMethodDebug("XCenter","xctr_xwpQueryWidgets");

    return (ctrpQueryWidgets(somSelf, pulCount));
}

/*
 *@@ xwpFreeWidgetsBuf:
 *      frees the widget buffer allocated by
 *      XCenter::xwpQueryWidgets. pBuf must
 *      be the value returned from there
 *      and ulCount must be the value of
 *      *pulCount from there.
 *
 *@@added V0.9.7 (2000-12-08) [umoeller]
 */

SOM_Scope void  SOMLINK xctr_xwpFreeWidgetsBuf(XCenter *somSelf,
                                               PVOID pBuf, ULONG ulCount)
{
    // XCenterData *somThis = XCenterGetData(somSelf);
    XCenterMethodDebug("XCenter","xctr_xwpFreeWidgetsBuf");

    ctrpFreeWidgetsBuf(pBuf, ulCount);
}

/*
 *@@ xwpInsertWidget:
 *      inserts a new widget into an XCenter at the
 *      specified index.
 *
 *      If (ulBeforeIndex == -1), we insert the new
 *      widget as the last widget.
 *
 *      An open view of this XCenter is automatically
 *      updated.
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xctr_xwpInsertWidget(XCenter *somSelf,
                                             ULONG ulBeforeIndex,
                                             PSZ pszWidgetClass,
                                             PSZ pszSetupString)
{
    BOOL brc = FALSE;
    XCenterMethodDebug("XCenter","xctr_xwpInsertWidget");

    TRY_LOUD(excpt1)
    {
        _Pmpf((__FUNCTION__ ": calling ctrInsertWidget with %s, %s",
                (pszWidgetClass) ? pszWidgetClass : "NULL",
                (pszSetupString) ? pszSetupString : "NULL"));
        brc = ctrpInsertWidget(somSelf,
                               ulBeforeIndex,
                               pszWidgetClass,
                               pszSetupString);
        _Pmpf((__FUNCTION__ ": got %d from ctrInsertWidget", brc));
    }
    CATCH(excpt1)
    {
        brc = FALSE;
    } END_CATCH();

    return (brc);
}

/*
 *@@ xwpRemoveWidget:
 *      removes the widget at the position specified
 *      with ulIndex (with 0 being the leftmost widget).
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xctr_xwpRemoveWidget(XCenter *somSelf,
                                             ULONG ulIndex)
{
    BOOL brc = FALSE;
    XCenterData *somThis = XCenterGetData(somSelf);
    XCenterMethodDebug("XCenter","xctr_xwpRemoveWidget");

    return (ctrpRemoveWidget(somSelf, ulIndex));
}

/*
 *@@ xwpQuerySetup2:
 *      this XFldObject method is overridden to support
 *      setup strings for folders.
 *
 *      See XFldObject::xwpQuerySetup2 for details.
 *
 *V0.9.7 (2000-12-09) [umoeller]
 */

SOM_Scope ULONG  SOMLINK xctr_xwpQuerySetup2(XCenter *somSelf,
                                             PSZ pszSetupString,
                                             ULONG cbSetupString)
{
    // XCenterData *somThis = XCenterGetData(somSelf);
    XCenterMethodDebug("XCenter","xctr_xwpQuerySetup2");

    // call implementation
    return (ctrpQuerySetup(somSelf, pszSetupString, cbSetupString));
}

/*
 *@@ wpInitData:
 *      this WPObject instance method gets called when the
 *      object is being initialized (on wake-up or creation).
 *      We initialize our additional instance data here.
 *      Always call the parent first.
 */

SOM_Scope void  SOMLINK xctr_wpInitData(XCenter *somSelf)
{
    XCenterData *somThis = XCenterGetData(somSelf);
    XCenterMethodDebug("XCenter","xctr_wpInitData");

    XCenter_parent_WPAbstract_wpInitData(somSelf);

    _ulWindowStyle = 0; // WS_TOPMOST | WS_ANIMATE;
    _ulAutoHide = 0; // 4000;

    _ulPosition = XCENTER_BOTTOM;

    _hwndOpenView = NULLHANDLE;

    _fShowingOpenViewMenu = FALSE;

    _pszPackedWidgetSettings = NULL;
    _cbPackedWidgetSettings = 0;

    _pllWidgetSettings = NULL;
}

/*
 *@@ wpUnInitData:
 *      this WPObject instance method is called when the object
 *      is destroyed as a SOM object, either because it's being
 *      made dormant or being deleted. All allocated resources
 *      should be freed here.
 *      The parent method must always be called last.
 */

SOM_Scope void  SOMLINK xctr_wpUnInitData(XCenter *somSelf)
{
    XCenterData *somThis = XCenterGetData(somSelf);
    XCenterMethodDebug("XCenter","xctr_wpUnInitData");

    ctrpFreeWidgets(somSelf);

    if (_pszPackedWidgetSettings)
    {
        free(_pszPackedWidgetSettings);
        _pszPackedWidgetSettings = NULL;
    }

    XCenter_parent_WPAbstract_wpUnInitData(somSelf);
}

/*
 *@@ wpObjectReady:
 *      this WPObject notification method gets called by the
 *      WPS when object instantiation is complete, for any reason.
 *      ulCode and refObject signify why and where from the
 *      object was created.
 *      The parent method must be called first.
 *
 *      Even though WPSREF doesn't really say so, this method
 *      must be used similar to a C++ copy constructor
 *      when the instance data contains pointers. Since when
 *      objects are copied, SOM just copies the binary instance
 *      data, you get two objects with instance pointers pointing
 *      to the same object, which can only lead to problems.
 */

SOM_Scope void  SOMLINK xctr_wpObjectReady(XCenter *somSelf,
                                           ULONG ulCode, WPObject* refObject)
{
    /* XCenterData *somThis = XCenterGetData(somSelf); */
    XCenterMethodDebug("XCenter","xctr_wpObjectReady");

    XCenter_parent_WPAbstract_wpObjectReady(somSelf, ulCode,
                                            refObject);
}

/*
 *@@ wpSaveState:
 *      this WPObject instance method saves an object's state
 *      persistently so that it can later be re-initialized
 *      with wpRestoreState. This gets called during wpClose,
 *      wpSaveImmediate or wpSaveDeferred processing.
 *      All persistent instance variables should be stored here.
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xctr_wpSaveState(XCenter *somSelf)
{
    BOOL brc = FALSE;
    XCenterData *somThis = XCenterGetData(somSelf);
    XCenterMethodDebug("XCenter","xctr_wpSaveState");

    brc = XCenter_parent_WPAbstract_wpSaveState(somSelf);

    TRY_LOUD(excpt1)
    {
        if (brc)
        {
            /*
             * key 1: widget settings
             *
             */

            if (_pszPackedWidgetSettings)
                // settings haven't even been unpacked yet:
                // just store the packed settings
                _wpSaveData(somSelf,
                            (PSZ)G_pcszXCenter,
                            1,
                            _pszPackedWidgetSettings,
                            _cbPackedWidgetSettings);
            else
                // once the settings have been unpacked
                // (i.e. XCenter needed access to them),
                // we have to repack them on each save
                if (_pllWidgetSettings)
                {
                    // compose array
                    ULONG cbSettingsArray = 0;
                    PSZ pszSettingsArray = ctrpStuffSettings(somSelf,
                                                             &cbSettingsArray);
                    if (pszSettingsArray)
                    {
                        _wpSaveData(somSelf,
                                    (PSZ)G_pcszXCenter,
                                    1,
                                    pszSettingsArray,
                                    cbSettingsArray);
                        free(pszSettingsArray);
                    }
                }

            /*
             * other keys
             *
             */

            _wpSaveLong(somSelf,
                        (PSZ)G_pcszXCenter,
                        2,
                        _ulWindowStyle);

            _wpSaveLong(somSelf,
                        (PSZ)G_pcszXCenter,
                        3,
                        _ulAutoHide);
        }
    }
    CATCH(excpt1)
    {
        brc = FALSE;
    } END_CATCH();

    return (brc);
}

/*
 *@@ wpRestoreState:
 *      this WPObject instance method gets called during object
 *      initialization (after wpInitData) to restore the data
 *      which was stored with wpSaveState.
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xctr_wpRestoreState(XCenter *somSelf,
                                            ULONG ulReserved)
{
    BOOL brc = FALSE;
    XCenterData *somThis = XCenterGetData(somSelf);
    XCenterMethodDebug("XCenter","xctr_wpRestoreState");

    brc = XCenter_parent_WPAbstract_wpRestoreState(somSelf,
                                                   ulReserved);

    TRY_LOUD(excpt1)
    {
        if (brc)
        {
            /*
             * key 1: widget settings
             *
             */

            BOOL    fError = FALSE;
            ULONG   ul;

            if (_pszPackedWidgetSettings)
            {
                free(_pszPackedWidgetSettings);
                _pszPackedWidgetSettings = 0;
            }

            _cbPackedWidgetSettings = 0;
            // get size of array
            if (_wpRestoreData(somSelf,
                               (PSZ)G_pcszXCenter,
                               1,
                               NULL,    // query size
                               &_cbPackedWidgetSettings))
            {
                _pszPackedWidgetSettings = (PSZ)malloc(_cbPackedWidgetSettings);
                if (_pszPackedWidgetSettings)
                {
                    if (!_wpRestoreData(somSelf,
                                       (PSZ)G_pcszXCenter,
                                       1,
                                       _pszPackedWidgetSettings,
                                       &_cbPackedWidgetSettings))
                        // error:
                        fError = TRUE;
                }
                else
                    fError = TRUE;
            }
            else
                // error:
                fError = TRUE;

            if (fError)
            {
                if (_pszPackedWidgetSettings)
                    free(_pszPackedWidgetSettings);
                _pszPackedWidgetSettings = NULL;
                _cbPackedWidgetSettings = 0;
            }

            /*
             * other keys
             *
             */

            if (_wpRestoreLong(somSelf,
                               (PSZ)G_pcszXCenter,
                               2,
                               &ul))
                _ulWindowStyle = ul;

            if (_wpRestoreLong(somSelf,
                               (PSZ)G_pcszXCenter,
                               3,
                               &ul))
                _ulAutoHide = ul;
        }
    }
    CATCH(excpt1)
    {
        brc = FALSE;
    } END_CATCH();

    return (brc);
}

/*
 *@@ wpFilterPopupMenu:
 *      this WPObject instance method allows the object to
 *      filter out unwanted menu items from the context menu.
 *      This gets called before wpModifyPopupMenu.
 */

SOM_Scope ULONG  SOMLINK xctr_wpFilterPopupMenu(XCenter *somSelf,
                                                ULONG ulFlags,
                                                HWND hwndCnr,
                                                BOOL fMultiSelect)
{
    /* XCenterData *somThis = XCenterGetData(somSelf); */
    XCenterMethodDebug("XCenter","xctr_wpFilterPopupMenu");

    return (XCenter_parent_WPAbstract_wpFilterPopupMenu(somSelf,
                                                        ulFlags,
                                                        hwndCnr,
                                                        fMultiSelect));
}

/*
 *@@ wpModifyPopupMenu:
 *      this WPObject instance methods gets called by the WPS
 *      when a context menu needs to be built for the object
 *      and allows the object to manipulate its context menu.
 *      This gets called _after_ wpFilterPopupMenu.
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xctr_wpModifyPopupMenu(XCenter *somSelf,
                                               HWND hwndMenu,
                                               HWND hwndCnr,
                                               ULONG iPosition)
{
    BOOL brc;
    XCenterData *somThis = XCenterGetData(somSelf);
    XCenterMethodDebug("XCenter","xctr_wpModifyPopupMenu");

    brc = XCenter_parent_WPAbstract_wpModifyPopupMenu(somSelf,
                                                      hwndMenu,
                                                      hwndCnr,
                                                      iPosition);
    if (brc)
        brc = ctrpModifyPopupMenu(somSelf, hwndMenu);

    return (brc);
}

/*
 *@@ wpMenuItemSelected:
 *      this WPObject method processes menu selections.
 *      This must be overridden to support new menu
 *      items which have been added in wpModifyPopupMenu.
 */

SOM_Scope BOOL  SOMLINK xctr_wpMenuItemSelected(XCenter *somSelf,
                                                HWND hwndFrame,
                                                ULONG ulMenuId)
{
    /* XCenterData *somThis = XCenterGetData(somSelf); */
    XCenterMethodDebug("XCenter","xctr_wpMenuItemSelected");

    return (XCenter_parent_WPAbstract_wpMenuItemSelected(somSelf,
                                                         hwndFrame,
                                                         ulMenuId));
}

/*
 *@@ wpMenuItemHelpSelected:
 *
 */

SOM_Scope BOOL  SOMLINK xctr_wpMenuItemHelpSelected(XCenter *somSelf,
                                                    ULONG MenuId)
{
    /* XCenterData *somThis = XCenterGetData(somSelf); */
    XCenterMethodDebug("XCenter","xctr_wpMenuItemHelpSelected");

    return (XCenter_parent_WPAbstract_wpMenuItemHelpSelected(somSelf,
                                                             MenuId));
}

/*
 *@@ wpQueryDefaultHelp:
 *      this WPObject instance method specifies the default
 *      help panel for an object (when "Extended help" is
 *      selected from the object's context menu). This should
 *      describe what this object can do in general.
 *      We must return TRUE to report successful completion.
 */

SOM_Scope BOOL  SOMLINK xctr_wpQueryDefaultHelp(XCenter *somSelf,
                                                PULONG pHelpPanelId,
                                                PSZ HelpLibrary)
{
    /* XCenterData *somThis = XCenterGetData(somSelf); */
    XCenterMethodDebug("XCenter","xctr_wpQueryDefaultHelp");

    strcpy(HelpLibrary, cmnQueryHelpLibrary());
    *pHelpPanelId = ID_XSH_XCENTER_MAIN;
    return (TRUE);
}

/*
 *@@ wpQueryDefaultView:
 *      this WPObject method returns the default view of an object,
 *      that is, which view is opened if the program file is
 *      double-clicked upon. This is also used to mark
 *      the default view in the "Open" context submenu.
 *
 *      This must be overridden for direct WPAbstract subclasses,
 *      because otherwise double-clicks on the object won't
 *      work.
 */

SOM_Scope ULONG  SOMLINK xctr_wpQueryDefaultView(XCenter *somSelf)
{
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    /* XCenterData *somThis = XCenterGetData(somSelf); */
    XCenterMethodDebug("XCenter","xctr_wpQueryDefaultView");

    return (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_XCENTER);
}

/*
 *@@ wpOpen:
 *      this WPObject instance method gets called when
 *      a new view needs to be opened. Normally, this
 *      gets called after wpViewObject has scanned the
 *      object's USEITEMs and has determined that a new
 *      view is needed.
 *
 *      This _normally_ runs on thread 1 of the WPS, but
 *      this is not always the case. If this gets called
 *      in response to a menu selection from the "Open"
 *      submenu or a double-click in the folder, this runs
 *      on the thread of the folder (which _normally_ is
 *      thread 1). However, if this results from WinOpenObject
 *      or an OPEN setup string, this will not be on thread 1.
 *
 *      We open an XCenter view here by calling ctrCreateXCenterView.
 */

SOM_Scope HWND  SOMLINK xctr_wpOpen(XCenter *somSelf,
                                    HWND hwndCnr,
                                    ULONG ulView,
                                    ULONG param)
{
    HWND    hwndNewView = 0;
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    XCenterData *somThis = XCenterGetData(somSelf);
    XCenterMethodDebug("XCenter","xctr_wpOpen");

    if (ulView == (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_XCENTER))
    {
        if (!_hwndOpenView)
        {
            // no open view yet (just make sure!)
            HAB hab;
            if (hwndCnr)
                hab = WinQueryAnchorBlock(hwndCnr);
            else
                hab = WinQueryAnchorBlock(cmnQueryActiveDesktopHWND());
            hwndNewView = ctrpCreateXCenterView(somSelf,
                                                hab,
                                                ulView);
            // store in instance data
            _hwndOpenView = hwndNewView;
        }
    }
    else
        // other view (probably settings):
        hwndNewView = XCenter_parent_WPAbstract_wpOpen(somSelf,
                                                       hwndCnr,
                                                       ulView,
                                                       param);
    return (hwndNewView);
}

/*
 *@@ wpSwitchTo:
 *      this WPObject method is called to give focus
 *      to an already open view. This gets called
 *      from wpViewObject instead of wpOpen if a view
 *      already exists, but can be called separately
 *      as well.
 *
 *      For the XCenter, we'll give focus to the
 *      XCenter view... however, if auto-hide is
 *      enabled, we'll also have to re-show the
 *      frame.
 *
 *@@added V0.9.7 (2000-12-04) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xctr_wpSwitchTo(XCenter *somSelf, ULONG View)
{
    BOOL brc = FALSE;
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    XCenterData *somThis = XCenterGetData(somSelf);
    XCenterMethodDebug("XCenter","xctr_wpSwitchTo");

    // check if we should switch to the existing XCenter view
    if (View == (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_XCENTER))
    {
        // yes:
        PUSEITEM    pUseItem = NULL;
        for (pUseItem = _wpFindUseItem(somSelf, USAGE_OPENVIEW, NULL);
             pUseItem;
             pUseItem = _wpFindUseItem(somSelf, USAGE_OPENVIEW, pUseItem))
        {
            PVIEWITEM pViewItem = (PVIEWITEM)(pUseItem+1);
            if (pViewItem->view == View)
            {
                // yes, it's an XCenter view:
                // instead of activating the view (which is what
                // the WPS normally does), show the frame and
                // restart the update timer
                // DO NOT GIVE FOCUS, DO NOT ACTIVATE
                ctrpReformatFrameHWND(pViewItem->handle);
                brc = TRUE;
                break;
            }
        }
    }
    else
        // view other than XCenter view (probably settings):
        brc = XCenter_parent_WPAbstract_wpSwitchTo(somSelf, View);

    return (brc);
}

/*
 *@@ wpAddObjectWindowPage:
 *      this WPObject instance method normally adds the
 *      "Standard Options" page to the settings notebook
 *      (that's what the WPS reference calls it; it's actually
 *      the "Window" page).
 */

SOM_Scope ULONG  SOMLINK xctr_wpAddObjectWindowPage(XCenter *somSelf,
                                                    HWND hwndNotebook)
{
    /* XCenterData *somThis = XCenterGetData(somSelf); */
    XCenterMethodDebug("XCenter","xctr_wpAddObjectWindowPage");

    return (SETTINGS_PAGE_REMOVED);
}

/*
 *@@ wpAddSettingsPages:
 *      this WPObject instance method gets called by the WPS
 *      when the Settings view is opened to have all the
 *      settings page inserted into hwndNotebook.
 *
 *      We call XCenter::xwpAddXXenterPages to have the
 *      "XCenter" pages inserted on top.
 */

SOM_Scope BOOL  SOMLINK xctr_wpAddSettingsPages(XCenter *somSelf,
                                                HWND hwndNotebook)
{
    BOOL brc = FALSE;
    /* XCenterData *somThis = XCenterGetData(somSelf); */
    XCenterMethodDebug("XCenter","xctr_wpAddSettingsPages");

    brc = XCenter_parent_WPAbstract_wpAddSettingsPages(somSelf,
                                                       hwndNotebook);
    if (brc)
        _xwpAddXCenterPages(somSelf, hwndNotebook);

    return (brc);
}

/* ******************************************************************
 *                                                                  *
 *   XCenter class methods                                          *
 *                                                                  *
 ********************************************************************/

/*
 *@@ wpclsInitData:
 *      initialize XCenter class data.
 */

SOM_Scope void  SOMLINK xctrM_wpclsInitData(M_XCenter *somSelf)
{
    /* M_XCenterData *somThis = M_XCenterGetData(somSelf); */
    M_XCenterMethodDebug("M_XCenter","xctrM_wpclsInitData");

    M_XCenter_parent_M_WPAbstract_wpclsInitData(somSelf);
}

/*
 *@@ wpclsQueryStyle:
 *      prevent print.
 */

SOM_Scope ULONG  SOMLINK xctrM_wpclsQueryStyle(M_XCenter *somSelf)
{
    /* M_XCenterData *somThis = M_XCenterGetData(somSelf); */
    M_XCenterMethodDebug("M_XCenter","xctrM_wpclsQueryStyle");

    return (// M_XCenter_parent_M_WPAbstract_wpclsQueryStyle(somSelf)
            CLSSTYLE_NEVERPRINT);
                    // but allow templates
}

/*
 *@@ wpclsQueryTitle:
 *      tell the WPS the new class default title for XCenter.
 */

SOM_Scope PSZ  SOMLINK xctrM_wpclsQueryTitle(M_XCenter *somSelf)
{
    /* M_XCenterData *somThis = M_XCenterGetData(somSelf); */
    M_XCenterMethodDebug("M_XCenter","xctrM_wpclsQueryTitle");

    return ("XCenter");
}

/*
 *@@ wpclsQueryIconData:
 *      this WPObject class method builds the default
 *      icon for objects of a class (i.e. the icon which
 *      is shown if no instance icon is assigned). This
 *      apparently gets called from some of the other
 *      icon instance methods if no instance icon was
 *      found for an object. The exact mechanism of how
 *      this works is not documented.
 *
 *      We override this to give XCenter object a new
 *      icon (src\shared\xcenter.ico).
 */

SOM_Scope ULONG  SOMLINK xctrM_wpclsQueryIconData(M_XCenter *somSelf,
                                                  PICONINFO pIconInfo)
{
    /* M_XCenterData *somThis = M_XCenterGetData(somSelf); */
    M_XCenterMethodDebug("M_XCenter","xctrM_wpclsQueryIconData");

    if (pIconInfo) {
       pIconInfo->fFormat = ICON_RESOURCE;
       pIconInfo->resid   = ID_ICONXCENTER;
       pIconInfo->hmod    = cmnQueryMainModuleHandle();
    }

    return (sizeof(ICONINFO));
}

/*
 *@@ wpclsQuerySettingsPageSize:
 *      this WPObject class method should return the
 *      size of the largest settings page in dialog
 *      units; if a settings notebook is initially
 *      opened, i.e. no window pos has been stored
 *      yet, the WPS will use this size, to avoid
 *      truncated settings pages.
 */

SOM_Scope BOOL  SOMLINK xctrM_wpclsQuerySettingsPageSize(M_XCenter *somSelf,
                                                         PSIZEL pSizl)
{
    /* M_XCenterData *somThis = M_XCenterGetData(somSelf); */
    M_XCenterMethodDebug("M_XCenter","xctrM_wpclsQuerySettingsPageSize");

    return (M_XCenter_parent_M_WPAbstract_wpclsQuerySettingsPageSize(somSelf,
                                                                     pSizl));
}


