
/*
 *@@sourcefile fileops.h:
 *      header file for fileops.c (file operations).
 *
 *      This file is ALL new with V0.9.0.
 *
 *@@include #include <os2.h>
 *@@include #include <wpobject.h>
 *@@include #include "filesys\fileops.h"
 *@@include #include "helpers\linklist.h" // only for some funcs
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

#ifndef FILEOPS_HEADER_INCLUDED
    #define FILEOPS_HEADER_INCLUDED

    #ifndef SOM_WPObject_h
        #error fileops.h requires wpobject.h to be included.
    #endif

    /* ******************************************************************
     *                                                                  *
     *   FOPSRET: file operations error codes                           *
     *                                                                  *
     ********************************************************************/

    #define FOPSERR_OK                        0
    #define FOPSERR_NOT_HANDLED_ABORT         1
    #define FOPSERR_INVALID_OBJECT            2
    #define FOPSERR_INTEGRITY_ABORT           3
    #define FOPSERR_CANCELLEDBYUSER           4
    // #define FOPSERR_NODELETETRASHOBJECT       5
    #define FOPSERR_MOVE2TRASH_READONLY       6         // non-fatal
            // moving WPFileSystem which has read-only:
            // this should prompt the user
    #define FOPSERR_MOVE2TRASH_NOT_DELETABLE  7         // fatal
            // moving non-deletable to trash can: this should abort
    #define FOPSERR_DELETE_READONLY           8         // non-fatal
            // deleting WPFileSystem which has read-only flag;
            // this should prompt the user
    #define FOPSERR_DELETE_NOT_DELETABLE      9         // fatal
            // deleting not-deletable; this should abort
    // #define FOPSERR_DESTROYNONTRASHOBJECT     10        // fatal
    #define FOPSERR_TRASHDRIVENOTSUPPORTED    11         // non-fatal
    #define FOPSERR_WPFREE_FAILED             12        // fatal
    #define FOPSERR_LOCK_FAILED               13        // fatal
            // requesting object mutex failed

    typedef unsigned char FOPSRET;

    /* ******************************************************************
     *                                                                  *
     *   Trash can setup                                                *
     *                                                                  *
     ********************************************************************/

    BOOL fopsTrashCanReady(VOID);

    BOOL fopsEnableTrashCan(HWND hwndOwner,
                            BOOL fEnable);

    /* ******************************************************************
     *                                                                  *
     *   Expanded object lists                                          *
     *                                                                  *
     ********************************************************************/

    #ifdef LINKLIST_HEADER_INCLUDED

        /*
         *@@ EXPANDEDOBJECT:
         *      used with fopsFolder2ExpandedList
         *      and fopsExpandObjectDeep. See remarks
         *      there.
         *
         *@@added V0.9.2 (2000-02-28) [umoeller]
         */

        typedef struct _EXPANDEDOBJECT
        {
            WPObject    *pObject;
                    // object; this can be of any class
            PLINKLIST   pllContentsSFL;
                    // if the object is a WPFolder, this
                    // is != NULL and contains a list of
                    // more EXPANDEDOBJECT's;
                    // if the object is not a folder, this
                    // item is always NULL
            ULONG       ulSizeThis;
                    // size of pObject; if pObject is a folder,
                    // this has the size of all objects
                    // on pllContentsSFL and sub-lists
        } EXPANDEDOBJECT, *PEXPANDEDOBJECT;

        PEXPANDEDOBJECT fopsExpandObjectDeep(WPObject *pObject);

        VOID fopsFreeExpandedObject(PEXPANDEDOBJECT pSOI);

        FOPSRET fopsExpandObjectFlat(PLINKLIST pllObjects,
                                     WPObject *pObject);
    #endif

    /********************************************************************
     *                                                                  *
     *   "File exists" (title clash) dialog                             *
     *                                                                  *
     ********************************************************************/

    ULONG fopsConfirmObjectTitle(WPObject *somSelf,
                                 WPFolder* Folder,
                                 WPObject** ppDuplicate,
                                 PSZ pszTitle,
                                 ULONG cbTitle,
                                 ULONG menuID);

    BOOL fopsMoveObjectConfirmed(WPObject *pObject,
                                 WPFolder *pTargetFolder);

    /********************************************************************
     *                                                                  *
     *   Generic file tasks framework                                   *
     *                                                                  *
     ********************************************************************/

    // file operation identifiers
    #define XFT_MOVE2TRASHCAN       1
    #define XFT_RESTOREFROMTRASHCAN 2
    // #define XFT_DESTROYTRASHOBJECTS 3
    #define XFT_TRUEDELETE          4

    typedef PVOID HFILETASKLIST;

    // status id for FOPSUPDATE
    #define FOPSUPD_EXPANDINGOBJECT         1
    #define FOPSUPD_SOURCEOBJECT            2
    #define FOPSUPD_SUBOBJECT               3

    /*
     *@@ FOPSUPDATE:
     *      notification structure used with
     *      FNFOPSPROGRESSCALLBACK.
     *
     *@@added V0.9.1 (2000-01-30) [umoeller]
     */

    typedef struct _FOPSUPDATE
    {
        ULONG       ulOperation;        // operation, as specified with fopsCreateFileTaskList

        WPFolder    *pSourceFolder;     // source folder; see fopsCreateFileTaskList
        WPFolder    *pTargetFolder;     // source folder; see fopsCreateFileTaskList

        ULONG       ulStatus;
                        // one of the following:
                        //  -- FOPSUPD_EXPANDINGOBJECT: collecting objects before
                        //     actual processing is started (with XFT_TRUEDELETE);
                        //     in that case, pObject points to the object
                        //     being expanded.
                        // --  FOPSUPD_SOURCEOBJECT: pObject is one of the
                        //     objects on the file task list (in pSourceFolder)
                        //     and is currently being processed. The title should be
                        //     displayed, and the progress bar should be updated.
                        // --  FOPSUPD_SUBOBJECT: pObject is an object in one
                        //     of the subfolders of the actual source objects.
                        //     The title should be
                        //     displayed, but the progress bar should not be updated,
                        //     since the values below are constant (speed).
        WPObject    *pObject;    // current object being worked on

        ULONG       ulCurrentObject;
                        // index of pCurrentObject (out of ulTotalObjects); useful
                        // for progress bar. This only changes when
                        // (ulStatus == FOPSUPD_SOURCEOBJECT).
        ULONG       ulTotalObjects;
                        // total objects count (100%).
    } FOPSUPDATE, *PFOPSUPDATE;

    /*
     *@@ FNFOPSPROGRESSCALLBACK:
     *      callback prototype used with file operations.
     *      Specify such a function with
     *      fopsCreateFileTasksList.
     *
     *      The callback gets called _before_ each object
     *      to be processed. In the FOPSUPDATE structure,
     *      the pCurrentObject field has that object's
     *      pointer. With each call, ulCurrentObject
     *      is incremented (starting at 0 for the first
     *      object).
     *
     *      If the callback returns FALSE, processing
     *      is aborted.
     *
     *      After all objects have been processed (or
     *      FALSE has been returned by the callback),
     *      the callback gets called once more with
     *      (pfu == NULL) to signify completion.
     *
     *      Warning: this callback gets called on the
     *      File thread, not on PM thread 1. Do not
     *      create windows here, but post message to
     *      a window on thread 1 instead.
     *
     *      The File thread does have a message queue,
     *      so you can send messages instead also.
     *
     *      Also, while the File thread is processing
     *      the file tasks lists, both the source and
     *      the target folders are locked using
     *      _wpRequestObjectMutexSem. Do not attempt
     *      to modify any objects in the callback!!
     *
     *@@added V0.9.1 (2000-01-30) [umoeller]
     */

    typedef BOOL APIENTRY FNFOPSPROGRESSCALLBACK(PFOPSUPDATE pfu,
                                                 ULONG ulUser);

    /*
     *@@ FNFOPSERRORCALLBACK:
     *      error callback if problems are encountered
     *      during file processing. If this function
     *      returns FOPSERR_OK, processing continues.
     *      For every other return value, processing
     *      will be aborted, and the return value
     *      will be passed upwards to the caller.
     *
     *      ulError will be one of the following:
     *
     *      -- FOPSERR_NODELETETRASHOBJECT: cannot move
     *         XWPTrashObject instances to trash can.
     *         ulOperation should be switched to
     *         XFT_DESTROYTRASHOBJECTS instead.
     *
     *      -- FOPSERR_OBJSTYLENODELETE: object to be deleted
     *         has OBJSTYLE_NODELETE flag.
     *
     *      -- FOPSERR_DESTROYNONTRASHOBJECT: trying to
     *         destroy trash object which is not of XWPTrashObject
     *         class. Shouldn't happen.
     *
     *      -- FOPSERR_TRASHDRIVENOTSUPPORTED: trying to
     *         move an object to the trash can whose drive
     *         is not supported.
     *
     *@@added V0.9.2 (2000-03-04) [umoeller]
     */

    typedef FOPSRET APIENTRY FNFOPSERRORCALLBACK(ULONG ulOperation,
                                                 WPObject *pObject,
                                                 FOPSRET frError);

    HFILETASKLIST fopsCreateFileTaskList(ULONG ulOperation,
                                         WPFolder *pSourceFolder,
                                         WPFolder *pTargetFolder,
                                         FNFOPSPROGRESSCALLBACK *pfnProgressCallback,
                                         FNFOPSERRORCALLBACK *pfnErrorCallback,
                                         ULONG ulUser);

    FOPSRET fopsValidateObjOperation(ULONG ulOperation,
                                     FNFOPSERRORCALLBACK *pfnErrorCallback,
                                     WPObject *pObject);

    FOPSRET fopsAddObjectToTask(HFILETASKLIST hftl,
                                WPObject *pObject);

    BOOL fopsStartTaskAsync(HFILETASKLIST hftl);

    VOID fopsFileThreadProcessing(HFILETASKLIST hftl);

    BOOL fopsDeleteFileTaskList(HFILETASKLIST hftl);

    /********************************************************************
     *                                                                  *
     *   Generic file operations implementation                         *
     *                                                                  *
     ********************************************************************/

    FOPSRET fopsStartTaskFromCnr(ULONG ulOperation,
                                 WPFolder *pSourceFolder,
                                 WPFolder *pTargetFolder,
                                 WPObject *pSourceObject,
                                 ULONG ulSelection,
                                 BOOL fRelatedObjects,
                                 HWND hwndCnr,
                                 ULONG ulMsgSingle,
                                 ULONG ulMsgMultiple);

    #ifdef LINKLIST_HEADER_INCLUDED
        FOPSRET fopsStartTaskFromList(ULONG ulOperation,
                                      WPFolder *pSourceFolder,
                                      WPFolder *pTargetFolder,
                                      PLINKLIST pllObjects);
    #endif

    /********************************************************************
     *                                                                  *
     *   Trash can file operations implementation                       *
     *                                                                  *
     ********************************************************************/

    FOPSRET fopsStartTrashDeleteFromCnr(WPFolder *pSourceFolder,
                                        WPObject *pSourceObject,
                                        ULONG ulSelection,
                                        HWND hwndCnr,
                                        BOOL fTrueDelete);

    FOPSRET fopsStartTrashRestoreFromCnr(WPFolder *pTrashSource,
                                         WPFolder *pTargetFolder,
                                         WPObject *pSourceObject,
                                         ULONG ulSelection,
                                         HWND hwndCnr);

    FOPSRET fopsStartTrashDestroyFromCnr(WPFolder *pTrashSource,
                                         WPObject *pSourceObject,
                                         ULONG ulSelection,
                                         HWND hwndCnr);
#endif


