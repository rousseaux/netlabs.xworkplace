
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

#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#define INCL_DOSEXCEPTIONS
#define INCL_DOSERRORS

#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WINMESSAGEMGR
#define INCL_WINSWITCHLIST

#define INCL_GPIBITMAPS                 // needed for helpers\shapewin.h
#include <os2.h>

#include <stdio.h>
#include <setjmp.h>

#define DONT_REPLACE_FOR_DBCS
#define DONT_REPLACE_MALLOC         // in case mem debug is enabled
#include "setup.h"                      // code generation and debugging options

#include "helpers\except.h"             // exception handling
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\regexp.h"             // extended regular expressions
#include "helpers\shapewin.h"           // shaped windows;
                                        // only needed for the window class names
#include "helpers\standards.h"          // some standard macros
#include "helpers\threads.h"

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

    return arc;
}

/*
 *@@ pgrLockWinlist:
 *      locks the window list.
 *
 *@@added V0.9.12 (2001-05-31) [umoeller]
 */

BOOL pgrLockWinlist(VOID)
{
    return !WinRequestMutexSem(G_hmtxWinInfos, TIMEOUT_HMTX_WINLIST);
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
        if (pWinInfo->swctl.hwnd == hwndThis)
        {
            pReturn = pWinInfo;
            break;
        }

        pNode = pNode->pNext;
    }

    if (ppListNode)
        *ppListNode = pNode;

    return pReturn;
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
 *   Debugging
 *
 ********************************************************************/

#ifdef __DEBUG__

static VOID DumpOneWindow(PCSZ pcszPrefix,
                          PPAGERWININFO pEntryThis)
{
    _Pmpf(("%s hwnd 0x%lX \"%s\":\"%s\" pid 0x%lX(%d) type %d",
           pcszPrefix,
           pEntryThis->swctl.hwnd,
           pEntryThis->swctl.szSwtitle,
           pEntryThis->szClassName,
           pEntryThis->swctl.idProcess,
           pEntryThis->swctl.idProcess,
           pEntryThis->bWindowType));
}

/*
 *@@ DumpAllWindows:
 *
 */

static VOID DumpAllWindows(VOID)
{
    if (pgrLockWinlist())
    {
        ULONG ul = 0;
        PLISTNODE pNode = lstQueryFirstNode(&G_llWinInfos);
        while (pNode)
        {
            PPAGERWININFO pEntryThis = (PPAGERWININFO)pNode->pItemData;
            CHAR szPrefix[100];
            sprintf(szPrefix, "Dump %d:", ul++);
            DumpOneWindow(szPrefix, pEntryThis);

            pNode = pNode->pNext;
        }

        pgrUnlockWinlist();
    }
}

#endif

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
 *      --  On input, pWinInfo->swctl.hwnd must be set to
 *          the window to be examined. All other fields
 *          are ignored and reset here.
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

    HWND    hwnd = pWinInfo->swctl.hwnd;
    ULONG   pid, tid;

    memset(pWinInfo, 0, sizeof(PAGERWININFO));

    if (    (WinIsWindow(G_habDaemon, hwnd))
         && (WinQueryWindowProcess(hwnd,
                                   &pWinInfo->swctl.idProcess,
                                   &pWinInfo->tid))
       )
    {
        pWinInfo->swctl.hwnd = hwnd;

        WinQueryClassName(hwnd,
                          sizeof(pWinInfo->szClassName),
                          pWinInfo->szClassName);

        WinQueryWindowPos(hwnd, &pWinInfo->swp);

        brc = TRUE;     // can be changed again

        if (pWinInfo->swctl.idProcess)
        {
            if (pWinInfo->swctl.idProcess == G_pidDaemon)
                // belongs to XPager:
                pWinInfo->bWindowType = WINDOW_XWPDAEMON;
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
            }

            if (brc)
            {
                switch (pWinInfo->bWindowType)
                {
                    case 0:     // not found yet:
                    case WINDOW_XWPDAEMON:
                    case WINDOW_WPSDESKTOP:
                    {
                        ULONG ulStyle = WinQueryWindowULong(hwnd, QWL_STYLE);
                        if (    (!(pWinInfo->hsw = WinQuerySwitchHandle(hwnd, 0)))
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
                        {
                            if (!pWinInfo->bWindowType)
                                pWinInfo->bWindowType = WINDOW_NIL;
                        }
                        else
                        {
                            // window is in tasklist:
                            if (!pWinInfo->bWindowType)
                            {
                                // the minimize attribute prevails the "sticky" attribute,
                                // "sticky" prevails maximize, and maximize prevails normal
                                // V0.9.18 (2002-02-21) [lafaix]
                                if (pWinInfo->swp.fl & SWP_MINIMIZE)
                                    pWinInfo->bWindowType = WINDOW_MINIMIZE;
                                else if (pgrIsSticky(hwnd, pWinInfo->swctl.szSwtitle))
                                    pWinInfo->bWindowType = WINDOW_STICKY;
                                else if (pWinInfo->swp.fl & SWP_MAXIMIZE)
                                    pWinInfo->bWindowType = WINDOW_MAXIMIZE;
                                else
                                    pWinInfo->bWindowType = WINDOW_NORMAL;
                            }
                        }

                        // try to get switch entry in all cases;
                        // otherwise we have an empty switch title
                        // for some windows in the list, which will cause
                        // the new refresh thread to fire "window changed"
                        // every time
                        // V0.9.19 (2002-06-08) [umoeller]
                        if (pWinInfo->hsw)
                            WinQuerySwitchEntry(pWinInfo->hsw,
                                                &pWinInfo->swctl);
                    }
                }
            }
        }
    }

    return brc;
}

