
/*
 *@@sourcefile pagemage.c:
 *      daemon/hook interface for PageMage.
 *
 *      Function prefix for this file:
 *      --  pfmi*: PageMage interface.
 *
 *      This file is ALL new with V0.9.0.
 *
 *@@added V0.9.3 (2000-04-08) [umoeller]
 *@@header "config\pagemage.h"
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
#define INCL_WINBUTTONS
#define INCL_WINLISTBOXES
#define INCL_WINSTDSLIDER
#define INCL_GPILOGCOLORTABLE
#include <os2.h>

// C library headers
#include <stdio.h>

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\gpih.h"               // GPI helper routines
#include "helpers\winh.h"               // PM helper routines

// SOM headers which don't crash with prec. header files
#include "xwpscreen.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling

// headers in /hook
#include "hook\xwphook.h"

#include "config\pagemage.h"            // PageMage interface

#pragma hdrstop

/* ******************************************************************
 *                                                                  *
 *   Global variables                                               *
 *                                                                  *
 ********************************************************************/

PFNWP   G_pfnwpOrigStatic = NULL;

/* ******************************************************************
 *                                                                  *
 *   PageMage (XWPScreen) notebook functions (notebook.c)           *
 *                                                                  *
 ********************************************************************/

/*
 *@@ LoadPageMageConfig:
 *
 *@@added V0.9.3 (2000-05-21) [umoeller]
 */

BOOL LoadPageMageConfig(PAGEMAGECONFIG* pPgmgConfig)
{
    ULONG cb = sizeof(PAGEMAGECONFIG);
    memset(pPgmgConfig, 0, sizeof(PAGEMAGECONFIG));
    // overwrite from INI, if found
    return (PrfQueryProfileData(HINI_USER,
                                INIAPP_XWPHOOK,
                                INIKEY_HOOK_PGMGCONFIG,
                                pPgmgConfig,
                                &cb));
}

/*
 *@@ SavePageMageConfig:
 *
 *@@added V0.9.3 (2000-04-09) [umoeller]
 */

VOID SavePageMageConfig(PAGEMAGECONFIG* pPgmgConfig,
                        ULONG ulFlags)  // in: PGMGCFG_* flags (xwphook.h)
{
    // settings changed:
    // 1) write back to OS2.INI
    if (PrfWriteProfileData(HINI_USER,
                            INIAPP_XWPHOOK,
                            INIKEY_HOOK_PGMGCONFIG,
                            pPgmgConfig,
                            sizeof(PAGEMAGECONFIG)))
    {
        // 2) notify daemon, but do this delayed, because
        //    page mage config changes may overload the
        //    system; the thread-1 object window starts
        //    a timer for this...
        krnPostThread1ObjectMsg(T1M_PAGEMAGECONFIGDELAYED,
                                (MPARAM)ulFlags,
                                0);
    }
}

/*
 *@@ pgmiPageMage1InitPage:
 *      notebook callback function (notebook.c) for the
 *      first "PageMage" page in the "Screen" settings object.
 *      Sets the controls on the page according to the
 *      Global Settings.
 */

