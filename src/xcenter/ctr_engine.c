
/*
 *@@sourcefile ctr_engine.c:
 *      XCenter internal engine. This does the hard work
 *      of maintaining widget classes, loading/unloading plugins,
 *      creation/destruction of widgets, widget settings management,
 *      creating/reformatting/destroying the XCenter view itself,
 *      and so on.
 *
 *      See src\shared\center.c for an introduction to the XCenter.
 *
 *      Function prefix for this file:
 *      --  ctrp*
 *
 *      This is all new with V0.9.7.
 *
 *@@added V0.9.7 (2000-11-27) [umoeller]
 *@@header "xcenter\centerp.h"
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

#define INCL_DOSPROCESS
#define INCL_DOSMODULEMGR
#define INCL_DOSEXCEPTIONS
#define INCL_DOSSEMAPHORES
#define INCL_DOSMISC
#define INCL_DOSERRORS

#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WINMESSAGEMGR
#define INCL_WINDIALOGS
#define INCL_WININPUT
#define INCL_WINTRACKRECT
#define INCL_WINPOINTERS
#define INCL_WINSYS
#define INCL_WINTIMER
#define INCL_WINMENUS

#define INCL_GPIPRIMITIVES
#define INCL_GPILOGCOLORTABLE
#include <os2.h>

// C library headers
#include <stdio.h>
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\comctl.h"             // common controls (window procs)
#include "helpers\except.h"             // exception handling
#include "helpers\gpih.h"               // GPI helper routines
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\winh.h"               // PM helper routines
#include "helpers\stringh.h"            // string helper routines
#include "helpers\threads.h"            // thread helpers
#include "helpers\timer.h"              // replacement PM timers
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
#include <wpshadow.h>

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

static BOOL                    G_fXCenterClassRegistered = FALSE;

static COUNTRYSETTINGS         G_CountrySettings = {0};

// array of classes created by ctrpLoadClasses
static PXCENTERWIDGETCLASS     G_paWidgetClasses = NULL;
static ULONG                   G_cWidgetClasses = 0;
// reference count (raised with each ctrpLoadClasses call)
static ULONG                   G_ulWidgetClassesRefCount = 0;

// global array of plugin modules which were loaded
static LINKLIST                G_llModules;      // this contains plain HMODULEs as data
static BOOL                    G_fModulesInitialized = FALSE;

/*
 * G_aBuiltInWidgets:
 *      array of the built-in widgets in this file.
 */

XCENTERWIDGETCLASS  G_aBuiltInWidgets[]
    ={
        {
            WNDCLASS_WIDGET_OBJBUTTON,
            BTF_OBJBUTTON,
            "ObjButton",
            "Object button",
            WGTF_NOUSERCREATE | WGTF_TOOLTIP,  // WGTF_UNIQUEPERXCENTER,
            NULL        // no settings dlg
        },
        {
            WNDCLASS_WIDGET_OBJBUTTON,
            BTF_XBUTTON,
            "XButton",
            "X-Button",
            WGTF_UNIQUEPERXCENTER | WGTF_TOOLTIP,
            NULL        // no settings dlg
        },
        {
            WNDCLASS_WIDGET_PULSE,
            0,
            "Pulse",
            "CPU load",
            WGTF_SIZEABLE | WGTF_UNIQUEPERXCENTER | WGTF_TOOLTIP,
            NULL        // no settings dlg
        }
    };

VOID StartAutoHide(PXCENTERWINDATA pXCenterData);

/* ******************************************************************
 *
 *   Internal widgets management
 *
 ********************************************************************/

/*
 *@@ RegisterBuiltInWidgets:
 *      registers the build-in widget PM window classes.
 *      Gets called from ctrpCreateXCenterView on its
 *      first invocation only.
 */

BOOL RegisterBuiltInWidgets(HAB hab)
{
    return (
                (WinRegisterClass(hab,
                                  WNDCLASS_WIDGET_OBJBUTTON,
                                  fnwpObjButtonWidget,
                                  CS_PARENTCLIP | CS_CLIPCHILDREN | CS_SIZEREDRAW | CS_SYNCPAINT,
                                  sizeof(PXCENTERWINDATA))) // additional bytes to reserve
             && (WinRegisterClass(hab,
                                  WNDCLASS_WIDGET_PULSE,
                                  fnwpPulseWidget,
                                  CS_PARENTCLIP | CS_CLIPCHILDREN | CS_SIZEREDRAW | CS_SYNCPAINT,
                                  sizeof(PXCENTERWINDATA))) // additional bytes to reserve
           );
}

/*
 *@@ GetWidgetSize:
 *      asks the specified widget for its desired size.
 *      If the widget does not respond, a default size is used.
 *
 *@@added V0.9.7 (2000-12-14) [umoeller]
 */

VOID GetWidgetSize(PWIDGETVIEWSTATE pWidgetThis)
{
    if (!WinSendMsg(pWidgetThis->Widget.hwndWidget,
                    WM_CONTROL,
                    MPFROM2SHORT(ID_XCENTER_CLIENT, XN_QUERYSIZE),
                    (MPARAM)&pWidgetThis->szlWanted))
    {
        // widget didn't react to this msg:
        pWidgetThis->szlWanted.cx = 10;
        pWidgetThis->szlWanted.cy = 10;
    }
}

/*
 *@@ ReformatWidgets:
 *      goes thru all widgets and repositions them according
 *      to each widget's width.
 *
 *      If (fShow == TRUE), this also shows all affected
 *      widgets, but not the frame.
 *
 *      Preconditions:
 *
 *      -- The client window must have been positioned in
 *         the frame already.
 *
 *      -- pXCenterData->Globals.cyClient must already have
 *         the height of the tallest widget.
 *
 *      Internal function, may only run on the XCenter
 *      GUI thread.
 */

ULONG ReformatWidgets(PXCENTERWINDATA pXCenterData,
                      BOOL fShow)               // in: show widgets?
{
    ULONG       ulrc = FALSE,
                x = 0,
                y = 0,
                cxWidgetsSpace = 0,
                cxStatics = 0,
                    // total size of static widgets
                    // (those that do not want all remaining space)
                cGreedies = 0,
                    // count of widgets that want all remaining space
                cxPerGreedy = 20,
                    // width for greedy widgets (recalc'd below)
                fl = SWP_MOVE | SWP_SIZE
                        | SWP_NOADJUST;
                            // do not give the widgets a chance to mess with us!
    RECTL       rclXCenterClient;

    PLISTNODE   pNode = lstQueryFirstNode(&pXCenterData->llWidgets);

    WinQueryWindowRect(pXCenterData->Globals.hwndClient,
                       &rclXCenterClient);

    cxWidgetsSpace = rclXCenterClient.xRight - 2 * pXCenterData->ulBorderWidth;
    x = pXCenterData->ulBorderWidth;
    y = pXCenterData->ulBorderWidth;

    if (fShow)
        fl |= SWP_SHOW;

    _Pmpf((__FUNCTION__ ": entering"));

    // pass 1: get each widget's desired size,
    // calculate max cx of all static widgets
    // and count "greedy" widgets (that want all remaining space)
    while (pNode)
    {
        PWIDGETVIEWSTATE pWidgetThis = (PWIDGETVIEWSTATE)pNode->pItemData;

        if (pWidgetThis->szlWanted.cx == -1)
            // this widget wants all remaining space:
            cGreedies++;
        else
            // static widget:
            // add its size to the size of all static widgets
            cxStatics += (pWidgetThis->szlWanted.cx + pXCenterData->Globals.ulSpacing);

        pNode = pNode->pNext;
    }

    // _Pmpf(("  cGreedies %d, cxStatics: %d", cGreedies, cxStatics));
    // _Pmpf(("  rclClient.xLeft: %d, rclClient.xRight: %d", rclClient.xLeft, rclClient.xRight));

    if (cGreedies)
    {
        // we have greedy widgets:
        cxPerGreedy = (   (cxWidgetsSpace)  // client width
                        // subtract space needed for statics:
                        - (cxStatics)
                        // subtract borders between greedy widgets:
                        - ((cGreedies - 1) * pXCenterData->Globals.ulSpacing)
                      ) / cGreedies;
    }

    // _Pmpf(("  cxPerGreedy %d", cxPerGreedy));

    // pass 2: set window positions!
    pNode = lstQueryFirstNode(&pXCenterData->llWidgets);
    while (pNode)
    {
        PWIDGETVIEWSTATE pWidgetThis = (PWIDGETVIEWSTATE)pNode->pItemData;
        ULONG   cx = pWidgetThis->szlWanted.cx;
        if (cx == -1)
            // greedy widget:
            // use size we calculated above instead
            cx = cxPerGreedy;

        /* if (x == 0)
            // first loop, and we didn't start from the beginning:
            x = pWidgetThis->xCurrent;
        else */
            // pWidgetThis->xCurrent = x;

        pWidgetThis->xCurrent = x;
        pWidgetThis->cxCurrent = cx;
        pWidgetThis->cyCurrent = pXCenterData->Globals.cyTallestWidget;

        _Pmpf(("  Setting widget %d, %d, %d, %d",
                    x, 0, cx, pXCenterData->Globals.cyTallestWidget));

        WinSetWindowPos(pWidgetThis->Widget.hwndWidget,
                        NULLHANDLE,
                        x,
                        y,
                        cx,
                        pXCenterData->Globals.cyTallestWidget,
                        fl);
        WinInvalidateRect(pWidgetThis->Widget.hwndWidget, NULL, TRUE);

        x += cx + pXCenterData->Globals.ulSpacing;

        pNode = pNode->pNext;
        ulrc++;
    }

    _Pmpf((__FUNCTION__ ": leaving"));

    return (ulrc);
}

/*
 *@@ ctrpShowSettingsDlg:
 *      displays the settings dialog for an open widget view.
 *      This gets called when the respective widget context
 *      menu item is selected by the user (and its WM_COMMAND
 *      is caught by ctrDefWidgetProc).
 *
 *      This may be called on any thread.
 *
 *@@added V0.9.7 (2000-12-07) [umoeller]
 */

VOID ctrpShowSettingsDlg(PXCENTERWINDATA pXCenterData,
                         PXCENTERWIDGET pViewData)
{
    WPSHLOCKSTRUCT Lock;
    if (wpshLockObject(&Lock,
                       pXCenterData->somSelf))
    {
        PXCENTERWIDGETCLASS pClass = ctrpFindClass(pViewData->pcszWidgetClass);
        if (pClass)
        {
            if (pClass->pShowSettingsDlg)
            {
                ULONG ulIndex = ctrpQueryWidgetIndexFromHWND(pXCenterData->somSelf,
                                                             pViewData->hwndWidget);
                PLINKLIST pllWidgetSettings = ctrpQuerySettingsList(pXCenterData->somSelf);
                PLISTNODE pSettingsNode = lstNodeFromIndex(pllWidgetSettings, ulIndex);
                if (pSettingsNode)
                {
                    PXCENTERWIDGETSETTING pSetting = (PXCENTERWIDGETSETTING)pSettingsNode->pItemData;
                    WGTSETTINGSTEMP Temp
                        = {
                                pXCenterData,
                                pSetting,
                                pViewData,
                                ulIndex
                          };
                    WIDGETSETTINGSDLGDATA DlgData
                        = {
                            pXCenterData->Globals.hwndFrame,
                            pSetting->pszSetupString,       // can be 0
                            (LHANDLE)&Temp,     // ugly hack
                            &pXCenterData->Globals,
                            pViewData,
                            0           // pUser
                          };
                    // disable auto-hide while we're showing the dlg
                    pXCenterData->fShowingSettingsDlg = TRUE;
                    pClass->pShowSettingsDlg(&DlgData);
                                // ### no, this must be outside of the lock

                    pXCenterData->fShowingSettingsDlg = FALSE;
                } // end if (pSettingsNode)
            } // end if (pViewData->pShowSettingsDlg)
        } // end if (pClass);
    } // end if (wpshLockObject)
    wpshUnlockObject(&Lock);
}

/*
 *@@ ctrpDrawEmphasis:
 *      draws source emphasis on the XCenter client for
 *      hwnd, which should either be the client itself
 *      or a widget window.
 *
 *      To remove the source emphasis, you must invalidate
 *      the client. Yeah, I know, not optimal.
 *
 *      If (hpsPre == NULLHANDLE), we request a presentation
 *      space from the client and release it. If you specify
 *      hpsPre yourself (e.g. during dragging, using DrgGetPS),
 *      you must request it for the client, and it must be
 *      in RGB mode.
 *
 *      If (fRemove == TRUE), we remove emphasis instead of
 *      painting it.
 *
 *      May only run on the XCenter GUI thread.
 *
 *@@added V0.9.7 (2000-12-07) [umoeller]
 */

VOID ctrpDrawEmphasis(PXCENTERWINDATA pXCenterData,
                      HWND hwnd,
                      BOOL fRemove,
                      HPS hpsPre)        // in: presentation space; if NULLHANDLE,
                                         // we use WinGetPS in here
{
    HPS hps;

    if (hpsPre)
        // presentation space given: use that
        hps = hpsPre;
    else
    {
        // no presentation space given: get one
        hps = WinGetPS(pXCenterData->Globals.hwndClient);
        gpihSwitchToRGB(hps);
    }

    if (hps)
    {
        RECTL rcl;
        // get window's rectangle
        WinQueryWindowRect(hwnd,
                           &rcl);
        if (hwnd != pXCenterData->Globals.hwndClient)
            // widget window specified: convert to client coords
            WinMapWindowPoints(hwnd,
                               pXCenterData->Globals.hwndClient,
                               (PPOINTL)&rcl,
                               2);
        if (fRemove == FALSE)
        {
            // add emphasis:
            GpiSetColor(hps, RGBCOL_BLACK);
            GpiSetLineType(hps, LINETYPE_DOT);
        }
        else
        {
            // remove emphasis:
            GpiSetColor(hps, WinQuerySysColor(HWND_DESKTOP,
                                              SYSCLR_INACTIVEBORDER,  // same as frame!
                                              0));
        }

        if (hwnd != pXCenterData->Globals.hwndClient)
        {
            // widget window given:
            // we draw emphasis _around_ the widget window
            // on the client, so add the ulSpacing around
            // the widget's window rectangle
            rcl.xLeft -= pXCenterData->Globals.ulSpacing;
            rcl.yBottom -= pXCenterData->Globals.ulSpacing;
            rcl.xRight += pXCenterData->Globals.ulSpacing;
            rcl.yTop += pXCenterData->Globals.ulSpacing;
        }
        // else (client window given): we just use that

        // WinQueryWindowRect returns an inclusive-exclusive
        // rectangle; since GpiBox uses inclusive-inclusive
        // rectangles, fix the top right
        rcl.xRight--;
        rcl.yTop--;
        gpihDrawThickFrame(hps,
                           &rcl,
                           pXCenterData->Globals.ulSpacing);

        if (hpsPre == NULLHANDLE)
            // we acquired a PS ourselves:
            WinReleasePS(hps);
    }
}

