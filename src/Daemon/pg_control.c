
/*
 *@@sourcefile pg_control.c:
 *      XPager Desktop control window.
 *
 *      The XPager was originally derived from PageMage,
 *      a program written by Carlos Ugarte, which was
 *      eventually released as open source under the
 *      GPL. PageMage implemented its own system hook.
 *      while the PageMage window and the move handling
 *      were in a separate PM program.
 *
 *      With V0.9.0, PageMage was integrated into XWorkplace
 *      (to be more precise, into the hook and the daemon).
 *      Since by now the XWorkplace implementation has
 *      very little in common with PageMage any more, it
 *      has been renamed to "XPager" with 0.9.18.
 *
 *      Basically, XPager consists of the following
 *      components:
 *
 *      --  The XPager control window (the pager).
 *          For one, this paints the representation
 *          of all virtual desktops in its client.
 *
 *          In order to be able to do this, it receives
 *          messages from the XWP hook whenever frame
 *          windows are created, moved, renamed, or
 *          destroyed.
 *
 *      --  XPager keeps its own window list to be
 *          able to quickly trace all windows and their
 *          positions without having to query the entire
 *          PM switch list or enumerate all desktop windows
 *          all the time. As a result, XPager's CPU load
 *          is pretty low.
 *
 *          The code for this is in pg_winscan.c.
 *
 *      --  The XPager "move thread", which is a second
 *          thread which gets started when the pager is
 *          created. This is in pg_move.c.
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
 *      --  As with the rest of the daemon, XPager receives
 *          notifications from XFLDR.DLL when its settings
 *          have been modified in the "Screen" settings notebook.
 */

/*
 *      Copyright (C) 1995-1999 Carlos Ugarte.
 *      Copyright (C) 2000-2002 Ulrich M”ller.
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
 *  ³0,0³1,0³2,0³3,0³
 *  ÃÄÄÄÅÄÄÄÅÄÄÄÅÄÄÄ´
 *  ³0,1³1,1³2,1³3,1³
 *  ÃÄÄÄÅÄÄÄÅÄÄÄÅÄÄÄ´
 *  ³0,2³1,2³2,2³3,2³
 *  ÀÄÄÄÁÄÄÄÁÄÄÄÁÄÄÄÙ
 *
 *  [Note that (0,0) is the _upper_ left desktop, not the _lower_ left.]
 */

#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#define INCL_DOSEXCEPTIONS
#define INCL_DOSMISC
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

#define DONT_REPLACE_MALLOC         // in case mem debug is enabled
#include "setup.h"                      // code generation and debugging options

#include "helpers\except.h"             // exception handling
#include "helpers\linklist.h"           // linked list helper routines

#include "xwpapi.h"                     // public XWorkplace definitions

#include "hook\xwphook.h"
#include "hook\hook_private.h"
#include "hook\xwpdaemn.h"              // XPager and daemon declarations

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

#define WNDCLASS_PAGERCLIENT         "XWPXPagerClient"

static PFNWP       G_pfnOldFrameWndProc = 0;

static ULONG       G_hUpdateTimer = NULLHANDLE;

HMTX        G_hmtxDisableSwitching = NULLHANDLE;    // V0.9.14 (2001-08-25) [umoeller]

/* ******************************************************************
 *
 *   XPager hook serialization
 *
 ********************************************************************/

/*
 *@@ pgmcDisableSwitching:
 *      sets the fDisablePgmgSwitching in the global
 *      hook data to prevent the send-msg hook from
 *      intercepting window messages.
 *
 *      This is required whenever the daemon forces
 *      moving messages, e.g. because of screen
 *      switches, because otherwise we'd recurse
 *      into the hook.
 *
 *      With V0.9.14, this has been encapsulated in
 *      this function to allow for protecting the
 *      flag with a mutex. It seems that previously
 *      the flag got set wrongly if two funcs attempted
 *      to set or clear it independently. As a
 *      result of this wrapper now, only one func
 *      can hold this flag set at a time.
 *
 *      Call pgmcReenableSwitching to clear the flag
 *      again, which will release the mutex too.
 *
 *@@added V0.9.14 (2001-08-25) [umoeller]
 */

BOOL pgmcDisableSwitching(VOID)
{
    if (!G_hmtxDisableSwitching)
        // first call:
        DosCreateMutexSem(NULL, &G_hmtxDisableSwitching, 0, FALSE);

    if (!WinRequestMutexSem(G_hmtxDisableSwitching, 4000))
        // WinRequestMutexSem works even if the thread has no message queue
    {
        G_pHookData->fDisablePgmgSwitching = TRUE;
        return TRUE;
    }

    DosBeep(100, 100);
    return FALSE;
}

/*
 *@@ pgmcReenableSwitching:
 *
 *@@added V0.9.14 (2001-08-25) [umoeller]
 */

VOID pgmcReenableSwitching(VOID)
{
    if (G_pHookData->fDisablePgmgSwitching)
    {
        G_pHookData->fDisablePgmgSwitching = FALSE;
        DosReleaseMutexSem(G_hmtxDisableSwitching);
    }
}

/* ******************************************************************
 *
 *   Debugging
 *
 ********************************************************************/

#ifdef __DEBUG__

/*
 *@@ DumpAllWindows:
 *
 */

VOID DumpAllWindows(VOID)
{
    ULONG  usIdx = 0;
    if (pgmwLock())
    {
        PLISTNODE pNode = lstQueryFirstNode(&G_llWinInfos);
        while (pNode)
        {
            PPGMGWININFO pEntryThis = (PPGMGWININFO)pNode->pItemData;
            _Pmpf(("Dump %d: hwnd 0x%lX \"%s\":\"%s\" pid 0x%lX(%d) type %d",
                   usIdx++,
                   pEntryThis->hwnd,
                   pEntryThis->szSwitchName,
                   pEntryThis->szClassName,
                   pEntryThis->pid,
                   pEntryThis->pid,
                   pEntryThis->bWindowType));

            pNode = pNode->pNext;
        }

        pgmwUnlock();
    }
}

