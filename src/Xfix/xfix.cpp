
/*
 *@@sourcefile xfix.cpp:
 *      the one and only source file for xfix.exe, the
 *      "XWorkplace handles fixer".
 *
 *      This was started as a quick hack in 2000... since
 *      my WPS got so confused with its handles in 2001,
 *      I finished this today (2001-01-21). Out of my
 *      11.000 handles, I managed to delete about 3.000,
 *      and my WPS booted again.
 *
 *@@header "xfix.h"
 *@@added V0.9.5 (2000-08-13) [umoeller]
 */

/*
 *      Copyright (C) 2000-2001 Ulrich M”ller.
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
#include "helpers\except.h"
#include "helpers\linklist.h"
#include "helpers\prfh.h"
#include "helpers\standards.h"
#include "helpers\stringh.h"
#include "helpers\threads.h"
#include "helpers\tree.h"
#include "helpers\winh.h"
#include "helpers\wphandle.h"
#include "helpers\xstring.h"

#include "shared\common.h"
#include "shared\helppanels.h"          // all XWorkplace help panel IDs

#include "bldlevel.h"

#include "xfix.h"

/* ******************************************************************
 *                                                                  *
 *   Global variables                                               *
 *                                                                  *
 ********************************************************************/

ULONG       G_ulrc = 0;         // return code from main()

HWND        G_hwndMain = NULLHANDLE;
HWND        G_hwndStatusBar = NULLHANDLE;
HWND        G_hwndContextMenuSingle = NULLHANDLE,
            G_hwndContextMenuMulti = NULLHANDLE;
HWND        G_hwndHelp = NULLHANDLE;        // help instance

PFNWP       G_pfnwpCnrOrig = NULL,
            G_fnwpFrameOrig = NULL;

PBYTE       G_pHandlesBuffer = NULL;
ULONG       G_cbHandlesBuffer = 0;

PDRIV       G_aDriveNodes[27] = {0};        // drive nodes for each drive
PNODE       G_aRootNodes[27] = {0};         // nodes for each root dir

THREADINFO  G_tiInsertHandles = {0},
            G_tiCheckFiles = {0};
ULONG       G_tidInsertHandlesRunning = 0,
            G_tidCheckFilesRunning = 0;
BOOL        G_fResolvingRefs = FALSE;
ULONG       G_ulPercentDone = 0;

ULONG       G_cHandlesParsed = 0;
ULONG       G_cDuplicatesFound = 0;

ULONG       G_ulHiwordFileSystem = 0;
                                // hi-word for file-system handles
                                // (calc'd in main())

LINKLIST    G_llDeferredNukes;

const char  *INIAPP                 = "XWorkplace";
const char  *INIKEY_MAINWINPOS      = "HandleFixWinPos";

#define     TIMERID_THREADRUNNING  998

#define NODESTAT_OK                     0x0000
#define NODESTAT_DUPLICATEDRIV          0x0001
#define NODESTAT_ISDUPLICATE            0x0002
#define NODESTAT_INVALIDDRIVE           0x0004
#define NODESTAT_INVALID_PARENT         0x0008

#define NODESTAT_FILENOTFOUND           0x0010
#define NODESTAT_PARENTNOTFOLDER        0x0020
#define NODESTAT_FOLDERPOSNOTFOLDER     0x0040
#define NODESTAT_ABSTRACTSNOTFOLDER     0x0080

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
 */

typedef struct _NODERECORD
{
    RECORDCORE  recc;

    struct _NODERECORD  *pNextRecord;            // next record;

    ULONG       ulIndex;
            // index

    ULONG       ulStatus;
            // status of this thing... this is
            // any combination of:
            // -- NODESTAT_OK
            // -- NODESTAT_DUPLICATEDRIV    (duplicate DRIV node)
            // -- NODESTAT_ISDUPLICATE      (duplicate NODE node)
            // -- NODESTAT_INVALIDDRIVE     (drive doesn't exist)
            // -- NODESTAT_INVALID_PARENT   (parent handle doesn't exist)
            // After checking files, the following can be set in addition:
            // -- NODESTAT_FILENOTFOUND
            // -- NODESTAT_PARENTNOTFOLDER  (parent handle is not a folder)
            // -- NODESTAT_FOLDERPOSNOTFOLDER (folderpos entry for non-folder)
            // -- NODESTAT_ABSTRACTSNOTFOLDER (abstracts in non-folder)

    ULONG       ulIsFolder;
            // inititially -1; after checking files, either
            // TRUE or FALSE, depending on whether file is
            // a folder

    PSZ         pszStatusDescr;
            // descriptive text of the status

    PSZ         pszType;        // static text

    ULONG       ulOfs;          // offset of NODE in global buffer

    ULONG       ulHandle;       // handle
    ULONG       ulParentHandle; // parent handle

    struct _NODERECORD *pNextDuplicate;
                        // if there is a duplicate NODE (i.e. a NODE
                        // with the same handle), this points to it;
                        // this is a linked list then if several
                        // duplicates exist (never seen this);
                        // the first entry of the linked list is
                        // stored in the records hash table
    ULONG       cDuplicates;
                        // no. of duplicates, if any

    CHAR        szHexHandle[10];
    PSZ         pszHexHandle;       // points to szHexHandle always

    CHAR        szHexParentHandle[10];
    PSZ         pszHexParentHandle; // points to szHexParentHandle always

    ULONG       cChildren;          // references to this handle

    ULONG       cAbstracts;        // abstract objects in this folder
    BOOL        fFolderPos;         // if TRUE, folderpos entry exists

    PSZ         pszRefcsDescr;
            // descriptive text for references

    PSZ         pszShortNameCopy;   // points into szShortNameCopy
    CHAR        szShortNameCopy[CCHMAXPATH];

    PSZ         pszLongName;        // points to szLongName
    CHAR        szLongName[2*CCHMAXPATH];
} NODERECORD, *PNODERECORD;

// root of records list
PNODERECORD     G_preccVeryFirst = NULL;

// nodes hash table
PNODE           G_NodeHashTable[65536];

// NODERECORD hash table
PNODERECORD     G_RecordHashTable[65536];

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
    HINI        hini;           // normally HINI_USER
    const char  *pcszApp;       // INI application
    PSZ         pszKey;         // INI key

    PVOID       pvData;         // or NULL for nuke
    ULONG       cbData;         // or 0 for nuke
} DEFERREDINI, *PDEFERREDINI;

// deferred changes to the desktop contents
BOOL                G_fDesktopLoaded = FALSE;
PNODERECORD         G_precDesktop = NULL;
PULONG              G_paulAbstractsTarget = NULL;
CHAR                G_szHandleDesktop[10];          // hex handle of desktop
ULONG               G_cAbstractsDesktop = 0;
ULONG               G_cTotalAbstractsMoved = 0;

VOID UpdateStatusBar(LONG lSeconds);

VOID MarkAllOrphans(PNODERECORD prec,
                    ULONG ulParentHandle);

/* ******************************************************************
 *
 *   Handles helpers
 *
 ********************************************************************/

/*
 *@@ wphFastFindPartName:
 *      similar to wphFindPartName, but this function doesn't
 *      search the whole buffer all the time, but uses a hash table
 *      instead. This is several times as fast as wphFindPartName.
 *      This function recurses, if neccessary.
 *
 *@@added V0.9.5 (2000-08-13) [umoeller]
 */

PNODE wphFastFindPartName(USHORT usHandle,      // in: handle to search for
                          PSZ pszFname,         // out: object partname
                          USHORT usMax)         // in: sizeof(pszFname)
{
    PNODE pNode = G_NodeHashTable[usHandle];
    if (pNode)
    {
        // handle exists:
        ULONG usSize = usMax - strlen(pszFname);
        if (usSize > pNode->usNameSize)
            usSize = pNode->usNameSize;
        if (pNode->usParentHandle)
        {
            // node has parent:
            if (!wphFastFindPartName(pNode->usParentHandle,
                                     pszFname,
                                     usMax))
                // parent handle not found:
                return (NULL);
            strcat(pszFname, "\\");
            strncat(pszFname, pNode->szName, usSize);
            return pNode;
        }
        else
        {
            strncpy(pszFname, pNode->szName, usSize);
            return pNode;
        }
    }

    return (pNode);
}

