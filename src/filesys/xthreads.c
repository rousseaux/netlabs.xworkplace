
/*
 *@@sourcefile xthreads.c:
 *      this file has the additional XWorkplace threads
 *      to offload tasks to the background.
 *
 *      Currently, there are three additional threads which
 *      are started by xthrStartThreads upon WPS startup
 *      and are _always_ running:
 *
 *      --  XFolder "Worker" thread / object window (fntWorkerThread, fnwpWorkerObject),
 *          running with Idle priority (which is now configurable, V0.9.0);
 *      --  XFolder "File" thread / object window (fntFileThread, fnwpFileObject;
 *          new with V0.9.0), running at regular priority;
 *      --  XFolder "Speedy" thread / object window (fntSpeedyThread, fnwpSpeedyObject),
 *          running at maximum regular priority.
 *
 *      Also, we have functions to communicate with these threads.
 *
 *      Function prefix for this file:
 *      --  xthr*
 *
 *@@header "filesys\xthreads.h"
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
 *  7)  headers in filesys\ (as needed)
 *  8)  #pragma hdrstop and then more SOM headers which crash with precompiled headers
 */

#define INCL_DOSSESMGR
#define INCL_DOSEXCEPTIONS
#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#define INCL_DOSMISC
#define INCL_DOSERRORS

#define INCL_WINWINDOWMGR
#define INCL_WINMESSAGEMGR
#define INCL_WINDIALOGS
#define INCL_WINTIMER
#define INCL_WINSHELLDATA
#define INCL_WINSTDCNR

#define INCL_GPIBITMAPS
#include <os2.h>

// C library headers
#include <stdio.h>              // needed for except.h
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h
#include <io.h>                 // access etc.

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\animate.h"            // icon and other animations
#include "helpers\cnrh.h"               // container helper routines
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\eah.h"                // extended attributes helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\gpih.h"               // GPI helper routines
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\shapewin.h"           // shaped windows helper functions
#include "helpers\stringh.h"            // string helper routines
#include "helpers\syssound.h"           // system sound helper routines
#include "helpers\threads.h"            // thread helpers
#include "helpers\winh.h"               // PM helper routines

#pragma hdrstop                 // VAC++ keeps crashing otherwise
// SOM headers which don't crash with prec. header files
#include "xfldr.h"

// headers in /folder
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

// headers in /hook
#include "hook\xwphook.h"

#include "filesys\fileops.h"            // file operations implementation
#include "filesys\folder.h"             // XFolder implementation
#include "filesys\xthreads.h"           // extra XWorkplace threads

#include "config\hookintf.h"             // daemon/hook interface

// headers in /hook
#include "hook\xwphook.h"

// other SOM headers
#include <wpshadow.h>                   // WPShadow
// #include <wpdesk.h>                     // WPDesktop
#include "classes\xtrash.h"             // XWPTrashCan
#include "classes\xtrashobj.h"             // XWPTrashCan
#include "filesys\trash.h"              // trash can implementation

/* ******************************************************************
 *                                                                  *
 *   Global variables                                               *
 *                                                                  *
 ********************************************************************/

// currently waiting messages for Worker thread;
// if this gets too large, its priority will be raised
HAB                 G_habWorkerThread = NULLHANDLE;
HMQ                 G_hmqWorkerThread = NULLHANDLE;
// flags for whether the Worker thread owns semaphores
// BOOL                G_fWorkerAwakeObjectsSemOwned = FALSE;

// Speedy thread
HAB                 G_habSpeedyThread = NULLHANDLE;
HMQ                 G_hmqSpeedyThread = NULLHANDLE;
CHAR                G_szBootupStatus[256];
HWND                G_hwndBootupStatus = NULLHANDLE;

// SOUND.DLL module handle
// HMODULE             G_hmodSoundDLL;
// imported functions (see sounddll.h)
// PFN_SNDOPENSOUND    G_psndOpenSound;
// PFN_SNDPLAYSOUND    G_psndPlaySound;
// PFN_SNDSTOPSOUND    G_psndStopSound;

// File thread
HAB                 G_habFileThread = NULLHANDLE;
HMQ                 G_hmqFileThread = NULLHANDLE;
ULONG               G_CurFileThreadMsg = 0;
            // current message that File thread is processing,
            // or null if none

/* ******************************************************************
 *                                                                  *
 *   here come the XFolder Worker thread functions                  *
 *                                                                  *
 ********************************************************************/

/*
 *@@ xthrResetWorkerThreadPriority:
 *      this resets the Worker thread priority
 *      to either maximum regular (if we're
 *      currently in "high priority" mode,
 *      which only happens if too many
 *      messages have piled up) or to what
 *      was specified in the global settings
 *      for "default priority" mode (XWPConfig).
 *
 *      This gets called from RaiseWorkerThreadPriority
 *      and from fncbXWCParanoiaItemChanged (xwpsetup.c).
 *
 *@@changed V0.9.0 [umoeller]: added beeping
 */

VOID xthrResetWorkerThreadPriority(VOID)
{
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    PCKERNELGLOBALS pKernelGlobals = krnQueryGlobals();
    ULONG   ulPrty, ulDelta;

    if (pKernelGlobals->WorkerThreadHighPriority)
    {
        // high priority
        ulPrty = PRTYC_REGULAR;
        ulDelta = +31;

        if (pGlobalSettings->fWorkerPriorityBeep)
            DosBeep(1200, 30);
    }
    else
    {
        // default priority
        switch (pGlobalSettings->bDefaultWorkerThreadPriority)
        {
            case 0:     // 0: idle +/-0
                ulPrty = PRTYC_IDLETIME;
                ulDelta = 0;
            break;

            case 2:     // 2: regular +/-0
                ulPrty = PRTYC_REGULAR;
                ulDelta = 0;
            break;

            default:    // 1: idle +31
                ulPrty = PRTYC_IDLETIME;
                ulDelta = +31;
                break;
        }

        if (pGlobalSettings->fWorkerPriorityBeep)
            DosBeep(1000, 30);
    }
    DosSetPriority(PRTYS_THREAD,
                   ulPrty,
                   ulDelta,
                   thrQueryID(&pKernelGlobals->tiWorkerThread));
}

/*
 *@@ RaiseWorkerThreadPriority:
 *      depending on fRaise, raise or lower Worker
 *      thread prty.
 *
 *      This gets called automatically if too many
 *      messages have piled up for fnwpWorkerObject.
 *
 *@@changed V0.9.0 [umoeller]: avoiding duplicate sets now
 */

BOOL RaiseWorkerThreadPriority(BOOL fRaise)
{
    BOOL        brc = FALSE;
    static BOOL fFirstTime = TRUE;

    PKERNELGLOBALS pKernelGlobals = NULL;

    ULONG ulNesting;
    DosEnterMustComplete(&ulNesting);

    TRY_LOUD(excpt1)
    {
        pKernelGlobals = krnLockGlobals(__FILE__, __LINE__, __FUNCTION__);
        if (pKernelGlobals)
        {
            if (    (fFirstTime)
                 || (fRaise != pKernelGlobals->WorkerThreadHighPriority)
               )
            {
                pKernelGlobals->WorkerThreadHighPriority = fRaise;
                xthrResetWorkerThreadPriority();
                fFirstTime = FALSE;
            }
        }
    }
    CATCH(excpt1) {} END_CATCH();

    if (pKernelGlobals)
        krnUnlockGlobals();

    DosExitMustComplete(&ulNesting);

    return (brc);
}

/*
 *@@ xthrPostWorkerMsg:
 *      this posts a msg to the XFolder Worker thread object window
 *      (fntWorkerThread, fnwpWorkerObject)
 *      and controls that thread's priority at the same time; use this
 *      function instead of WinPostMsg, cos otherwise the
 *      Worker thread gets confused.
 *
 *@@changed V0.9.3 (2000-04-26) [umoeller]: changed kernel locks
 *@@changed V0.9.7 (2000-12-13) [umoeller]: fixed sync problems
 */

BOOL xthrPostWorkerMsg(ULONG msg, MPARAM mp1, MPARAM mp2)
{
    BOOL brc = FALSE;

    PKERNELGLOBALS pKernelGlobals = NULL;

    ULONG ulNesting;
    DosEnterMustComplete(&ulNesting);

    TRY_LOUD(excpt1)
    {
        pKernelGlobals = krnLockGlobals(__FILE__, __LINE__, __FUNCTION__);
        if (pKernelGlobals)
        {
            if (thrQueryID(&pKernelGlobals->tiWorkerThread))
            {
                if (pKernelGlobals->hwndWorkerObject)
                {
                    brc = WinPostMsg(pKernelGlobals->hwndWorkerObject, msg, mp1, mp2);

                    if (brc)
                    {
                        pKernelGlobals->ulWorkerMsgCount++;
                        if (pKernelGlobals->ulWorkerMsgCount > 300)
                            // if the Worker thread msg queue gets congested,
                            // boost priority
                            RaiseWorkerThreadPriority(TRUE);
                    }
                }
            }
        }
    }
    CATCH(excpt1)
    {
        brc = FALSE;
    } END_CATCH();

    if (pKernelGlobals)
        krnUnlockGlobals();

    DosExitMustComplete(&ulNesting);

    return (brc);
}

/*
 *@@ fnwpGenericStatus:
 *      dlg func for bootup status wnd and maybe others.
 */

MRESULT EXPENTRY fnwpGenericStatus(HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = NULL;

    switch (msg)
    {
        case WM_INITDLG:
            mrc = (MPARAM)TRUE; // focus change flag;
            // mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
        break;

        default:
            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
        break;
    }

    return (mrc);
}

/*
 *@@ fnwpWorkerObject:
 *      wnd proc for Worker thread object window
 *      (see fntWorkerThread below).
 *
 *@@changed V0.9.0 [umoeller]: moved some features to File thread
 *@@changed V0.9.0 [umoeller]: adjust WOM_PROCESSORDEREDCONTENT for new XFolder::xwpBeginEnumContent functions
 *@@changed V0.9.0 [umoeller]: removed WM_INVALIDATEORDEREDCONTENT
 *@@changed V0.9.0 [umoeller]: WOM_ADDAWAKEOBJECT is now storing plain WPObject pointers (no more OBJECTLISTITEM)
 *@@changed V0.9.3 (2000-04-28) [umoeller]: now pre-resolving wpQueryContent for speed
 *@@changed V0.9.7 (2000-12-08) [umoeller]: fixed crash when kernel globals weren't returned
 *@@changed V0.9.7 (2000-12-08) [umoeller]: got rid of dtGetULongTime
 */

