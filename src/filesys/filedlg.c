
/*
 *@@sourcefile filedlg.c:
 *      replacement file dlg with full WPS support.
 *
 *      The entry point is fdlgFileDialog, which has the
 *      same syntax as WinFileDlg and should return about
 *      the same values. 100% compatibility cannot be
 *      achieved because of window subclassing and all those
 *      things.
 *
 *      This file is ALL new with V0.9.9.
 *
 *      Function prefix for this file:
 *      --  fdlr*
 *
 *@@added V0.9.9 (2001-03-11) [umoeller]
 *@@header "filesys\filedlf.h"
 */

/*
 *      Copyright (C) 2001 Ulrich M”ller.
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
#define INCL_WINFRAMEMGR
#define INCL_WININPUT
#define INCL_WINTIMER
#define INCL_WINPOINTERS
#define INCL_WINDIALOGS
#define INCL_WINBUTTONS
#define INCL_WINSTATICS
#define INCL_WINENTRYFIELDS
#define INCL_WINSTDFILE
#define INCL_WINSTDCNR
#define INCL_WINSHELLDATA       // Prf* functions
#define INCL_WINSYS

#define INCL_GPILOGCOLORTABLE
#include <os2.h>

// C library headers
#include <stdio.h>              // needed for except.h
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h
#include <ctype.h>

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
#include "helpers\threads.h"            // thread helpers
#include "helpers\winh.h"               // PM helper routines
#include "helpers\xstring.h"            // extended string helpers

// SOM headers which don't crash with prec. header files
#include "xfldr.ih"
#include "xfdisk.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

#include "filesys\filedlg.h"            // replacement file dialog implementation
#include "filesys\folder.h"             // XFolder implementation

// other SOM headers
#pragma hdrstop                         // VAC++ keeps crashing otherwise
#include <wpshadow.h>

/* ******************************************************************
 *
 *   Declarations
 *
 ********************************************************************/

const char *WC_FILEDLGCLIENT    = "XWPFileDlgClient";

/*
 *@@ FILEDLGDATA:
 *      single structure allocated on the stack
 *      in fdlgFileDlg. The main frame and the
 *      main client receive pointers to this
 *      and store them in QWL_USER.
 *
 *      This pretty much holds all the data the
 *      file dlg needs while it is running.
 */

typedef struct _FILEDLGDATA
{
    // window hierarchy
    HWND        hwndMainFrame,
                hwndMainClient,     // child of hwndMainFrame
                hwndSplitWindow,    // child of hwndMainClient
                hwndDrivesFrame,    // child of hwndSplitWindow
                hwndDrivesCnr,      // child of hwndDrivesFrame
                hwndFilesFrame,     // child of hwndSplitWindow
                hwndFilesCnr;       // child of hwndFilesFrame

    // controls in hwndMainClient
                // buttons:
    HWND        hwndOK,
                hwndCancel,
                hwndHelp,
                // directory statics:
                hwndDirTxt,
                hwndDirValue,
                // file static/entryfield:
                hwndFileTxt,
                hwndFileEntry;

    // drives view (left)
    PSUBCLASSEDFOLDERVIEW psfvDrives;   // created in fdlgFileDlg only once

    WPFolder    *pDrivesFolder;         // root folder to populate, whose contents
                                        // appear in "drives" tree
    THREADINFO  tiAddChildren;
    BOOL        fAddChildrenRunning;
    LINKLIST    llDriveObjectsInserted; // linked list of plain WPObject* pointers
                                        // inserted, no auto-free
    LINKLIST    llDisks;                // linked list of all WPDisk* objects
                                        // so that we can find them later;
                                        // no auto-free
    PMINIRECORDCORE precSelectedInDrives;

    // files view (right)
    PSUBCLASSEDFOLDERVIEW psfvFiles;
    BOOL        fFilesFrameSubclassed;

    THREADINFO  tiInsertContents;
    BOOL        fInsertContentsRunning;
    LINKLIST    llFileObjectsInserted;

    // full file name etc. (_splitpath)
    ULONG       ulLogicalDrive;         // e.g. 3 for 'C'
    CHAR        szDir[CCHMAXPATH],      // e.g. "\whatever\"
                szFileMask[CCHMAXPATH],    // e.g. "*.txt"
                szFileName[CCHMAXPATH]; // e.g. "test.txt"

    BOOL        fFileDlgReady;
            // while this is FALSE, the dialog doesn't
            // react to changes in the containers

    // return code (either DID_OK or DID_CANCEL)
    ULONG       ulReturnCode;

} FILEDLGDATA, *PFILEDLGDATA;

/*
 *@@ INSERTTHREADSDATA:
 *      temporary structure allocated from the heap
 *      and passed to the various transient threads
 *      that do the hard work for us (populate, insert children).
 *      This structure is free()'d by the thread that
 *      was started.
 */

typedef struct _INSERTTHREADSDATA
{
    PFILEDLGDATA        pWinData;

    PLINKLIST           pll;         // list of items to work on; format depends on thread.
                                     // List is freed on exit.

} INSERTTHREADSDATA, *PINSERTTHREADSDATA;

/*
 *@@ INSERTOBJECTSARRAY:
 *
 */

typedef struct _INSERTOBJECTSARRAY
{
    WPFolder            *pFolder;
    HWND                hwndCnr;
    WPObject            **papObjects;
    ULONG               cObjects;
    PMINIRECORDCORE     precParent;

    PLINKLIST           pllObjects;
    PLINKLIST           pllRecords;
} INSERTOBJECTSARRAY, *PINSERTOBJECTSARRAY;

#define XM_ADDFIRSTCHILD        WM_USER
#define XM_FILLFILESCNR         (WM_USER + 1)
#define XM_INSERTOBJARRAY       (WM_USER + 2)
#define XM_UPDATEPOINTER        (WM_USER + 3)

#define ID_TREEFRAME            1
#define ID_FILESFRAME           2

MRESULT EXPENTRY fnwpSubclassedFilesFrame(HWND hwndFrame, ULONG msg, MPARAM mp1, MPARAM mp2);

BOOL StartInsertContents(PFILEDLGDATA pWinData,
                         PMINIRECORDCORE precc);

/* ******************************************************************
 *
 *   Helper funcs
 *
 ********************************************************************/

#define FFL_DRIVE           0x0001
#define FFL_PATH            0x0002
#define FFL_FILEMASK        0x0004
#define FFL_FILENAME        0x0008

/*
 *@@ ParseFileString:
 *      parses the given full-file string and
 *      sets up the members in pWinData accordingly.
 *
 *      This returns a ULONG bitset with any combination
 *      of the following:
 *
 *      -- FFL_DRIVE: drive was specified,
 *         and FILEDLGDATA.ulLogicalDrive has been updated.
 *
 *      -- FFL_PATH: directory was specified,
 *         and FILEDLGDATA.szDir has been updated.
 *
 *      -- FFL_FILEMASK: a file mask was given ('*' or '?'
 *         characters found in filename), and
 *         FILEDLGDATA.szFileMask has been updated.
 *         FILEDLGDATA.szFileName is set to a null byte.
 *         This never comes with FFL_FILENAME.
 *
 *      -- FFL_FILENAME: a file name without wildcards was
 *         given, and FILEDLGDATA.szFileName has been updated.
 *         FILEDLGDATA.is unchanged.
 *         This never comes with FFL_FILEMASK.
 */

ULONG ParseFileString(PFILEDLGDATA pWinData,
                      const char *pcszFullFile)
{
    ULONG ulChanged = 0;

    const char  *p;

    _Pmpf((__FUNCTION__ ": parsing %s", pcszFullFile));

    // check if a drive is specified
    if (    (*pcszFullFile)
         && (pcszFullFile[1] == ':')
       )
    {
        // yes:
        LONG lNew = toupper(*pcszFullFile) - 'A' + 1;
        if (lNew > 0 && lNew <= 26)
            pWinData->ulLogicalDrive = lNew;

        _Pmpf(("  new logical drive is %d", pWinData->ulLogicalDrive));
        p = pcszFullFile + 2;
        ulChanged |= FFL_DRIVE;
    }
    else
        p = pcszFullFile;

    // get path from there
    if (*p)
    {
        const char *p2 = strrchr(p, '\\');
        if (p2)
        {
            // path specified:
            // ### handle relative paths here
            strhncpy0(pWinData->szDir,
                      p,        // start: either first char or after drive
                      (p2 - p));
            _Pmpf(("  new path is %s", pWinData->szDir));
            ulChanged |= FFL_PATH;
            p2++;
        }
        else
            // no path specified:
            p2 = p;

        // check if the following is a file mask
        // or a real file name
        if (    (strchr(p2, '*'))
             || (strchr(p2, '?'))
           )
        {
            // get file name (mask) after that
            strcpy(pWinData->szFileMask,
                   p2);
            ulChanged |= FFL_FILEMASK;
        }
        else
        {
            // name only:

            // compose full file name
            CHAR szFull[CCHMAXPATH];
            FILESTATUS3 fs3;
            BOOL fIsDir = FALSE;

            sprintf(szFull,
                    "%c:%s\\%s",
                    'A' + pWinData->ulLogicalDrive - 1,        // drive letter
                    pWinData->szDir,
                    p2);        // entry
            if (!DosQueryPathInfo(szFull,
                                  FIL_STANDARD,
                                  &fs3,
                                  sizeof(fs3)))
            {
                // this thing exists:
                if (fs3.attrFile & FILE_DIRECTORY)
                    fIsDir = TRUE;
            }

            if (fIsDir)
            {
                strcat(pWinData->szDir, "\\");
                strcat(pWinData->szDir, p2);
                ulChanged |= FFL_PATH;
            }
            else
            {
                // this doesn't exist, or it is a file:
                strcpy(pWinData->szFileName,
                       p2);
                ulChanged |= FFL_FILENAME;
            }
        }
    }

    return (ulChanged);
}

