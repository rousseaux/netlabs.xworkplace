
/*
 *@@sourcefile pager.c:
 *      daemon/hook interface for XPager.
 *
 *      Function prefix for this file:
 *      --  pfmi*: XPager interface.
 *
 *      This file is ALL new with V0.9.0.
 *
 *@@added V0.9.3 (2000-04-08) [umoeller]
 *@@header "config\pager.h"
 */

/*
 *      Copyright (C) 2000-2002 Ulrich M”ller.
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
#define INCL_WINSHELLDATA       // Prf* functions
#define INCL_WINWINDOWMGR
#define INCL_WININPUT
#define INCL_WINPOINTERS
#define INCL_WINSYS
#define INCL_WINMENUS
#define INCL_WINDIALOGS
#define INCL_WINSTATICS
#define INCL_WINBUTTONS
#define INCL_WINLISTBOXES
#define INCL_WINENTRYFIELDS
#define INCL_WINSTDCNR
#define INCL_WINSTDSLIDER
#define INCL_WINSTDSPIN
#define INCL_WINSTDVALSET
#define INCL_WINSWITCHLIST
#define INCL_GPILOGCOLORTABLE
#include <os2.h>

// C library headers
#include <stdio.h>

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\cnrh.h"               // container helper routines
#include "helpers\dialog.h"             // dialog helpers
#include "helpers\gpih.h"               // GPI helper routines
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\regexp.h"             // extended regular expressions
#include "helpers\standards.h"          // some standard macros
#include "helpers\stringh.h"            // string helper routines
#include "helpers\threads.h"            // thread helpers
#include "helpers\winh.h"               // PM helper routines

// SOM headers which don't crash with prec. header files
#include "xwpscreen.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\helppanels.h"          // all XWorkplace help panel IDs
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling

// headers in /hook
#include "hook\xwphook.h"

#include "config\pager.h"            // XPager interface

#pragma hdrstop

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

static PFNWP   G_pfnwpOrigStatic = NULL;

/* ******************************************************************
 *
 *   XPager (XWPScreen) helpers
 *
 ********************************************************************/

#ifndef __NOPAGER__

/*
 *@@ LoadXPagerConfig:
 *
 *@@added V0.9.3 (2000-05-21) [umoeller]
 */

static BOOL LoadXPagerConfig(PAGERCONFIG* pPgmgConfig)
{
    ULONG cb = sizeof(PAGERCONFIG);
    memset(pPgmgConfig, 0, sizeof(PAGERCONFIG));
    // overwrite from INI, if found
    return (PrfQueryProfileData(HINI_USER,
                                INIAPP_XWPHOOK,
                                INIKEY_HOOK_PGMGCONFIG,
                                pPgmgConfig,
                                &cb));
}

/*
 *@@ SaveXPagerConfig:
 *
 *@@added V0.9.3 (2000-04-09) [umoeller]
 */

static VOID SaveXPagerConfig(PAGERCONFIG* pPgmgConfig,
                               ULONG ulFlags)  // in: PGMGCFG_* flags (xwphook.h)
{
    // settings changed:
    // 1) write back to OS2.INI
    if (PrfWriteProfileData(HINI_USER,
                            INIAPP_XWPHOOK,
                            INIKEY_HOOK_PGMGCONFIG,
                            pPgmgConfig,
                            sizeof(PAGERCONFIG)))
    {
        // 2) notify daemon, but do this delayed, because
        //    page mage config changes may overload the
        //    system; the thread-1 object window starts
        //    a timer for this...
        //    after that, the daemon sends XDM_PAGERCONFIG
        //    to the daemon.
        krnPostThread1ObjectMsg(T1M_PAGERCONFIGDELAYED,
                                (MPARAM)ulFlags,
                                0);
    }
}

#endif

/* ******************************************************************
 *
 *   XPager General page notebook functions (notebook.c)
 *
 ********************************************************************/

#ifndef __NOPAGER__

/*
 *@@ UpdateValueSet:
 *
 *@@added V0.9.9 (2001-03-15) [lafaix]
 */

static VOID UpdateValueSet(HWND hwndValueSet,
                           PAGERCONFIG *pPgmgConfig)
{
   int row, col;
   BOOL bValid;

   for (row = 1; row <= 10; row++)
   {
       for (col = 1; col <= 10; col++)
       {
           bValid = (col <= pPgmgConfig->ptlMaxDesktops.x)
                 && (row <= pPgmgConfig->ptlMaxDesktops.y);

           WinSendMsg(hwndValueSet,
                      VM_SETITEM,
                      MPFROM2SHORT(row, col),
                      MPFROMLONG(bValid ? pPgmgConfig->lcNormal
                                        : pPgmgConfig->lcDivider));

           WinSendMsg(hwndValueSet,
                      VM_SETITEMATTR,
                      MPFROM2SHORT(row, col),
                      MPFROM2SHORT(VIA_DISABLED, !bValid));
       }
   }

   // highlight startup desktop
   WinSendMsg(hwndValueSet,
              VM_SETITEM,
              MPFROM2SHORT(min(pPgmgConfig->ptlStartDesktop.y,
                               pPgmgConfig->ptlMaxDesktops.y),
                           min(pPgmgConfig->ptlStartDesktop.x,
                               pPgmgConfig->ptlMaxDesktops.x)),
              MPFROMLONG(pPgmgConfig->lcCurrent));
}

/*
 *@@ pgmiXPagerGeneralInitPage:
 *      notebook callback function (notebook.c) for the
 *      first "XPager" page in the "Screen" settings object.
 *      Sets the controls on the page according to the
 *      Global Settings.
 *
 *@@changed V0.9.4 (2000-07-11) [umoeller]: fixed window flashing
 *@@changed V0.9.4 (2000-07-11) [umoeller]: added window flashing delay
 *@@changed V0.9.9 (2001-03-15) [lafaix]: "window" part moved to pgmiXPagerWindowInitPage
 */

static VOID pgmiXPagerGeneralInitPage(PNOTEBOOKPAGE pnbp,   // notebook info struct
                                      ULONG flFlags)        // CBI_* flags (notebook.h)
{
    if (flFlags & CBI_INIT)
    {
        // first call: create PAGERCONFIG
        // structure;
        // this memory will be freed automatically by the
        // common notebook window function (notebook.c) when
        // the notebook page is destroyed
        if (pnbp->pUser = malloc(sizeof(PAGERCONFIG)))
            LoadXPagerConfig(pnbp->pUser);

        // make backup for "undo"
        if (pnbp->pUser2 = malloc(sizeof(PAGERCONFIG)))
            memcpy(pnbp->pUser2, pnbp->pUser, sizeof(PAGERCONFIG));

        winhSetSliderTicks(WinWindowFromID(pnbp->hwndDlgPage, ID_SCDI_PGMG1_X_SLIDER),
                           (MPARAM)0, 3,
                           (MPARAM)-1, -1);
        winhSetSliderTicks(WinWindowFromID(pnbp->hwndDlgPage, ID_SCDI_PGMG1_Y_SLIDER),
                           (MPARAM)0, 3,
                           (MPARAM)-1, -1);
    }

    if (flFlags & CBI_SET)
    {
        PAGERCONFIG* pPgmgConfig = (PAGERCONFIG*)pnbp->pUser;

        // sliders
        winhSetSliderArmPosition(WinWindowFromID(pnbp->hwndDlgPage, ID_SCDI_PGMG1_X_SLIDER),
                                 SMA_INCREMENTVALUE,
                                 pPgmgConfig->ptlMaxDesktops.x - 1);
        winhSetSliderArmPosition(WinWindowFromID(pnbp->hwndDlgPage, ID_SCDI_PGMG1_Y_SLIDER),
                                 SMA_INCREMENTVALUE,
                                 pPgmgConfig->ptlMaxDesktops.y - 1);

        // valueset
        UpdateValueSet(WinWindowFromID(pnbp->hwndDlgPage,
                                       ID_SCDI_PGMG1_VALUESET),
                       pPgmgConfig);

        // hotkeys
        winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_SCDI_PGMG1_ARROWHOTKEYS,
                              pPgmgConfig->fEnableArrowHotkeys);
        winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_SCDI_PGMG1_HOTKEYS_CTRL,
                              ((pPgmgConfig->ulKeyShift & KC_CTRL) != 0));
        winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_SCDI_PGMG1_HOTKEYS_SHIFT,
                              ((pPgmgConfig->ulKeyShift & KC_SHIFT) != 0));
        winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_SCDI_PGMG1_HOTKEYS_ALT,
                              ((pPgmgConfig->ulKeyShift & KC_ALT) != 0));
        winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_SCDI_PGMG1_WRAPAROUND,
                              ((pPgmgConfig->bWrapAround) ? TRUE : FALSE));

    }

    if (flFlags & CBI_ENABLE)
    {
        BOOL fEnable = winhIsDlgItemChecked(pnbp->hwndDlgPage, ID_SCDI_PGMG1_ARROWHOTKEYS);
        winhEnableDlgItem(pnbp->hwndDlgPage, ID_SCDI_PGMG1_HOTKEYS_CTRL,
                         fEnable);
        winhEnableDlgItem(pnbp->hwndDlgPage, ID_SCDI_PGMG1_HOTKEYS_SHIFT,
                         fEnable);
        winhEnableDlgItem(pnbp->hwndDlgPage, ID_SCDI_PGMG1_HOTKEYS_ALT,
                         fEnable);
    }
}

/*
 *@@ pgmiXPagerGeneralItemChanged:
 *      notebook callback function (notebook.c) for the
 *      first "XPager" page in the "Screen" settings object.
 *      Reacts to changes of any of the dialog controls.
 *
 *@@changed V0.9.4 (2000-07-11) [umoeller]: fixed window flashing
 *@@changed V0.9.4 (2000-07-11) [umoeller]: added window flashing delay
 *@@changed V0.9.9 (2001-03-15) [lafaix]: "window" part moved to pgmiXPagerWindowItemChanged
 *@@changed V0.9.9 (2001-03-15) [lafaix]: fixed odd undo/default behavior
 */

