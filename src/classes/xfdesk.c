
/*
 *@@sourcefile xfdesk.c:
 *      This file contains SOM code for the following XWorkplace classes:
 *
 *      --  XFldDesktop (WPDesktop replacement)
 *
 *      XFldDesktop provides access to the eXtended shutdown
 *      feature (fntShutdownThread) by modifying popup menus.
 *
 *      Also, the XFldDesktop settings notebook pages offer access
 *      to startup and shutdown configuration data.
 *
 *      Installation of this class is now optional (V0.9.0).
 *
 *      Starting with V0.9.0, the files in classes\ contain only
 *      i.e. the methods themselves.
 *      The implementation for this class is mostly in filesys\desktop.c,
 *      filesys\shutdown.c, and filesys\archives.c.
 *
 *@@somclass XFldDesktop xfdesk_
 *@@somclass M_XFldDesktop xfdeskM_
 */

/*
 *      Copyright (C) 1997-99 Ulrich M�ller.
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

#ifndef SOM_Module_xfdesk_Source
#define SOM_Module_xfdesk_Source
#endif
#define XFldDesktop_Class_Source
#define M_XFldDesktop_Class_Source

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
#define INCL_DOSSEMAPHORES
#define INCL_DOSERRORS
#define INCL_WINWINDOWMGR
#define INCL_WINMENUS
#include <os2.h>

// C library headers
#include <stdio.h>
#include <io.h>
#include <math.h>

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\winh.h"               // PM helper routines

// SOM headers which don't crash with prec. header files
#include "xfdesk.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling

#include "filesys\desktop.h"            // XFldDesktop implementation
#include "filesys\xthreads.h"           // extra XWorkplace threads

#include "startshut\archives.h"         // WPSArcO declarations
#include "startshut\shutdown.h"         // XWorkplace eXtended Shutdown

// other SOM headers
#pragma hdrstop                 // VAC++ keeps crashing otherwise

/* ******************************************************************
 *                                                                  *
 *   here come the XFldDesktop instance methods                     *
 *                                                                  *
 ********************************************************************/

/*
 *@@ xwpInsertXFldDesktopMenuItemsPage:
 *      this actually adds the new "Menu items" page replacement
 *      to the Desktop's settings notebook.
 *
 *      This gets called from XFldDesktop::wpAddSettingsPages.
 *
 *      added V0.9.0
 */

SOM_Scope ULONG  SOMLINK xfdesk_xwpInsertXFldDesktopMenuItemsPage(XFldDesktop *somSelf,
                                                                 HWND hwndNotebook)
{
    PCREATENOTEBOOKPAGE pcnbp;
    HMODULE         savehmod = cmnQueryNLSModuleHandle(FALSE);
    PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();

    XFldDesktopData *somThis = XFldDesktopGetData(somSelf);
    XFldDesktopMethodDebug("XFldDesktop","xfdesk_xwpInsertXFldDesktopMenuItemsPage");

    pcnbp = malloc(sizeof(CREATENOTEBOOKPAGE));
    memset(pcnbp, 0, sizeof(CREATENOTEBOOKPAGE));
    pcnbp->somSelf = somSelf;
    pcnbp->hwndNotebook = hwndNotebook;
    pcnbp->hmod = savehmod;
    pcnbp->usPageStyleFlags = BKA_MAJOR;
    pcnbp->fEnumerate = TRUE;
    pcnbp->pszName = pNLSStrings->pszDtpMenuPage;
    pcnbp->usFirstControlID = ID_XSDI_DTP_SORT;
    pcnbp->ulFirstSubpanel = ID_XSH_SETTINGS_DTP1_SUB;   // help panel for "Sort"
    pcnbp->ulDlgID = ID_XSD_DTP_MENUITEMS;
    pcnbp->ulDefaultHelpPanel  = ID_XSH_SETTINGS_DTP_MENUITEMS;
    pcnbp->ulPageID = SP_DTP_MENUITEMS;
    pcnbp->pfncbInitPage    = dtpMenuItemsInitPage;
    pcnbp->pfncbItemChanged = dtpMenuItemsItemChanged;
    return (ntbInsertPage(pcnbp));
}

/*
 *@@ xwpInsertXFldDesktopStartupPage:
 *      this actually adds the new "Startup" page replacement
 *      to the Desktop's settings notebook.
 *
 *      This gets called from XFldDesktop::wpAddSettingsPages.
 *
 *      added V0.9.0
 */

