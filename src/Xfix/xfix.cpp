
/*
 *@@sourcefile xfix.cpp:
 *      the one and only source file for xfix.exe, the
 *      "XWorkplace handles fixer".
 *
 *      This was started as a quick hack in 2000... since
 *      my WPS got so confused with its handles in 2001,
 *      I finished this today (2001-01-21). Out of my
 *      11.000 handles, I managed to delete about 3.000,
 *      and my Desktop started again.
 *
 *      V0.9.16 added support for removing object IDs.
 *      V1.0.4  added national language support. Resources are stored in main
 *              resource file.
 *
 *@@header "xfix.h"
 *@@added V0.9.5 (2000-08-13) [umoeller]
 */

/*
 *      Copyright (C) 2000-2002 Ulrich M”ller.
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, in version 2 as it comes in the COPYING
 *      file of the XFolder main distribution.
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 */

#define OS2EMX_PLAIN_CHAR

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <setjmp.h>

#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_WIN
#include <os2.h>

#include "setup.h"

#include "helpers\cnrh.h"
#include "helpers\datetime.h"           // date/time helper routines
#include "helpers\dialog.h"
#include "helpers\dosh.h"
#include "helpers\except.h"
#include "helpers\linklist.h"
#include "helpers\nls.h"                // National Language Support helpers
#include "helpers\nlscache.h"           // added to load string resources from NLS DLL V1.0.4 (2005-02-24) [chennecke]
#include "helpers\prfh.h"
#include "helpers\standards.h"
#include "helpers\stringh.h"
#include "helpers\threads.h"
#include "helpers\tree.h"
#include "helpers\winh.h"
#define INCLUDE_WPHANDLE_PRIVATE
#include "helpers\wphandle.h"
#include "helpers\xstring.h"

#include "shared\common.h"
#include "shared\helppanels.h"          // all XWorkplace help panel IDs

#include "bldlevel.h"
#include "dlgids.h"

/* ******************************************************************
 *
 *   Stuff that should be in a header
 *
 *@@changed V1.0.9 (2011-10-24) [rwalsh]: separated definitions from data
 *
 ********************************************************************/

/*
 *@@ DEFERREDINI:
 *      describes an INI entry to be written or
 *      nuked on write-back.
 *
 *      Stored in G_llDeferredNukes.
 *
 *@@added V0.9.7 (2001-01-25) [umoeller]
 */

typedef struct _DEFERREDINI
{
    HINI        hini;                   // normally G_hiniUser
    const char  *pcszApp;               // INI application
    PSZ         pszKey;                 // INI key
    PVOID       pvData;                 // or NULL for nuke
    ULONG       cbData;                 // or 0 for nuke
} DEFERREDINI, *PDEFERREDINI;


/*
 *@@ OBJID:
 *
 *@@added V0.9.16 (2001-09-29) [umoeller]
 */

typedef struct _OBJID
{
    TREE        Tree;                   // ulKey has the object handle
    const char  *pcszID;                // object ID

    struct _OBJIDRECORD *pRecord;       // if the obj IDs window is currently
                                        // open, the object ID record.
                                        // NOTE: This pointer is _invalid_
                                        // if G_hwndObjIDsFrame is NULLHANDLE.
} OBJID, *POBJID;


/*
 *@@ OBJIDRECORD:
 *
 *@@added V0.9.16 (2001-09-29) [umoeller]
 */

typedef struct _OBJIDRECORD
{
    RECORDCORE      recc;

    POBJID          pObjID;             // the corresponding OBJID record

    ULONG           ulIndex;

    PCSZ            pcszID;             // points into pObjID

    PCSZ            pszStatus;          // NULL if valid

    ULONG           ulHandle;           // handle for sorting
    PCSZ            pcszHandle;         // points to szHandle
    CHAR            szHandle[20];

    PSZ             pszLongName;        // if FS obj, points to a NODERECORD.szLongName;
                                        // if abstract, points to szLongName
    CHAR            szLongName[2*CCHMAXPATH];

} OBJIDRECORD, *POBJIDRECORD;


/*
 *@@ NODERECORD:
 *      container record structure allocated for
 *      every single node. There can be several
 *      thousands of these.
 *
 *      Yes, I should use a MINIRECORDCORE, but
 *      my container functions don't support this
 *      yet.
 *
 *      There is sort of a reverse linkage between
 *      NODE's in the handles buffer and these
 *      records... but no direct pointers, since
 *      we change the buffer directly sometimes.
 *
 *      Use the "ulOfs" field to get the corresponding
 *      node. Even better, use the hash tables.
 *
 *@@changed V1.0.9 (2011-10-26) [rwalsh]: added ulType and NODETYPE_* flags, removed ulIsFolder
 *@@changed V1.0.9 (2011-10-28) [rwalsh]: removed NODESTAT_FOLDERPOSNOTFOLDER (it is not an error!)
 *                                        added   *_CORRUPT_PATH, *_ISFILENOTFOLDER, *_ISFOLDERNOTFILE
 *@@changed V1.0.9 (2011-10-30) [rwalsh]: added 'ulAction', 'cProtected', and NODEACTION_* flags
 *@@changed V1.0.9 (2011-11-01) [rwalsh]: added 'cOrphaned'
 */

// flags set in ulType while parsing handles buffer
// added V1.0.9 (2011-10-26) [rwalsh]: added NODETYPE_*
#define NODETYPE_ERROR                  0x0000
#define NODETYPE_DRIVE                  0x0001
#define NODETYPE_FOLDER                 0x0002
#define NODETYPE_FILE                   0x0003

// flags set in ulStatus while parsing handles buffer
#define NODESTAT_OK                     0x0000    // no errors
#define NODESTAT_DUPLICATEDRIV          0x0001    // duplicate DRIV node
#define NODESTAT_ISDUPLICATE            0x0002    // duplicate NODE node
#define NODESTAT_INVALIDDRIVE           0x0004    // drive doesn't exist
#define NODESTAT_INVALID_PARENT         0x0008    // parent handle doesn't exist
#define NODESTAT_CORRUPT_PATH           0x0010    // can't construct valid path

// additional flags set in ulStatus after checking files
#define NODESTAT_FILENOTFOUND           0x0100    // file/folder is missing
#define NODESTAT_PARENTNOTFOLDER        0x0200    // parent handle is not a folder
#define NODESTAT_ABSTRACTSNOTFOLDER     0x0400    // abstracts in non-folder
#define NODESTAT_ISFILENOTFOLDER        0x0800    // file is marked as a folder
#define NODESTAT_ISFOLDERNOTFILE        0x1000    // folder is marked as a file

// mutually-exclusive flags set in ulAction to identify user-initiated action
#define NODEACTION_NONE                 0x0000    // no action was requested
#define NODEACTION_DELETE               0x0001    // record is scheduled for deletion
#define NODEACTION_PROTECT              0x0002    // record is protected from deletion

typedef struct _NODERECORD
{
    RECORDCORE  recc;

    struct _NODERECORD  *pNextRecord;       // next record;

    HPOINTER    icon;                       // record icon

    ULONG       ulIndex;                    // index

    ULONG       ulDriveNdx;                 // for root folders only, contains an
                                            // index into G_DriveHashTable for the
                                            // corresponding DRIV record; for any
                                            // other record, this must be zero

    ULONG       ulType;                     // NODETYPE_* flags

    ULONG       ulStatus;                   // NODESTAT_* flags

    ULONG       ulAction;                   // NODEACTION_* flags

    ULONG       cProtected;                 // protection count; the record can't
                                            // be deleted until it is zero

    ULONG       cOrphaned;                  // count of pending deletions that will
                                            // cause this handle to become an orphan

    PSZ         pszStatusDescr;             // descriptive text of the status

    PCSZ        pcszType;                   // ulType description, may also be "root"

    ULONG       ulOfs;                      // offset of NODE in global buffer

    ULONG       ulHandle;                   // handle
    ULONG       ulParentHandle;             // parent handle

    struct _NODERECORD *pNextDuplicate;     // if there is a duplicate NODE (i.e. a NODE
                                            // with the same handle), this points to it;
                                            // this is a linked list then if several
                                            // duplicates exist (never seen this);
                                            // the first entry of the linked list is
                                            // stored in the records hash table

    ULONG       cDuplicates;                // no. of duplicates, if any

    CHAR        szHexHandle[8];
    PSZ         pszHexHandle;               // points to szHexHandle always

    CHAR        szHexParentHandle[8];
    PSZ         pszHexParentHandle;         // points to szHexParentHandle always

    ULONG       cChildren;                  // references to this handle

    ULONG       cAbstracts;                 // abstract objects in this folder
    BOOL        fFolderPos;                 // if TRUE, folderpos entry exists

    POBJID      pObjID;                     // if != NULL, this entry has an
                                            // object ID assigned

    PSZ         pszRefcsDescr;              // descriptive text for references

    PSZ         pszShortNameCopy;           // points into szShortNameCopy
    CHAR        szShortNameCopy[CCHMAXPATH];

    PSZ         pszLongName;                // points to szLongName
    CHAR        szLongName[CCHMAXPATH + 64];

    APIRET      arcComposePath;             // error code from wphComposePath

} NODERECORD, *PNODERECORD;


/*
 * Miscellaneous macros
 */

#define     TIMERID_THREADRUNNING       998
#define     TIMERID_SELECTIONCHANGED    997


/* ******************************************************************
 *
 *@@ RCArray:
 *      a simple class for creating extensible arrays of
 *      PRECORDCOREs that can easily be passed to functions
 *
 *@@added v1.0.9 (2011-10-30) [rwalsh]:
 *
 ********************************************************************/

class RCArray
{
public:
    RCArray(ULONG size = 64) : incr(64), total(0), ndx(0), pdata(0)
                              { if (size > 64) incr = size; }

    ~RCArray()                { if (pdata) free(pdata); }

    ULONG         Count()     { return ndx; }

    PRECORDCORE*  Data()      { return pdata; }

    ULONG   Append(PRECORDCORE element)
    {
        if (ndx >= total)
        {
            total += incr;
            pdata = (PRECORDCORE*)realloc(pdata, total * sizeof(PRECORDCORE));
        }
        pdata[ndx++] = element;
        return ndx;
    }

protected:
    ULONG       incr;
    ULONG       total;
    ULONG       ndx;
    PRECORDCORE *pdata;
};


/* ******************************************************************
 *
 *   Global variables
 *
 *@@changed V1.0.9 (2011-10-24) [rwalsh]: reorganized global data
 *
 ********************************************************************/

HAB           G_hab = NULLHANDLE;

HMODULE       G_hmodNLS = NULLHANDLE;       // module handle for NLS resource DLL V1.0.4 (2005-02-25) [chennecke]

HPOINTER      G_hptrMain    = NULLHANDLE,   // xfix icon V0.9.15 (2001-09-14) [umoeller]
              G_iconBlank   = NULLHANDLE,   // record icons V1.0.9 (2011-10-29) [rwalsh]
              G_iconError   = NULLHANDLE,
              G_iconDelete  = NULLHANDLE,
              G_iconProtect = NULLHANDLE,
              G_iconSafe    = NULLHANDLE,
              G_iconOrphan  = NULLHANDLE;

HPOINTER      *G_iconSort[] = { &G_iconError,   // this defines the sort order
                                &G_iconOrphan,  // when sorting by icon
                                &G_iconDelete,
                                &G_iconProtect,
                                &G_iconSafe,
                                &G_iconBlank
                              };

HWND          G_hwndMain = NULLHANDLE,
              G_hwndObjIDsFrame = NULLHANDLE,
              G_hwndContextMenuSingle = NULLHANDLE,
              G_hwndContextMenuMulti = NULLHANDLE,
              G_hwndHelp = NULLHANDLE;      // help instance

PFNWP         G_pfnwpCnrOrig = NULL,
              G_fnwpMainFrameOrig = NULL,
              G_fnwpObjIDsFrameOrig = NULL;

HINI          G_hiniUser = HINI_USER,
              G_hiniSystem = HINI_SYSTEM;   // V0.9.19 (2002-04-14) [umoeller]

const char    *INIAPP               = "XWorkplace";
const char    *INIKEY_MAINWINPOS    = "HandleFixWinPos";
const char    *INIKEY_OBJIDSWINPOS  = "ObjIDsWinPos";

THREADINFO    G_tiInsertHandles = {0},
              G_tiCheckFiles = {0};

volatile TID  G_tidInsertHandlesRunning = 0,
              G_tidCheckFilesRunning = 0;

BOOL          G_fResolvingRefs = FALSE;

ULONG         G_ulrc = 0,                   // return code from main()
              G_ulPercentDone = 0,
              G_cHandlesParsed = 0,
              G_cDuplicatesFound = 0,
              G_cHandlesSelected = 0,
              G_cHandlesInvalid = 0,
              G_cHandlesDeleted = 0,
              G_cHandlesOrphaned = 0;

PRECORDCORE   G_preccSource = NULL;         // record that initiated a popup menu

CHAR          G_cThousands[8] = ".",        // NLS thousands separator
              G_szStatusFmt[256];           // format string for statusbar counts

/*
 * File and Abstract Handles globals
 */

PHANDLESBUF   G_pHandlesBuf = NULL;
PNODERECORD   G_preccVeryFirst = NULL;      // root of records list
PNODERECORD   G_DriveHashTable[27];         // DRIV recordcore ptrs indexed by drive
PNODERECORD   G_RecordHashTable[65536];     // NODE recordcore ptrs indexed by handle
USHORT        G_ausAbstractFolders[65536];  // folder handles indexed by abstract handle 


/*
 * DEFERREDINI globals
 */

LINKLIST      G_llDeferredNukes;
BOOL          G_fDesktopLoaded = FALSE;
PNODERECORD   G_precDesktop = NULL;
PULONG        G_paulAbstractsTarget = NULL;
CHAR          G_szHandleDesktop[8];         // hex handle of desktop
ULONG         G_cAbstractsDesktop = 0;
ULONG         G_cTotalAbstractsMoved = 0;


/*
 * OBJID globals
 */

TREE          *G_ObjIDsTree;                // has all OBJID nodes
LONG          G_cObjIDs = 0;                // count of nodes


/*
 *@@ STRINGENTITY:
 *     Entity array for NLS strings
 *     This is the equivalent of XWPENTITY from common.c.
 *     The entities defined in this array are automatically
 *     replaced in the strings loaded from the NLS DLL via
 *     nlsGetString().
 *
 *     The array is passed to nlsInitStrings() along with
 *     the number of elements to initialize the NLS cache.
 *
 *@@added V1.0.4 (2002-03-04) [chennecke]
 */
 
PCSZ    G_pcszBldlevel = BLDLEVEL_VERSION,
        G_pcszBldDate = __DATE__,
        G_pcszNewLine = "\n",
        G_pcszCopyright = "(C)",
        G_pcszXWorkplace = "XWorkplace";
 
const STRINGENTITY G_aEntities[] =
    {
        "&copy;", &G_pcszCopyright,
        "&xwp;", &G_pcszXWorkplace,
        "&version;", &G_pcszBldlevel,
        "&date;", &G_pcszBldDate,
        "&nl;", &G_pcszNewLine,
    };


/* ******************************************************************
 *
 * Forward declarations
 *
 ********************************************************************/

VOID UpdateMenuItems(USHORT usSortCmd);

VOID MarkAllOrphans(PNODERECORD prec,
                    ULONG ulParentHandle);

VOID Unprotect(PNODERECORD precIn, RCArray* prca);

 
/* ******************************************************************
 *
 *   Handles helpers
 *
 ********************************************************************/

/*
 *@@ Append:
 *
 */

VOID Append(PXSTRING pstr,
            const char *pcsz,
            const char *pcszSeparator)
{
    if (pstr->ulLength)
        xstrcat(pstr, pcszSeparator, 0);
    xstrcat(pstr, pcsz, 0);
}

/*
 *@@ SetIconAndStatus:
 *      ensures that a record's ulStatus, ulAction,
 *      and cProtected fields are in a consistent state,
 *      then sets its icon and status text accordingly
 *
 *@@added V1.0.9 (2011-10-29) [rwalsh]:
 */

VOID  SetIconAndStatus(PNODERECORD prec)
{
    XSTRING     str;

    if (prec->pszStatusDescr)
    {
        free(prec->pszStatusDescr);
        prec->pszStatusDescr = NULL;
    }

    if (prec->ulStatus)
    {
        xstrInit(&str, 0);

        if (prec->ulAction == NODEACTION_DELETE)
            prec->icon = G_iconDelete;
        else
        {
            if (prec->ulAction == NODEACTION_PROTECT)
                Unprotect(prec, NULL);
            prec->cProtected = 0;
            prec->icon = G_iconError;
        }
        prec->cOrphaned = 0;

        if (prec->ulStatus & NODESTAT_DUPLICATEDRIV)
            Append(&str, nlsGetString(ID_FXSI_NODESTAT_DUPLICATEDRIV), "; ");

        if (prec->ulStatus & NODESTAT_ISDUPLICATE)
            Append(&str, nlsGetString(ID_FXSI_NODESTAT_ISDUPLICATE), "; ");

        if (prec->ulStatus & NODESTAT_INVALIDDRIVE)
            Append(&str, nlsGetString(ID_FXSI_NODESTAT_INVALIDDRIVE), "; ");

        if (prec->ulStatus & NODESTAT_INVALID_PARENT)
            Append(&str, nlsGetString(ID_FXSI_NODESTAT_INVALIDPARENT), "; ");

        if (prec->ulStatus & NODESTAT_CORRUPT_PATH)
            Append(&str, nlsGetString(ID_FXSI_NODESTAT_CORRUPTPATH), "; ");

        if (prec->ulStatus & NODESTAT_FILENOTFOUND)
            Append(&str, nlsGetString(ID_FXSI_NODESTAT_FILENOTFOUND), "; ");

        if (prec->ulStatus & NODESTAT_PARENTNOTFOLDER)
            Append(&str, nlsGetString(ID_FXSI_NODESTAT_PARENTNOTFOLDER), "; ");

        if (prec->ulStatus & NODESTAT_ABSTRACTSNOTFOLDER)
            Append(&str, nlsGetString(ID_FXSI_NODESTAT_ABSTRACTSNOTFOLDER), "; ");

        if (prec->ulStatus & NODESTAT_ISFILENOTFOLDER)
            Append(&str, nlsGetString(ID_FXSI_NODESTAT_ISFILENOTFOLDER), "; ");

        if (prec->ulStatus & NODESTAT_ISFOLDERNOTFILE)
            Append(&str, nlsGetString(ID_FXSI_NODESTAT_ISFOLDERNOTFILE), "; ");

        // we had an error
        if (str.ulLength)
            prec->pszStatusDescr = str.psz;

        return;
    }

    if (prec->cProtected)
    {
        if (prec->ulAction == NODEACTION_PROTECT)
            prec->icon = G_iconProtect;
        else
        {
            prec->ulAction = NODEACTION_NONE;
            prec->icon = G_iconSafe;
        }

        return;
    }

    if (prec->ulAction == NODEACTION_DELETE)
        prec->icon = G_iconDelete;
    else
    {
        prec->ulAction = NODEACTION_NONE;
        if (prec->cOrphaned)
        {
            xstrInit(&str, 0);
            xstrcat(&str, nlsGetString(ID_FXSI_NODESTAT_POSSIBLEORPHAN), 0);
            prec->pszStatusDescr = str.psz;
            prec->icon = G_iconOrphan;
        }
        else
            prec->icon = G_iconBlank;
    }

    return;
}

/*
 *@@ UpdateStatusDescr:
 *
 *@@changed V0.9.19 (2002-04-14) [umoeller]: added description for ERROR_WPH_INVALID_PARENT_HANDLE
 *@@changed V1.0.9 (2011-10-28) [rwalsh]: removed prec->arcComposePath msgs, added 'Corrupted Path' msg
 *@@changed V1.0.9 (2011-10-29) [rwalsh]: removed CRA_PICKED, moved status msgs to SetIconAndStatus()
 *@@changed V1.0.9 (2011-11-07) [rwalsh]: put each Reference item on a separate line
 */

VOID UpdateStatusDescr(PNODERECORD prec)
{
    XSTRING     str;

    // icon and status description
    SetIconAndStatus(prec);

    // references
    if (prec->pszRefcsDescr)
    {
        free(prec->pszRefcsDescr);
        prec->pszRefcsDescr = NULL;
    }

    xstrInit(&str, 0);

    if (prec->cAbstracts)
    {
        CHAR sz[100];
        sprintf(sz, nlsGetString(ID_FXSI_REFERENCES_ABSTRACTS), prec->cAbstracts);
        Append(&str, sz, "");
    }

    if (prec->fFolderPos)
        Append(&str, nlsGetString(ID_FXSI_REFERENCES_FOLDERPOS), "\n");

    if (prec->pObjID)
        Append(&str, prec->pObjID->pcszID, "\n");

    if (str.ulLength)
        prec->pszRefcsDescr = str.psz;
}

/*
 *@@ ComposeFullName:
 *
 *@@changed V1.0.9 (2011-10-28) [rwalsh]: restructured, added NODESTAT_CORRUPT_PATH
 *@@changed V1.0.9 (2011-11-02) [rwalsh]: added trailing backslash for folders
 */

