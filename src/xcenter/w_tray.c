
/*
 *@@sourcefile w_tray.c:
 *      XCenter "Tray" widget implementation. This is built
 *      into the XCenter and not in a plugin DLL.
 *
 *      The tray widget is quite special in some ways
 *      because it contains other widget as PM children.
 *      In other words, those "sub-widgets" are not children
 *      of the XCenter frame, but of the tray widget. This
 *      makes coordinate handling etc. a bit easier because
 *      we don't have to reformat every time the tray's
 *      position changes.
 *
 *      In addition, the tray widget tightly cooperates
 *      with the XCenter engine to maintain subwidget
 *      settings and views. Quite a number of changes had
 *      to be made to the engine to support this, but
 *      these should be transparent to all widget classes.
 *      ctrDefWidgetProc must now watch out for whether it's
 *      being called for a "regular" widget or a "sub-widget".
 *      In addition, some of the XCenter formatting routines
 *      have been split up to allow for code reuse from the
 *      tray widget, which needs to do the same thing as the
 *      XCenter client frequently.
 *
 *      Still, the tray widget is a regular widget and is
 *      treated as such by the XCenter formatting routines.
 *      The trick is simply that the tray widget will resize
 *      itself according to its member widgets.
 *
 *      The tray widget will allow other widgets inside
 *      itself if the widget class has the WGTF_TRAYABLE
 *      class setting. That setting has been added because
 *      there might be widget classes which simply assume
 *      that they are always direct children of the XCenter
 *      client. See XCENTERWIDGETCLASS for new requirements
 *      that a widget class must meet to be trayable.
 *
 *      Just like the XCenter, the tray differentiates between
 *      subwidget _settings_ and their views, which need not
 *      exist all the time. The tray creates the views only
 *      for widgets in the current tray. If the tray is changed,
 *      the old views are destroyed and new ones created. Not
 *      terribly fast, but saves memory.
 *
 *      The tray instance data structures are defined in
 *      centerp.h because the engine needs to access those
 *      as well.
 *
 *      Tray and subwidget settings are stored as a linked list
 *      together with the tray widget setting. For this to work,
 *      a new private layer around XCENTERWIDGETSETTING has been
 *      added (see PRIVATEWIDGETSETTING), which contains that
 *      linked list of tray and widget settings. "Normal" widgets
 *      need not know about this change.
 *
 *      Essentially, this works as follows:
 *
 +          XCENTERWIDGET for the tray widget
 +       contains
 +          --  pUser, a TRAYWIDGETPRIVATE which has the current
 +                     width and current tray of the tray widget
 +                     (saved as a regular setup string)
 +          --  pvWidgetSetting, which points to the
 +              PRIVATEWIDGETSETTING of the (tray) widget;
 +              this in turn has:
 +
 +              pllTraySettings
 +                 |
 +                 +--- TRAYSETTING
 +                 |    llSubwidgets
 +                 |         |
 +                 |         +--- TRAYSUBWIDGET
 +                 |         |        XCENTERWIDGETSETTING (subwidget)
 +                 |         |
 +                 |         +--- TRAYSUBWIDGET
 +                 |         |        XCENTERWIDGETSETTING (subwidget)
 +                 |         |
 +                 |         +--- TRAYSUBWIDGET
 +                 |                  XCENTERWIDGETSETTING (subwidget)
 +                 |
 +                 +--- TRAYSETTING
 +                      llSubwidgets ...
 +
 *      The trays and subwidgets are now directly saved with
 *      the other settings in the XCenter instance data.
 *      ctrpStuffSettings and ctrpUnstuffSettings have been
 *      extended to support this. For "normal" widgets, the
 *      pllTraySettings pointer in PRIVATEWIDGETSETTING is
 *      NULL.
 *
 *      Because the tray keeps creating and destroying windows,
 *      it is the sole responsibility of the tray widget to
 *      do so. ctrpDefWidgetProc will send messages here if
 *      this needs to be done for some other reason.
 *
 *      Function prefix for this file:
 *      --  Ywgt*
 *
 *      This is all new with V0.9.13.
 *
 *@@added V0.9.13 (2001-06-19) [umoeller]
 *@@header "shared\center.h"
 */

/*
 *      Copyright (C) 2000-2002 Ulrich M”ller.
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

#define INCL_DOSPROCESS
#define INCL_DOSEXCEPTIONS
#define INCL_DOSSEMAPHORES
#define INCL_DOSERRORS

#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WINMESSAGEMGR
#define INCL_WININPUT
#define INCL_WINMENUS
#define INCL_WINSYS
#define INCL_WINTIMER
#define INCL_WINPOINTERS

#define INCL_GPICONTROL
#define INCL_GPIPRIMITIVES
#define INCL_GPILOGCOLORTABLE
#define INCL_GPIREGIONS
#include <os2.h>

// C library headers
#include <stdio.h>
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\comctl.h"             // common controls (window procs)
#include "helpers\dialog.h"             // dialog helpers
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\gpih.h"               // GPI helper routines
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\winh.h"               // PM helper routines
#include "helpers\standards.h"          // some standard macros
#include "helpers\stringh.h"            // string helper routines
#include "helpers\threads.h"            // thread helpers
#include "helpers\xstring.h"            // extended string helpers

// SOM headers which don't crash with prec. header files
#include "xcenter.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\errors.h"              // private XWorkplace error codes
#include "shared\helppanels.h"          // all XWorkplace help panel IDs

#include "shared\center.h"              // public XCenter interfaces
#include "xcenter\centerp.h"            // private XCenter implementation

#pragma hdrstop                     // VAC++ keeps crashing otherwise

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

// moved these two from the private widget data to the
// globals here; also changed tray _icon_ to bitmap
// V0.9.16 (2001-10-28) [umoeller]
static HBITMAP      G_hbmTray = -1;     // tray icon
static LONG         G_cxTrayBmp = 0,
                    G_cyTrayBmp = 0;
static HPOINTER     G_hptrHand = -1;    // hand pointer

/* ******************************************************************
 *
 *   Private tray widget instance data
 *
 ********************************************************************/

/*
 *@@ TRAYSETUP:
 *      instance data to which setup strings correspond.
 *      This is also a member of TRAYWIDGETPRIVATE.
 *
 *@@changed V0.9.14 (2001-08-01) [umoeller]: moved ulCurrentTray to PRIVATEWIDGETSETTING
 */

typedef struct _TRAYSETUP
{
    ULONG           cx;     // widget width

    PPRIVATEWIDGETSETTING pPrivateSetting;
                        // ptr to tray widget's PRIVATEWIDGETSETTING,
                        // which has the ulCurrentTray member
                        // V0.9.14 (2001-08-01) [umoeller]

    // ULONG       ulCurrentTray;
                        // current tray index (from 0)
                        // or -1 if none

} TRAYSETUP, *PTRAYSETUP;

/*
 *@@ TRAYWIDGETPRIVATE:
 *      more window data for the traywidget.
 *
 *      An instance of this is created on WM_CREATE in
 *      fnwpPulseWidget and stored in XCENTERWIDGET.pUser.
 */

typedef struct _TRAYWIDGETPRIVATE
{
    PXCENTERWIDGET  pWidget;
            // reverse ptr to general widget data ptr; we need
            // that all the time and don't want to pass it on
            // the stack with each function call

    TRAYSETUP       Setup;
            // widget settings that correspond to a setup string

    PXCENTERWINDATA pXCenterData;
            // cached ptr to main XCenter win data, set up
            // in WM_CREATE
            // V0.9.19 (2002-04-25) [umoeller]

    BOOL            fMouseButton1Down,
                    fButtonSunk,
                    fMouseCaptured;

    HWND            hwndTraysMenu;

    SIZEL           szlCurrent;
            // our own current width height, which is the same as the
            // height of the tallest member widget

    BOOL            fContextMenuHacked;
            // TRUE after widget context menu has been modified
            // for the first time. This is necessary because
            // the context menu in XCENTERWIDGET is only valid
            // after WM_CREATE.

} TRAYWIDGETPRIVATE, *PTRAYWIDGETPRIVATE;

/* ******************************************************************
 *
 *   Subwidgets management
 *
 ********************************************************************/

/*
 *@@ DestroySubwidgetWindow:
 *      destroys the specified widget window.
 *      Does not remove the widget setting.
 *
 *      This is used when trays are switched and
 *      if a subwidget gets deleted by the user.
 */

