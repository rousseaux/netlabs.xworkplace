
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
 *      Copyright (C) 1997-99 Ulrich M”ller.
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
XFLDHOTKEY     FolderHotkeys[FLDRHOTKEYCOUNT];

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
    return (&FolderHotkeys[0]);
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
    FolderHotkeys[0].usKeyCode  = (USHORT)'a';
    FolderHotkeys[0].usFlags    = KC_CTRL;
    FolderHotkeys[0].usCommand  = WPMENUID_SELALL;

    // F5
    FolderHotkeys[1].usKeyCode  = VK_F5;
    FolderHotkeys[1].usFlags    = KC_VIRTUALKEY;
    FolderHotkeys[1].usCommand  = WPMENUID_REFRESH,

    // Backspace
    FolderHotkeys[2].usKeyCode  = VK_BACKSPACE;
    FolderHotkeys[2].usFlags    = KC_VIRTUALKEY;
    FolderHotkeys[2].usCommand  = ID_XFMI_OFS_OPENPARENT;

    // Ctrl+D: De-select all
    FolderHotkeys[3].usKeyCode  = (USHORT)'d';
    FolderHotkeys[3].usFlags    = KC_CTRL;
    FolderHotkeys[3].usCommand  = WPMENUID_DESELALL;

    // Ctrl+Shift+D: Details view
    FolderHotkeys[4].usKeyCode  = (USHORT)'D';
    FolderHotkeys[4].usFlags    = KC_CTRL+KC_SHIFT;
    FolderHotkeys[4].usCommand  = WPMENUID_DETAILS;

    // Ctrl+Shift+I: Icon  view
    FolderHotkeys[5].usKeyCode  = (USHORT)'I';
    FolderHotkeys[5].usFlags    = KC_CTRL+KC_SHIFT;
    FolderHotkeys[5].usCommand  = WPMENUID_ICON;

    // Ctrl+Shift+S: Open Settings
    FolderHotkeys[6].usKeyCode  = (USHORT)'S';
    FolderHotkeys[6].usFlags    = KC_CTRL+KC_SHIFT;
    FolderHotkeys[6].usCommand  = WPMENUID_PROPERTIES;

    // Ctrl+N: Sort by name
    FolderHotkeys[7].usKeyCode  = (USHORT)'n';
    FolderHotkeys[7].usFlags    = KC_CTRL;
    FolderHotkeys[7].usCommand  = ID_WPMI_SORTBYNAME;

    // Ctrl+Z: Sort by size
    FolderHotkeys[8].usKeyCode  = (USHORT)'z';
    FolderHotkeys[8].usFlags    = KC_CTRL;
    FolderHotkeys[8].usCommand  = ID_WPMI_SORTBYSIZE;

    // Ctrl+E: Sort by extension (NPS)
    FolderHotkeys[9].usKeyCode  = (USHORT)'e';
    FolderHotkeys[9].usFlags    = KC_CTRL;
    FolderHotkeys[9].usCommand  = ID_XFMI_OFS_SORTBYEXT;

    // Ctrl+W: Sort by write date
    FolderHotkeys[10].usKeyCode  = (USHORT)'w';
    FolderHotkeys[10].usFlags    = KC_CTRL;
    FolderHotkeys[10].usCommand  = ID_WPMI_SORTBYWRITEDATE;

    // Ctrl+Y: Sort by type
    FolderHotkeys[11].usKeyCode  = (USHORT)'y';
    FolderHotkeys[11].usFlags    = KC_CTRL;
    FolderHotkeys[11].usCommand  = ID_WPMI_SORTBYTYPE;

    // Shift+Backspace
    FolderHotkeys[12].usKeyCode  = VK_BACKSPACE;
    FolderHotkeys[12].usFlags    = KC_VIRTUALKEY+KC_SHIFT;
    FolderHotkeys[12].usCommand  = ID_XFMI_OFS_OPENPARENTANDCLOSE;

    // Ctrl+S: Select by name
    FolderHotkeys[13].usKeyCode  = (USHORT)'s';
    FolderHotkeys[13].usFlags    = KC_CTRL;
    FolderHotkeys[13].usCommand  = ID_XFMI_OFS_SELECTSOME;

    // Ctrl+insert: copy filename (w/out path)
    FolderHotkeys[14].usKeyCode  = VK_INSERT;
    FolderHotkeys[14].usFlags    = KC_VIRTUALKEY+KC_CTRL;
    FolderHotkeys[14].usCommand  = ID_XFMI_OFS_COPYFILENAME_SHORT;

    // list terminator
    FolderHotkeys[15].usCommand = 0;
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
    ULONG ulCopied2 = sizeof(FolderHotkeys);
    if (!PrfQueryProfileData(HINI_USERPROFILE, INIAPP_XWORKPLACE, INIKEY_ACCELERATORS,
                &FolderHotkeys, &ulCopied2))
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

    // store only the accels that are actually used
    while (FolderHotkeys[i2].usCommand)
        i2++;

    PrfWriteProfileData(HINI_USERPROFILE, INIAPP_XWORKPLACE, INIKEY_ACCELERATORS,
        &FolderHotkeys, (i2+1) * sizeof(XFLDHOTKEY));
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
 */

