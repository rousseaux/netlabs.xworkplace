
/*
 *@@sourcefile xwHealth.c:
 *      widget for interfacing STHEALTH.DLL.
 *
 *      This is all new with V0.9.9.
 *
 *@@added V0.9.9 (2001-02-01) [umoeller]
 *@@header "shared\center.h"
 */

/*
 *      Copyright (C) 2001 Stefan Milcke.
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
#define INCL_GPIREGIONS
#define NO_STHEALTH_PRAGMA_LIBRARY
#include "StHealth.h"
#include <os2.h>

// C library headers
#include <stdio.h>
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\comctl.h"             // common controls (window procs)
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\gpih.h"               // GPI helper routines
#include "helpers\prfh.h"               // INI file helper routines;
                                        // this include is required for some
                                        // of the structures in shared\center.h
#include "helpers\stringh.h"            // string helper routines
#include "helpers\timer.h"              // replacement PM timers
#include "helpers\winh.h"               // PM helper routines
#include "helpers\xstring.h"            // extended string helpers

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\center.h"              // public XCenter interfaces
#include "shared\common.h"              // the majestic XWorkplace include file

#pragma hdrstop                     // VAC++ keeps crashing otherwise

/* ******************************************************************
 *
 *   Private definitions
 *
 ********************************************************************/

#define MWGT_HEALTHMONITOR       1

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

#define WNDCLASS_WIDGET_XWHEALTH    "XWPHealthMonitorWidget"

