
/*
 *@@sourcefile xclslist.c:
 *      This file contains SOM code for the following XWorkplace classes:
 *
 *      -- XWPClassList (WPAbstract subclass)
 *
 *      Installation of this class is optional.
 *
 *      This class implements the "WPS Class List" object,
 *      which defines a new view for editing the WPS class list.
 *      With V0.9.0, this is now in a separate object,
 *      because the container window in the "Workplace Shell"
 *      object was a bit small.
 *
 *      This file is all new with V0.9.0.
 *      Some of the code in here used to be in xfsys.c for
 *      supporting the "WPS Classes" settings page in "Workplace
 *      Shell". However, much of the code has been rewritten
 *      and extended for V0.9.0.
 *
 *      Starting with V0.9.0, the files in classes\ contain only
 *      the SOM interface, i.e. the methods themselves.
 *      The implementation for this class is in config\classlst.c.
 *
 *      This class is an interesting example in two respects:
 *
 *      1)  For one, this shows how to implement a WPAbstract
 *          subclass from scratch, including defining a new
 *          object view. This is fairly well described in the
 *          "Object User Action: Popup Menu Methods" chapter
 *          in the WPS Programming Guide (the chapter has a
 *          misleading name, but the sample codes contains
 *          most of what is necessary).
 *
 *          In order to define our new object view, we do the
 *          following:
 *
 *          a)  XWPClassList::wpModifyPopupMenu (overridden)
 *              adds our new "Class List View" to the "Open"
 *              context submenu.
 *
 *          b)  XWPClassList::wpMenuItemSelected calls wpViewObject
 *              with our new view ID called when that menu item
 *              is selected. Note that the "ulView" parameter
 *              must always be the same as the menu item ID.
 *              (This is only mentioned in an obiter dictum in
 *              WPSGUIDE.)
 *
 *          c)  The standard wpViewObject (which is not overridden)
 *              will call XWPClassList::wpOpen, if a new view needs
 *              to be opened.
 *              So we override wpOpen to create the PM windows which
 *              are necessary for our open view. This is where
 *              standard PM programming comes in (see 2) below).
 *              To do this, XWPClassList::wpOpen calls cllCreateClassListView.
 *
 *              For WPS objects, there's additional work
 *              required to have the object's "use list" maintained.
 *              This requires calling wpAddToObjUseList and wpRegisterView.
 *              The view is added when WM_CREATE is received
 *              by fnwpClassListClient and removed upon WM_DESTROY
 *              (wpDeleteFromObjUseList).
 *
 *          d)  Also, XWPClassList::wpQueryDefaultView needs to return
 *              our new view to support double-clicking on the object.
 *
 *          e)  Finally, we must make sure that wpViewObject will
 *              never open concurrent views of the object. We
 *              therefore also remove the "Window" page from
 *              the settings notebook by overriding
 *              XWPClassList::wpAddObjectWindowPage.
 *
 *      2)  Secondly, this is a working example of "split windows",
 *          which PM does not have built-in support for. All the
 *          setup for this is done in XWPClassList::wpOpen, using
 *          the routines in /helpers/comctl.c. This is standard
 *          PM programming and not limited to the WPS.
 *
 *          The "WPS Class List" view creates a complex window
 *          hierarchy. See fnwpClassListClient for an overview.
 *
 *@@added V0.9.0 [umoeller]
 *
 *@@somclass XWPClassList xwlist_
 *@@somclass M_XWPClassList xwlistM_
 */

