
/*
 *@@sourcefile w_pulse.c:
 *      XCenter "Pulse" widget implementation.
 *      This is built into the XCenter and not in
 *      a plugin DLL.
 *
 *      Function prefix for this file:
 *      --  Bwgt*
 *
 *      This is all new with V0.9.7.
 *
 *@@added V0.9.7 (2000-11-27) [umoeller]
 *@@header "shared\center.h"
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
#define INCL_DOSEXCEPTIONS
#define INCL_DOSSEMAPHORES
#define INCL_DOSERRORS

#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WININPUT
#define INCL_WINSYS
#define INCL_WINTIMER

#define INCL_GPICONTROL
#define INCL_GPIPRIMITIVES
#define INCL_GPILOGCOLORTABLE
#define INCL_GPIREGIONS
#include <os2.h>

// C library headers
#include <stdio.h>
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\gpih.h"               // GPI helper routines
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\winh.h"               // PM helper routines
#include "helpers\stringh.h"            // string helper routines
#include "helpers\timer.h"              // replacement PM timers
#include "helpers\xstring.h"            // extended string helpers

// SOM headers which don't crash with prec. header files

// XWorkplace implementation headers
#include "shared\common.h"              // the majestic XWorkplace include file

#include "shared\center.h"              // public XCenter interfaces

#pragma hdrstop                     // VAC++ keeps crashing otherwise

/* ******************************************************************
 *
 *   Private widget instance data
 *
 ********************************************************************/

/*
 *@@ PULSESETUP:
 *      instance data to which setup strings correspond.
 *      This is also a member of WIDGETPRIVATE.
 *
 *      Putting these settings into a separate structure
 *      is no requirement, but comes in handy if you
 *      want to use the same setup string routines on
 *      both the open widget window and a settings dialog.
 *
 *@@added V0.9.7 (2000-12-07) [umoeller]
 */

typedef struct _PULSESETUP
{
    LONG            lcolBackground,
                    lcolGraph,
                    lcolGraphIntr,
                    lcolText;

    PSZ             pszFont;
            // if != NULL, non-default font (in "8.Helv" format);
            // this has been allocated using local malloc()!

    LONG            cx;
            // current width; we're sizeable, and we wanna
            // store this
} PULSESETUP, *PPULSESETUP;

/*
 *@@ WIDGETPRIVATE:
 *      more window data for the "pulse" widget.
 *
 *      An instance of this is created on WM_CREATE in
 *      fnwpPulseWidget and stored in XCENTERWIDGET.pUser.
 */

typedef struct _WIDGETPRIVATE
{
    PXCENTERWIDGET pWidget;
            // reverse ptr to general widget data ptr; we need
            // that all the time and don't want to pass it on
            // the stack with each function call

    PULSESETUP      Setup;
            // widget settings that correspond to a setup string

    ULONG           ulTimerID;      // if != NULLHANDLE, clock timer is running

    HDC             hdcMem;
    HPS             hpsMem;

    HBITMAP         hbmGraph;       // bitmap for pulse graph; this contains only
                                    // the "client" (without the 3D frame)
    BOOL            fUpdateGraph;
                                    // if TRUE, PwgtPaint recreates the entire
                                    // bitmap
    PDOSHPERFSYS    pPerfData;      // performance data (doshPerf* calls)

    ULONG           cLoads;
    PLONG           palLoads;       // ptr to an array of LONGs containing previous
                                    // CPU loads
    PLONG           palIntrs;       // ptr to an array of LONGs containing previous
                                    // CPU interrupt loads
                                    // added V0.9.9 (2001-03-14) [umoeller]

    APIRET          arc;            // if != NO_ERROR, an error occured, and
                                    // the error code is displayed instead.

    BOOL            fCrashed;       // set to TRUE if the pulse crashed somewhere.
                                    // This will disable display then to avoid
                                    // crashing again on each timer tick.

} WIDGETPRIVATE, *PWIDGETPRIVATE;

/* ******************************************************************
 *
 *   Widget setup management
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
 *@@ PwgtClearSetup:
 *      cleans up the data in the specified setup
 *      structure, but does not free the structure
 *      itself.
 */

