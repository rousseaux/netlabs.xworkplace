
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

#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_WIN
#define INCL_GPI
#include <os2.h>

#include <stdio.h>
#include <setjmp.h>

#include "setup.h"                      // code generation and debugging options

#include "helpers\except.h"
#include "helpers\threads.h"
#include "helpers\winh.h"               // PM helper routines

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
 *          Positive lXDelta:  at left edge of screen, panning left, window shift +
 *          Negative lXDelta:  at right edge of screen, panning right, window shift -
 *          Negative lYDelta:  at top edge of screen, panning up, window shift -
 *          Positive lYDelta:  at bottom edge of screen, panning down, window shift +
 *      Returns:
 *          TRUE:  moved successfully
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 *@@changed V0.9.3 (2000-04-10) [umoeller]: fixed excessive window repainting
 *@@changed V0.9.4 (2000-08-08) [umoeller]: added special maximized window handling
 */

INT pgmmMoveIt(LONG lXDelta,
               LONG lYDelta,
               BOOL bAllowUpdate)
{
    BOOL        bResult = FALSE;

    if (!bAllowUpdate)
        WinEnableWindowUpdate(G_pHookData->hwndPageMageClient, FALSE);

    if (DosRequestMutexSem(G_hmtxWindowList, TIMEOUT_PGMG_WINLIST)
            == NO_ERROR)
    {
        HWND        ahwndMoveList[MAX_WINDOWS];
                    // here we build a second list of windows to be moved
        SWP         swp[MAX_WINDOWS];
        USHORT      usMoveListCtr = 0;
        USHORT      usIdx = 0;

        // now go thru all windows on the main list and copy them
        // to the move list, if necessary
        for (usIdx = 0; usIdx < G_usWindowCount; usIdx++)
        {
            BOOL fRescan = FALSE;

            if (!WinIsWindow(G_habDaemon, G_MainWindowList[usIdx].hwnd))
                // window no longer valid:
                // mark for rescan so it will be removed later
                G_MainWindowList[usIdx].bWindowType = WINDOW_RESCAN;
            else
            {
                WinQueryWindowPos(G_MainWindowList[usIdx].hwnd, &G_MainWindowList[usIdx].swp);
                bResult = FALSE;

                // fix outdated minimize flags
                if (    (G_MainWindowList[usIdx].bWindowType == WINDOW_MINIMIZE)
                     && ( (G_MainWindowList[usIdx].swp.fl & SWP_MINIMIZE) == 0)
                   )
                    // no longer minimized:
                    fRescan = TRUE;
                else if (    (G_MainWindowList[usIdx].bWindowType == WINDOW_MAXIMIZE)
                          && ( (G_MainWindowList[usIdx].swp.fl & SWP_MAXIMIZE) == 0)
                        )
                        // no longer minimized:
                        fRescan = TRUE;

                if (G_MainWindowList[usIdx].bWindowType == WINDOW_NORMAL)
                {
                    if (G_MainWindowList[usIdx].swp.fl & SWP_MINIMIZE)
                        // now minimized:
                        G_MainWindowList[usIdx].bWindowType = WINDOW_MINIMIZE;
                    else if (G_MainWindowList[usIdx].swp.fl & SWP_MAXIMIZE)
                        // now maximized:
                        G_MainWindowList[usIdx].bWindowType = WINDOW_MAXIMIZE;
                }

                if (fRescan)
                {
                    if (pgmwGetWinInfo(G_MainWindowList[usIdx].hwnd,
                                       &G_MainWindowList[usIdx]))
                    {
                        // window still valid:
                        bResult = TRUE;
                    }
                    else
                        // window no longer valid:
                        // remove from list during next paint
                        G_MainWindowList[usIdx].bWindowType = WINDOW_RESCAN;
                }
                else
                    if (   (G_MainWindowList[usIdx].bWindowType == WINDOW_MAXIMIZE)
                        || (G_MainWindowList[usIdx].bWindowType == WINDOW_MAX_OFF)
                       )
                        // may even be hidden
                        bResult = TRUE;
                    else if (   (G_MainWindowList[usIdx].bWindowType == WINDOW_NORMAL)
                             && (!(G_MainWindowList[usIdx].swp.fl & SWP_HIDE))
                            )
                        bResult = TRUE;

                if (bResult)
                {
                    // OK, window to be moved:

                    // default flags
                    ULONG   fl = SWP_MOVE | SWP_NOADJUST;

                    ahwndMoveList[usMoveListCtr] = G_MainWindowList[usIdx].hwnd;

                    WinQueryWindowPos(ahwndMoveList[usMoveListCtr],
                                      &swp[usMoveListCtr]);
                    swp[usMoveListCtr].hwnd = ahwndMoveList[usMoveListCtr];
                    swp[usMoveListCtr].x += lXDelta;
                    swp[usMoveListCtr].y += lYDelta;

                    if (swp[usMoveListCtr].fl & SWP_MAXIMIZE)
                    {
                        // currently maximized:
                        // this can be one of two sorts:
                        if (G_MainWindowList[usIdx].bWindowType == WINDOW_MAX_OFF)
                        {
                            // this is a maxiized window which has been hidden by
                            // us previously: check if it's currently being shown

                            if (    // is desktop left in window?
                                    (swp[usMoveListCtr].x < 0)
                                 && (swp[usMoveListCtr].x + swp[usMoveListCtr].cx > G_pHookData->lCXScreen)
                                    // and desktop bottom in window?
                                 && (swp[usMoveListCtr].y < 0)
                                 && (swp[usMoveListCtr].y + swp[usMoveListCtr].cy > G_pHookData->lCYScreen)
                               )
                            {
                                // yes:
                                // unhide
                                fl = SWP_SHOW | SWP_MOVE | SWP_NOADJUST;
                                // and mark as maximized again
                                G_MainWindowList[usIdx].bWindowType = WINDOW_MAXIMIZE;
                            }
                        }
                        else
                        {
                            // regular maximized window on current Desktop:
                            // hide to avoid the borders on the adjacent screen
                            fl = SWP_HIDE | SWP_MOVE | SWP_NOADJUST;
                            // and mark this as "hidden by us" in the window list
                            // so we can un-hide it later
                            G_MainWindowList[usIdx].bWindowType = WINDOW_MAX_OFF;
                        }
                    }

                    swp[usMoveListCtr].fl = fl;

                    usMoveListCtr++;
                }
            }
        }
        DosReleaseMutexSem(G_hmtxWindowList);

        // now set all windows at once, this saves a lot of
        // repainting...
        bResult = WinSetMultWindowPos(NULLHANDLE, (PSWP)&swp, usMoveListCtr);
    }

    if (!bAllowUpdate)
        WinEnableWindowUpdate(G_pHookData->hwndPageMageClient, TRUE);

    return (bResult);
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

INT pgmmZMoveIt(LONG lXDelta,
                LONG lYDelta)
{
    BOOL        bReturn = FALSE;
    LONG        lRightEdge;
    LONG        lBottomEdge;
    LONG        lNewPosX;
    LONG        lNewPosY;

    lRightEdge = G_pHookData->PageMageConfig.ptlMaxDesktops.x * G_pHookData->lCXScreen;
    lBottomEdge = G_pHookData->PageMageConfig.ptlMaxDesktops.y * G_pHookData->lCYScreen;
    lNewPosX = G_ptlCurrPos.x - lXDelta;
    lNewPosY = G_ptlCurrPos.y + lYDelta;

    // determine if deltas need adjusting due to wraparound or hitting edges
    if (lXDelta != 0)
    {
        if (lNewPosX < 0)
        {
            if (G_ptlCurrPos.x == 0)
            {
                if (G_pHookData->PageMageConfig.bWrapAround)
                    lXDelta = - (LONG)((G_pHookData->PageMageConfig.ptlMaxDesktops.x - 1) * G_pHookData->lCXScreen);
                else
                    lXDelta = 0;
            }
            else
                lXDelta = G_ptlCurrPos.x;
        }
        else
        {
            if ((lNewPosX + G_pHookData->lCXScreen) >= lRightEdge)
            {
                if ((G_ptlCurrPos.x + G_pHookData->lCXScreen) == lRightEdge)
                {
                    if (G_pHookData->PageMageConfig.bWrapAround)
                        lXDelta = (G_pHookData->PageMageConfig.ptlMaxDesktops.x - 1) * G_pHookData->lCXScreen;
                    else
                        lXDelta = 0;
                }
                else
                    lXDelta = G_ptlCurrPos.x - lRightEdge + G_pHookData->lCXScreen;
            }
        }
    }

    if (lYDelta != 0)
    {
        if (lNewPosY < 0)
        {
            if (G_ptlCurrPos.y == 0)
            {
                if (G_pHookData->PageMageConfig.bWrapAround)
                    lYDelta = (G_pHookData->PageMageConfig.ptlMaxDesktops.y - 1) * G_pHookData->lCYScreen;
                else
                    lYDelta = 0;
            } else
                lYDelta = -G_ptlCurrPos.y;
        }
        else
        {
            if ((lNewPosY + G_pHookData->lCYScreen) >= lBottomEdge)
            {
                if ((G_ptlCurrPos.y + G_pHookData->lCYScreen) == lBottomEdge)
                {
                    if (G_pHookData->PageMageConfig.bWrapAround)
                        lYDelta = - (LONG)((G_pHookData->PageMageConfig.ptlMaxDesktops.y - 1) * G_pHookData->lCYScreen);
                    else
                        lYDelta = 0;
                } else
                    lYDelta = lBottomEdge - G_pHookData->lCYScreen - G_ptlCurrPos.y;
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
 */

MRESULT EXPENTRY fnwpMoveThread(HWND hwndObject, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT     mrc = 0;
    LONG        lDeltaX = 0,
                lDeltaY = 0;

    TRY_LOUD(excpt1, NULL)
    {
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
                    lDeltaX = lX / (G_ptlEachDesktop.x + 1);
                    lDeltaY = G_pHookData->PageMageConfig.ptlMaxDesktops.y - 1 - lY
                              / (G_ptlEachDesktop.y + 1);
                    lDeltaX *= G_pHookData->lCXScreen;
                    lDeltaY *= G_pHookData->lCYScreen;
                    lDeltaX = G_ptlCurrPos.x - lDeltaX;
                    lDeltaY -= G_ptlCurrPos.y;
                }

                if (G_pHookData->PageMageConfig.fClick2Activate)
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
                        lDeltaX = G_pHookData->lCXScreen;
                        break;
                    case 0x64:                              // right
                        lDeltaX = -G_pHookData->lCXScreen;
                        break;
                    case 0x61:                              // up
                        lDeltaY = -G_pHookData->lCYScreen;
                        break;
                    case 0x66:                              // down
                        lDeltaY = G_pHookData->lCYScreen;
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
                WinQueryWindowPos(hwndActive, &swpActive);

                // make sure we pick up valid windows only
                if (!(swpActive.fl & (SWP_HIDE | SWP_MINIMIZE)))
                {
                    // absolute coordinate (top left is (0,0)) of all desktops
                    LONG lAbsX = swpActive.x + (swpActive.cx / 2);
                    LONG lAbsY = swpActive.y + (swpActive.cy / 2);
                    lAbsX += G_ptlCurrPos.x;
                    lAbsY = G_ptlCurrPos.y + G_pHookData->lCYScreen - lAbsY;

                    // if we intend to move into a valid window
                    if (    (lAbsX >= 0)
                         && (lAbsX <= (G_pHookData->PageMageConfig.ptlMaxDesktops.x
                                       * G_pHookData->lCXScreen))
                         && (lAbsY >= 0)
                         && (lAbsY <= (G_pHookData->PageMageConfig.ptlMaxDesktops.y
                                       * G_pHookData->lCYScreen))
                       )
                    {
                        // put abs coord of desktop in lAbs
                        lAbsX /= G_pHookData->lCXScreen;
                        lAbsY /= G_pHookData->lCYScreen;
                        lAbsX *= G_pHookData->lCXScreen;
                        lAbsY *= G_pHookData->lCYScreen;
                        lDeltaX = G_ptlCurrPos.x - lAbsX;
                        lDeltaY = lAbsY - G_ptlCurrPos.y;
                    }
                }
            break; }

            default:
                mrc = WinDefWindowProc(hwndObject, msg, mp1, mp2);
        }

        if (lDeltaX || lDeltaY)
        {
            // active Desktop changed:

            // move windows around to switch Desktops
            BOOL    bReturn = pgmmZMoveIt(lDeltaX, lDeltaY);

            if (bReturn)
            {
                // success: flashing enabled?
                if (G_pHookData->PageMageConfig.fFlash)
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

    G_pHookData->hwndPageMageMoveThread = winhCreateObjectWindow(WC_MOVETHREAD, NULL);

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
    HENUM       henum;
    HWND        hwndTemp[MAX_WINDOWS];
    SWP         swpTemp[MAX_WINDOWS];
    SWP         swpHold;
    USHORT      usIdx, usIdx2;
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
                                  + (   G_pHookData->PageMageConfig.ptlMaxDesktops.x
                                        * G_pHookData->lCXScreen
                                    )
                                ) % G_pHookData->lCXScreen;
            swpTemp[usIdx2].y = (   (swpTemp[usIdx2].y)
                                  + (   G_pHookData->PageMageConfig.ptlMaxDesktops.y
                                        * G_pHookData->lCYScreen
                                    )
                                ) % G_pHookData->lCYScreen;

            if (swpTemp[usIdx2].x > G_pHookData->lCXScreen - 5)
                swpTemp[usIdx2].x = swpTemp[usIdx2].x - G_pHookData->lCXScreen;
            if (swpTemp[usIdx2].y > G_pHookData->lCYScreen - 5)
                swpTemp[usIdx2].y = swpTemp[usIdx2].y - G_pHookData->lCYScreen;
        }

        swpTemp[usIdx2].hwnd = hwndTemp[usIdx2];
        swpTemp[usIdx2].hwndInsertBehind = HWND_TOP;
    }

    bResult = WinSetMultWindowPos(NULLHANDLE, (PSWP) &swpTemp, usIdx);

} // RecoverThread