BOOL fdrProcessFldrHotkey(HWND hwndFrame,   // in: folder frame
                          MPARAM mp1,       // in: as in WM_CHAR
                          MPARAM mp2)       // in: as in WM_CHAR
{
    USHORT  us;

    USHORT usFlags    = SHORT1FROMMP(mp1);
    USHORT usch       = SHORT1FROMMP(mp2);
    USHORT usvk       = SHORT2FROMMP(mp2);
    USHORT usKeyCode  = 0;

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

        if (usFlags & KC_VIRTUALKEY)
            usKeyCode = usvk;
        else
            usKeyCode = usch;

        // filter out unwanted flags
        usFlags &= (KC_VIRTUALKEY | KC_CTRL | KC_ALT | KC_SHIFT);
        us = 0;

        #ifdef DEBUG_KEYS
            _Pmpf(("fdrProcessFldrHotkey: usKeyCode: 0x%lX, usFlags: 0x%lX", usKeyCode, usFlags));
        #endif

        // now go through the global accelerator list and check
        // if the pressed key was assigned an action to
        while (FolderHotkeys[us].usCommand)
        {
            USHORT usCommand;
            if (      (FolderHotkeys[us].usFlags == usFlags)
                   && (FolderHotkeys[us].usKeyCode == usKeyCode)
               )
            {   // OK: this is a hotkey; find the corresponding
                // "command" (= menu ID) and post it to the frame
                // window, which will execute it
                usCommand = FolderHotkeys[us].usCommand;

                if (    (usCommand >= WPMENUID_USER)
                     && (usCommand < WPMENUID_USER+FIRST_VARIABLE)
                   )
                {
                    // it's one of the "variable" menu items:
                    // add the global variable menu offset
                    PCGLOBALSETTINGS     pGlobalSettings = cmnQueryGlobalSettings();
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

                return (TRUE);
            }
            us++;
        }
    }
    return (FALSE);
}

/* ******************************************************************
 *                                                                  *
 *   Notebook callbacks (notebook.c) for XFldWPS "Hotkeys" page     *
 *                                                                  *
 ********************************************************************/

/*
 *  Folder hotkeys descriptions arrays:
 *      every item these arrays must match exactly one
 *      item in the ulLBCommands array in common.c.
 */

CHAR  szLBEntries[FLDRHOTKEYCOUNT][MAXLBENTRYLENGTH];

