
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
#include <stdlib.h>
#include <string.h>
#include <process.h>

#include "setup.h"                      // code generation and debugging options

#include "helpers\gpih.h"               // GPI helper routines

#include "hook\xwphook.h"
#include "hook\hook_private.h"
#include "hook\xwpdaemn.h"              // PageMage and daemon declarations

/* ******************************************************************
 *                                                                  *
 *   Global variables                                               *
 *                                                                  *
 ********************************************************************/

const char  *WNDCLASS_PAGEMAGECLIENT = "XWPPageMageClient";
SWP         G_swpFrameOriginal = {0};
FATTRS      G_fat = {0};
BOOL        G_ClientBitmapNeedsUpdate = FALSE;
ULONG       G_hUpdateTimer = NULLHANDLE;

/* ******************************************************************
 *                                                                  *
 *   PageMage control window                                        *
 *                                                                  *
 ********************************************************************/

/*
 *@@ pgmcCreateMainControlWnd:
 *      creates the PageMage control window (frame
 *      and client). This gets called by dmnStartPageMage
 *      when PageMage has been enabled.
 *
 *      This registers the PageMage client class,
 *      using fnwpPageMageClient which does the grunt
 *      work for PageMage.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 */

BOOL pgmcCreateMainControlWnd(VOID)
{
    _Pmpf(("pgmcCreateMainControlWnd, G_pHookData->hwndPageMageFrame: 0x%lX",
            G_pHookData->hwndPageMageFrame));
    // now go for PageMage client
    WinRegisterClass(G_habDaemon,
                     (PSZ)WNDCLASS_PAGEMAGECLIENT,
                     (PFNWP)fnwpPageMageClient,
                     0,
                     0);

    if (G_pHookData->hwndPageMageFrame == NULLHANDLE)
    {
        ULONG   flFrameFlags    = FCF_TITLEBAR |
                                  FCF_SYSMENU |
                                  FCF_MINBUTTON |
                                // | FCF_TASKLIST
                                // | FCF_ICON
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

            /* if (iConfigResult == INI_CREATED)
              WinPostMsg(hwndClient, WM_COMMAND,
                         MPFROMSHORT(PGMG_CMD_NOTEBOOK), MPVOID); */
            return (TRUE);
        }
    }

    return (FALSE);
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
 */

VOID pgmcSetPgmgFramePos(HWND hwnd)
{
    RECTL   rcl;
    ULONG   flOptions;

    rcl.xLeft   = G_swpPgmgFrame.x;
    rcl.yBottom = G_swpPgmgFrame.y;
    rcl.xRight  = G_swpPgmgFrame.cx + G_swpPgmgFrame.x;
    rcl.yTop    = G_swpPgmgFrame.cy + G_swpPgmgFrame.y;

    flOptions = SWP_MOVE | SWP_SIZE;

    if (!G_pHookData->PageMageConfig.fFlash)
        flOptions |= SWP_SHOW;

    /* if (G_pHookData->PageMageConfig.fStartMinimized)
        flOptions |= SWP_MINIMIZE; */ // ###

    WinSetWindowPos(hwnd, NULLHANDLE, rcl.xLeft, rcl.yBottom,
                    (rcl.xRight - rcl.xLeft), (rcl.yTop - rcl.yBottom),
                    flOptions);
} // pgmcSetPgmgFramePos

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
 */

