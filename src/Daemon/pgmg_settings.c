
/*
 *@@sourcefile pgmg_settings.c:
 *      PageMage configuration stuff.
 *
 *      See pgmg_control.c for an introduction to PageMage.
 */

/*
 *      Copyright (C) 1995-1999 Carlos Ugarte.
 *      Copyright (C) 2000 Ulrich M”ller.
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

#define INCL_WININPUT
#define INCL_WINSYS
#define INCL_WINSHELLDATA
#include <os2.h>

#include <stdio.h>
#include <process.h>

#include "setup.h"                      // code generation and debugging options

#include "helpers\gpih.h"               // GPI helper routines

#include "xwpapi.h"                     // public XWorkplace definitions

#include "hook\xwphook.h"
#include "hook\hook_private.h"
#include "hook\xwpdaemn.h"              // PageMage and daemon declarations

/*
 *@@ pgmsSetDefaults:
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 */

VOID pgmsSetDefaults(VOID)
{
    // shortcuts to global pagemage config
    PPAGEMAGECONFIG pPageMageConfig = &G_pHookData->PageMageConfig;
    PPOINTL         pptlMaxDesktops = &pPageMageConfig->ptlMaxDesktops;

    // first, set the fake desktop size... we no longer move windows
    // by the size of the PM screen (e.g. 1024x768), but instead we
    // use eight extra pixels, so we can get rid of the borders of
    // maximized windows.

    G_szlEachDesktopReal.cx = G_pHookData->lCXScreen + 8;
    G_szlEachDesktopReal.cy = G_pHookData->lCYScreen + 8;

    _Pmpf(("pgmsSetDefaults"));
    // Misc 1
    pptlMaxDesktops->x = 3;
    pptlMaxDesktops->y = 2;
    pPageMageConfig->ptlStartDesktop.x = 1;
    pPageMageConfig->ptlStartDesktop.y = 2;
    // pPageMageConfig->iEdgeBoundary = 5;
    // pPageMageConfig->ulSleepTime = 100;
    // pPageMageConfig->bRepositionMouse = TRUE;
    pPageMageConfig->fClick2Activate = TRUE;

    // Misc 2
    // pPageMageConfig->_bHoldWPS = TRUE;
    // pPageMageConfig->lPriority = 0;
    pPageMageConfig->fRecoverOnShutdown = TRUE;
    // pPageMageConfig->bSwitchToFocus = FALSE;

    // Display
    pPageMageConfig->fShowTitlebar = TRUE;
    // pPageMageConfig->fStartMinimized = FALSE;
    pPageMageConfig->fFlash = FALSE;
    pPageMageConfig->ulFlashDelay = 2000;
    // pPageMageConfig->bFloatToTop = FALSE;
    pPageMageConfig->fMirrorWindows = TRUE;
    pPageMageConfig->fShowWindowText = TRUE;
    pPageMageConfig->fPreserveProportions = TRUE;

    // Sticky
    memset(pPageMageConfig->aszSticky,
           0,
           sizeof(pPageMageConfig->aszSticky));
    pPageMageConfig->usStickyTextNum = 0;
    memset(pPageMageConfig->hwndSticky2,
           0,
           sizeof(pPageMageConfig->hwndSticky2));
    pPageMageConfig->usSticky2Num = 0;

    // Colors 1
    pPageMageConfig->lcNormal = RGBCOL_DARKBLUE;
    pPageMageConfig->lcCurrent = RGBCOL_BLUE;
    pPageMageConfig->lcDivider = RGBCOL_GRAY;

    pPageMageConfig->lcNormalApp = RGBCOL_WHITE;
    pPageMageConfig->lcCurrentApp = RGBCOL_GREEN;
    pPageMageConfig->lcAppBorder = RGBCOL_BLACK;

    pPageMageConfig->lcTxtNormalApp = RGBCOL_BLACK;
    pPageMageConfig->lcTxtCurrentApp = RGBCOL_BLACK;

    // Panning
    pPageMageConfig->bPanAtTop
      = pPageMageConfig->bPanAtBottom
      = pPageMageConfig->bPanAtLeft
      = pPageMageConfig->bPanAtRight = TRUE;
    pPageMageConfig->bWrapAround = FALSE;

    // Keyboard
    pPageMageConfig->fEnableArrowHotkeys = FALSE;
    pPageMageConfig->ulKeyShift = KC_CTRL | KC_ALT;

    // PageMage window position
    G_swpPgmgFrame.x = WinQuerySysValue(HWND_DESKTOP, SV_CXSIZEBORDER);
    G_swpPgmgFrame.y = WinQuerySysValue(HWND_DESKTOP, SV_CYSIZEBORDER);

    G_swpPgmgFrame.cx = G_swpPgmgFrame.x
                        + (.18 * G_pHookData->lCXScreen);
    G_swpPgmgFrame.cy = G_swpPgmgFrame.y
                        + pgmcCalcNewFrameCY(G_swpPgmgFrame.cx);
}

