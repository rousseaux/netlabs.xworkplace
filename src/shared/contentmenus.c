
/*
 *@@sourcefile contentmenus.c:
 *      this file contains shared functions for displaying
 *      folder contents in menus.
 *
 *      These "folder content" menus are subclassed PM menu
 *      controls which behave similar to the WarpCenter, i.e.
 *      they will auto-populate and display folder contents
 *      and open folders and objects in their default views
 *      when selected.
 *
 *      Folder content menus are neither exactly trivial to
 *      implement nor trivial to use because they require
 *      menu owner draw (for displaying icons in a menu)
 *      and subclassing popup menu controls (for intercepting
 *      mouse button 2).
 *
 *      You can only use folder content menus if you have
 *      access to messages send or posted to the menus's
 *      owner. For example, for folder popup menus, this is
 *      the case because XFolder subclasses the folder window
 *      frame, which owns the menu.
 *
 *      To use folder content menus, perform the following
 *      steps:
 *
 *      1)  Before displaying a popup menu which contains
 *          folder content submenus, call cmnuInitItemCache.
 *
 *      2)  For each folder content submenu to insert, call
 *          cmnuPrepareContentSubmenu.
 *
 *      3)  Intercept WM_INITMENU in the menu owner's window
 *          procedure. This msg gets sent to a menu's owner
 *          window when a (sub)menu is about to display.
 *          If you get WM_INITMENU for one of the folder
 *          content submenus, call cmnuFillContentSubmenu,
 *          which will populate the corresponding folder
 *          and insert its contents into the menu.
 *
 *      4)  Call cmnuMeasureItem for each WM_MEASUREITEM
 *          which comes in.
 *
 *      5)  Call cmnuDrawItem for each WM_DRAWITEM which
 *          comes in.
 *
 *      6)  The window's owner will receive a WM_COMMAND
 *          if one of the objects is selected by the user.
 *          To get a VARMENULISTITEM structure which describes
 *          the object in the menu, call cmnuGetVarItem with
 *          the command's menu ID. You can then open the
 *          object, for example.
 *
 *      Function prefix for this file:
 *      --  cmnu*
 *
 *      This file is new with V0.9.7 (2000-11-29) [umoeller].
 *      The code in here used to be in src\filesys\fdrmenus.c
 *      and has been moved here so that it can be used in
 *      other code parts as well.
 *
 *@@header "shared\contentmenus.h"
 */

/*
 *      Copyright (C) 1997-2000 Ulrich M”ller.
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
#define INCL_DOSEXCEPTIONS
#define INCL_DOSERRORS

#define INCL_WINWINDOWMGR
#define INCL_WINMESSAGEMGR
#define INCL_WINFRAMEMGR        // SC_CLOSE etc.
#define INCL_WININPUT
#define INCL_WINDIALOGS
#define INCL_WINBUTTONS
#define INCL_WINPOINTERS
#define INCL_WINMENUS
#define INCL_WINSTDCNR
#define INCL_WINMLE
#define INCL_WINCOUNTRY
#define INCL_WINCLIPBOARD
#define INCL_WINSYS
#define INCL_WINPROGRAMLIST     // needed for PROGDETAILS, wppgm.h

#define INCL_GPILOGCOLORTABLE
#define INCL_GPIPRIMITIVES
#define INCL_DEV
#include <os2.h>

// C library headers
#include <stdio.h>              // needed for except.h
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\stringh.h"            // string helper routines
#include "helpers\winh.h"               // PM helper routines

// SOM headers which don't crash with prec. header files
#include "xfldr.ih"
#include "xfdisk.ih"                    // XFldDisk

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\contentmenus.h"        // shared menu logic
#include "shared\kernel.h"              // XWorkplace Kernel

// #include "filesys\folder.h"             // XFolder implementation

// other SOM headers
#pragma hdrstop                         // VAC++ keeps crashing otherwise

#include <wppgm.h>                      // WPProgram
#include <wpshadow.h>                   // WPShadow
#include <wpdesk.h>                     // WPDesktop
#include <wpdataf.h>                    // WPDataFile
#include <wprootf.h>                    // WPRootFolder

#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

/* ******************************************************************
 *
 *   Definitions
 *
 ********************************************************************/

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

// counts for providing unique menu id's (exported)
ULONG               G_ulVarItemCount = 0;    // number of inserted menu items
SHORT               G_sNextMenuId = 0;      // next menu item ID to use

// llContentMenuItems contains ONLY folder content menus
static LINKLIST            G_llContentMenuItems; // changed V0.9.0

// linked lists / counts for variable context menu items
// llVarMenuItems contains ALL variable items ever inserted
// (i.e. config folder items AND folder content items)
static LINKLIST            G_llVarMenuItems;     // changed V0.9.0

// icon for drawing the little triangle in
// folder content menus (subfolders)
static HPOINTER            G_hMenuArrowIcon = NULLHANDLE;

// original wnd proc for folder content menus,
// which we must subclass
static PFNWP               G_pfnwpFolderContentMenuOriginal = NULL;

// flags for fdr_fnwpSubclassedFolderFrame;
// these are set by fdr_fnwpSubclFolderContentMenu.
// We can afford using global variables here
// because opening menus is a modal operation.
/* BOOL                G_fFldrContentMenuMoved = FALSE,
                    G_fFldrContentMenuButtonDown = FALSE; */

#define CX_ARROW 21

