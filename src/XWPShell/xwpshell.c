
/*
 *@@sourcefile xwpshell.c:
 *      startup wrapper around PMSHELL.EXE. Besides functioning
 *      as a shell starter, XWPShell also maintains users,
 *      processes, and access control lists to interact with
 *      the ring-0 device driver (XWPSEC32.SYS).
 *
 *      When XWPShell is installed thru CONFIG.SYS (for details,
 *      see below), it initializes PM, attempts to open
 *      XWPSEC32.SYS, and then does the following:
 *
 *      1. It displays the logon dialog for user authentication.
 *
 *      2. If the user has been authenticated, XWPShell then
 *         changes the user profile (replacement OS2.INI) by
 *         calling PrfReset and starts the desired shell process
 *         with the following environment:
 *
 *         -- USER is set to the user name;
 *         -- USERID is set to the user ID (uid);
 *         -- GROUP is set to the group name;
 *         -- GROUPID is set to the user's group ID (gid);
 *         -- HOME is set to the user's home directory;
 *         -- OS2_INI is set to the user's OS2.INI file. This
 *              does not affect the profile (which has been
 *              changed using PrfReset before), but is still
 *              changed in case any program references this
 *              variable.
 *
 *         All processes started by the WPS will inherit this
 *         environment since the WPS always copies its environment
 *         to child processes.
 *
 *      3. When the WPS terminates, go back to 1.
 *
 *      Note that Security Enabling Services (SES) is NOT used
 *      for user logon and authentication. In other words, this
 *      program serves neither as an SES System Logon Authority
 *      (SLA) nor a User Identification Authority (UIA). I have
 *      found SES Logon Shell Services (LSS) too buggy to be useful,
 *      but maybe I was too dumb to find out how to use them. But
 *      then again, these APIs are too complicated and error-prone
 *      to be used by ordinary humans.
 *
 *      So instead, I have rewritten the LSS functionality for
 *      XWPShell. SES is only used for kernel interfaces (KPIs)
 *      in XWPSEC32.SYS. As a result, the SECURE.SYS text file
 *      in \OS2\SECURITY\SESDB is not used either.
 *
 *      <B>Concepts:</B>
 *
 *      To implement XWPSec, I have reviewed the concepts of Linux,
 *      SES, and OS/2 LAN Server (Warp Server for e-Business).
 *      See subjects.c, users.c, and userdb.c for a discussion of
 *      security concepts which came out of this review.
 *
 *      As a summary, XWPSec follows the SES security models, with a
 *      few simplifications. To make processing more efficient, user
 *      IDs and group IDs have been added (like Linux uid and gid) to
 *      allow looking up data based on numbers instead of names.
 *
 *      <B>Recommended reading:</B>
 *
 *      -- "SES Developer's Guide" (IBM SG244668 redbook). This
 *         sucks, but it's the only source for SES information.
 *
 *      -- "Network Administrator Tasks" (BOOKS\A3AA3MST.INF
 *         on a WSeB installation). A very good book.
 *
 *      -- Linux manpages for group(5) and passwd(5); Linux info
 *         page for chmod (follow the "File permissions" node).
 *
 *      <B>XWPSec Modules:</B>
 *
 *      XWPSec consists of the following modules:
 *
 *      1. XWPShell, which can be compared to SES LSS. This
 *         consists of the following:
 *
 *         -- XWPShell (shell starter and logon dialog). This
 *            also manages all users which are currently logged
 *            on (either locally or remotely, via network).
 *
 *            This is the only part which needs PM. As a result,
 *            this can be rewritten for a command-line logon
 *            at system startup.
 *
 *         -- Subject handles. This references the user database
 *            (UserDB) and allows the ACL database (ACLDB) to be
 *            notified of subject handle creation and deletion.
 *
 *            Responsibilities:
 *
 *            1)   Allowing an ACLDB to be registered for
 *                 subject creation and deletion.
 *
 *            2)   Notification of the ACLDB on subject handle
 *                 creation and deletion.
 *
 *            See subjects.c.
 *
 *         -- Security context management. A security context
 *            is associated with each process on the system
 *            and determines the process's access rights. A
 *            security context consists of a user subject
 *            handle and a group subject handle plus authority
 *            flags.
 *
 *            Responsibilities:
 *
 *            1)   Creation and deletion of security contexts
 *                 upon notification of process creation / exit.
 *
 *            2)   Returning a security context for a given PID.
 *
 *            See contexts.c.
 *
 *         -- Currently-logged-on users database.
 *            This logs users on and off and keeps track of
 *            all currently logged-on users (either locally
 *            or via network).
 *
 *            Responsibilities:
 *
 *            1)   Modify the calling process's security
 *                 context.
 *
 *            See loggedon.c.
 *
 *         -- XWPShell ring-3 daemon thread. This is in contact
 *            with the ring-0 driver, receives notification
 *            about security-related events, and queries the
 *            ACLDB for each access request.
 *
 *            See fntRing3Daemon in this file.
 *
 *      2. OS/2 kernel security hooks in a ring-0 driver. This
 *         must implement:
 *
 *         -- Process creation/termination notification. This
 *            is needed so that security contexts can be
 *            attached to processes.
 *
 *         -- Resource protection. The ring-0 driver uses the
 *            SES kernel hooks to be able to turn down access to
 *            local resources (e.g. DosOpen, DosExecPgm).
 *
 *            The ring-0 driver communicates with the XWPShell
 *            ring-3 daemon thread, which in turn contacts the
 *            ACL database to give or turn down access to a
 *            resource based on the process's security context.
 *
 *      3. "Black boxes", which can be easily rewritten. These are:
 *
 *         -- User database (userdb.c). This can be replaced if
 *            some other method of storing user and group data
 *            is desired (e.g. for accessing a Warp Server user
 *            database).
 *
 *            This can be compared to an SES user identification
 *            authority (UIA).
 *
 *            The current implementation maintains user and
 *            group information in an XML file called xwpusers.xml.
 *            This allows XWPSec to be installed on machines without
 *            network functionality as well.
 *
 *         -- ACL database. This can be replaced if some other method
 *            of storing ACL's is desired, e.g. for interfacing
 *            WarpServer ACL's or HPFS386 local security (which are
 *            tied together, AFAIK).
 *
 *            This can be compared to an SES access control authority
 *            (ACA).
 *
 *            Responsibilities:
 *
 *            1)   Creation and deletion of ACL entries.
 *
 *            2)   Loading and unloading ACL entries on subject
 *                 handle creation and deletion.
 *
 *            3)   Verifying access rights for access to resources
 *                 (such as DosOpen, DosExecPgm) based on a security
 *                 context. See saclVerifyAccess.
 *
 *      <B>Example:</B>
 *
 *      1.  XWPShell starts up on system bootup.
 *
 *      2.  XWPShell displays the logon dialog.
 *
 *      3.  User "user" types in name and password. XWPShell
 *          calls sudbAuthenticateUser to verify that user
 *          name and password are correct.
 *
 *          If the user has been authenticated, the UserDB returns
 *          the user ID (uid) and group ID (gid) for the user.
 *          Let's assume that "user" belongs to group "users".
 *
 *          If the user has not been authenticated, go back to
 *          2.
 *
 *      4.  XWPShell calls slogLogOn to log the user onto the
 *          system. This creates two subject handles: one for
 *          "user", one for the group "users". (If the group
 *          "users" already has a subject handle, e.g. because
 *          another user of that group has already logged on,
 *          no subject handle will be created.)
 *
 *      5.  The ACLDB is notified of each subject handle creation.
 *          With each subject handle creation, it loads the
 *          access rights (ACL entries) associated with the handle.
 *          For the "user" creation, it will load ACL entries
 *          for "user". For the "users" group creation, it will
 *          load ACL entries for the "users" group.
 *
 *          (Here we already see the advantage of subject handles.
 *          If a second user of the "users" group logs on, no
 *          subject handle needs to be created. The ACL entries
 *          are already loaded.)
 *
 *          The ACLDB will build a hash table of subject handles
 *          to be able to quickly find ACL entries later.
 *
 *      6.  Logon is now complete.
 *
 *      7.  XWPShell then starts the user shell (normally the WPS).
 *
 *      8.  This already implies access to a resource, since the
 *          ring-0 driver has hooked DosExecPgm. The ring-3 daemon
 *          is unblocked and contacts the ACLDB, which verifies
 *          access to that resource. The ring-0 driver then
 *          returns NO_ERROR or ERROR_ACCESS_DENIED to the OS/2
 *          kernel.
 *
 *          For obvious reasons, the OS/2 directories should be
 *          given read and execute rights for all users. So in
 *          this case, access is granted, and the WPS starts up.
 *
 *          (For each file and directory accessed by the WPS, such
 *          as the Desktop hierachies, SOM DLLs and such, this is
 *          repeated.)
 *
 *      9.  Assume that the user attempts to delete a file from
 *          the OS/2 directory. Step 8 is processed again (this
 *          time the DosDelete hook is called in the ring-0 driver),
 *          and since the user does not have the proper access
 *          rights for the OS/2 directory, ERROR_ACCESS_DENIED
 *          is returned.
 *
 *      10. Now, "user" logs off. The subject handles for "user"
 *          and "users" are deleted (if no other user of the
 *          "users" group is logged on), and the ACLDB is notified
 *          so it can free the ACL entries associated with them.
 *
 *          Go back to 2.
 *
 *      <B>XWPShell Setup:</B>
 *
 *      In CONFIG.SYS, set RUNWORKPLACE to XWPSHELL.EXE's full path.
 *      This will make XWPShell initialize PM.
 *
 *      Put the user database (xwpusers.xml) into ?:\OS2 on your
 *      boot drive.
 *
 *      Debug setup:
 *
 *      Instead of SET RUNWORKPLACE=...\xwpshell.exe, use cmd.exe
 *      and start XWPSHELL.EXE manually from the command line.
 *
 *      Configuration via environment variables:
 *
 *      1.  In CONFIG.SYS, set XWPSHELL to PMSHELL.EXE's full path.
 *          The XWPSHELL environment variable specifies the executable
 *          to be started by XWPShell. If XWPSHELL is not defined,
 *          PMSHELL.EXE is used.
 *
 *      2.  In CONFIG.SYS, set XWPHOME to the full path of the HOME
 *          directory, which holds all user desktops and INI files.
 *          If this is not specified, this defaults to ?:\home on the
 *          boot drive.
 *
 *      3.  In CONFIG.SYS, set XWPUSERDB to the directory where
 *          XWPShell should keep its user data base files (xwpusers.xml
 *          and xwpusers.acc). If this is not specified, this defaults
 *          to ?:\OS2 on the boot drive.
 *
 *@@added V0.9.5 [umoeller]
 *@@header "security\xwpsecty.h"
 *@@header "xwpshell.h"
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

#define  INCL_WIN
#define  INCL_WINWORKPLACE
#define  INCL_DOS
#define  INCL_DOSERRORS
#include <os2.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <io.h>
#include <ctype.h>

#include "setup.h"

#include "helpers\cnrh.h"
#include "helpers\dosh.h"
#include "helpers\eah.h"
#include "helpers\prfh.h"
#include "helpers\procstat.h"
#include "helpers\stringh.h"
#include "helpers\winh.h"
#include "helpers\threads.h"

#include "bldlevel.h"
#include "dlgids.h"

#include "security\xwpsecty.h"
#include "security\ring0api.h"

#include "xwpshell.h"

/* ******************************************************************
 *                                                                  *
 *   Global variables                                               *
 *                                                                  *
 ********************************************************************/