/*
 *@@ RemoveDragoverEmphasis:
 *      shortcut to ctrpDrawEmphasis.
 *
 *      May only run on the XCenter GUI thread.
 *
 *@@added V0.9.7 (2000-12-08) [umoeller]
 */

VOID RemoveDragoverEmphasis(HWND hwndClient)
{
    PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(hwndClient, QWL_USER);
    HPS hps = DrgGetPS(pXCenterData->Globals.hwndClient);
    if (hps)
    {
        gpihSwitchToRGB(hps);
        ctrpDrawEmphasis(pXCenterData,
                         hwndClient,
                         TRUE,     // remove emphasis
                         hps);
        DrgReleasePS(hps);
    }
}

/*
 *@@ StartAutoHide:
 *
 *@@added V0.9.7 (2000-12-04) [umoeller]
 */

VOID StartAutoHide(PXCENTERWINDATA pXCenterData)
{
    XCenterData *somThis = XCenterGetData(pXCenterData->somSelf);
    if (_ulAutoHide)
    {
        pXCenterData->idTimerAutohide = tmrStartTimer(pXCenterData->Globals.hwndFrame,
                                                      TIMERID_AUTOHIDE_START,
                                                      _ulAutoHide);
    }
    else
        // auto-hide disabled:
        if (pXCenterData->idTimerAutohide)
        {
            // but timer still running (i.e. setting changed):
            tmrStopTimer(pXCenterData->Globals.hwndFrame,
                         TIMERID_AUTOHIDE_START);
            pXCenterData->idTimerAutohide = 0;
        }
}

/*
 *@@ ctrpReformat:
 *      one-shot function for refreshing the XCenter
 *      display.
 *
 *      ulFlags is used to optimize repainting. This
 *      can be any combination of XFMF_* flags or 0.
 *
 *      This does _not_
 *
 *      --  change the frame's visibility flag (WS_VISIBLE);
 *
 *      If the frame is currently auto-hidden (i.e. moved
 *      mostly off the screen), calling this function will
 *      re-show it and restart the auto-hide timer, even
 *      if ulFlags is 0.,
 *
 *      Calling this is necessary if new widgets have
 *      been added and the max client cy might have
 *      changed.
 *
 *      May only run on the XCenter GUI thread.
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
 */

VOID ctrpReformat(PXCENTERWINDATA pXCenterData,
                  ULONG ulFlags)
{
    XCenterData *somThis = XCenterGetData(pXCenterData->somSelf);

    // NOTE: We must not request the XCenter mutex here because
    // this function can get called during message sends from
    // thread 1 while thread 1 is holding the mutex. Requesting
    // the mutex here again would cause deadlocks.

    ULONG flSWP = SWP_MOVE | SWP_SIZE;

    // refresh the style bits; these might have changed
    // (mask out only the ones we're interested in changing)
    WinSetWindowBits(pXCenterData->Globals.hwndFrame,
                     QWL_STYLE,
                     _ulWindowStyle,            // flags
                     WS_TOPMOST | WS_ANIMATE);  // mask

    if (ulFlags & XFMF_DISPLAYSTYLECHANGED)
    {
        pXCenterData->Globals.ulDisplayStyle = _ulDisplayStyle;
        if (_ulDisplayStyle == XCS_BUTTON)
        {
            pXCenterData->ulBorderWidth = WinQuerySysValue(HWND_DESKTOP, SV_CYDLGFRAME);
            pXCenterData->Globals.ulSpacing = WinQuerySysValue(HWND_DESKTOP, SV_CXDLGFRAME);
        }
        else
        {
            pXCenterData->ulBorderWidth = 1;
            pXCenterData->Globals.ulSpacing = 1;
        }

        ulFlags |=    XFMF_GETWIDGETSIZES;       // reget all widget sizes

    }

    if (ulFlags & XFMF_GETWIDGETSIZES)
    {
        PLISTNODE   pNode = lstQueryFirstNode(&pXCenterData->llWidgets);

        // (re)get each widget's desired size
        while (pNode)
        {
            PWIDGETVIEWSTATE pWidgetThis = (PWIDGETVIEWSTATE)pNode->pItemData;

            // ask the widget for its desired size
            GetWidgetSize(pWidgetThis);

            pNode = pNode->pNext;
        }

        ulFlags |= XFMF_RECALCHEIGHT;
    }

    if (ulFlags & XFMF_RECALCHEIGHT)
    {
        // recalculate tallest widget
        PLISTNODE pNode = lstQueryFirstNode(&pXCenterData->llWidgets);

        pXCenterData->Globals.cyTallestWidget = 0;

        while (pNode)
        {
            PWIDGETVIEWSTATE pViewThis = (PWIDGETVIEWSTATE)pNode->pItemData;

            if (pViewThis->szlWanted.cy > pXCenterData->Globals.cyTallestWidget)
            {
                pXCenterData->Globals.cyTallestWidget = pViewThis->szlWanted.cy;
            }

            pNode = pNode->pNext;
        }

        // recalc cyFrame
        pXCenterData->cyFrame = (2 * pXCenterData->ulBorderWidth)
                                  + pXCenterData->Globals.cyTallestWidget;

        ulFlags |= XFMF_REPOSITIONWIDGETS;
    }

    if (ulFlags & (XFMF_REPOSITIONWIDGETS | XFMF_SHOWWIDGETS))
    {
        ReformatWidgets(pXCenterData,
                        // show?
                        ((ulFlags & XFMF_SHOWWIDGETS) != 0));
    }

    if (_ulPosition == XCENTER_TOP)
        pXCenterData->yFrame = winhQueryScreenCY() - pXCenterData->cyFrame;
    else
        pXCenterData->yFrame = 0;

    if (ulFlags & XFMF_RESURFACE)
        flSWP |= SWP_ZORDER;

    WinSetWindowPos(pXCenterData->Globals.hwndFrame,
                    HWND_TOP,       // only relevant if SWP_ZORDER was added above
                    0,
                    pXCenterData->yFrame,
                    pXCenterData->cxFrame,
                    pXCenterData->cyFrame,
                    flSWP);

    pXCenterData->fFrameAutoHidden = FALSE;
            // we must set this to FALSE before calling StartAutoHide
            // because otherwise StartAutoHide calls us again...

    // (re)start auto-hide timer
    StartAutoHide(pXCenterData);
}

/*
 *@@ CreateOneWidget:
 *      creates a new widget window as a child of
 *      an open XCenter frame/client. Gets called
 *      from CreateWidgets and ctrpInsertWidget.
 *
 *      Postconditions:
 *
 *      -- The position of the widget is arbitrary after
 *         this, and the widget window is NOT shown (WS_VISIBLE
 *         not set). The caller must call ReformatWidgets
 *         himself, and make the widget visible.
 *
 *      -- This raises pXCenterData->Globals.cyClient
 *         if the new widget wants a height larger
 *         than that.
 *
 *      May only run on the XCenter GUI thread.
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
 */

PWIDGETVIEWSTATE CreateOneWidget(PXCENTERWINDATA pXCenterData,
                                 PXCENTERWIDGETSETTING pSetting,
                                 ULONG ulIndex)
{
    PWIDGETVIEWSTATE pNewView = NULL;

    // NOTE: We must not request the XCenter mutex here because
    // this function can get called during message sends from
    // thread 1 while thread 1 is holding the mutex. Requesting
    // the mutex here again would cause deadlocks.

    if (!pSetting)
        cmnLog(__FILE__, __LINE__, __FUNCTION__,
               "PXCENTERWIDGETSETTING is NULL.");
    else
    {
        pNewView = (PWIDGETVIEWSTATE)malloc(sizeof(WIDGETVIEWSTATE));
                        // this includes the public XCENTERWIDGET struct

        if (pNewView)
        {
            // get ptr to XCENTERWIDGET struct
            PXCENTERWIDGET pWidget = &pNewView->Widget;
            // find the widget class for this
            PXCENTERWIDGETCLASS pWidgetClass = ctrpFindClass(pSetting->pszWidgetClass);
            memset(pNewView, 0, sizeof(*pNewView));

            if (pWidgetClass)
            {
                // set some safe defaults
                pWidget->habWidget = WinQueryAnchorBlock(pXCenterData->Globals.hwndClient);

                pWidget->pfnwpDefWidgetProc = ctrDefWidgetProc;

                pWidget->pGlobals = &pXCenterData->Globals;

                pWidget->pWidgetClass = pWidgetClass;
                pWidget->pcszWidgetClass = strdup(pWidgetClass->pcszWidgetClass);
                            // cleaned up in WM_DESTROY of ctrDefWidgetProc
                pWidget->ulClassFlags = pWidgetClass->ulClassFlags;

                pNewView->xCurrent = 0;         // changed later
                pNewView->cxCurrent = 20;
                pNewView->cyCurrent = 20;

                // setup fields
                pNewView->szlWanted.cx = 20;
                pNewView->szlWanted.cy = 20;

                pWidget->fSizeable = ((pWidgetClass->ulClassFlags & WGTF_SIZEABLE) != 0);

                pWidget->pcszSetupString = pSetting->pszSetupString;
                            // can be NULL

                _Pmpf(("  pNewView->pcszSetupString is %s",
                        (pWidget->pcszSetupString) ? pWidget->pcszSetupString : "NULL"));

                pWidget->hwndWidget = WinCreateWindow(pXCenterData->Globals.hwndClient,  // parent
                                                      (PSZ)pWidgetClass->pcszPMClass,
                                                      "",        // title
                                                      0, // WS_VISIBLE,
                                                      // x, y:
                                                      0,
                                                      0,
                                                      // cx, cy:
                                                      20,
                                                      20,
                                                      // owner:
                                                      pXCenterData->Globals.hwndClient,
                                                      HWND_TOP,
                                                      ulIndex,                // ID
                                                      pWidget, // pNewView,
                                                      0);              // presparams
                        // this sends WM_CREATE to the new widget window

                // clean up setup string
                pWidget->pcszSetupString = NULL;

                // unset widget class ptr
                pWidget->pWidgetClass = NULL;

                if (pWidget->hwndWidget)
                {
                    PSZ pszStdMenuFont = prfhQueryProfileData(HINI_USER,
                                                              "PM_Default_Colors",
                                                              "MenuTextFont",
                                                              NULL);

                    // store view data in widget's QWL_USER,
                    // in case the widget forgot; but this won't help
                    // much since the widget gets messages before WinCreateWindow
                    // returns....
                    WinSetWindowPtr(pWidget->hwndWidget, QWL_USER, pNewView);

                    // ask the widget for its size
                    GetWidgetSize(pNewView);
                    if (pNewView->szlWanted.cy > pXCenterData->Globals.cyTallestWidget)
                        pXCenterData->Globals.cyTallestWidget = pNewView->szlWanted.cy;

                    // load standard context menu
                    pWidget->hwndContextMenu = WinLoadMenu(pWidget->hwndWidget,
                                                           cmnQueryNLSModuleHandle(FALSE),
                                                           ID_CRM_WIDGET);
                    if (pszStdMenuFont)
                    {
                        // set a font presparam for this menu because
                        // otherwise it will inherit the widget's font
                        // presparam (duh)
                        winhSetWindowFont(pWidget->hwndContextMenu,
                                          pszStdMenuFont);
                        free(pszStdMenuFont);
                    }

                    // store view
                    if (    (ulIndex == -1)
                         || (ulIndex >= lstCountItems(&pXCenterData->llWidgets))
                       )
                        // append at the end:
                        lstAppendItem(&pXCenterData->llWidgets,
                                      pNewView);
                    else
                        lstInsertItemBefore(&pXCenterData->llWidgets,
                                            pNewView,
                                            ulIndex);
                } // end if (pWidget->hwndWidget)
            } // end if pWidgetClass

            if (!pWidget->hwndWidget)
            {
                // error creating window:
                PSZ     apsz[2];
                apsz[0] = (PSZ)pSetting->pszWidgetClass;
                cmnMessageBoxMsgExt(NULLHANDLE,
                                    194,        // XCenter Error
                                    apsz,
                                    1,
                                    195,
                                    MB_OK);
                free(pNewView);
                pNewView = NULL;
            }

        } // end if pNewView
    } // end if pSetting

    return (pNewView);
}

/*
 *@@ CreateWidgets:
 *      gets called from ctrpCreateXCenterView to create
 *      the widgets for that instance when the XCenter is
 *      initially opened.
 *
 *      This also resizes the frame window, if any of the widgets
 *      has indicated that it needs a larger height than the frame's
 *      client currently has. This does not change the frame's
 *      visibility though.
 *
 *      May only run on the XCenter GUI thread.
 *
 *      Preconditions:
 *
 *      -- The client window must have been positioned in
 *         the frame already.
 *
 *      -- The caller must have requested the XCenter mutex.
 */

ULONG CreateWidgets(PXCENTERWINDATA pXCenterData)
{
    ULONG   ulrc = 0;

    // NOTE: We must not request the XCenter mutex here because
    // this function can get called during message sends from
    // thread 1 while thread 1 is holding the mutex. Requesting
    // the mutex here again would cause deadlocks.

    ULONG   ul = 0;

    PLINKLIST pllWidgetSettings = ctrpQuerySettingsList(pXCenterData->somSelf);

    PLISTNODE pNode = lstQueryFirstNode(pllWidgetSettings);

    pXCenterData->Globals.cyTallestWidget = 0;     // raised by CreateOneWidget

    while (pNode)
    {
        PXCENTERWIDGETSETTING pSetting = (PXCENTERWIDGETSETTING)pNode->pItemData;
        if (CreateOneWidget(pXCenterData,
                            pSetting,
                            -1))        // at the end
            ulrc++;

        pNode = pNode->pNext;
    } // end for widgets

    // this is the first "reformat frame" when the XCenter
    // is created, so get max cy and reposition
    ctrpReformat(pXCenterData,
                 XFMF_DISPLAYSTYLECHANGED); // XFMF_RECALCHEIGHT | XFMF_REPOSITIONWIDGETS);

    return (ulrc);
}

/*
 *@@ DestroyWidgets:
 *      reversely to CreateWidgets, this cleans up.
 *
 *      May only run on the XCenter GUI thread.
 *
 *      This does not affect the XCENTERWIDGETSETTINGS.
 */

VOID DestroyWidgets(PXCENTERWINDATA pXCenterData)
{
    PLISTNODE pNode = lstQueryFirstNode(&pXCenterData->llWidgets);
    while (pNode)
    {
        PWIDGETVIEWSTATE pView = (PWIDGETVIEWSTATE)pNode->pItemData;

        WinDestroyWindow(pView->Widget.hwndWidget);
                // the window is responsible for cleaning up pView->pUser;
                // ctrDefWidgetProc will also free pView and remove it
                // from the widget views list

        pNode = pNode->pNext;
    }

    lstClear(&pXCenterData->llWidgets);
        // the list is in auto-free mode
}

