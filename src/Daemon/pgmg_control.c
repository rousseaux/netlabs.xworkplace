
/*
 *@@sourcefile pgmg_control.c:
 *      PageMage Desktop control window.
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

#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_WIN
#define INCL_GPI
#include <os2.h>

#include <stdio.h>
#include <time.h>
#include <setjmp.h>

#include "setup.h"                      // code generation and debugging options

#include "helpers\except.h"             // exception handling
#include "helpers\gpih.h"               // GPI helper routines

#include "hook\xwphook.h"
#include "hook\hook_private.h"
#include "hook\xwpdaemn.h"              // PageMage and daemon declarations

/* ******************************************************************
 *                                                                  *
 *   Global variables                                               *
 *                                                                  *
 ********************************************************************/

#define WNDCLASS_PAGEMAGECLIENT         "XWPPageMageClient"

PFNWP       G_pfnOldFrameWndProc = 0;
BOOL        G_ClientBitmapNeedsUpdate = FALSE;
ULONG       G_hUpdateTimer = NULLHANDLE;

/* ******************************************************************
 *                                                                  *
 *   Debugging                                                      *
 *                                                                  *
 ********************************************************************/

/*
 *@@ DumpAllWindows:
 *
 */

VOID DumpAllWindows(VOID)
{
    USHORT  usIdx;
    if (DosRequestMutexSem(G_hmtxWindowList, TIMEOUT_PGMG_WINLIST)
            == NO_ERROR)
    {
        for (usIdx = 0; usIdx < G_usWindowCount; usIdx++)
        {
            _Pmpf(("Dump %d: hwnd 0x%lX \"%s\":\"%s\" pid 0x%lX(%d) type %d",
                   usIdx,
                   G_MainWindowList[usIdx].hwnd,
                   G_MainWindowList[usIdx].szSwitchName,
                   G_MainWindowList[usIdx].szClassName,
                   G_MainWindowList[usIdx].pid,
                   G_MainWindowList[usIdx].pid,
                   G_MainWindowList[usIdx].bWindowType));
        }

        DosReleaseMutexSem(G_hmtxWindowList);
    }
}

/* ******************************************************************
 *                                                                  *
 *   PageMage control window                                        *
 *                                                                  *
 ********************************************************************/

/*
 *@@ pgmcCreateMainControlWnd:
 *      creates the PageMage control window (frame
 *      and client). This gets called by dmnStartPageMage
 *      on thread 1 when PageMage has been enabled.
 *
 *      This registers the PageMage client class,
 *      using fnwpPageMageClient which does the grunt
 *      work for PageMage.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 *@@changed V0.9.3 (2000-05-22) [umoeller]: the hook was interfering, fixed
 */

