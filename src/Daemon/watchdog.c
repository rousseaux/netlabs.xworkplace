
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

#define OP_IDLE                     0
#define OP_BEGINENUMWINDOWS         1
#define OP_GETNEXTWINDOW            2
#define OP_QUERYWINDOWPROCESS       3
#define OP_SENDMSG                  4
#define OP_ENDENUMWINDOWS           5

ULONG       G_ulOperation = OP_IDLE;

ULONG       G_ulOperationTime = 0;           // system uptime when operation was performed

ULONG       G_pidBeingSent = 0,         // PID of window that msg is being sent to
            G_tidBeingSent = 0;         // TID of window that msg is being sent to

HMTX        G_hmtxWatchdog = NULLHANDLE,
            G_hmtxHungs = NULLHANDLE;

THREADINFO  G_tiWatchdog,
            G_tiTeaser;

#ifndef WM_QUERYCTLTYPE
#define WM_QUERYCTLTYPE            0x0130
#endif

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

/*
 *@@ UnlockWatchdog:
 *
 *@@added V0.9.21 (2002-08-12) [umoeller]
 */

VOID UnlockWatchdog(VOID)
{
    DosReleaseMutexSem(G_hmtxWatchdog);
}



/*
 *@@ SetOperation:
 *
 *@@added V0.9.21 (2002-08-12) [umoeller]
 */

void SetOperation(ULONG op,
                  PBOOL pfLocked)
{
    if (*pfLocked = LockWatchdog())
    {
        DosQuerySysInfo(QSV_MS_COUNT, QSV_MS_COUNT,
                        &G_ulOperationTime,
                        sizeof(G_ulOperationTime));
        G_ulOperation = op;

        UnlockWatchdog();
        *pfLocked = FALSE;
    }
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
            HENUM   henum;
            HWND    hwnd;

            // call WinBeginEnumWindows
            SetOperation(OP_BEGINENUMWINDOWS, &fLocked);
            if (henum = WinBeginEnumWindows(HWND_DESKTOP))
            {
                while (TRUE)
                {
                    ULONG   pid, tid;

                    // call WinGetNextWindow
                    SetOperation(OP_GETNEXTWINDOW, &fLocked);
                    if (!(hwnd = WinGetNextWindow(henum)))
                        // last window of this enumeration:
                        // stop
                        break;


                    SetOperation(OP_QUERYWINDOWPROCESS, &fLocked);
                    if (!WinQueryWindowProcess(hwnd, &pid, &tid))
                        // window has died?
                        // start over
                        break;

                    if (fLocked = LockWatchdog())
                    {
                        G_pidBeingSent = pid;
                        G_tidBeingSent = tid;

                        DosQuerySysInfo(QSV_MS_COUNT, QSV_MS_COUNT,
                                        &G_ulOperationTime,
                                        sizeof(G_ulOperationTime));
                        G_ulOperation = OP_SENDMSG;

                        UnlockWatchdog();
                        fLocked = FALSE;

                        // call WinSendMsg
                        WinSendMsg(hwnd,
                                   WM_QUERYCTLTYPE,
                                   0,
                                   0);
                    }
                } // while (TRUE)
            }

            // call WinEndEnumWindows
            SetOperation(OP_ENDENUMWINDOWS, &fLocked);
            WinEndEnumWindows(henum);

            SetOperation(OP_IDLE, &fLocked);

            DosSleep(300);
        }
    }
    CATCH(excpt1)
    {
    } END_CATCH();

    if (fLocked)
        UnlockWatchdog();
}

#pragma pack(1)

/*
 *@@ PMSEM:
 *      PM semaphore.
 *
 *@@added V0.9.21 (2002-08-12) [umoeller]
 */

typedef struct _PMSEM
{
    CHAR        szMagic[7];
    BYTE        bSemaphore;         // 386 semaphore byte
    ULONG       ulOwnerPidTid;
    ULONG       cNesting;           // owner nested use count
    ULONG       cWaiters;           // number of waiters
    ULONG       ulDebug1;           // number of times sem used (debug only)
    HEV         hev;                // OS/2 event semaphore
    ULONG       ulDebug2;           // address of caller (debug only)
} PMSEM, *PPMSEM;

#pragma pack()

