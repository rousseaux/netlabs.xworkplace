
/*
 *@@sourcefile w_pulse.c:
 *      XCenter "Pulse" widget implementation.
 *      This is built into the XCenter and not in
 *      a plugin DLL.
 *
 *      This is an example of a more complicated
 *      widget since it now uses multithreading.
 *      A second thread is started on widget
 *      creation to monitor the CPU counters in
 *      the OS/2 kernel. The window proc (on the
 *      XCenter thread) still does the painting.
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
 *      Copyright (C) 2000-2001 Ulrich M�ller.
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
#define INCL_WINMESSAGEMGR
#define INCL_WININPUT
#define INCL_WINSYS
#define INCL_WINTIMER
#define INCL_WINDIALOGS
#define INCL_WINSTATICS
#define INCL_WINBUTTONS
#define INCL_WINENTRYFIELDS

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
#include "helpers\comctl.h"             // common controls (window procs)
#include "helpers\dialog.h"             // dialog helpers
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\gpih.h"               // GPI helper routines
#include "helpers\nls.h"                // National Language Support helpers
#include "helpers\winh.h"               // PM helper routines
#include "helpers\standards.h"          // some standard macros
#include "helpers\stringh.h"            // string helper routines
#include "helpers\threads.h"            // thread helpers
#include "helpers\timer.h"              // replacement PM timers
#include "helpers\xstring.h"            // extended string helpers

// SOM headers which don't crash with prec. header files

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\helppanels.h"          // all XWorkplace help panel IDs

#include "shared\center.h"              // public XCenter interfaces

#pragma hdrstop                     // VAC++ keeps crashing otherwise

/* ******************************************************************
 *
 *   Private widget instance data
 *
 ********************************************************************/

#define XM_NEWDATAAVAILABLE WM_USER

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
 *@@changed V0.9.16 (2002-01-05) [umoeller]: added new fields for SMP support
 */

