
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
 *@@header "filesys\filedlg.h"
 */

/*
 *      Copyright (C) 2001-2002 Ulrich M”ller.
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
#define INCL_WINFRAMEMGR
#define INCL_WINMESSAGEMGR
#define INCL_WININPUT
#define INCL_WINTIMER
#define INCL_WINPOINTERS
#define INCL_WINDIALOGS
#define INCL_WINBUTTONS
#define INCL_WINSTATICS
#define INCL_WINENTRYFIELDS
#define INCL_WINLISTBOXES
#define INCL_WINSTDFILE
#define INCL_WINSTDCNR
#define INCL_WINSHELLDATA       // Prf* functions
#define INCL_WINSYS

#define INCL_GPILOGCOLORTABLE
#define INCL_GPIBITMAPS
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
#include "helpers\dialog.h"             // dialog helpers
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\nls.h"                // National Language Support helpers
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\standards.h"          // some standard macros
#include "helpers\stringh.h"            // string helper routines
#include "helpers\threads.h"            // thread helpers
#include "helpers\winh.h"               // PM helper routines
#include "helpers\xstring.h"            // extended string helpers

// SOM headers which don't crash with prec. header files
#include "xfldr.ih"
#include "xfdisk.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\cnrsort.h"             // container sort comparison functions
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel

#include "filesys\filedlg.h"            // replacement file dialog implementation
#include "filesys\folder.h"             // XFolder implementation
#include "filesys\object.h"             // XFldObject implementation

// other SOM headers
#pragma hdrstop                         // VAC++ keeps crashing otherwise

/* ******************************************************************
 *
 *   Declarations
 *
 ********************************************************************/

const char *WC_FILEDLGCLIENT    = "XWPFileDlgClient";

#define IDDI_TYPESTXT           100
#define IDDI_TYPESCOMBO         101
                // directory statics:
#define IDDI_DIRTXT             102
#define IDDI_DIRVALUE           103
                // file static/entryfield:
#define IDDI_FILETXT            104
#define IDDI_FILEENTRY          105

/*
 *@@ FILEDLGDATA:
 *      central "window data" structure allocated on
 *      the stack in fdlgFileDlg. The main frame and
 *      the main client receive pointers to this
 *      and store them in QWL_USER.
 *
 *      This pretty much holds all the data the
 *      file dlg needs while it is running.
 *
 *@@changed V0.9.16 (2001-10-19) [umoeller]: added UNC support
 */

typedef struct _FILEDLGDATA
{
    // ptr to original FILEDLG structure passed into fdlgFileDlg
    PFILEDLG        pfd;

    FDRSPLITVIEW    sv;

    LINKLIST        llDisks;            // linked list of all WPDisk* objects
                                        // so that we can quickly find them for updating
                                        // the dialog; no auto-free

    // controls in hwndMainControl

    HWND
                    hwndTreeCnrTxt,     // child of sv.hwndMainControl (static text above cnr)
                    hwndFilesCnrTxt,    // child of sv.hwndMainControl (static text above cnr)
                    // types combo:
                    hwndTypesTxt,
                    hwndTypesCombo,
                    // directory statics:
                    hwndDirTxt,
                    hwndDirValue,
                    // file static/entryfield:
                    hwndFileTxt,
                    hwndFileEntry,
                    // buttons:
                    hwndOK,
                    hwndCancel,
                    hwndHelp;

    LINKLIST        llDialogControls;       // list of dialog controls for focus etc.

    // full file name etc., parsed and set by ParseFileString()
    CHAR        szDrive[CCHMAXPATH];        // e.g. "C:" if local drive,
                                            // or "\\SERVER\RESOURCE" if UNC
    BOOL        fUNCDrive;                  // TRUE if szDrive specifies something UNC
    CHAR        szDir[CCHMAXPATH],          // e.g. "\whatever\subdir"
                szFileMask[CCHMAXPATH],     // e.g. "*.TXT"
                szFileName[CCHMAXPATH];     // e.g. "test.txt"

} FILEDLGDATA, *PFILEDLGDATA;

MRESULT EXPENTRY fnwpSubclassedFilesFrame(HWND hwndFrame, ULONG msg, MPARAM mp1, MPARAM mp2);

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
 *      -- FFL_DRIVE: drive (or UNC resource) was specified,
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
 *
 *@@changed V0.9.18 (2002-02-06) [umoeller]: fixed a bunch of bugs
 *@@changed V0.9.19 (2002-06-15) [umoeller]: fixed broken file detection (double-clicks)
 */

static ULONG ParseFileString(PFILEDLGDATA pWinData,
                             const char *pcszFullFile)
{
    ULONG ulChanged = 0;

    const char  *p = pcszFullFile;
    ULONG       ulDriveSpecLen = 0;
    APIRET      arc2;

    if (    !(pcszFullFile)
         || !(*pcszFullFile)
       )
        return 0;

    _PmpfF(("parsing %s", pcszFullFile));

    if (!(arc2 = doshGetDriveSpec(pcszFullFile,
                                  pWinData->szDrive,
                                  &ulDriveSpecLen,
                                  &pWinData->fUNCDrive)))
    {
        // drive specified (local or UNC):
        p += ulDriveSpecLen,
        ulChanged |= FFL_DRIVE;
    }

    _PmpfF(("  doshGetDriveSpec returned %d, len: %d",
            arc2, ulDriveSpecLen));

    if (    (arc2)
            // continue if no drive spec given
         && (arc2 != ERROR_INVALID_PARAMETER)
       )
        // some error was detected:
        return 0;

    // get path from there
    if (p && *p)
    {
        BOOL    fMustBeDir = FALSE; // V0.9.18 (2002-02-06) [umoeller]

        // p3 = last backslash
        PCSZ    pStartOfFile;
        PCSZ    p3;
        if (p3 = strrchr(p, '\\'))
        {
            // path specified:
            // @@todo handle relative paths here
            ULONG cb = p3 - p;

            _PmpfF(("  checking if path \"%s\" is dir", p));

            // check if the last character is a '\';
            // the spec _must_ be a directory then
            if (p[cb] == '\0')      // fixed V0.9.19 (2002-06-15) [umoeller]
            {
                _PmpfF(("  ends in \\, must be dir"));
                fMustBeDir = TRUE;
                // p2++;           // points to null char now --> no file spec
            }

            strhncpy0(pWinData->szDir,
                      p,        // start: either first char or after drive
                      cb);
            _PmpfF(("  got path %s", pWinData->szDir));
            ulChanged |= FFL_PATH;
            pStartOfFile = p3 + 1;      // after the last backslash
        }
        else
            // no path specified:
            pStartOfFile = p;

        _PmpfF(("  pStartOfFile is \"%s\"", pStartOfFile));

        // check if the following is a file mask
        // or a real file name
        if (    (strchr(pStartOfFile, '*'))
             || (strchr(pStartOfFile, '?'))
           )
        {
            // get file name (mask) after that
            strcpy(pWinData->szFileMask,
                   pStartOfFile);

            _PmpfF(("  new mask is %s", pWinData->szFileMask));
            ulChanged |= FFL_FILEMASK;
        }
        else
        {
            // name only:

            // compose full file name
            CHAR szFull[CCHMAXPATH];
            FILESTATUS3 fs3;
            BOOL fIsDir = FALSE;
            PSZ pszThis = szFull;

            pszThis += sprintf(pszThis,
                               "%s%s",
                               pWinData->szDrive,      // either C: or \\SERVER\RESOURCE
                               pWinData->szDir);
            if (*pStartOfFile)
                // we have a file spec left:
                pszThis += sprintf(pszThis,
                                   "\\%s",
                                   pStartOfFile);        // entry

            _PmpfF(("   checking %s", szFull));
            if (!(arc2 = DosQueryPathInfo(szFull,
                                          FIL_STANDARD,
                                          &fs3,
                                          sizeof(fs3))))
            {
                // this thing exists:
                // is it a file or a directory?
                if (fs3.attrFile & FILE_DIRECTORY)
                    fIsDir = TRUE;
            }

            _PmpfF(("   DosQueryPathInfo returned %d, fIsDir is %d",
                        arc2, fIsDir));

            if (fIsDir)
            {
                // user specified directory:
                // append to existing and say "path changed"
                if (*pStartOfFile)
                {
                    strcat(pWinData->szDir, "\\");
                    strcat(pWinData->szDir, pStartOfFile);
                }
                _PmpfF(("  new path is %s", pWinData->szDir));
                ulChanged |= FFL_PATH;
            }
            else
            {
                // this is not a directory:
                if (fMustBeDir)
                    // but it must be (because user termianted string with "\"):
                    _PmpfF(("  not dir, but must be!"));
                else
                {
                    // this doesn't exist, or it is a file:
                    if (*pStartOfFile)
                    {
                        // and it has a length: V0.9.18 (2002-02-06) [umoeller]
                        strcpy(pWinData->szFileName,
                               pStartOfFile);
                        _PmpfF(("  new filename is %s", pWinData->szFileName));
                        ulChanged |= FFL_FILENAME;
                    }
                }
            }
        }
    }

    return (ulChanged);
}