#define PMSEM_ATOM          0
#define PMSEM_USER          1
#define PMSEM_VISLOCK       2
#define PMSEM_DEBUG         3
#define PMSEM_HOOK          4
#define PMSEM_HEAP          5
#define PMSEM_DLL           6
#define PMSEM_THUNK         7
#define PMSEM_XLCE          8
#define PMSEM_UPDATE        9
#define PMSEM_CLIP          10
#define PMSEM_INPUT         11
#define PMSEM_DESKTOP       12
#define PMSEM_HANDLE        13
#define PMSEM_ALARM         14
#define PMSEM_STRRES        15
#define PMSEM_TIMER         16
#define PMSEM_CONTROLS      17
#define GRESEM_GreInit      18
#define GRESEM_AutoHeap     19
#define GRESEM_PDEV         20
#define GRESEM_LDEV         21
#define GRESEM_CodePage     22
#define GRESEM_HFont        23
#define GRESEM_FontCntxt    24
#define GRESEM_FntDrvr      25
#define GRESEM_ShMalloc     26
#define GRESEM_GlobalData   27
#define GRESEM_DbcsEnv      28
#define GRESEM_SrvLock      29
#define GRESEM_SelLock      30
#define GRESEM_ProcLock     31
#define GRESEM_DriverSem    32
#define GRESEM_semIfiCache  33
#define GRESEM_semFontTable 34

#define LASTSEM             34

#define TIMEOUT     (1 * 1000)

/*
 *@@ LockHungs:
 *
 *@@added V0.9.21 (2002-08-12) [umoeller]
 */

BOOL LockHungs(VOID)
{
    if (G_hmtxHungs)
        return !DosRequestMutexSem(G_hmtxHungs, SEM_INDEFINITE_WAIT);

    // first call:
    return !DosCreateMutexSem(NULL,
                              &G_hmtxHungs,
                              0,
                              TRUE);      // request!
}

/*
 *@@ UnlockHungs:
 *
 *@@added V0.9.21 (2002-08-12) [umoeller]
 */

VOID UnlockHungs(VOID)
{
    DosReleaseMutexSem(G_hmtxHungs);
}

typedef struct _HUNG
{
    USHORT  pid;
    ULONG   op;
    ULONG   sem;
} HUNG, *PHUNG;

#define MAXHUNG         40

HUNG        G_aHungs[MAXHUNG];
ULONG       G_cHungs = 0;

/*
 *@@ AppendHung:
 *
 *      Caller must hold the hungs sem.
 *
 *@@added V0.9.21 (2002-08-12) [umoeller]
 */

VOID AppendHung(USHORT pid,
                ULONG op,
                ULONG sem)
{
    G_aHungs[G_cHungs].pid = pid;
    G_aHungs[G_cHungs].op = op;
    G_aHungs[G_cHungs++].sem = sem;
}

/*
 *@@ GetOpName:
 *
 *@@added V0.9.21 (2002-08-12) [umoeller]
 */

PCSZ GetOpName(ULONG opHung)
{
    switch (opHung)
    {
        #define SETCASE(o) case o: return # o

        SETCASE(OP_IDLE);
        SETCASE(OP_BEGINENUMWINDOWS);
        SETCASE(OP_GETNEXTWINDOW);
        SETCASE(OP_QUERYWINDOWPROCESS);
        SETCASE(OP_SENDMSG);
        SETCASE(OP_ENDENUMWINDOWS);
    }

    return "unknown";
}

static const PCSZ G_papcszSems[] =
    {
        "PMSEM_ATOM", // 0
        "PMSEM_USER", // 1
        "PMSEM_VISLOCK", // 2
        "PMSEM_DEBUG", // 3
        "PMSEM_HOOK", // 4
        "PMSEM_HEAP", // 5
        "PMSEM_DLL", // 6
        "PMSEM_THUNK", // 7
        "PMSEM_XLCE", // 8
        "PMSEM_UPDATE", // 9
        "PMSEM_CLIP", // 10
        "PMSEM_INPUT", // 11
        "PMSEM_DESKTOP", // 12
        "PMSEM_HANDLE", // 13
        "PMSEM_ALARM", // 14
        "PMSEM_STRRES", // 15
        "PMSEM_TIMER", // 16
        "PMSEM_CONTROLS", // 17
        "GRESEM_GreInit", // 18
        "GRESEM_AutoHeap", // 19
        "GRESEM_PDEV", // 20
        "GRESEM_LDEV", // 21
        "GRESEM_CodePage", // 22
        "GRESEM_HFont", // 23
        "GRESEM_FontCntxt", // 24
        "GRESEM_FntDrvr", // 25
        "GRESEM_ShMalloc", // 26
        "GRESEM_GlobalData", // 27
        "GRESEM_DbcsEnv", // 28
        "GRESEM_SrvLock", // 29
        "GRESEM_SelLock", // 30
        "GRESEM_ProcLock", // 31
        "GRESEM_DriverSem", // 32
        "GRESEM_semIfiCache", // 33
        "GRESEM_semFontTable", // 34
    };

