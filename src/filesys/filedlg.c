
/*
 *@@sourcefile filedlg.c:
 *      replacement file dlg with full WPS support.
 *
 *      The entry point is fdlgFileDialog, which has the
 *      same syntax as WinFileDlg and should return about
 *      the same values. 100% compatibility cannot be
 *      achieved because of window subclassing and all those
 *      things.
 *
 *      This file is ALL new with V0.9.9.
 *
 *      Function prefix for this file:
 *      --  fdlr*
 *
 *@@added V0.9.9 (2001-03-11) [umoeller]
 *@@header "filesys\filedlg.h"
 */

/*
 *      Copyright (C) 2001 Ulrich M”ller.
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

#define INCL_DOSERRORS
#define INCL_DOSSEMAPHORES
#define INCL_DOSEXCEPTIONS
#define INCL_DOSPROCESS

#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WINMESSAGEMGR
#define INCL_WININPUT
#define INCL_WINTIMER
#define INCL_WINPOINTERS
#define INCL_WINDIALOGS
#define INCL_WINBUTTONS
#define INCL_WINSTATICS
#define INCL_WINENTRYFIELDS
#define INCL_WINLISTBOXES
#define INCL_WINSTDFILE
#define INCL_WINSTDCNR
#define INCL_WINSHELLDATA       // Prf* functions
#define INCL_WINSYS

#define INCL_GPILOGCOLORTABLE
#define INCL_GPIBITMAPS
#include <os2.h>

// C library headers
#include <stdio.h>              // needed for except.h
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h
#include <ctype.h>

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\comctl.h"             // common controls (window procs)
#include "helpers\cnrh.h"               // container helper routines
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\stringh.h"            // string helper routines
#include "helpers\threads.h"            // thread helpers
#include "helpers\winh.h"               // PM helper routines
#include "helpers\xstring.h"            // extended string helpers

// SOM headers which don't crash with prec. header files
#include "xfldr.ih"
#include "xfdisk.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

#include "filesys\filedlg.h"            // replacement file dialog implementation
#include "filesys\folder.h"             // XFolder implementation

// other SOM headers
#pragma hdrstop                         // VAC++ keeps crashing otherwise
#include <wpshadow.h>

/* ******************************************************************
 *
 *   Declarations
 *
 ********************************************************************/

const char *WC_FILEDLGCLIENT    = "XWPFileDlgClient";
const char *WC_ADDCHILDRENOBJ   = "XWPFileDlgAddChildren";

#define IDDI_TYPESTXT           100
#define IDDI_TYPESCOMBO         101
                // directory statics:
#define IDDI_DIRTXT             102
#define IDDI_DIRVALUE           103
                // file static/entryfield:
#define IDDI_FILETXT            104
#define IDDI_FILEENTRY          105


/*
 *@@ FILEDLGDATA:
 *      single structure allocated on the stack
 *      in fdlgFileDlg. The main frame and the
 *      main client receive pointers to this
 *      and store them in QWL_USER.
 *
 *      This pretty much holds all the data the
 *      file dlg needs while it is running.
 */

typedef struct _FILEDLGDATA
{
    // ptr to FILEDLG structure passed in
    PFILEDLG    pfd;

    // window hierarchy
    HWND        hwndMainFrame,
                hwndMainClient,     // child of hwndMainFrame
                hwndSplitWindow,    // child of hwndMainClient
                hwndDrivesFrame,    // child of hwndSplitWindow
                hwndDrivesCnr,      // child of hwndDrivesFrame
                hwndFilesFrame,     // child of hwndSplitWindow
                hwndFilesCnr;       // child of hwndFilesFrame

    // controls in hwndMainClient
                // buttons:

    HWND        // types combo:
                hwndTypesTxt,
                hwndTypesCombo,
                // directory statics:
                hwndDirTxt,
                hwndDirValue,
                // file static/entryfield:
                hwndFileTxt,
                hwndFileEntry,
                // buttons:
                hwndOK,
                hwndCancel,
                hwndHelp;

    LINKLIST    llDialogControls;       // list of dialog controls for focus etc.

    // drives view (left)
    PSUBCLASSEDFOLDERVIEW psfvDrives;   // created in fdlgFileDlg only once

    WPFolder    *pDrivesFolder;         // root folder to populate, whose contents
                                        // appear in "drives" tree
    THREADINFO  tiAddChildren;
    BOOL        fAddChildrenRunning;
    HWND        hwndAddChildren;

    LINKLIST    llDriveObjectsInserted; // linked list of plain WPObject* pointers
                                        // inserted, no auto-free
    LINKLIST    llDisks;                // linked list of all WPDisk* objects
                                        // so that we can find them later;
                                        // no auto-free
    PMINIRECORDCORE precSelectedInDrives;

    // files view (right)
    PSUBCLASSEDFOLDERVIEW psfvFiles;
    BOOL        fFilesFrameSubclassed;

    THREADINFO  tiInsertContents;
    BOOL        fInsertContentsRunning;
    LINKLIST    llFileObjectsInserted;

    // full file name etc. (_splitpath)
    ULONG       ulLogicalDrive;         // e.g. 3 for 'C'
    CHAR        szDir[CCHMAXPATH],      // e.g. "\whatever"
                szFileMask[CCHMAXPATH],    // e.g. "*.txt"
                szFileName[CCHMAXPATH]; // e.g. "test.txt"

    BOOL        fFileDlgReady;
            // while this is FALSE, the dialog doesn't
            // react to changes in the containers

} FILEDLGDATA, *PFILEDLGDATA;

/*
 *@@ INSERTTHREADSDATA:
 *      temporary structure allocated from the heap
 *      and passed to the various transient threads
 *      that do the hard work for us (populate, insert children).
 *      This structure is free()'d by the thread that
 *      was started.
 */

typedef struct _INSERTTHREADSDATA
{
    PFILEDLGDATA        pWinData;

    PLINKLIST           pll;         // list of items to work on; format depends on thread.
                                     // List is freed on exit.

} INSERTTHREADSDATA, *PINSERTTHREADSDATA;

/*
 *@@ INSERTOBJECTSARRAY:
 *
 */

typedef struct _INSERTOBJECTSARRAY
{
    WPFolder            *pFolder;
    HWND                hwndCnr;
    WPObject            **papObjects;
    ULONG               cObjects;
    PMINIRECORDCORE     precParent;

    PLINKLIST           pllObjects;
    PLINKLIST           pllRecords;
} INSERTOBJECTSARRAY, *PINSERTOBJECTSARRAY;

#define XM_FILLFILESCNR         (WM_USER + 2)
#define XM_INSERTOBJARRAY       (WM_USER + 3)
#define XM_UPDATEPOINTER        (WM_USER + 4)

#define ACM_ADDCHILDREN         (WM_USER + 6)
#define ACM_ADDFIRSTCHILD       (WM_USER + 7)

#define ID_TREEFRAME            1
#define ID_FILESFRAME           2

MRESULT EXPENTRY fnwpSubclassedFilesFrame(HWND hwndFrame, ULONG msg, MPARAM mp1, MPARAM mp2);

BOOL StartInsertContents(PFILEDLGDATA pWinData,
                         PMINIRECORDCORE precc);

/* ******************************************************************
 *
 *   Super Combination Box control
 *
 ********************************************************************/

#define COMBO_BUTTON_WIDTH      20

#define ID_COMBO_BUTTON         1001
#define ID_COMBO_LISTBOX        1002

/*
 *@@ COMBODATA:
 *
 *@@added V0.9.9 (2001-03-17) [umoeller]
 */

typedef struct _COMBODATA
{
    PFNWP       pfnwpOrigEntryField,
                pfnwpOrigButton;
    ULONG       flStyle;

    // position of entire combo
    LONG        x,
                y,
                cx,
                cy;

    HWND        hwndButton,
                hwndListbox;

    HBITMAP     hbmButton;
    SIZEL       szlButton;          // bitmap dimensions

} COMBODATA, *PCOMBODATA;

/*
 *@@ PaintButtonBitmap:
 *
 *@@added V0.9.9 (2001-03-17) [umoeller]
 */

VOID PaintButtonBitmap(HWND hwnd,
                       PCOMBODATA pcd)
{
    HPS hps;
    RECTL rcl;
    POINTL ptlDest;

    hps = WinGetPS(hwnd);
    WinQueryWindowRect(hwnd, &rcl);

    ptlDest.x = (rcl.xRight - pcd->szlButton.cx) / 2;
    ptlDest.y = (rcl.yTop - pcd->szlButton.cy) / 2;
    WinDrawBitmap(hps,
                  pcd->hbmButton,
                  NULL,
                  &ptlDest,
                  0, 0,
                  DBM_NORMAL);

    WinReleasePS(hps);
}

/*
 *@@ fnwpSubclassedComboButton:
 *      window proc the combobox's button is subclassed with.
 *      This is only for WM_PAINT because BN_PAINT is really
 *      not that great for painting a button that looks like
 *      a standard button.
 *
 *@@added V0.9.9 (2001-03-17) [umoeller]
 */

MRESULT EXPENTRY fnwpSubclassedComboButton(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    switch (msg)
    {
        case WM_PAINT:
        {
            PCOMBODATA pcd = (PCOMBODATA)WinQueryWindowPtr(hwnd, QWL_USER);
            if (pcd)
            {
                mrc = pcd->pfnwpOrigButton(hwnd, msg, mp1, mp2);

                PaintButtonBitmap(hwnd, pcd);
            }
        break; }

        /*
         * default:
         *
         */

        default:
        {
            PCOMBODATA pcd = (PCOMBODATA)WinQueryWindowPtr(hwnd, QWL_USER);
            if (pcd)
                mrc = pcd->pfnwpOrigButton(hwnd, msg, mp1, mp2);
        break; }
    }

    return (mrc);
}

/*
 *@@ ShowListbox:
 *
 *@@added V0.9.9 (2001-03-17) [umoeller]
 */

VOID ShowListbox(HWND hwnd,      // in: subclassed entry field
                 PCOMBODATA pcd,
                 BOOL fShow)    // in: TRUE == show, FALSE == hide
{
    BOOL fHilite = FALSE;

    if (fShow)
    {
        // list box is invisible:
        SWP swp;
        POINTL ptl;
        WinQueryWindowPos(hwnd, &swp);

        _Pmpf(("showing lb"));

        // convert to desktop
        ptl.x = swp.x;
        ptl.y = swp.y;
        WinMapWindowPoints(WinQueryWindow(hwnd, QW_PARENT), // from
                           HWND_DESKTOP,    // to
                           &ptl,
                                // SWP.y comes before SWP.x
                           1);

        WinSetWindowPos(pcd->hwndListbox,
                        HWND_TOP,
                        ptl.x + COMBO_BUTTON_WIDTH,
                        ptl.y - 100,
                        swp.cx,
                        100,
                        SWP_MOVE | SWP_SIZE | SWP_ZORDER | SWP_NOREDRAW);
        WinSetParent(pcd->hwndListbox,
                     HWND_DESKTOP,
                     TRUE);        // redraw

        // set focus to subclassed entry field in any case;
        // we never let the listbox get the focus
        WinSetFocus(HWND_DESKTOP, hwnd);

        fHilite = TRUE;
    }
    else
    {
        // list box is showing:
        HWND hwndFocus = WinQueryFocus(HWND_DESKTOP);
        _Pmpf(("hiding listbox"));

        WinSetParent(pcd->hwndListbox,
                     HWND_OBJECT,
                     TRUE);         // redraw now
        // give focus back to entry field
        if (hwndFocus == pcd->hwndListbox)
            WinSetFocus(HWND_DESKTOP, hwnd);
    }

    WinSendMsg(pcd->hwndButton,
               BM_SETHILITE,
               (MPARAM)fHilite,
               0);
    PaintButtonBitmap(pcd->hwndButton, pcd);
}

/*
 *@@ fnwpComboSubclass:
 *
 *@@added V0.9.9 (2001-03-17) [umoeller]
 */

