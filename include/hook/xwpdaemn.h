
/*
 * xwpdaemn.h:
 *      PageMage and daemon declarations.
 *      These are not visible to the hook nor
 *      to XFLDR.DLL.
 *      Requires xwphook.h to be included first.
 *
 *@@include #define INCL_DOSSEMAPHORES
 *@@include #include <os2.h>
 */

/*
 *      Copyright (C) 1995-1999 Carlos Ugarte.
 *      Copyright (C) 2000 Ulrich M”ller.
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, in version 2 as it comes in the COPYING
 *      file of the XWorkplace main distribution.
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 */

#ifndef PAGEMAGE_HEADER_INCLUDED
    #define PAGEMAGE_HEADER_INCLUDED

    #define DLL_VERSION         6

    #define INI_OK              100
    #define INI_CREATED         101
    #define INI_FILEPROBS       102

    /* Which move hotkey was used */
    #define MOVE_LEFT           1
    #define MOVE_RIGHT          2
    #define MOVE_UP             4
    #define MOVE_DOWN           8

    // font ID to use for the PageMage window titles;
    // we always use the same one, because there's only one
    // in the daemon process
    #define LCID_PAGEMAGE_FONT  ((ULONG) 1)

    /* Window types */
    // these have been moved to hook_private.h because
    // the hook also needs them
    #define WINDOW_NORMAL       0x0000
    #define WINDOW_PAGEMAGE     0x0001      // some PageMage window, always sticky
    #define WINDOW_WPSDESKTOP   0x0002      // WPS desktop, always sticky
    #define WINDOW_STICKY       0x0003      // window is on sticky list
    #define WINDOW_MINIMIZE     0x0005      // window is minimized, treat as sticky
    #define WINDOW_MAXIMIZE     0x0006      // window is maximized; hide when moving
    #define WINDOW_MAX_OFF      0x0007      // window was maximized and has been hidden by us
    #define WINDOW_RESCAN       0x0008

    /*
     *@@ HWNDLIST:
     *      one of these exists for every window
     *      which is currently handled by PageMage.
     */

    typedef struct _HWNDLIST
    {
        HWND    hwnd;
        BYTE    bWindowType;
        // CHAR    szWindowName[30];
        CHAR    szSwitchName[30];
        CHAR    szClassName[30];
        ULONG   pid;
        ULONG   tid;
        SWP     swp;
    } HWNDLIST;

    // xwpdaemn.c
    VOID                dmnKillPageMage(BOOL fNotifyKernel);

    // pgmg_control.c
    BOOL pgmcCreateMainControlWnd(VOID);
    LONG pgmcCalcNewFrameCY(LONG cx);
    VOID pgmcSetPgmgFramePos(HWND);
    USHORT pgmgcStartFlashTimer(VOID);
    MRESULT EXPENTRY fnwpPageMageClient(HWND, ULONG, MPARAM, MPARAM);
    MRESULT EXPENTRY fnwpSubclPageMageFrame(HWND, ULONG, MPARAM, MPARAM);

    // pgmg_move.c
    #ifdef THREADS_HEADER_INCLUDED
        VOID _Optlink fntMoveQueueThread(PTHREADINFO pti);
    #endif
    INT pgmmMoveIt(LONG, LONG, BOOL);
    INT pgmmZMoveIt(LONG, LONG);
    VOID pgmmRecoverAllWindows(VOID);

    // pgmg_settings.c
    VOID pgmsSetDefaults(VOID);
    BOOL pgmsLoadSettings(ULONG flConfig);
    BOOL pgmsSaveSettings(VOID);

    // pgmg_winscan.c
    BOOL pgmwGetWinInfo(HWND hwnd,
                        HWNDLIST *phl);
    VOID pgmwScanAllWindows(VOID);
    VOID pgmwWindowListAdd(HWND hwnd);
    VOID pgmwWindowListDelete(HWND hwnd);
    BOOL pgmwWindowListRescan(VOID);
    BOOL pgmwStickyCheck(/* PCHAR, */ PCHAR);
    BOOL pgmwSticky2Check(HWND);
    HWND pgmwGetWindowFromClientPoint(ULONG, ULONG);

    /* ******************************************************************
     *                                                                  *
     *   Global variables in xwpdaemn.c                                 *
     *                                                                  *
     ********************************************************************/

    ULONG               G_pidDaemon;
    extern HAB          G_habDaemon;
    extern PHOOKDATA    G_pHookData;
    extern PDAEMONSHARED G_pDaemonShared;

    extern HPOINTER     G_hptrDaemon;

    extern HWNDLIST     G_MainWindowList[MAX_WINDOWS];
    extern USHORT       G_usWindowCount;

    extern HMTX         G_hmtxWindowList;
    extern POINTL       G_ptlCurrPos;
    extern POINTL       G_ptlPgmgClientSize;
    extern POINTL       G_ptlEachDesktop;
    extern BOOL         G_bConfigChanged;
    extern SWP          G_swpPgmgFrame;
    extern CHAR         G_szFacename[PGMG_TEXTLEN];
#endif

