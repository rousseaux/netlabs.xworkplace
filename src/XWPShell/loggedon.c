
/*
 *@@sourcefile loggedon.c:
 *      implements management for users which are currently
 *      logged on with XWorkplace Security (XWPSec).
 *
 *      A user can be logged onto the system by calling
 *      slogLogOn. This will create subject handles for
 *      the user and his group and store the user in the
 *      database.
 *
 *      Reversely, the user is logged off via slogLogOff.
 *
 *      Note: The APIs in this file do not differentiate
 *      between local and remote users. It is the responsibility
 *      of the caller to manage that information. With the
 *      current implementation, this is taken care of by
 *      XWPShell, which handles only one local user.
 *
 *      Neither does this function restart the user shell.
 *
 *      These functions can get called by:
 *
 *      -- XWPShell (or any other shell wrapper) to log
 *         a user onto a system (with data entered by
 *         a logon dialog) and then start a shell accordingly.
 *
 *      -- Any other program to change its own security
 *         context. For example, the user might open a
 *         command shell and might want to run a single
 *         program with administrator privileges.
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

#include "helpers\dosh.h"
#include "helpers\linklist.h"
#include "helpers\stringh.h"

#include "bldlevel.h"

#include "security\xwpsecty.h"

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

// subject infos
LINKLIST    G_llLoggedOn;
    // global linked list of currently active subjects
ULONG       G_cLoggedOn = 0;
    // subjects count

PXWPLOGGEDON G_pLoggedOnLocal = NULL;
    // current local user (there can be only one)

HMTX        G_hmtxLoggedOn = NULLHANDLE;
    // mutex semaphore protecting global data

/* ******************************************************************
 *
 *   Initialization
 *
 ********************************************************************/

/*
 *@@ slogInit:
 *      initializes XWPSecurity.
 */

APIRET slogInit(VOID)
{
    APIRET arc = NO_ERROR;

    if (G_hmtxLoggedOn == NULLHANDLE)
    {
        // first call:
        arc = DosCreateMutexSem(NULL,       // unnamed
                                &G_hmtxLoggedOn,
                                0,          // unshared
                                FALSE);     // unowned
        if (arc == NO_ERROR)
            lstInit(&G_llLoggedOn, TRUE);
    }
    else
        arc = XWPSEC_INSUFFICIENT_AUTHORITY;

    return arc;
}

/* ******************************************************************
 *
 *   Private Helpers
 *
 ********************************************************************/

/*
 *@@ LockLoggedOn:
 *      locks the global security data by requesting
 *      its mutex.
 *
 *      Always call UnlockLoggedOn() when you're done.
 */

APIRET LockLoggedOn(VOID)
{
    APIRET arc = NO_ERROR;

    arc = DosRequestMutexSem(G_hmtxLoggedOn,
                             SEM_INDEFINITE_WAIT);

    return arc;
}

/*
 *@@ UnlockLoggedOn:
 *      unlocks the global security data.
 */

APIRET UnlockLoggedOn(VOID)
{
    return (DosReleaseMutexSem(G_hmtxLoggedOn));
}

/*
 *@@ FindLoggedOnFromID:
 *      searches the list of logged-on users for the
 *      specified user ID.
 *
 *      Returns the XWPLOGGEDON from the list
 *      or NULL if not found.
 *
 *      Private function.
 *
 *      You must call LockLoggedOn() first.
 */

PCXWPLOGGEDON FindLoggedOnFromID(XWPSECID uid)
{
    PCXWPLOGGEDON p = NULL;

    PLISTNODE pNode = lstQueryFirstNode(&G_llLoggedOn);
    while (pNode)
    {
        PCXWPLOGGEDON plo = (PCXWPLOGGEDON)pNode->pItemData;

        if (plo->uid == uid)
        {
            p = plo;
            break;
        }

        pNode = pNode->pNext;
    }

    return (p);
}

/*
 *@@ RegisterLoggedOn:
 *      registers the specified users with the list
 *      of currently logged on users.
 *
 *      This does not create subject handles. These
 *      must have been created before calling this
 *      function.
 *
 *      Returns:
 *
 *      -- NO_ERROR: user was stored.
 *
 *      -- XWPSEC_USER_EXISTS: a user with this uid
 *         was already stored in the list.
 */

