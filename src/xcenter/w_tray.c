
/*
 *@@sourcefile w_tray.c:
 *      XCenter "Tray" widget implementation.
 *      This is built into the XCenter and not in
 *      a plugin DLL.
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

#include "shared\center.h"              // public XCenter interfaces

#include "shared\center.h"              // public XCenter interfaces
#include "xcenter\centerp.h"            // private XCenter implementation

#pragma hdrstop                     // VAC++ keeps crashing otherwise

/* ******************************************************************
 *
 *   Private tray widget instance data
 *
 ********************************************************************/

// As opposed to other widgets, the tray-internal structures
// are defined in centerp.h because the engine also needs
// access to them.

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

VOID DestroySubwidgetWindow(PTRAYWIDGETPRIVATE pPrivate,
                            PLISTNODE pNode)    // in: WIDGETVIEWSTATE node from pPrivate->llWidgetViews
{
    PWIDGETVIEWSTATE pView = (PWIDGETVIEWSTATE)pNode->pItemData;
    WinDestroyWindow(pView->Widget.hwndWidget);
            // ctrDefWidgetProc also frees the WIDGETVIEWSTATE;
            // however, since this is a tray widget, it does
            // not remove the node from the list, so we must
            // do this manually
    lstRemoveNode(&pPrivate->llSubwidgetViews, pNode);
}

/*
 *@@ ReformatTray:
 *      reformats the entire tray. This is the exact
 *      equivalent to ctrpReformat for the entire
 *      XCenter client.
 */

