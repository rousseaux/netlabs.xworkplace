
/*
 *@@sourcefile mptrprop.c:
 *
 *      This file is ALL new with V0.9.4.
 *
 *@@added V0.9.4 [umoeller]
 *@@header "pointers\mptrprop.h"
 */

/*
 *      Copyright (C) 1996-2000 Christian Langanke.
 *      Copyright (C) 2000 Ulrich Mîller.
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

// C Runtime
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

// OS/2 Toolkit
#define INCL_ERRORS
#define INCL_PM
#define INCL_WIN
#define INCL_DOS
#define INCL_DOSDEVIOCTL
#define INCL_DOSMISC
#include <os2.h>

// generic headers
#include "setup.h"              // code generation and debugging options

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS

#include "pointers\mptrcnr.h"
#include "pointers\mptrprop.h"
#include "pointers\mptrutil.h"
#include "pointers\mptrptr.h"
#include "pointers\mptrppl.h"
#include "pointers\mptrset.h"
#include "pointers\mptranim.h"
#include "pointers\wmuser.h"
#include "pointers\wmuser.h"
#include "pointers\macros.h"
#include "pointers\debug.h"

#include "..\..\001\dll\r_amptr001.h"

#define TABTEXT_SIZEX                 80
#define TABTEXT_SIZEY                 22
#define PAGEBUTTON_SIZEXY             21
#define NOTEBOOK_SIZEX               500
#define NOTEBOOK_SIZEY               400

#define WINDOW_POSADJUSTX          12
#define WINDOW_POSADJUSTY          15

//
#define  NBPAGEID_ACTION 1
#define  NBPAGEID_HIDE   2
#define  NBPAGEID_INIT   3

//      global vars
static PFNWP pfnwpFrameProc = 0;

CHAR szFileTypeAll[MAX_RES_STRLEN];
CHAR szFileTypePointer[MAX_RES_STRLEN];
CHAR szFileTypeCursor[MAX_RES_STRLEN];
CHAR szFileTypeWinAnimation[MAX_RES_STRLEN];
CHAR szFileTypeAnimouse[MAX_RES_STRLEN];

// structs
typedef struct _VALIDATERESULT
{
    BOOL fInvalid;
    ULONG ulFocusId;
    ULONG ulTitleId;
    ULONG ulMessageId;
    HMODULE hmodResource;
}
VALIDATERESULT, *PVALIDATERESULT;

// private prototypes
MRESULT EXPENTRY FrameWindowProc(HWND, ULONG, MPARAM, MPARAM);
MRESULT EXPENTRY PageDlgWindowProc(HWND, ULONG, MPARAM, MPARAM);

/*⁄ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒø
 *≥ Name      : ExecPropertiesNotebook                                     ≥
 *≥ Comment   :                                                            ≥
 *≥ Author    : C.Langanke                                                 ≥
 *≥ Date      : 07.02.1998                                                 ≥
 *≥ Update    : 07.02.1998                                                 ≥
 *≥ called by : diverse                                                    ≥
 *≥ calls     : Win*                                                       ≥
 *≥ Input     : ###                                                        ≥
 *≥ Tasks     : ###                                                        ≥
 *≥ returns   : BOOL - success flag                                        ≥
 *¿ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒŸ
 */