/*
 *@@ pgrCreateWinInfo:
 *      adds a new window to our window list.
 *
 *      Called upon XDM_WINDOWCHANGE in fnwpDaemonObject
 *      when either the hook or fntWinlistThread have
 *      determined that a new window might need to be added.
 *
 *      Returns what pgrGetWinInfo returned.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 *@@changed V0.9.7 (2001-01-21) [umoeller]: rewritten for linked list for wininfos
 *@@changed V0.9.19 (2002-05-28) [umoeller]: rewritten
 */

BOOL pgrCreateWinInfo(HWND hwnd)
{
    BOOL brc = FALSE;

    PAGERWININFO WinInfoTemp;
    WinInfoTemp.swctl.hwnd = hwnd;
    if (pgrGetWinInfo(&WinInfoTemp))
    {
        if (pgrLockWinlist())
        {
            PPAGERWININFO pWinInfo;

            // check if we have an entry for this window already
            if (!(pWinInfo = pgrFindWinInfo(hwnd, NULL)))
            {
                // not yet in list: create a new one
                pWinInfo = NEW(PAGERWININFO);
                ZERO(pWinInfo);
                pWinInfo->swctl.hwnd = hwnd;
                lstAppendItem(&G_llWinInfos, pWinInfo);
            }
            // else already present: refresh that one then

            memcpy(pWinInfo, &WinInfoTemp, sizeof(PAGERWININFO));

            brc = TRUE;

            pgrUnlockWinlist();
        }
    }

    return brc;
}

/*
 *@@ pgrBuildWinlist:
 *      (re)initializes the window list.
 *      This must get called exactly once when
 *      the window list is initialized.
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
            WinInfoTemp.swctl.hwnd = hwndTemp;
            if (pgrGetWinInfo(&WinInfoTemp))
            {
                // window found:
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
 *      Called upon XDM_WINDOWCHANGE in fnwpDaemonObject
 *      when either the hook or fntWinlistThread have
 *      determined that a window has died.
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
 *@@ pgrRefresh:
 *      attempts to refresh a window in our window
 *      list if it's already present. Will not add
 *      it as a new window though.
 *
 *@@added V0.9.19 (2002-06-02) [umoeller]
 */

BOOL pgrRefresh(HWND hwnd)
{
    BOOL brc = FALSE;
    if (pgrLockWinlist())
    {
        // check if we have an entry for this window already
        PPAGERWININFO    pWinInfo;
        if (pWinInfo = pgrFindWinInfo(hwnd,
                                      NULL))
        {
            // we have an entry:
            pgrGetWinInfo(pWinInfo);
            brc = TRUE;
        }

        pgrUnlockWinlist();
    }

    return brc;
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
    HWND    hwndClient;

    // check for system window list
    if (    (G_pHookData)
         && (hwnd == G_pHookData->hwndSwitchList)
       )
        return TRUE;

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
                    // _PmpfF(("checking %s", pcszSwtitle));
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

    return FALSE;
}

/*
 *@@ pgrIconChange:
 *
 *@@added V0.9.19 (2002-05-28) [umoeller]
 */

