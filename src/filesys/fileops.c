
/*
 *@@sourcefile fileops.c:
 *      this has the XWorkplace file operations engine.
 *      This gets called from various method overrides to
 *      implement certain file-operations features.
 *
 *      Current features:
 *
 *      --  Replacement "File exists" dialog implementation
 *          (fopsConfirmObjectTitle).
 *
 *      --  Expanding objects to hold all sub-objects if the
 *          object is a folder. See fopsExpandObjectDeep
 *          and fopsExpandObjectFlat for details.
 *
 *      --  Generic file-operations framework to process
 *          files (actually, any objects) on the XWorkplace
 *          File thread. The common functionality of this code
 *          is to build a list of objects which need to be processed
 *          and pass that list to the XWorkplace File thread, which
 *          will then do the actual processing.
 *
 *          The framework is separated into two layers:
 *
 *          --  the bottom layer (see fops_bottom.c);
 *          --  the top layer (see fops_top.c).
 *
 *          The framework uses the special FOPSRET error return type,
 *          which is NO_ERROR (0) if no error occured.
 *
 *      This file is ALL new with V0.9.0.
 *
 *      Function prefix for this file:
 *      --  fops*
 *
 *@@added V0.9.0 [umoeller]
 *@@header "filesys\fileops.h"
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
#include "helpers\comctl.h"             // common controls (window procs)
#include "helpers\cnrh.h"               // container helper routines
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\stringh.h"            // string helper routines
#include "helpers\winh.h"               // PM helper routines
#include "helpers\xstring.h"            // extended string helpers

// SOM headers which don't crash with prec. header files
#include "xfobj.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\xsetup.h"              // XWPSetup implementation

#include "filesys\fileops.h"            // file operations implementation
#include "filesys\xthreads.h"           // extra XWorkplace threads

// other SOM headers
#pragma hdrstop                         // VAC++ keeps crashing otherwise
#include <wpfolder.h>
#include "xtrash.h"
#include "xtrashobj.h"
#include "filesys\trash.h"              // trash can implementation
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

#include "helpers\undoc.h"              // some undocumented stuff

/* ******************************************************************
 *                                                                  *
 *   Expanded object lists                                          *
 *                                                                  *
 ********************************************************************/

/*
 *@@ fopsLoopSneaky:
 *      goes thru the contents of pFolder and
 *      adds the size of all non-instantiated
 *      files using Dos* functions. This operates
 *      on pFolder only and does not recurse into
 *      subfolders.
 *
 *      This ignores all subfolders in *pFolder.
 *
 *      This _raises_ the size of the contents
 *      specified with pulSizeContents by the
 *      size of all files found.
 *
 *      Preconditions: The caller should have locked
 *      pFolder before calling this to make sure the
 *      contents are not modified while we are working
 *      here.
 *
 *@@added V0.9.6 (2000-10-25) [umoeller]
 */

FOPSRET fopsLoopSneaky(WPFolder *pFolder,       // in: folder
                       PULONG pulFilesCount,    // out: no. of dormant files found (raised!)
                       PULONG pulSizeContents)  // out: total size of dormant files found (raised!)
{
    FOPSRET       frc = NO_ERROR;

    CHAR    szFolderPath[CCHMAXPATH];

    // if we have a "folders only" populate, we
    // need to go thru the contents of the folder
    // and for all DOS files which have not been
    // added yet, we need to add the file size...
    // V0.9.6 (2000-10-25) [umoeller]

    // get folder name
    if (!_wpQueryFilename(pFolder, szFolderPath, TRUE))
        frc = FOPSERR_WPQUERYFILENAME_FAILED;
    else
    {
        M_WPFileSystem  *pWPFileSystem = _WPFileSystem;

        CHAR            szSearchMask[CCHMAXPATH];
        HDIR            hdirFindHandle = HDIR_CREATE;
        FILEFINDBUF3    ffb3 = {0};      // returned from FindFirst/Next
        ULONG           cbFFB3 = sizeof(FILEFINDBUF3);
        ULONG           ulFindCount = 1;  // look for 1 file at a time

        // _Pmpf((__FUNCTION__ ": doing DosFindFirst for %s", szFolderPath));

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

            // _Pmpf(("    got file %s", szFullPath));

            if (!_wpclsQueryAwakeObject(pWPFileSystem,
                                        szFullPath))
            {
                // object not awake yet: this means that
                // the object was not added to the list above...
                // add the file's size
                // _Pmpf(("        not already instantiated"));
                (*pulFilesCount)++;
                *pulSizeContents += ffb3.cbFile;
            }
            /* else
            {
                _Pmpf(("        --> already instantiated"));
            } */

            ulFindCount = 1;
            frc = DosFindNext(hdirFindHandle,
                              &ffb3,
                              cbFFB3,
                              &ulFindCount);
        } // while (arc == NO_ERROR)

        if (frc == ERROR_NO_MORE_FILES)
            frc = NO_ERROR;

        DosFindClose(hdirFindHandle);
    }

    return (frc);
}

/*
 *@@ fopsFolder2ExpandedList:
 *      creates a LINKLIST of EXPANDEDOBJECT items
 *      for the given folder's contents.
 *
 *      Gets called by fopsExpandObjectDeep if that
 *      function has been invoked on a folder.
 *      This calls fopsExpandObjectDeep in turn for
 *      each object found, so this may be called
 *      recursively.
 *
 *      See fopsExpandObjectDeep for an introduction
 *      what these things are good for.
 *
 *@@added V0.9.2 (2000-02-28) [umoeller]
 *@@changed V0.9.3 (2000-04-28) [umoeller]: now pre-resolving wpQueryContent for speed
 *@@changed V0.9.6 (2000-10-25) [umoeller]: added fFoldersOnly
 */

