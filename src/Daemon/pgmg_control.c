
/*
 *@@sourcefile pgmg_control.c:
 *      PageMage Desktop control window.
 *
 *      PageMage was originally written by Carlos Ugarte
 *      and implemented its own system hook. This was
 *      integrated into the XWP hook, while the PageMage
 *      window (the pager) and the move handling were
 *      in a separate PM program, which is now running
 *      in XWPDAEMN.EXE.
 *
 *      Basically, PageMage consists of the following
 *      components:
 *
 *      --  The PageMage control window (the pager).
 *          For one, this paints the representation
 *          of all virtual desktops in its client.
 *
 *          In order to be able to do this, it receives
 *          messages from the XWP hook whenever frame
 *          windows are created, moved, renamed, or
 *          destroyed.
 *
 *      --  PageMage keeps its own window list to be
 *          able to quickly trace all windows and their
 *          positions without having to query the entire
 *          PM switch list or enumerate all desktop windows
 *          all the time. As a result, PageMage's CPU load
 *          is pretty low.
 *
 *          The code for this is in pgmg_winscan.c.
 *
 *      --  The PageMage "move thread", which is a second
 *          thread which gets started when the pager is
 *          created. This is in pgmg_move.c.
 *
 *          This thread is reponsible for actually switching
 *          desktops. Switching desktops is actually done by
 *          moving all windows (except the sticky ones). So
 *          when the user switches one desktop to the right,
 *          all windows are actually moved to the left by the
 *          size of the PM screen.
 *
 *          There are several occasions when the move thread
 *          gets notifications to move all windows (i.e. to
 *          switch desktops).
 *
 *          For one, this happens when the XWP hook detects
 *          an active window change. The move thread is notified
 *          of all such changes and will then check if the
 *          newly activated window is currently off-screen
 *          (i.e. "on another desktop") and will then switch
 *          to that desktop.
 *
 *          Of course, switching desktops can also be intitiated
 *          by the user (using the hotkeys or by clicking into
 *          the pager).
 *
 *      --  As with the rest of the daemon, PageMage receives
 *          notifications from XFLDR.DLL when its settings
 *          have been modified in the "Screen" settings notebook.
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


/*
 *  Desktop Arrangement (4,3):
 *
 *  ÚÄÄÄÂÄÄÄÂÄÄÄÂÄÄÄ¿
 *  ³0,2³1,2³2,2³3,2³
 *  ÃÄÄÄÅÄÄÄÅÄÄÄÅÄÄÄ´
 *  ³0,1³1,1³2,1³3,1³
 *  ÃÄÄÄÅÄÄÄÅÄÄÄÅÄÄÄ´
 *  ³0,0³1,0³2,0³3,0³
 *  ÀÄÄÄÁÄÄÄÁÄÄÄÁÄÄÄÙ
 *
 */

#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#define INCL_DOSEXCEPTIONS
#define INCL_DOSERRORS

#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WINMESSAGEMGR
#define INCL_WININPUT
#define INCL_WINSYS
#define INCL_WINSHELLDATA
#define INCL_WINTIMER
#define INCL_WINTRACKRECT

#define INCL_GPICONTROL
#define INCL_GPIPRIMITIVES
#define INCL_GPILOGCOLORTABLE
#define INCL_GPIBITMAPS
#define INCL_GPILCIDS
#include <os2.h>

#include <stdio.h>
#include <time.h>
#include <setjmp.h>

#include "setup.h"                      // code generation and debugging options

#include "helpers\except.h"             // exception handling
#include "helpers\linklist.h"           // linked list helper routines

#include "hook\xwphook.h"
#include "hook\hook_private.h"
#include "hook\xwpdaemn.h"              // PageMage and daemon declarations

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

#define WNDCLASS_PAGEMAGECLIENT         "XWPPageMageClient"

PFNWP       G_pfnOldFrameWndProc = 0;
// BOOL        G_ClientBitmapNeedsUpdate = FALSE;
        // put this into PAGEMAGECLIENTDATA V0.9.7 (2001-01-18) [umoeller]

ULONG       G_hUpdateTimer = NULLHANDLE;

/* ******************************************************************
 *
 *   Debugging
 *
 ********************************************************************/

/*
 *@@ DumpAllWindows:
 *
 */

/* VOID DumpAllWindows(VOID)
{
    ULONG  usIdx;
    if (WinRequestMutexSem(G_hmtxWindowList, TIMEOUT_HMTX_WINLIST)
            == NO_ERROR)
    {
        for (usIdx = 0; usIdx < G_usWindowCount; usIdx++)
        {
            PPGMGLISTENTRY pEntryThis = &G_MainWindowList[usIdx];
            _Pmpf(("Dump %d: hwnd 0x%lX \"%s\":\"%s\" pid 0x%lX(%d) type %d",
                   usIdx,
                   pEntryThis->hwnd,
                   pEntryThis->szSwitchName,
                   pEntryThis->szClassName,
                   pEntryThis->pid,
                   pEntryThis->pid,
                   pEntryThis->bWindowType));
        }

        DosReleaseMutexSem(G_hmtxWindowList);
    }
} */

/* ******************************************************************
 *
 *   PageMage control window
 *
 ********************************************************************/

/*
 *@@ pgmcCreateMainControlWnd:
 *      creates the PageMage control window (frame
 *      and client). This gets called by dmnStartPageMage
 *      on thread 1 when PageMage has been enabled.
 *
 *      This registers the PageMage client class
 *      using fnwpPageMageClient, which does most of
 *      the work for PageMage.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 *@@changed V0.9.3 (2000-05-22) [umoeller]: the hook was interfering, fixed
 */

BOOL pgmcCreateMainControlWnd(VOID)
{
    BOOL    brc = TRUE;

    _Pmpf(("Entering pgmcCreateMainControlWnd"));

    if (G_pHookData->hwndPageMageFrame == NULLHANDLE)
    {
        ULONG   flFrameFlags    = FCF_TITLEBAR |
                                  FCF_SYSMENU |
                                  FCF_HIDEBUTTON |
                                  FCF_NOBYTEALIGN |
                                  FCF_SIZEBORDER;
        _Pmpf(("Creating pagemage"));

        WinRegisterClass(G_habDaemon,
                         WNDCLASS_PAGEMAGECLIENT,
                         (PFNWP)fnwpPageMageClient,
                         0,
                         0);

        // disable message processing in the hook
        G_pHookData->fDisableSwitching = TRUE;

        G_pHookData->hwndPageMageFrame
            = WinCreateStdWindow(HWND_DESKTOP,
                                 0, // WS_ANIMATE,  // style
                                 &flFrameFlags,
                                 (PSZ)WNDCLASS_PAGEMAGECLIENT,
                                 "XWorkplace PageMage",
                                 0,
                                 0,
                                 1000,    // ID...
                                 &G_pHookData->hwndPageMageClient);

        if (!G_pHookData->hwndPageMageFrame)
            _Pmpf(("PageMage window creation failed...."));
        else
        {
            // SWP     swpPager;

            // shortcuts to global pagemage config
            PPAGEMAGECONFIG pPageMageConfig = &G_pHookData->PageMageConfig;
            PPOINTL         pptlMaxDesktops = &pPageMageConfig->ptlMaxDesktops;

            // set frame icon
            WinSendMsg(G_pHookData->hwndPageMageFrame,
                       WM_SETICON,
                       (MPARAM)G_hptrDaemon,
                       NULL);

            // subclass frame
            _Pmpf(("subclassing pagemage 0x%lX", G_pHookData->hwndPageMageFrame));

            G_pfnOldFrameWndProc = WinSubclassWindow(G_pHookData->hwndPageMageFrame,
                                                     fnwpSubclPageMageFrame);

            pgmcSetPgmgFramePos(G_pHookData->hwndPageMageFrame);

            brc = TRUE;
        }

        G_pHookData->fDisableSwitching = FALSE;
    }

    return (brc);
}