/*
 *@@ UpdateDlgWithFullFile:
 *      updates the dialog according to the
 *      current directory/path/file fields
 *      in pWinData.
 *
 *      Returns TRUE if we already initiated
 *      the full populate of a folder.
 *
 *@@changed V0.9.18 (2002-02-06) [umoeller]: mostly rewritten for better thread synchronization
 */

static BOOL UpdateDlgWithFullFile(PFILEDLGDATA pWinData)
{
    PMINIRECORDCORE precDiskSelect = NULL;
    WPFolder        *pRootFolder = NULL;
    BOOL            brc = FALSE;

    if (pWinData->fUNCDrive)
    {
         // @@todo
    }
    else
    {
        // we currently have a local drive:
        // go thru the disks list and find the WPDisk*
        // which matches the current logical drive
        PLISTNODE pNode = lstQueryFirstNode(&pWinData->llDisks);

        _PmpfF(("pWinData->szDrive = %s",
               pWinData->szDrive));

        while (pNode)
        {
            WPDisk *pDisk = (WPDisk*)pNode->pItemData;

            if (_wpQueryLogicalDrive(pDisk) == pWinData->szDrive[0] - 'A' + 1)
            {
                precDiskSelect = _wpQueryCoreRecord(pDisk);
                pRootFolder = _xwpSafeQueryRootFolder(pDisk, FALSE, NULL);
                break;
            }

            pNode = pNode->pNext;
        }

        _PmpfF(("    precDisk = 0x%lX", precDiskSelect));
    }

    if ((precDiskSelect) && (pRootFolder))
    {
        // we got a valid disk and root folder:
        WPFolder *pFullFolder;

        CHAR szFull[CCHMAXPATH];

        // populate and expand the current disk
        fdrPostFillFolder(&pWinData->sv,
                          precDiskSelect,
                          FFL_FOLDERSONLY | FFL_EXPAND);

        sprintf(szFull,
                "%s%s",
                pWinData->szDrive,      // C: or \\SERVER\RESOURCE
                pWinData->szDir);
        pFullFolder = _wpclsQueryFolder(_WPFolder, szFull, TRUE);
                    // this also awakes all folders in between
                    // the root folder and the full folder

        // now go for all the path particles, starting with the
        // root folder
        if (pFullFolder)
        {
            CHAR szComponent[CCHMAXPATH];
            PMINIRECORDCORE precParent = precDiskSelect; // start with disk
            WPFolder *pFdrThis = pRootFolder;
            const char *pcThis = &pWinData->szDir[1];   // start after root '\'

            _PmpfF(("    got folder %s for %s",
                    _wpQueryTitle(pFullFolder),
                    szFull));

            while (    (pFdrThis)
                    && (*pcThis)
                  )
            {
                const char *pBacksl = strchr(pcThis, '\\');
                WPFileSystem *pobj;

                _PmpfF(("       remaining: %s", pcThis));

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
                // e.g. if szDir was "F:\OS2\BOOK", we now have "OS2"

                _PmpfF(("       checking component %s", szComponent));

                // find this component in pFdrThis
                if (    (pobj = fdrSafeFindFSFromName(pFdrThis,
                                                      szComponent))
                     && (_somIsA(pobj, _WPFolder))
                   )
                {
                    // got that folder:
                    POINTL ptlIcon = {0, 0};
                    PMINIRECORDCORE pNew;
                    ULONG fl;

                    _PmpfF(("        -> got %s", _wpQueryTitle(pobj)));

                    pNew = _wpCnrInsertObject(pobj,
                                              pWinData->sv.hwndTreeCnr,
                                              &ptlIcon,
                                              precParent,  // parent == previous folder
                                              NULL); // next available position

                    if (!pBacksl)
                    {
                        // this was the last component: then
                        // populate fully and scroll to the thing
                        fl = FFL_SCROLLTO;
                        precDiskSelect = NULL;
                        // tell caller we fully populated
                        brc = TRUE;
                    }
                    else
                    {
                        // not the last componend:
                        // then we'll need to expand the thing
                        // and add the first child to each subfolder
                        fl = FFL_FOLDERSONLY | FFL_EXPAND;
                    }

                    if (pNew)
                    {
                        fdrPostFillFolder(&pWinData->sv,
                                          pNew,
                                          fl);

                        precParent = pNew;
                        // precDiskSelect = precParent;
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

    if (precDiskSelect)
    {
        // this only happens if a root drive was selected...
        // then we still need to fully populate and scroll:
        fdrPostFillFolder(&pWinData->sv,
                          precDiskSelect,
                          FFL_SCROLLTO);
        // tell caller we fully populated
        brc = TRUE;
    }

    _PmpfF(("exiting"));

    return brc;
}

/*
 *@@ ParseAndUpdate:
 *      parses the specified new file or file
 *      mask and updates the display.
 *
 *      Gets called on startup with the initial
 *      directory, file, and file mask passed to
 *      the dialog, and later whenever the user
 *      enters something in the entry field.
 *
 *      If pcszFullFile contains a full file name,
 *      we post WM_CLOSE to the dialog's client
 *      to dismiss the dialog.
 */

static VOID ParseAndUpdate(PFILEDLGDATA pWinData,
                           const char *pcszFullFile)
{
    // parse the new file string
    ULONG fl = ParseFileString(pWinData,
                               pcszFullFile);

    BOOL fAlreadyFull = FALSE;

    if (fl & FFL_FILENAME)
    {
        // no wildcard, but file specified:
        pWinData->pfd->lReturn = DID_OK;
        WinPostMsg(pWinData->sv.hwndMainControl, WM_CLOSE, 0, 0);
                // main msg loop detects that
        // get outta here
        return;
    }

    if (fl & (FFL_DRIVE | FFL_PATH | FFL_FILEMASK))
    {
        // set this to NULL so that main control will refresh
        pWinData->sv.precFolderContentsShowing = NULL;
        // drive or path specified:
        // expand that
        fAlreadyFull = UpdateDlgWithFullFile(pWinData);
    }
}

/*
 *@@ BuildDisksList:
 *      builds a linked list of all WPDisk* objects
 *      in pRootFolder, which better be the real
 *      "Drives" folder.
 */

static VOID BuildDisksList(WPFolder *pRootFolder,
                           PLINKLIST pllDisks)
{
    if (fdrCheckIfPopulated(pRootFolder,
                            FALSE))     // folders only?
    {
        BOOL fFolderLocked = FALSE;

        TRY_LOUD(excpt1)
        {
            if (fFolderLocked = !_wpRequestFolderMutexSem(pRootFolder, SEM_INDEFINITE_WAIT))
            {
                WPObject *pObject;
                // 1) count objects
                // V0.9.20 (2002-07-31) [umoeller]: now using get_pobjNext SOM attribute
                for (   pObject = _wpQueryContent(pRootFolder, NULL, QC_FIRST);
                        (pObject);
                        pObject = *__get_pobjNext(pObject)
                    )
                {
                    if (_somIsA(pObject, _WPDisk))
                        lstAppendItem(pllDisks, pObject);
                }
            }
        }
        CATCH(excpt1) {} END_CATCH();

        if (fFolderLocked)
            _wpReleaseFolderMutexSem(pRootFolder);
    }
}

/* ******************************************************************
 *
 *   File dialog window procs
 *
 ********************************************************************/

/*
 *@@ MainControlChar:
 *      implementation for WM_CHAR in fnwpMainControl.
 *
 *@@added V0.9.9 (2001-03-13) [umoeller]
 */

static MRESULT MainControlChar(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    BOOL brc = FALSE;               // not processed
    PFILEDLGDATA pWinData = WinQueryWindowPtr(hwnd, QWL_USER);

    if (pWinData)
    {
        USHORT usFlags    = SHORT1FROMMP(mp1);
        USHORT usch       = SHORT1FROMMP(mp2);
        USHORT usvk       = SHORT2FROMMP(mp2);

        // key down msg?
        if ((usFlags & KC_KEYUP) == 0)
        {
            _PmpfF(("usFlags = 0x%lX, usch = %d, usvk = %d",
                        usFlags, usch, usvk));

            if (usFlags & KC_VIRTUALKEY)
            {
                switch (usvk)
                {
                    case VK_TAB:
                        // find next focus window
                        dlghSetNextFocus(&pWinData->llDialogControls);
                    break;

                    case VK_BACKTAB:
                        // note: shift+tab produces this!!
                        dlghSetPrevFocus(&pWinData->llDialogControls);
                    break;

                    case VK_BACKSPACE:
                    {
                        // get current selection in drives view
                        PMINIRECORDCORE prec = (PMINIRECORDCORE)WinSendMsg(
                                                pWinData->sv.hwndTreeCnr,
                                                CM_QUERYRECORD,
                                                (MPARAM)pWinData->sv.precFolderContentsShowing,
                                                MPFROM2SHORT(CMA_PARENT,
                                                             CMA_ITEMORDER));
                        if (prec)
                            // not at root already:
                            cnrhSelectRecord(pWinData->sv.hwndTreeCnr,
                                             prec,
                                             TRUE);
                    }
                    break;

                    case VK_ESC:
                        WinPostMsg(hwnd,
                                   WM_COMMAND,
                                   (MPARAM)DID_CANCEL,
                                   0);
                    break;

                    case VK_NEWLINE:        // this comes from the main key
                    case VK_ENTER:          // this comes from the numeric keypad
                        dlghEnter(&pWinData->llDialogControls);
                    break;
                } // end switch
            } // end if (usFlags & KC_VIRTUALKEY)
            else
            {
                // no virtual key:
                // find the control for the keypress
                dlghProcessMnemonic(&pWinData->llDialogControls, usch);
            }
        }
    }

    return ((MPARAM)brc);
}

/*
 *@@ MainControlRepositionControls:
 *      part of the implementation of WM_SIZE in
 *      fnwpMainControl. This resizes all subwindows
 *      of the main client -- the split window and
 *      the controls on bottom.
 *
 *      Repositioning the split window will in turn
 *      cause the two container frames to be adjusted.
 */

static VOID MainControlRepositionControls(HWND hwnd,
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
                                 + 3*STATICS_HEIGHT + 3*OUTER_SPACING)
    SWP     aswp[12];       // three buttons
                            // plus two for types
                            // plus two statics for directory
                            // plus two controls for file
                            // plus split window
    ULONG   i = 0;

    memset(aswp, 0, sizeof(aswp));

    // "Drives" static on top:
    aswp[i].fl = SWP_MOVE | SWP_SIZE;
    aswp[i].x = OUTER_SPACING;
    aswp[i].y =    SHORT2FROMMP(mp2)     // new win cy
                 - OUTER_SPACING
                 - STATICS_HEIGHT;
    aswp[i].cx =   (SHORT1FROMMP(mp2)    // new win cx
                        - (2 * OUTER_SPACING))
                   / 2;
    aswp[i].cy = STATICS_HEIGHT;
    aswp[i++].hwnd = pWinData->hwndTreeCnrTxt;

    // "Files list" static on top:
    aswp[i].fl = SWP_MOVE | SWP_SIZE;
    aswp[i].x =   OUTER_SPACING
                + (SHORT1FROMMP(mp2)    // new win cx
                        - (2 * OUTER_SPACING))
                   / 2;
    aswp[i].y =    SHORT2FROMMP(mp2)     // new win cy
                 - OUTER_SPACING
                 - STATICS_HEIGHT;
    aswp[i].cx =   (SHORT1FROMMP(mp2)    // new win cx
                        - (2 * OUTER_SPACING))
                   / 2;
    aswp[i].cy = STATICS_HEIGHT;
    aswp[i++].hwnd = pWinData->hwndFilesCnrTxt;

    // split window
    aswp[i].fl = SWP_MOVE | SWP_SIZE;
    aswp[i].x = OUTER_SPACING;
    aswp[i].y = OUTER_SPACING + SPACE_BOTTOM;
    aswp[i].cx =   SHORT1FROMMP(mp2)   // new win cx
                 - 2 * OUTER_SPACING;
    aswp[i].cy = SHORT2FROMMP(mp2)     // new win cy
                 - 3 * OUTER_SPACING
                 - STATICS_HEIGHT
                 - SPACE_BOTTOM;
    aswp[i++].hwnd = pWinData->sv.hwndSplitWindow;

    // "Types:" static
    aswp[i].fl = SWP_MOVE | SWP_SIZE;
    aswp[i].x = OUTER_SPACING;
    aswp[i].y = BUTTON_HEIGHT + 4 * OUTER_SPACING + 2 * STATICS_HEIGHT;
    aswp[i].cx = STATICS_LEFTWIDTH;
    aswp[i].cy = STATICS_HEIGHT;
    aswp[i++].hwnd = pWinData->hwndTypesTxt;

    // types combobox:
    aswp[i].fl = SWP_MOVE | SWP_SIZE | SWP_ZORDER;
    aswp[i].x = 2*OUTER_SPACING + STATICS_LEFTWIDTH;
    aswp[i].y = BUTTON_HEIGHT + 4 * OUTER_SPACING + 2 * STATICS_HEIGHT;
    aswp[i].cx = SHORT1FROMMP(mp2)   // new win cx
                    - (3*OUTER_SPACING + STATICS_LEFTWIDTH);
    aswp[i].cy = STATICS_HEIGHT;
    aswp[i].hwndInsertBehind = HWND_TOP;
    aswp[i++].hwnd = pWinData->hwndTypesCombo;

    // "Directory:" static
    aswp[i].fl = SWP_MOVE | SWP_SIZE;
    aswp[i].x = OUTER_SPACING;
    aswp[i].y = BUTTON_HEIGHT + 3 * OUTER_SPACING + STATICS_HEIGHT;
    aswp[i].cx = STATICS_LEFTWIDTH;
    aswp[i].cy = STATICS_HEIGHT;
    aswp[i++].hwnd = pWinData->hwndDirTxt;

    // directory value static
    aswp[i].fl = SWP_MOVE | SWP_SIZE;
    aswp[i].x = 2*OUTER_SPACING + STATICS_LEFTWIDTH;
    aswp[i].y = BUTTON_HEIGHT + 3 * OUTER_SPACING + STATICS_HEIGHT;
    aswp[i].cx = SHORT1FROMMP(mp2)   // new win cx
                    - (3*OUTER_SPACING + STATICS_LEFTWIDTH);
    aswp[i].cy = STATICS_HEIGHT;
    aswp[i++].hwnd = pWinData->hwndDirValue;

    // "File:" static
    aswp[i].fl = SWP_MOVE | SWP_SIZE;
    aswp[i].x = OUTER_SPACING;
    aswp[i].y = BUTTON_HEIGHT + 2 * OUTER_SPACING;
    aswp[i].cx = STATICS_LEFTWIDTH;
    aswp[i].cy = STATICS_HEIGHT;
    aswp[i++].hwnd = pWinData->hwndFileTxt;

    // file entry field
    aswp[i].fl = SWP_MOVE | SWP_SIZE;
    aswp[i].x = 2*OUTER_SPACING + STATICS_LEFTWIDTH;
    aswp[i].y = BUTTON_HEIGHT + 2 * OUTER_SPACING + 2;
    aswp[i].cx = SHORT1FROMMP(mp2)   // new win cx
                    - (3*OUTER_SPACING + STATICS_LEFTWIDTH);
    aswp[i].cy = STATICS_HEIGHT - 2 * 2;
    aswp[i++].hwnd = pWinData->hwndFileEntry;

    // "OK" button
    aswp[i].fl = SWP_MOVE | SWP_SIZE;
    aswp[i].x = OUTER_SPACING;
    aswp[i].y = OUTER_SPACING;
    aswp[i].cx = BUTTON_WIDTH;
    aswp[i].cy = BUTTON_HEIGHT;
    aswp[i++].hwnd = pWinData->hwndOK;

    // "Cancel" button
    aswp[i].fl = SWP_MOVE | SWP_SIZE;
    aswp[i].x = OUTER_SPACING + BUTTON_SPACING + BUTTON_WIDTH;
    aswp[i].y = OUTER_SPACING;
    aswp[i].cx = BUTTON_WIDTH;
    aswp[i].cy = BUTTON_HEIGHT;
    aswp[i++].hwnd = pWinData->hwndCancel;

    // "Help" button
    if (pWinData->hwndHelp)
    {
        aswp[i].fl = SWP_MOVE | SWP_SIZE;
        aswp[i].x = OUTER_SPACING + 2*BUTTON_SPACING + 2*BUTTON_WIDTH;
        aswp[i].y = OUTER_SPACING;
        aswp[i].cx = BUTTON_WIDTH;
        aswp[i].cy = BUTTON_HEIGHT;
        aswp[i++].hwnd = pWinData->hwndHelp;
    }

    WinSetMultWindowPos(WinQueryAnchorBlock(hwnd),
                        aswp,
                        i);
}

/*
 *@@ MainControlCreate:
 *
 *@@changed V0.9.21 (2002-08-21) [umoeller]: extracted fdrSetupSplitView
 */

static MRESULT MainControlCreate(HWND hwnd,
                                 PFILEDLGDATA pWinData)
{
    MRESULT mrc;

    pWinData->sv.lSplitBarPos = 30;         // percentage of split bar pos

    if (!(mrc = fdrSetupSplitView(hwnd,
                                  !!(pWinData->pfd->fl & FDS_MULTIPLESEL),
                                  &pWinData->sv)))
    {
        // create static on top of left tree view
        pWinData->hwndTreeCnrTxt
            = winhCreateControl(hwnd,           // parent
                                WC_STATIC,
                                cmnGetString(ID_XFSI_FDLG_DRIVES),
                                WS_VISIBLE | SS_TEXT | DT_LEFT | DT_VCENTER | DT_MNEMONIC,
                                IDDI_TYPESTXT);
        lstAppendItem(&pWinData->llDialogControls, (PVOID)pWinData->hwndTreeCnrTxt);

        // append container next (tab order!)
        lstAppendItem(&pWinData->llDialogControls, (PVOID)pWinData->sv.hwndTreeCnr);

        // create static on top of right files cnr
        pWinData->hwndFilesCnrTxt
            = winhCreateControl(hwnd,           // parent
                                WC_STATIC,
                                cmnGetString(ID_XFSI_FDLG_FILESLIST),
                                WS_VISIBLE | SS_TEXT | DT_RIGHT | DT_VCENTER | DT_MNEMONIC,
                                IDDI_TYPESTXT);
        lstAppendItem(&pWinData->llDialogControls, (PVOID)pWinData->hwndFilesCnrTxt);

        // append container next (tab order!)
        lstAppendItem(&pWinData->llDialogControls, (PVOID)pWinData->sv.hwndFilesCnr);

        /*
         *  create the other controls
         *
         */

        pWinData->hwndTypesTxt
            = winhCreateControl(hwnd,           // parent
                                WC_STATIC,
                                cmnGetString(ID_XFSI_FDLG_TYPES),
                                WS_VISIBLE | SS_TEXT | DT_LEFT | DT_VCENTER | DT_MNEMONIC,
                                IDDI_TYPESTXT);
        lstAppendItem(&pWinData->llDialogControls, (PVOID)pWinData->hwndTypesTxt);

        pWinData->hwndTypesCombo
            = winhCreateControl(hwnd,           // parent
                                WC_ENTRYFIELD,
                                "",
                                WS_VISIBLE | ES_LEFT | ES_AUTOSCROLL | ES_MARGIN,
                                IDDI_TYPESCOMBO);
        ctlComboFromEntryField(pWinData->hwndTypesCombo,
                               CBS_DROPDOWNLIST);
        lstAppendItem(&pWinData->llDialogControls, (PVOID)pWinData->hwndTypesCombo);

        pWinData->hwndDirTxt
            = winhCreateControl(hwnd,           // parent
                                WC_STATIC,
                                cmnGetString(ID_XFSI_FDLG_DIRECTORY),
                                WS_VISIBLE | SS_TEXT | DT_LEFT | DT_VCENTER | DT_MNEMONIC,
                                IDDI_DIRTXT);

        pWinData->hwndDirValue
            = winhCreateControl(hwnd,           // parent
                                WC_STATIC,
                                cmnGetString(ID_XFSI_FDLG_WORKING),
                                WS_VISIBLE | SS_TEXT | DT_LEFT | DT_VCENTER,
                                IDDI_DIRVALUE);

        pWinData->hwndFileTxt
            = winhCreateControl(hwnd,           // parent
                                WC_STATIC,
                                cmnGetString(ID_XFSI_FDLG_FILE),
                                WS_VISIBLE | SS_TEXT | DT_LEFT | DT_VCENTER | DT_MNEMONIC,
                                IDDI_FILETXT);
        lstAppendItem(&pWinData->llDialogControls, (PVOID)pWinData->hwndFileTxt);

        pWinData->hwndFileEntry
            = winhCreateControl(hwnd,           // parent
                                WC_ENTRYFIELD,
                                "",             // initial text... we set this later
                                WS_VISIBLE | ES_LEFT | ES_AUTOSCROLL | ES_MARGIN,
                                IDDI_FILEENTRY);
        winhSetEntryFieldLimit(pWinData->hwndFileEntry, CCHMAXPATH - 1);
        lstAppendItem(&pWinData->llDialogControls, (PVOID)pWinData->hwndFileEntry);

        pWinData->hwndOK
            = winhCreateControl(hwnd,           // parent
                                WC_BUTTON,
                                (pWinData->pfd->pszOKButton)
                                  ? pWinData->pfd->pszOKButton
                                  : cmnGetString(DID_OK),
                                WS_VISIBLE | BS_PUSHBUTTON | BS_DEFAULT,
                                DID_OK);
        lstAppendItem(&pWinData->llDialogControls, (PVOID)pWinData->hwndOK);

        pWinData->hwndCancel
            = winhCreateControl(hwnd,           // parent
                                WC_BUTTON,
                                cmnGetString(DID_CANCEL),
                                WS_VISIBLE | BS_PUSHBUTTON,
                                DID_CANCEL);
        lstAppendItem(&pWinData->llDialogControls, (PVOID)pWinData->hwndCancel);

        if (pWinData->pfd->fl & FDS_HELPBUTTON)
        {
            pWinData->hwndHelp
                = winhCreateControl(hwnd,           // parent
                                    WC_BUTTON,
                                    cmnGetString(DID_HELP),
                                    WS_VISIBLE | BS_PUSHBUTTON | BS_HELP,
                                    DID_HELP);
            lstAppendItem(&pWinData->llDialogControls, (PVOID)pWinData->hwndHelp);
        }
    }

    return mrc;
}

/*
 *@@ fnwpMainControl:
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
 +        +--- WC_CLIENT (fnwpMainControl)
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
 *@@changed V0.9.18 (2002-02-06) [umoeller]: largely rewritten for new thread synchronization
 */

static MRESULT EXPENTRY fnwpMainControl(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
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
                mrc = MainControlCreate(hwnd, pWinData);
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
        }
        break;

        /*
         * WM_SIZE:
         *      resize all the controls.
         */

        case WM_SIZE:
        {
            PFILEDLGDATA pWinData;
            if (pWinData = WinQueryWindowPtr(hwnd, QWL_USER))
                MainControlRepositionControls(hwnd, pWinData, mp2);
        }
        break;

        /*
         * WM_COMMAND:
         *      button pressed.
         */

        case WM_COMMAND:
        {
            PFILEDLGDATA pWinData = WinQueryWindowPtr(hwnd, QWL_USER);

            if (pWinData->sv.fFileDlgReady)
                switch ((ULONG)mp1)
                {
                    case DID_OK:
                    {
                        // "OK" is tricky... we first need to
                        // get the thing from the entry field
                        // and check if it contains a wildcard.
                        // For some reason people have become
                        // accustomed to this.
                        PSZ pszFullFile;
                        if (pszFullFile = winhQueryWindowText(pWinData->hwndFileEntry))
                        {
                            ParseAndUpdate(pWinData,
                                           pszFullFile);
                                    // this posts WM_CLOSE if a full file name
                                    // was entered to close the dialog and return
                            free(pszFullFile);
                        }

                    }
                    break;

                    case DID_CANCEL:
                        pWinData->pfd->lReturn = DID_CANCEL;
                        WinPostMsg(hwnd, WM_CLOSE, 0, 0);
                                // main msg loop detects that
                    break;
                }
        }
        break;

        /*
         * WM_CHAR:
         *      this gets forwarded to us from all the
         *      sub-frames and also the controls.
         *      We re-implement a dialog manager here.
         */

        case WM_CHAR:
            mrc = MainControlChar(hwnd, mp1, mp2);
        break;

        /*
         *@@ FM_FILLFOLDER:
         *      posted to the main control to fill
         *      the dialog when a new folder has been
         *      selected in the left drives tree.
         *
         *      This automatically offloads populate
         *      to fntPopulate, which will then post
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
         *          populate the folder with subfolders only
         *          and expand the folder on the left. The
         *          files list is not changed.
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
         *          the record in the drives tree after
         *          populate and run "add first child" for
         *          each subrecord that was inserted.
         *
         *@@added V0.9.18 (2002-02-06) [umoeller]
         */

        case FM_FILLFOLDER:
        {
            PFILEDLGDATA pWinData = WinQueryWindowPtr(hwnd, QWL_USER);
            PMINIRECORDCORE prec = (PMINIRECORDCORE)mp1;
            BOOL        fFolderChanged = FALSE;
            WPFolder    *pFolder;

            _PmpfF(("FM_FILLFOLDER %s, fl 0x%lX",
                        prec->pszIcon,
                        mp2));

            if (0 == ((ULONG)mp2 & FFL_FOLDERSONLY))
                // not folders-only: then we need to
                // refresh the files list
                fdrClearContainer(pWinData->sv.hwndFilesCnr,
                                  &pWinData->sv.llFileObjectsInserted);

            fdrSplitPopulate(&pWinData->sv,
                             prec,
                             (ULONG)mp2);
        }
        break;

        /*
         *@@ FM_POPULATED_FILLTREE:
         *      posted by fntPopulate after populate has been
         *      done for a folder. This gets posted in any case,
         *      if the folder was populated in folders-only mode
         *      or not.
         *
         *      Parameters:
         *
         *      --  PMINIRECORDCODE mp1: record of folder
         *          (or disk or whatever) to fill with.
         *
         *      --  ULONG mp2: FFL_* flags for whether to
         *          expand.
         *
         *@@added V0.9.18 (2002-02-06) [umoeller]
         */

        case FM_POPULATED_FILLTREE:

            _PmpfF(("FM_POPULATED_FILLTREE %s",
                        mp1
                            ? ((PMINIRECORDCORE)mp1)->pszIcon
                            : "NULL"));

            if (mp1)
            {
                PMINIRECORDCORE prec = mp1;
                PFILEDLGDATA pWinData = WinQueryWindowPtr(hwnd, QWL_USER);
                WPFolder    *pFolder = fdrGetFSFromRecord(mp1, TRUE);
                PLISTNODE   pNode;
                HWND        hwndAddFirstChild = NULLHANDLE;

                if ((ULONG)mp2 & FFL_EXPAND)
                {
                    BOOL        fOld = pWinData->sv.fFileDlgReady;
                    // stop control notifications from messing with this
                    pWinData->sv.fFileDlgReady = FALSE;
                    cnrhExpandFromRoot(pWinData->sv.hwndTreeCnr,
                                       (PRECORDCORE)prec);
                    // then fire CM_ADDFIRSTCHILD too
                    hwndAddFirstChild = pWinData->sv.hwndSplitPopulate;

                    // re-enable control notifications
                    pWinData->sv.fFileDlgReady = fOld;
                }

                // insert subfolders into tree on the left
                fdrInsertContents(pFolder,
                                  pWinData->sv.hwndTreeCnr,
                                  (PMINIRECORDCORE)mp1,
                                  INSERT_FOLDERSANDDISKS,
                                  hwndAddFirstChild,
                                  NULL,       // file mask
                                  &pWinData->sv.llTreeObjectsInserted);
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
        {
            ULONG ul;
            PFILEDLGDATA pWinData = WinQueryWindowPtr(hwnd, QWL_USER);
            BOOL        fOld = pWinData->sv.fFileDlgReady;

            _PmpfF(("FM_POPULATED_SCROLLTO %s",
                        mp1
                            ? ((PMINIRECORDCORE)mp1)->pszIcon
                            : "NULL"));

            // stop control notifications from messing with this
            pWinData->sv.fFileDlgReady = FALSE;

            /* cnrhExpandFromRoot(pWinData->hwndTreeCnr,
                               (PRECORDCORE)mp1); */
            ul = cnrhScrollToRecord(pWinData->sv.hwndTreeCnr,
                                    (PRECORDCORE)mp1,
                                    CMA_ICON | CMA_TEXT | CMA_TREEICON,
                                    TRUE);       // keep parent
            cnrhSelectRecord(pWinData->sv.hwndTreeCnr,
                             (PRECORDCORE)mp1,
                             TRUE);
            if (ul && ul != 3)
                cmnLog(__FILE__, __LINE__, __FUNCTION__,
                        "Error: cnrhScrollToRecord returned %d", ul);

            // re-enable control notifications
            pWinData->sv.fFileDlgReady = fOld;
        }
        break;

        /*
         *@@ FM_POPULATED_FILLFILES:
         *      posted by fntPopulate after populate has been
         *      done for the newly selected folder, if this
         *      was not in folders-only mode. We must then fill
         *      the right half of the dialog with all the objects.
         *
         *      Parameters:
         *
         *      --  PMINIRECORDCODE mp1: record of folder
         *          (or disk or whatever) to fill with.
         *
         *      --  WPFolder* mp2: folder that was populated
         *          for that record.
         *
         *@@added V0.9.18 (2002-02-06) [umoeller]
         */

        case FM_POPULATED_FILLFILES:

            _PmpfF(("FM_POPULATED_FILLFILES %s",
                        mp1
                            ? ((PMINIRECORDCORE)mp1)->pszIcon
                            : "NULL"));


            if ((mp1) && (mp2))
            {
                PFILEDLGDATA pWinData = WinQueryWindowPtr(hwnd, QWL_USER);
                WPFolder    *pFolder = (WPFolder*)mp2;
                CHAR        szPathName[2*CCHMAXPATH];

                // insert all contents into list on the right
                fdrInsertContents(pFolder,
                                  pWinData->sv.hwndFilesCnr,
                                  NULL,    // parent
                                  INSERT_FILESYSTEMS,
                                  NULLHANDLE,      // no add first child
                                  pWinData->szFileMask,
                                  &pWinData->sv.llFileObjectsInserted);

                // set new "directory" static on bottom
                _wpQueryFilename(pFolder,
                                 szPathName,
                                 TRUE);     // fully q'fied
                strcat(szPathName, "\\");
                strcat(szPathName, pWinData->szFileMask);
                WinSetWindowText(pWinData->hwndDirValue,
                                 szPathName);

                // now, if this is for the files cnr, we must make
                // sure that the container owner (the parent frame)
                // has been subclassed by us... we do this AFTER
                // the WPS subclasses the owner, which happens during
                // record insertion

                if (!pWinData->sv.fFilesFrameSubclassed)
                {
                    // not subclassed yet:
                    // subclass now

                    pWinData->sv.psfvFiles
                        = fdrCreateSFV(pWinData->sv.hwndFilesFrame,
                                       pWinData->sv.hwndFilesCnr,
                                       QWL_USER,
                                       pFolder,
                                       pFolder);
                    pWinData->sv.psfvFiles->pfnwpOriginal
                        = WinSubclassWindow(pWinData->sv.hwndFilesFrame,
                                            fnwpSubclassedFilesFrame);
                    pWinData->sv.fFilesFrameSubclassed = TRUE;
                }
                else
                {
                    // already subclassed:
                    // update the folder pointers in the SFV
                    pWinData->sv.psfvFiles->somSelf = pFolder;
                    pWinData->sv.psfvFiles->pRealObject = pFolder;
                }
            }
        break;

        /*
         * CM_UPDATEPOINTER:
         *      posted when threads exit etc. to update
         *      the current pointer.
         */

        case FM_UPDATEPOINTER:
        {
            PFILEDLGDATA pWinData = WinQueryWindowPtr(hwnd, QWL_USER);
            WinSetPointer(HWND_DESKTOP,
                          fdrSplitQueryPointer(&pWinData->sv));
        }
        break;

        default:
            mrc = WinDefWindowProc(hwnd, msg, mp1, mp2);
    }

    return mrc;
}

/*
 *@@ fnwpSubclassedDrivesFrame:
 *      subclassed frame window on the left for the
 *      "Drives" container.
 *
 *      We use the XFolder subclassed window proc for
 *      most messages. In addition, we intercept a
 *      couple more for extra features.
 *
 *@@changed V0.9.18 (2002-02-06) [umoeller]: many fixes for new threads synchronization
 */

static MRESULT EXPENTRY fnwpSubclassedDrivesFrame(HWND hwndFrame, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT             mrc = 0;
    BOOL                fCallDefault = FALSE;
    PSUBCLFOLDERVIEW    psfv = WinQueryWindowPtr(hwndFrame, QWL_USER);

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
                     * CN_EMPHASIS:
                     *      selection changed:
                     */

                    case CN_EMPHASIS:
                    {
                        HWND            hwndMainControl;
                        PFILEDLGDATA    pWinData;
                        PNOTIFYRECORDEMPHASIS pnre = (PNOTIFYRECORDEMPHASIS)mp2;
                        PMINIRECORDCORE prec;

                        if (    (pnre->pRecord)
                             && (pnre->fEmphasisMask & CRA_SELECTED)
                             && (prec = (PMINIRECORDCORE)pnre->pRecord)
                             && (prec->flRecordAttr & CRA_SELECTED)
                             && (hwndMainControl = WinQueryWindow(hwndFrame, QW_OWNER))
                             && (pWinData = WinQueryWindowPtr(hwndMainControl, QWL_USER))
                             // notifications not disabled?
                             && (pWinData->sv.fFileDlgReady)
                             // record changed?
                             && (prec != pWinData->sv.precFolderContentsShowing)
                           )
                        {
                            _PmpfF(("CN_EMPHASIS %s",
                                    prec->pszIcon));

                            fdrPostFillFolder(&pWinData->sv,
                                              prec,
                                              0);
                        }
                    }
                    break;

                    /*
                     * CN_EXPANDTREE:
                     *      user clicked on "+" sign next to
                     *      tree item; expand that, but start
                     *      "add first child" thread again
                     */

                    case CN_EXPANDTREE:
                    {
                        HWND            hwndMainControl;
                        PFILEDLGDATA    pWinData;
                        PMINIRECORDCORE prec;

                        if (    (hwndMainControl = WinQueryWindow(hwndFrame, QW_OWNER))
                             && (pWinData = WinQueryWindowPtr(hwndMainControl, QWL_USER))
                             // notifications not disabled?
                             && (pWinData->sv.fFileDlgReady)
                             && (prec = (PMINIRECORDCORE)mp2)
                           )
                        {
                            _PmpfF(("CN_EXPANDTREE %s",
                                    prec->pszIcon));

                            fdrPostFillFolder(&pWinData->sv,
                                              prec,
                                              FFL_FOLDERSONLY | FFL_EXPAND);

                            // and call default because xfolder
                            // handles auto-scroll
                            fCallDefault = TRUE;
                        }
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
                        PMINIRECORDCORE prec;

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

                    default:
                        fCallDefault = TRUE;
                }
            }
            else
                fCallDefault = TRUE;
        }
        break;

        case WM_CHAR:
            // forward to main client
            WinPostMsg(WinQueryWindow(hwndFrame, QW_OWNER),
                       msg,
                       mp1,
                       mp2);
        break;

        case WM_SYSCOMMAND:
            // forward to main frame
            WinPostMsg(WinQueryWindow(WinQueryWindow(hwndFrame, QW_OWNER),
                                      QW_OWNER),
                       msg,
                       mp1,
                       mp2);
        break;

        case WM_CONTROLPOINTER:
        {
            HWND hwndMainControl = WinQueryWindow(hwndFrame, QW_OWNER);
                                    // the main client
            PFILEDLGDATA pWinData = WinQueryWindowPtr(hwndMainControl, QWL_USER);
            mrc = (MPARAM)fdrSplitQueryPointer(&pWinData->sv);
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

    return mrc;
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

static MRESULT EXPENTRY fnwpSubclassedFilesFrame(HWND hwndFrame, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT                 mrc = 0;
    BOOL                    fCallDefault = FALSE;
    PSUBCLFOLDERVIEW        psfv = WinQueryWindowPtr(hwndFrame, QWL_USER);

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
                     * CN_EMPHASIS:
                     *      selection changed:
                     *      if it is a file (and not a folder), update
                     *      windata and the entry field.
                     */

                    case CN_EMPHASIS:
                    {
                        PNOTIFYRECORDEMPHASIS pnre = (PNOTIFYRECORDEMPHASIS)mp2;
                        WPObject *pobj;
                        CHAR szFilename[CCHMAXPATH];
                        HWND hwndMainControl;
                        PFILEDLGDATA pWinData;
                        // if it's not a folder, update the entry field
                        // with the file's name:
                        if (    (pnre->pRecord)
                             && (pnre->fEmphasisMask & CRA_SELECTED)
                             && (((PMINIRECORDCORE)(pnre->pRecord))->flRecordAttr & CRA_SELECTED)
                             // alright, selection changed: get file-system object
                             // from the new record:
                             && (pobj = fdrGetFSFromRecord((PMINIRECORDCORE)(pnre->pRecord),
                                                           FALSE))
                             // do not update if folder:
                             && (!_somIsA(pobj, _WPFolder))
                             // it's a file: get filename
                             && (_wpQueryFilename(pobj, szFilename, FALSE))
                             && (hwndMainControl = WinQueryWindow(hwndFrame, QW_OWNER))
                             && (pWinData = WinQueryWindowPtr(hwndMainControl, QWL_USER))
                           )
                        {
                            // OK, file was selected: update windata
                            strcpy(pWinData->szFileName, szFilename);
                            // update entry field
                            WinSetWindowText(pWinData->hwndFileEntry, szFilename);
                        }
                    }
                    break;

                    case CN_ENTER:
                    {
                        PNOTIFYRECORDENTER pnre;
                        PMINIRECORDCORE prec;
                        HWND hwndMainControl;
                        PFILEDLGDATA pWinData;
                        WPObject *pobj;

                        if (    (pnre = (PNOTIFYRECORDENTER)mp2)
                             && (prec = (PMINIRECORDCORE)pnre->pRecord)
                                        // can be null for whitespace!
                             && (hwndMainControl = WinQueryWindow(hwndFrame, QW_OWNER))
                             && (pWinData = WinQueryWindowPtr(hwndMainControl, QWL_USER))
                             && (pobj = fdrGetFSFromRecord(prec, FALSE))
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
                    }
                    break;

                    default:
                        fCallDefault = TRUE;
                }
            }
            else
                fCallDefault = TRUE;
        }
        break;

        case WM_CHAR:
            // forward to main client
            WinPostMsg(WinQueryWindow(hwndFrame, QW_OWNER),
                       msg,
                       mp1,
                       mp2);
        break;

        case WM_SYSCOMMAND:
            // forward to main frame
            WinPostMsg(WinQueryWindow(WinQueryWindow(hwndFrame, QW_OWNER),
                                      QW_OWNER),
                       msg,
                       mp1,
                       mp2);
        break;

        case WM_CONTROLPOINTER:
        {
            HWND hwndMainControl = WinQueryWindow(hwndFrame, QW_OWNER);
                                    // the main client
            PFILEDLGDATA pWinData = WinQueryWindowPtr(hwndMainControl, QWL_USER);
            mrc = (MPARAM)fdrSplitQueryPointer(&pWinData->sv);
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

    return mrc;
}

/* ******************************************************************
 *
 *   File dialog API
 *
 ********************************************************************/

/*
 *@@ fdlgFileDlg:
 *      main entry point into all this mess. This displays a
 *      file dialog with full WPS support, including shadows,
 *      WPS context menus, and all that.
 *
 *      See fnwpMainControl for the (complex) window hierarchy.
 *
 *      Supported file-dialog flags in FILEDLG.fl:
 *
 *      -- FDS_FILTERUNION: when this flag is set, the dialog
 *         uses the union of the string filter and the
 *         extended-attribute type filter when filtering files
 *         for the Files list box. When this flag is not set,
 *         the list box, by default, uses the intersection of the
 *         two. @@todo
 *
 *      -- FDS_HELPBUTTON: a "Help" button is added which will
 *         post WM_HELP to hwndOwner.
 *
 *      -- FDS_MULTIPLESEL: when this flag is set, the Files container
 *         for the dialog is set to allow multiple selections. When
 *         this flag is not set, CCS_SINGLESEL is enabled. @@todo
 *
 *      -- FDS_OPEN_DIALOG or FDS_SAVEAS_DIALOG: one of the two
 *         should be set.
 *
 *      The following "fl" flags are ignored: FDS_APPLYBUTTON,
 *      FDS_CENTER, FDS_CUSTOM, FDS_ENABLEFILELB, FDS_INCLUDE_EAS,
 *      FDS_MODELESS, FDS_PRELOAD_VOLINFO.
 *
 *      FILEDLG.pszIType, papszITypeList, and sEAType are supported. @@todo
 *
 *      When FDS_MULTIPLESEL has been specified, FILEDLG.papszFQFileName
 *      receives the array of selected file names, as with WinFileDlg,
 *      and FILEDLG.ulFQFileCount receives the count. @@todo
 *
 *      The following other FILEDLG fields are ignored: ulUser,
 *      pfnDlgProc, pszIDrive, pszIDriveList, hMod, usDlgID, x, y.
 */

HWND fdlgFileDlg(HWND hwndOwner,
                 const char *pcszStartupDir,        // in: current directory or NULL
                 PFILEDLG pfd)
{
    HWND    hwndReturn = NULLHANDLE;
    static  s_fRegistered = FALSE;

    // static windata used by all components
    FILEDLGDATA WinData;
    memset(&WinData, 0, sizeof(WinData));

    WinData.pfd = pfd;

    lstInit(&WinData.llDialogControls, FALSE);

    lstInit(&WinData.llDisks, FALSE);

    pfd->lReturn = DID_CANCEL;           // for now

    TRY_LOUD(excpt1)
    {
        CHAR        szCurDir[CCHMAXPATH] = "";
        PSZ         pszDlgTitle;

        ULONG       flInitialParse = 0;

        // set wait pointer, since this may take a second
        winhSetWaitPointer();

        /*
         *  PATH/FILE MASK SETUP
         *
         */

        // OK, here's the trick. We first call ParseFile
        // string with the current directory plus the "*"
        // file mask to make sure all fields are properly
        // initialized;
        // we then call it a second time with
        // full path string given to us in FILEDLG.
        if (pcszStartupDir && *pcszStartupDir)
            strcpy(szCurDir, pcszStartupDir);
        else
            // startup not specified:
            doshQueryCurrentDir(szCurDir);

        if (strlen(szCurDir) > 3)
            strcat(szCurDir, "\\*");
        else
            strcat(szCurDir, "*");
        ParseFileString(&WinData,
                        szCurDir);

        _PmpfF(("pfd->szFullFile is %s", pfd->szFullFile));
        flInitialParse = ParseFileString(&WinData,
                                         pfd->szFullFile);
                            // store the initial parse flags so we
                            // can set the entry field properly below

        /*
         *  WINDOW CREATION
         *
         */

        if (!s_fRegistered)
        {
            // first call: register client class
            HAB hab = winhMyAnchorBlock();
            WinRegisterClass(hab,
                             (PSZ)WC_FILEDLGCLIENT,
                             fnwpMainControl,
                             CS_CLIPCHILDREN | CS_SIZEREDRAW,
                             sizeof(PFILEDLGDATA));
            s_fRegistered = TRUE;
        }

        pszDlgTitle = pfd->pszTitle;
        if (!pszDlgTitle)
            // no user title specified:
            if (pfd->fl & FDS_SAVEAS_DIALOG)
                pszDlgTitle = cmnGetString(ID_XFSI_FDLG_SAVEFILEAS);
            else
                pszDlgTitle = cmnGetString(ID_XFSI_FDLG_OPENFILE);

        // create main frame and client;
        // client's WM_CREATE creates all the controls in turn
        WinData.sv.hwndMainFrame = winhCreateStdWindow(HWND_DESKTOP,   // parent
                                                       NULL,
                                                       FCF_NOBYTEALIGN
                                                         | FCF_TITLEBAR
                                                         | FCF_SYSMENU
                                                         | FCF_MAXBUTTON
                                                         | FCF_SIZEBORDER
                                                         | FCF_AUTOICON,
                                                       0,  // frame style, not visible yet
                                                       pszDlgTitle,    // frame title
                                                       0,  // resids
                                                       WC_FILEDLGCLIENT,
                                                       WS_VISIBLE | WS_SYNCPAINT, // client style
                                                       0,  // frame ID
                                                       &WinData,
                                                       &WinData.sv.hwndMainControl);
        if (!WinData.sv.hwndMainFrame || !WinData.sv.hwndMainControl)
            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                   "Cannot create main window.");
        else
        {
            // set up types combo       WinFileDlg
            if (pfd->papszITypeList)
            {
                ULONG   ul = 0;
                while (TRUE)
                {
                    PSZ     *ppszTypeThis = pfd->papszITypeList[ul];

                    _PmpfF(("pszTypeThis[%d] = %s", ul,
                                        (*ppszTypeThis) ? *ppszTypeThis : "NULL"));

                    if (!*ppszTypeThis)
                        break;

                    WinInsertLboxItem(WinData.hwndTypesCombo,
                                      LIT_SORTASCENDING,
                                      *ppszTypeThis);
                    ul++;
                }
            }

            WinInsertLboxItem(WinData.hwndTypesCombo,
                              0,                // first item always
                              cmnGetString(ID_XFSI_FDLG_ALLTYPES));

            winhSetLboxSelectedItem(WinData.hwndTypesCombo,
                                    0,
                                    TRUE);

            // position dialog now
            if (!winhRestoreWindowPos(WinData.sv.hwndMainFrame,
                                      HINI_USER,
                                      INIAPP_XWORKPLACE,
                                      INIKEY_WNDPOSFILEDLG,
                                      SWP_MOVE | SWP_SIZE)) // no show yet
            {
                // no position stored yet:
                WinSetWindowPos(WinData.sv.hwndMainFrame,
                                HWND_TOP,
                                0, 0, 600, 400,
                                SWP_SIZE);
                winhCenterWindow(WinData.sv.hwndMainFrame);       // still invisible
            }

            WinSetWindowPos(WinData.sv.hwndMainFrame,
                            HWND_TOP,
                            0, 0, 0, 0,
                            SWP_SHOW | SWP_ACTIVATE | SWP_ZORDER);

            /*
             *  FILL DRIVES TREE
             *
             */

            // find the folder whose contents to
            // display in the left tree
            WinData.sv.pRootFolder = _wpclsQueryFolder(_WPFolder,
                                                      (PSZ)WPOBJID_DRIVES,
                                                      TRUE);
            if (    (!WinData.sv.pRootFolder)
                 || (!_somIsA(WinData.sv.pRootFolder, _WPFolder))
               )
                cmnLog(__FILE__, __LINE__, __FUNCTION__,
                       "Cannot get drives folder.");
            else
            {
                BOOL fExit = FALSE;
                PMINIRECORDCORE pDrivesRec;
                POINTL ptlIcon = {0, 0};

                BuildDisksList(WinData.sv.pRootFolder,
                               &WinData.llDisks);

                // insert the drives folder as the root of the tree
                pDrivesRec = _wpCnrInsertObject(WinData.sv.pRootFolder,
                                                WinData.sv.hwndTreeCnr,
                                                &ptlIcon,
                                                NULL,       // parent record
                                                NULL);      // RECORDINSERT

                // _wpCnrInsertObject subclasses the container owner,
                // so subclass this with the XFolder subclass
                // proc again; otherwise the new menu items
                // won't work
                WinData.sv.psfvTree = fdrCreateSFV(WinData.sv.hwndTreeFrame,
                                                   WinData.sv.hwndTreeCnr,
                                                   QWL_USER,
                                                   WinData.sv.pRootFolder,
                                                   WinData.sv.pRootFolder);
                WinData.sv.psfvTree->pfnwpOriginal = WinSubclassWindow(WinData.sv.hwndTreeFrame,
                                                                       fnwpSubclassedDrivesFrame);

                // and populate this once we're running
                fdrPostFillFolder(&WinData.sv,
                                  pDrivesRec,
                                  FFL_FOLDERSONLY);

                if (WinData.sv.psfvTree->pfnwpOriginal)
                {
                    // OK, drives frame subclassed:
                    QMSG    qmsg;
                    HAB     hab = WinQueryAnchorBlock(WinData.sv.hwndMainFrame);

                    // expand the tree so that the current
                    // directory/file mask etc. is properly
                    // expanded
                    UpdateDlgWithFullFile(&WinData);
                            // this will expand the tree and
                            // select the current directory;
                            // pllToExpand receives records to
                            // add first children to

                    WinData.sv.fFileDlgReady = TRUE;

                    // set the entry field's initial contents:
                    // a) if this is an "open" dialog, it should receive
                    //    the file mask (FILEDLG.szFullFile normally
                    //    contains something like "C:\path\*")
                    // b) if this is a "save as" dialog, it should receive
                    //    the filename that the application proposed
                    //    (FILEDLG.szFullFile normally has "C:\path\filename.ext")
                    WinSetWindowText(WinData.hwndFileEntry,     // WinFileDlg
                                     (flInitialParse & FFL_FILEMASK)
                                        ? WinData.szFileMask
                                        : WinData.szFileName);

                    WinSetFocus(HWND_DESKTOP, WinData.hwndFileEntry);

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
                        if (    (qmsg.hwnd == WinData.sv.hwndMainControl)
                             && (qmsg.msg == WM_CLOSE)
                           )
                        {
                            // main file dlg client got WM_CLOSE:
                            // terminate the modal loop then
                            fExit = TRUE;

                            winhSaveWindowPos(WinData.sv.hwndMainFrame,
                                              HINI_USER,
                                              INIAPP_XWORKPLACE,
                                              INIKEY_WNDPOSFILEDLG);
                        }

                        WinDispatchMsg(hab, &qmsg);

                        if (fExit)
                        {
                            if (pfd->lReturn == DID_OK)
                            {
                                sprintf(pfd->szFullFile,
                                        "%s%s\\%s",
                                        WinData.szDrive,     // C: or \\SERVER\RESOURCE
                                        WinData.szDir,
                                        WinData.szFileName);
                                pfd->ulFQFCount = 1;
                                    // @@todo multiple selections
                                pfd->sEAType = -1;
                                    // @@todo set this to the offset for "save as"
                            }

                            hwndReturn = (HWND)TRUE;
                            break;
                        }
                    }
                } // if (WinData.psfvTree->pfnwpOriginal)

            } // end else if (    (!WinData.pRootFolder)
        }
    }
    CATCH(excpt1)
    {
        // crash: return error
        hwndReturn = NULLHANDLE;
    }
    END_CATCH();

    /*
     *  CLEANUP
     *
     */

    fdrCleanupSplitView(&WinData.sv);

    lstClear(&WinData.llDisks);
    lstClear(&WinData.llDialogControls);

    _PmpfF(("exiting, pfd->lReturn is %d", pfd->lReturn));
    _PmpfF(("  pfd->szFullFile is %s", pfd->szFullFile));
    _PmpfF(("  returning 0x%lX", hwndReturn));

    return (hwndReturn);
}