VOID pgmiPageMage1InitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                           ULONG flFlags)        // CBI_* flags (notebook.h)
{
    if (flFlags & CBI_INIT)
    {
        if (pcnbp->pUser == 0)
        {
            // first call: create PAGEMAGECONFIG
            // structure;
            // this memory will be freed automatically by the
            // common notebook window function (notebook.c) when
            // the notebook page is destroyed
            pcnbp->pUser = malloc(sizeof(PAGEMAGECONFIG));
            if (pcnbp->pUser)
                LoadPageMageConfig(pcnbp->pUser);

            // make backup for "undo"
            pcnbp->pUser2 = malloc(sizeof(PAGEMAGECONFIG));
            if (pcnbp->pUser2)
                memcpy(pcnbp->pUser2, pcnbp->pUser, sizeof(PAGEMAGECONFIG));
        }

        winhSetSliderTicks(WinWindowFromID(pcnbp->hwndDlgPage, ID_SCDI_PGMG1_X_SLIDER),
                           0, 3);      // six pixels high
        winhSetSliderTicks(WinWindowFromID(pcnbp->hwndDlgPage, ID_SCDI_PGMG1_Y_SLIDER),
                           0, 3);      // six pixels high
    }

    if (flFlags & CBI_SET)
    {
        PAGEMAGECONFIG* pPgmgConfig = (PAGEMAGECONFIG*)pcnbp->pUser;

        // sliders
        winhSetSliderArmPosition(WinWindowFromID(pcnbp->hwndDlgPage, ID_SCDI_PGMG1_X_SLIDER),
                                 SMA_INCREMENTVALUE,
                                 pPgmgConfig->ptlMaxDesktops.x - 1);
        winhSetSliderArmPosition(WinWindowFromID(pcnbp->hwndDlgPage, ID_SCDI_PGMG1_Y_SLIDER),
                                 SMA_INCREMENTVALUE,
                                 pPgmgConfig->ptlMaxDesktops.y - 1);

        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_SCDI_PGMG1_TITLEBAR,
                              pPgmgConfig->fShowTitlebar);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_SCDI_PGMG1_PRESERVEPROPS,
                              pPgmgConfig->fPreserveProportions);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_SCDI_PGMG1_STAYONTOP,
                              pPgmgConfig->fStayOnTop);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_SCDI_PGMG1_FLASHTOTOP,
                              pPgmgConfig->fFlash);

        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_SCDI_PGMG1_SHOWWINDOWS,
                              pPgmgConfig->fMirrorWindows);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_SCDI_PGMG1_SHOWWINTITLES,
                              pPgmgConfig->fShowWindowText);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_SCDI_PGMG1_CLICK2ACTIVATE,
                              pPgmgConfig->fClick2Activate);

        // hotkeys
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_SCDI_PGMG1_ARROWHOTKEYS,
                              pPgmgConfig->fEnableArrowHotkeys);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_SCDI_PGMG1_HOTKEYS_CTRL,
                              ((pPgmgConfig->ulKeyShift & KC_CTRL) != 0));
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_SCDI_PGMG1_HOTKEYS_SHIFT,
                              ((pPgmgConfig->ulKeyShift & KC_SHIFT) != 0));
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_SCDI_PGMG1_HOTKEYS_ALT,
                              ((pPgmgConfig->ulKeyShift & KC_ALT) != 0));
    }

    if (flFlags & CBI_ENABLE)
    {
        BOOL fEnable = winhIsDlgItemChecked(pcnbp->hwndDlgPage, ID_SCDI_PGMG1_SHOWWINDOWS);
        WinEnableControl(pcnbp->hwndDlgPage, ID_SCDI_PGMG1_SHOWWINTITLES,
                         fEnable);
        WinEnableControl(pcnbp->hwndDlgPage, ID_SCDI_PGMG1_CLICK2ACTIVATE,
                         fEnable);

        // hotkeys
        fEnable = winhIsDlgItemChecked(pcnbp->hwndDlgPage, ID_SCDI_PGMG1_ARROWHOTKEYS);
        WinEnableControl(pcnbp->hwndDlgPage, ID_SCDI_PGMG1_HOTKEYS_CTRL,
                         fEnable);
        WinEnableControl(pcnbp->hwndDlgPage, ID_SCDI_PGMG1_HOTKEYS_SHIFT,
                         fEnable);
        WinEnableControl(pcnbp->hwndDlgPage, ID_SCDI_PGMG1_HOTKEYS_ALT,
                         fEnable);
    }
}

/*
 *@@ pgmiPageMage1ItemChanged:
 *      notebook callback function (notebook.c) for the
 *      first "PageMage" page in the "Screen" settings object.
 *      Reacts to changes of any of the dialog controls.
 */