static VOID DestroySubwidgetWindow(PTRAYWIDGETPRIVATE pPrivate,
                                   PLISTNODE pNode)    // in: PRIVATEWIDGETVIEW node from pPrivate->llWidgetViews
{
    // destroy the subwidget
    WinDestroyWindow(((PPRIVATEWIDGETVIEW)pNode->pItemData)->Widget.hwndWidget);
            // ctrDefWidgetProc also frees the PRIVATEWIDGETVIEW;
            // however, since this is a tray widget, it does
            // not remove the node from the list, so we must
            // do this manually
    // remove the subwidget's PRIVATEWIDGETVIEW node from the
    // tray widget's list of open widgets
    lstRemoveNode(((PPRIVATEWIDGETVIEW)pPrivate->pWidget)->pllSubwidgetViews,
                  pNode);
}

/*
 *@@ ReformatTray:
 *      reformats the entire tray. This is the exact
 *      equivalent to ctrpReformat for the entire
 *      XCenter client.
 */

static VOID ReformatTray(PTRAYWIDGETPRIVATE pPrivate)
{
    // this is the same sequence that ctrpReformat uses
    // for the entire XCenter client
    ULONG cyMax;
    PXCENTERWINDATA pXCenterData = pPrivate->pXCenterData;
    PPRIVATEWIDGETVIEW pTrayWidgetView = (PPRIVATEWIDGETVIEW)pPrivate->pWidget;

    // leftmost position of subwidgets:
    LONG xStart =   // pGlobals->cxMiniIcon
                    G_cxTrayBmp         // V0.9.16 (2001-10-28) [umoeller]
                  + 2
                  + 3;
            // tray button size plus an extra pixel
    if (0 == (pXCenterData->Globals.flDisplayStyle & XCS_FLATBUTTONS))
        xStart += 4;     // 2*2 for button borders

    // get widget sizes
    ctrpGetWidgetSizes(pTrayWidgetView->pllSubwidgetViews);

    // get tallest widget
    cyMax = ctrpQueryMaxWidgetCY(&pXCenterData->llWidgets);

    if (cyMax > pPrivate->szlCurrent.cy)
    {
        pPrivate->szlCurrent.cy = cyMax;
        // @@todo tell XCenter to grow vertically
    }
    else
        ctrpPositionWidgets(&pXCenterData->Globals,
                            pTrayWidgetView->pllSubwidgetViews,
                            xStart,
                            0,  // y
                            30, // cxPerGreedy
                            pPrivate->szlCurrent.cy, // height: same as ourselves
                            TRUE);      // show
}

/*
 *@@ FindCurrentTray:
 *
 */

static PTRAYSETTING FindCurrentTray(PTRAYWIDGETPRIVATE pPrivate)
{
    PPRIVATEWIDGETSETTING ppws = (PPRIVATEWIDGETSETTING)pPrivate->pWidget->pvWidgetSetting;

    if (pPrivate->Setup.pPrivateSetting->ulCurrentTray == -1)
        return (NULL);

    return (lstItemFromIndex(ppws->pllTraySettings,
                             pPrivate->Setup.pPrivateSetting->ulCurrentTray));
}

/*
 *@@ InvalidateMenu:
 *
 */

static VOID InvalidateMenu(PTRAYWIDGETPRIVATE pPrivate)
{
    // if we had built a tray menu before,
    // invalidate that
    if (pPrivate->hwndTraysMenu)
    {
        WinDestroyWindow(pPrivate->hwndTraysMenu);
        pPrivate->hwndTraysMenu = NULLHANDLE;
    }
}

/*
 *@@ SwitchToTray:
 *      switches to the specified tray. This destroys
 *      all current widget windows first, resets the
 *      current tray then, and creates new widgets for
 *      the new tray then.
 *
 *      If ulNewTray is -1, the current tray is
 *      set to -1 also and no widgets are created.
 *      This can also be used to quickly destroy
 *      the current subwidget views without creating
 *      new ones (e.g. in YwgtDestroy()).
 *
 *      This does not save the tray widget settings.
 *
 *      Returns TRUE if the tray was changed.
 *
 *@@changed V0.9.14 (2001-08-05) [lafaix]: check if subwidgets are trayable
 */

static BOOL SwitchToTray(PTRAYWIDGETPRIVATE pPrivate,
                         ULONG ulNewTray)      // in: new current tray or -1 for none
{
    if (pPrivate)
    {
        PLISTNODE pNode,
                  pNext;

        PXCENTERWIDGET pWidget = pPrivate->pWidget;
        PPRIVATEWIDGETSETTING ppws = (PPRIVATEWIDGETSETTING)pWidget->pvWidgetSetting;
        PTRAYSETTING pNewTray = NULL;

        _Pmpf((__FUNCTION__ ": old tray %d, new tray %d",
                    ppws->ulCurrentTray,
                    ulNewTray));

        if (
                // make sure the tray is really in the list
                (    (ulNewTray == -1)
                  || (pNewTray = (PTRAYSETTING)lstItemFromIndex(ppws->pllTraySettings,
                                                                ulNewTray))
                )
                // and the tray really changed
             && (ulNewTray != ppws->ulCurrentTray)
           )
        {
            // tray changed:
            PTRAYSETTING pCurrentTray;
            PPRIVATEWIDGETVIEW pTrayWidgetView = (PPRIVATEWIDGETVIEW)pPrivate->pWidget;
            if (pCurrentTray = FindCurrentTray(pPrivate))
            {
                // destroy all widget windows in the current tray
                pNode = lstQueryFirstNode(pTrayWidgetView->pllSubwidgetViews);
                while (pNode)
                {
                    pNext = pNode->pNext;
                    DestroySubwidgetWindow(pPrivate, pNode);
                    pNode = pNext;
                }
            }

            // switch trays then
            ppws->ulCurrentTray = ulNewTray;

            // create widget windows from the new tray, if any
            if (pNewTray)
            {
                pNode = lstQueryFirstNode(&pNewTray->llSubwidgetSettings);

                while (pNode)
                {
                    PPRIVATEWIDGETSETTING pSubwidget = (PPRIVATEWIDGETSETTING)pNode->pItemData;
                    PLISTNODE             pNodeSaved = pNode;
                    pNode = pNode->pNext;

                    // we must check if the subwidget class is trayable,
                    // as this may change
                    // V0.9.14 (2001-08-05) [lafaix]
                    if (!ctrpCheckClass(pSubwidget->Public.pszWidgetClass,
                                        TRUE))       // must be trayable
                    {
                        if (!ctrpCreateWidgetWindow(pPrivate->pXCenterData,
                                                    pWidget,
                                                         // parent: tray widget
                                                    pSubwidget,
                                                    -1))             // add rightmost
                            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                                   "Widget view creation failed.");
                    }
                    else
                    {
                        // remove:
                        ctrpFreeSettingData(&pSubwidget);
                                // V0.9.19 (2002-04-25) [umoeller]
                        lstRemoveNode(&pNewTray->llSubwidgetSettings,
                                      pNodeSaved);
                        _wpSaveDeferred(pPrivate->pXCenterData->somSelf);
                    }
                }

                // reformat widgets in tray then
                ReformatTray(pPrivate);

                // if we had built a tray menu before,
                // invalidate that
                InvalidateMenu(pPrivate);
            }
        }

        return (TRUE);
    }

    return (FALSE);
}

/*
 *@@ SetSubwidgetSize:
 *      implementation for XCM_SETWIDGETSIZE in
 *      fnwpTrayWidget.
 */

static BOOL SetSubwidgetSize(HWND hwnd,            // tray widget
                             HWND hwndWidget,      // subwidget
                             ULONG ulNewWidth)
{
    BOOL brc = FALSE;

    PXCENTERWIDGET pWidget;
    PTRAYWIDGETPRIVATE pPrivate;

    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (PTRAYWIDGETPRIVATE)pWidget->pUser)
       )
    {
        PPRIVATEWIDGETVIEW pTrayWidgetView = (PPRIVATEWIDGETVIEW)pWidget;
        PLISTNODE pNode = lstQueryFirstNode(pTrayWidgetView->pllSubwidgetViews);
        while (pNode)
        {
            PPRIVATEWIDGETVIEW pView = (PPRIVATEWIDGETVIEW)pNode->pItemData;
            if (pView->Widget.hwndWidget == hwndWidget)
            {
                // found it:
                // set new width
                pView->szlWanted.cx = ulNewWidth;

                ReformatTray(pPrivate);
                brc = TRUE;
                break;
            }

            pNode = pNode->pNext;
        }
    }

    return (brc);
}

/* ******************************************************************
 *
 *   Widget setup management
 *
 ********************************************************************/

/*
 *      This section contains shared code to manage the
 *      widget's settings. This can translate a widget
 *      setup string into the fields of a binary setup
 *      structure and vice versa. This code is used by
 *      both an open widget window and a settings dialog.
 */

/*
 *@@ YwgtClearSetup:
 *      cleans up the data in the specified setup
 *      structure, but does not free the structure
 *      itself.
 */