HWND        G_hwndShellObject = NULLHANDLE;
    // object window for communication

// user shell currently running:
// this must only be modified by thread-1!!
HAPP        G_happWPS = NULLHANDLE;
    // HAPP of WPS process or NULLHANDLE if the WPS is not running
PSZ         G_pszEnvironment = NULL;
    // environment of user shell

FILE        *G_LogFile = NULL;

HFILE       G_hfSec32DD = NULLHANDLE;

THREADINFO  G_tiRing3Daemon = {0};
BOOL        G_fRing3DaemonRunning = FALSE;
HEV         G_hevCallback = NULLHANDLE;

PXWPSHELLSHARED G_pXWPShellShared = 0;

/* ******************************************************************
 *                                                                  *
 *   Helpers                                                        *
 *                                                                  *
 ********************************************************************/

/*
 *@@ Error:
 *
 */

VOID Error(const char* pcszFormat,
           ...)
{
    va_list     args;
    CHAR        szError[1000];
    va_start(args, pcszFormat);
    vfprintf(G_LogFile, pcszFormat, args);
    fprintf(G_LogFile, "\n");
    fflush(G_LogFile);
    va_end(args);
    /* winhDebugBox(NULLHANDLE,
             "XWPShell",
             szError); */
    DosBeep(100, 1000);
    WinPostMsg(G_hwndShellObject, WM_QUIT, 0, 0);
}

/* ******************************************************************
 *
 *   Local Logon Dialog
 *
 ********************************************************************/

/*
 *@@ fnwpLogonDlg:
 *      dialog proc for the logon dialog. This takes
 *      the user name and password from the user.
 */

static MRESULT EXPENTRY fnwpLogonDlg(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    static PXWPUSERDBENTRY puiLogon = NULL;

    switch (msg)
    {
        case WM_INITDLG:
            winhSetControlsFont(hwnd, 0, 1000, NULL);
            winhCenterWindow(hwnd);
            puiLogon = (PXWPUSERDBENTRY)mp2;
            memset(puiLogon, 0, sizeof(XWPUSERDBENTRY));
        break;

        case WM_COMMAND:

            switch (SHORT1FROMMP(mp1))
            {
                case DID_OK:
                {
                    HWND hwndUserID = WinWindowFromID(hwnd, IDDI_USERENTRY);
                    HWND hwndPassword = WinWindowFromID(hwnd, IDDI_PASSWORDENTRY);
                    if (WinQueryWindowText(hwndUserID,
                                           sizeof(puiLogon->szUserName),
                                           puiLogon->szUserName))
                        if (WinQueryWindowText(hwndPassword,
                                               sizeof(puiLogon->szPassword),
                                               puiLogon->szPassword))
                            WinDismissDlg(hwnd, DID_OK);

                break; }
            }

        break;

        case WM_CONTROL:        // Dialogkommando
            break;

        default:
            return WinDefDlgProc(hwnd, msg, mp1, mp2);
    }

    return (MRESULT)0;
}

/*
 *@@ SetNewUserProfile:
 *      changes the current user profile using
 *      PrfReset.
 *
 *      Returns a new environment strings buffer
 *      (for use with WinStartApp) or NULL on errors.
 *
 *@@added V0.9.4 (2000-07-19) [umoeller]
 */