/*
 *@@ SetWidgetSize:
 *
 *      May only run on the XCenter GUI thread.
 */

BOOL SetWidgetSize(PXCENTERWINDATA pXCenterData,
                   HWND hwndWidget,
                   ULONG ulNewWidth)
{
    BOOL brc = FALSE;

    PLISTNODE pNode = lstQueryFirstNode(&pXCenterData->llWidgets);
    while (pNode)
    {
        PWIDGETVIEWSTATE pWidgetThis = (PWIDGETVIEWSTATE)pNode->pItemData;
        if (pWidgetThis->Widget.hwndWidget == hwndWidget)
        {
            // found it:
            // set new width
            pWidgetThis->szlWanted.cx = ulNewWidth;

            ctrpReformat(pXCenterData,
                         XFMF_RECALCHEIGHT | XFMF_REPOSITIONWIDGETS);
            brc = TRUE;
            break;
        }

        pNode = pNode->pNext;
    }

    return (brc);
}

/* ******************************************************************
 *
 *   XCenter frame window proc
 *
 ********************************************************************/

/*
 *@@ FrameCommand:
 *      implementation for WM_COMMAND in fnwpXCenterMainFrame.
 *
 *      Returns TRUE if the msg was processed. On FALSE,
 *      the parent winproc should be called.
 */

BOOL FrameCommand(HWND hwnd, USHORT usCmd)
{
    BOOL                fProcessed = TRUE;
    PXCENTERWINDATA     pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER);
    PCGLOBALSETTINGS    pGlobalSettings = cmnQueryGlobalSettings();
    XCenterData         *somThis = XCenterGetData(pXCenterData->somSelf);

    switch (usCmd)
    {
        case WPMENUID_CLOSE:
            // our custom "close" menu item
            WinPostMsg(hwnd, WM_SYSCOMMAND, (MPARAM)SC_CLOSE, 0);
        break;

        default:
            // is it one of the items in the "new widget" submenu?
            if (    (usCmd >= pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_VARIABLE)
                 && (usCmd < (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_VARIABLE
                                + G_cWidgetClasses)
                    )
               )
            {
                // yes: get the corresponding item
                USHORT usIndex = usCmd - (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_VARIABLE);
                if (G_paWidgetClasses)
                {
                    _xwpInsertWidget(pXCenterData->somSelf,
                                     -1,        // at the end
                                     (PSZ)G_paWidgetClasses[usIndex].pcszWidgetClass,
                                     NULL);     // no setup string yet
                }
            }
            else
                // some other command (probably WPS menu items):
                fProcessed = FALSE;
        break;
    }

    return (fProcessed);
}

/*
 *@@ FrameTimer:
 *      implementation for WM_TIMER in fnwpXCenterMainFrame.
 *
 *      Returns TRUE if the msg was processed. On FALSE,
 *      the parent winproc should be called.
 *
 *@@changed V0.9.7 (2000-12-08) [umoeller]: got rid of dtGetULongTime
 */

BOOL FrameTimer(HWND hwnd,
                PXCENTERWINDATA pXCenterData,
                USHORT usTimerID)
{
    BOOL brc = TRUE;            // processed

    switch (usTimerID)
    {
        /*
         * TIMERID_UNFOLDFRAME:
         *      animation timer for unfolding the frame
         *      when the XCenter is first opened.
         */

        case TIMERID_UNFOLDFRAME:
        {
            ULONG cxCurrent = 0;
            if (pXCenterData->ulStartTime == 0)
            {
                // first call:
                DosQuerySysInfo(QSV_MS_COUNT, QSV_MS_COUNT,
                                &pXCenterData->ulStartTime,
                                sizeof(pXCenterData->ulStartTime));
                cxCurrent = 2 * pXCenterData->ulBorderWidth;
                    // WinQuerySysValue(HWND_DESKTOP, SV_CYDLGFRAME);
            }
            else
            {
                ULONG ulNowTime;
                ULONG ulMSPassed;
                DosQuerySysInfo(QSV_MS_COUNT, QSV_MS_COUNT,
                                &ulNowTime,
                                sizeof(ulNowTime));
                ulMSPassed = ulNowTime - pXCenterData->ulStartTime;

                // total animation should last two seconds,
                // so check where we are now compared to
                // the time the animation started
                cxCurrent = (pXCenterData->cxFrame) * ulMSPassed / MAX_UNFOLDFRAME;

                if (cxCurrent >= pXCenterData->cxFrame)
                {
                    // last step:
                    cxCurrent = pXCenterData->cxFrame;
                    // stop this timer
                    tmrStopTimer(hwnd,
                                 usTimerID);
                    // start second timer for showing the widgets
                    tmrStartTimer(hwnd,
                                  TIMERID_SHOWWIDGETS,
                                  100);
                    pXCenterData->ulStartTime = 0;
                }
            }

            WinSetWindowPos(hwnd,
                            HWND_TOP,
                            0,
                            pXCenterData->yFrame,
                            cxCurrent,
                            pXCenterData->cyFrame,
                            SWP_MOVE | SWP_SIZE | SWP_SHOW | SWP_ZORDER);
        break; } // case TIMERID_UNFOLDFRAME

        /*
         * TIMERID_SHOWWIDGETS:
         *      animation timer for successively displaying
         *      the widgets after the frame has unfolded.
         */

        case TIMERID_SHOWWIDGETS:
        {
            PWIDGETVIEWSTATE pWidgetThis
                = (PWIDGETVIEWSTATE)lstItemFromIndex(&pXCenterData->llWidgets,
                                                     (pXCenterData->ulWidgetsShown)++);
            if (pWidgetThis)
                WinShowWindow(pWidgetThis->Widget.hwndWidget, TRUE);
            else
            {
                XCenterData *somThis = XCenterGetData(pXCenterData->somSelf);
                // no more widgets:
                tmrStopTimer(hwnd,
                             usTimerID);
                pXCenterData->ulWidgetsShown = 0;

                StartAutoHide(pXCenterData);
            }
        break; }

        /*
         * TIMERID_AUTOHIDE_START:
         *      delay timer if _ulAutoHide > 0, which is
         *      a one-shot timer (mostly) to delay hiding
         *      the XCenter frame. Started exclusively by
         *      StartAutoHide().
         */

        case TIMERID_AUTOHIDE_START:
        {
            BOOL fStart = TRUE;
            HWND hwndFocus = WinQueryFocus(HWND_DESKTOP);

            // this is only for the delay
            tmrStopTimer(hwnd,
                         usTimerID);
            pXCenterData->idTimerAutohide = 0;

            // disable auto-hide if a settings dialog is currently open
            if (pXCenterData->fShowingSettingsDlg)
                fStart = FALSE;
            // disable auto-hide if a menu is currently open
            else if (hwndFocus)
            {
                CHAR szWinClass[100];
                if (WinQueryClassName(hwndFocus, sizeof(szWinClass), szWinClass))
                {
                    _Pmpf((__FUNCTION__ ": win class %s", szWinClass));
                    if (strcmp(szWinClass, "#4") == 0)
                    {
                        // it's a menu:
                        /*  if (WinQueryWindow(hwndFocus, QW_OWNER)
                                == pXCenterData->Globals.hwndClient) */
                            fStart = FALSE;
                    }
                }
            }

            if (fStart)
            {
                // second check: if mouse is still over XCenter,
                // forget it
                POINTL ptlMouseDtp;
                WinQueryPointerPos(HWND_DESKTOP, &ptlMouseDtp);
                if (WinWindowFromPoint(HWND_DESKTOP,
                                       &ptlMouseDtp,
                                       FALSE)
                        == pXCenterData->Globals.hwndFrame)
                    fStart = FALSE;
            }

            if (fStart)
                tmrStartTimer(hwnd,
                              TIMERID_AUTOHIDE_RUN,
                              50);
            else
                StartAutoHide(pXCenterData);
        break; }

        /*
         * TIMERID_AUTOHIDE_RUN:
         *      animation timer which actually hides the
         *      frame once TIMERID_AUTOHIDE_START has
         *      elapsed.
         */

        case TIMERID_AUTOHIDE_RUN:
        {
            LONG cySubtractCurrent;
                        // cySubtractCurrent receives the currently visible
                        // width; this starts out with the full frame width

            if (pXCenterData->ulStartTime == 0)
            {
                // first call:
                DosQuerySysInfo(QSV_MS_COUNT, QSV_MS_COUNT,
                                &pXCenterData->ulStartTime,
                                sizeof(pXCenterData->ulStartTime));
                // initially, subtract nothing
                cySubtractCurrent = 0;
            }
            else
            {
                XCenterData *somThis = XCenterGetData(pXCenterData->somSelf);
                ULONG ulNowTime;
                ULONG cySubtractMax;
                // ULONG ulMSPassed;
                DosQuerySysInfo(QSV_MS_COUNT, QSV_MS_COUNT,
                                &ulNowTime,
                                sizeof(ulNowTime));

                // cySubtractMax has the width that should be
                // subtracted from the full frame width to
                // get only a single dlg frame
                cySubtractMax = pXCenterData->cyFrame - pXCenterData->ulBorderWidth;

                // total animation should last two seconds,
                // so check where we are now compared to
                // the time the animation started
                cySubtractCurrent = cySubtractMax
                                    * (ulNowTime - pXCenterData->ulStartTime)
                                    / MAX_AUTOHIDE;

                if (cySubtractCurrent >= cySubtractMax)
                {
                    // last step:
                    cySubtractCurrent = cySubtractMax;
                    // stop this timer
                    tmrStopTimer(hwnd,
                                 usTimerID);
                    pXCenterData->ulStartTime = 0;

                    // mark frame as "hidden"
                    pXCenterData->fFrameAutoHidden = TRUE;

                    if (_ulWindowStyle & WS_TOPMOST)
                        if (WinIsWindowVisible(pXCenterData->Globals.hwndFrame))
                            // repaint the XCenter, because if we're topmost,
                            // we sometimes have display errors that will never
                            // go away
                            WinInvalidateRect(pXCenterData->Globals.hwndClient, NULL, FALSE);
                }

                if (_ulPosition == XCENTER_BOTTOM)
                {
                    // XCenter sits at bottom:
                    // move downwards
                    pXCenterData->yFrame = -cySubtractCurrent;
                }
                else
                {
                    // XCenter sits at top:
                    // move upwards
                    pXCenterData->yFrame =   winhQueryScreenCY()
                                           - pXCenterData->cyFrame
                                           + cySubtractCurrent;
                }
            }

            WinSetWindowPos(hwnd,
                            HWND_TOP,
                            0,
                            pXCenterData->yFrame,
                            pXCenterData->cxFrame,
                            pXCenterData->cyFrame,
                            SWP_MOVE | SWP_SIZE | SWP_SHOW /* | SWP_ZORDER */ );
        break; }

        default:
            // other timer:
            // call parent winproc
            brc = FALSE;
    }

    return (brc);
}

/*
 *@@ fnwpXCenterMainFrame:
 *      window proc for the XCenter frame.
 *
 *      See fnwpXCenterMainClient for an overview.
 *
 *      This window proc is used for subclassing the
 *      regular PM WC_FRAME which is created for the
 *      XCenter view in ctrpCreateXCenterView. The
 *      original WC_FRAME window proc is stored in
 *      the XCenter view's XCENTERWINDATA, which
 *      in turn resides in the frame's QWL_USER.
 */

MRESULT EXPENTRY fnwpXCenterMainFrame(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT             mrc = 0;
    BOOL                fCallDefault = FALSE;

    TRY_LOUD(excpt1)
    {
        switch (msg)
        {
            /*
             * WM_SYSCOMMAND:
             *      we must intercept this; since we don't have
             *      a system menu, closing from the task list
             *      won't work otherwise.
             *
             *      NOTE: Apparently, since we're running on a
             *      separate thread, the tasklist posts WM_QUIT
             *      directly into the queue. Anyway, this msg
             *      does NOT come in for the tasklist close.
             */

            case WM_SYSCOMMAND:
                switch ((USHORT)mp1)
                {
                    case SC_CLOSE:
                        WinDestroyWindow(hwnd);
                    break;
                }
            break;

            /* case WM_FOCUSCHANGE:
                // do NOTHING!
            break;

            case WM_QUERYFOCUSCHAIN:
                // do NOTHING!
            break;

            case WM_QUERYFRAMEINFO:
                // DosBeep(2000, 30);
                // this msg never comes in... sigh
                mrc = (MRESULT)(FI_FRAME | FI_NOMOVEWITHOWNER);
                        // but not FI_ACTIVATEOK
            break; */

            /*
             * WM_ADJUSTWINDOWPOS:
             *      we never want to become active.
             */

            case WM_ADJUSTWINDOWPOS:
            {
                PSWP pswp = (PSWP)mp1;
                if (pswp->fl & SWP_ACTIVATE)
                    pswp->fl &= ~SWP_ACTIVATE;
                mrc = (MRESULT)AWP_DEACTIVATE;
            break; }

            case WM_ADJUSTFRAMEPOS:
                // do NOTHING!
            break;

            /*
             * WM_ACTIVATE:
             *      we must swallow this because we _never_
             *      want to become active, no matter what.
             *      If we don't do this, the frame will
             *      be invalidated with WPS context menus,
             *      for example.
             */

            case WM_ACTIVATE:
            break;

            /*
             * WM_COMMAND:
             *      menu command posted to the client's owner
             *      == the frame (yes, that's us here).
             *
             *      Note that since we subclassed the frame,
             *      we get all menu commands before the WPS
             *      -- even from the WPS menu we displayed
             *      using wpDisplayMenu from the client.
             */

            case WM_COMMAND:
                if (!FrameCommand(hwnd, (USHORT)mp1))
                    // not processed:
                    fCallDefault = TRUE;
            break;

            /*
             * WM_MOUSEMOVE:
             *      if mouse moves over the frame, re-show it
             *      if it's hidden. This is what makes the
             *      frame reappear if the mouse moves over the
             *      tiny line at the screen edge if the XCenter
             *      is currently auto-hidden.
             */

            case WM_MOUSEMOVE:
                ctrpReformat((PXCENTERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER),
                             0);        // just show
                fCallDefault = TRUE;
            break;

            /*
             * WM_TIMER:
             *      handle the various timers related to animations
             *      and such.
             */

            case WM_TIMER:
            {
                if (!FrameTimer(hwnd,
                                (PXCENTERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER),
                                (USHORT)mp1))
                    // not processed:
                    fCallDefault = TRUE;
            break; }

            /*
             * WM_DESTROY:
             *      stop all running timers.
             *
             *      The client should get this before the frame.
             */

            case WM_DESTROY:
                tmrStopAllTimers(hwnd);
                fCallDefault = TRUE;
                // stop the XCenter thread we're running on
                WinPostMsg(NULLHANDLE,      // post into the queue
                           WM_QUIT,
                           0,
                           0);
            break;

            default:
                fCallDefault = TRUE;
        }
    }
    CATCH(excpt1) {} END_CATCH();

    // do the call to the original WC_FRAME window proc outside
    // the exception handler
    if (fCallDefault)
    {
        PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER);
        mrc = pXCenterData->pfnwpFrameOrig(hwnd, msg, mp1, mp2);
    }

    return (mrc);
}

