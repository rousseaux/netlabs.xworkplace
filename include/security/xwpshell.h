
/*
 *@@sourcefile xwpshell.h:
 *      private header file for the XWPShell implementation.
 *
 *@@include #define INCL_DOSSEMAPHORES
 *@@include #include <os2.h>
 *@@include #include "helpers\tree.h"           // for ACLDB definitions
 *@@include #include "helpers\xwpsecty.h"
 *@@include #include "security\xwpshell.h"
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

#ifndef XWPSHELL_HEADER_INCLUDED
    #define XWPSHELL_HEADER_INCLUDED

    /* ******************************************************************
     *
     *   XWPShell PM constants
     *
     ********************************************************************/

    #define WC_SHELL_OBJECT         "XWPShellObject"

    #define XM_LOGON                (WM_USER + 1)
    #define XM_LOGOFF               (WM_USER + 2)
    #define XM_ERROR                (WM_USER + 3)
    #define XM_MESSAGE              (WM_USER + 4)

    #define IDD_LOGON               200
    #define IDDI_USERENTRY          201
    #define IDDI_PASSWORDENTRY      202
    #define IDDI_USERTEXT           204
    #define IDDI_PASSWORDTEXT       205

    /* ******************************************************************
     *
     *   ACL database implementation
     *
     ********************************************************************/

    #ifdef XWPTREE_INCLUDED

        /*
         *@@ ACLDBPERM:
         *
         *@@added V1.0.1 (2003-01-05) [umoeller]
         */

        typedef struct _ACLDBPERM
        {
            BYTE    bType;
                        // one of SUBJ_USER, SUBJ_GROUP, or SUBJ_PROCESS
            ULONG   id; // user, group, or process ID
            BYTE    fbPerm;
        } ACLDBPERM, *PACLDBPERM;

        /*
         *@@ ACLDBTREENODE:
         *
         *@@added V1.0.1 (2003-01-05) [umoeller]
         */

        typedef struct _ACLDBTREENODE
        {
            TREE        Tree;
            LINKLIST    llPerms;
            USHORT      usResNameLen;       // length of szResource, _ex_cluding null terminator
            CHAR        szResName[1];       // resource name, zero-terminated string
                                            // ("A:" or "A:\DIRECTORY" or "A:\DIR\FILE.TXT")
        } ACLDBTREENODE, *PACLDBTREENODE;

        APIRET saclLoadDatabase(PULONG pulLineWithError);

    #endif

    /* ******************************************************************
     *
     *   User database implementation
     *
     ********************************************************************/

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
     *   Security contexts
     *
     ********************************************************************/

    APIRET scxtInit(VOID);

    VOID scxtExit(VOID);

    APIRET scxtQueryStatus(PXWPSECSTATUS pStatus);

    #ifdef XWPTREE_INCLUDED
        APIRET scxtCreateACLEntry(PACLDBTREENODE pNewEntry);
    #endif

    APIRET scxtCreateSubject(PXWPSUBJECTINFO pSubjectInfo);

    APIRET scxtDeleteSubject(HXSUBJECT hSubject);

    APIRET scxtQuerySubjectInfo(PXWPSUBJECTINFO pSubjectInfo);

    APIRET scxtFindSubject(BYTE bType,
                           XWPSECID id,
                           HXSUBJECT *phSubject);

    APIRET scxtCreateSecurityContext(ULONG ulPID,
                                     ULONG cSubjects,
                                     HXSUBJECT *paSubjects);

    APIRET scxtFindSecurityContext(ULONG ulPID,
                                   PXWPSECURITYCONTEXT *ppContext);

    APIRET scxtVerifyAuthority(PXWPSECURITYCONTEXT pContext,
                               ULONG flActions);

    APIRET scxtRefresh(VOID);

    APIRET scxtQueryPermissions(PCSZ pcszResource,
                                ULONG cSubjects,
                                const HXSUBJECT *paSubjects,
                                PULONG pulAccess);

    /* ******************************************************************
     *
     *   Logons
     *
     ********************************************************************/

    APIRET slogInit(VOID);

    APIRET slogLogOn(PCSZ pcszUserName,
                     PCSZ pcszPassword,
                     BOOL fLocal,
                     XWPSECID *puid);

    APIRET slogLogOff(XWPSECID uid);

    APIRET slogQueryLocalUser(XWPSECID *puid);

    APIRET slogQueryLogon(XWPSECID uid,
                          PXWPLOGGEDON *ppLogon);

#endif
