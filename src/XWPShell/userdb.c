
/*
 *@@sourcefile userdb.c:
 *      implements the user database for XWorkplace Security
 *      (XWPSec), which contains all users and groups which
 *      have been defined for the system together with their
 *      passwords and authority flags.
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
 *         As a result, only the UserDB can authenticate
 *         users (match login name against a password).
 *         See sudbAuthenticateUser.
 *
 *      The following definitions must be met by this
 *      implementation:
 *
 *      -- A unique user ID (uid) must be assigned to each user
 *         name. For speed, XWPSec operates on user IDs instead
 *         of user names to identify users. So user IDs must be
 *         persistent between reboots. It is the responsibility
 *         of the UserDB to associate user names with user IDs.
 *
 *      -- Same for group names and group IDs (gid's).
 *
 *      -- Each user can belong to no group, one group, or
 *         several groups.
 *
 *      -- User ID 0 (zero) is special and reserved for the
 *         the superuser, who is granted full access. The
 *         user name for the superuser can be freely assigned
 *         (i.e. doesn't have to be "root"). XWPSec completely
 *         disables access control for uid 0.
 *
 *      -- Group ID 0 (zero) is special and reserved for
 *         the super user as well. No other user can be
 *         a member of this group.
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

#include "expat\expat.h"                // must come before xml.h
#include "helpers\tree.h"
#include "helpers\xstring.h"
#include "helpers\xml.h"

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
 *
 *   Global variables
 *
 ********************************************************************/

HMTX        G_hmtxUserDB = NULLHANDLE;
    // mutex semaphore protecting global data

XWPUSERDB   G_Database;

/* ******************************************************************
 *
 *   Private helpers
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

    return arc;
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

/*
 *@@ CreateGroup:
 *      creates a new XWPGROUPDBENTRY from the
 *      specified XML DOM node and appends it to
 *      the database.
 *
 *      Preconditions:
 *
 *      --  Caller must hold the db mutex.
 *
 *      Returns:
 *
 *      --  NO_ERROR: group was added to pDB->llGroups.
 *
 *      --  ERROR_NOT_ENOUGH_MEMORY
 *
 *      --  XWPSEC_DB_GROUP_SYNTAX: syntax error in
 *          group's XML element.
 *
 *@@added V0.9.11 (2001-04-22) [umoeller]
 */

APIRET CreateGroup(PXWPUSERDB pDB,
                   PDOMNODE pGroupElementThis)
{
    APIRET arc = NO_ERROR;

    PXWPGROUPDBENTRY pNewGroup = malloc(sizeof(XWPGROUPDBENTRY));
    if (!pNewGroup)
        arc = ERROR_NOT_ENOUGH_MEMORY;
    else
    {
        const XSTRING *pstrGroupID = xmlGetAttribute(pGroupElementThis,
                                                     "GROUPID");
        if (!pstrGroupID)
            arc = XWPSEC_DB_GROUP_SYNTAX;
        else
        {
            PDOMNODE pCDATA;

            pNewGroup->gid = atoi(pstrGroupID->psz);

            // get group name (GROUP element character data)
            if (!(pCDATA = xmlGetFirstText(pGroupElementThis)))
                arc = XWPSEC_DB_GROUP_SYNTAX;
            else
            {
                strhncpy0(pNewGroup->szGroupName,
                          pCDATA->pstrNodeValue->psz,
                          sizeof(pNewGroup->szGroupName));

                _PmpfF(("created group \"%s\"", pNewGroup->szGroupName));

                // add to database
                lstAppendItem(&pDB->llGroups, pNewGroup);
            }
        }

        if (arc)
            free(pNewGroup);
    }

    _PmpfF(("returning %d", arc));

    return arc;
}

/*
 *@@ CreateUser:
 *      creates a new XWPUSERDBENTRY from the
 *      specified XML DOM node and appends it to
 *      the database.
 *
 *      Preconditions:
 *
 *      --  The groups must already have been loaded.
 *
 *      --  Caller must hold the db mutex.
 *
 *      Returns:
 *
 *      --  NO_ERROR: user was added to pDB->llUsers.
 *
 *      --  ERROR_NOT_ENOUGH_MEMORY
 *
 *      --  XWPSEC_DB_USER_SYNTAX: syntax error in
 *          user's XML element.
 *
 *      --  XWPSEC_INVALID_GROUPID: cannot find group
 *          for the user's group ID.
 *
 *@@added V0.9.11 (2001-04-22) [umoeller]
 */

