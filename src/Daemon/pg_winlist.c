
/*
 *@@ pg_winlist.c:
 *      XPager window list.
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

#define INCL_DOSSEMAPHORES
#define INCL_DOSERRORS

#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WINMESSAGEMGR
#define INCL_WINSWITCHLIST

#define INCL_GPIBITMAPS                 // needed for helpers\shapewin.h
#include <os2.h>

#define DONT_REPLACE_MALLOC         // in case mem debug is enabled
#include "setup.h"                      // code generation and debugging options

#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\regexp.h"             // extended regular expressions
#include "helpers\shapewin.h"           // shaped windows;
                                        // only needed for the window class names
#include "helpers\standards.h"          // some standard macros

#include "xwpapi.h"                     // public XWorkplace definitions

#include "hook\xwphook.h"
#include "hook\hook_private.h"
#include "hook\xwpdaemn.h"              // XPager and daemon declarations

#pragma hdrstop

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

// XPager
LINKLIST        G_llWinInfos;
            // linked list of PAGERWININFO structs; this is auto-free
HMTX            G_hmtxWinInfos = 0;
            // mutex sem protecting that array
            // V0.9.12 (2001-05-31) [umoeller]: made this private

extern HAB      G_habDaemon;
extern USHORT   G_pidDaemon;

/* ******************************************************************
 *
 *   PAGERWININFO list maintenance
 *
 ********************************************************************/

/*
 *@@ pgrInit:
 *      initializes the winlist. Called from main().
 *
 *@@added V0.9.12 (2001-05-31) [umoeller]
 */

APIRET pgrInit(VOID)
{
    APIRET arc = DosCreateMutexSem(NULL,
                                   &G_hmtxWinInfos,
                                   0,
                                   FALSE);

    lstInit(&G_llWinInfos, TRUE);
            // V0.9.7 (2001-01-21) [umoeller]

    return (arc);
}

/*
 *@@ pgrLockWinlist:
 *      locks the window list.
 *
 *@@added V0.9.12 (2001-05-31) [umoeller]
 */

BOOL pgrLockWinlist(VOID)
{
    return (!WinRequestMutexSem(G_hmtxWinInfos, TIMEOUT_HMTX_WINLIST));
        // WinRequestMutexSem works even if the thread has no message queue
}

/*
 *@@ pgrUnlockWinlist:
 *
 *@@added V0.9.12 (2001-05-31) [umoeller]
 */

VOID pgrUnlockWinlist(VOID)
{
    DosReleaseMutexSem(G_hmtxWinInfos);
}

/*
 *@@ pgrFindWinInfo:
 *      returns the PAGERWININFO for hwndThis from
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

PPAGERWININFO pgrFindWinInfo(HWND hwndThis,         // in: window to find
                             PVOID *ppListNode)     // out: list node (ptr can be NULL)
{
    PPAGERWININFO pReturn = NULL;

    PLISTNODE pNode = lstQueryFirstNode(&G_llWinInfos);
    while (pNode)
    {
        PPAGERWININFO pWinInfo = (PPAGERWININFO)pNode->pItemData;
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
 *@@ ClearWinlist:
 *      clears the global list of PAGERWININFO entries.
 *
 *      Preconditions:
 *
 *      --  The caller must lock the list first.
 *
 *@@added V0.9.7 (2001-01-21) [umoeller]
 */

