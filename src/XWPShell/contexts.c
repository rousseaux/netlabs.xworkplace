
/*
 *@@sourcefile contexts.c:
 *      implementation for security contexts, including subject
 *      handles management, ACL setup, and ring-0 interfaces.
 *
 *@@added V0.9.5 [umoeller]
 *@@header "security\xwpshell.h"
 */

/*
 *      Copyright (C) 2000-2002 Ulrich M”ller.
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, in version 2 as it comes in the COPYING
 *      file of the XWorkplace main distribution.
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 */

#define INCL_DOSSEMAPHORES
#define INCL_DOSDEVICES
#define INCL_DOSERRORS
#include <os2.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <io.h>

#include "setup.h"

#include "helpers\dosh.h"
#include "helpers\linklist.h"
#include "helpers\nls.h"                // National Language Support helpers
#include "helpers\procstat.h"
#include "helpers\standards.h"
#include "helpers\stringh.h"
#include "helpers\threads.h"
#include "helpers\tree.h"               // red-black binary trees

#include "helpers\xwpsecty.h"
#include "security\xwpshell.h"
#include "security\ring0api.h"

/* ******************************************************************
 *
 *   Private Definitions
 *
 ********************************************************************/

/*
 *@@ SUBJECTTREENODE:
 *
 */

typedef struct _SUBJECTTREENODE
{
    TREE            Tree;             // tree item (required for tree* to work)
    XWPSUBJECTINFO  SubjectInfo;
} SUBJECTTREENODE, *PSUBJECTTREENODE;

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

HFILE       G_hfSec32DD = NULLHANDLE;

// subject infos
TREE        *G_treeSubjects;
    // global linked list of currently active subjects
LONG        G_cSubjects = 0;
    // subjects count
ULONG       G_ulNextHSubject = 1;
    // next HSUBJECT to use (raised with each subject creation);
    // we start with 1 (0 is the special one for root)

// ACL database
TREE        *G_treeACLDB;
    // balanced binary tree of currently loaded ACLs;
    // contains ACLDBTREENODENODE's
LONG        G_cACLDBEntries = 0;
HMTX        G_hmtxACLs = NULLHANDLE;
    // mutex semaphore protecting global data

PRING0BUF   G_pRing0Buf = NULL;         //  system ACL table

THREADINFO  G_tiLogger = {0};

extern PXFILE G_LogFile;            // xwpshell.c

/* ******************************************************************
 *
 *   Private Helpers
 *
 ********************************************************************/

/*
 *@@ LockACLs:
 *      locks the global security data by requesting
 *      its mutex.
 *
 *      Always call UnlockACLs() when you're done.
 */

BOOL LockACLs(VOID)
{
    return !DosRequestMutexSem(G_hmtxACLs, SEM_INDEFINITE_WAIT);
}

/*
 *@@ UnlockACLs:
 *      unlocks the global security data.
 */

VOID UnlockACLs(VOID)
{
    DosReleaseMutexSem(G_hmtxACLs);
}

/*
 *@@ SecIOCtl:
 *
 *@@added V1.0.1 (2003-01-10) [umoeller]
 */

APIRET SecIOCtl(ULONG ulFuncCode,
                PVOID pvData,
                ULONG cbData)
{
    return DosDevIOCtl(G_hfSec32DD,
                       IOCTL_XWPSEC,
                       ulFuncCode,
                       NULL,        // params, we have none
                       0,
                       NULL,
                       pvData,
                       cbData,
                       &cbData);
}

/*
 *@@ FindSubjectInfoFromHandle:
 *      searches the list of subject infos for the
 *      specified subject handle.
 *
 *      Since the subjects tree is sorted according
 *      to handles, this is extremely fast.
 *
 *      Returns the PSUBJECTTREENODE from the list
 *      or NULL if not found.
 *
 *      Preconditions:
 *
 *      --  Call LockACLs() first.
 */

PSUBJECTTREENODE FindSubjectInfoFromHandle(HXSUBJECT hSubject)
{
    PSUBJECTTREENODE pTreeItem
        = (PSUBJECTTREENODE)treeFind(G_treeSubjects,
                                     hSubject,
                                     treeCompareKeys);

    return (pTreeItem);
}