/*
 *@@ pgmcCalcNewFrameCY:
 *      calculates the new frame height according to
 *      the frame width, so that the frame can be
 *      sized proportionally.
 *
 *@@added V0.9.2 (2000-02-23) [umoeller]
 *@@changed V0.9.3 (2000-04-09) [umoeller]: now taking titlebar setting into account
 */

LONG pgmcCalcNewFrameCY(LONG cx)
{
    // shortcuts to global pagemage config
    PPAGEMAGECONFIG pPageMageConfig = &G_pHookData->PageMageConfig;
    PPOINTL         pptlMaxDesktops = &pPageMageConfig->ptlMaxDesktops;

    LONG lHeightPercentOfWidth = G_pHookData->lCYScreen * 100 / G_pHookData->lCXScreen;
                // e.g. 75 for 1024x768
    LONG lCY =      (  (cx * lHeightPercentOfWidth / 100)
                       * pptlMaxDesktops->y
                       / pptlMaxDesktops->x
                    );
    if (pPageMageConfig->fShowTitlebar)
        lCY += WinQuerySysValue(HWND_DESKTOP, SV_CYTITLEBAR);

    return (lCY);
}

/*
 *@@ pgmcSetPgmgFramePos:
 *      sets the position of the PageMage window.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 *@@changed V0.9.4 (2000-08-08) [umoeller]: now restoring window pos properly
 */

VOID pgmcSetPgmgFramePos(HWND hwnd)
{
    // RECTL   rcl;
    ULONG   flOptions;

    ULONG   cb = sizeof(G_swpPgmgFrame);

    // disable message processing in the hook;
    // not neccessary because the hook already checks
    // for the pagemage frame.
    // G_pHookData->fDisableSwitching = TRUE;

    PrfQueryProfileData(HINI_USER,
                        INIAPP_XWPHOOK,
                        INIKEY_HOOK_PGMGWINPOS,
                        &G_swpPgmgFrame,
                        &cb);
            // might fail

    /* rcl.xLeft   = G_swpPgmgFrame.x;
    rcl.yBottom = G_swpPgmgFrame.y;
    rcl.xRight  = G_swpPgmgFrame.cx + G_swpPgmgFrame.x;
    rcl.yTop    = G_swpPgmgFrame.cy + G_swpPgmgFrame.y; */

    flOptions = SWP_MOVE | SWP_SIZE
                    | SWP_SHOW; // V0.9.4 (2000-07-10) [umoeller]

    WinSetWindowPos(hwnd,
                    NULLHANDLE,
                    G_swpPgmgFrame.x,
                    G_swpPgmgFrame.y,
                    G_swpPgmgFrame.cx,
                    G_swpPgmgFrame.cy,
                    /* rcl.xLeft,
                    rcl.yBottom,
                    (rcl.xRight - rcl.xLeft),
                    (rcl.yTop - rcl.yBottom), */
                    flOptions);

    if (G_pHookData->PageMageConfig.fFlash)
        pgmgcStartFlashTimer();

    // G_pHookData->fDisableSwitching = fOld;

} // pgmcSetPgmgFramePos

/*
 *@@ pgmgcStartFlashTimer:
 *      shortcut to start the flash timer.
 *
 *@@added V0.9.4 (2000-07-10) [umoeller]
 */

USHORT pgmgcStartFlashTimer(VOID)
{
    return (WinStartTimer(G_habDaemon,
                          G_pHookData->hwndPageMageClient,
                          2,
                          G_pHookData->PageMageConfig.ulFlashDelay));
}

/* ******************************************************************
 *
 *   PageMage client window proc
 *
 ********************************************************************/

/*
 *@@ PAGEMAGECLIENTDATA:
 *      static (!) structure in fnwpPageMageClient
 *      holding all kinds of data needed for the client.
 *
 *      This replaces a whole bunch of global variables
 *      which used to be used with PageMage before V0.9.7.
 *
 *@@added V0.9.7 (2001-01-18) [umoeller]
 */

typedef struct _PAGEMAGECLIENTDATA
{
    SIZEL       szlClient;  // client's size

    HDC         hdcMem;     // memory DC for bitmap
    HPS         hpsMem;     // memory PS for bitmap

    HBITMAP     hbmClient;     // the bitmap for the client
    BITMAPINFOHEADER2    bmihMem;   // its bitmap info

    BOOL        fClientBitmapNeedsUpdate;
                // if this is set to TRUE, hbmClient is invalidated
                // on the next paint.

    CHAR        szFaceName[PGMG_TEXTLEN];
                // font face name

    // saved window handles for titlebar etc. changes:
    HWND        hwndSaveSysMenu,
                hwndSaveTitlebar,
                hwndSaveMinButton;
} PAGEMAGECLIENTDATA, *PPAGEMAGECLIENTDATA;

/*
 *@@ MINIWINDOW:
 *      representation of a WININFO entry in the
 *      PageMage client. An array of these is built
 *      in UpdateClientBitmap.
 *
 *@@added V0.9.7 (2001-01-21) [umoeller]
 */

typedef struct _MINIWINDOW
{
    HWND            hwnd;           // window handle

    POINTL          ptlBegin,
                    ptlEnd;         // calculated rectangle for mini window

    PPGMGWININFO    pWinInfo;       // ptr to PGMGWININFO this item
                                    // represents; always valid
} MINIWINDOW, *PMINIWINDOW;

/*
 *@@ UpdateClientBitmap:
 *      gets called when fnwpPageMageClient
 *      receives WM_PAINT and the paint bitmap has been
 *      marked as outdated (G_ClientBitmapNeedsUpdate == TRUE).
 *
 *      This draws the new contents of the PageMage
 *      client into the bitmap in the memory PS (in
 *      the client data).
 *
 *      That bitmap is then simply bit-blitted on the
 *      screen on WM_PAINT in fnwpPageMageClient.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 *@@changed V0.9.3 (2000-05-22) [umoeller]: fixed font problems
 *@@changed V0.9.4 (2000-08-08) [umoeller]: added special maximized window handling
 *@@changed V0.9.7 (2001-01-18) [umoeller]: changed prototype
 *@@changed V0.9.7 (2001-01-18) [umoeller]: fixed a serialization problem
 *@@changed V0.9.7 (2001-01-21) [umoeller]: now using linked list for wininfos
 *@@changed V0.9.7 (2001-01-21) [umoeller]: largely rewritten for speed
 */

