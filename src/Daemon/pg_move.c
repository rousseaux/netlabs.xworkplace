
/*
 *@@ pg_move.c:
 *      XPager "Move" thread.
 *
 *      See pg_control.c for an introduction to XPager.
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
#define INCL_WINSTDCNR
#define INCL_WINERRORS
#include <os2.h>

#include <stdio.h>
#include <setjmp.h>

#define DONT_REPLACE_FOR_DBCS
#define DONT_REPLACE_MALLOC         // in case mem debug is enabled
#include "setup.h"                      // code generation and debugging options

#include "helpers\except.h"
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\threads.h"

#include "xwpapi.h"                     // public XWorkplace definitions

#include "shared\kernel.h"              // XWorkplace Kernel

#include "hook\xwphook.h"
#include "hook\hook_private.h"
#include "hook\xwpdaemn.h"              // XPager and daemon declarations

#pragma hdrstop

PFNWP   G_pfnwpStaticOriginal = NULL;

/*
 *@@ MoveCurrentDesktop:
 *      moves the current desktop by moving windows around
 *      the screen. The delta values specify the pixels by
 *      which all windows should be moved. As a result,
 *      with respect to the current desktop:
 *
 *      --  positive dx moves the current desktop to
 *          the left;
 *
 *      --  negative dx moves the current desktop to
 *          the right;
 *
 *      --  positive dy moves the current desktop down;
 *
 *      --  negative dy moves the current desktop up.
 *
 *      Returns TRUE if the move was successful.
 *
 *@@added V0.9.19 (2002-05-07) [umoeller]
 */

