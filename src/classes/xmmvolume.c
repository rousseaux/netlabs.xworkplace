
/*
 *@@sourcefile xmmvolume.c:
 *      This file contains SOM code for the following XWorkplace classes:
 *
 *      --  XMMVolume: a new MMPM/2 "Master volume" control.
 *
 *      This class is new with V0.9.6.
 *
 *      Installation of this class is completely optional.
 *
 *@@added V0.9.6 (2000-11-09) [umoeller]
 *@@somclass XMMVolume vol_
 *@@somclass M_XMMVolume volM_
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
 *@@todo:
 *
 */

/*
 *  This file was generated by the SOM Compiler and Emitter Framework.
 *  Generated using:
 *      SOM Emitter emitctm: 2.41
 */

#ifndef SOM_Module_xmmvolume_Source
#define SOM_Module_xmmvolume_Source
#endif
#define XMMVolume_Class_Source
#define M_XMMVolume_Class_Source

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
#define INCL_WINMENUS
#include <os2.h>

// C library headers
#include <stdio.h>              // needed for except.h
#include <setjmp.h>

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\winh.h"               // PM helper routines

// SOM headers which don't crash with prec. header files
#include "xmmvolume.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel

#include "media\media.h"                // XWorkplace multimedia support

// other SOM headers

#pragma hdrstop

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

/* ******************************************************************
 *
 *   XMMVolume Instance Methods
 *
 ********************************************************************/

SOM_Scope ULONG  SOMLINK vol_xwpAddXMMVolumePages(XMMVolume *somSelf,
                                                  HWND hwndNotebook)
{
    /* XMMVolumeData *somThis = XMMVolumeGetData(somSelf); */
    XMMVolumeMethodDebug("XMMVolume","vol_xwpAddXMMVolumePages");

    return (0);
}

/*
 *@@ wpInitData:
 *      this WPObject instance method gets called when the
 *      object is being initialized (on wake-up or creation).
 *      We initialize our additional instance data here.
 *      Always call the parent first.
 */

SOM_Scope void  SOMLINK vol_wpInitData(XMMVolume *somSelf)
{
    /* XMMVolumeData *somThis = XMMVolumeGetData(somSelf); */
    XMMVolumeMethodDebug("XMMVolume","vol_wpInitData");

    XMMVolume_parent_WPAbstract_wpInitData(somSelf);
}

/*
 *@@ wpUnInitData:
 *      this WPObject instance method is called when the object
 *      is destroyed as a SOM object, either because it's being
 *      made dormant or being deleted. All allocated resources
 *      should be freed here.
 *      The parent method must always be called last.
 */

SOM_Scope void  SOMLINK vol_wpUnInitData(XMMVolume *somSelf)
{
    /* XMMVolumeData *somThis = XMMVolumeGetData(somSelf); */
    XMMVolumeMethodDebug("XMMVolume","vol_wpUnInitData");

    XMMVolume_parent_WPAbstract_wpUnInitData(somSelf);
}

/*
 *@@ wpObjectReady:
 *      this WPObject notification method gets called by the
 *      WPS when object instantiation is complete, for any reason.
 *      ulCode and refObject signify why and where from the
 *      object was created.
 *      The parent method must be called first.
 *
 *      Even though WPSREF doesn't really say so, this method
 *      must be used similar to a C++ copy constructor
 *      when the instance data contains pointers. Since when
 *      objects are copied, SOM just copies the binary instance
 *      data, you get two objects with instance pointers pointing
 *      to the same object, which can only lead to problems.
 */

SOM_Scope void  SOMLINK vol_wpObjectReady(XMMVolume *somSelf,
                                          ULONG ulCode, WPObject* refObject)
{
    /* XMMVolumeData *somThis = XMMVolumeGetData(somSelf); */
    XMMVolumeMethodDebug("XMMVolume","vol_wpObjectReady");

    XMMVolume_parent_WPAbstract_wpObjectReady(somSelf, ulCode,
                                              refObject);
}

/*
 *@@ wpSaveState:
 *      this WPObject instance method saves an object's state
 *      persistently so that it can later be re-initialized
 *      with wpRestoreState. This gets called during wpClose,
 *      wpSaveImmediate or wpSaveDeferred processing.
 *      All persistent instance variables should be stored here.
 */

SOM_Scope BOOL  SOMLINK vol_wpSaveState(XMMVolume *somSelf)
{
    /* XMMVolumeData *somThis = XMMVolumeGetData(somSelf); */
    XMMVolumeMethodDebug("XMMVolume","vol_wpSaveState");

    return (XMMVolume_parent_WPAbstract_wpSaveState(somSelf));
}