BOOL ExecPropertiesNotebook
 (
     HWND hwnd,
     HMODULE hmodResource,
     PSETTINGSHANDLERDATA pshd
)
{
    HWND hwndFrame;
    HWND hwndTitlebar;
    HWND hwndNotebook;
    HWND hwndHelp;
    CHAR szHelpLibrary[_MAX_PATH];
    CHAR szTitle[64];


    HAB hab = WinQueryAnchorBlock(hwnd);
    FRAMECDATA fcd;
    BOOL fSuccess = TRUE;
    HWND hwndAnimationPage = 0;
    HWND hwndHidePage = 0;
    HWND hwndDragDropPage = 0;
    HWND hwndInitPage = 0;

    HMQ hmq = 0;
    QMSG qmsg;

    PSZ pszNotebookFont = "8.Helv";
    PSZ pszTitlebarFont = "9.WarpSans Bold";

    POINTL ptl;

    do
    {

        // for exe loader:
        // disable main window and create own msg queue
        if (!pshd->somSelf)
        {
            WinEnableWindow(hwnd, FALSE);
            hmq = WinCreateMsgQueue(hab, 0);
        }

        // create frame win
        fcd.cb = sizeof(FRAMECDATA);
        fcd.flCreateFlags = FCF_TITLEBAR |
            FCF_SYSMENU |
            FCF_DLGBORDER;

        fcd.hmodResources = 0;
        fcd.idResources = 0;

        // load title
        LOADSTRING(IsWARP3()? IDDLG_DLG_CNRSETTINGS_230 : IDDLG_DLG_CNRSETTINGS, szTitle);

        // create frame
        hwndFrame = WinCreateWindow(HWND_DESKTOP,
                                    WC_FRAME,
                                    szTitle,
                                    0,
                                    0, 0, 0, 0,
                                    hwnd,
                                    HWND_TOP,
                                    IDDLG_DLG_CNRSETTINGS,
                                    &fcd,
                                    NULL);

        if (hwndFrame == NULLHANDLE)
        {
            fSuccess = FALSE;
            break;
        }

        // subclass frame window
        pfnwpFrameProc = WinSubclassWindow(hwndFrame, (PFNWP) FrameWindowProc);

        // create notebook
        hwndNotebook = WinCreateWindow(hwndFrame,
                                       WC_NOTEBOOK,
                                       NULL,
                                       BKS_BACKPAGESBR | BKS_MAJORTABRIGHT,
                                       0, 0, 0, 0,
                                       hwndFrame,
                                       HWND_BOTTOM,
                                       FID_CLIENT,
                                       NULL,
                                       NULL);

        if (hwndNotebook == NULLHANDLE)
        {
            fSuccess = FALSE;
            break;
        }


        // setup the notebook
        SETUPNOTEBOOK(hwndNotebook, TABTEXT_SIZEX, TABTEXT_SIZEY, PAGEBUTTON_SIZEXY);

        // -----------------------------

        pszNotebookFont = "8.Helv";
        fSuccess = WinSetPresParam(hwndNotebook, PP_FONTNAMESIZE, strlen(pszNotebookFont), pszNotebookFont);

        if (IsWARP3())
            pszTitlebarFont = "8.Helv";
        else
            pszTitlebarFont = "9.WarpSans Bold";

        hwndTitlebar = WinWindowFromID(hwndFrame, FID_TITLEBAR);
        fSuccess = WinSetPresParam(hwndTitlebar, PP_FONTNAMESIZE, strlen(pszTitlebarFont), pszTitlebarFont);

        // -----------------------------

        // setup notebook pages and save initial sizes
        hwndAnimationPage = CreateNotebookPage(hwndNotebook,
                                               hmodResource,
                                               IDDLG_NBANIMATION,
                                               IDTAB_NBANIMATION,
                                               0,
                                               PageDlgWindowProc,
                                               pshd);

        hwndHidePage = CreateNotebookPage(hwndNotebook,
                                          hmodResource,
                                          IDDLG_NBHIDE,
                                          IDTAB_NBHIDE,
                                          0,
                                          PageDlgWindowProc,
                                          pshd);

        hwndDragDropPage = CreateNotebookPage(hwndNotebook,
                                              hmodResource,
                                              IDDLG_NBDRAGDROP,
                                              IDTAB_NBDRAGDROP,
                                              0,
                                              PageDlgWindowProc,
                                              pshd);

        hwndInitPage = CreateNotebookPage(hwndNotebook,
                                          hmodResource,
                                          IDDLG_NBINIT,
                                          IDTAB_NBINIT,
                                          0,
                                          PageDlgWindowProc,
                                          pshd);

        if ((hwndAnimationPage == NULLHANDLE) ||
            (hwndHidePage == NULLHANDLE) ||
            (hwndDragDropPage == NULLHANDLE) ||
            (hwndInitPage == NULLHANDLE))
        {
            fSuccess = FALSE;
            break;
        }

        // window position so setzen,
        // da· die Maus auf dem SystemmenÅ erscheint
        if (WinQueryPointerPos(HWND_DESKTOP, &ptl))
        {
            ptl.x = (ptl.x < WINDOW_POSADJUSTX) ? 0 : ptl.x - WINDOW_POSADJUSTX;
            ptl.y = (ptl.y < NOTEBOOK_SIZEY) ? 0 : ptl.y - NOTEBOOK_SIZEY + WINDOW_POSADJUSTY;
            WinSetWindowPos(hwndFrame, 0, ptl.x, ptl.y, 0, 0, SWP_MOVE);
        }

        // window size setzen
        WinSetWindowPos(hwndFrame, 0, 0, 0, NOTEBOOK_SIZEX, NOTEBOOK_SIZEY, SWP_SIZE | SWP_SHOW);


        // now execute window
        if (!pshd->somSelf)
        {
            // for exe loader:
            // use own msg-queue, because SC_CLOSE would
            // close the complete msg queue, so also the
            // main window

            // does work only, if the notebook frame has the focus.
            // if a control has the focus, the SC_CLOSE still
            // closes also the main window

            while (WinGetMsg(hab, &qmsg, NULLHANDLE, 0, 0))
            {
                WinDispatchMsg(hab, &qmsg);
            }
            WinDestroyMsgQueue(hmq);
            WinEnableWindow(hwnd, TRUE);
        }
        else
            WinProcessDlg(hwndFrame);

    }
    while (FALSE);

// cleanup

    if (hwndAnimationPage)
        WinDestroyWindow(hwndAnimationPage);
    if (hwndHidePage)
        WinDestroyWindow(hwndHidePage);
    if (hwndDragDropPage)
        WinDestroyWindow(hwndDragDropPage);
    if (hwndInitPage)
        WinDestroyWindow(hwndInitPage);
    if (hwndFrame)
        WinDestroyWindow(hwndFrame);

    return fSuccess;

}

