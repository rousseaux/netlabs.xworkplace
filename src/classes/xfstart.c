
/*
 *@@sourcefile xfstart.c:
 *      This file contains SOM code for the following XWorkplace classes:
 *
 *      --  XFldStartup (startup folder, XFolder subclass)
 *      --  XFldShutdown (shutdown folder, XFolder subclass)
 *
 *      Installation of XFldStartup and XFldShutdown is
 *      optional. However, both classes are derived from
 *      XFolder directly (and not from WPFolder), so
 *      installation of XFolder is required if any of
 *      XFldStartup and XFldShutdown are installed.
 *
 *      These two classes used to be in xfldr.c before
 *      V0.9.0, but have been moved to this new separate file
 *      because the XFolder class is complex enough to be
 *      in a file of its own.
 *
 *      Starting with V0.9.0, the files in classes\ contain only
 *      i.e. the methods themselves.
 *      The implementation for this class is in filesys\folder.c.
 *
 *@@somclass XFldStartup xfstup_
 *@@somclass M_XFldStartup xfstupM_
 *@@somclass XFldShutdown xfshut_
 *@@somclass M_XFldShutdown xfshutM_
 */

/*
 *      Copyright (C) 1997-2000 Ulrich M�ller.
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
 *@@todo:
 *
 */

/*
 *  This file was generated by the SOM Compiler and Emitter Framework.
 *  Generated using:
 *      SOM Emitter emitctm: 2.41
 */

#ifndef SOM_Module_xfstart_Source
#define SOM_Module_xfstart_Source
#endif
#define XFldStartup_Class_Source
#define M_XFldStartup_Class_Source

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
#define INCL_DOSSEMAPHORES
#define INCL_WINWINDOWMGR
#define INCL_WINMENUS
#define INCL_WINDIALOGS
#define INCL_WINBUTTONS
#include <os2.h>

// C library headers
#include <stdio.h>              // needed for except.h
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\winh.h"               // PM helper routines

// SOM headers which don't crash with prec. header files
#include "xfstart.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\notebook.h"            // generic XWorkplace notebook handling
#include "shared\kernel.h"              // XWorkplace Kernel

#include "filesys\folder.h"             // XFolder implementation

// other SOM headers
#pragma hdrstop                         // VAC++ keeps crashing otherwise

/* ******************************************************************
 *                                                                  *
 *   here come the XFldStartup methods                              *
 *                                                                  *
 ********************************************************************/

/*
 *@@ xwpAddXFldStartupPage:
 *      this adds the "Startup" page into the startup folder's
 *      settings notebook.
 *
 *@@added V0.9.0 [umoeller]
 */

SOM_Scope ULONG  SOMLINK xfstup_xwpAddXFldStartupPage(XFldStartup *somSelf,
                                                     HWND hwndDlg)
{
    PCREATENOTEBOOKPAGE pcnbp;
    PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();

    /* XFldStartupData *somThis = XFldStartupGetData(somSelf); */
    XFldStartupMethodDebug("XFldStartup","xfstup_xwpAddXFldStartupPage");

    pcnbp = malloc(sizeof(CREATENOTEBOOKPAGE));
    memset(pcnbp, 0, sizeof(CREATENOTEBOOKPAGE));
    pcnbp->somSelf = somSelf;
    pcnbp->hwndNotebook = hwndDlg;
    pcnbp->hmod = cmnQueryNLSModuleHandle(FALSE);
    pcnbp->usPageStyleFlags = BKA_MAJOR;
    pcnbp->pszName = pNLSStrings->pszStartupPage;
    pcnbp->ulDlgID = ID_XSD_STARTUPFOLDER;
    pcnbp->ulDefaultHelpPanel  = ID_XSH_SETTINGS_XFLDSTARTUP;
    pcnbp->ulPageID = SP_STARTUPFOLDER;
    pcnbp->pfncbInitPage    = fdrStartupFolderInitPage;
    pcnbp->pfncbItemChanged = fdrStartupFolderItemChanged;

    return (ntbInsertPage(pcnbp));
}