#endif

/* ******************************************************************
 *
 *   XPager control window
 *
 ********************************************************************/

/*
 *@@ pgmcCreateMainControlWnd:
 *      creates the XPager control window (frame
 *      and client). This gets called by dmnStartXPager
 *      on thread 1 when XPager has been enabled.
 *
 *      This registers the XPager client class
 *      using fnwpXPagerClient, which does most of
 *      the work for XPager.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 *@@changed V0.9.3 (2000-05-22) [umoeller]: the hook was interfering, fixed
 */

BOOL pgmcCreateMainControlWnd(VOID)
{
    BOOL    brc = TRUE;

    // _Pmpf(("Entering pgmcCreateMainControlWnd"));

    if (!G_pHookData->hwndXPagerFrame)
    {
        ULONG   flFrameFlags    = FCF_TITLEBAR |
                                  FCF_SYSMENU |
                                  FCF_HIDEBUTTON |
                                  FCF_NOBYTEALIGN |
                                  FCF_SIZEBORDER;

        WinRegisterClass(G_habDaemon,
                         WNDCLASS_PAGERCLIENT,
                         (PFNWP)fnwpXPagerClient,
                         0,
                         0);

        // disable message processing in the hook
        if (pgmcDisableSwitching())
        {
            if (G_pHookData->hwndXPagerFrame
                = WinCreateStdWindow(HWND_DESKTOP,
                                     0, // WS_ANIMATE,  // style
                                     &flFrameFlags,
                                     (PSZ)WNDCLASS_PAGERCLIENT,
                                     "XWorkplace XPager",
                                     0,
                                     0,
                                     1000,    // ID...
                                     &G_pHookData->hwndXPagerClient))
            {
                // set frame icon
                WinSendMsg(G_pHookData->hwndXPagerFrame,
                           WM_SETICON,
                           (MPARAM)G_hptrDaemon,
                           NULL);
                // subclass frame
                G_pfnOldFrameWndProc = WinSubclassWindow(G_pHookData->hwndXPagerFrame,
                                                         fnwpSubclXPagerFrame);

                brc = TRUE;
            }

            pgmcReenableSwitching();

            pgmcSetPgmgFramePos(G_pHookData->hwndXPagerFrame);
        }
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
    // shortcuts to global config
    PPAGERCONFIG    pXPagerConfig = &G_pHookData->XPagerConfig;
    PPOINTL         pptlMaxDesktops = &pXPagerConfig->ptlMaxDesktops;

    LONG lHeightPercentOfWidth = G_pHookData->lCYScreen * 100 / G_pHookData->lCXScreen;
                // e.g. 75 for 1024x768
    LONG lCY =      (  (cx * lHeightPercentOfWidth / 100)
                       * pptlMaxDesktops->y
                       / pptlMaxDesktops->x
                    );
    if (pXPagerConfig->fShowTitlebar)
        lCY += WinQuerySysValue(HWND_DESKTOP, SV_CYTITLEBAR);

    return (lCY);
}

/*
 *@@ pgmcSetPgmgFramePos:
 *      sets the position of the XPager window.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 *@@changed V0.9.4 (2000-08-08) [umoeller]: now restoring window pos properly
 *@@changed V0.9.12 (2001-05-31) [umoeller]: added disable switching again
 */

VOID pgmcSetPgmgFramePos(HWND hwnd)
{
    ULONG   flOptions;

    ULONG   cb = sizeof(G_swpPgmgFrame);

    // disable message processing in the hook
    if (pgmcDisableSwitching())
    {
        PrfQueryProfileData(HINI_USER,
                            INIAPP_XWPHOOK,
                            INIKEY_HOOK_PGMGWINPOS,
                            &G_swpPgmgFrame,
                            &cb);
                // might fail

        flOptions = SWP_MOVE | SWP_SIZE
                        | SWP_SHOW; // V0.9.4 (2000-07-10) [umoeller]

        WinSetWindowPos(hwnd,
                        NULLHANDLE,
                        G_swpPgmgFrame.x,
                        G_swpPgmgFrame.y,
                        G_swpPgmgFrame.cx,
                        G_swpPgmgFrame.cy,
                        flOptions);

        if (G_pHookData->XPagerConfig.fFlash)
            pgmgcStartFlashTimer();

        pgmcReenableSwitching();
    }
}

/*
 *@@ pgmgcStartFlashTimer:
 *      shortcut to start the flash timer.
 *
 *@@added V0.9.4 (2000-07-10) [umoeller]
 */

USHORT pgmgcStartFlashTimer(VOID)
{
    return (WinStartTimer(G_habDaemon,
                          G_pHookData->hwndXPagerClient,
                          2,
                          G_pHookData->XPagerConfig.ulFlashDelay));
}

/* ******************************************************************
 *
 *   XPager client window proc
 *
 ********************************************************************/

/*
 *@@ PAGERCLIENTDATA:
 *      static (!) structure in fnwpXPagerClient
 *      holding all kinds of data needed for the client.
 *
 *      This replaces a whole bunch of global variables
 *      which used to be used with XPager before V0.9.7.
 *
 *@@added V0.9.7 (2001-01-18) [umoeller]
 */

typedef struct _PAGERCLIENTDATA
{
    SIZEL       szlClient;  // client's size

    HDC         hdcMem;     // memory DC for bitmap
    HPS         hpsMem;     // memory PS for bitmap

    HBITMAP     hbmClient;     // the bitmap for the client
    BITMAPINFOHEADER2    bmihMem;   // its bitmap info

    BOOL        fClientBitmapNeedsUpdate;
                // if this is set to TRUE, hbmClient is
                // repainted on the next paint

    CHAR        szFaceName[PGMG_TEXTLEN];
                // font face name

    // saved window handles for titlebar etc. changes:
    HWND        hwndSaveSysMenu,
                hwndSaveTitlebar,
                hwndSaveMinButton;
} PAGERCLIENTDATA, *PPAGERCLIENTDATA;

/*
 *@@ MINIWINDOW:
 *      representation of a WININFO entry in the
 *      XPager client. An array of these is built
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
 *      gets called when fnwpXPagerClient
 *      receives WM_PAINT and the paint bitmap has been
 *      marked as outdated (G_ClientBitmapNeedsUpdate == TRUE).
 *
 *      This draws the new contents of the XPager
 *      client into the bitmap in the memory PS (in
 *      the client data).
 *
 *      That bitmap is then simply bit-blitted on the
 *      screen on WM_PAINT in fnwpXPagerClient.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 *@@changed V0.9.3 (2000-05-22) [umoeller]: fixed font problems
 *@@changed V0.9.4 (2000-08-08) [umoeller]: added special maximized window handling
 *@@changed V0.9.7 (2001-01-18) [umoeller]: changed prototype
 *@@changed V0.9.7 (2001-01-18) [umoeller]: fixed a serialization problem
 *@@changed V0.9.7 (2001-01-21) [umoeller]: now using linked list for wininfos
 *@@changed V0.9.7 (2001-01-21) [umoeller]: largely rewritten for speed
 *@@changed V0.9.18 (2002-02-11) [lafaix]: fixed painting glitches
 *@@changed V0.9.18 (2002-02-20) [lafaix]: fixed missing restored windows
 */

VOID UpdateClientBitmap(PPAGERCLIENTDATA pClientData)
{
    HPS             hpsMem = pClientData->hpsMem;

    POINTL          ptlDest;
    SWP             swpClient;

    ULONG           ul;
    LONG            lTemp;

    // shortcuts to global config
    PPAGERCONFIG    pXPagerConfig = &G_pHookData->XPagerConfig;
    PPOINTL         pptlMaxDesktops = &pXPagerConfig->ptlMaxDesktops;

    PSIZEL          pszlClient = &pClientData->szlClient;

    // current desktop (0,0) is upper left, not lower left
    ULONG           ulCurrScreenX = (G_ptlCurrPos.x / G_szlEachDesktopReal.cx),
                    ulCurrScreenY = (G_ptlCurrPos.y / G_szlEachDesktopReal.cy);

    // expose the client size
    G_szlXPagerClient.cx = pszlClient->cx;
    G_szlXPagerClient.cy = pszlClient->cy;

    /*
     *   (1) entire-client painting
     *
     */

    // fill the background
    GpiSetColor(hpsMem,
                pXPagerConfig->lcNormal);
    ptlDest.x = ptlDest.y = 0;
    GpiMove(hpsMem,
            &ptlDest);
    WinQueryWindowPos(G_pHookData->hwndXPagerClient,
                      &swpClient);
    ptlDest.x = swpClient.cx;
    ptlDest.y = swpClient.cy;
    GpiBox(hpsMem,
           DRO_FILL,
           &ptlDest,
           0,
           0);

    // paint "current" desktop in different color
    GpiSetColor(hpsMem,
                pXPagerConfig->lcCurrent);
    ptlDest.x = (pszlClient->cx * ulCurrScreenX) / pptlMaxDesktops->x;
    ptlDest.y = pszlClient->cy - (pszlClient->cy * ulCurrScreenY) / pptlMaxDesktops->y;
    GpiMove(hpsMem,
            &ptlDest);
    ptlDest.x = (pszlClient->cx * (1 + ulCurrScreenX)) / pptlMaxDesktops->x;
    ptlDest.y = pszlClient->cy - (pszlClient->cy * (1 + ulCurrScreenY)) / pptlMaxDesktops->y;
    GpiBox(hpsMem,
           DRO_FILL,
           &ptlDest,
           0,
           0);

    // draw vertical lines
    GpiSetColor(hpsMem,
                pXPagerConfig->lcDivider);
    for (ul = 1;
         ul < pptlMaxDesktops->x;
         ul++)
    {
        lTemp = (pszlClient->cx * ul) / pptlMaxDesktops->x;
        ptlDest.x = lTemp;
        ptlDest.y = 0;
        GpiMove(hpsMem,
                &ptlDest);
        ptlDest.x = lTemp;
        ptlDest.y = pszlClient->cy;
        GpiLine(hpsMem,
                &ptlDest);
    }

    // draw horizontal lines
    for (ul = 1;
         ul < pptlMaxDesktops->y;
         ul++)
    {
        lTemp = (pszlClient->cy * ul) / pptlMaxDesktops->y;
        ptlDest.x = 0;
        ptlDest.y =   pszlClient->cy
                    - lTemp;
        GpiMove(hpsMem, &ptlDest);
        ptlDest.x = pszlClient->cx;
        ptlDest.y =   pszlClient->cy
                    - lTemp;
        GpiLine(hpsMem, &ptlDest);
    }

    /*
     *   (2) paint mini-windows
     *
     */

    if (pXPagerConfig->fMirrorWindows)
    {
        PMINIWINDOW     paMiniWindows = NULL;
        ULONG           cWinInfos = 0;
        BOOL            fNeedRescan = FALSE;

        // lock the window list all the while we're doing this
        // V0.9.7 (2001-01-21) [umoeller]
        if (pgmwLock())
        {
            if (cWinInfos = lstCountItems(&G_llWinInfos))
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
                if (paMiniWindows = (PMINIWINDOW)malloc(cWinInfos * sizeof(MINIWINDOW)))
                {
                    ULONG           cMiniWindowsUsed = 0;

                    HENUM           henum;
                    HWND            hwndThis;

                    float           fScale_X,
                                    fScale_Y;

//                    memset(paMiniWindows, 0, cWinInfos * sizeof(MINIWINDOW));

                    // set font to use; this font has been created on startup
                    GpiSetCharSet(hpsMem, LCID_PAGER_FONT);

                    fScale_X = (float)(    pptlMaxDesktops->x
                                         * G_szlEachDesktopReal.cx
                                      ) / pszlClient->cx;
                    fScale_Y = (float)(    pptlMaxDesktops->y
                                         * G_szlEachDesktopReal.cy
                                      ) / pszlClient->cy;

                    // start enum
                    if (henum = WinBeginEnumWindows(HWND_DESKTOP))
                    {
                        while (hwndThis = WinGetNextWindow(henum))
                        {
                            if (WinIsWindowVisible(hwndThis))
                            {
                                // go thru local list and find the
                                // current enumeration window
                                PPGMGWININFO pWinInfo = pgmwFindWinInfo(hwndThis,
                                                                        NULL);

                                if (pWinInfo)
                                {
                                    // item is on list:
                                    ULONG ulOldFl = pWinInfo->swp.fl;

                                    // update window pos in window list
                                    WinQueryWindowPos(pWinInfo->hwnd,
                                                      &pWinInfo->swp);

                                    // update the bWindowType status, as restoring
                                    // a previously minimized or hidden window may
                                    // change it
                                    //
                                    // (we don't have to update "special" windows)
                                    // V0.9.18 (2002-02-20) [lafaix]
                                    if (    (ulOldFl != pWinInfo->swp.fl)
                                         && (    (pWinInfo->bWindowType == WINDOW_NORMAL)
                                              || (pWinInfo->bWindowType == WINDOW_MINIMIZE)
                                              || (pWinInfo->bWindowType == WINDOW_MAXIMIZE)
                                            )
                                       )
                                    {
                                        pWinInfo->bWindowType = WINDOW_NORMAL;
                                        if (pWinInfo->swp.fl & SWP_MINIMIZE)
                                            pWinInfo->bWindowType = WINDOW_MINIMIZE;
                                        else
                                        if (pWinInfo->swp.fl & SWP_MAXIMIZE)
                                            pWinInfo->bWindowType = WINDOW_MAXIMIZE;
                                    }

                                    if(pWinInfo->bWindowType == WINDOW_RESCAN)
                                    {
                                        // window wasn't fully scanned: rescan
                                        if (!pgmwFillWinInfo(pWinInfo->hwnd,
                                                             pWinInfo))
                                            fNeedRescan = TRUE;
                                    }

                                    if (    (pWinInfo->bWindowType == WINDOW_NORMAL)
                                         || (pWinInfo->bWindowType == WINDOW_MAXIMIZE)
                                       )
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
                                              / fScale_Y + 1;

                                        pMiniWindowThis->ptlEnd.x
                                            =   (pswp->x + pswp->cx + G_ptlCurrPos.x)
                                              / fScale_X; // - 1;

                                        pMiniWindowThis->ptlEnd.y
                                            = (    (pptlMaxDesktops->y - 1)
                                                 * G_szlEachDesktopReal.cy
                                                 - G_ptlCurrPos.y + pswp->y + pswp->cy
                                              )
                                              / fScale_Y + 1; // - 1;

                                    } // end if (    (pWinInfo->bWindowType == WINDOW_NORMAL) ...
                                } // end if (pWinInfo)
                            } // end if (WinIsVisible)
                        } // end while ((hwndThis = WinGetNextWindow(henum)) != NULLHANDLE)
                        WinEndEnumWindows(henum);
                    }

                    // now paint

                    if (cMiniWindowsUsed)
                    {
                        // we got something to paint:
                        // paint the mini windows in reverse order then
                        // (bottom to top)

                        HWND hwndLocalActive = WinQueryActiveWindow(HWND_DESKTOP);
                        PSZ pszSwitchName;
                        ULONG ulSwitchNameLen;

                        // start with the last one
                        LONG lIdx = cMiniWindowsUsed - 1;

                        while (lIdx >= 0)
                        {
                            PMINIWINDOW pMiniWindowThis = &paMiniWindows[lIdx];

                            if (pMiniWindowThis->hwnd == hwndLocalActive)
                                // this is the active window:
                                GpiSetColor(hpsMem, pXPagerConfig->lcCurrentApp);
                            else
                                GpiSetColor(hpsMem, pXPagerConfig->lcNormalApp);

                            GpiMove(hpsMem,
                                    &pMiniWindowThis->ptlBegin);
                            GpiBox(hpsMem,
                                   DRO_FILL,
                                   &pMiniWindowThis->ptlEnd,
                                   0, 0);
                            GpiSetColor(hpsMem,
                                        pXPagerConfig->lcAppBorder);
                            GpiBox(hpsMem,
                                   DRO_OUTLINE,
                                   &pMiniWindowThis->ptlEnd,
                                   0, 0);

                            // draw window text too?
                            if (    (pXPagerConfig->fShowWindowText)
                                 && (pszSwitchName = pMiniWindowThis->pWinInfo->szSwitchName)
                                 && (ulSwitchNameLen = strlen(pszSwitchName))
                               )
                            {
                                RECTL   rclText;
                                LONG    lTextColor;

                                rclText.xLeft = pMiniWindowThis->ptlBegin.x + 1;
                                rclText.yBottom = pMiniWindowThis->ptlBegin.y + 1;
                                rclText.xRight = pMiniWindowThis->ptlEnd.x - 1;
                                rclText.yTop = pMiniWindowThis->ptlEnd.y - 1; // - 4;

                                if (pMiniWindowThis->hwnd == hwndLocalActive)
                                    // active:
                                    lTextColor = pXPagerConfig->lcTxtCurrentApp;
                                else
                                    lTextColor = pXPagerConfig->lcTxtNormalApp;

                                WinDrawText(hpsMem,
                                            ulSwitchNameLen,
                                            pszSwitchName,
                                            &rclText,
                                            lTextColor,
                                            0,
                                            DT_LEFT | DT_TOP);

                            } // end if (    (pXPagerConfig->fShowWindowText) ...

                            // go back in mini windows list
                            lIdx--;
                        } // end while (ulIdx >= 0)
                    } // end if (cMiniWindowsUsed)
                } // end if (paMiniWindows)
            } // if (cWinInfos)

            pgmwUnlock();
        } // end if (WinRequestMutexSem(G_hmtxWindowList, TIMEOUT_HMTX_WINLIST)

        if (paMiniWindows)
            free(paMiniWindows);

        // if we've found any windows with WINDOW_RESCAN set,
        // go rescan all windows later
        if (fNeedRescan)
            WinPostMsg(G_pHookData->hwndXPagerClient,
                       PGMG_WNDRESCAN,
                       0, 0);

        // unset font so we can delete it if it changes
        GpiSetCharSet(hpsMem, LCID_DEFAULT);
    }
}