/*⁄ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒø
 *≥ Name      : CreateNoteBookPage                                         ≥
 *≥ Comment   : This function can only insert pages with major tabs        ≥
 *≥             at the end of the notebook. No styles other than default   ≥
 *≥             can be used.                                               ≥
 *≥ Author    : C.Langanke                                                 ≥
 *≥ Date      : 07.02.1998                                                 ≥
 *≥ Update    : 07.02.1998                                                 ≥
 *≥ called by : diverse                                                    ≥
 *≥ calls     : Win*                                                       ≥
 *≥ Input     : HWND      - handle of notebook frame                       ≥
 *≥             HLIB      - handle of resource library, 0 for EXE          ≥
 *≥             ULONG     - res id of dialog                               ≥
 *≥             ULONG     - string res id of tab text                      ≥
 *≥             PFNWP     - pointer to window proc for page                ≥
 *≥             PPAGEDATA - pointer to pagedata                            ≥
 *≥ Tasks     : - load dialog for notebook page                            ≥
 *≥             - insert a new page                                        ≥
 *≥             - setup tab text                                           ≥
 *≥             - associate page to loaded dialog                          ≥
 *≥ returns   : HWND - handle of the inserted window                       ≥
 *¿ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒŸ
 */

HWND CreateNotebookPage
 (
     HWND hwndNotebook,
     HMODULE hmodResource,
     ULONG ulResIdDialog,
     ULONG ulResIdMajorTabText,
     ULONG ulResIdMinorTabText,
     PFNWP DlgWindowProc,
     PVOID pvParams
)
{
    HWND hwndNoteBookPage = 0;
    ULONG ulPageId = 0;
    BOOL fSuccess = TRUE;
    HAB hab = WinQueryAnchorBlock(hwndNotebook);
    ULONG ulTabStyle = 0;
    ULONG ulTabResId = 0;
    CHAR szTabText[32];

    do
    {
        // determine tab style
        // ignore minor tab if major tab is given
        if (ulResIdMinorTabText > 0)
        {
            ulTabStyle = BKA_MINOR;
            ulTabResId = ulResIdMinorTabText;
        }
        if (ulResIdMajorTabText > 0)
        {
            ulTabStyle = BKA_MAJOR;
            ulTabResId = ulResIdMajorTabText;
        }


        // create notebook page window
        hwndNoteBookPage = WinLoadDlg(hwndNotebook,
                                      hwndNotebook,
                                      DlgWindowProc,
                                      hmodResource,
                                      ulResIdDialog,
                                      pvParams);

        // is page there ?
        fSuccess = (hwndNoteBookPage != NULLHANDLE);
        if (!fSuccess)
            break;

        // insert page and store id
        ulPageId = (ULONG) WinSendMsg(hwndNotebook, BKM_INSERTPAGE, (MPARAM) NULL,
                                   MPFROM2SHORT(ulTabStyle | BKA_AUTOPAGESIZE,
                                                BKA_LAST));
        // set tab text
        if (ulTabResId > 0)
        {
            LOADSTRING(ulTabResId, szTabText);
            WinSendMsg(hwndNotebook, BKM_SETTABTEXT,
                       MPFROMLONG(ulPageId),
                       (MPARAM) szTabText);
        }

        // associate pages
        fSuccess = (BOOL) WinSendMsg(hwndNotebook,
                                     BKM_SETPAGEWINDOWHWND,
                                     MPFROMLONG(ulPageId),
                                     MPFROMHWND(hwndNoteBookPage));

    }
    while (FALSE);


// clean ip on error
    if (!fSuccess)
    {
        if (hwndNoteBookPage)
        {
            WinDestroyWindow(hwndNoteBookPage);
            hwndNoteBookPage = 0;
        }
    }

    return hwndNoteBookPage;
}