/*
 *
 *@@ wpFilterPopupMenu:
 *      this WPObject instance method allows the object to
 *      filter out unwanted menu items from the context menu.
 *      This gets called before wpModifyPopupMenu.
 *
 *      We remove "Create another" menu item.
 *
 *@@added V0.9.2 (2000-02-26) [umoeller]
 */

SOM_Scope ULONG  SOMLINK xfstup_wpFilterPopupMenu(XFldStartup *somSelf,
                                                  ULONG ulFlags,
                                                  HWND hwndCnr,
                                                  BOOL fMultiSelect)
{
    /* XFldStartupData *somThis = XFldStartupGetData(somSelf); */
    XFldStartupMethodDebug("XFldStartup","xfstup_wpFilterPopupMenu");

    return (XFldStartup_parent_XFolder_wpFilterPopupMenu(somSelf,
                                                         ulFlags,
                                                         hwndCnr,
                                                         fMultiSelect)
            & ~CTXT_NEW
           );
}

/*
 *@@ wpModifyPopupMenu:
 *      this WPObject instance methods gets called by the WPS
 *      when a context menu needs to be built for the object
 *      and allows the object to manipulate its context menu.
 *      This gets called _after_ wpFilterPopupMenu.
 *
 *      We add a "Process content" menu item to this
 *      popup menu; the other menu items are inherited
 *      from XFolder.
 */

SOM_Scope BOOL  SOMLINK xfstup_wpModifyPopupMenu(XFldStartup *somSelf,
                                                 HWND hwndMenu,
                                                 HWND hwndCnr,
                                                 ULONG iPosition)
{
    BOOL rc;
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();
    /* XFldStartupData *somThis = XFldStartupGetData(somSelf); */
    XFldStartupMethodDebug("XFldStartup","xfstup_wpModifyPopupMenu");

    rc = XFldStartup_parent_XFolder_wpModifyPopupMenu(somSelf,
                                                         hwndMenu,
                                                         hwndCnr,
                                                         iPosition);

    winhInsertMenuSeparator(hwndMenu, MIT_END,
            (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_SEPARATOR));

    winhInsertMenuItem(hwndMenu,
            MIT_END,
            (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_PROCESSCONTENT),
            pNLSStrings->pszProcessContent,
            MIS_TEXT, 0);

    return (rc);
}

/*
 *@@ wpMenuItemSelected:
 *      this WPObject method processes menu selections.
 *      This must be overridden to support new menu
 *      items which have been added in wpModifyPopupMenu.
 *
 *      We react to the "Process content" item we have
 *      inserted for the startup folder.
 *
 *      Note that the WPS invokes this method upon every
 *      object which has been selected in the container.
 *      That is, if three objects have been selected and
 *      a menu item has been selected for all three of
 *      them, all three objects will receive this method
 *      call. This is true even if FALSE is returned from
 *      this method.
 */

SOM_Scope BOOL  SOMLINK xfstup_wpMenuItemSelected(XFldStartup *somSelf,
                                                  HWND hwndFrame,
                                                  ULONG ulMenuId)
{
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    /* XFldStartupData *somThis = XFldStartupGetData(somSelf); */
    XFldStartupMethodDebug("XFldStartup","xfstup_wpMenuItemSelected");

    if ( (ulMenuId - pGlobalSettings->VarMenuOffset) == ID_XFMI_OFS_PROCESSCONTENT )
    {
        if (cmnMessageBoxMsg((hwndFrame)
                                ? hwndFrame
                                : HWND_DESKTOP,
                             116,
                             138,
                             MB_YESNO | MB_DEFBUTTON2)
                == MBID_YES)
        {

            krnPostThread1ObjectMsg(T1M_BEGINSTARTUP, MPNULL, MPNULL);
        }
        return (TRUE);
    }
    else
        return (XFldStartup_parent_XFolder_wpMenuItemSelected(somSelf,
                                                              hwndFrame,
                                                              ulMenuId));
}

