
/*
 *@@sourcefile fdrsplit.c:
 *      folder "split view" implementation.
 *
 *
 *@@added V0.9.21 (2002-08-21) [umoeller]
 *@@header "filesys\folder.h"
 */

/*
 *      Copyright (C) 2001-2002 Ulrich M”ller.
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
#define INCL_WINFRAMEMGR
#define INCL_WINPOINTERS
#define INCL_WININPUT
#define INCL_WINSTDCNR
#define INCL_WINSHELLDATA
#define INCL_WINSYS
#include <os2.h>

// C library headers
#include <stdio.h>              // needed for except.h
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\comctl.h"             // common controls (window procs)
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\cnrh.h"               // container helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\standards.h"          // some standard macros
#include "helpers\threads.h"            // thread helpers
#include "helpers\winh.h"               // PM helper routines

// SOM headers which don't crash with prec. header files
#include "xfldr.ih"
#include "xfdisk.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\cnrsort.h"             // container sort comparison functions

#include "filesys\folder.h"             // XFolder implementation
#include "filesys\object.h"             // XFldObject implementation

// other SOM headers
#pragma hdrstop                         // VAC++ keeps crashing otherwise

/* ******************************************************************
 *
 *   Private declarations
 *
 ********************************************************************/

PCSZ    WC_SPLITVIEWCLIENT  = "XWPSplitViewClient",
        WC_SPLITPOPULATE   = "XWPSplitViewPopulate";

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

PFNWP   G_pfnFrameProc = NULL;

static MRESULT EXPENTRY fnwpSubclassedFilesFrame(HWND hwndFrame, ULONG msg, MPARAM mp1, MPARAM mp2);

/* ******************************************************************
 *
 *   Populate management
 *
 ********************************************************************/

/*
 *@@ fdrGetFSFromRecord:
 *      returns the WPFileSystem* which is represented
 *      by the specified record.
 *      This resolves shadows and returns root folders
 *      for WPDisk objects cleanly.
 *
 *      Returns NULL
 *
 *      -- if (fFoldersOnly) and precc does not represent
 *         a folder;
 *
 *      -- if (!fFoldersOnly) and precc does not represent
 *         a file-system object;
 *
 *      -- if the WPDisk or WPShadow cannot be resolved.
 *
 *@@added V0.9.9 (2001-03-11) [umoeller]
 */

WPFileSystem* fdrGetFSFromRecord(PMINIRECORDCORE precc,
                                 BOOL fFoldersOnly)
{
    WPObject *pobj = NULL;
    if (    (precc)
         && (pobj = OBJECT_FROM_PREC(precc))
         && (pobj = objResolveIfShadow(pobj))
       )
    {
        if (_somIsA(pobj, _WPDisk))
            pobj = _xwpSafeQueryRootFolder(pobj,
                                           FALSE,   // no change map
                                           NULL);

        if (pobj)
        {
            if (fFoldersOnly)
            {
                if (!_somIsA(pobj, _WPFolder))
                    pobj = NULL;
            }
            else
                if (!_somIsA(pobj, _WPFileSystem))
                    pobj = NULL;
        }
    }

    return (pobj);
}

/*
 *@@ IsInsertable:
 *      checks if pObject can be inserted in a container.
 *
 *      The return value depends on ulInsert:
 *
 *      --  INSERT_ALL: we allow all objects to be inserted,
 *          even broken shadows.
 *
 *          pcszFileMask is ignored in this case.
 *
 *      --  INSERT_FILESYSTEMS: this inserts all WPFileSystem and
 *          WPDisk objects plus shadows pointing to them. This
 *          is for the file dialog, obviously, because opening
 *          abstracts with a file dialog is impossible (unless
 *          an abstract is a shadow pointing to a file-system
 *          object).
 *
 *          For file-system objects, if (pcszFileMask != NULL), the
 *          object's real name is checked against that file mask also.
 *          For example, if (pcszFileMask == *.TXT), this will
 *          return TRUE only if pObject's real name matches
 *          *.TXT.
 *
 *          This is for the right (files) view.
 *
 *      --  INSERT_FOLDERSONLY: only folders are inserted.
 *          We will not even insert disk objects or shadows,
 *          even if they point to shadows.
 *
 *          pcszFileMask is ignored in this case.
 *
 *          This is for the left (drives) view when items
 *          are expanded.
 *
 *          This will NOT resolve shadows because if we insert
 *          shadows of folders into a container, their contents
 *          cannot be inserted a second time. The WPS shares
 *          records so each object can only be inserted once.
 *
 *      --  INSERT_FOLDERSANDDISKS: in addition to folders
 *          (as with INSERT_FOLDERSONLY), we allow insertion
 *          of drive objects too.
 *
 *      In any case, FALSE is returned if the object matches
 *      the above, but the object has the "hidden" attribute on.
 */

BOOL fdrIsInsertable(WPObject *pObject,
                     BOOL ulFoldersOnly,
                     PCSZ pcszFileMask)     // in: upper-case file mask or NULL
{
    if (!pObject)
        return FALSE;       // in any case

    if (ulFoldersOnly > 1)      // INSERT_FOLDERSONLY or INSERT_FOLDERSANDDISKS
    {
        // folders only:

        if (_wpQueryStyle(pObject) & OBJSTYLE_TEMPLATE)
            return FALSE;

        // allow disks with INSERT_FOLDERSANDDISKS only
        if (    (ulFoldersOnly == INSERT_FOLDERSANDDISKS)
             && (_somIsA(pObject, _WPDisk))
           )
        {
            // always insert, even if drive not ready
            return TRUE;
        }

        if (_somIsA(pObject, _WPFolder))
        {
            // filter out folder templates and hidden folders
            if (    (!(_wpQueryStyle(pObject) & OBJSTYLE_TEMPLATE))
                 && (!(_wpQueryAttr(pObject) & FILE_HIDDEN))
               )
                return TRUE;
        }

        return FALSE;
    }

    // INSERT_ALL or INSERT_FILESYSTEMS:

    // resolve shadows
    if (!(pObject = objResolveIfShadow(pObject)))
        // broken shadow:
        // disallow with INSERT_FILESYSTEMS
        return (ulFoldersOnly == INSERT_ALL);

    if (ulFoldersOnly == INSERT_ALL)
        return TRUE;

    // INSERT_FILESYSTEMS:

    if (_somIsA(pObject, _WPDisk))
        return TRUE;

    if (    // filter out non-file systems (shadows pointing to them have been resolved):
            (_somIsA(pObject, _WPFileSystem))
            // filter out hidden objects:
         && (!(_wpQueryAttr(pObject) & FILE_HIDDEN))
       )
    {
        // OK, non-hidden file-system object:
        // regardless of filters, always insert folders
        if (_somIsA(pObject, _WPFolder))
            return TRUE;          // templates too

        if ((pcszFileMask) && (*pcszFileMask))
        {
            // file mask specified:
            CHAR szRealName[CCHMAXPATH];
            if (_wpQueryFilename(pObject,
                                 szRealName,
                                 FALSE))       // not q'fied
            {
                return doshMatch(pcszFileMask, szRealName);
            }
        }
        else
            // no file mask:
            return TRUE;
    }

    return FALSE;
}

/*
 *@@ IsObjectInCnr:
 *      returns TRUE if pObject has already been
 *      inserted into hwndCnr.
 *
 */

BOOL fdrIsObjectInCnr(WPObject *pObject,
                      HWND hwndCnr)
{
    BOOL    brc = FALSE;
    BOOL    fLocked = FALSE;

    TRY_LOUD(excpt1)
    {
        if (fLocked = !_wpRequestObjectMutexSem(pObject, SEM_INDEFINITE_WAIT))
        {
            PUSEITEM    pUseItem = NULL;
            for (pUseItem = _wpFindUseItem(pObject, USAGE_RECORD, NULL);
                 pUseItem;
                 pUseItem = _wpFindUseItem(pObject, USAGE_RECORD, pUseItem))
            {
                // USAGE_RECORD specifies where this object is
                // currently inserted
                PRECORDITEM pRecordItem = (PRECORDITEM)(pUseItem + 1);

                if (hwndCnr == pRecordItem->hwndCnr)
                {
                    brc = TRUE;
                    break;
                }
            }
        }
    }
    CATCH(excpt1)
    {
    } END_CATCH();

    if (fLocked)
        _wpReleaseObjectMutexSem(pObject);

    return brc;
}