BOOL ComposeFullName(PNODERECORD precc,
                     PNODE pNode)
{
    // if this failed already, don't try again
    if (!precc->arcComposePath)
    {
        precc->arcComposePath = wphComposePath((HHANDLES)G_pHandlesBuf,
                                               pNode->usHandle,
                                               precc->szLongName,
                                               sizeof(precc->szLongName),
                                               NULL);
        if (!precc->arcComposePath)
        {
            precc->pszLongName = precc->szLongName;
            if (precc->ulType == NODETYPE_FOLDER)
                strcat(precc->szLongName, "\\");
        }
        else
            if (precc->arcComposePath == ERROR_WPH_INVALID_PARENT_HANDLE)
                precc->ulStatus |= NODESTAT_INVALID_PARENT;
            else
                precc->ulStatus |= NODESTAT_CORRUPT_PATH;
    }

    return TRUE;
}

/*
 *@@ RebuildRecordsHashTable:
 *
 *      Only called after records have been removed.
 *
 *@@added V0.9.9 (2001-04-07) [umoeller]
 *@@changed V1.0.9 (2011-10-27) [rwalsh]: added G_DriveHashTable
 */

VOID RebuildRecordsHashTable(VOID)
{
    PNODERECORD prec = G_preccVeryFirst;

    memset(G_RecordHashTable, 0, sizeof(G_RecordHashTable));
    memset(G_DriveHashTable, 0, sizeof(G_DriveHashTable));

    // we can ignore duplicates because they were
    // dealt with when the buffer was first parsed
    while (prec)
    {
        if (prec->ulHandle)
        {
            if (!G_RecordHashTable[prec->ulHandle])
                G_RecordHashTable[prec->ulHandle] = prec;
        }
        else
        if (prec->ulDriveNdx && prec->ulDriveNdx < 27)
        {
            if (!G_DriveHashTable[prec->ulDriveNdx])
                G_DriveHashTable[prec->ulDriveNdx] = prec;

        }
        prec = prec->pNextRecord;
    }
}

/* ******************************************************************
 *
 *   Misc helpers
 *
 ********************************************************************/

/*
 *@@ MessageBox:
 *      wrapper for showing a message box.
 *
 *@@added V0.9.15 (2001-09-14) [umoeller]
 *@@changed V1.0.4 (2005-02-24) [chennecke]: replaced hard-coded strings with nlsGetString() calls
 */

ULONG MessageBox(HWND hwndOwner,
                 ULONG flFlags,             // in: standard message box flags
                 const char *pcszFormat,
                 ...)
{
    MSGBOXSTRINGS Strings =
        {
            nlsGetString(DID_YES),
            nlsGetString(DID_NO),
            nlsGetString(DID_OK),
            nlsGetString(DID_CANCEL),
            nlsGetString(DID_ABORT),
            nlsGetString(DID_RETRY),
            nlsGetString(DID_IGNORE),
            nlsGetString(DID_ENTER),
            nlsGetString(DID_YES2ALL)
        };

    CHAR szBuf[4000];
    va_list     args;
    int         i;
    va_start(args, pcszFormat);
    i = vsprintf(szBuf, pcszFormat, args);
    va_end(args);

    return (dlghMessageBox(hwndOwner,
                           G_hptrMain,
                           "xfix",
                           szBuf,
                           NULL,
                           flFlags,
                           "9.WarpSans",
                           &Strings));
}

/*
 *@@ CalcHandleCounts:
 *
 *@@added V1.0.9 (2011-11-04) [rwalsh]
 */

VOID  CalcHandleCounts(VOID)
{
    PNODERECORD prec;

    G_cHandlesParsed = 0,
    G_cHandlesSelected = 0,
    G_cHandlesDeleted = 0,
    G_cHandlesInvalid = 0,
    G_cHandlesOrphaned = 0;

    // for every record
    for (prec = G_preccVeryFirst;
         prec;
         prec = prec->pNextRecord)
    {
        G_cHandlesParsed++;

        if (prec->recc.flRecordAttr & CRA_SELECTED)
            G_cHandlesSelected++;

        if (prec->ulAction == NODEACTION_DELETE)
            G_cHandlesDeleted++;
        else
        if (prec->ulStatus)
            G_cHandlesInvalid++;
        else
        if (prec->cOrphaned)
            G_cHandlesOrphaned++;
    }
}


/*
 *@@ SetStatusBarText:
 *
 *@@added V0.9.16 (2001-09-29) [umoeller]
 *@@changed V1.0.9 (2011-11-04) [rwalsh]: added record counts to main statusbar
 */

VOID SetStatusBarText(HWND hwndFrame,
                      const char *pcszFormat,
                      ...)
{
    va_list args;
    int     i = 0;
    CHAR    szBuf[1024];

    if (hwndFrame == G_hwndMain)
    {
        CHAR    sz1[16],
                sz2[16],
                sz3[16],
                sz4[16],
                sz5[16];

        // update status counts
        i = sprintf(szBuf, G_szStatusFmt,
                    nlsThousandsULong(sz1, G_cHandlesParsed,   G_cThousands[0]),
                    nlsThousandsULong(sz2, G_cHandlesSelected, G_cThousands[0]),
                    nlsThousandsULong(sz3, G_cHandlesInvalid,  G_cThousands[0]),
                    nlsThousandsULong(sz4, G_cHandlesDeleted,  G_cThousands[0]),
                    nlsThousandsULong(sz5, G_cHandlesOrphaned, G_cThousands[0]));
    }

    // append message if provided
    if (pcszFormat)
    {
        va_start(args, pcszFormat);
        i = vsprintf(&szBuf[i], pcszFormat, args);
        va_end(args);
    }

    WinSetWindowText(WinWindowFromID(hwndFrame, FID_STATUSBAR),
                     szBuf);
}

/*
 *@@ StandardCommands:
 *      handles WM_COMMANDs shared between several windows.
 *
 *@@added V0.9.16 (2001-09-29) [umoeller]
 *@@changed V1.0.4 (2005-02-24) [chennecke]: replaced hard-coded strings with nlsGetString() calls
 *                                           renamed identifiers IDM... to ID_FXM...
 */

VOID StandardCommands(HWND hwndFrame, USHORT usCmd)
{
    switch (usCmd)
    {
        case ID_FXMI_EXIT:
            WinPostMsg(hwndFrame,
                       WM_QUIT,
                       0,
                       0);
        break;

        case ID_FXMI_HELP_GENERAL:
        case ID_FXMI_HELP_USINGHELP:
            if (G_hwndHelp)
            {
                winhDisplayHelpPanel(G_hwndHelp,
                                    (usCmd == ID_FXMI_HELP_GENERAL)
                                        ? ID_XSH_XFIX_INTRO
                                        : 0);       // "using help"
            }
        break;

        case ID_FXMI_HELP_PRODINFO:
            MessageBox(hwndFrame,
                       MB_OK | MB_MOVEABLE,
                       nlsGetString(ID_FXSI_PRODINFO));
        break;
    }
}

LONG inline CompareULongs(PVOID precc1, PVOID precc2, ULONG ulFieldOfs)
{
    ULONG   ul1 = *(PULONG)((PBYTE)precc1 + ulFieldOfs),
            ul2 = *(PULONG)((PBYTE)precc2 + ulFieldOfs);
    if (ul1 < ul2)
        return -1;
    if (ul1 > ul2)
        return 1;
    return 0;
}

/*
 *@@ CompareStrings:
 *
 *
 *@@changed V0.9.16 (2001-10-19) [umoeller]: made this use WinCompareStrings
 */

LONG inline CompareStrings(PVOID precc1, PVOID precc2, ULONG ulFieldOfs)
{
    char *psz1 = *(char**)((PBYTE)precc1 + ulFieldOfs);
    char *psz2 = *(char**)((PBYTE)precc2 + ulFieldOfs);
    if ((psz1) && (psz2))
    {
        switch (WinCompareStrings(G_hab,
                                  0,     // current codepage
                                  0,     // current country
                                  psz1,
                                  psz2,
                                  0))    // reserved
        {
            case WCS_LT: return -1;
            case WCS_GT: return +1;
            default:
            // case WCS_EQ:
                return 0;
        }
    }
    else if (psz1)
        // string 1 exists, but 2 doesn't:
        return (1);
    else if (psz2)
        // string 2 exists, but 1 doesn't:
        return (-1);

    return 0;
}

/*
 *@@ CompareExtension:
 *
 *
 *@@added V1.0.9 (2011-11-02) [rwalsh]:
 */

LONG inline CompareExtension(PVOID precc1, PVOID precc2, ULONG ulFieldOfs)
{
    char *psz1 = *(char**)((PBYTE)precc1 + ulFieldOfs);
    char *psz2 = *(char**)((PBYTE)precc2 + ulFieldOfs);

    if (psz1 && psz2)
    {
        char *pDot1 = strrchr(psz1, '.');
        char *pDot2 = strrchr(psz2, '.');

        if (pDot1 == psz1)
            pDot1 = NULL;
        if (pDot2 == psz2)
            pDot2 = NULL;

        if (pDot1 && pDot2)
        {
            switch (WinCompareStrings(G_hab,
                                      0,     // current codepage
                                      0,     // current country
                                      pDot1,
                                      pDot2,
                                      0))    // reserved
            {
                case WCS_LT: return -1;
                case WCS_GT: return +1;
                default:     return  0;
            }
        }
        psz1 = pDot1;
        psz2 = pDot2;
    }

    // string 1 exists, but 2 doesn't:
    if (psz1)
        return (1);

    // string 2 exists, but 1 doesn't:
    if (psz2)
        return (-1);

    return 0;
}

/* ******************************************************************
 *
 *   Insert Handles thread
 *
 ********************************************************************/

/*
 *@@ CompareFolderPosNodes:
 *
 *@@added V0.9.9 (2001-04-07) [umoeller]
 */

int TREEENTRY CompareStrings(ULONG ul1, ULONG ul2)
{
    return (strhcmp((const char*)(ul1),
                    (const char*)(ul2)));
}

/*
 *@@ HasFolderPos:
 *
 *@@added V0.9.9 (2001-04-07) [umoeller]
 */

BOOL HasFolderPos(TREE *pFolderPosTree,
                  ULONG ulHandle32)
{
    CHAR szDecimalHandle[30];
    sprintf(szDecimalHandle,
            "%d",
            ulHandle32);
    return (NULL != treeFind(pFolderPosTree,
                             (ULONG)szDecimalHandle,
                             CompareStrings));
}

typedef struct _FOLDERPOSNODE
{
    TREE        Tree;                       // ulKey points to szDecimalHandle

    CHAR        szDecimalHandle[30];        // decimal handle

} FOLDERPOSNODE, *PFOLDERPOSNODE;

/*
 *@@ fntInsertHandles:
 *
 *@@changed V0.9.9 (2001-04-07) [umoeller]: fixed wrong duplicates reports for UNC drive names
 *@@changed V0.9.9 (2001-04-07) [umoeller]: sped up folder pos search fourfold
 *@@changed V0.9.16 (2001-09-29) [umoeller]: fixed missing folderpos markers
 *@@changed V1.0.9 (2011-10-26) [rwalsh]: added ulType, removed ulIsFolder, changed pszType
 *@@changed V1.0.9 (2011-10-27) [rwalsh]: added ulDriveNdx and G_DriveHashTable, removed G_aDriveNodes
 */

void _Optlink fntInsertHandles(PTHREADINFO ptiMyself)
{
    ULONG   cReccsTotal = 0;

    HWND    hwndCnr = (HWND)ptiMyself->ulData;

    G_ulPercentDone = 0;
    G_fResolvingRefs = FALSE;

    // make this idle-time, or we'll hog the system
    DosSetPriority(PRTYS_THREAD,
                   PRTYC_IDLETIME,
                   +31,
                   0);

    ULONG ulMillisecondsStarted = dtGetULongTime();

    TRY_LOUD(excpt1)
    {
        // clear container
        cnrhRemoveAll(hwndCnr);

        // clear globals
        G_preccVeryFirst = NULL;
        G_cDuplicatesFound = 0;
        memset(G_DriveHashTable, 0, sizeof(G_DriveHashTable));

        // invalidate records hash table
        memset(G_RecordHashTable, 0, sizeof(G_RecordHashTable));

        // invalidate abstracts hash V0.9.16 (2001-09-29) [umoeller]
        memset(G_ausAbstractFolders, 0, sizeof(G_ausAbstractFolders));

        // load object IDs from OS2.INI
        APIRET arc;
        PSZ pszObjIDs = NULL;
        if (!(arc = prfhQueryKeysForApp(G_hiniUser,
                                        WPINIAPP_LOCATION, // "PM_Workplace:Location",
                                        &pszObjIDs)))
        {
            treeInit(&G_ObjIDsTree, &G_cObjIDs);

            // build tree from folderpos entries V0.9.9 (2001-04-07) [umoeller]
            const char *pcszObjIDThis = pszObjIDs;
            while (*pcszObjIDThis)
            {
                OBJID    *pNewNode = NEW(OBJID);
                pNewNode->pcszID = pcszObjIDThis;
                ULONG cb = sizeof(pNewNode->Tree.ulKey);
                PrfQueryProfileData(G_hiniUser,
                                    WPINIAPP_LOCATION, // "PM_Workplace:Location",
                                    pcszObjIDThis,
                                    &pNewNode->Tree.ulKey,
                                    &cb);
                treeInsert(&G_ObjIDsTree,
                           &G_cObjIDs,
                           (TREE*)pNewNode,
                           treeCompareKeys);      // sort by handle

                pcszObjIDThis += strlen(pcszObjIDThis) + 1;   // next key
            }

            // free(pszObjIDs);
            // do not free, the tree uses pointers into this buffer

        } // end  if (!(arc = prfhQueryKeysForApp(G_hiniUser, "PM_Workplace:Location"

        // build the abstract/folders hash table
        PSZ pszFolderContent;
        if (!(arc = prfhQueryKeysForApp(G_hiniUser,
                                        WPINIAPP_FDRCONTENT, // "PM_Abstract:FldrContent",
                                        &pszFolderContent)))
        {
            const char *pcszFolderThis = pszFolderContent;
            while (*pcszFolderThis)
            {
                ULONG cbFolderContent = 0;
                PSZ pszAbstracts = prfhQueryProfileData(G_hiniUser,
                                                        WPINIAPP_FDRCONTENT, // "PM_Abstract:FldrContent",
                                                        pcszFolderThis,
                                                        &cbFolderContent);
                if (pszAbstracts && cbFolderContent)
                {
                    USHORT usFolderHandle = strtol(pcszFolderThis, NULL, 16);

                    // this is an array of ULONGs really
                    ULONG cAbstracts = cbFolderContent / sizeof(ULONG);
                    PULONG paulAbstracts = (PULONG)pszAbstracts;

                    ULONG ul;
                    for (ul = 0;
                         ul < cAbstracts;
                         ul++)
                    {
                        G_ausAbstractFolders[paulAbstracts[ul]] = usFolderHandle;
                    }

                    free(pszAbstracts);
                }

                pcszFolderThis += strlen(pcszFolderThis) + 1;   // next key
            }
        }

        if (G_pHandlesBuf)
        {
            // got handles buffer (loaded in main()):
            // now set the pointer for the end of the BLOCKs buffer
            PBYTE pEnd = G_pHandlesBuf->pbData + G_pHandlesBuf->cbData;

            // pCur is our variable pointer where we're at now; there
            // is some offset of 4 bytes at the beginning (duh)
            PBYTE pCur = G_pHandlesBuf->pbData + 4;

            // first count records we need
            while (pCur < pEnd)
            {
                // _Pmpf(("Loop %d, pCur: 0x%lX, pEnd: 0x%lX", cReccs, pCur, pEnd));
                if (!memicmp(pCur, "DRIV", 4))
                {
                    // pCur points to a DRIVE node:
                    // we don't really care about these, because the root
                    // directory has a real NODE too, so we just
                    // skip this
                    PDRIVE pDriv = (PDRIVE)pCur;
                    cReccsTotal++;
                    pCur += sizeof(DRIVE) + strlen(pDriv->szName);
                }
                else if (!memicmp(pCur, "NODE", 4))
                {
                    // pCur points to a regular NODE: offset pointer first
                    // _Pmpf(("%d: Got NODE", cReccs));
                    PNODE pNode = (PNODE)pCur;
                    cReccsTotal++;
                    pCur += sizeof (NODE) + pNode->usNameSize;
                }
                else
                {
                    // neither DRIVE nor NODE: error
                    cReccsTotal = 0;
                    break;
                }
            } // end while

            if (cReccsTotal)
            {
                // no error: allocate records and start over
                G_preccVeryFirst
                    = (PNODERECORD)cnrhAllocRecords(hwndCnr,
                                                    sizeof(NODERECORD),
                                                    cReccsTotal);

                // load folderpos entries from OS2.INI
                APIRET arc;
                PSZ pszFolderPoses = NULL;
                if (!(arc = prfhQueryKeysForApp(G_hiniUser,
                                                WPINIAPP_FOLDERPOS, // "PM_Workplace:FolderPos",
                                                &pszFolderPoses)))
                {
                    TREE    *FolderPosesTree;
                    treeInit(&FolderPosesTree, NULL);

                    // build tree from folderpos entries V0.9.9 (2001-04-07) [umoeller]
                    // fixed... this shouldn't have used the full key name for
                    // indexing V0.9.16 (2001-09-29) [umoeller]
                    const char *pcszFolderPosThis = pszFolderPoses;
                    while (*pcszFolderPosThis)
                    {
                        FOLDERPOSNODE    *pNewNode = NEW(FOLDERPOSNODE);
                        PCSZ p;
                        ULONG c;
                        if (p = strchr(pcszFolderPosThis, '@'))
                            c = p - pcszFolderPosThis;
                        else
                            c = strlen(pcszFolderPosThis);

                        strncpy(pNewNode->szDecimalHandle, pcszFolderPosThis, c);
                        pNewNode->szDecimalHandle[c] = '\0';
                        pNewNode->Tree.ulKey = (ULONG)pNewNode->szDecimalHandle;
                        treeInsert(&FolderPosesTree,
                                   NULL,
                                   (TREE*)pNewNode,
                                   CompareStrings);
                                // @@ free the tree nodes
                        pcszFolderPosThis += strlen(pcszFolderPosThis) + 1;   // next type/filter
                    }

                    if ((G_preccVeryFirst) && (pszFolderPoses))
                    {
                        // changed V1.0.9 (2011-10-26) [rwalsh]: added NLS strings
                        PCSZ  pcszErrorNLS  = nlsGetString(ID_FXSI_TYPE_ERROR),
                              pcszDriveNLS  = nlsGetString(ID_FXSI_TYPE_DRIVE),
                              pcszRootNLS   = nlsGetString(ID_FXSI_TYPE_ROOT),
                              pcszFolderNLS = nlsGetString(ID_FXSI_TYPE_FOLDER),
                              pcszFileNLS   = nlsGetString(ID_FXSI_TYPE_FILE);

                        PNODERECORD preccThis = G_preccVeryFirst;

                        // restart at beginning of buffer
                        pCur = G_pHandlesBuf->pbData + 4;
                        // now set up records
                        ULONG   ulIndexThis = 0;
                        while (pCur < pEnd)
                        {
                            // copy ptr to next record as given to
                            // us by the container;
                            // I'm not sure this will always remain valid
                            preccThis->pNextRecord = (PNODERECORD)(preccThis->recc.preccNextRecord);

                            preccThis->ulIndex = ulIndexThis++;
                            preccThis->ulOfs = (pCur - G_pHandlesBuf->pbData);

                            preccThis->ulStatus = NODESTAT_OK;

                            preccThis->ulType = NODETYPE_ERROR;
                            preccThis->pcszType = pcszErrorNLS;
                            preccThis->ulDriveNdx = 0;

                            if (!memicmp(pCur, "DRIV", 4))
                            {
                                // pCur points to a DRIVE node:
                                PDRIVE pDriv = (PDRIVE)pCur;

                                // changed V1.0.9 (2011-10-26) [rwalsh]: added ulType, changed pszType
                                preccThis->ulType = NODETYPE_DRIVE;
                                preccThis->pcszType = pcszDriveNLS;

                                strcpy(preccThis->szShortNameCopy,
                                       pDriv->szName);
                                preccThis->pszShortNameCopy = preccThis->szShortNameCopy;

                                sprintf(preccThis->szLongName,
                                        "  [%s]",
                                        preccThis->szShortNameCopy);
                                preccThis->pszLongName = preccThis->szLongName;

                                // store this in global drives array
                                // there are nodes like "\\SERVER" for UNC drive names,
                                // so watch this V0.9.9 (2001-04-07) [umoeller]
                                if (    (pDriv->szName[0] >= 'A')
                                     && (pDriv->szName[0] <= 'Z')
                                   )
                                {
                                    ULONG ulDrive = pDriv->szName[0] - 0x40;

                                    if (!G_DriveHashTable[ulDrive])
                                        // drive not occupied yet:
                                        G_DriveHashTable[ulDrive] = preccThis;
                                    else
                                    {
                                        // that's a duplicate DRIV node!!!
                                        preccThis->ulStatus |= NODESTAT_DUPLICATEDRIV;
                                        G_cDuplicatesFound++;
                                    }
                                }

                                pCur += sizeof(DRIVE) + strlen(pDriv->szName);
                            }
                            else if (!memicmp(pCur, "NODE", 4))
                            {
                                // pCur points to a regular NODE: offset pointer first
                                PNODE pNode = (PNODE)pCur;

                                // changed V1.0.9 (2011-10-26) [rwalsh]:
                                // added ulType, changed pszType;
                                // added test of fsDirectory to determine
                                // if this is a file, folder, or root folder
                                if (pNode->fsDirectory)
                                {
                                    preccThis->ulType = NODETYPE_FOLDER;
                                    preccThis->pcszType = pcszFolderNLS;
                                    if (   pNode->szName[1] == ':'
                                        && pNode->szName[0] >= 'A'
                                        && pNode->szName[0] <= 'Z'
                                       )
                                    {
                                        preccThis->ulDriveNdx = pNode->szName[0] - 0x40;
                                        preccThis->pcszType = pcszRootNLS;
                                    }
                                }
                                else
                                {
                                    preccThis->ulType = NODETYPE_FILE;
                                    preccThis->pcszType = pcszFileNLS;
                                }

                                preccThis->ulHandle = pNode->usHandle;
                                sprintf(preccThis->szHexHandle, "%04lX", pNode->usHandle);
                                preccThis->pszHexHandle = preccThis->szHexHandle;

                                preccThis->ulParentHandle = pNode->usParentHandle;
                                sprintf(preccThis->szHexParentHandle, "%04lX", pNode->usParentHandle);
                                preccThis->pszHexParentHandle = preccThis->szHexParentHandle;

                                strcpy(preccThis->szShortNameCopy, pNode->szName);
                                preccThis->pszShortNameCopy = preccThis->szShortNameCopy;

                                ComposeFullName(preccThis, pNode);

                                // store record in hash table
                                if (!G_RecordHashTable[pNode->usHandle])
                                    G_RecordHashTable[pNode->usHandle] = preccThis;
                                else
                                {
                                    // wow, record already exists for this handle:
                                    // this means it's a duplicate...
                                    PNODERECORD precExisting = G_RecordHashTable[pNode->usHandle],
                                                prec2 = precExisting;

                                    // store in duplicates linked list
                                    while (prec2->pNextDuplicate)
                                        prec2 = prec2->pNextDuplicate;
                                    prec2->pNextDuplicate = preccThis;

                                    // update duplicates count of existing
                                    precExisting->cDuplicates++;

                                    // set error status of THIS record
                                    preccThis->ulStatus |= NODESTAT_ISDUPLICATE;

                                    G_cDuplicatesFound++;
                                }

                                // check abstracts in OS2.INI
                                CHAR    szHandleShort[10];
                                sprintf(szHandleShort, "%lX", pNode->usHandle);
                                                // yes, WPS uses "AB" if less than
                                                // four digits...
                                ULONG cbFolderContent = 0;
                                if (    (PrfQueryProfileSize(G_hiniUser,
                                                             WPINIAPP_FDRCONTENT, // "PM_Abstract:FldrContent",
                                                             szHandleShort,
                                                             &cbFolderContent))
                                     && (cbFolderContent)
                                   )
                                {
                                    // this is a folder, and it has abstract objects:
                                    // store the abstracts count...
                                    // this INI entry is an array of ULONGs
                                    preccThis->cAbstracts = cbFolderContent / sizeof(ULONG);
                                }

                                // check folderpos entries in OS2.INI...
                                // for some reason, these are decimal,
                                // followed by "@" and some other key...
                                // XWP adds keys to this too.
                                preccThis->fFolderPos = HasFolderPos(FolderPosesTree,
                                                                     // compose full handle (hiword)
                                                                     (   (pNode->usHandle)
                                                                       | (G_pHandlesBuf->usHiwordFileSystem << 16L)
                                                                     ));


                                // check if this node has an object ID assigned
                                // V0.9.16 (2001-09-29) [umoeller]
                                preccThis->pObjID = (POBJID)treeFind(G_ObjIDsTree,
                                                                 // handle:
                                                                 (   (pNode->usHandle)
                                                                   | (G_pHandlesBuf->usHiwordFileSystem << 16L)
                                                                 ),
                                                                 treeCompareKeys);

                                pCur += sizeof (NODE) + pNode->usNameSize;
                            }

                            if (ptiMyself->fExit)
                            {
                                // cancel:
                                cReccsTotal = 0;
                                // cReccs2Insert = 0;
                                break;
                            }

                            preccThis = preccThis->pNextRecord;

                            // report progress to thread 1
                            ULONG ulMax = (pEnd - G_pHandlesBuf->pbData);
                            ULONG ulNow = (pCur - G_pHandlesBuf->pbData);
                            G_ulPercentDone = ulNow * 100 / ulMax;
                        } // end while (pCur < pEnd)

                        // done!!

                        // now resolve references
                        if (cReccsTotal)
                        {
                            G_fResolvingRefs = TRUE;
                            G_ulPercentDone = 0;

                            PNODERECORD preccThis = G_preccVeryFirst;
                            ULONG cReccs2 = 0;
                            while (preccThis)
                            {
                                // is this a NODE? (DRIV handle is 0)
                                if (preccThis->ulHandle)
                                {
                                    // for each NODE record, climb up the
                                    // parents tree

                                    // get the NODE from the record hash table
                                    PNODETREENODE pNodeStart = G_pHandlesBuf->NodeHashTable[preccThis->ulHandle],
                                                  pNodeThis = pNodeStart;

                                    PNODERECORD pPrevParent = NULL;

                                    do
                                    {
                                        PNODERECORD pParentRec = NULL;

                                        if (    pNodeThis
                                             && pNodeThis->pNode->usParentHandle)
                                        {
                                            // we have a parent:
                                            pParentRec
                                                = G_RecordHashTable[pNodeThis->pNode->usParentHandle];

                                            if (pParentRec)
                                            {
                                                // parent found:
                                                // raise its usage count
                                                pParentRec->cChildren++;

                                                // go for next higher parent
                                                pNodeThis = G_pHandlesBuf->NodeHashTable[pParentRec->ulHandle];

                                            }
                                            else
                                            {
                                                // we have a parent handle, but no
                                                // parent record:
                                                // whoa, that's a problem
                                                preccThis->ulStatus |= NODESTAT_INVALID_PARENT;
                                                MarkAllOrphans(preccThis,
                                                               preccThis->ulHandle);
                                                break;
                                            }
                                        }
                                        else
                                            // reached the top:
                                            break;
                                    } while (TRUE);

                                } // end if (preccThis->ulHandle)

                                // update descriptions
                                UpdateStatusDescr(preccThis);

                                // next record
                                preccThis = preccThis->pNextRecord;
                                cReccs2++;

                                G_ulPercentDone = cReccs2 * 100 / cReccsTotal;
                            } // end while (preccThis)

                            // finally, insert records
                            cnrhInsertRecords(hwndCnr,
                                              NULL, // parent
                                              (PRECORDCORE)G_preccVeryFirst,
                                              TRUE, // invalidate
                                              NULL,
                                              CRA_RECORDREADONLY,
                                              cReccsTotal);

                        } // if (cReccsTotal)
                    } // end if ((G_preccVeryFirst) && (pszFolderPoses))

                    free(pszFolderPoses);

                } // end  if (!(arc = prfhQueryKeysForApp(G_hiniUser, "PM_Workplace:FolderPos", &pszFolderPoses)))

            } // end if (cReccs)
        }
    }
    CATCH(excpt1)
    {
    } END_CATCH();

    // cnrhInvalidateAll(hwndCnr);

    ULONG ulMillisecondsNow = dtGetULongTime();

    CalcHandleCounts();
    WinPostMsg(G_hwndMain,
               WM_USER,
               (MPARAM)cReccsTotal,
               (MPARAM)(ulMillisecondsNow - ulMillisecondsStarted));
}

