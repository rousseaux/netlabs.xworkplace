
/*
 *@@sourcefile fdrhotky.c:
 *      implementation file for the XFolder folder hotkeys.
 *
 *      This code gets interfaced from method overrides in
 *      xfldr.c, from fdr_fnwpSubclassedFolderFrame, and from
 *      XFldWPS (xfwps.c).
 *
 *      This is for _folder_ hotkeys only, which are implemented
 *      thru folder subclassing (fdr_fnwpSubclassedFolderFrame),
 *      which in turn calls fdrProcessFldrHotkey.
 *      The global object hotkeys are instead implemented using
 *      the XWorkplace hook (xwphook.c).
 *
 *      This file is ALL new with V0.9.0. Most of this code
 *      used to be in common.c before V0.9.0.
 *
 *      Function prefix for this file:
 *      --  fdr*
 *
 *@@added V0.9.0 [umoeller]
 *@@header "filesys\folder.h"
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

#define INCL_WINSHELLDATA       // Prf* functions
#define INCL_WININPUT
#define INCL_WINWINDOWMGR
#define INCL_WINDIALOGS
#define INCL_WINBUTTONS
#define INCL_WINLISTBOXES
#include <os2.h>

// C library headers

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\winh.h"               // PM helper routines

// SOM headers which don't crash with prec. header files
#include "xfldr.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\notebook.h"            // generic XWorkplace notebook handling
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

#include "filesys\folder.h"

// other SOM headers
#pragma hdrstop                     // VAC++ keeps crashing otherwise
#include "helpers\undoc.h"              // some undocumented stuff

/* ******************************************************************
 *                                                                  *
 *   Global variables                                               *
 *                                                                  *
 ********************************************************************/

/*
 *  The folder hotkeys are stored in a static array which
 *  is is FLDRHOTKEYCOUNT items in size. This is calculated
 *  at compile time according to the items which were
 *  declared in dlgids.h.
 *
 *  The following functions give the other XWorkplace components
 *  access to this data and/or manipulate it.
 */

// XFolder folder hotkeys static array
XFLDHOTKEY     G_FolderHotkeys[FLDRHOTKEYCOUNT];

/* ******************************************************************
 *                                                                  *
 *   Folder hotkey functions                                        *
 *                                                                  *
 ********************************************************************/

/*
 *@@ fdrQueryFldrHotkeys:
 *      this returns the address of the static
 *      folder hotkeys array in common.c. The
 *      size of that array is FLDRHOTKEYSSIZE (common.h).
 *
 *@@changed V0.9.0 [umoeller]: moved this here from common.c
 */

PXFLDHOTKEY fdrQueryFldrHotkeys(VOID)
{
    return (G_FolderHotkeys);
}

/*
 *@@ fdrLoadDefaultFldrHotkeys:
 *      this resets the folder hotkeys to the default
 *      values.
 *
 *@@changed V0.9.0 [umoeller]: moved this here from common.c
 */

void fdrLoadDefaultFldrHotkeys(VOID)
{
    // Ctrl+A: Select all
    G_FolderHotkeys[0].usKeyCode  = (USHORT)'a';
    G_FolderHotkeys[0].usFlags    = KC_CTRL;
    G_FolderHotkeys[0].usCommand  = WPMENUID_SELALL;

    // F5
    G_FolderHotkeys[1].usKeyCode  = VK_F5;
    G_FolderHotkeys[1].usFlags    = KC_VIRTUALKEY;
    G_FolderHotkeys[1].usCommand  = WPMENUID_REFRESH,

    // Backspace
    G_FolderHotkeys[2].usKeyCode  = VK_BACKSPACE;
    G_FolderHotkeys[2].usFlags    = KC_VIRTUALKEY;
    G_FolderHotkeys[2].usCommand  = ID_XFMI_OFS_OPENPARENT;

    // Ctrl+D: De-select all
    G_FolderHotkeys[3].usKeyCode  = (USHORT)'d';
    G_FolderHotkeys[3].usFlags    = KC_CTRL;
    G_FolderHotkeys[3].usCommand  = WPMENUID_DESELALL;

    // Ctrl+Shift+D: Details view
    G_FolderHotkeys[4].usKeyCode  = (USHORT)'D';
    G_FolderHotkeys[4].usFlags    = KC_CTRL+KC_SHIFT;
    G_FolderHotkeys[4].usCommand  = WPMENUID_DETAILS;

    // Ctrl+Shift+I: Icon  view
    G_FolderHotkeys[5].usKeyCode  = (USHORT)'I';
    G_FolderHotkeys[5].usFlags    = KC_CTRL+KC_SHIFT;
    G_FolderHotkeys[5].usCommand  = WPMENUID_ICON;

    // Ctrl+Shift+S: Open Settings
    G_FolderHotkeys[6].usKeyCode  = (USHORT)'S';
    G_FolderHotkeys[6].usFlags    = KC_CTRL+KC_SHIFT;
    G_FolderHotkeys[6].usCommand  = WPMENUID_PROPERTIES;

    // Ctrl+N: Sort by name
    G_FolderHotkeys[7].usKeyCode  = (USHORT)'n';
    G_FolderHotkeys[7].usFlags    = KC_CTRL;
    G_FolderHotkeys[7].usCommand  = ID_WPMI_SORTBYNAME;

    // Ctrl+Z: Sort by size
    G_FolderHotkeys[8].usKeyCode  = (USHORT)'z';
    G_FolderHotkeys[8].usFlags    = KC_CTRL;
    G_FolderHotkeys[8].usCommand  = ID_WPMI_SORTBYSIZE;

    // Ctrl+E: Sort by extension (NPS)
    G_FolderHotkeys[9].usKeyCode  = (USHORT)'e';
    G_FolderHotkeys[9].usFlags    = KC_CTRL;
    G_FolderHotkeys[9].usCommand  = ID_XFMI_OFS_SORTBYEXT;

    // Ctrl+W: Sort by write date
    G_FolderHotkeys[10].usKeyCode  = (USHORT)'w';
    G_FolderHotkeys[10].usFlags    = KC_CTRL;
    G_FolderHotkeys[10].usCommand  = ID_WPMI_SORTBYWRITEDATE;

    // Ctrl+Y: Sort by type
    G_FolderHotkeys[11].usKeyCode  = (USHORT)'y';
    G_FolderHotkeys[11].usFlags    = KC_CTRL;
    G_FolderHotkeys[11].usCommand  = ID_WPMI_SORTBYTYPE;

    // Shift+Backspace
    G_FolderHotkeys[12].usKeyCode  = VK_BACKSPACE;
    G_FolderHotkeys[12].usFlags    = KC_VIRTUALKEY+KC_SHIFT;
    G_FolderHotkeys[12].usCommand  = ID_XFMI_OFS_OPENPARENTANDCLOSE;

    // Ctrl+S: Select by name
    G_FolderHotkeys[13].usKeyCode  = (USHORT)'s';
    G_FolderHotkeys[13].usFlags    = KC_CTRL;
    G_FolderHotkeys[13].usCommand  = ID_XFMI_OFS_SELECTSOME;

    // Ctrl+insert: copy filename (w/out path)
    G_FolderHotkeys[14].usKeyCode  = VK_INSERT;
    G_FolderHotkeys[14].usFlags    = KC_VIRTUALKEY+KC_CTRL;
    G_FolderHotkeys[14].usCommand  = ID_XFMI_OFS_COPYFILENAME_SHORT;

    // list terminator
    G_FolderHotkeys[15].usCommand = 0;
}