PSZ SetNewUserProfile(HAB hab,
                      PCXWPLOGGEDON pNewUser)
{
    PSZ pEnv2 = NULL;
    DOSENVIRONMENT Env;
    if (doshGetEnvironment(&Env)
            == NO_ERROR)
    {
        ULONG cbEnv2 = 0;

        CHAR    szNewProfile[CCHMAXPATH];

        CHAR    szHomeBase[CCHMAXPATH];
        PSZ     pszHomeBase = getenv("XWPHOME");
        if (!pszHomeBase)
        {
            // XWPHOME not specified:
            sprintf(szHomeBase, "%c:\\home", doshQueryBootDrive());
            pszHomeBase = szHomeBase;
        }

        if (strcmp(pNewUser->szUserName, "root") == 0)
            // root gets default profile
            strcpy(szNewProfile,
                   getenv("USER_INI"));
        else
            // non-root:
            sprintf(szNewProfile,
                    "%s\\%s\\os2.ini",
                    pszHomeBase,
                    pNewUser->szUserName);

        if (access(szNewProfile, 0) == 0)
        {
            // user profile exists:
            // call PrfReset
            if (prfhSetUserProfile(hab,
                                   szNewProfile))
            {
                CHAR    szNewVar[1000];
                PSZ     p;
                sprintf(szNewVar, "USER_INI=%s", szNewProfile);
                doshSetEnvironmentVar(&Env, szNewVar);

                // set HOME var to home directory,
                // with Unix slashes
                sprintf(szNewVar, "HOME=%s/%s", pszHomeBase, pNewUser->szUserName);
                while (p = strchr(szNewVar, '\\'))
                    *p = '/';
                doshSetEnvironmentVar(&Env, szNewVar);

                // set USER var to user name
                sprintf(szNewVar, "USER=%s", pNewUser->szUserName);
                doshSetEnvironmentVar(&Env, szNewVar);

                // set USERID var to user name
                sprintf(szNewVar, "USERID=%d", pNewUser->uid);
                doshSetEnvironmentVar(&Env, szNewVar);

                // set USERSUBJECT var to user subject handle
                sprintf(szNewVar, "USERSUBJECT=%d", pNewUser->hsubjUser);
                doshSetEnvironmentVar(&Env, szNewVar);

                // set GROUP var to group name
                sprintf(szNewVar, "GROUP=%s", pNewUser->szGroupName);
                doshSetEnvironmentVar(&Env, szNewVar);

                // set GROUPID var to user name
                sprintf(szNewVar, "GROUPID=%d", pNewUser->gid);
                doshSetEnvironmentVar(&Env, szNewVar);

                // set GROUPSUBJECT var to group subject handle
                sprintf(szNewVar, "GROUPSUBJECT=%d", pNewUser->hsubjGroup);
                doshSetEnvironmentVar(&Env, szNewVar);

                doshConvertEnvironment(&Env, &pEnv2, &cbEnv2);
                    // pEnv != NULL now, which is returned
                    // == NO_ERROR!
            }
        } // end if (access(szNewProfile, 0) == 0)

        doshFreeEnvironment(&Env);

    } // end if (doshGetEnvironment(&Env)

    return (pEnv2);
}

/*
 *@@ StartUserShell:
 *
 */

APIRET StartUserShell(VOID)
{
    APIRET arc = NO_ERROR;
    PROGDETAILS pd;

    PSZ pszShell = getenv("XWPSHELL");

    if (pszShell == NULL)
        pszShell = "PMSHELL.EXE";

    memset(&pd, 0, sizeof(pd));
    pd.Length = sizeof(PROGDETAILS);
    pd.progt.progc = PROG_DEFAULT;
    pd.progt.fbVisible = SHE_VISIBLE;
    pd.pszTitle = "Workplace Shell";
    pd.pszExecutable = pszShell;
    pd.pszParameters = 0;
    pd.pszStartupDir = 0;
    pd.pszEnvironment = G_pszEnvironment;
                // new environment with new USER_INI
    pd.swpInitial.fl = SWP_ACTIVATE | SWP_ZORDER | SWP_SHOW;
    pd.swpInitial.hwndInsertBehind = HWND_TOP;
    G_happWPS = WinStartApp(G_hwndShellObject,
                            &pd,
                            NULL,
                            NULL,
                            SAF_INSTALLEDCMDLINE | SAF_STARTCHILDAPP);
    if (!G_happWPS)
    {
        Error("WinStartApp returned FALSE for starting %s",
                pszShell);
        arc = XWPSEC_CANNOT_START_SHELL;
    }

    return (arc);
}

/*
 *@@ LocalLogon:
 *      displays the logon dialog and creates a new
 *      locally logged-on user, including its
 *      user and group subject handles, sets the
 *      new shell environment and user profile
 *      and starts the user shell (normally the WPS).
 *
 *      This gets called from fnwpShellObject when
 *      XM_LOGON comes in.
 *
 *      On success, NO_ERROR is returned and
 *      G_pLocalLoggedOn contains the new user data.
 */

APIRET LocalLogon(VOID)
{
    APIRET      arc = NO_ERROR;

    XWPUSERDBENTRY  uiLogon;
    XWPLOGGEDON     LoggedOnUser;
    memset(&uiLogon, 0, sizeof(XWPUSERDBENTRY));
    memset(&LoggedOnUser, 0, sizeof(XWPLOGGEDON));

    _Pmpf(("Entering LocalLogon"));

    if (WinDlgBox(HWND_DESKTOP,
                  NULLHANDLE,      // owner
                  fnwpLogonDlg,
                  NULLHANDLE,
                  IDD_LOGON,
                  &uiLogon)         // puts szUserName, szPassword
            != DID_OK)
        arc = XWPSEC_NOT_AUTHENTICATED;
    else
    {
        // debug... remove this! ###
        if (strcmp(uiLogon.szUserName, "exit") == 0)
            // exit:
            WinPostMsg(G_hwndShellObject, WM_QUIT, 0, 0);
        else
        {
            strcpy(LoggedOnUser.szUserName, uiLogon.szUserName);
            arc = slogLogOn(&LoggedOnUser,
                            uiLogon.szPassword,
                            TRUE);       // mark as local user
                // creates subject handles

            if (arc == XWPSEC_NOT_AUTHENTICATED)
            {
                WinMessageBox(HWND_DESKTOP,
                              NULLHANDLE,
                              "Error: Invalid user name and/or password given.",
                              "XWorkplace Security",
                              0,
                              MB_OK | MB_SYSTEMMODAL | MB_MOVEABLE);
            }
            else if (arc != NO_ERROR)
            {
                Error("LocalLogon: slogLogOn returned arc %d", arc);
            }
            else
            {
                // user logged on, authenticated,
                // subject handles created,
                // registered with logged-on users:
                G_pszEnvironment = SetNewUserProfile(WinQueryAnchorBlock(G_hwndShellObject),
                                                     &LoggedOnUser);
                if (!G_pszEnvironment)
                {
                    Error("SetNewUserProfile returned NULL.", arc);
                    arc = XWPSEC_INVALID_PROFILE;
                }
                else
                {
                    // success:
                    // store local user
                    // (we must do this before the shell starts)
                    arc = StartUserShell();
                }

                if (arc != NO_ERROR)
                    arc = slogLogOff(LoggedOnUser.uid);

            } // end if Logon()

        }
    }

    return (arc);
}

/* ******************************************************************
 *
 *   Security Contexts
 *
 ********************************************************************/

/*
 *@@ DumpSecurityContexts:
 *
 */

void DumpSecurityContexts(VOID)
{
    PXWPSECURITYCONTEXT paContexts = NULL;
    ULONG               cContexts = 0;
    APIRET arc = scxtEnumSecurityContexts(0,
                                          &paContexts,
                                          &cContexts);
    if (arc != NO_ERROR)
        _Pmpf(("scxtEnumSecurityContexts returned %d.", arc));
    else
    {
        ULONG   ul;
        PXWPSECURITYCONTEXT pContextThis = paContexts;

        _Pmpf(("Dumping %d security contexts...", cContexts));

        for (ul = 0;
             ul < cContexts;
             ul++)
        {
            _Pmpf(("   %03d: pid = 0x%lX (%d), hsubjUser = %d, hsubjGroup = %d",
                   ul,
                   pContextThis->ulPID,
                   pContextThis->ulPID,
                   pContextThis->hsubjUser,
                   pContextThis->hsubjGroup));
            pContextThis++;
        }

        scxtFreeSecurityContexts(paContexts);
    }
}

