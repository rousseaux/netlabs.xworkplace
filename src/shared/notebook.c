
/*
 *@@sourcefile notebook.c:
 *      this file is new with V0.82 and contains very useful code for
 *      WPS Settings notebooks pages. Most XWorkplace notebook pages
 *      are implemented using these routines.
 *
 *      All the functions in this file have the ntb* prefix.
 *
 *      The concept of this is that when inserting a notebook page
 *      by overriding the proper WPS methods for an object, you call
 *      ntbInsertPage here instead of calling wpInsertSettingsPage.
 *      This function will always use the same window procedure
 *      (ntb_fnwpPageCommon) and call CALLBACKS for certain notebook
 *      events which you can specify in your call to ntbInsertPage.
 *
 *      Callbacks exist for everything you will need on a notebook page;
 *      this saves you from having to rewrite the same dumb window proc
 *      for every notebook page, especially all the dull "Undo" and
 *      "Default" button handling.
 *
 *      See the declaration of CREATENOTEBOOKPAGE in notebook.h for
 *      details about the callbacks.
 *
 *      These routines maintain a list of currently "initialized"
 *      notebook pages, i.e. pages that are currently instantiated
 *      in memory. You can iterate over these pages in order to
 *      have controls on other pages updated, if this is necessary,
 *      using ntbQueryOpenPages and ntbUpdateVisiblePage. This is
 *      useful when a global setting is changed which should affect
 *      other possibly open notebook pages. For example, if status
 *      bars are disabled globally, the status bar checkboxes should
 *      be disabled in folder settings notebooks.
 *
 *      All the notebook functions are fully thread-safe and protected
 *      by mutex semaphores. ntb_fnwpPageCommon installs an exception
 *      handler, so all the callbacks are protected by that handler too.
 *
 *@@header "shared\notebook.h"
 */

/*
 *      Copyright (C) 1997-2001 Ulrich M”ller.
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

#define INCL_DOSSEMAPHORES
#define INCL_DOSEXCEPTIONS
#define INCL_DOSPROCESS
#define INCL_DOSERRORS
#define INCL_WIN
#include <os2.h>

// C library headers
#include <stdio.h>              // needed for except.h
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\comctl.h"             // common controls (window procs)
#include "helpers\cnrh.h"               // container helper routines
#include "helpers\dialog.h"             // dialog helpers
#include "helpers\except.h"             // exception handling
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\standards.h"          // some standard macros
#include "helpers\winh.h"               // PM helper routines

// SOM headers which don't crash with prec. header files
#include "xfldr.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file

// other SOM headers
#pragma hdrstop

// finally, our own header file
#include "shared\notebook.h"            // generic XWorkplace notebook handling

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

// root of linked list of opened notebook pages
// (this holds NOTEBOOKPAGELISTITEM's)
static PLINKLIST       G_pllOpenPages = NULL;          // this is auto-free

// root of linked list of subclassed notebooks
// (this holds
static PLINKLIST       G_pllSubclNotebooks = NULL;     // this is auto-free

// mutex semaphore for both lists
static HMTX            G_hmtxNotebooks = NULLHANDLE;

MRESULT EXPENTRY ntb_fnwpSubclNotebook(HWND hwndNotebook, ULONG msg, MPARAM mp1, MPARAM mp2);

/* ******************************************************************
 *
 *   Notebook helpers
 *
 ********************************************************************/

/*
 *@@ LockNotebooks:
 *
 *@@added V0.9.16 (2001-10-25) [umoeller]
 */

BOOL LockNotebooks(VOID)
{
    if (G_hmtxNotebooks)
        return !WinRequestMutexSem(G_hmtxNotebooks, SEM_INDEFINITE_WAIT);
            // WinRequestMutexSem works even if the thread has no message queue
    else
    {
        // and mutex semaphore for those lists
        if (!DosCreateMutexSem(NULL,         // unnamed
                               &G_hmtxNotebooks,
                               0,            // unshared
                               TRUE))        // request!
        {
            G_pllOpenPages = lstCreate(TRUE); // NOTEBOOKPAGELISTITEMs are freeable
            G_pllSubclNotebooks = lstCreate(TRUE); // SUBCLNOTEBOOKLISTITEM are freeable

            #ifdef DEBUG_NOTEBOOKS
                _Pmpf(("Created NOTEBOOKPAGELISTITEM list and mutex"));
            #endif

            return (TRUE);
        }
    }

    return (FALSE);
}

/*
 *@@ UnlockNotebooks:
 *
 *@@added V0.9.16 (2001-10-25) [umoeller]
 */

VOID UnlockNotebooks(VOID)
{
    DosReleaseMutexSem(G_hmtxNotebooks);
}

/* ******************************************************************
 *
 *   Notebook page dialog function
 *
 ********************************************************************/

/*
 *@@ ntbInitPage:
 *      implementation for WM_INITDLG in
 *      ntb_fnwpPageCommon.
 *
 *@@added V0.9.1 (99-12-31) [umoeller]
 */

VOID ntbInitPage(PCREATENOTEBOOKPAGE pcnbp,
                 HWND hwndDlg)
{
    // PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    #ifdef DEBUG_NOTEBOOKS
        _Pmpf(("ntb_fnwpPageCommon: WM_INITDLG"));
    #endif

    // store the dlg hwnd in notebook structure
    pcnbp->hwndDlgPage = hwndDlg;

    // store the WM_INITDLG parameter in the
    // window words; the CREATENOTEBOOKPAGE
    // structure is passed to us by ntbInsertPage
    // as a creation parameter in mp2
    WinSetWindowULong(pcnbp->hwndDlgPage, QWL_USER, (ULONG)pcnbp);
    pcnbp->fPageInitialized = FALSE;

    // make Warp 4 notebook buttons and move controls
    winhAssertWarp4Notebook(pcnbp->hwndDlgPage,
                            100,         // ID threshold
                            14);

    // set controls font to 8.Helv, if global settings
    // want this (paranoia page, V0.9.0)
    if (cmnQuerySetting(sfUse8HelvFont))
        winhSetControlsFont(pcnbp->hwndDlgPage,
                            0,
                            8000,
                            "8.Helv");

    // initialize the other fields
    pcnbp->preccSource = (PRECORDCORE)-1;
    pcnbp->hwndSourceCnr = NULLHANDLE;

    // call "initialize" callback
    if (pcnbp->pfncbInitPage)
        pcnbp->pfncbInitPage(pcnbp, CBI_INIT | CBI_SET | CBI_ENABLE);

    // timer desired?
    if (pcnbp->ulTimer)
    {
        WinStartTimer(WinQueryAnchorBlock(hwndDlg),
                      hwndDlg,
                      1,
                      pcnbp->ulTimer);
        // call timer callback already now;
        // let's not wait until the first downrun
        if (pcnbp->pfncbTimer)
            pcnbp->pfncbTimer(pcnbp, 1);
    }

    // winhAdjustControls desired?
    if (    (pcnbp->pampControlFlags)
#ifndef __ALWAYSRESIZESETTINGSPAGES__
         && (cmnQuerySetting(sfResizeSettingsPages))
#endif
       )
    {
        // yes: allocate and zero
        pcnbp->pxac = malloc(sizeof(XADJUSTCTRLS));
        memset(pcnbp->pxac, 0, sizeof(XADJUSTCTRLS));
        winhAdjustControls(pcnbp->hwndDlgPage,
                           pcnbp->pampControlFlags,
                           pcnbp->cControlFlags,
                           NULL,    // INIT
                           pcnbp->pxac);
    }

    pcnbp->fPageInitialized = TRUE;
}

/*
 *@@ ntbDestroyPage:
 *      implementation for WM_DESTROY in
 *      ntb_fnwpPageCommon.
 *
 *@@added V0.9.1 (99-12-31) [umoeller]
 *@@changed V0.9.7 (2000-12-10) [umoeller]: fixed mutex problems
 */