static VOID YwgtClearSetup(PTRAYSETUP pSetup)
{
}

/*
 *@@ YwgtScanSetup:
 *      scans the given setup string and translates
 *      its data into the specified binary setup
 *      structure.
 *
 *      NOTE: It is assumed that pSetup is zeroed
 *      out. We do not clean up previous data here.
 *
 *@@changed V0.9.14 (2001-08-01) [umoeller]: fixed a bad default setting
 */

static VOID YwgtScanSetup(PCSZ pcszSetupString,
                          PTRAYSETUP pSetup)
{
    PSZ p;

    // width
    if (p = ctrScanSetupString(pcszSetupString,
                               "WIDTH"))
    {
        pSetup->cx = atoi(p);
        ctrFreeSetupValue(p);
    }
    else
        pSetup->cx = 100;

    // current tray
    if (p = ctrScanSetupString(pcszSetupString,
                               "CURRENTTRAY"))
    {
        pSetup->pPrivateSetting->ulCurrentTray = atoi(p);
        ctrFreeSetupValue(p);
    }
    else
        pSetup->pPrivateSetting->ulCurrentTray = -1;        // fixed V0.9.14 (2001-08-01) [umoeller]

}

/*
 *@@ YwgtSaveSetup:
 *      composes a new setup string.
 *      The caller must invoke xstrClear on the
 *      string after use.
 */

static VOID YwgtSaveSetup(PXSTRING pstrSetup,       // out: setup string (is cleared first)
                          PTRAYSETUP pSetup)
{
    CHAR    szTemp[100];
    xstrInit(pstrSetup, 100);

    sprintf(szTemp, "WIDTH=%d;",
            pSetup->cx);
    xstrcat(pstrSetup, szTemp, 0);

    sprintf(szTemp, "CURRENTTRAY=%d;",
            pSetup->pPrivateSetting->ulCurrentTray);
    xstrcat(pstrSetup, szTemp, 0);
}

/*
 *@@ YwgtSaveSetupAndSend:
 *
 */

static VOID YwgtSaveSetupAndSend(PTRAYWIDGETPRIVATE pPrivate)
{
    PXCENTERWIDGET pWidget = pPrivate->pWidget;
    XSTRING strSetup;
    YwgtSaveSetup(&strSetup,
                  &pPrivate->Setup);
    if (strSetup.ulLength)
        WinSendMsg(pWidget->pGlobals->hwndClient,
                   XCM_SAVESETUP,
                   (MPARAM)pWidget->hwndWidget,
                   (MPARAM)strSetup.psz);
    xstrClear(&strSetup);
}

/* ******************************************************************
 *
 *   Public tray APIs
 *
 ********************************************************************/

/*
 *@@ YwgtCreateSubwidget:
 *      tray widget interface for creating a subwidget
 *      setting and a view at the same time. Also
 *      called from the XCenter engine during d'n'd.
 *
 *@@changed V0.9.19 (2002-05-04) [umoeller]: rewritten to use public method
 */

APIRET YwgtCreateSubwidget(PTRAYWIDGETPRIVATE pPrivate,
                           PCSZ pcszWidgetClass,
                           PCSZ pcszSetupString,
                           ULONG ulIndex)       // in: index or -1
{
    APIRET arc;
    XCenter *somSelf = pPrivate->pXCenterData->somSelf;

    WIDGETPOSITION pos;

    if (!(arc = ctrpQueryWidgetIndexFromHWND(somSelf,
                                             pPrivate->pWidget->hwndWidget,
                                             &pos)))
    {
        // ulWidgetIndex now has the index of the tray widget...
        pos.ulTrayWidgetIndex = pos.ulWidgetIndex;
        pos.ulTrayIndex = pPrivate->Setup.pPrivateSetting->ulCurrentTray;
        pos.ulWidgetIndex = ulIndex;

        arc = _xwpCreateWidget(somSelf,
                               (PSZ)pcszWidgetClass,
                               (PSZ)pcszSetupString,
                               &pos);
    }

    return arc;


    /*
    PPRIVATEWIDGETSETTING pSubwidget = NULL;
    PXCENTERWIDGET pWidget = pPrivate->pWidget;
    PPRIVATEWIDGETVIEW pTrayWidgetView = (PPRIVATEWIDGETVIEW)pWidget;

    PTRAYSETTING    pCurrentTray;

    if (pCurrentTray = FindCurrentTray(pPrivate))
    {
        if (!ctrpCreateWidgetSetting(pPrivate->pXCenterData->somSelf,
                                     pCurrentTray,
                                     pcszWidgetClass,
                                     pcszSetupString,
                                     ulIndex,
                                     &pSubwidget,
                                     NULL,
                                     NULL))
        {
            // create widget window
            if (!ctrpCreateWidgetWindow(pPrivate->pXCenterData,
                                        pWidget,
                                             // parent: tray widget
                                        pSubwidget,
                                        ulIndex))
                cmnLog(__FILE__, __LINE__, __FUNCTION__,
                       "Subwidget view creation failed.");

            // reformat widgets in tray then
            ReformatTray(pPrivate);

            // recompose setup string with new trays
            YwgtSaveSetupAndSend(pPrivate);
        }
    }

    return (pSubwidget);
    */
}

/* ******************************************************************
 *
 *   Widget settings dialog
 *
 ********************************************************************/

// None currently.

// VOID EXPENTRY YwgtShowSettingsDlg(PWIDGETSETTINGSDLGDATA pData)

/* ******************************************************************
 *
 *   PM window class implementation
 *
 ********************************************************************/

/*
 *@@ YwgtCreate:
 *      implementation for WM_CREATE in fnwpTrayWidget.
 *
 *@@changed V0.9.16 (2001-10-28) [umoeller]: turned tray icon into bitmap and made it vertical
 */

static MRESULT YwgtCreate(HWND hwnd, MPARAM mp1)
{
    MRESULT mrc = 0;        // continue window creation

    HMODULE hmodRes = cmnQueryMainResModuleHandle();

    ULONG ulSwitchTo;

    PPRIVATEWIDGETSETTING pPrivateSetting;

    PXCENTERWIDGET pWidget = (PXCENTERWIDGET)mp1;
    PPRIVATEWIDGETVIEW pTrayWidgetView = (PPRIVATEWIDGETVIEW)pWidget;
    PTRAYWIDGETPRIVATE pPrivate = malloc(sizeof(TRAYWIDGETPRIVATE));
    memset(pPrivate, 0, sizeof(TRAYWIDGETPRIVATE));
    // link the two together
    pWidget->pUser = pPrivate;
    pPrivate->pWidget = pWidget;

    // here comes the very special code for the tray widget...
    // ctrpCreateWidgetWindow has been adjusted for this
    // to set a special new field in XCENTERWIDGET which
    // points to the PRIVATEWIDGETSETTING so that we can
    // access the linked list of subwidget settings in the
    // tray widget
    pPrivateSetting
    = pPrivate->Setup.pPrivateSetting
        = (PPRIVATEWIDGETSETTING)pWidget->pvWidgetSetting;

    pPrivate->pXCenterData = WinQueryWindowPtr(pWidget->pGlobals->hwndFrame, QWL_USER);
            // V0.9.19 (2002-04-25) [umoeller]

    // create the list of current widget views in the
    // tray widget's PRIVATEWIDGETVIEW...
    pTrayWidgetView->pllSubwidgetViews = lstCreate(FALSE);

    YwgtScanSetup(pWidget->pcszSetupString,
                  &pPrivate->Setup);

    // save current tray, can be -1
    ulSwitchTo = pPrivateSetting->ulCurrentTray;
    // fake current tray so that switch to will work below
    pPrivateSetting->ulCurrentTray = -1;

    // if the list has not been created yet by ctrpUnstuffSettings,
    // create one now
    if (!pPrivateSetting->pllTraySettings)
    {
        PTRAYSETTING pTray;
        CHAR sz[200];
        pPrivateSetting->pllTraySettings = lstCreate(FALSE);
        // create a default tray
        sprintf(sz,
                cmnGetString(ID_CRSI_TRAY),     // tray %d
                1);
        pTray = ctrpCreateTraySetting(pPrivateSetting,
                                      sz,
                                      NULL);
        // switch to tray 0 after all this
        ulSwitchTo = 0;
    }

    if (ulSwitchTo == -1)
        ulSwitchTo = 0;
        // V0.9.19 (2002-04-25) [umoeller]

    // enable context menu help
    pWidget->pcszHelpLibrary = cmnQueryHelpLibrary();
    pWidget->ulHelpPanelID = ID_XSH_WIDGET_TRAY;

    // load tray bitmap V0.9.16 (2001-10-28) [umoeller]
    if (G_hbmTray == -1)
    {
        HPS hps = WinGetPS(hwnd);
        if (!(G_hbmTray = GpiLoadBitmap(hps,
                                        hmodRes,
                                        ID_BITMAP_TRAY,
                                        0,
                                        0)))
            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                   "Cannot load tray bitmap");
        else
        {
            BITMAPINFOHEADER2 bmih2;
            bmih2.cbFix = sizeof(bmih2);
            GpiQueryBitmapInfoHeader(G_hbmTray,
                                     &bmih2);
            G_cxTrayBmp = bmih2.cx;
            G_cyTrayBmp = bmih2.cy;
        }
        WinReleasePS(hps);
    }
    // load tray icon V0.9.16 (2001-10-28) [umoeller]
    if (G_hptrHand == -1)
        G_hptrHand = WinLoadPointer(HWND_DESKTOP,
                                    hmodRes,
                                    ID_POINTER_HAND);

    WinPostMsg(hwnd,
               XCM_SWITCHTOTRAY,
               (MPARAM)ulSwitchTo,      // 0 if just created, otherwise the saved one
               0);

    return (mrc);
}

