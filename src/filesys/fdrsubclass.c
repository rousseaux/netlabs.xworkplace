
/*
 *@@sourcefile fdrsubclass.c:
 *      This file is ALL new with V0.9.3 and now implements
 *      folder frame subclassing, which has been largely
 *      rewritten with V0.9.3.
 *
 *      While we had an ugly global linked list of subclassed
 *      folder views in all XFolder and XWorkplace versions
 *      before V0.9.3, I have finally (after two years...)
 *      found a more elegant way of storing folder data. The
 *      secret is simply re-registing the folder view window
 *      class ("wpFolder window"), which is initially registered
 *      by the WPS somehow. No more global variables and mutex
 *      semaphores...
 *
 *      This hopefully fixes most folder serialization problems
 *      which have occured with previous XFolder/XWorkplace versions.
 *
 *      Since I was unable to find out where WinRegisterClass
 *      gets called from in the WPS, I have implemented a local
 *      send-message hook for PMSHELL.EXE only which re-registers
 *      that window class again when the first WM_CREATE for a
 *      folder view comes in. See fdr_SendMsgHook. Basically,
 *      this adds more bytes for the window words so we can store
 *      a SUBCLASSEDFOLDERVIEW structure for each folder view in
 *      the window words, instead of having to maintain a global
 *      linked list.
 *
 *      When a folder view is subclassed (during XFolder::wpOpen
 *      processing) by fdrSubclassFolderView, a SUBCLASSEDFOLDERVIEW is
 *      created and stored in the new window words. Appears to
 *      work fine so far.
 *
 *      This file also holds the fdr_fnwpSubclassedFolderFrame
 *      procedure, with which folder frames are subclassed.
 *
 *      Function prefix for this file:
 *      --  fdr*
 *
 *@@added V0.9.3 [umoeller]
 *@@header "filesys\folder.h"
 */

/*
 *      Copyright (C) 1997-2000 Ulrich M”ller.
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

#define INCL_DOSEXCEPTIONS
#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#define INCL_DOSERRORS
#define INCL_WINWINDOWMGR
#define INCL_WINMESSAGEMGR
#define INCL_WINFRAMEMGR
#define INCL_WININPUT
#define INCL_WINRECTANGLES
#define INCL_WINSYS             // needed for presparams
#define INCL_WINMENUS
#define INCL_WINTIMER
#define INCL_WINDIALOGS
#define INCL_WINSTATICS
#define INCL_WINBUTTONS
#define INCL_WINENTRYFIELDS
#define INCL_WINLISTBOXES
#define INCL_WINSTDCNR
#define INCL_WINSHELLDATA       // Prf* functions
#define INCL_WINHOOKS
#include <os2.h>

// C library headers
#include <stdio.h>              // needed for except.h
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\cnrh.h"               // container helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\winh.h"               // PM helper routines

// SOM headers which don't crash with prec. header files
#include "xfldr.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

#include "filesys\fileops.h"            // file operations implementation
#include "filesys\folder.h"             // XFolder implementation
#include "filesys\menus.h"              // shared context menu logic
#include "filesys\statbars.h"           // status bar translation logic
#include "filesys\xthreads.h"           // extra XWorkplace threads

// other SOM headers
#pragma hdrstop                         // VAC++ keeps crashing otherwise

// #include <wpdesk.h>                     // WPDesktop
#include <wpshadow.h>                   // WPShadow

/* ******************************************************************
 *                                                                  *
 *   Global variables                                               *
 *                                                                  *
 ********************************************************************/

// flag for whether we have manipulated the "wpFolder window"
// PM window class already; this is done in fdr_SendMsgHook
BOOL                G_WPFolderWinClassExtended = FALSE;

CLASSINFO           G_WPFolderWinClassInfo;

ULONG               G_SFVOffset = 0;

// root of linked list to remember original window
// procedures when subclassing frame windows for folder hotkeys
// LINKLIST            G_llSubclassed;                   // changed V0.9.0
// mutex semaphore for access to this list
// HMTX                G_hmtxSubclassed = NULLHANDLE;

// original wnd proc for folder content menus,
// which we must subclass
PFNWP               G_pfnwpFolderContentMenuOriginal = NULL;

// flags for fdr_fnwpSubclassedFolderFrame;
// these are set by fdr_fnwpSubclFolderContentMenu.
// We can afford using global variables here
// because opening menus is a modal operation.
BOOL                G_fFldrContentMenuMoved = FALSE,
                    G_fFldrContentMenuButtonDown = FALSE;

/* ******************************************************************
 *                                                                  *
 *   Send-message hook                                              *
 *                                                                  *
 ********************************************************************/

/*
 *@@ fdr_SendMsgHook:
 *      local send-message hook for PMSHELL.EXE only.
 *      This is installed from M_XFolder::wpclsInitData
 *      and needed to re-register the "wpFolder window"
 *      PM window class which is used by the WPS for
 *      folder views. We add more window words to that
 *      class for storing our window data.
 *
 *@@added V0.9.3 (2000-04-08) [umoeller]
 */

VOID EXPENTRY fdr_SendMsgHook(HAB hab,
                              PSMHSTRUCT psmh,
                              BOOL fInterTask)
{
    /*
     * WM_CREATE:
     *
     */

    if (psmh->msg == WM_CREATE)
    {
        // re-register the WPFolder window class if we haven't
        // done this yet; this is needed because per default,
        // the WPS "wpFolder window" class apparently uses
        // QWL_USER for other purposes...
        if (!G_WPFolderWinClassExtended)
        {
            CHAR    szClass[300];

            _Pmpf(("fdr_SendMsgHook: checking WM_CREATE window class"));

            WinQueryClassName(psmh->hwnd,
                              sizeof(szClass),
                              szClass);

            _Pmpf(("    got %s", szClass));

            if (strcmp(szClass, "wpFolder window") == 0)
            {
                // it's a folder:
                // OK, we have the first WM_CREATE for a folder window
                // after WPS startup now...
                if (WinQueryClassInfo(hab,
                                      "wpFolder window",
                                      &G_WPFolderWinClassInfo))
                {
                    _Pmpf(("    wpFolder cbWindowData: %d", G_WPFolderWinClassInfo.cbWindowData));
                    _Pmpf(("    QWL_USER is: %d", QWL_USER));

                    // replace original window class
                    if (WinRegisterClass(hab,
                                         "wpFolder window",
                                         G_WPFolderWinClassInfo.pfnWindowProc, // fdr_fnwpSubclassedFolder2,
                                         G_WPFolderWinClassInfo.flClassStyle,
                                         G_WPFolderWinClassInfo.cbWindowData + 16))
                    {
                        _Pmpf(("    WinRegisterClass OK"));

                        // OK, window class successfully re-registered:
                        // store the offset of our window word for the
                        // SUBCLASSEDFOLDERVIEW's in a global variable
                        G_SFVOffset = G_WPFolderWinClassInfo.cbWindowData + 12;

                        // and don't do all this again...
                        G_WPFolderWinClassExtended = TRUE;
                    }
                    else
                        _Pmpf(("    WinRegisterClass failed"));
                }
            }
        }
    }
}

/* ******************************************************************
 *                                                                  *
 *   Management of folder frame window subclassing                  *
 *                                                                  *
 ********************************************************************/

/*
 *@@ fdrSubclassFolderView:
 *      creates a SUBCLASSEDFOLDERVIEW for the given folder
 *      view and subclasses the folder frame with
 *      fdr_fnwpSubclassedFolderFrame.
 *
 *      We also create a supplementary folder object window
 *      for the view here and store the HWND in the SFV.
 *
 *@@added V0.9.3 (2000-04-08) [umoeller]
 *@@changed V0.9.3 (2000-04-08) [umoeller]: no longer using the linked list
 */

PSUBCLASSEDFOLDERVIEW fdrSubclassFolderView(HWND hwndFrame,
                                            HWND hwndCnr,
                                            WPFolder *somSelf,
                                                 // in: folder; either XFolder's somSelf
                                                 // or XFldDisk's root folder
                                            WPObject *pRealObject)
                                                 // in: the "real" object; for XFolder, this is == somSelf,
                                                 // for XFldDisk, this is the disk object (needed for object handles)
{
    BOOL fSemOwned = FALSE;
    PSUBCLASSEDFOLDERVIEW psliNew = 0;

    psliNew = malloc(sizeof(SUBCLASSEDFOLDERVIEW));
    if (psliNew)
    {
        memset(psliNew, 0, sizeof(SUBCLASSEDFOLDERVIEW)); // V0.9.0

        if (hwndCnr == NULLHANDLE)
            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                   "hwndCnr is NULLHANDLE for folder %s.",
                   _wpQueryTitle(somSelf));

        // store various other data here
        psliNew->hwndFrame = hwndFrame;
        psliNew->somSelf = somSelf;
        psliNew->pRealObject = pRealObject;
        psliNew->hwndCnr = hwndCnr;
        // psliNew->ulView = ulView;
        psliNew->fRemoveSrcEmphasis = FALSE;
        // set status bar hwnd to zero at this point;
        // this will be created elsewhere
        psliNew->hwndStatusBar = NULLHANDLE;

        // create a supplementary object window
        // for this folder frame (see
        // fdr_fnwpSupplFolderObject for details)
        psliNew->hwndSupplObject
            = winhCreateObjectWindow(WNDCLASS_SUPPLOBJECT,
                                     psliNew);

        if (!psliNew->hwndSupplObject)
            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                   "Unable to create suppl. folder object window..");

        // store SFV in frame's window words
        WinSetWindowPtr(hwndFrame,
                        G_SFVOffset,        // window word offset which we've
                                            // calculated in fdr_SendMsgHook
                        psliNew);
        psliNew->pfnwpOriginal = WinSubclassWindow(hwndFrame,
                                                   fdr_fnwpSubclassedFolderFrame);
    }
    else
        cmnLog(__FILE__, __LINE__, __FUNCTION__,
               "psliNew is NULL (malloc failed).");

    return (psliNew);
}