/*
 *@@ CreateBaseSecurityContexts:
 *      called when XWPShell starts up to
 *      create security contexts for all
 *      processes which are running initially.
 */

APIRET CreateBaseSecurityContexts(VOID)
{
    APIRET arc = NO_ERROR;
    PQPROCSTAT16 p16Info = prc16GetInfo(&arc);
    if (p16Info)
    {
        PQPROCESS16 pProcess;
        for ( pProcess = (PQPROCESS16)PTR(p16Info->ulProcesses, 0);
              pProcess->ulType != 3;
              pProcess = (PQPROCESS16)PTR(pProcess->ulThreadList,
                                          pProcess->usThreads * sizeof(QTHREAD16))
            )
        {
            if (pProcess->usPID)
                arc = scxtCreateSecurityContext(pProcess->usPID,
                                                0,      // unauthorized
                                                0);     // unauthorized
            if (arc != NO_ERROR)
                break;
        }

        prc16FreeInfo(p16Info);
    }

    return (arc);
}

/*
 *@@ CleanupSecurityContexts:
 *      since we have no ring-0 hook for "exit process",
 *      we need this function to check which processes
 *      are still running.
 *
 *      This calls scxtEnumSecurityContexts and then
 *      scxtDeleteSecurityContext for each PID which
 *      isn't valid any more.
 */

APIRET CleanupSecurityContexts(VOID)
{
    PXWPSECURITYCONTEXT paContexts = NULL;
    ULONG               cContexts = 0;
    APIRET              arc = NO_ERROR;
    PQPROCSTAT16 p16Info = prc16GetInfo(&arc);
    if (p16Info)
    {
        arc = scxtEnumSecurityContexts(0,
                                       &paContexts,
                                       &cContexts);
        if (arc != NO_ERROR)
            _Pmpf(("scxtEnumSecurityContexts returned %d.", arc));
        else
        {
            ULONG   ul;
            PXWPSECURITYCONTEXT pContextThis = paContexts;

            for (ul = 0;
                 ul < cContexts;
                 ul++)
            {
                if (!prc16FindProcessFromPID(p16Info,
                                             pContextThis->ulPID))
                    // not found:
                    // delete security context
                    arc = scxtDeleteSecurityContext(pContextThis->ulPID);

                pContextThis++;

                if (arc != NO_ERROR)
                    break;
            }

            scxtFreeSecurityContexts(paContexts);
        }

        prc16FreeInfo(p16Info);
    }

    return (arc);
}

/* ******************************************************************
 *
 *   Ring-3 daemon thread
 *
 ********************************************************************/

#define SecIOCtl(hfDriver, ulFunctionCode, pvData, cbDataMax, pcbData) \
    DosDevIOCtl((hfDriver), IOCTL_XWPSEC, (ulFunctionCode), \
                        NULL, 0, NULL, \
                        (pvData), (cbDataMax), (pcbData))

typedef APIRET (FNAUTHORIZE) (PXWPSECURITYCONTEXT pContext,
                              PSECIOSHARED pSecIOShared,
                              ULONG ulAuthorizeData);
typedef FNAUTHORIZE *PFNAUTHORIZE;

/*
 *@@ AuthorizeSimpleFilname:
 *      authorizes an event which only has a filename
 *      as the parameter.
 *
 *      See XWPSECEVENTDATA_FILEONLY for details.
 *
 *      This must write NO_ERROR or ERROR_ACCESS_DENIED
 *      into pSecIOShared->arc.
 *
 *      Also, if this returns something != NO_ERROR,
 *      the ring-3 daemon is stopped with a security violation.
 */

APIRET AuthorizeFileOnly(PXWPSECURITYCONTEXT pContext,
                         PSECIOSHARED pSecIOShared,
                         ULONG ulAccessRequired)       // in: required XWPACCESS_* flags
{
    APIRET arc = NO_ERROR;          // return code (NOT access permission!)
    PXWPSECEVENTDATA_FILEONLY pFileOnly
        = &pSecIOShared->EventData.FileOnly;

    APIRET  arcAuthorized = NO_ERROR;

    // skip devices (we get calls for "\DEV\MOUSE$" and such too)
    if (    (pFileOnly->szPath[0] != '\\')
         && (pFileOnly->szPath[1] == ':')
       )
    {
        arcAuthorized = saclVerifyAccess(pContext,
                                         pFileOnly->szPath,
                                         ulAccessRequired);
    }

    pSecIOShared->arc = arcAuthorized;

    return (arc);
}

/*
 *@@ AuthorizeLoaderOpen:
 *      authorizes a "loader open" event.
 *
 *      See XWPSECEVENTDATA_LOADEROPEN for details.
 *
 *      This must write NO_ERROR or ERROR_ACCESS_DENIED
 *      into pSecIOShared->arc.
 *
 *      Also, if this returns something != NO_ERROR,
 *      the ring-3 daemon is stopped with a security violation.
 */

APIRET AuthorizeLoaderOpen(PXWPSECURITYCONTEXT pContext,
                           PSECIOSHARED pSecIOShared,
                           ULONG ulDummy)
{
    APIRET arc = NO_ERROR;          // return code (NOT access permission!)
    PXWPSECEVENTDATA_LOADEROPEN pLoaderOpen
        = &pSecIOShared->EventData.LoaderOpen;

    APIRET  arcAuthorized = NO_ERROR;

    // skip devices (we get calls for "\DEV\MOUSE$" and such too)
    if (    (pLoaderOpen->szFileName[0] != '\\')
         && (pLoaderOpen->szFileName[1] == ':')
       )
    {
        arcAuthorized = saclVerifyAccess(pContext,
                                         pLoaderOpen->szFileName,
                                         XWPACCESS_EXEC);
    }

    pSecIOShared->arc = arcAuthorized;

    return (arc);
}

/*
 *@@ AuthorizeExecPgm:
 *      authorizes a DosExecPgm event.
 *
 *      See XWPSECEVENTDATA_EXECPGM for details.
 *
 *      This must write NO_ERROR or ERROR_ACCESS_DENIED
 *      into pSecIOShared->arc.
 *
 *      Also, if this returns something != NO_ERROR,
 *      the ring-3 daemon is stopped with a security violation.
 */

APIRET AuthorizeExecPgm(PXWPSECURITYCONTEXT pContext,
                        PSECIOSHARED pSecIOShared,
                        ULONG ulDummy)
{
    APIRET arc = NO_ERROR;          // return code (NOT access permission!)
    PXWPSECEVENTDATA_EXECPGM pExecPgm
        = &pSecIOShared->EventData.ExecPgm;

    APIRET  arcAuthorized = NO_ERROR;

    // skip devices (we get calls for "\DEV\MOUSE$" and such too)
    if (    (pExecPgm->szFileName[0] != '\\')
         && (pExecPgm->szFileName[1] == ':')
       )
    {
        arcAuthorized = saclVerifyAccess(pContext,
                                         pExecPgm->szFileName,
                                         XWPACCESS_EXEC);
    }

    pSecIOShared->arc = arcAuthorized;

    return (arc);
}

/*
 *@@ AuthorizeOpenPre:
 *      authorizes an OPEN_PRE callback (DosOpen call).
 *
 *      See XWPSECEVENTDATA_OPEN_PRE for details how
 *      this processes stuff.
 *
 *      This must write NO_ERROR or ERROR_ACCESS_DENIED
 *      into pSecIOShared->arc.
 *
 *      Also, if this returns something != NO_ERROR,
 *      the ring-3 daemon is stopped with a security violation.
 */