MRESULT EXPENTRY fnwpComboSubclass(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    switch (msg)
    {
        /*
         * WM_ADJUSTWINDOWPOS:
         *
         */

        case WM_ADJUSTWINDOWPOS:
        {
            PCOMBODATA pcd = (PCOMBODATA)WinQueryWindowPtr(hwnd, QWL_USER);
            if (pcd)
            {
                PSWP pswp = (PSWP)mp1;

                if (pswp->fl & SWP_SIZE)
                    // if we're being sized, make us smaller so that
                    // there's room for the button
                    pswp->cx -= COMBO_BUTTON_WIDTH;

                mrc = pcd->pfnwpOrigEntryField(hwnd, msg, mp1, mp2);
            }
        break; }

        /*
         * WM_WINDOWPOSCHANGED:
         *
         */

        case WM_WINDOWPOSCHANGED:
        {
            PCOMBODATA pcd = (PCOMBODATA)WinQueryWindowPtr(hwnd, QWL_USER);
            if (pcd)
            {
                PSWP pswpNew = (PSWP)mp1;

                if (pswpNew->fl & (SWP_SIZE | SWP_MOVE))
                {
                    // moved or sized:
                    SWP swp;
                    WinQueryWindowPos(hwnd, &swp);
                    WinSetWindowPos(pcd->hwndButton,
                                    0,
                                    pswpNew->x + pswpNew->cx, // has already been truncated!
                                    pswpNew->y,
                                    COMBO_BUTTON_WIDTH,
                                    pswpNew->cy,
                                    SWP_MOVE | SWP_SIZE);
                }

                mrc = pcd->pfnwpOrigEntryField(hwnd, msg, mp1, mp2);
            }
        break; }

        /*
         * WM_SETFOCUS:
         *      hide listbox if focus is going away from us
         */

        case WM_SETFOCUS:
        {
            PCOMBODATA pcd = (PCOMBODATA)WinQueryWindowPtr(hwnd, QWL_USER);
            if (pcd)
            {
                if (!mp2)
                    // we're losing focus:
                    // is listbox currently showing?
                    ShowListbox(hwnd,
                                pcd,
                                FALSE);

                mrc = pcd->pfnwpOrigEntryField(hwnd, msg, mp1, mp2);
            }
        break; }

        /*
         * WM_COMMAND:
         *      show/hide listbox if the button gets pressed.
         */

        case WM_COMMAND:
        {
            PCOMBODATA pcd = (PCOMBODATA)WinQueryWindowPtr(hwnd, QWL_USER);
            if (pcd)
            {
                if ((SHORT)mp1 == ID_COMBO_BUTTON)
                {
                    // button clicked:
                    ShowListbox(hwnd,
                                pcd,
                                // check state of list box
                                (WinQueryWindow(pcd->hwndListbox, QW_PARENT)
                                 == WinQueryObjectWindow(HWND_DESKTOP)));

                    // do not call parent
                    break;

                } // end if ((SHORT)mp1 == ID_COMBO_BUTTON)

                mrc = pcd->pfnwpOrigEntryField(hwnd, msg, mp1, mp2);
            }
        break; }

        /*
         * WM_CONTROL:
         *      handle notifications from listbox.
         */

        case WM_CONTROL:
        {
            PCOMBODATA pcd = (PCOMBODATA)WinQueryWindowPtr(hwnd, QWL_USER);
            if (pcd)
            {
                USHORT usid = SHORT1FROMMP(mp1),
                       uscode = SHORT2FROMMP(mp1);
                if (usid == ID_COMBO_LISTBOX)
                {
                    switch (uscode)
                    {
                        case LN_ENTER:
                        break;

                        case LN_SELECT:
                        {
                            SHORT sSelected = winhQueryLboxSelectedItem(pcd->hwndListbox,
                                                                        LIT_FIRST);
                            PSZ psz = NULL;
                            if (sSelected != LIT_NONE)
                            {
                                psz = winhQueryLboxItemText(pcd->hwndListbox,
                                                            sSelected);
                            }
                            WinSetWindowText(hwnd, psz);
                            if (psz)
                            {
                                WinPostMsg(hwnd,
                                           EM_SETSEL,
                                           MPFROM2SHORT(0, strlen(psz)),
                                           0);
                                free(psz);
                            }
                        break; }

                        case LN_SETFOCUS:
                            // when the list box gets the focus, always
                            // set focus to ourselves
                            WinSetFocus(HWND_DESKTOP, hwnd);
                        break;
                    }

                    // forward list box notifications to
                    // our own owner, but replace the id
                    // with the combo box id
                    WinPostMsg(WinQueryWindow(hwnd, QW_OWNER),
                               WM_CONTROL,
                               MPFROM2SHORT(WinQueryWindowUShort(hwnd, QWS_ID),
                                            uscode),
                               mp2);

                    // do not call parent
                    break;

                } // end if (usid == ID_COMBO_LISTBOX)

                mrc = pcd->pfnwpOrigEntryField(hwnd, msg, mp1, mp2);
            }
        break; }

        /*
         * WM_CHAR:
         *
         */

        case WM_CHAR:
        {
            PCOMBODATA pcd = (PCOMBODATA)WinQueryWindowPtr(hwnd, QWL_USER);
            if (pcd)
            {
                USHORT usFlags    = SHORT1FROMMP(mp1);
                USHORT usch       = SHORT1FROMMP(mp2);
                USHORT usvk       = SHORT2FROMMP(mp2);

                if ((usFlags & KC_KEYUP) == 0)
                {
                    if (usFlags & KC_VIRTUALKEY)
                    {
                        switch (usvk)
                        {
                            case VK_DOWN:
                            case VK_UP:
                                // if alt is pressed with these, show/hide listbox
                                if (usFlags & KC_ALT)
                                    WinPostMsg(hwnd,
                                               CBM_SHOWLIST,
                                               (MPARAM)(WinQueryWindow(pcd->hwndListbox, QW_PARENT)
                                                        == WinQueryObjectWindow(HWND_DESKTOP)),
                                               0);
                                else
                                {
                                    // just up or down, no alt:
                                    // select next or previous item in list box
                                    SHORT sSelected = winhQueryLboxSelectedItem(pcd->hwndListbox,
                                                                                LIT_FIRST),
                                          sNew = 0;

                                    if (usvk == VK_DOWN)
                                    {
                                        if (sSelected != LIT_NONE)
                                        {
                                            if (sSelected < WinQueryLboxCount(pcd->hwndListbox))
                                                sNew = sSelected + 1;
                                        }
                                        // else: sNew still 0
                                    }
                                    else
                                    {
                                        // up:
                                        if (    (sSelected != LIT_NONE)
                                             && (sSelected > 0)
                                           )
                                            sNew = sSelected - 1;
                                    }

                                    winhSetLboxSelectedItem(pcd->hwndListbox,
                                                            sNew,
                                                            TRUE);
                                }
                            break;
                        }
                    }
                }

                // call parent only if this is not a drop-down list
                if ((pcd->flStyle & CBS_DROPDOWNLIST) == 0)
                    mrc = pcd->pfnwpOrigEntryField(hwnd, msg, mp1, mp2);
                else
                    // forward to owner
                    WinSendMsg(WinQueryWindow(hwnd, QW_OWNER),
                               msg,
                               mp1,
                               mp2);
            }
        break; }

        /*
         * CBM_ISLISTSHOWING:
         *      implementation of the original combobox msg.
         */

        case CBM_ISLISTSHOWING:
        {
            PCOMBODATA pcd = (PCOMBODATA)WinQueryWindowPtr(hwnd, QWL_USER);
            if (pcd)
            {
                mrc = (MPARAM)(WinQueryWindow(pcd->hwndListbox, QW_PARENT)
                                     == WinQueryObjectWindow(HWND_DESKTOP));
            }
        break; }

        /*
         * CBM_SHOWLIST:
         *      implementation of the original combobox msg.
         */

        case CBM_SHOWLIST:
        {
            PCOMBODATA pcd = (PCOMBODATA)WinQueryWindowPtr(hwnd, QWL_USER);
            if (pcd)
            {
                ShowListbox(hwnd,
                            pcd,
                            (BOOL)mp1);
            }
        break; }

        /*
         * list box messages:
         *      forward all these to the listbox and
         *      return the listbox return value.
         */

        case LM_INSERTITEM:
        case LM_SETTOPINDEX:
        case LM_QUERYTOPINDEX:
        case LM_DELETEITEM:
        case LM_SELECTITEM:
        case LM_QUERYSELECTION:
        case LM_SETITEMTEXT:
        case LM_QUERYITEMTEXT:
        case LM_SEARCHSTRING:
        case LM_DELETEALL:
        {
            PCOMBODATA pcd = (PCOMBODATA)WinQueryWindowPtr(hwnd, QWL_USER);
            if (pcd)
            {
                mrc = WinSendMsg(pcd->hwndListbox, msg, mp1, mp2);
            }
        break; }

        /*
         * WM_DESTROY:
         *
         */

        case WM_DESTROY:
        {
            PCOMBODATA pcd = (PCOMBODATA)WinQueryWindowPtr(hwnd, QWL_USER);
            if (pcd)
            {
                WinDestroyWindow(pcd->hwndButton);
                WinDestroyWindow(pcd->hwndListbox);

                mrc = pcd->pfnwpOrigEntryField(hwnd, msg, mp1, mp2);

                free(pcd);
            }
        break; }

        /*
         * default:
         *
         */

        default:
        {
            PCOMBODATA pcd = (PCOMBODATA)WinQueryWindowPtr(hwnd, QWL_USER);
            if (pcd)
                mrc = pcd->pfnwpOrigEntryField(hwnd, msg, mp1, mp2);
        break; }
    }

    return (mrc);
}

/*
 *@@ ctlComboFromEntryField:
 *      turns a standard entry field control into an
 *      XComboBox.
 *
 *      The XComboBox is intended to work like a standard
 *      combobox, but it doesn't have the silly limitation
 *      that the size of the combobox is assumed to be
 *      the size of the dropped-down combobox. This limitation
 *      makes it impossible to use standard comboboxes in
 *      windows which have the WS_CLIPCHILDREN style because
 *      the entire combo area will always be clipped out.
 *
 *      This is not a full reimplementation. Only drop-down
 *      and drop-down list comboboxes are supported. Besides,
 *      the XComboBox is essentially a subclassed entryfield,
 *      so there might be limitations.
 *
 *      On input to this function, with flStyle, specify
 *      either CBS_DROPDOWN or CBS_DROPDOWNLIST. CBS_SIMPLE
 *      is not supported.
 *
 *      Supported messages to the XComboBox after this funcion
 *      has been called:
 *
 *      -- CBM_ISLISTSHOWING
 *
 *      -- CBM_SHOWLIST
 *
 *      -- LM_QUERYITEMCOUNT
 *
 *      -- LM_INSERTITEM
 *
 *      -- LM_SETTOPINDEX
 *
 *      -- LM_QUERYTOPINDEX
 *
 *      -- LM_DELETEITEM
 *
 *      -- LM_SELECTITEM
 *
 *      -- LM_QUERYSELECTION
 *
 *      -- LM_SETITEMTEXT
 *
 *      -- LM_QUERYITEMTEXT
 *
 *      -- LM_SEARCHSTRING
 *
 *      -- LM_DELETEALL
 *
 *      NOTE: This occupies QWL_USER of the entryfield.
 *
 *@@added V0.9.9 (2001-03-17) [umoeller]
 */

BOOL ctlComboFromEntryField(HWND hwnd,          // in: entry field to be converted
                            ULONG flStyle)      // in: combo box styles
{
    BOOL brc = FALSE;
    PFNWP pfnwpOrig = WinSubclassWindow(hwnd,
                                        fnwpComboSubclass);
    if (pfnwpOrig)
    {
        PCOMBODATA pcd = (PCOMBODATA)malloc(sizeof(*pcd));
        if (pcd)
        {
            SWP swp;
            BITMAPINFOHEADER2 bmih2;

            memset(pcd, 0, sizeof(*pcd));
            pcd->pfnwpOrigEntryField = pfnwpOrig;
            pcd->flStyle = flStyle;

            WinSetWindowPtr(hwnd, QWL_USER, pcd);

            WinQueryWindowPos(hwnd, &swp);
            pcd->x = swp.x;
            pcd->y = swp.y;
            pcd->cx = swp.cx;
            pcd->cy = swp.cy;

            swp.cx -= COMBO_BUTTON_WIDTH;
            WinSetWindowPos(hwnd,
                            0,
                            0, 0,
                            swp.cx, swp.cy,
                            SWP_SIZE | SWP_NOADJUST);       // circumvent subclassing

            pcd->hbmButton = WinGetSysBitmap(HWND_DESKTOP,
                                             SBMP_COMBODOWN);
            bmih2.cbFix = sizeof(bmih2);
            GpiQueryBitmapInfoHeader(pcd->hbmButton,
                                     &bmih2);
            pcd->szlButton.cx = bmih2.cx;
            pcd->szlButton.cy = bmih2.cy;

            pcd->hwndButton = WinCreateWindow(WinQueryWindow(hwnd, QW_PARENT),
                                              WC_BUTTON,
                                              "",
                                              WS_VISIBLE
                                                | BS_PUSHBUTTON | BS_NOPOINTERFOCUS,
                                              swp.x + swp.cx - COMBO_BUTTON_WIDTH,
                                              swp.y,
                                              COMBO_BUTTON_WIDTH,
                                              swp.cy,
                                              hwnd,     // owner == entry field!
                                              hwnd,     // insert behind entry field
                                              ID_COMBO_BUTTON,
                                              NULL,
                                              NULL);
            WinSetWindowPtr(pcd->hwndButton, QWL_USER, pcd);
            pcd->pfnwpOrigButton = WinSubclassWindow(pcd->hwndButton,
                                                     fnwpSubclassedComboButton);

            pcd->hwndListbox = WinCreateWindow(HWND_OBJECT,      // parent, for now
                                               WC_LISTBOX,
                                               "?",
                                               WS_VISIBLE | WS_SAVEBITS | WS_CLIPSIBLINGS
                                                 | LS_NOADJUSTPOS,
                                               0,
                                               0,
                                               0,
                                               0,
                                               hwnd,     // owner == entry field!
                                               HWND_TOP,     // insert behind entry field
                                               ID_COMBO_LISTBOX,
                                               NULL,
                                               NULL);

            // finally, set style of entry field... we force
            // these flags no matter what the original style
            // was
            /* WinSetWindowBits(hwnd,
                             QWL_STYLE,
                             // bits to set:
                            (flStyle & CBS_DROPDOWNLIST)
                                ? ES_READONLY
                                : 0,
                             // mask:
                             ES_READONLY); */
        }
    }

    return (brc);
}

/* ******************************************************************
 *
 *   Helper funcs
 *
 ********************************************************************/

#define FFL_DRIVE           0x0001
#define FFL_PATH            0x0002
#define FFL_FILEMASK        0x0004
#define FFL_FILENAME        0x0008

/*
 *@@ ParseFileString:
 *      parses the given full-file string and
 *      sets up the members in pWinData accordingly.
 *
 *      This returns a ULONG bitset with any combination
 *      of the following:
 *
 *      -- FFL_DRIVE: drive was specified,
 *         and FILEDLGDATA.ulLogicalDrive has been updated.
 *
 *      -- FFL_PATH: directory was specified,
 *         and FILEDLGDATA.szDir has been updated.
 *
 *      -- FFL_FILEMASK: a file mask was given ('*' or '?'
 *         characters found in filename), and
 *         FILEDLGDATA.szFileMask has been updated.
 *         FILEDLGDATA.szFileName is set to a null byte.
 *         This never comes with FFL_FILENAME.
 *
 *      -- FFL_FILENAME: a file name without wildcards was
 *         given, and FILEDLGDATA.szFileName has been updated.
 *         FILEDLGDATA.is unchanged.
 *         This never comes with FFL_FILEMASK.
 */

ULONG ParseFileString(PFILEDLGDATA pWinData,
                      const char *pcszFullFile)
{
    ULONG ulChanged = 0;

    const char  *p;

    _Pmpf((__FUNCTION__ ": parsing %s", pcszFullFile));

    // check if a drive is specified
    if (    (*pcszFullFile)
         && (pcszFullFile[1] == ':')
       )
    {
        // yes:
        LONG lNew = toupper(*pcszFullFile) - 'A' + 1;
        if (lNew > 0 && lNew <= 26)
            pWinData->ulLogicalDrive = lNew;

        _Pmpf(("  new logical drive is %d", pWinData->ulLogicalDrive));
        p = pcszFullFile + 2;
        ulChanged |= FFL_DRIVE;
    }
    else
        p = pcszFullFile;

    // get path from there
    if (*p)
    {
        const char *p2 = strrchr(p, '\\');
        if (p2)
        {
            // path specified:
            // ### handle relative paths here
            strhncpy0(pWinData->szDir,
                      p,        // start: either first char or after drive
                      (p2 - p));
            _Pmpf(("  new path is %s", pWinData->szDir));
            ulChanged |= FFL_PATH;
            p2++;
        }
        else
            // no path specified:
            p2 = p;

        // check if the following is a file mask
        // or a real file name
        if (    (strchr(p2, '*'))
             || (strchr(p2, '?'))
           )
        {
            // get file name (mask) after that
            strcpy(pWinData->szFileMask,
                   p2);
            ulChanged |= FFL_FILEMASK;
        }
        else
        {
            // name only:

            // compose full file name
            CHAR szFull[CCHMAXPATH];
            FILESTATUS3 fs3;
            BOOL fIsDir = FALSE;

            sprintf(szFull,
                    "%c:%s\\%s",
                    'A' + pWinData->ulLogicalDrive - 1,        // drive letter
                    pWinData->szDir,
                    p2);        // entry
            if (!DosQueryPathInfo(szFull,
                                  FIL_STANDARD,
                                  &fs3,
                                  sizeof(fs3)))
            {
                // this thing exists:
                if (fs3.attrFile & FILE_DIRECTORY)
                    fIsDir = TRUE;
            }

            if (fIsDir)
            {
                strcat(pWinData->szDir, "\\");
                strcat(pWinData->szDir, p2);
                ulChanged |= FFL_PATH;
            }
            else
            {
                // this doesn't exist, or it is a file:
                strcpy(pWinData->szFileName,
                       p2);
                ulChanged |= FFL_FILENAME;
            }
        }
    }

    return (ulChanged);
}

