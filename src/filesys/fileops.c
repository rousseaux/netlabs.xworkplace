
/*
 *@@sourcefile fileops.c:
 *      file operations code. This gets called from various
 *      method overrides to implement certain file-operations
 *      features.
 *
 *      Current features:
 *
 *      -- Replacement "File exists" dialog implementation
 *         (fopsConfirmObjectTitle).
 *
 *      -- Generic file-operations framework to process
 *         files (actually, any objects) on the XWorkplace
 *         File thread. See fopsCreateFileTaskList and
 *         fopsPrepareCommon for details.
 *
 *         The actual file operation calls are then made on the
 *         XWorkplace File thread (xthreads.c) which calls
 *         fopsFileThreadProcessing in this file.
 *
 *         Concrete implementations currently are
 *         fopsStartCnrDelete2Trash, fopsStartCnrRestoreFromTrash,
 *         and fopsPrepareDestroyTrashObjects for the XWorkplace
 *         trash can (XWPTrashCan class).
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

// SOM headers which don't crash with prec. header files
#include "xfobj.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\xsetup.h"              // XWPSetup implementation

#include "filesys\fileops.h"            // file operations implementation
#include "filesys\xthreads.h"           // extra XWorkplace threads

// other SOM headers
#pragma hdrstop                         // VAC++ keeps crashing otherwise
#include <wpfolder.h>
#include "xtrash.h"
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

        if (cmnQueryLock())
            DosBeep(100, 1000);
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
 *      Gets called by fopsExpandObject if that
 *      function has been invoked on a folder.
 *      This calls fopsExpandObject in turn for
 *      each object found, so this may be called
 *      recursively.
 *
 *      See fopsExpandObject for an introduction
 *      what these things are good for.
 *
 *@@added V0.9.2 (2000-02-28) [umoeller]
 */

PLINKLIST fopsFolder2ExpandedList(WPFolder *pFolder,
                                  PULONG pulSizeContents) // out: size of all objects on list
{
    PLINKLIST   pll = lstCreate(FALSE);       // do not free the items
    ULONG       ulSizeContents = 0;

    BOOL fFolderLocked = FALSE;

    _Pmpf(("Object \"%s\" is a folder, creating SFL", _wpQueryTitle(pFolder) ));

    // lock folder for querying content
    fFolderLocked = !_wpRequestObjectMutexSem(pFolder, 5000);
    if (fFolderLocked)
    {
        WPObject *pObject;

        wpshCheckIfPopulated(pFolder);

        // now collect all objects in folder
        for (pObject = _wpQueryContent(pFolder, NULL, QC_FIRST);
             pObject;
             pObject = _wpQueryContent(pFolder, pObject, QC_Next))
        {
            PEXPANDEDOBJECT fSOI = NULL;

            _Pmpf(("creating SOI for \"%s\" in folder \"%d\"",
                    _wpQueryTitle(pObject),
                    _wpQueryTitle(pFolder) ));
            // create a list item for this object;
            // if pObject is a folder, that function will
            // call ourselves again...
            fSOI = fopsExpandObject(pObject);
            ulSizeContents += fSOI->ulSizeThis;
            lstAppendItem(pll, fSOI);
        }
    }

    if (fFolderLocked)
    {
        _wpReleaseObjectMutexSem(pFolder);
        fFolderLocked = FALSE;
    }

    *pulSizeContents = ulSizeContents;

    return (pll);
}

/*
 *@@ fopsExpandObject:
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
 *      subobjects if that object represents a folder.
 *      After calling this function,
 *      EXPANDEDOBJECT.ulSizeThis has the total size
 *      of pObject and all objects which reside in
 *      subfolders (if applicable).
 *
 *@@added V0.9.2 (2000-02-28) [umoeller]
 */