// global data for owner draw
static ULONG   G_ulMiniIconSize = 0;
static RECTL   G_rtlMenuItem;
static LONG    G_lHiliteBackground,
               G_lBackground,
               G_lHiliteText,
               G_lText;

/* ******************************************************************
 *
 *   Functions
 *
 ********************************************************************/

/*
 *@@ fnwpSubclFolderContentMenu:
 *      this is the subclassed wnd proc for folder content menus.
 *      We need to intercept mouse button 2 msgs to open a folder
 *      (WarpCenter behavior).
 *
 *@@changed V0.9.0 [umoeller]: moved this func here from xfldr.c
 */

MRESULT EXPENTRY fnwpSubclFolderContentMenu(HWND hwndMenu, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT     mrc = 0;
    PFNWP       pfnwpOrig = 0;

    if (G_pfnwpFolderContentMenuOriginal)
        pfnwpOrig = G_pfnwpFolderContentMenuOriginal;
    else
    {
        cmnLog(__FILE__, __LINE__, __FUNCTION__,
                       "G_pfnwpFolderContentMenuOriginal is NULL");
        pfnwpOrig = WinDefWindowProc;
    }

    TRY_LOUD(excpt1)
    {
        USHORT  sSelected;
        POINTL  ptlMouse;
        RECTL   rtlItem;

        switch(msg)
        {
            /* case WM_ADJUSTWINDOWPOS:
            {
                PSWP pswp = (PSWP)mp1;
                BOOL fAdjusted = FALSE;
                #ifdef DEBUG_MENUS
                    _Pmpf(("WM_ADJUSTWINDOWPOS"));
                #endif

                if ((pswp->fl & (SWP_MOVE)) == (SWP_MOVE))
                {
                    #ifdef DEBUG_MENUS
                        _Pmpf(("  SWP_MOVE set"));
                    #endif

                    // if (!G_fFldrContentMenuMoved)
                    {
                        #ifdef DEBUG_MENUS
                            _Pmpf(("    Checking bounds"));
                        #endif
                        if ((pswp->x + pswp->cx)
                               > WinQuerySysValue(HWND_DESKTOP, SV_CXSCREEN))
                        {
                            pswp->x = 0;
                            #ifdef DEBUG_MENUS
                                _Pmpf(("    Changed x pos"));
                            #endif
                            // avoid several changes for this menu;
                            // this flag is reset by WM_INITMENU in
                            // fdr_fnwpSubclassedFolderFrame
                            // G_fFldrContentMenuMoved = TRUE;
                            fAdjusted = TRUE;
                        }
                        if ((pswp->y + pswp->cy) >
                                WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN))
                        {
                            pswp->y = 0;
                            #ifdef DEBUG_MENUS
                                _Pmpf(("    Changed y pos"));
                            #endif
                            // avoid several changes for this menu;
                            // this flag is reset by WM_INITMENU in
                            // fdr_fnwpSubclassedFolderFrame
                            G_fFldrContentMenuMoved = TRUE;
                            fAdjusted = TRUE;
                        }
                    }
                }
                if (fAdjusted)
                    pswp->fl |= (SWP_NOADJUST);
                mrc = (MRESULT)(pfnwpOrig)(hwndMenu, msg, mp1, mp2);
                G_fFldrContentMenuButtonDown = FALSE;
            break; } */

            #ifdef DEBUG_MENUS
                case MM_SELECTITEM:
                {
                    _Pmpf(( "MM_SELECTITEM: mp1 = %lX/%lX, mp2 = %lX",
                        SHORT1FROMMP(mp1),
                        SHORT2FROMMP(mp1),
                        mp2 ));
                    mrc = (MRESULT)(*pfnwpFolderContentMenuOriginal)(hwndMenu, msg, mp1, mp2);
                break; }
            #endif

            case WM_BUTTON2DOWN:
                #ifdef DEBUG_MENUS
                    _Pmpf(("WM_BUTTON2DOWN"));
                #endif

                ptlMouse.x = SHORT1FROMMP(mp1);
                ptlMouse.y = SHORT2FROMMP(mp1);
                WinSendMsg(hwndMenu, MM_SELECTITEM,
                           MPFROM2SHORT(MIT_NONE, FALSE),
                           MPFROM2SHORT(0, FALSE));
                sSelected = winhQueryItemUnderMouse(hwndMenu, &ptlMouse, &rtlItem);
                WinSendMsg(hwndMenu, MM_SETITEMATTR,
                           MPFROM2SHORT(sSelected,
                                        FALSE),
                           MPFROM2SHORT(MIA_HILITED,
                                        MIA_HILITED)
                    );
            break;

            case WM_BUTTON1DOWN:
                // let this be handled by the default proc
                #ifdef DEBUG_MENUS
                    _Pmpf(("WM_BUTTON1DOWN"));
                #endif
                // G_fFldrContentMenuButtonDown = TRUE;
                mrc = (MRESULT)(pfnwpOrig)(hwndMenu, msg, mp1, mp2);
            break;

            case WM_BUTTON1DBLCLK:
            case WM_BUTTON2UP:
            {
                // upon receiving these, we will open the object directly; we need to
                // cheat a little bit because sending MM_SELECTITEM would open the submenu
                #ifdef DEBUG_MENUS
                    _Pmpf(("WM_BUTTON2UP"));
                #endif
                // G_fFldrContentMenuButtonDown = TRUE;
                ptlMouse.x = SHORT1FROMMP(mp1);
                ptlMouse.y = SHORT2FROMMP(mp1);
                WinSendMsg(hwndMenu, MM_SELECTITEM,
                           MPFROM2SHORT(MIT_NONE, FALSE),
                           MPFROM2SHORT(0, FALSE));
                sSelected = winhQueryItemUnderMouse(hwndMenu, &ptlMouse, &rtlItem);

                cmnPlaySystemSound(MMSOUND_XFLD_CTXTSELECT);

                WinPostMsg(WinQueryWindow(hwndMenu, QW_OWNER),
                           WM_COMMAND,
                           (MPARAM)sSelected,
                           MPFROM2SHORT(CMDSRC_MENU, FALSE));
            break; }

            default:
                mrc = (MRESULT)(pfnwpOrig)(hwndMenu, msg, mp1, mp2);
            break;
        } // end switch
    }
    CATCH(excpt1)
    {
        // exception occured:
        return (MRESULT)0; // keep compiler happy
    } END_CATCH();

    return (mrc);
}