/*
 *@@ FindSubjectInfoFromID:
 *      searches the list of subject infos for the
 *      specified (user or group) ID.
 *
 *      Returns the XWPSUBJECTINFO from the list
 *      or NULL if not found.
 *
 *      In the worst case, this needs to traverse
 *      the entire list of subject infos, so this
 *      is not terribly fast.
 *
 *      Preconditions:
 *
 *      --  Call LockACLs() first.
 */

PSUBJECTTREENODE FindSubjectInfoFromID(BYTE bType,       // in: one of SUBJ_USER, SUBJ_GROUP, or SUBJ_PROCESS
                                       XWPSECID id)      // in: ID to search for
{
    PSUBJECTTREENODE pNode = (PSUBJECTTREENODE)treeFirst(G_treeSubjects);
    while (pNode)
    {
        PXWPSUBJECTINFO psi = &pNode->SubjectInfo;

        if ((psi->id == id) && (psi->bType == bType))
            return pNode;

        pNode = (PSUBJECTTREENODE)treeNext((TREE*)pNode);
    }

    return NULL;
}

/*
 *@@ CalcACLNodeSize:
 *      returns the size of the RESOURCEACL entry to be built
 *      from the given acl tree node, including the ACCESS
 *      structs needed for it.
 *
 *@@added V1.0.1 (2003-01-05) [umoeller]
 */

ULONG CalcACLNodeSize(PACLDBTREENODE pNode,
                      PRESOURCEACL pBuf)       // out: buf offsets
{
    ULONG   cPerms = lstCountItems(&pNode->llPerms);

    if (pBuf)
    {
        pBuf->cAccesses = cPerms;
        pBuf->cbName = pNode->usResNameLen + 1;
    }

    return   sizeof(RESOURCEACL)        // includes one byte for szName already
           + pNode->usResNameLen
           + cPerms * sizeof(ACCESS);
}

/* ******************************************************************
 *
 *   Security logging thread
 *
 ********************************************************************/

/*
 *@@ LogEntry:
 *
 *@@added V1.0.1 (2003-01-10) [umoeller]
 */

APIRET LogEntry(PEVENTLOGENTRY pThis,
                const char* pcszFormat,
                ...)
{
    APIRET arc = NO_ERROR;

    if (G_LogFile)
    {
        DATETIME dt;
        CHAR szTemp[2000];
        ULONG   ulLength;

        ulLength = sprintf(szTemp,
                           "%04lX|%04lX %04d-%02d-%02d %02d:%02d:%02d ",
                           pThis->ctxt.pid, pThis->ctxt.tid,
                           pThis->stamp.year, pThis->stamp.month, pThis->stamp.day,
                           pThis->stamp.hours, pThis->stamp.minutes, pThis->stamp.seconds);
        if (!(arc = doshWrite(G_LogFile,
                              ulLength,
                              szTemp)))
        {
            va_list arg_ptr;
            va_start(arg_ptr, pcszFormat);
            ulLength = vsprintf(szTemp, pcszFormat, arg_ptr);
            va_end(arg_ptr);

            szTemp[ulLength++] = '\n';

            arc = doshWrite(G_LogFile,
                            ulLength,
                            szTemp);
        }
    }

    return arc;
}

/*
 *@@ fntLogger:
 *
 *@@added V1.0.1 (2003-01-10) [umoeller]
 */

void _Optlink fntLogger(PTHREADINFO ptiMyself)
{
    APIRET  arc;

    PLOGBUF pLogBuf;

    if (!(arc = DosAllocMem((PVOID*)&pLogBuf,
                            LOGBUFSIZE,
                            PAG_COMMIT | PAG_READ | PAG_WRITE | OBJ_TILE)))
    {
        while (!ptiMyself->fExit)
        {
            if (!(arc = SecIOCtl(XWPSECIO_GETLOGBUF,
                                 pLogBuf,
                                 LOGBUFSIZE)))
            {
                ULONG ul;
                PEVENTLOGENTRY pThis = (PEVENTLOGENTRY)((PBYTE)pLogBuf + sizeof(LOGBUF));

                for (ul = 0;
                     ul < pLogBuf->cLogEntries;
                     ++ul)
                {
                    CHAR szTemp[CCHMAXPATH];
                    switch (pThis->ulEventCode)
                    {
                        case EVENT_OPENPRE:
                        case EVENT_OPENPOST:
                        {
                            PEVENTBUF_OPEN pOpen =
                                    (PEVENTBUF_OPEN)((PBYTE)pThis + sizeof(EVENTLOGENTRY));
                            memcpy(szTemp,
                                   pOpen->szPath,
                                   pOpen->ulPathLen + 1);

                            if (pThis->ulEventCode == EVENT_OPENPRE)
                                LogEntry(pThis,
                                         "%04d: OPENPRE  \"%s\" -> %d",
                                         ul,
                                         szTemp,
                                         pOpen->rc);
                            else
                                LogEntry(pThis,
                                         "%04d: OPENPOST \"%s\" -> %d",
                                         ul,
                                         szTemp,
                                         pOpen->rc);
                        }
                        break;
                    }

                    pThis = (PEVENTLOGENTRY)((PBYTE)pThis + pThis->cbStruct);
                }
            }
            else
            {
                doshWriteLogEntry(G_LogFile,
                                  "Error: XWPSECIO_GETLOGBUF returned %d",
                                  arc);
                break;
            }
        }

        doshWriteLogEntry(G_LogFile,
                          "Logger thread exiting");

        DosFreeMem(pLogBuf);
    }
}