VOID ntbDestroyPage(PCREATENOTEBOOKPAGE pcnbp)
{
    #ifdef DEBUG_NOTEBOOKS
        _Pmpf(("ntb_fnwpPageCommon: WM_DESTROY"));
    #endif

    if (pcnbp)
    {
        #ifdef DEBUG_NOTEBOOKS
            _Pmpf(("  found pcnbp"));
        #endif

        // stop timer, if started
        if (pcnbp->ulTimer)
        {
            #ifdef DEBUG_NOTEBOOKS
                _Pmpf(("  stopping timer"));
            #endif
            WinStopTimer(WinQueryAnchorBlock(pcnbp->hwndDlgPage),
                         pcnbp->hwndDlgPage,
                         1);
        }

        // call INIT callback with CBI_DESTROY
        if (pcnbp->pfncbInitPage)
            pcnbp->pfncbInitPage(pcnbp, CBI_DESTROY);

        // tooltip to be destroyed?
        if (pcnbp->hwndTooltip)
            WinDestroyWindow(pcnbp->hwndTooltip);

        // winhAdjustControls prepared?
        if (pcnbp->pxac)
        {
            // yes: clean up
            winhAdjustControls(pcnbp->hwndDlgPage,
                               NULL,    // cleanup
                               0,       // cleanup
                               NULL,    // cleanup
                               pcnbp->pxac);
            free(pcnbp->pxac);
            pcnbp->pxac = NULL;
        }

        // remove the NOTEBOOKPAGELISTITEM from the
        // linked list of open notebook pages
        // V0.9.7 (2000-12-09) [umoeller]: more mutex protection

        #ifdef DEBUG_NOTEBOOKS
            _Pmpf(("  trying to remove page ID %d from list",
                    pcnbp->ulPageID));
        #endif
        if (pcnbp->pnbli)
        {
            BOOL fSemOwned = FALSE;
            TRY_LOUD(excpt1)
            {
                if (fSemOwned = LockNotebooks())
                    // WinRequestMutexSem works even if the thread has no message queue
                {
                    if (!lstRemoveItem(G_pllOpenPages,
                                       pcnbp->pnbli))  // this is auto-free!
                                // this free's the pnbli
                        cmnLog(__FILE__, __LINE__, __FUNCTION__,
                               "lstRemoveItem returned FALSE.");
                }
                else
                    cmnLog(__FILE__, __LINE__, __FUNCTION__,
                           "hmtxNotebookLists request failed.");
            }
            CATCH(excpt1) {} END_CATCH();

            if (fSemOwned)
                UnlockNotebooks();
        }

        // free allocated user memory
        if (pcnbp->pUser)
            free(pcnbp->pUser);
        if (pcnbp->pUser2)
            free(pcnbp->pUser2);
        free(pcnbp);
    }
}

/*
 *@@ ntbPageWmControl:
 *      implementation for WM_CONTROL in ntb_fnwpPageCommon.
 *
 *      hwndDlg is not passed because this can be retrieved
 *      thru pcnbp->hwndDlgPage.
 *
 *@@added V0.9.1 (99-12-31) [umoeller]
 *@@changed V0.9.4 (2000-07-11) [umoeller]: added CN_HELP and fPassCnrHelp handling
 *@@changed V0.9.9 (2001-02-06) [umoeller]: added support for direct editing
 *@@changed V0.9.9 (2001-03-15) [lafaix]: added support for valuesets
 *@@changed V0.9.9 (2001-03-27) [umoeller]: changed ulExtra for CN_RECORDCHECKED
 */

MRESULT EXPENTRY ntbPageWmControl(PCREATENOTEBOOKPAGE pcnbp,
                                  ULONG msg, MPARAM mp1, MPARAM mp2) // in: as in WM_CONTROL
{
    // code returned to ntb_fnwpPageCommon
    MRESULT mrc = 0;

    // identify the source of the msg
    ULONG   ulItemID = SHORT1FROMMP(mp1),
            usNotifyCode = SHORT2FROMMP(mp1);

    BOOL    fCallItemChanged = FALSE;
            // if this becomes TRUE, we'll call the "item changed" callback

    CHAR    szClassName[100];
    ULONG   ulClassCode = 0;
    ULONG   ulExtra = -1;

    #ifdef DEBUG_NOTEBOOKS
        _Pmpf(("ntb_fnwpPageCommon: WM_CONTROL"));
    #endif

    // "item changed" callback defined?
    if (    (pcnbp->pfncbItemChanged)
         && (pcnbp->hwndControl = WinWindowFromID(pcnbp->hwndDlgPage, ulItemID))
            // we identify the control by querying its class.
            // The standard PM classes have those wicked "#xxxx" classnames;
            // when we find a supported control, we filter out messages
            // which are not of interest, and call the
            // callbacks only for these messages by setting fCallItemChanged to TRUE
         && (WinQueryClassName(pcnbp->hwndControl,
                               sizeof(szClassName),
                               szClassName))
            // system class?
         && (szClassName[0] == '#')
        )
    {
        // now translate the class name into a ULONG
        sscanf(&(szClassName[1]), "%d", &ulClassCode);

        switch (ulClassCode)
        {
            // checkbox? radio button?
            case 3:
            {
                if (    (usNotifyCode == BN_CLICKED)
                     || (usNotifyCode == BN_DBLCLICKED) // added V0.9.0
                   )
                {
                    // code for WC_BUTTON...
                    ULONG ulStyle = winhQueryWindowStyle(pcnbp->hwndControl);

                    if (ulStyle & BS_PRIMARYSTYLES)
                            // == 0x000F; BS_PUSHBUTTON has 0x0,
                            // so we exclude pushbuttons here
                    {
                        // for checkboxes and radiobuttons, pass
                        // the new check state to the callback
                        ulExtra = (ULONG)WinSendMsg(pcnbp->hwndControl,
                                                    BM_QUERYCHECK,
                                                    MPNULL,
                                                    MPNULL);
                        fCallItemChanged = TRUE;
                    }
                }
            break; }

            // spinbutton?
            case 32:
            {
                if (    (usNotifyCode == SPBN_UPARROW)
                     || (usNotifyCode == SPBN_DOWNARROW)
                     || (usNotifyCode == SPBN_CHANGE)   // manual input
                   )
                {
                    // for spinbuttons, pass the new spbn
                    // value in ulExtra
                    WinSendMsg(pcnbp->hwndControl,
                               SPBM_QUERYVALUE,
                               (MPARAM)&ulExtra,
                               MPFROM2SHORT(0, SPBQ_UPDATEIFVALID));
                    fCallItemChanged = TRUE;
                }
            break; }

            // listbox?
            case 7:
            // combobox?
            case 2:
                if (usNotifyCode == LN_SELECT)
                    fCallItemChanged = TRUE;
            break;

            // entry field?
            case 6:
                if (    (usNotifyCode == EN_CHANGE)
                     || (usNotifyCode == EN_SETFOCUS)
                     || (usNotifyCode == EN_KILLFOCUS)
                   )
                    fCallItemChanged = TRUE;
                else if (usNotifyCode == EN_HOTKEY)
                {
                    // from hotkey entry field (comctl.c):
                    fCallItemChanged = TRUE;
                    ulExtra = (ULONG)mp2;
                        // HOTKEYNOTIFY struct pointer
                }
            break;

            // multi-line entry field?
            case 10:
                if (    (usNotifyCode == MLN_CHANGE)
                     || (usNotifyCode == MLN_SETFOCUS)
                     || (usNotifyCode == MLN_KILLFOCUS)
                   )
                    fCallItemChanged = TRUE;
            break;

            // container?
            case 37:
                switch (usNotifyCode)
                {
                    case CN_EMPHASIS:
                    {
                        // get cnr notification struct
                        PNOTIFYRECORDEMPHASIS pnre;
                        if (    (pnre = (PNOTIFYRECORDEMPHASIS)mp2)
                             && (pnre->fEmphasisMask & CRA_SELECTED)
                             && (pnre->pRecord)
                             && (pnre->pRecord != pcnbp->preccLastSelected)
                           )
                        {
                            fCallItemChanged = TRUE;
                            ulExtra = (ULONG)(pnre->pRecord);
                            pcnbp->preccLastSelected = pnre->pRecord;
                        }
                    break; }

                    case CN_CONTEXTMENU:
                    {
                        fCallItemChanged = TRUE;
                        ulExtra = (ULONG)mp2;
                            // record core for context menu
                            // or NULL for cnr whitespace
                        WinQueryPointerPos(HWND_DESKTOP,
                                           &(pcnbp->ptlMenuMousePos));
                    break; }

                    case CN_PICKUP:
                    case CN_INITDRAG:
                    {
                        // get cnr notification struct (mp2)
                        PCNRDRAGINIT pcdi = (PCNRDRAGINIT)mp2;
                        if (pcdi)
                        {
                            fCallItemChanged = TRUE;
                            ulExtra = (ULONG)pcdi;
                        }
                    break; }

                    case CN_DRAGAFTER:
                    case CN_DRAGOVER:
                    case CN_DROP:
                    {
                        // get cnr notification struct (mp2)
                        PCNRDRAGINFO pcdi = (PCNRDRAGINFO)mp2;
                        if (pcdi)
                        {
                            fCallItemChanged = TRUE;
                            ulExtra = (ULONG)pcdi;
                        }
                    break; }

                    case CN_DROPNOTIFY:
                    {
                        // get cnr notification struct (mp2)
                        PCNRLAZYDRAGINFO pcldi = (PCNRLAZYDRAGINFO)mp2;
                        if (pcldi)
                        {
                            fCallItemChanged = TRUE;
                            ulExtra = (ULONG)pcldi;
                        }
                    break; }

                    case CN_RECORDCHECKED:
                    {
                        // extra check-box cnr notification code
                        // (cctl_checkcnr.c)
                        PCHECKBOXRECORDCORE precc = (PCHECKBOXRECORDCORE)mp2;
                        if (precc)
                        {
                            ulExtra = (ULONG)precc;
                                    // changed V0.9.9 (2001-03-27) [umoeller]
                            fCallItemChanged = TRUE;
                        }
                    break; }

                    /*
                     * CN_EXPANDTREE:
                     *      do tree-view auto scroll
                     *      (added V0.9.1)
                     */

                    case CN_EXPANDTREE:
                    {
                        // PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
                        mrc = WinDefDlgProc(pcnbp->hwndDlgPage, msg, mp1, mp2);
                        if (cmnQuerySetting(sfTreeViewAutoScroll))
                        {
                            // store record for WM_TIMER later
                            pcnbp->preccExpanded = (PRECORDCORE)mp2;
                            // and container also
                            pcnbp->hwndExpandedCnr = pcnbp->hwndControl;
                            WinStartTimer(WinQueryAnchorBlock(pcnbp->hwndDlgPage),
                                          pcnbp->hwndDlgPage,
                                          999,      // ID
                                          100);
                        }
                    break; }

                    case CN_HELP:
                        // does caller want CN_HELP passed?
                        if (pcnbp->fPassCnrHelp)
                            // yes:
                            // call "item changed"
                            fCallItemChanged = TRUE;
                        else
                            // no (default), don't pass:
                            // same handling as with WM_HELP
                            cmnDisplayHelp(pcnbp->somSelf,
                                           pcnbp->ulDefaultHelpPanel);
                                // V0.9.16 (2001-10-23) [umoeller]
                    break;

                    case CN_BEGINEDIT:
                    case CN_REALLOCPSZ:
                    case CN_ENDEDIT:
                        // support for these has been added with
                        // V0.9.9 (2001-02-06) [umoeller]
                        ulExtra = (ULONG)mp2;
                                    // PCNREDITDATA
                        fCallItemChanged = TRUE;
                    break;
                } // end switch (usNotifyCode)
            break;    // container

            // linear slider?
            case 38:
            {
                if (    (usNotifyCode == SLN_CHANGE)
                     || (usNotifyCode == SLN_SLIDERTRACK)
                   )
                    fCallItemChanged = TRUE;
            break; }

            // circular slider?
            case 65:
            {
                if (    (usNotifyCode == CSN_SETFOCUS)
                                // mp2 is TRUE or FALSE
                     || (usNotifyCode == CSN_CHANGED)
                                // mp2 has new slider value
                     || (usNotifyCode == CSN_TRACKING)
                                // mp2 has new slider value
                   )
                {
                    fCallItemChanged = TRUE;
                    ulExtra = (ULONG)mp2;
                }
            break; }

            // value set? (added V0.9.9 (2001-03-15) [lafaix])
            case 39:
            {
                if (    (usNotifyCode == VN_ENTER)
                                // mp2 is selected row/col
                     || (usNotifyCode == VN_SELECT)
                                // mp2 is selected row/col
                   )
                {
                    fCallItemChanged = TRUE;
                    ulExtra = (ULONG)mp2;
                }

            break; }

        } // end switch (ulClassCode)

        if (fCallItemChanged)
        {
            // "important" message found:
            // call "item changed" callback
            mrc = pcnbp->pfncbItemChanged(pcnbp,
                                          ulItemID,
                                          usNotifyCode,
                                          ulExtra);
        }
    } // end if (szClassName[0] == '#')

    return (mrc);
}

