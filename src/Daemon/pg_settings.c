
/*
 *@@sourcefile pg_settings.c:
 *      XPager configuration stuff.
 *
 *      See pg_control.c for an introduction to XPager.
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

#define DONT_REPLACE_MALLOC         // in case mem debug is enabled
#include "setup.h"                      // code generation and debugging options

#include "helpers\gpih.h"               // GPI helper routines
#include "helpers\regexp.h"             // extended regular expressions

#include "xwpapi.h"                     // public XWorkplace definitions

#include "hook\xwphook.h"
#include "hook\hook_private.h"
#include "hook\xwpdaemn.h"              // XPager and daemon declarations

/*
 *@@ pgmsSetDefaults:
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 *@@changed V0.9.9 (2001-03-15) [lafaix]: startup desktop default is upper left
 */

VOID pgmsSetDefaults(VOID)
{
    // shortcuts to global config
    PPAGERCONFIG pXPagerConfig = &G_pHookData->XPagerConfig;
    PPOINTL         pptlMaxDesktops = &pXPagerConfig->ptlMaxDesktops;

    // first, set the fake desktop size... we no longer move windows
    // by the size of the PM screen (e.g. 1024x768), but instead we
    // use eight extra pixels, so we can get rid of the borders of
    // maximized windows.

    G_szlEachDesktopReal.cx = G_pHookData->lCXScreen + 8;
    G_szlEachDesktopReal.cy = G_pHookData->lCYScreen + 8;

    // _Pmpf(("pgmsSetDefaults"));
    // Misc 1
    pptlMaxDesktops->x = 3;
    pptlMaxDesktops->y = 2;
    pXPagerConfig->ptlStartDesktop.x = 1;
    pXPagerConfig->ptlStartDesktop.y = 1;         // @@@
    // pXPagerConfig->iEdgeBoundary = 5;
    // pXPagerConfig->ulSleepTime = 100;
    // pXPagerConfig->bRepositionMouse = TRUE;
    pXPagerConfig->fClick2Activate = TRUE;

    // Misc 2
    // pXPagerConfig->_bHoldWPS = TRUE;
    // pXPagerConfig->lPriority = 0;
    pXPagerConfig->fRecoverOnShutdown = TRUE;
    // pXPagerConfig->bSwitchToFocus = FALSE;

    // Display
    pXPagerConfig->fShowTitlebar = TRUE;
    // pXPagerConfig->fStartMinimized = FALSE;
    pXPagerConfig->fFlash = FALSE;
    pXPagerConfig->ulFlashDelay = 2000;
    // pXPagerConfig->bFloatToTop = FALSE;
    pXPagerConfig->fMirrorWindows = TRUE;
    pXPagerConfig->fShowWindowText = TRUE;
    pXPagerConfig->fPreserveProportions = TRUE;

    // Sticky
    memset(pXPagerConfig->aszSticky,
           0,
           sizeof(pXPagerConfig->aszSticky));
    pXPagerConfig->usStickyTextNum = 0;
    memset(pXPagerConfig->aulStickyFlags,
           0,
           sizeof(pXPagerConfig->aulStickyFlags));
    pXPagerConfig->usUnused1 = 0;

    // Colors 1
    pXPagerConfig->lcNormal = RGBCOL_DARKBLUE;
    pXPagerConfig->lcCurrent = RGBCOL_BLUE;
    pXPagerConfig->lcDivider = RGBCOL_GRAY;

    pXPagerConfig->lcNormalApp = RGBCOL_WHITE;
    pXPagerConfig->lcCurrentApp = RGBCOL_GREEN;
    pXPagerConfig->lcAppBorder = RGBCOL_BLACK;

    pXPagerConfig->lcTxtNormalApp = RGBCOL_BLACK;
    pXPagerConfig->lcTxtCurrentApp = RGBCOL_BLACK;

    // Panning
    //pXPagerConfig->bPanAtTop
    //  = pXPagerConfig->bPanAtBottom
    //  = pXPagerConfig->bPanAtLeft
    //  = pXPagerConfig->bPanAtRight = TRUE;
    pXPagerConfig->bWrapAround = FALSE;

    // Keyboard
    pXPagerConfig->fEnableArrowHotkeys = FALSE;
    pXPagerConfig->ulKeyShift = KC_CTRL | KC_ALT;

    // XPager window position
    G_swpPgmgFrame.x = WinQuerySysValue(HWND_DESKTOP, SV_CXSIZEBORDER);
    G_swpPgmgFrame.y = WinQuerySysValue(HWND_DESKTOP, SV_CYSIZEBORDER);

    G_swpPgmgFrame.cx = G_swpPgmgFrame.x
                        + (.18 * G_pHookData->lCXScreen);
    G_swpPgmgFrame.cy = G_swpPgmgFrame.y
                        + pgmcCalcNewFrameCY(G_swpPgmgFrame.cx);
}