/*
 *@@ GetFSFromRecord:
 *      returns the WPFileSystem* which is represented
 *      by the specified record.
 *
 *      Returns NULL
 *
 *      -- if (fFoldersOnly) and precc does not represent
 *         a folder;
 *
 *      -- if (!fFoldersOnly) and precc does not represent
 *         a file-system object.
 *
 *      This resolves shadows and returns root folders
 *      for WPDisk objects cleanly. Returns NULL
 *      if either pobj is not valid or does not
 *      represent a folder.
 *
 *@@added V0.9.9 (2001-03-11) [umoeller]
 */

WPFileSystem* GetFSFromRecord(PMINIRECORDCORE precc,
                              BOOL fFoldersOnly)
{
    WPObject *pobj = NULL;
    if (precc)
    {
        pobj = OBJECT_FROM_PREC(precc);
        if (pobj)
        {
            if (_somIsA(pobj, _WPShadow))
                pobj = _wpQueryShadowedObject(pobj, TRUE);

            if (pobj)
            {
                if (_somIsA(pobj, _WPDisk))
                    pobj = wpshQueryRootFolder(pobj,
                                               FALSE,   // no change map
                                               NULL);

                if (pobj)
                {
                    if (fFoldersOnly)
                    {
                        if (!_somIsA(pobj, _WPFolder))
                            pobj = NULL;
                    }
                    else
                        if (!_somIsA(pobj, _WPFileSystem))
                            pobj = NULL;
                }
            }
        }
    }

    return (pobj);
}

/*
 *@@ IsInsertable:
 *      checks if pObject can be inserted in a container.
 *
 *      --  If (ulFoldersOnly == 0), this inserts all
 *          WPFileSystem and WPDisk objects plus shadows pointing
 *          to them.
 *
 *          For file-system objects, if (pcszFileMask != NULL), the
 *          object's real name is checked against that file mask also.
 *          For example, if (pcszFileMask == *.TXT), this will
 *          return TRUE only if pObject's real name matches
 *          *.TXT.
 *
 *          We never insert other abstracts or transients
 *          because these cannot be opened with the file dialog.
 *
 *          This is for the right (files) view.
 *
 *      --  If (ulFoldersOnly == 1), only folders are inserted.
 *          We will not even insert disk objects or shadows,
 *          even if they point to shadows.
 *
 *          pcszFileMask is ignored in this case.
 *
 *          This is for the left (drives) view when items
 *          are expanded.
 *
 *          This will NOT resolve shadows because if we insert
 *          shadows of folders into a container, their contents
 *          cannot be inserted a second time. The WPS shares
 *          records so each object can only be inserted once.
 *
 *      --  If (ulFoldersOnly == 2), in addition to folders
 *          (as with ulFoldersOnly == 1), we allow insertion
 *          of drive objects too.
 *
 *      In any case, FALSE is returned if the object matches
 *      the above, but the object has the "hidden" attribute on.
 */

BOOL IsInsertable(WPObject *pObject,
                  BOOL ulFoldersOnly,
                  const char *pcszFileMask)
{
    if (ulFoldersOnly)
    {
        // folders only:
        if (pObject)
        {
            // allow disks for mode 2 only
            if (    (ulFoldersOnly == 2)
                 && (_somIsA(pObject, _WPDisk))
               )
            {
                // always insert, even if drive not ready
                return (TRUE);
            }
            else
                if (_somIsA(pObject, _WPFolder))
                {
                    // filter out folder templates and hidden folders
                    if (    (!(_wpQueryStyle(pObject) & OBJSTYLE_TEMPLATE))
                         && (!(_wpQueryAttr(pObject) & FILE_HIDDEN))
                       )
                        return (TRUE);
                }
        }
    }
    else
    {
        // not folders-only:

        // resolve shadows
        if (pObject)
            if (_somIsA(pObject, _WPShadow))
                pObject = _wpQueryShadowedObject(pObject, TRUE);

        if (!pObject)
            // broken:
            return (FALSE);

        if (_somIsA(pObject, _WPDisk))
            return (TRUE);

        if (    // filter out non-file systems (shadows pointing to them have been resolved):
                (_somIsA(pObject, _WPFileSystem))
                // filter out hidden objects:
             && (!(_wpQueryAttr(pObject) & FILE_HIDDEN))
           )
        {
            // OK, non-hidden file-system object:
            // regardless of filters, always insert folders
            if (_somIsA(pObject, _WPFolder))
                return (TRUE);          // templates too

            if ((pcszFileMask) && (*pcszFileMask))
            {
                // file mask specified:
                CHAR szRealName[CCHMAXPATH];
                if (_wpQueryFilename(pObject,
                                     szRealName,
                                     FALSE))       // not q'fied
                    return (strhMatchOS2(pcszFileMask, szRealName));
            }
            else
                // no file mask:
                return (TRUE);
        }
    }

    return (FALSE);
}

/*
 *@@ InsertFirstChild:
 *
 */

VOID InsertFirstChild(HWND hwndMainClient,              // in: wnd to send XM_INSERTOBJARRAY to
                      HWND hwndCnr,                     // in: cnr to insert reccs to
                      WPFolder *pFolder,                // in: folder whose contents are to be inserted
                      PMINIRECORDCORE precParent,       // in: parent recc or NULL
                      PLINKLIST pllObjects,             // in/out: if != NULL, list where
                                                        // to append objects to
                      PLINKLIST pllRecords,             // in/out: if != NULL, list where
                                                        // to append inserted records to
                      PBOOL pfExit)                     // in: when this goes TRUE, we exit
{
    PMINIRECORDCORE precFirstChild
        = (PMINIRECORDCORE)WinSendMsg(hwndCnr,
                                      CM_QUERYRECORD,
                                      (MPARAM)precParent,
                                      MPFROM2SHORT(CMA_FIRSTCHILD, CMA_ITEMORDER));
    if (    (precFirstChild == 0)
         || ((ULONG)precFirstChild != -1)
       )
    {
        // we don't have a first child already:
        WPFolder *pFirstChildFolder = NULL;

        // check if we have a subfolder in the folder already
        BOOL fFolderLocked = FALSE;

        TRY_LOUD(excpt1)
        {
            fFolderLocked = !wpshRequestFolderMutexSem(pFolder, SEM_INDEFINITE_WAIT);
            if (fFolderLocked)
            {
                WPObject    *pObject;
                POINTL      ptlIcon = {0, 0};
                somTD_WPFolder_wpQueryContent rslv_wpQueryContent
                        = (somTD_WPFolder_wpQueryContent)wpshResolveFor(pFolder,
                                                                        NULL,
                                                                        "wpQueryContent");

                // 1) count objects
                for (   pObject = rslv_wpQueryContent(pFolder, NULL, QC_FIRST);
                        (pObject) && (!*pfExit);
                        pObject = rslv_wpQueryContent(pFolder, pObject, QC_NEXT)
                    )
                {
                    if (IsInsertable(pObject,
                                     TRUE,      // folders only
                                     NULL))
                    {
                        pFirstChildFolder = pObject;
                        break;
                    }
                }
            }
        }
        CATCH(excpt1) {} END_CATCH();

        if (fFolderLocked)
        {
            wpshReleaseFolderMutexSem(pFolder);
            fFolderLocked = FALSE;
        }

        if (!pFirstChildFolder)
        {
            // no folder awake in folder yet:
            // do a quick DosFindFirst loop to find the
            // first subfolder in here
            HDIR          hdir = HDIR_CREATE;
            FILEFINDBUF3  ffb3     = {0};
            ULONG         cbFFB3 = sizeof(FILEFINDBUF3);
            ULONG         ulFindCount    = 1;        // look for 1 file at a time
            APIRET        arc            = NO_ERROR;

            CHAR          szFolder[CCHMAXPATH],
                          szSearchMask[CCHMAXPATH];

            _wpQueryFilename(pFolder, szFolder, TRUE);
            sprintf(szSearchMask, "%s\\*", szFolder);

            ulFindCount = 1;
            arc = DosFindFirst(szSearchMask,
                               &hdir,
                               MUST_HAVE_DIRECTORY | FILE_ARCHIVED | FILE_SYSTEM | FILE_READONLY,
                                     // but exclude hidden
                               &ffb3,
                               sizeof(ffb3),
                               &ulFindCount,
                               FIL_STANDARD);

            while ((arc == NO_ERROR) && (!*pfExit))
            {
                // do not use "." and ".."
                if (    (strcmp(ffb3.achName, ".") != 0)
                     && (strcmp(ffb3.achName, "..") != 0)
                   )
                {
                    // this is good:
                    CHAR szFolder2[CCHMAXPATH];
                    sprintf(szFolder2, "%s\\%s", szFolder, ffb3.achName);
                    pFirstChildFolder = _wpclsQueryFolder(_WPFolder, szFolder2, TRUE);
                    break;
                }

                // search next file
                ulFindCount = 1;
                arc = DosFindNext(hdir,
                                 &ffb3,
                                 sizeof(ffb3),
                                 &ulFindCount);

            } // end while (rc == NO_ERROR)

            DosFindClose(hdir);
        }

        if (pFirstChildFolder)
        {
            INSERTOBJECTSARRAY ioa;
            ioa.pFolder = pFolder;
            ioa.hwndCnr = hwndCnr;
            ioa.papObjects = &pFirstChildFolder;
            ioa.cObjects = 1;
            ioa.precParent = precParent;
            ioa.pllObjects = pllObjects;
            ioa.pllRecords = pllRecords;
            // cross-thread send:
            // have the main thread insert the objects
            // because it will have to re-subclass the
            // container owner probably
            WinSendMsg(hwndMainClient,
                       XM_INSERTOBJARRAY,
                       (MPARAM)&ioa,
                       0);
        }
    }
}

/*
 *@@ InsertFolderContents:
 *      populates pFolder and inserts the contents of
 *      pFolder into hwndCnr.
 *
 *      You should set the view attributes of hwndCnr
 *      before calling this; they are not modified.
 *
 *      If (precParent == NULL), the object records
 *      are inserted at the root level. Only for tree
 *      views, you may set precParent to an existing
 *      record in the cnr to insert folder contents
 *      into a subtree. precParent should then be the
 *      record matching pFolder in the tree, of course.
 *
 *      This function filters out records according
 *      to ulFoldersOnly and pcszFileMask, which are
 *      passed to IsInsertable() for filtering.
 *
 *      While this is running, it keeps checking if
 *      *pfExit is TRUE. If so, the routine exits
 *      immediately. This is useful for having this
 *      routine running in a second thread.
 *
 *      After all objects have been collected and filtered,
 *      this sends (!) XM_INSERTOBJARRAY to hwndMainClient,
 *      which must then insert the objects specified in
 *      the INSERTOBJECTSARRAY pointed to by mp1 and maybe
 *      re-subclass the container owner.
 */

ULONG InsertFolderContents(HWND hwndMainClient,         // in: wnd to send XM_INSERTOBJARRAY to
                           HWND hwndCnr,                // in: cnr to insert reccs to
                           WPFolder *pFolder,           // in: folder whose contents are to be inserted
                           PMINIRECORDCORE precParent,  // in: parent recc or NULL
                           ULONG ulFoldersOnly,         // in: folders only?
                           const char *pcszFileMask,    // in: file mask or NULL
                           PLINKLIST pllObjects,        // in/out: if != NULL, list where
                                                        // to append objects to
                           PLINKLIST pllRecords,        // in/out: if != NULL, list where
                                                        // to append inserted records to
                           PBOOL pfExit)            // in: when this goes TRUE, we exit
{
    ULONG       cObjects = 0;

    if (wpshCheckIfPopulated(pFolder,
                             (ulFoldersOnly != 0)))     // folders only?
    {
        BOOL fFolderLocked = FALSE;

        TRY_LOUD(excpt1)
        {
            fFolderLocked = !wpshRequestFolderMutexSem(pFolder, SEM_INDEFINITE_WAIT);
            if (fFolderLocked)
            {
                WPObject    *pObject;
                POINTL      ptlIcon = {0, 0};
                somTD_WPFolder_wpQueryContent rslv_wpQueryContent
                        = (somTD_WPFolder_wpQueryContent)wpshResolveFor(pFolder,
                                                                        NULL,
                                                                        "wpQueryContent");

                // 1) count objects
                for (   pObject = rslv_wpQueryContent(pFolder, NULL, QC_FIRST);
                        (pObject) && (!*pfExit);
                        pObject = rslv_wpQueryContent(pFolder, pObject, QC_NEXT)
                    )
                {
                    if (IsInsertable(pObject,
                                     ulFoldersOnly,
                                     pcszFileMask))
                        cObjects++;
                }

                // 2) build array
                if ((cObjects) && (!*pfExit))
                {
                    WPObject **papObjects = (WPObject**)malloc(sizeof(WPObject*) * cObjects);

                    if (!papObjects)
                        cObjects = 0;
                    else
                    {
                        WPObject **ppThis = papObjects;
                        for (   pObject = rslv_wpQueryContent(pFolder, NULL, QC_FIRST);
                                (pObject) && (!*pfExit);
                                pObject = rslv_wpQueryContent(pFolder, pObject, QC_NEXT)
                            )
                        {
                            if (IsInsertable(pObject,
                                             ulFoldersOnly,
                                             pcszFileMask))
                            {
                                *ppThis = pObject;
                                ppThis++;
                            }
                        }

                        if (!*pfExit)
                        {
                            INSERTOBJECTSARRAY ioa;
                            ioa.pFolder = pFolder;
                            ioa.hwndCnr = hwndCnr;
                            ioa.papObjects = papObjects;
                            ioa.cObjects = cObjects;
                            ioa.precParent = precParent;
                            ioa.pllObjects = pllObjects;
                            ioa.pllRecords = pllRecords;
                            // cross-thread send:
                            // have the main thread insert the objects
                            // because it will have to re-subclass the
                            // container owner probably
                            WinSendMsg(hwndMainClient,
                                       XM_INSERTOBJARRAY,
                                       (MPARAM)&ioa,
                                       0);
                        }

                        free(papObjects);
                    }
                }
            }
        }
        CATCH(excpt1) {} END_CATCH();

        if (fFolderLocked)
            wpshReleaseFolderMutexSem(pFolder);
    }

    return (cObjects);
}

