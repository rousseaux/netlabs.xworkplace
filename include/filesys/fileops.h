
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

        PEXPANDEDOBJECT fopsExpandObjectDeep(WPObject *pObject,
                                             BOOL fFoldersOnly);

        VOID fopsFreeExpandedObject(PEXPANDEDOBJECT pSOI);

        FOPSRET fopsExpandObjectFlat(PLINKLIST pllObjects,
                                     WPObject *pObject,
                                     BOOL fFoldersOnly,
                                     PULONG pulObjectCount,
                                     PULONG pulDormantFilesCount);
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
    #define XFT_TRUEDELETE          3
    #define XFT_POPULATE            4
    #define XFT_INSTALLFONTS        5
    #define XFT_DEINSTALLFONTS      6

    typedef PVOID HFILETASKLIST;

    // status flags for FOPSUPDATE

    // only one of the following:
    // #define FOPSUPD_EXPANDINGOBJECT         1
    // #define FOPSUPD_SOURCEOBJECT            2
    // #define FOPSUPD_SUBOBJECT               3

    // to reduce flicker, the fields which have changed:
    #define FOPSUPD_SOURCEFOLDER_CHANGED        0x00000010
    #define FOPSUPD_TARGETFOLDER_CHANGED        0x00000020

    #define FOPSUPD_SOURCEOBJECT_CHANGED        0x00000100
                // pSourceObject changed
    #define FOPSUPD_SUBOBJECT_CHANGED           0x00000200
                // pszSubObject changed; this can be NULL if the
                // subobjects have been processed and we're going
                // for the next source object

    #define FOPSUPD_EXPANDING_SOURCEOBJECT_1ST  0x00001000
                // pSourceObject is currently being expanded;
                // only set after the first call for pSourceObject
                // without any other flags being set; after this,
                // several FOPSUPD_SUBOBJECT_CHANGED calls come in
    #define FOPSUPD_EXPANDING_SOURCEOBJECT_DONE 0x00002000
                // done expanding pSourceObject

    #define FOPSPROG_UPDATE_PROGRESS            0x10000000
                // progress fields have changed: update progress bar
    #define FOPSPROG_DONEWITHALL                0x20000000
                // done with all!

    /*
     *@@ FOPSUPDATE:
     *      notification structure used with
     *      FNFOPSPROGRESSCALLBACK.
     *
     *@@added V0.9.1 (2000-01-30) [umoeller]
     *@@changed V0.9.3 (2000-04-30) [umoeller]: changed completely for cleaner interface
     */

    typedef struct _FOPSUPDATE
    {
        ULONG       ulOperation;        // operation, as specified with fopsCreateFileTaskList

        WPFolder    *pSourceFolder;
                        // source folder (see fopsCreateFileTaskList).
                        // This is only valid if FOPSUPD_SOURCEFOLDER_CHANGED
                        // is set in the update flags.
        WPFolder    *pTargetFolder;
                        // target folder (see fopsCreateFileTaskList).
                        // This is only valid if FOPSUPD_TARGETFOLDER_CHANGED
                        // is set in the update flags.

        BOOL        fFirstCall;
        ULONG       flChanged;

        WPObject    *pSourceObject;
                        // current source object being worked on
                        // ("real" object on which the operation was invoked).
                        // This is only valid if FOPSUPD_SOURCEOBJECT_CHANGED
                        // is set in the update flags.
        PSZ         pszSubObject;
                        // if pSourceObject is a folder, this contains a
                        // description (usually a file name) of a subobject
                        // that is being worked on.
                        // This is only valid if FOPSUPD_SUBOBJECT_CHANGED
                        // is set in the update flags.

        ULONG       ulProgressScalar;
                        // a scalar signalling the total progress of the operation;
                        // this is between 0 and ulProgressMax, which represents 100%.
                        // The scalars are the number of objects * 100 plus a value
                        // between 0 and 100 for the subobjects. If there are no
                        // subobjects, the scalar advances in steps of 100.
                        // If flProgress & FOPSPROG_UPDATE_PROGRESS, the progress
                        // has changed and should be updated in the progress dialog.
        ULONG       ulProgressMax;
                        // maximum progress value
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
     *      returns NO_ERROR, processing continues.
     *      For every other return value, processing
     *      will be aborted, and the return value
     *      will be passed upwards to the caller.
     *
     *      ulError will be one of the following:
     *
     *      -- FOPSERR_MOVE2TRASH_READONLY: a read-only
     *         object is to be moved into the trash can.
     *         This error is recoverable. If subsequent
     *         errors of this type are to be ignored,
     *         the callback should OR *pfIgnoreSubsequent
     *         with FOPS_ISQ_MOVE2TRASH_READONLY.
     *
     *      -- FOPSERR_DELETE_READONLY: file-system object to
     *         be deleted is read-only.
     *         This error is recoverable. If subsequent
     *         errors of this type are to be ignored,
     *         the callback should OR *pfIgnoreSubsequent
     *         with FOPS_ISQ_DELETE_READONLY.
     *
     *      -- FOPSERR_DELETE_NOT_DELETABLE: some other
     *         object is non-deleteable. This is not
     *         recoverable.
     *
     *@@added V0.9.2 (2000-03-04) [umoeller]
     *@@changed V0.9.4 (2000-07-27) [umoeller]: added pfIgnoreSubsequent
     */

    typedef FOPSRET APIENTRY FNFOPSERRORCALLBACK(ULONG ulOperation,
                                                 WPObject *pObject,
                                                 FOPSRET frError,
                                                 PBOOL pfIgnoreSubsequent);

    #define FOPS_ISQ_MOVE2TRASH_READONLY    0x0001
    #define FOPS_ISQ_DELETE_READONLY        0x0002
    #define FOPS_ISQ_DELETE_FOLDERS         0x0004      // V0.9.16 (2001-12-06) [umoeller]
    #define FOPS_ISQ_FONTINSTALL            0x0008

    FOPSRET fopsCreateFileTaskList(HFILETASKLIST *phftl,
                                   ULONG ulOperation,
                                   WPFolder *pSourceFolder,
                                   WPFolder *pTargetFolder,
                                   FNFOPSPROGRESSCALLBACK *pfnProgressCallback,
                                   FNFOPSERRORCALLBACK *pfnErrorCallback,
                                   ULONG ulUser);

    FOPSRET fopsValidateObjOperation(ULONG ulOperation,
                                     FNFOPSERRORCALLBACK *pfnErrorCallback,
                                     WPObject *pObject,
                                     PBOOL pfIgnoreSubsequent);

    FOPSRET fopsAddObjectToTask(HFILETASKLIST hftl,
                                WPObject *pObject);

    FOPSRET fopsStartTask(HFILETASKLIST hftl,
                          HAB hab);

    VOID fopsFileThreadProcessing(HAB hab,
                                  HFILETASKLIST hftl,
                                  HWND hwndNotify);

    BOOL fopsDeleteFileTaskList(HFILETASKLIST hftl);

    /********************************************************************
     *                                                                  *
     *   Generic file operations implementation                         *
     *                                                                  *
     ********************************************************************/

    /*
     *@@ FOPSCONFIRM:
     *      confirmation data passed with fopsStartTaskFromCnr.
     *
     *@@added V0.9.5 (2000-08-11) [umoeller]
     */

    typedef struct _FOPSCONFIRM
    {
        ULONG       ulMsgSingle,        // in: msg no. if single object
                    ulMsgMultiple;      // in: msg no. if multiple objects
        HWND        hwndOwner;          // in: owner window for msg box
    } FOPSCONFIRM, *PFOPSCONFIRM;

     FOPSRET fopsStartTaskFromCnr(ULONG ulOperation,
                                  HAB hab,
                                  WPFolder *pSourceFolder,
                                  WPFolder *pTargetFolder,
                                  WPObject *pSourceObject,
                                  ULONG ulSelection,
                                  BOOL fRelatedObjects,
                                  HWND hwndCnr,
                                  PFOPSCONFIRM pConfirm);

    #ifdef LINKLIST_HEADER_INCLUDED
        FOPSRET fopsStartTaskFromList(ULONG ulOperation,
                                      HAB hab,
                                      WPFolder *pSourceFolder,
                                      WPFolder *pTargetFolder,
                                      PLINKLIST pllObjects);
    #endif

    /********************************************************************
     *                                                                  *
     *   Trash can file operations implementation                       *
     *                                                                  *
     ********************************************************************/

    FOPSRET fopsStartDeleteFromCnr(HAB hab,
                                   WPObject *pSourceObject,
                                   ULONG ulSelection,
                                   HWND hwndCnr,
                                   BOOL fTrueDelete);

    FOPSRET fopsStartTrashRestoreFromCnr(HAB hab,
                                         WPFolder *pTrashSource,
                                         WPFolder *pTargetFolder,
                                         WPObject *pSourceObject,
                                         ULONG ulSelection,
                                         HWND hwndCnr);

    FOPSRET fopsStartTrashDestroyFromCnr(HAB hab,
                                         WPFolder *pTrashSource,
                                         WPObject *pSourceObject,
                                         ULONG ulSelection,
                                         HWND hwndCnr,
                                         BOOL fConfirm);

    FOPSRET fopsStartPopulate(HAB hab,
                              WPFolder *pFolder);


    FOPSRET fopsStartFontDeinstallFromCnr(HAB hab,
                                          WPFolder *pFontFolderSource,
                                          WPObject *pSourceObject,
                                          ULONG ulSelection,
                                          HWND hwndCnr,
                                          BOOL fConfirm);
#endif