/*
 *@@ wpRestoreState:
 *      this WPObject instance method gets called during object
 *      initialization (after wpInitData) to restore the data
 *      which was stored with wpSaveState.
 */

SOM_Scope BOOL  SOMLINK vol_wpRestoreState(XMMVolume *somSelf,
                                           ULONG ulReserved)
{
    /* XMMVolumeData *somThis = XMMVolumeGetData(somSelf); */
    XMMVolumeMethodDebug("XMMVolume","vol_wpRestoreState");

    return (XMMVolume_parent_WPAbstract_wpRestoreState(somSelf,
                                                       ulReserved));
}

/*
 *@@ wpFilterPopupMenu:
 *      this WPObject instance method allows the object to
 *      filter out unwanted menu items from the context menu.
 *      This gets called before wpModifyPopupMenu.
 */

SOM_Scope ULONG  SOMLINK vol_wpFilterPopupMenu(XMMVolume *somSelf,
                                               ULONG ulFlags,
                                               HWND hwndCnr,
                                               BOOL fMultiSelect)
{
    /* XMMVolumeData *somThis = XMMVolumeGetData(somSelf); */
    XMMVolumeMethodDebug("XMMVolume","vol_wpFilterPopupMenu");

    return (XMMVolume_parent_WPAbstract_wpFilterPopupMenu(somSelf,
                                                          ulFlags,
                                                          hwndCnr,
                                                          fMultiSelect));
}

/*
 *@@ wpModifyPopupMenu:
 *      this WPObject instance methods gets called by the WPS
 *      when a context menu needs to be built for the object
 *      and allows the object to manipulate its context menu.
 *      This gets called _after_ wpFilterPopupMenu.
 */

SOM_Scope BOOL  SOMLINK vol_wpModifyPopupMenu(XMMVolume *somSelf,
                                              HWND hwndMenu,
                                              HWND hwndCnr, ULONG iPosition)
{
    BOOL brc = FALSE;
    /* XMMVolumeData *somThis = XMMVolumeGetData(somSelf); */
    XMMVolumeMethodDebug("XMMVolume","vol_wpModifyPopupMenu");

    brc = XMMVolume_parent_WPAbstract_wpModifyPopupMenu(somSelf,
                                                        hwndMenu,
                                                        hwndCnr,
                                                        iPosition);
    if (brc)
    {
        MENUITEM mi;
        // get handle to the "Open" submenu in the
        // the popup menu
        if (WinSendMsg(hwndMenu,
                       MM_QUERYITEM,
                       MPFROM2SHORT(WPMENUID_OPEN, TRUE),
                       (MPARAM)&mi))
        {
            // mi.hwndSubMenu now contains "Open" submenu handle,
            // which we add items to now
            PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
            PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();
            winhInsertMenuItem(mi.hwndSubMenu, MIT_END,
                               (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_OPENCLASSLIST),
                               "~Volume", // ###
                               MIS_TEXT, 0);
        }
    }

    return (brc);
}

/*
 *@@ wpMenuItemSelected:
 *      this WPObject method processes menu selections.
 *      This must be overridden to support new menu
 *      items which have been added in wpModifyPopupMenu.
 */

SOM_Scope BOOL  SOMLINK vol_wpMenuItemSelected(XMMVolume *somSelf,
                                               HWND hwndFrame,
                                               ULONG ulMenuId)
{
    BOOL brc = FALSE;
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    /* XMMVolumeData *somThis = XMMVolumeGetData(somSelf); */
    XMMVolumeMethodDebug("XMMVolume","vol_wpMenuItemSelected");

    if (ulMenuId == (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_OPENCLASSLIST))
    {
        // "Open" --> "Volume":
        // wpViewObject will call wpOpen if a new view is necessary
        _wpViewObject(somSelf,
                      NULLHANDLE,   // hwndCnr; "WPS-internal use only", IBM says
                      ulMenuId,     // ulView; must be the same as menu item
                      0);           // parameter passed to wpOpen
        brc = TRUE;
    }
    else
        brc = XMMVolume_parent_WPAbstract_wpMenuItemSelected(somSelf,
                                                             hwndFrame,
                                                             ulMenuId);
    return (brc);
}

/*
 *@@ wpMenuItemHelpSelected:
 *
 */

