
/*
 *@@sourcefile xfdisk.c:
 *      This file contains SOM code for the following XWorkplace classes:
 *
 *      --  XFldDisk (WPDisk replacement)
 *
 *      XFldDisk is needed mainly to modify the popup menus
 *      of Disk objects also. Since these are not instances
 *      of WPFolder, we need an extra subclass.
 *
 *      Installation of this class is now optional (V0.9.0).
 *      However, if it is installed, XFolder must also be
 *      installed.
 *
 *      Starting with V0.9.0, the files in classes\ contain only
 *      i.e. the methods themselves.
 *      The implementation for this class is mostly in filesys\disk.c.
 *
 *@@somclass XFldDisk xfdisk_
 *@@somclass M_XFldDisk xfdiskM_
 */

/*
 *      Copyright (C) 1997-2002 Ulrich M�ller.
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
 *      SOM Emitter emitctm: 2.42
 */

#ifndef SOM_Module_xfdisk_Source
#define SOM_Module_xfdisk_Source
#endif
#define XFldDisk_Class_Source
#define M_XFldDisk_Class_Source

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
#define INCL_DOSDEVIOCTL                // to get the disk constants in dosh.h

#define INCL_WINFRAMEMGR
#define INCL_WINMENUS
#include  <os2.h>

// C library headers
#include <stdio.h>

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\winh.h"               // PM helper routines
#include "helpers\standards.h"          // some standard macros

// SOM headers which don't crash with prec. header files
#include "xfdisk.ih"
#include "xfldr.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\helppanels.h"          // all XWorkplace help panel IDs
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling

#include "filesys\disk.h"               // XFldDisk implementation
#include "filesys\folder.h"             // XFolder implementation
#include "filesys\fdrcommand.h"         // folder command handling
#include "filesys\fdrmenus.h"           // shared folder menu logic
#include "filesys\fdrsubclass.h"        // folder subclassing engine
#include "filesys\fdrsplit.h"           // folder split views
// #include "filesys\object.h"             // XFldObject implementation
#include "filesys\statbars.h"           // status bar translation logic

// other SOM headers
#pragma hdrstop

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

/* ******************************************************************
 *
 *   here come the XFldDisk instance methods
 *
 ********************************************************************/

/*
 *@@ xwpSafeQueryRootFolder:
 *      safe version of WPDisk::wpQueryRootFolder which
 *      attempts not to provoke the terrible white
 *      hard-error box if the disk is not ready.
 *
 *      This used to be wpshQueryRootFolder, but has
 *      been made an instance method with V0.9.19.
 *
 *      On errors, NULL is returned, and *pulErrorCode
 *      receives the APIRET from doshAssertDrive.
 *      If you're not interested in the return code,
 *      you may pass pulErrorCode as NULL.
 *
 *@@added V0.9.19 (2002-06-15) [umoeller]
 */

SOM_Scope WPRootFolder*  SOMLINK xfdisk_xwpSafeQueryRootFolder(XFldDisk *somSelf,
                                                               BOOL fForceMap,
                                                               PULONG pulErrorCode)
{
    WPFolder    *pReturnFolder = NULL;
    APIRET      arc = NO_ERROR;
    ULONG       ulLogicalDrive;

    /* XFldDiskData *somThis = XFldDiskGetData(somSelf); */
    XFldDiskMethodDebug("XFldDisk","xfdisk_xwpSafeQueryRootFolder");

    ulLogicalDrive = _wpQueryLogicalDrive(somSelf);
    arc = doshAssertDrive(ulLogicalDrive, 0);

    if (    (arc == ERROR_DISK_CHANGE)
         && (fForceMap)
         && ((ulLogicalDrive == 1) || (ulLogicalDrive == 2))
         && (doshSetLogicalMap(ulLogicalDrive) == NO_ERROR)
       )
    {
        arc = doshAssertDrive(ulLogicalDrive, 0);
    }

    if (!arc)
    {
        // drive seems to be ready:
        if (!(pReturnFolder = _wpQueryRootFolder(somSelf)))
            // still NULL: something bad is going on
            // V0.9.2 (2000-03-09) [umoeller]
            arc = ERROR_NOT_DOS_DISK; // 26; cannot access disk
    }

    if (pulErrorCode)
        *pulErrorCode = arc;

    return pReturnFolder;
}

