
/*
 *@@sourcefile w_objbutton.c:
 *      XCenter "object button" widget implementation.
 *      This is build into the XCenter and not in
 *      a plugin DLL.
 *
 *      Function prefix for this file:
 *      --  Owgt*
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

#define INCL_DOSPROCESS
#define INCL_DOSEXCEPTIONS
#define INCL_DOSSEMAPHORES
#define INCL_DOSERRORS

#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WININPUT
#define INCL_WINSYS
#define INCL_WINTIMER
#define INCL_WINPOINTERS
#define INCL_WINMENUS

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
#include "helpers\gpih.h"               // GPI helper routines
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\winh.h"               // PM helper routines

// SOM headers which don't crash with prec. header files
#include "xcenter.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\contentmenus.h"        // shared menu logic
#include "shared\kernel.h"              // XWorkplace Kernel

#include "shared\center.h"              // public XCenter interfaces

#include "startshut\shutdown.h"         // XWorkplace eXtended Shutdown

#pragma hdrstop                     // VAC++ keeps crashing otherwise
#include <wpdesk.h>

/* ******************************************************************
 *
 *   Built-in "X-Button" widget
 *
 ********************************************************************/

/*
 *@@ PRIVATEWIDGETDATA:
 *      more window data for the "X-Button" widget.
 *
 *      An instance of this is created on WM_CREATE
 *      fnwpObjButtonWidget and stored in XCENTERWIDGETVIEW.pUser.
 */

typedef struct _PRIVATEWIDGETDATA
{
    BOOL        fMouseButton1Down;      // if TRUE, mouse button is currently down
    BOOL        fButtonSunk;            // if TRUE, button control is currently pressed;
                    // the control is painted "down" if either of the two are TRUE
    BOOL        fMouseCaptured; // if TRUE, mouse is currently captured

    HWND        hwndMenuMain;           // if != NULLHANDLE, this has the currently
                                        // open WPS context menu
    HPOINTER    hptrXMini;              // "X" icon for painting on button
} PRIVATEWIDGETDATA, *PPRIVATEWIDGETDATA;

/*
 *@@ OwgtCreate:
 *      implementation for WM_CREATE in fnwpObjButtonWidget.
 */

MRESULT OwgtCreate(HWND hwnd, MPARAM mp1)
{
    MRESULT mrc = 0;
    PXCENTERWIDGETVIEW pViewData = (PXCENTERWIDGETVIEW)mp1;
    PPRIVATEWIDGETDATA pPrivateData = malloc(sizeof(PRIVATEWIDGETDATA));
    memset(pPrivateData, 0, sizeof(PRIVATEWIDGETDATA));
    pViewData->pUser = pPrivateData;

    pPrivateData->hptrXMini = WinLoadPointer(HWND_DESKTOP,
                                            cmnQueryMainModuleHandle(),
                                            ID_ICONXMINI);

    pViewData->cxWanted
                  = pViewData->pGlobals->cxMiniIcon
                    + 4     // 2*2 for button borders
                    + 2;    // 2*1 spacing towards border
    // we wanna be square
    pViewData->cyWanted = pViewData->cxWanted;

    // enable context menu help
    pViewData->pcszHelpLibrary = cmnQueryHelpLibrary();
    pViewData->ulHelpPanelID = ID_XSH_WIDGET_OBJBUTTON_MAIN;

    // return 0 for success
    return (mrc);
}

/*
 * OwgtPaintButton:
 *      implementation for WM_PAINT in fnwpObjButtonWidget.
 */