SOM_Scope BOOL  SOMLINK vol_wpMenuItemHelpSelected(XMMVolume *somSelf,
                                                   ULONG MenuId)
{
    /* XMMVolumeData *somThis = XMMVolumeGetData(somSelf); */
    XMMVolumeMethodDebug("XMMVolume","vol_wpMenuItemHelpSelected");

    // ###

    return (XMMVolume_parent_WPAbstract_wpMenuItemHelpSelected(somSelf,
                                                               MenuId));
}

/*
 *@@ wpQueryDefaultHelp:
 *      this WPObject instance method specifies the default
 *      help panel for an object (when "Extended help" is
 *      selected from the object's context menu). This should
 *      describe what this object can do in general.
 *      We must return TRUE to report successful completion.
 */

SOM_Scope BOOL  SOMLINK vol_wpQueryDefaultHelp(XMMVolume *somSelf,
                                               PULONG pHelpPanelId,
                                               PSZ HelpLibrary)
{
    /* XMMVolumeData *somThis = XMMVolumeGetData(somSelf); */
    XMMVolumeMethodDebug("XMMVolume","vol_wpQueryDefaultHelp");

    return (XMMVolume_parent_WPAbstract_wpQueryDefaultHelp(somSelf,
                                                           pHelpPanelId,
                                                           HelpLibrary));
}

/*
 *@@ wpQueryDefaultView:
 *      this WPObject method returns the default view of an object,
 *      that is, which view is opened if the program file is
 *      double-clicked upon. This is also used to mark
 *      the default view in the "Open" context submenu.
 *
 *      This must be overridden for direct WPAbstract subclasses,
 *      because otherwise double-clicks on the object won't
 *      work.
 */

SOM_Scope ULONG  SOMLINK vol_wpQueryDefaultView(XMMVolume *somSelf)
{
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    /* XMMVolumeData *somThis = XMMVolumeGetData(somSelf); */
    XMMVolumeMethodDebug("XMMVolume","vol_wpQueryDefaultView");

    return (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_OPENCLASSLIST);
}

/*
 *@@ wpOpen:
 *      this WPObject instance method gets called when
 *      a new view needs to be opened. Normally, this
 *      gets called after wpViewObject has scanned the
 *      object's USEITEMs and has determined that a new
 *      view is needed.
 *
 *      This _normally_ runs on thread 1 of the WPS, but
 *      this is not always the case. If this gets called
 *      in response to a menu selection from the "Open"
 *      submenu or a double-click in the folder, this runs
 *      on the thread of the folder (which _normally_ is
 *      thread 1). However, if this results from WinOpenObject
 *      or an OPEN setup string, this will not be on thread 1.
 */

SOM_Scope HWND  SOMLINK vol_wpOpen(XMMVolume *somSelf, HWND hwndCnr,
                                   ULONG ulView, ULONG param)
{
    HWND    hwndNewView = 0;
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    /* XMMVolumeData *somThis = XMMVolumeGetData(somSelf); */
    XMMVolumeMethodDebug("XMMVolume","vol_wpOpen");

    if (ulView == (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_OPENCLASSLIST))
        hwndNewView = xmmCreateVolumeView(somSelf, hwndCnr, ulView);
                                // src/media/mmvolume.c
    else
        // other view (probably settings):
        hwndNewView = XMMVolume_parent_WPAbstract_wpOpen(somSelf,
                                                         hwndCnr,
                                                         ulView,
                                                         param);
    return (hwndNewView);
}

/*
 *@@ wpAddObjectWindowPage:
 *      this WPObject instance method normally adds the
 *      "Standard Options" page to the settings notebook
 *      (that's what the WPS reference calls it; it's actually
 *      the "Window" page).
 */

SOM_Scope ULONG  SOMLINK vol_wpAddObjectWindowPage(XMMVolume *somSelf,
                                                   HWND hwndNotebook)
{
    /* XMMVolumeData *somThis = XMMVolumeGetData(somSelf); */
    XMMVolumeMethodDebug("XMMVolume","vol_wpAddObjectWindowPage");

    return (SETTINGS_PAGE_REMOVED);
}

/*
 *@@ wpAddSettingsPages:
 *      this WPObject instance method gets called by the WPS
 *      when the Settings view is opened to have all the
 *      settings page inserted into hwndNotebook.
 *
 *      We call XCenter::xwpAddXXenterPages to have the
 *      "XCenter" pages inserted on top.
 */

