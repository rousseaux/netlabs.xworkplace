
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
 *      is that the XCenter @widgets can be dynamically replaced,
 *      even while the XCenter is open.
 *
 *      To write your own widgets, you only need to create a new
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
 *      See @xcenter_instance, @widget, @widget_class, @plugin_dll.
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
 *      -- Each known @widget_class (either one of the built-ins in
 *         XFLDR.DLL or those loaded from a plugin DLL) is described
 *         in an XCENTERWIDGETCLASS structure. An array of those
 *         structures must be returned by a widget @plugin_dll so
 *         that the XCenter can know what plugin classes are in
 *         a DLL.
 *
 *      -- The XCenter instance data contains a linked list of
 *         XCENTERWIDGETSETTING structures, which each describe
 *         a widget which has been configured for display. The
 *         widgets use their own class-specific setup strings
 *         to save and restore their data. This saves the widgets
 *         from having to maintain their own data in OS2.INI.
 *         The XCenter will simply dump all the widget strings
 *         with its other data (for WPS programmers: this happens
 *         in XCenter::wpSaveState).
 *
 *      -- When an XCenter is opened, it creates its widgets.
 *         Each widget then receives an XCENTERWIDGET structure
 *         on WM_CREATE for itself.
 *
 *         This contains all kinds of data specific to the widget.
 *         Other than setting a few of these fields on WM_CREATE,
 *         the XCenter does not care much about what the widget
 *         does -- it has its own PM window and can paint there
 *         like crazy. It is however strongly recommended to
 *         pass all unprocessed messages to ctrDefWidgetProc
 *         instead of WinDefWindowProc to avoid resource leaks.
 *
 *         The widget receives a function pointer to ctrDefWidgetProc
 *         in its XCENTERWIDGET structure (on WM_CREATE). So in
 *         order to be able to pass all unprocessed messages to
 *         ctrDefWidgetProc, the first thing the widget must do
 *         on WM_CREATE is to store the pointer to its XCENTERWIDGET
 *         in its QWL_USER window word (see PMREF).
 *
 *         There is a "pUser" pointer in that structure that the
 *         widget can use for allocating its own data.
 *
 *         For details, see ctrDefWidgetProc.
 *
 *      <B>Widget settings</B>
 *
 *      Again, each widget can store its own data in its window
 *      words (by using the pUser field of XCENTERWIDGET).
 *      This is OK while the widget is running...
 *
 *      Saving the settings is a bit more tricky because there
 *      can be many widgets in many XCenters. Even though a
 *      widget programmer may choose to use a fixed location
 *      in OS2.INI, for example, for saving widget settings,
 *      this isn't such a good idea.
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
 *      strings for the widgets, so the widget should only be
 *      able to do two things:
 *
 *      --  to parse a setup string and set up its binary data
 *          in XCENTERWIDGET on WM_CREATE (and possibly later,
 *          when a settings dialog changes the setup string);
 *
 *      --  to create a new setup string from its binary data
 *          to have that data saved.
 *
 *      XFLDR.DLL exports a few helper functions that plugins can
 *      import to aid in that (see ctrScanSetupString,
 *      ctrParseColorString, ctrFreeSetupValue, and ctrSetSetupString).
 *
 *      Using setup strings has the following advantages:
 *
 *      --  The data is stored together with the XCenter instance.
 *          When the XCenter gets deleted, your data is cleaned
 *          up automatically.
 *
 *      --  The XCenter can produce a single WPS setup string
 *          for itself so that the same XCenter with all its
 *          widgets can be recreated somewhere else.
 *
 *      XCenter also offers support for widget settings dialogs
 *      which work on both open widget settings and from the
 *      XCenter settings notebook itself (even if the XCenter
 *      is not currently open). Such settings dialogs operate
 *      on setup strings only. See WIDGETSETTINGSDLGDATA for
 *      details.
 *
 *      <B>Importing functions</B>
 *
 *      The only function that a widget is really required to
 *      use is ctrDefWidgetProc, whose address is passed to it
 *      on WM_CREATE in the XCENTERWIDGET structure. So there's
 *      no need to import additional functions from XFLDR.DLL.
 *
 *      However, to reduce the DLL's code size, a widget plugin
 *      DLL may import any function that is exported from XFLDR.DLL.
 *      (See the first part of the EXPORTS section in src\shared\xwp.def
 *      for a list of exported functions). In the "init module"
 *      export that is required for plugin DLLs, the plugin DLL
 *      receives the module handle of XFLDR.DLL that it can use
 *      with DosQueryProcAddr to receive a function pointer. See
 *      the samples in src\widgets for how this is done.
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
 *      (settings management, window creation, window reformatting,
 *      DLL management, etc.) is in xcenter\ctr_engine.c. You
 *      better not touch that  if you only want to write a plugin.
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
 *@@gloss: xcenter_instance XCenter instance
 *      An "XCenter instance" is an object of the XCenter WPS
 *      class. There can only be one open view per XCenter so
 *      that an XCenter instance is identical to an XCenter
 *      view when it's open, but there can be several XCenter
 *      instances on the system (i.e. one for each screen border).
 */