VOID OwgtPaintButton(HWND hwnd)
{
    RECTL rclPaint;
    HPS hps = WinBeginPaint(hwnd, NULLHANDLE, &rclPaint);

    if (hps)
    {
        // get widget data and its button data from QWL_USER
        PXCENTERWIDGETVIEW pViewData = (PXCENTERWIDGETVIEW)WinQueryWindowPtr(hwnd, QWL_USER);
        if (pViewData)
        {
            PPRIVATEWIDGETDATA pPrivateData = (PPRIVATEWIDGETDATA)pViewData->pUser;
            if (pPrivateData)
            {
                RECTL   rclWin;
                ULONG   ulBorder = 2,
                        cx,
                        cy,
                        cxMiniIcon = pViewData->pGlobals->cxMiniIcon,
                        ulOfs = 0;
                LONG    lLeft,
                        lRight,
                        lMiddle = WinQuerySysColor(HWND_DESKTOP, SYSCLR_BUTTONMIDDLE, 0);

                if (    (pPrivateData->fMouseButton1Down)
                     || (pPrivateData->fButtonSunk)
                   )
                {
                    // paint button "down":
                    lLeft = pViewData->pGlobals->lcol3DDark;
                    lRight = pViewData->pGlobals->lcol3DLight;
                    // add offset for icon painting at the bottom
                    ulOfs += 2;
                }
                else
                {
                    lLeft = pViewData->pGlobals->lcol3DLight;
                    lRight = pViewData->pGlobals->lcol3DDark;
                }

                // now paint button frame
                WinQueryWindowRect(hwnd, &rclWin);
                gpihSwitchToRGB(hps);
                gpihDraw3DFrame(hps,
                                &rclWin,
                                ulBorder,
                                lLeft,
                                lRight);

                // now paint button middle
                rclWin.xLeft += ulBorder;
                rclWin.yBottom += ulBorder;
                rclWin.xRight -= ulBorder;
                rclWin.yTop -= ulBorder;
                WinFillRect(hps,
                            &rclWin,
                            lMiddle);

                // now paint icon
                cx = rclWin.xRight - rclWin.xLeft;
                cy = rclWin.yTop - rclWin.yBottom;
                GpiIntersectClipRectangle(hps, &rclWin);
                WinDrawPointer(hps,
                               // center this in remaining rectl
                               rclWin.xLeft + ((cx - cxMiniIcon) / 2) + ulOfs,
                               rclWin.yBottom + ((cy - cxMiniIcon) / 2) - ulOfs,
                               pPrivateData->hptrXMini,
                               DP_MINI);
            } // end if (pPrivateData)
        } // end if (pViewData)

        WinEndPaint(hps);
    } // end if (hps)
}

/*
 * OwgtButton1Down:
 *      implementation for WM_BUTTON1DOWN in fnwpObjButtonWidget.
 */

VOID OwgtButton1Down(HWND hwnd)
{
    // get widget data and its button data from QWL_USER
    PXCENTERWIDGETVIEW pViewData = (PXCENTERWIDGETVIEW)WinQueryWindowPtr(hwnd, QWL_USER);
    if (pViewData)
    {
        PPRIVATEWIDGETDATA pPrivateData = (PPRIVATEWIDGETDATA)pViewData->pUser;
        if (pPrivateData)
        {
            if (WinIsWindowEnabled(hwnd))
            {
                pPrivateData->fMouseButton1Down = TRUE;
                WinInvalidateRect(hwnd, NULL, FALSE);

                // since we're not passing the message
                // to WinDefWndProc, we need to give
                // ourselves the focus; this will also
                // dismiss the button's menu, if open
                WinSetFocus(HWND_DESKTOP, hwnd);

                if (!pPrivateData->fMouseCaptured)
                {
                    // capture mouse events while the
                    // mouse button is down
                    WinSetCapture(HWND_DESKTOP, hwnd);
                    pPrivateData->fMouseCaptured = TRUE;
                }

                if (!pPrivateData->fButtonSunk)
                {
                    // toggle state is still UP (i.e. button pressed
                    // for the first time): create menu
                    WPDesktop *pActiveDesktop = cmnQueryActiveDesktop();
                    PSZ pszDesktopTitle = _wpQueryTitle(pActiveDesktop);
                    PCKERNELGLOBALS  pKernelGlobals = krnQueryGlobals();
                    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
                    ULONG   ulPosition = 1;

                    // pPrivateData->hwndMenuMain = WinCreateMenu(HWND_DESKTOP, NULL);
                    pPrivateData->hwndMenuMain = WinLoadMenu(hwnd,
                                                            cmnQueryNLSModuleHandle(FALSE),
                                                            ID_CRM_XCENTERBUTTON);
                    if (!pKernelGlobals->pXWPShellShared)
                    {
                        // XWPShell not running:
                        // remove "logoff"
                        winhRemoveMenuItem(pPrivateData->hwndMenuMain,
                                           ID_CRMI_LOGOFF);
                    }

                    // prepare globals in fdrmenus.c
                    cmnuInitItemCache(pGlobalSettings);

                    // prepare folder content submenu for Desktop
                    cmnuPrepareContentSubmenu(pActiveDesktop,
                                              pPrivateData->hwndMenuMain,
                                              pszDesktopTitle,
                                              ulPosition++,
                                              FALSE); // no owner draw in main context menu

                    ctlDisplayButtonMenu(hwnd,
                                         pPrivateData->hwndMenuMain);
                }
            }
        } // end if (pPrivateData)
    } // end if (pViewData)
}

/*
 * OwgtButton1Up:
 *      implementation for WM_BUTTON1UP in fnwpObjButtonWidget.
 */

