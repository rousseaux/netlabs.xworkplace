
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
 *      Copyright (C) 2000 Ulrich M”ller.
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
#include "xfont.ih"
#include "xfontfile.ih"
#include "xfontobj.ih"
#include "xtrash.ih"
#include "xtrashobj.ih"

// XWorkplace implementation headers
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

#include "config\fonts.h"               // font folder implementation

#include "filesys\fileops.h"            // file operations implementation
#include "filesys\trash.h"              // trash can implementation
#include "filesys\xthreads.h"           // extra XWorkplace threads

// other SOM headers
#pragma hdrstop                         // VAC++ keeps crashing otherwise
#include <wpfolder.h>

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
    BOOL        fSourceLocked;  // TRUE if wpRequestFolderMutexSem has been invoked
                                // on pSourceFolder

    WPFolder    *pTargetFolder; // target folder for the operation
    BOOL        fTargetLocked;  // TRUE if wpRequestFolderMutexSem has been invoked
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
 *      WARNING: This function requests the folder mutex semaphore
 *      for pSourceFolder. (While this semaphore is held, no other
 *      thread can add or remove objects from the folder.) You must
 *      add objects to the returned task list (using fopsAddFileTask)
 *      as quickly as possible.
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
 *          This expects trash objects (XWPTrashObject) on the list.
 *          In that case, pSourceFolder has the trash can to
 *          restore from (XWPTrashCan*).
 *          If (pTargetFolder == NULL), the objects will be restored
 *          to the respective locations where they were deleted from.
 *          If (pTargetFolder != NULL), the objects will all be moved
 *          to that folder.
 *
 *      -- XFT_DELETE: delete list items from pSourceFolder without
 *          moving them into the trash can first.
 *          In that case, pTargetFolder is ignored.
 *
 *          This is used with "true delete" (shift and "delete"
 *          context menu item). Besides, this is used to empty the
 *          trash can and to destroy trash objects; in these cases,
 *          this function assumes to be operating on the related
 *          objects, not the trash objects.
 *
 *      -- XFT_POPULATE: populate folders on list. pSourcefolder
 *          and pTargetFolder are ignored. Also you must specify
 *          both callbacks as NULL.
 *          fopsAddObjectToTask may only be used with folder objects.
 *          This operation is useful to populate folders synchronously
 *          without blocking the thread's message queue since
 *          fopsStartTask can avoid that.
 *
 *      -- XFT_INSTALLFONTS: install font files as PM fonts.
 *          pSourceFolder is ignored.
 *          pTargetFolder must be the default font folder.
 *          This expects font files (instances of XWPFontFile)
 *          on the list.
 *
 *      -- XFT_DEINSTALLFONTS: deinstall font objects.
 *          pSourceFolder should point to the XWPFontFolder containing
 *          the font objects.
 *          pTargetFolder is ignored.
 *          This expects font _objects_ (instances of XWPFontObject)
 *          on the list.
 *
 *@@added V0.9.1 (2000-01-27) [umoeller]
 *@@changed V0.9.2 (2000-03-04) [umoeller]: added error callback
 *@@changed V0.9.4 (2000-08-03) [umoeller]: added XFT_POPULATE
 *@@changed V0.9.7 (2001-01-13) [umoeller]: added XFT_INSTALLFONTS, XFT_DEINSTALLFONTS
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
        fSourceLocked = !wpshRequestFolderMutexSem(pSourceFolder, 5000);
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
        wpshReleaseFolderMutexSem(pSourceFolder);

    return (NULLHANDLE);
}