/*
 *@@gloss: widget widget
 *      A "widget" is a PM subwindow of an open XCenter which
 *      displays something. An open XCenter can contain zero,
 *      one, or many widgets. Every XCenter keeps a list of
 *      widgets together with their settings in its SOM instance
 *      data. This is totally transparent to the widgets.
 *
 *      Each widget is defined by its @widget_class.
 */

/*
 *@@gloss: widget_class widget class
 *      A "widget class" defines a @widget. Basically, it's a
 *      plain C structure (XCENTERWIDGETCLASS) with a PM window
 *      procedure which is used to create the PM widget windows
 *      (the "widget instances"). Depending on the widget
 *      class's attributes, there can be one or several
 *      instances of a widget class. If you want a different
 *      widget in the XCenter, you need to write a widget class.
 *
 *      A widget class basically boils down to writing a PM
 *      window class, with some additional rules to follow. To
 *      make things easier, several widget classes can share
 *      the same PM class though.
 *
 *      Some widget classes are built into the XCenter itself
 *      (i.e. reside in XFLDR.DLL), but the XCenter can load
 *      them from a @plugin_dll too. Several of the widget classes
 *      that come with XWorkplace have been created as plug-ins to
 *      show you how it's done (see the src\widgets directory).
 */

/*
 *@@gloss: plugin_dll plug-in DLL
 *      A "widget plug-in DLL" is a separate DLL which resides
 *      in the "plugins\xcenter" directory of the XWorkplace
 *      installation directory. This must contain one or
 *      several widget classes. There are a couple of
 *      procedures that a widget plug-in DLL is required to
 *      export to make the XCenter see the widget classes.
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
#include "helpers\except.h"             // exception handling
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\stringh.h"            // string helper routines
#include "helpers\timer.h"              // replacement PM timers

// SOM headers which don't crash with prec. header files
#include "xcenter.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

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
 *      This assumes that semicola (';') chars are used to
 *      separate the key/value pairs. You are not required
 *      to use that, but this function won't work otherwise,
 *      and you'll have to write your own parser then.
 *
 *      Example:
 +
 +          PSZ pszSetupString = "key1=value1;key2=value2;"
 +          PSZ pszValue = ctrScanSetupString(pszSetupString,
 +                                            "key1");
 +          ...
 +          ctrFreeSetupValue(pszValue);
 *
 *      would have returned "value1".
 *
 *      This searches for the keyword _without_ respect
 *      to case. It is recommended to use upper-case
 *      keywords only.
 */