VOID OwgtButton1Up(HWND hwnd)
{
    // get widget data and its button data from QWL_USER
    PXCENTERWIDGETVIEW pViewData = (PXCENTERWIDGETVIEW)WinQueryWindowPtr(hwnd, QWL_USER);
    if (pViewData)
    {
        PPRIVATEWIDGETDATA pPrivateData = (PPRIVATEWIDGETDATA)pViewData->pUser;
        if (pPrivateData)
        {
            if (WinIsWindowEnabled(hwnd))
            {
                // un-capture the mouse first
                if (pPrivateData->fMouseCaptured)
                {
                    WinSetCapture(HWND_DESKTOP, NULLHANDLE);
                    pPrivateData->fMouseCaptured = FALSE;
                }

                pPrivateData->fMouseButton1Down = FALSE;

                // toggle state with each WM_BUTTON1UP
                pPrivateData->fButtonSunk = !pPrivateData->fButtonSunk;
                WinInvalidateRect(hwnd, NULL, FALSE);
            }
        } // end if (pPrivateData)
    } // end if (pViewData)
}

/*
 * OwgtInitMenu:
 *      implementation for WM_INITMENU in fnwpObjButtonWidget.
 */

VOID OwgtInitMenu(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    SHORT sMenuIDMsg = (SHORT)mp1;
    HWND hwndMenuMsg = (HWND)mp2;

    // find out whether the menu of which we are notified
    // is a folder content menu; if so (and it is not filled
    // yet), the first menu item is ID_XFMI_OFS_DUMMY
    if ((ULONG)WinSendMsg(hwndMenuMsg,
                          MM_ITEMIDFROMPOSITION,
                          (MPARAM)0,        // menu item index
                          MPNULL)
               == (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_DUMMY))
    {
        // okay, let's go
        if (pGlobalSettings->FCShowIcons)
        {
            // show folder content icons ON:

            #ifdef DEBUG_MENUS
                _Pmpf(( "  preparing owner draw"));
            #endif

            cmnuPrepareOwnerDraw(hwndMenuMsg);
        }

        // add menu items according to folder contents
        cmnuFillContentSubmenu(sMenuIDMsg,
                               hwndMenuMsg);
    }
}

/*
 * OwgtMenuEnd:
 *      implementation for WM_MENUEND in fnwpObjButtonWidget.
 */

VOID OwgtMenuEnd(HWND hwnd, MPARAM mp2)
{
    // get widget data and its button data from QWL_USER
    PXCENTERWIDGETVIEW pViewData = (PXCENTERWIDGETVIEW)WinQueryWindowPtr(hwnd, QWL_USER);
    if (pViewData)
    {
        PPRIVATEWIDGETDATA pPrivateData = (PPRIVATEWIDGETDATA)pViewData->pUser;
        if (pPrivateData)
        {
            if ((HWND)mp2 == pPrivateData->hwndMenuMain)
            {
                // main menu is ending:
                if (!pPrivateData->fMouseButton1Down)
                {
                    // mouse button not currently down
                    // --> menu dismissed for some other reason:
                    pPrivateData->fButtonSunk = FALSE;
                    WinInvalidateRect(hwnd, NULL, FALSE);
                }

                WinDestroyWindow(pPrivateData->hwndMenuMain);
                pPrivateData->hwndMenuMain = NULLHANDLE;
            }
        } // end if (pPrivateData)
    } // end if (pViewData)
}

/*
 *@@ OwgtCommand:
 *      implementation for WM_COMMAND in fnwpObjButtonWidget.
 */

BOOL OwgtCommand(HWND hwnd, MPARAM mp1)
{
    BOOL fProcessed = TRUE;
    ULONG ulMenuId = (ULONG)mp1;
    switch (ulMenuId)
    {
        case ID_CRMI_SUSPEND:
        break;

        case ID_CRMI_LOGOFF:
            xsdInitiateRestartWPS(TRUE);    // logoff
        break;

        case ID_CRMI_RESTARTWPS:
            xsdInitiateRestartWPS(FALSE);   // restart WPS, no logoff
        break;

        case ID_CRMI_SHUTDOWN:
            xsdInitiateShutdown();
        break;

        default:
        {
            // other:
            PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
            ULONG ulFirstVarMenuId = pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_VARIABLE;
            if (     (ulMenuId >= ulFirstVarMenuId)
                  && (ulMenuId <  ulFirstVarMenuId + G_ulVarItemCount)
               )
            {
                // yes, variable menu item selected:
                // get corresponding menu list item from the list that
                // was created by mnuModifyFolderPopupMenu
                PVARMENULISTITEM pItem = cmnuGetVarItem(ulMenuId - ulFirstVarMenuId);
                WPObject    *pObject = NULL;

                if (pItem)
                    pObject = pItem->pObject;

                if (pObject)    // defaults to NULL
                    _wpViewObject(pObject, NULLHANDLE, OPEN_DEFAULT, 0);
            } // end if ((ulMenuId >= ID_XFM_VARIABLE) && (ulMenuId < ID_XFM_VARIABLE+varItemCount))
            else
                fProcessed = FALSE;
        break; }
    }

    return (fProcessed);
}