MRESULT pgmiPageMage1ItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                 USHORT usItemID, USHORT usNotifyCode,
                                 ULONG ulExtra)      // for checkboxes: contains new state
{
    MRESULT mrc = 0;
    BOOL    fSave = TRUE;      // save settings per default; this is set to FALSE if not needed
    ULONG   ulPgmgChangedFlags = 0;

    // access settings
    PAGEMAGECONFIG* pPgmgConfig = (PAGEMAGECONFIG*)pcnbp->pUser;

    switch (usItemID)
    {
        case ID_SCDI_PGMG1_X_SLIDER:
        {
            LONG lSliderIndex = winhQuerySliderArmPosition(pcnbp->hwndControl,
                                                           SMA_INCREMENTVALUE);

            WinSetDlgItemShort(pcnbp->hwndDlgPage,
                               ID_SCDI_PGMG1_X_TEXT2,
                               lSliderIndex + 1,
                               FALSE);      // unsigned

            LoadPageMageConfig(pcnbp->pUser);
            pPgmgConfig->ptlMaxDesktops.x = lSliderIndex + 1;
            ulPgmgChangedFlags = PGMGCFG_REPAINT | PGMGCFG_REFORMAT;
        break; }

        case ID_SCDI_PGMG1_Y_SLIDER:
        {
            LONG lSliderIndex = winhQuerySliderArmPosition(pcnbp->hwndControl,
                                                           SMA_INCREMENTVALUE);

            WinSetDlgItemShort(pcnbp->hwndDlgPage,
                               ID_SCDI_PGMG1_Y_TEXT2,
                               lSliderIndex + 1,
                               FALSE);      // unsigned

            LoadPageMageConfig(pcnbp->pUser);
            pPgmgConfig->ptlMaxDesktops.y = lSliderIndex + 1;
            ulPgmgChangedFlags = PGMGCFG_REPAINT | PGMGCFG_REFORMAT;
        break; }

        case ID_SCDI_PGMG1_TITLEBAR:
            LoadPageMageConfig(pcnbp->pUser);
            pPgmgConfig->fShowTitlebar = ulExtra;
            ulPgmgChangedFlags = PGMGCFG_REFORMAT;
        break;

        case ID_SCDI_PGMG1_PRESERVEPROPS:
            LoadPageMageConfig(pcnbp->pUser);
            pPgmgConfig->fPreserveProportions = ulExtra;
            ulPgmgChangedFlags = PGMGCFG_REFORMAT;
        break;

        case ID_SCDI_PGMG1_STAYONTOP:
            LoadPageMageConfig(pcnbp->pUser);
            pPgmgConfig->fStayOnTop = ulExtra;
        break;

        case ID_SCDI_PGMG1_FLASHTOTOP:
            LoadPageMageConfig(pcnbp->pUser);
            pPgmgConfig->fFlash = ulExtra;
        break;

        case ID_SCDI_PGMG1_SHOWWINDOWS:
            LoadPageMageConfig(pcnbp->pUser);
            pPgmgConfig->fMirrorWindows = ulExtra;
            ulPgmgChangedFlags = PGMGCFG_REPAINT;
        break;

        case ID_SCDI_PGMG1_SHOWWINTITLES:
            LoadPageMageConfig(pcnbp->pUser);
            pPgmgConfig->fShowWindowText = ulExtra;
            ulPgmgChangedFlags = PGMGCFG_REPAINT;
        break;

        case ID_SCDI_PGMG1_CLICK2ACTIVATE:
            LoadPageMageConfig(pcnbp->pUser);
            pPgmgConfig->fClick2Activate = ulExtra;
        break;

        case ID_SCDI_PGMG1_ARROWHOTKEYS:
            LoadPageMageConfig(pcnbp->pUser);
            pPgmgConfig->fEnableArrowHotkeys = ulExtra;
            (pcnbp->pfncbInitPage)(pcnbp, CBI_ENABLE);
        break;

        case ID_SCDI_PGMG1_HOTKEYS_CTRL:
        case ID_SCDI_PGMG1_HOTKEYS_SHIFT:
        case ID_SCDI_PGMG1_HOTKEYS_ALT:
        {
            ULONG ulOldKeyShift;
            LoadPageMageConfig(pcnbp->pUser);
            ulOldKeyShift = pPgmgConfig->ulKeyShift;

            pPgmgConfig->ulKeyShift = 0;
            if (winhIsDlgItemChecked(pcnbp->hwndDlgPage, ID_SCDI_PGMG1_HOTKEYS_CTRL))
                 pPgmgConfig->ulKeyShift |= KC_CTRL;
            if (winhIsDlgItemChecked(pcnbp->hwndDlgPage, ID_SCDI_PGMG1_HOTKEYS_SHIFT))
                 pPgmgConfig->ulKeyShift |= KC_SHIFT;
            if (winhIsDlgItemChecked(pcnbp->hwndDlgPage, ID_SCDI_PGMG1_HOTKEYS_ALT))
                 pPgmgConfig->ulKeyShift |= KC_ALT;

            if (pPgmgConfig->ulKeyShift == 0)
            {
                // no modifiers enabled: we really shouldn't allow this,
                // so restore the old value
                pPgmgConfig->ulKeyShift = ulOldKeyShift;
                WinAlarm(HWND_DESKTOP, WA_ERROR);
                (pcnbp->pfncbInitPage)(pcnbp, CBI_SET | CBI_ENABLE);
            }
        break; }

        /*
         * DID_DEFAULT:
         *
         */

        case DID_DEFAULT:
            LoadPageMageConfig(pcnbp->pUser);
            pPgmgConfig->ptlMaxDesktops.x = 3;
            pPgmgConfig->ptlMaxDesktops.y = 2;
            pPgmgConfig->fStayOnTop = FALSE;
            pPgmgConfig->fFlash = FALSE;
            pPgmgConfig->fShowTitlebar = TRUE;
            pPgmgConfig->fPreserveProportions = TRUE;
            pPgmgConfig->fMirrorWindows = TRUE;
            pPgmgConfig->fShowWindowText = TRUE;
            pPgmgConfig->fClick2Activate = TRUE;
            pPgmgConfig->fEnableArrowHotkeys = TRUE;
            pPgmgConfig->ulKeyShift = KC_CTRL | KC_ALT;

            // call INIT callback to reinitialize page
            (pcnbp->pfncbInitPage)(pcnbp, CBI_SET | CBI_ENABLE);

            ulPgmgChangedFlags = PGMGCFG_REPAINT | PGMGCFG_REFORMAT;
        break;

        /*
         * DID_UNDO:
         *
         */

        case DID_UNDO:
        {
            PAGEMAGECONFIG* pBackup = (PAGEMAGECONFIG*)pcnbp->pUser2;

            LoadPageMageConfig(pcnbp->pUser);
            pPgmgConfig->ptlMaxDesktops.x = pBackup->ptlMaxDesktops.x;
            pPgmgConfig->ptlMaxDesktops.y = pBackup->ptlMaxDesktops.y;
            pPgmgConfig->fShowTitlebar = pBackup->fShowTitlebar;
            pPgmgConfig->fPreserveProportions = pBackup->fPreserveProportions;
            pPgmgConfig->fStayOnTop = pBackup->fStayOnTop;
            pPgmgConfig->fFlash = pBackup->fFlash;
            pPgmgConfig->fMirrorWindows = pBackup->fMirrorWindows;
            pPgmgConfig->fShowWindowText = pBackup->fShowWindowText;
            pPgmgConfig->fClick2Activate = pBackup->fClick2Activate;
            pPgmgConfig->fEnableArrowHotkeys = pBackup->fEnableArrowHotkeys;
            pPgmgConfig->ulKeyShift = pBackup->ulKeyShift;

            // call INIT callback to reinitialize page
            (pcnbp->pfncbInitPage)(pcnbp, CBI_SET | CBI_ENABLE);

            ulPgmgChangedFlags = PGMGCFG_REPAINT | PGMGCFG_REFORMAT;
        break; }

        default:
            fSave = FALSE;
        break;
    }

    if (    (fSave)
         && (pcnbp->fPageInitialized)   // page initialized yet?
       )
    {
        SavePageMageConfig(pPgmgConfig,
                           ulPgmgChangedFlags);
    }

    return (mrc);
}

