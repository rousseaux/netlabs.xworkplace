
/*
 *@@ pgmg_winscan.c:
 *      PageMage window scanning and checking.
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
#include <process.h>

#include "setup.h"                      // code generation and debugging options

#include "helpers\shapewin.h"           // shaped windows;
                                        // only needed for the window class names

#include "hook\xwphook.h"
#include "hook\hook_private.h"
#include "hook\xwpdaemn.h"              // PageMage and daemon declarations

/*
 *@@ pgmwGetHWNDInfo:
 *      analyzes the specified window and
 *      stores the result in *phl.
 *
 *      Returns TRUE if the specified window data was
 *      returned.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 *@@changed V0.9.4 (2000-08-08) [umoeller]: removed "special" windows; now ignoring ShapeWin windows
 */

BOOL pgmwGetWinInfo(HWND hwnd,         // in: window to test
                    HWNDLIST *phl)     // out: window info
{
    BOOL    brc = FALSE;

    if (WinIsWindow(G_habDaemon, hwnd))
    {
        HSWITCH     hswitch;
        SWCNTRL     swctl;

        ULONG       pidPM;      // process ID of first PMSHELL.EXE process
        WinQueryWindowProcess(HWND_DESKTOP, &pidPM, NULL);

        phl->hwnd = hwnd;
        WinQueryWindowProcess(hwnd, &phl->pid, &phl->tid);
        WinQueryClassName(hwnd, sizeof(phl->szClassName), phl->szClassName);
        WinQueryWindowPos(hwnd, &phl->swp);

        phl->szSwitchName[0] = 0;

        brc = TRUE;

        // excludes invisible "Alt-Tab" window (Warp 4):
        if (phl->pid)
        {
            if (phl->pid == G_pidDaemon)
                // belongs to PageMage:
                phl->bWindowType = WINDOW_PAGEMAGE;
            else if (hwnd == G_pHookData->hwndWPSDesktop)
                // WPS Desktop window:
                phl->bWindowType = WINDOW_WPSDESKTOP;
            else
                if (   // not PM Desktop child?
                        (!WinIsChild(phl->hwnd, HWND_DESKTOP))
                     || (strcmp(phl->szClassName, "#32765") == 0)
                            // PM "Icon title" class
                     || (strcmp(phl->szClassName, "AltTabWindow") == 0)
                            // Warp 4 "Alt tab" window; this always exists,
                            // but is hidden most of the time
                     || (strcmp(phl->szClassName, "#4") == 0)
                            // menu, always ignore those
                     || (strcmp(phl->szClassName, WC_SHAPE_WINDOW) == 0)
                     || (strcmp(phl->szClassName, WC_SHAPE_REGION) == 0)
                            // ignore shaped windows (src\helpers\shapewin.c)
                   )
                    brc = FALSE;
                else
                {
                    phl->bWindowType = WINDOW_NORMAL;

                    hswitch = WinQuerySwitchHandle(hwnd, 0);
                    if (hswitch == NULLHANDLE)
                    {
                        phl->bWindowType = WINDOW_RESCAN;
                    }
                    else
                    {
                        // window is in tasklist:
                        WinQuerySwitchEntry(hswitch, &swctl);
                        strcpy(phl->szSwitchName, swctl.szSwtitle);

                        if (phl->swp.fl & SWP_MINIMIZE)
                            phl->bWindowType = WINDOW_MINIMIZE;
                        else if (phl->swp.fl & SWP_MAXIMIZE)
                            phl->bWindowType = WINDOW_MAXIMIZE;
                        else if (pgmwStickyCheck(/* phl->szWindowName, */ phl->szSwitchName))
                            phl->bWindowType = WINDOW_STICKY;
                    }
                }
        }
    }

    return (brc);
}

/*
 *@@ pgmwScanAllWindows:
 *      (re)initializes the window list.
 *      This must get called exactly once when
 *      PageMage is started. PageMage starts
 *      to go crazy if this gets called a second
 *      time.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 */

