
/*
 *@@sourcefile acldb.c:
 *      database for access control lists (ACL's).
 *
 *      ACL's are complicated and difficult to understand.
 *      The ACL database contains entries for (possibly)
 *      each directory and file on the system.
 *
 *      This implementation uses the Unix model of granting
 *      and denying access (as implemented by Linux).
 *
 *      As a result, each ACL entry has the following fields:
 *
 *      -- OWNUSER    name of user who owns this file.
 *      -- OWNGROUP   name of group this file belongs to.
 *      -- OWNERRIGHTS  rights of the owner (RWX).
 *      -- GROUPRIGHTS  rights of members of the group (RWX).
 *      -- OTHERRIGHTS  rights of everyone else (RWX).
 *
 *      This is exactly the Unix way of doing things.
 *      "ls" output is as follows:
 *
 +          -rwxrwxrwx  4  ownuser owngroup  ...
 +           \ /\ /\ /
 +            V  V  V
 +            ³  ³  ÀÄ other
 +            ³  ÀÄÄÄÄ group
 +            ÀÄÄÄÄÄÄÄ owner
 *
 *      This ACLDB implementation uses a plain text file
 *      for speed, where each ACL entry consists of a
 *      single line like this:
 +
 +        F "resname" decuid decgid octrights
 *
 *      being:
 *
 *      -- F: a flag marking the type of resource, being:
 *              -- "R": root directory (drive)
 *              -- "D": directory
 *              -- "F": file
 *              -- "P": process
 *
 *      -- "resname": the name of the resource, being
 *              -- for "R": Drive letter (e.g. "G:")
 *              -- for "D": full directory specification (e.g. "G:\DESKTOP")
 *              -- for "F": full file path (e.g. "G:\DESKTOP\INDEX.HTML")
 *              -- for "P": full executable path (e.g. "G:\OS2\E.EXE")
 *
 *      -- decuid: decimal user ID of owner
 *
 *      -- decgid: decimal group ID of owning group
 *
 *      -- octrights: rights flags (octal), as on Unix "chmod"
 *
 *      With this implementation, if no ACL entry exists for a resource
 *      (i.e. a file or directory), the ACL entry of the parent directory
 *      applies, climbing up until the drive level is reached. This way
 *      you only have to create ACL entries if they are supposed to be
 *      different from the parent's ones.
 *
 *      If no ACL entry is found this way (i.e. not even the drive has an
 *      ACL entry), access is denied.
 *
 *      <B>Example:</B>
 *
 *      1.  User "user" logs on, who belongs to group "users".
 *          The user's ID (uid) is 1, the group id (gid) is 1.
 *
 *      2.  XWPSec creates two subject handles, one for the
 *          user "user", one for the group "users".
 *
 *      3.  Now assume that "user" attempts to create a file
 *          in "F:\home\user" (his own directory).
 *
 *          This results in a saclVerifyAccess call for
 *          "F:\home\user", asking for authorization.
 *
 *          saclVerifyAccess receives the subject handle for
 *          "user" (and also for the "users" group) and will
 *          find out that an ACLENTRYNODE exists for the user
 *          subject handle and "F:\home\user".
 *
 *          Since "RWX" is specified in that entry, access is
 *          granted.
 *
 *      4.  Now assume that "user" attempts to create a file
 *          in "F:\home" (the parent directory).
 *
 *          Again, saclVerifyAccess receives the subject handle
 *          for "user" and the "users" group.
 *
 *          This time, however, since "F:\home" is not owned
 *          by user, no ACLENTRYNODE exists for that, and for
 *          none of the parent directories either.
 *
 *          saclVerifyAccess will then try the group subject
 *          handle (for "users"). "F:\home" is owned by "users",
 *          and access rights are defined as "R--". So creating
 *          a file is not permitted, and access is turned down
 *          gor food.
 *
 *      <B>Remarks:</B>
 *
 *      -- The functions in this file never get called
 *         for processes running on behalf of the
 *         superuser (root). This is because per definition,
 *         the superuser has unrestricted access to the
 *         system, so no ACL entries are needed.
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

#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_WIN
#include <os2.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <io.h>

#include "setup.h"

#include "helpers\dosh.h"
#include "helpers\linklist.h"
#include "helpers\stringh.h"
#include "helpers\tree.h"               // red-black binary trees

#include "bldlevel.h"

#include "security\xwpsecty.h"

/* ******************************************************************
 *
 *   Private Definitions
 *
 ********************************************************************/