/* ******************************************************************
 *
 *   Initialization, status
 *
 ********************************************************************/

/*
 *@@ InitRing0:
 *
 *@@added V1.0.1 (2003-01-10) [umoeller]
 */

APIRET InitRing0(VOID)
{
    APIRET  arc = NO_ERROR;
    ULONG   ulActionTaken = 0;

    // return no error if driver is not present,
    // only if it is and _then_ something goes wrong;
    // people are allowed to run XWPShell without the
    // driver
    if (!DosOpen("XWPSEC$",
                 &G_hfSec32DD,
                 &ulActionTaken,
                 0,
                 FILE_NORMAL,
                 OPEN_ACTION_OPEN_IF_EXISTS,
                 OPEN_SHARE_DENYNONE | OPEN_ACCESS_READWRITE | OPEN_FLAGS_FAIL_ON_ERROR,
                 NULL))
    {
        // driver opened:

        // build array of process IDs to send down with SECIO_REGISTER
        PQTOPLEVEL32 pInfo;
        if (pInfo = prc32GetInfo2(QS32_PROCESS | QS32_THREAD, &arc))
        {
            // count processes; i can't see a field for this
            PQPROCESS32 pProcThis = pInfo->pProcessData;
            ULONG cProcs = 0;
            while (pProcThis && pProcThis->ulRecType == 1)
            {
                PQTHREAD32  t = pProcThis->pThreads;
                ++cProcs;
                // for next process, skip the threads info;
                // the next process block comes after the
                // threads
                t += pProcThis->usThreadCount;
                pProcThis = (PQPROCESS32)t;
            }

            if (cProcs)
            {
                PPROCESSLIST    pList;
                ULONG           cbStruct =   sizeof(PROCESSLIST)
                                           + (cProcs - 1) * sizeof(ULONG);

                doshWriteLogEntry(G_LogFile,
                                  __FUNCTION__ ": got %d processes, list has %d bytes",
                                  cProcs,
                                  cbStruct);

                if (!(pList = malloc(cbStruct)))
                    arc = ERROR_NOT_ENOUGH_MEMORY;
                else
                {
                    PUSHORT     ppidThis = &pList->apidTrusted[0];

                    pList->cbStruct = cbStruct;
                    pList->cTrusted = cProcs;

                    // start over
                    pProcThis = pInfo->pProcessData;
                    while (pProcThis && pProcThis->ulRecType == 1)
                    {
                        PQTHREAD32  t = pProcThis->pThreads;

                        *ppidThis++ = pProcThis->usPID;

                        // for next process, skip the threads info;
                        // the next process block comes after the
                        // threads
                        t += pProcThis->usThreadCount;
                        pProcThis = (PQPROCESS32)t;
                    }

                    // alright, REGISTER these and ENABLE LOCAL SECURITY
                    // by calling DosDevIOCtl
                    if (!(arc = SecIOCtl(XWPSECIO_REGISTER,
                                         pList,
                                         pList->cbStruct)))
                    {
                        // this worked:
                        // start the logger thread
                        thrCreate(&G_tiLogger,
                                  fntLogger,
                                  NULL,
                                  "Logger",
                                  THRF_WAIT,
                                  0);
                    }

                    doshWriteLogEntry(G_LogFile,
                                      __FUNCTION__ ": XWPSECIO_REGISTER returned %d",
                                      arc);
                }
            }

            prc32FreeInfo(pInfo);
        }

        if (arc)
        {
            DosClose(G_hfSec32DD);
            G_hfSec32DD = NULLHANDLE;
        }
    }

    return arc;
}