SOM_Scope ULONG  SOMLINK xfdesk_xwpInsertXFldDesktopStartupPage(XFldDesktop *somSelf,
                                                               HWND hwndNotebook)
{
    PCREATENOTEBOOKPAGE pcnbp;
    HMODULE         savehmod = cmnQueryNLSModuleHandle(FALSE);
    PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();

    XFldDesktopData *somThis = XFldDesktopGetData(somSelf);
    XFldDesktopMethodDebug("XFldDesktop","xfdesk_xwpInsertXFldDesktopStartupPage");

    pcnbp = malloc(sizeof(CREATENOTEBOOKPAGE));
    memset(pcnbp, 0, sizeof(CREATENOTEBOOKPAGE));
    pcnbp->somSelf = somSelf;
    pcnbp->hwndNotebook = hwndNotebook;
    pcnbp->hmod = savehmod;
    pcnbp->usPageStyleFlags = BKA_MAJOR;
    pcnbp->pszName = pNLSStrings->pszStartupPage;
    pcnbp->ulDlgID = ID_XSD_DTP_STARTUP;
    pcnbp->ulDefaultHelpPanel  = ID_XSH_SETTINGS_DTP_STARTUP;
    pcnbp->ulPageID = SP_DTP_STARTUP;
    pcnbp->pfncbInitPage    = dtpStartupInitPage;
    pcnbp->pfncbItemChanged = dtpStartupItemChanged;
    return (ntbInsertPage(pcnbp));
}

/*
 *@@ xwpInsertXFldDesktopArchivesPage:
 *      this actually adds the new "Archives" page replacement
 *      to the Desktop's settings notebook.
 *
 *      This gets called from XFldDesktop::wpAddSettingsPages.
 *
 *      added V0.9.0
 */

SOM_Scope ULONG  SOMLINK xfdesk_xwpInsertXFldDesktopArchivesPage(XFldDesktop *somSelf,
                                                                HWND hwndNotebook)
{
    PCREATENOTEBOOKPAGE pcnbp;
    HMODULE         savehmod = cmnQueryNLSModuleHandle(FALSE);
    PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();

    XFldDesktopData *somThis = XFldDesktopGetData(somSelf);
    XFldDesktopMethodDebug("XFldDesktop","xfdesk_xwpInsertXFldDesktopArchivesPage");

    pcnbp = malloc(sizeof(CREATENOTEBOOKPAGE));
    memset(pcnbp, 0, sizeof(CREATENOTEBOOKPAGE));
    pcnbp->somSelf = somSelf;
    pcnbp->hwndNotebook = hwndNotebook;
    pcnbp->hmod = savehmod;
    pcnbp->usPageStyleFlags = BKA_MAJOR;
    pcnbp->pszName = pNLSStrings->pszArchivesPage;
    pcnbp->ulDlgID = ID_XSD_DTP_ARCHIVES;
    // pcnbp->usFirstControlID = ID_SDDI_ARCHIVES;
    // pcnbp->ulFirstSubpanel = ID_XSH_SETTINGS_DTP_SHUTDOWN_SUB;   // help panel for "System setup"
    pcnbp->ulDefaultHelpPanel  = ID_XSH_SETTINGS_DTP_ARCHIVES;
    pcnbp->ulPageID = SP_DTP_ARCHIVES;
    pcnbp->pfncbInitPage    = arcArchivesInitPage;
    pcnbp->pfncbItemChanged = arcArchivesItemChanged;
    return (ntbInsertPage(pcnbp));
}

/*
 *@@ xwpInsertXFldDesktopShutdownPage:
 *      this actually adds the new "Shutdown" page replacement
 *      to the Desktop's settings notebook.
 *
 *      This gets called from XFldDesktop::wpAddSettingsPages.
 *
 *      added V0.9.0
 */

SOM_Scope ULONG  SOMLINK xfdesk_xwpInsertXFldDesktopShutdownPage(XFldDesktop *somSelf,
                                                                HWND hwndNotebook)
{
    PCREATENOTEBOOKPAGE pcnbp;
    HMODULE         savehmod = cmnQueryNLSModuleHandle(FALSE);
    PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();

    XFldDesktopData *somThis = XFldDesktopGetData(somSelf);
    XFldDesktopMethodDebug("XFldDesktop","xfdesk_xwpInsertXFldDesktopShutdownPage");

    // insert "XShutdown" page,
    // if Shutdown has been enabled in XWPConfig
    pcnbp = malloc(sizeof(CREATENOTEBOOKPAGE));
    memset(pcnbp, 0, sizeof(CREATENOTEBOOKPAGE));
    pcnbp->somSelf = somSelf;
    pcnbp->hwndNotebook = hwndNotebook;
    pcnbp->hmod = savehmod;
    pcnbp->usPageStyleFlags = BKA_MAJOR;
    pcnbp->pszName = pNLSStrings->pszXShutdownPage;
    pcnbp->ulDlgID = ID_XSD_DTP_SHUTDOWN;
    pcnbp->usFirstControlID = ID_SDDI_REBOOT;
    pcnbp->ulFirstSubpanel = ID_XSH_SETTINGS_DTP_SHUTDOWN_SUB;   // help panel for "System setup"
    pcnbp->ulDefaultHelpPanel  = ID_XSH_SETTINGS_DTP_SHUTDOWN;
    pcnbp->ulPageID = SP_DTP_SHUTDOWN;
    pcnbp->pfncbInitPage    = xsdShutdownInitPage;
    pcnbp->pfncbItemChanged = xsdShutdownItemChanged;
    return (ntbInsertPage(pcnbp));
}