/*
 *@@ ntbPageWindowPosChanged:
 *      implementation for WM_WINDOWPOSCHANGED in ntb_fnwpPageCommon.
 *
 *@@added V0.9.7 (2000-12-10) [umoeller]
 */

VOID ntbPageWindowPosChanged(PCREATENOTEBOOKPAGE pcnbp,
                             MPARAM mp1)
{
    PSWP pswp = (PSWP)mp1;

    #ifdef DEBUG_NOTEBOOKS
        _Pmpf(("ntb_fnwpPageCommon: WM_WINDOWPOSCHANGED"));
    #endif

    if (!pcnbp)
        return;

    if (pswp->fl & SWP_SHOW)
    {
        // notebook page is being shown:
        // call "initialize" callback
        if (pcnbp->pfncbInitPage)
        {
            WinEnableWindowUpdate(pcnbp->hwndDlgPage, FALSE);
            pcnbp->fPageVisible = TRUE;
            pcnbp->pfncbInitPage(pcnbp,
                                 CBI_SHOW | CBI_ENABLE);
                    // we also set the ENABLE flag so
                    // that the callback can re-enable
                    // controls when the page is being
                    // turned to

            WinEnableWindowUpdate(pcnbp->hwndDlgPage, TRUE);
        }
    }
    else if (pswp->fl & SWP_HIDE)
    {
        // notebook page is being hidden:
        // call "initialize" callback
        if (pcnbp->pfncbInitPage)
        {
            pcnbp->fPageVisible = FALSE;
            pcnbp->pfncbInitPage(pcnbp,
                                 CBI_HIDE);
        }
    }

    if (pswp->fl & SWP_SIZE)
    {
        // notebook is being resized:
        // was winhAdjustControls prepared?
        if (pcnbp->pxac)
        {
            // yes:
            winhAdjustControls(pcnbp->hwndDlgPage,
                               pcnbp->pampControlFlags,
                               pcnbp->cControlFlags,
                               pswp,
                               pcnbp->pxac);
        }
    }
}

/*
 *@@ ntbPageTimer:
 *      implementation for WM_TIMER in ntb_fnwpPageCommon.
 *
 *@@added V0.9.7 (2000-12-10) [umoeller]
 */

VOID ntbPageTimer(PCREATENOTEBOOKPAGE pcnbp,
                  MPARAM mp1)
{
    #ifdef DEBUG_NOTEBOOKS
        _Pmpf(("ntb_fnwpPageCommon: WM_TIMER"));
    #endif

    switch ((USHORT)mp1)    // timer ID
    {
        case 1:
            // timer for caller: call callback
            if (pcnbp)
                if (pcnbp->pfncbTimer)
                    pcnbp->pfncbTimer(pcnbp, 1);
        break;

        case 999:
            // tree view auto-scroll timer:
            // CN_EXPANDTREE has set up the data we
            // need in pcnbp
            if (pcnbp->preccExpanded->flRecordAttr & CRA_EXPANDED)
            {
                PRECORDCORE     preccLastChild;
                WinStopTimer(WinQueryAnchorBlock(pcnbp->hwndDlgPage),
                             pcnbp->hwndDlgPage,
                             999);
                // scroll the tree view properly
                preccLastChild = WinSendMsg(pcnbp->hwndExpandedCnr,
                                            CM_QUERYRECORD,
                                            pcnbp->preccExpanded,
                                               // expanded PRECORDCORE from CN_EXPANDTREE
                                            MPFROM2SHORT(CMA_LASTCHILD,
                                                         CMA_ITEMORDER));
                if ((preccLastChild) && (preccLastChild != (PRECORDCORE)-1))
                {
                    // ULONG ulrc;
                    cnrhScrollToRecord(pcnbp->hwndExpandedCnr,
                                       (PRECORDCORE)preccLastChild,
                                       CMA_TEXT,   // record text rectangle only
                                       TRUE);      // keep parent visible
                }
            }
        break;
    }
}

