
/*
 *@@sourcefile w_pulse.c:
 *      XCenter "Pulse" widget implementation.
 *      This is build into the XCenter and not in
 *      a plugin DLL.
 *
 *      Function prefix for this file:
 *      --  Bwgt*
 *
 *      This is all new with V0.9.7.
 *
 *@@added V0.9.7 (2000-11-27) [umoeller]
 *@@header "shared\center.h"
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

#define INCL_WINWINDOWMGR
#define INCL_WINDIALOGS
#define INCL_WINBUTTONS
#define INCL_WINSTDCNR
#include <os2.h>

// C library headers
#include <stdio.h>
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\cnrh.h"               // container helper routines
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\winh.h"               // PM helper routines

// SOM headers which don't crash with prec. header files
#include "xcenter.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\notebook.h"            // generic XWorkplace notebook handling

#include "shared\center.h"              // public XCenter interfaces
#include "xcenter\centerp.h"            // private XCenter implementation

#pragma hdrstop                     // VAC++ keeps crashing otherwise

/* ******************************************************************
 *
 *   "View" page notebook callbacks (notebook.c)
 *
 ********************************************************************/

/*
 *@@ ctrpViewInitPage:
 *      notebook callback function (notebook.c) for the
 *      XCenter instance settings page.
 *      Sets the controls on the page according to the
 *      instance settings.
 *
 *@@added V0.9.7 (2000-12-05) [umoeller]
 */

VOID ctrpViewInitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                      ULONG flFlags)        // CBI_* flags (notebook.h)
{
    XCenterData *somThis = XCenterGetData(pcnbp->somSelf);

    if (flFlags & CBI_INIT)
    {
        // make backup of instance data
        if (pcnbp->pUser == NULL)
        {
            // copy data for "Undo"
            XCenterData *pBackup = (XCenterData*)malloc(sizeof(*somThis));
            memset(pBackup, 0, sizeof(*somThis));
            // be careful about copying... we have some pointers in there!
            pBackup->ulWindowStyle = _ulWindowStyle;
            pBackup->ulAutoHide = _ulAutoHide;

            // store in noteboot struct
            pcnbp->pUser = pBackup;
        }
    }

    if (flFlags & CBI_SET)
    {
        if (_ulPosition == XCENTER_TOP)
            winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_CRDI_VIEW_TOPOFSCREEN, TRUE);
        else
            winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_CRDI_VIEW_BOTTOMOFSCREEN, TRUE);

        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_CRDI_VIEW_ALWAYSONTOP,
                              ((_ulWindowStyle & WS_TOPMOST) != 0));
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_CRDI_VIEW_ANIMATE,
                              ((_ulWindowStyle & WS_ANIMATE) != 0));
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_CRDI_VIEW_AUTOHIDE,
                              (_ulAutoHide > 0));
    }

    if (flFlags & CBI_ENABLE)
    {
    }

    if (flFlags & CBI_DESTROY)
    {
    }
}

/*
 *@@ ctrpViewItemChanged:
 *      notebook callback function (notebook.c) for the
 *      XCenter instance settings page.
 *      Reacts to changes of any of the dialog controls.
 *
 *@@added V0.9.7 (2000-12-05) [umoeller]
 */

MRESULT ctrpViewItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                            USHORT usItemID, USHORT usNotifyCode,
                            ULONG ulExtra)      // for checkboxes: contains new state
{
    MRESULT     mrc = 0;
    XCenterData *somThis = XCenterGetData(pcnbp->somSelf);
    BOOL        fSave = TRUE;

    switch (usItemID)
    {
        case ID_CRDI_VIEW_TOPOFSCREEN:
            _ulPosition = XCENTER_TOP;
        break;

        case ID_CRDI_VIEW_BOTTOMOFSCREEN:
            _ulPosition = XCENTER_BOTTOM;
        break;

        case ID_CRDI_VIEW_ALWAYSONTOP:
            if (ulExtra)
                _ulWindowStyle |= WS_TOPMOST;
            else
                _ulWindowStyle &= ~WS_TOPMOST;
        break;

        case ID_CRDI_VIEW_ANIMATE:
            if (ulExtra)
                _ulWindowStyle |= WS_ANIMATE;
            else
                _ulWindowStyle &= ~WS_ANIMATE;
        break;

        case ID_CRDI_VIEW_AUTOHIDE:
            if (ulExtra)
                _ulAutoHide = 4000;
            else
                _ulAutoHide = 0;
        break;

        case DID_DEFAULT:
            _ulPosition = XCENTER_BOTTOM;
            _ulWindowStyle = WS_TOPMOST | WS_ANIMATE;
            _ulAutoHide = 4000;
            // call the init callback to refresh the page controls
            pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
        break;

        case DID_UNDO:
        {
            XCenterData *pBackup = (XCenterData*)pcnbp->pUser;
            _ulPosition = pBackup->ulPosition;
            _ulWindowStyle = pBackup->ulWindowStyle;
            _ulAutoHide = pBackup->ulAutoHide;
            // call the init callback to refresh the page controls
            pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
        break; }

        default:
            fSave = FALSE;
    }

    if (fSave)
    {
        _wpSaveDeferred(pcnbp->somSelf);

        if (_hwndOpenView)
        {
            // view is currently open:
            PXCENTERWINDATA pXCenterData = WinQueryWindowPtr(_hwndOpenView, QWL_USER);
            ctrpReformatFrame(pXCenterData);
        }
    }

    return (mrc);
}