typedef struct _PULSESETUP
{
    LONG            lcolBackground,
                    lcolGraphIntr,
                    lcolText;

    PLONG           palcolGraph;        // array of graph colors for each CPU
                                        // V0.9.16 (2002-01-05) [umoeller]

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
 *
 *@@changed V0.9.16 (2002-01-05) [umoeller]: added new fields for SMP support
 */

typedef struct _WIDGETPRIVATE
{
    PXCENTERWIDGET pWidget;
            // reverse ptr to general widget data ptr; we need
            // that all the time and don't want to pass it on
            // the stack with each function call

    PULSESETUP      Setup;
            // widget settings that correspond to a setup string

    PXBITMAP        pBitmap;        // bitmap for pulse graph; this contains only
                                    // the "client" (without the 3D frame)

    BOOL            fUpdateGraph;
                                    // if TRUE, PwgtPaint recreates the entire
                                    // bitmap

    THREADINFO      tiCollect;      // collect thread

    HEV             hevExit;        // event semaphore posted on destroy to tell
                                    // collect thread to exit
    HEV             hevExitComplete; // posted by collect thread when it's done

    HMTX            hmtxData;       // mutex for protecting the data

    // All the following data must be protected with the mutex.
    // Rules about the data: main thread allocates it according
    // to the window size, collect thread moves data around,
    // but never reallocates.

    PDOSHPERFSYS    pPerfData;      // performance data (doshPerf* calls)

    ULONG           cProcessors;          // CPU Count V0.9.16 (2002-01-05) [umoeller]
    ULONG           cLoads;
    PLONG           palLoads;       // ptr to an array of LONGs containing previous
                                    // CPU loads
    PLONG           palIntrs;       // ptr to an array of LONGs containing previous
                                    // CPU interrupt loads
                                    // added V0.9.9 (2001-03-14) [umoeller]

    APIRET          arc;            // if != NO_ERROR, an error occured, and
                                    // the error code is displayed instead.

    BOOL            fTooltipShowing;    // TRUE only while tooltip is showing over
                                    // this widget
    CHAR            szTooltipText[100]; // current tooltip text

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

#define INDEX_BACKGROUND        1000
#define INDEX_TEXT              1001
#define INDEX_IRQLOAD           1002

/*
 *@@ QueryDefaultColor:
 *      returns the default color for the given CPU index.
 *
 *      Special indices:
 *
 *      --  INDEX_BACKGROUND: background color.
 *      --  INDEX_TEXT:       text color.
 *      --  INDEX_IRQLOAD:    IRQ load color.
 *
 *@@added V0.9.16 (2002-01-05) [umoeller]
 */

LONG QueryDefaultColor(ULONG ulIndex)
{
    switch (ulIndex)
    {
        case INDEX_BACKGROUND:
            return WinQuerySysColor(HWND_DESKTOP, SYSCLR_DIALOGBACKGROUND, 0);

        case INDEX_TEXT:
            return WinQuerySysColor(HWND_DESKTOP, SYSCLR_WINDOWTEXT, 0);

        case INDEX_IRQLOAD:
            return RGBCOL_RED;

        case 0:
            return RGBCOL_DARKCYAN;

        case 1:
            return RGBCOL_DARKBLUE;

        case 2:
            return RGBCOL_GREEN;
    }

    // any other CPU (3 or higher):
    return RGBCOL_DARKGRAY;
}

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
        FREE(pSetup->pszFont);
        FREE(pSetup->palcolGraph);
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
 *@@changed V0.9.16 (2002-01-05) [umoeller]: added SMP support, added proper default colors
 */

VOID PwgtScanSetup(const char *pcszSetupString,
                   PPULSESETUP pSetup,
                   ULONG cProcessors)               // in: CPU count from WIDGETPRIVATE
{
    PSZ     p;
    ULONG   ul;

    // width
    if (p = ctrScanSetupString(pcszSetupString,
                               "WIDTH"))
    {
        pSetup->cx = atoi(p);
        ctrFreeSetupValue(p);
    }
    else
        pSetup->cx = 130;           // changed from 200 V0.9.16 (2002-01-13) [umoeller]

    // background color
    if (p = ctrScanSetupString(pcszSetupString,
                               "BGNDCOL"))
    {
        pSetup->lcolBackground = ctrParseColorString(p);
        ctrFreeSetupValue(p);
    }
    else
        pSetup->lcolBackground = QueryDefaultColor(INDEX_BACKGROUND);

    // graph color for each CPU:
    // V0.9.16 (2002-01-05) [umoeller]
    if (pSetup->palcolGraph = malloc(sizeof(LONG) * cProcessors))
    {
        for (ul = 0;
             ul < cProcessors;
             ul++)
        {
            LONG lColor;
            CHAR szKeyThis[100];
            sprintf(szKeyThis, "GRPHCOL%d", ul);
            if (p = ctrScanSetupString(pcszSetupString,
                                       szKeyThis))
            {
                lColor = ctrParseColorString(p);
                ctrFreeSetupValue(p);
            }
            else
                lColor = QueryDefaultColor(ul);

            pSetup->palcolGraph[ul] = lColor;
        }
    }

    // graph color: (interrupt load)
    if (p = ctrScanSetupString(pcszSetupString,
                               "GRPHINTRCOL"))
    {
        pSetup->lcolGraphIntr = ctrParseColorString(p);
        ctrFreeSetupValue(p);
    }
    else
        pSetup->lcolGraphIntr = QueryDefaultColor(INDEX_IRQLOAD);


    // text color:
    if (p = ctrScanSetupString(pcszSetupString,
                               "TEXTCOL"))
    {
        pSetup->lcolText = ctrParseColorString(p);
        ctrFreeSetupValue(p);
    }
    else
        pSetup->lcolText = QueryDefaultColor(INDEX_TEXT);

    // font:
    // we set the font presparam, which automatically
    // affects the cached presentation spaces
    if (p = ctrScanSetupString(pcszSetupString,
                               "FONT"))
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
 *
 *@@changed V0.9.16 (2002-01-05) [umoeller]: added SMP support
 *@@changed V0.9.16 (2002-01-05) [umoeller]: now adding string only if not default color
 */

VOID PwgtSaveSetup(PXSTRING pstrSetup,       // out: setup string (is cleared first)
                   PPULSESETUP pSetup,
                   ULONG cProcessors)            // added V0.9.16 (2002-01-05) [umoeller]
{
    CHAR    szTemp[100];
    ULONG   ul;

    xstrInit(pstrSetup, 100);

    sprintf(szTemp, "WIDTH=%d;",
            pSetup->cx);
    xstrcat(pstrSetup, szTemp, 0);

    if (pSetup->lcolBackground != QueryDefaultColor(INDEX_BACKGROUND))
    {
        sprintf(szTemp, "BGNDCOL=%06lX;",
                pSetup->lcolBackground);
        xstrcat(pstrSetup, szTemp, 0);
    }

    // graph colors for each CPU V0.9.16 (2002-01-05) [umoeller]
    if (pSetup->palcolGraph)
        for (ul = 0;
             ul < cProcessors;
             ul++)
        {
            if (pSetup->palcolGraph[ul] != QueryDefaultColor(ul))
            {
                sprintf(szTemp, "GRPHCOL%d=%06lX;",
                        ul,
                        pSetup->palcolGraph[ul]);
                xstrcat(pstrSetup, szTemp, 0);
            }
        }

    if (pSetup->lcolGraphIntr != QueryDefaultColor(INDEX_IRQLOAD))
    {
        sprintf(szTemp, "GRPHINTRCOL=%06lX;",
                pSetup->lcolGraphIntr);
        xstrcat(pstrSetup, szTemp, 0);
    }

    if (pSetup->lcolText != QueryDefaultColor(INDEX_TEXT))
    {
        sprintf(szTemp, "TEXTCOL=%06lX;",
                pSetup->lcolText);
        xstrcat(pstrSetup, szTemp, 0);
    }

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
 *   Helpers
 *
 ********************************************************************/

/*
 *@@ GetProcessorCount:
 *      returns the no. of processors on the system.
 *      I guess we could use DosQuerySysInfo also
 *      but this one should return the value from
 *      doshPerfGet which we should rather use.
 *
 *      Expensive call, so use with caution.
 *
 *@@added V0.9.16 (2002-01-05) [umoeller]
 */

APIRET GetProcessorCount(PULONG pcProcessors)
{
    APIRET arc;
    PDOSHPERFSYS    pPerfData;
    if (!(arc = doshPerfOpen(&pPerfData)))
    {
        // already doshPerfOpen gets the CPU count
        // so there's no need for doshPerfGet
        *pcProcessors = pPerfData->cProcessors;

        doshPerfClose(&pPerfData);
    }

    return (arc);
}

/* ******************************************************************
 *
 *   Widget settings dialog
 *
 ********************************************************************/

static PFNWP G_pfnwpOrigStatic = NULL;

/*
 *@@ ctl_fnwpSubclassedColorRect:
 *      window procedure for subclassed static frames representing
 *      a color.
 *
 *      The control simply paints itself as a rectangle with the
 *      color specified in its PP_BACKGROUNDCOLOR presentation
 *      parameter. If a window text is set for the control, it is
 *      painted with the PP_FOREGROUNDCOLOR color.
 *
 *      If the user drags a color onto the control, it notifies
 *      its owner with the WM_CONTROL message and the EN_CHANGE
 *      notification code, as with any entry field (since the
 *      static control knows no notifications, we use that code
 *      instead).
 *
 *@@added V0.9.16 (2002-01-05) [umoeller]
 */

MRESULT EXPENTRY ctl_fnwpSubclassedColorRect(HWND hwndStatic, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    switch (msg)
    {
        case WM_PAINT:
        {
            LONG    lColor;
            RECTL   rclPaint;
            PSZ     pszText;

            HPS hps = WinBeginPaint(hwndStatic,
                                    NULLHANDLE, // HPS
                                    NULL); // PRECTL
            gpihSwitchToRGB(hps);
            WinQueryWindowRect(hwndStatic,
                               &rclPaint);      // exclusive
            lColor = winhQueryPresColor(hwndStatic,
                                        PP_BACKGROUNDCOLOR,
                                        FALSE,      // no inherit
                                        SYSCLR_DIALOGBACKGROUND);

            // make rect inclusive
            rclPaint.xRight--;
            rclPaint.yTop--;

            // draw interior
            GpiSetColor(hps, lColor);
            gpihBox(hps,
                    DRO_FILL,
                    &rclPaint);

            // draw frame
            GpiSetColor(hps, RGBCOL_BLACK);
            gpihBox(hps,
                    DRO_OUTLINE,
                    &rclPaint);

            if (pszText = winhQueryWindowText(hwndStatic))
            {
                GpiSetColor(hps,
                            winhQueryPresColor(hwndStatic,
                                               PP_FOREGROUNDCOLOR,
                                               FALSE,
                                               -1));
                WinDrawText(hps,
                            strlen(pszText),
                            pszText,
                            &rclPaint,
                            0,
                            0,
                            DT_CENTER | DT_VCENTER | DT_TEXTATTRS);

                free(pszText);
            }

            WinEndPaint(hps);
        }
        break;

        case WM_PRESPARAMCHANGED:
            switch ((ULONG)mp1)
            {
                case PP_BACKGROUNDCOLOR:
                    WinInvalidateRect(hwndStatic,
                                      NULL,
                                      FALSE);
                    // notify owner; since the static control
                    // doesn't send any notifications, we
                    // use EN_CHANGED
                    WinSendMsg(WinQueryWindow(hwndStatic, QW_OWNER),
                               WM_CONTROL,
                               MPFROM2SHORT(WinQueryWindowUShort(hwndStatic, QWS_ID),
                                            EN_CHANGE),
                               (MPARAM)hwndStatic);
                break;
            }
        break;

        default:
            mrc = G_pfnwpOrigStatic(hwndStatic, msg, mp1, mp2);
        break;
    }

    return (mrc);
}

/*
 *@@ SubclassAndSetColor:
 *
 *@@added V0.9.16 (2002-01-05) [umoeller]
 */

VOID SubclassAndSetColor(HWND hwndDlg,
                         ULONG ulID,
                         PCSZ pcszTitle,
                         LONG lColor,
                         LONG lBackColor)
{
    HWND hwnd;
    if (hwnd = WinWindowFromID(hwndDlg, ulID))
    {
        WinSetWindowText(hwnd,
                         (PSZ)pcszTitle);
        winhSetPresColor(hwnd,
                         PP_BACKGROUNDCOLOR,
                         lColor);
        if (ulID == 1000 + INDEX_TEXT)
            winhSetPresColor(hwnd,
                             PP_FOREGROUNDCOLOR,
                             lBackColor);
        G_pfnwpOrigStatic = WinSubclassWindow(hwnd,
                                              ctl_fnwpSubclassedColorRect);
    }
}

/*
 *@@ GetColor:
 *
 *@@added V0.9.16 (2002-01-05) [umoeller]
 */

LONG GetColor(HWND hwndDlg,
              ULONG ulID)
{
    return (winhQueryPresColor(WinWindowFromID(hwndDlg, ulID),
                               PP_BACKGROUNDCOLOR,
                               FALSE,
                               SYSCLR_DIALOGBACKGROUND));
}

static CONTROLDEF
    OKButton = CONTROLDEF_DEFPUSHBUTTON(NULL, DID_OK, 100, 30),
    CancelButton = CONTROLDEF_PUSHBUTTON(NULL, DID_CANCEL, 100, 30),

    ProcessorsGroup = CONTROLDEF_GROUP(NULL, -1),

    IRQLoadColor
                = CONTROLDEF_TEXT(NULL,
                                  1000 + INDEX_IRQLOAD,
                                  100,
                                  40),

    ProcessorColor
                = CONTROLDEF_TEXT(NULL,
                                  1000,
                                  100,
                                  40),

    OthersGroup = CONTROLDEF_GROUP(NULL, -1),

    BackgroundColor
                = CONTROLDEF_TEXT(NULL,
                                  1000 + INDEX_BACKGROUND,
                                  100,
                                  40),
    TextColor
                = CONTROLDEF_TEXT(NULL,
                                  1000 + INDEX_TEXT,
                                  100,
                                  40);

static const DLGHITEM
    dlgPulseFront[] =
    {
        START_TABLE,
            START_ROW(0),
                START_GROUP_TABLE(&ProcessorsGroup),
                    START_ROW(0),
                        CONTROL_DEF(&IRQLoadColor)
    },
    dlgPulsePerProcessor[] =
    {
                    START_ROW(0),
                        CONTROL_DEF(&ProcessorColor),
    },
    dlgPulseTail[] =
    {
                END_TABLE,
                START_GROUP_TABLE(&OthersGroup),
                    START_ROW(0),
                        CONTROL_DEF(&BackgroundColor),
                    START_ROW(0),
                        CONTROL_DEF(&TextColor),
                END_TABLE,
            START_ROW(0),
                CONTROL_DEF(&OKButton),
                CONTROL_DEF(&CancelButton),
        END_TABLE
    };

/*
 *@@ PwgtShowSettingsDlg:
 *      shows the pulse widget settings dialog for
 *      setting up the colors.
 *
 *@@added V0.9.16 (2002-01-05) [umoeller]
 */

VOID EXPENTRY PwgtShowSettingsDlg(PWIDGETSETTINGSDLGDATA pData)
{
    HWND hwndDlg = NULLHANDLE;
    APIRET arc;

    ULONG       cProcessors = 0;
    ULONG       ul;

    if (!GetProcessorCount(&cProcessors))
    {
        PDLGARRAY pArray = NULL;

        ProcessorsGroup.pcszText = cmnGetString(ID_CRSI_PWGT_CPUGRAPHCOLORS);
        OthersGroup.pcszText = cmnGetString(ID_CRSI_PWGT_OTHERCOLORS);

        OKButton.pcszText = cmnGetString(ID_XSSI_DLG_OK);
        CancelButton.pcszText = cmnGetString(ID_XSSI_DLG_CANCEL);

        if (!(arc = dlghCreateArray(   ARRAYITEMCOUNT(dlgPulseFront)
                                     +    cProcessors
                                        * ARRAYITEMCOUNT(dlgPulsePerProcessor)
                                     + ARRAYITEMCOUNT(dlgPulseTail),
                                    &pArray)))
        {
            if (!(arc = dlghAppendToArray(pArray,
                                          dlgPulseFront,
                                          ARRAYITEMCOUNT(dlgPulseFront))))
            {
                for (ul = 0;
                     ul < cProcessors;
                     ul++)
                {
                    if (arc = dlghAppendToArray(pArray,
                                                dlgPulsePerProcessor,
                                                ARRAYITEMCOUNT(dlgPulsePerProcessor)))
                        break;
                }
            }
        }

        if (    (!arc)
             && (!(arc = dlghAppendToArray(pArray,
                                           dlgPulseTail,
                                           ARRAYITEMCOUNT(dlgPulseTail))))
             && (!(arc = dlghCreateDlg(&hwndDlg,
                                       pData->hwndOwner,
                                       FCF_TITLEBAR | FCF_SYSMENU | FCF_DLGBORDER | FCF_NOBYTEALIGN,
                                       WinDefDlgProc,
                                       cmnGetString(ID_CRSI_PWGT_TITLE),
                                       pArray->paDlgItems,
                                       pArray->cDlgItemsNow,
                                       NULL,
                                       cmnQueryDefaultFont())))
           )
        {
            PCSZ pcszCPUXUserColor = cmnGetString(ID_CRSI_PWGT_CPUXUSERCOLOR);

            // go scan the setup string
            PULSESETUP  Setup;
            PwgtScanSetup(pData->pcszSetupString,
                          &Setup,
                          cProcessors);

            // for each color control, set the background color
            // according to the settings
            SubclassAndSetColor(hwndDlg,
                                1000 + INDEX_IRQLOAD,
                                cmnGetString(ID_CRSI_PWGT_IRQLOADCOLOR), // "IRQ load",
                                Setup.lcolGraphIntr,
                                Setup.lcolBackground);

            for (ul = 0;
                 ul < cProcessors;
                 ul++)
            {
                CHAR sz[100];
                sprintf(sz,
                        pcszCPUXUserColor,
                        ul);
                SubclassAndSetColor(hwndDlg,
                                    1000 + ul,
                                    sz,
                                    Setup.palcolGraph[ul],
                                    Setup.lcolBackground);
            }

            SubclassAndSetColor(hwndDlg,
                                1000 + INDEX_BACKGROUND,
                                cmnGetString(ID_CRSI_PWGT_BACKGROUNDCOLOR), // "Background",
                                Setup.lcolBackground,
                                Setup.lcolBackground);
            SubclassAndSetColor(hwndDlg,
                                1000 + INDEX_TEXT,
                                cmnGetString(ID_CRSI_PWGT_TEXTCOLOR), // "Text",
                                Setup.lcolText,
                                Setup.lcolBackground);

            // go!
            winhCenterWindow(hwndDlg);
            if (DID_OK == WinProcessDlg(hwndDlg))
            {
                XSTRING strSetup;

                // get the colors back from the controls
                Setup.lcolGraphIntr = GetColor(hwndDlg,
                                               1000 + INDEX_IRQLOAD);
                for (ul = 0;
                     ul < cProcessors;
                     ul++)
                {
                    Setup.palcolGraph[ul] = GetColor(hwndDlg,
                                                     1000 + ul);
                }

                Setup.lcolBackground = GetColor(hwndDlg,
                                                1000 + INDEX_BACKGROUND);

                Setup.lcolText = GetColor(hwndDlg,
                                          1000 + INDEX_TEXT);

                PwgtSaveSetup(&strSetup,
                              &Setup,
                              cProcessors);
                pData->pctrSetSetupString(pData->hSettings,
                                          strSetup.psz);
                xstrClear(&strSetup);
            }

            WinDestroyWindow(hwndDlg);
        }

        dlghFreeArray(&pArray);
    }
}

/* ******************************************************************
 *
 *   Thread synchronization
 *
 ********************************************************************/

/*
 *@@ LockData:
 *      locks the private data. Note that we only
 *      wait half a second for the mutex to be available
 *      and return FALSE otherwise.
 *
 *@@added V0.9.12 (2001-05-20) [umoeller]
 */

BOOL LockData(PWIDGETPRIVATE pPrivate)
{
    return (!DosRequestMutexSem(pPrivate->hmtxData, 500));
}

/*
 *@@ UnlockData:
 *      unlocks the private data.
 *
 *@@added V0.9.12 (2001-05-20) [umoeller]
 */

VOID UnlockData(PWIDGETPRIVATE pPrivate)
{
    DosReleaseMutexSem(pPrivate->hmtxData);
}

/* ******************************************************************
 *
 *   "Collect" thread
 *
 ********************************************************************/

/*
 *@@ fntCollect:
 *      extra thread to collect the performance data from
 *      the OS/2 kernel.
 *
 *      This thread does NOT have a PM message queue. It
 *      is started from PwgtCreate and will simply collect
 *      the performance data in a loop, sleeping for a
 *      second after each query.
 *
 *      Before 0.9.12, we collected the performance data
 *      on the widget thread (i.e. the XCenter thread).
 *      This worked OK as long as no application hogged
 *      the input queue... in that situation, no performance
 *      data was collected while the SIQ was locked which
 *      lead to a highly incorrect display if PM was busy.
 *
 *      So what we do now is collect the data on this new
 *      thread and only do the display on the PM thread.
 *      The display is still not updated while the SIQ
 *      is busy, but after the SIQ becomes unlocked, it
 *      is then updated with all the data that was collected
 *      here in the meantime.
 *
 *      The optimal solution would be to move all drawing
 *      to this second thread... maybe at a later time.
 *
 *      The thread data is a pointer to WIDGETPRIVATE.
 *
 *      Note: This thread does not allocate the data fields
 *      in WIDGETPRIVATE. It only updates the fields, while
 *      the allocations (and the resizes) are still done
 *      on the main thread. To synchronize this, we have
 *      a mutex in WIDGETPRIVATE (see LockData()).
 *
 *@@added V0.9.12 (2001-05-20) [umoeller]
 */

VOID _Optlink fntCollect(PTHREADINFO ptiMyself)
{
    PWIDGETPRIVATE pPrivate = (PWIDGETPRIVATE)ptiMyself->ulData;
    if (pPrivate)
    {
        BOOL    fLocked = FALSE;
        ULONG   cCurCPU;

        // give this thread a higher-than-regular priority;
        // this way, the "loads" array is always up-to-date,
        // while the display may be delayed if the system
        // is really busy... time-critical is OK since we
        // spent only very little time in here

        DosSetPriority(PRTYS_THREAD,
                       PRTYC_TIMECRITICAL,  // 3, second highest class
                       0,                   // delta, let's not overdo it
                       0);                  // current thread

        TRY_LOUD(excpt1)
        {
            // open the performance counters
            if (!(pPrivate->arc = doshPerfOpen(&pPrivate->pPerfData)))
            {
                // alright:
                // start collecting

                while (TRUE)
                {
                    // LOCK the data while we're moving around here
                    if (fLocked = LockData(pPrivate))
                    {
                        // has main thread allocated data already?
                        if (    (pPrivate->palLoads)
                             && (pPrivate->palIntrs)
                                // get new loads from OS/2 kernel
                             && (!(pPrivate->arc = doshPerfGet(pPrivate->pPerfData)))
                           )
                        {
                            // in the array of loads, move each entry
                            // one to the front; drop the oldest entry
                            memcpy(&pPrivate->palIntrs[0],
                                   &pPrivate->palIntrs[1],
                                   sizeof(LONG) * (pPrivate->cLoads - 1));
                            // and update the last entry with the current value
                            pPrivate->palIntrs[pPrivate->cLoads - 1]
                                = pPrivate->pPerfData->palIntrs[0];

                            // get load for every CPU in the system [bvl]
                            for (cCurCPU=0;
                                 cCurCPU < pPrivate->cProcessors;
                                 cCurCPU++)
                            {
                                // in the array of loads, move each entry
                                // one to the front; drop the oldest entry
                                memcpy(&pPrivate->palLoads[cCurCPU*pPrivate->cLoads],
                                       &pPrivate->palLoads[(cCurCPU*pPrivate->cLoads)+1],
                                       sizeof(LONG) * (pPrivate->cLoads - 1));

                                // and update the last entry with the current value
                                pPrivate->palLoads[(pPrivate->cLoads + (pPrivate->cLoads * cCurCPU)) - 1]
                                    = pPrivate->pPerfData->palLoads[cCurCPU];
                            } // for cCurCPU
                        }

                        UnlockData(pPrivate);
                        fLocked = FALSE;

                    } // if (fLocked)

                    // have main thread update the display
                    // (if SIQ is hogged, several XM_NEWDATAAVAILABLEs
                    // will pile up)
                    if (!ptiMyself->fExit)
                    {
                        pPrivate->fUpdateGraph = TRUE;
                        WinPostMsg(pPrivate->pWidget->hwndWidget,
                                   XM_NEWDATAAVAILABLE,
                                   (MPARAM)1,
                                   0);
                    }

                    // some security checks: do not sleep
                    // if we have problems
                    if (    (!pPrivate->arc)
                         && (!ptiMyself->fExit)         // or should exit
                         && (!pPrivate->fCrashed)
                       )
                    {
                        // OK, we need to sleep for a second before
                        // we get the next chunk of performance
                        // data. However, we should not use DosSleep
                        // because we MUST exit quickly when the
                        // widget gets destroyed...
                        // the trick is to wait on an event semaphore:
                        // with a timeout of one second;
                        // this will sleep for a second, but exit
                        // earlier when PwgtDestroy() posts the semaphore
                        if (DosWaitEventSem(pPrivate->hevExit,
                                            1000)       // timeout == 1 second
                                != ERROR_TIMEOUT)
                            // whoa, semaphore was posted:
                            // GET OUTTA HERE
                            break;
                        // else timeout: continue, we then slept for a second
                    }
                    else
                        // stop right here
                        break;

                } // end while (TRUE);
            } // if (!(pPrivate->arc = doshPerfOpen(&pPrivate->pPerfData)))
        }
        CATCH(excpt1)
        {
            pPrivate->fCrashed = TRUE;
        }
        END_CATCH();

        if (fLocked)
            UnlockData(pPrivate);

        // PwgtDestroy is blocking on hevExitComplete,
        // so tell main thread we're done
        DosPostEventSem(pPrivate->hevExitComplete);
    }
}

/* ******************************************************************
 *
 *   PM window class implementation
 *
 ********************************************************************/

/*
 *@@ PwgtCreate:
 *      implementation for WM_CREATE.
 *
 *@@changed V0.9.12 (2001-05-20) [umoeller]: now using second thread
 */

MRESULT PwgtCreate(HWND hwnd, MPARAM mp1)
{
    MRESULT mrc = 0;        // continue window creation

    PXCENTERWIDGET pWidget = (PXCENTERWIDGET)mp1;
    PWIDGETPRIVATE pPrivate = malloc(sizeof(WIDGETPRIVATE));
    memset(pPrivate, 0, sizeof(WIDGETPRIVATE));
    // link the two together
    pWidget->pUser = pPrivate;
    pPrivate->pWidget = pWidget;

    // get CPU count for array rceation [bvl]
    pPrivate->arc = GetProcessorCount(&pPrivate->cProcessors);

    PwgtScanSetup(pWidget->pcszSetupString,
                  &pPrivate->Setup,
                  pPrivate->cProcessors);

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

    // create mutex for data protection
    DosCreateMutexSem(NULL,
                      &pPrivate->hmtxData,
                      0,
                      FALSE);           // no request

    // create event sems for collect thread
    DosCreateEventSem(NULL,
                      &pPrivate->hevExit,
                      0,
                      0);               // not posted
    DosCreateEventSem(NULL,
                      &pPrivate->hevExitComplete,
                      0,
                      0);               // not posted

    // start the collect thread V0.9.12 (2001-05-20) [umoeller]
    thrCreate(&pPrivate->tiCollect,
              fntCollect,
              NULL,
              "PulseWidgetCollect",
              THRF_WAIT,
              (ULONG)pPrivate);        // thread data

    pPrivate->fUpdateGraph = TRUE;

    pPrivate->szTooltipText[0] = '\0';

    return (mrc);
}

/*
 *@@ PwgtControl:
 *      implementation for WM_CONTROL.
 *
 *@@added V0.9.7 (2000-12-14) [umoeller]
 *@@changed V0.9.13 (2001-06-21) [umoeller]: added tooltip
 */

BOOL PwgtControl(HWND hwnd, MPARAM mp1, MPARAM mp2)
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
                        pszl->cy = 10;

                        brc = TRUE;
                    }
                    break;

                    /*
                     * XN_SETUPCHANGED:
                     *      XCenter has a new setup string for
                     *      us in mp2.
                     */

                    case XN_SETUPCHANGED:
                    {
                        const char *pcszNewSetupString = (const char*)mp2;

                        // reinitialize the setup data
                        PwgtClearSetup(&pPrivate->Setup);
                        PwgtScanSetup(pcszNewSetupString,
                                      &pPrivate->Setup,
                                      pPrivate->cProcessors);

                        WinInvalidateRect(pWidget->hwndWidget, NULL, FALSE);
                    }
                    break;
                }
            }
            break;

            case ID_XCENTER_TOOLTIP:
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
            break;
        }
    } // end if (pWidget)

    return (brc);
}