/* ******************************************************************
 *
 *   XCenter client window proc
 *
 ********************************************************************/

/*
 *@@ FindWidgetFromClientXY:
 *      returns TRUE if sx and sy specify client window
 *      coordinates which are on the right border of a
 *      widget. If that widget is sizeable, *pfIsSizable
 *      is set to TRUE, FALSE otherwise.
 *
 *      Returns FALSE if not found.
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
 */

BOOL FindWidgetFromClientXY(PXCENTERWINDATA pXCenterData,
                            SHORT sx,
                            SHORT sy,
                            PBOOL pfIsSizeable,              // out: TRUE if widget sizing border
                            PWIDGETVIEWSTATE *ppViewOver,    // out: widget mouse is over
                            PULONG pulIndexOver)             // out: that widget's index
{
    BOOL brc = FALSE;
    ULONG ul = 0;
    PLISTNODE pNode = lstQueryFirstNode(&pXCenterData->llWidgets);
    while (pNode)
    {
        PWIDGETVIEWSTATE pView = (PWIDGETVIEWSTATE)pNode->pItemData;
        ULONG   ulRightEdge = pView->xCurrent + pView->cxCurrent;
        if (sx >= ulRightEdge)
        {
            if (sx <= ulRightEdge + pXCenterData->Globals.ulSpacing)
            {
                // we're over the spacing border to the right of this view:
                brc = TRUE;
                if (pfIsSizeable)
                    *pfIsSizeable = pView->Widget.fSizeable;
                if (ppViewOver)
                    *ppViewOver = pView;
                if (pulIndexOver)
                    *pulIndexOver = ul;
                break;
            }
        }
        else
            break;

        pNode = pNode->pNext;
        ul++;
    }

    return (brc);
}

/*
 *@@ GetDragoverObjects:
 *      returns a linked list of plain WPObject* pointers
 *      for the objects being dragged over the XCenter.
 *
 *      The list is not in auto-free mode and must be freed
 *      by the caller.
 *
 *@@added V0.9.7 (2000-12-13) [umoeller]
 */

PLINKLIST GetDragoverObjects(PDRAGINFO pdrgInfo,
                             PUSHORT pusIndicator,  // out: indicator for DM_DRAGOVER (ptr can be NULL)
                             PUSHORT pusOp)         // out: def't op for DM_DRAGOVER (ptr can be NULL)
{
    PLINKLIST pllObjects = NULL;
    // assert default drop operation (no
    // modifier keys are pressed)
    if (    (    (pdrgInfo->usOperation == DO_DEFAULT)
              || (pdrgInfo->usOperation == DO_LINK)
            )
         && (pdrgInfo->cditem)
       )
    {
        // go thru dragitems
        ULONG ul = 0;
        if (pusIndicator)
            *pusIndicator = DOR_DROP;
        if (pusOp)
            *pusOp = DO_LINK;
        for (;
             ul < pdrgInfo->cditem;
             ul++)
        {
            // access that drag item
            PDRAGITEM pdrgItem = DrgQueryDragitemPtr(pdrgInfo, 0);
            WPObject *pobjDragged;
            if (!wpshQueryDraggedObject(pdrgItem, &pobjDragged))
            {
                // invalid object:
                if (pusIndicator)
                    *pusIndicator = DOR_NEVERDROP;
                if (pusOp)
                    *pusOp = DO_UNKNOWN;
                // and stop
                break; // for
            }
            else
            {
                // valid object:
                WPObject *pReal = pobjDragged;

                if (!pllObjects)
                    // list not created yet:
                    pllObjects = lstCreate(FALSE);

                // dereference shadows
                while ((pReal) && (_somIsA(pReal, _WPShadow)))
                    pReal = _wpQueryShadowedObject(pReal,
                                                   TRUE); // lock

                if (pReal)
                    lstAppendItem(pllObjects, pReal);
            }
        }
    }
    else
        // non-default drop op:
        if (pusIndicator)
            *pusIndicator = DOR_NODROP;
        // but do send DM_DRAGOVER again

    return (pllObjects);
}

/*
 *@@ ClientPaint:
 *      implementation for WM_PAINT in fnwpXCenterMainClient.
 */

VOID ClientPaint(HWND hwnd)
{
    RECTL rclPaint;
    HPS hps = WinBeginPaint(hwnd, NULLHANDLE, &rclPaint);
    if (hps)
    {
        PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER);
        RECTL rclWin;

        // switch to RGB
        gpihSwitchToRGB(hps);

        WinQueryWindowRect(hwnd,
                           &rclWin);        // exclusive
        rclWin.xRight--;
        rclWin.yTop--;
        gpihDraw3DFrame(hps,
                        &rclWin,            // inclusive
                        1,
                        pXCenterData->Globals.lcol3DLight,
                        pXCenterData->Globals.lcol3DDark);

        rclWin.xLeft++;
        rclWin.yBottom++;
        WinFillRect(hps,
                    &rclWin,
                    WinQuerySysColor(HWND_DESKTOP,
                                     SYSCLR_INACTIVEBORDER,  // same as frame!
                                     0));
        WinEndPaint(hps);
    }
}

/*
 *@@ ClientMouseMove:
 *      implementation for WM_MOUSEMOVE in fnwpXCenterMainClient.
 */

MRESULT ClientMouseMove(HWND hwnd, MPARAM mp1)
{
    PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER);
    SHORT           sxMouse = SHORT1FROMMP(mp1),  // win coords!
                    syMouse = SHORT2FROMMP(mp1);  // win coords!

    ULONG           idPtr = SPTR_ARROW;
    BOOL            fSizeable = FALSE;

    if (FindWidgetFromClientXY(pXCenterData,
                               sxMouse,
                               syMouse,
                               &fSizeable,
                               NULL,
                               NULL))
        if (fSizeable)
            idPtr = SPTR_SIZEWE;

    WinSetPointer(HWND_DESKTOP,
                  WinQuerySysPointer(HWND_DESKTOP,
                                     idPtr,
                                     FALSE));   // no copy

    /* ctrpReformat(pXCenterData,
                 0);        // just show
                                        */
    return ((MPARAM)TRUE);      // message processed
}

/*
 *@@ ClientButton1Down:
 *      implementation for WM_BUTTON1DOWN in fnwpXCenterMainClient.
 */

MRESULT ClientButton1Down(HWND hwnd, MPARAM mp1)
{
    PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER);
    SHORT           sx = SHORT1FROMMP(mp1);
    SHORT           sy = SHORT2FROMMP(mp1);
    PWIDGETVIEWSTATE pViewOver = NULL;
    BOOL            fSizeable = FALSE;

    if (    (FindWidgetFromClientXY(pXCenterData,
                                    sx,
                                    sy,
                                    &fSizeable,
                                    &pViewOver,
                                    NULL))
         && (fSizeable)
       )
    {
        if (pViewOver)
        {
            TRACKINFO ti;
            SWP swpWidget;
            WinQueryWindowPos(pViewOver->Widget.hwndWidget,
                              &swpWidget);
            memset(&ti, 0, sizeof(ti));
            ti.cxBorder = pXCenterData->Globals.ulSpacing;
            ti.cyBorder = pXCenterData->Globals.ulSpacing;
            ti.cxGrid = 0;
            ti.cyGrid = 0;
            ti.rclTrack.xLeft = swpWidget.x;
            ti.rclTrack.xRight = swpWidget.x + swpWidget.cx;
            ti.rclTrack.yBottom = swpWidget.y;
            ti.rclTrack.yTop = swpWidget.y + swpWidget.cy;
            WinQueryWindowRect(hwnd,
                               &ti.rclBoundary);
            ti.ptlMinTrackSize.x = 30; // pViewOver->cxMinimum;
            ti.ptlMaxTrackSize.x = ti.rclBoundary.xRight;
            ti.ptlMaxTrackSize.y = ti.rclBoundary.yTop;
            ti.fs = TF_ALLINBOUNDARY
                    | TF_RIGHT
                    | TF_SETPOINTERPOS;

            if (WinTrackRect(hwnd,
                             NULLHANDLE,    //hps?
                             &ti))
            {
                // tracking not cancelled:
                // set new width
                SetWidgetSize(pXCenterData,
                              pViewOver->Widget.hwndWidget,
                              ti.rclTrack.xRight - ti.rclTrack.xLeft);
            }
        }
    }

    return ((MPARAM)TRUE);      // message processed
}

/*
 *@@ ClientContextMenu:
 *      implementation for WM_CONTEXTMENU in fnwpXCenterMainClient.
 */

MRESULT ClientContextMenu(HWND hwnd, MPARAM mp1)
{
    PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER);
    XCenterData     *somThis = XCenterGetData(pXCenterData->somSelf);
    POINTL          ptlPopup;
    HPS             hps;

    ptlPopup.x = SHORT1FROMMP(mp1);
    ptlPopup.y = SHORT2FROMMP(mp1);
    // set flag for wpModifyPopupMenu so that it will
    // add the "close" menu item; there's no other way
    // to get this to work since we don't have a container
    // as our client... sigh
    _fShowingOpenViewMenu = TRUE;
            // this will be unset in wpModifyPopupMenu

    // draw source emphasis
    ctrpDrawEmphasis(pXCenterData,
                     hwnd,      // the client
                     FALSE,     // draw, not remove emphasis
                     NULLHANDLE);

    pXCenterData->hwndContextMenu
            = _wpDisplayMenu(pXCenterData->somSelf,
                             // owner: the frame
                             pXCenterData->Globals.hwndFrame,
                             // client: our client
                             pXCenterData->Globals.hwndClient,
                             &ptlPopup,
                             MENU_OPENVIEWPOPUP, //  | MENU_NODISPLAY,
                             0);

    return ((MPARAM)TRUE);      // message processed
}

/*
 *@@ ClientMenuEnd:
 *      implementation for WM_MENUEND in fnwpXCenterMainClient.
 */

VOID ClientMenuEnd(HWND hwnd, MPARAM mp2)
{
    PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER);

    if (   (pXCenterData->hwndContextMenu)
        && (((HWND)mp2) == pXCenterData->hwndContextMenu)
       )
    {
        // context menu closing:
        // remove emphasis
        WinInvalidateRect(pXCenterData->Globals.hwndClient, NULL, FALSE);
        pXCenterData->hwndContextMenu = NULLHANDLE;
    }
}

/*
 *@@ ClientDragOver:
 *      implementation for DM_DRAGOVER in fnwpXCenterMainClient.
 */

MRESULT ClientDragOver(HWND hwnd, MPARAM mp1)
{
    PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER);
    PDRAGINFO       pdrgInfo = (PDRAGINFO)mp1;
    // default return values
    USHORT          usIndicator = DOR_NEVERDROP,
                        // cannot be dropped, and don't send
                        // DM_DRAGOVER again
                    usOp = DO_UNKNOWN;
                        // target-defined drop operation:
                        // user operation (we don't want
                        // the WPS to copy anything)

    HPS hps = DrgGetPS(pXCenterData->Globals.hwndFrame);
    if (hps)
    {
        gpihSwitchToRGB(hps);
        ctrpDrawEmphasis(pXCenterData,
                         hwnd,
                         FALSE,     // draw, not remove emphasis
                         hps);
    }

    // go!
    if (DrgAccessDraginfo(pdrgInfo))
    {
        PLINKLIST pll = GetDragoverObjects(pdrgInfo,
                                           &usIndicator,
                                           &usOp);

        // clean up
        lstFree(pll);
        DrgFreeDraginfo(pdrgInfo);
    }

    if (hps)
        DrgReleasePS(hps);

    // and return the drop flags
    return (MRFROM2SHORT(usIndicator, usOp));
}

/*
 *@@ ClientDrop:
 *      implementation for DM_DROP in fnwpXCenterMainClient.
 */

VOID ClientDrop(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER);
    PDRAGINFO       pdrgInfo = (PDRAGINFO)mp1;
    if (DrgAccessDraginfo(pdrgInfo))
    {
        PLINKLIST pll = GetDragoverObjects(pdrgInfo,
                                           NULL,
                                           NULL);
        PLISTNODE pNode = lstQueryFirstNode(pll);
        POINTL ptlDrop;
        PWIDGETVIEWSTATE pViewOver = NULL;
        ULONG ulIndex = 0;

        // convert coordinates to client
        ptlDrop.x = pdrgInfo->xDrop;
        ptlDrop.y = pdrgInfo->yDrop;     // dtp coords
        WinMapWindowPoints(HWND_DESKTOP,
                           hwnd,            // to client
                           &ptlDrop,
                           1);

        FindWidgetFromClientXY(pXCenterData,
                               ptlDrop.x,
                               ptlDrop.y,
                               NULL,
                               &pViewOver,
                               &ulIndex);
        if (!pViewOver)
            ulIndex = 0;        // leftmost
        else
            // FindWidgetFromClientXY has returned the
            // widget whose _right_ border matched the
            // coordinates... however, we must specify
            // the index _before_ which we want to insert
            ulIndex++;

        while (pNode)
        {
            WPObject *pObjDragged = (WPObject*)pNode->pItemData;
            if (pObjDragged)
            {
                HOBJECT hobjDragged = _wpQueryHandle(pObjDragged);
                if (hobjDragged)
                {
                    CHAR szSetup[100];
                    sprintf(szSetup, "OBJECTHANDLE=%lX;", hobjDragged);
                    // insert object button widgets for the objects
                    // being dropped; this sets up the object further
                    _xwpInsertWidget(pXCenterData->somSelf,
                                     ulIndex,
                                     "ObjButton",    // widget class
                                     szSetup);
                }
            }
            // next object
            pNode = pNode->pNext;
        }

        lstFree(pll);
        DrgFreeDraginfo(pdrgInfo);
    }
    WinInvalidateRect(pXCenterData->Globals.hwndFrame, NULL, FALSE);
}

/*
 *@@ ClientControl:
 *      implementation for WM_CONTROL.
 *
 *@@added V0.9.7 (2001-01-03) [umoeller]
 */

MRESULT ClientControl(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    USHORT  usID = SHORT1FROMMP(mp1),
            usNotifyCode = SHORT2FROMMP(mp1);

    _Pmpf((__FUNCTION__ ": id 0x%lX, notify 0x%lX", usID, usNotifyCode));

    if (    (usID == ID_XCENTER_TOOLTIP)
         && (usNotifyCode == TTN_NEEDTEXT)
       )
    {
        PTOOLTIPTEXT pttt = (PTOOLTIPTEXT)mp2;
        // forward tooltip "need text" to widget
        mrc = WinSendMsg(pttt->hwndTool,      // widget window (tool)
                         WM_CONTROL,
                         mp1,
                         mp2);
    }

    return (mrc);
}

