
/*
 *@@sourcefile filetypes.c:
 *      extended file types implementation code. This has
 *      both method implementations for XFldDataFile as
 *      well as notebook pages for the "Workplace Shell"
 *      object (XFldWPS).
 *
 *      This file is ALL new with V0.9.0.
 *
 *      Function prefix for this file:
 *      --  ftyp*
 *
 *@@added V0.9.0 [umoeller]
 *@@header "filesys\filetype.h"
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

#define INCL_WINWINDOWMGR
#define INCL_WINDIALOGS
#define INCL_WININPUT           // WM_CHAR
#define INCL_WINPOINTERS
#define INCL_WINPROGRAMLIST     // needed for wppgm.h
#define INCL_WINSHELLDATA       // Prf* functions
#define INCL_WINMENUS
#define INCL_WINBUTTONS
#define INCL_WINENTRYFIELDS
#define INCL_WINLISTBOXES
#define INCL_WINSTDCNR
#define INCL_WINSTDDRAG
#include <os2.h>

// C library headers
#include <stdio.h>              // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\cnrh.h"               // container helper routines
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\stringh.h"            // string helper routines
#include "helpers\winh.h"               // PM helper routines

// SOM headers which don't crash with prec. header files
#include "xfwps.ih"
#include "xfdataf.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\cnrsort.h"             // container sort comparison functions
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\notebook.h"            // generic XWorkplace notebook handling

#include "filesys\filetype.h"           // extended file types implementation

// other SOM headers
#pragma hdrstop                 // VAC++ keeps crashing otherwise
#include <wppgm.h>              // WPProgram
#include <wppgmf.h>             // WPProgramFile
#include <wpshadow.h>           // WPShadow

/* ******************************************************************
 *                                                                  *
 *   Extended associations helper funcs                             *
 *                                                                  *
 ********************************************************************/

/*
 *@@ ftypListTypesFromString:
 *      this splits a standard WPS file types
 *      string (where several file types are
 *      separated by line feeds, \n) into
 *      a linked list of newly allocated PSZ's.
 *
 *@@added V0.9.0 (99-11-27) [umoeller]
 */

ULONG ftypListTypesFromString(PSZ pszTypes, // in: types string (e.g. "C Code\nPlain text")
                              PLINKLIST pllTypes) // in/out: list of newly allocated PSZ's
                                                  // with file types (e.g. "C Code", "Plain text")
{
    ULONG   ulrc = 0;
    // if we have several file types (which are then separated
    // by line feeds (\n), get the one according to ulView.
    // For example, if pszType has "C Code\nPlain text" and
    // ulView is 0x1001, get "Plain text".
    PSZ     pTypeThis = pszTypes,
            pLF = 0;

    // loop thru types list
    while (pTypeThis)
    {
        // get next line feed
        pLF = strchr(pTypeThis, '\n');
        if (pLF)
        {
            // line feed found:
            // extract type and store in list
            lstAppendItem(pllTypes,
                          strhSubstr(pTypeThis,
                                     pLF));
            ulrc++;
            // next type (after LF)
            pTypeThis = pLF + 1;
        }
        else
        {
            // no next line feed found:
            // store last item
            lstAppendItem(pllTypes,
                          strdup(pTypeThis));
            ulrc++;
            break;
        }
    }

    return (ulrc);
}

/*
 *@@ ftypListTypesForFile:
 *      this lists all extended file types which have
 *      a file filter assigned to them which matches
 *      the given object title.
 *
 *      For example, if "C Code" has the "*.c" filter
 *      assigned, this would add "C Code" to the given
 *      list.
 *
 *      In order to query all associations for a given
 *      object, pass the object title to this function
 *      first (to get the associated types). Then, for
 *      all types on the list which was filled for this
 *      function, call ftypListAssocsForType with the
 *      same objects list for each call.
 *
 *      Note that this func creates _copies_ of the
 *      file types in the given list, using strdup().
 *
 *      This returns the no. of items which were added
 *      (0 if none).
 *
 *@@added V0.9.0 (99-11-27) [umoeller]
 */

ULONG ftypListTypesForFile(PSZ pszObjectTitle,
                           PLINKLIST pllTypes)   // in/out: list of newly allocated PSZ's
                                                // with file types (e.g. "C Code", "Plain text")
{
    ULONG   ulrc = 0;
    // loop thru all extended file types which have
    // filters assigned to them to check whether the
    // filter matches the object title
    PSZ     pszTypesWithFiltersList = prfhQueryKeysForApp(HINI_USER,
                                                          INIAPP_XWPFILEFILTERS);
                                                            // "XWorkplace:FileFilters"
    if (pszTypesWithFiltersList)
    {
        PSZ pTypeWithFilterThis = pszTypesWithFiltersList;

        while (*pTypeWithFilterThis != 0)
        {
            // pFilterThis has the current type now
            // (e.g. "C Code");
            // get filters for that (e.g. "*.c");
            //  this is another list of null-terminated strings
            ULONG cbFiltersForTypeList = 0;
            PSZ pszFiltersForTypeList = prfhQueryProfileData(HINI_USER,
                                                             INIAPP_XWPFILEFILTERS, // "XWorkplace:FileFilters"
                                                             pTypeWithFilterThis,
                                                             &cbFiltersForTypeList);
                    // this would be e.g. "*.c" now
            if (pszFiltersForTypeList)
            {
                // second loop: thru all filters for this file type
                PSZ pFilterThis = pszFiltersForTypeList;
                while (*pFilterThis != 0)
                {

                    // _Pmpf(("  ftypListTypesForFile: %s against %s",
                       //      pFilterThis, pszObjectTitle));

                    // check if this matches the data file name
                    if (strhMatchOS2(pFilterThis, pszObjectTitle))
                    {
                        // matches:
                        // now we have:
                        // --  pszFilterThis e.g. "*.c"
                        // --  pTypeFilterThis e.g. "C Code"
                        // store the type (not the filter) in the output list
                        lstAppendItem(pllTypes,
                                      strdup(pTypeWithFilterThis));

                        #ifdef DEBUG_ASSOCS
                            _Pmpf(("  ftypListTypesForFile: found %s for %s",
                                pTypeWithFilterThis, pszObjectTitle));
                        #endif

                        ulrc++;     // found something

                        break;      // this file type is done, so go for next type
                    }

                    pFilterThis += strlen(pFilterThis) + 1;   // next type/filter
                    if (pFilterThis >= pszFiltersForTypeList + cbFiltersForTypeList)
                        break;      // no more filters left
                } // end while (*pFilterThis)

                free (pszFiltersForTypeList);
            }

            pTypeWithFilterThis += strlen(pTypeWithFilterThis) + 1;   // next type/filter
        } // end while (*pTypeWithFilterThis != 0)

        free(pszTypesWithFiltersList);
    }

    return (ulrc);
}

/*
 *@@ ftypListAssocsForType:
 *      this lists all associated WPProgram or WPProgramFile
 *      objects which have been assigned to the given type.
 *
 *      For example, if "System editor" has been assigned to
 *      the "C Code" type, this would add the "System editor"
 *      program object to the given list.
 *
 *      This adds plain SOM pointers to the given list, so
 *      you better not free() those.
 *
 *      This returns the no. of objects added to the list
 *      (0 if none).
 *
 *@@added V0.9.0 (99-11-27) [umoeller]
 */

ULONG ftypListAssocsForType(PSZ pszType0,         // in: file type (e.g. "C Code")
                            PLINKLIST pllAssocs) // in/out: list of WPProgram or WPProgramFile
                                                // objects to append to
{
    ULONG   ulrc = 0;
    ULONG   cbAssocData = 0;

    PSZ     pszType2 = pszType0,
            pszParentForType = 0;

    // outer loop for climbing up the file type parents
    do
    {
        // get associations from WPS INI data
        PSZ     pszAssocData = prfhQueryProfileData(HINI_USER,
                                                    WPINIAPP_ASSOCTYPE, // "PMWP_ASSOC_TYPE"
                                                    pszType2,
                                                    &cbAssocData);

        #ifdef DEBUG_ASSOCS
            _Pmpf(("      ftypListAssocsForType: got %s for type %s", pszAssocData, pszType2));
        #endif

        if (pszAssocData)
        {
            // pszAssocData now has the handles of the associated
            // objects (as decimal strings, which we'll decode now)
            PSZ     pAssoc = pszAssocData;
            if (pAssoc)
            {
                HOBJECT hobjAssoc;
                WPObject *pobjAssoc;

                // now parse the handles string
                while (*pAssoc)
                {
                    sscanf(pAssoc, "%d", &hobjAssoc);
                    pobjAssoc = _wpclsQueryObject(_WPObject, hobjAssoc);
                    if (pobjAssoc)
                    {
                        lstAppendItem(pllAssocs, pobjAssoc);
                        ulrc++;

                        // go for next object handle (after the 0 byte)
                        pAssoc += strlen(pAssoc) + 1;
                        if (pAssoc >= pszAssocData + cbAssocData)
                            break; // while (*pAssoc)
                    }
                    else
                        break; // while (*pAssoc)
                } // end while (*pAssoc)
            }

            free(pszAssocData);
        }

        // get parent type
        {
            PSZ     psz2Free = 0;
            if (pszParentForType)
                psz2Free = pszParentForType;
            pszParentForType = prfhQueryProfileData(HINI_USER,
                                                    INIAPP_XWPFILETYPES,
                                                    pszType2,
                                                    NULL);
            #ifdef DEBUG_ASSOCS
                _Pmpf(("        Got %s as parent for %s", pszParentForType, pszType2));
            #endif

            if (psz2Free)
                free(psz2Free);     // from last loop
            if (pszParentForType)
                // parent found: use that one
                pszType2 = pszParentForType;
            else
                break;
        }

    } while (TRUE);

    return (ulrc);
}

/* ******************************************************************
 *                                                                  *
 *   XFldDataFile extended associations                             *
 *                                                                  *
 ********************************************************************/

/*
 *@@ ftypQueryType:
 *      implementation for XFldDataFile::wpQueryType.
 *
 *@@added V0.9.0 [umoeller]
 */