VOID PwgtClearSetup(PPULSESETUP pSetup)
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
 *@@ PwgtScanSetup:
 *      scans the given setup string and translates
 *      its data into the specified binary setup
 *      structure.
 *
 *      NOTE: It is assumed that pSetup is zeroed
 *      out. We do not clean up previous data here.
 *
 *@@added V0.9.7 (2000-12-07) [umoeller]
 *@@changed V0.9.9 (2001-03-14) [umoeller]: added interrupts graph
 */

VOID PwgtScanSetup(const char *pcszSetupString,
                   PPULSESETUP pSetup)
{
    PSZ p;

    // width
    p = ctrScanSetupString(pcszSetupString,
                           "WIDTH");
    if (p)
    {
        pSetup->cx = atoi(p);
        ctrFreeSetupValue(p);
    }
    else
        pSetup->cx = 200;

    // background color
    p = ctrScanSetupString(pcszSetupString,
                           "BGNDCOL");
    if (p)
    {
        pSetup->lcolBackground = ctrParseColorString(p);
        ctrFreeSetupValue(p);
    }
    else
        pSetup->lcolBackground = WinQuerySysColor(HWND_DESKTOP, SYSCLR_DIALOGBACKGROUND, 0);

    // graph color:
    p = ctrScanSetupString(pcszSetupString,
                           "GRPHCOL");
    if (p)
    {
        pSetup->lcolGraph = ctrParseColorString(p);
        ctrFreeSetupValue(p);
    }
    else
        pSetup->lcolGraph = RGBCOL_DARKCYAN; // RGBCOL_BLUE;

    // graph color: (interrupt load)
    p = ctrScanSetupString(pcszSetupString,
                           "GRPHINTRCOL");
    if (p)
    {
        pSetup->lcolGraphIntr = ctrParseColorString(p);
        ctrFreeSetupValue(p);
    }
    else
        pSetup->lcolGraphIntr = RGBCOL_DARKBLUE;


    // text color:
    p = ctrScanSetupString(pcszSetupString,
                           "TEXTCOL");
    if (p)
    {
        pSetup->lcolText = ctrParseColorString(p);
        ctrFreeSetupValue(p);
    }
    else
        pSetup->lcolText = WinQuerySysColor(HWND_DESKTOP, SYSCLR_WINDOWSTATICTEXT, 0);

    // font:
    // we set the font presparam, which automatically
    // affects the cached presentation spaces
    p = ctrScanSetupString(pcszSetupString,
                           "FONT");
    if (p)
    {
        pSetup->pszFont = strdup(p);
        ctrFreeSetupValue(p);
    }
    // else: leave this field null
}

/*
 *@@ PwgtSaveSetup:
 *      composes a new setup string.
 *      The caller must invoke xstrClear on the
 *      string after use.
 */

