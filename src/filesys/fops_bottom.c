
/*
 *@@sourcefile fops_bottom.c:
 *      this has the bottom layer of the XWorkplace file operations
 *      engine. See fileops.c for an introduction.
 *
 *      The bottom layer is independent of the GUI and uses
 *      callbacks for reporting progress and errors. This
 *      is different from the ugly WPS implementation where
 *      you get message boxes when calling a file operations
 *      method, and there's no way to suppress them. (Try
 *      _wpFree on a read-only file system object.)
 *
 *      Starting a file operation requires several steps:
 *
 *      1.  Create a "file task list" using fopsCreateFileTaskList.
 *          This tells the file operatations engine what to do.
 *          Also, you can specify progress and error callbacks.
 *
 *      2.  Add objects to the list using fopsAddObjectToTask.
 *          This already does preliminary checks whether the
 *          file operation specified with fopsCreateFileTaskList
 *          will work on the respective object. If not, your
 *          error callback gets called and can attempt to fix
 *          the error.
 *
 *      3.  Call fopsStartTask which processes the file task
 *          list on the XWorkplace File thread. You can choose
 *          to have processing done synchronously or asynchronously.
 *          During processing, your progress callback gets called.
 *          If errors occur during processing, your error callback
 *          will get called again, but this time, processing aborts.
 *
 *      The file-operations top layer (fops_top.c) implements proper
 *      callbacks for progress and error reports.
 *
 *      This file has been separated from fileops.c with
 *      V0.9.4 (2000-07-27) [umoeller].
 *
 *      Function prefix for this file:
 *      --  fops*
 *
 *@@added V0.9.4 (2000-07-27) [umoeller]
 *@@header "filesys\fileops.h"
 */

/*
 *      Copyright (C) 2000 Ulrich M�ller.
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

#define INCL_DOSERRORS
#define INCL_DOSSEMAPHORES
#define INCL_DOSEXCEPTIONS
#define INCL_DOSPROCESS
#define INCL_WINWINDOWMGR
#define INCL_WININPUT
#define INCL_WINTIMER
#define INCL_WINPOINTERS
#define INCL_WINDIALOGS
#define INCL_WINBUTTONS
#define INCL_WINENTRYFIELDS
#define INCL_WINSTDCNR
#define INCL_WINSHELLDATA       // Prf* functions
#include <os2.h>

// C library headers
#include <stdio.h>              // needed for except.h
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\linklist.h"           // linked list helper routines

// SOM headers which don't crash with prec. header files
#include "xfobj.ih"

// XWorkplace implementation headers
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel

#include "filesys\fileops.h"            // file operations implementation
#include "filesys\xthreads.h"           // extra XWorkplace threads

// other SOM headers
#pragma hdrstop                         // VAC++ keeps crashing otherwise
#include <wpfolder.h>
#include "xtrash.h"
#include "xtrashobj.h"
#include "filesys\trash.h"              // trash can implementation
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

/*
 *@@ FILETASKLIST:
 *      internal file-task-list structure.
 *      This is what a HFILETASKLIST handle
 *      really points to.
 *
 *      A FILETASKLIST represents a common
 *      job to be performed on one or several
 *      objects. Such a structure is created
 *      by fopsCreateFileTaskList.
 *
 *@@added V0.9.1 (2000-01-27) [umoeller]
 *@@changed V0.9.2 (2000-03-04) [umoeller]: added error callback
 *@@changed V0.9.4 (2000-07-27) [umoeller]: added ulIgnoreSubsequent to ignore further errors
 */

typedef struct _FILETASKLIST
{
    LINKLIST    llObjects;      // list of simple WPObject* pointers to be processed
    ULONG       ulOperation;    // operation; see fopsCreateFileTaskList.

    WPFolder    *pSourceFolder; // source folder for the operation; this gets locked
    BOOL        fSourceLocked;  // TRUE if wpRequestObjectMutexSem has been invoked
                                // on pSourceFolder

    WPFolder    *pTargetFolder; // target folder for the operation
    BOOL        fTargetLocked;  // TRUE if wpRequestObjectMutexSem has been invoked
                                // on pTargetFolder

    POINTL      ptlTarget;      // target coordinates in pTargetFolder

    FNFOPSPROGRESSCALLBACK *pfnProgressCallback;
                    // progress callback specified with fopsCreateFileTaskList
    FNFOPSERRORCALLBACK *pfnErrorCallback;
                    // error callback specified with fopsCreateFileTaskList

    ULONG       ulIgnoreSubsequent;
                    // ignore subsequent errors; this
                    // is set to one of the FOPS_ISQ_* flags
                    // if the error callback chooses to ignore
                    // subsequent errors

    ULONG       ulUser;                 // user parameter specified with fopsCreateFileTaskList
} FILETASKLIST, *PFILETASKLIST;

