
/*
 *@@sourcefile subjects.c:
 *      subject handles management for XWorkplace Security.
 *
 *      A subject represents either a user, a group, or a
 *      special process.
 *
 *      Subjects are identified via subject handles. If
 *      a user successfully logs on (locally or via network),
 *      two subject handles are created for the user and the
 *      group he belongs to. For each user which is logged
 *      on, an XWPLOGGEDON is created which holds the subject
 *      handles for this user.
 *
 *      Subject handle 0 is special and means "no restrictions".
 *      Only the superuser ("root") will have user and group
 *      subject handles of 0. All other users will have nonzero
 *      subject handles.
 *
 *      In addition, this model supports trusted processes
 *      so that certain executables can be given additional
 *      privileges. This is not yet implemented.
 *
 *      Theoretically, on subject handle creation, an ACL
 *      database could load the ACL's associated with the user,
 *      the group, or the process associated with the subject
 *      handle. Whenever an action takes place which needs to be
 *      authorized (such as DosOpen), the ACLDB can then more
 *      quickly look up the access rights.
 *
 *      This feature is most useful with the LAN server model
 *      of ACL entries, where an infinite number of entries
 *      can be defined for each resource. This is presently
 *      not used in XWPSec because we use the Unix model with
 *      uid and gid statically assigned to each resource, so
 *      we need not preload anything.
 *
 *      Reversely, on logoff, the subject handles are deleted
 *      by calling subjDeleteSubject, which can in turn notify
 *      the ACLDB.
 *
 *      Comparison with other concepts:
 *
 *      -- The concept of a "subject" has been taken from
 *         SES. However, with SES, a subject is much more
 *         complicated and not really well explained. The
 *         SES documentation sucks anyway.
 *
 *      -- Warp (LAN) Server bases ACL entries on users and
 *         groups also, however without calling them "subjects".
 *         However, if I understood that right, LAN server can
 *         define an infinite number of ACL entries for each
 *         resource.
 *
 *      -- Linux does not have the notion of a subject.
 *         Instead, all access rights are determined from
 *         the user id (uid) and group id (gid), apparently.
 *         Even though we implement the subject handles here
 *         to allow replacing the current ACLDB implementation
 *         with a LAN server model, we are currently using
 *         the Unix model of ACL entries because it's simpler.
 *
 *      -- Java seems to have something similar; it bases
 *         ACL entries on "principals", where a principal
 *         can either be a user or a group. See the API
 *         docs for java.security.acl.Acl.
 *
 *@@added V0.9.5 [umoeller]
 *@@header "security\xwpsecty.h"
 */

/*
 *      Copyright (C) 2000 Ulrich M”ller.
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, in version 2 as it comes in the COPYING
 *      file of the XWorkplace main distribution.
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 */

#define  INCL_DOS
#define  INCL_DOSERRORS
#include <os2.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <io.h>

#include "setup.h"

#include "helpers\stringh.h"
#include "helpers\tree.h"               // red-black binary trees

#include "bldlevel.h"

#include "security\xwpsecty.h"

// #include "xwpshell.h"

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
 *                                                                  *
 *   Global variables                                               *
 *                                                                  *
 ********************************************************************/

// subject infos
TREE        *G_treeSubjects;
    // global linked list of currently active subjects
ULONG       G_cSubjects = 0;
    // subjects count
ULONG       G_ulNextHSubject = 1;
    // next HSUBJECT to use (raised with each subject creation);
    // we start with 1 (0 is special)

HMTX        G_hmtxSubjects = NULLHANDLE;
    // mutex semaphore protecting global data

/* ******************************************************************
 *
 *   Initialization
 *
 ********************************************************************/

/*
 *@@ subjInit:
 *      initializes XWPSecurity.
 */

APIRET subjInit(VOID)
{
    APIRET arc = NO_ERROR;

    if (G_hmtxSubjects == NULLHANDLE)
    {
        // first call:
        arc = DosCreateMutexSem(NULL,       // unnamed
                                &G_hmtxSubjects,
                                0,          // unshared
                                FALSE);     // unowned
        if (arc == NO_ERROR)
            treeInit(&G_treeSubjects);
    }
    else
        arc = XWPSEC_NO_AUTHORITY;

    return (arc);
}

/* ******************************************************************
 *
 *   Private Helpers
 *
 ********************************************************************/

/*
 *@@ LockSubjects:
 *      locks the global security data by requesting
 *      its mutex.
 *
 *      Always call UnlockSubjects() when you're done.
 */

APIRET LockSubjects(VOID)
{
    APIRET arc = NO_ERROR;

    arc = DosRequestMutexSem(G_hmtxSubjects,
                             SEM_INDEFINITE_WAIT);

    return (arc);
}

/*
 *@@ UnlockSubjects:
 *      unlocks the global security data.
 */

APIRET UnlockSubjects(VOID)
{
    return (DosReleaseMutexSem(G_hmtxSubjects));
}

/*
 *@@ fnCompareSubjects:
 *
 */

int fnCompareSubjects(unsigned long id1, unsigned long id2)
{
    if (id1 < id2)
        return -1;
    if (id1 > id2)
        return +1;
    return (0);
}

/* ******************************************************************
 *
 *   Private Helpers
 *
 ********************************************************************/

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
 *      Private function.
 *
 *      You must call LockSubjects() first.
 */