/* PSZ ftypQueryType(WPDataFile *somSelf,
                  PSZ pszOriginalTypes,
                  PCGLOBALSETTINGS pGlobalSettings)
{
    XFldDataFileData *somThis = XFldDataFileGetData(somSelf);

    if (_pszFileTypes == NULL)
    {
        // not queried yet:

        // 2)   append pseudo types based on file type filters
        //      declared by the user on "File types" page
        PSZ     pszFiltersList = prfhQueryKeysForApp(HINI_USER,
                                                     INIAPP_XWPFILEFILTERS);
        PSZ     pszObjectTitle = _wpQueryTitle(somSelf);

        // 1)   get the "real" type (.TYPE EA)
        if (pszOriginalTypes)
            _pszFileTypes = strdup(pszOriginalTypes);

        if (pszFiltersList)
        {
            PSZ pFilter2 = pszFiltersList;

            while (*pFilter2 != 0)
            {
                // pFilter2 has the current filter now;
                // get the profile data for this
                ULONG   cbFilterEntries = 0;
                PSZ     pszFilterEntries = prfhQueryProfileData(HINI_USER,
                                                                INIAPP_XWPFILEFILTERS,
                                                                pFilter2,
                                                                &cbFilterEntries);
                PSZ     pFilterThis = pszFilterEntries;

                // each data entry is a list of file filters, each
                // terminated by a zero byte
                while (pFilterThis < pszFilterEntries + cbFilterEntries)
                {
                    // does it match?
                    if (strhMatchOS2(pFilterThis, pszObjectTitle))
                    {
                        // yes: add to file types list
                        PSZ     pszTypes2Add = strdup(pFilter2);
                        PSZ     pszQueryParentFor = strdup(pFilter2);

                        // get the "parent" file types for this type
                        BOOL   fCont;
                        do
                        {
                            PSZ     pszParentType = prfhQueryProfileData(HINI_USER,
                                                                         INIAPP_XWPFILETYPES,
                                                                         pszQueryParentFor,
                                                                         NULL);
                            fCont = FALSE;
                            if (pszParentType)
                            {
                                // parent type exists: append to "types to add" string
                                ULONG cbTypes2AddNew = strlen(pszTypes2Add)
                                                         + strlen(pszParentType)
                                                         + 2;  // \n + null terminator
                                PSZ pszTypes2AddNew = (PSZ)malloc(cbTypes2AddNew);
                                sprintf(pszTypes2AddNew,
                                        "%s\n%s",
                                        pszTypes2Add,
                                        pszParentType);
                                free(pszTypes2Add);
                                pszTypes2Add = pszTypes2AddNew;

                                // climb up the parents tree
                                free(pszQueryParentFor);    // from last loop
                                pszQueryParentFor = pszParentType;
                                fCont = TRUE;
                            }
                        } while (fCont);
                        free(pszQueryParentFor);

                        if (_pszFileTypes)
                        {
                            ULONG   cbFileTypesNew = strlen(_pszFileTypes)
                                                      + strlen(pszTypes2Add)
                                                      + 2;  // \n + null terminator
                            PSZ     pszFileTypesNew = (PSZ)malloc(cbFileTypesNew);
                            sprintf(pszFileTypesNew,
                                    "%s\n%s",
                                    _pszFileTypes,
                                    pszTypes2Add);
                            free(_pszFileTypes);
                            _pszFileTypes = pszFileTypesNew;
                        }
                        else
                            _pszFileTypes = strdup(pszTypes2Add);

                        free(pszTypes2Add);
                    }

                    pFilterThis += strlen(pFilterThis) + 1;     // next entry
                }

                free(pszFilterEntries);

                pFilter2 += strlen(pFilter2)+1; // next key
            }
            free(pszFiltersList);
        }
    }

    return (_pszFileTypes);
} */

/*
 *@@ ftypBuildAssocsList:
 *      this helper function builds a list of all
 *      associated WPProgram and WPProgramFile objects
 *      in the data file's instance data. This list
 *      is used for caching the associations, both
 *      for getting the correct data file icon and
 *      for building the items in the "Open" context
 *      menu.
 *
 *      This returns the no. of objects added to the
 *      list (or 0 if none).
 *
 *      If (fRefresh == TRUE), the list is cleared
 *      and rebuilt in any case. Otherwise, and if
 *      the list has been built already, this func
 *      does nothing (and returns 0).
 *
 *@@added V0.9.0 (99-11-27) [umoeller]
 */

ULONG ftypBuildAssocsList(XFldDataFile *somSelf,
                          BOOL fRefresh)
{
    ULONG       cAssocObjects = 0;
    XFldDataFileData *somThis = XFldDataFileGetData(somSelf);
    BOOL        fRebuild = TRUE;

    if (_fAssocObjectsListReady)
        // list was built already:
        if (fRefresh)
            lstFree(_pvAssocObjectsList);
        else
        {
            // no refresh: do nothing
            cAssocObjects = lstCountItems(_pvAssocObjectsList);
            fRebuild = FALSE;
        }

    if (fRebuild)
    {
        // create list of type strings (to be freed)
        PLINKLIST   pllTypes = lstCreate(TRUE);       // free items
        ULONG       cTypes = 0;

        // check if the data file has a file type
        // assigned explicitly
        PSZ pszTypes = _wpQueryType(somSelf);

        #ifdef DEBUG_ASSOCS
            _Pmpf(("  Entering ftypQueryAssociatedProgram, type: %s", pszTypes));
        #endif
        if (pszTypes)
        {
            // yes, explicit type(s):
            // decode those
            cTypes = ftypListTypesFromString(pszTypes,
                                             pllTypes);
        }

        // add automatic (extended) file types based on the
        // object file name
        cTypes += ftypListTypesForFile(_wpQueryTitle(somSelf),
                                       pllTypes);

        #ifdef DEBUG_ASSOCS
            _Pmpf(("    ftypQueryAssociatedProgram: got %d matching types", cTypes));
        #endif

        if (cTypes)
        {
            // OK, file type(s) found (either explicit or automatic):

            // create list of associations (WPProgram or WPProgramFile)
            // from the types list we had above
            PLISTNODE pNode = lstQueryFirstNode(pllTypes);

            if (_pvAssocObjectsList == NULL)
                _pvAssocObjectsList = lstCreate(FALSE);

            // loop thru list
            while (pNode)
            {
                PSZ pszTypeThis = (PSZ)pNode->pItemData;
                cAssocObjects += ftypListAssocsForType(pszTypeThis,
                                                       _pvAssocObjectsList);
                pNode = pNode->pNext;
            }

            #ifdef DEBUG_ASSOCS
                _Pmpf(("    ftypQueryAssociatedProgram: got %d assocs", cAssocObjects));
            #endif
        }

        lstFree(pllTypes);

        // mark list as ready
        _fAssocObjectsListReady = TRUE;
    }

    return (cAssocObjects);
}

/*
 *@@ ftypQueryAssociatedProgram:
 *      implementation for XFldDataFile::wpQueryAssociatedProgram.
 *
 *      This gets called _instead_ of the WPDataFile version if
 *      extended associations have been enabled.
 *
 *      It is the responsibility of this method to return the
 *      associated WPProgram or WPProgramFile object for the
 *      given data file.
 *
 *      I am very unsure what all the parameters mean, because
 *      only ulView contains data as specified in WPREF. All the
 *      other fields contain garbage or are set to NULL.
 *
 *@@added V0.9.0 (99-11-27) [umoeller]
 */

WPObject* ftypQueryAssociatedProgram(XFldDataFile *somSelf,     // in: data file
                                     ULONG ulView,              // in: default view (normally 0x1000,
                                                                // can be > 1000 if the default view
                                                                // has been manually changed on the
                                                                // "Menu" page
                                     PULONG pulHowMatched,      // ?!?
                                     PSZ pszMatchString,        // ?!?
                                     ULONG cbMatchString,       // ?!?
                                     PSZ pszDefaultType)        // ?!?
{
    WPObject *pObjReturn = 0;

    ULONG cAssocObjects = ftypBuildAssocsList(somSelf,
                                              FALSE);   // no refresh
    if (cAssocObjects)
    {
        // any items found:
        ULONG               ulIndex = 0;
        XFldDataFileData    *somThis = XFldDataFileGetData(somSelf);
        PLISTNODE           pNode = 0;

        if (ulView >= 0x1000)
        {
            ulIndex = ulView - 0x1000;
            if (ulIndex > cAssocObjects)
                ulIndex = 0;
        }
        else
            ulIndex = 0;

        pNode = lstNodeFromIndex((PLINKLIST)(_pvAssocObjectsList),
                                 ulIndex);
        if (pNode)
            pObjReturn = (WPObject*)pNode->pItemData;
    }

    return (pObjReturn);
}

/*
 *@@ ftypModifyDataFileOpenSubmenu:
 *      this gets called from two places:
 *
 *      --  XFldDataFile::wpDisplayMenu, after a context
 *          menu has been completely built for a data file.
 *
 *      --  fdr_fnwpSubclassedFolderFrame, after the "Selected"
 *          pulldown menu has been built completely for a
 *          data file.
 *
 *      This is a brute-force hack to get around the
 *      limitations which IBM has imposed on manipulation
 *      of the "Open" submenu. See XFldDataFile::wpDisplayMenu
 *      for details.
 *
 *      We remove all associations from the "Open" menu and
 *      add our own ones instead.
 *
 *      Unfortunately, we cannot override wpDisplayMenu on
 *      the WPObject level either, because the damn thing
 *      doesn't get called for folder menu bars (even though
 *      there are plenty of flags for that).
 *
 *      I am beginning to wonder who implemented this menu
 *      bar crap at IBM. This is among the worst shit I've
 *      seen in the WPS so far. :-((((
 *
 *      This gets called only if extended associations are
 *      enabled.
 *
 *@@added V0.9.0 (99-11-27) [umoeller]
 */

BOOL ftypModifyDataFileOpenSubmenu(WPDataFile *somSelf, // in: data file in question
                                   HWND hwndMenu)
                                            // in: menu window with "Open" submenu;
                                            // either the return value from XFldDataFile::wpDisplayMenu
                                            // or the "Selected" pulldown
{
    BOOL brc = FALSE;
    MENUITEM        mi;

    // 1) remove existing (default WPS) association
    // items from "Open" submenu

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

                #ifdef DEBUG_ASSOCS
                    _Pmpf(("mnuModifyDataFilePopupMenu: removing 0x%lX (%s)",
                                ulItemID,
                                pszItemText));
                #endif

                free(pszItemText);

                winhDeleteMenuItem(mi.hwndSubMenu, ulItemID);

                brc = TRUE;
            }
            else
                break;

        } while (TRUE);
    }

    // 2) add the new extended associations

    if (brc)
    {
        ULONG cAssocs = ftypBuildAssocsList(somSelf,
                                            FALSE);

        if (cAssocs)
        {
            XFldDataFileData *somThis = XFldDataFileGetData(somSelf);

            // initial menu item ID; all associations must have
            // IDs >= 0x1000
            ULONG           ulItemID = 0x1000;

            // get data file default associations; this should
            // return something >= 0x1000 also
            ULONG           ulDefaultView = _wpQueryDefaultView(somSelf);

            // now add all the associations; this list has
            // instances of WPProgram and WPProgramFile
            PLISTNODE       pNode = lstQueryFirstNode((PLINKLIST)(_pvAssocObjectsList));
            while (pNode)
            {
                WPObject *pAssocThis = (WPObject*)pNode->pItemData;
                if (pAssocThis)
                {
                    PSZ pszAssocTitle = _wpQueryTitle(pAssocThis);
                    if (pszAssocTitle)
                    {
                        winhInsertMenuItem(mi.hwndSubMenu,  // still has "Open" submenu
                                           MIT_END,
                                           ulItemID,
                                           pszAssocTitle,
                                           MIS_TEXT,
                                           // if this is the default view,
                                           // mark as default
                                           (ulItemID == ulDefaultView)
                                                ? MIA_CHECKED
                                                : 0);
                    }
                }

                ulItemID++;     // raise item ID even if object was invalid;
                                // this must be the same in wpMenuItemSelected

                pNode = pNode->pNext;
            }
        }
    }

    return (brc);
}

/* ******************************************************************
 *                                                                  *
 *   XFldWPS notebook callbacks (notebook.c) for "File Types" page  *
 *                                                                  *
 ********************************************************************/

/*
 *  See ftypFileTypesInitPage for an introduction
 *  how all this crap works. This is complex.
 */

// forward declaration
MRESULT EXPENTRY fnwpImportWPSFilters(HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2);

/*
 * FILETYPERECORD:
 *      extended record core structure for
 *      "File types" container (Tree view)
 */

typedef struct _FILETYPERECORD
{
    RECORDCORE     recc;
} FILETYPERECORD, *PFILETYPERECORD;

/*
 * FILETYPELISTITEM:
 *      list item structure for building an internal
 *      linked list of all file types (linklist.c).
 */

typedef struct _FILETYPELISTITEM
{
    PFILETYPERECORD     precc;
    PSZ                 pszFileType;        // copy of file type in INI
    BOOL                fProcessed;
    BOOL                fCircular;          // security; prevent circular references
} FILETYPELISTITEM, *PFILETYPELISTITEM;

/*
 * ASSOCRECORD:
 *      extended record core structure for the
 *      "Associations" container.
 */

typedef struct _ASSOCRECORD
{
    RECORDCORE          recc;
    HOBJECT             hobj;
} ASSOCRECORD, *PASSOCRECORD;