/*
 *@@ fopsCreateFileTaskList:
 *      creates a new task list for the given file operation
 *      to which items can be added using fopsAddFileTask.
 *      Part of the bottom layer of the XWorkplace file
 *      operations engine. Can be called on any thread.
 *
 *      To start such a file task, three steps are required:
 *
 *      -- call fopsCreateFileTaskList (this function);
 *
 *      -- add objects to that list using fopsAddObjectToTask;
 *
 *      -- pass the task to the File thread using fopsStartTask,
 *         OR delete the list using fopsDeleteFileTaskList.
 *
 *      WARNING: This function requests the object mutex semaphore
 *      for pSourceFolder. You must add objects to the returned
 *      task list (using fopsAddFileTask) as quickly as possible.
 *
 *      The mutex will only be released after all file processing
 *      has been completed (thru fopsStartTask) or if you call
 *      fopsDeleteFileTaskList explicitly. So you MUST call either
 *      one of those two functions after a task list was successfully
 *      created using this function.
 *
 *      Returns NULL upon errors, e.g. because a folder
 *      mutex could not be accessed.
 *
 *      This is usually called on the thread of the folder view
 *      from which the file operation was started (mostly thread 1).
 *
 *      The following are supported for ulOperation:
 *
 *      -- XFT_MOVE2TRASHCAN: move list items into trash can.
 *          In that case, pSourceFolder has the source folder.
 *          This isn't really needed for processing, but the
 *          progress dialog should display this, so to speed
 *          up processing, pass it with this call.
 *          pTargetFolder is ignored because the default trash
 *          can is determined automatically.
 *
 *      -- XFT_RESTOREFROMTRASHCAN: restore objects from trash can.
 *          In that case, pSourceFolder has the trash can to
 *          restore from (XWPTrashCan*).
 *          If (pTargetFolder == NULL), the objects will be restored
 *          to the trash object's original location.
 *          If (pTargetFolder != NULL), the objects will all be moved
 *          to that folder.
 *
 *      -- XFT_DELETE: delete list items from pSourceFolder without
 *          moving them into the trash can first.
 *          In that case, pTargetFolder is ignored.
 *
 *      -- XFT_POPULATE: populate folders on list. pSourcefolder
 *          and pTargetFolder are ignored. Also you must specify
 *          both callbacks as NULL.
 *          fopsAddObjectToTask may only be used with folder objects.
 *          This operation is useful to populate folders synchronously
 *          without blocking the thread's message queue since
 *          fopsStartTask can avoid that.
 *
 *@@added V0.9.1 (2000-01-27) [umoeller]
 *@@changed V0.9.2 (2000-03-04) [umoeller]: added error callback
 *@@changed V0.9.4 (2000-08-03) [umoeller]: added XFT_POPULATE
 */

HFILETASKLIST fopsCreateFileTaskList(ULONG ulOperation,     // in: XFT_* flag
                                     WPFolder *pSourceFolder,
                                     WPFolder *pTargetFolder,
                                     FNFOPSPROGRESSCALLBACK *pfnProgressCallback, // in: callback procedure
                                     FNFOPSERRORCALLBACK *pfnErrorCallback, // in: error callback
                                     ULONG ulUser)          // in: user parameter passed to callback
{
    BOOL    fSourceLocked = FALSE,
            fProceed = FALSE;

    if (pSourceFolder)
    {
        fSourceLocked = !_wpRequestObjectMutexSem(pSourceFolder, 5000);
        if (fSourceLocked)
            fProceed = TRUE;
    }
    else
        fProceed = TRUE;

    if (fProceed)
    {
        PFILETASKLIST pftl = malloc(sizeof(FILETASKLIST));
        memset(pftl, 0, sizeof(FILETASKLIST));
        lstInit(&pftl->llObjects, FALSE);     // no freeing, this stores WPObject pointers!
        pftl->ulOperation = ulOperation;
        pftl->pSourceFolder = pSourceFolder;
        pftl->fSourceLocked = fSourceLocked;
        pftl->pTargetFolder = pTargetFolder;
        pftl->pfnProgressCallback = pfnProgressCallback;
        pftl->pfnErrorCallback = pfnErrorCallback;
        pftl->ulUser = ulUser;
        return ((HFILETASKLIST)pftl);
                // do not unlock pSourceFolder!
    }

    // error
    if (fSourceLocked)
        _wpReleaseObjectMutexSem(pSourceFolder);

    return (NULLHANDLE);
}

