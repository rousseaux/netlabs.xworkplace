
/*
 *@@sourcefile kernel.c:
 *      this file has code which I considered to be "internal"
 *      enough to be called the "XWorkplace kernel".
 *
 *      With V0.9.0, this code has been moved to shared\ to allow
 *      for extension by other programmers which need certain code
 *      to be running on thread 1 also.
 *
 *      In detail, we have:
 *      -- the KERNELGLOBALS interface (krnQueryGlobals);
 *      -- the thread-1 object window (krn_fnwpThread1Object);
 *      -- the XWorkplace initialization code (krnInitializeXWorkplace).
 *
 *      In this file, I have assembled code which you might consider
 *      useful for extensions. For example, if you need code to
 *      execute on thread 1 of PMSHELL.EXE (which is required for
 *      some WPS methods to work, unfortunately), you can add a
 *      message to be processed in krn_fnwpThread1Object.
 *
 *      If you need stuff to be executed upon WPS startup, you can
 *      insert a function into krnInitializeXWorkplace.
 *
 *      All functions in this file have the "krn*" prefix (V0.9.0).
 *
 *@@header "shared\kernel.h"
 */

/*
 *      Copyright (C) 1997-2000 Ulrich M”ller.
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

/*
 *@@todo:
 *
 */

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

#define INCL_DOSEXCEPTIONS
#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#define INCL_DOSQUEUES
#define INCL_DOSERRORS

#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WINMESSAGEMGR
#define INCL_WINTIMER
#define INCL_WINSYS
#define INCL_WINPOINTERS
#define INCL_WINDIALOGS
#define INCL_WINBUTTONS
#define INCL_WINPROGRAMLIST     // needed for PROGDETAILS, wppgm.h
#define INCL_WINSWITCHLIST
#define INCL_WINSHELLDATA
#include <os2.h>

// C library headers
#include <stdio.h>              // needed for except.h
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h
#include <io.h>                 // access etc.
#include <fcntl.h>
#include <sys\stat.h>

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\comctl.h"             // common controls (window procs)
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\procstat.h"           // DosQProcStat handling
#include "helpers\stringh.h"            // string helper routines
#include "helpers\winh.h"               // PM helper routines
#include "helpers\xstring.h"            // extended string helpers

// SOM headers which don't crash with prec. header files
#pragma hdrstop                         // VAC++ keeps crashing otherwise
#include "xfldr.h"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

#include "filesys\refresh.h"            // folder auto-refresh
#include "filesys\program.h"            // program implementation
#include "filesys\xthreads.h"           // extra XWorkplace threads

#include "media\media.h"                // XWorkplace multimedia support

#include "security\xwpsecty.h"          // XWorkplace Security

#include "startshut\archives.h"         // WPSArcO declarations
#include "startshut\shutdown.h"         // XWorkplace eXtended Shutdown

// headers in /hook
#include "hook\xwphook.h"

// other SOM headers
// #include <wpdesk.h>                     // WPDesktop

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

// global lock semaphore for krnLock etc.
static HMTX                G_hmtxCommonLock = NULLHANDLE;

// "Quick open" dlg status (thread-1 object wnd)
static ULONG               G_ulQuickOpenNow = 0,
                           G_ulQuickOpenMax = 0;
static HWND                G_hwndQuickStatus = NULLHANDLE;
static BOOL                G_fQuickOpenCancelled = FALSE;

// flags passed with mp1 of XDM_PAGEMAGECONFIG
static ULONG               G_PageMageConfigFlags = 0;

// global structure with data needed across threads
// (see kernel.h)
static KERNELGLOBALS       G_KernelGlobals = {0};

// resize information for ID_XFD_CONTAINERPAGE, which is used
// by many settings pages
MPARAM G_ampGenericCnrPage[] =
    {
        MPFROM2SHORT(ID_XFDI_CNR_GROUPTITLE, XAC_SIZEX | XAC_SIZEY),
        MPFROM2SHORT(ID_XFDI_CNR_CNR, XAC_SIZEX | XAC_SIZEY)
    };

extern MPARAM *G_pampGenericCnrPage = G_ampGenericCnrPage;
extern ULONG G_cGenericCnrPage = sizeof(G_ampGenericCnrPage) / sizeof(G_ampGenericCnrPage[0]);