/*
 *@@ GetFSFromRecord:
 *      returns the WPFolder* which is represented
 *      by the specified record.
 *
 *      This resolves shadows and returns root folders
 *      for WPDisk objects cleanly. Returns NULL
 *      if either pobj is not valid or does not
 *      represent a folder.
 *
 *@@added V0.9.9 (2001-03-11) [umoeller]
 */

WPFileSystem* GetFSFromRecord(PMINIRECORDCORE precc,
                              BOOL fFoldersOnly)
{
    WPObject *pobj = NULL;
    if (precc)
    {
        pobj = OBJECT_FROM_PREC(precc);
        if (pobj)
        {
            if (_somIsA(pobj, _WPShadow))
                pobj = _wpQueryShadowedObject(pobj, TRUE);

            if (pobj)
            {
                if (_somIsA(pobj, _WPDisk))
                    pobj = wpshQueryRootFolder(pobj,
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
        }
    }

    return (pobj);
}

/*
 *@@ IsInsertable:
 *      checks if pObject can be inserted in a container.
 *
 *      --  If (fFoldersOnly == TRUE), this will return TRUE
 *          if pObject is a folder. This will correctly resolve
 *          shadows and return TRUE for WPDisk's also. Useful
 *          for checking if an object should appear in a tree
 *          view.
 *
 *          pcszFileMask is ignored in this case.
 *
 *      --  If (fFoldersOnly == FALSE), this will return TRUE
 *          if pObject is a WPFileSystem or a shadow pointing
 *          to one.
 *
 *          In that case, if (pcszFileMask != NULL), the object's
 *          real name is checked against that file mask also.
 *          For example, if (pcszFileMask == *.TXT), this will
 *          return TRUE only if pObject's real name matches
 *          *.TXT.
 *
 *      In any case, FALSE is returned matches the above,
 *      but the file has the "hidden" attribute on.
 */

BOOL IsInsertable(WPObject *pObject,
                  BOOL fFoldersOnly,
                  const char *pcszFileMask)
{
    // resolve shadows
    if (pObject)
        if (_somIsA(pObject, _WPShadow))
            pObject = _wpQueryShadowedObject(pObject, TRUE);

    if (!pObject)
        // broken:
        return (FALSE);

    if (fFoldersOnly)
    {
        // folders only:
        if (pObject)
        {
            if (_somIsA(pObject, _WPDisk))
            {
                // always insert, even if drive not ready
                return (TRUE);
            }
            else
            {
                if (_somIsA(pObject, _WPFolder))
                {
                    // filter out folder templates and hidden folders
                    if (    (!(_wpQueryStyle(pObject) & OBJSTYLE_TEMPLATE))
                         && (!(_wpQueryAttr(pObject) & FILE_HIDDEN))
                       )
                        return (TRUE);
                }
            }
        }
    }
    else
    {
        // not folders-only:
        if (    // filter out non-file systems (shadows pointing to them have been resolved):
                (_somIsA(pObject, _WPFileSystem))
                // filter out hidden objects:
             && (!(_wpQueryAttr(pObject) & FILE_HIDDEN))
           )
        {
            // OK, non-hidden file-system object:
            // regardless of filters, always insert folders
            if (_somIsA(pObject, _WPFolder))
                return (TRUE);          // templates too

            if ((pcszFileMask) && (*pcszFileMask))
            {
                // file mask specified:
                CHAR szRealName[CCHMAXPATH];
                if (_wpQueryFilename(pObject,
                                     szRealName,
                                     FALSE))       // not q'fied
                    return (strhMatchOS2(pcszFileMask, szRealName));
            }
            else
                // no file mask:
                return (TRUE);
        }
    }

    return (FALSE);
}

/*
 *@@ InsertFolderContents:
 *      populates pFolder and inserts the contents of
 *      pFolder into hwndCnr.
 *
 *      You should set the view attributes of hwndCnr
 *      before calling this; they are not modified.
 *
 *      If (precParent == NULL), the object records
 *      are inserted at the root level. Only for tree
 *      views, you may set precParent to an existing
 *      record in the cnr to insert folder contents
 *      into a subtree. precParent should then be the
 *      record matching pFolder in the tree, of course.
 *
 *      If (fFoldersOnly = TRUE), populate occurs with
 *      that flag as well, and only folders (plus disks
 *      plus shadows to folders or disks) are inserted.
 *      This is for the left (drives) view.
 *
 *      If (fFoldersOnly = FALSE), this does a full
 *      populate and inserts all WPFileSystem objects
 *      plus shadows pointing to WPFileSystem objects.
 *      We do not insert other abstracts or transients
 *      because these cannot be opened with the file dialog.
 *      This is for the right (files) view.
 *
 *      While this is running, it keeps checking if
 *      *pfExit is TRUE. If so, the routine exits
 *      immediately. This is useful for having this
 *      routine running in a second thread.
 *
 *      After all objects have been collected and filtered,
 *      this sends (!) XM_INSERTOBJARRAY to hwndMainClient,
 *      which must then insert the objects specified in
 *      the INSERTOBJECTSARRAY pointed to by mp1 and maybe
 *      re-subclass the container owner.
 */

ULONG InsertFolderContents(HWND hwndMainClient,         // in: wnd to send XM_INSERTOBJARRAY to
                           HWND hwndCnr,                // in: cnr to insert reccs to
                           WPFolder *pFolder,           // in: folder whose contents are to be inserted
                           PMINIRECORDCORE precParent,  // in: parent recc or NULL
                           BOOL fFoldersOnly,           // in: folders only?
                           const char *pcszFileMask,    // in: file mask or NULL
                           PLINKLIST pllObjects,        // in/out: if != NULL, list where
                                                        // to append objects to
                           PLINKLIST pllRecords,        // in/out: if != NULL, list where
                                                        // to append inserted records to
                           PBOOL pfExit)            // in: when this goes TRUE, we exit
{
    ULONG       cObjects = 0;

    if (wpshCheckIfPopulated(pFolder,
                             fFoldersOnly))     // folders only?
    {
        BOOL fFolderLocked = FALSE;

        TRY_LOUD(excpt1)
        {
            fFolderLocked = !wpshRequestFolderMutexSem(pFolder, SEM_INDEFINITE_WAIT);
            if (fFolderLocked)
            {
                WPObject    *pObject;
                POINTL      ptlIcon = {0, 0};
                somTD_WPFolder_wpQueryContent rslv_wpQueryContent
                        = (somTD_WPFolder_wpQueryContent)wpshResolveFor(pFolder,
                                                                        NULL,
                                                                        "wpQueryContent");

                // 1) count objects
                for (   pObject = rslv_wpQueryContent(pFolder, NULL, QC_FIRST);
                        (pObject) && (!*pfExit);
                        pObject = rslv_wpQueryContent(pFolder, pObject, QC_NEXT)
                    )
                {
                    if (IsInsertable(pObject, fFoldersOnly, pcszFileMask))
                        cObjects++;
                }

                // 2) build array
                if ((cObjects) && (!*pfExit))
                {
                    WPObject **papObjects = (WPObject**)malloc(sizeof(WPObject*) * cObjects);

                    if (!papObjects)
                        cObjects = 0;
                    else
                    {
                        WPObject **ppThis = papObjects;
                        for (   pObject = rslv_wpQueryContent(pFolder, NULL, QC_FIRST);
                                (pObject) && (!*pfExit);
                                pObject = rslv_wpQueryContent(pFolder, pObject, QC_NEXT)
                            )
                        {
                            if (IsInsertable(pObject, fFoldersOnly, pcszFileMask))
                            {
                                *ppThis = pObject;
                                ppThis++;
                            }
                        }

                        if (!*pfExit)
                        {
                            INSERTOBJECTSARRAY ioa;
                            ioa.pFolder = pFolder;
                            ioa.hwndCnr = hwndCnr;
                            ioa.papObjects = papObjects;
                            ioa.cObjects = cObjects;
                            ioa.precParent = precParent;
                            ioa.pllObjects = pllObjects;
                            ioa.pllRecords = pllRecords;
                            // cross-thread send:
                            // have the main thread insert the objects
                            // because it will have to re-subclass the
                            // container owner probably
                            WinSendMsg(hwndMainClient,
                                       XM_INSERTOBJARRAY,
                                       (MPARAM)&ioa,
                                       0);
                        }

                        free(papObjects);
                    }
                }
            }
        }
        CATCH(excpt1) {} END_CATCH();

        if (fFolderLocked)
            wpshReleaseFolderMutexSem(pFolder);
    }

    return (cObjects);
}

/*
 *@@ ClearContainer:
 *
 */

ULONG ClearContainer(HWND hwndCnr,
                     PLINKLIST pllObjects)
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

    return (ulrc);
}

/*
 *@@ QueryCurrentPointer:
 *      returns the HPOINTER that should be used
 *      according to the present thread state.
 *
 *      Returns a HPOINTER for either the wait or
 *      arrow pointer.
 */

HPOINTER QueryCurrentPointer(HWND hwndMainClient)
{
    ULONG           idPtr = SPTR_ARROW;

    PFILEDLGDATA    pWinData = WinQueryWindowPtr(hwndMainClient, QWL_USER);

    if (pWinData)
        if (    pWinData->fAddChildrenRunning
             || pWinData->fInsertContentsRunning
           )
            idPtr = SPTR_WAIT;

    return (WinQuerySysPointer(HWND_DESKTOP,
                               idPtr,
                               FALSE));
}

/*
 *@@ UpdateDlgWithFullFile:
 *      updates the dialog according to the
 *      current directory/path/file fields
 *      in pWinData.
 */

VOID UpdateDlgWithFullFile(PFILEDLGDATA pWinData,
                           PLINKLIST pllToExpand)
{
    PMINIRECORDCORE precSelect = NULL;

    // go thru the disks list and find the WPDisk*
    // which matches the current logical drive
    PLISTNODE pNode = lstQueryFirstNode(&pWinData->llDisks);
    WPFolder *pRootFolder = NULL;

    _Pmpf((__FUNCTION__ ": pWinData->ulLogicalDrive = %d", pWinData->ulLogicalDrive));

    while (pNode)
    {
        WPDisk *pDisk = (WPDisk*)pNode->pItemData;

        if (_wpQueryLogicalDrive(pDisk) == pWinData->ulLogicalDrive)
        {
            precSelect = _wpQueryCoreRecord(pDisk);
            pRootFolder = wpshQueryRootFolder(pDisk, FALSE, NULL);
            break;
        }

        pNode = pNode->pNext;
    }

    _Pmpf(("    precDisk = 0x%lX", precSelect));

    if ((precSelect) && (pRootFolder))
    {
        // we got a valid disk and root folder:
        WPFolder *pFullFolder;

        // awake the directory we currently have
        CHAR szFull[CCHMAXPATH];
        sprintf(szFull,
                "%c:%s",
                'A' + pWinData->ulLogicalDrive - 1,        // drive letter
                pWinData->szDir);
        pFullFolder = _wpclsQueryFolder(_WPFolder, szFull, TRUE);
                    // this also awakes all folders in between
                    // the root folder and the full folder

        // now go for all the path particles, starting with the
        // root folder
        if (pFullFolder)
        {
            CHAR szComponent[CCHMAXPATH];
            PMINIRECORDCORE precParent = precSelect;    // start with disk
            WPFolder *pFdrThis = pRootFolder;
            const char *pcThis = &pWinData->szDir[1];   // start after root '\'

            _Pmpf(("    got folder %s for %s",
                    _wpQueryTitle(pFullFolder),
                    szFull));

            if (pllToExpand)
                lstAppendItem(pllToExpand, precSelect);

            while (    (pFdrThis)
                    && (*pcThis)
                  )
            {
                const char *pBacksl = strchr(pcThis, '\\');
                WPFileSystem *pobj;

                _Pmpf(("    remaining: %s", pcThis));

                if (!pBacksl)
                    strcpy(szComponent, pcThis);
                else
                {
                    ULONG c = (pBacksl - pcThis);
                    memcpy(szComponent,
                           pcThis,
                           c);
                    szComponent[c] = '\0';
                }

                // now szComponent contains the current
                // path component;
                // e.g. if szDir was "F:\OS2\BOOK",
                // we now have "OS2"

                _Pmpf(("    checking component %s", szComponent));

                // find this component in pFdrThis
                pobj = fdrFindFSFromName(pFdrThis,
                                         szComponent);
                if (pobj && _somIsA(pobj, _WPFolder))
                {
                    // got that folder:
                    POINTL ptlIcon = {0, 0};
                    PMINIRECORDCORE pNew;
                    _Pmpf(("        -> got %s", _wpQueryTitle(pobj)));

                    pNew = _wpCnrInsertObject(pobj,
                                              pWinData->hwndDrivesCnr,
                                              &ptlIcon,
                                              precParent,  // parent == previous folder
                                              NULL); // next available position

                    _Pmpf(("        got prec 0x%lX", precParent));
                    if (pNew)
                    {
                        if (!WinSendMsg(pWinData->hwndDrivesCnr,
                                        CM_EXPANDTREE,
                                        (MPARAM)precParent,
                                        0))
                            _Pmpf(("        cannot expand!!!"));

                        if (pllToExpand)
                            lstAppendItem(pllToExpand, pNew);

                        precParent = pNew;
                        precSelect = precParent;
                    }
                    else
                        break;

                    if (pBacksl)
                    {
                        // OK, go on with that folder
                        pFdrThis = pobj;
                        pcThis = pBacksl + 1;
                    }
                    else
                        break;
                }
                else
                    break;
            } // while (pFdrThis && *pcThis)

        } // if (pFullFolder)
    } // if ((precSelect) && (pRootFolder))

    if (precSelect)
    {
        ULONG ul;
        // got valid folder, apparently:
        cnrhSelectRecord(pWinData->hwndDrivesCnr,
                         precSelect,
                         TRUE);
        ul = cnrhScrollToRecord(pWinData->hwndDrivesCnr,
                                (PRECORDCORE)precSelect,
                                CMA_ICON | CMA_TEXT | CMA_TREEICON,
                                TRUE);       // keep parent
        if (ul && ul != 3)
            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                    "Error: cnrhScrollToRecord returned %d", ul);
    }

    _Pmpf((__FUNCTION__ ": exiting"));
}