SOM_Scope BOOL  SOMLINK vol_wpAddSettingsPages(XMMVolume *somSelf,
                                               HWND hwndNotebook)
{
    BOOL brc = FALSE;
    /* XMMVolumeData *somThis = XMMVolumeGetData(somSelf); */
    XMMVolumeMethodDebug("XMMVolume","vol_wpAddSettingsPages");

    brc = XMMVolume_parent_WPAbstract_wpAddSettingsPages(somSelf,
                                                         hwndNotebook);
    if (brc)
        _xwpAddXMMVolumePages(somSelf, hwndNotebook);

    return (brc);
}

/* ******************************************************************
 *
 *   XMMVolume Class Methods
 *
 ********************************************************************/

/*
 *@@ wpclsInitData:
 *      initialize XCenter class data.
 */

SOM_Scope void  SOMLINK volM_wpclsInitData(M_XMMVolume *somSelf)
{
    /* M_XMMVolumeData *somThis = M_XMMVolumeGetData(somSelf); */
    M_XMMVolumeMethodDebug("M_XMMVolume","volM_wpclsInitData");

    M_XMMVolume_parent_M_WPAbstract_wpclsInitData(somSelf);

    {
        PKERNELGLOBALS   pKernelGlobals = krnLockGlobals(5000);
        // store the class object in KERNELGLOBALS
        pKernelGlobals->fXMMVolume = TRUE;
        krnUnlockGlobals();
    }
}

/*
 *@@ wpclsQueryStyle:
 *      prevent print, template.
 */

SOM_Scope ULONG  SOMLINK volM_wpclsQueryStyle(M_XMMVolume *somSelf)
{
    /* M_XMMVolumeData *somThis = M_XMMVolumeGetData(somSelf); */
    M_XMMVolumeMethodDebug("M_XMMVolume","volM_wpclsQueryStyle");

    return (M_XMMVolume_parent_M_WPAbstract_wpclsQueryStyle(somSelf)
                | CLSSTYLE_NEVERPRINT
                | CLSSTYLE_NEVERTEMPLATE);
}

/*
 *@@ wpclsQueryTitle:
 *      tell the WPS the new class default title for XCenter.
 */

SOM_Scope PSZ  SOMLINK volM_wpclsQueryTitle(M_XMMVolume *somSelf)
{
    /* M_XMMVolumeData *somThis = M_XMMVolumeGetData(somSelf); */
    M_XMMVolumeMethodDebug("M_XMMVolume","volM_wpclsQueryTitle");

    return ("Volume");      // ###
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
 *      We override this to give the XMMVolume object a new
 *      icon (src\shared\volume.ico).
 */

SOM_Scope ULONG  SOMLINK volM_wpclsQueryIconData(M_XMMVolume *somSelf,
                                                 PICONINFO pIconInfo)
{
    /* M_XMMVolumeData *somThis = M_XMMVolumeGetData(somSelf); */
    M_XMMVolumeMethodDebug("M_XMMVolume","volM_wpclsQueryIconData");

    if (pIconInfo) {
       pIconInfo->fFormat = ICON_RESOURCE;
       pIconInfo->resid   = ID_ICONXMMVOLUME;
       pIconInfo->hmod    = cmnQueryMainModuleHandle();
    }

    return (sizeof(ICONINFO));
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

SOM_Scope BOOL  SOMLINK volM_wpclsQuerySettingsPageSize(M_XMMVolume *somSelf,
                                                        PSIZEL pSizl)
{
    /* M_XMMVolumeData *somThis = M_XMMVolumeGetData(somSelf); */
    M_XMMVolumeMethodDebug("M_XMMVolume","volM_wpclsQuerySettingsPageSize");

    return (M_XMMVolume_parent_M_WPAbstract_wpclsQuerySettingsPageSize(somSelf,
                                                                       pSizl));
}

/*
 *@@ wpclsCreateDefaultTemplates:
 *      this WPObject class method is called by the
 *      Templates folder to allow a class to
 *      create its default templates.
 *
 *      The default WPS behavior is to create new templates
 *      if the class default title is different from the
 *      existing templates.
 */

SOM_Scope BOOL  SOMLINK volM_wpclsCreateDefaultTemplates(M_XMMVolume *somSelf,
                                                         WPObject* Folder)
{
    /* M_XMMVolumeData *somThis = M_XMMVolumeGetData(somSelf); */
    M_XMMVolumeMethodDebug("M_XMMVolume","volM_wpclsCreateDefaultTemplates");

    return (M_XMMVolume_parent_M_WPAbstract_wpclsCreateDefaultTemplates(somSelf,
                                                                        Folder));
}