PSZ ctrScanSetupString(const char *pcszSetupString, // in: entire setup string
                       const char *pcszKeyword)     // in: keyword to look for (e.g. "FONT")
{
    PSZ pszValue = 0;
    if ((pcszSetupString) && (pcszKeyword))
    {
        ULONG       ulKeywordLen = strlen(pcszKeyword);
        const char  *pKeywordThis = pcszSetupString,
                    *pEquals = strchr(pcszSetupString, '=');

        while (pEquals)
        {
            if (strnicmp(pKeywordThis, pcszKeyword, ulKeywordLen) == 0)
            {
                // keyword found:
                // get value
                const char *pEOValue = strchr(pEquals, ';');
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
            pKeywordThis = strchr(pEquals, ';');
            if (!pKeywordThis)
                // was last keyword:
                break; // while
            else
            {
                // not last keyword: search on after separator
                pKeywordThis++;
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
 *      If the affected widget is currently open, it
 *      will be sent a WM_CONTROL message with the
 *      ID_XCENTER_CLIENT notification code.
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
        PXCENTERWIDGET pWidget = pSettingsTemp->pWidget;
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
            if (pWidget)
            {
                // yes:
                // send notification
                WinSendMsg(pWidget->hwndWidget,
                           WM_CONTROL,
                           MPFROM2SHORT(ID_XCENTER_CLIENT,
                                        XN_SETUPCHANGED),
                           (MPARAM)pSetting->pszSetupString);
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
    PXCENTERWIDGET pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER);
    if (pWidget)
    {
        HWND hwndClient = pWidget->pGlobals->hwndClient;
        PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(hwndClient, QWL_USER);
        if (pXCenterData)
        {
            PXCENTERWIDGETCLASS pClass = ctrpFindClass(pWidget->pcszWidgetClass);
            if (pClass)
            {
                POINTL  ptl;
                WinQueryPointerPos(HWND_DESKTOP, &ptl);

                // enable "properties" if class has show-settings proc
                WinEnableMenuItem(pWidget->hwndContextMenu,
                                  ID_CRMI_PROPERTIES,
                                  (pClass->pShowSettingsDlg != 0));

                // enable "help" if widget has specified help
                WinEnableMenuItem(pWidget->hwndContextMenu,
                                  ID_CRMI_HELP,
                                  (    (pWidget->pcszHelpLibrary != NULL)
                                    && (pWidget->ulHelpPanelID != 0)
                                  ));

                // draw source emphasis around widget
                ctrpDrawEmphasis(pXCenterData,
                                 hwnd,
                                 FALSE,     // draw, not remove emphasis
                                 NULLHANDLE);   // standard PS

                // show menu!!
                WinPopupMenu(HWND_DESKTOP,
                             hwnd,
                             pWidget->hwndContextMenu,
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
    PXCENTERWIDGET pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER);
    if (pWidget)
    {
        if (hwndMenu == pWidget->hwndContextMenu)
        {
            WinInvalidateRect(pWidget->pGlobals->hwndClient, NULL, FALSE);
            WinInvalidateRect(pWidget->pGlobals->hwndFrame, NULL, FALSE);
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
    PXCENTERWIDGET pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER);
    if (pWidget)
    {
        HWND hwndClient = pWidget->pGlobals->hwndClient;
        PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(hwndClient, QWL_USER);
        if (pWidget)
        {
            switch (usCmd)
            {
                case ID_CRMI_PROPERTIES:
                    // widget has a settings dialog:
                    // have the widget show it with the XCenter frame
                    // as its owner
                    ctrpShowSettingsDlg(pXCenterData,
                                        pWidget);
                break;

                case ID_CRMI_HELP:
                    if (    (pWidget->pcszHelpLibrary)
                         && (pWidget->ulHelpPanelID)
                       )
                    {
                        // widget has specified help itself:
                        _wpDisplayHelp(pXCenterData->somSelf,
                                       pWidget->ulHelpPanelID,
                                       (PSZ)pWidget->pcszHelpLibrary);
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
                {
                    ULONG ulMyIndex = ctrpQueryWidgetIndexFromHWND(pXCenterData->somSelf,
                                                                   hwnd);
                    _xwpRemoveWidget(pXCenterData->somSelf,
                                     ulMyIndex);
                break; }
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
    PXCENTERWIDGET pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER);
    if (pWidget)
    {
        HWND hwndClient = pWidget->pGlobals->hwndClient;
        PXCENTERWINDATA pXCenterData
            = (PXCENTERWINDATA)WinQueryWindowPtr(hwndClient,
                                                 QWL_USER);
        if (pXCenterData)
        {
            WPSHLOCKSTRUCT Lock;
            if (wpshLockObject(&Lock, pXCenterData->somSelf))
            {
                // stop all running timers
                // tmrStopAllTimers(hwnd);

                if (pXCenterData)
                {
                    // remove the widget from the list of open
                    // views in the XCenter... the problem here
                    // is that we only see the XCENTERWIDGET
                    // struct, which is really part of the
                    // WIDGETVIEWSTATE structure in the llWidgets
                    // list... so XCENTERWIDGET must be the first
                    // member of the WIDGETVIEWSTATE structure!
                    if (!lstRemoveItem(&pXCenterData->llWidgets,
                                       pWidget))
                        cmnLog(__FILE__, __LINE__, __FUNCTION__,
                               "lstRemoveItem failed.");
                }

                if (pWidget->pcszWidgetClass)
                {
                    free((PSZ)pWidget->pcszWidgetClass);
                    pWidget->pcszWidgetClass = NULL;
                }

                if (pWidget->hwndContextMenu)
                {
                    WinDestroyWindow(pWidget->hwndContextMenu);
                    pWidget->hwndContextMenu = NULLHANDLE;
                }

                free(pWidget);
                WinSetWindowPtr(hwnd, QWL_USER, 0);

                WinPostMsg(hwndClient,
                           XCM_REFORMAT,
                           (MPARAM)XFMF_REPOSITIONWIDGETS,
                           0);
            }

            wpshUnlockObject(&Lock);
        }
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
 *         its XCENTERWIDGET structure in mp1.
 *
 *         The first thing the widget MUST do on WM_CREATE
 *         is to store the XCENTERWIDGET pointer (from mp1)
 *         in its QWL_USER window word by calling:
 *
 +              WinSetWindowPtr(hwnd, QWL_USER, mp1);
 *
 *      -- The XCenter communicates with the widget using
 *         WM_CONTROL messages. SHORT1FROMMP(mp1), the
 *         source window ID, is always ID_XCENTER_CLIENT.
 *         SHORT2FROMMP(mp1), the notification code, can
 *         be:
 *
 *         --  XN_QUERYSIZE: the XCenter wants to know the
 *             widget's size.
 *
 *         --  XN_SETUPCHANGED: widget's setup string has
 *             changed.
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
 *         XCENTERWIDGET.hwndContextMenu, you must use
 *         menu item IDs >= 1000.
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
 *@@changed V0.9.9 (2001-03-01) [lafaix]: added XCM_* handling
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

/*
 *@@ ctrDefContainerWidgetProc:
 *      default window procedure for container widgets. This is
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
 *         its XCENTERWIDGET structure in mp1.
 *
 *         The first thing the widget MUST do on WM_CREATE
 *         is to store the XCENTERWIDGET pointer (from mp1)
 *         in its QWL_USER window word by calling:
 *
 +              WinSetWindowPtr(hwnd, QWL_USER, mp1);
 *
 *      -- All unprocessed messages should be routed
 *         to ctrDefContainerWidgetProc instead of WinDefWindowProc.
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
 *         XCENTERWIDGET.hwndContextMenu, you must use
 *         menu item IDs >= 1000.
 *
 *@@added V0.9.9 (2001-02-27) [lafaix]
 */

MRESULT EXPENTRY ctrDefContainerWidgetProc(HWND hwnd,
                                           ULONG msg,
                                           MPARAM mp1,
                                           MPARAM mp2)
{
    MRESULT mrc = 0;

    switch (msg)
    {
        // **lafaix: these used to be your messages here

        default:
            mrc = ctrDefWidgetProc(hwnd, msg, mp1, mp2);
    }

    return (mrc);
}