PEXPANDEDOBJECT fopsExpandObject(WPObject *pObject)
{
    // create object item
    PEXPANDEDOBJECT pSOI = (PEXPANDEDOBJECT)malloc(sizeof(EXPANDEDOBJECT));
    if (pSOI)
    {
        _Pmpf(("SOI for object %s", _wpQueryTitle(pObject) ));
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
        _Pmpf(("End of SOI for object %s", _wpQueryTitle(pObject) ));
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
 *      created by fopsExpandObject. This calls
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
                fFileExists = FALSE;
                for (   pExistingFile2 = _wpQueryContent(pFolder, NULL, (ULONG)QC_FIRST);
                        (pExistingFile2);
                        pExistingFile2 = _wpQueryContent(pFolder, pExistingFile2, (ULONG)QC_NEXT)
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
                strhxrpl(&pszTemp, 0, "%1", szRealNameFound, 0);
                strhxrpl(&pszTemp, 0, "%2", szFolder, 0);
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
                ulDlgReturn = WinProcessDlg(hwndConfirm);

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
 */

BOOL fopsMoveObjectConfirmed(WPObject *pObject,
                             WPFolder *pTargetFolder)
{
    // check if object exists in that folder already
    // (this might call the XFldObject replacement)
    WPObject    *pReplaceThis = NULL;
    CHAR        szNewTitle[CCHMAXPATH] = "";
    BOOL        fMove = TRUE;
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
            fMove = FALSE;
        break;

        case NAMECLASH_RENAME:
            _wpSetTitle(pObject,
                        szNewTitle);      // set by wpConfirmObjectTitle
        break;

        case NAMECLASH_REPLACE:
            _wpReplaceObject(pObject,
                             pReplaceThis,       // set by wpConfirmObjectTitle
                             TRUE);              // move and replace
            fMove = FALSE;
        break;

        // NAMECLASH_NONE: just go on
    }

    if (fMove)
        fMove = _wpMoveObject(pObject, pTargetFolder);

    return (fMove);
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
 *@@changed V0.9.2 (2000-03-04) [umoeller]: added callback
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
 *
 *      WARNING: This requests the object mutex semaphores
 *      for pSourceFolder. You must add objects to the returned
 *      task list (using fopsAddFileTask) as quickly as possible.
 *
 *      The mutex will only be released after all
 *      file processing has been completed (thru
 *      fopsStartProcessingTasks) or if you call
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
 *          In that case, pSourceFolder has the source folder;
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
 *      -- XFT_DELETE: delete list items without moving them
 *          into the trash can first.
 *          In that case, pTargetFolder is ignored.
 *
 *@@added V0.9.1 (2000-01-27) [umoeller]
 *@@changed V0.9.2 (2000-03-04) [umoeller]: added callback
 */

HFILETASKLIST fopsCreateFileTaskList(ULONG ulOperation,
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
    }

    // error
    if (fSourceLocked)
        _wpReleaseObjectMutexSem(pSourceFolder);

    return (NULLHANDLE);
}

/*
 *@@ fopsValidateObjOperation:
 *      returns TRUE only if ulOperation is valid
 *      on pObject. If an error has been found and
 *      (pfnErrorCallback != NULL), that callback
 *      gets called and its return value is returned.
 *      If the callback is NULL, FALSE is returned
 *      always.
 *
 *      This is usually called on the thread of the folder view
 *      from which the file operation was started (mostly
 *      thread 1).
 *
 *@@added V0.9.1 (2000-02-01) [umoeller]
 *@@changed V0.9.2 (2000-03-04) [umoeller]: added callback
 */

BOOL fopsValidateObjOperation(ULONG ulOperation,        // in: operation
                              FNFOPSERRORCALLBACK *pfnErrorCallback, // in: error callback or NULL
                              WPObject *pObject)        // in: current object
{
    BOOL    brc = TRUE;
    ULONG   ulError = 0;

    // error checking
    switch (ulOperation)
    {
        case XFT_MOVE2TRASHCAN:
            // prohibit "move to trash" for trash objects themselves
            if (_somIsA(pObject, _XWPTrashObject))
                ulError = FOPSERR_NODELETETRASHOBJECT;
            // prohibit "move to trash" for no-delete objects
            else if (_wpQueryStyle(pObject) & OBJSTYLE_NODELETE)
                ulError = FOPSERR_OBJSTYLENODELETE;
            else if (!trshIsOnSupportedDrive(pObject))
                ulError = FOPSERR_TRASHDRIVENOTSUPPORTED;
        break;

        case XFT_DESTROYTRASHOBJECTS:
            // allow "destroy trash object" only for trash objects
            if (!_somIsA(pObject, _XWPTrashObject))
                ulError = FOPSERR_DESTROYNONTRASHOBJECT;
        break;
    }

    if (ulError)
        if (pfnErrorCallback)
            brc = (pfnErrorCallback)(pObject, ulError);
        else
            brc = FALSE;

    return (brc);
}

/*
 *@@ fopsAddFileTask:
 *      adds an object to be processed to the given
 *      file task list.
 *
 *      Note: This function is reentrant only for the
 *      same HFILETASKLIST. Once fopsStartProcessingTasks
 *      has been called for the task list, you must not
 *      modify the task list again.
 *      There's no automatic protection against this!
 *      So call this function only AFTER fopsCreateFileTaskList
 *      and BEFORE fopsStartProcessingTasks.
 *
 *      This calls fopsValidateObjOperation on the object.
 *
 *      This returns FALSE if an error occured. Error
 *      conditions can be:
 *
 *      -- Internal list processing error.
 *      -- Invalid operation for pObject (fopsValidateObjOperation
 *         returned FALSE).
 *
 *@@added V0.9.1 (2000-01-26) [umoeller]
 *@@changed V0.9.2 (2000-03-04) [umoeller]: added callback
 */

BOOL fopsAddObjectToTask(HFILETASKLIST hftl,      // in: file-task-list handle
                         WPObject *pObject)
{
    BOOL brc = TRUE;
    PFILETASKLIST pftl = (PFILETASKLIST)hftl;

    brc = fopsValidateObjOperation(pftl->ulOperation,
                                   pftl->pfnErrorCallback,
                                   pObject);

    if (brc)
        // proceed:
        brc = (lstAppendItem(&pftl->llObjects, pObject) != NULL);

    return (brc);
}

/*
 *@@ fopsStartProcessingTasks:
 *      this starts processing the tasks which have
 *      been previously added to the file tasks lists
 *      using fopsAddFileTask.
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
 */

BOOL fopsStartProcessingTasks(HFILETASKLIST hftl)
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

/*
 *@@ fopsFileThreadProcessing:
 *      this actually performs the file operations
 *      on the objects which have been added to
 *      the given file task list.
 *
 *      WARNING: NEVER call this function manually.
 *      This gets called on the File thread (xthreads.c)
 *      automatically  after fopsStartProcessingTasks has
 *      been called.
 *
 *      This runs on the File thread.
 *
 *@@added V0.9.1 (2000-01-29) [umoeller]
 */

VOID fopsFileThreadProcessing(HFILETASKLIST hftl)
{
    PFILETASKLIST pftl = (PFILETASKLIST)hftl;

    #ifdef DEBUG_TRASHCAN
        _Pmpf(("fopsFileThreadProcessing hftl: 0x%lX", hftl));
    #endif

    TRY_LOUD(excpt2, NULL)
    {
        if (pftl)
        {
            // get first node
            PLISTNODE   pNode = lstQueryFirstNode(&pftl->llObjects);
            FOPSUPDATE  fu;

            memset(&fu, 0, sizeof(fu));

            if (pftl->ulOperation == XFT_MOVE2TRASHCAN)
            {
                // move to trash can:
                // in that case, pTargetFolder is NULL,
                // so query the target folder first
                pftl->pTargetFolder = _xwpclsQueryDefaultTrashCan(_XWPTrashCan);
            }

            fu.ulOperation = pftl->ulOperation;
            fu.pSourceFolder = pftl->pSourceFolder;
            fu.pTargetFolder = pftl->pTargetFolder;
            fu.ulCurrentObject = 0;
            fu.ulTotalObjects = lstCountItems(&pftl->llObjects);

            while (pNode)
            {
                WPObject *pObjectThis = (WPObject*)pNode->pItemData;

                if (wpshCheckObject(pObjectThis))
                {
                    // call progress callback with this object
                    if (pftl->pfnProgressCallback)
                    {
                        fu.pCurrentObject = pObjectThis;
                        if ((pftl->pfnProgressCallback)(&fu,
                                                        pftl->ulUser)
                                == FALSE)
                            // FALSE means abort:
                            break;
                    }
                }

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
                        _xwpDeleteIntoTrashCan(pftl->pTargetFolder, // default trash can
                                               pObjectThis);
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
                        _xwpRestoreFromTrashCan(pObjectThis,       // trash object
                                                pftl->pTargetFolder); // can be NULL
                    break;

                    /*
                     * XFT_DESTROYTRASHOBJECTS:
                     *
                     */

                    case XFT_DESTROYTRASHOBJECTS:
                        #ifdef DEBUG_TRASHCAN
                            _Pmpf(("  fopsFileThreadProcessing: destroying %s",
                                    _wpQueryTitle(pObjectThis) ));
                        #endif
                        _xwpDestroyTrashObject(pObjectThis); // trash object
                    break;
                }

                pNode = pNode->pNext;
                fu.ulCurrentObject++;
            } // end while (pNode)

            // call progress callback to say  "done"
            if (pftl->pfnProgressCallback)
                (pftl->pfnProgressCallback)(NULL,           // NULL means done
                                            pftl->ulUser);
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
 *      called fopsStartProcessingTasks because
 *      then the File thread is working on the
 *      list already. The file list will automatically
 *      get cleaned up then, so you only need to
 *      call this function yourself if you choose
 *      NOT to call fopsStartProcessingTasks.
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
    BOOL        fCancelPressed;             // TRUE after Cancel has been
                                            // pressed; the File thread is then
                                            // halted while a confirmation is
                                            // displayed
} GENERICPROGRESSWINDATA, *PGENERICPROGRESSWINDATA;

/*
 *@@ fopsGenericProgressCallback:
 *      progress callback which gets specified when
 *      fopsPrepareCommon calls fopsCreateFileTaskList.
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
 *      fopsPrepareCommon calls fopsCreateFileTaskList.
 *
 *      This is usually called on the thread of the folder view
 *      from which the file operation was started (mostly
 *      thread 1).
 *
 *@@added V0.9.2 (2000-03-04) [umoeller]
 */

BOOL APIENTRY fopsGenericErrorCallback(WPObject *pObject,
                                       ULONG ulError)
{
    BOOL    brc = TRUE;
    CHAR    szMsg[1000];
    PSZ     apsz[5] = {0};
    ULONG   cpsz = 0,
            ulMsg = 0,
            flFlags = 0;

    switch (ulError)
    {
        case FOPSERR_NODELETETRASHOBJECT:
        case FOPSERR_DESTROYNONTRASHOBJECT:
            ulMsg = 174;        // "invalid for XWPTrashObject"
            flFlags = MB_CANCEL;
        break;

        case FOPSERR_OBJSTYLENODELETE:
            return (FALSE);

        case FOPSERR_TRASHDRIVENOTSUPPORTED:
            ulMsg = 176;
            flFlags = MB_CANCEL;
        break;
    }

    if (flFlags)
    {
        ULONG ulrc = cmnMessageBoxMsgExt(NULLHANDLE,
                                         175,
                                         apsz,
                                         cpsz,
                                         ulMsg,
                                         flFlags);
        if ((ulrc == MBID_OK) || (ulrc == MBID_YES))
            return (TRUE);
    }

    return (FALSE);
}

/*
 *@@ fops_fnwpGenericProgress:
 *      dialog procedure for the standard XWorkplace
 *      file operations status window. This updates
 *      the window's status bar and the informational
 *      static texts for the objects and folders.
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
            {
                // update target folder
                if (pfu->ulOperation == XFT_RESTOREFROMTRASHCAN)
                    // restore from trash can: update for every object
                    WinSetDlgItemText(hwndProgress,
                                      ID_XSDI_TARGETFOLDER,
                                      _xwpQueryRelatedPath(pfu->pCurrentObject));

                // other operations: only on first call
                if (pfu->ulCurrentObject == 0)
                {
                    CHAR szPath[CCHMAXPATH] = "";

                    // first call: set folders
                    if (pfu->pSourceFolder)
                        _wpQueryFilename(pfu->pSourceFolder, szPath, TRUE);

                    WinSetDlgItemText(hwndProgress,
                                      ID_XSDI_SOURCEFOLDER,
                                      szPath);

                    if (pfu->ulOperation != XFT_RESTOREFROMTRASHCAN)
                    {
                        szPath[0] = 0;
                        if (pfu->pTargetFolder)
                            _wpQueryFilename(pfu->pTargetFolder, szPath, TRUE);

                        WinSetDlgItemText(hwndProgress,
                                          ID_XSDI_TARGETFOLDER,
                                          szPath);
                    }
                }

                // update current object
                WinSetDlgItemText(hwndProgress,
                                  ID_XSDI_CURRENTOBJECT,
                                  _wpQueryTitle(pfu->pCurrentObject));
                WinSendMsg(WinWindowFromID(hwndProgress, ID_SDDI_PROGRESSBAR),
                           WM_UPDATEPROGRESSBAR,
                           (MPARAM)pfu->ulCurrentObject,
                           (MPARAM)pfu->ulTotalObjects);

                if (ppwd->fCancelPressed)
                {
                    PSZ pszTitle = winhQueryWindowText(hwndProgress);
                    // this msg box halts the File thread because
                    // we're being SENT this message
                    if (cmnMessageBox(hwndProgress, // owner
                                      pszTitle,
                                      "Do you wish to cancel the operation in progress?",
                                            // ###
                                      MB_YESNO)
                            == MBID_YES)
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
            }
            else
            {
                // done:
                // disable "Cancel"
                WinEnableControl(hwndProgress, DID_CANCEL, FALSE);
                if (!ppwd->fCancelPressed)
                    // set progress to 100%
                    WinSendMsg(WinWindowFromID(hwndProgress, ID_SDDI_PROGRESSBAR),
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
 *
 *      Gets called by fopsPrepareCommon.
 *
 *@@added V0.9.1 (2000-02-01) [umoeller]
 */

BOOL fopsStartWithGenericProgress(HFILETASKLIST hftl,
                                  PGENERICPROGRESSWINDATA ppwd,
                                  PSZ pszTitle)
{
    // load progress dialog
    ppwd->hwndProgress = WinLoadDlg(HWND_DESKTOP,        // parent
                                    NULLHANDLE, // owner
                                    fops_fnwpGenericProgress,
                                    cmnQueryNLSModuleHandle(FALSE),
                                    ID_XFD_FILEOPSSTATUS,
                                    NULL);

    ppwd->fCancelPressed = FALSE;
    WinSetWindowPtr(ppwd->hwndProgress, QWL_USER, ppwd);

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

    return (fopsStartProcessingTasks(hftl));
}

/*
 *@@ fopsPrepareCommon:
 *      this sets up the generic file operations progress
 *      dialog (fops_fnwpGenericProgress), creates a file
 *      task list for the given job (fopsCreateFileTaskList),
 *      adds objects to that list (fopsAddObjectToTask), and
 *      starts processing (fopsStartProcessingTasks), all in
 *      one shot.
 *
 *      The actual file operation calls are then made on the
 *      XWorkplace File thread (xthreads.c), which calls
 *      fopsFileThreadProcessing in this file.
 *
 *      pSourceObject must have the first object which is to
 *      be deleted, that is, the object for which the context
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
 *@@added V0.9.1 (2000-01-31) [umoeller]
 */

BOOL fopsStartCnrCommon(ULONG ulOperation,       // in: operation; see fopsCreateFileTaskList
                        PSZ pszTitle,            // in: title for status dialog
                        WPFolder *pSourceFolder, // in: source folder, as required by fopsCreateFileTaskList
                        WPFolder *pTargetFolder, // in: target folder, as required by fopsCreateFileTaskList
                        WPObject *pSourceObject, // in: first object with source emphasis
                        ULONG ulSelection,       // in: SEL_* flags
                        HWND hwndCnr)            // in: container to get more source objects from
{
    BOOL    brc = FALSE;

    WPObject *pObject = pSourceObject;

    // allocate progress window data structure
    // this is passed to fopsCreateFileTaskList as ulUser
    PGENERICPROGRESSWINDATA ppwd = malloc(sizeof(GENERICPROGRESSWINDATA));

    #ifdef DEBUG_TRASHCAN
        _Pmpf(("fopsStartCnrCommon ulSelection: %d", ulSelection));
    #endif

    if (ppwd)
    {
        HFILETASKLIST hftl = NULLHANDLE;
        BOOL        fContinue = TRUE;

        // create task list for the desired task
        hftl = fopsCreateFileTaskList(ulOperation,      // as passed to us
                                      pSourceFolder,    // as passed to us
                                      pTargetFolder,    // as passed to us
                                      fopsGenericProgressCallback, // progress callback
                                      fopsGenericErrorCallback, // error callback
                                      (ULONG)ppwd);     // ulUser
             // this locks pSourceFolder

        if (pObject)
        {
            // now add all objects to the task list
            while (pObject)
            {
                #ifdef DEBUG_TRASHCAN
                    _Pmpf(("fopsStartCnrCommon: got object %s", pObject));
                #endif
                if (fopsAddObjectToTask(hftl, pObject))
                {
                    if (ulSelection == SEL_MULTISEL)
                        // more objects to go:
                        pObject = wpshQueryNextSourceObject(hwndCnr, pObject);
                    else
                        break;
                }
                else
                {
                    // error:
                    fContinue = FALSE;
                    break;
                }
            }
        }
        else
            fContinue = FALSE;

        if (fContinue)
            // *** go!!!
            brc = fopsStartWithGenericProgress(hftl,
                                               ppwd,
                                               pszTitle);
        else
        {
            fopsDeleteFileTaskList(hftl);
            brc = FALSE;
        }
    }

    return (brc);
}

/*
 *@@ fopsStartFdrContentsCommon:
 *      similar to fopsStartCnrCommon, but this
 *      does not check for selected objects,
 *      but adds the objects from the specified
 *      list.
 *
 *      pllObjects must be a list containing
 *      simple WPObject* pointers. The list is
 *      not freed or modified by this function,
 *      but read only.
 *
 *@@added V0.9.1 (2000-02-01) [umoeller]
 */

BOOL fopsStartFromListCommon(ULONG ulOperation,
                             PSZ pszTitle,
                             WPFolder *pSourceFolder,
                             WPFolder *pTargetFolder,
                             PLINKLIST pllObjects)      // in: list with WPObject* pointers
{
    BOOL    brc = FALSE;

    // allocate progress window data structure
    // this is passed to fopsCreateFileTaskList as ulUser
    PGENERICPROGRESSWINDATA ppwd = malloc(sizeof(GENERICPROGRESSWINDATA));

    if (ppwd)
    {
        HFILETASKLIST hftl = NULLHANDLE;
        BOOL        fContinue = TRUE;
        PLISTNODE   pNode = NULL;

        // create task list for the desired task
        hftl = fopsCreateFileTaskList(ulOperation,      // as passed to us
                                      pSourceFolder,    // as passed to us
                                      pTargetFolder,    // as passed to us
                                      fopsGenericProgressCallback, // progress callback
                                      fopsGenericErrorCallback, // error callback
                                      (ULONG)ppwd);     // ulUser
             // this locks pSourceFolder

        // add ALL objects from the list
        pNode = lstQueryFirstNode(pllObjects);
        while (pNode)
        {
            WPObject *pObject = (WPObject*)pNode->pItemData;

            #ifdef DEBUG_TRASHCAN
                _Pmpf(("fopsStartFromListCommon: got object %s", _wpQueryTitle(pObject) ));
            #endif
            if (!fopsAddObjectToTask(hftl, pObject))
            {
                fContinue = FALSE;
                break;
            }
            pNode = pNode->pNext;
        }

        if (fContinue)
            // *** go!!!
            brc = fopsStartWithGenericProgress(hftl,
                                               ppwd,
                                               pszTitle);
        else
        {
            fopsDeleteFileTaskList(hftl);
            brc = FALSE;
        }
    }

    return (brc);
}

/********************************************************************
 *                                                                  *
 *   Trash can file operations implementation                       *
 *                                                                  *
 ********************************************************************/

/*
 *@@ fopsStartCnrDelete2Trash:
 *      this gets called from fdr_fnwpSubclassedFolderFrame
 *      if trash can "delete" support is on and WM_COMMAND
 *      with WPMENUID_DELETE has been intercepted.
 *
 *      This calls fopsStartCnrCommon with the proper parameters
 *      to start moving objects which are selected in pSourceFolder
 *      to the default trash can on the File thread.
 *
 *@@added V0.9.1 (2000-01-29) [umoeller]
 */

BOOL fopsStartCnrDelete2Trash(WPFolder *pSourceFolder, // in: folder with objects to be moved
                              WPObject *pSourceObject, // in: first object with source emphasis
                              ULONG ulSelection,       // in: SEL_* flag
                              HWND hwndCnr)            // in: container
{
    return (fopsStartCnrCommon(XFT_MOVE2TRASHCAN,
                               "Moving to Trash", // ###
                               pSourceFolder,
                               NULL,         // target folder: not needed
                               pSourceObject,
                               ulSelection,
                               hwndCnr)
                != NULLHANDLE);
}

/*
 *@@ fopsStartCnrRestoreFromTrash:
 *      this gets called from trsh_fnwpSubclassedTrashCanFrame
 *      if WM_COMMAND with ID_XFMI_OFS_TRASHRESTORE has been intercepted.
 *
 *      This calls fopsStartCnrCommon with the proper parameters
 *      to start restoring the selected trash objects on the File thread.
 *
 *@@added V0.9.1 (2000-01-31) [umoeller]
 */

BOOL fopsStartCnrRestoreFromTrash(WPFolder *pTrashSource,  // in: XWPTrashCan* to restore from
                                  WPFolder *pTargetFolder, // in: specific target folder or NULL for
                                                           // each trash object's original folder
                                  WPObject *pSourceObject, // in: first object with source emphasis
                                  ULONG ulSelection,       // in: SEL_* flag
                                  HWND hwndCnr)            // in: container
{
    return (fopsStartCnrCommon(XFT_RESTOREFROMTRASHCAN,
                               "Restoring from Trash",
                               pTrashSource,
                               pTargetFolder, // can be NULL
                               pSourceObject,
                               ulSelection,
                               hwndCnr)
                != NULLHANDLE);
}

/*
 *@@ fopsStartCnrDestroyTrashObjects:
 *      this gets called from trsh_fnwpSubclassedTrashCanFrame
 *      if WM_COMMAND with ID_XFMI_OFS_TRASHDESTROY has been intercepted.
 *
 *      This calls fopsStartCnrCommon with the proper parameters
 *      to start destroying the selected trash objects on the File thread.
 *
 *@@added V0.9.1 (2000-01-31) [umoeller]
 */

BOOL fopsStartCnrDestroyTrashObjects(WPFolder *pTrashSource,  // in: XWPTrashCan* to restore from
                                     WPObject *pSourceObject, // in: first object with source emphasis
                                     ULONG ulSelection,       // in: SEL_* flag
                                     HWND hwndCnr)            // in: container
{
    return (fopsStartCnrCommon(XFT_DESTROYTRASHOBJECTS,
                               "Destroying Trash Objects",
                               pTrashSource,
                               NULL,             // no target folder
                               pSourceObject,
                               ulSelection,
                               hwndCnr)
                != NULLHANDLE);
}


