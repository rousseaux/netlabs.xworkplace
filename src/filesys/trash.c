
/*
 *@@sourcefile trash.c:
 *      this file has implementation code for XWPTrashCan
 *      and XWPTrashObject.
 *
 *      Function prefix for this file:
 *      --  trsh*
 *
 *      This file is ALL new with V0.9.1.
 *
 *@@header "filesys\trash.h"
 */

/*
 *      Copyright (C) 1999-2000 Ulrich M”ller.
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
 *  7)  headers in filesys\ (as needed)
 *  8)  #pragma hdrstop and then more SOM headers which crash with precompiled headers
 */

#define INCL_DOSPROCESS
#define INCL_DOSEXCEPTIONS
#define INCL_DOSSEMAPHORES
#define INCL_DOSERRORS
#define INCL_WINWINDOWMGR
#define INCL_WINMESSAGEMGR
#define INCL_WINPOINTERS
#define INCL_WINDIALOGS
#define INCL_WINBUTTONS
#define INCL_WINLISTBOXES
#define INCL_WINSHELLDATA
#include <os2.h>

// C library headers
#include <stdio.h>              // needed for except.h
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h
#include <io.h>

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\winh.h"               // PM helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\linklist.h"           // linked list helper routines

// SOM headers which don't crash with prec. header files
#include "xtrash.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

#include "filesys\fileops.h"            // file operations implementation
#include "filesys\trash.h"              // trash can implementation
#include "filesys\xthreads.h"           // extra XWorkplace threads

// other SOM headers
#pragma hdrstop
#include "xfobj.h"
#include "helpers\undoc.h"              // some undocumented stuff

/* ******************************************************************
 *                                                                  *
 *   Global variables                                               *
 *                                                                  *
 ********************************************************************/

/*
 * G_abSupportedDrives:
 *      drives support for trash can.
 *      Index 0 is for drive C:, 1 is for D:, and so on.
 *      Each item can be one of the following:
 *      --  XTRC_SUPPORTED: drive is supported.
 *      --  XTRC_SUPPORTED: drive is not supported.
 *      --  XTRC_INVALID: drive letter doesn't exist.
 */

BYTE G_abSupportedDrives[CB_SUPPORTED_DRIVES] = "";

/* ******************************************************************
 *                                                                  *
 *   Trash object creation                                          *
 *                                                                  *
 ********************************************************************/

/*
 *@@ trshUpdateStatusBars:
 *      updates the status bars of all currently
 *      open trash can views. Gets called several
 *      times while the trash can is being populated
 *      (trshPopulateFirstTime) or when objects are
 *      added or removed later.
 *
 *@@added V0.9.1 (2000-02-09) [umoeller]
 */

VOID trshUpdateStatusBars(XWPTrashCan *somSelf)
{
    HWND        ahwndStatusBars[2] = { 0, 0 },
                ahwndCnrs[2] = { 0, 0 },
                hwndFrameThis;
    ULONG       ulStatusBar = 0;

    if (hwndFrameThis = wpshQueryFrameFromView(somSelf,
                                               OPEN_CONTENTS))
    {
        _xwpUpdateStatusBar(somSelf,
                            WinWindowFromID(hwndFrameThis, 0x9001),
                            wpshQueryCnrFromFrame(hwndFrameThis));
    }

    if (hwndFrameThis = wpshQueryFrameFromView(somSelf,
                                               OPEN_DETAILS))
    {
        _xwpUpdateStatusBar(somSelf,
                            WinWindowFromID(hwndFrameThis, 0x9001),
                            wpshQueryCnrFromFrame(hwndFrameThis));
    }
}

/*
 *@@ trshCreateTrashObjectsList:
 *      this creates a new linked list (PLINKLIST, linklist.c,
 *      which is returned) containing all the trash objects in
 *      the specified trash can.
 *
 *      The list's item data pointers point to the XWPTrashObject*
 *      instances directly. Note that each item's related object
 *      on the list can possibly be a folder containing many more
 *      objects.
 *
 *      The list is created in any case, even if the trash can is
 *      empty. lstFree must therefore always be used to free this list.
 *
 *      If (fRelatedObjects == TRUE), not the trash objects, but
 *      the related objects will be added to the list instead.
 *
 *@@changed V0.9.3 (2000-04-28) [umoeller]: now pre-resolving wpQueryContent for speed
 *@@changed V0.9.3 (2000-04-28) [umoeller]: added fRelatedObjects
 */

PLINKLIST trshCreateTrashObjectsList(XWPTrashCan* somSelf,
                                     BOOL fRelatedObjects)
{
    PLINKLIST       pllTrashObjects = lstCreate(FALSE);
                                // FALSE: since we store the XWPTrashObjects directly,
                                // the list node items must not be free()'d
    XWPTrashObject* pTrashObject = 0;

    // pre-resolve _wpQueryContent for speed V0.9.3 (2000-04-28) [umoeller]
    somTD_WPFolder_wpQueryContent rslv_wpQueryContent
            = SOM_Resolve(somSelf, WPFolder, wpQueryContent);

    for (   pTrashObject = rslv_wpQueryContent(somSelf, NULL, (ULONG)QC_FIRST);
            (pTrashObject);
            pTrashObject = rslv_wpQueryContent(somSelf, pTrashObject, (ULONG)QC_NEXT)
        )
    {
        // pTrashObject now has the XWPTrashObject to delete
        lstAppendItem(pllTrashObjects,
                      (fRelatedObjects)
                        ? _xwpQueryRelatedObject(pTrashObject)
                        : pTrashObject);
    }

    return (pllTrashObjects);
}

/*
 *@@ trshCreateTrashObject:
 *      this creates a new transient "trash object" in the
 *      given trashcan by invoking wpclsNew upon the XWPTrashObject
 *      class object (somSelf of this function).
 *
 *      This returns the new trash object. If NULL is returned,
 *      either an error occured creating the trash object or
 *      a trash object with the same pRelatedObject already
 *      existed in the trash can.
 *
 *      WARNING: This does not move the related object to a TRASH
 *      directory.
 *      This function is only used internally by XWPTrashCan to
 *      map the contents of the "\Trash" directories into the
 *      open trashcan, either when the trash can is first populated
 *      (trshPopulateFirstTime) or later, when objects are moved
 *      into the trash can. So do not call this function manually.
 *
 *      To move an object into a trashcan, call
 *      XWPTrashCan::xwpDeleteIntoTrashCan, which automatically
 *      determines whether this function needs to be called.
 *
 *      There is no opposite concept of deleting a trash object.
 *      Instead, delete the related object, which will automatically
 *      destroy the trash object thru XFldObject::wpUnInitData.
 *
 *@@changed V0.9.1 (2000-01-31) [umoeller]: this used to be M_XWPTrashObject::xwpclsCreateTrashObject
 *@@changed V0.9.3 (2000-04-09) [umoeller]: only folders are calculated on File thread now
 */

XWPTrashObject* trshCreateTrashObject(M_XWPTrashObject *somSelf,
                                      XWPTrashCan* pTrashCan, // in: the trashcan to create the object in
                                      WPObject* pRelatedObject) // in: the object which the new trash object
                                                                // should represent
{
    XWPTrashObject *pTrashObject = NULL;

    if (    (pTrashCan)
         && (pRelatedObject)
       )
    {
        CHAR    szSetupString[300];

        #ifdef DEBUG_TRASHCAN
            _Pmpf(("xwpclsCreateTrashObject: Creating trash object \"%s\" in \"%s\"",
                    _wpQueryTitle(pRelatedObject),
                    _wpQueryTitle(pTrashCan)));
        #endif

        // create setup string; we pass the related object
        // as an address string directly. Looks ugly, but
        // it's the only way the trash object can know about
        // the related object immediately during creation.
        // Otherwise the object details are not fully set
        // on creation, and the trash can's Details view
        // flickers like crazy. Duh.
        sprintf(szSetupString,
                "RELATEDOBJECT=%lX",
                pRelatedObject);

        // create XWPTrashObject instance;
        // XWPTrashObject::wpSetupOnce handles the rest
        pTrashObject = _wpclsNew(somSelf,   // class object
                                 _wpQueryTitle(pRelatedObject),
                                        // same title as related object
                                 szSetupString, // setup string
                                 pTrashCan, // where to create the object
                                 TRUE);  // lock
    }

    return (pTrashObject);
}

/*
 *@@ trshSetupOnce:
 *      implementation for XWPTrashObject::wpSetupOnce.
 *      This parses the RELATEDOBJECT setup string
 *      to set the related object accordingly, which has
 *      been given to wpclsNew by trshCreateTrashObject.
 *
 *      Returns FALSE upon errors, which is then passed on
 *      as the return value of wpSetupOnce to abort object
 *      creation.
 *
 *@@added V0.9.3 (2000-04-09) [umoeller]
 */