/*
 *@@ PwgtUpdateGraph:
 *      updates the graph bitmap. This does not paint
 *      on the screen yet.
 *
 *      Preconditions:
 *
 *      --  pPrivate->pBitmap must be ready.
 *
 *      --  Caller must hold the data mutex.
 *
 *@@changed V0.9.9 (2001-03-14) [umoeller]: added interrupts graph
 *@@changed V0.9.13 (2001-06-21) [umoeller]: added tooltip refresh
 *@@changed V0.9.14 (2001-07-12) [umoeller]: fixed sporadic crash on some systems
 *@@changed V0.9.16 (2002-01-05) [umoeller]: added multiple CPUs support
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

    if (!pPrivate->pBitmap)
        pPrivate->pBitmap = gpihCreateXBitmap(pWidget->habWidget,
                                              rclBmp.xRight,
                                              rclBmp.yTop);
    if (pPrivate->pBitmap)
    {
        HPS     hpsMem = pPrivate->pBitmap->hpsMem;
        PCOUNTRYSETTINGS pCountrySettings = (PCOUNTRYSETTINGS)pWidget->pGlobals->pCountrySettings;

        // fill the bitmap rectangle
        GpiSetColor(hpsMem,
                    pPrivate->Setup.lcolBackground);
        gpihBox(hpsMem,
                DRO_FILL,
                &rclBmp);

        if (    pPrivate->pPerfData
             && pPrivate->cLoads
           )    // V0.9.14 (2001-07-12) [umoeller]
        {
            PLONG    palLoad1000 = _alloca(sizeof(LONG) * pPrivate->cProcessors);
            LONG     lIRQ1000 = pPrivate->pPerfData->palIntrs[0];
            ULONG    cCurCPU;
            PSZ      pszTooltipLoc;

            for (cCurCPU = 0;
                 cCurCPU < pPrivate->cProcessors;
                 cCurCPU++)
            {
                // go thru all values in the "Loads" LONG array
                for (ptl.x = 0;
                     (    (ptl.x < pPrivate->cLoads)
                       && (ptl.x < rclBmp.xRight)
                     );
                     ptl.x++)
                {
                    ptl.y = 0;

                    // interrupt load on bottom
                    // IRQ processing only takes place on CPU(0) so we only need
                    // that information ONCE [bvl]
                    if (pPrivate->palIntrs)
                    {
                        GpiSetColor(hpsMem,
                                    pPrivate->Setup.lcolGraphIntr);
                        // go thru all values in the "Interrupt Loads" LONG array
                        // Note: number of "loads" entries and "intrs" entries is the same

                        // bvl: this ensures line mode.. if the this is the first item in the load
                        //      start at the bottom, otherwise the previous point
                        if (ptl.x)
                            ptl.y += rclBmp.yTop * pPrivate->palIntrs[ptl.x-1] / 1000;

                        GpiMove(hpsMem, &ptl);
                        ptl.y += rclBmp.yTop * pPrivate->palIntrs[ptl.x] / 1000;
                        GpiLine(hpsMem, &ptl);
                    }


                    // LOAD Information is available for every CPU so itterate over it
                    palLoad1000[cCurCPU] = pPrivate->pPerfData->palLoads[cCurCPU];
                    // scan the CPU loads
                    if (pPrivate->palLoads)
                    {
                        if (pPrivate->Setup.palcolGraph)
                            GpiSetColor(hpsMem,
                                        pPrivate->Setup.palcolGraph[cCurCPU]);

                        // bvl: this ensures line mode.. if the this is the first item in the load
                        //      start at the bottom, otherwise the previous point
                        if (ptl.x != 0)
                            ptl.y += rclBmp.yTop * pPrivate->palLoads[(pPrivate->cLoads*cCurCPU) + ptl.x-1] / 1000;

                        GpiMove(hpsMem, &ptl);
                        ptl.y = rclBmp.yTop * pPrivate->palLoads[(pPrivate->cLoads*cCurCPU) + ptl.x] / 1000;
                        GpiLine(hpsMem, &ptl);
                    }

                } // end if (fLocked)
            } // for cCurCPU

            // update the tooltip text V0.9.13 (2001-06-21) [umoeller]

            pszTooltipLoc = pPrivate->szTooltipText;
            pszTooltipLoc += sprintf(pPrivate->szTooltipText,
                                     cmnGetString(ID_CRSI_PWGT_TOOLTIP1),
                                     // "CPU count: %d"            // bvl: show CPU count
                                     // "\nCPU 0 IRQ: %lu%c%lu%c",
                                     pPrivate->cProcessors,
                                     lIRQ1000 / 10,          // only CPU [0] does IRQ management
                                     pCountrySettings->cDecimal,
                                     lIRQ1000 % 10,
                                     '%'
                                     );
            for (cCurCPU = 0;
                 cCurCPU < pPrivate->cProcessors;
                 cCurCPU++)
            {
                pszTooltipLoc += sprintf(pszTooltipLoc, // @@todo localize
                                         cmnGetString(ID_CRSI_PWGT_TOOLTIP2),
                                         // "\nCPU %d User: %lu%c%lu%c",
                                         cCurCPU,
                                         palLoad1000[cCurCPU] / 10,
                                         pCountrySettings->cDecimal,
                                         palLoad1000[cCurCPU] % 10,
                                         '%');
            } // for cCurCPU

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
 *@@changed V0.9.12 (2001-05-20) [umoeller]: added mutex
 *@@changed V0.9.16 (2002-01-05) [umoeller]: added multiple CPUs support
 */

