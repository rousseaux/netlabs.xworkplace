
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
 +
 *@@added V0.9.9 (2001-01-29) [umoeller]
 *@@changed V0.9.14 (2001-08-21) [umoeller]: added "hide on click"
 *@@changed V0.9.16 (2001-10-15) [umoeller]: changed defaults
 */

static XWPSETUPENTRY    G_XCenterSetupSet[] =
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
               XCS_FLATBUTTONS | XCS_SUNKBORDERS | XCS_SIZINGBARS | XCS_SPACINGLINES,
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

/*
 *@@ ctrpQuerySetup:
 *      implementation for XCenter::xwpQuerySetup2.
 *
 *@@added V0.9.7 (2000-12-09) [umoeller]
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
        pNode = lstQueryFirstNode(pllSettings);
        if (pNode)
        {
            BOOL    fFirstWidget = TRUE;
            xstrcat(pstrSetup, "WIDGETS=", 0);

            // we have widgets:
            // go thru all of them and list all widget classes and setup strings.
            while (pNode)
            {
                PXCENTERWIDGETSETTING pSetting = (PXCENTERWIDGETSETTING)pNode->pItemData;

                if (!fFirstWidget)
                    // not first run:
                    // add separator
                    xstrcatc(pstrSetup, ',');
                else
                    fFirstWidget = FALSE;

                // add widget class
                xstrcat(pstrSetup, pSetting->pszWidgetClass, 0);

                if (    (pSetting->pszSetupString)
                     && (strlen(pSetting->pszSetupString))
                   )
                {
                    // widget has a setup string:
                    // add that in brackets
                    XSTRING strSetup2;

                    // copy widget setup string to temporary buffer
                    // for encoding... this has "=" and ";"
                    // chars in it, and these should not appear
                    // in the WPS setup string
                    xstrInitCopy(&strSetup2,
                                 pSetting->pszSetupString,
                                 40);

                    // add first separator
                    xstrcatc(pstrSetup, '(');

                    xstrEncode(&strSetup2,
                               "%,();=");

                    // now append encoded widget setup string
                    xstrcats(pstrSetup, &strSetup2);

                    // add terminator
                    xstrcatc(pstrSetup, ')');

                    xstrClear(&strSetup2);
                } // end if (    (pSetting->pszSetupString)...

                pNode = pNode->pNext;
            } // end for widgets

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

    return (brc);
}

/*
 *@@ ctrpSetup:
 *      implementation for XCenter::wpSetup.
 *
 *@@added V0.9.7 (2001-01-25) [umoeller]
 *@@changed V0.9.16 (2001-10-15) [umoeller]: fixed widget clearing which caused a log entry for empty XCenter
 */

BOOL ctrpSetup(XCenter *somSelf,
               PSZ pszSetupString)
{
    XCenterData *somThis = XCenterGetData(somSelf);
    ULONG   cSuccess = 0;

    // scan the standard stuff from the table...
    // this saves us a lot of work.
    BOOL    brc;

    _Pmpf((__FUNCTION__ ": string is \"%s\"", pszSetupString));

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
                // parse the WIDGETS string
                // format is: "widget1,widget2,widget3"
                PSZ pszToken = strtok(pszWidgets, ",");
                if (pszToken)
                {
                    _Pmpf(("got WIDGETS string, nuking existing widgets"));

                    // first of all, remove all existing widgets,
                    // we have a replacement here
                    while (ctrpQueryWidgetsCount(somSelf))       // V0.9.16 (2001-10-15) [umoeller]
                        if (!_xwpRemoveWidget(somSelf,
                                              0))
                        {
                            // error:
                            brc = FALSE;
                            break;
                        }

                    if (brc)
                    {
                        // now take the widgets
                        do
                        {
                            // pszToken now has one widget
                            PSZ pszWidgetClass = NULL;
                            PSZ pszWidgetSetup = NULL;
                            // check if this has brackets with the setup string
                            PSZ pBracket = strchr(pszToken, '(');

                            _Pmpf(("processing token \"%s\"", pszToken));

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
                    }
                } // if (pszToken)

                free(pszWidgets);
            } // end if (    (pszWidgets)...
        }
    }

    return (brc);
}

/*
 *@@ ctrpSetupOnce:
 *      implementation for XCenter::wpSetupOnce.
 *
 *@@added V0.9.16 (2001-10-15) [umoeller]
 */