/*
 *@@ fdrQuerySFV:
 *      this retrieves the PSUBCLASSEDFOLDERVIEW from the
 *      specified subclassed folder frame. One of these
 *      structs is maintained for each open folder view
 *      to store window data which is needed everywhere.
 *
 *      Returns NULL if not found.
 *
 *@@changed V0.9.0 [umoeller]: adjusted for new linklist functions
 *@@changed V0.9.0 [umoeller]: pulIndex added to function prototype
 *@@changed V0.9.0 [umoeller]: moved this func here from common.c
 *@@changed V0.9.1 (2000-02-14) [umoeller]: reversed order of functions; now subclassing is last
 *@@changed V0.9.3 (2000-04-08) [umoeller]: completely replaced
 */

PSUBCLASSEDFOLDERVIEW fdrQuerySFV(HWND hwndFrame,        // in: folder frame to find
                                  PULONG pulIndex)       // out: index in linked list if found
{
    PSUBCLASSEDFOLDERVIEW psliFound = (PSUBCLASSEDFOLDERVIEW)WinQueryWindowPtr(hwndFrame,
                                                                           G_SFVOffset);
    return (psliFound);
}

/*
 *@@ fdrRemoveSFV:
 *      reverse to fdrSubclassFolderView, this removes
 *      a PSUBCLASSEDFOLDERVIEW from the folder frame again.
 *      Called upon WM_CLOSE in folder frames.
 *
 *@@changed V0.9.0 [umoeller]: adjusted for new linklist functions
 *@@changed V0.9.0 [umoeller]: moved this func here from common.c
 *@@changed V0.9.3 (2000-04-08) [umoeller]: completely replaced
 */

VOID fdrRemoveSFV(PSUBCLASSEDFOLDERVIEW psfv)
{
    WinSetWindowPtr(psfv->hwndFrame,
                    G_SFVOffset,
                    NULL);
    free(psfv);
}

/*
 *@@ fdrManipulateNewView:
 *      this gets called from XFolder::wpOpen
 *      after a new Icon, Tree, or Details view
 *      has been successfully opened by the parent
 *      method (WPFolder::wpOpen).
 *
 *      This manipulates the view according to
 *      our needs (subclassing, sorting, full
 *      status bar, path in title etc.).
 *
 *      This is ONLY used for folders, not for
 *      WPDisk's. This calls fdrSubclassFolderView in turn.
 *
 *@@added V0.9.0 (99-11-27) [umoeller]
 *@@changed V0.9.1 (2000-02-08) [umoeller]: status bars were added even if globally disabled; fixed.
 *@@changed V0.9.3 (2000-04-08) [umoeller]: adjusted for new folder frame subclassing
 */

VOID fdrManipulateNewView(WPFolder *somSelf,        // in: folder with new view
                          HWND hwndNewFrame,        // in: new view (frame) of folder
                          ULONG ulView)             // in: OPEN_CONTENTS, OPEN_TREE, or OPEN_DETAILS
{
    PSUBCLASSEDFOLDERVIEW psfv = 0;
    PCGLOBALSETTINGS    pGlobalSettings = cmnQueryGlobalSettings();
    XFolderData         *somThis = XFolderGetData(somSelf);
    HWND                hwndCnr = wpshQueryCnrFromFrame(hwndNewFrame);

    if (pGlobalSettings->fNoSubclassing == FALSE) // V0.9.3 (2000-04-26) [umoeller]
    {
        // subclass the new folder frame window;
        // this creates a SUBCLASSEDFOLDERVIEW for the view
        psfv = fdrSubclassFolderView(hwndNewFrame,
                                     hwndCnr,
                                     somSelf,
                                     somSelf);  // "real" object; for folders, this is the folder too

        // change the window title to full path, if allowed
        if (    (_bFullPathInstance == 1)
             || ((_bFullPathInstance == 2) && (pGlobalSettings->FullPath))
           )
            fdrSetOneFrameWndTitle(somSelf, hwndNewFrame);

        // add status bar, if allowed:
        // 1) status bar only if allowed for the current folder
        if (   (pGlobalSettings->fEnableStatusBars)
            && (    (_bStatusBarInstance == STATUSBAR_ON)
                 || (   (_bStatusBarInstance == STATUSBAR_DEFAULT)
                     && (pGlobalSettings->fDefaultStatusBarVisibility)
                    )
               )
           )
            // 2) no status bar for active Desktop
            if (somSelf != cmnQueryActiveDesktop())
                // 3) check that subclassed list item is valid
                if (psfv)
                    // 4) status bar only if allowed for the current view type
                    if (    (   (ulView == OPEN_CONTENTS)
                             && (pGlobalSettings->SBForViews & SBV_ICON)
                            )
                         || (   (ulView == OPEN_TREE)
                             && (pGlobalSettings->SBForViews & SBV_TREE)
                            )
                         || (   (ulView == OPEN_DETAILS)
                             && (pGlobalSettings->SBForViews & SBV_DETAILS)
                            )
                        )
                        fdrCreateStatusBar(somSelf, psfv, TRUE);

        // replace sort stuff
        if (pGlobalSettings->ExtFolderSort)
        {
            if (hwndCnr)
                fdrSetFldrCnrSort(somSelf, hwndCnr, FALSE);
        }
    }
}

/* ******************************************************************
 *                                                                  *
 *   New subclassed folder frame message processing                 *
 *                                                                  *
 ********************************************************************/

/*
 * CalcFrameRect:
 *      this gets called from fdr_fnwpSubclassedFolderFrame
 *      when WM_CALCFRAMERECT is received. This implements
 *      folder status bars.
 *
 *@@changed V0.9.0 [umoeller]: moved this func here from xfldr.c
 */

VOID CalcFrameRect(MPARAM mp1, MPARAM mp2)
{
    PRECTL prclPassed = (PRECTL)mp1;
    ULONG ulStatusBarHeight = cmnQueryStatusBarHeight();

    if (SHORT1FROMMP(mp2))
        //     TRUE:  Frame rectangle provided, calculate client
        //     FALSE: Client area rectangle provided, calculate frame
    {
        //  TRUE: calculate the rectl of the client;
        //  call default window procedure to subtract child frame
        //  controls from the rectangle's height
        LONG lClientHeight;

        //  position the static text frame extension below the client
        lClientHeight = prclPassed->yTop - prclPassed->yBottom;
        if ( ulStatusBarHeight  > lClientHeight  )
        {
            // extension is taller than client, so set client height to 0
            prclPassed->yTop = prclPassed->yBottom;
        }
        else
        {
            //  set the origin of the client and shrink it based upon the
            //  static text control's height
            prclPassed->yBottom += ulStatusBarHeight;
            prclPassed->yTop -= ulStatusBarHeight;
        }
    }
    else
    {
        //  FALSE: calculate the rectl of the frame;
        //  call default window procedure to subtract child frame
        //  controls from the rectangle's height;
        //  set the origin of the frame and increase it based upon the
        //  static text control's height
        prclPassed->yBottom -= ulStatusBarHeight;
        prclPassed->yTop += ulStatusBarHeight;
    }
}

/*
 * FormatFrame:
 *      this gets called from fdr_fnwpSubclassedFolderFrame
 *      when WM_FORMATFRAME is received. This implements
 *      folder status bars.
 *
 *@@changed V0.9.0 [umoeller]: moved this func here from xfldr.c
 */