static VOID ClearWinlist(VOID)
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
 *@@ pgrGetWinInfo:
 *      analyzes pWinInfo->hwnd and stores the results
 *      back in *pWinInfo.
 *
 *      This does not allocate a new PAGERWININFO.
 *      Use pgrCreateWinInfo for that, which
 *      calls this function in turn.
 *
 *      Returns TRUE if the specified window data was
 *      returned.
 *
 *      If this returns FALSE, the window either
 *      does not exist or is considered irrelevant
 *      for XPager. This function excludes a number
 *      of window classes from the window list in
 *      order not to pollute anything.
 *      In that case, the fields in pWinInfo are
 *      undefined.
 *
 *      More specifically, the following are exluded:
 *
 *      --  Windows that are not children of HWND_DESKTOP.
 *
 *      --  Windows that belong to the pager.
 *
 *      --  Windows of the classes "PM Icon title",
 *          "AltTabWindow", "menu", and "shaped" windows.
 *
 *      Preconditions:
 *
 *      --  On input, pWinInfo->hwnd must be set to the
 *          window to be examined.
 *
 *      --  If pWinInfo is one of the items on the global
 *          linked list, the caller must lock the list
 *          before calling this.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 *@@changed V0.9.4 (2000-08-08) [umoeller]: removed "special" windows; now ignoring ShapeWin windows
 *@@changed V0.9.15 (2001-09-14) [umoeller]: now always checking for visibility; this fixes VX-REXX apps hanging XPager
 *@@changed V0.9.18 (2002-02-21) [lafaix]: minimized and maximized windows titles were not queryied and maximized windows couldn't be sticky
 */

BOOL pgrGetWinInfo(PPAGERWININFO pWinInfo)  // in/out: window info
{
    BOOL    brc = FALSE;

    HWND    hwnd = pWinInfo->hwnd;
    ULONG   pid, tid;

    memset(pWinInfo, 0, sizeof(PAGERWININFO));

    if (    (WinIsWindow(G_habDaemon, hwnd))
         && (WinQueryWindowProcess(hwnd, &pid, &tid))
       )
    {
        pWinInfo->hwnd = hwnd;
        pWinInfo->pid = pid;
        pWinInfo->tid = tid;

        // get PM winclass and create a copy string
        WinQueryClassName(hwnd,
                          sizeof(pWinInfo->szClassName),
                          pWinInfo->szClassName);

        WinQueryWindowPos(hwnd, &pWinInfo->swp);

        brc = TRUE;     // can be changed again

        if (pWinInfo->pid)
        {
            if (pWinInfo->pid == G_pidDaemon)
                // belongs to XPager:
                pWinInfo->bWindowType = WINDOW_PAGER;
            else if (hwnd == G_pHookData->hwndWPSDesktop)
                // WPS Desktop window:
                pWinInfo->bWindowType = WINDOW_WPSDESKTOP;
            else
            {
                const char *pcszClassName = pWinInfo->szClassName;
                if (
                        // make sure this is a desktop child;
                        // we use WinSetMultWindowPos, which requires
                        // that all windows have the same parent
                        (!WinIsChild(hwnd, HWND_DESKTOP))
                        // ignore PM "Icon title" class:
                     || (!strcmp(pcszClassName, "#32765"))
                        // ignore Warp 4 "Alt tab" window; this always exists,
                        // but is hidden most of the time
                     || (!strcmp(pcszClassName, "AltTabWindow"))
                        // ignore all menus:
                     || (!strcmp(pcszClassName, "#4"))
                        // ignore shaped windows (src\helpers\shapewin.c):
                     || (!strcmp(pcszClassName, WC_SHAPE_WINDOW))
                     || (!strcmp(pcszClassName, WC_SHAPE_REGION))
                   )
                {
                    brc = FALSE;
                }
                else
                {
                    HSWITCH hswitch;
                    ULONG ulStyle = WinQueryWindowULong(hwnd, QWL_STYLE);

                    if (    (!(hswitch = WinQuerySwitchHandle(hwnd, 0)))
                            // V0.9.15 (2001-09-14) [umoeller]:
                            // _always_ check for visibility, and
                            // if the window isn't visible, don't
                            // mark it as normal
                            // (this helps VX-REXX apps, which can
                            // solidly lock XPager with their hidden
                            // frame in the background, upon which
                            // WinSetMultWindowPos fails)
                         || (!(ulStyle & WS_VISIBLE))
                         || (pWinInfo->swp.fl & SWP_HIDE)
                         || (ulStyle & FCF_SCREENALIGN)  // netscape dialog
                       )
                        pWinInfo->bWindowType = WINDOW_DIRTY;
                    else
                    {
                        SWCNTRL     swctl;

                        // window is in tasklist:
                        WinQuerySwitchEntry(hswitch, &swctl);

                        memcpy(pWinInfo->szSwtitle,
                               swctl.szSwtitle,
                               sizeof(pWinInfo->szSwtitle));

                        // the minimize attribute prevails the "sticky" attribute,
                        // "sticky" prevails maximize, and maximize prevails normal
                        // V0.9.18 (2002-02-21) [lafaix]
                        if (pWinInfo->swp.fl & SWP_MINIMIZE)
                            pWinInfo->bWindowType = WINDOW_MINIMIZE;
                        else if (pgrIsSticky(hwnd, swctl.szSwtitle))
                            pWinInfo->bWindowType = WINDOW_STICKY;
                        else if (pWinInfo->swp.fl & SWP_MAXIMIZE)
                            pWinInfo->bWindowType = WINDOW_MAXIMIZE;
                        else
                            pWinInfo->bWindowType = WINDOW_NORMAL;
                    }
                }
            }
        }
    }

    return (brc);
}