/*
 *@@ ntb_fnwpPageCommon:
 *      this is the common notebook window procedure which is
 *      always set if you use ntbInsertPage to insert notebook
 *      pages. This function will analyze all incoming messages
 *      and call the corresponding callback functions which you
 *      passed to ntbInsertPage. Also, for quite a number of
 *      messages, a predefined behavior will take place so you
 *      don't have to recode the same stuff for each notebook page.
 *
 *      ntbInsertPage has stored the CREATENOTEBOOKPAGE structure
 *      in QWL_USER of the page.
 *
 *      This function installs exception handling during message
 *      processing, i.e. including your callbacks, using
 *      excHandlerLoud in except.c. This will automatically write
 *      trap logs if a dialog page window proc traps. Of course,
 *      that doesn't save you from protecting your own mutex
 *      semaphores and stuff yourself; see except.c. for details.
 *
 *@@changed V0.9.0 [umoeller]: adjusted for new linklist functions
 *@@changed V0.9.0 [umoeller]: sped up window class analysis
 *@@changed V0.9.0 [umoeller]: added support for containers, sliders, and MLEs
 *@@changed V0.9.0 [umoeller]: changed entryfield support
 *@@changed V0.9.0 [umoeller]: fixed the age-old button double-click bug
 *@@changed V0.9.0 [umoeller]: added cnr drag'n'drop support
 *@@changed V0.9.0 [umoeller]: added cnr ownerdraw support
 *@@changed V0.9.0 [umoeller]: added CREATENOTEBOOKPAGE.fPageInitialized flag support
 *@@changed V0.9.0 [umoeller]: added 8.Helv controls font support
 *@@changed V0.9.0 [umoeller]: added support for WM_COMMAND return value
 *@@changed V0.9.1 (99-11-29) [umoeller]: added checkbox container support (ctlMakeCheckboxContainer)
 *@@changed V0.9.1 (99-11-29) [umoeller]: added container auto-scroll
 *@@changed V0.9.1 (99-11-29) [umoeller]: reworked message flow
 *@@changed V0.9.1 (99-12-06) [umoeller]: added notebook subclassing
 *@@changed V0.9.1 (99-12-19) [umoeller]: added EN_HOTKEY support (ctlMakeHotkeyEntryField)
 *@@changed V0.9.1 (99-12-31) [umoeller]: extracted ntbInitPage, ntbDestroyPage, ntbPageWmControl
 *@@changed V0.9.3 (2000-05-01) [umoeller]: added WM_MOUSEMOVE pointer changing
 *@@changed V0.9.7 (2000-12-10) [umoeller]: fixed mutex problems
 *@@changed V0.9.7 (2000-12-10) [umoeller]: extracted ntbPageWindowPosChanged, ntbPageTimer
 */

MRESULT EXPENTRY ntb_fnwpPageCommon(HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT             mrc = NULL;
    BOOL                fProcessed = FALSE;

    // protect ALL the processing with the
    // loud exception handler; this includes
    // all message processing, including the
    // callbacks defined by the implementor
    TRY_LOUD(excpt1)
    {
        PCREATENOTEBOOKPAGE pcnbp = NULL;

        /*
         * WM_INITDLG:
         *
         */

        if (msg == WM_INITDLG)
        {
            pcnbp = (PCREATENOTEBOOKPAGE)mp2;
            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
            ntbInitPage(pcnbp, hwndDlg);
            fProcessed = TRUE;
        }
        else
        {
            MRESULT     mrc2 = 0;

            // get the notebook creation struct, which was passed
            // to ntbInsertPage, from the window words

            // run message callback defined by caller, if any

            if (    (pcnbp = (PCREATENOTEBOOKPAGE)WinQueryWindowPtr(hwndDlg, QWL_USER))
                 && (pcnbp->pfncbMessage)
                 && (pcnbp->pfncbMessage(pcnbp, msg, mp1, mp2, &mrc2))
               )
            {
                // TRUE returned == msg processed:
                // return the return value
                mrc = mrc2;
                fProcessed = TRUE;
            }
        }

        if (    (pcnbp)
             && (!fProcessed)
           )
        {
            fProcessed = TRUE;

            switch(msg)
            {

                /*
                 * WM_CONTROL:
                 *
                 */

                case WM_CONTROL:
                    mrc = ntbPageWmControl(pcnbp, msg, mp1, mp2);
                break;

                /*
                 * WM_DRAWITEM:
                 *      container owner draw
                 */

                case WM_DRAWITEM:
                {
                    HWND    hwndControl;
                    CHAR    szClassName[5];

                    if (    (hwndControl = WinWindowFromID(hwndDlg,
                                                           (USHORT)mp1)) // has the control ID
                         && (WinQueryClassName(hwndControl,
                                               sizeof(szClassName),
                                               szClassName))
                       )
                    {
                        if (!memcmp(szClassName, "#37", 4))
                            // container:
                            mrc = cnrhOwnerDrawRecord(mp2,
                                                      // V0.9.16 (2001-09-29) [umoeller]
                                                      pcnbp->ulCnrOwnerDraw);
                    }
                    // else: return default FALSE
                break; } // WM_DRAWITEM

                /*
                 * WM_MENUEND:
                 *      this is received when a menu is just
                 *      about to be destroyed. This might come
                 *      in if the notebook page has some context
                 *      menu defined by the caller.
                 *
                 *      For the purpose of container context menus,
                 *      per definition, if the caller sets
                 *      preccSource to some record core or NULL, we
                 *      will remove source emphasis from that recc
                 *      (or, if NULL, from the whole container)
                 *      in this situation.
                 *
                 *      Normally, hwndCnr is 0 and preccSource is -1.
                 */

                case WM_MENUEND:
                {
                    #ifdef DEBUG_NOTEBOOKS
                        _Pmpf(("ntb_fnwpPageCommon: WM_MENUEND"));
                    #endif

                    if (    (pcnbp->preccSource != (PRECORDCORE)-1)
                         && (pcnbp->hwndSourceCnr)
                       )
                    {
                        WinSendMsg(pcnbp->hwndSourceCnr,
                                   CM_SETRECORDEMPHASIS,
                                   (MPARAM)(pcnbp->preccSource),
                                   MPFROM2SHORT(FALSE, CRA_SOURCE));
                        // reset hwndCnr to make sure we won't
                        // do this again
                        pcnbp->hwndSourceCnr = 0;
                        // but leave preccSource as it is, because
                        // WM_MENUEND is posted before WM_COMMAND,
                        // and the caller might still need this
                    }
                    mrc = (MRESULT)0;
                break; } // WM_MENUEND

                /*
                 * WM_COMMAND:
                 *      for buttons, we also use the callback
                 *      for "item changed"; the difference
                 *      between WM_CONTROL and WM_COMMAND has
                 *      never made sense to me
                 */

                case WM_COMMAND:
                {
                    USHORT usItemID = SHORT1FROMMP(mp1);

                    #ifdef DEBUG_NOTEBOOKS
                        _Pmpf(("ntb_fnwpPageCommon: WM_COMMAND"));
                    #endif

                    // call "item changed" callback
                    if (pcnbp)
                        if (pcnbp->pfncbItemChanged)
                            mrc = pcnbp->pfncbItemChanged(pcnbp,
                                                          usItemID,
                                                          0,
                                                          (ULONG)mp2);
                break; } // WM_COMMAND

                /*
                 * WM_HELP:
                 *      results from the "Help" button or
                 *      from pressing F1; we display help
                 *      depending on the control which has
                 *      the focus and depending on the data
                 *      which has been passed to us
                 */

                case WM_HELP:
                    cmnDisplayHelp(pcnbp->somSelf,
                                   pcnbp->ulDefaultHelpPanel);
                            // V0.9.16 (2001-10-23) [umoeller]
                break; // WM_HELP

                /*
                 * WM_WINDOWPOSCHANGED:
                 *      cheap trick: this msg is posted when
                 *      the user switches to a different notebook
                 *      page. Since every notebook page is really
                 *      a separate dialog window, PM simulates
                 *      switching notebook pages by showing or
                 *      hiding the various dialog windows.
                 *      We will call the INIT callback with a
                 *      show/hide flag then.
                 */

                case WM_WINDOWPOSCHANGED:
                    ntbPageWindowPosChanged(pcnbp, mp1);
                    // call default
                    mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
                break;  // WM_WINDOWPOSCHANGED

                /*
                 * WM_MOUSEMOVE:
                 *      set mouse pointer to "wait" if the
                 *      corresponding flag in pcnbp is on.
                 */

                case WM_MOUSEMOVE:
                    if (pcnbp)
                        if (pcnbp->fShowWaitPointer)
                        {
                            WinSetPointer(HWND_DESKTOP,
                                          WinQuerySysPointer(HWND_DESKTOP,
                                                             SPTR_WAIT,
                                                             FALSE));
                            break;
                        }

                    mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
                break;

                /*
                 * WM_CONTROLPOINTER:
                 *      set mouse pointer to "wait" if the
                 *      corresponding flag in pcnbp is on.
                 */

                case WM_CONTROLPOINTER:
                    if (pcnbp)
                        if (pcnbp->fShowWaitPointer)
                        {
                            mrc = (MRESULT)WinQuerySysPointer(HWND_DESKTOP,
                                                              SPTR_WAIT,
                                                              FALSE);
                            break;
                        }

                    mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
                break;

                /*
                 * WM_TIMER:
                 *      call timer callback, if defined.
                 */

                case WM_TIMER:
                    ntbPageTimer(pcnbp, mp1);
                break;

                /*
                 *@@ XNTBM_UPDATE:
                 *      message posted by ntbUpdateVisiblePage
                 *      to update the page by calling the INIT
                 *      callback.
                 *
                 *      Parameters:
                 *
                 *      ULONG mp1: CBI_* flags to pass to INIT callback.
                 *
                 *@@added V0.9.3 (2000-04-24) [umoeller]
                 */

                case XNTBM_UPDATE:
                    if (pcnbp)
                        if (pcnbp->pfncbInitPage)
                            pcnbp->pfncbInitPage(pcnbp, (ULONG)mp1);
                break;

                /*
                 * WM_DESTROY:
                 *      clean up the allocated structures.
                 */

                case WM_DESTROY:
                    ntbDestroyPage(pcnbp);
                    fProcessed = FALSE;
                    // mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
                break;

                default:
                    fProcessed = FALSE;

            } // end switch (msg)
        } // end if (!fProcessed)
    }
    CATCH(excpt1) {} END_CATCH();

    if (!fProcessed)
        mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);

    return (mrc);
}

