
/*
 *@@sourcefile fdrmenus.c:
 *      this file contains the menu manipulation  logic for
 *      most of the XWorkplace menu features.
 *
 *      The functions in here are called by the XFolder and
 *      XFldDisk WPS methods for context menus. Since those two
 *      classes share many common menu items, they also share
 *      the routines for handling them.
 *
 *      mnuModifyFolderPopupMenu, which gets called from
 *      XFolder::wpModifyPopupMenu and XFldDisk::wpModifyPopupMenu,
 *      modifies folder and disk context menu items.
 *
 *      This code mainly does two things:
 *
 *      --  Remove default WPS menu items according to the
 *          flags that were set on the "Menu" page in
 *          "Workplace Shell". This has been totally reworked
 *          with V0.9.19. See MENUITEMDEF for explanations.
 *
 *      --  Recursively add the contents of the XFolder config
 *          folder to a context menu (on modify) and react to
 *          selections of these menu items.
 *
 *      --  Handle folder content menus, with the help of the
 *          shared content menu functions in src\shared\contentmenus.c.
 *
 *      Function prefix for this file:
 *      --  mnu*
 *
 *      This file was new with XFolder V0.81. This probably
 *      contains some of the oldest code that has been in XFolder
 *      forever, since config folders was what XFolder really
 *      started out with. So don't expect this code to be perfectly
 *      well designed.
 *
 *      All these functions used to be in xfldr.c with early
 *      XFolder versions and have been moved here later. This code
 *      has been completely worked over several times, and parts
 *      have been moved between other files.
 *
 *      With V0.9.7, all menu functions related to folder _content_
 *      menus have been moved to src\shared\contentmenus.c to
 *      allow sharing with other code parts, such as the XCenter.
 *
 *      With V0.9.21, the menu _selection_ logic was moved to
 *      fdrcommand.c.
 *
 *@@header "filesys\fdrmenus.h"
 */

/*
 *      Copyright (C) 1997-2002 Ulrich M”ller.
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

#define INCL_DOSEXCEPTIONS
#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#define INCL_DOSERRORS

#define INCL_WINWINDOWMGR
#define INCL_WINMESSAGEMGR
#define INCL_WINFRAMEMGR        // SC_CLOSE etc.
#define INCL_WININPUT
#define INCL_WINDIALOGS
#define INCL_WINBUTTONS
#define INCL_WINPOINTERS
#define INCL_WINMENUS
#define INCL_WINSTATICS
#define INCL_WINLISTBOXES
#define INCL_WINENTRYFIELDS
#define INCL_WINSTDCNR
#define INCL_WINMLE
#define INCL_WINCOUNTRY
#define INCL_WINCLIPBOARD
#define INCL_WINSYS
#define INCL_WINPROGRAMLIST     // needed for PROGDETAILS, wppgm.h
#define INCL_WINSHELLDATA       // Prf* functions

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
#include "helpers\comctl.h"             // common controls (window procs)
#include "helpers\cnrh.h"               // container helper routines
#include "helpers\dialog.h"             // dialog helpers
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\standards.h"          // some standard macros
#include "helpers\stringh.h"            // string helper routines
#include "helpers\syssound.h"           // system sound helper routines
#include "helpers\threads.h"            // thread helpers
#include "helpers\winh.h"               // PM helper routines
#include "helpers\xstring.h"            // extended string helpers

// SOM headers which don't crash with prec. header files
#include "xfobj.ih"                     // XFldObject
#include "xwpstring.ih"                 // XWPString
#include "xfdisk.ih"                    // XFldDisk
#include "xfldr.ih"                     // XFolder

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\classtest.h"           // some cheap funcs for WPS class checks
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\helppanels.h"          // all XWorkplace help panel IDs
#include "shared\contentmenus.h"        // shared menu logic
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

#include "filesys\folder.h"             // XFolder implementation
#include "filesys\fdrmenus.h"           // shared folder menu logic
#include "filesys\object.h"             // XFldObject implementation
#include "filesys\program.h"            // program implementation; WARNING: this redefines macros
#include "filesys\statbars.h"           // status bar translation logic
#include "filesys\xthreads.h"           // extra XWorkplace threads

#include "startshut\shutdown.h"         // XWorkplace eXtended Shutdown

// other SOM headers
#pragma hdrstop                         // VAC++ keeps crashing otherwise
#include <wppgm.h>                      // WPProgram, needed for program hacks

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

// linked list for config folder content:
static HMTX     G_hmtxConfigContent = NULLHANDLE;   // V0.9.9 (2001-04-04) [umoeller]
static LINKLIST G_llConfigContent;
static BOOL     G_fConfigCacheValid;                // if FALSE, cache is rebuilt

extern POINTL   G_ptlMouseMenu = {0, 0};    // ptr position when menu was opened
                                            // moved this here from XFolder instance
                                            // data V0.9.16 (2001-10-23) [umoeller]

/* ******************************************************************
 *
 *   Global WPS menu settings
 *
 ********************************************************************/

/*
 *@@ mnuQueryDefaultMenuBarVisibility:
 *      returns TRUE iff menu bars are presently
 *      enabled for the system (globally).
 *      On Warp 3, returns FALSE always.
 *
 *@@added V0.9.19 (2002-04-17) [umoeller]
 */

BOOL mnuQueryDefaultMenuBarVisibility(VOID)
{
    if (G_fIsWarp4)
    {
        CHAR    szTemp[20] = "";
        PrfQueryProfileString(HINI_USER,
                              (PSZ)WPINIAPP_WORKPLACE, // "PM_Workplace"
                              (PSZ)WPINIKEY_MENUBAR, // "FolderMenuBar",
                              "ON",         // V0.9.9 (2001-03-27) [umoeller]
                              szTemp,
                              sizeof(szTemp));
        if (!strcmp(szTemp, "ON"))
            return TRUE;
    }

    return FALSE;
}

/*
 *@@ mnuSetDefaultMenuBarVisibility:
 *      reversely to mnuQueryDefaultMenuBarVisibility,
 *      sets the default setting for folder menu bars.
 *      Returns FALSE if an error occured, most importantly,
 *      if we're running on Warp 3.
 *
 *@@added V0.9.19 (2002-04-17) [umoeller]
 */

BOOL mnuSetDefaultMenuBarVisibility(BOOL fVisible)
{
    if (G_fIsWarp4)
    {
        return PrfWriteProfileString(HINI_USER,
                                     (PSZ)WPINIAPP_WORKPLACE, // "PM_Workplace"
                                     (PSZ)WPINIKEY_MENUBAR, // "FolderMenuBar",
                                     fVisible ? "ON" : "OFF");
    }

    return FALSE;
}

/*
 *@@ mnuQueryShortMenuStyle:
 *      returns TRUE iff short menus are
 *      presently enabled for the system (globally).
 *      On Warp 3, returns FALSE always.
 *
 *
 *@@added V0.9.19 (2002-04-17) [umoeller]
 */

BOOL mnuQueryShortMenuStyle(VOID)
{
    if (G_fIsWarp4)
    {
        CHAR    szTemp[20] = "";
        PrfQueryProfileString(HINI_USER,
                              (PSZ)WPINIAPP_WORKPLACE, // "PM_Workplace"
                              (PSZ)WPINIKEY_SHORTMENUS, // "FolderMenus"
                              "",
                              szTemp,
                              sizeof(szTemp));
        if (!strcmp(szTemp, "SHORT"))
            return TRUE;
    }

    return FALSE;
}

/*
 *@@ mnuSetShortMenuStyle:
 *      reversely to mnuQueryShortMenuStyle,
 *      sets the default setting for short menus.
 *      Returns FALSE if an error occured, most
 *      importantly, if we're running on Warp 3.
 *
 *@@added V0.9.19 (2002-04-17) [umoeller]
 */

BOOL mnuSetShortMenuStyle(BOOL fShort)
{
    if (G_fIsWarp4)
    {
        return PrfWriteProfileString(HINI_USER,
                                     (PSZ)WPINIAPP_WORKPLACE, // "PM_Workplace"
                                     (PSZ)WPINIKEY_SHORTMENUS, // "FolderMenus"
                                     fShort ? "SHORT" : NULL);
    }

    return FALSE;
}

/* ******************************************************************
 *
 *   Menu manipulation
 *
 ********************************************************************/

#pragma pack(1)

/*
 *@@ MENUITEMDEF:
 *      highly obfuscated structure for each menu
 *      item that is either supported by the WPS
 *      or by XWP. Menu items configuration is
 *      thus now unified with V0.9.19.
 *
 *      The way this works is as follows:
 *
 *      1)  There are several "menu categories" for
 *          certain groups of WPS classes. These
 *          are specified with a bit mask here. These
 *          appear in the "Category" drop-down on the
 *          "Workplace Shell" Menu 2 page.
 *
 *          The following categories are supported:
 *
 *          --  CONFFL_WPOBJECT             0x00000000
 *          --  CONFFL_WPDATAFILE           0x00000001
 *          --  CONFFL_WPFOLDER             0x00000002
 *          --  CONFFL_WPDESKTOP            0x00000004
 *          --  CONFFL_WPDISK               0x00000008
 *
 *          I could also have tied this to each specific
 *          WPS class, but that would have meant configuration
 *          hell for the user in the "Workplace Shell"
 *          object.
 *
 *      2)  For each such category, there are two XWPSETTING
 *          items (cmnQuerySetting), one for the WPS menu items
 *          holding a CTXT_* bit mask, and one for the new
 *          XWP menu items holding an XWPCTXT_* bit mask.
 *
 *          Note that the XWPCTXT_* represent both new
 *          XWP menu items such as the "File attributes"
 *          menu plus some WPS menu item for which there
 *          is no CTXT_* flag, such as "locked in place",
 *          because we have to remove them manually via
 *          WinSendMsg from the context menu in wpModifyPopupMenu.
 *          See CATEGORYWITHFLAG.
 *
 *          For example, the XWPSETTING sflMenuFolderWPS
 *          holds the CTXT_* flags for all folders, and
 *          sflMenuFolderXWP holds the XWPCTXT_* flags
 *          for all folders.
 *
 *          When a bit is set, the corresponding menu
 *          item is _removed_. The default value for each
 *          setting is 0, meaning that all WPS and XWP
 *          menu items are visible per default.
 *
 *          These settings replace a lot of the old
 *          XWPSETTING's like sfRemoveLockInPlaceItem
 *          and such. Besides we can now configure
 *          differently whether these menu items should
 *          be visible for each category.
 *
 *          This has consequences:
 *
 *          --  The setting is _reverse_ to the respective
 *              checkbox on the "Menu 2" page. This is so
 *              that we can easily apply the CTXT_* flags
 *              in wpFilterMenu.
 *
 *          --  If a new menu item is added with a new XWP
 *              release, the bit in the XWPSETTING will be
 *              initially 0, meaning that the new item is
 *              visible per default.
 *
 *      3)  The category bit mask is stored in
 *          MENUITEMDEF.flConfig. In addition, there
 *          are a _lot_ of other flags to make the
 *          display correct on the "Menu" page in
 *          "Workplace Shell".
 *
 *@@added V0.9.19 (2002-04-17) [umoeller]
 */

typedef struct _MENUITEMDEF
{
    USHORT      ulString;   // id for cmnGetString
    USHORT      idMenu;     // WPMENUID_* or ID_* if XWP item
    ULONG       flConfig;   // CONFFL_* flags telling the class and config flags
    ULONG       flFilter;   // if XWPCTXT_HIGHBIT (highest bit) is not
                            // set, this is a CTXT_* flag; if it
                            // is set, it is a XWPCTXT_* flag
} MENUITEMDEF, *PMENUITEMDEF;

#pragma pack()

#define ID_SEPARATOR_NO_ID          0
#define ID_SEPARATOR_1              130

#define CONFFL_CLASSMASK            0x000000FF
#define CONFFL_WPOBJECT             0x00000000
            // menu item is supported by all objects,
            // unless CONFFL_FILTERED_* rules it out for a subclass
#define CONFFL_WPDATAFILE           0x00000001
            // menu item is supported by data files only
#define CONFFL_WPFOLDER             0x00000002
            // menu item is supported by folders only,
            // unless CONFFL_FILTERED_* rules it out for a subclass
#define CONFFL_WPDESKTOP            0x00000004
            // menu item is supported by desktops only
#define CONFFL_WPDISK               0x00000008
            // menu item is supported by disks only

#define CONFFL_FILTEREDMASK         0x0000FF00
#define CONFFL_FILTEREDSHIFT        8
#define CONFFL_FILTERED_WPDATAFILE  0x00000100
#define CONFFL_FILTERED_WPFOLDER    0x00000200
#define CONFFL_FILTERED_WPDESKTOP   0x00000400
#define CONFFL_FILTERED_WPDISK      0x00000800

#define CONFFL_CANNOTREMOVE         0x00010000
            // we cannot remove this item, e.g. the "Properties" item
#define CONFFL_HASSUBMENU           0x00020000
            // "Menu" page should mark this as a submenu (just for display)
#define CONFFL_BEGINSUBMENU         0x00040000
            // the following items in the array should be added as children
            // under this item's record (just for display)
#define CONFFL_ENDSUBMENU           0x00080000
            // this was the last item after CONFFL_BEGINSUBMENU, terminate
            // adding children (just for display)
#define CONFFL_NOQUOTES             0x00100000
            // do not add quotes around string
#define CONFFL_NOTINSHORTMENUS      0x00200000
            // menu item is hidden when short menus are on

/*
 *@@ G_MenuItemsWithIDs:
 *      array of MENUITEMDEF structs specifying
 *      all menu items supported either by the WPS
 *      or by XWP, for all classes, in the correct
 *      order as they appear in the menus.
 *
 *@@added V0.9.19 (2002-04-17) [umoeller]
 */

