
/*
 *@@ pgmg_move.c:
 *      PageMage "Move" thread.
 *
 *      See pgmg_control.c for an introduction to PageMage.
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

#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#define INCL_DOSEXCEPTIONS
#define INCL_DOSERRORS

#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WINMESSAGEMGR
#define INCL_WINSWITCHLIST
#define INCL_WININPUT
#define INCL_WINPOINTERS
#define INCL_WINSYS
#define INCL_WINERRORS
#include <os2.h>

#include <stdio.h>
#include <setjmp.h>

#define DONT_REPLACE_MALLOC         // in case mem debug is enabled
#include "setup.h"                      // code generation and debugging options

#include "helpers\except.h"
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\threads.h"

#include "xwpapi.h"                     // public XWorkplace definitions

#include "shared\kernel.h"              // XWorkplace Kernel

#include "hook\xwphook.h"
#include "hook\hook_private.h"
#include "hook\xwpdaemn.h"              // PageMage and daemon declarations

#pragma hdrstop

#define WC_MOVETHREAD      "XWPPgmgMoveObj"

/*
 *@@ pgmmIsPartlyOnCurrentDesktop:
 *      returns TRUE if the specified window is at least
 *      partly visible on the current desktp.
 *
 *      Based on a code snipped by Dmitry Kubov.
 *
 *@@added V0.9.7 (2001-01-21) [umoeller]
 */

BOOL pgmmIsPartlyOnCurrentDesktop(PSWP pswp)
{
    // this was rewritten V0.9.7 (2001-01-18) [umoeller]
    LONG        bx = WinQuerySysValue(HWND_DESKTOP, SV_CXSIZEBORDER);
    LONG        by = WinQuerySysValue(HWND_DESKTOP, SV_CYSIZEBORDER);

    return (     !(    // is right edge too wide to the left?
                       ((pswp->x + bx) >= G_szlEachDesktopReal.cx)
                    || ((pswp->x + pswp->cx - bx) <= 0)
                    || ((pswp->y + by) >= G_szlEachDesktopReal.cy)
                    || ((pswp->y + pswp->cy - by) <= 0)
                  )
            );
}

/*
 *@@ pgmmMoveIt:
 *      moves all windows around the desktop according to the
 *      given X and Y deltas. Runs on the Move thread.
 *
 *      Parameters:
 *
 *      --  Positive lXDelta:  at left edge of screen, panning left, window shift +
 *
 *      --  Negative lXDelta:  at right edge of screen, panning right, window shift -
 *
 *      --  Negative lYDelta:  at top edge of screen, panning up, window shift -
 *
 *      --  Positive lYDelta:  at bottom edge of screen, panning down, window shift +
 *
 *      Returns:
 *          TRUE:  moved successfully
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 *@@changed V0.9.3 (2000-04-10) [umoeller]: fixed excessive window repainting
 *@@changed V0.9.4 (2000-08-08) [umoeller]: added special maximized window handling
 */

