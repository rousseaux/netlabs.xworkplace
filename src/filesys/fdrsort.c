
/*
 *@@sourcefile fdrsort.c:
 *      this file contains the extended folder sort code,
 *      which has been mostly rewritten with V0.9.12.
 *
 *      <B>Introduction to WPS Sorting</B>
 *
 *      The WPS sort functions are a mess, and I suppose
 *      IBM knows that, since they have hardly documented
 *      them at all. So here's what I found out.
 *
 *      Normally, the WPS relates sort criteria to object
 *      details. That is, to implement a sort criterion,
 *      you have to have a corresponding object detail,
 *      as defined in the wpclsQueryDetailsInfo and
 *      wpQueryDetailsData methods. This is superficially
 *      explained in the "WPS Programming Guide", in
 *      "Object Criteria: Details methods".
 *
 *      There are many more object details than those shown
 *      in folder Details views. I have counted 23 here
 *      (but your results may vary), and since there are
 *      only 13 columns in Details view, there should be
 *      about 10 invisible details. (Side note: You can
 *      view all of them by turning to the "Include" page
 *      of any folder and pressing the "Add..." button.)
 *
 *      Most of the details are defined in WPFileSystem's
 *      override of wpclsQueryDetailsInfo, and if
 *      CLASSFIELDINFO.flData has the SORTBY_SUPPORTED flag
 *      set (even if the details column is never visible),
 *      the WPS will make this detail available as a sort
 *      criterion. That is, if such a sort flag is set for
 *      an object detail, the WPS inserts the column title
 *      in the "Sort" submenu, shows it on the "Sort" page,
 *      and sets some corresponding comparison function for
 *      the folder containers automatically.
 *
 *      The details to be taken into account for sorting are
 *      determined by the folder's "sort class". For example,
 *      the trash can's sort class is set to XWPTrashObject
 *      (instead of the standard WPFileSystem) and boom! we
 *      get the trash object details in the sort menu.
 *
 *      Over the years, I have tried several approaches to
 *      replace folder sorting. I ran into the following
 *      problems:
 *
 *      1.  It is impossible to add new default details to a
 *          replacement class of WPFileSystem. Even if
 *          WPFileSystem is replaced with another class,
 *          wpQueryFldrSortClass and wpQueryFldrDetailsClass
 *          do not return the replacement class object, but
 *          the original WPFileSystem class object. I have
 *          then tried to replace these two methods too, and
 *          details/sorting would still not always work. There
 *          seem to be some ugly kludges in the WPS internally.
 *
 *      2.  Two sort criteria ("Name" and "Type") do not
 *          correspond to any details columns. Normally, sort
 *          criteria are specified by passing the corresponding
 *          details column to a sort function (see
 *          wpIsSortAttribAvailable, for example). However,
 *          these two criteria have indices of "-2" and "-1".
 *
 *          So special checks had to be added for these indices,
 *          and XWP even adds two more negative indices. See
 *          below.
 *
 *      3.  The documentation for CLASSFIELDINFO is incomplete.
 *
 *      4.  The wpQueryFldrSort and wpSetFldrSort methods are
 *          completely obscure. The prototypes are really useless,
 *          since the WPS uses an undocumented structure for
 *          sorting instead, which we have defined as WPSSORTINFO
 *          below.
 *
 *      5.  The most important limitation was however that there
 *          are no default settings for sorting. The default
 *          sort criterion is always set to "Name", and
 *          "Always sort" is always off. This is hard-coded into
 *          WPFolder and cannot be changed. There is not even an
 *          instance method for querying or setting the "Always sort"
 *          flag, let alone a class method for the default values.
 *          The only documented thing is the ALWAYSSORT setup
 *          string.
 *
 *      Besides, the whole concept is so complicated that doubt
 *      many users even know what "sort classes" or "details classes"
 *      are about, even though they may set these in a folder's
 *      settings notebook.
 *
 *      <B>The XWorkplace Approach</B>
 *
 *      It's basically a "brute force" method. XWorkplace uses its
 *      own set of global and instance settings for both the default
 *      sort criterion and the "always sort" flag and then _always_
 *      sets its own comparison function directly on the container.
 *      As a result, only XWP does all the sorting now.
 *
 *      To be able to still get the WPSSORTINFO which sits somewhere
 *      in the WPFolder instance data, we added XFolder::wpRestoreData,
 *      which gets called when an object is awakened. Since the caller
 *      always passes a block of memory to which wpRestoreData should
 *      write if the data was found, we can intercept that pointer
 *      and store it in XWorkplace's instance data. We can therefore
 *      manipulate the "Always sort" flag in there also.
 *
 *      So, what's new with 0.9.12?
 *
 *      The old folder sort code (back from XFolder, used
 *      before 0.9.12) simply assumed that all items in
 *      the "sort" menu were the same for all folders and
 *      then intercepted the standard sort menu item IDs
 *      and set the corresponding comparison function
 *      from shared\cnrsort.c on the PM container directly.
 *
 *      The default WPS "Sort" menu has the following items:
 *
 *      --  Name                (0x1770)
 *      --  Type                (0x1771)
 *      --  Real name           (0x1777)
 *      --  Size                (0x1778)
 *      --  last write date     (0x1779)
 *      --  last access date    (0x177B)
 *      --  creation date       (0x177D)
 *
 *      The old code failed as soon as some folder used a
 *      non-standard sort class (that is, something other
 *      than WPFileSystem), and the menu items were then
 *      different. The most annoying example of this was
 *      the trash can (but the font folder as well).
 *
 *      I have now finally figured out how the stupid sort
 *      criteria relate to container sort comparison funcs
 *      so we can
 *
 *      1)  still implement global sort settings (default
 *          sort criterion and "always sort" flag as with
 *          XFolder),
 *
 *      2)  but implement sorting for non-standard sort
 *          classes too.
 *
 *      3)  In addition, we now support a global "folders
 *          first" flag.
 *
 *      Two things are changed by XWP extended folder sorting,
 *      and each needs different precautions.
 *
 *      1)  New sort criteria.
 *
 *          XWP introduces "sort by class" and "sort by extension".
 *          Besides, there is a flag for "sort folders first" always,
 *          in addition to the default sort criterion.
 *
 *          As a result, we need a common function for determining
 *          the container comparison func according for a folder
 *          (which will be determined from the folder instance
 *          settings, see below). This is fdrQuerySortFunc in this
 *          file. At all cost, we must set the container comparison
 *          func ourselves and not let the WPS do it, because the WPS
 *          doesn't know about the additional criteria. (This doesn't
 *          always work, but eventually we WILL set the sort func.)
 *
 *          When a folder is opened, the XWP sort func is then set
 *          from fdrManipulateNewView.
 *
 *      2)  Sort settings.
 *
 *          This is particularly difficult, because WPFolder stores
 *          a WPSSORTINFO structure with the instance data. This
 *          thing is very obscure because it even contains a pointer
 *          to the comparison func. While I have now found that the
 *          WPS needs this for getting the details data from the
 *          object records, this pointer even gets saved to the
 *          folders EA's then. Whatever.
 *
 *          We cannot hack the "default sort" field in that structure
 *          because the WPS will reference that internally all the
 *          time to determine the cnr comparison func. If we give
 *          unknown fields in that function, we are asking for trouble,
 *          since we CANNOT suppress the WPS sort code in all situations...
 *          much of it is hidden in undocumented methods which we cannot
 *          override.
 *
 *          As a result, we need our own XFolder instance variables
 *          for the default sort criterion and the "always sort"
 *          flag (see xfldr.idl).
 *
 *          In addition, we need to hack the "always sort" flag in the
 *          WPSSORTINFO structure to support global "always sort", but
 *          we will need to keep track of whether this flag is set
 *          explicitly for a folder or because the default flag was set.
 *
 *      Note that the object hotkeys for folder sorting still work
 *      even though they still post the hard-coded menu item IDs
 *      from the "Sort" menu as a folder WM_COMMAND. They might NOT
 *      work for non-default sort classes.
 *
 *@@header "filesys\folder.h"
 *@@added V0.9.12 (2001-05-18) [umoeller]
 */