MRESULT EXPENTRY fnwpWorkerObject(HWND hwndObject, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = NULL;

    switch (msg)
    {
        /*
         * WM_CREATE:
         *      set a timer to periodically free storage
         */

        case WM_CREATE:
        {
            mrc = WinDefWindowProc(hwndObject, msg, mp1, mp2);
            WinStartTimer(G_habWorkerThread,
                          hwndObject,
                          2,          // id
                          5*60*1000); // every five minutes
        break; }

        /*
         * WM_TIMER:
         *      timer 2 is started in WM_CREATE to run every
         *      five minutes. This returns unused memory
         *      back to the system and reloads the country
         *      settings which we have cached (V0.9.6 (2000-11-12) [umoeller]).
         */

        case WM_TIMER:
        {
            switch ((USHORT)mp1)   // timer id
            {
                // timer 2: free unused memory
                case 2:
                    #ifdef DEBUG_MEMORYBEEP
                        DosBeep(3000, 20);
                    #endif

                    #if (__IBMC__ >= 300)
                        _heapmin();
                        // _heapmin returns all unused memory from the default
                        // runtime heap to the operating system;
                        // this is VAC++3.0-specific
                    #endif

                    #ifdef DEBUG_MEMORYBEEP
                        DosBeep(3500, 20);
                    #endif

                    // reload country settings
                    cmnQueryCountrySettings(TRUE);
                break;
            }
        break; }

        /*
         * WOM_WELCOME:
         *     display hello dlg box
         */

        /* case WOM_WELCOME:
        {
            BOOL fDone = FALSE;
            if (doshIsWarp4())
            {
                // on Warp 4, open the SmartGuide window
                // which was created by the install script
                HOBJECT hobjIntro = WinQueryObject(XFOLDER_INTROID);
                if (hobjIntro)
                {
                    WinOpenObject(hobjIntro, OPEN_DEFAULT, TRUE);
                    fDone = TRUE;
                }
            }

            if (!fDone)
            {
                // SmartGuide object not found (Warp 3):
                // display simple dialog
                winhCenteredDlgBox(HWND_DESKTOP, HWND_DESKTOP,
                                   WinDefDlgProc,
                                   cmnQueryNLSModuleHandle(FALSE),
                                   ID_XFD_WELCOME,
                                   0);
            }
        break; } */

        /*
         * WOM_QUICKOPEN:
         *      this is posted by the XFolder Object window
         *      on the main PM thread for each folder with
         *      the QuickOpen flag on: we will populate it
         *      here and load all the icons.
         *      Parameters:
         *          XFolder* mp1    folder pointer
         *          ULONG    mp2    callback function (PFNWP)
         *      The callback will be called if mp2 != NULL
         *      with the following parameters:
         *          HWND    current folder
         *          ULONG   current object therein
         *          MPARAM  current object count
         *          MPARAM  maximum object count
         */

        case WOM_QUICKOPEN:
        {
            XFolder *pFolder = (XFolder*)mp1;
            PFNWP   pfncb = (PFNWP)mp2;
            BOOL    fContinue = FALSE;

            #ifdef DEBUG_STARTUP
                _Pmpf(("WOM_QUICKOPEN %lX", mp1));
            #endif

            if (pFolder)
            {
                fContinue = fdrQuickOpen(pFolder,
                                         pfncb);

                if (fContinue)
                    // now go for the next folder
                    krnPostThread1ObjectMsg(T1M_NEXTQUICKOPEN, mp1, MPNULL);
                    break;
            }

            krnPostThread1ObjectMsg(T1M_NEXTQUICKOPEN, NULL, MPNULL);
        break; }

        /*
         * WOM_PROCESSORDEREDCONTENT:
         *      this msg is posted by xfProcessOrderedContent to
         *      begin working our way through the contents of a
         *      folder; from here we will call a callback func
         *      which has been specified with xfProcessOrderedContent.
         *      Msg params:
         *          XFolder *mp1    folder to process
         *          PPROCESSCONTENTINFO
         *                  *mp2    structure with process info,
         *                          composed by xfProcessOrderedContent
         */

        case WOM_PROCESSORDEREDCONTENT:
        {
            XFolder             *pFolder = (XFolder*)mp1;
            PPROCESSCONTENTINFO pPCI = (PPROCESSCONTENTINFO)mp2;

            #ifdef DEBUG_STARTUP
                _Pmpf(("Entering WOM_PROCESSORDEREDCONTENT..."));
            #endif

            if (wpshCheckObject(pFolder))
            {
                if (pPCI)
                {
                    if (pPCI->ulObjectNow == 0)
                    {
                        // first call: initialize structure
                        pPCI->ulObjectMax = 0;
                        wpshCheckIfPopulated(pFolder,
                                             FALSE);        // full populate
                        // now count objects
                        for (   pPCI->pObject = _wpQueryContent(pFolder, NULL, QC_FIRST);
                                (pPCI->pObject);
                                pPCI->pObject = _wpQueryContent(pFolder, pPCI->pObject, QC_NEXT)
                            )
                        {
                            pPCI->ulObjectMax++;
                        }

                        #ifdef DEBUG_STARTUP
                            _Pmpf(("  Found %d objects", pPCI->ulObjectMax));
                        #endif

                        // get first object
                        pPCI->henum = _xwpBeginEnumContent(pFolder);
                        if (pPCI->henum)
                            pPCI->pObject = _xwpEnumNext(pFolder, pPCI->henum);
                    }
                    else
                    {
                        // subsequent calls: get next object
                        pPCI->pObject = _xwpEnumNext(pFolder, pPCI->henum);
                    }

                    // now process that object
                    pPCI->ulObjectNow++;
                    if (pPCI->pObject)
                    {
                        // this is not the last object: start it
                        if (pPCI->pfnwpCallback)
                        {
                            if (wpshCheckObject(pPCI->pObject))
                            {
                                #ifdef DEBUG_STARTUP
                                    _Pmpf(("  Sending T1M_POCCALLBACK"));
                                #endif
                                // the thread-1 object window will
                                // then call the callback on thread one
                                krnSendThread1ObjectMsg(T1M_POCCALLBACK,
                                                         (MPARAM)pPCI,  // contains the object
                                                         MPNULL);
                            }
                        }
                        DosQuerySysInfo(QSV_MS_COUNT, QSV_MS_COUNT,
                                        &pPCI->ulFirstTime,
                                        sizeof(pPCI->ulFirstTime));

                        // wait for next object
                        xthrPostWorkerMsg(WOM_WAITFORPROCESSNEXT,
                                          mp1, mp2);
                    }
                    else
                    {
                        // pPCI->pObject == NULL:
                        // no more objects, call callback with NULL, clean up and stop
                        if (pPCI->pfnwpCallback)
                        {
                            #ifdef DEBUG_STARTUP
                                _Pmpf(("  Sending T1M_POCCALLBACK, pObject == NULL"));
                            #endif
                            krnSendThread1ObjectMsg(T1M_POCCALLBACK,
                                                     (MPARAM)pPCI,  // pObject == NULL now
                                                     MPNULL);
                        }

                        _xwpEndEnumContent(pFolder, pPCI->henum);
                        free(pPCI);
                    }
                }
            }
            #ifdef DEBUG_STARTUP
                _Pmpf(("  Done with WOM_PROCESSORDEREDCONTENT"));
            #endif

        break; }

        case WOM_WAITFORPROCESSNEXT:
        {
            PPROCESSCONTENTINFO pPCI = (PPROCESSCONTENTINFO)mp2;
            BOOL                OKGetNext = FALSE;

            if (pPCI)
            {
                if (!pPCI->fCancelled)
                {
                    if (pPCI->ulTiming == 0)
                    {
                        // "wait for close" mode
                        if (_wpWaitForClose(pPCI->pObject,
                                            pPCI->hwndView,
                                            0,
                                            SEM_IMMEDIATE_RETURN,
                                            FALSE) == 0)
                            OKGetNext = TRUE;
                    }
                    else
                    {
                        // timing mode
                        ULONG ulNowTime;
                        DosQuerySysInfo(QSV_MS_COUNT, QSV_MS_COUNT,
                                        &ulNowTime,
                                        sizeof(ulNowTime));
                        if (ulNowTime > (pPCI->ulFirstTime + pPCI->ulTiming))
                            OKGetNext = TRUE;
                    }

                    if (OKGetNext)
                    {
                        PKERNELGLOBALS pKernelGlobals = krnLockGlobals(__FILE__, __LINE__, __FUNCTION__);
                        if (pKernelGlobals)
                        {
                            // ready to go for next object:
                            // make sure the damn PM hard error windows are not visible,
                            // because this locks any other PM activity
                            if (    (WinIsWindowVisible(pKernelGlobals->hwndHardError))
                                 || (WinIsWindowVisible(pKernelGlobals->hwndSysError))
                               )
                            {
                                DosBeep(250, 100);
                                // wait a little more and try again
                                OKGetNext = FALSE;
                            }

                            krnUnlockGlobals();
                        }
                    }

                    if (OKGetNext)
                        xthrPostWorkerMsg(WOM_PROCESSORDEREDCONTENT, mp1, mp2);
                    else
                    {
                        // not ready yet:
                        // sleep awhile
                        DosSleep(100);
                        // and repost
                        xthrPostWorkerMsg(WOM_WAITFORPROCESSNEXT, mp1, mp2);
                    }
                } // end if (!pPCI->fCancelled)
                else
                {
                    // cancelled: notify that we're done (V0.9.0)
                    pPCI->pObject = NULL;
                    krnSendThread1ObjectMsg(T1M_POCCALLBACK,
                                            (MPARAM)pPCI, MPNULL);
                    free(pPCI);
                }
            }
        break; }

        /*
         * WOM_ADDAWAKEOBJECT:
         *      this is posted by XFldObject for each
         *      object that is awaked by the WPS; we
         *      need to maintain a list of these objects
         *      for XShutdown.
         *
         *      Parameters:
         *          WPObject* mp1: somSelf as in XFldObject::wpObjectReady
         */

        case WOM_ADDAWAKEOBJECT:
        {
            WPObject        *pObj2Store = (WPObject*)(mp1);
            PKERNELGLOBALS  pKernelGlobals = NULL;
            BOOL            fWorkerAwakeObjectsSemOwned = FALSE;

            ULONG           ulNesting = 0;
            DosEnterMustComplete(&ulNesting);

            TRY_LOUD(excpt3)  // V0.9.7 (2000-12-13) [umoeller]
            {
                if (pKernelGlobals = krnLockGlobals(__FILE__, __LINE__, __FUNCTION__))
                {
                    #ifdef DEBUG_AWAKEOBJECTS
                       // _PmpfF(("WT: Adding awake object..."));
                    #endif

                    // set the quiet exception handler, because
                    // sometimes we get a message for an object too
                    // late, i.e. it is not awake any more, and then
                    // we'll trap
                    TRY_QUIET(excpt2)
                    {
                        // only save awake abstract and folder objects;
                        // if we included all WPFileSystem objects, all
                        // of these will be saved at XShutdown, that is,
                        // they'll all get .CLASSINFO EAs, which we don't
                        // want
                        if (    (_somIsA(pObj2Store, _WPAbstract))
                             // || (_somIsA(pObj2Store, _WPFolder))
                           )
                        {
                            if (strcmp(_somGetClassName(pObj2Store), "SmartCenter") == 0)
                                pKernelGlobals->pAwakeWarpCenter = pObj2Store;

                            // get the mutex semaphore
                            fWorkerAwakeObjectsSemOwned
                                  = (WinRequestMutexSem(pKernelGlobals->hmtxAwakeObjects, 4000)
                                     == NO_ERROR);

                            if (fWorkerAwakeObjectsSemOwned)
                            {
                                PLINKLIST pllAwakeObjects = (PLINKLIST)(pKernelGlobals->pllAwakeObjects);

                                #ifdef DEBUG_AWAKEOBJECTS
                                    _Pmpf(("WT: Storing 0x%lX (%s)", pObj2Store, _wpQueryTitle(pObj2Store)));
                                #endif

                                // check if this object is stored already
                                if (lstNodeFromItem(pllAwakeObjects, pObj2Store) == NULL)
                                {
                                    // object not stored yet: do it now
                                    #ifdef DEBUG_AWAKEOBJECTS
                                        _Pmpf(("WT:lstAppendItem rc: %d",
                                               lstAppendItem(pllAwakeObjects,
                                                             pObj2Store)));
                                    #else
                                        lstAppendItem(pllAwakeObjects,
                                                      pObj2Store);
                                    #endif

                                    // increment global count
                                    pKernelGlobals->lAwakeObjectsCount++;

                                }
                                #ifdef DEBUG_AWAKEOBJECTS
                                    else
                                        _Pmpf(("WT: Item is already on list"));
                                #endif
                            }
                        }
                    }
                    CATCH(excpt2)
                    {
                        // the thread exception handler puts us here
                        // if an exception occured:
                        #ifdef DEBUG_AWAKEOBJECTS
                            DosBeep(10000, 10);
                        #endif
                    } END_CATCH();
                } // end if (pKernelGlobals)
            }
            CATCH(excpt3) {} END_CATCH();

            if (pKernelGlobals)
            {
                if (fWorkerAwakeObjectsSemOwned)
                {
                    DosReleaseMutexSem(pKernelGlobals->hmtxAwakeObjects);
                    fWorkerAwakeObjectsSemOwned = FALSE;
                }

                krnUnlockGlobals();
            }

            DosExitMustComplete(&ulNesting);
        break; }

        /*
         * WOM_REMOVEAWAKEOBJECT:
         *      this is posted by WPObject also, but
         *      when an object goes back to sleep.
         *      Be careful: the object pointer in mp1
         *      does not point to a valid SOM object
         *      any more, because the object has
         *      already been freed in memory; so we
         *      must not call any methods here.
         *      We only use the object pointer
         *      for finding the respective object
         *      in the linked list.
         *
         *      Parameters:
         *          WPObject* mp1: somSelf as in XFldObject::wpUnInitData
         */

        case WOM_REMOVEAWAKEOBJECT:
        {
            WPObject        *pObj = (WPObject*)(mp1);
            PKERNELGLOBALS  pKernelGlobals = NULL;
            BOOL            fWorkerAwakeObjectsSemOwned = FALSE;

            ULONG           ulNesting = 0;
            DosEnterMustComplete(&ulNesting);

            #ifdef DEBUG_AWAKEOBJECTS
                _Pmpf(("WT: Removing asleep object, mp1: 0x%lX", mp1));
            #endif

            TRY_QUIET(excpt2)
            {
                if (pKernelGlobals = krnLockGlobals(__FILE__, __LINE__, __FUNCTION__))
                {
                    // get the mutex semaphore
                    fWorkerAwakeObjectsSemOwned =
                            (WinRequestMutexSem(pKernelGlobals->hmtxAwakeObjects, 4000)
                            == NO_ERROR);
                    if (fWorkerAwakeObjectsSemOwned)
                    {
                        // remove the object from the list
                        if (pObj)
                        {
                            #ifdef DEBUG_AWAKEOBJECTS
                                _Pmpf(("WT: Calling lstRemoveItem with poli = 0x%lX",
                                       pObj));
                            #endif
                            #ifdef DEBUG_AWAKEOBJECTS
                                _Pmpf(("WT: lstRemoveItem rc: %d",
                                       lstRemoveItem(pKernelGlobals->pllAwakeObjects,
                                                     pObj)));
                            #else
                                lstRemoveItem(pKernelGlobals->pllAwakeObjects,
                                              pObj);
                            #endif

                            // decrement global count
                            pKernelGlobals->lAwakeObjectsCount--;
                        }
                    }
                }
            }
            CATCH(excpt2) {} END_CATCH();

            if (pKernelGlobals)
            {
                if (fWorkerAwakeObjectsSemOwned)
                {
                    DosReleaseMutexSem(pKernelGlobals->hmtxAwakeObjects);
                    fWorkerAwakeObjectsSemOwned = FALSE;
                }

                krnUnlockGlobals();
            }

            DosExitMustComplete(&ulNesting);
        break; }

        /*
         * WOM_REFRESHFOLDERVIEWS:
         *    this one is posted by XFolder's overrides of wpMoveObject,
         *    wpSetTitle or wpRefresh with the calling instance's somSelf
         *    in mp1; we will now update the open frame window titles of both
         *    the caller and its open subfolders with the full folder path
         *    Parameters:
         *    XFolder* mp1  folder to update; if this is NULL, all open
         *                  folders on the system will be updated
         */

        case WOM_REFRESHFOLDERVIEWS:
        {
            XFolder     *pFolder = NULL,
                        *pCalling = (XFolder*)(mp1);

            for ( pFolder = _wpclsQueryOpenFolders(_WPFolder, NULL, QC_FIRST, FALSE);
                  pFolder;
                  pFolder = _wpclsQueryOpenFolders(_WPFolder, pFolder, QC_NEXT, FALSE))
            {
                if (wpshCheckObject(pFolder))
                    if (_somIsA(pFolder, _WPFolder))
                    {
                        if (pCalling == NULL)
                            fdrUpdateAllFrameWndTitles(pFolder);
                        else if (wpshResidesBelow(pFolder, pCalling))
                            fdrUpdateAllFrameWndTitles(pFolder);
                    }
            }
        break; }

        /*
         * WOM_UPDATEALLSTATUSBARS:
         *      goes thru all open folder views and updates
         *      status bars if necessary.
         *      Parameters:
         *      ULONG mp1   do-what flag
         *                  1:  show or hide status bars according
         *                      to folder instance and Global settings
         *                  2:  reformat status bars (e.g. because
         *                      fonts have changed)
         */

        case WOM_UPDATEALLSTATUSBARS:
        {
            #ifdef DEBUG_STATUSBARS
                _Pmpf(( "WT: WOM_UPDATEALLSTATUSBARS" ));
            #endif

            // for each open folder view, call the callback
            // which updates the status bars
            // (fncbUpdateStatusBars in folder.c)
            fdrForEachOpenGlobalView((ULONG)mp1,
                                     (PFNWP)fncbUpdateStatusBars);
        break; }

        /*
         * WOM_DELETEICONPOSEA:
         *
         */

        case WOM_DELETEICONPOSEA:
        {
            /* this msg is posted when the .ICONPOS EAs are
               do be deleted; we better do this in the
               background thread */
            EABINDING   eab;
            XFolder     *pFolder = (XFolder*)mp1;
            CHAR        szPath[CCHMAXPATH];

            if (!wpshCheckObject(pFolder))
                break;

            _wpQueryFilename(pFolder, szPath, TRUE); // fully qualfd.
            eab.bFlags = 0;
            eab.pszName = ".ICONPOS";
            eab.bNameLength = strlen(eab.pszName);
            eab.pszValue = "";
            eab.usValueLength = 0;
            eaPathWriteOne(szPath, &eab);
        break; }

        /*
         * WOM_DELETEFOLDERPOS:
         *      (new with V0.82) this is posted by
         *      XFolder::wpFree (i.e. when a folder
         *      is deleted); we will now search the
         *      OS2.INI file for folderpos entries
         *      for the deleted folder and remove
         *      them. This is only posted if the
         *      corresponding setting in "Workplace
         *      Shell" is on.
         *      Parameters: mp1 contains the
         *          (hex) five-digit handle, which
         *          should always be 0x3xxxx.
         */

        case WOM_DELETEFOLDERPOS:
        {
            // according to Henk Kelder:
            // for each open folder the WPS creates an INI entry, which
            // key is the decimal value of the HOBJECT, followed by
            // some wicked "@" codes, which probably stand for the
            // various folder views. Since I don't know the codes,
            // I'm getting the keys list for PM_Workplace:FolderPos.

            PSZ pszFolderPosKeys = prfhQueryKeysForApp(HINI_USER,
                                                       WPINIAPP_FOLDERPOS);

            if (pszFolderPosKeys)
            {
                PSZ     pKey2 = pszFolderPosKeys;
                CHAR    szComp[20];
                ULONG   cbComp;
                sprintf(szComp, "%d@", (ULONG)mp1);
                cbComp = strlen(szComp);

                #ifdef DEBUG_CNRCONTENT
                    _Pmpf(("Checking for folderpos %s", szComp));
                #endif

                // now walk thru all the keys in the folderpos
                // application and check if it's one for our
                // folder; if so, delete it
                while (*pKey2 != 0)
                {
                    if (memcmp(szComp, pKey2, cbComp) == 0)
                    {
                        PrfWriteProfileData(HINI_USER,
                                            (PSZ)WPINIAPP_FOLDERPOS, pKey2,
                                            NULL, 0);
                        #ifdef DEBUG_CNRCONTENT
                            _Pmpf(("  Deleted %s", pKey2));
                        #endif
                    }

                    pKey2 += strlen(pKey2)+1;
                }
                free(pszFolderPosKeys);
            }
        break; }

        /*
         *@@ WOM_STOREGLOBALSETTINGS:
         *      writes the GLOBALSETTINGS structure back to
         *      OS2.INI. This gets posted from cmnStoreGlobalSettings.
         *
         *      Parameters: none.
         *
         *@@changed V0.9.4 (2000-06-16) [umoeller]: moved this to Worker thread from File thread
         */

        case WOM_STOREGLOBALSETTINGS:
        {
            GLOBALSETTINGS *pGlobalSettings = NULL;
            ULONG ulNesting;
            DosEnterMustComplete(&ulNesting);
            TRY_LOUD(excpt1)
            {
                pGlobalSettings = cmnLockGlobalSettings(__FILE__, __LINE__, __FUNCTION__);
                if (pGlobalSettings)
                {
                    PrfWriteProfileData(HINI_USERPROFILE,
                                        (PSZ)INIAPP_XWORKPLACE,
                                        (PSZ)INIKEY_GLOBALSETTINGS,
                                        pGlobalSettings, sizeof(GLOBALSETTINGS));
                }
                else
                    cmnLog(__FILE__, __LINE__, __FUNCTION__,
                           "Unable to lock GLOBALSETTINGS.");
            }
            CATCH(excpt1) {} END_CATCH();
            if (pGlobalSettings)
                cmnUnlockGlobalSettings();
            DosExitMustComplete(&ulNesting);
        break; }

        #ifdef __DEBUG__
        case XM_CRASH:          // posted by debugging context menu of XFldDesktop
            CRASH;
        break;
        #endif

        default:
            mrc = WinDefWindowProc(hwndObject, msg, mp1, mp2);

    } // end switch

    return (mrc);
}

