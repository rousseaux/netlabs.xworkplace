
/*
 *@@sourcefile xwpscreen.c:
 *      This file contains SOM code for the following XWorkplace classes:
 *
 *      --  XWPScreen (WPSystem subclass)
 *
 *      XFldSystem implements the "Screen" settings object
 *      for PM manipulation and PageMage configuration.
 *
 *      Installation of this class is optional.
 *
 *      This is all new with V0.9.2 (2000-02-23) [umoeller].
 *
 *      The implementation for this class is mostly in config\screen.c.
 *
 *@@added V0.9.2 (2000-02-23) [umoeller]
 *@@somclass XWPScreen xwpscr_
 *@@somclass M_XWPScreen xwpscrM_
 */

/*
 *      Copyright (C) 2000 Ulrich M�ller.
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
 *  This file was generated by the SOM Compiler and Emitter Framework.
 *  Generated using:
 *      SOM Emitter emitctm: 2.41
 */

#ifndef SOM_Module_xwpscreen_Source
#define SOM_Module_xwpscreen_Source
#endif
#define XWPScreen_Class_Source
#define M_XWPScreen_Class_Source

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

#define INCL_DOSSEMAPHORES
#define INCL_DOSERRORS
#define INCL_WINWINDOWMGR
#include <os2.h>

// C library headers

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\dosh.h"               // Control Program helper routines

// SOM headers which don't crash with prec. header files
#include "xwpscreen.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling

// other SOM headers
#pragma hdrstop                         // VAC++ keeps crashing otherwise
// #include "xfobj.h"

/* ******************************************************************
 *                                                                  *
 *   here come the XWPScreen instance methods                       *
 *                                                                  *
 ********************************************************************/

/*
 *@@ xwpAddXWPScreenPages:
 *
 */

SOM_Scope ULONG  SOMLINK xwpscr_xwpAddXWPScreenPages(XWPScreen *somSelf,
                                                     HWND hwndDlg)
{
    /* XWPScreenData *somThis = XWPScreenGetData(somSelf); */
    XWPScreenMethodDebug("XWPScreen","xwpscr_xwpAddXWPScreenPages");

    /* Return statement to be customized: */
    return (TRUE);
}

/*
 *@@ wpFilterPopupMenu:
 *      remove "Create another" menu item.
 *
 *@@added V0.9.2 (2000-02-26) [umoeller]
 */

SOM_Scope ULONG  SOMLINK xwpscr_wpFilterPopupMenu(XWPScreen *somSelf,
                                                  ULONG ulFlags,
                                                  HWND hwndCnr,
                                                  BOOL fMultiSelect)
{
    /* XWPScreenData *somThis = XWPScreenGetData(somSelf); */
    XWPScreenMethodDebug("XWPScreen","xwpscr_wpFilterPopupMenu");

    return (XWPScreen_parent_WPSystem_wpFilterPopupMenu(somSelf,
                                                        ulFlags,
                                                        hwndCnr,
                                                        fMultiSelect)
            & ~CTXT_NEW
           );
}

/*
 *@@ wpQueryDefaultHelp:
 *
 */

SOM_Scope BOOL  SOMLINK xwpscr_wpQueryDefaultHelp(XWPScreen *somSelf,
                                                  PULONG pHelpPanelId,
                                                  PSZ HelpLibrary)
{
    /* XWPScreenData *somThis = XWPScreenGetData(somSelf); */
    XWPScreenMethodDebug("XWPScreen","xwpscr_wpQueryDefaultHelp");

    strcpy(HelpLibrary, cmnQueryHelpLibrary());
    *pHelpPanelId = ID_XSH_XWPSCREEN;
    return (TRUE);
}

/*
 *@@ wpAddSettingsPages:
 *      this instance method is overridden in order
 *      to add the new XWorkplace pages to the settings
 *      notebook.
 *      In order to to this, unlike the procedure used in
 *      the "Workplace Shell" object, we will explicitly
 *      call the WPSystem methods which insert the
 *      pages we want to see here into the notebook.
 *      As a result, the "Workplace Shell" object
 *      inherits all pages from the "System" object
 *      which might be added by other WPS utils, while
 *      this thing does not.
 *
 */