APIRET CreateUser(PXWPUSERDB pDB,
                  PDOMNODE pUserElementThis)
{
    APIRET arc = NO_ERROR;

    PXWPUSERDBENTRY pNewUser;
    if (!(pNewUser = malloc(sizeof(XWPUSERDBENTRY))))
        arc = ERROR_NOT_ENOUGH_MEMORY;
    else
    {
        const XSTRING *pstrUserID  = xmlGetAttribute(pUserElementThis,
                                                     "USERID");
        const XSTRING *pstrGroupID = xmlGetAttribute(pUserElementThis,
                                                     "GROUPID");
        const XSTRING *pstrName    = xmlGetAttribute(pUserElementThis,
                                                     "NAME");
        const XSTRING *pstrPass    = xmlGetAttribute(pUserElementThis,
                                                     "PASS");
        if (    (!pstrGroupID)
             || (!pstrUserID)
             || (!pstrName)
             || (!pstrPass)
           )
            arc = XWPSEC_DB_USER_SYNTAX;
        else
        {
            PDOMNODE pCDATA;
            PCXWPGROUPDBENTRY pGroupEntry;

            pNewUser->uid = atoi(pstrUserID->psz);
            pNewUser->gid = atoi(pstrGroupID->psz);

            strhncpy0(pNewUser->szUserName,
                      pstrName->psz,
                      sizeof(pNewUser->szUserName));

            strhncpy0(pNewUser->szPassword,
                      pstrPass->psz,
                      sizeof(pNewUser->szPassword));

            if (!(pGroupEntry = FindGroupFromID(&pDB->llGroups,
                                                pNewUser->gid)))
                // cannot find group for this user:
                arc = XWPSEC_INVALID_GROUPID;
            else
            {
                // copy group name
                memcpy(pNewUser->szGroupName,
                       pGroupEntry->szGroupName,
                       sizeof(pNewUser->szGroupName));

                // get long user name (USER element character data)
                if (!(pCDATA = xmlGetFirstText(pUserElementThis)))
                    arc = XWPSEC_DB_GROUP_SYNTAX;
                else
                {
                    strhncpy0(pNewUser->szFullName,
                              pCDATA->pstrNodeValue->psz,
                              sizeof(pNewUser->szFullName));

                    _PmpfF(("created user \"%s\", pass \"%s\"",
                                pNewUser->szUserName,
                                pNewUser->szPassword));

                    // add to database
                    lstAppendItem(&pDB->llUsers, pNewUser);
                }
            }
        }

        if (arc)
            free(pNewUser);
    }

    _PmpfF(("returning %d", arc));

    return arc;
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
 *      This function gets called every single time
 *      something is needed from the user database.
 *
 *      Preconditions: Caller must hold the UserDB
 *      mutex.
 *
 *      Returns:
 *
 *      --  NO_ERROR
 *
 *      --  XWPSEC_DB_USER_SYNTAX: syntax error in
 *          user fields.
 *
 *      --  XWPSEC_DB_GROUP_SYNTAX: syntax error in
 *          group fields.
 *
 *      plus the many error codes from doshLoadTextFile,
 *      xmlCreateDOM, xmlParse,
 */

APIRET LoadDB(VOID)
{
    APIRET      arc = NO_ERROR;

    CHAR    szUserDB[CCHMAXPATH];
    PSZ     pszUserDB = NULL,
            pszDBPath = NULL;
    CHAR    szDBPath[CCHMAXPATH];

    _PmpfF(("entering"));

    lstInit(&G_Database.llGroups, TRUE);      // auto-free
    lstInit(&G_Database.llUsers, TRUE);       // auto-free

    if (!(pszDBPath = getenv("XWPUSERDB")))
    {
        // XWPUSERDB not specified:
        // default to "?:\os2" on boot drive
        sprintf(szDBPath, "%c:\\OS2", doshQueryBootDrive());
        pszDBPath = szDBPath;
    }
    sprintf(szUserDB, "%s\\xwpusers.xml", pszDBPath);

    if (!(arc = doshLoadTextFile(szUserDB,
                                 &pszUserDB,
                                 NULL)))
    {
        // text file loaded:

        // create the DOM
        PXMLDOM pDom = NULL;
        if (!(arc = xmlCreateDOM(0,             // no validation
                                 NULL,
                                 NULL,
                                 NULL,
                                 &pDom)))
        {
            if (!(arc = xmlParse(pDom,
                                 pszUserDB,
                                 strlen(pszUserDB),
                                 TRUE)))    // last chunk (we only have one)
            {
                // OK, now we got all the data in the DOMNODEs:

                // 1) get the root element
                PDOMNODE pRootElement;
                if (pRootElement = xmlGetRootElement(pDom))
                {
                    // 2) get all GROUP elements
                    PLINKLIST pllGroups;

                    _PmpfF(("loading groups"));

                    if (pllGroups = xmlGetElementsByTagName(pRootElement,
                                                            "GROUP"))
                    {
                        // copy all groups to our private data
                        PLISTNODE pGroupNode;

                        _PmpfF(("got %d groups", lstCountItems(pllGroups)));

                        for (pGroupNode = lstQueryFirstNode(pllGroups);
                             (pGroupNode) && (!arc);
                             pGroupNode = pGroupNode->pNext)
                        {
                            PDOMNODE pGroupElementThis = (PDOMNODE)pGroupNode->pItemData;
                            arc = CreateGroup(&G_Database,
                                              pGroupElementThis);
                        }

                        lstFree(&pllGroups);

                        _PmpfF(("loading users"));

                        if (!arc)
                        {
                            // 3) get all USER elements
                            PLINKLIST pllUsers;
                            if (pllUsers = xmlGetElementsByTagName(pRootElement,
                                                                   "USER"))
                            {
                                // copy all users to our private data
                                PLISTNODE pUserNode;

                                _PmpfF(("got %d users", lstCountItems(pllUsers)));

                                for (pUserNode = lstQueryFirstNode(pllUsers);
                                     (pUserNode) && (!arc);
                                     pUserNode = pUserNode->pNext)
                                {
                                    PDOMNODE pUserElementThis = (PDOMNODE)pUserNode->pItemData;
                                    arc = CreateUser(&G_Database,
                                                     pUserElementThis);
                                }

                                lstFree(&pllUsers);
                            }
                            else
                                arc = XWPSEC_DB_USER_SYNTAX;
                        }
                    }
                    else
                        arc = XWPSEC_DB_USER_SYNTAX;
                }
                else
                    arc = XWPSEC_DB_USER_SYNTAX;

                xmlFreeDOM(pDom);
            }
        }

        free(pszUserDB);
    }

    _PmpfF(("returning %d", arc));

    return arc;
}

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
        if (!(arc = DosCreateMutexSem(NULL,       // unnamed
                                      &G_hmtxUserDB,
                                      0,          // unshared
                                      TRUE)))       // request now
        {
            arc = LoadDB();

            DosReleaseMutexSem(G_hmtxUserDB);
        }
    }
    else
        arc = XWPSEC_INSUFFICIENT_AUTHORITY;

    return arc;
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
 *      user. All other fields are ignored.
 *
 *      If XWPUSERDBENTRY.szUserName exists in the database
 *      and  XWPUSERDBENTRY.szPassword is correct, this
 *      returns NO_ERROR. In that case, the XWPUSERDBENTRY
 *      and XWPGROUPDBENTRY structures are updated to
 *      contain the user and group info, which XWPSec
 *      will then use to create the subject handles.
 *
 *      Otherwise this returns:
 *
 *      --  XWPSEC_CANNOT_GET_MUTEX:
 *
 *      --  XWPSEC_NOT_AUTHENTICATED: pUserInfo->szUserName
 *          is unknown, or pUserInfo->szPassword doesn't
 *          match the entry in the userdb.
 *
 *      --  XWPSEC_DB_INVALID_GROUPID: userdb error, group
 *          ID is invalid.
 */