/*
 *@@ wpMenuItemHelpSelected:
 *      display help for "Process content"
 *      menu item.
 */

SOM_Scope BOOL  SOMLINK xfstup_wpMenuItemHelpSelected(XFldStartup *somSelf,
                                                      ULONG MenuId)
{
    /* XFldStartupData *somThis = XFldStartupGetData(somSelf); */
    XFldStartupMethodDebug("XFldStartup","xfstup_wpMenuItemHelpSelected");

    return (XFldStartup_parent_XFolder_wpMenuItemHelpSelected(somSelf,
                                                              MenuId));
}

/*
 *@@ wpQueryDefaultHelp:
 *      this WPObject instance method specifies the default
 *      help panel for an object (when "Extended help" is
 *      selected from the object's context menu). This should
 *      describe what this object can do in general.
 *
 *      We'll display some help for the Startup folder.
 */

SOM_Scope BOOL  SOMLINK xfstup_wpQueryDefaultHelp(XFldStartup *somSelf,
                                                  PULONG pHelpPanelId,
                                                  PSZ HelpLibrary)
{
    /* XFldStartupData *somThis = XFldStartupGetData(somSelf); */
    XFldStartupMethodDebug("XFldStartup","xfstup_wpQueryDefaultHelp");

    strcpy(HelpLibrary, cmnQueryHelpLibrary());
    *pHelpPanelId = ID_XMH_STARTUPSHUTDOWN;
    return (TRUE);

    /* return (XFldStartup_parent_XFolder_wpQueryDefaultHelp(somSelf,
                                                          pHelpPanelId,
                                                          HelpLibrary)); */
}

/*
 *@@ wpAddSettingsPages:
 *      this WPObject instance method gets called by the WPS
 *      when the Settings view is opened to have all the
 *      settings page inserted into hwndNotebook.
 *
 *      Starting with V0.9.0, we override this method too to add
 *      the XWorkplace Startup folder's settings
 *      page.
 *
 *@@added V0.9.0 [umoeller]
 */

SOM_Scope BOOL  SOMLINK xfstup_wpAddSettingsPages(XFldStartup *somSelf,
                                                  HWND hwndNotebook)
{
    /* XFldStartupData *somThis = XFldStartupGetData(somSelf); */
    XFldStartupMethodDebug("XFldStartup","xfstup_wpAddSettingsPages");

    XFldStartup_parent_XFolder_wpAddSettingsPages(somSelf, hwndNotebook);

    // add the "XWorkplace Startup" page on top
    return (_xwpAddXFldStartupPage(somSelf, hwndNotebook));
}

/*
 *@@ wpclsInitData:
 *      initialize XFldStartup class data.
 *
 *@@changed V0.9.0 [umoeller]: added class object to KERNELGLOBALS
 */

SOM_Scope void  SOMLINK xfstupM_wpclsInitData(M_XFldStartup *somSelf)
{
    /* M_XFldStartupData *somThis = M_XFldStartupGetData(somSelf); */
    M_XFldStartupMethodDebug("M_XFldStartup","xfstupM_wpclsInitData");

    M_XFldStartup_parent_M_XFolder_wpclsInitData(somSelf);

    {
        PKERNELGLOBALS   pKernelGlobals = krnLockGlobals(5000);
        // store the class object in KERNELGLOBALS
        pKernelGlobals->fXFldStartup = TRUE;
        krnUnlockGlobals();
    }
}

/*
 *@@ wpclsQueryTitle:
 *      tell the WPS the new class default title:
 *      "XFolder Startup".
 *
 *@@changed V0.9.6 (2000-11-20) [umoeller]: changed to "XWorkplace"
 */

SOM_Scope PSZ  SOMLINK xfstupM_wpclsQueryTitle(M_XFldStartup *somSelf)
{
    /* M_XFldStartupData *somThis = M_XFldStartupGetData(somSelf); */
    M_XFldStartupMethodDebug("M_XFldStartup","xfstupM_wpclsQueryTitle");

    return ("XWorkplace Startup");
}