VOID FormatFrame(PSUBCLASSEDFOLDERVIEW psfv, // in: frame information
                 MPARAM mp1,            // in: mp1 from WM_FORMATFRAME (points to SWP array)
                 ULONG ulCount)         // in: frame control count (returned from default wnd proc)
{
    // access the SWP array that is passed to us
    // and search all the controls for the container child window,
    // which for folders always has the ID 0x8008
    ULONG       ul;
    PSWP        swpArr = (PSWP)mp1;
    CNRINFO     CnrInfo;

    for (ul = 0; ul < ulCount; ul++)
    {
        if (WinQueryWindowUShort( swpArr[ul].hwnd, QWS_ID ) == 0x8008 )
                                                         // FID_CLIENT
        {
            // container found: reduce size of container by
            // status bar height
            POINTL          ptlBorderSizes;
            // XFolderData     *somThis = XFolderGetData(psfv->somSelf);
            ULONG ulStatusBarHeight = cmnQueryStatusBarHeight();
            WinSendMsg(psfv->hwndFrame,
                       WM_QUERYBORDERSIZE,
                       (MPARAM)&ptlBorderSizes,
                       0);

            // first initialize the _new_ SWP for the status bar.
            // Since the SWP array for the std frame controls is
            // zero-based, and the standard frame controls occupy
            // indices 0 thru ulCount-1 (where ulCount is the total
            // count), we use ulCount for our static text control.
            swpArr[ulCount].fl = SWP_MOVE | SWP_SIZE | SWP_NOADJUST | SWP_ZORDER;
            swpArr[ulCount].x  = ptlBorderSizes.x;
            swpArr[ulCount].y  = ptlBorderSizes.y;
            swpArr[ulCount].cx = swpArr[ul].cx;  // same as cnr's width
            swpArr[ulCount].cy = ulStatusBarHeight;
            swpArr[ulCount].hwndInsertBehind = HWND_BOTTOM; // HWND_TOP;
            swpArr[ulCount].hwnd = psfv->hwndStatusBar;

            // adjust the origin and height of the container to
            // accomodate our static text control
            swpArr[ul].y  += swpArr[ulCount].cy;
            swpArr[ul].cy -= swpArr[ulCount].cy;

            // now we need to adjust the workspace origin of the cnr
            // accordingly, or otherwise the folder icons will appear
            // outside the visible cnr workspace and scroll bars will
            // show up.
            // We only do this the first time we're arriving here
            // (which should be before the WPS is populating the folder);
            // psfv->fNeedCnrScroll has been initially set to TRUE
            // by fdrCreateStatusBar.
            if (psfv->fNeedCnrScroll)
            {
                cnrhQueryCnrInfo(swpArr[ul].hwnd, &CnrInfo);

                #ifdef DEBUG_STATUSBARS
                    _Pmpf(( "Old CnrInfo.ptlOrigin.y: %lX", CnrInfo.ptlOrigin.y ));
                #endif

                if (    ((LONG)CnrInfo.ptlOrigin.y >= (LONG)ulStatusBarHeight)
                   )
                {
                    CnrInfo.ptlOrigin.y -= ulStatusBarHeight;

                    #ifdef DEBUG_STATUSBARS
                        _Pmpf(( "New CnrInfo.ptlOrigin.y: %lX", CnrInfo.ptlOrigin.y ));
                    #endif

                    WinSendMsg(swpArr[ul].hwnd,
                               CM_SETCNRINFO,
                               (MPARAM)&CnrInfo,
                               (MPARAM)CMA_PTLORIGIN);
                    // set flag to FALSE to prevent a second adjustment
                    psfv->fNeedCnrScroll = FALSE;
                }
            } // end if (psfv->fNeedCnrScroll)

            break;  // we're done
        } // end if WinQueryWindowUShort
    } // end for (ul = 0; ul < ulCount; ul++)
}

/*
 * InitMenu:
 *      this gets called from fdr_fnwpSubclassedFolderFrame
 *      when WM_INITMENU is received, _after_ the parent
 *      window proc has been given a chance to process this.
 *
 *      WM_INITMENU is sent to a menu owner right before a
 *      menu is about to be displayed. This applies to both
 *      pulldown and context menus.
 *
 *      This is needed for various menu features:
 *
 *      1)  for getting the object which currently has source
 *          emphasis in the folder container, because at the
 *          point WM_COMMAND comes in (and thus wpMenuItemSelected
 *          gets called in response), source emphasis has already
 *          been removed by the WPS.
 *
 *          This is needed for file operations on all selected
 *          objects in the container. We call wpshQuerySourceObject
 *          to find out more about this and store the result in
 *          our SUBCLASSEDFOLDERVIEW.
 *
 *      2)  for folder content menus, because these are
 *          inserted as empty stubs only in wpModifyPopupMenu
 *          and only filled after the user clicks on them.
 *          We will query a bunch of data first, which we need
 *          later for drawing our items, and then call
 *          mnuFillContentSubmenu, which populates the folder
 *          and fills the menu with the items therein.
 *
 *      3)  for the menu system sounds.
 *
 *      4)  for manipulating Warp 4 folder menu _bars_. We
 *          cannot use the Warp 4 WPS methods defined for
 *          that because we want XWorkplace to run on Warp 3
 *          also.
 *
 *      WM_INITMENU parameters:
 *          SHORT mp1   menu item id
 *          HWND  mp2   menu window handle
 *      Returns: NULL always.
 *
 *@@changed V0.9.0 [umoeller]: moved this func here from xfldr.c
 */

VOID InitMenu(PSUBCLASSEDFOLDERVIEW psfv, // in: frame information
              ULONG sMenuIDMsg,         // in: mp1 from WM_INITMENU
              HWND hwndMenuMsg)         // in: mp2 from WM_INITMENU
{
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    // get XFolder instance data
    XFolderData     *somThis = XFolderGetData(psfv->somSelf);

    #ifdef DEBUG_MENUS
        _Pmpf(( "WM_INITMENU: sMenuIDMsg = %lX, hwndMenuMsg = %lX",
                (ULONG)sMenuIDMsg,
                hwndMenuMsg ));
        _Pmpf(( "  psfv->hwndCnr: 0x%lX", psfv->hwndCnr));
    #endif

    // store object with source emphasis for later use;
    // this gets lost before WM_COMMAND otherwise
    psfv->pSourceObject = wpshQuerySourceObject(psfv->somSelf,
                                                psfv->hwndCnr,
                                                FALSE,      // menu mode
                                                &psfv->ulSelection);

    // store the container window handle in instance
    // data for wpModifyPopupMenu workaround;
    // buggy Warp 3 keeps setting hwndCnr to NULLHANDLE in there
    _hwndCnrSaved = psfv->hwndCnr;

    // play system sound
    if (    (sMenuIDMsg < 0x8000) // avoid system menu
         || (sMenuIDMsg == 0x8020) // but include context menu
       )
        cmnPlaySystemSound(MMSOUND_XFLD_CTXTOPEN);

    // find out whether the menu of which we are notified
    // is a folder content menu; if so (and it is not filled
    // yet), the first menu item is ID_XFMI_OFS_DUMMY
    if ((ULONG)WinSendMsg(hwndMenuMsg,
                          MM_ITEMIDFROMPOSITION,
                          (MPARAM)0,        // menu item index
                          MPNULL)
               == (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_DUMMY))
    {
        // okay, let's go
        if (pGlobalSettings->FCShowIcons)
        {
            // show folder content icons ON:

            #ifdef DEBUG_MENUS
                _Pmpf(( "  preparing owner draw"));
            #endif

            mnuPrepareOwnerDraw(sMenuIDMsg, hwndMenuMsg);
        }

        // add menu items according to folder contents
        mnuFillContentSubmenu(sMenuIDMsg, hwndMenuMsg,
                              // this func subclasses the folder content
                              // menu wnd and stores the result here:
                              &G_pfnwpFolderContentMenuOriginal);
    }
    else
    {
        // no folder content menu:

        // on Warp 4, check if the folder has a menu bar
        if (doshIsWarp4())
        {
            #ifdef DEBUG_MENUS
                _Pmpf(( "  checking for menu bar"));
            #endif

            if (sMenuIDMsg == 0x8005)
            {
                // seems to be some WPS menu item;
                // since the WPS seems to be using this
                // same ID for all the menu bar submenus,
                // we need to check the last selected
                // menu item, which was stored in the psfv
                // structure by WM_MENUSELECT (below).
                PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();

                switch (psfv->ulLastSelMenuItem)
                {
                    case 0x2D0: // "Edit" submenu
                    {
                        // find position of "Deselect all" item
                        SHORT sPos = (SHORT)WinSendMsg(hwndMenuMsg,
                                                       MM_ITEMPOSITIONFROMID,
                                                       MPFROM2SHORT(0x73,
                                                                    FALSE),
                                                       MPNULL);
                        #ifdef DEBUG_MENUS
                            _Pmpf(("  'Edit' menu found"));
                        #endif
                        // insert "Select by name" after that item
                        winhInsertMenuItem(hwndMenuMsg,
                                           sPos+1,
                                           (pGlobalSettings->VarMenuOffset
                                                   + ID_XFMI_OFS_SELECTSOME),
                                           pNLSStrings->pszSelectSome,
                                           MIS_TEXT, 0);
                    break; }

                    case 0x2D1: // "View" submenu
                    {
                        CNRINFO             CnrInfo;
                        #ifdef DEBUG_MENUS
                            _Pmpf(("  'View' menu found"));
                        #endif
                        // modify the "Sort" menu, as we would
                        // do it for context menus also
                        mnuModifySortMenu(psfv->somSelf,
                                          hwndMenuMsg,
                                          pGlobalSettings,
                                          pNLSStrings);
                        cnrhQueryCnrInfo(psfv->hwndCnr, &CnrInfo);
                        // and now insert the "folder view" items
                        winhInsertMenuSeparator(hwndMenuMsg,
                                                MIT_END,
                                                (pGlobalSettings->VarMenuOffset
                                                        + ID_XFMI_OFS_SEPARATOR));
                        mnuInsertFldrViewItems(psfv->somSelf,
                                               hwndMenuMsg,  // hwndViewSubmenu
                                               FALSE,
                                               psfv->hwndCnr,
                                               wpshQueryView(psfv->somSelf,
                                                             psfv->hwndFrame));
                                               // psfv->ulView);
                                               // wpshQueryView(psfv->somSelf,
                                                  //            psfv->hwndFrame));
                    break; }

                    case 0x2D2:     // "Selected" submenu:
                    {
                    break; }

                    case 0x2D3: // "Help" submenu: add XFolder product info
                        #ifdef DEBUG_MENUS
                            _Pmpf(("  'Help' menu found"));
                        #endif
                        winhInsertMenuSeparator(hwndMenuMsg, MIT_END,
                                               (pGlobalSettings->VarMenuOffset
                                                       + ID_XFMI_OFS_SEPARATOR));
                        winhInsertMenuItem(hwndMenuMsg, MIT_END,
                                           (pGlobalSettings->VarMenuOffset
                                                   + ID_XFMI_OFS_PRODINFO),
                                           pNLSStrings->pszProductInfo,
                                           MIS_TEXT, 0);
                    break;

                } // end switch (psfv->usLastSelMenuItem)
            } // end if (SHORT1FROMMP(mp1) == 0x8005)
        } // end if (doshIsWarp4())
    }
}