VOID pgmwScanAllWindows(VOID)
{
    HENUM heScan;
    HWND hwndTemp;

    _Pmpf(("Entering pgmwScanAllWindows..."));

    if (DosRequestMutexSem(G_hmtxWindowList, TIMEOUT_PGMG_WINLIST)
            == NO_ERROR)
    {
        G_usWindowCount = 0;
        memset(G_MainWindowList, 0, sizeof(G_MainWindowList));
        heScan = WinBeginEnumWindows(HWND_DESKTOP);
        while ((hwndTemp = WinGetNextWindow(heScan)) != NULLHANDLE)
        {
            // next window found:
            // store in list
            if (pgmwGetWinInfo(hwndTemp, &G_MainWindowList[G_usWindowCount]))
            {
                // window found:
                _Pmpf(("Test %d: hwnd 0x%lX \"%s\":\"%s\" pid 0x%lX(%d) type %d",
                       G_usWindowCount,
                       G_MainWindowList[G_usWindowCount].hwnd,
                       G_MainWindowList[G_usWindowCount].szSwitchName,
                       G_MainWindowList[G_usWindowCount].szClassName,
                       G_MainWindowList[G_usWindowCount].pid,
                       G_MainWindowList[G_usWindowCount].pid,
                       G_MainWindowList[G_usWindowCount].bWindowType));

                // advance offset
                G_usWindowCount++;
                if (G_usWindowCount == MAX_WINDOWS)
                {
                    // array full:
                    WinPostMsg(G_pHookData->hwndDaemonObject,
                               XDM_PGMGWINLISTFULL,
                               0, 0);
                    break;
                }
            }
        }
        WinEndEnumWindows(heScan);

        DosReleaseMutexSem(G_hmtxWindowList);
    }

    WinPostMsg(G_pHookData->hwndPageMageClient,
               PGMG_INVALIDATECLIENT,
               (MPARAM)FALSE,       // delayed
               0);

    _Pmpf(("Done with pgmwScanAllWindows."));
}

/*
 *@@ pgmwWindowListAdd:
 *      adds a new window to our window list.
 *      Called upon PGMG_WNDCHANGE in fnwpPageMageClient.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 */

VOID pgmwWindowListAdd(HWND hwnd)
{
    USHORT usIdx;

    if (DosRequestMutexSem(G_hmtxWindowList, TIMEOUT_PGMG_WINLIST)
            == NO_ERROR)
    {
        // check if we have an entry for this window already
        for (usIdx = 0; usIdx < G_usWindowCount; usIdx++)
            if (G_MainWindowList[usIdx].hwnd == hwnd)
                break;

        do {
            if (usIdx == G_usWindowCount)
                // no, we need a new entry:
                if (G_usWindowCount < MAX_WINDOWS)
                    G_usWindowCount++;
                else
                {
                    // window list is full:
                    WinPostMsg(G_pHookData->hwndDaemonObject,
                               XDM_PGMGWINLISTFULL,
                               0, 0);
                    break;
                }

            pgmwGetWinInfo(hwnd, &G_MainWindowList[usIdx]);
        } while (FALSE);

        /* if (G_MainWindowList[usIdx].bWindowType == WINDOW_RESCAN)
            WinPostMsg(G_pHookData->hwndPageMageClient,
                       PGMG_WNDCHANGE,
                       MPFROMHWND(hwnd),
                       MPFROMLONG(WM_CREATE)); */

        DosReleaseMutexSem(G_hmtxWindowList);
    }
}

/*
 *@@ pgmwWindowListDelete:
 *      removes a window from our window list which has
 *      been destroyed.
 *      Called upon PGMG_WNDCHANGE in fnwpPageMageClient.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 */

VOID pgmwWindowListDelete(HWND hwnd)
{
    USHORT usIdx;

    if (DosRequestMutexSem(G_hmtxWindowList, TIMEOUT_PGMG_WINLIST)
            == NO_ERROR)
    {
        for (usIdx = 0; usIdx < G_usWindowCount; usIdx++)
            if (G_MainWindowList[usIdx].hwnd == hwnd)
            {
                G_usWindowCount--;
                if (usIdx != G_usWindowCount)
                    memcpy(&G_MainWindowList[usIdx],
                           &G_MainWindowList[G_usWindowCount],
                           sizeof(HWNDLIST));
                break;
            }
        DosReleaseMutexSem(G_hmtxWindowList);
    }
}

/*
 *@@ pgmwWindowListRescan:
 *      rescans all windows which have the WINDOW_RESCAN
 *      flag set. Returns TRUE if the window list has
 *      changed.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 */

