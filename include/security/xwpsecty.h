
/*
 *@@sourcefile xwpsecty.h:
 *      declarations which are shared between the various ring-3
 *      parts of XWorkplace Security (XWPSec).
 *
 *      XWPSec consists of the following modules:
 *
 *      1)  XWPSHELL.EXE, which is to be installed through RUNWORKPLACE
 *          in CONFIG.SYS and functions as both a WPS starter and a
 *          daemon for managing the security driver;
 *
 *      2)  XWPSEC32.SYS, which is a ring-0 driver performing
 *          authentification for OS/2 APIs like DosOpen, DosMove etc.
 *          through SES callouts.
 *
 *      XWPSec does not require OS/2 Security Enabling Services (SES),
 *      but it does use the SES kernel callouts (KPIs) to authenticate
 *      events.
 *
 *
 *      <b>XWPSec Authentication Model</b>
 *
 *      Basically, there are two competing models of access permissions
 *      in the computing world, the POSIX model and the Windows/OS/2
 *      model. Since the POSIX model can be implemented as a subset of
 *      the OS/2 model, XWPSec follows the OS/2 model, with very
 *      fine-grained access permissions and different ACLs per resource.
 *
 *      XWPSec implements the usual stuff like users, groups, and
 *      privileged processes with increased access rights. As opposed
 *      to the POSIX model, group membership is an M:N relation, meaning
 *      that any user can be a member of an infinite number of groups.
 *      Also, while the POSIX model only allows controlling permissions
 *      for the owner, group, and "world" (rwxrwxrwx model), XWPSec has
 *      real ACLs which are unlimited in size per resource.
 *
 *      To implement this efficiently, XWPSec uses "subject handles", which
 *      is a concept borrowed from SES. However, XWPSec does not use the
 *      SES APIs for creating and managing those, but implements this
 *      functionality itself.
 *
 *      For authorizing events, XWPSec uses the XWPSUBJECTINFO and
 *      XWPSECURITYCONTEXT structures, plus an array of RESOURCEACL
 *      structs which forms the global system ACL table shared with
 *      the ring-0 device driver.
 *
 *
 *      <b>Logon</b>
 *
 *      XWPShell maintains a list of users that are currently
 *      logged on. There can be an infinite number of logged-on
 *      users. However, one user logon is special and is called
 *      the "local" logon: that user owns the shell, most
 *      importantly the Workplace Shell, and processes started
 *      via the shell run on behalf of that user.
 *
 *      When a user logs on, XWPShell authenticates the credentials
 *      (normally, via user name and password), and creates one
 *      subject handle for that user plus one subject handle for
 *      every group that the user belongs to. XWPShell then rebuilds
 *      the system ACL table and sends it to the driver so it can
 *      perform authentication.
 *
 *      Logon can thus happen in two situations. Most frequently,
 *      the local user logs on via the logon dialog when XWPShell
 *      starts up together with the system. However, XWPShell also
 *      allows any application to change its own security context
 *      be re-logging on with different credentials.
 *
 *      This is the typical sequence of events when XWPShell is running:
 *
 *      1)  When XWPShell starts up, it scans the process list to figure
 *          out all processes that are already running because of system
 *          startup. It then contacts the ring-0 driver, passing it an
 *          array of these PIDs, and thus enables local security. The
 *          driver then creates root security contexts for those processes.
 *
 *          From that point on, all system events are authenticated until
 *          the system is shut down.
 *
 *          (Before that, the driver does not deny access in order not to
 *          hickup the startup sequence.)
 *
 *      2)  XWPShell displays the logon dialog.
 *
 *      3)  After the user has entered his user name and password,
 *          XWPShell authenticates the user against the user database.
 *
 *      4)  If the user was authenticated, XWPSec creates subjects
 *          for the user and his groups. With each subject creation,
 *          XWPShell rebuilds the system ACL list with the subject
 *          handles and sends them down to ring 0 so that the driver
 *          can perform authorization (see RESOURCEACL for details).
 *
 *      5)  XWPShell sets the security context of itself (XWPSHELL.EXE)
 *          to contain these subject handles by calling a ring-0 driver
 *          API. Whoever owns XWPSHELL.EXE is, by definition, the "local"
 *          user.
 *
 *      6)  XWPShell then changes the user profile (OS2.INI) via PrfReset,
 *          builds an environment for the Workplace Shell, and starts
 *          PMSHELL.EXE so that the WPS starts up with the user's Desktop.
 *          The WPS is started as a child of XWPSHELL.EXE and thus
 *          inherits its security context. Since the WPS uses WinStartApp
 *          to start applications, those will inherit the local user's
 *          security context as well.
 *
 *          The following environment variables are set:
 *
 *          --  USER is set to the user name;
 *          --  USERID is set to the user ID (uid);
 *          --  HOME is set to the user's home directory;
 *          --  OS2_INI is set to the user's OS2.INI file. This
 *              does not affect the profile (which has been
 *              changed using PrfReset before), but is still
 *              changed in case any program references this
 *              variable.
 *
 *          All processes started by the WPS will inherit this
 *          environment since the WPS always copies its environment
 *          to child processes.
 *
 *      7)  From then on, all system events will be authorized against
 *          the local user's security context, which contains the subject
 *          handles of the local user and his groups. For example, when
 *          an application opens a file, the driver will authorize this
 *          event against the ACLs stored in the user's subject infos
 *          and permit the operation or deny access.
 *
 *      8)  When the user logs off via the new menu item in the Desktop's
 *          context menu (implemented by XWorkplace), XWorkplace will then
 *          terminate all processes running on behalf of this user and
 *          terminate the WPS process via DosExit. It sets a special flag
 *          in shared memory to indicate to XWPShell that this was not
 *          just a WPS restart (or trap), but a logoff event to prevent
 *          XWPShell from restarting the WPS immediately.
 *
 *      9)  When XWPShell then realizes that the WPS has terminated and
 *          finds this flag, it logs off the user. Logging off implies
 *          again checking that no processes are running on behalf of
 *          the local user any more, changing the security context of
 *          XWPSHELL.EXE back to "noone", and deleting the subject
 *          handles that were created in (4.). Go back to (2.).
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

#if __cplusplus
extern "C" {
#endif

#ifndef XWPSECTY_HEADER_INCLUDED
    #define XWPSECTY_HEADER_INCLUDED

    /* ******************************************************************
     *
     *   Global constants
     *
     ********************************************************************/

    #define XWPSEC_NAMELEN              32
    #define XWPSEC_FULLNAMELEN          80

    /* ******************************************************************
     *
     *   Subjects and security contexts
     *
     ********************************************************************/

    typedef unsigned long HXSUBJECT;

    typedef unsigned long XWPSECID;

    /*
     *@@ XWPSUBJECTINFO:
     *      describes a subject.
     *
     *      A subject represents either a user, a group, or a
     *      trusted process associated with an access control
     *      list.
     *
     *      Subject handles allow for reusing ACLs efficiently
     *      when multiple users are logged on and also isolate
     *      the ring-0 driver from user and group management.
     *
     *      When a user logs on, subject handles get created
     *      that represent the access permissions of that user.
     *      Every running process has a list of subject handles
     *      associated with it, the sum of which allow for
     *      authorizing events. The driver only sees subject
     *      handles, not users or groups. However, the user
     *      and group IDs are listed in the subject info structure
     *      to allow the driver to audit events.
     *
     *      Subject handles are created by XWPShell (ring 3)
     *      when users log on and then sent down to the ring-0
     *      driver together with a new system ACL list for future
     *      use.
     *
     *@@added V0.9.5 (2000-09-23) [umoeller]
     */

    typedef struct _XWPSUBJECTINFO
    {
        HXSUBJECT   hSubject;
                // handle of this subject (unique),
                // simply raised with each subject creation
        XWPSECID    id;
                // ID related to this subject;
                // -- for a user subject: the user id (uid); 0 if root
                // -- for a group subject: the group id (gid)
                // -- for a process subject: the process id (pid)
        BYTE        bType;
                // one of:
                #define SUBJ_USER       1
                #define SUBJ_GROUP      2
                #define SUBJ_PROCESS    3

        ULONG       cUsage;
                // usage count (to allow for reuse)

    } XWPSUBJECTINFO, *PXWPSUBJECTINFO;

    APIRET subjInit(VOID);

    APIRET subjCreateSubject(PXWPSUBJECTINFO pSubjectInfo);

    APIRET subjDeleteSubject(HXSUBJECT hSubject);

    APIRET subjQuerySubjectInfo(PXWPSUBJECTINFO pSubjectInfo);

    /*
     *@@ XWPSECURITYCONTEXT:
     *      describes the security context for a process.
     *
     *      For each process on the system, one security context
     *      defines its permissions via subject handles. Security
     *      contexts are created in ring 0 in the following
     *      situations:
     *
     *      --  Via the DosExecPgm callout, when a new process is
     *          started. The driver then creates a security context
     *          with the same subject handles as the new process's
     *          parent process. This way process permissions inherit
     *          from each other.
     *
     *      --  Via a ring-0 API that gets called by XWPShell to
     *          create security contexts for processes that are
     *          already running at startup, including XWPShell
     *          itself.
     *
     *      Each subject handle in the context represents either a user,
     *      a group, or a trusted process. A subject handle of "0" is
     *      special and indicates root access. If such a subject handle
     *      is found, the driver grants full access and does not reject
     *      anything.
     *
     *      As a result, the following are typical security contexts:
     *
     *      --  A process running on behalf of a certain user will
     *          usually contain at least two subject handles, one
     *          for the user (if any ACLs were defined for that user
     *          at all), and one for each group that the user belongs
     *          to.
     *
     *      --  Processes running on behalf of the superuser (root)
     *          will only have a single "0" subject handle.
     *
     *      --  A trusted process that was defined by an adnimistrator
     *          will contain an additional subject handle with the access
     *          permissions defined for it. For example, if the program
     *          "C:\MASTER\MASTER.EXE" is given specific permissions,
     *          there would be one subject handle representing the
     *          ACLs for that in addition to those of the user running
     *          it.
     *
     *      --  For processes that were started during system startup,
     *          the driver creates security contexts with a single "0"
     *          (root) subject handle when XWPShell passes a list of
     *          PIDs to the driver with the "initialization" ring-0 API.
     *          As a result, all processes started through CONFIG.SYS
     *          are considered trusted processes.
     */

    typedef struct _XWPSECURITYCONTEXT
    {
        ULONG           ulPID;          // process ID

        ULONG           cSubjects;      // no. of subject handles in this context

        HXSUBJECT       aSubjects[1];   // array of cSubjects subject handles,
                                        // determining the permissions of this
                                        // process

    } XWPSECURITYCONTEXT, *PXWPSECURITYCONTEXT;

    APIRET scxtInit(VOID);

    APIRET scxtCreateSecurityContext(ULONG ulPID,
                                     ULONG cSubjects,
                                     HXSUBJECT *paSubjects);

    APIRET scxtFindSecurityContext(PXWPSECURITYCONTEXT pContext);

    APIRET scxtVerifyAuthority(PXWPSECURITYCONTEXT pContext,
                               ULONG flActions);

    /* ******************************************************************
     *
     *   Permissions
     *
     ********************************************************************/

    /*
     *      The following global permissions exist:
     *
     */

    #define XWPPERM_CREATEUSER          0x00000100
                    // permission to create new users

    #define XWPPERM_CHANGEUSER          0x00000200
                    // permission to change user information

    #define XWPPERM_CHANGEUSERPERM      0x00000400
                    // permission to change permissions for a
                    // given user

    #define XWPPERM_DELETEUSER          0x00000800
                    // permission to delete existing users

    #define XWPPERM_CREATEGROUP         0x00001000
                    // permission to create new groups

    #define XWPPERM_CHANGEGROUP         0x00002000
                    // permission to change group information

    #define XWPPERM_CHANGEGROUPERM      0x00000400
                    // permission to change permissions for a
                    // given group

    #define XWPPERM_DELETEGROUP         0x00008000
                    // permission to delete existing groups

    #define XWPPERM_QUERYUSERINFO       0x00010000
                    // permission to retrieve user, group, and membership info

    /*
     *      The following permissions are defined per resource:
     *
     */

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
                    // authority for a given resource.
    #define XWPACCESS_ALL              0x7F
                    // Permission to read, write, create, execute,
                    // or delete a resource, or to modify attributes
                    // or permissions.

    APIRET saclInit(VOID);

    /*
     *@@ ACCESS:
     *
     *@@added V1.0.1 (2003-01-05) [umoeller]
     */

    typedef struct _ACCESS
    {
        BYTE        fbAccess;           // XWPACCESS_* flags
        HXSUBJECT   hSubject;           // subject handle; this is -1 if an entry
                                        // exists for this resource but the user or
                                        // group is not currently in use (because no
                                        // such user is logged on)
    } ACCESS, *PACCESS;

    /*
     *@@ RESOURCEACL:
     *      definition of a resource entry in the system access control
     *      list (ACL).
     *
     *      At ring 0, the driver has a list of all RESOURCEACL entries
     *      defined for the system. Each entry in turn has an array of
     *      ACCESS structs listing the subject handles for the resource,
     *      for example, defining that subject handle 1 (which could be
     *      representing a user) may read and write this resource, subject
     *      handle 2 (representing one of the groups the user belongs to)
     *      may execute, and so on.
     *
     *      There will only be one entry for any resource per subject.
     *      As a result, if the permissions for a resource are changed,
     *      the existing entry must be found and refreshed to avoid
     *      duplicates.
     *
     *      The global ACL table is build by XWPShell whenever it needs
     *      updating and passed down to the driver for future use. It
     *      will need rebuilding whenever a subject handle gets created
     *      or when access permissions are changed by an administrator.
     *
     *      The table will be build as follows by XWPShell:
     *
     *      1)  XWPShell loads the file defining the ACLs for the entire
     *          system.
     *
     *          For each definition in the file, it builds a RESOURCEACL
     *          entry. It checks the permissions defined for the resource
     *          in the file and sets up the array of ACCESS structures for
     *          the resource. If a permission was defined for a user for
     *          which a subject handle already exists (because the user
     *          is already logged on), that subject handle is stored.
     *          If a definition exists but none of the permissions apply
     *          to any of the current users (because those users are not
     *          logged on, or the groups are not in use yet), a dummy
     *          entry with a -1 subject handle is created to block access
     *          to the resource (see the algorithm description below).
     *
     *      2)  XWPShell then sends the system ACL list down to the driver.
     *
     *      During authorization, for any event, the driver first checks
     *      if a null ("root") subject handle exists in the process's
     *      security context. If so, access is granted unconditionally.
     *
     *      Otherwise, ACLs apply to all subdirectories too, unless a more
     *      specific ACL entry is encountered. In other words,
     *      the driver authorizes events bottom-up in the following order:
     *
     *      1)  It checks for whether an ACL entry for the given resource
     *          exists in the ACL table.
     *
     *          If any ACL entry was found for the resource, access is
     *          granted if any ACL entry allowed access for one of the
     *          subjects in the process's security context. Access is denied
     *          if ACL entries existed for the resource but none allowed access,
     *          which includes the "blocker" -1 entry described above.
     *
     *          In any case, the search stops if an ACL entry was found
     *          in the table, and access is either granted or denied.
     *
     *      2)  Only if no entry was found for the resource in any of the
     *          subject infos, we climb up to the parent directory and
     *          search all subject infos again. Go back to (1).
     *
     *      3)  After the root directory has been processed and still no
     *          entry exists, access is denied.
     *
     *      Examples:
     *
     *      User "dumbo" belongs to the groups "users" and "admins".
     *      The following ACLs are defined:
     *
     *      --  "users" may read "C:\DIR",
     *
     *      --  "admins" may read and write "C:\",
     *
     *      --  "admins" may create directories in "C:\DIR",
     *
     *      --  "otheruser" may read "C:\OTHERDIR".
     *
     *      Assuming that only "dumbo" is logged on presently and the following
     *      subject handles have thus been created:
     *
     *      --  1 for user "dumbo",
     *
     *      --  2 for group "users",
     *
     *      --  3 for group "admins",
     *
     *      the system ACL table will contain the following entries:
     *
     *      --  "C:\": 3 (group "admins") may read and write;
     *
     *      --  "C:\DIR": 2 (group "users") may read, 3 (group "admins) may
     *          create directories;
     *
     *      --  "C:\OTHERDIR": this will have a dummy -1 entry with no permissions
     *          because the only ACL defined is that user "otheruser" may read, and
     *          that user is not logged on.
     *
     *      1)  Assume a process running on behalf of "dumbo" wants to open
     *          C:\DIR\SUBDIR\TEXT.DOC for reading.
     *          Since the security context of "dumbo" has the three subject
     *          handles for user "dumbo" (1) and the groups "users" (2) and
     *          "admins" (3), the following happens:
     *
     *          a)  We check the system ACL table for "C:\DIR\SUBDIR\TEXT.DOC"
     *              and find no ACL entry.
     *
     *          b)  So we take the parent directory, "C:\DIR\SUBDIR",
     *              and again we find nothing.
     *
     *          c)  Taking the next parent, "C:\DIR\", we find the above two
     *              subject handles: since "users" (2) may read, and that is
     *              part of the security context, we grant access.
     *
     *      2)  Now assume that the same process wants to write the file back:
     *
     *          a)  Again, we find no ACL entries for "C:\DIR\SUBDIR\TEXT.DOC"
     *              or "C:\DIR\SUBDIR".
     *
     *          b)  Searching for "C:\DIR", we find that "users" (2) may only read,
     +              but not write. Also, "admins" (3) may create directories under
     *              "C:\DIR", which is not sufficient either. Since no other entries
     *              exist for "C:\DIR"  that would permit write, we deny access.
     *              That "admins" may write to "C:\" does not help since more
     *              specific entries exist for "C:\DIR".
     *
     *      3)  Now assume that the same process wants to create a new directory
     *          under "C:\DIR\SUBDIR".
     *
     *          a)  Again, we find no ACL entries for "C:\DIR\SUBDIR".
     *
     *          b)  Searching for "C:\DIR", we find that "users" may only read,
     *              which does not help. However, "admins" may create directories,
     *              so we grant access.
     *
     *      4)  Assume now that the process wants to create a new directory under
     *          "C:\OTHERDIR".
     *
     *          We find the ACL entry for "C:\OTHERDIR" and see only the -1
     *          subject handle (for user "otheruser", who's not logged on),
     *          and since no other permissions are set for us, we deny access.
     *
     *@@added V1.0.1 (2003-01-05) [umoeller]
     */

    typedef struct _RESOURCEACL
    {
        USHORT      cAccesses;          // no. of entries in aAccesses array

        USHORT      cbName;             // length of szName field, including null byte
        CHAR        szName;             // fully qualified filename of this resource
                                        // (zero-terminated)
        ACCESS      aAccesses[1];

    } RESOURCEACL, *PRESOURCEACL;

    /* ******************************************************************
     *
     *   User Database (userdb.c)
     *
     ********************************************************************/

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

    /*
     *@@ XWPUSERINFO:
     *      description of a user in the user database.
     *      See userdb.c for details.
     */

    typedef struct _XWPUSERINFO
    {
        XWPSECID    uid;                // user's ID (unique); 0 for root
        CHAR        szUserName[XWPSEC_NAMELEN];
        CHAR        szFullName[XWPSEC_FULLNAMELEN];       // user's clear name
    } XWPUSERINFO, *PXWPUSERINFO;

    /*
     *@@ XWPMEMBERSHIP:
     *
     *@@added V1.0.1 (2003-01-05) [umoeller]
     */

    typedef struct _XWPMEMBERSHIP
    {
        ULONG       cGroups;
        XWPSECID    aGIDs[1];
    } XWPMEMBERSHIP, *PXWPMEMBERSHIP;

    /*
     *@@ XWPUSERDBENTRY:
     *
     *@@added V1.0.1 (2003-01-05) [umoeller]
     */

    typedef struct _XWPUSERDBENTRY
    {
        ULONG           cbStruct;
        XWPUSERINFO     User;
        CHAR            szPassword[XWPSEC_NAMELEN];
        XWPMEMBERSHIP   Membership;
    } XWPUSERDBENTRY, *PXWPUSERDBENTRY;

    APIRET sudbInit(VOID);

    APIRET sudbAuthenticateUser(PXWPUSERINFO pUserInfo,
                                PCSZ pcszPassword);

    APIRET sudbQueryName(BYTE bType,
                         XWPSECID ID,
                         PSZ pszName,
                         ULONG cbName);

    APIRET sudbQueryID(BYTE bType,
                       PCSZ pcszName,
                       XWPSECID *pID);

    APIRET sudbQueryGroups(PULONG pcGroups,
                           PXWPGROUPDBENTRY *ppaGroups);

    APIRET sudbQueryUser(XWPSECID uid,
                         PXWPUSERDBENTRY *ppUser);

    APIRET sudbCreateUser(PXWPUSERDBENTRY pUserInfo);

    APIRET sudbDeleteUser(XWPSECID uid);

    APIRET sudbCreateGroup(PXWPGROUPDBENTRY pGroupInfo);

    APIRET sudbDeleteGroup(XWPSECID gid);

    APIRET sudbQueryUsers(PULONG pcUsers,
                          PXWPUSERDBENTRY *ppaUsers);

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
        ULONG       cbStruct;       // size of entire structure
        XWPSECID    uid;            // user's ID
        CHAR        szUserName[XWPSEC_NAMELEN];
        ULONG       cSubjects;      // no. of entries in aSubjects array
        HXSUBJECT   aSubjects[1];   // array of subject handles of this user; one "0" entry if root
    } XWPLOGGEDON, *PXWPLOGGEDON;

    APIRET slogInit(VOID);

    APIRET slogLogOn(PCSZ pcszUserName,
                     PCSZ pcszPassword,
                     BOOL fLocal,
                     XWPSECID *puid);

    APIRET slogLogOff(XWPSECID uid);

    APIRET slogQueryLocalUser(PXWPLOGGEDON pLoggedOnLocal);

    /* ******************************************************************
     *
     *   Errors
     *
     ********************************************************************/

    #define ERROR_XWPSEC_FIRST          31000

    #define XWPSEC_INTEGRITY            (ERROR_XWPSEC_FIRST + 1)
    #define XWPSEC_INVALID_DATA         (ERROR_XWPSEC_FIRST + 2)
    #define XWPSEC_CANNOT_GET_MUTEX     (ERROR_XWPSEC_FIRST + 3)
    #define XWPSEC_CANNOT_START_DAEMON  (ERROR_XWPSEC_FIRST + 4)

    #define XWPSEC_INSUFFICIENT_AUTHORITY  (ERROR_XWPSEC_FIRST + 5)

    #define XWPSEC_HSUBJECT_EXISTS      (ERROR_XWPSEC_FIRST + 6)
    #define XWPSEC_INVALID_HSUBJECT     (ERROR_XWPSEC_FIRST + 7)

    #define XWPSEC_INVALID_PID          (ERROR_XWPSEC_FIRST + 10)
    #define XWPSEC_NO_CONTEXTS          (ERROR_XWPSEC_FIRST + 11)

    #define XWPSEC_USER_EXISTS          (ERROR_XWPSEC_FIRST + 20)
    #define XWPSEC_NO_USERS             (ERROR_XWPSEC_FIRST + 21)
    #define XWPSEC_NO_GROUPS            (ERROR_XWPSEC_FIRST + 22)
    #define XWPSEC_INVALID_USERID       (ERROR_XWPSEC_FIRST + 23)
    #define XWPSEC_INVALID_GROUPID      (ERROR_XWPSEC_FIRST + 24)

    #define XWPSEC_NOT_AUTHENTICATED    (ERROR_XWPSEC_FIRST + 30)
    #define XWPSEC_NO_USER_PROFILE      (ERROR_XWPSEC_FIRST + 31)       // V0.9.19 (2002-04-02) [umoeller]
    #define XWPSEC_CANNOT_START_SHELL   (ERROR_XWPSEC_FIRST + 32)
    #define XWPSEC_INVALID_PROFILE      (ERROR_XWPSEC_FIRST + 33)
    #define XWPSEC_NO_LOCAL_USER        (ERROR_XWPSEC_FIRST + 34)

    #define XWPSEC_DB_GROUP_SYNTAX      (ERROR_XWPSEC_FIRST + 35)
    #define XWPSEC_DB_USER_SYNTAX       (ERROR_XWPSEC_FIRST + 36)
    #define XWPSEC_DB_INVALID_GROUPID   (ERROR_XWPSEC_FIRST + 37)

    #define XWPSEC_DB_ACL_SYNTAX        (ERROR_XWPSEC_FIRST + 40)

    #define XWPSEC_RING0_NOT_FOUND      (ERROR_XWPSEC_FIRST + 50)

    #define XWPSEC_QUEUE_INVALID_CMD    (ERROR_XWPSEC_FIRST + 51)

    #define XWPSEC_NAME_TOO_LONG        (ERROR_XWPSEC_FIRST + 52)       // V1.0.1 (2003-01-05) [umoeller]
    #define XWPSEC_STRUCT_MISMATCH      (ERROR_XWPSEC_FIRST + 52)       // V1.0.1 (2003-01-05) [umoeller]
            // sizeof XWPSHELLQUEUEDATA does not match (different versions?)

    #define ERROR_XWPSEC_LAST           (ERROR_XWPSEC_FIRST + 52)

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
     *      any process to receive more information and
     *      is also checked by XWorkplace at WPS startup
     *      to determine whether XWPShell is running or
     *      the WPS should be running in single-user
     *      mode.
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
     *   XWPShell Queue (Ring 3 API)
     *
     ********************************************************************/

    /*
     *      Ring-3 APIs are implemented via a regular OS/2 Control
     *      Program queue. Programs can call these APIs by allocating
     *      a chunk of shared memory, writing the pointer to that
     *      with a command code into this queue, and blocking on an
     *      event semaphore, which gets posted by XWPShell when the
     *      command has been processed. XWPShell then writes a
     *      result code (NO_ERROR or an error code) and possibly
     *      result data back into that shared memory.
     *
     *      Since OS/2 reports the PID of the queue writer with the
     *      queue APIs, XWPShell can validate whether the writing
     *      process is authorized to perform a certain event. For
     *      example, when a process issues the "create new user"
     *      command, the queue thread in XWPShell will check the
     *      writer's PID and validate the security context of that
     *      process for whether the subjects of that process include
     *      sufficient permission for that command.
     *
     *      The following commands are necessary:
     *
     *      --  Query security context for a PID (no permissions required)
     *
     *      --  Create user (admin permission required)
     *
     *      --  Create group (admin permission required)
     *
     *      --  Set User membership in groups
     *
     *      --  Query user info
     *
     *      --  Query group info
     *
     *      --  Query permissions for a resource
     *
     *      --  Set permissions for a resource
     *
     */

    #define QUEUE_XWPSHELL        "\\QUEUES\\XWORKPLC\\XWPSHELL.QUE"
            // queue name of the master XWPShell queue

    /*
     *@@ QUEUEUNION:
     *
     *@@added V0.9.11 (2001-04-22) [umoeller]
     */

    typedef union _QUEUEUNION
    {
        #define QUECMD_QUERYLOCALLOGGEDON           1
            // return data for user that is
            // currently logged on locally.
            // Required authority: None.
            // Possible error codes: see slogQueryLocalUser.
        XWPLOGGEDON     QueryLocalLoggedOn;

        #define QUECMD_QUERYUSERS                   2
            // return info for all users that
            // are defined in the userdb.
            // Required authority: administrator.
            // Possible error codes: see sudbQueryUsers.
            // If NO_ERROR is returned, paUsers has been
            // set to an array of cUsers XWPUSERDBENTRY
            // structures as shared memory given to the
            // caller. The caller must issue DosFreeMem.
            // Note that the szPassword field for each
            // user is always nulled out.
        struct
        {
            ULONG               cUsers;
            PXWPUSERDBENTRY     paUsers;
        } QueryUsers;

        #define QUECMD_QUERYGROUPS                  3
            // return info for all groups that
            // are defined in the userdb.
            // Required authority: administrator.
            // Possible error codes: see sudbQueryGroups.
            // If NO_ERROR is returned, paGroups has been
            // set to an array of cGroups XWPGROUPDBENTRY
            // structures as shared memory given to the
            // caller. The caller must issue DosFreeMem.
        struct
        {
            ULONG               cGroups;
            PXWPGROUPDBENTRY    paGroups;
        } QueryGroups;

        #define QUECMD_QUERYPROCESSOWNER            4
            // return the uid of the user who owns
            // the given process.
            // Required authority: administrator.
        struct
        {
            ULONG               ulPID;      // in: PID to query
            XWPSECID            uid;        // out: uid of owner, if NO_ERROR is returned
        } QueryProcessOwner;

        #define QUECMD_CREATEUSER                   5

        struct
        {
            CHAR        szUserName[XWPSEC_NAMELEN];         // user name
            CHAR        szFullName[XWPSEC_FULLNAMELEN];     // user's clear name
            CHAR        szPassword[XWPSEC_NAMELEN];         // password
            XWPSECID    uidCreated; // out: uid of new user
        } CreateUser;

        #define QUECMD_SETUSERDATA                  6

        struct
        {
            XWPSECID    uid;
            CHAR        szUserName[XWPSEC_NAMELEN];         // user name
            CHAR        szFullName[XWPSEC_FULLNAMELEN];     // user's clear name
        } SetUserData;

        #define QUECMD_DELETEUSER                   7

        XWPSECID        uidDelete;

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
        ULONG       cbStruct;           // size of XWPSHELLQUEUEDATA struct (for safety)

        ULONG       ulCommand;          // in: one of the QUECMD_* values

        HEV         hevData;            // in: event semaphore posted
                                        // when XWPShell has produced
                                        // the data

        APIRET      arc;                // out: error code set by XWPShell;
                                        // if NO_ERROR, the following is valid

        QUEUEUNION  Data;               // out: data, format depends on ulCommand

    } XWPSHELLQUEUEDATA, *PXWPSHELLQUEUEDATA;

