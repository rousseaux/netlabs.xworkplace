
/*
 *@@sourcefile ctr_notebook.c:
 *      XCenter instance setup and notebook pages.
 *
 *      Function prefix for this file:
 *      --  ctrp* also.
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

#define INCL_DOSEXCEPTIONS
#define INCL_DOSPROCESS
#define INCL_DOSMODULEMGR

#define INCL_WINWINDOWMGR
#define INCL_WINDIALOGS
#define INCL_WINBUTTONS
#define INCL_WINSTDCNR
#define INCL_WINSTDSLIDER
#include <os2.h>

// C library headers
#include <stdio.h>
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\cnrh.h"               // container helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\stringh.h"            // string helper routines
#include "helpers\winh.h"               // PM helper routines
#include "helpers\xstring.h"            // extended string helpers

// SOM headers which don't crash with prec. header files
#include "xcenter.ih"
#include "xfobj.ih"                     // XFldObject

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\notebook.h"            // generic XWorkplace notebook handling
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

#include "shared\center.h"              // public XCenter interfaces
#include "xcenter\centerp.h"            // private XCenter implementation

#pragma hdrstop                     // VAC++ keeps crashing otherwise

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

// class name for wpSaveState/wpRestoreState
static const char           *G_pcszXCenter = "XCenter";

/* ******************************************************************
 *
 *   XCenter setup set (see XWPSETUPENTRY)
 *
 ********************************************************************/

/*
 *@@ G_XCenterSetupSet:
 *      setup set of the XCenter. See XWPSETUPENTRY.
 *
 *      Presently used key values:
 *
 +          1: holds the packed setup string.
 +          2: _ulWindowStyle,
 +          3: _ulAutoHide,
 +          4: _flDisplayStyle,
 +          5: _fHelpDisplayed,
 +          6: _ulPriorityClass,
 +          7: _lPriorityDelta,        // this is a LONG
 +          8: _ulPosition,
 +          9: _ul3DBorderWidth,
 +          10: _ulBorderSpacing,
 +          11: _ulWidgetSpacing,
 +          12: _fReduceDesktopWorkarea
 +          13: _pszClientFont          // V0.9.9 (2001-03-07) [umoeller]
 +
 *@@added V0.9.9 (2001-01-29) [umoeller]
 */

static XWPSETUPENTRY    G_XCenterSetupSet[] =
    {
        /*
         * ulWindowStyle bitfield... first a LONG, then the bitfields
         *
         */

        // type,  setup string,     offset,
        STG_LONG,    NULL,           FIELDOFFSET(XCenterData, ulWindowStyle),
        //     key for wpSaveState/wpRestoreState
               2,      // bitfield! only first item!
        //     default, bitflag,            min, max
               WS_ANIMATE,  0,              0,   0,

        // type,  setup string,     offset,
        STG_BITFLAG, "ALWAYSONTOP",  FIELDOFFSET(XCenterData, ulWindowStyle),
        //     key for wpSaveState/wpRestoreState
               0,      // bitfield! only first item!
        //     default, bitflag,            min, max
               0,       WS_TOPMOST,         0,   0,

        // type,  setup string,     offset,
        STG_BITFLAG, "ANIMATE",     FIELDOFFSET(XCenterData, ulWindowStyle),
        //     key for wpSaveState/wpRestoreState
               0,      // bitfield! only first item!
        //     default, bitflag,            min, max
               WS_ANIMATE,  WS_ANIMATE,     0,   0,    // V0.9.9 (2001-02-28) [pr]: fix setup string default

        /*
         * flDisplayStyle bitfield... first a LONG, then the bitfields
         *
         */

        // type,  setup string,     offset,
        STG_LONG,    NULL,             FIELDOFFSET(XCenterData, flDisplayStyle),
        //     key for wpSaveState/wpRestoreState
               4,      // bitfield! only first item!
        //     default, bitflag,            min, max
               XCS_FLATBUTTONS | XCS_SUNKBORDERS | XCS_SIZINGBARS, 0, 0, 0,

        // type,  setup string,     offset,
        STG_BITFLAG, "FLATBUTTONS",    FIELDOFFSET(XCenterData, flDisplayStyle),
        //     key for wpSaveState/wpRestoreState
               0,      // bitfield! only first item!
        //     default, bitflag,            min, max
               XCS_FLATBUTTONS, XCS_FLATBUTTONS, 0,   0,

        // type,  setup string,     offset,
        STG_BITFLAG, "SUNKBORDERS",    FIELDOFFSET(XCenterData, flDisplayStyle),
        //     key for wpSaveState/wpRestoreState
               0,      // bitfield! only first item!
        //     default, bitflag,            min, max
               XCS_SUNKBORDERS, XCS_SUNKBORDERS, 0,   0,

        // type,  setup string,     offset,
        STG_BITFLAG, "SIZINGBARS",    FIELDOFFSET(XCenterData, flDisplayStyle),
        //     key for wpSaveState/wpRestoreState
               0,      // bitfield! only first item!
        //     default, bitflag,            min, max
               XCS_SIZINGBARS, XCS_SIZINGBARS, 0,   0,

        // type,  setup string,     offset,
        STG_BITFLAG, "ALL3DBORDERS",    FIELDOFFSET(XCenterData, flDisplayStyle),
        //     key for wpSaveState/wpRestoreState
               0,      // bitfield! only first item!
        //     default, bitflag,            min, max
               0, XCS_ALL3DBORDERS, 0,   0,

        /*
         * other LONGs
         *
         */

        // type,  setup string,     offset,
        STG_LONG,    "AUTOHIDE",    FIELDOFFSET(XCenterData, ulAutoHide),
        //     key for wpSaveState/wpRestoreState
               3,
        //     default, bitflag,            min, max
               0,       0,                  0,   4000,

        // type,  setup string,     offset,
        STG_BOOL, NULL,               FIELDOFFSET(XCenterData, fHelpDisplayed),
        //     key for wpSaveState/wpRestoreState
               5,
        //     default, bitflag,            min, max
               FALSE,   0,                  0,   1,

        // type,  setup string,     offset,
        STG_LONG, "PRIORITYCLASS",    FIELDOFFSET(XCenterData, ulPriorityClass),
        //     key for wpSaveState/wpRestoreState
               6,
        //     default, bitflag,            min, max
               PRTYC_REGULAR, 0,            1,   4,
                                            // PRTYC_IDLETIME == 1
                                            // PRTYC_FOREGROUNDSERVER = 4

        // type,  setup string,     offset,
        STG_LONG, "PRIORITYDELTA",    FIELDOFFSET(XCenterData, lPriorityDelta),
        //     key for wpSaveState/wpRestoreState
               7,
        //     default, bitflag,            min, max
               0,       0,                  0,   31,

        // type,  setup string,     offset,
        STG_LONG, NULL,               FIELDOFFSET(XCenterData, ulPosition),
        //     key for wpSaveState/wpRestoreState
               8,
        //     default, bitflag,            min, max
               XCENTER_BOTTOM, 0,           0,   3,

        // type,  setup string,     offset,
        STG_LONG, "3DBORDERWIDTH",    FIELDOFFSET(XCenterData, ul3DBorderWidth),
        //     key for wpSaveState/wpRestoreState
               9,
        //     default, bitflag,            min, max
               1,       0,                  0,   10,

        // type,  setup string,     offset,
        STG_LONG, "BORDERSPACING",    FIELDOFFSET(XCenterData, ulBorderSpacing),
        //     key for wpSaveState/wpRestoreState
               10,
        //     default, bitflag,            min, max
               1,       0,                  0,   10,

        // type,  setup string,     offset,
        STG_LONG, "WIDGETSPACING",    FIELDOFFSET(XCenterData, ulWidgetSpacing),
        //     key for wpSaveState/wpRestoreState
               11,
        //     default, bitflag,            min, max
               2,       0,                  1,   10,

        // type,  setup string,     offset,
        STG_BOOL,    "REDUCEDESKTOP",  FIELDOFFSET(XCenterData, fReduceDesktopWorkarea),
        //     key for wpSaveState/wpRestoreState
               12,
        //     default, bitflag,            min, max
               FALSE,   0,                  0,   0,

        /*
         * fonts/colors
         *      V0.9.9 (2001-03-07) [umoeller]
         */

        // type,  setup string,     offset,
        STG_PSZ,     "CLIENTFONT",  FIELDOFFSET(XCenterData, pszClientFont),
        //     key for wpSaveState/wpRestoreState
               13,
        //     default, bitflag,            min, max
               (LONG)NULL, 0,               0,   0,

        // type,  setup string,     offset,
        STG_LONG,    "CLIENTCOLOR", FIELDOFFSET(XCenterData, lcolClientBackground),
        //     key for wpSaveState/wpRestoreState
               14,
        //     default, bitflag,            min, max
               0xCCCCCC, 0,                 0,   0x00FFFFFF
    };