/*
 *@@ ClearContainer:
 *
 */

ULONG ClearContainer(HWND hwndCnr,
                     PLINKLIST pllObjects)
{
    ULONG       ulrc = 0;
    PLISTNODE   pNode;
    // OK, nuke the container contents...
    // we could simply call wpCnrRemoveObject
    // for each object in the container, but
    // this causes a lot of overhead.
    // From my testing, wpCnrRemoveObject
    // does the following:
    // 1) search the object's useitems and
    //    remove the matching RECORDITEM;
    // 2) only after that, it attempts to
    //    remove the record from the container.
    //    If that fails, the useitem has been
    //    removed anyway.

    // So to speed things up, we just kill
    // the entire container contents in one
    // flush and then call wpCnrRemoveObject
    // on each object that was inserted...
    WinSendMsg(hwndCnr,
               CM_REMOVERECORD,
               NULL,                // all records
               MPFROM2SHORT(0,      // all records
                            CMA_INVALIDATE));
                                // no free, WPS shares records
    pNode = lstQueryFirstNode(pllObjects);
    while (pNode)
    {
        WPObject *pobj = (WPObject*)pNode->pItemData;
        _wpCnrRemoveObject(pobj,
                           hwndCnr);
        _wpUnlockObject(pobj);       // was locked by "insert"
        pNode = pNode->pNext;
        ulrc++;
    }

    lstClear(pllObjects);

    return (ulrc);
}

/*
 *@@ QueryCurrentPointer:
 *      returns the HPOINTER that should be used
 *      according to the present thread state.
 *
 *      Returns a HPOINTER for either the wait or
 *      arrow pointer.
 */

HPOINTER QueryCurrentPointer(HWND hwndMainClient)
{
    ULONG           idPtr = SPTR_ARROW;

    PFILEDLGDATA    pWinData = WinQueryWindowPtr(hwndMainClient, QWL_USER);

    if (pWinData)
        if (    pWinData->fInsertContentsRunning
           )
            idPtr = SPTR_WAIT;

    return (WinQuerySysPointer(HWND_DESKTOP,
                               idPtr,
                               FALSE));
}

/*
 *@@ UpdateDlgWithFullFile:
 *      updates the dialog according to the
 *      current directory/path/file fields
 *      in pWinData.
 */

VOID UpdateDlgWithFullFile(PFILEDLGDATA pWinData)
{
    PMINIRECORDCORE precSelect = NULL;

    // go thru the disks list and find the WPDisk*
    // which matches the current logical drive
    PLISTNODE pNode = lstQueryFirstNode(&pWinData->llDisks);
    WPFolder *pRootFolder = NULL;

    _Pmpf((__FUNCTION__ ": pWinData->ulLogicalDrive = %d", pWinData->ulLogicalDrive));

    while (pNode)
    {
        WPDisk *pDisk = (WPDisk*)pNode->pItemData;

        if (_wpQueryLogicalDrive(pDisk) == pWinData->ulLogicalDrive)
        {
            precSelect = _wpQueryCoreRecord(pDisk);
            pRootFolder = wpshQueryRootFolder(pDisk, FALSE, NULL);
            break;
        }

        pNode = pNode->pNext;
    }

    _Pmpf(("    precDisk = 0x%lX", precSelect));

    if ((precSelect) && (pRootFolder))
    {
        // we got a valid disk and root folder:
        WPFolder *pFullFolder;

        // awake the directory we currently have
        CHAR szFull[CCHMAXPATH];

        WinPostMsg(pWinData->hwndAddChildren,
                   ACM_ADDCHILDREN,
                   (MPARAM)precSelect,      // for disk now
                   0);

        sprintf(szFull,
                "%c:%s",
                'A' + pWinData->ulLogicalDrive - 1,        // drive letter
                pWinData->szDir);
        pFullFolder = _wpclsQueryFolder(_WPFolder, szFull, TRUE);
                    // this also awakes all folders in between
                    // the root folder and the full folder

        // now go for all the path particles, starting with the
        // root folder
        if (pFullFolder)
        {
            CHAR szComponent[CCHMAXPATH];
            PMINIRECORDCORE precParent = precSelect;    // start with disk
            WPFolder *pFdrThis = pRootFolder;
            const char *pcThis = &pWinData->szDir[1];   // start after root '\'

            _Pmpf(("    got folder %s for %s",
                    _wpQueryTitle(pFullFolder),
                    szFull));

            while (    (pFdrThis)
                    && (*pcThis)
                  )
            {
                const char *pBacksl = strchr(pcThis, '\\');
                WPFileSystem *pobj;

                _Pmpf(("    remaining: %s", pcThis));

                if (!pBacksl)
                    strcpy(szComponent, pcThis);
                else
                {
                    ULONG c = (pBacksl - pcThis);
                    memcpy(szComponent,
                           pcThis,
                           c);
                    szComponent[c] = '\0';
                }

                // now szComponent contains the current
                // path component;
                // e.g. if szDir was "F:\OS2\BOOK",
                // we now have "OS2"

                _Pmpf(("    checking component %s", szComponent));

                // find this component in pFdrThis
                pobj = fdrFindFSFromName(pFdrThis,
                                         szComponent);
                if (pobj && _somIsA(pobj, _WPFolder))
                {
                    // got that folder:
                    POINTL ptlIcon = {0, 0};
                    PMINIRECORDCORE pNew;
                    _Pmpf(("        -> got %s", _wpQueryTitle(pobj)));

                    pNew = _wpCnrInsertObject(pobj,
                                              pWinData->hwndDrivesCnr,
                                              &ptlIcon,
                                              precParent,  // parent == previous folder
                                              NULL); // next available position

                    _Pmpf(("        got precNew 0x%lX, posting ACM_ADDCHILDREN", pNew));
                    if (pNew)
                    {
                        WinPostMsg(pWinData->hwndAddChildren,
                                   ACM_ADDCHILDREN,
                                   (MPARAM)pNew,
                                   0);

                        precParent = pNew;
                        precSelect = precParent;
                    }
                    else
                        break;

                    if (pBacksl)
                    {
                        // OK, go on with that folder
                        pFdrThis = pobj;
                        pcThis = pBacksl + 1;
                    }
                    else
                        break;
                }
                else
                    break;
            } // while (pFdrThis && *pcThis)

        } // if (pFullFolder)
    } // if ((precSelect) && (pRootFolder))

    if (precSelect)
    {
        ULONG ul;
        // got valid folder, apparently:
        cnrhExpandFromRoot(pWinData->hwndDrivesCnr,
                           (PRECORDCORE)precSelect);
        ul = cnrhScrollToRecord(pWinData->hwndDrivesCnr,
                                (PRECORDCORE)precSelect,
                                CMA_ICON | CMA_TEXT | CMA_TREEICON,
                                TRUE);       // keep parent
        cnrhSelectRecord(pWinData->hwndDrivesCnr,
                         precSelect,
                         TRUE);
        if (ul && ul != 3)
            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                    "Error: cnrhScrollToRecord returned %d", ul);
    }

    _Pmpf((__FUNCTION__ ": exiting"));
}

/*
 *@@ ParseAndUpdate:
 *
 */

VOID ParseAndUpdate(PFILEDLGDATA pWinData,
                    const char *pcszFullFile)
{
     // parse the new file string
     ULONG fl = ParseFileString(pWinData,
                                pcszFullFile);

     if (fl & FFL_FILENAME)
     {
         // no wildcard, but file specified:
         pWinData->pfd->lReturn = DID_OK;
         WinPostMsg(pWinData->hwndMainClient, WM_CLOSE, 0, 0);
                 // main msg loop detects that
         // get outta here
         return;
     }

     if (fl & (FFL_DRIVE | FFL_PATH))
     {
         // drive or path specified:
         // expand that
         UpdateDlgWithFullFile(pWinData);
     }

     if (fl & FFL_FILEMASK)
     {
         // file mask changed:
         // update files list
         StartInsertContents(pWinData,
                             pWinData->precSelectedInDrives);
     }
}

/*
 *@@ BuildDisksList:
 *      builds a linked list of all WPDisk* objects
 *      in pDrivesFolder, which better be the real
 *      "Drives" folder.
 */

VOID BuildDisksList(WPFolder *pDrivesFolder,
                    PLINKLIST pllDisks)
{
    if (wpshCheckIfPopulated(pDrivesFolder,
                             FALSE))     // folders only?
    {
        BOOL fFolderLocked = FALSE;

        TRY_LOUD(excpt1)
        {
            fFolderLocked = !wpshRequestFolderMutexSem(pDrivesFolder, SEM_INDEFINITE_WAIT);
            if (fFolderLocked)
            {
                WPObject *pObject;
                somTD_WPFolder_wpQueryContent rslv_wpQueryContent
                        = (somTD_WPFolder_wpQueryContent)wpshResolveFor(pDrivesFolder,
                                                                        NULL,
                                                                        "wpQueryContent");

                // 1) count objects
                for (   pObject = rslv_wpQueryContent(pDrivesFolder, NULL, QC_FIRST);
                        (pObject);
                        pObject = rslv_wpQueryContent(pDrivesFolder, pObject, QC_NEXT)
                    )
                {
                    if (_somIsA(pObject, _WPDisk))
                        lstAppendItem(pllDisks, pObject);
                }
            }
        }
        CATCH(excpt1) {} END_CATCH();

        if (fFolderLocked)
            wpshReleaseFolderMutexSem(pDrivesFolder);
    }
}

/* ******************************************************************
 *
 *   Add-all-children thread
 *
 ********************************************************************/

/*
 *@@ fnwpAddChildren:
 *      object window for "add children" thread.
 */

MRESULT EXPENTRY fnwpAddChildren(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    switch (msg)
    {
        case WM_CREATE:
            WinSetWindowPtr(hwnd, QWL_USER, mp1);       // FILEDLGDATA
        break;

        /*
         *@@ ACM_ADDCHILDREN:
         *      adds children to the folder specified by
         *      the MINIRECORDCORE pointer in mp1.
         *
         *      This does two things:
         *
         *      1)  Populate the folder with folders only
         *          and inserts its contents into the
         *          drives container.
         *
         *      2)  Adds a first child to each of the new
         *          subfolders.
         */

        case ACM_ADDCHILDREN:
        {
            PFILEDLGDATA pWinData = WinQueryWindowPtr(hwnd, QWL_USER);
            PMINIRECORDCORE prec = (PMINIRECORDCORE)mp1;
            WPFolder *pFolder = GetFSFromRecord(prec,
                                                TRUE);      // folders only
            if (pFolder)
            {
                LINKLIST llInsertedRecords;
                PLISTNODE pNode;
                lstInit(&llInsertedRecords, FALSE);

                _Pmpf((__FUNCTION__ ": got ACM_ADDCHILDREN for %s", _wpQueryTitle(pFolder)));

                InsertFolderContents(pWinData->hwndMainClient,
                                     pWinData->hwndDrivesCnr,
                                     pFolder,
                                     prec,          // parent
                                     TRUE,          // folders only
                                     NULL,          // file mask
                                     &pWinData->llDriveObjectsInserted,
                                     &llInsertedRecords,
                                     &pWinData->tiAddChildren.fExit); // &ptiMyself->fExit);

                // now post "add first child" for each record
                // inserted; we post a second, separate message
                // because there might be several "add children"
                // msgs in the queue, and we first want to fully
                // process all full "add children" before going
                // for the "add first child" for each new child.
                // Otherwise the user has to wait a long time for
                // the tree to fully expand.
                pNode = lstQueryFirstNode(&llInsertedRecords);
                while (pNode)
                {
                    PMINIRECORDCORE precSub = (PMINIRECORDCORE)pNode->pItemData;
                    WinPostMsg(hwnd,
                               ACM_ADDFIRSTCHILD,
                               (MPARAM)precSub,
                               0);

                    pNode = pNode->pNext;
                }

                lstClear(&llInsertedRecords);
            }
        break; }

        /*
         *@@ ACM_ADDFIRSTCHILD:
         *      adds a first child to the folder specified
         *      by the MINIRECORDCORE in mp1.
         */

        case ACM_ADDFIRSTCHILD:
        {
            PFILEDLGDATA pWinData = WinQueryWindowPtr(hwnd, QWL_USER);
            PMINIRECORDCORE precSub = (PMINIRECORDCORE)mp1;
            WPFolder *pFolderSub = GetFSFromRecord(precSub,
                                                   TRUE);
            if (pFolderSub)
            {
                _Pmpf((__FUNCTION__ ": got ACM_ADDFIRSTCHILD for %s", _wpQueryTitle(pFolderSub)));

                InsertFirstChild(pWinData->hwndMainClient,
                                 pWinData->hwndDrivesCnr,
                                 pFolderSub,
                                 precSub,
                                 &pWinData->llDriveObjectsInserted,
                                 NULL,
                                 &pWinData->tiAddChildren.fExit); // &ptiMyself->fExit);
            }
        break; }

        default:
            mrc = WinDefWindowProc(hwnd, msg, mp1, mp2);
    }

    return (mrc);
}

/*
 *@@ fntAddChildren:
 *      "add children" thread. This creates an object window
 *      so that we can easily serialize the order in which
 *      children are added to the drives tree.
 */

VOID _Optlink fntAddChildren(PTHREADINFO ptiMyself)
{
    TRY_LOUD(excpt1)
    {
        QMSG qmsg;
        PFILEDLGDATA pWinData = (PFILEDLGDATA)ptiMyself->ulData;

        WinRegisterClass(ptiMyself->hab,
                         (PSZ)WC_ADDCHILDRENOBJ,
                         fnwpAddChildren,
                         0,
                         sizeof(PVOID));
        pWinData->hwndAddChildren = winhCreateObjectWindow(WC_ADDCHILDRENOBJ,
                                                           pWinData);
        // thread 1 is waiting for obj window to be created
        DosPostEventSem(ptiMyself->hevRunning);

        while (WinGetMsg(ptiMyself->hab, &qmsg, NULLHANDLE, 0, 0))
            WinDispatchMsg(ptiMyself->hab, &qmsg);

        WinDestroyWindow(pWinData->hwndAddChildren);
    }
    CATCH(excpt1) {} END_CATCH();

}

