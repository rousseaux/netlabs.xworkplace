
/*
 *@@sourcefile shutdown.h:
 *      header file for shutdown.c.
 *
 *@@include #define INCL_DOSPROCESS
 *@@include #define INCL_DOSSEMAPHORES
 *@@include #define INCL_WINWINDOWMGR
 *@@include #define INCL_WINPOINTERS
 *@@include #define INCL_WINSWITCHLIST
 *@@include #include <os2.h>
 *@@include #include "helpers\linklist.h"
 *@@include #include <wpobject.h>
 *@@include #include "startshut\shutdown.h"
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

#ifndef XSHUTDWN_HEADER_INCLUDED
    #define XSHUTDWN_HEADER_INCLUDED

    /********************************************************************
     *
     *   Declarations
     *
     ********************************************************************/

    /*
     *@@ SHUTDOWNPARAMS:
     *      shutdown structure for external calls.
     *      This is also used internally in shutdown.c.
     *
     *@@changed V0.9.5 (2000-08-10) [umoeller]: changed ulRestartWPS
     */

    typedef struct _SHUTDOWNPARAMS
    {
        BOOL        optReboot,
                    optConfirm,
                    optDebug;
        ULONG       ulRestartWPS;
            // changed V0.9.5 (2000-08-10) [umoeller]:
            // restart WPS flag, meaning:
            // -- 0: no, do shutdown
            // -- 1: yes, restart WPS
            // -- 2: yes, restart WPS, but logoff also
            //          (only if XWPSHELL is running)
        BOOL        optWPSCloseWindows,
                    optAutoCloseVIO,
                    optLog,
                    // optAnimate,          // removed V0.9.12 (2001-05-12) [umoeller]
                    optAPMPowerOff,
                    optAPMDelay,
                    optWPSReuseStartupFolder,
                    optEmptyTrashCan,
                    optWarpCenterFirst;
        CHAR        szRebootCommand[CCHMAXPATH];
    } SHUTDOWNPARAMS, *PSHUTDOWNPARAMS;

    // traffic light animation
    #define XSD_ANIM_COUNT 8            // animation steps
    typedef struct _SHUTDOWNANIM {
        HPOINTER    ahptr[XSD_ANIM_COUNT];
    } SHUTDOWNANIM, *PSHUTDOWNANIM;

    /*
     *@@ AUTOCLOSELISTITEM:
     *      this is the user-defined list of auto-close items
     */

    typedef struct _AUTOCLOSELISTITEM
    {
        CHAR                szItemName[100];
        USHORT              usAction;
    } AUTOCLOSELISTITEM, *PAUTOCLOSELISTITEM;

    // auto-close actions
    #define ACL_CTRL_C              1
    #define ACL_WMCLOSE             2
    #define ACL_KILLSESSION         3
    #define ACL_SKIP                4

    #ifdef SOM_WPObject_h
    #ifdef LINKLIST_HEADER_INCLUDED

        typedef struct SHUTLIST_STRUCT
        {
            SWCNTRL                 swctl;          // system tasklist structure (see PM ref.)
            WPObject                *pObject;       // NULL for non-WPS windows
            CHAR                    szClass[100];   // window class of the task's main window
            LONG                    lSpecial;       // XSD_* flags; > 0 if Desktop or WarpCenter
        } SHUTLISTITEM, *PSHUTLISTITEM;

        /*
         *@@ AUTOCLOSEWINDATA:
         *      data structure used in the "Auto close"
         *      dialog (stored in QWL_USER).
         */

        typedef struct _AUTOCLOSEWINDATA
        {
            PLINKLIST           pllAutoClose;       // changed (V0.9.0)
            SHORT               sSelected;
            PAUTOCLOSELISTITEM  pliSelected;
            USHORT              usItemCount;
            /* LINKLIST           SHUTLISTITEM llShut
                                psliLast; */
        } AUTOCLOSEWINDATA, *PAUTOCLOSEWINDATA;

        /*
         * REBOOTLISTITEM:
         *      data structure for user reboot items.
         */

        typedef struct _REBOOTLISTITEM
        {
            CHAR                szItemName[100];
            CHAR                szCommand[CCHMAXPATH];
        } REBOOTLISTITEM, *PREBOOTLISTITEM;

        #ifdef DOSH_HEADER_INCLUDED
        /*
         *@@ REBOOTWINDATA:
         *      data structure used in the "Reboot items"
         *      dialog (stored in QWL_USER).
         */

        typedef struct _REBOOTWINDATA
        {
            PLINKLIST       pllReboot;              // changed (V0.9.0)
            SHORT           sSelected;
            PREBOOTLISTITEM pliSelected;
            USHORT          usItemCount;

            /* PPARTITIONINFO  ppi;                   // partitions list (V0.9.0)
            USHORT          usPartitions; */

            PPARTITIONSLIST pPartitionsList;        // new partitions list
                                                    // V0.9.9 (2001-04-07) [umoeller]
            BOOL            fPartitionsLoaded;      // TRUE after first attempt,
                                                    // even if failed
        } REBOOTWINDATA, *PREBOOTWINDATA;
        #endif

    #endif // LINKLIST_HEADER_INCLUDED
    #endif // SOM_WPObject_h

    /* ******************************************************************
     *
     *   Shutdown interface
     *
     ********************************************************************/

    VOID xsdQueryShutdownSettings(PSHUTDOWNPARAMS psdp);

    BOOL xsdIsShutdownRunning(VOID);

    BOOL xsdInitiateShutdown(VOID);

    BOOL xsdInitiateRestartWPS(BOOL fLogoff);

    BOOL xsdInitiateShutdownExt(PSHUTDOWNPARAMS psdp);

    /* ******************************************************************
     *
     *   Shutdown settings pages
     *
     ********************************************************************/

    #ifdef NOTEBOOK_HEADER_INCLUDED
        VOID XWPENTRY xsdShutdownInitPage(PCREATENOTEBOOKPAGE pcnbp,
                                          ULONG flFlags);

        MRESULT XWPENTRY xsdShutdownItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                       ULONG ulItemID,
                                       USHORT usNotifyCode,
                                       ULONG ulExtra);
    #endif

    /* ******************************************************************
     *
     *   Shutdown helper functions
     *
     ********************************************************************/

    VOID xsdLoadAnimation(PSHUTDOWNANIM psda);

    VOID xsdFreeAnimation(PSHUTDOWNANIM psda);

    VOID xsdRestartWPS(HAB hab,
                       BOOL fLogoff);

    APIRET xsdFlushWPS2INI(VOID);

    #ifdef INCL_WINSWITCHLIST
    #ifdef KERNEL_HEADER_INCLUDED
    #ifdef SOM_WPObject_h

        #define XSD_SYSTEM          -1
        #define XSD_INVISIBLE       -2
        #define XSD_DEBUGNEED       -3

        #define XSD_DESKTOP         1
        #define XSD_WARPCENTER      2

        typedef struct _SHUTDOWNCONSTS
        {
            PCKERNELGLOBALS pKernelGlobals;
            SOMClass        *pWPDesktop;
            WPObject        *pActiveDesktop;
            HWND            hwndActiveDesktop;
            HWND            hwndOpenWarpCenter; // OPEN_RUNNING view of open WarpCenter

            PID             pidWPS,     // WinQueryWindowProcess(G_hwndMain, &G_pidWPS, NULL);
                            pidPM;      // WinQueryWindowProcess(HWND_DESKTOP, &G_pidPM, NULL);

            HWND            hwndMain,
                                    // dlg with listbox (visible only in debug mode)
                            hwndShutdownStatus;
                                    // status window (always visible)
            HWND            hwndVioDlg;
                                    // if != NULLHANDLE, currently open VIO confirmation
                                    // window
        } SHUTDOWNCONSTS, *PSHUTDOWNCONSTS;

        VOID xsdGetShutdownConsts(PSHUTDOWNCONSTS pConsts);

        LONG xsdIsClosable(HAB hab,
                           PSHUTDOWNCONSTS pConsts,
                           SWENTRY *pSwEntry,
                           WPObject **ppObject);
    #endif
    #endif
    #endif

    /* ******************************************************************
     *
     *   XShutdown dialogs
     *
     ********************************************************************/

    ULONG xsdConfirmShutdown(PSHUTDOWNPARAMS psdParms);

    ULONG xsdConfirmRestartWPS(PSHUTDOWNPARAMS psdParms);

    MRESULT EXPENTRY fnwpAutoCloseDetails(HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2);

    MRESULT EXPENTRY fnwpUserRebootOptions(HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2);

    /* ******************************************************************
     *
     *   Shutdown thread
     *
     ********************************************************************/

    #ifdef THREADS_HEADER_INCLUDED
    void _Optlink fntShutdownThread(PTHREADINFO pti);
    #endif

    /* ******************************************************************
     *
     *   Window list debugging (winlist.c)
     *
     ********************************************************************/

    HWND winlCreateWinListWindow(VOID);

#endif