BOOL pgmwWindowListRescan(VOID)
{
    BOOL    brc = FALSE;
    USHORT  usIdx = 0;

    if (DosRequestMutexSem(G_hmtxWindowList, TIMEOUT_PGMG_WINLIST)
            == NO_ERROR)
    {
        for (usIdx = 0;
             usIdx < G_usWindowCount;
             usIdx++)

            if (G_MainWindowList[usIdx].bWindowType == WINDOW_RESCAN)
            {
                // window needs rescan:
                HWNDLIST hwndListTemp;
                if (!pgmwGetWinInfo(G_MainWindowList[usIdx].hwnd, &hwndListTemp))
                {
                    // window no longer valid:
                    pgmwWindowListDelete(G_MainWindowList[usIdx].hwnd);
                    brc = TRUE;
                }
                else
                {
                    if (memcmp(&G_MainWindowList[usIdx], &hwndListTemp, sizeof(hwndListTemp))
                            != 0)
                        // changed:
                        brc = TRUE;
                }
            }

        DosReleaseMutexSem(G_hmtxWindowList);
    }

    return (brc);
}

/*
 *@@ pgmwStickyCheck:
 *      returns TRUE if the window with the specified window
 *      and switch titles is a sticky window. A window is
 *      considered sticky if its switch list title is in
 *      the "sticky windows" list.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 */

BOOL pgmwStickyCheck(// CHAR *pszWindowName,
                     CHAR *pszSwitchName)
{
    USHORT  usIdx;
    BOOL    bFound = FALSE;

    for (usIdx = 0;
         usIdx < G_pHookData->PageMageConfig.usStickyTextNum;
         usIdx++)
    {
        if (strstr(pszSwitchName,
                   G_pHookData->PageMageConfig.aszSticky[usIdx]))
        {
            bFound = TRUE;
            break;
        }
    }

    return (bFound);
} // pgmwStickyCheck

/*
 *@@ pgmwSticky2Check:
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 */

BOOL pgmwSticky2Check(HWND hwndTest) // in: window to test for stickyness
{
    USHORT  usIdx;
    BOOL    bFound;

    bFound = FALSE;
    for (usIdx = 0; usIdx < G_pHookData->PageMageConfig.usSticky2Num; usIdx++)
    {
        if (G_pHookData->PageMageConfig.hwndSticky2[usIdx] == hwndTest)
        {
            bFound = TRUE;
            break;
        }
    }

    return (bFound);
} // pgmwSticky2Check

/*
 *@@ pgmwGetWindowFromClientPoint:
 *      returns the window from the list which
 *      matches the given pager client coordinates.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 */

HWND pgmwGetWindowFromClientPoint(ULONG ulX,  // in: x coordinate within the PageMage client
                                  ULONG ulY)  // in: y coordinate within the PageMage client
{
    POINTL  ptlCalc;
    float   flCalcX, flCalcY;
    HENUM   henumPoint;
    HWND    hwndPoint;
    SWP     swpPoint;
    HWND    hwndResult = NULLHANDLE;

    flCalcX = (float) ulX / (G_ptlEachDesktop.x + 1);
    flCalcY = (float) ulY / (G_ptlEachDesktop.y + 1);

    flCalcX = (float) flCalcX * G_pHookData->lCXScreen;
    flCalcY = (float) flCalcY * G_pHookData->lCYScreen;

    ptlCalc.x = ((int) flCalcX) - G_ptlCurrPos.x;
    ptlCalc.y = ((int) flCalcY)
                - (   (G_pHookData->PageMageConfig.ptlMaxDesktops.y - 1)
                       * G_pHookData->lCYScreen
                    - G_ptlCurrPos.y
                  );

    henumPoint = WinBeginEnumWindows(HWND_DESKTOP);
    while ((hwndPoint = WinGetNextWindow(henumPoint)) != NULLHANDLE)
    {
        if (hwndPoint == G_pHookData->hwndPageMageFrame)
            // ignore PageMage frame
            continue;

        WinQueryWindowPos(hwndPoint, &swpPoint);
        if (    (ptlCalc.x >= swpPoint.x)
             && (ptlCalc.x <= swpPoint.x + swpPoint.cx)
             && (ptlCalc.y >= swpPoint.y)
             && (ptlCalc.y <= swpPoint.y + swpPoint.cy)
           )
        {
            // this window matches the coordinates:
            // check if it's visible
            // V0.9.2 (2000-02-22) [umoeller]
            if (WinIsWindowVisible(hwndPoint))
            {
                hwndResult = hwndPoint;
                break;
            }
        }
    }
    WinEndEnumWindows(henumPoint);

    return (hwndResult);
} // pgmwGetWindowFromClientPoint