/*
 *@@ ClientSaveSetup:
 *      implementation for XCM_SAVESETUP to save a widget's
 *      new setup string.
 *
 *      This calls _wpSaveDeferred in turn.
 *
 *@@added V0.9.7 (2000-12-04) [umoeller]
 */

BOOL ClientSaveSetup(HWND hwndClient,
                     HWND hwndWidget,
                     const char *pcszSetupString)    // can be NULL
{
    PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(hwndClient, QWL_USER);
    BOOL brc = FALSE;

    ULONG ulIndex = ctrpQueryWidgetIndexFromHWND(pXCenterData->somSelf,
                                                 hwndWidget);
    if (ulIndex != -1)
    {
        PLINKLIST pllWidgetSettings = ctrpQuerySettingsList(pXCenterData->somSelf);
        PLISTNODE pSettingsNode = lstNodeFromIndex(pllWidgetSettings, ulIndex);
        if (pSettingsNode)
        {
            // got the settings:
            PXCENTERWIDGETSETTING pSetting = (PXCENTERWIDGETSETTING)pSettingsNode->pItemData;
            if (pSetting->pszSetupString)
            {
                // we already had a setup string:
                free(pSetting->pszSetupString);
                pSetting->pszSetupString = NULL;
            }

            if (pcszSetupString)
                pSetting->pszSetupString = strdup(pcszSetupString);

            brc = TRUE;

            _wpSaveDeferred(pXCenterData->somSelf);
        }
    }

    return (brc);
}

/*
 *@@ ClientDestroy:
 *      implementation for WM_DESTROY in fnwpXCenterMainClient.
 */

VOID ClientDestroy(HWND hwnd)
{
    PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER);
    XCenterData     *somThis = XCenterGetData(pXCenterData->somSelf);

    _pvOpenView = NULL;

    DestroyWidgets(pXCenterData);

    if (pXCenterData->hwndTooltip)
        WinDestroyWindow(pXCenterData->hwndTooltip);

    // remove this window from the object's use list
    _wpDeleteFromObjUseList(pXCenterData->somSelf,
                            &pXCenterData->UseItem);

    _wpFreeMem(pXCenterData->somSelf,
               (PBYTE)pXCenterData);

    ctrpFreeClasses();
}

/*
 *@@ fnwpXCenterMainClient:
 *      window proc for the client window of the XCenter bar.
 *
 *      The XCenter has the following window hierarchy (where
 *      lines signify both parent- and ownership):
 *
 +      WC_FRAME (fnwpXCenterMainFrame)
 +         |
 +         +----- FID_CLIENT (fnwpXCenterMainClient)
 +                    |
 +                    +---- widget 1
 +                    +---- widget 2
 +                    +---- ...
 *
 *      Widgets may of course create subchildren, of which
 *      the XCenter need not know.
 *
 *      The frame is created without any border. As a result,
 *      the client has the exact same size as the frame. It
 *      is actually the client which draws the 3D frame for
 *      the XCenter frame, because the frame is completely
 *      covered by it.
 *
 *      This may not sound effective, but the problem is that
 *      _wpRegisterView works only with frames, from my testing.
 *      So we can't just use our own window class in the
 *      first place. Besides, this mechanism gives the XWPHook
 *      a quick way to check if the window under the mouse is
 *      an XCenter, simply by testing FID_CLIENT for the window
 *      class.
 */

MRESULT EXPENTRY fnwpXCenterMainClient(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT             mrc = 0;

    TRY_LOUD(excpt1)
    {
        switch (msg)
        {
            /*
             * WM_CREATE:
             *
             */

            case WM_CREATE:
                // mp1 has control data
                WinSetWindowPtr(hwnd, QWL_USER, mp1);

                // return 0
            break;

            /*
             * WM_PAINT:
             *
             */

            case WM_PAINT:
                ClientPaint(hwnd);
            break;

            /*
             * WM_MOUSEMOVE:
             *
             */

            case WM_MOUSEMOVE:
                mrc = ClientMouseMove(hwnd, mp1);
            break;

            /*
             * WM_BUTTON1DOWN:
             *
             */

            case WM_BUTTON1DOWN:
                mrc = ClientButton1Down(hwnd, mp1);
            break;

            /*
             * WM_CONTEXTMENU:
             *
             */

            case WM_CONTEXTMENU:
                mrc = ClientContextMenu(hwnd, mp1);
            break;

            /*
             * WM_MENUEND:
             *
             */

            case WM_MENUEND:
                ClientMenuEnd(hwnd, mp2);
            break;

            /*
             * DM_DRAGOVER:
             *
             */

            case DM_DRAGOVER:
                mrc = ClientDragOver(hwnd, mp1);
            break;

            /*
             * DM_DRAGLEAVE:
             *
             */

            case DM_DRAGLEAVE:
                // remove emphasis
                RemoveDragoverEmphasis(hwnd);
            break;

            /*
             * DM_DROP:
             *
             */

            case DM_DROP:
                ClientDrop(hwnd, mp1, mp2);
            break;

            /*
             * WM_CONTROL:
             *
             */

            case WM_CONTROL:
                ClientControl(hwnd, mp1, mp2);
            break;

            /*
             * WM_CLOSE:
             *      this is sent to the client by the frame if
             *      it receives WM_CLOSE itself.
             */

            case WM_CLOSE:
                // destroy parent (the frame)
                WinDestroyWindow(WinQueryWindow(hwnd, QW_PARENT));
            break;

            /*
             * WM_DESTROY:
             *
             */

            case WM_DESTROY:
                ClientDestroy(hwnd);
            break;

            case XCM_SETWIDGETSIZE:
            {
                PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER);
                SetWidgetSize(pXCenterData, (HWND)mp1, (ULONG)mp2);
            break; }

            case XCM_REFORMAT:
            {
                PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER);
                ctrpReformat(pXCenterData,
                             (ULONG)mp1);       // flags
            break; }

            case XCM_SAVESETUP:
                ClientSaveSetup(hwnd, (HWND)mp1, (PSZ)mp2);
            break;

            case XCM_CREATEWIDGET:
            {
                // this msg is only used for creating the window on the
                // GUI thread because method calls may run on any thread...
                PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER);
                CreateOneWidget(pXCenterData,
                                (PXCENTERWIDGETSETTING)mp1,
                                (ULONG)mp2);        // ulWidgetIndex
            break; }

            case XCM_DESTROYWIDGET:
                // this msg is only used for destroying the window on the
                // GUI thread because method calls may run on any thread...
                WinDestroyWindow((HWND)mp1);
                        // this better be a widget window...
                        // ctrDefWidgetProc takes care of cleanup
            break;

            default:
                mrc = WinDefWindowProc(hwnd, msg, mp1, mp2);
        }
    }
    CATCH(excpt1) {} END_CATCH();

    return (mrc);
}

/* ******************************************************************
 *
 *   XCenter widget class management
 *
 ********************************************************************/

/*
 *@@ ctrFreeModule:
 *      wrapper around DosFreeModule which
 *      attempts to call the "UnInitModule"
 *      export from the widget DLL beforehand.
 *
 *      May run on any thread.
 *
 *@@added V0.9.7 (2000-12-07) [umoeller]
 */

APIRET ctrFreeModule(HMODULE hmod)
{
    // the following might crash
    TRY_QUIET(excpt2)
    {
        PFNWGTUNINITMODULE pfnWgtUnInitModule = NULL;
        APIRET arc2 = DosQueryProcAddr(hmod,
                                       2,      // ordinal
                                       NULL,
                                       (PFN*)(&pfnWgtUnInitModule));
        if ((arc2 == NO_ERROR) && (pfnWgtUnInitModule))
        {
            pfnWgtUnInitModule();
        }
    }
    CATCH(excpt2) {} END_CATCH();

    return (DosFreeModule(hmod));
}

/*
 *@@ ctrpLoadClasses:
 *      initializes the global array of widget classes.
 *
 *      This also goes thru the plugins\xcenter subdirectory
 *      of the XWorkplace installation directory and tests
 *      the DLLs in there for widget plugins.
 *
 *      Note: For each ctrpLoadClasses call, there must be
 *      a matching ctrpFreeClasses call, or the plugin DLLs
 *      will never be removed. This function maintains a
 *      reference count to the global data so calls to this
 *      function may be nested.
 *
 *      May run on any thread.
 */

VOID ctrpLoadClasses(VOID)
{
    _Pmpf((__FUNCTION__ ": G_ulWidgetClassesRefCount is %d", G_ulWidgetClassesRefCount));

    if (!G_fModulesInitialized)
    {
        // very first call:
        lstInit(&G_llModules, FALSE);
        G_fModulesInitialized = TRUE;
    }

    if (G_paWidgetClasses == NULL)
    {
        // widget classes not loaded yet (or have been released again):

        HAB             hab = WinQueryAnchorBlock(cmnQueryActiveDesktopHWND());

        HMODULE         hmodXFLDR = cmnQueryMainCodeModuleHandle();

        PXCENTERWIDGETCLASS paWidgetClasses = NULL;
        ULONG           cWidgetClasses = 0;

        // built-in widget classes:
        ULONG           cBuiltinClasses = sizeof(G_aBuiltInWidgets) / sizeof(G_aBuiltInWidgets[0]),
                        ul = 0;

        APIRET          arc = NO_ERROR;
        CHAR            szPluginsDir[2*CCHMAXPATH],
                        szSearchMask[2*CCHMAXPATH];
        HDIR            hdirFindHandle = HDIR_CREATE;
        FILEFINDBUF3    ffb3 = {0};      // returned from FindFirst/Next
        ULONG           cbFFB3 = sizeof(FILEFINDBUF3);
        ULONG           ulFindCount = 1;  // look for 1 file at a time

        LINKLIST        llWidgetClasses;    // contains XCENTERWIDGETCLASS structs
        lstInit(&llWidgetClasses, FALSE);   // no auto-free!

        // compose path for widget plugin DLLs
        cmnQueryXWPBasePath(szPluginsDir);
        strcat(szPluginsDir, "\\plugins\\xcenter");

        // step 1: append built-in widgets to list
        for (ul = 0;
             ul < cBuiltinClasses;
             ul++)
        {
            lstAppendItem(&llWidgetClasses,
                          &G_aBuiltInWidgets[ul]);
        }

        // step 2: append plugin DLLs to list

        sprintf(szSearchMask, "%s\\%s", szPluginsDir, "*.dll");

        _Pmpf((__FUNCTION__ ": searching for '%s'", szSearchMask));

        arc = DosFindFirst(szSearchMask,
                           &hdirFindHandle,
                           // find everything except directories
                           FILE_ARCHIVED | FILE_HIDDEN | FILE_SYSTEM | FILE_READONLY,
                           &ffb3,
                           cbFFB3,
                           &ulFindCount,
                           FIL_STANDARD);
        // and start looping...
        while (arc == NO_ERROR)
        {
            // alright... we got the file's name in ffb3.achName
            CHAR            szDLL[2*CCHMAXPATH],
                            szError[CCHMAXPATH] = "";
            HMODULE         hmod = NULLHANDLE;
            APIRET          arc2 = NO_ERROR;

            sprintf(szDLL, "%s\\%s", szPluginsDir, ffb3.achName);

            _Pmpf(("   found %s", szDLL));

            arc2 = DosLoadModule(szError,
                                 sizeof(szError),
                                 szDLL,
                                 &hmod);
            _Pmpf(("   DosLoadModule returned %d for '%s', szError: '%s'",
                    arc2, szDLL, szError));
            if (arc2 == NO_ERROR)
            {
                CHAR    szErrorMsg[500] = "nothing.";
                        // room for error msg by DLL
                PFNWGTINITMODULE pfnWgtInitModule = NULL;
                arc2 = DosQueryProcAddr(hmod,
                                        1,      // ordinal
                                        NULL,
                                        (PFN*)(&pfnWgtInitModule));
                _Pmpf(("   DosQueryProcAddr returned %d", arc2));
                if ((arc2 == NO_ERROR) && (pfnWgtInitModule))
                {
                    // yo, we got the function:
                    // call it to get widgets info
                    // (protect this with an exception handler, because
                    // this might crash)
                    TRY_QUIET(excpt2)
                    {
                        PXCENTERWIDGETCLASS paClasses = NULL;
                        ULONG cClassesThis = pfnWgtInitModule(hab,
                                                              hmodXFLDR,
                                                              &paClasses,
                                                              szErrorMsg);

                        _Pmpf(("  pfnQueryWidgetClasses returned %d for %s",
                                cClassesThis,
                                szDLL));

                        if (cClassesThis)
                        {
                            // paClasses must now point to an array of
                            // cClassesThis XCENTERWIDGETCLASS structures;
                            // copy these
                            for (ul = 0;
                                 ul < cClassesThis;
                                 ul++)
                            {
                                lstAppendItem(&llWidgetClasses,
                                              &paClasses[ul]);
                            }

                            // append this module to the global list of
                            // loaded modules
                            lstAppendItem(&G_llModules,
                                          (PVOID)hmod);
                        }
                        else
                            // no classes in module or error:
                            arc2 = ERROR_INVALID_ORDINAL;
                    }
                    CATCH(excpt2)
                    {
                        arc2 = ERROR_INVALID_ORDINAL;
                    } END_CATCH();
                } // end if DosQueryProcAddr

                if (arc2)
                {
                    // error occured (or crash):
                    // unload the module again
                    ctrFreeModule(hmod);
                    cmnLog(__FILE__, __LINE__, __FUNCTION__,
                           "WgtInitModule call @1 failed for plugin DLL %s."
                              "\nDLL error msg: %s",
                           szDLL,
                           szErrorMsg);
                }
            } // end if DosLoadModule

            // find next DLL
            ulFindCount = 1;
            arc = DosFindNext(hdirFindHandle,
                              &ffb3,
                              cbFFB3,
                              &ulFindCount);
        } // while (arc == NO_ERROR)

        DosFindClose(hdirFindHandle);

        // step 3: build array with all classes

        cWidgetClasses = lstCountItems(&llWidgetClasses);
        if (cWidgetClasses)
        {
            paWidgetClasses = malloc(sizeof(XCENTERWIDGETCLASS) * cWidgetClasses);
            if (paWidgetClasses)
            {
                PLISTNODE pNode = lstQueryFirstNode(&llWidgetClasses);
                ul = 0;
                while (pNode)
                {
                    PXCENTERWIDGETCLASS pClassThis = (PXCENTERWIDGETCLASS)pNode->pItemData;
                    memcpy(&paWidgetClasses[ul],
                           pClassThis,
                           sizeof(XCENTERWIDGETCLASS));
                    pNode = pNode->pNext;
                    ul++;
                }
            }
        }

        G_paWidgetClasses = paWidgetClasses;
        G_cWidgetClasses = cWidgetClasses;

        // clean up
        lstClear(&llWidgetClasses);
    }

    G_ulWidgetClassesRefCount++;
}