/*
 *@@ ParseAndUpdate:
 *
 */

VOID ParseAndUpdate(PFILEDLGDATA pWinData,
                    const char *pcszFullFile)
{
     // parse the new file string
     ULONG fl = ParseFileString(pWinData,
                                pcszFullFile);

     if (fl & FFL_FILENAME)
     {
         // no wildcard, but file specified:
         pWinData->ulReturnCode = DID_OK;
         WinPostMsg(pWinData->hwndMainClient, WM_CLOSE, 0, 0);
                 // main msg loop detects that
         // get outta here
         return;
     }

     if (fl & (FFL_DRIVE | FFL_PATH))
     {
         // drive or path specified:
         // expand that
         PLINKLIST pllToExpand = lstCreate(FALSE);
         UpdateDlgWithFullFile(pWinData,
                               pllToExpand);
         // start "add first child" thread which will
         // insert sub-records for each item in the drives
         // tree
         WinPostMsg(pWinData->hwndMainClient,
                    XM_ADDFIRSTCHILD,
                    (MPARAM)pllToExpand,
                    0);
     }

     if (fl & FFL_FILEMASK)
     {
         // file mask changed:
         // update files list
         StartInsertContents(pWinData,
                             pWinData->precSelectedInDrives);
     }
}

/*
 *@@ BuildDisksList:
 *      builds a linked list of all WPDisk* objects
 *      in pDrivesFolder, which better be the real
 *      "Drives" folder.
 */

VOID BuildDisksList(WPFolder *pDrivesFolder,
                    PLINKLIST pllDisks)
{
    if (wpshCheckIfPopulated(pDrivesFolder,
                             FALSE))     // folders only?
    {
        BOOL fFolderLocked = FALSE;

        TRY_LOUD(excpt1)
        {
            fFolderLocked = !wpshRequestFolderMutexSem(pDrivesFolder, SEM_INDEFINITE_WAIT);
            if (fFolderLocked)
            {
                WPObject *pObject;
                somTD_WPFolder_wpQueryContent rslv_wpQueryContent
                        = (somTD_WPFolder_wpQueryContent)wpshResolveFor(pDrivesFolder,
                                                                        NULL,
                                                                        "wpQueryContent");

                // 1) count objects
                for (   pObject = rslv_wpQueryContent(pDrivesFolder, NULL, QC_FIRST);
                        (pObject);
                        pObject = rslv_wpQueryContent(pDrivesFolder, pObject, QC_NEXT)
                    )
                {
                    if (_somIsA(pObject, _WPDisk))
                        lstAppendItem(pllDisks, pObject);
                }
            }
        }
        CATCH(excpt1) {} END_CATCH();

        if (fFolderLocked)
            wpshReleaseFolderMutexSem(pDrivesFolder);
    }
}

/* ******************************************************************
 *
 *   Add-children thread
 *
 ********************************************************************/