/*
 *@@ scxtInit:
 *      initializes XWPSecurity.
 */

APIRET scxtInit(VOID)
{
    APIRET arc;

    if (!(arc = DosCreateMutexSem(NULL,       // unnamed
                                  &G_hmtxACLs,
                                  0,          // unshared
                                  FALSE)))    // unowned
    {
        BOOL    fLocked;
        ULONG   ulLineWithError;

        treeInit(&G_treeSubjects, &G_cSubjects);
        treeInit(&G_treeACLDB, &G_cACLDBEntries);

        if (!(fLocked = LockACLs()))
            arc = XWPSEC_CANNOT_GET_MUTEX;
        else
        {
            if (!(arc = saclLoadDatabase(&ulLineWithError)))
                // try to open the driver
                arc = InitRing0();

            if (fLocked)
                UnlockACLs();
        }
    }

    return arc;
}

/*
 *@@ scxtExit:
 *      cleans up when XWPShell exits.
 *
 *@@added V1.0.1 (2003-01-10) [umoeller]
 */

VOID scxtExit(VOID)
{
    if (G_hfSec32DD)
    {
        SecIOCtl(XWPSECIO_DEREGISTER,
                 NULL,
                 0);
        DosClose(G_hfSec32DD);
        G_hfSec32DD = NULLHANDLE;
    }
}

/*
 *@@ scxtCreateACLEntry:
 *      helper API to be called by saclLoadDatabase as
 *      implemented by the ACL database implementation
 *      to insert an ACL entry into the global database.
 *
 *      Returns:
 *
 *      --  NO_ERROR
 *
 *      --  XWPSEC_DB_ACL_DUPRES: an entry for the
 *          given resource already existed.
 *
 *@@added V1.0.1 (2003-01-10) [umoeller]
 */

APIRET scxtCreateACLEntry(PACLDBTREENODE pNewEntry)
{
    pNewEntry->Tree.ulKey = (ULONG)pNewEntry->szResName;
    if (treeInsert(&G_treeACLDB,
                   &G_cACLDBEntries,
                   (TREE*)pNewEntry,
                   treeCompareStrings))
        return XWPSEC_DB_ACL_DUPRES;

    return NO_ERROR;
}

/*
 *@@ scxtQueryStatus:
 *
 *@@added V1.0.1 (2003-01-10) [umoeller]
 */

APIRET scxtQueryStatus(PXWPSECSTATUS pStatus)
{
    APIRET  arc = NO_ERROR;

    ZERO(pStatus);

    if (G_hfSec32DD)
    {
        RING0STATUS r0s;

        pStatus->fLocalSecurity = TRUE;

        if (!(arc = SecIOCtl(XWPSECIO_QUERYSTATUS,
                             &r0s,
                             sizeof(r0s))))
        {
            pStatus->cbAllocated = r0s.cbAllocated;
            pStatus->cAllocations = r0s.cAllocations;
            pStatus->cFrees = r0s.cFrees;
            pStatus->cLogBufs = r0s.cLogBufs;
            pStatus->cMaxLogBufs = r0s.cMaxLogBufs;
            pStatus->cLogged = r0s.cLogged;
            pStatus->cGranted = r0s.cGranted;
            pStatus->cDenied = r0s.cDenied;
        }
    }

    return arc;
}

/* ******************************************************************
 *
 *   Subject entry points
 *
 ********************************************************************/

/*
 *@@ scxtCreateSubject:
 *      creates a new subject handle, either for
 *      a user or a group, or raises the usage count
 *      for a subject if a handle for the given ID
 *      has already been created previously.
 *
 *      This checks for whether a subject handle exists
 *      already for the specified ID. If so, NO_ERROR is
 *      returned, and the existing subject's usage count
 *      is incremented. pSubjectInfo then receives the
 *      existing HXSUBJECT and usage count.
 *
 *      See XWPSUBJECTINFO for the definition of a subject.
 *
 *      Required input in XWPSUBJECINFO:
 *
 *      -- id: for a user subject: the user ID;
 *             for a group subject: the group ID
 *
 *      -- bType: one of SUBJ_USER, SUBJ_GROUP, SUBJ_PROCESS
 *
 *      This is NOT validated and better be valid.
 *
 *      Output in XWPSUBJECINFO, if NO_ERROR is returned:
 *
 *      -- hSubject: new or existing subject handle.
 *
 *      -- cUsage: usage count.
 *
 *      Returns:
 *
 *      -- NO_ERROR
 *
 *      -- XWPSEC_HSUBJECT_EXISTS: SUBJ_USER only; user already
 *         has a subject handle.
 *
 *      Postconditions:
 *
 *      --  Caller must call scxtRefresh afterwards to rebuild
 *          the system ACL table and send it down to the driver.
 *          This is not done here because the caller may create
 *          many subject handles and only refresh the table after
 *          all subject handles have been created. However, this
 *          operation _must_ be synchroneous, so you may not
 *          defer it to a different thread.
 */