PLINKLIST fopsFolder2ExpandedList(WPFolder *pFolder,
                                  PULONG pulSizeContents, // out: size of all objects on list
                                  BOOL fFoldersOnly)
{
    PLINKLIST   pll = lstCreate(FALSE);       // do not free the items
    ULONG       ulSizeContents = 0;

    BOOL        fFolderLocked = FALSE;

    ULONG       ulNesting = 0;
    DosEnterMustComplete(&ulNesting);

    #ifdef DEBUG_TRASHCAN
        _Pmpf(("Object \"%s\" is a folder, creating SFL", _wpQueryTitle(pFolder) ));
    #endif

    // lock folder for querying content
    TRY_LOUD(excpt1)
    {
        // populate (either fully or with folders only)
        if (wpshCheckIfPopulated(pFolder,
                                 fFoldersOnly))
        {
            fFolderLocked = !wpshRequestFolderMutexSem(pFolder, 5000);
            if (fFolderLocked)
            {
                WPObject *pObject;

                // pre-resolve _wpQueryContent for speed V0.9.3 (2000-04-28) [umoeller]
                somTD_WPFolder_wpQueryContent rslv_wpQueryContent
                        = SOM_Resolve(pFolder, WPFolder, wpQueryContent);

                // now collect all objects in folder;
                // -- if we have a full populate, we add all objects
                //    to the list;
                // -- if we have a "folders only" populate, we add
                //    all objects we have (which is at least all the
                //    folders, but can contain additional objects)
                for (pObject = rslv_wpQueryContent(pFolder, NULL, QC_FIRST);
                     pObject;
                     pObject = rslv_wpQueryContent(pFolder, pObject, QC_Next))
                {
                    PEXPANDEDOBJECT fSOI = NULL;

                    #ifdef DEBUG_TRASHCAN
                        _Pmpf(("creating SOI for \"%s\" in folder \"%d\"",
                                _wpQueryTitle(pObject),
                                _wpQueryTitle(pFolder) ));
                    #endif

                    // create a list item for this object;
                    // if pObject is a folder, that function will
                    // call ourselves again...
                    fSOI = fopsExpandObjectDeep(pObject,
                                                fFoldersOnly);
                    ulSizeContents += fSOI->ulSizeThis;
                    lstAppendItem(pll, fSOI);
                }

                if (fFoldersOnly)
                {
                    ULONG ulFilesCount = 0;
                    fopsLoopSneaky(pFolder,
                                   &ulFilesCount,       // not needed
                                   &ulSizeContents);
                }
            }
        }
    }
    CATCH(excpt1) {} END_CATCH();

    if (fFolderLocked)
    {
        wpshReleaseFolderMutexSem(pFolder);
        fFolderLocked = FALSE;
    }

    DosExitMustComplete(&ulNesting);

    *pulSizeContents = ulSizeContents;

    return (pll);
}

/*
 *@@ fopsExpandObjectDeep:
 *      creates a EXPANDEDOBJECT for the given
 *      object. If the object is a folder, the
 *      member list is automatically filled with
 *      the folder's contents (recursively, if
 *      the folder contains subfolders), by calling
 *      fopsFolder2ExpandedList.
 *
 *      --  If (fFoldersOnly == FALSE), this does a full
 *          populate on folder which was encountered.
 *          This can take a long time and for many subfolders
 *          can even hang the entire WPS.
 *
 *      --  If (fFoldersOnly == TRUE), this populates
 *          folders with subfolders only. Still, we add
 *          all objects which have been instantiated
 *          on the folder contents list already, but
 *          calculate the folder size using Dos* functions
 *          only.
 *
 *      Call fopsFreeExpandedObject to free the item
 *      returned by this function.
 *
 *      This function is useful for having a uniform
 *      interface which represents an object and all
 *      subobjects even if that object represents a folder.
 *      After calling this function,
 *      EXPANDEDOBJECT.ulSizeThis has the total size
 *      of pObject and all objects which reside in
 *      subfolders (if applicable).
 *
 *@@added V0.9.2 (2000-02-28) [umoeller]
 *@@changed V0.9.6 (2000-10-25) [umoeller]: added fFoldersOnly
 */

PEXPANDEDOBJECT fopsExpandObjectDeep(WPObject *pObject,
                                     BOOL fFoldersOnly)
{
    PEXPANDEDOBJECT pSOI = NULL;
    if (wpshCheckObject(pObject))
    {
        // create object item
        pSOI = (PEXPANDEDOBJECT)malloc(sizeof(EXPANDEDOBJECT));
        if (pSOI)
        {
            #ifdef DEBUG_TRASHCAN
                _Pmpf(("SOI for object %s", _wpQueryTitle(pObject) ));
            #endif

            pSOI->pObject = pObject;
            if (_somIsA(pObject, _WPFolder))
            {
                // object is a folder:
                // fill list (this calls us again for every object found)
                pSOI->pllContentsSFL = fopsFolder2ExpandedList(pObject,
                                                               &pSOI->ulSizeThis,
                                                                // out: size of files on list
                                                               fFoldersOnly);
            }
            else
            {
                // non-folder:
                pSOI->pllContentsSFL = NULL;
                if (_somIsA(pObject, _WPFileSystem))
                    // is a file system object:
                    pSOI->ulSizeThis = _wpQueryFileSize(pObject);
                else
                    // abstract object:
                    pSOI->ulSizeThis = 0;
            }

            #ifdef DEBUG_TRASHCAN
                _Pmpf(("End of SOI for object %s", _wpQueryTitle(pObject) ));
            #endif
        }
    }

    return (pSOI);
}

/*
 *@@ fopsFreeExpandedList:
 *      frees a LINKLIST of EXPANDEDOBJECT items
 *      created by fopsFolder2ExpandedList.
 *      This recurses.
 *
 *@@added V0.9.2 (2000-02-28) [umoeller]
 */

VOID fopsFreeExpandedList(PLINKLIST pllSFL)
{
    if (pllSFL)
    {
        PLISTNODE pNode = lstQueryFirstNode(pllSFL);
        while (pNode)
        {
            PEXPANDEDOBJECT pSOI = (PEXPANDEDOBJECT)pNode->pItemData;
            fopsFreeExpandedObject(pSOI);
                // after this, pSOI->pllContentsSFL is NULL

            pNode = pNode->pNext;
        }
        lstFree(pllSFL);
    }
}