APIRET AuthorizeOpenPre(PXWPSECURITYCONTEXT pContext,
                        PSECIOSHARED pSecIOShared,
                        ULONG ulAuthorizeData)
{
    APIRET arc = NO_ERROR;          // return code (NOT access permission!)
    PXWPSECEVENTDATA_OPEN_PRE pOpenPre
        = &pSecIOShared->EventData.OpenPre;

    APIRET  arcAuthorized = NO_ERROR;

    // skip devices (we get calls for "\DEV\MOUSE$" and such too)
    if (    (pOpenPre->szFileName[0] != '\\')
         && (pOpenPre->szFileName[1] == ':')
       )
    {
        ULONG ulRequiredForDrive = 0,
              ulRequiredForDir = 0,
              ulRequiredForFile = 0;

                         CHAR    szTemp[1000] = "";
                         _Pmpf(("SECEVENT_OPEN_PRE %s",
                                 pOpenPre->szFileName));

                         if (pOpenPre->fsOpenFlags & OPEN_ACTION_FAIL_IF_NEW)
                             strcat(szTemp, "OPEN_ACTION_FAIL_IF_NEW ");
                         if (pOpenPre->fsOpenFlags & OPEN_ACTION_CREATE_IF_NEW)
                             strcat(szTemp, "OPEN_ACTION_CREATE_IF_NEW ");
                         if (pOpenPre->fsOpenFlags & OPEN_ACTION_FAIL_IF_EXISTS)
                             strcat(szTemp, "OPEN_ACTION_FAIL_IF_EXISTS ");
                         if (pOpenPre->fsOpenFlags & OPEN_ACTION_OPEN_IF_EXISTS)
                             strcat(szTemp, "OPEN_ACTION_OPEN_IF_EXISTS ");
                         _Pmpf(("    fsOpenFlags: %s", szTemp));

                         szTemp[0] = 0;
                         if (pOpenPre->fsOpenMode & OPEN_FLAGS_DASD)
                             strcat(szTemp, "OPEN_FLAGS_DASD ");
                         switch (pOpenPre->fsOpenMode & 0x03)
                         {
                             case OPEN_ACCESS_READONLY:                      // 0x00
                                 strcat(szTemp, "OPEN_ACCESS_READONLY ");
                             break;

                             case OPEN_ACCESS_WRITEONLY:                     // 0x01
                                 strcat(szTemp, "OPEN_ACCESS_WRITEONLY ");
                             break;

                             case OPEN_ACCESS_READWRITE:                     // 0x02
                                 strcat(szTemp, "OPEN_ACCESS_READWRITE ");
                             break;
                         }
                         _Pmpf(("    fsOpenMode: %s", szTemp));

        // "open" for file name:
        if (pOpenPre->fsOpenFlags & OPEN_ACTION_CREATE_IF_NEW) // 0x0010
            ulRequiredForDir |= XWPACCESS_CREATE;
        if (pOpenPre->fsOpenFlags & OPEN_ACTION_OPEN_IF_EXISTS) // 0x0001
            ulRequiredForFile |= XWPACCESS_READ;
        if (pOpenPre->fsOpenFlags & OPEN_ACTION_REPLACE_IF_EXISTS) // 0x0002
        {
            ulRequiredForFile |= XWPACCESS_WRITE;
            ulRequiredForDir |= XWPACCESS_CREATE;
        }
        if (pOpenPre->fsOpenMode & OPEN_FLAGS_DASD) // (open drive) 0x8000
            ulRequiredForDir |= (XWPACCESS_WRITE | XWPACCESS_DELETE | XWPACCESS_CREATE);

        // bits 0-2 (mask: 0x3) specify access-mode flags
        switch (pOpenPre->fsOpenMode & 0x03)
        {
            case OPEN_ACCESS_READONLY:                      // 0x00
                ulRequiredForFile |= XWPACCESS_READ;
                ulRequiredForDir |= XWPACCESS_READ;
            break;

            case OPEN_ACCESS_WRITEONLY:                     // 0x01
                ulRequiredForFile |= XWPACCESS_WRITE;
                ulRequiredForDir |= XWPACCESS_WRITE;
            break;

            case OPEN_ACCESS_READWRITE:                     // 0x02
                ulRequiredForFile |= (XWPACCESS_READ | XWPACCESS_WRITE);
                ulRequiredForDir |= (XWPACCESS_READ | XWPACCESS_WRITE);
            break;
        }

        if (ulRequiredForDrive)
        {
            // authorization for drive needed:
            CHAR    szDrive[] = "?:\\";
            szDrive[0] = pOpenPre->szFileName[0];
            arcAuthorized = saclVerifyAccess(pContext,
                                             szDrive,
                                             ulRequiredForDrive);
        }

        if (arcAuthorized == NO_ERROR)
        {
            // authorized so far:
            if (ulRequiredForDir)
            {
                // authorization for file's directory needed:
                // create directory name from full path
                CHAR szDir[2*CCHMAXPATH];
                PSZ p ;
                strcpy(szDir, pOpenPre->szFileName);
                p = strrchr(szDir, '\\');
                if (p)
                {
                    *p = 0;
                    arcAuthorized = saclVerifyAccess(pContext,
                                                     szDir,
                                                     ulRequiredForDir);
                }
                // else no path: this can happen
                // for DASD open on drive ("G:")
            }

            if (arcAuthorized == NO_ERROR)
            {
                // authorized so far:
                if (ulRequiredForFile)
                {
                    // authorization for file itself needed:
                    arcAuthorized = saclVerifyAccess(pContext,
                                                     pOpenPre->szFileName,
                                                     ulRequiredForFile);
                }
            }
        }
    }

    pSecIOShared->arc = arcAuthorized;

    return (arc);
}

/*
 *@@ AuthorizeMovePre:
 *      authorizes a DosMove event.
 *
 *      See XWPSECEVENTDATA_MOVE_PRE for details.
 *
 *      This must write NO_ERROR or ERROR_ACCESS_DENIED
 *      into pSecIOShared->arc.
 *
 *      Also, if this returns something != NO_ERROR,
 *      the ring-3 daemon is stopped with a security violation.
 */

APIRET AuthorizeMovePre(PXWPSECURITYCONTEXT pContext,
                        PSECIOSHARED pSecIOShared,
                        ULONG ulDummy)
{
    APIRET arc = NO_ERROR;          // return code (NOT access permission!)
    PXWPSECEVENTDATA_MOVE_PRE pMovePre
        = &pSecIOShared->EventData.MovePre;

    APIRET  arcAuthorized = NO_ERROR;

    ULONG ulRequiredForOldDir = 0;

    // create directory name from file mask (e.g. "F:\test\*.txt")
    PSZ pDirOld = strdup(pMovePre->szOldPath);
    PSZ pOld = strrchr(pDirOld, '\\');
    PSZ pDirNew = strdup(pMovePre->szNewPath);
    PSZ pNew = strrchr(pDirNew, '\\');

    if (pOld)
    {
        *pOld = 0;

        if (pNew)
        {
            *pNew = 0;

            if (stricmp(pDirOld, pDirNew) != 0)
                // directories differ: that's a move...
                // check old dir also
                arcAuthorized = saclVerifyAccess(pContext,
                                                 pDirOld,
                                                 XWPACCESS_DELETE);

            if (arcAuthorized == NO_ERROR)
                arcAuthorized = saclVerifyAccess(pContext,
                                                 pDirNew,
                                                 XWPACCESS_WRITE);
            if (arcAuthorized == NO_ERROR)
                // now check file
                arcAuthorized = saclVerifyAccess(pContext,
                                                 pMovePre->szNewPath,
                                                 XWPACCESS_WRITE);
        }
        free(pDirNew);
    }
    free(pDirOld);

    pSecIOShared->arc = arcAuthorized;

    return (arc);
}