VOID PwgtPaint2(HWND hwnd,
                HPS hps,
                PWIDGETPRIVATE pPrivate,
                BOOL fDrawFrame)     // in: if TRUE, everything is painted
{
    BOOL fLocked = FALSE;

    TRY_LOUD(excpt1)
    {
        PXCENTERWIDGET pWidget = pPrivate->pWidget;
        RECTL       rclWin;
        ULONG       ulBorder = 1;
        CHAR        szPaint[256] = ""; // slightly larger to support SMP [bvl]
        ULONG       ulPaintLen = 0;

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

        if (!pPrivate->arc)
        {
            // performance counters are working:
            PCOUNTRYSETTINGS pCountrySettings = (PCOUNTRYSETTINGS)pWidget->pGlobals->pCountrySettings;
            POINTL      ptlBmpDest;
            PULONG      paulLoad1000;
            ULONG       cCurCPU;
            PSZ         pszPaintLoc;

            // allocate array for strings
            paulLoad1000 = _alloca(sizeof(ULONG) * pPrivate->cProcessors);

            // lock out the collect thread, we're reading the loads array
            if (fLocked = LockData(pPrivate))
            {
                if (pPrivate->fUpdateGraph)
                    // graph bitmap needs to be updated:
                    PwgtUpdateGraph(hwnd, pPrivate);

                // in the string, display the total load
                // (busy plus interrupt) V0.9.9 (2001-03-14) [umoeller]
                // use for loop to process all CPU's [bvl]
                for (cCurCPU = 0;
                     cCurCPU < pPrivate->cProcessors;
                     cCurCPU++)
                {
                    if (pPrivate->palLoads)
                        paulLoad1000[cCurCPU] = pPrivate->pPerfData->palLoads[cCurCPU];
                    if (pPrivate->palIntrs)
                        paulLoad1000[cCurCPU] += pPrivate->pPerfData->palIntrs[cCurCPU];
                } // for cCurCPU

                // everything below is safe, so unlock
                UnlockData(pPrivate);
                fLocked = FALSE;

                ptlBmpDest.x = rclWin.xLeft + ulBorder;
                ptlBmpDest.y = rclWin.yBottom + ulBorder;
                // now paint graph from bitmap
                WinDrawBitmap(hps,
                              pPrivate->pBitmap->hbm,
                              NULL,     // entire bitmap
                              &ptlBmpDest,
                              0, 0,
                              DBM_NORMAL);

                pszPaintLoc = szPaint;
                for (cCurCPU = 0;
                     cCurCPU < pPrivate->cProcessors;
                     cCurCPU++)
                {
                    pszPaintLoc += sprintf(pszPaintLoc,
                                           "%lu%c%lu%c / ",
                                           paulLoad1000[cCurCPU] / 10,
                                           pCountrySettings->cDecimal,
                                           paulLoad1000[cCurCPU] % 10,
                                           '%');
                } // for cCurCPU

                // delete trailing / if it was the last CPU
                if (ulPaintLen = pszPaintLoc - szPaint)
                {
                    szPaint[ulPaintLen - 3] = '\0';
                    ulPaintLen -= 3;
                }
            }
        }

        if (pPrivate->arc)
        {
            // performance counters are not working:
            // display error message
            rclWin.xLeft++;     // was made inclusive above
            rclWin.yBottom++;
            WinFillRect(hps, &rclWin, pPrivate->Setup.lcolBackground);
            ulPaintLen = sprintf(szPaint, "E %lu", pPrivate->arc);
        }

        if (ulPaintLen)
        {
            // GpiSetColor was missing V0.9.16 (2002-01-05) [umoeller]
            GpiSetColor(hps,
                        pPrivate->Setup.lcolText);
            WinDrawText(hps,
                        ulPaintLen,
                        szPaint,
                        &rclWin,
                        0,
                        0,
                        DT_CENTER | DT_VCENTER | DT_TEXTATTRS);
        }
    }
    CATCH(excpt1)
    {
        pPrivate->fCrashed = TRUE;
    } END_CATCH();

    if (fLocked)
        UnlockData(pPrivate);
}