static MRESULT pgmiXPagerGeneralItemChanged(PNOTEBOOKPAGE pnbp,
                                            ULONG ulItemID, USHORT usNotifyCode,
                                            ULONG ulExtra)      // for checkboxes: contains new state
{
    MRESULT mrc = 0;
    BOOL    fSave = TRUE;      // save settings per default; this is set to FALSE if not needed
    ULONG   ulPgmgChangedFlags = 0;

    // access settings
    PAGERCONFIG* pPgmgConfig = (PAGERCONFIG*)pnbp->pUser;

    switch (ulItemID)
    {
        case ID_SCDI_PGMG1_X_SLIDER:
        {
            LONG lSliderIndex = winhQuerySliderArmPosition(pnbp->hwndControl,
                                                           SMA_INCREMENTVALUE);

            WinSetDlgItemShort(pnbp->hwndDlgPage,
                               ID_SCDI_PGMG1_X_TEXT2,
                               lSliderIndex + 1,
                               FALSE);      // unsigned

            LoadXPagerConfig(pnbp->pUser);
            pPgmgConfig->ptlMaxDesktops.x = lSliderIndex + 1;
            ulPgmgChangedFlags = PGMGCFG_REPAINT | PGMGCFG_REFORMAT;
            UpdateValueSet(WinWindowFromID(pnbp->hwndDlgPage,
                                           ID_SCDI_PGMG1_VALUESET),
                           pPgmgConfig);
        }
        break;

        case ID_SCDI_PGMG1_Y_SLIDER:
        {
            LONG lSliderIndex = winhQuerySliderArmPosition(pnbp->hwndControl,
                                                           SMA_INCREMENTVALUE);

            WinSetDlgItemShort(pnbp->hwndDlgPage,
                               ID_SCDI_PGMG1_Y_TEXT2,
                               lSliderIndex + 1,
                               FALSE);      // unsigned

            LoadXPagerConfig(pnbp->pUser);
            pPgmgConfig->ptlMaxDesktops.y = lSliderIndex + 1;
            ulPgmgChangedFlags = PGMGCFG_REPAINT | PGMGCFG_REFORMAT;
            UpdateValueSet(WinWindowFromID(pnbp->hwndDlgPage,
                                           ID_SCDI_PGMG1_VALUESET),
                           pPgmgConfig);
        }
        break;

        case ID_SCDI_PGMG1_WRAPAROUND:
            LoadXPagerConfig(pnbp->pUser);
            pPgmgConfig->bWrapAround = ulExtra;
        break;

        case ID_SCDI_PGMG1_VALUESET:
            if (usNotifyCode == VN_ENTER)
            {
                pPgmgConfig->ptlStartDesktop.x = SHORT2FROMMP((MPARAM)ulExtra);
                pPgmgConfig->ptlStartDesktop.y = SHORT1FROMMP((MPARAM)ulExtra);
                UpdateValueSet(pnbp->hwndControl,
                               pPgmgConfig);
            }
        break;

        case ID_SCDI_PGMG1_ARROWHOTKEYS:
            LoadXPagerConfig(pnbp->pUser);
            pPgmgConfig->fEnableArrowHotkeys = ulExtra;
            pnbp->inbp.pfncbInitPage(pnbp, CBI_ENABLE);
        break;

        case ID_SCDI_PGMG1_HOTKEYS_CTRL:
        case ID_SCDI_PGMG1_HOTKEYS_SHIFT:
        case ID_SCDI_PGMG1_HOTKEYS_ALT:
        {
            ULONG ulOldKeyShift;
            LoadXPagerConfig(pnbp->pUser);
            ulOldKeyShift = pPgmgConfig->ulKeyShift;

            pPgmgConfig->ulKeyShift = 0;
            if (winhIsDlgItemChecked(pnbp->hwndDlgPage, ID_SCDI_PGMG1_HOTKEYS_CTRL))
                 pPgmgConfig->ulKeyShift |= KC_CTRL;
            if (winhIsDlgItemChecked(pnbp->hwndDlgPage, ID_SCDI_PGMG1_HOTKEYS_SHIFT))
                 pPgmgConfig->ulKeyShift |= KC_SHIFT;
            if (winhIsDlgItemChecked(pnbp->hwndDlgPage, ID_SCDI_PGMG1_HOTKEYS_ALT))
                 pPgmgConfig->ulKeyShift |= KC_ALT;

            if (pPgmgConfig->ulKeyShift == 0)
            {
                // no modifiers enabled: we really shouldn't allow this,
                // so restore the old value
                pPgmgConfig->ulKeyShift = ulOldKeyShift;
                WinAlarm(HWND_DESKTOP, WA_ERROR);
                pnbp->inbp.pfncbInitPage(pnbp, CBI_SET | CBI_ENABLE);
            }
        }
        break;

        /*
         * DID_DEFAULT:
         *
         *@@changed V0.9.9 (2001-03-15) [lafaix]: saves settings here
         */

        case DID_DEFAULT:
            LoadXPagerConfig(pnbp->pUser);
            pPgmgConfig->ptlMaxDesktops.x = 3;
            pPgmgConfig->ptlMaxDesktops.y = 2;
            pPgmgConfig->ptlStartDesktop.x = 1;
            pPgmgConfig->ptlStartDesktop.y = 1;
            pPgmgConfig->fEnableArrowHotkeys = TRUE;
            pPgmgConfig->ulKeyShift = KC_CTRL | KC_ALT;
            pPgmgConfig->bWrapAround = FALSE;

            ulPgmgChangedFlags = PGMGCFG_REPAINT | PGMGCFG_REFORMAT;

            SaveXPagerConfig(pPgmgConfig,
                               ulPgmgChangedFlags);

            // call INIT callback to reinitialize page
            pnbp->inbp.pfncbInitPage(pnbp, CBI_SET | CBI_ENABLE);

            fSave = FALSE;
        break;

        /*
         * DID_UNDO:
         *
         *@@changed V0.9.9 (2001-03-15) [lafaix]: saves settings here
         */

        case DID_UNDO:
        {
            PAGERCONFIG* pBackup = (PAGERCONFIG*)pnbp->pUser2;

            LoadXPagerConfig(pnbp->pUser);
            pPgmgConfig->ptlMaxDesktops.x = pBackup->ptlMaxDesktops.x;
            pPgmgConfig->ptlMaxDesktops.y = pBackup->ptlMaxDesktops.y;
            pPgmgConfig->ptlStartDesktop.x = pBackup->ptlStartDesktop.x;
            pPgmgConfig->ptlStartDesktop.y = pBackup->ptlStartDesktop.y;
            pPgmgConfig->fEnableArrowHotkeys = pBackup->fEnableArrowHotkeys;
            pPgmgConfig->ulKeyShift = pBackup->ulKeyShift;
            pPgmgConfig->bWrapAround = pBackup->bWrapAround;

            ulPgmgChangedFlags = PGMGCFG_REPAINT | PGMGCFG_REFORMAT;

            SaveXPagerConfig(pPgmgConfig,
                               ulPgmgChangedFlags);

            // call INIT callback to reinitialize page
            pnbp->inbp.pfncbInitPage(pnbp, CBI_SET | CBI_ENABLE);

            fSave = FALSE;
        }
        break;

        default:
            fSave = FALSE;
        break;
    }

    if (    (fSave)
         && (pnbp->fPageInitialized)   // page initialized yet?
       )
    {
        SaveXPagerConfig(pPgmgConfig,
                           ulPgmgChangedFlags);
    }

    return (mrc);
}

/* ******************************************************************
 *
 *   XPager Window page notebook functions (notebook.c)
 *
 ********************************************************************/

static const CONTROLDEF
    ControlWindowGroup = CONTROLDEF_GROUP(
                            LOAD_STRING,
                            ID_SCDI_PGMG1_WINDOW_GROUP,
                            -1,
                            -1),
    TitlebarCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,
                            ID_SCDI_PGMG1_TITLEBAR,
                            -1,
                            -1),
    PreservePropsCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,
                            ID_SCDI_PGMG1_PRESERVEPROPS,
                            -1,
                            -1),
    StayOnTopCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,
                            ID_SCDI_PGMG1_STAYONTOP,
                            -1,
                            -1),
    FlashToTopCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,
                            ID_SCDI_PGMG1_FLASHTOTOP,
                            -1,
                            -1),
    DelayTxt1 = CONTROLDEF_TEXT(
                            LOAD_STRING,
                            ID_SCDI_PGMG1_FLASH_TXT1,
                            -1,
                            -1),
    DelaySpin = CONTROLDEF_SPINBUTTON(
                            ID_SCDI_PGMG1_FLASH_SPIN,
                            50,
                            -1),
    DelayTxt2 = CONTROLDEF_TEXT(
                            LOAD_STRING,
                            ID_SCDI_PGMG1_FLASH_TXT2,
                            -1,
                            -1),
    MiniWindowsCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,
                            ID_SCDI_PGMG1_SHOWWINDOWS,
                            -1,
                            -1),
    ShowWinTitlesCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,
                            ID_SCDI_PGMG1_SHOWWINTITLES,
                            -1,
                            -1),
    Click2ActivateCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,
                            ID_SCDI_PGMG1_CLICK2ACTIVATE,
                            -1,
                            -1),
    ShowSecondaryCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,
                            ID_SCDI_PGMG1_SHOWSECONDARY,
                            -1,
                            -1),
    ShowStickyCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,
                            ID_SCDI_PGMG1_SHOWSTICKY,
                            -1,
                            -1);