APIRET sudbAuthenticateUser(PXWPUSERDBENTRY pUserInfo,   // in/out: user info
                            PXWPGROUPDBENTRY pGroupInfo) // out: group info
{
    APIRET arc = NO_ERROR;

    BOOL fLocked = (LockUserDB() == NO_ERROR);

    _PmpfF(("entering"));

    if (!fLocked)
        arc = XWPSEC_CANNOT_GET_MUTEX;
    else
    {
        PCXWPUSERDBENTRY pUserFound;
        if (!(pUserFound = FindUserFromName(&G_Database.llUsers,
                                            pUserInfo->szUserName)))
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
                PCXWPGROUPDBENTRY pGroupFound;
                if (!(pGroupFound = FindGroupFromID(&G_Database.llGroups,
                                                    pUserFound->gid)))
                    arc = XWPSEC_DB_INVALID_GROUPID;
                else
                {
                    // store user's uid, gid, group name
                    memcpy(pUserInfo,
                           pUserFound,
                           sizeof(XWPUSERDBENTRY));
                    memcpy(pGroupInfo,
                           pGroupFound,
                           sizeof(XWPGROUPDBENTRY));
                }
            }
        }
    }

    if (fLocked)
        UnlockUserDB();

    _PmpfF(("leaving"));

    return arc;
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
        PCXWPUSERDBENTRY pUserFound = FindUserFromName(&G_Database.llUsers,
                                                       pUserInfo->szUserName);
        if (pUserFound)
            // user exists: fail
            arc = XWPSEC_USER_EXISTS;
        else
        {
            // @@todo
            arc = XWPSEC_INTEGRITY;
        }
    }

    if (fLocked)
        UnlockUserDB();

    return arc;
}

/*
 *@@ sudbQueryUsers:
 *
 *      Returns:
 *
 *      --  XWPSEC_CANNOT_GET_MUTEX
 *
 *      --  XWPSEC_NO_USERS: no users in database.
 *
 *      --  ERROR_INVALID_PARAMETER
 *
 *      --  ERROR_NOT_ENOUGH_MEMORY
 *
 *      plus the many error codes from LoadDatabase.
 *
 *@@added V0.9.19 (2002-04-02) [umoeller]
 */