/*
 *@@ ctrpInitData:
 *      part of the implementation for XCenter::wpInitData.
 *
 *@@added V0.9.9 (2001-01-29) [umoeller]
 */

VOID ctrpInitData(XCenter *somSelf)
{
    XCenterData *somThis = XCenterGetData(somSelf);
    cmnSetupInitData(G_XCenterSetupSet,
                     ARRAYITEMCOUNT(G_XCenterSetupSet),
                     somThis);
}

/*
 *@@ ctrpQuerySetup:
 *      implementation for XCenter::xwpQuerySetup2.
 *
 *@@added V0.9.7 (2000-12-09) [umoeller]
 */

ULONG ctrpQuerySetup(XCenter *somSelf,
                     PSZ pszSetupString,
                     ULONG cbSetupString)
{
    ULONG ulReturn = 0;

    WPSHLOCKSTRUCT Lock;
    if (wpshLockObject(&Lock, somSelf))
    {
        // method pointer for parent class
        somTD_XFldObject_xwpQuerySetup pfn_xwpQuerySetup2 = 0;

        // compose setup string

        TRY_LOUD(excpt1)
        {
            // flag defined in
            #define WP_GLOBAL_COLOR         0x40000000

            XCenterData *somThis = XCenterGetData(somSelf);

            // temporary buffer for building the setup string
            CHAR szTemp[100];
            XSTRING strTemp;
            PLINKLIST pllSettings = ctrpQuerySettingsList(somSelf);
            PLISTNODE pNode;

            xstrInit(&strTemp, 400);

            /*
             * build string
             *
             */

            if (_ulPosition == XCENTER_TOP)
                xstrcat(&strTemp, "POSITION=TOP;", 0);

            // use array for the rest...
            cmnSetupBuildString(G_XCenterSetupSet,
                                ARRAYITEMCOUNT(G_XCenterSetupSet),
                                somThis,
                                &strTemp);

            // now build widgets string... this is complex.
            pNode = lstQueryFirstNode(pllSettings);
            if (pNode)
            {
                BOOL    fFirstWidget = TRUE;
                xstrcat(&strTemp, "WIDGETS=", 0);

                // we have widgets:
                // go thru all of them and list all widget classes and setup strings.
                while (pNode)
                {
                    PXCENTERWIDGETSETTING pSetting = (PXCENTERWIDGETSETTING)pNode->pItemData;

                    if (!fFirstWidget)
                        // not first run:
                        // add separator
                        xstrcatc(&strTemp, ',');
                    else
                        fFirstWidget = FALSE;

                    // add widget class
                    xstrcat(&strTemp, pSetting->pszWidgetClass, 0);

                    if (    (pSetting->pszSetupString)
                         && (strlen(pSetting->pszSetupString))
                       )
                    {
                        // widget has a setup string:
                        // add that in brackets
                        XSTRING strSetup2;

                        // characters that must be encoded
                        // CHAR    achEncode[] = ;

                        ULONG   ul = 0;

                        // copy widget setup string to temporary buffer
                        // for encoding... this has "=" and ";"
                        // chars in it, and these should not appear
                        // in the WPS setup string
                        xstrInitCopy(&strSetup2,
                                     pSetting->pszSetupString,
                                     40);

                        // add first separator
                        xstrcatc(&strTemp, '(');

                        xstrEncode(&strSetup2,
                                   "%,();=");

                        // now append encoded widget setup string
                        xstrcat(&strTemp, strSetup2.psz, strSetup2.ulLength);

                        // add terminator
                        xstrcatc(&strTemp, ')');

                        xstrClear(&strSetup2);
                    } // end if (    (pSetting->pszSetupString)...

                    pNode = pNode->pNext;
                } // end for widgets

                xstrcatc(&strTemp, ';');
            }

            /*
             * append string
             *
             */

            if (strTemp.ulLength)
            {
                // return string if buffer is given
                if ((pszSetupString) && (cbSetupString))
                    strhncpy0(pszSetupString,   // target
                              strTemp.psz,      // source
                              cbSetupString);   // buffer size

                // always return length of string
                ulReturn = strTemp.ulLength;
            }

            xstrClear(&strTemp);
        }
        CATCH(excpt1)
        {
            ulReturn = 0;
        } END_CATCH();

        // manually resolve parent method
        pfn_xwpQuerySetup2
            = (somTD_XFldObject_xwpQuerySetup)wpshResolveFor(somSelf,
                                                             _somGetParent(_XCenter),
                                                             "xwpQuerySetup2");
        if (pfn_xwpQuerySetup2)
        {
            // now call parent method
            if ( (pszSetupString) && (cbSetupString) )
                // string buffer already specified:
                // tell parent to append to that string
                ulReturn += pfn_xwpQuerySetup2(somSelf,
                                               pszSetupString + ulReturn, // append to existing
                                               cbSetupString - ulReturn); // remaining size
            else
                // string buffer not yet specified: return length only
                ulReturn += pfn_xwpQuerySetup2(somSelf, 0, 0);
        }
    }
    wpshUnlockObject(&Lock);

    return (ulReturn);
}