/*
 *@@ fntAddChildren:
 *      adds the children of the folder specified
 *      by INSERTTHREADSDATA.precc into the drives tree.
 *
 *      This expects a INSERTTHREADSDATA pointer
 *      as ulUser, which is free()'d on exit.
 *
 *      If INSERTTHREADSDATA.ll contains anything,
 *      the list items are expected to be MINIRECORDCORE's
 *      of tree items to be expanded.
 *
 *      If the list is empty, pWinData->pDrivesFolder
 *      is expanded instead.
 */

VOID _Optlink fntAddChildren(PTHREADINFO ptiMyself)
{
    PINSERTTHREADSDATA pThreadData
        = (PINSERTTHREADSDATA)ptiMyself->ulData;
    if (pThreadData)
    {
        PFILEDLGDATA pWinData = pThreadData->pWinData;

        LINKLIST    llInsertedRecords;
        lstInit(&llInsertedRecords, FALSE);

        if (pThreadData->pll)
        {
            // we have a list:
            PLISTNODE pNode = lstQueryFirstNode(pThreadData->pll);

            _Pmpf(("    pll has %d items", lstCountItems(pThreadData->pll)));

            while (pNode)
            {
                PMINIRECORDCORE prec = (PMINIRECORDCORE)pNode->pItemData;
                // get the folder from that, if it's insertable

                BOOL fFolderLocked = FALSE;

                TRY_LOUD(excpt1)
                {
                    // lock the drives folder... we're not really taking the
                    // contents, but we don't want objects to disappear, and
                    // we don't want thread 1 to hack the contents either
                    // while we're doing this
                    fFolderLocked = !wpshRequestFolderMutexSem(pWinData->pDrivesFolder, SEM_INDEFINITE_WAIT);
                    if (fFolderLocked)
                    {
                        // get the object from that
                        WPObject *pobj = GetFSFromRecord(prec, TRUE);
                        if (pobj)
                        {
                            // OK, now we have a folder to insert:
                            InsertFolderContents(pWinData->hwndMainClient,
                                                 pWinData->hwndDrivesCnr,
                                                 pobj,      // folder
                                                 prec,    // parent
                                                 TRUE,      // folders only
                                                 NULL,      // no file mask
                                                 &pWinData->llDriveObjectsInserted,
                                                 &llInsertedRecords,
                                                 &ptiMyself->fExit);
                        }
                    }
                }
                CATCH(excpt1) {} END_CATCH();

                if (fFolderLocked)
                    wpshReleaseFolderMutexSem(pWinData->pDrivesFolder);

                pNode = pNode->pNext;
            }

            // now go thru the records that have been
            // inserted and add children to them again
            pNode = lstQueryFirstNode(&llInsertedRecords);
            while (pNode)
            {
                PMINIRECORDCORE prec = (PMINIRECORDCORE)pNode->pItemData;

                BOOL fFolderLocked = FALSE;

                TRY_LOUD(excpt1)
                {
                    // lock the drives folder... we're not really taking the
                    // contents, but we don't want objects to disappear, and
                    // we don't want thread 1 to hack the contents either
                    // while we're doing this
                    fFolderLocked = !wpshRequestFolderMutexSem(pWinData->pDrivesFolder, SEM_INDEFINITE_WAIT);
                    if (fFolderLocked)
                    {
                        // get the object from that
                        WPObject *pobj = GetFSFromRecord(prec, TRUE);
                        if (pobj)
                        {
                            // OK, now we have a folder to insert:
                            InsertFolderContents(pWinData->hwndMainClient,
                                                 pWinData->hwndDrivesCnr,
                                                 pobj,      // folder
                                                 prec,    // parent
                                                 TRUE,      // folders only
                                                 NULL,      // no file mask
                                                 &pWinData->llDriveObjectsInserted,
                                                 NULL,
                                                 &ptiMyself->fExit);
                        }
                    }
                }
                CATCH(excpt1) {} END_CATCH();

                pNode = pNode->pNext;
            }

            lstFree(pThreadData->pll);
        }

        lstClear(&llInsertedRecords);

        WinPostMsg(pWinData->hwndMainClient,
                   XM_UPDATEPOINTER,
                   0, 0);

        free(pThreadData);
    }
}

/*
 *@@ StartAddChildren:
 *      Expects a LINKLIST of records to expand
 *      in pllRecords.
 *
 *      Returns FALSE if the thread was not started
 *      because it was already running.
 *
 *      If the thread was started, the list eventually
 *      gets freed by the thread. Otherwise the caller
 *      is responsible for freeing the list.
 *
 *@@added V0.9.9 (2001-03-10) [umoeller]
 */

BOOL StartAddChildren(PFILEDLGDATA pWinData,
                      PLINKLIST pllRecords)      // in: never NULL
{
    BOOL brc = FALSE;

    if (!pWinData->fAddChildrenRunning)
    {
        PINSERTTHREADSDATA pData = malloc(sizeof(INSERTTHREADSDATA));
        if (pData)
        {
            pData->pWinData = pWinData;
            pData->pll = pllRecords;
            brc = (thrCreate(&pWinData->tiAddChildren,
                             fntAddChildren,
                             &pWinData->fAddChildrenRunning,
                             "AddChildren",
                             THRF_PMMSGQUEUE | THRF_WAIT,
                             (ULONG)pData)
                        != 0);
        }
    }

    WinSetPointer(HWND_DESKTOP,
                  QueryCurrentPointer(pWinData->hwndMainClient));

    return (brc);
}

/* ******************************************************************
 *
 *   Insert-Contents thread
 *
 ********************************************************************/

/*
 *@@ fntInsertContents:
 *      adds the entire contents of the folder specified
 *      by INSERTTHREADSDATA.precc into the files container.
 *
 *      This expects a INSERTTHREADSDATA pointer
 *      as ulUser, which is free()'d on exit.
 */

VOID _Optlink fntInsertContents(PTHREADINFO ptiMyself)
{
    PINSERTTHREADSDATA pThreadData
        = (PINSERTTHREADSDATA)ptiMyself->ulData;
    if (pThreadData)
    {
        PFILEDLGDATA pWinData = pThreadData->pWinData;

        if (pWinData)
        {
            if (pThreadData->pll)
            {
                PLISTNODE pNode = lstQueryFirstNode(pThreadData->pll);
                while (pNode)
                {
                    PMINIRECORDCORE prec = (PMINIRECORDCORE)pNode->pItemData;
                    WPFolder *pFolder = GetFSFromRecord(prec, TRUE);
                    if (pFolder)
                    {
                        ClearContainer(pWinData->hwndFilesCnr,
                                       &pWinData->llFileObjectsInserted);

                        if (pFolder)
                        {
                            InsertFolderContents(pWinData->hwndMainClient,
                                                 pWinData->hwndFilesCnr,
                                                 pFolder,
                                                 NULL,      // no parent
                                                 FALSE,         // all records
                                                 pWinData->szFileMask, // file mask
                                                 &pWinData->llFileObjectsInserted,
                                                 NULL,
                                                 &ptiMyself->fExit);
                        }
                    }

                    pNode = pNode->pNext;
                }

                lstFree(pThreadData->pll);
            }

            WinPostMsg(pWinData->hwndMainClient,
                       XM_UPDATEPOINTER,
                       0, 0);
        }

        free(pThreadData);
    }
}

/*
 *@@ StartInsertContents:
 *      Returns FALSE if the thread was not started
 *      because it was already running.
 *
 *@@added V0.9.9 (2001-03-10) [umoeller]
 */

BOOL StartInsertContents(PFILEDLGDATA pWinData,
                         PMINIRECORDCORE precc)      // in: record of folder (never NULL)
{
    if (pWinData->fInsertContentsRunning)
        return (FALSE);
    else
    {
        PINSERTTHREADSDATA pData = malloc(sizeof(INSERTTHREADSDATA));
        if (pData)
        {
            pData->pWinData = pWinData;
            pData->pll = lstCreate(FALSE);
            lstAppendItem(pData->pll, precc);
            thrCreate(&pWinData->tiInsertContents,
                      fntInsertContents,
                      &pWinData->fInsertContentsRunning,
                      "InsertContents",
                      THRF_PMMSGQUEUE | THRF_WAIT,
                      (ULONG)pData);
        }
    }

    WinSetPointer(HWND_DESKTOP,
                  QueryCurrentPointer(pWinData->hwndMainClient));

    return (TRUE);
}

/* ******************************************************************
 *
 *   File dialog window procs
 *
 ********************************************************************/

HWND winhCreateControl(HWND hwndParentAndOwner,
                       const char *pcszClass,
                       const char *pcszText,
                       ULONG ulStyle,
                       ULONG ulID)
{
    return (WinCreateWindow(hwndParentAndOwner,
                            (PSZ)pcszClass,
                            (PSZ)pcszText,
                            ulStyle,
                            0, 0, 0, 0,
                            hwndParentAndOwner,
                            HWND_TOP,
                            ulID,
                            NULL,
                            NULL));
}

/*
 *@@ MainClientCreate:
 *      part of the implementation for WM_CREATE. This
 *      creates the controls on the bottom of the main client.
 */