APIRET scxtCreateSubject(PXWPSUBJECTINFO pSubjectInfo) // in/out: subject info
{
    APIRET arc = NO_ERROR;

    BOOL fLocked;
    if (!(fLocked = LockACLs()))
        arc = XWPSEC_CANNOT_GET_MUTEX;
    else
    {
        PSUBJECTTREENODE pExisting;
        if (pExisting = FindSubjectInfoFromID(pSubjectInfo->bType,
                                              pSubjectInfo->id))
        {
            // subject exists:
            // increase usage count
            pExisting->SubjectInfo.cUsage++;
            // output to caller
            pSubjectInfo->hSubject = pExisting->SubjectInfo.hSubject;
            pSubjectInfo->cUsage = pExisting->SubjectInfo.cUsage;
        }
        else
        {
            // not created yet:
            // allocate new subject info
            PSUBJECTTREENODE pNewSubject;
            if (!(pNewSubject = (PSUBJECTTREENODE)malloc(sizeof(SUBJECTTREENODE))))
                arc = ERROR_NOT_ENOUGH_MEMORY;
            else
            {
                ULONG   ulNewSubjectHandle;
                // new subject handle (globally unique for each boot)
                if (    (pSubjectInfo->bType == SUBJ_USER)
                     && (pSubjectInfo->id == 0)
                   )
                    // root logging on:
                    ulNewSubjectHandle = 0;
                else
                    // non-root: use new subject handle
                    ulNewSubjectHandle = G_ulNextHSubject++;

                // set key
                pNewSubject->Tree.ulKey = ulNewSubjectHandle;

                // append new item
                if (treeInsert(&G_treeSubjects,
                               &G_cSubjects,
                               (TREE*)pNewSubject,
                               treeCompareKeys))
                {
                    arc = XWPSEC_INTEGRITY;
                    free(pNewSubject);
                }
                else
                {
                    // set up data:

                    // new subject handle
                    pNewSubject->SubjectInfo.hSubject = ulNewSubjectHandle;
                    // copy ID (user or group)
                    pNewSubject->SubjectInfo.id = pSubjectInfo->id;
                    // copy type
                    pNewSubject->SubjectInfo.bType = pSubjectInfo->bType;
                    // set usage count
                    pNewSubject->SubjectInfo.cUsage = 1;

                    // output to caller
                    pSubjectInfo->hSubject = pNewSubject->SubjectInfo.hSubject;
                    pSubjectInfo->cUsage = pNewSubject->SubjectInfo.cUsage;

                    // call ACL database
                    // arc = saclSubjectHandleCreated(pSubjectInfo);
                }
            }
        }
    }

    if (fLocked)
        UnlockACLs();

    return arc;
}

/*
 *@@ scxtDeleteSubject:
 *      lowers the usage count of the given subject handle and
 *      deletes it if it is no longer in use.
 *
 *      Returns:
 *
 *      -- NO_ERROR
 *
 *      -- XWPSEC_INVALID_HSUBJECT: SUBJ_USER only; user already
 *         has a subject handle.
 *
 *      Postconditions:
 *
 *      --  Caller must call scxtRefresh afterwards to rebuild
 *          the system ACL table and send it down to the driver.
 *          This is not done here because the caller may create
 *          many subject handles and only refresh the table after
 *          all subject handles have been created. However, this
 *          operation _must_ be synchroneous, so you may not
 *          defer it to a different thread.
 */