/*⁄ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒø
 *≥ Name      : NotebookFrameWindowProc                                    ≥
 *≥ Comment   : subclass of frame dialog proc for handling SC_CLOSE        ≥
 *≥ Author    : C.Langanke                                                 ≥
 *≥ Date      : 07.02.1998                                                 ≥
 *≥ Update    : 07.02.1998                                                 ≥
 *≥ called by : diverse                                                    ≥
 *≥ calls     : -                                                          ≥
 *≥ Input     : HWND, ULONG, MPARAM, MPARAM                                ≥
 *≥ Tasks     : - handle pm messages for notebook frame                    ≥
 *≥ returns   : MRESULT - result of message handling                       ≥
 *¿ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒŸ
 */

MRESULT EXPENTRY FrameWindowProc
 (
     HWND hwnd,
     ULONG msg,
     MPARAM mp1,
     MPARAM mp2
)

{


    switch (msg)
    {
        case WM_SYSCOMMAND:

            switch (SHORT1FROMMP(mp1))
            {

                case SC_CLOSE:  // is close allowed ? check data of all notebook pages !

                    {
                        HAB hab = WinQueryAnchorBlock(hwnd);
                        ULONG ulPageId;
                        HWND hwndNotebook;
                        HWND hwndPage;
                        BOOL fCloseAllowed = TRUE;
                        VALIDATERESULT vr;

                        // query first page
                        memset(&vr, 0, sizeof(VALIDATERESULT));
                        hwndNotebook = WinWindowFromID(hwnd, FID_CLIENT);
                        ulPageId = (ULONG)WinSendMsg(hwndNotebook,
                                                     BKM_QUERYPAGEID,
                                                     0,
                                                     MPFROM2SHORT(BKA_FIRST, 0));

                        while (ulPageId != 0)
                        {
                            // get window handle
                            hwndPage = (HWND)WinSendMsg(hwndNotebook,
                                                    BKM_QUERYPAGEWINDOWHWND,
                                                    (MPARAM)ulPageId,
                                                    0);

                            // change page only if allowed
                            if (!WinSendMsg(hwndPage,
                                            WM_USER_VERIFYDATA,
                                            MPFROMP(&vr), 0))
                            {

                                HMODULE hmodResource = vr.hmodResource;
                                CHAR szTitle[MAX_RES_STRLEN];
                                CHAR szMessage[MAX_RES_MSGLEN];

                                // select the invalid page
                                WinSendMsg(hwndNotebook,
                                           BKM_TURNTOPAGE,
                                           (MPARAM)ulPageId,
                                           0);

                                // set focus to the invalid control
                                WinSetFocus(HWND_DESKTOP, WinWindowFromID(hwndPage, vr.ulFocusId));

                                // bring up popup
                                LOADMESSAGE(vr.ulTitleId, szTitle);
                                LOADMESSAGE(vr.ulMessageId, szMessage);
                                WinMessageBox(HWND_DESKTOP, HWND_DESKTOP,
                                              szMessage,
                                              szTitle,
                                              0,
                                              MB_MOVEABLE | MB_CANCEL | MB_ERROR | MB_APPLMODAL);

                                fCloseAllowed = FALSE;
                                break;
                            }

                            // query next page
                            ulPageId = (ULONG)WinSendMsg(hwndNotebook,
                                                  BKM_QUERYPAGEID,
                                                  (MPARAM)ulPageId,
                                                  MPFROM2SHORT(BKA_NEXT, 0));

                        }

                        // do not close on error
                        if (!fCloseAllowed)
                            return (MRESULT) TRUE;

                    }           // case SC_CLOSE

            }                   // switch (SHORT2FROMMP( mp1))

            break;

    }

    return pfnwpFrameProc(hwnd, msg, mp1, mp2);
}