VOID PwgtSaveSetup(PXSTRING pstrSetup,       // out: setup string (is cleared first)
                   PPULSESETUP pSetup)
{
    CHAR    szTemp[100];
    PSZ     psz = 0;
    xstrInit(pstrSetup, 100);

    sprintf(szTemp, "WIDTH=%d;",
            pSetup->cx);
    xstrcat(pstrSetup, szTemp, 0);

    sprintf(szTemp, "BGNDCOL=%06lX;",
            pSetup->lcolBackground);
    xstrcat(pstrSetup, szTemp, 0);

    sprintf(szTemp, "GRPHCOL=%06lX;",
            pSetup->lcolGraph);
    xstrcat(pstrSetup, szTemp, 0);

    sprintf(szTemp, "TEXTCOL=%06lX;",
            pSetup->lcolText);
    xstrcat(pstrSetup, szTemp, 0);

    if (pSetup->pszFont)
    {
        // non-default font:
        sprintf(szTemp, "FONT=%s;",
                pSetup->pszFont);
        xstrcat(pstrSetup, szTemp, 0);
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
 *   Callbacks stored in XCENTERWIDGET
 *
 ********************************************************************/

/*
 *@@ PwgtSetupStringChanged:
 *      this gets called from ctrSetSetupString if
 *      the setup string for a widget has changed.
 *
 *      This procedure's address is stored in
 *      XCENTERWIDGET so that the XCenter knows that
 *      we can do this.
 */

VOID EXPENTRY PwgtSetupStringChanged(PXCENTERWIDGET pWidget,
                                     const char *pcszNewSetupString)
{
    PWIDGETPRIVATE pPrivate = (PWIDGETPRIVATE)pWidget->pUser;
    if (pPrivate)
    {
        // reinitialize the setup data
        PwgtClearSetup(&pPrivate->Setup);
        PwgtScanSetup(pcszNewSetupString,
                      &pPrivate->Setup);
    }
}

// VOID EXPENTRY PwgtShowSettingsDlg(PWIDGETSETTINGSDLGDATA pData)

/* ******************************************************************
 *
 *   PM window class implementation
 *
 ********************************************************************/

/*
 *@@ PwgtCreate:
 *
 */

MRESULT PwgtCreate(HWND hwnd, MPARAM mp1)
{
    MRESULT mrc = 0;        // continue window creation

    PSZ     p = NULL;
    APIRET  arc = NO_ERROR;

    PXCENTERWIDGET pWidget = (PXCENTERWIDGET)mp1;
    PWIDGETPRIVATE pPrivate = malloc(sizeof(WIDGETPRIVATE));
    memset(pPrivate, 0, sizeof(WIDGETPRIVATE));
    // link the two together
    pWidget->pUser = pPrivate;
    pPrivate->pWidget = pWidget;

    PwgtScanSetup(pWidget->pcszSetupString,
                  &pPrivate->Setup);

    // set window font (this affects all the cached presentation
    // spaces we use)
    winhSetWindowFont(hwnd,
                      (pPrivate->Setup.pszFont)
                       ? pPrivate->Setup.pszFont
                       // default font: use the same as in the rest of XWorkplace:
                       : cmnQueryDefaultFont());

    // enable context menu help
    pWidget->pcszHelpLibrary = cmnQueryHelpLibrary();
    pWidget->ulHelpPanelID = ID_XSH_WIDGET_PULSE_MAIN;

    pPrivate->arc = doshPerfOpen(&pPrivate->pPerfData);

    if (pPrivate->arc == NO_ERROR)
    {
        pPrivate->ulTimerID = tmrStartXTimer((PXTIMERSET)pWidget->pGlobals->pvXTimerSet,
                                             hwnd,
                                             1,
                                             1000);
    }

    pPrivate->fUpdateGraph = TRUE;

    return (mrc);
}

/*
 *@@ PwgtControl:
 *      implementation for WM_CONTROL.
 *
 *@@added V0.9.7 (2000-12-14) [umoeller]
 */

BOOL PwgtControl(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    BOOL brc = FALSE;

    PXCENTERWIDGET pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER);
    if (pWidget)
    {
        PWIDGETPRIVATE pPrivate = (PWIDGETPRIVATE)pWidget->pUser;
        if (pPrivate)
        {
            USHORT  usID = SHORT1FROMMP(mp1),
                    usNotifyCode = SHORT2FROMMP(mp1);

            if (usID == ID_XCENTER_CLIENT)
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
                        pszl->cy = 10;

                        brc = TRUE;
                    break; }
                }
            }
        } // end if (pPrivate)
    } // end if (pWidget)

    return (brc);
}

/*
 *@@ PwgtUpdateGraph:
 *      updates the graph bitmap. This does not paint
 *      on the screen.
 *
 *      Preconditions:
 *      --  pPrivate->hbmGraph must be selected into
 *          pPrivate->hpsMem.
 *
 *@@changed V0.9.9 (2001-03-14) [umoeller]: added interrupts graph
 */