/*
 *@@ fdrLoadFolderHotkeys:
 *      this initializes the folder hotkey array with
 *      the data which was previously stored in OS2.INI.
 *
 *@@changed V0.9.0 [umoeller]: moved this here from common.c
 */

void fdrLoadFolderHotkeys(VOID)
{
    ULONG ulCopied2 = sizeof(G_FolderHotkeys);
    if (!PrfQueryProfileData(HINI_USERPROFILE,
                             (PSZ)INIAPP_XWORKPLACE, (PSZ)INIKEY_ACCELERATORS,
                             &G_FolderHotkeys,
                             &ulCopied2))
        fdrLoadDefaultFldrHotkeys();
}

/*
 *@@ fdrStoreFldrHotkeys:
 *       this stores the folder hotkeys in OS2.INI.
 *
 *@@changed V0.9.0 [umoeller]: moved this here from common.c
 */

void fdrStoreFldrHotkeys(VOID)
{
    SHORT i2 = 0;

    // store only the accels that are actually used,
    // so count them first
    while (G_FolderHotkeys[i2].usCommand)
        i2++;

    PrfWriteProfileData(HINI_USERPROFILE,
                        (PSZ)INIAPP_XWORKPLACE, (PSZ)INIKEY_ACCELERATORS,
                        &G_FolderHotkeys,
                        (i2+1) * sizeof(XFLDHOTKEY));
}

/*
 *@@ fdrFindHotkey:
 *      searches the hotkeys list for whether
 *      a hotkey has been defined for the specified
 *      command (WM_COMMAND msg value).
 *
 *      If so, TRUE is returned and the hotkey
 *      definition is stored in the specified two
 *      USHORT's.
 *
 *      Otherwise, FALSE is returned.
 *
 *@@added V0.9.2 (2000-03-08) [umoeller]
 */

BOOL fdrFindHotkey(USHORT usCommand,
                   PUSHORT pusFlags,
                   PUSHORT pusKeyCode)
{
    ULONG   i = 0;
    BOOL    brc = FALSE;
    // go thru all hotkeys
    while (G_FolderHotkeys[i].usCommand)
    {
        if (G_FolderHotkeys[i].usCommand == usCommand)
        {
            // found:
            *pusFlags = G_FolderHotkeys[i].usFlags;
            *pusKeyCode = G_FolderHotkeys[i].usKeyCode;
            brc = TRUE;
            break;
        }
        i++;
    }

    return (brc);
}

/*
 *@@ fdrProcessFldrHotkey:
 *      this is called by fdr_fnwpSubclassedFolderFrame to
 *      check for whether a given WM_CHAR message matches
 *      one of the folder hotkeys.
 *
 *      The parameters are those of the WM_CHAR message. This
 *      returns TRUE if the pressed key was a hotkey; in that
 *      case, the corresponding WM_COMMAND message is
 *      automatically posted to the folder frame, which will
 *      cause the defined action to occur (that is, the WPS
 *      will call the proper wpMenuItemSelected method).
 *
 *@@changed V0.9.0 [umoeller]: moved this here from common.c
 *@@changed V0.9.1 (2000-01-31) [umoeller]: changed prototype; this was using MPARAMS previously
 *@@changed V0.9.9 (2001-02-28) [pr]: allow multiple actions on same hotkey
 *@@changed V0.9.14 (2001-07-28) [umoeller]: now disabling sort and arrange hotkeys for desktop, if those menu items are disabled
 */

