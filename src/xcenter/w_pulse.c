
/*
 *@@sourcefile w_pulse.c:
 *      XCenter "Pulse" widget implementation.
 *      This is build into the XCenter and not in
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
 *      This is also a member of PULSEWIDGETDATA.
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
                    lcolText;

    PSZ         pszFont;
            // if != NULL, non-default font (in "8.Helv" format);
            // this has been allocated using local malloc()!
} PULSESETUP, *PPULSESETUP;

/*
 *@@ PULSEWIDGETDATA:
 *      more window data for the "pulse" widget.
 *
 *      An instance of this is created on WM_CREATE in
 *      fnwpPulseWidget and stored in XCENTERWIDGETVIEW.pUser.
 */

typedef struct _PULSEWIDGETDATA
{
    PXCENTERWIDGETVIEW pViewData;
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

    APIRET          arc;            // if != NO_ERROR, an error occured, and
                                    // the error code is displayed instead.
} PULSEWIDGETDATA, *PPULSEWIDGETDATA;

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
 */

VOID PwgtScanSetup(const char *pcszSetupString,
                   PLONG plCXWanted,
                   PPULSESETUP pSetup)
{
    PSZ p;
    p = ctrScanSetupString(pcszSetupString,
                           "WIDTH");
    if (p)
    {
        *plCXWanted = atoi(p);
        ctrFreeSetupValue(p);
    }

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
                   ULONG cxCurrent,
                   PPULSESETUP pSetup)
{
    CHAR    szTemp[100];
    PSZ     psz = 0;
    xstrInit(pstrSetup, 100);

    sprintf(szTemp, "WIDTH=%d" SETUP_SEPARATOR,
            cxCurrent);
    xstrcat(pstrSetup, szTemp);

    sprintf(szTemp, "BGNDCOL=%06lX" SETUP_SEPARATOR,
            pSetup->lcolBackground);
    xstrcat(pstrSetup, szTemp);

    sprintf(szTemp, "GRPHCOL=%06lX" SETUP_SEPARATOR,
            pSetup->lcolGraph);
    xstrcat(pstrSetup, szTemp);

    sprintf(szTemp, "TEXTCOL=%06lX" SETUP_SEPARATOR,
            pSetup->lcolText);
    xstrcat(pstrSetup, szTemp);

    if (pSetup->pszFont)
    {
        // non-default font:
        sprintf(szTemp, "FONT=%s" SETUP_SEPARATOR,
                pSetup->pszFont);
        xstrcat(pstrSetup, szTemp);
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
 *   Callbacks stored in XCENTERWIDGETVIEW
 *
 ********************************************************************/

/*
 *@@ PwgtSetupStringChanged:
 *      this gets called from ctrSetSetupString if
 *      the setup string for a widget has changed.
 *
 *      This procedure's address is stored in
 *      XCENTERWIDGETVIEW so that the XCenter knows that
 *      we can do this.
 */

VOID EXPENTRY PwgtSetupStringChanged(PXCENTERWIDGETVIEW pViewData,
                                     const char *pcszNewSetupString)
{
    PPULSEWIDGETDATA pPulseData = (PPULSEWIDGETDATA)pViewData->pUser;
    if (pPulseData)
    {
        // reinitialize the setup data
        PwgtClearSetup(&pPulseData->Setup);
        PwgtScanSetup(pcszNewSetupString,
                      &pPulseData->pViewData->cxWanted,
                      &pPulseData->Setup);
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

    PXCENTERWIDGETVIEW pViewData = (PXCENTERWIDGETVIEW)mp1;
    PPULSEWIDGETDATA pPulseData = malloc(sizeof(PULSEWIDGETDATA));
    memset(pPulseData, 0, sizeof(PULSEWIDGETDATA));
    // link the two together
    pViewData->pUser = pPulseData;
    pPulseData->pViewData = pViewData;

    pViewData->cxWanted = 200;
    pViewData->cyWanted = 10;

    PwgtScanSetup(pViewData->pcszSetupString,
                  &pViewData->cxWanted,
                  &pPulseData->Setup);

    // set window font (this affects all the cached presentation
    // spaces we use)
    winhSetWindowFont(hwnd,
                      (pPulseData->Setup.pszFont)
                       ? pPulseData->Setup.pszFont
                       // default font: use the same as in the rest of XWorkplace:
                       : cmnQueryDefaultFont());

    // enable context menu help
    pViewData->pcszHelpLibrary = cmnQueryHelpLibrary();
    pViewData->ulHelpPanelID = ID_XSH_WIDGET_PULSE_MAIN;

    pPulseData->arc = doshPerfOpen(&pPulseData->pPerfData);

    if (pPulseData->arc == NO_ERROR)
    {
        pPulseData->ulTimerID = tmrStartTimer(hwnd,
                                              1,
                                              1000);
    }

    pPulseData->fUpdateGraph = TRUE;

    return (mrc);
}

/*
 *@@ PwgtUpdateGraph:
 *
 *      Preconditions:
 *      --  pPulseData->hbmGraph must be selected into
 *          pPulseData->hpsMem.
 *
 */

VOID PwgtUpdateGraph(HWND hwnd,
                     PPULSEWIDGETDATA pPulseData)
{
    PXCENTERWIDGETVIEW pViewData = pPulseData->pViewData;
    ULONG   ul = 0;
    RECTL   rclBmp;
    POINTL  ptl;
    rclBmp.xLeft = 0;
    rclBmp.xRight = pViewData->cxCurrent - 2;
    rclBmp.yBottom = 0;
    rclBmp.yTop = pViewData->cyCurrent - 2;

    // start on the left
    ptl.x = 0;

    if (pPulseData->hpsMem == NULLHANDLE)
    {
        // create memory PS for bitmap
        SIZEL szlPS;
        szlPS.cx = rclBmp.xRight;
        szlPS.cy = rclBmp.yTop;
        gpihCreateMemPS(pViewData->habWidget,
                        &szlPS,
                        &pPulseData->hdcMem,
                        &pPulseData->hpsMem);
        gpihSwitchToRGB(pPulseData->hpsMem);
    }
    if (pPulseData->hbmGraph == NULLHANDLE)
    {
        pPulseData->hbmGraph = gpihCreateBitmap(pPulseData->hpsMem,
                                                rclBmp.xRight,
                                                rclBmp.yTop);
        GpiSetBitmap(pPulseData->hpsMem,
                     pPulseData->hbmGraph);
    }

    // fill the bitmap rectangle
    gpihBox(pPulseData->hpsMem,
            DRO_FILL,
            &rclBmp,
            pPulseData->Setup.lcolBackground);

    GpiSetColor(pPulseData->hpsMem,
                pPulseData->Setup.lcolGraph);
    // go thru all values in the "Loads" LONG array
    for (; ul < pPulseData->cLoads; ul++)
    {
        ptl.y = 0;
        GpiMove(pPulseData->hpsMem, &ptl);
        ptl.y = rclBmp.yTop * pPulseData->palLoads[ul] / 1000;
        GpiLine(pPulseData->hpsMem, &ptl);

        ptl.x++;
    }

    pPulseData->fUpdateGraph = FALSE;
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
 */

VOID PwgtPaint2(HWND hwnd,
                PPULSEWIDGETDATA pPulseData,
                HPS hps,
                BOOL fDrawFrame)     // in: if TRUE, everything is painted
{
    PXCENTERWIDGETVIEW pViewData = pPulseData->pViewData;
    RECTL       rclWin;
    ULONG       ulBorder = 1;
    CHAR        szPaint[100] = "";
    ULONG       ulPaintLen = 0;

    if (pPulseData->arc == NO_ERROR)
    {
        PCOUNTRYSETTINGS pCountrySettings = (PCOUNTRYSETTINGS)pViewData->pGlobals->pCountrySettings;
        POINTL      ptlBmpDest;

        if (pPulseData->fUpdateGraph)
            // graph bitmap needs to be updated:
            PwgtUpdateGraph(hwnd, pPulseData);

        // now paint button frame
        WinQueryWindowRect(hwnd, &rclWin);
        gpihSwitchToRGB(hps);
        if (fDrawFrame)
            gpihDraw3DFrame(hps,
                            &rclWin,
                            ulBorder,
                            pViewData->pGlobals->lcol3DDark,
                            pViewData->pGlobals->lcol3DLight);

        ptlBmpDest.x = rclWin.xLeft + ulBorder;
        ptlBmpDest.y = rclWin.yBottom + ulBorder;
        // now paint graph from bitmap
        WinDrawBitmap(hps,
                      pPulseData->hbmGraph,
                      NULL,     // entire bitmap
                      &ptlBmpDest,
                      0, 0,
                      DBM_NORMAL);

        sprintf(szPaint, "%lu%c%lu%c",
                pPulseData->pPerfData->palLoads[0] / 10,
                pCountrySettings->cDecimal,
                pPulseData->pPerfData->palLoads[0] % 10,
                '%');

    }
    else
        sprintf(szPaint, "E %lu", pPulseData->arc);

    ulPaintLen = strlen(szPaint);
    if (ulPaintLen)
        WinDrawText(hps,
                    ulPaintLen,
                    szPaint,
                    &rclWin,
                    0,      // background, ignored anyway
                    pPulseData->Setup.lcolText,
                    DT_CENTER | DT_VCENTER);
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
        PXCENTERWIDGETVIEW pViewData = (PXCENTERWIDGETVIEW)WinQueryWindowPtr(hwnd, QWL_USER);
        if (pViewData)
        {
            PPULSEWIDGETDATA pPulseData = (PPULSEWIDGETDATA)pViewData->pUser;
            if (pPulseData)
            {
                PwgtPaint2(hwnd,
                           pPulseData,
                           hps,
                           TRUE);        // draw frame
            } // end if (pPulseData)
        } // end if (pViewData)
        WinEndPaint(hps);
    } // end if (hps)
}

/*
 *@@ PwgtGetNewLoad:
 *      updates the CPU loads array, updates the graph bitmap
 *      and invalidates the window.
 */

VOID PwgtGetNewLoad(HWND hwnd)
{
    PXCENTERWIDGETVIEW pViewData = (PXCENTERWIDGETVIEW)WinQueryWindowPtr(hwnd, QWL_USER);
    if (pViewData)
    {
        PPULSEWIDGETDATA pPulseData = (PPULSEWIDGETDATA)pViewData->pUser;
        if (pPulseData)
        {
            if (pPulseData->arc == NO_ERROR)
            {
                HPS hps;

                ULONG ulClientCX = pViewData->cxCurrent - 2;     // minus 2*border
                if (pPulseData->palLoads == NULL)
                {
                    // create array of loads
                    pPulseData->cLoads = ulClientCX;
                    pPulseData->palLoads = (PLONG)malloc(sizeof(LONG) * pPulseData->cLoads);
                    memset(pPulseData->palLoads, 0, sizeof(LONG) * pPulseData->cLoads);
                }

                pPulseData->arc = doshPerfGet(pPulseData->pPerfData);
                if (pPulseData->arc == NO_ERROR)
                {
                    // in the array of loads, move each entry one to the front;
                    // drop the oldest entry
                    memcpy(&pPulseData->palLoads[0],
                           &pPulseData->palLoads[1],
                           sizeof(LONG) * (pPulseData->cLoads - 1));
                    // and update the last entry with the current value
                    pPulseData->palLoads[pPulseData->cLoads - 1]
                        = pPulseData->pPerfData->palLoads[0];

                    // update display
                    pPulseData->fUpdateGraph = TRUE;
                }

                hps = WinGetPS(hwnd);
                PwgtPaint2(hwnd,
                           pPulseData,
                           hps,
                           FALSE);       // do not draw frame
                WinReleasePS(hps);
            }
        } // end if (pPulseData)
    } // end if (pViewData)
}

/*
 *@@ PwgtWindowPosChanged:
 *      implementation for WM_WINDOWPOSCHANGED.
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
 */

VOID PwgtWindowPosChanged(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    TRY_LOUD(excpt1, NULL)
    {
        PXCENTERWIDGETVIEW pViewData = (PXCENTERWIDGETVIEW)WinQueryWindowPtr(hwnd, QWL_USER);
        if (pViewData)
        {
            PPULSEWIDGETDATA pPulseData = (PPULSEWIDGETDATA)pViewData->pUser;
            if (pPulseData)
            {
                PSWP pswpNew = (PSWP)mp1,
                     pswpOld = pswpNew + 1;
                if (pswpNew->fl & SWP_SIZE)
                {
                    // window was resized:

                    // destroy the buffer bitmap because we
                    // need a new one with a different size
                    if (pPulseData->hbmGraph)
                    {
                        GpiSetBitmap(pPulseData->hpsMem, NULLHANDLE);
                        GpiDeleteBitmap(pPulseData->hbmGraph);
                        pPulseData->hbmGraph = NULLHANDLE;
                                // recreated in PwgtUpdateGraph with the current size
                    }

                    if (pPulseData->hpsMem)
                    {
                        // memory PS already allocated: have those recreated
                        // as well
                        GpiDestroyPS(pPulseData->hpsMem);
                        pPulseData->hpsMem = NULLHANDLE;
                        DevCloseDC(pPulseData->hdcMem);
                        pPulseData->hdcMem = NULLHANDLE;
                    }

                    if (pswpNew->cx != pswpOld->cx)
                    {
                        XSTRING strSetup;
                        // width changed:
                        if (pPulseData->palLoads)
                        {
                            // we also need a new array of past loads
                            // since the array is cx items wide...
                            // so reallocate the array, but keep past
                            // values
                            ULONG ulNewClientCX = pswpNew->cx - 2;
                            PLONG palNewLoads = (PLONG)malloc(sizeof(LONG) * ulNewClientCX);

                            if (ulNewClientCX > pPulseData->cLoads)
                            {
                                // window has become wider:
                                // fill the front with zeroes
                                memset(palNewLoads,
                                       0,
                                       (ulNewClientCX - pPulseData->cLoads) * sizeof(LONG));
                                // and copy old values after that
                                memcpy(&palNewLoads[(ulNewClientCX - pPulseData->cLoads)],
                                       pPulseData->palLoads,
                                       pPulseData->cLoads * sizeof(LONG));
                            }
                            else
                            {
                                // window has become smaller:
                                // e.g. ulnewClientCX = 100
                                //      pPulseData->cLoads = 200
                                // drop the first items
                                ULONG ul = 0;
                                memcpy(palNewLoads,
                                       &pPulseData->palLoads[pPulseData->cLoads - ulNewClientCX],
                                       ulNewClientCX * sizeof(LONG));
                            }

                            pPulseData->cLoads = ulNewClientCX;
                            free(pPulseData->palLoads);
                            pPulseData->palLoads = palNewLoads;
                        }

                        PwgtSaveSetup(&strSetup,
                                      pswpNew->cx,
                                      &pPulseData->Setup);
                        if (strSetup.ulLength)
                            WinSendMsg(pViewData->pGlobals->hwndClient,
                                       XCM_SAVESETUP,
                                       (MPARAM)hwnd,
                                       (MPARAM)strSetup.psz);
                        xstrClear(&strSetup);
                    } // end if (pswpNew->cx != pswpOld->cx)

                    // force recreation of bitmap
                    pPulseData->fUpdateGraph = TRUE;
                    WinInvalidateRect(hwnd, NULL, FALSE);
                } // end if (pswpNew->fl & SWP_SIZE)
            } // end if (pPulseData)
        } // end if (pViewData)
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
    PXCENTERWIDGETVIEW pViewData = (PXCENTERWIDGETVIEW)WinQueryWindowPtr(hwnd, QWL_USER);
    if (pViewData)
    {
        PPULSEWIDGETDATA pPulseData = (PPULSEWIDGETDATA)pViewData->pUser;
        if (pPulseData)
        {
            BOOL fInvalidate = TRUE;
            switch (ulAttrChanged)
            {
                case 0:     // layout palette thing dropped
                case PP_BACKGROUNDCOLOR:
                case PP_FOREGROUNDCOLOR:
                    pPulseData->Setup.lcolBackground
                        = winhQueryPresColor(hwnd,
                                             PP_BACKGROUNDCOLOR,
                                             FALSE,
                                             SYSCLR_DIALOGBACKGROUND);
                    pPulseData->Setup.lcolGraph
                        = winhQueryPresColor(hwnd,
                                             PP_FOREGROUNDCOLOR,
                                             FALSE,
                                             -1);
                    if (pPulseData->Setup.lcolGraph == -1)
                        pPulseData->Setup.lcolGraph = RGBCOL_DARKCYAN;
                break;

                case PP_FONTNAMESIZE:
                {
                    PSZ pszFont = 0;
                    if (pPulseData->Setup.pszFont)
                    {
                        free(pPulseData->Setup.pszFont);
                        pPulseData->Setup.pszFont = NULL;
                    }

                    pszFont = winhQueryWindowFont(hwnd);
                    if (pszFont)
                    {
                        // we must use local malloc() for the font
                        pPulseData->Setup.pszFont = strdup(pszFont);
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
                pPulseData->fUpdateGraph = TRUE;
                WinInvalidateRect(hwnd, NULL, FALSE);

                PwgtSaveSetup(&strSetup,
                              pViewData->cxCurrent,
                              &pPulseData->Setup);
                if (strSetup.ulLength)
                    WinSendMsg(pViewData->pGlobals->hwndClient,
                               XCM_SAVESETUP,
                               (MPARAM)hwnd,
                               (MPARAM)strSetup.psz);
                xstrClear(&strSetup);
            }
        } // end if (pPulseData)
    } // end if (pViewData)
}

/*
 *@@ PwgtDestroy:
 *      implementation for WM_DESTROY.
 */

VOID PwgtDestroy(HWND hwnd)
{
    PXCENTERWIDGETVIEW pViewData = (PXCENTERWIDGETVIEW)WinQueryWindowPtr(hwnd, QWL_USER);
    if (pViewData)
    {
        PPULSEWIDGETDATA pPulseData = (PPULSEWIDGETDATA)pViewData->pUser;
        if (pPulseData)
        {
            if (pPulseData->ulTimerID)
                tmrStopTimer(hwnd,
                             pPulseData->ulTimerID);

            if (pPulseData->hbmGraph)
            {
                GpiSetBitmap(pPulseData->hpsMem, NULLHANDLE);
                GpiDeleteBitmap(pPulseData->hbmGraph);
            }

            if (pPulseData->hpsMem)
                GpiDestroyPS(pPulseData->hpsMem);
            if (pPulseData->hdcMem)
                DevCloseDC(pPulseData->hdcMem);

            if (pPulseData->pPerfData)
                doshPerfClose(&pPulseData->pPerfData);

            free(pPulseData);
        } // end if (pPulseData)
    } // end if (pViewData)
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
         *      XCENTERWIDGETVIEW in mp1, which was created for us.
         *
         *      The first thing the widget MUST do on WM_CREATE
         *      is to store the XCENTERWIDGETVIEW pointer (from mp1)
         *      in the QWL_USER window word by calling:
         *
         *          WinSetWindowPtr(hwnd, QWL_USER, mp1);
         *
         *      We use XCENTERWIDGETVIEW.pUser for allocating
         *      PULSEWIDGETDATA for our own stuff.
         *
         *      Each widget must write its desired width into
         *      XCENTERWIDGETVIEW.cx and cy.
         */

        case WM_CREATE:
            WinSetWindowPtr(hwnd, QWL_USER, mp1);
            mrc = PwgtCreate(hwnd, mp1);
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
         *@@ WM_WINDOWPOSCHANGED:
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
            {
                PXCENTERWIDGETVIEW pViewData = (PXCENTERWIDGETVIEW)WinQueryWindowPtr(hwnd, QWL_USER);
                if (!pViewData)
                    _Pmpf((__FUNCTION__ ": msg 0x%lX (0x%lX, 0x%lX) came in before WM_CREATE",
                                    msg, mp1, mp2));
            }
            mrc = ctrDefWidgetProc(hwnd, msg, mp1, mp2);
    } // end switch(msg)

    return (mrc);
}