BOOL trshSetupOnce(XWPTrashObject *somSelf,
                   PSZ pszSetupString)
{
    CHAR    szRelatedAddress[100];
    ULONG   cbRelatedAddress = sizeof(szRelatedAddress);

    // parse "RELATEDOBJECT="
    if (_wpScanSetupString(somSelf,
                           pszSetupString,
                           "RELATEDOBJECT",
                           szRelatedAddress,
                           &cbRelatedAddress))
    {
        // found:
        WPObject *pRelatedObject = NULL;
        XWPTrashCan *pTrashCan = _wpQueryFolder(somSelf);

        sscanf(szRelatedAddress, "%lX", &pRelatedObject);

        if ((pRelatedObject) && (pTrashCan))
        {
            // store related object in trash object
            _xwpSetRelatedObject(somSelf, pRelatedObject);
            // store trash object in related object;
            // this will caus the trash object to be freed
            // when the related object gets destroyed
            _xwpSetTrashObject(pRelatedObject, somSelf);

            // raise trash can "busy" flag by one;
            // this is decreased by trshCalcTrashObjectSize
            // again
            _xwpTrashCanBusy(pTrashCan,
                             +1);        // inc busy

            if (_somIsA(pRelatedObject, _WPFolder))
                // related object is a folder:
                // this can have many objects, so have size
                // of trash object calculated on File thread;
                // this will update the details later
                xthrPostFileMsg(FIM_CALCTRASHOBJECTSIZE,
                                (MPARAM)somSelf,
                                (MPARAM)pTrashCan);
            else
                // non-folder:
                // set size synchronously
                trshCalcTrashObjectSize(somSelf,
                                        pTrashCan);
            return (TRUE);
        }
    }

    return (FALSE);
}

/*
 *@@ trshCalcTrashObjectSize:
 *      implementation for the FIM_CALCTRASHOBJECTSIZE
 *      message on the File thread. This gets called
 *      after a trash object has been created (trshSetupOnce,
 *      while in trshCreateTrashObject) in two situations:
 *
 *      -- on the File thread if the trash object's related
 *         object is a folder, because we need to recurse into
 *         the subfolders then;
 *
 *      -- synchronously by trshSetupOnce directly if the object
 *         is not a folder, because then querying the size is
 *         fast.
 *
 *      This invokes XWPTrashObject::xwpSetExpandedObjectData
 *      on the trash object.
 *
 *@@added V0.9.2 (2000-02-28) [umoeller]
 */

VOID trshCalcTrashObjectSize(XWPTrashObject *pTrashObject,
                             XWPTrashCan *pTrashCan)
{
    if (pTrashObject)
    {
        WPObject *pRelatedObject = _xwpQueryRelatedObject(pTrashObject);
        // create structured file list for this object;
        // if this is a folder, this can possibly take a
        // long time
        PEXPANDEDOBJECT pSOI = fopsExpandObjectDeep(pRelatedObject);
        _xwpSetExpandedObjectData(pTrashObject,
                                  pSOI,
                                  pSOI->ulSizeThis,
                                  pTrashCan);
    }

    _xwpTrashCanBusy(pTrashCan,
                     -1);        // dec busy
}

/* ******************************************************************
 *                                                                  *
 *   Trash can populating                                           *
 *                                                                  *
 ********************************************************************/

/*
 *@@ trshAddTrashObjectsForTrashDir:
 *      this routine gets called from trshPopulateFirstTime
 *      for each "?:\Trash" directory which exists on all
 *      drives. pTrashDir must be the corresponding WPFolder
 *      object for that directory.
 *
 *      We then query the folder's contents and create trash
 *      objects in pTrashCan accordingly. If another folder
 *      is found (which is probable), we recurse.
 *
 *      This returns the total number of trash objects that were
 *      created.
 *
 *@@changed V0.9.1 (2000-01-29) [umoeller]: this returned 0 always; fixed, so that subdirs won't be deleted always
 *@@changed V0.9.3 (2000-04-11) [umoeller]: now returning BOOL
 *@@changed V0.9.3 (2000-04-25) [umoeller]: deleting empty TRASH directories finally works
 *@@changed V0.9.3 (2000-04-28) [umoeller]: now pre-resolving wpQueryContent for speed
 */

BOOL trshAddTrashObjectsForTrashDir(M_XWPTrashObject *pXWPTrashObjectClass, // in: _XWPTrashObject (for speed)
                                    XWPTrashCan *pTrashCan,  // in: trashcan to add objects to
                                    WPFolder *pTrashDir,     // in: trash directory to examine
                                    PULONG pulObjectCount)   // out: object count (req.)
{
    BOOL        brc = TRUE,
                fTrashDirSemOwned = FALSE;
    WPObject    *pObject;
    ULONG       ulObjectCount = 0;

    LINKLIST    llEmptyDirs;        // list of WPFolder's which are to be freed
                                    // because they're empty

    if (!pXWPTrashObjectClass)
        // error
        return (0);

    #ifdef DEBUG_TRASHCAN
        _Pmpf(("  Entering AddTrashObjectsForTrashDir for %s", _wpQueryTitle(pTrashDir)));
    #endif

    lstInit(&llEmptyDirs, FALSE);       // no auto-free items, these are WPFOlder* pointers

    TRY_LOUD(excpt1, NULL)
    {
        // populate
        if (wpshCheckIfPopulated(pTrashDir))
        {
            // request semaphore for that trash dir
            // to protect the contents list
            fTrashDirSemOwned = !_wpRequestObjectMutexSem(pTrashDir, 4000);
            if (fTrashDirSemOwned)
            {
                // pre-resolve _wpQueryContent for speed V0.9.3 (2000-04-28) [umoeller]
                somTD_WPFolder_wpQueryContent rslv_wpQueryContent
                        = SOM_Resolve(pTrashDir, WPFolder, wpQueryContent);

                #ifdef DEBUG_TRASHCAN
                    _Pmpf(("    fTrashDirSemOwned = %d", fTrashDirSemOwned));
                #endif

                for (   pObject = rslv_wpQueryContent(pTrashDir, NULL, (ULONG)QC_FIRST);
                        (pObject);
                        pObject = rslv_wpQueryContent(pTrashDir, pObject, (ULONG)QC_NEXT)
                    )
                {
                    BOOL    fAddTrashObject = TRUE;
                    if (_somIsA(pObject, _WPFolder))
                    {
                        // another folder found:
                        // check the attributes if it's one of the
                        // \TRASH subdirectories added by the trashcan
                        // or maybe a "real" WPS folder that had been
                        // deleted
                        CHAR    szFolderPath[2*CCHMAXPATH] = "";
                        ULONG   cbFolderPath = sizeof(szFolderPath);
                        ULONG   ulAttrs = 0;
                        if (_wpQueryRealName(pObject,
                                             szFolderPath,
                                             &cbFolderPath,
                                             TRUE))
                            if (doshQueryPathAttr(szFolderPath, &ulAttrs) == NO_ERROR)
                                if (ulAttrs & FILE_HIDDEN)
                                {
                                    // hidden directory: this is a trash directory,
                                    // so recurse!
                                    ULONG ulTrashObjectCountSub = 0;

                                    // #ifdef DEBUG_TRASHCAN
                                        _Pmpf(("    Recursing with %s", _wpQueryTitle(pObject)));
                                    // #endif

                                    if (trshAddTrashObjectsForTrashDir(pXWPTrashObjectClass,
                                                                       pTrashCan,
                                                                       pObject, // new trash dir
                                                                       &ulTrashObjectCountSub))
                                        // success:
                                        ulObjectCount += ulTrashObjectCountSub;
                                    else
                                        brc = FALSE;

                                    _Pmpf(("      Got %d objects (%d total) for %s",
                                            ulTrashObjectCountSub, ulObjectCount,
                                            _wpQueryTitle(pObject)));

                                    if (ulTrashObjectCountSub == 0)
                                    {
                                        // no objects found in this trash folder
                                        // or subfolders (if any):
                                        // delete this folder, it's useless,
                                        // but we can only do this outside the wpQueryContent
                                        // loop, so delay this
                                        lstAppendItem(&llEmptyDirs,
                                                      pObject); // the empty WPFolder
                                    }

                                    // don't create a trash object for this directory...
                                    fAddTrashObject = FALSE;
                                }
                    }

                    if (fAddTrashObject)
                    {
                        // non-folder or folder which is not hidden:
                        // add to trashcan!
                        #ifdef DEBUG_TRASHCAN
                            _Pmpf(("    Adding %s, _XWPTrashObject: 0x%lX",
                                        _wpQueryTitle(pObject),
                                        pXWPTrashObjectClass));
                        #endif

                        trshCreateTrashObject(pXWPTrashObjectClass,
                                              pTrashCan,
                                              pObject);  // related object
                        ulObjectCount++;
                    }
                } // end for (   pObject = _wpQueryContent(...
            } // end if (fTrashDirSemOwned)
            else
                cmnLog(__FILE__, __LINE__, __FUNCTION__,
                       "Couldn't request mutex semaphore for \trash subdir.");
        } // end if (wpshCheckIfPopulated(pTrashDir))
        else
            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                   "wpPopulate failed for \trash subdir.");
    }
    CATCH(excpt1)
    {
        // exception occured:
        brc = FALSE;        // report error
    } END_CATCH();

    if (fTrashDirSemOwned)
    {
        _wpReleaseObjectMutexSem(pTrashDir);
        fTrashDirSemOwned = FALSE;
    }

    #ifdef DEBUG_TRASHCAN
        _Pmpf(("  End of AddTrashObjectsForTrashDir for %s;", _wpQueryTitle(pTrashDir)));
        _Pmpf(("    returning %d objects ", ulObjectCount));
    #endif

    // OK, now that we're done running thru the subdirectories,
    // delete the empty ones before returning to the caller
    {
        PLISTNODE   pDirNode = lstQueryFirstNode(&llEmptyDirs);
        while (pDirNode)
        {
            WPFolder *pFolder = (WPFolder*)pDirNode->pItemData;

            _Pmpf(("    Freeing empty folder %s",
                    _wpQueryTitle(pFolder)));

            _wpFree(pFolder);

            pDirNode = pDirNode->pNext;
        }

        lstClear(&llEmptyDirs);
    }


    *pulObjectCount = ulObjectCount;

    return (brc);
}