BOOL MoveCurrentDesktop(HAB hab,
                        LONG dx,       // in: X delta
                        LONG dy)       // in: Y delta
{
    BOOL    brc = FALSE;

    LONG    cxEach = G_pHookData->szlEachDesktopFaked.cx,
            cDesktopsX = G_pHookData->PagerConfig.cDesktopsX,
            xCurrent = G_pHookData->ptlCurrentDesktop.x,
            xLimit = cDesktopsX * cxEach,
            xNew = xCurrent - dx;

    LONG    cyEach = G_pHookData->szlEachDesktopFaked.cy,
            cDesktopsY = G_pHookData->PagerConfig.cDesktopsY,
            yCurrent = G_pHookData->ptlCurrentDesktop.y,
            yLimit = cDesktopsY * cyEach,
            yNew = yCurrent - dy;

    ULONG   flPager = G_pHookData->PagerConfig.flPager;

    // bump X delta
    if (dx)
    {
        if (xNew < 0)
        {
            if (flPager & PGRFL_WRAPAROUND)
                dx = -((cDesktopsX - 1) * cxEach);
            else
                dx = 0;
        }
        else if ((xNew + cxEach) > xLimit)
        {
            if (flPager & PGRFL_WRAPAROUND)
                dx = (cDesktopsX - 1) * cxEach;
            else
                dx = 0;
        }
    }

    // bump Y delta
    if (dy)
    {
        if (yNew < 0)
        {
            if (flPager & PGRFL_WRAPAROUND)
                dy = -((cDesktopsY - 1) * cyEach);
            else
                dy = 0;
        }
        else if ((yNew + cyEach) > yLimit)
        {
            if (flPager & PGRFL_WRAPAROUND)
                dy = (cDesktopsY - 1) * cyEach;
            else
                dy = 0;
        }
    }

    if (dx || dy)
    {
        BOOL    fAnythingMoved = TRUE;

        PSWP    paswpNew = NULL;
        ULONG   cSwpNewUsed = 0;

        HWND    *pahwndNoMoveWithOwner = NULL;
        ULONG   cNoMoveWithOwner = 0;

        if (pgrLockWinlist())
        {
            // allocate an array of SWP entries for moving all windows
            // at once... we'll allocate one SWP entry for each item
            // on the wininfo list, but we'll probably not use them
            // all. cSwpNewUsed will be incremented for each item that's
            // actually used
            ULONG       cWinInfos,
                        cbSwpNew,
                        cbNoMoveWithOwner;

            if (    (cWinInfos = lstCountItems(&G_llWinInfos))
                 && (cbSwpNew = cWinInfos * sizeof(SWP))
                 && (cbNoMoveWithOwner  = cWinInfos * sizeof(HWND))
                 // allocate array of SWPs
                 && (paswpNew = (PSWP)malloc(cbSwpNew))
                 // allocate array of HWNDs for which we
                 // might disable FS_NOMOVEWITHOWNER
                 && (pahwndNoMoveWithOwner  = (HWND*)malloc(cbNoMoveWithOwner))
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

                    PXWININFO pEntryThis = (PXWININFO)pNode->pItemData;

                    if (!WinIsWindow(hab, pEntryThis->data.swctl.hwnd))
                        // window no longer valid:
                        // remove from the list NOW
                        WinPostMsg(G_pHookData->hwndDaemonObject,
                                   XDM_WINDOWCHANGE,
                                   (MPARAM)pEntryThis->data.swctl.hwnd,
                                   (MPARAM)WM_DESTROY);
                    else
                    {
                        BOOL fRefreshThis = FALSE;

                        if (     (!strcmp(pEntryThis->data.szClassName, "#1")
                              && (!(WinQueryWindowULong(pEntryThis->data.swctl.hwnd, QWL_STYLE)
                                            & FS_NOMOVEWITHOWNER))
                                 )
                           )
                        {
                            pahwndNoMoveWithOwner[cNoMoveWithOwner++] = pEntryThis->data.swctl.hwnd;
                            WinSetWindowBits(pEntryThis->data.swctl.hwnd,
                                             QWL_STYLE,
                                             FS_NOMOVEWITHOWNER,
                                             FS_NOMOVEWITHOWNER);
                        }

                        WinQueryWindowPos(pEntryThis->data.swctl.hwnd, &pEntryThis->data.swp);

                        // fix outdated minimize/maximize/hide flags
                        if (    (pEntryThis->data.bWindowType == WINDOW_MINIMIZE)
                             && ( (pEntryThis->data.swp.fl & SWP_MINIMIZE) == 0)
                           )
                            // no longer minimized:
                            fRefreshThis = TRUE;
                        else if (    (pEntryThis->data.bWindowType == WINDOW_MAXIMIZE)
                                  && ( (pEntryThis->data.swp.fl & SWP_MAXIMIZE) == 0)
                                )
                            // no longer minimized:
                            fRefreshThis = TRUE;

                        if (pEntryThis->data.bWindowType == WINDOW_NORMAL)
                        {
                            if (pEntryThis->data.swp.fl & SWP_HIDE)
                                fRefreshThis = TRUE;
                            else if (pEntryThis->data.swp.fl & SWP_MINIMIZE)
                                // now minimized:
                                pEntryThis->data.bWindowType = WINDOW_MINIMIZE;
                            else if (pEntryThis->data.swp.fl & SWP_MAXIMIZE)
                                // now maximized:
                                pEntryThis->data.bWindowType = WINDOW_MAXIMIZE;
                        }

                        if (fRefreshThis)
                        {
                            if (!pgrGetWinData(&pEntryThis->data))
                            {
                                // window no longer valid:
                                // remove from the list NOW
                                // V0.9.15 (2001-09-14) [umoeller]
                                // lstRemoveNode(&G_llWinInfos, pNode);
                                WinPostMsg(G_pHookData->hwndDaemonObject,
                                           XDM_WINDOWCHANGE,
                                           (MPARAM)pEntryThis->data.swctl.hwnd,
                                           (MPARAM)WM_DESTROY);

                                // update pEntryThis so that we don't try to
                                // move it later
                                // V0.9.19 (2002-04-04) [lafaix]
                                pEntryThis = NULL;
                            }
                        }

                        // move the window if it is not sticky, minimized,
                        // hidden, or invisible
                        // V0.9.19 (2002-04-04) [lafaix]
                        if (    (pEntryThis)
                             && (    (pEntryThis->data.bWindowType == WINDOW_MAXIMIZE)
                                  || (    (pEntryThis->data.bWindowType == WINDOW_NORMAL)
                                       && (!(pEntryThis->data.swp.fl & SWP_HIDE))
                                     )
                                  || (    (pEntryThis->data.bWindowType == WINDOW_NIL)
                                       && (!(pEntryThis->data.swp.fl & SWP_HIDE))
                                       && (WinQueryWindowULong(pEntryThis->data.swctl.hwnd,
                                                               QWL_STYLE)
                                                   & WS_VISIBLE)
                                     )
                                )
                           )
                        {
                            // OK, window to be moved:

                            // default flags
                            ULONG   fl = SWP_MOVE | SWP_NOADJUST;
                                    // SWP_NOADJUST is required or the windows
                                    // will end up in the middle of nowhere

                            PSWP    pswpNewThis = &paswpNew[cSwpNewUsed];

                            // we have queried the window pos above
                            memcpy(pswpNewThis,
                                   &pEntryThis->data.swp,
                                   sizeof(SWP));

                            pswpNewThis->hwnd = pEntryThis->data.swctl.hwnd;
                            // add the delta for moving
                            pswpNewThis->x += dx;
                            pswpNewThis->y += dy;

                            pswpNewThis->fl = fl;

                            // use next entry in SWP array
                            cSwpNewUsed++;

                        } // end if (    (pEntryThis->bWindowType == WINDOW_MAXIMIZE)...
                    }

                    pNode = pNext;      // V0.9.15 (2001-09-14) [umoeller]
                } // end while (pNode)
            } // end if (paswpNew = (PSWP)malloc(cbSwpNew))) etc.

            if (paswpNew)
            {
                if (cSwpNewUsed)
                {
                    // disable message processing in the hook
                    if (pgrLockHook(__FILE__, __LINE__, __FUNCTION__))
                    {
                        // now set all windows at once, this saves a lot of
                        // repainting...
                        fAnythingMoved = WinSetMultWindowPos(hab,
                                                             paswpNew,
                                                             cSwpNewUsed);
                        pgrUnlockHook();
                    }
                }

                // clean up SWP array
                free(paswpNew);
            } // if (paswpNew)

            // unset FS_NOMOVEWITHOWNER for the windows where we set it above
            while (cNoMoveWithOwner)
                WinSetWindowBits(pahwndNoMoveWithOwner[--cNoMoveWithOwner],
                                 QWL_STYLE,
                                 0,
                                 FS_NOMOVEWITHOWNER);

            pgrUnlockWinlist();
        }

        if (pahwndNoMoveWithOwner)
            free(pahwndNoMoveWithOwner);

        if (fAnythingMoved)
        {
            G_pHookData->ptlCurrentDesktop.x -= dx;
            G_pHookData->ptlCurrentDesktop.y -= dy;

            WinPostMsg(G_pHookData->hwndPagerClient,
                       PGRM_REFRESHCLIENT,
                       (MPARAM)FALSE,
                       0);

            if (flPager & PGRFL_FLASHTOTOP)
                WinSetWindowPos(G_pHookData->hwndPagerFrame,
                                HWND_TOP,
                                0, 0, 0, 0,
                                SWP_ZORDER | SWP_SHOW | SWP_RESTORE);

            brc = TRUE;
        }
    }

    return brc;
}

