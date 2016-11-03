
/*
 *@@sourcefile xview.c:
 *      Xview implementation.
 *
 * This file's initial code was moved here from fdrsplit.c
 *
 *@@added V1.0.11 (2016-08-20) [rwalsh]
 *@@header "filesys\xview.h"
 */

/*
 *      Copyright (C) 2001-2016 Ulrich M”ller.
 *      Copyright (C) 2016 Rich Walsh.
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

#pragma strings(readonly)

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

#define INCL_DOSEXCEPTIONS
#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#define INCL_DOSERRORS

#define INCL_WINWINDOWMGR
#define INCL_WINMESSAGEMGR
#define INCL_WINFRAMEMGR
#define INCL_WINPOINTERS
#define INCL_WININPUT
#define INCL_WINMENUS
#define INCL_WINSTDCNR
#define INCL_WINSHELLDATA
#define INCL_WINSCROLLBARS
#define INCL_WINSYS
#define INCL_WINTIMER

#define INCL_GPIBITMAPS
#define INCL_GPIREGIONS
#define INCL_GPIPRIMITIVES
#include <os2.h>

#define XVIEW_SRC

// C library headers
#include <stdio.h>              // needed for except.h
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\comctl.h"             // common controls (window procs)
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\cnrh.h"               // container helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\gpih.h"               // GPI helper routines
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\standards.h"          // some standard macros
#include "helpers\threads.h"            // thread helpers
#include "helpers\winh.h"               // PM helper routines

// SOM headers which don't crash with prec. header files
#include "xfldr.ih"
#include "xfdisk.ih"
#include "xfobj.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\classtest.h"           // some cheap funcs for WPS class checks
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\contentmenus.h"        // shared menu logic
#include "shared\cnrsort.h"             // container sort comparison functions
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

#include "filesys\folder.h"             // XFolder implementation
#include "filesys\fdrmenus.h"           // shared folder menu logic
#include "filesys\fdrsubclass.h"        // folder subclassing engine
#include "filesys\fdrsplit.h"           // folder split views
#include "filesys\fdrviews.h"           // common code for folder views
#include "filesys\object.h"             // XFldObject implementation
#include "filesys\statbars.h"           // status bar translation logic
#include "filesys\xview.h"              // file viewer based on splitview

// other SOM headers
#pragma hdrstop                         // VAC++ keeps crashing otherwise
#include <wprootf.h>

/* ******************************************************************
 *
 *   Local macros
 *
 *@@added V1.0.11 (2016-08-12) [rwalsh]
 *
 ********************************************************************/

// internal window message
#define XVM_UPDATEWNDTITLE  (WM_USER + 1027)

// persistent and per-instance state flags
#define XVF_DFLT_ICON       0x0001
#define XVF_DFLT_DETAILS    0x0002
#define XVF_STATUSBAR       0x0004
#define XVF_BACKGROUND      0x0008

#define XVF_OPEN_ICON       0x0100
#define XVF_OPEN_DETAILS    0x0200
#define XVF_BKGD_OK         0x4000
#define XVF_INIT_OK         0x8000

#define XVF_OPEN_MASK       (XVF_OPEN_ICON | XVF_OPEN_DETAILS)
#define XVF_DFLT_MASK       (XVF_DFLT_ICON | XVF_DFLT_DETAILS)
#define XVF_SAVEMASK        (XVF_DFLT_ICON | XVF_DFLT_DETAILS | \
                             XVF_STATUSBAR | XVF_BACKGROUND)

// bitmap used to identify which details view columns are displayed
#define COL_ICON        (1 <<  0)       // 0x0001
#define COL_TITLE       (1 <<  1)       // 0x0002
    // the 2 columns between title & class are always hidden
#define COL_CLASS       (1 <<  4)       // 0x0010
#define COL_NAME        (1 <<  5)       // 0x0020
#define COL_SIZE        (1 <<  6)       // 0x0040
#define COL_WDATE       (1 <<  7)       // 0x0080
#define COL_WTIME       (1 <<  8)       // 0x0100
#define COL_ADATE       (1 <<  9)       // 0x0200
#define COL_ATIME       (1 << 10)       // 0x0400
#define COL_CDATE       (1 << 11)       // 0x0800
#define COL_CTIME       (1 << 12)       // 0x1000
#define COL_FLAGS       (1 << 13)       // 0x2000
#define COL_SPLIT       (1 << 31)       // 0x10000000

#define COL_MASK        (COL_ICON  | COL_TITLE | COL_CLASS | COL_NAME |  \
                         COL_SIZE  | COL_WDATE | COL_WTIME | COL_ADATE | \
                         COL_ATIME | COL_CDATE | COL_CTIME | COL_FLAGS | COL_SPLIT)

// the only CNRINFO.flWindowAttr flags permitted in Icon view
#define ICON_MASK       (CV_ICON | CV_NAME | CV_FLOW | CV_MINI | \
                         CA_MIXEDTARGETEMPH | CA_OWNERPAINTBACKGROUND)

// the ID for "Undo Arrange" on the View submenu appears to be undocumented
#define WPMENUID_UNDOARRANGE2 (WPMENUID_ARRANGE + 1)

// a helper macro enable/disable a menu item
#define winhEnableMenuItem(hwndMenu_, usId_, bEnable_) \
        WinSendMsg(hwndMenu_, MM_SETITEMATTR, MPFROM2SHORT(usId_, TRUE), \
                   MPFROM2SHORT(MIA_DISABLED, (((bEnable_)) ? 0 : MIA_DISABLED)))

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

PFNWP       G_pfnwpXviewFrameOrig = NULL;

// added V1.0.11 (2016-08-12) [rwalsh]: details view support
CNRINFO     G_CnrInfo = {0};
PFIELDINFO  G_pFieldInfo = (PFIELDINFO)-1;
LONG        G_clrFore = 0;
LONG        G_clrBack = 0;
PSZ         G_pszFont = 0;
XVIEWPOS    G_xvPos   = {0, 0, 600, 400, 30, 0, 0, 0, 0};


/********************************************************************/

void xvwSetWindowTitle(PXVIEWDATA pxvd, PMINIRECORDCORE prec);

/*
 *@@ xvwCreateXview:
 *      creates a frame window with a split window and
 *      does the usual "register view and pass a zillion
 *      QWL_USER pointers everywhere" stuff.
 *
 *      Returns the frame window or NULLHANDLE on errors.
 *
 *@@changed V1.0.9 (2011-10-08) [rwalsh]: subclass frame after registering view to preempt WPS subclass
 *@@changed V1.0.11 (2016-08-12) [rwalsh]: force menu bar and status bar
 *@@changed V1.0.11 (2016-08-12) [rwalsh]: accomodate expanded & legacy XVIEWPOS entries
 */

HWND xvwCreateXview(WPObject *pRootObject,
                    WPFolder *pRootsFolder,
                    ULONG ulView)
{
    HWND        hwndReturn = NULLHANDLE;

    WPFolder    *pFolder;

    TRY_LOUD(excpt1)
    {
        PXVIEWDATA  pxvd;
        ULONG       rc;

        // allocate our XVIEWDATA, which contains the
        // FDRSPLITVIEW for the split view engine, plus
        // the useitem for this view
        if (pxvd = (PXVIEWDATA)_wpAllocMem(pRootObject, sizeof(XVIEWDATA), &rc))
        {
            XVIEWPOS pos;
            ULONG   cbPos;
            ULONG   flFrame = FCF_NOBYTEALIGN
                                  | FCF_TITLEBAR
                                  | FCF_SYSMENU
                                  | FCF_SIZEBORDER
                                  | FCF_AUTOICON;
            ULONG   ulButton = _wpQueryButtonAppearance(pRootObject);
            ULONG   flSplit;

            ZERO(pxvd);

            // set flag to prevent Xview file-viewer enhancements
            // from messing up the splitview-based file dialog
            pxvd->sv.fIsViewer = TRUE;

            if (ulButton == DEFAULTBUTTON)
                ulButton = PrfQueryProfileInt(HINI_USER,
                                              "PM_ControlPanel",
                                              "MinButtonType",
                                              HIDEBUTTON);

            if (ulButton == HIDEBUTTON)
                flFrame |= FCF_HIDEMAX;     // hide and maximize
            else
                flFrame |= FCF_MINMAX;      // minimize and maximize

            pxvd->cb = sizeof(XVIEWDATA);

            // try to restore window position, if present;
            // we put these in a separate XWorkplace app
            // because we're using a special format to
            // allow for saving the split position
            sprintf(pxvd->szFolderPosKey,
                    "%lX",
                    _wpQueryHandle(pRootObject));

            // init the global default values - these will be used
            // later to avoid saving values that haven't changed
            if (G_xvPos.x == 0)
            {
                G_xvPos.x = (winhQueryScreenCX() - 600) / 2;
                G_xvPos.y = (winhQueryScreenCY() - 400) / 2;
            }

            cbPos = sizeof(pos);
            if (!PrfQueryProfileData(HINI_USER, (PSZ)INIAPP_FDRSPLITVIEWPOS,
                                     pxvd->szFolderPosKey, &pos, &cbPos))
                cbPos = 0;

            // accomodate both the expanded and legacy ini entries
            // changed V1.0.11 (2016-08-12) [rwalsh]
            if (cbPos == sizeof(XVIEWPOS))
            {
                pxvd->sv.flState       = pos.flState;
                pxvd->sv.flIconAttr    = pos.flIconAttr;
                pxvd->sv.flColumns     = pos.flColumns;
                pxvd->sv.lCnrSplitPos  = pos.lCnrSplitPos;
            }
            else
            if (cbPos != FIELDOFFSET(XVIEWPOS, flState))
                pos = G_xvPos;

            // set split styles
            // changed V1.0.11 (2016-08-12) [rwalsh]
            flSplit =   SPLIT_ANIMATE
                      | SPLIT_FDRSTYLES
                      | SPLIT_MULTIPLESEL
                      | SPLIT_MENUBAR
                      | SPLIT_STATUSBAR;

            // create the frame and the client;
            // the client gets pxvd in mp1 with WM_CREATE
            // and creates the split window and everything else
            if (fdrSplitCreateFrame(pRootObject,
                                    pRootsFolder,
                                    &pxvd->sv,
                                    flFrame,
                                    "",
                                    flSplit,
                                    NULL,       // no file mask
                                    pos.lSplitBarPos))
            {
                // view-specific stuff

                WinSetWindowPtr(pxvd->sv.hwndMainFrame, QWL_USER, pxvd);

                // register the view
                cmnRegisterView(pRootObject,
                                &pxvd->ui,
                                ulView,
                                pxvd->sv.hwndMainFrame,
                                cmnGetString(ID_XFSI_FDR_XVIEW));

                // subclass the frame _after_ registering the view
                // so our subclass proc sees messages before they
                // are routed to the root folder object
                // changed V1.0.9 (2011-10-08) [rwalsh]
                G_pfnwpXviewFrameOrig = WinSubclassWindow(pxvd->sv.hwndMainFrame,
                                                          fnwpXviewFrame);

                // change the window title to full path, if allowed
                if (cmnQuerySetting(sfFullPath))
                    xvwSetWindowTitle(pxvd, _wpQueryCoreRecord(pRootObject));

                // subclass the cnrs to let us paint the bitmaps
                fdrvMakeCnrPaint(pxvd->sv.hwndTreeCnr);
                fdrvMakeCnrPaint(pxvd->sv.hwndFilesCnr);

                // set tree view colors
                WinSendMsg(pxvd->sv.hwndMainFrame,
                           FM_SETCNRLAYOUT,
                           pxvd->sv.pRootsFolder,
                           0);

                // now go show the damn thing
                WinSetWindowPos(pxvd->sv.hwndMainFrame,
                                HWND_TOP,
                                pos.x,
                                pos.y,
                                pos.cx,
                                pos.cy,
                                SWP_MOVE | SWP_SIZE | SWP_SHOW | SWP_ACTIVATE | SWP_ZORDER);

                // and set focus to the tree view
                pxvd->sv.hwndFocusCnr = pxvd->sv.hwndTreeCnr;  // V1.0.9 (2012-02-12) [pr]
                WinSetFocus(HWND_DESKTOP, pxvd->sv.hwndFocusCnr);

                pxvd->sv.fSplitViewReady = TRUE;

                hwndReturn = pxvd->sv.hwndMainFrame;
            }
        }
    }
    CATCH(excpt1)
    {
    } END_CATCH();

    return hwndReturn;
}