VOID PwgtUpdateGraph(HWND hwnd,
                     PWIDGETPRIVATE pPrivate)
{
    PXCENTERWIDGET pWidget = pPrivate->pWidget;
    RECTL   rclBmp;
    POINTL  ptl;

    // size for bitmap: same as widget, except
    // for the border
    WinQueryWindowRect(hwnd, &rclBmp);
    rclBmp.xRight -= 2;
    rclBmp.yTop -= 2;

    if (pPrivate->hpsMem == NULLHANDLE)
    {
        // create memory PS for bitmap
        SIZEL szlPS;
        szlPS.cx = rclBmp.xRight;
        szlPS.cy = rclBmp.yTop;
        gpihCreateMemPS(pWidget->habWidget,
                        &szlPS,
                        &pPrivate->hdcMem,
                        &pPrivate->hpsMem);
        gpihSwitchToRGB(pPrivate->hpsMem);
    }
    if (pPrivate->hbmGraph == NULLHANDLE)
    {
        pPrivate->hbmGraph = gpihCreateBitmap(pPrivate->hpsMem,
                                                rclBmp.xRight,
                                                rclBmp.yTop);
        GpiSetBitmap(pPrivate->hpsMem,
                     pPrivate->hbmGraph);
    }

    // fill the bitmap rectangle
    GpiSetColor(pPrivate->hpsMem,
                pPrivate->Setup.lcolBackground);
    gpihBox(pPrivate->hpsMem,
            DRO_FILL,
            &rclBmp);

    // go thru all values in the "Loads" LONG array
    for (ptl.x = 0;
         ((ptl.x < pPrivate->cLoads) && (ptl.x < rclBmp.xRight));
         ptl.x++)
    {
        ptl.y = 0;

        // interrupt load on bottom
        if (pPrivate->palIntrs)
        {
            GpiSetColor(pPrivate->hpsMem,
                        pPrivate->Setup.lcolGraphIntr);
            // go thru all values in the "Interrupt Loads" LONG array
            // Note: number of "loads" entries and "intrs" entries is the same
            GpiMove(pPrivate->hpsMem, &ptl);
            ptl.y += rclBmp.yTop * pPrivate->palIntrs[ptl.x] / 1000;
            GpiLine(pPrivate->hpsMem, &ptl);
        }

        // scan the CPU loads
        if (pPrivate->palLoads)
        {
            GpiSetColor(pPrivate->hpsMem,
                        pPrivate->Setup.lcolGraph);
            GpiMove(pPrivate->hpsMem, &ptl);
            ptl.y += rclBmp.yTop * pPrivate->palLoads[ptl.x] / 1000;
            GpiLine(pPrivate->hpsMem, &ptl);
        }
    }

    pPrivate->fUpdateGraph = FALSE;
}

/*
 * PwgtPaint2:
 *      this does the actual painting of the frame (if
 *      fDrawFrame==TRUE) and the pulse bitmap.
 *
 *      Gets called by PwgtPaint.
 *
 *      The specified HPS is switched to RGB mode before
 *      painting.
 *
 *      If DosPerfSysCall succeeds, this diplays the pulse.
 *      Otherwise an error msg is displayed.
 *
 *@@changed V0.9.9 (2001-03-14) [umoeller]: added interrupts graph
 */