/* ******************************************************************
 *
 *   Insert-Contents thread
 *
 ********************************************************************/

/*
 *@@ fntInsertContents:
 *      adds the entire contents of the folder specified
 *      by INSERTTHREADSDATA.precc into the files container.
 *
 *      This expects a INSERTTHREADSDATA pointer
 *      as ulUser, which is free()'d on exit.
 */

VOID _Optlink fntInsertContents(PTHREADINFO ptiMyself)
{
    PINSERTTHREADSDATA pThreadData
        = (PINSERTTHREADSDATA)ptiMyself->ulData;
    if (pThreadData)
    {
        PFILEDLGDATA pWinData = pThreadData->pWinData;

        if (pWinData)
        {
            if (pThreadData->pll)
            {
                PLISTNODE pNode = lstQueryFirstNode(pThreadData->pll);
                while (pNode)
                {
                    PMINIRECORDCORE prec = (PMINIRECORDCORE)pNode->pItemData;
                    WPFolder *pFolder = GetFSFromRecord(prec, TRUE);
                    if (pFolder)
                    {
                        ClearContainer(pWinData->hwndFilesCnr,
                                       &pWinData->llFileObjectsInserted);

                        if (pFolder)
                        {
                            InsertFolderContents(pWinData->hwndMainClient,
                                                 pWinData->hwndFilesCnr,
                                                 pFolder,
                                                 NULL,      // no parent
                                                 FALSE,         // all records
                                                 pWinData->szFileMask, // file mask
                                                 &pWinData->llFileObjectsInserted,
                                                 NULL,
                                                 &ptiMyself->fExit);
                        }
                    }

                    pNode = pNode->pNext;
                }

                lstFree(pThreadData->pll);
            }

            WinPostMsg(pWinData->hwndMainClient,
                       XM_UPDATEPOINTER,
                       0, 0);
        }

        free(pThreadData);
    }
}

/*
 *@@ StartInsertContents:
 *      Returns FALSE if the thread was not started
 *      because it was already running.
 *
 *@@added V0.9.9 (2001-03-10) [umoeller]
 */

BOOL StartInsertContents(PFILEDLGDATA pWinData,
                         PMINIRECORDCORE precc)      // in: record of folder (never NULL)
{
    if (pWinData->fInsertContentsRunning)
        return (FALSE);
    else
    {
        PINSERTTHREADSDATA pData = malloc(sizeof(INSERTTHREADSDATA));
        if (pData)
        {
            pData->pWinData = pWinData;
            pData->pll = lstCreate(FALSE);
            lstAppendItem(pData->pll, precc);
            thrCreate(&pWinData->tiInsertContents,
                      fntInsertContents,
                      &pWinData->fInsertContentsRunning,
                      "InsertContents",
                      THRF_PMMSGQUEUE | THRF_WAIT,
                      (ULONG)pData);
        }
    }

    WinSetPointer(HWND_DESKTOP,
                  QueryCurrentPointer(pWinData->hwndMainClient));

    return (TRUE);
}

/* ******************************************************************
 *
 *   File dialog window procs
 *
 ********************************************************************/

/*
 *@@ MainClientCreate:
 *      part of the implementation for WM_CREATE. This
 *      creates the controls on the bottom of the main client.
 */

VOID MainClientCreate(HWND hwnd,
                      PFILEDLGDATA pWinData)
{
    // set the window font for the main client...
    // all controls will inherit this
    winhSetWindowFont(hwnd,
                      cmnQueryDefaultFont());

    pWinData->hwndTypesTxt
        = winhCreateControl(hwnd,           // parent
                            WC_STATIC,
                            "Types:",       // @@todo localize
                            WS_VISIBLE | SS_TEXT | DT_LEFT | DT_VCENTER | DT_MNEMONIC,
                            IDDI_TYPESTXT);
    lstAppendItem(&pWinData->llDialogControls, (PVOID)pWinData->hwndTypesTxt);

    pWinData->hwndTypesCombo
        = winhCreateControl(hwnd,           // parent
                            WC_ENTRYFIELD,
                            "",
                            WS_VISIBLE | ES_LEFT | ES_AUTOSCROLL | ES_MARGIN,
                            IDDI_TYPESCOMBO);
    ctlComboFromEntryField(pWinData->hwndTypesCombo,
                           CBS_DROPDOWNLIST);
    WinInsertLboxItem(pWinData->hwndTypesCombo,
                      LIT_END,
                      "<All types>");       // @@todo localize
    WinInsertLboxItem(pWinData->hwndTypesCombo,
                      LIT_END,
                      "test1");
    WinInsertLboxItem(pWinData->hwndTypesCombo,
                      LIT_END,
                      "test2");
    winhSetLboxSelectedItem(pWinData->hwndTypesCombo,
                            0,
                            TRUE);
    lstAppendItem(&pWinData->llDialogControls, (PVOID)pWinData->hwndTypesCombo);

    pWinData->hwndDirTxt
        = winhCreateControl(hwnd,           // parent
                            WC_STATIC,
                            "Directory:",   // @@todo localize
                            WS_VISIBLE | SS_TEXT | DT_LEFT | DT_VCENTER | DT_MNEMONIC,
                            IDDI_DIRTXT);

    pWinData->hwndDirValue
        = winhCreateControl(hwnd,           // parent
                            WC_STATIC,
                            "",
                            WS_VISIBLE | SS_TEXT | DT_LEFT | DT_VCENTER,
                            IDDI_DIRVALUE);

    pWinData->hwndFileTxt
        = winhCreateControl(hwnd,           // parent
                            WC_STATIC,
                            "~File:",   // @@todo localize
                            WS_VISIBLE | SS_TEXT | DT_LEFT | DT_VCENTER | DT_MNEMONIC,
                            IDDI_FILETXT);
    lstAppendItem(&pWinData->llDialogControls, (PVOID)pWinData->hwndFileTxt);

    pWinData->hwndFileEntry
        = winhCreateControl(hwnd,           // parent
                            WC_ENTRYFIELD,
                            pWinData->szFileMask,     // initial text: file mask
                            WS_VISIBLE | ES_LEFT | ES_AUTOSCROLL | ES_MARGIN,
                            IDDI_FILEENTRY);
    winhSetEntryFieldLimit(pWinData->hwndFileEntry, CCHMAXPATH - 1);
    lstAppendItem(&pWinData->llDialogControls, (PVOID)pWinData->hwndFileEntry);

    pWinData->hwndOK
        = winhCreateControl(hwnd,           // parent
                            WC_BUTTON,
                            (pWinData->pfd->pszOKButton)
                              ? pWinData->pfd->pszOKButton
                              : "~OK",          // @@todo localize
                            WS_VISIBLE | BS_PUSHBUTTON | BS_DEFAULT,
                            DID_OK);
    lstAppendItem(&pWinData->llDialogControls, (PVOID)pWinData->hwndOK);

    pWinData->hwndCancel
        = winhCreateControl(hwnd,           // parent
                            WC_BUTTON,
                            "~Cancel",          // @@todo localize
                            WS_VISIBLE | BS_PUSHBUTTON,
                            DID_CANCEL);
    lstAppendItem(&pWinData->llDialogControls, (PVOID)pWinData->hwndCancel);

    if (pWinData->pfd->fl & FDS_HELPBUTTON)
    {
        pWinData->hwndHelp
            = winhCreateControl(hwnd,           // parent
                                WC_BUTTON,
                                "~Help",        // @@todo localize
                                WS_VISIBLE | BS_PUSHBUTTON | BS_HELP,
                                DID_HELP);
        lstAppendItem(&pWinData->llDialogControls, (PVOID)pWinData->hwndHelp);
    }
}

/*
 *@@ dlghSetPrevFocus:
 *      "backward" function for rotating the focus
 *      in a dialog when the "shift+tab" keys get
 *      pressed.
 *
 *      pllWindows must be a linked list with the
 *      plain HWND window handles of the focussable
 *      controls in the dialog.
 */

VOID dlghSetPrevFocus(PLINKLIST pllWindows)
{
    // check current focus
    HWND        hwndFocus = WinQueryFocus(HWND_DESKTOP);

    PLISTNODE   pNode = lstNodeFromItem(pllWindows, (PVOID)hwndFocus);

    BOOL fRestart = FALSE;

    while (pNode)
    {
        CHAR    szClass[100];

        // previos node
        pNode = pNode->pPrevious;

        if (    (!pNode)        // no next, or not found:
             && (!fRestart)     // avoid infinite looping if bad list
           )
        {
            pNode = lstQueryLastNode(pllWindows);
            fRestart = TRUE;
        }

        if (pNode)
        {
            // check if this is a focusable control
            if (WinQueryClassName((HWND)pNode->pItemData,
                                  sizeof(szClass),
                                  szClass))
            {
                if (    (strcmp(szClass, "#5"))    // not static
                   )
                    break;
                // else: go for next then
            }
        }
    }

    if (pNode)
    {
        WinSetFocus(HWND_DESKTOP,
                    (HWND)pNode->pItemData);
    }
}

/*
 *@@ dlghSetNextFocus:
 *      "forward" function for rotating the focus
 *      in a dialog when the "ab" key gets pressed.
 *
 *      pllWindows must be a linked list with the
 *      plain HWND window handles of the focussable
 *      controls in the dialog.
 */

VOID dlghSetNextFocus(PLINKLIST pllWindows)
{
    // check current focus
    HWND        hwndFocus = WinQueryFocus(HWND_DESKTOP);

    PLISTNODE   pNode = lstNodeFromItem(pllWindows, (PVOID)hwndFocus);

    BOOL fRestart = FALSE;

    while (pNode)
    {
        CHAR    szClass[100];

        // next focus in node
        pNode = pNode->pNext;

        if (    (!pNode)        // no next, or not found:
             && (!fRestart)     // avoid infinite looping if bad list
           )
        {
            pNode = lstQueryFirstNode(pllWindows);
            fRestart = TRUE;
        }

        if (pNode)
        {
            // check if this is a focusable control
            if (WinQueryClassName((HWND)pNode->pItemData,
                                  sizeof(szClass),
                                  szClass))
            {
                if (    (strcmp(szClass, "#5"))    // not static
                   )
                    break;
                // else: go for next then
            }
        }
    }

    if (pNode)
    {
        WinSetFocus(HWND_DESKTOP,
                    (HWND)pNode->pItemData);
    }
}

/*
 *@@ MatchMnemonic:
 *      returns TRUE if the specified control matches
 *
 *
 *@@added V0.9.9 (2001-03-17) [umoeller]
 */

/*
 *@@ dlghProcessMnemonic:
 *      finds the control which matches usch
 *      and gives it the focus. If this is a
 *      static control, the next control in the
 *      list is given focus instead. (Standard
 *      dialog behavior.)
 *
 *      Pass in usch from WM_CHAR. It is assumed
 *      that the caller has already tested for
 *      the "alt" key to be depressed.
 *
 *@@added V0.9.9 (2001-03-17) [umoeller]
 */

HWND dlghProcessMnemonic(PLINKLIST pllWindows,
                         USHORT usch)
{
    HWND hwndFound = NULLHANDLE;
    PLISTNODE pNode = lstQueryFirstNode(pllWindows);
    CHAR szClass[100];

    while (pNode)
    {
        HWND hwnd = (HWND)pNode->pItemData;

        if (WinSendMsg(hwnd,
                       WM_MATCHMNEMONIC,
                       (MPARAM)usch,
                       0))
        {
            // according to the docs, only buttons and static
            // return TRUE to that msg;
            // if this is a static, give focus to the next
            // control

            _Pmpf((__FUNCTION__ ": hwnd 0x%lX", hwnd));

            // check if this is a focusable control
            if (WinQueryClassName(hwnd,
                                  sizeof(szClass),
                                  szClass))
            {
                if (!strcmp(szClass, "#3"))
                    // it's a button: click it
                    WinSendMsg(hwnd, BM_CLICK, (MPARAM)TRUE, 0);
                else if (!strcmp(szClass, "#5"))
                {
                    // it's a static: give focus to following control
                    pNode = pNode->pNext;
                    if (pNode)
                        WinSetFocus(HWND_DESKTOP, (HWND)pNode->pItemData);
                }
            }
            else
                // any other control (are there any?): give them focus
                WinSetFocus(HWND_DESKTOP, hwnd);

            // in any case, stop
            hwndFound = hwnd;
            break;
        }

        pNode = pNode->pNext;
    }

    return (hwndFound);
}

/*
 *@@ dlghEnter:
 *      presses the first button with BS_DEFAULT.
 */

BOOL dlghEnter(PLINKLIST pllWindows)
{
    PLISTNODE pNode = lstQueryFirstNode(pllWindows);
    CHAR szClass[100];
    while (pNode)
    {
        HWND hwnd = (HWND)pNode->pItemData;
        if (WinQueryClassName(hwnd,
                              sizeof(szClass),
                              szClass))
        {
            if (!strcmp(szClass, "#3"))    // button
            {
                _Pmpf((__FUNCTION__ ": found button"));
                if (    (WinQueryWindowULong(hwnd, QWL_STYLE) & (BS_PUSHBUTTON | BS_DEFAULT))
                     == (BS_PUSHBUTTON | BS_DEFAULT)
                   )
                {
                    _Pmpf(("   is default!"));
                    WinPostMsg(hwnd,
                               BM_CLICK,
                               (MPARAM)TRUE,        // upclick
                               0);
                    return (TRUE);
                }
            }
        }

        pNode = pNode->pNext;
    }

    return (FALSE);
}

/*
 *@@ MainClientChar:
 *      implementation for WM_CHAR in fnwpMainClient.
 *
 *@@added V0.9.9 (2001-03-13) [umoeller]
 */