/*
 *@@ pgmsLoadSettings:
 *      loads the PageMage settings from OS2.INI,
 *      if present. Depending on flConfig, PageMage
 *      will reformat and/or re-adjust itself.
 *
 *@@added V0.9.3 (2000-04-09) [umoeller]
 *@@changed V0.9.4 (2000-07-10) [umoeller]: added PGMGCFG_STICKIES
 *@@changed V0.9.6 (2000-11-06) [umoeller]: fixed startup desktop to upper left
 */

BOOL pgmsLoadSettings(ULONG flConfig)
{
    // shortcuts to global pagemage config
    PPAGEMAGECONFIG pPageMageConfig = &G_pHookData->PageMageConfig;
    PPOINTL         pptlMaxDesktops = &pPageMageConfig->ptlMaxDesktops;

    ULONG   cb;
    _Pmpf(("pgmsLoadSettings"));

    cb = sizeof(PAGEMAGECONFIG);
    if (PrfQueryProfileData(HINI_USER,
                            INIAPP_XWPHOOK,
                            INIKEY_HOOK_PGMGCONFIG,
                            pPageMageConfig,
                            &cb))
    {
        // success:

        // always set start desktop to upper left V0.9.6 (2000-11-06) [umoeller]
        pPageMageConfig->ptlStartDesktop.x = 1;
        pPageMageConfig->ptlStartDesktop.y = pptlMaxDesktops->y;

        if (    (G_pHookData->hwndPageMageClient)
             && (G_pHookData->hwndPageMageFrame)
           )
        {
            if (flConfig & PGMGCFG_STICKIES)
                pgmwScanAllWindows();

            if (flConfig & PGMGCFG_REFORMAT)
                pgmcSetPgmgFramePos(G_pHookData->hwndPageMageFrame);

            if (flConfig & PGMGCFG_ZAPPO)
                WinPostMsg(G_pHookData->hwndPageMageClient,
                           PGMG_ZAPPO, 0, 0);

            if (flConfig & PGMGCFG_REPAINT)
                WinPostMsg(G_pHookData->hwndPageMageClient,
                           PGMG_INVALIDATECLIENT,
                           (MPARAM)FALSE,        // delayed
                           0);
        }

        return (TRUE);
    }

    return (FALSE);
}

/*
 *@@ pgmsSaveSettings:
 *
 *@@added V0.9.3 (2000-04-09) [umoeller]
 */

BOOL pgmsSaveSettings(VOID)
{
    _Pmpf(("pgmsSaveSettings"));
    return (PrfWriteProfileData(HINI_USER,
                                INIAPP_XWPHOOK,
                                INIKEY_HOOK_PGMGCONFIG,
                                &G_pHookData->PageMageConfig,
                                sizeof(PAGEMAGECONFIG)));
}

