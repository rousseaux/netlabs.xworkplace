
/*
 *@@sourcefile userdb.c:
 *      implements the user database for XWorkplace Security
 *      (XWPSec).
 *
 *      As explained in xwpshell.c, the user database is
 *      a black box. This has the following consequences:
 *
 *      -- The implementation can be completely replaced as
 *         long as the implementation is capable of creating
 *         XWPGROUPDBENTRY and XWPUSERDBENTRY entries,
 *         which represent entries in the database.
 *         For example, this implementation could be replaced
 *         with a version which parses a server database (e.g.
 *         using the Net* API's).
 *
 *      -- The XWPSec kernel knows nothing about passwords.
 *         It is up to the UserDB implementation to store
 *         passwords as well as encrypt and decrypt them.
 *         These are private to the UserDB and not seen
 *         externally. If password encryption is desired,
 *         it must be implemented here.
 *
 *      However, the following definitions must be met by
 *      this implementation:
 *
 *      -- A unique user ID must be assigned to each user name.
 *         XWPSec operates on user IDs instead of user names
 *         to identify users. So user IDs must be persistent
 *         between reboots.
 *
 *      -- Same for group names and group IDs.
 *
 *      -- Each user can belong to no group, one group, or
 *         several groups.
 *
 *      -- User ID 0 (zero) is special and reserved for the
 *         the superuser, who is granted full access. The
 *         user name for the superuser can be freely assigned
 *         (i.e. doesn't have to be "root").
 *
 *      -- Group ID 0 (zero) is special and reserved for
 *         the administrators group, who have some of the
 *         privileges of the superuser. The group name
 *         can be freely assigned (i.e. doesn't have to
 *         be "admin").
 *
 *      An XWP Security Database (UserDB) must implement the
 *      following functions:
 *
 *      --  sudbInit
 *
 *      --  sudbAuthenticateUser
 *
 *      --  sudbQueryName
 *
 *      --  sudbQueryID
 *
 *      --  sudbCreateUser
 *
 *      --  sudbDeleteUser
 *
 *      --  sudbCreateGroup
 *
 *      --  sudbDeleteGroup
 *
 *      These functions get called by the rest of XWPSec for
 *      user management.
 *
 *      This implementation stores users and groups in an XML
 *      file called xwpusers.xml.
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
 *   Declarations
 *
 ********************************************************************/

/*
 *@@ XWPUSERDB:
 *      the user database.
 *      This contains two linked lists with user
 *      and group definitions.
 */

typedef struct _XWPUSERDB
{
    LINKLIST    llGroups;       // list of XWPGROUPDBENTRY items
    LINKLIST    llUsers;        // list of XWPUSERDBENTRY items
} XWPUSERDB, *PXWPUSERDB;

/* ******************************************************************
 *                                                                  *
 *   Global variables                                               *
 *                                                                  *
 ********************************************************************/

HMTX        G_hmtxUserDB = NULLHANDLE;
    // mutex semaphore protecting global data

/* ******************************************************************
 *
 *   Initialization
 *
 ********************************************************************/

/*
 *@@ sudbInit:
 *      initializes XWPSecurity.
 */