BOOL fdrProcessFldrHotkey(WPFolder *somSelf,
                          HWND hwndFrame,   // in: folder frame
                          USHORT usFlags,
                          USHORT usch,
                          USHORT usvk)
{
    BOOL brc = FALSE;
    // now check if the key is relevant: filter out KEY UP
    // messages and check if either a virtual key (such as F5)
    // or Ctrl or Alt was pressed
    if (    ((usFlags & KC_KEYUP) == 0)
        &&  (     ((usFlags & KC_VIRTUALKEY) != 0)
                  // Ctrl pressed?
               || ((usFlags & KC_CTRL) != 0)
                  // Alt pressed?
               || ((usFlags & KC_ALT) != 0)
                  // or one of the Win95 keys?
               || (   ((usFlags & KC_VIRTUALKEY) == 0)
                   && (     (usch == 0xEC00)
                        ||  (usch == 0xED00)
                        ||  (usch == 0xEE00)
                      )
                  )
            )
       )
    {
        USHORT us = 0;
        USHORT usKeyCode  = 0;

        if (usFlags & KC_VIRTUALKEY)
            usKeyCode = usvk;
        else
            usKeyCode = usch;

        // filter out unwanted flags
        usFlags &= (KC_VIRTUALKEY | KC_CTRL | KC_ALT | KC_SHIFT);

        #ifdef DEBUG_KEYS
            _Pmpf(("fdrProcessFldrHotkey: usKeyCode: 0x%lX, usFlags: 0x%lX", usKeyCode, usFlags));
        #endif

        // now go through the global accelerator list and check
        // if the pressed key was assigned an action to
        while (G_FolderHotkeys[us].usCommand)
        {
            USHORT usCommand;
            if (      (G_FolderHotkeys[us].usFlags == usFlags)
                   && (G_FolderHotkeys[us].usKeyCode == usKeyCode)
               )
            {
                // OK: this is a hotkey...

                BOOL                fPost = TRUE;
                PCGLOBALSETTINGS    pGlobalSettings = cmnQueryGlobalSettings();

                // find the corresponding
                // "command" (= menu ID) and post it to the frame
                // window, which will execute it
                usCommand = G_FolderHotkeys[us].usCommand;

                // now, if sort or arrange are disabled for
                // the desktop and this is a sort or arrange
                // hotkey, swallow it
                // V0.9.14 (2001-07-28) [umoeller]
                if (cmnIsADesktop(somSelf))
                {
                    switch (usCommand)
                    {
                        case ID_WPMI_SORTBYNAME:
                        case ID_WPMI_SORTBYSIZE:
                        case ID_WPMI_SORTBYTYPE:
                        case ID_WPMI_SORTBYREALNAME:
                        case ID_WPMI_SORTBYWRITEDATE:
                        case ID_WPMI_SORTBYACCESSDATE:
                        case ID_WPMI_SORTBYCREATIONDATE:
                        case ID_XFMI_OFS_SORTBYEXT:
                        case ID_XFMI_OFS_SORTFOLDERSFIRST:
                        case ID_XFMI_OFS_SORTBYCLASS:
                            if (!pGlobalSettings->fDTMSort)
                                fPost = FALSE;
                        break;

                        case WPMENUID_ARRANGE:
                        case ID_WPMI_ARRANGEFROMTOP:
                        case ID_WPMI_ARRANGEFROMLEFT:
                        case ID_WPMI_ARRANGEFROMRIGHT:
                        case ID_WPMI_ARRANGEFROMBOTTOM:
                        case ID_WPMI_ARRANGEPERIMETER:
                        case ID_WPMI_ARRANGEHORIZONTALLY:
                        case ID_WPMI_ARRANGEVERTICALLY:
                            if (!pGlobalSettings->fDTMArrange)
                                fPost = FALSE;
                    }
                }

                if (fPost)
                {
                    if (    (usCommand >= WPMENUID_USER)
                         && (usCommand < WPMENUID_USER + FIRST_VARIABLE)
                       )
                    {
                        // it's one of the "variable" menu items:
                        // add the global variable menu offset
                        usCommand += pGlobalSettings->VarMenuOffset;
                    }

                    WinPostMsg(hwndFrame,
                               WM_COMMAND,
                               (MPARAM)usCommand,
                               MPFROM2SHORT(CMDSRC_MENU,
                                            FALSE) );     // results from keyboard operation

                    #ifdef DEBUG_KEYS
                        _Pmpf(("  Posting command 0x%lX", usCommand));
                    #endif
                }

                // return TRUE even if we did NOT post;
                // otherwise the WM_CHAR will be processed
                // by parent winproc
                brc = TRUE;
            }
            us++;
        }
    }

    return (brc);
}

/* ******************************************************************
 *                                                                  *
 *   Notebook callbacks (notebook.c) for XFldWPS "Hotkeys" page     *
 *                                                                  *
 ********************************************************************/

/*
 *@@ FLDRHOTKEYDESC:
 *
 *@@added V0.9.2 (2000-03-08) [umoeller]
 */

typedef struct _FLDRHOTKEYDESC
{
    ULONG       ulStringID;         // string ID for listbox
    USHORT      usPostCommand,      // command to post / store with hotkey
                usMenuCommand;      // menu item to append hotkey to
} FLDRHOTKEYDESC, *PFLDRHOTKEYDESC;

/*
 *@@ G_szLBEntries:
 *
 *
 */

CHAR  G_szLBEntries[FLDRHOTKEYCOUNT][MAXLBENTRYLENGTH];

/*
 *@@ G_aDescriptions:
 *      array of FLDRHOTKEYDESC items.
 *
 *@@added V0.9.2 (2000-03-08) [umoeller]
 */