/*
 *@@ YwgtControl:
 *      implementation for WM_CONTROL in fnwpTrayWidget.
 *
 */

static BOOL YwgtControl(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    BOOL brc = FALSE;

    PXCENTERWIDGET pWidget;
    PTRAYWIDGETPRIVATE pPrivate;

    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (PTRAYWIDGETPRIVATE)pWidget->pUser)
       )
    {
        PPRIVATEWIDGETVIEW pTrayWidgetView = (PPRIVATEWIDGETVIEW)pWidget;

        USHORT  usID = SHORT1FROMMP(mp1),
                usNotifyCode = SHORT2FROMMP(mp1);

        switch (usID)
        {
            case ID_XCENTER_CLIENT:
                switch (usNotifyCode)
                {
                    /*
                     * XN_QUERYSIZE:
                     *      XCenter wants to know our size.
                     */

                    case XN_QUERYSIZE:
                    {
                        PSIZEL pszl = (PSIZEL)mp2;
                        pszl->cx = pPrivate->Setup.cx;
                        pszl->cy = 10;

                        brc = TRUE;
                    }
                    break;

                    /*
                     * XN_DISPLAYSTYLECHANGED:
                     *      re-broadcast this to our subwidgets
                     */

                    case XN_DISPLAYSTYLECHANGED:
                    {
                        // reformat widgets; button size might
                        // have changed
                        ReformatTray(pPrivate);

                        // and re-broadcast this to our subwidgets
                        ctrpBroadcastWidgetNotify(pTrayWidgetView->pllSubwidgetViews,
                                                  usNotifyCode,
                                                  mp2);
                    }
                    break;

                    case XN_BEGINANIMATE:
                    case XN_ENDANIMATE:
                        // re-broadcast this to our subwidgets
                        // V0.9.13 (2001-06-23) [umoeller]
                        ctrpBroadcastWidgetNotify(pTrayWidgetView->pllSubwidgetViews,
                                                  usNotifyCode,
                                                  mp2);
                    break;
                }
            break;

            case ID_XCENTER_TOOLTIP:
                if (usNotifyCode == TTN_NEEDTEXT)
                {
                    PTOOLTIPTEXT pttt = (PTOOLTIPTEXT)mp2;
                    PTRAYSETTING pCurrentTray;

                    if (    (pPrivate->Setup.pPrivateSetting->ulCurrentTray != -1)
                         && (pCurrentTray = FindCurrentTray(pPrivate))
                       )
                    {
                        pttt->pszText = pCurrentTray->pszTrayName;
                    }
                    else
                        pttt->pszText = cmnGetString(ID_CRSI_NOTRAYACTIVE);

                    pttt->ulFormat = TTFMT_PSZ;
                }
            break;
        }
    } // end if (pWidget)

    return (brc);
}

/*
 *@@ YwgtPaint:
 *      implementation for WM_PAINT in fnwpTrayWidget.
 */

static VOID YwgtPaint(HWND hwnd)
{
    PXCENTERWIDGET pWidget;
    PTRAYWIDGETPRIVATE pPrivate;

    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (PTRAYWIDGETPRIVATE)pWidget->pUser)
       )
    {
        RECTL           rclWin;
        // CHAR            sz[100];
        const XCENTERGLOBALS *pGlobals = pWidget->pGlobals;
        XBUTTONDATA     xbd;
        ULONG           fl = XBF_BITMAP;        // V0.9.16 (2001-10-28) [umoeller]

        HPS hps = WinBeginPaint(hwnd, NULLHANDLE, NULL);
        gpihSwitchToRGB(hps);

        WinQueryWindowRect(hwnd, &rclWin);

        // fill entire widget background (more than just button
        WinFillRect(hps,
                    &rclWin,
                    pWidget->pGlobals->lcolClientBackground);

        // draw icon as X-button
        xbd.rcl.xLeft = 0;
        xbd.rcl.yBottom = 0;
        xbd.rcl.yTop = rclWin.yTop;
        xbd.rcl.xRight =   // pGlobals->cxMiniIcon
                           G_cxTrayBmp      // V0.9.16 (2001-10-28) [umoeller]
                         + 2
                         + 2;
        if (!(pGlobals->flDisplayStyle & XCS_FLATBUTTONS))
            xbd.rcl.xRight += 4;     // 2*2 for button borders
        else
            // flat buttons:
            fl |= XBF_FLAT;
        if (    (pPrivate->fMouseButton1Down)
             || (pPrivate->fButtonSunk)
           )
            fl |= XBF_PRESSED;

        xbd.cxIconOrBitmap = G_cxTrayBmp;
        xbd.cyIconOrBitmap = G_cyTrayBmp;           // V0.9.16 (2001-10-28) [umoeller]
        xbd.lcol3DDark = pGlobals->lcol3DDark;
        xbd.lcol3DLight = pGlobals->lcol3DLight;
        xbd.lMiddle = pGlobals->lcolClientBackground; // WinQuerySysColor(HWND_DESKTOP, SYSCLR_BUTTONMIDDLE, 0);

        xbd.hptr = G_hbmTray;           // V0.9.16 (2001-10-28) [umoeller]

        ctlPaintXButton(hps,
                        fl,         // note, XBD_BACKGROUND is never set
                        &xbd);

        WinEndPaint(hps);
    } // end if (pWidget && pPrivate)
}

/*
 *@@ YwgtWindowPosChanged:
 *      implementation for WM_WINDOWPOSCHANGED in fnwpTrayWidget.
 *
 */

static VOID YwgtWindowPosChanged(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    PXCENTERWIDGET pWidget;
    PTRAYWIDGETPRIVATE pPrivate;

    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (PTRAYWIDGETPRIVATE)pWidget->pUser)
       )
    {
        PSWP pswpNew = (PSWP)mp1,
             pswpOld = pswpNew + 1;
        if (pswpNew->fl & SWP_SIZE)
        {
            // window was resized:
            if (pswpNew->cx != pswpOld->cx)
            {
                // width changed:
                pPrivate->Setup.cx = pswpNew->cx;
                pPrivate->szlCurrent.cx = pswpNew->cx;
                // recompose setup string
                YwgtSaveSetupAndSend(pPrivate);
            }

            if (pswpNew->cy != pswpOld->cy)
            {
                // height changed (probably by XCenter):
                // reformat subwidgets
                pPrivate->szlCurrent.cy = pswpNew->cy;
                        // this will then be used for resizing the subwidgets too
                ReformatTray(pPrivate);
            }
        } // end if (pswpNew->fl & SWP_SIZE)
    } // end if (pWidget && pPrivate)
}

/*
 *@@ IsClickOnTrayButton:
 *
 *@@added V0.9.19 (2002-04-25) [umoeller]
 */

static BOOL IsClickOnTrayButton(const XCENTERGLOBALS *pGlobals,
                                MPARAM mp1)
{
    SHORT sx = SHORT1FROMMP(mp1);
    SHORT sComp = pGlobals->cxMiniIcon + 2;
    if (0 == (pGlobals->flDisplayStyle & XCS_FLATBUTTONS))
        sComp += 4;     // 2*2 for button borders
    return (sx < sComp);
}

/*
 * YwgtButton1Down:
 *      implementation for WM_BUTTON1DOWN in fnwpTrayWidget.
 *
 *@@changed V0.9.14 (2001-07-31) [lafaix]: fixed menu position
 *@@changed V0.9.14 (2001-08-05) [lafaix]: added MB1 click tray switch
 */