/*
 *@@ fopsFreeExpandedObject:
 *      this frees a EXPANDEDOBJECT previously
 *      created by fopsExpandObjectDeep. This calls
 *      fopsFreeExpandedList, possibly recursively,
 *      if the object contains a list (ie. is a
 *      folder).
 *
 *@@added V0.9.2 (2000-02-28) [umoeller]
 */

VOID fopsFreeExpandedObject(PEXPANDEDOBJECT pSOI)
{
    if (pSOI)
    {
        if (pSOI->pllContentsSFL)
            // object has a list:
            fopsFreeExpandedList(pSOI->pllContentsSFL);
                // this sets pllContentsSFL to NULL

        free(pSOI);
    }
}

/*
 *@@ fopsExpandObjectFlat:
 *      similar to fopsExpandObjectDeep, this builds
 *      a list of pObject and, if pObject is a folder,
 *      of all contained folders and objects. However,
 *      this creates a simple linked list containing
 *      the objects, so the hierarchy is lost.
 *
 *      This operates in two modes:
 *
 *      --  If (fFoldersOnly == FALSE), this does a
 *          full populate.
 *
 *          After the call, pllObjects contains all the
 *          plain WPObject* pointers, so the list should
 *          have been created with lstCreate(FALSE) in
 *          order not to invoke free() on it.
 *
 *          If folders are found, contained objects are
 *          appended to the list before the folder itself.
 *          Otherwise, objects are returned in the order
 *          in which wpQueryContent returns them, which
 *          is dependent on the underlying file system.
 *          and should not be relied upon.
 *
 *          For example, see the following folder/file hierarchy:
 +
 +              folder1
 +                 +--- folder2
 +                 |      +---- text1.txt
 +                 |      +---- text2.txt
 +                 +--- text3.txt
 *
 *          If fopsExpandObjectFlat(folder1) is invoked,
 *          you get the following order in the list:
 *
 +          text1.txt, text2.txt, folder2, text3.txt, folder1.
 *
 *          In this mode, pulDormantFilesCount is ignored because
 *          all files are awakened by this function.
 *
 *      --  If (fFoldersOnly == TRUE), this populates
 *          subfolders with folders only. As a result, on
 *          the list, you will receive all folders. Before
 *          the subfolder, you _may_ get regular objects
 *          with have already been awakened, but this might
 *          not be the case.
 *
 *          In this mode, pulObjectCount contains only the
 *          object count of awake objects (folders which
 *          have been awakened plus other objects which
 *          were already awake). In addition, pulDormantFilesCount
 *          receives the no. of dormant files encountered.
 *
 *          Again, contained (and awake) objects are
 *          appended to the list before the folder itself.
 *
 *      pllObjects is never cleared by this function,
 *      so you can call this several times with the
 *      same list. If you call this only once, make
 *      sure pllObjects is empty before calling.
 *
 *      This is most useful for (and used with) deleting
 *      a folder and all its contents.
 *
 *@@added V0.9.3 (2000-04-27) [umoeller]
 *@@changed V0.9.3 (2000-04-28) [umoeller]: now pre-resolving wpQueryContent for speed
 *@@changed V0.9.6 (2000-10-25) [umoeller]: added fFoldersOnly
 */

FOPSRET fopsExpandObjectFlat(PLINKLIST pllObjects,  // in: list to append to (plain WPObject* pointers)
                             WPObject *pObject,     // in: object to start with
                             BOOL fFoldersOnly,
                             PULONG pulObjectCount, // out: objects count on list;
                                                    // the ULONG must be set to 0 before calling this!
                             PULONG pulDormantFilesCount) // out: count of dormant files;
                                                    // the ULONG must be set to 0 before calling this!
{
    FOPSRET frc = NO_ERROR;

    if (_somIsA(pObject, _WPFolder))
    {
        // it's a folder:
        // lock it and build a list from its contents
        BOOL    fFolderLocked = FALSE;
        ULONG   ulNesting = 0;
        DosEnterMustComplete(&ulNesting);

        TRY_LOUD(excpt1)
        {
            if (!wpshCheckIfPopulated(pObject,
                                     fFoldersOnly))
                frc = FOPSERR_POPULATE_FAILED;
            else
            {
                fFolderLocked = !wpshRequestFolderMutexSem(pObject, 5000);
                if (fFolderLocked)
                {
                    WPObject *pSubObject;

                    // pre-resolve _wpQueryContent for speed V0.9.3 (2000-04-28) [umoeller]
                    somTD_WPFolder_wpQueryContent rslv_wpQueryContent
                            = SOM_Resolve(pObject, WPFolder, wpQueryContent);

                    // now collect all objects in folder;
                    // -- if we have a full populate, we add all objects
                    //    to the list;
                    // -- if we have a "folders only" populate, we add
                    //    all objects we have (which is at least all the
                    //    folders, but can contain additional objects)
                    for (pSubObject = rslv_wpQueryContent(pObject, NULL, QC_FIRST);
                         pSubObject;
                         pSubObject = rslv_wpQueryContent(pObject, pSubObject, QC_Next))
                    {
                        // recurse!
                        // this will add pSubObject to pllObjects
                        frc = fopsExpandObjectFlat(pllObjects,
                                                   pSubObject,
                                                   fFoldersOnly,
                                                   pulObjectCount,
                                                   pulDormantFilesCount);
                        if (frc != NO_ERROR)
                            // error:
                            break;
                    } // end for

                    if (frc == NO_ERROR)
                        if (fFoldersOnly)
                        {
                            ULONG ulSizeContents = 0;       // not used
                            // _Pmpf((__FUNCTION__ ": calling fopsLoopSneaky; count pre: %d",
                               //                   *pulDormantFilesCount));
                            frc = fopsLoopSneaky(pObject,
                                                 pulDormantFilesCount,
                                                 &ulSizeContents);
                            // _Pmpf(("    count post: %d", *pulDormantFilesCount));
                        }
                }
                else
                    frc = FOPSERR_LOCK_FAILED;
            }
        }
        CATCH(excpt1) {} END_CATCH();

        if (fFolderLocked)
        {
            wpshReleaseFolderMutexSem(pObject);
            fFolderLocked = FALSE;
        }

        DosExitMustComplete(&ulNesting);

    } // end if (_somIsA(pObject, _WPFolder))

    // append this object to the list;
    // since we have recursed first, the folder
    // contents come before the folder in the list
    if (frc == NO_ERROR)
    {
        // _Pmpf((__FUNCTION__ ": appending %s", _wpQueryTitle(pObject) ));
        lstAppendItem(pllObjects, pObject);
        if (pulObjectCount)
            (*pulObjectCount)++;
    }
        // _Pmpf((__FUNCTION__ ": error %d for %s", _wpQueryTitle(pObject) ));

    return (frc);
}