/*
 *@@ PwgtPaint:
 *      implementation for WM_PAINT.
 */

VOID PwgtPaint(HWND hwnd)
{
    PXCENTERWIDGET pWidget;
    PWIDGETPRIVATE pPrivate;

    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (PWIDGETPRIVATE)pWidget->pUser)
       )
    {
        HPS hps = WinBeginPaint(hwnd, NULLHANDLE, NULL);
        PwgtPaint2(hwnd,
                   hps,
                   pPrivate,
                   TRUE);        // draw frame
        WinEndPaint(hps);
    } // end if (pWidget && pPrivate)
}

/*
 *@@ PwgtNewDataAvailable:
 *      updates the CPU loads array, updates the graph bitmap
 *      and invalidates the window.
 *
 *@@changed V0.9.9 (2001-03-14) [umoeller]: added interrupts graph
 *@@changed V0.9.12 (2001-05-20) [umoeller]: added mutex
 *@@changed V0.9.16 (2002-01-05) [umoeller]: added multiple CPUs support
 */

VOID PwgtNewDataAvailable(HWND hwnd)
{
    PXCENTERWIDGET pWidget;
    PWIDGETPRIVATE pPrivate;

    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (PWIDGETPRIVATE)pWidget->pUser)
       )
    {
        BOOL fLocked = FALSE;

        TRY_LOUD(excpt1)
        {
            if (    (!pPrivate->fCrashed)
                 && (pPrivate->arc == NO_ERROR)
               )
            {
                RECTL rclClient;
                WinQueryWindowRect(hwnd, &rclClient);

                // lock out the collect thread
                if (fLocked = LockData(pPrivate))
                {
                    if (rclClient.xRight > 3)
                    {
                        HPS hps;
                        ULONG ulGraphCX = rclClient.xRight - 2;    // minus border

                        // memory we need: one array of ULONGs for
                        // each CPU V0.9.16 (2002-01-05) [umoeller]
                        ULONG cb =   sizeof(LONG)
                                   * ulGraphCX   // pPrivate->cLoads
                                   * pPrivate->cProcessors;

                        if (pPrivate->palLoads == NULL)
                        {
                            // create array of loads
                            pPrivate->cLoads = ulGraphCX;
                            pPrivate->palLoads = (PLONG)malloc(cb);
                            memset(pPrivate->palLoads, 0, cb);
                        }

                        if (pPrivate->palIntrs == NULL)
                        {
                            // create array of interrupt loads
                            pPrivate->cLoads = ulGraphCX;
                            pPrivate->palIntrs = (PLONG)malloc(cb);
                            memset(pPrivate->palIntrs, 0, cb);
                        }

                        UnlockData(pPrivate);
                        fLocked = FALSE;

                        hps = WinGetPS(hwnd);
                        PwgtPaint2(hwnd,
                                   hps,
                                   pPrivate,
                                   FALSE);        // no draw frame
                        WinReleasePS(hps);

                    } // end if (rclClient.xRight)
                }
            }
        }
        CATCH(excpt1)
        {
            pPrivate->fCrashed = TRUE;
        } END_CATCH();

        if (fLocked)
            UnlockData(pPrivate);

    } // end if (pWidget && pPrivate);
}