/*
 *@@ ComposeFullName:
 *
 */

BOOL ComposeFullName(PNODERECORD precc,
                     PNODE pNode)
{
    if (wphFastFindPartName(pNode->usHandle,
                            precc->szLongName,
                            sizeof(precc->szLongName)))
        precc->pszLongName = precc->szLongName;
    return (TRUE);
}

/*
 *@@ RebuildNodeHashTable:
 *
 */

VOID RebuildNodeHashTable(VOID)
{
    memset(G_NodeHashTable, 0, sizeof(G_NodeHashTable));

    PBYTE pEnd = G_pHandlesBuffer + G_cbHandlesBuffer;

    // start at beginning of buffer
    PBYTE pCur = G_pHandlesBuffer + 4;

    // now set up hash table
    while (pCur < pEnd)
    {
        if (!memicmp(pCur, "DRIV", 4))
        {
            // pCur points to a DRIVE node:
            // these never have handles, so skip this
            PDRIV pDriv = (PDRIV)pCur;
            pCur += sizeof(DRIV) + strlen(pDriv->szName);
        }
        else if (!memicmp(pCur, "NODE", 4))
        {
            // pCur points to a regular NODE: offset pointer first
            PNODE pNode = (PNODE)pCur;
            // store PNODE in hash table
            G_NodeHashTable[pNode->usHandle] = pNode;
            pCur += sizeof (NODE) + pNode->usNameSize;
        }
    }
}

/*
 *@@ RebuildRecordsHashTable:
 *
 *      Only called after records have been removed.
 *
 *@@added V0.9.9 (2001-04-07) [umoeller]
 */

VOID RebuildRecordsHashTable(VOID)
{
    memset(G_RecordHashTable, 0, sizeof(G_RecordHashTable));

    PNODERECORD prec = G_preccVeryFirst;
    while (prec)
    {
        if (!G_RecordHashTable[prec->ulHandle])
            // not yet set:
            G_RecordHashTable[prec->ulHandle] = prec;
        // otherwise it's a duplicate handle, but this
        // has been tested for already in fntInsertHandles

        prec = prec->pNextRecord;
    }
}

/* ******************************************************************
 *
 *   Handle records
 *
 ********************************************************************/

/*
 *@@ Append:
 *
 */

VOID Append(PXSTRING pstr, const char *pcsz)
{
    if (pstr->ulLength)
        xstrcat(pstr, "; ", 0);
    xstrcat(pstr, pcsz, 0);
}

/*
 *@@ UpdateStatusDescr:
 *
 */