/********************************************************************
 *                                                                  *
 *   "File exists" (title clash) dialog                             *
 *                                                                  *
 ********************************************************************/

/*
 *@@ fnwpTitleClashDlg:
 *      dlg proc for the "File exists" dialog.
 *      Most of the logic for that dialog is in the
 *      fopsConfirmObjectTitle function actually; this
 *      function is only responsible for adjusting the
 *      dialog items' keyboard focus and such.
 */

MRESULT EXPENTRY fnwpTitleClashDlg(HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    #define WM_DELAYEDFOCUS         (WM_USER+1)

    MRESULT mrc = (MRESULT)0;

    switch (msg)
    {
        /*
         * WM_CONTROL:
         *      intercept focus messages and
         *      set focus to what we really want
         */

        case WM_CONTROL:
        {
            switch (SHORT2FROMMP(mp1)) // usNotifyCode
            {
                case EN_SETFOCUS: // == BN_CLICKED
                {

                    if (SHORT1FROMMP(mp1) == ID_XFDI_CLASH_RENAMENEWTXT)
                        winhSetDlgItemChecked(hwndDlg, ID_XFDI_CLASH_RENAMENEW, TRUE);
                    else if (SHORT1FROMMP(mp1) == ID_XFDI_CLASH_RENAMEOLDTXT)
                        winhSetDlgItemChecked(hwndDlg, ID_XFDI_CLASH_RENAMEOLD, TRUE);
                    else if (SHORT1FROMMP(mp1) == ID_XFDI_CLASH_RENAMENEW)
                        WinPostMsg(hwndDlg, WM_DELAYEDFOCUS,
                                   (MPARAM)ID_XFDI_CLASH_RENAMENEWTXT, MPNULL);
                    else if (SHORT1FROMMP(mp1) == ID_XFDI_CLASH_RENAMEOLD)
                        WinPostMsg(hwndDlg, WM_DELAYEDFOCUS,
                                   (MPARAM)ID_XFDI_CLASH_RENAMEOLDTXT, MPNULL);
                }
            }
        break; }

        case WM_DELAYEDFOCUS:
            winhSetDlgItemFocus(hwndDlg, (HWND)mp1);
        break;

        /*
         * WM_COMMAND:
         *      intercept "OK"/"Cancel" buttons
         */

        case WM_COMMAND:
        {
            mrc = (MRESULT)0;
            switch ((ULONG)mp1)
            {
                case DID_OK:
                {
                    ULONG ulSelection = DID_CANCEL,
                          ulLastFocusID = 0;
                    CHAR szLastFocusID[20] = "";
                    if (winhIsDlgItemChecked(hwndDlg, ID_XFDI_CLASH_RENAMENEW))
                    {
                        ulSelection = ID_XFDI_CLASH_RENAMENEW;
                        ulLastFocusID = ID_XFDI_CLASH_RENAMENEWTXT;
                    }
                    else if (winhIsDlgItemChecked(hwndDlg, ID_XFDI_CLASH_RENAMEOLD))
                    {
                        ulSelection = ID_XFDI_CLASH_RENAMEOLD;
                        ulLastFocusID = ID_XFDI_CLASH_RENAMEOLDTXT;
                    }
                    else
                    {
                        ulSelection = ID_XFDI_CLASH_REPLACE;
                        ulLastFocusID = ID_XFDI_CLASH_REPLACE;
                    }

                    // store focus
                    sprintf(szLastFocusID, "%d",
                            ulLastFocusID);
                    PrfWriteProfileString(HINI_USER,
                                          (PSZ)INIAPP_XWORKPLACE,
                                          (PSZ)INIKEY_NAMECLASHFOCUS,
                                          szLastFocusID);
                    // store window pos
                    winhSaveWindowPos(hwndDlg,
                                HINI_USER,
                                INIAPP_XWORKPLACE, INIKEY_WNDPOSNAMECLASH);
                    WinDismissDlg(hwndDlg, ulSelection);
                break; }

                case DID_CANCEL:
                    WinDismissDlg(hwndDlg, DID_CANCEL);
                break;
            }
        break; }

        case WM_HELP:
            cmnDisplayHelp(NULL,    // active Desktop
                           ID_XFH_TITLECLASH);
        break;

        default:
            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
    }

    return (mrc);
}

/*
 *@@ fopsFindFSWithSameName:
 *      checks pFolder's contents for a
 *      WPFileSystem object with the same
 *      real name (or, if fCheckTitle == TRUE,
 *      the same title) as somSelf.
 *
 *      If such a thing is found, it is
 *      returned and the real name is written
 *      into pszName2Check.
 *
 *      Used by fopsConfirmObjectTitle.
 *
 *@@added V0.9.1 (2000-01-30) [umoeller]
 *@@changed V0.9.9 (2001-03-27) [umoeller]: rewritten to no longer use wpshContainsFile, which caused find mutex deadlocks
 *@@changed V0.9.12 (2001-04-29) [umoeller]: forgot to populate folder first, fixed
 */

