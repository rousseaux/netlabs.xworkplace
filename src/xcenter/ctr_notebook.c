
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

#define INCL_DOSEXCEPTIONS
#define INCL_DOSPROCESS
#define INCL_DOSMODULEMGR
#define INCL_DOSSEMAPHORES      // V0.9.20 (2002-07-23) [lafaix] needed for plugins.h
#define INCL_DOSERRORS

#define INCL_WINWINDOWMGR
#define INCL_WINMENUS
#define INCL_WINDIALOGS
#define INCL_WINBUTTONS
#define INCL_WINSTATICS
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
#include "helpers\apmh.h"               // Advanced Power Management helpers
#include "helpers\cnrh.h"               // container helper routines
#include "helpers\dialog.h"             // dialog helpers
#include "helpers\except.h"             // exception handling
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\standards.h"          // some standard macros
#include "helpers\stringh.h"            // string helper routines
#include "helpers\winh.h"               // PM helper routines
#include "helpers\xstring.h"            // extended string helpers

// SOM headers which don't crash with prec. header files
#include "xcenter.ih"
#include "xfobj.ih"                     // XFldObject

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\errors.h"              // private XWorkplace error codes
#include "shared\notebook.h"            // generic XWorkplace notebook handling
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)
#include "shared\plugins.h"             // generic plugins support

#include "shared\center.h"              // public XCenter interfaces
#include "xcenter\centerp.h"            // private XCenter implementation

#include "hook\xwphook.h"
#include "config\hookintf.h"            // daemon/hook interface

#pragma hdrstop                     // VAC++ keeps crashing otherwise

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

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
 +          14: _lcolClientBackground
 +          15: _fHideOnClick           // V0.9.14 (2001-08-21) [umoeller]
 +          16: _ulHeight               // thanks for mentioning your changes here, martin
 +          17: _ulWidth
 +          18: _fAutoScreenBorder      // V0.9.19 (2002-05-07) [umoeller]
 +
 *@@added V0.9.9 (2001-01-29) [umoeller]
 *@@changed V0.9.14 (2001-08-21) [umoeller]: added "hide on click"
 *@@changed V0.9.16 (2001-10-15) [umoeller]: changed defaults
 *@@changed V0.9.19 (2002-05-07) [umoeller]: added "auto screen border"
 */