static const DLGHITEM dlgXPagerWindow[] =
    {
        START_TABLE,
            START_ROW(0),
                START_GROUP_TABLE(&ControlWindowGroup),
                    START_ROW(0),
                        CONTROL_DEF(&TitlebarCB),
                    START_ROW(0),
                        CONTROL_DEF(&PreservePropsCB),
                    START_ROW(0),
                        CONTROL_DEF(&StayOnTopCB),
                    START_ROW(ROW_VALIGN_CENTER),
                        CONTROL_DEF(&FlashToTopCB),
                        CONTROL_DEF(&DelayTxt1),
                        CONTROL_DEF(&DelaySpin),
                        CONTROL_DEF(&DelayTxt2),
                    START_ROW(0),
                        CONTROL_DEF(&MiniWindowsCB),
                    START_ROW(0),
                        CONTROL_DEF(&G_Spacing),
                        CONTROL_DEF(&ShowWinTitlesCB),
                    START_ROW(0),
                        CONTROL_DEF(&G_Spacing),
                        CONTROL_DEF(&Click2ActivateCB),
                    START_ROW(0),
                        CONTROL_DEF(&G_Spacing),
                        CONTROL_DEF(&ShowSecondaryCB),
                    START_ROW(0),
                        CONTROL_DEF(&G_Spacing),
                        CONTROL_DEF(&ShowStickyCB),
                END_TABLE,
            START_ROW(0),       // notebook buttons (will be moved)
                CONTROL_DEF(&G_UndoButton),         // common.c
                CONTROL_DEF(&G_DefaultButton),      // common.c
                CONTROL_DEF(&G_HelpButton),         // common.c
        END_TABLE,
    };

/*
 *@@ pgmiXPagerWindowInitPage:
 *      notebook callback function (notebook.c) for the
 *      second "XPager" page in the "Screen" settings object.
 *      Sets the controls on the page according to the
 *      Global Settings.
 *
 *@@added V0.9.9 (2001-03-15) [lafaix]
 *@@changed V0.9.19 (2002-04-11) [lafaix]: added support for MDF_INCLUDE*
 */

static VOID pgmiXPagerWindowInitPage(PNOTEBOOKPAGE pnbp,   // notebook info struct
                                     ULONG flFlags)        // CBI_* flags (notebook.h)
{
    if (flFlags & CBI_INIT)
    {
        // first call: create PAGERCONFIG
        // structure;
        // this memory will be freed automatically by the
        // common notebook window function (notebook.c) when
        // the notebook page is destroyed
        if (pnbp->pUser = malloc(sizeof(PAGERCONFIG)))
            LoadXPagerConfig(pnbp->pUser);

        // make backup for "undo"
        if (pnbp->pUser2 = malloc(sizeof(PAGERCONFIG)))
            memcpy(pnbp->pUser2, pnbp->pUser, sizeof(PAGERCONFIG));

        // insert the controls using the dialog formatter
        // V0.9.19 (2002-04-17) [umoeller]
        ntbFormatPage(pnbp->hwndDlgPage,
                      dlgXPagerWindow,
                      ARRAYITEMCOUNT(dlgXPagerWindow));
    }

    if (flFlags & CBI_SET)
    {
        PAGERCONFIG* pPgmgConfig = (PAGERCONFIG*)pnbp->pUser;

        winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_SCDI_PGMG1_TITLEBAR,
                              pPgmgConfig->fShowTitlebar);
        winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_SCDI_PGMG1_PRESERVEPROPS,
                              pPgmgConfig->fPreserveProportions);
        winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_SCDI_PGMG1_STAYONTOP,
                              pPgmgConfig->fStayOnTop);

        // flash
        winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_SCDI_PGMG1_FLASHTOTOP,
                              pPgmgConfig->fFlash);
        winhSetDlgItemSpinData(pnbp->hwndDlgPage, ID_SCDI_PGMG1_FLASH_SPIN,
                               1, 30,       // min, max
                               pPgmgConfig->ulFlashDelay / 1000);  // current

        winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_SCDI_PGMG1_SHOWWINDOWS,
                              pPgmgConfig->fMirrorWindows);
        winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_SCDI_PGMG1_SHOWWINTITLES,
                              pPgmgConfig->fShowWindowText);
        winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_SCDI_PGMG1_CLICK2ACTIVATE,
                              pPgmgConfig->fClick2Activate);
        winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_SCDI_PGMG1_SHOWSECONDARY,
                              pPgmgConfig->ulMiniDisplayFlags & MDF_INCLUDESECONDARY);
        winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_SCDI_PGMG1_SHOWSTICKY,
                              pPgmgConfig->ulMiniDisplayFlags & MDF_INCLUDESTICKY);

    }

    if (flFlags & CBI_ENABLE)
    {
        BOOL fEnable = winhIsDlgItemChecked(pnbp->hwndDlgPage, ID_SCDI_PGMG1_SHOWWINDOWS);
        winhEnableDlgItem(pnbp->hwndDlgPage, ID_SCDI_PGMG1_SHOWWINTITLES,
                         fEnable);
        winhEnableDlgItem(pnbp->hwndDlgPage, ID_SCDI_PGMG1_CLICK2ACTIVATE,
                         fEnable);
        winhEnableDlgItem(pnbp->hwndDlgPage, ID_SCDI_PGMG1_SHOWSECONDARY,
                         fEnable);
        winhEnableDlgItem(pnbp->hwndDlgPage, ID_SCDI_PGMG1_SHOWSTICKY,
                         fEnable);

        // flash
        fEnable = winhIsDlgItemChecked(pnbp->hwndDlgPage, ID_SCDI_PGMG1_FLASHTOTOP);
        winhEnableDlgItem(pnbp->hwndDlgPage, ID_SCDI_PGMG1_FLASH_TXT1,
                         fEnable);
        winhEnableDlgItem(pnbp->hwndDlgPage, ID_SCDI_PGMG1_FLASH_SPIN,
                         fEnable);
        winhEnableDlgItem(pnbp->hwndDlgPage, ID_SCDI_PGMG1_FLASH_TXT2,
                         fEnable);

    }
}

/*
 *@@ pgmiXPagerWindowItemChanged:
 *      notebook callback function (notebook.c) for the
 *      second "XPager" page in the "Screen" settings object.
 *      Reacts to changes of any of the dialog controls.
 *
 *@@added V0.9.9 (2001-03-15) [lafaix]
 *@@changed V0.9.9 (2001-03-15) [lafaix]: fixed odd undo/default behavior
 *@@changed V0.9.19 (2002-04-11) [lafaix]: added support for MDF_INCLUDE*
 */

static MRESULT pgmiXPagerWindowItemChanged(PNOTEBOOKPAGE pnbp,
                                           ULONG ulItemID, USHORT usNotifyCode,
                                           ULONG ulExtra)      // for checkboxes: contains new state
{
    MRESULT mrc = 0;
    BOOL    fSave = TRUE;      // save settings per default; this is set to FALSE if not needed
    ULONG   ulPgmgChangedFlags = 0;

    // access settings
    PAGERCONFIG* pPgmgConfig = (PAGERCONFIG*)pnbp->pUser;

    switch (ulItemID)
    {
        case ID_SCDI_PGMG1_TITLEBAR:
            LoadXPagerConfig(pnbp->pUser);
            pPgmgConfig->fShowTitlebar = ulExtra;
            ulPgmgChangedFlags = PGMGCFG_REFORMAT | PGMGCFG_ZAPPO;
        break;

        case ID_SCDI_PGMG1_PRESERVEPROPS:
            LoadXPagerConfig(pnbp->pUser);
            pPgmgConfig->fPreserveProportions = ulExtra;
            ulPgmgChangedFlags = PGMGCFG_REFORMAT;
        break;

        case ID_SCDI_PGMG1_STAYONTOP:
            LoadXPagerConfig(pnbp->pUser);
            pPgmgConfig->fStayOnTop = ulExtra;
            ulPgmgChangedFlags = PGMGCFG_REFORMAT;
        break;

        case ID_SCDI_PGMG1_FLASHTOTOP:
            LoadXPagerConfig(pnbp->pUser);
            pPgmgConfig->fFlash = ulExtra;
            // call INIT callback to re-enable controls
            pnbp->inbp.pfncbInitPage(pnbp, CBI_ENABLE);
            ulPgmgChangedFlags = PGMGCFG_REFORMAT;
        break;

        case ID_SCDI_PGMG1_FLASH_SPIN:
            // delay spinbutton
            LoadXPagerConfig(pnbp->pUser);
            pPgmgConfig->ulFlashDelay = ulExtra * 1000;
            ulPgmgChangedFlags = PGMGCFG_REFORMAT;
        break;

        case ID_SCDI_PGMG1_SHOWWINDOWS:
            LoadXPagerConfig(pnbp->pUser);
            pPgmgConfig->fMirrorWindows = ulExtra;
            ulPgmgChangedFlags = PGMGCFG_REPAINT;
            pnbp->inbp.pfncbInitPage(pnbp, CBI_ENABLE);
        break;

        case ID_SCDI_PGMG1_SHOWWINTITLES:
            LoadXPagerConfig(pnbp->pUser);
            pPgmgConfig->fShowWindowText = ulExtra;
            ulPgmgChangedFlags = PGMGCFG_REPAINT;
        break;

        case ID_SCDI_PGMG1_SHOWSECONDARY:
            LoadXPagerConfig(pnbp->pUser);
            if (ulExtra)
                pPgmgConfig->ulMiniDisplayFlags |= MDF_INCLUDESECONDARY;
            else
                pPgmgConfig->ulMiniDisplayFlags &= ~MDF_INCLUDESECONDARY;
            ulPgmgChangedFlags = PGMGCFG_REPAINT;
        break;

        case ID_SCDI_PGMG1_SHOWSTICKY:
            LoadXPagerConfig(pnbp->pUser);
            if (ulExtra)
                pPgmgConfig->ulMiniDisplayFlags |= MDF_INCLUDESTICKY;
            else
                pPgmgConfig->ulMiniDisplayFlags &= ~MDF_INCLUDESTICKY;
            ulPgmgChangedFlags = PGMGCFG_REPAINT;
        break;

        case ID_SCDI_PGMG1_CLICK2ACTIVATE:
            LoadXPagerConfig(pnbp->pUser);
            pPgmgConfig->fClick2Activate = ulExtra;
        break;

        /*
         * DID_DEFAULT:
         *
         */

        case DID_DEFAULT:
            LoadXPagerConfig(pnbp->pUser);
            pPgmgConfig->fStayOnTop = FALSE;
            pPgmgConfig->fFlash = FALSE;
            pPgmgConfig->ulFlashDelay = 2000;
            pPgmgConfig->fShowTitlebar = TRUE;
            pPgmgConfig->fPreserveProportions = TRUE;
            pPgmgConfig->fMirrorWindows = TRUE;
            pPgmgConfig->fShowWindowText = TRUE;
            pPgmgConfig->fClick2Activate = TRUE;
            pPgmgConfig->ulMiniDisplayFlags = 0;

            SaveXPagerConfig(pPgmgConfig,
                               PGMGCFG_REPAINT | PGMGCFG_REFORMAT | PGMGCFG_ZAPPO);
            fSave = FALSE;      // V0.9.9 (2001-03-27) [umoeller]

            // call INIT callback to reinitialize page
            pnbp->inbp.pfncbInitPage(pnbp, CBI_SET | CBI_ENABLE);
        break;

        /*
         * DID_UNDO:
         *
         *changed V0.9.9 (2001-03-15) [lafaix]: save settings here
         */

        case DID_UNDO:
        {
            PAGERCONFIG* pBackup = (PAGERCONFIG*)pnbp->pUser2;

            LoadXPagerConfig(pnbp->pUser);
            pPgmgConfig->fShowTitlebar = pBackup->fShowTitlebar;
            pPgmgConfig->fPreserveProportions = pBackup->fPreserveProportions;
            pPgmgConfig->fStayOnTop = pBackup->fStayOnTop;
            pPgmgConfig->fFlash = pBackup->fFlash;
            pPgmgConfig->ulFlashDelay = pBackup->ulFlashDelay;
            pPgmgConfig->fMirrorWindows = pBackup->fMirrorWindows;
            pPgmgConfig->fShowWindowText = pBackup->fShowWindowText;
            pPgmgConfig->fClick2Activate = pBackup->fClick2Activate;
            pPgmgConfig->ulMiniDisplayFlags = pBackup->ulMiniDisplayFlags;

            SaveXPagerConfig(pPgmgConfig,
                               PGMGCFG_REPAINT | PGMGCFG_REFORMAT | PGMGCFG_ZAPPO);
                                        // fixed V0.9.9 (2001-03-27) [umoeller]
            fSave = FALSE;

            // call INIT callback to reinitialize page
            pnbp->inbp.pfncbInitPage(pnbp, CBI_SET | CBI_ENABLE);
        }
        break;

        default:
            fSave = FALSE;
        break;
    }

    if (    (fSave)
         && (pnbp->fPageInitialized)   // page initialized yet?
       )
    {
        SaveXPagerConfig(pPgmgConfig,
                           ulPgmgChangedFlags);
    }

    return (mrc);
}

