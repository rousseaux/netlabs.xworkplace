
/*
 *@@sourcefile xfsys.c:
 *      This file contains SOM code for the following XWorkplace classes:
 *
 *      --  XFldSystem (WPSystem subclass)
 *
 *      XFldSystem implements the "OS/2 Kernel" settings object
 *      (for CONFIG.SYS manipulation).
 *
 *      Installation of this class is optional.
 *
 *      This file has undergone major changes with V0.82, but this
 *      was mainly reorganized and clarified without introducing
 *      new features.
 *
 *      With V0.9.0, the XFldWPS class ("Workplace Shell" settings
 *      object) has been moved to a separate file, xfwps.c.
 *
 *      Starting with V0.9.0, the files in classes\ contain only
 *      i.e. the methods themselves.
 *      The implementation for this class is mostly in filesys\cfgsys.c.
 *
 *@@somclass XFldSystem xfsys_
 *@@somclass M_XFldSystem xfsysM_
 */

/*
 *      Copyright (C) 1997-2003 Ulrich M�ller.
 *
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

#ifndef SOM_Module_xfsys_Source
#define SOM_Module_xfsys_Source
#endif
#define xfsys_Class_Source
#define M_xfsys_Class_Source

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
#include "xfsys.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\helppanels.h"          // all XWorkplace help panel IDs
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling

#include "config\cfgsys.h"              // XFldSystem CONFIG.SYS pages implementation

// other SOM headers
#pragma hdrstop                         // VAC++ keeps crashing otherwise
// #include "xfobj.h"

/* ******************************************************************
 *
 *   here come the XFldSystem instance methods
 *
 ********************************************************************/

/*
 *@@ xwpAddXFldSystemPages:
 *      this actually adds all the new XWorkplace pages into the
 *      "OS/2 Kernel" notebook.
 *
 *@@changed V0.9.0 [umoeller]: added "System paths" page
 *@@changed V0.9.0 [umoeller]: added "Storage" page
 *@@changed V0.9.3 (2000-04-01) [umoeller]: removed "HPFS" page
 */

