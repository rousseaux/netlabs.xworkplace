
/*
 *@@sourcefile xwpshell.c:
 *      startup wrapper around PMSHELL.EXE. Besides functioning
 *      as a shell starter, XWPShell also maintains users,
 *      processes, and access control lists to interact with
 *      the ring-0 device driver (XWPSEC32.SYS).
 *
 *      See helpers\xwpsecty.h for an introduction of how all
 *      this works.
 *
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
 *      In debug mode, you can terminate XWPShell by entering the
 *      "exit" user name with any dummy password.
 *
 *      Configuration via environment variables:
 *
 *      1.  In CONFIG.SYS, set XWPSHELL to PMSHELL.EXE's full path.
 *          The XWPSHELL environment variable specifies the executable
 *          to be started by XWPShell. If XWPSHELL is not defined,
 *          PMSHELL.EXE is used.
 *
 *      2.  In CONFIG.SYS, set XWPHOME to the full path of the HOME
 *          directory tree, which holds all user desktops and INI files.
 *          If this is not specified, this defaults to ?:\home on the
 *          boot drive.
 *
 *      3.  In CONFIG.SYS, set XWPUSERDB to the directory where
 *          XWPShell should keep its user data base files (xwpusers.xml
 *          and xwpusers.acc). If this is not specified, this defaults
 *          to ?:\OS2 on the boot drive.
 *
 *@@added V0.9.5 [umoeller]
 *@@header "helpers\xwpsecty.h"
 *@@header "security\xwpshell.h"
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

#define INCL_DOSEXCEPTIONS
#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#define INCL_DOSQUEUES
#define INCL_DOSMISC
#define INCL_DOSERRORS

#define INCL_WINPOINTERS
#define INCL_WINPROGRAMLIST
#define INCL_WINWORKPLACE
#include <os2.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <io.h>
#include <ctype.h>
#include <setjmp.h>

#include "setup.h"

#include "helpers\apps.h"
#include "helpers\dosh.h"
#include "helpers\except.h"
#include "helpers\prfh.h"
#include "helpers\winh.h"
#include "helpers\threads.h"

#include "helpers\xwpsecty.h"
#include "security\xwpshell.h"

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

HWND        G_hwndShellObject = NULLHANDLE;
    // object window for communication

// user shell currently running:
// this must only be modified by thread-1!!
HAPP        G_happWPS = NULLHANDLE;
    // HAPP of WPS process or NULLHANDLE if the WPS is not running
PSZ         G_pszEnvironment = NULL;
    // environment of user shell

extern PXFILE G_LogFile = NULL;

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
    vsprintf(szError, pcszFormat, args);
    va_end(args);

    doshWrite(G_LogFile, 0, szError);
    doshWrite(G_LogFile, 0, "\n");
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

STATIC MRESULT EXPENTRY fnwpLogonDlg(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
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
                                           sizeof(puiLogon->User.szUserName),
                                           puiLogon->User.szUserName))
                    {
                        WinQueryWindowText(hwndPassword,
                                           sizeof(puiLogon->szPassword),
                                           puiLogon->szPassword);

                        WinDismissDlg(hwnd, DID_OK);

                    }
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

APIRET SetNewUserProfile(HAB hab,                       // in: XWPSHELL anchor block
                         ULONG uid,
                         PCSZ pcszUserName,
                         PSZ *ppszEnvironment)          // out: new environment
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

        if (!uid)
            // root gets default profile
            strcpy(szNewProfile,
                   getenv("USER_INI"));
        else
            // non-root:
            sprintf(szNewProfile,
                    "%s\\%s\\os2.ini",
                    pszHomeBase,
                    pcszUserName);

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

                // set HOME var to home directory
                sprintf(szNewVar, "HOME=%s\\%s", pszHomeBase, pcszUserName);
                if (arc = appSetEnvironmentVar(&Env, szNewVar, FALSE))
                    break;

                // set USER var to user name
                sprintf(szNewVar, "USER=%s", pcszUserName);
                if (arc = appSetEnvironmentVar(&Env, szNewVar, FALSE))
                    break;

                // set USERID var to user name
                sprintf(szNewVar, "USERID=%d", uid);
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

    return arc;
}

/*
 *@@ StartUserShell:
 *
 */