FLDRHOTKEYDESC G_aDescriptions[FLDRHOTKEYCOUNT] =
    {
         ID_XSSI_LB_REFRESHNOW, WPMENUID_REFRESH, WPMENUID_REFRESH,
         ID_XSSI_LB_SNAPTOGRID, ID_XFMI_OFS_SNAPTOGRID, ID_XFMI_OFS_SNAPTOGRID,
         ID_XSSI_LB_SELECTALL, WPMENUID_SELALL, WPMENUID_SELALL,
         ID_XSSI_LB_OPENPARENTFOLDER, ID_XFMI_OFS_OPENPARENT, 0x2CA,

         ID_XSSI_LB_OPENSETTINGSNOTEBOOK, WPMENUID_PROPERTIES, WPMENUID_PROPERTIES,

         ID_XSSI_LB_OPENNEWDETAILSVIEW, WPMENUID_DETAILS, WPMENUID_DETAILS,
         ID_XSSI_LB_OPENNEWICONVIEW, WPMENUID_ICON, WPMENUID_ICON,

         ID_XSSI_LB_DESELECTALL, WPMENUID_DESELALL, WPMENUID_DESELALL,

         ID_XSSI_LB_OPENNEWTREEVIEW, WPMENUID_TREE, WPMENUID_TREE,

         ID_XSSI_LB_FIND, WPMENUID_FIND, WPMENUID_FIND,

         ID_XSSI_LB_PICKUP, WPMENUID_PICKUP, WPMENUID_PICKUP,
         ID_XSSI_LB_PICKUPCANCELDRAG, WPMENUID_PUTDOWN_CANCEL, WPMENUID_PUTDOWN_CANCEL,

         ID_XSSI_LB_SORTBYNAME, ID_WPMI_SORTBYNAME, ID_WPMI_SORTBYNAME,
         ID_XSSI_LB_SORTBYSIZE, ID_WPMI_SORTBYSIZE, ID_WPMI_SORTBYSIZE,
         ID_XSSI_LB_SORTBYTYPE, ID_WPMI_SORTBYTYPE, ID_WPMI_SORTBYTYPE,
         ID_XSSI_LB_SORTBYREALNAME, ID_WPMI_SORTBYREALNAME, ID_WPMI_SORTBYREALNAME,
         ID_XSSI_LB_SORTBYWRITEDATE, ID_WPMI_SORTBYWRITEDATE, ID_WPMI_SORTBYWRITEDATE,
         ID_XSSI_LB_SORTBYACCESSDATE, ID_WPMI_SORTBYACCESSDATE, ID_WPMI_SORTBYACCESSDATE,
         ID_XSSI_LB_SORTBYCREATIONDATE, ID_WPMI_SORTBYCREATIONDATE, ID_WPMI_SORTBYCREATIONDATE,

         ID_XSSI_LB_SWITCHTOICONVIEW, ID_WPMI_SHOWICONVIEW, ID_WPMI_SHOWICONVIEW,
         ID_XSSI_LB_SWITCHTODETAILSVIEW, ID_WPMI_SHOWDETAILSVIEW, ID_WPMI_SHOWDETAILSVIEW,
         ID_XSSI_LB_SWITCHTOTREEVIEW, ID_WPMI_SHOWTREEVIEW, ID_WPMI_SHOWTREEVIEW,

         ID_XSSI_LB_ARRANGEDEFAULT, WPMENUID_ARRANGE, WPMENUID_ARRANGE,
         ID_XSSI_LB_ARRANGEFROMTOP, ID_WPMI_ARRANGEFROMTOP, ID_WPMI_ARRANGEFROMTOP,
         ID_XSSI_LB_ARRANGEFROMLEFT, ID_WPMI_ARRANGEFROMLEFT, ID_WPMI_ARRANGEFROMLEFT,
         ID_XSSI_LB_ARRANGEFROMRIGHT, ID_WPMI_ARRANGEFROMRIGHT, ID_WPMI_ARRANGEFROMRIGHT,
         ID_XSSI_LB_ARRANGEFROMBOTTOM, ID_WPMI_ARRANGEFROMBOTTOM, ID_WPMI_ARRANGEFROMBOTTOM,
         ID_XSSI_LB_ARRANGEPERIMETER, ID_WPMI_ARRANGEPERIMETER, ID_WPMI_ARRANGEPERIMETER,
         ID_XSSI_LB_ARRANGEHORIZONTALLY, ID_WPMI_ARRANGEHORIZONTALLY, ID_WPMI_ARRANGEHORIZONTALLY,
         ID_XSSI_LB_ARRANGEVERTICALLY, ID_WPMI_ARRANGEVERTICALLY, ID_WPMI_ARRANGEVERTICALLY,

         ID_XSSI_LB_INSERT, ID_WPMI_PASTE, ID_WPMI_PASTE,

         ID_XSSI_LB_SORTBYEXTENSION, ID_XFMI_OFS_SORTBYEXT, ID_XFMI_OFS_SORTBYEXT,
         ID_XSSI_LB_OPENPARENTFOLDERANDCLOSE, ID_XFMI_OFS_OPENPARENTANDCLOSE, ID_XFMI_OFS_OPENPARENTANDCLOSE,

         ID_XSSI_LB_CLOSEWINDOW, ID_XFMI_OFS_CLOSE, ID_XFMI_OFS_CLOSE,
         ID_XSSI_LB_SELECTSOME, ID_XFMI_OFS_SELECTSOME, ID_XFMI_OFS_SELECTSOME,
         ID_XSSI_LB_SORTFOLDERSFIRST, ID_XFMI_OFS_SORTFOLDERSFIRST, ID_XFMI_OFS_SORTFOLDERSFIRST,
         ID_XSSI_LB_SORTBYCLASS, ID_XFMI_OFS_SORTBYCLASS, ID_XFMI_OFS_SORTBYCLASS,

         ID_XSSI_LB_CONTEXTMENU, ID_XFMI_OFS_CONTEXTMENU, ID_XFMI_OFS_CONTEXTMENU,
         ID_XSSI_LB_TASKLIST, 0x8011, 0,

         ID_XSSI_LB_COPYFILENAME_SHORT, ID_XFMI_OFS_COPYFILENAME_SHORT, 0,
         ID_XSSI_LB_COPYFILENAME_FULL, ID_XFMI_OFS_COPYFILENAME_FULL, 0
    };