/*
 *@@ fopsValidateObjOperation:
 *      returns 0 (FOPSERR_OK) only if ulOperation is valid
 *      on pObject.
 *      Part of the bottom layer of the XWorkplace file
 *      operations engine.
 *
 *      This must be run on the thread which called fopsCreateFileTaskList.
 *
 *      If an error has been found and (pfnErrorCallback != NULL),
 *      that callback gets called and its return value is returned.
 *      This means that pfnErrorCallback can attempt to fix the error.
 *      If the callback is NULL, some meaningfull error gets
 *      returned if the operation is invalid, but there's no chance
 *      to fix it.
 *
 *      This is usually called from fopsAddObjectToTask on the thread
 *      of the folder view from which the file operation was started
 *      (mostly thread 1) while objects are added to a file task list.
 *
 *      This can also be called at any time. For example, the trash can
 *      uses this from XWPTrashCan::wpDragOver to validate the objects
 *      being dragged.
 *
 *@@added V0.9.1 (2000-02-01) [umoeller]
 *@@changed V0.9.2 (2000-03-04) [umoeller]: added error callback
 *@@changed V0.9.3 (2000-04-25) [umoeller]: reworked error management
 *@@changed V0.9.4 (2000-07-27) [umoeller]: added pulIgnoreSubsequent to ignore further errors
 */

FOPSRET fopsValidateObjOperation(ULONG ulOperation,        // in: operation
                                 FNFOPSERRORCALLBACK *pfnErrorCallback,
                                                           // in: error callback or NULL
                                 WPObject *pObject,        // in: current object to check
                                 PULONG pulIgnoreSubsequent)
                                    // in: ignore subsequent errors of the same type;
                                    // ULONG must be 0 on the first call; FOPS_ISQ_*
                                    // flags can be set by the error callback.
                                    // The ptr can only be NULL if pfnErrorCallback is
                                    // also NULL.
{
    FOPSRET frc = FOPSERR_OK;
    BOOL    fPromptUser = TRUE;

    // error checking
    switch (ulOperation)
    {
        case XFT_MOVE2TRASHCAN:
            if (!_wpIsDeleteable(pObject))
            {
                // abort right away
                frc = FOPSERR_MOVE2TRASH_NOT_DELETABLE;
                fPromptUser = FALSE;
            }
            else if (!trshIsOnSupportedDrive(pObject))
                frc = FOPSERR_TRASHDRIVENOTSUPPORTED;
        break;

        case XFT_TRUEDELETE:
            if (!_wpIsDeleteable(pObject))
                frc = FOPSERR_DELETE_NOT_DELETABLE;
        break;

        case XFT_POPULATE:
            // adding objects to populate jobs is not
            // supported
            if (!_somIsA(pObject, _WPFolder))
                frc = FOPSERR_POPULATE_FOLDERS_ONLY;
        break;
    }

    if (frc != FOPSERR_OK)
        if ((pfnErrorCallback) && (fPromptUser))
            // prompt user:
            // this can recover
            frc = (pfnErrorCallback)(ulOperation, pObject, frc, pulIgnoreSubsequent);
        // else: return error code

    return (frc);
}

/*
 *@@ fopsAddFileTask:
 *      adds an object to be processed to the given
 *      file task list.
 *      Part of the bottom layer of the XWorkplace file
 *      operations engine.
 *
 *      This must be run on the thread which called fopsCreateFileTaskList.
 *
 *      Note: This function is reentrant only for the
 *      same HFILETASKLIST. Once fopsStartTask
 *      has been called for the task list, you must not
 *      modify the task list again.
 *      There's no automatic protection against this!
 *      So call this function only AFTER fopsCreateFileTaskList
 *      and BEFORE fopsStartTask.
 *
 *      This calls fopsValidateObjOperation on the object and
 *      returns the error code of fopsValidateObjOperation.
 *
 *@@added V0.9.1 (2000-01-26) [umoeller]
 *@@changed V0.9.2 (2000-03-04) [umoeller]: added error callback
 *@@changed V0.9.3 (2000-04-25) [umoeller]: reworked error management
 */