ULONG szLBStringIDs[FLDRHOTKEYCOUNT] =
    {
         ID_XSSI_LB_REFRESHNOW          ,
         ID_XSSI_LB_SNAPTOGRID          ,
         ID_XSSI_LB_SELECTALL           ,
         ID_XSSI_LB_OPENPARENTFOLDER    ,

         ID_XSSI_LB_OPENSETTINGSNOTEBOOK,
         ID_XSSI_LB_OPENNEWDETAILSVIEW  ,
         ID_XSSI_LB_OPENNEWICONVIEW     ,
         ID_XSSI_LB_DESELECTALL         ,
         ID_XSSI_LB_OPENNEWTREEVIEW     ,

         ID_XSSI_LB_FIND                ,

         ID_XSSI_LB_PICKUP              ,
         ID_XSSI_LB_PICKUPCANCELDRAG    ,

         ID_XSSI_LB_SORTBYNAME          ,
         ID_XSSI_LB_SORTBYSIZE          ,
         ID_XSSI_LB_SORTBYTYPE          ,
         ID_XSSI_LB_SORTBYREALNAME      ,
         ID_XSSI_LB_SORTBYWRITEDATE     ,
         ID_XSSI_LB_SORTBYACCESSDATE    ,
         ID_XSSI_LB_SORTBYCREATIONDATE  ,

         ID_XSSI_LB_SWITCHTOICONVIEW    ,
         ID_XSSI_LB_SWITCHTODETAILSVIEW ,
         ID_XSSI_LB_SWITCHTOTREEVIEW    ,

         ID_XSSI_LB_ARRANGEDEFAULT      ,
         ID_XSSI_LB_ARRANGEFROMTOP      ,
         ID_XSSI_LB_ARRANGEFROMLEFT     ,
         ID_XSSI_LB_ARRANGEFROMRIGHT    ,
         ID_XSSI_LB_ARRANGEFROMBOTTOM   ,
         ID_XSSI_LB_ARRANGEPERIMETER    ,
         ID_XSSI_LB_ARRANGEHORIZONTALLY ,
         ID_XSSI_LB_ARRANGEVERTICALLY   ,

         ID_XSSI_LB_INSERT              ,

         ID_XSSI_LB_SORTBYEXTENSION     ,
         ID_XSSI_LB_OPENPARENTFOLDERANDCLOSE,

         ID_XSSI_LB_CLOSEWINDOW,
         ID_XSSI_LB_SELECTSOME,
         ID_XSSI_LB_SORTFOLDERSFIRST,
         ID_XSSI_LB_SORTBYCLASS,

         ID_XSSI_LB_CONTEXTMENU,
         ID_XSSI_LB_TASKLIST,

         ID_XSSI_LB_COPYFILENAME_SHORT,
         ID_XSSI_LB_COPYFILENAME_FULL

    };

ULONG ulLBCommands[FLDRHOTKEYCOUNT] =
    {
        WPMENUID_REFRESH,
        ID_XFMI_OFS_SNAPTOGRID,
        WPMENUID_SELALL,
        ID_XFMI_OFS_OPENPARENT,

        WPMENUID_PROPERTIES,
        WPMENUID_DETAILS,
        WPMENUID_ICON,
        WPMENUID_DESELALL,
        WPMENUID_TREE,

        WPMENUID_FIND,

        WPMENUID_PICKUP,
        WPMENUID_PUTDOWN_CANCEL,

        ID_WPMI_SORTBYNAME,
        ID_WPMI_SORTBYSIZE,
        ID_WPMI_SORTBYTYPE,
        ID_WPMI_SORTBYREALNAME,
        ID_WPMI_SORTBYWRITEDATE,
        ID_WPMI_SORTBYACCESSDATE,
        ID_WPMI_SORTBYCREATIONDATE,

        ID_WPMI_SHOWICONVIEW,
        ID_WPMI_SHOWDETAILSVIEW,
        ID_WPMI_SHOWTREEVIEW,

        WPMENUID_ARRANGE,
        ID_WPMI_ARRANGEFROMTOP,
        ID_WPMI_ARRANGEFROMLEFT,
        ID_WPMI_ARRANGEFROMRIGHT,
        ID_WPMI_ARRANGEFROMBOTTOM,
        ID_WPMI_ARRANGEPERIMETER,
        ID_WPMI_ARRANGEHORIZONTALLY,
        ID_WPMI_ARRANGEVERTICALLY,

        ID_WPMI_PASTE,

        ID_XFMI_OFS_SORTBYEXT,
        ID_XFMI_OFS_OPENPARENTANDCLOSE,

        ID_XFMI_OFS_CLOSE,
        ID_XFMI_OFS_SELECTSOME,
        ID_XFMI_OFS_SORTFOLDERSFIRST,
        ID_XFMI_OFS_SORTBYCLASS,

        ID_XFMI_OFS_CONTEXTMENU,
        0x8011,                     // show task list (sysmenu)

        ID_XFMI_OFS_COPYFILENAME_SHORT,
        ID_XFMI_OFS_COPYFILENAME_FULL
    };

