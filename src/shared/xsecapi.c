
/*
 *@@sourcefile xsecapi.c:
 *
 *@@header "shared\xsecapi.h"
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

#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#define INCL_DOSQUEUES
#define INCL_DOSMODULEMGR
#define INCL_DOSERRORS
#include <os2.h>

// C library headers
#include <stdio.h>

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\standards.h"          // some standard macros

// SOM headers which don't crash with prec. header files

// XWorkplace implementation headers
#include "shared\kernel.h"              // XWorkplace Kernel

#include "security\xwpsecty.h"          // XWorkplace Security

// other SOM headers
#pragma hdrstop

/* ******************************************************************
 *
 *   Helpers
 *
 ********************************************************************/

/*
 *@@ XWPSHELLCOMMAND:
 *
 *@@added V0.9.11 (2001-04-22) [umoeller]
 */

typedef struct _XWPSHELLCOMMAND
{
    PXWPSHELLQUEUEDATA  pShared;
    PID                 pidXWPShell;
    HQUEUE              hqXWPShell;
} XWPSHELLCOMMAND, *PXWPSHELLCOMMAND;

VOID FreeXWPShellCommand(PXWPSHELLCOMMAND pCommand);

/*
 *@@ CreateXWPShellCommand:
 *      creates a command for XWPShell to process.
 *
 *      See XWPSHELLQUEUEDATA for a description of
 *      what's going on here.
 *
 *      ulCommand must be one of QUECMD_* values.
 *
 *      If this returns NO_ERROR, the caller must
 *      then fill in the shared data according to
 *      what the command requires and use
 *      SendXWPShellCommand then.
 *
 *      Among others, this can return:
 *
 *      --  ERROR_NOT_ENOUGH_MEMORY
 *
 *      --  ERROR_QUE_NAME_NOT_EXIST (343): XWPShell queue
 *          not found, XWPShell is probably not running.
 *
 *@@added V0.9.11 (2001-04-22) [umoeller]
 */

APIRET CreateXWPShellCommand(ULONG ulCommand,               // in: command
                             PXWPSHELLCOMMAND *ppCommand)   // out: cmd structure if NO_ERROR is returned
{
    APIRET      arc = NO_ERROR;

    PXWPSHELLCOMMAND pCommand;
    if (!(pCommand = NEW(XWPSHELLCOMMAND)))
        arc = ERROR_NOT_ENOUGH_MEMORY;
    else
    {
        ZERO(pCommand);

        // check if XWPShell is running; if so the queue must exist
        if (    (!(arc = DosOpenQueue(&pCommand->pidXWPShell,
                                      &pCommand->hqXWPShell,
                                      QUEUE_XWPSHELL)))
             && (!(arc = DosAllocSharedMem((PVOID*)&pCommand->pShared,
                                           NULL,     // unnamed
                                           sizeof(XWPSHELLQUEUEDATA),
                                           PAG_COMMIT | OBJ_GIVEABLE | PAG_READ | PAG_WRITE)))
           )
        {
            PXWPSHELLQUEUEDATA pShared = pCommand->pShared;

            if (    (!(arc = DosGiveSharedMem(pShared,
                                              pCommand->pidXWPShell,
                                              PAG_READ | PAG_WRITE)))
                 && (!(arc = DosCreateEventSem(NULL,
                                               &pShared->hevData,
                                               DC_SEM_SHARED,
                                               FALSE)))      // reset
               )
            {
                pShared->ulCommand = ulCommand;
                pShared->cbStruct = sizeof(XWPSHELLQUEUEDATA);
            }
        }

        if (!arc)
            *ppCommand = pCommand;
        else
            FreeXWPShellCommand(pCommand);
    }

    return arc;
}

/*
 *@@ SendXWPShellCommand:
 *      sends a command to XWPShell and waits for
 *      the command to be processed.
 *
 *      This will wait a maximum of five seconds
 *      and return 640 (ERROR_TIMEOUT) if XWPShell
 *      didn't manage to respond in that time.
 *
 *      In addition to standard OS/2 error codes,
 *      this may return XWPShell error codes
 *      depending on the command that was sent.
 *      See XWPSHELLQUEUEDATA for commands and
 *      their possible return values.
 *
 *      If XWPSEC_QUEUE_INVALID_CMD is returned,
 *      you have specified an invalid command code.
 *
 *      Be warned, this blocks the calling thread.
 *      Even though XWPShell should be following
 *      the PM 0.1 seconds rule, you might want
 *      to start a second thread for this.
 *
 *@@added V0.9.11 (2001-04-22) [umoeller]
 */