/*
 *@@ CreateNBLI:
 *      creates a new SUBCLNOTEBOOKLISTITEM for hwndNotebook.
 *
 *      This has been extracted from ntbInsertPage because it
 *      wasn't such a good idea to put the entire function in
 *      a mutex block.
 *
 *@@added V0.9.7 (2000-12-09) [umoeller]
 */

PNOTEBOOKPAGELISTITEM CreateNBLI(PCREATENOTEBOOKPAGE pcnbp) // in: new struct from ntbInsertPage
{
    BOOL        fSemOwned = FALSE;

    // create NOTEBOOKPAGELISTITEM to be stored in list
    PNOTEBOOKPAGELISTITEM pnbliNew = NULL;

    TRY_LOUD(excpt1)
    {
        // store new page in linked list
        if (fSemOwned = LockNotebooks())
        {
            HWND        hwndDesktop = NULLHANDLE,
                        hwndCurrent = pcnbp->hwndNotebook;
            PLISTNODE   pNode;
            BOOL        fNotebookAlreadySubclassed = FALSE;

            pnbliNew = malloc(sizeof(NOTEBOOKPAGELISTITEM));

            pnbliNew->pcnbp = pcnbp;

            // store new list item in structure, so we can easily
            // find it upon WM_DESTROY
            pcnbp->pnbli = (PVOID)pnbliNew;

            // get frame to which this window belongs
            hwndDesktop = WinQueryDesktopWindow(WinQueryAnchorBlock(HWND_DESKTOP),
                                                NULLHANDLE);

            // find frame window handle of "Workplace Shell" window
            while ( (hwndCurrent) && (hwndCurrent != hwndDesktop))
            {
                pcnbp->hwndFrame = hwndCurrent;
                hwndCurrent = WinQueryWindow(hwndCurrent, QW_PARENT);
            }

            if (!hwndCurrent)
                pcnbp->hwndFrame = NULLHANDLE;

            lstAppendItem(G_pllOpenPages,
                          pnbliNew);
            #ifdef DEBUG_NOTEBOOKS
                _Pmpf(("Appended NOTEBOOKPAGELISTITEM to pages list"));
            #endif

            // now search the list of notebook list items
            // for whether a page has already been inserted
            // into this notebook; if not, subclass the
            // notebook (otherwise it has already been subclassed
            // by this func)
            pNode = lstQueryFirstNode(G_pllSubclNotebooks);
            while (pNode)
            {
                PSUBCLNOTEBOOKLISTITEM psnbliThis = (PSUBCLNOTEBOOKLISTITEM)pNode->pItemData;
                if (psnbliThis)
                    if (psnbliThis->hwndNotebook == pcnbp->hwndNotebook)
                    {
                        fNotebookAlreadySubclassed = TRUE;
                        break;
                    }

                pNode = pNode->pNext;
            }

            if (!fNotebookAlreadySubclassed)
            {
                // notebook not yet subclassed:
                // do it now
                PSUBCLNOTEBOOKLISTITEM pSubclNBLINew = (PSUBCLNOTEBOOKLISTITEM)malloc(sizeof(SUBCLNOTEBOOKLISTITEM));
                if (pSubclNBLINew)
                {
                    pSubclNBLINew->hwndNotebook = pcnbp->hwndNotebook;
                    lstAppendItem(G_pllSubclNotebooks,
                                  pSubclNBLINew);
                    pSubclNBLINew->pfnwpNotebookOrig
                        = WinSubclassWindow(pcnbp->hwndNotebook,
                                            ntb_fnwpSubclNotebook);
                }
            }
        } // end if (fSemOwned)
        else
            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                   "hmtxNotebookLists mutex request failed");
    }
    CATCH(excpt1) {} END_CATCH();

    if (fSemOwned)
        UnlockNotebooks();

    return (pnbliNew);
}

/*
 *@@ FindNBLI:
 *      finds the SUBCLNOTEBOOKLISTITEM for hwndNotebook.
 *
 *      This has been extracted from ntb_fnwpSubclNotebook
 *      because it wasn't such a good idea to put the entire
 *      window proc in a mutex block.
 *
 *@@added V0.9.7 (2000-12-09) [umoeller]
 */

PSUBCLNOTEBOOKLISTITEM FindNBLI(HWND hwndNotebook)
{
    PSUBCLNOTEBOOKLISTITEM pSubclNBLI = NULL;
    BOOL fSemOwned = FALSE;

    TRY_LOUD(excpt1)
    {
        if (fSemOwned = LockNotebooks())
        {
            PLISTNODE   pNode = lstQueryFirstNode(G_pllSubclNotebooks);
            while (pNode)
            {
                PSUBCLNOTEBOOKLISTITEM psnbliThis = (PSUBCLNOTEBOOKLISTITEM)pNode->pItemData;
                if (psnbliThis)
                    if (psnbliThis->hwndNotebook == hwndNotebook)
                    {
                        pSubclNBLI = psnbliThis;
                        break;
                    }

                pNode = pNode->pNext;
            }
        } // end if (fSemOwned)
    }
    CATCH(excpt1) {} END_CATCH();

    if (fSemOwned)
        UnlockNotebooks();

    return (pSubclNBLI);
}

/*
 *@@ DestroyNBLI:
 *      implementation for WM_DESTROY in ntb_fnwpSubclNotebook.
 *
 *      This has been extracted from ntb_fnwpSubclNotebook
 *      because it wasn't such a good idea to put the entire
 *      window proc in a mutex block.
 *
 *@@added V0.9.7 (2000-12-09) [umoeller]
 *@@changed V0.9.14 (2001-08-23) [umoeller]: fixed bad pointer on list node remove
 */

VOID DestroyNBLI(HWND hwndNotebook,
                 PSUBCLNOTEBOOKLISTITEM pSubclNBLI)
{
    BOOL fSemOwned = FALSE;

    TRY_LOUD(excpt1)
    {
        if (fSemOwned = LockNotebooks())
        {
            PLISTNODE pPageNode = lstQueryFirstNode(G_pllOpenPages);

            #ifdef DEBUG_NOTEBOOKS
                _Pmpf(("ntb_fnwpSubclNotebook: WM_DESTROY"));
            #endif

            while (pPageNode)
            {
                PLISTNODE pNext = pPageNode->pNext; // V0.9.14 (2001-08-23) [umoeller]
                PNOTEBOOKPAGELISTITEM pPageLI = (PNOTEBOOKPAGELISTITEM)pPageNode->pItemData;
                if (    (pPageLI)
                     && (pPageLI->pcnbp)
                     && (pPageLI->pcnbp->hwndNotebook == hwndNotebook) // our page?
                     && (!pPageLI->pcnbp->fPageInitialized)
                            // page has NOT been initialized
                            // (this flag is set by ntb_fnwpPageCommon):
                   )
                {
                    // remove it from list
                    #ifdef DEBUG_NOTEBOOKS
                        _Pmpf(("  removed page ID %d", pPageLI->pcnbp->ulPageID));
                    #endif
                    free(pPageLI->pcnbp);
                    lstRemoveNode(G_pllOpenPages,
                                  pPageNode);
                }

                pPageNode = pNext;
            }

            // remove notebook control from list
            lstRemoveItem(G_pllSubclNotebooks,
                          pSubclNBLI);      // this frees the pSubclNBLI
            #ifdef DEBUG_NOTEBOOKS
                _Pmpf(("  removed pSubclNBLI"));
            #endif
        } // end if (fSemOwned)
    }
    CATCH(excpt1) {} END_CATCH();

    if (fSemOwned)
        UnlockNotebooks();
}