/*
 *@@ FindHotkeyFromLBSel:
 *      this subroutine finds an index to the global
 *      FolderHotkeys[] array according to the currently
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
        if (strcmp(szTemp, szLBEntries[i]) == 0)
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
            if (pHotkey->usCommand == ulLBCommands[i])
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
            *pusCommand = ulLBCommands[i];

        return (pHotkeyFound);
    }
    else
        return (PXFLDHOTKEY)-1;     // LB entry not found
}

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
 */

MRESULT EXPENTRY fnwpFolderHotkeyEntryField(HWND hwndEdit, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    // get original wnd proc; this was stored in the
    // window words in xfwps.c
    PFNWP       OldEditProc = (PFNWP)WinQueryWindowULong(hwndEdit, QWL_USER);

    MRESULT mrc = (MPARAM)FALSE; // WM_CHAR not-processed flag

    switch (msg)
    {
        case WM_CHAR:
        {
            USHORT usCommand;
            USHORT usKeyCode;
            USHORT usFlags    = SHORT1FROMMP(mp1);
            USHORT usch       = SHORT1FROMMP(mp2);
            USHORT usvk       = SHORT2FROMMP(mp2);

            if (    ((usFlags & KC_KEYUP) == 0)
                &&  (     ((usFlags & KC_VIRTUALKEY) != 0)
                       || ((usFlags & KC_CTRL) != 0)
                       || ((usFlags & KC_ALT) != 0)
                       || (   ((usFlags & KC_VIRTUALKEY) == 0)
                           && (     (usch == 0xEC00)
                                ||  (usch == 0xED00)
                                ||  (usch == 0xEE00)
                              )
                          )
                    )
               )
            {
                PXFLDHOTKEY pHotkeyFound = NULL;

                usFlags &= (KC_VIRTUALKEY | KC_CTRL | KC_ALT | KC_SHIFT);
                if (usFlags & KC_VIRTUALKEY)
                    usKeyCode = usvk;
                else
                    usKeyCode = usch;

                pHotkeyFound = FindHotkeyFromLBSel(WinQueryWindow(hwndEdit, QW_PARENT),
                                             &usCommand);

                #ifdef DEBUG_KEYS
                    _Pmpf(("List box index: %d", i));
                #endif

                if (pHotkeyFound == NULL)
                {
                    // no hotkey defined yet: append a new one
                    pHotkeyFound = fdrQueryFldrHotkeys();

                    // go to the end of the list
                    while (pHotkeyFound->usCommand)
                        pHotkeyFound++;

                    pHotkeyFound->usCommand = usCommand;
                    // set a new list terminator
                    (*(pHotkeyFound+1)).usCommand = 0;

                }

                if ((ULONG)pHotkeyFound != -1)
                {
                    // no error: set hotkey data
                    CHAR szKeyName[200];
                    pHotkeyFound->usFlags      = usFlags;
                    pHotkeyFound->usKeyCode    = usKeyCode;

                    #ifdef DEBUG_KEYS
                        _Pmpf(("Stored usFlags = 0x%lX, usKeyCode = 0x%lX", usFlags, usKeyCode));
                    #endif

                    // show description
                    cmnDescribeKey(szKeyName, usFlags, usKeyCode);
                    WinSetWindowText(hwndEdit, szKeyName);
                    WinEnableControl(WinQueryWindow(hwndEdit, QW_PARENT),
                                     ID_XSDI_CLEARACCEL,
                                     TRUE);

                    // save hotkeys to INIs
                    fdrStoreFldrHotkeys();

                    mrc = (MPARAM)TRUE; // WM_CHAR processed flag;
                }
            }
        break; }

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

    HWND    hwndEditField = WinWindowFromID(pcnbp->hwndPage, ID_XSDI_DESCRIPTION);
    HWND    hwndListbox = WinWindowFromID(pcnbp->hwndPage, ID_XSDI_LISTBOX);

    SHORT i;

    if (flFlags & CBI_INIT)
    {
        HAB     hab = WinQueryAnchorBlock(pcnbp->hwndPage);
        HMODULE hmod = cmnQueryNLSModuleHandle(FALSE);
        PFNWP   OldEditProc;

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
                              szLBStringIDs[i],
                              sizeof(szLBEntries[i]),
                              szLBEntries[i])
                     == 0)
            {
                DebugBox("XFolder", "Unable to load strings.");
                break;
            }
        }

        OldEditProc = WinSubclassWindow(hwndEditField, fnwpFolderHotkeyEntryField);
        WinSetWindowULong(hwndEditField, QWL_USER, (ULONG)OldEditProc);
    }

    if (flFlags & CBI_SET)
    {
        winhSetDlgItemChecked(pcnbp->hwndPage, ID_XSDI_ACCELERATORS, pGlobalSettings->fFolderHotkeysDefault);

        WinSendMsg(hwndListbox, LM_DELETEALL, 0, 0);
        WinSetWindowText(hwndEditField, "");

        for (i = 0; i < FLDRHOTKEYCOUNT; i++)
        {
            if (szLBEntries[i])
            {
                WinSendMsg(hwndListbox,
                           LM_INSERTITEM,
                           (MPARAM)LIT_SORTASCENDING,
                           (MPARAM)szLBEntries[i]);
            }
            else break;
        }
    }

    if (flFlags & CBI_ENABLE)
    {
        BOOL fEnable = !(pGlobalSettings->NoSubclassing);
        WinEnableControl(pcnbp->hwndPage, ID_XSDI_ACCELERATORS, fEnable);
        WinEnableControl(pcnbp->hwndPage, ID_XSDI_LISTBOX, fEnable);
        WinEnableControl(pcnbp->hwndPage, ID_XSDI_CLEARACCEL, fEnable);
    }
}

