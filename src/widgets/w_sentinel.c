
/*
 *@@sourcefile w_theseus.c:
 *      XCenter Theseus widget.
 *
 *      This is all new with V0.9.9.
 *
 *@@added V0.9.9 (2001-02-08) [umoeller]
 *@@header "shared\center.h"
 */

/*
 *      Copyright (C) 2001 Ulrich M”ller.
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

#define INCL_DOSPROCESS
#define INCL_DOSMODULEMGR
#define INCL_DOSEXCEPTIONS
#define INCL_DOSSEMAPHORES
#define INCL_DOSDATETIME
#define INCL_DOSMISC
#define INCL_DOSERRORS

#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WININPUT
#define INCL_WINPOINTERS
#define INCL_WINPROGRAMLIST
#define INCL_WINSWITCHLIST
#define INCL_WINSYS
#define INCL_WINTIMER
#define INCL_WINMENUS
#define INCL_WINWORKPLACE

#define INCL_GPIPRIMITIVES
#define INCL_GPILOGCOLORTABLE
#define INCL_GPILCIDS
#define INCL_GPIREGIONS
#include <os2.h>

// C library headers
#include <stdio.h>
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#define DONT_REPLACE_MALLOC         // in case mem debug is enabled
#include "setup.h"                      // code generation and debugging options

// disable wrappers, because we're not linking statically
#ifdef WINH_STANDARDWRAPPERS
    #undef WINH_STANDARDWRAPPERS
#endif

// headers in /helpers
#include "helpers\comctl.h"             // common controls (window procs)
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\gpih.h"               // GPI helper routines
#include "helpers\nls.h"                // National Language Support helpers
#include "helpers\stringh.h"            // string helper routines
#include "helpers\timer.h"              // replacement PM timers
#include "helpers\winh.h"               // PM helper routines
#include "helpers\xstring.h"            // extended string helpers

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\center.h"              // public XCenter interfaces
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\helppanels.h"          // all XWorkplace help panel IDs

#include "config\cfgsys.h"              // XFldSystem CONFIG.SYS pages implementation
#include "config\drivdlgs.h"            // driver configuration dialogs

// win32k.sys includes
#include "win32k.h"

#pragma hdrstop                     // VAC++ keeps crashing otherwise

/* ******************************************************************
 *
 *   Private definitions
 *
 ********************************************************************/

/*
 *@@ SNAPSHOT:
 *
 *@@changed V0.9.9 (2001-03-30) [umoeller]: converted all fields to KB to avoid overflows
 */

typedef struct _SNAPSHOT
{
    ULONG   ulSwapperSizeKB,      // cfgQuerySwapperSize
            ulSwapperFreeKB,      // free space in swapper (win32k.sys only)
            ulPhysFreeKB;         // Dos16MemAvail

    // calculated values
    ULONG   ulVirtTotalKB,        // (const) physical RAM plus swapper size

            ulVirtInUseKB,        //  = pThis->ulVirtTotal - pThis->ulPhysFree,
            ulPhysInUseKB;        //  = ulVirtInUse - ulSwapper;

    /* ษอออออออออออออออออออป                  ฤฤฤฟ      ฤฤฤฟ
       บ                   บ                     ณ         ณ
       บ     ulPhysFree    บ                     ณ         ณ
       บ                   บ                     ณ         ณ
       ฬอออออออออออออออออออน ฤฤฤฟ            total phys    ณ
       บ                   บ    ณ                ณ         ณ  total pages
       บ     ulPhysInUse   บ    ณ                ณ         ณ  (ulVirtTotal)
       บ                   บ    ณ                ณ         ณ
       ฬอออออออออออออออออออน ulVirtInUse      ฤฤฤู         ณ
       บ                   บ    ณ                          ณ
       บ     ulSwapperSize บ    ณ                          ณ
       บ                   บ    ณ                          ณ
       ศอออออออออออออออออออผ ฤฤฤู                        ฤฤู

        There might be free pages in the swapper, but these are
        not reported by Dos16MemAvail... so we won't report them
        here either.
    */

} SNAPSHOT, *PSNAPSHOT;

APIRET16 APIENTRY16 Dos16MemAvail(PULONG pulAvailMem);

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

/* ******************************************************************
 *
 *   XCenter widget class definition
 *
 ********************************************************************/

/*
 *      This contains the name of the PM window class and
 *      the XCENTERWIDGETCLASS definition(s) for the widget
 *      class(es) in this DLL.
 */

#define WNDCLASS_WIDGET_SENTINEL    "XWPCenterSentinelWidget"

static XCENTERWIDGETCLASS G_WidgetClasses[] =
            {
                {
                    WNDCLASS_WIDGET_SENTINEL,
                    0,
                    "Sentinel",
                    "Sentinel memory watcher",
                    WGTF_SIZEABLE | WGTF_UNIQUEGLOBAL | WGTF_TOOLTIP,
                    NULL        // no settings dlg
                },
            };

/* ******************************************************************
 *
 *   Function imports from XFLDR.DLL
 *
 ********************************************************************/

/*
 *      To reduce the size of the widget DLL, it is
 *      compiled with the VAC subsystem libraries.
 *      In addition, instead of linking frequently
 *      used helpers against the DLL again, we import
 *      them from XFLDR.DLL, whose module handle is
 *      given to us in the INITMODULE export.
 *
 *      Note that importing functions from XFLDR.DLL
 *      is _not_ a requirement. We only do this to
 *      avoid duplicate code.
 *
 *      For each funtion that you need, add a global
 *      function pointer variable and an entry to
 *      the G_aImports array. These better match.
 */

// resolved function pointers from XFLDR.DLL
PCFGQUERYSWAPPERSIZE pcfgQuerySwapperSize = NULL;

PCMNQUERYDEFAULTFONT pcmnQueryDefaultFont = NULL;
PCMNQUERYHELPLIBRARY pcmnQueryHelpLibrary = NULL;

PCTRFREESETUPVALUE pctrFreeSetupValue = NULL;
PCTRPARSECOLORSTRING pctrParseColorString = NULL;
PCTRSCANSETUPSTRING pctrScanSetupString = NULL;

PDRV_SPRINTF pdrv_sprintf = NULL;

PGPIHBOX pgpihBox = NULL;
PGPIHCREATEBITMAP pgpihCreateBitmap = NULL;
PGPIHCREATEMEMPS pgpihCreateMemPS = NULL;
PGPIHDRAW3DFRAME pgpihDraw3DFrame = NULL;
PGPIHSWITCHTORGB pgpihSwitchToRGB = NULL;
PGPIHCREATEXBITMAP pgpihCreateXBitmap = NULL;
PGPIHDESTROYXBITMAP pgpihDestroyXBitmap = NULL;

PNLSDATETIME pnlsDateTime = NULL;
PNLSTHOUSANDSULONG pnlsThousandsULong = NULL;