VOID MainClientCreate(HWND hwnd,
                      PFILEDLGDATA pWinData)
{
    // set the window font for the main client...
    // all controls will inherit this
    winhSetWindowFont(hwnd,
                      cmnQueryDefaultFont());


    pWinData->hwndOK
        = winhCreateControl(hwnd,           // parent
                          WC_BUTTON,
                          "~OK",          // @@todo localize
                          WS_VISIBLE | BS_PUSHBUTTON | BS_DEFAULT,
                          DID_OK);

    pWinData->hwndCancel
        = winhCreateControl(hwnd,           // parent
                          WC_BUTTON,
                          "~Cancel",          // @@todo localize
                          WS_VISIBLE | BS_PUSHBUTTON,
                          DID_CANCEL);

    pWinData->hwndHelp
        = winhCreateControl(hwnd,           // parent
                          WC_BUTTON,
                          "~Help",        // @@todo localize
                          WS_VISIBLE | BS_PUSHBUTTON | BS_HELP,
                          DID_HELP);

    pWinData->hwndDirTxt
        = winhCreateControl(hwnd,           // parent
                          WC_STATIC,
                          "Directory:",   // @@todo localize
                          WS_VISIBLE | SS_TEXT | DT_LEFT | DT_VCENTER | DT_MNEMONIC,
                          -1);

    pWinData->hwndDirValue
        = winhCreateControl(hwnd,           // parent
                          WC_STATIC,
                          "",
                          WS_VISIBLE | SS_TEXT | DT_LEFT | DT_VCENTER,
                          -1);

    pWinData->hwndFileTxt
        = winhCreateControl(hwnd,           // parent
                          WC_STATIC,
                          "~File:",   // @@todo localize
                          WS_VISIBLE | SS_TEXT | DT_LEFT | DT_VCENTER | DT_MNEMONIC,
                          -1);

    pWinData->hwndFileEntry
        = winhCreateControl(hwnd,           // parent
                          WC_ENTRYFIELD,
                          pWinData->szFileMask,     // initial text: file mask
                          WS_VISIBLE | ES_LEFT | ES_AUTOSCROLL | ES_MARGIN,
                          -1);
    winhSetEntryFieldLimit(pWinData->hwndFileEntry, CCHMAXPATH - 1);
}

/*
 *@@ MainClientRepositionControls:
 *
 */

VOID MainClientRepositionControls(HWND hwnd,
                                  PFILEDLGDATA pWinData,
                                  MPARAM mp2)
{
    #define OUTER_SPACING       5

    #define BUTTON_WIDTH        100
    #define BUTTON_HEIGHT       30
    #define BUTTON_SPACING      10

    #define STATICS_HEIGHT      20
    #define STATICS_LEFTWIDTH   100

    #define SPACE_BOTTOM        (BUTTON_HEIGHT + OUTER_SPACING \
                                 + 2*STATICS_HEIGHT + 2*OUTER_SPACING)
    SWP     aswp[8];        // three buttons
                            // plus two statics for directory
                            // plus two controls for file
                            // plus split window
    ULONG   i = 0;

    memset(aswp, 0, sizeof(aswp));

    // split window
    aswp[i].fl = SWP_MOVE | SWP_SIZE;
    aswp[i].x = OUTER_SPACING;
    aswp[i].y = OUTER_SPACING + SPACE_BOTTOM;
    aswp[i].cx =   SHORT1FROMMP(mp2)   // new win cx
                 - 2 * OUTER_SPACING;
    aswp[i].cy = SHORT2FROMMP(mp2)     // new win cy
                 - 2 * OUTER_SPACING
                 - SPACE_BOTTOM;
    aswp[i].hwndInsertBehind = HWND_TOP;
    aswp[i++].hwnd = pWinData->hwndSplitWindow;

    // "Directory:" static
    aswp[i].fl = SWP_MOVE | SWP_SIZE;
    aswp[i].x = OUTER_SPACING;
    aswp[i].y = BUTTON_HEIGHT + 3 * OUTER_SPACING + STATICS_HEIGHT;
    aswp[i].cx = STATICS_LEFTWIDTH;
    aswp[i].cy = STATICS_HEIGHT;
    aswp[i].hwndInsertBehind = HWND_TOP;
    aswp[i++].hwnd = pWinData->hwndDirTxt;

    // directory value static
    aswp[i].fl = SWP_MOVE | SWP_SIZE;
    aswp[i].x = 2*OUTER_SPACING + STATICS_LEFTWIDTH;
    aswp[i].y = BUTTON_HEIGHT + 3 * OUTER_SPACING + STATICS_HEIGHT;
    aswp[i].cx = SHORT1FROMMP(mp2)   // new win cx
                    - (3*OUTER_SPACING + STATICS_LEFTWIDTH);
    aswp[i].cy = STATICS_HEIGHT;
    aswp[i].hwndInsertBehind = HWND_TOP;
    aswp[i++].hwnd = pWinData->hwndDirValue;

    // "File:" static
    aswp[i].fl = SWP_MOVE | SWP_SIZE;
    aswp[i].x = OUTER_SPACING;
    aswp[i].y = BUTTON_HEIGHT + 2 * OUTER_SPACING;
    aswp[i].cx = STATICS_LEFTWIDTH;
    aswp[i].cy = STATICS_HEIGHT;
    aswp[i].hwndInsertBehind = HWND_TOP;
    aswp[i++].hwnd = pWinData->hwndFileTxt;

    // file entry field
    aswp[i].fl = SWP_MOVE | SWP_SIZE;
    aswp[i].x = 2*OUTER_SPACING + STATICS_LEFTWIDTH;
    aswp[i].y = BUTTON_HEIGHT + 2 * OUTER_SPACING + 2;
    aswp[i].cx = SHORT1FROMMP(mp2)   // new win cx
                    - (3*OUTER_SPACING + STATICS_LEFTWIDTH);
    aswp[i].cy = STATICS_HEIGHT - 2 * 2;
    aswp[i].hwndInsertBehind = HWND_TOP;
    aswp[i++].hwnd = pWinData->hwndFileEntry;

    // "OK" button
    aswp[i].fl = SWP_MOVE | SWP_SIZE;
    aswp[i].x = OUTER_SPACING;
    aswp[i].y = OUTER_SPACING;
    aswp[i].cx = BUTTON_WIDTH;
    aswp[i].cy = BUTTON_HEIGHT;
    aswp[i].hwndInsertBehind = HWND_TOP;
    aswp[i++].hwnd = pWinData->hwndOK;

    // "Cancel" button
    aswp[i].fl = SWP_MOVE | SWP_SIZE;
    aswp[i].x = OUTER_SPACING + BUTTON_SPACING + BUTTON_WIDTH;
    aswp[i].y = OUTER_SPACING;
    aswp[i].cx = BUTTON_WIDTH;
    aswp[i].cy = BUTTON_HEIGHT;
    aswp[i].hwndInsertBehind = HWND_TOP;
    aswp[i++].hwnd = pWinData->hwndCancel;

    // "Help" button
    aswp[i].fl = SWP_MOVE | SWP_SIZE;
    aswp[i].x = OUTER_SPACING + 2*BUTTON_SPACING + 2*BUTTON_WIDTH;
    aswp[i].y = OUTER_SPACING;
    aswp[i].cx = BUTTON_WIDTH;
    aswp[i].cy = BUTTON_HEIGHT;
    aswp[i].hwndInsertBehind = HWND_TOP;
    aswp[i++].hwnd = pWinData->hwndHelp;

    WinSetMultWindowPos(WinQueryAnchorBlock(hwnd),
                        aswp,
                        i);
}

/*
 *@@ fnwpMainClient:
 *      winproc for the main client (child of the main frame
 *      of the file dlg). By definition, this winproc is
 *      reponsible for managing the actual dialog functionality...
 *      populating subfolders depending on record selections
 *      and all that.
 *
 *      The file dlg has the following window hierarchy:
 *
 +      WC_FRAME        (main file dlg, not subclassed)
 +        |
 +        +--- WC_CLIENT (fnwpMainClient)
 +                |
 +                +--- split window (cctl_splitwin.c)
 +                |      |
 +                |      +--- left split view, WC_FRAME with ID_TREEFRAME;
 +                |      |    subclassed with fnwpSubclassedDrivesFrame
 +                |      |      |
 +                |      |      +--- WC_CONTAINER (FID_CLIENT)
 +                |      |
 +                |      +--- right split view, WC_FRAME with ID_FILESFRAME
 +                |      |    subclassed with fnwpSubclassedFilesFrame
 +                |      |      |
 +                |      |      +--- WC_CONTAINER (FID_CLIENT)
 +                |
 +                +--- buttons, entry field etc.
 +
 *
 *@@added V0.9.9 (2001-03-10) [umoeller]
 */

