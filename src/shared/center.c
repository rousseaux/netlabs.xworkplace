
/*
 *@@sourcefile center.c:
 *      shared code for the XCenter implementation. This code
 *      has been declared "shared" because many different
 *      components rely on it -- for example, the widget plugin
 *      DLLs.
 *
 *      The XCenter is a WPAbstract subclass which creates an
 *      almost standard PM frame/client pair for its default view
 *      (on XCenter::wpOpen, which in turn calls ctrpCreateXCenterView).
 *
 *      The nice thing about the XCenter (compared to the WarpCenter)
 *      is that the XCenter "widgets" (that's how we call the things
 *      which are inside the XCenter bar) can be dynamically replaced,
 *      even while the XCenter is open.
 *
 *      To write your own widget, you only need to create a new
 *      standard PM window class which follows a few rules to
 *      interface with the XCenter. The XCenter is even capable
 *      of dynamically loading and unloading plugin widgets from a DLL.
 *      Best of all, it will automatically free the DLL when it's
 *      no longer used (e.g. when all XCenters are closed) so that
 *      WPS restarts are _not_ needed for writing plugins.
 *
 *      See fnwpXCenterMainClient for a description of the XCenter
 *      window hierarchy.
 *
 *      <B>Terminology</B>
 *
 *      -- An "XCenter instance" is an object of the XCenter WPS
 *         class. There can only be one open view per XCenter so
 *         that an XCenter instance is identical to an XCenter
 *         view when it's open, but there can be several XCenter
 *         instances on the system (i.e. one for each screen border).
 *
 *      -- A "widget" is a subwindow of an open XCenter which
 *         displays something. An open XCenter can contain zero,
 *         one, or many widgets. Every XCenter keeps a list of
 *         widgets together with their settings in its SOM instance
 *         data. This is totally transparent to the widgets.
 *
 *      -- A "widget class" defines a widget. Depending on the
 *         widget class's attributes, there can be one or several
 *         instances of a widget class (the widgets themselves).
 *         If you want a different widget in the XCenter, you need
 *         to write a widget class.
 *
 *         A widget class basically boils down to writing a PM
 *         window class, with additional rules to follow. To
 *         make things easier, several widget classes can share
 *         the same PM class though.
 *
 *         Some widget classes are built into the XCenter itself
 *         (i.e. reside in XFLDR.DLL), but the XCenter can load
 *         external DLLs too.
 *
 *      -- A "widget plugin DLL" is a separate DLL which resides
 *         in the "plugins\xcenter" directory of the XWorkplace
 *         installation directory. This must contain one or
 *         several widget classes. There are a couple of
 *         procedures that a widget plugin DLL is required to
 *         export to make the XCenter see the widget classes.
 *
 *      <B>Structures</B>
 *
 *      There are a bunch of structures which are used for communication
 *      between the various windows and classes.
 *
 *      -- On XCenter view creation, an XCENTERGLOBALS structure is
 *         created in the XCenter's window words, which is later
 *         made public to the widgets as well.
 *
 *      -- Each known widget class (either one of the built-ins in
 *         this file or those loaded from a plugin DLL) is described
 *         in an XCENTERWIDGETCLASS structure. An array of those
 *         structures must be returned by widget plugin DLLs so
 *         that the XCenter can know what plugin classes are in
 *         a DLL.
 *
 *      -- The XCenter instance data contains a linked list of
 *         XCENTERWIDGETSETTING structures, which each describe
 *         a widget which has been configured for display. The
 *         widgets use their own class-specific setup strings
 *         to save and restore their data. This saves the widgets
 *         from having to maintain their own data in OS2.INI;
 *         the XCenter will simply dump all the widget strings
 *         with XCenter::wpSaveState.
 *
 *      -- While the XCenter is open, each widget receives an
 *         XCENTERWIDGETVIEW structure on WM_CREATE for itself.
 *         This contains all kinds of data specific to the widget.
 *         Other than setting a few of these fields on WM_CREATE,
 *         the XCenter does not care much about what the widget
 *         does -- it has its own PM window and can paint there
 *         like crazy. It is however strongly recommended to
 *         pass all unprocessed messages to ctrDefWidgetProc
 *         instead of WinDefWindowProc to avoid resource leaks.
 *
 *         After WM_CREATE, the XCenter stores a pointer to the
 *         XCENTERWIDGETVIEW in the QWL_USER window word of the
 *         widget (see PMREF). There is a "pUser" pointer in
 *         that structure that the widget can use for allocating
 *         its own data.
 *
 *         For details, see ctrDefWidgetProc.
 *
 *      <B>Widget settings</B>
 *
 *      Again, each widget can store its own data in its window
 *      words. This is no problem while the widget is running.
 *      Saving the settings is a bit more tricky because there
 *      can be many widgets in many XCenters, so a fixed location
 *      in OS2.INI, for example, for saving widget settings isn't
 *      such a good idea.
 *
 *      The XCenter offers widgets to store their data together
 *      with the XCenter instance settings as a "setup string".
 *      These look similar to regular WPS setup strings (in the
 *      "keyword1=value1" form), even though the widgets are not
 *      WPS objects themselves.
 *
 *      Since the XCenter is derived from WPAbstract, if a widget
 *      chooses to do so, its instance data ends up somewhere in
 *      OS2.INI (thru the regular wpSaveState/wpRestoreState
 *      mechanism). The XCenter takes care of unpacking the setup
 *      strings for the widgets, so all the widgets has to do
 *      is convert its binary data into a string and set itself
 *      up according to a setup string. XFLDR.DLL exports a few
 *      helper functions that plugins can import to aid in that
 *      (see ctrScanSetupString, ctrParseColorString, ctrFreeSetupValue,
 *      and ctrSetSetupString).
 *
 *      Setup strings can _not_ however
 *
 *      --  use a semicolon for separating the parts; instead,
 *          use some other character. If you use the built-in
 *          helpers, you must use the SETUP_SEPARATOR separator
 *          defined in shared\center.h.
 *
 *      --  use "=" in their data, because this is used to
 *          separate keywords and data.
 *
 *      Only by following these rules, the setup strings can be
 *      imported and exported together with the other WPS
 *      setup strings of an XCenter. That is, the complete
 *      XCenter setup with all widgets and their settings can
 *      be described in a setup string.
 *
 *      XCenter also offers support for widget settings dialogs
 *      which work on both open widget settings and from the
 *      XCenter settings notebook itself (even if the XCenter
 *      is not currently open). Such settings dialogs operate
 *      on setup strings only. See WIDGETSETTINGSDLGDATA for
 *      details.
 *
 *      <B>Where is what?</B>
 *
 *      This file (shared\center.c) "only" contains things which
 *      are of interest for widget DLLs. This has mostly
 *      functions which are exported from XFLDR.DLL so that plugins
 *      can import them for convenience. Besides, this has the
 *      default widget window proc (ctrDefWidgetProc).
 *
 *      The "engine" of the XCenter which does all the hard stuff
 *      is in xcenter\ctr_engine.c. You better not touch that if
 *      you only want to write a plugin.
 *
 *      Function prefix for this file:
 *      --  ctr*
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
#define INCL_DOSMODULEMGR
#define INCL_DOSEXCEPTIONS
#define INCL_DOSSEMAPHORES
#define INCL_DOSERRORS

#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WININPUT
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
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\stringh.h"            // string helper routines
#include "helpers\timer.h"              // replacement PM timers

// SOM headers which don't crash with prec. header files
#include "xcenter.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file

#include "shared\center.h"              // public XCenter interfaces
#include "xcenter\centerp.h"            // private XCenter implementation

#pragma hdrstop                     // VAC++ keeps crashing otherwise

/* ******************************************************************
 *
 *   Public widget helpers (exported from XFLDR.DLL)
 *
 ********************************************************************/