BOOL pgmcCreateMainControlWnd(VOID)
{
    BOOL    brc = TRUE;

    G_pHookData->fDisableSwitching = TRUE;

    _Pmpf(("Entering pgmcCreateMainControlWnd"));
    // now go for PageMage client
    WinRegisterClass(G_habDaemon,
                     WNDCLASS_PAGEMAGECLIENT,
                     (PFNWP)fnwpPageMageClient,
                     0,
                     0);

    if (G_pHookData->hwndPageMageFrame == NULLHANDLE)
    {
        ULONG   flFrameFlags    = FCF_TITLEBAR |
                                  FCF_SYSMENU |
                                  FCF_HIDEBUTTON |
                                  FCF_NOBYTEALIGN |
                                  FCF_SIZEBORDER;
        _Pmpf(("Creating pagemage"));

        G_pHookData->hwndPageMageFrame
            = WinCreateStdWindow(HWND_DESKTOP,
                                 (ULONG)0,  // style
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
            SWP     swpPager;

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
            WinQueryWindowPos(G_pHookData->hwndPageMageClient, &swpPager);
            G_ptlPgmgClientSize.x = swpPager.cx;
            G_ptlPgmgClientSize.y = swpPager.cy;

            G_ptlEachDesktop.x = (G_ptlPgmgClientSize.x - G_pHookData->PageMageConfig.ptlMaxDesktops.x + 1)
                                 / G_pHookData->PageMageConfig.ptlMaxDesktops.x;
            G_ptlEachDesktop.y = (G_ptlPgmgClientSize.y - G_pHookData->PageMageConfig.ptlMaxDesktops.y + 1)
                                 / G_pHookData->PageMageConfig.ptlMaxDesktops.y;

            brc = TRUE;
        }
    }

    G_pHookData->fDisableSwitching = FALSE;

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
    LONG lHeightPercentOfWidth = G_pHookData->lCYScreen * 100 / G_pHookData->lCXScreen;
                // e.g. 75 for 1024x768
    LONG lCY =      (  (cx * lHeightPercentOfWidth / 100)
                       * G_pHookData->PageMageConfig.ptlMaxDesktops.y
                       / G_pHookData->PageMageConfig.ptlMaxDesktops.x
                    );
    if (G_pHookData->PageMageConfig.fShowTitlebar)
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
    RECTL   rcl;
    ULONG   flOptions;
    ULONG   cb = sizeof(G_swpPgmgFrame);

    PrfQueryProfileData(HINI_USER,
                        INIAPP_XWPHOOK,
                        INIKEY_HOOK_PGMGWINPOS,
                        &G_swpPgmgFrame,
                        &cb);
            // might fail

    rcl.xLeft   = G_swpPgmgFrame.x;
    rcl.yBottom = G_swpPgmgFrame.y;
    rcl.xRight  = G_swpPgmgFrame.cx + G_swpPgmgFrame.x;
    rcl.yTop    = G_swpPgmgFrame.cy + G_swpPgmgFrame.y;

    flOptions = SWP_MOVE | SWP_SIZE
                    | SWP_SHOW; // V0.9.4 (2000-07-10) [umoeller]

    WinSetWindowPos(hwnd,
                    NULLHANDLE,
                    rcl.xLeft, rcl.yBottom,
                    (rcl.xRight - rcl.xLeft), (rcl.yTop - rcl.yBottom),
                    flOptions);

    if (G_pHookData->PageMageConfig.fFlash)
        pgmgcStartFlashTimer();
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

/*
 *@@ UpdateClientBitmap:
 *      gets called when fnwpPageMageClient
 *      receives WM_PAINT and the paint bitmap has been
 *      marked as outdated (G_ClientBitmapNeedsUpdate == TRUE).
 *
 *      This draws the new contents of the PageMage
 *      client into the bitmap in the specified memory PS.
 *      That bitmap is then simply bit-blitted on the
 *      screen on WM_PAINT in fnwpPageMageClient.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 *@@changed V0.9.3 (2000-05-22) [umoeller]: fixed font problems
 *@@changed V0.9.4 (2000-08-08) [umoeller]: added special maximized window handling
 */

VOID UpdateClientBitmap(HPS hpsMem)       // in: memory PS with bitmap
{
    POINTL          ptlDest;
    SWP             swpClient;

    USHORT          usIdx;
    SHORT           sIdx;
    HWND            hwndLocalActive;
    float           fScale_X,
                    fScale_Y;

    G_ptlEachDesktop.x = (G_ptlPgmgClientSize.x - G_pHookData->PageMageConfig.ptlMaxDesktops.x + 1)
                         / G_pHookData->PageMageConfig.ptlMaxDesktops.x;
    G_ptlEachDesktop.y = (G_ptlPgmgClientSize.y - G_pHookData->PageMageConfig.ptlMaxDesktops.y + 1)
                         / G_pHookData->PageMageConfig.ptlMaxDesktops.y;

    // draw main box - all Desktops
    GpiSetColor(hpsMem, G_pHookData->PageMageConfig.lcNormal);
    ptlDest.x = ptlDest.y = 0;
    GpiMove(hpsMem, &ptlDest);
    WinQueryWindowPos(G_pHookData->hwndPageMageClient, &swpClient);
    ptlDest.x = swpClient.cx;
    ptlDest.y = swpClient.cy;
    GpiBox(hpsMem, DRO_OUTLINEFILL, &ptlDest, (LONG) 0, (LONG) 0);

    // paint "Current" Desktop
    GpiSetColor(hpsMem, G_pHookData->PageMageConfig.lcCurrent);
    ptlDest.x = ( (float) G_ptlCurrPos.x / (float) G_pHookData->lCXScreen)
                * ((float) G_ptlEachDesktop.x + 1)
                + 1;
    ptlDest.y = (  (   (float) (G_pHookData->PageMageConfig.ptlMaxDesktops.y - 1)
                       * G_pHookData->lCYScreen
                       - G_ptlCurrPos.y
                   )
                  / (float) G_pHookData->lCYScreen)
                  * ((float) G_ptlEachDesktop.y + 1)
                  + 2;
    GpiMove(hpsMem, &ptlDest);
    ptlDest.x += G_ptlEachDesktop.x - 1;
    ptlDest.y += G_ptlEachDesktop.y - 1;
    GpiBox(hpsMem, DRO_FILL, &ptlDest, (LONG) 0, (LONG) 0);

    // draw vertical lines
    GpiSetColor(hpsMem, G_pHookData->PageMageConfig.lcDivider);
    for (usIdx = 0;
         usIdx < G_pHookData->PageMageConfig.ptlMaxDesktops.x - 1;
         usIdx++)
    {
        ptlDest.x = (G_ptlEachDesktop.x + 1) * (usIdx + 1);
        ptlDest.y = 0;
        GpiMove(hpsMem, &ptlDest);
        ptlDest.x = (G_ptlEachDesktop.x + 1) * (usIdx + 1);
        ptlDest.y = G_ptlPgmgClientSize.y;
        GpiLine(hpsMem, &ptlDest);
    }

    // draw horizontal lines
    for (usIdx = 0;
         usIdx < G_pHookData->PageMageConfig.ptlMaxDesktops.y - 1;
         usIdx++)
    {
        ptlDest.x = 0;
        ptlDest.y = G_ptlPgmgClientSize.y
                    - (usIdx + 1)
                    * (G_ptlEachDesktop.y + 1);
        GpiMove(hpsMem, &ptlDest);
        ptlDest.x = G_ptlPgmgClientSize.x;
        ptlDest.y = G_ptlPgmgClientSize.y
                    - (usIdx + 1)
                    * (G_ptlEachDesktop.y + 1);
        GpiLine(hpsMem, &ptlDest);
    }

    // paint boxes - visible, standard windows
    if (G_pHookData->PageMageConfig.fMirrorWindows)
    {
        HENUM           henum;
        HWND            hwndThis;
        RECTL           rclText;
        BOOL            fNeedRescan = FALSE;

        HWND            hwndPaint[MAX_WINDOWS] = {0};
                // array of windows to be painted; this is filled here
        POINTL          ptlBegin[MAX_WINDOWS],
                        ptlFin[MAX_WINDOWS];
        USHORT          usPaintCount = 0;

        // set font to use; this font has been created on startup
        GpiSetCharSet(hpsMem, LCID_PAGEMAGE_FONT);

        usIdx = 0;
        fScale_X = (float) ( G_pHookData->PageMageConfig.ptlMaxDesktops.x
                             * G_pHookData->lCXScreen
                           ) / G_ptlPgmgClientSize.x;
        fScale_Y = (float) ( G_pHookData->PageMageConfig.ptlMaxDesktops.y
                             * G_pHookData->lCYScreen
                           ) / G_ptlPgmgClientSize.y;

        hwndLocalActive = WinQueryActiveWindow(HWND_DESKTOP);

        // enumerate Desktop windows; this is needed
        // because our internal list doesn't have the
        // Z-order right
        henum = WinBeginEnumWindows(HWND_DESKTOP);
        while ((hwndThis = WinGetNextWindow(henum)) != NULLHANDLE)
        {
            BOOL            fPaint = TRUE;

            if (DosRequestMutexSem(G_hmtxWindowList, TIMEOUT_PGMG_WINLIST)
                    == NO_ERROR)
            {
                // go thru local list and find the
                // current enumeration window
                for (usIdx = 0;
                     usIdx < G_usWindowCount;
                     usIdx++)
                {
                    if (G_MainWindowList[usIdx].hwnd == hwndThis)
                    {
                        SWP  swpBox;
                        WinQueryWindowPos(G_MainWindowList[usIdx].hwnd, &swpBox);
                        // update window pos in window list
                        WinQueryWindowPos(G_MainWindowList[usIdx].hwnd,
                                          &G_MainWindowList[usIdx].swp);

                        fPaint = TRUE;

                        if (G_MainWindowList[usIdx].bWindowType == WINDOW_RESCAN)
                        {
                            // window is not in switchlist:
                            // rescan
                            if (!pgmwGetWinInfo(G_MainWindowList[usIdx].hwnd,
                                                &G_MainWindowList[usIdx]))
                                fNeedRescan = TRUE;
                        }
                        else
                        {
                            // paint window only if it's "normal" and visible
                            if (    (G_MainWindowList[usIdx].bWindowType != WINDOW_NORMAL)
                                 && (G_MainWindowList[usIdx].bWindowType != WINDOW_MAXIMIZE)
                               )
                                // window not visible or special window:
                                // don't paint
                                fPaint = FALSE;
                        }

                        if (G_MainWindowList[usIdx].bWindowType == WINDOW_MAX_OFF)
                            fPaint = TRUE;
                        else if (!WinIsWindowVisible(G_MainWindowList[usIdx].hwnd)) // (G_MainWindowList[usIdx].swp.fl & SWP_HIDE) //
                            fPaint = FALSE;

                        /* _Pmpf(("Checked 0x%lX, paint: %d",
                                G_MainWindowList[usIdx].hwnd,
                                fPaint)); */
                        if (fPaint)
                        {
                            hwndPaint[usPaintCount] = G_MainWindowList[usIdx].hwnd;

                            ptlBegin[usPaintCount].x = (swpBox.x + G_ptlCurrPos.x)
                                                       / fScale_X;
                            ptlBegin[usPaintCount].y = (  (G_pHookData->PageMageConfig.ptlMaxDesktops.y - 1)
                                                         * G_pHookData->lCYScreen
                                                         - G_ptlCurrPos.y + swpBox.y
                                                        )
                                                        / fScale_Y;
                            ptlFin[usPaintCount].x = (swpBox.x + swpBox.cx + G_ptlCurrPos.x)
                                                      / fScale_X - 1;
                            ptlFin[usPaintCount].y = (    (G_pHookData->PageMageConfig.ptlMaxDesktops.y - 1)
                                                          * G_pHookData->lCYScreen
                                                          - G_ptlCurrPos.y + swpBox.y + swpBox.cy
                                                       )
                                                       / fScale_Y - 1;
                            usPaintCount++;
                        }
                    }
                }

                // done collecting windows, we can release
                // the list now
                DosReleaseMutexSem(G_hmtxWindowList);
            }
        } // end while WinGetNextWindow()
        WinEndEnumWindows(henum);

        // now paint

        sIdx = usPaintCount - 1;
        // _Pmpf(("Painting %d windows", usPaintCount));
        while (sIdx >= 0)
        {
            if (hwndPaint[sIdx] == hwndLocalActive)
                GpiSetColor(hpsMem, G_pHookData->PageMageConfig.lcCurrentApp);
            else
                GpiSetColor(hpsMem, G_pHookData->PageMageConfig.lcNormalApp);

            GpiMove(hpsMem, &(ptlBegin[sIdx]));
            GpiBox(hpsMem, DRO_FILL, &(ptlFin[sIdx]), (LONG) 0, (LONG) 0);
            GpiSetColor(hpsMem, G_pHookData->PageMageConfig.lcAppBorder);
            GpiBox(hpsMem, DRO_OUTLINE, &(ptlFin[sIdx]), (LONG) 0, (LONG) 0);
            rclText.xLeft = ptlBegin[sIdx].x + 1;
            rclText.yBottom = ptlBegin[sIdx].y + 1;
            rclText.xRight = ptlFin[sIdx].x - 1;
            rclText.yTop = ptlFin[sIdx].y - 4;

            // draw window text too?
            if (G_pHookData->PageMageConfig.fShowWindowText)
            {
                PSZ     pszWindowName = NULL;
                // CHAR        szWindowName[PGMG_TEXTLEN] = "";
                LONG    lTextColor;

                if (DosRequestMutexSem(G_hmtxWindowList, TIMEOUT_PGMG_WINLIST)
                        == NO_ERROR)
                {
                    for (usIdx = 0; usIdx < G_usWindowCount; usIdx++)
                        if (G_MainWindowList[usIdx].hwnd == hwndPaint[sIdx])
                        {
                            if (G_MainWindowList[usIdx].szSwitchName[0] != 0)
                                pszWindowName = G_MainWindowList[usIdx].szSwitchName;
                                /* strcpy(szWindowName, G_MainWindowList[usIdx].szSwitchName);
                            else
                                strcpy(szWindowName, G_MainWindowList[usIdx].szWindowName); */
                            // bFound = TRUE;
                            break;
                        }
                    DosReleaseMutexSem(G_hmtxWindowList);
                }

                if (pszWindowName)
                {
                    if (hwndPaint[sIdx] == hwndLocalActive)
                        // active:
                        lTextColor = G_pHookData->PageMageConfig.lcTxtCurrentApp;
                    else
                        lTextColor = G_pHookData->PageMageConfig.lcTxtNormalApp;

                    WinDrawText(hpsMem,
                                strlen(pszWindowName),
                                pszWindowName,
                                &rclText,
                                lTextColor,
                                (LONG)0,
                                DT_LEFT | DT_TOP /* | DT_WORDBREAK */);
                }
            }
            sIdx--;
        }

        // unset font so we can delete it if it changes
        GpiSetCharSet(hpsMem, LCID_DEFAULT);
        // GpiDeleteSetId(hps, LCID_FONT); */

        if (fNeedRescan)
        {
            // _Pmpf(("UpdateClientBitmap: posting rescan"));
            WinPostMsg(G_pHookData->hwndPageMageClient,
                       PGMG_WNDRESCAN,
                       MPVOID,
                       MPVOID);
        }
    } // bShowWindows
} // PaintProc

/*
 *@@ TrackWithinPager:
 *
 *@@added V0.9.2 (2000-02-23) [umoeller]
 *@@changed V0.9.6 (2000-11-06) [umoeller]: disabled dragging of WPS desktop
 */

VOID TrackWithinPager(HWND hwnd,
                      MPARAM mp1)
{
    HWND        hwndTracked;
    SWP         swpTracked;

    LONG        lMouseX = SHORT1FROMMP(mp1),
                lMouseY = SHORT2FROMMP(mp1);

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
        fScale_X = (float) (G_pHookData->PageMageConfig.ptlMaxDesktops.x * G_pHookData->lCXScreen)
                           / G_ptlPgmgClientSize.x;
        fScale_Y = (float) (G_pHookData->PageMageConfig.ptlMaxDesktops.y * G_pHookData->lCYScreen)
                           / G_ptlPgmgClientSize.y;
        ti.rclTrack.xLeft = (swpTracked.x + G_ptlCurrPos.x) / fScale_X;
        ti.rclTrack.yBottom = ( (G_pHookData->PageMageConfig.ptlMaxDesktops.y - 1)
                              * G_pHookData->lCYScreen
                              - G_ptlCurrPos.y + swpTracked.y
                              )
                              / fScale_Y;
        ti.rclTrack.xRight = (swpTracked.x + swpTracked.cx + G_ptlCurrPos.x)
                             / fScale_X - 1;
        ti.rclTrack.yTop = (    (G_pHookData->PageMageConfig.ptlMaxDesktops.y - 1)
                                * G_pHookData->lCYScreen
                                - G_ptlCurrPos.y
                                + swpTracked.y
                                + swpTracked.cy
                           )
                           / fScale_Y - 1;
        ti.rclBoundary.xLeft = 0;
        ti.rclBoundary.yBottom = 0;
        ti.rclBoundary.xRight = G_ptlPgmgClientSize.x;
        ti.rclBoundary.yTop = G_ptlPgmgClientSize.y;
        ti.ptlMinTrackSize.x = 2;
        ti.ptlMinTrackSize.y = 2;
        ti.ptlMaxTrackSize.x = G_ptlPgmgClientSize.x;
        ti.ptlMaxTrackSize.y = G_ptlPgmgClientSize.y;
        ti.fs = TF_STANDARD | TF_MOVE | TF_SETPOINTERPOS | TF_ALLINBOUNDARY;

        G_pHookData->fDisableSwitching = TRUE;
        if (WinTrackRect(hwnd, NULLHANDLE, &ti))
        {
            swpTracked.x = (ti.rclTrack.xLeft * fScale_X) - G_ptlCurrPos.x;
            swpTracked.y = (ti.rclTrack.yBottom * fScale_Y)
                           -  (   (G_pHookData->PageMageConfig.ptlMaxDesktops.y - 1)
                                  * G_pHookData->lCYScreen
                                  - G_ptlCurrPos.y
                              );
            swpTracked.x -= swpTracked.x % 16 +
                            WinQuerySysValue(HWND_DESKTOP, SV_CXDLGFRAME);

            WinSetWindowPos(hwndTracked, HWND_TOP, swpTracked.x, swpTracked.y,
                            0, 0,
                            SWP_MOVE | SWP_NOADJUST);

            WinPostMsg(hwnd, PGMG_INVALIDATECLIENT,
                       (MPARAM)TRUE,        // immediately
                       0);
                    // V0.9.2 (2000-02-22) [umoeller]
        }
        G_pHookData->fDisableSwitching = FALSE;
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
 */

MRESULT EXPENTRY fnwpPageMageClient(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    static HWND                 hwndSaveSysMenu,
                                hwndSaveTitlebar,
                                hwndSaveMinButton;
    static BITMAPINFOHEADER2    bmihMem;
    static HDC                  hdcMem;
    static HPS                  hpsMem;
    static HBITMAP              hbmMem = 0;

    MRESULT                     mReturn  = 0;
    BOOL                        bHandled = TRUE;

    TRY_LOUD(excpt1, NULL)
    {
        switch (msg)
        {
            /*
             * WM_CREATE:
             *
             */

            case WM_CREATE:
            {
                SIZEL       slMem;
                LONG        lBitmapFormat[2];

                G_ptlCurrPos.x = (G_pHookData->PageMageConfig.ptlStartDesktop.x - 1)
                                  * G_pHookData->lCXScreen;
                G_ptlCurrPos.y = (G_pHookData->PageMageConfig.ptlMaxDesktops.y
                                  - G_pHookData->PageMageConfig.ptlStartDesktop.y
                                 ) * G_pHookData->lCYScreen;

                WinPostMsg(hwnd,
                           PGMG_ZAPPO,
                           // frame initially has title bar; so if this is
                           // disabled, we need to hide it
                           (MPARAM)(!G_pHookData->PageMageConfig.fShowTitlebar),
                           MPVOID);

                // create memory DC and PS
                hdcMem = DevOpenDC(G_habDaemon, OD_MEMORY, "*", (LONG) 0, NULL, (LONG) 0);
                slMem.cx = 0;
                slMem.cy = 0;
                hpsMem = GpiCreatePS(G_habDaemon, hdcMem, &slMem, GPIA_ASSOC | PU_PELS);
                gpihSwitchToRGB(hpsMem);        // V0.9.3 (2000-04-09) [umoeller]

                // prepare bitmap creation
                GpiQueryDeviceBitmapFormats(hpsMem, (LONG) 2, lBitmapFormat);
                memset(&bmihMem, 0, sizeof(BITMAPINFOHEADER2));
                bmihMem.cbFix = sizeof(BITMAPINFOHEADER2);
                bmihMem.cx = G_ptlPgmgClientSize.x;
                bmihMem.cy = G_ptlPgmgClientSize.y;
                bmihMem.cPlanes = lBitmapFormat[0];
                bmihMem.cBitCount = lBitmapFormat[1];

                // set font; this creates a logical font in WM_PRESPARAMSCHANGED (below)
                WinSetPresParam(hwnd, PP_FONTNAMESIZE,
                                strlen(G_szFacename) + 1, G_szFacename);

                G_ClientBitmapNeedsUpdate = TRUE;
            break; }

            /*
             * WM_PAINT:
             *
             */

            case WM_PAINT:
            {
                POINTL      ptlCopy[4];
                HPS         hps = WinBeginPaint(hwnd, 0, 0);
                gpihSwitchToRGB(hps);   // V0.9.3 (2000-04-09) [umoeller]
                if (G_ClientBitmapNeedsUpdate)
                {
                    // bitmap has been marked as invalid:
                    UpdateClientBitmap(hpsMem);
                    G_ClientBitmapNeedsUpdate = FALSE;
                }

                // now just bit-blt the bitmap
                ptlCopy[0].x = 0;
                ptlCopy[0].y = 0;
                ptlCopy[1].x = G_ptlPgmgClientSize.x;
                ptlCopy[1].y = G_ptlPgmgClientSize.y;
                ptlCopy[2].x = 0;
                ptlCopy[2].y = 0;
                GpiBitBlt(hps, hpsMem, (LONG) 3, ptlCopy, ROP_SRCCOPY, BBO_IGNORE);
                WinEndPaint(hps);
            break; }

            /*
             * WM_ERASEBACKGROUND:
             *
             */

            case WM_ERASEBACKGROUND:
                mReturn = MRFROMLONG(FALSE);
                    // pretend we've processed this;
                    // we paint the entire bitmap in WM_PAINT anyway
                break;

            /*
             * WM_PRESPARAMCHANGED:
             *
             */

            case WM_PRESPARAMCHANGED:
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
                    if (strcmp(G_szFacename, szLocFacename))
                    {
                        G_bConfigChanged = TRUE;
                        strcpy(G_szFacename, szLocFacename);
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
                    GpiCreateLogFont(hpsMem, NULL, LCID_PAGEMAGE_FONT, &FontAttrs);

                    WinPostMsg(hwnd, PGMG_INVALIDATECLIENT,
                               (MPARAM)TRUE,        // immediately
                               0);
                }
                break;

            /*
             * WM_SIZE:
             *
             */

            case WM_SIZE:
            {
                SHORT cxNew = SHORT1FROMMP(mp2),
                      cyNew = SHORT2FROMMP(mp2);

                if (    (G_ptlPgmgClientSize.x != cxNew)
                     || (G_ptlPgmgClientSize.y != cyNew)
                   )
                {
                    G_ptlPgmgClientSize.x = cxNew;
                    G_ptlPgmgClientSize.y = cyNew;
                    bmihMem.cx = G_ptlPgmgClientSize.x;
                    bmihMem.cy = G_ptlPgmgClientSize.y;
                    G_ptlEachDesktop.x = (G_ptlPgmgClientSize.x - G_pHookData->PageMageConfig.ptlMaxDesktops.x + 1)
                                         / G_pHookData->PageMageConfig.ptlMaxDesktops.x;
                    G_ptlEachDesktop.y = (G_ptlPgmgClientSize.y - G_pHookData->PageMageConfig.ptlMaxDesktops.y + 1)
                                         / G_pHookData->PageMageConfig.ptlMaxDesktops.y;

                    if (hbmMem)
                    {
                        GpiSetBitmap(hpsMem, NULLHANDLE);
                            // free existing; otherwise GpiDeleteBitmap fails
                            // V0.9.2 (2000-02-22) [umoeller]
                        GpiDeleteBitmap(hbmMem);
                    }
                    hbmMem = GpiCreateBitmap(hpsMem, &bmihMem, (LONG) 0, NULL, NULL);
                    GpiSetBitmap(hpsMem, hbmMem);
                    WinPostMsg(hwnd, PGMG_INVALIDATECLIENT,
                               (MPARAM)TRUE,        // immediately
                               0);
                }
            break; }    // WM_SIZE

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
            {
                ULONG       ulRequest;
                ULONG       ulMsg = PGOM_CLICK2ACTIVATE;
                LONG        lMouseX = SHORT1FROMMP(mp1),
                            lMouseY = SHORT2FROMMP(mp1);

                // make sure the hook does not try to move stuff around...
                G_pHookData->fDisableSwitching = TRUE;
                if (msg == WM_BUTTON2CLICK)
                    ulMsg = PGOM_CLICK2LOWER;

                WinPostMsg(G_pHookData->hwndPageMageMoveThread,
                           ulMsg,
                           (MPARAM)lMouseX,
                           (MPARAM)lMouseY);
                    // this posts PGMG_CHANGEACTIVE back to us

                // reset the hook to its proper value
                G_pHookData->fDisableSwitching = FALSE;

                if (G_pHookData->PageMageConfig.fFlash)
                    pgmgcStartFlashTimer();
            break; }

            case WM_BUTTON2DBLCLK:
                DumpAllWindows();
            break;

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

                TrackWithinPager(hwnd, mp1);

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
                GpiDeleteBitmap(hbmMem);
                GpiDestroyPS(hpsMem);
                DevCloseDC(hdcMem);
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
                G_ClientBitmapNeedsUpdate = TRUE;
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

                    case 2:     // flash timer started by fntMoveQueueThread
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
                if (!G_pHookData->PageMageConfig.fShowTitlebar)
                {
                    // titlebar disabled:
                    hwndSaveSysMenu = WinWindowFromID(G_pHookData->hwndPageMageFrame,
                                                      FID_SYSMENU);
                    hwndSaveTitlebar = WinWindowFromID(G_pHookData->hwndPageMageFrame,
                                                       FID_TITLEBAR);
                    hwndSaveMinButton = WinWindowFromID(G_pHookData->hwndPageMageFrame,
                                                        FID_MINMAX);
                    WinSetParent(hwndSaveSysMenu, HWND_OBJECT,
                                 TRUE);     // redraw
                    WinSetParent(hwndSaveTitlebar, HWND_OBJECT, TRUE);
                    WinSetParent(hwndSaveMinButton, HWND_OBJECT, TRUE);
                    WinSendMsg(G_pHookData->hwndPageMageFrame,
                               WM_UPDATEFRAME,
                               MPFROMLONG(FCF_SIZEBORDER),
                               MPVOID);
                }
                else
                {
                    // titlebar enabled:
                    WinSetParent(hwndSaveSysMenu, G_pHookData->hwndPageMageFrame, TRUE);
                    WinSetParent(hwndSaveTitlebar, G_pHookData->hwndPageMageFrame, TRUE);
                    WinSetParent(hwndSaveMinButton, G_pHookData->hwndPageMageFrame, TRUE);
                    WinSendMsg(G_pHookData->hwndPageMageFrame,
                               WM_UPDATEFRAME,
                               MPFROMLONG(FCF_SIZEBORDER | FCF_SYSMENU |
                                          FCF_HIDEBUTTON | FCF_TITLEBAR),
                               MPVOID);
                }
            break;

            /*
             *@@ PGMG_LOCKUP:
             *      posted by hookLockupHook when the system is
             *      locked up (with mp1 == TRUE) or by hookSendMsgHook
             *      when the lockup window is destroyed.
             *
             *      HookData.hwndLockupFrame has the lockup frame window
             *      created by PM.
             */

            case PGMG_LOCKUP:
                // G_bLockup = LONGFROMMP(mp1);
                break;

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
                        pgmwWindowListAdd(hwndChanged);
                        break;
                    case WM_DESTROY:
                        pgmwWindowListDelete(hwndChanged);
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
             *      posted only by fntMoveQueueThread when
             *      PGMGQ_CLICK2ACTIVATE was retrieved from
             *      the "move" queue. This happens as a
             *      response to mb1 or mb2 clicks.
             *
             *      Parameters: new active HWND in mp1.
             */

            case PGMG_CHANGEACTIVE:
            {
                HWND    hwnd2Activate = HWNDFROMMP(mp1);
                G_pHookData->fDisableSwitching = TRUE;
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
                G_pHookData->fDisableSwitching = FALSE;
            break; }

            /*
             *@@ PGMG_LOWERWINDOW:
             *      posted only by fntMoveQueueThread when
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
        mReturn = WinDefWindowProc(hwnd, msg, mp1, mp2);

    return (mReturn);
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

            mrc = G_pfnOldFrameWndProc(hwnd, msg, mp1, mp2);
        break; }

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