VOID PwgtPaint2(HWND hwnd,
                PWIDGETPRIVATE pPrivate,
                HPS hps,
                BOOL fDrawFrame)     // in: if TRUE, everything is painted
{
    TRY_LOUD(excpt1)
    {
        PXCENTERWIDGET pWidget = pPrivate->pWidget;
        RECTL       rclWin;
        ULONG       ulBorder = 1;
        CHAR        szPaint[100] = "";
        ULONG       ulPaintLen = 0;

        // now paint button frame
        WinQueryWindowRect(hwnd,
                           &rclWin);        // exclusive
        gpihSwitchToRGB(hps);

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

            gpihDraw3DFrame(hps,
                            &rclWin,        // inclusive
                            ulBorder,
                            lDark,
                            lLight);
        }

        if (pPrivate->arc == NO_ERROR)
        {
            // performance counters are working:
            PCOUNTRYSETTINGS pCountrySettings = (PCOUNTRYSETTINGS)pWidget->pGlobals->pCountrySettings;
            POINTL      ptlBmpDest;
            LONG        lLoad1000 = 0;

            if (pPrivate->fUpdateGraph)
                // graph bitmap needs to be updated:
                PwgtUpdateGraph(hwnd, pPrivate);

            ptlBmpDest.x = rclWin.xLeft + ulBorder;
            ptlBmpDest.y = rclWin.yBottom + ulBorder;
            // now paint graph from bitmap
            WinDrawBitmap(hps,
                          pPrivate->hbmGraph,
                          NULL,     // entire bitmap
                          &ptlBmpDest,
                          0, 0,
                          DBM_NORMAL);

            // in the string, display the total load
            // (busy plus interrupt) V0.9.9 (2001-03-14) [umoeller]
            if (pPrivate->palLoads)
                lLoad1000 = pPrivate->pPerfData->palLoads[0];
            if (pPrivate->palIntrs)
                lLoad1000 += pPrivate->pPerfData->palIntrs[0];

            sprintf(szPaint, "%lu%c%lu%c",
                    lLoad1000 / 10,
                    pCountrySettings->cDecimal,
                    lLoad1000 % 10,
                    '%');
        }
        else
        {
            // performance counters are not working:
            // display error message
            rclWin.xLeft++;     // was made inclusive above
            rclWin.yBottom++;
            WinFillRect(hps, &rclWin, pPrivate->Setup.lcolBackground);
            sprintf(szPaint, "E %lu", pPrivate->arc);
        }

        ulPaintLen = strlen(szPaint);
        if (ulPaintLen)
            WinDrawText(hps,
                        ulPaintLen,
                        szPaint,
                        &rclWin,
                        0,      // background, ignored anyway
                        pPrivate->Setup.lcolText,
                        DT_CENTER | DT_VCENTER);
    }
    CATCH(excpt1)
    {
        pPrivate->fCrashed = TRUE;
    } END_CATCH();
}

/*
 *@@ PwgtPaint:
 *      implementation for WM_PAINT.
 */

VOID PwgtPaint(HWND hwnd)
{
    HPS hps = WinBeginPaint(hwnd, NULLHANDLE, NULL);
    if (hps)
    {
        // get widget data and its button data from QWL_USER
        PXCENTERWIDGET pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER);
        if (pWidget)
        {
            PWIDGETPRIVATE pPrivate = (PWIDGETPRIVATE)pWidget->pUser;
            if (pPrivate)
            {
                PwgtPaint2(hwnd,
                           pPrivate,
                           hps,
                           TRUE);        // draw frame
            } // end if (pPrivate)
        } // end if (pWidget)
        WinEndPaint(hps);
    } // end if (hps)
}

/*
 *@@ PwgtGetNewLoad:
 *      updates the CPU loads array, updates the graph bitmap
 *      and invalidates the window.
 *
 *@@changed V0.9.9 (2001-03-14) [umoeller]: added interrupts graph
 */

