
/*
 *@@sourcefile pgmg_settings.c:
 *      PageMage configuration stuff.
 *
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

#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_WIN
#define INCL_GPI
#include <os2.h>
#include <stdio.h>
#include <process.h>

#include "setup.h"                      // code generation and debugging options

#include "helpers\gpih.h"               // GPI helper routines

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
    _Pmpf(("pgmsSetDefaults"));
    // Misc 1
    G_pHookData->PageMageConfig.ptlMaxDesktops.x = 3;
    G_pHookData->PageMageConfig.ptlMaxDesktops.y = 2;
    G_pHookData->PageMageConfig.ptlStartDesktop.x = 1;
    G_pHookData->PageMageConfig.ptlStartDesktop.y = 2;
    // G_pHookData->PageMageConfig.iEdgeBoundary = 5;
    // G_pHookData->PageMageConfig.ulSleepTime = 100;
    // G_pHookData->PageMageConfig.bRepositionMouse = TRUE;
    G_pHookData->PageMageConfig.fClick2Activate = TRUE;

    // Misc 2
    // G_pHookData->PageMageConfig._bHoldWPS = TRUE;
    // G_pHookData->PageMageConfig.lPriority = 0;
    G_pHookData->PageMageConfig.fRecoverOnShutdown = TRUE;
    // G_pHookData->PageMageConfig.bSwitchToFocus = FALSE;

    // Display
    G_pHookData->PageMageConfig.fShowTitlebar = TRUE;
    // G_pHookData->PageMageConfig.fStartMinimized = FALSE; // ###
    G_pHookData->PageMageConfig.fFlash = FALSE;
    G_pHookData->PageMageConfig.ulFlashDelay = 2000;
    // G_pHookData->PageMageConfig.bFloatToTop = FALSE;
    G_pHookData->PageMageConfig.fMirrorWindows = TRUE;
    G_pHookData->PageMageConfig.fShowWindowText = TRUE;
    G_pHookData->PageMageConfig.fPreserveProportions = TRUE;

    // Sticky
    memset(G_pHookData->PageMageConfig.aszSticky,
           0,
           sizeof(G_pHookData->PageMageConfig.aszSticky));
    G_pHookData->PageMageConfig.usStickyTextNum = 0;
    memset(G_pHookData->PageMageConfig.hwndSticky2,
           0,
           sizeof(G_pHookData->PageMageConfig.hwndSticky2));
    G_pHookData->PageMageConfig.usSticky2Num = 0;

    // Colors 1
    G_pHookData->PageMageConfig.lcNormal = RGBCOL_DARKBLUE;
    G_pHookData->PageMageConfig.lcCurrent = RGBCOL_BLUE;
    G_pHookData->PageMageConfig.lcDivider = RGBCOL_GRAY;

    G_pHookData->PageMageConfig.lcNormalApp = RGBCOL_WHITE;
    G_pHookData->PageMageConfig.lcCurrentApp = RGBCOL_GREEN;
    G_pHookData->PageMageConfig.lcAppBorder = RGBCOL_BLACK;

    G_pHookData->PageMageConfig.lcTxtNormalApp = RGBCOL_BLACK;
    G_pHookData->PageMageConfig.lcTxtCurrentApp = RGBCOL_BLACK;

    // Panning
    G_pHookData->PageMageConfig.bPanAtTop
      = G_pHookData->PageMageConfig.bPanAtBottom
      = G_pHookData->PageMageConfig.bPanAtLeft
      = G_pHookData->PageMageConfig.bPanAtRight = TRUE;
    G_pHookData->PageMageConfig.bWrapAround = FALSE;

    // Keyboard
    G_pHookData->PageMageConfig.fEnableArrowHotkeys = FALSE;
    G_pHookData->PageMageConfig.ulKeyShift = KC_CTRL | KC_ALT;

    // PageMage window position
    G_swpPgmgFrame.x = WinQuerySysValue(HWND_DESKTOP, SV_CXSIZEBORDER);
    G_swpPgmgFrame.y = WinQuerySysValue(HWND_DESKTOP, SV_CYSIZEBORDER);

    G_swpPgmgFrame.cx = G_swpPgmgFrame.x
                        + (.18 * G_pHookData->lCXScreen);
    G_swpPgmgFrame.cy = G_swpPgmgFrame.y
                        + pgmcCalcNewFrameCY(G_swpPgmgFrame.cx);

    // Other
    strcpy(G_szFacename, "2.System VIO");
    // hps = WinGetPS(HWND_DESKTOP);
}

/*
 *@@ pgmsLoadSettings:
 *      loads the PageMage settings from OS2.INI,
 *      if present. Depending on flConfig, PageMage
 *      will reformat and/or re-adjust itself.
 *
 *@@added V0.9.3 (2000-04-09) [umoeller]
 *@@changed V0.9.4 (2000-07-10) [umoeller]: added PGMGCFG_STICKIES
 */

BOOL pgmsLoadSettings(ULONG flConfig)
{
    ULONG   cb;
    _Pmpf(("pgmsLoadSettings"));

    cb = sizeof(PAGEMAGECONFIG);
    if (PrfQueryProfileData(HINI_USER,
                            INIAPP_XWPHOOK,
                            INIKEY_HOOK_PGMGCONFIG,
                            &G_pHookData->PageMageConfig,
                            &cb))
    {
        // success:

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