static const XWPSETUPENTRY    G_XCenterSetupSet[] =
    {
        /*
         * ulWindowStyle bitfield... first a LONG, then the bitfields
         *
         */

        // type,  setup string,     offset,
        STG_LONG_DEC,    NULL,           FIELDOFFSET(XCenterData, ulWindowStyle),
        //     key for wpSaveState/wpRestoreState
               2,      // bitfield! only first item!
        //     default, ulExtra,            min, max
               WS_ANIMATE,  0,              0,   0,

        // type,  setup string,     offset,
        STG_BITFLAG, "ALWAYSONTOP",  FIELDOFFSET(XCenterData, ulWindowStyle),
        //     key for wpSaveState/wpRestoreState
               0,      // bitfield! only first item!
        //     default, ulExtra,            min, max
               0,       WS_TOPMOST,         0,   0,

        // type,  setup string,     offset,
        STG_BITFLAG, "ANIMATE",     FIELDOFFSET(XCenterData, ulWindowStyle),
        //     key for wpSaveState/wpRestoreState
               0,      // bitfield! only first item!
        //     default, ulExtra,            min, max
               WS_ANIMATE,  WS_ANIMATE,     0,   0,    // V0.9.9 (2001-02-28) [pr]: fix setup string default

        /*
         * flDisplayStyle bitfield... first a LONG, then the bitfields
         *
         */

        // type,  setup string,     offset,
        STG_LONG_DEC,    NULL,             FIELDOFFSET(XCenterData, flDisplayStyle),
        //     key for wpSaveState/wpRestoreState
               4,      // bitfield! only first item!
        //     default, ulExtra,            min, max
               XCS_FLATBUTTONS | /* XCS_SUNKBORDERS | */ XCS_SIZINGBARS | XCS_SPACINGLINES,
                                 // ^^^ fixed V0.9.19 (2002-04-25) [umoeller]
                        0,                  0,   0,

        // type,  setup string,     offset,
        STG_BITFLAG, "FLATBUTTONS",    FIELDOFFSET(XCenterData, flDisplayStyle),
        //     key for wpSaveState/wpRestoreState
               0,      // bitfield! only first item!
        //     default, ulExtra,            min, max
               XCS_FLATBUTTONS, XCS_FLATBUTTONS, 0,   0,

        // type,  setup string,     offset,
        STG_BITFLAG, "SUNKBORDERS",    FIELDOFFSET(XCenterData, flDisplayStyle),
        //     key for wpSaveState/wpRestoreState
               0,      // bitfield! only first item!
        //     default, ulExtra,            min, max
               0, XCS_SUNKBORDERS, 0,   0,
                    // default changed V0.9.16 (2001-10-15) [umoeller]

        // type,  setup string,     offset,
        STG_BITFLAG, "SIZINGBARS",    FIELDOFFSET(XCenterData, flDisplayStyle),
        //     key for wpSaveState/wpRestoreState
               0,      // bitfield! only first item!
        //     default, ulExtra,            min, max
               XCS_SIZINGBARS, XCS_SIZINGBARS, 0,   0,

        // type,  setup string,     offset,
        STG_BITFLAG, "ALL3DBORDERS",    FIELDOFFSET(XCenterData, flDisplayStyle),
        //     key for wpSaveState/wpRestoreState
               0,      // bitfield! only first item!
        //     default, ulExtra,            min, max
               0, XCS_ALL3DBORDERS, 0,   0,

        // type,  setup string,     offset,
        STG_BITFLAG, "SPACINGLINES",    FIELDOFFSET(XCenterData, flDisplayStyle),
        //     key for wpSaveState/wpRestoreState
               0,      // bitfield! only first item!
        //     default, ulExtra,            min, max
               XCS_SPACINGLINES, XCS_SPACINGLINES, 0,   0,
                    // default changed V0.9.16 (2001-10-15) [umoeller]

        // type,  setup string,     offset,
        // V0.9.16 (2001-10-24) [umoeller]
        STG_BITFLAG, "NOHATCHOPENOBJ",      FIELDOFFSET(XCenterData, flDisplayStyle),
        //     key for wpSaveState/wpRestoreState
               0,      // bitfield! only first item!
        //     default, ulExtra,            min, max
               0,       XCS_NOHATCHINUSE,   0,   0,
                    // default changed V0.9.16 (2001-10-15) [umoeller]

        /*
         * other LONGs
         *
         */

        // type,  setup string,     offset,
        STG_LONG_DEC,    "AUTOHIDE",    FIELDOFFSET(XCenterData, ulAutoHide),
        //     key for wpSaveState/wpRestoreState
               3,
        //     default, ulExtra,            min, max
               0,       0,                  0,   60000,

        // type,  setup string,     offset,
        STG_BOOL, NULL,               FIELDOFFSET(XCenterData, fHelpDisplayed),
        //     key for wpSaveState/wpRestoreState
               5,
        //     default, ulExtra,            min, max
               FALSE,   0,                  0,   1,

        // type,  setup string,     offset,
        STG_LONG_DEC, "PRIORITYCLASS",    FIELDOFFSET(XCenterData, ulPriorityClass),
        //     key for wpSaveState/wpRestoreState
               6,
        //     default, ulExtra,            min, max
               PRTYC_REGULAR, 0,            1,   4,
                                            // PRTYC_IDLETIME == 1
                                            // PRTYC_FOREGROUNDSERVER = 4

        // type,  setup string,     offset,
        STG_LONG_DEC, "PRIORITYDELTA",    FIELDOFFSET(XCenterData, lPriorityDelta),
        //     key for wpSaveState/wpRestoreState
               7,
        //     default, ulExtra,            min, max
               0,       0,                  0,   31,

        // type,  setup string,     offset,
        STG_LONG_DEC, NULL,               FIELDOFFSET(XCenterData, ulPosition),
        //     key for wpSaveState/wpRestoreState
               8,
        //     default, ulExtra,            min, max
               XCENTER_BOTTOM, 0,           0,   3,

        // type,  setup string,     offset,
        STG_LONG_DEC, "3DBORDERWIDTH",    FIELDOFFSET(XCenterData, ul3DBorderWidth),
        //     key for wpSaveState/wpRestoreState
               9,
        //     default, ulExtra,            min, max
               2,       0,                  0,   10,
               // changed default V0.9.16 (2001-12-08) [umoeller]

        // type,  setup string,     offset,
        STG_LONG_DEC, "BORDERSPACING",    FIELDOFFSET(XCenterData, ulBorderSpacing),
        //     key for wpSaveState/wpRestoreState
               10,
        //     default, ulExtra,            min, max
               2,       0,                  0,   10,

        // type,  setup string,     offset,
        STG_LONG_DEC, "WIDGETSPACING",    FIELDOFFSET(XCenterData, ulWidgetSpacing),
        //     key for wpSaveState/wpRestoreState
               11,
        //     default, ulExtra,            min, max
               2,       0,                  1,   10,

        // type,  setup string,     offset,
        STG_BOOL,    "REDUCEDESKTOP",  FIELDOFFSET(XCenterData, fReduceDesktopWorkarea),
        //     key for wpSaveState/wpRestoreState
               12,
        //     default, ulExtra,            min, max
               FALSE,   0,                  0,   0,

        // V0.9.14 (2001-08-21) [umoeller]
        // type,  setup string,     offset,
        STG_BOOL,    "HIDEONCLICK", FIELDOFFSET(XCenterData, fHideOnClick),
        //     key for wpSaveState/wpRestoreState
               15,
        //     default, ulExtra,            min, max
               FALSE,   0,                  0,   0,

        // V0.9.19 (2002-05-07) [umoeller]
        // type,  setup string,     offset,
        STG_BOOL,    "AUTOSCREENBORDER", FIELDOFFSET(XCenterData, fAutoScreenBorder),
        //     key for wpSaveState/wpRestoreState
               18,
        //     default, ulExtra,            min, max
               TRUE,    0,                  0,   0,

        // V0.9.19 (2002-04-16) [lafaix]
        // type,  setup string,     offset,
        STG_LONG_DEC,    "HEIGHT", FIELDOFFSET(XCenterData, ulHeight),
        //     key for wpSaveState/wpRestoreState
               16,
        //     default, ulExtra,            min, max
               0,       0,                  0,   0xFFFF,

        // V0.9.19 (2002-04-16) [lafaix]
        // type,  setup string,     offset,
        STG_LONG_DEC,    "WIDTH", FIELDOFFSET(XCenterData, ulWidth),
        //     key for wpSaveState/wpRestoreState
               17,
        //     default, ulExtra,            min, max
               0,       0,                  0,   0xFFFF,

        /*
         * fonts/colors
         *      V0.9.9 (2001-03-07) [umoeller]
         */

        // type,  setup string,     offset,
        STG_PSZ,     "CLIENTFONT",  FIELDOFFSET(XCenterData, pszClientFont),
        //     key for wpSaveState/wpRestoreState
               13,
        //     default, ulExtra,            min, max
               (LONG)NULL, 0,               0,   0,

        // type,  setup string,     offset,
        STG_LONG_RGB,    "CLIENTCOLOR", FIELDOFFSET(XCenterData, lcolClientBackground),
        //     key for wpSaveState/wpRestoreState
               14,
        //     default, ulExtra,            min, max
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

static VOID AppendWidgetSettings(PXSTRING pstrSetup,
                                 PLINKLIST pllSettings);

/*
 *@@ ctrpAppendWidgetSettings:
 *      appends the given widget setting as a
 *      setup string to the given XSTRING.
 *
 *      Used by AppendWidgetSettings (into which
 *      this will also recurse), but also by
 *      drag'n'drop to drop trays across XCenters
 *      correctly.
 *
 *      pstrSetup is assumed to be initialized.
 *
 *      In addition, you must pass in a BOOL variable
 *      in pfFirstWidget that must be set to FALSE.
 *
 *      Finally, this uses pstrTemp as a temp buffer
 *      for speed, which must be initialized and
 *      cleared after use also.
 *
 *@@added V0.9.19 (2002-05-04) [umoeller]
 */

VOID ctrpAppendWidgetSettings(PXSTRING pstrSetup,
                              PPRIVATEWIDGETSETTING pSetting,
                              PBOOL pfFirstWidget,
                              PXSTRING pstrTemp)
{
    ULONG ulSetupLen;

    if (!*pfFirstWidget)
        // not first run:
        // add separator
        xstrcatc(pstrSetup, ',');
    else
        *pfFirstWidget = FALSE;

    // add widget class
    xstrcat(pstrSetup, pSetting->Public.pszWidgetClass, 0);

    // add first separator
    xstrcatc(pstrSetup, '(');

    if (    (pSetting->Public.pszSetupString)
         && (ulSetupLen = strlen(pSetting->Public.pszSetupString))
       )
    {
        // widget has a setup string:
        // copy widget setup string to temporary buffer
        // for encoding... this has "=" and ";"
        // chars in it, and these should not appear
        // in the WPS setup string
        xstrcpy(pstrTemp,
                pSetting->Public.pszSetupString,
                ulSetupLen);

        xstrEncode(pstrTemp,
                   "%,{}[]();=");
                        // added {}[] V0.9.19 (2002-04-25) [umoeller]

        // now append encoded widget setup string
        xstrcats(pstrSetup, pstrTemp);

    } // end if (    (pSetting->pszSetupString)...

    // add terminator
    xstrcatc(pstrSetup, ')');

    // add trays in round brackets, if we have any
    if (pSetting->pllTraySettings)
    {
        PLISTNODE pTrayNode = lstQueryFirstNode(pSetting->pllTraySettings);

        xstrcatc(pstrSetup, '{');

        while (pTrayNode)
        {
            PTRAYSETTING pTraySetting = (PTRAYSETTING)pTrayNode->pItemData;

            // "Tray(setupstring){Tray1[widget1,widget],Tray2[widget]}"

            xstrcat(pstrSetup,
                    pTraySetting->pszTrayName,
                    0);

            // add subwidgets in square brackets
            xstrcatc(pstrSetup, '[');

            // recurse
            AppendWidgetSettings(pstrSetup,
                                 &pTraySetting->llSubwidgetSettings);

            xstrcatc(pstrSetup, ']');

            pTrayNode = pTrayNode->pNext;
        }

        xstrcatc(pstrSetup, '}');
    }
}

/*
 *@@ AppendWidgetSettings:
 *      appends all widgets in pllSettings to the
 *      given XSTRING, including trays and subwidgets,
 *      if any.
 *
 *@@added V0.9.19 (2002-04-25) [umoeller]
 */

static VOID AppendWidgetSettings(PXSTRING pstrSetup,
                                 PLINKLIST pllSettings)
{
    BOOL    fFirstWidget = TRUE;
    XSTRING strSetup2;
    PLISTNODE pNode = lstQueryFirstNode(pllSettings);

    xstrInit(&strSetup2, 0);

    while (pNode)
    {
        PPRIVATEWIDGETSETTING pSetting = (PPRIVATEWIDGETSETTING)pNode->pItemData;

        ctrpAppendWidgetSettings(pstrSetup, pSetting, &fFirstWidget, &strSetup2);

        pNode = pNode->pNext;
    } // end for widgets

    xstrClear(&strSetup2);
}

/*
 *@@ ctrpQuerySetup:
 *      implementation for XCenter::xwpQuerySetup2.
 *
 *@@added V0.9.7 (2000-12-09) [umoeller]
 *@@changed V0.9.19 (2002-04-25) [umoeller]: finally added trays support
 */

BOOL ctrpQuerySetup(XCenter *somSelf,
                    PVOID pstrSetup)
{
    BOOL brc = TRUE;

    // V0.9.16 (2001-10-11) [umoeller]:
    // removed object lock
    // this is properly handled by xwpQuerySetup already

    // compose setup string

    TRY_LOUD(excpt2)
    {
        XCenterData *somThis = XCenterGetData(somSelf);

        // temporary buffer for building the setup string
        PLINKLIST pllSettings = ctrpQuerySettingsList(somSelf);
        PLISTNODE pNode;

        /*
         * build string
         *
         */

        if (_ulPosition == XCENTER_TOP)
            xstrcat(pstrSetup, "POSITION=TOP;", 0);

        // use array for the rest...
        cmnSetupBuildString(G_XCenterSetupSet,
                            ARRAYITEMCOUNT(G_XCenterSetupSet),
                            somThis,
                            pstrSetup);

        // now build widgets string... this is complex.
        if (pNode = lstQueryFirstNode(pllSettings))
        {
            // we have widgets:
            // go thru all of them and list all widget classes and setup strings

            xstrcat(pstrSetup, "WIDGETS=", 0);

            AppendWidgetSettings(pstrSetup, pllSettings);
                    // V0.9.19 (2002-04-25) [umoeller]

            xstrcatc(pstrSetup, ';');
        }
    }
    CATCH(excpt2)
    {
        brc = FALSE;
    } END_CATCH();

    // if we haven't crashed
    if (brc)
    {
        // manually resolve parent method
        return (wpshParentQuerySetup2(somSelf,
                                      _somGetParent(_XCenter),
                                      pstrSetup));
    }

    return brc;
}

/*
 *@@ CreateWidgetFromString:
 *
 *@@added V0.9.19 (2002-04-25) [umoeller]
 */

static BOOL CreateWidgetFromString(XCenter *somSelf,
                                   PCSZ pcszClass,
                                   PCSZ pcszSetup,
                                   ULONG ulTrayWidgetIndex,
                                   ULONG ulTrayIndex,
                                   PULONG pcWidgets,
                                   PBOOL pfIsTray)
{
    WIDGETPOSITION pos2;
    APIRET arc;

    _PmpfF(("creating \"%s\", \"%s\"",
                STRINGORNULL(pcszClass),
                STRINGORNULL(pcszSetup)));

    pos2.ulTrayWidgetIndex = ulTrayWidgetIndex;
    pos2.ulTrayIndex = ulTrayIndex;
    pos2.ulWidgetIndex = -1;        // to the right
    if (arc = _xwpCreateWidget(somSelf,
                               (PSZ)pcszClass,
                               (PSZ)pcszSetup,
                               &pos2))
    {
        // error:
        cmnLog(__FILE__, __LINE__, __FUNCTION__,
               "Creating widget \"%s\" (\"%s\") failed, rc = %d.",
               pcszClass,
               pcszSetup,
               arc);
        return FALSE;
    }

    if (pfIsTray)
        *pfIsTray = (!strcmp(pcszClass, TRAY_WIDGET_CLASS_NAME));

    ++(*pcWidgets);

    return TRUE;
}

static BOOL ParseWidgetsString(XCenter *somSelf,
                               PSZ pszWidgets,      // in: WIDGETS= data only
                               ULONG ulTrayWidgetIndex,
                               ULONG ulTrayIndex);

/*
 *@@ ParseTraysList:
 *      called from ParseWidgetsString if a
 *      trays list is encountered. We will then
 *      recurse into ParseWidgetsString with
 *      the subwidget lists.
 *
 *@@added V0.9.19 (2002-04-25) [umoeller]
 */

static BOOL ParseTraysList(XCenter *somSelf,
                           PCSZ pcszTraysList,
                           ULONG ulTrayWidgetIndex)
{
    BOOL brc = TRUE;

    // on input, we get
    // "Tray1[widget1,widget]Tray2[widget]"
    PCSZ pTrayThis = pcszTraysList;
    ULONG cTraysSet = 0;
    while (    (pTrayThis)
            && (*pTrayThis)
            && (brc)
          )
    {
        // tray with widgets list comes next in square brackets
        PCSZ    pOpen,
                pClose;
        if (    (!(pOpen = strchr(pTrayThis, '[')))
             || (!(pClose = strchr(pOpen, ']')))
           )
            brc = FALSE;
        else
        {
            PSZ pszTrayName;
            if (!(pszTrayName = strhSubstr(pTrayThis,
                                           pOpen)))
                brc = FALSE;
            else
            {
                PSZ pszSubwidgetsList;
                APIRET arc;

                if (!cTraysSet)
                    // first tray: the tray widget creates
                    // an automatic tray if there's none
                    // on creation, so rename this
                    arc = _xwpRenameTray(somSelf,
                                         ulTrayWidgetIndex,
                                         0,
                                         pszTrayName);
                else
                    arc = _xwpCreateTray(somSelf,
                                         ulTrayWidgetIndex,
                                         pszTrayName);

                if (arc)
                    brc = FALSE;
                else
                {

                    // now recurse with the widgets list
                    if (!(pszSubwidgetsList = strhSubstr(pOpen + 1,
                                                         pClose)))
                        brc = FALSE;
                    else
                    {
                        _PmpfF(("recursing with string \"%s\"",
                              pszSubwidgetsList));

                        brc = ParseWidgetsString(somSelf,
                                                 pszSubwidgetsList,
                                                 ulTrayWidgetIndex,
                                                 cTraysSet);

                        _PmpfF(("ParseWidgetsString returned %d", brc));

                        free(pszSubwidgetsList);
                    }

                    // next tray after closing bracket
                    pTrayThis = pClose + 1;

                    ++cTraysSet;
                }
            }
        }
    }

    return brc;
}

/*
 *@@ ParseWidgetsString:
 *      implementation for the WIDGETS=xxx setup
 *      string particle in ctrpSetup.
 *
 *      Initially, this gets called from ctrpSetup
 *      with ulTrayWidgetIndex and ulTrayIndex
 *      both set to -1 to create the root widgets.
 *      If we find trays, we call ParseTraysList,
 *      which will recurse into this routine with
 *      the tray widget and tray indices set
 *      accordingly.
 *
 *      I am not sure that I will understand what
 *      this code is doing one week from now, but
 *      apparently it's working for now.
 *
 *@@added V0.9.19 (2002-04-25) [umoeller]
 */

static BOOL ParseWidgetsString(XCenter *somSelf,
                               PSZ pszWidgets,      // in: data from WIDGETS= only
                               ULONG ulTrayWidgetIndex,
                               ULONG ulTrayIndex)
{
    BOOL brc = TRUE;

    // now, off we go...
    // parse the WIDGETS string
    // format is: "widget1,widget2,widget3"
    // where each "widget" is one of
    // -- plain widget class name, e.g. "Pulse"
    // -- widget class name with encoded setup
    //    string, e.g. "Pulse(WIDTH%3D130%3)"
    // -- a tray with even more subwidgets,
    //    e.g. "Tray(setupstring){Tray1[widget1,widget]Tray2[widget]}"

    PCSZ    pThis = pszWidgets,
            pStartOfWidget = pszWidgets;
    CHAR    c;

    BOOL    fStop = FALSE;

    ULONG   cWidgetsCreated = 0;
    BOOL    fLastWasTray = FALSE;

    while (    (!fStop)
            && (brc)
          )
    {
        switch (c = *pThis)
        {
            case '\0':
            case ',':
            {
                // OK, widget terminator:
                // then we had no setup string
                PSZ pszWidgetClass = strhSubstr(pStartOfWidget,
                                                pThis);     // up to comma

                brc = CreateWidgetFromString(somSelf,
                                             pszWidgetClass,
                                             NULL,
                                             ulTrayWidgetIndex,
                                             ulTrayIndex,
                                             &cWidgetsCreated,
                                             NULL);

                FREE(pszWidgetClass);

                if (!c)
                    fStop = TRUE;
                else
                    pStartOfWidget = pThis + 1;
            }
            break;

            case '(':
            {
                // beginning of setup string:
                // extract setup
                PCSZ pClose;
                if (!(pClose = strchr(pThis + 1, ')')))
                {
                    cmnLog(__FILE__, __LINE__, __FUNCTION__,
                           "Expected ')' after \"%s\"",
                           pThis + 1);
                    brc = FALSE;
                }
                else
                {
                    // extract widget class before bracket
                    PSZ pszWidgetClass;
                    if (!(pszWidgetClass = strhSubstr(pStartOfWidget,
                                                      pThis)))     // up to bracket
                    {
                        cmnLog(__FILE__, __LINE__, __FUNCTION__,
                               "Invalid widget class at \"%s\"",
                               pStartOfWidget);
                        brc = FALSE;
                    }
                    else
                    {
                        // copy widget setup string to temporary
                        // buffer for decoding...
                        XSTRING strSetup2;
                        PSZ pszWidgetSetup = strhSubstr(pThis + 1,
                                                        pClose);

                        xstrInitSet(&strSetup2,
                                    pszWidgetSetup);
                        xstrDecode(&strSetup2);
                        pszWidgetSetup = strSetup2.psz;

                        brc = CreateWidgetFromString(somSelf,
                                                     pszWidgetClass,
                                                     pszWidgetSetup,
                                                     ulTrayWidgetIndex,
                                                     ulTrayIndex,
                                                     &cWidgetsCreated,
                                                     &fLastWasTray);

                        FREE(pszWidgetSetup);
                        FREE(pszWidgetClass);
                    }

                    if (brc)
                    {
                        // continue after closing bracket;
                        // next will either be a comma or, if this
                        // is a tray, a square bracket
                        pThis = pClose + 1;

                        switch (*pThis)
                        {
                            case ',':
                                ++pThis;
                                pStartOfWidget = pThis;
                                continue;

                            case '{':
                                continue;

                            case '\0':
                                fStop = TRUE;
                            break;

                            default:
                                cmnLog(__FILE__, __LINE__, __FUNCTION__,
                                       "Expected ',' after \"%s\"",
                                       pClose);
                                brc = FALSE;
                        }
                    }
                }
            }
            break;

            case '{':
            {
                // beginning of trays list for tray widget:
                // then we must have created a widget already,
                // or this will fail
                if (!fLastWasTray)
                    brc = FALSE;
                else
                {
                    PCSZ pClose;
                    if (!(pClose = strchr(pThis + 1, '}')))
                    {
                        cmnLog(__FILE__, __LINE__, __FUNCTION__,
                               "Expected '}' after \"%s\"",
                               pThis + 1);
                        brc = FALSE;
                    }
                    else
                    {
                        // now run thru the trays list:
                        PSZ pszTraysList;
                        if (!(pszTraysList = strhSubstr(pThis + 1,
                                                        pClose)))
                        {
                            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                                   "Invalid trays list at \"%s\"",
                                   pThis + 1);
                            brc = FALSE;
                        }
                        else
                        {
                            // alright, call this subroutine,
                            // which will recurse into this routine
                            // with the subwidgets lists
                            brc = ParseTraysList(somSelf,
                                                 pszTraysList,
                                                 cWidgetsCreated - 1);
                            free(pszTraysList);
                        }
                    }

                    if (brc)
                    {
                        // continue after closing bracket;
                        // next _must_ be a comma
                        pThis = pClose + 1;

                        switch (*pThis)
                        {
                            case ',':
                                ++pThis;
                                pStartOfWidget = pThis;
                                continue;

                            case '\0':
                                fStop = TRUE;
                            break;

                            default:
                                cmnLog(__FILE__, __LINE__, __FUNCTION__,
                                       "Expected ',' after \"%s\"",
                                       pClose);
                                brc = FALSE;
                        }
                    }
                }
            }
            break;

            case ')':
            case '}':
            case '[':
            case ']':
                // loose closing brackets: that's invalid syntax
                cmnLog(__FILE__, __LINE__, __FUNCTION__,
                       "Unexpected character '%c' at \"%s\"",
                       *pThis,
                       pThis);
                brc = FALSE;
        }

        pThis++;
    }

    return brc;
}

