
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
 *      Copyright (C) 1997-99 Ulrich M”ller.
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
#define INCL_DOSERRORS
#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WINTIMER
#define INCL_WINSYS
#define INCL_WINDIALOGS
#define INCL_WINBUTTONS
#define INCL_WINPROGRAMLIST     // needed for PROGDETAILS, wppgm.h
#define INCL_WINSHELLDATA
#include <os2.h>

// C library headers
#include <stdio.h>              // needed for except.h
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h
#include <io.h>                 // access etc.

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\comctl.h"             // common controls (window procs)
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\stringh.h"            // string helper routines
#include "helpers\winh.h"               // PM helper routines

// SOM headers which don't crash with prec. header files
#pragma hdrstop                         // VAC++ keeps crashing otherwise
#include "xfldr.h"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

#include "filesys\xthreads.h"           // extra XWorkplace threads

#include "startshut\archives.h"         // WPSArcO declarations

// headers in /hook
#include "hook\xwphook.h"

// other SOM headers
#include <wpdesk.h>                     // WPDesktop

/* ******************************************************************
 *                                                                  *
 *   Global variables                                               *
 *                                                                  *
 ********************************************************************/

// "Quick open" dlg status (thread-1 object wnd)
ULONG               ulQuickOpenNow = 0,
                    ulQuickOpenMax = 0;
HWND                hwndQuickStatus = NULLHANDLE;
BOOL                fQuickOpenCancelled = FALSE;

// global structure with data needed across threads
// (see kernel.h)
KERNELGLOBALS       ThreadGlobals = {0};