BOOL pgmmMoveIt(HAB hab,
                LONG lXDelta,
                LONG lYDelta,
                BOOL bAllowUpdate)
{
    BOOL    fAnythingMoved = TRUE; //[dk] allow desktop switching if no windows to move

    PSWP    paswpNew = NULL;
    ULONG   cSwpNewUsed = 0;

    HWND*   pahwndHacked = NULL;
    ULONG   chwndHacked = 0;

    // _Pmpf((__FUNCTION__": entering"));

    if (!bAllowUpdate)
        WinEnableWindowUpdate(G_pHookData->hwndPageMageClient, FALSE);

    if (pgmwLock())
    {
        // allocate an array of SWP entries for moving all windows
        // at once... we'll allocate one SWP entry for each item
        // on the wininfo list, but we'll probably not use them
        // all. cSwpNewUsed will be incremented for each item that's
        // actually used
        ULONG       cWinInfos,
                    cbSwpNew,
                    cbMoveP;

        if (    (cWinInfos = lstCountItems(&G_llWinInfos))
             && (cbSwpNew = cWinInfos * sizeof(SWP))
             && (cbMoveP  = cWinInfos * sizeof(HWND))
             // allocate array of SWPs
             && (paswpNew = (PSWP)malloc(cbSwpNew))
             // allocate array of HWNDs for which we
             // might disable FS_NOMOVEWITHOWNER
             && (pahwndHacked  = (HWND*)malloc(cbMoveP))
           )
        {
            PLISTNODE    pNode;

            memset(paswpNew, 0, cbSwpNew);

            // now go thru all windows on the main list and copy them
            // to the move list, if necessary
            pNode = lstQueryFirstNode(&G_llWinInfos);
            while (pNode)
            {
                PLISTNODE pNext = pNode->pNext; // V0.9.15 (2001-09-14) [umoeller]

                PPGMGWININFO pEntryThis = (PPGMGWININFO)pNode->pItemData;
                // update window pos in winlist's SWP
                PSWP pswpThis = &pEntryThis->swp;

                if (!WinIsWindow(hab, pEntryThis->hwnd))
                    // window no longer valid:
                    // remove from the list NOW
                    // V0.9.15 (2001-09-14) [umoeller]
                    lstRemoveNode(&G_llWinInfos, pNode);
                            // previously, this just marked the window
                            // for rescan, which obviously didn't work
                            // very well...
                else
                {
                    BOOL fMoveThis = FALSE;
                    BOOL fRescanThis = FALSE;

                    if (     (!strcmp(pEntryThis->szClassName, "#1")
                          && (!(WinQueryWindowULong(pEntryThis->hwnd, QWL_STYLE)
                                        & FS_NOMOVEWITHOWNER))
                             )
                       )
                    {
                        pahwndHacked[chwndHacked++] = pEntryThis->hwnd;
                        WinSetWindowBits(pEntryThis->hwnd, QWL_STYLE,
                                         FS_NOMOVEWITHOWNER,
                                         FS_NOMOVEWITHOWNER);
                    }

                    WinQueryWindowPos(pEntryThis->hwnd, pswpThis);

                    // fix outdated minimize/maximize/hide flags
                    if (    (pEntryThis->bWindowType == WINDOW_MINIMIZE)
                         && ( (pswpThis->fl & SWP_MINIMIZE) == 0)
                       )
                        // no longer minimized:
                        fRescanThis = TRUE;
                    else if (    (pEntryThis->bWindowType == WINDOW_MAXIMIZE)
                              && ( (pswpThis->fl & SWP_MAXIMIZE) == 0)
                            )
                            // no longer minimized:
                            fRescanThis = TRUE;

                    if (pEntryThis->bWindowType == WINDOW_NORMAL)
                    {
                        if (pswpThis->fl & SWP_HIDE)
                            fRescanThis = TRUE;
                        else if (pswpThis->fl & SWP_MINIMIZE)
                            // now minimized:
                            pEntryThis->bWindowType = WINDOW_MINIMIZE;
                        else if (pswpThis->fl & SWP_MAXIMIZE)
                            // now maximized:
                            pEntryThis->bWindowType = WINDOW_MAXIMIZE;
                    }

                    if (fRescanThis)
                    {
                        if (pgmwFillWinInfo(pEntryThis->hwnd,
                                            pEntryThis))
                        {
                            // window still valid:
                            fMoveThis = TRUE;
                        }
                        else
                            // window no longer valid:
                            // remove from the list NOW
                            // V0.9.15 (2001-09-14) [umoeller]
                            lstRemoveNode(&G_llWinInfos, pNode);
                    }
                    else
                        if (    (pEntryThis->bWindowType == WINDOW_MAXIMIZE)
                             || (    (pEntryThis->bWindowType == WINDOW_NORMAL)
                                  && (!(pswpThis->fl & SWP_HIDE))
                                )
                           )
                            fMoveThis = TRUE;

                    if (fMoveThis)
                    {
                        // OK, window to be moved:

                        // default flags
                        ULONG   fl = SWP_MOVE | SWP_NOADJUST;
                                // SWP_NOADJUST is required or the windows
                                // will end up in the middle of nowhere

                        PSWP    pswpNewThis = &paswpNew[cSwpNewUsed];

                        WinQueryWindowPos(pEntryThis->hwnd,
                                          pswpNewThis);

                        pswpNewThis->hwnd = pEntryThis->hwnd;
                        // add the delta for moving
                        pswpNewThis->x += lXDelta;
                        pswpNewThis->y += lYDelta;

                        pswpNewThis->fl = fl;

                        // use next entry in SWP array
                        cSwpNewUsed++;

                    } // end if (bResult)
                }

                pNode = pNext;      // V0.9.15 (2001-09-14) [umoeller]
            } // end while (pNode)
        } // end if (    (cbSwpnew)
          //          && (paswpNew = (PSWP)malloc(cbSwpNew)))

        // _Pmpf(("    Moving %d windows", cSwpNewUsed));

        if (paswpNew)
        {
            if (cSwpNewUsed)
            {
                // if window animations are enabled, turn them off for now.
                        // V0.9.7 (2001-01-18) [umoeller]
                /* BOOL fAnimation = FALSE;
                if (WinQuerySysValue(HWND_DESKTOP, SV_ANIMATION))
                {
                    fAnimation = TRUE;
                    WinSetSysValue(HWND_DESKTOP, SV_ANIMATION, FALSE);
                } */

                // disable message processing in the hook
                if (pgmcDisableSwitching())
                {
                    // now set all windows at once, this saves a lot of
                    // repainting...
                    if (!(fAnythingMoved = WinSetMultWindowPos(hab,
                                                               (PSWP)paswpNew,
                                                               cSwpNewUsed)))
                    {
                        // this FAILED:
                        // V0.9.15 (2001-09-14) [umoeller]

                        #ifdef __DEBUG__

                        ULONG ul;
                        // _Pmpf(("WinSetMultWindowPos failed, error %lX!",
                           //     WinGetLastError(hab)));

                        // this gives us 0x81001, meaning:
                        // severity: 0x8 = error
                        // 0x1001 == invalid window handle specified...

                        DosBeep(350, 500);

                        for (ul = 0;
                             ul < cSwpNewUsed;
                             ul++)
                        {
                            PSWP pswpThis = &paswpNew[ul];
                            HWND hwndClient;
                            CHAR szClass[100] = "?",
                                 szClient[100] = "none",
                                 szCoords[100];

                            WinQueryClassName(pswpThis->hwnd,
                                              sizeof(szClass),
                                              szClass);
                            if (hwndClient = WinWindowFromID(pswpThis->hwnd,
                                                             FID_CLIENT))
                                WinQueryClassName(hwndClient,
                                                  sizeof(szClient),
                                                  szClient);
                            if (pswpThis->fl & SWP_HIDE)
                                strcpy(szCoords, "hidden");
                            else
                                sprintf(szCoords,
                                        "%d, %d, %d, %d",
                                        pswpThis->x,
                                        pswpThis->y,
                                        pswpThis->cx,
                                        pswpThis->cy);

                            /* _Pmpf(("  hwnd[%d] is 0x%lX (%s, %s) class \"%s\", client \"%s\"",
                                   ul,
                                   pswpThis->hwnd,
                                   WinIsWindow(hab, pswpThis->hwnd)
                                        ? "valid" : "!! invalid",
                                   szCoords,
                                   szClass,
                                   szClient)); */
                        }

                        #endif // V0.9.15 (2001-09-14) [umoeller]
                    }

                    pgmcReenableSwitching();
                }

                /* if (fAnimation)
                    // turn animation back on
                    WinSetSysValue(HWND_DESKTOP, SV_ANIMATION, TRUE); */
            }

            // clean up SWP array
            free(paswpNew);
        } // if (paswpNew)

        // unset FS_NOMOVEWITHOWNER for the windows where we set it above
        while (chwndHacked)
            WinSetWindowBits(pahwndHacked[--chwndHacked],
                             QWL_STYLE,
                             0,
                             FS_NOMOVEWITHOWNER);

        pgmwUnlock();
    }

    if (pahwndHacked)
        free(pahwndHacked);

    if (!bAllowUpdate)
        WinEnableWindowUpdate(G_pHookData->hwndPageMageClient, TRUE);

    // _Pmpf((__FUNCTION__": leaving"));

    return (fAnythingMoved);
}