/*
 * FILETYPEPAGEDATA:
 *      this is created in PCREATENOTEBOOKPAGE
 *      to store various data for the file types
 *      page.
 */

typedef struct _FILETYPESPAGEDATA
{
    // reverse linkage to notebook page data
    // (needed for subwindows)
    PCREATENOTEBOOKPAGE pcnbp;

    // linked list of file types (linklist.c)
    PLINKLIST pllFileTypes;

    // linked list of various items which have
    // been allocated using malloc(); this
    // will be cleaned up when the page is
    // destroyed (CBI_DESTROY)
    PLINKLIST pllCleanup;

    // controls which are used all the time
    HWND    hwndTypesCnr,
            hwndAssocsCnr,
            hwndFiltersCnr,
            hwndIconStatic;

    // non-modal "Import WPS Filters" dialog
    // or NULLHANDLE, if not open
    HWND    hwndWPSImportDlg;

    // original window proc of the "Associations" list box
    // (which has been subclassed to allow for d'n'd);
    PFNWP   pfnwpListBoxOriginal;

    // currently selected record core in container
    // (updated by CN_EMPHASIS)
    PFILETYPERECORD pftreccSelected;

    // drag'n'drop within the file types container
    BOOL fFileTypesDnDValid;

    // drag'n'drop of WPS objects to assocs container;
    // NULL if d'n'd is invalid
    WPObject *pobjDrop;
    // record core after which item is to be inserted
    PRECORDCORE preccAfter;

} FILETYPESPAGEDATA, *PFILETYPESPAGEDATA;

// define a new rendering mechanism, which only
// our own container supports (this will make
// sure that we can only do d'n'd within this
// one container)
#define DRAG_RMF  "(DRM_XWPFILETYPES)x(DRF_UNKNOWN)"

#define RECORD_DISABLED CRA_DISABLED

/*
 *@@ AddFileType2Cnr:
 *      this adds the given file type to the given
 *      container window, which should be in Tree
 *      view to have a meaningful display.
 *
 *      pliAssoc->pftrecc is set to the new record
 *      core, which is also returned.
 */

PFILETYPERECORD AddFileType2Cnr(HWND hwndCnr,           // in: cnr to insert into
                                PFILETYPERECORD preccParent,  // in: parent recc for tree view
                                PFILETYPELISTITEM pliAssoc)   // in: file type to add
{
    PFILETYPERECORD      preccNew = (PFILETYPERECORD)
            cnrhAllocRecords(hwndCnr, sizeof(FILETYPERECORD), 1);
    // recc attributes
    ULONG            usAttrs = CRA_COLLAPSED
                               | CRA_RECORDREADONLY
                               | CRA_DROPONABLE;      // records can be dropped

    if (preccNew)
    {
        // insert the record (helpers/winh.c)
        cnrhInsertRecords(hwndCnr,
                        (PRECORDCORE)preccParent,
                        (PRECORDCORE)preccNew,
                        TRUE, // invalidate
                        pliAssoc->pszFileType, usAttrs, 1);
    }

    pliAssoc->precc = preccNew;
    pliAssoc->fProcessed = TRUE;

    return (preccNew);
}

/*
 *@@ AddFileTypeAndAllParents:
 *
 */

PFILETYPERECORD AddFileTypeAndAllParents(PFILETYPESPAGEDATA pftpd,
                                         PSZ pszKey)
{
    PFILETYPERECORD pftreccParent = NULL,
                        pftreccReturn = NULL;
    PLISTNODE           pAssocNode;

    // query the parent for pszKey
    PSZ pszParentForKey = prfhQueryProfileData(HINI_USER,
                                               INIAPP_XWPFILETYPES, // "XWorkplace:FileTypes"
                                               pszKey,
                                               NULL);

    /* if (YesNoBox(pszKey, "Continue?") != MBID_YES)
        return (NULL); */

    if (pszParentForKey)
    {
        // key has a parent: recurse!
        pftreccParent = AddFileTypeAndAllParents(pftpd,
                                                 // recurse with parent
                                                 pszParentForKey);
        free(pszParentForKey);
    }

    // we arrive here after the all the parents
    // of pszKey have been added;
    // if we have no parent, pftreccParent is NULL

    // now find the file type list item
    // which corresponds to pKey
    pAssocNode = lstQueryFirstNode(pftpd->pllFileTypes);
    while (pAssocNode)
    {
        PFILETYPELISTITEM pliAssoc = (PFILETYPELISTITEM)(pAssocNode->pItemData);

        if (strcmp(pliAssoc->pszFileType,
                   pszKey) == 0)
        {
            if (pliAssoc->fProcessed == FALSE)
            {
                if (!pliAssoc->fCircular)
                // add record core, which will be stored in
                // pliAssoc->pftrecc
                pftreccReturn = AddFileType2Cnr(pftpd->hwndTypesCnr,
                                                pftreccParent,
                                                    // parent record; this might be NULL
                                                pliAssoc);
                pliAssoc->fCircular = TRUE;
            }
            else
                // record core already created:
                // return that one
                pftreccReturn = pliAssoc->precc;

            // in any case, stop
            break;
        }

        pAssocNode = pAssocNode->pNext;
    }

    if (pAssocNode == NULL)
        // no file type found which corresponds
        // to the hierarchy INI item: delete it,
        // since it has no further meaning
        PrfWriteProfileString(HINI_USER,
                              INIAPP_XWPFILETYPES,    // "XWorkplace:FileTypes"
                              pszKey,
                              NULL);  // delete key

    // return the record core which we created;
    // if this is a recursive call, this will
    // be used as a parent by the parent call
    return (pftreccReturn);

    /*
        if ((preccMove == NULL) || (preccNewParent == NULL))
            // pszKey has no corresponding file type:
            // delete this key, since it has no meaning
        else
            cnrhMoveTree(hwndCnr,
                            // record to move
                            preccMove,
                            // new parent
                            preccNewParent,
                            // sort function (cnrsort.c)
                            (PFNCNRSORT)fnCompareName);


    } */
}

/*
 *@@ AddAssocObject2Cnr:
 *      this adds the given WPProgram or WPProgramFile object
 *      to the given position in the "Associations" container.
 *      The object's object handle is stored in the ASSOCRECORD.
 *
 *      PM provides those "handles" for each list box item,
 *      which can be used for any purpose by an application.
 *      We use it for the object handles here.
 *
 *      Returns the new ASSOCRECORD or NULL upon errors.
 */

PASSOCRECORD AddAssocObject2Cnr(HWND hwndAssocsCnr,
                                WPObject *pObject,  // in: must be a WPProgram or WPProgramFile
                                PRECORDCORE preccInsertAfter, // in: record to insert after (or CMA_FIRST or CMA_END)
                                BOOL fEnableRecord) // in: if FALSE, the record will be disabled
{
    // ULONG ulrc = LIT_ERROR;

    PSZ pszObjectTitle = _wpQueryTitle(pObject);

    PASSOCRECORD preccNew = (PASSOCRECORD)cnrhAllocRecords(
                                                hwndAssocsCnr,
                                                sizeof(ASSOCRECORD),
                                                1);
    if (preccNew)
    {
        ULONG   flRecordAttr = CRA_RECORDREADONLY | CRA_DROPONABLE;

        if (!fEnableRecord)
            flRecordAttr |= RECORD_DISABLED;

        // store object handle for later
        preccNew->hobj = _wpQueryHandle(pObject);

        #ifdef DEBUG_ASSOCS
            _Pmpf(("AddAssoc: flRecordAttr %lX", flRecordAttr));
        #endif

        cnrhInsertRecordAfter(hwndAssocsCnr,
                              (PRECORDCORE)preccNew,
                              pszObjectTitle,
                              flRecordAttr,
                              (PRECORDCORE)preccInsertAfter);
    }

    return (preccNew);
}

/*
 *@@ WriteAssocs2INI:
 *      this updates the "PMWP_ASSOC_FILTER" key in OS2.INI
 *      when the associations for a certain file type have
 *      been changed.
 *
 *      This func automatically determines the file type
 *      to update (== the INI "key") from hwndCnr and
 *      reads the associations for it from the list box.
 *      The list box entries must have been added using
 *      AddAssocObject2Cnr, because otherwise this
 *      func cannot find the object handles.
 */

BOOL WriteAssocs2INI(PSZ  pszProfileKey, // in: either "PMWP_ASSOC_TYPE" or "PMWP_ASSOC_FILTER"
                     HWND hwndTypesCnr,  // in: cnr with selected FILETYPERECORD
                     HWND hwndAssocsCnr) // in: cnr with ASSOCRECORDs
{
    BOOL    brc = FALSE;

    if ((hwndTypesCnr) && (hwndAssocsCnr))
    {
        // get selected file type; since the cnr is in
        // Tree view, there can be only one
        PFILETYPERECORD preccSelected =
                        (PFILETYPERECORD)WinSendMsg(hwndTypesCnr,
                                            CM_QUERYRECORDEMPHASIS,
                                            (MPARAM)CMA_FIRST,
                                            (MPARAM)CRA_SELECTED);
        if (    (preccSelected)
             && ((LONG)preccSelected != -1)
           )
        {
            // the file type is equal to the record core title
            PSZ     pszFileType = preccSelected->recc.pszIcon;

            CHAR    szAssocs[1000] = "";
            ULONG   cbAssocs = 0;

            // now create the handles string for PMWP_ASSOC_FILTER
            // from the ASSOCRECORD handles (which have been set
            // by AddAssocObject2Cnr above)

            PASSOCRECORD preccThis = (PASSOCRECORD)WinSendMsg(
                                                hwndAssocsCnr,
                                                CM_QUERYRECORD,
                                                NULL,
                                                MPFROM2SHORT(CMA_FIRST, CMA_ITEMORDER));

            while ((preccThis) && ((ULONG)preccThis != -1))
            {
                if (preccThis->hobj == 0)
                    // item not found: exit
                    break;

                // if the assocs record is not disabled (i.e. not from
                // the parent record), add it to the string
                if ((preccThis->recc.flRecordAttr & RECORD_DISABLED) == 0)
                {
                    cbAssocs += sprintf(&(szAssocs[cbAssocs]), "%d", preccThis->hobj) + 1;
                }

                preccThis = (PASSOCRECORD)WinSendMsg(hwndAssocsCnr,
                                                     CM_QUERYRECORD,
                                                     preccThis,
                                                     MPFROM2SHORT(CMA_NEXT, CMA_ITEMORDER));
            }

            if (cbAssocs == 0)
                // always write at least the initial 0 byte
                cbAssocs = 1;

            brc = PrfWriteProfileData(HINI_USER,
                                      pszProfileKey,
                                      pszFileType,        // from cnr
                                      szAssocs,
                                      cbAssocs);
        }
    }

    return (brc);
}

/*
 *@@ UpdateAssocsCnr:
 *      this updates the given list box with associations,
 *      calling AddAssocObject2Cnr to add the
 *      program objects.
 *
 *      This gets called in several situations:
 *
 *      a)  when we're in "Associations"
 *          mode and a new record core gets selected in
 *          the container, or if we're switching to
 *          "Associations" mode; in this case, the
 *          "Associations" list box on the notebook
 *          page is updated (using "PMWP_ASSOC_TYPE");
 *
 *      b)  from the "Import WPS Filters" dialog,
 *          with the "Associations" list box in that
 *          dialog (and "PMWP_ASSOC_FILTER").
 */