/*
 *@@ ctrScanSetupString:
 *      helper function exported from XFLDR.DLL.
 *
 *      A widget can use this helper to get the value
 *      which has been set with a keyword in a widget
 *      setup string.
 *
 *      Returns the value for the keyboard in a new
 *      buffer (which should be freed using ctrFreeSetupValue),
 *      or NULL if not found.
 *
 *      This assumes that SETUP_SEPARATOR is used to
 *      separate the key/value pairs. You are not required
 *      to use SETUP_SEPARATOR (as long as you don't use
 *      semicola in the setup string), but this function
 *      won't work otherwise, and you'll have to write
 *      your own parser then.
 *
 *      Example:
 +
 +          PSZ pszSetupString = "key1=value1" SETUP_SEPARATOR "key2=value2;"
 +          PSZ pszValue = ctrScanSetupString(pszSetupString,
 +                                            "key1");
 +          ...
 +          ctrFreeSetupValue(pszValue);
 *
 *      would have returned "value1".
 *
 *      This searches for the keyword _without_ respect
 *      to case.
 */

PSZ ctrScanSetupString(const char *pcszSetupString, // in: entire setup string
                       const char *pcszKeyword)     // in: keyword to look for (e.g. "FONT")
{
    PSZ pszValue = 0;
    if ((pcszSetupString) && (pcszKeyword))
    {
        ULONG       ulKeywordLen = strlen(pcszKeyword);
        ULONG       ulSepLen = strlen(SETUP_SEPARATOR);
        const char  *pKeywordThis = pcszSetupString,
                    *pEquals = strchr(pcszSetupString, '=');

        while (pEquals)
        {
            if (strnicmp(pKeywordThis, pcszKeyword, ulKeywordLen) == 0)
            {
                // keyword found:
                // get value
                const char *pEOValue = strstr(pEquals, SETUP_SEPARATOR);
                if (pEOValue)
                    // value is before another separator:
                    pszValue = strhSubstr(pEquals + 1, pEOValue);
                else
                    // ";" not found: use rest of string (0 byte)
                    pszValue = strdup(pEquals + 1);

                // we got what we need; exit
                break;
            }

            // else not our keyword:
            // go on
            pKeywordThis = strstr(pEquals, SETUP_SEPARATOR);
            if (!pKeywordThis)
                // was last keyword:
                break; // while
            else
            {
                // not last keyword: search on after separator
                pKeywordThis += ulSepLen;
                pEquals = strchr(pKeywordThis, '=');
            }

        } // end while (pEquals)
    }

    return (pszValue);
}