/*
 *@@ ctrpFreeClasses:
 *      decreases the reference count for the global
 *      widget classes array by one. If 0 is reached,
 *      all allocated resources are freed, and plugin
 *      DLL's are unloaded.
 *
 *      See ctrpLoadClasses().
 *
 *      May run on any thread.
 *
 *      ### add a global mutex for this
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
 */

VOID ctrpFreeClasses(VOID)
{
    if (G_ulWidgetClassesRefCount == 0)
        cmnLog(__FILE__, __LINE__, __FUNCTION__,
                       "G_ulWidgetClassesRefCount is already 0!");
    else
    {
        G_ulWidgetClassesRefCount--;
        if (G_ulWidgetClassesRefCount == 0)
        {
            // no more references to the data:
            PLISTNODE pNode;

            // free memory
            if (G_paWidgetClasses)
            {
                free(G_paWidgetClasses);
                G_paWidgetClasses = NULL;
            }

            // unload modules
            pNode = lstQueryFirstNode(&G_llModules);
            while (pNode)
            {
                HMODULE hmod = (HMODULE)pNode->pItemData;
                _Pmpf((__FUNCTION__ ": Unloading hmod %lX", hmod));
                ctrFreeModule(hmod);

                pNode = pNode->pNext;
            }

            lstClear(&G_llModules);
        }
    }

    _Pmpf((__FUNCTION__ ": leaving, G_ulWidgetClassesRefCount is %d", G_ulWidgetClassesRefCount));
}

/*
 *@@ ctrpFindClass:
 *      finds the XCENTERWIDGETCLASS entry from the
 *      global array which has the given widget class
 *      name (_not_ PM window class name!).
 *
 *      Preconditions:
 *
 *      --  The widget classes must have been loaded
 *          first (by calling ctrpLoadClasses).
 *
 *      Postconditions:
 *
 *      --  This returns a plain pointer to an item
 *          in the global classes array. Once the
 *          classes are unloaded, the pointer must
 *          no longer be used.
 *
 *      May run on any thread.
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
 */

PXCENTERWIDGETCLASS ctrpFindClass(const char *pcszWidgetClass)
{
    // XCenterData *somThis = XCenterGetData(somSelf);
    PXCENTERWIDGETCLASS pClass = NULL,
                        paClasses;
    ULONG ul = 0;

    // ctrpLoadClasses();       // if not loaded yet

    paClasses = G_paWidgetClasses;

    for (ul = 0;
         ul < G_cWidgetClasses;
         ul++)
    {
        if (paClasses[ul].pcszWidgetClass == NULL)
        {
            _Pmpf((__FUNCTION__ ": paClasses[ul].pcszWidgetClass is NULL!"));
            break;
        }
        else if (pcszWidgetClass == NULL)
        {
            _Pmpf((__FUNCTION__ ": pcszWidgetClass is NULL!"));
            break;
        }
        else if (strcmp(paClasses[ul].pcszWidgetClass,
                        pcszWidgetClass) == 0)
        {
            // found:
            pClass = &paClasses[ul];
            break;
        }
    }

    // ctrpFreeClasses();

    return (pClass);    // can be 0
}

/* ******************************************************************
 *
 *   XCenter view implementation
 *
 ********************************************************************/

/*
 *@@ ctrpQueryWidgetIndexFromHWND:
 *      returns the index of the widget with the
 *      specified window handle. The index is zero-based,
 *      i.e. the first widget has the 0 index.
 *
 *      This only works if an XCenter view is currently
 *      open, of course.
 *
 *      Returns -1 if not found.
 *
 *      May run on any thread.
 *
 *@@added V0.9.7 (2000-12-04) [umoeller]
 */

ULONG ctrpQueryWidgetIndexFromHWND(XCenter *somSelf,
                                   HWND hwnd)
{
    ULONG ulIndex = -1;
    WPSHLOCKSTRUCT Lock;
    if (wpshLockObject(&Lock, somSelf))
    {
        XCenterData *somThis = XCenterGetData(somSelf);
        if (_pvOpenView)
        {
            // XCenter view currently open:
            PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)_pvOpenView;
            PLISTNODE pViewNode = lstQueryFirstNode(&pXCenterData->llWidgets);
            ULONG     ulThis = 0;
            while (pViewNode)
            {
                PWIDGETVIEWSTATE pView = (PWIDGETVIEWSTATE)pViewNode->pItemData;
                if (pView->Widget.hwndWidget == hwnd)
                {
                    ulIndex = ulThis;
                    break;
                }

                pViewNode = pViewNode->pNext;
                ulThis++;
            }
        }
    }
    wpshUnlockObject(&Lock);

    return (ulIndex);
}

/*
 *@@ AddWidgetSetting:
 *      adds a new XCENTERWIDGETSETTING to the internal
 *      list of widget settings.
 *
 *      pSetting is assumed to be dynamic (i.e. allocated
 *      with malloc()).
 *
 *      This does not update open views. It neither saves
 *      the XCenter instance data. This has only been
 *      put into a separate procedure because it's both
 *      needed by ctrpInsertWidget and ctrpUnstuffSettings.
 *
 *      The caller must lock the XCenter himself.
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
 *@@changed V0.9.7 (2000-12-13) [umoeller]: changed prototype
 */

VOID AddWidgetSetting(XCenter *somSelf,
                      PXCENTERWIDGETSETTING pSetting,  // in: new setting
                      ULONG ulBeforeIndex,             // in: index to insert before or -1 for end
                      PULONG pulNewItemCount,          // out: new settings count (ptr can be NULL)
                      PULONG pulNewWidgetIndex)        // out: index of new widget (ptr can be NULL)
{
    PLINKLIST pllWidgetSettings = ctrpQuerySettingsList(somSelf);
    if (    (ulBeforeIndex == -1)
         || (ulBeforeIndex >= lstCountItems(pllWidgetSettings))
       )
        // append to the end:
        lstAppendItem(pllWidgetSettings,
                      pSetting);
    else
        // append at specified position:
        lstInsertItemBefore(pllWidgetSettings,
                            pSetting,
                            ulBeforeIndex);
    if (pulNewItemCount)
        *pulNewItemCount = lstCountItems(pllWidgetSettings);
    if (pulNewWidgetIndex)
        *pulNewWidgetIndex = lstIndexFromItem(pllWidgetSettings, pSetting);
}

/*
 *@@ ctrpFreeWidgets:
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
 */

VOID ctrpFreeWidgets(XCenter *somSelf)
{
    WPSHLOCKSTRUCT Lock;
    if (wpshLockObject(&Lock, somSelf))
    {
        XCenterData *somThis = XCenterGetData(somSelf);
        _Pmpf((__FUNCTION__ ": entering, _pllWidgetSettings is %lX", _pllWidgetSettings));
        if (_pllWidgetSettings)
        {
            lstFree(_pllWidgetSettings);
            _pllWidgetSettings = NULL;
        }
    }
    wpshUnlockObject(&Lock);
}

/*
 *@@ ctrpQueryWidgets:
 *      implementation for XCenter::xwpQueryWidgets.
 *
 *@@added V0.9.7 (2000-12-08) [umoeller]
 */

PVOID ctrpQueryWidgets(XCenter *somSelf,
                       PULONG pulCount)
{
    PXCENTERWIDGETSETTING paSettings = NULL;

    WPSHLOCKSTRUCT Lock;
    if (wpshLockObject(&Lock, somSelf))
    {
        PLINKLIST pllSettings = ctrpQuerySettingsList(somSelf);
        PLISTNODE pNode = lstQueryFirstNode(pllSettings);
        ULONG cSettings = lstCountItems(pllSettings);
        paSettings = malloc(sizeof(XCENTERWIDGETSETTING) * cSettings);
        if (paSettings)
        {
            ULONG ul = 0;
            for (;
                 ul < cSettings;
                 ul++)
            {
                PXCENTERWIDGETSETTING pSource = (PXCENTERWIDGETSETTING)pNode->pItemData;
                paSettings[ul].pszWidgetClass = strhdup(pSource->pszWidgetClass);
                paSettings[ul].pszSetupString = strhdup(pSource->pszSetupString);

                pNode = pNode->pNext;
            }
        }
    }
    else
    {
        // we get here on crashes
        if (paSettings)
        {
            free(paSettings);
            paSettings = NULL;
        }
    }

    wpshUnlockObject(&Lock);

    return (paSettings);
}

/*
 *@@ ctrpFreeWidgetsBuf:
 *      implementation for XCenter::FreeWidgetsBuf.
 *
 *      May run on any thread.
 *
 *@@added V0.9.7 (2000-12-08) [umoeller]
 */

VOID ctrpFreeWidgetsBuf(PVOID pBuf,
                        ULONG ulCount)
{
    // no semaphore needed here, we created a copy above
    ULONG ul = 0;
    PXCENTERWIDGETSETTING paSettings = (PXCENTERWIDGETSETTING)pBuf;
    for (;
         ul < ulCount;
         ul++)
    {
        if (paSettings[ul].pszWidgetClass)
            free(paSettings[ul].pszWidgetClass);
        if (paSettings[ul].pszSetupString)
            free(paSettings[ul].pszSetupString);
    }

    free(paSettings);
}

/*
 *@@ ctrpInsertWidget:
 *      implementation for XCenter::xwpInsertWidget.
 *
 *      May run on any thread.
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
 */

BOOL ctrpInsertWidget(XCenter *somSelf,
                      ULONG ulBeforeIndex,
                      const char *pcszWidgetClass,
                      const char *pcszSetupString)
{
    BOOL brc = FALSE;
    WPSHLOCKSTRUCT Lock;
    if (wpshLockObject(&Lock, somSelf))
    {
        XCenterData *somThis = XCenterGetData(somSelf);

        _Pmpf((__FUNCTION__ ": entering with %s, %s",
                (pcszWidgetClass) ? pcszWidgetClass : "NULL",
                (pcszSetupString) ? pcszSetupString : "NULL"));

        if (pcszWidgetClass)
        {
            PXCENTERWIDGETSETTING pSetting = malloc(sizeof(XCENTERWIDGETSETTING));

            if (pSetting)
            {
                ULONG   ulNewItemCount = 0,
                        ulWidgetIndex = 0;

                pSetting->pszWidgetClass = strdup(pcszWidgetClass);
                if (pcszSetupString)
                    pSetting->pszSetupString = strdup(pcszSetupString);
                else
                    pSetting->pszSetupString = NULL;

                // add new widget setting to internal linked list
                AddWidgetSetting(somSelf,
                                 pSetting,
                                 ulBeforeIndex,
                                 &ulNewItemCount,
                                 &ulWidgetIndex);

                _Pmpf(("  widget added, new item count: %d", ulNewItemCount));

                if (_pvOpenView)
                {
                    // XCenter view currently open:
                    PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)_pvOpenView;
                    // we must send msg instead of calling CreateOneWidget
                    // directly, because that func may only run on the
                    // XCenter GUI thread
                    WinSendMsg(pXCenterData->Globals.hwndClient,
                               XCM_CREATEWIDGET,
                               (MPARAM)pSetting,
                               (MPARAM)ulWidgetIndex);
                            // new widget is invisible, and at a random position
                    WinPostMsg(pXCenterData->Globals.hwndClient,
                               XCM_REFORMAT,
                               (MPARAM)(XFMF_RECALCHEIGHT
                                        | XFMF_REPOSITIONWIDGETS
                                        | XFMF_SHOWWIDGETS),
                               0);
                }

                // save instance data (with that linked list)
                _wpSaveDeferred(somSelf);

                // update the "widgets" notebook page, if open
                ntbUpdateVisiblePage(somSelf, SP_XCENTER_WIDGETS);

                brc = TRUE;
            }
        }
    }
    wpshUnlockObject(&Lock);

    return (brc);
}

/*
 *@@ ctrpRemoveWidget:
 *      implementation for XCenter::xwpRemoveWidget.
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
 */

BOOL ctrpRemoveWidget(XCenter *somSelf,
                      ULONG ulIndex)
{
    BOOL brc = FALSE;
    WPSHLOCKSTRUCT Lock;
    if (wpshLockObject(&Lock, somSelf))
    {
        PLINKLIST pllWidgetSettings = ctrpQuerySettingsList(somSelf);
        PLISTNODE pSettingsNode = lstNodeFromIndex(pllWidgetSettings, ulIndex);
        PXCENTERWINDATA pXCenterData = NULL;

        if (pSettingsNode)
        {
            // widget exists:
            XCenterData *somThis = XCenterGetData(somSelf);
            PXCENTERWIDGETSETTING pSetting = (PXCENTERWIDGETSETTING)pSettingsNode->pItemData;

            if (_pvOpenView)
            {
                // XCenter view currently open:
                PLISTNODE pViewNode;
                pXCenterData = (PXCENTERWINDATA)_pvOpenView;
                pViewNode = lstNodeFromIndex(&pXCenterData->llWidgets,
                                             ulIndex);
                if (pViewNode)
                {
                    // we must send a msg instead of doing WinDestroyWindow
                    // directly because only the XCenter GUI thread can
                    // destroy the window
                    PWIDGETVIEWSTATE pView = (PWIDGETVIEWSTATE)pViewNode->pItemData;
                    WinSendMsg(pXCenterData->Globals.hwndClient,
                               XCM_DESTROYWIDGET,
                               (MPARAM)pView->Widget.hwndWidget,
                               0);
                        // the window is responsible for cleaning up pView->pUser;
                        // ctrDefWidgetProc will also free pView and remove it
                        // from the widget views list
                }
            } // end if (_hwndOpenView)

            if (pSetting->pszWidgetClass)
                free(pSetting->pszWidgetClass);
            if (pSetting->pszSetupString)
                free(pSetting->pszSetupString);

            brc = lstRemoveNode(pllWidgetSettings, pSettingsNode);
                        // this list is in auto-free mode

            if (brc)
                // save instance data (with that linked list)
                _wpSaveDeferred(somSelf);

            // update the "widgets" notebook page, if open
            ntbUpdateVisiblePage(somSelf, SP_XCENTER_WIDGETS);

        } // end if (pSettingsNode)

        if (pXCenterData)
        {
            // we found the open view above:
            WinPostMsg(pXCenterData->Globals.hwndClient,
                       XCM_REFORMAT,
                       (MPARAM)(XFMF_RECALCHEIGHT | XFMF_REPOSITIONWIDGETS),
                       0);
        }
    }
    wpshUnlockObject(&Lock);

    return (brc);
}

/*
 *@@ ctrpMoveWidget:
 *      implementation for XCenter::xwpMoveWidget.
 *
 *@@added V0.9.7 (2000-12-10) [umoeller]
 */