/*
 *@@ fdrHotkeysItemChanged:
 *      notebook callback function (notebook.c) for the
 *      "Snap to grid" page in the "Workplace Shell" object.
 *      Reacts to changes of any of the dialog controls.
 *
 *@@changed V0.9.0 [umoeller]: adjusted function prototype
 */

MRESULT fdrHotkeysItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                              USHORT usItemID,
                              USHORT usNotifyCode,
                              ULONG ulExtra)      // for checkboxes: contains new state
{
    GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(5000);
    MRESULT mrc = (MPARAM)0;

    switch (usItemID)
    {
        case ID_XSDI_ACCELERATORS:
            pGlobalSettings->fFolderHotkeysDefault = ulExtra;
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
                pHotkeyFound = FindHotkeyFromLBSel(pcnbp->hwndPage, NULL);
                if ( (pHotkeyFound) && ((ULONG)pHotkeyFound != -1) )
                {
                    cmnDescribeKey(szKeyName,
                                   pHotkeyFound->usFlags,
                                   pHotkeyFound->usKeyCode);
                    WinEnableControl(pcnbp->hwndPage, ID_XSDI_CLEARACCEL, TRUE);
                }
                else
                {
                    // not found: set to "not defined"
                    strcpy(szKeyName,
                        (cmnQueryNLSStrings())->pszNotDefined);

                    WinEnableControl(pcnbp->hwndPage, ID_XSDI_CLEARACCEL, FALSE);
                }

                // set edit field to description text
                WinSetDlgItemText(pcnbp->hwndPage, ID_XSDI_DESCRIPTION,
                                  szKeyName);
                // enable previously disabled items
                WinEnableControl(pcnbp->hwndPage, ID_XSDI_DESCRIPTION, TRUE);
                WinEnableControl(pcnbp->hwndPage, ID_XSDI_DESCRIPTION_TX1, TRUE);
            }
        break;

        // we need not handle the entry field, because the
        // subclassed window procedure for that is smart
        // enough to update the hotkeys data itself

        case ID_XSDI_CLEARACCEL:
        {
            // "Clear" button:
            USHORT      usCommand;
            PXFLDHOTKEY pHotkeyFound;

            pHotkeyFound = FindHotkeyFromLBSel(pcnbp->hwndPage, &usCommand);

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

                WinSetDlgItemText(pcnbp->hwndPage, ID_XSDI_DESCRIPTION,
                            (cmnQueryNLSStrings())->pszNotDefined);
                WinEnableControl(pcnbp->hwndPage, ID_XSDI_CLEARACCEL, FALSE);
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
            (*(pcnbp->pfncbInitPage))(pcnbp, CBI_SET | CBI_ENABLE);
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
            (*(pcnbp->pfncbInitPage))(pcnbp, CBI_SET | CBI_ENABLE);
            cmnStoreGlobalSettings();
            fdrStoreFldrHotkeys();
        break; }
    }

    cmnUnlockGlobalSettings();

    return (mrc);
}


