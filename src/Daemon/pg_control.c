
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
 *      Since XWorkplace implementation had very little
 *      in common with PageMage any more, it has been
 *      renamed to "XPager" with 0.9.18. With V0.9.19,
 *      the remaining parts have been rewritten, so this
 *      is no longer based on PageMage.
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
 *          The code for this is in pg_winlist.c.
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
#define INCL_WINSWITCHLIST

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
#include "helpers\gpih.h"
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\standards.h"
#include "helpers\threads.h"

#include "xwpapi.h"                     // public XWorkplace definitions
#include "shared\kernel.h"              // XWorkplace Kernel

#include "hook\xwphook.h"
#include "hook\hook_private.h"
#include "hook\xwpdaemn.h"              // XPager and daemon declarations

/* ******************************************************************
 *
 *   Private definitions
 *
 ********************************************************************/

/*
 *@@ PAGERWINDATA:
 *
 *@@added V0.9.19 (2002-05-07) [umoeller]
 */

typedef struct _PAGERWINDATA
{
    USHORT      cb;                 // required for WM_CREATE

    PFNWP       pfnwpOrigFrame;

    SIZEL       szlClient;          // current client size

    PXBITMAP    pbmClient;          // client XBITMAP
    HBITMAP     hbmTemplate;        // "empty" bitmap with background and lines
                                    // used whenever client needs refresh for speed

    FATTRS      fattr;              // font attrs caught in presparamschanged
                                    // so we can quickly create a font for
                                    // the bitmap's memory PS

    BOOL        fNeedsRefresh;

} PAGERWINDATA, *PPAGERWINDATA;

#define TIMERID_PGR_ACTIVECHANGED   1
#define TIMERID_PGR_FLASH           2
#define TIMERID_PGR_REFRESHDIRTIES  3

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

static PCSZ         WC_PAGER = "XWPXPagerClient";

HMTX                G_hmtxDisableSwitching = NULLHANDLE;    // V0.9.14 (2001-08-25) [umoeller]

THREADINFO          G_tiMoveThread = {0};

extern HAB          G_habDaemon;        // xwpdaemn.c

/* ******************************************************************
 *
 *   XPager hook serialization
 *
 ********************************************************************/

/*
 *@@ pgrLockHook:
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
 *      Call pgrUnlockHook to clear the flag
 *      again, which will release the mutex too.
 *
 *@@added V0.9.14 (2001-08-25) [umoeller]
 *@@changed V0.9.19 (2002-05-07) [umoeller]: now allowing recursive calls on the same thread
 */

BOOL pgrLockHook(PCSZ pcszFile, ULONG ulLine, PCSZ pcszFunction)
{
    if (!G_hmtxDisableSwitching)
    {
        // first call:
        DosCreateMutexSem(NULL, &G_hmtxDisableSwitching, 0, FALSE);
    }

    if (!WinRequestMutexSem(G_hmtxDisableSwitching, 4000))
        // WinRequestMutexSem works even if the thread has no message queue
    {
        ++(G_pHookData->cDisablePagerSwitching);
                // V0.9.19 (2002-05-07) [umoeller]
        return TRUE;
    }

    DosBeep(100, 1000);
    return FALSE;
}

/*
 *@@ pgrUnlockHook:
 *
 *@@added V0.9.14 (2001-08-25) [umoeller]
 */