/*
 *@@ pgrCreateWinInfo:
 *      adds a new window to our window list.
 *
 *      Called upon PGRM_WINDOWCHANGED in fnwpXPagerClient.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 *@@changed V0.9.7 (2001-01-21) [umoeller]: rewritten for linked list for wininfos
 */

VOID pgrCreateWinInfo(HWND hwnd)
{
    if (pgrLockWinlist())
    {
        // check if we have an entry for this window already
        PPAGERWININFO pWinInfo = pgrFindWinInfo(hwnd, NULL);

        if (!pWinInfo)
        {
            // not yet in list: create a new one
            pWinInfo = NEW(PAGERWININFO);
            ZERO(pWinInfo);
            pWinInfo->hwnd = hwnd;
            lstAppendItem(&G_llWinInfos, pWinInfo);
        }
        // else already present: refresh that one then

        pgrGetWinInfo(pWinInfo);

        pgrUnlockWinlist();
    }
}

/*
 *@@ pgrBuildWinlist:
 *      (re)initializes the window list.
 *      This must get called exactly once when
 *      XPager is started. XPager starts
 *      to go crazy if this gets called a second
 *      time.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 *@@changed V0.9.7 (2001-01-21) [umoeller]: rewritten for linked list for wininfos
 */

VOID pgrBuildWinlist(VOID)
{
    if (pgrLockWinlist())
    {
        HENUM henum;
        HWND hwndTemp;

        ClearWinlist();

        henum = WinBeginEnumWindows(HWND_DESKTOP);
        while (hwndTemp = WinGetNextWindow(henum))
        {
            // next window found:
            // create a temporary WININFO struct here... this will
            // allocate the PSZ's in that struct, which we can
            // then copy
            PAGERWININFO WinInfoTemp;
            WinInfoTemp.hwnd = hwndTemp;
            if (pgrGetWinInfo(&WinInfoTemp))
            {
                // window found and strings allocated maybe:
                // append this thing to the list

                PPAGERWININFO pNew;
                if (pNew = NEW(PAGERWININFO))
                {
                    memcpy(pNew, &WinInfoTemp, sizeof(PAGERWININFO));
                    lstAppendItem(&G_llWinInfos, pNew);
                }
            }
        }
        WinEndEnumWindows(henum);

        pgrUnlockWinlist();
    }

    WinPostMsg(G_pHookData->hwndPagerClient,
               PGRM_REFRESHCLIENT,
               (MPARAM)FALSE,
               0);
}

/*
 *@@ pgrFreeWinInfo:
 *      removes a window from our window list which has
 *      been destroyed.
 *
 *      Called upon PGRM_WINDOWCHANGED in fnwpXPagerClient.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 *@@changed V0.9.7 (2001-01-21) [umoeller]: now using linked list for wininfos
 */