APIRET StartUserShell(VOID)
{
    APIRET      arc = NO_ERROR;

    XWPSECID    uidLocal;
    if (!(arc = slogQueryLocalUser(&uidLocal)))
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
            // change our own security context @@todo
        }
    }

    return arc;
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

    _PmpfF(("entering"));

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
        if (!strcmp(uiLogon.User.szUserName, "exit"))
            // exit:
            WinPostMsg(G_hwndShellObject, WM_QUIT, 0, 0);
        else
#endif
        {
            HPOINTER hptrOld = winhSetWaitPointer();

            arc = slogLogOn(uiLogon.User.szUserName,
                            uiLogon.szPassword,
                            TRUE,                // mark as local user
                            &uiLogon.User.uid);      // store uid
                // creates subject handles

            // nuke the password buffer
            memset(uiLogon.szPassword,
                   0,
                   sizeof(uiLogon.szPassword));

            switch (arc)
            {
                case XWPSEC_NOT_AUTHENTICATED:
                    WinMessageBox(HWND_DESKTOP,
                                  NULLHANDLE,
                                  "Error: Invalid user name and/or password given.",
                                  "XWorkplace Security",
                                  0,
                                  MB_OK | MB_SYSTEMMODAL | MB_MOVEABLE);
                break;

                case NO_ERROR:
                    // user logged on, authenticated,
                    // subject handles created,
                    // registered with logged-on users:
                    G_pszEnvironment = NULL;

                    if (arc = SetNewUserProfile(WinQueryAnchorBlock(G_hwndShellObject),
                                                uiLogon.User.uid,
                                                uiLogon.User.szUserName,
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

                    if (arc)
                        slogLogOff(LoggedOnUser.uid);
                break;

                default:
                    // some other error:
                    Error("LocalLogon: slogLogOn returned arc %d", arc);
            }

            WinSetPointer(HWND_DESKTOP, hptrOld);
        }
    }

    _PmpfF(("leaving, returning %d", arc));

    return arc;
}

/* ******************************************************************
 *
 *   Queue thread
 *
 ********************************************************************/

/*
 *@@ GiveMemToCaller:
 *
 *@@added V1.0.1 (2003-01-10) [umoeller]
 */

APIRET GiveMemToCaller(PVOID p,
                       ULONG pid)
{
    APIRET arc;

    arc = DosGiveSharedMem((PBYTE)p,
                           pid, // caller's PID
                           PAG_READ | PAG_WRITE);

    // free this for us; usage count is 2 presently,
    // so the chunk will be freed after the caller
    // has issued DosFreeMem also
    DosFreeMem((PBYTE)p);

    return arc;
}

/*
 *@@ ProcessQueueCommand:
 *
 *@@added V0.9.11 (2001-04-22) [umoeller]
 *@@changed V1.0.1 (2003-01-05) [umoeller]: added create user
 */