/*
 *xthrOnKillWorkerThread:
 *      thread "exit list" func registered with
 *      the TRY_xxx macros (helpers\except.c).
 *      In case the Worker thread gets killed,
 *      this function gets called. As opposed to
 *      real exit list functions, which always get
 *      called on thread 1, this gets called on
 *      the thread which registered the exception
 *      handler.
 *
 *      We release mutex semaphores here, which we
 *      must do, because otherwise the system might
 *      hang if another thread is waiting on the
 *      same semaphore.
 *
 *added V0.9.0 [umoeller]
 *removed V0.9.7 (2000-12-13) [umoeller]
 */

/* VOID APIENTRY xthrOnKillWorkerThread(PEXCEPTIONREGISTRATIONRECORD2 pRegRec2)
{
    if (G_fWorkerAwakeObjectsSemOwned)
    {
        PCKERNELGLOBALS  pKernelGlobals = krnQueryGlobals();
        DosReleaseMutexSem(pKernelGlobals->hmtxAwakeObjects);
        G_fWorkerAwakeObjectsSemOwned = FALSE;
    }
    krnUnlockGlobals(); // just to make sure
} */

/*
 *@@ fntWorkerThread:
 *          this is the thread function for the XFolder
 *          "Worker" (= background) thread, to which
 *          messages for tasks are passed which are too
 *          time-consuming to be processed on the
 *          Workplace thread; it creates an object window
 *          (using fnwpWorkerObject as the window proc)
 *          and stores its handle in KERNELGLOBALS.hwndWorkerObject.
 *          This thread is created by krnInitializeXWorkplace.
 *
 *@@changed V0.9.7 (2000-12-13) [umoeller]: made awake-objects mutex unnamed
 *@@changed V0.9.7 (2000-12-13) [umoeller]: fixed semaphore protection
 */