/*
 *@@ AppendObjects:
 *      helper for CreateDefaultWidgets.
 *
 *@@added V0.9.19 (2002-04-25) [umoeller]
 */

static ULONG AppendObjects(PXSTRING pstr,
                           PCSZ *apcsz,
                           ULONG c)
{
    ULONG cAdded = 0;
    ULONG ul;
    for (ul = 0;
         ul < c;
         ++ul)
    {
        if (wpshQueryObjectFromID(apcsz[ul], NULL))
        {
            if (cAdded)
                xstrcatc(pstr, ',');

            xstrcat(pstr,
                    "ObjButton(OBJECTHANDLE%3D",
                    0);
            xstrcat(pstr,
                    apcsz[ul],
                    0);
            xstrcat(pstr,
                    "%3B)",
                    0);

            ++cAdded;
        }
    }

    return cAdded;
}

/*
 *@@ CreateDefaultWidgets:
 *      this creates the default widgets in the XCenter.
 *      by creating a setup string accordingly and then
 *      calling ParseWidgetsString directly, which will
 *      go create the widgets from the string.
 *
 *      NOTE: This
 *
 *@@added V0.9.20 (2002-07-19) [umoeller]
 */

static BOOL CreateDefaultWidgets(XCenter *somSelf)
{
    BOOL brc;

    static const char *apcszMain[] =
        {
            "<XWP_LOCKUPSTR>",
            "<XWP_FINDSTR>",
            "<XWP_SHUTDOWNSTR>",
        };
    static const char *apcszTray1[] =
        {
            "<WP_DRIVES>",
            "<WP_CONFIG>",
            "<WP_PROMPTS>"
        };
    ULONG ul;
    XSTRING str;
    WPObject *pobj;
    CHAR sz[200];

    xstrInitCopy(&str,
                 "XButton(),",
                 0);

    if (AppendObjects(&str,
                      apcszMain,
                      ARRAYITEMCOUNT(apcszMain)))
        xstrcatc(&str, ',');

    xstrcat(&str,
            "Pulse(),"
            "Tray(){",
            0);

    // create a default tray
    sprintf(sz,
            cmnGetString(ID_CRSI_TRAY),     // tray %d
            1);
    xstrcat(&str,
            sz,
            0);

    xstrcatc(&str, '[');

    AppendObjects(&str,
                  apcszTray1,
                  ARRAYITEMCOUNT(apcszTray1));

    xstrcat(&str,
            "]},WindowList,"
            "Time",
            0);

    // on laptops, add battery widget too
    if (apmhHasBattery())
        xstrcat(&str,
                 ",Power()",
                 0);

    // call ParseWidgetsString directly
    brc = ParseWidgetsString(somSelf,
                             str.psz,
                             -1,
                             -1);

    xstrClear(&str);

    return brc;
}

/*
 *@@ ctrpSetup:
 *      implementation for XCenter::wpSetup.
 *
 *@@added V0.9.7 (2001-01-25) [umoeller]
 *@@changed V0.9.16 (2001-10-15) [umoeller]: fixed widget clearing which caused a log entry for empty XCenter
 *@@changed V0.9.19 (2002-04-25) [umoeller]: added exception handling; rewrote WIDGETS= setup string to handle trays
 */

BOOL ctrpSetup(XCenter *somSelf,
               PSZ pszSetupString)
{
    XCenterData *somThis = XCenterGetData(somSelf);
    ULONG   cSuccess = 0;

    // scan the standard stuff from the table...
    // this saves us a lot of work.
    BOOL    brc;

    TRY_LOUD(excpt1)
    {
        _PmpfF(("string is \"%s\"", pszSetupString));

        // now comes the non-standard stuff:
        if (brc = cmnSetupScanString(somSelf,
                                     G_XCenterSetupSet,
                                     ARRAYITEMCOUNT(G_XCenterSetupSet),
                                     somThis,
                                     pszSetupString,
                                     &cSuccess))
        {
            CHAR    szValue[100];
            ULONG   cb = sizeof(szValue);

            _Pmpf(("   cmnSetupScanString returned TRUE"));

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

            _Pmpf(("   brc still TRUE; scanning WIDGETS string"));

            if (_wpScanSetupString(somSelf,
                                   pszSetupString,
                                   "WIDGETS",
                                   NULL,
                                   &cb))
            {
                PSZ pszWidgets;

                _Pmpf(("got WIDGETS string, %d bytes, nuking existing widgets",
                        cb));

                if (    (brc)
                     && (cb)
                     && (pszWidgets = malloc(cb))
                   )
                {
                    // first of all, remove all existing widgets,
                    // we have a replacement here
                    WIDGETPOSITION pos;
                    pos.ulTrayWidgetIndex = -1;
                    pos.ulTrayIndex = -1;
                    pos.ulWidgetIndex = 0;      // leftmost widget
                    while (ctrpQueryWidgetsCount(somSelf))       // V0.9.16 (2001-10-15) [umoeller]
                        if (_xwpDeleteWidget(somSelf,
                                             &pos))
                        {
                            // error:
                            brc = FALSE;
                            break;
                        }

                    if (    (brc)
                         && (_wpScanSetupString(somSelf,
                                                pszSetupString,
                                                "WIDGETS",
                                                pszWidgets,
                                                &cb))
                       )
                        // allow "CLEAR" to just nuke the widgets
                        // V0.9.19 (2002-04-25) [umoeller]
                        if (!stricmp(pszWidgets, "CLEAR"))
                            ;
                        // allow "RESET" to reset the widgets to
                        // the defaults
                        // V0.9.20 (2002-07-19) [umoeller]
                        else if (!stricmp(pszWidgets, "RESET"))
                            brc = CreateDefaultWidgets(somSelf);
                        else
                            brc = ParseWidgetsString(somSelf,
                                                     pszWidgets,
                                                     -1,
                                                     -1);

                    free(pszWidgets);
                } // end if (    (pszWidgets)...
            }
        }
    }
    CATCH(excpt1)
    {
        brc = FALSE;
    } END_CATCH();

    return brc;
}

/*
 *@@ ctrpSetupOnce:
 *      implementation for XCenter::wpSetupOnce.
 *      Creates the default widgets if none are given.
 *
 *@@added V0.9.16 (2001-10-15) [umoeller]
 *@@changed V0.9.19 (2002-04-25) [umoeller]: added default tray with objects
 *@@changed V0.9.20 (2002-07-19) [umoeller]: now checking for whether WIDGETS is specified
 */