/*
 *      Copyright (C) 1997-2001 Ulrich M”ller.
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
#define INCL_WINMENUS
#define INCL_WINSTDCNR
#define INCL_WINPOINTERS
#include <os2.h>

// C library headers
#include <stdio.h>              // needed for except.h
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\cnrh.h"               // container helper routines
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\stringh.h"            // string helper routines
#include "helpers\winh.h"               // PM helper routines

#include "xfldr.ih"                     // XFolder

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\cnrsort.h"             // container sort comparison functions
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

#include "filesys\folder.h"             // XFolder implementation

#pragma hdrstop                         // VAC++ keeps crashing otherwise

/* ******************************************************************
 *
 *   Private definitions
 *
 ********************************************************************/

// these two are constant, no matter what sort class
#define WPMENUID_SORTBYNAME             0x1770      // "-2" sort criterion
#define WPMENUID_SORTBYTYPE             0x1771      // "-1" sort criterion

/*
 *@@ WPSSORTINFO:
 *      structure used internally by the WPS
 *      for sorting. This is undocumented and
 *      has been provided by Chris Wohlgemuth.
 *
 *      This is what wpQuerySortInfo really returns.
 *
 *      We intercept the address of this structure
 *      in the WPFolder instance data in
 *      XFolder::wpRestoreData and store the pointer
 *      in the XFolder instance data.
 *
 *@@added V0.9.12 (2001-05-18) [umoeller]
 */