// forward declarations
MRESULT EXPENTRY fnwpStartupDlg(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
MRESULT EXPENTRY fncbStartup(HWND hwndStatus, ULONG ulObject, MPARAM mpNow, MPARAM mpMax);
MRESULT EXPENTRY fnwpQuickOpenDlg(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
MRESULT EXPENTRY fncbQuickOpen(HWND hwndFolder, ULONG ulObject, MPARAM mpNow, MPARAM mpMax);

/* ******************************************************************
 *
 *   Resource protection (thread safety)
 *
 ********************************************************************/

const char  *G_pcszReqSourceFile = NULL;
ULONG       G_ulReqLine = 0;
const char  *G_pcszReqFunction = NULL;

/*
 *@@ krnLock:
 *      function to request the global hmtxCommonLock
 *      semaphore to finally make the kernel functions
 *      thread-safe. While this semaphore is held,
 *      all other threads are kept from accessing
 *      XWP kernel data.
 *
 *      Returns TRUE if the semaphore could be accessed
 *      within the specified timeout.
 *
 *      As parameters to this function, pass the caller's
 *      source file, line number, and function name.
 *      This is stored internally so that the xwplog.log
 *      file can report error messages properly if the
 *      mutex is not released.
 *
 *      The string pointers must be static const strings.
 *      Use "__FILE__, __LINE__, __FUNCTION__" always.
 *
 *      Note: This requires the existence of a message
 *      queue since we use WinRequestMutexSem. Also
 *      make sure that your code is properly protected
 *      with exception handlers (see helpers\except.c
 *      for remarks about that).
 *
 *      Proper usage:
 *
 +          BOOL fLocked = FALSE;
 +          ULONG ulNesting;
 +          DosEnterMustComplete(&ulNesting);
 +          TRY_LOUD(excpt1)
 +          {
 +              fLocked = krnLock(__FILE__, __LINE__, __FUNCTION__);
 +              if (fLocked)
 +              {
 +                  // ... precious code here
 +              }
 +          }
 +          CATCH(excpt1) { } END_CATCH();
 +
 +          if (fLocked)
 +              krnUnlock();        // NEVER FORGET THIS!!
 +          DosExitMustComplete(&ulNesting);
 *
 *@@added V0.9.0 (99-11-14) [umoeller]
 *@@changed V0.9.3 (2000-04-08) [umoeller]: moved this here from common.c
 *@@changed V0.9.7 (2000-12-13) [umoeller]: changed prototype to trace locks
 */

BOOL krnLock(const char *pcszSourceFile,        // in: __FILE__
             ULONG ulLine,                      // in: __LINE__
             const char *pcszFunction)          // in: __FUNCTION__
{
    if (G_hmtxCommonLock == NULLHANDLE)
        // first call:
        return (DosCreateMutexSem(NULL,         // unnamed
                                  &G_hmtxCommonLock,
                                  0,            // unshared
                                  TRUE)         // request now
                    == NO_ERROR);

    // subsequent calls:
    if (WinRequestMutexSem(G_hmtxCommonLock, 10*1000) == NO_ERROR)
    {
        // store owner (these are const strings, this is safe)
        G_pcszReqSourceFile = pcszSourceFile;
        G_ulReqLine = ulLine;
        G_pcszReqFunction = pcszFunction;
        return (TRUE);
    }
    else
    {
        // request failed within ten seconds:
        cmnLog(__FILE__, __LINE__, __FUNCTION__,
               "krnLock mutex request failed!!\n"
               "    First requestor: %s (%s, line %d))\n"
               "    Second (failed) requestor: %s (%s, line %d))\n",
               (G_pcszReqFunction) ? G_pcszReqFunction : "NULL",
               (G_pcszReqSourceFile) ? G_pcszReqSourceFile : "NULL",
               G_ulReqLine,
               pcszFunction,
               pcszSourceFile,
               ulLine);

        return (FALSE);
    }
}

/*
 *@@ krnUnlock:
 *
 *@@added V0.9.0 (99-11-14) [umoeller]
 *@@changed V0.9.3 (2000-04-08) [umoeller]: moved this here from common.c
 */

VOID krnUnlock(VOID)
{
    DosReleaseMutexSem(G_hmtxCommonLock);
}

/*
 *@@ krnQueryLock:
 *      returns the thread ID which currently owns
 *      the common lock semaphore or 0 if the semaphore
 *      is not owned (not locked).
 *
 *@@added V0.9.1 (2000-01-30) [umoeller]
 *@@changed V0.9.3 (2000-04-08) [umoeller]: moved this here from common.c
 */

ULONG krnQueryLock(VOID)
{
    PID     pid = 0;
    TID     tid = 0;
    ULONG   ulCount = 0;
    if (DosQueryMutexSem(G_hmtxCommonLock,
                         &pid,
                         &tid,
                         &ulCount)
            == NO_ERROR)
        return (tid);

    return (0);
}

/*
 *krnOnKillDuringLock:
 *      function to be used with the TRY_xxx
 *      macros to release the mutex semaphore
 *      on thread kills.
 *
 *added V0.9.0 (99-11-14) [umoeller]
 *changed V0.9.3 (2000-04-08) [umoeller]: moved this here from common.c
 *removed V0.9.7 (2000-12-10) [umoeller]
 */

/* VOID APIENTRY krnOnKillDuringLock(PEXCEPTIONREGISTRATIONRECORD2 pRegRec2)
{
    DosReleaseMutexSem(G_hmtxCommonLock);
} */

/********************************************************************
 *
 *   KERNELGLOBALS structure
 *
 ********************************************************************/

PCKERNELGLOBALS krnQueryGlobals(VOID)
{
    return &G_KernelGlobals;
}

/*
 *@@ krnLockGlobals:
 *      this returns the global KERNELGLOBALS structure
 *      which contains all kinds of data which need to
 *      be accessed across threads. This structure is
 *      a global structure in kernel.c.
 *
 *      This calls krnLock to lock the globals.
 *
 *@@changed V0.9.7 (2000-12-13) [umoeller]: changed prototype to trace locks
 */

PKERNELGLOBALS krnLockGlobals(const char *pcszSourceFile,
                              ULONG ulLine,
                              const char *pcszFunction)
{
    if (krnLock(pcszSourceFile, ulLine, pcszFunction))
        return (&G_KernelGlobals);
    else
        return (NULL);
}

/*
 *@@ krnUnlockGlobals:
 *
 */

VOID krnUnlockGlobals(VOID)
{
    krnUnlock();
}

/* ******************************************************************
 *
 *   XFolder hook for exception handlers (\helpers\except.c)
 *
 ********************************************************************/

/*
 *@@ krnExceptOpenLogFile:
 *      this opens or creates C:\XFLDTRAP.LOG and writes
 *      a debug header into it (date and time); returns
 *      a FILE* pointer for fprintf(), so additional data can
 *      be written. You should use fclose() to close the file.
 *
 *      This is an "exception hook" which is registered with
 *      the generic exception handlers in src\helpers\except.c.
 *      This code gets called from there whenever an exception
 *      occurs, but only with the "loud" exception handler.
 *
 *@@changed V0.9.0 [umoeller]: moved this stuff here from except.c
 *@@changed V0.9.0 [umoeller]: renamed function
 *@@changed V0.9.2 (2000-03-10) [umoeller]: switched date format to ISO
 */

FILE* _System krnExceptOpenLogFile(VOID)
{
    CHAR        szFileName[CCHMAXPATH];
    FILE        *file;

    sprintf(szFileName, "%c:\\%s", doshQueryBootDrive(), XFOLDER_CRASHLOG);
    file = fopen(szFileName, "a");

    if (file)
    {
        DATETIME    dt;
        DosGetDateTime(&dt);
        fprintf(file, "\nXWorkplace trap message -- Date: %04d-%02d-%02d, Time: %02d:%02d:%02d\n",
                dt.year, dt.month, dt.day,
                dt.hours, dt.minutes, dt.seconds);
        fprintf(file, "----------------------------------------------------------\n"
                      "\nAn internal error occurred within XWorkplace.\n"
                      "Please contact the author so that this error may be removed\n"
                      "in future XWorkplace versions. A contact address may be\n"
                      "obtained from the XWorkplace User Guide. Please supply\n"
                      "this file (?:\\" XFOLDER_CRASHLOG " with your e-mail and describe as\n"
                      "exactly as possible the conditions under which the error\n"
                      "occured.\n"
                      "\nRunning XWorkplace version: " XFOLDER_VERSION " built " __DATE__ "\n");

    }
    return (file);
}

/*
 *@@ krnExceptExplainXFolder:
 *      this is the only XFolder-specific information
 *      which is written to the logfile.
 *
 *      This is an "exception hook" which is registered with
 *      the generic exception handlers in src\helpers\except.c.
 *      This code gets called from there whenever an exception
 *      occurs, but only with the "loud" exception handler.
 *
 *@@changed V0.9.0 [umoeller]: moved this stuff here from except.c
 *@@changed V0.9.0 [umoeller]: renamed function
 *@@changed V0.9.1 (99-12-28) [umoeller]: updated written information; added File thread
 */

VOID _System krnExceptExplainXFolder(FILE *file,      // in: logfile from fopen()
                                     PTIB ptib)       // in: thread info block
{
    PID         pid;
    TID         tid;
    ULONG       ulCount;
    APIRET      arc;
    PCKERNELGLOBALS pKernelGlobals = krnQueryGlobals();

    // *** thread info
    if (ptib)
    {
        if (ptib->tib_ptib2)
        {
            // find out which thread trapped
            tid = ptib->tib_ptib2->tib2_ultid;

            fprintf(file,
                "Crashing thread:\n    TID 0x%lX (%d) ",
                tid,        // hex
                tid);       // dec

            if (tid == thrQueryID(&pKernelGlobals->tiWorkerThread))
                fprintf(file, " (XWorkplace Worker thread)");
            else if (tid == thrQueryID(&pKernelGlobals->tiSpeedyThread))
                fprintf(file, " (XWorkplace Speedy thread)");
            else if (tid == thrQueryID(&pKernelGlobals->tiFileThread))
                fprintf(file, " (XWorkplace File thread)");
            else if (tid == thrQueryID(&pKernelGlobals->tiUpdateThread))
                fprintf(file, " (XWorkplace Update thread)");
            else if (tid == thrQueryID(&pKernelGlobals->tiShutdownThread))
                fprintf(file, " (XWorkplace Shutdown thread)");
            else if (tid == pKernelGlobals->tidWorkplaceThread)
                fprintf(file, " (PMSHELL's Workplace thread)");
            else
                fprintf(file, " (unknown thread)");

            fprintf(file,
                    "\n    Thread priority: 0x%lX, ordinal: 0x%lX\n",
                    ptib->tib_ptib2->tib2_ulpri,
                    ptib->tib_ordinal);
        }
        else
            fprintf(file, "Thread ID cannot be determined.\n");
    } else
        fprintf(file, "Thread information was not available.\n");

    // running XFolder threads
    fprintf(file, "\nThe following running XWorkplace threads could be identified:\n");

    fprintf(file,  "    PMSHELL Workplace thread ID: 0x%lX\n", pKernelGlobals->tidWorkplaceThread);

    if (tid = thrQueryID(&pKernelGlobals->tiWorkerThread))
        fprintf(file,  "    XWorkplace Worker thread ID: 0x%lX (%d)\n", tid, tid);

    if (tid = thrQueryID(&pKernelGlobals->tiSpeedyThread))
        fprintf(file,  "    XWorkplace Speedy thread ID: 0x%lX (%d)\n", tid, tid);

    if (tid = thrQueryID(&pKernelGlobals->tiFileThread))
        fprintf(file,  "    XWorkplace File thread ID: 0x%lX (%d)\n", tid, tid);

    if (tid = thrQueryID(&pKernelGlobals->tiShutdownThread))
        fprintf(file,  "    XWorkplace Shutdown thread ID: 0x%lX (%d)\n", tid, tid);

    if (tid = thrQueryID(&pKernelGlobals->tiUpdateThread))
        fprintf(file,  "    XWorkplace Update thread ID: 0x%lX (%d)\n", tid, tid);

    tid = krnQueryLock();
    if (tid)
        fprintf(file, "\nGlobal lock semaphore is currently owned by thread 0x%lX (%u).\n", tid, tid);
    else
        fprintf(file, "\nGlobal lock semaphore is currently not owned.\n", tid, tid);

    arc = DosQueryMutexSem(pKernelGlobals->hmtxAwakeObjects,
                           &pid, &tid, &ulCount);
    if ((arc == NO_ERROR) && (tid))
        fprintf(file, "Awake-objects semaphore is currently owned by thread 0x%lX (%u) (request count: %d).\n",
                      tid, tid, ulCount);
    else
        fprintf(file, "Awake-objects semaphore is currently not owned (request count: %d).\n",
                tid, tid, ulCount);
}

/*
 *@@ krnExceptError:
 *      this is an "exception hook" which is registered with
 *      the generic exception handlers in src\helpers\except.c.
 *      This code gets called whenever a TRY_* macro fails to
 *      install an exception handler.
 *
 *@@added V0.9.2 (2000-03-10) [umoeller]
 */

VOID APIENTRY krnExceptError(const char *pcszFile,
                             ULONG ulLine,
                             const char *pcszFunction,
                             APIRET arc)     // in: DosSetExceptionHandler error code
{
    cmnLog(pcszFile, ulLine, pcszFunction,
           "TRY_* macro failed to install exception handler (APIRET %d)",
           arc);
}

/*
 *@@ krnMemoryError:
 *      reports memory error msgs if XWorkplace is
 *      compiled in debug mode _and_ memory debugging
 *      is enabled.
 *
 *@@added V0.9.3 (2000-04-11) [umoeller]
 */

VOID krnMemoryError(const char *pcszMsg)
{
    cmnLog(__FILE__, __LINE__, __FUNCTION__,
           "Memory error:\n    %s",
           pcszMsg);
}

/* ******************************************************************
 *
 *   Startup/Daemon interface
 *
 ********************************************************************/

/*
 *@@ krnSetProcessStartupFolder:
 *      this gets called during XShutdown to set
 *      the flag in the DAEMONSHARED shared-memory
 *      structure whether the XWorkplace startup
 *      folder should be re-used at the next WPS
 *      startup.
 *
 *      This is only meaningful between WPS restarts,
 *      because this flag does not get stored anywhere.
 *
 *@@added V0.9.0 [umoeller]
 */

VOID krnSetProcessStartupFolder(BOOL fReuse)
{
    PKERNELGLOBALS pKernelGlobals = NULL;
    ULONG ulNesting;
    DosEnterMustComplete(&ulNesting);
    TRY_LOUD(excpt1)
    {
        pKernelGlobals = krnLockGlobals(__FILE__, __LINE__, __FUNCTION__);
        if (pKernelGlobals)
        {
            if (pKernelGlobals->pDaemonShared)
            {
                // cast PVOID
                PDAEMONSHARED pDaemonShared = pKernelGlobals->pDaemonShared;
                pDaemonShared->fProcessStartupFolder = fReuse;
            }
        }
    }
    CATCH(excpt1) {} END_CATCH();

    if (pKernelGlobals)
        krnUnlockGlobals();

    DosExitMustComplete(&ulNesting);
}

/*
 *@@ krnNeed2ProcessStartupFolder:
 *      this returns TRUE if the startup folder needs to
 *      be processed. See krnSetProcessStartupFolder.
 *
 *@@changed V0.9.0 [umoeller]: completely rewritten; now using DAEMONSHARED shared memory.
 */

BOOL krnNeed2ProcessStartupFolder(VOID)
{
    BOOL brc = FALSE;
    PCKERNELGLOBALS pKernelGlobals = krnQueryGlobals();

    if (pKernelGlobals)
    {
        if (pKernelGlobals->pDaemonShared)
        {
            // cast PVOID
            PDAEMONSHARED pDaemonShared = pKernelGlobals->pDaemonShared;
            if (pDaemonShared->fProcessStartupFolder)
            {
                brc = TRUE;
                pDaemonShared->fProcessStartupFolder = FALSE;
            }
        }
    }

    return (brc);
}

/*
 *@@ krnPostDaemonMsg:
 *      this posts a message to the XWorkplace
 *      Daemon (XWPDAEMN.EXE). Returns TRUE if
 *      successful.
 *
 *      If FALSE is returned, the daemon is probably
 *      not running.
 *
 *@@added V0.9.0 [umoeller]
 */

BOOL krnPostDaemonMsg(ULONG msg, MPARAM mp1, MPARAM mp2)
{
    BOOL brc = FALSE;
    PCKERNELGLOBALS pKernelGlobals = krnQueryGlobals();
    if (pKernelGlobals)
    {
        // cast PVOID
        PDAEMONSHARED pDaemonShared = pKernelGlobals->pDaemonShared;
        if (!pDaemonShared)
            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                   "pDaemonShared is NULL.");
        else
            // get the handle of the daemon's object window
            if (!pDaemonShared->hwndDaemonObject)
                cmnLog(__FILE__, __LINE__, __FUNCTION__,
                       "pDaemonShared->hwndDaemonObject is NULLHANDLE.");
            else
                brc = WinPostMsg(pDaemonShared->hwndDaemonObject, msg, mp1, mp2);
    }

    return (brc);
}

/* ******************************************************************
 *
 *   Thread-1 object window
 *
 ********************************************************************/

BOOL     fLimitMsgOpen = FALSE;
HWND     hwndArchiveStatus = NULLHANDLE;

/*
 *@@ krn_T1M_DaemonReady:
 *      implementation for T1M_DAEMONREADY.
 *
 *@@added V0.9.3 (2000-04-24) [umoeller]
 */

VOID krn_T1M_DaemonReady(VOID)
{
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    // _Pmpf(("krn_T1M_DaemonReady"));

    if (G_KernelGlobals.pDaemonShared)
    {
        // cast PVOID
        PDAEMONSHARED pDaemonShared = G_KernelGlobals.pDaemonShared;
        if (    (pGlobalSettings->fEnableXWPHook)
             && (pDaemonShared->hwndDaemonObject)
           )
        {
            if (WinSendMsg(pDaemonShared->hwndDaemonObject,
                           XDM_HOOKINSTALL,
                           (MPARAM)TRUE,
                           0))
            {
                // success:
                // notify daemon of Desktop window;
                // this is still NULLHANDLE if we're
                // currently starting the WPS
                HWND hwndActiveDesktop = cmnQueryActiveDesktopHWND();
                // _Pmpf(("  Posting XDM_DESKTOPREADY (0x%lX)",
                //         hwndActiveDesktop));
                krnPostDaemonMsg(XDM_DESKTOPREADY,
                                 (MPARAM)hwndActiveDesktop,
                                 (MPARAM)0);

                // _Pmpf(("    pGlobalSettings->fPageMageEnabled: %d",
                //        pGlobalSettings->fEnablePageMage));

// #ifdef __PAGEMAGE__
                if (pGlobalSettings->fEnablePageMage)
                    // PageMage is enabled too:
                    WinSendMsg(pDaemonShared->hwndDaemonObject,
                               XDM_STARTSTOPPAGEMAGE,
                               (MPARAM)TRUE,
                               0);
// #endif
            }
        }
    }
}

/*
 *@@ krn_T1M_OpenObjectFromHandle:
 *      implementation for T1M_OPENOBJECTFROMHANDLE in
 *      krn_fnwpThread1Object.
 *
 *      Parameters:
 *      -- HOBJECT mp1: object handle to open.
 *              The following "special objects" exist:
 *              0xFFFF0000: show window list.
 *              0xFFFF0001: show Desktop's context menu.
 *      -- ULONG mp2: corner reached;
 *                  1 = lower left,
 *                  2 = top left,
 *                  3 = lower right,
 *                  4 = top right;
 *                  0 = no corner, probably object hotkey.
 *
 *@@added V0.9.3 (2000-04-20) [umoeller]
 *@@changed V0.9.3 (2000-04-20) [umoeller]: added system sound
 *@@changed V0.9.4 (2000-06-12) [umoeller]: fixed desktop menu position
 *@@changed V0.9.4 (2000-06-15) [umoeller]: fixed VIO windows in background
 *@@changed V0.9.7 (2000-11-29) [umoeller]: fixed memory leak
 */

VOID krn_T1M_OpenObjectFromHandle(HWND hwndObject,
                                  MPARAM mp1,
                                  MPARAM mp2)
{
    if (mp1)
    {
        HOBJECT hobjStart = (HOBJECT)mp1;
        if ((ULONG)hobjStart < 0xFFFF0000)
        {
            // normal object handle:
            WPObject *pobjStart = _wpclsQueryObject(_WPObject,
                                                    hobjStart);

            #ifdef DEBUG_KEYS
                _Pmpf(("T1Object: received hobj 0x%lX -> 0x%lX",
                        hobjStart,
                        pobjStart));
            #endif

            if (pobjStart)
            {
                HWND hwnd;

                if ((ULONG)mp2 == 0)
                    // object hotkey, not screen corner:
                    cmnPlaySystemSound(MMSOUND_XFLD_HOTKEYPRSD);
                                // V0.9.3 (2000-04-20) [umoeller]

                // open the object, or resurface if already open
                hwnd = _wpViewObject(pobjStart,
                                     NULLHANDLE,   // hwndCnr (?!?)
                                     OPEN_DEFAULT,
                                     0);           // "optional parameter" (?!?)

                #ifdef DEBUG_KEYS
                    _Pmpf(("krn_T1M_OpenObjectFromHandle: opened hwnd 0x%lX", hwnd));
                #endif

                if (hwnd)
                {
                    if (WinIsWindow(WinQueryAnchorBlock(hwndObject),
                                    hwnd))
                    {
                        // it's a window:
                        // move to front
                        WinSetActiveWindow(HWND_DESKTOP, hwnd);
                    }
                    else
                    {
                        // wpViewObject only returns a window handle for
                        // WPS windows. By contrast, if a program object is
                        // started, an obscure USHORT value is returned.
                        // I suppose this is a HAPP instead.
                        // From my testing, the lower byte (0xFF) contains
                        // the session ID of the started application, while
                        // the higher byte (0xFF00) contains the application
                        // type, which is:
                        // --   0x0300  presentation manager
                        // --   0x0200  VIO

                        // IBM, this is sick.

                        // So now we go thru the switch list and find the
                        // session which has this lo-byte. V0.9.4 (2000-06-15) [umoeller]
                        ULONG   cbItems = WinQuerySwitchList(NULLHANDLE, NULL, 0),
                                ul;
                        ULONG   ulBufSize = (cbItems * sizeof(SWENTRY)) + sizeof(HSWITCH);
                        PSWBLOCK pSwBlock = (PSWBLOCK)malloc(ulBufSize);
                        if (pSwBlock)
                        {
                            cbItems = WinQuerySwitchList(NULLHANDLE, pSwBlock, ulBufSize);

                            // loop through all the tasklist entries
                            for (ul = 0; ul < (pSwBlock->cswentry); ul++)
                            {
                                #ifdef DEBUG_KEYS
                                    _Pmpf((" swlist %d: hwnd 0x%lX, hprog 0x%lX, idSession 0x%lX",
                                            ul,
                                            pSwBlock->aswentry[ul].swctl.hwnd,
                                            pSwBlock->aswentry[ul].swctl.hprog, // always 0...
                                            pSwBlock->aswentry[ul].swctl.idSession));
                                #endif

                                if (pSwBlock->aswentry[ul].swctl.idSession == (hwnd & 0xFF))
                                {
                                    // got it:
                                    #ifdef DEBUG_KEYS
                                        _Pmpf(("      Found!"));
                                    #endif

                                    WinSetActiveWindow(HWND_DESKTOP,
                                                       pSwBlock->aswentry[ul].swctl.hwnd);
                                }
                            }
                            free(pSwBlock);  // V0.9.7 (2000-11-29) [umoeller]
                        }
                    }
                }
            }
        }
        else
        {
            // special objects:
            switch ((ULONG)hobjStart)
            {
                case 0xFFFF0000:
                    // show window list
                    WinPostMsg(cmnQueryActiveDesktopHWND(),
                               WM_COMMAND,
                               (MPARAM)0x8011,
                               MPFROM2SHORT(CMDSRC_MENU,
                                            TRUE));
                break;

                case 0xFFFF0001:
                {
                    // show Desktop's context menu V0.9.1 (99-12-19) [umoeller]
                    WPObject* pActiveDesktop = cmnQueryActiveDesktop();
                    HWND hwndFrame = cmnQueryActiveDesktopHWND();
                    if ((pActiveDesktop) && (hwndFrame))
                    {
                        HWND hwndClient = wpshQueryCnrFromFrame(hwndFrame);
                        POINTL ptlPopup = { 0, 0 }; // default: lower left
                        WinQueryPointerPos(HWND_DESKTOP, &ptlPopup);
                        /* switch ((ULONG)mp2)
                        {
                            // corner reached:
                            case 2: // top left
                                ptlPopup.x = 0;
                                ptlPopup.y = WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN);
                            break;

                            case 3: // lower right
                                ptlPopup.x = WinQuerySysValue(HWND_DESKTOP, SV_CXSCREEN);
                                ptlPopup.y = 0;
                            break;

                            case 4: // top right
                                ptlPopup.x = WinQuerySysValue(HWND_DESKTOP, SV_CXSCREEN);
                                ptlPopup.y = WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN);
                            break;
                        } */
                        _wpDisplayMenu(pActiveDesktop,
                                       hwndFrame,       // owner
                                       hwndClient,
                                       &ptlPopup,
                                       MENU_OPENVIEWPOPUP,
                                       0);      // reserved
                    }
                break; }
            }
        }
    }
}

/*
 *@@ krn_fnwpThread1Object:
 *      wnd proc for the thread-1 object window.
 *
 *      This is needed for processing messages which must be
 *      processed on thread 1. We cannot however process these
 *      messages in the subclassed folder frame wnd proc
 *      (fdr_fnwpSubclassedFolderFrame in folder.c),
 *      because adding user messages to that wnd proc could
 *      conflict with default WPFolder messages or those of
 *      some other WPS enhancer, and we can also never be
 *      sure whether the folder window proc is really running
 *      on thread 1. Sometimes it isn't. ;-)
 *
 *      We therefore create another object window, which we
 *      own all alone.
 *
 *      Even though the PM docs say that an object window should never
 *      be created on thread 1 (which we're doing here), because this
 *      would "slow down the system", this is not generally true.
 *      WRT performance, it doesn't matter if an object window or a
 *      frame window processes messages. And since we _need_ to process
 *      some messages on the Workplace thread (1), especially when
 *      manipulating WPS windows, we do it here.
 *
 *      But of course, since this is on thread 1, we must get out of
 *      here quickly (0.1 seconds rule), because while we're processing
 *      something in here, the WPS user interface ist blocked.
 *
 *      Note: Another view-specific object window is created
 *      for every folder view that is opened, because sometimes
 *      folder views do _not_ run on thread 1 and manipulating
 *      frame controls from thread 1 would then hang the PM.
 *      See fdr_fnwpSupplFolderObject in filesys\folder.c for details.
 *
 *@@changed V0.9.0 [umoeller]: T1M_QUERYXFOLDERVERSION message handling
 *@@changed V0.9.0 [umoeller]: T1M_OPENOBJECTFROMHANDLE added
 *@@changed V0.9.1 (99-12-19) [umoeller]: added "show Desktop menu" to T1M_OPENOBJECTFROMHANDLE
 *@@changed V0.9.2 (2000-02-23) [umoeller]: added T1M_PAGEMAGECLOSED
 *@@changed V0.9.3 (2000-04-09) [umoeller]: added T1M_PAGEMAGECONFIGDELAYED
 *@@changed V0.9.3 (2000-04-09) [umoeller]: fixed timer problem, which was never stopped... this solves the "disappearing windows" problem!!
 *@@changed V0.9.3 (2000-04-25) [umoeller]: startup folder was permanently disabled when panic flag was set; fixed
 *@@changed V0.9.4 (2000-06-05) [umoeller]: added exception handling
 *@@changed V0.9.6 (2000-10-16) [umoeller]: added WM_APPTERMINATENOTIFY
 */

MRESULT EXPENTRY krn_fnwpThread1Object(HWND hwndObject, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MPARAM  mrc = NULL;
    BOOL    fCallDefault = FALSE;

    TRY_LOUD(excpt1)
    {
        switch(msg)
        {
            /*
             * WM_TIMER:
             *
             */

            case WM_TIMER:
                switch ((USHORT)mp1)    // timer ID
                {
                    case 1:
                        // archive status timer
                        WinDestroyWindow(hwndArchiveStatus);
                    break;

                    case 2: // started from T1M_PAGEMAGECONFIGDELAYED
                    {
                        PDAEMONSHARED   pDaemonShared = G_KernelGlobals.pDaemonShared;

                        if (pDaemonShared)
                            if (pDaemonShared->hwndDaemonObject)
                            {
// #ifdef __PAGEMAGE__
                                // cross-process send msg: this
                                // does not return until the daemon
                                // has re-read the data
                                BOOL brc = (BOOL)WinSendMsg(pDaemonShared->hwndDaemonObject,
                                                            XDM_PAGEMAGECONFIG,
                                                            (MPARAM)G_PageMageConfigFlags,
                                                            0);
// #endif
                                // reset flags
                                G_PageMageConfigFlags = 0;
                            }
                    break; }

                }

                // stop timer; this was missing!! V0.9.3 (2000-04-09) [umoeller]
                WinStopTimer(WinQueryAnchorBlock(hwndObject),
                             hwndObject,
                             (USHORT)mp1);      // timer ID
            break;

            /*
             * WM_APPTERMINATENOTIFY:
             *      this gets posted from PM since we use
             *      this object window as the notify window
             *      to WinStartApp when we start program objects
             *      (progOpenProgram, filesys/program.c).
             *
             *      We must then remove source emphasis for
             *      the corresponding object.
             */

            case WM_APPTERMINATENOTIFY:
                /* _Pmpf((__FUNCTION__ ": WM_APPTERMINATENOTIFY happ 0x%lX ulrc %d",
                        mp1, mp2)); */
                progAppTerminateNotify((HAPP)mp1);
            break;

            /*
             *@@ T1M_BEGINSTARTUP:
             *      this is an XFolder msg posted by the Worker thread after
             *      the Desktop has been populated; this performs initialization
             *      of the Startup folder process, which will then be further
             *      performed by (again) the Worker thread.
             *
             *      Parameters: none.
             */

            case T1M_BEGINSTARTUP:
            {
                PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
                PCKERNELGLOBALS pKernelGlobals = krnQueryGlobals();
                XFolder         *pStartupFolder;

                #ifdef DEBUG_STARTUP
                    _Pmpf(("T1M_BEGINSTARTUP"));
                #endif

                // no: find startup folder
                if (pStartupFolder = _wpclsQueryFolder(_WPFolder, (PSZ)XFOLDER_STARTUPID, TRUE))
                {
                    // startup folder exists: create status window w/ progress bar,
                    // start folder content processing in worker thread
                    ULONG   hPOC;
                    HWND    hwndStartupStatus = WinLoadDlg(HWND_DESKTOP, NULLHANDLE,
                                                           fnwpStartupDlg,
                                                           cmnQueryNLSModuleHandle(FALSE),
                                                           ID_XFD_STARTUPSTATUS,
                                                           NULL);
                    if (pGlobalSettings->ShowStartupProgress)
                    {
                        // get last window position from INI
                        winhRestoreWindowPos(hwndStartupStatus,
                                             HINI_USER,
                                             INIAPP_XWORKPLACE, INIKEY_WNDPOSSTARTUP,
                                             // move only, no resize
                                             SWP_MOVE | SWP_SHOW | SWP_ACTIVATE);
                    }

                    hPOC = _xwpBeginProcessOrderedContent(pStartupFolder,
                                                          pGlobalSettings->ulStartupObjectDelay,
                                                          &fncbStartup,
                                                          (ULONG)hwndStartupStatus);

                    WinSetWindowULong(hwndStartupStatus, QWL_USER, hPOC);
                }
                else
                    // startup folder does not exist:
                    xthrPostFileMsg(FIM_STARTUPFOLDERDONE,
                                    (MPARAM)1,      // done with startup
                                    MPNULL);
            break; }

            /*
             *@@ T1M_POCCALLBACK:
             *      this msg is posted from the Worker thread whenever
             *      a callback func needs to be called during those
             *      "process ordered content" (POC) functions (initiated by
             *      XFolder::xwpProcessOrderedContent); we need this message to
             *      have the callback func run on the folder's thread.
             *
             *      Parameters:
             *      --  PPROCESSCONTENTINFO mp1:
             *                          structure with all the information;
             *                          this routine must set the hwndView field
             *      --  mp2: always NULL.
             */

            case T1M_POCCALLBACK:
            {
                PPROCESSCONTENTINFO ppci = (PPROCESSCONTENTINFO)mp1;
                if (ppci)
                {
                    ppci->hwndView = (HWND)((*(ppci->pfnwpCallback))(ppci->ulCallbackParam,
                                                                    (ULONG)ppci->pObject,
                                                                    (MPARAM)ppci->ulObjectNow,
                                                                    (MPARAM)ppci->ulObjectMax));
                }
            break; }

            /*
             *@@ T1M_BEGINQUICKOPEN:
             *      this is posted by the Worker thread after the startup
             *      folder has been processed; we will now go for the
             *      "Quick Open" folders by populating them and querying
             *      all their icons. If this is posted for the first time,
             *      mp1 must be NULL; for subsequent postings, mp1 contains the
             *      folder to be populated or (-1) if it's the last.
             */

            case T1M_BEGINQUICKOPEN:
            {
                PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
                PCKERNELGLOBALS pKernelGlobals = krnQueryGlobals();
                BOOL fWorkToDo = FALSE;

                if (mp1 == NULL)
                {
                    // first posting: prepare

                    // "quick open" disabled because Shift key pressed?
                    if ((pKernelGlobals->ulPanicFlags & SUF_SKIPQUICKOPEN) == 0)
                    {
                        // no:
                        XFolder *pQuick = NULL;

                        // count quick-open folders
                        G_ulQuickOpenNow = 0;
                        G_ulQuickOpenMax = 0;
                        for (pQuick = _xwpclsQueryQuickOpenFolder(_XFolder, NULL);
                             pQuick;
                             pQuick = _xwpclsQueryQuickOpenFolder(_XFolder, pQuick))
                        {
                            G_ulQuickOpenMax++;
                        }

                        if (G_ulQuickOpenMax)
                        {
                            fWorkToDo = TRUE;
                            // if we have any quick-open folders: go
                            if (pGlobalSettings->ShowStartupProgress)
                            {
                                PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();
                                G_hwndQuickStatus = WinLoadDlg(HWND_DESKTOP, NULLHANDLE,
                                                               fnwpQuickOpenDlg,
                                                               cmnQueryNLSModuleHandle(FALSE),
                                                               ID_XFD_STARTUPSTATUS,
                                                               NULL);
                                WinSetWindowText(G_hwndQuickStatus,
                                                 pNLSStrings->pszQuickStatus);

                                winhRestoreWindowPos(G_hwndQuickStatus,
                                                     HINI_USER,
                                                     INIAPP_XWORKPLACE, INIKEY_WNDPOSSTARTUP,
                                                     SWP_MOVE | SWP_SHOW | SWP_ACTIVATE);
                                WinSendMsg(WinWindowFromID(G_hwndQuickStatus, ID_SDDI_PROGRESSBAR),
                                                           WM_UPDATEPROGRESSBAR,
                                                           (MPARAM)0,
                                                           (MPARAM)G_ulQuickOpenMax);
                                G_fQuickOpenCancelled = FALSE;
                            }

                            // get first quick-open folder
                            pQuick = _xwpclsQueryQuickOpenFolder(_XFolder, NULL);
                            if (pQuick)
                                krnPostThread1ObjectMsg(T1M_BEGINQUICKOPEN,
                                                        (MPARAM)pQuick,
                                                        MPNULL);
                            // else none defined: do nothing
                        }
                    }
                } // end if (mp1 == NULL)
                else
                {
                    // subsequent postings
                    XFolder *pQuick = (XFolder*)mp1;

                    if (    (((ULONG)pQuick) != -1)
                        &&  (!G_fQuickOpenCancelled)
                       )
                    {
                        fWorkToDo = TRUE;
                        // subsequent postings: mp1 contains folder
                        if (pGlobalSettings->ShowStartupProgress)
                        {
                            CHAR szTemp[256];

                            _wpQueryFilename(pQuick, szTemp, TRUE);
                            WinSetDlgItemText(G_hwndQuickStatus, ID_SDDI_STATUS, szTemp);
                        }

                        // populate object in Worker thread
                        xthrPostWorkerMsg(WOM_QUICKOPEN,
                                         (MPARAM)pQuick,
                                         (MPARAM)fncbQuickOpen);
                        // the Worker thread will then post
                        // T1M_NEXTQUICKOPEN when it's done
                    }
                    else
                    {
                        DosSleep(500);
                        // -1 => no more folders: clean up
                        if (pGlobalSettings->ShowStartupProgress)
                        {
                            winhSaveWindowPos(G_hwndQuickStatus,
                                              HINI_USER,
                                              INIAPP_XWORKPLACE, INIKEY_WNDPOSSTARTUP);
                            WinDestroyWindow(G_hwndQuickStatus);
                        }
                    }
                }

                if (!fWorkToDo)
                    xthrPostFileMsg(FIM_STARTUPFOLDERDONE,
                                    (MPARAM)2,      // done with quick-open folders
                                    0);
            break; }

            case T1M_NEXTQUICKOPEN:
            {
                PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
                XFolder *pQuick = (XFolder*)mp1;

                if (pQuick)
                {
                    G_ulQuickOpenNow++;

                    if (pGlobalSettings->ShowStartupProgress)
                        WinSendMsg(WinWindowFromID(G_hwndQuickStatus, ID_SDDI_PROGRESSBAR),
                                   WM_UPDATEPROGRESSBAR,
                                   (MPARAM)(G_ulQuickOpenNow*100),
                                   (MPARAM)(G_ulQuickOpenMax*100));

                    // get next quick-open folder
                    pQuick = _xwpclsQueryQuickOpenFolder(_XFolder, pQuick);
                    krnPostThread1ObjectMsg(T1M_BEGINQUICKOPEN,
                                            (pQuick != NULL)
                                                    ? (MPARAM)pQuick
                                                    // last folder reached: cleanup signal
                                                    : (MPARAM)-1,
                                            MPNULL);
                    break;
                }

                // error, done, or cancelled:
                if (pGlobalSettings->ShowStartupProgress)
                    WinPostMsg(G_hwndQuickStatus, WM_CLOSE, MPNULL, MPNULL);
            break; }

            /*
             *@@ T1M_LIMITREACHED:
             *      this is posted by cmnAppendMi2List when too
             *      many menu items are in use, i.e. the user has
             *      opened a zillion folder content menus; we
             *      will display a warning dlg, which will also
             *      destroy the open menu.
             */

            case T1M_LIMITREACHED:
            {
                if (!fLimitMsgOpen)
                {
                    // avoid more than one dlg window
                    fLimitMsgOpen = TRUE;
                    cmnSetDlgHelpPanel(ID_XFH_LIMITREACHED);
                    WinDlgBox(HWND_DESKTOP,         // parent is desktop
                              HWND_DESKTOP,             // owner is desktop
                              (PFNWP)cmn_fnwpDlgWithHelp,    // dialog procedure, defd. at bottom
                              cmnQueryNLSModuleHandle(FALSE),  // from resource file
                              ID_XFD_LIMITREACHED,        // dialog resource id
                              (PVOID)NULL);             // no dialog parameters
                    fLimitMsgOpen = FALSE;
                }
            break; }

            /*
             *@@ T1M_EXCEPTIONCAUGHT:
             *      this is posted from the various XFolder threads
             *      when something trapped; it is assumed that
             *      mp1 is a PSZ to an error msg allocated with
             *      malloc(), and after displaying the error,
             *      (PSZ)mp1 is freed here. If mp2 != NULL, the WPS will
             *      be restarted (this is demanded by XSHutdown traps).
             *
             *@@changed V0.9.4 (2000-08-03) [umoeller]: fixed heap bug
             */

            case T1M_EXCEPTIONCAUGHT:
            {
                if (mp1)
                {
                    XSTRING strMsg;
                    xstrInit(&strMsg, 0);
                    xstrset(&strMsg, (PSZ)mp1);
                    if (mp2)
                    {
                        // restart WPS: Yes/No box
                        if (WinMessageBox(HWND_DESKTOP, HWND_DESKTOP,
                                          strMsg.psz,
                                          (PSZ)"XFolder: Exception caught",
                                          0,
                                          MB_YESNO | MB_ICONEXCLAMATION | MB_MOVEABLE)
                                 == MBID_YES)
                            // if yes: terminate the current process,
                            // which is PMSHELL.EXE. We cannot use DosExit()
                            // directly, because this might mess up the
                            // C runtime library.
                            exit(99);
                    }
                    else
                    {
                        // just report:
                        xstrcat(&strMsg,
                                "\n\nPlease post a bug report to "
                                "xworkplace-user@yahoogroups.com and attach the the file "
                                "XWPTRAP.LOG, which you will find in the root "
                                "directory of your boot drive. ", 0);
                        winhDebugBox(HWND_DESKTOP, "XFolder: Exception caught", strMsg.psz);
                    }

                    xstrClear(&strMsg);
                }
            break; }

            /*
             *@@ T1M_QUERYXFOLDERVERSION:
             *      this msg may be send to the XFolder object
             *      window from external programs to query the
             *      XFolder version number which is currently
             *      installed. We will return:
             *          mrc = MPFROM2SHORT(major, minor)
             *      which may be broken down by the external
             *      program using the SHORT1/2FROMMP macros.
             *      This is used by the XShutdown command-line
             *      interface (XSHUTDWN.EXE) to assert that
             *      XFolder is up and running, but can be used
             *      by other software too.
             */

            case T1M_QUERYXFOLDERVERSION:
            {
                ULONG   ulMajor = 0,
                        ulMinor = 0;
                sscanf(XFOLDER_VERSION, // e.g. 0.9.2, this is defined in dlgids.h
                        "%d.%d", &ulMajor, &ulMinor);   // V0.9.0

                mrc = MPFROM2SHORT(ulMajor, ulMinor);
            break; }

            /*
             *@@ T1M_EXTERNALSHUTDOWN:
             *      this msg may be posted to the XFolder object
             *      window from external programs to initiate
             *      the eXtended shutdown. mp1 is assumed to
             *      point to a block of shared memory containing
             *      a SHUTDOWNPARAMS structure.
             */

            case T1M_EXTERNALSHUTDOWN:
            {
                PSHUTDOWNPARAMS psdpShared = (PSHUTDOWNPARAMS)mp1;

                if ((ULONG)mp2 != 1234)
                {
                    // not special code:
                    // that's the send-msg from XSHUTDWN.EXE;
                    // copy the memory to local memory and
                    // return, otherwise XSHUTDWN.EXE hangs
                    APIRET arc = DosGetSharedMem(psdpShared, PAG_READ | PAG_WRITE);
                    if (arc == NO_ERROR)
                    {
                        // shared memory successfully accessed:
                        // the block has now two references (XSHUTDWN.EXE and
                        // PMSHELL.EXE);
                        // repost msg with that ptr to ourselves
                        WinPostMsg(hwndObject, T1M_EXTERNALSHUTDOWN, mp1, (MPARAM)1234);
                        // return TRUE to XSHUTDWN.EXE, which will then terminate
                        mrc = (MPARAM)TRUE;
                        // after XSHUTDWN.EXE terminates, the shared mem
                        // is not freed yet, because we still own it;
                        // we process that in the second msg (below)
                    }
                    else
                    {
                        winhDebugBox(0,
                                 "External XShutdown call",
                                 "Error calling DosGetSharedMem.");
                        mrc = (MPARAM)FALSE;
                    }
                }
                else
                {
                    // mp2 == 1234: second call
                    xsdInitiateShutdownExt(psdpShared);
                    // finally free shared mem
                    DosFreeMem(psdpShared);
                }
            break; }

            /*
             *@@ T1M_DESTROYARCHIVESTATUS:
             *      this gets posted from arcCheckIfBackupNeeded,
             *      which gets called from krnInitializeXWorkplace
             *      with the handle of this object wnd and this message ID.
             */

            case T1M_DESTROYARCHIVESTATUS:
                hwndArchiveStatus = (HWND)mp1;
                WinStartTimer(WinQueryAnchorBlock(hwndObject),
                              hwndObject,
                              1,
                              10);
            break;

            /*
             *@@ T1M_OPENOBJECTFROMHANDLE:
             *      this can be posted to the thread-1 object
             *      window from anywhere to have an object
             *      opened in its default view. As opposed to
             *      WinOpenObject, which opens the object on
             *      thread 13 (on my system), the thread-1
             *      object window will always open the object
             *      on thread 1, which leads to less problems.
             *
             *      See krn_T1M_OpenObjectFromHandle for the
             *      parameters.
             *
             *      Most notably, this is posted from the daemon
             *      to open "screen border objects" and global
             *      hotkey objects.
             */

            case T1M_OPENOBJECTFROMHANDLE:
                krn_T1M_OpenObjectFromHandle(hwndObject, mp1, mp2);
            break;

            /*
             *@@ T1M_OPENOBJECTFROMPTR:
             *      this can be posted or sent to the thread-1
             *      object window from anywhere to have an object
             *      opened in a specific view and have the HWND
             *      returned (on send only, of course).
             *
             *      This is useful if you must make sure that
             *      an object view is running on thread 1 and
             *      nowhere else. Used with the XCenter settings
             *      view, for example.
             *
             *      Parameters:
             *
             *      -- WPObject *mp1: SOM object pointer on which
             *         wpViewObject is to be invoked.
             *
             *      -- ULONG mp2: ulView to open (OPEN_DEFAULT,
             *         OPEN_SETTINGS, ...)
             *
             *      wpViewObject will be invoked with hwndCnr
             *      and param == NULL.
             *
             *      Returns the return value of wpViewObject,
             *      which is either a HWND or a HAPP.
             *
             *@@added V0.9.9 (2001-02-06) [umoeller]
             */

            case T1M_OPENOBJECTFROMPTR:
                mrc = (MPARAM)_wpViewObject((WPObject*)mp1,
                                            NULLHANDLE,     // hwndCnr
                                            (ULONG)mp2,
                                            0);             // param
            break;

            /*
             *@@ T1M_DAEMONREADY:
             *      posted by the XWorkplace daemon after it has
             *      successfully created its object window.
             *      This can happen in two situations:
             *
             *      -- during WPS startup, after krnInitializeXWorkplace
             *         has started the daemon;
             *      -- any time later, if the daemon has been restarted
             *         (shouldn't happen).
             *
             *      The thread-1 object window will then send XDM_HOOKINSTALL
             *      back to the daemon if the global settings have the
             *      hook enabled.
             */

            case T1M_DAEMONREADY:
                krn_T1M_DaemonReady();
            break;

// #ifdef __PAGEMAGE__
            /*
             *@@ T1M_PAGEMAGECLOSED:
             *      this gets posted by dmnKillPageMage when
             *      the user has closed the PageMage window.
             *      We then disable PageMage in the GLOBALSETTINGS.
             *
             *      Parameters:
             *      -- BOOL mp1: if TRUE, PageMage will be disabled
             *                   in the global settings.
             *
             *@@added V0.9.2 (2000-02-23) [umoeller]
             *@@changed V0.9.3 (2000-04-25) [umoeller]: added mp1 parameter
             */

            case T1M_PAGEMAGECLOSED:
            {
                if (mp1)
                {
                    GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(__FILE__, __LINE__, __FUNCTION__);
                    if (pGlobalSettings)
                    {
                        pGlobalSettings->fEnablePageMage = FALSE;
                        cmnUnlockGlobalSettings();
                        // _Pmpf(("  pGlobalSettings->fEnablePageMage = FALSE;"));
                    }
                }

                // update "Features" page, if open
                ntbUpdateVisiblePage(NULL, SP_SETUP_FEATURES);
            break; }
// #endif

// #ifdef __PAGEMAGE__
            /*
             *@@ T1M_PAGEMAGECONFIGDELAYED:
             *      posted by XWPScreen when any PageMage configuration
             *      has changed. We delay sending XDM_PAGEMAGECONFIG to
             *      the daemon for a little while in order not to overload
             *      the system, because PageMage needs to reconfigure itself
             *      every time.
             *
             *      Parameters:
             *      -- ULONG mp1: same flags as with XDM_PAGEMAGECONFIG
             *              mp1.
             *
             *@@added V0.9.3 (2000-04-09) [umoeller]
             */

            case T1M_PAGEMAGECONFIGDELAYED:
            {
                // add flags to global variable which will be
                // passed (and reset) when timer elapses
                G_PageMageConfigFlags |= (ULONG)mp1;
                // start timer 2
                WinStartTimer(WinQueryAnchorBlock(hwndObject),
                              hwndObject,
                              2,
                              500);     // half a second delay
            break; }
// #endif

            /*
             *@@ T1M_WELCOME:
             *      posted if XWorkplace has just been installed.
             *      This shows a little "Welcom" dialog on thread 1.
             *      This used to be a worker thread msg, but starting
             *      windows there wasn't such a good idea.
             *
             *@@added V0.9.7 (2001-01-07) [umoeller]
             */

            case T1M_WELCOME:
            /* {
                WPObject *pobj = wpshQueryObjectFromID(XFOLDER_MAINID,
                                                       NULL);
                if (pobj)
                    if (_wpViewObject(pobj, NULLHANDLE, OPEN_CONTENTS, 0))
                        cmnMessageBoxMsg(NULLHANDLE,
                                         121,       // xwp
                                         199,       // welcome
                                         MB_OK);
                */ // ### currently disabled... this hangs the system, dammit
            break;

            #ifdef __DEBUG__
            case XM_CRASH:          // posted by debugging context menu of XFldDesktop
                CRASH;
            break;
            #endif

            default:
                fCallDefault = TRUE;
        }
    }
    CATCH(excpt1) {} END_CATCH();

    if (fCallDefault)
        mrc = WinDefWindowProc(hwndObject, msg, mp1, mp2);

    return (mrc);
}

/*
 *@@ krnPostThread1ObjectMsg:
 *      post msg to thread-1 object window (krn_fnwpThread1Object).
 *      See include\shared\kernel.h for the supported T1M_*
 *      messages.
 *      This is used from all kinds of places and different threads.
 */

BOOL krnPostThread1ObjectMsg(ULONG msg, MPARAM mp1, MPARAM mp2)
{
    BOOL rc = FALSE;
    PCKERNELGLOBALS pKernelGlobals = krnQueryGlobals();

    if (pKernelGlobals->hwndThread1Object)
        rc = WinPostMsg(pKernelGlobals->hwndThread1Object, msg, mp1, mp2);

    return (rc);
}

/*
 *@@ krnSendThread1ObjectMsg:
 *      send msg to thread-1 object window (krn_fnwpThread1Object).
 *      See include\shared\kernel.h for the supported T1M_*
 *      messages.
 *      Note that, as usual, sending a message from another
 *      thread will block that thread until we return.
 */

MRESULT krnSendThread1ObjectMsg(ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = MPNULL;
    PCKERNELGLOBALS pKernelGlobals = krnQueryGlobals();

    if (pKernelGlobals->hwndThread1Object)
        mrc = WinSendMsg(pKernelGlobals->hwndThread1Object, msg, mp1, mp2);

    return (mrc);
}

/*
 *@@ fnwpStartupDlg:
 *      dlg proc for the Startup status window, which
 *      runs on the main PM thread (krn_fnwpThread1Object).
 */

MRESULT EXPENTRY fnwpStartupDlg(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc;

    switch (msg)
    {
        case WM_INITDLG:
        {
            // WinSetWindowULong(hwnd, QWL_USER, (ULONG)mp2);
                // we don't need this here, it's done by krn_fnwpThread1Object
            ctlProgressBarFromStatic(WinWindowFromID(hwnd, ID_SDDI_PROGRESSBAR),
                                     PBA_ALIGNCENTER | PBA_BUTTONSTYLE);
            mrc = WinDefDlgProc(hwnd, msg, mp1, mp2);
        break; }

        case WM_COMMAND:
            switch (SHORT1FROMMP(mp1))
            {
                case DID_CANCEL:
                {
                    PPROCESSCONTENTINFO pPCI;
                    pPCI = (PPROCESSCONTENTINFO)WinQueryWindowULong(hwnd, QWL_USER);
                    if (pPCI)
                        pPCI->fCancelled = TRUE;
                    mrc = WinDefDlgProc(hwnd, msg, mp1, mp2);
                break; }

                default:
                    mrc = WinDefDlgProc(hwnd, msg, mp1, mp2);
            }
        break;

        case WM_SYSCOMMAND:
        {
            switch (SHORT1FROMMP(mp1))
            {
                case SC_CLOSE:
                case SC_HIDE:
                {
                    GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(__FILE__, __LINE__, __FUNCTION__);
                    if (pGlobalSettings)
                    {
                        pGlobalSettings->ShowStartupProgress = 0;
                        cmnUnlockGlobalSettings();
                        cmnStoreGlobalSettings();
                    }
                    mrc = WinDefDlgProc(hwnd, msg, mp1, mp2);
                break; }

                default:
                    mrc = WinDefDlgProc(hwnd, msg, mp1, mp2);
            }
        break; }

        default:
            mrc = WinDefDlgProc(hwnd, msg, mp1, mp2);
    }

    return (mrc);
}

/*
 *@@ fncbStartup:
 *      callback function for XFolder::xwpProcessOrderedContent
 *      during Startup folder processing;
 *      updates the status window and opens objects.
 *      This is guaranteed to run on thread 1.
 */

MRESULT EXPENTRY fncbStartup(HWND hwndStatus, ULONG ulObject, MPARAM mpNow, MPARAM mpMax)
{
    CHAR szStarting2[200], szTemp[200];
    HWND rc = 0;
    #ifdef DEBUG_STARTUP
        _Pmpf(("  -- fncbStartup called"));
    #endif

    if (ulObject)
    {
        PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

        if (pGlobalSettings->ShowStartupProgress)
        {
            // update status text ("Starting xxx")
            strcpy(szTemp, _wpQueryTitle((WPObject*)ulObject));
            strhBeautifyTitle(szTemp);
            sprintf(szStarting2,
                    (cmnQueryNLSStrings())->pszStarting,
                    szTemp);
            WinSetDlgItemText(hwndStatus, ID_SDDI_STATUS, szStarting2);
        }

        // open object
        rc = _wpViewObject((WPObject*)ulObject, 0, OPEN_DEFAULT, 0);

        // update status bar
        if (pGlobalSettings->ShowStartupProgress)
            WinSendMsg(WinWindowFromID(hwndStatus, ID_SDDI_PROGRESSBAR),
                       WM_UPDATEPROGRESSBAR,
                       mpNow, mpMax);
    }
    else
    {
        // NULL: last object, close window
        winhSaveWindowPos(hwndStatus, HINI_USER, INIAPP_XWORKPLACE, INIKEY_WNDPOSSTARTUP);
        WinDestroyWindow(hwndStatus);
        // and notify File thread to go on
        xthrPostFileMsg(FIM_STARTUPFOLDERDONE,
                        (MPARAM)1,      // done with startup
                        MPNULL);
    }
    return (MRESULT)rc;
}

/*
 *@@ fnwpQuickOpenDlg:
 *      dlg proc for the QuickOpen status window.
 */

MRESULT EXPENTRY fnwpQuickOpenDlg(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc;

    switch (msg)
    {
        case WM_INITDLG:
        {
            // WinSetWindowULong(hwnd, QWL_USER, (ULONG)mp2);
            // we don't need this here, it's done by krn_fnwpThread1Object
            ctlProgressBarFromStatic(WinWindowFromID(hwnd, ID_SDDI_PROGRESSBAR),
                                     PBA_ALIGNCENTER | PBA_BUTTONSTYLE);
            mrc = WinDefDlgProc(hwnd, msg, mp1, mp2);
        break; }

        case WM_COMMAND:
            switch (SHORT1FROMMP(mp1))
            {
                case DID_CANCEL:
                {
                    G_fQuickOpenCancelled = TRUE;
                    // this will cause the callback below to
                    // return FALSE
                break; }

                default:
                    mrc = WinDefDlgProc(hwnd, msg, mp1, mp2);
            }
        break;

        case WM_SYSCOMMAND:
        {
            switch (SHORT1FROMMP(mp1))
            {
                case SC_HIDE:
                {
                    GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(__FILE__, __LINE__, __FUNCTION__);
                    if (pGlobalSettings)
                    {
                        pGlobalSettings->ShowStartupProgress = 0;
                        cmnUnlockGlobalSettings();
                        cmnStoreGlobalSettings();
                    }
                    mrc = WinDefDlgProc(hwnd, msg, mp1, mp2);
                break; }

                default:
                    mrc = WinDefDlgProc(hwnd, msg, mp1, mp2);
            }
        break; }

        case WM_DESTROY:
        {
            mrc = WinDefDlgProc(hwnd, msg, mp1, mp2);
        break; }

        default:
            mrc = WinDefDlgProc(hwnd, msg, mp1, mp2);
    }

    return (mrc);
}

/*
 *@@ fncbQuickOpen:
 *      callback func called by Worker thread for
 *      each object whose icon is queried.
 *      If this returns FALSE, processing is
 *      terminated.
 */

MRESULT EXPENTRY fncbQuickOpen(HWND hwndFolder,
                               ULONG ulObject,     //
                               MPARAM mpNow,       // current object count
                               MPARAM mpMax)       // maximum object count
{
    WinSendMsg(WinWindowFromID(G_hwndQuickStatus, ID_SDDI_PROGRESSBAR),
               WM_UPDATEPROGRESSBAR,
               (MPARAM)(
                           G_ulQuickOpenNow*100 +
                               ( (100 * (ULONG)mpNow) / (ULONG)mpMax )
                       ),
               (MPARAM)(G_ulQuickOpenMax*100));

    // if "Cancel" has been pressed, return FALSE
    return ((MPARAM)(!G_fQuickOpenCancelled));
}

/* ******************************************************************
 *
 *   XWorkplace initialization
 *
 ********************************************************************/

/*
 *@@ G_apszXFolderKeys:
 *      XFolder INI keys to be copied when upgrading
 *      from XFolder to XWorkplace.
 */

const char **G_appszXFolderKeys[]
        = {
                &INIKEY_GLOBALSETTINGS  , // "GlobalSettings"
                &INIKEY_ACCELERATORS    , // "Accelerators"
                &INIKEY_FAVORITEFOLDERS , // "FavoriteFolders"
                &INIKEY_QUICKOPENFOLDERS, // "QuickOpenFolders"
                &INIKEY_WNDPOSSTARTUP   , // "WndPosStartup"
                &INIKEY_WNDPOSNAMECLASH , // "WndPosNameClash"
                &INIKEY_NAMECLASHFOCUS  , // "NameClashLastFocus"
                &INIKEY_STATUSBARFONT   , // "SB_Font"
                &INIKEY_SBTEXTNONESEL   , // "SB_NoneSelected"
                &INIKEY_SBTEXT_WPOBJECT , // "SB_WPObject"
                &INIKEY_SBTEXT_WPPROGRAM, // "SB_WPProgram"
                &INIKEY_SBTEXT_WPFILESYSTEM, // "SB_WPDataFile"
                &INIKEY_SBTEXT_WPURL       , // "SB_WPUrl"
                &INIKEY_SBTEXT_WPDISK   , // "SB_WPDisk"
                &INIKEY_SBTEXT_WPFOLDER , // "SB_WPFolder"
                &INIKEY_SBTEXTMULTISEL  , // "SB_MultiSelected"
                &INIKEY_SB_LASTCLASS    , // "SB_LastClass"
                &INIKEY_DLGFONT         , // "DialogFont"
                &INIKEY_BOOTMGR         , // "RebootTo"
                &INIKEY_AUTOCLOSE        // "AutoClose"
          };

/*
 *@@ WaitForApp:
 *      waits for the specified application to terminate.
 *
 *      Returns:
 *
 *      -1: Error starting app (happ was zero, msg box displayed).
 *
 *      Other: Return code of the application.
 *
 *@@added V0.9.9 (2001-03-07) [umoeller]
 */

ULONG WaitForApp(const char *pcszTitle,
                 HAPP happ)
{
    ULONG   ulrc = -1;

    if (!happ)
    {
        // error:
        PSZ apsz[] = {(PSZ)pcszTitle};
        cmnMessageBoxMsgExt(NULLHANDLE,
                            121,       // xwp
                            apsz,
                            1,
                            206,       // cannot start %1
                            MB_OK);
    }
    else
    {
        // app started:
        // enter a modal message loop until we get the
        // WM_APPTERMINATENOTIFY for happ. Then we
        // know the app is done.
        HAB     hab = WinQueryAnchorBlock(G_KernelGlobals.hwndThread1Object);
        QMSG    qmsg;
        ULONG   ulXFixReturnCode = 0;
        while (WinGetMsg(hab, &qmsg, NULLHANDLE, 0, 0))
        {
            if (    (qmsg.msg == WM_APPTERMINATENOTIFY)
                 && (qmsg.hwnd == G_KernelGlobals.hwndThread1Object)
                 && (qmsg.mp1 == (MPARAM)happ)
               )
            {
                // xfix has terminated:
                // get xfix return code from mp2... this is:
                // -- 0: everything's OK, continue.
                // -- 1: handle section was rewritten, restart WPS
                //       now.
                ulrc = (ULONG)qmsg.mp2;
                // do not dispatch this
                break;
            }

            WinDispatchMsg(hab, &qmsg);
        }
    }

    return (ulrc);
}

/*
 *@@ krnShowStartupDlgs:
 *      this gets called from krnInitializeXWorkplace
 *      to show dialogs while the WPS is starting up.
 *
 *      If XWorkplace was just installed, we'll show
 *      an introductory page and offer to convert
 *      XFolder settings, if found.
 *
 *      If XWorkplace has just been installed, we show
 *      an introductory message that "Shift" will show
 *      the "panic" dialog.
 *
 *      If "Shift" is currently pressed, we'll show the
 *      "Panic" dialog.
 *
 *@@added V0.9.1 (99-12-18) [umoeller]
 */

VOID krnShowStartupDlgs(VOID)
{
    ULONG   cbData = 0;
    BOOL    fRepeat = FALSE;

    // check if XWorkplace was just installed
    if (PrfQueryProfileInt(HINI_USER,
                           (PSZ)INIAPP_XWORKPLACE,
                           (PSZ)INIKEY_JUSTINSTALLED,
                           0x123) != 0x123)
    {
        // yes: explain the "Panic" feature
        cmnMessageBoxMsg(HWND_DESKTOP,
                         121,       // "XWorkplace"
                         159,       // "press shift for panic"
                         MB_OK);
    }

    /*
     * convert XFolder settings
     *
     */

    if (PrfQueryProfileSize(HINI_USER,
                            (PSZ)INIAPP_XWORKPLACE,
                            (PSZ)INIKEY_GLOBALSETTINGS,
                            &cbData)
            == FALSE)
    {
        // XWorkplace keys do _not_ exist:
        // check if we have old XFolder settings
        if (PrfQueryProfileSize(HINI_USER,
                                (PSZ)INIAPP_OLDXFOLDER,
                                (PSZ)INIKEY_GLOBALSETTINGS,
                                &cbData))
        {
            if (cmnMessageBoxMsg(HWND_DESKTOP,
                                 121,       // "XWorkplace"
                                 160,       // "convert?"
                                 MB_YESNO)
                    == MBID_YES)
            {
                // yes, convert:
                // copy keys from "XFolder" to "XWorkplace"
                ULONG   ul;
                for (ul = 0;
                     ul < sizeof(G_appszXFolderKeys) / sizeof(G_appszXFolderKeys[0]);
                     ul++)
                {
                    prfhCopyKey(HINI_USER,
                                INIAPP_OLDXFOLDER,      // source
                                *G_appszXFolderKeys[ul],
                                HINI_USER,
                                INIAPP_XWORKPLACE);
                }

                // reload
                cmnLoadGlobalSettings(FALSE);
            }
        }
    }

    /*
     * "Panic" dlg
     *
     */

    while (    (doshQueryShiftState())
            || (fRepeat)       // set to TRUE after xfix V0.9.7 (2001-01-24) [umoeller]
          )
    {
        ULONG   ulrc = 0;
        PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
        // shift pressed: show "panic" dialog
        HWND    hwndPanic = WinLoadDlg(HWND_DESKTOP, HWND_DESKTOP,
                                       WinDefDlgProc,
                                       cmnQueryNLSModuleHandle(FALSE),
                                       ID_XFD_STARTUPPANIC,
                                       NULL);

        fRepeat = FALSE;

        winhCenterWindow(hwndPanic);

        // disable items which are irrelevant
        WinEnableControl(hwndPanic, ID_XFDI_PANIC_SKIPBOOTLOGO,
                         pGlobalSettings->BootLogo);
        WinEnableControl(hwndPanic, ID_XFDI_PANIC_NOARCHIVING,
                         pGlobalSettings->fReplaceArchiving);
        WinEnableControl(hwndPanic, ID_XFDI_PANIC_DISABLEREPLICONS,
                         pGlobalSettings->fReplaceIcons);
        WinEnableControl(hwndPanic, ID_XFDI_PANIC_DISABLEPAGEMAGE,
                         pGlobalSettings->fEnablePageMage);
        WinEnableControl(hwndPanic, ID_XFDI_PANIC_DISABLEMULTIMEDIA,
                         (xmmQueryStatus() == MMSTAT_WORKING));

        ulrc = WinProcessDlg(hwndPanic);

        switch (ulrc)
        {
            case DID_OK:        // continue
            {
                if (winhIsDlgItemChecked(hwndPanic, ID_XFDI_PANIC_SKIPBOOTLOGO))
                    G_KernelGlobals.ulPanicFlags |= SUF_SKIPBOOTLOGO;
                if (winhIsDlgItemChecked(hwndPanic, ID_XFDI_PANIC_SKIPXFLDSTARTUP))
                    G_KernelGlobals.ulPanicFlags |= SUF_SKIPXFLDSTARTUP;
                if (winhIsDlgItemChecked(hwndPanic, ID_XFDI_PANIC_SKIPQUICKOPEN))
                    G_KernelGlobals.ulPanicFlags |= SUF_SKIPQUICKOPEN;
                if (winhIsDlgItemChecked(hwndPanic, ID_XFDI_PANIC_NOARCHIVING))
                {
                    PWPSARCOSETTINGS pArcSettings = arcQuerySettings();
                    // disable "check archives" flag
                    pArcSettings->ulArcFlags &= ~ARCF_ENABLED;
                }
                if (winhIsDlgItemChecked(hwndPanic, ID_XFDI_PANIC_DISABLEREPLICONS))
                {
                    GLOBALSETTINGS *pGlobalSettings2 = cmnLockGlobalSettings(__FILE__, __LINE__, __FUNCTION__);
                    if (pGlobalSettings2)
                    {
                        pGlobalSettings2->fReplaceIcons = FALSE;
                        cmnUnlockGlobalSettings();
                        cmnStoreGlobalSettings();
                    }
                }
                if (winhIsDlgItemChecked(hwndPanic, ID_XFDI_PANIC_DISABLEPAGEMAGE))
                {
                    GLOBALSETTINGS *pGlobalSettings2 = cmnLockGlobalSettings(__FILE__, __LINE__, __FUNCTION__);
                    if (pGlobalSettings2)
                    {
                        pGlobalSettings2->fEnablePageMage = FALSE;  // ###
                        cmnUnlockGlobalSettings();
                        cmnStoreGlobalSettings();
                    }
                }
                if (winhIsDlgItemChecked(hwndPanic, ID_XFDI_PANIC_DISABLEMULTIMEDIA))
                {
                    xmmDisable();
                }
                if (winhIsDlgItemChecked(hwndPanic, ID_XFDI_PANIC_DISABLEFEATURES))
                {
                    cmnLoadGlobalSettings(TRUE);        // reset defaults
                    cmnStoreGlobalSettings();
                }
                if (winhIsDlgItemChecked(hwndPanic, ID_XFDI_PANIC_REMOVEHOTKEYS))
                    PrfWriteProfileData(HINI_USER,
                                        INIAPP_XWPHOOK,
                                        INIKEY_HOOK_HOTKEYS,
                                        0, 0);      // delete INI key
            break; }

            case DID_UNDO:      // run xfix:
            {
                CHAR        szXfix[CCHMAXPATH];
                PROGDETAILS pd = {0};
                HAPP        happXFix;
                cmnQueryXWPBasePath(szXfix);
                strcat(szXfix, "\\bin\\xfix.exe");

                pd.Length = sizeof(pd);
                pd.progt.progc = PROG_PM;
                pd.progt.fbVisible = SHE_VISIBLE;
                pd.pszExecutable = szXfix;
                happXFix = winhStartApp(G_KernelGlobals.hwndThread1Object,
                                        &pd);

                if (WaitForApp(szXfix,
                               happXFix)
                    == 1)
                {
                    // handle section changed:
                    cmnMessageBoxMsg(NULLHANDLE,
                                     121,       // xwp
                                     205,       // restart wps now.
                                     MB_OK);
                    DosExit(EXIT_PROCESS, 0);
                }

                fRepeat = TRUE;
            break; }

            case DID_APPLY:         // run cmd.exe
            {
                HAPP happCmd;
                PROGDETAILS pd = {0};
                pd.Length = sizeof(pd);
                pd.progt.progc = PROG_WINDOWABLEVIO;
                pd.progt.fbVisible = SHE_VISIBLE;
                pd.pszExecutable = "*";        // use OS2_SHELL
                happCmd = winhStartApp(G_KernelGlobals.hwndThread1Object,
                                       &pd);
                WaitForApp(getenv("OS2_SHELL"),
                           happCmd);
            break; }

            case DID_CANCEL:        // shutdown
                // "Shutdown" pressed:
                WinShutdownSystem(WinQueryAnchorBlock(HWND_DESKTOP),
                                  WinQueryWindowULong(HWND_DESKTOP, QWL_HMQ));
                while (TRUE)
                    DosSleep(1000);
        }

        WinDestroyWindow(hwndPanic);
    }

    if (getenv("XWP_NO_SUBCLASSING"))
    {
        // V0.9.3 (2000-04-26) [umoeller]
        GLOBALSETTINGS *pGlobalSettings2 = cmnLockGlobalSettings(__FILE__, __LINE__, __FUNCTION__);
        if (pGlobalSettings2)
        {
            pGlobalSettings2->fNoSubclassing = TRUE;
            cmnUnlockGlobalSettings();
            // _Pmpf(("krnShowStartupDlgs: disabled subclassing"));
        }
    }
}

/*
 *@@ krnReplaceRefreshEnabled:
 *      returns TRUE if "replace folder refresh" has been
 *      enabled.
 *
 *      This setting is not in the GLOBALSETTINGS because
 *      it has a separate entry in OS2.INI.
 *
 *      Note that this returns the current value of the
 *      setting. If the user has just changed the setting
 *      without restarting the WPS, this does not mean
 *      that "replace refresh" is actually currently working.
 *      Use KERNELGLOBALS.fAutoRefreshReplaced instead.
 *
 *@@added V0.9.9 (2001-01-31) [umoeller]
 */

BOOL krnReplaceRefreshEnabled(VOID)
{
    BOOL        fReplaceFolderRefresh = FALSE;
    ULONG       cb = sizeof(fReplaceFolderRefresh);

    PrfQueryProfileData(HINI_USER,
                        (PSZ)INIAPP_XWORKPLACE,
                        (PSZ)INIAPP_REPLACEFOLDERREFRESH,
                        &fReplaceFolderRefresh,
                        &cb);

    return (fReplaceFolderRefresh);
}

/*
 *@@ krnEnableReplaceRefresh:
 *      enables or disables "replace auto-refresh folders".
 *      The setting does not take effect until after a
 *      WPS restart.
 *
 *@@added V0.9.9 (2001-01-31) [umoeller]
 */

VOID krnEnableReplaceRefresh(BOOL fEnable)
{
    PrfWriteProfileData(HINI_USER,
                        (PSZ)INIAPP_XWORKPLACE,
                        (PSZ)INIAPP_REPLACEFOLDERREFRESH,
                        &fEnable,
                        sizeof(fEnable));
}

/*
 *@@ krnReplaceWheelWatcher:
 *      blocks out the standard WPS "WheelWatcher" thread
 *      (which usually does the DosFindNotify* stuff)
 *      and creates a new thread in XWP instead.
 *
 *      Gets called _after_ the panic dialog, but _before_
 *      the additional XWP threads are started.
 *
 *@@added V0.9.9 (2001-01-31) [umoeller]
 */

VOID krnReplaceWheelWatcher(FILE *DumpFile)
{
    APIRET      arc = NO_ERROR;
    HQUEUE      hqWheelWatcher = NULLHANDLE;

    if (DumpFile)
    {
        PQPROCSTAT16    pInfo = prc16GetInfo(&arc);

        fprintf(DumpFile,
                "\nEntering " __FUNCTION__":\n");

        if (pInfo && arc == NO_ERROR)
        {
            // find WPS entry in process info
            PQPROCESS16 pProcess = prc16FindProcessFromPID(pInfo,
                                                           G_KernelGlobals.pidWPS);
            if (pProcess)
            {
                // we now have the process info for the second PMSHELL.EXE...
                ULONG       ul;
                PQTHREAD16  pThread;

                fprintf(DumpFile,
                        "  Running WPS threads at this point:\n");

                for (ul = 0, pThread = (PQTHREAD16)PTR(pProcess->ulThreadList, 0);
                     ul < pProcess->usThreads;
                     ul++, pThread++ )
                {
                    CHAR    sz[100];
                    HENUM   henum;
                    HWND    hwndThis;
                    fprintf(DumpFile,
                            "    Thread %02d has priority 0x%04lX\n",
                            pThread->usTID,
                            pThread->ulPriority);

                    henum = WinBeginEnumWindows(HWND_OBJECT);
                    while (hwndThis = WinGetNextWindow(henum))
                    {
                        PID pid;
                        TID tid;
                        if (WinQueryWindowProcess(hwndThis, &pid, &tid))
                            if (    (pid == G_KernelGlobals.pidWPS)
                                 && (tid == pThread->usTID)
                               )
                            {
                                CHAR szClass[100];
                                WinQueryClassName(hwndThis, sizeof(szClass), szClass);
                                fprintf(DumpFile,
                                        "        object wnd 0x%lX (%s)\n",
                                        hwndThis,
                                        szClass);
                            }
                    }
                    WinEndEnumWindows(henum);
                } // end for (ul = 0, pThread =...
            }

            free(pInfo);
        }
    }

    // now lock out the WheelWatcher thread...
    // that thread is started AFTER us, and it attempts to
    // create a CP queue of the below name. If it cannot do
    // that, it will simply exit. So... by creating a queue
    // of the same name, the WheelWatcher will get an error
    // later, and exit.
    arc = DosCreateQueue(&hqWheelWatcher,
                         QUE_FIFO,
                         "\\QUEUES\\FILESYS\\NOTIFY");
    if (DumpFile)
        fprintf(DumpFile,
                "  Created HQUEUE 0x%lX (DosCreateQueue returned %d)\n",
                hqWheelWatcher,
                arc);

    if (arc == NO_ERROR)
    {
        // we got the queue: then our assumption was valid
        // that we are indeed running _before_ WheelWatcher here...
        // create our own thread instead
        thrCreate(&G_KernelGlobals.tiSentinel,
                  refr_fntSentinel,
                  NULL,
                  THRF_WAIT,
                  0);           // no data here

        if (DumpFile)
            fprintf(DumpFile,
                    "  Started XWP Sentinel thread, TID: %d\n",
                    G_KernelGlobals.tiSentinel.tid);

        G_KernelGlobals.fAutoRefreshReplaced = TRUE;
    }
}

/*
 *@@ krnInitializeXWorkplace:
 *      this gets called from M_XFldObject::wpclsInitData
 *      when the WPS is initializing. See remarks there.
 *
 *      From what I've checked, this function always gets
 *      called on thread 1 of PMSHELL.EXE.
 *
 *      As said there, at this point we own the computer
 *      all alone. Roughly speaking, the WPS has the following
 *      status:
 *
 *      --  The SOM kernel appears to be fully initialized.
 *
 *      --  The WPObject class object (_WPObject) is _being_
 *          initialized. The SOM kernel initializes a class
 *          object when the first instance (object) of a
 *          class is being created. I have no idea which
 *          object gets initialized first by the WPS, but
 *          maybe it's even the Desktop.
 *          Or maybe the class object is initialized
 *          explicitly by PMSHELL.EXE, while it is processing
 *          the WPS class list from OS2.INI, who knows.
 *
 *      --  Apparently, the other WPS threads are not yet
 *          running -- or if they are, they won't interfere
 *          with anything we're doing here. So we can
 *          suspend the boot process for as long as we want
 *          to.
 *
 *      So what we're doing here is the following (this is a
 *      bit complex):
 *
 *      a) Initialize XWorkplace's globals: the GLOBALSETTINGS,
 *         the KERNELGLOBALS, attempt to load SOUND.DLL, and such.
 *
 *      b) If the "Shift" key is pressed, show the "Panic" dialog
 *         (new with V0.9.0). In that case, we pause the WPS
 *         bootup simply by not returning from this function
 *         until the dialog is dismissed.
 *
 *      c) Create the Thread-1 object window (krn_fnwpThread1Object).
 *
 *      d) Call xthrStartThreads to have the additional XWorkplace
 *         threads started. The Speedy thread will then display the
 *         boot logo, if allowed.
 *
 *      e) Start the XWorkplace daemon (XWPDAEMN.EXE, xwpdaemn.c),
 *         which register the XWorkplace hook (XWPHOOK.DLL, xwphook.c,
 *         all new with V0.9.0). See xwpdaemon.c for details.
 *
 *      f) Finally, we call arcCheckIfBackupNeeded (archives.c)
 *         to enable WPS archiving, if necessary. The WPS will
 *         then archive the Desktop, after we return from this
 *         function (also new with V0.9.0).
 *
 *@@changed V0.9.0 [umoeller]: renamed from xthrInitializeThreads
 *@@changed V0.9.0 [umoeller]: added dialog for shift key during WPS startup
 *@@changed V0.9.0 [umoeller]: added XWorkplace daemon/hook
 *@@changed V0.9.0 [umoeller]: added WPS archiving
 *@@changed V0.9.1 (99-12-19) [umoeller]: added NumLock at startup
 *@@changed V0.9.3 (2000-04-27) [umoeller]: added PM error windows
 *@@changed V0.9.5 (2000-08-10) [umoeller]: added XWPSHELL interface
 *@@changed V0.9.7 (2000-12-13) [umoeller]: moved config.sys path composition here
 *@@changed V0.9.7 (2000-12-17) [umoeller]: got crashes if archiving displayed a msg box; moved archiving up
 */

VOID krnInitializeXWorkplace(VOID)
{
    FILE        *DumpFile = NULL;

    static BOOL fInitialized = FALSE;

    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    // check if we're called for the first time,
    // because we better initialize this only once
    if (!fInitialized)
    {
        // moved this here from xthrStartThreads
        PTIB        ptib;
        PPIB        ppib;

        CHAR        szDumpFile[CCHMAXPATH];

        fInitialized = TRUE;

        if (TRUE)           // ###
        {
            sprintf(szDumpFile, "%c:\\xwpstart.log", doshQueryBootDrive());
            DumpFile = fopen(szDumpFile, "a");

            if (DumpFile)
            {
                DATETIME dt;
                DosGetDateTime(&dt);
                fprintf(DumpFile,
                        "\nXWorkplace startup log -- Date: %04d-%02d-%02d, Time: %02d:%02d:%02d\n",
                        dt.year, dt.month, dt.day,
                        dt.hours, dt.minutes, dt.seconds);
                fprintf(DumpFile,
                        "----------------------------------------------------------\n");

                fprintf(DumpFile,
                        "\nEntering " __FUNCTION__":\n");
            }
        }

        // zero KERNELGLOBALS
        memset(&G_KernelGlobals, 0, sizeof(KERNELGLOBALS));

        // moved this here from xthrStartThreads V0.9.9 (2001-01-31) [umoeller]
        if (DosGetInfoBlocks(&ptib, &ppib) == NO_ERROR)
        {
            if (ppib)
                G_KernelGlobals.pidWPS = ppib->pib_ulpid;
            if (ptib && ptib->tib_ptib2)
                G_KernelGlobals.tidWorkplaceThread = ptib->tib_ptib2->tib2_ultid;

            if (DumpFile)
                fprintf(DumpFile,
                        "\n" __FUNCTION__ ": PID 0x%lX, TID 0x%lX\n",
                        G_KernelGlobals.pidWPS,
                        G_KernelGlobals.tidWorkplaceThread);
        }

        #ifdef __XWPMEMDEBUG__
            // set global memory error callback
            G_pMemdLogFunc = krnMemoryError;
        #endif

        // store WPS startup time
        DosGetDateTime(&G_KernelGlobals.StartupDateTime);

        // get PM system error windows V0.9.3 (2000-04-28) [umoeller]
        winhFindPMErrorWindows(&G_KernelGlobals.hwndHardError,
                               &G_KernelGlobals.hwndSysError);

        // _Pmpf(("G_KernalGlobals.hwndHardError: 0x%lX", G_KernelGlobals.hwndHardError));
        // _Pmpf(("G_KernalGlobals.hwndSysError: 0x%lX", G_KernelGlobals.hwndSysError));

        // initialize awake-objects list (which holds
        // plain WPObject* pointers)
        G_KernelGlobals.pllAwakeObjects = lstCreate(FALSE);   // no auto-free items

        // register exception hooks for /helpers/except.c
        excRegisterHooks(krnExceptOpenLogFile,
                         krnExceptExplainXFolder,
                         krnExceptError,
                         !pGlobalSettings->fNoExcptBeeps);

        // create thread-1 object window
        WinRegisterClass(WinQueryAnchorBlock(HWND_DESKTOP),
                         (PSZ)WNDCLASS_THREAD1OBJECT,    // class name
                         (PFNWP)krn_fnwpThread1Object,    // Window procedure
                         0,                  // class style
                         0);                 // extra window words
        G_KernelGlobals.hwndThread1Object
            = winhCreateObjectWindow(WNDCLASS_THREAD1OBJECT, // class name
                                     NULL);        // create params

        if (DumpFile)
            fprintf(DumpFile,
                    "XWorkplace thread-1 object window created, HWND 0x%lX\n",
                    G_KernelGlobals.hwndThread1Object);

        // if shift is pressed, show "Panic" dialog
        // V0.9.7 (2001-01-24) [umoeller]: moved this behind creation
        // of thread-1 window... we need this for starting xfix from
        // the "panic" dlg
        krnShowStartupDlgs();

        // check if "replace folder refresh" is enabled...
        if (krnReplaceRefreshEnabled())
        {
            // yes: kick out WPS wheel watcher thread,
            // start our own one instead
            krnReplaceWheelWatcher(DumpFile);
        }
    }

    // set up config.sys path
    sprintf(G_KernelGlobals.szConfigSys,
            "%c:\\config.sys",
            doshQueryBootDrive());

    /*
     *  enable NumLock at startup
     *      V0.9.1 (99-12-19) [umoeller]
     */

    if (pGlobalSettings->fNumLockStartup)
        winhSetNumLock(TRUE);

    // initialize multimedia V0.9.3 (2000-04-25) [umoeller]
    xmmInit(DumpFile);
            // moved this down V0.9.9 (2001-01-31) [umoeller]

    /*
     *  initialize threads
     *
     */

    xthrStartThreads(DumpFile);

    /*
     *  check WPS archiving (V0.9.0)
     *      moved this up V0.9.7 (2000-12-17) [umoeller];
     *      we get crashes if a msg box is displayed otherwise
     */

    if (pGlobalSettings->fReplaceArchiving)
        // check whether we need a WPS backup (archives.c)
        arcCheckIfBackupNeeded(G_KernelGlobals.hwndThread1Object,
                               T1M_DESTROYARCHIVESTATUS);

    /*
     *  start XWorkplace daemon (XWPDAEMN.EXE)
     *
     */

    {
        // check for the DAEMONSHARED structure, which
        // is used for communication between the daemon
        // and XFLDR.DLL (see src/Daemon/xwpdaemn.c).
        // We take advantage of the fact that OS/2 keeps
        // reference of the processes which allocate or
        // request access to a block of shared memory.
        // The DAEMONSHARED struct is allocated here
        // (just below) and requested by the daemon.
        //
        // -- If requesting the shared memory works at this point,
        //    this means that the daemon is still running!
        //    This happens after a WPS restart. We'll then
        //    skip the rest.
        //
        // -- If requesting the shared memory fails, this means
        //    that the daemon is _not_ running (the WPS is started
        //    for the first time). We then allocate the shared
        //    memory and start the daemon, which in turn requests
        //    this shared memory block. Note that this also happens
        //    if the daemon stopped for some reason (crash, kill)
        //    and the user then restarts the WPS.

        PDAEMONSHARED pDaemonShared = 0;
        APIRET arc = DosGetNamedSharedMem((PVOID*)&pDaemonShared,
                                          SHMEM_DAEMON,
                                          PAG_READ | PAG_WRITE);

        if (DumpFile)
            fprintf(DumpFile,
                    "\nAttempted to access " SHMEM_DAEMON ", DosGetNamedSharedMem returned %d\n",
                    arc);

        if (arc != NO_ERROR)
        {
            BOOL    fDaemonStarted = FALSE;

            // shared mem does not exist:
            // --> daemon not running; probably first WPS
            // startup, so we allocate the shared mem now and
            // start the XWorkplace daemon

            if (DumpFile)
                fprintf(DumpFile, "--> XWPDAEMN not running, starting now.\n");

            arc = DosAllocSharedMem((PVOID*)&pDaemonShared,
                                    SHMEM_DAEMON,
                                    sizeof(DAEMONSHARED), // rounded up to 4KB
                                    PAG_COMMIT | PAG_READ | PAG_WRITE);

            if (DumpFile)
                fprintf(DumpFile, "  DosAllocSharedMem returned %d\n", arc);

            if (arc == NO_ERROR)
            {
                CHAR    szDir[CCHMAXPATH],
                        szExe[CCHMAXPATH];

                // shared mem successfully allocated:
                memset(pDaemonShared, 0, sizeof(DAEMONSHARED));
                // store the thread-1 object window, which
                // gets messages from the daemon
                pDaemonShared->hwndThread1Object = G_KernelGlobals.hwndThread1Object;
                pDaemonShared->ulWPSStartupCount = 1;
                // at the first WPS start, always process startup folder
                pDaemonShared->fProcessStartupFolder = TRUE;

                // now start the daemon;
                // we need to specify XWorkplace's "BIN"
                // subdir as the working dir, because otherwise
                // the daemon won't find XWPHOOK.DLL.

                // compose paths
                if (cmnQueryXWPBasePath(szDir))
                {
                    // path found:
                    PROGDETAILS pd;
                    // working dir: append bin
                    strcat(szDir, "\\bin");
                    // exe: append bin\xwpdaemon.exe
                    sprintf(szExe,
                            "%s\\xwpdaemn.exe",
                            szDir);
                    memset(&pd, 0, sizeof(pd));
                    pd.Length = sizeof(PROGDETAILS);
                    pd.progt.progc = PROG_PM;
                    pd.progt.fbVisible = SHE_VISIBLE;
                    pd.pszTitle = "XWorkplace Daemon";
                    pd.pszExecutable = szExe;
                    pd.pszParameters = "-D";
                    pd.pszStartupDir = szDir;
                    pd.pszEnvironment = "WORKPLACE\0\0";

                    G_KernelGlobals.happDaemon = WinStartApp(G_KernelGlobals.hwndThread1Object, // hwndNotify,
                                                             &pd,
                                                             "-D", // params; otherwise the daemon
                                                                   // displays a msg box
                                                             NULL,
                                                             0);// no SAF_INSTALLEDCMDLINE,
                    if (DumpFile)
                        fprintf(DumpFile,
                                "  WinStartApp for %s returned HAPP 0x%lX\n",
                                pd.pszExecutable,
                                G_KernelGlobals.happDaemon);

                    if (G_KernelGlobals.happDaemon)
                        // success:
                        fDaemonStarted = TRUE;
                }
            } // end if DosAllocSharedMem

        } // end if DosGetNamedSharedMem
        else
        {
            // shared memory block already exists:
            // this means the daemon is already running
            // and we have a WPS restart

            if (DumpFile)
                fprintf(DumpFile, "--> XWPDAEMN already running, refreshing.\n");

            // store new thread-1 object wnd
            pDaemonShared->hwndThread1Object = G_KernelGlobals.hwndThread1Object;
            // increase WPS startup count
            pDaemonShared->ulWPSStartupCount++;

            if (pDaemonShared->hwndDaemonObject)
            {
                WinSendMsg(pDaemonShared->hwndDaemonObject,
                           XDM_HOOKCONFIG,
                           0, 0);
                WinSendMsg(pDaemonShared->hwndDaemonObject,
                           XDM_HOTKEYSCHANGED,
                           0, 0);
                    // cross-process post, synchronously:
                    // this returns only after the hook has been re-initialized
            }
            // we leave the "reuse startup folder" flag alone,
            // because this was already set by XShutdown before
            // the last WPS restart
        }

        G_KernelGlobals.pDaemonShared = pDaemonShared;
    }

    /*
     *  interface XWPSHELL.EXE
     *
     */

    {
        PXWPSHELLSHARED pXWPShellShared = 0;
        APIRET arc = DosGetNamedSharedMem((PVOID*)&pXWPShellShared,
                                          SHMEM_XWPSHELL,
                                          PAG_READ | PAG_WRITE);
        if (DumpFile)
            fprintf(DumpFile,
                    "\nAttempted to access " SHMEM_XWPSHELL ", DosGetNamedSharedMem returned %d\n",
                    arc);

        if (arc == NO_ERROR)
        {
            // shared memory exists:
            // this means that XWPSHELL.EXE is running...
            // store this in KERNELGLOBALS
            G_KernelGlobals.pXWPShellShared = pXWPShellShared;

            // set flag that WPS termination will not provoke
            // logon; this is in case WPS crashes or user
            // restarts WPS. Only "Logoff" desktop menu item
            // will clear that flag.
            pXWPShellShared->fNoLogonButRestart = TRUE;

            if (DumpFile)
                fprintf(DumpFile, "--> XWPSHELL running, refreshed.\n");
        }
        else
            if (DumpFile)
                fprintf(DumpFile, "--> XWPSHELL not running.\n");
    }

    if (DumpFile)
    {
        fprintf(DumpFile,
                        "\nLeaving " __FUNCTION__", closing log.\n");
        fclose(DumpFile);
    }
}