/********************************************************************/

void xvwMenuInit(PXVIEWDATA pxvd, USHORT usID, HWND hwndMenu);
BOOL xvwWmMenuSelect(MPARAM mp1);
BOOL xvwWmCommand(PFDRSPLITVIEW psv, USHORT usCmd, HWND hwndFrame);

/*
 *@@ fnwpXviewFrame:
 *
 *@@added V1.0.0 (2002-08-21) [umoeller]
 *@@changed V1.0.5 (2006-04-13) [pr]: Fix status bar update @@fixes 326
 *@@changed V1.0.9 (2011-10-08) [rwalsh]: generate menus for selected folder, not root folder
 *@@changed V1.0.9 (2011-10-08) [rwalsh]: handle WM_COMMAND as though it came from selected folder
 *@@changed V1.0.11 (2016-08-12) [rwalsh]: do XV-specific processing for menu-related messages
 *@@changed V1.0.11 (2016-08-12) [rwalsh]: prevent crashes when an unmounted disk is selected
 *@@changed V1.0.11 (2016-08-12) [rwalsh]: save XV view options when the window closes
 */

MRESULT EXPENTRY fnwpXviewFrame(HWND hwndFrame, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;
    PXVIEWDATA pxvd;

    switch (msg)
    {

        /*
         * WM_QUERYFRAMECTLCOUNT:
         *      this gets sent to the frame when PM wants to find out
         *      how many frame controls the frame has. According to what
         *      we return here, SWP structures are allocated for WM_FORMATFRAME.
         *      We call the "parent" window proc and add one for the status bar.
         */

        case WM_QUERYFRAMECTLCOUNT:
        {
            // query the standard frame controls count
            ULONG ulCount = (ULONG)G_pfnwpXviewFrameOrig(hwndFrame, msg, mp1, mp2);

            if ((pxvd = (PXVIEWDATA)WinQueryWindowPtr(hwndFrame, QWL_USER))
                 && (pxvd->sv.hwndStatusBar)
                 && (pxvd->sv.flState & XVF_STATUSBAR))
                ulCount++;

            mrc = (MRESULT)ulCount;
        }
        break;

        /*
         * WM_FORMATFRAME:
         *    this message is sent to a frame window to calculate the sizes
         *    and positions of all of the frame controls and the client window.
         *
         *    Parameters:
         *          mp1     PSWP    pswp        structure array; for each frame
         *                                      control which was announced with
         *                                      WM_QUERYFRAMECTLCOUNT, PM has
         *                                      allocated one SWP structure.
         *          mp2     PRECTL  pprectl     pointer to client window
         *                                      rectangle of the frame
         *          returns USHORT  ccount      count of the number of SWP
         *                                      arrays returned
         *
         *    It is the responsibility of this message code to set up the
         *    SWP structures in the given array to position the frame controls.
         *    We call the "parent" window proc and then set up our status bar.
         */

        case WM_FORMATFRAME:
        {
            //  query the number of standard frame controls
            ULONG ulCount = (ULONG)G_pfnwpXviewFrameOrig(hwndFrame, msg, mp1, mp2);

            if ((pxvd = (PXVIEWDATA)WinQueryWindowPtr(hwndFrame, QWL_USER))
                 && (pxvd->sv.hwndStatusBar)
                 && (pxvd->sv.flState & XVF_STATUSBAR))
            {
                // we have a status bar:
                // format the frame
                fdrFormatFrame(hwndFrame,
                               pxvd->sv.hwndStatusBar,
                               mp1,
                               ulCount,
                               NULL);

                // increment the number of frame controls
                // to include our status bar
                ulCount++;
            }

            mrc = (MRESULT)ulCount;
        }
        break;

        /*
         * WM_CALCFRAMERECT:
         *     this message occurs when an application uses the
         *     WinCalcFrameRect function.
         *
         *     Parameters:
         *          mp1     PRECTL  pRect      rectangle structure
         *          mp2     USHORT  usFrame    frame indicator
         *          returns BOOL    rc         rectangle-calculated indicator
         */

        case WM_CALCFRAMERECT:
            mrc = G_pfnwpXviewFrameOrig(hwndFrame, msg, mp1, mp2);

            if ((pxvd = (PXVIEWDATA)WinQueryWindowPtr(hwndFrame, QWL_USER))
                 && (pxvd->sv.hwndStatusBar)
                 && (pxvd->sv.flState & XVF_STATUSBAR))
            {
                winhCalcExtFrameRect(mp1,
                                     mp2,
                                     cmnQueryStatusBarHeight());
            }
        break;

        /*
         * WM_INITMENU:
         *      fill the menu bar pulldowns.
         */

        case WM_INITMENU:
            mrc = G_pfnwpXviewFrameOrig(hwndFrame, msg, mp1, mp2);

            if (pxvd = (PXVIEWDATA)WinQueryWindowPtr(hwndFrame, QWL_USER))
            {
                ULONG ulMenuType = 0;

                // init menu doesn't recognize our new menu
                // bars, so check for these explicitly

                switch (SHORT1FROMMP(mp1))
                {
                    case ID_XFM_BAR_FOLDER:
                        PMPF_POPULATESPLITVIEW(("ID_XFM_BAR_FOLDER, (HWND)mp2 is 0x%lX", mp2));
                        ulMenuType = MENU_FOLDERPULLDOWN;

                        // this operates on the root folder
                        pxvd->psfvBar = pxvd->sv.psfvTree;
                    break;

                    case ID_XFM_BAR_EDIT:
                        PMPF_POPULATESPLITVIEW(("ID_XFM_BAR_EDIT, (HWND)mp2 is 0x%lX", mp2));
                        ulMenuType = MENU_EDITPULLDOWN;

                        // this operates on the folder whose
                        // contents are showing on the right
                        pxvd->psfvBar = pxvd->sv.psfvFiles;
                    break;

                    case ID_XFM_BAR_VIEW:
                        PMPF_POPULATESPLITVIEW(("ID_XFM_BAR_VIEW, (HWND)mp2 is 0x%lX", mp2));
                        ulMenuType = MENU_VIEWPULLDOWN;

                        // this operates on the folder whose
                        // contents are showing on the right
                        pxvd->psfvBar = pxvd->sv.psfvFiles;
                    break;

                    case ID_XFM_BAR_SELECTED:
                        PMPF_POPULATESPLITVIEW(("ID_XFM_BAR_SELECTED, (HWND)mp2 is 0x%lX", mp2));
                        ulMenuType = MENU_SELECTEDPULLDOWN;

                        // this operates on the folder whose
                        // contents are showing on the right
                        pxvd->psfvBar = pxvd->sv.psfvFiles;
                    break;

                    case ID_XFM_BAR_HELP:
                        PMPF_POPULATESPLITVIEW(("ID_XFM_BAR_HELP, (HWND)mp2 is 0x%lX", mp2));
                        ulMenuType = MENU_HELPPULLDOWN;

                        // this operates on the folder whose
                        // contents are showing on the right
                        pxvd->psfvBar = pxvd->sv.psfvFiles;
                    break;
                }

                if (ulMenuType)
                {
                    HWND        hwndMenu;

                    // prevent a crash if an unmounted disk is selected
                    // in the tree frame; since it has no root folder,
                    // somSelf is null; for the Help menu, point at the
                    // tree frame so the user still has access to help;
                    // for the other menus, abort
                    // added V1.0.11 (2016-08-12) [rwalsh]: 
                    if (!pxvd->psfvBar->somSelf)
                    {
                        if (ulMenuType == MENU_HELPPULLDOWN &&
                            pxvd->sv.psfvTree->somSelf)
                            pxvd->psfvBar = pxvd->sv.psfvTree;
                        else
                        {
                            winhClearMenu((HWND)mp2);
                            break;
                        }
                    }

                    // changed V1.0.9 (2011-10-08) [rwalsh]:  generate a
                    // menu for the selected folder, not for the root folder
                    if (hwndMenu = _wpDisplayMenu(pxvd->psfvBar->somSelf,
                                                  pxvd->psfvBar->hwndFrame,
                                                  pxvd->psfvBar->hwndCnr,
                                                  NULL,
                                                  ulMenuType | MENU_NODISPLAY,
                                                  0))
                    {
                        // empty the old pulldown
                        winhClearMenu((HWND)mp2);

                        winhMergeMenus((HWND)mp2,
                                       MIT_END,
                                       hwndMenu,
                                       0);

                        // call our own routine to hack in new items
                        // changed V1.0.9 (2011-10-08) [rwalsh]:  select
                        // source object using the selected container
                        fdrManipulatePulldown(pxvd->psfvBar,
                                              (ULONG)mp1,
                                              (HWND)mp2);

                        // since we used MENU_NODISPLAY, apparently
                        // we have to delete the menu ourselves,
                        // or we end up with thousands of menus
                        // under HWND_DESKTOP
                        WinDestroyWindow(hwndMenu);
                    }

                    PMPF_POPULATESPLITVIEW(("    _wpDisplayMenu returned 0x%lX", hwndMenu));
                }

                // rare case, but if the user clicked on the title
                // bar then pxvd->psfvBar is not set; to be safe,
                // set it to the root folder (tree view)
                if (!pxvd->psfvBar)
                    pxvd->psfvBar = pxvd->sv.psfvTree;

                // call init menu in fdrsubclass.c for sounds
                // and content menu items support
                fdrInitMenu(pxvd->psfvBar,
                            (ULONG)mp1,
                            (HWND)mp2);

                // make final Xview-specific modifications
                // added V1.0.11 (2016-08-12) [rwalsh]
                xvwMenuInit(pxvd, SHORT1FROMMP(mp1), (HWND)mp2);
            }
        break;

        /*
         * WM_MEASUREITEM:
         *      same processing as in fdrProcessFolderMsgs.
         */

        case WM_MEASUREITEM:
            // set the size of the blank separator used in the Options
            // submenu and save the size of the current character box

            if (((POWNERITEM)mp2)->idItem == ID_XFMI_OFS_BLANKSEPARATOR + *G_pulVarMenuOfs
                && (pxvd = (PXVIEWDATA)WinQueryWindowPtr(hwndFrame, QWL_USER)))
            {
                POWNERITEM poi = (POWNERITEM)mp2;
                LONG cy = WinQuerySysValue(HWND_DESKTOP, SV_CYMENU) / 3;

                GpiQueryCharBox(poi->hps, &pxvd->szfCharBox);
                poi->rclItem.xRight = cy;
                poi->rclItem.yTop = cy;
                mrc = (MRESULT)cy;
                break;
            }

            if ((SHORT)mp1 > cmnQuerySetting(sulVarMenuOfs) + ID_XFMI_OFS_VARIABLE)
            {
                // call the measure-item func in fdrmenus.c
                mrc = cmnuMeasureItem((POWNERITEM)mp2);
            }
            else
                // none of our items: pass to original wnd proc
                mrc = G_pfnwpXviewFrameOrig(hwndFrame, msg, mp1, mp2);
        break;

        /*
         * WM_DRAWITEM:
         *      same processing as in fdrProcessFolderMsgs,
         *      except that we do not need cnr owner draw
         *      for FID_CLIENT.
         */

        case WM_DRAWITEM:
            // for the blank separator used in the Options submenu,
            // just reset the size of the current character box -
            // there's no need to draw anything

            if (((POWNERITEM)mp2)->idItem == ID_XFMI_OFS_BLANKSEPARATOR + *G_pulVarMenuOfs
                && (pxvd = (PXVIEWDATA)WinQueryWindowPtr(hwndFrame, QWL_USER)))
            {
                GpiSetCharBox(((POWNERITEM)mp2)->hps, &pxvd->szfCharBox);
                mrc = (MRESULT)TRUE;
                break;
            }

            if ((SHORT)mp1 > cmnQuerySetting(sulVarMenuOfs) + ID_XFMI_OFS_VARIABLE)
            {
                // variable menu item: this must be a folder-content
                // menu item, because for others no WM_DRAWITEM is sent
                if (cmnuDrawItem(mp1, mp2))
                    mrc = (MRESULT)TRUE;
                else // error occurred:
                    mrc = G_pfnwpXviewFrameOrig(hwndFrame, msg, mp1, mp2);
            }
            else
                mrc = G_pfnwpXviewFrameOrig(hwndFrame, msg, mp1, mp2);
        break;

        /*
         *@@WM_COMMAND and WM_MENUSELECT:
         *
         *      handle these messages as though they had come
         *      from the file or tree frames (as selected above
         *      in WM_INITMENU) so they get routed to the correct
         *      folder or object.  If they are forwarded to this
         *      frame's original wndproc, they'll all be routed
         *      to this view's root folder.
         *
         *      changed V1.0.9 (2011-10-08) [rwalsh]
         *      changed V1.0.11 (2016-08-12) [rwalsh]
         */

        case WM_COMMAND:
        case WM_MENUSELECT:
            if (pxvd = (PXVIEWDATA)WinQueryWindowPtr(hwndFrame, QWL_USER))
            {
                // handle View submenu options locally to ensure
                // they only affect our container's view and not
                // the object's default settings
                // added V1.0.11 (2016-08-12) [rwalsh]

                if (msg == WM_MENUSELECT)
                    mrc = (MRESULT)xvwWmMenuSelect(mp1);
                else
                    mrc = (MRESULT)xvwWmCommand(&pxvd->sv,
                                                       SHORT1FROMMP(mp1),
                                                       hwndFrame);
                if (mrc)
                    break;

                if (pxvd->psfvBar)
                    mrc = fdrProcessFolderMsgs(pxvd->psfvBar->hwndFrame,
                                               msg,
                                               mp1,
                                               mp2,
                                               pxvd->psfvBar,
                                               pxvd->psfvBar->pfnwpOriginal);
                else
                    mrc = G_pfnwpXviewFrameOrig(hwndFrame, msg, mp1, mp2);
            }
        break;

        /*
         *@@ WM_CONTROL:
         *      intercept SN_FRAMECLOSE.
         *      V1.0.0 (2002-09-13) [umoeller]
         */

        case WM_CONTROL:
            if ((SHORT1FROMMP(mp1) == FID_CLIENT)
                 && (pxvd = (PXVIEWDATA)WinQueryWindowPtr(hwndFrame, QWL_USER)))
            {
                switch (SHORT2FROMMP(mp1))
                {
                    case SN_FOLDERCHANGING:
                        // change the window title asynchronously when
                        // the selected folder in the tree cnr changes
                        WinPostMsg(hwndFrame, XVM_UPDATEWNDTITLE, mp2, 0);
                        break;

                    case SN_VKEY:
                        switch ((USHORT)mp2)
                        {
                            case VK_TAB:
                            case VK_BACKTAB:
                            {
                                HWND hwndFocus = WinQueryFocus(HWND_DESKTOP);
                                if (hwndFocus == pxvd->sv.hwndTreeCnr)
                                    hwndFocus = pxvd->sv.hwndFilesCnr;
                                else
                                    hwndFocus = pxvd->sv.hwndTreeCnr;

                                pxvd->sv.hwndFocusCnr = hwndFocus;  // V1.0.9 (2012-02-12) [pr]
                                WinSetFocus(HWND_DESKTOP, hwndFocus);
                                // V1.0.5 (2006-04-13) [pr]: Fix status bar update @@fixes 326
                                if (pxvd->sv.hwndStatusBar)
                                    WinPostMsg(pxvd->sv.hwndStatusBar,
                                               STBM_UPDATESTATUSBAR,
                                               (MPARAM) hwndFocus,
                                               MPNULL);

                                mrc = (MRESULT)TRUE;
                            }
                            break;
                        }
                    break;

                    case SN_FRAMECLOSE:
                    {
                        SWP swp;
                        XVIEWPOS pos;

                        PMPF_POPULATESPLITVIEW(("WM_CONTROL + SN_FRAMECLOSE"));

                        // save window position
                        WinQueryWindowPos(hwndFrame, &swp);
                        pos.x = swp.x;
                        pos.y = swp.y;
                        pos.cx = swp.cx;
                        pos.cy = swp.cy;
                        pos.lSplitBarPos = ctlQuerySplitPos(pxvd->sv.hwndSplitWindow);

                        // only save these values if they've been explicitly changed;
                        // this lets the folder adapt to changes in the system defaults
                        // added v1.0.11 (2016-08-12) [rwalsh]

                        pos.flState = pxvd->sv.flState & XVF_SAVEMASK;
                        if (pos.flState == cmnQuerySetting(sXviewDefaultState))
                            pos.flState = 0;

                        pos.flIconAttr = pxvd->sv.flIconAttr & ICON_MASK;
                        if (pos.flIconAttr == cmnQuerySetting(sXviewIconAttrib))
                            pos.flIconAttr = 0;

                        pos.flColumns = pxvd->sv.flColumns & COL_MASK;
                        if (pos.flColumns == cmnQuerySetting(sXviewDetailsCols))
                            pos.flColumns = 0;

                        // our cnr's stored splitbar position isn't updated when
                        // the user changes it, so get it now if it's visible
                        if (pxvd->sv.flColumns & COL_SPLIT)
                        {
                            CNRINFO CnrInfo;
                            if (WinSendMsg(pxvd->sv.hwndFilesCnr, CM_QUERYCNRINFO,
                                           (MPARAM)&CnrInfo, (MPARAM)sizeof(CNRINFO)))
                                pxvd->sv.lCnrSplitPos = CnrInfo.xVertSplitbar;
                        }
                        pos.lCnrSplitPos =
                            (pxvd->sv.lCnrSplitPos == G_CnrInfo.xVertSplitbar)
                            ? 0 : pxvd->sv.lCnrSplitPos;

                        // don't save entries that have all default values
                        // added v1.0.11 (2016-08-12) [rwalsh]
                        if (memcmp(&pos, &G_xvPos, sizeof(XVIEWPOS)))
                            PrfWriteProfileData(HINI_USER,
                                                (PSZ)INIAPP_FDRSPLITVIEWPOS,
                                                pxvd->szFolderPosKey,
                                                &pos,
                                                sizeof(pos));

                        // clear all containers, stop populate thread etc.;
                        // this destroys the frame, which in turn destroys
                        // us and frees psv
                        fdrSplitDestroyFrame(&pxvd->sv);
                    }
                }
            }
        break;

        /*
         * FM_SETCNRLAYOUT:
         *      this message gets posted (!) when the folder
         *      view needs to have its container background
         *      set. This gets posted from two locations:
         *
         *      --  fnwpSplitController when FM_FILLFOLDER
         *          comes in to set the background before
         *          we start populate;
         *
         *      --  our XFolder::wpRedrawFolderBackground
         *          override when folder background settings
         *          change.
         *
         *      Note: Both fnwpFilesFrame and fnwpSplitViewFrame
         *      react to this message, depending on which
         *      backgrounds needs to be updated.
         *
         *      Parameters:
         *
         *      --  WPFolder *mp1: folder that the container
         *          represents. This must be the root folder
         *          if we're displaying a disk object.
         *
         *      No return code.
         *
         *added V1.0.0 (2002-09-24) [umoeller]
         */

        case FM_SETCNRLAYOUT:
            PMPF_POPULATESPLITVIEW(("FM_SETCNRLAYOUT"));
            if ((pxvd = (PXVIEWDATA)WinQueryWindowPtr(hwndFrame, QWL_USER))
                 && (pxvd->sv.flSplit & SPLIT_FDRSTYLES)
                 && (mp1))
            {
                fdrvSetCnrLayout(pxvd->sv.hwndTreeCnr,
                                 (WPFolder*)mp1,
                                 OPEN_TREE);
            }
        break;

        case XVM_UPDATEWNDTITLE:
            if (mp1
                && cmnQuerySetting(sfFullPath)
                && (pxvd = (PXVIEWDATA)WinQueryWindowPtr(hwndFrame, QWL_USER)))
                xvwSetWindowTitle(pxvd, (PMINIRECORDCORE)mp1);
            break;

        /*
         * WM_DESTROY:
         *
         */

        case WM_DESTROY:
            if (pxvd = (PXVIEWDATA)WinQueryWindowPtr(hwndFrame, QWL_USER))
            {
                _wpDeleteFromObjUseList(pxvd->sv.pRootObject,
                                        &pxvd->ui);

                if (pxvd->sv.hwndStatusBar)
                    WinDestroyWindow(pxvd->sv.hwndStatusBar);

                _wpFreeMem(pxvd->sv.pRootObject,
                           (PBYTE)pxvd);
            }

            mrc = G_pfnwpXviewFrameOrig(hwndFrame, msg, mp1, mp2);
        break;

        default:
            mrc = G_pfnwpXviewFrameOrig(hwndFrame, msg, mp1, mp2);
    }

    return mrc;
}