/*
 * MenuSelect:
 *      this gets called from fdr_fnwpSubclassedFolderFrame
 *      when WM_MENUSELECT is received.
 *      We need this for three reasons:
 *
 *      1) we will play a system sound, if desired;
 *
 *      2) we need to swallow this for very large folder
 *         content menus, because for some reason, PM will
 *         select a random menu item after we have repositioned
 *         a menu window on the screen (duh);
 *
 *      3) we can intercept certain menu items so that
 *         these don't get passed to wpMenuItemSelected,
 *         which appears to get called when the WPS folder window
 *         procedure responds to WM_COMMAND (which comes after
 *         WM_MENUSELECT only).
 *         This is needed for menu items such as those in
 *         the "Sort" menu so that the menu is not dismissed
 *         after selection.
 *
 *      WM_MENUSELECT parameters:
 *      --  mp1 -- USHORT usItem - selected menu item
 *              -- USHORT usPostCommand - TRUE: if we return TRUE,
 *                  a message will be posted to the owner.
 *      --  mp2 HWND - menu control wnd handle
 *
 *      If we set pfDismiss to TRUE, wpMenuItemSelected will be
 *      called, and the menu will be dismissed.
 *      Otherwise the message will be swallowed.
 *      We return TRUE if the menu item has been handled here.
 *      Otherwise the default wnd proc will be used.
 *
 *@@changed V0.9.0 [umoeller]: moved this func here from xfldr.c
 */

BOOL MenuSelect(PSUBCLASSEDFOLDERVIEW psfv, // in: frame information
                MPARAM mp1,               // in: mp1 from WM_MENUSELECT
                MPARAM mp2,               // in: mp2 from WM_MENUSELECT
                BOOL *pfDismiss)          // out: dismissal flag
{
    BOOL fHandled = FALSE;
    // return value for WM_MENUSELECT;
    // TRUE means dismiss menu

    psfv->ulLastSelMenuItem = SHORT1FROMMP(mp1);

    // check if we have moved a folder content menu
    // (this flag is set by fdr_fnwpSubclFolderContentMenu); for
    // some reason, PM gets confused with the menu items
    // then and automatically tries to select the menu
    // item under the mouse, so we swallow this one
    // message if (a) the folder content menu has been
    // moved by fdr_fnwpSubclFolderContentMenu and (b) no mouse
    // button is pressed. These flags are all set by
    // fdr_fnwpSubclFolderContentMenu.
    if (G_fFldrContentMenuMoved)
    {
        #ifdef DEBUG_MENUS
            _Pmpf(( "  FCMenuMoved set!"));
        #endif
        if (!G_fFldrContentMenuButtonDown)
        {
            // mouse was not pressed: swallow this
            // menu selection
            *pfDismiss = FALSE;
        }
        else
        {
            // mouse was pressed: allow selection
            // and unset flags
            *pfDismiss = TRUE;
            G_fFldrContentMenuButtonDown = FALSE;
            G_fFldrContentMenuMoved = FALSE;
        }
        fHandled = TRUE;
    }
    else
    {
        USHORT      usItem = SHORT1FROMMP(mp1),
                    usPostCommand = SHORT2FROMMP(mp1);

        if (    (usPostCommand)
            && (    (usItem <  0x8000) // avoid system menu
                 || (usItem == 0x8020) // include context menu
               )
           )
        {
            HWND hwndCnr = wpshQueryCnrFromFrame(psfv->hwndFrame);

            // play system sound
            cmnPlaySystemSound(MMSOUND_XFLD_CTXTSELECT);

            // Now check if we have a menu item which we don't
            // want to see dismissed.

            if (hwndCnr)
            {
                // first find out what kind of objects we have here
                /* ULONG ulSelection = 0;
                WPObject *pObject1 =
                     wpshQuerySourceObject(psfv->somSelf,
                                           hwndCnr,
                                           &ulSelection); */

                WPObject *pObject = psfv->pSourceObject;

                #ifdef DEBUG_MENUS
                    _Pmpf(( "  Object selections: %d", ulSelection));
                #endif

                // dereference shadows
                if (pObject)
                    if (_somIsA(pObject, _WPShadow))
                        pObject = _wpQueryShadowedObject(pObject, TRUE);

                // now call the functions in menus.c for this,
                // depending on the class of the object for which
                // the menu was opened
                if (pObject)
                {
                    if (_somIsA(pObject, _WPFileSystem))
                    {
                        fHandled = mnuFileSystemSelectingMenuItem(
                                       psfv->pSourceObject,
                                            // note that we're passing
                                            // psfv->pSourceObject instead of pObject;
                                            // psfv->pSourceObject might be a shadow!
                                       usItem,
                                       (BOOL)usPostCommand,
                                       (HWND)mp2,               // hwndMenu
                                       hwndCnr,
                                       psfv->ulSelection,       // SEL_* flags
                                       pfDismiss);              // dismiss-menu flag

                        if (    (!fHandled)
                             && (_somIsA(pObject, _WPFolder))
                           )
                        {
                            fHandled = mnuFolderSelectingMenuItem(pObject,
                                           usItem,
                                           (BOOL)usPostCommand, // fPostCommand
                                           (HWND)mp2,               // hwndMenu
                                           hwndCnr,
                                           psfv->ulSelection,       // SEL_* flags
                                           pfDismiss);              // dismiss-menu flag
                        }
                    }

                    if (    (fHandled)
                         && (!(*pfDismiss))
                       )
                    {
                        // menu not to be dismissed: set the flag
                        // which will remove cnr source
                        // emphasis when the menu is dismissed
                        // later (WM_ENDMENU msg here)
                        psfv->fRemoveSrcEmphasis = TRUE;
                    }
                }
            }
        }
    }

    return (fHandled);
}

/*
 *@@ WMChar_Delete:
 *      this gets called if "delete into trash can"
 *      is enabled and WM_CHAR has been detected with
 *      the "Delete" key. We start a file task job
 *      to delete all selected objects in the container
 *      into the trash can, using the oh-so-much advanced
 *      functions in fileops.c.
 *
 *@@added V0.9.1 (2000-01-31) [umoeller]
 */

VOID WMChar_Delete(PSUBCLASSEDFOLDERVIEW psfv)
{
    ULONG   ulSelection = 0;
    WPObject *pSelected = wpshQuerySourceObject(psfv->somSelf,
                                                psfv->hwndCnr,
                                                TRUE,       // keyboard mode
                                                &ulSelection);
    #ifdef DEBUG_TRASHCAN
        _Pmpf(("WM_CHAR delete: first obj is %s", _wpQueryTitle(pSelected)));
    #endif

    if (    (pSelected)
         && (ulSelection != SEL_NONEATALL)
       )
    {
        // collect objects from cnr and start
        // moving them to trash can
        FOPSRET frc = fopsStartTrashDeleteFromCnr(psfv->somSelf, // source folder
                                                  pSelected,    // first selected object
                                                  ulSelection,  // can only be SEL_SINGLESEL
                                                                 // or SEL_MULTISEL
                                                  psfv->hwndCnr,
                                                  FALSE);   // move to trash can
        #ifdef DEBUG_TRASHCAN
            _Pmpf(("    got FOPSRET %d", frc));
        #endif
    }
}

/*
 *@@ ProcessFolderMsgs:
 *
 *@@added V0.9.3 (2000-04-08) [umoeller]
 */