/*
 *@@ PwgtWindowPosChanged:
 *      implementation for WM_WINDOWPOSCHANGED.
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
 *@@changed V0.9.9 (2001-03-14) [umoeller]: added interrupts graph
 *@@changed V0.9.13 (2001-06-21) [umoeller]: changed XCM_SAVESETUP call for tray support
 */

VOID PwgtWindowPosChanged(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    PXCENTERWIDGET pWidget;
    PWIDGETPRIVATE pPrivate;

    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (PWIDGETPRIVATE)pWidget->pUser)
       )
    {
        BOOL fLocked = FALSE;

        TRY_LOUD(excpt1)
        {
            PSWP pswpNew = (PSWP)mp1,
                 pswpOld = pswpNew + 1;
            if (pswpNew->fl & SWP_SIZE)
            {
                // window was resized:

                // destroy the bitmap because we need a new one
                if (pPrivate->pBitmap)
                    gpihDestroyXBitmap(&pPrivate->pBitmap);

                if (pswpNew->cx != pswpOld->cx)
                {
                    // width changed:
                    XSTRING strSetup;

                    // update load arrays

                    // lock out the collect thread
                    if (fLocked = LockData(pPrivate))
                    {
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
                                // ULONG ul = 0;
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
                                    // ULONG ul = 0;
                                    memcpy(palNewIntrs,
                                           &pPrivate->palIntrs[pPrivate->cLoads - ulNewClientCX],
                                           ulNewClientCX * sizeof(LONG));
                                }

                                free(pPrivate->palIntrs);
                                pPrivate->palIntrs = palNewIntrs;
                            }

                            pPrivate->cLoads = ulNewClientCX;

                        } // end if (pPrivate->palLoads)

                        UnlockData(pPrivate);
                        fLocked = FALSE;
                    } // end if fLocked


                    pPrivate->Setup.cx = pswpNew->cx;
                    PwgtSaveSetup(&strSetup,
                                  &pPrivate->Setup,
                                  pPrivate->cProcessors);
                    if (strSetup.ulLength)
                        // changed V0.9.13 (2001-06-21) [umoeller]:
                        // post it to parent instead of fixed XCenter client
                        // to make this trayable
                        WinSendMsg(WinQueryWindow(hwnd, QW_PARENT), // pPrivate->pWidget->pGlobals->hwndClient,
                                   XCM_SAVESETUP,
                                   (MPARAM)hwnd,
                                   (MPARAM)strSetup.psz);
                    xstrClear(&strSetup);
                } // end if (pswpNew->cx != pswpOld->cx)

                // force recreation of bitmap
                pPrivate->fUpdateGraph = TRUE;
                WinInvalidateRect(hwnd, NULL, FALSE);
            } // end if (pswpNew->fl & SWP_SIZE)
        }
        CATCH(excpt1)
        {
            pPrivate->fCrashed = TRUE;
        } END_CATCH();

        if (fLocked)
            UnlockData(pPrivate);
    } // end if (pWidget && pPrivate)
}