FOPSRET fopsAddObjectToTask(HFILETASKLIST hftl,      // in: file-task-list handle
                            WPObject *pObject)
{
    FOPSRET frc = FOPSERR_OK;
    PFILETASKLIST pftl = (PFILETASKLIST)hftl;

    frc = fopsValidateObjOperation(pftl->ulOperation,
                                   pftl->pfnErrorCallback,
                                   pObject,
                                   &pftl->ulIgnoreSubsequent);

    if (frc == FOPSERR_OK)
        // proceed:
        if (lstAppendItem(&pftl->llObjects, pObject) == NULL)
            // error:
            frc = FOPSERR_INTEGRITY_ABORT;

    return (frc);
}

/*
 *@@ fopsStartTask:
 *      this starts processing the specified file task list.
 *
 *      Part of the bottom layer of the XWorkplace file
 *      operations engine.
 *
 *      This must be run on the thread which called fopsCreateFileTaskList.
 *
 *      Further processing is done on the File thread
 *      (xthreads.c) in order not to block the user
 *      interface.
 *
 *      This function can operate in two modes:
 *
 *      -- If (hab == NULLHANDLE), this function returns
 *         almost immediately while processing continues
 *         on the File thread (asynchronous mode).
 *
 *         DO NOT WORK on the task list passed to this
 *         function once this function has been called.
 *         The File thread uses it and destroys it
 *         automatically when done.
 *
 *         There's no way to get the return code of the
 *         actual file processing in asynchronous mode.
 *         This function will return 0 (FOPSERR_OK) if
 *         the task has started, even if it will later fail
 *         on the File thread.
 *
 *      -- By contrast, if you specify the thread's anchor block
 *         in hab, this function waits for the processing to
 *         complete and returns an error code. This still uses
 *         the File thread, but during processing the current
 *         thread's message queue is processed modally (similar to
 *         what WinMessageBox does) so the system is not blocked
 *         during the operation.
 *
 *         This only works if you specify the HAB on which the
 *         thread is running. Of course, this implies that the
 *         thread has a message queue.
 *
 *@@added V0.9.1 (2000-01-27) [umoeller]
 *@@changed V0.9.3 (2000-04-25) [umoeller]: renamed from fopsStartProcessingTasks
 *@@changed V0.9.4 (2000-08-03) [umoeller]: added synchronous processing support; prototype changed
 */

FOPSRET fopsStartTask(HFILETASKLIST hftl,
                      HAB hab)             // in: if != NULLHANDLE, synchronous operation
{
    FOPSRET frc = NO_ERROR;
    PFILETASKLIST pftl = (PFILETASKLIST)hftl;
    HWND    hwndNotify = NULLHANDLE;
    // unlock the folders so the file thread
    // can start working
    if (pftl->fSourceLocked)
    {
        _wpReleaseObjectMutexSem(pftl->pSourceFolder);
        pftl->fSourceLocked = FALSE;
    }

    if (hab)
        // synchronous mode: create object window on the
        // running thread and pass that object window
        // (hwndNotify) to the File thread. We then
        // enter a modal message loop below, waiting
        // for T1M_FOPS_TASK_DONE to be posted by the
        // File thread.
        if (WinRegisterClass(hab,
                             "XWPFileOperationsNotify",
                             WinDefWindowProc,
                             0,
                             0))
        {
            hwndNotify = WinCreateWindow(HWND_OBJECT,
                                         "XWPFileOperationsNotify",
                                         (PSZ)"",
                                         0,
                                         0,0,0,0,
                                         0,
                                         HWND_BOTTOM,
                                         0,
                                         0,
                                         NULL);
            if (!hwndNotify)
                frc = FOPSERR_START_FAILED;
        }

    if (frc == NO_ERROR)
    {
        // have File thread process this list;
        // this calls fopsFileThreadProcessing below
        if (!xthrPostFileMsg(FIM_PROCESSTASKLIST,
                             (MPARAM)hftl,
                             (MPARAM)hwndNotify))
            frc = FOPSERR_START_FAILED;
        else
            if (hab)
            {
                // synchronous mode: enter modal message loop
                // for the window created above.
                // fopsFileThreadProcessing (on the File thread)
                // posts T1M_FOPS_TASK_DONE to that window, so
                // we examine each incoming message.
                QMSG qmsg;
                BOOL fQuit = FALSE;
                while (WinGetMsg(hab,
                                 &qmsg, 0, 0, 0))
                {
                    // current message for our object window?
                    if (qmsg.hwnd == hwndNotify)
                        // yes: "done" message?
                        if (qmsg.msg == T1M_FOPS_TASK_DONE)
                            // yes: check if it's our handle
                            // (for security)
                            if (qmsg.mp1 == pftl)
                            {
                                fQuit = TRUE;
                                // mp2 has FOPSRET return code
                                frc = (ULONG)qmsg.mp2;
                            }
                    WinDispatchMsg(hab, &qmsg);
                    if (fQuit)
                        break;
                }
            }
    }
    else
        // error:
        fopsDeleteFileTaskList(hftl);

    if (hwndNotify)
        WinDestroyWindow(hwndNotify);

    return (frc);
}