APIRET scxtDeleteSubject(LHANDLE hSubject)
{
    APIRET arc = NO_ERROR;

    BOOL fLocked;
    if (!(fLocked = LockACLs()))
        arc = XWPSEC_CANNOT_GET_MUTEX;
    else
    {
        PSUBJECTTREENODE psi;
        if (!(psi = FindSubjectInfoFromHandle(hSubject)))
            // not found:
            arc = XWPSEC_INVALID_HSUBJECT;
        else
            // decrease usage count
            if (!(psi->SubjectInfo.cUsage))
                arc = XWPSEC_INTEGRITY;
            else
            {
                psi->SubjectInfo.cUsage--;
                if (!(psi->SubjectInfo.cUsage))
                {
                    // usage count reached zero:
                    if (treeDelete(&G_treeSubjects,
                                   &G_cSubjects,
                                   (TREE*)psi))
                        arc = XWPSEC_INTEGRITY;
                    else
                        free(psi);
                }
            }
    }

    if (fLocked)
        UnlockACLs();

    return arc;
}

/*
 *@@ scxtQuerySubjectInfo:
 *      returns subject info for a subject handle.
 *
 *      On input, specify hSubject in pSubjInfo.
 *
 *      Returns:
 *
 *      -- NO_ERROR: user found, pSubjectInfo was filled.
 *
 *      -- XWPSEC_INVALID_HSUBJECT: specified subject handle
 *         does not exist.
 */

APIRET scxtQuerySubjectInfo(PXWPSUBJECTINFO pSubjectInfo)   // in/out: subject info

{
    APIRET arc = NO_ERROR;

    BOOL fLocked;
    if (!(fLocked = LockACLs()))
        arc = XWPSEC_CANNOT_GET_MUTEX;
    else
    {
        PSUBJECTTREENODE p;
        LHANDLE hsubj = pSubjectInfo->hSubject;

        // process was started before XWPShell: assume root for now
        if (hsubj == -1)
            hsubj = 0;

        if (!(p = FindSubjectInfoFromHandle(hsubj)))
            arc = XWPSEC_INVALID_HSUBJECT;
        else
            memcpy(pSubjectInfo,
                   &p->SubjectInfo,
                   sizeof(XWPSUBJECTINFO));
    }

    if (fLocked)
        UnlockACLs();

    return arc;
}

/*
 *@@ scxtFindSubject:
 *      returns the subject handle for the given user, group,
 *      or process ID.
 *
 *      Returns:
 *
 *      -- NO_ERROR: user found, pSubjectInfo was filled.
 *
 *      -- XWPSEC_INVALID_ID: no subject handle exists
 *         for the given ID.
 *
 *@@added V1.0.1 (2003-01-05) [umoeller]
 */

APIRET scxtFindSubject(BYTE bType,              // in: one of SUBJ_USER, SUBJ_GROUP, or SUBJ_PROCESS
                       XWPSECID id,             // in: user, group, or process ID
                       HXSUBJECT *phSubject)    // out: subject handle if NO_ERROR
{
    APIRET arc = NO_ERROR;

    BOOL fLocked;
    if (!(fLocked = LockACLs()))
        arc = XWPSEC_CANNOT_GET_MUTEX;
    else
    {
        PSUBJECTTREENODE p;
        if (!(p = FindSubjectInfoFromID(bType, id)))
            arc = XWPSEC_INVALID_ID;
        else
            *phSubject = p->SubjectInfo.hSubject;
    }

    if (fLocked)
        UnlockACLs();

    return arc;
}

/* ******************************************************************
 *
 *   Security context entry points
 *
 ********************************************************************/

/*
 *@@ scxtFindSecurityContext:
 *      this attempts to find a security context
 *      from a process ID.
 *
 *      This returns:
 *
 *      --  NO_ERROR: *ppContext was set to a newly allocated
 *          security context, which is to be free()'d by caller.
 *
 *      --  ERROR_NOT_ENOUGH_MEMORY
 *
 *      --  XWPSEC_INVALID_PID: security context doesn't exist.
 */

