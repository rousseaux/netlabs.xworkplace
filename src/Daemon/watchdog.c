
/*
 *@@sourcefile watchdog.c:
 *
 *@@added V0.9.21 (2002-08-12) [umoeller]
 *@@header "hook\xwpdaemn.h"
 */

/*
 *      Copyright (C) 2002 Ulrich M”ller.
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, in version 2 as it comes in the COPYING
 *      file of the XWorkplace main distribution.
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 */

#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#define INCL_DOSEXCEPTIONS
#define INCL_DOSMISC
#define INCL_DOSERRORS

#define INCL_WINWINDOWMGR
#define INCL_WINMESSAGEMGR
#define INCL_WINSYS
#include <os2.h>

#include <stdio.h>
#include <time.h>
#include <setjmp.h>

#define DONT_REPLACE_FOR_DBCS
#define DONT_REPLACE_MALLOC         // in case mem debug is enabled
#include "setup.h"                      // code generation and debugging options

#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\procstat.h"
#include "helpers\standards.h"
#include "helpers\threads.h"

#include "xwpapi.h"                     // public XWorkplace definitions

#include "filesys\disk.h"               // XFldDisk implementation

#include "hook\xwphook.h"               // hook and daemon definitions
#include "hook\xwpdaemn.h"              // XPager and daemon declarations

#include "bldlevel.h"

#pragma hdrstop

ULONG       G_pidInProgress = 0,
            G_tidInProgress = 0,
            G_ulTimeSent = 0;

HMTX        G_hmtxWatchdog = NULLHANDLE;

THREADINFO  G_tiWatchdog,
            G_tiTeaser;

/*
 *@@ LockWatchdog:
 *
 *@@added V0.9.21 (2002-08-12) [umoeller]
 */

BOOL LockWatchdog(VOID)
{
    if (G_hmtxWatchdog)
        return !DosRequestMutexSem(G_hmtxWatchdog, SEM_INDEFINITE_WAIT);

    // first call:
    return !DosCreateMutexSem(NULL,
                              &G_hmtxWatchdog,
                              0,
                              TRUE);      // request!
}

VOID UnlockWatchdog(VOID)
{
    DosReleaseMutexSem(G_hmtxWatchdog);
}

/*
 *@@ fntTeaser:
 *
 *@@added V [umoeller]
 */

void _Optlink fntTeaser(PTHREADINFO ptiMyself)
{
    BOOL    fLocked = FALSE;

    TRY_LOUD(excpt1)
    {
        // run forever unless exit
        while (!ptiMyself->fExit)
        {
            HENUM   henum = WinBeginEnumWindows(HWND_DESKTOP);
            HWND    hwnd;

            while (hwnd = WinGetNextWindow(henum))
            {
                if (    (fLocked)
                     || (fLocked = LockWatchdog())
                   )
                {
                    WinQueryWindowProcess(hwnd, &G_pidInProgress, &G_tidInProgress);
                    DosQuerySysInfo(QSV_MS_COUNT, QSV_MS_COUNT,
                                    &G_ulTimeSent,
                                    sizeof(G_ulTimeSent));

                    UnlockWatchdog();
                    fLocked = FALSE;

#ifndef WM_QUERYCTLTYPE
#define WM_QUERYCTLTYPE            0x0130
#endif

                    WinSendMsg(hwnd,
                               WM_QUERYCTLTYPE,
                               0,
                               0);

                    fLocked = LockWatchdog();
                }
            }

            WinEndEnumWindows(henum);

            DosSleep(300);
        }
    }
    CATCH(excpt1)
    {
    } END_CATCH();

    if (fLocked)
        UnlockWatchdog();
}

#define TIMEOUT     (2 * 1000)

/*
 *@@ fntWatchdog:
 *
 *@@added V0.9.21 (2002-08-12) [umoeller]
 */

void _Optlink fntWatchdog(PTHREADINFO ptiMyself)
{
    BOOL fLocked = FALSE;

    TRY_LOUD(excpt1)
    {
        thrCreate(&G_tiTeaser,
                  fntTeaser,
                  NULL,
                  "Teaser",
                  THRF_PMMSGQUEUE | THRF_WAIT,
                  0);

        // run forever unless exit
        while (!ptiMyself->fExit)
        {
            ULONG   pidHung = 0;

            if (fLocked = LockWatchdog())
            {
                ULONG   ulTimeNow;
                DosQuerySysInfo(QSV_MS_COUNT, QSV_MS_COUNT,
                                &ulTimeNow,
                                sizeof(ulTimeNow));

                if (G_ulTimeSent < ulTimeNow - TIMEOUT)
                    pidHung = G_pidInProgress;

                UnlockWatchdog();
                fLocked = FALSE;
            }

            _PmpfF(("pidHung: %d", pidHung));

            if (pidHung)
            {
                HPS hps;
                PQTOPLEVEL32    t;
                PQPROCESS32     p;
                PQMODULE32      m;
                CHAR            szProcess[CCHMAXPATH] = "?";

                if (t = prc32GetInfo(NULL))
                {
                    if (    (p = prc32FindProcessFromPID(t, pidHung))
                         && (m = prc32FindModule(t, p->usHModule))
                       )
                        strcpy(szProcess, m->pcName);

                    prc32FreeInfo(t);
                }

                if (hps = WinGetScreenPS(HWND_DESKTOP))
                {
                    CHAR sz[500];
                    RECTL rcl = { 10, 10, 600, 300 };
                    WinFillRect(hps,
                                &rcl,
                                CLR_WHITE);

                    sprintf(sz,
                            "PID 0x%lX (%d) (%s) is hung",
                            pidHung, pidHung,
                            szProcess);

                    WinDrawText(hps,
                                -1,
                                sz,
                                &rcl,
                                CLR_BLACK,
                                CLR_WHITE,
                                DT_CENTER | DT_VCENTER);

                    WinReleasePS(hps);
                }
            }

            DosSleep(1000);
        }
    }
    CATCH(excpt1)
    {
    } END_CATCH();

    if (fLocked)
        UnlockWatchdog();
}

/*
 *@@ dmnStartWatchdog:
 *
 *@@added V0.9.21 (2002-08-12) [umoeller]
 */

VOID dmnStartWatchdog(VOID)
{
    thrCreate(&G_tiWatchdog,
              fntWatchdog,
              NULL,
              "Watchdog",
              THRF_WAIT,
              0);
}