/*
 *@@ ctrParseColorString:
 *      helper function exported from XFLDR.DLL.
 *
 *      Returns an RGB LONG value for the color
 *      string pointed to by p. That value can
 *      be directly used with an HPS in RGB mode
 *      (see GpiCreateLogColorTable).
 *
 *      "p" is expected to point to a six-digit
 *      hex string in the "RRGGBB" format.
 */

LONG ctrParseColorString(const char *p)
{
    LONG lrc = 0;
    if (p)
        lrc = strtol(p, NULL, 16);
    return (lrc);
}

/*
 *@@ ctrFreeSetupValue:
 *      helper function exported from XFLDR.DLL.
 *
 *      Frees a value returned by ctrScanSetupString.
 */

VOID ctrFreeSetupValue(PSZ p)
{
    free(p);
}

/*
 *@@ ctrSetSetupString:
 *      helper function exported from XFLDR.DLL.
 *
 *      This helper must be invoked to change the
 *      setup string for a widget in the context
 *      of a widget settings dialog.
 *
 *      See WIDGETSETTINGSDLGDATA for more about
 *      settings dialogs.
 *
 *      This only works while a widget settings
 *      dialog is open. This separate function is
 *      needed because a settings dialog can be
 *      open even if the XCenter is not, and sending
 *      the XCM_SAVESETUP message wouldn't work then.
 *
 *      hSetting is passed to a setup dialog procedure
 *      in its WIDGETSETTINGSDLGDATA structure. This
 *      identifies the widget settings internally and
 *      better be valid.
 *
 *      If the affected widget is currently open and
 *      supports updating itself according to a setup
 *      string (i.e. if XCENTERWIDGETVIEW.pScanSetupString
 *      is != NULL), that function gets called from here.
 *
 *@@added V0.9.7 (2000-12-07) [umoeller]
 */