/* ******************************************************************
 *
 *   Functions
 *
 ********************************************************************/

/*
 *@@ cmnuInitItemCache:
 *      this initializes all global variables which hold
 *      object information while a folder context menu
 *      is open.
 *
 *      This must be called each time before a menu is
 *      displayed which contains one or several folder
 *      content submenus (inserted with cmnuPrepareContentSubmenu),
 *      or otherwise we'll quickly run out of menu item IDs.
 *
 *      For folder context menus, this gets called from
 *      mnuModifyFolderPopupMenu.
 *
 *@@added V0.9.7 (2000-11-29) [umoeller]
 */

VOID cmnuInitItemCache(PCGLOBALSETTINGS pGlobalSettings) // in: from cmnQueryGlobalSettings()
{
    G_sNextMenuId = (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_VARIABLE);
    G_ulVarItemCount = 0;         // reset the number of variable items to 0

    if (G_llContentMenuItems.ulMagic != LINKLISTMAGIC)
    {
        // first call: initialize lists
        lstInit(&G_llContentMenuItems, TRUE);
        lstInit(&G_llVarMenuItems, TRUE);
    } else
    {
        // subsequent calls: clear lists
        // (this might take a while)
        HPOINTER    hptrOld = winhSetWaitPointer();
        lstClear(&G_llContentMenuItems);
        lstClear(&G_llVarMenuItems);
        WinSetPointer(HWND_DESKTOP, hptrOld);
    }
}

/*
 *@@ cmnuAppendMi2List:
 *      this stores a variable XFolder menu item in the
 *      respective global linked list (llVarMenuItems)
 *      and increases sNextMenuId for the next item.
 *      Returns FALSE if too many items have already been
 *      used and menus should be closed.
 *
 *@@changed V0.9.0 [umoeller]: adjusted for new linklist functions
 *@@changed V0.9.0 [umoeller]: fixed bug with filenames > 100 chars
 *@@changed V0.9.0 [umoeller]: lowered menu item limit
 */

BOOL cmnuAppendMi2List(WPObject *pObject,
                       ULONG ulObjType)     // in: OC_* flag
{
    // CHAR* p;
    /* remember program object's data for later use in wpMenuItemSelected
       itemCount is unique for each inserted object */
    PVARMENULISTITEM pNewItem = (PVARMENULISTITEM)malloc(sizeof(VARMENULISTITEM));
    pNewItem->pObject = pObject;
    strhncpy0(pNewItem->szTitle,
              _wpQueryTitle(pObject),
              sizeof(pNewItem->szTitle)-1);

    strhBeautifyTitle(pNewItem->szTitle);

    pNewItem->ulObjType = ulObjType;
    lstAppendItem(&G_llVarMenuItems, pNewItem);

    if (G_sNextMenuId < 0x7800)       // lowered V0.9.0
    {
        G_sNextMenuId++;
        G_ulVarItemCount++;
        return (TRUE);
    }
    else
    {
        krnPostThread1ObjectMsg(T1M_LIMITREACHED, MPNULL, MPNULL);
        return (FALSE);
    }
}

/*
 *@@ cmnuAppendFldr2ContentList:
 *      this stores a folder / menu item id in a global linked
 *      list so that the subclassed window procedure can find
 *      it later for populating the folder and displaying its
 *      contents in the menu.
 *
 *@@changed V0.9.0 [umoeller]: adjusted for new linklist functions
 */

VOID cmnuAppendFldr2ContentList(WPFolder *pFolder,
                                SHORT sMenuId)
{
    PCONTENTMENULISTITEM pNewItem = (PCONTENTMENULISTITEM)malloc(sizeof(CONTENTMENULISTITEM));
    pNewItem->pFolder = pFolder;
    pNewItem->sMenuId = sMenuId;
    lstAppendItem(&G_llContentMenuItems, pNewItem);
}

/*
 *@@ cmnuInsertOneObjectMenuItem:
 *      this sub-subroutine is called by mnuFillContentSubmenu
 *      whenever a single menu item is to be inserted
 *      (all objects except folders); returns the menu
 *      item id.
 */

ULONG cmnuInsertOneObjectMenuItem(HWND       hAddToMenu,   // hwnd of menu to add to
                                  USHORT     iPosition,
                                  PSZ        pszNewItemString,          // title of new item
                                  USHORT     afStyle,
                                  WPObject   *pObject,                  // pointer to corresponding object
                                  ULONG      ulObjType)
{
    ULONG               rc = G_sNextMenuId;

    winhInsertMenuItem(hAddToMenu,
                       iPosition,
                       G_sNextMenuId,
                       pszNewItemString,
                       afStyle,
                       0);

    if (cmnuAppendMi2List(pObject, ulObjType))
        // give signal for calling function that we found
        // something
        return (rc);
    else
        return (0); // report error
}