/*
 *@@ wpInitData:
 *      this WPObject instance method gets called when the
 *      object is being initialized (on wake-up or creation).
 *      We initialize our additional instance data here.
 *      Always call the parent first.
 *
 *@@added V0.9.21 (2002-08-31) [umoeller]
 */

SOM_Scope void  SOMLINK xfdisk_wpInitData(XFldDisk *somSelf)
{
    XFldDiskData *somThis = XFldDiskGetData(somSelf);
    XFldDiskMethodDebug("XFldDisk","xfdisk_wpInitData");

    XFldDisk_parent_WPDisk_wpInitData(somSelf);

    _pMenuRootFolder = (WPObject*)-1;
}

/*
 *@@ wpFilterMenu:
 *      this WPObject instance method was new with Warp 4
 *      allows the object to filter out standard menu items
 *      in a more fine-grained way than wpModifyPopupMenu.
 *      For one, this gives the object the menu type that
 *      is being built, and secondly this allows for
 *      filtering more items than the 32 bits of the filter
 *      flag in wpFilterPopupMenu have room for.
 *
 *      Once again however, the description of this method
 *      in WPSREF is unusable. As input, this method
 *      receives an array of ULONGs in pFlags. wpobject.h
 *      in the 4.5 Toolkit lists the CTXT_* flags together
 *      with the ULONG array item index where each flag
 *      applies.
 *
 *@@added V0.9.21 (2002-08-31) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xfdisk_wpFilterMenu(XFldDisk *somSelf,
                                            FILTERFLAGS* pFlags,
                                            HWND hwndCnr,
                                            BOOL fMultiSelect,
                                            ULONG ulMenuType,
                                            ULONG ulView,
                                            ULONG ulReserved)
{
    BOOL    brc,
            fReplaceDNR,
            fCDROM = FALSE;

    static const struct
    {
        ULONG   flXWP,
                flWPS1;
    } aSuppressFlags[] =
        {
            XWPCTXT_CHKDSK, CTXT_CHECKDISK,
            XWPCTXT_FORMAT, CTXT_FORMATDISK,
            XWPCTXT_COPYDSK, CTXT_COPYDISK,
            XWPCTXT_LOCKDISK, CTXT_LOCKDISK,
            XWPCTXT_EJECTDISK, CTXT_EJECTDISK,
            XWPCTXT_UNLOCKDISK, CTXT_UNLOCKDISK,
        };

    ULONG   ul,
            flWPS = cmnQuerySetting(mnuQueryMenuWPSSetting(somSelf)),
            flXWP = cmnQuerySetting(mnuQueryMenuXWPSetting(somSelf));

    ULONG   ulLogicalDrive = _wpQueryLogicalDrive(somSelf),
            i = ulLogicalDrive - 1;

    XFldDiskData *somThis = XFldDiskGetData(somSelf);
    XFldDiskMethodDebug("XFldDisk","xfdisk_wpFilterMenu");

    PMPF_MENUS(("[%s] entering", _wpQueryTitle(somSelf) ));

#ifndef __NEVERREPLACEDRIVENOTREADY__
    if (fReplaceDNR = cmnQuerySetting(sfReplaceDriveNotReady))
    {
        PMPF_DISK(("getting drive data for drive %d", ulLogicalDrive));
        if (G_paDriveData[i].pDisk != somSelf)
        {
            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                   "Drive data mismatch: somSelf == 0x%lX, drive data pDisk == 0x%lX",
                   somSelf,
                   G_paDriveData[i].pDisk);
        }
        else
        {
            PMPF_DISK(("bFileSystem: 0x%lX (%d)",
                   G_paDriveData[i].bFileSystem,
                   G_paDriveData[i].bFileSystem));
            PMPF_DISK(("fNotLocal: %d",
                   G_paDriveData[i].fNotLocal));
            PMPF_DISK(("fFixedDisk: %d",
                   G_paDriveData[i].fFixedDisk));
            PMPF_DISK(("fZIP: %d",
                   G_paDriveData[i].fZIP));
            PMPF_DISK(("bDiskType: %d",
                   G_paDriveData[i].bDiskType));
            PMPF_DISK(("ulSerial: 0x%lX",
                   G_paDriveData[i].ulSerial));
            PMPF_DISK(("szVolLabel: %s",
                   G_paDriveData[i].szVolLabel));

            if (G_paDriveData[i].bDiskType == DRVTYPE_CDROM)
                fCDROM = TRUE;
        }
    }
#endif

    brc = XFldDisk_parent_WPDisk_wpFilterMenu(somSelf,
                                              pFlags,
                                              hwndCnr,
                                              fMultiSelect,
                                              ulMenuType,
                                              ulView,
                                              ulReserved);

#ifndef __NEVERREPLACEDRIVENOTREADY__
    // for CD-ROM drives, the parent method may return
    // total garbage, so fix some... note, fCDROM
    // can only be TRUE if sfReplaceDriveNotReady is
    // enabled (see above)
    if (fCDROM)
    {
        // if no media is present, the parent method removes
        // "eject disk", even though that item is useful
        // to insert some now. Instead, we should focus on
        // whether the door is open.

        pFlags->Flags[1] |= CTXT_EJECTDISK;

        // yeah, actually we should, but there's no way to
        // find out with OS/2. Wonderful job, IBM. See
        // the remarks with doshQueryCDStatus in dosh.c.

        // Anyway, if we have no media, we should remove
        // "lock".
        if (!_pMenuRootFolder)
            pFlags->Flags[1] &= ~ XWPCTXT_LOCKDISK;
    }
#endif

    // finally, go enforce the menu settings from "Workplace Shell"

    pFlags->Flags[0] &= ~(   flWPS
                           | CTXT_NEW);

    // the disk-specific items from the array above are
    // all in array index 1
    for (ul = 0;
         ul < ARRAYITEMCOUNT(aSuppressFlags);
         ++ul)
    {
        // if flag is currently set in settings, remove menu item
        if (flXWP & aSuppressFlags[ul].flXWP)
            pFlags->Flags[1] &= ~aSuppressFlags[ul].flWPS1;
    }

    PMPF_MENUS(("[%s] leaving, returning %d", _wpQueryTitle(somSelf), brc));

    return brc;
}

/*
 *@@ wpModifyPopupMenu:
 *      this legacy popup menu method is only overridden because
 *      the brain-dead peer classes provoke the white "drive not
 *      ready" box TWICE per popup menu.
 *
 *      The order of processing within wpDisplayMenu is
 *
 *      1)  wpFilterMenu (which is overridden by both WPDisk
 *          and ourselves)
 *
 *      2)  wpFilterPopupMenu (which does not seem to be
 *          overridden)
 *
 *      3)  wpModifyPopupMenu (which we override here to fix
 *          the white error boxes)
 *
 *      4)  wpModifyMenu (which is overridden by both WPDisk
 *          and ourselves).
 *
 *      In our wpDisplayMenu override, if "replace drive not
 *      ready" is enabled, we store a temp root folder pointer
 *      in the instance data. If that pointer is -1, the
 *      setting is disabled. If it's NULL, the disk is not
 *      ready and we DO NOT call the parent method. Otherwise
 *      we have a valid root folder.
 *
 *@@added V0.9.21 (2002-08-31) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xfdisk_wpModifyPopupMenu(XFldDisk *somSelf,
                                                 HWND hwndMenu,
                                                 HWND hwndCnr,
                                                 ULONG iPosition)
{
    BOOL    brc = TRUE;

    XFldDiskData *somThis = XFldDiskGetData(somSelf);
    XFldDiskMethodDebug("XFldDisk","xfdisk_wpModifyPopupMenu");

    PMPF_MENUS(("[%s] entering", _wpQueryTitle(somSelf)));

    if (!_pMenuRootFolder)
        // wpDisplayMenu has checked the root folder,
        // and the disk is not ready:
        // DO NOT CALL THE PARENT METHOD
        brc = FALSE;

    if (brc)
        brc = XFldDisk_parent_WPDisk_wpModifyPopupMenu(somSelf,
                                                       hwndMenu,
                                                       hwndCnr,
                                                       iPosition);

    PMPF_MENUS(("[%s] leaving, returning %d", _wpQueryTitle(somSelf), brc));

    return brc;
}

/*
 *@@ wpModifyMenu:
 *      this WPObject instance method was new with Warp 4 and
 *      allows the object to manipulate its menu in a more
 *      fine-grained way than wpModifyPopupMenu.
 *
 *      With V0.9.21, while adding support for the split view
 *      to the menu methods, I finally got tired of all the
 *      send-msg hacks to get Warp 4 menu items to work and
 *      decided to finally break Warp 3 support for XWorkplace.
 *      It's been fun while it lasted, but enough is enough.
 *      We are now overriding this method directly through
 *      the IDL files.
 *
 *      ulMenuType will be one of the following:
 *
 *      --  MENU_OBJECTPOPUP: Pop-up menu for the object icon.
 *          This can come in for any object.
 *
 *      --  MENU_OPENVIEWPOPUP: Pop-up menu for an open view.
 *          I think this can reasonably only come in for folders,
 *          although it seems to be handled by the WPObject method.
 *
 *      --  MENU_FOLDERPULLDOWN: Pull-down menu for a folder.
 *          This comes in for folders only.
 *
 *      --  MENU_EDITPULLDOWN: Pull-down menu for the Edit menu option.
 *          This comes in TWICE, first on the selected object in
 *          the view's container (if any) with ulView == CLOSED_ICON,
 *          and then a second time on the view's folder with ulView
 *          set to the folder's current view.
 *
 *      --  MENU_VIEWPULLDOWN: Pull-down menu for the View menu option.
 *          This comes in for folders only.
 *
 *      --  MENU_SELECTEDPULLDOWN: Pull-down menu for the Selected menu option.
 *          This comes in for non-folder objects also to let the object
 *          decide what options it wants to present in the folder's
 *          "Selected" pulldown.
 *
 *      --  MENU_HELPPULLDOWN: Pull-down menu for the Help menu option.
 *          This comes in for folders only.
 *
 *      --  MENU_TITLEBARPULLDOWN: this is listed in the toolkit headers,
 *          but not described in WPSREF. I think this is what comes in
 *          if the system menu is being built for an open object view.
 *
 *@@added V0.9.21 (2002-08-31) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xfdisk_wpModifyMenu(XFldDisk *somSelf,
                                            HWND hwndMenu,
                                            HWND hwndCnr,
                                            ULONG iPosition,
                                            ULONG ulMenuType,
                                            ULONG ulView,
                                            ULONG ulReserved)
{
    BOOL        brc = FALSE;
    WPFolder*   pRootFolder;

    XFldDiskData *somThis = XFldDiskGetData(somSelf);
    XFldDiskMethodDebug("XFldDisk","xfdisk_wpModifyMenu");

    PMPF_MENUS(("[%s] entering", _wpQueryTitle(somSelf)));

    if ((ULONG)_pMenuRootFolder != -1)
    {
        // wpDisplayMenu has tested the root folder:
        // use that pointer, it can be NULL if the
        // disk is not ready
        pRootFolder = _pMenuRootFolder;
    }
    else
        pRootFolder = _wpQueryRootFolder(somSelf);

    if (brc = XFldDisk_parent_WPDisk_wpModifyMenu(somSelf,
                                                  hwndMenu,
                                                  hwndCnr,
                                                  iPosition,
                                                  ulMenuType,
                                                  ulView,
                                                  ulReserved))
    {
        if (pRootFolder)
            brc = mnuModifyFolderMenu(pRootFolder,
                                      hwndMenu,
                                      hwndCnr,
                                      ulMenuType,
                                      ulView);
    }

    PMPF_MENUS(("[%s] leaving, returning %d", _wpQueryTitle(somSelf), brc));

    return brc;
}

/*
 *@@ wpDisplayMenu:
 *      this WPObject instance method creates and displays
 *      an object's popup menu, which is returned.
 *
 *      From my testing (after overriding menu methods),
 *      I found out that wpDisplayMenu calls the following
 *      methods in this order:
 *
 *      --  wpFilterMenu (Warp-4-specific);
 *
 *      --  wpFilterPopupMenu;
 *
 *      --  wpModifyPopupMenu;
 *
 *      --  wpModifyMenu (Warp-4-specific).
 *
 *      If "replace drive not ready" is enabled, we do a
 *      quick check here if the drive has media because
 *      the peer classes have problems doing that in
 *      their wpModifyPopupMenu overrides. See
 *      XFldDisk::wpModifyPopupMenu.
 *
 *@@added V0.9.21 (2002-08-31) [umoeller]
 */

