
/*
 *@@sourcefile xcenter.c:
 *      This file contains SOM code for the following XWorkplace classes:
 *
 *      --  XCenter: a WarpCenter replacement.
 *
 *      See src\shared\center.c for an introduction.
 *
 *      Installation of this class is completely optional.
 *
 *@@added V0.9.4 (2000-06-11) [umoeller]
 *@@somclass XCenter xctr_
 *@@somclass M_XCenter xctrM_
 */

/*
 *      Copyright (C) 2000-2002 Ulrich M�ller.
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
 *  This file was generated by the SOM Compiler and Emitter Framework.
 *  Generated using:
 *      SOM Emitter emitctm: 2.41
 */

#ifndef SOM_Module_xcenter_Source
#define SOM_Module_xcenter_Source
#endif
#define XCenter_Class_Source
#define M_XCenter_Class_Source

#pragma strings(readonly)

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
#define INCL_DOSERRORS

#define INCL_WINFRAMEMGR
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
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\winh.h"               // PM helper routines

// SOM headers which don't crash with prec. header files
#include "xcenter.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\helppanels.h"          // all XWorkplace help panel IDs
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling

#include "shared\center.h"              // public XCenter interfaces

#include "xcenter\centerp.h"            // private XCenter implementation

// other SOM headers

#pragma hdrstop

/* ******************************************************************
 *
 *   Declarations
 *
 ********************************************************************/

/*
 *@@ WPSAVELONGITEM:
 *      key/PULONG pair to be saved in a loop in
 *      wpSaveState.
 *
 *@@added V0.9.7 (2001-01-18) [umoeller]
 */

typedef struct _WPSAVELONGITEM
{
    ULONG   ulSaveKey;
    PULONG  pul;
} WPSAVELONGITEM, *PWPSAVELONGITEM;

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

/* ******************************************************************
 *
 *   XCenter instance methods
 *
 ********************************************************************/

/*
 *@@ xwpAddXCenterPages:
 *
 *@@changed V0.9.9 (2001-03-09) [umoeller]: made second "view" page major "style" page
 */