WPFileSystem* fopsFindFSWithSameName(WPFileSystem *somSelf,   // in: FS object to check for
                                     WPFolder *pFolder,       // in: folder to check
                                     const char *pcszFullFolder,  // in: full path spec of pFolder
                                     BOOL fCheckTitle,
                                     PSZ pszRealNameFound)    // out: new real name
{
    WPFileSystem *pFSReturn = NULL;
    BOOL    fFolderLocked = FALSE;

    CHAR    szTitleOrName[CCHMAXPATH],
            szCompare[CCHMAXPATH];
    if (fCheckTitle)
    {
        strcpy(szTitleOrName, _wpQueryTitle(somSelf));
        // (V0.84) this now does work for FAT to FAT;
        // HPFS to FAT needs to be checked
        // this should mimic the default WPS behavior close enough
    }
    else
        // check real name:
        _wpQueryFilename(somSelf, szTitleOrName, FALSE);

    // 2)   if the file is on FAT, make this 8+3
    doshMakeRealName(szCompare,         // target buffer
                     szTitleOrName,     // source
                     '!',               // replace-with char
                     doshIsFileOnFAT(pcszFullFolder));

    TRY_LOUD(excpt1)
    {
        WPObject    *pobj = 0;

        if (wpshCheckIfPopulated(pFolder, FALSE)) // V0.9.12 (2001-04-29) [umoeller]
        {
            fFolderLocked = !wpshRequestFolderMutexSem(pFolder, 5000);
            if (fFolderLocked)
            {
                // pre-resolve _wpQueryContent for speed V0.9.3 (2000-04-28) [umoeller]
                somTD_WPFolder_wpQueryContent rslv_wpQueryContent
                        = SOM_Resolve(pFolder, WPFolder, wpQueryContent);

                for (   pobj = rslv_wpQueryContent(pFolder, NULL, (ULONG)QC_FIRST);
                        (pobj);
                        pobj = rslv_wpQueryContent(pFolder, pobj, (ULONG)QC_NEXT)
                    )
                {
                    if (_somIsA(pobj, _WPFileSystem))
                    {
                        CHAR    szThisName[CCHMAXPATH];
                        if (_wpQueryFilename(pobj, szThisName, FALSE))
                            if (!stricmp(szThisName, szCompare))
                            {
                                pFSReturn = pobj;
                                strcpy(pszRealNameFound, szThisName);
                                break;
                            }
                    }
                }
            }
        }
    }
    CATCH(excpt1) { } END_CATCH();

    if (fFolderLocked)
        wpshReleaseFolderMutexSem(pFolder);

    return (pFSReturn);
}

/*
 *@@ fopsProposeNewTitle:
 *      proposes a new title for the object named
 *      pszTitle in the given folder. We attempt
 *      to find a title with a decimal number
 *      between the filestem and the extension which
 *      doesn't exist yet.
 *
 *      The proposed title is written into pszProposeTitle,
 *      which should be CCHMAXPATH in size.
 *
 *      For example, if you pass "demo.txt" and
 *      "demo.txt" and "demo:1.txt" already exist
 *      in pFolder, this would return "demo:2.txt".
 *
 *      Returns FALSE upon errors, e.g. if the internal
 *      exception handler crashed or a folder semaphore
 *      could not be requested within five seconds.
 *
 *      Used by fopsConfirmObjectTitle.
 *
 *@@added V0.9.1 (2000-01-30) [umoeller]
 *@@changed V0.9.1 (2000-01-08) [umoeller]: added mutex protection for folder queries
 *@@changed V0.9.3 (2000-04-28) [umoeller]: now pre-resolving wpQueryContent for speed
 *@@changed V0.9.9 (2001-03-27) [umoeller]: fixed hangs with .hidden filenames
 */

BOOL fopsProposeNewTitle(const char *pcszTitle,          // in: title to modify
                         WPFolder *pFolder,     // in: folder to check for existing titles
                         PSZ pszProposeTitle)   // out: buffer for new title
{
    CHAR    szFilenameWithoutCount[500];
    PSZ     p = 0,
            p2 = 0;
    BOOL    fFileExists = FALSE,
            fExit = FALSE;
    LONG    lFileCount = -1;

    BOOL    brc = TRUE;

    // propose new name; we will use the title and
    // add a ":num" before the extension and then
    // transform that one into a real name and keep
    // increasing "num" until no file with that
    // real name exists (as the WPS does it too)
    strcpy(szFilenameWithoutCount, pcszTitle);
    // check if title contains a ':num' already
    p2 = strchr(szFilenameWithoutCount, ':');
    if (p2)
    {
        lFileCount = atoi(p2+1);
        if (lFileCount)
        {
            // if we have a valid number following ":", remove it
            p = strchr(p2, '.');
            if (p)
                strcpy(p2, p);
            else
                *p2 = '\0';
        }
        else
            *p2 = '\0';     // V0.9.9 (2001-03-27) [umoeller]
    }

    // insert new number
    p = strrchr(szFilenameWithoutCount + 1, '.');       // last dot == extension
                // fixed V0.9.9 (2001-03-27) [umoeller]: start with
                // second char to handle ".unixhidden" files
    lFileCount = 1;
    do
    {
        BOOL            fFolderLocked = FALSE;
        CHAR            szFileCount[20];

        fExit = FALSE;

        sprintf(szFileCount, ":%d", lFileCount);

        if (p)
        {
            // has extension: insert
            ULONG ulBeforeDot = (p - szFilenameWithoutCount);
            // we don't care about FAT, because
            // we propose titles, not real names
            // (V0.84)
            strncpy(pszProposeTitle, szFilenameWithoutCount, ulBeforeDot);
            pszProposeTitle[ulBeforeDot] = '\0';
            strcat(pszProposeTitle, szFileCount);
            strcat(pszProposeTitle, p);
        }
        else
        {
            // has no extension: append
            strncpy(pszProposeTitle, szFilenameWithoutCount, 200);
            // we don't care about FAT, because
            // we propose titles, not real names
            // (V0.84)
            strcat(pszProposeTitle, szFileCount);
        }

        // now go through the folder content and
        // check whether we already have a file
        // with the proposed title (not real name!)
        // (V0.84)
        TRY_LOUD(excpt1)
        {
            WPFileSystem    *pExistingFile2 = 0;

            fFolderLocked = !wpshRequestFolderMutexSem(pFolder, 5000);
            if (fFolderLocked)
            {
                // pre-resolve _wpQueryContent for speed V0.9.3 (2000-04-28) [umoeller]
                somTD_WPFolder_wpQueryContent rslv_wpQueryContent
                        = SOM_Resolve(pFolder, WPFolder, wpQueryContent);

                fFileExists = FALSE;
                for (   pExistingFile2 = rslv_wpQueryContent(pFolder, NULL, (ULONG)QC_FIRST);
                        (pExistingFile2);
                        pExistingFile2 = rslv_wpQueryContent(pFolder, pExistingFile2, (ULONG)QC_NEXT)
                    )
                {
                    fFileExists = (stricmp(_wpQueryTitle(pExistingFile2),
                                           pszProposeTitle) == 0);
                    if (fFileExists)
                        break; // for wpQueryContent
                }
            }
            else
            {
                // error requesting mutex:
                fExit = TRUE;
                brc = FALSE;
            }
        }
        CATCH(excpt1)
        {
            fExit = TRUE;
            brc = FALSE;
        } END_CATCH();

        if (fFolderLocked)
            wpshReleaseFolderMutexSem(pFolder);

        lFileCount++;

        if (lFileCount > 99)
            // avoid endless loops
            break;

    } while (   (fFileExists)
              && (!fExit)
            );

    return (brc);
}