/*
 *@@ trshPopulateFirstTime:
 *      this gets called when XWPTrashCan::wpPopulate gets
 *      called for the very first time only. In that case,
 *      we need to go over all supported drives and add
 *      trash objects according to the \TRASH directories.
 *
 *      For subsequent wpPopulate calls, this is not
 *      necessary because the trash objects persist while
 *      the WPS is running.
 *
 *@@added V0.9.1 (2000-01-31) [umoeller]
 *@@changed V0.9.1 (2000-02-04) [umoeller]: added status bar updates while populating
 */

BOOL trshPopulateFirstTime(XWPTrashCan *somSelf,
                           ULONG ulFldrFlags)
{
    BOOL        brc = FALSE;
    XWPTrashCanData *somThis = XWPTrashCanGetData(somSelf);

    TRY_LOUD(excpt1, NULL)
    {
        _wpSetFldrFlags(somSelf, ulFldrFlags | FOI_POPULATEINPROGRESS);

        // populate with all:
        if ((ulFldrFlags & FOI_POPULATEDWITHALL) == 0)
        {
            CHAR        szTrashDir[30],
                        cDrive;
            BYTE        bIndex;     // into G_abSupportedDrives[]

            M_WPFolder  *pWPFolderClass = _WPFolder;
            M_XWPTrashObject *pXWPTrashObjectClass = _XWPTrashObject;

            // now go over all drives and create
            // trash objects for all the objects
            // in the "\Trash" directories.
            // Note that M_XWPTrashObject::xwpclsCreateTrashObject
            // will automatically check for whether a trash
            // object exists for a given related object.

            cDrive = 'C';  // (bIndex == 0) = drive C:

            for (bIndex = 0;
                 bIndex < CB_SUPPORTED_DRIVES;
                 bIndex++)
            {
                if (G_abSupportedDrives[bIndex] == XTRC_SUPPORTED)
                {
                    // drive supported:
                    WPFolder    *pTrashDir;

                    // update status bars for open views
                    _cDrivePopulating = cDrive;
                    trshUpdateStatusBars(somSelf);

                    // get "\trash" dir on that drive
                    sprintf(szTrashDir, "%c:\\Trash",
                            cDrive);
                    #ifdef DEBUG_TRASHCAN
                        _Pmpf(("  wpPopulate: Getting trash dir %s", szTrashDir));
                    #endif

                    pTrashDir = _wpclsQueryFolder(pWPFolderClass,   // _WPFolder
                                                  szTrashDir,
                                                  TRUE);       // lock object
                    if (pTrashDir)
                    {
                        ULONG ulObjectCount = 0;
                        // "\Trash" exists for this drive;
                        trshAddTrashObjectsForTrashDir(pXWPTrashObjectClass, // _XWPTrashObject
                                                       somSelf,     // trashcan to add objects to
                                                       pTrashDir,   // initial trash dir;
                                                       &ulObjectCount);
                               // this routine will recurse
                    }
                }
                cDrive++;
            }

            #ifdef DEBUG_TRASHCAN
                _Pmpf(("  wpPopulate: Added new trash objects, now %d", _ulTrashObjectCount));
            #endif

            ulFldrFlags |= (FOI_POPULATEDWITHFOLDERS
                              | FOI_POPULATEDWITHALL);
        }
    } CATCH(excpt1) { } END_CATCH();

    _cDrivePopulating = 0;
    trshUpdateStatusBars(somSelf);
    ulFldrFlags &= ~FOI_POPULATEINPROGRESS;

    _wpSetFldrFlags(somSelf, ulFldrFlags);

    return (brc);
}

/*
 *@@ trshRefresh:
 *      implementation for XWPTrashCan::wpRefresh.
 *      This calls XWPTrashObject::xwpValidateTrashObject
 *      on each trash object in the trash can.
 *
 *@@added V0.9.1 (2000-01-31) [umoeller]
 */

BOOL trshRefresh(XWPTrashCan *somSelf)
{
    // create list of all trash objects
    PLINKLIST   pllTrashObjects = trshCreateTrashObjectsList(somSelf,
                                                             FALSE); // trash objects
    PLISTNODE   pNode = lstQueryFirstNode(pllTrashObjects);

    // now go thru the list
    while (pNode)
    {
        XWPTrashObject* pTrashObject = (XWPTrashObject*)pNode->pItemData;

        _xwpValidateTrashObject(pTrashObject);
        // else error: means that item has been destroyed

        // go for next
        pNode = pNode->pNext;
    }
    lstFree(pllTrashObjects);

    trshUpdateStatusBars(somSelf);

    return (TRUE);
}

/* ******************************************************************
 *                                                                  *
 *   Trash can / trash object operations                            *
 *                                                                  *
 ********************************************************************/

/*
 *@@ trshDeleteIntoTrashCan:
 *      implementation for XWPTrashCan::xwpDeleteIntoTrashCan.
 *
 *      That method gets called in two situations:
 *      1)  from XWPTrashCan::wpDrop;
 *      2)  from fopsFileThreadProcessing, when our file-list
 *          processing is doing a "delete into trashcan" job.
 *          This happens if "Del" is pressed in a folder or if
 *          the "Delete" menu item has been selected.
 *
 *      When an object is thus "deleted" into the trashcan,
 *      this function does the following:
 *
 *      1)  create a hidden directory "\Trash" on the drive
 *          where the object resides, if that directory doesn't
 *          exist already;
 *
 *      2)  create a path in "\Trash" according to the path of
 *          the object; i.e., if "F:\Tools\XFolder\xfldr.dll"
 *          is moved into the trash can, "F:\Trash\Tools\XFolder"
 *          will be created;
 *
 *      3)  move the object which is being deleted into that
 *          directory (using wpMoveObject, so that all WPS
 *          shadows etc. remain valid);
 *
 *      4)  create a new instance of XWPTrashObject in the
 *          trash can (somSelf) which should represent the
 *          object by calling trshCreateTrashObject.
 *          However, this is only done if the trash can has
 *          already been populated (otherwise we'd get duplicate
 *          trash objects in the trash can when populating).
 *
 *      This returns FALSE upon errors.
 *
 *@@added V0.9.1 (2000-02-03) [umoeller]
 */

BOOL trshDeleteIntoTrashCan(XWPTrashCan *pTrashCan, // in: trash can where to create trash object
                            WPObject *pObject)      // in: object to delete
{
    BOOL brc = FALSE;
    XWPTrashCanData *somThis = XWPTrashCanGetData(pTrashCan);

    if (pObject)
    {
        WPFolder *pFolder = _wpQueryFolder(pObject),
                 *pFolderInTrash;
        if (pFolder)
        {
            CHAR szFolder[CCHMAXPATH];
            if (_wpQueryFilename(pFolder, szFolder, TRUE))
            {
                // now we have the directory name where pObject resides
                // in szFolder;
                // get the drive
                CHAR szTrashDir[CCHMAXPATH];

                #ifdef DEBUG_TRASHCAN
                    _Pmpf(("xwpDeleteIntoTrashCan: Source folder: %s", szFolder));
                #endif

                sprintf(szTrashDir, "%c:\\Trash%s",
                        szFolder[0],    // drive letter
                        &(szFolder[2]));   // rest of path

                #ifdef DEBUG_TRASHCAN
                    _Pmpf(("   xwpDeleteIntoTrashCan: Creating dir %s", szTrashDir));
                #endif

                if (!doshQueryDirExist(szTrashDir))
                    // create \trash subdirectory
                    doshCreatePath(szTrashDir,
                                   TRUE);   // hide that directory

                pFolderInTrash = _wpclsQueryFolder(_WPFolder,
                                                   szTrashDir,
                                                   TRUE);       // lock object
                if (pFolderInTrash)
                {
                    // close all open views
                    _wpClose(pObject);

                    if (fopsMoveObjectConfirmed(pObject,
                                                pFolderInTrash))
                    {
                        // successfully moved:

                        // set original object's deletion data
                        // to current date/time
                        _xwpSetDeletion(pObject, TRUE);

                        // return TRUE
                        brc = TRUE;

                        // if the trash can has been populated
                        // already, add a matching trash object;
                        // otherwise wpPopulate will do this
                        // later
                        if (_fAlreadyPopulated)
                        {
                            SOMClass *pTrashObjectClass = _XWPTrashObject;
                            #ifdef DEBUG_TRASHCAN
                                _Pmpf(("xwpDeleteIntoTrashCan: Trash can is populated: creating trash object"));
                                _Pmpf(("  pTrashObjectClass: 0x%lX", pTrashObjectClass));
                            #endif
                            if (pTrashObjectClass)
                            {
                                if (trshCreateTrashObject(pTrashObjectClass,
                                                          pTrashCan,    // trash can
                                                          pObject))
                                    #ifdef DEBUG_TRASHCAN
                                        _Pmpf(("xwpDeleteIntoTrashCan: Created trash object successfully"))
                                    #endif
                                    ;
                            }
                        }
                        else
                        {
                            // not populated yet:
                            #ifdef DEBUG_TRASHCAN
                                _Pmpf(("xwpDeleteIntoTrashCan: Trash can not populated, skipping trash object"));
                            #endif

                            // just raise the number of trash items
                            // and change the icon, wpPopulate will
                            // later correct this number
                            _ulTrashObjectCount++;
                            _xwpSetCorrectTrashIcon(pTrashCan, FALSE);
                        }
                    } // end if (fopsMoveObject(pObject, ...
                }
            }
        }
    }

    return (brc);
}