/*
 *@@ fnwpMoveThread:
 *      window procedure for the object window on the
 *      XPager "Move" thread (fntMoveThread).
 *
 *      This only receives the PGRM_MOVEBYDELTA
 *      message when the current desktop is to be moved.
 *
 *@@added V0.9.4 (2000-08-03) [umoeller]
 *@@changed V0.9.7 (2000-12-04) [umoeller]: now preventing sticky windows from switching
 *@@changed V0.9.9 (2001-03-14) [lafaix]: now preventing erratic mouse screen switch
 *@@changed V0.9.18 (2002-02-19) [lafaix]: now using G_szlXPagerClient
 *@@changed V0.9.19 (2002-04-11) [lafaix]: context menu no longer requires fMiniWinMouseActions
 *@@changed V0.9.19 (2002-05-07) [umoeller]: rewritten
 */

MRESULT EXPENTRY fnwpMoveThread(HWND hwndObject, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT     mrc = 0;

    TRY_LOUD(excpt1)
    {
        switch (msg)
        {
            /*
             *@@ PGRM_MOVEBYDELTA:
             *      posting this message moves the current
             *      desktop across the pane of all virtual
             *      desktops.
             *
             *      NEVER SEND THIS MESSAGE. This can deadlock
             *      the system.
             *
             *      Parameters:
             *
             *      --  LONG mp1: X delta. Positive values move
             *          the current desktop to the left, negative
             *          to the right.
             *
             *      --  LONG mp1: Y delta. Positive values move
             *          the current desktop down, negative up.
             *
             *@@added V0.9.19 (2002-05-07) [umoeller]
             */

            case PGRM_MOVEBYDELTA:
            {
                if (mp1 || mp2)
                {
                    // we got something to do:
                    MoveCurrentDesktop(WinQueryAnchorBlock(hwndObject),
                                       (LONG)mp1,
                                       (LONG)mp2);

                    // mouse switching is now possible again
                    G_pHookData->fProcessingWraparound = FALSE;
                }
            }
            break;

            default:
                mrc = G_pfnwpStaticOriginal(hwndObject, msg, mp1, mp2);

        } // end switch (msg)
    }
    CATCH(excpt1) {} END_CATCH();

    return mrc;
}