/*
 *@@ PrepareFileExistsDlg:
 *      prepares the "file exists" dialog by setting
 *      up all the controls.
 *
 *@@added V0.9.3 (2000-05-03) [umoeller]
 */

HWND PrepareFileExistsDlg(WPObject *somSelf,
                          ULONG menuID,
                          WPFolder *Folder,    // in: from fopsConfirmObjectTitle
                          PSZ pszFolder,
                          WPFileSystem *pFSExisting,
                          PSZ pszRealNameFound,
                          PSZ pszTitle)    // in/out: from fopsConfirmObjectTitle
{
    CHAR    szProposeTitle[CCHMAXPATH] = "????",
            szExistingFilename[CCHMAXPATH],
            szTemp[500];
    ULONG   ulDlgReturn = 0,
            ulLastFocusID = 0,
            ulLastDot = 0;
    HWND    hwndConfirm;
    PSZ     pszTemp = 0;

    // prepare file date/time etc. for
    // display in window
    FILESTATUS3         fs3;
    PCOUNTRYSETTINGS    pcs = cmnQueryCountrySettings(FALSE);
    ULONG               ulOfs = 0;
    // *** load confirmation dialog
    hwndConfirm = WinLoadDlg(HWND_DESKTOP, HWND_DESKTOP,
                             fnwpTitleClashDlg,
                             cmnQueryNLSModuleHandle(FALSE),
                             ID_XFD_TITLECLASH,
                             NULL);

    // disable window updates for the following changes
    WinEnableWindowUpdate(hwndConfirm, FALSE);

    // replace placeholders in introductory strings
    pszTemp = winhQueryWindowText(WinWindowFromID(hwndConfirm,
                                                  ID_XFDI_CLASH_TXT1));
    strhFindReplace(&pszTemp, &ulOfs, "%1", pszRealNameFound);
    ulOfs = 0;
    strhFindReplace(&pszTemp, &ulOfs, "%2", pszFolder);
    WinSetDlgItemText(hwndConfirm, ID_XFDI_CLASH_TXT1,
                      pszTemp);
    free(pszTemp);

    // set object information fields
    _wpQueryFilename(pFSExisting, szExistingFilename, TRUE);
    DosQueryPathInfo(szExistingFilename,
                     FIL_STANDARD,
                     &fs3, sizeof(fs3));
    strhFileDate(szTemp, &(fs3.fdateLastWrite),
                 pcs->ulDateFormat, pcs->cDateSep);
    WinSetDlgItemText(hwndConfirm, ID_XFDI_CLASH_DATEOLD, szTemp);
    strhFileTime(szTemp, &(fs3.ftimeLastWrite),
                 pcs->ulTimeFormat, pcs->cTimeSep);
    WinSetDlgItemText(hwndConfirm, ID_XFDI_CLASH_TIMEOLD, szTemp);

    strhThousandsULong(szTemp, fs3.cbFile, // )+512) / 1024 ,
                       pcs->cThousands);
    WinSetDlgItemText(hwndConfirm, ID_XFDI_CLASH_SIZEOLD, szTemp);

    if (pFSExisting != somSelf)
    {
        // if we're not copying within the same folder,
        // i.e. if the two objects are different,
        // give info on ourselves too
        CHAR szSelfFilename[CCHMAXPATH];
        _wpQueryFilename(somSelf, szSelfFilename, TRUE);
        DosQueryPathInfo(szSelfFilename,
                         FIL_STANDARD,
                         &fs3, sizeof(fs3));
        strhFileDate(szTemp, &(fs3.fdateLastWrite),
                     pcs->ulDateFormat, pcs->cDateSep);
        WinSetDlgItemText(hwndConfirm, ID_XFDI_CLASH_DATENEW, szTemp);
        strhFileTime(szTemp, &(fs3.ftimeLastWrite),
                     pcs->ulTimeFormat, pcs->cTimeSep);
        WinSetDlgItemText(hwndConfirm, ID_XFDI_CLASH_TIMENEW, szTemp);
        strhThousandsULong(szTemp, fs3.cbFile, // )+512) / 1024,
                           pcs->cThousands);
        WinSetDlgItemText(hwndConfirm, ID_XFDI_CLASH_SIZENEW, szTemp);
    }
    else
    {
        // if we're copying within the same folder,
        // set the "new object" fields empty
        WinSetDlgItemText(hwndConfirm, ID_XFDI_CLASH_DATENEW, "");
        WinSetDlgItemText(hwndConfirm, ID_XFDI_CLASH_TIMENEW, "");
        WinSetDlgItemText(hwndConfirm, ID_XFDI_CLASH_SIZENEW, "");
    }

    // get new title which doesn't exist in the folder yet
    fopsProposeNewTitle(pszTitle,
                        Folder,
                        szProposeTitle);

    // OK, we've found a new filename: set dlg items
    WinSendDlgItemMsg(hwndConfirm, ID_XFDI_CLASH_RENAMENEWTXT,
                      EM_SETTEXTLIMIT,
                      (MPARAM)(250), MPNULL);
    WinSetDlgItemText(hwndConfirm, ID_XFDI_CLASH_RENAMENEWTXT,
                      szProposeTitle);
    WinSendDlgItemMsg(hwndConfirm, ID_XFDI_CLASH_RENAMEOLDTXT,
                      EM_SETTEXTLIMIT,
                      (MPARAM)(250), MPNULL);
    WinSetDlgItemText(hwndConfirm, ID_XFDI_CLASH_RENAMEOLDTXT,
                      szProposeTitle);

    // select the first characters up to the extension
    // in the edit field
    pszTemp = strrchr(szProposeTitle, '.');       // last dot == extension
    if (pszTemp)
        ulLastDot = (pszTemp - szProposeTitle);
    else
        ulLastDot = 300;                    // too large == select all

    WinSendDlgItemMsg(hwndConfirm, ID_XFDI_CLASH_RENAMENEWTXT,
                      EM_SETSEL,
                      MPFROM2SHORT(0, ulLastDot),
                      MPNULL);
    WinSendDlgItemMsg(hwndConfirm, ID_XFDI_CLASH_RENAMEOLDTXT,
                      EM_SETSEL,
                      MPFROM2SHORT(0, ulLastDot),
                      MPNULL);

    // find the selection the user has made last time;
    // this INI key item is maintained by fnwpTitleClashDlg above
    ulLastFocusID = PrfQueryProfileInt(HINI_USER,
                                       (PSZ)INIAPP_XWORKPLACE,
                                       (PSZ)INIKEY_NAMECLASHFOCUS,
                                       ID_XFDI_CLASH_RENAMENEWTXT);
                                            // default value if not set

    // disable "Replace" and "Rename old"
    // if we're copying within the same folder
    // or if we're copying a folder to its parent
    if (    (pFSExisting == somSelf)
         || (pFSExisting == _wpQueryFolder(somSelf))
       )
    {
        WinEnableControl(hwndConfirm, ID_XFDI_CLASH_REPLACE, FALSE);
        WinEnableControl(hwndConfirm, ID_XFDI_CLASH_RENAMEOLD, FALSE);
        WinEnableControl(hwndConfirm, ID_XFDI_CLASH_RENAMEOLDTXT, FALSE);
        // if the last focus is one of the disabled items,
        // change it
        if (   (ulLastFocusID == ID_XFDI_CLASH_REPLACE)
            || (ulLastFocusID == ID_XFDI_CLASH_RENAMEOLDTXT)
           )
            ulLastFocusID = ID_XFDI_CLASH_RENAMENEWTXT;
    }
    else if (menuID == 0x006E)
    {
        // disable "Replace" for "Rename" mode; this is
        // not allowed
        WinEnableControl(hwndConfirm, ID_XFDI_CLASH_REPLACE, FALSE);
        // if the last focus is one of the disabled items,
        // change it
        if (ulLastFocusID == ID_XFDI_CLASH_REPLACE)
            ulLastFocusID = ID_XFDI_CLASH_RENAMENEWTXT;
    }

    // set focus to that item
    winhSetDlgItemFocus(hwndConfirm, ulLastFocusID);
        // this will automatically select the corresponding
        // radio button, see fnwpTitleClashDlg above

    // *** go!
    winhRestoreWindowPos(hwndConfirm,
                         HINI_USER,
                         INIAPP_XWORKPLACE, INIKEY_WNDPOSNAMECLASH,
                         SWP_MOVE | SWP_SHOW | SWP_ACTIVATE);
                                // move only, no resize
    return (hwndConfirm);
}