VOID UpdateStatusDescr(PNODERECORD prec)
{
    XSTRING     str;
    xstrInit(&str, 0);

    // node status
    if (prec->ulStatus & NODESTAT_DUPLICATEDRIV)
        Append(&str, "Duplicate DRIV");

    if (prec->ulStatus & NODESTAT_ISDUPLICATE)
        Append(&str, "Duplicate");

    if (prec->ulStatus & NODESTAT_INVALIDDRIVE)
        Append(&str, "Invalid drive");

    if (prec->ulStatus & NODESTAT_INVALID_PARENT)
        Append(&str, "Orphaned");

    if (prec->ulStatus & NODESTAT_FILENOTFOUND)
        Append(&str, "File not found");

    if (prec->ulStatus & NODESTAT_PARENTNOTFOLDER)
        Append(&str, "Parent is not folder");

    if (prec->ulStatus & NODESTAT_FOLDERPOSNOTFOLDER)
        Append(&str, "Folderpos for non-folder");

    if (prec->ulStatus & NODESTAT_ABSTRACTSNOTFOLDER)
        Append(&str, "Abstracts in non-folder");

    if (prec->pszStatusDescr)
    {
        free(prec->pszStatusDescr);
        prec->pszStatusDescr = NULL;
    }

    if (str.ulLength)
    {
        // we had an error: set "picked" emphasis
        prec->pszStatusDescr = str.psz;
        prec->recc.flRecordAttr |= CRA_PICKED;
    }
    else
        prec->recc.flRecordAttr &= ~CRA_PICKED;

    // references
    if (prec->pszRefcsDescr)
    {
        free(prec->pszRefcsDescr);
        prec->pszRefcsDescr = NULL;
    }

    if (prec->cAbstracts || prec->fFolderPos)
    {
        xstrInit(&str, 20);
                // do not free, we've used that string above

        if (prec->cAbstracts)
        {
            CHAR sz[50];
            sprintf(sz, "%d ABS", prec->cAbstracts);
            Append(&str, sz);
        }

        if (prec->fFolderPos)
            Append(&str, "FPOS");

        if (str.ulLength)
            prec->pszRefcsDescr = str.psz;
    }
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

int TREEENTRY CompareFolderPosNodes(TREE *t1, TREE *t2)
{
    return (strhcmp((const char*)(t1->id),
                    (const char*)(t2->id)));
}

/*
 *@@ CompareFolderNodesData:
 *
 *@@added V0.9.9 (2001-04-07) [umoeller]
 */

int TREEENTRY CompareFolderNodesData(TREE *t1,
                                     void *pData)
{
    char *pcsz = (char*)pData;
    if (pcsz)
    {
        ULONG cbLength = min(strlen((const char*)(t1->id)),
                             strlen(pcsz));
        int i = memicmp((void*)(t1->id),
                        pcsz,
                        cbLength);
        if (i < 0)
            return -1;
        if (i > 0)
            return +1;
    }

    return (0);
}

/*
 *@@ HasFolderPos:
 *
 *@@added V0.9.9 (2001-04-07) [umoeller]
 */

BOOL HasFolderPos(TREE **pFolderPosTree,
                  char *pcszDecimalHandle)
{
    return (!!treeFindEQData(pFolderPosTree,
                             pcszDecimalHandle,
                             CompareFolderNodesData));
}

/*
 *@@ fntInsertHandles:
 *
 *@@changed V0.9.9 (2001-04-07) [umoeller]: fixed wrong duplicates reports for UNC drive names
 *@@changed V0.9.9 (2001-04-07) [umoeller]: sped up folder pos search fourfold
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
        memset(&G_aDriveNodes, 0, sizeof(G_aDriveNodes));
        memset(&G_aRootNodes, 0, sizeof(G_aRootNodes));

        // invalidate records hash table
        memset(G_RecordHashTable, 0, sizeof(G_RecordHashTable));

        if (G_pHandlesBuffer)
        {
            // got handles buffer:
            // now set the pointer for the end of the BLOCKs buffer
            PBYTE pEnd = G_pHandlesBuffer + G_cbHandlesBuffer;

            // pCur is our variable pointer where we're at now; there
            // is some offset of 4 bytes at the beginning (duh)
            PBYTE pCur = G_pHandlesBuffer + 4;

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
                    PDRIV pDriv = (PDRIV)pCur;
                    cReccsTotal++;
                    pCur += sizeof(DRIV) + strlen(pDriv->szName);
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
                if (!(arc = prfhQueryKeysForApp(HINI_USER,
                                                "PM_Workplace:FolderPos",
                                                &pszFolderPoses)))
                {
                    TREE    *FolderPosTree;
                    treeInit(&FolderPosTree);

                    // build tree from folderpos entries V0.9.9 (2001-04-07) [umoeller]
                    const char *pcszFolderPosThis = pszFolderPoses;
                    while (*pcszFolderPosThis)
                    {
                        TREE    *pNewNode = NEW(TREE);
                        pNewNode->id = (ULONG)pcszFolderPosThis;
                        treeInsertNode(&FolderPosTree,
                                       pNewNode,
                                       CompareFolderPosNodes,
                                       FALSE);       // no duplicates
                                // @@ free the tree nodes
                        pcszFolderPosThis += strlen(pcszFolderPosThis) + 1;   // next type/filter
                    }

                    if ((G_preccVeryFirst) && (pszFolderPoses))
                    {
                        PNODERECORD preccThis = G_preccVeryFirst;

                        // restart at beginning of buffer
                        pCur = G_pHandlesBuffer + 4;
                        // now set up records
                        ULONG   ulIndexThis = 0;
                        while (pCur < pEnd)
                        {
                            // copy ptr to next record as given to
                            // us by the container;
                            // I'm not sure this will always remain valid
                            preccThis->pNextRecord = (PNODERECORD)(preccThis->recc.preccNextRecord);

                            preccThis->ulIndex = ulIndexThis++;
                            preccThis->ulOfs = (pCur - G_pHandlesBuffer);

                            preccThis->ulStatus = NODESTAT_OK;

                            preccThis->ulIsFolder = -1;     // unknown now

                            if (!memicmp(pCur, "DRIV", 4))
                            {
                                // pCur points to a DRIVE node:
                                // we don't care about these, because the root
                                // directory has a real NODE too, so we just
                                // skip this
                                PDRIV pDriv = (PDRIV)pCur;
                                preccThis->pszType = "DRIV";
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
                                    ULONG ulLogicalDrive =   pDriv->szName[0] // drive letter
                                                           - 'A'
                                                           + 1;

                                    if (G_aDriveNodes[ulLogicalDrive] == 0)
                                        // drive not occupied yet:
                                        G_aDriveNodes[ulLogicalDrive] = pDriv;
                                    else
                                    {
                                        // that's a duplicate DRIV node!!!
                                        preccThis->ulStatus |= NODESTAT_DUPLICATEDRIV;
                                        G_cDuplicatesFound++;
                                    }
                                }

                                pCur += sizeof(DRIV) + strlen(pDriv->szName);
                            }
                            else if (!memicmp(pCur, "NODE", 4))
                            {
                                // pCur points to a regular NODE: offset pointer first
                                PNODE pNode = (PNODE)pCur;
                                preccThis->pszType = "NODE";

                                preccThis->ulHandle = pNode->usHandle;
                                sprintf(preccThis->szHexHandle, "%04lX", pNode->usHandle);
                                preccThis->pszHexHandle = preccThis->szHexHandle;

                                preccThis->ulParentHandle = pNode->usParentHandle;
                                sprintf(preccThis->szHexParentHandle, "%04lX", pNode->usParentHandle);
                                preccThis->pszHexParentHandle = preccThis->szHexParentHandle;

                                strcpy(preccThis->szShortNameCopy, pNode->szName);
                                preccThis->pszShortNameCopy = preccThis->szShortNameCopy;

                                ComposeFullName(preccThis, pNode);

                                if (    (preccThis->szLongName[0] >= 'A')
                                     && (preccThis->szLongName[0] <= 'Z')
                                     && (strlen(preccThis->szLongName) == 2)
                                   )
                                {
                                    // this is a root node:
                                    // store this in global drives array
                                    ULONG ulLogicalDrive =   preccThis->szLongName[0]
                                                                    // drive letter
                                                           - 'A'
                                                           + 1;

                                    if (ulLogicalDrive <= 27)
                                    {
                                        if (G_aRootNodes[ulLogicalDrive] == 0)
                                            // root not occupied yet:
                                            G_aRootNodes[ulLogicalDrive] = pNode;
                                    }
                                    else
                                        preccThis->ulStatus |= NODESTAT_INVALIDDRIVE;
                                }

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
                                if (    (PrfQueryProfileSize(HINI_USER,
                                                             "PM_Abstract:FldrContent",
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
                                CHAR szDecimalHandle[30];
                                sprintf(szDecimalHandle,
                                        "%d@",
                                        // compose full handle (hiword)
                                        (ULONG)(pNode->usHandle)
                                              | (G_ulHiwordFileSystem << 16L));

                                preccThis->fFolderPos = HasFolderPos(&FolderPosTree,
                                                                     szDecimalHandle);

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
                            ULONG ulMax = (pEnd - G_pHandlesBuffer);
                            ULONG ulNow = (pCur - G_pHandlesBuffer);
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
                                    PNODE pNodeStart = G_NodeHashTable[preccThis->ulHandle],
                                          pNodeThis = pNodeStart;

                                    PNODERECORD pPrevParent = NULL;

                                    do
                                    {
                                        PNODERECORD pParentRec = NULL;

                                        if (pNodeThis->usParentHandle)
                                        {
                                            // we have a parent:
                                            pParentRec
                                                = G_RecordHashTable[pNodeThis->usParentHandle];

                                            if (pParentRec)
                                            {
                                                // parent found:
                                                // raise its usage count
                                                pParentRec->cChildren++;

                                                // go for next higher parent
                                                pNodeThis = G_NodeHashTable[pParentRec->ulHandle];

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

                } // end  if (!(arc = prfhQueryKeysForApp(HINI_USER, "PM_Workplace:FolderPos", &pszFolderPoses)))

            } // end if (cReccs)
        }
    }
    CATCH(excpt1)
    {
    } END_CATCH();

    // cnrhInvalidateAll(hwndCnr);

    ULONG ulMillisecondsNow = dtGetULongTime();

    WinPostMsg(G_hwndMain,
               WM_USER,
               (MPARAM)cReccsTotal,
               (MPARAM)(ulMillisecondsNow - ulMillisecondsStarted));
}

/* ******************************************************************
 *                                                                  *
 *   Check Files thread                                             *
 *                                                                  *
 ********************************************************************/

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
            FILESTATUS3 fs3;
            if (DosQueryPathInfo(preccThis->szLongName,
                                 FIL_STANDARD,
                                 &fs3,
                                 sizeof(fs3))
                    != NO_ERROR)
            {
                preccThis->ulStatus |= NODESTAT_FILENOTFOUND;
            }
            else
            {
                // mark record as file or folder
                if (fs3.attrFile & FILE_DIRECTORY)
                    preccThis->ulIsFolder = TRUE;
                else
                    preccThis->ulIsFolder = FALSE;
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
    // valid references to folders... sometimes we have folderpos
    // entries for non-folders, or parent handles which are not
    // pointing to a folder
    preccThis = G_preccVeryFirst;
    ULONG cReccs2 = 0;
    while (preccThis)
    {
        BOOL fChanged = FALSE;
        // is this a NODE? (DRIV handle is 0)
        if (preccThis->ulHandle)
        {
            // get the NODE from the record hash table
            PNODE pNodeThis = G_NodeHashTable[preccThis->ulHandle];
            if (pNodeThis->usParentHandle)
            {
                // record has parent:
                PNODERECORD pParentRec = G_RecordHashTable[pNodeThis->usParentHandle];

                if (pParentRec)
                {
                    if (pParentRec->ulIsFolder == FALSE)
                    {
                        // parent record is not a folder:

                        preccThis->ulStatus |= NODESTAT_PARENTNOTFOLDER;
                        fChanged = TRUE;
                    }
                }
            }

        } // end if (preccThis->ulHandle)

        // check if we have a folderpos entry for a non-folder
        if (preccThis->ulIsFolder == FALSE)
        {
            if (preccThis->fFolderPos)
            {
                preccThis->ulStatus |= NODESTAT_FOLDERPOSNOTFOLDER;
                fChanged = TRUE;
            }
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

    WinPostMsg(G_hwndMain,
               WM_USER + 1,
               (MPARAM)0, 0);
}

/* ******************************************************************
 *                                                                  *
 *   Container sort procs                                           *
 *                                                                  *
 ********************************************************************/

LONG inline CompareULongs(PNODERECORD precc1, PNODERECORD precc2, ULONG ulFieldOfs)
{
    ULONG   ul1 = *(PULONG)((PBYTE)precc1 + ulFieldOfs),
            ul2 = *(PULONG)((PBYTE)precc2 + ulFieldOfs);
    if (ul1 < ul2)
        return -1;
    if (ul1 > ul2)
        return 1;
    return (0);
}

LONG inline CompareStrings(PNODERECORD precc1, PNODERECORD precc2, ULONG ulFieldOfs)
{
    char *psz1 = *(char**)((PBYTE)precc1 + ulFieldOfs);
    char *psz2 = *(char**)((PBYTE)precc2 + ulFieldOfs);
    if ((psz1) && (psz2))
       return (strcmp(psz1,
                      psz2));
    else if (psz1)
        // string 1 exists, but 2 doesn't:
        return (1);
    else if (psz2)
        // string 2 exists, but 1 doesn't:
        return (-1);

    return (0);
}

SHORT EXPENTRY fnCompareIndex(PNODERECORD p1, PNODERECORD p2, PVOID pStorage)
{
    return (CompareULongs(p1, p2, FIELDOFFSET(NODERECORD, ulIndex)));
}

SHORT EXPENTRY fnCompareStatus(PNODERECORD p1, PNODERECORD p2, PVOID pStorage)
{
    return (CompareStrings(p1, p2, FIELDOFFSET(NODERECORD, pszStatusDescr)));
}

SHORT EXPENTRY fnCompareType(PNODERECORD p1, PNODERECORD p2, PVOID pStorage)
{
    return (CompareStrings(p1, p2, FIELDOFFSET(NODERECORD, pszType)));
}

SHORT EXPENTRY fnCompareHandle(PNODERECORD p1, PNODERECORD p2, PVOID pStorage)
{
    return (CompareULongs(p1, p2, FIELDOFFSET(NODERECORD, ulHandle)));
}

SHORT EXPENTRY fnCompareParent(PNODERECORD p1, PNODERECORD p2, PVOID pStorage)
{
    return (CompareULongs(p1, p2, FIELDOFFSET(NODERECORD, ulParentHandle)));
}

SHORT EXPENTRY fnCompareShortName(PNODERECORD p1, PNODERECORD p2, PVOID pStorage)
{
    return (CompareStrings(p1, p2, FIELDOFFSET(NODERECORD, pszShortNameCopy)));
}

SHORT EXPENTRY fnCompareChildren(PNODERECORD p1, PNODERECORD p2, PVOID pStorage)
{
    return (-CompareULongs(p1, p2, FIELDOFFSET(NODERECORD, cChildren)));
}

SHORT EXPENTRY fnCompareDuplicates(PNODERECORD p1, PNODERECORD p2, PVOID pStorage)
{
    return (-CompareULongs(p1, p2, FIELDOFFSET(NODERECORD, cDuplicates)));
}

SHORT EXPENTRY fnCompareReferences(PNODERECORD p1, PNODERECORD p2, PVOID pStorage)
{
    return (-CompareStrings(p1, p2, FIELDOFFSET(NODERECORD, pszRefcsDescr)));
}

SHORT EXPENTRY fnCompareLongName(PNODERECORD p1, PNODERECORD p2, PVOID pStorage)
{
    return (CompareStrings(p1, p2, FIELDOFFSET(NODERECORD, pszLongName)));
}

/* ******************************************************************
 *                                                                  *
 *   Container window proc                                          *
 *                                                                  *
 ********************************************************************/

/*
 *@@ fnwpSubclassedCnr:
 *
 */

MRESULT EXPENTRY fnwpSubclassedCnr(HWND hwndCnr, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    switch (msg)
    {
        case WM_CLOSE:
            winhSaveWindowPos(G_hwndMain,
                              HINI_USER,
                              INIAPP,
                              INIKEY_MAINWINPOS);
            mrc = G_pfnwpCnrOrig(hwndCnr, msg, mp1, mp2);
        break;

        default:
            mrc = G_pfnwpCnrOrig(hwndCnr, msg, mp1, mp2);
    }

    return (mrc);
}

/*
 *@@ SetupCnr:
 *
 */

VOID SetupCnr(HWND hwndCnr)
{
    // set up data for Details view columns
    XFIELDINFO      xfi[13];
    int             i = 0,
                    iSplitAfter = 0;

    xfi[i].ulFieldOffset = FIELDOFFSET(NODERECORD, ulIndex);
    xfi[i].pszColumnTitle = "i";
    xfi[i].ulDataType = CFA_ULONG;
    xfi[i++].ulOrientation = CFA_RIGHT;

    xfi[i].ulFieldOffset = FIELDOFFSET(NODERECORD, pszStatusDescr);
    xfi[i].pszColumnTitle = "";
    xfi[i].ulDataType = CFA_STRING;
    xfi[i++].ulOrientation = CFA_LEFT;

    xfi[i].ulFieldOffset = FIELDOFFSET(NODERECORD, pszType);
    xfi[i].pszColumnTitle = "Type";
    xfi[i].ulDataType = CFA_STRING;
    xfi[i++].ulOrientation = CFA_LEFT;

    xfi[i].ulFieldOffset = FIELDOFFSET(NODERECORD, ulOfs);
    xfi[i].pszColumnTitle = "Node ofs";
    xfi[i].ulDataType = CFA_ULONG;
    xfi[i++].ulOrientation = CFA_RIGHT;

    xfi[i].ulFieldOffset = FIELDOFFSET(NODERECORD, pszHexHandle);
    xfi[i].pszColumnTitle = "Handle";
    xfi[i].ulDataType = CFA_STRING;
    xfi[i++].ulOrientation = CFA_RIGHT;

    xfi[i].ulFieldOffset = FIELDOFFSET(NODERECORD, pszHexParentHandle);
    xfi[i].pszColumnTitle = "Parent";
    xfi[i].ulDataType = CFA_STRING;
    xfi[i++].ulOrientation = CFA_RIGHT;

    iSplitAfter = i;
    xfi[i].ulFieldOffset = FIELDOFFSET(NODERECORD, pszShortNameCopy);
    xfi[i].pszColumnTitle = "Short name";
    xfi[i].ulDataType = CFA_STRING;
    xfi[i++].ulOrientation = CFA_LEFT;

    xfi[i].ulFieldOffset = FIELDOFFSET(NODERECORD, cChildren);
    xfi[i].pszColumnTitle = "Children";
    xfi[i].ulDataType = CFA_ULONG;
    xfi[i++].ulOrientation = CFA_RIGHT;

    xfi[i].ulFieldOffset = FIELDOFFSET(NODERECORD, cDuplicates);
    xfi[i].pszColumnTitle = "Dups.";
    xfi[i].ulDataType = CFA_ULONG;
    xfi[i++].ulOrientation = CFA_RIGHT;

    xfi[i].ulFieldOffset = FIELDOFFSET(NODERECORD, pszRefcsDescr);
    xfi[i].pszColumnTitle = "Refcs.";
    xfi[i].ulDataType = CFA_STRING;
    xfi[i++].ulOrientation = CFA_LEFT;

    xfi[i].ulFieldOffset = FIELDOFFSET(NODERECORD, pszLongName);
    xfi[i].pszColumnTitle = "Long name";
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
}

/* ******************************************************************
 *                                                                  *
 *   Frame window proc                                              *
 *                                                                  *
 ********************************************************************/

/*
 *@@ UpdateStatusBar:
 *
 */

VOID UpdateStatusBar(LONG lSeconds)
{
    CHAR sz[100],
         sz2[100],
         sz3[100];
    CHAR cThousands[10] = ".";
    ULONG cb = sizeof(cThousands);
    PrfQueryProfileData(HINI_USER,
                        "PM_National",
                        "sThousand",
                        cThousands,
                        &cb);
    sprintf(sz, "Done, %s bytes (%s handles).",
            strhThousandsULong(sz3, (ULONG)G_cbHandlesBuffer, cThousands[0]),
            strhThousandsULong(sz2, (ULONG)G_cHandlesParsed, cThousands[0]));
    if (lSeconds != -1)
        sprintf(sz + strlen(sz) - 1,
                ", %d seconds.",
                lSeconds / 1000);

    if (G_cDuplicatesFound)
        sprintf(sz + strlen(sz),
                " -- WARNING: %d duplicate handles exist.",
                G_cDuplicatesFound);

    WinSetWindowText(G_hwndStatusBar, sz);
}

/*
 *@@ UpdateMenuItems:
 *
 */

VOID UpdateMenuItems(USHORT usSortCmd)
{
    static usLastCmd = 0;
    HWND    hmenuMain = WinWindowFromID(G_hwndMain, FID_MENU);

    // view menu
    BOOL fEnable = (!G_tidInsertHandlesRunning && !G_tidCheckFilesRunning);
    WinEnableMenuItem(hmenuMain, IDM_ACTIONS,
                      fEnable);
    WinEnableMenuItem(hmenuMain, IDM_SELECT,
                      fEnable);
    WinEnableMenuItem(hmenuMain, IDMI_RESCAN,
                      fEnable);
    WinEnableMenuItem(hmenuMain, IDMI_WRITETOINI,
                      fEnable);

    // disable "duplicates" if we have none
    WinEnableMenuItem(hmenuMain, IDMI_SORT_DUPS,
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
 *@@ SetSort:
 *
 */

VOID SetSort(USHORT usCmd)
{
    PVOID pvSortFunc = NULL;
    switch (usCmd)
    {
        case IDMI_SORT_INDEX:
            pvSortFunc = (PVOID)fnCompareIndex;
        break;

        case IDMI_SORT_STATUS:
            pvSortFunc = (PVOID)fnCompareStatus;
        break;

        case IDMI_SORT_TYPE:
            pvSortFunc = (PVOID)fnCompareType;
        break;

        case IDMI_SORT_HANDLE:
            pvSortFunc = (PVOID)fnCompareHandle;
        break;

        case IDMI_SORT_PARENT:
            pvSortFunc = (PVOID)fnCompareParent;
        break;

        case IDMI_SORT_SHORTNAME:
            pvSortFunc = (PVOID)fnCompareShortName;
        break;

        case IDMI_SORT_CHILDREN:
            pvSortFunc = (PVOID)fnCompareChildren;
        break;

        case IDMI_SORT_DUPS:
            pvSortFunc = (PVOID)fnCompareDuplicates;
        break;

        case IDMI_SORT_REFCS:
            pvSortFunc = (PVOID)fnCompareReferences;
        break;

        case IDMI_SORT_LONGNAME:
            pvSortFunc = (PVOID)fnCompareLongName;
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
                  100);

    UpdateMenuItems(IDMI_SORT_INDEX);
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
    PSZ pszAbstracts = prfhQueryProfileData(HINI_USER,
                                            "PM_Abstract:FldrContent",
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

            CreateDeferredNuke(HINI_USER,
                               "PM_Abstract:Objects",
                               szAbstract);
            CreateDeferredNuke(HINI_USER,
                               "PM_Abstract:Icons",
                               szAbstract);
        }

        *pulNuked += cAbstracts;

        // nuke folder content entry
        CreateDeferredNuke(HINI_USER,
                           "PM_Abstract:FldrContent",
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
 *@@added V0.9.7 (2001-01-25) [umoeller]
 */

VOID NukeFolderPoses(PNODERECORD prec,
                     const char *pcszFolderPoses,
                     PULONG pulNuked)                // out: entries deleted
{
    // check folderpos entries in OS2.INI...
    // for some reason, these are decimal,
    // followed by "@" and some other key...
    // XWP adds keys to this too.
    const char *pcszFolderPosThis = pcszFolderPoses;
    CHAR szDecimalHandle[30];
    sprintf(szDecimalHandle,
            "%d@",
            // compose full handle (hiword)
            prec->ulHandle
                  | (G_ulHiwordFileSystem << 16L));

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
                CreateDeferredNuke(HINI_USER,
                                   "PM_Workplace:FolderPos",
                                   pcszFolderPosThis);
                (*pulNuked)++;
            }
        }

        pcszFolderPosThis += ulLengthThis + 1;
    }

    prec->ulStatus &= ~NODESTAT_FOLDERPOSNOTFOLDER;
    prec->fFolderPos = FALSE;
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

                    CreateDeferredNuke(HINI_USER,
                                       "PM_Abstract:FldrContent",
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
 *@@ RemoveHandles:
 *
 *@@added V0.9.7 (2001-01-21) [umoeller]
 *@@changed V0.9.9 (2001-04-07) [umoeller]: greatly reordered, fixed many crashes
 */

ULONG RemoveHandles(HWND hwndCnr,
                    PLINKLIST pllRecords)       // in: linked list of records to work on
{
    APIRET      arc = NO_ERROR;
    PLISTNODE   pNode;

    ULONG       cAbstractsNuked = 0,
                cFolderPosesNuked = 0;

    // load folderpos entries from OS2.INI
    PSZ pszFolderPoses = NULL;

    if (!(arc = prfhQueryKeysForApp(HINI_USER,
                                    "PM_Workplace:FolderPos",
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

            PBYTE pbItem = G_pHandlesBuffer + precDelete->ulOfs;
            // address of next node in buffer: depends on whether
            // it's a DRIV or a NODE
            ULONG   cbDelete = 0;
            PBYTE   pbNextItem = 0;
            BOOL    fIsDrive = FALSE;

            if (memcmp(pbItem, "DRIV", 4) == 0)
            {
                // it's a DRIVE node:
                PDRIV pDrivDelete = (PDRIV)pbItem;
                cbDelete = sizeof(DRIV) + strlen(pDrivDelete->szName);
                fIsDrive = TRUE;
            }
            else if (memcmp(pbItem, "NODE", 4) == 0)
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

                // nuke abstracts, folderpos
                NukeAbstracts(precDelete,
                              &cAbstractsNuked);
                NukeFolderPoses(precDelete,
                                pszFolderPoses,
                                &cFolderPosesNuked);
            }

            /*
             *   delete bytes in handles buffer
             *
             */

            // overwrite node with everything that comes after it
            ULONG   ulOfsOfNextItem = (pbNextItem - G_pHandlesBuffer);
            memmove(pbItem,
                    pbNextItem,
                    // byte count to move:
                    G_cbHandlesBuffer - ulOfsOfNextItem);

            // shrink handles buffer
            G_cbHandlesBuffer -= cbDelete;

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

            // for each record to be removed, update global counts
            G_cHandlesParsed--;
            if (precDelete->ulStatus & NODESTAT_ISDUPLICATE)
                // this was a duplicate:
                G_cDuplicatesFound--;

            // next record to be deleted
            pNode = pNode->pNext;
        } // end while (pNode)

        RebuildNodeHashTable();
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

            UpdateStatusBar(-1);

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
                sprintf(sz, "%d abstract object(s)",
                        cAbstractsNuked);
                xstrcpy(&str, sz, 0);
            }
            if (cFolderPosesNuked)
            {
                if (sz[0])
                    xstrcat(&str, " and ", 0);
                sprintf(sz, "%d folder position(s)",
                        cFolderPosesNuked);
                xstrcat(&str, sz, 0);
            }

            if (str.ulLength)
                xstrcat(&str, " have been scheduled for deletion. ", 0);

            if (CnrInfo.cRecords != cNewRecords)
                sprintf(sz,
                        "Hmmm. CNRINFO reports %d handles left, but we counted %d.",
                        CnrInfo.cRecords,
                        cNewRecords);
            else
                sprintf(sz, "%d handles are left.", cNewRecords);
            xstrcat(&str, sz, 0);

            if (str.ulLength)
                winhDebugBox(G_hwndMain,
                             "xfix",
                             str.psz);

            xstrClear(&str);
        }

        free(pszFolderPoses);

    } // if (pszFolderPoses)

    return (arc);
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

    PBYTE   pStart = G_pHandlesBuffer;
    ULONG   ulCurSize = 4;

    PBYTE   p = G_pHandlesBuffer + 4,
            pEnd = G_pHandlesBuffer + G_cbHandlesBuffer;

    ULONG   ulCurrentBlock = 1;

    while (p < pEnd)
    {
        while (p < pEnd)
        {
            ULONG ulPartSize;
            if (!memicmp(p, "DRIV", 4))
            {
                PDRIV pDriv = (PDRIV)p;
                ulPartSize = sizeof(DRIV) + strlen(pDriv->szName);
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
        PrfWriteProfileData(HINI_SYSTEM,
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

        if (   (PrfQueryProfileSize(HINI_SYSTEM,
                                    pszHandles,
                                    szBlockName,
                                    &ulBlockSize))
             && (ulBlockSize != 0)
           )
        {
            PrfWriteProfileData(HINI_SYSTEM,
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
 */

VOID WriteBack(VOID)
{
    if (WinMessageBox(HWND_DESKTOP,
                      G_hwndMain,
                      "This will write all changes back to the OS2.INI and OS2SYS.INI files. "
                      "You should be sure of at least the following:\n"
                      "-- your changes won't nuke your system;\n"
                      "-- the WPS is not currently running, or you have started xfix from "
                      "the XWorkplace \"Panic\" dialog;\n"
                      "-- you have a working WPS backup somewhere.\n"
                      "So, are you sure you want to do this?",
                      "xfix",
                      0,
                      MB_YESNO | MB_DEFBUTTON2 | MB_MOVEABLE)
            != MBID_YES)
    {
        // "no":
        WinMessageBox(HWND_DESKTOP,
                      G_hwndMain,
                      "No changes have been made to your system. To leave xfix, just close the main window.",
                      "xfix",
                      0,
                      MB_OK | MB_MOVEABLE);
    }
    else
    {
        // "yes":
        CHAR szActiveHandles[200];
        wphQueryActiveHandles(HINI_SYSTEM,
                              szActiveHandles,
                              sizeof(szActiveHandles));

        ULONG   ulLastChar = strlen(szActiveHandles) - 1;
        ULONG   ulLastBlock = 0;

        szActiveHandles[ulLastChar] = '0';
        WriteAllBlocks(szActiveHandles, &ulLastBlock);
        szActiveHandles[ulLastChar] = '1';
        WriteAllBlocks(szActiveHandles, &ulLastBlock);

        // from main(), return 1 now... this tells XWorkplace
        // that the handles have changed
        G_ulrc = 1;

        CHAR szText[300];
        sprintf(szText,
                "%d BLOCKs have been written to both handles sections. ",
                ulLastBlock);

        // update desktop
        if (G_paulAbstractsTarget)
        {
            // deferred changes:
            PrfWriteProfileData(HINI_USER,
                                "PM_Abstract:FldrContent",
                                G_szHandleDesktop,
                                G_paulAbstractsTarget,
                                G_cAbstractsDesktop * sizeof(ULONG));

            sprintf(szText + strlen(szText),
                    "%d objects have been moved to the desktop. ",
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
                    "%d keys in OS2.INI have been deleted. ", cNukes);

        strcat(szText, "To end xfix, just close the main window now.");
        WinMessageBox(HWND_DESKTOP,
                      G_hwndMain,
                      szText,
                      "xfix",
                      0,
                      MB_OK | MB_MOVEABLE);
    }
}

/*
 *@@ winhCreateStatusBar:
 *
 *@@added V0.9.5 (2000-08-13) [umoeller]
 */

HWND winhCreateStatusBar(HWND hwndFrame,
                         HWND hwndOwner,
                         USHORT usID,
                         PSZ pszFont,
                         LONG lColor)
{
    // create status bar
    HWND        hwndReturn = NULLHANDLE;
    PPRESPARAMS ppp = NULL;
    winhStorePresParam(&ppp, PP_FONTNAMESIZE, strlen(pszFont)+1, pszFont);
    lColor = WinQuerySysColor(HWND_DESKTOP, SYSCLR_DIALOGBACKGROUND, 0);
    winhStorePresParam(&ppp, PP_BACKGROUNDCOLOR, sizeof(lColor), &lColor);
    lColor = CLR_BLACK;
    winhStorePresParam(&ppp, PP_FOREGROUNDCOLOR, sizeof(lColor), &lColor);
    hwndReturn = WinCreateWindow(G_hwndMain,
                                  WC_STATIC,
                                  "Welcome.",
                                  SS_TEXT | DT_VCENTER | WS_VISIBLE,
                                  0, 0, 0, 0,
                                  hwndOwner,
                                  HWND_TOP,
                                  usID,
                                  NULL,
                                  ppp);
    free(ppp);
    return (hwndReturn);
}

/*
 *@@ fncbSelectInvalid:
 *
 */

ULONG EXPENTRY fncbSelectInvalid(HWND hwndCnr,
                                 PRECORDCORE precc,
                                 ULONG ulUser1,
                                 ULONG ulUser2)
{
    PNODERECORD pNodeRecord = (PNODERECORD)precc;
    if (pNodeRecord->recc.flRecordAttr & CRA_PICKED)
        // status != OK:
        pNodeRecord->recc.flRecordAttr |= CRA_SELECTED;
    else
        pNodeRecord->recc.flRecordAttr &= ~CRA_SELECTED;

    return (0);     // continue
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

/*
 *@@ FrameCommand:
 *
 *@@added V0.9.7 (2001-01-21) [umoeller]
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
            case IDMI_RESCAN:
                StartInsertHandles(hwndCnr);
            break;

            case IDMI_WRITETOINI:
                WriteBack();
            break;

            case IDMI_EXIT:
                WinPostMsg(hwndFrame,
                           WM_SYSCOMMAND,
                           (MPARAM)SC_CLOSE,
                           0);
            break;

            case IDMI_SORT_INDEX:
            case IDMI_SORT_STATUS:
            case IDMI_SORT_TYPE:
            case IDMI_SORT_HANDLE:
            case IDMI_SORT_PARENT:
            case IDMI_SORT_SHORTNAME:
            case IDMI_SORT_CHILDREN:
            case IDMI_SORT_DUPS:
            case IDMI_SORT_REFCS:
            case IDMI_SORT_LONGNAME:
                SetSort(usCmd);
            break;

            case IDMI_ACTIONS_FILES:
                thrCreate(&G_tiCheckFiles,
                          fntCheckFiles,
                          &G_tidCheckFilesRunning,
                          "CheckFiles",
                          THRF_WAIT | THRF_PMMSGQUEUE,
                          0);     // thread param
                WinStartTimer(WinQueryAnchorBlock(hwndFrame),
                              G_hwndMain,
                              TIMERID_THREADRUNNING,
                              500);
                UpdateMenuItems(0);
            break;

            case IDMI_SELECT_INVALID:
            {
                HPOINTER hptrOld = winhSetWaitPointer();
                cnrhForAllRecords(hwndCnr,
                                  NULL, // preccParent,
                                  fncbSelectInvalid,        // callback
                                  0, 0);
                cnrhInvalidateAll(hwndCnr);
                WinSetPointer(HWND_DESKTOP, hptrOld);
            break; }

            case IDMI_DELETE:
            {
                ULONG       cRecs = 0;
                PLINKLIST   pll = GetSelectedRecords(hwndCnr,
                                                     precSource,
                                                     &cRecs);
                if (pll && cRecs)
                {
                    CHAR szText[500];
                    sprintf(szText, "You have selected %d handle(s) for removal. "
                                    "Are you sure you want to do this?", cRecs);

                    if (WinMessageBox(HWND_DESKTOP,
                                      G_hwndMain,
                                      szText,
                                      "xfix",
                                      0,
                                      MB_YESNO | MB_DEFBUTTON2 | MB_MOVEABLE)
                            == MBID_YES)
                    {
                        ULONG ulrc = RemoveHandles(hwndCnr, pll);

                        if (ulrc != 0)
                        {
                            sprintf(szText, "Error %d occured.", ulrc);
                            WinMessageBox(HWND_DESKTOP,
                                          G_hwndMain,
                                          szText,
                                          "xfix",
                                          0,
                                          MB_OK | MB_MOVEABLE);
                        }
                    }

                    lstFree(&pll);
                }

            break; }

            case IDMI_NUKEFOLDERPOS:
            {
                ULONG       cRecs = 0;
                PLINKLIST   pll = GetSelectedRecords(hwndCnr,
                                                     precSource,
                                                     &cRecs);
                if (pll && cRecs)
                {
                    APIRET arc;
                    PSZ pszFolderPoses = NULL;

                    if (!(arc = prfhQueryKeysForApp(HINI_USER,
                                                    "PM_Workplace:FolderPos",
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
                        sprintf(sz, "%d folder position(s) have been scheduled for deletion.",
                                cTotalNuked);
                        winhDebugBox(G_hwndMain,
                                     "xfix",
                                     sz);

                        free(pszFolderPoses);
                    }

                    lstFree(&pll);
                }
            break; }

            /*
             * IDMI_MOVEABSTRACTS:
             *      move abstracts to desktop
             */

            case IDMI_MOVEABSTRACTS:
            {
                ULONG       cAbstractsMoved = 0;

                // get handle of desktop
                if (G_precDesktop == NULL)
                {
                    // first call:
                    ULONG       hobjDesktop = 0;
                    ULONG       cb = sizeof(hobjDesktop);
                    if (    (PrfQueryProfileData(HINI_USER,
                                                 "PM_Workplace:Location",
                                                 "<WP_DESKTOP>",
                                                 &hobjDesktop,
                                                 &cb))
                         && (hobjDesktop)
                       )
                    {
                        // OK, found desktop:
                        // get its NODERECORD
                        G_precDesktop = G_RecordHashTable[hobjDesktop & 0xFFFF];

                        CHAR sz2[100];
                        sprintf(sz2, "Desktop handle is %lX", hobjDesktop & 0xFFFF);
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
                        sprintf(szText, "You have selected %d handles whose abstract objects "
                                        "should be moved to the deskop.\n"
                                        "Are you sure you want to do this?",
                                        cRecs);
                        if (WinMessageBox(HWND_DESKTOP,
                                          G_hwndMain,
                                          szText,
                                          "xfix",
                                          0,
                                          MB_YESNO | MB_DEFBUTTON2 | MB_MOVEABLE)
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
                    sprintf(sz, "%d abstracts have been moved to the desktop. They will "
                            "appear after the next WPS startup.",
                            cAbstractsMoved);
                    winhDebugBox(G_hwndMain, "xfix", sz);
                }
                else
                    winhDebugBox(G_hwndMain, "xfix", "An error occured moving the abstracts.");
            break; }

            case IDMI_HELP_GENERAL:
            case IDMI_HELP_USINGHELP:
                if (G_hwndHelp)
                {
                    winhDisplayHelpPanel(G_hwndHelp,
                                        (usCmd == IDMI_HELP_GENERAL)
                                            ? ID_XSH_XFIX_INTRO
                                            : 0);       // "using help"
                }
            break;

            case IDMI_HELP_PRODINFO:
            {
                CHAR szInfo[500];

                sprintf(szInfo,
                        "xfix V" BLDLEVEL_VERSION " built " __DATE__ "\n"
                        "(C) 2000-2001 Ulrich M”ller\n\n"
                        "XWorkplace File Handles Fixer.");

                WinMessageBox(HWND_DESKTOP,
                              G_hwndMain,
                              szInfo,
                              "xfix",
                              0,
                              MB_OK | MB_MOVEABLE);
            break; }
        }
    }
    CATCH(excpt1) {} END_CATCH();
}

/*
 *@@ winh_fnwpFrameWithStatusBar:
 *      subclassed frame window proc.
 *
 *@@added V0.9.5 (2000-08-13) [umoeller]
 */

MRESULT EXPENTRY winh_fnwpFrameWithStatusBar(HWND hwndFrame, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    static PRECORDCORE preccSource = NULL;

    switch (msg)
    {
        case WM_QUERYFRAMECTLCOUNT:
        {
            // query the standard frame controls count
            ULONG ulrc = (ULONG)(G_fnwpFrameOrig(hwndFrame, msg, mp1, mp2));

            // if we have a status bar, increment the count
            ulrc++;

            mrc = (MPARAM)ulrc;
        break; }

        case WM_FORMATFRAME:
        {
            //  query the number of standard frame controls
            ULONG ulCount = (ULONG)(G_fnwpFrameOrig(hwndFrame, msg, mp1, mp2));

            // we have a status bar:
            // format the frame
            ULONG       ul;
            PSWP        swpArr = (PSWP)mp1;

            for (ul = 0; ul < ulCount; ul++)
            {
                if (WinQueryWindowUShort( swpArr[ul].hwnd, QWS_ID ) == 0x8008 )
                                                                 // FID_CLIENT
                {
                    POINTL      ptlBorderSizes;
                    ULONG       ulStatusBarHeight = 20;
                    WinSendMsg(hwndFrame,
                               WM_QUERYBORDERSIZE,
                               (MPARAM)&ptlBorderSizes,
                               0);

                    // first initialize the _new_ SWP for the status bar.
                    // Since the SWP array for the std frame controls is
                    // zero-based, and the standard frame controls occupy
                    // indices 0 thru ulCount-1 (where ulCount is the total
                    // count), we use ulCount for our static text control.
                    swpArr[ulCount].fl = SWP_MOVE | SWP_SIZE | SWP_NOADJUST | SWP_ZORDER;
                    swpArr[ulCount].x  = ptlBorderSizes.x;
                    swpArr[ulCount].y  = ptlBorderSizes.y;
                    swpArr[ulCount].cx = swpArr[ul].cx;  // same as cnr's width
                    swpArr[ulCount].cy = ulStatusBarHeight;
                    swpArr[ulCount].hwndInsertBehind = HWND_BOTTOM; // HWND_TOP;
                    swpArr[ulCount].hwnd = G_hwndStatusBar;

                    // adjust the origin and height of the container to
                    // accomodate our static text control
                    swpArr[ul].y  += swpArr[ulCount].cy;
                    swpArr[ul].cy -= swpArr[ulCount].cy;
                }
            }

            // increment the number of frame controls
            // to include our status bar
            mrc = (MRESULT)(ulCount + 1);
        break; }

        case WM_CALCFRAMERECT:
        {
            mrc = G_fnwpFrameOrig(hwndFrame, msg, mp1, mp2);

            // we have a status bar: calculate its rectangle
            // CalcFrameRect(mp1, mp2);
        break; }

        case WM_TIMER:
        {
            ULONG ulID = (ULONG)mp1;
            if (G_tidInsertHandlesRunning)
            {
                CHAR sz[100];
                if (G_fResolvingRefs)
                    sprintf(sz, "Resolving cross-references, %03d%% done...",
                            G_ulPercentDone);
                else
                    sprintf(sz, "Parsing handles section, %03d%% done...",
                            G_ulPercentDone);
                WinSetWindowText(G_hwndStatusBar,
                                 sz);
            }
            else if (G_tidCheckFilesRunning)
            {
                CHAR sz[100];
                sprintf(sz, "Checking files, %03d%% done...",
                        G_ulPercentDone);
                WinSetWindowText(G_hwndStatusBar,
                                 sz);
            }
        break; }

        /*
         * WM_USER:
         *      mp1 has no. of handles or (-1),
         *      mp2 has milliseconds passed.
         */

        case WM_USER:
        {
            // thread is done:
            WinStopTimer(WinQueryAnchorBlock(G_hwndMain),
                         G_hwndMain,
                         TIMERID_THREADRUNNING);

            if ((LONG)mp1 == -1)
            {
                // resolving cross-references:
                WinSetWindowText(G_hwndStatusBar, "Resolving cross-references...");
            }
            else
            {
                // done:
                if (mp1)
                {
                    // done, success:
                    G_cHandlesParsed = (ULONG)mp1;
                    UpdateStatusBar((ULONG)mp2);        // seconds
                }
                else
                    // done, error:
                    WinSetWindowText(G_hwndStatusBar,
                                     "An error occured parsing the handles section.");

                thrWait(&G_tiInsertHandles);

                UpdateMenuItems(0);
            }
        break; }

        case WM_USER + 1:
            WinSetWindowText(G_hwndStatusBar,
                             "Done checking files.");
            cnrhInvalidateAll(WinWindowFromID(G_hwndMain, FID_CLIENT));
            thrWait(&G_tiCheckFiles);

            UpdateMenuItems(0);
        break;

        case WM_CONTROL:
        {
            USHORT usID = SHORT1FROMMP(mp1),
                   usNotifyCode = SHORT2FROMMP(mp1);
            if ((usID == FID_CLIENT) && (usNotifyCode == CN_CONTEXTMENU) )
            {
                HWND hwndCnr = WinWindowFromID(G_hwndMain, FID_CLIENT);
                preccSource = (PRECORDCORE)mp2;
                if (preccSource)
                {
                    ULONG       cRecs = 0;
                    PLINKLIST   pll = GetSelectedRecords(hwndCnr,
                                                         preccSource,
                                                         &cRecs);
                    if (pll)
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
                                fGotFolderPoses = FALSE;

                        PLISTNODE pNode = lstQueryFirstNode(pll);
                        while (pNode)
                        {
                            PNODERECORD prec = (PNODERECORD)pNode->pItemData;

                            if (prec->cAbstracts)
                                fGotAbstracts = TRUE;
                            if (prec->fFolderPos)
                                fGotFolderPoses = TRUE;

                            pNode = pNode->pNext;
                        }

                        WinEnableMenuItem(hwndMenu,
                                          IDMI_NUKEFOLDERPOS,
                                          fGotFolderPoses);
                        WinEnableMenuItem(hwndMenu,
                                          IDMI_MOVEABSTRACTS,
                                          fGotAbstracts);

                        cnrhShowContextMenu(hwndCnr,
                                            preccSource,
                                            hwndMenu,
                                            hwndFrame);

                        lstFree(&pll);
                    }
                }
            }
        break; }

        case WM_MENUEND:
            if (    (    ((HWND)mp2 == G_hwndContextMenuSingle)
                      || ((HWND)mp2 == G_hwndContextMenuMulti)
                    )
                 && (preccSource)
               )
                cnrhSetSourceEmphasis(WinWindowFromID(G_hwndMain, FID_CLIENT),
                                      preccSource,
                                      FALSE);
        break;

        case WM_COMMAND:
        {
            USHORT  usCmd = SHORT1FROMMP(mp1);
            FrameCommand(hwndFrame, usCmd, preccSource);
        break; }

        default:
            mrc = G_fnwpFrameOrig(hwndFrame, msg, mp1, mp2);
    }

    return (mrc);
}

/* ******************************************************************
 *
 *   main
 *
 ********************************************************************/

int main(int argc, char* argv[])
{
    HAB         hab;
    HMQ         hmq;
    QMSG        qmsg;

    DosError(FERR_DISABLEHARDERR | FERR_ENABLEEXCEPTION);

    if (!(hab = WinInitialize(0)))
        return FALSE;

    if (!(hmq = WinCreateMsgQueue(hab, 0)))
        return FALSE;

    // get the index of WPFileSystem from the base classes list...
    // we need this to determine the hiword for file-system handles
    // properly. Normally, this should be 3.
    ULONG cbBaseClasses = 0;
    PSZ pszBaseClasses = prfhQueryProfileData(HINI_USER,
                                              "PM_Workplace:BaseClass",
                                              "ClassList",
                                              &cbBaseClasses);
    if (pszBaseClasses)
    {
        // parse that buffer... these has the base class names,
        // separated by 0. List is terminated by two zeroes.
        PSZ     pszClassThis = pszBaseClasses;
        ULONG   ulHiwordThis = 1;
        while (    (*pszClassThis)
                && (pszClassThis - pszBaseClasses < cbBaseClasses)
              )
        {
            if (strcmp(pszClassThis, "WPFileSystem") == 0)
            {
                G_ulHiwordFileSystem = ulHiwordThis;
                break;
            }
            ulHiwordThis++;
            pszClassThis += strlen(pszClassThis) + 1;
        }

        free(pszBaseClasses);
    }

    lstInit(&G_llDeferredNukes, TRUE);

    if (!G_ulHiwordFileSystem)
    {
        winhDebugBox(NULLHANDLE,
                     "xfix",
                     "xfix was unable to determine the storage class index for "
                     "WPFileSystem objects. Terminating.");
    }
    else
    {
        // create frame and container
        HWND hwndCnr = NULLHANDLE;
        G_hwndMain = winhCreateStdWindow(HWND_DESKTOP,
                                         0,
                                         FCF_TITLEBAR
                                            | FCF_SYSMENU
                                            | FCF_MINMAX
                                            | FCF_SIZEBORDER
                                            | FCF_ICON
                                            | FCF_MENU
                                            | FCF_TASKLIST,
                                         0,
                                         "XWorkplace Handles Fixer",
                                         1,     // icon resource
                                         WC_CONTAINER,
                                         CCS_MINIICONS | CCS_READONLY | CCS_EXTENDSEL
                                            | WS_VISIBLE,
                                         0,
                                         NULL,
                                         &hwndCnr);

        if ((G_hwndMain) && (hwndCnr))
        {
            // subclass cnr (it's our client)
            G_pfnwpCnrOrig = WinSubclassWindow(hwndCnr, fnwpSubclassedCnr);

            // create status bar as child of the frame
            G_hwndStatusBar = winhCreateStatusBar(G_hwndMain,
                                                  hwndCnr,
                                                  0,
                                                  "9.WarpSans",
                                                  CLR_BLACK);
            // subclass frame for supporting status bar and msgs
            G_fnwpFrameOrig = WinSubclassWindow(G_hwndMain,
                                                winh_fnwpFrameWithStatusBar);

            SetupCnr(hwndCnr);

            // load icons

            G_hwndContextMenuSingle = WinLoadMenu(hwndCnr,
                                                  NULLHANDLE,
                                                  IDM_RECORDSELSINGLE);
            G_hwndContextMenuMulti  = WinLoadMenu(hwndCnr,
                                                  NULLHANDLE,
                                                  IDM_RECORDSELMULTI);

            // load NLS help library (..\help\xfldr001.hlp)
            PPIB     ppib;
            PTIB     ptib;
            CHAR     szHelpName[CCHMAXPATH];
            DosGetInfoBlocks(&ptib, &ppib);
            DosQueryModuleName(ppib->pib_hmte, sizeof(szHelpName), szHelpName);
                    // now we have: "J:\Tools\WPS\XWorkplace\bin\xfix.exe"
            PSZ pszLastBackslash = strrchr(szHelpName, '\\');
            if (pszLastBackslash)
            {
                *pszLastBackslash = 0;
                // again to get rid of "bin"
                pszLastBackslash = strrchr(szHelpName, '\\');
                if (pszLastBackslash)
                {
                    *pszLastBackslash = 0;
                    // now we have: "J:\Tools\WPS\XWorkplace"
                    CHAR szLanguage[10];
                    PrfQueryProfileString(HINI_USER,
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
                                      HINI_USER,
                                      INIAPP,
                                      INIKEY_MAINWINPOS,
                                      SWP_SHOW | SWP_ACTIVATE | SWP_MOVE | SWP_SIZE))
                WinSetWindowPos(G_hwndMain,
                                HWND_TOP,
                                10, 10, 500, 500,
                                SWP_SHOW | SWP_ACTIVATE | SWP_MOVE | SWP_SIZE);

            // load handles from OS2.INI
            CHAR szActiveHandles[200];
            wphQueryActiveHandles(HINI_SYSTEM,
                                  szActiveHandles,
                                  sizeof(szActiveHandles));

            if (!wphReadAllBlocks(HINI_SYSTEM,
                                  szActiveHandles,
                                  &G_pHandlesBuffer,
                                  &G_cbHandlesBuffer))
                G_pHandlesBuffer = NULL;

            RebuildNodeHashTable();

            StartInsertHandles(hwndCnr);

            // display introductory help with warnings
            WinPostMsg(G_hwndMain,
                       WM_COMMAND,
                       (MPARAM)IDMI_HELP_GENERAL,
                       0);

            // standard PM message loop
            while (WinGetMsg(hab, &qmsg, NULLHANDLE, 0, 0))
            {
                WinDispatchMsg(hab, &qmsg);
            }

            if (G_tidInsertHandlesRunning)
                thrFree(&G_tiInsertHandles);
            if (G_tidCheckFilesRunning)
                thrFree(&G_tiCheckFiles);
        }
    }

    // clean up on the way out
    WinDestroyMsgQueue(hmq);
    WinTerminate(hab);

    return (G_ulrc);        // 0 if nothing changed
                            // 1 if OS2SYS.INI was changed
}