BOOL ctrSetSetupString(LHANDLE hSetting,
                       const char *pcszNewSetupString)
{
    BOOL brc = FALSE;
    // get pointer to structure on stack in ShowSettingsDlg;
    // this identifies the setting...
    PWGTSETTINGSTEMP pSettingsTemp = (PWGTSETTINGSTEMP)hSetting;
    if (pSettingsTemp)
    {
        PXCENTERWINDATA pXCenterData = pSettingsTemp->pXCenterData;
        PXCENTERWIDGETSETTING pSetting = pSettingsTemp->pSetting;
        PXCENTERWIDGETVIEW pViewData = pSettingsTemp->pViewData;
        if (pSetting)
        {
            // change setup string in the settings structure
            if (pSetting->pszSetupString)
            {
                // we already had a setup string:
                free(pSetting->pszSetupString);
                pSetting->pszSetupString = NULL;
            }

            if (pcszNewSetupString)
                pSetting->pszSetupString = strdup(pcszNewSetupString);

            brc = TRUE;

            // do we have an open view?
            if (pViewData)
            {
                // yes:
                if (pViewData->pSetupStringChanged)
                    // updating supported:
                    pViewData->pSetupStringChanged(pViewData,
                                                   pSetting->pszSetupString);
            }

            _wpSaveDeferred(pXCenterData->somSelf);
        }
    }

    return (brc);
}

/*
 *@@ ctrDisplayHelp:
 *      helper function exported from XFLDR.DLL.
 *
 *      Displays the given help panel in the given
 *      help file. This saves widgets from having
 *      to maintain their own help instance.
 *
 *@@added V0.9.7 (2000-12-07) [umoeller]
 */

BOOL ctrDisplayHelp(PXCENTERGLOBALS pGlobals,
                    const char *pcszHelpFile,       // in: help file (can be q'fied)
                    ULONG ulHelpPanelID)
{
    BOOL brc = FALSE;
    if (pGlobals)
    {
        PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(pGlobals->hwndClient,
                                                                          QWL_USER);
        if (pXCenterData)
        {
            brc = _wpDisplayHelp(pXCenterData->somSelf,
                                 ulHelpPanelID,
                                 (PSZ)pcszHelpFile);
        }
    }

    return (brc);
}

/* ******************************************************************
 *
 *   Default widget window proc (exported from XFLDR.DLL)
 *
 ********************************************************************/

/*
 *@@ DwgtContextMenu:
 *      implementation for WM_CONTEXTMENU in ctrDefWidgetProc.
 */