/*
 *      Copyright (C) 1998-2000 Ulrich M�ller.
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

#ifndef SOM_Module_xclslist_Source
#define SOM_Module_xclslist_Source
#endif
#define XWPClassList_Class_Source
#define M_XWPClassList_Class_Source

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

#define INCL_DOSSEMAPHORES
#define INCL_DOSEXCEPTIONS
#define INCL_DOSPROCESS
#define INCL_WINWINDOWMGR
#define INCL_WINMENUS
#include <os2.h>

// C library headers
#include <stdio.h>
#include <setjmp.h>

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\except.h"             // exception handling
#include "helpers\winh.h"               // PM helper routines

// SOM headers which don't crash with prec. header files
#include "xclslist.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling

#include "config\classlst.h"            // SOM logic for "WPS Classes" page

#pragma hdrstop

/* ******************************************************************
 *                                                                  *
 *   Globals                                                        *
 *                                                                  *
 ********************************************************************/

// "XWPClassList" key for wpRestoreData etc.
const char *pcszXWPClassList = "XWPClassList";

/* ******************************************************************
 *                                                                  *
 *   XWPClassList Instance Methods                                  *
 *                                                                  *
 ********************************************************************/

/*
 *@@ xwpAddXWPClassListPages:
 *      this actually adds the new pages into the
 *      "WPS Class List" notebook.
 */

SOM_Scope ULONG  SOMLINK xwlist_xwpAddXWPClassListPages(XWPClassList *somSelf,
                                                        HWND hwndDlg)
{
    CHAR    szTitle[200];
    PCREATENOTEBOOKPAGE pcnbp;
    PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();
    PSZ     p;

    /* XWPClassListData *somThis = XWPClassListGetData(somSelf); */
    XWPClassListMethodDebug("XWPClassList","xwlist_xwpAddXWPClassListPages");

    // remove accelerator char
    strcpy(szTitle, pNLSStrings->pszOpenClassList);
    p = strchr(szTitle, '~');
    if (p)
        strcpy(p, p+1);

    pcnbp = malloc(sizeof(CREATENOTEBOOKPAGE));
    memset(pcnbp, 0, sizeof(CREATENOTEBOOKPAGE));
    pcnbp->somSelf = somSelf;
    pcnbp->hwndNotebook = hwndDlg;
    pcnbp->hmod = cmnQueryNLSModuleHandle(FALSE);
    pcnbp->usPageStyleFlags = BKA_MAJOR;
    pcnbp->pszName = szTitle;
    pcnbp->ulDlgID = ID_XLD_SETTINGS;
    pcnbp->ulDefaultHelpPanel  = ID_XSH_SETTINGS_CLASSLIST;
    pcnbp->ulPageID = SP_CLASSLIST;
    pcnbp->pfncbInitPage    = cllClassListInitPage;
    pcnbp->pfncbItemChanged = cllClassListItemChanged;

    return (ntbInsertPage(pcnbp));
}

/*
 *@@ wpInitData:
 *      this WPObject instance method gets called when the
 *      object is being initialized (on wake-up or creation).
 *      We initialize our additional instance data here.
 *      Always call the parent method first.
 */

SOM_Scope void  SOMLINK xwlist_wpInitData(XWPClassList *somSelf)
{
    XWPClassListData *somThis = XWPClassListGetData(somSelf);
    XWPClassListMethodDebug("XWPClassList","xwlist_wpInitData");

    XWPClassList_parent_WPAbstract_wpInitData(somSelf);

    // initialize instance data;
    // these values can be overwritten in wpRestoreState
    _fShowSOMObject = FALSE;
    _fShowMethods = TRUE;
    _ulSortID = ID_XLMI_METHOD_SORT_INDEX;
    _ulMethodsRadioB = ID_XLDI_RADIO_INSTANCEMETHODS;
    _hwndOpenView = NULLHANDLE;
    _fMenuCnrWhitespace = FALSE;
}

/*
 *@@ wpSaveState:
 *      this WPObject instance method saves an object's state
 *      persistently so that it can later be re-initialized
 *      with wpRestoreState. This gets called during wpClose,
 *      wpSaveImmediate or wpSaveDeferred processing.
 *      All persistent instance variables should be stored here.
 */

