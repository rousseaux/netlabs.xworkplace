
/*
 *@@ pgmg_move.c:
 *      PageMage "Move" thread.
 *
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
#define INCL_WINSWITCHLIST
#define INCL_WINSYS
#include <os2.h>

#include <stdio.h>
#include <setjmp.h>

#include "setup.h"                      // code generation and debugging options

#include "helpers\except.h"
#include "helpers\threads.h"
// #include "helpers\winh.h"               // PM helper routines

#include "hook\xwphook.h"
#include "hook\hook_private.h"
#include "hook\xwpdaemn.h"              // PageMage and daemon declarations

#pragma hdrstop

#define WC_MOVETHREAD      "XWPPgmgMoveObj"

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

BOOL pgmmMoveIt(LONG lXDelta,
                LONG lYDelta,
                BOOL bAllowUpdate)
{
    BOOL    fAnythingMoved = FALSE;

    if (!bAllowUpdate)
        WinEnableWindowUpdate(G_pHookData->hwndPageMageClient, FALSE);

    if (DosRequestMutexSem(G_hmtxWindowList, TIMEOUT_PGMG_WINLIST)
            == NO_ERROR)
    {
        HWND        ahwndMoveList[MAX_WINDOWS];
                    // here we build a second list of windows to be moved
        SWP         swp[MAX_WINDOWS];
        ULONG       ulMoveListCtr = 0;
        ULONG       usIdx = 0;

        LONG        bx = WinQuerySysValue(HWND_DESKTOP, SV_CXSIZEBORDER);
        LONG        by = WinQuerySysValue(HWND_DESKTOP, SV_CYSIZEBORDER);

        // now go thru all windows on the main list and copy them
        // to the move list, if necessary
        for (usIdx = 0;
             usIdx < G_usWindowCount;
             usIdx++)
        {
            PPGMGLISTENTRY pEntryThis = &G_MainWindowList[usIdx];

            BOOL fRescanThis = FALSE;
                    // if TRUE, this gets rescanned

            if (!WinIsWindow(G_habDaemon, pEntryThis->hwnd))
                // window no longer valid:
                // mark for rescan so it will be removed later
                // pEntryThis->bWindowType = WINDOW_RESCAN;
                                // V0.9.7 (2001-01-18) [umoeller]
                fRescanThis = TRUE;
            else
            {
                BOOL fMoveThis = FALSE;

                // update window pos in winlist's SWP
                PSWP pswpThis = &pEntryThis->swp;
                WinQueryWindowPos(pEntryThis->hwnd, pswpThis);

                // fix outdated minimize flags
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
                    if (pswpThis->fl & SWP_MINIMIZE)
                        // now minimized:
                        pEntryThis->bWindowType = WINDOW_MINIMIZE;
                    else if (pswpThis->fl & SWP_MAXIMIZE)
                        // now maximized:
                        pEntryThis->bWindowType = WINDOW_MAXIMIZE;
                }

                if (fRescanThis)
                {
                    if (pgmwGetWinInfo(pEntryThis->hwnd,
                                       pEntryThis))
                    {
                        // window still valid:
                        fMoveThis = TRUE;
                    }
                    else
                        // window no longer valid:
                        // remove from list during next paint
                        pEntryThis->bWindowType = WINDOW_RESCAN;
                }
                else
                    if (   (pEntryThis->bWindowType == WINDOW_MAXIMIZE)
                        // || (pEntryThis->bWindowType == WINDOW_MAX_OFF)
                       )
                        // may even be hidden
                        fMoveThis = TRUE;
                    else if (   (pEntryThis->bWindowType == WINDOW_NORMAL)
                             && (!(pswpThis->fl & SWP_HIDE))
                            )
                        fMoveThis = TRUE;

                if (fMoveThis)
                {
                    // OK, window to be moved:

                    // default flags
                    ULONG   fl = SWP_MOVE | SWP_NOADJUST;

                    PSWP    pswpNewThis = &swp[ulMoveListCtr];

                    ahwndMoveList[ulMoveListCtr] = pEntryThis->hwnd;

                    WinQueryWindowPos(ahwndMoveList[ulMoveListCtr],
                                      pswpNewThis);

                    pswpNewThis->hwnd = ahwndMoveList[ulMoveListCtr];
                    // add the delta for moving
                    pswpNewThis->x += lXDelta;
                    pswpNewThis->y += lYDelta;

                    if (pswpNewThis->fl & SWP_MAXIMIZE)
                    {
                        // maximized:

                        // V0.9.7 (2001-01-18) [dk]: check if visible
                        /* BOOL bVisible
                            = !(    ((pswpNewThis->x + bx) >= G_szlEachDesktopReal.cx)
                                 || ((pswpNewThis->x + pswpNewThis->cx - bx) <= 0)
                                 || ((pswpNewThis->y + by) >= G_szlEachDesktopReal.cy)
                                 || ((pswpNewThis->y + pswpNewThis->cy - by) <= 0)
                               );

                        // currently maximized:
                        // this can be one of two sorts:
                        if (G_MainWindowList[usIdx].bWindowType == WINDOW_MAX_OFF)
                        {
                            // this is a maximized window which has been hidden by
                            // us previously: check if it's currently being shown
                            if (bVisible)
                            {
                                // yes:
                                // unhide
                                fl |= SWP_SHOW;
                                pEntryThis->bWindowType = WINDOW_MAXIMIZE;
                            }
                        }
                        else
                        {
                            if (!bVisible)
                            {
                                // regular maximized window on current Desktop:
                                // hide to avoid the borders on the adjacent screen
                                fl = SWP_HIDE | SWP_MOVE | SWP_NOADJUST;
                                // and mark this as "hidden by us" in the window list
                                // so we can un-hide it later
                                G_MainWindowList[usIdx].bWindowType = WINDOW_MAX_OFF;
                            }
                         } */

                        // check if this was hidden by us
                        /* if (pEntryThis->bMaximizedAndHiddenByUs) //  == WINDOW_MAX_OFF)
                        {
                            // this is a maximized window which has been hidden by
                            // us previously: check if it's currently being shown

                            // if (    // is desktop left in window?
                                    (pswpNewThis->x < 0)
                                 && (pswpNewThis->x + pswpNewThis->cx > G_szlEachDesktopReal.cx)
                                    // and desktop bottom in window?
                                 && (pswpNewThis->y < 0)
                                 && (pswpNewThis->y + pswpNewThis->cy > G_szlEachDesktopReal.cy)
                               )

                            // check if the new center is on the current desktop
                                    // V0.9.7 (2001-01-18) [umoeller]
                            LONG lCenterX = (pswpNewThis->x + pswpNewThis->cx) / 2L;
                            LONG lCenterY = (pswpNewThis->y + pswpNewThis->cy) / 2L;

                            if (    (lCenterX > 0L)
                                 && (lCenterX < G_szlEachDesktopReal.cx)
                                 && (lCenterY > 0L)
                                 && (lCenterY < G_szlEachDesktopReal.cy)
                               )
                            {
                                // yes:
                                // unhide in addition to moving
                                fl |= SWP_SHOW;
                                // and mark this as maximized again
                                // pEntryThis->bWindowType = WINDOW_MAXIMIZE;
                                pEntryThis->bMaximizedAndHiddenByUs = FALSE;
                            }
                            // else: this is not on our desktop, so do not
                            // unhide it
                        }
                        else
                        {
                            // regular maximized window on current Desktop:
                            // hide to avoid the borders on the adjacent screen
                            fl |= SWP_HIDE;
                            // and mark this as "hidden by us" in the window list
                            // so we can un-hide it later
                            // pEntryThis->bWindowType = WINDOW_MAX_OFF;
                            pEntryThis->bMaximizedAndHiddenByUs = TRUE;
                        } */
                    }

                    pswpNewThis->fl = fl;

                    ulMoveListCtr++;
                } // end if (bResult)
            } // end else if (!WinIsWindow(G_habDaemon, pEntryThis->hwnd))
        } // end for for (usIdx = 0;...
        DosReleaseMutexSem(G_hmtxWindowList);

        {
            // if window animations are enabled, turn them off for now.
                    // V0.9.7 (2001-01-18) [umoeller]
            BOOL fAnimation = FALSE;
            if (WinQuerySysValue(HWND_DESKTOP, SV_ANIMATION))
            {
                fAnimation = TRUE;
                WinSetSysValue(HWND_DESKTOP, SV_ANIMATION, FALSE);
            }

            G_pHookData->fDisableSwitching = TRUE;

            // now set all windows at once, this saves a lot of
            // repainting...
            fAnythingMoved = WinSetMultWindowPos(NULLHANDLE,
                                                 (PSWP)&swp,
                                                 ulMoveListCtr);

            G_pHookData->fDisableSwitching = FALSE;

            if (fAnimation)
                // turn animation back on
                WinSetSysValue(HWND_DESKTOP, SV_ANIMATION, TRUE);
        }
    }

    if (!bAllowUpdate)
        WinEnableWindowUpdate(G_pHookData->hwndPageMageClient, TRUE);

    return (fAnythingMoved);
} // pgmmMoveIt

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
 */