/*
 *@@ fdrInsertContents:
 *      inserts the contents of the given folder into
 *      the given container.
 *
 *      It is assumed that the folder is already populated.
 *
 *      If (precParent != NULL), the contents are inserted
 *      as child records below that record. Of course that
 *      will work in Tree view only.
 *
 *      In addition, if (hwndAddFirstChild != NULLHANDLE),
 *      this will fire an CM_ADDFIRSTCHILD msg to that
 *      window for every record that was inserted.
 *
 */

VOID fdrInsertContents(WPFolder *pFolder,              // in: populated folder
                       HWND hwndCnr,                   // in: cnr to insert records to
                       PMINIRECORDCORE precParent,     // in: parent record or NULL
                       ULONG ulFoldersOnly,            // in: as with fdrIsInsertable
                       HWND hwndAddFirstChild,         // in: if != 0, we post CM_ADDFIRSTCHILD for each item too
                       PCSZ pcszFileMask,              // in: file mask filter or NULL
                       PLINKLIST pllObjects)           // in/out: linked list of objs that were inserted
{
    BOOL        fFolderLocked = FALSE;

    TRY_LOUD(excpt1)
    {
        if (fFolderLocked = !_wpRequestFolderMutexSem(pFolder, SEM_INDEFINITE_WAIT))
        {
            // count objects that should be inserted
            WPObject    *pObject;
            ULONG       cObjects = 0;
            for (   pObject = _wpQueryContent(pFolder, NULL, QC_FIRST);
                    pObject;
                    pObject = *__get_pobjNext(pObject)
                )
            {
                if (fdrIsInsertable(pObject,
                                    ulFoldersOnly,
                                    pcszFileMask))
                    cObjects++;
            }

            _PmpfF(("--> run 1: got %d objects", cObjects));

            // 2) build array
            if (cObjects)
            {
                // allocate array of objects to be inserted
                WPObject    **papObjects;
                ULONG       cAddFirstChilds = 0;

                if (papObjects = (WPObject**)malloc(sizeof(WPObject*) * cObjects))
                {
                    ULONG ul;
                    WPObject **ppThis = papObjects;
                    // reset object count, this might change
                    cObjects = 0;

                    for (   pObject = _wpQueryContent(pFolder, NULL, QC_FIRST);
                            pObject;
                            pObject = *__get_pobjNext(pObject)
                        )
                    {
                        if (fdrIsInsertable(pObject,
                                            ulFoldersOnly,
                                            pcszFileMask))
                        {
                            if (!fdrIsObjectInCnr(pObject, hwndCnr))
                            {
                                *ppThis = pObject;
                                ppThis++;
                                cObjects++;
                            }

                            if (    (hwndAddFirstChild)
                                 && (_somIsA(pObject, _WPFolder))
                               )
                            {
                                if (!cAddFirstChilds)
                                {
                                    // first post: tell thread to update
                                    // the wait pointer
                                    WinPostMsg(hwndAddFirstChild,
                                               FM2_ADDFIRSTCHILD_BEGIN,
                                               0, 0);
                                    cAddFirstChilds++;
                                }

                                WinPostMsg(hwndAddFirstChild,
                                           FM2_ADDFIRSTCHILD_NEXT,
                                           (MPARAM)pObject,
                                           NULL);
                            }
                        }
                    }

                    _PmpfF(("--> run 2: got %d objects", cObjects));

                    _wpclsInsertMultipleObjects(_somGetClass(pFolder),
                                                hwndCnr,
                                                NULL,
                                                (PVOID*)papObjects,
                                                precParent,
                                                cObjects);

                    for (ul = 0;
                         ul < cObjects;
                         ul++)
                    {
                        // BOOL fAppend = FALSE;
                        WPObject *pobjThis = papObjects[ul];

                        // lock the object!! we must make sure
                        // the WPS won't let it go dormant
                        // _wpLockObject(pobjThis);
                                // no, populate has locked it already
                                // V0.9.18 (2002-02-06) [umoeller]
                                // unlock is in "clear container"
                        if (pllObjects)
                            lstAppendItem(pllObjects, pobjThis);
                    }

                    free(papObjects);
                }

                if (cAddFirstChilds)
                {
                    // we had any "add-first-child" posts:
                    // post another msg which will get processed
                    // after all the "add-first-child" things
                    // so that the wait ptr can be reset
                    WinPostMsg(hwndAddFirstChild,
                               FM2_ADDFIRSTCHILD_DONE,
                               0, 0);
                }
            }
        }
    }
    CATCH(excpt1)
    {
    } END_CATCH();

    if (fFolderLocked)
        _wpReleaseFolderMutexSem(pFolder);
}

/*
 *@@ ClearContainer:
 *      removes all objects that were inserted into the
 *      specified container and updates the USEITEM's
 *      correctly.
 */

ULONG fdrClearContainer(HWND hwndCnr,              // in: cnr to clear
                        PLINKLIST pllObjects)      // in: list of objects to remove
{
    ULONG       ulrc = 0;
    PLISTNODE   pNode;
    // OK, nuke the container contents...
    // we could simply call wpCnrRemoveObject
    // for each object in the container, but
    // this causes a lot of overhead.
    // From my testing, wpCnrRemoveObject
    // does the following:
    // 1) search the object's useitems and
    //    remove the matching RECORDITEM;
    // 2) only after that, it attempts to
    //    remove the record from the container.
    //    If that fails, the useitem has been
    //    removed anyway.

    // So to speed things up, we just kill
    // the entire container contents in one
    // flush and then call wpCnrRemoveObject
    // on each object that was inserted...
    WinSendMsg(hwndCnr,
               CM_REMOVERECORD,
               NULL,                // all records
               MPFROM2SHORT(0,      // all records
                            CMA_INVALIDATE));
                                // no free, WPS shares records

    // WinEnableWindowUpdate(hwndCnr, FALSE);

    pNode = lstQueryFirstNode(pllObjects);
    while (pNode)
    {
        WPObject *pobj = (WPObject*)pNode->pItemData;
        _wpCnrRemoveObject(pobj,
                           hwndCnr);
        _wpUnlockObject(pobj);       // was locked by "insert"
        pNode = pNode->pNext;
        ulrc++;
    }

    lstClear(pllObjects);

    //  WinEnableWindowUpdate(hwndCnr, TRUE);

    return (ulrc);
}

/* ******************************************************************
 *
 *   Split Populate thread
 *
 ********************************************************************/

/*
 *@@ AddFirstChild:
 *      adds the first child record for precParent
 *      to the given container if precParent represents
 *      a folder that has subfolders.
 *
 *      This gets called for every record in the drives
 *      tree so we properly add the "+" expansion signs
 *      to each record without having to fully populate
 *      each folder. This is an imitation of the standard
 *      WPS behavior in Tree views.
 *
 *@@added V0.9.18 (2002-02-06) [umoeller]
 */