VOID UpdateClientBitmap(PPAGEMAGECLIENTDATA pClientData)
{
    HPS             hpsMem = pClientData->hpsMem;

    POINTL          ptlDest;
    SWP             swpClient;

    ULONG           usIdx;

    // shortcuts to global pagemage config
    PPAGEMAGECONFIG pPageMageConfig = &G_pHookData->PageMageConfig;
    PPOINTL         pptlMaxDesktops = &pPageMageConfig->ptlMaxDesktops;

    PSIZEL          pszlClient = &pClientData->szlClient;

    G_szlEachDesktopInClient.cx = (pszlClient->cx - pptlMaxDesktops->x + 1)
                                   / pptlMaxDesktops->x;
    G_szlEachDesktopInClient.cy = (pszlClient->cy - pptlMaxDesktops->y + 1)
                                   / pptlMaxDesktops->y;

    /*
     *   (1) entire-client painting
     *
     */

    // draw main box - all Desktops:
    GpiSetColor(hpsMem, pPageMageConfig->lcNormal);
    ptlDest.x = ptlDest.y = 0;
    GpiMove(hpsMem, &ptlDest);
    WinQueryWindowPos(G_pHookData->hwndPageMageClient, &swpClient);
    ptlDest.x = swpClient.cx;
    ptlDest.y = swpClient.cy;
    GpiBox(hpsMem, DRO_OUTLINEFILL, &ptlDest, (LONG) 0, (LONG) 0);

    // paint "current" desktop in different color
    GpiSetColor(hpsMem, pPageMageConfig->lcCurrent);
    ptlDest.x = ( (float) G_ptlCurrPos.x / (float) G_szlEachDesktopReal.cx)
                * ((float) G_szlEachDesktopInClient.cx + 1)
                + 1;
    ptlDest.y = (  (   (float) (pptlMaxDesktops->y - 1)
                       * G_szlEachDesktopReal.cy
                       - G_ptlCurrPos.y
                   )
                  / (float) G_szlEachDesktopReal.cy)
                  * ((float) G_szlEachDesktopInClient.cy + 1)
                  + 2;
    GpiMove(hpsMem, &ptlDest);
    ptlDest.x += G_szlEachDesktopInClient.cx - 1;
    ptlDest.y += G_szlEachDesktopInClient.cy - 1;
    GpiBox(hpsMem, DRO_FILL, &ptlDest, (LONG) 0, (LONG) 0);

    // draw vertical lines
    GpiSetColor(hpsMem, pPageMageConfig->lcDivider);
    for (usIdx = 0;
         usIdx < pptlMaxDesktops->x - 1;
         usIdx++)
    {
        ptlDest.x = (G_szlEachDesktopInClient.cx + 1) * (usIdx + 1);
        ptlDest.y = 0;
        GpiMove(hpsMem, &ptlDest);
        ptlDest.x = (G_szlEachDesktopInClient.cx + 1) * (usIdx + 1);
        ptlDest.y = pszlClient->cy;
        GpiLine(hpsMem, &ptlDest);
    }

    // draw horizontal lines
    for (usIdx = 0;
         usIdx < pptlMaxDesktops->y - 1;
         usIdx++)
    {
        ptlDest.x = 0;
        ptlDest.y = pszlClient->cy
                    - (usIdx + 1)
                    * (G_szlEachDesktopInClient.cy + 1);
        GpiMove(hpsMem, &ptlDest);
        ptlDest.x = pszlClient->cx;
        ptlDest.y = pszlClient->cy
                    - (usIdx + 1)
                    * (G_szlEachDesktopInClient.cy + 1);
        GpiLine(hpsMem, &ptlDest);
    }

    /*
     *   (2) paint mini-windows
     *
     */

    if (pPageMageConfig->fMirrorWindows)
    {
        // lock the window list all the while we're doing this
        // V0.9.7 (2001-01-21) [umoeller]
        if (WinRequestMutexSem(G_hmtxWindowList, TIMEOUT_HMTX_WINLIST)
                == NO_ERROR)
        {
            ULONG           cWinInfos = lstCountItems(&G_llWinInfos);

            // PHWND           pahwndPaint[MAX_WINDOWS] =

            _Pmpf(("Got %d wininfos.", cWinInfos));

            if (cWinInfos)
            {
                // allocate array of windows to be painted...
                // we fill this while enumerating Desktop windows,
                // which is needed for two reasons:
                // 1)   our internal list doesn't have the Z-order right;
                // 2)   WinBeginEnumWindows enumerates the windows from top
                //      to bottom, so we need to paint them in reverse order
                //      because the topmost window must be painted last

                // we use as many entries as are on the main
                // WININFO list to be on the safe side, but
                // not all will be used
                PMINIWINDOW     paMiniWindows
                    = (PMINIWINDOW)malloc(cWinInfos * sizeof(MINIWINDOW));
                if (paMiniWindows)
                {
                    ULONG           cMiniWindowsUsed = 0;

                    HENUM           henum;
                    HWND            hwndThis;
                    BOOL            fNeedRescan = FALSE;

                    float           fScale_X,
                                    fScale_Y;

                    memset(paMiniWindows, 0, cWinInfos * sizeof(MINIWINDOW));

                    // set font to use; this font has been created on startup
                    GpiSetCharSet(hpsMem, LCID_PAGEMAGE_FONT);

                    usIdx = 0;
                    fScale_X = (float) ( pptlMaxDesktops->x
                                         * G_szlEachDesktopReal.cx
                                       ) / pszlClient->cx;
                    fScale_Y = (float) ( pptlMaxDesktops->y
                                         * G_szlEachDesktopReal.cy
                                       ) / pszlClient->cy;

                    // start enum
                    henum = WinBeginEnumWindows(HWND_DESKTOP);
                    while ((hwndThis = WinGetNextWindow(henum)) != NULLHANDLE)
                    {
                        BOOL            fPaint = TRUE;

                        // go thru local list and find the
                        // current enumeration window
                        PPGMGWININFO pWinInfo = pgmwFindWinInfo(hwndThis,
                                                                NULL);

                        if (pWinInfo)
                        {
                            // item is on list:
                            // update window pos in window list
                            WinQueryWindowPos(pWinInfo->hwnd,
                                              &pWinInfo->swp);

                            fPaint = TRUE;

                            if (pWinInfo->bWindowType == WINDOW_RESCAN)
                            {
                                // window wasn't fully scanned: rescan
                                if (!pgmwFillWinInfo(pWinInfo->hwnd,
                                                     pWinInfo))
                                    fNeedRescan = TRUE;
                            }
                            else
                            {
                                // paint window only if it's "normal" and visible
                                if (    (pWinInfo->bWindowType != WINDOW_NORMAL)
                                     && (pWinInfo->bWindowType != WINDOW_MAXIMIZE)
                                   )
                                    // window not visible or special window:
                                    // don't paint
                                    fPaint = FALSE;
                            }

                            if (!WinIsWindowVisible(pWinInfo->hwnd))
                                fPaint = FALSE;

                            if (fPaint)
                            {
                                // this window is to be painted:
                                // use a new item on the MINIWINDOW
                                // array then and calculate the
                                // mapping of the mini window

                                PMINIWINDOW pMiniWindowThis
                                    = &paMiniWindows[cMiniWindowsUsed++];

                                PSWP pswp = &pWinInfo->swp;

                                // store WININFO ptr; we hold the winlist
                                // locked all the time, so this is safe
                                pMiniWindowThis->pWinInfo = pWinInfo;

                                pMiniWindowThis->hwnd = hwndThis;

                                pMiniWindowThis->ptlBegin.x
                                    =   (pswp->x + G_ptlCurrPos.x)
                                      / fScale_X;

                                pMiniWindowThis->ptlBegin.y
                                    = (   (pptlMaxDesktops->y - 1)
                                        * G_szlEachDesktopReal.cy
                                        - G_ptlCurrPos.y + pswp->y
                                      )
                                      / fScale_Y;

                                pMiniWindowThis->ptlEnd.x
                                    =   (pswp->x + pswp->cx + G_ptlCurrPos.x)
                                      / fScale_X - 1;

                                pMiniWindowThis->ptlEnd.y
                                    = (    (pptlMaxDesktops->y - 1)
                                         * G_szlEachDesktopReal.cy
                                         - G_ptlCurrPos.y + pswp->y + pswp->cy
                                      )
                                      / fScale_Y - 1;

                            } // end if (fPaint)
                        } // end if (pEntryThis)
                    } // end while ((hwndThis = WinGetNextWindow(henum)) != NULLHANDLE)
                    WinEndEnumWindows(henum);

                    // now paint

                    if (cMiniWindowsUsed)
                    {
                        // we got something to paint:
                        // paint the mini windows in reverse order then
                        // (bottom to top)

                        HWND hwndLocalActive = WinQueryActiveWindow(HWND_DESKTOP);

                        // start with the last one
                        LONG lIdx = cMiniWindowsUsed - 1;

                        _Pmpf(("Got %d mini windows.", cMiniWindowsUsed));

                        while (lIdx >= 0)
                        {
                            PMINIWINDOW pMiniWindowThis = &paMiniWindows[lIdx];

                            _Pmpf(("Painting miniwin %d", lIdx));

                            if (pMiniWindowThis->hwnd == hwndLocalActive)
                                // this is the active window:
                                GpiSetColor(hpsMem, pPageMageConfig->lcCurrentApp);
                            else
                                GpiSetColor(hpsMem, pPageMageConfig->lcNormalApp);

                            GpiMove(hpsMem,
                                    &pMiniWindowThis->ptlBegin);
                            GpiBox(hpsMem,
                                   DRO_FILL,
                                   &(pMiniWindowThis->ptlEnd),
                                   0, 0);
                            GpiSetColor(hpsMem,
                                        pPageMageConfig->lcAppBorder);
                            GpiBox(hpsMem,
                                   DRO_OUTLINE,
                                   &pMiniWindowThis->ptlEnd,
                                   0, 0);

                            // draw window text too?
                            if (    (pPageMageConfig->fShowWindowText)
                                 && (pMiniWindowThis->pWinInfo)
                               )
                            {
                                PSZ pszSwitchName
                                    = pMiniWindowThis->pWinInfo->szSwitchName;
                                RECTL   rclText;
                                LONG    lTextColor;

                                rclText.xLeft = pMiniWindowThis->ptlBegin.x + 1;
                                rclText.yBottom = pMiniWindowThis->ptlBegin.y + 1;
                                rclText.xRight = pMiniWindowThis->ptlEnd.x - 1;
                                rclText.yTop = pMiniWindowThis->ptlEnd.y - 4;

                                if (pMiniWindowThis->hwnd == hwndLocalActive)
                                    // active:
                                    lTextColor = pPageMageConfig->lcTxtCurrentApp;
                                else
                                    lTextColor = pPageMageConfig->lcTxtNormalApp;

                                WinDrawText(hpsMem,
                                            strlen(pszSwitchName),
                                            pszSwitchName,
                                            &rclText,
                                            lTextColor,
                                            (LONG)0,
                                            DT_LEFT | DT_TOP /* | DT_WORDBREAK */);

                            } // end if (    (pPageMageConfig->fShowWindowText) ...

                            // go back in mini windows list
                            lIdx--;
                        } // end while (ulIdx >= 0)

                        // if we've found any windows with WINDOW_RESCAN set,
                        // go rescan all windows later
                        if (fNeedRescan)
                        {
                            WinPostMsg(G_pHookData->hwndPageMageClient,
                                       PGMG_WNDRESCAN,
                                       MPVOID,
                                       MPVOID);
                        }

                    } // end if (cMiniWindowsUsed)

                    free(paMiniWindows);

                } // end if (paMiniWindows)
            } // if (cWinInfos)

            DosReleaseMutexSem(G_hmtxWindowList);
        } // end if (WinRequestMutexSem(G_hmtxWindowList, TIMEOUT_HMTX_WINLIST)

        // unset font so we can delete it if it changes
        GpiSetCharSet(hpsMem, LCID_DEFAULT);
        // GpiDeleteSetId(hps, LCID_FONT); */
    } // bShowWindows
} // PaintProc

