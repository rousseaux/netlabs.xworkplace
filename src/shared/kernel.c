
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
 *
 *      -- kernel locks and exception log information;
 *
 *      -- the KERNELGLOBALS interface (krnQueryGlobals);
 *
 *      -- the thread-1 object window (fnwpThread1Object);
 *
 *      In this file, I have assembled code which you might consider
 *      useful for extensions. For example, if you need code to
 *      execute on thread 1 of PMSHELL.EXE (which is required for
 *      some WPS methods to work, unfortunately), you can add a
 *      message to be processed in fnwpThread1Object.
 *
 *      If you need stuff to be executed upon Desktop startup, you can
 *      insert a function into initMain.
 *
 *      All functions in this file have the "krn*" prefix (V0.9.0).
 *
 *      The initialization code has been moved to init.c with
 *      V0.9.16.
 *
 *@@header "shared\kernel.h"
 */

/*
 *      Copyright (C) 1997-2001 Ulrich M”ller.
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

#pragma strings(readonly)

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
#define INCL_WINMENUS
#define INCL_WINDIALOGS
#define INCL_WINBUTTONS
#define INCL_WINPROGRAMLIST     // needed for PROGDETAILS, wppgm.h
#define INCL_WINSWITCHLIST
#define INCL_WINSHELLDATA
#define INCL_WINSTDFILE
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
#include "helpers\apps.h"               // application helpers
#include "helpers\comctl.h"             // common controls (window procs)
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\procstat.h"           // DosQProcStat handling
#include "helpers\standards.h"          // some standard macros
#include "helpers\stringh.h"            // string helper routines
#include "helpers\threads.h"            // thread helpers
#include "helpers\tree.h"               // red-black binary trees
#include "helpers\winh.h"               // PM helper routines
#include "helpers\xstring.h"            // extended string helpers

// SOM headers which don't crash with prec. header files
#include "xfstart.ih"
#include "xfldr.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "xwpapi.h"                     // public XWorkplace definitions

#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\helppanels.h"          // all XWorkplace help panel IDs
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

#include "filesys\filedlg.h"            // replacement file dialog implementation
#include "filesys\refresh.h"            // folder auto-refresh
#include "filesys\program.h"            // program implementation
#include "filesys\xthreads.h"           // extra XWorkplace threads

#include "media\media.h"                // XWorkplace multimedia support

#include "security\xwpsecty.h"          // XWorkplace Security

#include "startshut\archives.h"         // archiving declarations
#include "startshut\shutdown.h"         // XWorkplace eXtended Shutdown

// headers in /hook
#include "hook\xwphook.h"

// other SOM headers
// #include <wpdesk.h>                     // WPDesktop

#pragma hdrstop

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

// global lock semaphore for krnLock etc.
static HMTX             G_hmtxCommonLock = NULLHANDLE;

// "Quick open" dlg status (thread-1 object wnd)
// static ULONG            G_ulQuickOpenNow = 0,
   //                      G_ulQuickOpenMax = 0;
// static HWND             G_hwndQuickStatus = NULLHANDLE;
// static BOOL             G_fQuickOpenCancelled = FALSE;

// flags passed with mp1 of XDM_PAGEMAGECONFIG
static ULONG            G_PageMageConfigFlags = 0;

// global structure with data needed across threads
// (see kernel.h)
KERNELGLOBALS           G_KernelGlobals = {0};

// classes tree V0.9.16 (2001-09-29) [umoeller]
// see krnClassInitialized
TREE                    *G_ClassNamesTree;

// anchor block of WPS thread 1 (queried in initMain);
// this is exported thru kernel.h and never changed again
HAB                     G_habThread1 = NULLHANDLE;

// V0.9.11 (2001-04-25) [umoeller]
static HWND             G_hwndPageMageContextMenu = NULLHANDLE;

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
 *
 *@@added V0.9.0 (99-11-14) [umoeller]
 *@@changed V0.9.3 (2000-04-08) [umoeller]: moved this here from common.c
 *@@changed V0.9.7 (2000-12-13) [umoeller]: changed prototype to trace locks
 *@@changed V0.9.16 (2001-09-29) [umoeller]: added classes tree init
 */