/*
 *@@ trshRestoreFromTrashCan:
 *      implementation for XWPTrashObject::xwpRestoreFromTrashCan.
 *
 *@@added V0.9.1 (2000-02-01) [umoeller]
 */

BOOL trshRestoreFromTrashCan(XWPTrashObject *pTrashObject,
                             WPFolder *pTargetFolder)
{
    BOOL        brc = FALSE;
    XWPTrashObjectData *somThis = XWPTrashObjectGetData(pTrashObject);

    TRY_LOUD(excpt1, NULL)
    {
        do
        {
            if (_pRelatedObject)
            {
                WPFolder    *pTargetFolder2 = 0;
                PTASKREC    pTaskRecSelf = _wpFindTaskRec(pTrashObject);
                                // this is != NULL if we're being called
                                // from wpMoveObject(somSelf), that is,
                                // the object is dragged out of the trash
                                // can
                PTASKREC    pTaskRecRelated = _wpFindTaskRec(_pRelatedObject);
                                // from what I see, this is NULL always

                #ifdef DEBUG_TRASHCAN
                    _Pmpf(("xwpRestoreFromTrashCan: _pRelatedObject: %s",
                            _wpQueryTitle(_pRelatedObject) ));

                    wpshDumpTaskRec(pTrashObject, "xwpRestoreFromTrashCan (XWPTrashObject)", pTaskRecSelf);
                    wpshDumpTaskRec(pTrashObject, "xwpRestoreFromTrashCan (related obj)", pTaskRecRelated);
                #endif

                if (pTargetFolder)
                {
                    // target folder specified: use that one
                    pTargetFolder2 = pTargetFolder;
                    _wpSetTaskRec(_pRelatedObject,
                                  pTaskRecSelf,
                                  pTaskRecRelated);     // NULL
                }
                else
                {
                    PSZ         pszOriginalPath = NULL;
                    // target folder not specified: use the original dir
                    // where the object was deleted from
                    if (pszOriginalPath = _xwpQueryRelatedPath(pTrashObject))
                    {
                        #ifdef DEBUG_TRASHCAN
                        _Pmpf(("    using original path %s",
                                pszOriginalPath));
                        #endif
                        // make sure that the original directory exists;
                        // it might not if a whole folder (tree) was
                        // moved into the trash can
                        if (!doshQueryDirExist(pszOriginalPath))
                            // doesn't exist: recreate that directory
                            if (doshCreatePath(pszOriginalPath,
                                               FALSE)   // do not hide those directories
                                        != NO_ERROR)
                                // stop and return FALSE
                                break;
                        pTargetFolder2 = _wpclsQueryFolder(_WPFolder,
                                                           pszOriginalPath,
                                                           TRUE);       // lock object
                    }
                }

                if (pTargetFolder2)
                {
                    // folder exists:
                    if (fopsMoveObjectConfirmed(_pRelatedObject,
                                                pTargetFolder2))
                    {
                        // successfully moved:
                        // destroy the trash object
                        if (pTaskRecSelf)
                            _wpSetTaskRec(pTrashObject,
                                          NULL,     // new task rec
                                          pTaskRecSelf); // old task rec
                        brc = _wpFree(pTrashObject);
                    }
                } // end if (pTargetFolder2)
            } // end if (_pRelatedObject)
        } while (FALSE);
    }
    CATCH(excpt1) { } END_CATCH();

    return (brc);
}

/*
 *@@ trshDragOver:
 *      implementation for XWPTrashCan::wpDragOver.
 *
 *      We check whether the object(s) are deleteable and
 *      return flags accordingly.
 *
 *      This must return the return values of DM_DRAGOVER,
 *      which is a MRFROM2SHORT.
 *
 *      -- USHORT 1 usDrop must be:
 *
 *          --  DOR_DROP (0x0001): Object can be dropped.
 *
 *          --  DOR_NODROP (0x0000): Object cannot be dropped at this time,
 *              but type and format are supported. Send DM_DRAGOVER again.
 *
 *          --  DOR_NODROPOP (0x0002):  Object cannot be dropped at this time,
 *              but only the current operation is not acceptable.
 *
 *          --  DOR_NEVERDROP (0x0003): Object cannot be dropped at all. Do
 *              not send DM_DRAGOVER again.
 *
 *      -- USHORT 2 usDefaultOp must specify the default operation, which can be:
 *
 *          --  DO_COPY 0x0010:
 *
 *          --  DO_LINK 0x0018:
 *
 *          --  DO_MOVE 0x0020:
 *
 *          --  DO_CREATE 0x0040: create object from template
 *
 *          --  DO_NEW: from PMREF:
 *              Default operation is create another.  Use create another to create
 *              an object that has default settings and data.  The result of using
 *              create another is identical to creating an object from a template.
 *              This value should be defined as DO_UNKNOWN+3 if it is not
 *              recognized in the current level of the toolkit.
 *
 *          --  Other: This value should be greater than or equal to (>=) DO_UNKNOWN
 *              but not DO_NEW.
 *
 *@@added V0.9.1 (2000-02-01) [umoeller]
 */

MRESULT trshDragOver(XWPTrashCan *somSelf,
                     PDRAGINFO pdrgInfo)
{
    USHORT      usDrop = DOR_NODROP,
                usDefaultOp = DO_MOVE;
    ULONG       ulItemNow = 0;

    if (    (pdrgInfo->usOperation != DO_MOVE)
         && (pdrgInfo->usOperation != DO_DEFAULT)
       )
    {
        usDrop = DOR_NODROPOP;      // invalid drop operation, but send msg again
    }
    else
    {
        // valid operation: set flag to drag OK first
        usDrop = DOR_DROP;

        // valid operation:
        for (ulItemNow = 0;
             ulItemNow < pdrgInfo->cditem;
             ulItemNow++)
        {
            DRAGITEM    drgItem;
            if (DrgQueryDragitem(pdrgInfo,
                                 sizeof(drgItem),
                                 &drgItem,
                                 ulItemNow))
            {
                WPObject *pObjDragged = NULL;
                if (!wpshQueryDraggedObject(&drgItem,
                                            &pObjDragged))
                {
                    // not acceptable:
                    usDrop = DOR_NEVERDROP; // do not send msg again
                    // and stop processing
                    break;
                }
                else
                {
                    // we got the object:
                    // check if it's deletable
                    if (fopsValidateObjOperation(XFT_MOVE2TRASHCAN,
                                                 NULL, // no callback
                                                 pObjDragged)
                            != FOPSERR_OK)
                    {
                        // no:
                        usDrop = DOR_NEVERDROP;
                        break;
                    }
                }
            }
        }
    }

    // compose 2SHORT return value
    return (MRFROM2SHORT(usDrop, usDefaultOp));
}

/*
 *@@ trshMoveDropped2TrashCan:
 *      implementation for XWPTrashCan::wpDrop.
 *
 *      This collects all dropped items from the
 *      DRAGINFO (which was passed to wpDrop) and
 *      starts a file task for moving the items to
 *      the trash can, using fopsStartTaskFromList.
 *
 *      Returns either RC_DROP_ERROR or RC_DROP_DROPCOMPLETE.
 *
 *      We delete the objects into the trash can by invoking
 *      XWPTrashCan::xwpDeleteIntoTrashCan.
 *
 *      This must return one of:
 *
 *      -- RC_DROP_DROPCOMPLETE 2: all objects processed; prohibit
 *         further wpDrop calls
 *
 *      -- RC_DROP_ERROR -1: error occured, terminate drop
 *
 *      -- RC_DROP_ITEMCOMPLETE 1: one item processed, call wpDrop
 *         again for subsequent items
 *
 *      -- RC_DROP_RENDERING 0: request source rendering for this
 *         object, call wpDrop for next object.
 *
 *      We process all items at once here and return RC_DROP_DROPCOMPLETE
 *      unless an error occurs.
 *
 *@@added V0.9.1 (2000-02-01) [umoeller]
 */

MRESULT trshMoveDropped2TrashCan(XWPTrashCan *somSelf,
                                 PDRAGINFO pdrgInfo)
{
    MRESULT     mrc = (MRESULT)RC_DROP_ERROR;
    ULONG       ulItemNow = 0;
    PLINKLIST   pllDroppedObjects = lstCreate(FALSE);   // no auto-free
    BOOL        fItemsAdded = FALSE,
                fStartTask = TRUE;
    WPFolder    *pSourceFolder = NULL;

    for (ulItemNow = 0;
         ulItemNow < pdrgInfo->cditem;
         ulItemNow++)
    {
        DRAGITEM    drgItem;
        if (DrgQueryDragitem(pdrgInfo,
                             sizeof(drgItem),
                             &drgItem,
                             ulItemNow))
        {
            WPObject *pObjectDropped = NULL;

            if (wpshQueryDraggedObject(&drgItem,
                                       &pObjectDropped))
            {
                BOOL    fReportSuccess = TRUE;
                if (fStartTask)
                {
                    // no errors so far:
                    // add item to list
                    FOPSRET frc = FOPSERR_OK;
                    frc = fopsValidateObjOperation(XFT_MOVE2TRASHCAN,
                                                   NULL, // no callback
                                                   pObjectDropped);

                    if (frc == FOPSERR_OK)
                    {
                        lstAppendItem(pllDroppedObjects,
                                      pObjectDropped);
                        pSourceFolder = _wpQueryFolder(pObjectDropped);
                        fItemsAdded = TRUE;
                    }
                    else
                    {
                        fStartTask = FALSE;
                        fReportSuccess = FALSE;
                    }
                }
                else
                    // errors occured already:
                    fReportSuccess = FALSE;

                // notify source of the success of
                // this operation (target rendering)
                WinSendMsg(drgItem.hwndItem,        // source
                           DM_ENDCONVERSATION,
                           (MPARAM)(drgItem.ulItemID),
                           (MPARAM)((fReportSuccess)
                             ? DMFL_TARGETSUCCESSFUL
                             : DMFL_TARGETFAIL));
            }
        }
    }

    if (    (fStartTask)
         && (fItemsAdded)
       )
    {
        // OK:
        // start "move to trashcan" with the new list
        fopsStartTaskFromList(XFT_MOVE2TRASHCAN,
                              pSourceFolder,
                              NULL,             // target folder: not needed
                              pllDroppedObjects);
        lstFree(pllDroppedObjects);

        mrc = (MRESULT)RC_DROP_DROPCOMPLETE;
                // means: _all_ items have been processed,
                // and wpDrop should _not_ be called again
                // by the WPS for the next items, if any

    }
    else
        mrc = (MRESULT)RC_DROP_ERROR;

    return (mrc);
}