void _Optlink fntWorkerThread(PTHREADINFO pti)
{
    QMSG            qmsg;
    PSZ             pszErrMsg = NULL;
    BOOL            fTrapped = FALSE;

    // we will now create the mutex semaphore for access to
    // the linked list of awake objects; this is used in
    // wpInitData and wpUnInitData
    {
        PKERNELGLOBALS pKernelGlobals = krnLockGlobals(__FILE__, __LINE__, __FUNCTION__);
        if (pKernelGlobals)
        {
            DosCreateMutexSem(NULL, // "\\sem32\\AwakeObjectsList",
                              &(pKernelGlobals->hmtxAwakeObjects),
                              0,        // unshared
                              FALSE);   // unowned
            krnUnlockGlobals();
        }
    }

    fTrapped = FALSE;

    if (G_habWorkerThread = WinInitialize(0))
    {
        if (G_hmqWorkerThread = WinCreateMsgQueue(G_habWorkerThread, 4000))
        {
            BOOL fExit = FALSE;

            do
            {
                PKERNELGLOBALS pKernelGlobals = NULL;
                HWND hwndWorkerObjectTemp = NULLHANDLE;

                WinCancelShutdown(G_hmqWorkerThread, TRUE);

                WinRegisterClass(G_habWorkerThread,
                                 (PSZ)WNDCLASS_WORKEROBJECT,    // class name
                                 (PFNWP)fnwpWorkerObject,    // Window procedure
                                 0,                  // class style
                                 0);                 // extra window words

                // create Worker object window
                hwndWorkerObjectTemp = winhCreateObjectWindow(WNDCLASS_WORKEROBJECT, NULL);

                if (!hwndWorkerObjectTemp)
                    winhDebugBox(HWND_DESKTOP,
                             "XFolder: Error",
                             "XFolder failed to create the Worker thread object window.");

                {
                    ULONG ulNesting = 0;
                    DosEnterMustComplete(&ulNesting);

                    if (pKernelGlobals = krnLockGlobals(__FILE__, __LINE__, __FUNCTION__))
                    {
                        pKernelGlobals->hwndWorkerObject = hwndWorkerObjectTemp;

                        // set ourselves to idle-time priority; this will
                        //   be raised to regular when the msg queue becomes congested
                        pKernelGlobals->WorkerThreadHighPriority = TRUE;
                                // otherwise the following func won't work right
                        RaiseWorkerThreadPriority(FALSE);

                        krnUnlockGlobals();
                        pKernelGlobals = NULL;
                    }

                    DosExitMustComplete(&ulNesting);
                }

                TRY_LOUD(excpt1)
                {
                    // now enter the message loop
                    while (WinGetMsg(G_habWorkerThread, &qmsg, NULLHANDLE, 0, 0))
                    {
                        pKernelGlobals = krnLockGlobals(__FILE__, __LINE__, __FUNCTION__);
                        if (pKernelGlobals)
                        {
                            if (pKernelGlobals->ulWorkerMsgCount > 0)
                                pKernelGlobals->ulWorkerMsgCount--;

                            if (pKernelGlobals->ulWorkerMsgCount < 10)
                                RaiseWorkerThreadPriority(FALSE);

                            krnUnlockGlobals();
                            pKernelGlobals = NULL;
                        }

                        // dispatch the queue msg
                        WinDispatchMsg(G_habWorkerThread, &qmsg);
                    } // loop until WM_QUIT

                    // WM_QUIT:
                    fExit = TRUE;
                }
                CATCH(excpt1)
                {
                    // the exception handler puts us here if an exception occured:
                    // set flag that exception occured
                    fTrapped = TRUE;
                } END_CATCH();

                /* if (G_fWorkerAwakeObjectsSemOwned)
                {
                    PKERNELGLOBALS pKernelGlobals = krnLockGlobals(5000);
                    // in any case, release mutex semaphores owned by the Worker thread
                    DosReleaseMutexSem(pKernelGlobals->hmtxAwakeObjects);
                    G_fWorkerAwakeObjectsSemOwned = FALSE;
                    krnUnlockGlobals();
                } */

                if (pKernelGlobals)
                    krnUnlockGlobals();

                if (fTrapped)
                    if (pszErrMsg == NULL)
                    {
                        // only report the first error, or otherwise we will
                        // jam the system with msg boxes
                        pszErrMsg = malloc(1000);
                        if (pszErrMsg)
                        {
                            strcpy(pszErrMsg,
                                   "An error occured in the XWorkplace Worker thread. "
                                   "Since the error is probably not serious, "
                                   "the Worker thread will continue to run. "
                                   "However, if errors like these persist, "
                                   "you might want to disable the "
                                   "Worker thread in the \"XWorkplace Setup\" object.");
                            krnPostThread1ObjectMsg(T1M_EXCEPTIONCAUGHT, (MPARAM)pszErrMsg, MPNULL);
                        }
                    }

            } while (!fExit);
        }
    }

    // clean up
    {
        PKERNELGLOBALS pKernelGlobals = krnLockGlobals(__FILE__, __LINE__, __FUNCTION__);
        if (pKernelGlobals)
        {
            WinDestroyWindow(pKernelGlobals->hwndWorkerObject);
            pKernelGlobals->hwndWorkerObject = NULLHANDLE;
            krnUnlockGlobals();
        }

        WinDestroyMsgQueue(G_hmqWorkerThread);
        G_hmqWorkerThread = NULLHANDLE;
        WinTerminate(G_habWorkerThread);
        G_habWorkerThread = NULLHANDLE;
    }
}

/* ******************************************************************
 *                                                                  *
 *   here come the File thread functions                            *
 *                                                                  *
 ********************************************************************/

/*
 *@@ xthrPostFileMsg:
 *      this posts a msg to the File thread object window.
 */

BOOL xthrPostFileMsg(ULONG msg, MPARAM mp1, MPARAM mp2)
{
    BOOL rc = FALSE;
    PCKERNELGLOBALS pKernelGlobals = krnQueryGlobals();

    if (pKernelGlobals)
        if (thrQueryID(&pKernelGlobals->tiFileThread))
        {
            if (pKernelGlobals->hwndFileObject)
            {
                rc = WinPostMsg(pKernelGlobals->hwndFileObject,
                                msg,
                                mp1,
                                mp2);
            }
        }

    if (!rc)
        cmnLog(__FILE__, __LINE__, __FUNCTION__,
               "Failed to post msg = 0x%lX, mp1 = 0x%lX, mp2 = 0x%lX",
               msg, mp1, mp2);

    return (rc);
}