/*
 *@@ FindHotkeyFromLBSel:
 *      this subroutine finds an index to the global
 *      G_FolderHotkeys[] array according to the currently
 *      selected list box item. This is used on the
 *      "Folder hotkeys" notebook page.
 *      This returns:
 *        NULL  if no accelerator has been defined yet
 *          -1  if LB entries do not match array strings
 *              (should not happen)
 *       other: means that an accelerator has been defined.
 *              We then return the pointer to the hotkey.
 *      If pusCommand is != NULL, it will then contain the
 *      menu "command" of the selected LB item, such that
 *      a new accelerator can be created.
 */

PXFLDHOTKEY FindHotkeyFromLBSel(HWND hwndDlg,
                                USHORT *pusCommand)     // out: menu command
{
    SHORT               i, i2 = 0;
    CHAR                szTemp[MAXLBENTRYLENGTH];
    SHORT               sItem;
    SHORT               LBIndex = -1;
    PXFLDHOTKEY         pHotkey = fdrQueryFldrHotkeys(),
                        pHotkeyFound = NULL;

    sItem = (SHORT)WinSendDlgItemMsg(hwndDlg, ID_XSDI_LISTBOX,
                                     LM_QUERYSELECTION,
                                     (MPARAM)LIT_CURSOR,
                                     MPNULL);

    WinSendDlgItemMsg(hwndDlg, ID_XSDI_LISTBOX,
                      LM_QUERYITEMTEXT,
                      MPFROM2SHORT(sItem, sizeof(szTemp)),
                      (MPARAM)szTemp);
            // szTemp now contains the selected list box entry's text

    for (i = 0; i < FLDRHOTKEYCOUNT; i++)
    {
        if (strcmp(szTemp, G_szLBEntries[i]) == 0)
        {
            LBIndex = i;
            break;
        }
    }
        // i now contains the index in the listbox arrays:
        // -- szLBEntries[] for the strings,
        // -- ulLBCommands[] for the corresponding commands
        // or -1 if not found.

    // item found?
    if (LBIndex != -1)
    {
        // yes:
        // loop thru the hotkeys array
        while (pHotkey->usCommand)
        {
            if (pHotkey->usCommand == G_aDescriptions[i].usPostCommand)
            {
                // hotkey defined for this listbox item already:
                pHotkeyFound = pHotkey;
                break;
            }
            // else: go for next item
            pHotkey++;
            i2++;
        }

        if (    (pHotkeyFound == NULL)
             && (pusCommand)
           )
            // hotkey not yet defined: store the command
            // which corresponds to the selected item, so
            // a new hotkey can be added
            *pusCommand = G_aDescriptions[i].usPostCommand;

        return (pHotkeyFound);
    }
    else
        return (PXFLDHOTKEY)-1;     // LB entry not found
}

/*
 *@@ AddHotkeyToMenuItem:
 *
 *@@added V0.9.2 (2000-03-08) [umoeller]
 */

VOID AddHotkeyToMenuItem(HWND hwndMenu,
                         USHORT usPostCommand2Find,
                         USHORT usMenuCommand,
                         ULONG ulVarMenuOffset) // pGlobalSettings->VarMenuOffset
{
    USHORT  usFlags, usKeyCode;
    CHAR    szDescription[100];

    if (fdrFindHotkey(usPostCommand2Find,
                      &usFlags,
                      &usKeyCode))
    {
        if (    (usMenuCommand >= WPMENUID_USER)
             && (usMenuCommand < WPMENUID_USER+FIRST_VARIABLE)
           )
            // it's one of the "variable" menu items:
            // add the global variable menu offset
            usMenuCommand += ulVarMenuOffset;

        cmnDescribeKey(szDescription,
                       usFlags,
                       usKeyCode);
        // _Pmpf(("Found %s", szDescription));
        winhAppend2MenuItemText(hwndMenu,
                                usMenuCommand,
                                szDescription,
                                TRUE);
    }
}

/*
 *@@ fdrAddHotkeysToMenu:
 *      gets called by XFldObject::wpModifyPopupMenu to add
 *      hotkey descriptions to the popup menu.
 *
 *      Note that this adds the generic hotkeys for WPObject
 *      only.
 *
 *@@added V0.9.2 (2000-03-06) [umoeller]
 *@@changed V0.9.4 (2000-06-11) [umoeller]: hotkeys showed up even if hotkeys were globally disabled; fixed
 */