/*
 *@@ cmnuPrepareContentSubmenu:
 *      prepares a "folder content" submenu by inserting a
 *      submenu menu item with pszTitle into hwndMenu.
 *
 *      This submenu then only contains the "empty" menu item,
 *      however, this menu can be filled with objects if the
 *      user opens it.
 *
 *      To support this, you need to intercept the WM_INITMENU
 *      message and then call cmnuFillContentSubmenu.
 *
 *      This way, we can fill the folder content submenus only when
 *      this is needed, because we can impossibly populate all
 *      folder content menus when the context menu is initially
 *      opened.
 *
 *      This function returns the submenu item ID.
 */

SHORT cmnuPrepareContentSubmenu(WPFolder *somSelf, // in: folder whose content is to be displayed
                                HWND hwndMenu,    // in: menu to insert submenu into
                                PSZ pszTitle,     // in: submenu item title
                                USHORT iPosition, // in: position to insert at (or MIT_END)
                                BOOL fOwnerDraw)  // in: owner-draw style flag for submenu (ie. display icons)
{
    HWND    hwndNewMenu;
    SHORT   sId = G_sNextMenuId;
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    if (hwndNewMenu = winhInsertSubmenu(hwndMenu,
                                        iPosition,
                                        sId,
                                        pszTitle,
                                        (fOwnerDraw
                                            ? MIS_OWNERDRAW
                                            : 0),
                                        (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_DUMMY),
                                        (cmnQueryNLSStrings())->pszFldrEmpty,
                                        MIS_TEXT,
                                        MIA_DISABLED))
     {
        cmnuAppendFldr2ContentList(somSelf, sId);
        cmnuAppendMi2List(somSelf, OC_CONTENTFOLDER);
        return(sId);
    }

    return (0);
}

/*
 *@@ MENULISTITEM:
 *
 */

typedef struct _MENULISTITEM
{
    WPObject    *pObject;
    CHAR        szItemString[256];
} MENULISTITEM, *PMENULISTITEM;

/*
 *@@ fncbSortContentMenuItems:
 *      callback sort func for the sort functions in linklist.c.
 *
 *      This sorts the folder content menu items alphabetically.
 */

SHORT EXPENTRY fncbSortContentMenuItems(PVOID pItem1, PVOID pItem2, PVOID hab)
{
    switch (WinCompareStrings((HAB)hab, 0, 0,
                              ((PMENULISTITEM)pItem1)->szItemString,
                              ((PMENULISTITEM)pItem2)->szItemString, 0))
    {
        case WCS_LT:    return (-1);
        case WCS_GT:    return (1);
        default:        return (0);
    }

}

/*
 *@@ cmnuInsertObjectsIntoMenu:
 *      this does the real work for mnuFillContentSubmenu:
 *      collecting the folder's contents, sorting that into
 *      folders and objects and reformatting the submenu in
 *      columns.
 *
 *      You can call this function directly to have folder
 *      contents inserted into a certain menu, if you don't
 *      want to use a submenu. You still need to process
 *      messages in the menu's owner, as described at the
 *      top of contentmenus.c.
 *
 *      This calls another cmnuPrepareContentSubmenu for
 *      each folder or cmnuInsertOneObjectMenuItem for each
 *      non-folder item to be inserted.
 *
 *      The menu is separated into two sections (for
 *      folders and other objects), each of which
 *      is sorted alphabetically.
 *
 *      This has been extracted from mnuFillContentSubmenu
 *      for clarity and also because the folder mutex is now
 *      requested while the contents are retrieved.
 *
 *      Preconditions:
 *
 *      --  We assume that pFolder is fully populated. This does
 *          not populate pFolder any more.
 *
 *      --  Since this function can take a while and the PM msg
 *          queue is blocked while inserting objects, you should
 *          display a "Wait" pointer while calling this.
 *
 *@@added V0.9.1 (2000-02-01) [umoeller]
 *@@changed V0.9.3 (2000-04-28) [umoeller]: now pre-resolving wpQueryContent for speed
 */