/*
 *@@ trshEmptyTrashCan:
 *      implementation for XWPTrashCan::xwpEmptyTrashCan.
 *      This has been redone to use the File thread now.
 *
 *      Emptying the trash can is implemented thru creating
 *      a list of the trash objects in the trash can and
 *      starting a "destroy trash objects" file task with
 *      that list (fopsStartTaskFromList).
 *
 *@@added V0.9.1 (2000-01-31) [umoeller]
 *@@changed V0.9.3 (2000-04-28) [umoeller]: switched implementation to XFT_TRUEDELETE
 */

BOOL trshEmptyTrashCan(XWPTrashCan *somSelf,
                       BOOL fConfirm)
{
    BOOL    brc = FALSE;

    TRY_LOUD(excpt1, NULL)
    {
        BOOL    fContinue = !fConfirm;

        if (fConfirm)
            fContinue = (cmnMessageBoxMsg(HWND_DESKTOP,
                                          168,      // "trash can"
                                          169,      // "really empty?"
                                          MB_YESNO | MB_DEFBUTTON2)
                            == MBID_YES);
        if (fContinue)
        {
            // make sure the trash objects are up-to-date
            PLINKLIST   pllTrashObjects;

            wpshCheckIfPopulated(somSelf);
            pllTrashObjects = trshCreateTrashObjectsList(somSelf,
                                                         TRUE); // related objects

            brc = fopsStartTaskFromList(XFT_TRUEDELETE,
                                        somSelf,        // source folder
                                        NULL,           // target folder: not needed
                                        pllTrashObjects); // list with objects

            lstFree(pllTrashObjects);
        }
    }
    CATCH(excpt1) { } END_CATCH();

    return (brc);
}

/*
 *@@ trshValidateTrashObject:
 *      implementation for XWPTrashObject::xwpValidateTrashObject.
 *      See remarks there.
 *
 *@@added V0.9.2 (2000-02-28) [umoeller]
 */

APIRET trshValidateTrashObject(XWPTrashObject *somSelf)
{
    APIRET arc = NO_ERROR;
    XWPTrashObjectData *somThis = XWPTrashObjectGetData(somSelf);

    if (_pRelatedObject == 0)
        // not set yet:
        arc = ERROR_INVALID_HANDLE;
    else
        if (!wpshCheckObject(_pRelatedObject))
            // pointer invalid:
            arc = ERROR_FILE_NOT_FOUND;
        else
        {
            // object seems to be valid:
            // check if it's a file-system object and
            // if the actual file still exists
            if (_somIsA(_pRelatedObject, _WPFileSystem))
            {
                // yes:
                CHAR szFilename[2*CCHMAXPATH];
                if (_wpQueryFilename(_pRelatedObject, szFilename, TRUE))
                    if (access(szFilename, 0) != 0)
                    {
                        // file doesn't exist any more:
                        arc = ERROR_FILE_NOT_FOUND;
                    }
            }
        }

    if (arc != NO_ERROR)
        // any error found:
        // destroy the object
        _wpFree(somSelf);

    return (arc);
}

/*
 *@@ trshDestroyTrashObject:
 *      implementation for XWPTrashObject::xwpDestroyTrashObject.
 *      See remarks there.
 *
 *@@added V0.9.2 (2000-02-28) [umoeller]
 *@@changed V0.9.3 (2000-04-12) [umoeller]: added XFldObject related object support
 *@@changed V0.9.3 (2000-04-28) [umoeller]: removed completely
 */

/* BOOL trshDestroyTrashObject(XWPTrashObject *somSelf)
{
    BOOL brc = FALSE;

    _Pmpf(("Entering trshDestroyTrashObject for %s", _wpQueryTitle(somSelf)));

    // first check whether the object is valid
    if (trshValidateTrashObject(somSelf) != NO_ERROR)
        // error occured (object wasn't valid anyway):
        // means that somSelf has been destructed already
        brc = TRUE;
    else
    {
        // object still exists:
        XWPTrashObjectData *somThis = XWPTrashObjectGetData(somSelf);

        if (_pRelatedObject)
        {
            // get \trash directory of related object
            WPFolder* pTrashDir = _wpQueryFolder(_pRelatedObject);

            if (pTrashDir)
            {
                // unset "no delete" style flag
                ULONG ulStyle = _wpQueryStyle(_pRelatedObject);

                _Pmpf(("    related object in folder %s", _wpQueryTitle(pTrashDir) ));
                _Pmpf(("    related object style: 0x%lX", ulStyle ));

                if (_somIsA(_pRelatedObject, _WPFileSystem))
                {
                    // unset system, readonly, hidden attributes
                    CHAR    szFilename[CCHMAXPATH] = "";
                    ULONG   ulAttrs = 0;
                    _wpQueryFilename(_pRelatedObject,
                                     szFilename,
                                     TRUE);    // qualified
                    if (doshQueryPathAttr(szFilename,
                                          &ulAttrs)
                                == NO_ERROR)
                        if (ulAttrs & (FILE_READONLY | FILE_SYSTEM | FILE_HIDDEN))
                            _wpSetAttr(_pRelatedObject,
                                       ulAttrs & ~(FILE_READONLY | FILE_SYSTEM | FILE_HIDDEN));
                }

                if ((ulStyle & OBJSTYLE_NODELETE) == 0)
                {
                    // _wpSetStyle(_pRelatedObject, ulStyle & ~OBJSTYLE_NODELETE);

                    // unset trash object in related object;
                    // otherwise XFldObject::wpUnitData attempts to
                    // destroy the trash object, which would recurse...
                    // V0.9.3 (2000-04-12) [umoeller]
                    // _xwpSetRelatedObject(_pRelatedObject, NULL);

                    // destroy related object
                    _Pmpf(("    calling wpFree for related object"));
                    if (_wpFree(_pRelatedObject))
                    {
                        // successfully deleted:
                        // update the trashcan (folder of trash object)
                        // by killing ourselves

                        _Pmpf(("    OK, calling wpFree for trash object"));
                        brc = _wpFree(somSelf);
                        _Pmpf(("    OK!!"));

                        // check if other objects still exist in
                        // the subdirectory of "\Trash" where the
                        // related object was ###
                        while (pTrashDir)
                        {
                            WPFolder* pParent = _wpQueryFolder(pTrashDir);
                            if (!_somIsA(pTrashDir, _WPRootFolder))
                            {
                                // no root folder:
                                // check contents
                                if (_wpQueryContent(pTrashDir, NULL, QC_FIRST) == NULL)
                                {
                                    // no other items in there:
                                    // get rid of it
                                    _wpFree(pTrashDir);
                                }
                            }
                            else
                                // root folder: stop
                                break;

                            // and climb up the folder tree
                            pTrashDir = pParent;
                        }
                    }
                }
            }
        }
    }

    return (brc);
} */

/*
 *@@ trshUninitTrashObject:
 *      implementation for XWPTrashObject::wpUninitData.
 *
 *@@added V0.9.2 (2000-02-28) [umoeller]
 */

VOID trshUninitTrashObject(XWPTrashObject *somSelf)
{
    XWPTrashObjectData *somThis = XWPTrashObjectGetData(somSelf);
    if (_pvExpandedObject)
    {
        fopsFreeExpandedObject((PEXPANDEDOBJECT)_pvExpandedObject);
        _pvExpandedObject = 0;
    }
}

/* ******************************************************************
 *                                                                  *
 *   Trash can drives support                                       *
 *                                                                  *
 ********************************************************************/

/*
 *@@ trshSetDrivesSupport:
 *      implementation for M_XWPTrashCan::xwpclsSetDrivesSupport.
 *
 *@@added V0.9.1 (2000-02-03) [umoeller]
 */