MRESULT ProcessFolderMsgs(HWND hwndFrame,
                          ULONG msg,
                          MPARAM mp1,
                          MPARAM mp2,
                          PSUBCLASSEDFOLDERVIEW psfv,  // in: folder view data
                          PFNWP pfnwpOriginal)       // in: original frame window proc
{
    MRESULT mrc = 0;
    BOOL            fCallDefault = FALSE;

    TRY_LOUD(excpt1, NULL)
    {
        switch(msg)
        {
            /* *************************
             *                         *
             * Status bar:             *
             *                         *
             **************************/

            /*
             *  The following code adds status bars to folder frames.
             *  The XFolder status bars are implemented as frame controls
             *  (similar to the title-bar buttons and menus). In order
             *  to do this, we need to intercept the following messages
             *  which are sent to the folder's frame window when the
             *  frame is updated as a reaction to WM_UPDATEFRAME or WM_SIZE.
             *
             *  Note that wpOpen has created the status bar window (which
             *  is a static control subclassed with fdr_fnwpStatusBar) already
             *  and stored the HWND in the SUBCLASSEDFOLDERVIEW.hwndStatusBar
             *  structure member (which psfv points to now).
             *
             *  Here we only relate the status bar to the frame. The actual
             *  painting etc. is done in fdr_fnwpStatusBar.
             */

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
                ULONG ulrc = (ULONG)((*pfnwpOriginal)(hwndFrame, msg, mp1, mp2));

                // if we have a status bar, increment the count
                if (psfv->hwndStatusBar)
                    ulrc++;

                mrc = (MPARAM)ulrc;
            break; }

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
                ULONG ulCount = (ULONG)((*pfnwpOriginal)(hwndFrame, msg, mp1, mp2));

                #ifdef DEBUG_STATUSBARS
                    _Pmpf(( "WM_FORMATFRAME ulCount = %d", ulCount ));
                #endif

                if (psfv->hwndStatusBar)
                {
                    // we have a status bar:
                    // format the frame
                    FormatFrame(psfv, mp1, ulCount);

                    // increment the number of frame controls
                    // to include our status bar
                    mrc = (MRESULT)(ulCount + 1);
                } // end if (psfv->hwndStatusBar)
                else
                    // no status bar:
                    mrc = (MRESULT)ulCount;
            break; }

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
            {
                mrc = (*pfnwpOriginal)(hwndFrame, msg, mp1, mp2);

                if (psfv->hwndStatusBar)
                    // we have a status bar: calculate its rectangle
                    CalcFrameRect(mp1, mp2);
            break; }

            /* *************************
             *                         *
             * Menu items:             *
             *                         *
             **************************/

            /*
             * WM_INITMENU:
             *      this message is sent to a frame whenever a menu
             *      is about to be displayed. This is needed for
             *      various menu features; see InitMenu() above.
             */

            case WM_INITMENU:
            {
                PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

                // call the default, in case someone else
                // is subclassing folders (ObjectDesktop?!?);
                // from what I've checked, the WPS does NOTHING
                // with this message, not even for menu bars...
                mrc = (MRESULT)(*pfnwpOriginal)(hwndFrame, msg, mp1, mp2);

                if (pGlobalSettings->fNoFreakyMenus == FALSE)
                    // added V0.9.3 (2000-03-28) [umoeller]
                    InitMenu(psfv,
                             (ULONG)mp1,
                             (HWND)mp2);
            break; }

            /*
             * WM_MENUSELECT:
             *      this is SENT to a menu owner by the menu
             *      control to determine what to do right after
             *      a menu item has been selected. If we return
             *      TRUE, the menu will be dismissed.
             *
             *      See MenuSelect() above.
             */

            case WM_MENUSELECT:
            {
                PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
                BOOL fDismiss = TRUE;

                #ifdef DEBUG_MENUS
                    _Pmpf(( "WM_MENUSELECT: mp1 = %lX/%lX, mp2 = %lX",
                            SHORT1FROMMP(mp1),
                            SHORT2FROMMP(mp1),
                            mp2 ));
                #endif

                // always call the default, in case someone else
                // is subclassing folders (ObjectDesktop?!?)
                mrc = (MRESULT)(*pfnwpOriginal)(hwndFrame, msg, mp1, mp2);

                if (pGlobalSettings->fNoFreakyMenus == FALSE)
                    // added V0.9.3 (2000-03-28) [umoeller]
                    // now handle our stuff; this might modify mrc to
                    // have the menu stay on the screen
                    if (MenuSelect(psfv, mp1, mp2, &fDismiss))
                        // processed: return the modified flag instead
                        mrc = (MRESULT)fDismiss;
            break; }

            /*
             * WM_MENUEND:
             *      this message occurs when a menu control is about to
             *      terminate. We need to remove cnr source emphasis
             *      if the user has requested a context menu from a
             *      status bar.
             */

            case WM_MENUEND:
            {
                PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
                #ifdef DEBUG_MENUS
                    _Pmpf(( "WM_MENUEND: mp1 = %lX, mp2 = %lX",
                            mp1, mp2 ));
                    /* _Pmpf(( "  fFolderContentWindowPosChanged: %d",
                            fFolderContentWindowPosChanged));
                    _Pmpf(( "  fFolderContentButtonDown: %d",
                            fFolderContentButtonDown)); */
                #endif

                if (pGlobalSettings->fNoFreakyMenus == FALSE)
                {
                    // added V0.9.3 (2000-03-28) [umoeller]

                    // menu opened from status bar?
                    if (psfv->fRemoveSrcEmphasis)
                    {
                        // if so, remove cnr source emphasis
                        WinSendMsg(psfv->hwndCnr,
                                   CM_SETRECORDEMPHASIS,
                                   (MPARAM)NULL,   // undocumented: if precc == NULL,
                                                   // the whole cnr is given emphasis
                                   MPFROM2SHORT(FALSE,  // remove emphasis
                                           CRA_SOURCE));
                        // and make sure the container has the
                        // focus
                        WinSetFocus(HWND_DESKTOP, psfv->hwndCnr);
                        // reset flag for next context menu
                        psfv->fRemoveSrcEmphasis = FALSE;
                    }

                    // unset flag for WM_MENUSELECT above
                    G_fFldrContentMenuMoved = FALSE;
                }

                mrc = (MRESULT)(*pfnwpOriginal)(hwndFrame, msg, mp1, mp2);
            break; }

            /*
             * WM_MEASUREITEM:
             *      this msg is sent only once per owner-draw item when
             *      PM needs to know its size. This gets sent to us for
             *      items in folder content menus (if icons are on); the
             *      height of our items will be the same as with
             *      non-owner-draw ones, but we need to calculate the width
             *      according to the item text.
             *
             *      Return value: check mnuMeasureItem (menus.c)
             */

            case WM_MEASUREITEM:
            {
                PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
                if ( (SHORT)mp1 > (pGlobalSettings->VarMenuOffset+ID_XFMI_OFS_VARIABLE) )
                {
                    // call the measure-item func in menus.c
                    mrc = mnuMeasureItem((POWNERITEM)mp2, pGlobalSettings);
                }
                else
                    // none of our items: pass to original wnd proc
                    mrc = (MRESULT)(*pfnwpOriginal)(hwndFrame, msg, mp1, mp2);
            break; }

            /*
             * WM_DRAWITEM:
             *      this msg is sent for each item every time it
             *      needs to be redrawn. This gets sent to us for
             *      items in folder content menus (if icons are on).
             *
             *      ### Warning: Apparently we also get this for
             *      the WPS's container owner draw. We should add
             *      another check... V0.9.3 (2000-04-26) [umoeller]
             */

            case WM_DRAWITEM:
            {
                PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
                if ( (SHORT)mp1 > (pGlobalSettings->VarMenuOffset+ID_XFMI_OFS_VARIABLE) )
                {
                    // variable menu item: this must be a folder-content
                    // menu item, because for others no WM_DRAWITEM is sent
                    // (menus.c)
                    if (mnuDrawItem(pGlobalSettings,
                                    mp1, mp2))
                        mrc = (MRESULT)TRUE;
                    else // error occured:
                        fCallDefault = TRUE;    // V0.9.3 (2000-04-26) [umoeller]
                        // mrc = (MRESULT)(*pfnwpOriginal)(hwndFrame, msg, mp1, mp2);
                }
                else
                    fCallDefault = TRUE;    // V0.9.3 (2000-04-26) [umoeller]
                    // mrc = (MRESULT)(*pfnwpOriginal)(hwndFrame, msg, mp1, mp2);
            break; }

            /* *************************
             *                         *
             * Miscellaneae:           *
             *                         *
             **************************/

            /*
             * WM_COMMAND:
             *      this is intercepted to provide "delete" menu
             *      item support if "delete into trashcan"
             *      is on. We cannot use wpMenuItemSelected for
             *      that because that method gets called for
             *      every object, and we'd never know when it's
             *      called for the last object. So we do this
             *      here instead, and wpMenuItemSelected never
             *      gets called.
             */

            case WM_COMMAND:
            {
                USHORT usCommand = SHORT1FROMMP(mp1);

                if (usCommand == WPMENUID_DELETE)
                {
                    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
                    BOOL fCallXWPFops = FALSE;
                    BOOL fTrueDelete = FALSE;

                    if (pGlobalSettings->fTrashDelete)
                    {
                        // delete to trash can enabled:
                        if (doshQueryShiftState())
                            // shift pressed also:
                            fTrueDelete = TRUE;
                        else
                            // delete to trash can:
                            fCallXWPFops = TRUE;
                    }
                    else
                        // no delete to trash can:
                        // do a real delete
                        fTrueDelete = TRUE;

                    if (fTrueDelete)
                        // real delete:
                        // is real delete also replaced?
                        if (pGlobalSettings->fReplaceTrueDelete)
                            fCallXWPFops = TRUE;

                    if (fCallXWPFops)
                    {
                        // this is TRUE if
                        // -- delete to trash can has been enabled and regular delete
                        //    has been selected
                        // -- delete to trash can has been enabled, but Shift was pressed
                        // -- delete to trash can has been disabled, but regular delete
                        //    was replaced

                        // collect objects from container and start deleting
                        FOPSRET frc = fopsStartTrashDeleteFromCnr(psfv->somSelf,
                                                                    // source folder
                                                                  psfv->pSourceObject,
                                                                    // first source object
                                                                  psfv->ulSelection,
                                                                  psfv->hwndCnr,
                                                                  fTrueDelete);
                        #ifdef DEBUG_TRASHCAN
                            _Pmpf(("WM_COMMAND WPMENUID_DELETE: got FOPSRET %d", frc));
                        #endif
                        break;
                    }
                }

                fCallDefault = TRUE;

            break; }

            /*
             * WM_CHAR:
             *      this is intercepted to provide folder hotkeys
             *      and "Del" key support if "delete into trashcan"
             *      is on.
             */

            case WM_CHAR:
            {
                USHORT usFlags    = SHORT1FROMMP(mp1);
                // whatever happens, we're only interested
                // in key-down messages
                if ((usFlags & KC_KEYUP) == 0)
                {
                    XFolderData         *somThis = XFolderGetData(psfv->somSelf);
                    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

                    USHORT usch       = SHORT1FROMMP(mp2);
                    USHORT usvk       = SHORT2FROMMP(mp2);

                    // check whether "delete to trash can" is on
                    if (pGlobalSettings->fTrashDelete)
                    {
                        // yes: intercept "Del" key
                        if (usFlags & KC_VIRTUALKEY)
                            if (usvk == VK_DELETE)
                            {
                                WMChar_Delete(psfv);
                                // swallow this key,
                                // do not process default winproc
                                break;
                            }
                    }

                    // check whether folder hotkeys are allowed at all
                    if (    (pGlobalSettings->fEnableFolderHotkeys)
                        // yes: check folder and global settings
                         && (   (_bFolderHotkeysInstance == 1)
                            ||  (   (_bFolderHotkeysInstance == 2)   // use global settings:
                                 && (pGlobalSettings->fFolderHotkeysDefault)
                                )
                            )
                       )
                    {
                        if (fdrProcessFldrHotkey(hwndFrame,
                                                 usFlags,
                                                 usch,
                                                 usvk))
                        {
                            // was a hotkey:
                            mrc = (MRESULT)TRUE;
                            // swallow this key,
                            // do not process default winproc
                            break;
                        }
                    }
                }

                fCallDefault = TRUE;
            break; }

            /*
             * WM_CONTROL:
             *      this is intercepted to check for container
             *      notifications we might be interested in.
             */

            case WM_CONTROL:
            {
                if (SHORT1FROMMP(mp1) /* id */ == 0x8008) // container!!
                {
                    #ifdef DEBUG_CNRCNTRL
                        CHAR szTemp2[30];
                        sprintf(szTemp2, "unknown: %d", SHORT2FROMMP(mp1));
                        _Pmpf(("Cnr cntrl msg: %s, mp2: %lX",
                            (SHORT2FROMMP(mp1) == CN_BEGINEDIT) ? "CN_BEGINEDIT"
                                : (SHORT2FROMMP(mp1) == CN_COLLAPSETREE) ? "CN_COLLAPSETREE"
                                : (SHORT2FROMMP(mp1) == CN_CONTEXTMENU) ? "CN_CONTEXTMENU"
                                : (SHORT2FROMMP(mp1) == CN_DRAGAFTER) ? "CN_DRAGAFTER"
                                : (SHORT2FROMMP(mp1) == CN_DRAGLEAVE) ? "CN_DRAGLEAVE"
                                : (SHORT2FROMMP(mp1) == CN_DRAGOVER) ? "CN_DRAGOVER"
                                : (SHORT2FROMMP(mp1) == CN_DROP) ? "CN_DROP"
                                : (SHORT2FROMMP(mp1) == CN_DROPNOTIFY) ? "CN_DROPNOTIFY"
                                : (SHORT2FROMMP(mp1) == CN_DROPHELP) ? "CN_DROPHELP"
                                : (SHORT2FROMMP(mp1) == CN_EMPHASIS) ? "CN_EMPHASIS"
                                : (SHORT2FROMMP(mp1) == CN_ENDEDIT) ? "CN_ENDEDIT"
                                : (SHORT2FROMMP(mp1) == CN_ENTER) ? "CN_ENTER"
                                : (SHORT2FROMMP(mp1) == CN_EXPANDTREE) ? "CN_EXPANDTREE"
                                : (SHORT2FROMMP(mp1) == CN_HELP) ? "CN_HELP"
                                : (SHORT2FROMMP(mp1) == CN_INITDRAG) ? "CN_INITDRAG"
                                : (SHORT2FROMMP(mp1) == CN_KILLFOCUS) ? "CN_KILLFOCUS"
                                : (SHORT2FROMMP(mp1) == CN_PICKUP) ? "CN_PICKUP"
                                : (SHORT2FROMMP(mp1) == CN_QUERYDELTA) ? "CN_QUERYDELTA"
                                : (SHORT2FROMMP(mp1) == CN_REALLOCPSZ) ? "CN_REALLOCPSZ"
                                : (SHORT2FROMMP(mp1) == CN_SCROLL) ? "CN_SCROLL"
                                : (SHORT2FROMMP(mp1) == CN_SETFOCUS) ? "CN_SETFOCUS"
                                : szTemp2,
                            mp2));
                    #endif

                    switch (SHORT2FROMMP(mp1))      // usNotifyCode
                    {
                        /*
                         * CN_BEGINEDIT:
                         *      this is sent by the container control
                         *      when direct text editing is about to
                         *      begin, that is, when the user alt-clicks
                         *      on an object title.
                         *      We'll select the file stem of the object.
                         */

                        /* case CN_BEGINEDIT: {
                            PCNREDITDATA pced = (PCNREDITDATA)mp2;
                            mrc = (MRESULT)(*pfnwpOriginal)(hwndFrame, msg, mp1, mp2);
                            if (pced) {
                                PMINIRECORDCORE pmrc = (PMINIRECORDCORE)pced->pRecord;
                                if (pmrc) {
                                    // editing WPS record core, not title etc.:
                                    // get the window ID of the MLE control
                                    // in the cnr window
                                    HWND hwndMLE = WinWindowFromID(pced->hwndCnr,
                                                        CID_MLE);
                                    if (hwndMLE) {
                                        ULONG cbText = WinQueryWindowTextLength(
                                                                hwndMLE)+1;
                                        PSZ pszText = malloc(cbText);
                                        _Pmpf(("textlen: %d", cbText));
                                        if (WinQueryWindowText(hwndMLE,
                                                               cbText,
                                                               pszText))
                                        {
                                            PSZ pszLastDot = strrchr(pszText, '.');
                                            _Pmpf(("text: %s", pszText));
                                            WinSendMsg(hwndMLE,
                                                    EM_SETSEL,
                                                    MPFROM2SHORT(
                                                        // first char: 0
                                                        0,
                                                        // last char:
                                                        (pszLastDot)
                                                            ? (pszLastDot-pszText)
                                                            : 10000
                                                    ), MPNULL);
                                        }
                                        free(pszText);
                                    }
                                }
                            }
                        break; } */

                        /*
                         * CN_ENTER:
                         *      double-click or enter key:
                         *      play sound
                         */

                        case CN_ENTER:
                            cmnPlaySystemSound(MMSOUND_XFLD_CNRDBLCLK);
                            mrc = (MRESULT)(*pfnwpOriginal)(hwndFrame, msg, mp1, mp2);
                        break;

                        /*
                         * CN_EMPHASIS:
                         *      selection changed:
                         *      update status bar
                         */

                        case CN_EMPHASIS:
                            mrc = (MRESULT)(*pfnwpOriginal)(hwndFrame, msg, mp1, mp2);
                            if (psfv->hwndStatusBar)
                            {
                                #ifdef DEBUG_STATUSBARS
                                    _Pmpf(( "CN_EMPHASIS: posting PM_UPDATESTATUSBAR to hwnd %lX", psfv->hwndStatusBar ));
                                #endif
                                WinPostMsg(psfv->hwndStatusBar,
                                           STBM_UPDATESTATUSBAR,
                                           MPNULL,
                                           MPNULL);
                            }
                        break;

                        /*
                         * CN_EXPANDTREE:
                         *      tree view has been expanded:
                         *      do cnr auto-scroll in File thread
                         */

                        case CN_EXPANDTREE:
                        {
                            PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
                            mrc = (MRESULT)(*pfnwpOriginal)(hwndFrame, msg, mp1, mp2);
                            if (pGlobalSettings->TreeViewAutoScroll)
                                xthrPostFileMsg(FIM_TREEVIEWAUTOSCROLL,
                                                (MPARAM)hwndFrame,
                                                mp2); // PMINIRECORDCORE
                        break; }

                        default:
                            fCallDefault = TRUE;
                        break;
                    } // end switch (SHORT2FROMMP(mp1))      // usNotifyCode
                }
            break; }

            /*
             * WM_DESTROY:
             *      upon this, we need to clean up our linked list
             *      of subclassed windows
             */

            case WM_DESTROY:
            {
                // destroy the supplementary object window for this folder
                // frame window; do this first because this references
                // the SFV
                WinDestroyWindow(psfv->hwndSupplObject);

                // upon closing the window, undo the subclassing, in case
                // some other message still comes in
                // (there are usually still two more, even after WM_DESTROY!!)
                WinSubclassWindow(hwndFrame, pfnwpOriginal);

                // and remove this window from our subclassing linked list
                fdrRemoveSFV(psfv);

                // do the default stuff
                fCallDefault = TRUE;
            break; }

            default:
                fCallDefault = TRUE;
            break;

        } // end switch
    } // end TRY_LOUD
    CATCH(excpt1)
    {
        // exception occured:
        return (0);
    } END_CATCH();

    if (fCallDefault)
    {
        // this has only been set to TRUE for "default" in
        // the switch statement above; we then call the
        // default window procedure.
        // This is either the original folder frame window proc
        // of the WPS itself or maybe the one of other WPS enhancers
        // which have subclassed folder windows (ObjectDesktop
        // and the like).
        // We do this outside the TRY/CATCH stuff above so that
        // we don't get blamed for exceptions which we are not
        // responsible for, which was the case with XFolder < 0.85
        // (i.e. exceptions in WPFolder or Object Desktop or whatever).
        if (pfnwpOriginal)
            mrc = (MRESULT)(*pfnwpOriginal)(hwndFrame, msg, mp1, mp2);
        else
        {
            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                   "Folder's pfnwpOrig not found.");
            mrc = WinDefWindowProc(hwndFrame, msg, mp1, mp2);
        }
    }

    return (mrc);
}