PTMRSTARTXTIMER ptmrStartXTimer = NULL;
PTMRSTOPXTIMER ptmrStopXTimer = NULL;

PWINHFREE pwinhFree = NULL;
PWINHQUERYPRESCOLOR pwinhQueryPresColor = NULL;
PWINHQUERYWINDOWFONT pwinhQueryWindowFont = NULL;
PWINHSETWINDOWFONT pwinhSetWindowFont = NULL;

PXSTRCAT pxstrcat = NULL;
PXSTRCLEAR pxstrClear = NULL;
PXSTRINIT pxstrInit = NULL;

RESOLVEFUNCTION G_aImports[] =
    {
        "cfgQuerySwapperSize", (PFN*)&pcfgQuerySwapperSize,

        "cmnQueryDefaultFont", (PFN*)&pcmnQueryDefaultFont,
        "cmnQueryHelpLibrary", (PFN*)&pcmnQueryHelpLibrary,
        "ctrFreeSetupValue", (PFN*)&pctrFreeSetupValue,
        "ctrParseColorString", (PFN*)&pctrParseColorString,
        "ctrScanSetupString", (PFN*)&pctrScanSetupString,
        "drv_sprintf", (PFN*)&pdrv_sprintf,
        "gpihBox", (PFN*)&pgpihBox,
        "gpihCreateBitmap", (PFN*)&pgpihCreateBitmap,
        "gpihCreateMemPS", (PFN*)&pgpihCreateMemPS,
        "gpihDraw3DFrame", (PFN*)&pgpihDraw3DFrame,
        "gpihSwitchToRGB", (PFN*)&pgpihSwitchToRGB,
        "gpihCreateXBitmap", (PFN*)&pgpihCreateXBitmap,
        "gpihDestroyXBitmap", (PFN*)&pgpihDestroyXBitmap,
        "nlsDateTime", (PFN*)&pnlsDateTime,
        "nlsThousandsULong", (PFN*)&pnlsThousandsULong,
        "tmrStartXTimer", (PFN*)&ptmrStartXTimer,
        "tmrStopXTimer", (PFN*)&ptmrStopXTimer,
        "winhFree", (PFN*)&pwinhFree,
        "winhQueryPresColor", (PFN*)&pwinhQueryPresColor,
        "winhQueryWindowFont", (PFN*)&pwinhQueryWindowFont,
        "winhSetWindowFont", (PFN*)&pwinhSetWindowFont,
        "xstrcat", (PFN*)&pxstrcat,
        "xstrClear", (PFN*)&pxstrClear,
        "xstrInit", (PFN*)&pxstrInit
    };

/* ******************************************************************
 *
 *   Private widget instance data
 *
 ********************************************************************/

/*
 *@@ MONITORSETUP:
 *      instance data to which setup strings correspond.
 *      This is also a member of WIDGETPRIVATE.
 *
 *      Putting these settings into a separate structure
 *      is no requirement technically. However, once the
 *      widget uses a settings dialog, the dialog must
 *      support changing the widget settings even if the
 *      widget doesn't currently exist as a window, so
 *      separating the setup data from the widget window
 *      data will come in handy for managing the setup
 *      strings.
 */

typedef struct _MONITORSETUP
{
    LONG        lcolBackground,         // background color
                lcolForeground;         // foreground color (for text)

    LONG        lcolSwapFree,           // bottommost
                lcolSwap,
                lcolPhysInUse,
                lcolPhysFree;           // topmost

    PSZ         pszFont;
            // if != NULL, non-default font (in "8.Helv" format);
            // this has been allocated using local malloc()!

    LONG        cx;
            // current width; we're sizeable, and we wanna
            // store this
} MONITORSETUP, *PMONITORSETUP;

/*
 *@@ WIDGETPRIVATE:
 *      more window data for the various monitor widgets.
 *
 *      An instance of this is created on WM_CREATE in
 *      fnwpMonitorWidgets and stored in XCENTERWIDGET.pUser.
 */

typedef struct _WIDGETPRIVATE
{
    PXCENTERWIDGET pWidget;
            // reverse ptr to general widget data ptr; we need
            // that all the time and don't want to pass it on
            // the stack with each function call

    MONITORSETUP    Setup;
            // widget settings that correspond to a setup string

    BOOL            fCreating;              // TRUE while in WM_CREATE (anti-recursion)

    ULONG           ulTimerID;              // if != NULLHANDLE, update timer is running

    PXBITMAP        pBitmap;        // bitmap for pulse graph; this contains only
                                    // the "client" (without the 3D frame)

    BOOL            fUpdateGraph;

    ULONG           cyNeeded;       // returned for XN_QUERYSIZE... this is initialized
                                    // to 10, but probably changed later if we need
                                    // more space

    ULONG           ulTextWidth,    // space to be left clear for digits on the left
                    ulSpacing;      // spacing for current font

    ULONG           ulTotPhysMemKB; // DosQuerySysinfo(QSV_TOTPHYSMEM) / 1024;

    ULONG           ulMaxMemKBLast; // cache for scaling in bitmap

    APIRET          arcWin32K;      // return code from win32k.sys; if NO_ERROR,
                                    // we use win32k.sys for snapshots

    ULONG           cSnapshots;
    PSNAPSHOT       paSnapshots;
            // array of memory snapshots

    BOOL            fTooltipShowing;    // TRUE only while tooltip is currently
                                        // showing over this widget
    CHAR            szTooltipText[100]; // tooltip text

} WIDGETPRIVATE, *PWIDGETPRIVATE;

/* ******************************************************************
 *
 *   Widget settings dialog
 *
 ********************************************************************/

/*
 *      This section contains shared code to manage the
 *      widget's settings. This can translate a widget
 *      setup string into the fields of a binary setup
 *      structure and vice versa. This code is used by
 *      both an open widget window and a settings dialog.
 */

/*
 *@@ TwgtFreeSetup:
 *      cleans up the data in the specified setup
 *      structure, but does not free the structure
 *      itself.
 */

VOID TwgtFreeSetup(PMONITORSETUP pSetup)
{
    if (pSetup)
    {
        if (pSetup->pszFont)
        {
            free(pSetup->pszFont);
            pSetup->pszFont = NULL;
        }
    }
}

/*
 *@@ TwgtScanSetup:
 *      scans the given setup string and translates
 *      its data into the specified binary setup
 *      structure.
 *
 *      NOTE: It is assumed that pSetup is zeroed
 *      out. We do not clean up previous data here.
 *
 *@@changed V0.9.14 (2001-08-01) [umoeller]: fixed potential memory leak
 */