APIRET ProcessQueueCommand(PXWPSHELLQUEUEDATA pQD,
                           ULONG pid)
{
    APIRET  arc = NO_ERROR;

    // check size of struct; when we add fields, this can change,
    // and we don't want to blow up because of this V1.0.1 (2003-01-05) [umoeller]
    if (pQD->cbStruct != sizeof(XWPSHELLQUEUEDATA))
        return XWPSEC_STRUCT_MISMATCH;

#ifdef __DEBUG__
    TRY_LOUD(excpt1)
#else
    TRY_QUIET(excpt1)
#endif
    {
        // prepare security context so we can check if the
        // calling process has sufficient authority for
        // processing this request
        PXWPSECURITYCONTEXT psc;
        if (!(arc = scxtFindSecurityContext(pid,
                                            &psc)))
        {
            switch (pQD->ulCommand)
            {
                case QUECMD_QUERYSTATUS:
                    arc = scxtQueryStatus(&pQD->Data.Status);
                break;

                case QUECMD_QUERYLOCALUSER:
                {
                    // no authority needed for this
                    XWPSECID    uidLocal;
                    if (    (!(arc = slogQueryLocalUser(&uidLocal)))
                         && (!(arc = sudbQueryUser(uidLocal,
                                                   &pQD->Data.pLocalUser)))
                       )
                    {
                        // this has allocated a chunk of shared memory, so give
                        // this to the caller
                        arc = GiveMemToCaller(pQD->Data.pLocalUser,
                                              pid);
                    }
                }
                break;

                case QUECMD_QUERYUSERS:
                    if (    (!(arc = scxtVerifyAuthority(psc,
                                                         XWPPERM_QUERYUSERINFO)))
                         && (!(arc = sudbQueryUsers(&pQD->Data.QueryUsers.cUsers,
                                                    &pQD->Data.QueryUsers.paUsers)))
                       )
                    {
                        // this has allocated a chunk of shared memory, so give
                        // this to the caller
                        arc = GiveMemToCaller(pQD->Data.QueryUsers.paUsers,
                                              pid);
                    }
                break;

                case QUECMD_QUERYGROUPS:
                    if (    (!(arc = scxtVerifyAuthority(psc,
                                                         XWPPERM_QUERYUSERINFO)))
                         && (!(arc = sudbQueryGroups(&pQD->Data.QueryGroups.cGroups,
                                                     &pQD->Data.QueryGroups.paGroups)))
                       )
                    {
                        // this has allocated a chunk of shared memory, so give
                        // this to the caller
                        arc = GiveMemToCaller(pQD->Data.QueryGroups.paGroups,
                                              pid);
                    }
                break;

                case QUECMD_QUERYPROCESSOWNER:
                    // @@todo
                    arc = XWPSEC_QUEUE_INVALID_CMD;
                break;

                case QUECMD_CREATEUSER:
                    if (!(arc = scxtVerifyAuthority(psc,
                                                    XWPPERM_CREATEUSER)))
                    {
                        XWPUSERDBENTRY ue;
                        #define COPYITEM(a) memcpy(ue.User.a, pQD->Data.CreateUser.a, sizeof(ue.User.a))
                        COPYITEM(szUserName);
                        COPYITEM(szFullName);
                        memcpy(ue.szPassword, pQD->Data.CreateUser.szPassword, sizeof(ue.szPassword));
                        if (!(arc = sudbCreateUser(&ue)))
                            pQD->Data.CreateUser.uidCreated = ue.User.uid;
                    }
                break;

                case QUECMD_QUERYPERMISSIONS:
                    arc = scxtQueryPermissions(pQD->Data.QueryPermissions.szResource,
                                               psc->cSubjects,
                                               psc->aSubjects,
                                               &pQD->Data.QueryPermissions.flAccess);
                break;

                default:
                    // unknown code:
                    arc = XWPSEC_QUEUE_INVALID_CMD;
                break;
            }

            free(psc);
        }
    }
    CATCH(excpt1)
    {
        arc = ERROR_PROTECTION_VIOLATION;
    } END_CATCH();

    return arc;
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
            PXWPSHELLQUEUEDATA  pQD = (PXWPSHELLQUEUEDATA)(rq.ulData);
            HEV hev = pQD->hevData;

            _PmpfF(("got queue item, pQD->ulCommand: %d",
                        pQD->ulCommand));

            if (!DosOpenEventSem(NULL,
                                 &hev))
            {
                TRY_LOUD(excpt1)
                {
                    pQD->arc = ProcessQueueCommand(pQD,
                                                   // caller's pid
                                                   rq.pid);
                }
                CATCH(excpt1)
                {
                } END_CATCH();

                DosPostEventSem(hev);
                DosCloseEventSem(hev);
            }

            // free shared memory for this process... it was
            // given to us by the client, so we must lower
            // the resource count (client will still own it)
            DosFreeMem(pQD);
        }
        else
            _PmpfF(("DosReadQueue returned %d", arc));
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
            APIRET arc;

            if (arc = LocalLogon())
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
                    XWPSECID    uidLocal;
                    if (arc = slogQueryLocalUser(&uidLocal))
                        Error("slogQueryLocalUser returned %d", arc);
                    else
                    {
                        // log off the local user
                        // (this deletes the subject handles)
                        arc = slogLogOff(uidLocal);
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

    return mrc;
}