/********************************************************************
 *                                                                  *
 *   File thread processing                                         *
 *                                                                  *
 ********************************************************************/

/*
 *@@ fopsCallProgressCallback:
 *      Part of the bottom layer of the XWorkplace file
 *      operations engine.
 *
 *      Returns FALSE to abort.
 *
 *@@added V0.9.2 (2000-03-30) [cbo]:
 */

FOPSRET fopsCallProgressCallback(PFILETASKLIST pftl,
                                 ULONG flChanged,
                                 BOOL fUpdateProgress,
                                 FOPSUPDATE *pfu)   // in: update structure in fopsFileThreadProcessing;
                                                    // NULL means done
{
    FOPSRET frc = FOPSERR_OK;
    BOOL    fFirstCall = ((pfu->flProgress & FOPSPROG_FIRSTCALL) != 0);

    // store changed flags
    pfu->flChanged = flChanged;

    // set "progress changed" flag
    if (fUpdateProgress)
        pfu->flProgress |= FOPSPROG_UPDATE_PROGRESS;
    else
        pfu->flProgress &= ~FOPSPROG_UPDATE_PROGRESS;

    // set source and target folders
    // depending on operation
    switch (pftl->ulOperation)
    {
        case XFT_MOVE2TRASHCAN:
            if (fFirstCall)
            {
                // update source and target;
                // the target has been set to the trash can before
                // the first call by fopsFileThreadProcessing
                pfu->flChanged |= (FOPSUPD_SOURCEFOLDER_CHANGED
                                        | FOPSUPD_TARGETFOLDER_CHANGED);
            }
        break;

        case XFT_RESTOREFROMTRASHCAN:
            if (fFirstCall)
                // source is trash can, which is needed only for
                // the first call
                pfu->flChanged |= FOPSUPD_SOURCEFOLDER_CHANGED;

            // target folder can change with any call
            pfu->flChanged |= FOPSUPD_TARGETFOLDER_CHANGED;
        break;

        case XFT_TRUEDELETE:
            if (fFirstCall)
                // update source on first call; there is
                // no target folder
                pfu->flChanged |= FOPSUPD_SOURCEFOLDER_CHANGED;
        break;

    }

    if (pftl->pfnProgressCallback)
        if (!((pftl->pfnProgressCallback)(pfu,
                                          pftl->ulUser)))
            // FALSE means abort:
            frc = FOPSERR_CANCELLEDBYUSER;

    if (pfu)
        // unset first-call flag, which
        // was initially set
        pfu->flProgress &= ~FOPSPROG_FIRSTCALL;

    return (frc);
}

/*
 *@@ fopsFileThreadTrueDelete:
 *      gets called from fopsFileThreadProcessing with
 *      XFT_TRUEDELETE for every object on the list.
 *      Part of the bottom layer of the XWorkplace file
 *      operations engine.
 *
 *      This checks if pObject is a folder and will
 *      recurse, if necessary.
 *      This finally allows cancelling a "delete"
 *      operation when a subfolder is currently
 *      processed.
 *
 *@@added V0.9.3 (2000-04-27) [umoeller]
 *@@changed V0.9.3 (2000-04-30) [umoeller]: reworked progress reports
 *@@changed V0.9.4 (2000-07-27) [umoeller]: added ulIgnoreSubsequent to ignore further errors
 */