VOID TwgtScanSetup(const char *pcszSetupString,
                   PMONITORSETUP pSetup)
{
    PSZ p;

    // width
    if (p = pctrScanSetupString(pcszSetupString,
                                "WIDTH"))
    {
        pSetup->cx = atoi(p);
        pctrFreeSetupValue(p);
    }
    else
        pSetup->cx = 100;

    // background color
    if (p = pctrScanSetupString(pcszSetupString,
                                "BGNDCOL"))
    {
        pSetup->lcolBackground = pctrParseColorString(p);
        pctrFreeSetupValue(p);
    }
    else
        // default color:
        pSetup->lcolBackground = WinQuerySysColor(HWND_DESKTOP, SYSCLR_DIALOGBACKGROUND, 0);

    // text color:
    if (p = pctrScanSetupString(pcszSetupString,
                                "TEXTCOL"))
    {
        pSetup->lcolForeground = pctrParseColorString(p);
        pctrFreeSetupValue(p);
    }
    else
        pSetup->lcolForeground = WinQuerySysColor(HWND_DESKTOP, SYSCLR_WINDOWSTATICTEXT, 0);

    // font:
    // we set the font presparam, which automatically
    // affects the cached presentation spaces
    if (p = pctrScanSetupString(pcszSetupString,
                                "FONT"))
    {
        pSetup->pszFont = strdup(p);
        pctrFreeSetupValue(p);
    }
    else
        pSetup->pszFont = strdup("4.System VIO");

    pSetup->lcolSwapFree = RGBCOL_RED;
    pSetup->lcolSwap = RGBCOL_DARKPINK;
    pSetup->lcolPhysInUse = RGBCOL_DARKBLUE;
    pSetup->lcolPhysFree = RGBCOL_DARKGREEN;
}

/*
 *@@ TwgtSaveSetup:
 *      composes a new setup string.
 *      The caller must invoke xstrClear on the
 *      string after use.
 */

VOID TwgtSaveSetup(PXSTRING pstrSetup,       // out: setup string (is cleared first)
                   PMONITORSETUP pSetup)
{
    CHAR    szTemp[100];
    // PSZ     psz = 0;
    pxstrInit(pstrSetup, 100);

    pdrv_sprintf(szTemp, "WIDTH=%d;",
            pSetup->cx);
    pxstrcat(pstrSetup, szTemp, 0);

    pdrv_sprintf(szTemp, "BGNDCOL=%06lX;",
            pSetup->lcolBackground);
    pxstrcat(pstrSetup, szTemp, 0);

    pdrv_sprintf(szTemp, "TEXTCOL=%06lX;",
            pSetup->lcolForeground);
    pxstrcat(pstrSetup, szTemp, 0);

    if (pSetup->pszFont)
    {
        // non-default font:
        pdrv_sprintf(szTemp, "FONT=%s;",
                pSetup->pszFont);
        pxstrcat(pstrSetup, szTemp, 0);
    }
}

/* ******************************************************************
 *
 *   Widget settings dialog
 *
 ********************************************************************/

// None currently.

/* ******************************************************************
 *
 *   PM window class implementation
 *
 ********************************************************************/

/*
 *      This code has the actual PM window class.
 *
 */

/*
 *@@ CalcTextSpacing:
 *
 */

VOID CalcTextSpacing(HWND hwnd, PWIDGETPRIVATE pPrivate)
{
    // calculate new spacing for text
    HPS hps = WinGetPS(hwnd);
    if (hps)
    {
        FONTMETRICS fm;
        POINTL aptl[TXTBOX_COUNT];
        GpiQueryTextBox(hps,
                        3,
                        "999",      // test string
                        TXTBOX_COUNT,
                        aptl);

        GpiQueryFontMetrics(hps, sizeof(fm), &fm);

        pPrivate->ulTextWidth = aptl[TXTBOX_TOPRIGHT].x + 3;
        pPrivate->ulSpacing = fm.lMaxAscender - fm.lInternalLeading + 1;

        pPrivate->cyNeeded = (pPrivate->ulSpacing * 3) + 1;

        WinReleasePS(hwnd);
    }
}

/*
 *@@ TwgtCreate:
 *      implementation for WM_CREATE.
 */

MRESULT TwgtCreate(HWND hwnd,
                   PXCENTERWIDGET pWidget)
{
    MRESULT mrc = 0;        // continue window creation

    // PSZ p;

    PWIDGETPRIVATE pPrivate = malloc(sizeof(WIDGETPRIVATE));
    memset(pPrivate, 0, sizeof(WIDGETPRIVATE));
    // link the two together
    pWidget->pUser = pPrivate;
    pPrivate->pWidget = pWidget;

    pPrivate->fCreating = TRUE;

    // initialize binary setup structure from setup string
    TwgtScanSetup(pWidget->pcszSetupString,
                  &pPrivate->Setup);

    // set window font (this affects all the cached presentation
    // spaces we use)
    pwinhSetWindowFont(hwnd,
                       pPrivate->Setup.pszFont);

    CalcTextSpacing(hwnd, pPrivate);

    // enable context menu help
    pWidget->pcszHelpLibrary = pcmnQueryHelpLibrary();
    pWidget->ulHelpPanelID = ID_XSH_WIDGET_SENTINEL_MAIN;

    // get current RAM size
    DosQuerySysInfo(QSV_TOTPHYSMEM,
                    QSV_TOTPHYSMEM,
                    &pPrivate->ulTotPhysMemKB,
                    sizeof(pPrivate->ulTotPhysMemKB));
    // convert to KB
    pPrivate->ulTotPhysMemKB = (pPrivate->ulTotPhysMemKB + 512) / 1024;

    // initialize win32k.sys; if this returns NO_ERROR,
    // we'll use that driver, otherwise not
    pPrivate->arcWin32K = libWin32kInit();

    // start update timer
    pPrivate->ulTimerID = ptmrStartXTimer(pWidget->pGlobals->pvXTimerSet,
                                         hwnd,
                                         1,
                                         2000);


    pPrivate->fCreating = FALSE;

    pPrivate->szTooltipText[0] = '\0';

    return (mrc);
}

/*
 *@@ TwgtControl:
 *      implementation for WM_CONTROL.
 */

BOOL TwgtControl(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    BOOL brc = FALSE;

    PXCENTERWIDGET pWidget;
    PWIDGETPRIVATE pPrivate;
    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (PWIDGETPRIVATE)pWidget->pUser)
       )
    {
        USHORT  usID = SHORT1FROMMP(mp1),
                usNotifyCode = SHORT2FROMMP(mp1);

        switch (usID)
        {
            case ID_XCENTER_CLIENT:
            {
                switch (usNotifyCode)
                {
                    /*
                     * XN_QUERYSIZE:
                     *      XCenter wants to know our size.
                     */

                    case XN_QUERYSIZE:
                    {
                        PSIZEL pszl = (PSIZEL)mp2;
                        pszl->cx = pPrivate->Setup.cx;
                        pszl->cy = pPrivate->cyNeeded;
                                    // initially 10, possibly raised later
                        brc = TRUE;
                    break; }
                }
            }
            break;

            case ID_XCENTER_TOOLTIP:
            {
                switch (usNotifyCode)
                {
                    case TTN_NEEDTEXT:
                    {
                        PTOOLTIPTEXT pttt = (PTOOLTIPTEXT)mp2;
                        pttt->pszText = pPrivate->szTooltipText;
                        pttt->ulFormat = TTFMT_PSZ;
                    }
                    break;

                    case TTN_SHOW:
                        pPrivate->fTooltipShowing = TRUE;
                    break;

                    case TTN_POP:
                        pPrivate->fTooltipShowing = FALSE;
                    break;
                }
            }
        } // end if (pPrivate)
    } // end if (pWidget)

    return (brc);
}