SOM_Scope ULONG  SOMLINK xfsys_xwpAddXFldSystemPages(XFldSystem *somSelf,
                                                     HWND hwndDlg)
{
    INSERTNOTEBOOKPAGE inbp;
    HMODULE         savehmod;

    // XFldSystemData *somThis = XFldSystemGetData(somSelf);
    XFldSystemMethodDebug("XFldSystem","xfsys_xwpAddXFldSystemPages");

    savehmod = cmnQueryNLSModuleHandle(FALSE);

    // "Errors"
    memset(&inbp, 0, sizeof(INSERTNOTEBOOKPAGE));
    inbp.somSelf = somSelf;
    inbp.hwndNotebook = hwndDlg;
    inbp.hmod = savehmod;
    inbp.pfncbInitPage    = cfgConfigInitPage;
    inbp.pfncbItemChanged = cfgConfigItemChanged;
    inbp.usPageStyleFlags = BKA_MAJOR;
    inbp.pcszName = cmnGetString(ID_XSSI_ERRORS);  // pszErrors
    inbp.ulDlgID = ID_OSD_SETTINGS_ERRORS;
    inbp.ulDefaultHelpPanel  = ID_XSH_SETTINGS_ERRORS;
    // give this page a unique ID, which is
    // passed to the common config.sys callbacks
    inbp.ulPageID = SP_ERRORS;
    ntbInsertPage(&inbp);

    // "WPS" settings
    memset(&inbp, 0, sizeof(INSERTNOTEBOOKPAGE));
    inbp.somSelf = somSelf;
    inbp.hwndNotebook = hwndDlg;
    inbp.hmod = savehmod;
    inbp.pfncbInitPage    = cfgConfigInitPage;
    inbp.pfncbItemChanged = cfgConfigItemChanged;
    inbp.usPageStyleFlags = BKA_MAJOR;
    inbp.pcszName = cmnGetString(ID_XSSI_WPS);  // pszWPS
    inbp.ulDlgID = ID_OSD_SETTINGS_WPS;
    inbp.ulDefaultHelpPanel  = ID_XSH_SETTINGS_WPS;
    // give this page a unique ID (common.h), which
    // is passed to the common config.sys callbacks
    inbp.ulPageID = SP_WPS;
    ntbInsertPage(&inbp);

    // "System paths" page (V0.9.0)
    memset(&inbp, 0, sizeof(INSERTNOTEBOOKPAGE));
    inbp.somSelf = somSelf;
    inbp.hwndNotebook = hwndDlg;
    inbp.hmod = savehmod;
    inbp.pfncbInitPage    = cfgConfigInitPage;
    inbp.pfncbItemChanged = cfgConfigItemChanged;
    inbp.usPageStyleFlags = BKA_MAJOR;
    inbp.pcszName = cmnGetString(ID_XSSI_SYSPATHS);  // pszSysPaths
    inbp.ulDlgID = ID_OSD_SETTINGS_SYSPATHS;
    inbp.ulDefaultHelpPanel  = ID_XSH_SETTINGS_SYSPATHS;
    // give this page a unique ID (common.h), which
    // is passed to the common config.sys callbacks
    inbp.ulPageID = SP_SYSPATHS;
    ntbInsertPage(&inbp);

    // insert file-system settings pages
    memset(&inbp, 0, sizeof(INSERTNOTEBOOKPAGE));
    inbp.somSelf = somSelf;
    inbp.hwndNotebook = hwndDlg;
    inbp.hmod = savehmod;
    inbp.pfncbInitPage    = cfgConfigInitPage;
    inbp.pfncbItemChanged = cfgConfigItemChanged;
    inbp.usPageStyleFlags = BKA_MAJOR;
    inbp.pcszName = "~FAT";
    inbp.ulDlgID = ID_OSD_SETTINGS_FAT;
    inbp.ulDefaultHelpPanel  = ID_XSH_SETTINGS_HPFS; // it's FAT really, just historical...
    // give this page a unique ID (common.h), which
    // is passed to the common config.sys callbacks
    inbp.ulPageID = SP_FAT;
    ntbInsertPage(&inbp);

    // "Drivers"
    memset(&inbp, 0, sizeof(INSERTNOTEBOOKPAGE));
    inbp.somSelf = somSelf;
    inbp.hwndNotebook = hwndDlg;
    inbp.hmod = savehmod;
    inbp.usPageStyleFlags = BKA_MAJOR;
    inbp.pcszName = cmnGetString(ID_XSSI_DRIVERS);  // pszDrivers
    inbp.ulDlgID = ID_OSD_SETTINGS_DRIVERS;
    inbp.ulDefaultHelpPanel  = ID_XSH_SETTINGS_DRIVERS;
    inbp.ulPageID = SP_DRIVERS;
    inbp.pampControlFlags = G_pampDriversPage;
    inbp.cControlFlags = G_cDriversPage;
    inbp.pfncbInitPage    = cfgDriversInitPage;
    inbp.pfncbItemChanged = cfgDriversItemChanged;
    ntbInsertPage(&inbp);

    // "Memory"
    memset(&inbp, 0, sizeof(INSERTNOTEBOOKPAGE));
    inbp.somSelf = somSelf;
    inbp.hwndNotebook = hwndDlg;
    inbp.hmod = savehmod;
    inbp.pfncbInitPage    = cfgConfigInitPage;
    inbp.pfncbItemChanged = cfgConfigItemChanged;
    inbp.usPageStyleFlags = BKA_MAJOR;
    inbp.pcszName = cmnGetString(ID_XSSI_MEMORY);  // pszMemory
    inbp.ulDlgID = ID_OSD_SETTINGS_KERNEL2;
    inbp.ulDefaultHelpPanel  = ID_XSH_SETTINGS_KERNEL2;
    // give this page a unique ID (common.h), which
    // is passed to the common config.sys callbacks
    inbp.ulPageID = SP_MEMORY;
    // for this page, start a timer
    inbp.ulTimer = 2000;
    inbp.pfncbTimer = cfgConfigTimer;
    ntbInsertPage(&inbp);

    // "Scheduler"
    memset(&inbp, 0, sizeof(INSERTNOTEBOOKPAGE));
    inbp.somSelf = somSelf;
    inbp.hwndNotebook = hwndDlg;
    inbp.hmod = savehmod;
    inbp.pfncbInitPage    = cfgConfigInitPage;
    inbp.pfncbItemChanged = cfgConfigItemChanged;
    inbp.usPageStyleFlags = BKA_MAJOR;
    inbp.pcszName = cmnGetString(ID_XSSI_SCHEDULER);  // pszScheduler
    inbp.ulDlgID = ID_OSD_SETTINGS_KERNEL1;
    inbp.ulDefaultHelpPanel  = ID_XSH_SETTINGS_KERNEL1;
    // give this page a unique ID (common.h), which
    // is passed to the common config.sys callbacks
    inbp.ulPageID = SP_SCHEDULER;
    // for this page, start a timer
    inbp.ulTimer = 2000;
    inbp.pfncbTimer = cfgConfigTimer;
    ntbInsertPage(&inbp);

    // "Syslevel"
    memset(&inbp, 0, sizeof(INSERTNOTEBOOKPAGE));
    inbp.somSelf = somSelf;
    inbp.hwndNotebook = hwndDlg;
    inbp.hmod = savehmod;
    inbp.pcszName = cmnGetString(ID_XSSI_SYSLEVELPAGE);  // pszSyslevelPage
    inbp.ulDlgID = ID_XFD_CONTAINERPAGE; // generic cnr page
    inbp.ulDefaultHelpPanel  = ID_XSH_SETTINGS_SYSLEVEL;
    inbp.usPageStyleFlags = BKA_MAJOR;
    inbp.ulPageID = SP_SYSLEVEL;
    inbp.pampControlFlags = G_pampGenericCnrPage;
    inbp.cControlFlags = G_cGenericCnrPage;
    inbp.pfncbInitPage    = cfgSyslevelInitPage;
    inbp.pfncbItemChanged = cfgSyslevelItemChanged;
    return ntbInsertPage(&inbp);
}