VOID fdrAddHotkeysToMenu(WPObject *somSelf,
                         HWND hwndCnr,
                         HWND hwndMenu) // in: menu created by wpDisplayMenu
{
    PCGLOBALSETTINGS     pGlobalSettings = cmnQueryGlobalSettings();

    if (    (pGlobalSettings->fEnableFolderHotkeys) // V0.9.4 (2000-06-11) [umoeller]
         && (pGlobalSettings->fShowHotkeysInMenus)
       )
    {
        CHAR        szDescription[100];
        // PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();
        BOOL        // fIsFolder = _somIsA(somSelf, _WPFolder),
                    fCnrWhitespace = wpshIsViewCnr(somSelf, hwndCnr);

        if (!fCnrWhitespace)
        {
            // menu opened for object inserted into
            // container (not for cnr whitespace):

            // delete
            winhAppend2MenuItemText(hwndMenu,
                                    WPMENUID_DELETE,
                                    cmnGetString(ID_XSSI_KEY_DELETE),  // pszDelete
                                    TRUE);

            // open settings
            cmnDescribeKey(szDescription,
                           KC_ALT | KC_VIRTUALKEY,
                           VK_ENTER);
            winhAppend2MenuItemText(hwndMenu,
                                    WPMENUID_PROPERTIES,
                                    szDescription,
                                    TRUE);

            // default help
            cmnDescribeKey(szDescription,
                           KC_VIRTUALKEY,
                           VK_F1);
            winhAppend2MenuItemText(hwndMenu,
                                    WPMENUID_EXTENDEDHELP,
                                    szDescription,
                                    TRUE);

            // copy filename
            if (pGlobalSettings->AddCopyFilenameItem)
            {
                AddHotkeyToMenuItem(hwndMenu,
                                    ID_XFMI_OFS_COPYFILENAME_SHORT,
                                    ID_XFMI_OFS_COPYFILENAME_MENU,
                                    pGlobalSettings->VarMenuOffset);
                AddHotkeyToMenuItem(hwndMenu,
                                    ID_XFMI_OFS_COPYFILENAME_FULL,
                                    ID_XFMI_OFS_COPYFILENAME_MENU, // same menu item!
                                    pGlobalSettings->VarMenuOffset);
            }
        }
        else
        {
            // menu on cnr whitespace:
            ULONG       ul;

            for (ul = 0;
                 ul < (     sizeof(G_aDescriptions)
                          / sizeof(G_aDescriptions[0])
                      );
                 ul++)
            {
                // menu modification allowed for this command?
                if (G_aDescriptions[ul].usMenuCommand)
                {
                    AddHotkeyToMenuItem(hwndMenu,
                                        G_aDescriptions[ul].usPostCommand, // usPostCommand2Find
                                        G_aDescriptions[ul].usMenuCommand, // usMenuCommand
                                        pGlobalSettings->VarMenuOffset);
                }
            }

            // OK, now we got most menu items;
            // we need a few more special checks
            if (pGlobalSettings->MoveRefreshNow)
                AddHotkeyToMenuItem(hwndMenu,
                                    WPMENUID_REFRESH,
                                    ID_XFMI_OFS_REFRESH,
                                    pGlobalSettings->VarMenuOffset);
        }
    } // end if (pGlobalSettings->fShowHotkeysInMenus)
}

typedef struct _SUBCLHOTKEYEF
{
    PFNWP       pfnwpOrig;

    HWND        hwndSet,
                hwndClear;

    USHORT      usFlags;        // in: as in WM_CHAR
    USHORT      usKeyCode;      // in: if KC_VIRTUAL is set, this has usKeyCode;
                                //     otherwise usCharCode

} SUBCLHOTKEYEF, *PSUBCLHOTKEYEF;

/*
 *@@ fnwpFolderHotkeyEntryField:
 *      this is the window proc for the subclassed entry
 *      field on the "Folder Hotkeys" notebook page. We will
 *      intercept all WM_CHAR messages and set the entry field
 *      display to the key description instead of the character.
 *      Moreover, we will update the global folder hotkey array
 *      according to the currently selected listbox item on that
 *      page.
 *
 *@@changed V0.9.0 [umoeller]: renamed from fnwpHotkeyEntryField
 *@@changed V0.9.9 (2001-04-04) [umoeller]: added "set" support
 */

MRESULT EXPENTRY fnwpFolderHotkeyEntryField(HWND hwndEdit, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    // get original wnd proc; this was stored in the
    // window words in xfwps.c
    PSUBCLHOTKEYEF pshef = (PSUBCLHOTKEYEF)WinQueryWindowPtr(hwndEdit, QWL_USER);

    PFNWP OldEditProc = pshef->pfnwpOrig;

    MRESULT mrc = (MPARAM)FALSE; // WM_CHAR not-processed flag

    switch (msg)
    {
        case WM_CHAR:
        {
            // USHORT usCommand;
            USHORT usKeyCode;
            USHORT usFlags    = SHORT1FROMMP(mp1);
            USHORT usch       = SHORT1FROMMP(mp2);
            USHORT usvk       = SHORT2FROMMP(mp2);

            if ((usFlags & KC_KEYUP) == 0)
            {
                if (    (    (usFlags & KC_VIRTUALKEY)
                          && (    (usvk == VK_TAB)
                               || (usvk == VK_BACKTAB)
                             )
                        )
                     || (    ((usFlags & (KC_CTRL | KC_SHIFT | KC_ALT)) == KC_ALT)
                          && (   (WinSendMsg(pshef->hwndSet,
                                             WM_MATCHMNEMONIC,
                                             (MPARAM)usch,
                                             0))
                              || (WinSendMsg(pshef->hwndClear,
                                             WM_MATCHMNEMONIC,
                                             (MPARAM)usch,
                                             0))
                             )
                        )
                   )
                {
                    // pass those to owner
                    WinPostMsg(WinQueryWindow(hwndEdit, QW_OWNER),
                               msg,
                               mp1,
                               mp2);
                }
                /*
                else if (     ((usFlags & KC_VIRTUALKEY) != 0)
                           || ((usFlags & KC_CTRL) != 0)
                           || ((usFlags & KC_ALT) != 0)
                           || (   ((usFlags & KC_VIRTUALKEY) == 0)
                               && (     (usch == 0xEC00)
                                    ||  (usch == 0xED00)
                                    ||  (usch == 0xEE00)
                                  )
                              )
                        )
                */
                else
                {
                    usFlags &= (KC_VIRTUALKEY | KC_CTRL | KC_ALT | KC_SHIFT);
                    if (usFlags & KC_VIRTUALKEY)
                        usKeyCode = usvk;
                    else
                        usKeyCode = usch;

                    if (cmnIsValidHotkey(usFlags,
                                         usKeyCode))
                    {
                        // looks like a valid hotkey:
                        CHAR    szKeyName[100];

                        pshef->usFlags = usFlags;
                        pshef->usKeyCode = usKeyCode;

                        cmnDescribeKey(szKeyName, usFlags, usKeyCode);
                        WinSetWindowText(hwndEdit, szKeyName);

                        WinEnableWindow(pshef->hwndSet, TRUE);
                    }
                    else
                        WinEnableWindow(pshef->hwndSet, FALSE);

                    WinEnableWindow(pshef->hwndClear, TRUE);
                }
            }

            mrc = (MPARAM)TRUE;     // processed

        break; }

        case WM_DESTROY:
            free(pshef);
            mrc = OldEditProc(hwndEdit, msg, mp1, mp2);
        break;

        default:
            mrc = OldEditProc(hwndEdit, msg, mp1, mp2);
    }
    return (mrc);
}