/*
 *@@ fopsValidateObjOperation:
 *      returns 0 (NO_ERROR) only if ulOperation is valid
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
    FOPSRET frc = NO_ERROR;
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

        case XFT_INSTALLFONTS:
            if (!_somIsA(pObject, _XWPFontFile))
                frc = FOPSERR_NOT_FONT_FILE;
        break;

        case XFT_DEINSTALLFONTS:
            if (!_somIsA(pObject, _XWPFontObject))
                frc = FOPSERR_NOT_FONT_OBJECT;
        break;
    }

    if (frc != NO_ERROR)
        if ((pfnErrorCallback) && (fPromptUser))
            // prompt user:
            // this can recover
            frc = pfnErrorCallback(ulOperation, pObject, frc, pulIgnoreSubsequent);
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
    FOPSRET frc = NO_ERROR;
    PFILETASKLIST pftl = (PFILETASKLIST)hftl;

    frc = fopsValidateObjOperation(pftl->ulOperation,
                                   pftl->pfnErrorCallback,
                                   pObject,
                                   &pftl->ulIgnoreSubsequent);

    if (frc == NO_ERROR)
        // proceed:
        if (!lstAppendItem(&pftl->llObjects, pObject))
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
 *         This function will return 0 (NO_ERROR) if
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
        wpshReleaseFolderMutexSem(pftl->pSourceFolder);
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
 *      this gets called from fopsFileThreadProcessing
 *      every time the progress callback needs an update.
 *
 *      This is slightly complex. See fileops.h for the
 *      various FOPSUPD_* flags which can come in here.
 *
 *      Part of the bottom layer of the XWorkplace file
 *      operations engine.
 *
 *      Returns FALSE to abort.
 *
 *@@added V0.9.2 (2000-03-30) [cbo]:
 */

FOPSRET fopsCallProgressCallback(PFILETASKLIST pftl,
                                 ULONG flChanged,
                                 FOPSUPDATE *pfu)   // in: update structure in fopsFileThreadProcessing;
                                                    // NULL means done altogether
{
    FOPSRET frc = NO_ERROR;

    // store changed flags
    pfu->flChanged = flChanged;

    // set source and target folders
    // depending on operation
    switch (pftl->ulOperation)
    {
        case XFT_MOVE2TRASHCAN:
            if (pfu->fFirstCall)
            {
                // update source and target;
                // the target has been set to the trash can before
                // the first call by fopsFileThreadProcessing
                pfu->flChanged |= (FOPSUPD_SOURCEFOLDER_CHANGED
                                        | FOPSUPD_TARGETFOLDER_CHANGED);
            }
        break;

        case XFT_RESTOREFROMTRASHCAN:
            if (pfu->fFirstCall)
                // source is trash can, which is needed only for
                // the first call
                pfu->flChanged |= FOPSUPD_SOURCEFOLDER_CHANGED;

            // target folder can change with any call
            pfu->flChanged |= FOPSUPD_TARGETFOLDER_CHANGED;
        break;

        case XFT_TRUEDELETE:
            if (pfu->fFirstCall)
                // update source on first call; there is
                // no target folder
                pfu->flChanged |= FOPSUPD_SOURCEFOLDER_CHANGED;
        break;

    }

    if (pftl->pfnProgressCallback)
        if (!(pftl->pfnProgressCallback(pfu,
                                        pftl->ulUser)))
            // FALSE means abort:
            frc = FOPSERR_CANCELLEDBYUSER;

    if (pfu)
        // unset first-call flag, which
        // was initially set
        pfu->fFirstCall = FALSE;

    return (frc);
}

/*
 *@@ fopsFileThreadFixNonDeletable:
 *      called during "true delete" processing when
 *      an object is encountered which is non-deletable.
 *
 *      It is the responsibility of this function to
 *      prompt the user about what to do and fix the
 *      object to be deletable, if the user wants this.
 *
 *@@added V0.9.6 (2000-10-25) [umoeller]
 */

FOPSRET fopsFileThreadFixNonDeletable(PFILETASKLIST pftl,
                                      WPObject *pSubObjThis, // in: obj to fix
                                      PULONG pulIgnoreSubsequent) // in: ignore subsequent errors of the same type
{
    FOPSRET frc = NO_ERROR;

    if (_somIsA(pSubObjThis, _WPFileSystem))
    {
        if ( 0 == ((*pulIgnoreSubsequent) & FOPS_ISQ_DELETE_READONLY) )
            // first error, or user has not selected
            // to abort subsequent:
            frc = FOPSERR_DELETE_READONLY;      // non-fatal
        else
            // user has chosen to ignore subsequent:
            // this will call wpSetAttr below
            frc = NO_ERROR;
            _Pmpf(("      frc = %d", frc));
    }
    else
        frc = FOPSERR_DELETE_NOT_DELETABLE; // fatal

    if (frc != NO_ERROR)
        if (pftl->pfnErrorCallback)
        {
            // prompt user:
            // this can recover, but should have
            // changed the problem (read-only)
            _Pmpf(("      calling error callback"));
            frc = pftl->pfnErrorCallback(pftl->ulOperation,
                                         pSubObjThis,
                                         frc,
                                         pulIgnoreSubsequent);
        }

    if (frc == NO_ERROR)
        if (_somIsA(pSubObjThis, _WPFileSystem))
            if (!_wpSetAttr(pSubObjThis, FILE_NORMAL))
                frc = FOPSERR_WPSETATTR_FAILED;

    return (frc);
}