/********************************************************************/

 /*
 *@@ xvwSetWindowTitle:
 *
 * This is similar to fdrSetOneFrameWndTitle() but handles both
 * folders and disks. Its configuration is controlled by global
 * settings only.
 *
 *@@added V1.0.11 (2016-09-29) [rwalsh]
 */

void xvwSetWindowTitle(PXVIEWDATA pxvd, PMINIRECORDCORE prec)
{
    ULONG         ulMax;
    WPFileSystem* pFolder;
    char *        pText;
    char *        pPath;
    char *        ptr;
    char          szText[2 * CCHMAXPATH];
    char          szPath[CCHMAXPATH];

do {

    // get the root object's title, maybe
    if (cmnQuerySetting(sfKeepTitle)) {
        if (!(ptr = _wpQueryTitle(pxvd->sv.pRootObject)))
            return;
        strcpy(szText, ptr);
        strcat(szText, " - ");
        pText = strchr(szText, '\0');
    } else {
        pText = szText;
    }

    // this will fail for unmounted drives;
    // in that case, just display the selcted object's title
    if (!(pFolder = fdrvGetFSFromRecord(prec, TRUE))) {
        if (!(ptr = _wpQueryTitle(OBJECT_FROM_PREC(prec))))
            ptr = "???";
        strcpy(pText, ptr);
        break;
    }

    // find the first slash - it will be null if this is a
    // drive's root directory, so add a slash, then leave
    _wpQueryFilename(pFolder, szPath, TRUE);
    if (!(pPath = strchr(szPath, '\\'))) {
        strcat(szPath, "\\");
        strcpy(pText, szPath);
        break;
    }
    pPath++;

    ulMax = cmnQuerySetting(sulMaxPathChars);

    // leave if the path is within the max, or there's
    // nothing to elide - the folder's name won't be
    // truncated regardless of length
    if (   strlen(pPath) <= ulMax
        || !(ptr = strrchr(pPath, '\\'))
        || ptr - pPath <= 3) {
        strcpy(pText, szPath);
        break;
    }

    // copy in the root directory & elipsis, then
    // advance to where the elided path will start
    memcpy(pText, szPath, (pPath - szPath));
    pText += pPath - szPath;
    memcpy(pText, "...", 3);
    pText += 3;

    // find a short enough substring - if there
    // isn't one, it's because the target's name
    // is too long so we'll use it as-is
    ptr = strchr(pPath, '\\');
    while (strlen(ptr) > ulMax) {
        char * ptr2 = strchr(ptr+1, '\\');
        if (!ptr2)
            break;
        ptr = ptr2;
    }
    strcpy(pText, ptr);

} while (0);

    WinSetWindowText(pxvd->sv.hwndMainFrame, szText);
}