/*
 *@@ pgmsLoadSettings:
 *      loads the XPager settings from OS2.INI,
 *      if present. Depending on flConfig, XPager
 *      will reformat and/or re-adjust itself.
 *
 *@@added V0.9.3 (2000-04-09) [umoeller]
 *@@changed V0.9.4 (2000-07-10) [umoeller]: added PGMGCFG_STICKIES
 *@@changed V0.9.6 (2000-11-06) [umoeller]: fixed startup desktop to upper left
 *@@changed V0.9.9 (2001-03-15) [lafaix]: relaxed startup desktop position
 */

BOOL pgmsLoadSettings(ULONG flConfig)
{
    // shortcuts to global config
    PPAGERCONFIG pXPagerConfig = &G_pHookData->XPagerConfig;
    PPOINTL         pptlMaxDesktops = &pXPagerConfig->ptlMaxDesktops;

    ULONG   cb;
    // _Pmpf(("pgmsLoadSettings"));

    cb = sizeof(PAGERCONFIG);
    if (PrfQueryProfileData(HINI_USER,
                            INIAPP_XWPHOOK,
                            INIKEY_HOOK_PGMGCONFIG,
                            pXPagerConfig,
                            &cb))
    {
        // success:

        // always set start desktop within limits V0.9.9 (2001-03-15) [lafaix]
        if (pXPagerConfig->ptlStartDesktop.x > pptlMaxDesktops->x)
            pXPagerConfig->ptlStartDesktop.x = pptlMaxDesktops->x;
        if (pXPagerConfig->ptlStartDesktop.y > pptlMaxDesktops->y)
            pXPagerConfig->ptlStartDesktop.y = pptlMaxDesktops->y;

        if (    (G_pHookData->hwndXPagerClient)
             && (G_pHookData->hwndXPagerFrame)
           )
        {
            if (flConfig & PGMGCFG_STICKIES)
            {
                // stickies changed:
                if (pgmwLock())
                {
                    // kill all regexps that were compiled
                    // V0.9.19 (2002-04-17) [umoeller]
                    ULONG ul;
                    for (ul = 0;
                         ul < MAX_STICKIES;
                         ul++)
                    {
                        if (G_pHookData->paEREs[ul])
                        {
                            rxpFree(G_pHookData->paEREs[ul]);
                            G_pHookData->paEREs[ul] = NULL;
                        }
                    }

                    pgmwUnlock();
                }
                pgmwScanAllWindows();
            }

            if (flConfig & PGMGCFG_REFORMAT)
                pgmcSetPgmgFramePos(G_pHookData->hwndXPagerFrame);

            if (flConfig & PGMGCFG_ZAPPO)
                WinPostMsg(G_pHookData->hwndXPagerClient,
                           PGMG_ZAPPO, 0, 0);

            if (flConfig & PGMGCFG_REPAINT)
                WinPostMsg(G_pHookData->hwndXPagerClient,
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
    // _Pmpf(("pgmsSaveSettings"));
    return (PrfWriteProfileData(HINI_USER,
                                INIAPP_XWPHOOK,
                                INIKEY_HOOK_PGMGCONFIG,
                                &G_pHookData->XPagerConfig,
                                sizeof(PAGERCONFIG)));
}