APIRET RegisterLoggedOn(PCXWPLOGGEDON pcNewUser,
                        BOOL fLocal)            // in: if TRUE, mark user as local
{
    APIRET arc = NO_ERROR;

    BOOL fLocked = (LockLoggedOn() == NO_ERROR);
    if (!fLocked)
        arc = XWPSEC_CANNOT_GET_MUTEX;
    else
    {
        if (FindLoggedOnFromID(pcNewUser->uid))
            // error:
            arc = XWPSEC_USER_EXISTS;
        else
        {
            PXWPLOGGEDON pNew = malloc(sizeof(XWPLOGGEDON));
            if (!pNew)
                arc = ERROR_NOT_ENOUGH_MEMORY;
            else
            {
                memcpy(pNew, pcNewUser, sizeof(XWPLOGGEDON));
                if (!lstAppendItem(&G_llLoggedOn, pNew))
                    arc = XWPSEC_INTEGRITY;
                else
                    if (fLocal)
                        G_pLoggedOnLocal = pNew;
            }
        }
    }

    if (fLocked)
        UnlockLoggedOn();

    return arc;
}

/*
 *@@ DeregisterLoggedOn:
 *      removes the user with the specified user ID
 *      from the list of currently logged on users.
 *
 *      On input, specify the user's ID (uid) in
 *      pUser.
 *
 *      If NO_ERROR is returned, the user has been
 *      removed. In that case, the user's subject
 *      handles (for user and group ID's) are stored
 *      in pUser.
 *
 *      This does not delete subject handles. These
 *      must be deleted after calling this function.
 */

APIRET DeregisterLoggedOn(PXWPLOGGEDON pUser)
{
    APIRET arc = NO_ERROR;

    BOOL fLocked = (LockLoggedOn() == NO_ERROR);
    if (!fLocked)
        arc = XWPSEC_CANNOT_GET_MUTEX;
    else
    {
        PCXWPLOGGEDON pFound = FindLoggedOnFromID(pUser->uid);
        if (!pFound)
            // error:
            arc = XWPSEC_INVALID_USERID;
        else
        {
            // copy handles etc. for output
            memcpy(pUser, pFound, sizeof(XWPLOGGEDON));
            // remove from list
            if (!lstRemoveItem(&G_llLoggedOn,
                               (PVOID)pFound))
                arc = XWPSEC_INTEGRITY;
            else
                // warning: pFound has been freed
                if (pFound == G_pLoggedOnLocal)
                    // this was the local user:
                    G_pLoggedOnLocal = NULL;
        }
    }

    if (fLocked)
        UnlockLoggedOn();

    return arc;
}

/* ******************************************************************
 *
 *   Logged-on Users Management (Public APIs)
 *
 ********************************************************************/

/*
 *@@ slogLogOn:
 *      logs the specified user onto the system.
 *
 *      On input, specify XWPLOGGEDON.szUserName
 *      and pcszPassword.
 *
 *      This function then does the following:
 *
 *      1) Calls sudbAuthenticateUser.
 *
 *      2) Creates subject handles for the
 *         user's ID and group ID by calling
 *         subjCreateSubject.
 *
 *      3) Registers the user as logged on
 *         in our internal list.
 *
 *      4) Changes the security context of the
 *         calling process to the new user
 *         and group subject handles.
 *
 *      On output, XWPLOGGEDON is filled with the
 *      remaining user information (user/group names,
 *      ID's, subject handles).
 *
 *      XWPShell calls this API after the user has
 *      entered his user name and password and has
 *      pressed OK in the logon dialog.
 *
 *      See loggedon.c for additional remarks and
 *      restrictions.
 *
 *      Returns:
 *
 *      --  NO_ERROR: user was successfully logged on.
 *          The uid must then be passed to slogLogOff
 *          to log the user off again.
 *
 *      --  XWPSEC_NOT_AUTHENTICATED: authentication failed.
 *          NOTE: In that case, the function blocks for
 *          approximately three seconds before returning.
 *
 *      --  XWPSEC_USER_EXISTS: user is already logged on.
 */