/********************************************************************/

 /*
 *@@ xvwMenuInit:
 *
 *  This is a grab-bag of Xview-specific addtions and tweaks to
 *  the menubar's puldown menus. It runs after XWP and the WPS have
 *  performed their initializations.
 *
 *  Note: this reimplements the addition of icon view-specific menu
 *  items such as "Small icons" that would otherwise be done in
 *  fdrmenus.c. Using that code to add them and handle the messages
 *  they produce proved clumsy and required multiple hacks. Doing
 *  it here where all relevant view info is readily available is
 *  quite easy.
 *
 *@@added V1.0.11 (2016-08-12) [rwalsh]
 */

void xvwMenuInit(PXVIEWDATA pxvd, USHORT usID, HWND hwndMenu)
{
    if (usID == ID_XFM_BAR_FOLDER)
    {
        // disable "Create another" if the root object is a disk's
        // root folder (i.e. don't go creating phantom drives)
        if (pxvd->psfvBar->somSelf != pxvd->psfvBar->pRealObject)
            winhEnableMenuItem(hwndMenu, WPMENUID_CREATEANOTHER, FALSE);
        return;
    }

    if (usID == ID_XFM_BAR_EDIT)
    {
        // disable "Create another" if the selected object is a disk
        if (pxvd->psfvBar->pSourceObject &&
            _somIsA(pxvd->psfvBar->pSourceObject, _WPDisk))
                winhEnableMenuItem(hwndMenu, WPMENUID_CREATEANOTHER, FALSE);
        return;
    }

    if (usID == ID_XFM_BAR_VIEW)
    {
        HWND        hwndSubMenu;
        USHORT      id;
        ULONG       ulAttr;
        ULONG       flag;
        PFIELDINFO  pfi;
        MENUITEM    mi;

        // "Tree" is irrelevant, "Arrange" doesn't work, and
        // "Undo arrange" is always disabled, so delete them
        winhDeleteMenuItem(hwndMenu, WPMENUID_CHANGETOTREE);
        winhDeleteMenuItem(hwndMenu, WPMENUID_ARRANGE);
        winhDeleteMenuItem(hwndMenu, WPMENUID_UNDOARRANGE2);

        // enable to appropriate switch-to view item
        if (pxvd->sv.flState & XVF_OPEN_ICON)
        {
            if (pxvd->sv.flState & XVF_INIT_OK)
                winhEnableMenuItem(hwndMenu, WPMENUID_CHANGETODETAILS, TRUE);
        }
        else
            winhEnableMenuItem(hwndMenu, WPMENUID_CHANGETOICON, TRUE);

        // separator
        winhInsertMenuSeparator(hwndMenu, MIT_END,
                                *G_pulVarMenuOfs + ID_XFMI_OFS_SEPARATOR);

        // create and add an "Options" submenu
        if (!(hwndSubMenu = WinCreateMenu(hwndMenu, NULL)))
            return;
    
        mi.iPosition = MIT_END;
        mi.afStyle = MIS_SUBMENU | MIS_TEXT;
        mi.afAttribute = 0;
        mi.id = ID_XFM_OFS_OPTIONS + *G_pulVarMenuOfs;
        mi.hwndSubMenu = hwndSubMenu;
        mi.hItem = 0;

        WinSetWindowUShort(hwndSubMenu, QWS_ID, mi.id);
        id = SHORT1FROMMR(WinSendMsg(hwndMenu, MM_INSERTITEM, (MPARAM)&mi,
                                     (MPARAM)cmnGetString(ID_XFSI_OPTIONS)));

        // exit if the menu can't be added
        if (id == MIT_MEMERROR || id == MIT_ERROR)
            return;

        // set up to insert blank separators below
        mi.iPosition = MIT_END;
        mi.afStyle = MIS_OWNERDRAW | MIS_TEXT | MIS_STATIC;
        mi.afAttribute = 0;
        mi.id = *G_pulVarMenuOfs + ID_XFMI_OFS_BLANKSEPARATOR;
        mi.hwndSubMenu = 0;
        mi.hItem = 0;

        // only display details view column options if
        // details init suceeded - otherwise, we may crash
        // when trying to access text using G_pFieldInfo
        if (pxvd->sv.flState & XVF_INIT_OK)
        {
            // details view column header
            winhInsertMenuItem(hwndSubMenu, MIT_END,
                               *G_pulVarMenuOfs + ID_XFMI_OFS_SEPARATOR,
                               cmnGetString(ID_XFSI_DETAILSHDR),
                               MIS_TEXT | MIS_STATIC, 0);
           
            // separator
            winhInsertMenuSeparator(hwndSubMenu, MIT_END,
                                    *G_pulVarMenuOfs + ID_XFMI_OFS_SEPARATOR);
           
            // add entries to display/hide details columns;
            // the ptrs to the items' text is taken from the global
            // FIELDINFO array captured at startup; they are constant
            // ptrs to constant text so they're always valid; the
            // text is already NLS-appropriate and needs no translation
           
            id = *G_pulVarMenuOfs + ID_XFMI_OFS_ICON_COLUMN;
            for (flag = 1, pfi = G_pFieldInfo; flag <= COL_FLAGS; flag <<= 1, pfi++)
            {
                if (flag & COL_MASK)
                {
                    winhInsertMenuItem(hwndSubMenu, MIT_END, id,
                                       pfi->pTitleData,
                                       MIS_TEXT, MIA_NODISMISS |
                                       ((pxvd->sv.flColumns & flag) ? MIA_CHECKED : 0));
                    id++;
           
                    // if this is the "Object class" column,
                    // insert the "= split bar =" item after it
                    if (flag == COL_CLASS)
                        winhInsertMenuItem(hwndSubMenu, MIT_END,
                                           *G_pulVarMenuOfs + ID_XFMI_OFS_CNRSPLITBAR,
                                           cmnGetString(ID_XFSI_CNRSPLITBAR),
                                           MIS_TEXT, MIA_NODISMISS |
                                           ((pxvd->sv.flColumns & COL_SPLIT) ? MIA_CHECKED : 0));
                }
            }
           
            // don't let the "Title" column be hidden
            winhEnableMenuItem(hwndSubMenu, *G_pulVarMenuOfs + ID_XFMI_OFS_TITLE_COLUMN, FALSE);

            // force subsequent menu items to a new column
            ulAttr = MIS_BREAKSEPARATOR;
        }
        else
        {
            // if no details column, no MIS_BREAKSEPARATOR
            ulAttr = 0;
        }

        // icon view header - start new column if appropriate
        winhInsertMenuItem(hwndSubMenu, MIT_END,
                           *G_pulVarMenuOfs + ID_XFMI_OFS_SEPARATOR,
                           cmnGetString(ID_XFSI_ICONHDR),
                           MIS_TEXT | MIS_STATIC | ulAttr, 0);

        // separator
        winhInsertMenuSeparator(hwndSubMenu, MIT_END,
                                *G_pulVarMenuOfs + ID_XFMI_OFS_SEPARATOR);

        // flags used by the next 3 items
        ulAttr = pxvd->sv.flIconAttr & (CV_ICON | CV_NAME | CV_FLOW);

        // "as placed"
        winhInsertMenuItem(hwndSubMenu, MIT_END,
                           *G_pulVarMenuOfs + ID_XFMI_OFS_NOGRID,
                           cmnGetString(ID_XFSI_NOGRID),
                           MIS_TEXT, MIA_NODISMISS |
                           (ulAttr == CV_ICON ? MIA_CHECKED : 0));

        // "multiple columns"
        winhInsertMenuItem(hwndSubMenu, MIT_END,
                           *G_pulVarMenuOfs + ID_XFMI_OFS_FLOWED,
                           cmnGetString(ID_XFSI_FLOWED),
                           MIS_TEXT, MIA_NODISMISS |
                           (ulAttr == (CV_NAME | CV_FLOW) ? MIA_CHECKED : 0));

        // "single column"
        winhInsertMenuItem(hwndSubMenu, MIT_END,
                           *G_pulVarMenuOfs + ID_XFMI_OFS_NONFLOWED,
                           cmnGetString(ID_XFSI_NONFLOWED),
                           MIS_TEXT, MIA_NODISMISS |
                           (ulAttr == CV_NAME ? MIA_CHECKED : 0));

        // blank separator
        WinSendMsg(hwndSubMenu, MM_INSERTITEM, (MPARAM)&mi, "");

        // "small icons"
        winhInsertMenuItem(hwndSubMenu, MIT_END,
                           *G_pulVarMenuOfs + ID_XFMI_OFS_SMALLICONS,
                           cmnGetString(ID_XFSI_SMALLICONS),
                           MIS_TEXT, MIA_NODISMISS |
                           ((pxvd->sv.flIconAttr & CV_MINI) ? MIA_CHECKED : 0));

        // blank separator
        WinSendMsg(hwndSubMenu, MM_INSERTITEM, (MPARAM)&mi, "");

        // separator
        winhInsertMenuSeparator(hwndSubMenu, MIT_END,
                                *G_pulVarMenuOfs + ID_XFMI_OFS_SEPARATOR);

        // "This folder" header
        winhInsertMenuItem(hwndSubMenu, MIT_END,
                           *G_pulVarMenuOfs + ID_XFMI_OFS_SEPARATOR,
                           cmnGetString(ID_XFSI_THISFOLDER),
                           MIS_TEXT | MIS_STATIC, 0);

        // separator
        winhInsertMenuSeparator(hwndSubMenu, MIT_END,
                                *G_pulVarMenuOfs + ID_XFMI_OFS_SEPARATOR);

        // "icon view"
        winhInsertMenuItem(hwndSubMenu, MIT_END,
                           *G_pulVarMenuOfs + ID_XFMI_OFS_DEFAULTICON,
                           cmnGetString(ID_XSDI_MENU_ICONVIEW),
                           MIS_TEXT, MIA_NODISMISS |
                           ((pxvd->sv.flState & XVF_DFLT_ICON) ? MIA_CHECKED : 0));

        // "details view"
        winhInsertMenuItem(hwndSubMenu, MIT_END,
                           *G_pulVarMenuOfs + ID_XFMI_OFS_DEFAULTDETAILS,
                           cmnGetString(ID_XSDI_MENU_DETAILSVIEW),
                           MIS_TEXT, MIA_NODISMISS |
                           ((pxvd->sv.flState & XVF_DFLT_DETAILS) ? MIA_CHECKED : 0));

        // blank separator
        WinSendMsg(hwndSubMenu, MM_INSERTITEM, (MPARAM)&mi, "");

        // "backgrounds"
        winhInsertMenuItem(hwndSubMenu, MIT_END,
                           *G_pulVarMenuOfs + ID_XFMI_OFS_CNRBACKGROUNDS,
                           cmnGetString(ID_XFSI_CNRBACKGROUNDS),
                           MIS_TEXT, MIA_NODISMISS |
                           ((pxvd->sv.flState & XVF_BACKGROUND) ? MIA_CHECKED : 0));

        // "status bar"
        ulAttr = pxvd->sv.hwndStatusBar
                 ? ((pxvd->sv.flState & XVF_STATUSBAR) ? MIA_CHECKED : 0)
                 : MIA_DISABLED;
        winhInsertMenuItem(hwndSubMenu, MIT_END,
                           *G_pulVarMenuOfs + ID_XFMI_OFS_SHOWSTATUSBAR,
                           cmnGetString(ID_XFSI_SHOWSTATUSBAR),
                           MIS_TEXT, MIA_NODISMISS | ulAttr);

        // blank separator
        WinSendMsg(hwndSubMenu, MM_INSERTITEM, (MPARAM)&mi, "");

        // separator
        winhInsertMenuSeparator(hwndSubMenu, MIT_END,
                                *G_pulVarMenuOfs + ID_XFMI_OFS_SEPARATOR);

        // "All folders" header
        winhInsertMenuItem(hwndSubMenu, MIT_END,
                           *G_pulVarMenuOfs + ID_XFMI_OFS_SEPARATOR,
                           cmnGetString(ID_XFSI_ALLFOLDERS),
                           MIS_TEXT | MIS_STATIC, 0);

        // separator
        winhInsertMenuSeparator(hwndSubMenu, MIT_END,
                                *G_pulVarMenuOfs + ID_XFMI_OFS_SEPARATOR);

        // "Use these settings"
        winhInsertMenuItem(hwndSubMenu, MIT_END,
                           *G_pulVarMenuOfs + ID_XFMI_OFS_SETSYSDEFAULT,
                           cmnGetString(ID_XFSI_USETHESESETTINGS),
                           MIS_TEXT, 0);

        return;
    }
}