typedef struct _WPSSORTINFO
{
    LONG       lDefaultSort;     // default sort column index
    BOOL       fAlwaysSort;      // "always maintain sort order"
    LONG       lCurrentSort;     // current sort column index
    PFNCOMPARE pfnCompare;       // WPS comparison func called by fnCompareDetailsColumn
    ULONG      ulFieldOffset;    // field offset to compare
    M_WPObject *Class;           // sort class
} WPSSORTINFO, *PWPSSORTINFO;

/* ******************************************************************
 *
 *   Modify-menu funcs
 *
 ********************************************************************/

/*
 *@@ CheckDefaultSortItem:
 *      called from various menu functions to
 *      check the default item in the "sort" menu.
 *
 *@@added V0.9.12 (2001-05-18) [umoeller]
 */

VOID CheckDefaultSortItem(HWND hwndSortMenu,
                          LONG lSort)
{
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    SHORT sDefID;

    winhSetMenuItemChecked(hwndSortMenu,
                           WinSendMsg(hwndSortMenu,
                                      MM_QUERYDEFAULTITEMID,
                                      MPNULL, MPNULL), // find current default
                           FALSE);                     // uncheck

    // we need to differentiate here:

    // -- if it's "sort by class" or "sort by extension",
    //    which have been added above, we must set this
    //    explicitly

    switch (lSort)
    {
        case -1:
            sDefID = WPMENUID_SORTBYTYPE;
        break;

        case -2:
            sDefID = WPMENUID_SORTBYNAME;
        break;

        case -3:
            sDefID = pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_SORTBYCLASS;
        break;

        case -4:
            sDefID = pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_SORTBYEXT;
        break;

        default:
            sDefID =   lSort
                     + 2
                     + WPMENUID_SORTBYNAME;
        break;
    }

    WinSendMsg(hwndSortMenu,
               MM_SETDEFAULTITEMID,
               (MPARAM)sDefID,
               (MPARAM)NULL);
}

/*
 *@@ fdrModifySortMenu:
 *      adds the new sort items into the given menu.
 *
 *      This gets called from two locations:
 *
 *      -- from XFolder::wpModifyPopupMenu to hack the
 *         "Sort" menu of a folder context menu;
 *         in this case, hwndMenuWithSortSubmenu is the
 *         main context menu;
 *
 *      -- from the subclassed folder proc to hack the
 *         "Sort" submenu of a folder menu _bar_;
 *         in this case, hwndMenuWithSortSubmenu is the
 *         "View" pulldown, which has the "Sort" menu
 *         in turn.
 *
 *@@added V0.9.12 (2001-05-18) [umoeller]
 */

BOOL fdrModifySortMenu(WPFolder *somSelf,
                       HWND hwndMenuWithSortSubmenu)
{
    BOOL brc = FALSE;
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    if (pGlobalSettings->ExtFolderSort)
    {
        MENUITEM mi;

        if (winhQueryMenuItem(hwndMenuWithSortSubmenu,
                              WPMENUID_SORT,
                              FALSE,
                              &mi))
        {
            HWND    hwndSortMenu = mi.hwndSubMenu;

            XFolderData *somThis = XFolderGetData(somSelf);

            // cast pointer to WPFolder-internal sort data
            // PWPSSORTINFO psi = (PWPSSORTINFO)_pFolderSortInfo;
            SHORT sDefID;

            if (winhQueryMenuItem(hwndSortMenu,
                                  WPMENUID_SORTBYTYPE,
                                  FALSE,
                                  &mi))
            {
                BOOL    f;

                // "sort by class"
                winhInsertMenuItem(hwndSortMenu,
                                   mi.iPosition + 1,            // behind "sort by type"
                                   pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_SORTBYCLASS,
                                   cmnGetString(ID_XSSI_SV_CLASS), // pszSortByClass
                                   MIS_TEXT,
                                   0);

                // "sort by extension"
                winhInsertMenuItem(hwndSortMenu,
                                   mi.iPosition + 2,
                                   (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_SORTBYEXT),
                                   cmnGetString(ID_XSSI_SV_EXT), // pszSortByExt
                                   MIS_TEXT,
                                   0);

                brc = TRUE;

                // now check the default sort item...
                CheckDefaultSortItem(hwndSortMenu,
                                     (_lDefaultSort == SET_DEFAULT)
                                        ? pGlobalSettings->lDefaultSort
                                        : _lDefaultSort);

                // add "folders first"
                winhInsertMenuSeparator(hwndSortMenu,
                                        MIT_END,
                                        pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_SEPARATOR);
                f = (_lFoldersFirst == SET_DEFAULT)
                        ? pGlobalSettings->fFoldersFirst
                        : _lFoldersFirst;
                winhInsertMenuItem(hwndSortMenu,
                                   MIT_END,
                                   pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_SORTFOLDERSFIRST,
                                   cmnGetString(ID_XSSI_SV_FOLDERSFIRST),
                                   MIS_TEXT,
                                   (f) ? MIA_CHECKED : 0);

                // add "always sort"
                winhInsertMenuSeparator(hwndSortMenu, MIT_END,
                                  (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_SEPARATOR));
                f = (_lAlwaysSort == SET_DEFAULT)
                        ? pGlobalSettings->AlwaysSort
                        : _lAlwaysSort;
                winhInsertMenuItem(hwndSortMenu, MIT_END,
                                   (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_ALWAYSSORT),
                                   cmnGetString(ID_XSSI_SV_ALWAYSSORT), // pszAlwaysSort
                                   MIS_TEXT,
                                   (f) ? MIA_CHECKED : 0);
            }
        }
    }

    return (brc);
}