/*
 *@@ ntb_fnwpSubclNotebook:
 *      window procedure for notebook controls subclassed
 *      by ntbInsertPage.
 *
 *      The notebook control itself is automatically subclassed
 *      by ntbInsertPage if it hasn't been subclassed yet. Subclassing
 *      is necessary because, with V0.9.1, I finally realized
 *      that each call to ntbInsertPage allocates memory for
 *      the CREATENOTEBOOKPAGE items, but this is only
 *      released upon WM_DESTROY with pages that have actually
 *      been switched to. Pages which were inserted but never
 *      switched to will never get WM_DESTROY, so for those pages
 *      this subclassed notebook proc does the cleanup.
 *
 *      Er, these memory leaks must have been in XFolder for
 *      ages. Quite embarassing.
 *
 *@@added V0.9.1 (99-12-06) [umoeller]
 *@@changed V0.9.7 (2000-12-10) [umoeller]: fixed mutex problems
 */

MRESULT EXPENTRY ntb_fnwpSubclNotebook(HWND hwndNotebook, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    PSUBCLNOTEBOOKLISTITEM pSubclNBLI = FindNBLI(hwndNotebook);

    if (pSubclNBLI)
    {
        switch (msg)
        {
            /*
             * WM_DESTROY:
             *      notebook is being destroyed.
             *      Enumerate notebook pages inserted
             *      by ntbInsertPage and destroy those
             *      which haven't been initialized yet,
             *      because those won't get WM_DESTROY
             *      in ntb_fnwpPageCommon.
             */

            case WM_DESTROY:
            {
                PFNWP pfnwpNotebookOrig = pSubclNBLI->pfnwpNotebookOrig;
                DestroyNBLI(hwndNotebook, pSubclNBLI);
                            // after this, pSubclNBLI is invalid!!
                mrc = pfnwpNotebookOrig(hwndNotebook, msg, mp1, mp2);
            break; }

            default:
                mrc = pSubclNBLI->pfnwpNotebookOrig(hwndNotebook, msg, mp1, mp2);
        }
    } // end if (pSubclNBLI)
    else
        mrc = WinDefWindowProc(hwndNotebook, msg, mp1, mp2);

    return (mrc);
}

/*
 *@@ ntbInsertPage:
 *      this function inserts the specified notebook page
 *      using the wpInsertSettingsPage function. However,
 *      this always uses ntb_fnwpPageCommon for the notebook's
 *      window procedure, which then calls the callbacks which
 *      you may specify in the CREATENOTEBOOKPAGE structure.
 *
 *      This function returns the return code of wpInsertSettingsPages.
 *
 *      A linked list of currently open pages is maintained by this
 *      function, which can be accessed from ntbQueryOpenPages and
 *      ntbUpdateVisiblePage.
 *
 *      All the notebook functions are thread-safe.
 *
 *      <B>Example usage</B> from some WPS "add notebook page" method:
 *
 +          PCREATENOTEBOOKPAGE pcnbp = malloc(sizeof(CREATENOTEBOOKPAGE));
 +          // always zero all fields, because we don't use all of them
 +          memset(pcnbp, 0, sizeof(CREATENOTEBOOKPAGE));
 +          pcnbp->somSelf = somSelf;
 +          pcnbp->hwndNotebook = hwndNotebook;  // from WPS method header
 +          pcnbp->hmod = ...;          // resource module handle
 +          pcnbp->ulDlgID = ...;       // dialog ID in pcnbp->hmod
 +          pcnbp->fMajorTab = TRUE;
 +          pcnbp->pszName = "~Test page";
 +          pcnbp->ulDefaultHelpPanel  = ...;
 +          pcnbp->ulPageID = ...;      // unique ID
 +          pcnbp->pfncbInitPage    = fncbYourPageInitPage;     // init callback
 +          pcnbp->pfncbItemChanged = fncbYourPageItemChanged;  // item-changed callback
 +          ntbInsertPage(pcnbp);
 *
 *      The <B>ulPageID</B> is not required, but strongly recommended
 *      to tell the different notebook pages apart (esp. when using
 *      ntbUpdateVisiblePage). This can be used to easily update all
 *      open settings pages with ntbUpdateVisiblePage. For example,
 *      if a global setting changes which affects instance notebook
 *      pages as well, all of these can be easily updated. Use any
 *      ULONG you like, this has nothing to do with the dialog resources.
 *      XWorkplace uses the SP_* IDs from common.h.
 *
 *      <B>The "init" callback</B>
 *
 *      The "init" callback receives the following parameters:
 *
 *      --  PCREATENOTEBOOKPAGE pcnbp: notebook info struct
 *
 *      --  ULONG flFlags:           CBI_* flags (notebook.h), which determine
 *                                   the context of the call.
 *
 *      ntb_fnwpPageCommon will call this init callback itself
 *      in the following situations:
 *
 *      -- When the page is initialized (WM_INITDLG), flFlags is
 *         CBI_INIT | CBI_SET | CBI_ENABLE.
 *
 *      -- When the page is later turned away from, flFlags is
 *         CBI_HIDE.
 *
 *      -- When the page is later turned to, flFlags is
 *         CBI_SHOW | CBI_ENABLE.
 *
 *      -- When the page is destroyed (when the notebook is closed),
 *         flFlags is CBI_DESTROY.
 *
 *      It is recommended to have up to six blocks in your "init" callback
 *      (but you probably won't need all of them):
 *
 +      VOID fncbWhateverInitPage(PCREATENOTEBOOKPAGE pcnbp,  // notebook info struct
 +                                ULONG flFlags)              // CBI_* flags (notebook.h)
 +      {
 +          if (flFlags & CBI_INIT) // initialize page; this gets called exactly once
 +          {
 +              pcnbp->pUser = malloc(...); // allocate memory for "Undo";
 +                                          // this will be free()'d automatically
 +                                          // on destroy
 +              memcpy((PVOID)pcnbp->pUser, ...)  // backup data for "Undo"
 +
 +                   ...        // initialize controls (set styles, subclass controls, etc.)
 +          }
 +
 +          if (flFlags & CBI_SET)
 +                   ... // set controls' data; this gets called only once
 +                       // from ntb_fnwpPageCommon, but you can call this yourself
 +                       // several times
 +          if (flFlags & CBI_ENABLE)
 +                   ... // enable/disable controls; this can get called several times
 +          if (flFlags & CBI_HIDE)
 +                   ..  // page is turned away from: hide additional frame windows, if any
 +          if (flFlags & CBI_SHOW)
 +                   ..  // page is turned back to a second time: show windows again
 +          if (flFlags & CBI_DESTROY)
 +                   ... // clean up on exit: only once
 +      }
 *
 *      The "flFlags" approach allows you to call the init callback
 *      from your own code also if your page should need updating.
 *      For example, if a checkbox is unchecked, you can call the init
 *      callback with CBI_ENABLE only from the "item changed" callback (below)
 *      to disable other controls if needed.
 *
 *      This is especially useful with the ntbUpdateVisiblePage function,
 *      which iterates over all open pages.
 *
 *      <B>The "item changed" callback</B>
 *
 *      This gets called from ntb_fnwpPageCommon when either
 *      WM_CONTROL or WM_COMMAND comes in. Note that ntb_fnwpPageCommon
 *      _filters_ these messages and calls the "item changed" callback
 *      only for notifications which I have considered useful so far.
 *      If this is not sufficient for you, you must use the pfncbMessage
 *      callback (which gets really all the messages).
 *
 *      This callback is not required, but your page won't react to
 *      anything if you don't install this (which might be OK if your
 *      page is "read-only").
 *
 *      The "item changed" callback receives the following parameters:
 *
 *      --  PCREATENOTEBOOKPAGE pcnbp: notebook info struct
 *
 *      --  ULONG ulItemId:            ID of the changing item
 *
 *      --  USHORT usNotifyCode:       as in WM_CONTROL; NULL for WM_COMMAND
 *
 *      --  ULONG  ulExtra:            additional control data.
 *
 *      <B>ulExtra</B> has the following:
 *
 *      -- For checkboxes (BN_CLICKED), this is the new selection state
 *         of the control (0 or 1, or, for tri-state checkboxes, possibly 2).
 *
 *      -- For radio buttons (BN_CLICKED), only the radio button that
 *         got selected gets a callback with ulExtra == 1. The other radio
 *         buttons in the group (which got unchecked) do not produce
 *         messages.
 *
 *      -- For pushbuttons (WM_COMMAND message), this contains
 *         the mp2 of WM_COMMAND (usSourceType, usPointer).
 *
 *      -- For spinbuttons, this contains the new LONG value if
 *         a value has changed. This works for numerical spinbuttons only.
 *
 *      -- Container CN_EMPHASIS: has the new selected record
 *                    (only if CRA_SELECTED changed).
 *
 *      -- Container CN_CONTEXTMENU: has the record core (mp2);
 *                    rclMenuMousePos has the mouse position.
 *
 *      -- Container CN_INITDRAG or
 *                   CN_PICKUP:   has the PCNRDRAGINIT (mp2).
 *
 *      -- Container CN_DRAGOVER,
 *                   CN_DROP:     has the PCNRDRAGINFO (mp2).
 *
 *      -- Container CN_DROPNOTIFY: has the PCNRLAZYDRAGINFO (mp2).
 *
 *      -- Container CN_BEGINEDIT, CN_REALLOCPSZ, CN_ENDEDIT: has the
 *              PCNREDITDATA (mp2) (added with V0.9.9 (2001-02-06) [umoeller]).
 *
 *      -- Check-box container CN_RECORDCHECKED (see cctl_checkcnr.c):
 *              has the CHECKBOXRECORDCORE pointer that was clicked.
 *
 *      -- For circular and linear sliders (CSN_CHANGED or CSN_TRACKING),
 *                this has the new slider value.
 *
 *      -- For value sets, this contains the coordinates of the
 *         selected value (mp2).
 *
 *      -- For all other controls/messages, this is always -1.
 *
 *      Whatever the "item changed" callback returns will be the
 *      return value of ntb_fnwpPageCommon. Normally, you should
 *      return 0, except for the container d'n'd messages.
 *
 *      A couple of remarks about using this on radio buttons...
 *      If radio buttons just won't work in your dialog:
 *
 *      --  Make sure that the first radio button has the WS_GROUP style,
 *          and the first item _after_ the radio buttons has the WS_GROUP
 *          style as well.
 *
 *      --  The group of radio buttons should not appear at the top of the
 *          dialog. PM just keeps sending the wrong BN_CLICKED things if
 *          radio buttons are on top. I think this is a PM bug.
 *
 *      <B>Implementing an "Undo" button</B>
 *
 *      CREATENOTEBOOKPAGE has the pUser and pUser2 fields, which are
 *      PVOIDs to user data. Upon CBI_INIT in the "init" callback, you
 *      can allocate memory (using malloc()) and copy undo data into
 *      that buffer. In the "item changed" callback, check for the ID
 *      of your "Undo" button, copy that backed-up data back and call
 *      the "init" callback with CBI_SET | CBI_ENABLE to have the page
 *      updated with the backed-up data.
 *
 *      If pUser or pUser2 are != NULL, free() will automatically be
 *      invoked on them by ntb_fnwpPageCommon when the page is destroyed.
 *      This is done _after_ the INIT callback has been called which
 *      CBI_DESTROY, so if you store something in there which should
 *      not be free()'d, set those pointers to NULL upon CBI_DESTROY.
 *
 *      <B>Using timers</B>
 *
 *      Using timers can be helpful if your page should update itself
 *      periodically. This is extremely easy: just set
 *      CREATENOTEBOOKPAGE.ulTimer to the frequency (in ms) and
 *      install a timer callback in CREATENOTEBOOKPAGE.pfncbTimer,
 *      which then gets called with that frequency. The timer is
 *      automatically started and stopped when the page is destroyed.
 *
 *      In your "timer" callback, you can then call the "init" callback
 *      with CBI_SET to have your controls updated.
 *
 *      The "timer" callback gets these parameters:
 *      --  PCREATENOTEBOOKPAGE pcnbp: notebook info struct
 *      --  ULONG ulTimer:             timer id (always 1)
 *
 *@@changed V0.9.0 [umoeller]: adjusted for new linklist functions
 *@@changed V0.9.1 (99-12-06) [umoeller]: added notebook subclassing
 *@@changed V0.9.1 (2000-02-14) [umoeller]: reversed order of functions; now subclassing is last
 *@@changed V0.9.7 (2000-12-10) [umoeller]: fixed mutex problems
 *@@changed V0.9.9 (2001-03-27) [umoeller]: changed usItemID to ULONG in "item changed" callback
 *@@changed V0.9.16 (2001-10-23) [umoeller]: finally found out how to set minor tab titles
 */