/*
 *@@ xvwWmMenuSelect:
 *
 *  This prevents other parts of XWP from processing messages in
 *  WM_MENUSELECT that Xview will handle later in WM_COMMAND
 *
 *  Returns TRUE if the message should bypass standard handling,
 *  FALSE otherwise.
 *
 *@@added V1.0.11 (2016-08-12) [rwalsh]
 */

BOOL xvwWmMenuSelect(MPARAM mp1)
{
    if (SHORT2FROMMP(mp1))
    {
        switch ((ULONG)SHORT1FROMMP(mp1) - *G_pulVarMenuOfs)
        {
            case ID_XFMI_OFS_SMALLICONS:
            case ID_XFMI_OFS_FLOWED:
            case ID_XFMI_OFS_NONFLOWED:
            case ID_XFMI_OFS_NOGRID:
            case ID_XFMI_OFS_SHOWSTATUSBAR:
            case ID_XFMI_OFS_DEFAULTICON:
            case ID_XFMI_OFS_DEFAULTDETAILS:
            case ID_XFMI_OFS_CNRBACKGROUNDS:
            case ID_XFMI_OFS_SETSYSDEFAULT:
            case ID_XFMI_OFS_CNRSPLITBAR:
            case ID_XFMI_OFS_ICON_COLUMN:
            case ID_XFMI_OFS_TITLE_COLUMN:
            case ID_XFMI_OFS_CLASS_COLUMN:
            case ID_XFMI_OFS_NAME_COLUMN:
            case ID_XFMI_OFS_SIZE_COLUMN:
            case ID_XFMI_OFS_WDATE_COLUMN:
            case ID_XFMI_OFS_WTIME_COLUMN:
            case ID_XFMI_OFS_ADATE_COLUMN:
            case ID_XFMI_OFS_ATIME_COLUMN:
            case ID_XFMI_OFS_CDATE_COLUMN:
            case ID_XFMI_OFS_CTIME_COLUMN:
            case ID_XFMI_OFS_FLAGS_COLUMN:
                return TRUE;
        }
    }
    return FALSE;
}

/********************************************************************/

void xvwSetFilesCnrView(PFDRSPLITVIEW psv, ULONG ulView);
BOOL xvwSetIconAttr(PFDRSPLITVIEW psv, HWND hwndMenu, ULONG ulAttr);
BOOL xvwToggleCnrBackground(PFDRSPLITVIEW psv);
BOOL xvwToggleCnrSplitbar(PFDRSPLITVIEW psv);
BOOL xvwToggleStatusbar(PFDRSPLITVIEW psv, HWND hwndFrame);
BOOL xvwToggleDetailsColumn(PFDRSPLITVIEW psv, HWND hwndMenu,
                            ULONG ulColumn, USHORT usCmd);

/*
 *@@ xvwWmCommand:
 *
 *  This provides Xview-specific handling for some messages
 *  generated by the menubar pulldown menus. While many are
 *  Xview-only, some are processed here to preempt their
 *  handling elsewhere - in particular, the icon-view messages.
 *  It would take multiple hacks in fdrmenus.c to identify the
 *  correct target window and view they apply to while here we
 *  have that info immediately available.
 *
 *  Returns TRUE if the message was handled, FALSE otherwise.
 *
 *@@added V1.0.11 (2016-08-12) [rwalsh]
 */

