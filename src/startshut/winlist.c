
/*
 *@@sourcefile winlist.c:
 *      debug code to show the complete window (switch) list in
 *      a PM container window.
 *
 *@@header "startshut\shutdown.h"
 */

/*
 *      Copyright (C) 2000 Ulrich M”ller.
 *      This file is part of the XWorkplace source package.
 *      XWorkplace is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published
 *      by the Free Software Foundation, in version 2 as it comes in the
 *      "COPYING" file of the XWorkplace main distribution.
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 */

/*
 *@@todo:
 *
 */

/*
 *  Suggested #include order:
 *  1)  os2.h
 *  2)  C library headers
 *  3)  setup.h (code generation and debugging options)
 *  4)  headers in helpers\
 *  5)  at least one SOM implementation header (*.ih)
 *  6)  dlgids.h, headers in shared\ (as needed)
 *  7)  headers in implementation dirs (e.g. filesys\, as needed)
 *  8)  #pragma hdrstop and then more SOM headers which crash with precompiled headers
 */

#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_WIN
#include <os2.h>

// C library headers
#include <stdio.h>              // needed for except.h
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h
#include <stdarg.h>

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\cnrh.h"
#include "helpers\except.h"
#include "helpers\stringh.h"
#include "helpers\winh.h"

// SOM headers which don't crash with prec. header files
#include "xfobj.ih"

// XWorkplace implementation headers
#include "shared\kernel.h"              // XWorkplace Kernel
#include "startshut\shutdown.h"         // XWorkplace eXtended Shutdown

// other SOM headers
#pragma hdrstop

typedef struct _WINLISTRECORD
{
    RECORDCORE  recc;

    ULONG       ulIndex;

    HSWITCH     hSwitch;

    HWND        hwnd;
    CHAR        szHWND[20];
    PSZ         pszHWND;

    HPROGRAM    hPgm;
    PID         pid;
    ULONG       sid;
    ULONG       ulVisibility;

    CHAR        szSwTitle[200];
    PSZ         pszSwTitle;         // points to szSwTitle

    CHAR        szWinTitle[200];
    PSZ         pszWinTitle;        // points to szWinTitle

    CHAR        szObject[30];
    PSZ         pszObject;

    PSZ         pszObjectTitle;

    CHAR        szClosable[30];
    PSZ         pszClosable;        // points to szClosable
} WINLISTRECORD, *PWINLISTRECORD;

#define ID_WINLISTCNR       1001

/*
 *@@ winlCreateRecords:
 *
 *@@added V0.9.4 (2000-07-15) [umoeller]
 */