VOID cmnuInsertObjectsIntoMenu(WPFolder *pFolder,   // in: folder whose contents
                                                    // are to be inserted
                               HWND hwndMenu)       // in: submenu to append items to
{
    HAB             habDesktop = WinQueryAnchorBlock(HWND_DESKTOP);
    BOOL            fFolderLocked = FALSE;
    PLISTNODE       pNode = NULL;
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    // We will first create two lists in memory
    // for all folders and non-folders; we will
    // then sort these lists alphabetically and
    // finally insert their content into the menu
    PLINKLIST       pllFolders = lstCreate(TRUE),   // items are freeable
                    pllNonFolders = lstCreate(TRUE);

    MENUITEM        mi;

    SHORT           sItemId;
    SHORT           sItemSize, sItemsPerColumn, sItemCount,
                    sColumns,
                    s;

    RECTL           rtlItem;

    ULONG           ulObjectsLeftOut = 0;
                // counts items which were left out because
                // too many are in the folder to be displayed

    ULONG           ulNesting;
    DosEnterMustComplete(&ulNesting);
    TRY_LOUD(excpt1)
    {
        WPObject        *pObject, *pObject2;

        // subclass menu window to allow MB2 clicks
        G_pfnwpFolderContentMenuOriginal
            = WinSubclassWindow(hwndMenu,
                                fnwpSubclFolderContentMenu);

        // remove "empty" item (if it exists)
        winhRemoveMenuItem(hwndMenu,
                           (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_DUMMY));

        // start collecting stuff; lock the folder contents,
        // do this in a protected block (exception handler,
        // must-complete section)
        fFolderLocked = !wpshRequestFolderMutexSem(pFolder, 5000);
        if (fFolderLocked)
        {
            ULONG   ulTotalObjectsAdded = 0;

            // pre-resolve _wpQueryContent for speed V0.9.3 (2000-04-28) [umoeller]
            somTD_WPFolder_wpQueryContent rslv_wpQueryContent
                    = SOM_Resolve(pFolder, WPFolder, wpQueryContent);

            // now collect all objects in folder
            for (pObject = rslv_wpQueryContent(pFolder, NULL, QC_FIRST);
                 pObject;
                 pObject = rslv_wpQueryContent(pFolder, pObject, QC_Next))
            {
                // dereference shadows, if necessary
                pObject2 = pObject;
                while (pObject2)
                {
                    if (_somIsA(pObject2, _WPShadow))
                        pObject2 = _wpQueryShadowedObject(pObject2, TRUE);
                    else
                        break;
                }

                if (pObject2)
                {
                    // got an object (either real or dereferenced shadow):
                    BOOL fInsertThis = TRUE;

                    // exclude hidden file system objects
                    if (_somIsA(pObject, _WPFileSystem))
                    {
                        if ( _wpQueryAttr(pObject2) & FILE_HIDDEN )
                           fInsertThis = FALSE;
                    }

                    // exclude hidden WPS objects
                    if (_wpQueryStyle(pObject2) & OBJSTYLE_NOTVISIBLE)
                        fInsertThis = FALSE;

                    if (fInsertThis)
                    {
                        BOOL    fIsFolder = (    (_somIsA(pObject2, _WPFolder))
                                              || (_somIsA(pObject2, _WPDisk))
                                            );

                        // append this always if it's a folder
                        // or, if it's no folder, we haven't
                        // exceeded 100 objects yet
                        if (    (fIsFolder)
                             || (ulTotalObjectsAdded < 100)
                           )
                        {
                            PMENULISTITEM pmliNew = malloc(sizeof(MENULISTITEM));
                            pmliNew->pObject = pObject2;
                            strcpy(pmliNew->szItemString, _wpQueryTitle(pObject2));
                            // remove line breaks
                            strhBeautifyTitle(pmliNew->szItemString);

                            if (fIsFolder)
                                // folder/disk: append to folder list
                                lstAppendItem(pllFolders,
                                              pmliNew);
                            else
                                // other (non-folder):
                                // append to objects list
                                lstAppendItem(pllNonFolders,
                                              pmliNew);

                            // raise object count (to avoid too many)
                            ulTotalObjectsAdded++;
                        }
                        else
                            // item left out: count those separately
                            // to for informational menu item later
                            ulObjectsLeftOut++;
                    }
                }
            } // end for pObject
        } // end if (fFolderLocked)
    }
    CATCH(excpt1) { } END_CATCH();

    if (fFolderLocked)
        wpshReleaseFolderMutexSem(pFolder);

    DosExitMustComplete(&ulNesting);

    // now sort the lists alphabetically
    lstQuickSort(pllFolders,
                 fncbSortContentMenuItems,
                 (PVOID)habDesktop);
    lstQuickSort(pllNonFolders,
                 fncbSortContentMenuItems,
                 (PVOID)habDesktop);

    // insert folders into menu
    pNode = lstQueryFirstNode(pllFolders);
    while (pNode)
    {
        PMENULISTITEM pmli = pNode->pItemData;
        // folder items
        sItemId = cmnuPrepareContentSubmenu(pmli->pObject,
                                            hwndMenu,
                                            pmli->szItemString,
                                            MIT_END,
                                            pGlobalSettings->FCShowIcons);
                                                     // OwnerDraw flag

        // next folder
        pNode = pNode->pNext;
    }

    // if we have both objects and folders:
    // insert separator between them
    if (    (pllFolders->ulCount)
         && (pllNonFolders->ulCount)
       )
       winhInsertMenuSeparator(hwndMenu, MIT_END,
                               (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_SEPARATOR));

    // insert non-folder objects into menu
    pNode = lstQueryFirstNode(pllNonFolders);
    while (pNode)
    {
        PMENULISTITEM pmli = pNode->pItemData;
        sItemId = cmnuInsertOneObjectMenuItem(hwndMenu,
                                              MIT_END,
                                              pmli->szItemString,
                                              ((pGlobalSettings->FCShowIcons)
                                                 ? MIS_OWNERDRAW
                                                 : MIS_TEXT),
                                              pmli->pObject,
                                              OC_CONTENT);
        if (sItemId)
            pNode = pNode->pNext;
        else
            pNode = NULL;

        {
            SWP swpMenu;
            WinQueryWindowPos(hwndMenu, &swpMenu);
            _Pmpf(("current window pos: %d, %d, %d, %d",
                    swpMenu.x, swpMenu.y, swpMenu.cx, swpMenu.cy));
        }
    }

    // did we leave out any objects?
    if (ulObjectsLeftOut)
    {
        // yes: add a message saying so
        CHAR    szMsgItem[300];
        winhInsertMenuSeparator(hwndMenu, MIT_END,
                                (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_SEPARATOR));
        sprintf(szMsgItem, "... %d objects dropped,", ulObjectsLeftOut);
        winhInsertMenuItem(hwndMenu,
                           MIT_END,
                           (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_DUMMY),
                           szMsgItem,
                           MIS_TEXT,
                           MIA_DISABLED);
        winhInsertMenuItem(hwndMenu,
                           MIT_END,
                           (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_DUMMY),
                           "open folder to see them",
                           MIS_TEXT,
                           MIA_DISABLED);
    }

    // calculate maximum number of items per column by looking
    // at the screen and item sizes
    WinSendMsg(hwndMenu,
               MM_QUERYITEMRECT,
               MPFROM2SHORT(sItemId, FALSE),
               (MPARAM)&rtlItem);
    sItemSize = (rtlItem.yTop - rtlItem.yBottom);
    if (sItemSize == 0)
        sItemSize = 20;
    sItemsPerColumn = (USHORT)( (  WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN)
                                   - 30
                                )
                                / sItemSize );
    if (sItemsPerColumn == 0)
        sItemsPerColumn = 20;

    // sItemsPerColumn now contains the items which will
    // beautifully fit into one column; we now reduce this
    // number if the last column would contain white space
    sItemCount = (USHORT)WinSendMsg(hwndMenu,
                                    MM_QUERYITEMCOUNT,
                                    0, 0);
    // calculate number of resulting columns
    sColumns = (sItemCount / sItemsPerColumn) + 1;
    // distribute remainder in last column to all columns;
    // if you don't get this, don't worry, I got no clue either
    sItemsPerColumn -= (    (    sItemsPerColumn
                               - (sItemCount % sItemsPerColumn)
                            )
                         / sColumns
                       );

    // now, for through every (sItemsPerColumn)'th menu item,
    // set MIS_BREAK style to force a new column
    for (s = sItemsPerColumn;
         s < sItemCount;
         s += sItemsPerColumn)
    {
        sItemId = (USHORT)WinSendMsg(hwndMenu,
                                     MM_ITEMIDFROMPOSITION,
                                     (MPARAM)s, MPNULL);
        WinSendMsg(hwndMenu,
                   MM_QUERYITEM,
                   MPFROM2SHORT(sItemId, FALSE),
                   &mi);
        mi.afStyle |= MIS_BREAK;
        WinSendMsg(hwndMenu,
                   MM_SETITEM,
                   MPFROM2SHORT(sItemId, FALSE),
                   &mi);
    }

    // clean up
    lstFree(pllFolders);
    lstFree(pllNonFolders);
}