/*
 *@@ fdrSortMenuItemSelected:
 *      this is used by both mnuMenuItemSelected and
 *      fdr_fnwpSubclassedFolderFrame for checking if the selected
 *      menu item is one of the folder things and, if so,
 *      setting the folder sort settings accordingly.
 *
 *      We need to have a separate proc for this because
 *      fdr_fnwpSubclassedFolderFrame needs this if the user uses
 *      the mouse and mnuMenuItemSelected gets the folder
 *      hotkeys.
 *
 *      This returns TRUE if the menu item was processed
 *      and then sets *pbDismiss to whether the menu should
 *      be dismissed. If pbDismiss is passed as NULL, this
 *      routine assumes that we're dealing with hotkeys
 *      instead of menu items.
 *
 *@@changed V0.9.12 (2001-05-18) [umoeller]: moved this here from fdrmenus.c, mostly rewritten
 */

BOOL fdrSortMenuItemSelected(WPFolder *somSelf,
                             HWND hwndFrame,
                             HWND hwndMenu,      // may be NULLHANDLE if
                                                 // pbDismiss is NULL also
                             ULONG ulMenuId,
                             PBOOL pbDismiss)    // out: dismiss flag for fdr_fnwpSubclassedFolderFrame
{
    BOOL            brc = FALSE;
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    if (pGlobalSettings->ExtFolderSort)
    {
        LONG            lMenuId2 = (LONG)ulMenuId - (LONG)pGlobalSettings->VarMenuOffset;
        LONG            lAlwaysSort,
                        lFoldersFirst,
                        lDefaultSort;
        XFolderData     *somThis = XFolderGetData(somSelf);
        BOOL            fShiftPressed = doshQueryShiftState();

        LONG            lSort = -999;           // dumb default for "not set"

        // step 1:
        // check if one of the sort criteria was selected

        switch (lMenuId2)
        {
            // new sort items
            case ID_XFMI_OFS_SORTBYCLASS:
                lSort = -3;
            break;

            case ID_XFMI_OFS_SORTBYEXT:
                lSort = -4;
            break;

            // "Always sort"
            case ID_XFMI_OFS_ALWAYSSORT:
            {
                BOOL                fAlwaysSort = (_lAlwaysSort == SET_DEFAULT)
                                                        ? pGlobalSettings->AlwaysSort
                                                        : _lAlwaysSort;

                _xwpQueryFldrSort(somSelf,
                                  &lDefaultSort,
                                  &lFoldersFirst,
                                  &lAlwaysSort);
                _xwpSetFldrSort(somSelf,
                                lDefaultSort,
                                lFoldersFirst,
                                !fAlwaysSort);

                winhSetMenuItemChecked(hwndMenu,
                                       ulMenuId,
                                       !fAlwaysSort);

                brc = TRUE;
            }
            break;

            // "folders first"
            case ID_XFMI_OFS_SORTFOLDERSFIRST:
            {
                BOOL                fFoldersFirst = (_lFoldersFirst == SET_DEFAULT)
                                                        ? pGlobalSettings->fFoldersFirst
                                                        : _lFoldersFirst;

                _xwpQueryFldrSort(somSelf,
                                  &lDefaultSort,
                                  &lFoldersFirst,
                                  &lAlwaysSort);
                _xwpSetFldrSort(somSelf,
                                lDefaultSort,
                                !fFoldersFirst,
                                lAlwaysSort);

                winhSetMenuItemChecked(hwndMenu,
                                       ulMenuId,
                                       !fFoldersFirst);

                brc = TRUE;
            }
            break;

            default:
                if (ulMenuId == WPMENUID_SORTBYNAME)
                    lSort = -2;
                else if (ulMenuId == WPMENUID_SORTBYTYPE)
                    lSort = -1;
                else
                    // check if maybe this is one of the
                    // sort criteria from the details columns;
                    // for this, the WPS uses 6002 plus the
                    // details column index
                    if (    (ulMenuId >= 6002)
                         && (ulMenuId <= 6200)
                       )
                        // looks like it:
                        lSort = ulMenuId - 6002;
        }

        // 2) SORT if a sort criterion was selected
        if (lSort != -999)
        {
            // yes:
            _xwpQueryFldrSort(somSelf,
                              &lDefaultSort,
                              &lFoldersFirst,
                              &lAlwaysSort);

            if ((fShiftPressed) && (pbDismiss))
            {
                // shift was pressed, and not from hotkey:
                // change the folder sort settings
                _xwpSetFldrSort(somSelf,
                                lSort,
                                lFoldersFirst,
                                lAlwaysSort);
                // update the menu
                CheckDefaultSortItem(hwndMenu,
                                     lSort);
            }
            else
                // shift was NOT pressed, or hotkey:
                // just sort once
                _xwpSortViewOnce(somSelf,
                                 hwndFrame,
                                 lFoldersFirst,
                                 lSort);

            // say "processed"
            brc = TRUE;
        }

        _Pmpf((__FUNCTION__ ": returning %d", brc));

        if (brc)
            if (pbDismiss)
                // do not dismiss menu if shift was pressed
                *pbDismiss = (!fShiftPressed);
    }

    return (brc);
}