VOID ReformatTray(PXCENTERWINDATA pXCenterData,
                  PTRAYWIDGETPRIVATE pPrivate)
{
    // this is the same sequence that ctrpReformat uses
    // for the entire XCenter client
    ULONG cyMax;
    PXCENTERGLOBALS pGlobals = &pXCenterData->Globals;

    // leftmost position of subwidgets:
    LONG xStart = pGlobals->cxMiniIcon + 3;
            // tray button size plus an extra pixel
    if (0 == (pGlobals->flDisplayStyle & XCS_FLATBUTTONS))
        xStart += 4;     // 2*2 for button borders

    // get widget sizes
    ctrpGetWidgetSizes(&pPrivate->llSubwidgetViews);

    // get tallest widget
    cyMax = ctrpQueryMaxWidgetCY(&pXCenterData->llWidgets);

    if (cyMax > pPrivate->szlCurrent.cy)
    {
        pPrivate->szlCurrent.cy = cyMax;
        // @@todo tell XCenter to grow vertically
    }
    else
        ctrpPositionWidgets(pGlobals,
                            &pPrivate->llSubwidgetViews,
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

PTRAYSETTING FindCurrentTray(PTRAYWIDGETPRIVATE pPrivate)
{
    PPRIVATEWIDGETSETTING ppws = (PPRIVATEWIDGETSETTING)pPrivate->pWidget->pvWidgetSetting;

    if (pPrivate->Setup.ulCurrentTray == -1)
        return (NULL);

    return (lstItemFromIndex(ppws->pllTraySettings,
                             pPrivate->Setup.ulCurrentTray));
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
 */

BOOL SwitchToTray(PTRAYWIDGETPRIVATE pPrivate,
                  ULONG ulNewTray)      // in: new current tray or -1 for none
{
    if (pPrivate)
    {
        PPRIVATEWIDGETSETTING ppws = (PPRIVATEWIDGETSETTING)pPrivate->pWidget->pvWidgetSetting;
        PTRAYSETTING pNewTray = NULL;
        // make sure the tray is really in the list
        if (    (ulNewTray == -1)
             || (pNewTray = (PTRAYSETTING)lstItemFromIndex(ppws->pllTraySettings,
                                                           ulNewTray))
           )
        {
            // exists:
            if (ulNewTray != pPrivate->Setup.ulCurrentTray)
            {
                // tray changed:
                PTRAYSETTING pCurrentTray;
                PLISTNODE pNode,
                          pNext;
                if (pCurrentTray = FindCurrentTray(pPrivate))
                {
                    // destroy all widget windows in the current tray
                    pNode = lstQueryFirstNode(&pPrivate->llSubwidgetViews);
                    while (pNode)
                    {
                        pNext = pNode->pNext;
                        DestroySubwidgetWindow(pPrivate, pNode);
                        pNode = pNext;
                    }
                }

                pPrivate->Setup.ulCurrentTray = ulNewTray;

                // create widget windows from the new tray, if any
                if (pNewTray)
                {
                    PXCENTERWIDGET pWidget = pPrivate->pWidget;
                    PXCENTERWINDATA pXCenterData = WinQueryWindowPtr(pWidget->pGlobals->hwndFrame,
                                                                     QWL_USER);

                    for (pNode = lstQueryFirstNode(&pNewTray->llSubwidgets);
                         pNode;
                         pNode = pNode->pNext)
                    {
                        PTRAYSUBWIDGET pSubwidget = (PTRAYSUBWIDGET)pNode->pItemData;
                        if (!ctrpCreateWidgetWindow(pXCenterData,
                                                    (PWIDGETVIEWSTATE)pWidget,
                                                         // parent: tray widget
                                                    &pPrivate->llSubwidgetViews,
                                                    &pSubwidget->Setting,
                                                    -1))             // add rightmost
                            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                                   "Widget view creation failed.");
                    }

                    // reformat widgets in tray then
                    ReformatTray(pXCenterData,
                                 pPrivate);
                }

                // if we had built a tray menu before,
                // invalidate that
                if (pPrivate->hwndTraysMenu)
                {
                    WinDestroyWindow(pPrivate->hwndTraysMenu);
                    pPrivate->hwndTraysMenu = NULLHANDLE;
                }

                return (TRUE);
            }
        }
    }

    return (FALSE);
}

/* ******************************************************************
 *
 *   Trays management
 *
 ********************************************************************/

/*
 *@@ InvalidateMenu:
 *
 */

VOID InvalidateMenu(PTRAYWIDGETPRIVATE pPrivate)
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
 *@@ CreateTray:
 *      creates a new (empty) tray with the specified
 *      name. Does not switch to the tray yet.
 *
 *      This does not save the tray widget settings.
 */

PTRAYSETTING CreateTray(PTRAYWIDGETPRIVATE pPrivate,
                        const char *pcszTrayName,   // in: tray name
                        PULONG pulIndex)            // out: index of new tray
{
    PTRAYSETTING pNewTray;
    PPRIVATEWIDGETSETTING ppws = (PPRIVATEWIDGETSETTING)pPrivate->pWidget->pvWidgetSetting;

    if (pNewTray = NEW(TRAYSETTING))
    {
        pNewTray->pszTrayName = strhdup(pcszTrayName);

        lstInit(&pNewTray->llSubwidgets, FALSE);

        // append to widget setup
        lstAppendItem(ppws->pllTraySettings, pNewTray);

        InvalidateMenu(pPrivate);

        if (pulIndex)
            *pulIndex = lstCountItems(ppws->pllTraySettings) - 1;
    }

    return (pNewTray);
}

/*
 *@@ CreateSubwidget:
 *      creates a new subwidget setting in the specified
 *      tray. Does not create a subwidget window though.
 *
 *      This is used when the tray widget's setup string
 *      is parsed to create all the trays and subwidgets
 *      and also when new subwidgets are manually added
 *      by the user.
 *
 *      This does not save the tray widget settings.
 */

PTRAYSUBWIDGET CreateSubwidget(PXCENTERWINDATA pXCenterData,
                               PXCENTERWIDGET pTrayWidget,
                               PTRAYSETTING pTray,              // in: tray to create subwidget in
                               const char *pcszWidgetClass,     // in: new subwidget's class
                               const char *pcszSetupString,     // in: new subwidget's setup string
                               ULONG ulIndex)           // in: index (-1 for rightmost)
{
    PTRAYSUBWIDGET pNew;
    if (pNew = NEW(TRAYSUBWIDGET))
    {
        PTRAYWIDGETPRIVATE pPrivate = (PTRAYWIDGETPRIVATE)pTrayWidget->pUser;

        ZERO(pNew);

        pNew->pOwningTray = pTray;

        pNew->Setting.Public.pszWidgetClass = strhdup(pcszWidgetClass);
        pNew->Setting.Public.pszSetupString = strhdup(pcszSetupString);

        if (    (ulIndex == -1)
             || (ulIndex >= lstCountItems(&pTray->llSubwidgets))
           )
            lstAppendItem(&pTray->llSubwidgets,
                          pNew);
        else
            lstInsertItemBefore(&pTray->llSubwidgets,
                                pNew,
                                ulIndex);
    }

    return (pNew);
}

/*
 *@@ DeleteSubwidget:
 *      deletes the specified subwidgets from its
 *      owning tray.
 *
 *      This does not save the tray widget settings.
 *
 *      Preconditions:
 *
 *      -- The subwidget's window must have been destroyed first.
 */

BOOL DeleteSubwidget(PTRAYSUBWIDGET pSubwidget)     // in: subwidget to delete
{
    if (    (pSubwidget)
         && (pSubwidget->pOwningTray)
            // remove subwidget from owning tray's list
         && (lstRemoveItem(&pSubwidget->pOwningTray->llSubwidgets,
                           pSubwidget))
       )
    {
        // now clean up
        if (pSubwidget->Setting.Public.pszWidgetClass)
            free(pSubwidget->Setting.Public.pszWidgetClass);
        if (pSubwidget->Setting.Public.pszSetupString)
            free(pSubwidget->Setting.Public.pszSetupString);

        free(pSubwidget);

        return (TRUE);
    }

    return (FALSE);
}

/*
 *@@ DeleteTray:
 *      deletes the specified tray completely, including
 *      all its subwidgets.
 *
 *      If the tray is the current tray, this also
 *      destroys all widget views. It does not switch
 *      to another tray, so after this pSetup->pCurrentTray
 *      might be NULL.
 *
 *      This does not save the tray widget settings.
 */

BOOL DeleteTray(PTRAYWIDGETPRIVATE pPrivate,
                ULONG ulIndex)             // in: tray to delete
{
    if (pPrivate)
    {
        PPRIVATEWIDGETSETTING ppws = (PPRIVATEWIDGETSETTING)pPrivate->pWidget->pvWidgetSetting;

        PLISTNODE pTrayNode,
                  pSubwidgetNode;
        if (pTrayNode = lstNodeFromIndex(ppws->pllTraySettings,
                                         ulIndex))
        {
            PTRAYSETTING pTray = (PTRAYSETTING)pTrayNode->pItemData;

            // is this the current tray?
            if (ulIndex == pPrivate->Setup.ulCurrentTray)
                // yes:  switch first
                // (this destroys the subwidget windows)
                SwitchToTray(pPrivate,
                             -1);         // new tray: none

            // delete all subwidgets in the tray
            pSubwidgetNode = lstQueryFirstNode(&pTray->llSubwidgets);
            while (pSubwidgetNode)
            {
                PLISTNODE pNext = pSubwidgetNode->pNext;

                DeleteSubwidget((PTRAYSUBWIDGET)pSubwidgetNode->pItemData);

                pSubwidgetNode = pNext;
            }

            lstRemoveNode(ppws->pllTraySettings, pTrayNode);

            if (pTray->pszTrayName)
                free(pTray->pszTrayName);

            free(pTray);

            // if we had built a tray menu before,
            // invalidate that
            InvalidateMenu(pPrivate);

            return (TRUE);
        }
    }

    return (FALSE);
}

/*
 *@@ SetSubwidgetSize:
 *      implementation for XCM_SETWIDGETSIZE in
 *      fnwpTrayWidget.
 */

BOOL SetSubwidgetSize(HWND hwnd,            // tray widget
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
        PXCENTERWINDATA pXCenterData = WinQueryWindowPtr(pWidget->pGlobals->hwndFrame,
                                                         QWL_USER);
        PLISTNODE pNode = lstQueryFirstNode(&pPrivate->llSubwidgetViews);
        while (pNode)
        {
            PWIDGETVIEWSTATE pView = (PWIDGETVIEWSTATE)pNode->pItemData;
            if (pView->Widget.hwndWidget == hwndWidget)
            {
                // found it:
                // set new width
                pView->szlWanted.cx = ulNewWidth;

                ReformatTray(pXCenterData,
                             pPrivate);
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

VOID YwgtClearSetup(PTRAYSETUP pSetup)
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
 */

VOID YwgtScanSetup(const char *pcszSetupString,
                   PTRAYSETUP pSetup)
{
    PSZ p;

    // width
    p = ctrScanSetupString(pcszSetupString,
                           "WIDTH");
    if (p)
    {
        pSetup->cx = atoi(p);
        ctrFreeSetupValue(p);
    }
    else
        pSetup->cx = 100;

    // current tray
    p = ctrScanSetupString(pcszSetupString,
                           "CURRENTTRAY");
    if (p)
    {
        pSetup->ulCurrentTray = atoi(p);
        ctrFreeSetupValue(p);
    }
    else
        pSetup->cx = -1;

}

/*
 *@@ YwgtSaveSetup:
 *      composes a new setup string.
 *      The caller must invoke xstrClear on the
 *      string after use.
 */

VOID YwgtSaveSetup(PXSTRING pstrSetup,       // out: setup string (is cleared first)
                   PTRAYSETUP pSetup)
{
    CHAR    szTemp[100];
    xstrInit(pstrSetup, 100);

    sprintf(szTemp, "WIDTH=%d;",
            pSetup->cx);
    xstrcat(pstrSetup, szTemp, 0);

    sprintf(szTemp, "CURRENTTRAY=%d;",
            pSetup->ulCurrentTray);
    xstrcat(pstrSetup, szTemp, 0);
}

/*
 *@@ YwgtSaveSetupAndSend:
 *
 */

VOID YwgtSaveSetupAndSend(PTRAYWIDGETPRIVATE pPrivate)
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
 */

PTRAYSUBWIDGET YwgtCreateSubwidget(PXCENTERWINDATA pXCenterData,
                                   PTRAYWIDGETPRIVATE pPrivate,
                                   const char *pcszWidgetClass,
                                   const char *pcszSetupString,
                                   ULONG ulIndex)       // in: index or -1
{
    PTRAYSUBWIDGET pSubwidget = NULL;
    PXCENTERWIDGET pWidget = pPrivate->pWidget;

    PTRAYSETTING    pCurrentTray;

    if (pCurrentTray = FindCurrentTray(pPrivate))
    {
        if (pSubwidget = CreateSubwidget(pXCenterData,
                                         pWidget,
                                         pCurrentTray,
                                         pcszWidgetClass,
                                         pcszSetupString,
                                         ulIndex))
        {
            // create widget window
            if (!ctrpCreateWidgetWindow(pXCenterData,
                                        (PWIDGETVIEWSTATE)pWidget,
                                             // parent: tray widget
                                        &pPrivate->llSubwidgetViews,
                                        &pSubwidget->Setting,
                                        ulIndex))
                cmnLog(__FILE__, __LINE__, __FUNCTION__,
                       "Subwidget view creation failed.");

            // reformat widgets in tray then
            ReformatTray(pXCenterData,
                         pPrivate);

            // recompose setup string with new trays
            YwgtSaveSetupAndSend(pPrivate);
        }
    }

    return (pSubwidget);
}

/* ******************************************************************
 *
 *   Widget settings dialog
 *
 ********************************************************************/

// None currently.

/* ******************************************************************
 *
 *   Callbacks stored in XCENTERWIDGET
 *
 ********************************************************************/

/*
 *@@ YwgtSetupStringChanged:
 *      this gets called from ctrSetSetupString if
 *      the setup string for a widget has changed.
 *
 *      This procedure's address is stored in
 *      XCENTERWIDGET so that the XCenter knows that
 *      we can do this.
 */

VOID EXPENTRY YwgtSetupStringChanged(PXCENTERWIDGET pWidget,
                                     const char *pcszNewSetupString)
{
    PTRAYWIDGETPRIVATE pPrivate = (PTRAYWIDGETPRIVATE)pWidget->pUser;
    if (pPrivate)
    {
        // reinitialize the setup data
        YwgtClearSetup(&pPrivate->Setup);
        YwgtScanSetup(pcszNewSetupString,
                      &pPrivate->Setup);
    }
}

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
 */

MRESULT YwgtCreate(HWND hwnd, MPARAM mp1)
{
    MRESULT mrc = 0;        // continue window creation

    PSZ     p = NULL;
    APIRET  arc = NO_ERROR;

    HMODULE hmodRes = cmnQueryMainResModuleHandle();

    PPRIVATEWIDGETSETTING pPrivateSetting;
    ULONG ulSwitchTo;

    PXCENTERWIDGET pWidget = (PXCENTERWIDGET)mp1;
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
    pPrivateSetting = (PPRIVATEWIDGETSETTING)pWidget->pvWidgetSetting;

    // list of current widget views
    lstInit(&pPrivate->llSubwidgetViews, FALSE);

    YwgtScanSetup(pWidget->pcszSetupString,
                  &pPrivate->Setup);

    // save current tray, can be -1
    ulSwitchTo = pPrivate->Setup.ulCurrentTray;
    // fake current tray so that switch to will work below
    pPrivate->Setup.ulCurrentTray = -1;

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
        pTray = CreateTray(pPrivate,
                           sz,
                           NULL);
        // switch to tray 0 after all this
        ulSwitchTo = 0;
    }

    // enable context menu help
    pWidget->pcszHelpLibrary = cmnQueryHelpLibrary();
    pWidget->ulHelpPanelID = ID_XSH_WIDGET_TRAY;

    // load tray icons
    pPrivate->hptrTray = WinLoadPointer(HWND_DESKTOP,
                                        hmodRes,
                                        ID_ICON_TRAY);
    pPrivate->hptrHand  = WinLoadPointer(HWND_DESKTOP,
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

BOOL YwgtControl(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    BOOL brc = FALSE;

    PXCENTERWIDGET pWidget;
    PTRAYWIDGETPRIVATE pPrivate;

    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (PTRAYWIDGETPRIVATE)pWidget->pUser)
       )
    {
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
                        PXCENTERWINDATA pXCenterData = WinQueryWindowPtr(pWidget->pGlobals->hwndFrame,
                                                                         QWL_USER);
                        PLISTNODE pNode;

                        // reformat widgets; button size might
                        // have changed
                        ReformatTray(pXCenterData,
                                     pPrivate);

                        // and re-broadcast this to our subwidgets
                        for (pNode = lstQueryFirstNode(&pPrivate->llSubwidgetViews);
                             pNode;
                             pNode = pNode->pNext)
                        {
                            PWIDGETVIEWSTATE pView = (PWIDGETVIEWSTATE)pNode->pItemData;
                            WinSendMsg(pView->Widget.hwndWidget,
                                       WM_CONTROL,
                                       mp1,
                                       mp2);
                        }
                    }
                    break;
                }
            break;

            case ID_XCENTER_TOOLTIP:
                if (usNotifyCode == TTN_NEEDTEXT)
                {
                    PTOOLTIPTEXT pttt = (PTOOLTIPTEXT)mp2;
                    PTRAYSETTING pCurrentTray;

                    if (    (pPrivate->Setup.ulCurrentTray != -1)
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

VOID YwgtPaint(HWND hwnd)
{
    PXCENTERWIDGET pWidget;
    PTRAYWIDGETPRIVATE pPrivate;

    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (PTRAYWIDGETPRIVATE)pWidget->pUser)
       )
    {
        RECTL           rclWin;
        CHAR            sz[100];
        const XCENTERGLOBALS *pGlobals = pWidget->pGlobals;
        XBUTTONDATA     xbd;
        ULONG           fl = 0;

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
        xbd.rcl.xRight = pGlobals->cxMiniIcon + 2;
        if (0 == (pGlobals->flDisplayStyle & XCS_FLATBUTTONS))
            xbd.rcl.xRight += 4;     // 2*2 for button borders
        else
            // flat buttons:
            fl |= XBF_FLAT;
        if (    (pPrivate->fMouseButton1Down)
             || (pPrivate->fButtonSunk)
           )
            fl |= XBF_PRESSED;

        xbd.cxMiniIcon = pGlobals->cxMiniIcon;
        xbd.lcol3DDark = pGlobals->lcol3DDark;
        xbd.lcol3DLight = pGlobals->lcol3DLight;
        xbd.lMiddle = pGlobals->lcolClientBackground; // WinQuerySysColor(HWND_DESKTOP, SYSCLR_BUTTONMIDDLE, 0);

        xbd.hptr = pPrivate->hptrTray;

        ctlPaintXButton(hps,
                        fl,         // note, XBD_BACKGROUND is never set
                        &xbd);

        WinEndPaint(hps);
    } // end if (pWidget && pPrivate)
}

/*
 *@@ Ywg itWindowPosChanged:
 *      implementation for WM_WINDOWPOSCHANGED in fnwpTrayWidget.
 *
 */

VOID YwgtWindowPosChanged(HWND hwnd, MPARAM mp1, MPARAM mp2)
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
                PXCENTERWINDATA pXCenterData = WinQueryWindowPtr(pWidget->pGlobals->hwndFrame,
                                                                 QWL_USER);
                pPrivate->szlCurrent.cy = pswpNew->cy;
                        // this will then be used for resizing the subwidgets too
                ReformatTray(pXCenterData,
                             pPrivate);
            }
        } // end if (pswpNew->fl & SWP_SIZE)
    } // end if (pWidget && pPrivate)
}