MRESULT MainClientChar(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    BOOL brc = FALSE;               // not processed
    PFILEDLGDATA pWinData = WinQueryWindowPtr(hwnd, QWL_USER);

    if (pWinData)
    {
        USHORT usFlags    = SHORT1FROMMP(mp1);
        USHORT usch       = SHORT1FROMMP(mp2);
        USHORT usvk       = SHORT2FROMMP(mp2);

        // key down msg?
        if ((usFlags & KC_KEYUP) == 0)
        {
            _Pmpf((__FUNCTION__ ": usFlags = 0x%lX, usch = %d, usvk = %d",
                        usFlags, usch, usvk));

            if (usFlags & KC_VIRTUALKEY)
            {
                switch (usvk)
                {
                    case VK_TAB:
                    {
                        // find next focus window
                        dlghSetNextFocus(&pWinData->llDialogControls);
                    break; }

                    case VK_BACKTAB:
                        // note: shift+tab produces this!!
                        dlghSetPrevFocus(&pWinData->llDialogControls);
                    break;

                    case VK_BACKSPACE:
                    {
                        // get current selection in drives view
                        PMINIRECORDCORE prec = (PMINIRECORDCORE)WinSendMsg(
                                                pWinData->hwndDrivesCnr,
                                                CM_QUERYRECORD,
                                                (MPARAM)pWinData->precSelectedInDrives,
                                                MPFROM2SHORT(CMA_PARENT,
                                                             CMA_ITEMORDER));
                        if (prec)
                            // not at root already:
                            cnrhSelectRecord(pWinData->hwndDrivesCnr,
                                             prec,
                                             TRUE);
                    break; }

                    case VK_ESC:
                        WinPostMsg(hwnd,
                                   WM_COMMAND,
                                   (MPARAM)DID_CANCEL,
                                   0);
                    break;

                    case VK_NEWLINE:        // this comes from the main key
                    case VK_ENTER:          // this comes from the numeric keypad
                        dlghEnter(&pWinData->llDialogControls);
                    break;
                } // end switch
            } // end if (usFlags & KC_VIRTUALKEY)
            else
            {
                // no virtual key:
                // find the control for the keypress
                dlghProcessMnemonic(&pWinData->llDialogControls, usch);
            }
        }
    }

    return ((MPARAM)brc);
}

/*
 *@@ MainClientRepositionControls:
 *      part of the implementation of WM_SIZE in
 *      fnwpMainClient. This resizes all subwindows
 *      of the main client -- the split window and
 *      the controls on bottom.
 *
 *      Repositioning the split window will in turn
 *      cause the two container frames to be adjusted.
 */

VOID MainClientRepositionControls(HWND hwnd,
                                  PFILEDLGDATA pWinData,
                                  MPARAM mp2)
{
    #define OUTER_SPACING       5

    #define BUTTON_WIDTH        100
    #define BUTTON_HEIGHT       30
    #define BUTTON_SPACING      10

    #define STATICS_HEIGHT      20
    #define STATICS_LEFTWIDTH   100

    #define SPACE_BOTTOM        (BUTTON_HEIGHT + OUTER_SPACING \
                                 + 3*STATICS_HEIGHT + 3*OUTER_SPACING)
    SWP     aswp[10];       // three buttons
                            // plus two for types
                            // plus two statics for directory
                            // plus two controls for file
                            // plus split window
    ULONG   i = 0;

    memset(aswp, 0, sizeof(aswp));

    // split window
    aswp[i].fl = SWP_MOVE | SWP_SIZE;
    aswp[i].x = OUTER_SPACING;
    aswp[i].y = OUTER_SPACING + SPACE_BOTTOM;
    aswp[i].cx =   SHORT1FROMMP(mp2)   // new win cx
                 - 2 * OUTER_SPACING;
    aswp[i].cy = SHORT2FROMMP(mp2)     // new win cy
                 - 2 * OUTER_SPACING
                 - SPACE_BOTTOM;
    aswp[i].hwndInsertBehind = HWND_TOP;
    aswp[i++].hwnd = pWinData->hwndSplitWindow;

    // "Types:" static
    aswp[i].fl = SWP_MOVE | SWP_SIZE;
    aswp[i].x = OUTER_SPACING;
    aswp[i].y = BUTTON_HEIGHT + 4 * OUTER_SPACING + 2 * STATICS_HEIGHT;
    aswp[i].cx = STATICS_LEFTWIDTH;
    aswp[i].cy = STATICS_HEIGHT;
    aswp[i].hwndInsertBehind = HWND_TOP;
    aswp[i++].hwnd = pWinData->hwndTypesTxt;

    // types combobox:
    aswp[i].fl = SWP_MOVE | SWP_SIZE | SWP_ZORDER;
    aswp[i].x = 2*OUTER_SPACING + STATICS_LEFTWIDTH;
    aswp[i].y = BUTTON_HEIGHT + 4 * OUTER_SPACING + 2 * STATICS_HEIGHT;
    aswp[i].cx = SHORT1FROMMP(mp2)   // new win cx
                    - (3*OUTER_SPACING + STATICS_LEFTWIDTH);
    aswp[i].cy = STATICS_HEIGHT;
    aswp[i].hwndInsertBehind = HWND_TOP;
    aswp[i++].hwnd = pWinData->hwndTypesCombo;

    // "Directory:" static
    aswp[i].fl = SWP_MOVE | SWP_SIZE;
    aswp[i].x = OUTER_SPACING;
    aswp[i].y = BUTTON_HEIGHT + 3 * OUTER_SPACING + STATICS_HEIGHT;
    aswp[i].cx = STATICS_LEFTWIDTH;
    aswp[i].cy = STATICS_HEIGHT;
    aswp[i].hwndInsertBehind = HWND_TOP;
    aswp[i++].hwnd = pWinData->hwndDirTxt;

    // directory value static
    aswp[i].fl = SWP_MOVE | SWP_SIZE;
    aswp[i].x = 2*OUTER_SPACING + STATICS_LEFTWIDTH;
    aswp[i].y = BUTTON_HEIGHT + 3 * OUTER_SPACING + STATICS_HEIGHT;
    aswp[i].cx = SHORT1FROMMP(mp2)   // new win cx
                    - (3*OUTER_SPACING + STATICS_LEFTWIDTH);
    aswp[i].cy = STATICS_HEIGHT;
    aswp[i].hwndInsertBehind = HWND_TOP;
    aswp[i++].hwnd = pWinData->hwndDirValue;

    // "File:" static
    aswp[i].fl = SWP_MOVE | SWP_SIZE;
    aswp[i].x = OUTER_SPACING;
    aswp[i].y = BUTTON_HEIGHT + 2 * OUTER_SPACING;
    aswp[i].cx = STATICS_LEFTWIDTH;
    aswp[i].cy = STATICS_HEIGHT;
    aswp[i].hwndInsertBehind = HWND_TOP;
    aswp[i++].hwnd = pWinData->hwndFileTxt;

    // file entry field
    aswp[i].fl = SWP_MOVE | SWP_SIZE;
    aswp[i].x = 2*OUTER_SPACING + STATICS_LEFTWIDTH;
    aswp[i].y = BUTTON_HEIGHT + 2 * OUTER_SPACING + 2;
    aswp[i].cx = SHORT1FROMMP(mp2)   // new win cx
                    - (3*OUTER_SPACING + STATICS_LEFTWIDTH);
    aswp[i].cy = STATICS_HEIGHT - 2 * 2;
    aswp[i].hwndInsertBehind = HWND_TOP;
    aswp[i++].hwnd = pWinData->hwndFileEntry;

    // "OK" button
    aswp[i].fl = SWP_MOVE | SWP_SIZE;
    aswp[i].x = OUTER_SPACING;
    aswp[i].y = OUTER_SPACING;
    aswp[i].cx = BUTTON_WIDTH;
    aswp[i].cy = BUTTON_HEIGHT;
    aswp[i].hwndInsertBehind = HWND_TOP;
    aswp[i++].hwnd = pWinData->hwndOK;

    // "Cancel" button
    aswp[i].fl = SWP_MOVE | SWP_SIZE;
    aswp[i].x = OUTER_SPACING + BUTTON_SPACING + BUTTON_WIDTH;
    aswp[i].y = OUTER_SPACING;
    aswp[i].cx = BUTTON_WIDTH;
    aswp[i].cy = BUTTON_HEIGHT;
    aswp[i].hwndInsertBehind = HWND_TOP;
    aswp[i++].hwnd = pWinData->hwndCancel;

    // "Help" button
    if (pWinData->hwndHelp)
    {
        aswp[i].fl = SWP_MOVE | SWP_SIZE;
        aswp[i].x = OUTER_SPACING + 2*BUTTON_SPACING + 2*BUTTON_WIDTH;
        aswp[i].y = OUTER_SPACING;
        aswp[i].cx = BUTTON_WIDTH;
        aswp[i].cy = BUTTON_HEIGHT;
        aswp[i].hwndInsertBehind = HWND_TOP;
        aswp[i++].hwnd = pWinData->hwndHelp;
    }

    WinSetMultWindowPos(WinQueryAnchorBlock(hwnd),
                        aswp,
                        i);
}

/*
 *@@ fnwpMainClient:
 *      winproc for the main client (child of the main frame
 *      of the file dlg). By definition, this winproc is
 *      reponsible for managing the actual dialog functionality...
 *      populating subfolders depending on record selections
 *      and all that.
 *
 *      The file dlg has the following window hierarchy:
 *
 +      WC_FRAME        (main file dlg, not subclassed)
 +        |
 +        +--- WC_CLIENT (fnwpMainClient)
 +                |
 +                +--- split window (cctl_splitwin.c)
 +                |      |
 +                |      +--- left split view, WC_FRAME with ID_TREEFRAME;
 +                |      |    subclassed with fnwpSubclassedDrivesFrame
 +                |      |      |
 +                |      |      +--- WC_CONTAINER (FID_CLIENT)
 +                |      |
 +                |      +--- right split view, WC_FRAME with ID_FILESFRAME
 +                |      |    subclassed with fnwpSubclassedFilesFrame
 +                |      |      |
 +                |      |      +--- WC_CONTAINER (FID_CLIENT)
 +                |
 +                +--- buttons, entry field etc.
 +
 *
 *@@added V0.9.9 (2001-03-10) [umoeller]
 */

MRESULT EXPENTRY fnwpMainClient(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    switch (msg)
    {
        case WM_CREATE:
            if (mp1)
            {
                PFILEDLGDATA pWinData = (PFILEDLGDATA)mp1;
                // store this in window words
                WinSetWindowPtr(hwnd, QWL_USER, mp1);

                // create controls
                MainClientCreate(hwnd, pWinData);
            }
            else
                // no PFILEDLGDATA:
                // stop window creation
                mrc = (MPARAM)TRUE;
        break;

        case WM_PAINT:
        {
            RECTL rclPaint;
            HPS hps = WinBeginPaint(hwnd, NULLHANDLE, &rclPaint);
            GpiCreateLogColorTable(hps, 0, LCOLF_RGB, 0, 0, NULL);
            WinFillRect(hps,
                        &rclPaint,
                        WinQuerySysColor(HWND_DESKTOP,
                                         SYSCLR_DIALOGBACKGROUND,
                                         0));
            WinEndPaint(hps);
        break; }

        /*
         * WM_SIZE:
         *      resize all the controls.
         */

        case WM_SIZE:
        {

            PFILEDLGDATA pWinData = WinQueryWindowPtr(hwnd, QWL_USER);
            if (pWinData)
                MainClientRepositionControls(hwnd, pWinData, mp2);
        break; }

        /*
         * WM_COMMAND:
         *      button pressed.
         */

        case WM_COMMAND:
        {
            PFILEDLGDATA pWinData = WinQueryWindowPtr(hwnd, QWL_USER);

            if (pWinData->fFileDlgReady)
                switch ((ULONG)mp1)
                {
                    case DID_OK:
                    {
                        // "OK" is tricky... we first need to
                        // get the thing from the entry field
                        // and check if it contains a wildcard.
                        // For some reason people have become
                        // accustomed to this.
                        PSZ pszFullFile = winhQueryWindowText(pWinData->hwndFileEntry);
                        if (pszFullFile)
                        {
                            ParseAndUpdate(pWinData,
                                           pszFullFile);
                            free(pszFullFile);
                        }

                    break; }

                    case DID_CANCEL:
                        pWinData->pfd->lReturn = DID_CANCEL;
                        WinPostMsg(hwnd, WM_CLOSE, 0, 0);
                                // main msg loop detects that
                    break;
                }
        break; }

        /*
         * WM_CHAR:
         *      this gets forwarded to us from all the
         *      sub-frames and also the controls.
         *      We re-implement a dialog manager here.
         */

        case WM_CHAR:
            mrc = MainClientChar(hwnd, mp1, mp2);
        break;

        /*
         * XM_FILLFILESCNR:
         *      gets posted from fnwpSubclassedDrivesFrame when
         *      a tree item gets selected in the left drives tree.
         *
         *      Has MINIRECORDCORE of "folder" to fill files cnr with
         *      in mp1.
         */

        case XM_FILLFILESCNR:
        {
            PFILEDLGDATA pWinData = WinQueryWindowPtr(hwnd, QWL_USER);
            PMINIRECORDCORE prec = (PMINIRECORDCORE)mp1;

            if (prec != pWinData->precSelectedInDrives)
            {
                pWinData->precSelectedInDrives = prec;

                if (pWinData->fFileDlgReady)
                {
                    if (!StartInsertContents(pWinData,
                                             prec))
                    {
                        // thread is still busy:
                        // repost
                        winhSleep(100);
                        WinPostMsg(hwnd, msg, mp1, mp2);
                    }
                    else
                    {
                        // thread started:
                        // check if the object is a disk object
                        WPObject *pobj = OBJECT_FROM_PREC(prec);
                        if (_somIsA(pobj, _WPDisk))
                        {
                            WinPostMsg(pWinData->hwndAddChildren,
                                       ACM_ADDCHILDREN,
                                       (MPARAM)prec,
                                       0);
                        }
                    }
                }
            }
        break; }

        /*
         * XM_INSERTOBJARRAY:
         *      has INSERTOBJECTSARRAY pointer in mp1.
         *      This is _sent_ always so the pointer must
         *      not be freed.
         */

        case XM_INSERTOBJARRAY:
        {
            PFILEDLGDATA pWinData = WinQueryWindowPtr(hwnd, QWL_USER);
            PINSERTOBJECTSARRAY pioa = (PINSERTOBJECTSARRAY)mp1;
            if (pioa)
            {
                CHAR    szPathName[CCHMAXPATH];
                ULONG   ul;
                POINTL  ptlIcon = {0, 0};
                // _Pmpf(("XM_INSERTOBJARRAY: inserting %d recs", pioa->cObjects));

                // now differentiate...
                // if this is for the "files" container,
                // we can do wpclsInsertMultipleObjects,
                // which is way faster.
                if (pioa->hwndCnr != pWinData->hwndFilesCnr)
                {
                    // this is for the "drives" container:
                    // we can't use wpclsInsertMultipleObjects
                    // because this fails if only one object
                    // has already been inserted, which is
                    // frequently the case here. Dull WPS.
                    // So we need to insert the objects one
                    // by one.
                    for (ul = 0;
                         ul < pioa->cObjects;
                         ul++)
                    {
                        BOOL fAppend = FALSE;
                        WPObject *pobjThis = pioa->papObjects[ul];

                        PMINIRECORDCORE prec;
                        if (prec = _wpCnrInsertObject(pobjThis,
                                                      pioa->hwndCnr,
                                                      &ptlIcon,
                                                      pioa->precParent,
                                                      NULL))
                        {
                            // lock the object!! we must make sure
                            // the WPS won't let it go dormant
                            _wpLockObject(pobjThis);
                                    // unlock is in "clear container"

                            if (pioa->pllObjects)
                                lstAppendItem(pioa->pllObjects, pobjThis);
                            if (pioa->pllRecords)
                                lstAppendItem(pioa->pllRecords, prec);
                        }
                    }
                }
                else
                {
                    // files container:
                    // then we can use fast insert
                    // _Pmpf(("    fast insert"));
                    _wpclsInsertMultipleObjects(_somGetClass(pioa->pFolder),
                                                pioa->hwndCnr,
                                                NULL,
                                                (PVOID*)pioa->papObjects,
                                                pioa->precParent,
                                                pioa->cObjects);
                    // _Pmpf(("    fast insert done, locking"));

                    for (ul = 0;
                         ul < pioa->cObjects;
                         ul++)
                    {
                        BOOL fAppend = FALSE;
                        WPObject *pobjThis = pioa->papObjects[ul];

                        // lock the object!! we must make sure
                        // the WPS won't let it go dormant
                        _wpLockObject(pobjThis);
                                // unlock is in "clear container"
                        if (pioa->pllObjects)
                            lstAppendItem(pioa->pllObjects, pobjThis);
                    }
                    // _Pmpf(("    done locking"));

                    // set new "directory" on bottom
                    _wpQueryFilename(pioa->pFolder,
                                     szPathName,
                                     TRUE);     // fully q'fied
                    WinSetWindowText(pWinData->hwndDirValue,
                                     szPathName);

                    // now, if this is for the files cnr, we must make
                    // sure that the container owner (the parent frame)
                    // has been subclassed by us... we do this AFTER
                    // the WPS subclasses the owner, which happens during
                    // record insertion

                    if (!pWinData->fFilesFrameSubclassed)
                    {
                        // not subclassed yet:
                        // subclass now

                        pWinData->psfvFiles
                            = fdrCreateSFV(pWinData->hwndFilesFrame,
                                           pWinData->hwndFilesCnr,
                                           QWL_USER,
                                           pioa->pFolder,
                                           pioa->pFolder);
                        pWinData->psfvFiles->pfnwpOriginal
                            = WinSubclassWindow(pWinData->hwndFilesFrame,
                                                fnwpSubclassedFilesFrame);
                        pWinData->fFilesFrameSubclassed = TRUE;
                    }
                    else
                    {
                        // already subclassed:
                        // update the folder pointers in the SFV
                        pWinData->psfvFiles->somSelf = pioa->pFolder;
                        pWinData->psfvFiles->pRealObject = pioa->pFolder;
                    }
                }

                // _Pmpf(("XM_INSERTOBJARRAY: done"));
            }

        break; }

        /*
         * XM_UPDATEPOINTER:
         *      posted when threads exit etc. to update
         *      the current pointer.
         */

        case XM_UPDATEPOINTER:
            WinSetPointer(HWND_DESKTOP,
                          WinQuerySysPointer(HWND_DESKTOP,
                                             SPTR_ARROW,
                                             FALSE));
        break;

        default:
            mrc = WinDefWindowProc(hwnd, msg, mp1, mp2);
    }

    return (mrc);
}