BOOL ctrpSetupOnce(XCenter *somSelf,
                   PSZ pszSetupString)
{
    BOOL brc = TRUE;

    WPSHLOCKSTRUCT Lock = {0};
    TRY_LOUD(excpt1)
    {
        if (LOCK_OBJECT(Lock, somSelf))
        {
            PLINKLIST pllSettings = ctrpQuerySettingsList(somSelf);
            if (!lstCountItems(pllSettings))
            {
                // we have no widgets:
                // create some

                XSTRING str;
                xstrInitCopy(&str,
                             "WIDGETS=XButton,"
                             "Pulse,"
                             "Tray(WIDTH%3D141%3BCURRENTTRAY%3D0%3B),"
                             "WindowList,"
                             "Time",
                             0);
                // on laptops, add battery widget too
                if (apmhHasBattery())
                    xstrcat(&str,
                             ",Power",
                             0);
                xstrcatc(&str, ';');

                ctrpSetup(somSelf,
                          str.psz);
                xstrClear(&str);
            }
        }
    }
    CATCH(excpt1) {} END_CATCH();

    if (Lock.fLocked)
        _wpReleaseObjectMutexSem(Lock.pObject);

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

    return (brc);
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

    if (DosOpen((PSZ)pszDest,
                &hf,
                &ulAction,
                0L,
                0,
                FILE_OPEN|OPEN_ACTION_CREATE_IF_NEW,
                OPEN_ACCESS_WRITEONLY|OPEN_SHARE_DENYREADWRITE,
                0) == NO_ERROR)
    {
        // Adding a "Widget settings" .TYPE EA
        if ((pfea2l = (PFEA2LIST) malloc(sizeof(FEA2LIST)+sizeof(val))))
        {
            pfea2l->cbList = sizeof(FEA2LIST)+sizeof(val);
            pfea2l->list[0].oNextEntryOffset = 0;
            pfea2l->list[0].fEA = 0;
            pfea2l->list[0].cbName = 5;
            pfea2l->list[0].cbValue = sizeof(val)-6;
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

            if (DosSetFileInfo(hf,
                               FIL_QUERYEASIZE,
                               &eaop2,
                               sizeof(eaop2)) == NO_ERROR)
            {
                // create the file content:

                if (    // first, the widget class name, which cannot
                        // be NULL
                        (pszClass)
                     && (DosWrite(hf,
                                  (PVOID)pszClass,
                                  strlen(pszClass),
                                  &ulAction) == NO_ERROR)
                        // then, a CR/LF marker
                     && (DosWrite(hf,
                                  "\r\n",
                                  2,
                                  &ulAction) == NO_ERROR)
                        // and the setup string, which may be NULL
                     && (    (pszSetup == NULL)
                          || (DosWrite(hf,
                                       (PVOID)pszSetup,
                                       strlen(pszSetup),
                                       &ulAction) == NO_ERROR)
                             )
                   )
                    brc = TRUE;
            }

            free(pfea2l);
        } // end if ((pfea2l = (PFEA2LIST) malloc(...)))

        DosClose(hf);
    }

    return (brc);
}

/*
 *@@ ctrpReadFromFile:
 *      returns a packed representation of the widget.
 *
 *      If not NULL, ppszSetup contains the widget class name,
 *      followed by a '\r\n' pair, and followed by the widget
 *      setup string.
 *
 *      It is the responsability of the caller to free the
 *      memory block pointed to by ppszSetup.
 *
 *      Returns TRUE if the operation was successful (in this case,
 *      *ppszSetup points to the data).  Returns FALSE otherwise (in
 *      this case, *ppszSetup is NULL).
 *
 *@@added V0.9.14 (2001-07-30) [lafaix]
 */

