
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
 *   Trash can populating                                           *
 *                                                                  *
 ********************************************************************/

/*
 *@@ trshCreateTrashObjectsList:
 *      this creates a new linked list (PLINKLIST, linklist.c,
 *      which is returned) containing all the trash objects in
 *      the specified trash can.
 *
 *      The list's item data pointers point to the XWPTrashObject*
 *      instances directly.
 *
 *      Note that the list is created in any case, even if the
 *      trash can is empty. lstFree must therefore always be
 *      used to free this list.
 */

PLINKLIST trshCreateTrashObjectsList(XWPTrashCan* somSelf)
{
    PLINKLIST       pllTrashObjects = lstCreate(FALSE);
                                // FALSE: since we store the XWPTrashObjects directly,
                                // the list node items must not be free()'d
    XWPTrashObject* pTrashObject = 0;

    for (   pTrashObject = _wpQueryContent(somSelf, NULL, (ULONG)QC_FIRST);
            (pTrashObject);
            pTrashObject = _wpQueryContent(somSelf, pTrashObject, (ULONG)QC_NEXT)
        )
    {
        // pTrashObject now has the XWPTrashObject to delete
        lstAppendItem(pllTrashObjects, pTrashObject);
    }

    return (pllTrashObjects);
}

/*
 *@@ trshCreateTrashObject:
 *      this creates a new transient "trash object" in the
 *      given trashcan by invoking wpclsNew upon the XWPTrashObject
 *      class object (somSelf of this function).
 *
 *      Arguments:
 *      -- pTrashCan: the trashcan to create the object in.
 *      -- pRelatedObject: the object which the new trash object
 *         should be related to. We use a RELATEDOBJECT settings
 *         string with wpclsNew to set the related object
 *         (see XWPTrashObject::wpSetup).
 *
 *      This returns the new trash object. If NULL is returned,
 *      either an error occured creating the trash object or
 *      a trash object with the same pRelatedObject already
 *      existed in the trash can.
 *
 *      WARNING: This does not move the related object to a trash can.
 *      This method is only used internally by XWPTrashCan to
 *      map the contents of the "\Trash" directories into the
 *      open trashcan.
 *
 *      To move an object into a trashcan, call
 *      XWPTrashCan::xwpDeleteIntoTrashCan, which automatically
 *      determines whether this function needs to be called.
 *
 *@@changed V0.9.1 (2000-01-31) [umoeller]: this used to be M_XWPTrashObject::xwpclsCreateTrashObject
 */

XWPTrashObject* trshCreateTrashObject(M_XWPTrashObject *somSelf,
                                      XWPTrashCan* pTrashCan,
                                      WPObject* pRelatedObject)
{
    XWPTrashObject *pTrashObject = NULL;

    /* M_XWPTrashObjectData *somThis = M_XWPTrashObjectGetData(somSelf); */
    // M_XWPTrashObjectMethodDebug("M_XWPTrashObject","xtroM_xwpclsCreateTrashObject");

    if (    (pTrashCan)
         && (pRelatedObject)
       )
    {
        CHAR        szSetupString[100];
        // PLINKLIST   pllTrashObjects;
        // PLISTNODE   pNode = 0;
        /* BOOL        fRelatedExistsAlready = FALSE,
                    fTrashCanSemOwned = FALSE;

        TRY_LOUD(excpt1, NULL)
        {
            PLINKLIST pllTrashObjects;
            PLISTNODE pNode = 0;

            // check if the object already exists in the
            // trash can; we must do this because wpPopulate
            // gets called from wpRefresh also.
            // If the trash can has not been populated yet,
            // we need not worry, because then we definitely
            // have no trash objects in the trash can.
            fTrashCanSemOwned = !_wpRequestObjectMutexSem(pTrashCan, SEM_INDEFINITE_WAIT);
            if (fTrashCanSemOwned)
            {
                pllTrashObjects = trshCreateTrashObjectsList(pTrashCan);

                // now delete the items
                pNode = lstQueryFirstNode(pllTrashObjects);
                while (pNode)
                {
                    WPObject *pTestRelatedObject
                        = _xwpQueryRelatedObject((XWPTrashObject*)pNode->pItemData);
                                        // item data is trash object

                    if (pTestRelatedObject == pRelatedObject)
                    {
                        // exists already: set flag and break
                        fRelatedExistsAlready = TRUE;
                        break; // while (pNode)
                    }

                    // go for next
                    pNode = pNode->pNext;
                } // end while (pNode)

                lstFree(pllTrashObjects);
            } // end if (fTrashCanSemOwned)
            else
                krnPostThread1ObjectMsg(T1M_EXCEPTIONCAUGHT,
                                        (MPARAM)strdup("Couldn't request mutex semaphore"),
                                        (MPARAM)FALSE);
        }
        CATCH(excpt1) { } END_CATCH();

        if (fTrashCanSemOwned)
        {
            _wpReleaseObjectMutexSem(pTrashCan);
            fTrashCanSemOwned = FALSE;
        }

        if (!fRelatedExistsAlready) */
        {
            // related object not found above: go!
            #ifdef DEBUG_TRASHCAN
                _Pmpf(("xwpclsCreateTrashObject: Creating trash object \"%s\" in \"%s\"",
                        _wpQueryTitle(pRelatedObject),
                        _wpQueryTitle(pTrashCan)));
            #endif

            // we must pass the related object as a setup string,
            // because otherwise the Details data (which is initialized
            // at object creation only) will not know about this (duh)
            sprintf(szSetupString, "RELATEDOBJECT=%lX",
                    _wpQueryHandle(pRelatedObject));
            pTrashObject = _wpclsNew(somSelf,   // class object
                                     _wpQueryTitle(pRelatedObject), // same title as related object
                                     szSetupString, // setup string
                                     pTrashCan,     // where to create the object
                                     TRUE);  // lock
        }
    }

    return (pTrashObject);
}