/*
 *@@ PwgtPresParamChanged:
 *      implementation for WM_PRESPARAMCHANGED.
 *
 *@@changed V0.9.13 (2001-06-21) [umoeller]: changed XCM_SAVESETUP call for tray support
 */

VOID PwgtPresParamChanged(HWND hwnd,
                          ULONG ulAttrChanged)
{
    PXCENTERWIDGET pWidget;
    PWIDGETPRIVATE pPrivate;

    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (PWIDGETPRIVATE)pWidget->pUser)
       )
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
                if (pPrivate->Setup.palcolGraph)
                {
                    PLONG pl = &pPrivate->Setup.palcolGraph[0];
                    // allow the user to change cpu 0's color
                    // via drag'n'drop
                    *pl = winhQueryPresColor(hwnd,
                                             PP_FOREGROUNDCOLOR,
                                             FALSE,
                                             -1);
                    if (*pl == -1)
                        *pl = QueryDefaultColor(0);
                }
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
            }
            break;

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
                          &pPrivate->Setup,
                          pPrivate->cProcessors);
            if (strSetup.ulLength)
                // changed V0.9.13 (2001-06-21) [umoeller]:
                // post it to parent instead of fixed XCenter client
                // to make this trayable
                WinSendMsg(WinQueryWindow(hwnd, QW_PARENT), // pPrivate->pWidget->pGlobals->hwndClient,
                           XCM_SAVESETUP,
                           (MPARAM)hwnd,
                           (MPARAM)strSetup.psz);
            xstrClear(&strSetup);
        }
    } // end if (pWidget && pPrivate)
}