/* ******************************************************************
 *
 *   Check Files thread
 *
 ********************************************************************/

/*
 *@@ fntCheckFiles:
 *
 *@@changed V0.9.19 (2002-04-14) [umoeller]: fixed crash with bad parent handles, thanks Paul Ratcliffe
 *@@changed V1.0.9 (2011-10-26) [rwalsh]: replaced ulIsFolder with ulType
 *@@changed V1.0.9 (2011-10-28) [rwalsh]: removed check for folderpos for non-folders - this is not an error
 *@@changed V1.0.9 (2011-10-28) [rwalsh]: added test for misidentified files and folders
 */

void _Optlink fntCheckFiles(PTHREADINFO ptiMyself)
{
    HWND hwndCnr = WinWindowFromID(G_hwndMain, FID_CLIENT);

    G_ulPercentDone = 0;

    DosSetPriority(PRTYS_THREAD,
                   PRTYC_IDLETIME,
                   +31,
                   0);

    CNRINFO CnrInfo;
    WinSendMsg(hwndCnr,
               CM_QUERYCNRINFO,
               (MPARAM)&CnrInfo,
               (MPARAM)sizeof(CnrInfo));

    PNODERECORD preccThis = G_preccVeryFirst;
    // go thru all records
    ULONG   ulCount = 0,
            ulCount2 = 0;
    while (preccThis)
    {
        if ((preccThis->ulHandle) && (preccThis->ulParentHandle))
        {
            APIRET      rc;
            char        *pEnd;
            FILESTATUS3 fs3;

            // if this is a folder, temporarily
            // remove the trailing backslash
            if (   preccThis->ulType == NODETYPE_FOLDER
                && preccThis->szLongName[0] != 0
                && (pEnd = strchr(preccThis->szLongName, 0))
                && pEnd[-1] == '\\'
               )
                pEnd[-1] = 0;
            else
                pEnd = 0;

            rc = DosQueryPathInfo(preccThis->szLongName,
                                  FIL_STANDARD,
                                  &fs3,
                                  sizeof(fs3));

            // restore the trailing backslash; we trust that
            // the original trailing null has not been changed
            if (pEnd)
                pEnd[-1] = '\\';

            if (rc)
            {
                preccThis->ulStatus |= NODESTAT_FILENOTFOUND;
            }
            else
            {
                // test whether files or folders have been misidentified
                // added V1.0.9 (2011-10-28) [rwalsh]
                if (fs3.attrFile & FILE_DIRECTORY)
                {
                    if (preccThis->ulType == NODETYPE_FILE)
                        preccThis->ulStatus |= NODESTAT_ISFOLDERNOTFILE;
                }
                else
                {
                    if (preccThis->ulType == NODETYPE_FOLDER)
                        preccThis->ulStatus |= NODESTAT_ISFILENOTFOLDER;
                }
            }
        }

        UpdateStatusDescr(preccThis);

        G_ulPercentDone = ulCount * 100 / CnrInfo.cRecords;

        if (ptiMyself->fExit)
            break;

        preccThis = preccThis->pNextRecord;
        ulCount++;
        ulCount2++;
    }

    // now re-run thru all records and check if we have
    // valid references to folders... sometimes we have
    // parent handles which are not pointing to a folder
    preccThis = G_preccVeryFirst;
    ULONG cReccs2 = 0;
    while (preccThis)
    {
        BOOL fChanged = FALSE;
        // is this a NODE? (DRIV handle is 0)
        if (preccThis->ulHandle)
        {
            // get the NODE from the record hash table
            PNODETREENODE pNodeThis;
            // V0.9.19 (2002-04-14) [umoeller]
            // fixed crash here if user runs check files before
            // removing handles that have an invalid parent
            if (    (pNodeThis = G_pHandlesBuf->NodeHashTable[preccThis->ulHandle])
                 && (pNodeThis->pNode)
                 && (pNodeThis->pNode->usParentHandle)
               )
            {
                // record has parent:
                PNODERECORD pParentRec;
                if (pParentRec = G_RecordHashTable[pNodeThis->pNode->usParentHandle])
                {
                    if (pParentRec->ulType != NODETYPE_FOLDER)
                    {
                        // parent record is not a folder:
                        preccThis->ulStatus |= NODESTAT_PARENTNOTFOLDER;
                        fChanged = TRUE;
                    }
                }
            }

        } // end if (preccThis->ulHandle)

        // check if we have abstracts assigned to a non-folder
        // changed V1.0.9 (2011-10-28) [rwalsh]: removed check for folderpos for
        //                                       non-folders - this is *not* an error
        if (preccThis->ulType != NODETYPE_FOLDER)
        {
            if (preccThis->cAbstracts)
            {
                preccThis->ulStatus |= NODESTAT_ABSTRACTSNOTFOLDER;
                fChanged = TRUE;
            }
        }

        if (fChanged)
            UpdateStatusDescr(preccThis);

        // next record
        preccThis = preccThis->pNextRecord;

    } // end while (preccThis)

    CalcHandleCounts();
    WinPostMsg(G_hwndMain,
               WM_USER + 1,
               (MPARAM)0, 0);
}

/* ******************************************************************
 *
 *   Container sort procs
 *
 ********************************************************************/

SHORT EXPENTRY fnMainCompareIndex(PNODERECORD p1, PNODERECORD p2, PVOID pStorage)
{
    return (CompareULongs(p1, p2, FIELDOFFSET(NODERECORD, ulIndex)));
}

//@@changed V1.0.9 (2011-11-02) [rwalsh]:  put errors at the top of the listing
SHORT EXPENTRY fnMainCompareStatus(PNODERECORD p1, PNODERECORD p2, PVOID pStorage)
{
    if (p1->ulStatus == p2->ulStatus)
        return 0;
    if (!p1->ulStatus)
        return 1;
    if (!p2->ulStatus)
        return -1;
    return (CompareStrings(p1, p2, FIELDOFFSET(NODERECORD, pszStatusDescr)));
}

SHORT EXPENTRY fnMainCompareType(PNODERECORD p1, PNODERECORD p2, PVOID pStorage)
{
    return (CompareULongs(p1, p2, FIELDOFFSET(NODERECORD, ulType)));
}

SHORT EXPENTRY fnMainCompareHandle(PNODERECORD p1, PNODERECORD p2, PVOID pStorage)
{
    return (CompareULongs(p1, p2, FIELDOFFSET(NODERECORD, ulHandle)));
}

SHORT EXPENTRY fnMainCompareParent(PNODERECORD p1, PNODERECORD p2, PVOID pStorage)
{
    return (CompareULongs(p1, p2, FIELDOFFSET(NODERECORD, ulParentHandle)));
}

SHORT EXPENTRY fnMainCompareShortName(PNODERECORD p1, PNODERECORD p2, PVOID pStorage)
{
    return (CompareStrings(p1, p2, FIELDOFFSET(NODERECORD, pszShortNameCopy)));
}

SHORT EXPENTRY fnMainCompareChildren(PNODERECORD p1, PNODERECORD p2, PVOID pStorage)
{
    return (-CompareULongs(p1, p2, FIELDOFFSET(NODERECORD, cChildren)));
}

SHORT EXPENTRY fnMainCompareDuplicates(PNODERECORD p1, PNODERECORD p2, PVOID pStorage)
{
    return (-CompareULongs(p1, p2, FIELDOFFSET(NODERECORD, cDuplicates)));
}

SHORT EXPENTRY fnMainCompareReferences(PNODERECORD p1, PNODERECORD p2, PVOID pStorage)
{
    return (-CompareStrings(p1, p2, FIELDOFFSET(NODERECORD, pszRefcsDescr)));
}

SHORT EXPENTRY fnMainCompareLongName(PNODERECORD p1, PNODERECORD p2, PVOID pStorage)
{
    return (CompareStrings(p1, p2, FIELDOFFSET(NODERECORD, pszLongName)));
}

//@@added V1.0.9 (2011-11-02) [rwalsh]:
SHORT EXPENTRY fnMainCompareExtension(PNODERECORD p1, PNODERECORD p2, PVOID pStorage)
{
    LONG lRtn = CompareExtension(p1, p2, FIELDOFFSET(NODERECORD, pszShortNameCopy));
    if (!lRtn)
        lRtn = CompareStrings(p1, p2, FIELDOFFSET(NODERECORD, pszLongName));
    return (SHORT)lRtn;
}

//@@added V1.0.9 (2011-11-02) [rwalsh]:
SHORT EXPENTRY fnMainCompareIcon(PNODERECORD p1, PNODERECORD p2, PVOID pStorage)
{
    if (p1->icon == p2->icon)
        return 0;

    LONG    p1Ndx = 0,
            p2Ndx = 0;

    while (p1Ndx < sizeof(G_iconSort) / sizeof(G_iconSort[0]))
    {
        if (p1->icon == *(G_iconSort[p1Ndx]))
            break;
        p1Ndx++;
    }

    while (p2Ndx < sizeof(G_iconSort) / sizeof(G_iconSort[0]))
    {
        if (p2->icon == *(G_iconSort[p2Ndx]))
            break;
        p2Ndx++;
    }

    return (SHORT)(p1Ndx - p2Ndx);
}

/* ******************************************************************
 *
 *   View object IDs
 *
 ********************************************************************/

SHORT EXPENTRY fnObjIdsCompareIndex(POBJIDRECORD p1, POBJIDRECORD p2, PVOID pStorage)
{
    return (CompareULongs(p1, p2, FIELDOFFSET(OBJIDRECORD, ulIndex)));
}

SHORT EXPENTRY fnObjIdsCompareStatus(POBJIDRECORD p1, POBJIDRECORD p2, PVOID pStorage)
{
    return (CompareStrings(p1, p2, FIELDOFFSET(OBJIDRECORD, pszStatus)));
}

SHORT EXPENTRY fnObjIdsCompareIDs(POBJIDRECORD p1, POBJIDRECORD p2, PVOID pStorage)
{
    return (CompareStrings(p1, p2, FIELDOFFSET(OBJIDRECORD, pcszID)));
}

SHORT EXPENTRY fnObjIdsCompareHandles(POBJIDRECORD p1, POBJIDRECORD p2, PVOID pStorage)
{
    return (CompareULongs(p1, p2, FIELDOFFSET(OBJIDRECORD, ulHandle)));
}

SHORT EXPENTRY fnObjIdsCompareLongNames(POBJIDRECORD p1, POBJIDRECORD p2, PVOID pStorage)
{
    return (CompareStrings(p1, p2, FIELDOFFSET(OBJIDRECORD, pszLongName)));
}

/*
 *@@ CheckObjIdsSortItem:
 *
 *@@added V0.9.16 (2001-09-29) [umoeller]
 */

VOID CheckObjIdsSortItem(HWND hwndFrame, USHORT usCmd)
{
    HWND    hmenuMain = WinWindowFromID(hwndFrame, FID_MENU);

    static USHORT usLastCmd = 0;

    if (usLastCmd)
        WinCheckMenuItem(hmenuMain, usLastCmd, FALSE);
    if (usLastCmd != usCmd)
    {
        WinCheckMenuItem(hmenuMain, usCmd, TRUE);
        usLastCmd = usCmd;
    }
}

/*
 *@@ SetObjIDsSort:
 *
 *@@changed (2005-02-24) [chennecke]: renamed IDM... identifiers to ID_FXM...
 */

VOID SetObjIDsSort(HWND hwndFrame,
                   USHORT usCmd)
{
    PVOID pvSortFunc = NULL;
    switch (usCmd)
    {
        case ID_FXMI_SORT_INDEX:
            pvSortFunc = (PVOID)fnObjIdsCompareIndex;
        break;

        case ID_FXMI_SORT_STATUS:
            pvSortFunc = (PVOID)fnObjIdsCompareStatus;
        break;

        case ID_FXMI_SORT_ID:
            pvSortFunc = (PVOID)fnObjIdsCompareIDs;
        break;

        case ID_FXMI_SORT_HANDLE:
            pvSortFunc = (PVOID)fnObjIdsCompareHandles;
        break;

        case ID_FXMI_SORT_LONGNAME:
            pvSortFunc = (PVOID)fnObjIdsCompareLongNames;
        break;
    }

    if (pvSortFunc)
    {
        HPOINTER hptrOld = winhSetWaitPointer();

        WinSendDlgItemMsg(hwndFrame, FID_CLIENT,
                          CM_SORTRECORD,
                          (MPARAM)pvSortFunc,
                          0);

        CheckObjIdsSortItem(hwndFrame, usCmd);

        WinSetPointer(HWND_DESKTOP, hptrOld);
    }
}

/*
 *@@ fnwpSubclassedObjIDsFrame:
 *
 *@@added V0.9.16 (2001-09-29) [umoeller]
 *@@changed V1.0.4 (2005-02-24) [chennecke]: renamed IDM... identifiers to ID_FXM...
 */

MRESULT EXPENTRY fnwpSubclassedObjIDsFrame(HWND hwndFrame, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    switch (msg)
    {
        case WM_SYSCOMMAND:
            if (SHORT1FROMMP(mp1) == SC_CLOSE)
            {
                // intercept close since the original would
                // post WM_QUIT
                winhSaveWindowPos(hwndFrame,
                                  G_hiniUser,
                                  INIAPP,
                                  INIKEY_OBJIDSWINPOS);
                WinDestroyWindow(hwndFrame);
                G_hwndObjIDsFrame = NULLHANDLE;
            }
            else
                mrc = G_fnwpObjIDsFrameOrig(hwndFrame, msg, mp1, mp2);
        break;

        case WM_COMMAND:
        {
            USHORT  usCmd = SHORT1FROMMP(mp1);
            switch (usCmd)
            {
                case ID_FXMI_CLOSETHIS:
                    WinPostMsg(hwndFrame,
                               WM_SYSCOMMAND,
                               (MPARAM)SC_CLOSE,
                               0);
                break;

                case ID_FXMI_SORT_INDEX:
                case ID_FXMI_SORT_STATUS:
                case ID_FXMI_SORT_ID:
                case ID_FXMI_SORT_HANDLE:
                case ID_FXMI_SORT_LONGNAME:
                    SetObjIDsSort(hwndFrame, usCmd);
                break;

                default:
                    StandardCommands(hwndFrame, usCmd);
            }
        }
        break;

        default:
            mrc = G_fnwpObjIDsFrameOrig(hwndFrame, msg, mp1, mp2);
    }

    return mrc;
}