static WPObject* AddFirstChild(WPFolder *pFolder,
                               PMINIRECORDCORE precParent,     // in: folder record to insert first child for
                               HWND hwndCnr,                   // in: cnr where precParent is inserted
                               PLINKLIST pll)                  // in/out: list of objs
{
    PMINIRECORDCORE     precFirstChild;
    WPFolder            *pFirstChildFolder = NULL;

    if (!(precFirstChild = (PMINIRECORDCORE)WinSendMsg(hwndCnr,
                                                       CM_QUERYRECORD,
                                                       (MPARAM)precParent,
                                                       MPFROM2SHORT(CMA_FIRSTCHILD,
                                                                    CMA_ITEMORDER))))
    {
        // we don't have a first child already:

        // check if we have a subfolder in the folder already
        BOOL    fFolderLocked = FALSE,
                fFindLocked = FALSE;

        _Pmpf(("  "__FUNCTION__": CM_QUERYRECORD returned NULL"));

        TRY_LOUD(excpt1)
        {
            // request the find sem to make sure we won't have a populate
            // on the other thread; otherwise we get duplicate objects here
            if (fFindLocked = !_wpRequestFindMutexSem(pFolder, SEM_INDEFINITE_WAIT))
            {
                WPObject    *pObject;

                if (fFolderLocked = !_wpRequestFolderMutexSem(pFolder, SEM_INDEFINITE_WAIT))
                {
                    for (   pObject = _wpQueryContent(pFolder, NULL, QC_FIRST);
                            pObject;
                            pObject = *__get_pobjNext(pObject))
                    {
                        if (fdrIsInsertable(pObject,
                                            INSERT_FOLDERSONLY,
                                            NULL))
                        {
                            pFirstChildFolder = pObject;
                            break;
                        }
                    }

                    _wpReleaseFolderMutexSem(pFolder);
                    fFolderLocked = FALSE;
                }

                _Pmpf(("  "__FUNCTION__": pFirstChildFolder pop is 0x%lX", pFirstChildFolder));

                if (!pFirstChildFolder)
                {
                    // no folder awake in folder yet:
                    // do a quick DosFindFirst loop to find the
                    // first subfolder in here
                    HDIR          hdir = HDIR_CREATE;
                    FILEFINDBUF3  ffb3     = {0};
                    ULONG         ulFindCount    = 1;        // look for 1 file at a time
                    APIRET        arc            = NO_ERROR;

                    CHAR          szFolder[CCHMAXPATH],
                                  szSearchMask[CCHMAXPATH];

                    _wpQueryFilename(pFolder, szFolder, TRUE);
                    sprintf(szSearchMask, "%s\\*", szFolder);

                    _Pmpf(("  "__FUNCTION__": searching %s", szSearchMask));

                    ulFindCount = 1;
                    arc = DosFindFirst(szSearchMask,
                                       &hdir,
                                       MUST_HAVE_DIRECTORY | FILE_ARCHIVED | FILE_SYSTEM | FILE_READONLY,
                                             // but exclude hidden
                                       &ffb3,
                                       sizeof(ffb3),
                                       &ulFindCount,
                                       FIL_STANDARD);

                    while ((arc == NO_ERROR))
                    {
                        _Pmpf(("      "__FUNCTION__": got %s", ffb3.achName));

                        // do not use "." and ".."
                        if (    (strcmp(ffb3.achName, ".") != 0)
                             && (strcmp(ffb3.achName, "..") != 0)
                           )
                        {
                            // this is good:
                            CHAR szFolder2[CCHMAXPATH];
                            sprintf(szFolder2, "%s\\%s", szFolder, ffb3.achName);
                            _Pmpf(("      "__FUNCTION__": awaking %s", szFolder2));
                            pObject = _wpclsQueryFolder(_WPFolder,
                                                        szFolder2,
                                                        TRUE);
                            // exclude templates
                            if (fdrIsInsertable(pObject,
                                                INSERT_FOLDERSONLY,
                                                NULL))
                            {
                                pFirstChildFolder = pObject;
                                break;
                            }
                        }

                        // search next file
                        ulFindCount = 1;
                        arc = DosFindNext(hdir,
                                         &ffb3,
                                         sizeof(ffb3),
                                         &ulFindCount);

                    } // end while (rc == NO_ERROR)

                    DosFindClose(hdir);
                }
            }
        }
        CATCH(excpt1)
        {
        } END_CATCH();

        if (fFolderLocked)
            _wpReleaseFolderMutexSem(pFolder);
        if (fFindLocked)
            _wpReleaseFindMutexSem(pFolder);

        if (pFirstChildFolder)
        {
            POINTL ptl = {0, 0};
            if (_wpCnrInsertObject(pFirstChildFolder,
                                   hwndCnr,
                                   &ptl,        // without this the func fails
                                   precParent,
                                   NULL))
                lstAppendItem(pll,
                              pFirstChildFolder);
        }
    }

    return pFirstChildFolder;
}

/*
 *@@ fnwpSplitPopulate:
 *      object window for populate thread.
 */

static MRESULT EXPENTRY fnwpSplitPopulate(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    switch (msg)
    {
        case WM_CREATE:
            WinSetWindowPtr(hwnd, QWL_USER, mp1);       // PFDRSPLITVIEW
        break;

        /*
         *@@ FM2_POPULATE:
         *      posted by fnwpMainControl when
         *      FM_FILLFOLDER comes in to offload
         *      populate to this second thread.
         *
         *      After populate is done, we post the
         *      following back to fnwpMainControl:
         *
         *      --  FM_POPULATED_FILLTREE always so
         *          that the drives tree can get
         *          updated;
         *
         *      --  FM_POPULATED_SCROLLTO, if the
         *          FFL_SCROLLTO flag was set;
         *
         *      --  FM_POPULATED_FILLFILES, if the
         *          FFL_FOLDERSONLY flag was _not_
         *          set.
         *
         *      This processing is all new with V0.9.18
         *      to finally synchronize the populate with
         *      the main thread better.
         *
         *      Parameters:
         *
         *      --  PMINIRECORDCORE mp1
         *
         *      --  ULONG mp2: flags, as with FM_FILLFOLDER.
         *
         *@@added V0.9.18 (2002-02-06) [umoeller]
         */

        case FM2_POPULATE:
        {
            PFDRSPLITVIEW   psv = WinQueryWindowPtr(hwnd, QWL_USER);
            WPFolder        *pFolder;
            PMINIRECORDCORE prec = (PMINIRECORDCORE)mp1;
            ULONG           fl = (ULONG)mp2;

            if (pFolder = fdrGetFSFromRecord(prec, TRUE))
            {
                BOOL fFoldersOnly = ((fl & FFL_FOLDERSONLY) != 0);

                // set wait pointer
                (psv->cThreadsRunning)++;
                WinPostMsg(psv->hwndMainControl,
                           FM_UPDATEPOINTER,
                           0,
                           0);

                _PmpfF(("populating %s", _wpQueryTitle(pFolder)));

                if (fdrCheckIfPopulated(pFolder,
                                        fFoldersOnly))
                {
                    // in any case, refresh the tree
                    WinPostMsg(psv->hwndMainControl,
                               FM_POPULATED_FILLTREE,
                               (MPARAM)prec,
                               (MPARAM)fl);
                            // fnwpMainControl will check fl again and
                            // fire "add first child" msgs accordingly

                    if (fl & FFL_SCROLLTO)
                        WinPostMsg(psv->hwndMainControl,
                                   FM_POPULATED_SCROLLTO,
                                   (MPARAM)prec,
                                   0);

                    if (!fFoldersOnly)
                        // refresh the files only if we are not
                        // in folders-only mode
                        WinPostMsg(psv->hwndMainControl,
                                   FM_POPULATED_FILLFILES,
                                   (MPARAM)prec,
                                   (MPARAM)pFolder);
                }

                // clear wait pointer
                (psv->cThreadsRunning)--;
                WinPostMsg(psv->hwndMainControl,
                           FM_UPDATEPOINTER,
                           0,
                           0);
            }
        }
        break;

        /*
         *@@ FM2_ADDFIRSTCHILD_BEGIN:
         *      posted by InsertContents before the first
         *      FM2_ADDFIRSTCHILD_NEXT is posted so we
         *      can update the "wait" ptr accordingly.
         *
         *@@added V0.9.18 (2002-02-06) [umoeller]
         */

        case FM2_ADDFIRSTCHILD_BEGIN:
        {
            PFDRSPLITVIEW   psv = WinQueryWindowPtr(hwnd, QWL_USER);
            (psv->cThreadsRunning)++;
            WinPostMsg(psv->hwndMainControl,
                       FM_UPDATEPOINTER,
                       0, 0);
        }
        break;

        /*
         *@@ FM2_ADDFIRSTCHILD_NEXT:
         *      fired by InsertContents for every folder that
         *      is added to the drives tree.
         *
         *      Parameters:
         *
         *      --  WPFolder* mp1: folder to add first child for.
         *          This better be in the tree.
         *
         *@@added V0.9.18 (2002-02-06) [umoeller]
         */

        case FM2_ADDFIRSTCHILD_NEXT:
            if (mp1)
            {
                PFDRSPLITVIEW       psv = WinQueryWindowPtr(hwnd, QWL_USER);
                HWND                hwndCnr = psv->hwndTreeCnr;
                WPFolder            *pFolder = (WPObject*)mp1;
                PMINIRECORDCORE     precParent = _wpQueryCoreRecord(pFolder);

                _PmpfF(("CM_ADDFIRSTCHILD %s", _wpQueryTitle(mp1)));

                AddFirstChild(pFolder,
                              precParent,
                              hwndCnr,
                              &psv->llTreeObjectsInserted);
            }
        break;

        /*
         *@@ FM2_ADDFIRSTCHILD_DONE:
         *      posted by InsertContents after the last
         *      FM2_ADDFIRSTCHILD_NEXT was posted so we
         *      can reset the "wait" ptr.
         *
         *@@added V0.9.18 (2002-02-06) [umoeller]
         */

        case FM2_ADDFIRSTCHILD_DONE:
        {
            PFDRSPLITVIEW   psv = WinQueryWindowPtr(hwnd, QWL_USER);
            (psv->cThreadsRunning)--;
            WinPostMsg(psv->hwndMainControl,
                       FM_UPDATEPOINTER,
                       0,
                       0);
        }
        break;

        default:
            mrc = WinDefWindowProc(hwnd, msg, mp1, mp2);
    }

    return mrc;
}