/*
 *@@ TrackWithinPager:
 *      implementation for WM_BEGINDRAG in fnwpXPagerClient
 *      to drag and drop the mini-windows within the pager.
 *
 *@@added V0.9.2 (2000-02-23) [umoeller]
 *@@changed V0.9.6 (2000-11-06) [umoeller]: disabled dragging of WPS desktop
 *@@changed V0.9.11 (2001-04-25) [umoeller]: added tracking of entire XPager frame
 *@@changed V0.9.13 (2001-07-06) [umoeller]: added ctrl support; if not pressed, we jump to next desktop
 *@@changed V0.9.18 (2002-02-11) [lafaix]: changed ctrl to shift, made it work too
 */

VOID TrackWithinPager(HWND hwnd,
                      MPARAM mp1,
                      PPAGERCLIENTDATA pClientData)
{
    HWND        hwndTracked;
    SWP         swpTracked;

    LONG        lMouseX = SHORT1FROMMP(mp1),
                lMouseY = SHORT2FROMMP(mp1);

    // shortcuts to global config
    PPAGERCONFIG pXPagerConfig = &G_pHookData->XPagerConfig;
    PPOINTL         pptlMaxDesktops = &pXPagerConfig->ptlMaxDesktops;

    PSIZEL          pszlClient = &pClientData->szlClient;

    hwndTracked = pgmwGetWindowFromClientPoint(lMouseX, lMouseY);

    if (    (hwndTracked == NULLHANDLE)
         || (hwndTracked == G_pHookData->hwndWPSDesktop) // V0.9.6 (2000-11-06) [umoeller]
       )
    {
        // user has started dragging on a non-mini window
        // (XPager client background): drag the entire
        // window then V0.9.11 (2001-04-25) [umoeller]
        WinSendMsg(G_pHookData->hwndXPagerFrame,
                   WM_TRACKFRAME,
                   (MPARAM)TF_MOVE,
                   0);
    }
    else
    {
        // user has started dragging a mini window: track
        // that one then
        TRACKINFO       ti;
        float           fScale_X,
                        fScale_Y;
        POINTL          ptlInitial;

        WinQueryWindowPos(hwndTracked, &swpTracked);
        ti.cxBorder = 1;
        ti.cyBorder = 1;

        ti.fs = TF_STANDARD | TF_MOVE | TF_SETPOINTERPOS | TF_PARTINBOUNDARY; // V0.9.18 (2002-02-14) [lafaix] was TF_ALLINBOUNDARY;

        if (WinGetKeyState(HWND_DESKTOP, VK_SHIFT) & 0x8000)  // V0.9.13 (2001-07-06) [umoeller]
        {
            // shift pressed: set grid to each desktop
            ti.cxGrid = pszlClient->cx / pptlMaxDesktops->x;
            ti.cyGrid = pszlClient->cy / pptlMaxDesktops->y;
            ti.fs |= TF_GRID;
        }
        else
        {
            // shift not pressed: allow any position
            ti.cxGrid = 1;
            ti.cyGrid = 1;
        }

        ti.cxKeyboard = 1;
        ti.cyKeyboard = 1;

        fScale_X = (float)(pptlMaxDesktops->x * G_szlEachDesktopReal.cx)
                           / pszlClient->cx;
        fScale_Y = (float)(pptlMaxDesktops->y * G_szlEachDesktopReal.cy)
                           / pszlClient->cy;

        // for consistency, the track rect must have the size of the mini window
        // it represents
        ti.rclTrack.xLeft = (swpTracked.x + G_ptlCurrPos.x) / fScale_X;
        ti.rclTrack.yBottom = (   (pptlMaxDesktops->y - 1)
                                * G_szlEachDesktopReal.cy
                                - G_ptlCurrPos.y + swpTracked.y
                              )
                              / fScale_Y + 1;
        ti.rclTrack.xRight =   (swpTracked.x + swpTracked.cx + G_ptlCurrPos.x)
                             / fScale_X + 1; // - 1;
        ti.rclTrack.yTop = (    (pptlMaxDesktops->y - 1)
                              * G_szlEachDesktopReal.cy
                              - G_ptlCurrPos.y
                              + swpTracked.y
                              + swpTracked.cy
                           )
                           / fScale_Y + 2; // - 1;

        ptlInitial.x = ti.rclTrack.xLeft;
        ptlInitial.y = ti.rclTrack.yBottom;

        // add a 1px boundary to the client area so that the moved
        // window remains accessible
        ti.rclBoundary.xLeft = 1;
        ti.rclBoundary.yBottom = 1;
        ti.rclBoundary.xRight = pszlClient->cx -1;
        ti.rclBoundary.yTop = pszlClient->cy -1;

        ti.ptlMinTrackSize.x = 2;
        ti.ptlMinTrackSize.y = 2;
        ti.ptlMaxTrackSize.x = pszlClient->cx;
        ti.ptlMaxTrackSize.y = pszlClient->cy;

        if (WinTrackRect(hwnd,
                         NULLHANDLE,        // hps
                         &ti))
        {
            POINTL ptlTracked;

            if (ti.fs & TF_GRID)
            {
                // if in grid mode, we better keep the exact position we had
                // before, instead of the approximation the track returned
                // V0.9.18 (2002-02-14) [lafaix]
                ptlTracked.x = swpTracked.x + ((ti.rclTrack.xLeft - ptlInitial.x) / ti.cxGrid) * G_szlEachDesktopReal.cx;
                ptlTracked.y = swpTracked.y + ((ti.rclTrack.yBottom - ptlInitial.y) / ti.cyGrid) * G_szlEachDesktopReal.cy;
            }
            else
            {
                // if the x position hasn't changed, keep the exact x position
                // we had before V0.9.18 (2002-02-14) [lafaix]
                if (ti.rclTrack.xLeft != ptlInitial.x)
                {
                    ptlTracked.x = (ti.rclTrack.xLeft * fScale_X) - G_ptlCurrPos.x;
                }
                else
                    ptlTracked.x = swpTracked.x;

                // if the y position hasn't changed, keep the exact y position
                // we had before V0.9.18 (2002-02-14) [lafaix]
                if (ti.rclTrack.yBottom != ptlInitial.y)
                {
                    ptlTracked.y = (ti.rclTrack.yBottom * fScale_Y)
                                   -  (   (pptlMaxDesktops->y - 1)
                                          * G_szlEachDesktopReal.cy
                                          - G_ptlCurrPos.y
                                      );
                }
                else
                    ptlTracked.y = swpTracked.y;
            }

            // ?? [lafaix]
//            swpTracked.x -=   swpTracked.x % 16
//                            + WinQuerySysValue(HWND_DESKTOP, SV_CXDLGFRAME);


            // disable message processing in the hook
            if (pgmcDisableSwitching())
            {
                WinSetWindowPos(hwndTracked,
                                HWND_TOP,
                                ptlTracked.x,
                                ptlTracked.y,
                                0,
                                0,
                                SWP_MOVE | SWP_NOADJUST);

                pgmcReenableSwitching();

                WinPostMsg(hwnd,
                           PGMG_INVALIDATECLIENT,
                           (MPARAM)TRUE,        // immediately
                           0);
                        // V0.9.2 (2000-02-22) [umoeller]
            }
        }
    }
}