/*
 *@@ TrackWithinPager:
 *
 *@@added V0.9.2 (2000-02-23) [umoeller]
 *@@changed V0.9.6 (2000-11-06) [umoeller]: disabled dragging of WPS desktop
 */

VOID TrackWithinPager(HWND hwnd,
                      MPARAM mp1,
                      PPAGEMAGECLIENTDATA pClientData)
{
    HWND        hwndTracked;
    SWP         swpTracked;

    LONG        lMouseX = SHORT1FROMMP(mp1),
                lMouseY = SHORT2FROMMP(mp1);

    // shortcuts to global pagemage config
    PPAGEMAGECONFIG pPageMageConfig = &G_pHookData->PageMageConfig;
    PPOINTL         pptlMaxDesktops = &pPageMageConfig->ptlMaxDesktops;

    PSIZEL          pszlClient = &pClientData->szlClient;

    hwndTracked = pgmwGetWindowFromClientPoint(lMouseX, lMouseY);

    if (    (hwndTracked != NULLHANDLE)
         && (hwndTracked != G_pHookData->hwndWPSDesktop) // V0.9.6 (2000-11-06) [umoeller]
       )
    {
        TRACKINFO       ti;
        float           fScale_X,
                        fScale_Y;

        WinQueryWindowPos(hwndTracked, &swpTracked);
        ti.cxBorder = 1;
        ti.cyBorder = 1;
        ti.cxGrid = 1;
        ti.cyGrid = 1;
        ti.cxKeyboard = 1;
        ti.cyKeyboard = 1;
        fScale_X = (float) (pptlMaxDesktops->x * G_szlEachDesktopReal.cx)
                           / pszlClient->cx;
        fScale_Y = (float) (pptlMaxDesktops->y * G_szlEachDesktopReal.cy)
                           / pszlClient->cy;
        ti.rclTrack.xLeft = (swpTracked.x + G_ptlCurrPos.x) / fScale_X;
        ti.rclTrack.yBottom = ( (pptlMaxDesktops->y - 1)
                              * G_szlEachDesktopReal.cy
                              - G_ptlCurrPos.y + swpTracked.y
                              )
                              / fScale_Y;
        ti.rclTrack.xRight = (swpTracked.x + swpTracked.cx + G_ptlCurrPos.x)
                             / fScale_X - 1;
        ti.rclTrack.yTop = (    (pptlMaxDesktops->y - 1)
                                * G_szlEachDesktopReal.cy
                                - G_ptlCurrPos.y
                                + swpTracked.y
                                + swpTracked.cy
                           )
                           / fScale_Y - 1;
        ti.rclBoundary.xLeft = 0;
        ti.rclBoundary.yBottom = 0;
        ti.rclBoundary.xRight = pszlClient->cx;
        ti.rclBoundary.yTop = pszlClient->cy;
        ti.ptlMinTrackSize.x = 2;
        ti.ptlMinTrackSize.y = 2;
        ti.ptlMaxTrackSize.x = pszlClient->cx;
        ti.ptlMaxTrackSize.y = pszlClient->cy;
        ti.fs = TF_STANDARD | TF_MOVE | TF_SETPOINTERPOS | TF_ALLINBOUNDARY;

        if (WinTrackRect(hwnd, NULLHANDLE, &ti))
        {
            swpTracked.x = (ti.rclTrack.xLeft * fScale_X) - G_ptlCurrPos.x;
            swpTracked.y = (ti.rclTrack.yBottom * fScale_Y)
                           -  (   (pptlMaxDesktops->y - 1)
                                  * G_szlEachDesktopReal.cy
                                  - G_ptlCurrPos.y
                              );
            swpTracked.x -= swpTracked.x % 16 +
                            WinQuerySysValue(HWND_DESKTOP, SV_CXDLGFRAME);

            // disable message processing in the hook
            G_pHookData->fDisableSwitching = TRUE;

            WinSetWindowPos(hwndTracked, HWND_TOP, swpTracked.x, swpTracked.y,
                            0, 0,
                            SWP_MOVE | SWP_NOADJUST);

            G_pHookData->fDisableSwitching = FALSE;

            WinPostMsg(hwnd,
                       PGMG_INVALIDATECLIENT,
                       (MPARAM)TRUE,        // immediately
                       0);
                    // V0.9.2 (2000-02-22) [umoeller]
        }
    }
}