/* ******************************************************************
 *
 *   XPager Sticky page notebook functions (notebook.c)
 *
 ********************************************************************/

/*
 *@@ STICKYRECORD:
 *      extended record core for "Sticky windows" container.
 *
 *@@added V0.9.4 (2000-07-10) [umoeller]
 *@@changed V0.9.19 (2002-04-14) [lafaix]: added pcszCriteria, pcszAttribute and pcszOperator
 */

typedef struct _STICKYRECORD
{
    RECORDCORE  recc;
    CHAR        szSticky[PGMG_TEXTLEN];
    PSZ         pcszCriteria;
    PSZ         pcszAttribute;
    PSZ         pcszOperator;
    PSZ         pcszValue;
    ULONG       ulFlags;
} STICKYRECORD, *PSTICKYRECORD;

/*
 *@@ AdjustStickyRecord:
 *      adjusts the pcsz* values in the STICKYRECORD
 *
 *@@added V0.9.19 (2002-04-15) [lafaix]
 *@@changed V0.9.19 (2002-04-17) [umoeller]: added regexp support
 */

static VOID AdjustStickyRecord(PSTICKYRECORD pRec)
{
    switch (pRec->ulFlags & SF_CRITERIA_MASK)
    {
        case SF_INCLUDE:
            pRec->pcszCriteria = cmnGetString(ID_SCDI_STICKY_INCLUDE);
        break;
        case SF_EXCLUDE:
            pRec->pcszCriteria = cmnGetString(ID_SCDI_STICKY_EXCLUDE);
        break;
    }
    switch (pRec->ulFlags & SF_OPERATOR_MASK)
    {
        case SF_CONTAINS:
            pRec->pcszOperator = cmnGetString(ID_SCDI_STICKY_CONTAINS);
        break;
        case SF_BEGINSWITH:
            pRec->pcszOperator = cmnGetString(ID_SCDI_STICKY_BEGINSWITH);
        break;
        case SF_ENDSWITH:
            pRec->pcszOperator = cmnGetString(ID_SCDI_STICKY_ENDSWITH);
        break;
        case SF_EQUALS:
            pRec->pcszOperator = cmnGetString(ID_SCDI_STICKY_EQUALS);
        break;
        case SF_MATCHES: // V0.9.19 (2002-04-17) [umoeller]
            pRec->pcszOperator = cmnGetString(ID_SCDI_STICKY_MATCHES);
        break;
    }

    // only one attribute supported so far, SF_TITLE
    pRec->pcszAttribute = cmnGetString(ID_SCDI_STICKY_TITLEATTRIBUTE);
    pRec->pcszValue = pRec->szSticky;
}

/*
 *@@ AddStickyRecord:
 *      creates and inserts a STICKYRECORD for the given
 *      container with the specified title.
 *
 *@@added V0.9.4 (2000-07-10) [umoeller]
 *@@changed V0.9.19 (2002-04-14) [lafaix]: added ulFlags support
 */

static VOID AddStickyRecord(HWND hwndCnr,
                            PSZ pszStickyName,     // in: window or switch list title (for XPager)
                            ULONG ulFlags,         // in: entry flags (SF_*)
                            BOOL fInvalidate)      // in: if TRUE, invalidate records
{
    PSTICKYRECORD pRec;

    if (pRec = (PSTICKYRECORD)cnrhAllocRecords(hwndCnr,
                                               sizeof(STICKYRECORD),
                                               1))
    {
        strhncpy0(pRec->szSticky, pszStickyName, PGMG_TEXTLEN);
        pRec->ulFlags = ulFlags;

        AdjustStickyRecord(pRec);

        cnrhInsertRecords(hwndCnr,
                          NULL, // parent
                          (PRECORDCORE)pRec,
                          fInvalidate,
                          pRec->szSticky,
                          CRA_RECORDREADONLY,
                          1);   // count
    }
}

/*
 *@@ SaveStickies:
 *      enumerates the STICKYRECORD's in the given
 *      container and updates XPager's stickies
 *      list. This calls SaveXPagerConfig in turn.
 *
 *@@added V0.9.4 (2000-07-10) [umoeller]
 *@@changed V0.9.19 (2002-04-14) [lafaix]: added flags support
 */

static VOID SaveStickies(HWND hwndCnr,
                         PAGERCONFIG* pPgmgConfig)
{
    PSTICKYRECORD   pRec = NULL;
    USHORT          usCmd = CMA_FIRST;
    BOOL            fCont = TRUE;
    USHORT          usStickyIndex = 0;      // raised with each iteration

    do
    {
        pRec = (PSTICKYRECORD)WinSendMsg(hwndCnr,
                                         CM_QUERYRECORD,
                                         pRec, // ignored on first call
                                         MPFROM2SHORT(usCmd,     // CMA_FIRST or CMA_NEXT
                                                      CMA_ITEMORDER));
        usCmd = CMA_NEXT;

        if ((pRec) && ((ULONG)pRec != -1))
        {
            pPgmgConfig->aulStickyFlags[usStickyIndex] = pRec->ulFlags;
            strcpy(pPgmgConfig->aszSticky[usStickyIndex],
                   pRec->szSticky);
            usStickyIndex++;
        }
        else
            fCont = FALSE;

    } while (fCont);

    // store stickies count
    pPgmgConfig->usStickyTextNum = usStickyIndex;

    SaveXPagerConfig(pPgmgConfig,
                       PGMGCFG_REPAINT
                         | PGMGCFG_REFORMAT
                         | PGMGCFG_STICKIES);
}

#define ATTRWIDTH       100
#define OPERWIDTH       100
#define VALUEWIDTH      200

static const CONTROLDEF
    CriteriaGroup = CONTROLDEF_GROUP(
                            LOAD_STRING,
                            ID_SCDI_STICKY_CRITERIAGROUP,
                            -1,
                            -1),
    AttrTxt = CONTROLDEF_TEXT_CENTER(
                            LOAD_STRING,
                            ID_SCDI_STICKY_ATTRIBUTE,
                            ATTRWIDTH,
                            -1),
    AttrList = CONTROLDEF_DROPDOWNLIST(
                            ID_SCDI_STICKY_ATTRIBUTE_DROP,
                            ATTRWIDTH,
                            150),
    OperTxt = CONTROLDEF_TEXT_CENTER(
                            LOAD_STRING,
                            ID_SCDI_STICKY_OPERATOR,
                            OPERWIDTH,
                            -1),
    OperList = CONTROLDEF_DROPDOWNLIST(
                            ID_SCDI_STICKY_OPERATOR_DROP,
                            OPERWIDTH,
                            150),
    ValueTxt = CONTROLDEF_TEXT_CENTER(
                            LOAD_STRING,
                            ID_SCDI_STICKY_VALUE,
                            VALUEWIDTH,
                            -1),
    ValueList = CONTROLDEF_DROPDOWN(
                            ID_SCDI_STICKY_VALUE_DROP,
                            VALUEWIDTH,
                            400),
    MatchingGroup = CONTROLDEF_GROUP(
                            LOAD_STRING, // ""Folder view settings"
                            ID_SCDI_STICKY_MATCHINGGROUP,
                            -1,
                            -1),
    IncludeRadio = CONTROLDEF_FIRST_AUTORADIO(
                            LOAD_STRING,
                            ID_SCDI_STICKY_RADIO_INCLUDE,
                            -1,
                            -1),
    ExcludeRadio = CONTROLDEF_NEXT_AUTORADIO(
                            LOAD_STRING,
                            ID_SCDI_STICKY_RADIO_EXCLUDE,
                            -1,
                            -1);