/*
 *@@ fnwpSubclassedDrivesFrame:
 *      subclassed frame window on the left for the
 *      "Drives" container.
 *
 *      We use the XFolder subclassed window proc for
 *      most messages. In addition, we intercept a
 *      couple more for extra features.
 */

MRESULT EXPENTRY fnwpSubclassedDrivesFrame(HWND hwndFrame, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT                 mrc = 0;
    BOOL                    fCallDefault = FALSE;
    PSUBCLASSEDFOLDERVIEW   psfv = WinQueryWindowPtr(hwndFrame, QWL_USER);

    switch (msg)
    {
        case WM_CONTROL:
        {
            USHORT usID = SHORT1FROMMP(mp1),
                   usNotifyCode = SHORT2FROMMP(mp1);
            if (usID == FID_CLIENT)     // that's the container
            {
                switch (usNotifyCode)
                {
                    /*
                     * CN_EXPANDTREE:
                     *      user clicked on "+" sign next to
                     *      tree item; expand that, but start
                     *      "add first child" thread again
                     */

                    case CN_EXPANDTREE:
                    {
                        HWND hwndMainClient;
                        PFILEDLGDATA pWinData;
                        if (    (hwndMainClient = WinQueryWindow(hwndFrame, QW_OWNER))
                             && (pWinData = WinQueryWindowPtr(hwndMainClient, QWL_USER))
                           )
                        {
                            WinPostMsg(pWinData->hwndAddChildren,
                                       ACM_ADDCHILDREN,
                                       (MPARAM)mp2,
                                       0);
                        }
                        // and call default because xfolder
                        // handles auto-scroll
                        fCallDefault = TRUE;
                    break; }

                    /*
                     * CN_ENTER:
                     *      intercept this so that we won't open
                     *      a folder view.
                     *
                     *      Before this, we should have gotten
                     *      CN_EMPHASIS so the files list has
                     *      been updated already.
                     *
                     *      Instead, check whether the record has
                     *      been expanded or collapsed and do
                     *      the reverse.
                     */

                    case CN_ENTER:
                    {
                        PNOTIFYRECORDENTER pnre;
                        PMINIRECORDCORE prec;

                        if (    (pnre = (PNOTIFYRECORDENTER)mp2)
                             && (prec = (PMINIRECORDCORE)pnre->pRecord)
                                        // can be null for whitespace!
                           )
                        {
                            ULONG ulmsg = CM_EXPANDTREE;
                            if (prec->flRecordAttr & CRA_EXPANDED)
                                ulmsg = CM_COLLAPSETREE;

                            WinPostMsg(pnre->hwndCnr,
                                       ulmsg,
                                       (MPARAM)prec,
                                       0);
                        }
                    break; }

                    /*
                     * CN_EMPHASIS:
                     *      selection changed:
                     */

                    case CN_EMPHASIS:
                    {
                        PNOTIFYRECORDEMPHASIS pnre = (PNOTIFYRECORDEMPHASIS)mp2;
                        if (    (pnre->pRecord)
                             && (pnre->fEmphasisMask & CRA_SELECTED)
                             && (((PMINIRECORDCORE)(pnre->pRecord))->flRecordAttr & CRA_SELECTED)
                           )
                        {
                            WinPostMsg(WinQueryWindow(hwndFrame, QW_OWNER),
                                                    // the main client
                                       XM_FILLFILESCNR,
                                       (MPARAM)pnre->pRecord,
                                       0);
                        }
                    break; }

                    default:
                        fCallDefault = TRUE;
                }
            }
            else
                fCallDefault = TRUE;
        break; }

        case WM_CHAR:
            // forward to main client
            WinPostMsg(WinQueryWindow(hwndFrame, QW_OWNER),
                       msg,
                       mp1,
                       mp2);
        break;

        case WM_SYSCOMMAND:
            // forward to main frame
            WinPostMsg(WinQueryWindow(WinQueryWindow(hwndFrame, QW_OWNER),
                                      QW_OWNER),
                       msg,
                       mp1,
                       mp2);
        break;

        case WM_CONTROLPOINTER:
        {
            HWND hwndMainClient = WinQueryWindow(hwndFrame, QW_OWNER);
                                    // the main client
            mrc = (MPARAM)QueryCurrentPointer(hwndMainClient);
        break; }

        default:
            fCallDefault = TRUE;
    }

    if (fCallDefault)
        mrc = fdrProcessFolderMsgs(hwndFrame,
                                   msg,
                                   mp1,
                                   mp2,
                                   psfv,
                                   psfv->pfnwpOriginal);

    return (mrc);
}

/*
 *@@ fnwpSubclassedFilesFrame:
 *      subclassed frame window on the right for the
 *      "Files" container.
 *
 *      We use the XFolder subclassed window proc for
 *      most messages. In addition, we intercept a
 *      couple more for extra features.
 */

MRESULT EXPENTRY fnwpSubclassedFilesFrame(HWND hwndFrame, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT                 mrc = 0;
    BOOL                    fCallDefault = FALSE;
    PSUBCLASSEDFOLDERVIEW   psfv = WinQueryWindowPtr(hwndFrame, QWL_USER);

    switch (msg)
    {
        case WM_CONTROL:
        {
            USHORT usID = SHORT1FROMMP(mp1),
                   usNotifyCode = SHORT2FROMMP(mp1);
            if (usID == FID_CLIENT)     // that's the container
            {
                switch (usNotifyCode)
                {
                    case CN_ENTER:
                    {
                        PNOTIFYRECORDENTER pnre;
                        PMINIRECORDCORE prec;
                        HWND hwndMainClient;
                        PFILEDLGDATA pWinData;
                        WPObject *pobj;

                        if (    (pnre = (PNOTIFYRECORDENTER)mp2)
                             && (prec = (PMINIRECORDCORE)pnre->pRecord)
                                        // can be null for whitespace!
                             && (hwndMainClient = WinQueryWindow(hwndFrame, QW_OWNER))
                             && (pWinData = WinQueryWindowPtr(hwndMainClient, QWL_USER))
                             && (pobj = GetFSFromRecord(prec, FALSE))
                           )
                        {
                            CHAR szFullFile[CCHMAXPATH];
                            if (_wpQueryFilename(pobj,
                                                 szFullFile,
                                                 TRUE))     // fully q'fied
                            {
                                ParseAndUpdate(pWinData,
                                               szFullFile);
                            }
                        }
                    break; }

                    /*
                     * CN_EMPHASIS:
                     *      selection changed:
                     */

                    case CN_EMPHASIS:
                    {
                        PNOTIFYRECORDEMPHASIS pnre = (PNOTIFYRECORDEMPHASIS)mp2;
                        WPObject *pobj;
                        CHAR szFilename[CCHMAXPATH];
                        HWND hwndMainClient;
                        PFILEDLGDATA pWinData;
                        // if it's not a folder, update the entry field
                        // with the file's name:
                        if (    (pnre->pRecord)
                             && (pnre->fEmphasisMask & CRA_SELECTED)
                             && (((PMINIRECORDCORE)(pnre->pRecord))->flRecordAttr & CRA_SELECTED)
                             && (pobj = GetFSFromRecord((PMINIRECORDCORE)(pnre->pRecord),
                                                        FALSE))
                             && (!_somIsA(pobj, _WPFolder))
                             && (_wpQueryFilename(pobj, szFilename, FALSE))
                             && (hwndMainClient = WinQueryWindow(hwndFrame, QW_OWNER))
                             && (pWinData = WinQueryWindowPtr(hwndMainClient, QWL_USER))
                           )
                        {
                            strcpy(pWinData->szFileName, szFilename);
                            WinSetWindowText(pWinData->hwndFileEntry, szFilename);
                        }
                    break; }

                    default:
                        fCallDefault = TRUE;
                }
            }
            else
                fCallDefault = TRUE;
        break; }

        case WM_CHAR:
            // forward to main client
            WinPostMsg(WinQueryWindow(hwndFrame, QW_OWNER),
                       msg,
                       mp1,
                       mp2);
        break;

        case WM_SYSCOMMAND:
            // forward to main frame
            WinPostMsg(WinQueryWindow(WinQueryWindow(hwndFrame, QW_OWNER),
                                      QW_OWNER),
                       msg,
                       mp1,
                       mp2);
        break;

        case WM_CONTROLPOINTER:
        {
            HWND hwndMainClient = WinQueryWindow(hwndFrame, QW_OWNER);
                                    // the main client
            mrc = (MPARAM)QueryCurrentPointer(hwndMainClient);
        break; }

        default:
            fCallDefault = TRUE;
    }

    if (fCallDefault)
        mrc = fdrProcessFolderMsgs(hwndFrame,
                                   msg,
                                   mp1,
                                   mp2,
                                   psfv,
                                   psfv->pfnwpOriginal);

    return (mrc);
}

/* ******************************************************************
 *
 *   File dialog API
 *
 ********************************************************************/

/*
 *@@ CreateFrameWithCnr:
 *
 */

HWND CreateFrameWithCnr(ULONG ulFrameID,
                        PFILEDLGDATA pWinData,
                        BOOL fMultipleSelection,
                        HWND *phwndClient)          // out: client window
{
    HWND hwndFrame;
    ULONG ws;

    if (fMultipleSelection)
        ws =  WS_VISIBLE | WS_SYNCPAINT
                | CCS_AUTOPOSITION
                | CCS_MINIRECORDCORE
                | CCS_MINIICONS
                | CCS_EXTENDSEL;      // one or many items, but not zero
    else
        // single selection:
        ws =  WS_VISIBLE | WS_SYNCPAINT
                | CCS_AUTOPOSITION
                | CCS_MINIRECORDCORE
                | CCS_MINIICONS
                | CCS_SINGLESEL;        // one item at a time

    hwndFrame = winhCreateStdWindow(pWinData->hwndMainClient, // parent
                                    NULL,          // pswpFrame
                                    FCF_NOBYTEALIGN,
                                    WS_VISIBLE,
                                    "",
                                    0,             // resources ID
                                    WC_CONTAINER,  // client
                                    ws,            // client style
                                    ulFrameID,
                                    NULL,
                                    phwndClient);
    // set client as owner
    WinSetOwner(hwndFrame, pWinData->hwndMainClient);

    return (hwndFrame);
}