static VOID YwgtButton1Down(HWND hwnd, MPARAM mp1)
{
    PXCENTERWIDGET pWidget;
    PTRAYWIDGETPRIVATE pPrivate;
    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (PTRAYWIDGETPRIVATE)pWidget->pUser)
       )
    {
        const XCENTERGLOBALS *pGlobals = pWidget->pGlobals;
        SHORT sx = SHORT1FROMMP(mp1);
        if (IsClickOnTrayButton(pGlobals, mp1))
        {
            // click on tray button:
            if (WinIsWindowEnabled(hwnd))
            {
                pPrivate->fMouseButton1Down = TRUE;
                WinInvalidateRect(hwnd, NULL, FALSE);

                // since we're not passing the message
                // to WinDefWndProc, we need to give
                // ourselves the focus; this will also
                // dismiss the button's menu, if open
                WinSetFocus(HWND_DESKTOP, hwnd);

                if (!pPrivate->fMouseCaptured)
                {
                    // capture mouse events while the
                    // mouse button is down
                    WinSetCapture(HWND_DESKTOP, hwnd);
                    pPrivate->fMouseCaptured = TRUE;
                }

                if (!pPrivate->fButtonSunk)
                {
                    // toggle state is still UP (i.e. button pressed
                    // for the first time): show menu

                    if (!pPrivate->hwndTraysMenu)
                    {
                        // first call, or menu was invalidated:
                        // built it
                        SHORT sIndex = 100;
                        PLISTNODE pNode;
                        PPRIVATEWIDGETSETTING ppws = (PPRIVATEWIDGETSETTING)pPrivate->pWidget->pvWidgetSetting;
                        ULONG ulIndex = 0;

                        pPrivate->hwndTraysMenu = WinCreateMenu(HWND_DESKTOP, NULL);
                        for (pNode = lstQueryFirstNode(ppws->pllTraySettings);
                             pNode;
                             pNode = pNode->pNext, ulIndex++)
                        {
                            PTRAYSETTING pTray = (PTRAYSETTING)pNode->pItemData;
                            winhInsertMenuItem(pPrivate->hwndTraysMenu,
                                               MIT_END,
                                               sIndex++,
                                               pTray->pszTrayName,
                                               MIS_TEXT,
                                               (pPrivate->Setup.pPrivateSetting->ulCurrentTray == ulIndex)
                                                    ? MIA_CHECKED
                                                    : 0);
                        }
                    }

                    if (pPrivate->hwndTraysMenu)
                    {
                        // draw source emphasis around widget
                        ctrpDrawEmphasis(pPrivate->pXCenterData,
                                         FALSE,     // draw, not remove emphasis
                                         NULL,
                                         hwnd,
                                         NULLHANDLE);   // standard PS

                        // V0.9.14 (2001-07-31) [lafaix]
                        ctrPlaceAndPopupMenu(hwnd,
                                             pPrivate->hwndTraysMenu,
                                             pWidget->pGlobals->ulPosition == XCENTER_BOTTOM);
                    }
                } // end if (!pPrivate->fButtonSunk)
            }
        }
    } // end if (pWidget)
}

/*
 *@@ YwgtButton1Click:
 *
 *@@added V0.9.14 (2001-08-07) [umoeller]
 *@@changed V0.9.19 (2002-04-25) [umoeller]: now allowing single-click for switching trays
 */

static VOID YwgtButton1Click(HWND hwnd,
                             ULONG msg,
                             MPARAM mp1)
{
    PXCENTERWIDGET pWidget;
    PTRAYWIDGETPRIVATE pPrivate;
    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (PTRAYWIDGETPRIVATE)pWidget->pUser)
       )
    {
        BOOL fButton = IsClickOnTrayButton(pWidget->pGlobals, mp1);

        if (    ((fButton) && (msg == WM_BUTTON1DBLCLK))
             || ((!fButton) && (msg == WM_BUTTON1CLICK))
           )
        {
            // circle through the trays
            PPRIVATEWIDGETSETTING ppws = (PPRIVATEWIDGETSETTING)pPrivate->pWidget->pvWidgetSetting;
            ULONG cTrays = lstCountItems(ppws->pllTraySettings);
            if (cTrays > 1)
                SwitchToTray(pPrivate,
                             ((pPrivate->Setup.pPrivateSetting->ulCurrentTray) + 1) % cTrays);
        }
    }
}

/*
 * YwgtButton1Up:
 *      implementation for WM_BUTTON1UP in fnwpTrayWidget.
 */

static VOID YwgtButton1Up(HWND hwnd)
{
    PXCENTERWIDGET pWidget;
    PTRAYWIDGETPRIVATE pPrivate;
    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (PTRAYWIDGETPRIVATE)pWidget->pUser)
       )
    {
        if (pPrivate->fMouseButton1Down)
        {
            // un-capture the mouse first
            if (pPrivate->fMouseCaptured)
            {
                WinSetCapture(HWND_DESKTOP, NULLHANDLE);
                pPrivate->fMouseCaptured = FALSE;
            }

            pPrivate->fMouseButton1Down = FALSE;

            // toggle state with each WM_BUTTON1UP
            pPrivate->fButtonSunk = !pPrivate->fButtonSunk;

            // repaint sunk button state
            WinInvalidateRect(hwnd, NULL, FALSE);
        }
    } // end if (pWidget)
}

/*
 * YwgtMenuEnd:
 *      implementation for WM_MENUEND in fnwpTrayWidget.
 */

static MRESULT YwgtMenuEnd(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    PXCENTERWIDGET pWidget;
    PTRAYWIDGETPRIVATE pPrivate;
    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (PTRAYWIDGETPRIVATE)pWidget->pUser)
       )
    {
        if ((HWND)mp2 == pPrivate->hwndTraysMenu)
        {
            // main menu is ending:
            if (!pPrivate->fMouseButton1Down)
            {
                // mouse button not currently down
                // --> menu dismissed for some other reason:
                pPrivate->fButtonSunk = FALSE;
                WinInvalidateRect(hwnd, NULL, FALSE);

                // and remove emphasis again
                WinInvalidateRect(WinQueryWindow(hwnd, QW_PARENT),
                                  NULL,
                                  FALSE);
            }
        }
    } // end if (pWidget)

    return (ctrDefWidgetProc(hwnd, WM_MENUEND, mp1, mp2));
}

/*
 *@@ YwgtContextMenu:
 *      implementation for WM_CONTEXTMENU in  in fnwpTrayWidget.
 *
 *@@changed V0.9.16 (2001-12-08) [umoeller]: "delete" wasn't disabled if there was only one tray
 */

static MRESULT YwgtContextMenu(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    PXCENTERWIDGET pWidget;
    PTRAYWIDGETPRIVATE pPrivate;

    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (PTRAYWIDGETPRIVATE)pWidget->pUser)
       )
    {
        PPRIVATEWIDGETSETTING ppws = (PPRIVATEWIDGETSETTING)pPrivate->pWidget->pvWidgetSetting;
        HWND hwndContextMenu = pWidget->hwndContextMenu;
        BOOL fTrayValid;

        if (!pPrivate->fContextMenuHacked)
        {
            // first call:
            SHORT sPosition = 1;

            // hack the context menu so we have "add widgets"
            // with all the widget classes we can handle

            winhInsertMenuSeparator(hwndContextMenu,
                                    sPosition++,  // position
                                    ID_CRMI_SEP1);

            winhInsertMenuItem(hwndContextMenu,
                               sPosition++,
                               ID_CRMI_ADDTRAY,
                               cmnGetString(ID_CRSI_ADDTRAY),
                               MIS_TEXT,
                               0);
            winhInsertMenuItem(hwndContextMenu,
                               sPosition++,
                               ID_CRMI_RENAMETRAY,
                               cmnGetString(ID_CRSI_RENAMETRAY),
                               MIS_TEXT,
                               0);
            winhInsertMenuItem(hwndContextMenu,
                               sPosition++,
                               ID_CRMI_REMOVETRAY,
                               cmnGetString(ID_CRSI_REMOVETRAY),
                               MIS_TEXT,
                               0);

            winhInsertMenuSeparator(hwndContextMenu,
                                    sPosition++,  // position
                                    ID_CRMI_SEP1);

            ctrpAddWidgetsMenu(pWidget->pGlobals->pvSomSelf,
                               hwndContextMenu,
                               sPosition++,       // position
                               TRUE);   // trayable only

            pPrivate->fContextMenuHacked = TRUE;
        }

        fTrayValid = (pPrivate->Setup.pPrivateSetting->ulCurrentTray != -1);

        // allow "delete tray" only if we have more than one
        WinEnableMenuItem(hwndContextMenu,
                          ID_CRMI_REMOVETRAY,
                          (    (fTrayValid)
                            && (lstCountItems(ppws->pllTraySettings) > 1)
                          ));
        WinEnableMenuItem(hwndContextMenu,
                          ID_CRMI_RENAMETRAY,
                          (fTrayValid));
        /* WinEnableMenuItem(hwndContextMenu,
                          ID_CRMI_REMOVETRAY,
                          (fTrayValid));
            */      // wow, what was this doing in here?
                    // V0.9.16 (2001-12-08) [umoeller]
    }

    // let default widget proc display context menu
    return (ctrDefWidgetProc(hwnd,
                             WM_CONTEXTMENU,
                             mp1,
                             mp2));
}