/*
 *@@ ctrpSetup:
 *      implementation for XCenter::wpSetup.
 *
 *@@added V0.9.7 (2001-01-25) [umoeller]
 */

BOOL ctrpSetup(XCenter *somSelf,
               PSZ pszSetupString)
{
    XCenterData *somThis = XCenterGetData(somSelf);
    ULONG   cSuccess = 0;

    // scan the standard stuff from the table...
    // this saves us a lot of work.
    BOOL    brc = cmnSetupScanString(somSelf,
                                     G_XCenterSetupSet,
                                     ARRAYITEMCOUNT(G_XCenterSetupSet),
                                     somThis,
                                     pszSetupString,
                                     &cSuccess);

    // now comes the non-standard stuff:

    if (brc)
    {
        CHAR    szValue[100];
        ULONG   cb = sizeof(szValue);
        if (_wpScanSetupString(somSelf,
                               pszSetupString,
                               "POSITION",
                               szValue,
                               &cb))
        {
            if (!stricmp(szValue, "TOP"))
                _ulPosition = XCENTER_TOP;
            else if (!stricmp(szValue, "BOTTOM"))
                _ulPosition = XCENTER_BOTTOM;
            else
                brc = FALSE;
        }
    }

    if (brc)
    {
        // WIDGETS can be very long, so query size first
        ULONG   cb = 0;
        if (_wpScanSetupString(somSelf,
                               pszSetupString,
                               "WIDGETS",
                               NULL,
                               &cb))
        {
            PSZ pszWidgets = malloc(cb);
            if (    (pszWidgets)
                 && (_wpScanSetupString(somSelf,
                                        pszSetupString,
                                        "WIDGETS",
                                        pszWidgets,
                                        &cb))
               )
            {
                // now, off we go...

                // now parse the WIDGETS string
                // format is: "widget1,widget2,widget3"
                PSZ pszToken = strtok(pszWidgets, ",");
                if (pszToken)
                {
                    // first of all, remove all existing widgets,
                    // we have a replacement here
                    while (_xwpRemoveWidget(somSelf,
                                            0))
                               ;

                    // now take the widgets
                    do
                    {
                        // pszToken now has one widget
                        PSZ pszWidgetClass = NULL;
                        PSZ pszWidgetSetup = NULL;
                        // check if this has brackets
                        PSZ pBracket = strchr(pszToken, '(');
                        if (pBracket)
                        {
                            pszWidgetClass = strhSubstr(pszToken, pBracket);
                            // extract setup
                            pszWidgetSetup = strhExtract(pszToken, '(', ')', NULL);

                            // V0.9.9 (2001-03-03) [pr]
                            if (pszWidgetSetup)
                            {
                                XSTRING strSetup2;
                                // copy widget setup string to temporary
                                // buffer for decoding...
                                xstrInitSet(&strSetup2,
                                            pszWidgetSetup);
                                xstrDecode(&strSetup2);
                                pszWidgetSetup = strSetup2.psz;
                            } // end // V0.9.9 (2001-03-03) [pr]
                        }
                        else
                            // no setup string:
                            pszWidgetClass = strdup(pszToken);

                        // OK... set up the widget now

                        if (!_xwpInsertWidget(somSelf,
                                              -1,            // to the right
                                              pszWidgetClass,
                                              pszWidgetSetup))
                            brc = FALSE;

                        // V0.9.9 (2001-03-03) [pr]: fix memory leak
                        if (pszWidgetClass)
                            free(pszWidgetClass);

                        if (pszWidgetSetup)
                            free(pszWidgetSetup);

                    } while (brc && (pszToken = strtok(NULL, ",")));

                } // if (pszToken)

                free(pszWidgets);
            } // end if (    (pszWidgets)...
        }
    }

    return (brc);
}

/*
 *@@ ctrpSaveState:
 *      implementation for XCenter::wpSaveState.
 *
 *@@added V0.9.9 (2001-01-29) [umoeller]
 */

BOOL ctrpSaveState(XCenter *somSelf)
{
    BOOL brc = TRUE;

    TRY_LOUD(excpt1)
    {
        XCenterData *somThis = XCenterGetData(somSelf);

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

        cmnSetupSave(somSelf,
                     G_XCenterSetupSet,
                     ARRAYITEMCOUNT(G_XCenterSetupSet),
                     G_pcszXCenter,     // class name
                     somThis);
    }
    CATCH(excpt1)
    {
        brc = FALSE;
    } END_CATCH();

    return (brc);
}

/*
 *@@ ctrpRestoreState:
 *
 *@@added V0.9.9 (2001-01-29) [umoeller]
 */

BOOL ctrpRestoreState(XCenter *somSelf)
{
    BOOL brc = FALSE;

    TRY_LOUD(excpt1)
    {
        XCenterData *somThis = XCenterGetData(somSelf);

        /*
         * key 1: widget settings
         *
         */

        BOOL    fError = FALSE;

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

        cmnSetupRestore(somSelf,
                        G_XCenterSetupSet,
                        ARRAYITEMCOUNT(G_XCenterSetupSet),
                        G_pcszXCenter,     // class name
                        somThis);
    }
    CATCH(excpt1)
    {
        brc = FALSE;
    } END_CATCH();

    return (brc);
}

/* ******************************************************************
 *
 *   "View" page notebook callbacks (notebook.c)
 *
 ********************************************************************/

// two arrays to specify the affected settings with
// each "view" page; used with cmnSetupSetDefaults etc.