VOID UpdateAssocsCnr(HWND hwndAssocsCnr,    // in: container to update
                     PSZ  pszTypeOrFilter,  // in: file type or file filter
                     PSZ  pszINIApp,        // in: "PMWP_ASSOC_TYPE" or "PMWP_ASSOC_FILTER"
                     BOOL fEmpty,           // in: if TRUE, list box will be emptied beforehand
                     BOOL fEnableRecords)   // in: if FALSE, the records will be disabled
{
    // get WPS associations from OS2.INI for this file type/filter
    ULONG cbAssocData;
    PSZ pszAssocData = prfhQueryProfileData(HINI_USER,
                                            pszINIApp, // "PMWP_ASSOC_TYPE" or "PMWP_ASSOC_FILTER"
                                            pszTypeOrFilter,
                                            &cbAssocData);

    if (fEmpty)
        // empty listbox
        WinSendMsg(hwndAssocsCnr,
                   CM_REMOVERECORD,
                   NULL,
                   MPFROM2SHORT(0, CMA_FREE | CMA_INVALIDATE));

    if (pszAssocData)
    {
        // pszAssocData now has the handles of the associated
        // objects (as decimal strings, which we'll decode now)
        PSZ     pAssoc = pszAssocData;
        if (pAssoc)
        {
            HOBJECT hobjAssoc;
            WPObject *pobjAssoc;

            // now parse the handles string
            while (*pAssoc)
            {
                sscanf(pAssoc, "%d", &hobjAssoc);
                pobjAssoc = _wpclsQueryObject(_WPObject, hobjAssoc);
                if (pobjAssoc)
                {
                    #ifdef DEBUG_ASSOCS
                        _Pmpf(("UpdateAssocsCnr: Adding record, fEnable: %d", fEnableRecords));
                    #endif

                    AddAssocObject2Cnr(hwndAssocsCnr,
                                       pobjAssoc,
                                       (PRECORDCORE)CMA_END,
                                       fEnableRecords);

                    // go for next object handle (after the 0 byte)
                    pAssoc += strlen(pAssoc) + 1;
                    if (pAssoc >= pszAssocData + cbAssocData)
                        break; // while (*pAssoc)
                }
                else
                    break; // while (*pAssoc)
            } // end while (*pAssoc)
        }

        free(pszAssocData);
    }
}

/*
 *@@ AddFilter2Cnr:
 *      this adds the given file filter to the "Filters"
 *      container window, which should be in Name view
 *      to have a meaningful display.
 *
 *      The new record core is returned.
 */

PRECORDCORE AddFilter2Cnr(PFILETYPESPAGEDATA pftpd,
                          PSZ pszFilter)    // in: filter name
{
    PRECORDCORE preccNew = cnrhAllocRecords(pftpd->hwndFiltersCnr,
                                              sizeof(RECORDCORE), 1);
    if (preccNew)
    {
        PSZ pszNewFilter = strdup(pszFilter);
        // always make filters upper-case
        strupr(pszNewFilter);

        // insert the record (helpers/winh.c)
        cnrhInsertRecords(pftpd->hwndFiltersCnr,
                        (PRECORDCORE)NULL,      // parent
                        (PRECORDCORE)preccNew,
                        TRUE, // invalidate
                        pszNewFilter,
                        CRA_RECORDREADONLY,
                        1); // one record

        // store the new title in the linked
        // list of items to be cleaned up
        lstAppendItem(pftpd->pllCleanup, pszNewFilter);
    }

    return (preccNew);
}

/*
 *@@ WriteXWPFilters2INI:
 *      this updates the "XWorkplace:FileFilters" key in OS2.INI
 *      when the filters for a certain file type have been changed.
 *
 *      This func automatically determines the file type
 *      to update (== the INI "key") from the "Types" container
 *      and reads the filters for that type for it from the "Filters"
 *      container.
 *
 *      Returns the number of filters written into the INI data.
 */

ULONG WriteXWPFilters2INI(PFILETYPESPAGEDATA pftpd)
{
    ULONG ulrc = 0;

    if (pftpd->pftreccSelected)
    {
        PSZ pszFileType = pftpd->pftreccSelected->recc.pszIcon;
        CHAR    szFilters[2000] = "";   // should suffice
        ULONG   cbFilters = 0;
        PRECORDCORE preccFilter = NULL;
        // now create the filters string for INIAPP_XWPFILEFILTERS
        // from the record core titles in the "Filters" container
        while (TRUE)
        {
            preccFilter = WinSendMsg(pftpd->hwndFiltersCnr,
                                     CM_QUERYRECORD,
                                     preccFilter,   // ignored when CMA_FIRST
                                     MPFROM2SHORT((preccFilter)
                                                    ? CMA_NEXT
                                                    // first call:
                                                    : CMA_FIRST,
                                           CMA_ITEMORDER));

            if ((preccFilter == NULL) || (preccFilter == (PRECORDCORE)-1))
                // record not found: exit
                break;

            cbFilters += sprintf(&(szFilters[cbFilters]),
                                "%s",
                                preccFilter->pszIcon    // filter string
                               ) + 1;
            ulrc++;
        }

        PrfWriteProfileData(HINI_USER,
                            INIAPP_XWPFILEFILTERS, // "XWorkplace:FileFilters"
                            pszFileType,        // from cnr
                            (cbFilters)
                                ? szFilters
                                : NULL,     // no items found: delete key
                            cbFilters);

    }

    return (ulrc);
}

/*
 *@@ UpdateFiltersCnr:
 *      this updates the "Filters" container
 *      (which is only visible in "Filters" mode)
 *      according to FILETYPESPAGEDATA.pftreccSelected.
 *      (This structure is stored in CREATENOTEBOOKPAGE.pUser.)
 *
 *      This gets called when we're in "Filters"
 *      mode and a new record core gets selected in
 *      the container, or if we're switching to
 *      "Filters" mode.
 *
 *      In both cases, FILETYPESPAGEDATA.pftreccSelected
 *      has the currently selected file type in the container.
 */

VOID UpdateFiltersCnr(PFILETYPESPAGEDATA pftpd)
{
    // get text of selected record core
    PSZ pszFileType = pftpd->pftreccSelected->recc.pszIcon;
    // pszFileType now has the selected file type

    // BOOL fEnableRecords = TRUE;
        // this will be set to FALSE for subsequent loops

    // get the XWorkplace-defined filters for this file type
    ULONG cbFiltersData;
    PSZ pszFiltersData = prfhQueryProfileData(HINI_USER,
                                INIAPP_XWPFILEFILTERS,  // "XWorkplace:FileFilters"
                                pszFileType,
                                &cbFiltersData);

    // empty container
    WinSendMsg(pftpd->hwndFiltersCnr,
               CM_REMOVERECORD,
               (MPARAM)NULL,
               MPFROM2SHORT(0,      // all records
                        CMA_FREE | CMA_INVALIDATE));

    if (pszFiltersData)
    {
        // pszFiltersData now has the handles of the associated
        // objects (as decimal strings, which we'll decode now)
        PSZ     pFilter = pszFiltersData;

        if (pFilter)
        {
            // now parse the filters string
            while (*pFilter)
            {
                // add the filter to the "Filters" container
                AddFilter2Cnr(pftpd, pFilter);

                // go for next object filter (after the 0 byte)
                pFilter += strlen(pFilter) + 1;
                if (pFilter >= pszFiltersData + cbFiltersData)
                    break; // while (*pFilter))
            } // end while (*pFilter)
        }

        free(pszFiltersData);
    }
}

/*
 *@@ ftypFileTypesInitPage:
 *      notebook callback function (notebook.c) for the
 *      "File types" page in the "Workplace Shell" object.
 *
 *      This page is maybe the most complicated of the
 *      "Workplace Shell" settings pages, but maybe also
 *      the most useful. ;-)
 *
 *      In this function, we set up the following window
 *      hierarchy (all of which exists in the dialog
 *      template loaded from the NLS DLL):
 *
 +      CREATENOTEBOOKPAGE.hwndPage (dialog in notebook,
 +        |                          maintained by notebook.c)
 +        |
 +        +-- ID_XSDI_FT_CONTAINER (container with file types);
 +        |     this thing is rather smart in that it can handle
 +        |     d'n'd within the same window (see
 +        |     ftypFileTypesItemChanged for details).
 +        |
 +        +-- ID_XSDI_FT_FILTERSCNR (container with filters);
 +        |     this has the filters for the file type (e.g. "*.txt");
 +        |     a plain container in flowed text view.
 +        |     This gets updated via UpdateFiltersCnr.
 +        |
 +        +-- ID_XSDI_FT_ASSOCSCNR (container with associations);
 +              this container is in non-flowed name view to hold
 +              the associations for the current file type. This
 +              accepts WPProgram's and WPProgramFile's via drag'n'drop.
 +              This gets updated via UpdateAssocsCnr.
 *
 *      The means of interaction between all these controls is the
 *      FILETYPESPAGEDATA structure which contains lots of data
 *      so all the different functions know what's going on. This
 *      is created here and stored in CREATENOTEBOOKPAGE.pUser so
 *      that it is always accessible and will be free()'d automatically.
 *
 *@@added V0.9.0 [umoeller]
 */