/*
 *@@ YwgtCommand:
 *      implementation for WM_COMMAND in fnwpTrayWidget.
 *
 *      Besides the fixed menu item IDs, this can have:
 *
 *      --  >= 100: from the "trays" menu.
 *
 *      --  >= sulVarMenuItems: "add widget" menu.
 */

static MRESULT YwgtCommand(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    PXCENTERWIDGET pWidget;
    PTRAYWIDGETPRIVATE pPrivate;

    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (PTRAYWIDGETPRIVATE)pWidget->pUser)
       )
    {
        const XCENTERGLOBALS *pGlobals = pWidget->pGlobals;
        USHORT usCommand = SHORT1FROMMP(mp1);
        PPRIVATEWIDGETSETTING ppws = (PPRIVATEWIDGETSETTING)pPrivate->pWidget->pvWidgetSetting;
        PPRIVATEWIDGETCLASS pClass;
        PTRAYSETTING pCurrentTray;

        XCenter *somSelf = pPrivate->pXCenterData->somSelf;
        WIDGETPOSITION pos;

        ctrpQueryWidgetIndexFromHWND(somSelf,
                                     hwnd,
                                     &pos);

        switch (usCommand)
        {
            case ID_CRMI_ADDTRAY:
            {
                CHAR szTemp[30];
                PSZ pszNewTrayName;
                sprintf(szTemp,
                        cmnGetString(ID_CRSI_TRAY),     // tray %d
                        lstCountItems(ppws->pllTraySettings) + 1);
                if (pszNewTrayName = cmnTextEntryBox(pGlobals->hwndClient,
                                                     cmnGetString(ID_CRSI_ADDTRAY),
                                                     cmnGetString(ID_CRSI_ENTERNEWTRAY),
                                                     szTemp,
                                                     50,
                                                     TEBF_REMOVETILDE
                                                        | TEBF_REMOVEELLIPSE
                                                        | TEBF_SELECTALL))
                {
                    // create a tray with that name
                    ULONG ulIndex = 0;
                    PTRAYSETTING pTray = ctrpCreateTraySetting((PPRIVATEWIDGETSETTING)pWidget->pvWidgetSetting,
                                                               pszNewTrayName,
                                                               &ulIndex);
                    InvalidateMenu(pPrivate);
                    SwitchToTray(pPrivate,
                                 ulIndex);
                    free(pszNewTrayName);

                    // recompose setup string with new trays
                    YwgtSaveSetupAndSend(pPrivate);
                }
            }
            break;

            case ID_CRMI_RENAMETRAY:
                if (pCurrentTray = FindCurrentTray(pPrivate))
                {
                    PSZ pszNewTrayName;
                    if (pszNewTrayName = cmnTextEntryBox(pGlobals->hwndClient,
                                                         cmnGetString(ID_CRSI_RENAMETRAY),
                                                         cmnGetString(ID_CRSI_ENTERRENAMETRAY),
                                                         pCurrentTray->pszTrayName,
                                                         50,
                                                         TEBF_REMOVETILDE
                                                            | TEBF_REMOVEELLIPSE
                                                            | TEBF_SELECTALL))
                    {
                        _xwpRenameTray(somSelf,
                                       pos.ulWidgetIndex,
                                       ppws->ulCurrentTray,
                                       pszNewTrayName);
                        free(pszNewTrayName);
                    }
                }
            break;

            case ID_CRMI_REMOVETRAY:
                if (pCurrentTray = FindCurrentTray(pPrivate))
                {
                    PCSZ apsz = pCurrentTray->pszTrayName;
                    if (cmnMessageBoxExt(pGlobals->hwndClient,
                                         220,        // delete tray
                                         &apsz,
                                         1,
                                         221,        // delete tray %1?
                                         MB_YESNO)
                            == MBID_YES)
                    {
                        // alright, nuke it
                        // PLISTNODE pNode;
                        ULONG ulIndex = pPrivate->Setup.pPrivateSetting->ulCurrentTray;
                        // switch to "no tray" first
                        // (this destroys the subwidget windows)
                        SwitchToTray(pPrivate,
                                     -1);         // new tray: none
                        ctrpDeleteTraySetting(somSelf,
                                              (PPRIVATEWIDGETSETTING)pWidget->pvWidgetSetting,
                                              ulIndex);
                        // try to switch to first tray on list
                        SwitchToTray(pPrivate,
                                     0);

                        // recompose setup string with new trays
                        YwgtSaveSetupAndSend(pPrivate);
                    }
                }
            break;

            default:
                // is it one of the items in the "trays" menu?
                if (    (usCommand >= 100)
                     && (usCommand < lstCountItems(ppws->pllTraySettings) + 100)
                   )
                {
                    // yes: switch to tray
                    SwitchToTray(pPrivate,
                                 usCommand - 100);

                    // recompose setup string with new trays
                    YwgtSaveSetupAndSend(pPrivate);

                    return ((MRESULT)TRUE);
                }
                // is it one of the items in the "add widget" submenu?
                else if (pClass = ctrpFindClassFromMenuCommand(usCommand))
                {
                    YwgtCreateSubwidget(pPrivate,
                                        pClass->Public.pcszWidgetClass,
                                        NULL,       // setup string
                                        -1);        // add rightmost

                    // say "processed", do not call def. widget proc
                    return ((MRESULT)TRUE);
                }
                else
                    return (ctrDefWidgetProc(hwnd,
                                             WM_COMMAND,
                                             mp1,
                                             mp2));
        }
    }

    return (FALSE);
}

/*
 *@@ YwgtDragOver:
 *      implementation for DM_DRAGOVER in fnwpTrayWidget.
 *
 */