/*
 *@@ fdr_fnwpSubclassedFolderFrame:
 *      new window procedure for subclassed folder frames.
 *      This replaces the old procedure which was present
 *      before V0.9.3, and is needed for the new
 *      subclassing implementation.
 *
 *@@added @@changed V0.9.3 (2000-04-08) [umoeller]
 */

MRESULT EXPENTRY fdr_fnwpSubclassedFolderFrame(HWND hwndFrame,
                                               ULONG msg,
                                               MPARAM mp1,
                                               MPARAM mp2)
{
    MRESULT     mrc = 0;
    PSUBCLASSEDFOLDERVIEW psfv = fdrQuerySFV(hwndFrame,
                                            NULL);
    if (psfv)
        mrc = ProcessFolderMsgs(hwndFrame,
                                msg,
                                mp1,
                                mp2,
                                psfv,
                                psfv->pfnwpOriginal);
    else
    {
        // SFV not found: use the default
        // folder window procedure, but issue
        // a warning to the log
        cmnLog(__FILE__, __LINE__, __FUNCTION__,
               "Folder SUBCLASSEDFOLDERVIEW not found.");

        mrc = G_WPFolderWinClassInfo.pfnWindowProc(hwndFrame, msg, mp1, mp2);
    }

    return (mrc);
}

