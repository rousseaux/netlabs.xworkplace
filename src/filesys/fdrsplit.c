
/*
 *@@sourcefile fdrsplit.c:
 *      folder "split view" implementation.
 *
 *
 *@@added V1.0.0 (2002-08-21) [umoeller]
 *@@header "filesys\folder.h"
 */

/*
 *      Copyright (C) 2001-2016 Ulrich M”ller.
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
#define INCL_WINMENUS
#define INCL_WINSTDCNR
#define INCL_WINSHELLDATA
#define INCL_WINSCROLLBARS
#define INCL_WINSYS
#define INCL_WINTIMER

#define INCL_GPIBITMAPS
#define INCL_GPIREGIONS
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
#include "helpers\gpih.h"               // GPI helper routines
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\standards.h"          // some standard macros
#include "helpers\threads.h"            // thread helpers
#include "helpers\winh.h"               // PM helper routines

// SOM headers which don't crash with prec. header files
#include "xfldr.ih"
#include "xfdisk.ih"
#include "xfobj.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\classtest.h"           // some cheap funcs for WPS class checks
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\contentmenus.h"        // shared menu logic
#include "shared\cnrsort.h"             // container sort comparison functions
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

#include "filesys\folder.h"             // XFolder implementation
#include "filesys\fdrmenus.h"           // shared folder menu logic
#include "filesys\fdrsubclass.h"        // folder subclassing engine
#include "filesys\fdrsplit.h"           // folder split views
#include "filesys\fdrviews.h"           // common code for folder views
#include "filesys\object.h"             // XFldObject implementation
#include "filesys\statbars.h"           // status bar translation logic
#include "filesys\xview.h"              // file viewer based on splitview

// other SOM headers
#pragma hdrstop                         // VAC++ keeps crashing otherwise
#include <wprootf.h>

/* ******************************************************************
 *
 *   Private declarations
 *
 ********************************************************************/

PCSZ    WC_SPLITCONTROLLER  = "XWPSplitController",
        WC_SPLITPOPULATE   = "XWPSplitPopulate";

/* ******************************************************************
 *
 *   Split Insert Object
 *
 ********************************************************************/

/*
 *@@ fdrSplitInsertObject:
 *      called by XFolder::xf_wpAddToContent to insert newly
 *      awakened objects into open SplitView containers.
 *
 *      Most calls to this function occur as a folder is being
 *      populated for the first time.  Since SplitView doesn't
 *      insert a folder's contents into a container until it
 *      is done populating, the majority of calls return after
 *      doing nothing more than checking the folder flags.
 *
 *      There are two logic paths:  one for folders and disks,
 *      and one for everything else.  For the latter, we can
 *      ignore cases where the folder isn't populated yet
 *      (for the reason cited above) or the view is still
 *      opening.  Only objects newly added to a splitview
 *      file container that's fully ready need be acted upon.
 *
 *      For a folder or disk, we have to see if its parent
 *      (pFolder) has been inserted into the tree container.
 *      If so, pObject may need to be inserted if the parent
 *      is expanded or has no children yet. This is determined
 *      by fnwpTreeFrame when it receives our FM_INSERTFOLDER
 *      message.  That function will also _send_ an
 *      FM_INSERTOBJECT message to the files container if
 *      it happens to be showing the contents of pFolder.
 *
 *      Note:  because this function can run on any thread,
 *      it posts messages to the appropriate window rather
 *      than inserting the object itself to avoid having
 *      messages sent across thread boundaries.
 *
 *@@added V1.0.9 (2011-9-24) [rwalsh]: @@fixes 2
 */

BOOL  fdrSplitInsertObject(WPFolder *pFolder,
                           WPObject *pObject)
{
    BOOL      brc = TRUE,
              fFdrObjLocked = FALSE,
              fFdrOrDisk = FALSE;
    ULONG     ulFldrFlags = _wpQueryFldrFlags(pFolder);
    PUSEITEM  pui;

    if (ulFldrFlags & FOI_POPULATEINPROGRESS)
        return TRUE;

    if (   _somIsA(pObject, _WPFolder)
        || _somIsA(pObject, _WPDisk))
    {
        fFdrOrDisk = TRUE;
    }
    else
    {
        // there are cases where the folder is fully
        // populated but only WITHFOLDERS is set, so we
        // have to look for an open view in either case
        if (!(ulFldrFlags & (FOI_POPULATEDWITHALL | FOI_POPULATEDWITHFOLDERS)))
            return TRUE;
    }

    // if pFolder a root folder, use the corresponding disk object instead
    // since that's what gets inserted into containers and has open views
    // changed V1.0.9 (2012-02-13) [rwalsh]: fix refresh failure for root directories
    if (_somIsA(pFolder, _WPRootFolder)) {
        pFolder = _wpQueryDisk(pFolder);
        if (!pFolder)
            return FALSE;
    }

    TRY_LOUD(excpt1)
    {
        if ((fFdrObjLocked = !_wpRequestObjectMutexSem(pFolder, SEM_INDEFINITE_WAIT)))
        {
            if (fFdrOrDisk)
            {
                for (pui = _wpFindUseItem(pFolder, USAGE_RECORD, NULL);
                    pui;
                    pui = _wpFindUseItem(pFolder, USAGE_RECORD, pui))
                {
                    PRECORDITEM   pri = (PRECORDITEM)(pui + 1);
                    HWND          hwndFrame = WinQueryWindow(pri->hwndCnr, QW_PARENT);

                    if (WinQueryWindowUShort(hwndFrame, QWS_ID) == ID_TREEFRAME)
                    {
                        WinPostMsg(hwndFrame,
                                   FM_INSERTFOLDER,
                                   (MPARAM)pObject,
                                   (MPARAM)pFolder);
                    }
                }
            }
            else
            {
                for (pui = _wpFindUseItem(pFolder, USAGE_OPENVIEW, NULL);
                    pui;
                    pui = _wpFindUseItem(pFolder, USAGE_OPENVIEW, pui))
                {
                    PVIEWITEM   pvi = (PVIEWITEM)(pui + 1);
                    if (    (pvi->view == *G_pulVarMenuOfs + ID_XFMI_OFS_SPLITVIEW_SHOWING)
                        && (!(pvi->ulViewState & VIEWSTATE_OPENING)))
                    {
                        WinPostMsg(pvi->handle,
                                   FM_INSERTOBJECT,
                                   (MPARAM)pObject,
                                   (MPARAM)pFolder);
                    }
                }
            }
        }
    }
    CATCH(excpt1)
    {
       brc = FALSE;
    } END_CATCH();

    if (fFdrObjLocked)
        _wpReleaseObjectMutexSem(pFolder);

    return brc;
}

/* ******************************************************************
 *
 *   Split Populate thread
 *
 ********************************************************************/

/*
 *@@ FindFirstChild:
 *      returns the first child folder within a given folder.
 *
 *      This gets called for every visible record in the
 *      folder tree so we properly add the "+" expansion
 *      signs to each record without having to fully populate
 *      each folder. This is an imitation of the standard
 *      WPS behavior in Tree views.
 *
 *      While it will always return the first insertable child
 *      (if there is one), it will only return TRUE if the
 *      child needs to be explicitly inserted.   This is true
 *      for objects that are already awake and for the root
 *      folders of disks.  For newly awakened children,
 *      fdrSplitInsertObject will already have posted an
 *      FM_INSERTFOLDER message before this function returns.
 *
 *      Note:  the tests for 'pObject != pRoot' deals with an
 *      unusual situation where the first child found is also
 *      the root folder for the entire tree.  If we don't find
 *      another child to insert, the parent folder won't get a
 *      "+".  This can only happen if the root is somewhere in
 *      the Desktop tree and you drill down through the Drives
 *      folder back to the Desktop.
 *
 *      Runs on the split populate thread (fntSplitPopulate).
 *
 *@@added V0.9.18 (2002-02-06) [umoeller]
 *@@changed V1.0.0 (2002-09-09) [umoeller]: removed linklist
 *@@changed V1.0.9 (2011-09-24) [rwalsh]: restructured and renamed, was 'AddFirstChild'
 */