#define ACLTYPE_DRIVE           1
#define ACLTYPE_DIRECTORY       2
#define ACLTYPE_FILE            3
#define ACLTYPE_PROCESS         4

// Unix permissions
#define ACLACC_READ         0x0001
        // -- for files: allow read
        // -- for directories: allow list contents
#define ACLACC_WRITE        0x0002
        // -- for files: allow write
        // -- for directories: allow create, rename, remove files in directory
#define ACLACC_EXEC         0x0004
        // -- for files: allow execute
        // -- for directories: allow "access files", that is:
        //          -- change to directory

// octal constants as with Linux
// defined as:       0000 000u uugg gooo

// 1) rights for "other" users (bits 0-2) 0000 0000 0000 0ooo
#define ACLACC_READ_OTHER           01
#define ACLACC_WRITE_OTHER          02
#define ACLACC_EXEC_OTHER           04

#define ACLACC_OTHER_MASK           07
#define GET_OTHER_RIGHTS(ulFlags) ( (ulFlags) & ACLACC_OTHER_MASK )

// 2) rights for group members (bits 3-5) 0000 0000 00gg g000
#define ACLACC_READ_GROUP           010
#define ACLACC_WRITE_GROUP          020
#define ACLACC_EXEC_GROUP           040

#define ACLACC_GROUP_MASK           070
#define ACLACC_GROUP_SHR            3
#define GET_GROUP_RIGHTS(ulFlags) ( ((ulFlags) & ACLACC_GROUP_MASK) >> ACLACC_GROUP_SHR )

// 3) rights for owner user (bits 6-8)    0000 000u uu00 0000
#define ACLACC_READ_USER            0100
#define ACLACC_WRITE_USER           0200
#define ACLACC_EXEC_USER            0400

#define ACLACC_USER_MASK            0700
#define ACLACC_USER_SHR             6
#define GET_USER_RIGHTS(ulFlags) ( ((ulFlags) & ACLACC_USER_MASK) >> ACLACC_USER_SHR )

/*
 *@@ ACLDBENTRYNODE:
 *      this represents one ACL database entry.
 *      This corresponds to a single line in the
 *      database file.
 *
 *      This is an extended tree node (see helpers\tree.c).
 *      These nodes are sorted according to pszName;
 *      there are two comparison functions for this
 *      (fnCompareACLDBNames_Nodes, fnCompareACLDBNames_Data).
 *
 *      Created by LoadACLDatabase.
 */

