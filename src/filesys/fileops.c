
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
 *          --  The bottom layer is independent of the GUI and uses
 *              callbacks for reporting progress and errors. This
 *              is different from the ugly WPS implementation where
 *              you get message boxes when calling a file operations
 *              method, and there's no way to suppress them. (Try
 *              _wpFree on a read-only file system object.)
 *
 *              This bottom layer operates on "file task lists" which
 *              tell it what to do and which objects need to be
 *              processed. See fopsCreateFileTaskList for details.
 *              This list is then passed to the File thread which
 *              starts processing the objects. See fopsStartTaskAsync
 *              for details.
 *
 *          --  The top layer then uses the bottom layer for the
 *              XWorkplace file operations. This implements a generic
 *              progress window and proper error handling (hopefully).
 *
 *              You can use fopsStartTaskFromCnr and fopsStartTaskFromList
 *              to have the top layer start processing. These functions
 *              return quickly after collecting the objects, while processing
 *              then runs asynchronously on the File thread.
 *
 *              Concrete implementations currently are
 *              fopsStartDeleteFromCnr, fopsStartTrashRestoreFromCnr,
 *              and fopsStartTrashDestroyFromCnr for the XWorkplace
 *              trash can (XWPTrashCan class).
 *
 *          The framework uses the special FOPSRET error return type,
 *          which is FOPSERR_OK (0) if no error occured.
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
 *   Trash can setup                                                *
 *                                                                  *
 ********************************************************************/

/*
 *@@ fopsTrashCanReady:
 *      returns TRUE if the trash can classes are
 *      installed and the default trash can exists.
 *
 *      This does not check for whether "delete to
 *      trash can" is enabled. Query
 *      GLOBALSETTINGS.fTrashDelete to find out.
 *
 *@@added V0.9.1 (2000-02-01) [umoeller]
 */

BOOL fopsTrashCanReady(VOID)
{
    BOOL brc = FALSE;
    M_XWPTrashCan       *pXWPTrashCanClass = _XWPTrashCan;
    M_XWPTrashObject    *pXWPTrashObjectClass = _XWPTrashObject;

    if ((pXWPTrashCanClass) && (pXWPTrashObjectClass))
    {
        if (_xwpclsQueryDefaultTrashCan(pXWPTrashCanClass))
            brc = TRUE;
    }

    return (brc);
}

/*
 *@@ fopsEnableTrashCan:
 *      enables or disables the XWorkplace trash can
 *      altogether after displaying a confirmation prompt.
 *
 *      This does all of the following:
 *      -- (de)register XWPTrashCan and XWPTrashObject;
 *      -- enable "delete into trashcan" support;
 *      -- create or destroy the default trash can.
 *
 *@@added V0.9.1 (2000-02-01) [umoeller]
 */

BOOL fopsEnableTrashCan(HWND hwndOwner,     // for message boxes
                        BOOL fEnable)
{
    BOOL    brc = FALSE;

    if (fEnable)
    {
        // enable:
        M_XWPTrashCan       *pXWPTrashCanClass = _XWPTrashCan;
        M_XWPTrashObject    *pXWPTrashObjectClass = _XWPTrashObject;

        BOOL    fCreateObject = FALSE;

        if (    (!winhIsClassRegistered("XWPTrashCan"))
             || (!winhIsClassRegistered("XWPTrashObject"))
           )
        {
            // classes not registered yet:
            if (cmnMessageBoxMsg(hwndOwner,
                                 148,       // XWPSetup
                                 170,       // "register trash can?"
                                 MB_YESNO)
                    == MBID_YES)
            {
                CHAR szRegisterError[500];

                HPOINTER hptrOld = winhSetWaitPointer();

                if (WinRegisterObjectClass("XWPTrashCan",
                                           (PSZ)cmnQueryMainModuleFilename()))
                    if (WinRegisterObjectClass("XWPTrashObject",
                                               (PSZ)cmnQueryMainModuleFilename()))
                    {
                        fCreateObject = TRUE;
                        brc = TRUE;
                    }

                WinSetPointer(HWND_DESKTOP, hptrOld);

                if (!brc)
                    // error:
                    cmnMessageBoxMsg(hwndOwner,
                                     148,
                                     171, // "error"
                                     MB_CANCEL);
            }
        }
        else
            fCreateObject = TRUE;

        if (fCreateObject)
        {
            XWPTrashCan *pDefaultTrashCan = NULL;

            if (!wpshQueryObjectFromID(XFOLDER_TRASHCANID, NULL))
            {
                brc = setCreateStandardObject(hwndOwner,
                                              213,        // XWPTrashCan
                                              FALSE);     // XWP object
            }
            else
                brc = TRUE;

            if (brc)
            {
                GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(3000);
                pGlobalSettings->fTrashDelete = TRUE;
                cmnUnlockGlobalSettings();
            }
        }
    } // end if (fEnable)
    else
    {
        GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(3000);
        pGlobalSettings->fTrashDelete = FALSE;
        cmnUnlockGlobalSettings();

        if (krnQueryLock())
            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                   "Global lock already requested.");
        else
        {
            // disable:
            if (cmnMessageBoxMsg(hwndOwner,
                                 148,       // XWPSetup
                                 172,       // "deregister trash can?"
                                 MB_YESNO | MB_DEFBUTTON2)
                    == MBID_YES)
            {
                XWPTrashCan *pDefaultTrashCan = _xwpclsQueryDefaultTrashCan(_XWPTrashCan);
                if (pDefaultTrashCan)
                    _wpFree(pDefaultTrashCan);
                WinDeregisterObjectClass("XWPTrashCan");
                WinDeregisterObjectClass("XWPTrashObject");

                cmnMessageBoxMsg(hwndOwner,
                                 148,       // XWPSetup
                                 173,       // "done, restart WPS"
                                 MB_OK);
            }
        }
    }

    cmnStoreGlobalSettings();

    return (brc);
}

/* ******************************************************************
 *                                                                  *
 *   Expanded object lists                                          *
 *                                                                  *
 ********************************************************************/

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
 */

PLINKLIST fopsFolder2ExpandedList(WPFolder *pFolder,
                                  PULONG pulSizeContents) // out: size of all objects on list
{
    PLINKLIST   pll = lstCreate(FALSE);       // do not free the items
    ULONG       ulSizeContents = 0;

    BOOL fFolderLocked = FALSE;

    #ifdef DEBUG_TRASHCAN
        _Pmpf(("Object \"%s\" is a folder, creating SFL", _wpQueryTitle(pFolder) ));
    #endif

    // lock folder for querying content
    TRY_LOUD(excpt1, NULL)
    {
        fFolderLocked = !_wpRequestObjectMutexSem(pFolder, 5000);
        if (fFolderLocked)
        {
            WPObject *pObject;

            // pre-resolve _wpQueryContent for speed V0.9.3 (2000-04-28) [umoeller]
            somTD_WPFolder_wpQueryContent rslv_wpQueryContent
                    = SOM_Resolve(pFolder, WPFolder, wpQueryContent);

            wpshCheckIfPopulated(pFolder);

            // now collect all objects in folder
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
                fSOI = fopsExpandObjectDeep(pObject);
                ulSizeContents += fSOI->ulSizeThis;
                lstAppendItem(pll, fSOI);
            }
        }
    }
    CATCH(excpt1) {} END_CATCH();

    if (fFolderLocked)
    {
        _wpReleaseObjectMutexSem(pFolder);
        fFolderLocked = FALSE;
    }

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
 *      Call fopsFreeExpandedObject to free the item returned
 *      by this function.
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
 */