/*
 *@@ trshAddTrashObjectsForTrashDir:
 *      this routine gets called from XWPTrashCan::wpPopulate
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
 */

ULONG trshAddTrashObjectsForTrashDir(M_XWPTrashObject *pXWPTrashObjectClass, // in: _XWPTrashObject (for speed)
                                     XWPTrashCan *pTrashCan,  // in: trashcan to add objects to
                                     WPFolder *pTrashDir)     // in: trash directory to examine
{
    BOOL        fTrashDirSemOwned = FALSE;
    WPObject    *pObject;
    ULONG       ulObjectCount = 0;

    if (!pXWPTrashObjectClass)
        // error
        return (0);

    #ifdef DEBUG_TRASHCAN
        _Pmpf(("  Entering AddTrashObjectsForTrashDir for %s", _wpQueryTitle(pTrashDir)));
    #endif

    TRY_LOUD(excpt1, NULL)
    {
        wpshCheckIfPopulated(pTrashDir);

        // request semaphore for that trash dir
        // to protect the contents list
        fTrashDirSemOwned = !_wpRequestObjectMutexSem(pTrashDir, 4000);
        if (fTrashDirSemOwned)
        {
            #ifdef DEBUG_TRASHCAN
                _Pmpf(("    fTrashDirSemOwned = %d", fTrashDirSemOwned));
            #endif

            for (   pObject = _wpQueryContent(pTrashDir, NULL, (ULONG)QC_FIRST);
                    (pObject);
                    pObject = _wpQueryContent(pTrashDir, pObject, (ULONG)QC_NEXT)
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
                    CHAR    szFolderPath[CCHMAXPATH] = "";
                    ULONG   ulAttrs = 0;
                    _wpQueryFilename(pObject, szFolderPath, TRUE);
                    if (doshQueryPathAttr(szFolderPath, &ulAttrs) == NO_ERROR)
                        if (ulAttrs & FILE_HIDDEN)
                        {
                            // hidden directory: this is a trash directory,
                            // so recurse!
                            #ifdef DEBUG_TRASHCAN
                                _Pmpf(("    Recursing with %s", _wpQueryTitle(pObject)));
                            #endif

                            ulObjectCount += trshAddTrashObjectsForTrashDir(pXWPTrashObjectClass,
                                                                            pTrashCan,
                                                                            pObject); // new trash dir
                            // skip the following
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

                    // note that M_XWPTrashObject::xwpclsCreateTrashObject
                    // will automatically check for whether a trash
                    // object exists for a given related object
                    trshCreateTrashObject(pXWPTrashObjectClass,
                                          pTrashCan,
                                          pObject);  // related object
                    ulObjectCount++;
                }
            } // end for (   pObject = _wpQueryContent(...

            if (ulObjectCount == 0)
            {
                // no objects found in this trash folder
                // or subfolders (if any):
                // delete this folder, it's useless
                _wpFree(pTrashDir);
            }
        } // end if (fTrashDirSemOwned)
        else
            krnPostThread1ObjectMsg(T1M_EXCEPTIONCAUGHT,
                                    (MPARAM)strdup("Couldn't request mutex semaphore"),
                                    (MPARAM)FALSE);
    }
    CATCH(excpt1) { } END_CATCH();

    if (fTrashDirSemOwned)
    {
        _wpReleaseObjectMutexSem(pTrashDir);
        fTrashDirSemOwned = FALSE;
    }

    #ifdef DEBUG_TRASHCAN
        _Pmpf(("  End of AddTrashObjectsForTrashDir for %s;", _wpQueryTitle(pTrashDir)));
        _Pmpf(("    returning %d objects ", ulObjectCount));
    #endif

    return (ulObjectCount);
}

/*
 *@@ trshUpdateStatusBars:
 *      updates the status bars of all currently
 *      open trash can views.
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
                    _cCurrentDrivePopulating = cDrive;
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
                        // "\Trash" exists for this drive;
                        trshAddTrashObjectsForTrashDir(pXWPTrashObjectClass, // _XWPTrashObject
                                                       somSelf,     // trashcan to add objects to
                                                       pTrashDir);  // initial trash dir;
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

    _cCurrentDrivePopulating = 0;
    trshUpdateStatusBars(somSelf);
    ulFldrFlags &= ~FOI_POPULATEINPROGRESS;

    _wpSetFldrFlags(somSelf, ulFldrFlags);

    return (brc);
}

/*
 *@@ trshRefresh:
 *      implementation for XWPTrashCan::wpRefresh.
 *
 *@@added V0.9.1 (2000-01-31) [umoeller]
 */

