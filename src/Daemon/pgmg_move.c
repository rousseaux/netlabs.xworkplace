
/*
 *@@ pgmg_move.c:
 *      PageMage window moving...
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
#define INCL_WIN
#define INCL_GPI
#include <os2.h>
#include <stdio.h>
#include <setjmp.h>

#include "setup.h"                      // code generation and debugging options

#include "helpers\except.h"
#include "helpers\threads.h"

#include "hook\xwphook.h"
#include "hook\hook_private.h"
#include "hook\xwpdaemn.h"              // PageMage and daemon declarations

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
 *@@change3d V0.9.3 (2000-04-10) [umoeller]: renamed from fntMoveThread
 */

VOID _Optlink fntMoveQueueThread(PTHREADINFO pti)
{
    REQUESTDATA rdRequest;
    ULONG       ulDataLength;
    PULONG      pulDataAddress;
    BYTE        byElemPriority;
    BYTE        byMsgType;
    POINTL      ptlMousePos;
    ULONG       ulParm1 = 0,
                ulParm2 = 0;
    LONG        lDeltaX = 0,
                lDeltaY = 0;
    LONG        lAbsX, lAbsY;
    BYTE        byQuit = FALSE;
    SWP         swpActive;

    _Pmpf(("Move thread running..."));

    TRY_LOUD(excpt1, NULL)
    {
        while (!byQuit)
        {
            DosReadQueue(G_hqPageMage,
                         &rdRequest,
                         &ulDataLength,
                         (PVOID *)&pulDataAddress,
                         0,
                         DCWW_WAIT,         // wait --> block thread
                         &byElemPriority,
                         NULLHANDLE);
            byMsgType = MSGFROMPGMGQ(rdRequest.ulData);
            ulParm1 = PARM1FROMPGMGQ(rdRequest.ulData);
            ulParm2 = PARM2FROMPGMGQ(rdRequest.ulData);
            lDeltaX = 0;
            lDeltaY = 0;
            switch (byMsgType)
            {
                /*
                 *@@ PGMGQ_QUIT:
                 *
                 */

                case PGMGQ_QUIT:
                    byQuit = TRUE;
                    break;

                /*
                 *@@ PGMGQ_MOUSEMOVE:
                 *      posted by mouse move thread; currently not used.
                 */

                /* case PGMGQ_MOUSEMOVE:
                    if (ulParm1 & MOVE_LEFT)
                        lDeltaX = G_ptlMoveDelta.x;
                    else if (ulParm1 & MOVE_RIGHT)
                        lDeltaX = -G_ptlMoveDelta.x;
                    if (ulParm1 & MOVE_UP)
                        lDeltaY = -G_ptlMoveDelta.y;
                    else if (ulParm1 & MOVE_DOWN)
                        lDeltaY = G_ptlMoveDelta.y;
                    if (G_pHookData->PageMageConfig.bRepositionMouse)
                    {
                        WinQueryPointerPos(HWND_DESKTOP, &ptlMousePos);
                        if (ulParm1 & MOVE_LEFT)
                            ptlMousePos.x = G_pHookData->lCXScreen - 2;
                        else if (ulParm1 & MOVE_RIGHT)
                            ptlMousePos.x = 2;
                        if (ulParm1 & MOVE_UP)
                            ptlMousePos.y = 2;
                        else if (ulParm1 & MOVE_DOWN)
                            ptlMousePos.y = G_pHookData->lCYScreen - 2;
                        WinSetPointerPos(HWND_DESKTOP, ptlMousePos.x, ptlMousePos.y);
                    }
                    break;
                */

                /*
                 *@@ PGMGQ_CLICK2ACTIVATE:
                 *      written into the queue when
                 *      WM_BUTTON1CLICK was received
                 *      by fnwpPageMageClient.
                 */

                case PGMGQ_CLICK2ACTIVATE:      // mouse button 1
                case PGMGQ_CLICK2LOWER:         // mouse button 2
                    // set lDeltas to which desktop we want
                    lDeltaX = ulParm1 / (G_ptlEachDesktop.x + 1);
                    lDeltaY = G_pHookData->PageMageConfig.ptlMaxDesktops.y - 1 - ulParm2
                              / (G_ptlEachDesktop.y + 1);
                    lDeltaX *= G_pHookData->lCXScreen;
                    lDeltaY *= G_pHookData->lCYScreen;
                    lDeltaX = G_ptlCurrPos.x - lDeltaX;
                    lDeltaY -= G_ptlCurrPos.y;

                    if (G_pHookData->PageMageConfig.fClick2Activate)
                    {
                        HWND hwndActive = pgmwGetWindowFromClientPoint(ulParm1, ulParm2);
                        if (hwndActive != NULLHANDLE)
                            // we must have a msg queue to use WinSetActiveWindow(),
                            // so post this back
                            if (byMsgType == PGMGQ_CLICK2ACTIVATE)
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
                    break;

                /*
                 *@@ PGMGQ_KEY:
                 *      currently not used.
                 */

                /* case PGMGQ_KEY:
                    switch (ulParm1)
                    {
                        case VK_LEFT:
                            lDeltaX = G_ptlMoveDelta.x;
                            break;
                        case VK_RIGHT:
                            lDeltaX = -G_ptlMoveDelta.x;
                            break;
                        case VK_UP:
                            lDeltaY = -G_ptlMoveDelta.y;
                            break;
                        case VK_DOWN:
                            lDeltaY = G_ptlMoveDelta.y;
                            break;
                    }
                    break; */

                /*
                 *@@ PGMGQ_HOOKKEY:
                 *      posted by hookPreAccelHook when
                 *      one of the arrow hotkeys was
                 *      detected. ulParm1 contains the
                 *      keyboard scan code of the arrow
                 *      key, which we examine here to
                 *      switch desktops.
                 */

                case PGMGQ_HOOKKEY:
                    switch (ulParm1)
                    {
                        case 0x63:                              // left
                            lDeltaX = G_ptlMoveDelta.x;
                            break;
                        case 0x64:                              // right
                            lDeltaX = -G_ptlMoveDelta.x;
                            break;
                        case 0x61:                              // up
                            lDeltaY = -G_ptlMoveDelta.y;
                            break;
                        case 0x66:                              // down
                            lDeltaY = G_ptlMoveDelta.y;
                            break;
                    }
                    break;

                /*
                 *@@ PGMGQ_FOCUSCHANGE:
                 *      posted by ProcessMsgsForPageMage in
                 *      the hook when the focus or active
                 *      window changes. We then check whether
                 *      we need to switch desktops and whether
                 *      the PageMage client needs to be repainted.
                 */

                case PGMGQ_FOCUSCHANGE:
                {
                    // determine the middle coordinate of the active window
                    HWND hwndActive = WinQueryActiveWindow(HWND_DESKTOP);
                    WinQueryWindowPos(hwndActive, &swpActive);

                    // make sure we pick up valid windows only
                    if (!(swpActive.fl & (SWP_HIDE | SWP_MINIMIZE)))
                    {
                        // absolute coordinate (top left is (0,0)) of all desktops
                        lAbsX = swpActive.x + (swpActive.cx / 2);
                        lAbsY = swpActive.y + (swpActive.cy / 2);
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
                        DosSleep(G_pHookData->PageMageConfig.ulFlashDelay);
                        WinShowWindow(G_pHookData->hwndPageMageFrame, FALSE);
                    }
                }
            }
        }
    }
    CATCH(excpt1) {} END_CATCH();

}

/*
 *@@ pgmmMoveIt:
 *      moves all windows around the desktop according to the
 *      given X and Y deltas.
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
 */

INT pgmmMoveIt(LONG lXDelta,
               LONG lYDelta,
               BOOL bAllowUpdate)
{
    HWND        hwndMoveList[MAX_WINDOWS];
    SWP         swp[MAX_WINDOWS];
    USHORT      usMoveListCtr;
    USHORT      usIdx;
    BOOL        bResult;
    FILE        *fp;

    if (!bAllowUpdate)
        WinEnableWindowUpdate(G_pHookData->hwndPageMageClient, FALSE);

    usMoveListCtr = 0;
    DosRequestMutexSem(G_hmtxWindowList, SEM_INDEFINITE_WAIT);
    for (usIdx = 0; usIdx < G_usWindowCount; usIdx++)
    {
        WinQueryWindowPos(G_MainWindowList[usIdx].hwnd, &G_MainWindowList[usIdx].swp);
        bResult = FALSE;

        // fix outdated minimize flags
        if (    (G_MainWindowList[usIdx].bWindowType == WINDOW_MINIMIZED)
             && ( (G_MainWindowList[usIdx].swp.fl & SWP_MINIMIZE) == 0)
           )
            G_MainWindowList[usIdx].bWindowType = WINDOW_NORMAL;

        if (    (G_MainWindowList[usIdx].bWindowType == WINDOW_NORMAL)
             && (   !(G_MainWindowList[usIdx].swp.fl & SWP_HIDE))
                 && (WinIsWindow(G_habDaemon, G_MainWindowList[usIdx].hwnd)
                )
           )
            bResult = TRUE;
        /* if (G_MainWindowList[usIdx].bWindowType == WINDOW_RESCAN)
            bResult = TRUE; */

        if (bResult)
            hwndMoveList[usMoveListCtr++] = G_MainWindowList[usIdx].hwnd;
    }
    DosReleaseMutexSem(G_hmtxWindowList);

    for (usIdx = 0; usIdx < usMoveListCtr; usIdx++)
    {
        WinQueryWindowPos(hwndMoveList[usIdx], &swp[usIdx]);
        swp[usIdx].fl = SWP_MOVE | SWP_NOADJUST;
        swp[usIdx].x += lXDelta;
        swp[usIdx].y += lYDelta;
        swp[usIdx].hwnd = hwndMoveList[usIdx];
        // swp[usIdx].hwndInsertBehind = (HWND) 0;
        swp[usIdx].hwndInsertBehind = HWND_TOP;     // ignored anyways
        /* WinSetWindowPos(swp[usIdx].hwnd,     // removed V0.9.3 (2000-04-10) [umoeller]
                        NULLHANDLE,
                        swp[usIdx].x,
                        swp[usIdx].y,
                        0,
                        0,
                        SWP_MOVE | SWP_NOADJUST); */
    }
    bResult = WinSetMultWindowPos(NULLHANDLE, (PSWP) &swp, usIdx);

#if 0
    BOOL        b1, b2, b3;
    HWND        client;
    char        buf[60];
    for (usIdx = 0; usIdx < usMoveListCtr; usIdx++) {
      b1 = WinUpdateWindow(swp[usIdx].hwnd);
      b1 = WinInvalidateRect(swp[usIdx].hwnd, NULL, TRUE);
      b1 = WinShowWindow(swp[usIdx].hwnd, FALSE);
      b2 = WinShowWindow(swp[usIdx].hwnd, TRUE);
      b1 = WinUpdateWindow(swp[usIdx].hwnd);
      b2 = WinUpdateWindow(WinWindowFromID(swp[usIdx].hwnd, FID_CLIENT));
      WinSetWindowPos(swp[usIdx].hwnd, NULLHANDLE, 0, 0, 0, 0, SWP_MINIMIZE);
      WinSetWindowPos(swp[usIdx].hwnd, NULLHANDLE, 0, 0, 0, 0, SWP_RESTORE);
      b1=WinSendMsg(swp[usIdx].hwnd, WM_ERASEBACKGROUND,
      client = WinWindowFromID(swp[usIdx].hwnd, FID_CLIENT);
      WinQueryWindowText(swp[usIdx].hwnd, 50, buf);
printf("Window %d:  NULLHANDLE=%d, client=%d, buf=%s\n", usIdx,
       NULLHANDLE, client, buf);
      b2=WinPostMsg(client, WM_PAINT, 0, 0);
printf("Window %d:  b1=%d, b2=%d, b3=%d\n", usIdx, b1, b2, b3);
    }
#endif

    if (!bAllowUpdate)
        WinEnableWindowUpdate(G_pHookData->hwndPageMageClient, TRUE);

    if (!bResult) {
      //DosPostEventSem(hevWindowList);
    }

    return (bResult);
} // pgmmMoveIt

/*
 *@@ pgmmZMoveIt:
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

INT pgmmZMoveIt(LONG lXDelta, LONG lYDelta)
{
    BOOL        bReturn;
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

    DosRequestMutexSem(G_hmtxPageMage, SEM_INDEFINITE_WAIT);
    bReturn = pgmmMoveIt(lXDelta, lYDelta, FALSE);
    if (bReturn)
    {
        G_ptlCurrPos.x -= lXDelta;
        G_ptlCurrPos.y += lYDelta;
        WinPostMsg(G_pHookData->hwndPageMageClient,
                   PGMG_INVALIDATECLIENT,
                   (MPARAM)TRUE,        // immediately
                   MPVOID);
  /*      WinUpdateWindow(hwndClient);*/
    }
    DosReleaseMutexSem(G_hmtxPageMage);
    return (bReturn);
} // pgmmZMoveIt