/*
 * main:
 *      program entry point.
 */

int main(int argc, char *argv[])
{
    APIRET      arc = NO_ERROR;

    HAB         hab;
    HMQ         hmq;
    DATETIME    DT;
    ULONG       cbFile = 0;

    CHAR        szLog[500];
    CHAR        szBoot[] = "?:";
    PSZ         pszLogDir;
    if (DosScanEnv("LOGFILES",      // new eCS 1.1 setting
                   &pszLogDir))
    {
        // variable not set:
#ifdef __EWORKPLACE__
        return FALSE;
#else
        szBoot[0] = doshQueryBootDrive();
        pszLogDir = szBoot;
#endif
    }

    sprintf(szLog,
            "%s\\xwpshell.log",
            pszLogDir);

    doshOpen(szLog,
             XOPEN_READWRITE_NEW, // XOPEN_READWRITE_APPEND,        // not XOPEN_BINARY
             &cbFile,
             &G_LogFile);

    DosGetDateTime(&DT);
    sprintf(szLog,
            "\n\nXWPShell startup -- %04d-%02d-%02d %02d:%02d:%02d\n",
            DT.year, DT.month, DT.day,
            DT.hours, DT.minutes, DT.seconds);
    doshWrite(G_LogFile,
              0,
              szLog);
    doshWrite(G_LogFile,
              0,
              "---------------------------------------\n");

    if (!(hab = WinInitialize(0)))
        return 99;

    if (!(hmq = WinCreateMsgQueue(hab, 0)))
        return 99;

    winhInitGlobals();      // V1.0.1 (2002-11-30) [umoeller]

    if (winhAnotherInstance("\\SEM32\\XWPSHELL.MTX", FALSE))
    {
        // already running:
        winhDebugBox(NULLHANDLE,
                 "XWorkplace Security",
                 "Another instance of XWPSHELL.EXE is already running. "
                 "This instance will terminate now.");
        arc = -1;
    }
    else
    {
        TRY_LOUD(excpt1)
        {
            // since this program will never stop running, make
            // sure we survive even shutdown
            WinCancelShutdown(hmq, TRUE);

            // allocate XWPSHELLSHARED
            if (arc = DosAllocSharedMem((PVOID*)&G_pXWPShellShared,
                                        SHMEM_XWPSHELL,
                                        sizeof(XWPSHELLSHARED),
                                        PAG_COMMIT | PAG_READ | PAG_WRITE))
                Error("DosAllocSharedMem returned %d.", arc);
            // create master queue
            else if (arc = DosCreateQueue(&G_hqXWPShell,
                                          QUE_FIFO | QUE_NOCONVERT_ADDRESS,
                                          QUEUE_XWPSHELL))
                Error("DosCreateQueue returned %d.", arc);
            // initialize subsystems
            else if (arc = scxtInit())
                Error("Error %d initializing security contexts.", arc);
            else if (    (arc = sudbInit())
                      || (arc = slogInit())
                    )
                Error("Initialization error %d.", arc);
            // create shell object (thread 1)
            else if (!WinRegisterClass(hab,
                                       WC_SHELL_OBJECT,
                                       fnwpShellObject,
                                       0,
                                       sizeof(ULONG)))
                arc = -1;
            else if (!(G_hwndShellObject = WinCreateWindow(HWND_OBJECT,
                                                           WC_SHELL_OBJECT,
                                                           "XWPShellObject",
                                                           0,             // style
                                                           0, 0, 0, 0,
                                                           NULLHANDLE,    // owner
                                                           HWND_BOTTOM,
                                                           0,             // ID
                                                           NULL,
                                                           NULL)))
                arc = -1;
            else
            {
                // OK:
                QMSG qmsg;
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
                    WinDispatchMsg(hab, &qmsg);
            }
        }
        CATCH(excpt1)
        {
        } END_CATCH();

        scxtExit();
    }

    // clean up on the way out
    WinDestroyMsgQueue(hmq);
    WinTerminate(hab);

    doshWriteLogEntry(G_LogFile,
                      "XWPShell exiting");
    doshClose(&G_LogFile);

    return arc;
}