BOOL ctrpSetupOnce(XCenter *somSelf,
                   PSZ pszSetupString)
{
    BOOL brc = TRUE;

    WPObject *pobjLock = NULL;
    TRY_LOUD(excpt1)
    {
        ULONG cb;
        // create default widgets if we have no widgets yet
        // AND the WIDGETS string is not specified on
        // creation (otherwise ctrpSetup will handle this)
        if (    (!_wpScanSetupString(somSelf,           // V0.9.20 (2002-07-19) [umoeller]
                                     pszSetupString,
                                     "WIDGETS",
                                     NULL,
                                     &cb))
             && (pobjLock = cmnLockObject(somSelf))
           )
        {
            PLINKLIST pllSettings = ctrpQuerySettingsList(somSelf);
            if (!lstCountItems(pllSettings))
                CreateDefaultWidgets(somSelf);
        }
    }
    CATCH(excpt1) {} END_CATCH();

    if (pobjLock)
        _wpReleaseObjectMutexSem(pobjLock);

    return brc;
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
                        (PSZ)G_pcszXCenterReal,
                        1,
                        _pszPackedWidgetSettings,
                        _cbPackedWidgetSettings);
        else
            // once the settings have been unpacked
            // (i.e. XCenter needed access to them),
            // we have to repack them on each save
            if (_pllAllWidgetSettings)
            {
                // compose array
                ULONG cbSettingsArray = 0;
                PSZ pszSettingsArray = ctrpStuffSettings(somSelf,
                                                         &cbSettingsArray);
                if (pszSettingsArray)
                {
                    _wpSaveData(somSelf,
                                (PSZ)G_pcszXCenterReal,
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
                     G_pcszXCenterReal,     // class name
                     somThis);
    }
    CATCH(excpt1)
    {
        brc = FALSE;
    } END_CATCH();

    return brc;
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
                           (PSZ)G_pcszXCenterReal,
                           1,
                           NULL,    // query size
                           &_cbPackedWidgetSettings))
        {
            _pszPackedWidgetSettings = (PSZ)malloc(_cbPackedWidgetSettings);
            if (_pszPackedWidgetSettings)
            {
                if (!_wpRestoreData(somSelf,
                                   (PSZ)G_pcszXCenterReal,
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
                        G_pcszXCenterReal,     // class name
                        somThis);
    }
    CATCH(excpt1)
    {
        brc = FALSE;
    } END_CATCH();

    return brc;
}

/*
 *@@ ctrpSaveToFile:
 *      write the widget settings to a file, and sets its .TYPE
 *      attribute to DRT_WIDGET.
 *
 *      pszClass must not be NULL.  pszSetup may be NULL.
 *
 *      Returns TRUE if the operation was successful.
 *
 *@@added V0.9.14 (2001-07-30) [lafaix]
 */

BOOL ctrpSaveToFile(PCSZ pszDest,
                    PCSZ pszClass,
                    PCSZ pszSetup)
{
    HFILE     hf;
    ULONG     ulAction;
    EAOP2     eaop2;
    PFEA2LIST pfea2l;
    #pragma pack(1)
    struct
    {
        CHAR   ach[6];
        USHORT usMainType,
               usCodepage,
               usCount;
        USHORT usVal1Type,
               usVal1Len;
        CHAR   achVal1[15];
        USHORT usVal2Type,
               usVal2Len;
        CHAR   achVal2[10];
    } val;
    #pragma pack()
    BOOL      brc = FALSE;

    if (!DosOpen((PSZ)pszDest,
                 &hf,
                 &ulAction,
                 0L,
                 0,
                 FILE_OPEN | OPEN_ACTION_CREATE_IF_NEW,
                 OPEN_ACCESS_WRITEONLY | OPEN_SHARE_DENYREADWRITE,
                 0))
    {
        // Adding a "Widget settings" .TYPE EA
        if ((pfea2l = (PFEA2LIST)malloc(sizeof(FEA2LIST) + sizeof(val))))
        {
            pfea2l->cbList = sizeof(FEA2LIST) + sizeof(val);
            pfea2l->list[0].oNextEntryOffset = 0;
            pfea2l->list[0].fEA = 0;
            pfea2l->list[0].cbName = 5;
            pfea2l->list[0].cbValue = sizeof(val) - 6;
            strcpy(val.ach, ".TYPE");
            val.usMainType = EAT_MVMT;
            val.usCodepage = 0;
            val.usCount = 2;
            val.usVal1Type = EAT_ASCII;
            val.usVal1Len = 15; // strlen(DRT_WIDGET)
            memcpy(val.achVal1, DRT_WIDGET, 15);
            val.usVal2Type = EAT_ASCII;
            val.usVal2Len = 10; // strlen(DRT_TEXT)
            memcpy(val.achVal2, DRT_TEXT, 10);
            memcpy(pfea2l->list[0].szName, &val, sizeof(val));
            eaop2.fpFEA2List = pfea2l;

            if (!DosSetFileInfo(hf,
                                FIL_QUERYEASIZE,
                                &eaop2,
                                sizeof(eaop2)))
            {
                // create the file content:
                if (    // first, the widget class name, which cannot
                        // be NULL
                        (pszClass)
                     && (!DosWrite(hf,
                                   (PVOID)pszClass,
                                   strlen(pszClass),
                                   &ulAction))
                        // then, a CR/LF marker
                     && (!DosWrite(hf,
                                   "\r\n",
                                   2,
                                   &ulAction))
                        // and the setup string, which may be NULL
                     && (    (pszSetup == NULL)
                          || (!DosWrite(hf,
                                        (PVOID)pszSetup,
                                        strlen(pszSetup),
                                        &ulAction))
                             )
                   )
                    brc = TRUE;
            }

            free(pfea2l);
        } // end if ((pfea2l = (PFEA2LIST) malloc(...)))

        DosClose(hf);
    }

    return brc;
}

/*
 *@@ ctrpReadFromFile:
 *      returns a packed representation of the widget.
 *
 *      Returns:
 *
 *      --  NO_ERROR if the operation was successful. In this case,
 *          *ppszClass and *ppszSetup receive a malloc'd buffer
 *          each with the widget class name and setup string to
 *          be freed by the caller.
 *
 *      --  ERROR_NOT_ENOUGH_MEMORY
 *
 *      --  ERROR_BAD_FORMAT: file format is invalid.
 *
 *      plus the error codes of DosOpen and the like.
 *
 *@@added V0.9.14 (2001-07-30) [lafaix]
 *@@changed V0.9.19 (2002-05-22) [umoeller]: mostly rewritten, prototype changed; now returning APIRET and ready-made strings
 */

APIRET ctrpReadFromFile(PCSZ pszSource,     // in: source file name
                        PSZ *ppszClass,     // out: widget class name
                        PSZ *ppszSetup)     // out: widget setup string
{
    APIRET      arc;
    HFILE       hf;
    ULONG       ulAction;
    FILESTATUS3 fs3;
    PSZ         pszBuff = NULL;

    if (!(arc = DosOpen((PSZ)pszSource,
                        &hf,
                        &ulAction,
                        0L,
                        0,
                        FILE_OPEN,
                        OPEN_ACCESS_READONLY | OPEN_SHARE_DENYWRITE,
                        0)))
    {
        // we will read the file in just one block,
        // so we must know its size
        if (!(arc = DosQueryFileInfo(hf,
                                     FIL_STANDARD,
                                     &fs3,
                                     sizeof(fs3))))
        {
            if (!(pszBuff = malloc(fs3.cbFile + 1)))
            {
                arc = ERROR_NOT_ENOUGH_MEMORY;
                _PmpfF(("malloc I failed (%d bytes)", fs3.cbFile + 1));
            }
            else
            {
                if (!(arc = DosRead(hf,
                                    pszBuff,
                                    fs3.cbFile,
                                    &ulAction)))
                {
                    pszBuff[fs3.cbFile] = 0;
                }
            }
        }

        DosClose(hf);
    }

    if (!arc)
    {
        ULONG   ulClassLen, ulSetupLen;
        PSZ     pSeparator;
        if (    (!(pSeparator = strstr(pszBuff, "\r\n")))
             || (!(ulClassLen = pSeparator - pszBuff))
             || (!(ulSetupLen = strlen(pSeparator + 2)))
           )
            arc = ERROR_BAD_FORMAT;
        else
        {
            if (!(*ppszClass = malloc(ulClassLen + 1)))
                arc = ERROR_NOT_ENOUGH_MEMORY;
            else if (!(*ppszSetup = malloc(ulSetupLen + 1)))
            {
                free(*ppszClass);
                arc = ERROR_NOT_ENOUGH_MEMORY;
            }
            else
            {
                memcpy(*ppszClass, pszBuff, ulClassLen);
                (*ppszClass)[ulClassLen] = '\0';
                pSeparator += 2;
                memcpy(*ppszSetup, pSeparator, ulSetupLen);
                (*ppszSetup)[ulSetupLen] = '\0';
            }
        }
    }

    if (pszBuff)
        free(pszBuff);

    return arc;
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

static SLDCDATA
        DelaySliderCData =
             {
                     sizeof(SLDCDATA),
            // usScale1Increments:
                     60,          // scale 1 increments
                     0,         // scale 1 spacing
                     1,          // scale 2 increments
                     0           // scale 2 spacing
             },
        PrioritySliderCData =
             {
                     sizeof(SLDCDATA),
            // usScale1Increments:
                     32,          // scale 1 increments
                     0,         // scale 1 spacing
                     1,          // scale 2 increments
                     0           // scale 2 spacing
             };

#define VIEW_TABLE_WIDTH    200
#define HALF_TABLE_WIDTH    ((VIEW_TABLE_WIDTH / 2) - COMMON_SPACING - GROUP_INNER_SPACING_X)
#define DESCRTXT_WIDTH      15
#define SLIDER_WIDTH        (HALF_TABLE_WIDTH - 4 * COMMON_SPACING - DESCRTXT_WIDTH )

static const CONTROLDEF
    FrameGroup = LOADDEF_GROUP(ID_XRDI_VIEW_FRAMEGROUP, VIEW_TABLE_WIDTH),
    ReduceDesktopCB = LOADDEF_AUTOCHECKBOX(ID_CRDI_VIEW_REDUCEWORKAREA),
    AlwaysOnTopCB = LOADDEF_AUTOCHECKBOX(ID_CRDI_VIEW_ALWAYSONTOP),
    AnimateCB = LOADDEF_AUTOCHECKBOX(ID_CRDI_VIEW_ANIMATE),
    AutoHideCB = LOADDEF_AUTOCHECKBOX(ID_CRDI_VIEW_AUTOHIDE),
    DelayTxt1 = LOADDEF_TEXT(ID_CRDI_VIEW_AUTOHIDE_TXT1),
    DelaySlider = CONTROLDEF_SLIDER(ID_CRDI_VIEW_AUTOHIDE_SLIDER, 108, 14, &DelaySliderCData),
    DelayTxt2 = CONTROLDEF_TEXT_CENTER("TBR", ID_CRDI_VIEW_AUTOHIDE_TXT2, DESCRTXT_WIDTH, SZL_AUTOSIZE),
    AutoHideClickCB = LOADDEF_AUTOCHECKBOX(ID_CRDI_VIEW_AUTOHIDE_CLICK),
    AutoScreenBorderCB = LOADDEF_AUTOCHECKBOX(ID_CRDI_VIEW_AUTOSCREENBORDER),
    PriorityGroup = LOADDEF_GROUP(ID_CRDI_VIEW_PRTY_GROUP, HALF_TABLE_WIDTH),
    PrioritySlider = CONTROLDEF_SLIDER(ID_CRDI_VIEW_PRTY_SLIDER, SLIDER_WIDTH, 16, &PrioritySliderCData),
    PriorityTxt2 = CONTROLDEF_TEXT_CENTER("TBR", ID_CRDI_VIEW_PRTY_TEXT, DESCRTXT_WIDTH, SZL_AUTOSIZE),
    PositionGroup = LOADDEF_GROUP(ID_CRDI_VIEW_POSITION_GROUP, HALF_TABLE_WIDTH),
    TopOfScreenRadio = LOADDEF_FIRST_AUTORADIO(ID_CRDI_VIEW_TOPOFSCREEN),
    BottomOfScreenRadio = LOADDEF_NEXT_AUTORADIO(ID_CRDI_VIEW_BOTTOMOFSCREEN);

static const DLGHITEM G_dlgXCenterView[] =
    {
        START_TABLE,            // root table, required
            START_ROW(0),
                START_GROUP_TABLE(&FrameGroup),
                    START_ROW(0),
                        CONTROL_DEF(&ReduceDesktopCB),
                    START_ROW(0),
                        CONTROL_DEF(&AlwaysOnTopCB),
                    START_ROW(0),
                        CONTROL_DEF(&AnimateCB),
                    START_ROW(0),
                        CONTROL_DEF(&AutoHideCB),
                    START_ROW(ROW_VALIGN_CENTER),
                        CONTROL_DEF(&G_Spacing),
                        CONTROL_DEF(&DelayTxt1),
                        CONTROL_DEF(&DelaySlider),
                        CONTROL_DEF(&DelayTxt2),
                    START_ROW(ROW_VALIGN_CENTER),
                        CONTROL_DEF(&G_Spacing),
                        CONTROL_DEF(&AutoHideClickCB),
                    START_ROW(0),
                        CONTROL_DEF(&AutoScreenBorderCB),
                END_TABLE,
            START_ROW(0),
                START_GROUP_TABLE(&PositionGroup),
                    START_ROW(0),
                        CONTROL_DEF(&TopOfScreenRadio),
                    START_ROW(0),
                        CONTROL_DEF(&BottomOfScreenRadio),
                END_TABLE,
                START_GROUP_TABLE(&PriorityGroup),
                    START_ROW(ROW_VALIGN_CENTER),
                        CONTROL_DEF(&PrioritySlider),
                        CONTROL_DEF(&PriorityTxt2),
                END_TABLE,
            START_ROW(0),       // notebook buttons (will be moved)
                CONTROL_DEF(&G_UndoButton),         // common.c
                CONTROL_DEF(&G_DefaultButton),      // common.c
                CONTROL_DEF(&G_HelpButton),         // common.c
        END_TABLE
    };

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
 *@@changed V0.9.14 (2001-08-21) [umoeller]: added "hide on click"
 *@@changed V0.9.19 (2002-05-07) [umoeller]: now using dlg formatter
 *@@changed V0.9.19 (2002-05-07) [umoeller]: added auto screen border
 *@@changed V0.9.19 (2002-06-08) [umoeller]: fixed wrong slider ticks for priority
 */

VOID ctrpView1InitPage(PNOTEBOOKPAGE pnbp,   // notebook info struct
                       ULONG flFlags)        // CBI_* flags (notebook.h)
{
    XCenterData *somThis = XCenterGetData(pnbp->inbp.somSelf);

    if (flFlags & CBI_INIT)
    {
        // make backup of instance data for "Undo"
        XCenterData *pBackup;
        if (pBackup = (XCenterData*)malloc(sizeof(*somThis)))
            memcpy(pBackup, somThis, sizeof(*somThis));
            // be careful about using the copy... we have some pointers in there!
        // store in notebook struct
        pnbp->pUser = pBackup;

        // insert the controls using the dialog formatter
        // V0.9.19 (2002-05-07) [umoeller]
        ntbFormatPage(pnbp->hwndDlgPage,
                      G_dlgXCenterView,
                      ARRAYITEMCOUNT(G_dlgXCenterView));

        winhSetSliderTicks(WinWindowFromID(pnbp->hwndDlgPage,
                                           ID_CRDI_VIEW_AUTOHIDE_SLIDER),
                           MPFROM2SHORT(9, 10), 6,
                           (MPARAM)-1, -1);

        winhSetSliderTicks(WinWindowFromID(pnbp->hwndDlgPage,
                                           ID_CRDI_VIEW_PRTY_SLIDER),
                           (MPARAM)0, 3,
                           MPFROM2SHORT(0, 10), 6);
                                // fixed V0.9.19 (2002-06-08) [umoeller]
    }

    if (flFlags & CBI_SET)
    {
        LONG lSliderIndex = 0;
        winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_CRDI_VIEW_REDUCEWORKAREA,
                              _fReduceDesktopWorkarea);

        winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_CRDI_VIEW_ALWAYSONTOP,
                              ((_ulWindowStyle & WS_TOPMOST) != 0));
        winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_CRDI_VIEW_ANIMATE,
                              ((_ulWindowStyle & WS_ANIMATE) != 0));

        // autohide
        winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_CRDI_VIEW_AUTOHIDE,
                              (_ulAutoHide > 0));
        if (_ulAutoHide)
            lSliderIndex = (_ulAutoHide / 1000) - 1;

        // prevent the stupid slider control from interfering
        G_fSetting = TRUE;
        winhSetSliderArmPosition(WinWindowFromID(pnbp->hwndDlgPage,
                                                 ID_CRDI_VIEW_AUTOHIDE_SLIDER),
                                 SMA_INCREMENTVALUE,
                                 lSliderIndex);
        G_fSetting = FALSE;

        WinSetDlgItemShort(pnbp->hwndDlgPage,
                           ID_CRDI_VIEW_AUTOHIDE_TXT2,
                           _ulAutoHide / 1000,
                           FALSE);      // unsigned

        winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_CRDI_VIEW_AUTOHIDE_CLICK,
                              _fHideOnClick);

        // V0.9.19 (2002-05-07) [umoeller]
        winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_CRDI_VIEW_AUTOSCREENBORDER,
                              _fAutoScreenBorder);

        // priority
        winhSetSliderArmPosition(WinWindowFromID(pnbp->hwndDlgPage,
                                                 ID_CRDI_VIEW_PRTY_SLIDER),
                                 SMA_INCREMENTVALUE,
                                 _lPriorityDelta);
        WinSetDlgItemShort(pnbp->hwndDlgPage,
                           ID_CRDI_VIEW_PRTY_TEXT,
                           _lPriorityDelta,
                           FALSE);      // unsigned

        if (_ulPosition == XCENTER_TOP)
            winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_CRDI_VIEW_TOPOFSCREEN, TRUE);
        else
            winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_CRDI_VIEW_BOTTOMOFSCREEN, TRUE);

    }

    if (flFlags & CBI_ENABLE)
    {
        BOOL fAutoHide =    (_fReduceDesktopWorkarea == FALSE)
                         && (_ulAutoHide != 0);

        // disable auto-hide if workarea is to be reduced
        WinEnableControl(pnbp->hwndDlgPage, ID_CRDI_VIEW_AUTOHIDE,
                          (_fReduceDesktopWorkarea == FALSE));
        WinEnableControl(pnbp->hwndDlgPage, ID_CRDI_VIEW_AUTOHIDE_TXT1,
                          fAutoHide);
        WinEnableControl(pnbp->hwndDlgPage, ID_CRDI_VIEW_AUTOHIDE_SLIDER,
                          fAutoHide);
        WinEnableControl(pnbp->hwndDlgPage, ID_CRDI_VIEW_AUTOHIDE_TXT2,
                          fAutoHide);
        WinEnableControl(pnbp->hwndDlgPage, ID_CRDI_VIEW_AUTOHIDE_CLICK,
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
 *@@changed V0.9.14 (2001-08-21) [umoeller]: added "hide on click"
 *@@changed V0.9.19 (2002-04-17) [umoeller]: now automatically making XCenter screen border object
 */

MRESULT ctrpView1ItemChanged(PNOTEBOOKPAGE pnbp,
                             ULONG ulItemID,
                             USHORT usNotifyCode,
                             ULONG ulExtra)      // for checkboxes: contains new state
{
    MRESULT     mrc = 0;
    XCenterData *somThis = XCenterGetData(pnbp->inbp.somSelf);
    BOOL        fSave = TRUE;
    ULONG       ulCallInitCallback = 0;

    ULONG       ulUpdateFlags = XFMF_DISPLAYSTYLECHANGED;

    switch (ulItemID)
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

            // make sure the XCenter is the current screen
            // border object for the matching border
            // V0.9.19 (2002-04-17) [umoeller]
            hifSetScreenBorderObjectUnique(SCREENCORNER_TOP,
                                           _wpQueryHandle(pnbp->inbp.somSelf));
        break;

        case ID_CRDI_VIEW_BOTTOMOFSCREEN:
            _ulPosition = XCENTER_BOTTOM;

            // make sure the XCenter is the current screen
            // border object for the matching border
            // V0.9.19 (2002-04-17) [umoeller]
            hifSetScreenBorderObjectUnique(SCREENCORNER_BOTTOM,
                                           _wpQueryHandle(pnbp->inbp.somSelf));
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
                LONG lSliderIndex = winhQuerySliderArmPosition(pnbp->hwndControl,
                                                               SMA_INCREMENTVALUE);
                WinSetDlgItemShort(pnbp->hwndDlgPage,
                                   ID_CRDI_VIEW_AUTOHIDE_TXT2,
                                   lSliderIndex + 1,
                                   FALSE);      // unsigned
                _ulAutoHide = (lSliderIndex + 1) * 1000;
                            // range is 0 thru 59
                ulUpdateFlags = 0;      // no complete reformat
            }
        break;

        case ID_CRDI_VIEW_AUTOHIDE_CLICK:
            _fHideOnClick = ulExtra;
        break;

        // V0.9.19 (2002-05-07) [umoeller]
        case ID_CRDI_VIEW_AUTOSCREENBORDER:
            _fAutoScreenBorder = ulExtra;
        break;

        case ID_CRDI_VIEW_PRTY_SLIDER:
        {
            LONG lSliderIndex = winhQuerySliderArmPosition(pnbp->hwndControl,
                                                           SMA_INCREMENTVALUE);
            WinSetDlgItemShort(pnbp->hwndDlgPage,
                               ID_CRDI_VIEW_PRTY_TEXT,
                               lSliderIndex,
                               FALSE);      // unsigned
            // _lPriorityDelta = lSliderIndex;
            _xwpSetPriority(pnbp->inbp.somSelf,
                            _ulPriorityClass,        // unchanged
                            lSliderIndex);
            ulUpdateFlags = 0;
        }
        break;

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
            XCenterData *pBackup = (XCenterData*)pnbp->pUser;
            cmnSetupRestoreBackup(G_aulView1SetupOffsets,
                                  ARRAYITEMCOUNT(G_aulView1SetupOffsets),
                                  somThis,
                                  pBackup);
            ulCallInitCallback = CBI_SET | CBI_ENABLE;
        }
        break;

        default:
            fSave = FALSE;
    }

    if (ulCallInitCallback)
        // call the init callback to refresh the page controls
        pnbp->inbp.pfncbInitPage(pnbp, ulCallInitCallback);

    if (fSave)
    {
        _wpSaveDeferred(pnbp->inbp.somSelf);

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

    return mrc;
}