VOID PwgtGetNewLoad(HWND hwnd)
{
    PXCENTERWIDGET pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER);
    if (pWidget)
    {
        PWIDGETPRIVATE pPrivate = (PWIDGETPRIVATE)pWidget->pUser;
        if (pPrivate)
        {
            TRY_LOUD(excpt1)
            {
                if (    (!pPrivate->fCrashed)
                     && (pPrivate->arc == NO_ERROR)
                   )
                {
                    HPS hps;
                    RECTL rclClient;
                    WinQueryWindowRect(hwnd, &rclClient);
                    if (rclClient.xRight)
                    {
                        ULONG ulGraphCX = rclClient.xRight - 2;    // minus border
                        if (pPrivate->palLoads == NULL)
                        {
                            // create array of loads
                            pPrivate->cLoads = ulGraphCX;
                            pPrivate->palLoads = (PLONG)malloc(sizeof(LONG) * pPrivate->cLoads);
                            memset(pPrivate->palLoads, 0, sizeof(LONG) * pPrivate->cLoads);
                        }

                        if (pPrivate->palIntrs == NULL)
                        {
                            // create array of interrupt loads
                            pPrivate->cLoads = ulGraphCX;
                            pPrivate->palIntrs = (PLONG)malloc(sizeof(LONG) * pPrivate->cLoads);
                            memset(pPrivate->palIntrs, 0, sizeof(LONG) * pPrivate->cLoads);
                        }


                        if (pPrivate->palLoads || pPrivate->palIntrs)
                        {
                            pPrivate->arc = doshPerfGet(pPrivate->pPerfData);
                            if (pPrivate->arc == NO_ERROR)
                            {
                                // in the array of loads, move each entry one to the front;
                                // drop the oldest entry
                                if (pPrivate->palLoads)
                                {
                                    memcpy(&pPrivate->palLoads[0],
                                           &pPrivate->palLoads[1],
                                           sizeof(LONG) * (pPrivate->cLoads - 1));

                                    // and update the last entry with the current value
                                    pPrivate->palLoads[pPrivate->cLoads - 1]
                                        = pPrivate->pPerfData->palLoads[0];
                                }

                                if (pPrivate->palIntrs)
                                {
                                    memcpy(&pPrivate->palIntrs[0],
                                           &pPrivate->palIntrs[1],
                                           sizeof(LONG) * (pPrivate->cLoads - 1));

                                    // and update the last entry with the current value
                                    pPrivate->palIntrs[pPrivate->cLoads - 1]
                                        = pPrivate->pPerfData->palIntrs[0];
                                }

                                // update display
                                pPrivate->fUpdateGraph = TRUE;
                            }
                        }

                        hps = WinGetPS(hwnd);
                        PwgtPaint2(hwnd,
                                   pPrivate,
                                   hps,
                                   FALSE);       // do not draw frame
                        WinReleasePS(hps);
                    } // end if (rclClient.xRight)
                }
            }
            CATCH(excpt1)
            {
                pPrivate->fCrashed = TRUE;
            } END_CATCH();
        } // end if (pPrivate)
    } // end if (pWidget)
}

/*
 *@@ PwgtWindowPosChanged:
 *      implementation for WM_WINDOWPOSCHANGED.
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
 *@@changed V0.9.9 (2001-03-14) [umoeller]: added interrupts graph
 */