static ULONG    G_aulView1SetupOffsets[]
    = {
            FIELDOFFSET(XCenterData, fReduceDesktopWorkarea),
            FIELDOFFSET(XCenterData, ulPosition),
            FIELDOFFSET(XCenterData, ulWindowStyle),
            FIELDOFFSET(XCenterData, ulAutoHide),
            FIELDOFFSET(XCenterData, lPriorityDelta)   // V0.9.9 (2001-02-28) [pr]: added
      };

static ULONG    G_aulView2SetupOffsets[]
    = {
            FIELDOFFSET(XCenterData, flDisplayStyle),
            FIELDOFFSET(XCenterData, ul3DBorderWidth),
            FIELDOFFSET(XCenterData, ulBorderSpacing),
            FIELDOFFSET(XCenterData, ulWidgetSpacing)
      };

static BOOL     G_fSetting = FALSE;

/*
 *@@ ctrpView1InitPage:
 *      notebook callback function (notebook.c) for the
 *      first XCenter "View" instance settings page.
 *      Sets the controls on the page according to the
 *      instance settings.
 *
 *@@added V0.9.7 (2000-12-05) [umoeller]
 *@@changed V0.9.9 (2001-01-29) [umoeller]: "Undo" data wasn't working
 *@@changed V0.9.9 (2001-03-09) [umoeller]: added auto-hide delay slider
 */

VOID ctrpView1InitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
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
            memcpy(pBackup, somThis, sizeof(*somThis));
            // be careful about using the copy... we have some pointers in there!

            // store in noteboot struct
            pcnbp->pUser = pBackup;
        }

        winhSetSliderTicks(WinWindowFromID(pcnbp->hwndDlgPage,
                                           ID_CRDI_VIEW_AUTOHIDE_SLIDER),
                           MPFROM2SHORT(9, 10), 6,
                           (MPARAM)-1, -1);

        winhSetSliderTicks(WinWindowFromID(pcnbp->hwndDlgPage,
                                           ID_CRDI_VIEW_PRTY_SLIDER),
                           (MPARAM)0, 3,
                           MPFROM2SHORT(9, 10), 6);
    }

    if (flFlags & CBI_SET)
    {
        LONG lSliderIndex = 0;
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_CRDI_VIEW_REDUCEWORKAREA,
                              _fReduceDesktopWorkarea);

        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_CRDI_VIEW_ALWAYSONTOP,
                              ((_ulWindowStyle & WS_TOPMOST) != 0));
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_CRDI_VIEW_ANIMATE,
                              ((_ulWindowStyle & WS_ANIMATE) != 0));

        // autohide
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_CRDI_VIEW_AUTOHIDE,
                              (_ulAutoHide > 0));
        if (_ulAutoHide)
            lSliderIndex = (_ulAutoHide / 1000) - 1;

        // prevent the stupid slider control from interfering
        G_fSetting = TRUE;
        winhSetSliderArmPosition(WinWindowFromID(pcnbp->hwndDlgPage,
                                                 ID_CRDI_VIEW_AUTOHIDE_SLIDER),
                                 SMA_INCREMENTVALUE,
                                 lSliderIndex);
        G_fSetting = FALSE;

        WinSetDlgItemShort(pcnbp->hwndDlgPage,
                           ID_CRDI_VIEW_AUTOHIDE_TXT2,
                           _ulAutoHide / 1000,
                           FALSE);      // unsigned

        // priority
        winhSetSliderArmPosition(WinWindowFromID(pcnbp->hwndDlgPage,
                                                 ID_CRDI_VIEW_PRTY_SLIDER),
                                 SMA_INCREMENTVALUE,
                                 _lPriorityDelta);
        WinSetDlgItemShort(pcnbp->hwndDlgPage,
                           ID_CRDI_VIEW_PRTY_TEXT,
                           _lPriorityDelta,
                           FALSE);      // unsigned

        if (_ulPosition == XCENTER_TOP)
            winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_CRDI_VIEW_TOPOFSCREEN, TRUE);
        else
            winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_CRDI_VIEW_BOTTOMOFSCREEN, TRUE);

    }

    if (flFlags & CBI_ENABLE)
    {
        BOOL fAutoHide =    (_fReduceDesktopWorkarea == FALSE)
                         && (_ulAutoHide != 0);

        // disable auto-hide if workarea is to be reduced
        WinEnableControl(pcnbp->hwndDlgPage, ID_CRDI_VIEW_AUTOHIDE,
                         (_fReduceDesktopWorkarea == FALSE));
        WinEnableControl(pcnbp->hwndDlgPage, ID_CRDI_VIEW_AUTOHIDE_TXT1,
                         fAutoHide);
        WinEnableControl(pcnbp->hwndDlgPage, ID_CRDI_VIEW_AUTOHIDE_SLIDER,
                         fAutoHide);
        WinEnableControl(pcnbp->hwndDlgPage, ID_CRDI_VIEW_AUTOHIDE_TXT2,
                         fAutoHide);
    }
}

/*
 *@@ ctrpView1ItemChanged:
 *      notebook callback function (notebook.c) for the
 *      XCenter "View"  instance settings page.
 *      Reacts to changes of any of the dialog controls.
 *
 *@@added V0.9.7 (2000-12-05) [umoeller]
 *@@changed V0.9.9 (2001-01-29) [umoeller]: now using cmnSetup* funcs
 *@@changed V0.9.9 (2001-03-09) [umoeller]: added auto-hide delay slider
 */

