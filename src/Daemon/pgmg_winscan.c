
/*
 *@@ pgmg_winscan.c:
 *      PageMage window scanning and checking.
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

#define INCL_DOSSEMAPHORES
#define INCL_DOSERRORS

#define INCL_WINWINDOWMGR
#define INCL_WINMESSAGEMGR
#define INCL_WINSWITCHLIST

#define INCL_GPIBITMAPS
#include <os2.h>

#include "setup.h"                      // code generation and debugging options

#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\shapewin.h"           // shaped windows;
                                        // only needed for the window class names

#include "hook\xwphook.h"
#include "hook\hook_private.h"
#include "hook\xwpdaemn.h"              // PageMage and daemon declarations

/* ******************************************************************
 *
 *   PGMGWININFO list maintenance
 *
 ********************************************************************/

/*
 *@@ pgmwFindWinInfo:
 *      returns the PGMGWININFO for hwndThis from
 *      the global linked list or NULL if no item
 *      exists for that window.
 *
 *      If ppListNode is != NULL, it receives a
 *      pointer to the LISTNODE representing that
 *      item (in case you want to quickly free it).
 *
 *      Preconditions:
 *
 *      --  The caller must lock the list first.
 *
 *@@added V0.9.7 (2001-01-21) [umoeller]
 */

PPGMGWININFO pgmwFindWinInfo(HWND hwndThis,         // in: window to find
                             PVOID *ppListNode)     // out: list node (ptr can be NULL)
{
    PPGMGWININFO pReturn = NULL;

    PLISTNODE pNode = lstQueryFirstNode(&G_llWinInfos);
    while (pNode)
    {
        PPGMGWININFO pWinInfo = (PPGMGWININFO)pNode->pItemData;
        if (pWinInfo->hwnd == hwndThis)
        {
            pReturn = pWinInfo;
            break;
        }

        pNode = pNode->pNext;
    }

    if (ppListNode)
        *ppListNode = pNode;

    return (pReturn);
}

/*
 *@@ pgmwClearWinInfos:
 *      clears the global list of PGMGWININFO entries.
 *
 *      Preconditions:
 *
 *      --  The caller must lock the list first.
 *
 *@@added V0.9.7 (2001-01-21) [umoeller]
 */

VOID pgmwClearWinInfos(VOID)
{
    // clear the list... it's in auto-free mode,
    // so this will clear the WININFO structs as well
    lstClear(&G_llWinInfos);
}

/* ******************************************************************
 *
 *   Window analysis, exported interfaces
 *
 ********************************************************************/

/*
 *@@ pgmwFillWinInfo:
 *      analyzes the specified window and stores the
 *      results in *pWinInfo.
 *
 *      This does not allocate a new PGMGWININFO.
 *      Use pgmwAppendNewWinInfo for that, which
 *      calls this function in turn.
 *
 *      Returns TRUE if the specified window data was
 *      returned and maybe memory was allocated.
 *
 *      If this returns FALSE, the window either
 *      does not exist or is considered irrelevant
 *      for PageMage. This function excludes a number
 *      of window classes from the window list in
 *      order not to pollute anything.
 *      In that case, no memory was allocated.
 *
 *      Preconditions:
 *
 *      -- If pWinInfo is one of the items on the global
 *         linked list, the caller must lock the list
 *         before calling this.
 *
 *      -- If the pWinInfo has not been used before, the
 *         caller must zero all fields, or this func will
 *         attempt to free the strings in there.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 *@@changed V0.9.4 (2000-08-08) [umoeller]: removed "special" windows; now ignoring ShapeWin windows
 */