/* ******************************************************************
 *
 *   "View 2" ("Style") page notebook callbacks (notebook.c)
 *
 ********************************************************************/

static SLDCDATA
        BorderWidthSliderCData =
             {
                     sizeof(SLDCDATA),
            // usScale1Increments:
                     11,          // scale 1 increments
                     0,         // scale 1 spacing
                     1,          // scale 2 increments
                     0           // scale 2 spacing
             },
        BorderSpacingSliderCData =
             {
                     sizeof(SLDCDATA),
            // usScale1Increments:
                     11,          // scale 1 increments
                     0,         // scale 1 spacing
                     1,          // scale 2 increments
                     0           // scale 2 spacing
             },
        WidgetSpacingSliderCData =
             {
                     sizeof(SLDCDATA),
            // usScale1Increments:
                     10,          // scale 1 increments
                     0,         // scale 1 spacing
                     1,          // scale 2 increments
                     0           // scale 2 spacing
             };

#define STYLE_SLIDERS_WIDTH         SLIDER_WIDTH        // from above
#define STYLE_SLIDERS_HEIGHT        15
#define STYLE_SLIDERTEXT_WIDTH      DESCRTXT_WIDTH      // from above

static const CONTROLDEF
    BorderWidthGroup = LOADDEF_GROUP(ID_CRDI_VIEW2_3DBORDER_GROUP, HALF_TABLE_WIDTH),
    BorderWidthSlider =
        {
                WC_SLIDER,
                NULL,
                WS_VISIBLE | WS_TABSTOP | WS_GROUP
                    | SLS_HORIZONTAL
                    | SLS_PRIMARYSCALE1
                    | SLS_BUTTONSRIGHT
                    | SLS_SNAPTOINCREMENT,
                ID_CRDI_VIEW2_3DBORDER_SLIDER,
                CTL_COMMON_FONT,
                0,
                { STYLE_SLIDERS_WIDTH, STYLE_SLIDERS_HEIGHT },     // size
                COMMON_SPACING,
                &BorderWidthSliderCData
        },
    BorderWidthText = CONTROLDEF_TEXT_CENTER(
                            "M",           // to be replaced
                            ID_CRDI_VIEW2_3DBORDER_TEXT,
                            STYLE_SLIDERTEXT_WIDTH,
                            -1),
    DrawAll3DBordersCB = LOADDEF_AUTOCHECKBOX(ID_CRDI_VIEW2_ALL3DBORDERS),
    BorderSpacingGroup = LOADDEF_GROUP(ID_CRDI_VIEW2_BDRSPACE_GROUP, HALF_TABLE_WIDTH),
    BorderSpacingSlider =
        {
                WC_SLIDER,
                NULL,
                WS_VISIBLE | WS_TABSTOP | WS_GROUP
                    | SLS_HORIZONTAL
                    | SLS_PRIMARYSCALE1
                    | SLS_BUTTONSRIGHT
                    | SLS_SNAPTOINCREMENT,
                ID_CRDI_VIEW2_BDRSPACE_SLIDER,
                CTL_COMMON_FONT,
                0,
                { STYLE_SLIDERS_WIDTH, STYLE_SLIDERS_HEIGHT },     // size
                COMMON_SPACING,
                &BorderSpacingSliderCData
        },
    BorderSpacingText = CONTROLDEF_TEXT_CENTER(
                            "M",           // to be replaced
                            ID_CRDI_VIEW2_BDRSPACE_TEXT,
                            STYLE_SLIDERTEXT_WIDTH,
                            -1),
    WidgetSpacingGroup = LOADDEF_GROUP(ID_CRDI_VIEW2_WGTSPACE_GROUP, HALF_TABLE_WIDTH),
    WidgetSpacingSlider =
        {
                WC_SLIDER,
                NULL,
                WS_VISIBLE | WS_TABSTOP | WS_GROUP
                    | SLS_HORIZONTAL
                    | SLS_PRIMARYSCALE1
                    | SLS_BUTTONSRIGHT
                    | SLS_SNAPTOINCREMENT,
                ID_CRDI_VIEW2_WGTSPACE_SLIDER,
                CTL_COMMON_FONT,
                0,
                { STYLE_SLIDERS_WIDTH, STYLE_SLIDERS_HEIGHT },     // size
                COMMON_SPACING,
                &WidgetSpacingSliderCData
        },
    WidgetSpacingText = CONTROLDEF_TEXT_CENTER(
                            "M",           // to be replaced
                            ID_CRDI_VIEW2_WGTSPACE_TEXT,
                            STYLE_SLIDERTEXT_WIDTH,
                            -1),
    SizingBarsCB = LOADDEF_AUTOCHECKBOX(ID_CRDI_VIEW2_SIZINGBARS),
    SpacingLinesCB = LOADDEF_AUTOCHECKBOX(ID_CRDI_VIEW2_SPACINGLINES),
    DefWidgetStylesGroup = LOADDEF_GROUP(ID_CRDI_VIEW2_DEFSTYLES_GROUP, HALF_TABLE_WIDTH),

    FlatButtonsCB = LOADDEF_AUTOCHECKBOX(ID_CRDI_VIEW2_FLATBUTTONS),
    SunkBordersCB = LOADDEF_AUTOCHECKBOX(ID_CRDI_VIEW2_SUNKBORDERS),
    HatchInUseCB = LOADDEF_AUTOCHECKBOX(ID_CRDI_VIEW2_HATCHINUSE);