static MRESULT YwgtDragOver(HWND hwnd, MPARAM mp1)
{
    PXCENTERWIDGET pWidget;
    if (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
    {
        return (ctrpDragOver(pWidget->pGlobals->hwndClient,
                             hwnd,        // tray window
                             (PDRAGINFO)mp1));
    }

    return (0);
}

/*
 *@@ YwgtDragLeave:
 *      implementation for DM_DRAGLEAVE in fnwpTrayWidget.
 *
 */

static VOID YwgtDragLeave(HWND hwnd)
{
    PXCENTERWIDGET pWidget;
    if (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
    {
        ctrpRemoveDragoverEmphasis(pWidget->pGlobals->hwndClient);
    }
}

/*
 *@@ YwgtDrop:
 *      implementation for DM_DROP in fnwpTrayWidget.
 *
 */

static VOID YwgtDrop(HWND hwnd, MPARAM mp1)
{
    PXCENTERWIDGET pWidget;
    if (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
    {
        ctrpDrop(pWidget->pGlobals->hwndClient,
                 hwnd,        // tray window
                 (PDRAGINFO)mp1);
    }
}


/*
 *@@ YwgtSaveSubwidgetSetup:
 *      implementation for XCM_SAVESETUP in fnwpTrayWidget.
 *
 *      This gets sent to us from a subwidget. We
 *      must then update our own subwidget settings
 *      list and notify the XCenter client in turn
 *      to save the entire XCenter setup.
 */

static BOOL YwgtSaveSubwidgetSetup(HWND hwnd,
                                   HWND hwndSubwidget,
                                   PCSZ pcszSetupString)     // can be NULL
{
    PXCENTERWIDGET pWidget;
    PTRAYWIDGETPRIVATE pPrivate;

    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (PTRAYWIDGETPRIVATE)pWidget->pUser)
       )
    {
        PPRIVATEWIDGETVIEW pTrayWidgetView = (PPRIVATEWIDGETVIEW)pWidget;

        // 1) find the subwidget in the subwidgets list
        PLISTNODE pNode;
        ULONG ulSubwidgetIndex = 0;
        // PPRIVATEWIDGETSETTING ppws = (PPRIVATEWIDGETSETTING)pPrivate->pWidget->pvWidgetSetting;

        for (pNode = lstQueryFirstNode(pTrayWidgetView->pllSubwidgetViews);
             pNode;
             pNode = pNode->pNext, ulSubwidgetIndex++)
        {
            PPRIVATEWIDGETVIEW pView = (PPRIVATEWIDGETVIEW)pNode->pItemData;
            if (pView->Widget.hwndWidget == hwndSubwidget)
            {
                // that's ours:
                // find the corresponding setting entry
                PTRAYSETTING pCurrentTray;
                PPRIVATEWIDGETSETTING pSubwidget;

                if (    (pCurrentTray = FindCurrentTray(pPrivate))
                     && (pSubwidget = lstItemFromIndex(&pCurrentTray->llSubwidgetSettings,
                                                       ulSubwidgetIndex))
                   )
                {
                    // got it:
                    // update the setup string
                    if (pSubwidget->Public.pszSetupString)
                        free(pSubwidget->Public.pszSetupString);
                    pSubwidget->Public.pszSetupString = strhdup(pcszSetupString, NULL);

                    // now recompose our own setup string, which
                    // will include the subwidget's new setup string,
                    // and save it with the XCenter... duh
                    YwgtSaveSetupAndSend(pPrivate);

                    return (TRUE);
                }
                else
                    cmnLog(__FILE__, __LINE__, __FUNCTION__,
                           "Found subwidget 0x%lX in views list, but not in settings.",
                           hwndSubwidget);

                break;
            }
        }
    }

    return (FALSE);
}

/*
 * YwgtRemoveSubwidget:
 *      implementation for XCM_REMOVESUBWIDGET in fnwpTrayWidget.
 *      This is a private message sent from ctrDefWidgetProc
 *      when the ID_CRMI_REMOVEWGT menu item is
 *      received from a subwidget. We then destroy
 *      that and update our lists.
 *
 * removed V0.9.19 (2002-05-04) [umoeller]
 */

/*
static MRESULT YwgtRemoveSubwidget(HWND hwnd,
                                   PPRIVATEWIDGETVIEW pWidget2Remove)
{
    // mp1 has the PRIVATEWIDGETVIEW to be destroyed:
    // search our list for that
    PXCENTERWIDGET pWidget;
    PTRAYWIDGETPRIVATE pPrivate;

    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (PTRAYWIDGETPRIVATE)pWidget->pUser)
         && (pPrivate->Setup.pPrivateSetting->ulCurrentTray != -1)
       )
    {
        PPRIVATEWIDGETVIEW pTrayWidgetView = (PPRIVATEWIDGETVIEW)pWidget;

        // step 1: destroy the widget view
        PLISTNODE pNode = lstQueryFirstNode(pTrayWidgetView->pllSubwidgetViews);
        ULONG ulIndex = 0;
        BOOL fFound = FALSE;
        while (pNode)
        {
            PPRIVATEWIDGETVIEW pView = (PPRIVATEWIDGETVIEW)pNode->pItemData;
            if (pView == pWidget2Remove)
            {
                DestroySubwidgetWindow(pPrivate,
                                       pNode);
                fFound = TRUE;
                break;
            }
            pNode = pNode->pNext;
            ulIndex++;
        }

        if (!fFound)
            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                   "Unable to destroy subwidget window.");
        else
        {
            // step 2: nuke widget setting
            PTRAYSETTING    pCurrentTray;
            PLINKLIST       pllSubwidgets;
            PPRIVATEWIDGETSETTING  pThis;

            if (    (pCurrentTray = FindCurrentTray(pPrivate))
                 && (pllSubwidgets = &pCurrentTray->llSubwidgetSettings)
                 && (pThis = lstItemFromIndex(pllSubwidgets,
                                              ulIndex))
                 && (ctrpRemoveWidgetSetting(pPrivate->pXCenterData->somSelf,
                                             &pThis))
               )
            {
                // found it:
                // recompose setup string with new trays
                YwgtSaveSetupAndSend(pPrivate);

                // reformat widgets in tray
                ReformatTray(pPrivate);

                return ((MRESULT)TRUE);
            }
            else
                cmnLog(__FILE__, __LINE__, __FUNCTION__,
                       "Failed to delete subwidget %d",
                       ulIndex);
        }
    }

    return ((MRESULT)FALSE);
}
*/

/*
 *@@ YwgtSwitchToTray:
 *
 */

static VOID YwgtSwitchToTray(HWND hwnd,
                             ULONG ulSwitchTo)
{
    PXCENTERWIDGET pWidget;
    PTRAYWIDGETPRIVATE pPrivate;

    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (PTRAYWIDGETPRIVATE)pWidget->pUser)
       )
    {
        SwitchToTray(pPrivate,
                     ulSwitchTo);
        // recompose setup string with new trays
        YwgtSaveSetupAndSend(pPrivate);
    }
}

/*
 *@@ YwgtDestroy:
 *      implementation for WM_DESTROY in fnwpTrayWidget.
 *
 *@@changed V0.9.14 (2001-08-01) [umoeller]: fixed memory leak
 */

static VOID YwgtDestroy(HWND hwnd)
{
    PXCENTERWIDGET pWidget;
    PTRAYWIDGETPRIVATE pPrivate;

    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (PTRAYWIDGETPRIVATE)pWidget->pUser)
       )
    {
        PPRIVATEWIDGETVIEW pTrayWidgetView = (PPRIVATEWIDGETVIEW)pWidget;

        // PTRAYSETUP pSetup = &pPrivate->Setup;
        // PPRIVATEWIDGETSETTING ppws = (PPRIVATEWIDGETSETTING)pPrivate->pWidget->pvWidgetSetting;

        // destroy all current subwidgets
        SwitchToTray(pPrivate,
                     -1);

        // WinDestroyPointer(pPrivate->hptrTray);
        // WinDestroyPointer(pPrivate->hptrHand);

        // if we had built a tray menu before,
        // invalidate that
        InvalidateMenu(pPrivate);

        lstFree(&pTrayWidgetView->pllSubwidgetViews);    // V0.9.14 (2001-08-01) [umoeller]

        YwgtClearSetup(&pPrivate->Setup);

        free(pPrivate);
    } // end if (pWidget && pPrivate);
}

/*
 *@@ fnwpTrayWidget:
 *      window procedure for the "Tray" widget class.
 *
 *@@changed V0.9.19 (2002-05-04) [umoeller]: removed all the ugly hack msgs since xwpCreateWidget can now handle everything correctly
 */