MRESULT ctrpView1ItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                             USHORT usItemID, USHORT usNotifyCode,
                             ULONG ulExtra)      // for checkboxes: contains new state
{
    MRESULT     mrc = 0;
    XCenterData *somThis = XCenterGetData(pcnbp->somSelf);
    BOOL        fSave = TRUE;
    ULONG       ulCallInitCallback = 0;

    ULONG       ulUpdateFlags = XFMF_DISPLAYSTYLECHANGED;

    switch (usItemID)
    {
        case ID_CRDI_VIEW_REDUCEWORKAREA:
            _fReduceDesktopWorkarea = ulExtra;
            if (_ulAutoHide)
                // this conflicts with auto-hide...
                // disable that.
                _ulAutoHide = 0;
            // renable items
            ulCallInitCallback = CBI_SET | CBI_ENABLE;
        break;

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
            ulCallInitCallback = CBI_SET | CBI_ENABLE;
            ulUpdateFlags = 0;      // no complete reformat
        break;

        case ID_CRDI_VIEW_AUTOHIDE_SLIDER:
            // we get this message even if the init callback is
            // setting this... stupid, stupid slider control
            if (!G_fSetting)
            {
                LONG lSliderIndex = winhQuerySliderArmPosition(pcnbp->hwndControl,
                                                               SMA_INCREMENTVALUE);
                WinSetDlgItemShort(pcnbp->hwndDlgPage,
                                   ID_CRDI_VIEW_AUTOHIDE_TXT2,
                                   lSliderIndex + 1,
                                   FALSE);      // unsigned
                _ulAutoHide = (lSliderIndex + 1) * 1000;
                            // range is 0 thru 59
                ulUpdateFlags = 0;      // no complete reformat
            }
        break;

        case ID_CRDI_VIEW_PRTY_SLIDER:
        {
            LONG lSliderIndex = winhQuerySliderArmPosition(pcnbp->hwndControl,
                                                           SMA_INCREMENTVALUE);
            WinSetDlgItemShort(pcnbp->hwndDlgPage,
                               ID_CRDI_VIEW_PRTY_TEXT,
                               lSliderIndex,
                               FALSE);      // unsigned
            // _lPriorityDelta = lSliderIndex;
            _xwpSetPriority(pcnbp->somSelf,
                            _ulPriorityClass,        // unchanged
                            lSliderIndex);
            ulUpdateFlags = 0;
        break; }

        case DID_DEFAULT:
            cmnSetupSetDefaults(G_XCenterSetupSet,
                                ARRAYITEMCOUNT(G_XCenterSetupSet),
                                G_aulView1SetupOffsets,
                                ARRAYITEMCOUNT(G_aulView1SetupOffsets),
                                somThis);
            ulCallInitCallback = CBI_SET | CBI_ENABLE;
        break;

        case DID_UNDO:
        {
            XCenterData *pBackup = (XCenterData*)pcnbp->pUser;
            cmnSetupRestoreBackup(G_aulView1SetupOffsets,
                                  ARRAYITEMCOUNT(G_aulView1SetupOffsets),
                                  somThis,
                                  pBackup);
            ulCallInitCallback = CBI_SET | CBI_ENABLE;
        break; }

        default:
            fSave = FALSE;
    }

    if (ulCallInitCallback)
        // call the init callback to refresh the page controls
        pcnbp->pfncbInitPage(pcnbp, ulCallInitCallback);

    if (fSave)
    {
        _wpSaveDeferred(pcnbp->somSelf);

        if (_pvOpenView)
        {
            // view is currently open:
            PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)_pvOpenView;
            // this can be on a different thread, so post msg
            WinPostMsg(pXCenterData->Globals.hwndClient,
                       XCM_REFORMAT,
                       (MPARAM)ulUpdateFlags,
                       0);
        }
    }

    return (mrc);
}

/*
 *@@ ctrpView2InitPage:
 *      notebook callback function (notebook.c) for the
 *      first XCenter "View" instance settings page.
 *      Sets the controls on the page according to the
 *      instance settings.
 *
 *@@added V0.9.7 (2000-12-05) [umoeller]
 *@@changed V0.9.9 (2001-01-29) [umoeller]: "Undo" data wasn't working
 */

VOID ctrpView2InitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
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
            memcpy(pBackup, somThis, sizeof(*somThis));
            // be careful about using the copy... we have some pointers in there!

            // store in noteboot struct
            pcnbp->pUser = pBackup;
        }

        winhSetSliderTicks(WinWindowFromID(pcnbp->hwndDlgPage,
                                           ID_CRDI_VIEW2_3DBORDER_SLIDER),
                           (MPARAM)0, 3,
                           MPFROM2SHORT(4, 5), 6);

        winhSetSliderTicks(WinWindowFromID(pcnbp->hwndDlgPage,
                                           ID_CRDI_VIEW2_BDRSPACE_SLIDER),
                           (MPARAM)0, 3,
                           MPFROM2SHORT(4, 5), 6);

        winhSetSliderTicks(WinWindowFromID(pcnbp->hwndDlgPage,
                                           ID_CRDI_VIEW2_WGTSPACE_SLIDER),
                           (MPARAM)0, 3,
                           MPFROM2SHORT(4, 5), 6);
    }

    if (flFlags & CBI_SET)
    {
        HWND hwndSlider;

        // 3D borders
        hwndSlider = WinWindowFromID(pcnbp->hwndDlgPage, ID_CRDI_VIEW2_3DBORDER_SLIDER);
        winhSetSliderArmPosition(hwndSlider,
                                 SMA_INCREMENTVALUE,
                                 _ul3DBorderWidth);     // slider scale is from 0 to 10
        WinSetDlgItemShort(pcnbp->hwndDlgPage,
                           ID_CRDI_VIEW_PRTY_TEXT,
                           _ul3DBorderWidth,
                           FALSE);      // unsigned
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_CRDI_VIEW2_ALL3DBORDERS,
                              ((_flDisplayStyle & XCS_ALL3DBORDERS) != 0));

        // border spacing
        hwndSlider = WinWindowFromID(pcnbp->hwndDlgPage, ID_CRDI_VIEW2_BDRSPACE_SLIDER);
        winhSetSliderArmPosition(hwndSlider,
                                 SMA_INCREMENTVALUE,
                                 _ulBorderSpacing);     // slider scale is from 0 to 10
        WinSetDlgItemShort(pcnbp->hwndDlgPage,
                           ID_CRDI_VIEW_PRTY_TEXT,
                           _ulBorderSpacing,
                           FALSE);      // unsigned

        // widget spacing
        hwndSlider = WinWindowFromID(pcnbp->hwndDlgPage, ID_CRDI_VIEW2_WGTSPACE_SLIDER);
        winhSetSliderArmPosition(hwndSlider,
                                 SMA_INCREMENTVALUE,
                                 _ulWidgetSpacing - 1);     // slider scale is from 0 to 9
        WinSetDlgItemShort(pcnbp->hwndDlgPage,
                           ID_CRDI_VIEW_PRTY_TEXT,
                           _ulWidgetSpacing,
                           FALSE);      // unsigned
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_CRDI_VIEW2_SIZINGBARS,
                              ((_flDisplayStyle & XCS_SIZINGBARS) != 0));


        // default widget styles
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_CRDI_VIEW2_FLATBUTTONS,
                            ((_flDisplayStyle & XCS_FLATBUTTONS) != 0));

        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_CRDI_VIEW2_SUNKBORDERS,
                            ((_flDisplayStyle & XCS_SUNKBORDERS) != 0));
    }
}