static const DLGHITEM dlgAddSticky[] =
    {
        START_TABLE,            // root table, required
            START_ROW(0),       // row 1 in the root table, required
                // create group on top
                START_GROUP_TABLE(&CriteriaGroup),
                    START_ROW(0),
                        START_TABLE,
                            START_ROW(0),
                                CONTROL_DEF(&AttrTxt),
                            START_ROW(0),
                                CONTROL_DEF(&AttrList),
                        END_TABLE,
                        START_TABLE,
                            START_ROW(0),
                                CONTROL_DEF(&OperTxt),
                            START_ROW(0),
                                CONTROL_DEF(&OperList),
                        END_TABLE,
                        START_TABLE,
                            START_ROW(0),
                                CONTROL_DEF(&ValueTxt),
                            START_ROW(0),
                                CONTROL_DEF(&ValueList),
                        END_TABLE,
                    START_ROW(0),
                        START_GROUP_TABLE(&MatchingGroup),
                            START_ROW(0),
                                CONTROL_DEF(&IncludeRadio),
                            START_ROW(0),
                                CONTROL_DEF(&ExcludeRadio),
                        END_TABLE,
                END_TABLE,
            START_ROW(0),       // notebook buttons (will be moved)
                CONTROL_DEF(&G_OKButton),           // common.c
                CONTROL_DEF(&G_CancelButton),       // common.c
                CONTROL_DEF(&G_HelpButton),         // common.c
        END_TABLE
    };

/*
 *@@ fnwpEditStickyRecord:
 *
 *@@added V0.9.19 (2002-04-17) [umoeller]
 */

static MRESULT EXPENTRY fnwpEditStickyRecord(HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    switch (msg)
    {
        case WM_COMMAND:
        {
            BOOL fDismiss = TRUE;
            HWND hwndCombo;

            if (    (SHORT1FROMMP(mp1) == DID_OK)
                 && (hwndCombo = WinWindowFromID(hwndDlg, ID_SCDI_STICKY_OPERATOR_DROP))
                 && (WinQueryLboxSelectedItem(hwndCombo) == 4)      // SF_MATCHES
                 // reassign for below
                 && (hwndCombo = WinWindowFromID(hwndDlg, ID_SCDI_STICKY_VALUE_DROP))
               )
            {
                // before allowing OK, check if the regexp is valid
                PSZ pszRegExp;
                if (pszRegExp = winhQueryWindowText(hwndCombo))
                {
                    ERE *ere;
                    int rc;
                    if (!(ere = rxpCompile(pszRegExp,
                                           0,
                                           &rc)))
                    {
                        cmnErrorMsgBox(hwndDlg,
                                    rc,
                                    234,
                                    MB_OK,
                                    TRUE);
                        fDismiss = FALSE;

                        WinSetFocus(HWND_DESKTOP, hwndCombo);
                    }

                    if (ere)
                        rxpFree(ere);

                    free(pszRegExp);
                }
                else
                    fDismiss = FALSE;
            }

            if (fDismiss)
                WinDismissDlg(hwndDlg, SHORT1FROMMP(mp1));
        }
        break;

        case WM_HELP:
            cmnDisplayHelp(NULL, ID_XSH_SETTINGS_PAGER_STICKY + 2);
        break;

        default:
            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
    }

    return mrc;
}

/*
 *@@ EditStickyRecord:
 *      edit and possibly inserts a STICKYRECORD.
 *
 *@@added V0.9.19 (2002-04-14) [lafaix]
 *@@changed V0.9.19 (2002-04-17) [umoeller]: now using dialog formatter, added regexps
 */

static VOID EditStickyRecord(PSTICKYRECORD pRec,
                             PNOTEBOOKPAGE pnbp,
                             HWND hwndCnr,
                             BOOL fInsert)
{
    HWND        hwndDlg;
    PDLGHITEM   paNew;

    if (!cmnLoadDialogStrings(dlgAddSticky,
                              ARRAYITEMCOUNT(dlgAddSticky),
                              &paNew))
    {
        if (!dlghCreateDlg(&hwndDlg,
                           pnbp->hwndDlgPage,
                           FCF_FIXED_DLG,
                           fnwpEditStickyRecord,
                           cmnGetString(fInsert
                                            ? ID_SCDI_STICKY_ADDTITLE
                                            : ID_SCDI_STICKY_EDITTITLE),
                           paNew,
                           ARRAYITEMCOUNT(dlgAddSticky),
                           NULL,
                           cmnQueryDefaultFont()))
        {
            PSWBLOCK    pSwBlock;

            winhCenterWindow(hwndDlg);

            // get all the tasklist entries into a buffer
            // V0.9.16 (2002-01-05) [umoeller]: now using winhQuerySwitchList
            if (pSwBlock = winhQuerySwitchList(WinQueryAnchorBlock(pnbp->hwndDlgPage)))
            {
                ULONG ul;
                HWND  hwndCombo = WinWindowFromID(hwndDlg, ID_SCDI_STICKY_ATTRIBUTE_DROP);

                // filling the possible attributes (just Title currently)
                WinInsertLboxItem(hwndCombo,
                                  0,
                                  cmnGetString(ID_SCDI_STICKY_TITLEATTRIBUTE));
                WinSendMsg(hwndCombo, LM_SELECTITEM, MPFROMSHORT(0), MPFROMSHORT(TRUE));

                // filling the possible operators
                hwndCombo = WinWindowFromID(hwndDlg, ID_SCDI_STICKY_OPERATOR_DROP);
                WinInsertLboxItem(hwndCombo, 0, cmnGetString(ID_SCDI_STICKY_CONTAINS));
                WinInsertLboxItem(hwndCombo, 1, cmnGetString(ID_SCDI_STICKY_BEGINSWITH));
                WinInsertLboxItem(hwndCombo, 2, cmnGetString(ID_SCDI_STICKY_ENDSWITH));
                WinInsertLboxItem(hwndCombo, 3, cmnGetString(ID_SCDI_STICKY_EQUALS));
                WinInsertLboxItem(hwndCombo, 4, cmnGetString(ID_SCDI_STICKY_MATCHES));

                switch (pRec->ulFlags & SF_OPERATOR_MASK)
                {
                    case SF_CONTAINS:
                        WinSendMsg(hwndCombo, LM_SELECTITEM, MPFROMSHORT(0), MPFROMSHORT(TRUE));
                    break;
                    case SF_BEGINSWITH:
                        WinSendMsg(hwndCombo, LM_SELECTITEM, MPFROMSHORT(1), MPFROMSHORT(TRUE));
                    break;
                    case SF_ENDSWITH:
                        WinSendMsg(hwndCombo, LM_SELECTITEM, MPFROMSHORT(2), MPFROMSHORT(TRUE));
                    break;
                    case SF_EQUALS:
                        WinSendMsg(hwndCombo, LM_SELECTITEM, MPFROMSHORT(3), MPFROMSHORT(TRUE));
                    break;
                    case SF_MATCHES:
                        WinSendMsg(hwndCombo, LM_SELECTITEM, MPFROMSHORT(4), MPFROMSHORT(TRUE));
                    break;
                }

                // loop through all the tasklist entries
                hwndCombo = WinWindowFromID(hwndDlg, ID_SCDI_STICKY_VALUE_DROP);
                for (ul = 0;
                     ul < pSwBlock->cswentry;
                     ul++)
                {
                    PSWCNTRL pCtrl = &pSwBlock->aswentry[ul].swctl;
                    if (    (strlen(pCtrl->szSwtitle))
                         && ((pCtrl->uchVisibility & SWL_VISIBLE) != 0) // V0.9.11 (2001-04-25) [umoeller]
                       )
                        WinInsertLboxItem(hwndCombo,
                                          LIT_SORTASCENDING,
                                          pCtrl->szSwtitle);
                }
                WinSetWindowText(hwndCombo, pRec->szSticky);

                WinCheckButton(hwndDlg,
                               ID_SCDI_STICKY_RADIO_INCLUDE,
                               (pRec->ulFlags & SF_CRITERIA_MASK) == SF_INCLUDE);
                WinCheckButton(hwndDlg,
                               ID_SCDI_STICKY_RADIO_EXCLUDE,
                               (pRec->ulFlags & SF_CRITERIA_MASK) == SF_EXCLUDE);

                if (WinProcessDlg(hwndDlg) == DID_OK)
                {
                    // OK pressed:
                    PSZ pszSticky;
                    if (pszSticky = winhQueryWindowText(hwndCombo))
                    {
                        ULONG ulFlags = 0;

                        // build flags from settings
                        if (WinQueryButtonCheckstate(hwndDlg,
                                                     ID_SCDI_STICKY_RADIO_EXCLUDE))
                            ulFlags |= SF_EXCLUDE;
                        hwndCombo = WinWindowFromID(hwndDlg, ID_SCDI_STICKY_OPERATOR_DROP);
                        switch (WinQueryLboxSelectedItem(hwndCombo))
                        {
                            case 1:
                                ulFlags |= SF_BEGINSWITH;
                            break;
                            case 2:
                                ulFlags |= SF_ENDSWITH;
                            break;
                            case 3:
                                ulFlags |= SF_EQUALS;
                            break;
                            case 4:
                                ulFlags |= SF_MATCHES;
                            break;
                        }
                        pRec->ulFlags = ulFlags;
                        strhncpy0(pRec->szSticky, pszSticky, PGMG_TEXTLEN);

                        if (fInsert)
                            AddStickyRecord(hwndCnr,
                                            pszSticky,
                                            ulFlags,
                                            TRUE);          // invalidate
                        else
                        {
                            // pRec is already in container
                            AdjustStickyRecord(pRec);

                            // invalidate container to refresh view
                            WinSendMsg(hwndCnr,
                                       CM_INVALIDATERECORD,
                                       (MPARAM)&pRec,
                                       MPFROM2SHORT(1,
                                                    CMA_TEXTCHANGED));
                        }

                        SaveStickies(hwndCnr,
                                     (PAGERCONFIG*)pnbp->pUser);
                        free(pszSticky);
                    }
                }

                free(pSwBlock);
            }

            WinDestroyWindow(hwndDlg);
        }

        free(paNew);
    }
}