FOPSRET fopsFileThreadTrueDelete(HFILETASKLIST hftl,
                                 FOPSUPDATE *pfu,       // in: update structure in fopsFileThreadProcessing
                                 WPObject **ppObject,   // in: object to delete;
                                                        // out: failing object if error
                                 PULONG pulIgnoreSubsequent) // in: ignore subsequent errors of the same type
{
    FOPSRET frc = FOPSERR_OK;
    PFILETASKLIST pftl = (PFILETASKLIST)hftl;

    // list of WPObject* pointers
    LINKLIST llObjects;
    lstInit(&llObjects,
             FALSE);    // no free, we have WPObject* pointers

    // say "collecting objects"
    // pfu->pSourceObject = *ppObject;      // callback has already been called
                                            // for this one
    frc = fopsCallProgressCallback(pftl,
                                   FOPSUPD_EXPANDING_SOURCEOBJECT_1ST,
                                   FALSE,    // update progress
                                   pfu);

    if (frc == FOPSERR_OK)
    {
        // build list of all objects to be deleted;
        // this can take a loooong time
        ULONG ulSubObjectsCount = 0;
        frc = fopsExpandObjectFlat(&llObjects,
                                   *ppObject,
                                   &ulSubObjectsCount);
                // now ulSubObjectsCount has the no. of objects

        if (frc == FOPSERR_OK)
            // "done collecting objects"
            frc = fopsCallProgressCallback(pftl,
                                           FOPSUPD_EXPANDING_SOURCEOBJECT_DONE,
                                           FALSE,    // update progress
                                           pfu);

        if (    (frc == FOPSERR_OK)
             && (ulSubObjectsCount) // avoid division by zero below
           )
        {
            ULONG ulSubObjectThis = 0,
                  // save progress scalar
                  ulProgressScalarFirst = pfu->ulProgressScalar;

            // all sub-objects collected:
            // go thru the whole list and start deleting.
            // Note that folder contents come before the
            // folder ("bottom-up" directory list).
            PLISTNODE pNode = lstQueryFirstNode(&llObjects);
            while (pNode)
            {
                WPObject *pObjThis = (WPObject*)pNode->pItemData;

                // call callback for subobject
                pfu->pSubObject = pObjThis;
                // calc new sub-progress: this is the value we first
                // had before working on the subobjects (which
                // is a multiple of 100) plus a sub-progress between
                // 0 and 100 for the subobjects
                pfu->ulProgressScalar = ulProgressScalarFirst
                                        + ((ulSubObjectThis * 100 )
                                             / ulSubObjectsCount);
                frc = fopsCallProgressCallback(pftl,
                                               FOPSUPD_SUBOBJECT_CHANGED,
                                               TRUE, // update progress
                                               pfu);
                if (frc)
                {
                    // error or cancelled:
                    *ppObject = pObjThis;
                    break;
                }

                if (!_wpIsDeleteable(pObjThis))
                {
                    BOOL fIsFileSystem = _somIsA(pObjThis, _WPFileSystem);
                    if (fIsFileSystem)
                    {
                        if (    (*pulIgnoreSubsequent & FOPS_ISQ_DELETE_READONLY)
                             == 0)
                            // first error, or user has not selected
                            // to abort subsequent:
                            frc = FOPSERR_DELETE_READONLY;      // non-fatal
                        else
                            // user has chosen to ignore subsequent:
                            // this will call wpSetAttr below
                            frc = NO_ERROR;
                    }
                    else
                        frc = FOPSERR_DELETE_NOT_DELETABLE; // fatal

                    if (frc != NO_ERROR)
                        if (pftl->pfnErrorCallback)
                            // prompt user:
                            // this can recover, but should have
                            // changed the problem (read-only)
                            frc = (pftl->pfnErrorCallback)(pftl->ulOperation,
                                                           pObjThis,
                                                           frc,
                                                           pulIgnoreSubsequent);
                    if (frc == NO_ERROR)
                        if (fIsFileSystem)
                            _wpSetAttr(pObjThis, FILE_NORMAL);

                }

                if (frc == FOPSERR_OK)
                {
                    // either no problem or problem fixed:
                    if (!_wpFree(pObjThis))
                    {
                        frc = FOPSERR_WPFREE_FAILED;
                        *ppObject = pObjThis;
                        break;
                    }
                }
                else
                {
                    // error:
                    *ppObject = pObjThis;
                    break;
                }

                pNode = pNode->pNext;
                ulSubObjectThis++;
            }

            // done with subobjects: report NULL subobject
            if (frc == FOPSERR_OK)
            {
                pfu->pSubObject = NULL;
                frc = fopsCallProgressCallback(pftl,
                                               FOPSUPD_SUBOBJECT_CHANGED,
                                               FALSE, // no update progress
                                               pfu);
            }
        }
    }

    lstClear(&llObjects);

    return (frc);
}