/*
 *@@ ViewObjectIDs:
 *
 *@@added V0.9.16 (2001-09-29) [umoeller]
 *@@changed V1.0.4 (2005-02-24) [chennecke]: replaced hard-coded strings with nlsGetString() calls
 *                                           renamed IDM... identifiers to ID_FXM...
 */

VOID ViewObjectIDs(VOID)
{
    EXTFRAMECDATA xfd =
        {
            NULL,               // pswpFrame
            FCF_TITLEBAR
               | FCF_SYSMENU
               | FCF_MINMAX
               | FCF_SIZEBORDER
               | FCF_ICON
//                | FCF_MENU
               | FCF_TASKLIST,
            XFCF_STATUSBAR,
            0,                      // frame style
            nlsGetString(ID_FXDI_OBJIDS),
            1,     // icon resource
            WC_CONTAINER,
            CCS_MINIICONS | CCS_READONLY | CCS_EXTENDSEL
               | WS_VISIBLE,
            0,
            NULL
        };

    HWND    hwndObjIDsCnr;

    if (!G_hwndObjIDsFrame)
    {
        // not yet created:

        if (    (G_hwndObjIDsFrame = winhCreateExtStdWindow(&xfd,
                                                            &hwndObjIDsCnr))
             && (hwndObjIDsCnr)
           )
        {
            HWND hwndCnr = WinWindowFromID(G_hwndObjIDsFrame, FID_CLIENT);

            // subclass frame for supporting msgs
            G_fnwpObjIDsFrameOrig = WinSubclassWindow(G_hwndObjIDsFrame,
                                                      fnwpSubclassedObjIDsFrame);

            // load the different menu bar explicitly
            WinLoadMenu(G_hwndObjIDsFrame,
                        G_hmodNLS,
                        ID_FXM_OBJIDS);

            CheckObjIdsSortItem(G_hwndObjIDsFrame, ID_FXMI_SORT_INDEX);

            // set up data for Details view columns
            XFIELDINFO      xfi[5];
            int             i = 0,
                            iSplitAfter;

            xfi[i].ulFieldOffset = FIELDOFFSET(OBJIDRECORD, ulIndex);
            xfi[i].pszColumnTitle = nlsGetString(ID_FXSI_NUMBER);
            xfi[i].ulDataType = CFA_ULONG;
            xfi[i++].ulOrientation = CFA_RIGHT | CFA_TOP;

            xfi[i].ulFieldOffset = FIELDOFFSET(OBJIDRECORD, pszStatus);
            xfi[i].pszColumnTitle = nlsGetString(ID_FXSI_STATUS);
            xfi[i].ulDataType = CFA_STRING;
            xfi[i++].ulOrientation = CFA_LEFT;

            iSplitAfter = i;
            xfi[i].ulFieldOffset = FIELDOFFSET(OBJIDRECORD, pcszID);
            xfi[i].pszColumnTitle = nlsGetString(ID_FXSI_ID);
            xfi[i].ulDataType = CFA_STRING;
            xfi[i++].ulOrientation = CFA_LEFT;

            xfi[i].ulFieldOffset = FIELDOFFSET(OBJIDRECORD, pcszHandle);
            xfi[i].pszColumnTitle = nlsGetString(ID_FXSI_HANDLE);
            xfi[i].ulDataType = CFA_STRING;
            xfi[i++].ulOrientation = CFA_RIGHT;

            xfi[i].ulFieldOffset = FIELDOFFSET(OBJIDRECORD, pszLongName);
            xfi[i].pszColumnTitle = nlsGetString(ID_FXSI_LONGNAME);
            xfi[i].ulDataType = CFA_STRING;
            xfi[i++].ulOrientation = CFA_LEFT;

            cnrhClearFieldInfos(hwndCnr, FALSE);

            PFIELDINFO pFieldInfoLast
                = cnrhSetFieldInfos(hwndCnr,
                                    &xfi[0],
                                    i,
                                    TRUE,          // draw lines
                                    iSplitAfter);            // column index to return
            BEGIN_CNRINFO()
            {
                // set split bar
                cnrhSetSplitBarAfter(pFieldInfoLast);
                cnrhSetSplitBarPos(300);
                // switch view
                cnrhSetView(CV_DETAIL | CV_MINI | CA_DETAILSVIEWTITLES | CA_DRAWICON);
            } END_CNRINFO(hwndCnr)

            winhSetWindowFont(hwndCnr, NULL);

            if (!winhRestoreWindowPos(G_hwndObjIDsFrame,
                                      G_hiniUser,
                                      INIAPP,
                                      INIKEY_OBJIDSWINPOS,
                                      SWP_SHOW | SWP_ACTIVATE | SWP_MOVE | SWP_SIZE))
                WinSetWindowPos(G_hwndObjIDsFrame,
                                HWND_TOP,
                                10, 10, 500, 500,
                                SWP_SHOW | SWP_ACTIVATE | SWP_MOVE | SWP_SIZE);

            // now go create the records
            POBJIDRECORD precFirst = (POBJIDRECORD)cnrhAllocRecords(hwndCnr,
                                                                    sizeof(OBJIDRECORD),
                                                                    G_cObjIDs),
                         precThis = precFirst;

            POBJID      pobjidThis = (POBJID)treeFirst(G_ObjIDsTree);

            ULONG ul,
                  cRecords = 0;
            for (ul = 0;
                 ul < G_cObjIDs;
                 ul++)
            {
                XSTRING strStatus;
                xstrInit(&strStatus, 0);

                ULONG ulHandle = pobjidThis->Tree.ulKey;        // 32 bits

                precThis->ulIndex = cRecords++;
                precThis->pcszID = pobjidThis->pcszID;
                precThis->ulHandle = ulHandle;
                sprintf(precThis->szHandle,
                        "0x%lX",
                        ulHandle);
                precThis->pcszHandle = precThis->szHandle;

                if (HIUSHORT(ulHandle) == G_pHandlesBuf->usHiwordFileSystem)
                {
                    // file-system object: check handle from table
                    PNODERECORD p;
                    if (p = G_RecordHashTable[LOUSHORT(ulHandle)])
                        precThis->pszLongName = p->szLongName;
                    else
                        Append(&strStatus, nlsGetString(ID_FXSI_ERROR_INVALIDHANDLE), ";");
                }
                else
                {
                    // abstract:
                    USHORT usFolder;
                    if (!(usFolder = G_ausAbstractFolders[LOUSHORT(ulHandle)]))
                    {
                        Append(&strStatus, nlsGetString(ID_FXSI_ERROR_ABSFOLDERHANDLELOST), ";");
                    }
                    else
                    {
                        // OK, we got the folder handle:
                        // check if that handle is valid
                        PNODERECORD prec;
                        if (prec = G_RecordHashTable[usFolder])
                            sprintf(precThis->szLongName,
                                    nlsGetString(ID_FXSI_LONGNAME_ABSTRACTVALID),
                                    usFolder,
                                    prec->szLongName);
                        else
                        {
                            // invalid folder handle:
                            sprintf(precThis->szLongName,
                                    nlsGetString(ID_FXSI_LONGNAME_ABSTRACTINVALID),
                                    usFolder);
                            Append(&strStatus, nlsGetString(ID_FXSI_ERROR_INVALIDFOLDERHANDLE), ";");
                        }

                        precThis->pszLongName = precThis->szLongName;
                    }
                }

                if (strStatus.ulLength)
                {
                    precThis->pszStatus = strStatus.psz;
                    precThis->recc.flRecordAttr |= CRA_PICKED;
                }

                // reverse linkage between objid and record
                pobjidThis->pRecord = precThis;

                if (!(pobjidThis = (POBJID)treeNext((TREE*)pobjidThis)))
                    break;

                if (!(precThis = (POBJIDRECORD)precThis->recc.preccNextRecord))
                    break;
            }

            cnrhInsertRecords(hwndCnr,
                              NULL,
                              (PRECORDCORE)precFirst,
                              TRUE,
                              NULL,
                              CRA_RECORDREADONLY,
                              cRecords);

            CHAR sz2[100], sz3[100];
            SetStatusBarText(G_hwndObjIDsFrame,
                             nlsGetString(ID_FXSI_STATBAR_LOADEDINSERTED),
                             nlsThousandsULong(sz2, G_cObjIDs, G_cThousands[0]),
                             nlsThousandsULong(sz3, cRecords, G_cThousands[0]));
        }
    }
    else
        WinSetActiveWindow(HWND_DESKTOP, G_hwndObjIDsFrame);
}

/* ******************************************************************
 *
 *   Main container window proc
 *
 ********************************************************************/

/*
 *@@ fnwpSubclassedMainCnr:
 *
 */

MRESULT EXPENTRY fnwpSubclassedMainCnr(HWND hwndCnr, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    switch (msg)
    {
        case WM_CLOSE:
            winhSaveWindowPos(G_hwndMain,
                              G_hiniUser,
                              INIAPP,
                              INIKEY_MAINWINPOS);
            mrc = G_pfnwpCnrOrig(hwndCnr, msg, mp1, mp2);
        break;

        default:
            mrc = G_pfnwpCnrOrig(hwndCnr, msg, mp1, mp2);
    }

    return mrc;
}

/*
 *@@ SetupMainCnr:
 *
 *@@changed V1.0.4 (2005-02-24) [chennecke]: replaced hard-coded strings with nlsGetString() calls
 *@@changed V1.0.9 (2011-10-29) [rwalsh]: added icon column
 *@@changed V1.0.9 (2011-11-07) [rwalsh]: reordered columns, moved default splitbar position rightward
 */

VOID SetupMainCnr(HWND hwndCnr)
{
    // set up data for Details view columns
    XFIELDINFO      xfi[12];
    int             i = 0,
                    iSplitAfter = 0;

    xfi[i].ulFieldOffset = FIELDOFFSET(NODERECORD, ulIndex);
    xfi[i].pszColumnTitle = nlsGetString(ID_FXSI_NUMBER);
    xfi[i].ulDataType = CFA_ULONG;
    xfi[i++].ulOrientation = CFA_RIGHT | CFA_TOP;

    xfi[i].ulFieldOffset = FIELDOFFSET(NODERECORD, icon);
    xfi[i].pszColumnTitle = "";
    xfi[i].ulDataType = CFA_BITMAPORICON;
    xfi[i++].ulOrientation = CFA_CENTER | CFA_VCENTER;

    xfi[i].ulFieldOffset = FIELDOFFSET(NODERECORD, pszStatusDescr);
    xfi[i].pszColumnTitle = nlsGetString(ID_FXSI_STATUS);
    xfi[i].ulDataType = CFA_STRING;
    xfi[i++].ulOrientation = CFA_LEFT;

    xfi[i].ulFieldOffset = FIELDOFFSET(NODERECORD, pszHexHandle);
    xfi[i].pszColumnTitle = nlsGetString(ID_FXSI_HANDLE);
    xfi[i].ulDataType = CFA_STRING;
    xfi[i++].ulOrientation = CFA_RIGHT;

    xfi[i].ulFieldOffset = FIELDOFFSET(NODERECORD, pszHexParentHandle);
    xfi[i].pszColumnTitle = nlsGetString(ID_FXSI_PARENT);
    xfi[i].ulDataType = CFA_STRING;
    xfi[i++].ulOrientation = CFA_RIGHT;

    xfi[i].ulFieldOffset = FIELDOFFSET(NODERECORD, pcszType);
    xfi[i].pszColumnTitle = nlsGetString(ID_FXSI_TYPE);
    xfi[i].ulDataType = CFA_STRING;
    xfi[i++].ulOrientation = CFA_LEFT;

    xfi[i].ulFieldOffset = FIELDOFFSET(NODERECORD, pszShortNameCopy);
    xfi[i].pszColumnTitle = nlsGetString(ID_FXSI_SHORTNAME);
    xfi[i].ulDataType = CFA_STRING;
    xfi[i++].ulOrientation = CFA_LEFT;

    iSplitAfter = i;
    xfi[i].ulFieldOffset = FIELDOFFSET(NODERECORD, ulOfs);
    xfi[i].pszColumnTitle = nlsGetString(ID_FXSI_NODEOFS);
    xfi[i].ulDataType = CFA_ULONG;
    xfi[i++].ulOrientation = CFA_RIGHT;

    xfi[i].ulFieldOffset = FIELDOFFSET(NODERECORD, cChildren);
    xfi[i].pszColumnTitle = nlsGetString(ID_FXSI_CHILDREN);
    xfi[i].ulDataType = CFA_ULONG;
    xfi[i++].ulOrientation = CFA_RIGHT;

    xfi[i].ulFieldOffset = FIELDOFFSET(NODERECORD, cDuplicates);
    xfi[i].pszColumnTitle = nlsGetString(ID_FXSI_DUPLICATES);
    xfi[i].ulDataType = CFA_ULONG;
    xfi[i++].ulOrientation = CFA_RIGHT;

    xfi[i].ulFieldOffset = FIELDOFFSET(NODERECORD, pszRefcsDescr);
    xfi[i].pszColumnTitle = nlsGetString(ID_FXSI_REFERENCES);
    xfi[i].ulDataType = CFA_STRING;
    xfi[i++].ulOrientation = CFA_LEFT;

    xfi[i].ulFieldOffset = FIELDOFFSET(NODERECORD, pszLongName);
    xfi[i].pszColumnTitle = nlsGetString(ID_FXSI_LONGNAME);
    xfi[i].ulDataType = CFA_STRING;
    xfi[i++].ulOrientation = CFA_LEFT;

    cnrhClearFieldInfos(hwndCnr, FALSE);

    PFIELDINFO pFieldInfoLast
        = cnrhSetFieldInfos(hwndCnr,
                            &xfi[0],
                            i,
                            TRUE,          // draw lines
                            iSplitAfter);            // column index to return
    BEGIN_CNRINFO()
    {
        // set split bar
        cnrhSetSplitBarAfter(pFieldInfoLast);
        cnrhSetSplitBarPos(400);
        // switch view
        cnrhSetView(CV_DETAIL | CV_MINI | CA_DETAILSVIEWTITLES | CA_DRAWICON);
    } END_CNRINFO(hwndCnr)

    winhSetWindowFont(hwndCnr, NULL);
}

/* ******************************************************************
 *
 *   Main frame window proc
 *
 ********************************************************************/

/*
 *@@ ShowInsertCompleteMsg:
 *
 *@@changed V1.0.4 (2005-02-24) [chennecke]: replaced hard-coded strings with nlsGetString() calls
 *@@changed V1.0.9 (2011-11-06) [rwalsh]: rewrote and renamed, was UpdateStatusBar
 */

VOID ShowInsertCompleteMsg(ULONG cRecs, ULONG cSeconds)
{
    CHAR  sz[256] = "",
          sz2[16],
          sz3[16];
    PSZ   psz = sz;

    if (!cRecs)
    {
        SetStatusBarText(G_hwndMain,
                         nlsGetString(ID_FXSI_ERROR_PARSEHANDLES));
        return;
    }

    psz += sprintf(psz,
                   nlsGetString(ID_FXSI_STATBAR_DONE),
                   nlsThousandsULong(sz2, (ULONG)G_pHandlesBuf->cbData, G_cThousands[0]),
                   nlsThousandsULong(sz3, (ULONG)cRecs, G_cThousands[0]));

    psz += sprintf(psz,
                   nlsGetString(ID_FXSI_STATBAR_SECONDS),
                   cSeconds / 1000);

    if (G_cDuplicatesFound)
        psz += sprintf(psz,
                       nlsGetString(ID_FXSI_STATBAR_DUPWARNING),
                       G_cDuplicatesFound);

    SetStatusBarText(G_hwndMain, sz);
}

/*
 *@@ UpdateMenuItems:
 *
 *@@changed V1.0.4 (2005-02-24) [chennecke]: renamed IDM... identifiers to ID_FXM...
 */

VOID UpdateMenuItems(USHORT usSortCmd)
{
    static usLastCmd = 0;
    HWND    hmenuMain = WinWindowFromID(G_hwndMain, FID_MENU);

    // view menu
    BOOL fEnable = (!G_tidInsertHandlesRunning && !G_tidCheckFilesRunning);
    WinEnableMenuItem(hmenuMain, ID_FXM_ACTIONS,
                      fEnable);
    WinEnableMenuItem(hmenuMain, ID_FXM_SELECT,
                      fEnable);
    WinEnableMenuItem(hmenuMain, ID_FXMI_RESCAN,
                      fEnable);
    WinEnableMenuItem(hmenuMain, ID_FXMI_WRITETOINI,
                      fEnable);

    // disable "object IDs" window if already open
    /* WinEnableMenuItem(hmenuMain, ID_FXMI_VIEW_OBJIDS,
                      (G_hwndObjIDsFrame == NULL)); */

    // disable "duplicates" if we have none
    WinEnableMenuItem(hmenuMain, ID_FXMI_SORT_DUPS,
                      (G_cDuplicatesFound != 0));

    if (usSortCmd)
    {
        if (usLastCmd)
            WinCheckMenuItem(hmenuMain, usLastCmd, FALSE);
        if (usLastCmd != usSortCmd)
        {
            WinCheckMenuItem(hmenuMain, usSortCmd, TRUE);
            usLastCmd = usSortCmd;
        }
    }
}

/*
 *@@ SetMainSort:
 *
 *@@changed V1.0.4 (2005-02-24) [chennecke]: renamed IDM... identifiers to ID_FXM...
 *@@changed V1.0.9 (2011-11-02) [rwalsh]: added Sort by Icon and Extension
 */

VOID SetMainSort(USHORT usCmd)
{
    PVOID pvSortFunc = NULL;
    switch (usCmd)
    {
        case ID_FXMI_SORT_INDEX:
            pvSortFunc = (PVOID)fnMainCompareIndex;
        break;

        case ID_FXMI_SORT_ICON:
            pvSortFunc = (PVOID)fnMainCompareIcon;
        break;

        case ID_FXMI_SORT_STATUS:
            pvSortFunc = (PVOID)fnMainCompareStatus;
        break;

        case ID_FXMI_SORT_TYPE:
            pvSortFunc = (PVOID)fnMainCompareType;
        break;

        case ID_FXMI_SORT_HANDLE:
            pvSortFunc = (PVOID)fnMainCompareHandle;
        break;

        case ID_FXMI_SORT_PARENT:
            pvSortFunc = (PVOID)fnMainCompareParent;
        break;

        case ID_FXMI_SORT_SHORTNAME:
            pvSortFunc = (PVOID)fnMainCompareShortName;
        break;

        //@@added V1.0.9 (2011-11-02) [rwalsh]:
        case ID_FXMI_SORT_EXTENSION:
            pvSortFunc = (PVOID)fnMainCompareExtension;
        break;

        case ID_FXMI_SORT_CHILDREN:
            pvSortFunc = (PVOID)fnMainCompareChildren;
        break;

        case ID_FXMI_SORT_DUPS:
            pvSortFunc = (PVOID)fnMainCompareDuplicates;
        break;

        case ID_FXMI_SORT_REFCS:
            pvSortFunc = (PVOID)fnMainCompareReferences;
        break;

        case ID_FXMI_SORT_LONGNAME:
            pvSortFunc = (PVOID)fnMainCompareLongName;
        break;
    }

    if (pvSortFunc)
    {
        HPOINTER hptrOld = winhSetWaitPointer();

        WinSendDlgItemMsg(G_hwndMain, FID_CLIENT,
                          CM_SORTRECORD,
                          (MPARAM)pvSortFunc,
                          0);

        UpdateMenuItems(usCmd);
        WinSetPointer(HWND_DESKTOP, hptrOld);
    }
}

/*
 *@@ StartInsertHandles:
 *
 *@@added V0.9.7 (2001-01-21) [umoeller]
 *@@changed V1.0.4 (2005-02-24) [chennecke]: renamed IDM... identifiers to ID_FXM...
 */

VOID StartInsertHandles(HWND hwndCnr)
{
    // start collect thread
    thrCreate(&G_tiInsertHandles,
              fntInsertHandles,
              &G_tidInsertHandlesRunning,
              "InsertHandles",
              THRF_WAIT | THRF_PMMSGQUEUE,
              hwndCnr);     // thread param

    // start timer
    WinStartTimer(WinQueryAnchorBlock(G_hwndMain),
                  G_hwndMain,
                  TIMERID_THREADRUNNING,
                  200);

    UpdateMenuItems(ID_FXMI_SORT_INDEX);
}

/*
 *@@ CreateDeferredNuke:
 *      stores a new entry in the global list of objects
 *      to be nuked on "Write".
 *
 *@@added V0.9.7 (2001-01-25) [umoeller]
 *@@changed V0.9.9 (2001-04-07) [umoeller]: fixed bad alloc error
 */

VOID CreateDeferredNuke(HINI hini,              // in: INI file
                        const char *pcszApp,    // in: INI app
                        const char *pcszKey)    // in: INI key or NULL
{
    // PDEFERREDINI pNuke = (PDEFERREDINI)malloc(sizeof(PDEFERREDINI));
                // whow V0.9.9 (2001-04-07) [umoeller]
    PDEFERREDINI pNuke = NEW(DEFERREDINI);
    if (pNuke)
    {
        pNuke->hini = hini;
        pNuke->pcszApp = pcszApp;
        pNuke->pszKey = strdup(pcszKey);
        pNuke->pvData = NULL;               // nuke
        pNuke->cbData = 0;                  // nuke
        lstAppendItem(&G_llDeferredNukes,
                      pNuke);
    }
}