VOID ftypFileTypesInitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                           ULONG flFlags)        // CBI_* flags (notebook.h)
{
    // PGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    HWND hwndCnr = WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_FT_CONTAINER);

    /*
     * CBI_INIT:
     *      initialize page (called only once)
     */

    if (flFlags & CBI_INIT)
    {
        // HWND    hwndListBox;
        // CNRINFO CnrInfo;

        if (pcnbp->pUser == NULL)
        {
            PFILETYPESPAGEDATA pftpd;
            /* SWP     swpAssocListBox,
                    swpUpButton; */
            // PPRESPARAMS pPresParams;
            // LONG    lColor;

            // first call: create FILETYPESPAGEDATA
            // structure;
            // this memory will be freed automatically by the
            // common notebook window function (notebook.c) when
            // the notebook page is destroyed
            pftpd = malloc(sizeof(FILETYPESPAGEDATA));
            pcnbp->pUser = pftpd;
            memset(pftpd, 0, sizeof(FILETYPESPAGEDATA));

            pftpd->pcnbp = pcnbp;

            // create "cleanup" list; this will hold all kinds
            // of items which need to be free()'d when the
            // notebook page is destroyed.
            // We just keep storing stuff in here so we need not
            // keep track of where we allocated what.
            pftpd->pllCleanup = lstCreate(TRUE);

            // store container hwnd's
            pftpd->hwndTypesCnr = hwndCnr;
            pftpd->hwndFiltersCnr = WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_FT_FILTERSCNR);
            pftpd->hwndAssocsCnr = WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_FT_ASSOCSCNR);

            // setup file types container
            BEGIN_CNRINFO()
            {
                cnrhSetView(CV_TREE | CA_TREELINE | CV_TEXT);
                cnrhSetSortFunc(fnCompareName);
            } END_CNRINFO(hwndCnr);

            // setup filters container
            BEGIN_CNRINFO()
            {
                cnrhSetView(CV_TEXT | CV_FLOW);
                cnrhSetSortFunc(fnCompareName);
            } END_CNRINFO(pftpd->hwndFiltersCnr);

            // setup assocs container
            BEGIN_CNRINFO()
            {
                cnrhSetView(CV_TEXT
                             | CA_ORDEREDTARGETEMPH
                                            // allow dropping only _between_ records
                             | CA_OWNERDRAW);
                                            // for disabled records
                // no sort here
            } END_CNRINFO(pftpd->hwndAssocsCnr);
        }
    }

    /*
     * CBI_SET:
     *      set controls' data
     */

    if (flFlags & CBI_SET)
    {
        PFILETYPESPAGEDATA pftpd = (PFILETYPESPAGEDATA)pcnbp->pUser;

        /*
         * fill container with associations:
         *      this happens in four steps:
         *
         *      1)  load the WPS file types list
         *          from OS2.INI ("PMWP_ASSOC_FILTER")
         *          and create a linked list from
         *          it in FILETYPESPAGEDATA.pllFileTypes;
         *
         *      2)  load the XWorkplace file types
         *          hierarchy from OS2.INI
         *          ("XWorkplace:FileTypes");
         *
         *      3)  insert all hierarchical file
         *          types in that list;
         *
         *      4)  finally, insert all remaining
         *          WPS file types which have not
         *          been found in the hierarchical
         *          list.
         */

        PSZ pszAssocTypeList;

        HPOINTER hptrOld = winhSetWaitPointer();

        // step 1: load WPS file types list
        pszAssocTypeList = prfhQueryKeysForApp(HINI_USER,
                                               WPINIAPP_ASSOCTYPE);
                                                    // "PMWP_ASSOC_TYPE"
        if (pszAssocTypeList)
        {
            PSZ         pKey = pszAssocTypeList;
            PSZ         pszFileTypeHierarchyList;
            PLISTNODE   pAssocNode;

            // if the list had been created before, free it now
            lstFree(pftpd->pllFileTypes);
            // create new empty list
            pftpd->pllFileTypes = lstCreate(TRUE);      // items are freeable

            while (*pKey != 0)
            {
                // for each WPS file type,
                // create a list item
                PFILETYPELISTITEM pliAssoc = malloc(sizeof(FILETYPELISTITEM));
                // mark as "not processed"
                pliAssoc->fProcessed = FALSE;
                // set anti-recursion flag
                pliAssoc->fCircular = FALSE;
                // store file type
                pliAssoc->pszFileType = strdup(pKey);
                // add item to list
                lstAppendItem(pftpd->pllFileTypes, pliAssoc);

                // go for next key
                pKey += strlen(pKey)+1;
            }

            free(pszAssocTypeList);

            // step 2: load XWorkplace file types hierarchy
            WinEnableWindowUpdate(hwndCnr, FALSE);

            pszFileTypeHierarchyList = prfhQueryKeysForApp(
                                            HINI_USER,
                                            INIAPP_XWPFILETYPES);
                                                // "XWorkplace:FileTypes"

            // step 3: go thru the file type hierarchy
            // and add parents;
            // AddFileTypeAndAllParents will mark the
            // inserted items as processed (for step 4)
            if (pszFileTypeHierarchyList)
            {
                pKey = pszFileTypeHierarchyList;
                while (*pKey != 0)
                {
                    AddFileTypeAndAllParents(pftpd, pKey);
                            // this will recurse

                    // go for next key
                    pKey += strlen(pKey)+1;
                }
            }

            // step 4: add all remaining file types
            // to root level
            pAssocNode = lstQueryFirstNode(pftpd->pllFileTypes);
            while (pAssocNode)
            {
                PFILETYPELISTITEM pliAssoc = (PFILETYPELISTITEM)(pAssocNode->pItemData);
                if (!pliAssoc->fProcessed)
                {
                    AddFileType2Cnr(pftpd->hwndTypesCnr,
                                    NULL,       // parent record == root
                                    pliAssoc);
                }
                pAssocNode = pAssocNode->pNext;
            }

            WinShowWindow(hwndCnr, TRUE);
        }

        WinSetPointer(HWND_DESKTOP, hptrOld);
    }

    /*
     * CBI_SHOW / CBI_HIDE:
     *      notebook page is turned to or away from
     */

    if (flFlags & (CBI_SHOW | CBI_HIDE))
    {
        PFILETYPESPAGEDATA pftpd = (PFILETYPESPAGEDATA)pcnbp->pUser;
        if (pftpd->hwndWPSImportDlg)
            WinShowWindow(pftpd->hwndWPSImportDlg, (flFlags & CBI_SHOW));
    }

    /*
     * CBI_DESTROY:
     *      clean up page before destruction
     */

    if (flFlags & CBI_DESTROY)
    {
        PFILETYPESPAGEDATA pftpd = (PFILETYPESPAGEDATA)pcnbp->pUser;
        PLISTNODE pAssocNode = lstQueryFirstNode(pftpd->pllFileTypes);
        PFILETYPELISTITEM pliAssoc;
        while (pAssocNode)
        {
            pliAssoc = pAssocNode->pItemData;
            if (pliAssoc->pszFileType)
                free(pliAssoc->pszFileType);
            // the pliAssoc will be freed by lstFree

            pAssocNode = pAssocNode->pNext;
        }

        lstFree(pftpd->pllFileTypes);

        // destroy "cleanup" list; this will
        // also free all the data on the list
        // using free()
        lstFree(pftpd->pllCleanup);
    }
}

/*
 *@@ ftypFileTypesItemChanged:
 *      notebook callback function (notebook.c) for the
 *      "File types" page in the "Workplace Shell" object.
 *      Reacts to changes of any of the dialog controls.
 *
 *      This is a real monster function, since it handles
 *      all the controls on the page, including container
 *      drag and drop.
 *
 *@@added V0.9.0 [umoeller]
 */