static const DLGHITEM G_dlgXCenterStyle[] =
    {
        START_TABLE,            // root table, required
            START_ROW(0),
                START_GROUP_TABLE(&BorderWidthGroup),
                    START_ROW(ROW_VALIGN_CENTER),
                        CONTROL_DEF(&BorderWidthSlider),
                        CONTROL_DEF(&BorderWidthText),
                    START_ROW(0),
                        CONTROL_DEF(&DrawAll3DBordersCB),
                END_TABLE,
            // START_ROW(0),
                START_GROUP_TABLE(&BorderSpacingGroup),
                    START_ROW(ROW_VALIGN_CENTER),
                        CONTROL_DEF(&BorderSpacingSlider),
                        CONTROL_DEF(&BorderSpacingText),
                END_TABLE,
            START_ROW(0),
                START_GROUP_TABLE(&WidgetSpacingGroup),
                    START_ROW(ROW_VALIGN_CENTER),
                        CONTROL_DEF(&WidgetSpacingSlider),
                        CONTROL_DEF(&WidgetSpacingText),
                    START_ROW(0),
                        CONTROL_DEF(&SizingBarsCB),
                    START_ROW(0),
                        CONTROL_DEF(&SpacingLinesCB),
                END_TABLE,
            // START_ROW(0),
                START_GROUP_TABLE(&DefWidgetStylesGroup),
                    START_ROW(0),
                        CONTROL_DEF(&FlatButtonsCB),
                    START_ROW(0),
                        CONTROL_DEF(&SunkBordersCB),
                    START_ROW(0),
                        CONTROL_DEF(&HatchInUseCB),
                END_TABLE,
            START_ROW(0),       // notebook buttons (will be moved)
                CONTROL_DEF(&G_UndoButton),         // common.c
                CONTROL_DEF(&G_DefaultButton),      // common.c
                CONTROL_DEF(&G_HelpButton),         // common.c
        END_TABLE
    };


/*
 *@@ ctrpView2InitPage:
 *      notebook callback function (notebook.c) for the
 *      first XCenter "View" instance settings page.
 *      Sets the controls on the page according to the
 *      instance settings.
 *
 *@@added V0.9.7 (2000-12-05) [umoeller]
 *@@changed V0.9.9 (2001-01-29) [umoeller]: "Undo" data wasn't working
 *@@changed V0.9.13 (2001-06-19) [umoeller]: added spacing lines setting
 *@@changed V0.9.16 (2001-10-24) [umoeller]: now using dialog formatter
 *@@changed V0.9.16 (2001-10-24) [umoeller]: added hatch-in-use setting
 */

VOID ctrpView2InitPage(PNOTEBOOKPAGE pnbp,   // notebook info struct
                       ULONG flFlags)        // CBI_* flags (notebook.h)
{
    XCenterData *somThis = XCenterGetData(pnbp->inbp.somSelf);

    if (flFlags & CBI_INIT)
    {
        // make backup of instance data for "Undo"
        XCenterData *pBackup;
        if (pBackup = (XCenterData*)malloc(sizeof(*somThis)))
            memcpy(pBackup, somThis, sizeof(*somThis));
            // be careful about using the copy... we have some pointers in there!
        // store in notebook struct
        pnbp->pUser = pBackup;

        // insert the controls using the dialog formatter
        // V0.9.16 (2001-10-24) [umoeller]
        ntbFormatPage(pnbp->hwndDlgPage,
                      G_dlgXCenterStyle,
                      ARRAYITEMCOUNT(G_dlgXCenterStyle));

        winhSetSliderTicks(WinWindowFromID(pnbp->hwndDlgPage,
                                           ID_CRDI_VIEW2_3DBORDER_SLIDER),
                           (MPARAM)0, 3,
                           MPFROM2SHORT(4, 5), 6);

        winhSetSliderTicks(WinWindowFromID(pnbp->hwndDlgPage,
                                           ID_CRDI_VIEW2_BDRSPACE_SLIDER),
                           (MPARAM)0, 3,
                           MPFROM2SHORT(4, 5), 6);

        winhSetSliderTicks(WinWindowFromID(pnbp->hwndDlgPage,
                                           ID_CRDI_VIEW2_WGTSPACE_SLIDER),
                           (MPARAM)0, 3,
                           MPFROM2SHORT(4, 5), 6);
    }

    if (flFlags & CBI_SET)
    {
        HWND hwndSlider;

        // 3D borders
        hwndSlider = WinWindowFromID(pnbp->hwndDlgPage, ID_CRDI_VIEW2_3DBORDER_SLIDER);
        winhSetSliderArmPosition(hwndSlider,
                                 SMA_INCREMENTVALUE,
                                 _ul3DBorderWidth);     // slider scale is from 0 to 10
        WinSetDlgItemShort(pnbp->hwndDlgPage,
                           ID_CRDI_VIEW_PRTY_TEXT,
                           _ul3DBorderWidth,
                           FALSE);      // unsigned
        winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_CRDI_VIEW2_ALL3DBORDERS,
                              ((_flDisplayStyle & XCS_ALL3DBORDERS) != 0));

        // border spacing
        hwndSlider = WinWindowFromID(pnbp->hwndDlgPage, ID_CRDI_VIEW2_BDRSPACE_SLIDER);
        winhSetSliderArmPosition(hwndSlider,
                                 SMA_INCREMENTVALUE,
                                 _ulBorderSpacing);     // slider scale is from 0 to 10
        WinSetDlgItemShort(pnbp->hwndDlgPage,
                           ID_CRDI_VIEW_PRTY_TEXT,
                           _ulBorderSpacing,
                           FALSE);      // unsigned

        // widget spacing
        hwndSlider = WinWindowFromID(pnbp->hwndDlgPage, ID_CRDI_VIEW2_WGTSPACE_SLIDER);
        winhSetSliderArmPosition(hwndSlider,
                                 SMA_INCREMENTVALUE,
                                 _ulWidgetSpacing - 1);     // slider scale is from 0 to 9
        WinSetDlgItemShort(pnbp->hwndDlgPage,
                           ID_CRDI_VIEW_PRTY_TEXT,
                           _ulWidgetSpacing,
                           FALSE);      // unsigned
        winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_CRDI_VIEW2_SIZINGBARS,
                              ((_flDisplayStyle & XCS_SIZINGBARS) != 0));

        winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_CRDI_VIEW2_SPACINGLINES,
                              ((_flDisplayStyle & XCS_SPACINGLINES) != 0));
                    // added V0.9.13 (2001-06-19) [umoeller]

        // default widget styles
        winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_CRDI_VIEW2_FLATBUTTONS,
                            ((_flDisplayStyle & XCS_FLATBUTTONS) != 0));
        winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_CRDI_VIEW2_SUNKBORDERS,
                            ((_flDisplayStyle & XCS_SUNKBORDERS) != 0));
        // V0.9.16 (2001-10-24) [umoeller]
        winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_CRDI_VIEW2_HATCHINUSE,
                            ((_flDisplayStyle & XCS_NOHATCHINUSE) == 0));
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
 *@@changed V0.9.13 (2001-06-19) [umoeller]: added spacing lines setting
 *@@changed V0.9.16 (2001-10-24) [umoeller]: added hatch-in-use setting
 */