/*⁄ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒø
 *≥ Name      : PageDlgWindowProc                                          ≥
 *≥ Comment   : generic page dialog window procedure                       ≥
 *≥ Author    : C.Langanke                                                 ≥
 *≥ Date      : 07.02.1998                                                 ≥
 *≥ Update    : 15.02.1998                                                 ≥
 *≥ called by : diverse                                                    ≥
 *≥ calls     : -                                                          ≥
 *≥ Input     : HWND, ULONG, MPARAM, MPARAM                                ≥
 *≥ Tasks     : - handle pm messages for notebook page dialog              ≥
 *≥ returns   : MRESULT - result of message handling                       ≥
 *¿ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒŸ
 */



#define FILETYPEFROMITEM(i)  aulItem2FileType[i]
#define ITEMFROMFILETYPE(t)  aulFileType2Item[t]

//                                 "", ".PTR", "<DIR>", ".CUR", "", ".AND", "", ".ANI", "", ".ANM"};
static ULONG aulFileType2Item[] =
{0, 0, 0, 1, 0, 3, 0, 2, 0, 0};

static ULONG aulItem2FileType[] =
{RESFILETYPE_POINTER, RESFILETYPE_CURSOR,
 RESFILETYPE_WINANIMATION, RESFILETYPE_ANIMOUSE};
MRESULT EXPENTRY PageDlgWindowProc
 (
     HWND hwnd,
     ULONG msg,
     MPARAM mp1,
     MPARAM mp2
)