/*
 *@@ cmnuFillContentSubmenu:
 *      this fills a folder content submenu stub (which was created
 *      with cmnuPrepareContentSubmenu) with the contents of the
 *      corresponding folder.
 *
 *      This checks for the "empty" menu item inserted by
 *      cmnuPrepareContentSubmenu and removes that before inserting
 *      the folder contents. As a result, this only works with menu
 *      stubs inserted by cmnuPrepareContentSubmenu. (To directly
 *      fill a menu, call cmnuInsertObjectsIntoMenu instead, which
 *      otherwise gets called by this function.)
 *
 *      This must then be called from the menu's owner window proc
 *      when WM_INITMENU is received for a folder content menu item.
 *      For example, fdr_fnwpSubclassedFolderFrame calls this for
 *      folder content menus.
 *
 *      This way, we can fill the folder content submenus only when
 *      this is needed, because we can impossibly populate all
 *      folder content menus when the context menu is initially
 *      opened.
 *
 *      This func only needs the sMenuId returned by that function
 *      and the hwndMenu of the submenu which will be filled.
 *      It automatically finds the folder contents from the
 *      somSelf which was specified with cmnuPrepareContentSubmenu.
 *      The submenu will be subclassed in order to allow opening
 *      objects with MB2.
 *
 *      Note that at the time that WM_INITMENU comes in, the (sub)menu
 *      is still invisible.
 *
 *@@changed V0.9.0 [umoeller]: adjusted for new linklist functions
 *@@changed V0.9.1 (2000-02-01) [umoeller]: extracted mnuInsertObjectsIntoMenu for mutex protection
 */

