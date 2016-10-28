
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
 *   Global variables
 *
 ********************************************************************/

PFNWP       G_pfnwpXviewFrameOrig = NULL;

/********************************************************************/

/*
 *@@ xvwCreateXview:
 *      creates a frame window with a split window and
 *      does the usual "register view and pass a zillion
 *      QWL_USER pointers everywhere" stuff.
 *
 *      Returns the frame window or NULLHANDLE on errors.
 *
 *@@changed V1.0.9 (2011-10-06) [rwalsh]: use global settings for status and menu bars
 *@@changed V1.0.9 (2011-10-08) [rwalsh]: subclass frame after registering view to preempt WPS subclass
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
            cbPos = sizeof(pos);
            if ((!(PrfQueryProfileData(HINI_USER,
                                       (PSZ)INIAPP_FDRSPLITVIEWPOS,
                                       pxvd->szFolderPosKey,
                                       &pos,
                                       &cbPos)))
                 || (cbPos != sizeof(XVIEWPOS)))
            {
                // no position stored yet:
                pos.x = (winhQueryScreenCX() - 600) / 2;
                pos.y = (winhQueryScreenCY() - 400) / 2;
                pos.cx = 600;
                pos.cy = 400;
                pos.lSplitBarPos = 30;
            }

            flSplit =   SPLIT_ANIMATE
                      | SPLIT_FDRSTYLES
                      | SPLIT_MULTIPLESEL;

            //@@changed V1.0.9 (2011-10-06) [rwalsh]: display status and menu
            //  bars based on global settings rather than the current root's
            // if (stbFolderWantsStatusBars(pRootsFolder))
            // if (_xwpQueryMenuBarVisibility(pRootsFolder))

            if (cmnQuerySetting(sfDefaultStatusBarVisibility))
                flSplit |= SPLIT_STATUSBAR;

            if (mnuQueryDefaultMenuBarVisibility())
                flSplit |= SPLIT_MENUBAR;

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
                                cmnGetString(ID_XFSI_FDR_SPLITVIEW));

                // subclass the frame _after_ registering the view
                // so our subclass proc sees messages before they
                // are routed to the root folder object
                // changed V1.0.9 (2011-10-08) [rwalsh]
                G_pfnwpXviewFrameOrig = WinSubclassWindow(pxvd->sv.hwndMainFrame,
                                                          fnwpXviewFrame);

                // change the window title to full path, if allowed
                // (not for WPDisk)
                if (pRootObject == pRootsFolder)
                {
                    XFolderData *somThis = XFolderGetData(pRootObject);
                    if ((_bFullPathInstance == 1)
                        || ((_bFullPathInstance == 2)
                            && (cmnQuerySetting(sfFullPath))))
                        fdrSetOneFrameWndTitle(pRootObject, pxvd->sv.hwndMainFrame);
                }

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

/*
 *@@ fnwpXviewFrame:
 *
 *@@added V1.0.0 (2002-08-21) [umoeller]
 *@@changed V1.0.5 (2006-04-13) [pr]: Fix status bar update @@fixes 326
 *@@changed V1.0.9 (2011-10-08) [rwalsh]: generate menus for selected folder, not root folder
 *@@changed V1.0.9 (2011-10-08) [rwalsh]: handle WM_COMMAND as though it came from selected folder
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
                 && (pxvd->sv.hwndStatusBar))
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
                 && (pxvd->sv.hwndStatusBar))
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
                 && (pxvd->sv.hwndStatusBar))
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

            }
        break;

        /*
         * WM_MEASUREITEM:
         *      same processing as in fdrProcessFolderMsgs.
         */

        case WM_MEASUREITEM:
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
         */

        case WM_COMMAND:
        case WM_MENUSELECT:
            if (pxvd = (PXVIEWDATA)WinQueryWindowPtr(hwndFrame, QWL_USER))
            {
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