/*
 *@@ fopsConfirmObjectTitle:
 *      this gets called from XFldObject::wpConfirmObjectTitle
 *      to actually implement the "File exists" dialog. This
 *      performs a number of checks and loads a dialog from
 *      the NLS resources, if necessary, using fnwpTitleClashDlg.
 *
 *      See XFldObject::wpConfirmObjectTitle for a detailed
 *      description. The parameters are exactly the same.
 *
 *@@changed V0.9.1 (2000-01-30) [umoeller]: extracted some functions for clarity; this was a total mess
 *@@changed V0.9.3 (2000-04-08) [umoeller]: fixed "Create another" problem
 *@@changed V0.9.3 (2000-05-03) [umoeller]: extracted ProcessFileExistsDlg
 *@@changed V0.9.4 (2000-07-15) [umoeller]: fixed "Create another" problem for good
 */

ULONG fopsConfirmObjectTitle(WPObject *somSelf,
                             WPFolder *Folder,
                             WPObject **ppDuplicate,
                             PSZ pszTitle,
                             ULONG cbTitle,
                             ULONG menuID)
{
    ULONG ulrc = NAMECLASH_NONE;

    _Pmpf(("fopsConfirmObjectTitle: menuID is 0x%lX", menuID));

    #ifndef NO_NAMECLASH_DIALOG
        #define NO_NAMECLASH_DIALOG           0x80
    #endif

    // nameclashs are only a problem for file-system objects,
    // and only if we're not creating shadows
    if (    (_somIsA(somSelf, _WPFileSystem))
         && (menuID != 0x13C) // "create shadow" code
         && (menuID != 0x065) // various "create another" codes; V0.9.4 (2000-07-15) [umoeller]
         && (menuID != 0x066)
         && (menuID < 0x7D0)
         // && (0 == (_wpQueryNameClashOptions(somSelf, menuID) & NO_NAMECLASH_DIALOG))
                    // this doesn't work V0.9.9 (2001-04-07) [umoeller]
       )
    {
        CHAR            szFolder[CCHMAXPATH],
                        szRealNameFound[CCHMAXPATH];
        WPFileSystem    *pFSExisting = NULL;

        // check whether the target folder is valid
        if (    (!Folder)
             || (!_somIsA(Folder, _WPFolder))
             || (!_wpQueryFilename(Folder, szFolder, TRUE))
           )
            return (NAMECLASH_CANCEL);

        pFSExisting = fopsFindFSWithSameName(somSelf,
                                             Folder,
                                             szFolder,
                                             // fCheckTitle:
                                             (    (menuID == 0x6E)    // "rename" code
                                               || (menuID == 0x65)    // "create another" code (V0.9.0)
                                             ),
                                             szRealNameFound);
        if (pFSExisting)
        {
            #ifdef DEBUG_TITLECLASH
                CHAR szFolder2[CCHMAXPATH];
                _Pmpf(("  pObjExisting == 0x%lX", pFSExisting));
                _Pmpf(("    Title: %s", _wpQueryTitle(pFSExisting) ));
                _wpQueryFilename(_wpQueryFolder(pFSExisting), szFolder2, TRUE);
                _Pmpf(("    in folder: %s", szFolder2));
            #endif

            // file exists: now we need to differentiate.
            // 1)   If we're copying, we need to always
            //      display the confirmation.
            // 2)   If we're renaming somSelf, we only
            //      display the confirmation if the
            //      existing object is != somSelf, because
            //      otherwise we have no problem.
            if (    (somSelf != pFSExisting)
                 || (menuID != 0x6E)    // not "rename"
               )
            {
                HWND hwndConfirm = PrepareFileExistsDlg(somSelf,
                                                        menuID,
                                                        Folder,
                                                        szFolder,
                                                        pFSExisting,
                                                        szRealNameFound,
                                                        pszTitle);

                ULONG ulDlgReturn = WinProcessDlg(hwndConfirm);

                // check return value
                switch (ulDlgReturn)
                {
                    case ID_XFDI_CLASH_RENAMENEW:
                    {
                        // rename new: copy user's entry to buffer
                        // provided by the method
                        WinQueryDlgItemText(hwndConfirm, ID_XFDI_CLASH_RENAMENEWTXT,
                                            cbTitle-1, pszTitle);
                        ulrc = NAMECLASH_RENAME;
                    break; }

                    case ID_XFDI_CLASH_RENAMEOLD:
                    {
                        CHAR szTemp[1000];
                        // rename old: use wpSetTitle on existing object
                        WinQueryDlgItemText(hwndConfirm, ID_XFDI_CLASH_RENAMEOLDTXT,
                                            sizeof(szTemp)-1, szTemp);
                        _wpSetTitleAndRenameFile(pFSExisting, szTemp, 0);

                        if (menuID == 0x6E)     // "rename" code
                        {
                            CHAR szNewRealName2[CCHMAXPATH];
                            doshMakeRealName(szNewRealName2, pszTitle, '!', TRUE);
                            _wpSetTitleAndRenameFile(somSelf, pszTitle, 0);
                        }
                        ulrc = NAMECLASH_NONE;
                    break; }

                    case ID_XFDI_CLASH_REPLACE:
                        *ppDuplicate = pFSExisting;
                        ulrc = NAMECLASH_REPLACE;
                    break;

                    case DID_CANCEL:
                        ulrc = NAMECLASH_CANCEL;
                    break;
                }

                WinDestroyWindow(hwndConfirm);
            } // end if  (   (menuID != 0x6E) // "rename" code
              // || (somSelf != pFSExisting)
              // )
        }
        else if (menuID == 0x6E)     // "rename" code
        {
            // if (!pobjExisting) and we're renaming
            CHAR szNewRealName2[CCHMAXPATH];
            doshMakeRealName(szNewRealName2, pszTitle, '!', TRUE);
            if (_wpSetTitleAndRenameFile(somSelf,
                                         pszTitle,
                                         0))
                // no error:
                ulrc = NAMECLASH_NONE;
            else
                ulrc = NAMECLASH_CANCEL;
        }
    } // end if (_somIsA(somSelf, _WPFileSystem))

    return (ulrc);
}