/*
 *@@ fdr_fnwpSubclassedFolderFrame:
 *      New window proc for subclassed folder frame windows.
 *      Folder frame windows are subclassed in fdrSubclassFolderView
 *      (which gets called from XFolder::wpOpen or XFldDisk::wpOpen
 *      for Disk views) with the address of this window procedure.
 *
 *      This is maybe the most central (and most complex) part of
 *      XWorkplace. Since most WPS methods are really just reacting
 *      to messages in the default WPS frame window proc, but for
 *      some features methods are just not sufficient, basically we
 *      simulate what the WPS does here by intercepting _lots_
 *      of messages before the WPS gets them.
 *
 *      Unfortunately, this leads to quite a mess, but we have
 *      no choice.
 *
 *      Things we do in this proc:
 *
 *      --  frame control manipulation for status bars
 *
 *      --  Warp 4 folder menu bar manipulation (WM_INITMENU)
 *
 *      --  handling of certain menu items w/out dismissing
 *          the menu; this calls functions in menus.c
 *
 *      --  menu owner draw (folder content menus w/ icons);
 *          this calls functions in menus.c also
 *
 *      --  complete interception of file-operation menu items
 *          such as "delete" for deleting all objects into the
 *          XWorkplace trash can
 *
 *      --  container control messages: tree view auto-scroll,
 *          updating status bars etc.
 *
 *      --  playing the new system sounds for menus and such.
 *
 *      Note that this function calls lots of "external" functions
 *      spread across all over the XFolder code. I have tried to
 *      reduce the size of this window procedure to an absolute
 *      minimum because this function gets called very often (for
 *      every single folder message) and large message procedures
 *      may thrash the processor caches.
 *
 *@@changed V0.9.0 [umoeller]: moved cleanup code from WM_CLOSE to WM_DESTROY; un-subclassing removed
 *@@changed V0.9.0 [umoeller]: moved this func here from xfldr.c
 *@@changed V0.9.1 (2000-01-29) [umoeller]: added WPMENUID_DELETE support
 *@@changed V0.9.1 (2000-01-31) [umoeller]: added "Del" key support
 *@@changed V0.9.2 (2000-02-22) [umoeller]: moved default winproc out of exception handler
 *@@changed V0.9.3 (2000-03-28) [umoeller]: added freaky menus setting
 */

/* MRESULT EXPENTRY fdr_fnwpSubclassedFolderFrame(HWND hwndFrame,
                                               ULONG msg,
                                               MPARAM mp1,
                                               MPARAM mp2)
{
    PSUBCLASSEDFOLDERVIEW psfv = NULL;
    PFNWP           pfnwpOriginal = NULL;
    MRESULT         mrc = MRFALSE;

    // find the original wnd proc in the
    // global linked list, so we can pass messages
    // on to it
    psfv = fdrQuerySFV(hwndFrame, NULL);
    if (psfv)
    {
        pfnwpOriginal = psfv->pfnwpOriginal;
        // somSelf = psfv->somSelf;
    }

    if (pfnwpOriginal)
    {
        if (psfv->somSelf == NULL)
            psfv->somSelf = _wpclsQueryObjectFromFrame(_WPFolder, hwndFrame);


        mrc = ProcessFolderMsgs(hwndFrame,
                                msg,
                                mp1,
                                mp2,
                                psfv,
                                pfnwpOriginal);

    } // end if (pfnwpOriginal)
    else
    {
        // original window procedure not found:
        // that's an error
        cmnLog(__FILE__, __LINE__, __FUNCTION__,
               "Folder's pfnwpOrig not found.");
        mrc = WinDefWindowProc(hwndFrame, msg, mp1, mp2);
    }

    return (mrc);
} */

/*
 *@@ fdr_fnwpSupplFolderObject:
 *      this is the wnd proc for the "Supplementary Object wnd"
 *      which is created for each folder frame window when it's
 *      subclassed. We need this window to handle additional
 *      messages which are not part of the normal message set,
 *      which is handled by fdr_fnwpSubclassedFolderFrame.
 *
 *      This window gets created in fdrSubclassFolderView, when
 *      the folder frame is also subclassed.
 *
 *      If we processed additional messages in fdr_fnwpSubclassedFolderFrame,
 *      we'd probably ruin other WPS enhancers which might use the same
 *      message in a different context (ObjectDesktop?), so we use a
 *      different window, which we own all alone.
 *
 *      We cannot use the global XFolder object window either
 *      (krn_fnwpThread1Object, kernel.c) because sometimes
 *      folder windows do not run in the main PM thread
 *      (TID 1), esp. when they're opened using WinOpenObject or
 *      REXX functions. I have found that manipulating windows
 *      from threads other than the one which created the window
 *      can produce really funny results or bad PM hangs.
 *
 *      This wnd proc always runs in the same thread as the folder
 *      frame wnd does.
 *
 *      This func is new with XFolder V0.82.
 *
 *@@changed V0.9.0 [umoeller]: moved this func here from xfldr.c
 */

