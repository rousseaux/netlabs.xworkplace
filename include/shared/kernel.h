
/*
 *@@sourcefile kernel.h:
 *      header file for kernel.c. See remarks there.
 *
 *@@include #define INCL_DOSSEMAPHORES
 *@@include #define INCL_WINWINDOWMGR
 *@@include #define INCL_WINPOINTERS
 *@@include #include <os2.h>
 *@@include #include "helpers\linklist.h"       // for some features
 *@@include #include "helpers\threads.h"
 *@@include #include <wpobject.h>       // or any other WPS header, for KERNELGLOBALS
 *@@include #include "shared\common.h"
 *@@include #include "shared\kernel.h"
 */

/*
 *      Copyright (C) 1997-99 Ulrich M”ller.
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, in version 2 as it comes in the COPYING
 *      file of the XFolder main distribution.
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 */

#ifndef KERNEL_HEADER_INCLUDED
    #define KERNEL_HEADER_INCLUDED

    // required header files
    #include "helpers\threads.h"

    /********************************************************************
     *                                                                  *
     *   KERNELGLOBALS structure                                        *
     *                                                                  *
     ********************************************************************/

    #ifdef SOM_WPObject_h

        // startup flags for KERNELGLOBALS.ulStartupFlags
        #define SUF_SKIPBOOTLOGO            0x0001  // skip boot logo
        #define SUF_SKIPXFLDSTARTUP         0x0002  // skip XFldStartup processing
        #define SUF_SKIPQUICKOPEN           0x0004  // skip "quick open" folder processing

        // MMPM/2 status flags in KERNELGLOBALS.ulMMPM2Working;
        // these reflect the status of SOUND.DLL.
        // If this is anything other than MMSTAT_WORKING, sounds
        // are disabled.
        #define MMSTAT_UNKNOWN             0        // initial value
        #define MMSTAT_WORKING             1        // SOUND.DLL is working
        #define MMSTAT_MMDIRNOTFOUND       2        // MMPM/2 directory not found
        #define MMSTAT_SOUNDLLNOTFOUND     3        // SOUND.DLL not found
        #define MMSTAT_SOUNDLLNOTLOADED    4        // SOUND.DLL not loaded (DosLoadModule error)
        #define MMSTAT_SOUNDLLFUNCERROR    5
        #define MMSTAT_CRASHED             6        // SOUND.DLL crashed, sounds disabled

        /*
         *@@ KERNELGLOBALS:
         *      this structure is stored in a static global
         *      variable in kernel.c, whose address can
         *      always be obtained thru t1QueryGlobals.
         *
         *      This structure is used to store information
         *      which is of importance to all parts of XWorkplace.
         *      This includes the old XFolder code as well as
         *      possible new parts.
         *
         *      Here we store information about the classes
         *      which have been successfully initialized and
         *      lots of thread data.
         *
         *      You may extend this structure if you need to
         *      store global data, but please do not modify
         *      the existing fields. Also, for lucidity, use
         *      this structure only if you need to access
         *      data across several code files. Otherwise,
         *      please use global variables.
         */

        typedef struct _KERNELGLOBALS
        {

            ULONG               ulPanicFlags;
                    // flags set by the "panic" dialog if "Shift"
                    // was pressed during startup.
                    // Per default, this field is set to zero,
                    // but if the user disables something in the
                    // "Panic" dialog, this may have any of the
                    // following flags:
                    // -- SUF_SKIPBOOTLOGO: skip boot logo
                    // -- SUF_SKIPXFLDSTARTUP: skip XFldStartup processing
                    // -- SUF_SKIPQUICKOPEN: skip "quick open" folder processing

            /*
             * XWorkplace class objects (new with V0.9.0):
             *      if any of these is FALSE, this means that
             *      the class has not been installed. All
             *      these things are set to TRUE by the respective
             *      wpclsInitData class methods.
             *
             *      If you introduce a new class to the XWorkplace
             *      class setup, please add a BOOL to this list and
             *      set that field to TRUE in the wpclsInitData of
             *      your class.
             */

            // class replacements
            BOOL                fXFldObject,
                                fXFolder,
                                fXFldDisk,
                                fXFldDesktop,
                                fXFldDataFile,
                                fXFldProgramFile,
                                fXWPSound,

            // new classes
                                fXWPSetup,
                                fXFldSystem,
                                fXFldWPS,
                                fXFldStartup,
                                fXFldShutdown,
                                fXWPClassList,
                                fXWPTrashCan,
                                fXWPTrashObject;

            /*
             * XWorkplace daemon
             */

            HAPP                happDaemon;
            PVOID               pDaemonShared;  // ptr to DAEMONSHARED structure

            /*
             * Thread-1 object window:
             *      additional object window on thread 1.
             *      This is always created.
             */

            // Workplace thread ID (PMSHELL.EXE); should be 1
            TID                 tidWorkplaceThread;

            // XFolder Workplace object window handle
            HWND                hwndThread1Object;

            // WPS startup date and time (krnInitializeXWorkplace)
            DATETIME            StartupDateTime;

            /*
             * Worker thread:
             *      this thread is always running.
             */

            PTHREADINFO         ptiWorkerThread;
            HWND                hwndWorkerObject;

            ULONG               ulWorkerMsgCount;
            BOOL                WorkerThreadHighPriority;

            // here comes the linked list to remember all objects which
            // have been awakened by the WPS; this list is maintained
            // by the Worker thread and evaluated during XShutdown

            // root of this linked list; this holds plain
            // WPObject* pointers and is created in krnInitializeXWorkplace
            // with lstCreate(FALSE)
            PVOID               pllAwakeObjects;
                // this has changed with V0.90; this is actually a PLINKLIST,
                // but since not all source files have #include'd linklist.h,
                // we declare this as PVOID to avoid compilation errors.

            // mutex semaphore for access to this list
            HMTX                hmtxAwakeObjects;

            // count of currently awake objects
            LONG                lAwakeObjectsCount;

            // address of awake WarpCenter; stored by Worker
            // thread, read by Shutdown thread
            WPObject            *pAwakeWarpCenter;

            /*
             * Quick thread:
             *      this thread is always running.
             */

            PTHREADINFO         ptiQuickThread;
            HWND                hwndQuickObject;

            // sound data
            ULONG               ulMMPM2Working;      // MMSTAT_* flags above
            USHORT              usDeviceID;

            /*
             * File thread:
             *      this thread is always running also,
             *      but with regular priority.
             */

            PTHREADINFO         ptiFileThread;
            HWND                hwndFileObject;

            /*
             * Shutdown threads:
             *      the following threads are only running
             *      while XShutdown is in progress.
             */

            PTHREADINFO         ptiShutdownThread,
                                ptiUpdateThread;

            BOOL                fShutdownRunning;

            // debugging flags
            ULONG               ulShutdownFunc,
                                ulShutdownFunc2;

            // CONFIG.SYS filename (XFldSystem)
            CHAR                szConfigSys[CCHMAXPATH];

        } KERNELGLOBALS, *PKERNELGLOBALS;

        typedef const KERNELGLOBALS* PCKERNELGLOBALS;

        PCKERNELGLOBALS krnQueryGlobals(VOID);

        PKERNELGLOBALS krnLockGlobals(ULONG ulTimeout);

        VOID krnUnlockGlobals(VOID);

    #endif

    /* ******************************************************************
     *                                                                  *
     *   Startup/Daemon interface                                       *
     *                                                                  *
     ********************************************************************/

    VOID krnSetProcessStartupFolder(BOOL fReuse);

    BOOL krnNeed2ProcessStartupFolder(VOID);

    BOOL krnPostDaemonMsg(ULONG msg, MPARAM mp1, MPARAM mp2);

    /* ******************************************************************
     *                                                                  *
     *   Thread-1 object window                                         *
     *                                                                  *
     ********************************************************************/

    #define T1M_BEGINSTARTUP            WM_USER+1100

    #define T1M_POCCALLBACK             WM_USER+1101

    #define T1M_BEGINQUICKOPEN          WM_USER+1102
    #define T1M_NEXTQUICKOPEN           WM_USER+1103

    #define T1M_LIMITREACHED            WM_USER+1104

    #define T1M_EXCEPTIONCAUGHT         WM_USER+1105

    #define T1M_QUERYXFOLDERVERSION     WM_USER+1106

    #define T1M_EXTERNALSHUTDOWN        WM_USER+1107

    #define T1M_DESTROYARCHIVESTATUS    WM_USER+1108    // added V0.9.0

    #define T1M_OPENOBJECTFROMHANDLE    WM_USER+1110    // added V0.9.0

    #define T1M_DAEMONREADY             WM_USER+1111    // added V0.9.0

    MRESULT EXPENTRY krn_fnwpThread1Object(HWND hwndObject, ULONG msg, MPARAM mp1, MPARAM mp2);

    BOOL krnPostThread1ObjectMsg(ULONG msg, MPARAM mp1, MPARAM mp2);

    MRESULT krnSendThread1ObjectMsg(ULONG msg, MPARAM mp1, MPARAM mp2);

    /* ******************************************************************
     *                                                                  *
     *   XWorkplace initialization                                      *
     *                                                                  *
     ********************************************************************/

    VOID krnInitializeXWorkplace(VOID);

#endif