/*
 *@@ wpclsQueryStyle:
 *      we return a flag so that no templates are created
 *      for the Startup folder.
 */

SOM_Scope ULONG  SOMLINK xfstupM_wpclsQueryStyle(M_XFldStartup *somSelf)
{
    /* M_XFldStartupData *somThis = M_XFldStartupGetData(somSelf); */
    M_XFldStartupMethodDebug("M_XFldStartup","xfstupM_wpclsQueryStyle");

    return (M_XFldStartup_parent_M_XFolder_wpclsQueryStyle(somSelf)
                | CLSSTYLE_NEVERTEMPLATE
                | CLSSTYLE_NEVERCOPY
                // | CLSSTYLE_NEVERDELETE
           );

}

/*
 *@@ wpclsQueryIconData:
 *      this WPObject class method builds the default
 *      icon for objects of a class (i.e. the icon which
 *      is shown if no instance icon is assigned). This
 *      apparently gets called from some of the other
 *      icon instance methods if no instance icon was
 *      found for an object. The exact mechanism of how
 *      this works is not documented.
 *
 *      We give the Startup folder a new closed icon.
 */

SOM_Scope ULONG  SOMLINK xfstupM_wpclsQueryIconData(M_XFldStartup *somSelf,
                                                    PICONINFO pIconInfo)
{
    /* M_XFldStartupData *somThis = M_XFldStartupGetData(somSelf); */
    M_XFldStartupMethodDebug("M_XFldStartup","xfstupM_wpclsQueryIconData");

    if (pIconInfo) {
       pIconInfo->fFormat = ICON_RESOURCE;
       pIconInfo->resid   = ID_STARTICON1;
       pIconInfo->hmod    = cmnQueryMainModuleHandle();
    }

    return (sizeof(ICONINFO));
}

/*
 *@@ wpclsQueryIconDataN:
 *      give the Startup folder a new animated icon.
 */

SOM_Scope ULONG  SOMLINK xfstupM_wpclsQueryIconDataN(M_XFldStartup *somSelf,
                                                     ICONINFO* pIconInfo,
                                                     ULONG ulIconIndex)
{
    /* M_XFldStartupData *somThis = M_XFldStartupGetData(somSelf); */
    M_XFldStartupMethodDebug("M_XFldStartup","xfstupM_wpclsQueryIconDataN");

    if (pIconInfo) {
       pIconInfo->fFormat = ICON_RESOURCE;
       pIconInfo->resid   = ID_STARTICON2;
       pIconInfo->hmod    = cmnQueryMainModuleHandle();
    }

    return (sizeof(ICONINFO));
}


/* ******************************************************************
 *                                                                  *
 *   here come the XFldShutdown methods                             *
 *                                                                  *
 ********************************************************************/

/*
 *@@ wpQueryDefaultHelp:
 *      this WPObject instance method specifies the default
 *      help panel for an object (when "Extended help" is
 *      selected from the object's context menu). This should
 *      describe what this object can do in general.
 *      We must return TRUE to report successful completion.
 *
 *      We'll display some help for the Shutdown folder.
 */

SOM_Scope BOOL  SOMLINK xfshut_wpQueryDefaultHelp(XFldShutdown *somSelf,
                                                  PULONG pHelpPanelId,
                                                  PSZ HelpLibrary)
{
    /* XFldShutdownData *somThis = XFldShutdownGetData(somSelf); */
    XFldShutdownMethodDebug("XFldShutdown","xfshut_wpQueryDefaultHelp");

    strcpy(HelpLibrary, cmnQueryHelpLibrary());
    *pHelpPanelId = ID_XMH_STARTUPSHUTDOWN;
    return (TRUE);

    /* return (XFldShutdown_parent_XFolder_wpQueryDefaultHelp(somSelf,
                                                           pHelpPanelId,
                                                           HelpLibrary)); */
}

/*
 *@@ wpclsInitData:
 *      initialize XFldShutdown class data.
 *
 *@@changed V0.9.0 [umoeller]: added class object to KERNELGLOBALS
 */