BOOL ctrpReadFromFile(PCSZ pszSource,
                      PSZ *ppszSetup)
{
    HFILE       hf;
    ULONG       ulAction;
    FILESTATUS3 fs3 = {0};
    PSZ         pszBuff = NULL;
    BOOL        brc = FALSE;

    if (DosOpen((PSZ)pszSource,
                &hf,
                &ulAction,
                0L,
                0,
                FILE_OPEN,
                OPEN_ACCESS_READONLY|OPEN_SHARE_DENYWRITE,
                0) == NO_ERROR)
    {
        // we will read the file in just one block,
        // so we must know its size
        if (DosQueryFileInfo(hf,
                             FIL_STANDARD,
                             &fs3,
                             sizeof(fs3)) == NO_ERROR)
        {
            pszBuff = malloc(fs3.cbFile+1);
            if (pszBuff)
            {
                if (DosRead(hf,
                            pszBuff,
                            fs3.cbFile,
                            &ulAction) == NO_ERROR)
                {
                    pszBuff[fs3.cbFile] = 0;
                    brc = TRUE;
                }
                else
                {
                    // an error occured while reading data; we must free
                    // our buffer so that no memory leaks
                    free(pszBuff);
                    pszBuff = NULL;
                }
            }
        }

        DosClose(hf);
    }

    *ppszSetup = pszBuff;

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
 *@@changed V0.9.14 (2001-08-21) [umoeller]: added "hide on click"
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

        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_CRDI_VIEW_AUTOHIDE_CLICK,
                              _fHideOnClick);

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
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_CRDI_VIEW_AUTOHIDE,
                          (_fReduceDesktopWorkarea == FALSE));
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_CRDI_VIEW_AUTOHIDE_TXT1,
                          fAutoHide);
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_CRDI_VIEW_AUTOHIDE_SLIDER,
                          fAutoHide);
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_CRDI_VIEW_AUTOHIDE_TXT2,
                          fAutoHide);
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_CRDI_VIEW_AUTOHIDE_CLICK,
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
 */

MRESULT ctrpView1ItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                             ULONG ulItemID, USHORT usNotifyCode,
                             ULONG ulExtra)      // for checkboxes: contains new state
{
    MRESULT     mrc = 0;
    XCenterData *somThis = XCenterGetData(pcnbp->somSelf);
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

        case ID_CRDI_VIEW_AUTOHIDE_CLICK:
            _fHideOnClick = ulExtra;
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

/* ******************************************************************
 *
 *   "View 2" ("Style") page notebook callbacks (notebook.c)
 *
 ********************************************************************/

SLDCDATA
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

#define STYLE_SLIDERS_WIDTH        150
#define STYLE_SLIDERS_HEIGHT        30
#define STYLE_SLIDERTEXT_WIDTH      30

static CONTROLDEF
    BorderWidthGroup = CONTROLDEF_GROUP(
                            LOAD_STRING,
                            ID_CRDI_VIEW2_3DBORDER_GROUP),
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
                5,               // spacing
                &BorderWidthSliderCData
        },
    BorderWidthText = CONTROLDEF_TEXT(
                            "M",           // to be replaced
                            ID_CRDI_VIEW2_3DBORDER_TEXT,
                            STYLE_SLIDERTEXT_WIDTH,
                            -1),
    DrawAll3DBordersCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,
                            ID_CRDI_VIEW2_ALL3DBORDERS,
                            -1,
                            -1),
    BorderSpacingGroup = CONTROLDEF_GROUP(
                            LOAD_STRING,
                            ID_CRDI_VIEW2_BDRSPACE_GROUP),
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
                5,               // spacing
                &BorderSpacingSliderCData
        },
    BorderSpacingText = CONTROLDEF_TEXT(
                            "M",           // to be replaced
                            ID_CRDI_VIEW2_BDRSPACE_TEXT,
                            STYLE_SLIDERTEXT_WIDTH,
                            -1),
    WidgetSpacingGroup = CONTROLDEF_GROUP(
                            LOAD_STRING,
                            ID_CRDI_VIEW2_WGTSPACE_GROUP),
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
                5,               // spacing
                &WidgetSpacingSliderCData
        },
    WidgetSpacingText = CONTROLDEF_TEXT(
                            "M",           // to be replaced
                            ID_CRDI_VIEW2_WGTSPACE_TEXT,
                            STYLE_SLIDERTEXT_WIDTH,
                            -1),
    SizingBarsCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,
                            ID_CRDI_VIEW2_SIZINGBARS,
                            -1,
                            -1),
    SpacingLinesCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,
                            ID_CRDI_VIEW2_SPACINGLINES,
                            -1,
                            -1),
    DefWidgetStylesGroup = CONTROLDEF_GROUP(
                            LOAD_STRING,
                            ID_CRDI_VIEW2_DEFSTYLES_GROUP),

    FlatButtonsCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,
                            ID_CRDI_VIEW2_FLATBUTTONS,
                            -1,
                            -1),
    SunkBordersCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,
                            ID_CRDI_VIEW2_SUNKBORDERS,
                            -1,
                            -1),
    HatchInUseCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,
                            ID_CRDI_VIEW2_HATCHINUSE,
                            -1,
                            -1);