MRESULT EXPENTRY fnwpMainClient(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    switch (msg)
    {
        case WM_CREATE:
            if (mp1)
            {
                PFILEDLGDATA pWinData = (PFILEDLGDATA)mp1;
                // store this in window words
                WinSetWindowPtr(hwnd, QWL_USER, mp1);

                // create controls
                MainClientCreate(hwnd, pWinData);
            }
            else
                // no PFILEDLGDATA:
                // stop window creation
                mrc = (MPARAM)TRUE;
        break;

        case WM_PAINT:
        {
            RECTL rclPaint;
            HPS hps = WinBeginPaint(hwnd, NULLHANDLE, &rclPaint);
            GpiCreateLogColorTable(hps, 0, LCOLF_RGB, 0, 0, NULL);
            WinFillRect(hps,
                        &rclPaint,
                        WinQuerySysColor(HWND_DESKTOP,
                                         SYSCLR_DIALOGBACKGROUND,
                                         0));
            WinEndPaint(hps);
        break; }

        /*
         * WM_SIZE:
         *      resize all the controls.
         */

        case WM_SIZE:
        {

            PFILEDLGDATA pWinData = WinQueryWindowPtr(hwnd, QWL_USER);
            if (pWinData)
                MainClientRepositionControls(hwnd, pWinData, mp2);
        break; }

        /*
         * WM_COMMAND:
         *      button pressed.
         */

        case WM_COMMAND:
        {
            PFILEDLGDATA pWinData = WinQueryWindowPtr(hwnd, QWL_USER);

            if (pWinData->fFileDlgReady)
                switch ((ULONG)mp1)
                {
                    case DID_OK:
                    {
                        // "OK" is tricky... we first need to
                        // get the thing from the entry field
                        // and check if it contains a wildcard.
                        // For some reason people have become
                        // accustomed to this.
                        PSZ pszFullFile = winhQueryWindowText(pWinData->hwndFileEntry);
                        if (pszFullFile)
                        {
                            ParseAndUpdate(pWinData,
                                           pszFullFile);
                            free(pszFullFile);
                        }

                    break; }

                    case DID_CANCEL:
                        pWinData->ulReturnCode = DID_CANCEL;
                        WinPostMsg(hwnd, WM_CLOSE, 0, 0);
                                // main msg loop detects that
                    break;
                }
        break; }

        /*
         * WM_CHAR:
         *
         */

        case WM_CHAR:
            DosBeep(1000, 10);
        break;

        /*
         * XM_ADDFIRSTCHILD:
         *      gets posted from fnwpSubclassedDrivesFrame when
         *      a tree item gets expanded on the left drives tree.
         *
         *      Has a PLINKLIST of MINIRECORDCOREs of folders to
         *      expand in mp1.
         */

        case XM_ADDFIRSTCHILD:
        {
            PFILEDLGDATA pWinData = WinQueryWindowPtr(hwnd, QWL_USER);

            if (pWinData->fFileDlgReady)
                if (!StartAddChildren(pWinData,
                                      (PLINKLIST)mp1))
                {
                    // thread is still busy:
                    // repost
                    winhSleep(100);
                    WinPostMsg(hwnd, msg, mp1, mp2);
                }

        break; }

        /*
         * XM_FILLFILESCNR:
         *      gets posted from fnwpSubclassedDrivesFrame when
         *      a tree item gets selected in the left drives tree.
         *
         *      Has MINIRECORDCORE of "folder" to fill files cnr with
         *      in mp1.
         */

        case XM_FILLFILESCNR:
        {
            PFILEDLGDATA pWinData = WinQueryWindowPtr(hwnd, QWL_USER);
            PMINIRECORDCORE prec = (PMINIRECORDCORE)mp1;

            if (prec != pWinData->precSelectedInDrives)
            {
                pWinData->precSelectedInDrives = prec;

                if (pWinData->fFileDlgReady)
                {
                    if (!StartInsertContents(pWinData,
                                             prec))
                    {
                        // thread is still busy:
                        // repost
                        winhSleep(100);
                        WinPostMsg(hwnd, msg, mp1, mp2);
                    }
                    else
                    {
                        // thread started:
                        // check if the object is a disk object
                        WPObject *pobj = OBJECT_FROM_PREC(prec);
                        if (_somIsA(pobj, _WPDisk))
                        {
                            PLINKLIST pll = lstCreate(FALSE);
                            lstAppendItem(pll, prec);
                            WinPostMsg(hwnd,
                                       XM_ADDFIRSTCHILD,
                                       (MPARAM)pll,
                                       (MPARAM)0);
                        }
                    }
                }
            }
        break; }

        /*
         * XM_INSERTOBJARRAY:
         *      has INSERTOBJECTSARRAY pointer in mp1.
         *      This is _sent_ always so the pointer must
         *      not be freed.
         */

        case XM_INSERTOBJARRAY:
        {
            PFILEDLGDATA pWinData = WinQueryWindowPtr(hwnd, QWL_USER);
            PINSERTOBJECTSARRAY pioa = (PINSERTOBJECTSARRAY)mp1;
            if (pioa)
            {
                CHAR    szPathName[CCHMAXPATH];
                ULONG   ul;
                POINTL  ptlIcon = {0, 0};
                _Pmpf(("XM_INSERTOBJARRAY: inserting %d recs",
                        pioa->cObjects));

                // now differentiate...
                // if this is for the "files" container,
                // we can do wpclsInsertMultipleObjects,
                // which is way faster.
                if (pioa->hwndCnr != pWinData->hwndFilesCnr)
                {
                    // this is for the "drives" container:
                    // we can't use wpclsInsertMultipleObjects
                    // because this fails if only one object
                    // has already been inserted, which is
                    // frequently the case here. Dull WPS.
                    // So we need to insert the objects one
                    // by one.
                    for (ul = 0;
                         ul < pioa->cObjects;
                         ul++)
                    {
                        BOOL fAppend = FALSE;
                        WPObject *pobjThis = pioa->papObjects[ul];

                        PMINIRECORDCORE prec;
                        if (prec = _wpCnrInsertObject(pobjThis,
                                                      pioa->hwndCnr,
                                                      &ptlIcon,
                                                      pioa->precParent,
                                                      NULL))
                        {
                            // lock the object!! we must make sure
                            // the WPS won't let it go dormant
                            _wpLockObject(pobjThis);
                                    // unlock is in "clear container"

                            if (pioa->pllObjects)
                                lstAppendItem(pioa->pllObjects, pobjThis);
                            if (pioa->pllRecords)
                                lstAppendItem(pioa->pllRecords, prec);
                        }
                    }
                }
                else
                {
                    // files container:
                    // then we can use fast insert
                    _wpclsInsertMultipleObjects(_somGetClass(pioa->pFolder),
                                                pioa->hwndCnr,
                                                NULL,
                                                (PVOID*)pioa->papObjects,
                                                pioa->precParent,
                                                pioa->cObjects);

                    for (ul = 0;
                         ul < pioa->cObjects;
                         ul++)
                    {
                        BOOL fAppend = FALSE;
                        WPObject *pobjThis = pioa->papObjects[ul];

                        // lock the object!! we must make sure
                        // the WPS won't let it go dormant
                        _wpLockObject(pobjThis);
                                // unlock is in "clear container"
                        if (pioa->pllObjects)
                            lstAppendItem(pioa->pllObjects, pobjThis);
                    }

                    // set new "directory" on bottom
                    _wpQueryFilename(pioa->pFolder,
                                     szPathName,
                                     TRUE);     // fully q'fied
                    WinSetWindowText(pWinData->hwndDirValue,
                                     szPathName);

                    // now, if this is for the files cnr, we must make
                    // sure that the container owner (the parent frame)
                    // has been subclassed by us... we do this AFTER
                    // the WPS subclasses the owner, which happens during
                    // record insertion

                    if (!pWinData->fFilesFrameSubclassed)
                    {
                        // not subclassed yet:
                        // subclass now

                        pWinData->psfvFiles
                            = fdrCreateSFV(pWinData->hwndFilesFrame,
                                           pWinData->hwndFilesCnr,
                                           QWL_USER,
                                           pioa->pFolder,
                                           pioa->pFolder);
                        pWinData->psfvFiles->pfnwpOriginal
                            = WinSubclassWindow(pWinData->hwndFilesFrame,
                                                fnwpSubclassedFilesFrame);
                        pWinData->fFilesFrameSubclassed = TRUE;
                    }
                    else
                    {
                        // already subclassed:
                        // update the folder pointers in the SFV
                        pWinData->psfvFiles->somSelf = pioa->pFolder;
                        pWinData->psfvFiles->pRealObject = pioa->pFolder;
                    }
                }
            }

        break; }

        /*
         * XM_UPDATEPOINTER:
         *      posted when threads exit etc. to update
         *      the current pointer.
         */

        case XM_UPDATEPOINTER:
            WinSetPointer(HWND_DESKTOP,
                          WinQuerySysPointer(HWND_DESKTOP,
                                             SPTR_ARROW,
                                             FALSE));
        break;

        default:
            mrc = WinDefWindowProc(hwnd, msg, mp1, mp2);
    }

    return (mrc);
}

/*
 *@@ fnwpSubclassedDrivesFrame:
 *      subclassed frame window on the left for the
 *      "Drives" container.
 *
 *      We use the XFolder subclassed window proc for
 *      most messages. In addition, we intercept a
 *      couple more for extra features.
 */