/*
 *@@ fntSplitPopulate:
 *      "split populate" thread. This creates an object window
 *      so that we can easily serialize the order in which
 *      folders are populate and such.
 *
 *      This is responsible for both populating folders _and_
 *      doing the "add first child" processing. This was all
 *      new with V0.9.18's file dialog and was my second attempt
 *      at getting the thread synchronization right, which
 *      turned out to work pretty well.
 *
 *      We _need_ a second thread for "add first child" too
 *      because even adding the first child can take quite a
 *      while. For example, if a folder has 1,000 files in it
 *      and the 999th is a directory, the file system has to
 *      scan the entire contents first.
 */

static VOID _Optlink fntSplitPopulate(PTHREADINFO ptiMyself)
{
    TRY_LOUD(excpt1)
    {
        QMSG qmsg;
        PFDRSPLITVIEW psv = (PFDRSPLITVIEW)ptiMyself->ulData;

        _PmpfF(("thread starting"));

        WinRegisterClass(ptiMyself->hab,
                         (PSZ)WC_SPLITPOPULATE,
                         fnwpSplitPopulate,
                         0,
                         sizeof(PVOID));
        if (!(psv->hwndSplitPopulate = winhCreateObjectWindow(WC_SPLITPOPULATE,
                                                              psv)))
            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                   "Cannot create split populate object window.");

        // thread 1 is waiting for obj window to be created
        DosPostEventSem(ptiMyself->hevRunning);

        while (WinGetMsg(ptiMyself->hab, &qmsg, NULLHANDLE, 0, 0))
            WinDispatchMsg(ptiMyself->hab, &qmsg);

        WinDestroyWindow(psv->hwndSplitPopulate);

        _PmpfF(("thread ending"));
    }
    CATCH(excpt1) {} END_CATCH();

}

/*
 *@@ fdrSplitPopulate:
 *      posts FM2_POPULATE to fnwpSplitPopulate to
 *      populate the folder represented by the
 *      given MINIRECORDCORE according to fl.
 */

VOID fdrSplitPopulate(PFDRSPLITVIEW psv,
                      PMINIRECORDCORE prec,
                      ULONG fl)
{
    _PmpfF(("psv->hwndSplitPopulate: 0x%lX", psv->hwndSplitPopulate));

    WinPostMsg(psv->hwndSplitPopulate,
               FM2_POPULATE,
               (MPARAM)prec,
               (MPARAM)fl);
}

#ifdef __DEBUG__
VOID DumpFlags(ULONG fl)
{
    _Pmpf(("  fl %s %s %s",
                (fl & FFL_FOLDERSONLY) ? "FFL_FOLDERSONLY " : "",
                (fl & FFL_SCROLLTO) ? "FFL_SCROLLTO " : "",
                (fl & FFL_EXPAND) ? "FFL_EXPAND " : ""));
}
#else
#define DumpFlags(fl)
#endif

/*
 *@@ PostFillFolder:
 *      posts FM_FILLFOLDER to the main control
 *      window with the given parameters.
 *
 *@@added V0.9.18 (2002-02-06) [umoeller]
 */

VOID fdrPostFillFolder(PFDRSPLITVIEW psv,
                       PMINIRECORDCORE prec,       // in: record with folder to populate
                       ULONG fl)                   // in: FFL_* flags
{
    _PmpfF(("        posting FM_FILLFOLDER %s",
                prec->pszIcon
            ));
    DumpFlags(fl);

    WinPostMsg(psv->hwndMainControl,
               FM_FILLFOLDER,
               (MPARAM)prec,
               (MPARAM)fl);
}

/*
 *@@ fdrSplitQueryPointer:
 *      returns the HPOINTER that should be used
 *      according to the present thread state.
 *
 *      Returns a HPOINTER for either the wait or
 *      arrow pointer.
 *
 *@@changed V0.9.16 (2001-10-19) [umoeller]: fixed sticky wait pointer
 */

HPOINTER fdrSplitQueryPointer(PFDRSPLITVIEW psv)
{
    ULONG           idPtr = SPTR_ARROW;

    if (    (psv)
         && (psv->cThreadsRunning)
       )
        idPtr = SPTR_WAIT;

    return WinQuerySysPointer(HWND_DESKTOP,
                              idPtr,
                              FALSE);
}

/* ******************************************************************
 *
 *   Split view main control (main frame's client)
 *
 ********************************************************************/

/*
 *@@ fdrCreateFrameWithCnr:
 *      creates a new WC_FRAME with a WC_CONTAINER as its
 *      client window, with hwndParentOwner being the parent
 *      and owner of the frame.
 *
 *      With flCnrStyle, specify the cnr style to use. The
 *      following may optionally be set:
 *
 *      --  CCS_MINIICONS (optionally)
 *
 *      --  CCS_EXTENDSEL: allow zero, one, many icons to be
 *          selected (default WPS style).
 *
 *      --  CCS_SINGLESEL: allow only exactly one icon to be
 *          selected at a time.
 *
 *      --  CCS_MULTIPLESEL: allow zero, one, or more icons
 *          to be selected, and toggle selections (totally
 *          unusable).
 *
 *      WS_VISIBLE, WS_SYNCPAINT, CCS_AUTOPOSITION, and
 *      CCS_MINIRECORDCORE will always be set.
 *
 *      Returns the frame.
 *
 */

HWND fdrCreateFrameWithCnr(ULONG ulFrameID,
                           HWND hwndParentOwner,     // in: main client window
                           ULONG flCnrStyle,         // in: cnr style
                           HWND *phwndClient)        // out: client window (cnr)
{
    HWND    hwndFrame;
    ULONG   ws =   WS_VISIBLE
                 | WS_SYNCPAINT
                 | CCS_AUTOPOSITION
                 | CCS_MINIRECORDCORE
                 | flCnrStyle;

    if (hwndFrame = winhCreateStdWindow(hwndParentOwner, // parent
                                        NULL,          // pswpFrame
                                        FCF_NOBYTEALIGN,
                                        WS_VISIBLE,
                                        "",
                                        0,             // resources ID
                                        WC_CONTAINER,  // client
                                        ws,            // client style
                                        ulFrameID,
                                        NULL,
                                        phwndClient))
    {
        // set client as owner
        WinSetOwner(hwndFrame, hwndParentOwner);
    }

    return hwndFrame;
}

/*
 *@@ fdrSetupSplitView:
 *      creates all the controls for the split view.
 *
 *      Returns NULL if no error occured. As a result,
 *      the return value can be returned from WM_CREATE,
 *      which stops window creation if != 0 is returned.
 */