SOM_Scope HWND  SOMLINK xfdisk_wpDisplayMenu(XFldDisk *somSelf,
                                             HWND hwndOwner,
                                             HWND hwndClient,
                                             POINTL* ptlPopupPt,
                                             ULONG ulMenuType,
                                             ULONG ulReserved)
{
    HWND hwndMenu;
    XFldDiskData *somThis = XFldDiskGetData(somSelf);
    XFldDiskMethodDebug("XFldDisk","xfdisk_wpDisplayMenu");

    PMPF_MENUS(("[%s] entering", _wpQueryTitle(somSelf)));

    _pMenuRootFolder = (WPObject*)-1;

#ifndef __NEVERREPLACEDRIVENOTREADY__
    switch (ulMenuType)
    {
        case MENU_OBJECTPOPUP:
        case MENU_SELECTEDPULLDOWN:
            if (cmnQuerySetting(sfReplaceDriveNotReady))
            {
                PMPF_DISK(("safe-checking root folder"));
                // yes: use the safe way of opening the drive
                _pMenuRootFolder = _xwpSafeQueryRootFolder(somSelf, FALSE, NULL);
            }
        break;
    }
#endif

    hwndMenu = XFldDisk_parent_WPDisk_wpDisplayMenu(somSelf,
                                                    hwndOwner,
                                                    hwndClient,
                                                    ptlPopupPt,
                                                    ulMenuType,
                                                    ulReserved);

    _pMenuRootFolder = (WPObject*)-1;

    PMPF_MENUS(("[%s] leaving, returning 0x%lX", _wpQueryTitle(somSelf), hwndMenu));

    return hwndMenu;
}