{

    USHORT usPageId = WinQueryWindowUShort(hwnd, QWS_ID);

    switch (msg)
    {

            // ===========================================================================

        case WM_INITDLG:
            {
                PSETTINGSHANDLERDATA pshd = (PSETTINGSHANDLERDATA) mp2;
                HAB hab = WinQueryAnchorBlock(hwnd);

                // get pagedata
                if (pshd == NULL)
                    break;

                // save ptr to page data
                WinSetWindowPtr(hwnd, QWS_USER, pshd);

                switch (usPageId)
                {

                        // ----------------------------------------------------------

                    case IDDLG_NBANIMATION:

                        // initialize spin button for timeframe value
                        {

#define SPINBUTTON_STEP           TIMEOUT_STEP
#define SPINBUTTON_VALUECOUNT     (((TIMEOUT_MAX - TIMEOUT_MIN) / SPINBUTTON_STEP) + 1)
#define SPINBUTTON_INDEX(v)       ( (v - TIMEOUT_MIN) / SPINBUTTON_STEP )

                            PSZ apszValues;
                            PSZ pszThisValue;
                            PPSZ ppszValueEntry;
                            ULONG i;

                            apszValues = malloc(SPINBUTTON_VALUECOUNT * (sizeof(PSZ) + 5));
                            if (apszValues)
                            {
                                // create value array in memory
                                for (i = 0,
                                     ppszValueEntry = (PPSZ) apszValues,
                                     pszThisValue = apszValues + (sizeof(PSZ) * SPINBUTTON_VALUECOUNT);

                                     i < SPINBUTTON_VALUECOUNT;

                                     i++,
                                     ppszValueEntry++,
                                     pszThisValue += 5)
                                {
                                    _ltoa((i * SPINBUTTON_STEP) + TIMEOUT_MIN, pszThisValue, 10);
                                    *ppszValueEntry = pszThisValue;
                                }

                                // activate array
                                WinSendDlgItemMsg(hwnd, IDDLG_SB_FRAMELENGTH,
                                                  SPBM_SETARRAY,
                                                  (MPARAM) apszValues,
                                           MPFROMLONG(SPINBUTTON_VALUECOUNT));
                                // cleanup
                                free(apszValues);
                            }

                        }
                        break;

                        // ----------------------------------------------------------

                    case IDDLG_NBHIDE:

                        // initialize spin button for delay for hide pointer
                        WinSendDlgItemMsg(hwnd, IDDLG_SB_HIDEPOINTERDELAY,
                                          SPBM_SETLIMITS,
                                          (MPARAM)HIDEPONTERDELAY_MAX,
                                          (MPARAM)HIDEPONTERDELAY_MIN);
                        break;

                        // ----------------------------------------------------------

                    case IDDLG_NBDRAGDROP:
                        {
                            // PSETTINGSHANDLERDATA pshd = WinQueryWindowPtr(hwnd, QWL_USER);
                            HMODULE hmodResource = pshd->hmodResource;

                            LOADSTRING(IDSTR_FILETYPE_POINTER, szFileTypePointer);
                            LOADSTRING(IDSTR_FILETYPE_CURSOR, szFileTypeCursor);
                            LOADSTRING(IDSTR_FILETYPE_WINANIMATION, szFileTypeWinAnimation);
                            LOADSTRING(IDSTR_FILETYPE_ANIMOUSE, szFileTypeAnimouse);

                            INSERTITEM(hwnd, IDDLG_EF_DRAGPTRTYPE, szFileTypePointer);
                            INSERTITEM(hwnd, IDDLG_EF_DRAGPTRTYPE, szFileTypeCursor);
                            INSERTITEM(hwnd, IDDLG_EF_DRAGPTRTYPE, szFileTypeWinAnimation);
                            INSERTITEM(hwnd, IDDLG_EF_DRAGPTRTYPE, szFileTypeAnimouse);

                            INSERTITEM(hwnd, IDDLG_EF_DRAGSETTYPE, szFileTypePointer);
                            INSERTITEM(hwnd, IDDLG_EF_DRAGSETTYPE, szFileTypeCursor);
                            INSERTITEM(hwnd, IDDLG_EF_DRAGSETTYPE, szFileTypeWinAnimation);
                            INSERTITEM(hwnd, IDDLG_EF_DRAGSETTYPE, szFileTypeAnimouse);
                        }
                        break;

                        // ----------------------------------------------------------

                    case IDDLG_NBINIT:
                        // initialize spin button for animation init delay
                        WinSendDlgItemMsg(hwnd, IDDLG_SB_INITDELAY,
                                          SPBM_SETLIMITS,
                                          (MPARAM)INITDELAY_MAX,
                                          (MPARAM)INITDELAY_MIN);
                        break;

                }               // end switch (usPageId)

                // init values
                WinSendMsg(hwnd, WM_COMMAND, MPFROM2SHORT(IDDLG_PB_UNDO, 0), 0L);

            }
            break;

            // ===========================================================================

        case WM_COMMAND:

            switch (SHORT1FROMMP(mp1))
            {

                case IDDLG_PB_UNDO:

                    switch (usPageId)
                    {

                            // ----------------------------------------------------------

                        case IDDLG_NBANIMATION:
                            // current values
                            DLGSETSTRINGTEXTLIMIT(hwnd,
                                            IDDLG_EF_ANIMATIONPATH,
                                            _MAX_PATH);
                            DLGSETSTRING(hwnd,
                                    IDDLG_EF_ANIMATIONPATH,
                                    (PSZ)getAnimationPath());

                            DLGSETCHECK(hwnd, IDDLG_CB_USEFORALL, getOverrideTimeout());
                            DLGSETCHECK(hwnd, IDDLG_CB_ANIMATEONLOAD, getActivateOnLoad());

                            DLGSETSPIN(hwnd, IDDLG_SB_FRAMELENGTH, SPINBUTTON_INDEX(getDefaultTimeout()));
                            break;

                            // ----------------------------------------------------------

                        case IDDLG_NBHIDE:
                            DLGSETCHECK(hwnd, IDDLG_CB_HIDEPOINTER, getHidePointer());
                            DLGSETSPIN(hwnd, IDDLG_SB_HIDEPOINTERDELAY, getHidePointerDelay());
                            break;

                            // ----------------------------------------------------------

                        case IDDLG_NBDRAGDROP:
                            SETSELECTION(hwnd, IDDLG_EF_DRAGPTRTYPE, ITEMFROMFILETYPE(getDragPtrType()));
                            SETSELECTION(hwnd, IDDLG_EF_DRAGSETTYPE, ITEMFROMFILETYPE(getDragSetType()));
                            break;

                            // ----------------------------------------------------------

                        case IDDLG_NBINIT:
                            DLGSETSPIN(hwnd, IDDLG_SB_INITDELAY, getAnimationInitDelay());
                            break;

                    }           // end switch (usPageId)

                    break;      // case IDDLG_PB_UNDO:


                case IDDLG_PB_DEFAULT:
                    switch (usPageId)
                    {

                            // ----------------------------------------------------------

                        case IDDLG_NBANIMATION:
                            // default values
                            DLGSETSTRING(hwnd, IDDLG_EF_ANIMATIONPATH, DEFAULT_ANIMATIONPATH);
                            DLGSETCHECK(hwnd, IDDLG_CB_USEFORALL, DEFAULT_OVERRIDETIMEOUT);
                            DLGSETCHECK(hwnd, IDDLG_CB_ANIMATEONLOAD, DEFAULT_ACTIVATEONLOAD);
                            DLGSETSPIN(hwnd, IDDLG_SB_FRAMELENGTH, SPINBUTTON_INDEX(DEFAULT_ANIMATION_TIMEOUT));
                            break;

                            // ----------------------------------------------------------

                        case IDDLG_NBHIDE:
                            DLGSETCHECK(hwnd, IDDLG_CB_HIDEPOINTER, DEFAULT_HIDEPOINTER);
                            DLGSETSPIN(hwnd, IDDLG_SB_HIDEPOINTERDELAY, DEFAULT_HIDEPOINTERDELAY);
                            break;

                            // ----------------------------------------------------------

                        case IDDLG_NBDRAGDROP:
                            SETSELECTION(hwnd, IDDLG_EF_DRAGPTRTYPE, ITEMFROMFILETYPE(DEFAULT_DRAGPTRTYPE));
                            SETSELECTION(hwnd, IDDLG_EF_DRAGSETTYPE, ITEMFROMFILETYPE(DEFAULT_DRAGSETTYPE));
                            break;

                            // ----------------------------------------------------------

                        case IDDLG_NBINIT:
                            DLGSETSPIN(hwnd, IDDLG_SB_INITDELAY, getDefaultAnimationInitDelay());
                            break;

                    }           // end switch (usPageId)

                    break;      // case IDDLG_PB_DEFAULT

            }                   // end switch (SHORT1FROMMP(mp1))

            // do not call WinDefDlgProc:
            // so no dismiss !
            return (MRESULT) TRUE;

            // break;              // end case WM_COMMAND

            // ===========================================================================

        case WM_USER_VERIFYDATA:
            {
                PVALIDATERESULT pvr = (PVALIDATERESULT) mp1;

                // sind Parameter gÅltig ?
                if (!pvr)
                    break;

                switch (usPageId)
                {

                        // ----------------------------------------------------------

                    case IDDLG_NBANIMATION:
                        {

                            PSETTINGSHANDLERDATA pshd = WinQueryWindowPtr(hwnd, QWL_USER);
                            CHAR szAnimationPath[_MAX_PATH];

                            // validate
                            DLGQUERYSTRING(hwnd, IDDLG_EF_ANIMATIONPATH, szAnimationPath);
                            if (!FileExist(szAnimationPath, TRUE))
                            {
                                pvr->fInvalid = TRUE;
                                pvr->ulFocusId = IDDLG_EF_ANIMATIONPATH;
                                pvr->ulTitleId = IDMSG_TITLE_ERROR;
                                pvr->ulMessageId = IDMSG_ANIMATIONPATH_NOT_FOUND;
                                pvr->hmodResource = pshd->hmodResource;
                                return (MRESULT) FALSE;
                            }
                        }
                        break;

                        // ----------------------------------------------------------

                    case IDDLG_NBHIDE:
                        break;

                        // ----------------------------------------------------------

                    case IDDLG_NBDRAGDROP:
                        break;

                        // ----------------------------------------------------------

                    case IDDLG_NBINIT:
                        break;

                }               // end switch (usPageId)

                return (MRESULT) TRUE;

            }
            // break;              // case WM_USER_VERIFYDATA:

            // ===========================================================================

        case WM_DESTROY:
            {
                PSETTINGSHANDLERDATA pshd = WinQueryWindowPtr(hwnd, QWL_USER);


                CHAR szValue[20];
                CHAR szAnimationPath[_MAX_PATH];

                switch (usPageId)
                {

                        // ----------------------------------------------------------

                    case IDDLG_NBANIMATION:

                        // set animation path.
                        // Validation is done already by WM_USER_VERIFYDATA on SC_CLOSE
                        DLGQUERYSTRING(hwnd, IDDLG_EF_ANIMATIONPATH, szAnimationPath);
                        setAnimationPath(szAnimationPath);

                        // check boxes
                        setOverrideTimeout(DLGQUERYCHECK(hwnd, IDDLG_CB_USEFORALL));
                        setActivateOnLoad(DLGQUERYCHECK(hwnd, IDDLG_CB_ANIMATEONLOAD));

                        // spin button framelength value
                        if (WinSendDlgItemMsg(hwnd, IDDLG_SB_FRAMELENGTH, SPBM_QUERYVALUE,
                                              (MPARAM) szValue,
                                              (MPARAM) MPFROM2SHORT(sizeof(szValue), SPBQ_DONOTUPDATE)))
                            setDefaultTimeout(atol(szValue));

                        pshd->fRefreshView = TRUE;
                        break;

                        // ----------------------------------------------------------

                    case IDDLG_NBHIDE:
                        {
                            BOOL fHidePointer = DLGQUERYCHECK(hwnd, IDDLG_CB_HIDEPOINTER);

                            // set new value only if it has changed
                            if (getHidePointer() != fHidePointer)
                                pshd->fToggleHidePointer = TRUE;

                            // spin button hide Pointer delay
                            if (WinSendDlgItemMsg(hwnd, IDDLG_SB_HIDEPOINTERDELAY, SPBM_QUERYVALUE,
                                                  (MPARAM) szValue,
                                                  (MPARAM) MPFROM2SHORT(sizeof(szValue), SPBQ_DONOTUPDATE)))
                            {
                                setHidePointerDelay(atol(szValue));
                                pshd->fHidePointerDelayChanged = TRUE;
                            }

                        }
                        break;

                        // ----------------------------------------------------------

                    case IDDLG_NBDRAGDROP:
                        setDragPtrType(FILETYPEFROMITEM(QUERYSELECTION(hwnd, IDDLG_EF_DRAGPTRTYPE, LIT_FIRST)));
                        setDragSetType(FILETYPEFROMITEM(QUERYSELECTION(hwnd, IDDLG_EF_DRAGSETTYPE, LIT_FIRST)));
                        break;

                        // ----------------------------------------------------------

                    case IDDLG_NBINIT:

                        // spin button animation init delay
                        if (WinSendDlgItemMsg(hwnd, IDDLG_SB_INITDELAY, SPBM_QUERYVALUE,
                                              (MPARAM) szValue,
                                              (MPARAM) MPFROM2SHORT(sizeof(szValue), SPBQ_DONOTUPDATE)))
                            setAnimationInitDelay(atol(szValue));
                        break;

                }               // end switch (usPageId)

            }
            break;              // case WM_DESTROY:

    }                           // end switch (msg)

    return WinDefDlgProc(hwnd, msg, mp1, mp2);
}