static const MENUITEMDEF G_MenuItemsWithIDs[] =
    {
        // "~Open as"
        ID_XSDI_MENU_OPENAS, WPMENUID_OPEN,
                CONFFL_CANNOTREMOVE | CONFFL_HASSUBMENU,
                0,
        // "Propertie~s",
        ID_XSDI_MENU_PROPERTIES, WPMENUID_PROPERTIES,
                CONFFL_CANNOTREMOVE,
                0,
        // "Open parent",
        ID_XSDI_MENU_OPENPARENT, WPMENUID_OPENPARENT,
                CONFFL_CANNOTREMOVE | CONFFL_WPFOLDER,
                0,
        // "~Help"
        DID_HELP, WPMENUID_HELP,
                CONFFL_HASSUBMENU | CONFFL_NOTINSHORTMENUS,
                CTXT_HELP,
        // "Create a~nother",
        ID_XSDI_MENU_CREATEANOTHER, WPMENUID_CREATEANOTHER,
                CONFFL_HASSUBMENU | CONFFL_FILTERED_WPDISK | CONFFL_FILTERED_WPDESKTOP | CONFFL_NOTINSHORTMENUS,
                CTXT_CRANOTHER,
        // "~Move..."
        ID_XSDI_MENU_MOVE, WPMENUID_MOVE,
                CONFFL_FILTERED_WPDISK | CONFFL_FILTERED_WPDESKTOP | CONFFL_NOTINSHORTMENUS,
                CTXT_MOVE,
        // "~Copy..."
        ID_XSDI_MENU_COPY, WPMENUID_COPY,
                CONFFL_FILTERED_WPDISK | CONFFL_FILTERED_WPDESKTOP | CONFFL_NOTINSHORTMENUS,
                CTXT_COPY,
        // "Paste...",
        ID_XSDI_MENU_PASTE, WPMENUID_PASTE,
                CONFFL_WPFOLDER | CONFFL_WPDISK,    // this is in short menus
                CTXT_PASTE,
        // "Create shado~w..."
        ID_XSDI_MENU_CREATESHADOW, WPMENUID_CREATESHADOW,
                CONFFL_NOTINSHORTMENUS,
                CTXT_SHADOW,
        // "~Delete...",
        ID_XSDI_MENU_DELETE, WPMENUID_DELETE,
                CONFFL_FILTERED_WPDISK | CONFFL_FILTERED_WPDESKTOP | CONFFL_NOTINSHORTMENUS,
                CTXT_DELETE,
        // "P~ickup",
        ID_XSDI_MENU_PICKUP, WPMENUID_PICKUP,
                CONFFL_FILTERED_WPDESKTOP,
                CTXT_PICKUP,
        // "~Find..."
        ID_XSDI_MENU_FIND, WPMENUID_FIND,
                CONFFL_WPFOLDER,
                CTXT_FIND,
        // "~View"
        ID_XFSI_FLDRSETTINGS, WPMENUID_VIEW,
                CONFFL_WPFOLDER | CONFFL_BEGINSUBMENU,
                CTXT_VIEW,
            // "~Icon view"
            ID_XSDI_MENU_ICONVIEW, WPMENUID_CHANGETOICON,
                    CONFFL_WPFOLDER,
                    CTXT_CHANGETOICON,
            // "~Tree view"
            ID_XSDI_MENU_TREEVIEW, WPMENUID_CHANGETOTREE,
                    CONFFL_WPFOLDER,
                    CTXT_CHANGETOTREE,
            // "~Details view"
            ID_XSDI_MENU_DETAILSVIEW, WPMENUID_CHANGETODETAILS,
                    CONFFL_WPFOLDER,
                    CTXT_CHANGETODETAILS,
            // "Select ~all"
            ID_XFDI_SOME_SELECTALL, WPMENUID_SELALL,
                    CONFFL_WPFOLDER,
                    XWPCTXT_HIGHBIT | XWPCTXT_SELALL,
            // "Dese~lect all"
            ID_XFDI_SOME_DESELECTALL, WPMENUID_DESELALL,
                    CONFFL_WPFOLDER,
                    XWPCTXT_HIGHBIT | XWPCTXT_DESELALL,
            // "Select by name"
            ID_XSSI_SELECTSOME, ID_XFMI_OFS_SELECTSOME,
                    CONFFL_WPFOLDER,
                    XWPCTXT_HIGHBIT | XWPCTXT_SELECTSOME,
            // "Batch rename" V0.9.19 (2002-06-18) [umoeller]
            ID_XSDI_MENU_BATCHRENAME, ID_XFMI_OFS_BATCHRENAME,
                    CONFFL_WPFOLDER,
                    XWPCTXT_HIGHBIT | XWPCTXT_BATCHRENAME,
            // "~Refresh now",
            ID_XSSI_REFRESHNOW, WPMENUID_REFRESH,       // 503, correct
                    CONFFL_WPFOLDER,
                    // CTXT_REFRESH,                  this flag doesn't work
                    XWPCTXT_HIGHBIT | XWPCTXT_REFRESH_IN_VIEW,
            // "La~yout items"
            ID_XSDI_FLDRVIEWS, 0,
                    CONFFL_WPFOLDER | CONFFL_ENDSUBMENU | CONFFL_NOQUOTES,
                    XWPCTXT_HIGHBIT | XWPCTXT_LAYOUTITEMS,
        // "Sor~t"
        ID_XSDI_MENU_SORT, WPMENUID_SORT,
                CONFFL_WPFOLDER | CONFFL_HASSUBMENU,
                CTXT_SORT,
        // "~Arrange"
        ID_XSDI_MENU_ARRANGE, WPMENUID_ARRANGE,
                CONFFL_WPFOLDER | CONFFL_HASSUBMENU,
                CTXT_ARRANGE,
/*
           "~Standard", WPMENUID_STANDARD, CONFFL_WPFOLDER, 0,
           "From ~Top", WPMENUID_ARRANGETOP, CONFFL_WPFOLDER, 0,
           "From ~Left", WPMENUID_ARRANGELEFT, CONFFL_WPFOLDER, 0,
           "From ~Right", WPMENUID_ARRANGERIGHT, CONFFL_WPFOLDER, 0,
           "From ~Bottom", WPMENUID_ARRANGEBOTTOM, CONFFL_WPFOLDER, 0,
           "~Perimeter", WPMENUID_PERIMETER, CONFFL_WPFOLDER, 0,
           "Selected ~Horizontal", WPMENUID_SELECTEDHORZ, CONFFL_WPFOLDER, 0,
           "Selected ~Vertical", WPMENUID_SELECTEDVERT, CONFFL_WPFOLDER | CONFFL_ENDSUBMENU, 0,
*/
        // "~Print"
        ID_XSDI_MENU_PRINT, WPMENUID_PRINT,
                CONFFL_HASSUBMENU | CONFFL_FILTERED_WPDISK | CONFFL_FILTERED_WPFOLDER
                        | CONFFL_FILTERED_WPDESKTOP | CONFFL_NOTINSHORTMENUS,
                CTXT_PRINT,
        // "Lock in place"
        ID_XSDI_ICON_LOCKPOSITION_CB, WPMENUID_LOCKEDINPLACE,
                0,
                XWPCTXT_HIGHBIT | XWPCTXT_LOCKEDINPLACE,
        // "~Lockup now",
        ID_XSDI_MENU_LOCKUP, WPMENUID_LOCKUP,
                CONFFL_WPDESKTOP,
                CTXT_LOCKUP,
        // "Lo~goff network now",
        ID_XSDI_MENU_LOGOFFNETWORKNOW, WPMENUID_LOGOFF,
                CONFFL_WPDESKTOP,
                XWPCTXT_HIGHBIT | XWPCTXT_LOGOFF,
        // "Shut ~down..."
        ID_XSDI_MENU_SHUTDOWN, WPMENUID_SHUTDOWN,
                CONFFL_WPDESKTOP,
                CTXT_SHUTDOWN,
        // "Shut ~down..."
        ID_XSDI_MENU_SHUTDOWN, WPMENUID_SHUTDOWN,
                CONFFL_WPDESKTOP,
                CTXT_SHUTDOWN,
        // "S~ystem setup"
        ID_XSDI_MENU_SYSTEMSETUP, WPMENUID_SYSTEMSETUP,
                CONFFL_WPDESKTOP,
                XWPCTXT_HIGHBIT | XWPCTXT_SYSTEMSETUP,
        // "Chec~k disk..."
        ID_XSDI_MENU_CHKDSK, WPMENUID_CHKDSK,
                CONFFL_WPDISK,
                XWPCTXT_HIGHBIT | XWPCTXT_CHKDSK,
        // "Fo~rmat disk..."
        ID_XSDI_MENU_FORMAT, WPMENUID_FORMAT,
                CONFFL_WPDISK,
                XWPCTXT_HIGHBIT | XWPCTXT_FORMAT,
        // "Co~py disk..."
        ID_XSDI_MENU_COPYDSK, WPMENUID_COPYDSK,
                CONFFL_WPDISK,
                XWPCTXT_HIGHBIT | XWPCTXT_COPYDSK,
        // "~Lock disk"
        ID_XSDI_MENU_LOCKDISK, WPMENUID_LOCKDISK,
                CONFFL_WPDISK,
                XWPCTXT_HIGHBIT | XWPCTXT_LOCKDISK,
        // "~Eject disk"
        ID_XSDI_MENU_EJECTDISK, WPMENUID_EJECTDISK,
                CONFFL_WPDISK,
                XWPCTXT_HIGHBIT | XWPCTXT_EJECTDISK,
        // "~Unlock disk"
        ID_XSDI_MENU_UNLOCKDISK, WPMENUID_UNLOCKDISK,
                CONFFL_WPDISK,
                XWPCTXT_HIGHBIT | XWPCTXT_UNLOCKDISK,
        // "File attributes"
        ID_XFSI_ATTRIBUTES, ID_XFM_OFS_ATTRIBUTES,
                CONFFL_WPDATAFILE | CONFFL_WPFOLDER,
                XWPCTXT_HIGHBIT | XWPCTXT_ATTRIBUTESMENU,
        // "Co~py filename"
        ID_XSSI_COPYFILENAME, ID_XFMI_OFS_COPYFILENAME_MENU,
                CONFFL_WPDATAFILE | CONFFL_WPFOLDER,
                XWPCTXT_HIGHBIT | XWPCTXT_COPYFILENAME,
#ifndef __NOMOVEREFRESHNOW__
        // "~Refresh now" in main menu
        ID_XSSI_REFRESHNOW, 0,
                CONFFL_WPFOLDER,
                XWPCTXT_HIGHBIT | XWPCTXT_REFRESH_IN_MAIN,
#endif
        // "F~older contents"
        ID_XSSI_FLDRCONTENT, 0,
                CONFFL_WPFOLDER,
                XWPCTXT_HIGHBIT | XWPCTXT_FOLDERCONTENTS,
    };

/*
 *@@ mnuQueryMenuWPSSetting:
 *      returns the XWPSETTING id for the WPS menu
 *      items which applies to the given object.
 *
 *      This must then be fed into cmnQuerySetting to
 *      get the CTXT_* flags which apply to the object.
 *
 *@@added V0.9.19 (2002-04-17) [umoeller]
 */

XWPSETTING mnuQueryMenuWPSSetting(WPObject *somSelf)
{
    ULONG flObject = objQueryFlags(somSelf);
    if (flObject & OBJFL_WPFOLDER)
    {
        if (cmnIsADesktop(somSelf))
        {
            #ifdef DEBUG_MENUS
                _PmpfF(("returning sflMenuDesktopWPS"));
            #endif

            return sflMenuDesktopWPS;
        }
        else
        {
            #ifdef DEBUG_MENUS
                _PmpfF(("returning sflMenuFolderWPS"));
            #endif

            return sflMenuFolderWPS;
        }
    }
    else if (flObject & OBJFL_WPFILESYSTEM)
        return sflMenuFileWPS;
    else if (_somIsA(somSelf, _WPDisk))
        return sflMenuDiskWPS;

    return sflMenuObjectWPS;
}

/*
 *@@ mnuQueryMenuXWPSetting:
 *      returns the XWPSETTING id for the XWorkplace
 *      menu items which applies to the given object.
 *
 *      This must then be fed into cmnQuerySetting to
 *      get the XWPCTXT_* flags which apply to the object.
 *
 *@@added V0.9.19 (2002-04-17) [umoeller]
 */

XWPSETTING mnuQueryMenuXWPSetting(WPObject *somSelf)
{
    ULONG flObject = objQueryFlags(somSelf);
    if (flObject & OBJFL_WPFOLDER)
    {
        if (cmnIsADesktop(somSelf))
        {
            #ifdef DEBUG_MENUS
                _PmpfF(("returning sflMenuDesktopXWP"));
            #endif

            return sflMenuDesktopXWP;
        }
        else
        {
            #ifdef DEBUG_MENUS
                _PmpfF(("returning sflMenuFolderXWP"));
            #endif

            return sflMenuFolderXWP;
        }
    }
    else if (flObject & OBJFL_WPFILESYSTEM)
        return sflMenuFileXWP;
    else if (_somIsA(somSelf, _WPDisk))
        return sflMenuDiskXWP;

    return sflMenuObjectXWP;
}

/*
 *@@ mnuRemoveMenuItems:
 *      removes a bunch of default WPS menu items
 *      by sending MM_REMOVEITEM against the given
 *      menu.
 *
 *      To be called during wpModifyPopupMenu.
 *
 *      This runs mnuQueryMenuXWPSetting to get
 *      the matching XWPSETTING for the given
 *      object first. We then run through the
 *      given array of XWPCTXT_* flags and check
 *      for each flag if it is set in the XWPSETTING
 *      value. If so, the corresponding menu item
 *      is removed (or a bunch of menu items,
 *      if the XWPCTXT_* flag applies to several
 *      menu items).
 *
 *      For example, if XWPCTXT_CHANGEVIEW is
 *      in the array and the object is a desktop
 *      and has this bit set in sflMenuDesktopXWP,
 *      the menu items with the IDs WPMENUID_CHANGETOICON,
 *      WPMENUID_CHANGETOTREE, and WPMENUID_CHANGETODETAILS
 *      are removed from the given menu.
 *
 *@@added V0.9.19 (2002-04-17) [umoeller]
 */

VOID mnuRemoveMenuItems(WPObject *somSelf,
                        HWND hwndMenu,
                        const ULONG *aSuppressFlags,
                        ULONG cSuppressFlags)
{
    XWPSETTING s = mnuQueryMenuXWPSetting(somSelf);
    ULONG fl = cmnQuerySetting(s);
    ULONG ul;

    #ifdef DEBUG_MENUS
        _PmpfF(("got 0x%08lX for setting %d", fl, s));
    #endif

    for (ul = 0;
         ul < cSuppressFlags;
         ++ul)
    {
        // caller passes in an array of XWPCTXT_* flags
        // if flag is currently set in settings, remove menu item
        if (fl & aSuppressFlags[ul])
        {
            // flag is set: find the menu ID from the array
            // (in the array, the highbit is set!)
            ULONG ul2;
            ULONG flTest = aSuppressFlags[ul] | XWPCTXT_HIGHBIT;

            #ifdef DEBUG_MENUS
            _Pmpf(("   finding flag 0x%08lX", flTest));
            #endif

            for (ul2 = 0;
                 ul2 < ARRAYITEMCOUNT(G_MenuItemsWithIDs);
                 ++ul2)
            {
                if (flTest == G_MenuItemsWithIDs[ul2].flFilter)
                {
                    #ifdef DEBUG_MENUS
                    _PmpfF(("flag %s set, removing id %d",
                            cmnGetString(G_MenuItemsWithIDs[ul2].ulString),
                            G_MenuItemsWithIDs[ul2].idMenu));
                    #endif

                    // regular ID:
                    winhDeleteMenuItem(hwndMenu, G_MenuItemsWithIDs[ul2].idMenu);
                }
            }
        }
    }
}