BOOL xvwWmCommand(PFDRSPLITVIEW psv, USHORT usCmd, HWND hwndFrame)
{
    HWND        hwndMenu;
    ULONG       ulCmd;
    ULONG       ulColumn;

    switch (usCmd)
    {
        case WPMENUID_CHANGETOICON:
            xvwSetFilesCnrView(psv, XVF_OPEN_ICON);
            return TRUE;

        case WPMENUID_CHANGETODETAILS:
            xvwSetFilesCnrView(psv, XVF_OPEN_DETAILS);
            return TRUE;

        default:
            // exit now if the msg couldn't be something we handle
            if ((ULONG)usCmd < ID_XFMI_OFS_SEPARATOR + *G_pulVarMenuOfs)
                return FALSE;
    }

    ulColumn = 0;
    hwndMenu = WinWindowFromID(hwndFrame, FID_MENU);
    ulCmd = (ULONG)usCmd - *G_pulVarMenuOfs;

    switch (ulCmd)
    {
        case ID_XFMI_OFS_NOGRID:
            return xvwSetIconAttr(psv, hwndMenu, CV_ICON);

        case ID_XFMI_OFS_FLOWED:
            return xvwSetIconAttr(psv, hwndMenu, CV_NAME | CV_FLOW);

        case ID_XFMI_OFS_NONFLOWED:
            return xvwSetIconAttr(psv, hwndMenu, CV_NAME);

        case ID_XFMI_OFS_SMALLICONS:
            return xvwSetIconAttr(psv, hwndMenu, CV_MINI);

        case ID_XFMI_OFS_CNRBACKGROUNDS:
            winhSetMenuItemChecked(hwndMenu, usCmd,
                                   (xvwToggleCnrBackground(psv)));
            return TRUE;

        case ID_XFMI_OFS_CNRSPLITBAR:
            winhSetMenuItemChecked(hwndMenu, usCmd,
                                   (xvwToggleCnrSplitbar(psv)));
            return TRUE;

        case ID_XFMI_OFS_SHOWSTATUSBAR:
            winhSetMenuItemChecked(hwndMenu, usCmd,
                                   (xvwToggleStatusbar(psv, hwndFrame)));
            return TRUE;

        case ID_XFMI_OFS_SETSYSDEFAULT:
            cmnSetSetting(sXviewDefaultState, (psv->flState & XVF_SAVEMASK));
            cmnSetSetting(sXviewIconAttrib, (psv->flIconAttr & ICON_MASK));
            cmnSetSetting(sXviewDetailsCols, (psv->flColumns & COL_MASK));
            return TRUE;

        case ID_XFMI_OFS_DEFAULTICON:
        case ID_XFMI_OFS_DEFAULTDETAILS:
            psv->flState &= ~XVF_DFLT_MASK;
            psv->flState |= (ulCmd == ID_XFMI_OFS_DEFAULTDETAILS)
                            ? XVF_DFLT_DETAILS : XVF_DFLT_ICON;

            winhSetMenuItemChecked(hwndMenu,
                                   ID_XFMI_OFS_DEFAULTICON + *G_pulVarMenuOfs,
                                   (psv->flState & XVF_DFLT_ICON));
            winhSetMenuItemChecked(hwndMenu,
                                   ID_XFMI_OFS_DEFAULTDETAILS + *G_pulVarMenuOfs,
                                   (psv->flState & XVF_DFLT_DETAILS));
            return TRUE;

        case ID_XFMI_OFS_ICON_COLUMN:
            ulColumn = COL_ICON;
            break;
    
        case ID_XFMI_OFS_TITLE_COLUMN:
            ulColumn = COL_TITLE;
            break;
    
        case ID_XFMI_OFS_CLASS_COLUMN:
            ulColumn = COL_CLASS;
            break;
    
        case ID_XFMI_OFS_NAME_COLUMN:
            ulColumn = COL_NAME;
            break;
    
        case ID_XFMI_OFS_SIZE_COLUMN:
            ulColumn = COL_SIZE;
            break;
    
        case ID_XFMI_OFS_WDATE_COLUMN:
            ulColumn = COL_WDATE;
            break;
    
        case ID_XFMI_OFS_WTIME_COLUMN:
            ulColumn = COL_WTIME;
            break;
    
        case ID_XFMI_OFS_ADATE_COLUMN:
            ulColumn = COL_ADATE;
            break;
    
        case ID_XFMI_OFS_ATIME_COLUMN:
            ulColumn = COL_ATIME;
            break;
    
        case ID_XFMI_OFS_CDATE_COLUMN:
            ulColumn = COL_CDATE;
            break;
    
        case ID_XFMI_OFS_CTIME_COLUMN:
            ulColumn = COL_CTIME;
            break;
    
        case ID_XFMI_OFS_FLAGS_COLUMN:
            ulColumn = COL_FLAGS;
            break;

        default:
            return FALSE;
    }

    if (ulColumn)
        return xvwToggleDetailsColumn(psv, hwndMenu, ulColumn, usCmd);

    // it's unlikely we'd get here
    return FALSE;
}

/*
 *@@ xvwSetFilesCnrView:
 *
 *  This is called the first time the container is displayed,
 *  and again each time the user toggles the view using the menu.
 *  If details view init failed, the container stays in icon view.
 *
 *@@added V1.0.11 (2016-08-12) [rwalsh]
 */

void xvwSetFilesCnrView(PFDRSPLITVIEW psv, ULONG ulView)
{
    ULONG flWindowAttr;

    if (ulView == XVF_OPEN_DETAILS && (psv->flState & XVF_INIT_OK))
    {
        flWindowAttr = G_CnrInfo.flWindowAttr;
    }
    else
    {
        ulView = XVF_OPEN_ICON;
        flWindowAttr = psv->flIconAttr;
    }

    // switch the container's view
    BEGIN_CNRINFO()
    {
        cnrhSetView(flWindowAttr);
    } END_CNRINFO(psv->hwndFilesCnr);

    // save the current view
    psv->flState = (psv->flState & ~XVF_OPEN_MASK) | ulView;
    return;
}

/*
 *@@ xvwSetIconAttr:
 *
 *  This sets icon view attribute flags, applies them
 *  if the files container is currently in icon view,
 *  and checks the corresponding menu items
 *
 *  Always returns TRUE.
 *
 *@@added V1.0.11 (2016-08-12) [rwalsh]
 */

BOOL xvwSetIconAttr(PFDRSPLITVIEW psv, HWND hwndMenu, ULONG ulAttr)
{
    // update the flags
    if (ulAttr == CV_MINI)
        psv->flIconAttr ^= CV_MINI;
    else
    {
        psv->flIconAttr &= ~(CV_ICON | CV_NAME | CV_FLOW);
        psv->flIconAttr |= ulAttr;
    }

    // update the container if it's currently in Icon view
    if (psv->flState & XVF_OPEN_ICON)
        xvwSetFilesCnrView(psv, XVF_OPEN_ICON);

    // check/uncheck the relevant menuitems
    if (ulAttr == CV_MINI)
        winhSetMenuItemChecked(hwndMenu,
                               ID_XFMI_OFS_SMALLICONS + *G_pulVarMenuOfs,
                               (psv->flIconAttr & CV_MINI) == CV_MINI);
    else
    {
        ULONG ulMask = psv->flIconAttr & (CV_ICON | CV_NAME | CV_FLOW);

        winhSetMenuItemChecked(hwndMenu,
                               ID_XFMI_OFS_NOGRID + *G_pulVarMenuOfs,
                               ulMask == CV_ICON);
        winhSetMenuItemChecked(hwndMenu,
                               ID_XFMI_OFS_FLOWED + *G_pulVarMenuOfs,
                               ulMask == (CV_NAME | CV_FLOW));
        winhSetMenuItemChecked(hwndMenu,
                               ID_XFMI_OFS_NONFLOWED + *G_pulVarMenuOfs,
                               ulMask == CV_NAME);
    }

    return TRUE;
}

/*
 *@@ xvwToggleCnrBackground:
 *
 *  Enable/disable the use of the selected folder's customized font
 *  and background color/bitmap by toggling the SPLIT_FDRSTYLES bit.
 *  When off, the wndprocs will stop responding to FM_SETCNRLAYOUT
 *  which sets these presparams.
 *
 *  For each toggle, the current presparams must be reset here to
 *  provide immediate feedback. If enabled, use the values from the
 *  root folder and selected folder. If disabled, use the values
 *  from <XWP_SVDETAILS> which should reflect current system defaults.
 *
 *  Returns the current background state (on/off).
 *
 *@@added V1.0.11 (2016-08-12) [rwalsh]
 */

BOOL xvwToggleCnrBackground(PFDRSPLITVIEW psv)
{
    BOOL        rc;
    WPFolder *  pFiles;
    CNRINFO     CnrInfo;

    // don't disable backgrounds if we don't have the generic presparams
    if ((psv->flState & XVF_BACKGROUND) && !G_pszFont)
        return TRUE;

    // toggle appropriate bits & identify the result
    psv->flSplit ^= SPLIT_FDRSTYLES;
    psv->flState ^= XVF_BACKGROUND;
    rc = (psv->flState & XVF_BACKGROUND) == XVF_BACKGROUND;

    // because the cnrs are subclassed and CA_OWNERPAINTBACKGROUND is set,
    // XWP will continue to paint the bkgnd unless we tell it not to
    fdrvDisableBkgndPainting(psv->hwndTreeCnr, !rc);
    fdrvDisableBkgndPainting(psv->hwndFilesCnr, !rc);

    if (rc)
    {
        // if on, point at the folders used to set
        // various presparams, then have XWP set them

        if (!(pFiles = fdrvGetFSFromRecord(psv->precFilesShowing, TRUE)))
            pFiles = psv->pRootsFolder;
        fdrvSetCnrLayout(psv->hwndTreeCnr, psv->pRootsFolder, OPEN_TREE);
        fdrvSetCnrLayout(psv->hwndFilesCnr, pFiles,
                         (psv->flState & XVF_OPEN_DETAILS)
                         ? OPEN_DETAILS : OPEN_CONTENTS);
    }
    else
    {
        // if off, set the presparams here; disable window
        // painting until we're done to avoid flickering

        WinEnableWindowUpdate(psv->hwndMainControl, FALSE);

        winhSetPresColor(psv->hwndTreeCnr, PP_BACKGROUNDCOLOR, G_clrBack);
        winhSetPresColor(psv->hwndTreeCnr, PP_FOREGROUNDCOLOR, G_clrFore);
        winhSetWindowFont(psv->hwndTreeCnr, G_pszFont);

        winhSetPresColor(psv->hwndFilesCnr, PP_BACKGROUNDCOLOR, G_clrBack);
        winhSetPresColor(psv->hwndFilesCnr, PP_FOREGROUNDCOLOR, G_clrFore);
        winhSetWindowFont(psv->hwndFilesCnr, G_pszFont);

        WinEnableWindowUpdate(psv->hwndMainControl, TRUE);
        WinUpdateWindow(psv->hwndMainControl);
    }

    return rc;
}