MPARAM fdrSetupSplitView(HWND hwnd,
                         BOOL fMultipleSel,     // in: allow multiple selections in files cnr?
                         PFDRSPLITVIEW psv)
{
    MPARAM mrc = (MPARAM)FALSE;         // return value of WM_CREATE: 0 == OK

    SPLITBARCDATA sbcd;
    HAB hab = WinQueryAnchorBlock(hwnd);

    lstInit(&psv->llTreeObjectsInserted, FALSE);
    lstInit(&psv->llFileObjectsInserted, FALSE);

    // set the window font for the main client...
    // all controls will inherit this
    winhSetWindowFont(hwnd,
                      cmnQueryDefaultFont());

    /*
     *  split window with two containers
     *
     */

    // create two subframes to be linked in split window

    // 1) left: drives tree
    psv->hwndTreeFrame = fdrCreateFrameWithCnr(ID_TREEFRAME,
                                               hwnd,    // main client
                                               CCS_MINIICONS | CCS_SINGLESEL,
                                               &psv->hwndTreeCnr);
    BEGIN_CNRINFO()
    {
        cnrhSetView(   CV_TREE | CA_TREELINE | CV_ICON
                     | CV_MINI);
        cnrhSetTreeIndent(20);
        cnrhSetSortFunc(fnCompareName);             // shared/cnrsort.c
    } END_CNRINFO(psv->hwndTreeCnr);

    // 2) right: files
    psv->hwndFilesFrame = fdrCreateFrameWithCnr(ID_FILESFRAME,
                                                hwnd,    // main client
                                                (fMultipleSel)
                                                   ? CCS_MINIICONS | CCS_EXTENDSEL
                                                   : CCS_MINIICONS | CCS_SINGLESEL,
                                                &psv->hwndFilesCnr);
    BEGIN_CNRINFO()
    {
        cnrhSetView(   CV_NAME | CV_FLOW
                     | CV_MINI);
        cnrhSetTreeIndent(30);
        cnrhSetSortFunc(fnCompareNameFoldersFirst);     // shared/cnrsort.c
    } END_CNRINFO(psv->hwndFilesCnr);

    // 3) fonts
    winhSetWindowFont(psv->hwndTreeCnr,
                      cmnQueryDefaultFont());
    winhSetWindowFont(psv->hwndFilesCnr,
                      cmnQueryDefaultFont());

    // create split window
    sbcd.ulSplitWindowID = 1;
        // split window becomes client of main frame
    sbcd.ulCreateFlags =   SBCF_VERTICAL
                         | SBCF_PERCENTAGE
                         | SBCF_3DSUNK
                         | SBCF_MOVEABLE;
    sbcd.lPos = psv->lSplitBarPos;   // in percent
    sbcd.ulLeftOrBottomLimit = 100;
    sbcd.ulRightOrTopLimit = 100;
    sbcd.hwndParentAndOwner = hwnd;         // client

    if (!(psv->hwndSplitWindow = ctlCreateSplitWindow(hab,
                                                      &sbcd)))
    {
        cmnLog(__FILE__, __LINE__, __FUNCTION__,
                "Cannot create split window.");
        // stop window creation!
        mrc = (MPARAM)TRUE;
    }
    else
    {
        // link left and right container
        WinSendMsg(psv->hwndSplitWindow,
                   SPLM_SETLINKS,
                   (MPARAM)psv->hwndTreeFrame,       // left
                   (MPARAM)psv->hwndFilesFrame);       // right
    }

    // create the "populate" thread
    thrCreate(&psv->tiSplitPopulate,
              fntSplitPopulate,
              &psv->tidSplitPopulate,
              "SplitPopulate",
              THRF_PMMSGQUEUE | THRF_WAIT_EXPLICIT,
                        // populate posts event sem when
                        // it has created its obj wnd
              (ULONG)psv);
    // this will wait until the object window has been created

    return mrc;
}

/*
 *@@ fdrCleanupSplitView:
 *
 *      Does NOT free psv because we can't know how it was
 *      allocated.
 *
 *@@added V0.9.21 (2002-08-21) [umoeller]
 */

VOID fdrCleanupSplitView(PFDRSPLITVIEW psv)
{
    // stop threads; we crash if we exit
    // before these are stopped
    WinPostMsg(psv->hwndSplitPopulate,
               WM_QUIT,
               0,
               0);

    psv->tiSplitPopulate.fExit = TRUE;
    DosSleep(0);
    while (psv->tidSplitPopulate)
        winhSleep(50);

    // prevent dialog updates
    psv->fFileDlgReady = FALSE;
    fdrClearContainer(psv->hwndTreeCnr,
                      &psv->llTreeObjectsInserted);
    fdrClearContainer(psv->hwndFilesCnr,
                      &psv->llFileObjectsInserted);

    if (psv->pRootFolder)
        _wpUnlockObject(psv->pRootFolder);

    // clean up
    if (psv->hwndSplitWindow)
        WinDestroyWindow(psv->hwndSplitWindow);

    if (psv->hwndTreeFrame)
        WinDestroyWindow(psv->hwndTreeFrame);
    if (psv->hwndFilesFrame)
        WinDestroyWindow(psv->hwndFilesFrame);
    if (psv->hwndMainFrame)
        WinDestroyWindow(psv->hwndMainFrame);
}

/*
 *@@ fnwpSplitController:
 *
 */