/*
 *@@ xthrIsFileThreadBusy:
 *      this can be called to check whether the
 *      File thread is currently working.
 *      If this returns 0, the File thread is idle.
 *
 *      Otherwise this returns the FIM_* message
 *      number which the file thread is currently
 *      working on.
 *
 *@@added V0.9.2 (2000-02-28) [umoeller]
 */

ULONG xthrIsFileThreadBusy(VOID)
{
    return (G_CurFileThreadMsg);
}

/*
 *@@ CollectDoubleFiles:
 *      implementation for FIM_DOUBLEFILES.
 *      This runs on the File thread.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 */

VOID CollectDoubleFiles(MPARAM mp1)
{
    PDOUBLEFILES pdf = (PDOUBLEFILES)mp1;

    // get first entry (node item is a PSZ with the directory)
    PLISTNODE pNodePath = lstQueryFirstNode(pdf->pllDirectories);

    PLINKLIST pllFilesTemp = lstCreate(TRUE);   // free items

    pdf->pllDoubleFiles = lstCreate(TRUE);   // free items

    while (pNodePath)
    {
        PSZ             pszDirThis = (PSZ)pNodePath->pItemData;
        // collect files of that directory

        HDIR          hdirFindHandle = HDIR_SYSTEM;
        FILEFINDBUF3  ffb3           = {0};      // returned from FindFirst/Next
        ULONG         ulResultBufLen = sizeof(FILEFINDBUF3);
        ULONG         ulFindCount    = 1;        // look for 1 file at a time
        APIRET        arc;

        // skip "." path entries
        if (strcmp(pszDirThis, ".") != 0)
        {
            CHAR    szSearchMask[CCHMAXPATH];
            sprintf(szSearchMask, "%s\\*", pszDirThis);

            arc = DosFindFirst(szSearchMask,         // file pattern
                               &hdirFindHandle,      // directory search handle
                               FILE_NORMAL,          // search attribute
                               &ffb3,                // result buffer
                               ulResultBufLen,       // result buffer length
                               &ulFindCount,         // number of entries to find
                               FIL_STANDARD);        // return level 1 file info

            // keep finding the next file until there are no more files
            while (arc == NO_ERROR)
            {
                // file found:
                PFILELISTITEM pfliNew = malloc(sizeof(FILELISTITEM)),
                              pfliExisting = NULL;
                // search all files we've found so far
                // if we have doubles
                PLISTNODE pNodePrevious = lstQueryFirstNode(pllFilesTemp);
                while (pNodePrevious)
                {
                    PFILELISTITEM pfli = (PFILELISTITEM)pNodePrevious->pItemData;
                    if (stricmp(pfli->szFilename, ffb3.achName) == 0)
                    {
                        pfliExisting = pfli;
                        break;
                    }
                    pNodePrevious = pNodePrevious->pNext;
                }

                // append file to temporary list
                strcpy(pfliNew->szFilename, ffb3.achName);
                pfliNew->pszDirectory = pszDirThis;
                pfliNew->fDate = ffb3.fdateLastWrite;
                pfliNew->fTime = ffb3.ftimeLastWrite;
                pfliNew->ulSize = ffb3.cbFile;
                pfliNew->fProcessed = FALSE;
                lstAppendItem(pllFilesTemp, pfliNew);

                if (pfliExisting)
                {
                    // double item: append copies to doubles list
                    // the copies will not be destroyed

                    PFILELISTITEM pfli2;

                    if (!pfliExisting->fProcessed)
                    {
                        // make copy of existing item for doubles list
                        pfli2 = malloc(sizeof(FILELISTITEM));
                        memcpy(pfli2, pfliExisting, sizeof(FILELISTITEM));
                        lstAppendItem(pdf->pllDoubleFiles, pfli2);
                        pfliExisting->fProcessed = TRUE;
                    }

                    // make copy of new item for doubles list
                    pfli2 = malloc(sizeof(FILELISTITEM));
                    memcpy(pfli2, pfliNew, sizeof(FILELISTITEM));
                    lstAppendItem(pdf->pllDoubleFiles, pfli2);
                }

                ulFindCount = 1;                    // reset find count
                arc = DosFindNext(hdirFindHandle,
                                 &ffb3,             // result buffer
                                 ulResultBufLen,
                                 &ulFindCount);     // number of entries to find

            } // endwhile
        }

        // next dir
        pNodePath = pNodePath->pNext;
    }

    // destroy the temprary list, including all files
    lstFree(pllFilesTemp);

    WinPostMsg(pdf->hwndNotify,
               pdf->ulNotifyMsg,
               (MPARAM)pdf,
               (MPARAM)0);
}

/*
 *@@ fnwpFileObject:
 *      wnd proc for File thread object window
 *      (see fntFileThread below).
 *
 *@@added V0.9.0 [umoeller]
 *@@changed V0.9.3 (2000-04-25) [umoeller]: startup folder was permanently disabled when panic flag was set; fixed
 *@@changed V0.9.3 (2000-04-25) [umoeller]: redid initial desktop open processing
 */