/*
 *@@ xvwToggleCnrSplitbar:
 *
 *  Disable the split bar by setting its position to -1 and the
 *  last column before the bar to null; enable it by restoring
 *  the previously saved values. Note: changing both values ensures
 *  the bar is hidden and highlighted records are painted properly.
 *
 *  Returns the current state of the split bar.
 *
 *@@added V1.0.11 (2016-08-12) [rwalsh]
 */

BOOL xvwToggleCnrSplitbar(PFDRSPLITVIEW psv)
{
    CNRINFO CnrInfo;

    // if the bar is on, save its current position,
    // then set up to disable it 
    if (psv->flColumns & COL_SPLIT)
    {
        if (WinSendMsg(psv->hwndFilesCnr, CM_QUERYCNRINFO,
                       (MPARAM)&CnrInfo, (MPARAM)sizeof(CnrInfo)))
        {
            psv->lCnrSplitPos = CnrInfo.xVertSplitbar;
            psv->pCnrSplitCol = CnrInfo.pFieldInfoLast;
        }
        CnrInfo.xVertSplitbar = -1;
        CnrInfo.pFieldInfoLast = NULL;
    }
    // if off, restore the previous position
    else
    {
        CnrInfo.xVertSplitbar = psv->lCnrSplitPos;
        CnrInfo.pFieldInfoLast = psv->pCnrSplitCol;
    }

    // update position; if successful, toggle the flag
    CnrInfo.cb = sizeof(CnrInfo);
    if (WinSendMsg(psv->hwndFilesCnr, CM_SETCNRINFO, (MPARAM)&CnrInfo,
                   (MPARAM)(CMA_PFIELDINFOLAST | CMA_XVERTSPLITBAR)))
        psv->flColumns ^= COL_SPLIT;

    return ((psv->flColumns & COL_SPLIT) == COL_SPLIT);
}

/*
 *@@ xvwToggleStatusbar:
 *
 *  This toggles the status bar's visibility without actually doing
 *  anything to it. It simply changes the frame's height to include
 *  or exclude the bar from it's visible region.
 *
 *  Note: when XVF_STATUSBAR is off the frame no longer responds to
 *  WM_FORMATFRAME and friends, so the statusbar stays where it is
 *
 *  Returns the current statusbar state (on/off).
 *
 *@@added V1.0.11 (2016-08-12) [rwalsh]
 */

BOOL xvwToggleStatusbar(PFDRSPLITVIEW psv, HWND hwndFrame)
{
    BOOL    rc;
    SWP     swp;
    LONG    cy;

    psv->flState ^= XVF_STATUSBAR;
    rc = (psv->flState & XVF_STATUSBAR) == XVF_STATUSBAR;

    cy = cmnQueryStatusBarHeight();
    WinQueryWindowPos(hwndFrame, &swp);

    // adjust the frame's height & position so that none
    // of the windows above the statusbar change location
    swp.y  += rc ? -cy :  cy;
    swp.cy += rc ?  cy : -cy;

    // disable updates to the main split window and its children
    // to avoid having them bounce around as the frame gets reformatted
    WinEnableWindowUpdate(psv->hwndMainControl, FALSE);

    WinSetWindowPos(hwndFrame, 0, swp.x, swp.y, swp.cx, swp.cy,
                    SWP_SIZE | SWP_MOVE | SWP_NOADJUST);

    // reenable updating, then update just in case; this should
    // have no effect since nothing about these windows changed
    WinEnableWindowUpdate(psv->hwndMainControl, TRUE);
    WinUpdateWindow(psv->hwndMainControl);

    return rc;
}

/*
 *@@ xvwToggleDetailsColumn:
 *
 *  This toggles the visibility bit for the selected column's
 *  FIELDINFO, alerts the container that a change has occurred
 *  which will trigger a visual update if currently in details
 *  view, then checks the corresponding menu items
 *
 *  Always returns TRUE.
 *
 *@@added V1.0.11 (2016-08-12) [rwalsh]
 */

BOOL xvwToggleDetailsColumn(PFDRSPLITVIEW psv, HWND hwndMenu,
                              ULONG ulColumn, USHORT usCmd)
{
    PFIELDINFO  pfi;
    ULONG       flag;

    flag = 1;
    pfi = (PFIELDINFO)WinSendMsg(psv->hwndFilesCnr, CM_QUERYDETAILFIELDINFO,
                                 0, (MPARAM)CMA_FIRST);

    // with each iteration, the flag bit is shifted left
    // until it's in the same position as the bit in ulColumn;
    // when they match, we've found the right column 
    while (flag <= ulColumn && pfi && pfi != (PFIELDINFO)-1)
    {
        // we've arrived at the FIELDINFO we want
        if (flag == ulColumn)
        {
            // toggle the visibility flag, then update the container
            pfi->flData ^= CFA_INVISIBLE;
            WinSendMsg(psv->hwndFilesCnr, CM_INVALIDATEDETAILFIELDINFO, 0, 0);

            // update the bitmap holding the visibility of each column
            psv->flColumns &= ~ulColumn;
            psv->flColumns |= (pfi->flData & CFA_INVISIBLE) ? 0 : ulColumn;

            // check/uncheck the relevant menuitem
            winhSetMenuItemChecked(hwndMenu, usCmd,
                                   (psv->flColumns & ulColumn));
            break;
        }

        // move on to the next column's FIELDINFO
        flag <<= 1;
        pfi = (PFIELDINFO)WinSendMsg(psv->hwndFilesCnr, CM_QUERYDETAILFIELDINFO,
                                     (MPARAM)pfi, (MPARAM)CMA_NEXT);
    }

    return TRUE;
}

/********************************************************************/

void xvwInitDetailsView(PFDRSPLITVIEW psv);
void xvwInitDefaultLayout(XFolder *pFolder);
BOOL xvwSetupDetailsView(PFDRSPLITVIEW psv);

/*
 *@@ xvwInitXview:
 *
 *  This is called by fdrSetupSplitView() the first time the
 *  container is displayed to initialize various state and view
 *  flags. It also triggers local and global initialization for
 *  details view. If they fail, or have failed previously, it
 *  forces the container to stay in icon view mode.
 *
 *@@added V1.0.11 (2016-08-12) [rwalsh]
 */

void xvwInitXview(void* pv)
{
    PFDRSPLITVIEW psv = (PFDRSPLITVIEW)pv;
    CNRINFO CnrInfo;
    LONG    lAlwaysSort;
    LONG    lDefaultSort;

    // set up icon view attributes
    if (!psv->flIconAttr)
        psv->flIconAttr = cmnQuerySetting(sXviewIconAttrib);
    psv->flIconAttr &= ICON_MASK;

    // set up details view columns
    if (!psv->flColumns)
        psv->flColumns = cmnQuerySetting(sXviewDetailsCols);
    psv->flColumns &= COL_MASK;
    psv->flColumns |= COL_TITLE;

    // set up the default window state (view, statusbar, etc)
    if (!psv->flState)
        psv->flState = cmnQuerySetting(sXviewDefaultState);
    psv->flState &= XVF_SAVEMASK;

    // G_pFieldInfo acts as a flag that identifies whether global
    // initialization has been done;  if init failed, it won't be
    // retried & details view won't be available for this session
    // -1 == needs init;  0 == init failed;  other == init OK
    if (G_pFieldInfo == (PFIELDINFO)-1)
        xvwInitDetailsView(psv);

    // set up details view for this container
    if (G_pFieldInfo && xvwSetupDetailsView(psv))
        psv->flState |= XVF_INIT_OK;

    // set the open view to match the default view,
    // provided everything is OK - if not, force icon view
    if ((psv->flState & XVF_DFLT_MASK) == XVF_DFLT_DETAILS &&
        (psv->flState & XVF_INIT_OK))
        psv->flState |= XVF_OPEN_DETAILS;
    else
        psv->flState = (psv->flState & ~XVF_DFLT_MASK) |
                       (XVF_DFLT_ICON | XVF_OPEN_ICON);

    // if backgrounds is off, set the containers' preparams to the
    // default folder's; if successful, this turns XVF_BACKGROUND
    // off again and also turns off SPLIT_FDRSTYLES
    if (!(psv->flState & XVF_BACKGROUND))
    {
        psv->flState |= XVF_BACKGROUND;
        xvwToggleCnrBackground(psv);
    }

    // set sort criterion to the root folder's default
    _xwpQueryFldrSort(psv->pRootsFolder, &lDefaultSort, 0, &lAlwaysSort);
    CnrInfo.pSortRecord = lAlwaysSort
                          ? (void*)fdrQuerySortFunc(psv->pRootsFolder, lDefaultSort)
                          : NULL;
    WinSendMsg(psv->hwndFilesCnr, CM_SETCNRINFO, (MPARAM)&CnrInfo, (MPARAM)CMA_PSORTRECORD);

    // finally, set the selected view and we're done
    xvwSetFilesCnrView(psv, (psv->flState & XVF_OPEN_MASK));

    return;
}

