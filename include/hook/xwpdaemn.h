
/*
 * xwpdaemn.h:
 *      PageMage and daemon declarations.
 *      These are not visible to the hook nor
 *      to XFLDR.DLL.
 *      Requires xwphook.h to be included first.
 */

/*
 *      Copyright (C) 1995-1999 Carlos Ugarte.
 *      Copyright (C) 2000 Ulrich M�ller.
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

    #define PAGEMAGE_MUTEX      "\\SEM32\\PAGEMAGE"
    #define PAGEMAGE_WNDLSTMTX  "\\SEM32\\PGMG_WNDLSTMTX"
    #define PAGEMAGE_WNDLSTEV   "\\SEM32\\PGMG_WNDLSTEV"
    #define PAGEMAGE_WNDQUEUE   "\\QUEUES\\PAGEMAGE"
    #define DLL_VERSION         6

    #define INI_OK              100
    #define INI_CREATED         101
    #define INI_FILEPROBS       102

    /* Which move hotkey was used */
    #define MOVE_LEFT           1
    #define MOVE_RIGHT          2
    #define MOVE_UP             4
    #define MOVE_DOWN           8

    #define LCID_FONT                   ((ULONG) 1)

    /* Window types */
    // these have been moved to hook_private.h because
    // the hook also needs them
    #define WINDOW_NORMAL       0
    #define WINDOW_PAGEMAGE     1
    #define WINDOW_WPS          2
    #define STICKY_PERMANENT    3
    #define STICKY_TEMPORARY    4
    #define WINDOW_MINIMIZED    5
    #define WINDOW_RESCAN       7

    #define WINDOW_SPECIAL      99

    /* #define WINDOW_INVISIBLE    8 */

    /*
     *@@ HWNDLIST:
     *      one of these exists for every window
     *      which is currently handled by PageMage.
     */

    typedef struct _HWNDLIST
    {
        HWND    hwnd;
        BYTE    bWindowType;
        CHAR    szWindowName[30];
        CHAR    szSwitchName[30];
        CHAR    szClassName[30];
        ULONG   pid;
        ULONG   tid;
        SWP     swp;
    } HWNDLIST;

    typedef struct _CONFIG_HWND
    {
        HWND    hwndNotebook;
        HWND    hwndPage1;
        HWND    hwndPage2;
        HWND    hwndPage3;
        HWND    hwndPage4;
        HWND    hwndPage5;
        HWND    hwndPage6;
        HWND    hwndPage7;
        HWND    hwndPage8;
    } CONFIG_HWND;

    /* Window Procedures */
    VOID    _Optlink    fntWindowScanThread(PVOID);

    // xwpdaemn.c
    VOID                dmnKillPageMage(BOOL fNotifyKernel);

    // pgmg_control.c
    LONG                pgmcCalcNewFrameCY(LONG cx);
    MRESULT EXPENTRY    fnwpPageMageClient(HWND, ULONG, MPARAM, MPARAM);
    MRESULT EXPENTRY    fnwpSubclPageMageFrame(HWND, ULONG, MPARAM, MPARAM);
    VOID                pgmcSetPgmgFramePos(HWND);
    BOOL                pgmcCreateMainControlWnd(VOID);

    // pgmg_move.c
    VOID    _Optlink    fntMoveThread(PVOID);
    INT                 pgmmMoveIt(LONG, LONG, BOOL);
    INT                 pgmmZMoveIt(LONG, LONG);
    VOID                pgmmRecoverAllWindows(VOID);

    // pgmg_settings.c
    VOID                pgmsSetDefaults(VOID);

    // pgmg_winscan.c
    BOOL                pgmwGetWinInfo(HWND hwnd,
                                       HWNDLIST *phl);
    VOID                pgmwScanAllWindows(VOID);
    VOID                pgmwWindowListAdd(HWND hwnd);
    VOID                pgmwWindowListDelete(HWND hwnd);
    BOOL                pgmwWindowListRescan(VOID);
    BOOL                pgmwStickyCheck(PCHAR, PCHAR);
    BOOL                pgmwSticky2Check(HWND);
    HWND                pgmwGetWindowFromClientPoint(ULONG, ULONG);

    /* ******************************************************************
     *                                                                  *
     *   Global variables in xwpdaemn.c                                 *
     *                                                                  *
     ********************************************************************/

    extern HAB          G_habDaemon;
    extern PHOOKDATA    G_pHookData;
    extern PDAEMONSHARED G_pDaemonShared;

    extern HPOINTER     G_hptrDaemon;

    extern HWNDLIST     G_MainWindowList[MAX_WINDOWS];
    extern USHORT       G_usWindowCount;

    extern HMTX         G_hmtxPageMage;
    extern HMTX         G_hmtxWindowList;
    extern HEV          G_hevWindowList;
    extern HQUEUE       G_hqPageMage;
    extern POINTL       G_ptlCurrPos;
    extern POINTL       G_ptlMoveDeltaPcnt;
    extern POINTL       G_ptlMoveDelta;
    extern POINTL       G_ptlPagerSize;
    extern POINTL       G_ptlEachDesktop;
    extern BOOL         G_bConfigChanged;
    extern BOOL         G_bTitlebarChange;
    extern SWP          G_swpPgmgFrame;
    extern CHAR         G_chArg0[64];
    extern CHAR         G_szFacename[TEXTLEN];
    extern POINTL       G_ptlWhich;
    extern SWP          G_swpRecover;

    extern PFNWP        G_pfnOldFrameWndProc;

    extern PAGEMAGECONFIG G_PageMageConfig;

#endif