/* ******************************************************************
 *
 *   Ring-0 (driver) APIs
 *
 ********************************************************************/

/*
 *      Ring 0 interfaces required to be called from XWPShell:
 *
 *      --  Initialization: to be called exactly once when
 *          XWPShell starts up. This call enables local security
 *          and switches the driver into authorization mode:
 *          from then on, all system events are authenticated
 *          via the KPI callouts.
 *
 *          With this call, XWPShell must pass down an array of
 *          PIDs that were already running when XWPShell was
 *          started, including XWPShell itself. For these
 *          processes, the driver will create security contexts
 *          as trusted processes.
 *
 *          In addition, XWPShell sends down an array with all
 *          definitions of trusted processes so that the driver
 *          can create special security contexts for those.
 *
 *      --  Query security context: XWPShell needs to be able
 *          to retrieve the security context of a given PID
 *          from the driver to be able to authorize ring-3 API
 *          calls such as "create user" or changing permissions.
 *
 *      --  Set security context: changes the security context
 *          of an existing process. This is used by XWPShell
 *          to change its own context when the local user logs
 *          on. In addition, XWPShell will call this when a
 *          third party process has requested to change its
 *          context and this request was authenticated.
 *
 *      --  ACL table: Whenever subject handles are created or
 *          deleted, XWPShell needs to rebuild the system ACL
 *          table to contain the fresh subject handles and
 *          pass them all down to the driver.
 *
 *      --  Refresh process list: XWPShell needs to periodically
 *          call into the driver to pass it a list of processes
 *          that are currently running. Since there is no callout
 *          for when a process has terminated, the driver will
 *          end up with plenty of zombie PIDs after a while. This
 *          call will also be necessary before a user logs off
 *          after his processes have been terminated to make
 *          sure that subject handles are no longer in use.
 */

#endif

#if __cplusplus
}
#endif