static XCENTERWIDGETCLASS G_WidgetClasses[] =
{
    {
        WNDCLASS_WIDGET_XWHEALTH,
        MWGT_HEALTHMONITOR,
        "Health Monitor",
        "Health Monitor",
        WGTF_UNIQUEPERXCENTER,
        NULL
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
PCMNQUERYDEFAULTFONT pcmnQueryDefaultFont = NULL;
PCMNQUERYHELPLIBRARY pcmnQueryHelpLibrary = NULL;

PCTRFREESETUPVALUE pctrFreeSetupValue = NULL;
PCTRPARSECOLORSTRING pctrParseColorString = NULL;
PCTRSCANSETUPSTRING pctrScanSetupString = NULL;

PGPIHDRAW3DFRAME pgpihDraw3DFrame = NULL;
PGPIHSWITCHTORGB pgpihSwitchToRGB = NULL;

PSTRHDATETIME pstrhDateTime = NULL;
PSTRHTHOUSANDSULONG pstrhThousandsULong = NULL;

PTMRSTARTTIMER ptmrStartTimer = NULL;
PTMRSTOPTIMER ptmrStopTimer = NULL;

PWINHFREE pwinhFree = NULL;
PWINHQUERYPRESCOLOR pwinhQueryPresColor = NULL;
PWINHQUERYWINDOWFONT pwinhQueryWindowFont = NULL;
PWINHSETWINDOWFONT pwinhSetWindowFont = NULL;

PXSTRCAT pxstrcat = NULL;
PXSTRCLEAR pxstrClear = NULL;
PXSTRINIT pxstrInit = NULL;

RESOLVEFUNCTION G_aImports[] =
{
    "cmnQueryDefaultFont", (PFN*)&pcmnQueryDefaultFont,
    "cmnQueryHelpLibrary", (PFN*)&pcmnQueryHelpLibrary,
    "ctrFreeSetupValue", (PFN*)&pctrFreeSetupValue,
    "ctrParseColorString", (PFN*)&pctrParseColorString,
    "ctrScanSetupString", (PFN*)&pctrScanSetupString,
    "gpihDraw3DFrame", (PFN*)&pgpihDraw3DFrame,
    "gpihSwitchToRGB", (PFN*)&pgpihSwitchToRGB,
    "strhDateTime", (PFN*)&pstrhDateTime,
    "strhThousandsULong", (PFN*)&pstrhThousandsULong,
    "tmrStartTimer", (PFN*)&ptmrStartTimer,
    "tmrStopTimer", (PFN*)&ptmrStopTimer,
    "winhFree", (PFN*)&pwinhFree,
    "winhQueryPresColor", (PFN*)&pwinhQueryPresColor,
    "winhQueryWindowFont", (PFN*)&pwinhQueryWindowFont,
    "winhSetWindowFont", (PFN*)&pwinhSetWindowFont,
    "xstrcat", (PFN*)&pxstrcat,
    "xstrClear", (PFN*)&pxstrClear,
    "xstrInit", (PFN*)&pxstrInit
};

BOOL(*_System sthRegisterDaemon) (BOOL) = NULL;
BOOL(*_System sthUnregisterDaemon) (void) = NULL;
BOOL(*_System sthDetectChip) (void) = NULL;
long (*_System sthFan) (int, BOOL) = NULL;
double (*_System sthTemp) (int, BOOL) = NULL;
double (*_System sthVoltage) (int, BOOL) = NULL;
int (*_System sthFilterEvents) (PULONG, ULONG, PULONG, ULONG, PVOID) = NULL;

RESOLVEFUNCTION G_StHealthImports[] =
{
    "StHealthRegisterDaemon", (PFN*)&sthRegisterDaemon,
    "StHealthUnregisterDaemon", (PFN*)&sthUnregisterDaemon,
    "StHealthDetectChip", (PFN*)&sthDetectChip,
    "StHealthFan", (PFN*)&sthFan,
    "StHealthTemp", (PFN*)&sthTemp,
    "StHealthVoltage", (PFN*)&sthVoltage,
    "StHealthFilterEvents", (PFN*)&sthFilterEvents
};

HMODULE hmStHealth = 0;

/*
 *@@loadStHealth:
 *      load StHealth library and query function pointers.
 *
 *@@added V0.9.9 (2001-02-06) [smilcke]
 */

ULONG loadStHealth(void)
{
    ULONG rc = 0;

    if (!hmStHealth)
    {
        char failMod[500];
        ULONG ul = 0;

        rc = DosLoadModule(failMod, 500, "StHealth", &hmStHealth);
        if (rc == NO_ERROR)
            for (ul = 0;
                 ul < sizeof(G_StHealthImports) / sizeof(G_StHealthImports[0]);
                 ul++)
            {
                if (DosQueryProcAddr(hmStHealth, 0, (PSZ) G_StHealthImports[ul].pcszFunctionName
                            ,G_StHealthImports[ul].ppFuncAddress) != NO_ERROR)
                {
                    rc = 1;
                }
            }
    }

    if (rc != NO_ERROR)
    {
        if(hmStHealth)
        {
            DosFreeModule(hmStHealth);
            hmStHealth=0;
        }
    }
    return rc;
}

/*
 *@@unloadStHealth:
 *      unload StHealth library.
 *
 *@@added V0.9.9 (2001-02-06) [smilcke]
 */

ULONG unloadStHealth(void)
{
    ULONG rc = 0;
    if(hmStHealth)
    {
        rc = DosFreeModule(hmStHealth);
        rc = 0;
    }
    return rc;
}

/* ******************************************************************
 *
 *   Private widget instance data
 *
 ********************************************************************/

/*
 *@@ MONITORSETUP:
 *      instance data to which setup strings correspond.
 *      This is also a member of MONITORPRIVATE.
 *
 *      Putting these settings into a separate structure
 *      is no requirement, but comes in handy if you
 *      want to use the same setup string routines on
 *      both the open widget window and a settings dialog.
 */

typedef struct _MONITORSETUP
{
    LONG        lcolBackground,         // background color
                lcolForeground;         // foreground color (for text)

    PSZ         pszFont;
            // if != NULL, non-default font (in "8.Helv" format);
            // this has been allocated using local malloc()!
    PSZ         pszViewString;
} MONITORSETUP, *PMONITORSETUP;

/*
 *@@ MONITORPRIVATE:
 *      more window data for the various monitor widgets.
 *
 *      An instance of this is created on WM_CREATE in
 *      fnwpMonitorWidgets and stored in XCENTERWIDGET.pUser.
 */

typedef struct _MONITORPRIVATE
{
    PXCENTERWIDGET pWidget;
            // reverse ptr to general widget data ptr; we need
            // that all the time and don't want to pass it on
            // the stack with each function call

    ULONG           ulType;
                // this is copied from the widget class on WM_CREATE

    ULONG           cxCurrent,
                    cyCurrent;

    MONITORSETUP    Setup;
            // widget settings that correspond to a setup string

    ULONG           ulTimerID;              // if != NULLHANDLE, update timer is running

} MONITORPRIVATE, *PMONITORPRIVATE;

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
 *@@ MwgtFreeSetup:
 *      cleans up the data in the specified setup
 *      structure, but does not free the structure
 *      itself.
 */

VOID MwgtFreeSetup(PMONITORSETUP pSetup)
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
 *@@ MwgtScanSetup:
 *      scans the given setup string and translates
 *      its data into the specified binary setup
 *      structure.
 *
 *      NOTE: It is assumed that pSetup is zeroed
 *      out. We do not clean up previous data here.
 *
 *@@changed V0.9.9 (2001-02-06) [smilcke]: Added setup strings for health monitoring
 */

VOID MwgtScanSetup(const char *pcszSetupString,
                   PMONITORSETUP pSetup)
{
    PSZ p;
    // background color
    p = pctrScanSetupString(pcszSetupString,
                            "BGNDCOL");
    if (p)
    {
        pSetup->lcolBackground = pctrParseColorString(p);
        pctrFreeSetupValue(p);
    }
    else
        // default color:
        pSetup->lcolBackground = WinQuerySysColor(HWND_DESKTOP, SYSCLR_DIALOGBACKGROUND, 0);

    // text color:
    p = pctrScanSetupString(pcszSetupString,
                            "TEXTCOL");
    if (p)
    {
        pSetup->lcolForeground = pctrParseColorString(p);
        pctrFreeSetupValue(p);
    }
    else
        pSetup->lcolForeground = WinQuerySysColor(HWND_DESKTOP, SYSCLR_WINDOWSTATICTEXT, 0);

    // font:
    // we set the font presparam, which automatically
    // affects the cached presentation spaces
    p = pctrScanSetupString(pcszSetupString,
                            "FONT");
    if (p)
    {
        pSetup->pszFont = strdup(p);
        pctrFreeSetupValue(p);
    }
    // else: leave this field null

    // Health view string
    p = pctrScanSetupString(pcszSetupString,
                            "HEALTHVSTR");
    if (p)
    {
        pSetup->pszViewString = strdup(p);
        pctrFreeSetupValue(p);
    }
    else
    {
        pSetup->pszViewString = "Temp:%T0/%T1/%T2 øC - Fan:%F0/%F1/%F2 RPM - Volt:%V0/%V1/%V2/%V3/%V4/%V5/%V6";
    }
}

/*
 *@@ MwgtSaveSetup:
 *      composes a new setup string.
 *      The caller must invoke xstrClear on the
 *      string after use.
 *
 *@@added V0.9.7 (2000-12-04) [umoeller]
 *@@changed V0.9.9 (2001-02-06) [smilcke]: Added setup strings for health monitoring
 */

VOID MwgtSaveSetup(PXSTRING pstrSetup,  // out: setup string (is cleared first)
                   PMONITORSETUP pSetup)
{
    CHAR szTemp[400];
    PSZ psz = 0;

    pxstrInit(pstrSetup, 400);
    sprintf(szTemp, "BGNDCOL=%06lX;", pSetup->lcolBackground);
    pxstrcat(pstrSetup, szTemp, 0);
    sprintf(szTemp, "TEXTCOL=%06lX;", pSetup->lcolForeground);
    pxstrcat(pstrSetup, szTemp, 0);
    if (pSetup->pszFont)
    {
        // non-default font:
        sprintf(szTemp, "FONT=%s;", pSetup->pszFont);
        pxstrcat(pstrSetup, szTemp, 0);
    }

    if(pSetup->pszViewString)
    {
        sprintf(szTemp,"HEALTHVSTR=Hallo;", pSetup->pszViewString);
        pxstrcat(pstrSetup, szTemp, 0);
    }
}

/*
 *@@ buildHealthString:
 *      composes a new string with health values.
 *
 *@@added V0.9.9 (2001-02-06) [smilcke]:
 */

void buildHealthString(PSZ szPaint,PSZ szViewString)
{
    if(szPaint && szViewString)
    {
        CHAR identifier;
        CHAR stringValue[500];
        unsigned int number;
        double t[10];
        int f[10];
        double v[10];
        int i,j;
        szPaint[0]=(char)0;
        for(i=0;i<10;i++)
        {
            t[i]=sthTemp(i,FALSE);
            f[i]=sthFan(i,FALSE);
            v[i]=sthVoltage(i,FALSE);
        }

        j=0;

        for(i = 0;
            i < strlen(szViewString);
            i++)
        {
            if(szViewString[i]=='%')
            {
                if(strlen(szViewString) >= i+2)
                {
                    identifier=szViewString[i+1];
                    number=szViewString[i+2]-'0';
                    if (number > 9)
                        number=9;
                    switch(identifier)
                    {
                        case 'T':
                            if(t[number]==STHEALTH_NOT_PRESENT_ERROR)
                                strcat(szPaint,"[ERR]");
                            else
                            {
                                sprintf(stringValue,"%.2f",t[number]);
                                strcat(szPaint,stringValue);
                            }
                            i+=2;
                        break;

                        case 'V':
                            if(v[number]==STHEALTH_NOT_PRESENT_ERROR)
                                strcat(szPaint,"[ERR]");
                            else
                            {
                                sprintf(stringValue,"%.2f",v[number]);
                                strcat(szPaint,stringValue);
                            }
                            i+=2;
                        break;

                        case 'F':
                            if(f[number]==STHEALTH_NOT_PRESENT_ERROR)
                                strcat(szPaint,"[ERR]");
                            else
                            {
                                sprintf(stringValue,"%d",f[number]);
                                strcat(szPaint,stringValue);
                            }
                            i+=2;
                        break;
                    }
                }
            }
            else
            {
                stringValue[0]=szViewString[i];
                stringValue[1]=(char)0;
                strcat(szPaint,stringValue);
            }
        }
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
 *@@ MwgtCreate:
 *      implementation for WM_CREATE.
 */

MRESULT MwgtCreate(HWND hwnd,
                   PXCENTERWIDGET pWidget)
{
    MRESULT mrc = 0;            // continue window creation

    PSZ p;
    PMONITORPRIVATE pPrivate = malloc(sizeof(MONITORPRIVATE));

    memset(pPrivate, 0, sizeof(MONITORPRIVATE));
    // link the two together
    pWidget->pUser = pPrivate;
    pPrivate->pWidget = pWidget;
    // get widget type (clock, memory, ...) from class setting;
    // this is lost after WM_CREATE
    if (pWidget->pWidgetClass)
        pPrivate->ulType = pWidget->pWidgetClass->ulExtra;

    pPrivate->cxCurrent = 10;   // we'll resize ourselves later
    pPrivate->cyCurrent = 10;
    // initialize binary setup structure from setup string
    MwgtScanSetup(pWidget->pcszSetupString, &pPrivate->Setup);
    // set window font (this affects all the cached presentation
    // spaces we use)
    pwinhSetWindowFont(hwnd, (pPrivate->Setup.pszFont) ? pPrivate->Setup.pszFont
    // default font: use the same as in the rest of XWorkplace:
                       : pcmnQueryDefaultFont());

    // enable context menu help
    pWidget->pcszHelpLibrary = pcmnQueryHelpLibrary();
    switch (pPrivate->ulType)
    {
        case MWGT_HEALTHMONITOR:
            pWidget->ulHelpPanelID = ID_XSH_WIDGET_CLOCK_MAIN;
            break;
    }
    // start update timer
    pPrivate->ulTimerID = ptmrStartTimer(hwnd, 1, 1500);
    return (mrc);
}

/*
 *@@ MwgtControl:
 *      implementation for WM_CONTROL.
 *
 *@@added V0.9.7 (2000-12-14) [umoeller]
 */

BOOL MwgtControl(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    BOOL brc = FALSE;
    PXCENTERWIDGET pWidget = (PXCENTERWIDGET) WinQueryWindowPtr(hwnd, QWL_USER);

    if (pWidget)
    {
        PMONITORPRIVATE pPrivate = (PMONITORPRIVATE) pWidget->pUser;

        if (pPrivate)
        {
            USHORT usID = SHORT1FROMMP(mp1);
            USHORT usNotifyCode = SHORT2FROMMP(mp1);

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
                        PSIZEL pszl = (PSIZEL) mp2;

                        pszl->cx = pPrivate->cxCurrent;
                        pszl->cy = pPrivate->cyCurrent;
                        brc = TRUE;
                        break;
                    }
                }

            }
        }
    } // end if (pWidget)

    return (brc);
}

/*
 * MwgtPaint:
 *      implementation for WM_PAINT.
 *
 *      The specified HPS is switched to RGB mode before
 *      painting.
 */

VOID MwgtPaint(HWND hwnd, PMONITORPRIVATE pPrivate, HPS hps, BOOL fDrawFrame)
{
    RECTL rclWin;
    ULONG ulBorder = 1;
    CHAR szPaint[900] = "";
    CHAR szValue[200];
    ULONG ulPaintLen = 0;
    POINTL aptlText[TXTBOX_COUNT];
    INT i;
    double dvalue;
    INT ivalue;
    LONG lcol = pPrivate->Setup.lcolBackground;

    // country settings from XCenter globals
    // (what a pointer)
    PCOUNTRYSETTINGS pCountrySettings
        = (PCOUNTRYSETTINGS) pPrivate->pWidget->pGlobals->pCountrySettings;

    // now paint button frame
    WinQueryWindowRect(hwnd, &rclWin);
    pgpihSwitchToRGB(hps);
    if (pPrivate->pWidget->pGlobals->flDisplayStyle & XCS_SUNKBORDERS)
    {
        if (fDrawFrame)
        {
            RECTL rcl2;

            rcl2.xLeft = rclWin.xLeft;
            rcl2.yBottom = rclWin.yBottom;
            rcl2.xRight = rclWin.xRight - 1;
            rcl2.yTop = rclWin.yTop - 1;
            pgpihDraw3DFrame(hps, &rcl2, ulBorder
                             ,pPrivate->pWidget->pGlobals->lcol3DDark
                             ,pPrivate->pWidget->pGlobals->lcol3DLight);
        }

        // now paint middle
        rclWin.xLeft += ulBorder;
        rclWin.yBottom += ulBorder;
        rclWin.xRight -= ulBorder;
        rclWin.yTop -= ulBorder;
    }
    // Because frame color can change frequently (wether there is an
    // active event or not) we have to paint the background every time
    // if(fDrawFrame)
    {
        // Get color from setup
        // Check, if there is an event for this source active
        for (i = 0; i < 10; i++)
        {
            STHEALTH_EVT_FILTER_102 fltr;
            ULONG hEvt[10];
            ULONG numEvents = 0;

            fltr.cbLen = sizeof(STHEALTH_EVT_FILTER_102);
            fltr.source = 1;
            fltr.sourceNumber = i;
            fltr.actionState = STHEALTH_EVT_STATE_ISACTIVE;
            sthFilterEvents(hEvt, 10, &numEvents, STHEALTH_EVT_FILTER_LEVEL_102, &fltr);
            if (numEvents)
            {
                // Active event found. Set color to red
                lcol = 0x00ff0000;
                break;
            }
            fltr.source = 2;
            sthFilterEvents(hEvt, 10, &numEvents, STHEALTH_EVT_FILTER_LEVEL_102, &fltr);
            if (numEvents)
            {
                // Active event found. Set color to red
                lcol = 0x00ff0000;
                break;
            }
            fltr.source = 3;
            sthFilterEvents(hEvt, 10, &numEvents, STHEALTH_EVT_FILTER_LEVEL_102, &fltr);
            if (numEvents)
            {
                // Active event found. Set color to red
                lcol = 0x00ff0000;
                break;
            }
        }
        WinFillRect(hps, &rclWin, lcol);
    }
    switch (pPrivate->ulType)
    {
        case MWGT_HEALTHMONITOR:
            buildHealthString(szPaint,pPrivate->Setup.pszViewString);
            break;
    }
    ulPaintLen = strlen(szPaint);
    GpiQueryTextBox(hps, ulPaintLen, szPaint, TXTBOX_COUNT, aptlText);
    if (abs(aptlText[TXTBOX_TOPRIGHT].x + 4 - rclWin.xRight) > 4)
    {
        // we need more space: tell XCenter client
        pPrivate->cxCurrent = (aptlText[TXTBOX_TOPRIGHT].x + 2 * ulBorder + 4);
        WinPostMsg(WinQueryWindow(hwnd, QW_PARENT), XCM_SETWIDGETSIZE
                   ,(MPARAM) hwnd, (MPARAM) pPrivate->cxCurrent);
    }
    else
    {
        // sufficient space:
        GpiSetBackMix(hps, BM_OVERPAINT);
        WinDrawText(hps, ulPaintLen, szPaint, &rclWin,
                    pPrivate->Setup.lcolForeground,
                    lcol,
                    DT_CENTER | DT_VCENTER);
    }
}

/*
 *@@ MwgtPresParamChanged:
 *      implementation for WM_PRESPARAMCHANGED.
 */

VOID MwgtPresParamChanged(HWND hwnd,
                          ULONG ulAttrChanged,
                          PXCENTERWIDGET pWidget)
{
    PMONITORPRIVATE pPrivate = (PMONITORPRIVATE) pWidget->pUser;

    if (pPrivate)
    {
        BOOL fInvalidate = TRUE;

        switch (ulAttrChanged)
        {
            case 0:         // layout palette thing dropped

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
                    break;
                }
            default:
                fInvalidate = FALSE;
        }
        if (fInvalidate)
        {
            XSTRING strSetup;

            WinInvalidateRect(hwnd, NULL, FALSE);
            MwgtSaveSetup(&strSetup, &pPrivate->Setup);
            if (strSetup.ulLength)
                WinSendMsg(pPrivate->pWidget->pGlobals->hwndClient,
                           XCM_SAVESETUP,
                           (MPARAM) hwnd,
                           (MPARAM) strSetup.psz);
            pxstrClear(&strSetup);
        }
    }                           // end if (pPrivate)

}