MRESULT EXPENTRY fnwpFileObject(HWND hwndObject, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = NULL;
    // PCKERNELGLOBALS pKernelGlobals = krnQueryGlobals();

    G_CurFileThreadMsg = msg;

    switch (msg)
    {
        /*
         *@@ FIM_DESKTOPPOPULATED:
         *      this msg is posted by XFldDesktop::wpPopulate;
         *      we will now go for the XWorkplace startup
         *      processing.
         *
         *      Parameters:
         *      --  WPDesktop* mp1:     currently active Desktop.
         *      --  HWND mp2:           frame of Desktop just opened.
         *
         *@@added V0.9.3 (2000-04-26) [umoeller]
         *@@changed V0.9.4 (2000-08-02) [umoeller]: initial delay now configurable
         *@@changed V0.9.5 (2000-08-26) [umoeller]: reverted to populate from open...
         */

        case FIM_DESKTOPPOPULATED:
        {
            PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

            #ifdef DEBUG_STARTUP
                _Pmpf(("fnwpFileObject: got FIM_DESKTOPPOPULATED"));
            #endif

            /* if ((_wpQueryFldrFlags((WPFolder*)mp1)
                        & FOI_POPULATEDWITHALL) == 0)
            {
                #ifdef DEBUG_STARTUP
                    _Pmpf(("    Desktop not populated yet, reposting FIM_DESKTOPPOPULATED"));
                #endif

                DosSleep(100);
                // if Desktop is not populated yet, post this
                // msg again

                WinPostMsg(hwndObject, FIM_DESKTOPPOPULATED, mp1, mp2);
                // do not continue
                break;
            } */

            #ifdef DEBUG_STARTUP
                _Pmpf(("    Desktop populated, sleep(1000)"));
            #endif

            // sleep a little while more
            // V0.9.4 (2000-08-02) [umoeller]
            winhSleep(WinQueryAnchorBlock(hwndObject),
                      pGlobalSettings->ulStartupInitialDelay);

            #ifdef DEBUG_STARTUP
                _Pmpf(("    Posting XDM_DESKTOPREADY (0x%lX) to daemon",
                        hwndActiveDesktop));
            #endif

            // notify daemon of WPS desktop window handle
            krnPostDaemonMsg(XDM_DESKTOPREADY,
                             (MPARAM)cmnQueryActiveDesktopHWND(),
                             (MPARAM)0);

            #ifdef DEBUG_STARTUP
                _Pmpf(("    Posting FIM_STARTUP (1) to File thread",
                        hwndActiveDesktop));
            #endif

            // go for startup folder
            xthrPostFileMsg(FIM_STARTUP,
                            (MPARAM)1,  // startup action indicator: first post
                            0); // ignored
        break; }

        /*
         *@@ FIM_STARTUP:
         *      this gets posted in turn by FIM_DESKTOPREADY
         *      initially and several times afterwards with an
         *      increasing value in mp1, depending on which
         *      different startup thingies will be processed.
         *
         *      Parameters:
         *          ULONG mp1: startup action indicator.
         *
         *      Valid values for mp1:
         *      -- 1:   initial posting by FIM_DESKTOPREADY.
         *              This checks for the "just installed" flag
         *              in OS2.INI and creates the config folders
         *              and posts WOM_WELCOME to the Worker thread.
         *      -- 2:   start processing XWorkplace Startup folder.
         *      -- 3:   populate Quick Open folders
         *      -- 4:   destroy boot logo
         *      -- 0:   done, post no next FIM_STARTUP.
         *
         *      This is maybe the most obscure code in XWorkplace.
         */

        case FIM_STARTUP:
        {
            ULONG   ulAction = (ULONG)mp1;
            BOOL    fPostNext = FALSE;

            #ifdef DEBUG_STARTUP
                _Pmpf(("fnwpFileObject: got FIM_STARTUP(%d)", ulAction));
            #endif

            switch (ulAction)
            {
                case 1: // "just installed" check
                {
                    // if XFolder was just installed, check for
                    // existence of config folders and
                    // display welcome msg
                    if (PrfQueryProfileInt(HINI_USER,
                                           (PSZ)INIAPP_XWORKPLACE,
                                           (PSZ)INIKEY_JUSTINSTALLED,
                                           0x123) != 0x123)
                    {   // XFolder was just installed:
                        // delete "just installed" INI key
                        PrfWriteProfileString(HINI_USER,
                                              (PSZ)INIAPP_XWORKPLACE,
                                              (PSZ)INIKEY_JUSTINSTALLED,
                                              NULL);
                        // even if the installation folder exists, create a
                        // a new one
                        // if (!_wpclsQueryFolder(_WPFolder, XFOLDER_MAINID, TRUE))
                            /* xthrPostFileMsg(FIM_RECREATECONFIGFOLDER,
                                            (MPARAM)RCF_MAININSTALLFOLDER,
                                            MPNULL); */

                        krnPostThread1ObjectMsg(T1M_WELCOME, MPNULL, MPNULL);
                    }

                    fPostNext = TRUE;
                break; }

                case 2: // startup folder
                {
                    fPostNext = TRUE;
                    // initiate processing of startup folder; this is
                    // again done by the Thread-1 object window.
                    if (krnNeed2ProcessStartupFolder())
                    {
                        PCKERNELGLOBALS pKernelGlobals = krnQueryGlobals();
                        // check if startup folder is to be skipped
                        if ((pKernelGlobals->ulPanicFlags & SUF_SKIPXFLDSTARTUP) == 0)
                                // V0.9.3 (2000-04-25) [umoeller]
                        {
                            // OK, process startup folder
                            krnPostThread1ObjectMsg(T1M_BEGINSTARTUP, MPNULL, MPNULL);
                                // this will continue asynchronously and
                                // post FIM_STARTUPFOLDERDONE when done
                            fPostNext = FALSE;
                        }
                    }
                break; }

                case 3: // populate config folders
                {
                    /* XFolder *pFolder = _xwpclsQueryConfigFolder(_XFolder); // changed V0.9.0
                    if (pFolder)
                        wpshPopulateTree(pFolder); */

                    // now go for the "Quick open" folders;
                    krnPostThread1ObjectMsg(T1M_BEGINQUICKOPEN, MPNULL, MPNULL);
                            // this posts FIM_STARTUPFOLDERDONE(2)
                            // back to us, which in turn posts
                            // FIM_STARTUP(4)... kinda sick.
                break; }

                case 4: // done completely:
                {
                    CHAR    szDesktopDir[2*CCHMAXPATH];
                    // stop this posting game
                    fPostNext = FALSE;

                    // destroy boot logo, if present
                    xthrPostSpeedyMsg(QM_DESTROYLOGO, 0, 0);

                    if (_wpQueryFilename(cmnQueryActiveDesktop(),
                                         szDesktopDir,
                                         TRUE))     // fully q'fied
                    {
                        WPFileSystem *pMarkerFile;
                        strcat(szDesktopDir, "\\");
                        strcat(szDesktopDir, XWORKPLACE_ARCHIVE_MARKER);
                        pMarkerFile
                            = _wpclsQueryObjectFromPath(_WPFileSystem, szDesktopDir);
                        if (pMarkerFile)
                            // exists:
                            _wpFree(pMarkerFile);
                    }
                break; }
            } // end switch

            if (fPostNext)
                // post next startup action
                xthrPostFileMsg(FIM_STARTUP,
                                (MPARAM)(ulAction+1),
                                0);
        break; }

        /*
         *@@ FIM_STARTUPFOLDERDONE:
         *      this gets posted from various locations
         *      involved in processing the Startup folder
         *      when processing is complete or has been
         *      cancelled.
         *
         *      Parameters:
         *          mp1:        if 1, we're done with the startup folder.
         *                      If 2, we're done with the quick-open folders.
         */

        case FIM_STARTUPFOLDERDONE:
        {
            ULONG ulNextAction = 0;
            switch ((ULONG)mp1)
            {
                case 1: ulNextAction = 3; break;
                case 2: ulNextAction = 4; break;
            }

            xthrPostFileMsg(FIM_STARTUP,
                            (MPARAM)ulNextAction,
                            0);
        break; }

        /*
         *@@ FIM_RECREATECONFIGFOLDER:
         *    this msg is posted if the
         *    config folder was not found.
         */

        case FIM_RECREATECONFIGFOLDER:
        {
            static BOOL fWarningOpen = FALSE;
            ULONG   ulAction = (ULONG)mp1;
            ULONG   ulReturn = ulAction;

            if (ulAction == RCF_QUERYACTION)
            {
                HWND  hwndDlg;

                if (fWarningOpen)
                    break;

                cmnSetDlgHelpPanel(ID_XFH_NOCONFIG);
                hwndDlg = WinLoadDlg(HWND_DESKTOP,
                                    HWND_DESKTOP,
                                    (PFNWP)cmn_fnwpDlgWithHelp,
                                    cmnQueryNLSModuleHandle(FALSE),
                                    ID_XFD_NOCONFIG, // "not found" dialog
                                    (PVOID)NULL);
                fWarningOpen = TRUE;
                ulReturn = (WinProcessDlg(hwndDlg)
                                 == DID_DEFAULT)
                            ?  RCF_DEFAULTCONFIGFOLDER
                            :  RCF_EMPTYCONFIGFOLDERONLY;
                WinDestroyWindow(hwndDlg);
                fWarningOpen = FALSE;
            }

            if (ulReturn == RCF_EMPTYCONFIGFOLDERONLY)
            {
                CHAR szObjectID[50];
                sprintf(szObjectID, "OBJECTID=%s;", XFOLDER_CONFIGID);
                WinCreateObject("WPFolder",             // now create new config folder w/ proper objID
                                "XFolder Configuration",
                                szObjectID,
                                (PSZ)WPOBJID_DESKTOP, // "<WP_DESKTOP>",                 // on desktop
                                CO_UPDATEIFEXISTS);
            }
            else if (   (ulReturn == RCF_DEFAULTCONFIGFOLDER)
                     || (ulReturn == RCF_MAININSTALLFOLDER)
                    )
            {
                HWND    hwndCreating;
                CHAR    szPath[CCHMAXPATH], szPath2[CCHMAXPATH], szPath3[CCHMAXPATH];
                if (cmnQueryXWPBasePath(szPath))
                {   // INI entry found: append "\" if necessary
                    PID     pid;
                    ULONG   sid;
                    sprintf(szPath2, "/c %s\\install\\%s%s.cmd",
                            szPath,
                            (ulReturn == RCF_DEFAULTCONFIGFOLDER)
                                ? "crobj"       // script for config folder only
                                : "instl",      // script for full installation folder
                                                // (used after first WPS bootup)
                            cmnQueryLanguageCode());
                    strcpy(szPath3, &(szPath2[3]));

                    // "creating config" window
                    hwndCreating = WinLoadDlg(HWND_DESKTOP, HWND_DESKTOP,
                                              WinDefDlgProc,
                                              cmnQueryNLSModuleHandle(FALSE),
                                              ID_XFD_CREATINGCONFIG,
                                              0);
                    WinShowWindow(hwndCreating, TRUE);
                    doshQuickStartSession("cmd.exe",
                                          szPath2,
                                          FALSE, // background
                                          SSF_CONTROL_INVISIBLE, // but auto-close
                                          TRUE,  // wait
                                          &sid, &pid);

                    WinDestroyWindow(hwndCreating);
                }
            }
        break; }

        /*
         *@@ FIM_PROCESSTASKLIST:
         *      this processes a file-task-list created
         *      using fopsCreateFileTaskList. This message
         *      gets posted by fopsStartProcessingTasks.
         *
         *      This calls fopsFileThreadProcessing in turn.
         *      This message only makes sure that processing
         *      runs on the File thread.
         *
         *      Note: Do not _send_ this msg, since
         *      fopsFileThreadProcessing possibly takes a
         *      long time.
         *
         *      Parameters:
         *      -- HFILETASKLIST mp1: file task list to process.
         *      -- HWND mp2: temporary window for modal processing.
         *
         *@@added V0.9.1 (2000-01-29) [umoeller]
         *@@changed V0.9.4 (2000-08-03) [umoeller]: added hwndNotify
         */

        case FIM_PROCESSTASKLIST:
            fopsFileThreadProcessing(WinQueryAnchorBlock(hwndObject),
                                     (HFILETASKLIST)mp1,
                                     (HWND)mp2);
        break;

        /*
         * FIM_REFRESH:
         *      this refreshes a folder's content by invoking
         *      wpRefresh on it. This gets posted from
         *      mnuMenuItemSelected if the
         *
         *      Parameters:
         *          WPFolder* mp1:  folder to refresh
         *          HWND hwndFrame:  frame of folder to refresh
         *
         *changed V0.9.4 (2000-08-02) [umoeller]: now invalidating cnr also
         *      removed this   V0.9.6 (2000-10-16) [umoeller]
         */

        /*
         *@@ FIM_DOUBLEFILES:
         *      collects all files in a given list of directories
         *      and creates a new list of files which occur in
         *      several directories.
         *
         *      This gets called from the "Double files" dialog
         *      on OS/2 kernel's "System path" page.
         *
         *      Parameters:
         *          PDOUBLEFILES mp1:  info structure.
         *
         *      When done, we post DOUBLEFILES.ulNotifyMsg to
         *      the window specified in DOUBLEFILES.hwndNotify.
         *      With that message, the window gets the DOUBLEFILES
         *      structure back in mp1, with pllFiles set to the
         *      new files list of FILELISTITEM's.
         *
         *      It is the responsibility of the notify window to
         *      invoke lstFree() on DOUBLEFILES.pllFiles.
         */

        case FIM_DOUBLEFILES:
            CollectDoubleFiles(mp1);
        break;

        /*
         *@@ FIM_INSERTHOTKEYS:
         *      inserts all object hotkeys as
         *      HOTKEYRECORD records into the given
         *      container window. This is passed
         *      to us from hifKeybdHotkeysInitPage
         *      because this takes several seconds.
         *
         *      Parameters:
         *      -- HWND mp1: container window.
         *      -- PBOOL mp2: flag set to TRUE while we're
         *                    working on this.
         *
         *      Note: this locks all objects to which
         *      object hotkeys have been assigned.
         */

        case FIM_INSERTHOTKEYS:
            hifCollectHotkeys(mp1, mp2);
        break;

        /*
         *@@ FIM_CALCTRASHOBJECTSIZE:
         *      calls trshCalcTrashObjectSize. This
         *      message has only been implemented to
         *      offload this task to the File thread.
         *
         *      Parameters:
         *      -- XWPTrashObject* mp1: trash object.
         *      -- XWPTrashCan* mp2: trash can to which mp1 has been added.
         *
         *@@added V0.9.2 (2000-02-28) [umoeller]
         */

        case FIM_CALCTRASHOBJECTSIZE:
            trshCalcTrashObjectSize((XWPTrashObject*)mp1,
                                    (XWPTrashCan*)mp2);
        break;

        #ifdef __DEBUG__
        case XM_CRASH:          // posted by debugging context menu of XFldDesktop
            CRASH;
        break;
        #endif

        default:
            G_CurFileThreadMsg = 0;
            mrc = WinDefWindowProc(hwndObject, msg, mp1, mp2);
    }

    G_CurFileThreadMsg = 0;

    return (mrc);
}

/*
 *@@ xthrOnKillFileThread:
 *      thread "exit list" func registered with
 *      the TRY_xxx macros (helpers\except.c).
 *      In case the File thread gets killed,
 *      this function gets called. As opposed to
 *      real exit list functions, which always get
 *      called on thread 1, this gets called on
 *      the thread which registered the exception
 *      handler.
 *
 *      We release mutex semaphores here, which we
 *      must do, because otherwise the system might
 *      hang if another thread is waiting on the
 *      same semaphore.
 *
 *@@added V0.9.0 [umoeller]
 */