APIRET scxtFindSecurityContext(ULONG ulPID,
                               PXWPSECURITYCONTEXT *ppContext)
{
    // @@todo no security contexts yet, build one for the current
    // local user
    APIRET          arc;
    XWPSECID        uid;
    PXWPLOGGEDON    pLogon;
    if (    (!(arc = slogQueryLocalUser(&uid)))
         && (!(arc = slogQueryLogon(uid,
                                    &pLogon)))
       )
    {
        PXWPSECURITYCONTEXT pReturn;
        ULONG cb =   sizeof(XWPSECURITYCONTEXT)
                   + ((pLogon->cSubjects)
                        ? (pLogon->cSubjects - 1) * sizeof(HXSUBJECT)
                        : 0);
        if (!(pReturn = (PXWPSECURITYCONTEXT)malloc(cb)))
            arc = ERROR_NOT_ENOUGH_MEMORY;
        else
        {
            pReturn->cbStruct = cb;
            pReturn->ulPID = ulPID;
            pReturn->cSubjects = pLogon->cSubjects;
            memcpy(pReturn->aSubjects,
                   pLogon->aSubjects,
                   pLogon->cSubjects * sizeof(HXSUBJECT));

            *ppContext = pReturn;
        }

        free(pLogon);
    }

    return NO_ERROR;
}

/*
 *@@ scxtVerifyAuthority:
 *      returns NO_ERROR only if the specified process
 *      has sufficient authority to perform the
 *      given action.
 *
 *      Otherwise this returns XWPSEC_INSUFFICIENT_AUTHORITY.
 *
 *@@added V0.9.19 (2002-04-02) [umoeller]
 */

APIRET scxtVerifyAuthority(PXWPSECURITYCONTEXT pContext,
                           ULONG flActions)
{
    if (!pContext || !flActions)
        return ERROR_INVALID_PARAMETER;

    // presently, we only allow root to do anything
    if (    (pContext->cSubjects)
         && (!pContext->aSubjects[0])       // null is root
       )
        return NO_ERROR;

    return XWPSEC_INSUFFICIENT_AUTHORITY;
}

/* ******************************************************************
 *
 *   ACL entry points
 *
 ********************************************************************/

/*
 *@@ scxtRefresh:
 *      rebuilds the system ACL table and sends it down to
 *      the driver.
 *
 *@@added V1.0.1 (2003-01-05) [umoeller]
 */

APIRET scxtRefresh(VOID)
{
    APIRET  arc = NO_ERROR;
    BOOL    fLocked;

    if (!(fLocked = LockACLs()))
        arc = XWPSEC_CANNOT_GET_MUTEX;
    else
    {
        PACLDBTREENODE  pNode;
        ULONG           cbTotal,
                        cACLs;

        // free previous ACL table, if any
        if (G_pRing0Buf)
        {
            DosFreeMem(G_pRing0Buf);
            G_pRing0Buf = NULL;
        }

        // step 1: check how much memory we need

        cbTotal = sizeof(RING0BUF);
        cACLs = 0;

        // @@todo build subjects array

        for (pNode = (PACLDBTREENODE)treeFirst(G_treeACLDB);
             pNode;
             pNode = (PACLDBTREENODE)treeNext((TREE*)pNode))
        {
            cbTotal += CalcACLNodeSize(pNode, NULL);
            ++cACLs;
        }

        // step 2: allocate buffer

        if (    (cbTotal)
             && (!(arc = DosAllocMem((PVOID*)&G_pRing0Buf,
                                     cbTotal,
                                     PAG_COMMIT | OBJ_TILE | PAG_READ | PAG_WRITE)))
           )
        {
            PBYTE   pbCurrent = (PBYTE)G_pRing0Buf + sizeof(RING0BUF);

            G_pRing0Buf->cbTotal = cbTotal;
            G_pRing0Buf->cSubjectInfos = 0;
            G_pRing0Buf->cACLs = cACLs;
            G_pRing0Buf->ofsACLs = sizeof(RING0BUF);

            // step 3: fill subject infos @@todo

            // step 4: fill ACLs
            pbCurrent = (PBYTE)G_pRing0Buf + G_pRing0Buf->ofsACLs;
            for (pNode = (PACLDBTREENODE)treeFirst(G_treeACLDB);
                 pNode;
                 pNode = (PACLDBTREENODE)treeNext((TREE*)pNode))
            {
                ULONG           ul;
                PRESOURCEACL    pEntryThis = (PRESOURCEACL)pbCurrent;
                PACCESS         pAccessThis;
                PLISTNODE       pListNode;

                pEntryThis->cbStruct = CalcACLNodeSize(pNode,
                                                       pEntryThis);
                memcpy(pEntryThis->szName,
                       pNode->szResName,
                       pNode->usResNameLen + 1);

                // set up ACCESS structs
                pAccessThis = (PACCESS)((PBYTE)pEntryThis->szName + pEntryThis->cbName);
                for (pListNode = lstQueryFirstNode(&pNode->llPerms);
                     pListNode;
                     pListNode = pListNode->pNext)
                {
                    PACLDBPERM pPerm = (PACLDBPERM)pListNode->pItemData;
                    if (scxtFindSubject(pPerm->bType,
                                        pPerm->id,
                                        &pAccessThis->hSubject))
                        // error (no subject found for this node):
                        // then the user or group is not currently in
                        // use, so we must fill in the -1 dummy hSubject
                        // to block access
                        pAccessThis->hSubject = -1;

                    pAccessThis->fbAccess = pPerm->fbPerm;

                    pAccessThis++;
                }

                pbCurrent += pEntryThis->cbStruct;
            }

            // @@todo call driver
        }
    }

    if (fLocked)
        UnlockACLs();

    return arc;
}