/* ******************************************************************
 *
 *   Folder sort comparison funcs
 *
 ********************************************************************/

/*
 *@@ CompareStrings:
 *
 *      NOTE: This is a WPS comparison func, NOT a cnr
 *      comparison func. This gets called from
 *      fnCompareDetailsColumn.
 *
 *@@added V0.9.12 (2001-05-18) [umoeller]
 */

LONG EXPENTRY CompareStrings(PSZ *ppsz1,     // ptr to PSZ 1
                             PSZ *ppsz2)     // ptr to PSZ 2
{
    int i = stricmp(*ppsz1, *ppsz2);
    if (i > 0)
        return (CMP_GREATER);
    if (i < 0)
        return (CMP_LESS);
    return (CMP_EQUAL);
}

/*
 *@@ CompareULongs:
 *
 *      NOTE: This is a WPS comparison func, NOT a cnr
 *      comparison func. This gets called from
 *      fnCompareDetailsColumn.
 *
 *@@added V0.9.12 (2001-05-18) [umoeller]
 */

LONG EXPENTRY CompareULongs(PULONG pul1,     // ptr to ul1
                            PULONG pul2)     // ptr to ul2
{
    if (*pul1 > *pul2)
        return (CMP_GREATER);
    if (*pul1 < *pul2)
        return (CMP_LESS);
    return (CMP_EQUAL);
}

/*
 *@@ fnCompareDetailsColumn:
 *      special container comparison func which is
 *      set on the folder cnr if the sort is to be
 *      performed according to one of the details
 *      columns.
 *
 *      While four sort criteria are hard-coded
 *      (those with the negative values, see
 *      XFolder::xwpSetFldrSort), all positive
 *      values are assumed to refer to details
 *      columns.
 *
 *      Quite a bit of trickery is necessary to
 *      get access to the objects' details data
 *      since the stupid container control doesn't
 *      support the "pStorage" parameter when a
 *      sort function is set permanently via CNRINFO.
 *
 *@@added V0.9.12 (2001-05-18) [umoeller]
 */

SHORT EXPENTRY fnCompareDetailsColumn(PMINIRECORDCORE pmrc1,
                                      PMINIRECORDCORE pmrc2,
                                      PVOID pStorage)           // unused
{
    // get the object pointers
    WPObject *pobj1 = OBJECT_FROM_PREC(pmrc1),
             *pobj2 = OBJECT_FROM_PREC(pmrc2);

    // get the folder (same for both objects)
    WPFolder *pFolder = _wpQueryFolder(pobj1);
    // get folder instance data
    XFolderData *somThis = XFolderGetData(pFolder);
    // get WPFolder sort struct
    PWPSSORTINFO pSortInfo = (PWPSSORTINFO)_pFolderSortInfo;

    // check if the objects are descended from the
    // folder's sort class; the "Class" field has
    // been set by fdrQuerySortFunc
    BOOL    f1IsOfSortClass = _somIsA(pobj1, pSortInfo->Class),
            f2IsOfSortClass = _somIsA(pobj2, pSortInfo->Class);

    if (     (f1IsOfSortClass)
          && (f2IsOfSortClass)
       )
    {
        // OK, we can go for the data values... this is VERY
        // kludgy: the object details data comes directly after
        // the MINIRECORDCORE of an object.
        // The field offset has been set by fdrQuerySortFunc.
        PBYTE   pb1 =   (PBYTE)pmrc1                // object 1's MINIRECORDCORE
                      + sizeof(MINIRECORDCORE)      // plus MINIRECORDCORE: start of data
                      + pSortInfo->ulFieldOffset;   // details column field offset
        PBYTE   pb2 =   (PBYTE)pmrc2
                      + sizeof(MINIRECORDCORE)
                      + pSortInfo->ulFieldOffset;

        LONG    lResult = pSortInfo->pfnCompare(pb1, pb2);

        // WPS comparison functions return:
        // -- CMP_EQUAL        0
        // -- CMP_GREATER      1
        // -- CMP_LESS         2
        if (lResult == CMP_LESS)
            // convert to cnr comparison value
            return (-1);

        return (lResult);
   }
   // else: at least one object doesn't support the criterion...

   if (!f1IsOfSortClass)
      // but object 2 is:
      return (1);

   if (!f2IsOfSortClass)
      return (-1);

   // neither does:
   return (0);
}