STATIC BOOL FindFirstChild(PMINIRECORDCORE precParent, // in: folder rec to search for first child
                           WPFolder *pRoot,            // in: the tree's root folder
                           WPObject **ppChild)         // out: the first insertable child
{
    WPFolder  *pFolder;
    BOOL      fFolderLocked = FALSE,
              fFindLocked   = FALSE,
              fInsert       = FALSE;

    *ppChild = NULL;
    if (!(pFolder = fdrvGetFSFromRecord(precParent, TRUE)))
        return FALSE;

    TRY_LOUD(excpt1)
    {
        // request the find sem to make sure we won't have a populate
        // on the other thread; otherwise we get duplicate objects here
        if (fFindLocked = !_wpRequestFindMutexSem(pFolder, SEM_INDEFINITE_WAIT))
        {
            WPObject    *pObject;

            // check if we have a subfolder in the folder already
            if (fFolderLocked = !_wpRequestFolderMutexSem(pFolder, SEM_INDEFINITE_WAIT))
            {
                for (   pObject = _wpQueryContent(pFolder, NULL, QC_FIRST);
                        pObject;
                        pObject = *__get_pobjNext(pObject))
                {
                    // changed V1.0.9 (2011-09-24) [rwalsh]: replaced FOLDERSONLY
                    // with FOLDERSANDDISKS so drives get the "+" next to them;
                    // also added test for pObject == pRoot (see above)
                    if (   pObject != pRoot
                        && fdrvIsInsertable(pObject,
                                            INSERT_FOLDERSANDDISKS,
                                            NULL))
                    {
                        *ppChild = pObject;
                        fInsert = TRUE;
                        break;
                    }
                }

                _wpReleaseFolderMutexSem(pFolder);
                fFolderLocked = FALSE;
            }

            PMPF_POPULATESPLITVIEW(("pFirstChild pop is 0x%lX", *ppChild));

            if (!*ppChild)
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

                PMPF_POPULATESPLITVIEW(("searching %s", szSearchMask));

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
                    PMPF_POPULATESPLITVIEW(("got %s", ffb3.achName));

                    // do not use "." and ".."
                    if (    (strcmp(ffb3.achName, ".") != 0)
                         && (strcmp(ffb3.achName, "..") != 0)
                       )
                    {
                        // this is good:
                        CHAR szFolder2[CCHMAXPATH];
                        sprintf(szFolder2, "%s\\%s", szFolder, ffb3.achName);

                        PMPF_POPULATESPLITVIEW(("awaking %s", szFolder2));

                        pObject = _wpclsQueryFolder(_WPFolder,
                                                    szFolder2,
                                                    TRUE);

                        // exclude templates - no need for INSERT_FOLDERSANDDISKS
                        // here since disk objects aren't filesystem objects
                        // changed V1.0.9 (2011-09-24) [rwalsh]: added test
                        //                for pObject != pRoot (see above)
                        if (   pObject != pRoot
                            && fdrvIsInsertable(pObject,
                                                INSERT_FOLDERSONLY,
                                                NULL))
                        {
                            *ppChild = pObject;

                            // when fdrSplitInsertObject posts an FM_INSERTFOLDER
                            // for a Root Folder, hwndTreeFrame won't be able to
                            // identify it as a child of any existing folder record;
                            // consequently, the caller must post the message and
                            // identify the corresponding disk object as its
                            // parent
                            if (_somIsA(pFolder, _WPRootFolder))
                                fInsert = TRUE;
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

    return fInsert;
}

/*
 *@@ fnwpSplitPopulate:
 *      object window for populate thread.
 *
 *      Runs on the split populate thread (fntSplitPopulate).
 *
 *@@changed V1.0.0 (2002-11-23) [umoeller]: split view stopped working when populate failed, @@fixes 192
 *@@changed V1.0.9 (2011-09-24) [rwalsh]: added FM2_ADDFIRSTCHILDREN and restructured first-child code
 */

STATIC MRESULT EXPENTRY fnwpSplitPopulate(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
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
            ULONG           fl = (ULONG)mp2,
                            fl2 = FFL_POPULATEFAILED;
            BOOL            fFoldersOnly = ((fl & FFL_FOLDERSONLY) != 0);
            ULONG           ulPopulated = 0;

            // set wait pointer
            (psv->cThreadsRunning)++;
            WinPostMsg(psv->hwndMainControl,
                       FM_UPDATEPOINTER,
                       0,
                       0);

            if (pFolder = fdrvGetFSFromRecord(prec, TRUE))
            {
                PMPF_POPULATESPLITVIEW(("populating %s", _wpQueryTitle(pFolder)));

                if (ulPopulated = fdrCheckIfPopulated(pFolder,
                                                      fFoldersOnly))
                {
                    // ulPopulated is 1 if we really did populate,
                    // or 2 if the folder was already populated

                    fl2 = fl;
                    // only if the folder was newly populated, then
                    // we need to set the FFL_UNLOCK bit so we
                    // can unlock objects that either do not
                    // get inserted or when the container
                    // is cleared again
                    if (ulPopulated == 1)
                        fl2 |= FFL_UNLOCKOBJECTS;
                }
            }

            // refresh the files only if we are not
            // in folders-only mode
            if (!fFoldersOnly)
                WinPostMsg(psv->hwndMainControl,
                           FM_POPULATED_FILLFILES,
                           (MPARAM)prec,
                           (MPARAM)fl2);

            // in any case, refresh the tree
            WinPostMsg(psv->hwndMainControl,
                       FM_POPULATED_FILLTREE,
                       (MPARAM)prec,
                       (MPARAM)(fl | fl2));
                    // fnwpMainControl will check fl again and
                    // fire "add first child" msgs accordingly

            if (ulPopulated)
                if (fl & FFL_SCROLLTO)
                    WinPostMsg(psv->hwndMainControl,
                               FM_POPULATED_SCROLLTO,
                               (MPARAM)prec,
                               0);

            // clear wait pointer
            (psv->cThreadsRunning)--;
            WinPostMsg(psv->hwndMainControl,
                       FM_UPDATEPOINTER,
                       0,
                       0);
        }
        break;

        /*
         *@@ FM2_ADDFIRSTCHILDREN:
         *      posted by fdrAddFirstChild to locate and add the
         *      first child folder for each folder or disk listed
         *      in the supplied array.  The goal is to display a
         *      "+" next to each folder that contains subfolders.
         *
         *      --  PMINIRECORDCORE[] mp1:  pointer to an array of
         *          record pointers representing folders and disks
         *          in the tree;  none of them currently have any
         *          child records. The array must be freed when done.
         *
         *      --  ULONG mp2:  count of record pointers in the array
         *
         *      Children are not inserted directly from here because
         *      doing so would require a cross-thread call to
         *      WinSendMsg.  Instead, an FM_INSERTFOLDER message is
         *      posted to hwndTreeFrame so that the insertion occurs
         *      on the thread that created the window.
         *
         *      Note:  while FindFirstChild will always return the
         *      first insertable child (if there is one), it will
         *      only return TRUE if we need to explicitly have it
         *      inserted.  In cases where the child is newly awakened,
         *      fdrSplitInsertObject will already have posted an
         *      FM_INSERTFOLDER message before FindFirstChild returns.
         *
         *@@added V1.0.9 (2011-09-24) [rwalsh]: replaced FM2_ADDFIRSTCHILD_BEGIN/NEXT/DONE
         */
        case FM2_ADDFIRSTCHILDREN:
        {
            PFDRSPLITVIEW     psv = WinQueryWindowPtr(hwnd, QWL_USER);
            WPFolder          *pFirstChild;
            PMINIRECORDCORE   *papRecs;
            ULONG             ctr;

            if (mp1 && mp2 && psv)
            {
                for (papRecs = (PMINIRECORDCORE*)mp1, ctr = 0;
                     ctr < (ULONG)mp2;
                     ctr++)
                {
                    if (FindFirstChild(papRecs[ctr],
                                       psv->pRootObject,
                                       &pFirstChild))
                    {
                        WinPostMsg(psv->hwndTreeFrame,
                                   FM_INSERTFOLDER,
                                   (MPARAM)pFirstChild,
                                   (MPARAM)OBJECT_FROM_PREC(papRecs[ctr]));
                    }
                }
            }
            if (mp1)
                free(mp1);
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

STATIC VOID _Optlink fntSplitPopulate(PTHREADINFO ptiMyself)
{
    TRY_LOUD(excpt1)
    {
        QMSG qmsg;
        PFDRSPLITVIEW psv = (PFDRSPLITVIEW)ptiMyself->ulData;

        PMPF_POPULATESPLITVIEW(("thread starting"));

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

        PMPF_POPULATESPLITVIEW(("thread ending"));

    }
    CATCH(excpt1) {} END_CATCH();

}

/*
 *@@ fdrSplitPopulate:
 *      posts FM2_POPULATE to fnwpSplitPopulate to
 *      populate the folder represented by the
 *      given MINIRECORDCORE according to fl.
 *
 *      fnwpSplitPopulate does the following:
 *
 *      1)  Before populating, raise psv->cThreadsRunning
 *          and post FM_UPDATEPOINTER to hwndMainControl.
 *          to have it display the "wait" pointer.
 *
 *      2)  Run _wpPopulate on the folder represented
 *          by prec.
 *
 *      3)  Post FM_POPULATED_FILLTREE back to hwndMainControl
 *          in any case to fill the tree under prec with the
 *          subfolders that were found.
 *
 *      4)  If the FFL_SCROLLTO flag is set, post
 *          FM_POPULATED_SCROLLTO back to hwndMainControl
 *          so that the tree can be scrolled properly.
 *
 *      5)  If the FFL_FOLDERSONLY flag was _not_ set,
 *          post FM_POPULATED_FILLFILES to hwndMainControl
 *          so it can insert all objects into the files
 *          container.
 *
 *      6)  Decrement psv->cThreadsRunning and post
 *          FM_UPDATEPOINTER again to reset the wait
 *          pointer.
 */

VOID fdrSplitPopulate(PFDRSPLITVIEW psv,
                      PMINIRECORDCORE prec,     // in: record to populate (can be disk or shadow)
                      ULONG fl)
{
    WinPostMsg(psv->hwndSplitPopulate,
               FM2_POPULATE,
               (MPARAM)prec,
               (MPARAM)fl);
}

#ifdef __DEBUG__
VOID DumpFlags(ULONG fl)
{
    PMPF_POPULATESPLITVIEW(("  fl %s %s %s %s",
                (fl & FFL_FOLDERSONLY) ? "FFL_FOLDERSONLY " : "",
                (fl & FFL_SCROLLTO) ? "FFL_SCROLLTO " : "",
                (fl & FFL_EXPAND) ? "FFL_EXPAND " : "",
                (fl & FFL_SETBACKGROUND) ? "FFL_SETBACKGROUND " : ""
              ));
}
#else
    #define DumpFlags(fl)
#endif

/*
 *@@ fdrPostFillFolder:
 *      posts FM_FILLFOLDER to the main control
 *      window with the given parameters.
 *
 *      This gets called from the tree frame
 *      to fire populate when things get
 *      selected in the tree.
 *
 *      The main control window will then call
 *      fdrSplitPopulate to have FM2_POPULATE
 *      posted to the split populate thread.
 */

VOID fdrPostFillFolder(PFDRSPLITVIEW psv,
                       PMINIRECORDCORE prec,       // in: record with folder to populate
                       ULONG fl)                   // in: FFL_* flags
{
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


/*
 *@@ fdrAddFirstChild:
 *    begins the process of putting a "+" next to
 *    each folder in the tree container that needs
 *    one.
 *
 *    It is called by the the split-controller
 *    wndproc in response to FM_POPULATED_FILLTREE
 *    after new records have been inserted in the
 *    folder tree.
 *
 *    It creates an array of pointers to records
 *    in the tree that have no child records, then
 *    posts an FM2_ADDFIRSTCHILDREN message to the
 *    split-populate thread with the array pointer
 *    and record count as params.
 *
 *    The populate thread will look for a suitable
 *    child folder for each record, then cause an
 *    FM_INSERTFOLDER message to be posted to the
 *    tree's frame window if found. When done,
 *    the populate thread will free the array.
 *
 *    The process of querying the container is done
 *    here, rather than on the split-populate thread,
 *    to eliminate the cross-thread WinSendMsg calls
 *    in the previous implementation.
 *
 *@@added V1.0.9 (2011-09-24) [rwalsh]
 */

VOID fdrAddFirstChild(PMINIRECORDCORE precParent,
                      HWND hwndCnr,
                      HWND hwndAddFirstChild)
{
    PMINIRECORDCORE   prec;
    PMINIRECORDCORE   *papRecs = NULL;
    ULONG             cRecs = 0,
                      cArray = 0;

    // get the first of the records we'll be enumerating
    prec = (PMINIRECORDCORE)WinSendMsg(hwndCnr,
                                       CM_QUERYRECORD,
                                       precParent,
                                       MPFROM2SHORT(CMA_FIRSTCHILD,
                                                    CMA_ITEMORDER));

    while (prec && prec != (PMINIRECORDCORE)-1)
    {
        // if the rec doesn't have any children, add it to the array
        if (!WinSendMsg(hwndCnr,
                        CM_QUERYRECORD,
                        (MPARAM)prec,
                        MPFROM2SHORT(CMA_FIRSTCHILD,
                                     CMA_ITEMORDER)))
        {
            // allocate array on first use, expand when necessary
            if (cRecs >= cArray)
            {
                cArray += 256;
                papRecs = (PMINIRECORDCORE*)realloc(papRecs,
                              cArray * sizeof(PMINIRECORDCORE));
            }
            papRecs[cRecs++] = prec;
        }

        // get the next record
        prec = (PMINIRECORDCORE)WinSendMsg(hwndCnr,
                                           CM_QUERYRECORD,
                                           (MPARAM)prec,
                                           MPFROM2SHORT(CMA_NEXT,
                                                        CMA_ITEMORDER));
    }

    // if there were any records without children, post a msg
    // to the populate thread with the array and record count;
    // it will free the array when done
    if (cRecs)
        WinPostMsg(hwndAddFirstChild,
                   FM2_ADDFIRSTCHILDREN,
                   (MPARAM)papRecs,
                   (MPARAM)cRecs);
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
 *      flInsert is used as follows:
 *
 *      --  If the INSERT_UNLOCKFILTERED bit is set
 *          (0x10000000), we automatically unlock each
 *          object for which fdrvIsInsertable has told us
 *          that it should not be inserted. This bit must
 *          be set if you have populated the folder,
 *          because wpPopulate locks each object in the
 *          folder once. If there was no fresh populate
 *          because the folder was already populated,
 *          that bit MUST BE CLEAR or we'll run into
 *          objects that might become dormant while
 *          still being inserted.
 *
 *      --  The low byte (which is copied to ulFoldersOnly)
 *          has the insert mode as given to fdrvIsInsertable.
 *          See remarks there.
 *
 *@@changed V1.0.0 (2002-09-09) [umoeller]: removed linklist
 *@@changed V1.0.9 (2011-09-24) [rwalsh]: moved FM2_ADDFIRSTCHILD_* logic to fdrAddFirstChild
 *@@changed V1.0.9 (2011-09-24) [rwalsh]: added 'cInsertable' as return value
 *@@changed V1.0.11 (2016-08-12) [rwalsh]: added details view support
 */
ULONG fdrInsertContents(WPFolder *pFolder,              // in: populated folder
                        HWND hwndCnr,                   // in: cnr to insert records to
                        PMINIRECORDCORE precParent,     // in: parent record or NULL
                        ULONG flInsert,                 // in: INSERT_* flags
                        PCSZ pcszFileMask)              // in: file mask filter or NULL
{
    BOOL        fFolderLocked = FALSE;
    ULONG       cInsertable = 0;

    TRY_LOUD(excpt1)
    {
        // lock the folder contents for wpQueryContent
        if (fFolderLocked = !_wpRequestFolderMutexSem(pFolder, SEM_INDEFINITE_WAIT))
        {
            // build an array of objects that should be inserted
            // (we use an array because we want to use
            // _wpclsInsertMultipleObjects);
            // we run through the folder and check if the object
            // is insertable and, if so, add it to the array,
            // which is increased in size in chunks of 1000 pointers
            WPObject    *pObject;
            WPObject    **papObjects = NULL;
            ULONG       cObjects = 0,
                        cArray = 0;

            // filter out the real INSERT_* flags
            // V1.0.0 (2002-09-13) [umoeller]
            ULONG       ulFoldersOnly = flInsert & 0x0f;

            PMPF_POPULATESPLITVIEW(("file mask is \"%s\", ulFoldersOnly %d",
                                    (pcszFileMask) ? pcszFileMask : "NULL",
                                    ulFoldersOnly));

            for (   pObject = _wpQueryContent(pFolder, NULL, QC_FIRST);
                    pObject;
                    pObject = *__get_pobjNext(pObject)
                )
            {
                if (fdrvIsInsertable(pObject,
                                     ulFoldersOnly,
                                     pcszFileMask))
                {
                    PMINIRECORDCORE prc;

                    // count of records that _could_ be inserted -
                    // used by FM_POPULATED_FILLTREE
                    cInsertable++;

                    // _wpclsInsertMultipleObjects fails on the
                    // entire array if only one is already in the
                    // cnr, so make sure it isn't
                    if (!fdrvIsObjectInCnr(pObject, hwndCnr))
                    {
                        // create/expand array if necessary
                        if (cObjects >= cArray)     // on the first iteration,
                                                    // both are null
                        {
                            cArray += 1000;
                            papObjects = (WPObject**)realloc(papObjects, // NULL on first call
                                                             cArray * sizeof(WPObject*));
                        }

                        // mark non-filesystem records with CRA_IGNORE
                        // to prevent the WPS from drawing f/s details
                        // columns for objects that have no such data
                        // V1.0.11 (2016-08-12) [rwalsh]
                        if (ulFoldersOnly != INSERT_FOLDERSONLY &&
                            !_somIsA(pObject, _WPFileSystem) &&
                            (prc = _wpQueryCoreRecord(pObject)))
                            prc->flRecordAttr |= CRA_IGNORE;

                        // store in array
                        papObjects[cObjects++] = pObject;
                    }
                }
                else
                    // object is _not_ insertable:
                    // then we might need to unlock it
                    // V1.0.0 (2002-09-13) [umoeller]
                    if (flInsert & INSERT_UNLOCKFILTERED)
                        _wpUnlockObject(pObject);
            }

            PMPF_POPULATESPLITVIEW(("--> got %d objects to insert", cObjects));

            if (cObjects)
                _wpclsInsertMultipleObjects(_somGetClass(pFolder),
                                            hwndCnr,
                                            NULL,
                                            (PVOID*)papObjects,
                                            precParent,
                                            cObjects);

            if (papObjects)
                free(papObjects);
        }
    }
    CATCH(excpt1)
    {
    } END_CATCH();

    if (fFolderLocked)
        _wpReleaseFolderMutexSem(pFolder);

    return cInsertable;
}

/* ******************************************************************
 *
 *   Split view main control (main frame's client)
 *
 ********************************************************************/

STATIC MRESULT EXPENTRY fnwpFilesFrame(HWND hwndFrame, ULONG msg, MPARAM mp1, MPARAM mp2);

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
 *      creates all the subcontrols of the main controller
 *      window, that is, the split window with the two
 *      subframes and containers.
 *
 *      Returns NULL if no error occurred. As a result,
 *      the return value can be returned from WM_CREATE,
 *      which stops window creation if != 0 is returned.
 *
 *@@changed V1.0.9 (2011-10-16) [rwalsh]: added CA_MIXEDTARGETEMPH for files container
 *@@changed V1.0.11 (2016-08-12) [rwalsh]: added support for details view
 */

MPARAM fdrSetupSplitView(HWND hwnd,
                         PFDRSPLITVIEW psv)
{
    MPARAM mrc = (MPARAM)FALSE;         // return value of WM_CREATE: 0 == OK

    SPLITBARCDATA sbcd;
    HAB hab = WinQueryAnchorBlock(hwnd);

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
                                                (psv->flSplit & SPLIT_MULTIPLESEL)
                                                   ? CCS_MINIICONS | CCS_EXTENDSEL
                                                   : CCS_MINIICONS | CCS_SINGLESEL,
                                                &psv->hwndFilesCnr);

    if (psv->fIsViewer)
    {
        // init viewer settings and enable the selected view
        // added V1.0.11 (2018-08-01) [rwalsh]
        xvwInitXview(psv);
    }
    else
    {
        BEGIN_CNRINFO()
        {
            // changed V1.0.9 (2011-10-16) [rwalsh]: added CA_MIXEDTARGETEMPH
            cnrhSetView(   CV_NAME | CV_FLOW
                         | CV_MINI | CA_MIXEDTARGETEMPH);
            cnrhSetTreeIndent(30);
            cnrhSetSortFunc(fnCompareNameFoldersFirst);     // shared/cnrsort.c
        } END_CNRINFO(psv->hwndFilesCnr);

        winhSetWindowFont(psv->hwndTreeCnr, cmnQueryDefaultFont());
        winhSetWindowFont(psv->hwndFilesCnr, cmnQueryDefaultFont());

        // set the window font for the main client...
        // all controls will inherit this
        winhSetWindowFont(hwnd, cmnQueryDefaultFont());
    }

    // create split window
    sbcd.ulSplitWindowID = 1;
        // split window becomes client of main frame
    sbcd.ulCreateFlags =   SBCF_VERTICAL
                         | SBCF_PERCENTAGE
                         | SBCF_3DEXPLORERSTYLE
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
                   (MPARAM)psv->hwndTreeFrame,      // left
                   (MPARAM)psv->hwndFilesFrame);    // right
    }

    // create the "populate" thread
    thrCreate(&psv->tiSplitPopulate,
              fntSplitPopulate,
              &psv->tidSplitPopulate,
              "SplitPopulate",
              THRF_PMMSGQUEUE | THRF_WAIT_EXPLICIT,
                        // we MUST wait until the thread
                        // is ready; populate posts event
                        // sem when it has created its obj wnd
              (ULONG)psv);
    // this will wait until the object window has been created

    return mrc;
}

/*
 *@@ SplitSendWMControl:
 *
 *@@added V1.0.0 (2002-09-13) [umoeller]
 */

STATIC MRESULT SplitSendWMControl(PFDRSPLITVIEW psv,
                                  USHORT usNotifyCode,
                                  MPARAM mp2)
{
    return WinSendMsg(psv->hwndMainFrame,
                      WM_CONTROL,
                      MPFROM2SHORT(FID_CLIENT,
                                   usNotifyCode),
                      mp2);
}

/*
 *@@ fnwpSplitController:
 *      window proc for the split view controller, which is
 *      the client of the split view's main frame and has
 *      its own window class (WC_SPLITCONTROLLER).
 *
 *      This is really the core of the split view that
 *      handles the cooperation of the two containers and
 *      manages populate.
 *
 *      In addition, this gives the frame (its parent) a
 *      chance to fine-tune the behavior of the split view.
 *      As a result, the user code (the "real" folder split
 *      view, or the file dialog) can subclass the frame
 *      and thus influence how the split view works.
 *
 *      In detail:
 *
 *      --  We send WM_CONTROL with SN_FOLDERCHANGING,
 *          SN_FOLDERCHANGED, SN_OBJECTSELECTED, or
 *          SN_OBJECTENTER to the frame whenever selections
 *          change.
 *
 *      --  We make sure that the frame gets a chance
 *          to process WM_CLOSE. The problem is that
 *          with PM, only the client gets WM_CLOSE if
 *          a client exists. So we forward (post) that
 *          to the frame when we get it, and the frame
 *          can take action.
 *
 *@@changed V1.0.6 (2006-10-16) [pr]: fixed show/maximize from minimize @@fixes 865
 */

MRESULT EXPENTRY fnwpSplitController(HWND hwndClient, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT         mrc = 0;
    PFDRSPLITVIEW   psv;

    TRY_LOUD(excpt1)
    {
        switch (msg)
        {
            /*
             * WM_CREATE:
             *      we get PFDRSPLITVIEW in mp1.
             */

            case WM_CREATE:
                WinSetWindowPtr(hwndClient, QWL_USER, mp1);

                mrc = fdrSetupSplitView(hwndClient,
                                        (PFDRSPLITVIEW)mp1);
            break;

            /*
             * WM_WINDOWPOSCHANGED:
             *
             */

            case WM_WINDOWPOSCHANGED:
                if (((PSWP)mp1)->fl & SWP_SIZE)
                {
                    if (    (psv = WinQueryWindowPtr(hwndClient, QWL_USER))
                            // file dialog handles sizing itself
                         && (!(psv->flSplit & SPLIT_NOAUTOPOSITION))
                       )
                    {
                        // adjust size of "split window",
                        // which will rearrange all the linked
                        // windows (comctl.c)
                        WinSetWindowPos(psv->hwndSplitWindow,
                                        HWND_TOP,
                                        0,
                                        0,
                                        ((PSWP)mp1)->cx,
                                        ((PSWP)mp1)->cy,
                                        SWP_SIZE);
                    }
                }

                // return default NULL
            break;

            /*
             * WM_MINMAXFRAME:
             *      when minimizing, we hide the "split window",
             *      because otherwise the child dialogs will
             *      display garbage
             */

            case WM_MINMAXFRAME:
                if (psv = WinQueryWindowPtr(hwndClient, QWL_USER))
                {
                    PSWP pswp = (PSWP)mp1;
                    if (pswp->fl & SWP_MINIMIZE)
                        WinShowWindow(psv->hwndSplitWindow, FALSE);
                    else if (pswp->fl & (SWP_MAXIMIZE | SWP_RESTORE))  // V1.0.6 (2006-10-16) [pr]: @@fixes 865
                        WinShowWindow(psv->hwndSplitWindow, TRUE);
                }
            break;

            /*
             *@@ FM_FILLFOLDER:
             *      posted to the main control from the tree
             *      frame to fill the dialog when a new folder
             *      has been selected in the tree.
             *
             *      Use fdrPostFillFolder for posting this
             *      message, which is type-safe.
             *
             *      This automatically offloads populate
             *      to fntSplitPopulate, which will then post
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
             *          populate the folder with subfolders only.
             *          The files list is not changed.
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
             *          the record in the tree after
             *          populate and run "add first child" for
             *          each subrecord that was inserted.
             *
             *      --  If FFL_SETBACKGROUND is set, we will
             *          revamp the files container to use the
             *          view settings of the given folder.
             *
             *@@added V0.9.18 (2002-02-06) [umoeller]
             */

            case FM_FILLFOLDER:

                if (psv = WinQueryWindowPtr(hwndClient, QWL_USER))
                {
                    PMINIRECORDCORE prec = (PMINIRECORDCORE)mp1;

                    // V1.0.4 (2005-07-16) [pr]
                    PMPF_POPULATESPLITVIEW(("FM_FILLFOLDER %s", prec ? prec->pszIcon : "NULL"));

                    DumpFlags((ULONG)mp2);

                    if (!((ULONG)mp2 & FFL_FOLDERSONLY))
                    {
                        // not folders-only: then we need to
                        // refresh the files list

                        PMPF_POPULATESPLITVIEW(("  calling fdrvClearContainer"));

                        // disable the whitespace context menu and
                        // CN_EMPHASIS notifications until all objects
                        // are inserted
                        psv->precFilesShowing = NULL;

                        // notify frame that we're busy
                        // changed V1.0.11 (2016-09-29) [rwalsh] -
                        // send prec, not the folder it resolves to
                        SplitSendWMControl(psv,
                                           SN_FOLDERCHANGING,
                                           prec);

                        fdrvClearContainer(psv->hwndFilesCnr,
                                           (psv->fUnlockOnClear)
                                               ? CLEARFL_UNLOCKOBJECTS
                                               : 0);

                        // if we had a previous view item for the
                        // files cnr, remove it... since the entire
                        // psv structure was initially zeroed, this
                        // check is safe
                        if (psv->pobjUseList)
                            _wpDeleteFromObjUseList(psv->pobjUseList,
                                                    &psv->uiDisplaying);

                        // register a view item for the object
                        // that was selected in the tree so that
                        // it is marked as "open" and the refresh
                        // thread can handle updates for it too;
                        // use the OBJECT, not the folder derived
                        // from it, since for disk objects, the
                        // disks have the views, not the root folder
                        if (   prec  // V1.0.4 (2005-07-16) [pr]: @@fixes 530
                            &&
                            (psv->pobjUseList = OBJECT_FROM_PREC(prec))
                           )
                        {
                            psv->uiDisplaying.type = USAGE_OPENVIEW;
                            memset(&psv->viDisplaying, 0, sizeof(VIEWITEM));
                            psv->viDisplaying.view =
                                    *G_pulVarMenuOfs + ID_XFMI_OFS_SPLITVIEW_SHOWING;
                            psv->viDisplaying.handle = psv->hwndFilesFrame;
                                    // do not change this! XFolder::wpUnInitData
                                    // relies on this!
                            // set this flag so that we can disable
                            // _wpAddToContent for this view while we're
                            // populating; the flag is cleared once we're done
                            psv->viDisplaying.ulViewState = VIEWSTATE_OPENING;

                            _wpAddToObjUseList(psv->pobjUseList,
                                               &psv->uiDisplaying);

                            if ((ULONG)mp2 & FFL_SETBACKGROUND)
                            {
                                // changed V1.0.11 (2016-09-29) [rwalsh] -
                                // don't resolve prec until it's actually needed
                                WPFolder *pFolder =
                                    fdrvGetFSFromRecord(prec, TRUE); // folders only

                                // change files container background NOW
                                // to give user immediate feedback
                                // V1.0.0 (2002-09-24) [umoeller]: use
                                // msg because we need this code from XFolder
                                // method code now too
                                WinPostMsg(psv->hwndFilesFrame,
                                           FM_SETCNRLAYOUT,
                                           pFolder,
                                           0);
                            }
                        }
                    }

                    // mark this folder as "populating"
                    psv->precPopulating = prec;

                    // post FM2_POPULATE
                    if (prec)  // V1.0.4 (2005-07-16) [pr]: @@fixes 530
                        fdrSplitPopulate(psv,
                                         prec,
                                         (ULONG)mp2);
                }

            break;

            /*
             *@@ FM_POPULATED_FILLTREE:
             *      posted by fntSplitPopulate after populate has been
             *      done for a folder. This gets posted in any case,
             *      if the folder was populated in folders-only mode
             *      or not.
             *
             *      Parameters:
             *
             *      --  PMINIRECORDCODE mp1: record of folder
             *          (or disk or whatever) to fill from.
             *
             *      --  ULONG mp2: FFL_* flags for whether to
             *          expand.
             *
             *      If FFL_EXPAND was set the tree is expanded,
             *      then fdrInsertContents is called to insert
             *      subfolders into the tree.  Finally, we call
             *      fdrAddFirstChild so each subfolder gets a
             *      "+" next to it if appropriate.
             *
             *@@added V0.9.18 (2002-02-06) [umoeller]
             *@@changed V1.0.9 (2011-09-24) [rwalsh]: added call to fdrAddFirstChild
             */

            case FM_POPULATED_FILLTREE:
            {
                PMINIRECORDCORE prec;

                PMPF_POPULATESPLITVIEW(("FM_POPULATED_FILLTREE %s",
                            mp1
                                ? ((PMINIRECORDCORE)mp1)->pszIcon
                                : "NULL"));

                if (    (psv = WinQueryWindowPtr(hwndClient, QWL_USER))
                     && (prec = (PMINIRECORDCORE)mp1)
                   )
                {
                    // we're done populating
                    psv->precPopulating = NULL;

                    if (!((ULONG)mp2 & FFL_POPULATEFAILED))
                    {
                        ULONG     cRecs;
                        WPFolder  *pFolder = fdrvGetFSFromRecord(prec, TRUE);

                        if ((ULONG)mp2 & FFL_EXPAND)
                        {
                            // stop control notifications from messing with this
                            BOOL        fOld = psv->fSplitViewReady;

                            psv->fSplitViewReady = FALSE;
                            cnrhExpandFromRoot(psv->hwndTreeCnr,
                                               (PRECORDCORE)prec);
                            psv->fSplitViewReady = fOld;
                        }

                        // insert subfolders into tree on the left
                        cRecs = fdrInsertContents(pFolder,
                                                  psv->hwndTreeCnr,
                                                  prec,
                                                  INSERT_FOLDERSANDDISKS,
                                                  NULL);      // file mask

                        // if prec has subfolders and it is expanded,
                        // insert each subfolder's first child folder
                        if (cRecs && ((ULONG)mp2 & FFL_EXPAND))
                            fdrAddFirstChild(prec,
                                             psv->hwndTreeCnr,
                                             psv->hwndSplitPopulate);
                    }
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

                if (psv = WinQueryWindowPtr(hwndClient, QWL_USER))
                {
                    BOOL    fOld = psv->fSplitViewReady;
                    ULONG   ul;

                    PMPF_POPULATESPLITVIEW(("FM_POPULATED_SCROLLTO %s",
                                mp1
                                    ? ((PMINIRECORDCORE)mp1)->pszIcon
                                    : "NULL"));

                    // stop control notifications from messing with this
                    psv->fSplitViewReady = FALSE;

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
                    psv->fSplitViewReady = fOld;
                }

            break;

            /*
             *@@ FM_POPULATED_FILLFILES:
             *      posted by fntSplitPopulate after populate has been
             *      done for the newly selected folder, if this
             *      was not in folders-only mode. We must then fill
             *      the right half of the dialog with all the objects.
             *
             *      Parameters:
             *
             *      --  PMINIRECORDCODE mp1: record of folder
             *          (or disk or whatever) to fill with.
             *          Use fdrvGetFSFromRecord(mp1, TRUE) to
             *          get the real folder since this might
             *          be a shadow or disk object.
             *
             *      --  ULONG mp2: FFL_* flags. In addition to
             *          the flags that were initially passed with
             *          FM_FILLFOLDER, this will have the
             *          FFL_UNLOCKOBJECTS bit set if the folder
             *          was freshly populated and objects should
             *          therefore be unlocked, or FFL_POPULATEFAILED
             *          if populate failed and we should simply
             *          refresh the view state without actually
             *          inserting anything.
             *
             *@@added V0.9.18 (2002-02-06) [umoeller]
             *@@changed V1.0.0 (2002-09-13) [umoeller]: changed mp2 definition
             */

            case FM_POPULATED_FILLFILES:

                if (psv = WinQueryWindowPtr(hwndClient, QWL_USER))
                {
                    PMPF_POPULATESPLITVIEW(("FM_POPULATED_FILLFILES %s",
                                mp1
                                    ? ((PMINIRECORDCORE)mp1)->pszIcon
                                    : "NULL"));

                    if (mp1)
                    {
                        WPFolder    *pFolder = NULL;

                        psv->fUnlockOnClear = FALSE;

                        if (    (!((ULONG)mp2 & FFL_POPULATEFAILED))
                             && (pFolder = fdrvGetFSFromRecord((PMINIRECORDCORE)mp1,
                                                               TRUE))
                           )
                        {
                            CHAR    szPathName[2*CCHMAXPATH];
                            ULONG   flInsert;

                            // if a file mask is specified, then we're
                            // working for the file dialog, and should
                            // only insert filesystem objects
                            if (psv->pcszFileMask)
                                flInsert = INSERT_FILESYSTEMS;
                            else
                                flInsert = INSERT_ALL;

                            PMPF_POPULATESPLITVIEW(("   FFL_UNLOCKOBJECTS is 0x%lX", ((ULONG)mp2 & FFL_UNLOCKOBJECTS)));

                            if ((ULONG)mp2 & FFL_UNLOCKOBJECTS)
                            {
                                flInsert |= INSERT_UNLOCKFILTERED;
                                psv->fUnlockOnClear = TRUE;
                            }

                            // insert all contents into list on the right
                            fdrInsertContents(pFolder,
                                              psv->hwndFilesCnr,
                                              NULL,        // parent
                                              flInsert,
                                              psv->pcszFileMask);
                                                    // ptr to file mask buffer if specified
                                                    // with fdrSplitCreateFrame; this is
                                                    // NULL unless we're working for the
                                                    // file dialog
                        }

                        // clear the "opening" flag in the VIEWITEM
                        // so that XFolder::wpAddToContent will start
                        // giving us new objects
                        psv->viDisplaying.ulViewState &= ~VIEWSTATE_OPENING;

                        // re-enable the whitespace context menu and
                        // CN_EMPHASIS notifications
                        psv->precFilesShowing = (PMINIRECORDCORE)mp1;

                        // update the folder pointers in the SFV
                        psv->psfvFiles->somSelf = pFolder;
                        psv->psfvFiles->pRealObject = OBJECT_FROM_PREC((PMINIRECORDCORE)mp1);

                        // notify frame that we're done being busy
                        // changed V1.0.11 (2016-09-29) [rwalsh] -
                        // send prec, not the folder it resolves to
                        SplitSendWMControl(psv,
                                           SN_FOLDERCHANGED,
                                           mp1);
                    }
                }

            break;

            /*
             * CM_UPDATEPOINTER:
             *      posted when threads exit etc. to update
             *      the current pointer.
             */

            case FM_UPDATEPOINTER:

                if (psv = WinQueryWindowPtr(hwndClient, QWL_USER))
                {
                    WinSetPointer(HWND_DESKTOP,
                                  fdrSplitQueryPointer(psv));
                }

            break;

            /*
             * WM_PAINT:
             *      file dialog needs us to paint the background
             *      because in the file dialog, we are not entirely
             *      covered by the split window. This code should
             *      never get called for the real split view.
             */

            case WM_PAINT:
            {
                HPS hps;
                if (hps = WinBeginPaint(hwndClient, NULLHANDLE, NULL))
                {
                    RECTL rcl;
                    WinQueryWindowRect(hwndClient, &rcl);
                    gpihSwitchToRGB(hps);
                    WinFillRect(hps,
                                &rcl,
                                winhQueryPresColor2(hwndClient,
                                                    PP_BACKGROUNDCOLOR,
                                                    PP_BACKGROUNDCOLORINDEX,
                                                    FALSE,
                                                    SYSCLR_DIALOGBACKGROUND));


                    WinEndPaint(hps);
                }
            }
            break;

            /*
             * WM_CLOSE:
             *      posts (!) the SN_FRAMECLOSE notification to
             *      the frame.
             *
             *      See SN_FRAMECLOSE in fdrsplit.h for
             *      details.
             */

            case WM_CLOSE:
                if (psv = WinQueryWindowPtr(hwndClient, QWL_USER))
                {
                    PMPF_POPULATESPLITVIEW(("WM_CLOSE --> sending SN_FRAMECLOSE"));

                    // post, do not send, or the file dialog
                    // will never see this in the WinGetMsg loop
                    WinPostMsg(psv->hwndMainFrame,
                               WM_CONTROL,
                               MPFROM2SHORT(FID_CLIENT,
                                            SN_FRAMECLOSE),
                               NULL);
                }

                // never pass this on, because the default
                // window proc posts WM_QUIT
            break;

            default:
                mrc = WinDefWindowProc(hwndClient, msg, mp1, mp2);
        }
    }
    CATCH(excpt1) {} END_CATCH();

    return mrc;
}

/* ******************************************************************
 *
 *   Left tree frame and client
 *
 ********************************************************************/

/*
 *@@ HandleWMChar:
 *      common WM_CHAR handler for both the tree and
 *      the files frame.
 *
 *      Returns TRUE if the msg was processed.
 *
 *@@added V1.0.0 (2002-11-23) [umoeller]
 */

STATIC BOOL HandleWMChar(HWND hwndFrame,
                         MPARAM mp1,
                         MPARAM mp2)
{
    HWND    hwndMainControl;
    USHORT  usFlags    = SHORT1FROMMP(mp1);
    USHORT  usvk       = SHORT2FROMMP(mp2);
    PFDRSPLITVIEW       psv;
    BOOL    fProcessed = FALSE;

    if (    (!(usFlags & KC_KEYUP))
         && (usFlags & KC_VIRTUALKEY)
         && (hwndMainControl = WinQueryWindow(hwndFrame, QW_OWNER))
         && (psv = WinQueryWindowPtr(hwndMainControl, QWL_USER))
       )
    {
        if (!(fProcessed = (BOOL)SplitSendWMControl(psv,
                                                    SN_VKEY,
                                                    (MPARAM)usvk)))
        {
            switch (usvk)
            {
                case VK_BACKSPACE:
                {
                    // get current selection in drives view
                    PMINIRECORDCORE prec;
                    if (prec = (PMINIRECORDCORE)WinSendMsg(
                                            psv->hwndTreeCnr,
                                            CM_QUERYRECORD,
                                            (MPARAM)psv->precTreeSelected,
                                            MPFROM2SHORT(CMA_PARENT,
                                                         CMA_ITEMORDER)))
                    {
                        // not at root already:
                        cnrhSelectRecord(psv->hwndTreeCnr,
                                         prec,
                                         TRUE);
                        fProcessed = TRUE;
                    }
                }
                break;
            }
        }
    }

    return fProcessed;
}

/*
 *@@ TreeFrameControl:
 *      implementation for WM_CONTROL for FID_CLIENT
 *      in fnwpTreeFrame.
 *
 *      Set *pfCallDefault to TRUE if you want the
 *      parent window proc to be called.
 *
 *@@added V1.0.0 (2002-08-26) [umoeller]
 *@@changed V1.0.5 (2006-04-13) [pr]: Fix status bar update @@fixes 326
 *@@changed V1.0.6 (2006-12-06) [pr]: added CN_SETFOCUS @@fixes 326
 */

STATIC MRESULT TreeFrameControl(HWND hwndFrame,
                                MPARAM mp1,
                                MPARAM mp2,
                                PBOOL pfCallDefault)
{
    MRESULT mrc = 0;
    HWND                hwndMainControl;
    PFDRSPLITVIEW       psv;
    PMINIRECORDCORE     prec;

    switch (SHORT2FROMMP(mp1))
    {
        /*
         * CN_EMPHASIS:
         *      selection changed:
         */

        case CN_EMPHASIS:
        {
            PNOTIFYRECORDEMPHASIS pnre = (PNOTIFYRECORDEMPHASIS)mp2;

            // V1.0.5 (2006-04-13) [pr]: Fix status bar update @@fixes 326
            if (    (pnre->fEmphasisMask & CRA_SELECTED)
                 && (hwndMainControl = WinQueryWindow(hwndFrame, QW_OWNER))
                 && (psv = WinQueryWindowPtr(hwndMainControl, QWL_USER))
                 // notifications not disabled?
                 && (psv->fSplitViewReady)
                 && (prec = (PMINIRECORDCORE)pnre->pRecord)
                 && (prec->flRecordAttr & CRA_SELECTED)
               )
            {
                PMPF_POPULATESPLITVIEW(("CN_EMPHASIS %s",
                        prec->pszIcon));

                // record changed?
                if (prec != psv->precTreeSelected)
                {
                    // then go refresh the files container
                    // with a little delay; see WM_TIMER below
                    // if the user goes thru a lot of records
                    // in the tree, this timer gets restarted
                    psv->precToPopulate
                        = psv->precTreeSelected
                        = prec;
                    WinStartTimer(psv->habGUI,
                                  hwndFrame,        // post to tree frame
                                  1,
                                  200);
                }

                if (psv->hwndStatusBar)
                {
                    PMPF_POPULATESPLITVIEW(("CN_EMPHASIS: posting STBM_UPDATESTATUSBAR to hwnd %lX",
                                psv->hwndStatusBar ));

                    // have the status bar updated and make
                    // sure the status bar retrieves its info
                    // from the _left_ cnr
                    WinPostMsg(psv->hwndStatusBar,
                               STBM_UPDATESTATUSBAR,
                               (MPARAM)psv->hwndTreeCnr,
                               MPNULL);
                }
            }
        }
        break;

        /*
         * CN_SETFOCUS:
         *      V1.0.6 (2006-12-06) [pr]
         */

        case CN_SETFOCUS:
            if (    (hwndMainControl = WinQueryWindow(hwndFrame, QW_OWNER))
                 && (psv = WinQueryWindowPtr(hwndMainControl, QWL_USER))
                 // notifications not disabled?
                 && (psv->fSplitViewReady)
                 && (psv->hwndStatusBar)
               )
            {
                PMPF_POPULATESPLITVIEW(("CN_SETFOCUS: posting STBM_UPDATESTATUSBAR to hwnd %lX",
                            psv->hwndStatusBar ));

                // have the status bar updated and make
                // sure the status bar retrieves its info
                // from the _left_ cnr
                psv->hwndFocusCnr = psv->hwndTreeCnr;  // V1.0.9 (2012-02-12) [pr]
                WinPostMsg(psv->hwndStatusBar,
                           STBM_UPDATESTATUSBAR,
                           (MPARAM)psv->hwndFocusCnr,
                           MPNULL);
            }
        break;

        /*
         * CN_EXPANDTREE:
         *      user clicked on "+" sign next to
         *      tree item; expand that, but start
         *      "add first child" thread again.
         *
         *      Since a record is automatically selected
         *      also when it is expanded, we get both
         *      CN_EMPHASIS and CN_EXPANDTREE, where
         *      CN_EMPHASIS comes in first, from my testing.
         *
         *      We have to differentiate between two cases:
         *
         *      --  If the user expands a record that was
         *          previously _not_ selected (or double-clicks
         *          on the tree record, which has the same
         *          effect), we only mark precTreeExpanded
         *          so that the WM_TIMER that was started by
         *          the previous CN_EMPHASIS can run "add first
         *          child" as well.
         *
         *      --  If the user expands a record that is
         *          currently selected, we did not get CN_EMPHASIS
         *          previously and need to run "add first child"
         *          manually.
         *
         *      V1.0.0 (2002-09-13) [umoeller]
         */

        case CN_EXPANDTREE:
            if (    (hwndMainControl = WinQueryWindow(hwndFrame, QW_OWNER))
                 && (psv = WinQueryWindowPtr(hwndMainControl, QWL_USER))
                 // notifications not disabled?
                 && (psv->fSplitViewReady)
                 && (prec = (PMINIRECORDCORE)mp2)
               )
            {
                PMPF_POPULATESPLITVIEW(("CN_EXPANDTREE for %s; precToPopulate is %s",
                        prec->pszIcon,
                        (psv->precToPopulate) ? psv->precToPopulate->pszIcon : "NULL"));

                psv->precTreeExpanded = prec;

                if (psv->precToPopulate != prec)
                    // the record that is being expanded has _not_ just
                    // been selected: run "add first child"
                    // V1.0.0 (2002-11-23) [umoeller]
                    fdrPostFillFolder(psv,
                                      prec,
                                      FFL_FOLDERSONLY | FFL_EXPAND);

                // and call default because xfolder
                // handles auto-scroll
                *pfCallDefault = TRUE;
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

        /*
         * CN_CONTEXTMENU:
         *      we need to intercept this for context menus
         *      on whitespace, because the WPS won't do it.
         *      We pass all other cases on because the WPS
         *      does do things correctly for object menus.
         */

        case CN_CONTEXTMENU:
        {
            *pfCallDefault = TRUE;

            if (    (hwndMainControl = WinQueryWindow(hwndFrame, QW_OWNER))
                 && (psv = WinQueryWindowPtr(hwndMainControl, QWL_USER))
                 && (!mp2)      // whitespace:
                    // display the menu for the root folder
               )
            {
#if 1
                POINTL  ptl;
                WinQueryPointerPos(HWND_DESKTOP, &ptl);
                // convert to cnr coordinates
                WinMapWindowPoints(HWND_DESKTOP,        // from
                                   psv->hwndTreeCnr,   // to
                                   &ptl,
                                   1);
                _wpDisplayMenu(psv->pRootObject,
                               psv->hwndTreeFrame, // owner
                               psv->hwndTreeCnr,   // parent
                               &ptl,
                               MENU_OPENVIEWPOPUP,
                               0);
#endif

                *pfCallDefault = FALSE;
            }
        }
        break;

        default:
            *pfCallDefault = TRUE;
    }

    return mrc;
}

/*
 *@@ fnwpTreeFrame:
 *      subclassed frame window on the right for the
 *      "Files" container. This has the files cnr
 *      as its FID_CLIENT.
 *
 *      We use the XFolder subclassed window proc for
 *      most messages. In addition, we intercept a
 *      couple more for extra features.
 *
 *@@changed V1.0.0 (2002-11-23) [umoeller]: brought back keyboard support
 *@@changed V1.0.9 (2011-09-24) [rwalsh]: added FM_INSERTFOLDER @@fixes 2
 */

STATIC MRESULT EXPENTRY fnwpTreeFrame(HWND hwndFrame, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT             mrc = 0;

    TRY_LOUD(excpt1)
    {
        BOOL                fCallDefault = FALSE;
        PSUBCLFOLDERVIEW    psfv = WinQueryWindowPtr(hwndFrame, QWL_USER);
        HWND                hwndMainControl;
        PFDRSPLITVIEW       psv;
        PMINIRECORDCORE     prec;

        switch (msg)
        {
            case WM_CONTROL:
                if (SHORT1FROMMP(mp1) == FID_CLIENT)     // that's the container
                    mrc = TreeFrameControl(hwndFrame,
                                           mp1,
                                           mp2,
                                           &fCallDefault);
                else
                    fCallDefault = TRUE;
            break;

            case WM_TIMER:
                if (    ((ULONG)mp1 == 1)
                     && (hwndMainControl = WinQueryWindow(hwndFrame, QW_OWNER))
                     && (psv = WinQueryWindowPtr(hwndMainControl, QWL_USER))
                   )
                {
                    // timer 1 gets (re)started every time the user
                    // selects a record in three (see CN_EMPHASIS above);
                    // we use this to avoid crowding the populate thread
                    // with populate messages if the user is using the
                    // keyboard to run down the tree; CN_EMPHASIS only
                    // sets psv->precFolderToPopulate and starts this
                    // timer.

                    PMPF_POPULATESPLITVIEW(("WM_TIMER 1, precFolderPopulating is 0x%lX (%s)",
                                psv->precPopulating,
                                (psv->precPopulating)
                                    ? psv->precPopulating->pszIcon
                                    : "NULL"));

                    // If we're still busy populating something,
                    // keep the timer running and do nothing.
                    if (!psv->precPopulating)
                    {
                        // then stop the timer
                        WinStopTimer(psv->habGUI,
                                     hwndFrame,
                                     1);

                        // fire populate twice:

                        // 1) full populate for the files cnr
                        PMPF_POPULATESPLITVIEW(("posting FM_FILLFOLDER for files cnr"));
                        fdrPostFillFolder(psv,
                                          psv->precToPopulate,
                                          FFL_SETBACKGROUND);

                        if (psv->precToPopulate == psv->precTreeExpanded)
                        {
                            // 2) this record was also expanded: fire another
                            //    populate so that the grandchildren get added
                            //    to the tree
                            PMPF_POPULATESPLITVIEW(("posting second FM_FILLFOLDER for expanding tree"));
                            fdrPostFillFolder(psv,
                                              psv->precToPopulate,
                                              FFL_FOLDERSONLY | FFL_SCROLLTO | FFL_EXPAND);
                        }

                        psv->precToPopulate = NULL;
                    }

                    // reset "expanded" flag
                    psv->precTreeExpanded = NULL;
                }
            break;

            case WM_SYSCOMMAND:
                // forward to main frame
                WinPostMsg(WinQueryWindow(WinQueryWindow(hwndFrame, QW_OWNER),
                                          QW_OWNER),
                           msg,
                           mp1,
                           mp2);
            break;

            case WM_CHAR:
                fCallDefault = !HandleWMChar(hwndFrame, mp1, mp2);
            break;

            /*
             * WM_QUERYOBJECTPTR:
             *      we receive this message from the WPS somewhere
             *      when it tries to process menu items for the
             *      whitespace context menu. I guess normally this
             *      message is taken care of when a frame is registered
             *      as a folder view, but this frame is not. So we must
             *      answer by returning the folder that this frame
             *      represents.
             *
             *      Answering this message will enable all WPS whitespace
             *      magic: both displaying the whitespace context menu
             *      and making messages work for the whitespace menu items.
             */

            case WM_QUERYOBJECTPTR:
                PMPF_POPULATESPLITVIEW(("WM_QUERYOBJECTPTR"));
                if (    (hwndMainControl = WinQueryWindow(hwndFrame, QW_OWNER))
                     && (psv = WinQueryWindowPtr(hwndMainControl, QWL_USER))
                   )
                    mrc = (MRESULT)psv->pRootObject;
            break;

            /*
             * WM_CONTROLPOINTER:
             *      show wait pointer if we're busy.
             */

            case WM_CONTROLPOINTER:
                if (    (hwndMainControl = WinQueryWindow(hwndFrame, QW_OWNER))
                     && (psv = WinQueryWindowPtr(hwndMainControl, QWL_USER))
                   )
                    mrc = (MPARAM)fdrSplitQueryPointer(psv);
            break;

            case FM_INSERTOBJECT:
                PMPF_POPULATESPLITVIEW(("fnwpTreeFrame received FM_INSERTOBJECT in error"));
            break;

            /*
             * FM_INSERTFOLDER:
             *      notifies the tree container of a folder that
             *      may need to be inserted in the tree.
             *
             *      It is posted primarily by fdrSplitInsertObject
             *      when a folder or disk is awakened.  It may also
             *      be posted by the split-populate thread when a
             *      folder or disk that's already awake should be
             *      inserted as the first child of a folder that's
             *      already in the tree.
             *
             *      --  WPObject* mp1: a folder or disk object to
             *          insert
             *
             *      --  WPFolder* mp2: the folder containing mp1
             *
             *      The object in mp1 is only inserted if its parent
             *      (mp2) has no children yet (and thus, has no "+"
             *      next to it yet), or if the parent is expanded
             *      so that the object will be visible when inserted.
             *
             *      Regardless of whether the object is inserted in
             *      the tree, an FM_INSERTOBJECT message is sent to
             *      the files container if the parent folder's
             *      contents are visible there.
             *
             *      See fdrSplitInsertObject and FindFirstChild for
             *      additional details.
             *
             * added V1.0.9 (2011-09-24) [rwalsh]: @@fixes 2
             */
            case FM_INSERTFOLDER:
            {
                PMINIRECORDCORE precParent;
                PMINIRECORDCORE precChild;

                // preparation
                if (   !(hwndMainControl = WinQueryWindow(hwndFrame, QW_OWNER))
                    || !(psv = WinQueryWindowPtr(hwndMainControl, QWL_USER))
                    || !(mp1)
                    || !(mp2)
                    || !(precParent = _wpQueryCoreRecord((WPObject*)mp2))
                   )
                    break;

                // Insert the record if the parent folder is either childless
                // or expanded, and the folder object is insertable.
                // Note:  we must test for "childless" first, then "expanded"
                if (   (WinSendMsg(psv->hwndTreeCnr,
                                   CM_QUERYRECORDINFO,
                                   &precParent,
                                   (MPARAM)1))
                    && (   !(precChild = WinSendMsg(psv->hwndTreeCnr,
                                                    CM_QUERYRECORD,
                                                    (MPARAM)precParent,
                                                    MPFROM2SHORT(CMA_FIRSTCHILD,
                                                                 CMA_ITEMORDER)))
                        || (precParent->flRecordAttr & CRA_EXPANDED)
                       )
                    && (fdrvIsInsertable((WPObject*)mp1,
                                         INSERT_FOLDERSANDDISKS,
                                         NULL))
                   )
                {
                    POINTL ptl = {0, 0};

                    // prevent childless folders from expanding after insertion
                    if (   !(precChild)
                        && (precParent->flRecordAttr & CRA_EXPANDED)
                       )
                        WinSendMsg(psv->hwndTreeCnr,
                                   CM_COLLAPSETREE,
                                   (MPARAM)precParent,
                                   NULL);

                    // do it
                    _wpCnrInsertObject((WPObject*)mp1,
                                       psv->hwndTreeCnr,
                                       &ptl,
                                       precParent,
                                       NULL);
                }

                // if the parent's contents are showing on the right,
                // insert the new folder there as well
                if (precParent == psv->precFilesShowing)
                    WinSendMsg(psv->hwndFilesFrame,
                               FM_INSERTOBJECT,
                               mp1,
                               mp2);
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
    }
    CATCH(excpt1) {} END_CATCH();

    return mrc;
}

/* ******************************************************************
 *
 *   Right files frame and client
 *
 ********************************************************************/

/*
 *@@ FilesFrameControl:
 *      implementation for WM_CONTROL for FID_CLIENT
 *      in fnwpFilesFrame.
 *
 *      Set *pfCallDefault to TRUE if you want the
 *      parent window proc to be called.
 *
 *@@added V1.0.0 (2002-08-26) [umoeller]
 *@@changed V1.0.5 (2006-04-13) [pr]: Fix status bar update @@fixes 326
 *@@changed V1.0.6 (2006-12-06) [pr]: added CN_SETFOCUS and delay timer @@fixes 326
 */

STATIC MRESULT FilesFrameControl(HWND hwndFrame,
                                 MPARAM mp1,
                                 MPARAM mp2,
                                 PBOOL pfCallDefault)
{
    MRESULT mrc = 0;
    HWND                hwndMainControl;
    PFDRSPLITVIEW       psv;

    switch (SHORT2FROMMP(mp1))
    {
        /*
         * CN_EMPHASIS:
         *      selection changed: refresh
         *      the status bar.
         */

        case CN_EMPHASIS:
        {
            PNOTIFYRECORDEMPHASIS pnre = (PNOTIFYRECORDEMPHASIS)mp2;
            PMINIRECORDCORE prec;

            // V1.0.5 (2006-04-13) [pr]: Fix status bar update @@fixes 326
            if (    (pnre->fEmphasisMask & (CRA_SELECTED | CRA_CURSORED))
                 && (hwndMainControl = WinQueryWindow(hwndFrame, QW_OWNER))
                 && (psv = WinQueryWindowPtr(hwndMainControl, QWL_USER))
                 // notifications not disabled?
                 && (psv->fSplitViewReady)
                 && (prec = (PMINIRECORDCORE)pnre->pRecord)
                    // and we're not currently populating?
                    // (the cnr automatically selects the first obj
                    // that gets inserted, and we'd rather not have
                    // the file dialog react to such auto-selections);
                    // precFilesShowing is TRUE only after all objects
                    // have been inserted
                 && (psv->precFilesShowing)
               )
            {
                PMPF_POPULATESPLITVIEW(("CN_EMPHASIS [%s]",
                                         prec->pszIcon));
                if (prec->flRecordAttr & CRA_SELECTED)
                    SplitSendWMControl(psv,
                                       SN_OBJECTSELECTED,
                                       prec);

                if (psv->hwndStatusBar)
                {
                    PMPF_POPULATESPLITVIEW(("CN_EMPHASIS: starting Timer 1"));

                    // V1.0.6 (2006-12-06) [pr]
                    // delay status bar update otherwise we get a flash from the
                    // CN_SETFOCUS message if the selections in the container are
                    // changing
                    WinStartTimer(psv->habGUI,
                                  hwndFrame,        // post to files frame
                                  1,
                                  50);
                }
            }
        }
        break;

        /*
         * CN_SETFOCUS:
         *      V1.0.6 (2006-12-06) [pr]
         */

        case CN_SETFOCUS:
            if (    (hwndMainControl = WinQueryWindow(hwndFrame, QW_OWNER))
                 && (psv = WinQueryWindowPtr(hwndMainControl, QWL_USER))
                 && (psv->hwndStatusBar)
               )
            {
                PMPF_POPULATESPLITVIEW(("CN_SETFOCUS: starting Timer 1"));

                WinStartTimer(psv->habGUI,
                              hwndFrame,        // post to files frame
                              1,
                              50);
            }
        break;

        /*
         * CN_ENTER:
         *      double-click on tree record: intercept
         *      folders so we can influence the tree
         *      view on the right.
         */

        case CN_ENTER:
        {
            PNOTIFYRECORDENTER pnre;
            PMINIRECORDCORE prec;

            if (    (pnre = (PNOTIFYRECORDENTER)mp2)
                 && (prec = (PMINIRECORDCORE)pnre->pRecord)
                            // can be null for whitespace!
                 && (hwndMainControl = WinQueryWindow(hwndFrame, QW_OWNER))
                 && (psv = WinQueryWindowPtr(hwndMainControl, QWL_USER))
               )
            {
                // send SN_OBJECTENTER to the frame to see if
                // the frame wants a custom action. If that
                // returns TRUE, that means "processed", and
                // we do nothing (file dlg processes this itself).
                // Otherwise we try to be smart and do something.
                // changed V1.0.11 (2016-09-28) [rwalsh] -
                // dblclicking on a folder failed to display that folder
                if (!SplitSendWMControl(psv,
                                        SN_OBJECTENTER,
                                        (MPARAM)prec))
                {
                    WPObject *pobj;
                    WPFolder *pFolder;
                    if (    (pobj = fdrvGetFSFromRecord(prec, TRUE))    // folders only:
                         && (psv->precFilesShowing)
                         && (pFolder = fdrvGetFSFromRecord(psv->precFilesShowing, TRUE))
                       )
                    {
                        // double click on folder:
                        // if this is a _direct_ subfolder of the folder
                        // that we are currently displaying, that's easy
                        if (_wpQueryFolder(pobj) == pFolder)
                        {
                            WinPostMsg(psv->hwndTreeCnr,
                                       CM_EXPANDTREE,
                                       (MPARAM)psv->precFilesShowing,
                                       MPNULL);
                            WinPostMsg(psv->hwndTreeCnr,
                                       CM_SETRECORDEMPHASIS,
                                       (MPARAM)prec,
                                       MPFROM2SHORT(CRA_SELECTED, CRA_SELECTED));
                        }
                        else
                            // not a direct child (shadow probably):
                            // call parent (have WPS open default view)
                            *pfCallDefault = TRUE;
                    }
                    else
                        // not folder:
                        *pfCallDefault = TRUE;
                }
                // else frame handled this message: do nothing,
                // do not even call parent
            }
        }
        break;

        /*
         * CN_CONTEXTMENU:
         *      we need to intercept this for context menus
         *      on whitespace, because the WPS won't do it.
         *      We pass all other cases on because the WPS
         *      does do things correctly for object menus.
         */

        case CN_CONTEXTMENU:
        {
            *pfCallDefault = TRUE;

            if (    (hwndMainControl = WinQueryWindow(hwndFrame, QW_OWNER))
                 && (psv = WinQueryWindowPtr(hwndMainControl, QWL_USER))
                    // whitespace?
                 && (!mp2)
               )
            {
                // ok, this is a whitespace menu: if the view is
                // NOT populated, then just swallow for safety
                if (psv->precFilesShowing)
                {
                    POINTL  ptl;
                    WinQueryPointerPos(HWND_DESKTOP, &ptl);
                    // convert to cnr coordinates
                    WinMapWindowPoints(HWND_DESKTOP,        // from
                                       psv->hwndFilesCnr,   // to
                                       &ptl,
                                       1);
                    _wpDisplayMenu(OBJECT_FROM_PREC(psv->precFilesShowing),
                                   psv->hwndFilesFrame, // owner
                                   psv->hwndFilesCnr,   // parent
                                   &ptl,
                                   MENU_OPENVIEWPOPUP,
                                   0);
                }

                *pfCallDefault = FALSE;
            }
        }
        break;

        default:
            *pfCallDefault = TRUE;
    }

    return mrc;
}

/*
 *@@ fnwpFilesFrame:
 *      subclassed frame window on the right for the
 *      "Files" container. This has the tree cnr
 *      as its FID_CLIENT.
 *
 *      We use the XFolder subclassed window proc for
 *      most messages. In addition, we intercept a
 *      couple more for extra features.
 *
 *@@changed V1.0.0 (2002-11-23) [umoeller]: brought back keyboard support
 *@@changed V1.0.6 (2006-12-06) [pr]: added timer 1 delay @@fixes 326
 *@@changed V1.0.9 (2011-09-24) [rwalsh]: added FM_INSERTOBJECT @@fixes 2
 *@@changed V1.0.11 (2016-08-12) [rwalsh]: added WM_DRAWITEM to support details view
 */

STATIC MRESULT EXPENTRY fnwpFilesFrame(HWND hwndFrame, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT             mrc = 0;

    TRY_LOUD(excpt1)
    {
        BOOL                fCallDefault = FALSE;
        PSUBCLFOLDERVIEW    psfv = WinQueryWindowPtr(hwndFrame, QWL_USER);
        HWND                hwndMainControl;
        PFDRSPLITVIEW       psv;

        switch (msg)
        {
            case WM_CONTROL:
                if (SHORT1FROMMP(mp1) == FID_CLIENT)     // that's the container
                    mrc = FilesFrameControl(hwndFrame,
                                            mp1,
                                            mp2,
                                            &fCallDefault);
                else
                    fCallDefault = TRUE;
            break;

            // V1.0.6 (2006-12-06) [pr]
            case WM_TIMER:
                if (    ((ULONG)mp1 == 1)
                     && (hwndMainControl = WinQueryWindow(hwndFrame, QW_OWNER))
                     && (psv = WinQueryWindowPtr(hwndMainControl, QWL_USER))
                   )
                {
                    PMPF_POPULATESPLITVIEW(("WM_TIMER 1: posting STBM_UPDATESTATUSBAR to hwnd %lX",
                                psv->hwndStatusBar ));

                    WinStopTimer(psv->habGUI,
                                 hwndFrame,
                                 1);

                    if (   // notifications not disabled?
                           (psv->fSplitViewReady)
                        && (psv->precFilesShowing)
                        && (psv->hwndStatusBar)
                       )
                    {
                        // have the status bar updated and make
                        // sure the status bar retrieves its info
                        // from the _right_ cnr
                        psv->hwndFocusCnr = psv->hwndFilesCnr;  // V1.0.9 (2012-02-12) [pr]
                        WinPostMsg(psv->hwndStatusBar,
                                   STBM_UPDATESTATUSBAR,
                                   (MPARAM)psv->hwndFocusCnr,
                                   MPNULL);
                    }
                }
            break;

            case WM_SYSCOMMAND:
                // forward to main frame
                WinPostMsg(WinQueryWindow(WinQueryWindow(hwndFrame,
                                                         QW_OWNER),
                                          QW_OWNER),
                           msg,
                           mp1,
                           mp2);
            break;

            case WM_CHAR:
                fCallDefault = !HandleWMChar(hwndFrame, mp1, mp2);
            break;

            /*
             * WM_QUERYOBJECTPTR:
             *      we receive this message from the WPS somewhere
             *      when it tries to process menu items for the
             *      whitespace context menu. I guess normally this
             *      message is taken care of when a frame is registered
             *      as a folder view, but this frame is not. So we must
             *      answer by returning the folder that this frame
             *      represents.
             *
             *      Answering this message will enable all WPS whitespace
             *      magic: both displaying the whitespace context menu
             *      and making messages work for the whitespace menu items.
             */

            case WM_QUERYOBJECTPTR:
                PMPF_POPULATESPLITVIEW(("WM_QUERYOBJECTPTR"));
                if (    (hwndMainControl = WinQueryWindow(hwndFrame, QW_OWNER))
                     && (psv = WinQueryWindowPtr(hwndMainControl, QWL_USER))
                     // return the showing folder only if it is done
                     // populating
                     && (psv->precFilesShowing)
                   )
                    mrc = (MRESULT)OBJECT_FROM_PREC(psv->precFilesShowing);
                // else return default null
            break;

            /*
             * WM_CONTROLPOINTER:
             *      show wait pointer if we're busy.
             */

            case WM_CONTROLPOINTER:
                if (    (hwndMainControl = WinQueryWindow(hwndFrame, QW_OWNER))
                     && (psv = WinQueryWindowPtr(hwndMainControl, QWL_USER))
                   )
                    mrc = (MPARAM)fdrSplitQueryPointer(psv);
            break;

            /*
             *@@ FM_DELETINGFDR:
             *      this message is sent from XFolder::wpUnInitData
             *      if it finds the ID_XFMI_OFS_SPLITVIEW_SHOWING
             *      useitem in the folder. In other words, the
             *      folder whose contents are currently showing
             *      in the files cnr is about to go dormant.
             *      We must null the pointers in the splitviewdata
             *      that point to ourselves, or we'll have endless
             *      problems.
             *
             *@@added V1.0.0 (2002-08-28) [umoeller]
             */

            case FM_DELETINGFDR:
                PMPF_POPULATESPLITVIEW(("FM_DELETINGFDR"));
                if (    (hwndMainControl = WinQueryWindow(hwndFrame, QW_OWNER))
                     && (psv = WinQueryWindowPtr(hwndMainControl, QWL_USER))
                   )
                {
                    psv->pobjUseList = NULL;
                    psv->precFilesShowing = NULL;
                }
            break;

            /*
             *@@ FM_SETCNRLAYOUT:
             *      this message gets posted (!) when the folder
             *      view needs to have its container background
             *      set. This gets posted from two locations:
             *
             *      --  fnwpSplitController when FM_FILLFOLDER
             *          comes in to set the background before
             *          we start populate;
             *
             *      --  our XFolder::wpRedrawFolderBackground
             *          override when folder background settings
             *          change.
             *
             *      Note: Both fnwpFilesFrame and fnwpSplitViewFrame
             *      react to this message, depending on which
             *      backgrounds needs to be updated.
             *
             *      Parameters:
             *
             *      --  WPFolder *mp1: folder that the container
             *          represents. This must be the root folder
             *          if we're displaying a disk object.
             *
             *      No return value.
             *
             *@@added V1.0.0 (2002-09-24) [umoeller]
             */

            case FM_SETCNRLAYOUT:
                PMPF_POPULATESPLITVIEW(("FM_SETCNRLAYOUT"));
                if (    (hwndMainControl = WinQueryWindow(hwndFrame, QW_OWNER))
                     && (psv = WinQueryWindowPtr(hwndMainControl, QWL_USER))
                     && (mp1)
                     && (psv->flSplit & SPLIT_FDRSTYLES)
                   )
                {
                    CNRINFO CnrInfo;

                    PMPF_POPULATESPLITVIEW(("psv->pobjUseList [%s]", _wpQueryTitle(psv->pobjUseList)));

                    WinSendMsg(psv->hwndFilesCnr, CM_QUERYCNRINFO,
                               (MPARAM)&CnrInfo, (MPARAM)sizeof(CNRINFO));

                    fdrvSetCnrLayout(psv->hwndFilesCnr, (WPFolder*)mp1,
                                     (CnrInfo.flWindowAttr & CV_DETAIL)
                                      ? OPEN_DETAILS : OPEN_CONTENTS);

                    // and set sort func V1.0.0 (2002-09-13) [umoeller]
                    fdrSetFldrCnrSort((WPFolder*)mp1,
                                      psv->hwndFilesCnr,
                                      TRUE);        // force
                }
            break;

            /*
             * FM_INSERTOBJECT:
             *      notifies the files container of an object that
             *      may need to be inserted.
             *
             *      It is posted primarily by fdrSplitInsertObject
             *      when a non-folder or non-disk object is awakened.
             *      It may also be _sent_ by the tree frame after it
             *      processes an FM_INSERTFOLDER for a new folder or
             *      disk.
             *
             *      Parameters:
             *
             *      --  WPObject* mp1: an object to insert
             *
             *      --  WPFolder* mp2: the folder containing mp1
             *
             *      If there is no file mask, the object is inserted
             *      unconditionally;  if there is, it must match the
             *      specified criteria.
             *
             *      See fdrSplitInsertObject and fnwpTreeFrame for
             *      additional details.
             *
             * added V1.0.9 (2011-09-24) [rwalsh]: @@fixes 2
             */

            case FM_INSERTOBJECT:
            {
                WPObject        *pObject;

                if (    (hwndMainControl = WinQueryWindow(hwndFrame, QW_OWNER))
                     && (psv = WinQueryWindowPtr(hwndMainControl, QWL_USER))
                     && (OBJECT_FROM_PREC(psv->precFilesShowing) == (WPObject*)mp2)
                     && (pObject = (WPObject*)mp1)
                     && (   !(psv->pcszFileMask)
                         || !(*psv->pcszFileMask)
                         || (fdrvIsInsertable(pObject,
                                              INSERT_FILESYSTEMS,
                                              psv->pcszFileMask))
                        )
                   )
                {
                    POINTL ptl = {0, 0};  // required or call will fail
                    PMINIRECORDCORE prc;

                    // non-filesystem objects have to be marked "ignore"
                    // or else details view will crash when it tries to
                    // display filesystem-only details they lack
                    if (!_somIsA(pObject, _WPFileSystem) &&
                        (prc = _wpQueryCoreRecord(pObject)))
                        prc->flRecordAttr |= CRA_IGNORE;

                    _wpCnrInsertObject(pObject,
                                       psv->hwndFilesCnr,
                                       &ptl,
                                       NULL,
                                       NULL);
                }
            }
            break;

            case FM_INSERTFOLDER:
                PMPF_POPULATESPLITVIEW(("fnwpFilesFrame received FM_INSERTFOLDER in error"));
            break;

            /*
             * WM_DRAWITEM:
             *
             *  for details view, don't pass this msg to any of the
             *  subclass procs if it's for a filesystem-only column
             *  (CFA_IGNORE) for a non-filesystem object (CRA_IGNORE);
             *  the cnr will paint the appropriate emphasis but leave
             *  the contents blank (which is what we want)
             *
             * added V1.0.11 (2016-08-12) [rwalsh]
             */

            case WM_DRAWITEM:
            {            
                PCNRDRAWITEMINFO pcdi = (PCNRDRAWITEMINFO)((POWNERITEM)mp2)->hItem;

                if (!pcdi
                    || !pcdi->pFieldInfo
                    || !(pcdi->pFieldInfo->flData & CFA_IGNORE)
                    || !pcdi->pRecord
                    || !(pcdi->pRecord->flRecordAttr & CRA_IGNORE))
                        fCallDefault = TRUE;
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
    }
    CATCH(excpt1) {} END_CATCH();

    return mrc;
}

/*
 *@@ fdrSplitCreateFrame:
 *      creates a WC_FRAME with the split view controller
 *      (fnwpSplitController) as its FID_CLIENT.
 *
 *      The caller is responsible for allocating a
 *      FDRSPLITVIEW structure, zeroing it out, and
 *      passing it in - possibly with some members
 *      initialized.
 *
 *      The caller must also subclass the frame that is
 *      returned and free that structure on WM_DESTROY.
 *
 *      The frame is created invisible with an undefined
 *      position. The handle of the frame window is
 *      stored in psv->hwndMainFrame, if TRUE is returned.
 *
 *      With flSplit, pass in any combination of the
 *      following:
 *
 *      --  SPLIT_ANIMATE: give the frame the WS_ANIMATE
 *          style.
 *
 *      --  SPLIT_FDRSTYLES: make the two containers
 *          replicate the exact styles (backgrounds,
 *          fonts, colors) of the folders they are
 *          currently displaying. This is CPU-intensive,
 *          but pretty.
 *
 *      --  SPLIT_MULTIPLESEL: make the files cnr support
 *          multiple selections. Otherwise the files cnr
 *          will only allow one object at a time to be
 *          selected.
 *
 *      --  SPLIT_STATUSBAR: create a status bar below
 *          the split view. Note that is the responsibility
 *          of the caller then to position the status bar.
 *
 *      --  SPLIT_MENUBAR: create a menu bar for the frame.
 *
 *      --  SPLIT_NOAUTOPOSITION: force the split controller
 *          to not automatically reposition the subwindows
 *          in FDRSPLITVIEW. You must then explicitly
 *          position everything on WM_WINDOWPOSCHANGED.
 *          (File dlg uses this to control the extra buttons.)
 *
 *      --  SPLIT_NOAUTOPOPOPULATE: do not automatically
 *          populate the view with the root folder.
 *          (File dlg needs this to force populate to a
 *          subtree on startup.)
 *
 *@@added V1.0.0 (2002-08-28) [umoeller]
 *@@changed V1.0.11 (2016-08-12) [rwalsh]: don't zero-out *psv
 */

BOOL fdrSplitCreateFrame(WPObject *pRootObject,
                         WPFolder *pRootsFolder,
                         PFDRSPLITVIEW psv,
                         ULONG flFrame,
                         PCSZ pcszTitle,
                         ULONG flSplit,
                         PCSZ pcszFileMask,
                         LONG lSplitBarPos)
{
    static      s_fRegistered = FALSE;

    if (!s_fRegistered)
    {
        // first call: register the controller class
        s_fRegistered = TRUE;

        WinRegisterClass(winhMyAnchorBlock(),
                         (PSZ)WC_SPLITCONTROLLER,
                         fnwpSplitController,
                         CS_SIZEREDRAW,
                         sizeof(PVOID));
    }

    psv->cbStruct = sizeof(*psv);
    psv->lSplitBarPos = lSplitBarPos;
    psv->pRootObject = pRootObject;
    psv->pRootsFolder = pRootsFolder;
    psv->flSplit = flSplit;

    psv->pcszFileMask = pcszFileMask;

    if (psv->hwndMainFrame = winhCreateStdWindow(HWND_DESKTOP, // parent
                                                 NULL,         // pswp
                                                 flFrame,
                                                 (flSplit & SPLIT_ANIMATE)
                                                    ? WS_ANIMATE   // frame style, not yet visible
                                                    : 0,
                                                 pcszTitle,
                                                 0,            // resids
                                                 WC_SPLITCONTROLLER,
                                                 WS_VISIBLE,   // client style
                                                 0,            // frame ID
                                                 psv,
                                                 &psv->hwndMainControl))
    {
        PMINIRECORDCORE pRootRec;
        POINTL  ptlIcon = {0, 0};

        psv->habGUI = WinQueryAnchorBlock(psv->hwndMainFrame);

        if (flSplit & SPLIT_MENUBAR)
            WinLoadMenu(psv->hwndMainFrame,
                        cmnQueryNLSModuleHandle(FALSE),
                        ID_XFM_SPLITVIEWBAR);

        if (flSplit & SPLIT_STATUSBAR)
            psv->hwndStatusBar = stbCreateBar(pRootsFolder,
                                              pRootObject,
                                              psv->hwndMainFrame,
                                              psv->hwndTreeCnr);


        // insert somSelf as the root of the tree
        pRootRec = _wpCnrInsertObject(pRootObject,
                                      psv->hwndTreeCnr,
                                      &ptlIcon,
                                      NULL,       // parent record
                                      NULL);      // RECORDINSERT

        // _wpCnrInsertObject subclasses the container owner,
        // so subclass this with the XFolder subclass
        // proc again; otherwise the new menu items
        // won't work
        psv->psfvTree = fdrCreateSFV(psv->hwndTreeFrame,
                                     psv->hwndTreeCnr,
                                     QWL_USER,
                                     pRootsFolder,
                                     pRootObject);
        psv->psfvTree->pfnwpOriginal = WinSubclassWindow(psv->hwndTreeFrame,
                                                         fnwpTreeFrame);

        // same thing for files frame; however we need to
        // insert a temp object first to let the WPS subclass
        // the cnr owner first
        _wpCnrInsertObject(pRootsFolder,
                           psv->hwndFilesCnr,
                           &ptlIcon,
                           NULL,
                           NULL);
        psv->psfvFiles = fdrCreateSFV(psv->hwndFilesFrame,
                                      psv->hwndFilesCnr,
                                      QWL_USER,
                                      pRootsFolder,
                                      pRootObject);
        psv->psfvFiles->pfnwpOriginal = WinSubclassWindow(psv->hwndFilesFrame,
                                                          fnwpFilesFrame);

        // remove the temp object again
        _wpCnrRemoveObject(pRootsFolder,
                           psv->hwndFilesCnr);

        if (!(flSplit & SPLIT_NOAUTOPOPOPULATE))
            // and populate this once we're running
            fdrPostFillFolder(psv,
                              pRootRec,
                              // full populate, and expand tree on the left,
                              // and set background
                              FFL_SETBACKGROUND | FFL_EXPAND);

        return TRUE;
    }

    return FALSE;
}

/*
 *@@ fdrSplitDestroyFrame:
 *      being the reverse to fdrSetupSplitView, this
 *      cleans up all allocated resources and destroys
 *      the windows, including the main frame.
 *
 *      Does NOT free psv because we can't know how it was
 *      allocated. (As fdrSetupSplitView says, allocating
 *      and freeing the FDRSPLITVIEW is the job of the
 *      user code.)
 *
 *@@added V1.0.0 (2002-08-21) [umoeller]
 */

VOID fdrSplitDestroyFrame(PFDRSPLITVIEW psv)
{
    // hide the main window, the cnr cleanup can
    // cause a lot of repaint, and for the file
    // dlg, the user should get immediate feedback
    // when the buttons are pressed
    WinShowWindow(psv->hwndMainFrame, FALSE);

    // remove use item for right view
    if (psv->pobjUseList)
        _wpDeleteFromObjUseList(psv->pobjUseList,
                                &psv->uiDisplaying);

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
    psv->fSplitViewReady = FALSE;

    fdrvClearContainer(psv->hwndTreeCnr,
                       CLEARFL_TREEVIEW);
    fdrvClearContainer(psv->hwndFilesCnr,
                       // not tree view, but maybe unlock
                       (psv->fUnlockOnClear)
                            ? CLEARFL_UNLOCKOBJECTS
                            : 0);

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