BOOL pgmmZMoveIt(LONG lXDelta,
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

    // determine if deltas need adjusting due to wraparound or hitting edges
    if (lXDelta != 0)
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

    if (lYDelta != 0)
    {
        if (lNewPosY < 0)
        {
            if (G_ptlCurrPos.y == 0)
            {
                if (pPageMageConfig->bWrapAround)
                    lYDelta = (pptlMaxDesktops->y - 1) * G_szlEachDesktopReal.cy;
                else
                    lYDelta = 0;
            } else
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
                } else
                    lYDelta = lBottomEdge - G_szlEachDesktopReal.cy - G_ptlCurrPos.y;
            }
        }
    }

    if (lXDelta || lYDelta)
    {
        bReturn = pgmmMoveIt(lXDelta, lYDelta, FALSE);
        if (bReturn)
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
 *      PageMage "Move" thread (fntMoveQueueThread).
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
                LONG   lX = (ULONG)mp1,
                       lY = (ULONG)mp2;

                // calc new desktop if mb1 was clicked (always)
                if (msg == PGOM_CLICK2ACTIVATE)
                {
                    // set lDeltas to which desktop we want
                    lDeltaX = lX / (G_szlEachDesktopInClient.cx + 1);
                    lDeltaY = pptlMaxDesktops->y - 1 - lY
                              / (G_szlEachDesktopInClient.cy + 1);
                    lDeltaX *= G_szlEachDesktopReal.cx;
                    lDeltaY *= G_szlEachDesktopReal.cy;
                    lDeltaX = G_ptlCurrPos.x - lDeltaX;
                    lDeltaY -= G_ptlCurrPos.y;
                }

                if (pPageMageConfig->fClick2Activate)
                {
                    HWND hwndActive = pgmwGetWindowFromClientPoint(lX, lY);

                    if (hwndActive != NULLHANDLE)
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
            break; }

            /*
             *@@ PGOM_HOOKKEY:
             *      posted by hookPreAccelHook when
             *      one of the arrow hotkeys was
             *      detected.
             *
             *      (UCHAR)mp1 has the scan code of the
             *      key which was pressed.
             */

            case PGOM_HOOKKEY:
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
             */

            case PGOM_FOCUSCHANGE:
            {
                SWP         swpActive;
                // determine the middle coordinate of the active window
                HWND hwndActive = WinQueryActiveWindow(HWND_DESKTOP);

                // test if this is a stick window;
                // if so, never switch desktops
                // V0.9.7 (2000-12-04) [umoeller]
                HSWITCH hsw = WinQuerySwitchHandle(hwndActive, 0);
                if (hsw)
                {
                    SWCNTRL swc;
                    if (WinQuerySwitchEntry(hsw, &swc))
                        if (pgmwStickyCheck(swc.szSwtitle))
                            // it's sticky: get outta here
                            break;
                }

                WinQueryWindowPos(hwndActive, &swpActive);

                // make sure we pick up valid windows only
                if (!(swpActive.fl & (SWP_HIDE | SWP_MINIMIZE)))
                {
                    // absolute coordinate (top left is (0,0)) of all desktops
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
            break; }

            default:
                mrc = WinDefWindowProc(hwndObject, msg, mp1, mp2);
        } // end switch (msg)

        if (lDeltaX || lDeltaY)
        {
            // active Desktop changed:

            // move windows around to switch Desktops
            BOOL    bReturn = pgmmZMoveIt(lDeltaX, lDeltaY);

            if (bReturn)
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
        }
    }
    CATCH(excpt1) {} END_CATCH();

    return (mrc);
}