/*
 *@@ PwgtButton1DblClick:
 *      open the OS/2 Kernel object on double-click.
 *
 *@@added V0.9.16 (2001-09-29) [umoeller]
 */

VOID PwgtButton1DblClick(HWND hwnd)
{
    PXCENTERWIDGET pWidget;
    PWIDGETPRIVATE pPrivate;

    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (PWIDGETPRIVATE)pWidget->pUser)
       )
    {
        HOBJECT hobj;
        if (hobj = WinQueryObject("<XWP_KERNEL>"))
        {
            WinOpenObject(hobj,
                          2, // OPEN_SETTINGS,
                          TRUE);
        }
    }
}

/*
 *@@ PwgtDestroy:
 *      implementation for WM_DESTROY.
 *
 *@@changed V0.9.9 (2001-02-06) [umoeller]: fixed call to doshPerfClose
 *@@changed V0.9.14 (2001-08-01) [umoeller]: fixed three memory leaks
 */

VOID PwgtDestroy(HWND hwnd)
{
    PXCENTERWIDGET pWidget;
    PWIDGETPRIVATE pPrivate;

    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (PWIDGETPRIVATE)pWidget->pUser)
       )
    {
        // FIRST thing to do is stop the collect thread
        // a) set fExit flag so that collect thread won't
        //    collect any more
        pPrivate->tiCollect.fExit = TRUE;
        // b) post exit event, in case the collect thread
        //    is currently sleeping
        DosPostEventSem(pPrivate->hevExit);
        // c) now wait for collect thread to post "exit done"
        WinWaitEventSem(pPrivate->hevExitComplete, 2000);

        if (pPrivate->pBitmap)
            gpihDestroyXBitmap(&pPrivate->pBitmap);

        DosCloseMutexSem(pPrivate->hmtxData);
        DosCloseEventSem(pPrivate->hevExit);
        DosCloseEventSem(pPrivate->hevExitComplete);

        if (pPrivate->pPerfData)
            doshPerfClose(&pPrivate->pPerfData);

        if (pPrivate->palLoads)
            free(pPrivate->palLoads);       // V0.9.14 (2001-08-01) [umoeller]
        if (pPrivate->palIntrs)
            free(pPrivate->palIntrs);       // V0.9.14 (2001-08-01) [umoeller]

        PwgtClearSetup(&pPrivate->Setup);   // V0.9.14 (2001-08-01) [umoeller]

        // do not destroy pPrivate->hdcWin, it is
        // destroyed automatically

        free(pPrivate);
    } // end if (pWidget && pPrivate);
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
 *
 *@@changed V0.9.16 (2001-09-29) [umoeller]: added OS/2 Kernel open on double-click
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
         * XM_NEWDATAAVAILABLE:
         *      collect thread has new data --> repaint.
         *
         */

        case XM_NEWDATAAVAILABLE:
            PwgtNewDataAvailable(hwnd);
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
         *@@ WM_BUTTON1DBLCLK:
         *      on double-click on clock, open
         *      system clock settings.
         */

        case WM_BUTTON1DBLCLK:
            PwgtButton1DblClick(hwnd);
            mrc = (MPARAM)TRUE;     // message processed
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