/*
 *@@ ClientCreate:
 *      implementation for WM_CREATE in fnwpXPagerClient.
 *
 *@@added V0.9.7 (2001-01-18) [umoeller]
 *@@changed V0.9.9 (2001-03-15) [lafaix]: fixed initial desktop position
 */

VOID ClientCreate(HWND hwnd,
                  PPAGERCLIENTDATA pClientData)
{
    // shortcuts to global config
    PPAGERCONFIG pXPagerConfig = &G_pHookData->XPagerConfig;
    // PPOINTL         pptlMaxDesktops = &pXPagerConfig->ptlMaxDesktops;

    G_ptlCurrPos.x =    (pXPagerConfig->ptlStartDesktop.x - 1)
                      * G_szlEachDesktopReal.cx;
    G_ptlCurrPos.y =    (pXPagerConfig->ptlStartDesktop.y - 1)
                      * G_szlEachDesktopReal.cy;
        // V0.9.9 (2001-03-15) [lafaix]: desktops pos start at _upper_ left

    strcpy(pClientData->szFaceName, "2.System VIO");

    WinPostMsg(hwnd,
               PGMG_ZAPPO,
               // frame initially has title bar; so if this is
               // disabled, we need to hide it
               (MPARAM)(!pXPagerConfig->fShowTitlebar),
               MPVOID);

    pClientData->fClientBitmapNeedsUpdate = TRUE;
}