/*
 *@@ fopsMoveObjectConfirmed:
 *      moves an object to the specified target folder.
 *
 *      This calls _wpConfirmObjectTitle before to
 *      check whether that object already exists in the
 *      target folder. If "Replace file exists" has been
 *      enabled, this will call the XWorkplace replacement
 *      method (XFldObject::wpConfirmObjectTitle).
 *      This handles the return codes correctly.
 *
 *      Returns TRUE if the object has been moved.
 *
 *@@added V0.9.2 (2000-03-04) [umoeller]
 *@@changed V0.9.3 (2000-04-28) [umoeller]: "replace" destroyed pObject; fixed
 */

BOOL fopsMoveObjectConfirmed(WPObject *pObject,
                             WPFolder *pTargetFolder)
{
    // check if object exists in that folder already
    // (this might call the XFldObject replacement)
    WPObject    *pReplaceThis = NULL;
    CHAR        szNewTitle[CCHMAXPATH] = "";
    BOOL        fDoMove = TRUE,
                fDidMove = FALSE;
    ULONG       ulAction;

    strcpy(szNewTitle, _wpQueryTitle(pObject));
    ulAction = _wpConfirmObjectTitle(pObject,      // object
                                     pTargetFolder, // folder
                                     &pReplaceThis,  // object to replace (if NAMECLASH_REPLACE)
                                     szNewTitle,     // in/out: object title
                                     sizeof(szNewTitle),
                                     0x006B);        // move code

    // _Pmpf(("    _wpConfirmObjectTitle returned %d", ulAction));

    switch (ulAction)
    {
        case NAMECLASH_CANCEL:
            fDoMove = FALSE;
        break;

        case NAMECLASH_RENAME:
            _wpSetTitle(pObject,
                        szNewTitle);      // set by wpConfirmObjectTitle
        break;

        case NAMECLASH_REPLACE:
            // delete the object to be replaced;
            // if this was successfull, move pObject there
            fDoMove = _wpFree(pReplaceThis);
            /* fDoMove = FALSE;
            fDidMove = _wpReplaceObject(pObject,
                                        pReplaceThis,       // set by wpConfirmObjectTitle
                                        TRUE);              // move and replace
               */
                    // after this pObject is deleted, so
                    // this cannot be used any more
        break;

        // NAMECLASH_NONE: just go on
    }

    if (fDoMove)
        fDidMove = _wpMoveObject(pObject, pTargetFolder);

    return (fDidMove);
}


