
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

        PLINKLIST fopsFolder2ExpandedList(WPFolder *pFolder,
                                          PULONG pulSizeContents);

        PEXPANDEDOBJECT fopsExpandObject(WPObject *pObject);

        VOID fopsFreeExpandedList(PLINKLIST pllSFL);

        VOID fopsFreeExpandedObject(PEXPANDEDOBJECT pSOI);

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

    #define XFT_MOVE2TRASHCAN       1
    #define XFT_RESTOREFROMTRASHCAN 2
    #define XFT_DESTROYTRASHOBJECTS 3
    #define XFT_EMPTYTRASHCAN       4
    #define XFT_DELETE              5

    typedef PVOID HFILETASKLIST;

    /*
     *@@ FOPSUPDATE:
     *      notification structure used with
     *      FNFOPSPROGRESSCALLBACK.
     *
     *@@added V0.9.1 (2000-01-30) [umoeller]
     */

    typedef struct _FOPSUPDATE
    {
        ULONG       ulOperation;

        WPFolder    *pSourceFolder;
        WPObject    *pCurrentObject;
        WPFolder    *pTargetFolder;

        ULONG       ulCurrentObject;
        ULONG       ulTotalObjects;
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

    #define FOPSERR_NODELETETRASHOBJECT         1
    #define FOPSERR_OBJSTYLENODELETE            2
    #define FOPSERR_DESTROYNONTRASHOBJECT       3
    #define FOPSERR_TRASHDRIVENOTSUPPORTED      4

    /*
     *@@ FNFOPSERRORCALLBACK:
     *      error callback if problems are encountered
     *      during file processing. If this function
     *      returns TRUE, processing continues. If this
     *      returns FALSE, processing is halted.
     *
     *      ulError will be one of the following:
     *
     *      -- FOPSERR_NODELETETRASHOBJECT: cannot move
     *         XWPTrashObject instances. Shouldn't happen.
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

    typedef BOOL APIENTRY FNFOPSERRORCALLBACK(WPObject *pObject,
                                              ULONG ulError);

    HFILETASKLIST fopsCreateFileTaskList(ULONG ulOperation,
                                         WPFolder *pSourceFolder,
                                         WPFolder *pTargetFolder,
                                         FNFOPSPROGRESSCALLBACK *pfnProgressCallback,
                                         FNFOPSERRORCALLBACK *pfnErrorCallback,
                                         ULONG ulUser);

    BOOL fopsValidateObjOperation(ULONG ulOperation,
                                  FNFOPSERRORCALLBACK *pfnErrorCallback,
                                  WPObject *pObject);

    BOOL fopsAddObjectToTask(HFILETASKLIST hftl,
                             WPObject *pObject);

    BOOL fopsStartProcessingTasks(HFILETASKLIST hftl);

    VOID fopsFileThreadProcessing(HFILETASKLIST hftl);

    BOOL fopsDeleteFileTaskList(HFILETASKLIST hftl);

    /********************************************************************
     *                                                                  *
     *   Generic file operations implementation                         *
     *                                                                  *
     ********************************************************************/

    BOOL fopsStartCnrCommon(ULONG ulOperation,
                            PSZ pszTitle,
                            WPFolder *pSourceFolder,
                            WPFolder *pTargetFolder,
                            WPObject *pSourceObject,
                            ULONG ulSelection,
                            HWND hwndCnr);

    #ifdef LINKLIST_HEADER_INCLUDED
        BOOL fopsStartFromListCommon(ULONG ulOperation,
                                     PSZ pszTitle,
                                     WPFolder *pSourceFolder,
                                     WPFolder *pTargetFolder,
                                     PLINKLIST pllObjects);
    #endif

    /********************************************************************
     *                                                                  *
     *   Trash can file operations implementation                       *
     *                                                                  *
     ********************************************************************/

    BOOL fopsStartCnrDelete2Trash(WPFolder *pSourceFolder,
                                  WPObject *pSourceObject,
                                  ULONG ulSelection,
                                  HWND hwndCnr);

    BOOL fopsStartCnrRestoreFromTrash(WPFolder *pTrashSource,
                                      WPFolder *pTargetFolder,
                                      WPObject *pSourceObject,
                                      ULONG ulSelection,
                                      HWND hwndCnr);

    BOOL fopsStartCnrDestroyTrashObjects(WPFolder *pTrashSource,
                                         WPObject *pSourceObject,
                                         ULONG ulSelection,
                                         HWND hwndCnr);
#endif