/*
 *@@ wpMenuItemSelected:
 *      this WPObject method processes menu selections.
 *      This must be overridden to support new menu
 *      items which have been added in wpModifyPopupMenu.
 *
 *      See XFldObject::wpMenuItemSelected for additional
 *      remarks.
 *
 *      We pass the input to mnuMenuItemSelected in fdrmenus.c
 *      because disk menu items are mostly shared with XFolder.
 */

SOM_Scope BOOL  SOMLINK xfdisk_wpMenuItemSelected(XFldDisk *somSelf,
                                                  HWND hwndFrame,
                                                  ULONG ulMenuId)
{
    XFolder         *pFolder = _wpQueryRootFolder(somSelf);
    // XFldDiskData *somThis = XFldDiskGetData(somSelf);
    XFldDiskMethodDebug("XFldDisk","xfdisk_wpMenuItemSelected");

    if (fcmdMenuItemSelected(pFolder, hwndFrame, ulMenuId))
        return TRUE;
    else
        return XFldDisk_parent_WPDisk_wpMenuItemSelected(somSelf,
                                                         hwndFrame,
                                                         ulMenuId);
}

/*
 *@@ wpMenuItemHelpSelected:
 *      display help for a context menu item;
 *      we pass the input to mnuMenuItemHelpSelected in fdrmenus.c.
 */