static const CONTROLDEF
    StickiesGroup = CONTROLDEF_GROUP(
                            LOAD_STRING,
                            ID_SCDI_STICKY_GROUP,
                            -1,
                            -1),
    StickiesCnr = CONTROLDEF_CONTAINER(
                            ID_SCDI_STICKY_CNR,
                            400,        // for now, will be resized
                            200),       // for now, will be resized
    AddButton = CONTROLDEF_PUSHBUTTON(
                            LOAD_STRING,
                            DID_ADD,
                            100,
                            30),
    EditButton = CONTROLDEF_PUSHBUTTON(
                            LOAD_STRING,
                            DID_EDIT,
                            100,
                            30),
    RemoveButton = CONTROLDEF_PUSHBUTTON(
                            LOAD_STRING,
                            DID_REMOVE,
                            100,
                            30);

static const DLGHITEM dlgStickies[] =
    {
        START_TABLE,            // root table, required
            START_ROW(0),       // row 1 in the root table, required
                // create group on top
                START_GROUP_TABLE(&StickiesGroup),
                    START_ROW(0),
                        CONTROL_DEF(&StickiesCnr),
                    START_ROW(0),
                        CONTROL_DEF(&AddButton),
                        CONTROL_DEF(&EditButton),
                        CONTROL_DEF(&RemoveButton),
                END_TABLE,
            START_ROW(0),       // notebook buttons (will be moved)
                CONTROL_DEF(&G_UndoButton),         // common.c
                CONTROL_DEF(&G_DefaultButton),      // common.c
                CONTROL_DEF(&G_HelpButton),         // common.c
        END_TABLE
    };

MPARAM G_ampStickies[] =
    {
        MPFROM2SHORT(ID_SCDI_STICKY_CNR, XAC_SIZEX | XAC_SIZEY),
        MPFROM2SHORT(ID_SCDI_STICKY_GROUP, XAC_SIZEX | XAC_SIZEY),
    };

/*
 *@@ pgmiXPagerStickyInitPage:
 *      notebook callback function (notebook.c) for the
 *      "XPager Sticky Windows" page in the "Screen" settings object.
 *      Sets the controls on the page according to the
 *      Global Settings.
 *
 *@@added V0.9.4 (2000-07-10) [umoeller]
 *@@changed V0.9.19 (2002-04-14) [lafaix]: modified container view
 *@@changed V0.9.19 (2002-04-17) [umoeller]: now using dialog formatter
 */

static VOID pgmiXPagerStickyInitPage(PNOTEBOOKPAGE pnbp,   // notebook info struct
                                     ULONG flFlags)        // CBI_* flags (notebook.h)
{
    if (flFlags & CBI_INIT)
    {
        HWND hwndCnr;

        XFIELDINFO xfi[5];
        int        i = 0;

        // first call: create PAGERCONFIG
        // structure;
        // this memory will be freed automatically by the
        // common notebook window function (notebook.c) when
        // the notebook page is destroyed
        if (pnbp->pUser = malloc(sizeof(PAGERCONFIG)))
            LoadXPagerConfig(pnbp->pUser);

        // make backup for "undo"
        if (pnbp->pUser2 = malloc(sizeof(PAGERCONFIG)))
            memcpy(pnbp->pUser2, pnbp->pUser, sizeof(PAGERCONFIG));

        // insert the controls using the dialog formatter
        // V0.9.19 (2002-04-17) [umoeller]
        ntbFormatPage(pnbp->hwndDlgPage,
                      dlgStickies,
                      ARRAYITEMCOUNT(dlgStickies));

        hwndCnr = WinWindowFromID(pnbp->hwndDlgPage,
                                  ID_SCDI_STICKY_CNR);

        // set up cnr details view
        xfi[i].ulFieldOffset = FIELDOFFSET(STICKYRECORD, pcszCriteria);
        xfi[i].pszColumnTitle = cmnGetString(ID_SCDI_STICKY_CRITERIA);
        xfi[i].ulDataType = CFA_STRING | CFA_HORZSEPARATOR;
        xfi[i++].ulOrientation = CFA_LEFT;

        xfi[i].ulFieldOffset = FIELDOFFSET(STICKYRECORD, pcszAttribute);
        xfi[i].pszColumnTitle = cmnGetString(ID_SCDI_STICKY_ATTRIBUTE);
        xfi[i].ulDataType = CFA_STRING | CFA_HORZSEPARATOR;
        xfi[i++].ulOrientation = CFA_LEFT;

        xfi[i].ulFieldOffset = FIELDOFFSET(STICKYRECORD, pcszOperator);
        xfi[i].pszColumnTitle = cmnGetString(ID_SCDI_STICKY_OPERATOR);
        xfi[i].ulDataType = CFA_STRING | CFA_HORZSEPARATOR;
        xfi[i++].ulOrientation = CFA_LEFT;

        xfi[i].ulFieldOffset = FIELDOFFSET(STICKYRECORD, pcszValue);
        xfi[i].pszColumnTitle = cmnGetString(ID_SCDI_STICKY_VALUE);
        xfi[i].ulDataType = CFA_STRING | CFA_HORZSEPARATOR;
        xfi[i++].ulOrientation = CFA_LEFT;

        cnrhSetFieldInfos(hwndCnr,
                          xfi,
                          i,             // array item count
                          FALSE,         // don't draw lines
                          1);            // return first column

        BEGIN_CNRINFO()
        {
            cnrhSetView(CV_DETAIL | CA_DETAILSVIEWTITLES);
        } END_CNRINFO(hwndCnr);
    }

    if (flFlags & CBI_SET)
    {
        // initialize container with currently sticky windows
        PAGERCONFIG* pPgmgConfig = (PAGERCONFIG*)pnbp->pUser;
        USHORT          us;

        HWND hwndCnr = WinWindowFromID(pnbp->hwndDlgPage,
                                       ID_SCDI_STICKY_CNR);

        cnrhRemoveAll(hwndCnr);

        for (us = 0;
             us < pPgmgConfig->usStickyTextNum;
             us++)
        {
            AddStickyRecord(hwndCnr,
                            pPgmgConfig->aszSticky[us],
                            pPgmgConfig->aulStickyFlags[us],
                            FALSE);
            cnrhInvalidateAll(hwndCnr);
        }
    }

    if (flFlags & CBI_ENABLE)
    {
        PAGERCONFIG* pPgmgConfig = (PAGERCONFIG*)pnbp->pUser;

        winhEnableDlgItem(pnbp->hwndDlgPage,
                          DID_ADD,
                          pPgmgConfig->usStickyTextNum < MAX_STICKIES);
        winhEnableDlgItem(pnbp->hwndDlgPage,
                          DID_EDIT,
                          pPgmgConfig->usStickyTextNum != 0);
        winhEnableDlgItem(pnbp->hwndDlgPage,
                          DID_REMOVE,
                          pPgmgConfig->usStickyTextNum != 0);
    }
}

/*
 *@@ pgmiXPagerStickyItemChanged:
 *      notebook callback function (notebook.c) for the
 *      "XPager Sticky Windows" page in the "Screen" settings object.
 *      Reacts to changes of any of the dialog controls.
 *
 *@@added V0.9.4 (2000-07-10) [umoeller]
 *@@changed V0.9.11 (2001-04-25) [umoeller]: no longer listing invisible switchlist entries
 *@@changed V0.9.19 (2002-04-14) [lafaix]: using new sticky settings dialog
 *@@changed V0.9.19 (2002-04-16) [lafaix]: fixed popup menu font and DID_UNDO
 */