/*
 *@@ ctrpView2ItemChanged:
 *      notebook callback function (notebook.c) for the
 *      XCenter "View"  instance settings page.
 *      Reacts to changes of any of the dialog controls.
 *
 *@@added V0.9.7 (2000-12-05) [umoeller]
 *@@changed V0.9.9 (2001-01-29) [umoeller]: now using cmnSetup* funcs
 */

MRESULT ctrpView2ItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                             USHORT usItemID, USHORT usNotifyCode,
                             ULONG ulExtra)      // for checkboxes: contains new state
{
    MRESULT     mrc = 0;
    XCenterData *somThis = XCenterGetData(pcnbp->somSelf);
    BOOL        fSave = TRUE,
                fDisplayStyleChanged = FALSE;
    LONG        lSliderIndex;
    ULONG       ulDisplayFlagChanged = 0;

    switch (usItemID)
    {
        case ID_CRDI_VIEW2_3DBORDER_SLIDER:
            lSliderIndex = winhQuerySliderArmPosition(pcnbp->hwndControl,
                                                      SMA_INCREMENTVALUE);
            WinSetDlgItemShort(pcnbp->hwndDlgPage,
                               ID_CRDI_VIEW2_3DBORDER_TEXT,
                               lSliderIndex,
                               FALSE);      // unsigned
            _ul3DBorderWidth = lSliderIndex;
        break;

        case ID_CRDI_VIEW2_ALL3DBORDERS:
            ulDisplayFlagChanged = XCS_ALL3DBORDERS;
        break;

        case ID_CRDI_VIEW2_BDRSPACE_SLIDER:
            lSliderIndex = winhQuerySliderArmPosition(pcnbp->hwndControl,
                                                      SMA_INCREMENTVALUE);
            WinSetDlgItemShort(pcnbp->hwndDlgPage,
                               ID_CRDI_VIEW2_BDRSPACE_TEXT,
                               lSliderIndex,
                               FALSE);      // unsigned
            _ulBorderSpacing = lSliderIndex;
        break;

        case ID_CRDI_VIEW2_WGTSPACE_SLIDER:
            lSliderIndex = winhQuerySliderArmPosition(pcnbp->hwndControl,
                                                      SMA_INCREMENTVALUE);
            WinSetDlgItemShort(pcnbp->hwndDlgPage,
                               ID_CRDI_VIEW2_WGTSPACE_TEXT,
                               lSliderIndex + 1,
                               FALSE);      // unsigned
            _ulWidgetSpacing = lSliderIndex + 1;
        break;

        case ID_CRDI_VIEW2_SIZINGBARS:
            ulDisplayFlagChanged = XCS_SIZINGBARS;
        break;

        case ID_CRDI_VIEW2_FLATBUTTONS:
            ulDisplayFlagChanged = XCS_FLATBUTTONS;
        break;

        case ID_CRDI_VIEW2_SUNKBORDERS:
            ulDisplayFlagChanged = XCS_SUNKBORDERS;
        break;

        case DID_DEFAULT:
            cmnSetupSetDefaults(G_XCenterSetupSet,
                                ARRAYITEMCOUNT(G_XCenterSetupSet),
                                G_aulView2SetupOffsets,
                                ARRAYITEMCOUNT(G_aulView2SetupOffsets),
                                somThis);
            // call the init callback to refresh the page controls
            pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
        break;

        case DID_UNDO:
        {
            XCenterData *pBackup = (XCenterData*)pcnbp->pUser;
            cmnSetupRestoreBackup(G_aulView2SetupOffsets,
                                  ARRAYITEMCOUNT(G_aulView2SetupOffsets),
                                  somThis,
                                  pBackup);
            // call the init callback to refresh the page controls
            pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
        break; }

        default:
            fSave = FALSE;
    }

    if (ulDisplayFlagChanged)
    {
        if (ulExtra)
            _flDisplayStyle |= ulDisplayFlagChanged;
        else
            _flDisplayStyle &= ~ulDisplayFlagChanged;
    }

    if (fSave)
    {
        _wpSaveDeferred(pcnbp->somSelf);

        if (_pvOpenView)
        {
            // view is currently open:
            PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)_pvOpenView;
            // this can be on a different thread, so post msg
            WinPostMsg(pXCenterData->Globals.hwndClient,
                       XCM_REFORMAT,
                       (MPARAM)XFMF_DISPLAYSTYLECHANGED,
                       0);
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

    // const char      *pcszClass;
            // widget's class name; points into instance data, do not free!
            // we use pszIcon for that

    const char      *pcszSetupString;
            // widget's setup string; points into instance data, do not free!

} WIDGETRECORD, *PWIDGETRECORD;

/*
 *@@ ctrpWidgetsInitPage:
 *      notebook callback function (notebook.c) for the
 *      XCenter "Widgets" instance settings page.
 *      Sets the controls on the page according to the
 *      instance settings.
 *
 *@@added V0.9.9 (2001-03-09) [umoeller]
 */

VOID ctrpWidgetsInitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                         ULONG flFlags)        // CBI_* flags (notebook.h)
{
    XCenterData *somThis = XCenterGetData(pcnbp->somSelf);

    if (flFlags & CBI_INIT)
    {
        PNLSSTRINGS     pNLSStrings = cmnQueryNLSStrings();
        HWND hwndCnr = WinWindowFromID(pcnbp->hwndDlgPage, ID_XFDI_CNR_CNR);
        XFIELDINFO      xfi[5];
        PFIELDINFO      pfi = NULL;
        int             i = 0;

        // set group cnr title
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XFDI_CNR_GROUPTITLE,
                          pNLSStrings->pszWidgetsPage);

        // set up cnr details view
        xfi[i].ulFieldOffset = FIELDOFFSET(WIDGETRECORD, ulIndex);
        xfi[i].pszColumnTitle = "";
        xfi[i].ulDataType = CFA_ULONG;
        xfi[i++].ulOrientation = CFA_LEFT;

        xfi[i].ulFieldOffset = FIELDOFFSET(RECORDCORE, pszIcon);
        xfi[i].pszColumnTitle = pNLSStrings->pszWidgetClass; // "Class";
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        xfi[i].ulFieldOffset = FIELDOFFSET(WIDGETRECORD, pcszSetupString);
        xfi[i].pszColumnTitle = pNLSStrings->pszWidgetSetup; // "Setup";
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        pfi = cnrhSetFieldInfos(hwndCnr,
                                xfi,
                                i,             // array item count
                                TRUE,          // draw lines
                                1);            // return second column

        BEGIN_CNRINFO()
        {
            cnrhSetView(CV_DETAIL | CA_DETAILSVIEWTITLES
                             | CA_ORDEREDTARGETEMPH); // target emphasis between records
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
                preccThis->recc.pszIcon = pSetting->pszWidgetClass;
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

// define a new rendering mechanism, which only
// our own container supports (this will make
// sure that we can only do d'n'd within this
// one container)
#define WIDGET_RMF_MECH     "DRM_XWPXCENTERWIDGET"
#define WIDGET_RMF_FORMAT   "DRF_XWPWIDGETRECORD"
#define WIDGET_PRIVATE_RMF  "(" WIDGET_RMF_MECH ")x(" WIDGET_RMF_FORMAT ")"

static PWIDGETRECORD G_precDragged = NULL,
                     G_precAfter = NULL;

/*
 *@@ ctrpWidgetsItemChanged:
 *      notebook callback function (notebook.c) for the
 *      XCenter "Widgets" instance settings page.
 *      Reacts to changes of any of the dialog controls.
 *
 *@@added V0.9.9 (2001-03-09) [umoeller]
 */

MRESULT ctrpWidgetsItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                               USHORT usItemID, USHORT usNotifyCode,
                               ULONG ulExtra)      // for checkboxes: contains new state
{
    MRESULT     mrc = 0;
    XCenterData *somThis = XCenterGetData(pcnbp->somSelf);

    switch (usItemID)
    {
        case ID_XFDI_CNR_CNR:
            switch (usNotifyCode)
            {
                /*
                 * CN_INITDRAG:
                 *      user begins dragging a widget
                 */

                case CN_INITDRAG:
                {
                    PCNRDRAGINIT pcdi = (PCNRDRAGINIT)ulExtra;
                    if (DrgQueryDragStatus())
                        // (lazy) drag currently in progress: stop
                        break;

                    if (pcdi)
                        // filter out whitespace
                        if (pcdi->pRecord)
                        {
                            cnrhInitDrag(pcdi->hwndCnr,
                                         pcdi->pRecord,
                                         usNotifyCode,
                                         WIDGET_PRIVATE_RMF,
                                         DO_MOVEABLE);
                        }
                break; }

                /*
                 * CN_DRAGAFTER:
                 *      something's being dragged over the widgets cnr;
                 *      we allow dropping only for widget records being
                 *      dragged _within_ the cnr.
                 *
                 *      Note that since we have set CA_ORDEREDTARGETEMPH
                 *      for the "Assocs" cnr, we do not get CN_DRAGOVER,
                 *      but CN_DRAGAFTER only.
                 *
                 *      PMREF doesn't say which record is really in
                 *      in the CNRDRAGINFO. From my testing, that field
                 *      is set to CMA_FIRST if the source record is
                 *      dragged before the first record; it is set to
                 *      the record pointer _before_ the record being
                 *      dragged for any other record.
                 *
                 *      In other words, if the draggee is after the
                 *      first record, we get the first record in
                 *      CNRDRAGINFO; if it's after the second, we
                 *      get the second, and so on.
                 */

                case CN_DRAGAFTER:
                {
                    PCNRDRAGINFO pcdi = (PCNRDRAGINFO)ulExtra;
                    USHORT      usIndicator = DOR_NODROP,
                                    // cannot be dropped, but send
                                    // DM_DRAGOVER again
                                usOp = DO_UNKNOWN;
                                    // target-defined drop operation:
                                    // user operation (we don't want
                                    // the WPS to copy anything)

                    // reset global variable
                    G_precDragged = NULL;

                    // get access to the drag'n'drop structures
                    if (DrgAccessDraginfo(pcdi->pDragInfo))
                    {
                        if (
                                // accept no more than one single item at a time;
                                // we cannot move more than one file type
                                (pcdi->pDragInfo->cditem != 1)
                                // make sure that we only accept drops from ourselves,
                                // not from other windows
                             || (pcdi->pDragInfo->hwndSource
                                        != pcnbp->hwndControl)
                            )
                        {
                            usIndicator = DOR_NEVERDROP;
                        }
                        else
                        {
                            // OK, we have exactly one item:
                            PDRAGITEM pdrgItem = NULL;

                            if (    (    (pcdi->pDragInfo->usOperation == DO_DEFAULT)
                                      || (pcdi->pDragInfo->usOperation == DO_MOVE)
                                    )
                                    // do not allow drag upon whitespace,
                                    // but only between records
                                 && (pcdi->pRecord)
                                 && (pdrgItem = DrgQueryDragitemPtr(pcdi->pDragInfo, 0))
                               )
                            {
                                PWIDGETRECORD precDragged = (PWIDGETRECORD)pdrgItem->ulItemID;

                                G_precAfter = NULL;
                                // check target record...
                                if (   (pcdi->pRecord == (PRECORDCORE)CMA_FIRST)
                                    || (pcdi->pRecord == (PRECORDCORE)CMA_LAST)
                                   )
                                    // store record after which to insert
                                    G_precAfter = (PWIDGETRECORD)pcdi->pRecord;
                                // do not allow dropping after
                                // disabled records
                                else if ((pcdi->pRecord->flRecordAttr & CRA_DISABLED) == 0)
                                    // store record after which to insert
                                    G_precAfter = (PWIDGETRECORD)pcdi->pRecord;

                                if (G_precAfter)
                                {
                                    if (    ((PWIDGETRECORD)pcdi->pRecord  // target recc
                                              != precDragged) // source recc
                                         && (DrgVerifyRMF(pdrgItem,
                                                          WIDGET_RMF_MECH,
                                                          WIDGET_RMF_FORMAT))
                                       )
                                    {
                                        // allow drop:
                                        // store record being dragged
                                        G_precDragged = precDragged;
                                        // G_precAfter already set
                                        usIndicator = DOR_DROP;
                                        usOp = DO_MOVE;
                                    }
                                }
                            }
                        }

                        DrgFreeDraginfo(pcdi->pDragInfo);
                    }

                    // and return the drop flags
                    mrc = (MRFROM2SHORT(usIndicator, usOp));
                break; }

                /*
                 * CN_DROP:
                 *      something _has_ now been dropped on the cnr.
                 */

                case CN_DROP:
                {
                    // check the global variable which has been set
                    // by CN_DRAGAFTER above:
                    if (G_precDragged)
                    {
                        // CN_DRAGOVER above has considered this valid:
                        if (G_precAfter)
                        {
                            ULONG ulIndex = -2;

                            if (G_precAfter == (PWIDGETRECORD)CMA_FIRST)
                            {
                                // move before index 0
                                ulIndex = 0;
                            }
                            else if (G_precAfter == (PWIDGETRECORD)CMA_LAST)
                            {
                                // shouldn't happen
                                DosBeep(100, 100);
                            }
                            else
                            {
                                // we get the record _before_ the draggee
                                // (CN_DRAGAFTER), but xwpMoveWidget wants
                                // the index of the widget _before_ which the
                                // widget should be inserted:
                                ulIndex = G_precAfter->ulIndex + 1;
                            }
                            if (ulIndex != -2)
                                // OK... move the widgets around:
                                _xwpMoveWidget(pcnbp->somSelf,
                                               G_precDragged->ulIndex,  // from
                                               ulIndex);                // to
                                    // this saves the instance data
                                    // and updates the view
                                    // and also calls the init callback
                                    // to update the settings page!
                        }
                        G_precDragged = NULL;
                    }
                break; }

            } // end switch (usNotifyCode)
        break;
    }

    return (mrc);
}

/* ******************************************************************
 *
 *   "Classes" page notebook callbacks (notebook.c)
 *
 ********************************************************************/

/*
 *@@ XCLASSRECORD:
 *      extended record core structure for "Classes" container.
 *
 *@@added V0.9.9 (2001-03-09) [umoeller]
 */

typedef struct _XCLASSRECORD
{
    RECORDCORE      recc;

    PSZ             pszDLL;         // points to szDLL
    CHAR            szDLL[CCHMAXPATH];
    PSZ             pszClass;
    PSZ             pszClassTitle;
    PSZ             pszVersion;     // points to szVersion
    CHAR            szVersion[40];
} XCLASSRECORD, *PXCLASSRECORD;

/*
 *@@ ctrpClassesInitPage:
 *      notebook callback function (notebook.c) for the
 *      XCenter "Widgets" instance settings page.
 *      Sets the controls on the page according to the
 *      instance settings.
 *
 *@@added V0.9.7 (2000-12-05) [umoeller]
 */

VOID ctrpClassesInitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                         ULONG flFlags)        // CBI_* flags (notebook.h)
{
    XCenterData *somThis = XCenterGetData(pcnbp->somSelf);

    if (flFlags & CBI_INIT)
    {
        PNLSSTRINGS     pNLSStrings = cmnQueryNLSStrings();
        HWND hwndCnr = WinWindowFromID(pcnbp->hwndDlgPage, ID_XFDI_CNR_CNR);
        XFIELDINFO      xfi[5];
        PFIELDINFO      pfi = NULL;
        int             i = 0;

        // set group cnr title
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XFDI_CNR_GROUPTITLE,
                          pNLSStrings->pszClassesPage);

        // set up cnr details view
        xfi[i].ulFieldOffset = FIELDOFFSET(XCLASSRECORD, pszDLL);
        xfi[i].pszColumnTitle = "DLL";
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        xfi[i].ulFieldOffset = FIELDOFFSET(XCLASSRECORD, pszClass);
        xfi[i].pszColumnTitle = pNLSStrings->pszWidgetClass; // "Class";
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        xfi[i].ulFieldOffset = FIELDOFFSET(XCLASSRECORD, pszClassTitle);
        xfi[i].pszColumnTitle = "Class title"; // @@todo
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        xfi[i].ulFieldOffset = FIELDOFFSET(XCLASSRECORD, pszVersion);
        xfi[i].pszColumnTitle = "Version"; // @@todo
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
            cnrhSetSplitBarPos(200);
        } END_CNRINFO(hwndCnr);

        ctrpLoadClasses();
    }

    if (flFlags & CBI_SET)
    {
        PLINKLIST pllClasses = ctrpQueryClasses();
        ULONG cClasses = lstCountItems(pllClasses);

        if (cClasses)
        {
            HWND hwndCnr = WinWindowFromID(pcnbp->hwndDlgPage, ID_XFDI_CNR_CNR);
            PXCLASSRECORD precFirst
                = (PXCLASSRECORD)cnrhAllocRecords(hwndCnr,
                                                  sizeof(XCLASSRECORD),
                                                  cClasses);

            if (precFirst)
            {
                PXCLASSRECORD precThis = precFirst;
                PLISTNODE pNode = lstQueryFirstNode(pllClasses);
                while (pNode)
                {
                    PPRIVATEWIDGETCLASS pClass = (PPRIVATEWIDGETCLASS)pNode->pItemData;

                    if (pClass->hmod)
                    {
                        PSZ p = NULL;
                        CHAR sz[CCHMAXPATH];
                        if (!DosQueryModuleName(pClass->hmod,
                                                sizeof(sz),
                                                sz))
                        {
                            p = strrchr(sz, '\\');
                            if (p)
                            {
                                strcpy(precThis->szDLL, p + 1);
                                precThis->pszDLL = precThis->szDLL;
                            }
                        }

                        if (!p)
                            precThis->pszDLL = "Error";

                        sprintf(precThis->szVersion,
                                "%d.%d.%d",
                                pClass->ulVersionMajor,
                                pClass->ulVersionMinor,
                                pClass->ulVersionRevision);
                        precThis->pszVersion = precThis->szVersion;
                    }
                    else
                        precThis->pszDLL = "Built-in";

                    precThis->pszClass = (PSZ)pClass->Public.pcszWidgetClass;

                    precThis->pszClassTitle = (PSZ)pClass->Public.pcszClassTitle;

                    precThis = (PXCLASSRECORD)precThis->recc.preccNextRecord;
                    pNode = pNode->pNext;
                }
            }

            cnrhInsertRecords(hwndCnr,
                              NULL,         // parent
                              (PRECORDCORE)precFirst,
                              TRUE,         // invalidate
                              NULL,
                              CRA_RECORDREADONLY,
                              cClasses);
        }
    }

    if (flFlags & CBI_ENABLE)
    {
    }

    if (flFlags & CBI_DESTROY)
    {
        ctrpFreeClasses();
    }
}

/*
 *@@ ctrpClassesItemChanged:
 *      notebook callback function (notebook.c) for the
 *      XCenter "Widgets" instance settings page.
 *      Reacts to changes of any of the dialog controls.
 *
 *@@added V0.9.7 (2000-12-05) [umoeller]
 */

MRESULT ctrpClassesItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                               USHORT usItemID, USHORT usNotifyCode,
                               ULONG ulExtra)      // for checkboxes: contains new state
{
    MRESULT     mrc = 0;

    return (mrc);
}