VOID UpdateClientBitmap(HPS hps)       // in: memory PS with bitmap
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

    // Draw main box - all Desktops
    GpiSetColor(hps, G_pHookData->PageMageConfig.lcNormal);
    ptlDest.x = ptlDest.y = 0;
    GpiMove(hps, &ptlDest);
    WinQueryWindowPos(G_pHookData->hwndPageMageClient, &swpClient);
    ptlDest.x = swpClient.cx;
    ptlDest.y = swpClient.cy;
    GpiBox(hps, DRO_OUTLINEFILL, &ptlDest, (LONG) 0, (LONG) 0);

    // Paint "Current" Desktop
    GpiSetColor(hps, G_pHookData->PageMageConfig.lcCurrent);
    ptlDest.x = (   (float) G_ptlCurrPos.x / (float) G_pHookData->lCXScreen)
                  * ((float) G_ptlEachDesktop.x + 1)
                  + 1;
    ptlDest.y = (   ((float) (G_pHookData->PageMageConfig.ptlMaxDesktops.y - 1)
                  * G_pHookData->lCYScreen - G_ptlCurrPos.y)
                  / (float) G_pHookData->lCYScreen)
                  * ((float) G_ptlEachDesktop.y + 1)
                  + 2;
    GpiMove(hps, &ptlDest);
    ptlDest.x += G_ptlEachDesktop.x - 1;
    ptlDest.y += G_ptlEachDesktop.y - 1;
    GpiBox(hps, DRO_FILL, &ptlDest, (LONG) 0, (LONG) 0);

    // Draw vertical lines
    GpiSetColor(hps, G_pHookData->PageMageConfig.lcDivider);
    for (usIdx = 0; usIdx < G_pHookData->PageMageConfig.ptlMaxDesktops.x - 1; usIdx++)
    {
        ptlDest.x = (G_ptlEachDesktop.x + 1) * (usIdx + 1);
        ptlDest.y = 0;
        GpiMove(hps, &ptlDest);
        ptlDest.x = (G_ptlEachDesktop.x + 1) * (usIdx + 1);
        ptlDest.y = G_ptlPgmgClientSize.y;
        GpiLine(hps, &ptlDest);
    }

    // Draw horizontal lines
    for (usIdx = 0; usIdx < G_pHookData->PageMageConfig.ptlMaxDesktops.y - 1; usIdx++)
    {
        ptlDest.x = 0;
        ptlDest.y = G_ptlPgmgClientSize.y
                    - (usIdx + 1)
                    * (G_ptlEachDesktop.y + 1);
        GpiMove(hps, &ptlDest);
        ptlDest.x = G_ptlPgmgClientSize.x;
        ptlDest.y = G_ptlPgmgClientSize.y
                    - (usIdx + 1)
                    * (G_ptlEachDesktop.y + 1);
        GpiLine(hps, &ptlDest);
    }

    // Paint boxes - visible, standard windows
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


        GpiCreateLogFont(hps, NULL, LCID_FONT, &G_fat);
        GpiSetCharSet(hps, LCID_FONT);
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

            DosRequestMutexSem(G_hmtxWindowList, SEM_INDEFINITE_WAIT);

            // go thru local list and find the
            // current enumeration window
            for (usIdx = 0;
                 usIdx < G_usWindowCount;
                 usIdx++)
            {
                if (G_MainWindowList[usIdx].hwnd == hwndThis)
                {
                    fPaint = TRUE;

                    // paint window only if it's "normal" and visible
                    if (G_MainWindowList[usIdx].bWindowType != WINDOW_NORMAL)
                        // window not visible or special window:
                        // don't paint
                        fPaint = FALSE;

                    if (G_MainWindowList[usIdx].bWindowType == WINDOW_RESCAN)
                    {
                        // window is not in switchlist:
                        // rescan
                        if (pgmwGetWinInfo(G_MainWindowList[usIdx].hwnd,
                                           &G_MainWindowList[usIdx]))
                            // window still valid:
                            fPaint = TRUE;
                        else
                            fNeedRescan = TRUE;
                    }

                    if (!WinIsWindowVisible(G_MainWindowList[usIdx].hwnd)) // (G_MainWindowList[usIdx].swp.fl & SWP_HIDE) //
                        fPaint = FALSE;

                    /* _Pmpf(("Checked 0x%lX, paint: %d",
                            G_MainWindowList[usIdx].hwnd,
                            fPaint)); */
                    if (fPaint)
                    {
                        SWP             swpBox;
                        WinQueryWindowPos(G_MainWindowList[usIdx].hwnd, &swpBox);
                        // #define swpBox G_MainWindowList[usIdx].swp
                        // update window pos in window list
                        WinQueryWindowPos(G_MainWindowList[usIdx].hwnd,
                                          &G_MainWindowList[usIdx].swp);

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

            // new window!  tell thread to rescan, get title, draw it anyways
            /* if (!fFound)
            {
                // TODO:  hmm.  need to fix something here
                continue;
                bNewWindows = TRUE;
                hwndPaint[usPaintCtr] = hwndBox;
            } */

        } // end while WinGetNextWindow()
        WinEndEnumWindows(henum);

        sIdx = usPaintCount - 1;
        // _Pmpf(("Painting %d windows", usPaintCount));
        while (sIdx >= 0)
        {
            if (hwndPaint[sIdx] == hwndLocalActive)
                GpiSetColor(hps, G_pHookData->PageMageConfig.lcCurrentApp);
            else
                GpiSetColor(hps, G_pHookData->PageMageConfig.lcNormalApp);

            GpiMove(hps, &(ptlBegin[sIdx]));
            GpiBox(hps, DRO_FILL, &(ptlFin[sIdx]), (LONG) 0, (LONG) 0);
            GpiSetColor(hps, G_pHookData->PageMageConfig.lcAppBorder);
            GpiBox(hps, DRO_OUTLINE, &(ptlFin[sIdx]), (LONG) 0, (LONG) 0);
            rclText.xLeft = ptlBegin[sIdx].x + 1;
            rclText.yBottom = ptlBegin[sIdx].y + 1;
            rclText.xRight = ptlFin[sIdx].x - 1;
            rclText.yTop = ptlFin[sIdx].y - 4;

            // draw window text too?
            if (G_pHookData->PageMageConfig.fShowWindowText)
            {
                CHAR        szWindowName[TEXTLEN] = "";
                LONG        lTextColor;

                // BOOL        bFound = FALSE;
                DosRequestMutexSem(G_hmtxWindowList, SEM_INDEFINITE_WAIT);
                for (usIdx = 0; usIdx < G_usWindowCount; usIdx++)
                    if (G_MainWindowList[usIdx].hwnd == hwndPaint[sIdx])
                    {
                        if (G_MainWindowList[usIdx].szSwitchName[0] != 0)
                            strcpy(szWindowName, G_MainWindowList[usIdx].szSwitchName);
                        else
                            strcpy(szWindowName, G_MainWindowList[usIdx].szWindowName);
                        // bFound = TRUE;
                        break;
                    }
                DosReleaseMutexSem(G_hmtxWindowList);

                if (hwndPaint[sIdx] == hwndLocalActive)
                    // active:
                    lTextColor = G_pHookData->PageMageConfig.lcTxtCurrentApp;
                else
                    lTextColor = G_pHookData->PageMageConfig.lcTxtNormalApp;

                WinDrawText(hps,
                            strlen(szWindowName),
                            szWindowName,
                            &rclText,
                            lTextColor,
                            (LONG)0,
                            DT_LEFT | DT_TOP /* | DT_WORDBREAK */);
            }
            sIdx--;
        }

        GpiSetCharSet(hps, LCID_DEFAULT);
        GpiDeleteSetId(hps, LCID_FONT);

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
 */

VOID TrackWithinPager(HWND hwnd,
                      MPARAM mp1)
{
    HWND        hwndTracked;
    SWP         swpTracked;

    LONG        lMouseX = SHORT1FROMMP(mp1),
                lMouseY = SHORT2FROMMP(mp1);

    hwndTracked = pgmwGetWindowFromClientPoint(lMouseX, lMouseY);

    if (hwndTracked != NULLHANDLE)
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
/*
            swpTracked.y -= swpTracked.y % 16 + 4 +
                            WinQuerySysValue(HWND_DESKTOP, SV_CYDLGFRAME);
*/
            WinSetWindowPos(hwndTracked, HWND_TOP, swpTracked.x, swpTracked.y,
                            0, 0,
                            SWP_MOVE | SWP_NOADJUST);
            // WinInvalidateRect(hwndTracked, NULL, TRUE);
            // WinUpdateWindow(WinWindowFromID(hwndTracked, FID_CLIENT));

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
    static BOOL                 bGetSize = FALSE;

    MRESULT                     mReturn  = 0;
    BOOL                        bHandled = TRUE;

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

            G_bTitlebarChange = !G_pHookData->PageMageConfig.fShowTitlebar;
            bGetSize = TRUE;
            WinPostMsg(hwnd, PGMG_ZAPPO, MPVOID, MPVOID);

            hdcMem = DevOpenDC(G_habDaemon, OD_MEMORY, "*", (LONG) 0, NULL, (LONG) 0);
            slMem.cx = 0;
            slMem.cy = 0;
            hpsMem = GpiCreatePS(G_habDaemon, hdcMem, &slMem, GPIA_ASSOC | PU_PELS);
            gpihSwitchToRGB(hpsMem);        // V0.9.3 (2000-04-09) [umoeller]
            GpiQueryDeviceBitmapFormats(hpsMem, (LONG) 2, lBitmapFormat);
            memset(&bmihMem, 0, sizeof(BITMAPINFOHEADER2));
            bmihMem.cbFix = sizeof(BITMAPINFOHEADER2);
            bmihMem.cx = G_ptlPgmgClientSize.x;
            bmihMem.cy = G_ptlPgmgClientSize.y;
            bmihMem.cPlanes = lBitmapFormat[0];
            bmihMem.cBitCount = lBitmapFormat[1];
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
                // we paint the entire bitmap in WM_PAINT anyways
            break;

        /*
         * WM_PRESPARAMCHANGED:
         *
         */

        case WM_PRESPARAMCHANGED:
            if (LONGFROMMP(mp1) == PP_FONTNAMESIZE)
            {
                HPS             hps;
                FONTMETRICS     fm;
                CHAR            szLocFacename[TEXTLEN];

                WinQueryPresParam(hwnd, PP_FONTNAMESIZE, 0, NULL,
                                  sizeof(szLocFacename), &szLocFacename,
                                  0);
                if (strcmp(G_szFacename, szLocFacename))
                {
                    G_bConfigChanged = TRUE;
                    strcpy(G_szFacename, szLocFacename);
                }

                hps = WinGetPS(hwnd);
                GpiQueryFontMetrics(hps, sizeof(FONTMETRICS), &fm);
                WinReleasePS(hps);

                G_fat.usRecordLength = sizeof(FATTRS);
                G_fat.fsSelection = fm.fsSelection;
                G_fat.lMatch = fm.lMatch;
                strcpy(G_fat.szFacename, fm.szFacename);
                G_fat.idRegistry = fm.idRegistry;
                G_fat.usCodePage = fm.usCodePage;
                G_fat.lMaxBaselineExt = fm.lMaxBaselineExt;
                G_fat.lAveCharWidth = fm.lAveCharWidth;

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
            ULONG       ulCode = PGMGQ_CLICK2ACTIVATE;
            LONG        lMouseX = SHORT1FROMMP(mp1),
                        lMouseY = SHORT2FROMMP(mp1);

            // make sure the hook does not try to move stuff around...
            G_pHookData->fDisableSwitching = TRUE;

            if (msg == WM_BUTTON2CLICK)
                ulCode = PGMGQ_CLICK2LOWER;

            ulRequest = PGMGQENCODE(ulCode, lMouseX, lMouseY);
            DosWriteQueue(G_hqPageMage, ulRequest, 0, NULL, 0);
                // this posts PGMG_CHANGEACTIVE back to us

            // reset the hook to its proper value
            G_pHookData->fDisableSwitching = FALSE;
        break; }

        /*
         * WM_BUTTON2MOTIONSTART:
         *      move the windows within the pager.
         */

        case WM_BUTTON2MOTIONSTART:
        {
            TrackWithinPager(hwnd, mp1);
        break; }

        /* case WM_BUTTON2MOTIONSTART:
            WinSendMsg(G_pHookData->hwndPageMageFrame,
                       WM_TRACKFRAME,
                       MPFROMSHORT(TF_MOVE),
                       0);
            break; */

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

        case WM_TIMER:
            WinStopTimer(G_habDaemon,
                         hwnd,
                         1);
            G_hUpdateTimer = NULLHANDLE;
            // invalidate the client rectangle;
            // since G_ClientBitmapNeedsUpdate is TRUE
            // now, this will recreate the bitmap in WM_PAINT
            WinInvalidateRect(hwnd, NULL, TRUE);
        break;

        /*
         *@@ PGMG_ZAPPO:
         *      apparently posted or sent when config has
         *      changed.
         */

        case PGMG_ZAPPO:
            if (G_bTitlebarChange)
                if (!G_pHookData->PageMageConfig.fShowTitlebar)
                {
                    // titlebar disabled:
                    hwndSaveSysMenu = WinWindowFromID(G_pHookData->hwndPageMageFrame,
                                                      FID_SYSMENU);
                    hwndSaveTitlebar = WinWindowFromID(G_pHookData->hwndPageMageFrame,
                                                       FID_TITLEBAR);
                    hwndSaveMinButton = WinWindowFromID(G_pHookData->hwndPageMageFrame,
                                                        FID_MINMAX);
                    WinSetParent(hwndSaveSysMenu, HWND_OBJECT, TRUE);
                    WinSetParent(hwndSaveTitlebar, HWND_OBJECT, TRUE);
                    WinSetParent(hwndSaveMinButton, HWND_OBJECT, TRUE);
                    WinSendMsg(G_pHookData->hwndPageMageFrame,
                               WM_UPDATEFRAME,
                               MPFROMLONG(FCF_TASKLIST | FCF_ICON | FCF_SIZEBORDER),
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
                               MPFROMLONG(FCF_TASKLIST | FCF_ICON |
                                          FCF_SIZEBORDER | FCF_SYSMENU |
                                          FCF_MINBUTTON | FCF_TITLEBAR),
                               MPVOID);
                }

            G_bTitlebarChange = FALSE;
            if (bGetSize)
            {
                WinQueryWindowPos(G_pHookData->hwndPageMageFrame, &G_swpFrameOriginal);
                bGetSize = FALSE;
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
        break; }

        case PGMG_PASSTHRU:
        {
            ULONG ulRequest = LONGFROMMP(mp1);
            DosWriteQueue(G_hqPageMage, ulRequest, 0, NULL, 0);
        break; }

        default:
            bHandled = FALSE;
            break;
    }

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
 */

MRESULT EXPENTRY fnwpSubclPageMageFrame(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    PTRACKINFO  ptr;

    switch (msg)
    {
        case WM_ADJUSTWINDOWPOS:
        {
            PSWP pswp = (PSWP)mp1;

            if (pswp->cx < 100)
                pswp->cx = 100;
            if (pswp->cy < 100)
                pswp->cy = 100;

            if (G_pHookData->PageMageConfig.fPreserveProportions)
                pswp->cy = pgmcCalcNewFrameCY(pswp->cx);
        break; }

        case WM_QUERYTRACKINFO:
            G_pfnOldFrameWndProc(hwnd, msg, mp1, mp2);
            ptr = (PTRACKINFO) PVOIDFROMMP(mp2);
            ptr->ptlMinTrackSize.x = 100;
            ptr->ptlMinTrackSize.y = 100;
            return (MRFROMSHORT(TRUE));
    }

    return (G_pfnOldFrameWndProc(hwnd, msg, mp1, mp2));
} // FrameWndProc


