
/*
 *@@sourcefile contexts.c:
 *      security contexts management for XWorkplace Security.
 *
 *      A security context is defined in XWPSECURITYCONTEXT.
 *      One security context is created for each process on
 *      the system and contains the subject handles representing
 *      the process's access rights.
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

/* ******************************************************************
 *
 *   Private Definitions
 *
 ********************************************************************/

/*
 *@@ CONTEXTTREENODE:
 *
 */

typedef struct _CONTEXTTREENODE
{
    TREE        Tree;               // tree item (required for tree* to work);
                                    // ID has the PID
    XWPSECURITYCONTEXT Context;     // security context
} CONTEXTTREENODE, *PCONTEXTTREENODE;

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

// subject infos
TREE        *G_treeContexts;
    // balanced binary tree of current security contexts
LONG        G_cContexts = 0;
HMTX        G_hmtxContexts = NULLHANDLE;
    // mutex semaphore protecting global data

/* ******************************************************************
 *
 *   Initialization
 *
 ********************************************************************/

/*
 *@@ scxtInit:
 *      initializes XWPSecurity.
 */

APIRET scxtInit(VOID)
{
    APIRET arc = NO_ERROR;

    if (G_hmtxContexts == NULLHANDLE)
    {
        // first call:
        arc = DosCreateMutexSem(NULL,       // unnamed
                                &G_hmtxContexts,
                                0,          // unshared
                                FALSE);     // unowned
        if (arc == NO_ERROR)
            treeInit(&G_treeContexts, &G_cContexts);
    }
    else
        arc = XWPSEC_INSUFFICIENT_AUTHORITY;

    return (arc);
}

/* ******************************************************************
 *
 *   Private Helpers
 *
 ********************************************************************/

/*
 *@@ LockContexts:
 *      locks the global security data by requesting
 *      its mutex.
 *
 *      Always call UnlockContexts() when you're done.
 */

APIRET LockContexts(VOID)
{
    APIRET arc = NO_ERROR;

    arc = DosRequestMutexSem(G_hmtxContexts,
                             SEM_INDEFINITE_WAIT);

    return (arc);
}

/*
 *@@ UnlockContexts:
 *      unlocks the global security data.
 */

APIRET UnlockContexts(VOID)
{
    return (DosReleaseMutexSem(G_hmtxContexts));
}

/*
 *@@ FindContextFromPID:
 *      searches the list of security contexts for the
 *      specified process ID.
 *
 *      Returns the XWPSECURITYCONTEXT from the list
 *      or NULL if not found.
 *
 *      Private function.
 *
 *      You must call LockContexts() first.
 */

PCONTEXTTREENODE FindContextFromPID(ULONG ulPID)
{
    PCONTEXTTREENODE pTreeItem = (PCONTEXTTREENODE)treeFind(G_treeContexts,
                                                            ulPID,
                                                            treeCompareKeys);


    return (pTreeItem);
}

/* ******************************************************************
 *
 *   Security Contexts APIs
 *
 ********************************************************************/

/*
 *@@ scxtCreateSecurityContext:
 *      this creates a new security context
 *      for the specified process ID.
 */

APIRET scxtCreateSecurityContext(ULONG ulPID,
                                 HXSUBJECT hsubjUser,
                                 HXSUBJECT hsubjGroup)
{
    APIRET arc = NO_ERROR;

    BOOL fLocked = (LockContexts() == NO_ERROR);
    if (!fLocked)
        arc = XWPSEC_CANNOT_GET_MUTEX;
    else
    {
        // first check if a tree item for that PID
        // exists already... this might happen
        PCONTEXTTREENODE pContextItem;
        if (pContextItem = FindContextFromPID(ulPID))
        {
            // exists:
            // replace with new values
            pContextItem->Context.hsubjUser = hsubjUser;
            pContextItem->Context.hsubjGroup = hsubjGroup;
        }
        else
        {
            // new item needed:
            if (!(pContextItem = (PCONTEXTTREENODE)malloc(sizeof(CONTEXTTREENODE))))
                arc = ERROR_NOT_ENOUGH_MEMORY;
            else
            {
                int i = 0;

                pContextItem->Tree.ulKey = ulPID;

                pContextItem->Context.ulPID = ulPID;
                pContextItem->Context.hsubjUser = hsubjUser;
                pContextItem->Context.hsubjGroup = hsubjGroup;

                if (treeInsert(&G_treeContexts,
                               &G_cContexts,
                               (TREE*)pContextItem,
                               treeCompareKeys))
                    // shouldn't happen
                    arc = XWPSEC_INTEGRITY;
            }
        }

        /* _Pmpf(("scxtCreateSecurityContext: pid 0x%lX (%d), arc: %d",
                ulPID, ulPID, arc)); */
    }

    if (fLocked)
        UnlockContexts();

    return (arc);
}