/*
 *@@ ClientCreate:
 *      implementation for WM_CREATE in fnwpPageMageClient.
 *
 *@@added V0.9.7 (2001-01-18) [umoeller]
 */

VOID ClientCreate(HWND hwnd,
                  PPAGEMAGECLIENTDATA pClientData)
{
    SIZEL           szlMem;

    // shortcuts to global pagemage config
    PPAGEMAGECONFIG pPageMageConfig = &G_pHookData->PageMageConfig;
    PPOINTL         pptlMaxDesktops = &pPageMageConfig->ptlMaxDesktops;

    G_ptlCurrPos.x = (pPageMageConfig->ptlStartDesktop.x - 1)
                      * G_szlEachDesktopReal.cx;
    G_ptlCurrPos.y = (pptlMaxDesktops->y
                      - pPageMageConfig->ptlStartDesktop.y
                     ) * G_szlEachDesktopReal.cy;

    strcpy(pClientData->szFaceName, "2.System VIO");
                // ###

    WinPostMsg(hwnd,
               PGMG_ZAPPO,
               // frame initially has title bar; so if this is
               // disabled, we need to hide it
               (MPARAM)(!pPageMageConfig->fShowTitlebar),
               MPVOID);

    pClientData->fClientBitmapNeedsUpdate = TRUE;
}

/*
 *@@ ClientPaint:
 *      implementation for WM_PAINT in fnwpPageMageClient.
 *
 *@@added V0.9.7 (2001-01-18) [umoeller]
 */

VOID ClientPaint(HWND hwnd,
                 PPAGEMAGECLIENTDATA pClientData)
{
    PSIZEL      pszlClient = &pClientData->szlClient;
    POINTL      ptlCopy[4];
    HPS         hps = WinBeginPaint(hwnd, 0, 0);

    // switch to RGB mode
    GpiCreateLogColorTable(hps, 0, LCOLF_RGB, 0, 0, NULL);
        // V0.9.3 (2000-04-09) [umoeller]
    if (pClientData->fClientBitmapNeedsUpdate)
    {
        // bitmap has been marked as invalid:
        UpdateClientBitmap(pClientData);

        pClientData->fClientBitmapNeedsUpdate = FALSE;
    }

    // now just bit-blt the bitmap
    ptlCopy[0].x = 0;
    ptlCopy[0].y = 0;
    ptlCopy[1].x = pszlClient->cx;
    ptlCopy[1].y = pszlClient->cy;
    ptlCopy[2].x = 0;
    ptlCopy[2].y = 0;
    GpiBitBlt(hps,
              pClientData->hpsMem,
              (LONG)3,
              ptlCopy,
              ROP_SRCCOPY,
              BBO_IGNORE);
    WinEndPaint(hps);
}

/*
 *@@ ClientPaint:
 *      implementation for WM_PRESPARAMCHANGED in fnwpPageMageClient.
 *
 *@@added V0.9.7 (2001-01-18) [umoeller]
 */

VOID ClientPresParamChanged(HWND hwnd,
                            MPARAM mp1,
                            PPAGEMAGECLIENTDATA pClientData)
{
    if (LONGFROMMP(mp1) == PP_FONTNAMESIZE)
    {
        HPS             hps;
        FATTRS          FontAttrs = {0};
        FONTMETRICS     fm;
        CHAR            szLocFacename[PGMG_TEXTLEN];

        WinQueryPresParam(hwnd, PP_FONTNAMESIZE, 0, NULL,
                          sizeof(szLocFacename), &szLocFacename,
                          0);
        _Pmpf(("New facename: %s", szLocFacename));
        if (strcmp(pClientData->szFaceName, szLocFacename))
        {
            G_bConfigChanged = TRUE;
            strcpy(pClientData->szFaceName, szLocFacename);
        }

        // get the font metrics of the current window
        // font; if we get a cached micro PS, PM will
        // automatically set the font
        hps = WinGetPS(hwnd);
        GpiQueryFontMetrics(hps, sizeof(FONTMETRICS), &fm);
        WinReleasePS(hps);

        FontAttrs.usRecordLength = sizeof(FATTRS);
        FontAttrs.fsSelection = fm.fsSelection;
        FontAttrs.lMatch = fm.lMatch;
        strcpy(FontAttrs.szFacename, fm.szFacename);
        FontAttrs.idRegistry = fm.idRegistry;
        FontAttrs.usCodePage = fm.usCodePage;
        FontAttrs.lMaxBaselineExt = fm.lMaxBaselineExt;
        FontAttrs.lAveCharWidth = fm.lAveCharWidth;

        // create logical font for the memory PS;
        // this replaces the existing one if
        // LCID_PAGEMAGE_FONT has already been used, GPIREF says
        GpiCreateLogFont(pClientData->hpsMem,
                         NULL,
                         LCID_PAGEMAGE_FONT,
                         &FontAttrs);

        WinPostMsg(hwnd, PGMG_INVALIDATECLIENT,
                   (MPARAM)TRUE,        // immediately
                   0);
    }
}

/*
 *@@ ClientSize:
 *      implementation for WM_SIZE in fnwpPageMageClient.
 *
 *@@added V0.9.7 (2001-01-18) [umoeller]
 */