static MRESULT pgmiXPagerStickyItemChanged(PNOTEBOOKPAGE pnbp,
                                           ULONG ulItemID, USHORT usNotifyCode,
                                           ULONG ulExtra)      // for checkboxes: contains new state
{
    MRESULT mrc = 0;

    HWND hwndCnr = WinWindowFromID(pnbp->hwndDlgPage,
                                   ID_SCDI_STICKY_CNR);

    switch (ulItemID)
    {
        case ID_SCDI_STICKY_CNR:
            switch (usNotifyCode)
            {
                /*
                 * CN_CONTEXTMENU:
                 *
                 */

                case CN_CONTEXTMENU:
                {
                    HWND    hPopupMenu = NULLHANDLE;

                    // in the CREATENOTEBOOKPAGE structure
                    // so that the notebook.c function can
                    // remove source emphasis later automatically
                    pnbp->hwndSourceCnr = pnbp->hwndControl;
                    if (pnbp->preccSource = (PRECORDCORE)ulExtra)
                    {
                        // popup menu on container recc:
                        // disabling "add" item if sticky array full
                        if (hPopupMenu = WinLoadMenu(pnbp->hwndDlgPage, // owner
                                                     cmnQueryNLSModuleHandle(FALSE),
                                                     ID_XSM_STICKY_SEL))
                            WinEnableMenuItem(hPopupMenu,
                                              ID_XSMI_STICKY_NEW,
                                              (((PAGERCONFIG*)pnbp->pUser)->usStickyTextNum < MAX_STICKIES));
                    }
                    else
                    {
                        // popup menu on cnr whitespace
                        // disabling "add" item if sticky array full
                        if (hPopupMenu = WinLoadMenu(pnbp->hwndDlgPage, // owner
                                                     cmnQueryNLSModuleHandle(FALSE),
                                                     ID_XSM_STICKY_NOSEL))
                            WinEnableMenuItem(hPopupMenu,
                                              ID_XSMI_STICKY_NEW,
                                              (((PAGERCONFIG*)pnbp->pUser)->usStickyTextNum < MAX_STICKIES));
                    }

                    if (hPopupMenu)
                    {
                        // font stuff snarfed from ctr_engine.c
                        PSZ pszStdMenuFont;
                        if (!(pszStdMenuFont = prfhQueryProfileData(HINI_USER,
                                                                    PMINIAPP_SYSTEMFONTS, // "PM_SystemFonts",
                                                                    PMINIKEY_MENUSFONT, // "Menus",
                                                                    NULL)))
                            pszStdMenuFont = prfhQueryProfileData(HINI_USER,
                                                                  PMINIAPP_SYSTEMFONTS, // "PM_SystemFonts",
                                                                  PMINIKEY_DEFAULTFONT, // "DefaultFont",
                                                                  NULL);

                        if (pszStdMenuFont)
                        {
                            winhSetWindowFont(hPopupMenu,
                                              pszStdMenuFont);
                            free(pszStdMenuFont);
                        }

                        cnrhShowContextMenu(pnbp->hwndControl,  // cnr
                                            (PRECORDCORE)pnbp->preccSource,
                                            hPopupMenu,
                                            pnbp->hwndDlgPage);    // owner
                    }
                }
                break;

                /*
                 * CN_ENTER:
                 *
                 */

                case CN_ENTER:
                    EditStickyRecord((PSTICKYRECORD)ulExtra,
                                     pnbp,
                                     hwndCnr,
                                     FALSE); // do not create a new record
                break;
            }
        break;

        /*
         * ID_XSMI_STICKY_NEW:
         *      show "New sticky window" dialog and add
         *      a new sticky window from that dialog.
         */

        case DID_ADD:
        case ID_XSMI_STICKY_NEW:
        {
            STICKYRECORD rec;
            memset(&rec, 0, sizeof(rec));
            EditStickyRecord(&rec,
                             pnbp,
                             hwndCnr,
                             TRUE); // create a new record if needed
            pnbp->inbp.pfncbInitPage(pnbp, CBI_ENABLE);
        }
        break;

        /*
         * ID_XSMI_STICKY_EDIT:
         *      show "Edit sticky window entry" dialog and edit
         *      the entry from that dialog
         *      (menu item command).
         */

        case ID_XSMI_STICKY_EDIT:
            EditStickyRecord((PSTICKYRECORD)pnbp->preccSource,
                             pnbp,
                             hwndCnr,
                             FALSE); // do not create a new record
        break;

        /*
         * ID_SCDI_STICKY_EDIT
         *      show "Edit sticky window entry" dialog and edit
         *      the currently selected entry from that dialog
         *      (button command).
         */

        case DID_EDIT:
        {
            // get current selected record
            PSTICKYRECORD pRec;
            if (    (pRec = (PSTICKYRECORD)WinSendMsg(hwndCnr,
                                                      CM_QUERYRECORDEMPHASIS,
                                                      (MPARAM)CMA_FIRST,
                                                      (MPARAM)CRA_SELECTED))
                 && ((LONG)pRec != -1L)
               )
                EditStickyRecord(pRec,
                                 pnbp,
                                 hwndCnr,
                                 FALSE); // do not create a new record
        }
        break;

        /*
         * ID_XSMI_STICKY_DELETE:
         *      remove sticky window record
         *      (menu item command).
         */

        case ID_XSMI_STICKY_DELETE:
            WinSendMsg(hwndCnr,
                       CM_REMOVERECORD,
                       &(pnbp->preccSource), // double pointer...
                       MPFROM2SHORT(1, CMA_FREE | CMA_INVALIDATE));
            SaveStickies(hwndCnr,
                         (PAGERCONFIG*)pnbp->pUser);
            pnbp->inbp.pfncbInitPage(pnbp, CBI_ENABLE);
        break;

        /*
         * ID_SCDI_STICKY_REMOVE:
         *      remove the currently selected entry
         *      (button command).
         */

        case DID_REMOVE:
        {
            // get current selected record
            PSTICKYRECORD pRec;
            if (    (pRec = (PSTICKYRECORD)WinSendMsg(hwndCnr,
                                                      CM_QUERYRECORDEMPHASIS,
                                                      (MPARAM)CMA_FIRST,
                                                      (MPARAM)CRA_SELECTED))
                 && ((LONG)pRec != -1L)
               )
            {
                WinSendMsg(hwndCnr,
                           CM_REMOVERECORD,
                           &pRec, // double pointer...
                           MPFROM2SHORT(1, CMA_FREE | CMA_INVALIDATE));
                SaveStickies(hwndCnr,
                             (PAGERCONFIG*)pnbp->pUser);
                pnbp->inbp.pfncbInitPage(pnbp, CBI_ENABLE);
            }
        }
        break;

        /*
         * DID_UNDO:
         *      "Undo" button.
         */

        case DID_UNDO:
        {
            PAGERCONFIG* pPgmgConfig = (PAGERCONFIG*)pnbp->pUser;
            PAGERCONFIG* pBackup = (PAGERCONFIG*)pnbp->pUser2;
            USHORT       us;

            // overwrite entire string array with backup
            memcpy(pPgmgConfig->aszSticky,
                   pBackup->aszSticky,
                   sizeof(pPgmgConfig->aszSticky));
            // overwrite entire flags array
            memcpy(pPgmgConfig->aulStickyFlags,
                   pBackup->aulStickyFlags,
                   sizeof(pPgmgConfig->aulStickyFlags));
            // and count too
            pPgmgConfig->usStickyTextNum = pBackup->usStickyTextNum;

            // SaveXPagerConfig is cheaper that SaveStickies: we don't
            // have to update the container first
            SaveXPagerConfig(pPgmgConfig,
                             PGMGCFG_REPAINT
                               | PGMGCFG_REFORMAT
                               | PGMGCFG_STICKIES);

            // call INIT callback to reinitialize page
            pnbp->inbp.pfncbInitPage(pnbp, CBI_SET | CBI_ENABLE);
        }
        break;

        /*
         * DID_DEFAULT:
         *      "Clear" button.
         */

        case ID_XSMI_STICKY_DELETEALL:
        case DID_DEFAULT:
            cnrhRemoveAll(hwndCnr);
            SaveStickies(hwndCnr,
                         (PAGERCONFIG*)pnbp->pUser);
            pnbp->inbp.pfncbInitPage(pnbp, CBI_ENABLE);
        break;
    }

    return (mrc);
}

/* ******************************************************************
 *
 *   XPager Colors page notebook functions (notebook.c)
 *
 ********************************************************************/

/*
 *@@ GetColorPointer:
 *
 *@@added V0.9.3 (2000-04-09) [umoeller]
 */

static PLONG GetColorPointer(HWND hwndStatic,
                             PAGERCONFIG* pPgmgConfig)
{
    USHORT usID = WinQueryWindowUShort(hwndStatic, QWS_ID);
    switch (usID)
    {
        case ID_SCDI_PGMG2_DTP_INACTIVE:
            return (&pPgmgConfig->lcNormal);

        case ID_SCDI_PGMG2_DTP_ACTIVE:
            return (&pPgmgConfig->lcCurrent);

        case ID_SCDI_PGMG2_DTP_BORDER:
            return (&pPgmgConfig->lcDivider);

        case ID_SCDI_PGMG2_WIN_INACTIVE:
            return (&pPgmgConfig->lcNormalApp);

        case ID_SCDI_PGMG2_WIN_ACTIVE:
            return (&pPgmgConfig->lcCurrentApp);

        case ID_SCDI_PGMG2_WIN_BORDER:
            return (&pPgmgConfig->lcAppBorder);

        case ID_SCDI_PGMG2_TXT_INACTIVE:
            return (&pPgmgConfig->lcTxtNormalApp);

        case ID_SCDI_PGMG2_TXT_ACTIVE:
            return (&pPgmgConfig->lcTxtCurrentApp);
    }

    return (NULL);
}

/*
 *@@ pgmi_fnwpSubclassedStaticRect:
 *      common window procedure for subclassed static
 *      frames representing XPager colors.
 *
 *@@added V0.9.3 (2000-04-09) [umoeller]
 *@@changed V0.9.7 (2001-01-17) [umoeller]: fixed inclusive rect bug
 */

static MRESULT EXPENTRY pgmi_fnwpSubclassedStaticRect(HWND hwndStatic, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;
    // access settings; these have been stored in QWL_USER
    PNOTEBOOKPAGE pnbp = (PNOTEBOOKPAGE)WinQueryWindowPtr(hwndStatic, QWL_USER);
    // get PAGERCONFIG which has been stored in user param there
    PAGERCONFIG* pPgmgConfig = (PAGERCONFIG*)pnbp->pUser;

    switch (msg)
    {
        case WM_PAINT:
        {
            PLONG   plColor;
            RECTL   rclPaint;
            HPS hps = WinBeginPaint(hwndStatic,
                                    NULLHANDLE, // HPS
                                    NULL); // PRECTL
            gpihSwitchToRGB(hps);
            WinQueryWindowRect(hwndStatic,
                               &rclPaint);      // exclusive
            if (plColor = GetColorPointer(hwndStatic, pPgmgConfig))
            {
                // make rect inclusive
                rclPaint.xRight--;
                rclPaint.yTop--;

                // draw interior
                GpiSetColor(hps, *plColor);
                gpihBox(hps,
                        DRO_FILL,
                        &rclPaint);

                // draw frame
                GpiSetColor(hps, RGBCOL_BLACK);
                gpihBox(hps,
                        DRO_OUTLINE,
                        &rclPaint);
            }

            WinEndPaint(hps);
        }
        break;

        case WM_PRESPARAMCHANGED:
            switch ((ULONG)mp1)
            {
                case PP_BACKGROUNDCOLOR:
                {
                    PLONG plColor;
                    if (plColor = GetColorPointer(hwndStatic, pPgmgConfig))
                    {
                        ULONG   ul = 0,
                                attrFound = 0;

                        WinQueryPresParam(hwndStatic,
                                          (ULONG)mp1,
                                          0,
                                          &attrFound,
                                          (ULONG)sizeof(ul),
                                          (PVOID)&ul,
                                          0);
                        *plColor = ul;
                        WinInvalidateRect(hwndStatic,
                                          NULL, FALSE);

                        if (pnbp->fPageInitialized)   // page initialized yet?
                            SaveXPagerConfig(pPgmgConfig,
                                               PGMGCFG_REPAINT);
                    }
                }
                break;
            }
        break;

        default:
            mrc = G_pfnwpOrigStatic(hwndStatic, msg, mp1, mp2);
        break;
    }

    return (mrc);
}

static USHORT ausStaticFrameIDs[] =
    {
         ID_SCDI_PGMG2_DTP_INACTIVE,
         ID_SCDI_PGMG2_DTP_ACTIVE,
         ID_SCDI_PGMG2_DTP_BORDER,
         ID_SCDI_PGMG2_WIN_INACTIVE,
         ID_SCDI_PGMG2_WIN_ACTIVE,
         ID_SCDI_PGMG2_WIN_BORDER,
         ID_SCDI_PGMG2_TXT_INACTIVE,
         ID_SCDI_PGMG2_TXT_ACTIVE
    };