/*
 * YwgtButton1Down:
 *      implementation for WM_BUTTON1DOWN in fnwpTrayWidget.
 */

VOID YwgtButton1Down(HWND hwnd, MPARAM mp1)
{
    PXCENTERWIDGET pWidget;
    PTRAYWIDGETPRIVATE pPrivate;
    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (PTRAYWIDGETPRIVATE)pWidget->pUser)
       )
    {
        const XCENTERGLOBALS *pGlobals = pWidget->pGlobals;
        SHORT sx = SHORT1FROMMP(mp1);
        SHORT sComp = pGlobals->cxMiniIcon + 2;
        if (0 == (pGlobals->flDisplayStyle & XCS_FLATBUTTONS))
            sComp += 4;     // 2*2 for button borders
        if (sx < sComp)
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
                                               (pPrivate->Setup.ulCurrentTray == ulIndex)
                                                    ? MIA_CHECKED
                                                    : 0);
                        }
                    }

                    if (pPrivate->hwndTraysMenu)
                    {
                        PXCENTERWINDATA pXCenterData = WinQueryWindowPtr(pWidget->pGlobals->hwndFrame,
                                                                         QWL_USER);
                        RECTL rclButton;
                        WinQueryWindowRect(hwnd, &rclButton);
                        rclButton.xRight = pGlobals->cxMiniIcon + 2;
                        if (0 == (pGlobals->flDisplayStyle & XCS_FLATBUTTONS))
                            rclButton.xRight += 4;     // 2*2 for button borders
                        // rclButton now has button coordinates;
                        // convert this to screen coordinates:
                        WinMapWindowPoints(hwnd,
                                           HWND_DESKTOP,
                                           (PPOINTL)&rclButton,
                                           2);          // rectl == 2 points

                        /* if (pWidget->pGlobals->ulPosition == XCENTER_TOP)
                            cmnuSetPositionBelow((PPOINTL)&rclButton); */

                        // draw source emphasis around widget
                        ctrpDrawEmphasis(pXCenterData,
                                         FALSE,     // draw, not remove emphasis
                                         NULL,
                                         hwnd,
                                         NULLHANDLE);   // standard PS

                        ctlDisplayButtonMenu(hwnd,
                                             pPrivate->hwndTraysMenu);
                    }
                } // end if (!pPrivate->fButtonSunk)
            }
        }
    } // end if (pWidget)
}