/*
 *@@ PaintGraphLine:
 *      paints one of those funny lines in
 *      the memory graph.
 *
 *      Preconditions:
 *
 *      -- pPrivate->pBitmap must exist.
 */

VOID PaintGraphLine(PWIDGETPRIVATE pPrivate,
                    PSNAPSHOT pThis,
                    ULONG ulMaxMemKB,
                    LONG x,             // in: xpos to paint at
                    LONG yTop,          // in: yTop of bitmap rect
                    LONG lFillBkgnd)    // in: if != -1, bkgnd color
{
    if (pPrivate->pBitmap)
    {
        HPS hpsMem = pPrivate->pBitmap->hpsMem;
        PMONITORSETUP pSetup = &pPrivate->Setup;
        POINTL ptl;
        ptl.x = x;

        if (ulMaxMemKB)           // avoid division by zero
        {
            ptl.y = 0;
            GpiMove(hpsMem, &ptl);

            // win32k.sys available?
            if (pThis->ulSwapperFreeKB)
            {
                // yes: paint "swapper free"
                GpiSetColor(hpsMem,
                            pSetup->lcolSwapFree);
                ptl.y += yTop * pThis->ulSwapperFreeKB / ulMaxMemKB;
                GpiLine(hpsMem, &ptl);

                // paint "swapper used"
                GpiSetColor(hpsMem,
                            pSetup->lcolSwap);
                ptl.y += yTop * (pThis->ulSwapperSizeKB - pThis->ulSwapperFreeKB) / ulMaxMemKB;
                GpiLine(hpsMem, &ptl);
            }
            else
            {
                // win32.sys not available:
                // paint "swapper size"
                GpiSetColor(hpsMem,
                            pSetup->lcolSwap);
                ptl.y += yTop * pThis->ulSwapperSizeKB / ulMaxMemKB;
                GpiLine(hpsMem, &ptl);
            }

            // paint "physically used mem"
            GpiSetColor(hpsMem,
                        pSetup->lcolPhysInUse);
            ptl.y += yTop * pThis->ulPhysInUseKB / ulMaxMemKB;
            GpiLine(hpsMem, &ptl);

            // paint "free mem" in green
            GpiSetColor(hpsMem,
                        pSetup->lcolPhysFree);
            ptl.y += yTop * pThis->ulPhysFreeKB / ulMaxMemKB;
            GpiLine(hpsMem, &ptl);
        }

        if (lFillBkgnd != -1)
        {
            GpiSetColor(hpsMem, lFillBkgnd);
            ptl.y = yTop;
            GpiLine(hpsMem, &ptl);
        }
    } // end if (pPrivate->pBitmap)
}

/*
 *@@ TwgtUpdateGraph:
 *      updates the graph bitmap. This does not paint
 *      on the screen.
 *
 *      Preconditions:
 *      --  pPrivate->hbmGraph must be selected into
 *          pPrivate->hpsMem.
 */

VOID TwgtUpdateGraph(HWND hwnd,
                     PWIDGETPRIVATE pPrivate)
{
    PXCENTERWIDGET pWidget = pPrivate->pWidget;
    ULONG   ul = 0;
    RECTL   rclBmp;

    // size for bitmap: same as widget, except
    // for the border
    WinQueryWindowRect(hwnd, &rclBmp);
    rclBmp.xRight -= 2;
    rclBmp.yTop -= 2;

    if (!pPrivate->pBitmap)
    {
        // bitmap needs to be created:
        pPrivate->pBitmap = pgpihCreateXBitmap(pWidget->habWidget,
                                               rclBmp.xRight,
                                               rclBmp.yTop);
        // make sure we repaint below
        pPrivate->ulMaxMemKBLast = 0;
    }

    if (pPrivate->pBitmap)
    {
        HPS hpsMem = pPrivate->pBitmap->hpsMem;

        if (!pPrivate->paSnapshots)
        {
            // no snapshots yet:
            // just fill the bitmap rectangle
            GpiSetColor(hpsMem,
                        pPrivate->Setup.lcolBackground);
            pgpihBox(hpsMem,
                     DRO_FILL,
                     &rclBmp);
        }
        else
        {
            PSNAPSHOT pLatest
                = &pPrivate->paSnapshots[pPrivate->cSnapshots - 1];
            CHAR    sz1[50],
                    sz2[50],
                    sz3[50];
            PSZ     p;
            PCOUNTRYSETTINGS pCountrySettings = (PCOUNTRYSETTINGS)pWidget->pGlobals->pCountrySettings;
            CHAR    cThousands = pCountrySettings->cThousands;

            // find the max total RAM value first
            ULONG ulMaxMemKB = 0;
            for (ul = 0;
                 ((ul < pPrivate->cSnapshots) && (ul < rclBmp.xRight));
                 ul++)
            {
                PSNAPSHOT pThis = &pPrivate->paSnapshots[ul];
                ULONG ulThis = pThis->ulVirtTotalKB;
                if (ulThis > ulMaxMemKB)
                    ulMaxMemKB = ulThis;
            }

            if (ulMaxMemKB != pPrivate->ulMaxMemKBLast)
            {
                // scaling has changed (or first call):
                // well, then we need to repaint the entire
                // damn bitmap
                POINTL  ptl;
                ptl.x = pPrivate->ulTextWidth;

                // fill the bitmap rectangle
                GpiSetColor(hpsMem,
                            pPrivate->Setup.lcolBackground);
                pgpihBox(hpsMem,
                         DRO_FILL,
                         &rclBmp);

                for (ul = pPrivate->ulTextWidth;
                     ((ul < pPrivate->cSnapshots) && (ul < rclBmp.xRight));
                     ul++)
                {
                    PSNAPSHOT pThis = &pPrivate->paSnapshots[ul];

                    PaintGraphLine(pPrivate,
                                   pThis,
                                   ulMaxMemKB,
                                   ptl.x,
                                   rclBmp.yTop,
                                   -1);             // no bkgnd, we just filled that
                    ptl.x++;
                }

                // store this for next time
                pPrivate->ulMaxMemKBLast = ulMaxMemKB;
            }
            else
            {
                // scaling has not changed:
                // we can then bitblt the bitmap one to the left
                // and only paint the rightmost column
                POINTL      ptlCopy[3];

                // lower left of target
                ptlCopy[0].x = pPrivate->ulTextWidth;
                ptlCopy[0].y = 0;
                // upper right of target (inclusive!)
                ptlCopy[1].x = rclBmp.xRight - 1;
                ptlCopy[1].y = rclBmp.yTop;
                // lower left of source
                ptlCopy[2].x = ptlCopy[0].x + 1;
                ptlCopy[2].y = 0;
                GpiBitBlt(hpsMem,
                          hpsMem,
                          (LONG)3,
                          ptlCopy,
                          ROP_SRCCOPY,
                          BBO_IGNORE);

                // add a new column to the right
                PaintGraphLine(pPrivate,
                               pLatest, // &pPrivate->paSnapshots[pPrivate->cSnapshots - 1],
                               ulMaxMemKB,
                               pPrivate->cSnapshots - 1,
                               rclBmp.yTop,
                               pPrivate->Setup.lcolBackground);
            }

            // update the tooltip text V0.9.13 (2001-06-21) [umoeller]
            p = pPrivate->szTooltipText;
            p += pdrv_sprintf(p,
                         "Physical memory usage"                  // @@todo localize
                         "\nFree RAM: %s KB"
                         "\nUsed RAM: %s KB"
                         "\nSwapper size: %s KB",
                         pnlsThousandsULong(sz1, pLatest->ulPhysFreeKB, cThousands),
                         pnlsThousandsULong(sz2, pLatest->ulPhysInUseKB, cThousands),
                         pnlsThousandsULong(sz3, pLatest->ulSwapperSizeKB, cThousands));

            if (pPrivate->arcWin32K == NO_ERROR)
                pdrv_sprintf(p,
                        "\nFree in swapper: %s KB ",
                        pnlsThousandsULong(sz1, pLatest->ulSwapperFreeKB, cThousands));

            if (pPrivate->fTooltipShowing)
                // tooltip currently showing:
                // refresh its display
                WinSendMsg(pWidget->pGlobals->hwndTooltip,
                           TTM_UPDATETIPTEXT,
                           (MPARAM)pPrivate->szTooltipText,
                           0);
        }
    }

    pPrivate->fUpdateGraph = FALSE;
}