/*
 *@@ fdrHotkeysInitPage:
 *      notebook callback function (notebook.c) for the
 *      "Folder hotkeys" page in the "Workplace Shell" object.
 *      Sets the controls on the page according to the
 *      Global Settings.
 *
 *@@changed V0.9.0 [umoeller]: adjusted function prototype
 */

VOID fdrHotkeysInitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                        ULONG flFlags)        // CBI_* flags (notebook.h)
{
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    HWND    hwndEditField = WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_DESCRIPTION);
    HWND    hwndListbox = WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_LISTBOX);

    SHORT i;

    if (flFlags & CBI_INIT)
    {
        HAB     hab = WinQueryAnchorBlock(pcnbp->hwndDlgPage);
        HMODULE hmod = cmnQueryNLSModuleHandle(FALSE);
        PSUBCLHOTKEYEF pshef;

        if (pcnbp->pUser == NULL)
        {
            // first call: backup Global Settings for "Undo" button;
            // this memory will be freed automatically by the
            // common notebook window function (notebook.c) when
            // the notebook page is destroyed
            pcnbp->pUser = malloc(sizeof(GLOBALSETTINGS));
            memcpy(pcnbp->pUser, pGlobalSettings, sizeof(GLOBALSETTINGS));
            // and also backup the Folder Hotkeys array in the
            // second pointer
            pcnbp->pUser2 = malloc(FLDRHOTKEYSSIZE);
            memcpy(pcnbp->pUser2,
                   fdrQueryFldrHotkeys(),
                   FLDRHOTKEYSSIZE);
        }

        for (i = 0; i < FLDRHOTKEYCOUNT; i++)
        {
            if (WinLoadString(hab, hmod,
                              G_aDescriptions[i].ulStringID,
                              sizeof(G_szLBEntries[i]),
                              G_szLBEntries[i])
                     == 0)
            {
                cmnLog(__FILE__, __LINE__, __FUNCTION__,
                       "Unable to load strings.");
                break;
            }
        }

        pshef = (PSUBCLHOTKEYEF)malloc(sizeof(SUBCLHOTKEYEF));
        if (pshef)
        {
            memset(pshef, 0, sizeof(*pshef));
            WinSetWindowPtr(hwndEditField, QWL_USER, pshef);
            pshef->pfnwpOrig = WinSubclassWindow(hwndEditField, fnwpFolderHotkeyEntryField);

            pshef->hwndSet = WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_SETACCEL);
            pshef->hwndClear = WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_CLEARACCEL);
        }
    }

    if (flFlags & CBI_SET)
    {
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_ACCELERATORS,
                              pGlobalSettings->fFolderHotkeysDefault);

        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_SHOWINMENUS,
                              pGlobalSettings->fShowHotkeysInMenus);

        WinSendMsg(hwndListbox, LM_DELETEALL, 0, 0);
        WinSetWindowText(hwndEditField, "");

        for (i = 0; i < FLDRHOTKEYCOUNT; i++)
        {
            if (G_szLBEntries[i])
            {
                WinSendMsg(hwndListbox,
                           LM_INSERTITEM,
                           (MPARAM)LIT_SORTASCENDING,
                           (MPARAM)G_szLBEntries[i]);
            }
            else break;
        }
    }

    if (flFlags & CBI_ENABLE)
    {
        BOOL fEnable = !(pGlobalSettings->fNoSubclassing);
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_ACCELERATORS, fEnable);
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_LISTBOX, fEnable);
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_CLEARACCEL, fEnable);
    }
}

/*
 *@@ fdrHotkeysItemChanged:
 *      notebook callback function (notebook.c) for the
 *      "Folder hotkeys" page in the "Workplace Shell" object.
 *      Reacts to changes of any of the dialog controls.
 *
 *@@changed V0.9.0 [umoeller]: adjusted function prototype
 *@@changed V0.9.9 (2001-04-04) [umoeller]: added "Set" button
 */