typedef struct _ACLDBENTRYNODE
{
    TREE        Tree;
            // base tree node; "ulkey" is a PSZ to the resource name

    // PSZ         pszName;
            // name of the resource to which this entry applies
            // (in a new buffer allocated with malloc());
            // one of the following:
            // -- ACLTYPE_DRIVE:        the drive name (e.g. "G:")
            // -- ACLTYPE_DIRECTORY     the capitalized directory name (e.g. "DESKTOP")
            // -- ACLTYPE_FILE          the capitalized file name (e.g. "INDEX.HTML")

    ULONG       ulType;
            // one of the following:
            // -- ACLTYPE_DRIVE         entry is for an entire drive
            // -- ACLTYPE_DIRECTORY     entry is for a directory (or subdirectory)
            // -- ACLTYPE_FILE          entry is for a file

    XWPSECID    uid;
            // user ID (owner of resource)
    XWPSECID    gid;
            // group ID (owner of resource)

    ULONG       ulUnixAccessRights;
            // Unix access rights flags as stored in ACL database;
            // this is any combination of the following:
            // 1) rights for "other" users
            // -- ACLACC_READ_OTHER           01
            // -- ACLACC_WRITE_OTHER          02
            // -- ACLACC_EXEC_OTHER           04

            // 2) rights for group members
            // -- ACLACC_READ_GROUP           010
            // -- ACLACC_WRITE_GROUP          020
            // -- ACLACC_EXEC_GROUP           040

            // 3) rights for owner user
            // -- ACLACC_READ_USER            0100
            // -- ACLACC_WRITE_USER           0200
            // -- ACLACC_EXEC_USER            0400

    ULONG       ulXWPUserAccessFlags,
                ulXWPGroupAccessFlags,
                ulXWPOtherAccessFlags;

} ACLDBENTRYNODE, *PACLDBENTRYNODE;

APIRET LoadACLDatabase(PULONG pulLineWithError);

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

// ACL database
TREE        *G_treeACLDB;
    // balanced binary tree of currently loaded ACLs;
    // contains ACLDBENTRYNODE's
ULONG       G_cACLDBEntries = 0;
HMTX        G_hmtxACLs = NULLHANDLE;
    // mutex semaphore protecting global data

/* ******************************************************************
 *
 *   Initialization
 *
 ********************************************************************/

/*
 *@@ saclInit:
 *      initializes XWPSecurity.
 */