/*
 *@@ fntWatchdog:
 *
 *@@added V0.9.21 (2002-08-12) [umoeller]
 */

void _Optlink fntWatchdog(PTHREADINFO ptiMyself)
{
    BOOL    semWatchdog = FALSE,
            semHungs = FALSE;
    CHAR    szTemp[500];
    HPS     hpsScreen = NULLHANDLE;

    TRY_LOUD(excpt1)
    {
        APIRET  arc;
        PPMSEM  paPMSems;

        LONG    cxScreen = WinQuerySysValue(HWND_DESKTOP, SV_CXSCREEN);

        thrCreate(&G_tiTeaser,
                  fntTeaser,
                  NULL,
                  "Teaser",
                  THRF_PMMSGQUEUE | THRF_WAIT,
                  0);

        // PMSEMAPHORES PMMERGE.6203
        if (arc = doshQueryProcAddr("PMMERGE",
                                    6203,
                                    (PFN*)&paPMSems))
        {
            // and exit
            ptiMyself->fExit = TRUE;
        }
        else
        {
            CHAR szTest[9];
            szTest[8] = '\0';
            memcpy(szTest, paPMSems, 7);
        }

        // run forever unless exit
        while (!ptiMyself->fExit)
        {
            ULONG   opHung = OP_IDLE,
                    pidBeingSent = 0;
            ULONG   ulTimeNow;
            PCSZ    pcszOp = NULL;

            PSZ     apsz[MAXHUNG];
            ULONG   cPaint = 0;

            memset(apsz, 0, sizeof(apsz));

            if (semWatchdog = LockWatchdog())
            {
                DosQuerySysInfo(QSV_MS_COUNT, QSV_MS_COUNT,
                                &ulTimeNow,
                                sizeof(ulTimeNow));

                if (    (G_ulOperation != OP_IDLE)
                     && (G_ulOperationTime < ulTimeNow - TIMEOUT)
                   )
                {
                    opHung = G_ulOperation;
                    pidBeingSent = G_pidBeingSent;
                }

                UnlockWatchdog();
                semWatchdog = FALSE;
            }

            // lock the global list of hung processes
            // while we're checking the results
            if (semHungs = LockHungs())
            {
                // reset global count
                G_cHungs = 0;

                if (opHung != OP_IDLE)
                {
                    ULONG   ulSemThis;

                    CHAR    szLogFile[CCHMAXPATH];
                    FILE    *LogFile;
                    sprintf(szLogFile,
                            "%c:\\watchdog.log",
                            doshQueryBootDrive());

                    LogFile = fopen(szLogFile, "a");

                    DosBeep(100, 100);

                    // now differentiate:

                    // 1) If the teaser thread was stuck in WinSendMsg,
                    //    that's the simple case where the target window
                    //    is not currently processing its message queue.
                    //    It is most probable that the hung process is
                    //    thus the process of the target window of
                    //    the WinSendMsg call, which was stored in
                    //    G_pidBeingSent (and has been copied to pidHung).
                    //
                    //    From my testing, in the simple case, all the
                    //    PMSEMS are unowned.
                    //
                    // 2) If the teaser thread got stuck in another PM
                    //    function however, it is quite probable that
                    //    some other PM thread is owning a global PMSEM.

                    // So to find candidates for "hung processes", we
                    // use the current target of WinSendMsg, plus all
                    // current owners of the global PMSEMs we find.

                    if (    (opHung == OP_SENDMSG)
                         && (pidBeingSent)
                       )
                    {
                        AppendHung(pidBeingSent,
                                   opHung,      // OP_SENDMSG
                                   -1);         // no sem yet
                    }

                    // run through the PMSEMs to see if
                    // they're owned
                    for (ulSemThis = 0;
                         ulSemThis <= LASTSEM;
                         ++ulSemThis)
                    {
                        // make sure this is really a PMSEM
                        if (    (!memcmp(paPMSems->szMagic, "PMSEM", 5))
                             || (!memcmp(paPMSems->szMagic, "GRESEM", 6))
                           )
                        {
                            ULONG pidtid;

                            if (pidtid = paPMSems[ulSemThis].ulOwnerPidTid)
                            {
                                AppendHung(LOUSHORT(pidtid),
                                           opHung,      // OP_SENDMSG
                                           ulSemThis);         // semid
                            }

                            if (LogFile)
                                fprintf(LogFile,
                                        "pmsem %d (%s) has owner %lX, %d waiters\n",
                                        ulSemThis,
                                        G_papcszSems[ulSemThis],
                                        pidtid,
                                        paPMSems[ulSemThis].cWaiters);

                        }
                        else
                        {
                            // magic is invalid:
                            if (LogFile)
                                fprintf(LogFile,
                                        "magic for sem %d (%s) is invalid\n",
                                        ulSemThis,
                                        G_papcszSems[ulSemThis]);
                            break;
                        }
                    }

                    if (G_cHungs)
                    {
                        PQTOPLEVEL32    t;
                        PQPROCESS32     p;
                        PQMODULE32      m;

                        if (LogFile)
                                fprintf(LogFile,
                                        "!!! got %d hung processes\n",
                                        G_cHungs);

                        // now describe the hung processes
                        if (t = prc32GetInfo(NULL))
                        {
                            ULONG ulHung;
                            for (ulHung = 0;
                                 ulHung < G_cHungs;
                                 ++ulHung)
                            {
                                if (p = prc32FindProcessFromPID(t, G_aHungs[ulHung].pid))
                                {
                                    if (m = prc32FindModule(t, p->usHModule))
                                    {
                                        ULONG   sem = G_aHungs[ulHung].sem;
                                        PCSZ    pcszSem;

                                        if (sem < ARRAYITEMCOUNT(G_papcszSems))
                                            pcszSem = G_papcszSems[sem];
                                        else
                                            pcszSem = "no sem";

                                        sprintf(szTemp,
                                                "PID 0x%lX (%d) (%s) is hung (op %s, %s)",
                                                G_aHungs[ulHung].pid,
                                                G_aHungs[ulHung].pid,
                                                m->pcName,      // module name
                                                GetOpName(G_aHungs[ulHung].op),
                                                pcszSem
                                              );

                                        if (LogFile)
                                            fprintf(LogFile,
                                                    "hung process [%d]: %s\n",
                                                    ulHung,
                                                    szTemp);

                                        apsz[ulHung] = strdup(szTemp);
                                    }
                                }
                            }

                            prc32FreeInfo(t);
                        }
                    } // end if (cHungProcesses)

                    if (LogFile)
                        fclose(LogFile);

                } // end else if (opHung == OP_IDLE)

                // copy var locally for painting
                cPaint = G_cHungs;

                UnlockHungs();
                semHungs = FALSE;
            } // if (semHungs = LockHungs())

            if (cPaint)
            {
                // found any hung processes:
                if (hpsScreen = WinGetScreenPS(HWND_DESKTOP))
                {
                    #define CX_BORDER   10
                    #define CY_BOTTOM   30
                    #define SPACING     5
                    #define LINEHEIGHT  20

                    RECTL   rcl;
                    POINTL  ptl;
                    ULONG   ulPaint;

                    rcl.xLeft = CX_BORDER;
                    rcl.yBottom = CY_BOTTOM;
                    rcl.xRight = cxScreen - 2 * CX_BORDER;
                    rcl.yTop =   rcl.yBottom
                               + 2 * SPACING
                               + cPaint * LINEHEIGHT;

                    WinFillRect(hpsScreen,
                                &rcl,
                                CLR_WHITE);

                    ptl.x = CX_BORDER + SPACING;
                    ptl.y = rcl.yTop - SPACING - LINEHEIGHT;

                    GpiSetColor(hpsScreen, CLR_BLACK);

                    for (ulPaint = 0;
                         ulPaint < cPaint;
                         ++ulPaint)
                    {
                        PSZ psz;
                        if (psz = apsz[ulPaint])
                        {
                            GpiCharStringAt(hpsScreen,
                                            &ptl,
                                            strlen(psz),
                                            psz);
                            free(psz);

                            ptl.y -= LINEHEIGHT;
                        }
                    }

                    WinReleasePS(hpsScreen);
                    hpsScreen = NULLHANDLE;
                } // if (hpsScreen = WinGetScreenPS(HWND_DESKTOP))
            } // if (cPaint)

            DosSleep(1000);
        } // while (!ptiMyself->fExit)
    }
    CATCH(excpt1)
    {
    } END_CATCH();

    if (semWatchdog)
        UnlockWatchdog();

    if (semHungs)
        UnlockHungs();

    if (hpsScreen)
        WinReleasePS(hpsScreen);
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
