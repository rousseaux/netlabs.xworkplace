
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
 *          this case, access is granted, and the Desktop starts up.
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
#include <setjmp.h>

#include "setup.h"

#include "helpers\apps.h"
#include "helpers\cnrh.h"
#include "helpers\dosh.h"
#include "helpers\eah.h"
#include "helpers\except.h"
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
 *
 *   Global variables
 *
 ********************************************************************/

typedef HSWITCH APIENTRY WINHSWITCHFROMHAPP(HAPP happ);

WINHSWITCHFROMHAPP *G_WinHSWITCHfromHAPP = NULL;

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
BOOL        G_fAccessControl = FALSE;       // TRUE if ring-3 daemon is running
                                            // and access control is enabled
                                            // V0.9.19 (2002-04-02) [umoeller]

THREADINFO  G_tiRing3Daemon = {0};
ULONG       G_tidRing3DaemonRunning = FALSE;
// HEV         G_hevCallback = NULLHANDLE;

PXWPSHELLSHARED G_pXWPShellShared = 0;

HQUEUE      G_hqXWPShell = 0;
THREADINFO  G_tiQueueThread = {0};

/* ******************************************************************
 *
 *   Helpers
 *
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

                }
                break;
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
 *      Returns:
 *
 *      --  NO_ERROR: profile was successfully changed;
 *          in that case, *ppszEnvironment has been
 *          set to an environment strings buffer for
 *          use with WinStartApp. This must be freed
 *          by the caller.
 *
 *      --  XWPSEC_NO_USER_PROFILE: OS2.INI doesn't
 *          exist in the new user's home directory.
 *
 *      plus the error codes from appGetEnvironment;
 *
 *@@added V0.9.4 (2000-07-19) [umoeller]
 *@@changed V0.9.19 (2002-04-02) [umoeller]: changed prototype to return APIRET
 */

APIRET SetNewUserProfile(HAB hab,                   // in: XWPSHELL anchor block
                         PCXWPLOGGEDON pNewUser,    // in: new user to set profile for
                         PSZ *ppszEnvironment)      // out: new environment
{
    APIRET arc = NO_ERROR;
    PSZ pEnv2 = NULL;
    DOSENVIRONMENT Env;

    if (!(arc = appGetEnvironment(&Env)))
    {
        ULONG cbEnv2 = 0;

        CHAR    szNewProfile[CCHMAXPATH];

        CHAR    szHomeBase[CCHMAXPATH];
        PSZ     pszHomeBase;
        if (!(pszHomeBase = getenv("XWPHOME")))
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

        if (access(szNewProfile, 0) != 0)
            // OS2.INI doesn't exist:
            arc = XWPSEC_NO_USER_PROFILE;
        else
        {
            do      // for break
            {
                CHAR    szNewVar[1000];
                PSZ     p;
                sprintf(szNewVar, "USER_INI=%s", szNewProfile);
                if (arc = appSetEnvironmentVar(&Env, szNewVar, FALSE))
                    break;

                // set HOME var to home directory,
                // with Unix slashes
                sprintf(szNewVar, "HOME=%s/%s", pszHomeBase, pNewUser->szUserName);
                while (p = strchr(szNewVar, '\\'))
                    *p = '/';
                if (arc = appSetEnvironmentVar(&Env, szNewVar, FALSE))
                    break;

                // set USER var to user name
                sprintf(szNewVar, "USER=%s", pNewUser->szUserName);
                if (arc = appSetEnvironmentVar(&Env, szNewVar, FALSE))
                    break;

                // set USERID var to user name
                sprintf(szNewVar, "USERID=%d", pNewUser->uid);
                if (arc = appSetEnvironmentVar(&Env, szNewVar, FALSE))
                    break;

                // set USERSUBJECT var to user subject handle
                sprintf(szNewVar, "USERSUBJECT=%d", pNewUser->hsubjUser);
                if (arc = appSetEnvironmentVar(&Env, szNewVar, FALSE))
                    break;

                // set GROUP var to group name
                sprintf(szNewVar, "GROUP=%s", pNewUser->szGroupName);
                if (arc = appSetEnvironmentVar(&Env, szNewVar, FALSE))
                    break;

                // set GROUPID var to user name
                sprintf(szNewVar, "GROUPID=%d", pNewUser->gid);
                if (arc = appSetEnvironmentVar(&Env, szNewVar, FALSE))
                    break;

                // set GROUPSUBJECT var to group subject handle
                sprintf(szNewVar, "GROUPSUBJECT=%d", pNewUser->hsubjGroup);
                if (arc = appSetEnvironmentVar(&Env, szNewVar, FALSE))
                    break;

                if (arc = appConvertEnvironment(&Env, &pEnv2, &cbEnv2))
                    // pEnv != NULL now, which is returned
                    break;

            } while (FALSE);

            if (!arc)
            {
                // user profile exists:
                // call PrfReset
                arc = prfhSetUserProfile(hab,
                                         szNewProfile);
            }
        } // end if (access(szNewProfile, 0) == 0)

        appFreeEnvironment(&Env);

    } // end if (doshGetEnvironment(&Env)

    if (!arc)
        *ppszEnvironment = pEnv2;
    else
        if (pEnv2)
            free(pEnv2);

    return (arc);
}