PEXPANDEDOBJECT fopsExpandObjectDeep(WPObject *pObject)
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
                // fill list
                pSOI->pllContentsSFL = fopsFolder2ExpandedList(pObject,
                                                      &pSOI->ulSizeThis);
                                                        // out: size of files on list
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
 *      After the call, pllObjects contains all the
 *      plain WPObject* pointers, so the list should
 *      have been created with lstCreate(FALSE) in
 *      order not to invoke free() on it.
 *
 *      If folders are found, contained objects are
 *      appended to the list before the folder itself.
 *      Otherwise, objects are returned in the order
 *      in which wpQueryContent returns them, which
 *      is dependent on the underlying file system.
 *      and should not be relied upon.
 *
 *      For example, see the following folder/file hierarchy:
 +
 +          folder1
 +             +--- folder2
 +             |      +---- text1.txt
 +             |      +---- text2.txt
 +             +--- text3.txt
 *
 *      If fopsExpandObjectFlat(folder1) is invoked,
 *      you get the following order in the list:
 *
 +      text1.txt, text2.txt, folder2, text3.txt, folder1.
 *
 *      pllObjects is not cleared by this function,
 *      so you can call this several times with the
 *      same list.
 *
 *@@added V0.9.3 (2000-04-27) [umoeller]
 *@@changed V0.9.3 (2000-04-28) [umoeller]: now pre-resolving wpQueryContent for speed
 */