/*
 *@@ DrawNumber:
 *
 */

VOID DrawNumber(HPS hps,
                LONG y,
                ULONG ulNumber,
                LONG lColor)
{
    POINTL  ptl;
    CHAR    szPaint[30] = "";
    pdrv_sprintf(szPaint,
            "%lu",
            ulNumber);
    ptl.x = 2;
    ptl.y = y;
    GpiSetColor(hps, lColor);
    GpiCharStringAt(hps,
                    &ptl,
                    strlen(szPaint),
                    szPaint);
}

/*
 * TwgtPaint2:
 *      this does the actual painting of the frame (if
 *      fDrawFrame==TRUE) and the pulse bitmap.
 *
 *      Gets called by TwgtPaint.
 *
 *      The specified HPS is switched to RGB mode before
 *      painting.
 *
 *      If DosPerfSysCall succeeds, this diplays the pulse.
 *      Otherwise an error msg is displayed.
 */

VOID TwgtPaint2(HWND hwnd,
                PWIDGETPRIVATE pPrivate,
                HPS hps,
                BOOL fDrawFrame)     // in: if TRUE, everything is painted
{
    PXCENTERWIDGET pWidget = pPrivate->pWidget;
    PMONITORSETUP pSetup = &pPrivate->Setup;
    RECTL       rclWin;
    ULONG       ulBorder = 1;
    // CHAR        szPaint[100] = "";
    // ULONG       ulPaintLen = 0;

    // now paint button frame
    WinQueryWindowRect(hwnd,
                       &rclWin);        // exclusive
    pgpihSwitchToRGB(hps);

    rclWin.xRight--;
    rclWin.yTop--;
        // rclWin is now inclusive

    if (fDrawFrame)
    {
        LONG lDark, lLight;

        if (pPrivate->pWidget->pGlobals->flDisplayStyle & XCS_SUNKBORDERS)
        {
            lDark = pWidget->pGlobals->lcol3DDark;
            lLight = pWidget->pGlobals->lcol3DLight;
        }
        else
        {
            lDark =
            lLight = pPrivate->Setup.lcolBackground;
        }

        pgpihDraw3DFrame(hps,
                         &rclWin,        // inclusive
                         ulBorder,
                         lDark,
                         lLight);
    }

    if (pPrivate->fUpdateGraph)
        // graph bitmap needs to be updated:
        TwgtUpdateGraph(hwnd, pPrivate);

    if (pPrivate->pBitmap)
    {
        POINTL      ptlBmpDest;
        ptlBmpDest.x = rclWin.xLeft + ulBorder;
        ptlBmpDest.y = rclWin.yBottom + ulBorder;
        // now paint graph from bitmap
        WinDrawBitmap(hps,
                      pPrivate->pBitmap->hbm,
                      NULL,     // entire bitmap
                      &ptlBmpDest,
                      0, 0,
                      DBM_NORMAL);
    }

    if (pPrivate->paSnapshots)
    {
        PSNAPSHOT pLatest
            = &pPrivate->paSnapshots[pPrivate->cSnapshots - 1];

        LONG    y = 1;

        DrawNumber(hps,
                   y,
                   (pLatest->ulSwapperSizeKB + (1024 / 2)) / 1024,
                   pSetup->lcolSwap);
        y += pPrivate->ulSpacing;
        DrawNumber(hps,
                   y,
                   (pLatest->ulPhysInUseKB + (1024 / 2)) / 1024,
                   pSetup->lcolPhysInUse);
        y += pPrivate->ulSpacing;
        DrawNumber(hps,
                   y,
                   (pLatest->ulPhysFreeKB + (1024 / 2)) / 1024,
                   pSetup->lcolPhysFree);
    }
}

/*
 *@@ TwgtPaint:
 *      implementation for WM_PAINT.
 */

VOID TwgtPaint(HWND hwnd)
{
    HPS hps = WinBeginPaint(hwnd, NULLHANDLE, NULL);
    if (hps)
    {
        // get widget data and its button data from QWL_USER
        PXCENTERWIDGET pWidget;
        PWIDGETPRIVATE pPrivate;
        if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
             && (pPrivate = (PWIDGETPRIVATE)pWidget->pUser)
           )
        {
            TwgtPaint2(hwnd,
                       pPrivate,
                       hps,
                       TRUE);        // draw frame
        } // end if (pPrivate)

        WinEndPaint(hps);

    } // end if (hps)
}