/*
 *@@ wpFilterPopupMenu:
 *      this WPObject instance method allows the object to
 *      filter out unwanted menu items from the context menu.
 *      This gets called before wpModifyPopupMenu.
 *
 *      We remove "Create another" menu item.
 *
 *@@added V0.9.2 (2000-02-26) [umoeller]
 */

SOM_Scope ULONG  SOMLINK xfsys_wpFilterPopupMenu(XFldSystem *somSelf,
                                                 ULONG ulFlags,
                                                 HWND hwndCnr,
                                                 BOOL fMultiSelect)
{
    /* XFldSystemData *somThis = XFldSystemGetData(somSelf); */
    XFldSystemMethodDebug("XFldSystem","xfsys_wpFilterPopupMenu");

    return (XFldSystem_parent_WPSystem_wpFilterPopupMenu(somSelf,
                                                         ulFlags,
                                                         hwndCnr,
                                                         fMultiSelect)
            & ~CTXT_NEW
           );
}

/*
 *@@ wpAddSettingsPages:
 *      this WPObject instance method gets called by the WPS
 *      when the Settings view is opened to have all the
 *      settings page inserted into hwndNotebook. Override
 *      this method to add new settings pages to either the
 *      top or the bottom of notebooks of a given class.
 *
 *      In order to to this, unlike the procedure used in
 *      the "Workplace Shell" object, we will explicitly
 *      call the WPSystem methods which insert the
 *      pages we want to see here into the notebook.
 *      As a result, the "Workplace Shell" object
 *      inherits all pages from the "System" object
 *      which might be added by other WPS utils, while
 *      this thing does not.
 *
 *@@changed V0.9.1 (2000-02-17) [umoeller]: cut adding "Internals" page
 *@@changed V0.9.2 (2000-02-23) [umoeller]: removed "Screen" pages; these are now in XWPScreen
 */

SOM_Scope BOOL  SOMLINK xfsys_wpAddSettingsPages(XFldSystem *somSelf,
                                                 HWND hwndNotebook)
{
    // XFldSystemData *somThis = XFldSystemGetData(somSelf);
    XFldSystemMethodDebug("XFldSystem","xfsys_wpwpAddSettingsPages");

    // do _not_ call the parent, but call the page methods
    // explicitly

    // XFolder "Internals" page bottommost
    // _xwpAddObjectInternalsPage(somSelf, hwndNotebook);

    // "Symbol" page next
    _wpAddObjectGeneralPage(somSelf, hwndNotebook);
        // this inserts the "Internals"/"Object" page now

    // XFolder CONFIG.SYS pages on top
    _xwpAddXFldSystemPages(somSelf, hwndNotebook);

    return TRUE;
}

/* ******************************************************************
 *
 *   here come the XFldSystem class methods
 *
 ********************************************************************/

/*
 *@@ wpclsInitData:
 *      this M_WPObject class method gets called when a class
 *      is loaded by the WPS (probably from within a
 *      somFindClass call) and allows the class to initialize
 *      itself.
 *
 *@@changed V0.9.0 [umoeller]: added class object to KERNELGLOBALS
 */

SOM_Scope void  SOMLINK xfsysM_wpclsInitData(M_XFldSystem *somSelf)
{
    /* M_XFldSystemData *somThis = M_XFldSystemGetData(somSelf); */
    M_XFldSystemMethodDebug("M_XFldSystem","xfsysM_wpclsInitData");

    M_XFldSystem_parent_M_WPSystem_wpclsInitData(somSelf);

    krnClassInitialized(G_pcszXFldSystem);
}