SOM_Scope ULONG  SOMLINK xctr_xwpAddXCenterPages(XCenter *somSelf,
                                                 HWND hwndNotebook)
{
    PCREATENOTEBOOKPAGE pcnbp;
    // PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();

    /* XCenterData *somThis = XCenterGetData(somSelf); */
    XCenterMethodDebug("XCenter","xctr_xwpAddXCenterPages");

    // add the "Widgets" page
    pcnbp = malloc(sizeof(CREATENOTEBOOKPAGE));
    memset(pcnbp, 0, sizeof(CREATENOTEBOOKPAGE));
    pcnbp->somSelf = somSelf;
    pcnbp->hwndNotebook = hwndNotebook;
    pcnbp->hmod = cmnQueryNLSModuleHandle(FALSE);
    pcnbp->usPageStyleFlags = BKA_MAJOR;
    pcnbp->pszName = cmnGetString(ID_XSSI_WIDGETSPAGE);  // pszWidgetsPage
    pcnbp->ulDlgID = ID_XFD_CONTAINERPAGE;
    pcnbp->ulDefaultHelpPanel  = ID_XSH_XCENTER_WIDGETS;
    pcnbp->ulPageID = SP_XCENTER_WIDGETS;
    pcnbp->pampControlFlags = G_pampGenericCnrPage;
    pcnbp->cControlFlags = G_cGenericCnrPage;
    pcnbp->pfncbInitPage    = ctrpWidgetsInitPage;
    pcnbp->pfncbItemChanged = ctrpWidgetsItemChanged;
    ntbInsertPage(pcnbp);

    // add the "Classes" page
    pcnbp = malloc(sizeof(CREATENOTEBOOKPAGE));
    memset(pcnbp, 0, sizeof(CREATENOTEBOOKPAGE));
    pcnbp->somSelf = somSelf;
    pcnbp->hwndNotebook = hwndNotebook;
    pcnbp->hmod = cmnQueryNLSModuleHandle(FALSE);
    pcnbp->usPageStyleFlags = BKA_MAJOR;
    pcnbp->pszName = cmnGetString(ID_XSSI_CLASSESPAGE);  // pszClassesPage
    pcnbp->ulDlgID = ID_XFD_CONTAINERPAGE;
    pcnbp->ulDefaultHelpPanel  = ID_XSH_XCENTER_CLASSES;
    pcnbp->ulPageID = SP_XCENTER_CLASSES;
    pcnbp->pampControlFlags = G_pampGenericCnrPage;
    pcnbp->cControlFlags = G_cGenericCnrPage;
    pcnbp->pfncbInitPage    = ctrpClassesInitPage;
    pcnbp->pfncbItemChanged = ctrpClassesItemChanged;
    ntbInsertPage(pcnbp);

    // add the "View2" page on top
    pcnbp = malloc(sizeof(CREATENOTEBOOKPAGE));
    memset(pcnbp, 0, sizeof(CREATENOTEBOOKPAGE));
    pcnbp->somSelf = somSelf;
    pcnbp->hwndNotebook = hwndNotebook;
    pcnbp->hmod = cmnQueryNLSModuleHandle(FALSE);
    pcnbp->usPageStyleFlags = BKA_MAJOR;
    pcnbp->fEnumerate = TRUE;
    pcnbp->pszName = cmnGetString(ID_XSSI_STYLEPAGE);  // pszStylePage
    pcnbp->ulDlgID = ID_XFD_EMPTYDLG;           // V0.9.16 (2001-09-29) [umoeller] ID_CRD_SETTINGS_VIEW2;
    pcnbp->ulDefaultHelpPanel  = ID_XSH_XCENTER_VIEW2;
    pcnbp->ulPageID = SP_XCENTER_VIEW2;
    pcnbp->pfncbInitPage    = ctrpView2InitPage;
    pcnbp->pfncbItemChanged = ctrpView2ItemChanged;
    ntbInsertPage(pcnbp);

    // add the "View1" page on top
    pcnbp = malloc(sizeof(CREATENOTEBOOKPAGE));
    memset(pcnbp, 0, sizeof(CREATENOTEBOOKPAGE));
    pcnbp->somSelf = somSelf;
    pcnbp->hwndNotebook = hwndNotebook;
    pcnbp->hmod = cmnQueryNLSModuleHandle(FALSE);
    pcnbp->usPageStyleFlags = BKA_MAJOR;
    pcnbp->fEnumerate = TRUE;
    pcnbp->pszName = cmnGetString(ID_XSSI_VIEWPAGE);  // pszViewPage
    pcnbp->ulDlgID = ID_CRD_SETTINGS_VIEW;
    pcnbp->ulDefaultHelpPanel  = ID_XSH_XCENTER_VIEW1;
    pcnbp->ulPageID = SP_XCENTER_VIEW1;
    pcnbp->pfncbInitPage    = ctrpView1InitPage;
    pcnbp->pfncbItemChanged = ctrpView1ItemChanged;
    return (ntbInsertPage(pcnbp));
}

/*
 *@@ xwpQueryWidgets:
 *      returns an array of XCENTERWIDGETSETTING structures
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
 *      Returns FALSE on errors.
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
        /* _Pmpf((__FUNCTION__ ": calling ctrInsertWidget with %s, %s",
                (pszWidgetClass) ? pszWidgetClass : "NULL",
                (pszSetupString) ? pszSetupString : "NULL")); */
        brc = ctrpInsertWidget(somSelf,
                               ulBeforeIndex,
                               pszWidgetClass,
                               pszSetupString);
        // _Pmpf((__FUNCTION__ ": got %d from ctrInsertWidget", brc));
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
 *      Returns FALSE on errors.
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xctr_xwpRemoveWidget(XCenter *somSelf,
                                             ULONG ulIndex)
{
    // XCenterData *somThis = XCenterGetData(somSelf);
    XCenterMethodDebug("XCenter","xctr_xwpRemoveWidget");

    return (ctrpRemoveWidget(somSelf, ulIndex));
}