/*
 *@@ GetSnapshot:
 *      updates the newest entry in the snapshots
 *      array with the current memory values.
 *
 *      If win32k.sys was found, this uses that driver
 *      to update the values. Otherwise we use the slow
 *      and imprecise standard methods.
 *
 *@@added V0.9.9 (2001-03-30) [umoeller]
 */

VOID GetSnapshot(PWIDGETPRIVATE pPrivate)
{
    PSNAPSHOT pLatest = &pPrivate->paSnapshots[pPrivate->cSnapshots - 1];
    memset(pLatest, 0, sizeof(SNAPSHOT));

    if (pPrivate->arcWin32K == NO_ERROR)
    {
        K32SYSTEMMEMINFO MemInfo;

        memset(&MemInfo, 0xFE, sizeof(MemInfo));
        MemInfo.cb = sizeof(K32SYSTEMMEMINFO);
        MemInfo.flFlags = K32_SYSMEMINFO_ALL;
        pPrivate->arcWin32K = W32kQuerySystemMemInfo(&MemInfo);

        if (pPrivate->arcWin32K == NO_ERROR)
        {
            pLatest->ulSwapperSizeKB = (MemInfo.cbSwapFileSize + 512) / 1024;
            pLatest->ulSwapperFreeKB = (MemInfo.cbSwapFileAvail + 512) / 1024;
            pLatest->ulPhysFreeKB = (MemInfo.cbPhysAvail + 512) / 1024;
            pLatest->ulVirtTotalKB = pPrivate->ulTotPhysMemKB + pLatest->ulSwapperSizeKB;

            pLatest->ulVirtInUseKB = (pLatest->ulVirtTotalKB - pLatest->ulPhysFreeKB);
            // pLatest->ulPhysInUseKB = (MemInfo.cbPhysUsed + 512) / 1024;
            pLatest->ulPhysInUseKB = (pLatest->ulVirtInUseKB - pLatest->ulSwapperSizeKB);
        }
    }
    else
    {
        // win32k.sys failed:
        pLatest->ulSwapperSizeKB = (pcfgQuerySwapperSize() + 512) / 1024;
        Dos16MemAvail(&pLatest->ulPhysFreeKB);
        pLatest->ulPhysFreeKB = (pLatest->ulPhysFreeKB + 512) / 1024;
        pLatest->ulVirtTotalKB = pPrivate->ulTotPhysMemKB + pLatest->ulSwapperSizeKB;

        // now do calcs based on that... we don't wanna go thru
        // this on every paint
        pLatest->ulVirtInUseKB = (pLatest->ulVirtTotalKB - pLatest->ulPhysFreeKB);
        pLatest->ulPhysInUseKB = (pLatest->ulVirtInUseKB - pLatest->ulSwapperSizeKB);
    }
}

/*
 *@@ TwgtTimer:
 *      updates the snapshots array, updates the
 *      graph bitmap, and invalidates the window.
 */

VOID TwgtTimer(HWND hwnd)
{
    PXCENTERWIDGET pWidget;
    PWIDGETPRIVATE pPrivate;
    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (PWIDGETPRIVATE)pWidget->pUser)
       )
    {
        HPS hps;
        RECTL rclClient;
        WinQueryWindowRect(hwnd, &rclClient);
        if (rclClient.xRight)
        {
            ULONG ulGraphCX = rclClient.xRight - 2;    // minus border

            if (pPrivate->paSnapshots == NULL)
            {
                // create array of loads
                ULONG cb = sizeof(SNAPSHOT) * ulGraphCX;
                pPrivate->cSnapshots = ulGraphCX;
                pPrivate->paSnapshots
                    = (PSNAPSHOT)malloc(cb);
                memset(pPrivate->paSnapshots, 0, cb);
            }

            if (pPrivate->paSnapshots)
            {
                // in the array of loads, move each entry one to the front;
                // drop the oldest entry
                memcpy(&pPrivate->paSnapshots[0],
                       &pPrivate->paSnapshots[1],
                       sizeof(SNAPSHOT) * (pPrivate->cSnapshots - 1));

                // and update the last entry with the current value
                GetSnapshot(pPrivate);

                // update display
                pPrivate->fUpdateGraph = TRUE;

                hps = WinGetPS(hwnd);
                TwgtPaint2(hwnd,
                           pPrivate,
                           hps,
                           FALSE);       // do not draw frame
                WinReleasePS(hps);
            }
        } // end if (rclClient.xRight)
    } // end if (pPrivate)
}

/*
 *@@ TwgtWindowPosChanged:
 *      implementation for WM_WINDOWPOSCHANGED.
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
 *@@changed V0.9.13 (2001-06-21) [umoeller]: changed XCM_SAVESETUP call for tray support
 */

VOID TwgtWindowPosChanged(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    PXCENTERWIDGET pWidget;
    PWIDGETPRIVATE pPrivate;
    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (PWIDGETPRIVATE)pWidget->pUser)
       )
    {
        PSWP pswpNew = (PSWP)mp1,
             pswpOld = pswpNew + 1;
        if (pswpNew->fl & SWP_SIZE)
        {
            // window was resized:

            // destroy the buffer bitmap because we
            // need a new one with a different size
            if (pPrivate->pBitmap)
                pgpihDestroyXBitmap(&pPrivate->pBitmap);

            if (pswpNew->cx != pswpOld->cx)
            {
                XSTRING strSetup;
                // width changed:
                if (pPrivate->paSnapshots)
                {
                    // we also need a new array of past loads
                    // since the array is cx items wide...
                    // so reallocate the array, but keep past
                    // values
                    ULONG ulNewClientCX = pswpNew->cx - 2;
                    PSNAPSHOT paNewSnapshots =
                        (PSNAPSHOT)malloc(sizeof(SNAPSHOT) * ulNewClientCX);

                    if (ulNewClientCX > pPrivate->cSnapshots)
                    {
                        // window has become wider:
                        // fill the front with zeroes
                        memset(paNewSnapshots,
                               0,
                               (ulNewClientCX - pPrivate->cSnapshots) * sizeof(SNAPSHOT));
                        // and copy old values after that
                        memcpy(&paNewSnapshots[(ulNewClientCX - pPrivate->cSnapshots)],
                               pPrivate->paSnapshots,
                               pPrivate->cSnapshots * sizeof(SNAPSHOT));
                    }
                    else
                    {
                        // window has become smaller:
                        // e.g. ulnewClientCX = 100
                        //      pPrivate->cLoads = 200
                        // drop the first items
                        // ULONG ul = 0;
                        memcpy(paNewSnapshots,
                               &pPrivate->paSnapshots[pPrivate->cSnapshots - ulNewClientCX],
                               ulNewClientCX * sizeof(SNAPSHOT));
                    }

                    pPrivate->cSnapshots = ulNewClientCX;
                    free(pPrivate->paSnapshots);
                    pPrivate->paSnapshots = paNewSnapshots;
                } // end if (pPrivate->palLoads)

                pPrivate->Setup.cx = pswpNew->cx;
                TwgtSaveSetup(&strSetup,
                              &pPrivate->Setup);
                if (strSetup.ulLength)
                    // changed V0.9.13 (2001-06-21) [umoeller]:
                    // post it to parent instead of fixed XCenter client
                    // to make this trayable
                    WinSendMsg(WinQueryWindow(hwnd, QW_PARENT), // pPrivate->pWidget->pGlobals->hwndClient,
                               XCM_SAVESETUP,
                               (MPARAM)hwnd,
                               (MPARAM)strSetup.psz);
                pxstrClear(&strSetup);
            } // end if (pswpNew->cx != pswpOld->cx)

            // force recreation of bitmap
            pPrivate->fUpdateGraph = TRUE;
            WinInvalidateRect(hwnd, NULL, FALSE);
        } // end if (pswpNew->fl & SWP_SIZE)
    } // end if (pPrivate)
}