VOID pgrUnlockHook(VOID)
{
    if (G_pHookData->cDisablePagerSwitching)
    {
        --(G_pHookData->cDisablePagerSwitching);
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

static VOID DumpAllWindows(VOID)
{
    if (pgrLockWinlist())
    {
        ULONG ul = 0;
        PLISTNODE pNode = lstQueryFirstNode(&G_llWinInfos);
        while (pNode)
        {
            PPAGERWININFO pEntryThis = (PPAGERWININFO)pNode->pItemData;
            _Pmpf(("Dump %d: hwnd 0x%lX \"%s\":\"%s\" pid 0x%lX(%d) type %d",
                   ul++,
                   pEntryThis->hwnd,
                   pEntryThis->szSwtitle,
                   pEntryThis->szClassName,
                   pEntryThis->pid,
                   pEntryThis->pid,
                   pEntryThis->bWindowType));

            pNode = pNode->pNext;
        }

        pgrUnlockWinlist();
    }
}

#endif

/* ******************************************************************
 *
 *   Helpers
 *
 ********************************************************************/

/*
 *@@ pgrCalcClientCY:
 *      calculates the new client height according to
 *      the frame width, so that the frame can be
 *      sized proportionally.
 *
 *@@added V0.9.2 (2000-02-23) [umoeller]
 *@@changed V0.9.3 (2000-04-09) [umoeller]: now taking titlebar setting into account
 */

LONG pgrCalcClientCY(LONG cx)
{
    // shortcuts to global config
    LONG lHeightPercentOfWidth = G_pHookData->lCYScreen * 100 / G_pHookData->lCXScreen;
                // e.g. 75 for 1024x768
    LONG lCY =      (  (cx * lHeightPercentOfWidth / 100)
                       * G_pHookData->PagerConfig.cDesktopsY
                       / G_pHookData->PagerConfig.cDesktopsX
                    );
    return (lCY);
}

/*
 *@@ CheckFlashTimer:
 *      shortcut to start the flash timer if
 *      flashing is enabled.
 *
 *@@added V0.9.19 (2002-05-07) [umoeller]
 */

static VOID CheckFlashTimer(VOID)
{
    if (G_pHookData->PagerConfig.flPager & PGRFL_FLASHTOTOP)
        WinStartTimer(G_habDaemon,
                      G_pHookData->hwndPagerClient,
                      TIMERID_PGR_FLASH,
                      G_pHookData->PagerConfig.ulFlashDelay);
}

/*
 *@@ pgrIsShowing:
 *      returns TRUE if the specified window is at least
 *      partially visible on the current desktp.
 *
 *      Based on a code snipped by Dmitry Kubov.
 *
 *@@added V0.9.7 (2001-01-21) [umoeller]
 */

BOOL pgrIsShowing(PSWP pswp)
{
    // this was rewritten V0.9.7 (2001-01-18) [umoeller]
    LONG        bx = WinQuerySysValue(HWND_DESKTOP, SV_CXSIZEBORDER);
    LONG        by = WinQuerySysValue(HWND_DESKTOP, SV_CYSIZEBORDER);

    return (     !(    // is right edge too wide to the left?
                       ((pswp->x + bx) >= G_pHookData->szlEachDesktopFaked.cx)
                    || ((pswp->x + pswp->cx - bx) <= 0)
                    || ((pswp->y + by) >= G_pHookData->szlEachDesktopFaked.cy)
                    || ((pswp->y + pswp->cy - by) <= 0)
                  )
            );
}

/*
 *@@ pgrRecoverWindows:
 *      recovers all windows (hopefully).
 *
 *      Gets called on WM_CLOSE in fnwpXPagerClient.
 *      This makes sure that when XPager terminates
 *      for whatever reason, the user isn't left with
 *      windows which are completely off the screen.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 */

VOID pgrRecoverWindows(HAB hab)
{
    #define MAX_WINDOWS 1000        // yes, that's a lot... too lazy
                                    // to build another linked list here

    HENUM       henum;
    HWND        hwndTemp[MAX_WINDOWS];
    SWP         swpTemp[MAX_WINDOWS];
    ULONG       ul,
                ul2;
    CHAR        szClassName[30];

    LONG    cxEach = G_pHookData->szlEachDesktopFaked.cx,
            cDesktopsX = G_pHookData->PagerConfig.cDesktopsX,
            cyEach = G_pHookData->szlEachDesktopFaked.cy,
            cDesktopsY = G_pHookData->PagerConfig.cDesktopsY;

    ul = 0;
    henum = WinBeginEnumWindows(HWND_DESKTOP);
    while ((hwndTemp[ul] = WinGetNextWindow(henum)))
    {
        HWND    hwndThis = hwndTemp[ul];
        PSWP    pswpThis = &swpTemp[ul];
        if (    (WinQueryWindowPos(hwndThis, pswpThis))
             && (WinQueryClassName(hwndThis, sizeof(szClassName), szClassName))
             && ((pswpThis->fl & SWP_HIDE) == 0)
             && (!(pswpThis->fl & SWP_MINIMIZE))
             && (WinIsChild(hwndThis, HWND_DESKTOP))
             && (strcmp(szClassName, "#32765"))
                // not WPS window:
             && (hwndThis != G_pHookData->hwndWPSDesktop)
             && (!pgrIsShowing(pswpThis))
             && (!WinIsChild(hwndThis, G_pHookData->hwndPagerFrame))
           )
            ul++;
    }
    WinEndEnumWindows(henum);

    for (ul2 = 0; ul2 < ul; ul2++)
    {
        PSWP         pswpThis = &swpTemp[ul2];
        WinQueryWindowPos(hwndTemp[ul2],
                          pswpThis);

        pswpThis->fl = SWP_MOVE;
        if (!WinIsWindowVisible(hwndTemp[ul2]))
            pswpThis->fl |= SWP_HIDE;

        pswpThis->x =    (   (swpTemp[ul2].x)
                              + (   cDesktopsX
                                    * cxEach
                                )
                            ) % cxEach;
        pswpThis->y = (   (swpTemp[ul2].y)
                              + (   cDesktopsY
                                    * cyEach
                                )
                            ) % cyEach;

        if (pswpThis->x > cxEach - 5)
            pswpThis->x = pswpThis->x - cxEach;
        if (pswpThis->y > cyEach - 5)
            pswpThis->y = pswpThis->y - cyEach;

        pswpThis->hwnd = hwndTemp[ul2];
        pswpThis->hwndInsertBehind = HWND_TOP;
    }

    WinSetMultWindowPos(hab, swpTemp, ul);
}

/*
 *@@ GRID_X:
 *      returns the X coordinate of the grid line which
 *      is to the left of the given desktop no.
 *
 *      lDesktopX == 0 specifies the leftmost desktop.
 *
 *@@added V0.9.19 (2002-05-07) [umoeller]
 */

#define GRID_X(cx, lDesktopX) (cx * (lDesktopX) / G_pHookData->PagerConfig.cDesktopsX)

/*
 *@@ GRID_Y:
 *      returns the Y coordinate of the grid line which
 *      is to the bottom of the given desktop no.
 *
 *      lDesktopY == 0 specifies the bottommost desktop.
 *
 *@@added V0.9.19 (2002-05-07) [umoeller]
 */

#define GRID_Y(cy, lDesktopY) (cy * (lDesktopY) / G_pHookData->PagerConfig.cDesktopsY)

/*
 *@@ CreateTemplateBmp:
 *      creates and returns the "template bitmap",
 *      which is the "empty" bitmap for the background
 *      of the pager. This includes the color fading,
 *      if enabled, and the desktop grid lines only.
 *
 *      We create the bitmap in RGB mode to profit
 *      from dithering if the display is not running
 *      in true color mode.
 *
 *@@added V0.9.19 (2002-05-07) [umoeller]
 */

static HBITMAP CreateTemplateBmp(HAB hab,
                                 PSIZEL pszl)
{
    HBITMAP hbmTemplate = NULLHANDLE;
    PXBITMAP pbmTemplate;
    if (pbmTemplate = gpihCreateXBitmap2(hab,
                                         pszl->cx,
                                         pszl->cy,
                                         1,
                                         24))
    {
        BKGNDINFO info;
        LONG l;
        HPS hpsMem = pbmTemplate->hpsMem;
        POINTL ptl;
        RECTL rclBkgnd;
        LONG lMax;
        info.flPaintMode = G_pHookData->PagerConfig.flPaintMode;
        info.hbm = NULLHANDLE;
        info.lcol1 = G_pHookData->PagerConfig.lcolDesktop1;
        info.lcol2 = G_pHookData->PagerConfig.lcolDesktop2;
        rclBkgnd.xLeft = 0;
        rclBkgnd.yBottom = 0;
        rclBkgnd.xRight = pszl->cx - 1;
        rclBkgnd.yTop = pszl->cy - 1;
        gpihFillBackground(hpsMem,
                           &rclBkgnd,
                           &info);

        // draw the desktop grid on top of that
        GpiSetColor(hpsMem,
                    G_pHookData->PagerConfig.lcolGrid);

        // a) verticals (X separators)
        lMax = G_pHookData->PagerConfig.cDesktopsX;
        for (l = 1;
             l < lMax;
             ++l)
        {
            ptl.x = GRID_X(pszl->cx, l);
            ptl.y = 0;
            GpiMove(hpsMem, &ptl);
            ptl.y = pszl->cy - 1;
            GpiLine(hpsMem, &ptl);
        }

        // b) horizontals (Y separators)
        lMax = G_pHookData->PagerConfig.cDesktopsY;
        for (l = 1;
             l < lMax;
             ++l)
        {
            ptl.x = 0;
            ptl.y = GRID_Y(pszl->cy, l);
            GpiMove(hpsMem, &ptl);
            ptl.x = pszl->cx - 1;
            GpiLine(hpsMem, &ptl);
        }

        // detach this because otherwise we can't bitblt
        hbmTemplate = gpihDetachBitmap(pbmTemplate);
        gpihDestroyXBitmap(&pbmTemplate);
    }

    return hbmTemplate;
}

/*
 *@@ DestroyBitmaps:
 *      nukes the two member bitmaps to enforce
 *      a complete refresh on the next paint.
 *
 *@@added V0.9.19 (2002-05-07) [umoeller]
 */

static VOID DestroyBitmaps(PPAGERWINDATA pWinData)
{
    if (pWinData->hbmTemplate)
    {
        GpiDeleteBitmap(pWinData->hbmTemplate);
        pWinData->hbmTemplate = NULLHANDLE;
    }
    gpihDestroyXBitmap(&pWinData->pbmClient);
}

/*
 *@@ MINIWINDOW:
 *      representation of a WININFO entry in the
 *      XPager client. This is only used for
 *      a temporary list in RefreshPagerBmp.
 *
 *@@added V0.9.7 (2001-01-21) [umoeller]
 */

typedef struct _MINIWINDOW
{
    HWND            hwnd;           // window handle

    // calculated rectangle for mini window
    // in client coordinates (inclusive)
    POINTL          ptlLowerLeft,
                    ptlTopRight;
    PPAGERWININFO   pWinInfo;       // ptr to PAGERWININFO this item
                                    // represents; always valid
} MINIWINDOW, *PMINIWINDOW;

/*
 *@@ RefreshPagerBmp:
 *
 *      Calls CreateTemplateBmp if necessary.
 *
 *      Preconditions:
 *
 *      --  The client bitmap must already have
 *          been created, but it is completely
 *          overpainted here.
 *
 *@@added V0.9.19 (2002-05-07) [umoeller]
 */

static VOID RefreshPagerBmp(HWND hwnd,
                            PPAGERWINDATA pWinData)
{
    HPS hpsMem = pWinData->pbmClient->hpsMem;
    POINTL ptl;

    LONG        cxEach = G_pHookData->szlEachDesktopFaked.cx,
                cyEach = G_pHookData->szlEachDesktopFaked.cy,
                xCurrent = G_pHookData->ptlCurrentDesktop.x,
                yCurrent = G_pHookData->ptlCurrentDesktop.y,
                cxClient = pWinData->szlClient.cx,
                cyClient = pWinData->szlClient.cy;

    ULONG       xDesktop = xCurrent / cxEach;
    ULONG       yDesktop = yCurrent / cyEach;

    ULONG flPager = G_pHookData->PagerConfig.flPager;

    // check if we need a new template bitmap for background
    // (this gets destroyed on resize only)
    if (!pWinData->hbmTemplate)
        pWinData->hbmTemplate = CreateTemplateBmp(G_habDaemon,
                                                  &pWinData->szlClient);

    // a) bitblt the template into the client bitmap
    if (pWinData->hbmTemplate)
    {
        POINTL  aptl[4];
        memset(aptl, 0, sizeof(POINTL) * 4);
        aptl[1].x = cxClient - 1;
        aptl[1].y = cyClient - 1;
        aptl[3].x = cxClient;
        aptl[3].y = cyClient;
        GpiWCBitBlt(hpsMem,         // target HPS (bmp selected)
                    pWinData->hbmTemplate,        // source bmp
                    4L,             // must always be 4
                    &aptl[0],       // points array
                    ROP_SRCCOPY,
                    BBO_IGNORE);
    }

    // b) paint hatch for active desktop
    GpiSetColor(hpsMem, G_pHookData->PagerConfig.lcolActiveDesktop);
    GpiSetPattern(hpsMem, PATSYM_DIAG1);

    ptl.x = GRID_X(cxClient, xDesktop) + 1;
    ptl.y = GRID_Y(cyClient, yDesktop) + 1;

    GpiMove(hpsMem, &ptl);
    ptl.x = GRID_X(cxClient, xDesktop + 1) -1;
    ptl.y = GRID_Y(cyClient, yDesktop + 1) - 1;
    GpiBox(hpsMem,
           DRO_FILL,
           &ptl,
           0,
           0);

    GpiSetPattern(hpsMem, PATSYM_DEFAULT);

    // c) paint mini-windows
    if (flPager & PGRFL_MINIWINDOWS)
    {
        PMINIWINDOW     paMiniWindows = NULL;
        ULONG           cWinInfos = 0;
        BOOL            fDirtiesFound = FALSE;

        // lock the window list all the while we're doing this
        // V0.9.7 (2001-01-21) [umoeller]
        if (pgrLockWinlist())
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

            HENUM           henum = NULLHANDLE;

            if (    (cWinInfos = lstCountItems(&G_llWinInfos))
                 && (paMiniWindows = (PMINIWINDOW)malloc(cWinInfos * sizeof(MINIWINDOW)))
                 && (henum = WinBeginEnumWindows(HWND_DESKTOP))
               )
            {
                ULONG   cMiniWindowsUsed = 0;

                HWND    hwndThis;

                double  dScale_X =   (double)G_pHookData->PagerConfig.cDesktopsX
                                   * cxEach
                                   / cxClient,
                        dScale_Y =   (double)G_pHookData->PagerConfig.cDesktopsY
                                   * cyEach
                                   / cyClient;

                while (hwndThis = WinGetNextWindow(henum))
                {
                    PPAGERWININFO pWinInfo;
                    // go thru local list and find the
                    // current enumeration window
                    if (    (WinIsWindowVisible(hwndThis))
                         && (pWinInfo = pgrFindWinInfo(hwndThis,
                                                       NULL))
                       )
                    {
                        // item is on list:
                        ULONG   flSaved = pWinInfo->swp.fl;
                        BYTE    bTypeThis = pWinInfo->bWindowType;

                        // update window pos in window list
                        WinQueryWindowPos(pWinInfo->hwnd,
                                          &pWinInfo->swp);

                        // update the bWindowType status, as restoring
                        // a previously minimized or hidden window may
                        // change it
                        // (we don't have to update "special" windows)
                        // V0.9.18 (2002-02-20) [lafaix]
                        if (    (flSaved != pWinInfo->swp.fl)
                             && (    (bTypeThis == WINDOW_NORMAL)
                                  || (bTypeThis == WINDOW_MINIMIZE)
                                  || (bTypeThis == WINDOW_MAXIMIZE)
                                )
                           )
                        {
                            bTypeThis = WINDOW_NORMAL;
                            if (pWinInfo->swp.fl & SWP_MINIMIZE)
                                bTypeThis = WINDOW_MINIMIZE;
                            else
                            if (pWinInfo->swp.fl & SWP_MAXIMIZE)
                                bTypeThis = WINDOW_MAXIMIZE;
                        }

                        if (bTypeThis == WINDOW_DIRTY)
                        {
                            // window is in unknown state:
                            if (!pgrGetWinInfo(pWinInfo))
                                fDirtiesFound = TRUE;
                        }

                        if (    (bTypeThis == WINDOW_NORMAL)
                             || (bTypeThis == WINDOW_MAXIMIZE)
                             || (    (bTypeThis == WINDOW_STICKY)
                                  && (flPager & PGRFL_INCLUDESTICKY)
                                )
                             || (    (bTypeThis == WINDOW_DIRTY)
                                  && (flPager & PGRFL_INCLUDESECONDARY)
                                )
                           )
                        {
                            // this window is to be painted:
                            // use a new item on the MINIWINDOW
                            // array then and calculate the
                            // mapping of the mini window

                            PMINIWINDOW pMiniThis
                                = &paMiniWindows[cMiniWindowsUsed++];

                            LONG xThis = pWinInfo->swp.x + xCurrent,
                                 yThis = pWinInfo->swp.y + yCurrent;

                            // store WININFO ptr; we hold the winlist
                            // locked all the time, so this is safe
                            pMiniThis->pWinInfo = pWinInfo;

                            pMiniThis->hwnd = hwndThis;

                            pMiniThis->ptlLowerLeft.x
                                =   (xThis)
                                  / dScale_X;

                            pMiniThis->ptlLowerLeft.y
                                =   (yThis)
                                  / dScale_Y;

                            pMiniThis->ptlTopRight.x
                                =   (xThis + pWinInfo->swp.cx)
                                  / dScale_X
                                  - 1;

                            pMiniThis->ptlTopRight.y
                                =   (yThis + pWinInfo->swp.cy)
                                  / dScale_Y
                                  - 1;

                        } // end if (    (bTypeThis == WINDOW_NORMAL) ...
                    } // end if (WinIsVisible)
                } // end while ((hwndThis = WinGetNextWindow(henum)) != NULLHANDLE)

                WinEndEnumWindows(henum);

                // now paint

                if (cMiniWindowsUsed)
                {
                    // we got something to paint:
                    // paint the mini windows in reverse order then
                    // (bottom to top)

                    HWND hwndActive = WinQueryActiveWindow(HWND_DESKTOP);
                    // start with the last one
                    LONG l;
                    for (l = cMiniWindowsUsed - 1;
                         l >= 0;
                         --l)
                    {
                        PMINIWINDOW pMiniThis = &paMiniWindows[l];
                        LONG    lcolCenter,
                                lcolText;
                        PCSZ    pcszSwtitle;
                        ULONG   ulSwtitleLen;

                        // draw center
                        if (pMiniThis->hwnd == hwndActive)
                        {
                            // this is the active window:
                            lcolCenter = G_pHookData->PagerConfig.lcolActiveWindow;
                            lcolText = G_pHookData->PagerConfig.lcolActiveText;
                        }
                        else
                        {
                            lcolCenter = G_pHookData->PagerConfig.lcolInactiveWindow;
                            lcolText = G_pHookData->PagerConfig.lcolInactiveText;
                        }

                        GpiSetColor(hpsMem, lcolCenter);
                        GpiMove(hpsMem,
                                &pMiniThis->ptlLowerLeft);
                        GpiBox(hpsMem,
                               DRO_FILL,
                               &pMiniThis->ptlTopRight,
                               0,
                               0);

                        // draw border
                        GpiSetColor(hpsMem,
                                    G_pHookData->PagerConfig.lcolWindowFrame);
                        GpiBox(hpsMem,
                               DRO_OUTLINE,
                               &pMiniThis->ptlTopRight,
                               0,
                               0);

                        // draw window text too?
                        if (    (flPager & PGRFL_MINIWIN_TITLES)
                             && (pcszSwtitle = pMiniThis->pWinInfo->szSwtitle)
                             && (ulSwtitleLen = strlen(pcszSwtitle))
                           )
                        {
                            RECTL   rclText;

                            rclText.xLeft = pMiniThis->ptlLowerLeft.x + 1;
                            rclText.yBottom = pMiniThis->ptlLowerLeft.y + 1;
                            rclText.xRight = pMiniThis->ptlTopRight.x - 2;
                            rclText.yTop = pMiniThis->ptlTopRight.y - 2;

                            WinDrawText(hpsMem,
                                        ulSwtitleLen,
                                        (PSZ)pcszSwtitle,
                                        &rclText,
                                        lcolText,
                                        0,
                                        DT_LEFT | DT_TOP);

                        } // end if (    (G_pHookData->PagerConfig.fShowWindowText) ...
                    }
                } // end if (cMiniWindowsUsed)
            } // if (cWinInfos)

            pgrUnlockWinlist();
        }

        if (paMiniWindows)
            free(paMiniWindows);

        // if we've found any windows with WINDOW_DIRTY set,
        // go refresh windows later
        if (fDirtiesFound)
            WinPostMsg(G_pHookData->hwndPagerClient,
                       PGRM_REFRESHDIRTIES,
                       0,
                       0);
    }
}

/*
 *@@ FindWindow:
 *      returns the window from the list which
 *      matches the given pager client coordinates.
 *      Returns NULLHANDLE if no window is found or
 *      if mini windows are not shown.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 *@@changed V0.9.18 (2002-02-19) [lafaix]: uses G_szlXPagerClient
 *@@changed V0.9.18 (2002-02-20) [lafaix]: reworked to correctly handle pager settings
 */

static HWND FindWindow(PPAGERWINDATA pWinData,
                       PPOINTL ptlClient,
                       BOOL fAllowStickes)         // in: if TRUE, allow returning sticky windows
{
    HWND    hwndResult = NULLHANDLE;

    if (G_pHookData->PagerConfig.flPager & PGRFL_MINIWINDOWS)
    {
        // mini windows are shown: check if the point denotes one
        // of them
        // V0.9.18 (2002-02-20) [lafaix]
        POINTL  ptlCalc;
        double  dCalcX,
                dCalcY;
        HENUM   henumPoint;
        HWND    hwndPoint;
        SWP     swpPoint;

        dCalcX =    (double)ptlClient->x
                  * G_pHookData->szlEachDesktopFaked.cx
                  / (pWinData->szlClient.cx / G_pHookData->PagerConfig.cDesktopsX);
        dCalcY =    (double)ptlClient->y
                  * G_pHookData->szlEachDesktopFaked.cy
                  / (pWinData->szlClient.cy / G_pHookData->PagerConfig.cDesktopsY);

        ptlCalc.x = ((int)dCalcX) - G_pHookData->ptlCurrentDesktop.x;
        ptlCalc.y = ((int)dCalcY) - G_pHookData->ptlCurrentDesktop.y;

        // the WinxxxEnum part is a bit complex, but we can't just use
        // the wininfo list, as it does not reflect the z order

        henumPoint = WinBeginEnumWindows(HWND_DESKTOP);
        while (hwndPoint = WinGetNextWindow(henumPoint))
        {
            if (hwndPoint == G_pHookData->hwndPagerFrame)
                // ignore XPager frame
                continue;

            if (    (WinIsWindowVisible(hwndPoint))
                 && (WinQueryWindowPos(hwndPoint, &swpPoint))
                 && (ptlCalc.x >= swpPoint.x)
                 && (ptlCalc.x <= swpPoint.x + swpPoint.cx)
                 && (ptlCalc.y >= swpPoint.y)
                 && (ptlCalc.y <= swpPoint.y + swpPoint.cy)
               )
            {
                // this window matches the coordinates:
                // check if it's visible in the client window
                // V0.9.18 (2002-02-20) [lafaix]

                if (pgrLockWinlist())
                {
                    PPAGERWININFO pWinInfo = pgrFindWinInfo(hwndPoint, NULL);

                    if (    (pWinInfo)
                         && (    (pWinInfo->bWindowType == WINDOW_NORMAL)
                              || (pWinInfo->bWindowType == WINDOW_MAXIMIZE)
                              || (fAllowStickes && pWinInfo->bWindowType == WINDOW_STICKY)
                            )
                       )
                    {
                        hwndResult = hwndPoint;
                    }

                    pgrUnlockWinlist();

                    if (hwndResult)
                        break;
                }
            }
        }
        WinEndEnumWindows(henumPoint);
    }
    return (hwndResult);
}

/* ******************************************************************
 *
 *   Pager client
 *
 ********************************************************************/

/*
 *@@ PagerPaint:
 *
 *@@added V0.9.19 (2002-05-07) [umoeller]
 */

static VOID PagerPaint(HWND hwnd)
{
    PPAGERWINDATA pWinData;
    HPS hps;
    if (    (pWinData = (PPAGERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER))
         && (hps = WinBeginPaint(hwnd, NULLHANDLE, NULL))
       )
    {
        RECTL rclClient;
        static POINTL ptlNull = {0, 0};
        WinQueryWindowRect(hwnd, &rclClient);

        gpihSwitchToRGB(hps);

        if (!pWinData->pbmClient)
        {
            // we don't have a client bitmap yet,
            // or the window has been resized:
            // create one
            if (pWinData->pbmClient = gpihCreateXBitmap(G_habDaemon,
                                                        rclClient.xRight,
                                                        rclClient.yTop))
            {
                // create logical font for the memory PS
                // (this data has been set in presparamschanged)

                HPS hpsMem = pWinData->pbmClient->hpsMem;
                GpiCreateLogFont(hpsMem,
                                 NULL,
                                 LCID_PAGER_FONT,
                                 &pWinData->fattr);
                GpiSetCharSet(hpsMem, LCID_PAGER_FONT);

                pWinData->fNeedsRefresh = TRUE;
            }
        }

        if (pWinData->pbmClient)
        {
            if (pWinData->fNeedsRefresh)
            {
                // client bitmap needs refresh:
                RefreshPagerBmp(hwnd,
                                pWinData);
                pWinData->fNeedsRefresh = FALSE;
            }

            WinDrawBitmap(hps,
                          pWinData->pbmClient->hbm,
                          NULL,     // whole bitmap
                          &ptlNull,
                          0,
                          0,
                          DBM_NORMAL);

            #ifdef __DEBUG__
            {
                CHAR sz[300];
                sprintf(sz,
                        "%d/%d",
                        G_pHookData->ptlCurrentDesktop.x,
                        G_pHookData->ptlCurrentDesktop.y);
                GpiSetColor(hps, RGBCOL_WHITE);
                WinDrawText(hps,
                            strlen(sz),
                            sz,
                            &rclClient,
                            0,
                            0,
                            DT_LEFT | DT_BOTTOM | DT_TEXTATTRS);
            }
            #endif
        }

        WinEndPaint(hps);
    }
}

/*
 *@@ PagerPresParamChanged:
 *
 *@@added V0.9.19 (2002-05-07) [umoeller]
 */

static VOID PagerPresParamChanged(HWND hwnd, MPARAM mp1)
{
    PPAGERWINDATA pWinData;
    if (    (LONGFROMMP(mp1) == PP_FONTNAMESIZE)
         && (pWinData = (PPAGERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER))
       )
    {
        // get the font metrics of the current window
        // font; if we get a cached micro PS, PM will
        // automatically have the font set, so we can
        // easily get its data and create our own
        // logical font for the bitmap work
        FONTMETRICS fm;
        HPS         hps = WinGetPS(hwnd);
        GpiQueryFontMetrics(hps, sizeof(FONTMETRICS), &fm);

        pWinData->fattr.usRecordLength = sizeof(FATTRS);
        pWinData->fattr.fsSelection = fm.fsSelection;
        pWinData->fattr.lMatch = fm.lMatch;
        strcpy(pWinData->fattr.szFacename, fm.szFacename);
        pWinData->fattr.idRegistry = fm.idRegistry;
        pWinData->fattr.usCodePage = fm.usCodePage;
        pWinData->fattr.lMaxBaselineExt = fm.lMaxBaselineExt;
        pWinData->fattr.lAveCharWidth = fm.lAveCharWidth;

        WinReleasePS(hps);

        WinPostMsg(hwnd,
                   PGRM_REFRESHCLIENT,
                   (MPARAM)FALSE,
                   0);
    }
}

/*
 *@@ PagerPositionFrame:
 *
 *@@added V0.9.19 (2002-05-07) [umoeller]
 */

static VOID PagerPositionFrame(VOID)
{
    // disable message processing in the hook
    if (pgrLockHook(__FILE__, __LINE__, __FUNCTION__))
    {
        ULONG   flOptions;
        SWP     swpPager;

        ULONG   cb = sizeof(swpPager);
        swpPager.x = 10;
        swpPager.y = 10;
        swpPager.cx = G_pHookData->lCXScreen * 18 / 100;
        swpPager.cy = pgrCalcClientCY(swpPager.cx);

        swpPager.cx += 2 * WinQuerySysValue(HWND_DESKTOP,
                                            SV_CXSIZEBORDER);
        swpPager.cy += 2 * WinQuerySysValue(HWND_DESKTOP,
                                            SV_CYSIZEBORDER);

        PrfQueryProfileData(HINI_USER,
                            INIAPP_XWPHOOK,
                            INIKEY_HOOK_PAGERWINPOS,
                            &swpPager,
                            &cb);
                // might fail, we then use the default settings above

        WinSetWindowPos(G_pHookData->hwndPagerFrame,
                        NULLHANDLE,
                        swpPager.x,
                        swpPager.y,
                        swpPager.cx,
                        swpPager.cy,
                        SWP_MOVE | SWP_SIZE | SWP_SHOW);

        CheckFlashTimer();

        pgrUnlockHook();
    }
}

/*
 *@@ PagerButtonClick:
 *      implementation for WM_BUTTON1CLICK and WM_BUTTON2CLICK
 *      in fnwpPager.
 *
 *@@added V0.9.7 (2001-01-18) [umoeller]
 */

static MRESULT PagerButtonClick(HWND hwnd,
                                ULONG msg,
                                MPARAM mp1)
{
    POINTL  ptlMouse =
        {
            SHORT1FROMMP(mp1),
            SHORT2FROMMP(mp1)
        };

    HWND    hwndClicked = NULLHANDLE;
    PPAGERWINDATA pWinData;

    if (    (pWinData = (PPAGERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER))
         && (G_pHookData->PagerConfig.flPager & PGRFL_MINIWIN_MOUSE)
       )
    {
        if (    (hwndClicked = FindWindow(pWinData,
                                          &ptlMouse,
                                          TRUE))        // allow clicking on stickies
             && (WinIsWindowEnabled(hwndClicked))
           )
        {
            if (msg == WM_BUTTON1CLICK)
            {
                // mb1: activate window
                WinSetActiveWindow(HWND_DESKTOP, hwndClicked);

                // even though the hook should monitor
                // this change and post PGRM_ACTIVECHANGED
                // to us, this does not work if the user
                // clicked on the currently active window
                // because then there's no change...
                // so post PGRM_ACTIVECHANGED anyway
                WinPostMsg(hwnd,
                           PGRM_ACTIVECHANGED,
                           0,
                           0);

                // and refresh client right away because
                // we have a delay with activechanged
                WinPostMsg(hwnd,
                           PGRM_REFRESHCLIENT,
                           (MPARAM)FALSE,
                           0);
            }
            else
            {
                // mb2: lower window
                if (pgrLockHook(__FILE__, __LINE__, __FUNCTION__))
                {

                     WinSetWindowPos(hwndClicked,
                                     HWND_BOTTOM,
                                     0, 0, 0, 0,
                                     SWP_ZORDER);

                     WinPostMsg(hwnd,
                                PGRM_REFRESHCLIENT,
                                (MPARAM)FALSE,
                                0);

                    pgrUnlockHook();
                }
            }
        }
    }

    if (!hwndClicked)
    {
        switch (msg)
        {
            case WM_BUTTON2CLICK:
                // mouse button 2 on empty area:
                // tell XFLDR.DLL that pager context menu
                // was requested, together with desktop
                // coordinates of where the user clicked
                // so the XFLDR.DLL can display the context
                // menu.... we don't want the NLS stuff in
                // the daemon! V0.9.11 (2001-04-25) [umoeller]
                WinMapWindowPoints(G_pHookData->hwndPagerClient,
                                   HWND_DESKTOP,
                                   &ptlMouse,
                                   1);
                WinPostMsg(G_pXwpGlobalShared->hwndThread1Object,
                           T1M_PAGERCTXTMENU,
                           MPFROM2SHORT(ptlMouse.x,
                                        ptlMouse.y),
                           0);
            break;

            case WM_BUTTON1CLICK:
            {
                // set lDeltas to which desktop we want
                LONG lDeltaX, lDeltaY;
                lDeltaX =   G_pHookData->ptlCurrentDesktop.x
                          - (   ptlMouse.x
                              * G_pHookData->PagerConfig.cDesktopsX
                              / pWinData->szlClient.cx
                              * G_pHookData->szlEachDesktopFaked.cx
                            );
                lDeltaY =   G_pHookData->ptlCurrentDesktop.y
                          - (   ptlMouse.y
                              * G_pHookData->PagerConfig.cDesktopsY
                              / pWinData->szlClient.cy
                              * G_pHookData->szlEachDesktopFaked.cy
                            );
                WinPostMsg(G_pHookData->hwndPagerMoveThread,
                           PGRM_MOVEBYDELTA,
                           (MPARAM)lDeltaX,
                           (MPARAM)lDeltaY);
            }
            break;
        }
    }

    CheckFlashTimer();

    return ((MPARAM)TRUE);      // processed
}

/*
 *@@ PagerDrag:
 *
 *@@added V0.9.19 (2002-05-07) [umoeller]
 */

static VOID PagerDrag(HWND hwnd, MPARAM mp1)
{
    PPAGERWINDATA pWinData;
    if (pWinData = (PPAGERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER))
    {
        HWND        hwndTracked;
        SWP         swpTracked;

        POINTL      ptlMouse =
            {
                SHORT1FROMMP(mp1),
                SHORT2FROMMP(mp1)
            };

        // we must have the focus or the cursor and escape keys
        // won't work while dragging
        HWND            hwndOldFocus = WinQueryFocus(HWND_DESKTOP);
        WinSetFocus(HWND_DESKTOP, hwnd);

        if (G_pHookData->PagerConfig.flPager & PGRFL_FLASHTOTOP)
            WinStopTimer(G_habDaemon,
                         hwnd,
                         TIMERID_PGR_FLASH);

        if (    (!(hwndTracked = FindWindow(pWinData,
                                            &ptlMouse,
                                            FALSE)))        // do not track stickies
             || (hwndTracked == G_pHookData->hwndWPSDesktop)
           )
        {
            // user has started dragging on a non-mini window
            // (XPager client background): drag the entire
            // window then
            WinSendMsg(G_pHookData->hwndPagerFrame,
                       WM_TRACKFRAME,
                       (MPARAM)TF_MOVE,
                       0);
        }
        else
        {
            // user has started dragging a mini window: track
            // that one then
            TRACKINFO   ti;
            POINTL      ptlInitial;
            LONG        cxEach = G_pHookData->szlEachDesktopFaked.cx,
                        cyEach = G_pHookData->szlEachDesktopFaked.cy,
                        xCurrent = G_pHookData->ptlCurrentDesktop.x,
                        yCurrent = G_pHookData->ptlCurrentDesktop.y,
                        cxClient = pWinData->szlClient.cx,
                        cyClient = pWinData->szlClient.cy;
            double      dScaleX =   (double)G_pHookData->PagerConfig.cDesktopsX
                                  * cxEach
                                  / cxClient,
                        dScaleY =   (double)G_pHookData->PagerConfig.cDesktopsY
                                  * cyEach
                                  / cyClient;

            WinQueryWindowPos(hwndTracked, &swpTracked);
            ti.cxBorder = 1;
            ti.cyBorder = 1;

            ti.fs = TF_STANDARD | TF_MOVE | TF_SETPOINTERPOS | TF_PARTINBOUNDARY;

            if (WinGetKeyState(HWND_DESKTOP, VK_SHIFT) & 0x8000)
            {
                // shift pressed: set grid to each desktop
                ti.cxGrid = cxClient / G_pHookData->PagerConfig.cDesktopsX;
                ti.cyGrid = cyClient / G_pHookData->PagerConfig.cDesktopsY;
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

            // for consistency, the track rect must have the size of the mini window
            // it represents
            ti.rclTrack.xLeft   =   (swpTracked.x + xCurrent)
                                  / dScaleX;
            ti.rclTrack.yBottom =   (swpTracked.y + yCurrent)
                                  / dScaleY;
            ti.rclTrack.xRight  =   (swpTracked.x + swpTracked.cx + xCurrent)
                                  / dScaleX
                                  + 1;
            ti.rclTrack.yTop    =   (swpTracked.y + swpTracked.cy + yCurrent)
                                  / dScaleY
                                  + 1;

            ptlInitial.x = ti.rclTrack.xLeft;
            ptlInitial.y = ti.rclTrack.yBottom;

            // add a 1px boundary to the client area so that the moved
            // window remains accessible
            ti.rclBoundary.xLeft = 1;
            ti.rclBoundary.yBottom = 1;
            ti.rclBoundary.xRight = cxClient - 1;
            ti.rclBoundary.yTop = cyClient - 1;

            ti.ptlMinTrackSize.x = 2;
            ti.ptlMinTrackSize.y = 2;
            ti.ptlMaxTrackSize.x = cxClient;
            ti.ptlMaxTrackSize.y = cyClient;

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
                    ptlTracked.x =   swpTracked.x
                                   +   ((ti.rclTrack.xLeft - ptlInitial.x) / ti.cxGrid)
                                     * cxEach;
                    ptlTracked.y =   swpTracked.y
                                   +   ((ti.rclTrack.yBottom - ptlInitial.y) / ti.cyGrid)
                                     * cyEach;
                }
                else
                {
                    // if the x position hasn't changed, keep the exact x position
                    // we had before V0.9.18 (2002-02-14) [lafaix]
                    if (ti.rclTrack.xLeft != ptlInitial.x)
                        ptlTracked.x =   (ti.rclTrack.xLeft * dScaleX)
                                       - xCurrent;
                    else
                        ptlTracked.x = swpTracked.x;

                    // if the y position hasn't changed, keep the exact y position
                    // we had before V0.9.18 (2002-02-14) [lafaix]
                    if (ti.rclTrack.yBottom != ptlInitial.y)
                        ptlTracked.y =   (ti.rclTrack.yBottom * dScaleY)
                                       - yCurrent;
                    else
                        ptlTracked.y = swpTracked.y;
                }

                // disable message processing in the hook
                if (pgrLockHook(__FILE__, __LINE__, __FUNCTION__))
                {
                    WinSetWindowPos(hwndTracked,
                                    HWND_TOP,
                                    ptlTracked.x,
                                    ptlTracked.y,
                                    0,
                                    0,
                                    SWP_MOVE | SWP_NOADJUST);

                    pgrUnlockHook();
                }
            }
        }

        // restore the old focus if we had one
        // (WinSetFocus(NULLHANDLE) brings up the window list...)
        if (hwndOldFocus)
            WinSetFocus(HWND_DESKTOP, hwndOldFocus);
                // Note: this also has the side effect that if the
                // user drags the window that is currently active
                // to another desktop, we switch desktops because
                // the hook will detect that a different window
                // got activated. I think that's useful... it will
                // not happen if a non-active window gets dragged.

        WinPostMsg(hwnd,
                   PGRM_REFRESHCLIENT,
                   (MPARAM)FALSE,
                   0);
    }

    CheckFlashTimer();
}