MRESULT EXPENTRY fnwpSplitController(HWND hwndClient, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    switch (msg)
    {
        /*
         * WM_CREATE:
         *      we get PFDRSPLITVIEW in mp1.
         */

        case WM_CREATE:
        {
            PFDRSPLITVIEW psv = (PFDRSPLITVIEW)mp1;
            WinSetWindowPtr(hwndClient, QWL_USER, mp1);

            mrc = fdrSetupSplitView(hwndClient,
                                    TRUE,       // multiple selections
                                    psv);
        }
        break;

        /*
         * WM_WINDOWPOSCHANGED:
         *
         */

        case WM_WINDOWPOSCHANGED:
        {
            // this msg is passed two SWP structs:
            // one for the old, one for the new data
            // (from PM docs)
            PSWP pswpNew = PVOIDFROMMP(mp1);
            // PSWP pswpOld = pswpNew + 1;

            // resizing?
            if (pswpNew->fl & SWP_SIZE)
            {
                PFDRSPLITVIEW  psv;
                if (psv = WinQueryWindowPtr(hwndClient, QWL_USER))
                {
                    // adjust size of "split window",
                    // which will rearrange all the linked
                    // windows (comctl.c)
                    WinSetWindowPos(psv->hwndSplitWindow,
                                    HWND_TOP,
                                    0,
                                    0,
                                    pswpNew->cx,
                                    pswpNew->cy,
                                    SWP_SIZE);
                }
            }

            // return default NULL
        }
        break;

        /*
         * WM_MINMAXFRAME:
         *      when minimizing, we hide the "split window",
         *      because otherwise the child dialogs will
         *      display garbage
         */

        case WM_MINMAXFRAME:
        {
            PFDRSPLITVIEW  psv;
            if (psv = WinQueryWindowPtr(hwndClient, QWL_USER))
            {
                PSWP pswp = (PSWP)mp1;
                if (pswp->fl & SWP_MINIMIZE)
                    WinShowWindow(psv->hwndSplitWindow, FALSE);
                else if (pswp->fl & SWP_RESTORE)
                    WinShowWindow(psv->hwndSplitWindow, TRUE);
            }
        }
        break;

        /*
         *@@ FM_FILLFOLDER:
         *      posted to the main control to fill
         *      the dialog when a new folder has been
         *      selected in the left drives tree.
         *
         *      This automatically offloads populate
         *      to fntPopulate, which will then post
         *      a bunch of messages back to us so we
         *      can update the dialog properly.
         *
         *      Parameters:
         *
         *      --  PMINIRECORDCODE mp1: record of folder
         *          (or disk or whatever) to fill with.
         *
         *      --  ULONG mp2: dialog flags.
         *
         *      mp2 can be any combination of the following:
         *
         *      --  If FFL_FOLDERSONLY is set, this operates
         *          in "folders only" mode. We will then
         *          populate the folder with subfolders only
         *          and expand the folder on the left. The
         *          files list is not changed.
         *
         *          If the flag is not set, the folder is
         *          fully populated and the files list is
         *          updated as well.
         *
         *      --  If FFL_SCROLLTO is set, we will scroll
         *          the drives tree so that the given record
         *          becomes visible.
         *
         *      --  If FFL_EXPAND is set, we will also expand
         *          the record in the drives tree after
         *          populate and run "add first child" for
         *          each subrecord that was inserted.
         *
         *@@added V0.9.18 (2002-02-06) [umoeller]
         */

        case FM_FILLFOLDER:
        {
            PFDRSPLITVIEW  psv;
            if (psv = WinQueryWindowPtr(hwndClient, QWL_USER))
            {
                PMINIRECORDCORE prec = (PMINIRECORDCORE)mp1;
                BOOL        fFolderChanged = FALSE;
                WPFolder    *pFolder;

                _PmpfF(("FM_FILLFOLDER %s",
                            prec->pszIcon));
                DumpFlags((ULONG)mp2);

                if (0 == ((ULONG)mp2 & FFL_FOLDERSONLY))
                    // not folders-only: then we need to
                    // refresh the files list
                    fdrClearContainer(psv->hwndFilesCnr,
                                      &psv->llFileObjectsInserted);

                fdrSplitPopulate(psv,
                                 prec,
                                 (ULONG)mp2);
            }
        }
        break;

        /*
         *@@ FM_POPULATED_FILLTREE:
         *      posted by fntPopulate after populate has been
         *      done for a folder. This gets posted in any case,
         *      if the folder was populated in folders-only mode
         *      or not.
         *
         *      Parameters:
         *
         *      --  PMINIRECORDCODE mp1: record of folder
         *          (or disk or whatever) to fill with.
         *
         *      --  ULONG mp2: FFL_* flags for whether to
         *          expand.
         *
         *@@added V0.9.18 (2002-02-06) [umoeller]
         */

        case FM_POPULATED_FILLTREE:
        {
            PFDRSPLITVIEW  psv;
            PMINIRECORDCORE prec;

            _PmpfF(("FM_POPULATED_FILLTREE %s",
                        mp1
                            ? ((PMINIRECORDCORE)mp1)->pszIcon
                            : "NULL"));

            if (    (psv = WinQueryWindowPtr(hwndClient, QWL_USER))
                 && (prec = (PMINIRECORDCORE)mp1)
               )
            {
                WPFolder    *pFolder = fdrGetFSFromRecord(mp1, TRUE);
                PLISTNODE   pNode;
                HWND        hwndAddFirstChild = NULLHANDLE;

                if ((ULONG)mp2 & FFL_EXPAND)
                {
                    BOOL        fOld = psv->fFileDlgReady;
                    // stop control notifications from messing with this
                    psv->fFileDlgReady = FALSE;
                    cnrhExpandFromRoot(psv->hwndTreeCnr,
                                       (PRECORDCORE)prec);
                    // then fire CM_ADDFIRSTCHILD too
                    hwndAddFirstChild = psv->hwndSplitPopulate;

                    // re-enable control notifications
                    psv->fFileDlgReady = fOld;
                }

                // insert subfolders into tree on the left
                fdrInsertContents(pFolder,
                                  psv->hwndTreeCnr,
                                  (PMINIRECORDCORE)mp1,
                                  INSERT_FOLDERSANDDISKS,
                                  hwndAddFirstChild,
                                  NULL,       // file mask
                                  &psv->llTreeObjectsInserted);
            }
        }
        break;

        /*
         *@@ FM_POPULATED_SCROLLTO:
         *
         *      Parameters:
         *
         *      --  PMINIRECORDCODE mp1: record of folder
         *          (or disk or whatever) that was populated
         *          and should now be scrolled to.
         *
         *@@added V0.9.18 (2002-02-06) [umoeller]
         */

        case FM_POPULATED_SCROLLTO:
        {
            PFDRSPLITVIEW  psv;
            if (psv = WinQueryWindowPtr(hwndClient, QWL_USER))
            {
                BOOL    fOld = psv->fFileDlgReady;
                ULONG   ul;

                _PmpfF(("FM_POPULATED_SCROLLTO %s",
                            mp1
                                ? ((PMINIRECORDCORE)mp1)->pszIcon
                                : "NULL"));

                // stop control notifications from messing with this
                psv->fFileDlgReady = FALSE;

                /* cnrhExpandFromRoot(pWinData->hwndTreeCnr,
                                   (PRECORDCORE)mp1); */
                ul = cnrhScrollToRecord(psv->hwndTreeCnr,
                                        (PRECORDCORE)mp1,
                                        CMA_ICON | CMA_TEXT | CMA_TREEICON,
                                        TRUE);       // keep parent
                cnrhSelectRecord(psv->hwndTreeCnr,
                                 (PRECORDCORE)mp1,
                                 TRUE);
                if (ul && ul != 3)
                    cmnLog(__FILE__, __LINE__, __FUNCTION__,
                            "Error: cnrhScrollToRecord returned %d", ul);

                // re-enable control notifications
                psv->fFileDlgReady = fOld;
            }
        }
        break;

        /*
         *@@ FM_POPULATED_FILLFILES:
         *      posted by fntPopulate after populate has been
         *      done for the newly selected folder, if this
         *      was not in folders-only mode. We must then fill
         *      the right half of the dialog with all the objects.
         *
         *      Parameters:
         *
         *      --  PMINIRECORDCODE mp1: record of folder
         *          (or disk or whatever) to fill with.
         *
         *      --  WPFolder* mp2: folder that was populated
         *          for that record.
         *
         *@@added V0.9.18 (2002-02-06) [umoeller]
         */

        case FM_POPULATED_FILLFILES:
        {
            PFDRSPLITVIEW  psv;
            if (psv = WinQueryWindowPtr(hwndClient, QWL_USER))
            {

                _PmpfF(("FM_POPULATED_FILLFILES %s",
                            mp1
                                ? ((PMINIRECORDCORE)mp1)->pszIcon
                                : "NULL"));


                if ((mp1) && (mp2))
                {
                    WPFolder    *pFolder = (WPFolder*)mp2;
                    CHAR        szPathName[2*CCHMAXPATH];

                    // insert all contents into list on the right
                    fdrInsertContents(pFolder,
                                      psv->hwndFilesCnr,
                                      NULL,        // parent
                                      INSERT_ALL,
                                      NULLHANDLE,  // no add first child
                                      NULL,        // no file mask
                                      &psv->llFileObjectsInserted);

                    // now, if this is for the files cnr, we must make
                    // sure that the container owner (the parent frame)
                    // has been subclassed by us... we do this AFTER
                    // the WPS subclasses the owner, which happens during
                    // record insertion

                    if (!psv->fFilesFrameSubclassed)
                    {
                        // not subclassed yet:
                        // subclass now

                        psv->psfvFiles
                            = fdrCreateSFV(psv->hwndFilesFrame,
                                           psv->hwndFilesCnr,
                                           QWL_USER,
                                           pFolder,
                                           pFolder);
                        psv->psfvFiles->pfnwpOriginal
                            = WinSubclassWindow(psv->hwndFilesFrame,
                                                fnwpSubclassedFilesFrame);
                        psv->fFilesFrameSubclassed = TRUE;
                    }
                    else
                    {
                        // already subclassed:
                        // update the folder pointers in the SFV
                        psv->psfvFiles->somSelf = pFolder;
                        psv->psfvFiles->pRealObject = pFolder;
                    }
                }
            }
        }
        break;

        /*
         * CM_UPDATEPOINTER:
         *      posted when threads exit etc. to update
         *      the current pointer.
         */

        case FM_UPDATEPOINTER:
        {
            PFDRSPLITVIEW  psv;
            if (psv = WinQueryWindowPtr(hwndClient, QWL_USER))
            {
                WinSetPointer(HWND_DESKTOP,
                              fdrSplitQueryPointer(psv));
            }
        }
        break;

        /*
         * WM_CLOSE:
         *      window list is being closed:
         *      clean up
         */

        case WM_CLOSE:
        {
            PFDRSPLITVIEW  psv;
            _PmpfF(("WM_CLOSE"));
            if (psv = WinQueryWindowPtr(hwndClient, QWL_USER))
            {
                // clear all containers, stop populate thread etc.
                fdrCleanupSplitView(psv);

                // destroy the frame window (which destroys us too)
                WinDestroyWindow(psv->hwndMainFrame);
            }

            // return default NULL
        }
        break;

        default:
            mrc = WinDefWindowProc(hwndClient, msg, mp1, mp2);
    }

    return mrc;
}

/* ******************************************************************
 *
 *   Left tree frame and client
 *
 ********************************************************************/

/*
 *@@ fnwpSubclassedTreeFrame:
 *
 */