APIRET slogLogOn(PXWPLOGGEDON pNewUser,     // in/out: user info
                 const char *pcszPassword,  // in: password
                 BOOL fLocal)
{
    APIRET arc = NO_ERROR;

    XWPUSERDBENTRY  uiLogon;
    XWPGROUPDBENTRY giLogon;

    _Pmpf((__FUNCTION__ ": entering, user \"%s\", pwd \"%s\"",
                pNewUser->szUserName,
                pcszPassword));

    strhncpy0(uiLogon.szUserName,
              pNewUser->szUserName,
              sizeof(uiLogon.szUserName));
    strhncpy0(uiLogon.szPassword,
              pcszPassword,
              sizeof(uiLogon.szPassword));

    arc = sudbAuthenticateUser(&uiLogon,
                               &giLogon);

    if (arc != NO_ERROR)
    {
        DosSleep(3000);
    }
    else
    {
        // create subject handles:
        XWPSUBJECTINFO  siUser;

        // create user subject (always)
        siUser.hSubject = 0;
        siUser.id = uiLogon.uid;   // returned by sudbAuthenticateUser
        siUser.bType = SUBJ_USER;

        _Pmpf(("  sudbAuthenticateUser returned uid %d, gid %d", uiLogon.uid, uiLogon.gid));

        if (!(arc = subjCreateSubject(&siUser)))
        {
            // OK:
            XWPSUBJECTINFO siGroup;

            // copy user subject and id
            pNewUser->hsubjUser = siUser.hSubject;
            pNewUser->uid = siUser.id;
            // user name has been set from input

            // create group subject (if it doesn't exist yet)
            siGroup.hSubject = 0;
            siGroup.id = uiLogon.gid;     // returned by sudbAuthenticateUser
            siGroup.bType = SUBJ_GROUP;

            if (!(arc = subjCreateSubject(&siGroup)))
            {
                // copy group subject and id
                pNewUser->hsubjGroup = siGroup.hSubject;
                pNewUser->gid = siGroup.id;
                strcpy(pNewUser->szGroupName, giLogon.szGroupName);

                // and register this user
                if (arc = RegisterLoggedOn(pNewUser,
                                           fLocal))
                {
                    // error: clean up
                    subjDeleteSubject(siGroup.hSubject);
                }
            }

            if (!arc)
                _Pmpf(("Logged on uid %d, gid %d", pNewUser->uid, pNewUser->gid));
            else
                // clean up
                subjDeleteSubject(siUser.hSubject);
        }
    }

    _Pmpf((__FUNCTION__ ": leaving, returning %d", arc));

    return arc;
}

/*
 *@@ slogLogOff:
 *      logs off the specified user.
 *
 *      This does the following:
 *
 *      1) Removes the user from the list
 *         of logged-on users.
 *
 *      2) Deletes the subject handles for the
 *         user's ID and group ID.
 *
 *      Returns:
 *
 *      -- NO_ERROR: user was successfully
 *         logged off.
 *
 *      -- XWPSEC_INVALID_USERID: uid does
 *         not specify a currently logged-on
 *         user.
 */

APIRET slogLogOff(XWPSECID uid)
{
    APIRET arc = NO_ERROR;

    XWPLOGGEDON LogOff;
    LogOff.uid = uid;

    if (!(arc = DeregisterLoggedOn(&LogOff)))
    {
        if (!(arc = subjDeleteSubject(LogOff.hsubjUser)))
            arc = subjDeleteSubject(LogOff.hsubjGroup);
    }

    return arc;
}

/*
 *@@ slogQueryLocalUser:
 *      returns the data of the current local user.
 *
 *      If NO_ERROR is returned, pLoggedOnLocal
 *      receives the logon data of the local user.
 *
 *      If XWPSEC_NO_LOCAL_USER, no local user
 *      has logged on.
 */

APIRET slogQueryLocalUser(PXWPLOGGEDON pLoggedOnLocal)
{
    APIRET arc = NO_ERROR;

    BOOL fLocked = (LockLoggedOn() == NO_ERROR);
    if (!fLocked)
        arc = XWPSEC_CANNOT_GET_MUTEX;
    else
    {
        if (!G_pLoggedOnLocal)
            arc = XWPSEC_NO_LOCAL_USER;
        else
            memcpy(pLoggedOnLocal, G_pLoggedOnLocal, sizeof(XWPLOGGEDON));
    }

    if (fLocked)
        UnlockLoggedOn();

    return arc;
}