/*
 *@@ PagerActiveChanged:
 *      implementation for PGRM_ACTIVECHANGED in
 *      fnwpPager.
 *
 *      Note that this is deferred through a PM timer
 *      and actually gets called when WM_TIMER for
 *      TIMERID_PGR_ACTIVECHANGED comes in.
 *
 *@@added V0.9.19 (2002-05-07) [umoeller]
 */

static VOID PagerActiveChanged(HWND hwnd)
{
    // we only do this if we are not currently processing
    // a pager wraparound
    if (!G_pHookData->fProcessingWraparound)
    {
        HWND        hwndActive;

        if (hwndActive = WinQueryActiveWindow(HWND_DESKTOP))
        {
            // test if this is a sticky window;
            // if so, never switch desktops
            HSWITCH hsw;
            SWP     swpActive;
            if (hsw = WinQuerySwitchHandle(hwndActive, 0))
            {
                SWCNTRL swc;
                // if (WinQuerySwitchEntry(hsw, &swc))
                if (0 == WinQuerySwitchEntry(hsw, &swc))
                        // for some reason, this returns 0 on success!!
                {
                    if (pgrIsSticky(hwndActive, swc.szSwtitle))
                        // it's sticky: get outta here
                        hwndActive = NULLHANDLE;
                }
                else
                    // no switch entry available: do not switch
                    hwndActive = NULLHANDLE;
            }

            if (    (hwndActive)
                 && (WinQueryWindowPos(hwndActive, &swpActive))
               )
            {
                // do not switch to hidden or minimized windows
                if (    (0 == (swpActive.fl & (SWP_HIDE | SWP_MINIMIZE)))
                        // only move if window is not visible
                     && (!pgrIsShowing(&swpActive))
                   )
                {
                    // calculate the absolute coordinate of the center
                    // of the active window relative to the bottom
                    // left desktop:
                    LONG    cx = G_pHookData->szlEachDesktopFaked.cx,
                            cy = G_pHookData->szlEachDesktopFaked.cy,
                            xCurrent = G_pHookData->ptlCurrentDesktop.x,
                            yCurrent = G_pHookData->ptlCurrentDesktop.y,
                            x =      (    swpActive.x
                                        + (swpActive.cx / 2)
                                        + xCurrent
                                     ) / cx
                                       * cx,
                            y =      (    swpActive.y
                                        + (swpActive.cy / 2)
                                        + yCurrent
                                     ) / cy
                                       * cy;

                    // bump boundaries
                    if (    (x >= 0)
                         && (x <= (G_pHookData->PagerConfig.cDesktopsX * cx))
                         && (y >= 0)
                         && (y <= (G_pHookData->PagerConfig.cDesktopsY * cy))
                       )
                    {
                        WinPostMsg(G_pHookData->hwndPagerMoveThread,
                                   PGRM_MOVEBYDELTA,
                                   (MPARAM)(xCurrent - x),
                                   (MPARAM)(yCurrent - y));
                    }
                }
            }
        } // end if (hwndActive)
    } // end if (!G_pHookData->fProcessingWraparound)
}