BOOL ctrpMoveWidget(XCenter *somSelf,
                    ULONG ulIndex2Move,
                    ULONG ulBeforeIndex)
{
    BOOL brc = FALSE;

    WPSHLOCKSTRUCT Lock;
    if (wpshLockObject(&Lock, somSelf))
    {
        PLINKLIST pllWidgetSettings = ctrpQuerySettingsList(somSelf);
        PLISTNODE pSettingsNode = lstNodeFromIndex(pllWidgetSettings, ulIndex2Move);
        PXCENTERWINDATA pXCenterData = NULL;

        if (!pSettingsNode)
            // index invalid: use last widget instead
            pSettingsNode = lstNodeFromIndex(pllWidgetSettings,
                                             lstCountItems(pllWidgetSettings) - 1);

        if (pSettingsNode)
        {
            // widget exists:
            XCenterData *somThis = XCenterGetData(somSelf);
            PXCENTERWIDGETSETTING pSetting = (PXCENTERWIDGETSETTING)pSettingsNode->pItemData,
                                  pNewSetting = NULL;

            if (_pvOpenView)
            {
                // XCenter view currently open:
                PLISTNODE pViewNode;
                pXCenterData = (PXCENTERWINDATA)_pvOpenView;
                pViewNode = lstNodeFromIndex(&pXCenterData->llWidgets,
                                             ulIndex2Move);
                if (pViewNode)
                {
                    // the list is NOT in auto-free mode;
                    // re-insert the node at the new position
                    lstInsertItemBefore(&pXCenterData->llWidgets,
                                        pViewNode->pItemData,
                                        ulBeforeIndex);
                    // remove old node
                    lstRemoveNode(&pXCenterData->llWidgets, pViewNode);
                }
            } // end if (_hwndOpenView)

            // now move the settings
            /* if (pSetting->pszWidgetClass)
                free(pSetting->pszWidgetClass);
            if (pSetting->pszSetupString)
                free(pSetting->pszSetupString); */

            // the settings list is in auto-free mode,
            // so we have to create a new one first
            pNewSetting = (PXCENTERWIDGETSETTING)malloc(sizeof(XCENTERWIDGETSETTING));
            memcpy(pNewSetting, pSetting, sizeof(*pNewSetting));
            brc = lstRemoveNode(pllWidgetSettings, pSettingsNode);
                    // this frees pSetting, but not the member pointers
            if (brc)
                lstInsertItemBefore(pllWidgetSettings,
                                    pNewSetting,
                                    ulBeforeIndex);

            if (brc)
                // save instance data (with that linked list)
                _wpSaveDeferred(somSelf);

            // update the "widgets" notebook page, if open
            ntbUpdateVisiblePage(somSelf, SP_XCENTER_WIDGETS);

        } // end if (pSettingsNode)

        if (pXCenterData)
        {
            // we found the open view above:
            WinPostMsg(pXCenterData->Globals.hwndClient,
                       XCM_REFORMAT,
                       (MPARAM)(XFMF_REPOSITIONWIDGETS),
                       0);
        }
    }
    wpshUnlockObject(&Lock);

    return (brc);
}

/*
 *@@ ctrpStuffSettings:
 *      packs all the settings into a binary array
 *      which can be saved with wpSaveState.
 *
 *      The format is defined as two strings alternating
 *      for each widget: the PM window class name, and
 *      then the setup string (or a space if there's none).
 *
 *      The caller must free() the return value.
 *
 *      The XCenter stores all the widget settings (that is:
 *      descriptions of the widget instances plus their
 *      respective setup strings) in its instance data.
 *
 *      The problem is that the widget settings require
 *      that the plugin DLLs be loaded, but we don't want
 *      to keep these DLLs loaded unless an XCenter view
 *      is currently open. However, XCenter::wpRestoreState
 *      is called already when the XCenter gets awakened
 *      by the WPS (i.e. when its folder is populated).
 *
 *      The solution is that XCenter::wpSaveState saves
 *      a binary block of memory only, and XCenter::wpRestoreState
 *      loads that block. Only when the widget settings
 *      are really needed, they are unpacked into a linked
 *      list (using ctrpUnstuffSettings, which also loads the
 *      DLLs). XCenter::wpSaveState then packs the linked list
 *      again by calling this function.
 *
 *      In other words, this is a mechanism to defer loading
 *      the plugin DLLs until they are really needed.
 *
 *      The caller must lock the XCenter himself.
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
 */

PSZ ctrpStuffSettings(XCenter *somSelf,
                      PULONG pcbSettingsArray)   // out: array byte count
{
    PSZ     psz = 0;
    ULONG   cb = 0;
    XCenterData *somThis = XCenterGetData(somSelf);

    _Pmpf((__FUNCTION__ ": entering, _pszPackedWidgetSettings is %lX", _pszPackedWidgetSettings));

    if (_pllWidgetSettings)
    {
        PLISTNODE pNode = lstQueryFirstNode(_pllWidgetSettings);
        while (pNode)
        {
            PXCENTERWIDGETSETTING pSetting = (PXCENTERWIDGETSETTING)pNode->pItemData;
            strhArrayAppend(&psz,
                            pSetting->pszWidgetClass,
                            &cb);
            if (pSetting->pszSetupString)
                strhArrayAppend(&psz,
                                pSetting->pszSetupString,
                                &cb);
            else
                // no setup string: use a space
                strhArrayAppend(&psz,
                                " ",
                                &cb);

            pNode = pNode->pNext;
        }
    }

    *pcbSettingsArray = cb;

    return (psz);
}

/*
 *@@ ctrpUnstuffSettings:
 *      unpacks a settings array previously packed
 *      with ctrpStuffSettings.
 *
 *      After this has been called, a linked list
 *      of XCENTERWIDGETSETTING exists in the XCenter's
 *      instance data (in the _pllWidgetSettings field).
 *
 *      See ctrpStuffSettings for details.
 *
 *      The caller must lock the XCenter himself.
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
 */

ULONG ctrpUnstuffSettings(XCenter *somSelf)
{
    ULONG ulrc = 0;
    XCenterData *somThis = XCenterGetData(somSelf);

    _Pmpf((__FUNCTION__": entering, _pszPackedWidgetSettings is %lX", _pszPackedWidgetSettings));

    if (_pszPackedWidgetSettings)
    {
        const char *p = _pszPackedWidgetSettings;

        ctrpLoadClasses();      // just to make sure; ctrpFindClass would otherwise
                                // load them for each loop

        while (    (*p)
                && (p < _pszPackedWidgetSettings + _cbPackedWidgetSettings)
              )
        {
            ULONG ulClassLen = strlen(p),
                  ulSetupLen = 0;

            // find class from PM class name
            PXCENTERWIDGETSETTING pSetting
                = (PXCENTERWIDGETSETTING)malloc(sizeof(XCENTERWIDGETSETTING));
            if (pSetting)
                pSetting->pszWidgetClass = strdup(p);

            // after class name, we have the setup string
            p += ulClassLen + 1;    // go beyond null byte

            ulSetupLen = strlen(p);
            if (pSetting)
            {
                if (ulSetupLen > 1)
                    pSetting->pszSetupString = strdup(p);
                else
                    pSetting->pszSetupString = NULL;

                // store in list
                AddWidgetSetting(somSelf,
                                 pSetting,
                                 -1,
                                 NULL,
                                 NULL);
            }

            // go for next setting
            p += ulSetupLen + 1;    // go beyond null byte
        }

        // remove the packed settings;
        // from now on we'll use the list
        free(_pszPackedWidgetSettings);
        _pszPackedWidgetSettings = NULL;

        ctrpFreeClasses();
    }

    return (ulrc);
}

/*
 *@@ ctrpQuerySettingsList:
 *      returns the list of XWPWIDGETSETTING structures
 *      from the XCenter instance data. This unpacks
 *      the binary array if called for the first time.
 *
 *      The caller must lock the XCenter himself.
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
 */

PLINKLIST ctrpQuerySettingsList(XCenter *somSelf)
{
    XCenterData *somThis = XCenterGetData(somSelf);

    _Pmpf((__FUNCTION__ ": entering, G_ulWidgetClassesRefCount is %d", G_ulWidgetClassesRefCount));

    if (!_pllWidgetSettings)
    {
        // list not created yet:
        _pllWidgetSettings = lstCreate(TRUE);

        ctrpUnstuffSettings(somSelf);
    }

    return (_pllWidgetSettings);
}

/*
 *@@ ctrpSetPriority:
 *
 *@@added V0.9.7 (2001-01-03) [umoeller]
 */