/*
 *@@ AuthorizeFindFirst:
 *      authorizes a DosFindFirst event.
 *
 *      See XWPSECEVENTDATA_FINDFIRST for details.
 *
 *      This must write NO_ERROR or ERROR_ACCESS_DENIED
 *      into pSecIOShared->arc.
 *
 *      Also, if this returns something != NO_ERROR,
 *      the ring-3 daemon is stopped with a security violation.
 */

APIRET AuthorizeFindFirst(PXWPSECURITYCONTEXT pContext,
                          PSECIOSHARED pSecIOShared,
                          ULONG ulDummy)
{
    APIRET arc = NO_ERROR;          // return code (NOT access permission!)
    PXWPSECEVENTDATA_FINDFIRST pFindFirst
        = &pSecIOShared->EventData.FindFirst;

    APIRET  arcAuthorized = NO_ERROR;

    // create directory name from file mask (e.g. "F:\test\*.txt")
    PSZ pDup = strdup(pFindFirst->szPath);
    PSZ p = strrchr(pDup, '\\');
    if (p)
    {
        *p = 0;
        arcAuthorized = saclVerifyAccess(pContext,
                                         pDup,
                                         XWPACCESS_READ);

    }
    free(pDup);

    pSecIOShared->arc = arcAuthorized;

    return (arc);
}

/*
 *@@ ProcessRing0Event:
 *      gets called from fntRing3Daemon for each
 *      event that the ring-0 driver (XWPSEC32.SYS)
 *      sends to us.
 *
 *      This receives a SECIOSHARED structure and
 *      must authorize access to the resource
 *      specified by SECIOSHARED.ulEventCode and
 *      SECIOSHARED.EventData. The event data is
 *      a XWPSECEVENTDATA union. Choose the member
 *      of that union based on the event code.
 *
 *      This MUST put the result of the authorization
 *      into SECIOSHARED.arc. Put NO_ERROR if access
 *      has been authorized, or ERROR_ACCESS_DENIED.
 *      The driver will then return this value to the
 *      application thread which requested access to
 *      the resource (and unblock that thread).
 *
 *      If this returns NO_ERROR, fntRing3Daemon continues
 *      looping, otherwise it stops.
 *
 *      Restrictions:
 *
 *      This thread MUST NOT START ANY OTHER PROCESSES
 *      because the EXEC_PGM callouts are NOT checked for
 *      whether they are called by the daemon. The system
 *      can deadlock or trap (haven't tested this) if this
 *      thread starts another process because ring 0 will
 *      recurse then.
 */

APIRET ProcessRing0Event(PSECIOSHARED pSecIOShared)
{
    APIRET  arc = NO_ERROR;

    PFNAUTHORIZE    pfnAuthorize = NULL;
    ULONG           ulAuthorizeData = 0;

    pSecIOShared->arc = NO_ERROR;

    switch (pSecIOShared->ulEventCode)
    {
        case SECEVENT_CLOSE:
        case SECEVENT_DELETE_POST:
        case SECEVENT_MOVE_POST:
        case SECEVENT_OPEN_POST:
        break;

        /*
         * SECEVENT_DELETE_PRE:
         *      DosDelete.
         */

        case SECEVENT_DELETE_PRE:
                         _Pmpf(("SECEVENT_DELETE_PRE %s",
                                 pSecIOShared->EventData.FileOnly.szPath));
            pfnAuthorize = AuthorizeFileOnly;
            ulAuthorizeData = XWPACCESS_DELETE; // req. access flags
        break;

        /*
         * SECEVENT_MAKEDIR:
         *
         */

        case SECEVENT_MAKEDIR:
                         _Pmpf(("SECEVENT_MAKEDIR %s",
                                 pSecIOShared->EventData.FileOnly.szPath));
            pfnAuthorize = AuthorizeFileOnly;
            ulAuthorizeData = XWPACCESS_CREATE; // req. access flags
        break;

        /*
         * SECEVENT_CHANGEDIR:
         *
         */

        case SECEVENT_CHANGEDIR:
                         _Pmpf(("SECEVENT_CHANGEDIR %s",
                                 pSecIOShared->EventData.FileOnly.szPath));
            pfnAuthorize = AuthorizeFileOnly;
            ulAuthorizeData = XWPACCESS_EXEC; // req. access flags
        break;

        /*
         * SECEVENT_REMOVEDIR:
         *
         */

        case SECEVENT_REMOVEDIR:
                         _Pmpf(("SECEVENT_REMOVEDIR %s",
                                 pSecIOShared->EventData.FileOnly.szPath));
            pfnAuthorize = AuthorizeFileOnly;
            ulAuthorizeData = XWPACCESS_DELETE; // req. access flags
        break;

        /*
         * SECEVENT_LOADEROPEN:
         *
         */

        case SECEVENT_LOADEROPEN:
                         _Pmpf(("SECEVENT_LOADEROPEN %s",
                                 pSecIOShared->EventData.LoaderOpen.szFileName));
            pfnAuthorize = AuthorizeLoaderOpen;
        break;

        /*
         * SECEVENT_GETMODULE:
         *
         */

        case SECEVENT_GETMODULE:
                         _Pmpf(("SECEVENT_GETMODULE %s",
                                 pSecIOShared->EventData.FileOnly.szPath));
            pfnAuthorize = AuthorizeFileOnly;
            ulAuthorizeData = XWPACCESS_EXEC; // req. access flags
        break;

        /*
         * SECEVENT_EXECPGM:
         *
         */

        case SECEVENT_EXECPGM:
                         _Pmpf(("SECEVENT_EXECPGM %s",
                                 pSecIOShared->EventData.ExecPgm.szFileName));
            pfnAuthorize = AuthorizeExecPgm;
        break;

        /*
         * SECEVENT_EXECPGM_POST:
         *      create security context for process.
         */

        case SECEVENT_EXECPGM_POST:
        {
            PXWPSECEVENTDATA_EXECPGM_POST pExecPgmPost
                = &pSecIOShared->EventData.ExecPgmPost;

            HXSUBJECT   hsubjUser = 0,      // unauthorized
                        hsubjGroup = 0;     // unauthorized

            XWPLOGGEDON LoggedOnLocal;

            if (NO_ERROR == slogQueryLocalUser(&LoggedOnLocal))
            {
                // local user logged on already:
                // ### temporary version;
                // really we should query which uid the
                // parent process is running on and check
                // for agent (privileged) processes here...
                hsubjUser = LoggedOnLocal.hsubjUser;
                hsubjGroup = LoggedOnLocal.hsubjGroup;
            }
            arc = scxtCreateSecurityContext(pExecPgmPost->ulNewPID,
                                            hsubjUser,
                                            hsubjGroup);
        if (arc != NO_ERROR)
            _Pmpf(("   scxtCreateSecurityContext returned %d.", arc));
        break; }

        /*
         *@@ SECEVENT_FINDFIRST:
         *
         */

        case SECEVENT_FINDFIRST:
                         _Pmpf(("SECEVENT_FINDFIRST %s",
                                 pSecIOShared->EventData.FindFirst.szPath));
            pfnAuthorize = AuthorizeFindFirst;
        break;

        /*
         * SECEVENT_MOVE_PRE:
         *
         */

        case SECEVENT_MOVE_PRE:
                         _Pmpf(("SECEVENT_MOVE_PRE %s -> %s",
                                 pSecIOShared->EventData.MovePre.szOldPath,
                                 pSecIOShared->EventData.MovePre.szNewPath));
            pfnAuthorize = AuthorizeMovePre;
        break;
        /*
         * SECEVENT_OPEN_PRE:
         *      DosOpen.
         */

        case SECEVENT_OPEN_PRE:
            pfnAuthorize = AuthorizeOpenPre;
        break;
    }

    if (pfnAuthorize)
    {
        // call needs authorization:

        // first check if process is running
        // on behalf of root... in that case,
        // we are authorized!

        XWPSECURITYCONTEXT Context;
        Context.ulPID = pSecIOShared->ulCallerPID;
        arc = scxtFindSecurityContext(&Context);

        if (arc != NO_ERROR)
            _Pmpf(("   scxtFindSecurityContext returned %d.", arc));
        else
        {
            if (Context.hsubjUser != 0)
            {
                // non-root:
                arc = pfnAuthorize(&Context,
                                   pSecIOShared,
                                   ulAuthorizeData);
                if (arc != NO_ERROR)
                    Error("pfnAuthorize returned %d", arc);
            }
            // else root: pSecIOShared->arc is still NO_ERROR;
        }
    }

    return (arc);
}