VOID pgrFreeWinInfo(HWND hwnd)
{
    if (pgrLockWinlist())
    {
        PLISTNODE       pNodeFound = NULL;
        PPAGERWININFO    pWinInfo;

        if (    (pWinInfo = pgrFindWinInfo(hwnd,
                                            (PVOID*)&pNodeFound))
             && (pNodeFound)
           )
            // we have an item for this window:
            // remove from list, which will also free pWinInfo
            lstRemoveNode(&G_llWinInfos, pNodeFound);

        pgrUnlockWinlist();
    }
}

/*
 *@@ pgrMarkDirty:
 *      marks a window as "dirty" in our window list
 *      so that it will be refreshed on the next
 *      repaint, since the pager control window
 *      refreshes all dirty windows.
 *
 *      Called upon PGRM_WINDOWCHANGED in fnwpXPagerClient.
 *
 *@@added V0.9.7 (2001-01-15) [dk]
 *@@changed V0.9.7 (2001-01-21) [umoeller]: rewritten for linked list for wininfos
 */

VOID pgrMarkDirty(HWND hwnd)
{
    if (pgrLockWinlist())
    {
        // check if we have an entry for this window already
        PPAGERWININFO    pWinInfo;
        if (pWinInfo = pgrFindWinInfo(hwnd,
                                       NULL))
            // we have an entry:
            // mark it as dirty
            pWinInfo->bWindowType = WINDOW_DIRTY;

        pgrUnlockWinlist();
    }
}

/*
 *@@ pgrRefreshDirties:
 *      refreshes all windows which have the WINDOW_DIRTY
 *      flag set.
 *
 *      Returns TRUE if the window list has changed.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 *@@changed V0.9.7 (2001-01-17) [dk]: this scanned, but never updated. Fixed.
 *@@changed V0.9.7 (2001-01-21) [umoeller]: rewritten for linked list for wininfos
 *@@changed V0.9.12 (2001-05-31) [umoeller]: removed temp delete list
 *@@changed V0.9.18 (2002-02-20) [lafaix]: this never returned TRUE
 */

BOOL pgrRefreshDirties(VOID)
{
    BOOL    brc = FALSE;

    if (pgrLockWinlist())
    {
        // LINKLIST    llDeferredDelete;
        PLISTNODE   pNode;

        pNode = lstQueryFirstNode(&G_llWinInfos);
        while (pNode)
        {
            // get next node NOW because we can delete pNode here
            // V0.9.12 (2001-05-31) [umoeller]
            PLISTNODE pNext = pNode->pNext;

            PPAGERWININFO pWinInfo = (PPAGERWININFO)pNode->pItemData;
            if (pWinInfo->bWindowType == WINDOW_DIRTY)
            {
                // window needs refresh:
                // well, refresh then... this clears strings
                // in the wininfo and only allocates new strings
                // if TRUE is returned
                PAGERWININFO WinInfoTemp;
                WinInfoTemp.hwnd = pWinInfo->hwnd;
                if (!pgrGetWinInfo(&WinInfoTemp))
                {
                    // window is no longer valid:
                    // remove it from the list then
                    // (defer this, since we're iterating over the list)
                    lstRemoveNode(&G_llWinInfos, pNode);
                            // V0.9.12 (2001-05-31) [umoeller]
                    // report "changed"
                    brc = TRUE;
                }
                else
                {
                    // window is still valid:
                    // check if it has changed
                    if (    (pWinInfo->bWindowType != WinInfoTemp.bWindowType)
                         || (memcmp(&pWinInfo->swp, &WinInfoTemp.swp, sizeof(SWP)))
                         || (strcmp(pWinInfo->szSwtitle, WinInfoTemp.szSwtitle))
                       )
                    {
                        // window changed:
                        // copy new wininfo over that; we have new strings
                        // in the new wininfo
                        memcpy(pWinInfo, &WinInfoTemp, sizeof(WinInfoTemp));

                        // report "changed"
                        brc = TRUE;
                    }
                }
            } // end if (pWinInfo->bWindowType == WINDOW_DIRTY)

            pNode = pNext;
        } // end while (pNode)

        pgrUnlockWinlist();
    }

    return (brc);
}