APIRET SendXWPShellCommand(PXWPSHELLCOMMAND pCommand)
{
    APIRET arc = NO_ERROR;

    if (!pCommand)
        arc = ERROR_INVALID_PARAMETER;
    else
    {
        if (    (!(arc = DosWriteQueue(pCommand->hqXWPShell,
                                       (ULONG)pCommand->pShared,   // request data
                                       0,
                                       NULL,
                                       0)))              // priority
                // wait 5 seconds for XWPShell to write the data
             && (!(arc = DosWaitEventSem(pCommand->pShared->hevData,
                                         5*1000)))
           )
        {
            // return what XWPShell returns
            arc = pCommand->pShared->arc;
        }
    }

    return arc;
}

/*
 *@@ FreeXWPShellCommand:
 *      cleans up.
 *
 *@@added V0.9.11 (2001-04-22) [umoeller]
 */

VOID FreeXWPShellCommand(PXWPSHELLCOMMAND pCommand)
{
    if (pCommand)
    {
        if (pCommand->pShared)
        {
            if (pCommand->pShared->hevData)
                DosCloseEventSem(pCommand->pShared->hevData);

            DosFreeMem(pCommand->pShared);
        }

        if (pCommand->hqXWPShell)
            DosCloseQueue(pCommand->hqXWPShell);

        free(pCommand);
    }
}

/* ******************************************************************
 *
 *   XWPShell security APIs
 *
 ********************************************************************/

/*
 *@@ xsecQueryLocalLoggedOn:
 *      returns the user who's currently logged
 *      on locally.
 *
 *      Required authority: None.
 *
 *      Among others, this can return:
 *
 *      --  ERROR_NOT_ENOUGH_MEMORY
 *
 *      --  ERROR_QUE_NAME_NOT_EXIST (343): XWPShell queue
 *          not found, XWPShell is probably not running.
 *
 *      --  XWPSEC_NO_LOCAL_USER: no user is currently
 *          logged on locally (XWPShell is probably
 *          displaying logon dialog).
 *
 *@@added V0.9.11 (2001-04-22) [umoeller]
 */

APIRET xsecQueryLocalLoggedOn(PXWPLOGGEDON pLoggedOn)       // out: currently logged on user
{
    APIRET              arc = NO_ERROR;
    PXWPSHELLCOMMAND    pCommand;

    if (!(arc = CreateXWPShellCommand(QUECMD_QUERYLOCALLOGGEDON,
                                      &pCommand)))
    {
        if (!(arc = SendXWPShellCommand(pCommand)))
        {
            // alright:
            // copy output
            memcpy(pLoggedOn,
                   &pCommand->pShared->Data.QueryLocalLoggedOn,      // union member
                   sizeof(XWPLOGGEDON));
        }

        FreeXWPShellCommand(pCommand);
    }

    return arc;
}

/*
 *@@ xsecQueryUsers:
 *      returns an array of XWPUSERDBENTRY structs
 *      with all the users currently in the userdb.
 *
 *      Warning: The array items are variable in
 *      size depending on group memberships, so
 *      always use the cbStruct member of each
 *      array item to climb to the next.
 *
 *      Returns:
 *
 *      --  NO_ERROR
 *
 *      --  ERROR_NOT_ENOUGH_MEMORY
 *
 *      --  XWPSEC_INSUFFICIENT_AUTHORITY: process
 *          owner does not have sufficient authority
 *          for this query.
 *
 *      plus the error codes from sudbQueryUsers.
 *
 *@@added V0.9.19 (2002-04-02) [umoeller]
 */

APIRET xsecQueryUsers(PULONG pcUsers,               // ou: array item count
                      PXWPUSERDBENTRY *ppaUsers)    // out: array of users (to be freed)
{
    APIRET              arc = NO_ERROR;
    PXWPSHELLCOMMAND    pCommand;

    if (!(arc = CreateXWPShellCommand(QUECMD_QUERYUSERS,
                                      &pCommand)))
    {
        if (!(arc = SendXWPShellCommand(pCommand)))
        {
            // alright:
            // the array items are variable in size, so check
            ULONG   ul,
                    cbTotal = 0;
            PBYTE   pbUserThis = (PBYTE)pCommand->pShared->Data.QueryUsers.paUsers;
            PXWPUSERDBENTRY paUsers;
            for (ul = 0;
                 ul < pCommand->pShared->Data.QueryUsers.cUsers;
                 ++ul)
            {
                ULONG cbThis = ((PXWPUSERDBENTRY)pbUserThis)->cbStruct;
                cbTotal += cbThis;
                pbUserThis += cbThis;
            }

            if (!(paUsers = malloc(cbTotal)))
                arc = ERROR_NOT_ENOUGH_MEMORY;
            else
            {
                memcpy(paUsers,
                       pCommand->pShared->Data.QueryUsers.paUsers,
                       cbTotal);
                *pcUsers = pCommand->pShared->Data.QueryUsers.cUsers;
                *ppaUsers = paUsers;
            }

            // free shared mem given to us by XWPShell
            DosFreeMem(pCommand->pShared->Data.QueryUsers.paUsers);
        }

        FreeXWPShellCommand(pCommand);
    }

    return arc;
}