VOID PwgtWindowPosChanged(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    TRY_LOUD(excpt1)
    {
        PXCENTERWIDGET pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER);
        if (pWidget)
        {
            PWIDGETPRIVATE pPrivate = (PWIDGETPRIVATE)pWidget->pUser;
            if (pPrivate)
            {
                PSWP pswpNew = (PSWP)mp1,
                     pswpOld = pswpNew + 1;
                if (pswpNew->fl & SWP_SIZE)
                {
                    // window was resized:

                    // destroy the buffer bitmap because we
                    // need a new one with a different size
                    if (pPrivate->hbmGraph)
                    {
                        GpiSetBitmap(pPrivate->hpsMem, NULLHANDLE);
                        GpiDeleteBitmap(pPrivate->hbmGraph);
                        pPrivate->hbmGraph = NULLHANDLE;
                                // recreated in PwgtUpdateGraph with the current size
                    }

                    if (pPrivate->hpsMem)
                    {
                        // memory PS already allocated: have those recreated
                        // as well
                        GpiDestroyPS(pPrivate->hpsMem);
                        pPrivate->hpsMem = NULLHANDLE;
                        DevCloseDC(pPrivate->hdcMem);
                        pPrivate->hdcMem = NULLHANDLE;
                    }

                    if (pswpNew->cx != pswpOld->cx)
                    {
                        XSTRING strSetup;
                        // width changed:
                        if (pPrivate->palLoads)
                        {
                            // we also need a new array of past loads
                            // since the array is cx items wide...
                            // so reallocate the array, but keep past
                            // values
                            ULONG ulNewClientCX = pswpNew->cx - 2;
                            PLONG palNewLoads = (PLONG)malloc(sizeof(LONG) * ulNewClientCX);

                            if (ulNewClientCX > pPrivate->cLoads)
                            {
                                // window has become wider:
                                // fill the front with zeroes
                                memset(palNewLoads,
                                       0,
                                       (ulNewClientCX - pPrivate->cLoads) * sizeof(LONG));
                                // and copy old values after that
                                memcpy(&palNewLoads[(ulNewClientCX - pPrivate->cLoads)],
                                       pPrivate->palLoads,
                                       pPrivate->cLoads * sizeof(LONG));
                            }
                            else
                            {
                                // window has become smaller:
                                // e.g. ulnewClientCX = 100
                                //      pPrivate->cLoads = 200
                                // drop the first items
                                ULONG ul = 0;
                                memcpy(palNewLoads,
                                       &pPrivate->palLoads[pPrivate->cLoads - ulNewClientCX],
                                       ulNewClientCX * sizeof(LONG));
                            }

                            free(pPrivate->palLoads);
                            pPrivate->palLoads = palNewLoads;

                            // do the same for the interrupt load
                            if (pPrivate->palIntrs)
                            {
                                PLONG palNewIntrs = (PLONG)malloc(sizeof(LONG) * ulNewClientCX);

                                if (ulNewClientCX > pPrivate->cLoads)
                                {
                                    // window has become wider:
                                    // fill the front with zeroes
                                    memset(palNewIntrs,
                                           0,
                                           (ulNewClientCX - pPrivate->cLoads) * sizeof(LONG));
                                    // and copy old values after that
                                    memcpy(&palNewIntrs[(ulNewClientCX - pPrivate->cLoads)],
                                           pPrivate->palIntrs,
                                           pPrivate->cLoads * sizeof(LONG));
                                }
                                else
                                {
                                    // window has become smaller:
                                    // e.g. ulnewClientCX = 100
                                    //      pPrivate->cLoads = 200
                                    // drop the first items
                                    ULONG ul = 0;
                                    memcpy(palNewIntrs,
                                           &pPrivate->palIntrs[pPrivate->cLoads - ulNewClientCX],
                                           ulNewClientCX * sizeof(LONG));
                                }

                                free(pPrivate->palIntrs);
                                pPrivate->palIntrs = palNewIntrs;
                            }

                            pPrivate->cLoads = ulNewClientCX;

                        } // end if (pPrivate->palLoads)

                        pPrivate->Setup.cx = pswpNew->cx;
                        PwgtSaveSetup(&strSetup,
                                      &pPrivate->Setup);
                        if (strSetup.ulLength)
                            WinSendMsg(pWidget->pGlobals->hwndClient,
                                       XCM_SAVESETUP,
                                       (MPARAM)hwnd,
                                       (MPARAM)strSetup.psz);
                        xstrClear(&strSetup);
                    } // end if (pswpNew->cx != pswpOld->cx)

                    // force recreation of bitmap
                    pPrivate->fUpdateGraph = TRUE;
                    WinInvalidateRect(hwnd, NULL, FALSE);
                } // end if (pswpNew->fl & SWP_SIZE)
            } // end if (pPrivate)
        } // end if (pWidget)
    }
    CATCH(excpt1)
    { } END_CATCH();
}

/*
 *@@ PwgtPresParamChanged:
 *      implementation for WM_PRESPARAMCHANGED.
 */

VOID PwgtPresParamChanged(HWND hwnd,
                          ULONG ulAttrChanged)
{
    PXCENTERWIDGET pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER);
    if (pWidget)
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
                        = winhQueryPresColor(hwnd,
                                             PP_BACKGROUNDCOLOR,
                                             FALSE,
                                             SYSCLR_DIALOGBACKGROUND);
                    pPrivate->Setup.lcolGraph
                        = winhQueryPresColor(hwnd,
                                             PP_FOREGROUNDCOLOR,
                                             FALSE,
                                             -1);
                    if (pPrivate->Setup.lcolGraph == -1)
                        pPrivate->Setup.lcolGraph = RGBCOL_DARKCYAN;
                break;

                case PP_FONTNAMESIZE:
                {
                    PSZ pszFont = 0;
                    if (pPrivate->Setup.pszFont)
                    {
                        free(pPrivate->Setup.pszFont);
                        pPrivate->Setup.pszFont = NULL;
                    }

                    pszFont = winhQueryWindowFont(hwnd);
                    if (pszFont)
                    {
                        // we must use local malloc() for the font
                        pPrivate->Setup.pszFont = strdup(pszFont);
                        winhFree(pszFont);
                    }
                break; }

                default:
                    fInvalidate = FALSE;
            }

            if (fInvalidate)
            {
                XSTRING strSetup;
                // force recreation of bitmap
                pPrivate->fUpdateGraph = TRUE;
                WinInvalidateRect(hwnd, NULL, FALSE);

                PwgtSaveSetup(&strSetup,
                              &pPrivate->Setup);
                if (strSetup.ulLength)
                    WinSendMsg(pWidget->pGlobals->hwndClient,
                               XCM_SAVESETUP,
                               (MPARAM)hwnd,
                               (MPARAM)strSetup.psz);
                xstrClear(&strSetup);
            }
        } // end if (pPrivate)
    } // end if (pWidget)
}