VOID APIENTRY xthrOnKillFileThread(PEXCEPTIONREGISTRATIONRECORD2 pRegRec2)
{
    krnUnlockGlobals(); // just to make sure
}

/*
 *@@ fntFileThread:
 *          this is the thread function for the XFolder
 *          "File" thread.
 *
 *          This thread is used for file operations mainly,
 *          but also for other tasks which should not be
 *          processed in the (idle-time) Worker thread.
 *
 *          Post the File object window (fnwpFileObject)
 *          a FIM_* message (using xthrPostFileMsg),
 *          and the corresponding file action will be executed.
 *
 *          As opposed to the "Worker" thread, this thread runs
 *          with normal regular priority (delta +/- 0).
 *
 *          This thread is also created from krnInitializeXWorkplace.
 *
 *@@added V0.9.0 [umoeller]
 */

void _Optlink fntFileThread(PTHREADINFO pti)
{
    QMSG                  qmsg;
    PSZ                   pszErrMsg = NULL;

    TRY_LOUD(excpt1)
    {
        if (G_habFileThread = WinInitialize(0))
        {
            if (G_hmqFileThread = WinCreateMsgQueue(G_habFileThread, 3000))
            {
                PKERNELGLOBALS pKernelGlobals = krnLockGlobals(__FILE__, __LINE__, __FUNCTION__);
                if (pKernelGlobals)
                {
                    WinCancelShutdown(G_hmqFileThread, TRUE);

                    WinRegisterClass(G_habFileThread,
                                     (PSZ)WNDCLASS_FILEOBJECT,    // class name
                                     (PFNWP)fnwpFileObject,    // Window procedure
                                     0,                  // class style
                                     0);                 // extra window words

                    // set ourselves to regular priority
                    DosSetPriority(PRTYS_THREAD,
                                   PRTYC_REGULAR,
                                   0, // priority delta
                                   0);

                    // create object window
                    pKernelGlobals->hwndFileObject
                        = winhCreateObjectWindow(WNDCLASS_FILEOBJECT, NULL);

                    if (!pKernelGlobals->hwndFileObject)
                        winhDebugBox(HWND_DESKTOP,
                                 "XFolder: Error",
                                 "XFolder failed to create the File thread object window.");

                    krnUnlockGlobals();
                    pKernelGlobals = NULL;
                }

                // now enter the message loop
                while (WinGetMsg(G_habFileThread, &qmsg, NULLHANDLE, 0, 0))
                    // loop until WM_QUIT
                    WinDispatchMsg(G_habFileThread, &qmsg);
            }
        }
    }
    CATCH(excpt1)
    {
        // the thread exception handler puts us here if an exception occured:
        // clean up

        if (pszErrMsg == NULL)
        {
            // only report the first error, or otherwise we will
            // jam the system with msg boxes
            pszErrMsg = malloc(1000);
            if (pszErrMsg)
            {
                strcpy(pszErrMsg, "An error occured in the XFolder File thread.\n"
                                  "The File thread has been terminated. This severely limits "
                                  "XWorkplace's functionality. Please restart the WPS now "
                                  "to have the File thread restarted also. ");
                krnPostThread1ObjectMsg(T1M_EXCEPTIONCAUGHT, (MPARAM)pszErrMsg, MPNULL);
            }
        }
        // get out of here
    } END_CATCH();

    {
        PKERNELGLOBALS pKernelGlobals = krnLockGlobals(__FILE__, __LINE__, __FUNCTION__);
        if (pKernelGlobals)
        {
            WinDestroyWindow(pKernelGlobals->hwndFileObject);
            pKernelGlobals->hwndFileObject = NULLHANDLE;
            krnUnlockGlobals();
        }
        WinDestroyMsgQueue(G_hmqFileThread);
        G_hmqFileThread = NULLHANDLE;
        WinTerminate(G_habFileThread);
        G_habFileThread = NULLHANDLE;
    }
}

/* ******************************************************************
 *                                                                  *
 *   here come the Speedy thread functions                           *
 *                                                                  *
 ********************************************************************/

/*
 *@@ xthrPostSpeedyMsg:
 *      this posts a msg to the XFolder Speedy thread object window.
 */

BOOL xthrPostSpeedyMsg(ULONG msg, MPARAM mp1, MPARAM mp2)
{
    BOOL rc = FALSE;
    PCKERNELGLOBALS pKernelGlobals = krnQueryGlobals();

    if (thrQueryID(&pKernelGlobals->tiSpeedyThread))
        if (pKernelGlobals->hwndSpeedyObject)
            rc = WinPostMsg(pKernelGlobals->hwndSpeedyObject,
                            msg,
                            mp1,
                            mp2);
    if (!rc)
        cmnLog(__FILE__, __LINE__, __FUNCTION__,
               "Failed to post msg = 0x%lX, mp1 = 0x%lX, mp2 = 0x%lX",
               msg, mp1, mp2);

    return (rc);
}

SHAPEFRAME sb = {0};

/*
 *@@ fnwpSpeedyObject:
 *      wnd proc for Speedy thread object window
 *      (see fntSpeedyThread below)
 *
 *@@changed V0.9.0 [umoeller]: adjusted for new global settings
 *@@changed V0.9.3 (2000-04-25) [umoeller]: moved all multimedia stuff to media\mmthread.c
 */

MRESULT EXPENTRY fnwpSpeedyObject(HWND hwndObject, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = NULL;

    switch (msg)
    {
        /*
         * WM_CREATE:
         *      show XFolder logo at bootup
         */

        case WM_CREATE:
        {
            PCKERNELGLOBALS pKernelGlobals = krnQueryGlobals();
            PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
            // these are already initialized by xfobjM_wpclsInitData

            memset(&sb, 0, sizeof(sb));

            // boot logo disabled because Shift pressed at WPS startup?
            if ((pKernelGlobals->ulPanicFlags & SUF_SKIPBOOTLOGO) == 0)
                // no: logo allowed?
                if (pGlobalSettings->BootLogo)
                {
                    PSZ pszBootLogoFile = cmnQueryBootLogoFile();

                    if (pGlobalSettings->bBootLogoStyle == 1)
                    {
                        // blow-up mode:
                        HDC         hdcMem;
                        HPS         hpsMem;
                        SIZEL       szlPage = {0, 0};
                        if (gpihCreateMemPS(G_habSpeedyThread,
                                            &szlPage,
                                            &hdcMem,
                                            &hpsMem))
                        {
                            HBITMAP hbmBootLogo;
                            ULONG   ulError;
                            if (hbmBootLogo = gpihLoadBitmapFile(hpsMem,
                                                                 pszBootLogoFile,
                                                                 &ulError))
                            {
                                HPS     hpsScreen = WinGetScreenPS(HWND_DESKTOP);
                                anmBlowUpBitmap(hpsScreen,
                                                hbmBootLogo,
                                                1000);  // total animation time
                                WinReleasePS(hpsScreen);

                                // delete the bitmap again
                                GpiDeleteBitmap(hbmBootLogo);
                            }
                            GpiDestroyPS(hpsMem);
                            DevCloseDC(hdcMem);
                        }
                    }
                    else
                    {
                        // transparent mode:
                        if (shpLoadBitmap(G_habSpeedyThread,
                                          pszBootLogoFile, // from file,
                                          0, 0,     // not from resources
                                          &sb))
                            // create shape (transparent) windows
                            shpCreateWindows(&sb);
                    }

                    free(pszBootLogoFile);
                } // end if (pGlobalSettings->BootLogo)

            mrc = WinDefWindowProc(hwndObject, msg, mp1, mp2);
        break; }

        /*
         * QM_DESTROYLOGO:
         *
         */

        case QM_DESTROYLOGO:
        {
            PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

            if (pGlobalSettings->bBootLogoStyle == 0)
                // was bitmap window created successfully?
                if (sb.hwndShapeFrame)
                {
                    // yes: clean up
                    shpFreeBitmap(&sb);
                    WinDestroyWindow(sb.hwndShapeFrame) ;
                    WinDestroyWindow(sb.hwndShape);
                }
        break; }

        /*
         * QM_BOOTUPSTATUS:
         *      posted by XFldObject::wpclsInitData every
         *      time a WPS class gets initialized and
         *      "Report class initialization" on the "WPS
         *      Classes" page is on. We will then display
         *      a little window for a few seconds.
         *      Parameters: mp1 points to the SOM class
         *      object being initalized; if NULL, then
         *      the bootup status wnd will be destroyed.
         */

        case QM_BOOTUPSTATUS:
        {
            if (G_hwndBootupStatus == NULLHANDLE)
            {
                // window currently not visible:
                // create it
                strcpy(G_szBootupStatus, "Loaded %s");
                G_hwndBootupStatus = WinLoadDlg(HWND_DESKTOP, HWND_DESKTOP,
                                              fnwpGenericStatus,
                                              cmnQueryNLSModuleHandle(FALSE),
                                              ID_XFD_BOOTUPSTATUS,
                                              NULL);
                WinSetWindowPos(G_hwndBootupStatus,
                                HWND_TOP,
                                0, 0, 0, 0,
                                SWP_SHOW); // show only, do not activate;
                                    // we don't want the window to get the
                                    // focus, because this would close
                                    // open menus and such
            }

            if (G_hwndBootupStatus)
                // window still visible or created anew:
                if (mp1 == NULL)
                    WinDestroyWindow(G_hwndBootupStatus);
                else
                {
                    CHAR szTemp[2000];
                    WPObject* pObj = (WPObject*)mp1;

                    // get class title (e.g. "WPObject")
                    PSZ pszClassTitle = _somGetName(pObj);
                    if (pszClassTitle)
                        sprintf(szTemp, G_szBootupStatus, pszClassTitle);
                    else sprintf(szTemp, G_szBootupStatus, "???");

                    WinSetDlgItemText(G_hwndBootupStatus,
                                      ID_XFDI_BOOTUPSTATUSTEXT,
                                      szTemp);

                    // show window for 2 secs
                    WinStartTimer(G_habSpeedyThread, hwndObject, 1, 2000);
                }
        break; }

        /*
         * WM_TIMER:
         *      destroy bootup status wnd
         */

        case WM_TIMER:
        {
            switch ((USHORT)mp1) // timer id
            {

                // timer 1: bootup status wnd
                case 1:
                    if (G_hwndBootupStatus)
                    {
                        WinDestroyWindow(G_hwndBootupStatus);
                        G_hwndBootupStatus = NULLHANDLE;
                    }
                    WinStopTimer(G_habWorkerThread, hwndObject, 1);
                break;
            }
        break; }

        /*
         *@@ QM_TREEVIEWAUTOSCROLL:
         *     this msg is posted mainly by fdr_fnwpSubclassedFolderFrame
         *     (subclassed folder windows) after the "plus" sign has
         *     been clicked on (WM_CONTROL for containers with
         *     CN_EXPANDTREE notification).
         *
         *      Parameters:
         *          HWND mp1:    frame wnd handle
         *          PMINIRECORDCORE mp2:
         *                       the expanded minirecordcore
         *
         *@@changed V0.9.4 (2000-06-16) [umoeller]: moved this to Speedy thread from File thread
         */

        case QM_TREEVIEWAUTOSCROLL:
        {
            WPObject    *pFolder = OBJECT_FROM_PREC((PMINIRECORDCORE)mp2);

            if (wpshCheckObject(pFolder))
            {
                while (_somIsA(pFolder, _WPShadow))
                    pFolder = _wpQueryShadowedObject(pFolder, TRUE);

                if (!_somIsA(pFolder, _WPFolder)) // check only folders, avoid disks
                    break;

                // now check if the folder whose "plus" sign has been
                // clicked on is already populated: if so, the WPS seems
                // to insert the objects directly (i.e. in the Workplace
                // thread), if not, the objects are inserted by some
                // background populate thread, which we have no control
                // over...
                if ( (_wpQueryFldrFlags(pFolder) & FOI_POPULATEDWITHFOLDERS) == 0)
                {
                    // NOT fully populated: sleep a while, then post
                    // the same msg again, until the "populated" folder
                    // flag has been set
                    DosSleep(100);
                    xthrPostSpeedyMsg(QM_TREEVIEWAUTOSCROLL, mp1, mp2);
                }
                else
                {
                    // otherwise: scroll the tree view properly
                    HWND                hwndCnr = WinWindowFromID((HWND)mp1, 0x8008);
                    PMINIRECORDCORE     preccLastChild;
                    preccLastChild = WinSendMsg(hwndCnr, CM_QUERYRECORD,
                                                mp2,   // PMINIRECORDCORE
                                                MPFROM2SHORT(CMA_LASTCHILD,
                                                             CMA_ITEMORDER));
                    cnrhScrollToRecord(hwndCnr,
                                       (PRECORDCORE)preccLastChild,
                                       CMA_TEXT,
                                       TRUE);
                }
            }
        break; }

        default:
            mrc = WinDefWindowProc(hwndObject, msg, mp1, mp2);
    }

    return (mrc);
}