BOOL trshSetDrivesSupport(PBYTE pabSupportedDrives)
{
    BOOL brc = FALSE;

    if (krnLock(5000))
    {
        if (pabSupportedDrives)
        {
            // drives specified:
            memcpy(G_abSupportedDrives, pabSupportedDrives, CB_SUPPORTED_DRIVES);
            // write to INI
            PrfWriteProfileData(HINI_USER,
                                INIAPP_XWORKPLACE, INIKEY_TRASHCANDRIVES,
                                G_abSupportedDrives,
                                sizeof(G_abSupportedDrives));
        }
        else
        {
            // pointer is NULL:
            // CHAR    szFSType[30];
            ULONG   ulLogicalDrive = 3;     // start with drive C:
            BYTE    bIndex = 0;             // index into G_abSupportedDrives

            memset(G_abSupportedDrives, 0, sizeof(G_abSupportedDrives));

            for (ulLogicalDrive = 3;
                 ulLogicalDrive < CB_SUPPORTED_DRIVES + 3;
                 ulLogicalDrive++)
            {
                APIRET  arc = doshAssertDrive(ulLogicalDrive);

                switch (arc)
                {
                    case NO_ERROR:
                        G_abSupportedDrives[bIndex] = XTRC_SUPPORTED;
                    break;

                    case ERROR_INVALID_DRIVE:
                        G_abSupportedDrives[bIndex] = XTRC_INVALID;
                    break;

                    default:
                        // this includes ERROR_NOT_READY, ERROR_NOT_SUPPORTED
                        G_abSupportedDrives[bIndex] = XTRC_UNSUPPORTED;
                }

                bIndex++;
            }

            // delete INI key
            PrfWriteProfileString(HINI_USER,
                                  INIAPP_XWORKPLACE, INIKEY_TRASHCANDRIVES,
                                  NULL);        // delete
        }

        brc = TRUE;

        krnUnlock();
    }

    return (brc);
}

/*
 *@@ trshQueryDrivesSupport:
 *      implementation for M_XWPTrashCan::xwpclsQueryDrivesSupport.
 *
 *@@added V0.9.1 (2000-02-03) [umoeller]
 */

BOOL trshQueryDrivesSupport(PBYTE pabSupportedDrives)
{
    BOOL brc = FALSE;

    if (pabSupportedDrives)
    {
        memcpy(pabSupportedDrives, &G_abSupportedDrives, sizeof(G_abSupportedDrives));
        brc = TRUE;
    }

    return (brc);
}

/*
 *@@ trshLoadDrivesSupport:
 *      loads the drives support data from OS2.INI.
 *      Used in M_XWPTrashCan::wpclsInitData.
 *
 *@@added V0.9.1 (2000-02-03) [umoeller]
 */

VOID trshLoadDrivesSupport(M_XWPTrashCan *somSelf)
{
    ULONG   cbSupportedDrives = sizeof(G_abSupportedDrives);
    memset(G_abSupportedDrives, XTRC_INVALID, cbSupportedDrives);
    if (!PrfQueryProfileData(HINI_USER,
                             INIAPP_XWORKPLACE, INIKEY_TRASHCANDRIVES,
                             G_abSupportedDrives,
                             &cbSupportedDrives))
        // data not found:
        _xwpclsSetDrivesSupport(somSelf,
                                NULL);     // defaults
}

/*
 *@@ trshIsOnSupportedDrive:
 *      returns TRUE only if pObject is on a drive for which
 *      trash can support has been enabled.
 *
 *@@added V0.9.2 (2000-03-04) [umoeller]
 */

BOOL trshIsOnSupportedDrive(WPObject *pObject)
{
    BOOL brc = FALSE;
    WPFolder *pFolder = _wpQueryFolder(pObject);
    if (pFolder)
    {
        CHAR szFolderPath[CCHMAXPATH];
        if (_wpQueryFilename(pFolder, szFolderPath, TRUE))
        {
            strupr(szFolderPath);
            if (szFolderPath[0] >= 'C')
            {
                // is on hard disk:
                if (G_abSupportedDrives[szFolderPath[0] - 'C'] == XTRC_SUPPORTED)
                    brc = TRUE;
            }
        }
    }

    return (brc);
}

/* ******************************************************************
 *                                                                  *
 *   Trash can frame subclassing                                    *
 *                                                                  *
 ********************************************************************/

LINKLIST    llSubclassedTrashCans;
HMTX        hmtxSubclassedTrashCans = NULLHANDLE;

/*
 *@@ trshSubclassTrashCanFrame:
 *      this subclasses the given trash can folder
 *      frame with trsh_fnwpSubclassedTrashCanFrame.
 *
 *      We need to subclass trash can folder frames
 *      again. We cannot use fdr_fnwpSubclassedFolderFrame
 *      because XFolder might not be installed.
 *
 *@@added V0.9.1 (2000-01-31) [umoeller]
 *@@changed V0.9.1 (2000-02-14) [umoeller]: reversed order of functions; now subclassing is last
 */

BOOL trshSubclassTrashCanFrame(HWND hwndFrame,
                               XWPTrashCan *somSelf,
                               ULONG ulView)
{
    BOOL                    brc = FALSE;
    PSUBCLASSEDTRASHFRAME   pstfNew = NULL;
    BOOL                    fSemOwned = FALSE;

    TRY_LOUD(excpt1, NULL)
    {
        // now check if frame wnd has already been subclassed;
        // just another security check
        PFNWP pfnwpCurrent = (PFNWP)WinQueryWindowPtr(hwndFrame, QWP_PFNWP);
        if (pfnwpCurrent != (PFNWP)trsh_fnwpSubclassedTrashCanFrame)
        {
            HWND  hwndCnr = wpshQueryCnrFromFrame(hwndFrame);
            if (hwndCnr)
            {
                pstfNew = malloc(sizeof(SUBCLASSEDTRASHFRAME));
                if (pstfNew)
                {
                    if (hmtxSubclassedTrashCans == NULLHANDLE)
                    {
                        // not initialized yet:
                        lstInit(&llSubclassedTrashCans, TRUE);
                        DosCreateMutexSem(NULL,
                                          &hmtxSubclassedTrashCans, 0, FALSE);
                    }

                    // append to global list
                    fSemOwned = (WinRequestMutexSem(hmtxSubclassedTrashCans,
                                                    4000) == NO_ERROR);
                    if (fSemOwned)
                    {
                        lstAppendItem(&llSubclassedTrashCans,
                                      pstfNew);

                        memset(pstfNew, 0, sizeof(SUBCLASSEDTRASHFRAME));
                        pstfNew->hwndFrame = hwndFrame;
                        pstfNew->somSelf = somSelf;
                        pstfNew->ulView = ulView;
                        pstfNew->pfnwpOrig
                            = WinSubclassWindow(hwndFrame,
                                                trsh_fnwpSubclassedTrashCanFrame);
                        pstfNew->hwndCnr = hwndCnr;
                    }
                    else
                        cmnLog(__FILE__, __LINE__, __FUNCTION__,
                               "hmtxSubclassedTrashCans request failed.");
                }
            }
        }
    }
    CATCH(excpt1) { } END_CATCH();

    if (fSemOwned)
    {
        DosReleaseMutexSem(hmtxSubclassedTrashCans);
        fSemOwned = FALSE;
    }

    return (brc);
}

/*
 *@@ trshQueryPSTF:
 *      finds the corresponding PSUBCLASSEDTRASHFRAME
 *      for the given trash can frame window.
 *
 *@@added V0.9.1 (2000-01-31) [umoeller]
 */

PSUBCLASSEDTRASHFRAME trshQueryPSTF(HWND hwndFrame,        // in: folder frame to find
                                    PULONG pulIndex)       // out: index in linked list if found
{
    PLISTNODE           pNode = 0;
    PSUBCLASSEDTRASHFRAME psliThis = 0,
                        psliFound = 0;
    BOOL                fSemOwned = FALSE;
    ULONG               ulIndex = 0;

    TRY_QUIET(excpt1, NULL)
    {
        if (hwndFrame)
        {
            fSemOwned = (WinRequestMutexSem(hmtxSubclassedTrashCans, 4000) == NO_ERROR);
            if (fSemOwned)
            {
                pNode = lstQueryFirstNode(&llSubclassedTrashCans);
                while (pNode)
                {
                    psliThis = pNode->pItemData;
                    if (psliThis->hwndFrame == hwndFrame)
                    {
                        // item found:
                        psliFound = psliThis;
                        if (pulIndex)
                            *pulIndex = ulIndex;
                        break; // while
                    }

                    pNode = pNode->pNext;
                    ulIndex++;
                }
            }
            else
                cmnLog(__FILE__, __LINE__, __FUNCTION__,
                       "hmtxSubclassedTrashCans request failed.");
        }
    }
    CATCH(excpt1) {  } END_CATCH();

    if (fSemOwned)
    {
        DosReleaseMutexSem(hmtxSubclassedTrashCans);
        fSemOwned = FALSE;
    }

    return (psliFound);
}

/*
 *@@ trshRemovePSTF:
 *      removes a PSUBCLASSEDTRASHFRAME from the
 *      internal list of subclassed trash can frames.
 *
 *@@added V0.9.1 (2000-01-31) [umoeller]
 */

VOID trshRemovePSTF(PSUBCLASSEDTRASHFRAME pstf)
{
    BOOL fSemOwned = FALSE;

    TRY_QUIET(excpt1, NULL)
    {
        fSemOwned = (WinRequestMutexSem(hmtxSubclassedTrashCans, 4000) == NO_ERROR);
        if (fSemOwned)
            lstRemoveItem(&llSubclassedTrashCans,
                          pstf);
        else
            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                   "hmtxSubclassedTrashCans request failed.");
    }
    CATCH(excpt1) { } END_CATCH();

    if (fSemOwned)
    {
        DosReleaseMutexSem(hmtxSubclassedTrashCans);
        fSemOwned = FALSE;
    }
}

/*
 *@@ trsh_fnwpSubclassedTrashCanFrame:
 *      window procedure for subclassed trash can
 *      frames. We cannot use fdr_fnwpSubclassedFolderFrame
 *      because XFolder might not be installed.
 *
 *      This intercepts WM_INITMENU and WM_COMMAND to
 *      implement trash can file processing properly.
 *
 *@@added V0.9.1 (2000-01-31) [umoeller]
 */