FOPSRET fopsExpandObjectFlat(PLINKLIST pllObjects,  // in: list to append to
                             WPObject *pObject,     // in: object to start with
                             PULONG pulObjectCount) // out: objects count on list;
                                                    // the ULONG must be set to 0 before calling this!
{
    FOPSRET frc = FOPSERR_OK;

    if (_somIsA(pObject, _WPFolder))
    {
        // it's a folder:
        // lock it and build a list from its contents
        BOOL fFolderLocked = FALSE;

        TRY_LOUD(excpt1, NULL)
        {
            fFolderLocked = !_wpRequestObjectMutexSem(pObject, 5000);
            if (fFolderLocked)
            {
                WPObject *pSubObject;

                // pre-resolve _wpQueryContent for speed V0.9.3 (2000-04-28) [umoeller]
                somTD_WPFolder_wpQueryContent rslv_wpQueryContent
                        = SOM_Resolve(pObject, WPFolder, wpQueryContent);

                wpshCheckIfPopulated(pObject);

                // now collect all objects in folder
                for (pSubObject = rslv_wpQueryContent(pObject, NULL, QC_FIRST);
                     pSubObject;
                     pSubObject = rslv_wpQueryContent(pObject, pSubObject, QC_Next))
                {
                    // recurse!
                    // this will add pSubObject to pllObjects
                    frc = fopsExpandObjectFlat(pllObjects,
                                               pSubObject,
                                               pulObjectCount);
                    if (frc != FOPSERR_OK)
                        // error:
                        break;
                } // end for
            }
            else
                frc = FOPSERR_LOCK_FAILED;

        }
        CATCH(excpt1) {} END_CATCH();

        if (fFolderLocked)
        {
            _wpReleaseObjectMutexSem(pObject);
            fFolderLocked = FALSE;
        }
    }

    // append this object to the list;
    // since we have recursed first, the folder
    // contents come before the folder in the list
    _Pmpf(("Appending %s", _wpQueryTitle(pObject) ));
    lstAppendItem(pllObjects, pObject);
    if (pulObjectCount)
        (*pulObjectCount)++;

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
                    PrfWriteProfileString(HINI_USER, INIAPP_XWORKPLACE,
                                          INIKEY_NAMECLASHFOCUS,
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
 */

WPFileSystem* fopsFindFSWithSameName(WPFileSystem *somSelf,   // in: FS object to check for
                                     WPFolder *pFolder,       // in: folder to check
                                     PSZ pszFullFolder,       // in: full path spec of pFolder
                                     BOOL fCheckTitle,
                                     PSZ pszName2Check)       // out: new real name
{
    CHAR    szTitleOrName[1000];

    // this is not trivial, since we need to take
    // in account all the FAT and HPFS file name
    // conversion.
    // 1)   get real name or title
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
    doshMakeRealName(pszName2Check,     // target buffer
                     szTitleOrName,     // source
                     '!',               // replace-with char
                     doshIsFileOnFAT(pszFullFolder));

    #ifdef DEBUG_TITLECLASH
        _Pmpf(("  Checking for existence of %s", pszName2Check));
    #endif

    // 3) does this file exist in pFolder?
    return (wpshContainsFile(pFolder, pszName2Check));
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
 */

BOOL fopsProposeNewTitle(PSZ pszTitle,          // in: title to modify
                         WPFolder *pFolder,     // in: folder to check for existing titles
                         PSZ pszProposeTitle)   // out: buffer for new title
{
    CHAR    szTemp[500],
            szFileCount[20];
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
    strcpy(szTemp, pszTitle);
    // check if title contains a ':num' already
    p2 = strchr(szTemp, ':');
    if (p2)
    {
        lFileCount = atoi(p2+1);
        if (lFileCount)
        {
            // if we have a valid number following ":", remove it
            p = strchr(szTemp, '.');
            if (p)
                strcpy(p2, p);
            else
                *p2 = '\0';
        }
    }

    // insert new number
    p = strrchr(szTemp, '.');       // last dot == extension
    lFileCount = 1;
    do
    {
        WPFileSystem    *pExistingFile2 = 0;
        BOOL            fFolderLocked = FALSE;

        fExit = FALSE;

        sprintf(szFileCount, ":%d", lFileCount);
        if (p)
        {
            // has extension: insert
            ULONG ulBeforeDot = (p - szTemp);
            // we don't care about FAT, because
            // we propose titles, not real names
            // (V0.84)
            strncpy(pszProposeTitle, szTemp, ulBeforeDot);
            pszProposeTitle[ulBeforeDot] = '\0';
            strcat(pszProposeTitle, szFileCount);
            strcat(pszProposeTitle, p);
        }
        else
        {
            // has no extension: append
            strncpy(pszProposeTitle, szTemp, 255);
            // we don't care about FAT, because
            // we propose titles, not real names
            // (V0.84)
            strcat(pszProposeTitle, szFileCount);
        }
        lFileCount++;

        if (lFileCount > 99)
            // avoid endless loops
            break;

        // now go through the folder content and
        // check whether we already have a file
        // with the proposed title (not real name!)
        // (V0.84)
        TRY_LOUD(excpt1, NULL)
        {
            fFolderLocked = !_wpRequestObjectMutexSem(pFolder, 5000);
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
                        break;
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
            _wpReleaseObjectMutexSem(pFolder);

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
    COUNTRYSETTINGS     cs;
    prfhQueryCountrySettings(&cs);

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
    xstrrpl(&pszTemp, 0, "%1", pszRealNameFound, 0);
    xstrrpl(&pszTemp, 0, "%2", pszFolder, 0);
    WinSetDlgItemText(hwndConfirm, ID_XFDI_CLASH_TXT1,
                      pszTemp);
    free(pszTemp);

    // set object information fields
    _wpQueryFilename(pFSExisting, szExistingFilename, TRUE);
    DosQueryPathInfo(szExistingFilename,
                     FIL_STANDARD,
                     &fs3, sizeof(fs3));
    strhFileDate(szTemp, &(fs3.fdateLastWrite),
                 cs.ulDateFormat, cs.cDateSep);
    WinSetDlgItemText(hwndConfirm, ID_XFDI_CLASH_DATEOLD, szTemp);
    strhFileTime(szTemp, &(fs3.ftimeLastWrite),
                 cs.ulTimeFormat, cs.cTimeSep);
    WinSetDlgItemText(hwndConfirm, ID_XFDI_CLASH_TIMEOLD, szTemp);

    strhThousandsULong(szTemp, fs3.cbFile, // )+512) / 1024 ,
                       cs.cThousands);
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
                     cs.ulDateFormat, cs.cDateSep);
        WinSetDlgItemText(hwndConfirm, ID_XFDI_CLASH_DATENEW, szTemp);
        strhFileTime(szTemp, &(fs3.ftimeLastWrite),
                     cs.ulTimeFormat, cs.cTimeSep);
        WinSetDlgItemText(hwndConfirm, ID_XFDI_CLASH_TIMENEW, szTemp);
        strhThousandsULong(szTemp, fs3.cbFile, // )+512) / 1024,
                           cs.cThousands);
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
    ulLastFocusID = PrfQueryProfileInt(HINI_USER, INIAPP_XWORKPLACE,
                                       INIKEY_NAMECLASHFOCUS,
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
 */

ULONG fopsConfirmObjectTitle(WPObject *somSelf,
                             WPFolder *Folder,
                             WPObject **ppDuplicate,
                             PSZ pszTitle,
                             ULONG cbTitle,
                             ULONG menuID)
{
    ULONG ulrc = NAMECLASH_NONE;

    // nameclashs are only a problem for file-system objects,
    // and only if we're not creating shadows
    if (    (_somIsA(somSelf, _WPFileSystem))
         && (menuID != 0x13C) // "create shadow" code
         && (menuID != 0x0065) // "create another" code; V0.9.3 (2000-04-08) [umoeller]
                        // wrong, still not working ###
       )
    {
        CHAR            szFolder[CCHMAXPATH],
                        szRealNameFound[CCHMAXPATH];
        WPFileSystem    *pFSExisting = NULL;

        // check whether the target folder is valid
        if (!Folder)
            return (NAMECLASH_CANCEL);

        if (!_wpQueryFilename(Folder, szFolder, TRUE))
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

/********************************************************************
 *                                                                  *
 *   Generic file tasks framework                                   *
 *                                                                  *
 ********************************************************************/

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

    ULONG       ulUser;                 // user parameter specified with fopsCreateFileTaskList
} FILETASKLIST, *PFILETASKLIST;

/*
 *@@ fopsCreateFileTaskList:
 *      creates a new task list for the given file operation
 *      to which items can be added using fopsAddFileTask.
 *      Part of the bottom layer of the XWorkplace file
 *      operations engine.
 *
 *      To start such a file task, three steps are required:
 *
 *      -- call fopsCreateFileTaskList (this function;
 *
 *      -- add objects to that list using fopsAddFileTask;
 *
 *      -- pass the task to the File thread using fopsStartTaskAsync,
 *         OR delete the list using fopsDeleteFileTaskList.
 *
 *      WARNING: This function requests the object mutex semaphore
 *      for pSourceFolder. You must add objects to the returned
 *      task list (using fopsAddFileTask) as quickly as possible.
 *
 *      The mutex will only be released after all
 *      file processing has been completed (thru
 *      fopsStartTaskAsync) or if you call
 *      fopsDeleteFileTaskList explicitly. So you MUST
 *      call either one of those two functions after a
 *      task list was successfully created using this
 *      function.
 *
 *      Returns NULL upon errors, e.g. because a folder
 *      mutex could not be accessed.
 *
 *      This is usually called on the thread of the folder view
 *      from which the file operation was started (mostly
 *      thread 1).
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
 *@@added V0.9.1 (2000-01-27) [umoeller]
 *@@changed V0.9.2 (2000-03-04) [umoeller]: added error callback
 */

HFILETASKLIST fopsCreateFileTaskList(ULONG ulOperation,     // in: XFT_* flag
                                     WPFolder *pSourceFolder,
                                     WPFolder *pTargetFolder,
                                     FNFOPSPROGRESSCALLBACK *pfnProgressCallback, // in: callback procedure
                                     FNFOPSERRORCALLBACK *pfnErrorCallback, // in: error callback
                                     ULONG ulUser)          // in: user parameter for callback
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
        lstInit(&pftl->llObjects, FALSE);     // no freeing, this stores WPObject pointers!
        pftl->ulOperation = ulOperation;
        pftl->pSourceFolder = pSourceFolder;
        pftl->fSourceLocked = fSourceLocked;
        pftl->pTargetFolder = pTargetFolder;
        pftl->fTargetLocked = FALSE;
        pftl->ptlTarget.x = 0;
        pftl->ptlTarget.y = 0;
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
 *      If an error has been found and (pfnErrorCallback != NULL),
 *      that callback gets called and its return value is returned.
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
 */

FOPSRET fopsValidateObjOperation(ULONG ulOperation,        // in: operation
                                 FNFOPSERRORCALLBACK *pfnErrorCallback, // in: error callback or NULL
                                 WPObject *pObject)        // in: current object to check
{
    FOPSRET frc = FOPSERR_OK;
    BOOL    fPromptUser = TRUE;

    // error checking
    switch (ulOperation)
    {
        case XFT_MOVE2TRASHCAN:
            // prohibit "move to trash" for trash objects themselves
            /* if (_somIsA(pObject, _XWPTrashObject))
            {
                frc = FOPSERR_NODELETETRASHOBJECT;
                fPromptUser = FALSE;
            }
            // prohibit "move to trash" for no-delete objects
            else */
            if (!_wpIsDeleteable(pObject))
                // this includes trash objects
                if (_somIsA(pObject, _WPFileSystem))
                    // this is recoverable, but prompt the user:
                    frc = FOPSERR_MOVE2TRASH_READONLY;
                else
                {
                    // abort right away
                    frc = FOPSERR_MOVE2TRASH_NOT_DELETABLE;
                    fPromptUser = FALSE;
                }
            else if (!trshIsOnSupportedDrive(pObject))
                frc = FOPSERR_TRASHDRIVENOTSUPPORTED;
        break;

        /* case XFT_DESTROYTRASHOBJECTS:
            // allow "destroy trash object" only for trash objects
            if (!_somIsA(pObject, _XWPTrashObject))
                frc = FOPSERR_DESTROYNONTRASHOBJECT;
        break; */

        case XFT_TRUEDELETE:
            // ###
        break;
    }

    if (frc != FOPSERR_OK)
        if ((pfnErrorCallback) && (fPromptUser))
            // prompt user:
            // this can recover
            frc = (pfnErrorCallback)(ulOperation, pObject, frc);
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
 *      Note: This function is reentrant only for the
 *      same HFILETASKLIST. Once fopsStartTaskAsync
 *      has been called for the task list, you must not
 *      modify the task list again.
 *      There's no automatic protection against this!
 *      So call this function only AFTER fopsCreateFileTaskList
 *      and BEFORE fopsStartTaskAsync.
 *
 *      This calls fopsValidateObjOperation on the object.
 *
 *      This returns the error code of fopsValidateObjOperation.
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
                                   pObject);

    if (frc == FOPSERR_OK)
        // proceed:
        if (lstAppendItem(&pftl->llObjects, pObject) == NULL)
            // error:
            frc = FOPSERR_INTEGRITY_ABORT;

    return (frc);
}

/*
 *@@ fopsStartTaskAsync:
 *      this starts processing the tasks which have
 *      been previously added to the file tasks lists
 *      using fopsAddFileTask.
 *      Part of the bottom layer of the XWorkplace file
 *      operations engine.
 *
 *      This function returns almost immediately
 *      because further processing is done on the
 *      File thread (xthreads.c).
 *
 *      DO NOT WORK on the task list passed to this
 *      function once this function has been called.
 *      The File thread uses it and destroys it
 *      automatically when done.
 *
 *@@added V0.9.1 (2000-01-27) [umoeller]
 *@@changed V0.9.3 (2000-04-25) [umoeller]: renamed from fopsStartProcessingTasks
 */

BOOL fopsStartTaskAsync(HFILETASKLIST hftl)
{
    PFILETASKLIST pftl = (PFILETASKLIST)hftl;
    // unlock the folders so the file thread
    // can start working
    if (pftl->fSourceLocked)
    {
        _wpReleaseObjectMutexSem(pftl->pSourceFolder);
        pftl->fSourceLocked = FALSE;
    }

    // have File thread process this list;
    // this calls fopsFileThreadProcessing below
    return (xthrPostFileMsg(FIM_PROCESSTASKLIST,
                            (MPARAM)hftl,
                            (MPARAM)NULL));
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
 */

FOPSRET fopsFileThreadTrueDelete(HFILETASKLIST hftl,
                                 FOPSUPDATE *pfu,       // in: update structure in fopsFileThreadProcessing
                                 WPObject **ppObject)   // in: object to delete;
                                                        // out: failing object if error
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

            // all objects collected:
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
                        frc = FOPSERR_DELETE_READONLY;      // non-fatal
                    else
                        frc = FOPSERR_DELETE_NOT_DELETABLE; // fatal

                    if (pftl->pfnErrorCallback)
                        // prompt user:
                        // this can recover, but should have
                        // changed the problem (read-only)
                        frc = (pftl->pfnErrorCallback)(pftl->ulOperation,
                                                       pObjThis,
                                                       frc);
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
 *      automatically  after fopsStartTaskAsync has
 *      been called.
 *
 *      This runs on the File thread.
 *
 *@@added V0.9.1 (2000-01-29) [umoeller]
 *@@changed V0.9.3 (2000-04-25) [umoeller]: reworked error management
 *@@changed V0.9.3 (2000-04-26) [umoeller]: added "true delete" support
 *@@changed V0.9.3 (2000-04-30) [umoeller]: reworked progress reports
 */

VOID fopsFileThreadProcessing(HFILETASKLIST hftl)
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
                _Pmpf(("    %d items on list", fu.ulTotalObjects));
            #endif

            /*
             * 2) process objects on list
             *
             */

            while (pNode)
            {
                WPObject    *pObjectThis = (WPObject*)pNode->pItemData;

                #ifdef DEBUG_TRASHCAN
                    _Pmpf(("    checking object 0x%lX", pObjectThis));
                #endif

                // check if object is still valid
                if (wpshCheckObject(pObjectThis))
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
                else
                    // error:
                    break;

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
                     * XFT_DESTROYTRASHOBJECTS:
                     *
                     */

                    /* case XFT_DESTROYTRASHOBJECTS:
                        #ifdef DEBUG_TRASHCAN
                            _Pmpf(("  fopsFileThreadProcessing: destroying %s",
                                    _wpQueryTitle(pObjectThis) ));
                        #endif
                        if (!_xwpDestroyTrashObject(pObjectThis)) // trash object
                            frc = FOPSERR_NOT_HANDLED_ABORT;

                        // after this pObjectThis is invalid!
                    break; */

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
                                                       &pObjectThis);
                                           // after this pObjectThis is invalid!
                    break;
                }

                if (frc != FOPSERR_OK)
                {
                    CHAR szMsg[1000];
                    sprintf(szMsg,
                            "Error %d with object %s",
                            frc,
                            _wpQueryTitle(pObjectThis));
                    DebugBox(NULLHANDLE,
                             "XWorkplace File thread",
                             szMsg);
                }

                pNode = pNode->pNext;
                ulCurrentObject++;
                fu.ulProgressScalar = ulCurrentObject * 100; // for progress
            } // end while (pNode)

            // call progress callback to say  "done"
            fu.flProgress = FOPSPROG_LASTCALL_DONE;
            fopsCallProgressCallback(pftl,
                                     0,         // no update flags, just "done"
                                     TRUE,      // update progress
                                     &fu);         // NULL means done
        } // end if (pftl)
    }
    CATCH(excpt2)
    {
    } END_CATCH();

    fopsDeleteFileTaskList(hftl);
            // this unlocks the folders also
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
 *      called fopsStartTaskAsync because
 *      then the File thread is working on the
 *      list already. The file list will automatically
 *      get cleaned up then, so you only need to
 *      call this function yourself if you choose
 *      NOT to call fopsStartTaskAsync.
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

/********************************************************************
 *                                                                  *
 *   Generic file operations implementation                         *
 *                                                                  *
 ********************************************************************/

/*
 *@@ GENERICPROGRESSWINDATA:
 *      window data stored in QWL_USER of
 *      fops_fnwpGenericProgress.
 *
 *@@added V0.9.1 (2000-01-31) [umoeller]
 */

typedef struct _GENERICPROGRESSWINDATA
{
    HWND        hwndProgress;               // progress window handle
    HWND        hwndProgressBar;            // progress bar control (cached for speed)
    BOOL        fCancelPressed;             // TRUE after Cancel has been
                                            // pressed; the File thread is then
                                            // halted while a confirmation is
                                            // displayed
    ULONG       ulIgnoreFlags;              // initially 0; later set to ...
} GENERICPROGRESSWINDATA, *PGENERICPROGRESSWINDATA;

/*
 *@@ fopsGenericProgressCallback:
 *      progress callback which gets specified when
 *      fopsStartTaskFromCnr calls fopsCreateFileTaskList.
 *      Part of the top layer of the XWorkplace file
 *      operations engine.
 *
 *      This assumes that a progress window using
 *      fops_fnwpGenericProgress has been created and
 *      ulUser is a PGENERICPROGRESSWINDATA.
 *
 *      Warning: This runs on the File thread. That's why
 *      we send a message to the progress window so that
 *      the File thread is blocked while the progress window
 *      does its processing.
 *
 *@@added V0.9.1 (2000-01-30) [umoeller]
 */

BOOL APIENTRY fopsGenericProgressCallback(PFOPSUPDATE pfu,
                                          ULONG ulUser)
{
    PGENERICPROGRESSWINDATA ppwd = (PGENERICPROGRESSWINDATA)ulUser;

    return ((BOOL)WinSendMsg(ppwd->hwndProgress,
                             XM_UPDATE,
                             (MPARAM)pfu,
                             (MPARAM)0));
}

/*
 *@@ fopsGenericErrorCallback:
 *      error callback which gets specified when
 *      fopsStartTaskFromCnr calls fopsCreateFileTaskList.
 *      Part of the top layer of the XWorkplace file
 *      operations engine.
 *
 *      This is usually called on the thread of the folder view
 *      from which the file operation was started (mostly
 *      thread 1).
 *
 *      This thing displays a confirmation box only for errors
 *      which apply to a single trash object and are not recoverable.
 *      For other errors, the error code is simply returned to the
 *      caller, which in turn passes the error code to its caller(s).
 *
 *@@added V0.9.2 (2000-03-04) [umoeller]
 *@@changed V0.9.3 (2000-04-25) [umoeller]: reworked error management
 */

FOPSRET APIENTRY fopsGenericErrorCallback(ULONG ulOperation,
                                          WPObject *pObject,
                                          FOPSRET frError)
{
    FOPSRET frc = FOPSERR_OK;
    CHAR    szMsg[1000];
    PSZ     apsz[5] = {0};
    ULONG   cpsz = 0,
            ulMsg = 0,
            flFlags = 0;

    switch (frError)
    {
        /* case FOPSERR_NODELETETRASHOBJECT:
        case FOPSERR_DESTROYNONTRASHOBJECT:
            ulMsg = 174;        // "invalid for XWPTrashObject"
            flFlags = MB_CANCEL;
        break; */

        case FOPSERR_MOVE2TRASH_READONLY:
            // this comes in for a "move2trash" operation
            // on read-only WPFileSystems
            ulMsg = 179;
            flFlags = MB_YESNO | MB_DEFBUTTON2;
            apsz[0] = _wpQueryTitle(pObject);
            cpsz = 1;
        break;

        case FOPSERR_DELETE_READONLY:
            // this comes in for a "delete" operation
            // on read-only WPFileSystems
            ulMsg = 184;
            flFlags = MB_YESNO | MB_DEFBUTTON2;
            apsz[0] = _wpQueryTitle(pObject);
            cpsz = 1;
                // if we return FOPSERR_OK after this,
                // the fileops engine unlocks the file
        break;

        case FOPSERR_DELETE_NOT_DELETABLE:
            // object not deleteable:
            ulMsg = 185;
            flFlags = MB_CANCEL;
            apsz[0] = _wpQueryTitle(pObject);
            cpsz = 1;
        break;

        /* case FOPSERR_TRASHDRIVENOTSUPPORTED: pass this to caller
            ulMsg = 176;
            flFlags = MB_CANCEL;
        break; */
    }

    if (flFlags)
    {
        ULONG ulrc = cmnMessageBoxMsgExt(NULLHANDLE,
                                         175,
                                         apsz,
                                         cpsz,
                                         ulMsg,
                                         flFlags);
        if (    (ulrc == MBID_OK)
             || (ulrc == MBID_YES)
           )
            return (FOPSERR_OK);
    }

    return (frError);
}

/*
 *@@ fops_fnwpGenericProgress:
 *      dialog procedure for the standard XWorkplace
 *      file operations status window. This updates
 *      the window's status bar and the informational
 *      static texts for the objects and folders.
 *
 *      Part of the top layer of the XWorkplace file
 *      operations engine.
 *
 *      This receives messages from fopsGenericProgressCallback.
 *
 *@@added V0.9.1 (2000-01-30) [umoeller]
 */

MRESULT EXPENTRY fops_fnwpGenericProgress(HWND hwndProgress, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    switch (msg)
    {
        case WM_INITDLG:
        {
            cmnSetControlsFont(hwndProgress, 0, 5000);
            ctlProgressBarFromStatic(WinWindowFromID(hwndProgress, ID_SDDI_PROGRESSBAR),
                                     PBA_ALIGNCENTER | PBA_BUTTONSTYLE);
            mrc = WinDefDlgProc(hwndProgress, msg, mp1, mp2);
        break; }

        case WM_COMMAND:
            switch (SHORT1FROMMP(mp1))
            {
                case DID_CANCEL:
                {
                    PGENERICPROGRESSWINDATA ppwd = WinQueryWindowPtr(hwndProgress, QWL_USER);
                    WinEnableControl(hwndProgress, DID_CANCEL, FALSE);
                    ppwd->fCancelPressed = TRUE;
                break; }

                default:
                    mrc = WinDefDlgProc(hwndProgress, msg, mp1, mp2);
            }
        break;

        /*
         * XM_UPDATE:
         *      has a FOPSUPDATE structure in mp1.
         *      If this returns FALSE, processing
         *      is aborted in fopsGenericProgressCallback.
         */

        case XM_UPDATE:
        {
            PGENERICPROGRESSWINDATA ppwd = WinQueryWindowPtr(hwndProgress, QWL_USER);
            PFOPSUPDATE pfu = (PFOPSUPDATE)mp1;

            // return value: TRUE means continue
            mrc = (MRESULT)TRUE;

            if (pfu)
                if ((pfu->flProgress & FOPSPROG_LASTCALL_DONE) == 0)
                {
                    CHAR    szTargetFolder[CCHMAXPATH] = "";
                    PSZ     pszTargetFolder = NULL;
                    PSZ     pszSubObject = NULL;

                    // update source folder?
                    if (pfu->flChanged & FOPSUPD_SOURCEFOLDER_CHANGED)
                    {
                        CHAR        szSourceFolder[CCHMAXPATH] = "";
                        if (pfu->pSourceFolder)
                            _wpQueryFilename(pfu->pSourceFolder, szSourceFolder, TRUE);
                        WinSetDlgItemText(hwndProgress,
                                          ID_XSDI_SOURCEFOLDER,
                                          szSourceFolder);
                    }

                    // update target folder?
                    if (pfu->flChanged & FOPSUPD_TARGETFOLDER_CHANGED)
                    {
                        if (pfu->pTargetFolder)
                        {
                            _wpQueryFilename(pfu->pTargetFolder, szTargetFolder, TRUE);
                            pszTargetFolder = szTargetFolder;
                        }
                    }

                    if (pfu->flChanged & FOPSUPD_SOURCEOBJECT_CHANGED)
                    {
                        if (pfu->pSourceObject)
                            WinSetDlgItemText(hwndProgress,
                                              ID_XSDI_SOURCEOBJECT,
                                              _wpQueryTitle(pfu->pSourceObject));
                    }

                    if (pfu->flChanged & FOPSUPD_EXPANDING_SOURCEOBJECT_1ST)
                    {
                        // expanding source objects list:
                        PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();
                        pszSubObject = pNLSStrings->pszPopulating;
                                            // "Collecting objects..."
                    }

                    if (pfu->flChanged & FOPSUPD_SUBOBJECT_CHANGED)
                    {
                        if (pfu->pSubObject)
                            // can be null!
                            pszSubObject = _wpQueryTitle(pfu->pSubObject);
                        else
                            // clear:
                            pszSubObject = "";
                    }

                    // set controls text composed above
                    if (pszTargetFolder)
                        WinSetDlgItemText(hwndProgress,
                                          ID_XSDI_TARGETFOLDER,
                                          pszTargetFolder);

                    if (pszSubObject)
                        WinSetDlgItemText(hwndProgress,
                                          ID_XSDI_SUBOBJECT,
                                          pszSubObject);
                    // update status bar?
                    if (pfu->flProgress & FOPSPROG_UPDATE_PROGRESS)
                        WinSendMsg(WinWindowFromID(hwndProgress, ID_SDDI_PROGRESSBAR),
                                   WM_UPDATEPROGRESSBAR,
                                   (MPARAM)pfu->ulProgressScalar,
                                   (MPARAM)pfu->ulProgressMax);

                    // has cancel been pressed?
                    // (flag is set from WM_COMMAND)
                    if (ppwd->fCancelPressed)
                    {
                        // display "confirm cancel" message box;
                        // this msg box suspends the File thread because
                        // we're being SENT this message...
                        // so file operations are suspended until we
                        // return TRUE from here!
                        PSZ pszTitle = winhQueryWindowText(hwndProgress);
                        CHAR szMsg[1000];
                        cmnGetMessage(NULL, 0, szMsg, sizeof(szMsg),
                                      186);     // "really cancel?"
                        if (cmnMessageBox(hwndProgress, // owner
                                          pszTitle,
                                          szMsg,
                                          MB_RETRYCANCEL)
                                != MBID_RETRY)
                        {
                            // cancel: return FALSE
                            mrc = (MRESULT)FALSE;
                        }
                        else
                        {
                            WinEnableControl(hwndProgress, DID_CANCEL, TRUE);
                            ppwd->fCancelPressed = FALSE;
                        }

                        if (pszTitle)
                            free(pszTitle);
                    }
                } // end if (pfu)
                else
                {
                    // FOPSPROG_LASTCALL_DONE set:
                    // disable "Cancel"
                    WinEnableControl(hwndProgress, DID_CANCEL, FALSE);
                    if (!ppwd->fCancelPressed)
                        // set progress to 100%
                        WinSendMsg(ppwd->hwndProgressBar,
                                   WM_UPDATEPROGRESSBAR,
                                   (MPARAM)100,
                                   (MPARAM)100);
                    // start timer to destroy window
                    WinStartTimer(WinQueryAnchorBlock(hwndProgress),
                                  hwndProgress,
                                  1,
                                  500);
                }
        break; }

        case WM_TIMER:
            if ((SHORT)mp1 == 1)
            {
                // "destroy window" timer
                WinStopTimer(WinQueryAnchorBlock(hwndProgress),
                             hwndProgress,
                             1);

                // store window position
                winhSaveWindowPos(hwndProgress,
                                  HINI_USER,
                                  INIAPP_XWORKPLACE, INIKEY_FILEOPSPOS);
                // clean up
                free(WinQueryWindowPtr(hwndProgress, QWL_USER));
                WinDestroyWindow(hwndProgress);
            }
            else
                mrc = WinDefDlgProc(hwndProgress, msg, mp1, mp2);
        break;

        default:
            mrc = WinDefDlgProc(hwndProgress, msg, mp1, mp2);
    }

    return (mrc);
}

/*
 *@@ fopsStartWithGenericProgress:
 *      starts processing the given file task list
 *      with the generic XWorkplace file progress
 *      dialog (fops_fnwpGenericProgress).
 *      Part of the top layer of the XWorkplace file
 *      operations engine.
 *
 *      This calls fopsStartTaskAsync in turn.
 *
 *      Gets called by fopsStartTaskFromCnr.
 *
 *@@added V0.9.1 (2000-02-01) [umoeller]
 */

BOOL fopsStartWithGenericProgress(HFILETASKLIST hftl,
                                  PGENERICPROGRESSWINDATA ppwd)
{
    PSZ pszTitle = "unknown task";
    PFILETASKLIST pftl = (PFILETASKLIST)hftl;

    // load progress dialog
    ppwd->hwndProgress = WinLoadDlg(HWND_DESKTOP,        // parent
                                    NULLHANDLE, // owner
                                    fops_fnwpGenericProgress,
                                    cmnQueryNLSModuleHandle(FALSE),
                                    ID_XFD_FILEOPSSTATUS,
                                    NULL);

    ppwd->fCancelPressed = FALSE;
    ppwd->hwndProgressBar = WinWindowFromID(ppwd->hwndProgress,
                                            ID_SDDI_PROGRESSBAR);
    // store in window words
    WinSetWindowPtr(ppwd->hwndProgress, QWL_USER, ppwd);

    // determine title for the progress window ###
    switch (pftl->ulOperation)
    {
        case XFT_MOVE2TRASHCAN:
            pszTitle = "Moving to Trash Can...";
        break;

        case XFT_RESTOREFROMTRASHCAN:
            pszTitle = "Restoring from Trash Can...";
        break;

        // case XFT_DESTROYTRASHOBJECTS:
        case XFT_TRUEDELETE:
            pszTitle = "Deleting Objects...";
        break;
    }

    // set progress window title
    WinSetWindowText(ppwd->hwndProgress, pszTitle);

    winhCenterWindow(ppwd->hwndProgress);
    // get last window position from INI
    winhRestoreWindowPos(ppwd->hwndProgress,
                         HINI_USER,
                         INIAPP_XWORKPLACE, INIKEY_FILEOPSPOS,
                         // move only, no resize
                         SWP_MOVE | SWP_SHOW | SWP_ACTIVATE);
    // *** go!!!
    WinShowWindow(ppwd->hwndProgress, TRUE);

    return (fopsStartTaskAsync(hftl));
}

/*
 *@@ fopsStartTaskFromCnr:
 *      this sets up the generic file operations progress
 *      dialog (fops_fnwpGenericProgress), creates a file
 *      task list for the given job (fopsCreateFileTaskList),
 *      adds objects to that list (fopsAddObjectToTask), and
 *      starts processing (fopsStartTaskAsync), all in
 *      one shot.
 *
 *      Entry point to the top layer of the XWorkplace file
 *      operations engine.
 *
 *      The actual file operation calls are then made on the
 *      XWorkplace File thread (xthreads.c), which calls
 *      fopsFileThreadProcessing in this file.
 *
 *      pSourceObject must have the first object which is to
 *      be worked on, that is, the object for which the context
 *      menu had been opened. This should be the result of a
 *      previous wpshQuerySourceObject call while the context
 *      menu was open. This can be the object under the mouse
 *      or (if ulSelection == SEL_WHITESPACE) the folder itself
 *      also.
 *
 *      If (ulSelection == SEL_MULTISEL), this func will use
 *      wpshQueryNextSourceObject to get the other objects
 *      because several objects are to be deleted.
 *
 *      If you want the operation confirmed, specify message
 *      numbers with ulMsgSingle and ulMsgMultiple. Those numbers
 *      must match a message in the XWorkplace NLS text message
 *      file (XFLDRxxx.TMF).
 *
 *      -- If a single object is selected in the container,
 *         ulMsgSingle will be used. The "%1" parameter in the
 *         message will be replaced with the object's title.
 *
 *      -- If multiple objects are selected in the container,
 *         ulMsgMultiple will be used. The "%1" parameter in
 *         the message will then be replaced with the number
 *         of objects to be worked on.
 *
 *      If both msg numbers are null, no confirmation is displayed.
 *
 *      This returns FOPSERR_OK if the task was successfully started
 *      or some other error code if not.
 *
 *@@added V0.9.1 (2000-01-31) [umoeller]
 *@@changed V0.9.3 (2000-04-10) [umoeller]: added confirmation messages
 *@@changed V0.9.3 (2000-04-11) [umoeller]: fixed memory leak when errors occured
 *@@changed V0.9.3 (2000-04-25) [umoeller]: reworked error management
 *@@changed V0.9.3 (2000-04-28) [umoeller]: added fRelatedObject
 */

FOPSRET fopsStartTaskFromCnr(ULONG ulOperation,       // in: operation; see fopsCreateFileTaskList
                             WPFolder *pSourceFolder, // in: source folder, as required by fopsCreateFileTaskList
                             WPFolder *pTargetFolder, // in: target folder, as required by fopsCreateFileTaskList
                             WPObject *pSourceObject, // in: first object with source emphasis
                             ULONG ulSelection,       // in: SEL_* flags
                             BOOL fRelatedObjects,    // in: if TRUE, then the objects must be trash objects,
                                                      // and their related objects will be collected instead
                             HWND hwndCnr,            // in: container to get more source objects from
                             ULONG ulMsgSingle,       // in: msg no. in TMF msg file for single object
                             ULONG ulMsgMultiple)     // in: msg no. in TMF msg file for multiple objects
{
    FOPSRET frc = FOPSERR_OK;

    WPObject *pObject = pSourceObject;

    // allocate progress window data structure
    // this is passed to fopsCreateFileTaskList as ulUser
    PGENERICPROGRESSWINDATA ppwd = malloc(sizeof(GENERICPROGRESSWINDATA));

    #ifdef DEBUG_TRASHCAN
        _Pmpf(("fopsStartTaskFromCnr: first obj is %s", _wpQueryTitle(pObject)));
        _Pmpf(("        ulSelection: %d", ulSelection));
    #endif

    if (ppwd)
    {
        HFILETASKLIST hftl = NULLHANDLE;
        ULONG       cObjects = 0;

        // create task list for the desired task
        hftl = fopsCreateFileTaskList(ulOperation,      // as passed to us
                                      pSourceFolder,    // as passed to us
                                      pTargetFolder,    // as passed to us
                                      fopsGenericProgressCallback, // progress callback
                                      fopsGenericErrorCallback, // error callback
                                      (ULONG)ppwd);     // ulUser
             // this locks pSourceFolder

        if ((hftl) && (pObject))
        {
            // now add all objects to the task list
            while (pObject)
            {
                FOPSRET     frc2;
                WPObject    *pAddObject = pObject;
                #ifdef DEBUG_TRASHCAN
                    _Pmpf(("fopsStartTaskFromCnr: got object %s", _wpQueryTitle(pObject)));
                #endif

                if (fRelatedObjects)
                    // collect related objects instead:
                    // then this better be a trash object
                    pAddObject = _xwpQueryRelatedObject(pObject);

                frc2 = fopsAddObjectToTask(hftl, pAddObject);
                if (frc2 == FOPSERR_OK)
                {
                    // raise objects count
                    cObjects++;

                    if (ulSelection == SEL_MULTISEL)
                        // more objects to go:
                        pObject = wpshQueryNextSourceObject(hwndCnr, pObject);
                    else
                        break;
                }
                else
                {
                    // error:
                    frc = frc2;
                    break;
                }
            }
        }
        else
            frc = FOPSERR_INVALID_OBJECT;

        if (frc == FOPSERR_OK)
        {
            // confirmations?

            _Pmpf(("ulMsgSingle: %d", ulMsgSingle));

            if (   (ulMsgSingle != 0)
                && (ulMsgMultiple != 0)
               )
            {
                CHAR    szObjectCount[30];
                PSZ     apsz = NULL;
                ULONG   ulMsg;
                // yes:
                _Pmpf(("Confirming..."));
                if (cObjects == 1)
                {
                    // single object:
                    apsz = _wpQueryTitle(pSourceObject);
                    ulMsg = ulMsgSingle;
                }
                else
                {
                    sprintf(szObjectCount, "%d", cObjects);
                    apsz = szObjectCount;
                    ulMsg = ulMsgMultiple;
                }

                if (cmnMessageBoxMsgExt(NULLHANDLE,  // owner
                                        121, // "XWorkplace"
                                        &apsz,
                                        1,
                                        ulMsg,
                                        MB_YESNO)
                            != MBID_YES)
                    frc = FOPSERR_CANCELLEDBYUSER;
            }

            if (frc == FOPSERR_OK)
                // *** go!!!
                if (!fopsStartWithGenericProgress(hftl,
                                                  ppwd))
                    // error:
                    frc = FOPSERR_INTEGRITY_ABORT;
        } // end if (frc == FOPSERR_OK)

        if (frc != FOPSERR_OK)
        {
            // cancel or no success: clean up
            fopsDeleteFileTaskList(hftl);
            free(ppwd);     // V0.9.3 (2000-04-11) [umoeller]
        }
    }

    #ifdef DEBUG_TRASHCAN
        _Pmpf(("    returning FOPSRET %d", frc));
    #endif

    return (frc);
}

/*
 *@@ fopsStartTaskFromList:
 *      similar to fopsStartTaskFromCnr, but this
 *      does not check for selected objects,
 *      but adds the objects from the specified
 *      list.
 *
 *      Entry point to the top layer of the XWorkplace file
 *      operations engine.
 *
 *      pllObjects must be a list containing
 *      simple WPObject* pointers. The list is
 *      not freed or modified by this function,
 *      but read only.
 *
 *@@added V0.9.1 (2000-02-01) [umoeller]
 *@@changed V0.9.3 (2000-04-25) [umoeller]: reworked error management
 */

FOPSRET fopsStartTaskFromList(ULONG ulOperation,
                              WPFolder *pSourceFolder,
                              WPFolder *pTargetFolder,
                              PLINKLIST pllObjects)      // in: list with WPObject* pointers
{
    FOPSRET frc = FOPSERR_OK;

    // allocate progress window data structure
    // this is passed to fopsCreateFileTaskList as ulUser
    PGENERICPROGRESSWINDATA ppwd = malloc(sizeof(GENERICPROGRESSWINDATA));

    #ifdef DEBUG_TRASHCAN
        _Pmpf(("fopsStartTaskFromList, op: %d, source: %s, target: %s",
                ulOperation,
                (pSourceFolder) ? _wpQueryTitle(pSourceFolder) : "NULL",
                (pTargetFolder) ? _wpQueryTitle(pTargetFolder) : "NULL"
                ));
    #endif

    if (ppwd)
    {
        HFILETASKLIST hftl = NULLHANDLE;
        PLISTNODE   pNode = NULL;

        // create task list for the desired task
        hftl = fopsCreateFileTaskList(ulOperation,      // as passed to us
                                      pSourceFolder,    // as passed to us
                                      pTargetFolder,    // as passed to us
                                      fopsGenericProgressCallback, // progress callback
                                      fopsGenericErrorCallback, // error callback
                                      (ULONG)ppwd);     // ulUser
             // this locks pSourceFolder

        if (hftl)
        {
            // add ALL objects from the list
            pNode = lstQueryFirstNode(pllObjects);
            while (pNode)
            {
                WPObject *pObject = (WPObject*)pNode->pItemData;
                FOPSRET frc2;

                #ifdef DEBUG_TRASHCAN
                    _Pmpf(("fopsStartTaskFromList: got object %s", _wpQueryTitle(pObject) ));
                #endif

                frc2 = fopsAddObjectToTask(hftl, pObject);
                #ifdef DEBUG_TRASHCAN
                    _Pmpf(("        got FOPSRET %d for that", frc2));
                #endif
                if (frc2 != FOPSERR_OK)
                {
                    frc = frc2;
                    break;
                }
                pNode = pNode->pNext;
            }
        }
        else
            frc = FOPSERR_INTEGRITY_ABORT;

        if (frc == FOPSERR_OK)
            // *** go!!!
            if (!fopsStartWithGenericProgress(hftl,
                                              ppwd))
                // error:
                frc = FOPSERR_INTEGRITY_ABORT;

        if (frc != FOPSERR_OK)
        {
            // cancel or no success: clean up
            fopsDeleteFileTaskList(hftl);
            free(ppwd);     // V0.9.3 (2000-04-11) [umoeller]
        }
    }

    #ifdef DEBUG_TRASHCAN
        _Pmpf(("    returning FOPSRET %d", frc));
    #endif

    return (frc);
}

/********************************************************************
 *                                                                  *
 *   Trash can file operations implementation                       *
 *                                                                  *
 ********************************************************************/

/*
 *@@ fopsStartDeleteFromCnr:
 *      this gets called from fdr_fnwpSubclassedFolderFrame
 *      if trash can "delete" support is on and a "delete"
 *      command was intercepted for a number of objects.
 *
 *      Entry point to the top layer of the XWorkplace file
 *      operations engine.
 *
 *      This calls fopsStartTaskFromCnr with the proper parameters
 *      to start moving objects which are selected in hwndCnr
 *      to the default trash can on the File thread.
 *
 *      Note: The "source folder" for fopsStartTaskFromCnr is
 *      automatically determined as the folder in which pSourceObject
 *      resides. This is because if an object somewhere deep in a
 *      Tree view hierarchy gets deleted, the progress window would
 *      otherwise display the main folder being deleted, which
 *      might panic the user. The source folder for delete operations
 *      is only for display anyways; the file operations engine
 *      doesn't really need it.
 *
 *@@added V0.9.1 (2000-01-29) [umoeller]
 *@@changed V0.9.3 (2000-04-25) [umoeller]: reworked error management
 *@@changed V0.9.3 (2000-04-30) [umoeller]: removed pSourceFolder parameter
 */

FOPSRET fopsStartDeleteFromCnr(WPObject *pSourceObject, // in: first object with source emphasis
                               ULONG ulSelection,       // in: SEL_* flag
                               HWND hwndCnr,            // in: container to collect objects from
                               BOOL fTrueDelete)        // in: if TRUE, perform true delete; if FALSE, move to trash can
{
    FOPSRET frc;

    ULONG       ulOperation = XFT_MOVE2TRASHCAN,
                ulMsgSingle = 0,
                ulMsgMultiple = 0;
    ULONG       ulConfirmations = _wpQueryConfirmations(pSourceObject);
    BOOL        fConfirm = FALSE;
    WPFolder    *pParentFolder;

    #ifdef DEBUG_TRASHCAN
        _Pmpf(("fopsStartDeleteFromCnr: first obj is %s", _wpQueryTitle(pSourceObject)));
        _Pmpf(("ulConfirmations: 0x%lX", ulConfirmations));
    #endif

    if (ulConfirmations & CONFIRM_DELETE)
        fConfirm = TRUE;

    if (fTrueDelete)
    {
        ulOperation = XFT_TRUEDELETE;
        if (fConfirm)
        {
            ulMsgSingle = 177;
            ulMsgMultiple = 178;
        }
    }
    else
        // move to trash can:
        if (fConfirm)
        {
            ulMsgSingle = 182;
            ulMsgMultiple = 183;
        }

    if (!pSourceObject)
        frc = FOPSERR_INVALID_OBJECT;
    else
    {
        pParentFolder = _wpQueryFolder(pSourceObject);
        if (!pParentFolder)
            frc = FOPSERR_INVALID_OBJECT;
        else
        {
            frc = fopsStartTaskFromCnr(ulOperation,
                                       pParentFolder, // pSourceFolder,
                                       NULL,         // target folder: not needed
                                       pSourceObject,
                                       ulSelection,
                                       FALSE,       // no related objects
                                       hwndCnr,
                                       ulMsgSingle,
                                       ulMsgMultiple);

            if (    (!fTrueDelete)      // delete into trashcan
                 && (frc == FOPSERR_TRASHDRIVENOTSUPPORTED) // and trash drive not supported
               )
            {
                // source folder is not supported by trash can:
                // start a "delete" job instead, with the proper
                // confirmation messages ("Drive not supported, delete instead?")
                frc = fopsStartTaskFromCnr(XFT_TRUEDELETE,
                                           pParentFolder, // pSourceFolder,
                                           NULL,         // target folder: not needed
                                           pSourceObject,
                                           ulSelection,
                                           FALSE,       // no related objects
                                           hwndCnr,
                                           180, // ulMsgSingle
                                           181); // ulMsgMultiple
            }
        }
    }

    return (frc);
}

/*
 *@@ fopsStartTrashRestoreFromCnr:
 *      this gets called from trsh_fnwpSubclassedTrashCanFrame
 *      if WM_COMMAND with ID_XFMI_OFS_TRASHRESTORE has been intercepted.
 *
 *      Entry point to the top layer of the XWorkplace file
 *      operations engine.
 *
 *      This calls fopsStartTaskFromCnr with the proper parameters
 *      to start restoring the selected trash objects on the File thread.
 *
 *@@added V0.9.1 (2000-01-31) [umoeller]
 */

FOPSRET fopsStartTrashRestoreFromCnr(WPFolder *pTrashSource,  // in: XWPTrashCan* to restore from
                                     WPFolder *pTargetFolder, // in: specific target folder or NULL for
                                                              // each trash object's original folder
                                     WPObject *pSourceObject, // in: first object with source emphasis
                                     ULONG ulSelection,       // in: SEL_* flag
                                     HWND hwndCnr)            // in: container to collect objects from
{
    return (fopsStartTaskFromCnr(XFT_RESTOREFROMTRASHCAN,
                                 pTrashSource,
                                 pTargetFolder, // can be NULL
                                 pSourceObject,
                                 ulSelection,
                                 FALSE,       // no related objects
                                 hwndCnr,
                                 0, 0)        // no confirmations
            );
}

/*
 *@@ fopsStartTrashDestroyFromCnr:
 *      this gets called from trsh_fnwpSubclassedTrashCanFrame
 *      if WM_COMMAND with ID_XFMI_OFS_TRASHDESTROY has been intercepted.
 *
 *      Entry point to the top layer of the XWorkplace file
 *      operations engine.
 *
 *      This calls fopsStartTaskFromCnr with the proper parameters
 *      to start destroying the selected trash objects on the File thread.
 *
 *@@added V0.9.1 (2000-01-31) [umoeller]
 *@@changed V0.9.3 (2000-04-28) [umoeller]: switched implementation to "true delete"
 */

FOPSRET fopsStartTrashDestroyFromCnr(WPFolder *pTrashSource,  // in: XWPTrashCan* to restore from
                                     WPObject *pSourceObject, // in: first object with source emphasis
                                     ULONG ulSelection,       // in: SEL_* flag
                                     HWND hwndCnr)            // in: container to collect objects from
{
    return (fopsStartTaskFromCnr(XFT_TRUEDELETE,
                                 pTrashSource,
                                 NULL,             // no target folder
                                 pSourceObject,
                                 ulSelection,
                                 TRUE,       // collect related objects instead
                                 hwndCnr,
                                 177,
                                 178)
            );
}