MRESULT EXPENTRY fnwpSubclassedDrivesFrame(HWND hwndFrame, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT                 mrc = 0;
    BOOL                    fCallDefault = FALSE;
    PSUBCLASSEDFOLDERVIEW   psfv = WinQueryWindowPtr(hwndFrame, QWL_USER);

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
                     * CN_EXPANDTREE:
                     *      user clicked on "+" sign next to
                     *      tree item; expand that, but start
                     *      "add first child" thread again
                     */

                    case CN_EXPANDTREE:
                    {
                        PLINKLIST pll = lstCreate(FALSE);
                        lstAppendItem(pll, mp2);        // record
                        WinPostMsg(WinQueryWindow(hwndFrame, QW_OWNER),
                                                    // the main client
                                   XM_ADDFIRSTCHILD,
                                   (MPARAM)pll,
                                   (MPARAM)0);
                        // and call default because xfolder
                        // handles auto-scroll
                        fCallDefault = TRUE;
                    break; }

                    /*
                     * CN_EMPHASIS:
                     *      selection changed:
                     */

                    case CN_EMPHASIS:
                    {
                        PNOTIFYRECORDEMPHASIS pnre = (PNOTIFYRECORDEMPHASIS)mp2;
                        if (    (pnre->pRecord)
                             && (pnre->fEmphasisMask & CRA_SELECTED)
                             && (((PMINIRECORDCORE)(pnre->pRecord))->flRecordAttr & CRA_SELECTED)
                           )
                        {
                            WinPostMsg(WinQueryWindow(hwndFrame, QW_OWNER),
                                                    // the main client
                                       XM_FILLFILESCNR,
                                       (MPARAM)pnre->pRecord,
                                       0);
                        }
                    break; }

                    default:
                        fCallDefault = TRUE;
                }
            }
            else
                fCallDefault = TRUE;
        break; }

        case WM_CHAR:
            // forward to main client
            WinPostMsg(WinQueryWindow(hwndFrame, QW_OWNER),
                       msg,
                       mp1,
                       mp2);
        break;

        case WM_CONTROLPOINTER:
        {
            HWND hwndMainClient = WinQueryWindow(hwndFrame, QW_OWNER);
                                    // the main client
            mrc = (MPARAM)QueryCurrentPointer(hwndMainClient);
        break; }

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

    return (mrc);
}

/*
 *@@ fnwpSubclassedFilesFrame:
 *      subclassed frame window on the right for the
 *      "Files" container.
 *
 *      We use the XFolder subclassed window proc for
 *      most messages. In addition, we intercept a
 *      couple more for extra features.
 */

MRESULT EXPENTRY fnwpSubclassedFilesFrame(HWND hwndFrame, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT                 mrc = 0;
    BOOL                    fCallDefault = FALSE;
    PSUBCLASSEDFOLDERVIEW   psfv = WinQueryWindowPtr(hwndFrame, QWL_USER);

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
                        HWND hwndMainClient;
                        PFILEDLGDATA pWinData;
                        WPObject *pobj;

                        if (    (pnre = (PNOTIFYRECORDENTER)mp2)
                             && (prec = (PMINIRECORDCORE)pnre->pRecord)
                                        // can be null for whitespace!
                             && (hwndMainClient = WinQueryWindow(hwndFrame, QW_OWNER))
                             && (pWinData = WinQueryWindowPtr(hwndMainClient, QWL_USER))
                             && (pobj = GetFSFromRecord(prec, FALSE))
                           )
                        {
                            CHAR szFullFile[CCHMAXPATH];
                            if (_wpQueryFilename(pobj,
                                                 szFullFile,
                                                 TRUE))     // fully q'fied
                            {
                                ParseAndUpdate(pWinData,
                                               szFullFile);
                            }
                        }
                    break; }

                    /*
                     * CN_EMPHASIS:
                     *      selection changed:
                     */

                    case CN_EMPHASIS:
                    {
                        PNOTIFYRECORDEMPHASIS pnre = (PNOTIFYRECORDEMPHASIS)mp2;
                        WPObject *pobj;
                        CHAR szFilename[CCHMAXPATH];
                        HWND hwndMainClient;
                        PFILEDLGDATA pWinData;
                        // if it's not a folder, update the entry field
                        // with the file's name:
                        if (    (pnre->pRecord)
                             && (pnre->fEmphasisMask & CRA_SELECTED)
                             && (((PMINIRECORDCORE)(pnre->pRecord))->flRecordAttr & CRA_SELECTED)
                             && (pobj = GetFSFromRecord((PMINIRECORDCORE)(pnre->pRecord),
                                                        FALSE))
                             && (!_somIsA(pobj, _WPFolder))
                             && (_wpQueryFilename(pobj, szFilename, FALSE))
                             && (hwndMainClient = WinQueryWindow(hwndFrame, QW_OWNER))
                             && (pWinData = WinQueryWindowPtr(hwndMainClient, QWL_USER))
                           )
                        {
                            strcpy(pWinData->szFileName, szFilename);
                            WinSetWindowText(pWinData->hwndFileEntry, szFilename);
                        }
                    break; }

                    default:
                        fCallDefault = TRUE;
                }
            }
            else
                fCallDefault = TRUE;
        break; }

        case WM_CHAR:
            // forward to main client
            WinPostMsg(WinQueryWindow(hwndFrame, QW_OWNER),
                       msg,
                       mp1,
                       mp2);
        break;

        case WM_CONTROLPOINTER:
        {
            HWND hwndMainClient = WinQueryWindow(hwndFrame, QW_OWNER);
                                    // the main client
            mrc = (MPARAM)QueryCurrentPointer(hwndMainClient);
        break; }

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

    return (mrc);
}

/* ******************************************************************
 *
 *   File dialog API
 *
 ********************************************************************/

/*
 *@@ CreateFrameWithCnr:
 *
 */

HWND CreateFrameWithCnr(ULONG ulFrameID,
                        PFILEDLGDATA pWinData,
                        HWND *phwndClient)          // out: client window
{
    HWND hwndFrame = winhCreateStdWindow(pWinData->hwndMainClient, // parent
                                         NULL,          // pswpFrame
                                         FCF_NOBYTEALIGN,
                                         WS_VISIBLE,
                                         "",
                                         0,             // resources ID
                                         WC_CONTAINER,  // client
                                         WS_VISIBLE | WS_SYNCPAINT
                                            | CCS_AUTOPOSITION
                                            | CCS_MINIRECORDCORE
                                            | CCS_MINIICONS,
                                            // | CCS_SINGLESEL,
                                         ulFrameID,
                                         NULL,
                                         phwndClient);
    // set client as owner
    WinSetOwner(hwndFrame, pWinData->hwndMainClient);

    return (hwndFrame);
}

/*
 *@@ fdlgFileDlg:
 *      main entry point into all this mess. This is supposed
 *      to be 100% compatibly with WinFileDlg, but does something
 *      completely different -- it displays a file dialog with full
 *      WPS support, including shadows, WPS context menus, and all
 *      that.
 *
 *      See fnwpMainClient for the (complex) window hierarchy.
 *
 *      If we detect parameters that we cannot handle, we call
 *      the standard WinFileDlg instead. This will happen especially
 *      if
 *
 *      --  a parent other than hwndParent is specified; we cannot
 *          handle that because we are running in the WPS process.
 *
 *      --  pfd is NULL; I am unsure what the default behavior is,
 *          so we call WinFileDlg, which will probably just crash.
 *
 *      --  if a non-standard (old?) size of FILEDLG.cbSize is
 *          specified;
 *
 *      --  if either FDS_CUSTOM or FDS_MODELESS or the
 *          pfnDlgProc field is set. We cannot handle subclassing,
 *          custom dialogs, or modeless operation.
 *
 *      As with WinFileDlg, this returns a HWND for modal operation
 *      (which will the HWND returned from the standard WinFileDlg
 *      call) or (HWND)TRUE or FALSE for non-modal operation.
 */