SOM_Scope BOOL  SOMLINK xfdisk_wpMenuItemHelpSelected(XFldDisk *somSelf,
                                                      ULONG MenuId)
{
    XFolder         *pFolder = _wpQueryRootFolder(somSelf);
    // XFldDiskData *somThis = XFldDiskGetData(somSelf);
    XFldDiskMethodDebug("XFldDisk","xfdisk_wpMenuItemHelpSelected");

    if (fcmdMenuItemHelpSelected(pFolder, MenuId))
        return TRUE;
    else
        return XFldDisk_parent_WPDisk_wpMenuItemHelpSelected(somSelf,
                                                             MenuId);
}

/*
 *@@ wpViewObject:
 *      this WPObject method either opens a new view of the
 *      object (by calling wpOpen) or resurfaces an already
 *      open view, if one exists already and "concurrent views"
 *      are enabled. This gets called every single time when
 *      an object is to be opened... e.g. on double-clicks.
 *
 *      For WPDisks, the WPS seems to be doing no drive checking
 *      in here, which leads to the annoying "Drive not ready"
 *      popups. So we try to implement this here.
 *
 *@@changed V0.9.0 [umoeller]: added global setting for disabling this feature
 *@@changed V0.9.16 (2001-10-23) [umoeller]: now intercepting OPEN_SETTINGS too
 */