/*
 *@@ fntMoveQueueThread:
 *      this thread gets started when PageMage
 *      is started (dmnStartPageMage in xwpdaemn.c).
 *      It continually reads from a CP queue for
 *      commands, which are written into the queue
 *      mostly by fnwpPageMageClient.
 *
 *      This thread is responsible for moving
 *      windows around and doing other stuff which
 *      should be done synchronusly.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 *@@changed V0.9.3 (2000-04-10) [umoeller]: renamed from fntMoveThread
 *@@changed V0.9.4 (2000-07-10) [umoeller]: fixed flashing; now using a win timer
 */

VOID _Optlink fntMoveQueueThread(PTHREADINFO pti)
{
    _Pmpf(("Move thread running..."));

    WinRegisterClass(pti->hab,
                     WC_MOVETHREAD,
                     fnwpMoveThread,
                     0, 0);

    G_pHookData->hwndPageMageMoveThread
            = WinCreateWindow(HWND_OBJECT,
                              WC_MOVETHREAD,
                              (PSZ)"",
                              0,
                              0,0,0,0,
                              0,
                              HWND_BOTTOM,
                              0,
                              NULL,
                              NULL);

    _Pmpf(("Move thread object window is 0x%lX", G_pHookData->hwndPageMageMoveThread));

    if (G_pHookData->hwndPageMageMoveThread)
    {
        QMSG qmsg;
        while (WinGetMsg(pti->hab, &qmsg, NULLHANDLE, 0, 0))
            // dispatch the queue msg
            WinDispatchMsg(pti->hab, &qmsg);
    }

    _Pmpf(("Exiting Move thread."));
}