/*
 *@@ PagerHotkey:
 *
 *@@added V0.9.19 (2002-05-07) [umoeller]
 */

static VOID PagerHotkey(MPARAM mp1)
{
    LONG    lDeltaX = 0,
            lDeltaY = 0;

    switch ((ULONG)mp1)
    {
        case 0x63:                              // left
            lDeltaX = G_pHookData->szlEachDesktopFaked.cx;
        break;

        case 0x64:                              // right
            lDeltaX = -G_pHookData->szlEachDesktopFaked.cx;
        break;

        case 0x66:                              // down
            lDeltaY = G_pHookData->szlEachDesktopFaked.cy;
        break;

        case 0x61:                              // up
            lDeltaY = -G_pHookData->szlEachDesktopFaked.cy;
        break;
    }

    WinPostMsg(G_pHookData->hwndPagerMoveThread,
               PGRM_MOVEBYDELTA,
               (MPARAM)lDeltaX,
               (MPARAM)lDeltaY);
}

/*
 *@@ fnwpPager:
 *      window proc for the pager client, which shows the
 *      mini windows.
 *
 *@@added V0.9.19 (2002-05-07) [umoeller]
 */

static MRESULT EXPENTRY fnwpPager(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    TRY_LOUD(excpt1)
    {
        switch (msg)
        {
            /*
             * WM_CREATE:
             *      we receive the PAGERWINDATA as the create param.
             */

            case WM_CREATE:
                WinSetWindowPtr(hwnd, QWL_USER, mp1);

                // set offsets for current desktop
                G_pHookData->ptlCurrentDesktop.x
                    =   (G_pHookData->PagerConfig.bStartX - 1)
                      * G_pHookData->szlEachDesktopFaked.cx;
                G_pHookData->ptlCurrentDesktop.y
                    =   (G_pHookData->PagerConfig.bStartY - 1)
                      * G_pHookData->szlEachDesktopFaked.cy;
            break;

            case WM_PAINT:
                PagerPaint(hwnd);
            break;

            case WM_WINDOWPOSCHANGED:
                if (((PSWP)mp1)->fl & SWP_SIZE)
                {
                    // size changed:
                    // then the bitmaps need a resize,
                    // destroy both the template and the client bitmaps
                    PPAGERWINDATA pWinData;
                    if (pWinData = (PPAGERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER))
                    {
                        DestroyBitmaps(pWinData);

                        pWinData->szlClient.cx = ((PSWP)mp1)->cx;
                        pWinData->szlClient.cy = ((PSWP)mp1)->cy;

                        // we have CS_SIZEREDRAW, so we get repainted now
                    }
                }
            break;

            case WM_PRESPARAMCHANGED:
                PagerPresParamChanged(hwnd, mp1);
            break;

            /*
             *@@ PGRM_POSITIONFRAME:
             *      causes the pager to position itself
             *      based on the current settings.
             *      Sent on startup.
             *
             *@@added V0.9.19 (2002-05-07) [umoeller]
             */

            case PGRM_POSITIONFRAME:
                PagerPositionFrame();
            break;

            /*
             *@@ PGRM_REFRESHCLIENT:
             *      posted from various places if the
             *      pager window needs not only be repainted,
             *      but completely refreshed (i.e. we need
             *      to rebuild the client bitmap because
             *      window positions have changed etc.).
             *
             *      In order not to cause excessive rebuilds
             *      of the bitmap, we only post WM_SEM2 here.
             *      According to PMREF, WM_SEM2 is lower in
             *      priority than msgs posted by WinPostMsg
             *      and from the input queue, but still higher
             *      than WM_PAINT. As a result, if there are
             *      several window updates pending, the client
             *      window will not get rebuilt until all
             *      are processed.
             *
             *      Parameters:
             *
             *      --  BOOL mp1: if TRUE, we delete the cached
             *          background bitmap in order to have the
             *          complete display refreshed. That is
             *          expensive and only needed if color
             *          settings have changed.
             *
             *      --  mp2: not used.
             */

            case PGRM_REFRESHCLIENT:
            {
                PPAGERWINDATA pWinData;
                if (pWinData = (PPAGERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER))
                {
                    pWinData->fNeedsRefresh = TRUE;
                    WinPostMsg(hwnd,
                               WM_SEM2,
                               (MPARAM)1,       // semaphore bits
                               0);
                    if (mp1)
                        DestroyBitmaps(pWinData);
                }
            }
            break;

            case WM_SEM2:
                WinInvalidateRect(hwnd, NULL, FALSE);
            break;

            /*
             *@@ PGRM_WINDOWCHANGED:
             *      posted by ProcessMsgsForXPager in the hook
             *      whenever important window changes are detected.
             *      This applies to the msgs listed below, but
             *      not WM_ACTIVATE, for which PGRM_ACTIVECHANGED
             *      is posted instead.
             *
             *      Parameters:
             *
             *      --  HWND mp1: window for which the msg came in.
             *
             *      --  ULONG mp2: message that came in.
             *
             *      mp2 will be one of the following messages:
             *
             *      --  WM_CREATE
             *
             *      --  WM_DESTROY
             *
             *      --  WM_SETWINDOWPARAMS
             *
             *      --  WM_WINDOWPOSCHANGED
             *
             *@@added V0.9.19 (2002-05-07) [umoeller]
             */

            case PGRM_WINDOWCHANGED:
                switch ((ULONG)mp2)
                {
                    case WM_CREATE:
                        pgrCreateWinInfo((HWND)mp1);
                    break;

                    case WM_DESTROY:
                        pgrFreeWinInfo((HWND)mp1);
                    break;

                    case WM_SETWINDOWPARAMS:
                        pgrMarkDirty((HWND)mp1);
                    break;
                }

                // refresh the client bitmap
                WinPostMsg(hwnd,
                           PGRM_REFRESHCLIENT,
                           (MPARAM)FALSE,
                           0);
            break;

            /*
             *@@ PGRM_ACTIVECHANGED:
             *      posted by ProcessMsgsForXPager in the hook
             *      whenever the active window has changed.
             *
             *      Parameters:
             *
             *      --  HWND mp1: window for which WM_ACTIVATE
             *          with mp1 == TRUE came in.
             *
             *      --  mp2: always null.
             *
             *@@added V0.9.19 (2002-05-07) [umoeller]
             */

            case PGRM_ACTIVECHANGED:
                // at this point, start a timer only so we can
                // defer processing a bit to avoid flicker
                WinStartTimer(G_habDaemon,
                              hwnd,
                              TIMERID_PGR_ACTIVECHANGED,
                              200);     // 200 ms
            break;

            /*
             *@@ PGRM_REFRESHDIRTIES:
             *      refreshes all nodes on the window
             *      list that have been marked as dirty.
             *
             *      This only gets posted from RefreshPagerBmp
             *      when dirty windows were encountered so
             *      we get a chance to refresh them maybe if
             *      a window was still in the process of
             *      building up.
             *
             *      No parameters.
             *
             *@@added V0.9.19 (2002-05-07) [umoeller]
             */

            case PGRM_REFRESHDIRTIES:
                WinStartTimer(G_habDaemon,
                              hwnd,
                              TIMERID_PGR_REFRESHDIRTIES,
                              300);
            break;

            /*
             *@@ PGRM_WRAPAROUND:
             *      sent by the daemon when the mouse has hit
             *      a screen border and a wraparound was configured.
             *
             *      If a mouse switch request is pending, we must
             *      discard incoming requests, so as to prevent
             *      erratic desktop movement.  Otherwise, we post
             *      ourself a message to process the request at
             *      a later time.
             *
             *      This message must be sent, not posted.
             */

            case PGRM_WRAPAROUND:
                // avoid excessive switching, and disable
                // sliding focus too
                if (!G_pHookData->fProcessingWraparound)
                {
                    G_pHookData->fProcessingWraparound = TRUE;

                    // delay the switch
                    WinPostMsg(hwnd,
                               PGRM_PAGERHOTKEY,
                               mp1,
                               (MPARAM)TRUE);
                }
            break;

            /*
             *@@ PGRM_PAGERHOTKEY:
             *      posted by hookPreAccelHook when
             *      one of the arrow hotkeys was
             *      detected.
             *
             *      (UCHAR)mp1 has the scan code of the
             *      key which was pressed.
             *
             *      (BOOL)mp2 is true if this has been
             *      posted due to a mouse wraparound request.
             *
             *@@changed V0.9.9 (2001-03-14) [lafaix]: mp2 defined.
             */

            case PGRM_PAGERHOTKEY:
                PagerHotkey(mp1);
            break;

            case WM_TIMER:
            {
                BOOL fStop = TRUE;
                switch ((USHORT)mp1)
                {
                    case TIMERID_PGR_ACTIVECHANGED:
                        PagerActiveChanged(hwnd);
                    break;

                    case TIMERID_PGR_FLASH:
                        WinShowWindow(G_pHookData->hwndPagerFrame, FALSE);
                    break;

                    case TIMERID_PGR_REFRESHDIRTIES:
                        if (pgrRefreshDirties())
                            // refresh the client bitmap
                            WinPostMsg(hwnd,
                                       PGRM_REFRESHCLIENT,
                                       (MPARAM)FALSE,
                                       0);
                    break;

                    default:
                        fStop = FALSE;
                        mrc = WinDefWindowProc(hwnd, msg, mp1, mp2);
                }

                if (fStop)
                    WinStopTimer(G_habDaemon,
                                 hwnd,
                                 (USHORT)mp1);
            }
            break;

            /*
             * WM_BUTTON1CLICK:
             *      activate a specific window.
             */

            case WM_BUTTON1CLICK:
            case WM_BUTTON2CLICK:
                mrc = PagerButtonClick(hwnd, msg, mp1);
            break;

            /*
             * WM_BUTTON2MOTIONSTART:
             *      move the windows within the pager.
             */

            // case WM_BUTTON2MOTIONSTART:
            case WM_BEGINDRAG:
                PagerDrag(hwnd, mp1);
            break;

            default:
                mrc = WinDefWindowProc(hwnd, msg, mp1, mp2);
            break;
        }
    }
    CATCH(excpt1)
    {
        mrc = 0;
    } END_CATCH();

    return mrc;
}