/*
 *@@ GetAbstracts:
 *      loads the ULONG array of abstract objects
 *      for the specified record.
 *
 *      Returns a new buffer, which must be free()'d.
 *
 *@@added V0.9.7 (2001-01-25) [umoeller]
 */

PULONG GetAbstracts(PNODERECORD prec,
                    PSZ pszHandleShort,     // out: key name
                    PULONG pcAbstracts)     // out: array count (NOT array size)
{
    ULONG cbFolderContent = 0;
    sprintf(pszHandleShort, "%lX", prec->ulHandle);
                    // yes, WPS uses "AB" if less than
                    // four digits...
    PSZ pszAbstracts = prfhQueryProfileData(G_hiniUser,
                                            WPINIAPP_FDRCONTENT, // "PM_Abstract:FldrContent",
                                            pszHandleShort,
                                            &cbFolderContent);
    if (pszAbstracts && cbFolderContent)
        // this is an array of ULONGs really
        *pcAbstracts = cbFolderContent / sizeof(ULONG);

    return ((PULONG)pszAbstracts);
}

/*
 *@@ NukeAbstracts:
 *      nukes all abstract objects in OS2.INI related to the
 *      specified handle record.
 *
 *@@added V0.9.7 (2001-01-25) [umoeller]
 */

VOID NukeAbstracts(PNODERECORD prec,
                   PULONG pulNuked)             // out: abstracts nuked
{
    // check abstracts in OS2.INI
    CHAR    szHandleShort[10];
    ULONG   cAbstracts = 0;
    PULONG  paulAbstracts = GetAbstracts(prec,
                                         szHandleShort,
                                         &cAbstracts);
    if (paulAbstracts && cAbstracts)
    {
        ULONG ul;
        for (ul = 0;
             ul < cAbstracts;
             ul++)
        {
            // get abstract handle
            ULONG ulAbstractThis = paulAbstracts[ul];

            // nuke INI entries for this
            CHAR szAbstract[10];
            sprintf(szAbstract, "%lX", ulAbstractThis);

            // invalidate the hash table entry V0.9.16 (2001-09-29) [umoeller]
            G_ausAbstractFolders[LOUSHORT(ulAbstractThis)] = 0;

            CreateDeferredNuke(G_hiniUser,
                               "PM_Abstract:Objects",
                               szAbstract);
            CreateDeferredNuke(G_hiniUser,
                               "PM_Abstract:Icons",
                               szAbstract);
        }

        *pulNuked += cAbstracts;

        // nuke folder content entry
        CreateDeferredNuke(G_hiniUser,
                           WPINIAPP_FDRCONTENT, // "PM_Abstract:FldrContent",
                           szHandleShort);
        free(paulAbstracts);

        prec->cAbstracts = 0;
        prec->ulStatus &= ~NODESTAT_ABSTRACTSNOTFOLDER;
        UpdateStatusDescr(prec);
    }
}

/*
 *@@ NukeFolderPoses:
 *      removes all folderpos entries in OS2.INI related
 *      to the specified handle record.
 *
 *      Note:  the value that follows the '@' in a folderpos entry
 *      is the view ID followed by a sequence number, usually zero
 *      (you can have multiple folderpos's for a view).
 *      E.g. @1010 is the first entry for a Tree view of this folder.
 *
 *      Folderpos entries are also created when a file or folder's
 *      Properties notebook is moved or resized (i.e. xxxxx@20).
 *      Because having a folderpos for a non-folder is NOT an error,
 *      the former NODESTAT_FOLDERPOSNOTFOLDER error has been removed.
 *
 *@@added V0.9.7 (2001-01-25) [umoeller]
 *@@changed V1.0.9 (2011-10-28) [rwalsh]: removed NODESTAT_FOLDERPOSNOTFOLDER
 */

VOID NukeFolderPoses(PNODERECORD prec,
                     const char *pcszFolderPoses,
                     PULONG pulNuked)                // out: entries deleted
{
    // check folderpos entries in OS2.INI...
    // these are decimal, followed by "@" and the key described above.
    // XWP adds keys to this too.

    const char *pcszFolderPosThis = pcszFolderPoses;
    CHAR szDecimalHandle[30];
    sprintf(szDecimalHandle,
            "%d@",
            // compose full handle (hiword)
            prec->ulHandle
                  | (G_pHandlesBuf->usHiwordFileSystem << 16L));

    ULONG DecimalLen = strlen(szDecimalHandle);
    while (*pcszFolderPosThis)
    {
        ULONG ulLengthThis = strlen(pcszFolderPosThis);
        if (!ulLengthThis)
            break;
        else
        {
            ULONG cbComp = DecimalLen;
            if (ulLengthThis < cbComp)
                cbComp = ulLengthThis;
            if (memcmp(pcszFolderPosThis,
                       szDecimalHandle,
                       cbComp)
                   == 0)
            {
                // matches:
                CreateDeferredNuke(G_hiniUser,
                                   WPINIAPP_FOLDERPOS, // "PM_Workplace:FolderPos",
                                   pcszFolderPosThis);
                (*pulNuked)++;
            }
        }

        pcszFolderPosThis += ulLengthThis + 1;
    }

    prec->fFolderPos = FALSE;
    UpdateStatusDescr(prec);
}

/*
 *@@ NukeObjID:
 *      removes all object ID entries in OS2.INI related
 *      to the specified handle record.
 *
 *@@added V0.9.7 (2001-01-25) [umoeller]
 *@@changed V1.0.4 (2005-02-24) [chennecke]: replaced hard-coded strings with nlsGetString() calls
 */

VOID NukeObjID(PNODERECORD prec,
               PULONG pulNuked)                // out: entries deleted
{
    if (prec->pObjID)
    {
        // this node has an object ID assigned:
        CreateDeferredNuke(G_hiniUser,
                           WPINIAPP_LOCATION, // "PM_Workplace:Location",
                           prec->pObjID->pcszID);
        (*pulNuked)++;
    }

    // if the objids window is currently open,
    // remove the record
    if (G_hwndObjIDsFrame)
    {
        if (prec->pObjID->pRecord)
            WinSendMsg(WinWindowFromID(G_hwndObjIDsFrame, FID_CLIENT),
                       CM_REMOVERECORD,
                       (MPARAM)&prec->pObjID->pRecord,
                       MPFROM2SHORT(1,
                                    CMA_FREE | CMA_INVALIDATE));
    }

    // remove the objid from the tree
    if (!treeDelete(&G_ObjIDsTree,
                    &G_cObjIDs,
                    (TREE*)prec->pObjID))
    {
        CHAR sz2[30];

        if (G_hwndObjIDsFrame)
            SetStatusBarText(G_hwndObjIDsFrame,
                             nlsGetString(ID_FXSI_STATBAR_IDSLOADED),
                             nlsThousandsULong(sz2, G_cObjIDs, G_cThousands[0]));
    }
    free(prec->pObjID);

    prec->pObjID = NULL;

    UpdateStatusDescr(prec);
}

/*
 *@@ MoveAbstracts:
 *      moves the abstract records for the handles
 *      on the linked list to the desktop.
 *
 *@@added V0.9.7 (2001-01-25) [umoeller]
 */

VOID MoveAbstracts(PLINKLIST pll,               // in: linked list of NODERECORDs
                   PULONG pulAbstractsMoved)    // out: abstracts moved
{
    if (G_fDesktopLoaded == FALSE)
    {
        // first call: get abstracts of desktop
        G_paulAbstractsTarget = GetAbstracts(G_precDesktop,
                                             G_szHandleDesktop,
                                             &G_cAbstractsDesktop);
                    // this can be NULL if none exist
        G_fDesktopLoaded = TRUE;
    }

    // OK, go thru records.
    PLISTNODE pNode = lstQueryFirstNode(pll);
    while (pNode)
    {
        PNODERECORD prec = (PNODERECORD)pNode->pItemData;

        if (prec->cAbstracts)
        {
            // prec now has the handle entry of an item
            // with abstract objects... get these!
            CHAR    szHandleSource[10];
            ULONG   cAbstractsSource = 0;
            PULONG  paulAbstractsSource = GetAbstracts(prec,
                                                       szHandleSource,
                                                       &cAbstractsSource);
            if (paulAbstractsSource && cAbstractsSource)
            {
                // append to desktop's array
                ULONG cAbstractsTargetNew
                    = G_cAbstractsDesktop + cAbstractsSource;
                G_paulAbstractsTarget
                    = (PULONG)realloc(G_paulAbstractsTarget,
                                      sizeof(ULONG) * cAbstractsTargetNew);
                if (G_paulAbstractsTarget)
                {
                    // append to global array to be written back later
                    memcpy(&G_paulAbstractsTarget[G_cAbstractsDesktop],
                                    // target: after existing
                           paulAbstractsSource,
                                    // source: from folder
                           cAbstractsSource * sizeof(ULONG));
                    // replace existing count
                    G_cAbstractsDesktop = cAbstractsTargetNew;

                    CreateDeferredNuke(G_hiniUser,
                                       WPINIAPP_FDRCONTENT, // "PM_Abstract:FldrContent",
                                       szHandleSource);

                    *pulAbstractsMoved += cAbstractsSource;
                    G_cTotalAbstractsMoved += cAbstractsSource;

                    prec->cAbstracts = 0;
                    prec->ulStatus &= ~NODESTAT_ABSTRACTSNOTFOLDER;
                    UpdateStatusDescr(prec);
                }
            }
        }

        pNode = pNode->pNext;
    }

    G_precDesktop->cAbstracts += (*pulAbstractsMoved);
    UpdateStatusDescr(G_precDesktop);
}

/*
 *@@ MarkAllOrphans:
 *      starting with prec, this marks all records
 *      as orphaned which have ulParentHandle as their
 *      parent handle.
 *
 *      This recurses for marked records, because if
 *      a record is orphaned, its dependent records
 *      will be too.
 *
 *@@added V0.9.7 (2001-01-21) [umoeller]
 *@@changed V0.9.9 (2001-04-07) [umoeller]: fixed multiple recursions
 */

VOID MarkAllOrphans(PNODERECORD prec,         // in: rec to start with (advanced)
                    ULONG ulParentHandle)
{
    while (prec)
    {
        // check if record references our one as a parent
        if (prec->ulParentHandle == ulParentHandle)
        {
            prec->ulStatus |= NODESTAT_INVALID_PARENT;
            UpdateStatusDescr(prec);

            // recurse
            MarkAllOrphans(prec,
                           prec->ulHandle);
        }

        // next record
        prec = prec->pNextRecord;
    }
}

/*
 *@@ MarkPendingOrphans:
 *      called to mark records that will be
 *      orphaned by a pending deletion
 *
 *@@added V1.0.9 (2011-11-01) [rwalsh]:
 */
 
VOID MarkPendingOrphans(PNODERECORD prec,
                        ULONG ulParentHandle,
                        RCArray* prca)
{
    if (!prec)
        return;

    // special case for drives and root folders:
    // mark everything that isn't invalid as orphaned
    if (   prec->ulType == NODETYPE_DRIVE
        || prec->ulDriveNdx
       )
    {
        for (prec = prec->pNextRecord;
             prec && prec->ulType != NODETYPE_DRIVE;
             prec = prec->pNextRecord)
        {
            if (prec->ulStatus)
                continue;

            prec->cOrphaned++;

            if (prec->cOrphaned == 1)
            {
                UpdateStatusDescr(prec);
                if (prca)
                    prca->Append(&prec->recc);
            }
        }

        return;
    }

    // for other types records, use recursion
    while (prec && prec->ulType != NODETYPE_DRIVE)
    {
        // check if record references our one as a parent
        if (   prec->ulParentHandle == ulParentHandle
            && !prec->ulStatus)
        {
            prec->cOrphaned++;

            if (prec->cOrphaned == 1)
            {
                if (prec->ulAction != NODEACTION_DELETE)
                    G_cHandlesOrphaned++;
                UpdateStatusDescr(prec);
                if (prca)
                    prca->Append(&prec->recc);
            }

            // recurse
            MarkPendingOrphans(prec,
                               prec->ulHandle,
                               prca);
        }

        prec = prec->pNextRecord;
    }
}

/*
 *@@ UmarkPendingOrphans:
 *      called to unmark records that would have been
 *      orphaned if a pending deletion hadn't been cancelled
 *
 *@@added V1.0.9 (2011-11-01) [rwalsh]:
 */

VOID UnmarkPendingOrphans(PNODERECORD prec,
                          ULONG ulParentHandle,
                          RCArray* prca)
{
    if (!prec)
        return;

    // special case for drives and root folders:
    // unmark everything that isn't invalid
    if (   prec->ulType == NODETYPE_DRIVE
        || prec->ulDriveNdx
       )
    {
        for (prec = prec->pNextRecord;
             prec && prec->ulType != NODETYPE_DRIVE;
             prec = prec->pNextRecord)
        {
            if (prec->ulStatus)
                continue;

            if (prec->cOrphaned)
                prec->cOrphaned--;

            if (!prec->cOrphaned)
            {
                UpdateStatusDescr(prec);
                if (prca)
                    prca->Append(&prec->recc);
            }
        }

        return;
    }

    // for other types records, use recursion
    while (prec && prec->ulType != NODETYPE_DRIVE)
    {
        // check if record references our one as a parent
        if (   prec->ulParentHandle == ulParentHandle
            && !prec->ulStatus)
        {
            if (prec->cOrphaned)
                prec->cOrphaned--;

            if (!prec->cOrphaned)
            {
                if (prec->ulAction != NODEACTION_DELETE)
                    G_cHandlesOrphaned--;
                UpdateStatusDescr(prec);
                if (prca)
                    prca->Append(&prec->recc);
            }

            // recurse
            UnmarkPendingOrphans(prec,
                                 prec->ulHandle,
                                 prca);
        }

        prec = prec->pNextRecord;
    }
}

/*
 *@@ RemoveHandles:
 *
 *@@added V0.9.7 (2001-01-21) [umoeller]
 *@@changed V0.9.9 (2001-04-07) [umoeller]: greatly reordered, fixed many crashes
 *@@changed V1.0.4 (2005-02-24) [chennecke]: replaced hard-coded strings with nlsGetString() calls
 */

ULONG RemoveHandles(HWND hwndCnr,
                    PLINKLIST pllRecords)       // in: linked list of records to work on
{
    APIRET      arc = NO_ERROR;
    PLISTNODE   pNode;

    ULONG       cAbstractsNuked = 0,
                cFolderPosesNuked = 0,
                cObjIDsNuked = 0;

    // load folderpos entries from OS2.INI
    PSZ pszFolderPoses = NULL;

    if (!(arc = prfhQueryKeysForApp(G_hiniUser,
                                    WPINIAPP_FOLDERPOS, // "PM_Workplace:FolderPos",
                                    &pszFolderPoses)))
    {
        // for each record, remove the corresponding NODE in the global NODE data
        pNode = lstQueryFirstNode(pllRecords);
        while (pNode)
        {
            PNODERECORD precDelete = (PNODERECORD)pNode->pItemData;

            /*
             *   calculate bytes in handles buffer to delete
             *
             */

            PBYTE pbItem = G_pHandlesBuf->pbData + precDelete->ulOfs;
            // address of next node in buffer: depends on whether
            // it's a DRIV or a NODE
            ULONG   cbDelete = 0;
            PBYTE   pbNextItem = 0;
            BOOL    fIsDrive = FALSE;

            if (!memcmp(pbItem, "DRIV", 4))
            {
                // it's a DRIVE node:
                PDRIVE pDrivDelete = (PDRIVE)pbItem;
                cbDelete = sizeof(DRIVE) + strlen(pDrivDelete->szName);
                fIsDrive = TRUE;
            }
            else if (!memcmp(pbItem, "NODE", 4))
            {
                // it's a NODE node:
                PNODE pNodeDelete = (PNODE)pbItem;
                cbDelete = sizeof(NODE) + pNodeDelete->usNameSize;
            }
            else
            {
                arc = ERROR_BAD_FORMAT;
                break;
            }

            // address of next node in buffer:
            pbNextItem = pbItem + cbDelete;

            /*
             *   delete abstracts, folder pos entries
             *
             */

            if (!fIsDrive)
            {
                // regular node:

                // nuke abstracts, folderpos, objids
                NukeAbstracts(precDelete,
                              &cAbstractsNuked);
                NukeFolderPoses(precDelete,
                                pszFolderPoses,
                                &cFolderPosesNuked);
                NukeObjID(precDelete,
                          &cObjIDsNuked);
            }

            /*
             *   delete bytes in handles buffer
             *
             */

            // overwrite node with everything that comes after it
            ULONG   ulOfsOfNextItem = (pbNextItem - G_pHandlesBuf->pbData);
            memmove(pbItem,
                    pbNextItem,
                    // byte count to move:
                    G_pHandlesBuf->cbData - ulOfsOfNextItem);

            // shrink handles buffer
            G_pHandlesBuf->cbData -= cbDelete;

            /*
             *   update NODERECORD pointers
             *
             */

            // go thru all records which come after this one and
            // update references
            PNODERECORD precAfterThis = precDelete->pNextRecord;
            while (precAfterThis)
            {
                // hack all record NODE offsets which come after the deletee
                precAfterThis->ulOfs -= cbDelete;

                // next record
                precAfterThis = precAfterThis->pNextRecord;
            }

            // go thru the entire records list and check which other
            // records reference this record to be deleted
            PNODERECORD prec2 = G_preccVeryFirst;
            while (prec2)
            {
                if (prec2->pNextDuplicate == precDelete)
                {
                    // record to be deleted is stored as a duplicate:
                    // replace with ptr to next duplicate (probably NULL)
                    prec2->pNextDuplicate = precDelete->pNextDuplicate;
                    prec2->cDuplicates--;
                }

                PNODERECORD pNextRec = prec2->pNextRecord;

                if (pNextRec == precDelete)
                    // record to be deleted is stored as "next record":
                    // replace with ptr to the one afterwards (can be NULL)
                    prec2->pNextRecord = precDelete->pNextRecord;

                prec2 = pNextRec;
            }

            if (precDelete->ulStatus & NODESTAT_ISDUPLICATE)
                // this was a duplicate:
                G_cDuplicatesFound--;

            // next record to be deleted
            pNode = pNode->pNext;
        } // end while (pNode)

        wphRebuildNodeHashTable((HHANDLES)G_pHandlesBuf,
                                FALSE);
        RebuildRecordsHashTable();

        // only now that we have rebuilt all record
        // pointers, we can safely mark orphans
        // V0.9.9 (2001-04-07) [umoeller]
        ULONG cNewRecords = 0;
        PNODERECORD prec2 = G_preccVeryFirst;
        while (prec2)
        {
            if (prec2->ulParentHandle)
            {
                PNODERECORD pParent = G_RecordHashTable[prec2->ulParentHandle];
                BOOL        fInvalid = FALSE;
                if (!pParent)
                    fInvalid = TRUE;
                else if (pParent->ulStatus & NODESTAT_INVALID_PARENT)
                    fInvalid = TRUE;

                if (fInvalid)
                    if (!(prec2->ulStatus & NODESTAT_INVALID_PARENT))
                    {
                        prec2->ulStatus |= NODESTAT_INVALID_PARENT;
                        UpdateStatusDescr(prec2);
                    }
            }

            prec2 = prec2->pNextRecord;
            cNewRecords++;
        }

        if (!arc)
        {
            // now update the container:
            // build an array of PRECORDCORE's to be removed
            // (damn, this SICK message...)
            ULONG cReccs = lstCountItems(pllRecords);
            PRECORDCORE *papReccs = (PRECORDCORE*)malloc(   cReccs
                                                          * sizeof(PRECORDCORE));
            if (papReccs)
            {
                PRECORDCORE *ppThis = papReccs;
                pNode = lstQueryFirstNode(pllRecords);
                while (pNode)
                {
                    PRECORDCORE precDelete = (PRECORDCORE)pNode->pItemData;
                    *ppThis = precDelete;
                    pNode = pNode->pNext;
                    ppThis++;
                }

                WinSendMsg(hwndCnr,
                           CM_REMOVERECORD,
                           (MPARAM)papReccs,
                           MPFROM2SHORT(cReccs,
                                        CMA_FREE));
                free(papReccs);
            }

            CalcHandleCounts();
            SetStatusBarText(G_hwndMain, NULL);

            // we must invalidate all because many record
            // offsets and emphasis will have changed
            cnrhInvalidateAll(hwndCnr);

            XSTRING str;
            CHAR    sz[500] = "";
            xstrInit(&str, 100);

            CNRINFO CnrInfo;
            WinSendMsg(hwndCnr,
                       CM_QUERYCNRINFO,
                       (MPARAM)&CnrInfo,
                       (MPARAM)sizeof(CnrInfo));

            if (cAbstractsNuked)
            {
                sprintf(sz, nlsGetString(ID_FXSI_NUKE_ABSTRACTS),
                        cAbstractsNuked);
                xstrcpy(&str, sz, 0);
            }
            if (cFolderPosesNuked)
            {
                if (sz[0])
                    xstrcat(&str, nlsGetString(ID_FXSI_NUKE_AND), 0);
                sprintf(sz, nlsGetString(ID_FXSI_NUKE_FOLDERPOS),
                        cFolderPosesNuked);
                xstrcat(&str, sz, 0);
            }

            if (cObjIDsNuked)
            {
                if (sz[0])
                    xstrcat(&str, nlsGetString(ID_FXSI_NUKE_AND), 0);
                sprintf(sz, nlsGetString(ID_FXSI_NUKE_OBJECTIDS),
                        cObjIDsNuked);
                xstrcat(&str, sz, 0);
            }

            if (str.ulLength)
                xstrcat(&str, nlsGetString(ID_FXSI_NUKE_SCHEDULED), 0);

            if (CnrInfo.cRecords != cNewRecords)
                sprintf(sz,
                        nlsGetString(ID_FXSI_NUKE_HANDLEPROBLEM),
                        CnrInfo.cRecords,
                        cNewRecords);
            else
                sprintf(sz, nlsGetString(ID_FXSI_NUKE_HANDLESLEFT), cNewRecords);
            xstrcat(&str, sz, 0);

            if (str.ulLength)
                winhDebugBox(G_hwndMain,
                             "xfix",
                             str.psz);

            xstrClear(&str);
        }

        free(pszFolderPoses);

    } // if (pszFolderPoses)

    return arc;
}