/*
 *@@ xwpMoveWidget:
 *      moves a widget to a new position within the
 *      XCenter.
 *
 *      ulIndex2Move specifies the widget to be moved
 *      (with 0 being the leftmost widget).
 *
 *      ulBeforeIndex specifies the position to which
 *      the widget should be moved. 0 means leftmost,
 *      1 means before first widget, etc.
 *      If ulBeforeIndex is -1 or larger than the
 *      no. of widgets in the XCenter, we insert the
 *      new widget as the last widget.
 *
 *@@added V0.9.7 (2000-12-10) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xctr_xwpMoveWidget(XCenter *somSelf,
                                           ULONG ulIndex2Move,
                                           ULONG ulBeforeIndex)
{
    // XCenterData *somThis = XCenterGetData(somSelf);
    XCenterMethodDebug("XCenter","xctr_xwpMoveWidget");

    return (ctrpMoveWidget(somSelf, ulIndex2Move, ulBeforeIndex));
}

/*
 *@@ xwpSetPriority:
 *      sets a new priority for the XCenter view.
 *      ulClass and lDelta are used as with DosSetPriority.
 *      An open XCenter thread is updated.
 *
 *@@added V0.9.7 (2001-01-03) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xctr_xwpSetPriority(XCenter *somSelf,
                                            ULONG ulClass, long lDelta)
{
    // XCenterData *somThis = XCenterGetData(somSelf);
    XCenterMethodDebug("XCenter","xctr_xwpSetPriority");

    return (ctrpSetPriority(somSelf, ulClass, lDelta));
}

/*
 *@@ xwpQuerySetup2:
 *      this XFldObject method is overridden to support
 *      setup strings for folders.
 *
 *      See XFldObject::xwpQuerySetup2 for details.
 *
 *@@added V0.9.7 (2000-12-09) [umoeller]
 *@@changed V0.9.16 (2001-10-11) [umoeller]: adjusted to new implementation
 */

