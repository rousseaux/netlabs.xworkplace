
/*
 *@@sourcefile xwpsecty.h:
 *      declarations which are shared between the various ring-3
 *      parts of XWorkplace Security.
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

#if __cplusplus
extern "C" {
#endif

#ifndef XWPSECTY_HEADER_INCLUDED
    #define XWPSECTY_HEADER_INCLUDED

    /* ******************************************************************
     *
     *   Constants
     *
     ********************************************************************/

    #define XWPSEC_NAMELEN              32

    /* ******************************************************************
     *
     *   Errors
     *
     ********************************************************************/

    #define XWPSEC_ERROR_FIRST          30000

    #define XWPSEC_INTEGRITY            (XWPSEC_ERROR_FIRST + 1)
    #define XWPSEC_INVALID_DATA         (XWPSEC_ERROR_FIRST + 2)
    #define XWPSEC_CANNOT_GET_MUTEX     (XWPSEC_ERROR_FIRST + 3)
    #define XWPSEC_CANNOT_START_DAEMON  (XWPSEC_ERROR_FIRST + 4)

    #define XWPSEC_NO_AUTHORITY         (XWPSEC_ERROR_FIRST + 5)

    #define XWPSEC_HSUBJECT_EXISTS      (XWPSEC_ERROR_FIRST + 6)
    #define XWPSEC_INVALID_HSUBJECT     (XWPSEC_ERROR_FIRST + 7)

    #define XWPSEC_INVALID_PID          (XWPSEC_ERROR_FIRST + 10)
    #define XWPSEC_NO_CONTEXTS          (XWPSEC_ERROR_FIRST + 11)

    #define XWPSEC_USER_EXISTS          (XWPSEC_ERROR_FIRST + 20)
    #define XWPSEC_INVALID_USERID       (XWPSEC_ERROR_FIRST + 21)
    #define XWPSEC_INVALID_GROUPID      (XWPSEC_ERROR_FIRST + 22)

    #define XWPSEC_NOT_AUTHENTICATED    (XWPSEC_ERROR_FIRST + 30)
    #define XWPSEC_CANNOT_START_SHELL   (XWPSEC_ERROR_FIRST + 31)
    #define XWPSEC_INVALID_PROFILE      (XWPSEC_ERROR_FIRST + 32)
    #define XWPSEC_NO_LOCAL_USER        (XWPSEC_ERROR_FIRST + 33)

    #define XWPSEC_DB_GROUP_SYNTAX      (XWPSEC_ERROR_FIRST + 34)
    #define XWPSEC_DB_USER_SYNTAX       (XWPSEC_ERROR_FIRST + 35)
    #define XWPSEC_DB_INVALID_GROUPID   (XWPSEC_ERROR_FIRST + 36)

    #define XWPSEC_DB_ACL_SYNTAX        (XWPSEC_ERROR_FIRST + 40)

    #define XWPSEC_RING0_NOT_FOUND      (XWPSEC_ERROR_FIRST + 50)

    #define XWPSEC_QUEUE_INVALID_CMD    (XWPSEC_ERROR_FIRST + 51)

    /* ******************************************************************
     *
     *   User Database (userdb.c)
     *
     ********************************************************************/

    typedef unsigned long XWPSECID;

    /*
     *@@ XWPGROUPDBENTRY:
     *      group entry in the user database.
     *      See userdb.c for details.
     */

    typedef struct _XWPGROUPDBENTRY
    {
        XWPSECID    gid;                            // group ID
        CHAR        szGroupName[XWPSEC_NAMELEN];    // group name
    } XWPGROUPDBENTRY, *PXWPGROUPDBENTRY;

    typedef const XWPGROUPDBENTRY * PCXWPGROUPDBENTRY;

    /*
     *@@ XWPUSERDBENTRY:
     *      description of a user in the user database.
     *      See userdb.c for details.
     */

    typedef struct _XWPUSERDBENTRY
    {
        CHAR        szUserName[XWPSEC_NAMELEN];
        CHAR        szPassword[XWPSEC_NAMELEN];

        XWPSECID    uid;        // user's ID (unique); 0 for root
        XWPSECID    gid;        // user's group; references an XWPGROUPDBENTRY
    } XWPUSERDBENTRY, *PXWPUSERDBENTRY;

    typedef const XWPUSERDBENTRY * PCXWPUSERDBENTRY;

    APIRET sudbInit(VOID);

    APIRET sudbAuthenticateUser(PXWPUSERDBENTRY pUserInfo,
                                PXWPGROUPDBENTRY pGroupInfo);

    APIRET sudbQueryName(BYTE bType,
                         XWPSECID ID,
                         PSZ pszName,
                         ULONG cbName);

    APIRET sudbQueryID(BYTE bType,
                       PCSZ pcszName,
                       XWPSECID *pID);

    APIRET sudbCreateUser(PXWPUSERDBENTRY pUserInfo);

    APIRET sudbDeleteUser(XWPSECID uid);

    APIRET sudbCreateGroup(PXWPGROUPDBENTRY pGroupInfo);

    APIRET sudbDeleteGroup(XWPSECID gid);

    /* ******************************************************************
     *
     *   Subject Handles Management (subjects.c)
     *
     ********************************************************************/

    typedef unsigned long HXSUBJECT;

    #define SUBJ_USER       1
    #define SUBJ_GROUP      2
    #define SUBJ_PROCESS    3

    /*
     *@@ XWPSUBJECTINFO:
     *      describes a subject.
     *
     *      See subjects.c for details.
     *
     *@@added V0.9.5 (2000-09-23) [umoeller]
     */

    typedef struct _XWPSUBJECTINFO
    {
        HXSUBJECT   hSubject;
                // handle of this subject (unique);
                // 0 only if root user / group
        XWPSECID    id;
                // ID related to this subject;
                // -- for a user subject: the user id (uid); 0 if root
                // -- for a group subject: the group id (gid)
                // -- for a process subject: don't know yet
        BYTE        bType;
                // one of:
                // -- SUBJ_USER
                // -- SUBJ_GROUP
                // -- SUBJ_PROCESS
        ULONG       cUsage;
                // usage count, if SUBJ_GROUP;
                // otherwise 1 always
    } XWPSUBJECTINFO, *PXWPSUBJECTINFO;

    typedef const XWPSUBJECTINFO * PCXWPSUBJECTINFO;

    APIRET subjInit(VOID);

    APIRET subjCreateSubject(PXWPSUBJECTINFO pSubjectInfo);

    APIRET subjDeleteSubject(HXSUBJECT hSubject);

    APIRET subjQuerySubjectInfo(PXWPSUBJECTINFO pSubjectInfo);

    /* ******************************************************************
     *
     *   Security Contexts Management
     *
     ********************************************************************/

    /*
     *@@ XWPSECURITYCONTEXT:
     *      describes the security context for
     *      a process.
     *
     *      Once XWPSec access control is enabled,
     *      a security context is assigned to each
     *      process on the system. Besides, for each
     *      process which is started afterwards, a
     *      security context is created and attached
     *      to the new process.
     */

    typedef struct _XWPSECURITYCONTEXT
    {
        ULONG           ulPID;          // process ID

        HXSUBJECT       hsubjUser;      // user subject handle; 0 if root
        HXSUBJECT       hsubjGroup;     // group subject handle; 0 if root

        // ULONG           ulUMask;        // access rights when new file-system
                                        // objects are created

    } XWPSECURITYCONTEXT, *PXWPSECURITYCONTEXT;

    typedef const XWPSECURITYCONTEXT * PCXWPSECURITYCONTEXT;

    APIRET scxtInit(VOID);

    APIRET scxtCreateSecurityContext(ULONG ulPID,
                                     HXSUBJECT hsubjUser,
                                     HXSUBJECT hsubjGroup);

    APIRET scxtDeleteSecurityContext(ULONG ulPID);

    APIRET scxtFindSecurityContext(PXWPSECURITYCONTEXT pContext);

    APIRET scxtEnumSecurityContexts(HXSUBJECT hsubjUser,
                                    PXWPSECURITYCONTEXT *ppaContexts,
                                    PULONG pulCount);

    APIRET scxtFreeSecurityContexts(PXWPSECURITYCONTEXT paContexts);

    /* ******************************************************************
     *
     *   Access Control Management
     *
     ********************************************************************/

     #define XWPACCESS_READ             0x01                // "R"
                    // For files: Permission to read data from a file and
                    // copy the file.
                    // For directories: Permission to view the directory's
                    // contents.
                    // For copying a file, both the file and its directory
                    // need "Read" permission.
     #define XWPACCESS_WRITE            0x02                // "W"
                    // For files: Permission to write data to a file.
                    // For directories: Permission to write to files
                    // in the directory, but not create files ("Create"
                    // is required for that).
                    // Should be used together with "Read", because
                    // "Write" alone doesn't make much sense.
                    // Besides, "Attrib" permission will also be
                    // required.
     #define XWPACCESS_CREATE           0x04                // "C"
                    // Permission to create subdirectories and files
                    // in a directory. "Create" alone allows creation
                    // of the file only, but once it's closed, it
                    // cannot be changed any more.
     #define XWPACCESS_EXEC             0x08                // "X"
                    // Permission to run (not copy) an executable
                    // or command file.
                    // Note: For .CMD and .BAT files, "Read" permission
                    // is also required.
                    // -- for directories: XWPSec defines this as
                    //    with Unix, to change to a directory.
     #define XWPACCESS_DELETE           0x10                // "D"
                    // Permission to delete subdirectories and files.
     #define XWPACCESS_ATRIB            0x20                // "A"
                    // Permission to modify the attributes of a
                    // resource (read-only, hidden, system, and
                    // the date and time a file was last modified).
     #define XWPACCESS_PERM             0x40
                    // Permission to modify the permissions (read,
                    // write, create, execute, and delete) assigned
                    // to a resource for a user or application.
                    // This gives the user limited administration
                    // authority.
     #define XWPACCESS_ALL              0x7F
                    // Permission to read, write, create, execute,
                    // or delete a resource, or to modify attributes
                    // or permissions.

    typedef APIRET (SACLSUBJECTHANDLECREATED)(PCXWPSUBJECTINFO pSubjectInfo);

    typedef APIRET (SACLSUBJECTHANDLEDELETED)(HXSUBJECT hSubject);

    APIRET saclInit(VOID);

    APIRET saclVerifyAccess(PCXWPSECURITYCONTEXT pContext,
                            PCSZ pcszDir,
                            ULONG ulRequiredAccess);

    /* ******************************************************************
     *
     *   Current Users Management
     *
     ********************************************************************/

    /*
     *@@ XWPLOGGEDON:
     *      describes a current user (i.e. a user
     *      which is in the user database and is
     *      currently logged on, either locally
     *      or via network).
     */

    typedef struct _XWPLOGGEDON
    {
        CHAR            szUserName[XWPSEC_NAMELEN];
        XWPSECID        uid;        // user's ID (unique);
                                    // same as in hsubjUser (cached for speed)
        HXSUBJECT       hsubjUser;  // user's subject handle; 0 if root

        CHAR            szGroupName[XWPSEC_NAMELEN];
        XWPSECID        gid;        // user's group ID;
                                    // same as in hsubjGroup (cached for speed)
        HXSUBJECT       hsubjGroup; // user's group subject handle
    } XWPLOGGEDON, *PXWPLOGGEDON;

    typedef const XWPLOGGEDON * PCXWPLOGGEDON;

    APIRET slogInit(VOID);

    APIRET slogLogOn(PXWPLOGGEDON pNewUser,
                     PCSZ pcszPassword,
                     BOOL fLocal);

    APIRET slogLogOff(XWPSECID uid);

    APIRET slogQueryLocalUser(PXWPLOGGEDON pLoggedOnLocal);

    /* ******************************************************************
     *
     *   XWPShell Shared Memory
     *
     ********************************************************************/

    #define SHMEM_XWPSHELL        "\\SHAREMEM\\XWORKPLC\\XWPSHELL.DAT"
            // shared memory name of XWPSHELLSHARED structure

    /*
     *@@ XWPSHELLSHARED:
     *      shared memory structure allocated by XWPShell
     *      when it starts up. This can be requested by
     *      any process to receive more information.
     */

    typedef struct _XWPSHELLSHARED
    {
        BOOL        fNoLogonButRestart;
                // when the shell process terminates, it
                // can set this flag to TRUE to prevent
                // the logon prompt from appearing; instead,
                // the shell should be restarted with the
                // same user
    } XWPSHELLSHARED, *PXWPSHELLSHARED;

    /* ******************************************************************
     *
     *   XWPShell Queue
     *
     ********************************************************************/

    #define QUEUE_XWPSHELL        "\\QUEUES\\XWORKPLC\\XWPSHELL.QUE"
            // queue name of the master XWPSHell queue

    #define QUECMD_LOCALLOGGEDON                1
    #define QUECMD_PROCESSLOGGEDON              2

    /*
     *@@ QUEUEUNION:
     *
     *@@added V0.9.11 (2001-04-22) [umoeller]
     */

    typedef union _QUEUEUNION
    {
        XWPLOGGEDON     LoggedOn;
        // used with the following commands:
        //
        // -- QUECMD_LOCALLOGGEDON: return data for user that is
        //    currently logged on locally.
        //    Possible error codes: see slogQueryLocalUser.
        //
        // -- QUECMD_PROCESSLOGGEDON: return data for user on whose
        //    behalf the caller process is running.

    } QUEUEUNION, *PQUEUEUNION;

    /*
     *@@ XWPSHELLQUEUEDATA:
     *      structure used in shared memory to communicate
     *      with XWPShell.
     *
     *      A client process must use the following procedure
     *      to communicate with XWPShell:
     *
     *      1)  Open the public XWPShell queue.
     *
     *      2)  Allocate a giveable shared XWPSHELLQUEUEDATA
     *          and give it to XWPShell as read/write.
     *
     *      3)  Create a shared event semaphore in
     *          XWPSHELLQUEUEDATA.hev.
     *
     *      4)  Set XWPSHELLQUEUEDATA.ulCommand to one of the
     *          QUECMD_*  flags, specifying the data to be queried.
     *
     *      5)  Write the XWPSHELLQUEUEDATA pointer to the
     *          queue.
     *
     *      6)  Block on XWPSHELLQUEUEDATA.hevData, which gets
     *          posted by XWPShell when the data has been filled.
     *
     *      7)  Check XWPSHELLQUEUEDATA.arc. If NO_ERROR,
     *          XWPSHELLQUEUEDATA.Data union has been filled
     *          with data according to ulCommand.
     *
     *      8)  Close the event sem and free the shared memory.
     *
     *@@added V0.9.11 (2001-04-22) [umoeller]
     */

    typedef struct _XWPSHELLQUEUEDATA
    {
        ULONG       ulCommand;          // one of the QUECMD_* values

        HEV         hevData;            // event semaphore posted
                                        // when XWPShell has produced
                                        // the data

        APIRET      arc;                // error code set by XWPShell;
                                        // if NO_ERROR, the following is valid

        QUEUEUNION  Data;               // data, format depends on ulCommand

    } XWPSHELLQUEUEDATA, *PXWPSHELLQUEUEDATA;

#endif

#if __cplusplus
}
#endif