BOOL pgmwFillWinInfo(HWND hwnd,              // in: window to test
                     PPGMGWININFO pWinInfo)  // out: window info
{
    BOOL    brc = FALSE;

    memset(pWinInfo, 0, sizeof(PGMGWININFO));

    if (WinIsWindow(G_habDaemon, hwnd))
    {
        pWinInfo->hwnd = hwnd;
        WinQueryWindowProcess(hwnd, &pWinInfo->pid, &pWinInfo->tid);

        // get PM winclass and create a copy string
        WinQueryClassName(hwnd,
                          sizeof(pWinInfo->szClassName),
                          pWinInfo->szClassName);

        WinQueryWindowPos(hwnd, &pWinInfo->swp);

        brc = TRUE;     // can be changed again

        // excludes invisible "Alt-Tab" window (Warp 4):
        if (pWinInfo->pid)
        {
            if (pWinInfo->pid == G_pidDaemon)
                // belongs to PageMage:
                pWinInfo->bWindowType = WINDOW_PAGEMAGE;
            else if (hwnd == G_pHookData->hwndWPSDesktop)
                // WPS Desktop window:
                pWinInfo->bWindowType = WINDOW_WPSDESKTOP;
            else
            {
                const char *pcszClassName = pWinInfo->szClassName;
                if (   // not PM Desktop child?
                        (!WinIsChild(pWinInfo->hwnd, HWND_DESKTOP))
                     || (strcmp(pcszClassName, "#32765") == 0)
                            // PM "Icon title" class
                     || (strcmp(pcszClassName, "AltTabWindow") == 0)
                            // Warp 4 "Alt tab" window; this always exists,
                            // but is hidden most of the time
                     || (strcmp(pcszClassName, "#4") == 0)
                            // menu, always ignore those
                     || (strcmp(pcszClassName, WC_SHAPE_WINDOW) == 0)
                     || (strcmp(pcszClassName, WC_SHAPE_REGION) == 0)
                            // ignore shaped windows (src\helpers\shapewin.c)
                   )
                    brc = FALSE;
                else
                {
                    HSWITCH hswitch = WinQuerySwitchHandle(hwnd, 0);

                    pWinInfo->bWindowType = WINDOW_NORMAL;

                    if (hswitch == NULLHANDLE)
                    {
                        // V0.9.7 (2001-01-23) [dk]
                        ULONG ulQ = WinQueryWindowULong(pWinInfo->hwnd, QWL_STYLE);
                        if (
                                (!(ulQ & WS_VISIBLE))
                             || (ulQ & FCF_SCREENALIGN)  // netscape dialog
                           )
                            pWinInfo->bWindowType = WINDOW_RESCAN;
                    }
                    else if (pWinInfo->swp.fl & SWP_MINIMIZE)
                        pWinInfo->bWindowType = WINDOW_MINIMIZE;
                    else if (pWinInfo->swp.fl & SWP_MAXIMIZE)
                        pWinInfo->bWindowType = WINDOW_MAXIMIZE;
                    else
                    {
                        SWCNTRL     swctl;

                        // window is in tasklist:
                        WinQuerySwitchEntry(hswitch, &swctl);

                        if (swctl.szSwtitle[0])
                            strncpy(pWinInfo->szSwitchName,
                                    swctl.szSwtitle,
                                    sizeof(pWinInfo->szSwitchName) - 1);

                        if (pgmwStickyCheck(swctl.szSwtitle))
                             pWinInfo->bWindowType = WINDOW_STICKY;
                    }
                }
            }
        }
    }

    return (brc);
}

/*
 *@@ pgmwWinInfosEqual:
 *      compares two wininfos and returns TRUE
 *      if they are about the same.
 *
 *@@added V0.9.7 (2001-01-21) [umoeller]
 */