VOID ClientSize(HWND hwnd,
                MPARAM mp2,
                PPAGEMAGECLIENTDATA pClientData)
{
    PSIZEL      pszlClient = &pClientData->szlClient;
    SHORT       cxNew = SHORT1FROMMP(mp2),
                cyNew = SHORT2FROMMP(mp2);

    if (    (pszlClient->cx != cxNew)
         || (pszlClient->cy != cyNew)
       )
    {
        // size changed, or first call (then pszlClient is 0, 0):

        // shortcuts to global pagemage config
        PPAGEMAGECONFIG pPageMageConfig = &G_pHookData->PageMageConfig;
        PPOINTL         pptlMaxDesktops = &pPageMageConfig->ptlMaxDesktops;

        pszlClient->cx = cxNew;
        pszlClient->cy = cyNew;

        pClientData->bmihMem.cx = pszlClient->cx;
        pClientData->bmihMem.cy = pszlClient->cy;

        G_szlEachDesktopInClient.cx = (pszlClient->cx - pptlMaxDesktops->x + 1)
                                       / pptlMaxDesktops->x;
        G_szlEachDesktopInClient.cy = (pszlClient->cy - pptlMaxDesktops->y + 1)
                                       / pptlMaxDesktops->y;

        if (pClientData->hbmClient == NULLHANDLE)
        {
            // first call (bitmap not yet created):
            LONG            lBitmapFormat[2];

            // create memory DC and PS
            pClientData->hdcMem = DevOpenDC(G_habDaemon,
                                            OD_MEMORY,
                                            "*",
                                            (LONG)0,
                                            NULL,
                                            (LONG)0);       // screen-compatible
            /* slMem.cx = 0;
            slMem.cy = 0; */
                    // no, this uses too much memory V0.9.7 (2001-01-18) [umoeller]
            pClientData->hpsMem = GpiCreatePS(G_habDaemon,
                                              pClientData->hdcMem,
                                              pszlClient,
                                              GPIA_ASSOC | PU_PELS);
            // switch to RGB mode
            GpiCreateLogColorTable(pClientData->hpsMem, 0, LCOLF_RGB, 0, 0, NULL);
                // V0.9.3 (2000-04-09) [umoeller]

            // prepare bitmap creation
            GpiQueryDeviceBitmapFormats(pClientData->hpsMem,
                                        (LONG)2,
                                        lBitmapFormat);
            memset(&pClientData->bmihMem, 0, sizeof(BITMAPINFOHEADER2));
            pClientData->bmihMem.cbFix = sizeof(BITMAPINFOHEADER2);
            pClientData->bmihMem.cx = pszlClient->cx;
            pClientData->bmihMem.cy = pszlClient->cy;
            pClientData->bmihMem.cPlanes = lBitmapFormat[0];
            pClientData->bmihMem.cBitCount = lBitmapFormat[1];

            // set font; this creates a logical font in WM_PRESPARAMSCHANGED (below)
            WinSetPresParam(hwnd, PP_FONTNAMESIZE,
                            strlen(pClientData->szFaceName) + 1,
                            pClientData->szFaceName);
        }
        else
        {
            // not first call: delete old bitmap then

            GpiSetBitmap(pClientData->hpsMem, NULLHANDLE);
                // free existing; otherwise GpiDeleteBitmap fails;
                // this was an enourmous resource leak on resize
                // V0.9.2 (2000-02-22) [umoeller]
            GpiDeleteBitmap(pClientData->hbmClient);
            pClientData->hbmClient = NULLHANDLE;

            // and resize the PS
            GpiSetPS(pClientData->hpsMem,
                     pszlClient,
                     0);
        }

        pClientData->hbmClient = GpiCreateBitmap(pClientData->hpsMem,
                                                 &pClientData->bmihMem,
                                                 (LONG)0,
                                                 NULL,
                                                 NULL);
        GpiSetBitmap(pClientData->hpsMem,
                     pClientData->hbmClient);
        WinPostMsg(hwnd,
                   PGMG_INVALIDATECLIENT,
                   (MPARAM)TRUE,        // immediately
                   0);
    }
}

/*
 *@@ ClientButtonClick:
 *      implementation for WM_BUTTON1CLICK and WM_BUTTON2CLICK
 *      in fnwpPageMageClient.
 *
 *@@added V0.9.7 (2001-01-18) [umoeller]
 */

MRESULT ClientButtonClick(HWND hwnd,
                          ULONG msg,
                          MPARAM mp1)
{
    ULONG       ulRequest;
    ULONG       ulMsg = PGOM_CLICK2ACTIVATE;
    LONG        lMouseX = SHORT1FROMMP(mp1),
                lMouseY = SHORT2FROMMP(mp1);

    // disable message processing in the hook
    // G_pHookData->fDisableSwitching = TRUE;

    if (msg == WM_BUTTON2CLICK)
        ulMsg = PGOM_CLICK2LOWER;

    WinPostMsg(G_pHookData->hwndPageMageMoveThread,
               ulMsg,
               (MPARAM)lMouseX,
               (MPARAM)lMouseY);
        // this posts PGMG_CHANGEACTIVE back to us

    // reset the hook to its proper value
    // G_pHookData->fDisableSwitching = FALSE;

    if (G_pHookData->PageMageConfig.fFlash)
        pgmgcStartFlashTimer();

    return ((MPARAM)TRUE);      // processed
}

/*
 *@@ ClientDestroy:
 *      implementation for WM_DESTROY in fnwpPageMageClient.
 *
 *@@added V0.9.7 (2001-01-18) [umoeller]
 */

VOID ClientDestroy(PPAGEMAGECLIENTDATA pClientData)
{
    GpiSetBitmap(pClientData->hpsMem, NULLHANDLE);
            // this was missing V0.9.7 (2001-01-18) [umoeller]
    GpiDeleteBitmap(pClientData->hbmClient);
    GpiAssociate(pClientData->hpsMem, NULLHANDLE);
            // this was missing V0.9.7 (2001-01-18) [umoeller], geese!
    GpiDestroyPS(pClientData->hpsMem);
    DevCloseDC(pClientData->hdcMem);
}

/*
 *@@ ClientZappo:
 *      implementation for PGMG_ZAPPO in fnwpPageMageClient.
 *
 *@@added V0.9.7 (2001-01-18) [umoeller]
 */

VOID ClientZappo(PPAGEMAGECLIENTDATA pClientData)
{
    HWND hwndPageMageFrame = G_pHookData->hwndPageMageFrame;

    if (!G_pHookData->PageMageConfig.fShowTitlebar)
    {
        // titlebar disabled:
        pClientData->hwndSaveSysMenu = WinWindowFromID(hwndPageMageFrame,
                                                     FID_SYSMENU);
        pClientData->hwndSaveTitlebar = WinWindowFromID(hwndPageMageFrame,
                                                      FID_TITLEBAR);
        pClientData->hwndSaveMinButton = WinWindowFromID(hwndPageMageFrame,
                                                       FID_MINMAX);
        WinSetParent(pClientData->hwndSaveSysMenu, HWND_OBJECT,
                     TRUE);     // redraw
        WinSetParent(pClientData->hwndSaveTitlebar, HWND_OBJECT, TRUE);
        WinSetParent(pClientData->hwndSaveMinButton, HWND_OBJECT, TRUE);
        WinSendMsg(hwndPageMageFrame,
                   WM_UPDATEFRAME,
                   MPFROMLONG(FCF_SIZEBORDER),
                   MPVOID);
    }
    else
    {
        // titlebar enabled:
        WinSetParent(pClientData->hwndSaveSysMenu, hwndPageMageFrame, TRUE);
        WinSetParent(pClientData->hwndSaveTitlebar, hwndPageMageFrame, TRUE);
        WinSetParent(pClientData->hwndSaveMinButton, hwndPageMageFrame, TRUE);
        WinSendMsg(hwndPageMageFrame,
                   WM_UPDATEFRAME,
                   MPFROMLONG(FCF_SIZEBORDER | FCF_SYSMENU |
                              FCF_HIDEBUTTON | FCF_TITLEBAR),
                   MPVOID);
    }
}