/*
 *@@ fntRing3Daemon:
 *      ring-3 daemon thread for serving the ring-0
 *      kernel hooks in XWPSEC32.SYS.
 *
 *      This gets started from InitDaemon() and calls
 *      XWPSEC32.SYS with the XWPSECIO_REGISTER IOCtl.
 *      After this, access control is ENABLED and the
 *      daemon receives messages in a block of shared
 *      memory every time access needs to be authorized.
 *
 *      This thread does not have a PM message queue.
 *
 *      Restrictions:
 *
 *      This thread MUST NOT START ANY OTHER PROCESSES
 *      because the EXEC_PGM callouts are NOT checked for
 *      whether they are called by the daemon. The system
 *      can deadlock or trap (haven't tested this) if this
 *      thread starts another process because ring 0 will
 *      recurse then.
 */

void _Optlink fntRing3Daemon(PTHREADINFO ptiMyself)
{
    APIRET  arc = NO_ERROR;

    // XWPSEC32.SYS file handle opened by main()
    HFILE   hfSec32DD = (HFILE)ptiMyself->ulData;

    // allocate memory for communication with ring-0 driver;
    // we use a buffer of SECIOSHARED, which we pass down
    // to the driver in the SECIOREGISTER struct with IoCtl

    PSECIOSHARED pSecIOShared = 0;

    // THIS MEMORY MUST BE ON A PAGE BOUNDARY, SO USE DOSALLOCMEM
    arc = DosAllocMem((PVOID*)&pSecIOShared,
                      sizeof(SECIOSHARED),
                      PAG_COMMIT | PAG_READ | PAG_WRITE);
    if (arc != NO_ERROR)
        Error("fntRing3Daemon: DosAllocMem returned %d.", arc);
    else
    {
        // register this daemon with ring-0 driver (XWPSEC32.SYS);
        // XWPSECIO_REGISTER IOCtl expects a SECIOREGISTER structure
        SECIOREGISTER   SecIORegister = {0};
        ULONG   cbDataLen = sizeof(SecIORegister);

        // store HEV so driver can post it
        SecIORegister.hevCallback = G_hevCallback;

        // store memory buffer so driver can lock it
        SecIORegister.pSecIOShared = pSecIOShared;

        // pass SECIOREGISTER
        arc = SecIOCtl(hfSec32DD,
                       XWPSECIO_REGISTER,
                       &SecIORegister,       // in      address of command data
                       sizeof(SecIORegister), // in      maximum size of command data
                       &cbDataLen);        // in out  size of command data returned
        if (arc != NO_ERROR)
            Error("fntRing3Daemon: DosDevIOCtl XWPSECIO_REGISTER returned %d.", arc);
        else
        {
            // OK, daemon successfully registered:

            // ACCESS CONTROL IS NOW ENABLED!!!
            // ANY RESOURCE ACCESS ON ANY THREAD IN THE SYSTEM
            // IS NOW BLOCKED UNTIL THIS THREAD HAS AUTHORIZED IT!!!

            // now loop and wait for ring-0 requests...

            while ((!ptiMyself->fExit) && (arc == NO_ERROR))
            {
                // wait for event semaphore
                arc = DosWaitEventSem(G_hevCallback, SEM_INDEFINITE_WAIT);
                // posted if
                // -- ring-0 DD got something for us
                // -- process is exiting (in that case, ptiMyself->fExit is TRUE)

                SecIORegister.pSecIOShared->arc = NO_ERROR;

                if ((arc == NO_ERROR) && (!ptiMyself->fExit))
                {
                    // command waiting:
                    // at this point, all requests in XWPSEC32.SYS are BLOCKED
                    // until we return a return code!

                    ULONG ulPostCount;
                    arc = DosResetEventSem(G_hevCallback, &ulPostCount);

                    arc = ProcessRing0Event(SecIORegister.pSecIOShared);
                    if (arc != NO_ERROR)
                        // stop:
                        ptiMyself->fExit = TRUE;
                }

                // ProcessRing0Event has put the access control
                // result into SECIOSHARED.arc;
                // notify ring-0 driver that access has been
                // checked... this will unblock the application
                // thread that was waiting

                arc = SecIOCtl(hfSec32DD,
                               XWPSECIO_JOBDONE,
                               NULL, 0, NULL);
            } // while ((!ptiMyself->fExit) && (arc == NO_ERROR))

            // done: deregister daemon...
            /* arc = */ SecIOCtl(hfSec32DD,
                           XWPSECIO_DEREGISTER,
                           NULL, 0, NULL);
            if (arc != NO_ERROR)
                Error("fntRing3Daemon: Error %d occured.", arc);
        }
    }
}

/*
 *@@ InitDaemon:
 *      this gets called from main() to initialize the ring-3
 *      daemon thread.
 *
 *      This first attempts to open XWPSEC32.SYS. If the driver
 *      is not installed, ERROR_FILE_NOT_FOUND is returned.
 *
 *      Otherwise, the G_hevCallback semaphore is created (for
 *      ring-0 communication), and the daemon thread is started.
 *
 *      IF THIS RETURNS "NO_ERROR", ACCESS CONTROL IS ENABLED
 *      FOR THE ENTIRE SYSTEM!
 */

APIRET InitDaemon(VOID)
{
    // open driver
    ULONG   ulActionTaken = 0;

    APIRET arc = DosOpen("XWPSEC$",
                         &G_hfSec32DD,
                         &ulActionTaken,
                         0,    // file size
                         FILE_NORMAL,    // file attribute
                         OPEN_ACTION_OPEN_IF_EXISTS,   // do not create
                         OPEN_SHARE_DENYNONE | OPEN_ACCESS_READWRITE | OPEN_FLAGS_FAIL_ON_ERROR,
                         NULL);    // EAs
    if (arc == NO_ERROR)
    {
        // driver opened:

        // create shared ring-3/ring-0 event semaphore;
        // XWPSEC32.SYS posts this semaphore when there's a
        // notification waiting. The semaphore must be created "shared",
        // otherwise the driver cannot open it.
        arc = DosCreateEventSem(NULL,               // unnamed
                                &G_hevCallback,
                                DC_SEM_SHARED,      // shared
                                FALSE);             // not posted
        if (arc != NO_ERROR)
            Error("fntRing3Daemon: DosCreateEventSem failed.");
        else
            // start ring-3 daemon thread, which
            // interfaces ring-0 driver...
            if (!thrCreate(&G_tiRing3Daemon,
                           fntRing3Daemon,
                           &G_fRing3DaemonRunning,
                           THRF_WAIT,
                           G_hfSec32DD))      // driver file handle
            {
                Error("Cannot create ring-3 daemon thread.");
                arc = XWPSEC_CANNOT_START_DAEMON;
                DosCloseEventSem(G_hevCallback);
                G_hevCallback = NULLHANDLE;
            }

        if (arc != NO_ERROR)
        {
            // some error, but driver was opened:
            DosClose(G_hfSec32DD);
            G_hfSec32DD = NULLHANDLE;
        }
    }

    return (arc);
}