SOM_Scope BOOL  SOMLINK xctr_xwpQuerySetup2(XCenter *somSelf,
                                            PVOID pstrSetup)
{
    // XCenterData *somThis = XCenterGetData(somSelf);
    XCenterMethodDebug("XCenter","xctr_xwpQuerySetup2");

    // call implementation
    return (ctrpQuerySetup(somSelf, pstrSetup));
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

    // initialize defaults from the table for those
    // variables which are in there...
    ctrpInitData(somSelf);

    _pvOpenView = NULL;
    _tidRunning = 0;

    _fShowingOpenViewMenu = FALSE;

    _pszPackedWidgetSettings = NULL;
    _cbPackedWidgetSettings = 0;

    _pllAllWidgetSettings = NULL;
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
 *@@ wpSetup:
 *      this WPObject instance method is called to allow an
 *      object to set itself up according to setup strings.
 *      As opposed to wpSetupOnce, this gets called any time
 *      a setup string is invoked.
 *
 *      We scan for the XCenter setup here. The implementation
 *      is in ctrpSetup.
 *
 *@@added V0.9.7 (2001-01-25) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xctr_wpSetup(XCenter *somSelf, PSZ pszSetupString)
{
    BOOL brc = FALSE;
    // XCenterData *somThis = XCenterGetData(somSelf);
    XCenterMethodDebug("XCenter","xctr_wpSetup");

    brc = XCenter_parent_WPAbstract_wpSetup(somSelf, pszSetupString);

    ctrpSetup(somSelf,
              pszSetupString);

    return (brc);
}

/*
 *@@ wpSetupOnce:
 *      this WPObject method allows special object handling
 *      based on a creation setup string after an object has
 *      been fully created.
 *      As opposed to WPObject::wpSetup, this method _only_
 *      gets called during object creation. The WPObject
 *      implementation calls wpSetup in turn.
 *      If FALSE is returned, object creation is aborted.
 *
 *      After having parsed the setup string, we check if
 *      we have any widgets in the XCenter already. If not,
 *      this probably means that no initial WIDGETS setup
 *      string was given with the object, and we create
 *      some standard XCenter widgets.
 *
 *@@added V0.9.16 (2001-10-15) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xctr_wpSetupOnce(XCenter *somSelf, PSZ pszSetupString)
{
    XCenterData *somThis = XCenterGetData(somSelf);
    XCenterMethodDebug("XCenter","xctr_wpSetupOnce");

    if (XCenter_parent_WPAbstract_wpSetupOnce(somSelf, pszSetupString))
    {
        return (ctrpSetupOnce(somSelf, pszSetupString));
    }

    return FALSE;
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
    // XCenterData *somThis = XCenterGetData(somSelf);
    XCenterMethodDebug("XCenter","xctr_wpSaveState");

    brc = XCenter_parent_WPAbstract_wpSaveState(somSelf);

    if (brc)
        brc = ctrpSaveState(somSelf);

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
    // XCenterData *somThis = XCenterGetData(somSelf);
    XCenterMethodDebug("XCenter","xctr_wpRestoreState");

    brc = XCenter_parent_WPAbstract_wpRestoreState(somSelf,
                                                   ulReserved);

    if (brc)
        brc = ctrpRestoreState(somSelf);

    return (brc);
}

/*
 *@@ wpModifyPopupMenu:
 *      this WPObject instance methods gets called by the WPS
 *      when a context menu needs to be built for the object
 *      and allows the object to manipulate its context menu.
 *      This gets called _after_ wpFilterPopupMenu.
 *
 *      We override this to add the "Widgets" submenu to an
 *      open XCenter's popup menu. Note that we have not
 *      overridden wpMenuItemSelected, because WM_COMMAND
 *      is intercepted in the XCenter windows directly.
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xctr_wpModifyPopupMenu(XCenter *somSelf,
                                               HWND hwndMenu,
                                               HWND hwndCnr,
                                               ULONG iPosition)
{
    BOOL brc;
    // XCenterData *somThis = XCenterGetData(somSelf);
    XCenterMethodDebug("XCenter","xctr_wpModifyPopupMenu");

    brc = XCenter_parent_WPAbstract_wpModifyPopupMenu(somSelf,
                                                      hwndMenu,
                                                      hwndCnr,
                                                      iPosition);
    if (brc)
        // add "Add widget" submenu etc.
        brc = ctrpModifyPopupMenu(somSelf, hwndMenu);

    return (brc);
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
    // // PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    /* XCenterData *somThis = XCenterGetData(somSelf); */
    XCenterMethodDebug("XCenter","xctr_wpQueryDefaultView");

    return (cmnQuerySetting(sulVarMenuOffset) + ID_XFMI_OFS_XWPVIEW);
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
 *      We open an XCenter view here by calling ctrpCreateXCenterView.
 *
 *@@changed V0.9.9 (2001-02-06) [umoeller]: now redirecting settings to thread-1
 */

SOM_Scope HWND  SOMLINK xctr_wpOpen(XCenter *somSelf,
                                    HWND hwndCnr,
                                    ULONG ulView,
                                    ULONG param)
{
    HWND    hwndNewView = NULLHANDLE;
    // // PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    XCenterData *somThis = XCenterGetData(somSelf);
    XCenterMethodDebug("XCenter","xctr_wpOpen");

    if (ulView == (cmnQuerySetting(sulVarMenuOffset) + ID_XFMI_OFS_XWPVIEW))
    {
        if (!_tidRunning)       // V0.9.12 (2001-05-20) [umoeller]
        {
            // no open view yet (just make sure!)
            HAB hab;
            if (hwndCnr)
                hab = WinQueryAnchorBlock(hwndCnr);
            else
                hab = WinQueryAnchorBlock(cmnQueryActiveDesktopHWND());
            hwndNewView = ctrpCreateXCenterView(somSelf,
                                                hab,
                                                ulView,
                                                // store in instance data
                                                &_pvOpenView);
            if (!_fHelpDisplayed)
            {
                ULONG ulPanel = 0;
                CHAR szHelp[CCHMAXPATH];
                _wpQueryDefaultHelp(somSelf,
                                    &ulPanel,
                                    szHelp);
                // help not displayed yet:
                _wpDisplayHelp(somSelf,
                               ulPanel,
                               szHelp);

                _fHelpDisplayed = TRUE;
                _wpSaveDeferred(somSelf);
            }
        }
    }
    else
    {
        // other view (probably settings):

        // make sure we don't open the other views on the XCenter
        // view thread... this causes problems in various situations
        if (    (_tidRunning)
             && (doshMyTID() == _tidRunning)
           )
        {
            // we're on the XCenter thread here:
            // redirect to thread 1
            hwndNewView = (HWND)krnSendThread1ObjectMsg(T1M_OPENOBJECTFROMPTR,
                                                        (MPARAM)somSelf,
                                                        (MPARAM)ulView);
        }
        else
            hwndNewView = XCenter_parent_WPAbstract_wpOpen(somSelf,
                                                           hwndCnr,
                                                           ulView,
                                                           param);
    }

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
 *@@changed V0.9.16 (2001-12-31) [umoeller]: this never worked; now sending message to client properly
 */

SOM_Scope BOOL  SOMLINK xctr_wpSwitchTo(XCenter *somSelf, ULONG View)
{
    BOOL brc = FALSE;
    // PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    // XCenterData *somThis = XCenterGetData(somSelf);
    XCenterMethodDebug("XCenter","xctr_wpSwitchTo");

    // check if we should switch to the existing XCenter view
    if (View == (cmnQuerySetting(sulVarMenuOffset) + ID_XFMI_OFS_XWPVIEW))
    {
        // yes:
        PUSEITEM    pUseItem = NULL;
        for (pUseItem = _wpFindUseItem(somSelf, USAGE_OPENVIEW, NULL);
             pUseItem;
             pUseItem = _wpFindUseItem(somSelf, USAGE_OPENVIEW, pUseItem))
        {
            PVIEWITEM pViewItem = (PVIEWITEM)(pUseItem + 1);
            if (pViewItem->view == View)
            {
                HWND hwndClient;
                // yes, it's an XCenter view:
                // instead of activating the view (which is what
                // the WPS normally does), show the frame on top
                // and restart the update timer
                // DO NOT GIVE FOCUS, DO NOT ACTIVATE

                    // duh, this must be posted to the client, not the
                    // frame... V0.9.16 (2001-12-31) [umoeller]
                if (hwndClient = WinWindowFromID(pViewItem->handle, FID_CLIENT))
                {
                    // _Pmpf((__FUNCTION__ ": posting XFMF_RESURFACE to %lX", hwndClient));
                    WinPostMsg(hwndClient,
                               XCM_REFORMAT,
                               (MPARAM)XFMF_RESURFACE,
                               0);
                    brc = TRUE;
                }

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
 *@@ wpClose:
 *      this WPObject method goes through the USAGE_OPENVIEW
 *      useitems of the object and sends (!) WM_CLOSE to
 *      each of them.
 *
 *      This is also used by XShutdown to close objects.
 *
 *      For the XCenter, we call the parent to have this
 *      job done. Howver, in addition, we MUST wait for
 *      the XCenter thread to terminate or otherwise we
 *      might hang on Desktop workarea resize...
 *
 *      The problem is that once WM_CLOSE is received by the
 *      XCenter frame, it will destroy all child windows
 *      and then exit the XCenter thread a bit later. During
 *      exit, if "resize desktop" is enabled, the desktop is
 *      resized. If XShutdown closes the XCenter and the
 *      desktop next, the WPS apparently cannot handle this
 *      if this isn't properly serialized.
 *
 *@@added V0.9.9 (2001-04-04) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xctr_wpClose(XCenter *somSelf)
{
    // XCenterData *somThis = XCenterGetData(somSelf);
    XCenterMethodDebug("XCenter","xctr_wpClose");

    if (XCenter_parent_WPAbstract_wpClose(somSelf))
    {
        /* while (_tid)
            winhSleep(100); */

        return (TRUE);
    }

    return (FALSE);
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
 *      this WPObject class method gets called when a class
 *      is loaded by the WPS (probably from within a
 *      somFindClass call) and allows the class to initialize
 *      itself.
 */

SOM_Scope void  SOMLINK xctrM_wpclsInitData(M_XCenter *somSelf)
{
    APIRET arc = NO_ERROR;
    /* M_XCenterData *somThis = M_XCenterGetData(somSelf); */
    M_XCenterMethodDebug("M_XCenter","xctrM_wpclsInitData");

    M_XCenter_parent_M_WPAbstract_wpclsInitData(somSelf);

    krnClassInitialized(G_pcszXCenterReal);

    // resolve WinSetDesktopWorkArea etc. (ctr_engine.c)
    arc = ctrpDesktopWorkareaSupported();
    // _Pmpf((__FUNCTION__ ": ctrpChangeDesktopSupported returned %d.", arc));
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
 *      this WPObject class method tells the WPS the clear
 *      name of a class, which is shown in the third column
 *      of a Details view and also used as the default title
 *      for new objects of a class.
 */

SOM_Scope PSZ  SOMLINK xctrM_wpclsQueryTitle(M_XCenter *somSelf)
{
    /* M_XCenterData *somThis = M_XCenterGetData(somSelf); */
    M_XCenterMethodDebug("M_XCenter","xctrM_wpclsQueryTitle");

    return ((PSZ)ENTITY_XCENTER);
}

/*
 *@@ wpclsCreateDefaultTemplates:
 *      this WPObject class method is called by the
 *      Templates folder to allow a class to
 *      create its default templates.
 *
 *      The default WPS behavior is to create new templates
 *      if the class default title is different from the
 *      existing templates.
 *
 *@@added V0.9.7 (2001-01-11) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xctrM_wpclsCreateDefaultTemplates(M_XCenter *somSelf,
                                                          WPObject* Folder)
{
    /* M_XCenterData *somThis = M_XCenterGetData(somSelf); */
    M_XCenterMethodDebug("M_XCenter","xctrM_wpclsCreateDefaultTemplates");

    // pretend we've created the templates
    return (TRUE);
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

    if (pIconInfo)
    {
       pIconInfo->fFormat = ICON_RESOURCE;
       pIconInfo->resid   = ID_ICONXCENTER;
       pIconInfo->hmod    = cmnQueryMainResModuleHandle();
    }

    return (sizeof(ICONINFO));
}