/*
 *@@ ConfirmHandleRemoval:
 *      asks the user to confirm handle removal then
 *      calls RemoveHandles() if OK;
 *      This function is called by the "Delete now"
 *      command handler and by WriteBack() if there
 *      are any deletions pending.
 *
 *@@added V1.0.9 (2011-11-05) [rwalsh]
 */

BOOL ConfirmHandleRemoval(BOOL fWriteback)
{
    BOOL      fRtn = TRUE;
    ULONG     ul;
    PLINKLIST pll;
    CHAR      szText[512];

    if (fWriteback)
    {
        ul = sprintf(szText, nlsGetString(ID_FXSI_REMOVE_BEFOREWRITEBACK), G_cHandlesDeleted);
        if (G_cHandlesOrphaned)
            sprintf(&szText[ul],
                    nlsGetString(ID_FXSI_REMOVE_WARNING),
                    G_cHandlesOrphaned);

        ul = MessageBox(G_hwndMain,
                        MB_YESNOCANCEL | MB_DEFBUTTON3 | MB_MOVEABLE,
                        szText);
    }
    else
    {
        ul = sprintf(szText, nlsGetString(ID_FXSI_REMOVE_HANDLES), G_cHandlesDeleted);
        if (G_cHandlesOrphaned)
            ul += sprintf(&szText[ul],
                          nlsGetString(ID_FXSI_REMOVE_WARNING),
                          G_cHandlesOrphaned);
        sprintf(&szText[ul], nlsGetString(ID_FXSI_REMOVE_AREYOUSURE));

        ul = MessageBox(G_hwndMain,
                        MB_YESNO | MB_DEFBUTTON2 | MB_MOVEABLE,
                        szText);
    }

    if (ul != MBID_YES)
    {
        if (ul == MBID_NO)
            return TRUE;

        return FALSE;
    }

    if (!(pll = lstCreate(FALSE)))
        fRtn = FALSE;
    else
    {
        // for every record
        for (PNODERECORD prec = G_preccVeryFirst;
             prec;
             prec = prec->pNextRecord)
        {
            if (prec->ulAction == NODEACTION_DELETE)
                lstAppendItem(pll, prec);
        }

        if (lstCountItems(pll))
            if (RemoveHandles(WinWindowFromID(G_hwndMain, FID_CLIENT), pll))
                fRtn = FALSE;

        // free the list
        lstFree(&pll);
    }

    if (!fRtn)
    {
        MessageBox(G_hwndMain,
                   MB_OK | MB_MOVEABLE,
                   nlsGetString(ID_FXSI_ERROR_REMOVEHANDLES));

        WinEnableMenuItem(WinWindowFromID(G_hwndMain, FID_MENU),
                          ID_FXMI_WRITETOINI,
                          FALSE);
    }

    return fRtn;
}

/*
 *@@ WriteAllBlocks:
 *
 *@@added V0.9.7 (2001-01-21) [umoeller]
 */

BOOL WriteAllBlocks(PSZ pszHandles,
                    PULONG pulLastBlockWritten)
{
    BYTE    szBlockName[10];

    PBYTE   pStart = G_pHandlesBuf->pbData;
    ULONG   ulCurSize = 4;

    PBYTE   p = G_pHandlesBuf->pbData + 4,
            pEnd = G_pHandlesBuf->pbData + G_pHandlesBuf->cbData;

    ULONG   ulCurrentBlock = 1;

    while (p < pEnd)
    {
        while (p < pEnd)
        {
            ULONG ulPartSize;
            if (!memicmp(p, "DRIV", 4))
            {
                PDRIVE pDriv = (PDRIVE)p;
                ulPartSize = sizeof(DRIVE) + strlen(pDriv->szName);
            }
            else if (!memicmp(p, "NODE", 4))
            {
                PNODE pNode = (PNODE)p;
                ulPartSize = sizeof (NODE) + pNode->usNameSize;
            }

            if (ulCurSize + ulPartSize > 0x0000FFFF)
                break;

            ulCurSize += ulPartSize;
            p         += ulPartSize;
        }

        *pulLastBlockWritten = ulCurrentBlock;

        sprintf(szBlockName, "BLOCK%d", ulCurrentBlock++);
        PrfWriteProfileData(G_hiniSystem,
                            pszHandles,         // PM_Workplace:Handles0 or 1
                            szBlockName,
                            pStart,
                            ulCurSize);
        pStart    = p;
        ulCurSize = 0;
    }

    // delete remaining buffers
    while (ulCurrentBlock < 20)
    {
        ULONG ulBlockSize = 0;
        sprintf(szBlockName, "BLOCK%d", ulCurrentBlock++);

        if (   (PrfQueryProfileSize(G_hiniSystem,
                                    pszHandles,
                                    szBlockName,
                                    &ulBlockSize))
             && (ulBlockSize != 0)
           )
        {
            PrfWriteProfileData(G_hiniSystem,
                                pszHandles,
                                szBlockName,
                                NULL,
                                0);
        }
    }

    return TRUE;
}

/*
 *@@ WriteBack:
 *
 *@@added V0.9.7 (2001-01-21) [umoeller]
 *@@changed V1.0.4 (2005-02-24) [chennecke]: replaced hard-coded strings with nlsGetString() calls
 *@@changed V1.0.9 (2011-11-05) [rwalsh]: added handle removal before writeback
 */

VOID WriteBack(VOID)
{
    char msg[2000];  // made constant a variable V1.0.4 (2005-02-25) [chennecke]

    // added V1.0.9 (2011-11-05) [rwalsh]
    if (G_cHandlesDeleted)
    {
        if (!ConfirmHandleRemoval(TRUE))
            return;
    }

    // rewritten V0.9.20 (2002-07-16) [umoeller]
    // split this up because string length exceeded limit for resource files V1.0.4 (2005-02-25) [chennecke]
    strcpy(msg, nlsGetString(ID_FXSI_WRITEBACK_INTRO));
    strcat(msg, nlsGetString(ID_FXSI_WRITEBACK_FIRSTFIRST));
    strcat(msg, nlsGetString(ID_FXSI_WRITEBACK_FIRSTSECOND));
    strcat(msg, nlsGetString(ID_FXSI_WRITEBACK_SECOND));
    strcat(msg, nlsGetString(ID_FXSI_WRITEBACK_THIRD));
    strcat(msg, nlsGetString(ID_FXSI_WRITEBACK_CONFIRM));

    if (MessageBox(G_hwndMain,
                   MB_YESNO | MB_DEFBUTTON2 | MB_MOVEABLE,
                   msg)
            != MBID_YES)
    {
        // "no":
        MessageBox(G_hwndMain,
                   MB_OK | MB_MOVEABLE,
                   nlsGetString(ID_FXSI_WRITEBACKABORTED)
                   );
    }
    else
    {
        // "yes":
        CHAR szText[300];
        APIRET arc;

        PSZ pszActiveHandles;
        if (arc = wphQueryActiveHandles(G_hiniSystem,
                                        &pszActiveHandles))
            strcpy(szText, nlsGetString(ID_FXSI_ERROR_CANTGETACTIVEHANDLES));
        else
        {
            ULONG   ulLastChar = strlen(pszActiveHandles) - 1;
            ULONG   ulLastBlock = 0;

            pszActiveHandles[ulLastChar] = '0';
            WriteAllBlocks(pszActiveHandles, &ulLastBlock);
            pszActiveHandles[ulLastChar] = '1';
            WriteAllBlocks(pszActiveHandles, &ulLastBlock);

            // from main(), return 1 now... this tells XWorkplace
            // that the handles have changed
            G_ulrc = 1;

            sprintf(szText,
                    nlsGetString(ID_FXSI_WRITEBACKSUCCESS_BLOCKS),
                    ulLastBlock);

            // update desktop
            if (G_paulAbstractsTarget)
            {
                // deferred changes:
                PrfWriteProfileData(G_hiniUser,
                                    WPINIAPP_FDRCONTENT, // "PM_Abstract:FldrContent",
                                    G_szHandleDesktop,
                                    G_paulAbstractsTarget,
                                    G_cAbstractsDesktop * sizeof(ULONG));

                sprintf(szText + strlen(szText),
                        nlsGetString(ID_FXSI_WRITEBACKSUCCESS_OBJMOVED),
                        G_cTotalAbstractsMoved);
            }

            // now nuke the entries in OS2.INI as well
            ULONG cNukes = 0;
            PLISTNODE pNode = lstQueryFirstNode(&G_llDeferredNukes);
            while (pNode)
            {
                PDEFERREDINI pNuke = (PDEFERREDINI)pNode->pItemData;
                PrfWriteProfileData(pNuke->hini,
                                    pNuke->pcszApp,
                                    pNuke->pszKey,
                                    NULL,
                                    0);
                cNukes++;

                pNode = pNode->pNext;
            }

            if (cNukes)
                sprintf(szText + strlen(szText),
                        nlsGetString(ID_FXSI_WRITEBACKSUCCESS_KEYSDELETED), cNukes);

            strcat(szText, nlsGetString(ID_FXSI_WRITEBACKSUCCESS_EXIT));
        }

        MessageBox(G_hwndMain,
                   MB_OK | MB_MOVEABLE,
                   szText);
    }
}

/*
 *@@ GetSelectedRecords:
 *      creates a linked list with all records that are
 *      currently selected.
 *
 *@@added V0.9.7 (2001-01-25) [umoeller]
 */

PLINKLIST GetSelectedRecords(HWND hwndCnr,
                             PRECORDCORE precSource,
                             PULONG pcRecs)         // out: records count
{
    PLINKLIST pll = lstCreate(FALSE);
    if (pll)
    {
        ULONG ulSel = 0;
        PNODERECORD prec = (PNODERECORD)cnrhQuerySourceRecord(hwndCnr,
                                                              precSource,
                                                              &ulSel);
        if (prec)
        {
            lstAppendItem(pll, prec);
            if (ulSel == SEL_MULTISEL)
            {
                PRECORDCORE prec2 = (PRECORDCORE)prec;
                while (prec2 = cnrhQueryNextSelectedRecord(hwndCnr, prec2))
                    lstAppendItem(pll, prec2);
            }
        }

        *pcRecs = lstCountItems(pll);
    }

    return (pll);
}

#define IDDI_FILEMASK       500
#define IDDI_SELECT         501
#define IDDI_DESELECT       502
#define IDDI_SELECTALL      503
#define IDDI_DESELECTALL    504

/*
 *@@ RefreshAfterSelect:
 *
 *@@added V0.9.15 (2001-09-14) [umoeller]
 */

VOID RefreshAfterSelect(HWND hwndDlg)
{
    // cnr is in QWL_USER
    HWND hwndCnr = WinQueryWindowULong(hwndDlg, QWL_USER);

    cnrhInvalidateAll(hwndCnr);

    // give entry field the focus again
    HWND hwndEF = WinWindowFromID(hwndDlg, IDDI_FILEMASK);
    winhEntryFieldSelectAll(hwndEF);
    WinSetFocus(HWND_DESKTOP, hwndEF);
    // update the status bar
    CalcHandleCounts();
    SetStatusBarText(G_hwndMain, NULL);
}

/*
 *@@ fnwpSelectByName:
 *
 *@@added V0.9.15 (2001-09-14) [umoeller]
 */

MRESULT EXPENTRY fnwpSelectByName(HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    switch (msg)
    {
        case WM_COMMAND:
        {
            USHORT usCommand = SHORT1FROMMP(mp1);
            switch (usCommand)
            {
                case IDDI_SELECT:
                case IDDI_DESELECT:
                {
                    PSZ pszMask;
                    if (pszMask = winhQueryDlgItemText(hwndDlg, IDDI_FILEMASK))
                    {
                        // cnr is in QWL_USER
                        HWND hwndCnr = WinQueryWindowULong(hwndDlg, QWL_USER);

                        // run thru all records and select/deselect them if
                        // they match
                        HPOINTER hptrOld = winhSetWaitPointer();

                        PNODERECORD prec = G_preccVeryFirst;
                        while (prec)
                        {
                            // does this match the file mask?
                            if (doshMatch(pszMask,
                                          prec->szShortNameCopy))
                            {
                                // yes:
                                if (usCommand == IDDI_SELECT)
                                {
                                    if (!(prec->recc.flRecordAttr & CRA_SELECTED))
                                        prec->recc.flRecordAttr |= CRA_SELECTED;
                                }
                                else
                                    if (prec->recc.flRecordAttr & CRA_SELECTED)
                                        prec->recc.flRecordAttr &= ~CRA_SELECTED;
                            }

                            prec = prec->pNextRecord;
                        }

                        RefreshAfterSelect(hwndDlg);

                        WinSetPointer(HWND_DESKTOP, hptrOld);

                        free(pszMask);
                    }
                }
                break;

                case IDDI_SELECTALL:
                case IDDI_DESELECTALL:
                {
                    HPOINTER hptrOld = winhSetWaitPointer();
                    // cnr is in QWL_USER
                    HWND hwndCnr = WinQueryWindowULong(hwndDlg, QWL_USER);

                    PNODERECORD prec = G_preccVeryFirst;
                    while (prec)
                    {
                        if (usCommand == IDDI_SELECTALL)
                        {
                            if (0 == (prec->recc.flRecordAttr & CRA_SELECTED))
                                prec->recc.flRecordAttr |= CRA_SELECTED;
                        }
                        else
                            if (prec->recc.flRecordAttr & CRA_SELECTED)
                                prec->recc.flRecordAttr &= ~CRA_SELECTED;

                        prec = prec->pNextRecord;
                    }

                    RefreshAfterSelect(hwndDlg);

                    WinSetPointer(HWND_DESKTOP, hptrOld);
                }
                break;

                default:
                    mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
            }
        }
        break;

        default:
            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
    }

    return mrc;
}

/*
 *@@ SelectByName:
 *
 *@@added V0.9.15 (2001-09-14) [umoeller]
 *@@changed V1.0.4 (2005-02-24) [chennecke]: replaced hard-coded strings with nlsGetString() calls
 */

VOID SelectByName(HWND hwndCnr)
{
    static const CONTROLDEF
                Static = {
                            WC_STATIC,
                            nlsGetString(ID_FXDI_FILEMASKSELECT_MASK),
                            WS_VISIBLE | SS_TEXT | DT_LEFT | DT_WORDBREAK,
                            -1,
                            CTL_COMMON_FONT,
                            { 150, SZL_AUTOSIZE },     // size
                            COMMON_SPACING,
                         },
                Entry = {
                            WC_ENTRYFIELD,
                            "*",
                            WS_VISIBLE | WS_TABSTOP | ES_LEFT | ES_MARGIN | ES_AUTOSCROLL,
                            IDDI_FILEMASK,
                            CTL_COMMON_FONT,
                            { 150, -1 },     // size
                            COMMON_SPACING,
                         },
                SelectButton = {
                            WC_BUTTON,
                            nlsGetString(ID_FXDI_FILEMASKSELECT_SELECT),
                            WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON | BS_DEFAULT,
                            IDDI_SELECT,
                            CTL_COMMON_FONT,
                            { STD_BUTTON_WIDTH, STD_BUTTON_HEIGHT },    // size
                            COMMON_SPACING,
                         },
                DeselectButton = {
                            WC_BUTTON,
                            nlsGetString(ID_FXDI_FILEMASKSELECT_DESELECT),
                            WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                            IDDI_DESELECT,
                            CTL_COMMON_FONT,
                            { STD_BUTTON_WIDTH, STD_BUTTON_HEIGHT },    // size
                            COMMON_SPACING,
                         },
                SelectAllButton = {
                            WC_BUTTON,
                            nlsGetString(ID_FXDI_FILEMASKSELECT_SELECTALL),
                            WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                            IDDI_SELECTALL,
                            CTL_COMMON_FONT,
                            { STD_BUTTON_WIDTH, STD_BUTTON_HEIGHT },    // size
                            COMMON_SPACING,
                         },
                DeselectAllButton = {
                            WC_BUTTON,
                            nlsGetString(ID_FXDI_FILEMASKSELECT_DESELECTALL),
                            WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                            IDDI_DESELECTALL,
                            CTL_COMMON_FONT,
                            { STD_BUTTON_WIDTH, STD_BUTTON_HEIGHT },    // size
                            COMMON_SPACING,
                         },
                CloseButton = {
                            WC_BUTTON,
                            nlsGetString(DID_CLOSE),
                            WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                            DID_CANCEL,
                            CTL_COMMON_FONT,
                            { STD_BUTTON_WIDTH, STD_BUTTON_HEIGHT },    // size
                            COMMON_SPACING,
                         };

    static const DLGHITEM dlgSelect[] =
        {
            START_TABLE,
                START_ROW(0),
                    CONTROL_DEF(&Static),
                START_ROW(0),
                    CONTROL_DEF(&Entry),
                START_ROW(0),
                    CONTROL_DEF(&SelectButton),
                    CONTROL_DEF(&DeselectButton),
                START_ROW(0),
                    CONTROL_DEF(&SelectAllButton),
                    CONTROL_DEF(&DeselectAllButton),
                START_ROW(0),
                    CONTROL_DEF(&CloseButton),
            END_TABLE
        };

    HWND hwndDlg = NULLHANDLE;
    PSZ  pszReturn = NULL;

    if (!dlghCreateDlg(&hwndDlg,
                       G_hwndMain,
                       FCF_FIXED_DLG,
                       fnwpSelectByName,
                       nlsGetString(ID_FXDI_FILEMASKSELECT),
                       dlgSelect,      // DLGHITEM array
                       ARRAYITEMCOUNT(dlgSelect),
                       NULL,
                       "9.WarpSans"))
    {
        HWND hwndEF = WinWindowFromID(hwndDlg, IDDI_FILEMASK);
        winhCenterWindow(hwndDlg);
        winhSetEntryFieldLimit(hwndEF, CCHMAXPATH);
        winhEntryFieldSelectAll(hwndEF);
        WinSetFocus(HWND_DESKTOP, hwndEF);

        // store cnr in QWL_USER
        WinSetWindowULong(hwndDlg, QWL_USER, hwndCnr);

        // go!
        WinProcessDlg(hwndDlg);             // fnwpSelectByName handles the buttons

        WinDestroyWindow(hwndDlg);
    }
}

/*
 *@@ Protect:
 *      mark the record as protected against deletion
 *      then increase protection count for all ancestors
 *      to protect them also
 *
 *@@added V1.0.9 (2011-10-30) [rwalsh]:
 */

void  Protect(PNODERECORD precIn, RCArray* prca)
{
    PNODERECORD prec;

    // don't protect records with errors or those already protected
    if (   precIn->ulAction == NODEACTION_PROTECT
        || precIn->ulStatus)
        return;

    // for the input record and all its ancestors...
    for (prec = precIn;
         prec;
         prec =   prec->ulParentHandle
                ? G_RecordHashTable[prec->ulParentHandle]
                : G_DriveHashTable[prec->ulDriveNdx]
        )
    {
        prec->cProtected++;

        if (prec == precIn || prec->cProtected == 1)
        {
            if (prec->ulAction == NODEACTION_DELETE)
            {
                prec->ulAction = NODEACTION_NONE;
                G_cHandlesDeleted--;
                if (prec->cOrphaned)
                    G_cHandlesOrphaned++;
                if (prec->ulType == NODETYPE_FOLDER)
                    UnmarkPendingOrphans(prec,
                                         prec->ulHandle,
                                         prca);
            }
            if (prec == precIn)
                prec->ulAction = NODEACTION_PROTECT;

            SetIconAndStatus(prec);
            if (prca)
                prca->Append(&prec->recc);
        }
    }

    return;
}

/*
 *@@ Unprotect:
 *      remove protection from the input record, then
 *      decrease the protection count for all ancestors
 *
 *@@added V1.0.9 (2011-10-30) [rwalsh]:
 */

void  Unprotect(PNODERECORD precIn, RCArray* prca)
{
    PNODERECORD prec;

    // don't unprotect records that aren't protected
    if (precIn->ulAction != NODEACTION_PROTECT)
        return;

    precIn->ulAction = NODEACTION_NONE;

    // for the input record and all its ancestors...
    for (prec = precIn;
         prec;
         prec =   prec->ulParentHandle
                ? G_RecordHashTable[prec->ulParentHandle]
                : G_DriveHashTable[prec->ulDriveNdx]
        )
    {
        // this should always be > 0, but test just in case...
        if (prec->cProtected)
            prec->cProtected--;

        if (prec == precIn || !prec->cProtected)
        {
            SetIconAndStatus(prec);
            if (prca)
                prca->Append(&prec->recc);
        }
    }

    return;
}