// forward declarations
MRESULT EXPENTRY fnwpStartupDlg(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
MRESULT EXPENTRY fncbStartup(HWND hwndStatus, ULONG ulObject, MPARAM mpNow, MPARAM mpMax);
MRESULT EXPENTRY fnwpQuickOpenDlg(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
MRESULT EXPENTRY fncbQuickOpen(HWND hwndFolder, ULONG ulObject, MPARAM mpNow, MPARAM mpMax);

/********************************************************************
 *                                                                  *
 *   KERNELGLOBALS structure                                        *
 *                                                                  *
 ********************************************************************/

PCKERNELGLOBALS krnQueryGlobals(VOID)
{
    return &ThreadGlobals;
}

/*
 *@@ krnLockGlobals:
 *      this returns the global KERNELGLOBALS structure
 *      which contains all kinds of data which need to
 *      be accessed across threads. This structure is
 *      a global structure in kernel.c.
 *
 *      This calls cmnLock to lock the globals.
 */

PKERNELGLOBALS krnLockGlobals(ULONG ulTimeout)
{
    if (cmnLock(ulTimeout))
        return (&ThreadGlobals);
    else
        return (NULL);
}

/*
 *@@ krnUnlockGlobals:
 *
 */

VOID krnUnlockGlobals(VOID)
{
    cmnUnlock();
}

/* ******************************************************************
 *                                                                  *
 *   XFolder hook for exception handlers (\helpers\except.c)        *
 *                                                                  *
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
 */

FILE* _System krnExceptOpenLogFile(VOID)
{
    CHAR        szFileName[CCHMAXPATH];
    FILE        *file;
    DATETIME    DT;

    // ULONG ulCopied;

    sprintf(szFileName, "%c:\\%s", doshQueryBootDrive(), XFOLDER_CRASHLOG);
    file = fopen(szFileName, "a");

    if (file) {
        DosGetDateTime(&DT);
        fprintf(file, "\nXFolder trap message -- Date: %02d/%02d/%04d, Time: %02d:%02d:%02d\n",
            DT.month, DT.day, DT.year,
            DT.hours, DT.minutes, DT.seconds);
        fprintf(file, "--------------------------------------------------------\n"
                      "\nAn internal error occurred within XWorkplace.\n"
                      "Please contact the author so that this error may be removed\n"
                      "in future XWorkplace versions. A contact address may be\n"
                      "obtained from the XWorkplace User Guide. Please supply\n"
                      "this file (?:\\XFLDTRAP.LOG) with your e-mail and describe as\n"
                      "exactly as possible the conditions under which the error\n"
                      "occured.\n"
                      "\nRunning XWorkplace version: " XFOLDER_VERSION "\n");

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
    TID         tid;
    PCKERNELGLOBALS pKernelGlobals = krnQueryGlobals();

    // *** thread info
    if (ptib)
    {
        if (ptib->tib_ptib2)
        {

            // find out which thread trapped
            fprintf(file,
                "Thread information: \n    TID 0x%lX (%d) ",
                ptib->tib_ptib2->tib2_ultid,        // hex
                ptib->tib_ptib2->tib2_ultid);       // dec

            if (ptib->tib_ptib2->tib2_ultid == thrQueryID(pKernelGlobals->ptiWorkerThread))
                fprintf(file, " (XWorkplace Worker thread)");
            else if (ptib->tib_ptib2->tib2_ultid == thrQueryID(pKernelGlobals->ptiQuickThread))
                fprintf(file, " (XWorkplace Quick thread)");
            else if (ptib->tib_ptib2->tib2_ultid == thrQueryID(pKernelGlobals->ptiFileThread))
                fprintf(file, " (XWorkplace File thread)");
            else if (ptib->tib_ptib2->tib2_ultid == thrQueryID(pKernelGlobals->ptiUpdateThread))
                fprintf(file, " (XWorkplace Update thread)");
            else if (ptib->tib_ptib2->tib2_ultid == thrQueryID(pKernelGlobals->ptiShutdownThread))
                fprintf(file, " (XWorkplace Shutdown thread)");
            else if (ptib->tib_ptib2->tib2_ultid == pKernelGlobals->tidWorkplaceThread)
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

    if (tid = thrQueryID(pKernelGlobals->ptiWorkerThread))
        fprintf(file,  "    XWorkplace Worker thread ID: 0x%lX (%d)\n", tid, tid);

    if (tid = thrQueryID(pKernelGlobals->ptiQuickThread))
        fprintf(file,  "    XWorkplace Quick thread ID: 0x%lX (%d)\n", tid, tid);

    if (tid = thrQueryID(pKernelGlobals->ptiFileThread))
        fprintf(file,  "    XWorkplace File thread ID: 0x%lX (%d)\n", tid, tid);

    if (tid = thrQueryID(pKernelGlobals->ptiShutdownThread))
        fprintf(file,  "    XWorkplace Shutdown thread ID: 0x%lX (%d)\n", tid, tid);

    if (tid = thrQueryID(pKernelGlobals->ptiUpdateThread))
        fprintf(file,  "    XWorkplace Update thread ID: 0x%lX (%d)\n", tid, tid);

}

/* ******************************************************************
 *                                                                  *
 *   Startup/Daemon interface                                       *
 *                                                                  *
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
    PKERNELGLOBALS pKernelGlobals = krnLockGlobals(5000);
    if (pKernelGlobals->pDaemonShared)
    {
        // cast PVOID
        PDAEMONSHARED pDaemonShared = pKernelGlobals->pDaemonShared;
        pDaemonShared->fProcessStartupFolder = fReuse;
    }
    krnUnlockGlobals();
}

/*
 *@@ krnNeed2ProcessStartupFolder:
 *      this returns TRUE if the startup folder needs to
 *      be processed.
 *
 *@@changed V0.9.0 [umoeller]: completely rewritten; now using DAEMONSHARED shared memory.
 */

BOOL krnNeed2ProcessStartupFolder(VOID)
{
    BOOL brc = FALSE;
    PKERNELGLOBALS pKernelGlobals = krnLockGlobals(5000);
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

    krnUnlockGlobals();

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
    // cast PVOID
    PDAEMONSHARED pDaemonShared = pKernelGlobals->pDaemonShared;
    if (pDaemonShared)
        // get the handle of the daemon's object window
        if (pDaemonShared->hwndDaemonObject)
            brc = WinPostMsg(pDaemonShared->hwndDaemonObject, msg, mp1, mp2);

    return (brc);
}

/* ******************************************************************
 *                                                                  *
 *   Thread-1 object window                                         *
 *                                                                  *
 ********************************************************************/

BOOL     fLimitMsgOpen = FALSE;
HWND     hwndArchiveStatus = NULLHANDLE;

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
 */

MRESULT EXPENTRY krn_fnwpThread1Object(HWND hwndObject, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MPARAM mrc = NULL;

    switch(msg)
    {
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
                DosBeep(1000, 1000);
            #endif

            // check if startup folder is to be skipped
            if ((pKernelGlobals->ulPanicFlags & SUF_SKIPXFLDSTARTUP) == 0)
            {
                // no: find startup folder
                if (pStartupFolder = _wpclsQueryFolder(_WPFolder, XFOLDER_STARTUPID, TRUE))
                {
                    // startup folder exists: create status window w/ progress bar,
                    // start folder content processing in worker thread
                    // CHAR szTitle[200];
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
                        #ifdef DEBUG_STARTUP
                            DosBeep(2000, 1000);
                        #endif
                    }

                    hPOC = _xwpBeginProcessOrderedContent(pStartupFolder,
                                                          pGlobalSettings->ulStartupDelay,
                                                          &fncbStartup,
                                                          (ULONG)hwndStartupStatus);

                    WinSetWindowULong(hwndStartupStatus, QWL_USER, hPOC);
                }
                else
                    // startup folder does not exist:
                    xthrPostFileMsg(FIM_STARTUPFOLDERDONE,
                                    (MPARAM)1,      // done with startup
                                    MPNULL);
            }
            else
            {
                // shift pressed: skip startup
                xthrPostFileMsg(FIM_STARTUPFOLDERDONE,
                                (MPARAM)1,      // done with startup
                                MPNULL);
            }
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
                    ulQuickOpenNow = 0;
                    ulQuickOpenMax = 0;
                    for (pQuick = _xwpclsQueryQuickOpenFolder(_XFolder, NULL);
                         pQuick;
                         pQuick = _xwpclsQueryQuickOpenFolder(_XFolder, pQuick))
                    {
                        ulQuickOpenMax++;
                    }

                    if (ulQuickOpenMax)
                    {
                        fWorkToDo = TRUE;
                        // if we have any quick-open folders: go
                        if (pGlobalSettings->ShowStartupProgress)
                        {
                            PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();
                            hwndQuickStatus = WinLoadDlg(HWND_DESKTOP, NULLHANDLE,
                                                         fnwpQuickOpenDlg,
                                                         cmnQueryNLSModuleHandle(FALSE),
                                                         ID_XFD_STARTUPSTATUS,
                                                         NULL);
                            WinSetWindowText(hwndQuickStatus,
                                    pNLSStrings->pszQuickStatus);

                            winhRestoreWindowPos(hwndQuickStatus,
                                        HINI_USER,
                                        INIAPP_XWORKPLACE, INIKEY_WNDPOSSTARTUP,
                                        SWP_MOVE | SWP_SHOW | SWP_ACTIVATE);
                            #ifdef DEBUG_STARTUP
                                DosBeep(2000, 1000);
                            #endif
                            WinSendMsg(WinWindowFromID(hwndQuickStatus, ID_SDDI_PROGRESSBAR),
                                       WM_UPDATEPROGRESSBAR,
                                       (MPARAM)0, (MPARAM)ulQuickOpenMax);
                            fQuickOpenCancelled = FALSE;
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
                    &&  (!fQuickOpenCancelled)
                   )
                {
                    fWorkToDo = TRUE;
                    // subsequent postings: mp1 contains folder
                    if (pGlobalSettings->ShowStartupProgress)
                    {
                        CHAR szTemp[256];

                        _wpQueryFilename(pQuick, szTemp, TRUE);
                        WinSetDlgItemText(hwndQuickStatus, ID_SDDI_STATUS, szTemp);
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
                        winhSaveWindowPos(hwndQuickStatus,
                                          HINI_USER,
                                          INIAPP_XWORKPLACE, INIKEY_WNDPOSSTARTUP);
                        WinDestroyWindow(hwndQuickStatus);
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
                ulQuickOpenNow++;

                if (pGlobalSettings->ShowStartupProgress)
                    WinSendMsg(WinWindowFromID(hwndQuickStatus, ID_SDDI_PROGRESSBAR),
                               WM_UPDATEPROGRESSBAR,
                               (MPARAM)(ulQuickOpenNow*100),
                               (MPARAM)(ulQuickOpenMax*100));

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

            // error or cancelled:
            if (pGlobalSettings->ShowStartupProgress)
                WinPostMsg(hwndQuickStatus, WM_CLOSE, MPNULL, MPNULL);
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
                cmnSetHelpPanel(ID_XFH_LIMITREACHED);
                WinDlgBox(HWND_DESKTOP,         // parent is desktop
                          HWND_DESKTOP,             // owner is desktop
                          (PFNWP)fnwpDlgGeneric,    // dialog procedure, defd. at bottom
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
         */

        case T1M_EXCEPTIONCAUGHT:
        {
            if (mp1)
            {
                if (mp2)
                {
                    // restart WPS: Yes/No box
                    if (WinMessageBox(HWND_DESKTOP, HWND_DESKTOP,
                                      (PSZ)mp1, (PSZ)"XFolder: Exception caught",
                                      0, MB_YESNO | MB_ICONEXCLAMATION | MB_MOVEABLE)
                             == MBID_YES)
                        // if yes: terminate the current process,
                        // which is PMSHELL.EXE. We cannot use DosExit()
                        // directly, because this might mess up the
                        // C runtime library.
                        exit(99);
                }
                else
                    // just report:
                    DebugBox("XFolder: Exception caught", (PSZ)mp1);
                free((PSZ)mp1);
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
            sscanf(XFOLDER_VERSION, // this is defined in dlgids.h
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

        case T1M_EXTERNALSHUTDOWN: // ###
        {
/*             PSHUTDOWNPARAMS psdp = (PSHUTDOWNPARAMS)mp1;
            if ((ULONG)mp2 != 1234)
            {
                APIRET arc = DosGetSharedMem(psdp, PAG_READ | PAG_WRITE);
                if (arc == NO_ERROR)
                {
                    WinPostMsg(hwndObject, T1M_EXTERNALSHUTDOWN, mp1, (MPARAM)1234);
                    mrc = (MPARAM)TRUE;
                }
                else
                {
                    DebugBox("External XShutdown call", "Error calling DosGetSharedMem.");
                    mrc = (MPARAM)FALSE;
                }
            }
            else
            {
                // second call
                xsdInitiateShutdownExt(psdp);
                DosFreeMem(psdp);
            } */
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

        case WM_TIMER:
            if ((USHORT)mp1 == 1)
                WinDestroyWindow(hwndArchiveStatus);
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
         *                  0 = no corner.
         */

        case T1M_OPENOBJECTFROMHANDLE:
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
                        HWND hwnd = _wpViewObject(pobjStart,
                                                  NULLHANDLE,   // hwndCnr (?!?)
                                                  OPEN_DEFAULT,
                                                  0);           // "optional parameter" (?!?)
                        if (hwnd)
                            // move to front
                            WinSetActiveWindow(HWND_DESKTOP, hwnd);
                    }
                }
                else
                {
                    // special objects:
                    switch ((ULONG)hobjStart)
                    {
                        case 0xFFFF0000:
                            // show window list
                            WinPostMsg(_wpclsQueryActiveDesktopHWND(_WPDesktop),
                                       WM_COMMAND,
                                       (MPARAM)0x8011,
                                       MPFROM2SHORT(CMDSRC_MENU,
                                                    TRUE));
                        break;

                        case 0xFFFF0001:
                        {
                            // show Desktop's context menu V0.9.1 (99-12-19) [umoeller]
                            WPDesktop* pActiveDesktop = _wpclsQueryActiveDesktop(_WPDesktop);
                            HWND hwndFrame = _wpclsQueryActiveDesktopHWND(_WPDesktop);
                            if ((pActiveDesktop) && (hwndFrame))
                            {
                                HWND hwndClient = wpshQueryCnrFromFrame(hwndFrame);
                                POINTL ptlPopup = { 0, 0 }; // default: lower left
                                switch ((ULONG)mp2)
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
                                }
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
        break;

        /*
         *@@ T1M_DAEMONREADY:
         *      posted by the XWorkplace daemon after it has
         *      successfully created its object window. The
         *      thread-1 object window will then send XDM_HOOKINSTALL
         *      back to the daemon if the global settings have the
         *      hook enabled.
         */

        case T1M_DAEMONREADY:
        {
            PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
            if (ThreadGlobals.pDaemonShared)
            {
                // cast PVOID
                PDAEMONSHARED pDaemonShared = ThreadGlobals.pDaemonShared;
                if (    (pGlobalSettings->fEnableXWPHook)
                     && (pDaemonShared->hwndDaemonObject)
                   )
                    WinPostMsg(pDaemonShared->hwndDaemonObject,
                               XDM_HOOKINSTALL,
                               (MPARAM)TRUE,
                               0);
            }
        break; }

        default:
            mrc = WinDefWindowProc(hwndObject, msg, mp1, mp2);
    }
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
                    GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(5000);
                    pGlobalSettings->ShowStartupProgress = 0;
                    cmnUnlockGlobalSettings();
                    cmnStoreGlobalSettings();
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
            switch (SHORT1FROMMP(mp1)) {

                case DID_CANCEL:
                {
                    fQuickOpenCancelled = TRUE;
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
                    GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(5000);
                    pGlobalSettings->ShowStartupProgress = 0;
                    cmnUnlockGlobalSettings();
                    cmnStoreGlobalSettings();
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
    WinSendMsg(WinWindowFromID(hwndQuickStatus, ID_SDDI_PROGRESSBAR),
               WM_UPDATEPROGRESSBAR,
               (MPARAM)(
                           ulQuickOpenNow*100 +
                               ( (100 * (ULONG)mpNow) / (ULONG)mpMax )
                       ),
               (MPARAM)(ulQuickOpenMax*100));

    // if "Cancel" has been pressed, return FALSE
    return ((MPARAM)(!fQuickOpenCancelled));
}

/* ******************************************************************
 *                                                                  *
 *   XWorkplace initialization                                      *
 *                                                                  *
 ********************************************************************/

/*
 *@@ krnShowStartupDlgs:
 *      this gets called from krnInitializeXWorkplace
 *      to show dialogs while the WPS is starting up.
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

    // check if XWorkplace was just installed
    if (PrfQueryProfileInt(HINI_USER, INIAPP_XWORKPLACE,
                           INIKEY_JUSTINSTALLED,
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
                            INIAPP_XWORKPLACE, INIKEY_GLOBALSETTINGS,
                            &cbData)
            == FALSE)
    {
        // XWorkplace keys do _not_ exist:
        // check if we have old XFolder settings
        if (PrfQueryProfileSize(HINI_USER,
                                INIAPP_OLDXFOLDER, INIKEY_GLOBALSETTINGS,
                                &cbData))
        {
            if (cmnMessageBoxMsg(HWND_DESKTOP,
                                 121,       // "XWorkplace"
                                 160,       // "convert?"
                                 MB_YESNO)
                    == MBID_YES)
            {
                // yes, convert:
                // copy application from "XFolder" to "XWorkplace"
                prfhCopyApp(HINI_USER,
                            INIAPP_OLDXFOLDER,      // source
                            HINI_USER,
                            INIAPP_XWORKPLACE,
                            NULL);
                // reload
                cmnLoadGlobalSettings(FALSE);
            }
        }
    }

    /*
     * "Panic" dlg
     *
     */

    if (doshQueryShiftState())
    {
        PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
        // shift pressed: show "panic" dialog
        HWND    hwndPanic = WinLoadDlg(HWND_DESKTOP, HWND_DESKTOP,
                                       WinDefDlgProc,
                                       cmnQueryNLSModuleHandle(FALSE),
                                       ID_XFD_STARTUPPANIC,
                                       NULL);

        winhCenterWindow(hwndPanic);

        // disable items which are irrelevant
        WinEnableControl(hwndPanic, ID_XFDI_PANIC_SKIPBOOTLOGO,
                          pGlobalSettings->BootLogo);
        WinEnableControl(hwndPanic, ID_XFDI_PANIC_NOARCHIVING,
                          pGlobalSettings->fReplaceArchiving);
        WinEnableControl(hwndPanic, ID_XFDI_PANIC_DISABLEREPLICONS,
                          pGlobalSettings->fReplaceIcons);

        if (WinProcessDlg(hwndPanic) == DID_OK)
        {
            if (winhIsDlgItemChecked(hwndPanic, ID_XFDI_PANIC_SKIPBOOTLOGO))
                ThreadGlobals.ulPanicFlags |= SUF_SKIPBOOTLOGO;
            if (winhIsDlgItemChecked(hwndPanic, ID_XFDI_PANIC_SKIPXFLDSTARTUP))
                ThreadGlobals.ulPanicFlags |= SUF_SKIPXFLDSTARTUP;
            if (winhIsDlgItemChecked(hwndPanic, ID_XFDI_PANIC_SKIPQUICKOPEN))
                ThreadGlobals.ulPanicFlags |= SUF_SKIPQUICKOPEN;
            if (winhIsDlgItemChecked(hwndPanic, ID_XFDI_PANIC_NOARCHIVING))
            {
                PWPSARCOSETTINGS pArcSettings = arcQuerySettings();
                // disable "check archives" flag
                pArcSettings->ulArcFlags &= ~ARCF_ENABLED;
            }
            if (winhIsDlgItemChecked(hwndPanic, ID_XFDI_PANIC_DISABLEREPLICONS))
            {
                GLOBALSETTINGS *pGlobalSettings2 = cmnLockGlobalSettings(5000);
                pGlobalSettings2->fReplaceIcons = FALSE;
                cmnUnlockGlobalSettings();
                cmnStoreGlobalSettings();
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
        }
        else
        {
            // "Shutdown" pressed:
            WinShutdownSystem(WinQueryAnchorBlock(HWND_DESKTOP),
                              WinQueryWindowULong(HWND_DESKTOP, QWL_HMQ));
            while (TRUE)
                DosSleep(1000);
        }

        WinDestroyWindow(hwndPanic);
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
 *         threads started. The Quick thread will then display the
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
 */

VOID krnInitializeXWorkplace(VOID)
{
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    // check if we're called for the first time,
    // because we better initialize this only once
    if (ThreadGlobals.hwndThread1Object == NULLHANDLE)
    {

        // zero KERNELGLOBALS
        memset(&ThreadGlobals, 0, sizeof(KERNELGLOBALS));

        // store WPS startup time
        DosGetDateTime(&ThreadGlobals.StartupDateTime);

        // register exception hooks for /helpers/except.c
        excRegisterHooks(krnExceptOpenLogFile,
                         krnExceptExplainXFolder,
                         !pGlobalSettings->fNoExcptBeeps);

        // initialize awake-objects list (which holds
        // plain WPObject* pointers)
        ThreadGlobals.pllAwakeObjects = lstCreate(FALSE);   // no auto-free items

        // if shift is pressed, show "Panic" dialog
        krnShowStartupDlgs();

        // create main object window
        WinRegisterClass(WinQueryAnchorBlock(HWND_DESKTOP),
                         WNDCLASS_THREAD1OBJECT,    // class name
                         (PFNWP)krn_fnwpThread1Object,    // Window procedure
                         0,                  // class style
                         0);                 // extra window words
        ThreadGlobals.hwndThread1Object = WinCreateWindow(
                            HWND_OBJECT,
                            WNDCLASS_THREAD1OBJECT, // class name
                            (PSZ)"",     // title
                            0,           // style
                            0,0,0,0,     // position
                            0,           // owner
                            HWND_BOTTOM, // z-order
                            ID_THREAD1OBJECT, // window id
                            NULL,        // create params
                            NULL);       // pres params

        if (ThreadGlobals.hwndThread1Object == NULLHANDLE)
            DebugBox("XFolder: Error",
                    "XFolder failed to create the XFolder Workplace object window.");
    }

    /*
     *  enable NumLock at startup
     *      V0.9.1 (99-12-19) [umoeller]
     */

    if (pGlobalSettings->fNumLockStartup)
        winhSetNumLock(TRUE);

    /*
     *  initialize threads
     *
     */

    xthrStartThreads();

    /*
     *  start XWorkplace daemon (XWPDAEMN.EXE)
     *
     */

    {
        // check for the DAEMONSHARED structure, which
        // is used for communication between the daemon
        // and XFLDR.DLL.
        // We take advantage of the fact that OS/2 keeps
        // reference of the processes which allocate or
        // request access to a block of shared memory.
        // The DAEMONSHARED struct is allocated here
        // (just below) and requested by the daemon.
        // -- If requesting the shared memory works at this point,
        //    this means that the daemon is still running!
        //    This happens after a WPS restart. We'll then
        //    skip the rest.
        // -- If requesting the shared memory fails, this means
        //    that the daemon is _not_ running (the WPS is started
        //    for the first time). We then allocate the shared
        //    memory and start the daemon, which in turn requests
        //    this shared memory block.

        PDAEMONSHARED pDaemonShared = 0;
        APIRET arc = DosGetNamedSharedMem((PVOID*)&pDaemonShared,
                                          IDSHMEM_DAEMON,
                                          PAG_READ | PAG_WRITE);

        #ifdef DEBUG_STARTUP
            _Pmpf(("xwpinit: DosGetSharedMem arc = %d", arc));
        #endif

        if (arc != NO_ERROR)
        {
            BOOL    fDaemonStarted = FALSE;

            // shared mem does not exist:
            // --> daemon not running; probably first WPS
            // startup, so we allocate the shared mem now and
            // start the XWorkplace daemon
            arc = DosAllocSharedMem((PVOID*)&pDaemonShared,
                                    IDSHMEM_DAEMON,
                                    sizeof(DAEMONSHARED), // rounded up to 4KB
                                    PAG_COMMIT | PAG_READ | PAG_WRITE);

            #ifdef DEBUG_STARTUP
                _Pmpf(("xwpinit: DosAllocSharedMem arc = %d", arc));
            #endif

            if (arc != NO_ERROR)
                DebugBox("XWorkplace",
                         "Error allocating shared memory for daemon.");
            else
            {
                CHAR    szDir[CCHMAXPATH],
                        szExe[CCHMAXPATH];

                // shared mem successfully allocated:
                memset(pDaemonShared, 0, sizeof(DAEMONSHARED));
                // store the thread-1 object window, which
                // gets messages from the daemon
                pDaemonShared->hwndThread1Object = ThreadGlobals.hwndThread1Object;
                pDaemonShared->ulWPSStartupCount = 1;
                // at the first WPS start, always process startup folder
                pDaemonShared->fProcessStartupFolder = TRUE;

                // now start the daemon;
                // we need to specify XWorkplace's "BIN"
                // subdir as the working dir, because otherwise
                // the daemon won't find XWPHOOK.DLL.

                // compose paths
                if (cmnQueryXFolderBasePath(szDir))
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

                    ThreadGlobals.happDaemon = WinStartApp(ThreadGlobals.hwndThread1Object, // hwndNotify,
                                                           &pd,
                                                           "-D", // params; otherwise the daemon
                                                                 // displays a msg box
                                                           NULL,
                                                           0);// no SAF_INSTALLEDCMDLINE,
                    if (ThreadGlobals.happDaemon)
                        // success:
                        fDaemonStarted = TRUE;
                }
            } // end if DosAllocSharedMem

            if (!fDaemonStarted)
                // error:
                DebugBox("XWorkplace",
                         "The XWorkplace daemon (XWPDAEMON.EXE) could not be started.");
        } // end if DosGetNamedSharedMem
        else
        {
            // shared memory block already exists:
            // this means the daemon is already running
            // and we have a WPS restart

            // store new thread-1 object wnd
            pDaemonShared->hwndThread1Object = ThreadGlobals.hwndThread1Object;
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

        ThreadGlobals.pDaemonShared = pDaemonShared;
    }

    /*
     *  check WPS archiving (V0.9.0)
     *
     */

    if (pGlobalSettings->fReplaceArchiving)
        // check whether we need a WPS backup (archives.c)
        arcCheckIfBackupNeeded(ThreadGlobals.hwndThread1Object,
                               T1M_DESTROYARCHIVESTATUS);
}