VOID DwgtContextMenu(HWND hwnd)
{
    PXCENTERWIDGETVIEW pViewData = (PXCENTERWIDGETVIEW)WinQueryWindowPtr(hwnd, QWL_USER);
    if (pViewData)
    {
        HWND hwndClient = pViewData->pGlobals->hwndClient;
        PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(hwndClient, QWL_USER);
        if (pXCenterData)
        {
            PXCENTERWIDGETCLASS pClass = ctrpFindClass(pXCenterData->somSelf,
                                                       pViewData->pcszWidgetClass);
            if (pClass)
            {
                POINTL  ptl;
                WinQueryPointerPos(HWND_DESKTOP, &ptl);

                // enable "properties" if class has show-settings proc
                WinEnableMenuItem(pViewData->hwndContextMenu,
                                  ID_CRMI_PROPERTIES,
                                  (pClass->pShowSettingsDlg != 0));

                // enable "help" if widget has specified help
                WinEnableMenuItem(pViewData->hwndContextMenu,
                                  ID_CRMI_HELP,
                                  (    (pViewData->pcszHelpLibrary != NULL)
                                    && (pViewData->ulHelpPanelID != 0)
                                  ));

                // draw source emphasis around widget
                ctrpDrawEmphasis(pXCenterData,
                                 hwnd,
                                 FALSE,     // draw, not remove emphasis
                                 NULLHANDLE);   // standard PS

                // show menu!!
                WinPopupMenu(HWND_DESKTOP,
                             hwnd,
                             pViewData->hwndContextMenu,
                             ptl.x,
                             ptl.y,
                             0,
                             PU_HCONSTRAIN | PU_VCONSTRAIN | PU_MOUSEBUTTON1
                                | PU_MOUSEBUTTON2 | PU_KEYBOARD);
            }
        }
    }
}

/*
 *@@ DwgtMenuEnd:
 *      implementation for WM_MENUEND in ctrDefWidgetProc.
 */

VOID DwgtMenuEnd(HWND hwnd,
                 HWND hwndMenu)
{
    PXCENTERWIDGETVIEW pViewData = (PXCENTERWIDGETVIEW)WinQueryWindowPtr(hwnd, QWL_USER);
    if (pViewData)
    {
        if (hwndMenu == pViewData->hwndContextMenu)
        {
            WinInvalidateRect(pViewData->pGlobals->hwndClient, NULL, FALSE);
            WinInvalidateRect(pViewData->pGlobals->hwndFrame, NULL, FALSE);
        }
    }
}

/*
 *@@ DwgtCommand:
 *      implementation for WM_COMMAND in ctrDefWidgetProc.
 */

VOID DwgtCommand(HWND hwnd,
                 USHORT usCmd)
{
    PXCENTERWIDGETVIEW pViewData = (PXCENTERWIDGETVIEW)WinQueryWindowPtr(hwnd, QWL_USER);
    if (pViewData)
    {
        HWND hwndClient = pViewData->pGlobals->hwndClient;
        PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(hwndClient, QWL_USER);
        ULONG ulMyIndex = -1;
        if (pViewData)
        {
            switch (usCmd)
            {
                case ID_CRMI_PROPERTIES:
                    // widget has a settings dialog:
                    // have the widget show it with the XCenter frame
                    // as its owner
                    ctrpShowSettingsDlg(pXCenterData,
                                        pViewData);
                break;

                case ID_CRMI_HELP:
                    if (    (pViewData->pcszHelpLibrary)
                         && (pViewData->ulHelpPanelID)
                       )
                    {
                        // widget has specified help itself:
                        _wpDisplayHelp(pXCenterData->somSelf,
                                       pViewData->ulHelpPanelID,
                                       (PSZ)pViewData->pcszHelpLibrary);
                    }
                    else
                    {
                        // use XCenter help
                        ULONG ulPanel;
                        CHAR szHelp[CCHMAXPATH];
                        if (_wpQueryDefaultHelp(pXCenterData->somSelf,
                                                &ulPanel,
                                                szHelp))
                            _wpDisplayHelp(pXCenterData->somSelf,
                                           ulPanel,
                                           szHelp);
                    }
                break;

                case ID_CRMI_REMOVEWGT:
                    ulMyIndex = ctrpQueryWidgetIndexFromHWND(pXCenterData->somSelf,
                                                             hwnd);
                    _xwpRemoveWidget(pXCenterData->somSelf,
                                     ulMyIndex);
                break;
            }
        }
    }
}

/*
 *@@ DwgtDestroy:
 *      implementation for WM_DESTROY in ctrDefWidgetProc.
 */