SOM_Scope HWND  SOMLINK xfdisk_wpViewObject(XFldDisk *somSelf,
                                            HWND hwndCnr,
                                            ULONG ulView,
                                            ULONG param)
{
    HWND            hwndFrame = NULLHANDLE; // default: error occured

    /* XFldDiskData *somThis = XFldDiskGetData(somSelf); */
    XFldDiskMethodDebug("XFldDisk","xfdisk_wpViewObject");

    PMPF_DISK(("entering"));

    // "Drive not ready" replacement enabled?
#ifndef __NEVERREPLACEDRIVENOTREADY__
    if (    (cmnQuerySetting(sfReplaceDriveNotReady))
         // && (ulView != OPEN_SETTINGS)
                // V0.9.16 (2001-10-23) [umoeller]
                // do this for settings too,
                // however, on eCS this _never_ gets called...
                // we leave this in here in case Warp 3 behaves
                // differently, but we do the same check again in
                // wpOpen below now
       )
    {
        // yes: use the safe way of opening the
        // drive (this prompts the user upon errors)
        XFolder*        pRootFolder = NULL;
        if (!(pRootFolder = dskCheckDriveReady(somSelf)))
            // error: do _not_ call default
            somSelf = NULL;
    }
#endif

    if (somSelf)
        // drive checking disabled, or disk is ready:
        hwndFrame = XFldDisk_parent_WPDisk_wpViewObject(somSelf,
                                                        hwndCnr,
                                                        ulView,
                                                        param);

    PMPF_DISK(("leaving"));

    return (hwndFrame);
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
 *
 *      We call the parent first and then subclass the
 *      resulting frame window, similar to what we're doing
 *      with folder views (see XFolder::wpOpen).
 *
 *@@changed V0.9.2 (2000-03-06) [umoeller]: drives were checked even if replacement dlg was disabled; fixed
 *@@changed V0.9.3 (2000-04-08) [umoeller]: adjusted for new folder subclassing
 *@@changed V0.9.16 (2001-10-23) [umoeller]: now intercepting OPEN_SETTINGS too
 */

SOM_Scope HWND  SOMLINK xfdisk_wpOpen(XFldDisk *somSelf,
                                      HWND hwndCnr,
                                      ULONG ulView,
                                      ULONG param)
{
    HWND            hwndNewFrame = NULLHANDLE; // default: error occured
    XFolder         *pRootFolder = NULL;

    PMPF_DISK(("entering"));

    switch (ulView)
    {
        case OPEN_CONTENTS:
        case OPEN_TREE:
        case OPEN_DETAILS:
        case OPEN_SETTINGS:     // V0.9.16 (2001-10-23) [umoeller]
        {
            // XFldDiskData *somThis = XFldDiskGetData(somSelf);
            XFldDiskMethodDebug("XFldDisk","xfdisk_wpOpen");

#ifndef __NEVERREPLACEDRIVENOTREADY__
            // "Drive not ready" replacement enabled?
            if (cmnQuerySetting(sfReplaceDriveNotReady))
            {
                // query root folder (WPRootFolder class, which is a descendant
                // of WPFolder/XFolder); each WPDisk is paired with one of those,
                // and the actual display is done by this class, so we will pass
                // pRootFolder instead of somSelf to most following method calls.
                // We use xwpSafeQueryRootFolder instead of wpQueryRootFolder to
                // avoid "Drive not ready" popups.
                if (pRootFolder = dskCheckDriveReady(somSelf))
                                // V0.9.16 (2001-10-23) [umoeller]
                                // now using dskCheckDriveReady instead of
                                // xwpSafeQueryRootFolder because wpViewObject
                                // never gets called for OPEN_SETTINGS
                    // drive ready: call parent to get frame handle
                    hwndNewFrame = XFldDisk_parent_WPDisk_wpOpen(somSelf,
                                                                 hwndCnr,
                                                                 ulView,
                                                                 param);
                // else: hwndNewFrame is still NULLHANDLE
            }
            else
#endif
            {
                // "drive not ready" replacement disabled:
                if (hwndNewFrame = XFldDisk_parent_WPDisk_wpOpen(somSelf,
                                                                 hwndCnr,
                                                                 ulView,
                                                                 param))
                    // no error:
                    pRootFolder = _wpQueryRootFolder(somSelf);
            }

            if (    (pRootFolder)
                 && (hwndNewFrame)
                 && (ulView != OPEN_SETTINGS)
               )
            {
                PSUBCLFOLDERVIEW psfv = NULL;

                hwndCnr = WinWindowFromID(hwndNewFrame, FID_CLIENT);

                // subclass frame window; this is the same
                // proc which is used for normal folder frames,
                // we just use pRootFolder instead.
                // However, we pass somSelf as the "real" object
                // which will then be stored in *psli.
                psfv = fdrSubclassFolderView(hwndNewFrame,
                                             hwndCnr,
                                             pRootFolder, // folder
                                             somSelf);    // real object; for disks, this
                                                          // is the WPDisk object...
                // add status bar, if allowed: as opposed to
                // XFolder's, for XFldDisk's we only check the
                // global setting, because there's no instance
                // setting for this with XFldDisk's
                if (
#ifndef __NOCFGSTATUSBARS__
                        (cmnQuerySetting(sfStatusBars))
                                                // feature enabled?
                     &&
#endif
                        (cmnQuerySetting(sfDefaultStatusBarVisibility))
                                                // bars visible per default?
                   )
                    // assert that subclassed list item is valid
                    if (psfv)
                    {
                        ULONG flViews = cmnQuerySetting(sflSBForViews);
                        // add status bar only if allowed for the current view type
                        if (    (   (ulView == OPEN_CONTENTS)
                                 && (flViews & SBV_ICON)
                                )
                             || (   (ulView == OPEN_TREE)
                                 && (flViews & SBV_TREE)
                                )
                             || (   (ulView == OPEN_DETAILS)
                                 && (flViews & SBV_DETAILS)
                                )
                            )
                            // this reformats the window with the status bar
                            stbCreate(psfv);
                    }

                // extended sort functions
#ifndef __ALWAYSEXTSORT__
                if (cmnQuerySetting(sfExtendedSorting))
#endif
                    if (hwndCnr)
                        fdrSetFldrCnrSort(pRootFolder,
                                          hwndCnr,
                                          FALSE);
            } // if (pRootFolder)
        }
        break;

        default:
            // added split view
            // V0.9.21 (2002-08-28) [umoeller]
            if (ulView == *G_pulVarMenuOfs + ID_XFMI_OFS_SPLITVIEW)
            {
                if (pRootFolder = dskCheckDriveReady(somSelf))
                    hwndNewFrame = fdrCreateSplitView(somSelf,
                                                      pRootFolder,
                                                      ulView);
            }
            else
                hwndNewFrame = XFldDisk_parent_WPDisk_wpOpen(somSelf,
                                                             hwndCnr,
                                                             ulView,
                                                             param);
    } // switch (ulView)

    PMPF_DISK(("leaving"));

    return (hwndNewFrame);
}

/*
 *@@ wpAddSettingsPages:
 *      this WPObject instance method gets called by the WPS
 *      when the Settings view is opened to have all the
 *      settings page inserted into hwndNotebook.
 *
 *      We override this so we can attempt to avoid the
 *      disk error msg boxes here.
 *
 *@@added V0.9.16 (2001-10-23) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xfdisk_wpAddSettingsPages(XFldDisk *somSelf,
                                                  HWND hwndNotebook)
{
    BOOL brc = FALSE;
    /* XFldDiskData *somThis = XFldDiskGetData(somSelf); */
    XFldDiskMethodDebug("XFldDisk","xfdisk_wpAddSettingsPages");

#ifndef __NEVERREPLACEDRIVENOTREADY__
    if (cmnQuerySetting(sfReplaceDriveNotReady))
    {
        WPFolder *pRoot;
        if (!(pRoot = _xwpSafeQueryRootFolder(somSelf, FALSE, NULL)))
            return FALSE;
    }
#endif

    return (XFldDisk_parent_WPDisk_wpAddSettingsPages(somSelf,
                                                      hwndNotebook));
}