APIRET sudbInit(VOID)
{
    APIRET arc = NO_ERROR;

    if (G_hmtxUserDB == NULLHANDLE)
    {
        // first call:
        arc = DosCreateMutexSem(NULL,       // unnamed
                                &G_hmtxUserDB,
                                0,          // unshared
                                FALSE);     // unowned
        /* if (arc == NO_ERROR)
            lstInit(&G_llSubjectInfos, TRUE); */
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
 *@@ LockUserDB:
 *      locks the global security data by requesting
 *      its mutex.
 *
 *      Always call UnlockUserDB() when you're done.
 */

APIRET LockUserDB(VOID)
{
    APIRET arc = NO_ERROR;

    arc = DosRequestMutexSem(G_hmtxUserDB,
                             SEM_INDEFINITE_WAIT);

    return (arc);
}

/*
 *@@ UnlockUserDB:
 *      unlocks the global security data.
 */

APIRET UnlockUserDB(VOID)
{
    return (DosReleaseMutexSem(G_hmtxUserDB));
}

/*
 *@@ GrabString:
 *
 */

BOOL GrabString(const char *pcszBuf,        // in: buf to search
                const char *pcszTag,        // in: tag to search for
                PSZ pszResult,              // out: result
                ULONG cbResult)             // in: sizeof (*pszResult)
{
    BOOL    brc = FALSE;
    ULONG   ulOfs = 0;
    PSZ     pszTemp = 0;

    if (0 != (pszTemp = strhGetTextAttr(pcszBuf,
                                        pcszTag,
                                        &ulOfs)))
    {
        // got user name:
        // copy
        strhncpy0(pszResult,
                  pszTemp,
                  cbResult);
        free(pszTemp);
        brc = TRUE;
    }

    return (brc);
}

/*
 *@@ LoadDatabase:
 *      this function must load the XWorkplace users
 *      database.
 *
 *      This must fill the specified LINKLIST's with
 *      XWPUSERDBENTRY's for each user in the database
 *      and XWPGROUPDBENTRY's for each group in the
 *      database.
 *
 *      This implementation parses ?:\os2\xwpusers.xml,
 *      which has a special XML-compliant format.
 *
 *      Preconditions: This function need not be
 *      reentrant. Proper serialization is taken
 *      care of by users.c.
 */

APIRET LoadDatabase(PVOID pDatabase)
{
    APIRET      arc = NO_ERROR;

    PXWPUSERDB  pDB = (PXWPUSERDB)pDatabase;

    CHAR    szUserDB[CCHMAXPATH];
    PSZ     pszUserDB = NULL,
            pszDBPath = NULL;
    CHAR    szDBPath[CCHMAXPATH];

    lstInit(&pDB->llGroups, FALSE);
    lstInit(&pDB->llUsers, FALSE);

    pszDBPath = getenv("XWPUSERDB");
    if (!pszDBPath)
    {
        // XWPUSERDB not specified:
        // default to "?:\os2" on boot drive
        sprintf(szDBPath, "%c:\\OS2", doshQueryBootDrive());
        pszDBPath = szDBPath;
    }
    sprintf(szUserDB, "%s\\xwpusers.xml", pszDBPath);

    arc = doshReadTextFile(szUserDB,
                           &pszUserDB);
    if (arc == NO_ERROR)
    {
        // text file loaded:

        // now go parse groups...

        // current search pointer
        ULONG   ulSearchOffset = 0,
                ulGroupBlockOfs = 0,
                ulGroupAttrOfs = 0;
        PSZ     pszGroupBlock = 0,
                pszGroupAttrs = 0;
        while (0 == strhGetBlock(pszUserDB,
                                 &ulSearchOffset,
                                 "GROUP",
                                 &pszGroupBlock,
                                 &pszGroupAttrs,
                                 &ulGroupBlockOfs,
                                 &ulGroupAttrOfs))
        {
            // another GROUP found:

            if ((!pszGroupAttrs) || (!pszGroupBlock))
                arc = XWPSEC_DB_GROUP_SYNTAX;
            else
            {
                PXWPGROUPDBENTRY pNewGroup = malloc(sizeof(XWPGROUPDBENTRY));
                if (!pNewGroup)
                    arc = ERROR_NOT_ENOUGH_MEMORY;
                else
                {
                    // get GROUPID attribute and store it
                    // in new group
                    if (!strhGetNumAttribValue(pszGroupAttrs,
                                               "GROUPID",
                                               (PLONG)&pNewGroup->gid))
                        arc = XWPSEC_DB_GROUP_SYNTAX;
                    else
                    {
                        // get body between GROUP tags (group name)
                        strhncpy0(pNewGroup->szGroupName,
                                  pszGroupBlock,
                                  sizeof(pNewGroup->szGroupName));

                        // add to database
                        lstAppendItem(&pDB->llGroups, pNewGroup);
                    }
                }

                if (arc != NO_ERROR)
                    free(pNewGroup);
            }

            if (pszGroupBlock)
                free(pszGroupBlock);
            if (pszGroupAttrs)
                free(pszGroupAttrs);

            if (arc != NO_ERROR)
                break;

        } // end while

        if (arc == NO_ERROR)
        {
            // no error parsing groups:

            // now go parse users...

            ULONG   ulUserBlockOfs = 0,
                    ulUserAttrOfs = 0;
            PSZ     pszUserBlock = 0,
                    pszUserAttrs = 0;

            // reset current search pointer
            ulSearchOffset = 0;

            while (0 == strhGetBlock(pszUserDB,
                                     &ulSearchOffset,
                                     "USER",
                                     &pszUserBlock,
                                     &pszUserAttrs,
                                     &ulUserBlockOfs,
                                     &ulUserAttrOfs))
            {
                // another USER found:

                if ((!pszUserAttrs) || (!pszUserBlock))
                    arc = XWPSEC_DB_USER_SYNTAX;
                else
                {
                    PXWPUSERDBENTRY pNewUser = malloc(sizeof(XWPUSERDBENTRY));
                    if (!pNewUser)
                        arc = ERROR_NOT_ENOUGH_MEMORY;
                    else
                    {
                        // get GROUPID attribute and store it with new user
                        if (    (!strhGetNumAttribValue(pszUserAttrs,
                                                        "USERID",
                                                        (PLONG)&pNewUser->uid))
                        // get USERID attribute and store it with new user
                             || (!strhGetNumAttribValue(pszUserAttrs,
                                                        "GROUPID",
                                                        (PLONG)&pNewUser->gid))
                           )
                            arc = XWPSEC_DB_USER_SYNTAX;

                        if (    (!GrabString(pszUserAttrs,
                                             "NAME",     // user name
                                             pNewUser->szUserName,
                                             sizeof(pNewUser->szUserName)))
                             || (!GrabString(pszUserAttrs,
                                             "PASS",     // password
                                             pNewUser->szPassword,
                                             sizeof(pNewUser->szPassword)))
                            )
                            arc = XWPSEC_DB_USER_SYNTAX;

                        // get body between GROUP tags (user name)
                        /* strhncpy0(pNewUser->szUserName,
                                  pszUserBlock,
                                  sizeof(pNewUser->szUserName)); */

                        if (arc == NO_ERROR)
                            // add to database
                            lstAppendItem(&pDB->llUsers, pNewUser);
                    }

                    if (arc != NO_ERROR)
                        free(pNewUser);
                }

                if (pszUserBlock)
                    free(pszUserBlock);
                if (pszUserAttrs)
                    free(pszUserAttrs);

                if (arc != NO_ERROR)
                    break;
            } // end while
        }

        free(pszUserDB);
    }

    return (arc);
}

/*
 *@@ UnloadDatabase:
 *          #### todo!!
 */

APIRET UnloadDatabase(PVOID pDatabase)
{
    APIRET arc = NO_ERROR;

    return (arc);
}

/*
 *@@ FindUserFromName:
 *
 *
 *      Private function.
 *
 *      You must load the users first using secLoadUsers.
 *      You must also call LockUserDB() yourself.
 */

PCXWPUSERDBENTRY FindUserFromName(PLINKLIST pllUsers,
                                  const char *pcszUserName)
{
    PCXWPUSERDBENTRY p = 0;

    PLISTNODE pNode = lstQueryFirstNode(pllUsers);
    while (pNode)
    {
        PCXWPUSERDBENTRY pUserThis = (PCXWPUSERDBENTRY)pNode->pItemData;
        if (strcmp(pUserThis->szUserName, pcszUserName) == 0)
        {
            // found:
            p = pUserThis;
            break;
        }

        pNode = pNode->pNext;
    }

    return (p);
}

/*
 *@@ FindGroupFromID:
 *
 *
 *      Private function.
 *
 *      You must call LockUserDB() yourself.
 */

PCXWPGROUPDBENTRY FindGroupFromID(PLINKLIST pllGroups,
                                  XWPSECID gid)
{
    PCXWPGROUPDBENTRY p = 0;

    PLISTNODE pNode = lstQueryFirstNode(pllGroups);
    while (pNode)
    {
        PCXWPGROUPDBENTRY pGroupThis = (PCXWPGROUPDBENTRY)pNode->pItemData;
        if (pGroupThis->gid == gid)
        {
            // found:
            p = pGroupThis;
            break;
        }

        pNode = pNode->pNext;
    }

    return (p);
}

/* ******************************************************************
 *
 *   Public Interfaces
 *
 ********************************************************************/

/*
 *@@ sudbAuthenticateUser:
 *      authenticates the user specified by pUserInfo.
 *      This gets called during logon.
 *
 *      On input, specify XWPUSERDBENTRY.szUserName and
 *      XWPUSERDBENTRY.szPassword, as entered by the
 *      user.
 *
 *      If XWPUSERDBENTRY.szUserName exists in the database
 *      and  XWPUSERDBENTRY.szPassword is correct, this
 *      returns NO_ERROR. In that case, the XWPUSERDBENTRY
 *      and XWPGROUPDBENTRY structures are updated to
 *      contain the user and group info, which can then be
 *      used to create the subject handles.
 *
 *      Otherwise XWPSEC_NOT_AUTHENTICATED is
 *      returned, and the structures are not updated.
 */

APIRET sudbAuthenticateUser(PXWPUSERDBENTRY pUserInfo,   // in/out: user info
                            PXWPGROUPDBENTRY pGroupInfo) // out: group info
{
    APIRET arc = NO_ERROR;

    BOOL fLocked = (LockUserDB() == NO_ERROR);
    if (!fLocked)
        arc = XWPSEC_CANNOT_GET_MUTEX;
    else
    {
        XWPUSERDB   Database;

        arc = LoadDatabase(&Database);

        if (arc == NO_ERROR)
        {
            PCXWPUSERDBENTRY pUserFound = FindUserFromName(&Database.llUsers,
                                                           pUserInfo->szUserName);
            if (!pUserFound)
                arc = XWPSEC_NOT_AUTHENTICATED;
            else
            {
                // user exists:
                if (strcmp(pUserInfo->szPassword,
                           pUserFound->szPassword)
                        != 0)
                    arc = XWPSEC_NOT_AUTHENTICATED;
                else
                {
                    // password correct also:

                    // find group
                    PCXWPGROUPDBENTRY pGroup = FindGroupFromID(&Database.llGroups,
                                                               pUserFound->gid);
                    if (!pGroup)
                        arc = XWPSEC_DB_INVALID_GROUPID;
                    else
                    {
                        // store user's uid, gid, group name
                        pUserInfo->uid = pUserFound->uid;
                        pUserInfo->gid = pUserFound->gid;
                        memcpy(pGroupInfo, pGroup, sizeof(XWPGROUPDBENTRY));
                    }
                }
            }

            UnloadDatabase(&Database);
        }
    }

    if (fLocked)
        UnlockUserDB();

    return (arc);
}

/*
 *@@ sudbCreateUser:
 *      creates a new user in the user database.
 *
 *      On input, XWPUSERDBENTRY.UserID is ignored.
 *      However, you must specify all other fields.
 *
 *      This does not create a subject handle for the
 *      user. However, to create a subject handle, the
 *      user must be in the database.
 *
 *      Required authority: Process must be running
 *      on behalf of a user of "admin" group (1).
 */

APIRET sudbCreateUser(PXWPUSERDBENTRY pUserInfo)
{
    APIRET arc = NO_ERROR;

    BOOL fLocked = (LockUserDB() == NO_ERROR);
    if (!fLocked)
        arc = XWPSEC_CANNOT_GET_MUTEX;
    else
    {
        XWPUSERDB   Database;

        arc = LoadDatabase(&Database);

        if (arc == NO_ERROR)
        {
            PCXWPUSERDBENTRY pUserFound = FindUserFromName(&Database.llUsers,
                                                           pUserInfo->szUserName);
            if (pUserFound)
                // user exists: fail
                arc = XWPSEC_USER_EXISTS;
            else
            {
                // ###
                arc = XWPSEC_INTEGRITY;
            }

            UnloadDatabase(&Database);
        }
    }

    if (fLocked)
        UnlockUserDB();

    return (arc);
}