/*
 *@@ pgrIsSticky:
 *      returns TRUE if the window with the specified window
 *      and switch titles is a sticky window. A window is
 *      considered sticky if its switch list title matches
 *      an include entry in the "sticky windows" list or if
 *      it is an XCenter and it does not match an exclude
 *      entry.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 *@@changed V0.9.16 (2001-10-31) [umoeller]: now making system window list sticky always
 *@@changed V0.9.19 (2002-04-14) [lafaix]: now uses matching criteria
 *@@changed V0.9.19 (2002-04-17) [umoeller]: added regular expressions (SF_MATCHES)
 */

BOOL pgrIsSticky(HWND hwnd,
                 PCSZ pcszSwtitle)
{
    HWND hwndClient;

    // check for system window list
    if (    (G_pHookData)
         && (hwnd == G_pHookData->hwndSwitchList)
       )
        return (TRUE);

    if (pcszSwtitle)
    {
        ULONG   ul;

        for (ul = 0;
             ul < G_pHookData->PagerConfig.cStickies;
             ul++)
        {
            ULONG   flThis = G_pHookData->PagerConfig.aulStickyFlags[ul];
            PCSZ    pcszThis = G_pHookData->PagerConfig.aszStickies[ul];

            BOOL    fInclude = (flThis & SF_CRITERIA_MASK) == SF_INCLUDE;

            if ((flThis & SF_OPERATOR_MASK) == SF_MATCHES)
            {
                // regular expression:
                // check if we have compiled this one already
                int rc;
                ERE *pERE;
                if (!(G_pHookData->paEREs[ul]))
                    // compile now
                    G_pHookData->paEREs[ul] = rxpCompile(pcszThis,
                                                         0,
                                                         &rc);

                if (pERE = G_pHookData->paEREs[ul])
                {
                    int             pos, length;
                    ERE_MATCHINFO   mi;
                    // _Pmpf((__FUNCTION__ ": checking %s", pcszSwtitle));
                    if (rxpMatch_fwd(pERE,
                                     0,
                                     pcszSwtitle,
                                     0,
                                     &pos,
                                     &length,
                                     &mi))
                    {
                         // make sure we don't just have a substring
                         if (    (pos == 0)
                              && (    (length >= STICKYLEN - 1)
                                   || (length == strlen(pcszSwtitle))
                                 )
                            )
                            return fInclude;
                    }
                }
            }
            else
            {
                PCSZ pResult = strstr(pcszSwtitle,
                                      pcszThis);

                switch (flThis & SF_OPERATOR_MASK)
                {
                     case SF_CONTAINS:
                         if (pResult)
                             return fInclude;
                     break;

                     case SF_BEGINSWITH:
                         if (pResult == pcszSwtitle)
                             return fInclude;
                     break;

                     case SF_ENDSWITH:
                         if (    (pResult)
                                 // as the pattern has been found, the switch name
                                 // is at least as long as the pattern, so the following
                                 // is safe
                              && (!strcmp(  pcszSwtitle
                                          + strlen(pcszSwtitle)
                                          - strlen(pcszThis),
                                          pcszThis))
                            )
                         {
                             return fInclude;
                         }
                     break;

                     case SF_EQUALS:
                         if (     (pResult)
                              &&  (!strcmp(pcszSwtitle,
                                           pcszThis))
                            )
                         {
                             return fInclude;
                         }
                     break;
                }
            }
        }
    }

    // not in sticky names list:
    // check if it's an XCenter (check client class name)
    if (hwndClient = WinWindowFromID(hwnd, FID_CLIENT))
    {
        CHAR szClass[100];
        if (    (WinQueryClassName(hwndClient, sizeof(szClass), szClass))
             && (!strcmp(szClass, WC_XCENTER_CLIENT))
           )
            // target is XCenter:
            return TRUE;
    }

    return (FALSE);
}