/*
 *@@ mnuInsertFldrViewItems:
 *      this inserts the folder view menu items
 *      ("Small icons", "Flowed" etc.) into hwndViewSubmenu
 *      if the current cnr view makes sense for this.
 *
 *      This gets called from two places:
 *
 *      -- mnuModifyFolderPopupMenu for regular popup
 *         menus;
 *
 *      -- fnwpSubclWPFolderWindow upon WM_INITMENU
 *         for the "View" pulldown in folder menu bars.
 *
 *      hwndViewSubmenu contains the submenu to add
 *      items to:
 *
 *      --  on Warp 4, this is the default "View" submenu
 *          (either in the context menu or the "View" pulldown)
 *
 *      --  on Warp 3, mnuModifyFolderPopupMenu creates a new
 *          "View" submenu, which is passed to this func.
 *
 *      Returns TRUE if the menu items were inserted.
 *
 *@@changed V0.9.0 [umoeller]: added "menu bar" item under Warp 4
 *@@changed V0.9.0 [umoeller]: fixed wrong separators
 *@@changed V0.9.0 [umoeller]: now using cmnQueryObjectFromID to get the config folder
 *@@changed V0.9.0 [umoeller]: fixed broken "View" item in menu bar
 *@@changed V0.9.21 (2002-08-24) [umoeller]: changed prototype to receive CNRINFO instead of cnr
 */

BOOL mnuInsertFldrViewItems(WPFolder *somSelf,      // in: folder w/ context menu
                            HWND hwndViewSubmenu,   // in: submenu to add items to
                            BOOL fInsertNewMenu,    // in: insert "View" into hwndViewSubmenu?
                            PCNRINFO pCnrInfo,      // in: cnr info
                            ULONG ulView)           // in: OPEN_* flag
{
    BOOL        brc = FALSE;
    XFolderData *somThis = XFolderGetData(somSelf);

    // we have a valid open view:
    ULONG       ulOfs = cmnQuerySetting(sulVarMenuOfs);
    ULONG       ulAttr = 0;
    USHORT      usIconsAttr;

    #ifdef DEBUG_MENUS
        _PmpfF(("entering"));
    #endif


    // add "small icons" item for all view types,
    // but disable for Details view
    if (ulView == OPEN_DETAILS)
    {
        // Details view: disable and check
        // "mini icons"
        usIconsAttr = (MIA_DISABLED | MIA_CHECKED);
    }
    else
    {
        // otherwise: set "mini icons" to cnr info data
        usIconsAttr = (pCnrInfo->flWindowAttr & CV_MINI)
                              ? MIA_CHECKED
                              : 0;
    }

    if (fInsertNewMenu)
    {
        // on Warp 3, we need to add a "View" submenu
        // for the folder view flags, because Warp 3
        // doesn't have one
        hwndViewSubmenu = winhInsertSubmenu(hwndViewSubmenu,
                                            MIT_END,
                                            ulOfs + ID_XFM_OFS_WARP3FLDRVIEW,
                                            cmnGetString(ID_XFSI_FLDRSETTINGS), // pszWarp3FldrView */
                                                MIS_SUBMENU,
                                            // item
                                            ulOfs + ID_XFMI_OFS_SMALLICONS,
                                            cmnGetString(ID_XFSI_SMALLICONS), // pszSmallIcons */
                                            MIS_TEXT,
                                            usIconsAttr);
    }
    else
        winhInsertMenuItem(hwndViewSubmenu,
                           MIT_END,
                           ulOfs + ID_XFMI_OFS_SMALLICONS,
                           cmnGetString(ID_XFSI_SMALLICONS),  // pszSmallIcons
                           MIS_TEXT,
                           usIconsAttr);

    brc = TRUE; // modified flag

    if (ulView == OPEN_CONTENTS)
    {
        // icon view:
        winhInsertMenuSeparator(hwndViewSubmenu, MIT_END,
                    (ulOfs + ID_XFMI_OFS_SEPARATOR));


        // "as placed"
        winhInsertMenuItem(hwndViewSubmenu, MIT_END,
                           ulOfs + ID_XFMI_OFS_NOGRID,
                           cmnGetString(ID_XFSI_NOGRID),  MIS_TEXT,
                           ((pCnrInfo->flWindowAttr & (CV_ICON | CV_TREE)) == CV_ICON)
                                ? MIA_CHECKED
                                : 0);
        // "multiple columns"
        winhInsertMenuItem(hwndViewSubmenu, MIT_END,
                           ulOfs + ID_XFMI_OFS_FLOWED,
                           cmnGetString(ID_XFSI_FLOWED),  MIS_TEXT, // pszFlowed
                           ((pCnrInfo->flWindowAttr & (CV_NAME | CV_FLOW)) == (CV_NAME | CV_FLOW))
                                ? MIA_CHECKED
                                : 0);
        // "single column"
        winhInsertMenuItem(hwndViewSubmenu, MIT_END,
                           ulOfs + ID_XFMI_OFS_NONFLOWED,
                           cmnGetString(ID_XFSI_NONFLOWED),  MIS_TEXT, // pszNonFlowed
                           ((pCnrInfo->flWindowAttr & (CV_NAME | CV_FLOW)) == (CV_NAME))
                                ? MIA_CHECKED
                                : 0);
    }

    // for all views: add separator before menu and status bar items
    // if one of these is enabled
    if (    (G_fIsWarp4)
#ifndef __NOCFGSTATUSBARS__
         || (cmnQuerySetting(sfStatusBars))  // added V0.9.0
#endif
       )
        winhInsertMenuSeparator(hwndViewSubmenu, MIT_END,
                                ulOfs + ID_XFMI_OFS_SEPARATOR);

    // on Warp 4, insert "menu bar" item (V0.9.0)
    if (G_fIsWarp4)
    {
        winhInsertMenuItem(hwndViewSubmenu, MIT_END,
                           ulOfs + ID_XFMI_OFS_WARP4MENUBAR,
                           cmnGetString(ID_XFSI_WARP4MENUBAR),  MIS_TEXT, // pszWarp4MenuBar
                           (_xwpQueryMenuBarVisibility(somSelf))
                               ? MIA_CHECKED
                               : 0);
    }

    // insert "status bar" item if status bar feature
    // is enabled in XWPSetup
#ifndef __NOCFGSTATUSBARS__
    if (cmnQuerySetting(sfStatusBars))
#endif
    {
        BOOL fDefaultVis = cmnQuerySetting(sfDefaultStatusBarVisibility);

        if (!stbClassCanHaveStatusBars(somSelf)) // V0.9.19 (2002-04-17) [umoeller]
            // always disable for Desktop
            ulAttr = MIA_DISABLED;
        else if (ctsIsRootFolder(somSelf))
            // for root folders (WPDisk siblings),
            // check global setting only
            ulAttr = MIA_DISABLED
                        | ( (fDefaultVis)
                            ? MIA_CHECKED
                            : 0);
        else
            // for regular folders, check both instance
            // and global status bar setting
            ulAttr = (    (_bStatusBarInstance == STATUSBAR_ON)
                       || (    (_bStatusBarInstance == STATUSBAR_DEFAULT)
                            && (fDefaultVis)
                          )
                     )
                        ? MIA_CHECKED
                        : 0;

        // insert status bar item with the above attribute
        winhInsertMenuItem(hwndViewSubmenu,
                           MIT_END,
                           ulOfs + ID_XFMI_OFS_SHOWSTATUSBAR,
                           cmnGetString(ID_XFSI_SHOWSTATUSBAR),  // pszShowStatusBar
                           MIS_TEXT,
                           ulAttr);
    }

    return brc;
}

/*
 *@@ BuildConfigItemsList:
 *      this recursive function gets called from
 *      InsertConfigFolderItems initially to build
 *      the config folder cache (the list of objects
 *      in the configuration folder, CONTENTLISTITEM
 *      structures).
 *
 *      Initially, pllContentThis is set to the global
 *      pllConfigContent variable, and pFolderThis points
 *      to the config folder.
 *
 *      When another folder is found in pFolderThis, we
 *      create another list and recurse with that folder
 *      and the new list.
 *
 *      This way of building the config folder menu items
 *      has been introduced with V0.9.0.
 *
 *@@added V0.9.0 [umoeller]
 *@@changed V0.9.14 (2001-08-25) [umoeller]: added XWPString support
 *@@changed V0.9.19 (2002-06-08) [umoeller]: finally treating shadows to program objs like program objs
 */

static BOOL BuildConfigItemsList(PLINKLIST pllContentThis,     // in: CONTENTLISTITEM list to append to
                                 XFolder *pFolderThis)         // in: folder to append from
{
    BOOL        brc = TRUE;
    WPObject    *pObject,
                *pObject2Insert;
    HPOINTER    hptrOld = winhSetWaitPointer();
    PCKERNELGLOBALS pKernelGlobals = krnQueryGlobals();
    BOOL        fStringInstalled = krnIsClassReady(G_pcszXWPString);
                                    // XWPString installed?

    // iterate over the content of *pFolderThis;
    // use the XFolder method which recognizes item order
    ULONG       henum;

    if (henum = _xwpBeginEnumContent(pFolderThis))
    {
        while (pObject = _xwpEnumNext(pFolderThis, henum))
        {
            // if the object is a shadow, we will only de-reference it
            // if it's a template
            // and also now if it's pointing to a program object
            // V0.9.19 (2002-06-08) [umoeller]
            if (    (pObject2Insert = objResolveIfShadow(pObject))
                 && (!(_wpQueryStyle(pObject2Insert) & OBJSTYLE_TEMPLATE))
                 && (!(_somIsA(pObject2Insert, _WPProgram)))
               )
                pObject2Insert = pObject;

            // eliminate broken shadows
            if (pObject2Insert)
            {
                // create list item
                PCONTENTLISTITEM pcli = malloc(sizeof(CONTENTLISTITEM));
                memset(pcli, 0, sizeof(CONTENTLISTITEM));

                pcli->pObject = pObject2Insert;
                strhncpy0(pcli->szTitle,
                          _wpQueryTitle(pObject2Insert),
                          sizeof(pcli->szTitle));

                // now we check on the type of the class that we found and
                // remember the relevant types in the linked object list:
                // -- template: insert name, mark as QC_TEMPLATE
                // -- WPProgram: insert name into menu and mark as QC_PROGRAM;
                // -- WPFolder: insert submenu and recurse this routine;
                // -- others: insert name, mark as QC_OTHER;
                // we mark WPPrograms separately, since we will perform some
                // tricks on them in mnuMenuItemSelected when selected
                if (_wpQueryStyle(pObject2Insert) & OBJSTYLE_TEMPLATE)
                    pcli->ulObjectType = OC_TEMPLATE;
                else if (_somIsA(pObject2Insert, _WPProgram))
                {
                    // program object:
                    // check if it's to be a menu separator
                    if (strncmp(pcli->szTitle, "---", 3))
                        // no: insert as program object
                        pcli->ulObjectType = OC_PROGRAM;
                    else
                        // title == "---": insert separator
                        pcli->ulObjectType = OC_SEPARATOR;
                }
                else if (_somIsA(pObject2Insert, _WPFolder))
                {
                    // folder:
                    pcli->ulObjectType = OC_FOLDER;
                    // create another list for the contents
                    pcli->pllFolderContent = lstCreate(TRUE);

                    // now call ourselves recursively with the new
                    // folder and the new list
                    brc = BuildConfigItemsList(pcli->pllFolderContent,
                                               pcli->pObject);  // the folder
                }
                else if (    (fStringInstalled)   // XWPString installed?
                          && (_somIsA(pObject2Insert, _XWPString))
                        )
                {
                    // V0.9.14 (2001-08-25) [umoeller]
                    pcli->ulObjectType = OC_XWPSTRING;
                }
                else
                    // some other object: mark as OC_OTHER
                    // (will simply be opened)
                    pcli->ulObjectType = OC_OTHER;

                // insert new item in list
                lstAppendItem(pllContentThis,
                              pcli);

                // mark this object as being in the config folder
                _xwpModifyFlags(pObject2Insert,
                                     OBJLIST_CONFIGFOLDER,
                                     OBJLIST_CONFIGFOLDER);
            } // end if (pObject2Insert)
        } // end while

        _xwpEndEnumContent(pFolderThis, henum);
    }

    WinSetPointer(HWND_DESKTOP, hptrOld);

    return brc;
}

/*
 *@@ InsertObjectsFromList:
 *      this recursive function gets called from
 *      InsertConfigFolderItems after the list of
 *      config folder items has been built to
 *      insert its list items into the menu.
 *
 *      Initially, pllContentThis points to the
 *      global variable pllConfigContent (the
 *      contents of the config folder). We recurse
 *      if, on that list, we find another folder
 *      item with a list, and call ourselves again
 *      with that list and a new menu.
 *
 *@@changed V0.9.0 [umoeller]: renamed from mnuFillMenuWithObjects; prototype changed; now running with lists
 */