/*
 *@@ xsecQueryGroups:
 *      returns an array of XWPGROUPDBENTRY structs
 *      with all the groups currently in the userdb.
 *
 *      Returns:
 *
 *      --  NO_ERROR
 *
 *      --  ERROR_NOT_ENOUGH_MEMORY
 *
 *      --  XWPSEC_INSUFFICIENT_AUTHORITY: process
 *          owner does not have sufficient authority
 *          for this query.
 *
 *      plus the error codes from sudbQueryGroups.
 *
 *@@added V0.9.19 (2002-04-02) [umoeller]
 */

APIRET xsecQueryGroups(PULONG pcGroups,
                       PXWPGROUPDBENTRY *ppaGroups)    // out: array of users (to be freed)
{
    APIRET              arc = NO_ERROR;
    PXWPSHELLCOMMAND    pCommand;

    if (!(arc = CreateXWPShellCommand(QUECMD_QUERYGROUPS,
                                      &pCommand)))
    {
        if (!(arc = SendXWPShellCommand(pCommand)))
        {
            // alright:
            PXWPGROUPDBENTRY paGroups;
            ULONG cb =   pCommand->pShared->Data.QueryGroups.cGroups
                       * sizeof(XWPGROUPDBENTRY);

            if (!(paGroups = malloc(cb)))
                arc = ERROR_NOT_ENOUGH_MEMORY;
            else
            {
                memcpy(paGroups,
                       pCommand->pShared->Data.QueryGroups.paGroups,
                       cb);
                *pcGroups = pCommand->pShared->Data.QueryGroups.cGroups;
                *ppaGroups = paGroups;
            }

            // free shared mem given to us by XWPShell
            DosFreeMem(pCommand->pShared->Data.QueryGroups.paGroups);
        }

        FreeXWPShellCommand(pCommand);
    }

    return arc;
}

/*
 *@@ xsecQueryProcessOwner:
 *      returns the UID on whose behalf the given
 *      process is running.
 *
 *      Returns:
 *
 *      --  NO_ERROR
 *
 *      --  ERROR_NOT_ENOUGH_MEMORY
 *
 *      --  XWPSEC_INSUFFICIENT_AUTHORITY: process
 *          owner does not have sufficient authority
 *          for this query.
 *
 *      plus the error codes from subjQuerySubjectInfo.
 *
 *@@added V0.9.19 (2002-04-02) [umoeller]
 */

APIRET xsecQueryProcessOwner(ULONG ulPID,           // in: process ID
                             XWPSECID *puid)        // out: user ID
{
    APIRET              arc = NO_ERROR;
    PXWPSHELLCOMMAND    pCommand;

    if (!(arc = CreateXWPShellCommand(QUECMD_QUERYPROCESSOWNER,
                                      &pCommand)))
    {
        if (!(arc = SendXWPShellCommand(pCommand)))
        {
            // alright:
            *puid = pCommand->pShared->Data.QueryProcessOwner.uid;
        }

        FreeXWPShellCommand(pCommand);
    }

    return arc;
}

/*
 *@@ CopyString:
 *
 *@@added V1.0.1 (2003-01-05) [umoeller]
 */

STATIC APIRET CopyString(PSZ pszTarget,
                         PCSZ pcszSource,
                         ULONG cbTarget)
{
    ULONG cb;
    if (!pcszSource || !*pcszSource)
        return ERROR_INVALID_PARAMETER;
    cb = strlen(pcszSource) + 1;
    if (cb > cbTarget)
        return XWPSEC_NAME_TOO_LONG;

    memcpy(pszTarget,
           pcszSource,
           cb);
    return NO_ERROR;
}

