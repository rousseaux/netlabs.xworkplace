
/*
 * xwpdaemn.h:
 *      PageMage and daemon declarations.
 *      These are not visible to the hook nor
 *      to XFLDR.DLL.
 *
 *      Requires xwphook.h to be included first.
 *
 *@@include #define INCL_DOSSEMAPHORES
 *@@include #include <os2.h>
 *@@include #include "hook\xwphook.h"
 *@@include #include "hook\xwpdaemn.h"
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

    #define TIMEOUT_HMTX_WINLIST    20*1000
                // raised V0.9.12 (2001-05-31) [umoeller]

    // move hotkey flags
    #define MOVE_LEFT           1
    #define MOVE_RIGHT          2
    #define MOVE_UP             4
    #define MOVE_DOWN           8

    // font ID to use for the PageMage window titles;
    // we always use the same one, because there's only one
    // in the daemon process
    #define LCID_PAGEMAGE_FONT  ((ULONG)1)

    // window types
    // these have been moved to hook_private.h because
    // the hook also needs them
    #define WINDOW_NORMAL       0x0000
    #define WINDOW_PAGEMAGE     0x0001      // some PageMage window, always sticky
    #define WINDOW_WPSDESKTOP   0x0002      // WPS desktop, always sticky
    #define WINDOW_STICKY       0x0003      // window is on sticky list
    #define WINDOW_MINIMIZE     0x0005      // window is minimized, treat as sticky
    #define WINDOW_MAXIMIZE     0x0006      // window is maximized; hide when moving
    #define WINDOW_RESCAN       0x0008

    /*
     *@@ PGMGWININFO:
     *      one of these exists for every window
     *      which is currently handled by PageMage.
     *
     *      This was completely revamped with V0.9.7.
     *      PageMage no longer uses a global fixed
     *      array of these items, but maintains a
     *      linked list now. I believe some of the
     *      pagemage hangs we had previously were
     *      due to an overflow of the global array.
     *
     *@@added V0.9.7 (2001-01-21) [umoeller]
     */

    typedef struct _PGMGWININFO
    {
        HWND        hwnd;           // window handle
        BYTE        bWindowType;
        CHAR        szSwitchName[30];
        CHAR        szClassName[30];
        ULONG       pid;
        ULONG       tid;
        SWP         swp;
    } PGMGWININFO, *PPGMGWININFO;

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
        VOID _Optlink fntMoveThread(PTHREADINFO pti);
    #endif
    BOOL pgmmMoveIt(LONG, LONG, BOOL);
    BOOL pgmmZMoveIt(LONG, LONG);
    VOID pgmmRecoverAllWindows(VOID);

    // pgmg_settings.c
    VOID pgmsSetDefaults(VOID);
    BOOL pgmsLoadSettings(ULONG flConfig);
    BOOL pgmsSaveSettings(VOID);

    // pgmg_winscan.c
    APIRET pgmwInit(VOID);

    BOOL pgmwLock(VOID);

    VOID pgmwUnlock(VOID);

    PPGMGWININFO pgmwFindWinInfo(HWND hwndThis,
                                 PVOID *ppListNode);

    BOOL pgmwFillWinInfo(HWND hwnd,
                         PPGMGWININFO pWinInfo);

    VOID pgmwScanAllWindows(VOID);

    VOID pgmwAppendNewWinInfo(HWND hwnd);

    VOID pgmwDeleteWinInfo(HWND hwnd);

    VOID pgmwUpdateWinInfo(HWND hwnd);

    BOOL pgmwWindowListRescan(VOID);

    BOOL pgmwStickyCheck(const char *pcszSwitchName);

    BOOL pgmwSticky2Check(HWND hwndText);

    HWND pgmwGetWindowFromClientPoint(ULONG ulX,
                                      ULONG ulY);

    /* ******************************************************************
     *                                                                  *
     *   Global variables in xwpdaemn.c                                 *
     *                                                                  *
     ********************************************************************/

    ULONG               G_pidDaemon;
    extern HAB          G_habDaemon;
    extern PHOOKDATA    G_pHookData;
    extern PXWPGLOBALSHARED G_pXwpGlobalShared;

    extern HPOINTER     G_hptrDaemon;

    #ifdef LINKLIST_HEADER_INCLUDED
        extern LINKLIST     G_llWinInfos;
    #endif

    extern POINTL       G_ptlCurrPos;
    extern SIZEL        G_szlEachDesktopReal;
    extern SIZEL        G_szlEachDesktopInClient;
    extern BOOL         G_bConfigChanged;
    extern SWP          G_swpPgmgFrame;
#endif