MRESULT EXPENTRY fnwpSubclassedTreeFrame(HWND hwndFrame, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT             mrc = 0;
    BOOL                fCallDefault = FALSE;
    PSUBCLFOLDERVIEW    psfv = WinQueryWindowPtr(hwndFrame, QWL_USER);

    switch (msg)
    {
        case WM_CONTROL:
        {
            USHORT usID = SHORT1FROMMP(mp1),
                   usNotifyCode = SHORT2FROMMP(mp1);
            if (usID == FID_CLIENT)     // that's the container
            {
                switch (usNotifyCode)
                {
                    /*
                     * CN_EMPHASIS:
                     *      selection changed:
                     */

                    case CN_EMPHASIS:
                    {
                        HWND            hwndMainControl;
                        PFDRSPLITVIEW   psv;
                        PNOTIFYRECORDEMPHASIS pnre = (PNOTIFYRECORDEMPHASIS)mp2;
                        PMINIRECORDCORE prec;

                        if (    (pnre->pRecord)
                             && (pnre->fEmphasisMask & CRA_SELECTED)
                             && (prec = (PMINIRECORDCORE)pnre->pRecord)
                             && (prec->flRecordAttr & CRA_SELECTED)
                             && (hwndMainControl = WinQueryWindow(hwndFrame, QW_OWNER))
                             && (psv = WinQueryWindowPtr(hwndMainControl, QWL_USER))
                             // notifications not disabled?
                             && (psv->fFileDlgReady)
                             // record changed?
                             && (prec != psv->precFolderContentsShowing)
                           )
                        {
                            _PmpfF(("CN_EMPHASIS %s",
                                    prec->pszIcon));

                            fdrPostFillFolder(psv,
                                              prec,
                                              0);
                        }
                    }
                    break;

                    /*
                     * CN_EXPANDTREE:
                     *      user clicked on "+" sign next to
                     *      tree item; expand that, but start
                     *      "add first child" thread again
                     */

                    case CN_EXPANDTREE:
                    {
                        HWND            hwndMainControl;
                        PFDRSPLITVIEW   psv;
                        PMINIRECORDCORE prec;

                        if (    (hwndMainControl = WinQueryWindow(hwndFrame, QW_OWNER))
                             && (psv = WinQueryWindowPtr(hwndMainControl, QWL_USER))
                             // notifications not disabled?
                             && (psv->fFileDlgReady)
                             && (prec = (PMINIRECORDCORE)mp2)
                           )
                        {
                            _PmpfF(("CN_EXPANDTREE %s",
                                    prec->pszIcon));

                            fdrPostFillFolder(psv,
                                              prec,
                                              FFL_FOLDERSONLY | FFL_EXPAND);

                            // and call default because xfolder
                            // handles auto-scroll
                            fCallDefault = TRUE;
                        }
                    }
                    break;

                    /*
                     * CN_ENTER:
                     *      intercept this so that we won't open
                     *      a folder view.
                     *
                     *      Before this, we should have gotten
                     *      CN_EMPHASIS so the files list has
                     *      been updated already.
                     *
                     *      Instead, check whether the record has
                     *      been expanded or collapsed and do
                     *      the reverse.
                     */

                    case CN_ENTER:
                    {
                        PNOTIFYRECORDENTER pnre;
                        PMINIRECORDCORE prec;

                        if (    (pnre = (PNOTIFYRECORDENTER)mp2)
                             && (prec = (PMINIRECORDCORE)pnre->pRecord)
                                        // can be null for whitespace!
                           )
                        {
                            ULONG ulmsg = CM_EXPANDTREE;
                            if (prec->flRecordAttr & CRA_EXPANDED)
                                ulmsg = CM_COLLAPSETREE;

                            WinPostMsg(pnre->hwndCnr,
                                       ulmsg,
                                       (MPARAM)prec,
                                       0);
                        }
                    }
                    break;

                    default:
                        fCallDefault = TRUE;
                }
            }
            else
                fCallDefault = TRUE;
        }
        break;

        case WM_CHAR:
            // forward to main client
            WinPostMsg(WinQueryWindow(hwndFrame, QW_OWNER),
                       msg,
                       mp1,
                       mp2);
        break;

        case WM_SYSCOMMAND:
            // forward to main frame
            WinPostMsg(WinQueryWindow(WinQueryWindow(hwndFrame, QW_OWNER),
                                      QW_OWNER),
                       msg,
                       mp1,
                       mp2);
        break;

        case WM_CONTROLPOINTER:
        {
            HWND hwndMainControl = WinQueryWindow(hwndFrame, QW_OWNER);
                                    // the main client
            PFDRSPLITVIEW psv = WinQueryWindowPtr(hwndMainControl, QWL_USER);

            mrc = (MPARAM)fdrSplitQueryPointer(psv);
        }
        break;

        default:
            fCallDefault = TRUE;
    }

    if (fCallDefault)
        mrc = fdrProcessFolderMsgs(hwndFrame,
                                   msg,
                                   mp1,
                                   mp2,
                                   psfv,
                                   psfv->pfnwpOriginal);

    return mrc;
}

/* ******************************************************************
 *
 *   Right files frame and client
 *
 ********************************************************************/

/*
 *@@ fnwpSubclassedFilesFrame:
 *      subclassed frame window on the right for the
 *      "Files" container.
 *
 *      We use the XFolder subclassed window proc for
 *      most messages. In addition, we intercept a
 *      couple more for extra features.
 */

static MRESULT EXPENTRY fnwpSubclassedFilesFrame(HWND hwndFrame, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT             mrc = 0;
    BOOL                fCallDefault = FALSE;
    PSUBCLFOLDERVIEW    psfv = WinQueryWindowPtr(hwndFrame, QWL_USER);

    switch (msg)
    {
        case WM_CONTROL:
        {
            USHORT usID = SHORT1FROMMP(mp1),
                   usNotifyCode = SHORT2FROMMP(mp1);
            if (usID == FID_CLIENT)     // that's the container
            {
                switch (usNotifyCode)
                {
                    case CN_ENTER:
                    {
                        PNOTIFYRECORDENTER pnre;
                        PMINIRECORDCORE prec;
                        HWND hwndMainControl;
                        PFDRSPLITVIEW psv;
                        WPObject *pobj;

                        if (    (pnre = (PNOTIFYRECORDENTER)mp2)
                             && (prec = (PMINIRECORDCORE)pnre->pRecord)
                                        // can be null for whitespace!
                             && (hwndMainControl = WinQueryWindow(hwndFrame, QW_OWNER))
                             && (psv = WinQueryWindowPtr(hwndMainControl, QWL_USER))
                             && (pobj = fdrGetFSFromRecord(prec,
                                                           TRUE))       // folders only:
                           )
                        {
                            // dou

                        }
                        else
                            fCallDefault = TRUE;
                    }
                    break;

                    default:
                        fCallDefault = TRUE;
                }
            }
            else
                fCallDefault = TRUE;
        }
        break;

        case WM_CHAR:
            // forward to main client
            WinPostMsg(WinQueryWindow(hwndFrame, QW_OWNER),
                       msg,
                       mp1,
                       mp2);
        break;

        case WM_SYSCOMMAND:
            // forward to main frame
            WinPostMsg(WinQueryWindow(WinQueryWindow(hwndFrame, QW_OWNER),
                                      QW_OWNER),
                       msg,
                       mp1,
                       mp2);
        break;

        case WM_CONTROLPOINTER:
        {
            HWND hwndMainControl = WinQueryWindow(hwndFrame, QW_OWNER);
                                    // the main client
            PFDRSPLITVIEW psv = WinQueryWindowPtr(hwndMainControl, QWL_USER);

            mrc = (MPARAM)fdrSplitQueryPointer(psv);
        }
        break;

        default:
            fCallDefault = TRUE;
    }

    if (fCallDefault)
        mrc = fdrProcessFolderMsgs(hwndFrame,
                                   msg,
                                   mp1,
                                   mp2,
                                   psfv,
                                   psfv->pfnwpOriginal);

    return mrc;
}

/* ******************************************************************
 *
 *   Folder split view
 *
 ********************************************************************/

/*
 *@@ SPLITVIEWPOS:
 *
 *@@added V0.9.21 (2002-08-21) [umoeller]
 */

typedef struct _SPLITVIEWPOS
{
    LONG        x,
                y,
                cx,
                cy;

    LONG        lSplitBarPos;

} SPLITVIEWPOS, *PSPLITVIEWPOS;

/*
 *@@ SPLITVIEWDATA:
 *
 */

