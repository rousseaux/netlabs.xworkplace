
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
        if (!(arc = DosCreateMutexSem(NULL,       // unnamed
                                      &G_hmtxLoggedOn,
                                      0,          // unshared
                                      FALSE)))    // unowned
            lstInit(&G_llLoggedOn, FALSE);
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

BOOL LockLoggedOn(VOID)
{
    return !DosRequestMutexSem(G_hmtxLoggedOn,
                               SEM_INDEFINITE_WAIT);
}

/*
 *@@ UnlockLoggedOn:
 *      unlocks the global security data.
 */

VOID UnlockLoggedOn(VOID)
{
    DosReleaseMutexSem(G_hmtxLoggedOn);
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

const XWPLOGGEDON* FindLoggedOnFromID(XWPSECID uid)
{
    PLISTNODE pNode = lstQueryFirstNode(&G_llLoggedOn);
    while (pNode)
    {
        const XWPLOGGEDON *plo = (const XWPLOGGEDON *)pNode->pItemData;

        if (plo->uid == uid)
            return plo;

        pNode = pNode->pNext;
    }

    return NULL;
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
 *      It is assumed that the struct has been malloc'd
 *      by the caller. No copy is made, and the caller
 *      must not free the struct if NO_ERROR is returned.
 *
 *      Returns:
 *
 *      -- NO_ERROR: user was stored.
 *
 *      -- XWPSEC_USER_EXISTS: a user with this uid
 *         was already stored in the list.
 */

APIRET RegisterLoggedOn(PXWPLOGGEDON pNewUser,
                        BOOL fLocal)            // in: if TRUE, mark user as local
{
    APIRET  arc = NO_ERROR;
    BOOL    fLocked;

    if (!(fLocked = LockLoggedOn()))
        arc = XWPSEC_CANNOT_GET_MUTEX;
    else
    {
        if (FindLoggedOnFromID(pNewUser->uid))
            // error:
            arc = XWPSEC_USER_EXISTS;
        else
        {
            if (!lstAppendItem(&G_llLoggedOn, pNewUser))
                arc = XWPSEC_INTEGRITY;
            else
                if (fLocal)
                    G_pLoggedOnLocal = pNewUser;
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
 *      On input, specify the user's ID with uid.
 *
 *      If NO_ERROR is returned, the user has been
 *      unlinked. The pointer to the logon struct
 *      that was removed from the list is returned
 *      with *ppLogon and must be freed by the caller.
 *
 *      This does not delete subject handles. These
 *      must be deleted after calling this function.
 */

APIRET DeregisterLoggedOn(XWPSECID uid,
                          PXWPLOGGEDON *ppLogon)        // out: logon struct (to be freed by caller)
{
    APIRET  arc = NO_ERROR;
    BOOL    fLocked;

    if (!(fLocked = LockLoggedOn()))
        arc = XWPSEC_CANNOT_GET_MUTEX;
    else
    {
        PXWPLOGGEDON pFound;
        if (!(pFound = (PXWPLOGGEDON)FindLoggedOnFromID(uid)))
            // error:
            arc = XWPSEC_INVALID_USERID;
        else
        {
            // remove from list
            if (!lstRemoveItem(&G_llLoggedOn,       // no auto-free
                               (PVOID)pFound))
                arc = XWPSEC_INTEGRITY;
            else
            {
                // return logon we found
                *ppLogon = pFound;

                if (pFound == G_pLoggedOnLocal)
                    // this was the local user:
                    G_pLoggedOnLocal = NULL;
            }
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
 *      This does not change the security context of
 *      processes or manage the user shell.
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

APIRET slogLogOn(PCSZ pcszUserName,         // in: user name
                 PCSZ pcszPassword,  // in: password
                 BOOL fLocal,
                 XWPSECID *puid)           // out: user ID if NO_ERROR
{
    APIRET arc = NO_ERROR;

    XWPUSERINFO uiLogon;

    _PmpfF(("entering, user \"%s\", pwd \"%s\"",
            pcszUserName,
            pcszPassword));

    strhncpy0(uiLogon.szUserName,
              pcszUserName,
              sizeof(uiLogon.szUserName));

    if (arc = sudbAuthenticateUser(&uiLogon,
                                   pcszPassword))
        DosSleep(3000);
    else
    {
        // get all the groups that this user is a member of
        PXWPUSERDBENTRY  pUserDBEntry;
        if (!(arc = sudbQueryUser(uiLogon.uid,
                                  &pUserDBEntry)))
        {
            // allocate XWPLOGGEDON struct
            PXWPLOGGEDON    pLogon;
            ULONG           cbStruct =   sizeof(XWPLOGGEDON)    // has one subject for user already
                                       + pUserDBEntry->Membership.cGroups * sizeof(HXSUBJECT);
            if (!(pLogon = (PXWPLOGGEDON)malloc(cbStruct)))
                arc = ERROR_NOT_ENOUGH_MEMORY;
            else
            {
                XWPSUBJECTINFO  siUser;
                ULONG           ul;

                pLogon->cbStruct = cbStruct;
                memcpy(pLogon->szUserName,
                       pUserDBEntry->User.szUserName,
                       sizeof(pLogon->szUserName));
                pLogon->uid = uiLogon.uid;
                pLogon->cSubjects = 0;      // for now

                // create user subject (always)
                siUser.hSubject = 0;
                siUser.id = uiLogon.uid;   // returned by sudbAuthenticateUser
                siUser.bType = SUBJ_USER;

                _Pmpf(("  sudbAuthenticateUser returned uid %d", uiLogon.uid));

                if (!(arc = subjCreateSubject(&siUser)))
                {
                    // got user subject:
                    pLogon->aSubjects[0] = siUser.hSubject;
                    ++pLogon->cSubjects;

                    // create subjects for all the groups,
                    // unless this is root
                    if (uiLogon.uid)
                    {
                        // not root:
                        for (ul = 0;
                             ul < pUserDBEntry->Membership.cGroups;
                             ++ul)
                        {
                            XWPSUBJECTINFO siGroup;
                            // create group subject (if it doesn't exist yet)
                            siGroup.hSubject = 0;
                            siGroup.id = pUserDBEntry->Membership.aGIDs[ul];
                            siGroup.bType = SUBJ_GROUP;

                            if (arc = subjCreateSubject(&siGroup))
                                break;

                            pLogon->aSubjects[pLogon->cSubjects++] = siGroup.hSubject;
                        }

                        if (!arc)
                        {
                            // register this user as logged on
                            if (!(arc = RegisterLoggedOn(pLogon,
                                                         fLocal)))
                                // pass out user ID
                                *puid = uiLogon.uid;
                        }
                    }
                    else
                        if (arc)
                            // error: kill the subjects we created
                            for (ul = 0;
                                 ul < pLogon->cSubjects;
                                 ++ul)
                                subjDeleteSubject(pLogon->aSubjects[ul]);
                }

                if (arc)
                    free(pLogon);
            }

            DosFreeMem(pUserDBEntry);
        }
    }

    _PmpfF(("leaving, returning %d", arc));

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

    PXWPLOGGEDON pLogoff;

    if (!(arc = DeregisterLoggedOn(uid,
                                   &pLogoff)))
    {
        ULONG   ul;
        for (ul = 0;
             ul < pLogoff->cSubjects;
             ++ul)
        {
            subjDeleteSubject(pLogoff->aSubjects[ul]);
        }

        free(pLogoff);
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
    APIRET  arc = NO_ERROR;
    BOOL    fLocked;

    if (!(fLocked = LockLoggedOn()))
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