BOOL trshRefresh(XWPTrashCan *somSelf)
{
    PLINKLIST   pllTrashObjects = trshCreateTrashObjectsList(somSelf);
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
 *@@added V0.9.1 (2000-02-03) [umoeller]
 */

BOOL trshDeleteIntoTrashCan(XWPTrashCan *pTrashCan,
                            WPObject *pObject)
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

                    if (_wpMoveObject(pObject, pFolderInTrash))
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
                    }
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
                    // check if object exists in that folder already
                    // (this might call the XFldObject replacement)
                    WPObject    *pReplaceThis = NULL;
                    CHAR        szNewTitle[CCHMAXPATH] = "";
                    BOOL        fMove = TRUE;
                    ULONG       ulAction;

                    strcpy(szNewTitle, _wpQueryTitle(_pRelatedObject));
                    ulAction = _wpConfirmObjectTitle(_pRelatedObject,      // object
                                                     pTargetFolder2,       // folder
                                                     &pReplaceThis,        // object to replace (if NAMECLASH_REPLACE)
                                                     szNewTitle,           // in/out: object title
                                                     sizeof(szNewTitle),
                                                     0x006B);              // move code

                    // _Pmpf(("    _wpConfirmObjectTitle returned %d", ulAction));

                    switch (ulAction)
                    {
                        case NAMECLASH_CANCEL:
                            fMove = FALSE;
                        break;

                        case NAMECLASH_RENAME:
                            _wpSetTitle(_pRelatedObject,
                                        szNewTitle);      // set by wpConfirmObjectTitle
                        break;

                        case NAMECLASH_REPLACE:
                            _wpReplaceObject(_pRelatedObject,
                                             pReplaceThis,       // set by wpConfirmObjectTitle
                                             TRUE);              // move and replace
                            fMove = FALSE;
                        break;

                        // NAMECLASH_NONE: just go on
                    }

                    if (fMove)
                        // move related object
                        if (_wpMoveObject(_pRelatedObject, pTargetFolder2))
                        {
                            // successful: destroy the trash object
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
                    if (!fopsValidateObjOperation(XFT_MOVE2TRASHCAN,
                                                  pObjDragged))
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
 *      the trash can, using fopsStartFromListCommon.
 *
 *      Returns either RC_DROP_ERROR or RC_DROP_DROPCOMPLETE.
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
                BOOL    fReportSuccess = FALSE;
                if (fStartTask)
                {
                    // no errors so far:
                    // add item to list
                    fReportSuccess = fopsValidateObjOperation(XFT_MOVE2TRASHCAN,
                                                              pObjectDropped);

                    if (fReportSuccess)
                    {
                        lstAppendItem(pllDroppedObjects,
                                      pObjectDropped);
                        fItemsAdded = TRUE;
                    }
                    else
                        fStartTask = FALSE;
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
        fopsStartFromListCommon(XFT_MOVE2TRASHCAN,
                                "Moving to Trash", // ###
                                somSelf,            // pSourceFolder
                                NULL,         // target folder: not needed
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
 *@@added V0.9.1 (2000-01-31) [umoeller]
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
            pllTrashObjects = trshCreateTrashObjectsList(somSelf);

            brc = fopsStartFromListCommon(XFT_DESTROYTRASHOBJECTS,
                                          "Emptying Trash Can",
                                          somSelf,        // source folder
                                          NULL,           // target folder: not needed
                                          pllTrashObjects); // list with objects

            lstFree(pllTrashObjects);
        }
    }
    CATCH(excpt1) { } END_CATCH();

    return (brc);
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

    if (cmnLock(5000))
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

        cmnUnlock();
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
                            fopsStartCnrRestoreFromTrash(pstf->somSelf,  // source: trash can
                                                         NULL,           // target folder
                                                         pstf->pSourceObject, // first source object
                                                         pstf->ulSelection,
                                                         pstf->hwndCnr);
                        break;

                        case ID_XFMI_OFS_TRASHDESTROY:
                            fopsStartCnrDestroyTrashObjects(pstf->somSelf, // source: trash can
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
            DosBeep(10000, 10);
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
                              pGlobalSettings->fTrashConfirmEmpty);
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
            pGlobalSettings->fTrashConfirmEmpty = ulExtra;
        break;

        case DID_UNDO:
        {
            // "Undo" button: get pointer to backed-up Global Settings
            GLOBALSETTINGS *pGSBackup = (GLOBALSETTINGS*)(pcnbp->pUser);

            // and restore the settings for this page
            pGlobalSettings->fTrashDelete = pGSBackup->fTrashDelete;
            pGlobalSettings->fTrashEmptyStartup = pGSBackup->fTrashEmptyStartup;
            pGlobalSettings->fTrashEmptyShutdown = pGSBackup->fTrashEmptyShutdown;
            pGlobalSettings->fTrashConfirmEmpty = pGSBackup->fTrashConfirmEmpty;

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