SOM_Scope BOOL  SOMLINK xwlist_wpSaveState(XWPClassList *somSelf)
{
    XWPClassListData *somThis = XWPClassListGetData(somSelf);
    XWPClassListMethodDebug("XWPClassList","xwlist_wpSaveState");

    XWPClassList_parent_WPAbstract_wpSaveState(somSelf);

    _wpSaveLong(somSelf, (PSZ)pcszXWPClassList, 1, (ULONG)_fShowSOMObject);
    _wpSaveLong(somSelf, (PSZ)pcszXWPClassList, 2, (ULONG)_fShowMethods);
    _wpSaveLong(somSelf, (PSZ)pcszXWPClassList, 3, (ULONG)_ulSortID);
    _wpSaveLong(somSelf, (PSZ)pcszXWPClassList, 4, (ULONG)_ulMethodsRadioB);

    return (TRUE);
}

/*
 *@@ wpRestoreState:
 *      this WPObject instance method gets called during object
 *      initialization (after wpInitData) to restore the data
 *      which was stored with wpSaveState.
 */

SOM_Scope BOOL  SOMLINK xwlist_wpRestoreState(XWPClassList *somSelf,
                                              ULONG ulReserved)
{
    ULONG   ul;
    XWPClassListData *somThis = XWPClassListGetData(somSelf);
    XWPClassListMethodDebug("XWPClassList","xwlist_wpRestoreState");

    XWPClassList_parent_WPAbstract_wpRestoreState(somSelf, ulReserved);

    if (_wpRestoreLong(somSelf, (PSZ)pcszXWPClassList, 1, &ul))
        _fShowSOMObject = ul;
    if (_wpRestoreLong(somSelf, (PSZ)pcszXWPClassList, 2, &ul))
        _fShowMethods = ul;
    if (_wpRestoreLong(somSelf, (PSZ)pcszXWPClassList, 3, &ul))
        _ulSortID = ul;
    if (_wpRestoreLong(somSelf, (PSZ)pcszXWPClassList, 4, &ul))
        _ulMethodsRadioB = ul;

    return (TRUE);
}

/*
 *@@ wpModifyPopupMenu:
 *      this WPObject instance methods gets called by the WPS
 *      when a context menu needs to be built for the object
 *      and allows the object to manipulate its context menu.
 *      This gets called _after_ wpFilterPopupMenu.
 *
 *      We need to modify the "Open" submenu for the WPS class
 *      list so that our new "Class List View" is shown.
 */

SOM_Scope BOOL  SOMLINK xwlist_wpModifyPopupMenu(XWPClassList *somSelf,
                                                 HWND hwndMenu,
                                                 HWND hwndCnr,
                                                 ULONG iPosition)
{
    /* XWPClassListData *somThis = XWPClassListGetData(somSelf); */
    XWPClassListMethodDebug("XWPClassList","xwlist_wpModifyPopupMenu");

    if (XWPClassList_parent_WPAbstract_wpModifyPopupMenu(somSelf,
                                                         hwndMenu,
                                                         hwndCnr,
                                                         iPosition))
        return (cllModifyPopupMenu(somSelf,
                                   hwndMenu,
                                   hwndCnr,
                                   iPosition));
    else
        return (FALSE);
}

/*
 *@@ wpMenuItemSelected:
 *      this WPObject method processes menu selections.
 *      This must be overridden to support new menu
 *      items which have been added in wpModifyPopupMenu.
 *
 *      We need to support "open class list view".
 *
 *      Note that the WPS invokes this method upon every
 *      object which has been selected in the container.
 *      That is, if three objects have been selected and
 *      a menu item has been selected for all three of
 *      them, all three objects will receive this method
 *      call. This is true even if FALSE is returned from
 *      this method.
 */