/*
 *@@ xthrOnKillSpeedyThread:
 *      thread "exit list" func registered with
 *      the TRY_xxx macros (helpers\except.c).
 *      In case the Speedy thread gets killed,
 *      this function gets called. As opposed to
 *      real exit list functions, which always get
 *      called on thread 1, this gets called on
 *      the thread which registered the exception
 *      handler.
 *
 *      We release mutex semaphores here, which we
 *      must do, because otherwise the system might
 *      hang if another thread is waiting on the
 *      same semaphore.
 *
 *@@added V0.9.0 [umoeller]
 */

VOID APIENTRY xthrOnKillSpeedyThread(PEXCEPTIONREGISTRATIONRECORD2 pRegRec2)
{
    krnUnlockGlobals(); // just to make sure
}

/*
 *@@ fntSpeedyThread:
 *          this is the thread function for the XFolder
 *          "Speedy" thread, which is responsible for
 *          showing that bootup logo and displaying
 *          the notification window when classes get initialized.
 *
 *          Also this plays the new XFolder system sounds by
 *          calling the funcs in sounddll.c (SOUND.DLL), if that
 *          DLL was successfully loaded.
 *
 *          To play system sounds, use xthrPostSpeedyMsg.
 *
 *          As opposed to the "Worker" thread, this thread runs
 *          with high regular priority.
 *
 *          This thread is also created from krnInitializeXWorkplace.
 *
 *@@changed V0.9.3 (2000-04-25) [umoeller]: moved all multimedia stuff to media\mmthread.c
 */

void _Optlink fntSpeedyThread(PTHREADINFO pti)
{
    QMSG                  qmsg;
    PSZ                   pszErrMsg = NULL;
    BOOL                  fTrapped = FALSE;

    TRY_LOUD(excpt1)
    {
        if (G_habSpeedyThread = WinInitialize(0))
        {
            if (G_hmqSpeedyThread = WinCreateMsgQueue(G_habSpeedyThread, 3000))
            {
                PKERNELGLOBALS pKernelGlobals = krnLockGlobals(__FILE__, __LINE__, __FUNCTION__);
                if (pKernelGlobals)
                {
                    WinCancelShutdown(G_hmqSpeedyThread, TRUE);

                    WinRegisterClass(G_habSpeedyThread,
                                     (PSZ)WNDCLASS_QUICKOBJECT,    // class name
                                     (PFNWP)fnwpSpeedyObject,    // Window procedure
                                     0,                  // class style
                                     0);                 // extra window words

                    // set ourselves to higher regular priority
                    DosSetPriority(PRTYS_THREAD,
                                   PRTYC_REGULAR,
                                   +31, // priority delta
                                   0);

                    // create object window
                    pKernelGlobals->hwndSpeedyObject
                        = winhCreateObjectWindow(WNDCLASS_QUICKOBJECT, NULL);

                    if (!pKernelGlobals->hwndSpeedyObject)
                        winhDebugBox(HWND_DESKTOP,
                                 "XFolder: Error",
                                 "XFolder failed to create the Speedy thread object window.");

                    krnUnlockGlobals();
                    pKernelGlobals = NULL;
                }

                // now enter the message loop
                while (WinGetMsg(G_habSpeedyThread, &qmsg, NULLHANDLE, 0, 0))
                    WinDispatchMsg(G_habSpeedyThread, &qmsg);
                                // loop until WM_QUIT
            }
        }
    }
    CATCH(excpt1)
    {
        // the thread exception handler puts us here if an exception occured:
        // clean up
        if (pszErrMsg == NULL)
        {
            // only report the first error, or otherwise we will
            // jam the system with msg boxes
            pszErrMsg = malloc(1000);
            if (pszErrMsg)
            {
                strcpy(pszErrMsg, "An error occured in the XFolder Speedy thread. "
                        "\n\nThe additional XFolder system sounds will be disabled for the "
                        "rest of this Workplace Shell session. You will need to restart "
                        "the WPS in order to re-enable them. "
                        "\n\nIf errors like these persist, you might want to disable the "
                        "additional XFolder system sounds again. For doing this, execute "
                        "SOUNDOFF.CMD in the BIN subdirectory of the XFolder installation "
                        "directory. ");
                krnPostThread1ObjectMsg(T1M_EXCEPTIONCAUGHT, (MPARAM)pszErrMsg, MPNULL);
            }
        }

        // disable sounds
        fTrapped = TRUE;
    } END_CATCH();

    {
        PKERNELGLOBALS pKernelGlobals = krnLockGlobals(__FILE__, __LINE__, __FUNCTION__);
        if (pKernelGlobals)
        {
            WinDestroyWindow(pKernelGlobals->hwndSpeedyObject);
            pKernelGlobals->hwndSpeedyObject = NULLHANDLE;
        }

        WinDestroyMsgQueue(G_hmqSpeedyThread);
        G_hmqSpeedyThread = NULLHANDLE;
        WinTerminate(G_habSpeedyThread);
        G_habSpeedyThread = NULLHANDLE;

        /* if (fTrapped)
            pKernelGlobals->ulMMPM2Working = MMSTAT_CRASHED;
        else
            pKernelGlobals->ulMMPM2Working = MMSTAT_UNKNOWN; */

        krnUnlockGlobals();
    }
}

/*
 *@@ xthrStartThreads:
 *      this gets called by krnInitializeXWorkplace upon
 *      system startup to start the three additional
 *      XWorkplace threads.
 *
 *@@changed V0.9.3 (2000-04-25) [umoeller]: moved all multimedia stuff to media\mmthread.c
 */

BOOL xthrStartThreads(FILE *DumpFile)
{
    BOOL brc = FALSE;
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    PKERNELGLOBALS pKernelGlobals = krnLockGlobals(__FILE__, __LINE__, __FUNCTION__);

    if (DumpFile)
        fprintf(DumpFile,
                "\nEntering " __FUNCTION__":\n");

    if (pKernelGlobals)
    {
        if (thrQueryID(&pKernelGlobals->tiWorkerThread) == NULLHANDLE)
        {
            APIRET      arc;
            CHAR szSoundDLL[CCHMAXPATH] = "";

            // store the thread ID of the calling thread;
            // this should always be 1
            // ... moved this to krnInitializeXWorkplace

            /*
             *  start threads
             *
             */

            if (pGlobalSettings->NoWorkerThread == 0)
            {
                // threads not disabled:
                pKernelGlobals->ulWorkerMsgCount = 0;
                thrCreate(&pKernelGlobals->tiWorkerThread,
                          fntWorkerThread,
                          NULL, // running flag
                          THRF_WAIT,    // no msgq, but wait V0.9.9 (2001-01-31) [umoeller]
                          0);

                if (DumpFile)
                    fprintf(DumpFile,
                            "  Started XWP Worker thread, TID: %d\n",
                            pKernelGlobals->tiWorkerThread.tid);

                thrCreate(&pKernelGlobals->tiSpeedyThread,
                          fntSpeedyThread,
                          NULL, // running flag
                          THRF_WAIT,    // no msgq, but wait V0.9.9 (2001-01-31) [umoeller]
                          0);

                if (DumpFile)
                    fprintf(DumpFile,
                            "  Started XWP Speedy thread, TID: %d\n",
                            pKernelGlobals->tiSpeedyThread.tid);
            }

            thrCreate(&pKernelGlobals->tiFileThread,
                      fntFileThread,
                      NULL, // running flag
                      THRF_WAIT,    // no msgq, but wait V0.9.9 (2001-01-31) [umoeller]
                      0);

            if (DumpFile)
                fprintf(DumpFile,
                        "  Started XWP File thread, TID: %d\n",
                        pKernelGlobals->tiFileThread.tid);
        }

        krnUnlockGlobals();
    }

    return (brc);
}