/*
 * fnwpPageMageClient:
 *      window procedure of the PageMage client window,
 *      which was created by pgmcCreateMainControlWnd.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 *@@changed V0.9.3 (2000-04-09) [umoeller]: now using RGB colors
 *@@changed V0.9.4 (2000-07-10) [umoeller]: fixed flashing; now using a win timer
 *@@changed V0.9.4 (2000-07-10) [umoeller]: now suspending flashing when dragging in pager
 *@@changed V0.9.4 (2000-08-08) [umoeller]: fixed various update/formatting problems
 *@@changed V0.9.7 (2001-01-18) [umoeller]: extracted all the Client* funcs
 *@@changed V0.9.7 (2001-01-18) [umoeller]: reduced memory usage for mem PS, reorganized bitmap code
 *@@changed V0.9.7 (2001-01-18) [umoeller]: removed PGMG_LOCKUP call, pagemage doesn't need this
 */

MRESULT EXPENTRY fnwpPageMageClient(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    static PAGEMAGECLIENTDATA   ClientData = {0};

    MRESULT                     mrc  = 0;
    BOOL                        bHandled = TRUE;

    TRY_LOUD(excpt1)
    {
        switch (msg)
        {
            /*
             * WM_CREATE:
             *
             */

            case WM_CREATE:
                ClientCreate(hwnd, &ClientData);
            break;

            /*
             * WM_PAINT:
             *
             */

            case WM_PAINT:
                ClientPaint(hwnd, &ClientData);
            break;

            /*
             * WM_ERASEBACKGROUND:
             *      don't need this, WinDefWindowproc returns FALSE for this.
             *      V0.9.7 (2001-01-18) [umoeller]
             */

            /* case WM_ERASEBACKGROUND:
                mReturn = MRFROMLONG(FALSE);
                    // pretend we've processed this;
                    // we paint the entire bitmap in WM_PAINT anyway
                break;
               */

            /*
             * WM_FOCUSCHANGE:
             *      the default winproc forwards this to the
             *      frame. This is not what we want. We never
             *      want the focus, no matter what.
             */

            case WM_FOCUSCHANGE:
            break;

            /*
             * WM_PRESPARAMCHANGED:
             *
             */

            case WM_PRESPARAMCHANGED:
                ClientPresParamChanged(hwnd,
                                       mp1,
                                       &ClientData);
            break;

            /*
             * WM_SIZE:
             *
             */

            case WM_SIZE:
                ClientSize(hwnd, mp2, &ClientData);
            break;

            /*
             * WM_COMMAND:
             *
             */

            case WM_COMMAND:
                break;

            /*
             * WM_BUTTON1CLICK:
             *      activate a specific window.
             */

            case WM_BUTTON1CLICK:
            case WM_BUTTON2CLICK:
                mrc = ClientButtonClick(hwnd, msg, mp1);
            break;

            /* case WM_BUTTON2DBLCLK:
                DumpAllWindows();
            break; */

            /*
             * WM_BUTTON2MOTIONSTART:
             *      move the windows within the pager.
             */

            case WM_BUTTON2MOTIONSTART:
            {
                if (G_pHookData->PageMageConfig.fFlash)
                    WinStopTimer(G_habDaemon,
                                 hwnd,
                                 2);

                TrackWithinPager(hwnd,
                                 mp1,
                                 &ClientData);

                if (G_pHookData->PageMageConfig.fFlash)
                    pgmgcStartFlashTimer();
            break; }

            /*
             * WM_CLOSE:
             *      window being closed:
             *      notify XFLDR.DLL and restore windows.
             */

            case WM_CLOSE:
                _Pmpf(("fnwpPageMageClient: WM_CLOSE, notifying Kernel"));
                dmnKillPageMage(TRUE);  // notify XFLDR.DLL
                bHandled = TRUE;        // no default processing
            break;

            case WM_DESTROY:
                ClientDestroy(&ClientData);
                bHandled = FALSE;
            break;

            /*
             *@@ PGMG_INVALIDATECLIENT:
             *      posted by hookSendMsgHook when
             *      WM_WINDOWPOSCHANGED or WM_SETFOCUS
             *      have been detected.
             *      We then set a flag that the bitmap
             *      needs updating and delay invalidating
             *      the client for a while to avoid excessive
             *      redraw.
             */

            case PGMG_INVALIDATECLIENT:
                ClientData.fClientBitmapNeedsUpdate = TRUE;
                // start timer to invalidate rectangle

                if (G_hUpdateTimer != NULLHANDLE)
                    WinStopTimer(G_habDaemon,
                                 hwnd,
                                 1);
                if (mp1)
                    // immediate update:
                    WinInvalidateRect(hwnd, NULL, TRUE);
                else
                    // start delayed update
                    G_hUpdateTimer = WinStartTimer(G_habDaemon,
                                                   hwnd,
                                                   1,
                                                   200);     // milliseconds
                break;

            /*
             * WM_TIMER:
             *      one-shot timers for painting and flashing.
             */

            case WM_TIMER:
            {
                USHORT usTimerID = (USHORT)mp1;     // timer ID
                WinStopTimer(G_habDaemon,
                             hwnd,
                             usTimerID);

                switch (usTimerID)
                {
                    case 1:     // delayed paint timer
                        G_hUpdateTimer = NULLHANDLE;
                        // invalidate the client rectangle;
                        // since G_ClientBitmapNeedsUpdate is TRUE
                        // now, this will recreate the bitmap in WM_PAINT
                        WinInvalidateRect(hwnd, NULL, TRUE);
                    break;

                    case 2:     // flash timer started by fntMoveThread
                        WinShowWindow(G_pHookData->hwndPageMageFrame, FALSE);
                }
            break; }

            /*
             *@@ PGMG_ZAPPO:
             *      this needs to be posted when the titlebar etc.
             *      settings have changed to reformat the frame
             *      properly.
             *
             *      Parameters: none.
             */

            case PGMG_ZAPPO:
                ClientZappo(&ClientData);
            break;

            /*
             * PGMG_LOCKUP:
             *      posted by hookLockupHook when the system is
             *      locked up (with mp1 == TRUE) or by hookSendMsgHook
             *      when the lockup window is destroyed.
             *
             *      HookData.hwndLockupFrame has the lockup frame window
             *      created by PM.
             *
             * V0.9.7 (2001-01-18) [umoeller]: removed, this did nothing
             */

            /* case PGMG_LOCKUP:
                // G_bLockup = LONGFROMMP(mp1);
                break; */

            /*
             *@@ PGMG_WNDCHANGE:
             *      posted by hookSendMsgHook when
             *      WM_CREATE or WM_DESTROY has been
             *      detected.
             *
             *      Parameters:
             *      -- HWND mp1: window being created or destroyed.
             *      -- ULONG mp2: message (WM_CREATE or WM_DESTROY).
             */

            case PGMG_WNDCHANGE:
            {
                HWND    hwndChanged = HWNDFROMMP(mp1);
                switch (LONGFROMMP(mp2))
                {
                    case WM_CREATE:
                        pgmwAppendNewWinInfo(hwndChanged);
                    break;

                    case WM_DESTROY:
                        pgmwDeleteWinInfo(hwndChanged);
                    break;

                    case WM_SETWINDOWPARAMS: // V0.9.7 (2001-01-15) [dk]
                        pgmwUpdateWinInfo(hwndChanged);
                    break;
                }

                // _Pmpf(("PGMG_WNDCHANGE, posting PGMG_REDRAW"));
                WinPostMsg(hwnd,
                           PGMG_INVALIDATECLIENT,
                           (MPARAM)FALSE,       // delayed update
                           0);
            break; }

            /*
             *@@ PGMG_WNDRESCAN:
             *      posted only from UpdateClientBitmap if any
             *      windows with the WINDOW_RESCAN flag
             *      have been encountered. In that case,
             *      we need to re-scan the window list.
             */

            case PGMG_WNDRESCAN:
                if (pgmwWindowListRescan())
                    // changed:
                    WinPostMsg(hwnd,
                               PGMG_INVALIDATECLIENT,
                               (MPARAM)FALSE,       // delayed
                               MPVOID);
                break;

            /*
             *@@ PGMG_CHANGEACTIVE:
             *      posted only by fntMoveThread when
             *      PGMGQ_CLICK2ACTIVATE was retrieved from
             *      the "move" queue. This happens as a
             *      response to mb1 or mb2 clicks.
             *
             *      Parameters: new active HWND in mp1.
             */

            case PGMG_CHANGEACTIVE:
            {
                HWND    hwnd2Activate = HWNDFROMMP(mp1);

                // disable message processing in the hook
                // G_pHookData->fDisableSwitching = TRUE;

                if (WinIsWindowEnabled(hwnd2Activate))
                        // fixed V0.9.2 (2000-02-23) [umoeller]
                {
                    WinSetActiveWindow(HWND_DESKTOP, hwnd2Activate);
                            // this can be on any desktop, even if it's
                            // not the current one... the hook will determine
                            // that the active window has changed and have
                            // the windows repositioned accordingly.
                    WinPostMsg(hwnd,
                               PGMG_INVALIDATECLIENT,
                               (MPARAM)TRUE,        // immediately
                               0);
                                    // V0.9.2 (2000-02-22) [umoeller]
                }

                // G_pHookData->fDisableSwitching = fOld;
            break; }

            /*
             *@@ PGMG_LOWERWINDOW:
             *      posted only by fntMoveThread when
             *      PGMGQ_CLICK2LOWER was retrieved from
             *      the "move" queue.
             *
             *      Parameters: HWND to lower in mp1.
             *
             *@@added V0.9.2 (2000-02-23) [umoeller]
             */

            case PGMG_LOWERWINDOW:
            {
                HWND hwnd2Lower = HWNDFROMMP(mp1);
                /* if (NO_ERROR == DosRequestMutexSem(G_pHookData->hmtxPageMage,
                                                   10000)) */
                {
                    // disable message processing in the hook
                    G_pHookData->fDisableSwitching = TRUE;

                    WinSetWindowPos(hwnd2Lower,
                                    HWND_BOTTOM,
                                    0,0,0,0,
                                    SWP_ZORDER);
                    WinPostMsg(hwnd,
                               PGMG_INVALIDATECLIENT,
                               (MPARAM)TRUE,        // immediately
                               0);

                    G_pHookData->fDisableSwitching = FALSE;
                    // DosReleaseMutexSem(G_pHookData->hmtxPageMage);
                }
            break; }

            default:
                bHandled = FALSE;
                break;
        }
    }
    CATCH(excpt1)
    {
        bHandled = FALSE;
    } END_CATCH();

    if (!bHandled)
        mrc = WinDefWindowProc(hwnd, msg, mp1, mp2);

    return (mrc);
} // fnwpPageMageClient