VOID DwgtDestroy(HWND hwnd)
{
    PXCENTERWIDGETVIEW pViewData = (PXCENTERWIDGETVIEW)WinQueryWindowPtr(hwnd, QWL_USER);
    if (pViewData)
    {
        HWND hwndClient = pViewData->pGlobals->hwndClient;
        PXCENTERWINDATA pXCenterData
            = (PXCENTERWINDATA)WinQueryWindowPtr(hwndClient,
                                                 QWL_USER);

        // stop all running timers
        tmrStopAllTimers(hwnd);

        if (pXCenterData)
        {
            if (!lstRemoveItem(&pXCenterData->llWidgetsLeft,
                               pViewData))
                cmnLog(__FILE__, __LINE__, __FUNCTION__,
                       "lstRemoveItem failed.");
        }

        if (pViewData->pcszWidgetClass)
        {
            free((PSZ)pViewData->pcszWidgetClass);
            pViewData->pcszWidgetClass = NULL;
        }

        if (pViewData->hwndContextMenu)
        {
            WinDestroyWindow(pViewData->hwndContextMenu);
            pViewData->hwndContextMenu = NULLHANDLE;
        }

        free(pViewData);
        WinSetWindowPtr(hwnd, QWL_USER, 0);

        WinPostMsg(hwndClient,
                   XCM_REFORMATALL,
                   0, 0);
    }
}

/*
 *@@ ctrDefWidgetProc:
 *      default window procedure for widgets. This is
 *      exported from XFLDR.DLL.
 *
 *      There are a few rules which must be followed
 *      with the window procedures of the widget classes:
 *
 *      -- At the very least, the widget's window proc must
 *         implement WM_CREATE, WM_PAINT, and (if cleanup
 *         is necessary) WM_DESTROY. WM_PRESPARAMCHANGED
 *         would also be nice to support fonts and colors
 *         dropped on the widget.
 *
 *      -- On WM_CREATE, the widget receives a pointer to
 *         its XCENTERWIDGETVIEW structure in mp1. After
 *         the window has been successfully created, XCenter
 *         stores that pointer in QWL_USER of the widget's
 *         window words.
 *
 *      -- On WM_CREATE, the widget should write its desired
 *         width into XCENTERWIDGETVIEW.cx. It can also
 *         write the minimum height it requires into the
 *         cy field (for example, if you want your control
 *         to have at least the system icon size or
 *         something).
 *
 *         After all widgets have been created, the XCenter
 *         (and all widgets) are resized to have the largest
 *         cy requested. As a result, your window proc cannot
 *         assume that it will always have the size it
 *         requested.
 *
 *      -- All unprocessed messages should be routed
 *         to ctrDefWidgetProc instead of WinDefWindowProc.
 *
 *      -- WM_MENUEND must _always_ be passed on after your
 *         own processing (if any) to remove source emphasis
 *         for popup menus.
 *
 *      -- WM_DESTROY must _always_ be passed on after
 *         your own cleanup code to avoid memory leaks,
 *         because this function performs important cleanup
 *         as well.
 *
 *      -- WM_COMMAND command values below 1000 are reserved.
 *         If you extend the context menu given to you in
 *         XCENTERWIDGETVIEW.hwndContextMenu, you must use
 *         menu item IDs >= 1000.
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
 */

MRESULT EXPENTRY ctrDefWidgetProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    switch (msg)
    {
        /*
         * WM_CONTEXTMENU:
         *
         */

        case WM_CONTEXTMENU:
            DwgtContextMenu(hwnd);
        break;

        /*
         * WM_MENUEND:
         *
         */

        case WM_MENUEND:
            DwgtMenuEnd(hwnd, (HWND)mp2);
        break;

        /*
         * WM_COMMAND:
         *
         */

        case WM_COMMAND:
            DwgtCommand(hwnd, (USHORT)mp1);
        break;

        /*
         * WM_DESTROY:
         *
         */

        case WM_DESTROY:
            DwgtDestroy(hwnd);
        break;

        default:
            mrc = WinDefWindowProc(hwnd, msg, mp1, mp2);
    }

    return (mrc);
}