ULONG winlCreateRecords(HWND hwndCnr)
{
    PSWBLOCK        pSwBlock   = NULL;         // Pointer to information returned
    ULONG           ul,
                    cbItems    = 0,            // Number of items in list
                    ulBufSize  = 0;            // Size of buffer for information

    HAB             hab = WinQueryAnchorBlock(hwndCnr);

    PWINLISTRECORD  pFirstRec = NULL,
                    pRecThis = NULL;
    ULONG           ulIndex = 0;

    SHUTDOWNCONSTS  SDConsts;

    // get all the tasklist entries into a buffer
    cbItems = WinQuerySwitchList(hab, NULL, 0);
    ulBufSize = (cbItems * sizeof(SWENTRY)) + sizeof(HSWITCH);
    pSwBlock = (PSWBLOCK)malloc(ulBufSize);
    cbItems = WinQuerySwitchList(hab, pSwBlock, ulBufSize);

    xsdGetShutdownConsts(&SDConsts);

    // allocate records
    pFirstRec = (PWINLISTRECORD)cnrhAllocRecords(hwndCnr,
                                                 sizeof(WINLISTRECORD),
                                                 pSwBlock->cswentry);
    if (pFirstRec)
    {
        pRecThis = pFirstRec;

        // loop through all the tasklist entries
        for (ul = 0;
             (ul < pSwBlock->cswentry) && (pRecThis);
             ul++)
        {
            WPObject    *pObject;
            LONG        lClosable;

            pRecThis->ulIndex = ulIndex++;

            pRecThis->hSwitch = pSwBlock->aswentry[ul].hswitch;

            pRecThis->hwnd = pSwBlock->aswentry[ul].swctl.hwnd;
            sprintf(pRecThis->szHWND, "0x%lX", pSwBlock->aswentry[ul].swctl.hwnd);
            pRecThis->pszHWND = pRecThis->szHWND;

            pRecThis->hPgm = pSwBlock->aswentry[ul].swctl.hprog;
            pRecThis->pid = pSwBlock->aswentry[ul].swctl.idProcess;
            pRecThis->sid = pSwBlock->aswentry[ul].swctl.idSession;
            pRecThis->ulVisibility = pSwBlock->aswentry[ul].swctl.uchVisibility;

            strhncpy0(pRecThis->szSwTitle,
                      pSwBlock->aswentry[ul].swctl.szSwtitle,
                      sizeof(pRecThis->szSwTitle));
            pRecThis->pszSwTitle = pRecThis->szSwTitle;

            WinQueryWindowText(pSwBlock->aswentry[ul].swctl.hwnd,
                               sizeof(pRecThis->szWinTitle),
                               pRecThis->szWinTitle);
            pRecThis->pszWinTitle = pRecThis->szWinTitle;

            lClosable = xsdIsClosable(hab,
                                      &SDConsts,
                                      &pSwBlock->aswentry[ul],
                                      &pObject);
            sprintf(pRecThis->szClosable, "%d", lClosable);
            pRecThis->pszClosable = pRecThis->szClosable;

            sprintf(pRecThis->szObject, "0x%lX", pObject);
            pRecThis->pszObject = pRecThis->szObject;

            if (pObject)
                pRecThis->pszObjectTitle = _wpQueryTitle(pObject);

            pRecThis = (PWINLISTRECORD)pRecThis->recc.preccNextRecord;
        }
    }

    cnrhInsertRecords(hwndCnr,
                      NULL,         // parent
                      (PRECORDCORE)pFirstRec,
                      TRUE,
                      NULL,
                      CRA_RECORDREADONLY,
                      ulIndex);

    free(pSwBlock);

    return (ulIndex);
}

/*
 *@@ winl_fnwpWinList:
 *
 *@@added V0.9.4 (2000-07-15) [umoeller]
 */