static LONG InsertObjectsFromList(PLINKLIST  pllContentThis, // in: list to take items from (var.)
                                  HWND       hMenuThis,      // in: menu to add items to (var.)
                                  HWND       hwndCnr,        // in: needed for wpInsertPopupMenuItems (const)
                                  ULONG      ulOfs)          // in: cmnQuerySetting(sulVarMenuOfs)
{
    LONG       lDefaultItem = 0;
    LONG       rc = 0,
               lReturnDefaultItem = 0;

    PLISTNODE  pContentNode = lstQueryFirstNode(pllContentThis);

    while (pContentNode)
    {
        PCONTENTLISTITEM pcli = (PCONTENTLISTITEM)pContentNode->pItemData;

        // now we check on the type of the class that we found and
        // remember the relevant types in the linked object list:
        // -- template: insert name, mark as QC_TEMPLATE
        // -- WPProgram: insert name into menu and mark as QC_PROGRAM;
        // -- WPFolder: insert submenu and recurse this routine;
        // -- others: insert name, mark as QC_OTHER;
        // we mark WPPrograms separately, since we will perform some
        // tricks on them in mnuMenuItemSelected when selected
        switch (pcli->ulObjectType)
        {
            case OC_TEMPLATE:
            case OC_PROGRAM:
            case OC_XWPSTRING:      // V0.9.14 (2001-08-25) [umoeller]
            case OC_OTHER:
                rc = cmnuInsertOneObjectMenuItem(hMenuThis,
                                                 MIT_END,
                                                 pcli->szTitle,
                                                 MIS_TEXT,
                                                 pcli->pObject,
                                                 pcli->ulObjectType);

                if (lReturnDefaultItem == 0)
                    lReturnDefaultItem = rc;
            break;

            case OC_SEPARATOR:
                winhInsertMenuSeparator(hMenuThis,
                                        MIT_END,
                                        ulOfs + ID_XFMI_OFS_SEPARATOR);
            break;

            case OC_FOLDER:
            {
                //  if we find another folder, we add a submenu containing
                //  "[Config folder empty]"), which will be removed if more
                //  objects are found in the respective config folder
                HWND hNewMenu = winhInsertSubmenu(hMenuThis,
                                                  MIT_END,
                                                  G_sNextMenuId,
                                                  pcli->szTitle,
                                                  MIS_TEXT,
                                                  ulOfs + ID_XFMI_OFS_BORED,
                                                  cmnGetString(ID_XSSI_BORED), // (cmnQueryNLSStrings())->pszBored,
                                                  MIS_TEXT,
                                                  0);
                cmnuAppendMi2List(pcli->pObject, OC_FOLDER);

                // recurse with the new list and the new submenu handle
                lDefaultItem = InsertObjectsFromList(pcli->pllFolderContent,
                                                     hNewMenu,
                                                     hwndCnr,
                                                     ulOfs);
                // now we're back: check if error occured; if so, exit
                // immediately to stop recursing
                if (lDefaultItem == -1)
                    return (-1);

                if (lDefaultItem)
                {
                    // items were inserted:
                    // remove static "config folder empty" menu item
                    WinSendMsg(hNewMenu,
                               MM_DELETEITEM,
                               MPFROM2SHORT((ulOfs + ID_XFMI_OFS_BORED),
                                            TRUE),
                               (MPARAM)NULL);

                    if ((cmnQuerySetting(sfMenuCascadeMode)) && (lDefaultItem != 1))
                    {
                        // make the XFolder submenu cascading
                        winhSetMenuCondCascade(hNewMenu,
                                               lDefaultItem);
                    }
                }

                if (lReturnDefaultItem == 0)
                    lReturnDefaultItem = 1;

            }
            break;
        } // end if (pObject2)

        pContentNode = pContentNode->pNext;
    } // end while (pContentNode)
    return (lReturnDefaultItem);
}

/*
 *@@ LockConfigCache:
 *      locks the global config folder content cache.
 *
 *      This has only been added with V0.9.9. Sigh...
 *      I had always thought that mutex protection
 *      for the cache wasn't needed since there was
 *      only ever one menu open in PM. This isn't
 *      quite true... for one, we can't ever trust
 *      PM for such things, secondly, the config
 *      cache gets invalidated behind our back if
 *      an object from the config folders gets
 *      deleted.
 *
 *@@added V0.9.9 (2001-04-04) [umoeller]
 */

static BOOL LockConfigCache(VOID)
{
    if (G_hmtxConfigContent)
        return !DosRequestMutexSem(G_hmtxConfigContent, SEM_INDEFINITE_WAIT);

    // first call:
    if (!DosCreateMutexSem(NULL,
                           &G_hmtxConfigContent,
                           0,
                           TRUE))
    {
        lstInit(&G_llConfigContent, FALSE);
        G_fConfigCacheValid = FALSE;        // rebuild
        return TRUE;
    }

    return FALSE;
}

/*
 *@@ UnlockConfigCache:
 *
 *@@added V0.9.9 (2001-04-04) [umoeller]
 */

static VOID UnlockConfigCache(VOID)
{
    DosReleaseMutexSem(G_hmtxConfigContent);
}

/*
 *@@ mnuInvalidateConfigCache:
 *      this gets called from our override of
 *      XFolder::wpStoreIconPosData when the
 *      .ICONPOS EAs for a config folder have
 *      been rewritten. We need to invalidate
 *      our config folder cache then so that
 *      the lists will be rebuilt.
 *
 *@@added V0.9.0 [umoeller]
 *@@changed V0.9.9 (2001-04-04) [umoeller]: added mutex protection for cache
 */

VOID mnuInvalidateConfigCache(VOID)
{
    #ifdef DEBUG_MENUS
    _Pmpf((__FUNCTION__));
    #endif

    if (LockConfigCache())
    {
        lstClear(&G_llConfigContent);
        G_fConfigCacheValid = FALSE;
            // this will enfore a rebuild in InsertConfigFolderItems

        UnlockConfigCache();
    }
}

/*
 *@@ InsertConfigFolderItems:
 *      this gets called from mnuModifyFolderPopupMenu to insert
 *      the config folder items into a folder's context menu.
 *
 *      When this is called for the first time, a list of
 *      CONTENTLISTITEM structures is built in the global
 *      variable pllConfigContent, which holds the contents
 *      of the config folder for subsequent calls ("config
 *      folder caching").
 *
 *      We then insert menu items according to that list,
 *      which saves us from querying the folders' contents
 *      every time the context menu is opened. This feature
 *      is new with V0.9.0.
 *
 *      That list can be invalidated by calling mnuInvalidateConfigCache,
 *      e.g. because the contents of the config folder have
 *      changed. This is done by several XFolder method overrides.
 *
 *@@added V0.9.0 [umoeller]
 *@@changed V0.9.9 (2001-04-04) [umoeller]: added mutex protection for cache
 *@@changed V0.9.12 (2001-05-22) [umoeller]: added extended close menu
 */

static BOOL InsertConfigFolderItems(XFolder *somSelf,
                                    HWND hwndMenu,
                                    HWND hwndCnr,
                                    ULONG ulOfs)
{
    BOOL brc = FALSE;

    XFolder *pConfigFolder;

    if (!(pConfigFolder = _xwpclsQueryConfigFolder(_XFolder)))
        // config folder not or no longer found:
        mnuInvalidateConfigCache();
    else
    {
        // config folder found:
        if (LockConfigCache()) // V0.9.9 (2001-04-04) [umoeller]
        {
            // have we built a list of objects yet?
            if (!G_fConfigCacheValid)
            {
                // no: create one
                #ifdef DEBUG_MENUS
                _PmpfF(("calling BuildConfigItemsList"));
                #endif

                BuildConfigItemsList(&G_llConfigContent,
                                     pConfigFolder);
                G_fConfigCacheValid = TRUE;
            }

            // do we have any objects?
            if (lstCountItems(&G_llConfigContent))
            {
                // yes:
                // append another separator to the menu first
                winhInsertMenuSeparator(hwndMenu, MIT_END,
                                        ulOfs + ID_XFMI_OFS_SEPARATOR);

                // now insert items in pConfigFolder into main context menu (hwndMenu);
                // this routine will call itself recursively if it finds subfolders.
                // Since we have registered an exception handler, if errors occur,
                // this will lead to a message box only.
                InsertObjectsFromList(&G_llConfigContent,
                                      hwndMenu,
                                      hwndCnr,
                                      ulOfs);
            }

            UnlockConfigCache();        // V0.9.9 (2001-04-04) [umoeller]
        }
    }

    /* else
    {
        // config folder not found: give message and create it anew
        xthrPostFileMsg(FIM_RECREATECONFIGFOLDER,
                       (MPARAM)RCF_QUERYACTION,
                       MPNULL);
        brc = FALSE;
    } */

    return brc;
}

/*
 *@@ mnuModifyFolderPopupMenu:
 *      this is the general menu modifier routine which gets called
 *      by XFolder::wpModifyPopupMenu and XFldDisk::wpModifyPopupMenu.
 *      Since menu items are mostly the same for these two classes,
 *      the shared code has been moved into this function.
 *
 *      Note that when called from XFldDisk, somSelf points to the "root
 *      folder" of the disk object.
 *
 *      First we remove and insert various menu items according to
 *      the Global and instance settings.
 *
 *      We then insert the submenu stubs for folder content menus
 *      by calling mnuPrepareContentSubmenu for the current folder
 *      and all favorite folders.
 *
 *      Finally, we insert the config folder items by calling
 *      InsertConfigFolderItems, which now implements caching for
 *      these items (V0.9.0).
 *
 *@@changed V0.9.0 [umoeller]: adjusted for new linklist functions
 *@@changed V0.9.0 [umoeller]: introduced config folder menu items cache
 *@@changed V0.9.1 (2000-02-01) [umoeller]: "select some" was added for Tree view also; fixed
 *@@changed V0.9.3 (2000-04-10) [umoeller]: snap2grid feature setting was ignored; fixed
 *@@changed V0.9.12 (2001-05-22) [umoeller]: "refresh now" was added even for non-open-view menus
 *@@changed V0.9.14 (2001-08-07) [pr]: added Run menu item
 *@@changed V0.9.21 (2002-08-24) [umoeller]: various changes for split view support
 */