/*
 *@@ TwgtPresParamChanged:
 *      implementation for WM_PRESPARAMCHANGED.
 *
 *@@changed V0.9.13 (2001-06-21) [umoeller]: changed XCM_SAVESETUP call for tray support
 */

VOID TwgtPresParamChanged(HWND hwnd,
                          ULONG ulAttrChanged,
                          PXCENTERWIDGET pWidget)
{
    PWIDGETPRIVATE pPrivate = (PWIDGETPRIVATE)pWidget->pUser;
    if (pPrivate)
    {
        BOOL fInvalidate = TRUE;
        switch (ulAttrChanged)
        {
            case 0:     // layout palette thing dropped
            case PP_BACKGROUNDCOLOR:
            case PP_FOREGROUNDCOLOR:
                pPrivate->Setup.lcolBackground
                    = pwinhQueryPresColor(hwnd,
                                          PP_BACKGROUNDCOLOR,
                                          FALSE,
                                          SYSCLR_DIALOGBACKGROUND);
                pPrivate->Setup.lcolForeground
                    = pwinhQueryPresColor(hwnd,
                                          PP_FOREGROUNDCOLOR,
                                          FALSE,
                                          SYSCLR_WINDOWSTATICTEXT);
            break;

            case PP_FONTNAMESIZE:
            {
                // HPS hps;
                PSZ pszFont = 0;
                if (pPrivate->Setup.pszFont)
                {
                    free(pPrivate->Setup.pszFont);
                    pPrivate->Setup.pszFont = NULL;
                }

                pszFont = pwinhQueryWindowFont(hwnd);
                if (pszFont)
                {
                    // we must use local malloc() for the font
                    pPrivate->Setup.pszFont = strdup(pszFont);
                    pwinhFree(pszFont);
                }

                CalcTextSpacing(hwnd, pPrivate);

                // do not do this during WM_CREATE
                if (!pPrivate->fCreating)
                {
                    WinPostMsg(pWidget->pGlobals->hwndClient,
                               XCM_REFORMAT,
                               (MPARAM)XFMF_GETWIDGETSIZES,
                               0);
                }

            break; }

            default:
                fInvalidate = FALSE;
        }

        if (fInvalidate)
        {
            XSTRING strSetup;
            WinInvalidateRect(hwnd, NULL, FALSE);

            TwgtSaveSetup(&strSetup,
                          &pPrivate->Setup);
            if (strSetup.ulLength)
                // changed V0.9.13 (2001-06-21) [umoeller]:
                // post it to parent instead of fixed XCenter client
                // to make this trayable
                WinSendMsg(WinQueryWindow(hwnd, QW_PARENT), // pPrivate->pWidget->pGlobals->hwndClient,
                           XCM_SAVESETUP,
                           (MPARAM)hwnd,
                           (MPARAM)strSetup.psz);
            pxstrClear(&strSetup);
        }
    } // end if (pPrivate)
}

/*
 *@@ TwgtButton1DblClick:
 *      implementation for WM_BUTTON1DBLCLK.
 */

VOID TwgtButton1DblClick(HWND hwnd,
                         PXCENTERWIDGET pWidget)
{
    PWIDGETPRIVATE pPrivate = (PWIDGETPRIVATE)pWidget->pUser;
    if (pPrivate)
    {
        const char *pcszID = "<XWP_KERNEL>";
        HOBJECT hobj = WinQueryObject((PSZ)pcszID);

        if (hobj)
        {
            WinOpenObject(hobj,
                          2, // OPEN_SETTINGS,
                          TRUE);
        }
    } // end if (pPrivate)
}

/*
 *@@ fnwpMonitorWidgets:
 *      window procedure for the "Sentinel".
 *
 *@@changed V0.9.12 (2001-05-20) [umoeller]: fixed resource leak on destroy
 */

MRESULT EXPENTRY fnwpMonitorWidgets(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;
    PXCENTERWIDGET pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER);
                    // this ptr is valid after WM_CREATE

    switch (msg)
    {
        /*
         * WM_CREATE:
         *      as with all widgets, we receive a pointer to the
         *      XCENTERWIDGET in mp1, which was created for us.
         *
         *      The first thing the widget MUST do on WM_CREATE
         *      is to store the XCENTERWIDGET pointer (from mp1)
         *      in the QWL_USER window word by calling:
         *
         *          WinSetWindowPtr(hwnd, QWL_USER, mp1);
         *
         *      We use XCENTERWIDGET.pUser for allocating
         *      WIDGETPRIVATE for our own stuff.
         */

        case WM_CREATE:
            WinSetWindowPtr(hwnd, QWL_USER, mp1);
            pWidget = (PXCENTERWIDGET)mp1;
            if ((pWidget) && (pWidget->pfnwpDefWidgetProc))
                mrc = TwgtCreate(hwnd, pWidget);
            else
                // stop window creation!!
                mrc = (MPARAM)TRUE;
        break;

        /*
         * WM_CONTROL:
         *      process notifications/queries from the XCenter.
         */

        case WM_CONTROL:
            mrc = (MPARAM)TwgtControl(hwnd, mp1, mp2);
        break;

        /*
         * WM_PAINT:
         *
         */

        case WM_PAINT:
            TwgtPaint(hwnd);
        break;

        /*
         * WM_TIMER:
         *      clock timer --> repaint.
         */

        case WM_TIMER:
            TwgtTimer(hwnd);
        break;

        /*
         * WM_WINDOWPOSCHANGED:
         *      on window resize, allocate new bitmap.
         */

        case WM_WINDOWPOSCHANGED:
            TwgtWindowPosChanged(hwnd, mp1, mp2);
        break;

        /*
         * WM_PRESPARAMCHANGED:
         *
         */

        case WM_PRESPARAMCHANGED:
            if (pWidget)
                // this gets sent before this is set!
                TwgtPresParamChanged(hwnd, (ULONG)mp1, pWidget);
        break;

        /*
         *@@ WM_BUTTON1DBLCLK:
         *      on double-click on clock, open
         *      system clock settings.
         */

        case WM_BUTTON1DBLCLK:
            TwgtButton1DblClick(hwnd, pWidget);
            mrc = (MPARAM)TRUE;     // message processed
        break;

        /*
         * WM_DESTROY:
         *      clean up. This _must_ be passed on to
         *      ctrDefWidgetProc.
         */

        case WM_DESTROY:
        {
            PWIDGETPRIVATE pPrivate = (PWIDGETPRIVATE)pWidget->pUser;
            if (pPrivate)
            {
                if (pPrivate->ulTimerID)
                    ptmrStopXTimer(pPrivate->pWidget->pGlobals->pvXTimerSet,
                                  hwnd,
                                  pPrivate->ulTimerID);

                libWin32kTerm();

                if (pPrivate->pBitmap)
                    pgpihDestroyXBitmap(&pPrivate->pBitmap);
                            // this was missing V0.9.12 (2001-05-20) [umoeller]

                free(pPrivate);
            } // end if (pPrivate)
            mrc = pWidget->pfnwpDefWidgetProc(hwnd, msg, mp1, mp2);
        break; }

        default:
            if (pWidget)
                mrc = pWidget->pfnwpDefWidgetProc(hwnd, msg, mp1, mp2);
            else
                mrc = WinDefWindowProc(hwnd, msg, mp1, mp2);
    } // end switch(msg)

    return (mrc);
}