/*
 *@@ pgmmZMoveIt:
 *      moves windows around the screen. This gets called by
 *      fnwpMoveThread if we have determined that we need to
 *      switch to another desktop.
 *
 *      Parameters:
 *          Positive lXDelta:  at left edge of screen, panning left, window shift +
 *          Negative lXDelta:  at right edge of screen, panning right, window shift -
 *          Negative lYDelta:  at top edge of screen, panning up, window shift -
 *          Positive lYDelta:  at bottom edge of screen, panning down, window shift +
 *      Returns:
 *          TRUE:    move successful
 *          FALSE:   move not successful
 *
 *      This calls pgmmMoveIt in turn.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 *@@changed V0.9.7 (2001-01-18) [umoeller]: added window rescan before switching; this fixes lost windows
 */

BOOL pgmmZMoveIt(HAB hab,
                 LONG lXDelta,
                 LONG lYDelta)
{
    BOOL        bReturn = FALSE;

    // shortcuts to global pagemage config
    PPAGEMAGECONFIG pPageMageConfig = &G_pHookData->PageMageConfig;
    PPOINTL         pptlMaxDesktops = &pPageMageConfig->ptlMaxDesktops;

    LONG        lRightEdge = pptlMaxDesktops->x * G_szlEachDesktopReal.cx;
    LONG        lBottomEdge = pptlMaxDesktops->y * G_szlEachDesktopReal.cy;
    LONG        lNewPosX = G_ptlCurrPos.x - lXDelta;
    LONG        lNewPosY = G_ptlCurrPos.y + lYDelta;

    // _Pmpf((__FUNCTION__ ": 1) ldX = %d, ldY = %d", lXDelta, lYDelta));

    // handle X delta
    if (lXDelta)
    {
        if (lNewPosX < 0)
        {
            if (G_ptlCurrPos.x == 0)
            {
                if (pPageMageConfig->bWrapAround)
                    lXDelta = - (LONG)((pptlMaxDesktops->x - 1) * G_szlEachDesktopReal.cx);
                else
                    lXDelta = 0;
            }
            else
                lXDelta = G_ptlCurrPos.x;
        }
        else
        {
            if ((lNewPosX + G_szlEachDesktopReal.cx) >= lRightEdge)
            {
                if ((G_ptlCurrPos.x + G_szlEachDesktopReal.cx) == lRightEdge)
                {
                    if (pPageMageConfig->bWrapAround)
                        lXDelta = (pptlMaxDesktops->x - 1) * G_szlEachDesktopReal.cx;
                    else
                        lXDelta = 0;
                }
                else
                    lXDelta = G_ptlCurrPos.x - lRightEdge + G_szlEachDesktopReal.cx;
            }
        }
    }

    // handle Y delta
    if (lYDelta)
    {
        if (lNewPosY < 0)
        {
            if (G_ptlCurrPos.y == 0)
            {
                if (pPageMageConfig->bWrapAround)
                    lYDelta = (pptlMaxDesktops->y - 1) * G_szlEachDesktopReal.cy;
                else
                    lYDelta = 0;
            }
            else
                lYDelta = -G_ptlCurrPos.y;
        }
        else
        {
            if ((lNewPosY + G_szlEachDesktopReal.cy) >= lBottomEdge)
            {
                if ((G_ptlCurrPos.y + G_szlEachDesktopReal.cy) == lBottomEdge)
                {
                    if (pPageMageConfig->bWrapAround)
                        lYDelta = - (LONG)((pptlMaxDesktops->y - 1) * G_szlEachDesktopReal.cy);
                    else
                        lYDelta = 0;
                }
                else
                    lYDelta = lBottomEdge - G_szlEachDesktopReal.cy - G_ptlCurrPos.y;
            }
        }
    }

    // _Pmpf((__FUNCTION__ ": 1) ldX = %d, ldY = %d", lXDelta, lYDelta));

    if (lXDelta || lYDelta)
    {
        // rescan all windows which might still have the "rescan" flag
        // set... if we don't do this, we get sticky windows that aren't
        // supposed to be sticky
                // V0.9.7 (2001-01-18) [umoeller]
        pgmwWindowListRescan();

        if (bReturn = pgmmMoveIt(hab, lXDelta, lYDelta, FALSE))
        {
            G_ptlCurrPos.x -= lXDelta;
            G_ptlCurrPos.y += lYDelta;
            WinPostMsg(G_pHookData->hwndPageMageClient,
                       PGMG_INVALIDATECLIENT,
                       (MPARAM)TRUE,        // immediately
                       MPVOID);
        }
    }

    return (bReturn);
}