/*
 *@@ wpAddDiskDetailsPage:
 *      this adds the "Details" page to a disk object's
 *      settings notebook.
 *
 *      We override this method to insert our enhanced
 *      page instead.
 *
 *@@added V0.9.0 [umoeller]
 */

SOM_Scope ULONG  SOMLINK xfdisk_wpAddDiskDetailsPage(XFldDisk *somSelf,
                                                     HWND hwndNotebook)
{
    /* XFldDiskData *somThis = XFldDiskGetData(somSelf); */
    XFldDiskMethodDebug("XFldDisk","xfdisk_wpAddDiskDetailsPage");

#ifndef __ALWAYSREPLACEFILEPAGE__
    if (cmnQuerySetting(sfReplaceFilePage))
#endif
    {
        INSERTNOTEBOOKPAGE inbp;
        memset(&inbp, 0, sizeof(INSERTNOTEBOOKPAGE));
        inbp.somSelf = somSelf;
        inbp.hwndNotebook = hwndNotebook;
        inbp.hmod = cmnQueryNLSModuleHandle(FALSE);
        inbp.ulDlgID = ID_XSD_DISK_DETAILS;
        inbp.ulPageID = SP_DISK_DETAILS;
        inbp.usPageStyleFlags = BKA_MAJOR;
        inbp.pcszName = cmnGetString(ID_XSSI_DETAILSPAGE);  // pszDetailsPage
        inbp.ulDefaultHelpPanel  = ID_XSH_SETTINGS_DISKDETAILS;
        inbp.pfncbInitPage    = (PFNCBACTION)dskDetailsInitPage;
        inbp.pfncbItemChanged = dskDetailsItemChanged;
        return (ntbInsertPage(&inbp));
    }

#ifndef __ALWAYSREPLACEFILEPAGE__
    return (XFldDisk_parent_WPDisk_wpAddDiskDetailsPage(somSelf,
                                                        hwndNotebook));
#endif
}