MRESULT fdrHotkeysItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                              ULONG ulItemID,
                              USHORT usNotifyCode,
                              ULONG ulExtra)      // for checkboxes: contains new state
{
    GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(__FILE__, __LINE__, __FUNCTION__);
    MRESULT mrc = (MPARAM)0;

    switch (ulItemID)
    {
        case ID_XSDI_ACCELERATORS:
            pGlobalSettings->fFolderHotkeysDefault = ulExtra;
            cmnStoreGlobalSettings();
        break;

        case ID_XSDI_SHOWINMENUS:
            pGlobalSettings->fShowHotkeysInMenus = ulExtra;
            cmnStoreGlobalSettings();
        break;

        case ID_XSDI_LISTBOX:
            if (usNotifyCode == LN_SELECT)
            {
                // new hotkey description from listbox selected:
                CHAR            szKeyName[200];
                PXFLDHOTKEY     pHotkeyFound;

                // update the Edit field with new
                // key description, but do NOT save settings yet
                pHotkeyFound = FindHotkeyFromLBSel(pcnbp->hwndDlgPage, NULL);
                if ( (pHotkeyFound) && ((ULONG)pHotkeyFound != -1) )
                {
                    cmnDescribeKey(szKeyName,
                                   pHotkeyFound->usFlags,
                                   pHotkeyFound->usKeyCode);
                    winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_CLEARACCEL, TRUE);
                }
                else
                {
                    // not found: set to "not defined"
                    strcpy(szKeyName,
                           cmnGetString(ID_XSSI_NOTDEFINED));

                    winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_CLEARACCEL, FALSE);
                }

                winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_SETACCEL, FALSE);

                // set edit field to description text
                WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XSDI_DESCRIPTION,
                                  szKeyName);
                // enable previously disabled items
                winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_DESCRIPTION, TRUE);
                winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_DESCRIPTION_TX1, TRUE);
            }
        break;

        // we need not handle the entry field, because we
        // have subclassed the window procedure

        /*
         * ID_XSDI_SETACCEL:
         *      "set" button. Copy flags from entry field.
         *
         *      The "Set" button is only enabled if the
         *      entry field has considered the hotkey good.
         */

        case ID_XSDI_SETACCEL:
        {
            HWND hwndEdit = WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_DESCRIPTION);
            PXFLDHOTKEY pHotkeyFound = NULL;
            USHORT usCommand;
            pHotkeyFound = FindHotkeyFromLBSel(pcnbp->hwndDlgPage,
                                               &usCommand);

            if (pHotkeyFound == NULL)
            {
                // no hotkey defined yet: append a new one
                pHotkeyFound = fdrQueryFldrHotkeys();

                // go to the end of the list
                while (pHotkeyFound->usCommand)
                    pHotkeyFound++;

                pHotkeyFound->usCommand = usCommand;
                // set a new list terminator
                (*(pHotkeyFound + 1)).usCommand = 0;
            }

            if ((ULONG)pHotkeyFound != -1)
            {
                // no error: set hotkey data
                CHAR szKeyName[200];
                PSUBCLHOTKEYEF pshef = (PSUBCLHOTKEYEF)WinQueryWindowPtr(hwndEdit, QWL_USER);

                pHotkeyFound->usFlags      = pshef->usFlags;
                pHotkeyFound->usKeyCode    = pshef->usKeyCode;

                #ifdef DEBUG_KEYS
                    _Pmpf(("Stored usFlags = 0x%lX, usKeyCode = 0x%lX", usFlags, usKeyCode));
                #endif

                // show description
                cmnDescribeKey(szKeyName, pshef->usFlags, pshef->usKeyCode);
                WinSetWindowText(hwndEdit, szKeyName);
                WinEnableWindow(pshef->hwndSet, FALSE);
                WinEnableWindow(pshef->hwndClear, TRUE);

                // save hotkeys to INIs
                fdrStoreFldrHotkeys();
            }
        break; }

        case ID_XSDI_CLEARACCEL:
        {
            // "Clear" button:
            USHORT      usCommand;
            PXFLDHOTKEY pHotkeyFound;

            pHotkeyFound = FindHotkeyFromLBSel(pcnbp->hwndDlgPage, &usCommand);

            if ( (pHotkeyFound) && ((ULONG)pHotkeyFound != -1) )
            {
                // accelerator defined:
                PXFLDHOTKEY pHotkey = pHotkeyFound;

                // in order to delete the marked accelerator,
                // move all following hotkeys in the global
                // hotkeys table one position ahead
                while (pHotkey->usCommand)
                {
                    memcpy(pHotkey, pHotkey+1, sizeof(XFLDHOTKEY));
                    pHotkey++;
                }

                WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XSDI_DESCRIPTION,
                                  cmnGetString(ID_XSSI_NOTDEFINED));
                winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_SETACCEL, FALSE);
                winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_CLEARACCEL, FALSE);
                fdrStoreFldrHotkeys();
            }
        break; }

        case DID_UNDO:
        {
            // "Undo" button: get pointer to backed-up Global Settings
            PCGLOBALSETTINGS pGSBackup = (PCGLOBALSETTINGS)(pcnbp->pUser);

            // and restore the settings for this page
            pGlobalSettings->fFolderHotkeysDefault = pGSBackup->fFolderHotkeysDefault;

            // here, also restore the backed-up FolderHotkeys array
            // second pointer
            memcpy(fdrQueryFldrHotkeys(), pcnbp->pUser2, FLDRHOTKEYSSIZE);

            // update the display by calling the INIT callback
            pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
            fdrStoreFldrHotkeys();
        break; }

        case DID_DEFAULT:
        {
            // set the default settings for this settings page
            // (this is in common.c because it's also used at
            // WPS startup)
            cmnSetDefaultSettings(pcnbp->ulPageID);
            fdrLoadDefaultFldrHotkeys();
            // update the display by calling the INIT callback
            pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
            cmnStoreGlobalSettings();
            fdrStoreFldrHotkeys();
        break; }
    }

    cmnUnlockGlobalSettings();

    return (mrc);
}


