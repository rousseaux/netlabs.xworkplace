
/*
 *@@sourcefile kernel.h:
 *      header file for kernel.c. See remarks there.
 *
 *@@include #define INCL_DOSDATETIME
 *@@include #include <os2.h>
 *@@include #include "helpers\linklist.h"       // for some features
 *@@include #include "filesys\xthreads.h"
 */

/*
 *      Copyright (C) 1997-2000 Ulrich M”ller.
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, in version 2 as it comes in the COPYING
 *      file of the XWorkplace main distribution.
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 */

#ifndef XTHREADS_HEADER_INCLUDED
    #define XTHREADS_HEADER_INCLUDED

    /********************************************************************
     *                                                                  *
     *   Worker thread messages (WOM_xxx)                               *
     *                                                                  *
     ********************************************************************/

    #define WOM_WELCOME                 (WM_USER+100)

    /*
     *@@ WOM_ADDAWAKEOBJECT:
     *      this is posted by XFldObject for each
     *      object that is awaked by the WPS; we
     *      need to maintain a list of these objects
     *      for XShutdown.
     *
     *      Parameters:
     *          WPObject* mp1: somSelf as in XFldObject::wpObjectReady
     */

    #define WOM_ADDAWAKEOBJECT          (WM_USER+101)

    /*
     *@@ WOM_REMOVEAWAKEOBJECT:
     *      this is posted by XFldObject also, but
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

    #define WOM_REMOVEAWAKEOBJECT       (WM_USER+102)
    #define WOM_SHOWFOLDERDATA          (WM_USER+103)
    #define WOM_REFRESHFOLDERVIEWS      (WM_USER+104)

    #define WOM_PROCESSORDEREDCONTENT   (WM_USER+110)
    #define WOM_WAITFORPROCESSNEXT      (WM_USER+111)

    // #define WOM_INVALIDATEORDEREDCONTENT WM_USER+120 removed V0.9.0
    #define WOM_DELETEICONPOSEA         (WM_USER+121)
    #define WOM_DELETEFOLDERPOS         (WM_USER+122)

    #define WOM_UPDATEALLSTATUSBARS     (WM_USER+133)
    #define WOM_QUICKOPEN               (WM_USER+135)

    #define WOM_STOREGLOBALSETTINGS     (WM_USER+136)

    /********************************************************************
     *                                                                  *
     *   Speedy thread messages (QM_xxx)                                *
     *                                                                  *
     ********************************************************************/

    #define QM_BOOTUPSTATUS             (WM_USER+140)
    // #define QM_PLAYSOUND                (WM_USER+141)
    // #define QM_PLAYSYSTEMSOUND          (WM_USER+142)

    #define QM_DESTROYLOGO              (WM_USER+143)

    #define QM_TREEVIEWAUTOSCROLL       (WM_USER+144)

    /********************************************************************
     *                                                                  *
     *   File thread messages (FIM_xxx)                                 *
     *                                                                  *
     ********************************************************************/

    // flags for recreating config folder (FIM_RECREATECONFIGFOLDER)
    #define RCF_QUERYACTION            0
            // display message box
    #define RCF_EMPTYCONFIGFOLDERONLY  1
            // create empty config folder
    #define RCF_DEFAULTCONFIGFOLDER    2
            // create default config folder
    #define RCF_MAININSTALLFOLDER      3
            // create "main" installation folder

    // #define FIM_DESKTOPPOPULATED        (WM_USER+130)

    #define FIM_DESKTOPPOPULATED        (WM_USER+130)
        // changed V0.9.5 (2000-08-26) [umoeller]

    #define FIM_RECREATECONFIGFOLDER    (WM_USER+131)

    #define FIM_STARTUP                 (WM_USER+132)

    #define FIM_STARTUPFOLDERDONE       (WM_USER+133)

    #define FIM_PROCESSTASKLIST         (WM_USER+201)

    #define FIM_REFRESH                 (WM_USER+204)

    /*
     *@@ FILELISTITEM:
     *      structure used for DOUBLEFILES.pllFiles.
     */

    typedef struct _FILELISTITEM
    {
        CHAR        szFilename[CCHMAXPATH];
        PSZ         pszDirectory;       // directory (points into DOUBLEFILES.pllDirectories)
        FDATE       fDate;
        FTIME       fTime;
        ULONG       ulSize;
        BOOL        fProcessed;         // anti-recursion flag, ignore
    } FILELISTITEM, *PFILELISTITEM;

    #ifdef LINKLIST_HEADER_INCLUDED
        /*
         *@@ DOUBLEFILES:
         *      structure posted with FIM_DOUBLEFILES.
         */

        typedef struct _DOUBLEFILES
        {
            PLINKLIST   pllDirectories;     // in: linked list of PSZs with directory names
            HWND        hwndNotify;         // in: notify this wnd when done
            ULONG       ulNotifyMsg;        // in: message to use for notification
            PLINKLIST   pllDoubleFiles;     // out: linked list of FILELISTITEM structures
        } DOUBLEFILES, *PDOUBLEFILES;
    #endif

    #define FIM_DOUBLEFILES             (WM_USER+205)

    #define FIM_INSERTHOTKEYS           (WM_USER+206)

    #define FIM_CALCTRASHOBJECTSIZE     (WM_USER+207)

    /********************************************************************
     *                                                                  *
     *   Prototypes                                                     *
     *                                                                  *
     ********************************************************************/

    VOID xthrResetWorkerThreadPriority(VOID);

    BOOL xthrPostWorkerMsg(ULONG msg, MPARAM mp1, MPARAM mp2);

    BOOL xthrPostFileMsg(ULONG msg, MPARAM mp1, MPARAM mp2);

    ULONG xthrIsFileThreadBusy(VOID);

    BOOL xthrPostSpeedyMsg(ULONG msg, MPARAM mp1, MPARAM mp2);

    BOOL xthrStartThreads(VOID);

#endif