MRESULT EXPENTRY trsh_fnwpSubclassedTrashCanFrame(HWND hwndFrame,
                                                  ULONG msg,
                                                  MPARAM mp1,
                                                  MPARAM mp2)
{
    PSUBCLASSEDTRASHFRAME pstf = NULL;
    PFNWP           pfnwpOriginal = NULL;
    MRESULT         mrc = MRFALSE;
    BOOL            fCallDefault = FALSE;

    TRY_LOUD(excpt1, NULL)
    {
        // find the original wnd proc in the
        // global linked list, so we can pass messages
        // on to it
        pstf = trshQueryPSTF(hwndFrame, NULL);
        if (pstf)
            pfnwpOriginal = pstf->pfnwpOrig;

        if (pfnwpOriginal)
        {
            switch(msg)
            {
                case WM_INITMENU:
                    // call the default, in case someone else
                    // is subclassing folders (ObjectDesktop?!?);
                    // from what I've checked, the WPS does NOTHING
                    // with this message, not even for menu bars...
                    mrc = (MRESULT)(*pfnwpOriginal)(hwndFrame, msg, mp1, mp2);

                    // store object with source emphasis for later use;
                    // this gets lost before WM_COMMAND otherwise
                    pstf->pSourceObject = wpshQuerySourceObject(pstf->somSelf,
                                                                pstf->hwndCnr,
                                                                FALSE, // menu mode
                                                                &pstf->ulSelection);
                break;

                /*
                 * WM_COMMAND:
                 *
                 */

                case WM_COMMAND:
                {
                    USHORT usCommand = SHORT1FROMMP(mp1);
                    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
                    LONG lMenuID2 = usCommand - pGlobalSettings->VarMenuOffset;

                    switch (lMenuID2)
                    {
                        case ID_XFMI_OFS_TRASHRESTORE:
                            fopsStartTrashRestoreFromCnr(pstf->somSelf,  // source: trash can
                                                         NULL,           // target folder
                                                         pstf->pSourceObject, // first source object
                                                         pstf->ulSelection,
                                                         pstf->hwndCnr);
                        break;

                        case ID_XFMI_OFS_TRASHDESTROY:
                            fopsStartTrashDestroyFromCnr(pstf->somSelf, // source: trash can
                                                         pstf->pSourceObject,
                                                         pstf->ulSelection,
                                                         pstf->hwndCnr);
                        break;

                        default:
                            // other: default handling
                            fCallDefault = TRUE;
                    }
                break; }

                /*
                 * WM_DESTROY:
                 *      upon this, we need to clean up our linked list
                 *      of subclassed windows
                 */

                case WM_DESTROY:
                {
                    // upon closing the window, undo the subclassing, in case
                    // some other message still comes in
                    // (there are usually still two more, even after WM_DESTROY!!)
                    WinSubclassWindow(hwndFrame, pfnwpOriginal);

                    // remove this window from our subclassing linked list
                    trshRemovePSTF(pstf);

                    // do the default stuff
                    fCallDefault = TRUE;
                break; }

                default:
                    fCallDefault = TRUE;
                break;
            }
        } // end if (pfnwpOriginal)
        else
        {
            // original window procedure not found:
            // that's an error
            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                   "Trash can's pfnwpOriginal not found.");
            mrc = WinDefWindowProc(hwndFrame, msg, mp1, mp2);
        }
    } // end TRY_LOUD
    CATCH(excpt1)
    {
        // exception occured:
        return (0);
    } END_CATCH();

    if (fCallDefault)
    {
        // this has only been set to TRUE for "default" in
        // the switch statement above; we then call the
        // default window procedure.
        // This is either fdr_fnwpSubclassedFolderFrame or
        // the original WPS folder frame proc.
        // We do this outside the TRY/CATCH stuff above so that
        // we don't get blamed for exceptions which we are not
        // responsible for, which was the case with XFolder < 0.85
        // (i.e. exceptions in PMWP.DLL or Object Desktop or whatever).
        mrc = (MRESULT)(*pfnwpOriginal)(hwndFrame, msg, mp1, mp2);
    }

    return (mrc);
}

/* ******************************************************************
 *                                                                  *
 *   XWPTrashCan notebook callbacks (notebook.c)                    *
 *                                                                  *
 ********************************************************************/

/*
 *@@ trshTrashCanSettingsInitPage:
 *      notebook callback function (notebook.c) for the
 *      "TrashCan" settings page.
 *      Sets the controls on the page according to the
 *      Global Settings.
 */

VOID trshTrashCanSettingsInitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                                  ULONG flFlags)        // CBI_* flags (notebook.h)
{
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    if (flFlags & CBI_INIT)
    {
        if (pcnbp->pUser == NULL)
        {
            // first call: backup Global Settings for "Undo" button;
            // this memory will be freed automatically by the
            // common notebook window function (notebook.c) when
            // the notebook page is destroyed
            pcnbp->pUser = malloc(sizeof(GLOBALSETTINGS));
            memcpy(pcnbp->pUser, pGlobalSettings, sizeof(GLOBALSETTINGS));
        }
    }

    if (flFlags & CBI_ENABLE)
    {
        /* PCKERNELGLOBALS pKernelGlobals = krnQueryGlobals();
        WinEnableControl(pcnbp->hwndDlgPage, ID_XTDI_DELETE,
                          (pKernelGlobals->fXFldObject)); */
    }

    if (flFlags & CBI_SET)
    {
        /* winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XTDI_DELETE,
                              pGlobalSettings->fTrashDelete); */
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XTDI_EMPTYSTARTUP,
                              pGlobalSettings->fTrashEmptyStartup);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XTDI_EMPTYSHUTDOWN,
                              pGlobalSettings->fTrashEmptyShutdown);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XTDI_CONFIRMEMPTY,
                              (pGlobalSettings->ulTrashConfirmEmpty & TRSHCONF_EMPTYTRASH)
                                    != 0);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XTDI_CONFIRMDESTROY,
                              (pGlobalSettings->ulTrashConfirmEmpty & TRSHCONF_DESTROYOBJ)
                                    != 0);
    }
}

/*
 *@@ trshTrashCanSettingsItemChanged:
 *      notebook callback function (notebook.c) for the
 *      "TrashCan" settings page.
 *      Reacts to changes of any of the dialog controls.
 */

MRESULT trshTrashCanSettingsItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                        USHORT usItemID, USHORT usNotifyCode,
                                        ULONG ulExtra)      // for checkboxes: contains new state
{
    GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(5000);
    MRESULT mrc = (MPARAM)0;
    BOOL fSave = TRUE;

    switch (usItemID)
    {
        /* case ID_XTDI_DELETE:
            pGlobalSettings->fTrashDelete = ulExtra;
        break; */

        case ID_XTDI_EMPTYSTARTUP:
            pGlobalSettings->fTrashEmptyStartup = ulExtra;
        break;

        case ID_XTDI_EMPTYSHUTDOWN:
            pGlobalSettings->fTrashEmptyShutdown = ulExtra;
        break;

        case ID_XTDI_CONFIRMEMPTY:
            if (ulExtra)
                pGlobalSettings->ulTrashConfirmEmpty |= TRSHCONF_EMPTYTRASH;
            else
                pGlobalSettings->ulTrashConfirmEmpty &= ~TRSHCONF_EMPTYTRASH;
        break;

        case ID_XTDI_CONFIRMDESTROY:
            if (ulExtra)
                pGlobalSettings->ulTrashConfirmEmpty |= TRSHCONF_DESTROYOBJ;
            else
                pGlobalSettings->ulTrashConfirmEmpty &= ~TRSHCONF_DESTROYOBJ;
        break;

        case DID_UNDO:
        {
            // "Undo" button: get pointer to backed-up Global Settings
            GLOBALSETTINGS *pGSBackup = (GLOBALSETTINGS*)(pcnbp->pUser);

            // and restore the settings for this page
            // pGlobalSettings->fTrashDelete = pGSBackup->fTrashDelete;
            pGlobalSettings->fTrashEmptyStartup = pGSBackup->fTrashEmptyStartup;
            pGlobalSettings->fTrashEmptyShutdown = pGSBackup->fTrashEmptyShutdown;
            pGlobalSettings->ulTrashConfirmEmpty = pGSBackup->ulTrashConfirmEmpty;

            // update the display by calling the INIT callback
            (*(pcnbp->pfncbInitPage))(pcnbp, CBI_SET | CBI_ENABLE);
        break; }

        case DID_DEFAULT:
        {
            // set the default settings for this settings page
            // (this is in common.c because it's also used at
            // WPS startup)
            cmnSetDefaultSettings(pcnbp->ulPageID);
            // update the display by calling the INIT callback
            (*(pcnbp->pfncbInitPage))(pcnbp, CBI_SET | CBI_ENABLE);
        break; }

        default:
            fSave = FALSE;
    }

    cmnUnlockGlobalSettings();

    if (fSave)
        cmnStoreGlobalSettings();

    return (mrc);
}

/*
 *@@ trshTrashCanDrivesInitPage:
 *      notebook callback function (notebook.c) for the
 *      trash can "Drives" settings page.
 *      Sets the controls on the page according to the
 *      Global Settings.
 */

VOID trshTrashCanDrivesInitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                                ULONG flFlags)        // CBI_* flags (notebook.h)
{
    // PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    if (flFlags & CBI_INIT)
    {
        if (pcnbp->pUser == NULL)
        {
            // first call: backup drives array for "Undo" button;
            // this memory will be freed automatically by the
            // common notebook window function (notebook.c) when
            // the notebook page is destroyed
            pcnbp->pUser = malloc(CB_SUPPORTED_DRIVES);
            _xwpclsQueryDrivesSupport(_XWPTrashCan, pcnbp->pUser);
            // memcpy(pcnbp->pUser, abSupportedDrives, sizeof(abSupportedDrives));
        }
    }

    if (flFlags & CBI_ENABLE)
    {
        // enable "Add" button if items are selected
        // in the "Unsupported" listbox
        WinEnableControl(pcnbp->hwndDlgPage, ID_XTDI_ADD_SUPPORTED,
                         (winhQueryLboxSelectedItem(WinWindowFromID(pcnbp->hwndDlgPage,
                                                                    ID_XTDI_UNSUPPORTED_LB),
                                                     LIT_FIRST)
                                != LIT_NONE));
        // enable "Remove" button if items are selected
        // in the "Supported" listbox
        WinEnableControl(pcnbp->hwndDlgPage, ID_XTDI_REMOVE_SUPPORTED,
                         (winhQueryLboxSelectedItem(WinWindowFromID(pcnbp->hwndDlgPage,
                                                                    ID_XTDI_SUPPORTED_LB),
                                                     LIT_FIRST)
                                != LIT_NONE));
    }

    if (flFlags & CBI_SET)
    {
        ULONG   bIndex = 0;
        CHAR    szDriveName[3] = "C:";
        HWND    hwndSupportedLB = WinWindowFromID(pcnbp->hwndDlgPage, ID_XTDI_SUPPORTED_LB),
                hwndUnsupportedLB = WinWindowFromID(pcnbp->hwndDlgPage, ID_XTDI_UNSUPPORTED_LB);

        winhDeleteAllItems(hwndSupportedLB);
        winhDeleteAllItems(hwndUnsupportedLB);

        for (bIndex = 0;
             bIndex < CB_SUPPORTED_DRIVES;
             bIndex++)
        {
            if (G_abSupportedDrives[bIndex] != XTRC_INVALID)
            {
                // if drive is supported, insert into "supported",
                // otherwise into "unsupported"
                HWND    hwndLbox = (G_abSupportedDrives[bIndex] == XTRC_SUPPORTED)
                                        ? hwndSupportedLB
                                        : hwndUnsupportedLB;
                // insert
                LONG lInserted = WinInsertLboxItem(hwndLbox,
                                                   LIT_END,
                                                   szDriveName);
                // set item's handle to array index
                winhSetLboxItemHandle(hwndLbox,
                                      lInserted,
                                      bIndex);
            }

            // raise drive letter
            (szDriveName[0])++;
        }
    }
}

/*
 *@@ StoreSupportedDrives:
 *      this gets called from trshTrashCanDrivesItemChanged
 *      to read the (un)supported drives from the listbox.
 *
 *@@added V0.9.1 (99-12-14) [umoeller]
 */

BOOL StoreSupportedDrives(HWND hwndSupportedLB, // in: list box with supported drives
                          HWND hwndUnsupportedLB) // in: list box with unsupported drives
{
    BOOL    brc = FALSE;

    if ((hwndSupportedLB) && (hwndUnsupportedLB))
    {
        BYTE    abSupportedDrivesNew[CB_SUPPORTED_DRIVES];
        SHORT   sIndexThis = 0;
        SHORT   sItemCount;

        // set all drives to "invalid"; only those
        // items will be overwritten which are in the
        // listboxes
        memset(abSupportedDrivesNew, XTRC_INVALID, sizeof(abSupportedDrivesNew));

        // go thru "supported" listbox
        sItemCount = winhQueryLboxItemCount(hwndSupportedLB);
        for (sIndexThis = 0;
             sIndexThis < sItemCount;
             sIndexThis++)
        {
            ULONG   ulIndexHandle = winhQueryLboxItemHandle(hwndSupportedLB,
                                                            sIndexThis);
            abSupportedDrivesNew[ulIndexHandle] = XTRC_SUPPORTED;
        }

        // go thru "unsupported" listbox
        sItemCount = winhQueryLboxItemCount(hwndUnsupportedLB);
        for (sIndexThis = 0;
             sIndexThis < sItemCount;
             sIndexThis++)
        {
            ULONG   ulIndexHandle = winhQueryLboxItemHandle(hwndUnsupportedLB,
                                                            sIndexThis);
            abSupportedDrivesNew[ulIndexHandle] = XTRC_UNSUPPORTED;
        }

        // update trash can class with that data
        _xwpclsSetDrivesSupport(_XWPTrashCan,
                                abSupportedDrivesNew);
    }

    return (brc);
}

/*
 *@@ trshTrashCanDrivesItemChanged:
 *      notebook callback function (notebook.c) for the
 *      trash can "Drives" settings page.
 *      Reacts to changes of any of the dialog controls.
 */

MRESULT trshTrashCanDrivesItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                      USHORT usItemID, USHORT usNotifyCode,
                                      ULONG ulExtra)      // for checkboxes: contains new state
{
    MRESULT mrc = (MPARAM)0;
    BOOL fSave = TRUE;

    static BOOL fNoDeselection = FALSE;

    switch (usItemID)
    {
        /*
         * ID_XTDI_UNSUPPORTED_LB:
         * ID_XTDI_SUPPORTED_LB:
         *      if list box selections change,
         *      re-enable "Add"/"Remove" buttons
         */

        case ID_XTDI_UNSUPPORTED_LB:
        case ID_XTDI_SUPPORTED_LB:
            if (usNotifyCode == LN_SELECT)
            {
                // deselect all items in the other listbox
                if (!fNoDeselection)
                {
                    fNoDeselection = TRUE;
                            // this recurses
                    winhLboxSelectAll(WinWindowFromID(pcnbp->hwndDlgPage,
                                                      ((usItemID == ID_XTDI_UNSUPPORTED_LB)
                                                        ? ID_XTDI_SUPPORTED_LB
                                                        : ID_XTDI_UNSUPPORTED_LB)),
                                      FALSE); // deselect
                    fNoDeselection = FALSE;
                }

                // re-enable items
                (*(pcnbp->pfncbInitPage))(pcnbp, CBI_ENABLE);
            }
        break;

        /*
         * ID_XTDI_ADD_SUPPORTED:
         *      "Add" button: move item(s)
         *      from "unsupported" to "supported"
         *      list box
         */

        case ID_XTDI_ADD_SUPPORTED:
        {
            SHORT   sItemStart = LIT_FIRST;
            HWND    hwndSupportedLB = WinWindowFromID(pcnbp->hwndDlgPage, ID_XTDI_SUPPORTED_LB),
                    hwndUnsupportedLB = WinWindowFromID(pcnbp->hwndDlgPage, ID_XTDI_UNSUPPORTED_LB);
            // CHAR    szItemText[10];

            fNoDeselection = TRUE;
            while (TRUE)
            {
                sItemStart = winhQueryLboxSelectedItem(hwndUnsupportedLB,
                                                       sItemStart);
                if (sItemStart == LIT_NONE)
                    break;

                // move item
                winhMoveLboxItem(hwndUnsupportedLB,
                                 sItemStart,
                                 hwndSupportedLB,
                                 LIT_SORTASCENDING,
                                 TRUE);         // select
            }
            fNoDeselection = FALSE;

            // re-enable buttons
            (*(pcnbp->pfncbInitPage))(pcnbp, CBI_ENABLE);

            // update internal drives data
            StoreSupportedDrives(hwndSupportedLB,
                                 hwndUnsupportedLB);
        break; }

        /*
         * ID_XTDI_REMOVE_SUPPORTED:
         *      "Add" button: move item(s)
         *      from "unsupported" to "supported"
         *      list box
         */

        case ID_XTDI_REMOVE_SUPPORTED:
        {
            SHORT   sItemStart = LIT_FIRST;
            HWND    hwndSupportedLB = WinWindowFromID(pcnbp->hwndDlgPage, ID_XTDI_SUPPORTED_LB),
                    hwndUnsupportedLB = WinWindowFromID(pcnbp->hwndDlgPage, ID_XTDI_UNSUPPORTED_LB);
            // CHAR    szItemText[10];

            fNoDeselection = TRUE;
            while (TRUE)
            {
                sItemStart = winhQueryLboxSelectedItem(hwndSupportedLB,
                                                       sItemStart);
                if (sItemStart == LIT_NONE)
                    break;

                // move item
                winhMoveLboxItem(hwndSupportedLB,
                                 sItemStart,
                                 hwndUnsupportedLB,
                                 LIT_SORTASCENDING,
                                 TRUE);         // select
            }
            fNoDeselection = FALSE;

            // re-enable buttons
            (*(pcnbp->pfncbInitPage))(pcnbp, CBI_ENABLE);

            // update internal drives data
            StoreSupportedDrives(hwndSupportedLB,
                                 hwndUnsupportedLB);
        break; }

        case DID_UNDO:
        {
            // copy array back which was stored in init callback
            _xwpclsSetDrivesSupport(_XWPTrashCan,
                                    pcnbp->pUser);  // backup data
            // update the display by calling the INIT callback
            (*(pcnbp->pfncbInitPage))(pcnbp, CBI_SET | CBI_ENABLE);
        break; }

        case DID_DEFAULT:
        {
            // set defaults
            _xwpclsSetDrivesSupport(_XWPTrashCan,
                                    NULL);     // defaults
            // update the display by calling the INIT callback
            (*(pcnbp->pfncbInitPage))(pcnbp, CBI_SET | CBI_ENABLE);
        break; }

        default:
            fSave = FALSE;
    }

    if (fSave)
        cmnStoreGlobalSettings();

    return (mrc);
}