/*
 *@@ scxtDeleteSecurityContext:
 *      deletes the security context for the
 *      specified process ID.
 *
 *      Returns:
 *      -- NO_ERROR: security context was deleted.
 *
 *      -- XWPSEC_INVALID_PID: security context doesn't exist.
 */

APIRET scxtDeleteSecurityContext(ULONG ulPID)
{
    APIRET arc = NO_ERROR;
    BOOL fLocked = (LockContexts() == NO_ERROR);
    if (!fLocked)
        arc = XWPSEC_CANNOT_GET_MUTEX;
    else
    {
        PCONTEXTTREENODE pContextItem;
        if (!(pContextItem = FindContextFromPID(ulPID)))
            arc = XWPSEC_INVALID_PID;
        else
        {
            if (treeDelete(&G_treeContexts,
                           &G_cContexts,
                           (TREE*)pContextItem))
                // shouldn't happen
                arc = XWPSEC_INTEGRITY;
            else
            {
                free(pContextItem);
            }
        }
    }

    if (fLocked)
        UnlockContexts();

    return (arc);
}

/*
 *@@ scxtFindSecurityContext:
 *      this attempts to find a security context
 *      from a process ID.
 *
 *      On input, specify XWPSECURITYCONTEXT.ulPID.
 *
 *      This returns:
 *
 *      -- NO_ERROR: rest of XWPSECURITYCONTEXT was
 *         filled with context info.
 *
 *      -- XWPSEC_INVALID_PID: security context doesn't exist.
 */

APIRET scxtFindSecurityContext(PXWPSECURITYCONTEXT pContext)
{
    APIRET arc = NO_ERROR;

    BOOL fLocked = (LockContexts() == NO_ERROR);
    if (!fLocked)
        arc = XWPSEC_CANNOT_GET_MUTEX;
    else
    {
        PCONTEXTTREENODE pContextItem;
        if (!(pContextItem = FindContextFromPID(pContext->ulPID)))
            arc = XWPSEC_INVALID_PID;
        else
        {
            memcpy(pContext,
                   &pContextItem->Context,
                   sizeof(XWPSECURITYCONTEXT));
        }
    }

    if (fLocked)
        UnlockContexts();

    return (arc);
}

/*
 *@@ scxtEnumSecurityContexts:
 *      enumerates all security contexts for the
 *      specified user subject handle.
 *
 *      If hsubjUser is 0, this enumerates all
 *      security contexts.
 *
 *      This allocates memory for the contexts.
 *      Pass paContexts to scxtFreeSecurityContexts
 *      when you're done with the data.
 *
 *      *pulCount receives the count of XWPSECURITYCONTEXT
 *      structures which were allocated and returned.
 *
 *      Returns:
 *
 *      -- NO_ERROR: paContexts allocated.
 *
 *      -- XWPSEC_NO_CONTEXTS: no contexts found.
 */

APIRET scxtEnumSecurityContexts(HXSUBJECT hsubjUser,
                                PXWPSECURITYCONTEXT *ppaContexts,
                                PULONG pulCount)
{
    APIRET arc = NO_ERROR;

    BOOL fLocked = (LockContexts() == NO_ERROR);
    if (!fLocked)
        arc = XWPSEC_CANNOT_GET_MUTEX;
    else
    {
        if (G_cContexts == 0)
            arc = XWPSEC_NO_CONTEXTS;
        else
        {
            PXWPSECURITYCONTEXT paContexts
                = (PXWPSECURITYCONTEXT)malloc(sizeof(XWPSECURITYCONTEXT) * G_cContexts);
            if (!paContexts)
                arc = ERROR_NOT_ENOUGH_MEMORY;
            else
            {
                ULONG   cCount = 0;
                PXWPSECURITYCONTEXT pContextThis = paContexts;

                TREE *t = treeFirst(G_treeContexts);
                while (t && !arc)
                {
                    if (    (hsubjUser == 0)     // enumerate all?
                         || (hsubjUser == ((PCONTEXTTREENODE)t)->Context.hsubjUser)
                       )
                    {
                        // security check:
                        if (cCount++ < G_cContexts)
                        {
                            memcpy(pContextThis,
                                   &(((PCONTEXTTREENODE)t)->Context),
                                   sizeof(XWPSECURITYCONTEXT));
                            pContextThis++;
                        }
                        else
                            arc = XWPSEC_INTEGRITY;
                    }

                    t = treeNext(t);
                }

                *ppaContexts = paContexts;
                *pulCount = cCount;
            }
        }
    }

    if (fLocked)
        UnlockContexts();

    return (arc);
}

/*
 *@@ scxtFreeSecurityContexts:
 *      frees resources allocated by scxtEnumSecurityContexts.
 */

APIRET scxtFreeSecurityContexts(PXWPSECURITYCONTEXT paContexts)
{
    if (paContexts)
        free(paContexts);

    return (NO_ERROR);
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
    if (    (pContext->hsubjUser == 0)
         || (pContext->hsubjGroup == 0)
       )
        return NO_ERROR;

    return XWPSEC_INSUFFICIENT_AUTHORITY;
}