/* ******************************************************************
 *
 *   here come the XFldDisk class methods
 *
 ********************************************************************/

/*
 *@@ wpclsInitData:
 *      this WPObject class method gets called when a class
 *      is loaded by the WPS (probably from within a
 *      somFindClass call) and allows the class to initialize
 *      itself.
 *
 *@@changed V0.9.0 [umoeller]: added class object to KERNELGLOBALS
 */

SOM_Scope void  SOMLINK xfdiskM_wpclsInitData(M_XFldDisk *somSelf)
{
    // M_XFldDiskData *somThis = M_XFldDiskGetData(somSelf);
    M_XFldDiskMethodDebug("M_XFldDisk","xfdiskM_wpclsInitData");

    M_XFldDisk_parent_M_WPDisk_wpclsInitData(somSelf);

    krnClassInitialized(G_pcszXFldDisk);
}

/*
 *@@ wpclsQueryTitle:
 *      this WPObject class method tells the WPS the clear
 *      name of a class, which is shown in the third column
 *      of a Details view and also used as the default title
 *      for new objects of a class.
 *
 *      We override the standard folder class name only if
 *      the user has enabled "fix class titles" in XWPSetup.
 *
 *@@added V0.9.12 (2001-05-22) [umoeller]
 */

SOM_Scope PSZ  SOMLINK xfdiskM_wpclsQueryTitle(M_XFldDisk *somSelf)
{
    /* M_XFldDiskData *somThis = M_XFldDiskGetData(somSelf); */
    M_XFldDiskMethodDebug("M_XFldDisk","xfdiskM_wpclsQueryTitle");

#ifndef __ALWAYSFIXCLASSTITLES__
    if (!cmnQuerySetting(sfFixClassTitles))
        return (M_XFldDisk_parent_M_WPDisk_wpclsQueryTitle(somSelf));
#endif

    return cmnGetString(ID_XSSI_CLASSTITLE_DISK);
}