/*
 *@@ wpFilterPopupMenu:
 *      remove "Create another" for Desktop, because
 *      we don't want to allow creating another Desktop.
 *      For some reason, the "Create another" option
 *      doesn't seem to be working right with XFolder,
 *      so we need to add all this manually (see
 *      XFldObject::wpFilterPopupMenu).
 */

SOM_Scope ULONG  SOMLINK xfdesk_wpFilterPopupMenu(XFldDesktop *somSelf,
                                                  ULONG ulFlags,
                                                  HWND hwndCnr,
                                                  BOOL fMultiSelect)
{
    // items to suppress
    ULONG   ulSuppress = CTXT_CRANOTHER;
    PCGLOBALSETTINGS     pGlobalSettings = cmnQueryGlobalSettings();

    // XFldDesktopData *somThis = XFldDesktopGetData(somSelf);
    XFldDesktopMethodDebug("XFldDesktop","xfdesk_wpFilterPopupMenu");

    // suppress sort menu?
    if (!pGlobalSettings->fDTMSort)
        ulSuppress |= CTXT_SORT;
    if (!pGlobalSettings->fDTMArrange)
        ulSuppress |= CTXT_ARRANGE;

    return (XFldDesktop_parent_WPDesktop_wpFilterPopupMenu(somSelf,
                                                           ulFlags,
                                                           hwndCnr,
                                                           fMultiSelect)
            & ~(ulSuppress));
}

/*
 *@@ wpModifyPopupMenu:
 *      play with the Desktop menu entries
 *      (Shutdown and such)
 *
 *@@changed V0.9.0 [umoeller]: reworked context menu items
 */

SOM_Scope BOOL  SOMLINK xfdesk_wpModifyPopupMenu(XFldDesktop *somSelf,
                                                 HWND hwndMenu,
                                                 HWND hwndCnr,
                                                 ULONG iPosition)
{
    BOOL rc;

    // XFldDesktopData *somThis = XFldDesktopGetData(somSelf);
    XFldDesktopMethodDebug("XFldDesktop","xfdesk_wpModifyPopupMenu");

    // calling the parent (which is XFolder!) will insert all the
    // variable menu items
    rc = XFldDesktop_parent_WPDesktop_wpModifyPopupMenu(somSelf,
                                                           hwndMenu,
                                                           hwndCnr,
                                                           iPosition);

    if (rc)
        if (_wpIsCurrentDesktop(somSelf))
            dtpModifyPopupMenu(somSelf,
                               hwndMenu);

    return (rc);
}

/*
 *@@ wpMenuItemSelected:
 *      process input when any menu item was selected;
 *      intercept Shutdown and such
 */

SOM_Scope BOOL  SOMLINK xfdesk_wpMenuItemSelected(XFldDesktop *somSelf,
                                                  HWND hwndFrame,
                                                  ULONG ulMenuId)
{
    // XFldDesktopData *somThis = XFldDesktopGetData(somSelf);
    XFldDesktopMethodDebug("XFldDesktop","xfdesk_wpMenuItemSelected");

    if (dtpMenuItemSelected(somSelf, hwndFrame, ulMenuId))
        // one of the new items processed:
        return (TRUE);
    else
        return (XFldDesktop_parent_WPDesktop_wpMenuItemSelected(somSelf,
                                                                hwndFrame,
                                                                ulMenuId));
}

/*
 *@@ wpAddDesktopArcRest1Page:
 *      this instance method inserts the "Archives" page
 *      into the Desktop's settings notebook. If the
 *      extended archiving has been enabled, we return
 *      SETTINGS_PAGE_REMOVED because we want the "Archives"
 *      page at a different position, which gets inserted
 *      from XFldDesktop::wpAddSettingsPages then.
 *
 *@@added V0.9.0 [umoeller]
 */