static const DLGHITEM dlgXCenterStyle[] =
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
                CONTROL_DEF(&G_UndoButton),         // notebook.c
                CONTROL_DEF(&G_DefaultButton),      // notebook.c
                CONTROL_DEF(&G_HelpButton),         // notebook.c
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

            // insert the controls using the dialog formatter
            // V0.9.16 (2001-10-24) [umoeller]
            ntbFormatPage(pcnbp->hwndDlgPage,
                          dlgXCenterStyle,
                          ARRAYITEMCOUNT(dlgXCenterStyle));
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

        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_CRDI_VIEW2_SPACINGLINES,
                              ((_flDisplayStyle & XCS_SPACINGLINES) != 0));
                    // added V0.9.13 (2001-06-19) [umoeller]

        // default widget styles
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_CRDI_VIEW2_FLATBUTTONS,
                            ((_flDisplayStyle & XCS_FLATBUTTONS) != 0));
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_CRDI_VIEW2_SUNKBORDERS,
                            ((_flDisplayStyle & XCS_SUNKBORDERS) != 0));
        // V0.9.16 (2001-10-24) [umoeller]
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_CRDI_VIEW2_HATCHINUSE,
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

MRESULT ctrpView2ItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                             ULONG ulItemID, USHORT usNotifyCode,
                             ULONG ulExtra)      // for checkboxes: contains new state
{
    MRESULT     mrc = 0;
    XCenterData *somThis = XCenterGetData(pcnbp->somSelf);
    BOOL        fSave = TRUE;
                // fDisplayStyleChanged = FALSE;
    LONG        lSliderIndex;
    ULONG       ulDisplayFlagChanged = 0;

    switch (ulItemID)
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

MRESULT EXPENTRY fnwpWidgetsCnr(HWND hwndCnr,
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

            return (mrc);
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
                    _xwpRemoveWidget(pcnbp->somSelf,
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

PWIDGETRECORD InsertWidgetSetting(HWND hwndCnr,
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

VOID ctrpWidgetsInitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                         ULONG flFlags)        // CBI_* flags (notebook.h)
{
    // XCenterData *somThis = XCenterGetData(pcnbp->somSelf);

    if (flFlags & CBI_INIT)
    {
        // PNLSSTRINGS     pNLSStrings = cmnQueryNLSStrings();
        HWND hwndCnr = WinWindowFromID(pcnbp->hwndDlgPage, ID_XFDI_CNR_CNR);
        XFIELDINFO      xfi[5];
        PFIELDINFO      pfi = NULL;
        int             i = 0;

        // set group cnr title
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XFDI_CNR_GROUPTITLE,
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
        WinSetWindowULong(hwndCnr, QWL_USER, (ULONG)pcnbp);
    }

    if (flFlags & CBI_SET)
    {
        HWND        hwndCnr = WinWindowFromID(pcnbp->hwndDlgPage, ID_XFDI_CNR_CNR);
        PLINKLIST   pllWidgets = ctrpQuerySettingsList(pcnbp->somSelf);
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
        if (pcnbp->pUser)
            // unload menu
            WinDestroyWindow((HWND)pcnbp->pUser);

        pcnbp->pUser = NULL;

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

MRESULT ctrpWidgetsItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                               ULONG ulItemID, USHORT usNotifyCode,
                               ULONG ulExtra)      // for checkboxes: contains new state
{
    MRESULT     mrc = 0;
    // XCenterData *somThis = XCenterGetData(pcnbp->somSelf);

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
                                if (pcdi->pDragInfo->hwndSource != pcnbp->hwndControl)
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
                                ulIndex = G_precAfter->Position.ulWidgetIndex + 1; // ulRootIndex + 1;
                            }
                            if (ulIndex != -2)
                                // OK... move the widgets around:
                                _xwpMoveWidget(pcnbp->somSelf,
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
                                PSZ         pszBuff;

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

                                    strcpy(achAll, achCnr);
                                    strcat(achAll, achSrc);

                                    ctrpReadFromFile(achAll, &pszBuff);

                                    if (pszBuff)
                                    {
                                        PSZ pszSetupString = strstr(pszBuff, "\r\n");

                                        if (pszSetupString)
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
                                                *pszSetupString = 0;
                                                pszSetupString += 2;

                                                _xwpInsertWidget(pcnbp->somSelf,
                                                                 ulIndex,
                                                                 pszBuff,
                                                                 pszSetupString);
                                            }
                                        }

                                        free(pszBuff);
                                    }
                                }
                            }

                            DrgFreeDraginfo(pcdi->pDragInfo);
                        }
                    }
                break; }

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
                    pcnbp->hwndSourceCnr = pcnbp->hwndControl;
                    pcnbp->preccSource = (PRECORDCORE)ulExtra;
                    if (pcnbp->preccSource)
                    {
                        PCXCENTERWIDGETCLASS pClass;

                        // popup menu on container recc (not whitespace):

                        if (!pcnbp->pUser)
                            // context menu not yet loaded:
                            pcnbp->pUser = (PVOID)WinLoadMenu(pcnbp->hwndControl,
                                                       cmnQueryNLSModuleHandle(FALSE),
                                                       ID_CRM_WIDGET);

                        hPopupMenu = (HWND)pcnbp->pUser;

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
                        if (pClass = ctrpFindClass(pcnbp->preccSource->pszIcon))  // class name
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
                        cnrhShowContextMenu(pcnbp->hwndControl,  // cnr
                                            (PRECORDCORE)pcnbp->preccSource,
                                            hPopupMenu,
                                            pcnbp->hwndDlgPage);    // owner
                break; } // CN_CONTEXTMENU

            } // end switch (usNotifyCode)
        break;

        /*
         * ID_CRMI_PROPERTIES:
         *      command from widget context menu.
         */

        case ID_CRMI_PROPERTIES:
        {
            PCXCENTERWIDGETCLASS pClass;
            if (    (pcnbp->preccSource)
                 && (pClass = ctrpFindClass(pcnbp->preccSource->pszIcon))  // class name
                 && (pClass->pShowSettingsDlg != 0)
               )
            {
                PWIDGETRECORD prec = (PWIDGETRECORD)pcnbp->preccSource;
                ctrpShowSettingsDlg(pcnbp->somSelf,
                                    pcnbp->hwndDlgPage, // owner
                                    &prec->Position);

                /* if (prec->ulRootIndex != -1)
                    // root widget:
                    ctrpShowSettingsDlg(pcnbp->somSelf,
                                        pcnbp->hwndDlgPage, // owner
                                        -1,
                                        0,
                                        prec->ulRootIndex);
                else
                    ctrpShowSettingsDlg(pcnbp->somSelf,
                                        pcnbp->hwndDlgPage, // owner
                                        prec->ulParentIndex,
                                        prec->ulTrayIndex,
                                        prec->ulSubwidgetIndex);
                */
            }
        break; }

        /*
         * ID_CRMI_REMOVEWGT:
         *      command from widget context menu.
         */

        case ID_CRMI_REMOVEWGT:
        {
            PWIDGETRECORD prec = (PWIDGETRECORD)pcnbp->preccSource;
            if (prec->Position.ulTrayWidgetIndex == -1) // ulRootIndex != -1)        // @@todo
                _xwpRemoveWidget(pcnbp->somSelf,
                                 prec->Position.ulWidgetIndex); // ulRootIndex);
                      // this saves the instance data
                      // and updates the view
                      // and also calls the init callback
                      // to update the settings page!
        break; }

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
    // XCenterData *somThis = XCenterGetData(pcnbp->somSelf);

    if (flFlags & CBI_INIT)
    {
        // PNLSSTRINGS     pNLSStrings = cmnQueryNLSStrings();
        HWND hwndCnr = WinWindowFromID(pcnbp->hwndDlgPage, ID_XFDI_CNR_CNR);
        XFIELDINFO      xfi[5];
        PFIELDINFO      pfi = NULL;
        int             i = 0;

        // set group cnr title
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XFDI_CNR_GROUPTITLE,
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
                        precThis->pszDLL = cmnGetString(ID_CRSI_BUILTINCLASS);

                    precThis->pszClass = (PSZ)pClass->Public.pcszWidgetClass;

                    precThis->pszClassTitle = (PSZ)pClass->Public.pcszClassTitle;

                    precThis = (PXCLASSRECORD)precThis->recc.preccNextRecord;
                    pNode = pNode->pNext;
                }
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
                               ULONG ulItemID, USHORT usNotifyCode,
                               ULONG ulExtra)      // for checkboxes: contains new state
{
    MRESULT     mrc = 0;

    return (mrc);
}