/*
 * MouseMoveThread:
 * Parameters:
 *   zilch
 * Returns:
 *   zilch
 *
 * Keeps track of mouse position, calls move
 *
 */
/* VOID _Optlink fntMouseMoveThread(PVOID pvDummy)
{
    POINTL      ptlMousePos;
    ULONG       ulRequest;
    LONG        lOption;

    G_siVD.lXDim = G_pHookData->ulCXScreen;
    G_siVD.lYDim = G_pHookData->ulCYScreen;
    G_siVD.lXDim = WinQuerySysValue(HWND_DESKTOP, SV_CXSCREEN);
    G_siVD.lYDim = WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN);
    G_siVD.sLeftEdge = 0;
    G_siVD.sRightEdge = 0;
    G_siVD.sBottomEdge = 0;
    G_siVD.sTopEdge = 0;

    while (TRUE) {
      if (!G_bLockup) {
        lOption = 0;
        WinQueryPointerPos(HWND_DESKTOP, &ptlMousePos);

        if (ptlMousePos.x == 0) {
          G_siVD.sRightEdge = 0;
          G_siVD.sLeftEdge++;
        } else {
          if (ptlMousePos.x >= G_siVD.lXDim - 1) {
            G_siVD.sLeftEdge = 0;
            G_siVD.sRightEdge++;
          } else {
            G_siVD.sLeftEdge = 0;
            G_siVD.sRightEdge = 0;
          }
        }

        if (ptlMousePos.y == 0) {
          G_siVD.sTopEdge = 0;
          G_siVD.sBottomEdge++;
        } else {
          if (ptlMousePos.y >= G_siVD.lYDim - 1) {
            G_siVD.sBottomEdge = 0;
            G_siVD.sTopEdge++;
          } else {
            G_siVD.sBottomEdge = 0;
            G_siVD.sTopEdge = 0;
          }
        }

        if (G_pHookData->PageMageConfig.bPanAtLeft && (G_siVD.sLeftEdge >= G_pHookData->PageMageConfig.iEdgeBoundary)) {
          G_siVD.sLeftEdge = 0;
          lOption |= MOVE_LEFT;
        }

        if (G_pHookData->PageMageConfig.bPanAtRight && (G_siVD.sRightEdge >= G_pHookData->PageMageConfig.iEdgeBoundary)) {
          G_siVD.sRightEdge = 0;
          lOption |= MOVE_RIGHT;
        }

        if (G_pHookData->PageMageConfig.bPanAtBottom && (G_siVD.sBottomEdge >= G_pHookData->PageMageConfig.iEdgeBoundary)) {
          G_siVD.sBottomEdge = 0;
          lOption |= MOVE_DOWN;
        }

        if (G_pHookData->PageMageConfig.bPanAtTop && (G_siVD.sTopEdge >= G_pHookData->PageMageConfig.iEdgeBoundary)) {
          G_siVD.sTopEdge = 0;
          lOption |= MOVE_UP;
        }

        if (lOption) {
          ulRequest = PGMGQENCODE(PGMGQ_MOUSEMOVE, lOption, lOption);
          DosWriteQueue(G_hqPageMage, ulRequest, 0, NULL, 0);
        }
      }
      DosSleep(G_pHookData->PageMageConfig.ulSleepTime);
    } // while

} */ // MouseMoveThread

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
    CHAR        szClassName[TEXTLEN];
    BOOL        bResult;

    usIdx = 0;
    henum = WinBeginEnumWindows(HWND_DESKTOP);
    while ((hwndTemp[usIdx] = WinGetNextWindow(henum)) != NULLHANDLE)
    {
        WinQueryWindowPos(hwndTemp[usIdx], &swpHold);
        WinQueryClassName(hwndTemp[usIdx], TEXTLEN, szClassName);
        if (    (WinIsWindowVisible(hwndTemp[usIdx]))
             && (!(swpHold.fl & SWP_MINIMIZE))
             // && (!(swpHold.fl & SWP_HIDE))
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

        swpTemp[usIdx2].hwnd = hwndTemp[usIdx2];
        swpTemp[usIdx2].hwndInsertBehind = HWND_TOP;
    }

    bResult = WinSetMultWindowPos(NULLHANDLE, (PSWP) &swpTemp, usIdx);
    //DosPostEventSem(hevWindowList);

} // RecoverThread