/*
 *@@ fopsFileThreadProcessing:
 *      this actually performs the file operations
 *      on the objects which have been added to
 *      the given file task list.
 *      Part of the bottom layer of the XWorkplace file
 *      operations engine.
 *
 *      WARNING: NEVER call this function manually.
 *      This gets called on the File thread (xthreads.c)
 *      automatically  after fopsStartTask has
 *      been called.
 *
 *      This runs on the File thread.
 *
 *@@added V0.9.1 (2000-01-29) [umoeller]
 *@@changed V0.9.3 (2000-04-25) [umoeller]: reworked error management
 *@@changed V0.9.3 (2000-04-26) [umoeller]: added "true delete" support
 *@@changed V0.9.3 (2000-04-30) [umoeller]: reworked progress reports
 *@@changed V0.9.4 (2000-08-03) [umoeller]: added synchronous processing support
 */

VOID fopsFileThreadProcessing(HFILETASKLIST hftl,
                              HWND hwndNotify)      // in: if != NULLHANDLE, post this
                                                    // window a T1M_FOPS_TASK_DONE msg
{
    BOOL frc = FOPSERR_OK;
    PFILETASKLIST pftl = (PFILETASKLIST)hftl;

    #ifdef DEBUG_TRASHCAN
        _Pmpf(("fopsFileThreadProcessing hftl: 0x%lX", hftl));
    #endif

    TRY_LOUD(excpt2, NULL)
    {
        if (pftl)
        {
            /*
             * 1) preparations before collecting objects
             *
             */

            // objects count
            ULONG ulCurrentObject = 0;

            // ignore subsequent errors; this
            // is set to one of the FOPS_ISQ_* flags
            // if the error callback chooses to ignore
            // subsequent errors
            ULONG ulIgnoreSubsequent = 0;

            // get first node
            PLISTNODE   pNode = lstQueryFirstNode(&pftl->llObjects);
            FOPSUPDATE  fu;
            memset(&fu, 0, sizeof(fu));

            switch (pftl->ulOperation)
            {
                case XFT_MOVE2TRASHCAN:
                {
                    // move to trash can:
                    // in that case, pTargetFolder is NULL,
                    // so query the target folder first
                    pftl->pTargetFolder = _xwpclsQueryDefaultTrashCan(_XWPTrashCan);

                    #ifdef DEBUG_TRASHCAN
                        _Pmpf(("  target trash can is %s", _wpQueryTitle(pftl->pTargetFolder) ));
                    #endif
                break; }

                case XFT_TRUEDELETE:
                {
                    // true delete (destroy objects):
                break; }
            }

            fu.ulOperation = pftl->ulOperation;
            fu.pSourceFolder = pftl->pSourceFolder;
            fu.pTargetFolder = pftl->pTargetFolder;
            fu.flProgress = FOPSPROG_FIRSTCALL;   // unset by CallProgressCallback after first call
            fu.ulProgressScalar = 0;
            fu.ulProgressMax = lstCountItems(&pftl->llObjects) * 100;

            #ifdef DEBUG_TRASHCAN
                _Pmpf(("    %d items on list", fu.ulProgressMax / 100));
            #endif

            /*
             * 2) process objects on list
             *
             */

            while ((pNode) && (frc == NO_ERROR))
            {
                WPObject    *pObjectThis = (WPObject*)pNode->pItemData;

                #ifdef DEBUG_TRASHCAN
                    _Pmpf(("    checking object 0x%lX", pObjectThis));
                #endif

                // check if object is still valid
                if (!wpshCheckObject(pObjectThis))
                {
                    frc = FOPSERR_INVALID_OBJECT;
                    break;
                }
                else
                {
                    // call progress callback with this object
                    if (pftl->pfnProgressCallback)
                    {
                        fu.pSourceObject = pObjectThis;
                        frc = fopsCallProgressCallback(pftl,
                                                       FOPSUPD_SOURCEOBJECT_CHANGED,
                                                       TRUE, // update progress
                                                       &fu);
                        if (frc)
                            // error or cancelled:
                            break;
                    }
                }

                #ifdef DEBUG_TRASHCAN
                    _Pmpf(("    processing %s", _wpQueryTitle(pObjectThis) ));
                #endif

                // now, for each object, perform
                // the desired operation
                switch (pftl->ulOperation)
                {
                    /*
                     * XFT_MOVE2TRASHCAN:
                     *
                     */

                    case XFT_MOVE2TRASHCAN:
                        #ifdef DEBUG_TRASHCAN
                            _Pmpf(("  fopsFileThreadProcessing: trashmove %s",
                                        _wpQueryTitle(pObjectThis) ));
                        #endif
                        if (!_xwpDeleteIntoTrashCan(pftl->pTargetFolder, // default trash can
                                                    pObjectThis))
                            frc = FOPSERR_NOT_HANDLED_ABORT;
                    break;

                    /*
                     * XFT_RESTOREFROMTRASHCAN:
                     *
                     */

                    case XFT_RESTOREFROMTRASHCAN:
                        #ifdef DEBUG_TRASHCAN
                            _Pmpf(("  fopsFileThreadProcessing: restoring %s",
                                    _wpQueryTitle(pObjectThis) ));
                        #endif
                        if (!_xwpRestoreFromTrashCan(pObjectThis,       // trash object
                                                     pftl->pTargetFolder)) // can be NULL
                            frc = FOPSERR_NOT_HANDLED_ABORT;
                        // after this pObjectThis is invalid!
                    break;

                    /*
                     * XFT_TRUEDELETE:
                     *
                     */

                    case XFT_TRUEDELETE:
                        #ifdef DEBUG_TRASHCAN
                            _Pmpf(("  fopsFileThreadProcessing: wpFree'ing %s",
                                    _wpQueryTitle(pObjectThis) ));
                        #endif
                        frc = fopsFileThreadTrueDelete(hftl,
                                                       &fu,
                                                       &pObjectThis,
                                                       &ulIgnoreSubsequent);
                                           // after this pObjectThis is invalid!
                    break;

                    /*
                     * XFT_POPULATE:
                     *
                     */

                    case XFT_POPULATE:
                        if (!wpshCheckIfPopulated(pObjectThis))
                            frc = FOPSERR_POPULATE_FAILED;
                    break;
                }

                /* #ifdef __DEBUG__
                    if (frc != FOPSERR_OK)
                    {
                        CHAR szMsg[3000];
                        PSZ pszTitle = "?";
                        if (wpshCheckObject(pObjectThis))
                            pszTitle = _wpQueryTitle(pObjectThis);
                        sprintf(szMsg,
                                "Error %d with object %s",
                                frc,
                                pszTitle);
                        DebugBox(NULLHANDLE,
                                 "XWorkplace File thread",
                                 szMsg);
                    }
                #endif */

                pNode = pNode->pNext;
                ulCurrentObject++;
                fu.ulProgressScalar = ulCurrentObject * 100; // for progress
            } // end while (pNode)

            // call progress callback to say "done"
            fu.flProgress = FOPSPROG_LASTCALL_DONE;
            fopsCallProgressCallback(pftl,
                                     0,         // no update flags, just "done"
                                     TRUE,      // update progress
                                     &fu);      // NULL means done
        } // end if (pftl)

        // even with errors, delete file task list now
        fopsDeleteFileTaskList(hftl);
                // this unlocks the folders also
    }
    CATCH(excpt2)
    {
        frc == FOPSERR_FILE_THREAD_CRASHED;
    } END_CATCH();

    if (hwndNotify)
        // synchronous mode:
        // post msg to thread-1 object window;
        // this leaves the modal message loop in fopsStartTask
        WinPostMsg(hwndNotify,
                   T1M_FOPS_TASK_DONE,
                   pftl,            // ptr no longer valid, but used
                                    // as identifier
                   (MPARAM)frc);
}