APIRET sudbQueryUsers(PULONG pcUsers,               // out: user count
                      PXWPUSERDBENTRY *ppaUsers)    // out: array of users (shared memory)
{
    APIRET arc = NO_ERROR;
    BOOL fLocked;

    if (!pcUsers || !ppaUsers)
        return ERROR_INVALID_PARAMETER;

    fLocked = (LockUserDB() == NO_ERROR);

    _PmpfF(("entering"));

    if (!fLocked)
        arc = XWPSEC_CANNOT_GET_MUTEX;
    else
    {
        if (!(*pcUsers = lstCountItems(&G_Database.llUsers)))
            arc = XWPSEC_NO_USERS;
        else
        {
            PXWPUSERDBENTRY paUsers;
            if (!(arc = DosAllocSharedMem((PVOID*)&paUsers,
                                          NULL,
                                          sizeof(XWPUSERDBENTRY) * *pcUsers,
                                          PAG_COMMIT | OBJ_GIVEABLE | PAG_READ | PAG_WRITE)))
            {
                PLISTNODE pNode = lstQueryFirstNode(&G_Database.llUsers);
                PXWPUSERDBENTRY pTargetThis = paUsers;
                while (pNode)
                {
                    PCXWPUSERDBENTRY pSourceThis = (PCXWPUSERDBENTRY)pNode->pItemData;

                    pTargetThis->uid = pSourceThis->uid;
                    memcpy(pTargetThis->szUserName,
                           pSourceThis->szUserName,
                           sizeof(pTargetThis->szUserName));
                    memcpy(pTargetThis->szFullName,
                           pSourceThis->szFullName,
                           sizeof(pTargetThis->szFullName));
                    // never give out the passwords
                    memset(pTargetThis->szPassword,
                           0,
                           sizeof(pTargetThis->szPassword));

                    pTargetThis->gid = pSourceThis->gid;
                    memcpy(pTargetThis->szGroupName,
                           pSourceThis->szGroupName,
                           sizeof(pTargetThis->szGroupName));

                    pNode = pNode->pNext;
                    pTargetThis++;
                }

                *ppaUsers = paUsers;
            }
        }
    }

    if (fLocked)
        UnlockUserDB();

    _PmpfF(("leaving"));

    return arc;
}

/*
 *@@ sudbQueryGroups:
 *
 *      Returns:
 *
 *      --  XWPSEC_CANNOT_GET_MUTEX
 *
 *      --  XWPSEC_NO_GROUPS: no groups in database.
 *
 *      --  ERROR_INVALID_PARAMETER
 *
 *      --  ERROR_NOT_ENOUGH_MEMORY
 *
 *      plus the many error codes from LoadDatabase.
 *
 *@@added V0.9.19 (2002-04-02) [umoeller]
 */

APIRET sudbQueryGroups(PULONG pcGroups,               // out: user count
                       PXWPGROUPDBENTRY *ppaGroups)    // out: array of users (shared memory)
{
    APIRET arc = NO_ERROR;
    BOOL fLocked;

    if (!pcGroups || !ppaGroups)
        return ERROR_INVALID_PARAMETER;

    fLocked = (LockUserDB() == NO_ERROR);

    _PmpfF(("entering"));

    if (!fLocked)
        arc = XWPSEC_CANNOT_GET_MUTEX;
    else
    {
        if (!(*pcGroups = lstCountItems(&G_Database.llGroups)))
            arc = XWPSEC_NO_GROUPS;
        else
        {
            PXWPGROUPDBENTRY paGroups;
            if (!(arc = DosAllocSharedMem((PVOID*)&paGroups,
                                          NULL,
                                          sizeof(XWPGROUPDBENTRY) * *pcGroups,
                                          PAG_COMMIT | OBJ_GIVEABLE | PAG_READ | PAG_WRITE)))
            {
                PLISTNODE pNode = lstQueryFirstNode(&G_Database.llGroups);
                PXWPGROUPDBENTRY pTargetThis = paGroups;
                while (pNode)
                {
                    PCXWPGROUPDBENTRY pSourceThis = (PCXWPGROUPDBENTRY)pNode->pItemData;

                    pTargetThis->gid = pSourceThis->gid;
                    memcpy(pTargetThis->szGroupName,
                           pSourceThis->szGroupName,
                           sizeof(pTargetThis->szGroupName));

                    pNode = pNode->pNext;
                    pTargetThis++;
                }

                *ppaGroups = paGroups;
            }
        }
    }

    if (fLocked)
        UnlockUserDB();

    _PmpfF(("leaving"));

    return arc;
}