APIRET saclInit(VOID)
{
    APIRET arc = NO_ERROR;

    if (G_hmtxACLs == NULLHANDLE)
    {
        // first call:
        arc = DosCreateMutexSem(NULL,       // unnamed
                                &G_hmtxACLs,
                                0,          // unshared
                                FALSE);     // unowned
        if (arc == NO_ERROR)
        {
            ULONG   ulLineWithError;
            treeInit(&G_treeACLDB);
            arc = LoadACLDatabase(&ulLineWithError);
        }
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
 *@@ LockACLs:
 *      locks the global security data by requesting
 *      its mutex.
 *
 *      Always call UnlockACLs() when you're done.
 */

APIRET LockACLs(VOID)
{
    APIRET arc = NO_ERROR;

    arc = DosRequestMutexSem(G_hmtxACLs,
                             SEM_INDEFINITE_WAIT);

    return (arc);
}

/*
 *@@ UnlockACLs:
 *      unlocks the global security data.
 */

APIRET UnlockACLs(VOID)
{
    return (DosReleaseMutexSem(G_hmtxACLs));
}

/*
 *@@ fnCompareACLDBNames_Nodes:
 *
 */

int TREEENTRY fnCompareStrings(ULONG ul1, ULONG ul2)
{
    return (strcmp((PSZ)ul1,
                   (PSZ)ul2));
}

#ifdef _PMPRINTF_
    /*
     *@@ DumpAccessFlags:
     *
     */

    VOID DumpAccessFlags(PSZ pszBuf,
                         ULONG ulAccess)
    {
        if (ulAccess & XWPACCESS_READ)
            *pszBuf++ = 'R';
        if (ulAccess & XWPACCESS_WRITE)
            *pszBuf++ = 'W';
        if (ulAccess & XWPACCESS_CREATE)
            *pszBuf++ = 'C';
        if (ulAccess & XWPACCESS_EXEC)
            *pszBuf++ = 'X';
        if (ulAccess & XWPACCESS_DELETE)
            *pszBuf++ = 'D';
        if (ulAccess & XWPACCESS_ATRIB)
            *pszBuf++ = 'A';
        if (ulAccess & XWPACCESS_PERM)
            *pszBuf++ = 'P';
        *pszBuf = 0;
    }
#else
#define DumpAccessFlags(a, b)
#endif

/* ******************************************************************
 *
 *   ACL Database
 *
 ********************************************************************/

/*
 *@@ ConvertUnix2XWP:
 *      converts Unix access flags (ACLACC_READ, WRITE, EXEC)
 *      to the XWorkplace (OS/2) ones.
 *
 *      This never returns the XWPACCESS_PERM bit, which is
 *      reserved for the (user) owner of a resource.
 */

ULONG ConvertUnix2XWP(ULONG ulUnixFlags,    // in: any combination of:
                                            // -- ACLACC_READ         0x0001
                                            // -- ACLACC_WRITE        0x0002
                                            // -- ACLACC_EXEC         0x0004
                      ULONG ulType)         // in: one of:
                                            // -- ACLTYPE_DRIVE           1
                                            // -- ACLTYPE_DIRECTORY       2
                                            // -- ACLTYPE_FILE            3
                                            // -- ACLTYPE_PROCESS         4
{
    ULONG ulXWPFlags = 0;

    switch (ulType)
    {
        // drive or directory:
        case ACLTYPE_DRIVE:
        case ACLTYPE_DIRECTORY:
            if (ulUnixFlags & ACLACC_READ)
                ulXWPFlags |=
                    XWPACCESS_READ;
                    // For directories: Permission to view the directory's
                    // contents.
            if (ulUnixFlags & ACLACC_WRITE)
                ulXWPFlags |=
                    (XWPACCESS_WRITE
                    // For directories: Permission to write to files
                    // in the directory, but not create files ("Create"
                    // is required for that).
                    | XWPACCESS_CREATE
                    // Permission to create subdirectories and files
                    // in a directory. "Create" alone allows creation
                    // of the file only, but once it's closed, it
                    // cannot be changed any more.
                    | XWPACCESS_DELETE
                    // Permission to delete subdirectories and files.
                    | XWPACCESS_ATRIB);
                    // Permission to modify the attributes of a
                    // resource (read-only, hidden, system, and
                    // the date and time a file was last modified).
            if (ulUnixFlags & ACLACC_EXEC)
                ulXWPFlags |=
                    XWPACCESS_EXEC;
                    // Permission to run (not copy) an executable
                    // or command file.
                    // Note: For .CMD and .BAT files, "Read" permission
                    // is also required.
                    // -- for directories: XWPSec defines this as
                    //    with Unix, to change to a directory.
        break;

        case ACLTYPE_FILE:
            if (ulUnixFlags & ACLACC_READ)
                ulXWPFlags |=
                    XWPACCESS_READ;
                    // For files: Permission to read data from a file and
                    // copy the file.
            if (ulUnixFlags & ACLACC_WRITE)
                ulXWPFlags |=
                    (XWPACCESS_WRITE
                    // For files: Permission to write data to a file.
                    | XWPACCESS_DELETE
                    // Permission to delete subdirectories and files.
                    | XWPACCESS_ATRIB);
                    // Permission to modify the attributes of a
                    // resource (read-only, hidden, system, and
                    // the date and time a file was last modified).
            if (ulUnixFlags & ACLACC_EXEC)
                ulXWPFlags |=
                    XWPACCESS_EXEC;
                    // Permission to run (not copy) an executable
                    // or command file.
                    // Note: For .CMD and .BAT files, "Read" permission
                    // is also required.
        break;
    }

    return (ulXWPFlags);
}

ULONG G_aulRights[]          // rwxrwxrwx
    = {
            // first digits: user read, write, execute
            ACLACC_READ_USER,
            ACLACC_WRITE_USER,
            ACLACC_EXEC_USER,

            // second digits: group read, write, execute
            ACLACC_READ_GROUP,
            ACLACC_WRITE_GROUP,
            ACLACC_EXEC_GROUP,

            // third digits: other read, write, execute
            ACLACC_READ_OTHER,
            ACLACC_WRITE_OTHER,
            ACLACC_EXEC_OTHER
      };

/*
 *@@ CreateACLDBEntry:
 *      creates a new ACLDB entry. pszName must have
 *      been malloc'd.
 */

APIRET CreateACLDBEntry(ULONG ulType,
                        PSZ pszName,
                        XWPSECID uid,
                        XWPSECID gid,
                        PSZ pszRights)  // in: ptr to "rwxrwxrwx" string
{
    APIRET arc = NO_ERROR;

    PACLDBENTRYNODE pNewEntry
        = (PACLDBENTRYNODE)malloc(sizeof(ACLDBENTRYNODE));
    if (!pNewEntry)
        arc = ERROR_NOT_ENOUGH_MEMORY;
    else
    {
        ULONG   ulDigit = 0;
        PSZ     pLine = 0;

        memset(pNewEntry, 0, sizeof(*pNewEntry));

        pNewEntry->ulType = ulType;
        pNewEntry->Tree.ulKey = (ULONG)pszName;
        pNewEntry->uid = uid;
        pNewEntry->gid = gid;

        pLine = pszRights;
        for (ulDigit = 0;
             ulDigit < 9;
             ulDigit++)
        {
            if (*pLine != '-')
                // access granted:
                pNewEntry->ulUnixAccessRights
                        |= G_aulRights[ulDigit];
            pLine++;
        }

        // convert Unix access flags for...

        // a) user (owner)
        pNewEntry->ulXWPUserAccessFlags
            = (ConvertUnix2XWP(GET_USER_RIGHTS(pNewEntry->ulUnixAccessRights),
                              ulType)
                | XWPACCESS_PERM);  // user may also change permissions

        // b) group (owning group)
        pNewEntry->ulXWPGroupAccessFlags
            = ConvertUnix2XWP(GET_GROUP_RIGHTS(pNewEntry->ulUnixAccessRights),
                              ulType);

        // c) other users (non-user, non-group)
        pNewEntry->ulXWPOtherAccessFlags
            = ConvertUnix2XWP(GET_OTHER_RIGHTS(pNewEntry->ulUnixAccessRights),
                              ulType);

        treeInsert(&G_treeACLDB,
                   (TREE*)pNewEntry,
                   fnCompareStrings);
        G_cACLDBEntries++;

        _Pmpf(("LoadACLDatabase: got entry \"%s\" -> 0x%lX",
                pNewEntry->pszName,
                pNewEntry->ulUnixAccessRights));
    }

    return (arc);
}

/*
 *@@ LoadACLDatabase:
 *      loads the ACL database.
 *
 *      Called on startup by saclInit.
 *
 *      The caller must lock the ACLDB before using this.
 */

APIRET LoadACLDatabase(PULONG pulLineWithError)
{
    APIRET  arc = NO_ERROR;

    CHAR    szUserDB[CCHMAXPATH];
    PSZ     pszUserDB = NULL,
            pszDBPath = NULL;
    CHAR    szDBPath[CCHMAXPATH];
    FILE    *UserDBFile;

    ULONG   ulLineCount = 0;

    pszDBPath = getenv("XWPACLDB");
    if (!pszDBPath)
    {
        // XWPUSERDB not specified:
        // default to "?:\os2" on boot drive
        sprintf(szDBPath, "%c:\\OS2", doshQueryBootDrive());
        pszDBPath = szDBPath;
    }
    sprintf(szUserDB, "%s\\xwpusers.acc", pszDBPath);

    UserDBFile = fopen(szUserDB, "r");
    if (!UserDBFile)
        arc = _doserrno;
    else
    {
        CHAR        szLine[300];
        PSZ         pLine = NULL;
        while ((pLine = fgets(szLine, sizeof(szLine), UserDBFile)) != NULL)
        {
            ULONG   ulType = 0;

            // skip spaces, tabs
            while (     (*pLine)
                     && ( (*pLine == ' ') || (*pLine == '\t') )
                  )
                pLine++;

            switch (*pLine)
            {
                case 'R':       // root directory (drive)
                    ulType = ACLTYPE_DRIVE;
                break;

                case 'D':       // directory
                    ulType = ACLTYPE_DIRECTORY;
                break;

                case 'F':       // file
                    ulType = ACLTYPE_FILE;
                break;

                case 'P':       // process
                    ulType = ACLTYPE_PROCESS;
                break;
            }

            if (ulType)
            {
                PSZ pszName = 0;
                pLine++;        // on space now
                while (     (*pLine)
                         && ( (*pLine == ' ') || (*pLine == '\t') )
                      )
                    pLine++;

                if (!*pLine)
                    arc = XWPSEC_DB_ACL_SYNTAX;
                else
                {
                    // on '"' now
                    pszName = strhQuote(pLine, '"', &pLine);
                    if (!pszName)
                        arc = XWPSEC_DB_ACL_SYNTAX;
                    else
                    {
                        // OK, got name:
                        // pLine points to space after '"' now
                        while (     (*pLine)
                                 && ( (*pLine == ' ') || (*pLine == '\t') )
                              )
                            pLine++;

                        if (!*pLine)
                            arc = XWPSEC_DB_ACL_SYNTAX;
                        else
                        {
                            ULONG   uid = 0,
                                    gid = 0;
                            CHAR    szRights[20];
                            if (sscanf(pLine, "%d %d %s", &uid, &gid, szRights) != 3)
                                arc = XWPSEC_DB_ACL_SYNTAX;
                            else
                            {
                                if (strlen(szRights) != 9)
                                    arc = XWPSEC_DB_ACL_SYNTAX;
                                else
                                {
                                    // OK, got fields:
                                    arc = CreateACLDBEntry(ulType,
                                                           pszName,
                                                           uid,
                                                           gid,
                                                           szRights);
                                }
                            }
                        }
                    }
                }
            }

            if (arc != NO_ERROR)
                break;  // while

            ulLineCount++;
        } // end while ((pLine = fgets(szLine, sizeof(szLine), UserDBFile)) != NULL)
        fclose(UserDBFile);
    }

    *pulLineWithError = ulLineCount;

    return (arc);
}

/*
 *@@ FindACLDBEntry:
 *
 */

PACLDBENTRYNODE FindACLDBEntry(const char *pcszName)
{
    return ((PACLDBENTRYNODE)treeFind(G_treeACLDB,
                                      (ULONG)pcszName,
                                      fnCompareStrings));
}

/* ******************************************************************
 *
 *   Access Control APIs
 *
 ********************************************************************/

/*
 *@@ saclVerifyAccess:
 *      this function is called every single time
 *      when XWPSec needs to have access to a
 *      directory authorized.
 *
 *      This must return either NO_ERROR (if access
 *      is granted) or ERROR_ACCESS_DENIED (if
 *      access is denied).
 *
 *      If this returns any other error code, a
 *      security violation is raised.
 *
 *      In pContext, this function receives the
 *      XWPSECURITYCONTEXT of the process which
 *      needs to have access verified.
 *
 *      This function then needs to look up the
 *      ACL's for the directory specified in pcszDir,
 *      based on the user and group subject handles
 *      provided in the security context.
 *
 *      It is up to this function to determine
 *      how user and group rights are evaluated
 *      and which one should have priority.
 *
 *      Preconditions:
 *
 *      -- pcszResource presently will always contain a fully
 *         qualified file name or directory name.
 *         However, this  may be in mixed case, and the case
 *         need not be the same as on the resource.
 *
 *      -- This never gets called for processes running
 *         on behalf of root's subject handle (whose uid
 *         is 0) because per definition, there are no ACL
 *         entries for root. Root has unrestricted access
 *         to the machine.
 */

APIRET saclVerifyAccess(PCXWPSECURITYCONTEXT pContext,   // in: security context of process
                        const char *pcszResource,        // in: fully qualified file name
                        ULONG ulRequiredAccess)          // in: OR'ed XWPACCESS_* flags
{
    APIRET arc = NO_ERROR;

    BOOL fLocked = (LockACLs() == NO_ERROR);

    if (!fLocked)
        arc = XWPSEC_CANNOT_GET_MUTEX;
    else
    {
        PACLDBENTRYNODE pACLEntry = NULL;       // default: not found

        PSZ pszDir2 = strdup(pcszResource);
        strupr(pszDir2);

        _Pmpf(("Authorizing %s", pszDir2));

        // STEP 1: find ACL entry for this resource;
        // climb up directories until an entry is found....

        while (TRUE)        // run at least once
        {
            PSZ p;
            // find matching ACL entry
            pACLEntry = FindACLDBEntry(pszDir2);
            if (pACLEntry)
                // item found:
                break;

            // no ACL entry found:
            // try parent directory
            p = strrchr(pszDir2, '\\');
            if (!p)
                // we've reached root:
                // that means ACL's don't even
                // exist for the root directory,
                // so deny access
                break;
            else
                // overwrite '\' with 0,
                // so we can search for parent directory;
                // e.g. C:\DESKTOP\PROGRAMS\GAMES
                *p = 0;
                // now  G:\DESKTOP\PROGRAMS
        }

        if (!pACLEntry)
            // no ACL entry found:
            arc = ERROR_ACCESS_DENIED;
        else
        {
            // ACL entry found (either for resource or for parent):

            // STEP 2: find access rights which apply here
            // (user, group, or other)

            // compose access flags
            ULONG   ulAccessFlags = 0;  // XWPACCESS_* flags; default: no access

            XWPSUBJECTINFO SubjectInfo;

            // get subject info for user
            SubjectInfo.hSubject = pContext->hsubjUser;
            arc = subjQuerySubjectInfo(&SubjectInfo);
            if (arc == NO_ERROR)
            {
                // now we know the user ID
                if (pACLEntry->uid == SubjectInfo.id)
                {
                    // process's user owns this resource:
                    // use these access flags
                    ulAccessFlags = pACLEntry->ulXWPUserAccessFlags;
                }
                else
                {
                    // user does NOT own this resource:
                    // try group instead...

                    // get subject info for group
                    SubjectInfo.hSubject = pContext->hsubjGroup;
                    arc = subjQuerySubjectInfo(&SubjectInfo);
                    if (arc == NO_ERROR)
                    {
                        // now we know the group ID
                        if (pACLEntry->gid == SubjectInfo.id)
                        {
                            // process's group owns this resource:
                            // use these access flags
                            ulAccessFlags = pACLEntry->ulXWPGroupAccessFlags;
                        }
                        else
                        {
                            // user's group does NOT own this resource:
                            // then use "other" flags
                            ulAccessFlags = pACLEntry->ulXWPOtherAccessFlags;
                        }
                    }
                }
            }

            if (arc == NO_ERROR)
            {
                // ulAccessFlags are valid:

                // STEP 3: verify access!
                CHAR    szRequired[30],
                        szGiven[30];
                DumpAccessFlags(szRequired, ulRequiredAccess);
                DumpAccessFlags(szGiven, ulAccessFlags);
                _Pmpf(("    req: \"%s\", given: \"%s\"", szRequired, szGiven));

                if (    (ulRequiredAccess       // e.g. XWPACCESS_WRITE | XWPACCESS_CREATE
                            & ulAccessFlags)    // e.g. XWPACCESS_WRITE
                     != ulRequiredAccess)
                    arc = ERROR_ACCESS_DENIED;
            }
        } // end else if (!pACLEntry)

        free(pszDir2);
    }

    if (fLocked)
        UnlockACLs();

    _Pmpf(("        returning arc %d", arc));

    return (arc);
}