/* ******************************************************************
 *
 *   "Widgets" page notebook callbacks (notebook.c)
 *
 ********************************************************************/

/*
 *@@ WIDGETRECORD:
 *      extended record core structure for "Widgets" container.
 *
 *@@added V0.9.7 (2000-12-09) [umoeller]
 */

typedef struct _WIDGETRECORD
{
    RECORDCORE      recc;

    ULONG           ulIndex;

    const char      *pcszClass;
            // widget's class name; points into instance data, do not free!

    const char      *pcszSetupString;
            // widget's setup string; points into instance data, do not free!

} WIDGETRECORD, *PWIDGETRECORD;

/*
 *@@ ctrpWidgetsInitPage:
 *      notebook callback function (notebook.c) for the
 *      XCenter instance settings page.
 *      Sets the controls on the page according to the
 *      instance settings.
 *
 *@@added V0.9.7 (2000-12-05) [umoeller]
 */

VOID ctrpWidgetsInitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                         ULONG flFlags)        // CBI_* flags (notebook.h)
{
    XCenterData *somThis = XCenterGetData(pcnbp->somSelf);

    if (flFlags & CBI_INIT)
    {
        HWND hwndCnr = WinWindowFromID(pcnbp->hwndDlgPage, ID_XFDI_CNR_CNR);
        XFIELDINFO      xfi[5];
        PFIELDINFO      pfi = NULL;
        int             i = 0;

        // set group cnr title
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XFDI_CNR_GROUPTITLE,
                          "Wi~dgets");   // ###

        // set up cnr details view
        xfi[i].ulFieldOffset = FIELDOFFSET(WIDGETRECORD, ulIndex);
        xfi[i].pszColumnTitle = "";
        xfi[i].ulDataType = CFA_ULONG;
        xfi[i++].ulOrientation = CFA_LEFT;

        xfi[i].ulFieldOffset = FIELDOFFSET(WIDGETRECORD, pcszClass);
        xfi[i].pszColumnTitle = "Class";
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        xfi[i].ulFieldOffset = FIELDOFFSET(WIDGETRECORD, pcszSetupString);
        xfi[i].pszColumnTitle = "Setup";
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        pfi = cnrhSetFieldInfos(hwndCnr,
                                xfi,
                                i,             // array item count
                                TRUE,          // draw lines
                                1);            // return second column

        BEGIN_CNRINFO()
        {
            cnrhSetView(CV_DETAIL | CA_DETAILSVIEWTITLES);
            cnrhSetSplitBarAfter(pfi);
            cnrhSetSplitBarPos(100);
        } END_CNRINFO(hwndCnr);

        // make backup of instance data
        if (pcnbp->pUser == NULL)
        {
        }
    }

    if (flFlags & CBI_SET)
    {
        HWND        hwndCnr = WinWindowFromID(pcnbp->hwndDlgPage, ID_XFDI_CNR_CNR);
        PLINKLIST   pllWidgets = ctrpQuerySettingsList(pcnbp->somSelf);
        PLISTNODE   pNode = lstQueryFirstNode(pllWidgets);
        ULONG       cWidgets = lstCountItems(pllWidgets),
                    ulIndex = 0;
        PWIDGETRECORD preccFirst;

        cnrhRemoveAll(hwndCnr);

        preccFirst = (PWIDGETRECORD)cnrhAllocRecords(hwndCnr,
                                                     sizeof(WIDGETRECORD),
                                                     cWidgets);

        if (preccFirst)
        {
            PWIDGETRECORD preccThis = preccFirst;

            while (pNode)
            {
                PXCENTERWIDGETSETTING pSetting = (PXCENTERWIDGETSETTING)pNode->pItemData;

                preccThis->ulIndex = ulIndex++;
                preccThis->pcszClass = pSetting->pszWidgetClass;
                preccThis->pcszSetupString = pSetting->pszSetupString;

                preccThis = (PWIDGETRECORD)preccThis->recc.preccNextRecord;
                pNode = pNode->pNext;
            }
        }

        cnrhInsertRecords(hwndCnr,
                          NULL,         // parent
                          (PRECORDCORE)preccFirst,
                          TRUE,         // invalidate
                          NULL,
                          CRA_RECORDREADONLY,
                          cWidgets);
    }

    if (flFlags & CBI_ENABLE)
    {
    }

    if (flFlags & CBI_DESTROY)
    {
    }
}

/*
 *@@ ctrpWidgetsItemChanged:
 *      notebook callback function (notebook.c) for the
 *      XCenter instance settings page.
 *      Reacts to changes of any of the dialog controls.
 *
 *@@added V0.9.7 (2000-12-05) [umoeller]
 */

MRESULT ctrpWidgetsItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                               USHORT usItemID, USHORT usNotifyCode,
                               ULONG ulExtra)      // for checkboxes: contains new state
{
    MRESULT     mrc = 0;
    XCenterData *somThis = XCenterGetData(pcnbp->somSelf);
    BOOL        fSave = TRUE;

    switch (usItemID)
    {

        default:
            fSave = FALSE;
    }

    if (fSave)
    {
        _wpSaveDeferred(pcnbp->somSelf);

        if (_hwndOpenView)
        {
            // view is currently open:
            PXCENTERWINDATA pXCenterData = WinQueryWindowPtr(_hwndOpenView, QWL_USER);
            ctrpReformatFrame(pXCenterData);
        }
    }

    return (mrc);
}