SOM_Scope BOOL  SOMLINK xwlist_wpMenuItemSelected(XWPClassList *somSelf,
                                                  HWND hwndFrame,
                                                  ULONG ulMenuId)
{
    BOOL brc = TRUE;

    /* XWPClassListData *somThis = XWPClassListGetData(somSelf); */
    XWPClassListMethodDebug("XWPClassList","xwlist_wpMenuItemSelected");

    brc = cllMenuItemSelected(somSelf, hwndFrame, ulMenuId);

    if (!brc)
        // not our menu item (maybe settings): call parent
        brc = XWPClassList_parent_WPAbstract_wpMenuItemSelected(somSelf,
                                                                hwndFrame,
                                                                ulMenuId);

    return (brc);
}

/*
 *@@ wpQueryDefaultView:
 *      this returns the default view of the object, which
 *      for XWPClassList is the "Class list" view. For
 *      user-defined views (which ours is one of), this
 *      must be the same as the menu item ID in the "Open"
 *      submenu.
 */

SOM_Scope ULONG  SOMLINK xwlist_wpQueryDefaultView(XWPClassList *somSelf)
{
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    /* XWPClassListData *somThis = XWPClassListGetData(somSelf); */
    XWPClassListMethodDebug("XWPClassList","xwlist_wpQueryDefaultView");

    return (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_OPENCLASSLIST);
}

/*
 *@@ wpQueryDefaultHelp:
 *      this WPObject instance method specifies the default
 *      help panel for an object (when "Extended help" is
 *      selected from the object's context menu). This should
 *      describe what this object can do in general.
 *      We must return TRUE to report successful completion.
 *
 *      We will display some help for the XWPClassList object.
 */