/*
 *@@ fdrQuerySortFunc:
 *      this returns the sort comparison function for
 *      the specified sort criterion.
 *
 *      See XFolder::xwpSetFldrSort for the possible
 *      values for lSort.
 *
 *      If one of the hard-coded criteria (with a negative
 *      value) is set, this is easy: we simply return one
 *      of the functions in shared\cnrsort.c.
 *
 *      Otherwise, the comparison function will be
 *      fnCompareDetailsColumn, and a bit of setup will
 *      be required for that function to work.
 *
 *@@changed V0.9.0 [umoeller]: moved this func here from xfldr.c
 *@@changed V0.9.12 (2001-05-18) [umoeller]: rewritten
 */

PFN fdrQuerySortFunc(WPFolder *somSelf,
                     LONG lSort)        // in: sort criterion
{
    if (lSort == SET_DEFAULT)
    {
        PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
        lSort =  pGlobalSettings->lDefaultSort;
    }

    switch (lSort)
    {
        // hard-coded criteria: return sort functions from shared\cnrsort.c
        case -1:
            return ((PFN)fnCompareType);

        case -2:
            return ((PFN)fnCompareName);

        case -3:
            return ((PFN)fnCompareClass);

        case -4:
            return ((PFN)fnCompareExt);

        default:
        {
            // looks like caller wants a details column... well then.
            if (lSort >= 0)
            {
                XFolderData *somThis = XFolderGetData(somSelf);
                PWPSSORTINFO psi;

                if (    (psi = (PWPSSORTINFO)_pFolderSortInfo)
                     && (_wpIsSortAttribAvailable(somSelf,
                                                  lSort))
                     && (psi->Class = _wpQueryFldrSortClass(somSelf))
                   )
                {
                    // alright, set up the sort info for the
                    // cnr comparison func
                    PCLASSFIELDINFO pcfi;
                    ULONG ul;

                    psi->lCurrentSort = lSort;

                    // 1) calculate the field offset into the details data
                    _wpclsQueryDetailsInfo(psi->Class,
                                           &pcfi,
                                           NULL);
                    psi->ulFieldOffset = 0;
                    for (ul = 0;
                         ul < lSort;
                         ul++, pcfi = pcfi->pNextFieldInfo)
                    {
                        // skip the first two columns, they are
                        // not part of the details data
                        if (ul >= 2)
                            psi->ulFieldOffset += pcfi->ulLenFieldData;
                    } // end for

                    // pcfi points to the proper column now

                    // 2) set the WPS comparison func

                    // a) the details column might have specified
                    //    its own WPS comparison function (for example,
                    //    "size" does this)
                    psi->pfnCompare = pcfi->pfnSort;

                    if (!psi->pfnCompare)
                    {
                        // no special sort function specified:
                        if (pcfi->flData & CFA_STRING)
                            psi->pfnCompare = (PFNCOMPARE)CompareStrings;
                        else if (pcfi->flData & CFA_ULONG)
                            psi->pfnCompare = (PFNCOMPARE)CompareULongs;

                        // @@todo: sort by date
                        /* else if (pcfi->flData & CFA_DATE)
                            psi->pfnCompare = (PFNCOMPARE)CompareDate;
                        else if (pcfi->flData & CFA_TIME)
                            psi->pfnCompare = (PFNCOMPARE)CompareTime; */

                        else
                            // unsupported format: sort by name then
                            // (shouldn't happen, but better be safe
                            // than sorry)
                            return (PFN)fnCompareName;
                    }

                    // 3) return the details column _cnr_ comparison func,
                    //    which will use the WPS comparison func
                    //    in turn
                    return ((PFN)fnCompareDetailsColumn);
                }
            }
        }
    }

    // invalid lSort specified:
    return ((PFN)fnCompareName);
}

/* ******************************************************************
 *
 *   Interfaces, callbacks
 *
 ********************************************************************/

/*
 *@@ fdrHasAlwaysSort:
 *      general function which returns whether the
 *      specified folder has the "always sort" flag
 *      set.
 *
 *      If extended folder sorting is enabled, this
 *      returns the instance or the global setting.
 *
 *      Otherwise, this returns the flag from
 *      the WPSSORTINFO.
 *
 *@@added V0.9.12 (2001-05-19) [umoeller]
 */