/*
 *@@ GetColorPointer:
 *
 *@@added V0.9.3 (2000-04-09) [umoeller]
 */

PLONG GetColorPointer(HWND hwndStatic,
                      PAGEMAGECONFIG* pPgmgConfig)
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
 *      frames representing PageMage colors.
 *
 *@@added V0.9.3 (2000-04-09) [umoeller]
 */

MRESULT EXPENTRY pgmi_fnwpSubclassedStaticRect(HWND hwndStatic, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;
    // access settings; these have been stored in QWL_USER
    PCREATENOTEBOOKPAGE pcnbp
            = (PCREATENOTEBOOKPAGE)WinQueryWindowPtr(hwndStatic, QWL_USER);
    // get PAGEMAGECONFIG which has been stored in user param there
    PAGEMAGECONFIG* pPgmgConfig = (PAGEMAGECONFIG*)pcnbp->pUser;

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
            WinQueryWindowRect(hwndStatic, &rclPaint);
            plColor = GetColorPointer(hwndStatic, pPgmgConfig);
            if (plColor)
                WinFillRect(hps, &rclPaint, *plColor);
            else
                WinFillRect(hps, &rclPaint, CLR_BLACK); // shouldn't happen...

            gpihBox(hps, DRO_OUTLINE, &rclPaint, CLR_BLACK);

            WinEndPaint(hps);
        break; }

        case WM_PRESPARAMCHANGED:
            switch ((ULONG)mp1)
            {
                case PP_BACKGROUNDCOLOR:
                {
                    PLONG plColor = GetColorPointer(hwndStatic, pPgmgConfig);
                    if (plColor)
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

                        if (pcnbp->fPageInitialized)   // page initialized yet?
                            SavePageMageConfig(pPgmgConfig,
                                               PGMGCFG_REPAINT);
                    }
                break; }
            }
        break;

        default:
            mrc = G_pfnwpOrigStatic(hwndStatic, msg, mp1, mp2);
        break;
    }

    return (mrc);
}