/*
 *@@ MwgtButton1DblClick:
 *      implementation for WM_BUTTON1DBLCLK.
 */

VOID MwgtButton1DblClick(HWND hwnd, PXCENTERWIDGET pWidget)
{
    PMONITORPRIVATE pPrivate = (PMONITORPRIVATE) pWidget->pUser;

    if (pPrivate)
    {
        const char *pcszID = NULL;
        HOBJECT hobj = NULLHANDLE;

        switch (pPrivate->ulType)
        {
            case MWGT_HEALTHMONITOR:
                break;
        }
        if (pcszID)
            hobj = WinQueryObject((PSZ) pcszID);
        if (hobj)
        {
            WinOpenObject(hobj, 2,  // OPEN_SETTINGS,
                           TRUE);
        }
    }                           // end if (pPrivate)

}

/*
 *@@ fnwpMonitorWidgets:
 *      window procedure for the various "Monitor" widget classes.
 *
 *      This window proc is shared among the "Monitor" widgets,
 *      which all have in common that they are text-only display
 *      widgets with a little bit of extra functionality.
 *
 *      Presently, the following widgets are implemented this way:
 *      -- clock;
 *      -- swapper monitor;
 *      -- available-memory monitor.
 *      Supported setup strings:
 *      -- "TYPE={CLOCK|SWAPPER|MEMORY}"
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
         *      MONITORPRIVATE for our own stuff.
         */

        case WM_CREATE:
            WinSetWindowPtr(hwnd, QWL_USER, mp1);
            pWidget = (PXCENTERWIDGET)mp1;
            if ((pWidget) && (pWidget->pfnwpDefWidgetProc))
                mrc = MwgtCreate(hwnd, pWidget);
            else
                // stop window creation!!
                mrc = (MPARAM)TRUE;
        break;

        /*
         * WM_CONTROL:
         *      process notifications/queries from the XCenter.
         */

        case WM_CONTROL:
            mrc = (MPARAM)MwgtControl(hwnd, mp1, mp2);
        break;

        /*
         * WM_PAINT:
         *
         */

        case WM_PAINT:
        {
            HPS hps = WinBeginPaint(hwnd, NULLHANDLE, NULL);

            if (hps)
            {
                // get widget data and its button data from QWL_USER
                PMONITORPRIVATE pPrivate = (PMONITORPRIVATE)pWidget->pUser;
                if (pPrivate)
                {
                    MwgtPaint(hwnd,
                              pPrivate,
                              hps,
                              TRUE);        // draw everything
                } // end if (pPrivate)

                WinEndPaint(hps);
            }
        break; }

        /*
         * WM_TIMER:
         *      clock timer --> repaint.
         */

        case WM_TIMER:
        {
            HPS hps = WinGetPS(hwnd);
            if (hps)
            {
                // get widget data and its button data from QWL_USER
                PMONITORPRIVATE pPrivate = (PMONITORPRIVATE)pWidget->pUser;
                if (pPrivate)
                {
                    MwgtPaint(hwnd,
                              pPrivate,
                              hps,
                              FALSE);   // text only
                } // end if (pPrivate)

                WinReleasePS(hps);
            }
        break; }

        /*
         *@@ WM_BUTTON1DBLCLK:
         *      on double-click on clock, open
         *      system clock settings.
         */

        case WM_BUTTON1DBLCLK:
            MwgtButton1DblClick(hwnd, pWidget);
            mrc = (MPARAM)TRUE;     // message processed
        break;

        /*
         * WM_PRESPARAMCHANGED:
         *
         */

        case WM_PRESPARAMCHANGED:
            if (pWidget)
                // this gets sent before this is set!
                MwgtPresParamChanged(hwnd, (ULONG)mp1, pWidget);
        break;

        /*
         * WM_DESTROY:
         *      clean up. This _must_ be passed on to
         *      ctrDefWidgetProc.
         */

        case WM_DESTROY:
        {
            PMONITORPRIVATE pPrivate = (PMONITORPRIVATE)pWidget->pUser;
            if (pPrivate)
            {
                if (pPrivate->ulTimerID)
                    ptmrStopTimer(hwnd,
                                  pPrivate->ulTimerID);
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
 *@@ MwgtInitModule:
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

ULONG EXPENTRY MwgtInitModule(HAB hab,  // XCenter's anchor block
                              HMODULE hmodPlugin, // module handle of the widget DLL
                              HMODULE hmodXFLDR,   // XFLDR.DLL module handle
                              PXCENTERWIDGETCLASS *ppaClasses,
                              PSZ pszErrorMsg)  // if 0 is returned, 500 bytes of error msg
{
    ULONG ulrc = 0;
    ULONG ul = 0;
    BOOL fImportsFailed = FALSE;

    // resolve imports from XFLDR.DLL (this is basically
    // a copy of the doshResolveImports code, but we can't
    // use that before resolving...)
    for (ul = 0; ul < sizeof(G_aImports) / sizeof(G_aImports[0]); ul++)
    {
        if (DosQueryProcAddr(hmodXFLDR,
                             0, // ordinal, ignored
                             (PSZ)G_aImports[ul].pcszFunctionName,
                             G_aImports[ul].ppFuncAddress)
            != NO_ERROR)
        {
            sprintf(pszErrorMsg, "Import %s failed.", G_aImports[ul].pcszFunctionName);
            fImportsFailed = TRUE;
            break;
        }
    }

    if (!fImportsFailed)
    {
        if (!loadStHealth())
        {
            sthRegisterDaemon(FALSE);
            if (!sthDetectChip())
            {
                if (!WinRegisterClass(hab
                                      ,WNDCLASS_WIDGET_XWHEALTH
                                      ,fnwpMonitorWidgets
                                 ,CS_PARENTCLIP | CS_SIZEREDRAW | CS_SYNCPAINT
                                      ,sizeof(PMONITORPRIVATE)))
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
            else
                strcpy(pszErrorMsg, "No compatible chip detected.");
        }
        else
            strcpy(pszErrorMsg, "Unable to load StHealth.DLL.");
    }

    return (ulrc);
}

/*
 *@@ MwgtUnInitModule:
 *      optional export with ordinal 2, which can clean
 *      up global widget class data.
 *
 *      This gets called by the XCenter right before
 *      a widget DLL gets unloaded. Note that this
 *      gets called even if the "init module" export
 *      returned 0 (meaning an error) and the DLL
 *      gets unloaded right away.
 *
 *@@added V0.9.7 (2000-12-07) [umoeller]
 */

VOID EXPENTRY MwgtUnInitModule(VOID)
{
    if (sthUnregisterDaemon)
        sthUnregisterDaemon();
    unloadStHealth();
}

/*
 *@@ MwgtQueryVersion:
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
 *
 *@@added V0.9.9 (2001-02-06) [umoeller]
 */

VOID EXPENTRY MwgtQueryVersion(PULONG pulMajor,
                               PULONG pulMinor,
                               PULONG pulRevision)
{
    // report 0.9.9
    *pulMajor = 0;
    *pulMinor = 9;
    *pulRevision = 9;
}