/*
 *@@ fnwpObjButtonWidget:
 *      window procedure for the desktop button widget class.
 *
 *      This is also the owner for the menu it displays and
 *      therefore handles control of the folder content menus.
 */

MRESULT EXPENTRY fnwpObjButtonWidget(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    switch (msg)
    {
        /*
         * WM_CREATE:
         *      as with all widgets, we receive a pointer to the
         *      XCENTERWIDGETVIEW in mp1, which was created for us.
         *
         *      The first thing the widget MUST do on WM_CREATE
         *      is to store the XCENTERWIDGETVIEW pointer (from mp1)
         *      in the QWL_USER window word by calling:
         *
         *          WinSetWindowPtr(hwnd, QWL_USER, mp1);
         *
         *      We use XCENTERWIDGETVIEW.pUser for allocating
         *      PRIVATEWIDGETDATA for our own stuff.
         *
         *      Each widget must write its desired width into
         *      XCENTERWIDGETVIEW.cx and cy.
         */

        case WM_CREATE:
            WinSetWindowPtr(hwnd, QWL_USER, mp1);
            mrc = OwgtCreate(hwnd, mp1);
        break;

        /*
         * WM_PAINT:
         *
         */

        case WM_PAINT:
            OwgtPaintButton(hwnd);
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
            OwgtButton1Down(hwnd);
            mrc = (MPARAM)TRUE;     // message processed
        break;

        /*
         * WM_BUTTON1UP:
         *
         */

        case WM_BUTTON1UP:
            OwgtButton1Up(hwnd);
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
         * WM_INITMENU:
         *
         */

        case WM_INITMENU:
            OwgtInitMenu(hwnd, mp1, mp2);
        break;

        /*
         * WM_MEASUREITEM:
         *      this msg is sent only once per owner-draw item when
         *      PM needs to know its size. This gets sent to us for
         *      items in folder content menus (if icons are on); the
         *      height of our items will be the same as with
         *      non-owner-draw ones, but we need to calculate the width
         *      according to the item text.
         *
         *      (SHORT)mp1 is supposed to contain a "menu identifier",
         *      but from my testing this contains some random value.
         *
         *      Return value: check mnuMeasureItem.
         */

        case WM_MEASUREITEM:
        {
            PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
            mrc = cmnuMeasureItem((POWNERITEM)mp2, pGlobalSettings);
        break; }

        /*
         * WM_DRAWITEM:
         *      this msg is sent for each item every time it
         *      needs to be redrawn. This gets sent to us for
         *      items in folder content menus (if icons are on).
         *
         *      (SHORT)mp1 is supposed to contain a "menu identifier",
         *      but from my testing this contains some random value.
         */

        case WM_DRAWITEM:
        {
            PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
            if (cmnuDrawItem(pGlobalSettings,
                             mp1, mp2))
                mrc = (MRESULT)TRUE;
        break; }

        /*
         * WM_MENUEND:
         *
         */

        case WM_MENUEND:
            OwgtMenuEnd(hwnd, mp2);
            mrc = ctrDefWidgetProc(hwnd, msg, mp1, mp2);
        break;

        /*
         *@@ WM_COMMAND:
         *      handle command from menus.
         */

        case WM_COMMAND:
            if (!OwgtCommand(hwnd, mp1))
                // not processed:
                mrc = ctrDefWidgetProc(hwnd, msg, mp1, mp2);
        break;

        /*
         * WM_DESTROY:
         *      clean up. This _must_ be passed on to
         *      ctrDefWidgetProc.
         */

        case WM_DESTROY:
        {
            PXCENTERWIDGETVIEW pViewData = (PXCENTERWIDGETVIEW)WinQueryWindowPtr(hwnd, QWL_USER);
            if (pViewData)
            {
                PPRIVATEWIDGETDATA pPrivateData = (PPRIVATEWIDGETDATA)pViewData->pUser;
                if (pPrivateData)
                {
                    WinDestroyPointer(pPrivateData->hptrXMini);
                    // free button data
                    free(pPrivateData);
                            // pViewData is cleaned up by DestroyWidgets
                }
            }
            mrc = ctrDefWidgetProc(hwnd, msg, mp1, mp2);
        break; }

        default:
            mrc = ctrDefWidgetProc(hwnd, msg, mp1, mp2);
    } // end switch(msg)

    return (mrc);
}