/*
 *@@ fnwpMoveThread:
 *      window procedure for the object window on the
 *      PageMage "Move" thread (fntMoveThread).
 *      This receives PGOM_* messages and processes them.
 *      The object window handle is available in
 *      G_pHookData->hwndPageMageMoveThread.
 *
 *      This has been rewritten completely with V0.9.4
 *      in order to get rid of the old DosReadQueue
 *      stuff which apparently wasn't working. I've tried
 *      using an object window instead, and it seems to
 *      be doing OK.
 *
 *@@added V0.9.4 (2000-08-03) [umoeller]
 *@@changed V0.9.7 (2000-12-04) [umoeller]: now preventing sticky windows from switching
 *@@changed V0.9.9 (2001-03-14) [lafaix]: now preventing erratic mouse screen switch
 *@@changed V0.9.18 (2002-02-19) [lafaix]: now using G_szlPageMageClient
 */

MRESULT EXPENTRY fnwpMoveThread(HWND hwndObject, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT     mrc = 0;
    LONG        lDeltaX = 0,
                lDeltaY = 0;

    TRY_LOUD(excpt1)
    {
        // shortcuts to global pagemage config
        PPAGEMAGECONFIG pPageMageConfig = &G_pHookData->PageMageConfig;
        PPOINTL         pptlMaxDesktops = &pPageMageConfig->ptlMaxDesktops;

        // _Pmpf((__FUNCTION__ ": msg %d", msg));

        switch (msg)
        {
            /*
             *@@ PGOM_CLICK2ACTIVATE:
             *      written into the queue when
             *      WM_BUTTON1CLICK was received
             *      by fnwpPageMageClient.
             *
             *      Parameters:
             *      -- LONG mp1: mouse x.
             *      -- LONG mp2: mouse y.
             */

            case PGOM_CLICK2ACTIVATE:      // mouse button 1
            case PGOM_CLICK2LOWER:         // mouse button 2
            {
                POINTL ptlMouse = {(LONG)mp1,
                                   (LONG)mp2};

                // _Pmpf(("  PGOM_CLICK2ACTIVATE/PGOM_CLICK2LOWER"));

                // calc new desktop if mb1 was clicked (always)
                if (msg == PGOM_CLICK2ACTIVATE)
                {
                    // set lDeltas to which desktop we want
                    lDeltaX = ptlMouse.x * pptlMaxDesktops->x / G_szlPageMageClient.cx;
                    lDeltaY = pptlMaxDesktops->y - 1 - ptlMouse.y
                              * pptlMaxDesktops->y / G_szlPageMageClient.cy;
                    lDeltaX *= G_szlEachDesktopReal.cx;
                    lDeltaY *= G_szlEachDesktopReal.cy;
                    lDeltaX = G_ptlCurrPos.x - lDeltaX;
                    lDeltaY -= G_ptlCurrPos.y;
                }

                if (pPageMageConfig->fClick2Activate)
                {
                    HWND hwndActive;
                    if (hwndActive = pgmwGetWindowFromClientPoint(ptlMouse.x,
                                                                  ptlMouse.y))
                    {
                        // we must have a msg queue to use WinSetActiveWindow(),
                        // so post this back
                        if (msg == PGOM_CLICK2ACTIVATE)
                            // "raise" request:
                            WinPostMsg(G_pHookData->hwndPageMageClient,
                                       PGMG_CHANGEACTIVE,
                                       MPFROMHWND(hwndActive),
                                       MPVOID);
                        else
                            // "lower" request
                            WinPostMsg(G_pHookData->hwndPageMageClient,
                                       PGMG_LOWERWINDOW,
                                       MPFROMHWND(hwndActive),
                                       MPVOID);
                    }
                    else
                        // click on pager background (no mini-window):
                        if (msg == PGOM_CLICK2LOWER)
                        {
                            // mouse button 2:
                            // tell XFLDR.DLL that pager context menu
                            // was requested, together with desktop
                            // coordinates of where the user clicked
                            // so the XFLDR.DLL can display the context
                            // menu.... we don't want the NLS stuff in
                            // the daemon! V0.9.11 (2001-04-25) [umoeller]
                            WinMapWindowPoints(G_pHookData->hwndPageMageClient,
                                               HWND_DESKTOP,
                                               &ptlMouse,
                                               1);
                            WinPostMsg(G_pXwpGlobalShared->hwndThread1Object,
                                       T1M_PAGEMAGECTXTMENU,
                                       MPFROM2SHORT(ptlMouse.x,
                                                    ptlMouse.y),
                                       0);
                        }
                }
            }
            break;

            /*
             *@@ PGOM_MOUSESWITCH:
             *      sent by the daemon when a mouse switch is
             *      requested.
             *
             *      If a mouse switch request is pending, we must
             *      discard incoming requests, so as to prevent
             *      erratic desktop movement.  Otherwise, we post
             *      ourself a message to process the request at
             *      a later time.
             *
             *      This message must be send, not posted.
             *
             *@@added V0.9.9 (2001-03-14) [lafaix]
             */

            case PGOM_MOUSESWITCH:
                // _Pmpf(("  PGOM_MOUSESWITCH"));
                if (G_pHookData->fDisableMouseSwitch == FALSE)
                {
                    // no mouse switch pending
                    G_pHookData->fDisableMouseSwitch = TRUE;

                    // delay the switch
                    WinPostMsg(hwndObject, PGOM_HOOKKEY, mp1, (MPARAM)TRUE);
                }
            break;

            /*
             *@@ PGOM_HOOKKEY:
             *      posted by hookPreAccelHook when
             *      one of the arrow hotkeys was
             *      detected.
             *
             *      (UCHAR)mp1 has the scan code of the
             *      key which was pressed.
             *
             *      (BOOL)mp2 is true if this has been
             *      posted due to a mouse switch request.
             *
             *@@changed V0.9.9 (2001-03-14) [lafaix]: mp2 defined.
             */

            case PGOM_HOOKKEY:
                // _Pmpf(("  PGOM_HOOKKEY"));
                switch ((ULONG)mp1)
                {
                    case 0x63:                              // left
                        lDeltaX = G_szlEachDesktopReal.cx;
                        break;
                    case 0x64:                              // right
                        lDeltaX = -G_szlEachDesktopReal.cx;
                        break;
                    case 0x61:                              // up
                        lDeltaY = -G_szlEachDesktopReal.cy;
                        break;
                    case 0x66:                              // down
                        lDeltaY = G_szlEachDesktopReal.cy;
                        break;
                }
            break;

            /*
             *@@ PGOM_FOCUSCHANGE:
             *      posted by ProcessMsgsForPageMage in
             *      the hook when the focus or active
             *      window changes. We then check whether
             *      we need to switch desktops and whether
             *      the PageMage client needs to be repainted.
             *
             *@@changed V0.9.7 (2001-01-19) [umoeller]: switch list entry checks never worked at all, fixed.
             */

            case PGOM_FOCUSCHANGE:
                // _Pmpf(("  PGOM_FOCUSCHANGE"));
                if (!G_pHookData->fDisableMouseSwitch)
                {
                    // we only do this if we are not currently processing
                    // a mouse switch
                    // V0.9.9 (2001-03-14) [lafaix]
                    SWP         swpActive;
                    // get the new active window
                    HWND hwndActive;

                    DosSleep(100);

                    // _Pmpf((__FUNCTION__ ": PGOM_FOCUSCHANGE, hwndActive: 0x%lX", hwndActive));

                    if (hwndActive = WinQueryActiveWindow(HWND_DESKTOP))
                    {
                        // test if this is a sticky window;
                        // if so, never switch desktops
                        // V0.9.7 (2000-12-04) [umoeller]
                        HSWITCH hsw;
                        if (hsw = WinQuerySwitchHandle(hwndActive, 0))
                        {
                            SWCNTRL swc;
                            // if (WinQuerySwitchEntry(hsw, &swc))
                            if (0 == WinQuerySwitchEntry(hsw, &swc))
                                    // fixed V0.9.7 (2001-01-19) [umoeller]...
                                    // for some reason, this returns 0 on success!!
                            {
                                if (pgmwIsSticky(hwndActive, swc.szSwtitle))
                                    // it's sticky: get outta here
                                    break;
                            }
                            else
                                // no switch entry available: do not switch
                                // V0.9.7 (2001-01-19) [umoeller]
                                break;
                        }

                        // check if the active window is valid
                        WinQueryWindowPos(hwndActive, &swpActive);

                        // do not switch to hidden or minimized windows
                        if (    (0 == (swpActive.fl & (SWP_HIDE | SWP_MINIMIZE)))
                                // only move if window is not visible
                             && (!pgmmIsPartlyOnCurrentDesktop(&swpActive))
                           )
                        {
                            // calculate the absolute coordinate (top left is (0,0))
                            // of the active window relative to all desktops:
                            LONG lAbsX = swpActive.x + (swpActive.cx / 2);
                            LONG lAbsY = swpActive.y + (swpActive.cy / 2);
                            lAbsX += G_ptlCurrPos.x;
                            lAbsY = G_ptlCurrPos.y + G_szlEachDesktopReal.cy - lAbsY;

                            // if we intend to move into a valid window
                            if (    (lAbsX >= 0)
                                 && (lAbsX <= (pptlMaxDesktops->x
                                               * G_szlEachDesktopReal.cx))
                                 && (lAbsY >= 0)
                                 && (lAbsY <= (pptlMaxDesktops->y
                                               * G_szlEachDesktopReal.cy))
                               )
                            {
                                // put abs coord of desktop in lAbs
                                lAbsX /= G_szlEachDesktopReal.cx;
                                lAbsY /= G_szlEachDesktopReal.cy;
                                lAbsX *= G_szlEachDesktopReal.cx;
                                lAbsY *= G_szlEachDesktopReal.cy;

                                lDeltaX = G_ptlCurrPos.x - lAbsX;
                                lDeltaY = lAbsY - G_ptlCurrPos.y;
                            }
                        }
                    } // end if (hwndActive)
                } // end if (!G_pHookData->fDisableMouseSwitch)
            break;

            default:
                mrc = WinDefWindowProc(hwndObject, msg, mp1, mp2);
        } // end switch (msg)

        if (    (lDeltaX)
             || (lDeltaY)
           )
        {
            // _Pmpf(("  ldX = %d, ldY = %d", lDeltaX, lDeltaY));
            if (    (!G_pHookData->fDisableMouseSwitch)
                 || (    (msg == PGOM_HOOKKEY)
                      && (mp2 == (MPARAM)TRUE)
                    )
               )
            {
                // _Pmpf(("  calling pgmmZMoveIt"));
                // we got something to move:
                // move windows around to switch Desktops
                if (pgmmZMoveIt(WinQueryAnchorBlock(hwndObject),
                                lDeltaX,
                                lDeltaY))
                {
                    // success: flashing enabled?
                    if (pPageMageConfig->fFlash)
                    {
                        WinSetWindowPos(G_pHookData->hwndPageMageFrame,
                                        HWND_TOP,
                                        0, 0, 0, 0,
                                        SWP_ZORDER | SWP_SHOW | SWP_RESTORE);
                        // start or restart timer for flashing
                        // fixed V0.9.4 (2000-07-10) [umoeller]
                        pgmgcStartFlashTimer();
                    }
                }

                // mouse switching is now possible again
                G_pHookData->fDisableMouseSwitch = FALSE;
            }
        }
    }
    CATCH(excpt1) {} END_CATCH();

    return (mrc);
}