/*
 *@@ StartUserShell:
 *
 */

APIRET StartUserShell(VOID)
{
    APIRET arc = NO_ERROR;

    XWPLOGGEDON loLocal;
    if (!(arc = slogQueryLocalUser(&loLocal)))
    {
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

        if (!(G_happWPS = WinStartApp(G_hwndShellObject,
                                      &pd,
                                      NULL,
                                      NULL,
                                      SAF_INSTALLEDCMDLINE | SAF_STARTCHILDAPP)))
        {
            Error("WinStartApp returned FALSE for starting %s",
                    pszShell);
            arc = XWPSEC_CANNOT_START_SHELL;
        }
        else
        {
            // in case ring-0 support is not running,
            // create security context for the shell
            // (otherwise it will be refreshed)
            HSWITCH hsw;
            if (!(hsw = G_WinHSWITCHfromHAPP(G_happWPS)))
            {
                _Pmpf((__FUNCTION__ ": Cannot find HSWITCH for happ 0x%lX", G_happWPS));
            }
            else
            {
                SWCNTRL swc;
                if (!WinQuerySwitchEntry(hsw, &swc))
                {
                    arc = scxtCreateSecurityContext(swc.idProcess,
                                                    loLocal.hsubjUser,
                                                    loLocal.hsubjGroup);
                    _Pmpf((__FUNCTION__ ": scxtCreateSecurityContext returned %d", arc));
                }
                else
                    _Pmpf((__FUNCTION__ ": WinQuerySwitchEntry for shell failed"));
            }
        }
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

    _Pmpf((__FUNCTION__ ": entering"));

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
#ifdef __DEBUG__
        // in debug builds, allow exit
        if (strcmp(uiLogon.szUserName, "exit") == 0)
            // exit:
            WinPostMsg(G_hwndShellObject, WM_QUIT, 0, 0);
        else
#endif
        {
            HPOINTER hptrOld = winhSetWaitPointer();

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
                G_pszEnvironment = NULL;

                if (arc = SetNewUserProfile(WinQueryAnchorBlock(G_hwndShellObject),
                                            &LoggedOnUser,
                                            &G_pszEnvironment))
                {
                    Error("SetNewUserProfile returned %d.", arc);
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

            WinSetPointer(HWND_DESKTOP, hptrOld);
        }
    }

    _Pmpf((__FUNCTION__ ": leaving, returning %d", arc));

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
    PQPROCSTAT16 p16Info;

    if (!(arc = prc16GetInfo(&p16Info)))
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
                                                -1,      // pre-running
                                                -1);     // pre-running
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
    PQPROCSTAT16        p16Info;

    if (!(arc = prc16GetInfo(&p16Info)))
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

    _Pmpf(("SECEVENT_OPEN_PRE %s",
            pOpenPre->szFileName));

    // skip devices (we get calls for "\DEV\MOUSE$" and such too)
    if (    (pOpenPre->szFileName[0] != '\\')
         && (pOpenPre->szFileName[1] == ':')
       )
    {
        if (    (pOpenPre->szFileName[0] == 'N')
             && (pOpenPre->szFileName[1] == ':')
           )
            arcAuthorized = ERROR_ACCESS_DENIED;

        /* ULONG ulRequiredForDrive = 0,
              ulRequiredForDir = 0,
              ulRequiredForFile = 0;

                         CHAR    szTemp[1000] = "";

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
        } */
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
                // @@todo temporary version;
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
        }
        break;

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

        _Pmpf(("   scxtFindSecurityContext returned %d.", arc));

        if (!arc)
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

    // allocate memory for communication with ring-0 driver;
    // we use a buffer of SECIOSHARED, which we pass down
    // to the driver in the SECIOREGISTER struct with IoCtl

    PSECIOSHARED pSecIOShared = 0;

    DosSetPriority(PRTYS_THREAD,
                   4,
                   +31,
                   0);          // current thread

    // THIS MEMORY MUST BE ON A PAGE BOUNDARY, SO USE DOSALLOCMEM
    arc = DosAllocMem((PVOID*)&pSecIOShared,
                      sizeof(SECIOSHARED),
                      PAG_COMMIT | PAG_READ | PAG_WRITE);
    if (arc != NO_ERROR)
        Error(__FUNCTION__ ": DosAllocMem returned %d.", arc);
    else
    {
        // register this daemon with ring-0 driver (XWPSEC32.SYS);
        ULONG cbDataLen = sizeof(SECIOSHARED);

        // pass SECIOSHARED;
        // this call blocks until we have something to do!
        if (!(arc = SecIOCtl(G_hfSec32DD,
                             XWPSECIO_REGISTER,
                             pSecIOShared,          // in      address of command data
                             sizeof(SECIOSHARED),   // in      maximum size of command data
                             &cbDataLen)))          // in out  size of command data returned
        {
            // this returned: then the buffer is already
            // filled with an authorization request
            while ((!ptiMyself->fExit) && (!arc))
            {
                // process request
                // at this point, all requests in XWPSEC32.SYS are BLOCKED
                // until we return a return code!

                pSecIOShared->arc = NO_ERROR;

                if (arc = ProcessRing0Event(pSecIOShared))
                    // error:
                    break;

                // ProcessRing0Event has put the access control
                // result into SECIOSHARED.arc;
                // notify ring-0 driver that access has been
                // checked... this will unblock the application
                // thread that was waiting

                cbDataLen = sizeof(SECIOSHARED);
                arc = SecIOCtl(G_hfSec32DD,
                               XWPSECIO_AUTHORIZED_NEXT,
                               pSecIOShared,
                               sizeof(SECIOSHARED),
                               &cbDataLen);
            }

            // done: deregister daemon...
            SecIOCtl(G_hfSec32DD,
                     XWPSECIO_DEREGISTER,
                     NULL,
                     0,
                     NULL);

            _Pmpf((__FUNCTION__ ": access control disabled."));

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

        // start ring-3 daemon thread, which
        // interfaces ring-0 driver...
        if (!thrCreate(&G_tiRing3Daemon,
                       fntRing3Daemon,
                       &G_tidRing3DaemonRunning,
                       "Ring3Daemon",
                       THRF_WAIT,
                       G_hfSec32DD))      // driver file handle
        {
            Error("Cannot create ring-3 daemon thread.");
            arc = XWPSEC_CANNOT_START_DAEMON;
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
    if (G_tidRing3DaemonRunning)
    {
        G_tiRing3Daemon.fExit = TRUE;

        SecIOCtl(G_hfSec32DD,
                 XWPSECIO_DEREGISTER,
                 NULL,
                 0,
                 NULL);
        thrWait(&G_tiRing3Daemon);
    }

    if (G_hfSec32DD)
    {
        DosClose(G_hfSec32DD);
        G_hfSec32DD = NULLHANDLE;
    }
}

/* ******************************************************************
 *
 *   Queue thread
 *
 ********************************************************************/

APIRET ProcessQueueCommand(PXWPSHELLQUEUEDATA pSharedQueueData,
                           ULONG pid)
{
    APIRET arc = NO_ERROR;

    // prepare security context so we can check if the
    // calling process has sufficient authority for
    // processing this request
    XWPSECURITYCONTEXT sc;
    sc.ulPID = pid;

    switch (pSharedQueueData->ulCommand)
    {
        case QUECMD_QUERYLOCALLOGGEDON:
            // no authority needed for this
            arc = slogQueryLocalUser(&pSharedQueueData->Data.QueryLocalLoggedOn);
        break;

        case QUECMD_QUERYUSERS:
            if (    (!(arc = scxtFindSecurityContext(&sc)))
                 && (!(arc = scxtVerifyAuthority(
                                    &sc,
                                    ACTION_QUERYUSERSGROUPS)))
                 && (!(arc = sudbQueryUsers(
                                    &pSharedQueueData->Data.QueryUsers.cUsers,
                                    &pSharedQueueData->Data.QueryUsers.paUsers)))
               )
            {
                // this has allocated a chunk of shared memory, so give
                // this to the caller
                arc = DosGiveSharedMem(
                                    (PBYTE)pSharedQueueData->Data.QueryUsers.paUsers,
                                    pid, // caller's PID
                                    PAG_READ | PAG_WRITE);

                // free this for us; usage count is 2 presently,
                // so the chunk will be freed after the caller
                // as issued DosFreeMem also
                DosFreeMem((PBYTE)pSharedQueueData->Data.QueryUsers.paUsers);
            }
        break;

        case QUECMD_QUERYGROUPS:
            if (    (!(arc = scxtFindSecurityContext(&sc)))
                 && (!(arc = scxtVerifyAuthority(
                                    &sc,
                                    ACTION_QUERYUSERSGROUPS)))
                 && (!(arc = sudbQueryGroups(
                                    &pSharedQueueData->Data.QueryGroups.cGroups,
                                    &pSharedQueueData->Data.QueryGroups.paGroups)))
               )
            {
                // this has allocated a chunk of shared memory, so give
                // this to the caller
                arc = DosGiveSharedMem(
                                    (PBYTE)pSharedQueueData->Data.QueryGroups.paGroups,
                                    pid, // caller's PID
                                    PAG_READ | PAG_WRITE);

                // free this for us; usage count is 2 presently,
                // so the chunk will be freed after the caller
                // as issued DosFreeMem also
                DosFreeMem((PBYTE)pSharedQueueData->Data.QueryGroups.paGroups);
            }
        break;

        case QUECMD_QUERYPROCESSOWNER:
            // no authority needed for this
            sc.ulPID = pSharedQueueData->Data.QueryProcessOwner.ulPID;
            if (!(arc = scxtFindSecurityContext(&sc)))
            {
                if (sc.hsubjUser == -1)
                    pSharedQueueData->Data.QueryProcessOwner.uid = -1;
                else
                {
                    XWPSUBJECTINFO si;
                    si.hSubject = sc.hsubjUser;
                    if (!(arc = subjQuerySubjectInfo(&si)))
                        pSharedQueueData->Data.QueryProcessOwner.uid = si.id;
                }
            }
        break;

        default:
            // unknown code:
            arc = XWPSEC_QUEUE_INVALID_CMD;
        break;
    }

    return (arc);
}

/*
 *@@ fntQueueThread:
 *      "queue" thread started by main() to process the
 *      XWPShell command queue. This allows other processes
 *      ("clients") to send commands to XWPShell.
 *
 *      The way this works is that a client allocates
 *      an XWPSHELLQUEUEDATA struct as shared memory,
 *      gives this to the XWPShell process, and writes
 *      an entry into the queue. We then process the
 *      command and post hevData when done to signal
 *      to the caller that data is available.
 *
 *@@added V0.9.11 (2001-04-22) [umoeller]
 */

void _Optlink fntQueueThread(PTHREADINFO ptiMyself)
{
    while (!ptiMyself->fExit)
    {
        APIRET      arc = NO_ERROR;
        REQUESTDATA rq;
        ULONG       ulDummySize;
        PULONG      pulDummyData;
        BYTE        bPriority;

        // block on the queue until a command comes in
        if (!(arc = DosReadQueue(G_hqXWPShell,
                                 &rq,
                                 &ulDummySize,
                                 (PVOID*)&pulDummyData,
                                 0,                     // remove first element
                                 DCWW_WAIT,
                                 &bPriority,            // priority
                                 NULLHANDLE)))           // event semaphore, ignored for DCWW_WAIT
        {
            // got a command:
            PXWPSHELLQUEUEDATA  pSharedQueueData = (PXWPSHELLQUEUEDATA)(rq.ulData);
            HEV hev = pSharedQueueData->hevData;

            _Pmpf((__FUNCTION__ ": got queue item, pSharedQueueData->ulCommand: %d",
                        pSharedQueueData->ulCommand));

            if (!DosOpenEventSem(NULL,
                                 &hev))
            {
                TRY_LOUD(excpt1)
                {
                    pSharedQueueData->arc = ProcessQueueCommand(pSharedQueueData,
                                                                // caller's pid
                                                                rq.pid);

                    // free shared memory for this process... it was
                    // given to us by the client, so we must lower
                    // the resource count (client will still own it)
                    DosFreeMem(pSharedQueueData);
                }
                CATCH(excpt1)
                {
                } END_CATCH();

                DosPostEventSem(hev);
                DosCloseEventSem(hev);
            }
        }
        else
            _Pmpf((__FUNCTION__ ": DosReadQueue returned %d", arc));
    }
}

/* ******************************************************************
 *
 *   Object window
 *
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
        }
        break;

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
                        if (G_pszEnvironment)
                        {
                            free(G_pszEnvironment);
                            G_pszEnvironment = NULL;
                        }
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
        }
        break;

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
        }
        break;

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

    CHAR szLog[100];
    sprintf(szLog, "%c:\\xwpshell.log", doshQueryBootDrive());
    G_LogFile = fopen(szLog, "a");

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

        // import WinHSWITCHfromHAPP
        // WinHSWITCHfromHAPP PMMERGE.5199
        if ((arc = doshQueryProcAddr("PMMERGE",
                                     5199,
                                     (PFN*)&G_WinHSWITCHfromHAPP)))
            Error("doshQueryProcAddr returned %d resolving WinHSWITCHfromHAPP.", arc);
        // allocate XWPSHELLSHARED
        else if ((arc = DosAllocSharedMem((PVOID*)&G_pXWPShellShared,
                                SHMEM_XWPSHELL,
                                sizeof(XWPSHELLSHARED),
                                PAG_COMMIT | PAG_READ | PAG_WRITE)))
            Error("DosAllocSharedMem returned %d.", arc);
        // create master queue
        else if ((arc = DosCreateQueue(&G_hqXWPShell,
                                       QUE_FIFO | QUE_NOCONVERT_ADDRESS,
                                       QUEUE_XWPSHELL)))
            Error("DosCreateQueue returned %d.", arc);
        // initialize subsystems
        else if (    (saclInit() != NO_ERROR)
                  || (scxtInit() != NO_ERROR)
                  || (subjInit() != NO_ERROR)
                  || (sudbInit() != NO_ERROR)
                  || (slogInit() != NO_ERROR)
                )
            irc = 1;
        // create shell object (thread 1)
        else if (!WinRegisterClass(hab,
                                   WC_SHELL_OBJECT,
                                   fnwpShellObject,
                                   0,
                                   sizeof(ULONG)))
            irc = 2;
        else
        {
            if (!(G_hwndShellObject = WinCreateWindow(HWND_OBJECT,
                                                WC_SHELL_OBJECT,
                                                "XWPShellObject",
                                                0,             // style
                                                0, 0, 0, 0,
                                                NULLHANDLE,    // owner
                                                HWND_BOTTOM,
                                                0,             // ID
                                                NULL,
                                                NULL)))
                irc = 3;
            else
            {
                // OK:
                QMSG qmsg;

                if (arc = CreateBaseSecurityContexts())
                    irc = 4;
                else
                {
                    InitDaemon();
                            // might fail, but who cares

                    // create the queue thread
                    thrCreate(&G_tiQueueThread,
                              fntQueueThread,
                              NULL,
                              "Queue thread",
                              THRF_WAIT,
                              0);

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
        }
    }

    // clean up on the way out
    WinDestroyMsgQueue(hmq);
    WinTerminate(hab);

    fclose(G_LogFile);

    return (irc);
}