MRESULT ftypFileTypesItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                 USHORT usItemID,
                                 USHORT usNotifyCode,
                                 ULONG ulExtra)      // for checkboxes: contains new state
{
    // PGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    MRESULT mrc = (MPARAM)0;

    PFILETYPESPAGEDATA pftpd = (PFILETYPESPAGEDATA)pcnbp->pUser;

    switch (usItemID)
    {
        /*
         * ID_XSDI_FT_CONTAINER:
         *      "File types" container;
         *      supports d'n'd within itself
         */

        case ID_XSDI_FT_CONTAINER:
        {
            switch (usNotifyCode)
            {
                /*
                 * CN_EMPHASIS:
                 *      new file type selected in file types tree container:
                 *      update the data of the other controls.
                 *      (There can only be one selected record in the tree
                 *      at once.)
                 */

                case CN_EMPHASIS:
                {
                    // ulExtra has new selected record core;
                    if (pftpd->pftreccSelected != (PFILETYPERECORD)ulExtra)
                    {
                        PSZ     pszFileType = 0,
                                pszParentFileType = 0;
                        BOOL    fFirstLoop = TRUE;

                        // store it in FILETYPESPAGEDATA
                        pftpd->pftreccSelected = (PFILETYPERECORD)ulExtra;

                        UpdateFiltersCnr(pftpd);

                        // now go for this file type and keep climbing up
                        // the parent file types
                        pszFileType = strdup(pftpd->pftreccSelected->recc.pszIcon);
                        while (pszFileType)
                        {
                            UpdateAssocsCnr(pftpd->hwndAssocsCnr,
                                                    // cnr to update
                                            pszFileType,
                                                    // file type to search for
                                                    // (selected record core)
                                            WPINIAPP_ASSOCTYPE,  // "PMWP_ASSOC_TYPE"
                                            fFirstLoop,   // empty container first
                                            fFirstLoop);  // enable records
                            fFirstLoop = FALSE;

                            // get parent file type;
                            // this will be NULL if there is none
                            pszParentFileType = prfhQueryProfileData(HINI_USER,
                                                    INIAPP_XWPFILETYPES,    // "XWorkplace:FileTypes"
                                                    pszFileType,
                                                    NULL);
                            free(pszFileType);
                            pszFileType = pszParentFileType;
                        } // end while (pszFileType)

                        if (pftpd->hwndWPSImportDlg)
                            // "Merge" dialog open:
                            // update "Merge with" text
                            WinSetDlgItemText(pftpd->hwndWPSImportDlg,
                                              ID_XSDI_FT_TYPE,
                                              pftpd->pftreccSelected->recc.pszIcon);
                    }
                break; } // case CN_EMPHASIS

                /*
                 * CN_INITDRAG:
                 *      file type being dragged
                 * CN_PICKUP:
                 *      lazy drag initiated (Alt+MB2)
                 */

                case CN_INITDRAG:
                case CN_PICKUP:
                {
                    PCNRDRAGINIT pcdi = (PCNRDRAGINIT)ulExtra;

                    if (DrgQueryDragStatus())
                        // (lazy) drag currently in progress: stop
                        break;

                    if (pcdi)
                        // filter out whitespace
                        if (pcdi->pRecord)
                            cnrhInitDrag(pcdi->hwndCnr,
                                         pcdi->pRecord,
                                         usNotifyCode,
                                         DRAG_RMF,
                                         DO_MOVEABLE);

                break; } // case CN_INITDRAG

                /*
                 * CN_DRAGOVER:
                 *      file type being dragged over other file type
                 */

                case CN_DRAGOVER:
                {
                    PCNRDRAGINFO pcdi = (PCNRDRAGINFO)ulExtra;
                    PDRAGITEM   pdrgItem;
                    USHORT      usIndicator = DOR_NODROP,
                                    // cannot be dropped, but send
                                    // DM_DRAGOVER again
                                usOp = DO_UNKNOWN;
                                    // target-defined drop operation:
                                    // user operation (we don't want
                                    // the WPS to copy anything)

                    // reset global variable
                    pftpd->fFileTypesDnDValid = FALSE;

                    // OK so far:
                    // get access to the drag'n'drop structures
                    if (DrgAccessDraginfo(pcdi->pDragInfo))
                    {
                        if (
                                // accept no more than one single item at a time;
                                // we cannot move more than one file type
                                (pcdi->pDragInfo->cditem != 1)
                                // make sure that we only accept drops from ourselves,
                                // not from other windows
                             || (pcdi->pDragInfo->hwndSource
                                        != pftpd->hwndTypesCnr)
                            )
                        {
                            usIndicator = DOR_NEVERDROP;
                        }
                        else
                        {

                            // accept only default drop operation or move
                            if (    (pcdi->pDragInfo->usOperation == DO_DEFAULT)
                                 || (pcdi->pDragInfo->usOperation == DO_MOVE)
                               )
                            {
                                // get the item being dragged (PDRAGITEM)
                                if (pdrgItem = DrgQueryDragitemPtr(pcdi->pDragInfo, 0))
                                {
                                    if (    (pcdi->pRecord  // target recc
                                              != (PRECORDCORE)pdrgItem->ulItemID) // source recc
                                         && (DrgVerifyRMF(pdrgItem, "DRM_XWPFILETYPES", "DRF_UNKNOWN"))
                                       )
                                    {
                                        // allow drop
                                        usIndicator = DOR_DROP;
                                        pftpd->fFileTypesDnDValid = TRUE;
                                    }
                                }
                            }
                        }

                        DrgFreeDraginfo(pcdi->pDragInfo);
                    }

                    // and return the drop flags
                    mrc = (MRFROM2SHORT(usIndicator, usOp));
                break; } // case CN_DRAGOVER

                /*
                 * CN_DROP:
                 *      file type being dropped
                 *      (both for modal d'n'd and non-modal lazy drag)
                 */

                case CN_DROP:
                {
                    PCNRDRAGINFO pcdi = (PCNRDRAGINFO)ulExtra;

                    // check global valid recc, which was set above
                    if (pftpd->fFileTypesDnDValid)
                    {
                        // get access to the drag'n'drop structures
                        if (DrgAccessDraginfo(pcdi->pDragInfo))
                        {
                            // OK, move the record core tree to the
                            // new location.

                            // This is a bit unusual, because normally
                            // it is the source application which does
                            // the operation as a result of d'n'd. But
                            // who cares, source and target are the
                            // same window here anyway, so let's go.
                            PDRAGITEM   pdrgItem = DrgQueryDragitemPtr(pcdi->pDragInfo, 0);

                            // update container
                            if (cnrhMoveTree(pcdi->pDragInfo->hwndSource, // hwndCnr
                                            // record to move
                                            (PRECORDCORE)(pdrgItem->ulItemID),
                                            // new parent; might be NULL for root level
                                            pcdi->pRecord,
                                            // sort function (cnrsort.c)
                                            (PFNCNRSORT)fnCompareName))
                                // update OS2.INI
                                PrfWriteProfileString(HINI_USER,
                                                // application: "XWorkplace:FileTypes"
                                                INIAPP_XWPFILETYPES,
                                                // key
                                                ((PRECORDCORE)(pdrgItem->ulItemID))->pszIcon,
                                                // string:
                                                (pcdi->pRecord)
                                                    // the parent
                                                    ? pcdi->pRecord->pszIcon
                                                    // NULL == root: delete key
                                                    : NULL);
                                        // aaarrgh
                        }

                        // If CN_DROP was the result of a "real" (modal) d'n'd,
                        // the DrgDrag function in CN_INITDRAG (above)
                        // returns now.

                        // If CN_DROP was the result of a lazy drag (pickup and drop),
                        // the container will now send CN_DROPNOTIFY (below).

                        // In both cases, we clean up the resources: either in
                        // CN_INITDRAG or in CN_DROPNOTIFY.
                    }
                break; } // case CN_DROP

                /*
                 * CN_DROPNOTIFY:
                 *      this is only sent to the container when
                 *      a lazy drag operation (pickup and drop)
                 *      is finished. Since lazy drag is non-modal
                 *      (DrgLazyDrag in CN_PICKUP returned immediately),
                 *      this is where we must clean up the resources
                 *      (the same as we have done in CN_INITDRAG modally).
                 */

                case CN_DROPNOTIFY:
                {
                    PCNRLAZYDRAGINFO pcldi = (PCNRLAZYDRAGINFO)ulExtra;

                    // get access to the drag'n'drop structures
                    if (DrgAccessDraginfo(pcldi->pDragInfo))
                    {
                        // get the moved record core
                        PDRAGITEM   pdrgItem = DrgQueryDragitemPtr(pcldi->pDragInfo, 0);

                        // remove record "pickup" emphasis
                        WinSendMsg(pcldi->pDragInfo->hwndSource, // hwndCnr
                                        CM_SETRECORDEMPHASIS,
                                        // record to move
                                        (MPARAM)(pdrgItem->ulItemID),
                                        MPFROM2SHORT(FALSE,
                                                CRA_PICKED));
                    }

                    // clean up resources
                    DrgDeleteDraginfoStrHandles(pcldi->pDragInfo);
                    DrgFreeDraginfo(pcldi->pDragInfo);

                break; }

                /*
                 * CN_CONTEXTMENU:
                 *      ulExtra has the record core
                 */

                case CN_CONTEXTMENU:
                {
                    HWND    hPopupMenu;

                    // get drag status
                    BOOL    fDragging = DrgQueryDragStatus();

                    // we store the container and recc.
                    // in the CREATENOTEBOOKPAGE structure
                    // so that the notebook.c function can
                    // remove source emphasis later automatically
                    pcnbp->hwndSourceCnr = pcnbp->hwndControl;
                    pcnbp->preccSource = (PRECORDCORE)ulExtra;
                    if (pcnbp->preccSource)
                    {
                        // popup menu on container recc:
                        hPopupMenu = WinLoadMenu(pcnbp->hwndDlgPage,
                                                 cmnQueryNLSModuleHandle(FALSE),
                                                 ID_XSM_FILETYPES_SEL);

                        // if lazy drag is currently in progress,
                        // disable "Pickup" item (we can handle only one)
                        if (fDragging)
                            WinEnableMenuItem(hPopupMenu,
                                              ID_XSMI_FILETYPES_PICKUP, FALSE);
                    }
                    else
                    {
                        // on whitespace: different menu
                        hPopupMenu = WinLoadMenu(pcnbp->hwndDlgPage,
                                                 cmnQueryNLSModuleHandle(FALSE),
                                                 ID_XSM_FILETYPES_NOSEL);

                        if (pftpd->hwndWPSImportDlg)
                            // already open: disable
                            WinEnableMenuItem(hPopupMenu,
                                              ID_XSMI_FILEFILTER_IMPORTWPS, FALSE);
                    }

                    // for both menu types
                    if (!fDragging)
                    {
                        // no lazy drag in progress:
                        // disable drop
                        WinEnableMenuItem(hPopupMenu,
                            ID_XSMI_FILETYPES_DROP, FALSE);
                        // disable cancel-drag
                        WinEnableMenuItem(hPopupMenu,
                            ID_XSMI_FILETYPES_CANCELDRAG, FALSE);
                    }
                    cnrhShowContextMenu(pcnbp->hwndControl,     // cnr
                                        (PRECORDCORE)pcnbp->preccSource,
                                        hPopupMenu,
                                        pcnbp->hwndDlgPage);    // owner
                break; }

            } // end switch (usNotifyCode)


        break; } // case ID_XSDI_FT_CONTAINER

        /*
         * ID_XSMI_FILETYPES_DELETE:
         *      "Delete file type" context menu item
         *      (file types container tree)
         */

        case ID_XSMI_FILETYPES_DELETE:
        {
            PFILETYPERECORD pftrecc = (PFILETYPERECORD)pcnbp->preccSource;
                        // this has been set in CN_CONTEXTMENU above
            if (pftrecc) {
                // delete file type from INI
                PrfWriteProfileString(HINI_USER,
                                      WPINIAPP_ASSOCTYPE, // "PMWP_ASSOC_TYPE",
                                      pftrecc->recc.pszIcon,
                                      NULL);     // delete
                // and remove record core from container
                WinSendMsg(pftpd->hwndTypesCnr,
                           CM_REMOVERECORD,
                           (MPARAM)&pftrecc,
                           MPFROM2SHORT(1,  // only one record
                                    CMA_FREE | CMA_INVALIDATE));
            }
        break; }

        /*
         * ID_XSMI_FILETYPES_NEW:
         *      "Create file type" context menu item
         *      (file types container tree)
         */

        case ID_XSMI_FILETYPES_NEW:
        {
            HWND hwndDlg = WinLoadDlg(HWND_DESKTOP,     // parent
                                      pcnbp->hwndFrame,  // owner
                                      WinDefDlgProc,
                                      cmnQueryNLSModuleHandle(FALSE),
                                      ID_XSD_NEWFILETYPE,   // "New File Type" dlg
                                      NULL);            // pCreateParams
            if (hwndDlg)
            {
                WinSendDlgItemMsg(hwndDlg, ID_XSDI_FT_ENTRYFIELD,
                                    EM_SETTEXTLIMIT,
                                    (MPARAM)50,
                                    MPNULL);
                if (WinProcessDlg(hwndDlg) == DID_OK)
                {
                    // initial data for file type: 1 null-byte,
                    // meaning that no associations have been defined
                    BYTE bData = 0;
                    CHAR szNewType[50];
                    PFILETYPELISTITEM pliAssoc;

                    // get new file type name from dlg
                    WinQueryDlgItemText(hwndDlg, ID_XSDI_FT_ENTRYFIELD,
                                    sizeof(szNewType)-1, szNewType);

                    // write to WPS's file types list
                    PrfWriteProfileData(HINI_USER,
                                        WPINIAPP_ASSOCTYPE, // "PMWP_ASSOC_TYPE"
                                        szNewType,
                                        &bData,
                                        1);     // one byte

                    // create new list item
                    pliAssoc = malloc(sizeof(FILETYPELISTITEM));
                    // mark as "processed"
                    pliAssoc->fProcessed = TRUE;
                    // store file type
                    pliAssoc->pszFileType = strdup(szNewType);
                    // add record core, which will be stored in
                    // pliAssoc->pftrecc
                    AddFileType2Cnr(pftpd->hwndTypesCnr,
                                    NULL,   // parent recc
                                    pliAssoc);
                    lstAppendItem(pftpd->pllFileTypes, pliAssoc);
                }
                WinDestroyWindow(hwndDlg);
            }

        break; }

        /*
         * ID_XSMI_FILETYPES_PICKUP:
         *      "Pickup file type" context menu item
         *      (file types container tree)
         */

        case ID_XSMI_FILETYPES_PICKUP:
        {
            if (    (pcnbp->preccSource)
                 && (pcnbp->preccSource != (PRECORDCORE)-1)
               )
            {
                // initialize lazy drag just as if the
                // user had pressed Alt+MB2
                cnrhInitDrag(pftpd->hwndTypesCnr,
                                pcnbp->preccSource,
                                CN_PICKUP,
                                DRAG_RMF,
                                DO_MOVEABLE);
            }
        break; }

        /*
         * ID_XSMI_FILETYPES_DROP:
         *      "Drop file type" context menu item
         *      (file types container tree)
         */

        case ID_XSMI_FILETYPES_DROP:
        {
            DrgLazyDrop(pftpd->hwndTypesCnr,
                        DO_DEFAULT,
                        &(pcnbp->ptlMenuMousePos));
                            // this is the pointer position at
                            // the time the context menu was
                            // requested, which should be
                            // over the target record core
                            // or container whitespace
        break; }

        /*
         * ID_XSMI_FILETYPES_CANCELDRAG:
         *      "Cancel drag" context menu item
         *      (file types container tree)
         */

        case ID_XSMI_FILETYPES_CANCELDRAG:
        {
            // for one time, this is simple
            DrgCancelLazyDrag();
        break; }

        /*
         * ID_XSDI_FT_FILTERSCNR:
         *      "File filters" container
         */

        case ID_XSDI_FT_FILTERSCNR:
        {
            switch (usNotifyCode)
            {
                /*
                 * CN_CONTEXTMENU:
                 *      ulExtra has the record core
                 */

                case CN_CONTEXTMENU:
                {
                    HWND    hPopupMenu;

                    // get drag status
                    // BOOL    fDragging = DrgQueryDragStatus();

                    // we store the container and recc.
                    // in the CREATENOTEBOOKPAGE structure
                    // so that the notebook.c function can
                    // remove source emphasis later automatically
                    pcnbp->hwndSourceCnr = pcnbp->hwndControl;
                    pcnbp->preccSource = (PRECORDCORE)ulExtra;
                    if (pcnbp->preccSource)
                    {
                        // popup menu on container recc:
                        hPopupMenu = WinLoadMenu(pcnbp->hwndDlgPage,
                                                 cmnQueryNLSModuleHandle(FALSE),
                                                 ID_XSM_FILEFILTER_SEL);
                    }
                    else
                    {
                        // no selection: different menu
                        hPopupMenu = WinLoadMenu(pcnbp->hwndDlgPage,
                                                 cmnQueryNLSModuleHandle(FALSE),
                                                 ID_XSM_FILEFILTER_NOSEL);
                        if (pftpd->hwndWPSImportDlg)
                            // already open: disable
                            WinEnableMenuItem(hPopupMenu,
                                              ID_XSMI_FILEFILTER_IMPORTWPS, FALSE);
                    }

                    cnrhShowContextMenu(pcnbp->hwndControl,
                                           (PRECORDCORE)pcnbp->preccSource,
                                           hPopupMenu,
                                           pcnbp->hwndDlgPage);    // owner
                break; }

            } // end switch (usNotifyCode)
        break; } // case ID_XSDI_FT_FILTERSCNR

        /*
         * ID_XSMI_FILEFILTER_DELETE:
         *      "Delete filter" context menu item
         *      (file filters container)
         */

        case ID_XSMI_FILEFILTER_DELETE:
        {
            ULONG       ulSelection = 0;

            PRECORDCORE precc = cnrhQuerySourceRecord(pftpd->hwndFiltersCnr,
                                                      pcnbp->preccSource,
                                                      &ulSelection);

            while (precc)
            {
                PRECORDCORE preccNext = 0;

                if (ulSelection == SEL_MULTISEL)
                    // get next record first, because we can't query
                    // this after the record has been removed
                    preccNext = cnrhQueryNextSelectedRecord(pftpd->hwndFiltersCnr,
                                                        precc);

                WinSendMsg(pftpd->hwndFiltersCnr,
                           CM_REMOVERECORD,
                           (MPARAM)&precc,
                           MPFROM2SHORT(1,  // only one record
                                    CMA_FREE | CMA_INVALIDATE));

                precc = preccNext;
                // NULL if none
            }

            WriteXWPFilters2INI(pftpd);
        break; }

        /*
         * ID_XSMI_FILEFILTER_NEW:
         *      "New filter" context menu item
         *      (file filters container)
         */

        case ID_XSMI_FILEFILTER_NEW:
        {
            HWND hwndDlg = WinLoadDlg(HWND_DESKTOP,     // parent
                                      pcnbp->hwndFrame,  // owner
                                      WinDefDlgProc,
                                      cmnQueryNLSModuleHandle(FALSE),
                                      ID_XSD_NEWFILTER, // "New Filter" dlg
                                      NULL);            // pCreateParams
            if (hwndDlg)
            {
                WinSendDlgItemMsg(hwndDlg, ID_XSDI_FT_ENTRYFIELD,
                                    EM_SETTEXTLIMIT,
                                    (MPARAM)50,
                                    MPNULL);
                if (WinProcessDlg(hwndDlg) == DID_OK)
                {
                    CHAR szNewFilter[50];
                    // get new file type name from dlg
                    WinQueryDlgItemText(hwndDlg, ID_XSDI_FT_ENTRYFIELD,
                                    sizeof(szNewFilter)-1, szNewFilter);

                    AddFilter2Cnr(pftpd, szNewFilter);
                    WriteXWPFilters2INI(pftpd);
                }
                WinDestroyWindow(hwndDlg);
            }
        break; }

        /*
         * ID_XSMI_FILEFILTER_IMPORTWPS:
         *      "Import filter" context menu item
         *      (file filters container)
         */

        case ID_XSMI_FILEFILTER_IMPORTWPS:
        {
            pftpd->hwndWPSImportDlg = WinLoadDlg(
                           HWND_DESKTOP,     // parent
                           pcnbp->hwndFrame,  // owner
                           fnwpImportWPSFilters,
                           cmnQueryNLSModuleHandle(FALSE),
                           ID_XSD_IMPORTWPS, // "Import WPS Filters" dlg
                           pftpd);           // FILETYPESPAGEDATA for the dlg
            WinShowWindow(pftpd->hwndWPSImportDlg, TRUE);
        break; }

        /*
         * ID_XSDI_FT_ASSOCSCNR:
         *      the "Associations" container, which can handle
         *      drag'n'drop.
         */

        case ID_XSDI_FT_ASSOCSCNR:
        {

            switch (usNotifyCode)
            {

                /*
                 * CN_INITDRAG:
                 *      file type being dragged (we don't support
                 *      lazy drag here, cos I'm too lazy)
                 */

                case CN_INITDRAG:
                {
                    PCNRDRAGINIT pcdi = (PCNRDRAGINIT)ulExtra;

                    if (pcdi)
                        // filter out whitespace
                        if (pcdi->pRecord)
                        {
                            // filter out disabled (parent) associations
                            if ((pcdi->pRecord->flRecordAttr & RECORD_DISABLED) == 0)
                                cnrhInitDrag(pcdi->hwndCnr,
                                                pcdi->pRecord,
                                                usNotifyCode,
                                                DRAG_RMF,
                                                DO_MOVEABLE);
                        }

                break; } // case CN_INITDRAG

                /*
                 * CN_DRAGAFTER:
                 *      something's being dragged over the "Assocs" cnr;
                 *      we allow dropping only for single WPProgram and
                 *      WPProgramFile objects or if one of the associations
                 *      is dragged _within_ the container.
                 *
                 *      Note that since we have set CA_ORDEREDTARGETEMPHASIS
                 *      for the "Assocs" cnr, we do not get CN_DRAGOVER,
                 *      but CN_DRAGAFTER only.
                 */

                case CN_DRAGAFTER:
                {
                    PCNRDRAGINFO pcdi = (PCNRDRAGINFO)ulExtra;
                    USHORT      usIndicator = DOR_NODROP,
                                    // cannot be dropped, but send
                                    // DM_DRAGOVER again
                                usOp = DO_UNKNOWN;
                                    // target-defined drop operation:
                                    // user operation (we don't want
                                    // the WPS to copy anything)

                    // reset global variable
                    pftpd->pobjDrop = NULL;

                    // OK so far:
                    // get access to the drag'n'drop structures
                    if (DrgAccessDraginfo(pcdi->pDragInfo))
                    {
                        if (
                                // accept no more than one single item at a time;
                                // we cannot move more than one file type
                                (pcdi->pDragInfo->cditem != 1)
                                // make sure that we only accept drops from ourselves,
                                // not from other windows
                            )
                        {
                            usIndicator = DOR_NEVERDROP;
                        }
                        else
                        {
                            // OK, we have exactly one item:

                            // 1)   is this a WPProgram or WPProgramFile from the WPS?
                            PDRAGITEM pdrgItem = DrgQueryDragitemPtr(pcdi->pDragInfo, 0);

                            if (DrgVerifyRMF(pdrgItem, "DRM_OBJECT", NULL))
                            {
                                // the WPS stores the MINIRECORDCORE of the
                                // object in ulItemID of the DRAGITEM structure;
                                // we use OBJECT_FROM_PREC to get the SOM pointer
                                WPObject *pObject = OBJECT_FROM_PREC(pdrgItem->ulItemID);

                                if (pObject)
                                {
                                    // dereference shadows
                                    while (     (pObject)
                                             && (_somIsA(pObject, _WPShadow))
                                          )
                                        pObject = _wpQueryShadowedObject(pObject,
                                                            TRUE);  // lock

                                    if (    (_somIsA(pObject, _WPProgram))
                                         || (_somIsA(pObject, _WPProgramFile))
                                       )
                                    {
                                        usIndicator = DOR_DROP;

                                        // store dragged object
                                        pftpd->pobjDrop = pObject;
                                        // store record after which to insert
                                        pftpd->preccAfter = pcdi->pRecord;
                                        if (pftpd->preccAfter == NULL)
                                            // if cnr whitespace: make last
                                            pftpd->preccAfter = (PRECORDCORE)CMA_END;
                                    }
                                }
                            } // end if (DrgVerifyRMF(pdrgItem, "DRM_OBJECT", NULL))

                            if (pftpd->pobjDrop == NULL)
                            {
                                // no object found yet:
                                // 2)   is this a record being dragged within our
                                //      container?
                                if (pcdi->pDragInfo->hwndSource != pftpd->hwndAssocsCnr)
                                    usIndicator = DOR_NEVERDROP;
                                else
                                {
                                    if (    (pcdi->pDragInfo->usOperation == DO_DEFAULT)
                                         || (pcdi->pDragInfo->usOperation == DO_MOVE)
                                       )
                                    {
                                        // do not allow drag upon whitespace,
                                        // but only between records
                                        if (pcdi->pRecord)
                                        {
                                            if (   (pcdi->pRecord == (PRECORDCORE)CMA_FIRST)
                                                || (pcdi->pRecord == (PRECORDCORE)CMA_LAST)
                                               )
                                                // allow drop
                                                usIndicator = DOR_DROP;

                                            // do not allow dropping after
                                            // disabled records
                                            else if ((pcdi->pRecord->flRecordAttr & RECORD_DISABLED) == 0)
                                                usIndicator = DOR_DROP;

                                            if (usIndicator == DOR_DROP)
                                            {
                                                pftpd->pobjDrop = (WPObject*)-1;
                                                        // ugly special flag: this is no
                                                        // object, but our own record core
                                                // store record after which to insert
                                                pftpd->preccAfter = pcdi->pRecord;
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        DrgFreeDraginfo(pcdi->pDragInfo);
                    }

                    // and return the drop flags
                    mrc = (MRFROM2SHORT(usIndicator, usOp));
                break; }

                /*
                 * CN_DROP:
                 *      something _has_ now been dropped on the cnr.
                 */

                case CN_DROP:
                {
                    // check the global variable which has been set
                    // by CN_DRAGOVER above:
                    if (pftpd->pobjDrop)
                    {
                        // CN_DRAGOVER above has considered this valid:

                        if (pftpd->pobjDrop == (WPObject*)-1)
                        {
                            // special flag for record being dragged within
                            // the container:

                            // get access to the drag'n'drop structures
                            PCNRDRAGINFO pcdi = (PCNRDRAGINFO)ulExtra;
                            if (DrgAccessDraginfo(pcdi->pDragInfo))
                            {
                                // OK, move the record core tree to the
                                // new location.

                                // This is a bit unusual, because normally
                                // it is the source application which does
                                // the operation as a result of d'n'd. But
                                // who cares, source and target are the
                                // same window here anyway, so let's go.
                                PDRAGITEM   pdrgItem = DrgQueryDragitemPtr(pcdi->pDragInfo, 0);

                                // update container
                                PRECORDCORE preccMove = (PRECORDCORE)pdrgItem->ulItemID;
                                cnrhMoveRecord(pftpd->hwndAssocsCnr,
                                                  preccMove,
                                                  pftpd->preccAfter);
                            }
                        }
                        else
                        {
                            // WPProgram or WPProgramFile:
                            AddAssocObject2Cnr(pftpd->hwndAssocsCnr,
                                               pftpd->pobjDrop,
                                               pftpd->preccAfter,
                                                    // record to insert after; has been set by CN_DRAGOVER
                                               TRUE);
                                                    // enable record
                        }

                        // write the new associations to INI
                        WriteAssocs2INI(WPINIAPP_ASSOCTYPE, // "PMWP_ASSOC_TYPE",
                                        pftpd->hwndTypesCnr,
                                        pftpd->hwndAssocsCnr);
                        pftpd->pobjDrop = NULL;
                    }
                break; }

                /*
                 * CN_CONTEXTMENU ("Associations" container):
                 *      ulExtra has the record core
                 */

                case CN_CONTEXTMENU:
                {
                    HWND    hPopupMenu;
                    // we store the container and recc.
                    // in the CREATENOTEBOOKPAGE structure
                    // so that the notebook.c function can
                    // remove source emphasis later automatically
                    pcnbp->hwndSourceCnr = pcnbp->hwndControl;
                    pcnbp->preccSource = (PRECORDCORE)ulExtra;

                    if (ulExtra)
                    {
                        // popup menu on record core:
                        hPopupMenu = WinLoadMenu(pcnbp->hwndDlgPage,
                                                 cmnQueryNLSModuleHandle(FALSE),
                                                 ID_XSM_FILEASSOC_SEL);
                        if (pcnbp->preccSource->flRecordAttr & CRA_DISABLED)
                            // association from parent file type:
                            // do not allow deletion
                            WinEnableMenuItem(hPopupMenu,
                                              ID_XSMI_FILEASSOC_DELETE, FALSE);
                    }
                    else
                    {
                        // on whitespace: different menu
                        hPopupMenu = WinLoadMenu(pcnbp->hwndDlgPage,
                                                 cmnQueryNLSModuleHandle(FALSE),
                                                 ID_XSM_FILEASSOC_NOSEL);

                        if (pftpd->hwndWPSImportDlg)
                            // already open: disable
                            WinEnableMenuItem(hPopupMenu,
                                              ID_XSMI_FILEFILTER_IMPORTWPS, FALSE);
                    }

                    cnrhShowContextMenu(pcnbp->hwndControl,
                                        (PRECORDCORE)pcnbp->preccSource,
                                        hPopupMenu,
                                        pcnbp->hwndDlgPage);    // owner
                break; }
            } // end switch (usNotifyCode)

        break; }

        /*
         * ID_XSMI_FILEASSOC_DELETE:
         *      "Delete association" context menu item
         *      (file association container)
         */

        case ID_XSMI_FILEASSOC_DELETE:
        {
            ULONG       ulSelection = 0;

            PRECORDCORE precc = cnrhQuerySourceRecord(pftpd->hwndAssocsCnr,
                                                      pcnbp->preccSource,
                                                      &ulSelection);

            while (precc)
            {
                PRECORDCORE preccNext = 0;

                if (ulSelection == SEL_MULTISEL)
                    // get next record first, because we can't query
                    // this after the record has been removed
                    preccNext = cnrhQueryNextSelectedRecord(pftpd->hwndAssocsCnr,
                                                        precc);

                WinSendMsg(pftpd->hwndAssocsCnr,
                           CM_REMOVERECORD,
                           (MPARAM)&precc,
                           MPFROM2SHORT(1,  // only one record
                                    CMA_FREE | CMA_INVALIDATE));

                precc = preccNext;
                // NULL if none
            }

            // write the new associations to INI
            WriteAssocs2INI(WPINIAPP_ASSOCTYPE, // "PMWP_ASSOC_TYPE",
                            pftpd->hwndTypesCnr,
                            pftpd->hwndAssocsCnr);
        break; }

    }

    return (mrc);
}

/*
 * fnwpImportWPSFilters:
 *      dialog procedure for the "Import WPS Filters" dialog,
 *      which gets loaded upon ID_XSMI_FILEFILTER_IMPORTWPS from
 *      ftypFileTypesItemChanged.
 *
 *      This needs the PFILETYPESPAGEDATA from the notebook
 *      page as a pCreateParam in WinLoadDlg (passed to
 *      WM_INITDLG here).
 *
 *@@added V0.9.0 [umoeller]
 */

MRESULT EXPENTRY fnwpImportWPSFilters(HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    PFILETYPESPAGEDATA pftpd = (PFILETYPESPAGEDATA)WinQueryWindowPtr(hwndDlg,
                                                                     QWL_USER);

    switch (msg)
    {
        /*
         * WM_INITDLG:
         *      mp2 has the PFILETYPESPAGEDATA
         */

        case WM_INITDLG:
        {
            HPOINTER hptrOld = winhSetWaitPointer();

            pftpd = (PFILETYPESPAGEDATA)mp2;
            WinSetWindowPtr(hwndDlg, QWL_USER, pftpd);

            BEGIN_CNRINFO()
            {
                cnrhSetView(CV_TEXT | CA_ORDEREDTARGETEMPH);
            } END_CNRINFO(WinWindowFromID(hwndDlg,
                                          ID_XSDI_FT_ASSOCSCNR));

            if (pftpd)
            {
                PSZ pszKeysList;

                // 1) set the "Merge with" text to
                //    the selected record core

                WinSetDlgItemText(hwndDlg,
                                  ID_XSDI_FT_TYPE,
                                  pftpd->pftreccSelected->recc.pszIcon);

                /*
                PSZ pszKeysList = prfhQueryKeysForApp(
                                            HINI_USER,
                                            WPINIAPP_ASSOCTYPE); // "PMWP_ASSOC_TYPE"

                if (pszKeysList)
                {
                    PSZ         pKey = pszKeysList;
                    HWND        hwndDropDown = WinWindowFromID(hwndDlg,
                                                              ID_XSDI_FT_TYPE);
                    // add each WPS file type to the drop-down box
                    while (*pKey != 0)
                    {
                        WinInsertLboxItem(hwndDropDown,
                                          LIT_SORTASCENDING,
                                          pKey);

                        // go for next key
                        pKey += strlen(pKey)+1;
                    }

                    free(pszKeysList);

                    // select the item in the drop-down box
                    // which corresponds to the selected record
                    // core on the notebook page
                    if (pftpd->pftreccSelected)
                    {
                        SHORT sItem = (SHORT)WinSendMsg(hwndDropDown,
                                                LM_SEARCHSTRING,
                                                MPFROM2SHORT(LSS_CASESENSITIVE,
                                                             LIT_FIRST),
                                                // search string:
                                                pftpd->pftreccSelected->recc.pszIcon);
                        WinSendMsg(hwndDropDown,
                                   LM_SELECTITEM,
                                   (MPARAM)sItem,
                                   (MPARAM)TRUE);    // select
                    }

                } // end if (pszKeysList)
                */

                // 2) fill the left list box with the
                //    WPS-defined filters
                pszKeysList = prfhQueryKeysForApp(HINI_USER,
                                                  WPINIAPP_ASSOCFILTER);
                                                        // "PMWP_ASSOC_FILTER"
                if (pszKeysList)
                {
                    PSZ         pKey = pszKeysList;
                    HWND        hwndListBox = WinWindowFromID(hwndDlg,
                                                              ID_XSDI_FT_FILTERLIST);
                    // add each WPS filter to the list box
                    while (*pKey != 0)
                    {
                        BOOL    fInsert = TRUE;
                        ULONG   ulSize = 0;
                        // insert the list box item only if
                        // a) associations have been defined _and_
                        // b) the filter has not been assigned to a
                        //    file type yet

                        // a) check WPS filters
                        if (!PrfQueryProfileSize(HINI_USER,
                                            WPINIAPP_ASSOCFILTER,  // "PMWP_ASSOC_FILTER"
                                            pKey,
                                            &ulSize))
                            fInsert = FALSE;
                        else if (ulSize < 2)
                            // a length of 1 is the NULL byte == no assocs
                            fInsert = FALSE;
                        else
                            // b) now check XWorkplace filters
                            if (PrfQueryProfileSize(HINI_USER,
                                            INIAPP_XWPFILEFILTERS,  // "XWorkplace:FileFilters"
                                            pKey,
                                            &ulSize))
                                if (ulSize > 1)
                                    fInsert = FALSE;

                        if (fInsert)
                            WinInsertLboxItem(hwndListBox,
                                              LIT_SORTASCENDING,
                                              pKey);

                        // go for next key
                        pKey += strlen(pKey)+1;
                    }

                    free(pszKeysList);

                } // end if (pszKeysList)

            } // end if (pftpd)
            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);

            WinSetPointer(HWND_DESKTOP, hptrOld);
        break; }

        /*
         * WM_CONTROL:
         *
         */

        case WM_CONTROL:
        {
            USHORT usItemID = SHORT1FROMMP(mp1),
                   usNotifyCode = SHORT2FROMMP(mp1);

            switch (usItemID)
            {
                // "Filters" listbox
                case ID_XSDI_FT_FILTERLIST:
                {
                    if (usNotifyCode == LN_SELECT)
                    {
                        // selection changed on the left: fill right container

                        HWND    hwndFiltersListBox = WinWindowFromID(hwndDlg,
                                                              ID_XSDI_FT_FILTERLIST),
                                hwndAssocsCnr     =  WinWindowFromID(hwndDlg,
                                                              ID_XSDI_FT_ASSOCSCNR);
                        SHORT   sItemSelected = LIT_FIRST;
                        CHAR    szFilterText[200];
                        BOOL    fFirstRun = TRUE;

                        HPOINTER hptrOld = winhSetWaitPointer();

                        // OK, something has been selected in
                        // the "Filters" list box.
                        // Since we support multiple selections,
                        // we go thru all the selected items
                        // and add the corresponding filters
                        // to the right container.

                        // WinEnableWindowUpdate(hwndAssocsListBox, FALSE);

                        while (TRUE)
                        {
                            sItemSelected = (SHORT)WinSendMsg(hwndFiltersListBox,
                                       LM_QUERYSELECTION,
                                       (MPARAM)sItemSelected,  // initially LIT_FIRST
                                       MPNULL);

                            if (sItemSelected == LIT_NONE)
                                // no more selections:
                                break;
                            else
                            {
                                WinSendMsg(hwndFiltersListBox,
                                           LM_QUERYITEMTEXT,
                                           MPFROM2SHORT(sItemSelected,
                                                    sizeof(szFilterText)-1),
                                           (MPARAM)szFilterText);

                                UpdateAssocsCnr(hwndAssocsCnr,
                                                        // cnr to update
                                                szFilterText,
                                                        // file filter to search for
                                                WPINIAPP_ASSOCFILTER,
                                                        // "PMWP_ASSOC_FILTER"
                                                fFirstRun,
                                                        // empty container the first time
                                                TRUE);
                                                        // enable all records

                                // winhLboxSelectAll(hwndAssocsListBox, TRUE);

                                fFirstRun = FALSE;
                            }
                        } // end while (TRUE);

                        WinSetPointer(HWND_DESKTOP, hptrOld);
                    }
                break; }
            } // end switch (usItemID)

        break; } // case case WM_CONTROL

        /*
         * WM_COMMAND:
         *      process buttons pressed
         */

        case WM_COMMAND:
        {
            SHORT usCmd = SHORT1FROMMP(mp1);

            switch (usCmd)
            {
                /*
                 * DID_OK:
                 *      "Merge" button
                 */

                case DID_OK:
                {
                    HWND    hwndFiltersListBox = WinWindowFromID(hwndDlg,
                                                          ID_XSDI_FT_FILTERLIST),
                            hwndAssocsCnr     =  WinWindowFromID(hwndDlg,
                                                          ID_XSDI_FT_ASSOCSCNR);
                    SHORT   sItemSelected = LIT_FIRST;
                    CHAR    szFilterText[200];
                    PASSOCRECORD preccThis = (PASSOCRECORD)CMA_FIRST;

                    // 1)  copy the selected filters in the
                    //     "Filters" listbox to the "Filters"
                    //     container on the notebook page

                    while (TRUE)
                    {
                        sItemSelected = (SHORT)WinSendMsg(hwndFiltersListBox,
                                   LM_QUERYSELECTION,
                                   (MPARAM)sItemSelected,  // initially LIT_FIRST
                                   MPNULL);

                        if (sItemSelected == LIT_NONE)
                            break;
                        else
                        {
                            // copy filter from list box to container
                            WinSendMsg(hwndFiltersListBox,
                                       LM_QUERYITEMTEXT,
                                       MPFROM2SHORT(sItemSelected,
                                                sizeof(szFilterText)-1),
                                       (MPARAM)szFilterText);
                            AddFilter2Cnr(pftpd, szFilterText);
                        }
                    } // end while (TRUE);

                    // write the new filters to OS2.INI
                    WriteXWPFilters2INI(pftpd);

                    // 2)  copy all the selected container items from
                    //     the "Associations" container in the dialog
                    //     to the one on the notebook page

                    while (TRUE)
                    {
                        // get first or next selected record
                        preccThis = (PASSOCRECORD)WinSendMsg(hwndAssocsCnr,
                                            CM_QUERYRECORDEMPHASIS,
                                            (MPARAM)preccThis,
                                            (MPARAM)CRA_SELECTED);

                        if ((preccThis == 0) || ((LONG)preccThis == -1))
                            // last or error
                            break;
                        else
                        {
                            // get object from handle
                            WPObject *pobjAssoc = _wpclsQueryObject(_WPObject,
                                                                    preccThis->hobj);

                            if (pobjAssoc)
                                // add the object to the notebook list box
                                AddAssocObject2Cnr(pftpd->hwndAssocsCnr,
                                                   pobjAssoc,
                                                   (PRECORDCORE)CMA_END,
                                                   TRUE);
                        }
                    } // end while (TRUE);

                    // write the new associations to INI
                    WriteAssocs2INI(WPINIAPP_ASSOCTYPE, // "PMWP_ASSOC_TYPE",
                                    pftpd->hwndTypesCnr,
                                        // for selected record core
                                    pftpd->hwndAssocsCnr);
                                        // from updated list box
                break; } // case DID_OK

                case ID_XSDI_FT_SELALL:
                case ID_XSDI_FT_DESELALL:
                    cnrhSelectAll(WinWindowFromID(hwndDlg,
                                                     ID_XSDI_FT_ASSOCSCNR),
                                     (usCmd == ID_XSDI_FT_SELALL)); // TRUE = select
                break;

                default:
                    // this includes "Close"
                    mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
            } // end switch (usCmd)

        break; } // case case WM_CONTROL

        case WM_HELP:
            // display help using the "Workplace Shell" SOM object
            // (in CREATENOTEBOOKPAGE structure)
            cmnDisplayHelp(pftpd->pcnbp->somSelf,
                           pftpd->pcnbp->ulDefaultHelpPanel + 1);
                            // help panel which follows the one on the main page
        break;

        case WM_DESTROY:
            pftpd->hwndWPSImportDlg = NULLHANDLE;
            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
        break;

        default:
            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
    }

    return (mrc);
}