/*
 *@@ PwgtDestroy:
 *      implementation for WM_DESTROY.
 *
 *@@changed V0.9.9 (2001-02-06) [umoeller]: fixed call to doshPerfClose
 */

VOID PwgtDestroy(HWND hwnd)
{
    PXCENTERWIDGET pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER);
    if (pWidget)
    {
        PWIDGETPRIVATE pPrivate = (PWIDGETPRIVATE)pWidget->pUser;
        if (pPrivate)
        {
            if (pPrivate->ulTimerID)
                tmrStopXTimer((PXTIMERSET)pWidget->pGlobals->pvXTimerSet,
                              hwnd,
                              pPrivate->ulTimerID);

            if (pPrivate->hbmGraph)
            {
                GpiSetBitmap(pPrivate->hpsMem, NULLHANDLE);
                GpiDeleteBitmap(pPrivate->hbmGraph);
            }

            if (pPrivate->hpsMem)
                GpiDestroyPS(pPrivate->hpsMem);
            if (pPrivate->hdcMem)
                DevCloseDC(pPrivate->hdcMem);

            if (pPrivate->pPerfData)
                doshPerfClose(&pPrivate->pPerfData);

            free(pPrivate);
        } // end if (pPrivate)
    } // end if (pWidget)
}

/*
 *@@ fnwpPulseWidget:
 *      window procedure for the "Pulse" widget class.
 *
 *      Supported setup strings:
 *
 *      --  "TEXTCOL=rrggbb": color for the percentage text (if displayed).
 *
 *      --  "BGNDCOL=rrggbb": background color.
 *
 *      --  "GRPHCOL=rrggbb": graph color.
 *
 *      --  "FONT=point.face": presentation font.
 *
 *      --  "WIDTH=cx": widget display width.
 */

MRESULT EXPENTRY fnwpPulseWidget(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

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
         *
         *      Each widget must write its desired width into
         *      XCENTERWIDGET.cx and cy.
         */

        case WM_CREATE:
            WinSetWindowPtr(hwnd, QWL_USER, mp1);
            mrc = PwgtCreate(hwnd, mp1);
        break;

        /*
         * WM_CONTROL:
         *      process notifications/queries from the XCenter.
         */

        case WM_CONTROL:
            mrc = (MPARAM)PwgtControl(hwnd, mp1, mp2);
        break;

        /*
         * WM_PAINT:
         *
         */

        case WM_PAINT:
            PwgtPaint(hwnd);
        break;

        /*
         * WM_TIMER:
         *      clock timer --> repaint.
         */

        case WM_TIMER:
            PwgtGetNewLoad(hwnd);
                // repaints!
        break;

        /*
         * WM_WINDOWPOSCHANGED:
         *      on window resize, allocate new bitmap.
         */

        case WM_WINDOWPOSCHANGED:
            PwgtWindowPosChanged(hwnd, mp1, mp2);
        break;

        /*
         * WM_PRESPARAMCHANGED:
         *
         */

        case WM_PRESPARAMCHANGED:
            PwgtPresParamChanged(hwnd, (ULONG)mp1);
        break;

        /*
         * WM_DESTROY:
         *
         */

        case WM_DESTROY:
            PwgtDestroy(hwnd);
            mrc = ctrDefWidgetProc(hwnd, msg, mp1, mp2);
        break;

        default:
            mrc = ctrDefWidgetProc(hwnd, msg, mp1, mp2);
    } // end switch(msg)

    return (mrc);
}


