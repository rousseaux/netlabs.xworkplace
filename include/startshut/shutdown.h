
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
     *                                                                  *
     *   Declarations                                                   *
     *                                                                  *
     ********************************************************************/

    /*
     *@@ SHUTDOWNPARAMS:
     *      shutdown structure for external calls
     */

    typedef struct _SHUTDOWNPARAMS
    {
        BOOL        optReboot,
                    optConfirm,
                    optDebug,
                    optRestartWPS,
                    optWPSCloseWindows,
                    optAutoCloseVIO,
                    optLog,
                    optAnimate,
                    optAPMPowerOff,
                    optWPSReuseStartupFolder;
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
            PPARTITIONINFO  ppi;                   // partitions list (V0.9.0)
            USHORT          usPartitions;
        } REBOOTWINDATA, *PREBOOTWINDATA;
        #endif

    #endif
    #endif

    /* ******************************************************************
     *                                                                  *
     *   Shutdown interface                                             *
     *                                                                  *
     ********************************************************************/

    BOOL xsdInitiateShutdown(VOID);

    BOOL xsdInitiateRestartWPS(VOID);

    BOOL xsdInitiateShutdownExt(PSHUTDOWNPARAMS psdp);

    /* ******************************************************************
     *                                                                  *
     *   Shutdown settings pages                                        *
     *                                                                  *
     ********************************************************************/

    #ifdef NOTEBOOK_HEADER_INCLUDED
        VOID xsdShutdownInitPage(PCREATENOTEBOOKPAGE pcnbp,
                                  ULONG flFlags);

        MRESULT xsdShutdownItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                        USHORT usItemID,
                                        USHORT usNotifyCode,
                                        ULONG ulExtra);
    #endif

    /* ******************************************************************
     *                                                                  *
     *   Shutdown helper functions                                      *
     *                                                                  *
     ********************************************************************/

    VOID xsdLoadAnimation(PSHUTDOWNANIM psda);

    VOID xsdFreeAnimation(PSHUTDOWNANIM psda);

    VOID xsdRestartWPS(VOID);

    APIRET xsdFlushWPS2INI(VOID);

    /* ******************************************************************
     *                                                                  *
     *   XShutdown dialogs                                              *
     *                                                                  *
     ********************************************************************/

    ULONG xsdConfirmShutdown(PSHUTDOWNPARAMS psdParms);

    ULONG xsdConfirmRestartWPS(PSHUTDOWNPARAMS psdParms);

    MRESULT EXPENTRY fnwpAutoCloseDetails(HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2);

    MRESULT EXPENTRY fnwpUserRebootOptions(HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2);

    /* ******************************************************************
     *                                                                  *
     *   Shutdown thread                                                *
     *                                                                  *
     ********************************************************************/

    void _Optlink fntShutdownThread(PVOID ptiMyself);

#endif