BOOL krnLock(const char *pcszSourceFile,        // in: __FILE__
             ULONG ulLine,                      // in: __LINE__
             const char *pcszFunction)          // in: __FUNCTION__
{
    if (!G_hmtxCommonLock)
    {
        // first call:
        treeInit(&G_ClassNamesTree,        // V0.9.16 (2001-09-29) [umoeller]
                 NULL);
        return (!DosCreateMutexSem(NULL,         // unnamed
                                   &G_hmtxCommonLock,
                                   0,            // unshared
                                   TRUE));       // request now
    }

    // subsequent calls:
    if (WinRequestMutexSem(G_hmtxCommonLock, 10*1000) == NO_ERROR)
        // WinRequestMutexSem works even if the thread has no message queue
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
               "    Second (failed) requestor: %s (%s, line %d))",
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

/********************************************************************
 *
 *   KERNELGLOBALS structure
 *
 ********************************************************************/

/*
 *@@ krnQueryGlobals:
 *
 *@@added V0.9.16 (2001-09-29) [umoeller]
 */

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
 *   Class maintanance
 *
 ********************************************************************/

/*
 *@@ krnClassInitialized:
 *      registers the specified class name as
 *      "initialized" with the kernel.
 *
 *      This mechanism replaces the BOOLs in
 *      KERNELGLOBALS which had to be set to
 *      TRUE by each class's wpclsInitData.
 *      Instead, we now maintain a map of class
 *      names.
 *
 *      pcszClassName must be the simple class
 *      name, such as "XFldDataFile".
 *
 *      Returns TRUE only if the class name was
 *      added, that is, if it was not already in
 *      the list.
 *
 *@@added V0.9.16 (2001-09-29) [umoeller]
 */

BOOL krnClassInitialized(PCSZ pcszClassName)
{
    BOOL brc = FALSE;

    if (krnLock(__FILE__, __LINE__, __FUNCTION__))
            // krnLock initializes the tree now
    {
        TREE *pNew;

        if (pNew = NEW(TREE))
        {
            pNew->ulKey = (ULONG)pcszClassName;
            brc = !treeInsert(&G_ClassNamesTree,
                              NULL,
                              pNew,
                              treeCompareStrings);
        }

        krnUnlock();
    }

    return (brc);
}

/*
 *@@ krnIsClassReady:
 *      returns TRUE if the specified class was
 *      registered with krnClassInitialized, i.e.
 *      if the class is usable on the system.
 *
 *@@added V0.9.16 (2001-09-29) [umoeller]
 */

BOOL krnIsClassReady(PCSZ pcszClassName)
{
    BOOL brc = FALSE;

    if (krnLock(__FILE__, __LINE__, __FUNCTION__))
    {
        brc = (NULL != treeFind(G_ClassNamesTree,
                                (ULONG)pcszClassName,
                                treeCompareStrings));

        krnUnlock();
    }

    return (brc);
}

/* ******************************************************************
 *
 *   Exception handlers (\helpers\except.c)
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
        fprintf(file, "-----------------------------------------------------------\n"
                      "\nXWorkplace encountered an internal error.\n"
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

    ULONG       cThreadInfos = 0;
    PTHREADINFO paThreadInfos = NULL;

    // *** thread info
    if (ptib)
    {
        if (ptib->tib_ptib2)
        {
            THREADINFO ti;

            // find out which thread trapped
            tid = ptib->tib_ptib2->tib2_ultid;

            fprintf(file,
                    "Crashing thread:\n    TID 0x%lX (%d) ",
                    tid,        // hex
                    tid);       // dec

            if (thrFindThread(&ti, tid))
                fprintf(file, " (%s)", ti.pcszThreadName);
            else
                if (tid == pKernelGlobals->tidWorkplaceThread)  // V0.9.16 (2001-11-02) [pr]: Added thread 1 identification
                    fprintf(file, " (Workplace thread)");
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

    // V0.9.9 (2001-03-07) [umoeller]
    paThreadInfos = thrListThreads(&cThreadInfos);
    if (paThreadInfos)
    {
        ULONG ul;
        for (ul = 0;
             ul < cThreadInfos;
             ul++)
        {
            PTHREADINFO pThis = &paThreadInfos[ul];
            fprintf(file,
                    "    %s: ID 0x%lX (%d)\n",
                    pThis->pcszThreadName,
                    pThis->tid,
                    pThis->tid);
        }
        free(paThreadInfos);
    }

    tid = krnQueryLock();
    if (tid)
        fprintf(file, "\nGlobal lock semaphore is currently owned by thread 0x%lX (%u).\n", tid, tid);
    else
        fprintf(file, "\nGlobal lock semaphore is currently not owned.\n", tid, tid);

    arc = xthrQueryAwakeObjectsMutexOwner(&pid,
                                          &tid,
                                          &ulCount);
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
 *@@ krnEnableReplaceRefresh:
 *      enables or disables "replace auto-refresh folders".
 *      The setting does not take effect until after a
 *      Desktop restart.
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
 *@@ krnSetProcessStartupFolder:
 *      this gets called during XShutdown to set
 *      the flag in the XWPGLOBALSHARED shared-memory
 *      structure whether the XWorkplace startup
 *      folder should be re-used at the next WPS
 *      startup.
 *
 *      This is only meaningful between Desktop restarts,
 *      because this flag does not get stored anywhere.
 *
 *@@added V0.9.0 [umoeller]
 */

VOID krnSetProcessStartupFolder(BOOL fReuse)
{
    PKERNELGLOBALS pKernelGlobals = NULL;
    // ULONG ulNesting;
    // DosEnterMustComplete(&ulNesting);
    TRY_LOUD(excpt1)
    {
        pKernelGlobals = krnLockGlobals(__FILE__, __LINE__, __FUNCTION__);
        if (pKernelGlobals)
        {
            if (pKernelGlobals->pXwpGlobalShared)
            {
                // cast PVOID
                PXWPGLOBALSHARED pXwpGlobalShared = pKernelGlobals->pXwpGlobalShared;
                pXwpGlobalShared->fProcessStartupFolder = fReuse;
            }
        }
    }
    CATCH(excpt1) {} END_CATCH();

    if (pKernelGlobals)
        krnUnlockGlobals();

    // DosExitMustComplete(&ulNesting);
}

/*
 *@@ krnNeed2ProcessStartupFolder:
 *      this returns TRUE if the startup folder needs to
 *      be processed. See krnSetProcessStartupFolder.
 *
 *@@changed V0.9.0 [umoeller]: completely rewritten; now using XWPGLOBALSHARED shared memory.
 */

BOOL krnNeed2ProcessStartupFolder(VOID)
{
    BOOL brc = FALSE;
    PCKERNELGLOBALS pKernelGlobals = krnQueryGlobals();

    if (pKernelGlobals)
    {
        if (pKernelGlobals->pXwpGlobalShared)
        {
            // cast PVOID
            PXWPGLOBALSHARED pXwpGlobalShared = pKernelGlobals->pXwpGlobalShared;
            if (pXwpGlobalShared->fProcessStartupFolder)
            {
                brc = TRUE;
                // V0.9.9 (2001-03-19) [pr]: clear this after all startup
                // folders have been processed
                // pDaemonShared->fProcessStartupFolder = FALSE;
            }
        }
    }

    return (brc);
}

/*
 *@@ krnQueryDaemonObject:
 *      returns the window handle of the object window
 *      in XWPDAEMN.EXE or NULLHANDLE if the daemon
 *      is no longer running for some reason.
 *
 *@@added V0.9.14 (2001-08-01) [umoeller]
 */

HWND krnQueryDaemonObject(VOID)
{
    PCKERNELGLOBALS pKernelGlobals = krnQueryGlobals();
    if (pKernelGlobals)
    {
        // cast PVOID
        PXWPGLOBALSHARED pXwpGlobalShared = pKernelGlobals->pXwpGlobalShared;
        if (!pXwpGlobalShared)
            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                   "pXwpGlobalShared is NULL.");
        else
            // get the handle of the daemon's object window
            if (!pXwpGlobalShared->hwndDaemonObject)
                cmnLog(__FILE__, __LINE__, __FUNCTION__,
                       "pXwpGlobalShared->hwndDaemonObject is NULLHANDLE.");
            else
                return (pXwpGlobalShared->hwndDaemonObject);
    }

    return (NULLHANDLE);
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
    HWND hwnd;
    if (hwnd = krnQueryDaemonObject())
        brc = WinPostMsg(hwnd, msg, mp1, mp2);

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

    if (G_KernelGlobals.pXwpGlobalShared)
    {
        // cast PVOID
        PXWPGLOBALSHARED pXwpGlobalShared = G_KernelGlobals.pXwpGlobalShared;
        if (    (pGlobalSettings->fEnableXWPHook)
             && (pXwpGlobalShared->hwndDaemonObject)
           )
        {
            if (WinSendMsg(pXwpGlobalShared->hwndDaemonObject,
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

#ifndef __NOPAGEMAGE__
                if (pGlobalSettings->fEnablePageMage)
                    // PageMage is enabled too:
                    WinSendMsg(pXwpGlobalShared->hwndDaemonObject,
                               XDM_STARTSTOPPAGEMAGE,
                               (MPARAM)TRUE,
                               0);
#endif
            }
        }
    }
}

/*
 *@@ krn_T1M_OpenObjectFromHandle:
 *      implementation for T1M_OPENOBJECTFROMHANDLE in
 *      fnwpThread1Object.
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
 *@@changed V0.9.13 (2001-06-23) [umoeller]: now using winhQuerySwitchList
 *@@changed V0.9.16 (2001-11-22) [umoeller]: now disallowing object open during startup and shutdown
 */

VOID krn_T1M_OpenObjectFromHandle(HWND hwndObject,
                                  MPARAM mp1,
                                  MPARAM mp2)
{
    HOBJECT hobjStart;

    // make sure the desktop is already fully populated
    // V0.9.16 (2001-10-25) [umoeller]
    if (!G_KernelGlobals.fDesktopPopulated)
        return;

    // make sure we're not shutting down,
    // but allow object hotkeys while confirmation
    // dialogs are open
    // V0.9.16 (2001-11-22) [umoeller]
    if (xsdIsShutdownRunning() > 1)
        return;

    if (hobjStart = (HOBJECT)mp1)
    {
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

                        PSWBLOCK pSwBlock;
                                // now using winhQuerySwitchList V0.9.13 (2001-06-23) [umoeller]

                        if (pSwBlock = winhQuerySwitchList(G_habThread1))
                        {
                            // loop through all the tasklist entries
                            ULONG ul;
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
 *@@ fnwpThread1Object:
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
 *@@changed V0.9.14 (2001-08-07) [pr]: added T1M_OPENRUNDIALOG
 */

MRESULT EXPENTRY fnwpThread1Object(HWND hwndObject, ULONG msg, MPARAM mp1, MPARAM mp2)
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

#ifndef __NOPAGEMAGE__
                    case 2: // started from T1M_PAGEMAGECONFIGDELAYED
                    {
                        PXWPGLOBALSHARED   pXwpGlobalShared = G_KernelGlobals.pXwpGlobalShared;

                        if (pXwpGlobalShared)
                            if (pXwpGlobalShared->hwndDaemonObject)
                            {
                                // cross-process send msg: this
                                // does not return until the daemon
                                // has re-read the data
                                BOOL brc = (BOOL)WinSendMsg(pXwpGlobalShared->hwndDaemonObject,
                                                            XDM_PAGEMAGECONFIG,
                                                            (MPARAM)G_PageMageConfigFlags,
                                                            0);
                                // reset flags
                                G_PageMageConfigFlags = 0;
                            }
                    break; }
#endif
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
                progAppTerminateNotify((HAPP)mp1);
            break;

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
                        // restart Desktop: Yes/No box
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
             *      which gets called from initMain
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
             *@@ T1M_MENUITEMSELECTED:
             *      calls wpMenuItemSelected on thread 1.
             *      This can be posted or sent, we don't care.
             *      This is used from the XCenter to make sure
             *      views won't get opened on the XCenter thread.
             *
             *      Parameters:
             *
             *      --  WPObject* mp1: object on which to
             *          invoke wpMenuItemSelected.
             *
             *      --  ULONG mp2: menu item ID to pass to
             *          wpMenuItemSelected.
             *
             *      Returns the BOOL rc of wpMenuItemSelected.
             *
             *@@added V0.9.11 (2001-04-18) [umoeller]
             */

            case T1M_MENUITEMSELECTED:
                mrc = (MPARAM)_wpMenuItemSelected((WPObject*)mp1,
                                                  NULLHANDLE,       // hwndFrame
                                                  (ULONG)mp2);
            break;

            /*
             *@@ T1M_DAEMONREADY:
             *      posted by the XWorkplace daemon after it has
             *      successfully created its object window.
             *      This can happen in two situations:
             *
             *      -- during Desktop startup, after initMain
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

#ifndef __NOPAGEMAGE__
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
#endif

            /*
             *@@ T1M_WELCOME:
             *      posted if XWorkplace has just been installed.
             *
             *      This post comes from the Startup thread after
             *      all other startup processing (startup folders,
             *      quick open, etc.) has completed, but only if
             *      the "just installed" flag was set in OS2.INI
             *      (which has then been removed).
             *
             *      Starting with V0.9.9, we now allow the user
             *      to create the XWorkplace standard objects
             *      here. We no longer do this from WarpIn because
             *      we also defer class registration into the OS2.INI
             *      file to avoid the frequent error messages that
             *      WarpIN produces otherwise.
             *
             *@@added V0.9.7 (2001-01-07) [umoeller]
             *@@changed V0.9.9 (2001-03-27) [umoeller]: added obj creation here
             */

            case T1M_WELCOME:
                if (cmnMessageBoxMsg(NULLHANDLE,
                                     121,
                                     211,       // create objects?
                                     MB_OKCANCEL)
                        == MBID_OK)
                {
                    // produce objects NOW
                    xthrPostFileMsg(FIM_RECREATECONFIGFOLDER,
                                    (MPARAM)RCF_MAININSTALLFOLDER,
                                    0);
                }
            break;

            /*
             *@@ T1M_PAGEMAGECTXTMENU:
             *      gets posted from PageMage if the user
             *      right-clicked onto an empty space in the pager
             *      window (and not on a mini-window).
             *
             *      We should then display the PageMage context
             *      menu here because
             *
             *      1)  PageMage cannot handle the commands in
             *          the first place (such as open settings)
             *
             *      2)  we don't want NLS stuff in the daemon.
             *
             *      Parameters:
             *
             *      SHORT1FROMMP(mp1): desktop x coordinate of
             *                         mouse click.
             *      SHORT2FROMMP(mp1): desktop y coordinate of
             *                         mouse click.
             *
             *@@added V0.9.11 (2001-04-25) [umoeller]
             */

            case T1M_PAGEMAGECTXTMENU:
                if (!G_hwndPageMageContextMenu)
                    G_hwndPageMageContextMenu = WinLoadMenu(hwndObject,
                                                            cmnQueryNLSModuleHandle(FALSE),
                                                            ID_XSM_PAGEMAGECTXTMENU);

                WinPopupMenu(HWND_DESKTOP,      // parent
                             hwndObject,        // owner
                             G_hwndPageMageContextMenu,
                             SHORT1FROMMP(mp1),
                             SHORT2FROMMP(mp1),
                             0,
                             PU_HCONSTRAIN | PU_VCONSTRAIN | PU_MOUSEBUTTON1
                                | PU_MOUSEBUTTON2 | PU_KEYBOARD);
                                    // WM_COMMAND is handled below
            break;

            /*
             * WM_COMMAND:
             *      handle commands from the PageMage context menu
             *      here (thread-1 object window was specified as
             *      menu's owner above).
             * added V0.9.11 (2001-04-25) [umoeller]
             */

            case WM_COMMAND:
                switch ((USHORT)mp1)
                {
                    case ID_CRMI_PROPERTIES:
                    {
                        // open "Screen" object
                        HOBJECT hobj = WinQueryObject((PSZ)XFOLDER_SCREENID);
                        if (hobj)
                            krn_T1M_OpenObjectFromHandle(hwndObject,
                                                         (MPARAM)hobj,
                                                         (MPARAM)0);   // no screen corner
                    break; }

                    case ID_CRMI_HELP:
                        cmnDisplayHelp(NULL,        // active desktop
                                       ID_XSH_PAGEMAGE_INTRO);
                    break;
                }
            break;

            /*
             *@@ T1M_INITIATEXSHUTDOWN:
             *      posted from the XCenter X-button widget
             *      to have XShutdown initiated with the
             *      current settings.
             *
             *      (ULONG)mp1 must be one of the following:
             *      --  ID_CRMI_LOGOFF: logoff.
             *      --  ID_CRMI_RESTARTWPS: restart Desktop.
             *      --  ID_CRMI_SHUTDOWN: "real" shutdown.
             *
             *      These are the menu item IDs from the
             *      X-button menu.
             *
             *      We have this message here now because
             *      initiating XShutdown from an XCenter
             *      thread means asking for trouble.
             *
             *@@added V0.9.12 (2001-04-28) [umoeller]
             */

            case T1M_INITIATEXSHUTDOWN:
                switch ((ULONG)mp1)
                {
                    case ID_CRMI_LOGOFF:
                        xsdInitiateRestartWPS(TRUE);    // logoff
                    break;

                    case ID_CRMI_RESTARTWPS:
                        xsdInitiateRestartWPS(FALSE);   // restart Desktop, no logoff
                    break;

                    case ID_CRMI_SHUTDOWN:
                        xsdInitiateShutdown();
                    break;
                }
            break;

            /*
             *@@ T1M_OPENRUNDIALOG:
             *      this gets posted from the XCenter thread
             *      to open the Run dialog.
             */

            case T1M_OPENRUNDIALOG:
                cmnRunCommandLine((HWND)mp1,
                                  (const char *)mp2);
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
 *      post msg to thread-1 object window (fnwpThread1Object).
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
 *      send msg to thread-1 object window (fnwpThread1Object).
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

/* ******************************************************************
 *
 *   API object window
 *
 ********************************************************************/

/*
 *@@ fnwpAPIObject:
 *      window proc for the XWorkplace API object window.
 *
 *      This API object window is quite similar to the thread-1
 *      object window (fnwpThread1Object), except that its
 *      messages are defined in include\xwpapi.h. As a result,
 *      this thing handles public messages to allow external
 *      processes to communicate with XWorkplace in the WPS
 *      process.
 *
 *      Like the thread-1 object window, this is created on
 *      Desktop startup by initMain and runs on
 *      thread-1 of the WPS always.
 *
 *@@added V0.9.9 (2001-03-23) [umoeller]
 */

MRESULT EXPENTRY fnwpAPIObject(HWND hwndObject, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    switch (msg)
    {
        /*
         *@@ APIM_FILEDLG:
         *      opens the XWorkplace file dialog.
         *
         *      Parameters:
         *
         *      -- PXWPFILEDLG mp1: file dialog parameters. NOTE:
         *         If this message comes from another process,
         *         this better point to shared memory, and all
         *         member pointers must point to shared memory
         *         as well. This shared memory must also be
         *         given to the WPS process.
         *
         *      -- mp2: not used, always 0.
         *
         *      This message must be POSTED to the API object
         *      window, and the posting thread should then enter
         *      a modal message loop until XWPFILEDLG.hwndNotify
         *      receives WM_USER.
         *
         *      Returns: HWND like WinFileDlg.
         *
         *@@added V0.9.9 (2001-03-23) [umoeller]
         */

        case APIM_FILEDLG:
        {
            PXWPFILEDLG pfd = (PXWPFILEDLG)mp1;
            if (pfd)
            {
                // open the (modal) file dialog; this does
                // not return until the dialog is dismissed
                pfd->hwndReturn = fdlgFileDlg(pfd->hwndOwner,
                                              pfd->szCurrentDir,
                                              &pfd->fd);
            }

            // now post WM_USER back to the notify window
            // given to us
            WinPostMsg(pfd->hwndNotify,
                       WM_USER,
                       0, 0);
        }
        break;

        /*
         *@@ APIM_NETSCDDEHELP:
         *      displays the help for NetscapeDDE.
         *
         *      No parameters.
         *
         *@@added V0.9.16 (2001-10-02) [umoeller]
         */

        case APIM_NETSCDDEHELP:
            cmnDisplayHelp(NULL, ID_XSH_NETSCAPEDDE);
        break;

        default:
            mrc = WinDefWindowProc(hwndObject, msg, mp1, mp2);
    }

    return (mrc);
}