/*
 *@@ ClientPaint:
 *      implementation for WM_PAINT in fnwpXPagerClient.
 *
 *@@added V0.9.7 (2001-01-18) [umoeller]
 */

VOID ClientPaint(HWND hwnd,
                 PPAGERCLIENTDATA pClientData)
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
              3,
              ptlCopy,
              ROP_SRCCOPY,
              BBO_IGNORE);
    WinEndPaint(hps);
}

/*
 *@@ ClientPaint:
 *      implementation for WM_PRESPARAMCHANGED in fnwpXPagerClient.
 *
 *@@added V0.9.7 (2001-01-18) [umoeller]
 */

VOID ClientPresParamChanged(HWND hwnd,
                            MPARAM mp1,
                            PPAGERCLIENTDATA pClientData)
{
    if (LONGFROMMP(mp1) == PP_FONTNAMESIZE)
    {
        HPS             hps;
        FATTRS          FontAttrs = {0};
        FONTMETRICS     fm;
        CHAR            szLocFacename[PGMG_TEXTLEN];

        WinQueryPresParam(hwnd,
                          PP_FONTNAMESIZE,
                          0,
                          NULL,
                          sizeof(szLocFacename),
                          &szLocFacename,
                          0);
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
        // LCID_PAGER_FONT has already been used, GPIREF says
        GpiCreateLogFont(pClientData->hpsMem,
                         NULL,
                         LCID_PAGER_FONT,
                         &FontAttrs);

        WinPostMsg(hwnd,
                   PGMG_INVALIDATECLIENT,
                   (MPARAM)TRUE,        // immediately
                   0);
    }
}