BOOL fdrHasAlwaysSort(WPFolder *somSelf)
{
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    XFolderData *somThis = XFolderGetData(somSelf);

    if (pGlobalSettings->ExtFolderSort)
        return (_lAlwaysSort == SET_DEFAULT)
                   ? pGlobalSettings->AlwaysSort
                   : _lAlwaysSort;

    if (_pFolderSortInfo)
        return (((PWPSSORTINFO)_pFolderSortInfo)->fAlwaysSort);

    return (FALSE);
}

/*
 * fdrSortAllViews:
 *      callback function for sorting all folder views.
 *      This is called by xf(cls)ForEachOpenView, which also passes
 *      the parameters to this func.
 *
 *@@changed V0.9.0 [umoeller]: moved this func here from xfldr.c
 */

MRESULT EXPENTRY fdrSortAllViews(HWND hwndView,    // open folder view frame hwnd
                                 ULONG ulSort,     // sort flag
                                 MPARAM mpView,    // OPEN_xxx flag
                                 MPARAM mpFolder)  // XFolder*
{
    XFolder     *somSelf = (XFolder*)mpFolder;
    MRESULT     mrc = (MPARAM)FALSE;

    if (   ((ULONG)mpView == OPEN_CONTENTS)
        || ((ULONG)mpView == OPEN_TREE)
        || ((ULONG)mpView == OPEN_DETAILS)
       )
    {
        _xwpSortViewOnce(somSelf,
                         hwndView,
                         FALSE,             // @@todo
                         ulSort);
        mrc = (MPARAM)TRUE;
    }
    return (mrc);
}

/*
 *@@ fdrSetFldrCnrSort:
 *      this is the most central function of XFolder's
 *      extended sorting capabilities. This is called
 *      every time the container in an open folder view
 *      needs to have its sort settings updated. In other
 *      words, this function evaluates the current folder
 *      sort settings and finds out the corresponding
 *      container sort comparison functions and other
 *      settings.
 *
 *      Parameters:
 *      --  HWND hwndCnr     cnr of open view of somSelf
 *      --  BOOL fForce      TRUE: always update the cnr
 *                           settings, even if they have
 *                           not changed
 *
 *      This function gets called:
 *
 *      1)  from our wpOpen override to set folder sort
 *          when a new folder view opens;
 *
 *      2)  from our wpSetFldrSort override (see notes
 *          there);
 *
 *      3)  after folder sort settings have been changed
 *          using xwpSetFldrSort.
 *
 *      It is usually not necessary to call this method
 *      directly. To sort folders, you should call
 *      XFolder::xwpSetFldrSort or XFolder::xwpSortViewOnce
 *      instead.
 *
 *@@changed V0.9.0 [umoeller]: this used to be an instance method
 *@@changed V0.9.0 [umoeller]: moved this func here from xfldr.c
 *@@changed V0.9.12 (2001-05-18) [umoeller]: now setting wait pointer
 */