SOM_Scope void  SOMLINK xfshutM_wpclsInitData(M_XFldShutdown *somSelf)
{
    /* M_XFldShutdownData *somThis = M_XFldShutdownGetData(somSelf); */
    M_XFldShutdownMethodDebug("M_XFldShutdown","xfshutM_wpclsInitData");

    M_XFldShutdown_parent_M_XFolder_wpclsInitData(somSelf);

    {
        PKERNELGLOBALS   pKernelGlobals = krnLockGlobals(5000);
        // store the class object in KERNELGLOBALS
        pKernelGlobals->fXFldShutdown = TRUE;
        krnUnlockGlobals();
    }
}

/*
 *@@ wpclsQueryTitle:
 *      tell the WPS the new class default title:
 *      "XFolderShutdown".
 *
 *@@changed V0.9.6 (2000-11-20) [umoeller]: changed to "XWorkplace"
 */

SOM_Scope PSZ  SOMLINK xfshutM_wpclsQueryTitle(M_XFldShutdown *somSelf)
{
    /* M_XFldShutdownData *somThis = M_XFldShutdownGetData(somSelf); */
    M_XFldShutdownMethodDebug("M_XFldShutdown","xfshutM_wpclsQueryTitle");

    return ("XWorkplace Shutdown");
}

/*
 *@@ wpclsQueryStyle:
 *      we return a flag so that no templates are created
 *      for the Shutdown folder.
 */

SOM_Scope ULONG  SOMLINK xfshutM_wpclsQueryStyle(M_XFldShutdown *somSelf)
{
    /* M_XFldShutdownData *somThis = M_XFldShutdownGetData(somSelf); */
    M_XFldShutdownMethodDebug("M_XFldShutdown","xfshutM_wpclsQueryStyle");

    return (M_XFldShutdown_parent_M_XFolder_wpclsQueryStyle(somSelf)
                | CLSSTYLE_NEVERTEMPLATE
                | CLSSTYLE_NEVERCOPY
                // | CLSSTYLE_NEVERDELETE
           );
}

/*
 *@@ wpclsQueryIconData:
 *      give the Shutdown folder a new closed icon.
 */

SOM_Scope ULONG  SOMLINK xfshutM_wpclsQueryIconData(M_XFldShutdown *somSelf,
                                                    PICONINFO pIconInfo)
{
    /* M_XFldShutdownData *somThis = M_XFldShutdownGetData(somSelf); */
    M_XFldShutdownMethodDebug("M_XFldShutdown","xfshutM_wpclsQueryIconData");

    if (pIconInfo) {
       pIconInfo->fFormat = ICON_RESOURCE;
       pIconInfo->resid   = ID_SHUTICON1;
       pIconInfo->hmod    = cmnQueryMainModuleHandle();
    }

    return (sizeof(ICONINFO));

    /* return (M_XFldShutdown_parent_M_XFolder_wpclsQueryIconData(somSelf,
                                                               pIconInfo)); */
}

/*
 *@@ wpclsQueryIconDataN:
 *      give the Shutdown folder a new animated icon.
 */

SOM_Scope ULONG  SOMLINK xfshutM_wpclsQueryIconDataN(M_XFldShutdown *somSelf,
                                                     ICONINFO* pIconInfo,
                                                     ULONG ulIconIndex)
{
    /* M_XFldShutdownData *somThis = M_XFldShutdownGetData(somSelf); */
    M_XFldShutdownMethodDebug("M_XFldShutdown","xfshutM_wpclsQueryIconDataN");

    if (pIconInfo) {
       pIconInfo->fFormat = ICON_RESOURCE;
       pIconInfo->resid   = ID_SHUTICON2;
       pIconInfo->hmod    = cmnQueryMainModuleHandle();
    }

    return (sizeof(ICONINFO));

    /* return (M_XFldShutdown_parent_M_XFolder_wpclsQueryIconDataN(somSelf,
                                                                pIconInfo,
                                                                ulIconIndex)); */
}