/*
 *@@ FrameCommand:
 *
 *@@added V0.9.7 (2001-01-21) [umoeller]
 *@@changed V1.0.4 (2005-02-24) [chennecke]: replaced hard-coded strings with nlsGetString() calls
 *                                           renamed IDM... identifiers to ID_FXM...
 *@@changed V1.0.9 (2011-10-30) [rwalsh]: added record protection routines
 *@@changed V1.0.9 (2011-11-06) [rwalsh]: rewrote ID_FXMI_SELECT_INVALID handler, removed callback
 */

VOID FrameCommand(HWND hwndFrame,
                  USHORT usCmd,
                  PRECORDCORE precSource)
{
    HWND hwndCnr = WinWindowFromID(G_hwndMain, FID_CLIENT);

    TRY_LOUD(excpt1)
    {
        switch (usCmd)
        {
            case ID_FXMI_RESCAN:
                StartInsertHandles(hwndCnr);
            break;

            case ID_FXMI_WRITETOINI:
                WriteBack();
            break;

            case ID_FXMI_VIEW_OBJIDS:
                ViewObjectIDs();
            break;

            case ID_FXMI_SORT_INDEX:
            case ID_FXMI_SORT_ICON:
            case ID_FXMI_SORT_STATUS:
            case ID_FXMI_SORT_TYPE:
            case ID_FXMI_SORT_HANDLE:
            case ID_FXMI_SORT_PARENT:
            case ID_FXMI_SORT_SHORTNAME:
            case ID_FXMI_SORT_EXTENSION:
            case ID_FXMI_SORT_CHILDREN:
            case ID_FXMI_SORT_DUPS:
            case ID_FXMI_SORT_REFCS:
            case ID_FXMI_SORT_LONGNAME:
                SetMainSort(usCmd);
            break;

            case ID_FXMI_ACTIONS_FILES:
                thrCreate(&G_tiCheckFiles,
                          fntCheckFiles,
                          &G_tidCheckFilesRunning,
                          "CheckFiles",
                          THRF_WAIT | THRF_PMMSGQUEUE,
                          0);     // thread param
                WinStartTimer(WinQueryAnchorBlock(hwndFrame),
                              G_hwndMain,
                              TIMERID_THREADRUNNING,
                              200);
                UpdateMenuItems(0);
            break;

            // changed V1.0.9 (2011-11-06) [rwalsh]: removed callback, added counts
            case ID_FXMI_SELECT_INVALID:
            {
                PNODERECORD prec;

                for (prec = G_preccVeryFirst;
                     prec;
                     prec = prec->pNextRecord)
                {
                    if (prec->recc.flRecordAttr & CRA_SELECTED)
                    {
                        prec->recc.flRecordAttr &= ~CRA_SELECTED;
                        G_cHandlesSelected--;
                    }
                    if (prec->ulStatus)
                    {
                        prec->recc.flRecordAttr |= CRA_SELECTED;
                        G_cHandlesSelected++;
                    }
                }

                // invalidate the entire container
                SetStatusBarText(hwndFrame, NULL);
                WinSendMsg(hwndCnr, CM_INVALIDATERECORD, 0, 0);
            }
            break;

            case ID_FXMI_SELECT_BYNAME:
                SelectByName(hwndCnr);
            break;

            /*
             * ID_FXMI_DELETE:
             * ID_FXMI_UNDELETE:
             * ID_FXMI_TOGGLEDELETE:
             *      mark/unmark/toggle delete flag
             *      for unprotected records
             *
             *@@changed V1.0.9 (2011-10-30) [rwalsh]: added record protection
             *@@changed V1.0.9 (2011-10-31) [rwalsh]: added deferred deletion
             */

            case ID_FXMI_DELETE:
            case ID_FXMI_UNDELETE:
            case ID_FXMI_TOGGLEDELETE:
            {
                ULONG       cRecs = 0;
                PLINKLIST   pll;

                if (!(pll = GetSelectedRecords(hwndCnr,
                                               precSource,
                                               &cRecs)))
                    break;

                RCArray     rca(cRecs * 2);
                PLISTNODE   pNode;

                for (pNode = lstQueryFirstNode(pll);
                     pNode;
                     pNode = pNode->pNext)
                {
                    PNODERECORD prec = (PNODERECORD)pNode->pItemData;

                    // skip records that are protected
                    // or are already in the desired state
                    if (   (prec->cProtected)
                        || (usCmd == ID_FXMI_UNDELETE && prec->ulAction == NODEACTION_NONE)
                        || (usCmd == ID_FXMI_DELETE   && prec->ulAction == NODEACTION_DELETE)
                       )
                        continue;

                    if (prec->ulAction == NODEACTION_DELETE)
                    {
                        prec->ulAction = NODEACTION_NONE;
                        G_cHandlesDeleted--;
                        if (prec->ulStatus)
                            G_cHandlesInvalid++;
                        if (prec->cOrphaned)
                            G_cHandlesOrphaned++;
                    }
                    else
                    {
                        prec->ulAction = NODEACTION_DELETE;
                        G_cHandlesDeleted++;
                        if (prec->ulStatus && G_cHandlesInvalid)
                            G_cHandlesInvalid--;
                        if (prec->cOrphaned)
                            G_cHandlesOrphaned--;
                    }

                    SetIconAndStatus(prec);
                    rca.Append(&prec->recc);

                    if (   prec->ulType == NODETYPE_FOLDER
                        || prec->ulType == NODETYPE_DRIVE)
                    {
                        if (prec->ulAction == NODEACTION_DELETE)
                            MarkPendingOrphans(prec,
                                               prec->ulHandle,
                                               &rca);
                        else
                            UnmarkPendingOrphans(prec,
                                                 prec->ulHandle,
                                                 &rca);
                    }
                }

                // invalidate any record whose icon changed
                if (rca.Count())
                {
                    SetStatusBarText(hwndFrame, NULL);
                    WinSendMsg(hwndCnr,
                               CM_INVALIDATERECORD,
                               (MPARAM)rca.Data(),
                               MPFROM2SHORT(rca.Count(), 0));
                }

                // free the list
                lstFree(&pll);
            }
            break;

            /*
             * ID_FXMI_ACTIONS_DELETENOW:
             *      remove records marked for deletion
             *
             *@@added V1.0.9 (2011-10-31) [rwalsh]: added deferred deletion
             *@@changed V1.0.9 (2011-11-05) [rwalsh]: moved delete code to ConfirmHandleRemoval()
             */
            case ID_FXMI_ACTIONS_DELETENOW:
                ConfirmHandleRemoval(FALSE);
            break;

            case ID_FXMI_NUKEFOLDERPOS:
            {
                ULONG       cRecs = 0;
                PLINKLIST   pll = GetSelectedRecords(hwndCnr,
                                                     precSource,
                                                     &cRecs);
                if (pll && cRecs)
                {
                    APIRET arc;
                    PSZ pszFolderPoses = NULL;

                    if (!(arc = prfhQueryKeysForApp(G_hiniUser,
                                                    WPINIAPP_FOLDERPOS, // "PM_Workplace:FolderPos",
                                                    &pszFolderPoses)))
                    {
                        PLISTNODE pNode = lstQueryFirstNode(pll);
                        ULONG cTotalNuked = 0;
                        while (pNode)
                        {
                            PNODERECORD prec = (PNODERECORD)pNode->pItemData;

                            if (prec->fFolderPos)
                            {
                                ULONG cNukedThis = 0;
                                NukeFolderPoses(prec,
                                                pszFolderPoses,
                                                &cNukedThis);

                                if (cNukedThis)
                                {
                                    cTotalNuked += cNukedThis;
                                    prec->fFolderPos = FALSE;
                                    UpdateStatusDescr(prec);
                                }
                            }

                            pNode = pNode->pNext;
                        }

                        if (cTotalNuked)
                            cnrhInvalidateAll(hwndCnr);

                        CHAR sz[200];
                        sprintf(sz, nlsGetString(ID_FXSI_NUKEFOLDERPOS),
                                cTotalNuked);
                        winhDebugBox(G_hwndMain,
                                     "xfix",
                                     sz);

                        free(pszFolderPoses);
                    }

                    lstFree(&pll);
                }
            }
            break;

            case ID_FXMI_NUKEOBJID:
            {
                ULONG       cRecs = 0;
                PLINKLIST   pll = GetSelectedRecords(hwndCnr,
                                                     precSource,
                                                     &cRecs);
                if (pll && cRecs)
                {
                    PLISTNODE pNode = lstQueryFirstNode(pll);
                    ULONG cTotalNuked = 0;
                    while (pNode)
                    {
                        PNODERECORD prec = (PNODERECORD)pNode->pItemData;

                        if (prec->pObjID)
                        {
                            ULONG cNukedThis = 0;
                            NukeObjID(prec,
                                      &cNukedThis);

                            if (cNukedThis)
                                cTotalNuked += cNukedThis;
                        }

                        pNode = pNode->pNext;
                    }

                    if (cTotalNuked)
                        cnrhInvalidateAll(hwndCnr);

                    CHAR sz[200];
                    sprintf(sz, nlsGetString(ID_FXSI_DELETEHANDLES),
                            cTotalNuked);
                    winhDebugBox(G_hwndMain,
                                 "xfix",
                                 sz);

                    lstFree(&pll);
                }
            }
            break;

            /*
             * ID_FXMI_MOVEABSTRACTS:
             *      move abstracts to desktop
             */

            case ID_FXMI_MOVEABSTRACTS:
            {
                ULONG       cAbstractsMoved = 0;

                // get handle of desktop
                if (G_precDesktop == NULL)
                {
                    // first call:
                    ULONG       hobjDesktop = 0;
                    ULONG       cb = sizeof(hobjDesktop);
                    if (    (PrfQueryProfileData(G_hiniUser,
                                                 WPINIAPP_LOCATION, // "PM_Workplace:Location",
                                                 WPOBJID_DESKTOP, // "<WP_DESKTOP>",
                                                 &hobjDesktop,
                                                 &cb))
                         && (hobjDesktop)
                       )
                    {
                        // OK, found desktop:
                        // get its NODERECORD
                        G_precDesktop = G_RecordHashTable[hobjDesktop & 0xFFFF];

                        CHAR sz2[100];
                        sprintf(sz2, nlsGetString(ID_FXSI_MOVEABSTRACTS_DESKTOPID), hobjDesktop & 0xFFFF);
                        winhDebugBox(NULLHANDLE,
                                     "xfix",
                                     sz2);
                    }
                }

                if (G_precDesktop)
                {
                    ULONG       cRecs = 0;
                    PLINKLIST   pll = GetSelectedRecords(hwndCnr,
                                                         precSource,
                                                         &cRecs);
                    if (pll && cRecs)
                    {
                        CHAR szText[500];
                        sprintf(szText, nlsGetString(ID_FXSI_MOVEABSTRACTS_CONFIRM), cRecs);
                        if (MessageBox(hwndFrame,
                                       MB_YESNO | MB_DEFBUTTON2 | MB_MOVEABLE,
                                       szText)
                                == MBID_YES)
                        {
                            // user really wants this:
                            // load existing abstracts for desktop, if any
                            MoveAbstracts(pll,
                                          &cAbstractsMoved);
                        }

                        lstFree(&pll);
                    }
                }

                if (cAbstractsMoved)
                {
                    CHAR sz[200];
                    sprintf(sz, nlsGetString(ID_FXSI_MOVEABSTRACTSSUCCESS), cAbstractsMoved);
                    winhDebugBox(G_hwndMain, "xfix", sz);
                }
                else
                {
                    // added work-around so compiler doesn't compain about const = variable V1.0.4 (2005-02-24) [chennecke]
                    CHAR sz[100];
                    sprintf(sz, nlsGetString(ID_FXSI_ERROR_MOVEABSTRACTS));
                    winhDebugBox(G_hwndMain, "xfix", sz);
                }
            }
            break;

            /*
             * ID_FXMI_PROTECT:
             *      mark selected items as protected against deletion
             *      then increase protection count for all ancestors
             *      to protect them also
             *
             * ID_FXMI_UNPROTECT:
             *      remove protection from selected items that have
             *      been marked as protected, then decrease protection
             *      count for all ancestors
             *
             * ID_FXMI_TOGGLEPROTECT:
             *      toggle protection for selected items
             *
             *@@added V1.0.9 (2011-10-30) [rwalsh]:
             */

            case ID_FXMI_PROTECT:
            case ID_FXMI_UNPROTECT:
            case ID_FXMI_TOGGLEPROTECT:
            {
                ULONG       cNodes = 0;
                PLINKLIST   pll;

                if (!(pll = GetSelectedRecords(hwndCnr, precSource, &cNodes)))
                    break;

                RCArray     rca(cNodes * 4);
                PLISTNODE   pNode = lstQueryFirstNode(pll);

                while (pNode)
                {
                    PNODERECORD prec = (PNODERECORD)pNode->pItemData;

                    if (   (usCmd == ID_FXMI_UNPROTECT)
                        || (   usCmd == ID_FXMI_TOGGLEPROTECT
                            && prec->ulAction == NODEACTION_PROTECT)
                       )
                        Unprotect(prec, &rca);
                    else
                        Protect(prec, &rca);

                    pNode = pNode->pNext;
                }

                // invalidate any record whose icon or emphasis changed
                if (rca.Count())
                {
                    SetStatusBarText(hwndFrame, NULL);
                    WinSendMsg(hwndCnr,
                               CM_INVALIDATERECORD,
                               (MPARAM)rca.Data(),
                               MPFROM2SHORT(rca.Count(), 0));
                }

                // cleanup
                lstFree(&pll);
            }
            break;

            /*
             * ID_FXMI_ACTIONS_AUTOPROTECT:
             *      mark items that fit criteria to protect them from
             *      deletion, then increase protection count for all
             *      ancestors to protect them also
             *
             *@@added V1.0.9 (2011-10-30) [rwalsh]:
             */

            case ID_FXMI_ACTIONS_AUTOPROTECT:
            {
                PNODERECORD prec;

                // for every record in hwndCnr...
                for (prec = G_preccVeryFirst;
                     prec;
                     prec = prec->pNextRecord)
                {
                    // ignore records with errors or those already marked
                    if (   (prec->ulStatus)
                        || (prec->ulAction == NODEACTION_PROTECT)
                       )
                        continue;

                    // protect records that contain abstracts or have
                    // object IDs, plus drives and their root folders
                    if (   prec->cAbstracts
                        || prec->pObjID
                        || prec->ulDriveNdx
                        || prec->ulType == NODETYPE_DRIVE
                       )
                    {
                        Protect(prec, NULL);
                        continue;
                    }

                    // for files, protect executables
                    if (prec->ulType == NODETYPE_FILE)
                    {
                        char *ptr = strrchr(prec->pszShortNameCopy, '.');

                        // ignore files named, e.g., ".exe"
                        if (ptr && ptr != prec->pszShortNameCopy)
                        {
                            ptr++;
                            if (   !memcmp(ptr, "EXE", 4)
                                || !memcmp(ptr, "CMD", 4)
                                || !memcmp(ptr, "COM", 4)
                                || !memcmp(ptr, "BAT", 4)
                               )
                                Protect(prec, NULL);
                        }
                    }
                }

                // invalidate the entire container
                SetStatusBarText(hwndFrame, NULL);
                WinSendMsg(hwndCnr, CM_INVALIDATERECORD, 0, 0);
            }
            break;

            /*
             * ID_FXMI_ACTIONS_UNPROTECTALL:
             *      remove protection from all items in hwndCnr
             *      and remove all protection indicators
             *
             *@@added V1.0.9 (2011-10-30) [rwalsh]:
             */

            case ID_FXMI_ACTIONS_UNPROTECTALL:
            {
                PNODERECORD prec = G_preccVeryFirst;

                // for every record in hwndCnr...
                while (prec)
                {
                    // if it has any protection at all, remove it
                    if (prec->cProtected)
                    {
                        prec->cProtected = 0;
                        SetIconAndStatus(prec);
                    }

                    prec = prec->pNextRecord;
                }

                // invalidate the entire container
                SetStatusBarText(hwndFrame, NULL);
                WinSendMsg(hwndCnr, CM_INVALIDATERECORD, 0, 0);
            }
            break;

            default:
                StandardCommands(G_hwndMain, usCmd);
        }
    }
    CATCH(excpt1) {} END_CATCH();
}

/*
 *@@ FrameWMControl:
 *      handler for WM_CONTROL for the client in
 *      winh_fnwpFrameWithStatusBar.
 *
 *@@added V0.9.15 (2001-09-14) [umoeller]
 *@@changed V1.0.4 (2005-02-24) [chennecke]: renamed IDM... identifiers to ID_FXM...
 *@@changed V1.0.9 (2011-10-30) [rwalsh]: added enable/disable for 'Protect' menuitems
 *@@changed V1.0.9 (2011-11-06) [rwalsh]: rewrote CN_EMPHASIS handling
 */

MRESULT FrameWMControl(HWND hwndFrame,
                       USHORT usNotifyCode,
                       MPARAM mp2)
{
    MRESULT mrc = 0;

    switch (usNotifyCode)
    {
        /*
         * CN_CONTEXTMENU:
         *
         */

        case CN_CONTEXTMENU:
        {
            HWND hwndCnr = WinWindowFromID(G_hwndMain, FID_CLIENT);
            if (G_preccSource = (PRECORDCORE)mp2)
            {
                ULONG       cRecs = 0;
                PLINKLIST   pll;
                if (pll = GetSelectedRecords(hwndCnr,
                                             G_preccSource,
                                             &cRecs))
                {
                    HWND hwndMenu = G_hwndContextMenuSingle;
                    if (cRecs > 1)
                    {
                        // more than one record selected:
                        hwndMenu = G_hwndContextMenuMulti;
                    }

                    // go thru selected records and disable menu
                    // items accordingly
                    BOOL    fGotAbstracts = FALSE,
                            fGotFolderPoses = FALSE,
                            fGotIDs = FALSE,
                            fDeleteOK = FALSE,
                            fUndeleteOK = FALSE,
                            fProtectOK = FALSE,
                            fUnprotectOK = FALSE;

                    PLISTNODE pNode = lstQueryFirstNode(pll);
                    while (pNode)
                    {
                        PNODERECORD prec = (PNODERECORD)pNode->pItemData;

                        if (prec->cAbstracts)
                            fGotAbstracts = TRUE;
                        if (prec->fFolderPos)
                            fGotFolderPoses = TRUE;
                        if (prec->pObjID)
                            fGotIDs = TRUE;
                        if (prec->ulAction == NODEACTION_PROTECT)
                            fUnprotectOK = TRUE;
                        else
                            if (!prec->ulStatus)
                                fProtectOK = TRUE;
                        if (prec->ulAction == NODEACTION_DELETE)
                            fUndeleteOK = TRUE;
                        else
                            if (!prec->cProtected)
                                fDeleteOK = TRUE;

                        pNode = pNode->pNext;
                    }

                    WinEnableMenuItem(hwndMenu,
                                      ID_FXMI_NUKEFOLDERPOS,
                                      fGotFolderPoses);
                    WinEnableMenuItem(hwndMenu,
                                      ID_FXMI_MOVEABSTRACTS,
                                      fGotAbstracts);
                    WinEnableMenuItem(hwndMenu,
                                      ID_FXMI_NUKEOBJID,
                                      fGotIDs);
                    WinEnableMenuItem(hwndMenu,
                                      ID_FXMI_DELETE,
                                      fDeleteOK);
                    WinEnableMenuItem(hwndMenu,
                                      ID_FXMI_UNDELETE,
                                      fUndeleteOK);
                    WinEnableMenuItem(hwndMenu,
                                      ID_FXMI_PROTECT,
                                      fProtectOK);
                    WinEnableMenuItem(hwndMenu,
                                      ID_FXMI_UNPROTECT,
                                      fUnprotectOK);

                    cnrhShowContextMenu(hwndCnr,
                                        G_preccSource,
                                        hwndMenu,
                                        hwndFrame);

                    lstFree(&pll);
                }
            }
        }
        break;

        /*
         * CN_EMPHASIS:
         *
         */

        // changed V1.0.9 (2011-11-06) [rwalsh]: eliminated recount of every selected record,
        //                                       eliminated SelectionChanged(),
        //                                       and moved WinStartTimer() here
        case CN_EMPHASIS:
        {
            PNOTIFYRECORDEMPHASIS pnre = (PNOTIFYRECORDEMPHASIS)mp2;

            if (pnre->fEmphasisMask & CRA_SELECTED)
            {
                if (pnre->pRecord->flRecordAttr & CRA_SELECTED)
                    G_cHandlesSelected++;
                else
                    G_cHandlesSelected--;

                WinStartTimer(G_hab,
                              G_hwndMain,
                              TIMERID_SELECTIONCHANGED,
                              200);
            }
        }
        break;
    }

    return mrc;
}

/*
 *@@ fnwpSubclassedMainFrame:
 *
 *@@added V0.9.16 (2001-09-29) [umoeller]
 *@@changed V1.0.4 (2005-02-24) [chennecke]: replaced hard-coded strings with nlsGetString() calls
 *@@changed V1.0.9 (2011-11-03) [rwalsh]: added keyboard accelerator support
 *@@changed V1.0.9 (2011-11-06) [rwalsh]: revised WM_USER* handling
 */