/*
 *@@ fopsFileThreadSneakyDeleteFolderContents:
 *      this gets called from fopsFileThreadTrueDelete before
 *      an instance of WPFolder actually gets deleted.
 *
 *      Starting with V0.9.6, we no longer fully populate
 *      folders in order to do the entire processing via
 *      WPS projects. Instead, we only populate folders
 *      with subfolders and delete files using Dos* functions,
 *      which is much faster and puts less stress on the WPS
 *      object management which runs into trouble otherwise.
 *
 *      This function must delete all dormant files in the
 *      specified folder, but not the folder itself.
 *
 *      Preconditions:
 *
 *      1)  Subfolders of the folder have already been deleted.
 *
 *      2)  Files in the folder which were already awake have
 *          already been deleted using _wpFree.
 *
 *      So there should be only dormant files left, which we
 *      can safely delete using Dos* functions.
 *
 *@@added V0.9.6 (2000-10-25) [umoeller]
 */

FOPSRET fopsFileThreadSneakyDeleteFolderContents(PFILETASKLIST pftl,
                                                 FOPSUPDATE *pfu,
                                    // in: update structure in fopsFileThreadProcessing
                                                 WPObject **ppObject,
                                    // in: folder whose contents are to be deleted,
                                    // out: failing object if error
                                                 WPFolder *pMainFolder,
                                    // in: main folder of which *ppObject is a subobject
                                    // somehow
                                                 PULONG pulIgnoreSubsequent,
                                    // in: ignore subsequent errors of the same type
                                                 ULONG ulProgressScalarFirst,
                                                 ULONG cSubObjects,
                                                 PULONG pulSubObjectThis)
{
    FOPSRET     frc = NO_ERROR;
    WPFolder    *pFolder = *ppObject;
    CHAR        szFolderPath[CCHMAXPATH] = "";
    CHAR        szMainFolderPath[CCHMAXPATH] = "";
    HDIR        hdirFindHandle = HDIR_CREATE;

    BOOL    fFolderSemOwned = FALSE;

    ULONG   ulNesting = 0;
    // DosEnterMustComplete(&ulNesting); ###

    TRY_LOUD(excpt1)
    {
        fFolderSemOwned = !wpshRequestFolderMutexSem(pFolder, 5000);
        if (!fFolderSemOwned)
            frc = FOPSERR_REQUESTFOLDERMUTEX_FAILED;
        else
        {
            if (    (!_wpQueryFilename(pFolder, szFolderPath, TRUE))
                 || (!_wpQueryFilename(pMainFolder, szMainFolderPath, TRUE))
               )
                frc = FOPSERR_WPQUERYFILENAME_FAILED;
            else
            {
                ULONG           ulMainFolderPathLen = strlen(szMainFolderPath);
                M_WPFileSystem  *pWPFileSystem = _WPFileSystem;

                CHAR            szSearchMask[CCHMAXPATH];
                FILEFINDBUF3    ffb3 = {0};      // returned from FindFirst/Next
                ULONG           cbFFB3 = sizeof(FILEFINDBUF3);
                ULONG           ulFindCount = 1;  // look for 1 file at a time

                _Pmpf((__FUNCTION__ ": doing DosFindFirst for %s",
                            szFolderPath));

                // now go find...
                sprintf(szSearchMask, "%s\\*", szFolderPath);
                frc = DosFindFirst(szSearchMask,
                                   &hdirFindHandle,
                                   // find everything except directories
                                   FILE_ARCHIVED | FILE_HIDDEN | FILE_SYSTEM | FILE_READONLY,
                                   &ffb3,
                                   cbFFB3,
                                   &ulFindCount,
                                   FIL_STANDARD);
                // and start looping...
                while (frc == NO_ERROR)
                {
                    // alright... we got the file's name in ffb3.achName
                    CHAR    szFullPath[2*CCHMAXPATH];
                    sprintf(szFullPath, "%s\\%s", szFolderPath, ffb3.achName);

                    _Pmpf(("    got file %s", szFullPath));

                    // call callback for subobject;
                    // as the path, get the full path name of the file
                    // minus the full path of the main folder we are
                    // working on
                    pfu->pszSubObject = szFullPath + ulMainFolderPathLen + 1;
                    // calc new sub-progress: this is the value we first
                    // had before working on the subobjects (which
                    // is a multiple of 100) plus a sub-progress between
                    // 0 and 100 for the subobjects
                    pfu->ulProgressScalar = ulProgressScalarFirst
                                            + (((*pulSubObjectThis) * 100 )
                                                 / cSubObjects);
                    frc = fopsCallProgressCallback(pftl,
                                                   FOPSUPD_SUBOBJECT_CHANGED
                                                    | FOPSPROG_UPDATE_PROGRESS,
                                                   pfu);

                    if (frc == NO_ERROR)
                    {
                        // no error, not cancelled:
                        // check file's attributes
                        if (ffb3.attrFile & (FILE_SYSTEM | FILE_READONLY))
                        {
                            // system or read-only file:
                            // prompt!!

                            // 1) awake object
                            WPFileSystem *pFSObj = _wpclsQueryObjectFromPath(_WPFileSystem,
                                                                             szFullPath);
                            _Pmpf(("        is readonly"));
                            if (!pFSObj)
                                frc = FOPSERR_INVALID_OBJECT;
                            else
                            {
                                _Pmpf(("        calling fopsFileThreadFixNonDeletable"));
                                frc = fopsFileThreadFixNonDeletable(pftl,
                                                                    pFSObj,
                                                                    pulIgnoreSubsequent);
                            }

                            if (frc == NO_ERROR)
                            {
                                // either no problem or problem fixed:
                                if (!_wpFree(pFSObj))
                                {
                                    frc = FOPSERR_WPFREE_FAILED;
                                    *ppObject = pFSObj;
                                }
                            }
                        }
                        else
                        {
                            // sneaky delete!!
                            frc = DosDelete(szFullPath);
                            _Pmpf(("    deleted file %s --> %d", szFullPath, frc));
                        }
                    }

                    if (frc == NO_ERROR)
                    {
                        ulFindCount = 1;
                        frc = DosFindNext(hdirFindHandle,
                                          &ffb3,
                                          cbFFB3,
                                          &ulFindCount);

                        // raise object count for progress...
                        (*pulSubObjectThis)++;
                    }
                } // while (arc == NO_ERROR)

                if (frc == ERROR_NO_MORE_FILES)
                    frc = NO_ERROR;
            }
        }
    }
    CATCH(excpt1)
    {
        frc = FOPSERR_FILE_THREAD_CRASHED;
    } END_CATCH();

    if (hdirFindHandle != HDIR_CREATE)
        DosFindClose(hdirFindHandle);

    if (fFolderSemOwned)
        wpshReleaseFolderMutexSem(pFolder);

    // DosExitMustComplete(&ulnesting) ###

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
 *      call fopsFileThreadSneakyDeleteFolderContents,
 *      if necessary.
 *
 *      This finally allows cancelling a "delete"
 *      operation when a subfolder is currently
 *      processed.
 *
 *@@added V0.9.3 (2000-04-27) [umoeller]
 *@@changed V0.9.3 (2000-04-30) [umoeller]: reworked progress reports
 *@@changed V0.9.4 (2000-07-27) [umoeller]: added ulIgnoreSubsequent to ignore further errors
 *@@changed V0.9.6 (2000-10-25) [umoeller]: largely rewritten to support sneaky delete (much faster)
 */

FOPSRET fopsFileThreadTrueDelete(HFILETASKLIST hftl,
                                 FOPSUPDATE *pfu,       // in: update structure in fopsFileThreadProcessing
                                 WPObject **ppObject,   // in: object to delete;
                                                        // out: failing object if error
                                 PULONG pulIgnoreSubsequent) // in: ignore subsequent errors of the same type
{
    FOPSRET frc = NO_ERROR;
    PFILETASKLIST pftl = (PFILETASKLIST)hftl;

    // list of WPObject* pointers
    LINKLIST llSubObjects;
    lstInit(&llSubObjects,
             FALSE);    // no free, we have WPObject* pointers

    // another exception handler to allow for cleanup
    TRY_LOUD(excpt1)
    {
        // say "collecting objects"
        pfu->pSourceObject = *ppObject;      // callback has already been called
                                             // for this one

        frc = fopsCallProgressCallback(pftl,
                                       FOPSUPD_EXPANDING_SOURCEOBJECT_1ST,
                                       pfu);

        if (frc == NO_ERROR)
        {
            // build list of all objects to be deleted,
            // but populate folders only...
            // this will give us all objects in the
            // folders which are already awake before
            // the folder itself.
            ULONG   cSubObjects = 0,
                    cSubObjectsTemp = 0,
                    cSubDormantFilesTemp = 0;
            frc = fopsExpandObjectFlat(&llSubObjects,
                                       *ppObject,
                                       TRUE,        // populate folders only
                                       &cSubObjectsTemp,
                                       &cSubDormantFilesTemp);
                    // now cSubObjectsTemp has the no. of awake objects,
                    // cSubDormantFilesTemp has the no. of dormant files

            // So we need to do the following:

            // 1) If the object is a non-folder, delete it.
            //    It is some object in a folder which was already
            //    awake.
            // 2) If we encounter a folder, do a "sneaky delete"
            //    of all files in the folder (to avoid the WPS
            //    popups for read-only files) and then delete
            //    the folder object.

            if (frc != NO_ERROR)
                _Pmpf((__FUNCTION__ ": fopsExpandObjectFlat returned %d", frc));
            else
            {
                _Pmpf((__FUNCTION__ ": fopsExpandObjectFlat got %d + %d objects",
                            cSubObjectsTemp, cSubDormantFilesTemp));

                // say "done collecting objects"
                frc = fopsCallProgressCallback(pftl,
                                               FOPSUPD_EXPANDING_SOURCEOBJECT_DONE,
                                               pfu);
                // calc total count for progress
                cSubObjects = cSubObjectsTemp + cSubDormantFilesTemp;
            }

            if (    (frc == NO_ERROR)
                 && (cSubObjects) // avoid division by zero below
               )
            {
                ULONG ulSubObjectThis = 0,
                      // save progress scalar
                      ulProgressScalarFirst = pfu->ulProgressScalar;

                // all sub-objects collected:
                // go thru the whole list and start deleting.
                // Note that folder contents which are already awake
                // come before the containing folder ("bottom-up" directory list).
                PLISTNODE pNode = lstQueryFirstNode(&llSubObjects);
                while ((pNode) && (frc == NO_ERROR))
                {
                    // get object to delete...
                    WPObject *pSubObjThis = (WPObject*)pNode->pItemData;

                    // now check if we have a folder...
                    // if so, we need some special processing
                    if (_somIsA(pSubObjThis, _WPFolder))
                    {
                        // folder:
                        // do sneaky delete of dormant folder contents
                        // (awake folder contents have already been freed
                        // because these came before the folder on the
                        // subobjects list)
                        WPObject *pobj2 = pSubObjThis;
                        _Pmpf(("Sneaky delete folder %s", _wpQueryTitle(pSubObjThis) ));
                        frc = fopsFileThreadSneakyDeleteFolderContents(pftl,
                                                                       pfu,
                                                                       &pobj2,
                                                                       *ppObject,
                                                                        // main folder; this must be
                                                                        // a folder, because we have a subobject
                                                                       pulIgnoreSubsequent,
                                                                       ulProgressScalarFirst,
                                                                       cSubObjects,
                                                                       &ulSubObjectThis);
                        if (frc != NO_ERROR)
                            *ppObject = pobj2;

                        // force the WPS to flush all pending
                        // auto-refresh information for this folder...
                        // these have all piled up for the sneaky stuff
                        // above and will cause some internal overflow
                        // if we don't give the WPS a chance to process them!
                        wpshFlushNotifications(pSubObjThis);
                    }

                    if (frc == NO_ERROR)
                    {
                        _Pmpf(("Real-delete object %s", _wpQueryTitle(pSubObjThis) ));

                        // delete the object: we get here for any instantiated
                        // WPS object which must be deleted, that is
                        // -- for any awake WPS object in any subfolder (before the folder)
                        // -- for any folder after its contents have been deleted
                        //    (either sneakily or by a previous wpFree)

                        // call callback for subobject
                        pfu->pszSubObject = _wpQueryTitle(pSubObjThis);
                        // calc new sub-progress: this is the value we first
                        // had before working on the subobjects (which
                        // is a multiple of 100) plus a sub-progress between
                        // 0 and 100 for the subobjects
                        pfu->ulProgressScalar = ulProgressScalarFirst
                                                + ((ulSubObjectThis * 100 )
                                                     / cSubObjects);
                        frc = fopsCallProgressCallback(pftl,
                                                       FOPSUPD_SUBOBJECT_CHANGED
                                                        | FOPSPROG_UPDATE_PROGRESS,
                                                       pfu);
                        if (frc)
                        {
                            // error or cancelled:
                            *ppObject = pSubObjThis;
                            break;
                        }

                        if (!_wpIsDeleteable(pSubObjThis))
                            // not deletable: prompt user about
                            // what to do with this
                            frc = fopsFileThreadFixNonDeletable(pftl,
                                                                pSubObjThis,
                                                                pulIgnoreSubsequent);

                        if (frc == NO_ERROR)
                        {
                            // either no problem or problem fixed:
                            if (!_wpFree(pSubObjThis))
                            {
                                frc = FOPSERR_WPFREE_FAILED;
                                *ppObject = pSubObjThis;
                                break;
                            }
                        }
                        else
                            // error:
                            *ppObject = pSubObjThis;
                    }

                    pNode = pNode->pNext;
                    ulSubObjectThis++;
                } // end while ((pNode) && (frc == NO_ERROR))

                // done with subobjects: report NULL subobject
                if (frc == NO_ERROR)
                {
                    pfu->pszSubObject = NULL;
                    frc = fopsCallProgressCallback(pftl,
                                                   FOPSUPD_SUBOBJECT_CHANGED,
                                                   pfu);
                }
            } // end if (    (frc == NO_ERROR)
        } // end if (frc == NO_ERROR)
    }
    CATCH(excpt1)
    {
        frc = FOPSERR_FILE_THREAD_CRASHED;
    } END_CATCH();

    lstClear(&llSubObjects);

    return (frc);
}

/*
 *@@ fopsFileThreadFontProcessing:
 *      gets called from fopsFileThreadProcessing with
 *      XFT_INSTALLFONTS and  XFT_DEINSTALLFONTS for every
 *      object on the list.
 *
 *      This calls the font engine (src\config.fonts.c)
 *      in turn, most notably fonInstallFont or fonDeInstallFont.
 *
 *@@added V0.9.7 (2001-01-13) [umoeller]
 */

FOPSRET fopsFileThreadFontProcessing(HAB hab,
                                     PFILETASKLIST pftl,
                                     WPObject *pObjectThis,
                                     PULONG pulIgnoreSubsequent)
{
    WPObject *pFree = NULL;
    FOPSRET frc = FOPSERR_INTEGRITY_ABORT;

    switch (pftl->ulOperation)
    {
        case XFT_INSTALLFONTS:
            frc = fonInstallFont(hab,
                                 pftl->pTargetFolder, // font folder
                                 pObjectThis,    // font file
                                 NULL);          // out: new obj
                    // this returns proper APIRET or FOPSRET codes
        break;

        case XFT_DEINSTALLFONTS:
            frc = fonDeInstallFont(hab,
                                   pftl->pSourceFolder,
                                   pObjectThis);     // font object
                    // this returns proper APIRET or FOPSRET codes
            if ((frc == NO_ERROR) || (frc == FOPSERR_FONT_STILL_IN_USE))
                // in these two cases only, destroy the
                // font object after we've called the error
                // callback... it still needs the object!
                pFree = pObjectThis;
        break;
    }

    if (frc != NO_ERROR)
    {
        if ( 0 == ((*pulIgnoreSubsequent) & FOPS_ISQ_FONTINSTALL) )
            // first error, or user has not selected
            // to abort subsequent:
            // now, these errors we should report;
            // if the error callback returns NO_ERROR,
            // we continue anyway
            frc = pftl->pfnErrorCallback(pftl->ulOperation,
                                         pObjectThis,
                                         frc,
                                         pulIgnoreSubsequent);
        else
            // user has chosen to ignore subsequent:
            // just go on
            frc = NO_ERROR;
    }

    if (pFree)
        _wpFree(pFree);

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
 *@@changed V0.9.7 (2001-01-13) [umoeller]: added hab to prototype (needed for font install)
 *@@changed V0.9.7 (2001-01-13) [umoeller]: added XFT_INSTALLFONTS, XFT_DEINSTALLFONTS
 */

VOID fopsFileThreadProcessing(HAB hab,              // in: file thread's anchor block
                              HFILETASKLIST hftl,
                              HWND hwndNotify)      // in: if != NULLHANDLE, post this
                                                    // window a T1M_FOPS_TASK_DONE msg
{
    BOOL frc = NO_ERROR;
    PFILETASKLIST pftl = (PFILETASKLIST)hftl;

    #ifdef DEBUG_TRASHCAN
        _Pmpf(("fopsFileThreadProcessing hftl: 0x%lX", hftl));
    #endif

    TRY_LOUD(excpt2)
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
            }

            fu.ulOperation = pftl->ulOperation;
            fu.pSourceFolder = pftl->pSourceFolder;
            fu.pTargetFolder = pftl->pTargetFolder;
            fu.fFirstCall = TRUE;  // unset by CallProgressCallback after first call
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
                                                       FOPSUPD_SOURCEOBJECT_CHANGED
                                                        | FOPSPROG_UPDATE_PROGRESS,
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
                        if (!wpshCheckIfPopulated(pObjectThis,
                                                  FALSE))   // full populate
                            frc = FOPSERR_POPULATE_FAILED;
                    break;

                    /*
                     * XFT_INSTALLFONTS:
                     *
                     */

                    case XFT_INSTALLFONTS:
                    case XFT_DEINSTALLFONTS:
                        frc = fopsFileThreadFontProcessing(hab,
                                                           pftl,
                                                           pObjectThis,
                                                           &ulIgnoreSubsequent);
                    break;
                }

                #ifdef __DEBUG__
                    if (frc != NO_ERROR)
                    {
                        CHAR szMsg[3000];
                        PSZ pszTitle = "?";
                        if (wpshCheckObject(pObjectThis))
                            pszTitle = _wpQueryTitle(pObjectThis);
                        sprintf(szMsg,
                                "Error %d with object %s",
                                frc,
                                pszTitle);
                        WinMessageBox(HWND_DESKTOP, NULLHANDLE,
                                      szMsg,
                                      "XWorkplace File thread",
                                      0, MB_OK | MB_ICONEXCLAMATION | MB_MOVEABLE);
                    }
                #endif

                pNode = pNode->pNext;
                ulCurrentObject++;
                fu.ulProgressScalar = ulCurrentObject * 100; // for progress
            } // end while (pNode)

            // call progress callback to say "done"
            fopsCallProgressCallback(pftl,
                                     FOPSPROG_DONEWITHALL | FOPSPROG_UPDATE_PROGRESS,
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
            wpshReleaseFolderMutexSem(pftl->pTargetFolder);
            pftl->fTargetLocked = FALSE;
        }
        if (pftl->fSourceLocked)
        {
            wpshReleaseFolderMutexSem(pftl->pSourceFolder);
            pftl->fSourceLocked = FALSE;
        }
        lstClear(&pftl->llObjects);       // frees items automatically
        free(pftl);
    }
    return (brc);
}