BOOL mnuModifyFolderPopupMenu(WPFolder *somSelf,  // in: folder or root folder
                              HWND hwndMenu,      // in: main context menu hwnd
                              HWND hwndCnr,       // in: cnr hwnd
                              ULONG iPosition)    // in: dunno
{
    XFolder         *pFavorite;
    BOOL            rc = TRUE;
    MENUITEM        mi;

    ULONG           ulVarMenuOfs = cmnQuerySetting(sulVarMenuOfs);

    ULONG           flWPS = cmnQuerySetting(mnuQueryMenuWPSSetting(somSelf)),
                    flXWP = cmnQuerySetting(mnuQueryMenuXWPSetting(somSelf));
    BOOL            fAddFolderContentItem = (!(flXWP & XWPCTXT_FOLDERCONTENTS));

    // protect the following with the quiet exception handler (except.c)
    TRY_QUIET(excpt1)
    {
        // some preparations
        XFolderData *somThis = XFolderGetData(somSelf);
        HWND        hwndFrame = NULLHANDLE;
        CNRINFO     CnrInfo;
        ULONG       ulView = OPEN_UNKNOWN,  // receives OPEN_* flag based on cnrinfo
                    ulRealWPSView = OPEN_UNKNOWN;
                                            // receives OPEN_* flag based on wpshQueryView
                                            // V0.9.21 (2002-08-26) [umoeller]
                                          /*
                                            #define OPEN_UNKNOWN      -1
                                            #define OPEN_DEFAULT       0
                                            #define OPEN_CONTENTS      1
                                            #define OPEN_SETTINGS      2
                                            #define OPEN_HELP          3
                                            #define OPEN_RUNNING       4
                                            #define OPEN_PROMPTDLG     5
                                            #define OPEN_PALETTE       121
                                            #define CLOSED_ICON        122
                                            #define OPEN_USER          0x6500
                                          */
        BOOL        bSepAdded = FALSE;
        BOOL        fOpen;

        if (hwndCnr)
        {
            // get view (OPEN_CONTENTS etc.)
            // V0.9.21 (2002-08-24) [umoeller]: do this
            // from cnrinfo now to make this work with
            // split views
            cnrhQueryCnrInfo(hwndCnr, &CnrInfo);
            if (CnrInfo.flWindowAttr & CV_TREE)
                ulView = OPEN_TREE;
            else if (CnrInfo.flWindowAttr & CV_DETAIL)
                ulView = OPEN_DETAILS;
            else if (CnrInfo.flWindowAttr & (CV_ICON | CV_NAME | CV_TEXT))
                ulView = OPEN_CONTENTS;

            if (hwndFrame = WinQueryWindow(hwndCnr, QW_PARENT))
                // check if this is a registered "real" WPS view;
                // this rules out split views for the "view" menu
                // items below
                ulRealWPSView = wpshQueryView(somSelf, hwndFrame);
                        // V0.9.21 (2002-08-26) [umoeller]
                        // returns OPEN_UNKNOWN if not found
        }

        // store mouse pointer position for creating objects from templates
        WinQueryMsgPos(G_habThread1,
                       &G_ptlMouseMenu);   // V0.9.16 (2001-10-23) [umoeller]

        #ifdef DEBUG_MENUS
            _PmpfF(("[%s] hwndCnr: 0x%lX", _wpQueryTitle(somSelf), hwndCnr));

            _Pmpf(("  ulView is 0x%lX (%s)",
                    ulView,
                    (ulView == OPEN_CONTENTS) ? "OPEN_CONTENTS"
                    : (ulView == OPEN_DETAILS) ? "OPEN_DETAILS"
                    : (ulView == OPEN_TREE) ? "OPEN_TREE"
                    : (ulView == ulVarMenuOfs + ID_XFMI_OFS_SPLITVIEW) ? "ID_XFMI_OFS_SPLITVIEW"
                    : "unknown"));
        #endif

        // in wpFilterPopupMenu, because no codes are provided;
        // we only do this if the Global Settings want it

        /*
         * "Open" submenu:
         *
         */

        // hack in split view V0.9.21 (2002-08-24) [umoeller]
        if (fOpen = winhQueryMenuItem(hwndMenu,
                                      WPMENUID_OPEN,
                                      TRUE,
                                      &mi))
        {
            winhInsertMenuItem(mi.hwndSubMenu,
                               MIT_END,
                               ulVarMenuOfs + ID_XFMI_OFS_SPLITVIEW,
                               cmnGetString(ID_XFSI_FDR_SPLITVIEW),
                               MIS_TEXT, 0);
        }

#ifndef __NOFDRDEFAULTDOCS__
        /*
         * Default document in "Open" submenu:
         *
         */

        if (cmnQuerySetting(sfFdrDefaultDoc))
        {
            WPFileSystem *pDefDoc;
            if (    (pDefDoc = _xwpQueryDefaultDocument(somSelf))
                 && (fOpen) // mi.hwndSubMenu still contains "Open" submenu handle
               )
            {
                // add "Default document"
                CHAR szDefDoc[2*CCHMAXPATH];
                sprintf(szDefDoc,
                        "%s \"%s\"",
                        cmnGetString(ID_XSSI_FDRDEFAULTDOC),
                        _wpQueryTitle(pDefDoc));

                winhInsertMenuSeparator(mi.hwndSubMenu, MIT_END,
                                        ulVarMenuOfs + ID_XFMI_OFS_SEPARATOR);

                winhInsertMenuItem(mi.hwndSubMenu, MIT_END,
                                   ulVarMenuOfs + ID_XFMI_OFS_FDRDEFAULTDOC,
                                   szDefDoc,
                                   MIS_TEXT, 0);
            }
        }
#endif

        /*
         * product info:
         *
         */

#ifndef __XWPLITE__
        if (!(flWPS & CTXT_HELP))
            cmnAddProductInfoMenuItem(somSelf, hwndMenu);
#endif

        // on Warp 4, work on the "View" submenu; do this only
        // if the "View" menu has not been removed entirely
        // (yes, Warp 3 support is broken now)
        if (    (G_fIsWarp4)
             && (!(flWPS & CTXT_VIEW))
           )
        {
            SHORT sPos = MIT_END;

            if (winhQueryMenuItem(hwndMenu,
                                  WPMENUID_VIEW,    // 0x68
                                  TRUE,
                                  &mi))
            {
                // mi.hwndSubMenu now contains "Select"/"View" submenu handle,
                // which we can add items to now

                #ifdef DEBUG_MENUS
                    _Pmpf(("  'View'/'Select' hwnd:0x%X", mi.hwndSubMenu));
                #endif

                // add "Select by name" and "Batch rename" only
                // if not in Tree view V0.9.1 (2000-02-01) [umoeller]
                if (
                        (!(flXWP & (XWPCTXT_SELECTSOME | XWPCTXT_BATCHRENAME)))
                     && (    (ulView == OPEN_CONTENTS)
                          || (ulView == OPEN_DETAILS)
                        )
                   )
                {
                    // get position of "Refresh now";
                    // we'll add behind that item
                    sPos = (SHORT)WinSendMsg(mi.hwndSubMenu,
                                             MM_ITEMPOSITIONFROMID,
                                             MPFROM2SHORT(WPMENUID_REFRESH,
                                                          FALSE),
                                             MPNULL);

                    if (!(flXWP & XWPCTXT_SELECTSOME))
                        winhInsertMenuItem(mi.hwndSubMenu,
                                           sPos++,
                                           ulVarMenuOfs + ID_XFMI_OFS_SELECTSOME,
                                           cmnGetString(ID_XSSI_SELECTSOME),
                                           MIS_TEXT, 0);

                    // add batch rename V0.9.19 (2002-06-18) [umoeller]
                    if (!(flXWP & XWPCTXT_BATCHRENAME))
                        winhInsertMenuItem(mi.hwndSubMenu,
                                           sPos++,
                                           ulVarMenuOfs + ID_XFMI_OFS_BATCHRENAME,
                                           cmnGetString(ID_XSDI_MENU_BATCHRENAME),
                                           MIS_TEXT, 0);

                    // another separator before "Refresh now"
                    winhInsertMenuSeparator(mi.hwndSubMenu,
                                            sPos,
                                            ulVarMenuOfs + ID_XFMI_OFS_SEPARATOR);

                    // if all the "switch view" items are disabled, we
                    // end up with a leading separator on top... the
                    // WPS uses ID 7000 for the separator, so check
                    // if the first menu item with that ID has the index
                    // null now
                    // V0.9.19 (2002-04-17) [umoeller]
                    /*
                    sPos = (SHORT)WinSendMsg(mi.hwndSubMenu,
                                             MM_ITEMIDFROMPOSITION,
                                             (SHORT)0,
                                             MPNULL);
                    _Pmpf(("id of view position 0 is %d", sPos));
                    damn, this doesn't work... if CTXT_CHANGETOICON
                    and the like are set, apparently the wps only
                    removes these items after ModifyPopupMenu
                    */
                }

                // additional "view" items (icon size etc.)
                if (!(flXWP & XWPCTXT_LAYOUTITEMS))
                {
                    // rule out possible user views
                    // of WPFolder subclasses
                    // V0.9.21 (2002-08-26) [umoeller]: now using
                    // ulRealWPSView to rule out split views as well
                    if (    (ulRealWPSView == OPEN_TREE)
                         || (ulRealWPSView == OPEN_CONTENTS)
                         || (ulRealWPSView == OPEN_DETAILS)
                       )
                    {
                        if (G_fIsWarp4)
                        {
                            // for Warp 4, use the existing "View" submenu,
                            // but add an additional separator after
                            // the existing "View"/"Refresh now" item

                            // V0.9.19 (2002-04-17) [umoeller]
                            // add separator only
                            // -- if both "select by name" and "refresh" are visible
                            // -- if "select by name" is invisible, but "refresh" is visible
                            // -- if neither "select by name" nor "refresh" are visible
                            // but not if only "select by name" is visible
                            if (    XWPCTXT_REFRESH_IN_VIEW
                                 != (flXWP & (XWPCTXT_REFRESH_IN_VIEW | XWPCTXT_SELECTSOME))
                               )
                                winhInsertMenuSeparator(mi.hwndSubMenu,
                                                        MIT_END,
                                                        ulVarMenuOfs + ID_XFMI_OFS_SEPARATOR);

                            mnuInsertFldrViewItems(somSelf,
                                                   mi.hwndSubMenu,
                                                   FALSE,       // no submenu
                                                   &CnrInfo,
                                                   ulView);
                        }
                        else
                            // Warp 3:
                            mnuInsertFldrViewItems(somSelf,
                                                   hwndMenu,
                                                   TRUE, // on Warp 3, insert new submenu
                                                   &CnrInfo,
                                                   ulView);
                    }
                }

                // finally, remove some others
                {
                    static const ULONG aSuppressFlags[] =
                        {
                            XWPCTXT_SELALL,
                            XWPCTXT_DESELALL,
                            XWPCTXT_REFRESH_IN_VIEW
                        };
                    mnuRemoveMenuItems(somSelf,
                                       mi.hwndSubMenu,
                                       aSuppressFlags,
                                       ARRAYITEMCOUNT(aSuppressFlags));
                }
            } // end if MM_QUERYITEM;
            // else: "View" menu not found; but this can
            // happen with Warp 4 menu bars, which have
            // a "View" menu separate from the "Folder" menu...
        }

        // work on "Sort" menu; we have put this
        // into a subroutine, because this is also
        // needed for folder menu _bars_

        fdrModifySortMenu(somSelf,
                          hwndMenu);

        if (!(flXWP & XWPCTXT_COPYFILENAME))
        {
            winhInsertMenuSeparator(hwndMenu, MIT_END,
                                    ulVarMenuOfs + ID_XFMI_OFS_SEPARATOR);
            bSepAdded = TRUE; // V0.9.14

            winhInsertMenuItem(hwndMenu, MIT_END,
                               ulVarMenuOfs + ID_XFMI_OFS_COPYFILENAME_MENU,
                               cmnGetString(ID_XSSI_COPYFILENAME), // (pNLSStrings)->pszCopyFilename,
                               0, 0);
        }

        // V0.9.14
/*      if (cmnQuerySetting(sAddRunItem))
        {
            if (!bSepAdded)
            {
                winhInsertMenuSeparator(hwndMenu, MIT_END,
                                        ulVarMenuOfs + ID_XFMI_OFS_SEPARATOR);

                bSepAdded = TRUE;
            }

            winhInsertMenuItem(hwndMenu,
                               MIT_END,
                               ulVarMenuOfs + ID_XFMI_OFS_RUN,
                               cmnGetString(ID_XSSI_RUN),
                               MIS_TEXT,
                               0);
        } */

        // insert the "Refresh now" and "Snap to grid" items only
        // if the folder is currently open
        if (ulView != -1)           // fixed V0.9.12 (2001-05-22) [umoeller]
        {
            if (!bSepAdded) // V0.9.14
                winhInsertMenuSeparator(hwndMenu,
                                        MIT_END,
                                        ulVarMenuOfs + ID_XFMI_OFS_SEPARATOR);
            bSepAdded = TRUE;

#ifndef __NOMOVEREFRESHNOW__
            // "Refresh now" in main menu
            if (!(flXWP & XWPCTXT_REFRESH_IN_MAIN))
                winhInsertMenuItem(hwndMenu,
                                   MIT_END,
                                   ulVarMenuOfs + ID_XFMI_OFS_REFRESH,
                                   cmnGetString(ID_XSSI_REFRESHNOW),  // pszRefreshNow
                                   MIS_TEXT,
                                   0);
#endif

            // "Snap to grid" feature enabled? V0.9.3 (2000-04-10) [umoeller]
#ifndef __NOSNAPTOGRID__
            if (    (cmnQuerySetting(sfSnap2Grid))
                 // "Snap to grid" enabled locally or globally?
                 && (    (_bSnapToGridInstance == 1)
                      || (   (_bSnapToGridInstance == 2)
                          && (cmnQuerySetting(sfAddSnapToGridDefault))
                         )
                    )
                 // insert only when sorting is off
                 && (!fdrHasAlwaysSort(somSelf))
                 // and only in icon view
                 && (ulView == OPEN_CONTENTS)
                )
            {
                // insert "Snap to grid" only for open icon views
                winhInsertMenuItem(hwndMenu,
                                   MIT_END,
                                   ulVarMenuOfs + ID_XFMI_OFS_SNAPTOGRID,
                                   cmnGetString(ID_XSSI_SNAPTOGRID),  // pszSnapToGrid
                                   MIS_TEXT,
                                   0);
            }
#endif
        } // end if view open

        // now do necessary preparations for all variable menu items
        // (i.e. folder content and config folder items)
        cmnuInitItemCache(); // pGlobalSettings);

        /*
         * folder content / favorite folders:
         *
         */

#ifndef __NOFOLDERCONTENTS__
        // get first favorite folder; we will only work on
        // this if either folder content for every folder is
        // enabled or at least one favorite folder exists
        pFavorite = _xwpclsQueryFavoriteFolder(_XFolder, NULL);
        if (    (!cmnQuerySetting(sfNoSubclassing))
             && (    (fAddFolderContentItem)
                  || (pFavorite)
                )
           )
        {
            winhInsertMenuSeparator(hwndMenu,
                                    MIT_END,
                                    ulVarMenuOfs + ID_XFMI_OFS_SEPARATOR);

            if (cmnQuerySetting(sfFolderContentShowIcons))
            {
                // before actually inserting the content submenus, we need a real
                // awful cheat, because otherwise the owner draw items won't work
                // right (I don't know why the hell they are not being sent
                // WM_MEASUREITEM msgs if we don't do this); so we insert a
                // content submenu and remove it again right away
                WinSendMsg(hwndMenu,
                           MM_REMOVEITEM,
                           MPFROM2SHORT(cmnuPrepareContentSubmenu(somSelf, hwndMenu,
                                                                  cmnGetString(ID_XSSI_FLDRCONTENT),  // pszFldrContent
                                                                  MIT_END,
                                                                  FALSE), // no owner draw
                                        FALSE),
                           MPNULL);
            }

            if ((fAddFolderContentItem))
            {
                // add "Folder content" only if somSelf is not a favorite folder,
                // because then we will insert the folder content anyway
                if (!_xwpIsFavoriteFolder(somSelf))
                    // somself is not in favorites list: add "Folder content"
                    cmnuPrepareContentSubmenu(somSelf,
                                              hwndMenu,
                                              cmnGetString(ID_XSSI_FLDRCONTENT),
                                              MIT_END,
                                              FALSE); // no owner draw in main context menu
            }

            // now add favorite folders
            pFavorite = NULL;
            while (pFavorite = _xwpclsQueryFavoriteFolder(_XFolder,
                                                          pFavorite))
            {
                cmnuPrepareContentSubmenu(pFavorite,
                                          hwndMenu,
                                          _wpQueryTitle(pFavorite),
                                          MIT_END,
                                          FALSE); // no owner draw in main context menu
            }
        } // end folder contents
#endif

        /*
         * config folders:
         *
         */

        InsertConfigFolderItems(somSelf,
                                hwndMenu,
                                hwndCnr,
                                ulVarMenuOfs);

        // V0.9.19 (2002-04-17) [umoeller]
        // note that we also override XFolder::wpDisplayMenu
        // to get rid of some additional menu items that can't
        // be removed otherwise

    }
    CATCH(excpt1)
    {
        // exception caught:
        PSZ     pszErrMsg = malloc(1000);
        if (pszErrMsg)
        {
            strcpy(pszErrMsg, "An error occured while XFolder was trying to build "
                    "a folder's context menu. This might be due to the fact "
                    "that you have deleted objects from the Configuration folders, "
                    "but you did "
                    "not have these folders opened in the Icon or Details views "
                    "while doing so. "
                    "You should open and close the configuration folder and all "
                    "of its subfolders once. Make sure that all the folders are either "
                    "in Icon or Details view per default.");
            krnPostThread1ObjectMsg(T1M_EXCEPTIONCAUGHT, (MPARAM)pszErrMsg, MPNULL);
            mnuInvalidateConfigCache();
        }
    } END_CATCH();

    return (rc);
}

/*
 *@@ mnuHackFolderClose:
 *      this gets closed from XFolder::wpDisplayMenu
 *      only if "extend close menu" is enabled.
 *
 *      We kick out the standard "close" menu item
 *      and replace it with a submenu. See
 *      XFolder::wpDisplayMenu for remarks why we
 *      can't do it in mnuModifyFolderPopupMenu.
 *
 *@@added V0.9.12 (2001-05-22) [umoeller]
 */

/* nah, this doesn't work. Try again later. */

/* BOOL mnuHackFolderClose(WPFolder *somSelf,      // in: folder
                        HWND hwndOwner,         // in: from wpDisplayMenu
                        HWND hwndClient,        // in: from wpDisplayMenu
                        PPOINTL pptlPopupPt,    // in: from wpDisplayMenu
                        ULONG ulMenuType,       // in: from wpDisplayMenu
                        HWND hwndMenu)          // in: main context menu created by parent_wpDisplayMenu
{
    BOOL brc = FALSE;

    ULONG           ulVarMenuOfs = cmnQuerySetting(sulVarMenuOfs);

    HWND hNewMenu;

    // for some reason, WPS is no longer using
    // WPMENUID_CLOSE but SC_CLOSE directly (0x8004)
    winhDeleteMenuItem(hwndMenu,
                       SC_CLOSE);

    hNewMenu = winhInsertSubmenu(hwndMenu,
                                 MIT_END,
                                 ulVarMenuOfs + ID_XFMI_OFS_CLOSESUBMENU,
                                 cmnGetString(ID_XSSI_CLOSE),   // "close"
                                 MIS_TEXT,
                                 SC_CLOSE,
                                 cmnGetString(ID_XSSI_CLOSETHISVIEW), // "close this view
                                 MIS_SYSCOMMAND | MIS_TEXT,
                                        // note, MIS_SYSCOMMAND
                                 0);

    winhInsertMenuSeparator(hNewMenu,
                            MIT_END,
                            ulVarMenuOfs + ID_XFMI_OFS_SEPARATOR);

    winhInsertMenuItem(hNewMenu,
                       MIT_END,
                       ulVarMenuOfs + ID_XFMI_OFS_CLOSEALLTHISFDR,
                       cmnGetString(ID_XSSI_CLOSEALLTHISFDR), // "close all of this fdr"
                       0,
                       0);
    winhInsertMenuItem(hNewMenu,
                       MIT_END,
                       ulVarMenuOfs + ID_XFMI_OFS_CLOSEALLSUBFDRS,
                       cmnGetString(ID_XSSI_CLOSEALLSUBFDRS), // "close all subfdrs"
                       0,
                       0);

    // set default menu item
    winhSetMenuCondCascade(hNewMenu,
                           SC_CLOSE);


    if (0 == (ulMenuType & MENU_NODISPLAY))
    {
        // we were supposed to show the menu:
        // show it then
        POINTL ptl;
        HWND hwndConvertSource;
        memcpy(&ptl, pptlPopupPt, sizeof(POINTL));
        if (hwndClient)
            hwndConvertSource = hwndClient;
        else
            hwndConvertSource = hwndOwner;

        WinMapWindowPoints(hwndConvertSource,
                           HWND_DESKTOP,
                           &ptl,
                           1);
        WinSetFocus(HWND_DESKTOP, hwndConvertSource);

        // alright, display the menu
        WinPopupMenu(HWND_DESKTOP,
                     hwndConvertSource,
                     hwndMenu,
                     ptl.x,
                     ptl.y,
                     0,
                     PU_HCONSTRAIN | PU_VCONSTRAIN | PU_MOUSEBUTTON1
                        | PU_MOUSEBUTTON2 | PU_KEYBOARD);
    }

    return TRUE;
} */