/*
 * fnwpSubclPageMageFrame:
 *      window procedure for the subclassed PageMage
 *      frame, which was subclassed by pgmcCreateMainControlWnd.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 *@@changed V0.9.4 (2000-08-08) [umoeller]: fixed various update/formatting problems
 */

MRESULT EXPENTRY fnwpSubclPageMageFrame(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT     mrc = 0;

    switch (msg)
    {
        case WM_ADJUSTWINDOWPOS:
        {
            PSWP pswp = (PSWP)mp1;

            if (pswp->cx < 20)
                pswp->cx = 20;
            if (pswp->cy < 20)
                pswp->cy = 20;

            if (G_pHookData->PageMageConfig.fPreserveProportions)
                pswp->cy = pgmcCalcNewFrameCY(pswp->cx);

            // we never want to become active:
            if (pswp->fl & SWP_ACTIVATE)
            {
                pswp->fl &= ~SWP_ACTIVATE;
                pswp->fl |= SWP_DEACTIVATE;
            }

            mrc = G_pfnOldFrameWndProc(hwnd, msg, mp1, mp2);
        break; }

        /*
         * WM_FOCUSCHANGE:
         *      the standard frame window proc sends out a
         *      flurry of WM_SETFOCUS and WM_ACTIVATE
         *      messages. We never want to get the focus,
         *      no matter what.
         */

        case WM_FOCUSCHANGE:
        break;

        /*
         * WM_TRACKFRAME:
         *      comes in when the user attempts to resize
         *      the PageMage window. We need to stop the
         *      flash timer while this is going on.
         */

        case WM_TRACKFRAME:
            // stop flash timer, if running
            if (G_pHookData->PageMageConfig.fFlash)
                WinStopTimer(G_habDaemon,
                             G_pHookData->hwndPageMageClient,
                             2);

            // now track window (WC_FRAME runs a modal message loop)
            mrc = G_pfnOldFrameWndProc(hwnd, msg, mp1, mp2);

            if (G_pHookData->PageMageConfig.fFlash)
                pgmgcStartFlashTimer();
        break;

        /*
         * WM_QUERYTRACKINFO:
         *      the frame control generates this when
         *      WM_TRACKFRAME comes in.
         */

        case WM_QUERYTRACKINFO:
        {
            PTRACKINFO  ptr;
            G_pfnOldFrameWndProc(hwnd, msg, mp1, mp2);
            ptr = (PTRACKINFO) PVOIDFROMMP(mp2);
            ptr->ptlMinTrackSize.x = 20;
            ptr->ptlMinTrackSize.y = 20;

            mrc = MRFROMSHORT(TRUE);
        break; }

        /*
         * WM_WINDOWPOSCHANGED:
         *      store new window pos in OS2.INI.
         */

        case WM_WINDOWPOSCHANGED:
        {
            PSWP pswp1 = (PSWP)mp1;
            PrfWriteProfileData(HINI_USER,
                                INIAPP_XWPHOOK,
                                INIKEY_HOOK_PGMGWINPOS,
                                pswp1,
                                sizeof(SWP));
            mrc = G_pfnOldFrameWndProc(hwnd, msg, mp1, mp2);
        break; }

        default:
            mrc = G_pfnOldFrameWndProc(hwnd, msg, mp1, mp2);
    }

    return (mrc);

} // FrameWndProc