SOM_Scope ULONG  SOMLINK xfdesk_wpAddDesktopArcRest1Page(XFldDesktop *somSelf,
                                                         HWND hwndNotebook)
{
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    // XFldDesktopData *somThis = XFldDesktopGetData(somSelf);
    XFldDesktopMethodDebug("XFldDesktop","xfdesk_wpAddDesktopArcRest1Page");

    if (pGlobalSettings->fReplaceArchiving)
        // remove this, we'll add a new one at a different
        // location in wpAddSettingsPages
        return (SETTINGS_PAGE_REMOVED);
    else
        return (XFldDesktop_parent_WPDesktop_wpAddDesktopArcRest1Page(somSelf,
                                                                      hwndNotebook));
}

/*
 *@@ wpAddSettingsPages:
 *      insert additional settings page into the Desktop's settings
 *      notebook.
 *
 *      As opposed to the "XFolder" page, which deals with instance
 *      data, we save the Desktop settings in the GLOBALSETTINGS
 *      structure, because there should ever be only one active
 *      Desktop.
 *
 *@@changed V0.9.0 [umoeller]: reworked settings pages
 */

SOM_Scope BOOL  SOMLINK xfdesk_wpAddSettingsPages(XFldDesktop *somSelf,
                                                  HWND hwndNotebook)
{
    BOOL            rc;

    // XFldDesktopData *somThis = XFldDesktopGetData(somSelf);
    XFldDesktopMethodDebug("XFldDesktop","xfdesk_wpAddSettingsPages");

    rc = (XFldDesktop_parent_WPDesktop_wpAddSettingsPages(somSelf,
                                                            hwndNotebook));
    if (rc)
        if (_wpIsCurrentDesktop(somSelf))
        {
            PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

            // insert "Menu" page
            _xwpInsertXFldDesktopMenuItemsPage(somSelf, hwndNotebook);

            if (pGlobalSettings->fXShutdown)
                _xwpInsertXFldDesktopShutdownPage(somSelf, hwndNotebook);

            if (pGlobalSettings->fReplaceArchiving)
                // insert new "Archives" page;
                // at the same time, the old archives page method
                // will return SETTINGS_PAGE_REMOVED
                _xwpInsertXFldDesktopArchivesPage(somSelf, hwndNotebook);

            // insert "Startup" page
            _xwpInsertXFldDesktopStartupPage(somSelf, hwndNotebook);
        }

    return (rc);
}

/*
 *@@ wpPopulate:
 *      this instance method populates a folder, in this case, the
 *      Desktop. After the active Desktop has been populated at
 *      WPS startup, we'll post a message to the Worker thread to
 *      initiate all the XWorkplace startup processing.
 *
 *@@changed V0.9.0 [umoeller]: this was previously done in wpOpen
 */

SOM_Scope BOOL  SOMLINK xfdesk_wpPopulate(XFldDesktop *somSelf,
                                          ULONG ulReserved,
                                          PSZ pszPath,
                                          BOOL fFoldersOnly)
{
    BOOL    brc = FALSE;
    XFldDesktopData *somThis = XFldDesktopGetData(somSelf);
    XFldDesktopMethodDebug("XFldDesktop","xfdesk_wpPopulate");

    brc = XFldDesktop_parent_WPDesktop_wpPopulate(somSelf,
                                                    ulReserved,
                                                    pszPath,
                                                    fFoldersOnly);

    if (!_fDesktopOpen)
        if (_wpIsCurrentDesktop(somSelf))
        {
            _fDesktopOpen = TRUE;
            // notify file thread to start processing
            xthrPostFileMsg(FIM_DESKTOPPOPULATED,
                            (MPARAM)somSelf,
                            MPNULL);
        }

    return (brc);
}

/*
 *@@ wpInitData:
 *      this instance method gets called when the object
 *      is being initialized. We initialize our instance
 *      data here.
 */

SOM_Scope void  SOMLINK xfdesk_wpInitData(XFldDesktop *somSelf)
{
    XFldDesktopData *somThis = XFldDesktopGetData(somSelf);
    XFldDesktopMethodDebug("XFldDesktop","xfdesk_wpInitData");

    _fDesktopOpen = FALSE;
    _fInsertArchivesPageNow = FALSE;

    XFldDesktop_parent_WPDesktop_wpInitData(somSelf);
}

/*
 *@@ wpclsInitData:
 *      initialize XFldDesktop class data.
 *
 *@@changed V0.9.0 [umoeller]: added class object to KERNELGLOBALS
 */