BOOL pgrIconChange(HWND hwnd,
                   HPOINTER hptr)
{
    BOOL brc = FALSE;
    BOOL fLocked = FALSE;

    TRY_LOUD(excpt1)
    {
        if (fLocked = pgrLockWinlist())
        {
            PLISTNODE pNode = lstQueryFirstNode(&G_llWinInfos);
            while (pNode)
            {
                // get next node NOW because we can delete pNode here
                // V0.9.12 (2001-05-31) [umoeller]
                PPAGERWININFO pWinInfo = (PPAGERWININFO)pNode->pItemData;

                if (pWinInfo->swctl.hwnd == hwnd)
                {
                    // check icons only if the switch list item
                    // is visible in the first place; otherwise,
                    // with some windows (e.g. mozilla), we fire
                    // out the "icon change" message before the
                    // xcenter winlist gets a chance of adding
                    // the switch list item to its private list
                    // because it suppresses entries without
                    // SWL_VISIBLE
                    if (    (pWinInfo->swctl.uchVisibility & SWL_VISIBLE)
                         && (pWinInfo->hptr != hptr)
                       )
                    {
                        pWinInfo->hptr = hptr;
                        brc = TRUE;
                    }
                    break;
                }

                pNode = pNode->pNext;
            }
        }
    }
    CATCH(excpt1)
    {
    } END_CATCH();

    if (fLocked)
    {
        pgrUnlockWinlist();
        fLocked = FALSE;
    }

    return brc;
}

/*
 *@@ pgrQueryWinList:
 *      implementation for XDM_QUERYWINLIST.
 *
 *@@added V0.9.19 (2002-05-28) [umoeller]
 */

PSWBLOCK pgrQueryWinList(ULONG pid)
{
    BOOL brc = FALSE;
    BOOL fLocked = FALSE;
    PSWBLOCK pSwblockReturn = NULL,
             pSwblock;

    TRY_LOUD(excpt1)
    {
        if (fLocked = pgrLockWinlist())
        {
            PLISTNODE pNode;
            ULONG cWindows, cbSwblock;
            if (    (cWindows = lstCountItems(&G_llWinInfos))
                 && (cbSwblock = (cWindows * sizeof(SWENTRY)) + sizeof(HSWITCH))
                 && (!DosAllocSharedMem((PVOID*)&pSwblock,
                                        NULL,
                                        cbSwblock,
                                        PAG_COMMIT | OBJ_GIVEABLE | PAG_READ | PAG_WRITE))
               )
            {
                ULONG ul = 0;
                pSwblock->cswentry = cWindows;
                pNode = lstQueryFirstNode(&G_llWinInfos);
                while (pNode)
                {
                    PPAGERWININFO pWinInfo = (PPAGERWININFO)pNode->pItemData;

                    // return switch handle
                    pSwblock->aswentry[ul].hswitch = pWinInfo->hsw;

                    // return SWCNTRL
                    memcpy(&pSwblock->aswentry[ul].swctl,
                           &pWinInfo->swctl,
                           sizeof(SWCNTRL));

                    // override hwndIcon with the frame icon if we
                    // queried one in the background thread
                    pSwblock->aswentry[ul].swctl.hwndIcon = pWinInfo->hptr;

                    pNode = pNode->pNext;
                    ++ul;
                }

                if (!DosGiveSharedMem(pSwblock,
                                      pid,
                                      PAG_READ | PAG_WRITE))
                {
                    pSwblockReturn = pSwblock;
                }

                DosFreeMem(pSwblock);
            }
        }
    }
    CATCH(excpt1)
    {
    } END_CATCH();

    if (fLocked)
    {
        pgrUnlockWinlist();
        fLocked = FALSE;
    }

    return pSwblockReturn;
}

/*
 *@@ CheckWindow:
 *      checks one window from the system switchlist
 *      against our private list.
 *
 *      We only hold the winlist mutex (for the private
 *      list) locked for a very short time in order not
 *      to block the system since we might send msgs
 *      from here.
 *
 *      This fires XDM_WINDOWCHANGE to the daemon object
 *      window if the window changed compared to our
 *      list.
 *
 *@@added V0.9.19 (2002-05-28) [umoeller]
 */