MRESULT EXPENTRY fdr_fnwpSupplFolderObject(HWND hwndObject, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MPARAM mrc = NULL;
    PSUBCLASSEDFOLDERVIEW psfv = (PSUBCLASSEDFOLDERVIEW)
                WinQueryWindowULong(hwndObject, QWL_USER);

    switch (msg)
    {
        case WM_CREATE:
            // set the USER window word to the SUBCLASSEDFOLDERVIEW
            // structure which is passed to us upon window
            // creation (see cmnSubclassFrameWnd, which creates us)
            mrc = WinDefWindowProc(hwndObject, msg, mp1, mp2);
            psfv = (PSUBCLASSEDFOLDERVIEW)mp1;
            WinSetWindowULong(hwndObject, QWL_USER, (ULONG)psfv);
        break;

        /*
         *@@ SOM_ACTIVATESTATUSBAR:
         *      add / remove / repaint the folder status bar;
         *      this is posted every time XFolder needs to change
         *      anything about status bars. We must not play with
         *      frame controls from threads other than the thread
         *      in which the status bar was created, i.e. the thread
         *      in which the folder frame is running (which, in most
         *      cases, is thread 1, the main PM thread of the WPS),
         *      because reformatting frame controls from other
         *      threads will cause PM hangs or WPS crashes.
         *
         *      Parameters:
         *      -- ULONG mp1   -- 0: disable (destroy) status bar
         *                     -- 1: enable (create) status bar
         *                     -- 2: update (reformat) status bar
         *      -- HWND  mp2:  hwndView (frame) to update
         */

        case SOM_ACTIVATESTATUSBAR:
        {
            HWND hwndFrame = (HWND)mp2;
            #ifdef DEBUG_STATUSBARS
                _Pmpf(( "SOM_ACTIVATESTATUSBAR, mp1: %lX, psfv: %lX", mp1, psfv));
            #endif

            if (psfv)
                switch ((ULONG)mp1)
                {
                    case 0:
                        fdrCreateStatusBar(psfv->somSelf, psfv, FALSE);
                    break;

                    case 1:
                        fdrCreateStatusBar(psfv->somSelf, psfv, TRUE);
                    break;

                    default:
                    {
                        // == 2 => update status bars; this is
                        // necessary if the font etc. has changed
                        const char* pszStatusBarFont =
                                cmnQueryStatusBarSetting(SBS_STATUSBARFONT);
                        // avoid recursing
                        WinSendMsg(psfv->hwndStatusBar, STBM_PROHIBITBROADCASTING,
                                   (MPARAM)TRUE, MPNULL);
                        // set font
                        WinSetPresParam(psfv->hwndStatusBar,
                                        PP_FONTNAMESIZE,
                                        (ULONG)(strlen(pszStatusBarFont) + 1),
                                        (PVOID)pszStatusBarFont);
                        // update frame controls
                        WinSendMsg(hwndFrame, WM_UPDATEFRAME, MPNULL, MPNULL);
                        // update status bar text synchronously
                        WinSendMsg(psfv->hwndStatusBar, STBM_UPDATESTATUSBAR, MPNULL, MPNULL);
                    break; }
                }
        break; }

        /*
         *@@ SOM_CREATEFROMTEMPLATE:
         *      posted from mnuMenuItemSelected when
         *      a template from a config folder menu
         *      has been selected. We now need to
         *      create a new object from that template
         *      and move it to the mouse position.
         *      This is again done in menus.c by
         *      mnuCreateFromTemplate, we just use
         *      this message to do this asynchronously
         *      and to make sure that function runs on
         *      the folder view thread.
         *
         *      Parameters:
         *      -- WPObject* mp1: template object from menu
         *      -- WPFolder* mp2: folder in which to create
         *
         *@@added V0.9.2 (2000-02-26) [umoeller]
         */

        case SOM_CREATEFROMTEMPLATE:
            mnuCreateFromTemplate((WPObject*)mp1,
                                  (WPFolder*)mp2);
        break;

        default:
            mrc = WinDefWindowProc(hwndObject, msg, mp1, mp2);
    }
    return (mrc);
}

/*
 *@@ fdr_fnwpSubclFolderContentMenu:
 *      this is the subclassed wnd proc for folder content menus;
 *      we need to intercept mouse button 2 msgs to open a folder
 *      (WarpCenter behavior).
 *
 *      This function resides in folder.c because we need access
 *      to some folder globals. Subclassing happens in menus.c though.
 *
 *@@changed V0.9.0 [umoeller]: moved this func here from xfldr.c
 */

MRESULT EXPENTRY fdr_fnwpSubclFolderContentMenu(HWND hwndMenu, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT     mrc = 0;
    PFNWP       pfnwpOrig = 0;

    if (G_pfnwpFolderContentMenuOriginal)
        pfnwpOrig = G_pfnwpFolderContentMenuOriginal;
    else
    {
        cmnLog(__FILE__, __LINE__, __FUNCTION__,
               "G_pfnwpFolderContentMenuOriginal is NULL");
        pfnwpOrig = WinDefWindowProc;
    }

    TRY_LOUD(excpt1, NULL)
    {
        USHORT  sSelected;
        POINTL  ptlMouse;
        RECTL   rtlItem;

        switch(msg)
        {
            case WM_ADJUSTWINDOWPOS:
            {
                PSWP pswp = (PSWP)mp1;
                BOOL fAdjusted = FALSE;
                #ifdef DEBUG_MENUS
                    _Pmpf(("WM_ADJUSTWINDOWPOS"));
                #endif

                if ((pswp->fl & (SWP_MOVE)) == (SWP_MOVE))
                {
                    #ifdef DEBUG_MENUS
                        _Pmpf(("  SWP_MOVE set"));
                    #endif

                    if (!G_fFldrContentMenuMoved)
                    {
                        #ifdef DEBUG_MENUS
                            _Pmpf(("    Checking bounds"));
                        #endif
                        if ((pswp->x + pswp->cx)
                               > WinQuerySysValue(HWND_DESKTOP, SV_CXSCREEN))
                        {
                            pswp->x = 0;
                            #ifdef DEBUG_MENUS
                                _Pmpf(("    Changed x pos"));
                            #endif
                            // avoid several changes for this menu;
                            // this flag is reset by WM_INITMENU in
                            // fdr_fnwpSubclassedFolderFrame
                            G_fFldrContentMenuMoved = TRUE;
                            fAdjusted = TRUE;
                        }
                        if ((pswp->y + pswp->cy) >
                                WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN))
                        {
                            pswp->y = 0;
                            #ifdef DEBUG_MENUS
                                _Pmpf(("    Changed y pos"));
                            #endif
                            // avoid several changes for this menu;
                            // this flag is reset by WM_INITMENU in
                            // fdr_fnwpSubclassedFolderFrame
                            G_fFldrContentMenuMoved = TRUE;
                            fAdjusted = TRUE;
                        }
                    }
                }
                if (fAdjusted)
                    pswp->fl |= (SWP_NOADJUST);
                mrc = (MRESULT)(pfnwpOrig)(hwndMenu, msg, mp1, mp2);
                G_fFldrContentMenuButtonDown = FALSE;
            break; }

            #ifdef DEBUG_MENUS
                case MM_SELECTITEM:
                {
                    _Pmpf(( "MM_SELECTITEM: mp1 = %lX/%lX, mp2 = %lX",
                        SHORT1FROMMP(mp1),
                        SHORT2FROMMP(mp1),
                        mp2 ));
                    mrc = (MRESULT)(*pfnwpFolderContentMenuOriginal)(hwndMenu, msg, mp1, mp2);
                break; }
            #endif

            case WM_BUTTON2DOWN:
                #ifdef DEBUG_MENUS
                    _Pmpf(("WM_BUTTON2DOWN"));
                #endif

                ptlMouse.x = SHORT1FROMMP(mp1);
                ptlMouse.y = SHORT2FROMMP(mp1);
                WinSendMsg(hwndMenu, MM_SELECTITEM,
                           MPFROM2SHORT(MIT_NONE, FALSE),
                           MPFROM2SHORT(0, FALSE));
                sSelected = winhQueryItemUnderMouse(hwndMenu, &ptlMouse, &rtlItem);
                WinSendMsg(hwndMenu, MM_SETITEMATTR,
                           MPFROM2SHORT(sSelected,
                                        FALSE),
                           MPFROM2SHORT(MIA_HILITED,
                                        MIA_HILITED)
                    );
            break;

            case WM_BUTTON1DOWN:
                // let this be handled by the default proc
                #ifdef DEBUG_MENUS
                    _Pmpf(("WM_BUTTON1DOWN"));
                #endif
                G_fFldrContentMenuButtonDown = TRUE;
                mrc = (MRESULT)(pfnwpOrig)(hwndMenu, msg, mp1, mp2);
            break;

            case WM_BUTTON1DBLCLK:
            case WM_BUTTON2UP:
            {
                // upon receiving these, we will open the object directly; we need to
                // cheat a little bit because sending MM_SELECTITEM would open the submenu
                #ifdef DEBUG_MENUS
                    _Pmpf(("WM_BUTTON2UP"));
                #endif
                G_fFldrContentMenuButtonDown = TRUE;
                ptlMouse.x = SHORT1FROMMP(mp1);
                ptlMouse.y = SHORT2FROMMP(mp1);
                WinSendMsg(hwndMenu, MM_SELECTITEM,
                           MPFROM2SHORT(MIT_NONE, FALSE),
                           MPFROM2SHORT(0, FALSE));
                sSelected = winhQueryItemUnderMouse(hwndMenu, &ptlMouse, &rtlItem);

                cmnPlaySystemSound(MMSOUND_XFLD_CTXTSELECT);

                WinPostMsg(WinQueryWindow(hwndMenu, QW_OWNER),
                           WM_COMMAND,
                           (MPARAM)sSelected,
                           MPFROM2SHORT(CMDSRC_MENU, FALSE));
            break; }

            default:
                mrc = (MRESULT)(pfnwpOrig)(hwndMenu, msg, mp1, mp2);
            break;
        } // end switch
    }
    CATCH(excpt1)
    {
        // exception occured:
        return (MRESULT)0; // keep compiler happy
    } END_CATCH();

    return (mrc);
}