/*
 *@@ mnuModifyDataFilePopupMenu:
 *      implementation for XFldDataFile::wpModifyPopupMenu.
 *
 *      This does stuff like "Copy filename", "Attributes"
 *      etc.
 *
 *@@added V0.9.0 [umoeller]
 *@@changed V0.9.4 (2000-06-09) [umoeller]: added default documents
 *@@changed V0.9.4 (2000-06-09) [umoeller]: fixed separators
 */

BOOL mnuModifyDataFilePopupMenu(WPObject *somSelf,  // in: data file
                                HWND hwndMenu,
                                HWND hwndCnr,
                                ULONG iPosition)
{
    ULONG           ulVarMenuOfs = cmnQuerySetting(sulVarMenuOfs);

    /* if (cmnQuerySetting(sfExtAssocs))
    {
        // THIS HAS BEEN REMOVED FROM HERE;
        // XFldDataFile now handles extended file associations

        /// this doesn't work. VERY FUNNY, IBM. Look what
        // I got when I enumerated the submenu items in the "Open"
        // submenu IN THIS FUNCTION (WPDataFile).

        mnuModifyDataFilePopupMenu: removing 0x70 (Propertie~s)
        mnuModifyDataFilePopupMenu: removing 0x12F (~Icon view)
        mnuModifyDataFilePopupMenu: removing 0x7B (~Tree view)
        mnuModifyDataFilePopupMenu: removing 0x130 (~Details view)
        mnuModifyDataFilePopupMenu: removing 0x84 (~Program)
        mnuModifyDataFilePopupMenu: removing 0x13D (~Palette)
        mnuModifyDataFilePopupMenu: removing 0x67B (Date/~Time)

        // Now you guys explain why the **DATAFILE** "Open" menu
        // initially has "Palette" and "Tree view" menu items, and
        // where this is removed. COME ON.

        // get handle to "Open" submenu

        MENUITEM        mi;
        if (WinSendMsg(hwndMenu,
                       MM_QUERYITEM,
                       MPFROM2SHORT(WPMENUID_OPEN, TRUE),
                       (MPARAM)&mi))
        {
            // found:
            // find first item
            ULONG       ulItemID = 0;

            do
            {
                ulItemID = (ULONG)WinSendMsg(mi.hwndSubMenu,
                                             MM_ITEMIDFROMPOSITION,
                                             0,       // first item
                                             0);      // reserved
                if ((ulItemID) && (ulItemID != MIT_ERROR))
                {
                    PSZ pszItemText = winhQueryMenuItemText(mi.hwndSubMenu, ulItemID);
                    _Pmpf(("mnuModifyDataFilePopupMenu: removing 0x%lX (%s)",
                            ulItemID,
                            pszItemText));
                    free(pszItemText);

                    winhDeleteMenuItem(mi.hwndSubMenu, ulItemID);
                }
                else
                    break;

            } while (TRUE);
        }
    } */

    ULONG fl = cmnQuerySetting(mnuQueryMenuXWPSetting(somSelf));
    BOOL fFileAttribs = (!(fl & XWPCTXT_ATTRIBUTESMENU));
    BOOL fAddCopyFilenameItem = (!(fl & XWPCTXT_COPYFILENAME));

    // insert separator V0.9.4 (2000-06-09) [umoeller]
    if (
            fFileAttribs
         || fAddCopyFilenameItem
#ifndef __NOFDRDEFAULTDOCS__
         || (cmnQuerySetting(sfFdrDefaultDoc))
#endif
       )
        winhInsertMenuSeparator(hwndMenu,
                                MIT_END,
                                ulVarMenuOfs + ID_XFMI_OFS_SEPARATOR);

    // insert "Attributes" submenu (for data files
    // only, not for folders
    if (fFileAttribs)
    {
        ULONG ulAttr;
        HWND hwndAttrSubmenu;

        // get this file's file-system attributes
        ulAttr = _wpQueryAttr(somSelf);
        // insert submenu
        hwndAttrSubmenu = winhInsertSubmenu(hwndMenu,
                                            MIT_END,
                                            ulVarMenuOfs + ID_XFM_OFS_ATTRIBUTES,
                                            cmnGetString(ID_XFSI_ATTRIBUTES),
                                            0, // pszAttributes
        // "archived" item, checked or not according to file-system attributes
                                            ulVarMenuOfs + ID_XFMI_OFS_ATTR_ARCHIVED,
                                            cmnGetString(ID_XFSI_ATTR_ARCHIVE),
                                            MIS_TEXT,
                                            ((ulAttr & FILE_ARCHIVED) ? MIA_CHECKED : 0));
        // "read-only" item, checked or not according to file-system attributes
        winhInsertMenuItem(hwndAttrSubmenu,
                           MIT_END,
                           ulVarMenuOfs + ID_XFMI_OFS_ATTR_READONLY,
                           cmnGetString(ID_XFSI_ATTR_READONLY),
                           MIS_TEXT, // pszAttrReadOnly
                           ((ulAttr & FILE_READONLY) ? MIA_CHECKED : 0));
        // "system" item, checked or not according to file-system attributes
        winhInsertMenuItem(hwndAttrSubmenu,
                           MIT_END,
                           ulVarMenuOfs + ID_XFMI_OFS_ATTR_SYSTEM,
                           cmnGetString(ID_XFSI_ATTR_SYSTEM),
                           MIS_TEXT, // pszAttrSystem
                           ((ulAttr & FILE_SYSTEM) ? MIA_CHECKED : 0));
        // "hidden" item, checked or not according to file-system attributes
        winhInsertMenuItem(hwndAttrSubmenu,
                           MIT_END,
                           ulVarMenuOfs + ID_XFMI_OFS_ATTR_HIDDEN,
                           cmnGetString(ID_XFSI_ATTR_HIDDEN),
                           MIS_TEXT, // pszAttrHidden
                           ((ulAttr & FILE_HIDDEN) ? MIA_CHECKED : 0));
    }

    // insert "Copy filename" for data files
    // (the XFolder class does this also)
    if (fAddCopyFilenameItem)
        winhInsertMenuItem(hwndMenu,
                           MIT_END,
                           ulVarMenuOfs + ID_XFMI_OFS_COPYFILENAME_MENU,
                           cmnGetString(ID_XSSI_COPYFILENAME), // (pNLSStrings)->pszCopyFilename,
                           0,
                           0);

    // insert "Default document" if enabled
#ifndef __NOFDRDEFAULTDOCS__
    if (cmnQuerySetting(sfFdrDefaultDoc))
    {
        ULONG flAttr = 0;
        if (_xwpQueryDefaultDocument(_wpQueryFolder(somSelf)) == somSelf)
            // somSelf is default document of its folder:
            flAttr = MIA_CHECKED;

        winhInsertMenuItem(hwndMenu,
                           MIT_END,
                           ulVarMenuOfs + ID_XFMI_OFS_FDRDEFAULTDOC,
                           cmnGetString(ID_XSSI_DATAFILEDEFAULTDOC), // (pNLSStrings)->pszDataFileDefaultDoc,
                           MIS_TEXT,
                           flAttr);
    }
#endif

    return TRUE;
}

/* ******************************************************************
 *
 *   Notebook callbacks (notebook.c) for XFldWPS "Menu 1" page
 *
 ********************************************************************/

static const CONTROLDEF
    MenuStyleGroup = LOADDEF_GROUP(ID_XSDI_MENU_STYLE_GROUP, SZL_AUTOSIZE),
    LongRadio = LOADDEF_FIRST_AUTORADIO(ID_XSDI_MENUS_LONG),
    ShortRadio = LOADDEF_NEXT_AUTORADIO(ID_XSDI_MENUS_SHORT),
    BarsCB = LOADDEF_AUTOCHECKBOX(ID_XSDI_MENUS_BARS),
#ifndef __NOFOLDERCONTENTS__
    FCShowIconsCB = LOADDEF_AUTOCHECKBOX(ID_XSDI_FC_SHOWICONS),
#endif
    LockInPlaceNoSubCB = LOADDEF_AUTOCHECKBOX(ID_XSDI_LOCKINPLACE_NOSUB);

static const DLGHITEM G_dlgMenuSettings[] =
    {
        START_TABLE,
            START_ROW(0),
                START_GROUP_TABLE(&MenuStyleGroup),
                    START_ROW(0),
                        CONTROL_DEF(&LongRadio),
                    START_ROW(0),
                        CONTROL_DEF(&ShortRadio),
                END_TABLE,
            START_ROW(0),
                CONTROL_DEF(&BarsCB),
#ifndef __NOFOLDERCONTENTS__
            START_ROW(0),
                CONTROL_DEF(&FCShowIconsCB),
#endif
            START_ROW(0),
                CONTROL_DEF(&LockInPlaceNoSubCB),
            START_ROW(0),
                CONTROL_DEF(&G_UndoButton),         // common.c
                CONTROL_DEF(&G_DefaultButton),      // common.c
                CONTROL_DEF(&G_HelpButton),         // common.c
        END_TABLE
    };

static const XWPSETTING G_MenuSettingsBackup[] =
    {
        sfFixLockInPlace
#ifndef __NOFOLDERCONTENTS__
        , sfFolderContentShowIcons
#endif
    };

typedef struct _MOREBACKUP
{
    BOOL        fShortMenus;
    BOOL        fFolderBars;
} COMMONMOREBACKUP, *PCOMMONMOREBACKUP;

/*
 *@@ mnuSettingsInitPage:
 *      notebook callback function (notebook.c) for the
 *      first "Menu" page in the "Workplace Shell" object
 *      ("Common menu settings").
 *      Sets the controls on the page according to the
 *      Global Settings.
 *
 *@@added V0.9.19 (2002-04-17) [umoeller]
 */

static VOID mnuSettingsInitPage(PNOTEBOOKPAGE pnbp,   // notebook info struct
                                ULONG flFlags)        // CBI_* flags (notebook.h)
{
    if (flFlags & CBI_INIT)
    {
        HWND hwndCnr, hwndDrop;
        ULONG ul;

        // first call: backup Global Settings for "Undo" button;
        // this memory will be freed automatically by the
        // common notebook window function (notebook.c) when
        // the notebook page is destroyed
        pnbp->pUser = cmnBackupSettings(G_MenuSettingsBackup,
                                        ARRAYITEMCOUNT(G_MenuSettingsBackup));

        if (pnbp->pUser2 = NEW(COMMONMOREBACKUP))
        {
            PCOMMONMOREBACKUP p2 = (PCOMMONMOREBACKUP)pnbp->pUser2;
            p2->fShortMenus = mnuQueryShortMenuStyle();
            p2->fFolderBars = mnuQueryDefaultMenuBarVisibility();
        }

        // insert the controls using the dialog formatter
        ntbFormatPage(pnbp->hwndDlgPage,
                      G_dlgMenuSettings,
                      ARRAYITEMCOUNT(G_dlgMenuSettings));
    }

    if (flFlags & CBI_SET)
    {
        ULONG ulRadio = ID_XSDI_MENUS_LONG;
        if (mnuQueryShortMenuStyle())
            ulRadio = ID_XSDI_MENUS_SHORT;
        winhSetDlgItemChecked(pnbp->hwndDlgPage, ulRadio, TRUE);

        winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_XSDI_MENUS_BARS,
                              mnuQueryDefaultMenuBarVisibility());

        winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_XSDI_LOCKINPLACE_NOSUB,
                              cmnQuerySetting(sfFixLockInPlace));

#ifndef __NOFOLDERCONTENTS__
        winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_XSDI_FC_SHOWICONS,
                              cmnQuerySetting(sfFolderContentShowIcons));
#endif
    }
}

/*
 *@@ mnuSettingsItemChanged:
 *      notebook callback function (notebook.c) for the
 *      first "Menu" page in the "Workplace Shell" object
 *      ("Common menu settings").
 *      Reacts to changes of any of the dialog controls.
 *
 *@@added V0.9.19 (2002-04-17) [umoeller]
 */

static MRESULT mnuSettingsItemChanged(PNOTEBOOKPAGE pnbp,
                                      ULONG ulItemID,
                                      USHORT usNotifyCode,
                                      ULONG ulExtra)
{
    MRESULT mrc = 0;

    switch (ulItemID)
    {
        case ID_XSDI_MENUS_SHORT:
        case ID_XSDI_MENUS_LONG:
            mnuSetShortMenuStyle((ulItemID == ID_XSDI_MENUS_SHORT));
            // update the display by calling the INIT callback
            pnbp->inbp.pfncbInitPage(pnbp, CBI_ENABLE);
            // and update menu items page too, this needs a refresh then
            ntbUpdateVisiblePage(NULL, SP_MENUITEMS);
        break;

        case ID_XSDI_MENUS_BARS:
            mnuSetDefaultMenuBarVisibility(ulExtra);
        break;

#ifndef __NOFOLDERCONTENTS__
        case ID_XSDI_FC_SHOWICONS:
            cmnSetSetting(sfFolderContentShowIcons, ulExtra);
        break;
#endif

        case ID_XSDI_LOCKINPLACE_NOSUB:  // V0.9.7 (2000-12-10) [umoeller]
            cmnSetSetting(sfFixLockInPlace, ulExtra);
        break;

        case DID_UNDO:
        {
            PCOMMONMOREBACKUP p2;
            if (p2 = (PCOMMONMOREBACKUP)pnbp->pUser2)
            {
                mnuSetShortMenuStyle(p2->fShortMenus);
                mnuSetDefaultMenuBarVisibility(p2->fFolderBars);
            }

            cmnRestoreSettings(pnbp->pUser,
                               ARRAYITEMCOUNT(G_MenuSettingsBackup));
            // update the display by calling the INIT callback
            pnbp->inbp.pfncbInitPage(pnbp, CBI_SET | CBI_ENABLE);
        }
        break;

        case DID_DEFAULT:
            // set defaults:
            mnuSetShortMenuStyle(FALSE);
            mnuSetDefaultMenuBarVisibility(TRUE);
            cmnSetDefaultSettings(pnbp->inbp.ulPageID);
            // update the display by calling the INIT callback
            pnbp->inbp.pfncbInitPage(pnbp, CBI_SET | CBI_ENABLE);
        break;

    }

    return mrc;
}