/*
 *@@ ClientSize:
 *      implementation for WM_SIZE in fnwpXPagerClient.
 *
 *@@added V0.9.7 (2001-01-18) [umoeller]
 *@@changed V0.9.18 (2002-02-19) [lafaix]: added G_szlXPagerClient handling
 */

VOID ClientSize(HWND hwnd,
                MPARAM mp2,
                PPAGERCLIENTDATA pClientData)
{
    PSIZEL      pszlClient = &pClientData->szlClient;
    SHORT       cxNew = SHORT1FROMMP(mp2),
                cyNew = SHORT2FROMMP(mp2);

    if (    (pszlClient->cx != cxNew)
         || (pszlClient->cy != cyNew)
       )
    {
        // size changed, or first call (then pszlClient is 0, 0):

        // shortcuts to global config
        PPAGERCONFIG pXPagerConfig = &G_pHookData->XPagerConfig;
        PPOINTL         pptlMaxDesktops = &pXPagerConfig->ptlMaxDesktops;

        pszlClient->cx = cxNew;
        pszlClient->cy = cyNew;

        pClientData->bmihMem.cx = pszlClient->cx;
        pClientData->bmihMem.cy = pszlClient->cy;

        // V0.9.18 (2002-02-19) [lafaix]
        G_szlXPagerClient.cx = pszlClient->cx;
        G_szlXPagerClient.cy = pszlClient->cy;

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
            WinSetPresParam(hwnd,
                            PP_FONTNAMESIZE,
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
 *      in fnwpXPagerClient.
 *
 *@@added V0.9.7 (2001-01-18) [umoeller]
 */

MRESULT ClientButtonClick(HWND hwnd,
                          ULONG msg,
                          MPARAM mp1)
{
    ULONG       ulMsg = PGOM_CLICK2ACTIVATE;
    LONG        lMouseX = SHORT1FROMMP(mp1),
                lMouseY = SHORT2FROMMP(mp1);

    // disable message processing in the hook
    // G_pHookData->fDisableSwitching = TRUE;

    if (msg == WM_BUTTON2CLICK)
        ulMsg = PGOM_CLICK2LOWER;

    WinPostMsg(G_pHookData->hwndXPagerMoveThread,
               ulMsg,
               (MPARAM)lMouseX,
               (MPARAM)lMouseY);
        // this posts PGMG_CHANGEACTIVE back to us

    // reset the hook to its proper value
    // G_pHookData->fDisableSwitching = FALSE;

    if (G_pHookData->XPagerConfig.fFlash)
        pgmgcStartFlashTimer();

    return ((MPARAM)TRUE);      // processed
}

/*
 *@@ ClientDestroy:
 *      implementation for WM_DESTROY in fnwpXPagerClient.
 *
 *@@added V0.9.7 (2001-01-18) [umoeller]
 *@@changed V0.9.16 (2001-11-21) [pr]: fixed broken repaint after destroy
 */

VOID ClientDestroy(PPAGERCLIENTDATA pClientData)
{
    GpiSetBitmap(pClientData->hpsMem, NULLHANDLE);
            // this was missing V0.9.7 (2001-01-18) [umoeller]
    GpiDeleteBitmap(pClientData->hbmClient);
    GpiAssociate(pClientData->hpsMem, NULLHANDLE);
            // this was missing V0.9.7 (2001-01-18) [umoeller], geese!
    GpiDestroyPS(pClientData->hpsMem);
    DevCloseDC(pClientData->hdcMem);
    memset(pClientData, 0, sizeof(PAGERCLIENTDATA)); // V0.9.16 (2001-11-21) [pr]: this was missing!
}

/*
 *@@ ClientZappo:
 *      implementation for PGMG_ZAPPO in fnwpXPagerClient.
 *
 *@@added V0.9.7 (2001-01-18) [umoeller]
 */

VOID ClientZappo(PPAGERCLIENTDATA pClientData)
{
    HWND hwndXPagerFrame = G_pHookData->hwndXPagerFrame;

    if (!G_pHookData->XPagerConfig.fShowTitlebar)
    {
        // titlebar disabled:
        pClientData->hwndSaveSysMenu = WinWindowFromID(hwndXPagerFrame,
                                                     FID_SYSMENU);
        pClientData->hwndSaveTitlebar = WinWindowFromID(hwndXPagerFrame,
                                                      FID_TITLEBAR);
        pClientData->hwndSaveMinButton = WinWindowFromID(hwndXPagerFrame,
                                                       FID_MINMAX);
        WinSetParent(pClientData->hwndSaveSysMenu, HWND_OBJECT,
                     TRUE);     // redraw
        WinSetParent(pClientData->hwndSaveTitlebar, HWND_OBJECT, TRUE);
        WinSetParent(pClientData->hwndSaveMinButton, HWND_OBJECT, TRUE);
        WinSendMsg(hwndXPagerFrame,
                   WM_UPDATEFRAME,
                   MPFROMLONG(FCF_SIZEBORDER),
                   MPVOID);
    }
    else
    {
        // titlebar enabled:
        WinSetParent(pClientData->hwndSaveSysMenu, hwndXPagerFrame, TRUE);
        WinSetParent(pClientData->hwndSaveTitlebar, hwndXPagerFrame, TRUE);
        WinSetParent(pClientData->hwndSaveMinButton, hwndXPagerFrame, TRUE);
        WinSendMsg(hwndXPagerFrame,
                   WM_UPDATEFRAME,
                   MPFROMLONG(FCF_SIZEBORDER | FCF_SYSMENU |
                              FCF_HIDEBUTTON | FCF_TITLEBAR),
                   MPVOID);
    }
}

/*
 * fnwpXPagerClient:
 *      window procedure of the XPager client window,
 *      which was created by pgmcCreateMainControlWnd.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 *@@changed V0.9.3 (2000-04-09) [umoeller]: now using RGB colors
 *@@changed V0.9.4 (2000-07-10) [umoeller]: fixed flashing; now using a win timer
 *@@changed V0.9.4 (2000-07-10) [umoeller]: now suspending flashing when dragging in pager
 *@@changed V0.9.4 (2000-08-08) [umoeller]: fixed various update/formatting problems
 *@@changed V0.9.7 (2001-01-18) [umoeller]: extracted all the Client* funcs
 *@@changed V0.9.7 (2001-01-18) [umoeller]: reduced memory usage for mem PS, reorganized bitmap code
 *@@changed V0.9.7 (2001-01-18) [umoeller]: removed PGMG_LOCKUP call, pager doesn't need this
 */

MRESULT EXPENTRY fnwpXPagerClient(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    static PAGERCLIENTDATA   ClientData = {0};

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
            #ifndef __DEBUG__           // dblclick below doesn't work otherwise
            case WM_BUTTON2CLICK:
            #endif
                mrc = ClientButtonClick(hwnd, msg, mp1);
            break;

            #ifdef __DEBUG__
            case WM_BUTTON2DBLCLK:
                DosBeep(1000, 50);
//                {
//                    USHORT i;
//                    ULONG ulStartTime, ulEndTime;
//
//                    _Pmpf(("About to update client bitmap..."));
//                    DosQuerySysInfo(QSV_MS_COUNT, QSV_MS_COUNT, &ulStartTime, sizeof(ULONG));
//
//                    for (i = 0; i < 200; i++)
//                      UpdateClientBitmap(&ClientData);
//
//                    DosQuerySysInfo(QSV_MS_COUNT, QSV_MS_COUNT, &ulEndTime, sizeof(ULONG));
//                    _Pmpf(("Done in %d ms", ulEndTime - ulStartTime));
//                }
                DumpAllWindows();
            break;
            #endif

            /*
             * WM_BUTTON2MOTIONSTART:
             *      move the windows within the pager.
             */

            // case WM_BUTTON2MOTIONSTART:
            case WM_BEGINDRAG:
            {
                if (G_pHookData->XPagerConfig.fFlash)
                    WinStopTimer(G_habDaemon,
                                 hwnd,
                                 2);

                TrackWithinPager(hwnd,
                                 mp1,
                                 &ClientData);

                if (G_pHookData->XPagerConfig.fFlash)
                    pgmgcStartFlashTimer();
            }
            break;

            /*
             * WM_CLOSE:
             *      window being closed:
             *      notify XFLDR.DLL and restore windows.
             */

            case WM_CLOSE:
                // _Pmpf(("fnwpXPagerClient: WM_CLOSE, notifying Kernel"));
                dmnKillXPager(TRUE);  // notify XFLDR.DLL
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
                        WinShowWindow(G_pHookData->hwndXPagerFrame, FALSE);
                }
            }
            break;

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
            }
            break;

            /*
             *@@ PGMG_WNDRESCAN:
             *      posted only from UpdateClientBitmap if any
             *      windows with the WINDOW_RESCAN flag
             *      have been encountered. In that case,
             *      we need to re-scan the window list.
             */

            case PGMG_WNDRESCAN:
                _Pmpf(("PGMG_WNDRESCAN"));
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

                if (WinIsWindowEnabled(hwnd2Activate))
                        // fixed V0.9.2 (2000-02-23) [umoeller]
                {
                    WinSetActiveWindow(HWND_DESKTOP, hwnd2Activate);
                            // this can be on any desktop, even if it's
                            // not the current one... the hook will determine
                            // that the active window has changed and have
                            // the windows repositioned accordingly.
                }

                WinPostMsg(hwnd,
                           PGMG_INVALIDATECLIENT,
                           (MPARAM)TRUE,        // immediately
                           0);
                                // V0.9.2 (2000-02-22) [umoeller]
            }
            break;

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

                // disable message processing in the hook
                if (pgmcDisableSwitching())
                {
                     WinSetWindowPos(hwnd2Lower,
                                     HWND_BOTTOM,
                                     0,0,0,0,
                                     SWP_ZORDER);
                     pgmcReenableSwitching();
                }

                WinPostMsg(hwnd,
                           PGMG_INVALIDATECLIENT,
                           (MPARAM)TRUE,        // immediately
                           0);
            }
            break;

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
}