/* ******************************************************************
 *
 *   Pager frame
 *
 ********************************************************************/

/*
 *@@ fnwpSubclPagerFrame:
 *      window proc for the subclassed pager frame to
 *      hack ourselves into the resizing and stuff.
 *
 *@@added V0.9.19 (2002-05-07) [umoeller]
 */

static MRESULT EXPENTRY fnwpSubclPagerFrame(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT     mrc = 0;

    PPAGERWINDATA pWinData;

    switch (msg)
    {
        case WM_ADJUSTWINDOWPOS:
            if (pWinData = (PPAGERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER))
            {
                PSWP pswp = (PSWP)mp1;

                if (pswp->cx < 20)
                    pswp->cx = 20;
                if (pswp->cy < 20)
                    pswp->cy = 20;

                if (G_pHookData->PagerConfig.flPager & PGRFL_PRESERVEPROPS)
                {
                    LONG cx =   pswp->cx
                              - 2 * WinQuerySysValue(HWND_DESKTOP,
                                                     SV_CXSIZEBORDER);
                    pswp->cy =   pgrCalcClientCY(cx)
                               + 2 * WinQuerySysValue(HWND_DESKTOP,
                                                      SV_CYSIZEBORDER);
                }

                // we never want to become active:
                if (pswp->fl & SWP_ACTIVATE)
                {
                    pswp->fl &= ~SWP_ACTIVATE;
                    pswp->fl |= SWP_DEACTIVATE;
                }

                mrc = pWinData->pfnwpOrigFrame(hwnd, msg, mp1, mp2);
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
            if (pWinData = (PPAGERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER))
            {
                // stop flash timer, if running
                if (G_pHookData->PagerConfig.flPager & PGRFL_FLASHTOTOP)
                    WinStopTimer(G_habDaemon,
                                 G_pHookData->hwndPagerClient,
                                 TIMERID_PGR_FLASH);

                // now track window (WC_FRAME runs a modal message loop)
                mrc = pWinData->pfnwpOrigFrame(hwnd, msg, mp1, mp2);

                CheckFlashTimer();
            }
        break;

        /*
         * WM_QUERYTRACKINFO:
         *      the frame control generates this when
         *      WM_TRACKFRAME comes in.
         */

        case WM_QUERYTRACKINFO:
            if (pWinData = (PPAGERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER))
            {
                PTRACKINFO  ptr;
                pWinData->pfnwpOrigFrame(hwnd, msg, mp1, mp2);
                ptr = (PTRACKINFO) PVOIDFROMMP(mp2);
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
            if (pWinData = (PPAGERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER))
            {
                if (((PSWP)mp1)->fl & (SWP_SIZE | SWP_MOVE))
                    PrfWriteProfileData(HINI_USER,
                                        INIAPP_XWPHOOK,
                                        INIKEY_HOOK_PAGERWINPOS,
                                        (PSWP)mp1,
                                        sizeof(SWP));
                if (((PSWP)mp1)->fl & SWP_SHOW)
                    CheckFlashTimer();

                mrc = pWinData->pfnwpOrigFrame(hwnd, msg, mp1, mp2);
            }
        break;

        /*
         * WM_DESTROY:
         *      children receive this before the frame,
         *      so free the win data here and not in
         *      the client.
         */

        case WM_DESTROY:
            if (pWinData = (PPAGERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER))
            {
                mrc = pWinData->pfnwpOrigFrame(hwnd, msg, mp1, mp2);
                DestroyBitmaps(pWinData);
                G_pHookData->hwndPagerFrame = NULLHANDLE;
                G_pHookData->hwndPagerClient = NULLHANDLE;
                free(pWinData);
            }
        break;

        default:
            if (pWinData = (PPAGERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER))
                mrc = pWinData->pfnwpOrigFrame(hwnd, msg, mp1, mp2);
    }

    return mrc;
}

/* ******************************************************************
 *
 *   Pager creation
 *
 ********************************************************************/

/*
 *@@ pgrCreatePager:
 *      creates the XPager control window (frame
 *      and client). This gets called by dmnStartXPager
 *      on thread 1 when XPager has been enabled.
 *
 *      This registers the XPager client class
 *      using fnwpPager, which does most of
 *      the work for XPager, and subclasses the
 *      frame with fnwpSubclPagerFrame.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 *@@changed V0.9.3 (2000-05-22) [umoeller]: the hook was interfering, fixed
 */

BOOL pgrCreatePager(VOID)
{
    PPAGERWINDATA pWinData;
    BOOL    brc = TRUE;

    if (G_pHookData->hwndPagerFrame)
        return TRUE;

    WinRegisterClass(G_habDaemon,
                     (PSZ)WC_PAGER,
                     (PFNWP)fnwpPager,
                     CS_SIZEREDRAW,
                     sizeof(PVOID));

    // allocate pager win data
    if (pWinData = NEW(PAGERWINDATA))
    {
        ZERO(pWinData);
        pWinData->cb = sizeof(PAGERWINDATA);

        // disable message processing in the hook
        if (pgrLockHook(__FILE__, __LINE__, __FUNCTION__))
        {
            FRAMECDATA  fcdata;
            HWND        hwndFrame;
            RECTL       rclClient;

            fcdata.cb            = sizeof(FRAMECDATA);
            fcdata.flCreateFlags = FCF_NOBYTEALIGN | FCF_SIZEBORDER;
            fcdata.hmodResources = (HMODULE)NULL;
            fcdata.idResources   = 0;

            // create frame and client
            if (    (G_pHookData->hwndPagerFrame = WinCreateWindow(HWND_DESKTOP,
                                                                   WC_FRAME,
                                                                   "Pager",
                                                                   0,
                                                                   0, 0, 0, 0,
                                                                   NULLHANDLE,       // owner
                                                                   HWND_TOP,
                                                                   0,                // id
                                                                   &fcdata,         // frame class data
                                                                   NULL))           // no presparams
                 && (G_pHookData->hwndPagerClient = WinCreateWindow(G_pHookData->hwndPagerFrame,
                                                                    (PSZ)WC_PAGER,
                                                                    "",
                                                                    WS_VISIBLE,
                                                                    0, 0, 0, 0,
                                                                    G_pHookData->hwndPagerFrame, // owner
                                                                    HWND_TOP,
                                                                    FID_CLIENT,
                                                                    pWinData,
                                                                    NULL))
               )
            {
                // give frame access to win data too
                WinSetWindowPtr(G_pHookData->hwndPagerFrame,
                                QWL_USER,
                                pWinData);

                // set frame icon
                WinSendMsg(G_pHookData->hwndPagerFrame,
                           WM_SETICON,
                           (MPARAM)G_hptrDaemon,
                           NULL);

                // set font (client catches this)
                #define DEFAULTFONT "2.System VIO"
                WinSetPresParam(G_pHookData->hwndPagerClient,
                                PP_FONTNAMESIZE,
                                sizeof(DEFAULTFONT),
                                DEFAULTFONT);

                pgrBuildWinlist();

                thrCreate(&G_tiMoveThread,
                          fntMoveThread,
                          NULL, // running flag
                          "PgmgMove",
                          THRF_WAIT | THRF_PMMSGQUEUE,    // PM msgq
                          0);
                    // this creates the XPager object window

                // subclass frame
                pWinData->pfnwpOrigFrame = WinSubclassWindow(G_pHookData->hwndPagerFrame,
                                                             fnwpSubclPagerFrame);

                // have pgr position itself
                WinSendMsg(G_pHookData->hwndPagerClient,
                           PGRM_POSITIONFRAME,
                           0,
                           0);

                brc = TRUE;
            }

            pgrUnlockHook();
        }
    }

    return (brc);
}