BOOL pgmwWinInfosEqual(PPGMGWININFO pWinInfo1,
                       PPGMGWININFO pWinInfo2)
{
    BOOL brc = FALSE;
    if (    (pWinInfo1->bWindowType == pWinInfo2->bWindowType)
         && (!memcmp(&pWinInfo1->swp, &pWinInfo2->swp, sizeof(SWP)))
       )
    {
        brc = (!strcmp(pWinInfo2->szSwitchName, pWinInfo2->szSwitchName));
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
 *@@changed V0.9.7 (2001-01-21) [umoeller]: rewritten for linked list for wininfos
 */

VOID pgmwScanAllWindows(VOID)
{
    HENUM heScan;
    HWND hwndTemp;

    _Pmpf(("Entering pgmwScanAllWindows..."));

    if (WinRequestMutexSem(G_hmtxWindowList, TIMEOUT_HMTX_WINLIST)
            == NO_ERROR)
    {
        pgmwClearWinInfos();

        heScan = WinBeginEnumWindows(HWND_DESKTOP);
        while ((hwndTemp = WinGetNextWindow(heScan)) != NULLHANDLE)
        {
            // next window found:
            // create a temporary WININFO struct here... this will
            // allocate the PSZ's in that struct, which we can
            // then copy
            PGMGWININFO WinInfoTemp;
            memset(&WinInfoTemp, 0, sizeof(WinInfoTemp));
            if (pgmwFillWinInfo(hwndTemp, &WinInfoTemp))
            {
                // window found and strings allocated maybe:
                // append this thing to the list

                PPGMGWININFO pNew = (PPGMGWININFO)malloc(sizeof(PGMGWININFO));
                if (pNew)
                {
                    memcpy(pNew, &WinInfoTemp, sizeof(PGMGWININFO));
                    lstAppendItem(&G_llWinInfos, pNew);
                }

                /* _Pmpf(("Test %d: hwnd 0x%lX \"%s\":\"%s\" pid 0x%lX(%d) type %d",
                       G_usWindowCount,
                       G_MainWindowList[G_usWindowCount].hwnd,
                       G_MainWindowList[G_usWindowCount].szSwitchName,
                       G_MainWindowList[G_usWindowCount].szClassName,
                       G_MainWindowList[G_usWindowCount].pid,
                       G_MainWindowList[G_usWindowCount].pid,
                       G_MainWindowList[G_usWindowCount].bWindowType)); */

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
 *@@ pgmwAppendNewWinInfo:
 *      adds a new window to our window list.
 *
 *      Called upon PGMG_WNDCHANGE in fnwpPageMageClient.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 *@@changed V0.9.7 (2001-01-21) [umoeller]: rewritten for linked list for wininfos
 */

VOID pgmwAppendNewWinInfo(HWND hwnd)
{
    USHORT usIdx;

    if (WinRequestMutexSem(G_hmtxWindowList, TIMEOUT_HMTX_WINLIST)
            == NO_ERROR)
    {
        PPGMGWININFO pWinInfo = NULL;
        BOOL fAppend = FALSE;

        // check if we have an entry for this window already
        pWinInfo = pgmwFindWinInfo(hwnd, NULL);

        if (!pWinInfo)
        {
            // not yet in list: create a new one
            pWinInfo = (PPGMGWININFO)malloc(sizeof(PGMGWININFO));
            memset(pWinInfo, 0, sizeof(PGMGWININFO));
            fAppend = TRUE;
        }
        // else already present: refresh that one then

        pgmwFillWinInfo(hwnd, pWinInfo);

        if (fAppend)
            lstAppendItem(&G_llWinInfos, pWinInfo);

        DosReleaseMutexSem(G_hmtxWindowList);
    }
}

/*
 *@@ pgmwDeleteWinInfo:
 *      removes a window from our window list which has
 *      been destroyed.
 *
 *      Called upon PGMG_WNDCHANGE in fnwpPageMageClient.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 *@@changed V0.9.7 (2001-01-21) [umoeller]: now using linked list for wininfos
 */

VOID pgmwDeleteWinInfo(HWND hwnd)
{
    if (WinRequestMutexSem(G_hmtxWindowList, TIMEOUT_HMTX_WINLIST)
            == NO_ERROR)
    {
        PLISTNODE       pNodeFound = NULL;
        PPGMGWININFO    pWinInfo = pgmwFindWinInfo(hwnd,
                                                   (PVOID*)&pNodeFound);

        if (pWinInfo && pNodeFound)
        {
            // we have an item for this window:
            // remove from list, which will also free pWinInfo
            lstRemoveNode(&G_llWinInfos, pNodeFound);
        }

        DosReleaseMutexSem(G_hmtxWindowList);
    }
}

/*
 *@@ pgmwUpdateWinInfo:
 *      updates a window in our window list.
 *
 *      Called upon PGMG_WNDCHANGE in fnwpPageMageClient.
 *
 *@@added V0.9.7 (2001-01-15) [dk]
 *@@changed V0.9.7 (2001-01-21) [umoeller]: rewritten for linked list for wininfos
 */

VOID pgmwUpdateWinInfo(HWND hwnd)
{
    USHORT usIdx;

    if (WinRequestMutexSem(G_hmtxWindowList, TIMEOUT_HMTX_WINLIST)
            == NO_ERROR)
    {
        // check if we have an entry for this window already
        PPGMGWININFO    pWinInfo = pgmwFindWinInfo(hwnd,
                                                   NULL);

        if (pWinInfo)
            // we have an entry:
            // mark it as dirty
            pWinInfo->bWindowType = WINDOW_RESCAN;

        // rescan all dirties
        pgmwWindowListRescan();

        DosReleaseMutexSem(G_hmtxWindowList);
    }
}

/*
/*
 *@@ pgmwWindowListRescan:
 *      rescans all windows which have the WINDOW_RESCAN
 *      flag set. Returns TRUE if the window list has
 *      changed.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 *@@changed V0.9.7 (2001-01-17) [dk]: this scanned, but never updated. Fixed.
 *@@changed V0.9.7 (2001-01-21) [umoeller]: rewritten for linked list for wininfos
 */

BOOL pgmwWindowListRescan(VOID)
{
    BOOL    brc = FALSE;
    USHORT  usIdx = 0;

    if (WinRequestMutexSem(G_hmtxWindowList, TIMEOUT_HMTX_WINLIST)
            == NO_ERROR)
    {
        LINKLIST    llDeferredDelete;
        PLISTNODE   pNode;

        lstInit(&llDeferredDelete,
                FALSE);        // no auto-free, this points to
                               // existing list nodes

        pNode = lstQueryFirstNode(&G_llWinInfos);
        while (pNode)
        {
            PPGMGWININFO pWinInfo = (PPGMGWININFO)pNode->pItemData;
            if (pWinInfo->bWindowType == WINDOW_RESCAN)
            {
                // window needs rescan:
                // well, rescan then... this clears strings
                // in the wininfo and only allocates new strings
                // if TRUE is returned
                PGMGWININFO WinInfoTemp;
                memset(&WinInfoTemp, 0, sizeof(WinInfoTemp));
                if (!pgmwFillWinInfo(pWinInfo->hwnd, &WinInfoTemp))
                    // window is no longer valid:
                    // remove it from the list then
                    // (defer this, since we're iterating over the list)
                    lstAppendItem(&llDeferredDelete, pWinInfo);
                else
                {
                    // window is still valid:
                    // check if it has changed
                    if (!pgmwWinInfosEqual(&WinInfoTemp,
                                           pWinInfo))
                    {
                        // window changed:
                        // copy new wininfo over that; we have new strings
                        // in the new wininfo
                        memcpy(pWinInfo, &WinInfoTemp, sizeof(WinInfoTemp));
                    }
                }
            } // end if (pWinInfo->bWindowType == WINDOW_RESCAN)

            pNode = pNode->pNext;
        } // end while (pNode)

        // go thru "deferred delete" list
        pNode = lstQueryFirstNode(&llDeferredDelete);
        while (pNode)
        {
            PPGMGWININFO pWinInfo = (PPGMGWININFO)pNode->pItemData;
            lstRemoveItem(&G_llWinInfos, pWinInfo);
            pNode = pNode->pNext;
        }
        lstClear(&llDeferredDelete);

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

BOOL pgmwStickyCheck(const char *pcszSwitchName)
{
    BOOL    bFound = FALSE;

    // shortcuts to global pagemage config
    if (pcszSwitchName)
    {
        PPAGEMAGECONFIG pPageMageConfig = &G_pHookData->PageMageConfig;

        USHORT  usIdx;

        for (usIdx = 0;
             usIdx < pPageMageConfig->usStickyTextNum;
             usIdx++)
        {
            if (strstr(pcszSwitchName,
                       pPageMageConfig->aszSticky[usIdx]))
            {
                bFound = TRUE;
                break;
            }
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
    // shortcuts to global pagemage config
    PPAGEMAGECONFIG pPageMageConfig = &G_pHookData->PageMageConfig;

    USHORT  usIdx;
    BOOL    bFound;

    bFound = FALSE;
    for (usIdx = 0;
         usIdx < pPageMageConfig->usSticky2Num;
         usIdx++)
    {
        if (pPageMageConfig->hwndSticky2[usIdx] == hwndTest)
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
    // shortcuts to global pagemage config
    PPAGEMAGECONFIG pPageMageConfig = &G_pHookData->PageMageConfig;
    PPOINTL         pptlMaxDesktops = &pPageMageConfig->ptlMaxDesktops;

    POINTL  ptlCalc;
    float   flCalcX, flCalcY;
    HENUM   henumPoint;
    HWND    hwndPoint;
    SWP     swpPoint;
    HWND    hwndResult = NULLHANDLE;

    flCalcX = (float) ulX / (G_szlEachDesktopInClient.cx + 1);
    flCalcY = (float) ulY / (G_szlEachDesktopInClient.cy + 1);

    flCalcX = (float) flCalcX * G_szlEachDesktopReal.cx;
    flCalcY = (float) flCalcY * G_szlEachDesktopReal.cy;

    ptlCalc.x = ((int) flCalcX) - G_ptlCurrPos.x;
    ptlCalc.y = ((int) flCalcY)
                - (   (pptlMaxDesktops->y - 1)
                       * G_szlEachDesktopReal.cy
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