typedef struct _SPLITVIEWDATA
{
    USHORT          cb;

    USEITEM         ui;
    VIEWITEM        vi;

    CHAR            szFolderPosKey[10];

    FDRSPLITVIEW    sv;         // pRootFolder == somSelf

} SPLITVIEWDATA, *PSPLITVIEWDATA;

/*
 *@@ fnwpSplitViewFrame:
 *
 *@@added V0.9.21 (2002-08-21) [umoeller]
 */

MRESULT EXPENTRY fnwpSplitViewFrame(HWND hwndFrame, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    switch (msg)
    {
        case WM_SYSCOMMAND:
        {
            PSPLITVIEWDATA psvd;
            if (    (SHORT1FROMMP(mp1) == SC_CLOSE)
                 && (psvd = (PSPLITVIEWDATA)WinQueryWindowPtr(hwndFrame,
                                                              QWL_USER))
               )
            {
                // save window position
                SWP swp;
                SPLITVIEWPOS pos;
                WinQueryWindowPos(hwndFrame,
                                  &swp);
                pos.x = swp.x;
                pos.y = swp.y;
                pos.cx = swp.cx;
                pos.cy = swp.cy;

                pos.lSplitBarPos = ctlQuerySplitPos(psvd->sv.hwndSplitWindow);

                PrfWriteProfileData(HINI_USER,
                                    (PSZ)INIAPP_FDRSPLITVIEWPOS,
                                    psvd->szFolderPosKey,
                                    &pos,
                                    sizeof(pos));
            }

            mrc = G_pfnFrameProc(hwndFrame, msg, mp1, mp2);
        }
        break;

        case WM_DESTROY:
        {
            PSPLITVIEWDATA psvd;

            if (psvd = (PSPLITVIEWDATA)WinQueryWindowPtr(hwndFrame,
                                                         QWL_USER))
            {
                _PmpfF(("PSPLITVIEWDATA is 0x%lX", psvd));

                _wpDeleteFromObjUseList(psvd->sv.pRootFolder,  // somSelf
                                        &psvd->ui);

                _wpFreeMem(psvd->sv.pRootFolder,  // somSelf,
                           (PBYTE)psvd);
            }

            mrc = G_pfnFrameProc(hwndFrame, msg, mp1, mp2);
        }
        break;

        default:
            mrc = G_pfnFrameProc(hwndFrame, msg, mp1, mp2);
    }

    return mrc;
}

/*
 *@@ fdrCreateSplitView:
 *      creates a frame window with a split window and
 *      registers it as a folder view.
 *
 *      Returns the frame window or NULLHANDLE on errors.
 *
 */

HWND fdrCreateSplitView(WPFolder *somSelf,
                        ULONG ulView)
{
    static  s_fRegistered = FALSE;

    HWND    hwndReturn = NULLHANDLE;

    TRY_LOUD(excpt1)
    {
        PSPLITVIEWDATA  psvd;
        ULONG           rc;

        if (!s_fRegistered)
        {
            s_fRegistered = TRUE;

            WinRegisterClass(winhMyAnchorBlock(),
                             (PSZ)WC_SPLITVIEWCLIENT,
                             fnwpSplitController,
                             CS_SIZEREDRAW,
                             sizeof(PSPLITVIEWDATA));
        }

        if (psvd = (PSPLITVIEWDATA)_wpAllocMem(somSelf,
                                               sizeof(SPLITVIEWDATA),
                                               &rc))
        {
            SPLITVIEWPOS pos;
            ULONG   cbPos;
            ULONG   flFrame = FCF_NOBYTEALIGN
                                  | FCF_TITLEBAR
                                  | FCF_SYSMENU
                                  | FCF_SIZEBORDER
                                  | FCF_AUTOICON;
            ULONG   ulButton = _wpQueryButtonAppearance(somSelf);

            ZERO(psvd);

            if (ulButton == DEFAULTBUTTON)
                ulButton = PrfQueryProfileInt(HINI_USER,
                                              "PM_ControlPanel",
                                              "MinButtonType",
                                              HIDEBUTTON);

            if (ulButton == HIDEBUTTON)
                flFrame |= FCF_HIDEMAX;     // hide and maximize
            else
                flFrame |= FCF_MINMAX;      // minimize and maximize

            psvd->cb = sizeof(SPLITVIEWDATA);

            psvd->sv.pRootFolder = somSelf;

            // try to restore window position, if present
            sprintf(psvd->szFolderPosKey,
                    "%lX",
                    _wpQueryHandle(somSelf));
            cbPos = sizeof(pos);
            if (    (!(PrfQueryProfileData(HINI_USER,
                                           (PSZ)INIAPP_FDRSPLITVIEWPOS,
                                           psvd->szFolderPosKey,
                                           &pos,
                                           &cbPos)))
                 || (cbPos != sizeof(SPLITVIEWPOS))
               )
            {
                // no position stored yet:
                pos.x = (winhQueryScreenCX() - 600) / 2;
                pos.y = (winhQueryScreenCY() - 400) / 2;
                pos.cx = 600;
                pos.cy = 400;
                psvd->sv.lSplitBarPos = 30;
            }
            else
                psvd->sv.lSplitBarPos = pos.lSplitBarPos;

            // create the frame and the client;
            // the client gets psvd in mp1 with WM_CREATE
            // and creates the split window and everything else
            if (psvd->sv.hwndMainFrame = winhCreateStdWindow(HWND_DESKTOP, // parent
                                                             NULL,         // pswp
                                                             flFrame,
                                                             0,            // window style, not yet visible
                                                             "Split view",
                                                             0,            // resids
                                                             WC_SPLITVIEWCLIENT,
                                                             WS_VISIBLE,   // client style
                                                             0,            // frame ID
                                                             &psvd->sv,
                                                             &psvd->sv.hwndMainControl))
            {
                PMINIRECORDCORE pRootRec;
                POINTL  ptlIcon = {0, 0};

                // insert somSelf as the root of the tree
                pRootRec = _wpCnrInsertObject(psvd->sv.pRootFolder,
                                              psvd->sv.hwndTreeCnr,
                                              &ptlIcon,
                                              NULL,       // parent record
                                              NULL);      // RECORDINSERT

                // _wpCnrInsertObject subclasses the container owner,
                // so subclass this with the XFolder subclass
                // proc again; otherwise the new menu items
                // won't work
                psvd->sv.psfvTree = fdrCreateSFV(psvd->sv.hwndTreeFrame,
                                                 psvd->sv.hwndTreeCnr,
                                                 QWL_USER,
                                                 psvd->sv.pRootFolder,
                                                 psvd->sv.pRootFolder);
                psvd->sv.psfvTree->pfnwpOriginal = WinSubclassWindow(psvd->sv.hwndTreeFrame,
                                                                     fnwpSubclassedTreeFrame);

                // and populate this once we're running
                fdrPostFillFolder(&psvd->sv,
                                  pRootRec,
                                  /* FFL_FOLDERSONLY | */ FFL_EXPAND);

                // view-specific stuff

                _PmpfF(("setting frame's PSPLITVIEWDATA to 0x%lX", psvd));

                WinSetWindowPtr(psvd->sv.hwndMainFrame,
                                QWL_USER,
                                psvd);
                G_pfnFrameProc = WinSubclassWindow(psvd->sv.hwndMainFrame,
                                                   fnwpSplitViewFrame);

                // register the view
                cmnRegisterView(somSelf,
                                &psvd->ui,
                                ulView,
                                psvd->sv.hwndMainFrame,
                                cmnGetString(ID_XFSI_FDR_SPLITVIEW));

                WinSetWindowPos(psvd->sv.hwndMainFrame,
                                HWND_TOP,
                                pos.x,
                                pos.y,
                                pos.cx,
                                pos.cy,
                                SWP_MOVE | SWP_SIZE);

                WinSetWindowPos(psvd->sv.hwndMainFrame,
                                HWND_TOP,
                                0, 0, 0, 0,
                                SWP_SHOW | SWP_ACTIVATE | SWP_ZORDER);

                _PmpfF(("frame's PSPLITVIEWDATA is now 0x%lX",
                        WinQueryWindowPtr(psvd->sv.hwndMainFrame, QWL_USER) ));

                psvd->sv.fFileDlgReady = TRUE;

                hwndReturn = psvd->sv.hwndMainFrame;
            }
        }
    }
    CATCH(excpt1)
    {
    } END_CATCH();

    return hwndReturn;
}