BOOL ctrpSetPriority(XCenter *somSelf,
                     ULONG ulClass,
                     LONG lDelta)
{
    BOOL brc = FALSE;

    WPSHLOCKSTRUCT Lock;
    if (wpshLockObject(&Lock,
                       somSelf))
    {
        XCenterData *somThis = XCenterGetData(somSelf);
        _ulPriorityClass = ulClass;
        _lPriorityDelta = lDelta;

        if (_pvOpenView)
        {
            PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)_pvOpenView;
            DosSetPriority(PRTYS_THREAD,
                           ulClass,
                           lDelta,
                           pXCenterData->tidXCenter);       // tid of XCenter GUI thread
        }
    }
    else
        brc = FALSE;
    wpshUnlockObject(&Lock);

    return (brc);
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
            XSTRING strTemp;
            PLINKLIST pllSettings = ctrpQuerySettingsList(somSelf);
            PLISTNODE pNode;

            xstrInit(&strTemp, 400);

            /*
             * build string
             *
             */

            if (_ulWindowStyle & WS_TOPMOST)
                xstrcat(&strTemp, "ALWAYSONTOP=YES;");
            if ((_ulWindowStyle & WS_ANIMATE) != 0)
                xstrcat(&strTemp, "ANIMATE=YES;");
            if (_ulAutoHide)
            {
                CHAR szTemp[100];
                xstrcat(&strTemp, "AUTOHIDE=");
                sprintf(szTemp, "%d;", _ulAutoHide);
                xstrcat(&strTemp, szTemp);
            }
            if (_ulPosition == XCENTER_TOP)
                xstrcat(&strTemp, "POSITION=TOP;");

            pNode = lstQueryFirstNode(pllSettings);
            if (pNode)
            {
                BOOL    fFirstWidget = TRUE;
                xstrcat(&strTemp, "WIDGETS=");

                // we have widgets:
                // go thru all of them and list all widget classes and setup strings.
                while (pNode)
                {
                    PXCENTERWIDGETSETTING pSetting = (PXCENTERWIDGETSETTING)pNode->pItemData;

                    if (!fFirstWidget)
                        // not first run:
                        // add separator
                        xstrcat(&strTemp, ",");
                    else
                        fFirstWidget = FALSE;

                    // add widget class
                    xstrcat(&strTemp, pSetting->pszWidgetClass);

                    if (    (pSetting->pszSetupString)
                         && (strlen(pSetting->pszSetupString))
                       )
                    {
                        // widget has a setup string:
                        // add that in brackets
                        XSTRING strSetup2;

                        // characters that must be encoded
                        CHAR    achEncode[] = "%,();=";

                        ULONG   ul = 0;

                        // copy widget setup string to temporary buffer
                        // for encoding... this has "=" and ";"
                        // chars in it, and these should not appear
                        // in the WPS setup string
                        xstrInitCopy(&strSetup2,
                                     pSetting->pszSetupString,
                                     40);

                        // add first separator
                        xstrcat(&strTemp, "(");

                        // now encode the widget setup string...
                        for (ul = 0;
                             ul < strlen(achEncode);
                             ul++)
                        {
                            CHAR        szFind[3] = "?",
                                        szReplace[10] = "%xx";
                            XSTRING     strFind,
                                        strReplace;
                            size_t  ShiftTable[256];
                            BOOL    fRepeat = FALSE;
                            ULONG ulOfs = 0;

                            // search string:
                            szFind[0] = achEncode[ul];
                            xstrInitSet(&strFind, szFind);

                            // replace string: ASCII encoding
                            sprintf(szReplace, "%c%lX", '%', achEncode[ul]);
                            xstrInitSet(&strReplace, szReplace);

                            // replace all occurences
                            while (xstrrpl(&strSetup2,
                                           &ulOfs,
                                           &strFind,
                                           &strReplace,
                                           ShiftTable,
                                           &fRepeat))
                                    ;
                        } // for ul; next encoding

                        // now append encoded widget setup string
                        xstrcat(&strTemp, strSetup2.psz);

                        // add terminator
                        xstrcat(&strTemp, ")");

                        xstrClear(&strSetup2);
                    } // end if (    (pSetting->pszSetupString)...

                    pNode = pNode->pNext;
                } // end for widgets

                xstrcat(&strTemp, ";");
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
 *@@ ctrpModifyPopupMenu:
 *      implementation for XCenter::wpModifyPopupMenu.
 *
 *      This can run on any thread.
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
 */

BOOL ctrpModifyPopupMenu(XCenter *somSelf,
                         HWND hwndMenu)
{
    BOOL brc = TRUE;
    WPSHLOCKSTRUCT Lock;
    if (wpshLockObject(&Lock, somSelf))
    {
        XCenterData *somThis = XCenterGetData(somSelf);
        MENUITEM mi;
        PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
        // get handle to the "Open" submenu in the
        // the popup menu
        if (WinSendMsg(hwndMenu,
                       MM_QUERYITEM,
                       MPFROM2SHORT(WPMENUID_OPEN, TRUE),
                       (MPARAM)&mi))
        {
            // mi.hwndSubMenu now contains "Open" submenu handle,
            // which we add items to now
            PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();
            winhInsertMenuItem(mi.hwndSubMenu, MIT_END,
                               (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_XWPVIEW),
                               "XCenter",
                               MIS_TEXT, 0);
        }

        if (_fShowingOpenViewMenu)
        {
            // context menu for open XCenter client:
            HWND    hwndWidgetsSubmenu = NULLHANDLE;

            winhInsertMenuSeparator(hwndMenu,
                                    MIT_END,
                                    (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_SEPARATOR));
            ctrpLoadClasses();

            hwndWidgetsSubmenu =  winhInsertSubmenu(hwndMenu,
                                                    MIT_END,
                                                    (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_SEPARATOR),
                                                    "~Add widget", // ###
                                                    MIS_TEXT,
                                                    0, NULL, 0, 0);
            if (G_paWidgetClasses)
            {
                ULONG ul = 0;
                // insert all the classes
                for (; ul < G_cWidgetClasses; ul++)
                {
                    ULONG ulAttr = 0;

                    // should this be added?
                    if (G_paWidgetClasses[ul].ulClassFlags & WGTF_NOUSERCREATE)
                        ulAttr |= MIA_DISABLED;
                    else
                    {
                        // if a widget of the same class already exists
                        // and the class doesn't allow multiple instances,
                        // disable this menu item
                        PLINKLIST pllWidgetSettings = ctrpQuerySettingsList(somSelf);
                        PLISTNODE pNode = lstQueryFirstNode(pllWidgetSettings);
                        while (pNode)
                        {
                            PXCENTERWIDGETSETTING pSettingThis = (PXCENTERWIDGETSETTING)pNode->pItemData;
                            if (strcmp(pSettingThis->pszWidgetClass,
                                       G_paWidgetClasses[ul].pcszWidgetClass)
                                   == 0)
                            {
                                // class found:
                                if (G_paWidgetClasses[ul].ulClassFlags & WGTF_UNIQUEPERXCENTER)
                                    ulAttr = MIA_DISABLED;

                                break;
                            }
                            pNode = pNode->pNext;
                        }
                    }

                    winhInsertMenuItem(hwndWidgetsSubmenu,
                                       MIT_END,
                                       pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_VARIABLE + ul,
                                       (PSZ)G_paWidgetClasses[ul].pcszClassTitle,
                                       MIS_TEXT,
                                       ulAttr);
                }
            }

            cmnAddCloseMenuItem(hwndMenu);

            ctrpFreeClasses();

            _fShowingOpenViewMenu = FALSE;
        }
    }
    wpshUnlockObject(&Lock);

    return (brc);
}

/*
 *@@ ctrp_fntXCenter:
 *      the XCenter thread. One of these gets created
 *      by ctrpCreateXCenterView for each XCenter that
 *      is opened and keeps running while the XCenter
 *      is open. The XCenter frame posts WM_QUIT when
 *      the XCenter is closed.
 *
 *      On input, ctrpCreateXCenterView specifies the
 *      somSelf field in XCENTERWINDATA. Also, the USEITEM
 *      ulView is set.
 *
 *      ctrpCreateXCenterView waits on XCENTERWINDATA.hevRunning
 *      and will then expect the XCenter's frame window in
 *      XCENTERWINDATA.Globals.hwndFrame.
 *
 *      This thread is created with a PM message queue.
 *
 *@@added V0.9.7 (2001-01-03) [umoeller]
 */

void _Optlink ctrp_fntXCenter(PTHREADINFO ptiMyself)
{
    BOOL fCreated = FALSE;
    PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)ptiMyself->ulData;

    if (pXCenterData)
    {
        XCenterData *somThis = XCenterGetData(pXCenterData->somSelf);
        PXCENTERGLOBALS pGlobals = &pXCenterData->Globals;
        SWP swpFrame;

        TRY_LOUD(excpt1)
        {
            // set priority to what user wants
            DosSetPriority(PRTYS_THREAD,
                           _ulPriorityClass,
                           _lPriorityDelta,
                           0);      // current thread

            pXCenterData->tidXCenter = ptiMyself->tid;

            ctrpLoadClasses();
                    // matching "unload" is in WM_DESTROY of the client

            lstInit(&pXCenterData->llWidgets,
                    FALSE);      // no auto-free

            pXCenterData->Globals.pCountrySettings = &G_CountrySettings;
            prfhQueryCountrySettings(&G_CountrySettings);

            pXCenterData->cxFrame = winhQueryScreenCX();

            pGlobals->cxMiniIcon = WinQuerySysValue(HWND_DESKTOP, SV_CXICON) / 2;

            pGlobals->lcol3DDark = WinQuerySysColor(HWND_DESKTOP, SYSCLR_BUTTONDARK, 0);
            pGlobals->lcol3DLight = WinQuerySysColor(HWND_DESKTOP, SYSCLR_BUTTONLIGHT, 0);

            // copy display style so that the widgets can see it
            pGlobals->ulDisplayStyle = _ulDisplayStyle;

            // create the XCenter frame with our special frame
            // window class
            memset(&swpFrame, 0, sizeof(swpFrame));
            swpFrame.x = 0;
            swpFrame.cx = pXCenterData->cxFrame;
            if (_ulPosition == XCENTER_BOTTOM)
            {
                swpFrame.y = 0;
                swpFrame.cy = 20;       // changed later
            }
            else
            {
                // at the top:
                swpFrame.cy = 20;
                swpFrame.y = winhQueryScreenCY() - swpFrame.cy;
            }

            if (_ulDisplayStyle == XCS_BUTTON)
            {
                // button style:
                pGlobals->ulSpacing = WinQuerySysValue(HWND_DESKTOP, SV_CXDLGFRAME);
            }
            else
                pGlobals->ulSpacing = 1;

            pXCenterData->ulBorderWidth = pGlobals->ulSpacing;

            pGlobals->hwndFrame
                = winhCreateStdWindow(HWND_DESKTOP, // frame's parent
                                      &swpFrame,
                                      FCF_NOBYTEALIGN,
                                      _ulWindowStyle,   // frame style
                                      _wpQueryTitle(pXCenterData->somSelf),
                                      0,
                                      WC_XCENTER_CLIENT, // client class
                                      WS_VISIBLE,       // client style
                                      0,
                                      pXCenterData,     // client control data
                                      &pGlobals->hwndClient);
                                            // out: client window

            if ((pGlobals->hwndFrame) && (pGlobals->hwndClient))
                // frame and client created:
                fCreated = TRUE;
        }
        CATCH(excpt1) {} END_CATCH();

        // in any case, post the event semaphore
        // to notify thread 1 that we're done
        DosPostEventSem(pXCenterData->hevRunning);

        if (fCreated)
        {
            // successful above:
            // protect the entire thread with the exception
            // handler (this includes the message loop)
            TRY_LOUD(excpt1)
            {
                // RECTL rclClient;
                SWP swpClient;
                pGlobals->hab = WinQueryAnchorBlock(pGlobals->hwndFrame);

                // store win data in frame's QWL_USER
                // (client does this in its WM_CREATE)
                WinSetWindowPtr(pGlobals->hwndFrame,
                                QWL_USER,
                                pXCenterData);

                // store client area;
                // we must use WinQueryWindowPos because WinQueryWindowRect
                // returns a 0,0,0,0 rectangle for invisible windows
                /* WinQueryWindowRect(pGlobals->hwndClient,
                                   &rclClient); */
                WinQueryWindowPos(pGlobals->hwndClient, &swpClient);
                pGlobals->cyTallestWidget = swpClient.cy;

                _Pmpf((__FUNCTION__": swpClient.cx: %d, swpClient.cy: %d", swpClient.cx, swpClient.cy));

                // add the use list item to the object's use list;
                // this struct has been zeroed above
                pXCenterData->UseItem.type    = USAGE_OPENVIEW;
                // pXCenterData->ViewItem.view   = pXCenterData->ulView;
                pXCenterData->ViewItem.handle = pGlobals->hwndFrame;
                if (!_wpAddToObjUseList(pXCenterData->somSelf,
                                        &(pXCenterData->UseItem)))
                    cmnLog(__FILE__, __LINE__, __FUNCTION__,
                           "_wpAddToObjUseList failed.");

                _wpRegisterView(pXCenterData->somSelf,
                                pGlobals->hwndFrame,
                                "XCenter"); // view title

                // subclass the frame AGAIN (the WPS has subclassed it
                // with wpRegisterView)
                pXCenterData->pfnwpFrameOrig = WinSubclassWindow(pGlobals->hwndFrame,
                                                                 fnwpXCenterMainFrame);
                if (pXCenterData->pfnwpFrameOrig)
                {
                    QMSG qmsg;

                    // subclassing succeeded:
                    CreateWidgets(pXCenterData);
                            // this sets the frame's size, invisibly

                    // register tooltip class
                    ctlRegisterTooltip(pGlobals->hab);
                    // create tooltip window
                    pXCenterData->hwndTooltip
                            = WinCreateWindow(HWND_DESKTOP,  // parent
                                              COMCTL_TOOLTIP_CLASS, // wnd class
                                              "",            // window text
                                              XWP_TOOLTIP_STYLE,
                                                   // tooltip window style (common.h)
                                              10, 10, 10, 10,    // window pos and size, ignored
                                              pGlobals->hwndClient,
                                                        // owner window -- important!
                                              HWND_TOP,      // hwndInsertBehind, ignored
                                              ID_XCENTER_TOOLTIP, // window ID, optional
                                              NULL,          // control data
                                              NULL);         // presparams
                                    // destroyed in WM_DESTROY of client

                    if (!pXCenterData->hwndTooltip)
                        cmnLog(__FILE__, __LINE__, __FUNCTION__,
                               "Cannot create tooltip.");
                    else
                    {
                        // tooltip successfully created:
                        // add tools (i.e. widgets)
                        // according to the class flags
                        PLISTNODE pNode;
                        TOOLINFO    ti = {0};
                        if (_ulPosition == XCENTER_BOTTOM)
                            ti.ulFlags = TTF_CENTERABOVE | TTF_SUBCLASS;
                        else
                            ti.ulFlags = TTF_CENTERBELOW | TTF_SUBCLASS;
                        ti.hwndToolOwner = pGlobals->hwndClient;
                        ti.pszText = PSZ_TEXTCALLBACK;  // send TTN_NEEDTEXT

                        // go thru all widgets and add them as tools
                        pNode = lstQueryFirstNode(&pXCenterData->llWidgets);
                        while (pNode)
                        {
                            PWIDGETVIEWSTATE pWidgetThis = (PWIDGETVIEWSTATE)pNode->pItemData;
                            if (pWidgetThis->Widget.ulClassFlags & WGTF_TOOLTIP)
                            {
                                // add tool to tooltip control
                                ti.hwndTool = pWidgetThis->Widget.hwndWidget;
                                WinSendMsg(pXCenterData->hwndTooltip,
                                           TTM_ADDTOOL,
                                           (MPARAM)0,
                                           &ti);
                            }
                            pNode = pNode->pNext;
                        }

                        // set timers
                        WinSendMsg(pXCenterData->hwndTooltip,
                                   TTM_SETDELAYTIME,
                                   (MPARAM)TTDT_AUTOPOP,
                                   (MPARAM)(40*1000));        // 40 secs for autopop (hide)
                    }

                    if (_ulWindowStyle & WS_ANIMATE)
                    {
                        // user wants animation:
                        // start timer on the frame which will unfold
                        // the XCenter from the left by repositioning it
                        // with each timer tick... when this is done,
                        // the next timer(s) will be started automatically
                        // until the frame is fully showing
                        tmrStartTimer(pGlobals->hwndFrame,
                                      TIMERID_UNFOLDFRAME,
                                      50);
                    }
                    else
                    {
                        // no animation:
                        ctrpReformat(pXCenterData,
                                     XFMF_RECALCHEIGHT
                                       | XFMF_REPOSITIONWIDGETS
                                       | XFMF_SHOWWIDGETS);
                                // frame is still hidden

                        WinSetWindowPos(pGlobals->hwndFrame,
                                        HWND_TOP,
                                        0,
                                        pXCenterData->yFrame,
                                        pXCenterData->cxFrame,
                                        pXCenterData->cyFrame,
                                        SWP_MOVE | SWP_SIZE | SWP_SHOW | SWP_ZORDER);
                    }

                    // now enter the message loop;
                    // fnwpXCenterMainFrame posts WM_QUIT on destroy
                    while (WinGetMsg(ptiMyself->hab, &qmsg, NULLHANDLE, 0, 0))
                        // loop until WM_QUIT
                        WinDispatchMsg(ptiMyself->hab, &qmsg);

                    // WM_QUIT came in: checks below...
                } // end if (pXCenterData->pfnwpFrameOrig)
            }
            CATCH(excpt1) {} END_CATCH();

            // We can get here for several reasons.
            // 1)   The user used the tasklist to kill the XCenter.
            //      Apparently, the task list posts WM_QUIT directly...
            //      so at this point, the XCenter might not have been
            //      destroyed.
            // 2)   By contrast, WM_DESTROY in fnwpFrame posts QM_QUIT
            //      too. At that point, the XCenter is already destroyed.
            // 3)   The XCenter crashed. At that point, the window still
            //      exists.
            // So check...
            if (_pvOpenView)
            {
                // not zeroed yet (zeroed by ClientDestroy):
                // this means the window still exists... clean up
                TRY_QUIET(excpt1)
                {
                    WinDestroyWindow(pGlobals->hwndFrame);
                    // ClientDestroy sets _pvOpenView to NULL
                }
                CATCH(excpt1) {} END_CATCH();
            }

        } // end if (fCreated)
    } // if (pXCenterData)

    // free(pXCenterOpenStruct);
}

/*
 *@@ ctrpCreateXCenterView:
 *      creates a new XCenter view on the screen.
 *      This is the implementation for XCenter::wpOpen.
 *
 *      To be precise, this starts a new thread for the
 *      XCenter view, since we want to allow the user to
 *      specify the XCenter's priority. The new thread is
 *      in ctrp_fntXCenter.
 */

HWND ctrpCreateXCenterView(XCenter *somSelf,
                           HAB hab,             // in: caller's anchor block
                           ULONG ulView,        // in: view (from wpOpen)
                           PVOID *ppvOpenView)  // out: ptr to PXCENTERWINDATA
{
    XCenterData *somThis = XCenterGetData(somSelf);
    HWND hwndFrame = NULLHANDLE;

    TRY_LOUD(excpt1)
    {
        if (!G_fXCenterClassRegistered)
        {
            // get window proc for WC_FRAME
            if (   (WinRegisterClass(hab,
                                     WC_XCENTER_CLIENT,
                                     fnwpXCenterMainClient,
                                     CS_PARENTCLIP | CS_CLIPCHILDREN | CS_SIZEREDRAW | CS_SYNCPAINT,
                                     sizeof(PXCENTERWINDATA))) // additional bytes to reserve
                && (RegisterBuiltInWidgets(hab))
               )
            {
                G_fXCenterClassRegistered = TRUE;
            }
            else
                cmnLog(__FILE__, __LINE__, __FUNCTION__,
                       "Error registering PM window classes.");
        }

        if (G_fXCenterClassRegistered)
        {
            PXCENTERWINDATA pXCenterData =
                (PXCENTERWINDATA)_wpAllocMem(somSelf,
                                             sizeof(XCENTERWINDATA),
                                             NULL);
            if (pXCenterData)
            {
                BOOL fRunning = FALSE;
                memset(pXCenterData, 0, sizeof(*pXCenterData));

                pXCenterData->cbSize = sizeof(*pXCenterData);

                pXCenterData->somSelf = somSelf;
                // pXCenterData->hab = hab;
                pXCenterData->ViewItem.view = ulView;

                DosCreateEventSem(NULL,     // unnamed
                                  &pXCenterData->hevRunning,
                                  0,        // unshared
                                  FALSE);   // not posted
                if (thrCreate(NULL,
                              ctrp_fntXCenter,
                              &fRunning,
                              THRF_TRANSIENT | THRF_PMMSGQUEUE | THRF_WAIT,
                              (ULONG)pXCenterData))
                {
                    // thread created:
                    // wait until it's created the XCenter frame
                    WinWaitEventSem(pXCenterData->hevRunning,
                                    10*1000);
                    hwndFrame = pXCenterData->Globals.hwndFrame;
                    if (hwndFrame)
                       *ppvOpenView = pXCenterData;
                }

                DosCloseEventSem(pXCenterData->hevRunning);
            }
        } // end if (G_fXCenterClassRegistered)
    }
    CATCH(excpt1) {} END_CATCH();

    return (hwndFrame);
}