VOID fdrSetFldrCnrSort(WPFolder *somSelf,      // in: folder to sort
                       HWND hwndCnr,           // in: container of open view of somSelf
                       BOOL fForce)            // in: always invalidate container?
{
    XFolderData *somThis = XFolderGetData(somSelf);

    if (hwndCnr)
    {
        // set wait pointer V0.9.12 (2001-05-18) [umoeller]
        HPOINTER hptrOld = winhSetWaitPointer();

        WPSHLOCKSTRUCT Lock;
        if (wpshLockObject(&Lock, somSelf))
        {
            PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

            // use our macro for determining this folder's always-sort flag;
            // this is TRUE if "Always sort" is on either locally or globally
            BOOL            AlwaysSort = (_lAlwaysSort == SET_DEFAULT)
                                            ? pGlobalSettings->AlwaysSort
                                            : _lAlwaysSort;

            // get our sort comparison func
            PFN             pfnSort =  (AlwaysSort)
                                           ? fdrQuerySortFunc(somSelf,
                                                              (_lDefaultSort == SET_DEFAULT)
                                                                 ? pGlobalSettings->lDefaultSort
                                                                 : _lDefaultSort)
                                           : NULL;
            CNRINFO         CnrInfo = {0};
            BOOL            Update = FALSE;

            cnrhQueryCnrInfo(hwndCnr, &CnrInfo);

            #ifdef DEBUG_SORT
                _Pmpf((__FUNCTION__ ": %s with hwndCnr = 0x%lX",
                        _wpQueryTitle(somSelf), hwndCnr ));
                _Pmpf(( "  _Always: %d, Global->Always: %d",
                        _lAlwaysSort, pGlobalSettings->AlwaysSort ));
                _Pmpf(( "  ALWAYS_SORT returned %d", AlwaysSort ));
                _Pmpf(( "  _Default: %d, Global->Default: %d",
                        _lDefaultSort, pGlobalSettings->lDefaultSort ));
                _Pmpf(( "  pfnSort is 0x%lX", pfnSort ));
            #endif

            // for icon views, we need extra precautions
            if ((CnrInfo.flWindowAttr & (CV_ICON | CV_TREE)) == CV_ICON)
            {
                // for some reason, cnr icon views need to have "auto arrange" on
                // when sorting, or the cnr will allow to drag'n'drop icons freely
                // within the same cnr, which is not useful when auto-sort is on

                ULONG       ulStyle = WinQueryWindowULong(hwndCnr, QWL_STYLE);

                if (AlwaysSort)
                {
                    // always sort: we need to set CCS_AUTOPOSITION, if not set
                    if ((ulStyle & CCS_AUTOPOSITION) == 0)
                    {
                        #ifdef DEBUG_SORT
                            _Pmpf(( "  Setting CCS_AUTOPOSITION"));
                        #endif
                        WinSetWindowULong(hwndCnr, QWL_STYLE, (ulStyle | CCS_AUTOPOSITION));
                        Update = TRUE;
                    }
                }
                else
                {
                    // NO always sort: we need to unset CCS_AUTOPOSITION, if set
                    if ((ulStyle & CCS_AUTOPOSITION) != 0)
                    {
                        #ifdef DEBUG_SORT
                            _Pmpf(( "  Clearing CCS_AUTOPOSITION"));
                        #endif
                        WinSetWindowULong(hwndCnr, QWL_STYLE, (ulStyle & (~CCS_AUTOPOSITION)));
                        Update = TRUE;
                    }
                }
            }

            // now also update the internal WPFolder sort info, because otherwise
            // the WPS will keep reverting the cnr attrs; we have obtained the pointer
            // to this structure in wpRestoreData
            if (_wpIsObjectInitialized(somSelf))
                if (_pFolderSortInfo)
                    ((PWPSSORTINFO)_pFolderSortInfo)->fAlwaysSort = AlwaysSort;

            // finally, set the cnr sort function: we perform these checks
            // to avoid cnr flickering
            if (    // sort function changed?
                    /* (CnrInfo.pSortRecord != (PVOID)pfnSort)
                    // ^^^ disabled this check, because we can now have
                    // the same sort proc for several criteria
                    // V0.9.12 (2001-05-18) [umoeller]
                 || */
                    // CCS_AUTOPOSITION flag changed above?
                    (Update)
                 || (fForce)
               )
            {
                #ifdef DEBUG_SORT
                    _Pmpf(( "  Resetting pSortRecord to %lX", pfnSort ));
                #endif

                // set the cnr sort function; if this is != NULL, the
                // container will always sort the records. If auto-sort
                // is off, pfnSort has been set to NULL above.
                CnrInfo.pSortRecord = (PVOID)pfnSort;

                // now update the CnrInfo, which will sort the
                // contents and repaint the cnr also;
                // this might take a long time
                WinSendMsg(hwndCnr,
                           CM_SETCNRINFO,
                           (MPARAM)&CnrInfo,
                           (MPARAM)CMA_PSORTRECORD);
            }
        }
        wpshUnlockObject(&Lock);

        WinSetPointer(HWND_DESKTOP, hptrOld);

    } // end if (hwndCnr)
}

/*
 * fdrUpdateFolderSorts:
 *      callback function for updating all folder sorts.
 *      This is called by xf(cls)ForEachOpenView, which also passes
 *      the parameters to this func.
 *
 *@@changed V0.9.0 [umoeller]: moved this func here from xfldr.c
 *@@changed V0.9.7 (2000-12-18) [umoeller]: fixed wrong window handle
 */

MRESULT EXPENTRY fdrUpdateFolderSorts(HWND hwndView,   // frame wnd handle
                                      ULONG fForce,
                                      MPARAM mpView,   // OPEN_xxx flag for this view
                                      MPARAM mpFolder) // somSelf
{
    XFolder     *somSelf = (XFolder*)mpFolder;
    MRESULT     mrc = (MPARAM)FALSE;

    #ifdef DEBUG_SORT
        _Pmpf((__FUNCTION__ ": %s", _wpQueryTitle(somSelf) ));
    #endif

    if (   ((ULONG)mpView == OPEN_CONTENTS)
        || ((ULONG)mpView == OPEN_TREE)
        || ((ULONG)mpView == OPEN_DETAILS)
       )
    {
        HWND hwndCnr = wpshQueryCnrFromFrame(hwndView);

        #ifdef DEBUG_SORT
            _Pmpf(("  hwndView 0x%lX, hwndCnr 0x%lX", hwndView, hwndCnr));
        #endif

        fdrSetFldrCnrSort(somSelf,
                          hwndCnr, // hwndView, // wrong!! V0.9.7 (2000-12-18) [umoeller]
                          fForce);
        mrc = (MPARAM)TRUE;
    }
    return (mrc);
}