/*
 *@@ fdlgFileDlg:
 *      main entry point into all this mess. This displays a
 *      file dialog with full WPS support, including shadows,
 *      WPS context menus, and all that.
 *
 *      See fnwpMainClient for the (complex) window hierarchy.
 *
 *      Supported file-dialog flags in FILEDLG.fl:
 *
 *      -- FDS_FILTERUNION: when this flag is set, the dialog
 *         uses the union of the string filter and the
 *         extended-attribute type filter when filtering files
 *         for the Files list box. When this flag is not set,
 *         the list box, by default, uses the intersection of the
 *         two. @@todo
 *
 *      -- FDS_HELPBUTTON: a "Help" button is added which will
 *         post WM_HELP to hwndOwner.
 *
 *      -- FDS_MULTIPLESEL: when this flag is set, the Files container
 *         for the dialog is set to allow multiple selections. When
 *         this flag is not set, CCS_SINGLESEL is enabled.
 *
 *      -- FDS_OPEN_DIALOG or FDS_SAVEAS_DIALOG: one of the two
 *         should be set.
 *
 *      The following "fl" flags are ignored: FDS_APPLYBUTTON,
 *      FDS_CENTER, FDS_CUSTOM, FDS_ENABLEFILELB, FDS_INCLUDE_EAS,
 *      FDS_MODELESS, FDS_PRELOAD_VOLINFO.
 *
 *      FILEDLG.pszIType, papszITypeList, and sEAType are supported. @@todo
 *
 *      When FDS_MULTIPLESEL has been specified, FILEDLG.papszFQFileName
 *      receives the array of selected file names, as with WinFileDlg,
 *      and FILEDLG.ulFQFileCount receives the count. @@todo
 *
 *      The following other FILEDLG fields are ignored: ulUser,
 *      pfnDlgProc, pszIDrive, pszIDriveList, hMod, usDlgID, x, y.
 */

HWND fdlgFileDlg(HWND hwndOwner,
                 PFILEDLG pfd)
{
    HWND    hwndReturn = NULLHANDLE;
    static  s_fRegistered = FALSE;

    // first, some compatibility checks...
    if (    (!pfd)
         || (pfd->cbSize != sizeof(FILEDLG))
         || (pfd->fl & (FDS_CUSTOM | FDS_MODELESS))
         || (pfd->pfnDlgProc)
       )
        hwndReturn = WinFileDlg(HWND_DESKTOP, hwndOwner, pfd);
    else
    {
        // OK, it seems that we can handle this
        TRY_LOUD(excpt1)
        {
            // create left frame
            FILEDLGDATA WinData = {0};

            CHAR        szCurDir[CCHMAXPATH] = "";
            ULONG       cbCurDir;
            APIRET      arc;
            PSZ         pszDlgTitle;

            WinData.pfd = pfd;
            pfd->lReturn = DID_CANCEL;           // for now

            lstInit(&WinData.llDialogControls, FALSE);

            lstInit(&WinData.llDriveObjectsInserted, FALSE);
            lstInit(&WinData.llFileObjectsInserted, FALSE);
            lstInit(&WinData.llDisks, FALSE);

            // set wait pointer, since this may take a second
            winhSetWaitPointer();

            /*
             *  PATH/FILE MASK SETUP
             *
             */

            // OK, here's the trick. We first call ParseFile
            // string with the current directory plus the "*"
            // file mask; we then call it a second time with
            // full path string given to us in FILEDLG.
            doshQueryCurrentDir(szCurDir);
            if (strlen(szCurDir) > 3)
                strcat(szCurDir, "\\*");
            else
                strcat(szCurDir, "*");
            ParseFileString(&WinData,
                            szCurDir);

            _Pmpf((__FUNCTION__ ": pfd->szFullFile is %s", pfd->szFullFile));
            ParseFileString(&WinData,
                            pfd->szFullFile);

            /*
             *  WINDOW CREATION
             *
             */

            if (!s_fRegistered)
            {
                HWND hwnd = winhCreateObjectWindow(WC_BUTTON, NULL);
                HAB hab = WinQueryAnchorBlock(hwnd);
                WinRegisterClass(hab,
                                 (PSZ)WC_FILEDLGCLIENT,
                                 fnwpMainClient,
                                 CS_CLIPCHILDREN | CS_SIZEREDRAW,
                                 sizeof(PFILEDLGDATA));
                WinDestroyWindow(hwnd);
                s_fRegistered = TRUE;
            }

            pszDlgTitle = pfd->pszTitle;
            if (!pszDlgTitle)
                // no user title specified:
                if (pfd->fl & FDS_SAVEAS_DIALOG)
                    pszDlgTitle = "Save File As...";
                else
                    pszDlgTitle = "Open File...";

            // create main frame and client
            WinData.hwndMainFrame = winhCreateStdWindow(HWND_DESKTOP,   // parent
                                                        NULL,
                                                        FCF_NOBYTEALIGN
                                                          | FCF_TITLEBAR
                                                          | FCF_SYSMENU
                                                          | FCF_MAXBUTTON
                                                          | FCF_SIZEBORDER
                                                          | FCF_AUTOICON,
                                                        0,  // frame style, not visible yet
                                                        pszDlgTitle,    // frame title
                                                        0,  // resids
                                                        WC_FILEDLGCLIENT,
                                                        WS_VISIBLE, // client style
                                                        0,  // frame ID
                                                        &WinData,
                                                        &WinData.hwndMainClient);
            if (!WinData.hwndMainFrame || !WinData.hwndMainClient)
                cmnLog(__FILE__, __LINE__, __FUNCTION__,
                        "Cannot create main window.");
            else
            {
                SPLITBARCDATA sbcd;
                HAB hab = WinQueryAnchorBlock(WinData.hwndMainFrame);
                QMSG qmsg;

                // create two subframes to be linked in split window

                // 1) left: drives tree
                WinData.hwndDrivesFrame = CreateFrameWithCnr(ID_TREEFRAME,
                                                             &WinData,
                                                             FALSE,     // single sel
                                                             &WinData.hwndDrivesCnr);
                BEGIN_CNRINFO()
                {
                    cnrhSetView(   CV_TREE | CA_TREELINE | CV_ICON
                                 | CV_MINI);
                    cnrhSetTreeIndent(30);
                } END_CNRINFO(WinData.hwndDrivesCnr);

                lstAppendItem(&WinData.llDialogControls, (PVOID)WinData.hwndDrivesCnr);

                // 2) right: files
                WinData.hwndFilesFrame = CreateFrameWithCnr(ID_FILESFRAME,
                                                            &WinData,
                                                            ((pfd->fl & FDS_MULTIPLESEL) != 0),
                                                                    // multiple sel?
                                                            &WinData.hwndFilesCnr);
                BEGIN_CNRINFO()
                {
                    cnrhSetView(   CV_NAME | CV_FLOW
                                 | CV_MINI);
                    cnrhSetTreeIndent(30);
                } END_CNRINFO(WinData.hwndFilesCnr);

                lstAppendItem(&WinData.llDialogControls, (PVOID)WinData.hwndFilesCnr);

                // 3) fonts
                winhSetWindowFont(WinData.hwndDrivesCnr,
                                  cmnQueryDefaultFont());
                winhSetWindowFont(WinData.hwndFilesCnr,
                                  cmnQueryDefaultFont());

                // create split window
                sbcd.ulSplitWindowID = 1;
                    // split window becomes client of main frame
                sbcd.ulCreateFlags =   SBCF_VERTICAL
                                     | SBCF_PERCENTAGE
                                     | SBCF_3DSUNK
                                     | SBCF_MOVEABLE;
                sbcd.lPos = 30;     // percent
                sbcd.ulLeftOrBottomLimit = 100;
                sbcd.ulRightOrTopLimit = 100;
                sbcd.hwndParentAndOwner = WinData.hwndMainClient;

                WinData.hwndSplitWindow = ctlCreateSplitWindow(hab,
                                                               &sbcd);
                if (!WinData.hwndSplitWindow)
                    cmnLog(__FILE__, __LINE__, __FUNCTION__,
                            "Cannot create split window.");
                else
                {
                    // link left and right container
                    WinSendMsg(WinData.hwndSplitWindow,
                               SPLM_SETLINKS,
                               (MPARAM)WinData.hwndDrivesFrame,
                               (MPARAM)WinData.hwndFilesFrame);

                    // show main dlg now
                    WinSetWindowPos(WinData.hwndMainFrame,
                                    HWND_TOP,
                                    100, 100, 600, 600,
                                    SWP_MOVE | SWP_SIZE | SWP_SHOW | SWP_ACTIVATE | SWP_ZORDER);

                    /*
                     *  FILL DRIVES TREE
                     *
                     */

                    // find the folder whose contents to
                    // display in the left tree
                    WinData.pDrivesFolder = _wpclsQueryFolder(_WPFolder,
                                                              "<WP_DRIVES>",
                                                              TRUE);
                    if (    (!WinData.pDrivesFolder)
                         || (!_somIsA(WinData.pDrivesFolder, _WPFolder))
                       )
                        cmnLog(__FILE__, __LINE__, __FUNCTION__,
                               "Cannot get drives folder.");
                    else
                    {
                        PLISTNODE pNode;
                        BOOL fExit = FALSE;
                        PMINIRECORDCORE pDrivesRec;
                        POINTL ptlIcon = {0, 0};

                        BuildDisksList(WinData.pDrivesFolder,
                                       &WinData.llDisks);

                        // insert the drives folder
                        pDrivesRec = _wpCnrInsertObject(WinData.pDrivesFolder,
                                                        WinData.hwndDrivesCnr,
                                                        &ptlIcon,
                                                        NULL,       // parent record
                                                        NULL);      // RECORDINSERT

                        // insert the folder contents into the
                        // drives tree on the left... this is
                        // relatively fast, so we do this
                        // synchronously
                        InsertFolderContents(WinData.hwndMainClient,
                                             WinData.hwndDrivesCnr,
                                             WinData.pDrivesFolder,
                                             pDrivesRec,   // parent record
                                             2,         // folders plus disks
                                             NULL,      // no file mask
                                             &WinData.llDriveObjectsInserted,
                                             NULL,
                                             &fExit);
                                        // note that we do not add children
                                        // to the disk objects yet... this
                                        // would attempt to get the root folder
                                        // for each of them

                        // expand "drives"
                        WinSendMsg(WinData.hwndDrivesCnr,
                                   CM_EXPANDTREE,
                                   (MPARAM)pDrivesRec,
                                   MPNULL);

                        // this has called wpCnrinsertObjects which
                        // subclasses the container owner, so
                        // subclass this with the XFolder subclass
                        // proc again; otherwise the new menu items
                        // won't work
                        WinData.psfvDrives
                            = fdrCreateSFV(WinData.hwndDrivesFrame,
                                           WinData.hwndDrivesCnr,
                                           QWL_USER,
                                           WinData.pDrivesFolder,
                                           WinData.pDrivesFolder);
                        WinData.psfvDrives->pfnwpOriginal
                            = WinSubclassWindow(WinData.hwndDrivesFrame,
                                                fnwpSubclassedDrivesFrame);

                        if (WinData.psfvDrives->pfnwpOriginal)
                        {
                            // OK, drives frame subclassed:

                            // create the "add children" thread
                            thrCreate(&WinData.tiAddChildren,
                                      fntAddChildren,
                                      &WinData.fAddChildrenRunning,
                                      "AddChildren",
                                      THRF_PMMSGQUEUE | THRF_WAIT_EXPLICIT,
                                                // "add child" posts event sem
                                                // when it has created the obj wnd
                                      (ULONG)&WinData);
                            // this will wait until the object window has been created

                            // expand the tree so that the current
                            // directory/file mask etc. is properly
                            // expanded
                            UpdateDlgWithFullFile(&WinData);
                                    // this will expand the tree and
                                    // select the current directory;
                                    // pllToExpand receives records to
                                    // add first children to

                            // start "add first child" thread which will
                            // insert sub-records for each item in the drives
                            // tree
                            /* if (!StartAddFirstChild(&WinData,
                                                    pllToExpand))
                                lstFree(pllToExpand); */

                            WinData.fFileDlgReady = TRUE;

                            WinSetFocus(HWND_DESKTOP, WinData.hwndFileEntry);

                            /*
                             *  PM MSG LOOP
                             *
                             */

                            // standard PM message loop... we stay in here
                            // (simulating a modal dialog) until WM_CLOSE
                            // comes in for the main client
                            while (WinGetMsg(hab, &qmsg, NULLHANDLE, 0, 0))
                            {
                                fExit = FALSE;
                                if (    (qmsg.hwnd == WinData.hwndMainClient)
                                     && (qmsg.msg == WM_CLOSE)
                                   )
                                {
                                    _Pmpf((__FUNCTION__ ": got WM_CLOSE"));
                                    fExit = TRUE;
                                }

                                WinDispatchMsg(hab, &qmsg);

                                if (fExit)
                                {
                                    if (pfd->lReturn == DID_OK)
                                    {
                                        sprintf(pfd->szFullFile,
                                                "%c:%s\\%s",
                                                'A' + WinData.ulLogicalDrive - 1,        // drive letter
                                                WinData.szDir,
                                                WinData.szFileName);
                                    }

                                    hwndReturn = (HWND)TRUE;
                                    break;
                                }
                            }
                        } // if (WinData.psfvDrives->pfnwpOriginal)

                        // stop threads
                        WinPostMsg(WinData.hwndAddChildren,
                                   WM_QUIT,
                                   0, 0);
                        WinData.tiAddChildren.fExit = TRUE;
                        WinData.tiInsertContents.fExit = TRUE;
                        DosSleep(0);
                        while (    (WinData.fAddChildrenRunning)
                                || (WinData.fInsertContentsRunning)
                              )
                        {
                            winhSleep(50);
                        }

                        /*
                         *  CLEANUP
                         *
                         */

                        // prevent dialog updates
                        WinData.fFileDlgReady = FALSE;
                        ClearContainer(WinData.hwndDrivesCnr,
                                       &WinData.llDriveObjectsInserted);
                        ClearContainer(WinData.hwndFilesCnr,
                                       &WinData.llFileObjectsInserted);

                    } // end else if (    (!WinData.pDrivesFolder)

                    if (WinData.pDrivesFolder)
                        _wpUnlockObject(WinData.pDrivesFolder);

                    // clean up
                    WinDestroyWindow(WinData.hwndSplitWindow);
                } // end else if (!WinData.hwndSplitWindow)

                WinDestroyWindow(WinData.hwndDrivesFrame);
                WinDestroyWindow(WinData.hwndFilesFrame);
                WinDestroyWindow(WinData.hwndMainFrame);
            }

            lstClear(&WinData.llDisks);

            lstClear(&WinData.llDialogControls);
        }
        CATCH(excpt1)
        {
        }
        END_CATCH();

        _Pmpf((__FUNCTION__ ": exiting, pfd->lReturn is %d", pfd->lReturn));
        _Pmpf(("  pfd->szFullFile is %s", pfd->szFullFile));
        _Pmpf(("  returning 0x%lX", hwndReturn));
    }

    return (hwndReturn);
}