VOID cmnuFillContentSubmenu(SHORT sMenuId, // in: menu ID of selected folder content submenu
                            HWND hwndMenu) // in: that submenu's window handle
{
    PLISTNODE       pNode = lstQueryFirstNode(&G_llContentMenuItems);
    PCONTENTMENULISTITEM pcmli = NULL;

    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    WPFolder        *pFolder = NULL;

    // first check if the menu contains the "[empty]" item;
    // this means that it's one of the folder content menus
    // and hasn't been filled yet (it could have been re-clicked,
    // and we don't want double menu items);
    if ((ULONG)WinSendMsg(hwndMenu,
                          MM_ITEMIDFROMPOSITION,
                          0, 0)
                == (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_DUMMY)
       )
    {
        _Pmpf(("    first item is DUMMY"));

        // get folder to be populated from the linked list
        // (llContentMenuItems)
        while (pNode)
        {
            pcmli = pNode->pItemData;
            if (pcmli->sMenuId == sMenuId)
            {
                pFolder = pcmli->pFolder;
                break;
            }

            pNode = pNode->pNext;
            pcmli = 0;
        }

        _Pmpf(("    folder is %s", (pFolder) ? _wpQueryTitle(pFolder) : "NULL"));

        // pFolder now contains the folder,
        // pcmli has the CONTENTMENULISTITEM
        if (pFolder)
        {   // folder found: populate

            // show "Wait" pointer
            HPOINTER    hptrOld = winhSetWaitPointer();

            // if pFolder is a disk object: get root folder
            if (_somIsA(pFolder, _WPDisk))
                pFolder = wpshQueryRootFolder(pFolder, FALSE, NULL);

            if (pFolder)
            {
                // populate
                wpshCheckIfPopulated(pFolder,
                                     FALSE);    // full populate

                if (_wpQueryContent(pFolder, NULL, QC_FIRST))
                {
                    // folder does contain objects: go!
                    // insert all objects (this takes a long time)...
                    cmnuInsertObjectsIntoMenu(pFolder,
                                              hwndMenu);

                    // fix menu position...
                }
            }

            // reset the mouse pointer
            WinSetPointer(HWND_DESKTOP, hptrOld);
        } // end if (pFolder)
    } // end if MM_ITEMIDFROMPOSITION == ID_XFMI_OFS_DUMMY
}

/*
 *@@ cmnuGetVarItem:
 *
 *@@added V0.9.7 (2000-11-29) [umoeller]
 */

PVARMENULISTITEM cmnuGetVarItem(ULONG ulOfs)
{
    PVARMENULISTITEM pItem = (PVARMENULISTITEM)lstItemFromIndex(&G_llVarMenuItems,
                                                                ulOfs);
    return (pItem);
}

/* ******************************************************************
 *
 *   Functions for folder content menu ownerdraw
 *
 ********************************************************************/

/*
 *@@ cmnuPrepareOwnerDraw:
 *      this is called from the subclassed folder frame procedure
 *      (fdr_fnwpSubclassedFolderFrame in xfldr.c) when it receives
 *      WM_INITMENU for a folder content submenu. We can now
 *      do a few queries to get important data which we need for
 *      owner-drawing later.
 *
 *@@changed V0.9.0 [umoeller]: changed colors
 */

VOID cmnuPrepareOwnerDraw(// SHORT sMenuIDMsg, // from WM_INITMENU: SHORT mp1 submenu id
                          HWND hwndMenuMsg) // from WM_INITMENU: HWND  mp2 menu window handle
{
    // PCKERNELGLOBALS  pKernelGlobals = krnQueryGlobals();

    // query bounding rectangle of "[empty]" item, according to
    // which we will format our own items
    WinSendMsg(hwndMenuMsg,
               MM_QUERYITEMRECT,
               MPFROM2SHORT(WinSendMsg(hwndMenuMsg,
                                       MM_ITEMIDFROMPOSITION,
                                       0, 0),
                            FALSE),
               &G_rtlMenuItem);

    // query presentation parameters (colors and font) for menu (changed V0.9.0)
    G_lBackground     = winhQueryPresColor(hwndMenuMsg,
                                           PP_MENUBACKGROUNDCOLOR,
                                           FALSE,       // no inherit
                                           SYSCLR_MENU);
    G_lText           = winhQueryPresColor(hwndMenuMsg,
                                           PP_MENUFOREGROUNDCOLOR, FALSE, SYSCLR_MENUTEXT);
    G_lHiliteBackground = winhQueryPresColor(hwndMenuMsg,
                                           PP_MENUHILITEBGNDCOLOR, FALSE, SYSCLR_MENUHILITEBGND);
    G_lHiliteText     = winhQueryPresColor(hwndMenuMsg,
                                           PP_MENUHILITEFGNDCOLOR, FALSE, SYSCLR_MENUHILITE);
}

/*
 *@@ cmnuMeasureItem:
 *      this is called from the subclassed folder frame procedure
 *      (fdr_fnwpSubclassedFolderFrame in xfldr.c) when it receives
 *      WM_MEASUREITEM for each owner-draw folder content menu item.
 *      We will use the data queried above to calculate the dimensions
 *      of the items we're going to draw later.
 *      We must return the MRESULT for WM_MEASUREITEM here, about
 *      which different documentation exists... I have chosen to return
 *      the height of the menu bar only.
 *
 *@@changed V0.9.0 [umoeller]: adjusted for new linklist functions
 */

MRESULT cmnuMeasureItem(POWNERITEM poi,      // owner-draw info structure
                        PCGLOBALSETTINGS pGlobalSettings) // shortcut to global settings
{
    MRESULT mrc = (MRESULT)FALSE;

    // get the item from the linked list of variable menu items
    // which corresponds to the menu item whose size is being queried
    PVARMENULISTITEM pItem
        = (PVARMENULISTITEM)lstItemFromIndex(&G_llVarMenuItems,
                                             (poi->idItem
                                                - (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_VARIABLE)));

    if (G_ulMiniIconSize == 0)
        // not queried yet?
        G_ulMiniIconSize = WinQuerySysValue(HWND_DESKTOP, SV_CYICON) / 2;

    if (pItem)
    {
        // find out the space required for drawing this item with
        // the current font and fill the owner draw structure (mp2)
        // accordingly
        POINTL aptlText[TXTBOX_COUNT];
        GpiQueryTextBox(poi->hps,
                        strlen(pItem->szTitle),
                        pItem->szTitle,
                        TXTBOX_COUNT,
                        (PPOINTL)&aptlText);
        poi->rclItem.xLeft = 0;
        poi->rclItem.yBottom = 0;
        poi->rclItem.xRight = aptlText[TXTBOX_TOPRIGHT].x
                                + G_ulMiniIconSize
                                - 15
                                + CX_ARROW;

        poi->rclItem.yTop = G_rtlMenuItem.yTop - G_rtlMenuItem.yBottom;
        /* if (poi->rclItem.yTop < G_ulMiniIconSize)
            poi->rclItem.yTop = G_ulMiniIconSize; */
    }
    mrc = MRFROMSHORT(poi->rclItem.yTop); //(MPARAM)poi->rclItem.yTop;

    return (mrc);
}