VOID CheckWindow(HAB hab,
                 PSWCNTRL pCtrlThis)
{
    BOOL fLocked = FALSE;

    TRY_LOUD(excpt1)
    {
        if (fLocked = pgrLockWinlist())
        {
            // check if the entry is in the list already...

            // 1) rule out obvious non-windows
            if (    (pCtrlThis->hwnd)
                 && WinIsWindow(hab, pCtrlThis->hwnd)
                 // don't monitor invisible switch list entries here
                 // or we'll bomb the clients with entries for the
                 // first PMSHELL.EXE and the like
                 && (pCtrlThis->uchVisibility & SWL_VISIBLE)
               )
            {
                PPAGERWININFO pInfo;
                PLISTNODE pNode;

                if (!(pInfo = pgrFindWinInfo(pCtrlThis->hwnd,
                                             (PVOID*)&pNode)))
                {
                    // window is not in list: add it then
                    /*
                    #ifdef __DEBUG__
                        CHAR szClass[30];
                        WinQueryClassName(pCtrlThis->hwnd, sizeof(szClass), szClass);
                        _PmpfF(("hwnd 0x%lX is not in list (%s, %s)",
                               pCtrlThis->hwnd,
                            pCtrlThis->szSwtitle,
                            szClass));
                    #endif
                    */

                    WinPostMsg(G_pHookData->hwndDaemonObject,
                               XDM_WINDOWCHANGE,
                               (MPARAM)pCtrlThis->hwnd,
                               (MPARAM)WM_CREATE);
                }
                else
                {
                    HWND        hwnd;
                    HPOINTER    hptrOld,
                                hptrNew;
                    // OK, this list node is still in the switchlist:
                    // check if all the data is still valid
                    if (strcmp(pCtrlThis->szSwtitle, pInfo->swctl.szSwtitle))
                    {
                        // session title changed:
                        #ifdef DEBUG_WINDOWLIST
                        _PmpfF(("title changed hwnd 0x%lX (old %s, new %s)",
                            pCtrlThis->hwnd,
                            pInfo->swctl.szSwtitle,
                            pCtrlThis->szSwtitle
                            ));
                        #endif

                        memcpy(pInfo->swctl.szSwtitle,
                               pCtrlThis->szSwtitle,
                               sizeof(pCtrlThis->szSwtitle));
                        WinPostMsg(G_pHookData->hwndDaemonObject,
                                   XDM_WINDOWCHANGE,
                                   (MPARAM)pCtrlThis->hwnd,
                                   (MPARAM)WM_SETWINDOWPARAMS);
                    }

                    hwnd = pCtrlThis->hwnd;
                    hptrOld = pInfo->hptr;

                    // WinSendMsg below can block so unlock the list now
                    pgrUnlockWinlist();
                    fLocked = FALSE;

                    // check icon
                    hptrNew = (HPOINTER)WinSendMsg(hwnd,
                                                   WM_QUERYICON,
                                                   0,
                                                   0);
                    if (hptrNew != hptrOld)
                    {
                        // icon changed:
                        #ifdef DEBUG_WINDOWLIST
                        _PmpfF(("icon changed hwnd 0x%lX", pCtrlThis->hwnd));
                        #endif

                        WinPostMsg(G_pHookData->hwndDaemonObject,
                                   XDM_ICONCHANGE,
                                   (MPARAM)hwnd,
                                   (MPARAM)hptrNew);
                    }
                }
            }
        }
    }
    CATCH(excpt1)
    {
    } END_CATCH();

    if (fLocked)
    {
        pgrUnlockWinlist();
        fLocked = FALSE;
    }
}

/*
 *@@ fntWinlistThread:
 *      window list thread started when the first
 *      XDM_ADDWINLISTWATCH comes in.
 *
 *      This scans the system switch list and updates
 *      our private window list in the background,
 *      making sure that everything is always up
 *      to date. This is required to get the icons
 *      right, since sending WM_QUERYICON to a
 *      frame can block with misbehaving apps like
 *      PMMail.
 *
 *@@added V0.9.19 (2002-05-28) [umoeller]
 */

VOID _Optlink fntWinlistThread(PTHREADINFO pti)
{
    while (!pti->fExit)
    {
        ULONG   cItems,
                cbSwblock;
        PSWBLOCK pSwBlock;
        BOOL fLocked = FALSE;

        if (    (cItems = WinQuerySwitchList(pti->hab, NULL, 0))
             && (cbSwblock = (cItems * sizeof(SWENTRY)) + sizeof(HSWITCH))
             && (pSwBlock = (PSWBLOCK)malloc(cbSwblock))
           )
        {
            if (cItems = WinQuerySwitchList(pti->hab, pSwBlock, cbSwblock))
            {
                // run through all switch list entries
                ULONG ul;
                for (ul = 0;
                     ul < pSwBlock->cswentry;
                     ++ul)
                {
                    // and compare each entry with our private list
                    CheckWindow(pti->hab,
                                &pSwBlock->aswentry[ul].swctl);
                }
            }

            free(pSwBlock);
        }

        if (pti->fExit)
            break;

        // now clean out non-existent windows
        TRY_LOUD(excpt1)
        {
            if (fLocked = pgrLockWinlist())
            {
                PLISTNODE pNode = lstQueryFirstNode(&G_llWinInfos);
                while (pNode)
                {
                    PPAGERWININFO pWinInfo = (PPAGERWININFO)pNode->pItemData;
                    if (!WinIsWindow(pti->hab,
                                     pWinInfo->swctl.hwnd))
                        WinPostMsg(G_pHookData->hwndDaemonObject,
                                   XDM_WINDOWCHANGE,
                                   (MPARAM)pWinInfo->swctl.hwnd,
                                   (MPARAM)WM_DESTROY);

                    pNode = pNode->pNext;
                }
            }
        }
        CATCH(excpt1)
        {
        } END_CATCH();

        if (fLocked)
        {
            pgrUnlockWinlist();
            fLocked = FALSE;
        }


        DosSleep(500);
    }
}