/*
 *@@ xvwInitDetailsView:
 *
 *  This runs once and performs global initialization the first time
 *  details view is used. If it fails, details view won't be available
 *  for the current session.
 *
 *  In order to fully emulate the WPS's undocumented internal handling
 *  for this view, it opens a generic folder (creating it if necessary)
 *  then copies its CNRINFO and series of FIELDINFO structures into
 *  global variables. These form the basis for the flags and values
 *  used for Xview's details view implementation.
 *
 *  This routine also calls xvwInitDefaultLayout() to capture the
 *  current default colors and font for a generic folder.
 *
 *  Note: the CLASSFIELDINFO structure that you might expect would be
 *  used here is *not* suitable. The container's FIELDINFOs have many
 *  additional flags set, plus they have the correct offset for the
 *  fields' data - the CFI just has the offset from the start of the
 *  class's data.
 *
 *@@added V1.0.11 (2016-08-12) [rwalsh]
 */

void xvwInitDetailsView(PFDRSPLITVIEW psv)
{
    HWND            hFrame = 0;
    HWND            hCnr;
    ULONG           ctr;
    BOOL            fUnlock = FALSE;
    PFIELDINFO      pfi;
    PFIELDINFO      pfiIn;
    WPFolder *      pDetails = NULL;
    PSZ             pszError = NULL;

do {
    G_pFieldInfo = NULL;

    // use our folder's ObjectID to see if it can be found
    pDetails = _wpclsQueryObjectFromPath(_WPFileSystem, "<XWP_SVDETAILS>");

    // the folder should be in the NOWHERE directory with the name XWP_SVDETAILS
    if (!pDetails)
    {
        WPFolder *  pNowhere;
        PSZ pszDetailsSetup = "DEFAULTVIEW=102;NODELETE=YES;NOMOVE=YES;"
                              "OBJECTID=<XWP_SVDETAILS>;";

        // locate Nowhere, then look for the folder by name
        pNowhere = (WPFolder*)_wpclsQueryObjectFromPath(_WPFileSystem, "<WP_NOWHERE>");
        if (!pNowhere)
        {
            pszError = "unable to locate <WP_NOWHERE>";
            break;
        }
        pDetails = (WPFolder*)_wpclsFileSysExists(_WPFileSystem, pNowhere,
                                                  "XWP_SVDETAILS", FILE_DIRECTORY);

        // if found, reattach it's objectID
        if (pDetails)
        {
            if (!_wpSetup(pDetails, pszDetailsSetup))
            {
                pszError = "wpSetup failed for XWP_SVDETAILS";
                break;
            }
        }
        // otherwise, create a new folder
        else
        {
            pDetails = (WPFolder*)_wpclsNew(_WPFolder, "XWP_SVDETAILS",
                                            pszDetailsSetup, pNowhere, TRUE);
            if (!pDetails)
            {
                pszError = "wpclsNew failed for XWP_SVDETAILS";
                break;
            }
        }
    }

    // lock the object while we're working with it
    if (!_wpIsLocked(pDetails))
    {
        _wpLockObject(pDetails);
        fUnlock = TRUE;
    }

    // open the folder in Details view, then hide the window;
    // the very first time the folder opens, it will be visible
    // momentarily but should never be visible again
    hFrame = _wpOpen(pDetails, 0, OPEN_DETAILS, 0);
    if (hFrame == 0)
    {
        pszError = "wpOpen failed";
        break;
    }
    WinShowWindow(hFrame, FALSE);

    // wait until the folder has been opened, then
    // use it to capture the default colors & font
    xvwInitDefaultLayout(pDetails);

    hCnr = WinWindowFromID(hFrame, FID_CLIENT);
    if (hCnr == 0)
    {
        pszError = "WinWindowFromId failed";
        break;
    }

    // get and save the container's CNRINFO
    if (WinSendMsg(hCnr, CM_QUERYCNRINFO, (MPARAM)&G_CnrInfo, (MPARAM)sizeof(CNRINFO)) !=
        (MRESULT)sizeof(CNRINFO))
    {
        pszError = "CM_QUERYCNRINFO failed";
        break;
    }

    if (G_CnrInfo.cFields == 0)
    {
        pszError = "G_CnrInfo.cFields is zero";
        break;
    }

    // begin the process of capturing the container's FIELDINFO structs;
    // if we can't get the first one, there's no reason to bother
    // allocating memory to store them
    pfiIn = (PFIELDINFO)WinSendMsg(hCnr, CM_QUERYDETAILFIELDINFO, 0, (MPARAM)CMA_FIRST);
    if (pfiIn == 0 || pfiIn == (PFIELDINFO)-1)
    {
        pszError = "first CM_QUERYDETAILFIELDINFO failed";
        break;
    }

    G_pFieldInfo = (PFIELDINFO)_somAllocate(_WPFileSystem,
                                            G_CnrInfo.cFields * sizeof(FIELDINFO));
    if (!G_pFieldInfo)
    {
        pszError = "somAllocate failed";
        break;
    }

    // while the WPS handles FIELDINFOs as a linked list,
    // we store them in an array for easier access
    ctr = G_CnrInfo.cFields;
    pfi = G_pFieldInfo;
    do {
        memcpy(pfi, pfiIn, sizeof(FIELDINFO));
        ctr--;
        pfi++;
        pfiIn = (PFIELDINFO)WinSendMsg(hCnr, CM_QUERYDETAILFIELDINFO,
                                       (MPARAM)pfiIn, (MPARAM)CMA_NEXT);
    } while (ctr && pfiIn != 0 && pfiIn != (PFIELDINFO)-1);

    if (ctr || pfiIn == (PFIELDINFO)-1)
    {
        pszError = "CM_QUERYDETAILFIELDINFO loop failed";

        _somDeallocate(_WPFileSystem, G_pFieldInfo);
        G_pFieldInfo = NULL;
        break;
    }

} while (0);

    if (hFrame)
        _wpClose(pDetails);

    if (fUnlock)
        _wpUnlockObject(pDetails);

    if (pszError)
        cmnLog(__FILE__, __LINE__, __FUNCTION__, pszError);

    return;
}

 /*
 *@@ xvwInitDefaultLayout:
 *
 *  Captures the default colors and font of the XWP_SVDETAILS
 *  folder for use when "backgrounds" is turned off.
 *
 *@@added V1.0.11 (2016-08-12) [rwalsh]
 */

void xvwInitDefaultLayout(XFolder *pFolder)
{
    PSZ     pszImageFileName;
    ULONG   ulImageMode;
    ULONG   ulScaleFactor;
    ULONG   ulBackgroundType;
    LONG    lBackgroundColor;

    G_pszFont = _wpQueryFldrFont(pFolder, OPEN_CONTENTS);
    G_clrFore = _wpQueryIconTextColor(pFolder, OPEN_CONTENTS);

    _wpQueryFldrBackground(pFolder, &pszImageFileName, &ulImageMode,
                           &ulScaleFactor, &ulBackgroundType,
                           &lBackgroundColor);
    G_clrBack = lBackgroundColor;
}

/*
 *@@ xvwSetupDetailsView:
 *
 *  This runs once for each new container the first time it is opened.
 *  It copies the relevant data from the global CNRINFO and FIELDINFO
 *  structs, modifies it as needed, then inserts it into the container.
 *  It also inits the splitbar's visibility and position.
 *
 *@@added V1.0.11 (2016-08-12) [rwalsh]
 */

BOOL xvwSetupDetailsView(PFDRSPLITVIEW psv)
{
    PFIELDINFO      pFI = 0;
    PFIELDINFO      pfi;
    PFIELDINFO      pfiIn;
    FIELDINFOINSERT fii;
    CNRINFO         ci;
    ULONG           ctr;
    ULONG           flag;
    BOOL            brc = FALSE;
    PSZ             pszError = NULL;

do {
    pFI = (PFIELDINFO)WinSendMsg(psv->hwndFilesCnr, CM_ALLOCDETAILFIELDINFO,
                                 (MPARAM)G_CnrInfo.cFields, 0);
    if (!pFI)
    {
        pszError = "CM_ALLOCDETAILFIELDINFO failed";
        break;
    }

    ci = G_CnrInfo;
    ctr = ci.cFields;
    pfi = pFI;
    pfiIn = G_pFieldInfo;
    flag = 1;

    do {
        // only copy the relevant parts of the global data
        memcpy(&pfi->flData, &pfiIn->flData, (5 * sizeof(ULONG)));

        // identify the field where the splitbar will go; also, have
        // the cnr draw a column separator - otherwise, it will look
        // wierd if the splitbar is hidden but the field is visible
        if (flag == COL_CLASS)
        {
            psv->pCnrSplitCol = pfi;
            pfi->flData |= CFA_SEPARATOR;
        }

        // use this folder's visible-columns bitmap to determine
        // whether to mark this column as visible or hidden
        pfi->flData &= ~CFA_INVISIBLE;
        pfi->flData |= (flag & psv->flColumns) ? 0 : CFA_INVISIBLE;

        // move on to the next one
        pfi = pfi->pNextFieldInfo;
        pfiIn++;
        flag <<= 1;
        ctr--;
    } while (ctr && pfi);

    // setup, then insert the FIELDINFOs
    fii.cb = sizeof(FIELDINFOINSERT);
    fii.pFieldInfoOrder = (PFIELDINFO)CMA_LAST;
    fii.fInvalidateFieldInfo = TRUE;
    fii.cFieldInfoInsert = ci.cFields;

    if (!WinSendMsg(psv->hwndFilesCnr, CM_INSERTDETAILFIELDINFO,
                    (MPARAM)pFI, (MPARAM)&fii))
    {
        pszError = "CM_INSERTDETAILFIELDINFO failed";
        break;
    }

    // init the splitbar visibility and position
    if (psv->lCnrSplitPos <= 0)
        psv->lCnrSplitPos = G_CnrInfo.xVertSplitbar;
    if (psv->flColumns & COL_SPLIT)
    {
        ci.xVertSplitbar  = psv->lCnrSplitPos;
        ci.pFieldInfoLast = psv->pCnrSplitCol;
    }
    else
    {
        ci.xVertSplitbar = -1;
        ci.pFieldInfoLast = NULL;
    }

    // this sets splitbar options but doesn't switch the view
    if (!WinSendMsg(psv->hwndFilesCnr, CM_SETCNRINFO, (MPARAM)&ci,
                    (MPARAM)(CMA_PFIELDINFOLAST | CMA_XVERTSPLITBAR)))
    {
        pszError = "CM_SETCNRINFO failed";
        break;
    }

    brc = TRUE;

} while (0);

    if (pszError)
        cmnLog(__FILE__, __LINE__, __FUNCTION__, pszError);

    return brc;
}

/********************************************************************/