MRESULT EXPENTRY winl_fnwpWinList(HWND hwndClient, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    switch (msg)
    {
        case WM_CREATE:
        {
            TRY_LOUD(excpt1, NULL)
            {
                // PCREATESTRUCT pcs = (PCREATESTRUCT)mp2;
                HWND hwndCnr;
                hwndCnr = WinCreateWindow(hwndClient,        // parent
                                          WC_CONTAINER,
                                          "",
                                          WS_VISIBLE | CCS_MINIICONS | CCS_READONLY | CCS_SINGLESEL,
                                          0, 0, 0, 0,
                                          hwndClient,        // owner
                                          HWND_TOP,
                                          ID_WINLISTCNR,
                                          NULL, NULL);
                if (hwndCnr)
                {
                    XFIELDINFO      xfi[20];
                    PFIELDINFO      pfi = NULL;
                    PWINLISTRECORD  pMemRecordFirst;
                    int             i = 0;

                    ULONG           ulTotalItems = 0,
                                    ulAllocatedItems = 0,
                                    ulFreedItems = 0;
                    ULONG           ulTotalBytes = 0,
                                    ulAllocatedBytes = 0,
                                    ulFreedBytes = 0;

                    // set up cnr details view
                    xfi[i].ulFieldOffset = FIELDOFFSET(WINLISTRECORD, ulIndex);
                    xfi[i].pszColumnTitle = "No.";
                    xfi[i].ulDataType = CFA_ULONG;
                    xfi[i++].ulOrientation = CFA_RIGHT;

                    xfi[i].ulFieldOffset = FIELDOFFSET(WINLISTRECORD, hSwitch);
                    xfi[i].pszColumnTitle = "hSw";
                    xfi[i].ulDataType = CFA_ULONG;
                    xfi[i++].ulOrientation = CFA_RIGHT;

                    xfi[i].ulFieldOffset = FIELDOFFSET(WINLISTRECORD, pszSwTitle);
                    xfi[i].pszColumnTitle = "Switch title";
                    xfi[i].ulDataType = CFA_STRING;
                    xfi[i++].ulOrientation = CFA_LEFT;

                    xfi[i].ulFieldOffset = FIELDOFFSET(WINLISTRECORD, pszWinTitle);
                    xfi[i].pszColumnTitle = "Win title";
                    xfi[i].ulDataType = CFA_STRING;
                    xfi[i++].ulOrientation = CFA_LEFT;

                    xfi[i].ulFieldOffset = FIELDOFFSET(WINLISTRECORD, pszHWND);
                    xfi[i].pszColumnTitle = "hwnd";
                    xfi[i].ulDataType = CFA_STRING;
                    xfi[i++].ulOrientation = CFA_RIGHT;

                    xfi[i].ulFieldOffset = FIELDOFFSET(WINLISTRECORD, pid);
                    xfi[i].pszColumnTitle = "PID";
                    xfi[i].ulDataType = CFA_ULONG;
                    xfi[i++].ulOrientation = CFA_RIGHT;

                    xfi[i].ulFieldOffset = FIELDOFFSET(WINLISTRECORD, sid);
                    xfi[i].pszColumnTitle = "SID";
                    xfi[i].ulDataType = CFA_ULONG;
                    xfi[i++].ulOrientation = CFA_RIGHT;

                    xfi[i].ulFieldOffset = FIELDOFFSET(WINLISTRECORD, ulVisibility);
                    xfi[i].pszColumnTitle = "Vis";
                    xfi[i].ulDataType = CFA_ULONG;
                    xfi[i++].ulOrientation = CFA_RIGHT;

                    xfi[i].ulFieldOffset = FIELDOFFSET(WINLISTRECORD, pszClosable);
                    xfi[i].pszColumnTitle = "Close";
                    xfi[i].ulDataType = CFA_STRING;
                    xfi[i++].ulOrientation = CFA_RIGHT;

                    xfi[i].ulFieldOffset = FIELDOFFSET(WINLISTRECORD, pszObject);
                    xfi[i].pszColumnTitle = "Obj";
                    xfi[i].ulDataType = CFA_STRING;
                    xfi[i++].ulOrientation = CFA_RIGHT;

                    xfi[i].ulFieldOffset = FIELDOFFSET(WINLISTRECORD, pszObjectTitle);
                    xfi[i].pszColumnTitle = "Obj Title";
                    xfi[i].ulDataType = CFA_STRING;
                    xfi[i++].ulOrientation = CFA_RIGHT;

                    pfi = cnrhSetFieldInfos(hwndCnr,
                                            &xfi[0],
                                            i,             // array item count
                                            TRUE,          // no draw lines
                                            3);            // return column

                    {
                        PSZ pszFont = "9.WarpSans";
                        WinSetPresParam(hwndCnr,
                                        PP_FONTNAMESIZE,
                                        strlen(pszFont),
                                        pszFont);
                    }

                    winlCreateRecords(hwndCnr);

                    BEGIN_CNRINFO()
                    {
                        cnrhSetView(CV_DETAIL | CV_MINI | CA_DETAILSVIEWTITLES
                                        | CA_DRAWICON
                                    /* | CA_CONTAINERTITLE | CA_TITLEREADONLY
                                        | CA_TITLESEPARATOR | CA_TITLELEFT*/ );
                        cnrhSetSplitBarAfter(pfi);
                        cnrhSetSplitBarPos(250);
                    } END_CNRINFO(hwndCnr);

                    WinSetFocus(HWND_DESKTOP, hwndCnr);
                }
            }
            CATCH(excpt1) {} END_CATCH();

            mrc = WinDefWindowProc(hwndClient, msg, mp1, mp2);
        break; }

        case WM_WINDOWPOSCHANGED:
        {
            PSWP pswp = (PSWP)mp1;
            mrc = WinDefWindowProc(hwndClient, msg, mp1, mp2);
            if (pswp->fl & SWP_SIZE)
            {
                WinSetWindowPos(WinWindowFromID(hwndClient, ID_WINLISTCNR), // cnr
                                HWND_TOP,
                                0, 0, pswp->cx, pswp->cy,
                                SWP_SIZE | SWP_MOVE | SWP_SHOW);
            }
        break; }

        case WM_CONTROL:
        {
            USHORT usItemID = SHORT1FROMMP(mp1),
                   usNotifyCode = SHORT2FROMMP(mp1);
            if (usItemID == ID_WINLISTCNR)       // cnr
            {
                switch (usNotifyCode)
                {
                    case CN_CONTEXTMENU:
                    {
                        PWINLISTRECORD precc = (PWINLISTRECORD)mp2;
                        if (precc == NULL)
                        {
                            // whitespace
                            HWND hwndMenu = WinCreateMenu(HWND_DESKTOP,
                                                          NULL); // no menu template
                            winhInsertMenuItem(hwndMenu,
                                               MIT_END,
                                               1001,
                                               "Sort by index",
                                               MIS_TEXT, 0);
                            winhInsertMenuItem(hwndMenu,
                                               MIT_END,
                                               1002,
                                               "Sort by source file",
                                               MIS_TEXT, 0);
                            cnrhShowContextMenu(WinWindowFromID(hwndClient, ID_WINLISTCNR),
                                                NULL,       // record
                                                hwndMenu,
                                                hwndClient);
                        }
                    }
                }
            }
        break; }

        case WM_COMMAND:
/*             switch (SHORT1FROMMP(mp1))
            {
                case 1001:  // sort by index
                    WinSendMsg(WinWindowFromID(hwndClient, ID_WINLISTCNR),
                               CM_SORTRECORD,
                               (MPARAM)mnu_fnCompareIndex,
                               0);
                break;

                case 1002:  // sort by source file
                    WinSendMsg(WinWindowFromID(hwndClient, ID_WINLISTCNR),
                               CM_SORTRECORD,
                               (MPARAM)mnu_fnCompareSourceFile,
                               0);
                break;
            } */
        break;

        case WM_CLOSE:
            WinDestroyWindow(WinWindowFromID(hwndClient, ID_WINLISTCNR));
            WinDestroyWindow(WinQueryWindow(hwndClient, QW_PARENT));
        break;

        default:
            mrc = WinDefWindowProc(hwndClient, msg, mp1, mp2);
    }

    return (mrc);
}

/*
 *@@ winlCreateWinListWindow:
 *
 *@@added V0.9.4 (2000-07-15) [umoeller]
 */

HWND winlCreateWinListWindow(VOID)
{
    HWND hwndFrame = NULLHANDLE;

    ULONG flStyle = FCF_TITLEBAR | FCF_SYSMENU | FCF_HIDEMAX
                    | FCF_SIZEBORDER | FCF_SHELLPOSITION
                    | FCF_NOBYTEALIGN | FCF_TASKLIST;
    if (WinRegisterClass(WinQueryAnchorBlock(HWND_DESKTOP),
                         "XWPWinList",
                         winl_fnwpWinList, 0L, 0))
    {
        HWND hwndClient;
        hwndFrame = WinCreateStdWindow(HWND_DESKTOP,
                                       0L,
                                       &flStyle,
                                       "XWPWinList",
                                       "Window List",
                                       0L,
                                       NULLHANDLE,     // resource
                                       0,
                                       &hwndClient);
        if (hwndFrame)
        {
            WinSetWindowPos(hwndFrame,
                            HWND_TOP,
                            0, 0, 0, 0,
                            SWP_ZORDER | SWP_SHOW | SWP_ACTIVATE);
        }
    }

    return (hwndFrame);
}