/*
 *@@ fntMoveThread:
 *      this thread gets started when PageMage
 *      is started (dmnStartPageMage in xwpdaemn.c).
 *      It creates an object window which receives
 *      notifications mostly from fnwpPageMageClient.
 *
 *      This thread is responsible for moving
 *      windows around and doing other stuff which
 *      should be done synchronusly.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 *@@changed V0.9.3 (2000-04-10) [umoeller]: renamed from fntMoveThread
 *@@changed V0.9.4 (2000-07-10) [umoeller]: fixed flashing; now using a win timer
 *@@changed V0.9.7 (2001-01-20) [umoeller]: now using higher priority
 */

VOID _Optlink fntMoveThread(PTHREADINFO pti)
{
    // _Pmpf(("Move thread running..."));

    // give ourselves higher priority...
    // otherwise we can't compete with Netscape and Win-OS/2 windows.
    DosSetPriority(PRTYS_THREAD,
                   PRTYC_REGULAR,
                   PRTYD_MAXIMUM,
                   0);      // current thread

    WinRegisterClass(pti->hab,
                     WC_MOVETHREAD,
                     fnwpMoveThread,
                     0, 0);

    // _Pmpf(("Move thread object window is 0x%lX", G_pHookData->hwndPageMageMoveThread));

    if (G_pHookData->hwndPageMageMoveThread
            = WinCreateWindow(HWND_OBJECT,
                              WC_MOVETHREAD,
                              (PSZ)"",
                              0,
                              0,0,0,0,
                              0,
                              HWND_BOTTOM,
                              0,
                              NULL,
                              NULL))
    {
        QMSG qmsg;
        while (WinGetMsg(pti->hab, &qmsg, NULLHANDLE, 0, 0))
            WinDispatchMsg(pti->hab, &qmsg);
    }

    // _Pmpf(("Exiting Move thread."));

    // destroy the object window V0.9.12 (2001-05-12) [umoeller]
    WinDestroyWindow(G_pHookData->hwndPageMageMoveThread);
    G_pHookData->hwndPageMageMoveThread = NULLHANDLE;
}