/*
 *@@ FindAccess:
 *
 *      Preconditions:
 *
 *      --  Caller must have checked that G_pbSysACLs is not NULL.
 *
 *      --  Caller must hold ACLDB mutex.
 *
 *@@added V1.0.1 (2003-01-05) [umoeller]
 */

PACCESS FindAccess(PCSZ pcszResource,
                   PULONG pcAccesses)       // out: array item count
{
    PBYTE   pbCurrent = (PBYTE)G_pRing0Buf + G_pRing0Buf->ofsACLs;
    ULONG   ul;

    for (ul = 0;
         ul < G_pRing0Buf->cACLs;
         ++ul)
    {
        PRESOURCEACL pEntry = (PRESOURCEACL)pbCurrent;
        if (!strcmp(pcszResource,
                    pEntry->szName))
        {
            *pcAccesses = pEntry->cAccesses;
            return (PACCESS)((PBYTE)pEntry->szName + pEntry->cbName);
        }

        pbCurrent += pEntry->cbStruct;
    }

    return NULL;
}

/*
 *@@ QueryPermissions:
 *      returns the ORed XWPACCESS_* flags for the
 *      given resource based on the security context
 *      represented by the given subject handles.
 *
 *      Preconditions:
 *
 *      --  Caller must hold ACLDB mutex.
 *
 *@@added V1.0.1 (2003-01-05) [umoeller]
 */

ULONG QueryPermissions(PSZ pszResource,             // in: buf with res name (trashed!)
                       ULONG cSubjects,
                       const HXSUBJECT *paSubjects)
{
    PACCESS paAccesses;
    ULONG   cAccesses;

    if (!*paSubjects || !G_pRing0Buf)
        // null subject handle: root may do anything
        return XWPACCESS_ALL;

    while (TRUE)
    {
        PSZ p;

        if (paAccesses = FindAccess(pszResource,
                                    &cAccesses))
        {
            ULONG   flAccess = 0;
            ULONG   ulA,
                    ulS;
            for (ulA = 0;
                 ulA < cAccesses;
                 ++ulA)
            {
                for (ulS = 0;
                     ulS < cSubjects;
                     ++ulS)
                {
                    if (paSubjects[ulS] == paAccesses[ulA].hSubject)
                        flAccess |= paAccesses[ulA].fbAccess;
                }
            }

            return flAccess;
        }

        // not found: climb up to parent
        if (p = strrchr(pszResource, '\\'))
            *p = 0;
        else
            break;
    }

    return 0;
}

/*
 *@@ scxtQueryPermissions:
 *
 *      Returns one of:
 *
 *      --  NO_ERROR
 *
 *      --  ERROR_NOT_ENOUGH_MEMORY
 *
 *@@added V1.0.1 (2003-01-05) [umoeller]
 */

APIRET scxtQueryPermissions(PCSZ pcszResource,          // in: resource name
                            ULONG cSubjects,            // in: no. of subjects in paSubjects array
                            const HXSUBJECT *paSubjects,    // in: array of subject handles
                            PULONG pulAccess)           // out: XWPACCESS_* flags
{
    APIRET  arc = NO_ERROR;
    BOOL    fLocked;

    if (!(fLocked = LockACLs()))
        arc = XWPSEC_CANNOT_GET_MUTEX;
    else
    {
        PSZ p;
        if (!(p = strdup(pcszResource)))
            arc = ERROR_NOT_ENOUGH_MEMORY;
        else
        {
            *pulAccess = QueryPermissions(p,
                                          cSubjects,
                                          paSubjects);
            free(p);
        }
    }

    if (fLocked)
        UnlockACLs();

    return arc;
}