/*
 * YwgtButton1Up:
 *      implementation for WM_BUTTON1UP in fnwpTrayWidget.
 */

VOID YwgtButton1Up(HWND hwnd)
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

MRESULT YwgtMenuEnd(HWND hwnd, MPARAM mp1, MPARAM mp2)
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
 */

MRESULT YwgtContextMenu(HWND hwnd, MPARAM mp1, MPARAM mp2)
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

        fTrayValid = (pPrivate->Setup.ulCurrentTray != -1);

        // allow "delete tray" only if we have more than one
        WinEnableMenuItem(hwndContextMenu,
                          ID_CRMI_REMOVETRAY,
                          (    (fTrayValid)
                            && (lstCountItems(ppws->pllTraySettings) > 1)
                          ));
        WinEnableMenuItem(hwndContextMenu,
                          ID_CRMI_RENAMETRAY,
                          (fTrayValid));
        WinEnableMenuItem(hwndContextMenu,
                          ID_CRMI_REMOVETRAY,
                          (fTrayValid));
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
 *      --  >= GLOBALSETTINGS.VarMenuItems: "add widget" menu.
 */

MRESULT YwgtCommand(HWND hwnd, MPARAM mp1, MPARAM mp2)
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
                                                     TEBF_REMOVETILDE | TEBF_REMOVEELLIPSE))
                {
                    // create a tray with that name
                    ULONG ulIndex = 0;
                    PTRAYSETTING pTray = CreateTray(pPrivate,
                                                    pszNewTrayName,
                                                    &ulIndex);
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
                                                         TEBF_REMOVETILDE | TEBF_REMOVEELLIPSE))
                    {
                        if (pCurrentTray->pszTrayName)
                            free(pCurrentTray->pszTrayName);
                        pCurrentTray->pszTrayName = pszNewTrayName;
                                // this is malloc'd too

                        // if we had built a tray menu before,
                        // invalidate that
                        InvalidateMenu(pPrivate);

                        // recompose setup string with new trays
                        YwgtSaveSetupAndSend(pPrivate);
                    }
                }
            break;

            case ID_CRMI_REMOVETRAY:
                if (pCurrentTray = FindCurrentTray(pPrivate))
                {
                    PSZ apsz = pCurrentTray->pszTrayName;
                    if (cmnMessageBoxMsgExt(pGlobals->hwndClient,
                                            220,        // delete tray
                                            &apsz,
                                            1,
                                            221,        // delete tray %1?
                                            MB_YESNO)
                            == MBID_YES)
                    {
                        // alright, nuke it
                        PLISTNODE pNode;
                        DeleteTray(pPrivate,
                                   pPrivate->Setup.ulCurrentTray);
                        // try to switch to first tray on list
                        SwitchToTray(pPrivate,
                                     0);
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
                    PXCENTERWINDATA pXCenterData = WinQueryWindowPtr(pWidget->pGlobals->hwndFrame,
                                                                     QWL_USER);

                    YwgtCreateSubwidget(pXCenterData,
                                        pPrivate,
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

MRESULT YwgtDragOver(HWND hwnd,
                     MPARAM mp1)
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

VOID YwgtDragLeave(HWND hwnd)
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

VOID YwgtDrop(HWND hwnd,
              MPARAM mp1)
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

BOOL YwgtSaveSubwidgetSetup(HWND hwnd,
                            HWND hwndSubwidget,
                            const char *pcszSetupString)     // can be NULL
{
    PXCENTERWIDGET pWidget;
    PTRAYWIDGETPRIVATE pPrivate;

    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (PTRAYWIDGETPRIVATE)pWidget->pUser)
       )
    {
        // 1) find the subwidget in the subwidgets list
        PLISTNODE pNode;
        ULONG ulSubwidgetIndex = 0;
        PPRIVATEWIDGETSETTING ppws = (PPRIVATEWIDGETSETTING)pPrivate->pWidget->pvWidgetSetting;

        for (pNode = lstQueryFirstNode(&pPrivate->llSubwidgetViews);
             pNode;
             pNode = pNode->pNext, ulSubwidgetIndex++)
        {
            PWIDGETVIEWSTATE pView = (PWIDGETVIEWSTATE)pNode->pItemData;
            if (pView->Widget.hwndWidget == hwndSubwidget)
            {
                // that's ours:
                // find the corresponding setting entry
                PTRAYSETTING pCurrentTray;
                PTRAYSUBWIDGET pSubwidget;

                if (    (pCurrentTray = FindCurrentTray(pPrivate))
                     && (pSubwidget = lstItemFromIndex(&pCurrentTray->llSubwidgets,
                                                       ulSubwidgetIndex))
                   )
                {
                    // got it:
                    // update the setup string
                    if (pSubwidget->Setting.Public.pszSetupString)
                        free(pSubwidget->Setting.Public.pszSetupString);
                    pSubwidget->Setting.Public.pszSetupString = strhdup(pcszSetupString);

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
 *@@ YwgtRemoveSubwidget:
 *      implementation for XCM_REMOVESUBWIDGET in fnwpTrayWidget.
 *      This is a private message sent from ctrDefWidgetProc
 *      when the ID_CRMI_REMOVEWGT menu item is
 *      received from a subwidget. We then destroy
 *      that and update our lists.
 */

MRESULT YwgtRemoveSubwidget(HWND hwnd,
                            PWIDGETVIEWSTATE pWidget2Remove)
{
    // mp1 has the WIDGETVIEWSTATE to be destroyed:
    // search our list for that
    PXCENTERWIDGET pWidget;
    PTRAYWIDGETPRIVATE pPrivate;

    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (PTRAYWIDGETPRIVATE)pWidget->pUser)
         && (pPrivate->Setup.ulCurrentTray != -1)
       )
    {
        // step 1: destroy the widget view
        PLISTNODE pNode = lstQueryFirstNode(&pPrivate->llSubwidgetViews);
        ULONG ulIndex = 0;
        BOOL fFound = FALSE;
        while (pNode)
        {
            PWIDGETVIEWSTATE pView = (PWIDGETVIEWSTATE)pNode->pItemData;
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
            PTRAYSUBWIDGET  pThis;

            if (    (pCurrentTray = FindCurrentTray(pPrivate))
                 && (pllSubwidgets = &pCurrentTray->llSubwidgets)
                 && (pThis = lstItemFromIndex(pllSubwidgets,
                                              ulIndex))
                 && (DeleteSubwidget(pThis))
               )
            {
                // found it:
                PXCENTERWINDATA pXCenterData = WinQueryWindowPtr(pWidget->pGlobals->hwndFrame,
                                                                 QWL_USER);

                // recompose setup string with new trays
                YwgtSaveSetupAndSend(pPrivate);

                // reformat widgets in tray
                ReformatTray(pXCenterData,
                             pPrivate);

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

/*
 *@@ YwgtSwitchToTray:
 *
 */

VOID YwgtSwitchToTray(HWND hwnd,
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
    }
}

/*
 *@@ YwgtDestroy:
 *      implementation for WM_DESTROY in fnwpTrayWidget.
 *
 */

VOID YwgtDestroy(HWND hwnd)
{
    PXCENTERWIDGET pWidget;
    PTRAYWIDGETPRIVATE pPrivate;

    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (PTRAYWIDGETPRIVATE)pWidget->pUser)
       )
    {
        PTRAYSETUP pSetup = &pPrivate->Setup;
        PPRIVATEWIDGETSETTING ppws = (PPRIVATEWIDGETSETTING)pPrivate->pWidget->pvWidgetSetting;

        // destroy all current subwidgets
        SwitchToTray(pPrivate,
                     -1);

        WinDestroyPointer(pPrivate->hptrTray);
        WinDestroyPointer(pPrivate->hptrHand);

        // if we had built a tray menu before,
        // invalidate that
        InvalidateMenu(pPrivate);

        free(pPrivate);
    } // end if (pWidget && pPrivate);
}

/*
 *@@ fnwpTrayWidget:
 *      window procedure for the "Tray" widget class.
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
                    WinSetPointer(HWND_DESKTOP, pPrivate->hptrHand);
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
        case WM_BUTTON1DBLCLK:
            YwgtButton1Down(hwnd, mp1);
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
         * WM_BUTTON1CLICK:
         *      swallow this
         */

        case WM_BUTTON1CLICK:
            mrc = (MPARAM)TRUE;
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
         * XCM_REFORMAT:
         *      @@todo, subwidget may send this
         */

        case XCM_REFORMAT:
            /*
            PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER);
            if (pXCenterData->fFrameFullyShown) // V0.9.9 (2001-02-08) [umoeller]
                ctrpReformat(pXCenterData,
                             (ULONG)mp1);       // flags
            */
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

        case XCM_REMOVESUBWIDGET:
            mrc = YwgtRemoveSubwidget(hwnd, (PWIDGETVIEWSTATE)mp1);
        break;

        case XCM_SWITCHTOTRAY:
            YwgtSwitchToTray(hwnd, (ULONG)mp1);
        break;

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