/*
 *@@ xsecCreateUser:
 *      returns the UID on whose behalf the given
 *      process is running.
 *
 *      Returns:
 *
 *      --  NO_ERROR
 *
 *      --  ERROR_NOT_ENOUGH_MEMORY
 *
 *      --  XWPSEC_NAME_TOO_LONG: one of the given
 *          strings is too long.
 *
 *      --  XWPSEC_INSUFFICIENT_AUTHORITY: process
 *          owner does not have sufficient authority
 *          for this query.
 *
 *      plus the error codes from sudbCreateUser.
 *
 *@@added V0.9.19 (2002-04-02) [umoeller]
 */

APIRET xsecCreateUser(PCSZ pcszUserName,        // in: user name
                      PCSZ pcszFullName,        // in: user's descriptive name
                      PCSZ pcszPassword,        // in: user's password
                      XWPSECID gid,             // in: group of the new user
                      XWPSECID *puid)           // out: user ID that was created
{
    APIRET              arc = NO_ERROR;
    PXWPSHELLCOMMAND    pCommand;

    if (!(arc = CreateXWPShellCommand(QUECMD_CREATEUSER,
                                      &pCommand)))
    {
        PQUEUEUNION pUnion = &pCommand->pShared->Data;

        if (    (!(arc = CopyString(pUnion->CreateUser.szUserName,
                                    pcszUserName,
                                    sizeof(pUnion->CreateUser.szUserName))))
             && (!(arc = CopyString(pUnion->CreateUser.szFullName,
                                    pcszFullName,
                                    sizeof(pUnion->CreateUser.szFullName))))
             && (!(arc = CopyString(pUnion->CreateUser.szPassword,
                                    pcszPassword,
                                    sizeof(pUnion->CreateUser.szPassword))))
             && (!(arc = SendXWPShellCommand(pCommand)))
           )
        {
            // alright:
            *puid = pUnion->CreateUser.uidCreated;
        }

        FreeXWPShellCommand(pCommand);
    }

    return arc;
}

/*
 *@@ xsecSetUserData:
 *      updates the user data for the given UID.
 *
 *      Returns:
 *
 *      --  NO_ERROR
 *
 *      --  ERROR_NOT_ENOUGH_MEMORY
 *
 *      --  XWPSEC_NAME_TOO_LONG: one of the given
 *          strings is too long.
 *
 *      --  XWPSEC_INSUFFICIENT_AUTHORITY: process
 *          owner does not have sufficient authority
 *          for this query.
 *
 *      plus the error codes from sudbCreateUser.
 *
 *@@added V1.0.1 (2003-01-05) [umoeller]
 */

APIRET xsecSetUserData(XWPSECID uid,
                       PCSZ pcszUserName,
                       PCSZ pcszFullName)
{
    APIRET              arc = NO_ERROR;
    PXWPSHELLCOMMAND    pCommand;

    if (!(arc = CreateXWPShellCommand(QUECMD_SETUSERDATA,
                                      &pCommand)))
    {
        PQUEUEUNION pUnion = &pCommand->pShared->Data;

        pUnion->SetUserData.uid = uid;

        if (    (!(arc = CopyString(pUnion->SetUserData.szUserName,
                                    pcszUserName,
                                    sizeof(pUnion->SetUserData.szUserName))))
             && (!(arc = CopyString(pUnion->SetUserData.szFullName,
                                    pcszFullName,
                                    sizeof(pUnion->SetUserData.szFullName))))
             && (!(arc = SendXWPShellCommand(pCommand)))
           )
        {
        }

        FreeXWPShellCommand(pCommand);
    }

    return arc;
}

/*
 *@@ xsecDeleteUser:
 *      deletes the user account for the given UID.
 *
 *      Returns:
 *
 *      --  NO_ERROR
 *
 *      --  ERROR_NOT_ENOUGH_MEMORY
 *
 *      --  XWPSEC_NAME_TOO_LONG: one of the given
 *          strings is too long.
 *
 *      --  XWPSEC_INSUFFICIENT_AUTHORITY: process
 *          owner does not have sufficient authority
 *          for this query.
 *
 *      plus the error codes from sudbCreateUser.
 *
 *@@added V1.0.1 (2003-01-05) [umoeller]
 */

APIRET xsecDeleteUser(XWPSECID uid)
{
    APIRET              arc = NO_ERROR;
    PXWPSHELLCOMMAND    pCommand;

    if (!(arc = CreateXWPShellCommand(QUECMD_DELETEUSER,
                                      &pCommand)))
    {
        PQUEUEUNION pUnion = &pCommand->pShared->Data;

        pUnion->uidDelete = uid;

        arc = SendXWPShellCommand(pCommand);

        FreeXWPShellCommand(pCommand);
    }

    return arc;
}