/*
 *@@ wpclsQuerySettingsPageSize:
 *      this M_WPObject class method should return the
 *      size of the largest settings page in dialog
 *      units; if a settings notebook is initially
 *      opened, i.e. no window pos has been stored
 *      yet, the WPS will use this size, to avoid
 *      truncated settings pages.
 */

SOM_Scope BOOL  SOMLINK xfsysM_wpclsQuerySettingsPageSize(M_XFldSystem *somSelf,
                                                          PSIZEL pSizl)
{
    BOOL brc;
    /* M_XFldSystemData *somThis = M_XFldSystemGetData(somSelf); */
    M_XFldSystemMethodDebug("M_XFldSystem","xfsysM_wpclsQuerySettingsPageSize");

    brc = M_XFldSystem_parent_M_WPSystem_wpclsQuerySettingsPageSize(somSelf,
                                                                    pSizl);
    if (brc)
    {
        pSizl->cy = 177;        // this is the height of the "WPS Classes" page,
                                // which seems to be the largest in the "Workplace
                                // Shell" object
        if (G_fIsWarp4)
            // on Warp 4, reduce again, because we're moving
            // the notebook buttons to the bottom
            pSizl->cy -= WARP4_NOTEBOOK_OFFSET;
    }

    return brc;
}

/*
 *@@ wpclsQueryTitle:
 *      this M_WPObject class method tells the WPS the clear
 *      name of a class, which is shown in the third column
 *      of a Details view and also used as the default title
 *      for new objects of a class.
 */

SOM_Scope PSZ  SOMLINK xfsysM_wpclsQueryTitle(M_XFldSystem *somSelf)
{
    // M_XFldSystemData *somThis = M_XFldSystemGetData(somSelf);
    M_XFldSystemMethodDebug("M_XFldSystem","xfsysM_wpclsQueryTitle");

    return "OS/2 Kernel";       // @@todo localize
}

/*
 *@@ wpclsQueryDefaultHelp:
 *      this M_WPObject class method returns the default help
 *      panel for objects of this class. This gets called
 *      from WPObject::wpQueryDefaultHelp if no instance
 *      help settings (HELPLIBRARY, HELPPANEL) have been
 *      set for an individual object. It is thus recommended
 *      to override this method instead of the instance
 *      method to change the default help panel for a class
 *      in order not to break instance help settings (fixed
 *      with 0.9.20).
 *
 *      We return the default help for the "OS/2 Kernel"
 *      object here.
 *
 *@@added V0.9.20 (2002-07-12) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xfsysM_wpclsQueryDefaultHelp(M_XFldSystem *somSelf,
                                                     PULONG pHelpPanelId,
                                                     PSZ pszHelpLibrary)
{
    /* M_XFldSystemData *somThis = M_XFldSystemGetData(somSelf); */
    M_XFldSystemMethodDebug("M_XFldSystem","xfsysM_wpclsQueryDefaultHelp");

    strcpy(pszHelpLibrary, cmnQueryHelpLibrary());
    *pHelpPanelId = ID_XSH_XFLDSYSTEM;
    return TRUE;
}

/*
 *@@ wpclsQueryIconData:
 *      this M_WPObject class method must return information
 *      about how to build the default icon for objects
 *      of a class. This gets called from various other
 *      methods whenever a class default icon is needed;
 *      most importantly, M_WPObject::wpclsQueryIcon
 *      calls this to build a class default icon, which
 *      is then cached in the class's instance data.
 *      If a subclass wants to change a class default icon,
 *      it should always override _this_ method instead of
 *      wpclsQueryIcon.
 *
 *      Note that the default WPS implementation does not
 *      allow for specifying the ICON_FILE format here,
 *      which is why we have overridden
 *      M_XFldObject::wpclsQueryIcon too. This allows us
 *      to return icon _files_ for theming too. For details
 *      about the WPS's crappy icon management, refer to
 *      src\filesys\icons.c.
 *
 *      We give the "OS/2 Kernel" object a new icon.
 */

SOM_Scope ULONG  SOMLINK xfsysM_wpclsQueryIconData(M_XFldSystem *somSelf,
                                                   PICONINFO pIconInfo)
{
    // M_XFldSystemData *somThis = M_XFldSystemGetData(somSelf);
    M_XFldSystemMethodDebug("M_XFldSystem","xfsysM_wpclsQueryIconData");

    if (pIconInfo)
    {
        pIconInfo->fFormat = ICON_RESOURCE;
        pIconInfo->resid   = ID_ICONSYS;
        pIconInfo->hmod    = cmnQueryMainResModuleHandle();
    }

    return sizeof(ICONINFO);
}