/*
 *@@ ShutdownDaemon:
 *      -- stops daemon thread
 *      -- closes G_hevCallback
 *      -- closes XWPSEC32.SYS
 */

VOID ShutdownDaemon(VOID)
{
    APIRET arc = NO_ERROR;

    // stop callback thread
    if (G_fRing3DaemonRunning)
    {
        G_tiRing3Daemon.fExit = TRUE;
        // post event semaphore so that thread
        // gets a chance to run...
        arc = DosPostEventSem(G_hevCallback);
        if (arc != NO_ERROR)
            Error("main: DosPostEventSem failed.");
        else
        {
            ULONG ulPostCount = 0;
            // must also reset it or it will post forever
            DosResetEventSem(G_hevCallback, &ulPostCount);
            thrWait(&G_tiRing3Daemon);
        }
    }

    if (G_hevCallback)
    {
        DosCloseEventSem(G_hevCallback);
        G_hevCallback = NULLHANDLE;
    }

    if (G_hfSec32DD)
    {
        DosClose(G_hfSec32DD);
        G_hfSec32DD = NULLHANDLE;
    }
}

/* ******************************************************************
 *                                                                  *
 *   Object window                                                  *
 *                                                                  *
 ********************************************************************/

VOID DumpEnv(PDOSENVIRONMENT pEnv)
{
    PSZ     *ppszThis = pEnv->papszVars;
    PSZ     pszThis;
    ULONG   ul = 0;
    _Pmpf(("cVars: %d", pEnv->cVars));

    for (ul = 0;
         ul < pEnv->cVars;
         ul++)
    {
        pszThis = *ppszThis;
        _Pmpf(("var %d: %s", ul, pszThis));
        // next environment string
        ppszThis++;
    }
}

/*
 *@@ fnwpShellObject:
 *      winproc for XWPShell's object window on thread 1.
 *      This is created by main().
 */

MRESULT EXPENTRY fnwpShellObject(HWND hwndObject, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;
    PTHREADINFO ptiSESLogonEvent = NULL;

    switch (msg)
    {
        /*
         *@@ XM_LOGON:
         *
         */

        case XM_LOGON:
        {
            // display logon dialog, create subject handles,
            // set environment, start shell, ...
            APIRET arc = LocalLogon();

            if (arc != NO_ERROR)
            {
                // error:
                // repost to try again
                WinPostMsg(hwndObject,
                           XM_LOGON,
                           0, 0);
            }
        break; }

        /*
         * WM_APPTERMINATENOTIFY:
         *      gets posted by PM when an application
         *      started with WinStartApp terminates.
         *      In this case, it's the WPS. So we log
         *      off.
         */

        case WM_APPTERMINATENOTIFY:
        {
            HAPP    happ = (HAPP)mp1;
            if (happ == G_happWPS)
            {
                // PMSHELL.EXE (or other user shell) has terminated:
                APIRET arc = NO_ERROR;

                G_happWPS = NULLHANDLE;

                // check if the shell wants a restart or
                // a new logon
                if (G_pXWPShellShared->fNoLogonButRestart)
                {
                    arc = StartUserShell();
                }
                else
                {
                    XWPLOGGEDON LoggedOnLocal;
                    arc = slogQueryLocalUser(&LoggedOnLocal);
                    if (arc != NO_ERROR)
                        Error("slogQueryLocalUser returned %d", arc);
                    else
                    {
                        arc = CleanupSecurityContexts();
                        if (arc != NO_ERROR)
                            Error("CleanupSecurityContexts returned %d", arc);
                        // DumpSecurityContexts();

                        // log off the local user
                        // (this deletes the subject handles)
                        arc = slogLogOff(LoggedOnLocal.uid);
                        free(G_pszEnvironment);
                        G_pszEnvironment = NULL;
                    }

                    if (arc != NO_ERROR)
                        Error("WM_APPTERMINATENOTIFY: arc %d on logoff",
                            arc);

                    // show logon dlg again
                    WinPostMsg(hwndObject,
                               XM_LOGON,
                               0, 0);
                }
            }
        break; }

        case XM_ERROR:
        {
            PSZ     pszError = "Unknown error.";

            switch ((ULONG)mp1)
            {
                case XWPSEC_RING0_NOT_FOUND:
                    pszError = "XWPSEC32.SYS driver not installed.";
                break;
            }

            winhDebugBox(0,
                     "XWPShell Error",
                     pszError);
        break; }

        case XM_MESSAGE:
            winhDebugBox(0,
                     "XWPShell Message",
                     (PSZ)mp1);
            free(mp1);
        break;

        default:
            mrc = WinDefWindowProc(hwndObject, msg, mp1, mp2);
    }

    return (mrc);
}

/*
 * main:
 *      program entry point.
 */

int main(int argc, char *argv[])
{
    int         irc = 0;

    HAB         hab;
    HMQ         hmq;
    APIRET      arc;

    _Pmpf(("Entering main."));

    if (!(hab = WinInitialize(0)))
        return 99;

    if (!(hmq = WinCreateMsgQueue(hab, 0)))
        return 99;

    if (winhAnotherInstance("\\SEM32\\XWPSHELL.MTX", FALSE))
    {
        // already running:
        winhDebugBox(NULLHANDLE,
                 "XWorkplace Security",
                 "Another instance of XWPSHELL.EXE is already running. "
                 "This instance will terminate now.");
        irc = 99;
    }
    else
    {
        // since this program will never stop running, make
        // sure we survive even shutdown
        WinCancelShutdown(hmq, TRUE);

        // allocate XWPSHELLSHARED
        arc = DosAllocSharedMem((PVOID*)&G_pXWPShellShared,
                                SHMEM_XWPSHELL,
                                sizeof(XWPSHELLSHARED),
                                PAG_COMMIT | PAG_READ | PAG_WRITE);
        if (arc != NO_ERROR)
            Error("DosAllocSharedMem returned %d.", arc);
        else
        {
            // initialize subsystems
            if (    (saclInit() != NO_ERROR)
                 || (scxtInit() != NO_ERROR)
                 || (subjInit() != NO_ERROR)
                 || (sudbInit() != NO_ERROR)
                 || (slogInit() != NO_ERROR)
               )
                irc = 1;
            else
            {
                if (!WinRegisterClass(hab,
                                      WC_SHELL_OBJECT,
                                      fnwpShellObject,
                                      0,
                                      sizeof(ULONG)))
                    irc = 2;
                else
                {
                    G_LogFile = fopen("E:\\xwpshell.log", "a");

                    G_hwndShellObject = WinCreateWindow(HWND_OBJECT,
                                                        WC_SHELL_OBJECT,
                                                        "XWPShellObject",
                                                        0,             // style
                                                        0, 0, 0, 0,
                                                        NULLHANDLE,    // owner
                                                        HWND_BOTTOM,
                                                        0,             // ID
                                                        NULL,
                                                        NULL);
                    if (!G_hwndShellObject)
                        irc = 3;
                    else
                    {
                        // OK:
                        QMSG qmsg;

                        arc = CreateBaseSecurityContexts();
                        if (arc != NO_ERROR)
                            irc =3;
                        else
                        {
                            InitDaemon();

                            // do a logon first
                            WinPostMsg(G_hwndShellObject,
                                       XM_LOGON,
                                       0, 0);

                            // enter standard PM message loop
                            while (WinGetMsg(hab, &qmsg, NULLHANDLE, 0, 0))
                            {
                                WinDispatchMsg(hab, &qmsg);
                            }

                            ShutdownDaemon();
                        }
                    }

                    fclose(G_LogFile);
                }
            }
        }
    }

    // clean up on the way out
    WinDestroyMsgQueue(hmq);
    WinTerminate(hab);

    return (irc);
}