/*
 *@@ fntMoveThread:
 *      this thread gets started when XPager
 *      is started (dmnStartXPager in xwpdaemn.c).
 *      It creates an object window which receives
 *      notifications mostly from fnwpXPagerClient.
 *
 *      This thread is responsible for moving
 *      windows around and doing other stuff which
 *      should be done synchronusly.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 *@@changed V0.9.3 (2000-04-10) [umoeller]: renamed from fntMoveThread
 *@@changed V0.9.4 (2000-07-10) [umoeller]: fixed flashing; now using a win timer
 *@@changed V0.9.7 (2001-01-20) [umoeller]: now using higher priority
 *@@changed V0.9.19 (2002-05-07) [umoeller]: rewritten
 */

VOID _Optlink fntMoveThread(PTHREADINFO pti)
{
    // give ourselves higher priority...
    // otherwise we can't compete with Netscape and Win-OS/2 windows.
    DosSetPriority(PRTYS_THREAD,
                   PRTYC_REGULAR,
                   PRTYD_MAXIMUM,
                   0);      // current thread

    if (    (G_pHookData->hwndPagerMoveThread = WinCreateWindow(HWND_OBJECT,
                                                                 WC_STATIC,
                                                                 "",
                                                                 0,
                                                                 0, 0, 0, 0,
                                                                 0,
                                                                 HWND_BOTTOM,
                                                                 0,
                                                                 NULL,
                                                                 NULL))
         && (G_pfnwpStaticOriginal = WinSubclassWindow(G_pHookData->hwndPagerMoveThread,
                                                       fnwpMoveThread))
       )
    {
        QMSG qmsg;
        while (WinGetMsg(pti->hab, &qmsg, NULLHANDLE, 0, 0))
            WinDispatchMsg(pti->hab, &qmsg);
    }

    // destroy the object window V0.9.12 (2001-05-12) [umoeller]
    WinDestroyWindow(G_pHookData->hwndPagerMoveThread);
    G_pHookData->hwndPagerMoveThread = NULLHANDLE;
}