SOM_Scope void  SOMLINK xfdeskM_wpclsInitData(M_XFldDesktop *somSelf)
{
    // M_XFldDesktopData *somThis = M_XFldDesktopGetData(somSelf);
    M_XFldDesktopMethodDebug("M_XFldDesktop","xfdeskM_wpclsInitData");

    M_XFldDesktop_parent_M_WPDesktop_wpclsInitData(somSelf);

    {
        PKERNELGLOBALS   pKernelGlobals = krnLockGlobals(5000);
        // store the class object in KERNELGLOBALS
        pKernelGlobals->fXFldDesktop = TRUE;
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

SOM_Scope BOOL  SOMLINK xfdeskM_wpclsQuerySettingsPageSize(M_XFldDesktop *somSelf,
                                                           PSIZEL pSizl)
{
    BOOL brc;
    /* M_XFldDesktopData *somThis = M_XFldDesktopGetData(somSelf); */
    M_XFldDesktopMethodDebug("M_XFldDesktop","xfdeskM_wpclsQuerySettingsPageSize");

    brc = M_XFldDesktop_parent_M_WPDesktop_wpclsQuerySettingsPageSize(somSelf,
                                                                        pSizl);
    if (brc)
    {
        LONG lCompCY = 153;
        if (doshIsWarp4())
            // on Warp 4, reduce again, because we're moving
            // the notebook buttons to the bottom
            lCompCY -= WARP4_NOTEBOOK_OFFSET;

        if (pSizl->cy < lCompCY)
            pSizl->cy = lCompCY;  // this is the height of the "XDesktop" page,
                                // which is pretty large
        if (pSizl->cx < 260)
            pSizl->cx = 260;    // and the width

    }
    return (brc);
}

/*
 *@@ wpclsQueryIconData:
 *      give XFldDesktop's a new default closed icon, if the
 *      global settings allow this.
 *      This is loaded from /ICONS/ICONS.DLL.
 */

SOM_Scope ULONG  SOMLINK xfdeskM_wpclsQueryIconData(M_XFldDesktop *somSelf,
                                                    PICONINFO pIconInfo)
{
    ULONG       ulrc;
    HMODULE     hmodIconsDLL = NULLHANDLE;
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    // M_XFldDesktopData *somThis = M_XFldDesktopGetData(somSelf);
    M_XFldDesktopMethodDebug("M_XFldDesktop","xfdeskM_wpclsQueryIconData");

    if (pGlobalSettings->fReplaceIcons)
    {
        hmodIconsDLL = cmnQueryIconsDLL();
        // icon replacements allowed:
        if ((pIconInfo) && (hmodIconsDLL))
        {
            pIconInfo->fFormat = ICON_RESOURCE;
            pIconInfo->hmod = hmodIconsDLL;
            pIconInfo->resid = 110;
        }
        ulrc = sizeof(ICONINFO);
    }

    if (hmodIconsDLL == NULLHANDLE)
        // icon replacements not allowed: call default
        ulrc = M_XFldDesktop_parent_M_WPDesktop_wpclsQueryIconData(somSelf,
                                                                   pIconInfo);
    return (ulrc);
}

/*
 *@@ wpclsQueryIconDataN:
 *      give XFldDesktop's a new open closed icon, if the
 *      global settings allow this.
 *      This is loaded from /ICONS/ICONS.DLL.
 */

SOM_Scope ULONG  SOMLINK xfdeskM_wpclsQueryIconDataN(M_XFldDesktop *somSelf,
                                                     ICONINFO* pIconInfo,
                                                     ULONG ulIconIndex)
{
    ULONG       ulrc;
    HMODULE     hmodIconsDLL = NULLHANDLE;
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    // M_XFldDesktopData *somThis = M_XFldDesktopGetData(somSelf);
    M_XFldDesktopMethodDebug("M_XFldDesktop","xfdeskM_wpclsQueryIconDataN");

    if (pGlobalSettings->fReplaceIcons)
    {
        hmodIconsDLL = cmnQueryIconsDLL();
        // icon replacements allowed:
        if ((pIconInfo) && (hmodIconsDLL))
        {
            pIconInfo->fFormat = ICON_RESOURCE;
            pIconInfo->hmod = hmodIconsDLL;
            pIconInfo->resid = 111;
        }
        ulrc = sizeof(ICONINFO);
    }

    if (hmodIconsDLL == NULLHANDLE)
        // icon replacements not allowed: call default
        ulrc = M_XFldDesktop_parent_M_WPDesktop_wpclsQueryIconDataN(somSelf,
                                                                    pIconInfo,
                                                                    ulIconIndex);
    return (ulrc);
}