ULONG ntbInsertPage(PCREATENOTEBOOKPAGE pcnbp)
{
    PAGEINFO        pi;
    ULONG           ulrc;

    memset(&pi, 0, sizeof(PAGEINFO));

    pi.cb                  = sizeof(PAGEINFO);
    pi.hwndPage            = NULLHANDLE;
    pi.pfnwp               = ntb_fnwpPageCommon;
    pi.resid               = pcnbp->hmod;
    pi.dlgid               = pcnbp->ulDlgID;
    pi.pCreateParams       = pcnbp;
    pi.usPageStyleFlags    = BKA_STATUSTEXTON |
                                    pcnbp->usPageStyleFlags;
    pi.usPageInsertFlags   = BKA_FIRST;
    pi.usSettingsFlags     = ((pcnbp->fEnumerate) ? SETTINGS_PAGE_NUMBERS : 0);
                                    // enumerate in status line
    pi.pszName             = pcnbp->pszName;

    pi.pszHelpLibraryName  = (PSZ)cmnQueryHelpLibrary();
    pi.idDefaultHelpPanel  = pcnbp->ulDefaultHelpPanel;

    // insert page
    if (ulrc = _wpInsertSettingsPage(pcnbp->somSelf,
                                     pcnbp->hwndNotebook,
                                     &pi))
            // this returns the notebook page ID
    {
        // successfully inserted:

        // if this is a major tab, hack the "page info"
        // to display the minor tab title in the notebook
        // context menu V0.9.16 (2001-10-23) [umoeller]
        if (pcnbp->pszMinorName)
        {
            BOOKPAGEINFO bpi;
            memset(&bpi, 0, sizeof(&bpi));
            bpi.cb = sizeof(BOOKPAGEINFO);
            bpi.fl = BFA_MINORTABTEXT;
            bpi.pszMinorTab = pcnbp->pszMinorName;
            bpi.cbMinorTab = strlen(pcnbp->pszMinorName);
            WinSendMsg(pcnbp->hwndNotebook,
                       BKM_SETPAGEINFO,
                       (MPARAM)ulrc,
                       (MPARAM)&bpi);
        }

        // store PM notebook page ID
        pcnbp->ulNotebookPageID = ulrc;

        // create SUBCLNOTEBOOKLISTITEM
        CreateNBLI(pcnbp);
    }
    else
        winhDebugBox(HWND_DESKTOP,
                     "XWorkplace: Error in ntbInsertPage",
                     "Notebook page could not be inserted (wpInsertSettingsPage failed).");

    return (ulrc);
}

/*
 *@@ ntbFormatPage:
 *      wrapper around dlghFormatDialog for dynamically
 *      formatting notebook pages.
 *
 *      Usage:
 *
 *      1)  With ntbInsertPage, specify ID_XFD_EMPTYDLG for
 *          the dialog ID to be loaded (which is an empty
 *          dialog frame).
 *
 *      2)  In your "init page" callback, when CBI_INIT
 *          comes in, call this function on the array of
 *          DLGHITEM which represents the controls to
 *          be added.
 *
 *      3)  For dynamic string loading, set each dialog
 *          item's pcszText to LOADSTRING, which will
 *          cause this function to load the string
 *          resources for each dialog item from the NLS
 *          DLL. It is assumed that the NLS string has
 *          the same ID as the control.
 *
 *@@added V0.9.16 (2001-09-29) [umoeller]
 */

APIRET ntbFormatPage(HWND hwndDlg,              // in: dialog frame to work on
                     PDLGHITEM paDlgItems,      // in: definition array
                     ULONG cDlgItems)           // in: array item count (NOT array size)
{
    APIRET arc;

    cmnLoadDialogStrings(paDlgItems, cDlgItems);

    // go create the controls
    if (!(arc = dlghFormatDlg(hwndDlg,
                              paDlgItems,
                              cDlgItems,
                              cmnQueryDefaultFont(),
                              DFFL_CREATECONTROLS | DFFL_RESIZEFRAME)))
    {
        // make Warp 4 notebook buttons and move controls
        // (this was already called in ntbInitPage on WM_INITDLG,
        // but at that point the init callback wasn't called yet...)
        winhAssertWarp4Notebook(hwndDlg,
                                100,         // ID threshold
                                14);
    }

    return (arc);
}