HWND fdlgFileDlg(HWND hwndParent,
                 HWND hwndOwner,
                 PFILEDLG pfd)
{
    HWND    hwndMain = NULLHANDLE;
    static  s_fRegistered = FALSE;

    // first, some compatibility checks...
    if (    (hwndParent != HWND_DESKTOP)
         || (!pfd)
         || (pfd->cbSize != sizeof(FILEDLG))
         || (pfd->fl & (FDS_CUSTOM | FDS_MODELESS))
         || (pfd->pfnDlgProc)
       )
        hwndMain = WinFileDlg(hwndParent, hwndOwner, pfd);
    else
    {
        // OK, it seems that we can handle this

        // create left frame
        FILEDLGDATA WinData = {0};

        CHAR        szCurDir[CCHMAXPATH] = "";
        ULONG       cbCurDir;
        APIRET      arc;

        lstInit(&WinData.llDriveObjectsInserted, FALSE);
        lstInit(&WinData.llFileObjectsInserted, FALSE);
        lstInit(&WinData.llDisks, FALSE);

        // standard return code: DID_CANCEL
        WinData.ulReturnCode = DID_CANCEL;

        // set wait pointer, since this may take a second
        winhSetWaitPointer();

        /*
         *  PATH/FILE MASK SETUP
         *
         */

        // OK, here's the trick. We first call ParseFile
        // string with the current directory plus the "*"
        // file mask; we then call it a second time with
        // full path string given to us in FILEDLG.
        doshQueryCurrentDir(szCurDir);

        strcat(szCurDir, "\\*");
        ParseFileString(&WinData,
                        szCurDir);

        ParseFileString(&WinData,
                        pfd->szFullFile);

        /*
         *  WINDOW CREATION
         *
         */

        if (!s_fRegistered)
        {
            HWND hwnd = winhCreateObjectWindow(WC_BUTTON, NULL);
            HAB hab = WinQueryAnchorBlock(hwnd);
            WinRegisterClass(hab,
                             (PSZ)WC_FILEDLGCLIENT,
                             fnwpMainClient,
                             CS_CLIPCHILDREN | CS_SIZEREDRAW,
                             sizeof(PFILEDLGDATA));
            WinDestroyWindow(hwnd);
            s_fRegistered = TRUE;
        }

        // create main frame and client
        WinData.hwndMainFrame = winhCreateStdWindow(HWND_DESKTOP,   // parent
                                                    NULL,
                                                    FCF_NOBYTEALIGN
                                                      | FCF_TITLEBAR
                                                      | FCF_SYSMENU
                                                      | FCF_MAXBUTTON
                                                      | FCF_SIZEBORDER
                                                      | FCF_AUTOICON,
                                                    0,  // frame style, not visible yet
                                                    "File", // @@todo localize
                                                    0,  // resids
                                                    WC_FILEDLGCLIENT,
                                                    WS_VISIBLE, // client style
                                                    0,  // frame ID
                                                    &WinData,
                                                    &WinData.hwndMainClient);
        if (!WinData.hwndMainFrame || !WinData.hwndMainClient)
            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                    "Cannot create main window.");
        else
        {
            SPLITBARCDATA sbcd;
            HAB hab = WinQueryAnchorBlock(WinData.hwndMainFrame);
            QMSG qmsg;

            // create two subframes to be linked in split window

            // 1) left: drives tree
            WinData.hwndDrivesFrame = CreateFrameWithCnr(ID_TREEFRAME,
                                                         &WinData,
                                                         &WinData.hwndDrivesCnr);
            BEGIN_CNRINFO()
            {
                cnrhSetView(   CV_TREE | CA_TREELINE | CV_ICON
                             | CV_MINI);
                cnrhSetTreeIndent(30);
            } END_CNRINFO(WinData.hwndDrivesCnr);

            // 2) right: files
            WinData.hwndFilesFrame = CreateFrameWithCnr(ID_FILESFRAME,
                                                        &WinData,
                                                        &WinData.hwndFilesCnr);
            BEGIN_CNRINFO()
            {
                cnrhSetView(   CV_NAME | CV_FLOW
                             | CV_MINI);
                cnrhSetTreeIndent(30);
            } END_CNRINFO(WinData.hwndFilesCnr);


            // 3) fonts
            winhSetWindowFont(WinData.hwndDrivesCnr,
                              cmnQueryDefaultFont());
            winhSetWindowFont(WinData.hwndFilesCnr,
                              cmnQueryDefaultFont());

            // create split window
            sbcd.ulSplitWindowID = 1;
                // split window becomes client of main frame
            sbcd.ulCreateFlags =   SBCF_VERTICAL
                                 | SBCF_PERCENTAGE
                                 | SBCF_3DSUNK
                                 | SBCF_MOVEABLE;
            sbcd.lPos = 30;     // percent
            sbcd.ulLeftOrBottomLimit = 100;
            sbcd.ulRightOrTopLimit = 100;
            sbcd.hwndParentAndOwner = WinData.hwndMainClient;

            WinData.hwndSplitWindow = ctlCreateSplitWindow(hab,
                                                           &sbcd);
            if (!WinData.hwndSplitWindow)
                cmnLog(__FILE__, __LINE__, __FUNCTION__,
                        "Cannot create split window.");
            else
            {
                // link left and right container
                WinSendMsg(WinData.hwndSplitWindow,
                           SPLM_SETLINKS,
                           (MPARAM)WinData.hwndDrivesFrame,
                           (MPARAM)WinData.hwndFilesFrame);

                // show main dlg now
                WinSetWindowPos(WinData.hwndMainFrame,
                                HWND_TOP,
                                100, 100, 600, 600,
                                SWP_MOVE | SWP_SIZE | SWP_SHOW | SWP_ACTIVATE | SWP_ZORDER);

                /*
                 *  FILL DRIVES TREE
                 *
                 */

                // find the folder whose contents to
                // display in the left tree
                WinData.pDrivesFolder = _wpclsQueryFolder(_WPFolder,
                                                          "<WP_DRIVES>",
                                                          TRUE);
                if (    (!WinData.pDrivesFolder)
                     || (!_somIsA(WinData.pDrivesFolder, _WPFolder))
                   )
                    cmnLog(__FILE__, __LINE__, __FUNCTION__,
                           "Cannot get drives folder.");
                else
                {
                    PLISTNODE pNode;
                    BOOL fExit = FALSE;

                    BuildDisksList(WinData.pDrivesFolder,
                                   &WinData.llDisks);

                    // insert the folder contents into the
                    // drives tree on the left... this is
                    // relatively fast, so we do this
                    // synchronously
                    InsertFolderContents(WinData.hwndMainClient,
                                         WinData.hwndDrivesCnr,
                                         WinData.pDrivesFolder,
                                         NULL,      // parent record
                                         TRUE,      // folders only
                                         NULL,      // no file mask
                                         &WinData.llDriveObjectsInserted,
                                         NULL,
                                         &fExit);

                    // this has called wpCnrinsertObjects which
                    // subclasses the container owner, so
                    // subclass this with the XFolder subclass
                    // proc again; otherwise the new menu items
                    // won't work
                    WinData.psfvDrives
                        = fdrCreateSFV(WinData.hwndDrivesFrame,
                                       WinData.hwndDrivesCnr,
                                       QWL_USER,
                                       WinData.pDrivesFolder,
                                       WinData.pDrivesFolder);
                    WinData.psfvDrives->pfnwpOriginal
                        = WinSubclassWindow(WinData.hwndDrivesFrame,
                                            fnwpSubclassedDrivesFrame);

                    if (WinData.psfvDrives->pfnwpOriginal)
                    {
                        // OK, drives frame subclassed:

                        PLINKLIST   pllToExpand = lstCreate(FALSE);

                        // expand the tree so that the current
                        // directory/file mask etc. is properly
                        // expanded
                        UpdateDlgWithFullFile(&WinData,
                                              pllToExpand);
                                // this will expand the tree and
                                // select the current directory;
                                // pllToExpand receives records to
                                // add children to

                        // start "add first child" thread which will
                        // insert sub-records for each item in the drives
                        // tree
                        if (!StartAddChildren(&WinData,
                                               pllToExpand))
                            lstFree(pllToExpand);

                        WinData.fFileDlgReady = TRUE;

                        /*
                         *  PM MSG LOOP
                         *
                         */

                        // standard PM message loop... we stay in here
                        // (simulating a modal dialog) until WM_CLOSE
                        // comes in for the main client
                        while (WinGetMsg(hab, &qmsg, NULLHANDLE, 0, 0))
                        {
                            fExit = FALSE;
                            if (    (qmsg.hwnd == WinData.hwndMainClient)
                                 && (qmsg.msg == WM_CLOSE)
                               )
                                fExit = TRUE;

                            WinDispatchMsg(hab, &qmsg);

                            if (fExit)
                            {
                                pfd->lReturn = WinData.ulReturnCode;
                                        // DID_OK or DID_CANCEL
                                hwndMain = (HWND)TRUE;
                                break;
                            }
                        }
                    } // if (WinData.psfvDrives->pfnwpOriginal)

                    // stop threads
                    WinData.tiAddChildren.fExit = TRUE;
                    WinData.tiInsertContents.fExit = TRUE;
                    while (    (WinData.fAddChildrenRunning)
                            || (WinData.fInsertContentsRunning)
                          )
                    {
                        winhSleep(300);
                    }

                    /*
                     *  CLEANUP
                     *
                     */

                    // prevent dialog updates
                    WinData.fFileDlgReady = FALSE;
                    ClearContainer(WinData.hwndDrivesCnr,
                                   &WinData.llDriveObjectsInserted);
                    ClearContainer(WinData.hwndFilesCnr,
                                   &WinData.llFileObjectsInserted);

                } // end else if (    (!WinData.pDrivesFolder)

                if (WinData.pDrivesFolder)
                    _wpUnlockObject(WinData.pDrivesFolder);

                // clean up
                WinDestroyWindow(WinData.hwndSplitWindow);
            } // end else if (!WinData.hwndSplitWindow)

            WinDestroyWindow(WinData.hwndDrivesFrame);
            WinDestroyWindow(WinData.hwndFilesFrame);
            WinDestroyWindow(WinData.hwndMainFrame);
        }

        lstClear(&WinData.llDisks);
    }

    DosBeep(100, 100);

    return (hwndMain);
}