/*
 *@@ cmnuDrawItem:
 *      this is called from the subclassed folder frame procedure
 *      (fdr_fnwpSubclassedFolderFrame in xfldr.c) when it receives
 *      WM_DRAWITEM for each owner-draw folder content menu item.
 *      We will draw one menu item including the icons with each
 *      call of this function.
 *      This must return TRUE if the item was drawn.
 *
 *@@changed V0.9.0 [umoeller]: adjusted for new linklist functions
 */

BOOL cmnuDrawItem(PCGLOBALSETTINGS pGlobalSettings,   // shortcut to global settings
                  MPARAM mp1,     // from WM_DRAWITEM: USHORT menu item id
                  MPARAM mp2)     // from WM_DRAWITEM: POWNERITEM structure
{
    BOOL brc = FALSE;
    // HBITMAP hbm;
    RECTL      rcl;
    // ULONG      x;
    LONG       lColor; // , lBmpBackground;
    POWNERITEM poi = (POWNERITEM)mp2;
    POINTL     ptl;

    // get the item from the linked list of variable menu items
    // which corresponds to the menu item being drawn
    PVARMENULISTITEM pItem
        = (PVARMENULISTITEM)lstItemFromIndex(&G_llVarMenuItems,
                                             (poi->idItem
                                                - (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_VARIABLE)));
    HPOINTER hIcon;

    if (pItem)
    {
        // get the item's (object's) icon;
        // this call can take a while if the folder
        // was just queried
        hIcon = _wpQueryIcon(pItem->pObject);

        // switch to RGB mode
        GpiCreateLogColorTable(poi->hps, 0, LCOLF_RGB, 0, 0, NULL);

        // find out the background color, which depends
        // on whether the item is highlighted (= selected);
        // these colors have been initialized by WM_INITMENU
        // above
        if (poi->fsAttribute & MIA_HILITED)
            lColor = G_lHiliteBackground;
            // lBmpBackground = lBackground;
        else
            lColor = G_lBackground;
            // lBmpBackground = lHiliteBackground;

        // draw rectangle in lColor, size of whole item
        rcl = poi->rclItem;
        WinFillRect(poi->hps, &rcl, lColor);

        // print the item's text
        ptl.x = poi->rclItem.xLeft+5+(G_ulMiniIconSize);
        ptl.y = poi->rclItem.yBottom+4;
        GpiMove(poi->hps, &ptl);
        GpiSetColor(poi->hps,
                    (poi->fsAttribute & MIA_HILITED)
                        ? G_lHiliteText
                        : G_lText);
        GpiCharString(poi->hps, strlen(pItem->szTitle), pItem->szTitle);

        // draw the item's icon
        WinDrawPointer(poi->hps,
                       poi->rclItem.xLeft+2,
                       poi->rclItem.yBottom
                         +( (G_rtlMenuItem.yTop - G_rtlMenuItem.yBottom - G_ulMiniIconSize) / 2 ),
                       hIcon,
                       DP_MINI);

        if (poi->fsAttribute != poi->fsAttributeOld)
        {
            // if the attribute has changed, i.e. item's
            // hilite state has been altered: we then need
            // to repaint the little "submenu" arrow, because
            // this has been overpainted by the WinFilLRect
            // above. We do this using icons from the XFLDR.DLL
            // resources, because no system bitmap has been
            // defined for the little Warp 4 triangle.
            if (pItem->ulObjType == OC_CONTENTFOLDER)
            {
                if (G_hMenuArrowIcon == NULLHANDLE)
                {
                    G_hMenuArrowIcon = WinLoadPointer(HWND_DESKTOP,
                                                      cmnQueryMainResModuleHandle(),
                                                      doshIsWarp4()
                                                          // on Warp 4, load the triangle
                                                        ? ID_ICONMENUARROW4
                                                          // on Warp 3, load the arrow
                                                        : ID_ICONMENUARROW3);
                }
                // _Pmpf(("hIcon: 0x%lX", hMenuArrowIcon));
                if (G_hMenuArrowIcon)
                {
                    WinDrawPointer(poi->hps,
                                   poi->rclItem.xRight - CX_ARROW,
                                   poi->rclItem.yBottom
                                   +(
                                       (G_rtlMenuItem.yTop - G_rtlMenuItem.yBottom - G_ulMiniIconSize) / 2
                                   ),
                                   G_hMenuArrowIcon,
                                   DP_MINI);
                }
            }
        }

        // now, this is funny: we need to ALWAYS delete the
        // MIA_HILITED flag in both the old and new attributes,
        // or PM will invert the item again for some reason;
        // this must be code from the stone ages (i.e. Microsoft)
        poi->fsAttributeOld = (poi->fsAttribute &= ~MIA_HILITED);

        brc = TRUE;
    }
    else
        brc = FALSE;

    return (brc);
}