MRESULT EXPENTRY fnwpTrayWidget(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    switch (msg)
    {
        /*
         * WM_CREATE:
         *      as with all widgets, we receive a pointer to the
         *      XCENTERWIDGET in mp1, which was created for us.
         *
         *      The first thing the widget MUST do on WM_CREATE
         *      is to store the XCENTERWIDGET pointer (from mp1)
         *      in the QWL_USER window word by calling:
         *
         *          WinSetWindowPtr(hwnd, QWL_USER, mp1);
         *
         *      We use XCENTERWIDGET.pUser for allocating
         *      TRAYWIDGETPRIVATE for our own stuff.
         *
         *      Each widget must write its desired width into
         *      XCENTERWIDGET.cx and cy.
         */

        case WM_CREATE:
            WinSetWindowPtr(hwnd, QWL_USER, mp1);
            mrc = YwgtCreate(hwnd, mp1);
        break;

        /*
         * WM_CONTROL:
         *      process notifications/queries from the XCenter.
         */

        case WM_CONTROL:
            mrc = (MPARAM)YwgtControl(hwnd, mp1, mp2);
        break;

        /*
         * WM_PAINT:
         *
         */

        case WM_PAINT:
            YwgtPaint(hwnd);
        break;

        /*
         * WM_WINDOWPOSCHANGED:
         *
         */

        case WM_WINDOWPOSCHANGED:
            YwgtWindowPosChanged(hwnd, mp1, mp2);
        break;

        /*
         * WM_MOUSEMOVE:
         *
         */

        case WM_MOUSEMOVE:
        {
            PXCENTERWIDGET pWidget;
            PTRAYWIDGETPRIVATE pPrivate;

            if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
                 && (pPrivate = (PTRAYWIDGETPRIVATE)pWidget->pUser)
               )
            {
                SHORT sx = SHORT1FROMMP(mp1);
                if ((sx > 0) && (sx < pWidget->pGlobals->cxMiniIcon))
                {
                    WinSetPointer(HWND_DESKTOP, G_hptrHand);
                    break;
                }
            }
            mrc = ctrDefWidgetProc(hwnd, msg, mp1, mp2);
        }
        break;

        /*
         * WM_BUTTON1DOWN:
         * WM_BUTTON1UP:
         *      these show/hide the menu.
         *
         *      Showing the menu follows these steps:
         *          a)  first WM_BUTTON1DOWN hilites the button;
         *          b)  first WM_BUTTON1UP shows the menu.
         *
         *      When the button is pressed again, the open
         *      menu loses focus, which results in WM_MENUEND
         *      and destroys the window automatically.
         */

        case WM_BUTTON1DOWN:
        // case WM_BUTTON1DBLCLK:
            YwgtButton1Down(hwnd, mp1);
            mrc = (MPARAM)TRUE;     // message processed
        break;

        case WM_BUTTON1CLICK:       // V0.9.19 (2002-04-25) [umoeller]
        case WM_BUTTON1DBLCLK:
            YwgtButton1Click(hwnd, msg, mp1);
            mrc = (MPARAM)TRUE;     // message processed
        break;

        /*
         * WM_BUTTON1UP:
         *
         */

        case WM_BUTTON1UP:
            YwgtButton1Up(hwnd);
            mrc = (MPARAM)TRUE;     // message processed
        break;

        /*
         * WM_MENUEND:
         *
         */

        case WM_MENUEND:
            mrc = YwgtMenuEnd(hwnd, mp1, mp2);
        break;

        /*
         * WM_CONTEXTMENU:
         *
         */

        case WM_CONTEXTMENU:
            mrc = YwgtContextMenu(hwnd, mp1, mp2);
        break;

        /*
         * WM_COMMAND:
         *
         */

        case WM_COMMAND:
            mrc = YwgtCommand(hwnd, mp1, mp2);
        break;

        /*
         * DM_DRAGOVER:
         *
         */

        case DM_DRAGOVER:
            mrc = YwgtDragOver(hwnd, mp1);
        break;

        /*
         * DM_DRAGLEAVE:
         *
         */

        case DM_DRAGLEAVE:
            // remove emphasis
            YwgtDragLeave(hwnd);
        break;

        /*
         * DM_DROP:
         *
         */

        case DM_DROP:
            YwgtDrop(hwnd, mp1);
        break;

        // the following messages are normally handled by
        // the XCenter client, but since the subwidget will
        // now post them to the tray widget window, we must
        // handle them as well:

        /*
         * XCM_SETWIDGETSIZE:
         *
         */

        case XCM_SETWIDGETSIZE:
            SetSubwidgetSize(hwnd, (HWND)mp1, (ULONG)mp2);
        break;

        /*
         * XCM_CREATEWIDGETWINDOW:
         *      mirror of the implementation of this msg
         *      in fnwpXCenterMainClient.
         *
         *      mp1 has new PPRIVATEWIDGETSETTING,
         *      mp2 has widget index.
         */

        case XCM_CREATEWIDGETWINDOW:  // V0.9.19 (2002-04-25) [umoeller]
        {
            PXCENTERWIDGET pWidget;
            PTRAYWIDGETPRIVATE pPrivate;
            if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
                 && (pPrivate = (PTRAYWIDGETPRIVATE)pWidget->pUser)
               )
                // this msg is only used for creating the window on the
                // GUI thread because method calls may run on any thread...
                ctrpCreateWidgetWindow(pPrivate->pXCenterData,
                                       pWidget,        // owning tray widget
                                       (PPRIVATEWIDGETSETTING)mp1,
                                       (ULONG)mp2);        // ulWidgetIndex
        }
        break;

        /*
         * XCM_DESTROYWIDGETWINDOW:
         *      mirror of the implementation of this msg
         *      in fnwpXCenterMainClient.
         *
         *      While the main client will simply call
         *      WinDestroyWindow and ctrDefWidgetProc
         *      will do the cleanup of the widgets list,
         *      we have to do the cleanup manually here
         *      because otherwise we run into trouble
         *      with tray switching... at least I can't
         *      get this to work otherwise.
         *
         *      Still, we now have the same msg for both
         *      the client and trays, finally.
         *
         *      (HWND)mp1 is the subwidget window.
         *
         *added here V0.9.19 (2002-05-04) [umoeller]
         */

        case XCM_DESTROYWIDGETWINDOW:
        {
            PXCENTERWIDGET pWidget;
            PTRAYWIDGETPRIVATE pPrivate;
            HWND hwndSubwidget;
            if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
                 && (pPrivate = (PTRAYWIDGETPRIVATE)pWidget->pUser)
                 && (hwndSubwidget = (HWND)mp1)
               )
            {
                // look up the subwidget window in our list
                // if we can't find it, we shouldn't destroy
                // anything!
                PPRIVATEWIDGETVIEW pTrayWidgetView = (PPRIVATEWIDGETVIEW)pWidget;
                PLISTNODE pNode = lstQueryFirstNode(pTrayWidgetView->pllSubwidgetViews);
                while (pNode)
                {
                    PPRIVATEWIDGETVIEW pView = (PPRIVATEWIDGETVIEW)pNode->pItemData;
                    if (pView->Widget.hwndWidget == hwndSubwidget)
                    {
                        WinDestroyWindow(hwndSubwidget);
                        lstRemoveNode(pTrayWidgetView->pllSubwidgetViews, pNode);
                        break;
                    }

                    pNode = pNode->pNext;
                }

                if (!pNode)
                    cmnLog(__FILE__, __LINE__, __FUNCTION__,
                           "XCM_DESTROYWIDGETWINDOW: cannot find subwidget HWND 0x%lX",
                           hwndSubwidget);
            }
        }
        break;

        /*
         * XCM_REFORMAT:
         */

        case XCM_REFORMAT: // V0.9.19 (2002-04-25) [umoeller]
        {
            PXCENTERWIDGET pWidget;
            PTRAYWIDGETPRIVATE pPrivate;
            if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
                 && (pPrivate = (PTRAYWIDGETPRIVATE)pWidget->pUser)
               )
                // reformat widgets in tray then
                ReformatTray(pPrivate);
        }
        break;

        /*
         * XCM_SAVESETUP:
         */

        case XCM_SAVESETUP:
            mrc = (MRESULT)YwgtSaveSubwidgetSetup(hwnd,
                                                  (HWND)mp1,        // subwidget
                                                  (PSZ)mp2);        // new setup string
        break;

        /*
         * XCM_REMOVESUBWIDGET:
         */

        /* case XCM_REMOVESUBWIDGET:
            mrc = YwgtRemoveSubwidget(hwnd, (PPRIVATEWIDGETVIEW)mp1);
        break; */      // removed V0.9.19 (2002-05-04) [umoeller]

        /*
         * XCM_TRAYSCHANGED:
         *
         *added V0.9.19 (2002-04-25) [umoeller]
         */

        case XCM_TRAYSCHANGED:
        {
            PXCENTERWIDGET pWidget;
            PTRAYWIDGETPRIVATE pPrivate;
            if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
                 && (pPrivate = (PTRAYWIDGETPRIVATE)pWidget->pUser)
               )
            {
                InvalidateMenu(pPrivate);

                if (pPrivate->Setup.pPrivateSetting->ulCurrentTray == -1)
                    SwitchToTray(pPrivate, 0);

                // recompose setup string with new trays
                YwgtSaveSetupAndSend(pPrivate);
            }
        }
        break;

        case XCM_SWITCHTOTRAY:
            YwgtSwitchToTray(hwnd, (ULONG)mp1);
        break;

        /* case XCM_CREATEOBJECTBUTTON: // removed V0.9.19 (2002-05-04) [umoeller]
        {
            PXCENTERWIDGET pWidget;
            PTRAYWIDGETPRIVATE pPrivate;
            if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
                 && (pPrivate = (PTRAYWIDGETPRIVATE)pWidget->pUser)
               )
            {
                YwgtCreateSubwidget(pPrivate,
                                    "ObjButton",  // widget class
                                    (PCSZ)mp1,
                                    (ULONG)mp2);
            }
        }
        break; */

        /*
        case XCM_CREATESUBWIDGET: // removed V0.9.19 (2002-05-04) [umoeller]
            // process later, as we may still be answering
            // a dnd sequence
            #define UM_CREATESUBWIDGET (WM_USER + 600)

            WinPostMsg(hwnd,
                       UM_CREATESUBWIDGET,
                       (MPARAM)strdup((PSZ)mp1),
                       mp2);
        break;

        case UM_CREATESUBWIDGET: // removed V0.9.19 (2002-05-04) [umoeller]
        {
            PXCENTERWIDGET pWidget;
            PTRAYWIDGETPRIVATE pPrivate;

            if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
                 && (pPrivate = (PTRAYWIDGETPRIVATE)pWidget->pUser)
               )
            {
                PSZ pszBuff = (PSZ)mp1;
                PSZ pszSettings = strstr(pszBuff, "\r\n");

                pszSettings[0] = 0;
                pszSettings += 2;

                YwgtCreateSubwidget(pPrivate,
                                    pszBuff,  // widget class
                                    pszSettings,
                                    (ULONG)mp2);
            }

            free((PSZ)mp1);
        }
        break;
        */

        /*
         * WM_DESTROY:
         *
         */

        case WM_DESTROY:
            YwgtDestroy(hwnd);
            mrc = ctrDefWidgetProc(hwnd, msg, mp1, mp2);
        break;

        default:
            mrc = ctrDefWidgetProc(hwnd, msg, mp1, mp2);
    } // end switch(msg)

    return (mrc);
}