USHORT ausStaticFrameIDs[] = {
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
 *@@ pgmiPageMage2InitPage:
 *      notebook callback function (notebook.c) for the
 *      second "PageMage" page in the "Screen" settings object.
 *      Sets the controls on the page according to the
 *      Global Settings.
 */

VOID pgmiPageMage2InitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                           ULONG flFlags)        // CBI_* flags (notebook.h)
{
    if (flFlags & CBI_INIT)
    {
        if (pcnbp->pUser == 0)
        {
            ULONG ul = 0;

            // first call: create PAGEMAGECONFIG
            // structure;
            // this memory will be freed automatically by the
            // common notebook window function (notebook.c) when
            // the notebook page is destroyed
            pcnbp->pUser = malloc(sizeof(PAGEMAGECONFIG));
            if (pcnbp->pUser)
                LoadPageMageConfig(pcnbp->pUser);

            // make backup for "undo"
            pcnbp->pUser2 = malloc(sizeof(PAGEMAGECONFIG));
            if (pcnbp->pUser2)
                memcpy(pcnbp->pUser2, pcnbp->pUser, sizeof(PAGEMAGECONFIG));

            // subclass static rectangles
            for (ul = 0;
                 ul < sizeof(ausStaticFrameIDs) / sizeof(ausStaticFrameIDs[0]);
                 ul++)
            {
                HWND    hwndFrame = WinWindowFromID(pcnbp->hwndDlgPage,
                                                    ausStaticFrameIDs[ul]);
                // store pcnbp in QWL_USER of that control
                // so the control knows about its purpose and can
                // access the PAGEMAGECONFIG data
                WinSetWindowPtr(hwndFrame, QWL_USER, (PVOID)pcnbp);
                // subclass this control
                G_pfnwpOrigStatic = WinSubclassWindow(hwndFrame,
                                                      pgmi_fnwpSubclassedStaticRect);
            }
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
            HWND    hwndFrame = WinWindowFromID(pcnbp->hwndDlgPage,
                                                ausStaticFrameIDs[ul]);
            WinInvalidateRect(hwndFrame, NULL, FALSE);
        }
    }
}