MRESULT EXPENTRY fnwpSubclassedMainFrame(HWND hwndFrame, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    switch (msg)
    {
        case WM_TIMER:
        {
            ULONG ulID = (ULONG)mp1;
            switch (ulID)
            {
                case TIMERID_SELECTIONCHANGED:
                    WinStopTimer(G_hab,
                                 G_hwndMain,
                                 ulID);
                    SetStatusBarText(G_hwndMain, NULL);
                break;

                default:
                    if (G_tidInsertHandlesRunning)
                    {
                        CHAR sz[100];
                        if (G_fResolvingRefs)
                            SetStatusBarText(G_hwndMain,
                                             nlsGetString(ID_FXSI_STATBAR_RESOLVINGPERCENT),
                                             G_ulPercentDone);
                        else
                            SetStatusBarText(G_hwndMain,
                                             nlsGetString(ID_FXSI_STATBAR_PARSINGPERCENT),
                                             G_ulPercentDone);
                    }
                    else if (G_tidCheckFilesRunning)
                    {
                        SetStatusBarText(G_hwndMain,
                                         nlsGetString(ID_FXSI_STATBAR_CHECKINGPERCENT),
                                         G_ulPercentDone);
                    }
            }
        }
        break;

        /*
         * WM_USER:
         *      mp1 has no. of handles or (-1),
         *      mp2 has milliseconds passed.
         *
         * changed V1.0.9 (2011-11-06) [rwalsh]: moved msg display to new function
         */

        case WM_USER:
        {
            // thread is done:
            WinStopTimer(G_hab,
                         G_hwndMain,
                         TIMERID_THREADRUNNING);

            // inserting records generated a CN_EMPHASIS
            // which then started this timer;  if we don't
            // stop it, it will erase our "done" message
            WinStopTimer(G_hab,
                         G_hwndMain,
                         TIMERID_SELECTIONCHANGED);

            ShowInsertCompleteMsg((ULONG)mp1, (ULONG)mp2);
            thrWait(&G_tiInsertHandles);
            UpdateMenuItems(0);
        }
        break;

        // changed V1.0.9 (2011-11-06) [rwalsh]: added WinStopTimer
        case WM_USER + 1:
        {
            // thread is done:
            WinStopTimer(G_hab,
                         G_hwndMain,
                         TIMERID_THREADRUNNING);

            SetStatusBarText(G_hwndMain,
                             nlsGetString(ID_FXSI_STATBAR_DONECHECKING));
            cnrhInvalidateAll(WinWindowFromID(G_hwndMain, FID_CLIENT));
            thrWait(&G_tiCheckFiles);
            UpdateMenuItems(0);
        }
        break;

        /*
         * WM_CONTROL:
         *
         */

        case WM_CONTROL:
        {
            USHORT usID = SHORT1FROMMP(mp1),
                   usNotifyCode = SHORT2FROMMP(mp1);
            if (usID == FID_CLIENT)
                mrc = FrameWMControl(hwndFrame, usNotifyCode, mp2);
        }
        break;

        case WM_INITMENU:
            if (SHORT1FROMMP(mp1) == ID_FXM_ACTIONS)
                WinEnableMenuItem((HWND)mp2,
                                  ID_FXMI_ACTIONS_DELETENOW,
                                  (G_cHandlesDeleted != 0));
            else
                mrc = G_fnwpMainFrameOrig(hwndFrame, msg, mp1, mp2);
        break;

        case WM_MENUEND:
            if (    (    ((HWND)mp2 == G_hwndContextMenuSingle)
                      || ((HWND)mp2 == G_hwndContextMenuMulti)
                    )
                 && (G_preccSource)
               )
                cnrhSetSourceEmphasis(WinWindowFromID(G_hwndMain, FID_CLIENT),
                                      G_preccSource,
                                      FALSE);
        break;

        case WM_COMMAND:
        {
            USHORT  usCmd = SHORT1FROMMP(mp1);
            FrameCommand(hwndFrame, usCmd, G_preccSource);
        }
        break;

        // added V1.0.9 (2011-11-03) [rwalsh]: keyboard accelerator support
        case WM_TRANSLATEACCEL:
        {
            // this sets G_preccSource to the first selected
            // record so accelerator commands work properly;
            // it also defeats typematic so one keypress only
            // generates one command;  if nothing is selected
            // or the msg is the result of typematic, it returns
            // zero so the translation doesn't happen

            ULONG ulPrevDown = (ULONG)(((PQMSG)mp1)->mp1) & KC_PREVDOWN;

            mrc = G_fnwpMainFrameOrig(hwndFrame, msg, mp1, mp2);
            if (mrc)
            {
                if (   ulPrevDown
                    || !(G_preccSource = cnrhQueryNextSelectedRecord(
                                         WinWindowFromID(G_hwndMain, FID_CLIENT),
                                         (PRECORDCORE)CMA_FIRST)))
                    mrc = 0;
            }
        }
        break;

        default:
            mrc = G_fnwpMainFrameOrig(hwndFrame, msg, mp1, mp2);
    }

    return mrc;
}

/* ******************************************************************
 *
 *   main
 *
 ********************************************************************/

BOOL HandleProfile(PCSZ pcszFilename,
                   PCSZ pcszUserOrSystem,
                   HINI *phINI)
{
    ULONG cb;
    APIRET arc;

    if (arc = doshQueryPathSize(pcszFilename, &cb))
    {
        MessageBox(NULLHANDLE,
                   MB_CANCEL,
                   nlsGetString(ID_FXSI_ERROR_PROFILENOTEXIST),
                   pcszUserOrSystem,
                   pcszFilename);
        return FALSE;
    }
    else if (cb == 0)
    {
        MessageBox(NULLHANDLE,
                   MB_CANCEL,
                   nlsGetString(ID_FXSI_ERROR_PROFILEZEROBYTES),
                   pcszUserOrSystem,
                   pcszFilename);
        return FALSE;
    }
    else if (!(*phINI = PrfOpenProfile(G_hab,
                                       pcszFilename)))
    {
        ERRORID id = WinGetLastError(G_hab);
        MessageBox(NULLHANDLE,
                   MB_CANCEL,
                   nlsGetString(ID_FXSI_ERROR_CANTOPENPROFILE),
                   pcszUserOrSystem,
                   pcszFilename,
                   id);
        return FALSE;
    }

    return TRUE;
}

/*
 *@@ ParseArgs:
 *
 *@@added V0.9.19 (2002-04-14) [umoeller]
 *@@changed V1.0.4 (2005-02-24) [chennecke]: replaced hard-coded strings with nlsGetString() calls
 */

BOOL ParseArgs(int argc, char* argv[])
{
    int i = 0;
    while (i++ < argc - 1)
    {
        if (argv[i][0] == '-')
        {
            ULONG i2;
            for (i2 = 1; i2 < strlen(argv[i]); i2++)
            {
                BOOL fNextArg = FALSE;

                switch (argv[i][i2])
                {
                    case 'U':
                        if (!HandleProfile(&argv[i][i2+1],
                                           nlsGetString(ID_FXSI_ERROR_USER),
                                           &G_hiniUser))
                            return FALSE;

                        fNextArg = TRUE;
                    break;

                    case 'S':
                        if (!HandleProfile(&argv[i][i2+1],
                                           nlsGetString(ID_FXSI_ERROR_SYSTEM),
                                           &G_hiniSystem))
                            return FALSE;

                        fNextArg = TRUE;
                    break;

                    default:
                        MessageBox(NULLHANDLE,
                                   MB_CANCEL,
                                   nlsGetString(ID_FXSI_ERROR_INVALIDPARMC),
                                   argv[i][i2]);
                        return FALSE;
                }

                if (fNextArg)
                    break; // for (i2 = 1; i2 < strlen(argv[i]); i2++)

            } // end for (i2 = 1; i2 < strlen(argv[i]); i2++)
        } // end if (argv[i][0] == '-')
        else
        {
            MessageBox(NULLHANDLE,
                       MB_CANCEL,
                       nlsGetString(ID_FXSI_ERROR_INVALIDPARMS),
                       argv[i]);
            return FALSE;
        } // end else if (argv[i][0] == '-')
    } // end while ((i++ < argc-1) && (fContinue))

    return TRUE;
}

/*
 *@@ LoadNLS:
 *      xfix NLS interface.
 *
 *@@added V1.0.4 (2005-02-24) [chennecke]: copied from treesize.c
 */
 
BOOL LoadNLS(VOID)
{
    CHAR        szNLSDLL[2*CCHMAXPATH];
    BOOL Proceed = TRUE;
 
    if (PrfQueryProfileString(HINI_USER,
                              "XWorkplace",
                              "XFolderPath",
                              "",
                              szNLSDLL, sizeof(szNLSDLL)) < 3)
 
    {
        WinMessageBox(HWND_DESKTOP, HWND_DESKTOP,
                      "xfix was unable to determine the location of the "
                      "XWorkplace National Language Support DLL, which is "
                      "required for operation. The OS2.INI file does not contain "
                      "this information. "
                      "xfix cannot proceed. Please re-install XWorkplace.",
                      "xfix: Fatal Error",
                      0, MB_OK | MB_MOVEABLE);
        Proceed = FALSE;
    }
    else
    {
        CHAR    szLanguageCode[50] = "";
 
        // now compose module name from language code
        PrfQueryProfileString(HINI_USERPROFILE,
                              "XWorkplace", "Language",
                              "001",
                              (PVOID)szLanguageCode,
                              sizeof(szLanguageCode));
 
        // allow '?:\' for boot drive
        // V0.9.19 (2002-06-08) [umoeller]
        if (szNLSDLL[0] == '?')
        {
            ULONG ulBootDrive;
            DosQuerySysInfo(QSV_BOOT_DRIVE, QSV_BOOT_DRIVE,
                            &ulBootDrive,
                            sizeof(ulBootDrive));
            szNLSDLL[0] = (CHAR)ulBootDrive + 'A' - 1;
        }
 
        strcat(szNLSDLL, "\\bin\\xfldr");
        strcat(szNLSDLL, szLanguageCode);
        strcat(szNLSDLL, ".dll");
 
        // try to load the module
        if (DosLoadModule(NULL,
                          0,
                          szNLSDLL,
                          &G_hmodNLS))
        {
            CHAR    szMessage[2000];
            sprintf(szMessage,
                    "xfix was unable to load \"%s\", "
                    "the National Language DLL which "
                    "is specified for XWorkplace in OS2.INI.",
                    szNLSDLL);
            WinMessageBox(HWND_DESKTOP, HWND_DESKTOP,
                          szMessage,
                          "xfix: Fatal Error",
                          0, MB_OK | MB_MOVEABLE);
            Proceed = FALSE;
        }
 
        _Pmpf(("DosLoadModule: 0x%lX", G_hmodNLS));
    }
    return (Proceed);
}

/*
 *@@ morph:
 *      Enable this program to use PM functions if it
 *      was linked or started as a VIO app - this has
 *      no effect when linked or started as a PM app.
 *      (Starting the app as VIO provides a console
 *      window to display printf() output without
 *      having to use pmPrintf.)
 *
 *@@added V1.0.9 (2011-10-25) [rwalsh]
 */
void        Morph( void)

{
    PPIB    ppib;
    PTIB    ptib;

    DosGetInfoBlocks( &ptib, &ppib);
    ppib->pib_ultype = 3;
    return;
}

/*
 *@@ main:
 *
 *@@added V0.9.16 (2001-09-29) [umoeller]
 *@@changed V0.9.19 (2002-07-01) [umoeller]: added error msgs
 *@@changed V1.0.4 (2005-02-24) [chennecke]: added national language support
 *                                           renamed IDM... identifiers to ID_FXM...
 *@@changed V1.0.9 (2011-10-29) [rwalsh]: added record icons
 *@@changed V1.0.9 (2011-11-03) [rwalsh]: added keyboard accelerator support
 *@@changed V1.0.9 (2011-11-04) [rwalsh]: added statusbar format string
*/

int main(int argc, char* argv[])
{
    APIRET      arc;

    HMQ         hmq;
    QMSG        qmsg;

    Morph();

    DosError(FERR_DISABLEHARDERR | FERR_ENABLEEXCEPTION);

    if (!(G_hab = WinInitialize(0)))
        return 1;

    if (!(hmq = WinCreateMsgQueue(G_hab, 0)))
        return 1;

    G_hptrMain = WinLoadPointer(HWND_DESKTOP, NULLHANDLE, 1);

    if (LoadNLS()) // continue only if NLS DLL loaded V1.0.4 (2005-02-24) [chennecke]
    {
        ULONG cb = sizeof(G_cThousands);
        PrfQueryProfileData(G_hiniUser,
                            "PM_National",
                            "sThousand",
                            G_cThousands,
                            &cb);

        lstInit(&G_llDeferredNukes, TRUE);

        if (!(ParseArgs(argc, argv)))
            return 1;

        // initialize NLS cache V1.0.4 (2005-02-24) [chennecke]
        ULONG G_EntitiesNumber = sizeof(G_aEntities) / sizeof (G_aEntities[0]);
        nlsInitStrings(G_hab,
                       G_hmodNLS,
                       G_aEntities,
                       G_EntitiesNumber);

        // create frame and handles container
        HWND hwndCnr = NULLHANDLE;

        EXTFRAMECDATA xfd =
        {
                  0,
                  FCF_TITLEBAR
                     | FCF_SYSMENU
                     | FCF_MINMAX
                     | FCF_SIZEBORDER
                     | FCF_ICON
                      // removed FCF_MENU, load menu from NLS DLL instead V1.0.4 (2005-02-25) [chennecke]
                      // | FCF_MENU
                     | FCF_TASKLIST,
                  XFCF_STATUSBAR,
                  0,
                      nlsGetString(ID_FXDI_MAIN),
                  1,     // icon resource
                  WC_CONTAINER,
                  CCS_MINIICONS | CCS_READONLY | CCS_EXTENDSEL
                     | WS_VISIBLE,
                  0,
                  NULL
        };

        if (    (G_hwndMain = winhCreateExtStdWindow(&xfd,
                                                     &hwndCnr))
             && (hwndCnr)
           )
        {
            // subclass cnr (it's our client)
            G_pfnwpCnrOrig = WinSubclassWindow(hwndCnr, fnwpSubclassedMainCnr);

            // subclass frame for supporting msgs
            G_fnwpMainFrameOrig = WinSubclassWindow(G_hwndMain,
                                                    fnwpSubclassedMainFrame);

            // load main menu from NLS DLL V1.0.4 (2005-02-25) [chennecke]
            WinLoadMenu(G_hwndMain, G_hmodNLS, ID_FXM_MAIN);

            // build format string for statusbar counts
            // added V1.0.9 (2011-11-04) [rwalsh]
            sprintf(G_szStatusFmt,
                    "  %% 6s %s   |  %% 6s %s   |  %% 6s %s   " // no comma here
                    "|  %% 6s %s   |  %% 6s %s   |   ",
                    nlsGetString(ID_FXSI_STATBAR_HANDLES),
                    nlsGetString(ID_FXSI_STATBAR_SELECTED),
                    nlsGetString(ID_FXSI_STATBAR_INVALID),
                    nlsGetString(ID_FXSI_STATBAR_DELETED),
                    nlsGetString(ID_FXSI_STATBAR_ORPHANS));
        
            SetStatusBarText(G_hwndMain,
                             nlsGetString(ID_FXSI_STATBAR_PARSINGHANDLES));

            SetupMainCnr(hwndCnr);

            // load icons
            // added V1.0.9 (2011-10-29) [rwalsh]: record icons
            G_iconBlank   = WinLoadPointer(HWND_DESKTOP,
                                           NULLHANDLE,
                                           ID_FXICO_BLANK);
            G_iconError   = WinLoadPointer(HWND_DESKTOP,
                                           NULLHANDLE,
                                           ID_FXICO_ERROR);
            G_iconDelete  = WinLoadPointer(HWND_DESKTOP,
                                           NULLHANDLE,
                                           ID_FXICO_DELETE);
            G_iconProtect = WinLoadPointer(HWND_DESKTOP,
                                           NULLHANDLE,
                                           ID_FXICO_PROTECT);
            G_iconSafe    = WinLoadPointer(HWND_DESKTOP,
                                           NULLHANDLE,
                                           ID_FXICO_SAFE);
            G_iconOrphan  = WinLoadPointer(HWND_DESKTOP,
                                           NULLHANDLE,
                                           ID_FXICO_ORPHAN);

            // load popup menus
            G_hwndContextMenuSingle = WinLoadMenu(hwndCnr,
                                                  G_hmodNLS,
                                                  ID_FXM_RECORDSELSINGLE);
            G_hwndContextMenuMulti  = WinLoadMenu(hwndCnr,
                                                  G_hmodNLS,
                                                  ID_FXM_RECORDSELMULTI);

            // load keyboard accelerator table
            // added V1.0.9 (2011-11-03) [rwalsh]
            HACCEL hAccel = WinLoadAccelTable(G_hab,
                                              G_hmodNLS,
                                              ID_FX_ACCEL);
            if (hAccel)
                WinSetAccelTable(G_hab,
                                 hAccel,
                                 G_hwndMain);

            // load NLS help library (..\help\xfldr001.hlp)
            PPIB     ppib;
            PTIB     ptib;
            CHAR     szHelpName[CCHMAXPATH];
            DosGetInfoBlocks(&ptib, &ppib);
            DosQueryModuleName(ppib->pib_hmte, sizeof(szHelpName), szHelpName);
                // now we have: "J:\Tools\WPS\XWorkplace\bin\xfix.exe"
            PSZ pszLastBackslash;
            if (pszLastBackslash = strrchr(szHelpName, '\\'))
            {
                *pszLastBackslash = 0;
                // again to get rid of "bin"
                if (pszLastBackslash = strrchr(szHelpName, '\\'))
                {
                    *pszLastBackslash = 0;
                    // now we have: "J:\Tools\WPS\XWorkplace"
                    CHAR szLanguage[10];
                    PrfQueryProfileString(G_hiniUser,
                                          INIAPP,
                                          "Language",
                                          "001",        // default
                                          szLanguage,
                                          sizeof(szLanguage));
                    sprintf(szHelpName + strlen(szHelpName),
                            "\\help\\xfldr%s.hlp",
                            szLanguage);
                    G_hwndHelp = winhCreateHelp(G_hwndMain,
                                                szHelpName,
                                                NULLHANDLE,
                                                NULL,
                                                "xfix");
                }
            }

            if (!winhRestoreWindowPos(G_hwndMain,
                                      G_hiniUser,
                                      INIAPP,
                                      INIKEY_MAINWINPOS,
                                      SWP_SHOW | SWP_ACTIVATE | SWP_MOVE | SWP_SIZE))
                WinSetWindowPos(G_hwndMain,
                                HWND_TOP,
                                10, 10, 500, 500,
                                SWP_SHOW | SWP_ACTIVATE | SWP_MOVE | SWP_SIZE);

            // load handles from OS2.INI
            PSZ pszActiveHandles;
            if (arc = wphQueryActiveHandles(G_hiniSystem,
                                            &pszActiveHandles))
                MessageBox(NULLHANDLE,
                           MB_CANCEL,
                           nlsGetString(ID_FXSI_ERROR_CANTGETACTIVEHANDLES));
            else
            {
                PCSZ pcszError;

                // added error messages here V0.9.19 (2002-07-01) [umoeller]

                if (arc = wphLoadHandles(G_hiniUser,
                                         G_hiniSystem,
                                         pszActiveHandles,
                                         (HHANDLES*)&G_pHandlesBuf))
                {
                    // if we fail here, then the handles data is totally
                    // broken, and we have no chance to recover
                    // V0.9.19 (2002-07-01) [umoeller]

                    if (!(pcszError = wphDescribeError(arc)))
                        pcszError = nlsGetString(ID_FXSI_ERROR_NOTAVAILABLE);

                    MessageBox(NULLHANDLE,
                               MB_CANCEL,
                               nlsGetString(ID_FXSI_ERROR_FATALLOADHANDLES),
                               arc,
                               pcszError);
                }
                else
                {
                    if (arc = wphRebuildNodeHashTable((HHANDLES)G_pHandlesBuf,
                                                      FALSE))
                    {
                        if (!(pcszError = wphDescribeError(arc)))
                            pcszError = nlsGetString(ID_FXSI_ERROR_NOTAVAILABLE);

                        MessageBox(NULLHANDLE,
                                   MB_CANCEL,
                                   nlsGetString(ID_FXSI_ERROR_BUILDHANDLESCACHE),
                                   arc,
                                   pcszError);
                    }

                    StartInsertHandles(hwndCnr);

                    // display introductory help with warnings
                    WinPostMsg(G_hwndMain,
                               WM_COMMAND,
                               (MPARAM)ID_FXMI_HELP_GENERAL,
                               0);

                    // standard PM message loop
                    while (WinGetMsg(G_hab, &qmsg, NULLHANDLE, 0, 0))
                        WinDispatchMsg(G_hab, &qmsg);

                    if (G_tidInsertHandlesRunning)
                        thrFree(&G_tiInsertHandles);
                    if (G_tidCheckFilesRunning)
                        thrFree(&G_tiCheckFiles);
                }
            }
        }

        if (G_hiniUser != HINI_USER)
            PrfCloseProfile(G_hiniUser);
        if (G_hiniSystem != HINI_USER)
            PrfCloseProfile(G_hiniSystem);
    } // end if (LoadNLS)

    // clean up on the way out
    WinDestroyMsgQueue(hmq);
    WinTerminate(G_hab);

    return (G_ulrc);        // 0 if nothing changed
                            // 1 if OS2SYS.INI was changed
}