/* ******************************************************************
 *
 *   Notebook callbacks (notebook.c) for XFldWPS "Menu 2" page
 *
 ********************************************************************/

#define CNR_WIDTH               (DEFAULT_TABLE_WIDTH - 2 * COMMON_SPACING)

static const CONTROLDEF
    EditCategoryTxt = CONTROLDEF_TEXT(LOAD_STRING, ID_XSDI_MENU_EDIT_CAT_TXT, -1, -1),
    EditCategoryDrop = CONTROLDEF_DROPDOWNLIST(ID_XSDI_MENU_EDIT_CAT_DROP, 80, 64),
    EditGroup = LOADDEF_GROUP(ID_XSDI_MENU_EDIT_GROUP, DEFAULT_TABLE_WIDTH),
    EditCnr = CONTROLDEF_CONTAINER(ID_XSDI_MENU_EDIT_CNR, CNR_WIDTH, 40);

static const DLGHITEM G_dlgMenuItems[] =
    {
        START_TABLE,
            START_ROW(ROW_VALIGN_CENTER),
                CONTROL_DEF(&EditCategoryTxt),
                CONTROL_DEF(&EditCategoryDrop),
            START_ROW(0),
                START_GROUP_TABLE(&EditGroup),
                    START_ROW(0),
                        CONTROL_DEF(&EditCnr),
                END_TABLE,
            START_ROW(0),
                CONTROL_DEF(&G_UndoButton),         // common.c
                CONTROL_DEF(&G_DefaultButton),      // common.c
                CONTROL_DEF(&G_HelpButton),         // common.c
        END_TABLE
    };

static MPARAM G_ampFile1Page[] =
    {
        MPFROM2SHORT(ID_XSDI_MENU_EDIT_CAT_TXT, XAC_MOVEY),
        MPFROM2SHORT(ID_XSDI_MENU_EDIT_CAT_DROP, XAC_MOVEY),
        MPFROM2SHORT(ID_XSDI_MENU_EDIT_GROUP, XAC_SIZEY),
        MPFROM2SHORT(ID_XSDI_MENU_EDIT_CNR, XAC_SIZEY),
    };

/*
 *@@ CATEGORYWITHFLAG:
 *      array item correlating the available
 *      object categories for menu items with
 *      the flags from G_MenuItemsWithIDs and
 *      the XWPSETTING's that are used for
 *      each category.
 *
 *@@added V0.9.19 (2002-04-17) [umoeller]
 */

typedef struct _CATEGORYWITHFLAG
{
    ULONG       ulString;       // id for cmnGetString
    ULONG       fl;             // CONFFL_* flag
    XWPSETTING  sWPS;           // XWPSETTING id for WPS CTXT_* flags
    XWPSETTING  sXWP;           // XWPSETTING id for XWorkplace XWPCTXT_* flags
} CATEGORYWITHFLAG, *PCATEGORYWITHFLAG;

/*
 *@@ G_CategoriesWithFlags:
 *
 *@@added V0.9.19 (2002-04-17) [umoeller]
 */

static const CATEGORYWITHFLAG G_CategoriesWithFlags[] =
    {
        // "Folders"
        ID_XSDI_MENU_EDIT_CAT_FOLDERS, CONFFL_WPFOLDER,
                sflMenuFolderWPS, sflMenuFolderXWP,
        // "Desktop"
        ID_XSDI_MENU_EDIT_CAT_DESKTOP, CONFFL_WPFOLDER | CONFFL_WPDESKTOP,
                sflMenuDesktopWPS, sflMenuDesktopXWP,
        // "Disks"
        ID_XSDI_MENU_EDIT_CAT_DISKS, CONFFL_WPDISK,
                sflMenuDiskWPS, sflMenuDiskXWP,
        // "Files"
        ID_XSDI_MENU_EDIT_CAT_FILES, CONFFL_WPDATAFILE,
                sflMenuFileWPS, sflMenuFileXWP,
        // "All other objects"
        ID_XSDI_MENU_EDIT_CAT_OBJECTS, CONFFL_WPOBJECT,
                sflMenuObjectWPS, sflMenuObjectXWP,
    };

/*
 *@@ MENUITEMRECORD:
 *      extended CHECKBOXRECORDCORE for one menu
 *      item on "Menu" page 2.
 *
 *@@added V0.9.19 (2002-04-24) [umoeller]
 */

typedef struct _MENUITEMRECORD
{
    CHECKBOXRECORDCORE      recc;
    CHAR                    szTitle[100];
    const MENUITEMDEF       *pItem;
} MENUITEMRECORD, *PMENUITEMRECORD;

static const XWPSETTING G_MenuItemsBackup[] =
    {
        sflMenuObjectWPS,
        sflMenuObjectXWP,
        sflMenuFileWPS,
        sflMenuFileXWP,
        sflMenuFolderWPS,
        sflMenuFolderXWP,
        sflMenuDesktopWPS,
        sflMenuDesktopXWP,
        sflMenuDiskWPS,
        sflMenuDiskXWP
    };

/*
 *@@ mnuItemsInitPage:
 *      notebook callback function (notebook.c) for the
 *      second "Menu" page in the "Workplace Shell" object
 *      ("Menu items").
 *      Sets the controls on the page according to the
 *      Global Settings.
 *
 *      This is all new with V0.9.19 and finally cleans
 *      up the major menu pages mess that existed in XWP
 *      before, and adds a few new features as well.
 *
 *@@added V0.9.19 (2002-04-17) [umoeller]
 */

static VOID mnuItemsInitPage(PNOTEBOOKPAGE pnbp,   // notebook info struct
                             ULONG flFlags)        // CBI_* flags (notebook.h)
{
    if (flFlags & CBI_INIT)
    {
        HWND hwndCnr, hwndDrop;
        ULONG ul;

        // first call: backup Global Settings for "Undo" button;
        // this memory will be freed automatically by the
        // common notebook window function (notebook.c) when
        // the notebook page is destroyed
        pnbp->pUser = cmnBackupSettings(G_MenuItemsBackup,
                                        ARRAYITEMCOUNT(G_MenuItemsBackup));

        // insert the controls using the dialog formatter
        ntbFormatPage(pnbp->hwndDlgPage,
                      G_dlgMenuItems,
                      ARRAYITEMCOUNT(G_dlgMenuItems));

        WinSetWindowBits(pnbp->hwndDlgPage,
                         QWL_STYLE,
                         0,
                         WS_CLIPSIBLINGS);

        hwndCnr = WinWindowFromID(pnbp->hwndDlgPage, ID_XSDI_MENU_EDIT_CNR);

        // put the cnr on top or the TOTALLY FREAKING
        // BRAIN-DEAD COMBOBOX will cut it off
        WinSetWindowPos(hwndCnr, HWND_TOP, 0, 0, 0, 0, SWP_ZORDER);

        ctlMakeCheckboxContainer(pnbp->hwndDlgPage,
                                 ID_XSDI_MENU_EDIT_CNR);
                // this switches to tree view etc.

        hwndDrop = WinWindowFromID(pnbp->hwndDlgPage, ID_XSDI_MENU_EDIT_CAT_DROP);

        // fill the drop-down with the categories
        for (ul = 0;
             ul < ARRAYITEMCOUNT(G_CategoriesWithFlags);
             ++ul)
        {
            LONG lIndex = WinInsertLboxItem(hwndDrop,
                                            LIT_END,
                                            cmnGetString(G_CategoriesWithFlags[ul].ulString));
            // set the address of the CATEGORYWITHFLAG as the handle
            // so we can retrieve it elsewhere
            winhSetLboxItemHandle(hwndDrop,
                                  lIndex,
                                  &G_CategoriesWithFlags[ul]);
        }

        // select "all objects"
        winhSetLboxSelectedItem(hwndDrop, 0, TRUE);
    }

    if (flFlags & CBI_SET)
    {
        HWND    hwndCnr = WinWindowFromID(pnbp->hwndDlgPage, ID_XSDI_MENU_EDIT_CNR);
        ULONG   ul;
        BOOL    fShortMenus = mnuQueryShortMenuStyle();

        // get index of currently selected category
        HWND    hwndDrop = WinWindowFromID(pnbp->hwndDlgPage, ID_XSDI_MENU_EDIT_CAT_DROP);
        LONG    lSelected = winhQueryLboxSelectedItem(hwndDrop, LIT_FIRST);
        // category pointer was set for each combo box item with CBI_INIT
        PCATEGORYWITHFLAG pCategory = (PCATEGORYWITHFLAG)winhQueryLboxItemHandle(hwndDrop, lSelected);

        PMENUITEMRECORD preccNew,
                        preccParent = NULL;
        BOOL            fDisable = FALSE;

        // clear container, this gets called every time the
        // category dropdown changes
        cnrhRemoveAll(hwndCnr);

        // run thru all available menu IDs and insert items
        // only if they match the current category
        for (ul = 0;
             ul < ARRAYITEMCOUNT(G_MenuItemsWithIDs);
             ++ul)
        {
            ULONG flConfig = G_MenuItemsWithIDs[ul].flConfig;

            if (
                    // 1) is this menu item supported by the category?
                    (    (!(flConfig & CONFFL_CLASSMASK))        // item is for all objects
                      || ((flConfig & CONFFL_CLASSMASK) & pCategory->fl)
                    )
                    // 2) is this menu item not filtered out for the subclass?
                    // CONFFL_WPDESKTOP            0x00000004
                    // CONFFL_FILTERED_WPDESKTOP   0x00000400
                 && (!(   ((flConfig & CONFFL_FILTEREDMASK) >> CONFFL_FILTEREDSHIFT)
                        & pCategory->fl))
                    // skip entry if item is not configurable
                 && (!(flConfig & CONFFL_CANNOTREMOVE))
                    // skip entry if short menus are on and flag is set
                 && ((!fShortMenus) || (!(flConfig & CONFFL_NOTINSHORTMENUS)))
                    // skip separators for now
                 && (G_MenuItemsWithIDs[ul].ulString)
                 && (preccNew = (PMENUITEMRECORD)cnrhAllocRecords(hwndCnr,
                                                             sizeof(MENUITEMRECORD),
                                                             1))
               )
            {
                // now get the corresponding bit from the XWPSETTING
                // that matches the current category and check
                // the record checkbox accordingly
                ULONG flStyle = CRA_COLLAPSED | CRA_RECORDREADONLY;
                PSZ p;

                /*      we could disable static menu items,
                        but why show them all (see above)
                if (flConfig & CONFFL_CANNOTREMOVE)
                {
                    preccNew->recc.ulStyle = WS_VISIBLE;
                    preccNew->recc.usCheckState = 1;
                    flStyle |= CRA_DISABLED;
                }
                else
                */
                {
                    XWPSETTING s;
                    ULONG flFilter = G_MenuItemsWithIDs[ul].flFilter;
                    preccNew->recc.ulStyle = WS_VISIBLE | BS_AUTOCHECKBOX;

                    if (flFilter & XWPCTXT_HIGHBIT)
                        // use XWP setting (XWPCTXT_* flag):
                        s = pCategory->sXWP;
                    else
                        // use WPS setting (CTXT_* flag):
                        s = pCategory->sWPS;

                    // clear the highbit
                    flFilter &= ~XWPCTXT_HIGHBIT;
                    // check record if setting's bit is clear
                    preccNew->recc.usCheckState =
                        (0 == (cmnQuerySetting(s) & flFilter));
                }

                preccNew->recc.ulItemID = (ULONG)&G_MenuItemsWithIDs[ul];

                // store array item with record
                preccNew->pItem = &G_MenuItemsWithIDs[ul];

                if (flConfig & (CONFFL_HASSUBMENU | CONFFL_BEGINSUBMENU))
                {
                    // submenu:
                    sprintf(preccNew->szTitle,
                            cmnGetString(ID_XSDI_MENU_MENUSTRING), // "%s" menu
                            cmnGetString(G_MenuItemsWithIDs[ul].ulString));
                }
                else
                {
                    // no submenu, plain item
                    if (flConfig & CONFFL_NOQUOTES)
                        strhncpy0(preccNew->szTitle,
                                  cmnGetString(G_MenuItemsWithIDs[ul].ulString),
                                  sizeof(preccNew->szTitle));
                    else
                    {
                        // enclose in quotes
                        preccNew->szTitle[0] = '\"';
                        strhncpy0(preccNew->szTitle + 1,
                                  cmnGetString(G_MenuItemsWithIDs[ul].ulString),
                                  sizeof(preccNew->szTitle) - 2);
                        strcat(preccNew->szTitle, "\"");
                    }
                }

                if (fDisable)
                    // this is a subrecord, and parent is not checked:
                    flStyle |= CRA_DISABLED;

                // remove tilde
                if (p = strchr(preccNew->szTitle, '~'))
                    strcpy(p, p + 1);

                // insert the record
                cnrhInsertRecords(hwndCnr,
                                  (PRECORDCORE)preccParent,
                                  (PRECORDCORE)preccNew,
                                  TRUE, // invalidate
                                  preccNew->szTitle,
                                  flStyle,
                                  1);

                if (flConfig & CONFFL_BEGINSUBMENU)
                {
                    preccParent = preccNew;
                    // disable submenu items if this parent is not checked
                    fDisable = !preccNew->recc.usCheckState;
                }
                else if (flConfig & CONFFL_ENDSUBMENU)
                {
                    preccParent = NULL;
                    fDisable = FALSE;
                }
            }
        } // for (ul = 0;
    }
}

/*
 *@@ mnuItemsItemChanged:
 *      notebook callback function (notebook.c) for the
 *      second "Menu" page in the "Workplace Shell" object
 *      ("Menu items").
 *      Reacts to changes of any of the dialog controls.
 *
 *      This is all new with V0.9.19 and finally cleans
 *      up the major menu pages mess that existed in XWP
 *      before, and adds a few new features as well.
 *
 *@@added V0.9.19 (2002-04-17) [umoeller]
 */