/*
 *@@ pgmiPageMage2ItemChanged:
 *      notebook callback function (notebook.c) for the
 *      first "PageMage" page in the "Screen" settings object.
 *      Reacts to changes of any of the dialog controls.
 */

MRESULT pgmiPageMage2ItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                 USHORT usItemID, USHORT usNotifyCode,
                                 ULONG ulExtra)      // for checkboxes: contains new state
{
    MRESULT mrc = 0;

    switch (usItemID)
    {
        /*
         * DID_DEFAULT:
         *
         */

        case DID_DEFAULT:
        {
            PAGEMAGECONFIG* pPgmgConfig = (PAGEMAGECONFIG*)pcnbp->pUser;
            LoadPageMageConfig(pcnbp->pUser);
            pPgmgConfig->lcNormal = RGBCOL_DARKBLUE;
            pPgmgConfig->lcCurrent = RGBCOL_BLUE;
            pPgmgConfig->lcDivider = RGBCOL_GRAY;
            pPgmgConfig->lcNormalApp = RGBCOL_WHITE;
            pPgmgConfig->lcCurrentApp = RGBCOL_GREEN;
            pPgmgConfig->lcAppBorder = RGBCOL_BLACK;
            pPgmgConfig->lcTxtNormalApp = RGBCOL_BLACK;
            pPgmgConfig->lcTxtCurrentApp = RGBCOL_BLACK;

            // call INIT callback to reinitialize page
            (pcnbp->pfncbInitPage)(pcnbp, CBI_SET | CBI_ENABLE);

            SavePageMageConfig(pPgmgConfig,
                               PGMGCFG_REPAINT | PGMGCFG_REFORMAT);
        break; }

        /*
         * DID_UNDO:
         *
         */

        case DID_UNDO:
        {
            PAGEMAGECONFIG* pPgmgConfig = (PAGEMAGECONFIG*)pcnbp->pUser;
            PAGEMAGECONFIG* pBackup = (PAGEMAGECONFIG*)pcnbp->pUser2;

            LoadPageMageConfig(pcnbp->pUser);
            pPgmgConfig->lcNormal = pBackup->lcNormal;
            pPgmgConfig->lcCurrent = pBackup->lcCurrent;
            pPgmgConfig->lcDivider = pBackup->lcDivider;
            pPgmgConfig->lcNormalApp = pBackup->lcNormalApp;
            pPgmgConfig->lcCurrentApp = pBackup->lcCurrentApp;
            pPgmgConfig->lcAppBorder = pBackup->lcAppBorder;
            pPgmgConfig->lcTxtNormalApp = pBackup->lcTxtNormalApp;
            pPgmgConfig->lcTxtCurrentApp = pBackup->lcTxtCurrentApp;

            // call INIT callback to reinitialize page
            (pcnbp->pfncbInitPage)(pcnbp, CBI_SET | CBI_ENABLE);

            SavePageMageConfig(pPgmgConfig,
                               PGMGCFG_REPAINT | PGMGCFG_REFORMAT);
        break; }
    }

    return (mrc);
}
