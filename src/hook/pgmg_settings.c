
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
#include <stdlib.h>
#include <string.h>
#include <process.h>

#include "hook\xwphook.h"
#include "hook\hook_private.h"
#include "hook\xwpdaemn.h"              // PageMage and daemon declarations

#include "setup.h"                      // code generation and debugging options

/*
 *@@ pgmsSetDefaults:
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 */

VOID pgmsSetDefaults(VOID)
{
    // Misc 1
    G_PageMageConfig.ptlMaxDesktops.x = 3;
    G_PageMageConfig.ptlMaxDesktops.y = 2;
    G_PageMageConfig.ptlStartDesktop.x = 1;
    G_PageMageConfig.ptlStartDesktop.y = 2;
    G_PageMageConfig.iEdgeBoundary = 5;
    G_PageMageConfig.ulSleepTime = 100;
    G_PageMageConfig.bRepositionMouse = TRUE;
    G_PageMageConfig.bClickActivate = TRUE;

    // Misc 2
    G_PageMageConfig._bHoldWPS = TRUE;
    G_PageMageConfig.lPriority = 0;
    G_PageMageConfig.bRecoverOnShutdown = TRUE;
    G_PageMageConfig.bSwitchToFocus = FALSE;

    // Display
    G_PageMageConfig.bShowTitlebar = TRUE;
    G_PageMageConfig.bStartMin = FALSE;
    G_PageMageConfig.bFlash = FALSE;
    G_PageMageConfig.ulFlashDelay = 500;
    G_PageMageConfig.bFloatToTop = FALSE;
    G_PageMageConfig.bShowWindows = TRUE;
    G_PageMageConfig.bShowWindowText = TRUE;
    G_PageMageConfig.fPreserveProportions = TRUE;

    // Sticky
    memset(G_PageMageConfig.aszSticky, 0, sizeof(G_PageMageConfig.aszSticky));
    G_PageMageConfig.usStickyTextNum = 0;
    memset(G_PageMageConfig.hwndSticky2, 0, sizeof(G_PageMageConfig.hwndSticky2));
    G_PageMageConfig.usSticky2Num = 0;

    // Colors 1
    G_PageMageConfig.lcNormal = CLR_DARKBLUE;
    G_PageMageConfig.lcCurrent = CLR_BLUE;
    G_PageMageConfig.lcDivider = CLR_PALEGRAY;
    strcpy(G_PageMageConfig.szNormal, "Dark Blue");
    strcpy(G_PageMageConfig.szCurrent, "Blue");
    strcpy(G_PageMageConfig.szDivider, "Pale Gray");

    // Colors 2
    G_PageMageConfig.lcNormalApp = CLR_WHITE;
    G_PageMageConfig.lcCurrentApp = CLR_GREEN;
    G_PageMageConfig.lcAppBorder = CLR_BLACK;
    strcpy(G_PageMageConfig.szNormalApp, "White");
    strcpy(G_PageMageConfig.szCurrentApp, "Green");
    strcpy(G_PageMageConfig.szAppBorder, "Black");

    // Panning
    G_PageMageConfig.bPanAtTop
      = G_PageMageConfig.bPanAtBottom
      = G_PageMageConfig.bPanAtLeft
      = G_PageMageConfig.bPanAtRight = TRUE;
    G_PageMageConfig.bWrapAround = FALSE;

    // Keyboard
    G_PageMageConfig.ulKeyShift = KC_CTRL | KC_ALT;
    G_PageMageConfig.bReturnKeystrokes = TRUE;
    G_PageMageConfig.bHotkeyGrabFocus = TRUE;

    G_swpPgmgFrame.x = WinQuerySysValue(HWND_DESKTOP, SV_CXSIZEBORDER);
    G_swpPgmgFrame.y = WinQuerySysValue(HWND_DESKTOP, SV_CYSIZEBORDER);

    G_swpPgmgFrame.cx = G_swpPgmgFrame.x
                        + (.18 * G_pHookData->lCXScreen);

    G_swpPgmgFrame.cy = G_swpPgmgFrame.y
                        + pgmcCalcNewFrameCY(G_swpPgmgFrame.cx);

    // Other
    G_ptlMoveDeltaPcnt.x = 100;
    G_ptlMoveDeltaPcnt.y = 100;
    G_ptlMoveDelta.x = G_pHookData->lCXScreen;
    G_ptlMoveDelta.y = G_pHookData->lCYScreen;
    strcpy(G_szFacename, "2.System VIO");
    // hps = WinGetPS(HWND_DESKTOP);
}