/* ******************************************************************
 *
 *   Exported procedures
 *
 ********************************************************************/

/*
 *@@ TwgtInitModule:
 *      required export with ordinal 1, which must tell
 *      the XCenter how many widgets this DLL provides,
 *      and give the XCenter an array of XCENTERWIDGETCLASS
 *      structures describing the widgets.
 *
 *      With this call, you are given the module handle of
 *      XFLDR.DLL. For convenience, you may resolve imports
 *      for some useful functions which are exported thru
 *      src\shared\xwp.def. See the code below.
 *
 *      This function must also register the PM window classes
 *      which are specified in the XCENTERWIDGETCLASS array
 *      entries. For this, you are given a HAB which you
 *      should pass to WinRegisterClass. For the window
 *      class style (4th param to WinRegisterClass),
 *      you should specify
 *
 +          CS_PARENTCLIP | CS_SIZEREDRAW | CS_SYNCPAINT
 *
 *      Your widget window _will_ be resized, even if you're
 *      not planning it to be.
 *
 *      This function only gets called _once_ when the widget
 *      DLL has been successfully loaded by the XCenter. If
 *      there are several instances of a widget running (in
 *      the same or in several XCenters), this function does
 *      not get called again. However, since the XCenter unloads
 *      the widget DLLs again if they are no longer referenced
 *      by any XCenter, this might get called again when the
 *      DLL is re-loaded.
 *
 *      There will ever be only one load occurence of the DLL.
 *      The XCenter manages sharing the DLL between several
 *      XCenters. As a result, it doesn't matter if the DLL
 *      has INITINSTANCE etc. set or not.
 *
 *      If this returns 0, this is considered an error, and the
 *      DLL will be unloaded again immediately.
 *
 *      If this returns any value > 0, *ppaClasses must be
 *      set to a static array (best placed in the DLL's
 *      global data) of XCENTERWIDGETCLASS structures,
 *      which must have as many entries as the return value.
 *
 */

ULONG EXPENTRY TwgtInitModule(HAB hab,         // XCenter's anchor block
                              HMODULE hmodPlugin, // module handle of the widget DLL
                              HMODULE hmodXFLDR,    // XFLDR.DLL module handle
                              PXCENTERWIDGETCLASS *ppaClasses,
                              PSZ pszErrorMsg)  // if 0 is returned, 500 bytes of error msg
{
    ULONG   ulrc = 0,
            ul = 0;
    BOOL    fImportsFailed = FALSE;

    // resolve imports from XFLDR.DLL (this is basically
    // a copy of the doshResolveImports code, but we can't
    // use that before resolving...)
    for (ul = 0;
         ul < sizeof(G_aImports) / sizeof(G_aImports[0]); // array item count
         ul++)
    {
        if (DosQueryProcAddr(hmodXFLDR,
                             0,               // ordinal, ignored
                             (PSZ)G_aImports[ul].pcszFunctionName,
                             G_aImports[ul].ppFuncAddress)
                    != NO_ERROR)
        {
            strcpy(pszErrorMsg, "Import ");
            strcat(pszErrorMsg, G_aImports[ul].pcszFunctionName);
            strcat(pszErrorMsg, " failed.");
            fImportsFailed = TRUE;
            break;
        }
    }

    if (!fImportsFailed)
    {
        if (!WinRegisterClass(hab,
                              WNDCLASS_WIDGET_SENTINEL,
                              fnwpMonitorWidgets,
                              CS_PARENTCLIP | CS_SIZEREDRAW | CS_SYNCPAINT,
                              sizeof(PWIDGETPRIVATE))
                                    // extra memory to reserve for QWL_USER
                            )
            strcpy(pszErrorMsg, "WinRegisterClass failed.");
        else
        {
            // no error:
            // return classes
            *ppaClasses = G_WidgetClasses;
            // no. of classes in this DLL:
            ulrc = sizeof(G_WidgetClasses) / sizeof(G_WidgetClasses[0]);
        }
    }

    return (ulrc);
}

/*
 *@@ TwgtUnInitModule:
 *      optional export with ordinal 2, which can clean
 *      up global widget class data.
 *
 *      This gets called by the XCenter right before
 *      a widget DLL gets unloaded. Note that this
 *      gets called even if the "init module" export
 *      returned 0 (meaning an error) and the DLL
 *      gets unloaded right away.
 */

VOID EXPENTRY TwgtUnInitModule(VOID)
{
}

/*
 *@@ TwgtQueryVersion:
 *      this new export with ordinal 3 can return the
 *      XWorkplace version number which is required
 *      for this widget to run. For example, if this
 *      returns 0.9.10, this widget will not run on
 *      earlier XWorkplace versions.
 *
 *      NOTE: This export was mainly added because the
 *      prototype for the "Init" export was changed
 *      with V0.9.9. If this returns 0.9.9, it is
 *      assumed that the INIT export understands
 *      the new FNWGTINITMODULE_099 format (see center.h).
 */

VOID EXPENTRY TwgtQueryVersion(PULONG pulMajor,
                               PULONG pulMinor,
                               PULONG pulRevision)
{
    *pulMajor = XFOLDER_MAJOR;              // dlgids.h
    *pulMinor = XFOLDER_MINOR;
    *pulRevision = XFOLDER_REVISION;
}