/*
 *@@ fopsDeleteTaskList:
 *      frees all resources allocated with
 *      the given file-task list and unlocks
 *      the folders involved in the operation.
 *
 *      Note: You can call this manually to
 *      delete a file-task list which you have
 *      created using fopsCreateFileTaskList,
 *      but you MUST call this function on the
 *      same thread which called fopsCreateFileTaskList,
 *      or the WPS will probably hang itself up.
 *
 *      Do NOT call this function after you have
 *      called fopsStartTask because
 *      then the File thread is working on the
 *      list already. The file list will automatically
 *      get cleaned up then, so you only need to
 *      call this function yourself if you choose
 *      NOT to call fopsStartTask.
 *
 *@@added V0.9.1 (2000-01-29) [umoeller]
 */

BOOL fopsDeleteFileTaskList(HFILETASKLIST hftl)
{
    BOOL brc = TRUE;
    PFILETASKLIST pftl = (PFILETASKLIST)hftl;
    if (pftl)
    {
        if (pftl->fTargetLocked)
        {
            _wpReleaseObjectMutexSem(pftl->pTargetFolder);
            pftl->fTargetLocked = FALSE;
        }
        if (pftl->fSourceLocked)
        {
            _wpReleaseObjectMutexSem(pftl->pSourceFolder);
            pftl->fSourceLocked = FALSE;
        }
        lstClear(&pftl->llObjects);       // frees items automatically
        free(pftl);
    }
    return (brc);
}