/*
 *@@ ntbQueryOpenPages:
 *      this function returns the CREATENOTEBOOKPAGE
 *      structures for currently open notebook pages, which
 *      are maintained by ntbInsertPage and ntb_fnwpPageCommon.
 *      This way you can iterate over all open pages and call
 *      the callbacks of certain pages to have pages updated,
 *      if necessary.
 *
 *      If (pcnpb == NULL), the first open page is returned;
 *      otherwise, the page which follows after pcnbp in
 *      our internal list.
 *
 *      In order to identify pages properly, you should always
 *      set unique ulPageID identifiers when inserting notebook
 *      pages and evaluate somSelf, if these are instance pages.
 *
 *      Warning: This function returns all pages which have
 *      been inserted using ntbInsertPage. This does not
 *      necessarily mean that the INIT callback has been
 *      invoked on the page yet. You should check pcnbp->fPageVisible
 *      for the current visibility of the page returned by
 *      this function.
 *
 *@@changed V0.9.0 [umoeller]: adjusted for new linklist functions
 *@@changed V0.9.0 [umoeller]: fixed an endless loop problem
 *@@changed V0.9.7 (2000-12-10) [umoeller]: fixed mutex problems
 */

PCREATENOTEBOOKPAGE ntbQueryOpenPages(PCREATENOTEBOOKPAGE pcnbp)
{
    PLISTNODE           pNode = 0;
    PNOTEBOOKPAGELISTITEM   pItemReturn = 0;
    BOOL                fSemOwned = FALSE;

    TRY_QUIET(excpt1)
    {
        // list created yet?
        if (G_pllOpenPages)
        {
            if (fSemOwned = LockNotebooks())
            {
                pNode = lstQueryFirstNode(G_pllOpenPages);

                if (pcnbp == NULL)
                {
                    // pcnbp == NULL: return first item
                    if (pNode)
                        pItemReturn = (PNOTEBOOKPAGELISTITEM)pNode->pItemData;
                }
                else
                    // pcnbp given: search for that page
                    while (pNode)
                    {
                        PNOTEBOOKPAGELISTITEM pItem = (PNOTEBOOKPAGELISTITEM)pNode->pItemData;
                        if (pItem->pcnbp == pcnbp)
                        {
                            // page found: return next
                            pNode = pNode->pNext;
                            if (pNode)
                                pItemReturn = (PNOTEBOOKPAGELISTITEM)pNode->pItemData;
                            break;
                        }

                        pNode = pNode->pNext;
                    }
            } // end if (fSemOwned)
            else
                cmnLog(__FILE__, __LINE__, __FUNCTION__,
                       "hmtxNotebookLists mutex request failed");
        } // end if (pllOpenPages)
    }
    CATCH(excpt1) { } END_CATCH();

    if (fSemOwned)
    {
        UnlockNotebooks();
        fSemOwned = FALSE;
    }

    if (pItemReturn)
        return (pItemReturn->pcnbp);
    else
        return (NULL);
}

/*
 *@@ ntbUpdateVisiblePage:
 *      this will go thru all currently open notebook
 *      pages (which are maintained by ntb_fnwpPageCommon
 *      in a linked list) and update a page (by calling its
 *      "init" callback with CBI_SET | CBI_ENABLE) if it
 *      matches the specified criteria.
 *
 *      These criteria are:
 *      --  somSelf         must match CREATENOTEBOOKPAGE.somSelf
 *      --  ulPageID        must match CREATENOTEBOOKPAGE.ulPageID
 *
 *      If any of these criteria is NULL, it's considered
 *      a "don't care", i.e. it is not checked for.
 *      A page is only updated if it's currently visible,
 *      i.e. turned to in an open settings notebook.
 *
 *      <B>Example:</B> If a certain XWorkplace feature is disabled
 *      in "XWorkplace Setup", we will need to disable controls
 *      in all open folder instance settings notebooks which are
 *      related to that feature. So we can call ntbUpdateVisiblePage
 *      with somSelf == NULL (all objects) and ulPageID == the
 *      folder page ID which has these controls, and there we go.
 *
 *      Returns the number of pages that were updated.
 *
 *@@changed V0.9.3 (2000-04-24) [umoeller]: finally made this thread-safe; now sending XNTBM_UPDATE
 */

ULONG ntbUpdateVisiblePage(WPObject *somSelf, ULONG ulPageID)
{
    ULONG ulrc = 0;
    PCREATENOTEBOOKPAGE pcnbp = NULL;

    while (pcnbp = ntbQueryOpenPages(pcnbp))
    {
        if (pcnbp->fPageVisible)
        {
            if (    (   (ulPageID == 0)     // don't care?
                     || (pcnbp->ulPageID == ulPageID)
                    )
                 && (   (somSelf == NULL)   // don't care?
                     || (pcnbp->somSelf == somSelf)
                    )
               )
            {
                WinSendMsg(pcnbp->hwndDlgPage,
                           XNTBM_UPDATE,
                           (MPARAM)(CBI_SET | CBI_ENABLE),
                           0);
                ulrc++;
            }
        }
    } // while (pcnbp = ntbQueryOpenPages(pcnbp))
    return (ulrc);
}

/*
 * ntbDisplayFocusHelp:
 *      this is used from all kinds of settings dlg procs to
 *      display help panels.
 *
 *      This will either display a sub-panel for the dialog
 *      control which currently has the keyboard focus or a
 *      "global" help page for the whole dialog.
 *
 *      This works as follows:
 *
 *      1)  First, the dialog item which currently has the
 *          keyboard focus is queried.
 *
 *      2)  We then attempt to open a help panel which
 *          corresponds to that ID, depending on the
 *          usFirstControlID and ulFirstSubpanel parameters:
 *          the ID of the focused control is queried,
 *          usFirstControlID is subtracted, and the result
 *          is added to ulFirstSubpanel (to get the help panel
 *          which is to be opened).
 *
 *          For example, if the control with the ID
 *          (usFirstControlID + 1) currently has the focus,
 *          (usFirstSubpanel + 1) will be displayed.
 *
 *          You should therefore assign the control IDs on
 *          the page in ascending order and order your help
 *          panels accordingly.
 *
 *      3)  If that fails (or if ulFirstSubpanel == 0),
 *          ulPanelIfNotFound will be displayed instead, which
 *          should be the help panel for the whole dialog.
 *
 *      This returns FALSE if step 3) failed also.
 *
 *changed V0.9.0 [umoeller]: functionality altered completely
 *
 *      This code was no longer really used and thus removed.
 *      V0.9.16 (2001-10-23) [umoeller]
 */

/* BOOL ntbDisplayFocusHelp(WPObject *somSelf,         // in: input for wpDisplayHelp
                         USHORT usFirstControlID,   // in: first dialog item ID
                         ULONG ulFirstSubpanel,     // in: help panel ID which corresponds to usFirstControlID
                         ULONG ulPanelIfNotFound)   // in: subsidiary help panel ID
{
    BOOL brc = TRUE;
    const char *pszHelpLibrary = cmnQueryHelpLibrary();

    HWND hwndFocus = WinQueryFocus(HWND_DESKTOP);
    if ((hwndFocus) && (somSelf))
    {

        USHORT  idItem = WinQueryWindowUShort(hwndFocus, QWS_ID);
        BOOL    fOpenPagePanel = FALSE;

        // attempt to display subpanel

        if (ulFirstSubpanel != 0)
        {
            if (!_wpDisplayHelp(somSelf,
                                (idItem-usFirstControlID) + ulFirstSubpanel,
                                (PSZ)pszHelpLibrary))
                fOpenPagePanel = TRUE;
        }
        else
            fOpenPagePanel = TRUE;

        if (fOpenPagePanel)
        {
            #ifdef DEBUG_NOTEBOOKS
                _Pmpf(( "ntbDisplayFocusHelp: not found, displaying superpanel %d", ulPanelIfNotFound ));
            #endif

            // didn't work: display page panel
            if (!_wpDisplayHelp(somSelf,
                                ulPanelIfNotFound,
                                (PSZ)pszHelpLibrary))
            {
                // still errors: complain
                cmnHelpNotFound(ulPanelIfNotFound);
                brc = FALSE;
            }
        }
    }
    return (brc);
} */