/*
 * fnwpSubclXPagerFrame:
 *      window procedure for the subclassed XPager
 *      frame, which was subclassed by pgmcCreateMainControlWnd.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 *@@changed V0.9.4 (2000-08-08) [umoeller]: fixed various update/formatting problems
 */

MRESULT EXPENTRY fnwpSubclXPagerFrame(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
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

            if (G_pHookData->XPagerConfig.fPreserveProportions)
                pswp->cy = pgmcCalcNewFrameCY(pswp->cx);

            // we never want to become active:
            if (pswp->fl & SWP_ACTIVATE)
            {
                pswp->fl &= ~SWP_ACTIVATE;
                pswp->fl |= SWP_DEACTIVATE;
            }

            mrc = G_pfnOldFrameWndProc(hwnd, msg, mp1, mp2);
        }
        break;

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
         *      the XPager window. We need to stop the
         *      flash timer while this is going on.
         */

        case WM_TRACKFRAME:
            // stop flash timer, if running
            if (G_pHookData->XPagerConfig.fFlash)
                WinStopTimer(G_habDaemon,
                             G_pHookData->hwndXPagerClient,
                             2);

            // now track window (WC_FRAME runs a modal message loop)
            mrc = G_pfnOldFrameWndProc(hwnd, msg, mp1, mp2);

            if (G_pHookData->XPagerConfig.fFlash)
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
            ptr = (PTRACKINFO)PVOIDFROMMP(mp2);
            ptr->ptlMinTrackSize.x = 20;
            ptr->ptlMinTrackSize.y = 20;

            mrc = MRFROMSHORT(TRUE);
        }
        break;

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
        }
        break;

        default:
            mrc = G_pfnOldFrameWndProc(hwnd, msg, mp1, mp2);
    }

    return (mrc);

}