PSUBJECTTREENODE FindSubjectInfoFromHandle(HXSUBJECT hSubject)
{
    PSUBJECTTREENODE pTreeItem
        = (PSUBJECTTREENODE)treeFindEQID(&G_treeSubjects,
                                         hSubject,
                                         fnCompareSubjects);

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
 *      Private function.
 *
 *      You must call LockSubjects() first.
 */

PSUBJECTTREENODE FindSubjectInfoFromID(BYTE bType,       // in: SUBJ_USER, SUBJ_GROUP, or SUBJ_PROCESS
                                       XWPSECID id)      // in: ID to search for
{
    PSUBJECTTREENODE p = NULL;

    PSUBJECTTREENODE pNode = treeFirst(G_treeSubjects);
    while (pNode)
    {
        PXWPSUBJECTINFO psi = &pNode->SubjectInfo;

        if ((psi->id == id) && (psi->bType == bType))
        {
            p = pNode;
            break;
        }

        pNode = treeNext((TREE*)pNode);
    }

    return (p);
}

/* ******************************************************************
 *
 *   Public API's
 *
 ********************************************************************/

/*
 *@@ subjCreateSubject:
 *      creates a new subject handle, either for
 *      a user or a group.
 *
 *      Required input in XWPSUBJECINFO:
 *
 *      -- id: for a user subject: the user ID;
 *             for a group subject: the group ID
 *      -- bType: one of SUBJ_USER, SUBJ_GROUP, SUBJ_PROCESS
 *
 *      This is NOT validated and better be valid.
 *
 *      Output:
 *
 *      -- hSubject: new or existing subject handle.
 *      -- cUsage: usage count.
 *
 *      Note: This checks for whether a subject handle
 *      exists already for the specified ID. However,
 *      if a subject handle already exists, the behavior
 *      depends on the type:
 *
 *      -- For user subjects (SUBJ_USER specified with bType),
 *         if a subject handle exists for that ID,
 *         XWPSEC_HSUBJECT_EXISTS is returned, and no
 *         subject handle is created.
 *
 *      -- For group subjects (SUBJ_GROUP specified with bType),
 *         if a subject handle exists for that ID,
 *         NO_ERROR is returned, and the existing subject's
 *         usage count is incremented. pSubjectInfo then
 *         receives the existing HXSUBJECT and usage count.
 *
 *      See XWPSUBJECTINFO for the definition of a subject.
 *
 *      Returns:
 *
 *      -- NO_ERROR: subject handle created or usage count for
 *         existing subject handle raised (SUBJ_GROUP only).
 *         In that case, pSubjectInfo has been updated.
 *
 *      -- XWPSEC_HSUBJECT_EXISTS: SUBJ_USER only; user already
 *         has a subject handle.
 */

APIRET subjCreateSubject(PXWPSUBJECTINFO pSubjectInfo) // in/out: subject info
{
    APIRET arc = NO_ERROR;

    BOOL fLocked = (LockSubjects() == NO_ERROR);
    if (!fLocked)
        arc = XWPSEC_CANNOT_GET_MUTEX;
    else
    {
        PSUBJECTTREENODE pExisting = FindSubjectInfoFromID(pSubjectInfo->bType,
                                                           pSubjectInfo->id);
        if (pExisting)
        {
            // subject exists:
            if (pSubjectInfo->bType == SUBJ_USER)
                // user: this must be unique, so return error
                arc = XWPSEC_HSUBJECT_EXISTS;
            else if (pSubjectInfo->bType == SUBJ_GROUP)
            {
                // group: increase usage count
                pExisting->SubjectInfo.cUsage++;
                // output to caller
                pSubjectInfo->hSubject = pExisting->SubjectInfo.hSubject;
                pSubjectInfo->cUsage = pExisting->SubjectInfo.cUsage;
            }
        }
        else
        {
            // not created yet:
            // allocate new subject info
            PSUBJECTTREENODE pNewSubject
                = (PSUBJECTTREENODE)malloc(sizeof(SUBJECTTREENODE));
            if (!pNewSubject)
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
                pNewSubject->Tree.id = ulNewSubjectHandle;

                // append new item
                if (treeInsertID(&G_treeSubjects,
                                 (TREE*)pNewSubject,
                                 fnCompareSubjects,
                                 FALSE))
                {
                    arc = XWPSEC_INTEGRITY;
                    free(pNewSubject);
                }
                else
                {
                    G_cSubjects++;

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
        UnlockSubjects();

    return (arc);
}

/*
 *@@ subjDeleteSubject:
 *      deletes an existing subject.
 *
 *      Required authorities: Logon.
 */

APIRET subjDeleteSubject(LHANDLE hSubject)
{
    APIRET arc = NO_ERROR;

    BOOL fLocked = (LockSubjects() == NO_ERROR);
    if (!fLocked)
        arc = XWPSEC_CANNOT_GET_MUTEX;
    else
    {
        PSUBJECTTREENODE psi = FindSubjectInfoFromHandle(hSubject);
        if (!psi)
            // not found:
            arc = XWPSEC_INVALID_HSUBJECT;
        else
            if (treeDelete(&G_treeSubjects,
                           (TREE*)psi))
                arc = XWPSEC_INTEGRITY;
            else
            {
                G_cSubjects--;
                free(psi);
            }
    }

    if (fLocked)
        UnlockSubjects();

    return (arc);
}

/*
 *@@ subjQuerySubjectInfo:
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

APIRET subjQuerySubjectInfo(PXWPSUBJECTINFO pSubjectInfo)   // in/out: subject info

{
    APIRET arc = XWPSEC_INVALID_HSUBJECT;

    BOOL fLocked = (LockSubjects() == NO_ERROR);
    if (fLocked)
    {
        PSUBJECTTREENODE p = FindSubjectInfoFromHandle(pSubjectInfo->hSubject);
        if (p)
        {
            memcpy(pSubjectInfo, &p->SubjectInfo, sizeof(XWPSUBJECTINFO));
            arc = NO_ERROR;
        }
    }

    if (fLocked)
        UnlockSubjects();

    return (arc);
}