SOM_Scope BOOL  SOMLINK xwpscr_wpAddSettingsPages(XWPScreen *somSelf,
                                                  HWND hwndNotebook)
{
    /* XWPScreenData *somThis = XWPScreenGetData(somSelf); */
    XWPScreenMethodDebug("XWPScreen","xwpscr_wpAddSettingsPages");

    // do _not_ call the parent, but call the page methods
    // explicitly

    // XFolder "Internals" page bottommost
    // _xwpAddObjectInternalsPage(somSelf, hwndNotebook);

    // "Symbol" page next
    _wpAddObjectGeneralPage(somSelf, hwndNotebook);
        // this inserts the "Internals"/"Object" page now

    // "print screen" page next
    _wpAddSystemPrintScreenPage(somSelf, hwndNotebook);

    // XWorkplace screen pages
    _xwpAddXWPScreenPages(somSelf, hwndNotebook);

    // "Screen" page 2 next; this page may exist on some systems
    // depending on the video driver, and we want this in "Screen"
    // also
    _wpAddDMQSDisplayTypePage(somSelf, hwndNotebook);
    // "Screen" page 1 next
    _wpAddSystemScreenPage(somSelf, hwndNotebook);

    return (TRUE);
}

/* ******************************************************************
 *                                                                  *
 *   here come the XWPScreen class methods                          *
 *                                                                  *
 ********************************************************************/

/*
 *@@ wpclsInitData:
 *      initialize XWPScreen class data.
 */

SOM_Scope void  SOMLINK xwpscrM_wpclsInitData(M_XWPScreen *somSelf)
{
    /* M_XWPScreenData *somThis = M_XWPScreenGetData(somSelf); */
    M_XWPScreenMethodDebug("M_XWPScreen","xwpscrM_wpclsInitData");

    M_XWPScreen_parent_M_WPSystem_wpclsInitData(somSelf);

    {
        PKERNELGLOBALS   pKernelGlobals = krnLockGlobals(5000);
        // store the class object in KERNELGLOBALS
        pKernelGlobals->fXWPScreen = TRUE;
        krnUnlockGlobals();
    }
}

/*
 *@@ wpclsQuerySettingsPageSize:
 *      this WPObject class method should return the
 *      size of the largest settings page in dialog
 *      units; if a settings notebook is initially
 *      opened, i.e. no window pos has been stored
 *      yet, the WPS will use this size, to avoid
 *      truncated settings pages.
 */

SOM_Scope BOOL  SOMLINK xwpscrM_wpclsQuerySettingsPageSize(M_XWPScreen *somSelf,
                                                           PSIZEL pSizl)
{
    /* M_XWPScreenData *somThis = M_XWPScreenGetData(somSelf); */
    M_XWPScreenMethodDebug("M_XWPScreen","xwpscrM_wpclsQuerySettingsPageSize");

    pSizl->cx = 275;       // size of "Object" page
    pSizl->cy = 130;       // size of "Object" page

    return (TRUE);
}

/*
 *@@ wpclsQueryTitle:
 *
 */

SOM_Scope PSZ  SOMLINK xwpscrM_wpclsQueryTitle(M_XWPScreen *somSelf)
{
    PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();
    /* M_XWPScreenData *somThis = M_XWPScreenGetData(somSelf); */
    M_XWPScreenMethodDebug("M_XWPScreen","xwpscrM_wpclsQueryTitle");

    return (pNLSStrings->pszXWPScreenTitle);
}

/*
 *@@ wpclsQueryIconData:
 *
 */

SOM_Scope ULONG  SOMLINK xwpscrM_wpclsQueryIconData(M_XWPScreen *somSelf,
                                                    PICONINFO pIconInfo)
{
    /* M_XWPScreenData *somThis = M_XWPScreenGetData(somSelf); */
    M_XWPScreenMethodDebug("M_XWPScreen","xwpscrM_wpclsQueryIconData");

    if (pIconInfo)
    {
        pIconInfo->fFormat = ICON_RESOURCE;
        pIconInfo->resid   = ID_ICONXWPSCREEN;
        pIconInfo->hmod    = cmnQueryMainModuleHandle();
    }

    return (sizeof(ICONINFO));
}