/*
 *@@ pgmiXPagerColorsInitPage:
 *      notebook callback function (notebook.c) for the
 *      "XPager Colors" page in the "Screen" settings object.
 *      Sets the controls on the page according to the
 *      Global Settings.
 */

static VOID pgmiXPagerColorsInitPage(PNOTEBOOKPAGE pnbp,   // notebook info struct
                                     ULONG flFlags)        // CBI_* flags (notebook.h)
{
    if (flFlags & CBI_INIT)
    {
        ULONG ul = 0;

        // first call: create PAGERCONFIG
        // structure;
        // this memory will be freed automatically by the
        // common notebook window function (notebook.c) when
        // the notebook page is destroyed
        if (pnbp->pUser = malloc(sizeof(PAGERCONFIG)))
            LoadXPagerConfig(pnbp->pUser);

        // make backup for "undo"
        if (pnbp->pUser2 = malloc(sizeof(PAGERCONFIG)))
            memcpy(pnbp->pUser2, pnbp->pUser, sizeof(PAGERCONFIG));

        // subclass static rectangles
        for (ul = 0;
             ul < sizeof(ausStaticFrameIDs) / sizeof(ausStaticFrameIDs[0]);
             ul++)
        {
            HWND    hwndFrame = WinWindowFromID(pnbp->hwndDlgPage,
                                                ausStaticFrameIDs[ul]);
            // store pcnbp in QWL_USER of that control
            // so the control knows about its purpose and can
            // access the PAGERCONFIG data
            WinSetWindowPtr(hwndFrame, QWL_USER, (PVOID)pnbp);
            // subclass this control
            G_pfnwpOrigStatic = WinSubclassWindow(hwndFrame,
                                                  pgmi_fnwpSubclassedStaticRect);
        }
    }

    if (flFlags & CBI_SET)
    {
        ULONG ul = 0;
        // repaint all static controls
        for (ul = 0;
             ul < sizeof(ausStaticFrameIDs) / sizeof(ausStaticFrameIDs[0]);
             ul++)
        {
            WinInvalidateRect(WinWindowFromID(pnbp->hwndDlgPage,
                                              ausStaticFrameIDs[ul]),
                              NULL,
                              FALSE);
        }
    }
}

/*
 *@@ pgmiXPagerColorsItemChanged:
 *      notebook callback function (notebook.c) for the
 *      "XPager Colors" page in the "Screen" settings object.
 *      Reacts to changes of any of the dialog controls.
 */

static MRESULT pgmiXPagerColorsItemChanged(PNOTEBOOKPAGE pnbp,
                                           ULONG ulItemID, USHORT usNotifyCode,
                                           ULONG ulExtra)      // for checkboxes: contains new state
{
    MRESULT mrc = 0;

    switch (ulItemID)
    {
        /*
         * DID_DEFAULT:
         *
         */

        case DID_DEFAULT:
        {
            PAGERCONFIG* pPgmgConfig = (PAGERCONFIG*)pnbp->pUser;
            LoadXPagerConfig(pnbp->pUser);
            pPgmgConfig->lcNormal = RGBCOL_DARKBLUE;
            pPgmgConfig->lcCurrent = RGBCOL_BLUE;
            pPgmgConfig->lcDivider = RGBCOL_GRAY;
            pPgmgConfig->lcNormalApp = RGBCOL_WHITE;
            pPgmgConfig->lcCurrentApp = RGBCOL_GREEN;
            pPgmgConfig->lcAppBorder = RGBCOL_BLACK;
            pPgmgConfig->lcTxtNormalApp = RGBCOL_BLACK;
            pPgmgConfig->lcTxtCurrentApp = RGBCOL_BLACK;

            // call INIT callback to reinitialize page
            pnbp->inbp.pfncbInitPage(pnbp, CBI_SET | CBI_ENABLE);

            SaveXPagerConfig(pPgmgConfig,
                               PGMGCFG_REPAINT | PGMGCFG_REFORMAT);
        }
        break;

        /*
         * DID_UNDO:
         *
         */

        case DID_UNDO:
        {
            PAGERCONFIG* pPgmgConfig = (PAGERCONFIG*)pnbp->pUser;
            PAGERCONFIG* pBackup = (PAGERCONFIG*)pnbp->pUser2;

            LoadXPagerConfig(pnbp->pUser);
            pPgmgConfig->lcNormal = pBackup->lcNormal;
            pPgmgConfig->lcCurrent = pBackup->lcCurrent;
            pPgmgConfig->lcDivider = pBackup->lcDivider;
            pPgmgConfig->lcNormalApp = pBackup->lcNormalApp;
            pPgmgConfig->lcCurrentApp = pBackup->lcCurrentApp;
            pPgmgConfig->lcAppBorder = pBackup->lcAppBorder;
            pPgmgConfig->lcTxtNormalApp = pBackup->lcTxtNormalApp;
            pPgmgConfig->lcTxtCurrentApp = pBackup->lcTxtCurrentApp;

            // call INIT callback to reinitialize page
            pnbp->inbp.pfncbInitPage(pnbp, CBI_SET | CBI_ENABLE);

            SaveXPagerConfig(pPgmgConfig,
                               PGMGCFG_REPAINT | PGMGCFG_REFORMAT);
        }
        break;
    }

    return (mrc);
}

/*
 *@@ pgmiInsertPagerPages:
 *      implementation for XWPScreen::xwpAddXWPScreenPages,
 *      as far as the pager config pages are concerned.
 *
 *@@added V0.9.19 (2002-04-17) [umoeller]
 *@@changed V0.9.19 (2002-04-19) [pr]: page wasn't showing "page 2/4" etc; fixed
 */

ULONG pgmiInsertPagerPages(WPObject *somSelf,       // in: screen object
                           HWND hwndDlg,            // in: notebook
                           HMODULE savehmod)        // in: NLS module
{
    INSERTNOTEBOOKPAGE  inbp;

    // "XPager" colors
    memset(&inbp, 0, sizeof(INSERTNOTEBOOKPAGE));
    inbp.somSelf = somSelf;
    inbp.hwndNotebook = hwndDlg;
    inbp.hmod = savehmod;
    inbp.pfncbInitPage    = pgmiXPagerColorsInitPage;
    inbp.pfncbItemChanged = pgmiXPagerColorsItemChanged;
    inbp.usPageStyleFlags = BKA_MINOR;
    inbp.fEnumerate = TRUE;
    // inbp.pcszName = "~XPager";
    inbp.ulDlgID = ID_SCD_PAGER_COLORS;
    inbp.ulDefaultHelpPanel  = ID_XSH_SETTINGS_PAGER_COLORS;
    // give this page a unique ID, which is
    // passed to the common config.sys callbacks
    inbp.ulPageID = SP_PAGER_COLORS;
    ntbInsertPage(&inbp);

    // "XPager" sticky windows
    memset(&inbp, 0, sizeof(INSERTNOTEBOOKPAGE));
    inbp.somSelf = somSelf;
    inbp.hwndNotebook = hwndDlg;
    inbp.hmod = savehmod;
    inbp.pfncbInitPage    = pgmiXPagerStickyInitPage;
    inbp.pfncbItemChanged = pgmiXPagerStickyItemChanged;
    inbp.usPageStyleFlags = BKA_MINOR;
    inbp.fEnumerate = TRUE;
    // inbp.pcszName = "~XPager";
    inbp.ulDlgID = ID_XFD_EMPTYDLG; // ID_SCD_PAGER_STICKY; V0.9.19 (2002-04-17) [umoeller]
    inbp.ulDefaultHelpPanel  = ID_XSH_SETTINGS_PAGER_STICKY;
    // make this sizeable V0.9.19 (2002-04-17) [umoeller]
    inbp.pampControlFlags = G_ampStickies;
    inbp.cControlFlags = ARRAYITEMCOUNT(G_ampStickies);
    // give this page a unique ID, which is
    // passed to the common config.sys callbacks
    inbp.ulPageID = SP_PAGER_STICKY;
    ntbInsertPage(&inbp);

    // "XPager" window settings V0.9.9 (2001-03-15) [lafaix]
    memset(&inbp, 0, sizeof(INSERTNOTEBOOKPAGE));
    inbp.somSelf = somSelf;
    inbp.hwndNotebook = hwndDlg;
    inbp.hmod = savehmod;
    inbp.pfncbInitPage    = pgmiXPagerWindowInitPage;
    inbp.pfncbItemChanged = pgmiXPagerWindowItemChanged;
    inbp.usPageStyleFlags = BKA_MINOR;
    inbp.fEnumerate = TRUE;
    // inbp.pcszName = "~XPager";
    inbp.ulDlgID = ID_XFD_EMPTYDLG; // ID_SCD_PAGER_WINDOW; V0.9.19 (2002-04-17) [umoeller]
    inbp.ulDefaultHelpPanel  = ID_XSH_SETTINGS_PAGER_WINDOW;
    // give this page a unique ID, which is
    // passed to the common config.sys callbacks
    inbp.ulPageID = SP_PAGER_WINDOW;
    ntbInsertPage(&inbp);

    // "XPager" general settings
    memset(&inbp, 0, sizeof(INSERTNOTEBOOKPAGE));
    inbp.somSelf = somSelf;
    inbp.hwndNotebook = hwndDlg;
    inbp.hmod = savehmod;
    inbp.pfncbInitPage    = pgmiXPagerGeneralInitPage;
    inbp.pfncbItemChanged = pgmiXPagerGeneralItemChanged;
    inbp.usPageStyleFlags = BKA_MAJOR;
    inbp.fEnumerate = TRUE;
    inbp.pcszName = "~XPager";
    inbp.ulDlgID = ID_SCD_PAGER_GENERAL;
    inbp.ulDefaultHelpPanel  = ID_XSH_SETTINGS_PAGER_GENERAL;
    // give this page a unique ID, which is
    // passed to the common config.sys callbacks
    inbp.ulPageID = SP_PAGER_MAIN;
    return ntbInsertPage(&inbp);
}

#endif // __NOPAGER__