MRESULT ctrpView2ItemChanged(PNOTEBOOKPAGE pnbp,
                             ULONG ulItemID,
                             USHORT usNotifyCode,
                             ULONG ulExtra)      // for checkboxes: contains new state
{
    MRESULT     mrc = 0;
    XCenterData *somThis = XCenterGetData(pnbp->inbp.somSelf);
    BOOL        fSave = TRUE;
                // fDisplayStyleChanged = FALSE;
    LONG        lSliderIndex;
    ULONG       ulDisplayFlagChanged = 0;

    switch (ulItemID)
    {
        case ID_CRDI_VIEW2_3DBORDER_SLIDER:
            lSliderIndex = winhQuerySliderArmPosition(pnbp->hwndControl,
                                                      SMA_INCREMENTVALUE);
            WinSetDlgItemShort(pnbp->hwndDlgPage,
                               ID_CRDI_VIEW2_3DBORDER_TEXT,
                               lSliderIndex,
                               FALSE);      // unsigned
            _ul3DBorderWidth = lSliderIndex;
        break;

        case ID_CRDI_VIEW2_ALL3DBORDERS:
            ulDisplayFlagChanged = XCS_ALL3DBORDERS;
        break;

        case ID_CRDI_VIEW2_BDRSPACE_SLIDER:
            lSliderIndex = winhQuerySliderArmPosition(pnbp->hwndControl,
                                                      SMA_INCREMENTVALUE);
            WinSetDlgItemShort(pnbp->hwndDlgPage,
                               ID_CRDI_VIEW2_BDRSPACE_TEXT,
                               lSliderIndex,
                               FALSE);      // unsigned
            _ulBorderSpacing = lSliderIndex;
        break;

        case ID_CRDI_VIEW2_WGTSPACE_SLIDER:
            lSliderIndex = winhQuerySliderArmPosition(pnbp->hwndControl,
                                                      SMA_INCREMENTVALUE);
            WinSetDlgItemShort(pnbp->hwndDlgPage,
                               ID_CRDI_VIEW2_WGTSPACE_TEXT,
                               lSliderIndex + 1,
                               FALSE);      // unsigned
            _ulWidgetSpacing = lSliderIndex + 1;
        break;

        case ID_CRDI_VIEW2_SIZINGBARS:
            ulDisplayFlagChanged = XCS_SIZINGBARS;
        break;

        case ID_CRDI_VIEW2_SPACINGLINES:
            ulDisplayFlagChanged = XCS_SPACINGLINES;
        break;

        case ID_CRDI_VIEW2_FLATBUTTONS:
            ulDisplayFlagChanged = XCS_FLATBUTTONS;
        break;

        case ID_CRDI_VIEW2_SUNKBORDERS:
            ulDisplayFlagChanged = XCS_SUNKBORDERS;
        break;

        case ID_CRDI_VIEW2_HATCHINUSE:              // V0.9.16 (2001-10-24) [umoeller]
            // note, this one is reversed
            if (!ulExtra)
                _flDisplayStyle |= XCS_NOHATCHINUSE;
            else
                _flDisplayStyle &= ~XCS_NOHATCHINUSE;
        break;

        case DID_DEFAULT:
            cmnSetupSetDefaults(G_XCenterSetupSet,
                                ARRAYITEMCOUNT(G_XCenterSetupSet),
                                G_aulView2SetupOffsets,
                                ARRAYITEMCOUNT(G_aulView2SetupOffsets),
                                somThis);
            // call the init callback to refresh the page controls
            pnbp->inbp.pfncbInitPage(pnbp, CBI_SET | CBI_ENABLE);
        break;

        case DID_UNDO:
        {
            XCenterData *pBackup = (XCenterData*)pnbp->pUser;
            cmnSetupRestoreBackup(G_aulView2SetupOffsets,
                                  ARRAYITEMCOUNT(G_aulView2SetupOffsets),
                                  somThis,
                                  pBackup);
            // call the init callback to refresh the page controls
            pnbp->inbp.pfncbInitPage(pnbp, CBI_SET | CBI_ENABLE);
        }
        break;

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
        _wpSaveDeferred(pnbp->inbp.somSelf);

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

    return mrc;
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
            // pszIcon contains the widget's class name

    // ULONG           ulRootIndex;
            // index of widget if it's a root widget;
            // otherwise (subwidget in some tray), this is -1

    /* ULONG           ulParentIndex,
                    ulTrayIndex,
                    ulSubwidgetIndex;
       */

    WIDGETPOSITION  Position;           // V0.9.16 (2001-12-31) [umoeller]

    PSZ             pszIndex;           // points to szIndex
    CHAR            szIndex[40];

    const char      *pcszSetupString;
            // widget's setup string; points into instance data, do not free!

} WIDGETRECORD, *PWIDGETRECORD;

PFNWP G_pfnwpWidgetsCnr = NULL;

/*
 *@@ fnwpWidgetsCnr:
 *      subclassed window proc for the widgets container.  Handles
 *      DM_RENDER and DM_DISCARDOBJECT messages.
 *
 *@@added V0.9.14 (2001-07-29) [lafaix]
 */

static MRESULT EXPENTRY fnwpWidgetsCnr(HWND hwndCnr,
                                       ULONG msg,
                                       MPARAM mp1,
                                       MPARAM mp2)
{
    switch (msg)
    {
        case DM_RENDER:
        {
            PDRAGTRANSFER pdt = (PDRAGTRANSFER)mp1;
            MRESULT       mrc = (MRESULT)FALSE;

            if (DrgVerifyRMF(pdt->pditem, "DRM_OS2FILE", NULL))
            {
                CHAR          ach[CCHMAXPATH];
                PWIDGETRECORD pwr = (PWIDGETRECORD)pdt->pditem->ulItemID;
                BOOL          bSuccess;

                DrgQueryStrName(pdt->hstrRenderToName,
                                CCHMAXPATH,
                                ach);

                bSuccess = ctrpSaveToFile(ach,
                                          pwr->recc.pszIcon,
                                          pwr->pcszSetupString);

                WinPostMsg(pdt->hwndClient,
                           DM_RENDERCOMPLETE,
                           MPFROMP(pdt),
                           (bSuccess) ? MPFROMSHORT(DMFL_RENDEROK)
                                      : MPFROMSHORT(DMFL_RENDERFAIL));

                mrc = (MRESULT)TRUE;
            }

            DrgFreeDragtransfer(pdt);

            return mrc;
        }

    /* ??? This part does not work.  DM_DISCARDOBJECT is only received
       ??? ONCE per session.  I have absolutely no idea why.  Will check
       ??? another day. // @@todo
       ??? V0.9.14 (2001-07-30) [lafaix]
    */

        case DM_DISCARDOBJECT:
        {
            PDRAGINFO pdi;
_Pmpf(("DM_DISCARDOBJECT"));
DosBeep(100, 100);
/*            PDRAGINFO pdi;
            PWIDGETRECORD prec;
            PCREATENOTEBOOKPAGE pcnbp;

            if (    (pdi = (PDRAGINFO)mp1)
                 && (pditem = DrgQueryDragitemPtr(pdi, 0))
                 && (prec = (PWIDGETRECORD)pditem->ulItemID)
                 && (pcnbp = (PCREATENOTEBOOKPAGE)WinQueryWindowULong(hwndCnr, QWL_USER))
               )
            {
                if (prec->ulRootIndex != -1)        // @@todo
                    _xwpDeleteWidget(pnbp->inbp.somSelf,
                                     prec->ulRootIndex);
                          // this saves the instance data
                          // and updates the view
                          // and also calls the init callback
                          // to update the settings page!

                return (MRESULT)DRR_SOURCE;
            }
            else
                _Pmpf(("DM_DISCARDOBJECT NULL"));
*/        }
    }

    return (G_pfnwpWidgetsCnr(hwndCnr, msg, mp1, mp2));
}

/*
 *@@ InsertWidgetSetting:
 *
 *@@added V0.9.13 (2001-06-23) [umoeller]
 */

static PWIDGETRECORD InsertWidgetSetting(HWND hwndCnr,
                                         PPRIVATEWIDGETSETTING pSetting,
                                         const WIDGETPOSITION *pPosition)
                                  /*
                                  ULONG ulRootIndex,    // in: if -1, this is a subwidget;
                                                        // otherwise the widget index
                                  ULONG ulParentIndex,
                                  ULONG ulTrayIndex,
                                  ULONG ulSubwidgetIndex) */
{
    PWIDGETRECORD preccThis = (PWIDGETRECORD)cnrhAllocRecords(hwndCnr,
                                                              sizeof(WIDGETRECORD),
                                                              1);
    memcpy(&preccThis->Position, pPosition, sizeof(WIDGETPOSITION));
    /* preccThis->ulRootIndex = ulRootIndex;
    preccThis->ulParentIndex = ulParentIndex;
    preccThis->ulTrayIndex = ulTrayIndex;
    preccThis->ulSubwidgetIndex = ulSubwidgetIndex; */
    if (pPosition->ulTrayWidgetIndex != -1)
    {
        // subwidget:
        sprintf(preccThis->szIndex,
                "  %d.%d.%d",
                pPosition->ulTrayWidgetIndex,
                pPosition->ulTrayIndex,
                pPosition->ulWidgetIndex);
    }
    else
    {
        sprintf(preccThis->szIndex,
                "%d",
                pPosition->ulWidgetIndex);
    }
    preccThis->pszIndex = preccThis->szIndex;
    preccThis->recc.pszIcon = pSetting->Public.pszWidgetClass;
    preccThis->pcszSetupString = pSetting->Public.pszSetupString;

    cnrhInsertRecords(hwndCnr,
                      NULL,         // parent
                      (PRECORDCORE)preccThis,
                      TRUE,         // invalidate
                      NULL,
                      CRA_RECORDREADONLY,
                      1);

    return (preccThis);
}

/*
 *@@ ctrpWidgetsInitPage:
 *      notebook callback function (notebook.c) for the
 *      XCenter "Widgets" instance settings page.
 *      Sets the controls on the page according to the
 *      instance settings.
 *
 *      CREATENOTEBOOKPAGE.pUser is used for the widget
 *      context menu HWND.
 *
 *@@added V0.9.9 (2001-03-09) [umoeller]
 *@@changed V0.9.12 (2001-05-08) [lafaix]: forced class loading/unloading
 */

VOID ctrpWidgetsInitPage(PNOTEBOOKPAGE pnbp,   // notebook info struct
                         ULONG flFlags)        // CBI_* flags (notebook.h)
{
    // XCenterData *somThis = XCenterGetData(pnbp->inbp.somSelf);

    if (flFlags & CBI_INIT)
    {
        // PNLSSTRINGS     pNLSStrings = cmnQueryNLSStrings();
        HWND hwndCnr = WinWindowFromID(pnbp->hwndDlgPage, ID_XFDI_CNR_CNR);
        XFIELDINFO      xfi[5];
        PFIELDINFO      pfi = NULL;
        int             i = 0;

        // set group cnr title
        WinSetDlgItemText(pnbp->hwndDlgPage, ID_XFDI_CNR_GROUPTITLE,
                          cmnGetString(ID_XSSI_WIDGETSPAGE)) ; // pszWidgetsPage

        // set up cnr details view
        xfi[i].ulFieldOffset = FIELDOFFSET(WIDGETRECORD, pszIndex);
        xfi[i].pszColumnTitle = "";
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        xfi[i].ulFieldOffset = FIELDOFFSET(RECORDCORE, pszIcon);
        xfi[i].pszColumnTitle = cmnGetString(ID_XSSI_WIDGETCLASS);  // "Class"; // pszWidgetClass
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        xfi[i].ulFieldOffset = FIELDOFFSET(WIDGETRECORD, pcszSetupString);
        xfi[i].pszColumnTitle = cmnGetString(ID_XSSI_WIDGETSETUP);  // "Setup"; // pszWidgetSetup
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

        ctrpLoadClasses();

        // we must subclass the widgets container, so that we can handle
        // the DM_DISCARDOBJECT and DM_RENDER msgs
        // V0.9.14 (2001-07-29) [lafaix]
        G_pfnwpWidgetsCnr = WinSubclassWindow(hwndCnr, fnwpWidgetsCnr);

        // we also need a copy of pcnbp, so that we can easily handle
        // DM_DISCARDOBJECT without having to climb up the window
        // hierarchy
        // V0.9.14 (2001-07-30) [lafaix]
        WinSetWindowULong(hwndCnr, QWL_USER, (ULONG)pnbp);
    }

    if (flFlags & CBI_SET)
    {
        HWND        hwndCnr = WinWindowFromID(pnbp->hwndDlgPage, ID_XFDI_CNR_CNR);
        PLINKLIST   pllWidgets = ctrpQuerySettingsList(pnbp->inbp.somSelf);
        PLISTNODE   pNode = lstQueryFirstNode(pllWidgets);
        ULONG       ulIndex = 0;

        cnrhRemoveAll(hwndCnr);

        while (pNode)
        {
            PPRIVATEWIDGETSETTING pSetting = (PPRIVATEWIDGETSETTING)pNode->pItemData;
            WIDGETPOSITION Pos;
            Pos.ulTrayWidgetIndex = -1;
            Pos.ulTrayIndex = -1;
            Pos.ulWidgetIndex = ulIndex;
            InsertWidgetSetting(hwndCnr,
                                pSetting,
                                &Pos);

            if (pSetting->pllTraySettings)
            {
                PLISTNODE pTrayNode = lstQueryFirstNode(pSetting->pllTraySettings);
                // ULONG ulTray = 0;

                Pos.ulTrayWidgetIndex = ulIndex;
                Pos.ulTrayIndex = 0;

                while (pTrayNode)
                {
                    PTRAYSETTING pTray = (PTRAYSETTING)pTrayNode->pItemData;
                    // ULONG ulSubwidget = 0;
                    PLISTNODE pSubwidgetNode = lstQueryFirstNode(&pTray->llSubwidgetSettings);

                    Pos.ulWidgetIndex = 0;

                    while (pSubwidgetNode)
                    {
                        PPRIVATEWIDGETSETTING pSubwidget = (PPRIVATEWIDGETSETTING)pSubwidgetNode->pItemData;

                        InsertWidgetSetting(hwndCnr,
                                            pSubwidget,
                                            &Pos);
                                            /* -1,     // non-root
                                            ulIndex,
                                            ulTray,
                                            ulSubwidget); */

                        // ulSubwidget++;
                        (Pos.ulWidgetIndex)++;
                        pSubwidgetNode = pSubwidgetNode->pNext;
                    }

                    pTrayNode = pTrayNode->pNext;
                    // ulTray++;
                    (Pos.ulTrayIndex)++;
                }
            }

            pNode = pNode->pNext;
            ulIndex++;
        }
    }

    if (flFlags & CBI_ENABLE)
    {
    }

    if (flFlags & CBI_DESTROY)
    {
        if (pnbp->pUser)
            // unload menu
            WinDestroyWindow((HWND)pnbp->pUser);

        pnbp->pUser = NULL;

        ctrpFreeClasses();
    }
}

// define a new rendering mechanism, which only
// our own container supports (this will make
// sure that we can only do d'n'd within this
// one container)
#define WIDGET_RMF_MECH     "DRM_XWPXCENTERWIDGET"
#define WIDGET_RMF_FORMAT   "DRF_XWPWIDGETRECORD"
// #define WIDGET_PRIVATE_RMF  "(" WIDGET_RMF_MECH ")x(" WIDGET_RMF_FORMAT ")"
#define WIDGET_PRIVATE_RMF  "(" WIDGET_RMF_MECH ",DRM_OS2FILE,DRM_DISCARD)x(" WIDGET_RMF_FORMAT ")"

static PWIDGETRECORD G_precDragged = NULL,
                     G_precAfter = NULL;

/*
 *@@ ctrpWidgetsItemChanged:
 *      notebook callback function (notebook.c) for the
 *      XCenter "Widgets" instance settings page.
 *      Reacts to changes of any of the dialog controls.
 *
 *@@added V0.9.9 (2001-03-09) [umoeller]
 *@@changed V0.9.12 (2001-05-08) [lafaix]: fixed problems if widget class not found
 *@@changed V0.9.14 (2001-07-29) [lafaix]: now handles widget settings files dnd
 */

MRESULT ctrpWidgetsItemChanged(PNOTEBOOKPAGE pnbp,
                               ULONG ulItemID, USHORT usNotifyCode,
                               ULONG ulExtra)      // for checkboxes: contains new state
{
    MRESULT     mrc = 0;
    // XCenterData *somThis = XCenterGetData(pnbp->inbp.somSelf);

    switch (ulItemID)
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
                            // for now, allow only dragging of root widgets
                            // @@todo
                            if (((PWIDGETRECORD)pcdi->pRecord)->Position.ulTrayWidgetIndex == -1)
                                cnrhInitDrag(pcdi->hwndCnr,
                                             pcdi->pRecord,
                                             usNotifyCode,
                                             WIDGET_PRIVATE_RMF,
                                             DO_COPYABLE | DO_MOVEABLE); // DO_MOVEABLE);
                        }
                }
                break;

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
                        if (pcdi->pDragInfo->cditem != 1)
                            usIndicator = DOR_NEVERDROP;
                        else
                        {
                            // we must accept the drop if (1) this is a file
                            // whose type is DRT_WIDGET, or (2) this a widget
                            // from ourselves
                            PDRAGITEM pdrgItem = NULL;

                            if (    // do not allow drag upon whitespace,
                                    // but only between records
                                    (pcdi->pRecord)
                                 && (pdrgItem = DrgQueryDragitemPtr(pcdi->pDragInfo, 0))
                               )
                            {
                                if (    (DrgVerifyRMF(pdrgItem, "DRM_OS2FILE", NULL))
                                     && (ctrpVerifyType(pdrgItem, DRT_WIDGET))
                                   )
                                {
                                    // this is case (1)
                                    G_precAfter = NULL;
                                    // check target record...
                                    if (    (pcdi->pRecord == (PRECORDCORE)CMA_FIRST)
                                         || (pcdi->pRecord == (PRECORDCORE)CMA_LAST)
                                       )
                                    {
                                        // store record after which to insert
                                        G_precAfter = (PWIDGETRECORD)pcdi->pRecord;
                                    }
                                    // do not allow dropping after
                                    // disabled records
                                    else if (!(pcdi->pRecord->flRecordAttr & CRA_DISABLED))
                                        // store record after which to insert
                                        G_precAfter = (PWIDGETRECORD)pcdi->pRecord;

                                    if (G_precAfter)
                                    {
                                        usIndicator = DOR_DROP;
                                        usOp = DO_COPY;
                                    }
                                }
                                else
                                if (pcdi->pDragInfo->hwndSource != pnbp->hwndControl)
                                    // neither case (1) nor (2)
                                    usIndicator = DOR_NEVERDROP;
                                else
                                {
                                    // this is case (2)
                                    if (    (pcdi->pDragInfo->usOperation == DO_DEFAULT)
                                         || (pcdi->pDragInfo->usOperation == DO_MOVE)
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
                            }
                        }

                        DrgFreeDraginfo(pcdi->pDragInfo);
                    }

                    // and return the drop flags
                    mrc = (MRFROM2SHORT(usIndicator, usOp));
                }
                break;

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
                                ulIndex = G_precAfter->Position.ulWidgetIndex + 1; // ulRootIndex + 1;
                            }
                            if (ulIndex != -2)
                                // OK... move the widgets around:
                                _xwpMoveWidget(pnbp->inbp.somSelf,
                                               G_precDragged->Position.ulWidgetIndex, // ulRootIndex,  // from
                                               ulIndex);                // to
                                    // this saves the instance data
                                    // and updates the view
                                    // and also calls the init callback
                                    // to update the settings page!
                        }
                        G_precDragged = NULL;
                    }
                    else
                    // it was not an internal widget drag; it could be a
                    // widget settings file drop, though.  Lets check.
                    {
                        PCNRDRAGINFO pcdi = (PCNRDRAGINFO)ulExtra;

                        if (DrgAccessDraginfo(pcdi->pDragInfo))
                        {
                            PDRAGITEM pdrgItem = DrgQueryDragitemPtr(pcdi->pDragInfo, 0);

                            if (    (pdrgItem)
                                 && (G_precAfter)
                                 && (DrgVerifyRMF(pdrgItem, "DRM_OS2FILE", NULL))
                                 && (ctrpVerifyType(pdrgItem, DRT_WIDGET))
                               )
                            {
                                // that was it.  We must create a new widget
                                // here.
                                CHAR        achCnr[CCHMAXPATH],
                                            achSrc[CCHMAXPATH];

                                // the dnd part guaranties the following
                                // will not overflow
                                DrgQueryStrName(pdrgItem->hstrContainerName,
                                                CCHMAXPATH,
                                                achCnr);
                                DrgQueryStrName(pdrgItem->hstrSourceName,
                                                CCHMAXPATH,
                                                achSrc);

                                if ((strlen(achCnr)+strlen(achSrc)) < (CCHMAXPATH-1))
                                {
                                    CHAR achAll[CCHMAXPATH];
                                    PSZ pszClass, pszSetup;
                                    APIRET arc;

                                    strcpy(achAll, achCnr);
                                    strcat(achAll, achSrc);

                                    if (!(arc = ctrpReadFromFile(achAll,
                                                                 &pszClass,
                                                                 &pszSetup)))
                                    {
                                        // it looks like it is a valid
                                        // widget settings data
                                        ULONG ulIndex = -2;

                                        if (G_precAfter == (PWIDGETRECORD)CMA_FIRST)
                                        {
                                            // copy before index 0
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
                                            ulIndex = G_precAfter->Position.ulWidgetIndex + 1; // ulRootIndex + 1;
                                        }
                                        if (ulIndex != -2)
                                        {
                                            WIDGETPOSITION pos2;
                                            pos2.ulTrayWidgetIndex = -1;
                                            pos2.ulTrayIndex = -1;
                                            pos2.ulWidgetIndex = ulIndex;

                                            _xwpCreateWidget(pnbp->inbp.somSelf,
                                                             pszClass,
                                                             pszSetup,
                                                             &pos2);
                                        }

                                        free(pszClass);
                                        free(pszSetup);
                                    }
                                }
                            }

                            DrgFreeDraginfo(pcdi->pDragInfo);
                        }
                    }
                }
                break;

                /*
                 * CN_CONTEXTMENU:
                 *      cnr context menu requested
                 *      for widget
                 */

                case CN_CONTEXTMENU:
                {
                    HWND    hPopupMenu = NULLHANDLE;

                    // we store the container and recc.
                    // in the CREATENOTEBOOKPAGE structure
                    // so that the notebook.c function can
                    // remove source emphasis later automatically
                    pnbp->hwndSourceCnr = pnbp->hwndControl;
                    pnbp->preccSource = (PRECORDCORE)ulExtra;
                    if (pnbp->preccSource)
                    {
                        PCXCENTERWIDGETCLASS pClass;

                        // popup menu on container recc (not whitespace):

                        if (!pnbp->pUser)
                            // context menu not yet loaded:
                            pnbp->pUser = (PVOID)WinLoadMenu(pnbp->hwndControl,
                                                       cmnQueryNLSModuleHandle(FALSE),
                                                       ID_CRM_WIDGET);

                        if (hPopupMenu = (HWND)pnbp->pUser)
                        {
                            // kill the new menu items for XCenter
                            // properties and close which make no sense here
                            // V0.9.19 (2002-06-15) [umoeller]
                            winhDeleteMenuItem(hPopupMenu, ID_CRMI_SEP3);
                            winhDeleteMenuItem(hPopupMenu, ID_CRM_XCSUB);
                        }

                        // remove the "Help" menu item... we can't display
                        // help without the widget view from here
                        WinSendMsg(hPopupMenu,
                                   MM_DELETEITEM,
                                   MPFROM2SHORT(ID_CRMI_HELP,
                                                FALSE),
                                   0);
                        WinSendMsg(hPopupMenu,
                                   MM_DELETEITEM,
                                   MPFROM2SHORT(ID_CRMI_SEP1,
                                                FALSE),
                                   0);

                        // check if the widget class supports a settings dialog
                        if (!ctrpFindClass(pnbp->preccSource->pszIcon,  // class name
                                           FALSE,       // fMustBeTrayable
                                           &pClass))
                            WinEnableMenuItem(hPopupMenu,
                                              ID_CRMI_PROPERTIES,
                                              (pClass->pShowSettingsDlg != 0));
                        else
                            // V0.9.12 (2001-08-05) [lafaix]
                            WinEnableMenuItem(hPopupMenu,
                                              ID_CRMI_PROPERTIES,
                                              FALSE);
                    }

                    if (hPopupMenu)
                        cnrhShowContextMenu(pnbp->hwndControl,  // cnr
                                            (PRECORDCORE)pnbp->preccSource,
                                            hPopupMenu,
                                            pnbp->hwndDlgPage);    // owner
                }
                break;  // CN_CONTEXTMENU

            } // end switch (usNotifyCode)
        break;

        /*
         * ID_CRMI_PROPERTIES:
         *      command from widget context menu.
         */

        case ID_CRMI_PROPERTIES:
        {
            PWIDGETRECORD           prec;
            PCXCENTERWIDGETCLASS    pClass;

            if (    (prec = (PWIDGETRECORD)pnbp->preccSource)
                 && (!ctrpFindClass(pnbp->preccSource->pszIcon,  // class name
                                    FALSE,
                                    &pClass))
                 && (pClass->pShowSettingsDlg != 0)
               )
            {
                ctrpShowSettingsDlg(pnbp->inbp.somSelf,
                                    pnbp->hwndDlgPage, // owner
                                    &prec->Position);
            }
        }
        break;

        /*
         * ID_CRMI_REMOVEWGT:
         *      command from widget context menu.
         */

        case ID_CRMI_REMOVEWGT:
        {
            PWIDGETRECORD prec;
            if (    (prec = (PWIDGETRECORD)pnbp->preccSource)
                 // && (prec->Position.ulTrayWidgetIndex == -1)    // @@todo
               )
                _xwpDeleteWidget(pnbp->inbp.somSelf,
                                 &prec->Position);
                      // this saves the instance data
                      // and updates the view
                      // and also calls the init callback
                      // to update the settings page!
        }
        break;
    }

    return mrc;
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
 *@@changed V0.9.20 (2002-07-23) [lafaix]: uses generic plugin support now
 */