SOM_Scope BOOL  SOMLINK xwlist_wpQueryDefaultHelp(XWPClassList *somSelf,
                                                  PULONG pHelpPanelId,
                                                  PSZ HelpLibrary)
{
    /* XWPClassListData *somThis = XWPClassListGetData(somSelf); */
    XWPClassListMethodDebug("XWPClassList","xwlist_wpQueryDefaultHelp");

    strcpy(HelpLibrary, cmnQueryHelpLibrary());
    *pHelpPanelId = ID_XSH_SETTINGS_WPSCLASSES;
    return (TRUE);
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
 *      We need to support the new "Class list view" here.
 *
 *      The implementation for this is in cllCreateClassListView.
 *      See fnwpClassListClient for an overview of the
 *      window hierarchy which is created from this method.
 */

SOM_Scope HWND  SOMLINK xwlist_wpOpen(XWPClassList *somSelf,
                                      HWND hwndCnr, ULONG ulView,
                                      ULONG param)
{
    HWND    hwndNewView = 0;
    BOOL    fLocked = FALSE;
    XWPClassListMethodDebug("XWPClassList","xwlist_wpOpen");

    TRY_LOUD(excpt1, NULL)
    {
        PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
        XWPClassListData *somThis = XWPClassListGetData(somSelf);

        fLocked = !_wpRequestObjectMutexSem(somSelf, 5000);
        if (fLocked)
        {
            if (ulView == (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_OPENCLASSLIST))
            {
                hwndNewView = cllCreateClassListView(somSelf, hwndCnr, ulView);
                _hwndOpenView = hwndNewView;
            }
            else
                // other view (probably settings):
                hwndNewView = XWPClassList_parent_WPAbstract_wpOpen(somSelf,
                                                                    hwndCnr,
                                                                    ulView,
                                                                    param);
        }
    }
    CATCH(excpt1) { } END_CATCH();

    if (fLocked)
        _wpReleaseObjectMutexSem(somSelf);

    return (hwndNewView);
}

/*
 *@@ wpAddObjectWindowPage:
 *      this instance method normally adds the "Standard Options"
 *      page to the settings notebook (that's what the WPS
 *      reference calls it; it's actually the "Window" page).
 *      We don't want that page in XWPClassList, so we remove it.
 */

SOM_Scope ULONG  SOMLINK xwlist_wpAddObjectWindowPage(XWPClassList *somSelf,
                                                      HWND hwndNotebook)
{
    /* XWPClassListData *somThis = XWPClassListGetData(somSelf); */
    XWPClassListMethodDebug("XWPClassList","xwlist_wpAddObjectWindowPage");

    /* return (XWPClassList_parent_WPAbstract_wpAddObjectWindowPage(somSelf,
                                                                 hwndNotebook)); */
    return (SETTINGS_PAGE_REMOVED);
}

/*
 *@@ wpAddSettingsPages:
 *      this WPObject instance method gets called by the WPS
 *      when the Settings view is opened to have all the
 *      settings page inserted into hwndNotebook.
 *
 *      This instance method is overridden in order
 *      to add the new XWPClassList pages on top of
 *      the generic WPAbstract settings pages.
 */

SOM_Scope BOOL  SOMLINK xwlist_wpAddSettingsPages(XWPClassList *somSelf,
                                                  HWND hwndNotebook)
{
    /* XWPClassListData *somThis = XWPClassListGetData(somSelf); */
    XWPClassListMethodDebug("XWPClassList","xwlist_wpAddSettingsPages");

    XWPClassList_parent_WPAbstract_wpAddSettingsPages(somSelf, hwndNotebook);

    return (_xwpAddXWPClassListPages(somSelf, hwndNotebook));
}

/* ******************************************************************
 *                                                                  *
 *   XWPClassList Class Methods                                     *
 *                                                                  *
 ********************************************************************/

/*
 *@@ wpclsInitData:
 *      initialize XWPClassList class data.
 */

SOM_Scope void  SOMLINK xwlistM_wpclsInitData(M_XWPClassList *somSelf)
{
    /* M_XWPClassListData *somThis = M_XWPClassListGetData(somSelf); */
    M_XWPClassListMethodDebug("M_XWPClassList","xwlistM_wpclsInitData");

    M_XWPClassList_parent_M_WPAbstract_wpclsInitData(somSelf);

    {
        PKERNELGLOBALS   pKernelGlobals = krnLockGlobals(5000);
        // store the class object in KERNELGLOBALS
        pKernelGlobals->fXWPClassList = TRUE;
        krnUnlockGlobals();
    }
}

/*
 * wpclsQueryStyle:
 *      prevent copy, delete, print.
 */

SOM_Scope ULONG  SOMLINK xwlistM_wpclsQueryStyle(M_XWPClassList *somSelf)
{
    /* M_XWPClassListData *somThis = M_XWPClassListGetData(somSelf); */
    M_XWPClassListMethodDebug("M_XWPClassList","xwlistM_wpclsQueryStyle");

    return (M_XWPClassList_parent_M_WPAbstract_wpclsQueryStyle(somSelf)
                | CLSSTYLE_NEVERPRINT
                | CLSSTYLE_NEVERCOPY
                | CLSSTYLE_NEVERDELETE);
}

/*
 *@@ wpclsQueryTitle:
 *      tell the WPS the new class default title:
 *      "WPS Class List".
 */

SOM_Scope PSZ  SOMLINK xwlistM_wpclsQueryTitle(M_XWPClassList *somSelf)
{
    PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();
    /* M_XWPClassListData *somThis = M_XWPClassListGetData(somSelf); */
    M_XWPClassListMethodDebug("M_XWPClassList","xwlistM_wpclsQueryTitle");

    return (pNLSStrings->pszXWPClassList);
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
 *      We give the "WPS Class List" object
 *      a new icon.
 */

SOM_Scope ULONG  SOMLINK xwlistM_wpclsQueryIconData(M_XWPClassList *somSelf,
                                                    PICONINFO pIconInfo)
{
    /* M_XWPClassListData *somThis = M_XWPClassListGetData(somSelf); */
    M_XWPClassListMethodDebug("M_XWPClassList","xwlistM_wpclsQueryIconData");

    if (pIconInfo) {
       pIconInfo->fFormat = ICON_RESOURCE;
       pIconInfo->resid   = ID_ICONXWPLIST;
       pIconInfo->hmod    = cmnQueryMainModuleHandle();
    }

    return (sizeof(ICONINFO));
}