static MRESULT mnuItemsItemChanged(PNOTEBOOKPAGE pnbp,
                                   ULONG ulItemID,
                                   USHORT usNotifyCode,
                                   ULONG ulExtra)
{
    MRESULT mrc = 0;

    switch (ulItemID)
    {
        case ID_XSDI_MENU_EDIT_CAT_DROP:
            if (usNotifyCode == LN_SELECT)
            {
                pnbp->inbp.pfncbInitPage(pnbp, CBI_SET);
            }
        break;

        case ID_XSDI_MENU_EDIT_CNR:
            if (usNotifyCode == CN_RECORDCHECKED)
            {
                // handle checkbox notifications from cnr
                PMENUITEMRECORD precc = (PMENUITEMRECORD)ulExtra;

                // get index of currently selected category
                HWND    hwndDrop = WinWindowFromID(pnbp->hwndDlgPage, ID_XSDI_MENU_EDIT_CAT_DROP);
                LONG    lSelected = winhQueryLboxSelectedItem(hwndDrop, LIT_FIRST);
                // category pointer was set for each combo box item with CBI_INIT
                PCATEGORYWITHFLAG pCategory = (PCATEGORYWITHFLAG)winhQueryLboxItemHandle(hwndDrop, lSelected);

                // precc->pItem points to the array item
                // in G_MenuItemsWithIDs (set with CBI_INIT)
                ULONG flFilter = precc->pItem->flFilter,
                      fl;

                XWPSETTING s;

                #ifdef DEBUG_MENUS
                _PmpfF(("category is %s", cmnGetString(pCategory->ulString)));
                _Pmpf(("  recc %s, flFilter 0x%08lX",
                      cmnGetString(precc->pItem->ulString), flFilter));
                #endif

                if (flFilter & XWPCTXT_HIGHBIT)
                    // use XWP setting (XWPCTXT_* flag):
                    s = pCategory->sXWP;
                else
                    // use WPS setting (CTXT_* flag):
                    s = pCategory->sWPS;

                // clear the highbit
                flFilter &= ~XWPCTXT_HIGHBIT;
                // clear bit if record is set and reversely
                fl = cmnQuerySetting(s);

                #ifdef DEBUG_MENUS
                _Pmpf(("  old setting %d is 0x%08lX", s, fl));
                #endif

                fl &= ~flFilter;
                if (!precc->recc.usCheckState)
                    fl |= flFilter;

                #ifdef DEBUG_MENUS
                _Pmpf(("  new setting %d is 0x%08lX", s, fl));
                #endif

                cmnSetSetting(s, fl);

                // if this was the record for a submenu,
                // reinsert to get the enabling correct
                if (precc->pItem->flConfig & CONFFL_BEGINSUBMENU)
                    pnbp->inbp.pfncbInitPage(pnbp, CBI_SET);
            }
        break;

        case DID_UNDO:
            cmnRestoreSettings(pnbp->pUser,
                               ARRAYITEMCOUNT(G_MenuItemsBackup));
            pnbp->inbp.pfncbInitPage(pnbp, CBI_SET);
        break;

        case DID_DEFAULT:
            cmnSetDefaultSettings(pnbp->inbp.ulPageID);
            pnbp->inbp.pfncbInitPage(pnbp, CBI_SET);
        break;
    }

    return mrc;
}

/* ******************************************************************
 *
 *   Notebook callbacks (notebook.c) for XFldWPS "Menu 3" page
 *
 ********************************************************************/

static const CONTROLDEF
    ConfigFdrGroup = LOADDEF_GROUP(ID_XSDI_MENUS_CONFIGFDR_GROUP, DEFAULT_TABLE_WIDTH),
    CondCascadeCB = LOADDEF_AUTOCHECKBOX(ID_XSDI_CASCADE),
    RemoveXCB = LOADDEF_AUTOCHECKBOX(ID_XSDI_REMOVEX),
    AppendParamCB = LOADDEF_AUTOCHECKBOX(ID_XSDI_APPDPARAM),
    TemplateGroup = LOADDEF_GROUP(ID_XSDI_TPL_GROUP, DEFAULT_TABLE_WIDTH),
    TplDoNothingRadio = LOADDEF_FIRST_AUTORADIO(ID_XSDI_TPL_DONOTHING),
    TplEditTitleRadio = LOADDEF_NEXT_AUTORADIO(ID_XSDI_TPL_EDITTITLE),
    TplOpenPropertiesRadio = LOADDEF_NEXT_AUTORADIO(ID_XSDI_TPL_OPENSETTINGS),
    TplPositionCB = LOADDEF_AUTOCHECKBOX(ID_XSDI_TPL_POSITION);

static const DLGHITEM G_dlgMenuConfigFdr[] =
    {
        START_TABLE,
            START_ROW(0),
                START_GROUP_TABLE(&ConfigFdrGroup),
                    START_ROW(0),
                        CONTROL_DEF(&CondCascadeCB),
                    START_ROW(0),
                        CONTROL_DEF(&RemoveXCB),
                    START_ROW(0),
                        CONTROL_DEF(&AppendParamCB),
                END_TABLE,
            START_ROW(0),
                START_GROUP_TABLE(&TemplateGroup),
                    START_ROW(0),
                        CONTROL_DEF(&TplDoNothingRadio),
                    START_ROW(0),
                        CONTROL_DEF(&TplEditTitleRadio),
                    START_ROW(0),
                        CONTROL_DEF(&TplOpenPropertiesRadio),
                    START_ROW(0),
                        CONTROL_DEF(&TplPositionCB),
                END_TABLE,
            START_ROW(0),
                CONTROL_DEF(&G_UndoButton),         // common.c
                CONTROL_DEF(&G_DefaultButton),      // common.c
                CONTROL_DEF(&G_HelpButton),         // common.c
        END_TABLE
    };

static const XWPSETTING G_MenuConfigFdrBackup[] =
    {
        sfMenuCascadeMode,
        sfRemoveX,
        sfAppdParam,
        sulTemplatesOpenSettings,
        sfTemplatesReposition
    };

/*
 *@@ mnuConfigFolderMenusInitPage:
 *      notebook callback function (notebook.c) for the
 *      third "Menu" page in the "Workplace Shell" object
 *      ("Config folder items").
 *      Sets the controls on the page according to the
 *      Global Settings.
 *
 *      With V0.9.19, this is now under "Menus" page 3.
 *
 *@@changed V0.9.0 [umoeller]: adjusted function prototype
 *@@changed V0.9.19 (2002-04-24) [umoeller]: now using dialog formatter
 */

static VOID mnuConfigFolderMenusInitPage(PNOTEBOOKPAGE pnbp,   // notebook info struct
                                         ULONG flFlags)        // CBI_* flags (notebook.h)
{
    if (flFlags & CBI_INIT)
    {
        // first call: backup Global Settings for "Undo" button;
        // this memory will be freed automatically by the
        // common notebook window function (notebook.c) when
        // the notebook page is destroyed
        pnbp->pUser = cmnBackupSettings(G_MenuConfigFdrBackup,
                                         ARRAYITEMCOUNT(G_MenuConfigFdrBackup));

        // insert the controls using the dialog formatter
        // V0.9.19 (2002-04-24) [umoeller]
        ntbFormatPage(pnbp->hwndDlgPage,
                      G_dlgMenuConfigFdr,
                      ARRAYITEMCOUNT(G_dlgMenuConfigFdr));
    }

    if (flFlags & CBI_SET)
    {
        winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_XSDI_CASCADE,
                              cmnQuerySetting(sfMenuCascadeMode));
        winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_XSDI_REMOVEX,
                              cmnQuerySetting(sfRemoveX));
        winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_XSDI_APPDPARAM,
                              cmnQuerySetting(sfAppdParam));

        switch (cmnQuerySetting(sulTemplatesOpenSettings))
        {
            case 0:  winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_XSDI_TPL_DONOTHING, 1); break;
            case 1:  winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_XSDI_TPL_OPENSETTINGS, 1); break;
            default:  winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_XSDI_TPL_EDITTITLE, 1); break;
        }

        winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_XSDI_TPL_POSITION,
                              cmnQuerySetting(sfTemplatesReposition));
    }
}

/*
 *@@ mnuConfigFolderMenusItemChanged:
 *      notebook callback function (notebook.c) for the
 *      third "Menu" page in the "Workplace Shell" object
 *      ("Config folder items").
 *      Reacts to changes of any of the dialog controls.
 *
 *      With V0.9.19, this is now under "Menus" page 3.
 *
 *@@changed V0.9.0 [umoeller]: adjusted function prototype
 */

static MRESULT mnuConfigFolderMenusItemChanged(PNOTEBOOKPAGE pnbp,
                                               ULONG ulItemID,
                                               USHORT usNotifyCode,
                                               ULONG ulExtra)      // for checkboxes: contains new state
{
    MRESULT mrc = (MRESULT)0;
    BOOL fSave = TRUE;

    switch (ulItemID)
    {
        case ID_XSDI_CASCADE:
            cmnSetSetting(sfMenuCascadeMode, ulExtra);
        break;

        case ID_XSDI_REMOVEX:
            cmnSetSetting(sfRemoveX, ulExtra);
        break;

        case ID_XSDI_APPDPARAM:
            cmnSetSetting(sfAppdParam, ulExtra);
        break;

        // "create from templates" settings
        case ID_XSDI_TPL_DONOTHING:
            cmnSetSetting(sulTemplatesOpenSettings, 0);
        break;

        case ID_XSDI_TPL_EDITTITLE:
            cmnSetSetting(sulTemplatesOpenSettings, BM_INDETERMINATE);
        break;

        case ID_XSDI_TPL_OPENSETTINGS:
            cmnSetSetting(sulTemplatesOpenSettings, BM_CHECKED);
        break;

        case ID_XSDI_TPL_POSITION:
            cmnSetSetting(sfTemplatesReposition, ulExtra);
        break;

        case DID_UNDO:
        {
            // "Undo" button: get pointer to backed-up Global Settings
            // and restore the settings for this page
            cmnRestoreSettings(pnbp->pUser,
                               ARRAYITEMCOUNT(G_MenuConfigFdrBackup));

            // update the display by calling the INIT callback
            pnbp->inbp.pfncbInitPage(pnbp, CBI_SET | CBI_ENABLE);
        }
        break;

        case DID_DEFAULT:
        {
            // set the default settings for this settings page
            // (this is in common.c because it's also used at
            // Desktop startup)
            cmnSetDefaultSettings(pnbp->inbp.ulPageID);
            // update the display by calling the INIT callback
            pnbp->inbp.pfncbInitPage(pnbp, CBI_SET | CBI_ENABLE);
        }
        break;

        default:
            fSave = FALSE;
    }

    return mrc;
}

/*
 *@@ mnuAddWPSMenuPages:
 *      implementation for XFldWPS::xwpAddWPSMenuPages
 *      so we won't have to export all the notebook
 *      callbacks.
 *
 *@@added V0.9.19 (2002-04-17) [umoeller]
 */

ULONG mnuAddWPSMenuPages(WPObject *somSelf,     // in: XFldWPS* object
                         HWND hwndDlg)          // in: dialog
{
    INSERTNOTEBOOKPAGE inbp;
    HMODULE         savehmod = cmnQueryNLSModuleHandle(FALSE);
    ULONG           ulrc;

    // insert "Config folder menu items" page (bottom)
    memset(&inbp, 0, sizeof(INSERTNOTEBOOKPAGE));
    inbp.somSelf = somSelf;
    inbp.hwndNotebook = hwndDlg;
    inbp.hmod = savehmod;
    inbp.usPageStyleFlags = BKA_MINOR;
    inbp.fEnumerate = TRUE;
    inbp.pcszName =
    inbp.pcszMinorName = cmnGetString(ID_XSSI_26CONFIGITEMS);
    inbp.ulDlgID = ID_XFD_EMPTYDLG; // ID_XSD_SET26CONFIGMENUS; V0.9.19 (2002-04-24) [umoeller]
    inbp.ulDefaultHelpPanel  = ID_XSH_SETTINGS_CFGM;
    inbp.ulPageID = SP_26CONFIGITEMS;
    inbp.pfncbInitPage    = mnuConfigFolderMenusInitPage;
    inbp.pfncbItemChanged = mnuConfigFolderMenusItemChanged;
    ntbInsertPage(&inbp);

    // new menu items page V0.9.19 (2002-04-24) [umoeller]
    memset(&inbp, 0, sizeof(INSERTNOTEBOOKPAGE));
    inbp.somSelf = somSelf;
    inbp.hwndNotebook = hwndDlg;
    inbp.hmod = savehmod;
    inbp.usPageStyleFlags = BKA_MINOR;
    inbp.fEnumerate = TRUE;
    inbp.pcszName = cmnGetString(ID_XSSI_DTPMENUPAGE);
    inbp.pcszMinorName = cmnGetString(ID_XSDI_MENU_ITEMS);
    inbp.ulDlgID = ID_XFD_EMPTYDLG;
    inbp.ulDefaultHelpPanel  = ID_XSH_SETTINGS_MENUITEMS;
    inbp.ulPageID = SP_MENUITEMS;
    inbp.pfncbInitPage    = mnuItemsInitPage;
    inbp.pfncbItemChanged = mnuItemsItemChanged;
    inbp.pampControlFlags = G_ampFile1Page;
    inbp.cControlFlags = ARRAYITEMCOUNT(G_ampFile1Page);
    ulrc = ntbInsertPage(&inbp);

    // new menu settings page V0.9.19 (2002-04-24) [umoeller]
    memset(&inbp, 0, sizeof(INSERTNOTEBOOKPAGE));
    inbp.somSelf = somSelf;
    inbp.hwndNotebook = hwndDlg;
    inbp.hmod = savehmod;
    inbp.usPageStyleFlags = BKA_MAJOR | BKA_MINOR;
    inbp.fEnumerate = TRUE;
    inbp.pcszName = cmnGetString(ID_XSSI_DTPMENUPAGE);
    inbp.pcszMinorName = cmnGetString(ID_XSDI_MENU_SETTINGS);
    inbp.ulDlgID = ID_XFD_EMPTYDLG;
    inbp.ulDefaultHelpPanel  = ID_XSH_SETTINGS_MENUSETTINGS;
    inbp.ulPageID = SP_MENUSETTINGS;
    inbp.pfncbInitPage    = mnuSettingsInitPage;
    inbp.pfncbItemChanged = mnuSettingsItemChanged;
    ulrc = ntbInsertPage(&inbp);

    return ulrc;
}