/*
 *@@ pgmmRecoverAllWindows:
 *      recovers all windows (hopefully).
 *
 *      Gets called on WM_CLOSE in fnwpPageMageClient.
 *      This makes sure that when PageMage terminates
 *      for whatever reason, the user isn't left with
 *      windows which are completely off the screen.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 */

VOID pgmmRecoverAllWindows(VOID)
{
    // shortcuts to global pagemage config
    PPAGEMAGECONFIG pPageMageConfig = &G_pHookData->PageMageConfig;
    PPOINTL         pptlMaxDesktops = &pPageMageConfig->ptlMaxDesktops;

    #define MAX_WINDOWS 1000        // yes, that's a lot... too lazy
                                    // to build another linked list here

    HENUM       henum;
    HWND        hwndTemp[MAX_WINDOWS];
    SWP         swpTemp[MAX_WINDOWS];
    ULONG       usIdx,
                usIdx2;
    CHAR        szClassName[PGMG_TEXTLEN];
    // BOOL        bResult;

    usIdx = 0;
    henum = WinBeginEnumWindows(HWND_DESKTOP);
    while ((hwndTemp[usIdx] = WinGetNextWindow(henum)) != NULLHANDLE)
    {
        PSWP         pswpThis = &swpTemp[usIdx];
        WinQueryWindowPos(hwndTemp[usIdx], pswpThis);

        WinQueryClassName(hwndTemp[usIdx], PGMG_TEXTLEN, szClassName);

        if (    ((pswpThis->fl & SWP_HIDE) == 0)
             && (!(pswpThis->fl & SWP_MINIMIZE))
             && (WinIsChild(hwndTemp[usIdx], HWND_DESKTOP))
             && (strcmp(szClassName, "#32765"))
                // not WPS window:
             && (hwndTemp[usIdx] != G_pHookData->hwndWPSDesktop)
             && (!pgmmIsPartlyOnCurrentDesktop(pswpThis))
                                    // V0.9.7 (2001-01-22) [umoeller]
             && (!WinIsChild(hwndTemp[usIdx], G_pHookData->hwndPageMageFrame))
           )
            usIdx++;
    }
    WinEndEnumWindows(henum);

    for (usIdx2 = 0; usIdx2 < usIdx; usIdx2++)
    {
        PSWP         pswpThis = &swpTemp[usIdx2];
        WinQueryWindowPos(hwndTemp[usIdx2],
                          pswpThis);

        pswpThis->fl = SWP_MOVE;
        if (!WinIsWindowVisible(hwndTemp[usIdx2]))
            pswpThis->fl |= SWP_HIDE;

        pswpThis->x =    (   (swpTemp[usIdx2].x)
                              + (   pptlMaxDesktops->x
                                    * G_szlEachDesktopReal.cx
                                )
                            ) % G_szlEachDesktopReal.cx;
        pswpThis->y = (   (swpTemp[usIdx2].y)
                              + (   pptlMaxDesktops->y
                                    * G_szlEachDesktopReal.cy
                                )
                            ) % G_szlEachDesktopReal.cy;

        if (pswpThis->x > G_szlEachDesktopReal.cx - 5)
            pswpThis->x = pswpThis->x - G_szlEachDesktopReal.cx;
        if (pswpThis->y > G_szlEachDesktopReal.cy - 5)
            pswpThis->y = pswpThis->y - G_szlEachDesktopReal.cy;

        pswpThis->hwnd = hwndTemp[usIdx2];
        pswpThis->hwndInsertBehind = HWND_TOP;
    }

    /* bResult =  */ WinSetMultWindowPos(NULLHANDLE, swpTemp, usIdx);
}