/*
 *@@ pgmmRecoverAllWindows:
 *      recovers all windows (hopefully).
 *      Gets called on WM_CLOSE in fnwpPageMageClient.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 */

VOID pgmmRecoverAllWindows(VOID)
{
    // shortcuts to global pagemage config
    PPAGEMAGECONFIG pPageMageConfig = &G_pHookData->PageMageConfig;
    PPOINTL         pptlMaxDesktops = &pPageMageConfig->ptlMaxDesktops;

    HENUM       henum;
    HWND        hwndTemp[MAX_WINDOWS];
    SWP         swpTemp[MAX_WINDOWS];
    SWP         swpHold;
    ULONG       usIdx,
                usIdx2;
    CHAR        szClassName[PGMG_TEXTLEN];
    BOOL        bResult;

    usIdx = 0;
    henum = WinBeginEnumWindows(HWND_DESKTOP);
    while ((hwndTemp[usIdx] = WinGetNextWindow(henum)) != NULLHANDLE)
    {
        WinQueryWindowPos(hwndTemp[usIdx], &swpHold);

        WinQueryClassName(hwndTemp[usIdx], PGMG_TEXTLEN, szClassName);
        if (    (// (WinIsWindowVisible(hwndTemp[usIdx]))
                    ((swpHold.fl & (SWP_HIDE | SWP_MAXIMIZE)) == (SWP_HIDE | SWP_MAXIMIZE))
                 || ((swpHold.fl & SWP_HIDE) == 0)
                )
             && (!(swpHold.fl & SWP_MINIMIZE))
             && (WinIsChild(hwndTemp[usIdx], HWND_DESKTOP))
             && (strcmp(szClassName, "#32765"))
                // not WPS window:
             && (hwndTemp[usIdx] != G_pHookData->hwndWPSDesktop)
             && (!WinIsChild(hwndTemp[usIdx], G_pHookData->hwndPageMageFrame))
           )
            usIdx++;
    }
    WinEndEnumWindows(henum);

    for (usIdx2 = 0; usIdx2 < usIdx; usIdx2++)
    {
        WinQueryWindowPos(hwndTemp[usIdx2],
                          &swpTemp[usIdx2]);
        if ((swpHold.fl & (SWP_HIDE | SWP_MAXIMIZE)) == (SWP_HIDE | SWP_MAXIMIZE))
            // hidden and maximized by us:
            swpTemp[usIdx2].fl = SWP_RESTORE;
        else
        {
            swpTemp[usIdx2].fl = SWP_MOVE;
            if (!WinIsWindowVisible(hwndTemp[usIdx2]))
                swpTemp[usIdx2].fl |= SWP_HIDE;

            swpTemp[usIdx2].x = (   (swpTemp[usIdx2].x)
                                  + (   pptlMaxDesktops->x
                                        * G_szlEachDesktopReal.cx
                                    )
                                ) % G_szlEachDesktopReal.cx;
            swpTemp[usIdx2].y = (   (swpTemp[usIdx2].y)
                                  + (   pptlMaxDesktops->y
                                        * G_szlEachDesktopReal.cy
                                    )
                                ) % G_szlEachDesktopReal.cy;

            if (swpTemp[usIdx2].x > G_szlEachDesktopReal.cx - 5)
                swpTemp[usIdx2].x = swpTemp[usIdx2].x - G_szlEachDesktopReal.cx;
            if (swpTemp[usIdx2].y > G_szlEachDesktopReal.cy - 5)
                swpTemp[usIdx2].y = swpTemp[usIdx2].y - G_szlEachDesktopReal.cy;
        }

        swpTemp[usIdx2].hwnd = hwndTemp[usIdx2];
        swpTemp[usIdx2].hwndInsertBehind = HWND_TOP;
    }

    bResult = WinSetMultWindowPos(NULLHANDLE, (PSWP) &swpTemp, usIdx);

} // RecoverThread