VOID ctrpClassesInitPage(PNOTEBOOKPAGE pnbp,   // notebook info struct
                         ULONG flFlags)        // CBI_* flags (notebook.h)
{
    if (flFlags & CBI_INIT)
    {
        // PNLSSTRINGS     pNLSStrings = cmnQueryNLSStrings();
        HWND hwndCnr = WinWindowFromID(pnbp->hwndDlgPage, ID_XFDI_CNR_CNR);
        XFIELDINFO      xfi[5];
        PFIELDINFO      pfi = NULL;
        int             i = 0;

        // set group cnr title
        WinSetDlgItemText(pnbp->hwndDlgPage, ID_XFDI_CNR_GROUPTITLE,
                          cmnGetString(ID_XSSI_CLASSESPAGE)) ; // pszClassesPage

        // set up cnr details view
        xfi[i].ulFieldOffset = FIELDOFFSET(XCLASSRECORD, pszDLL);
        xfi[i].pszColumnTitle = "DLL";
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        xfi[i].ulFieldOffset = FIELDOFFSET(XCLASSRECORD, pszClass);
        xfi[i].pszColumnTitle = cmnGetString(ID_XSSI_WIDGETCLASS);  // "Class"; // pszWidgetClass
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        xfi[i].ulFieldOffset = FIELDOFFSET(XCLASSRECORD, pszClassTitle);
        xfi[i].pszColumnTitle = cmnGetString(ID_XSSI_XC_CLASSTITLE);
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        xfi[i].ulFieldOffset = FIELDOFFSET(XCLASSRECORD, pszVersion);
        xfi[i].pszColumnTitle = cmnGetString(ID_XSSI_XC_VERSION);
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
        PLINKLIST   pllClasses;
        ULONG       cClasses;
        if (    (pllClasses = ctrpQueryClasses())
             && (cClasses = lstCountItems(pllClasses))
           )
        {
            HWND hwndCnr = WinWindowFromID(pnbp->hwndDlgPage, ID_XFDI_CNR_CNR);
            PXCLASSRECORD precFirst;

            if (precFirst = (PXCLASSRECORD)cnrhAllocRecords(hwndCnr,
                                                            sizeof(XCLASSRECORD),
                                                            cClasses))
            {
                PXCLASSRECORD precThis = precFirst;
                PLISTNODE pNode = lstQueryFirstNode(pllClasses);
                while (pNode)
                {
                    PPLUGINCLASS pClass = (PPLUGINCLASS)pNode->pItemData;

                    if (!plgIsClassBuiltIn(pClass))
                    {
                        PSZ p = NULL;
                        CHAR sz[CCHMAXPATH];
                        ULONG ulMajor,
                              ulMinor,
                              ulRevision;

                        if (!plgQueryClassModuleName(pClass,
                                                     sizeof(sz),
                                                     sz))
                        {
                            if (p = strrchr(sz, '\\'))
                            {
                                strcpy(precThis->szDLL, p + 1);
                                precThis->pszDLL = precThis->szDLL;
                            }
                        }

                        if (!p)
                            precThis->pszDLL = "Error";

                        if (!plgQueryClassVersion(pClass,
                                                  &ulMajor,
                                                  &ulMinor,
                                                  &ulRevision))
                        {
                            sprintf(precThis->szVersion,
                                    "%d.%d.%d",
                                    ulMajor,
                                    ulMinor,
                                    ulRevision);
                        }

                        precThis->pszVersion = precThis->szVersion;
                    }
                    else
                        precThis->pszDLL = cmnGetString(ID_CRSI_BUILTINCLASS);

                    precThis->pszClass = (PSZ)pClass->pcszClass;

                    precThis->pszClassTitle = (PSZ)pClass->pcszClassTitle;

                    precThis = (PXCLASSRECORD)precThis->recc.preccNextRecord;
                    pNode = pNode->pNext;
                }

                // kah-wump
                cnrhInsertRecords(hwndCnr,
                                  NULL,         // parent
                                  (PRECORDCORE)precFirst,
                                  TRUE,         // invalidate
                                  NULL,
                                  CRA_RECORDREADONLY,
                                  cClasses);
            }
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

MRESULT ctrpClassesItemChanged(PNOTEBOOKPAGE pnbp,
                               ULONG ulItemID,
                               USHORT usNotifyCode,
                               ULONG ulExtra)      // for checkboxes: contains new state
{
    MRESULT     mrc = 0;

    return mrc;
}

