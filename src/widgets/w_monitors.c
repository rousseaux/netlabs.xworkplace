
/*
 *@@sourcefile w_monitors.c:
 *      XCenter monitor widgets (clock, memory, swapper).
 *
 *      This is an example of an XCenter widget plugin.
 *      This widget resides in MONITORS.DLL, which (as
 *      with all widget plugins) must be put into the
 *      plugins/xcenter directory of the XWorkplace
 *      installation directory.
 *
 *      Any XCenter widget plugin DLL must export the
 *      following procedures by ordinal:
 *
 *      -- Ordinal 1 (MwgtInitModule): this must
 *         return the widgets which this DLL provides.
 *
 *      -- Ordinal 2 (MwgtUnInitModule): this must
 *         clean up global DLL data.
 *
 *      The makefile in src\widgets compiles widgets
 *      with the VAC subsystem library. As a result,
 *      multiple threads are not supported.
 *
 *      This is all new with V0.9.7.
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
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
#define INCL_DOSMODULEMGR
#define INCL_DOSEXCEPTIONS
#define INCL_DOSSEMAPHORES
#define INCL_DOSDATETIME
#define INCL_DOSMISC
#define INCL_DOSDEVICES
#define INCL_DOSDEVIOCTL
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

#include "startshut\apm.h"            // APM power-off for XShutdown

#pragma hdrstop                     // VAC++ keeps crashing otherwise

/* ******************************************************************
 *
 *   Private definitions
 *
 ********************************************************************/

#define MWGT_DATE               1
#define MWGT_SWAPPER            2
#define MWGT_MEMORY             3
#define MWGT_TIME               4
#define MWGT_POWER              5       // V0.9.12 (2001-05-26) [umoeller]

APIRET16 APIENTRY16 Dos16MemAvail(PULONG pulAvailMem);

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

#define WNDCLASS_WIDGET_MONITORS    "XWPCenterMonitorWidget"

static XCENTERWIDGETCLASS G_WidgetClasses[] =
            {
                {
                    WNDCLASS_WIDGET_MONITORS,
                    MWGT_DATE,
                    "Date",
                    "Date monitor",
                    WGTF_UNIQUEPERXCENTER | WGTF_TOOLTIP | WGTF_TRAYABLE,
                    NULL        // no settings dlg
                },
                {
                    WNDCLASS_WIDGET_MONITORS,
                    MWGT_TIME,
                    "Time",
                    "Time monitor",
                    WGTF_UNIQUEPERXCENTER | WGTF_TOOLTIP | WGTF_TRAYABLE,
                    NULL        // no settings dlg
                },
                {
                    WNDCLASS_WIDGET_MONITORS,
                    MWGT_MEMORY,
                    "PhysMemory",
                    "Free physical memory monitor",
                    WGTF_UNIQUEPERXCENTER | WGTF_TOOLTIP | WGTF_TRAYABLE,
                    NULL        // no settings dlg
                },
                {
                    WNDCLASS_WIDGET_MONITORS,
                    MWGT_POWER,
                    "Power",
                    "Battery power monitor",
                    WGTF_UNIQUEGLOBAL | WGTF_TOOLTIP | WGTF_TRAYABLE,
                    NULL        // no settings dlg
                }
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
PCMNQUERYMAINRESMODULEHANDLE pcmnQueryMainResModuleHandle = NULL;

PCTRFREESETUPVALUE pctrFreeSetupValue = NULL;
PCTRPARSECOLORSTRING pctrParseColorString = NULL;
PCTRSCANSETUPSTRING pctrScanSetupString = NULL;

PGPIHDRAW3DFRAME pgpihDraw3DFrame = NULL;
PGPIHSWITCHTORGB pgpihSwitchToRGB = NULL;

PSTRHDATETIME pstrhDateTime = NULL;
PSTRHTHOUSANDSULONG pstrhThousandsULong = NULL;

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
        "cmnQueryDefaultFont", (PFN*)&pcmnQueryDefaultFont,
        "cmnQueryHelpLibrary", (PFN*)&pcmnQueryHelpLibrary,
        "cmnQueryMainResModuleHandle", (PFN*)&pcmnQueryMainResModuleHandle,
        "ctrFreeSetupValue", (PFN*)&pctrFreeSetupValue,
        "ctrParseColorString", (PFN*)&pctrParseColorString,
        "ctrScanSetupString", (PFN*)&pctrScanSetupString,
        "gpihDraw3DFrame", (PFN*)&pgpihDraw3DFrame,
        "gpihSwitchToRGB", (PFN*)&pgpihSwitchToRGB,
        "strhDateTime", (PFN*)&pstrhDateTime,
        "strhThousandsULong", (PFN*)&pstrhThousandsULong,
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
 *   APM definitions (from DDK)
 *
 ********************************************************************/

/*---------------------------------------------------------------------------*
 * Category 12 (Power Management) IOCtl Function Codes                       *
 *---------------------------------------------------------------------------*/

#define APMGIO_Category           12    // Generic IOCtl Category for APM.

#define APMGIO_SendEvent        0x40    // Function Codes.
#define APMGIO_SetEventSem      0x41
#define APMGIO_ConfirmEvent     0x42    // 0x42 is UNDOCUMENTED.
#define APMGIO_BroadcastEvent   0x43    // 0x43 is UNDOCUMENTED.
#define APMGIO_RegDaemonThread  0x44    // 0x44 is UNDOCUMENTED.
#define APMGIO_OEMFunction      0x45
#define APMGIO_QueryStatus      0x60
#define APMGIO_QueryEvent       0x61
#define APMGIO_QueryInfo        0x62
#define APMGIO_QueryState       0x63

#pragma pack(1)

/*---------------------------------------------------------------------------*
 * Function 0x60, Query Power Status                                         *
 * Reference MS/Intel APM specification for interpretation of status codes.  *
 *---------------------------------------------------------------------------*/

typedef struct _APMGIO_QSTATUS_PPKT {           // Parameter Packet.

  USHORT ParmLength;    // Length, in bytes, of the Parm Packet.
  USHORT Flags;         // Output:  Flags.
  UCHAR  ACStatus;
                        // Output:  0x00 if not on AC,
                        //          0x01 if AC,
                        //          0x02 if on backup power,
                        //          0xFF if unknown
  UCHAR  BatteryStatus;
                        // Output:  0x00 if battery high,
                        //          0x01 if battery low,
                        //          0x02 if battery critically low,
                        //          0x03 if battery charging
                        //          0xFF if unknown
  UCHAR  BatteryLife;
                        // Output:  Battery power life (as percentage)
  UCHAR  BatteryTimeForm;
                        // Output:
                        //          0x00 if format is seconds,
                        //          0x01 if format is minutes,
                        //          0xFF if unknown
  USHORT BatteryTime;   // Output:  Remaining battery time.
  UCHAR  BatteryFlags;  // Output:  Battery status flags

} APMGIO_QSTATUS_PPKT, *NPAPMGIO_QSTATUS_PPKT, FAR *PAPMGIO_QSTATUS_PPKT;

typedef struct _APMGIO_10_QSTATUS_PPKT {        // Parameter Packet for
                                                // APM 1.0 interface
  USHORT ParmLength;    // Length, in bytes, of the Parm Packet.
  USHORT Flags;         // Output:  Flags.
  UCHAR  ACStatus;      // Output:  AC line power status.
  UCHAR  BatteryStatus; // Output:  Battery power status
  UCHAR  BatteryLife;   // Output:  Battery power status

} APMGIO_10_QSTATUS_PPKT;

// Error return codes.
#define GIOERR_PowerNoError             0
#define GIOERR_PowerBadSubId            1
#define GIOERR_PowerBadReserved         2
#define GIOERR_PowerBadDevId            3
#define GIOERR_PowerBadPwrState         4
#define GIOERR_PowerSemAlreadySetup     5
#define GIOERR_PowerBadFlags            6
#define GIOERR_PowerBadSemHandle        7
#define GIOERR_PowerBadLength           8
#define GIOERR_PowerDisabled            9
#define GIOERR_PowerNoEventQueue       10
#define GIOERR_PowerTooManyQueues      11
#define GIOERR_PowerBiosError          12
#define GIOERR_PowerBadSemaphore       13
#define GIOERR_PowerQueueOverflow      14
#define GIOERR_PowerStateChangeReject  15
#define GIOERR_PowerNotSupported       16
#define GIOERR_PowerDisengaged         17
#define GIOERR_PowerHighestErrCode     17

typedef struct _APMGIO_DPKT {

  USHORT ReturnCode;

} APMGIO_DPKT, *NPAPMGIO_DPKT, FAR *PAPMGIO_DPKT;

#pragma pack()

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
                // one of the following:
                // -- MWGT_DATE: date widget
                // -- MWGT_TIME: time widget
                // -- MWGT_SWAPPER: swap monitor widget
                // -- MWGT_MEMORY: memory monitor widget;
                // -- MWGT_POWER: power monitor widget;
                // this is copied from the widget class on WM_CREATE

    ULONG           cxCurrent,
                    cyCurrent;

    MONITORSETUP    Setup;
            // widget settings that correspond to a setup string

    ULONG           ulTimerID;              // if != NULLHANDLE, update timer is running

    HFILE           hfAPMSys;               // APM.SYS driver handle, if open
    APIRET          arcAPM;                 // error code if APM.SYS open failed

    ULONG           ulBatteryStatus;
                // copy of APM battery status, that is:
                        // Output:  0x00 if battery high,
                        //          0x01 if battery low,
                        //          0x02 if battery critically low,
                        //          0x03 if battery charging
                        //          0xFF if unknown
    ULONG           ulBatteryLife;
                // current battery life as percentage

    HPOINTER        hptrAC,         // "AC" icon
                    hptrBattery;    // "battery" icon

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
}

/*
 *@@ MwgtSaveSetup:
 *      composes a new setup string.
 *      The caller must invoke xstrClear on the
 *      string after use.
 *
 *@@added V0.9.7 (2000-12-04) [umoeller]
 */

VOID MwgtSaveSetup(PXSTRING pstrSetup,       // out: setup string (is cleared first)
                   PMONITORSETUP pSetup)
{
    CHAR    szTemp[100];
    PSZ     psz = 0;
    pxstrInit(pstrSetup, 100);

    sprintf(szTemp, "BGNDCOL=%06lX;",
            pSetup->lcolBackground);
    pxstrcat(pstrSetup, szTemp, 0);

    sprintf(szTemp, "TEXTCOL=%06lX;",
            pSetup->lcolForeground);
    pxstrcat(pstrSetup, szTemp, 0);

    if (pSetup->pszFont)
    {
        // non-default font:
        sprintf(szTemp, "FONT=%s;",
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
 *   APM monitor
 *
 ********************************************************************/

/*
 *@@ ApmIOCtl:
 *
 *@@added V0.9.12 (2001-05-26) [umoeller]
 */

APIRET ApmIOCtl(HFILE hfAPMSys,
                ULONG ulFunction,
                PVOID pvParamPck,
                ULONG cbParamPck)
{
    APIRET          arc;
    APMGIO_DPKT     DataPacket;
    ULONG           ulRetSize = sizeof(DataPacket);
    DataPacket.ReturnCode = GIOERR_PowerNoError;
    if (!(arc = DosDevIOCtl(hfAPMSys,
                            APMGIO_Category,
                            ulFunction,
                            pvParamPck, cbParamPck, &cbParamPck,
                            &DataPacket, sizeof(DataPacket), &ulRetSize)))
        if (DataPacket.ReturnCode)
            arc = DataPacket.ReturnCode | 10000;

    return (arc);
}

/*
 *@@ ReadNewAPMValue:
 *
 *@@added V0.9.12 (2001-05-26) [umoeller]
 */

VOID ReadNewAPMValue(PMONITORPRIVATE pPrivate)
{
    if ((!pPrivate->arcAPM) && (pPrivate->hfAPMSys))
    {
        APMGIO_QSTATUS_PPKT  PowerStatus;
        PowerStatus.ParmLength = sizeof(PowerStatus);

        if (!(pPrivate->arcAPM = ApmIOCtl(pPrivate->hfAPMSys,
                                          APMGIO_QueryStatus,
                                          &PowerStatus,
                                          PowerStatus.ParmLength)))
        {
            pPrivate->ulBatteryStatus = PowerStatus.BatteryStatus;
            pPrivate->ulBatteryLife = PowerStatus.BatteryLife;
        }
    }
}

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
 *
 *@@changed V0.9.12 (2001-05-26) [umoeller]: added "power" support
 */

MRESULT MwgtCreate(HWND hwnd,
                   PXCENTERWIDGET pWidget)
{
    MRESULT mrc = 0;        // continue window creation
    ULONG ulUpdateFreq = 1000;

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

    pPrivate->cxCurrent = 10;          // we'll resize ourselves later
    pPrivate->cyCurrent = 10;

    // initialize binary setup structure from setup string
    MwgtScanSetup(pWidget->pcszSetupString,
                  &pPrivate->Setup);

    // set window font (this affects all the cached presentation
    // spaces we use)
    pwinhSetWindowFont(hwnd,
                       (pPrivate->Setup.pszFont)
                        ? pPrivate->Setup.pszFont
                        // default font: use the same as in the rest of XWorkplace:
                        : pcmnQueryDefaultFont());

    // enable context menu help
    pWidget->pcszHelpLibrary = pcmnQueryHelpLibrary();
    switch (pPrivate->ulType)
    {
        case MWGT_DATE:
        case MWGT_TIME:
            pWidget->ulHelpPanelID = ID_XSH_WIDGET_CLOCK_MAIN;
        break;

        case MWGT_SWAPPER:
            pWidget->ulHelpPanelID = ID_XSH_WIDGET_SWAP_MAIN;
        break;

        case MWGT_MEMORY:
            pWidget->ulHelpPanelID = ID_XSH_WIDGET_MEMORY_MAIN;
        break;

        case MWGT_POWER:
        {
            ULONG ulAction = 0;

            pWidget->ulHelpPanelID = ID_XSH_WIDGET_POWER_MAIN;
            ulUpdateFreq = 10 * 1000;       // once per minute

            // open APM.SYS
            pPrivate->arcAPM = DosOpen("\\DEV\\APM$",
                                       &pPrivate->hfAPMSys,
                                       &ulAction,
                                       0,
                                       FILE_NORMAL,
                                       OPEN_ACTION_OPEN_IF_EXISTS,
                                       OPEN_FLAGS_FAIL_ON_ERROR
                                            | OPEN_SHARE_DENYNONE
                                            | OPEN_ACCESS_READWRITE,
                                       NULL);
            if (!pPrivate->arcAPM)
            {
                GETPOWERINFO    getpowerinfo;
                ULONG           ulAPMRc = NO_ERROR;
                ULONG           ulPacketSize = sizeof(getpowerinfo),
                                ulDataSize = sizeof(ulAPMRc);
                // query version of APM-BIOS and APM driver
                memset(&getpowerinfo, 0, sizeof(getpowerinfo));
                getpowerinfo.usParmLength = sizeof(getpowerinfo);

                if (!(pPrivate->arcAPM = ApmIOCtl(pPrivate->hfAPMSys,
                                                  POWER_GETPOWERINFO,
                                                  &getpowerinfo,
                                                  getpowerinfo.usParmLength)))
                {
                    // swap lower-byte(major vers.) to higher-byte(minor vers.)
                    USHORT usBIOSVersion =     (getpowerinfo.usBIOSVersion & 0xff) << 8
                                             | (getpowerinfo.usBIOSVersion >> 8);
                    USHORT usDriverVersion =   (getpowerinfo.usDriverVersion & 0xff) << 8
                                             | (getpowerinfo.usDriverVersion >> 8);

                    // set general APM version to lower
                    USHORT usLowestAPMVersion = (usBIOSVersion < usDriverVersion)
                                                 ? usBIOSVersion : usDriverVersion;

                    if (usLowestAPMVersion < 0x101)  // version 1.1 or above
                        // not 1.2 or higher:
                        pPrivate->arcAPM = -1;
                    else
                    {
                        // no error at all:
                        // read first value, because next update
                        // won't be before 1 minute from now
                        ReadNewAPMValue(pPrivate);

                        // and load the icons
                        pPrivate->hptrAC = WinLoadPointer(HWND_DESKTOP,
                                                          pcmnQueryMainResModuleHandle(),
                                                          ID_POWER_AC);
                        pPrivate->hptrBattery = WinLoadPointer(HWND_DESKTOP,
                                                               pcmnQueryMainResModuleHandle(),
                                                               ID_POWER_BATTERY);

                        pPrivate->cyCurrent = pWidget->pGlobals->cxMiniIcon + 2;
                    }
                }
            }

            if (pPrivate->arcAPM)
                ulUpdateFreq = 0;
        }
        break;
    }

    // start update timer
    if (ulUpdateFreq)
        pPrivate->ulTimerID = ptmrStartXTimer(pWidget->pGlobals->pvXTimerSet,
                                              hwnd,
                                              1,
                                              ulUpdateFreq);    // 1000, unless "power"

    return (mrc);
}

/*
 *@@ MwgtControl:
 *      implementation for WM_CONTROL.
 *
 *@@added V0.9.7 (2000-12-14) [umoeller]
 *@@changed V0.9.9 (2001-02-01) [umoeller]: added tooltips
 */

BOOL MwgtControl(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    BOOL brc = FALSE;
    PXCENTERWIDGET pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER);

    if (pWidget)
    {
        PMONITORPRIVATE pPrivate = (PMONITORPRIVATE)pWidget->pUser;
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
                        pszl->cx = pPrivate->cxCurrent;
                        pszl->cy = pPrivate->cyCurrent;
                        brc = TRUE;
                    break; }
                }
            }
            else if (usID == ID_XCENTER_TOOLTIP)
            {
                if (usNotifyCode == TTN_NEEDTEXT)
                {
                    PTOOLTIPTEXT pttt = (PTOOLTIPTEXT)mp2;
                    switch (pPrivate->ulType)
                    {
                        case MWGT_DATE:
                            pttt->pszText = "Date";
                        break;

                        case MWGT_TIME:
                            pttt->pszText = "Time";
                        break;

                        case MWGT_SWAPPER:
                            pttt->pszText = "Current swapper size";
                        break;

                        case MWGT_MEMORY:
                            pttt->pszText = "Currently free memory";
                        break;

                        case MWGT_POWER:
                            pttt->pszText = "Battery power";
                        break;
                    }

                    pttt->ulFormat = TTFMT_PSZ;
                }
            }
        } // end if (pPrivate)
    } // end if (pWidget)

    return (brc);
}

/*
 * MwgtPaint:
 *      implementation for WM_PAINT.
 *
 *      The specified HPS is switched to RGB mode before
 *      painting.
 *
 *@@changed V0.9.12 (2001-05-26) [umoeller]: added "power" support
 */

VOID MwgtPaint(HWND hwnd,
               PMONITORPRIVATE pPrivate,
               HPS hps,
               BOOL fDrawFrame)
{
    RECTL       rclWin;
    ULONG       ulBorder = 1;
    CHAR        szPaint[400] = "";
    ULONG       ulPaintLen = 0;
    POINTL      aptlText[TXTBOX_COUNT];
    ULONG       ulExtraWidth = 0;

    // country settings from XCenter globals
    // (what a pointer)
    PCOUNTRYSETTINGS pCountrySettings
        = (PCOUNTRYSETTINGS)pPrivate->pWidget->pGlobals->pCountrySettings;

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
            pgpihDraw3DFrame(hps,
                             &rcl2,
                             ulBorder,
                             pPrivate->pWidget->pGlobals->lcol3DDark,
                             pPrivate->pWidget->pGlobals->lcol3DLight);
        }

        // now paint middle
        rclWin.xLeft += ulBorder;
        rclWin.yBottom += ulBorder;
        rclWin.xRight -= ulBorder;
        rclWin.yTop -= ulBorder;
    }

    if (fDrawFrame)
        WinFillRect(hps,
                    &rclWin,
                    pPrivate->Setup.lcolBackground);

    switch (pPrivate->ulType)
    {
        case MWGT_DATE:
        case MWGT_TIME:
        {
            DATETIME    DateTime;

            DosGetDateTime(&DateTime);
            pstrhDateTime((pPrivate->ulType == MWGT_DATE) ? szPaint : NULL,
                          (pPrivate->ulType == MWGT_TIME) ? szPaint : NULL,
                          &DateTime,
                          pCountrySettings->ulDateFormat,
                          pCountrySettings->cDateSep,
                          pCountrySettings->ulTimeFormat,
                          pCountrySettings->cTimeSep);
        break; }

        case MWGT_SWAPPER:
        break;

        case MWGT_MEMORY:
        {
            ULONG ulMem = 0,
                  ulLogicalSwapDrive = 8;
            if (Dos16MemAvail(&ulMem) == NO_ERROR)
            {
                pstrhThousandsULong(szPaint,
                                    ulMem / 1024,
                                    pCountrySettings->cThousands);
                strcat(szPaint, " KB");
            }
            else
                strcpy(szPaint, "?!?!?");
        break; }

        case MWGT_POWER:
            if (pPrivate->arcAPM == -1)
                // insufficient APM version:
                strcpy(szPaint, "APM 1.2 required.");
            else if (pPrivate->arcAPM)
                // other error occured:
                sprintf(szPaint, "E %d", pPrivate->arcAPM);
            else
            {
                // APM is OK:
                const char *pcsz = NULL;
                switch (pPrivate->ulBatteryStatus)
                {
                    case 0x00: pcsz = "High"; break;
                    case 0x01: pcsz = "Low"; break;
                    case 0x02: pcsz = "Critical"; break;
                    case 0x03: pcsz = "Charging"; break;

                    default: sprintf(szPaint, "No battery");
                }
                if (pcsz)
                {
                    ULONG cxMiniIcon = pPrivate->pWidget->pGlobals->cxMiniIcon;

                    sprintf(szPaint, "%d%%", pPrivate->ulBatteryLife);

                    WinDrawPointer(hps,
                                   0,
                                   (    (rclWin.yTop - rclWin.yBottom)
                                      - cxMiniIcon
                                   ) / 2,
                                   (pPrivate->ulBatteryStatus == 3)
                                        ? pPrivate->hptrAC
                                        : pPrivate->hptrBattery,
                                   DP_MINI);

                    // add offset for painting text
                    ulExtraWidth = cxMiniIcon;
                }
            }
        break;
    }

    ulPaintLen = strlen(szPaint);
    GpiQueryTextBox(hps,
                    ulPaintLen,
                    szPaint,
                    TXTBOX_COUNT,
                    aptlText);
    if (    abs((aptlText[TXTBOX_TOPRIGHT].x + ulExtraWidth) + 4 - rclWin.xRight)
            > 4
       )
    {
        // we need more space: tell XCenter client
        pPrivate->cxCurrent = (aptlText[TXTBOX_TOPRIGHT].x + ulExtraWidth + 2*ulBorder + 4);
        WinPostMsg(WinQueryWindow(hwnd, QW_PARENT),
                   XCM_SETWIDGETSIZE,
                   (MPARAM)hwnd,
                   (MPARAM)pPrivate->cxCurrent
                  );
    }
    else
    {
        // sufficient space:
        GpiSetBackMix(hps, BM_OVERPAINT);
        rclWin.xLeft += ulExtraWidth;
        WinDrawText(hps,
                    ulPaintLen,
                    szPaint,
                    &rclWin,
                    pPrivate->Setup.lcolForeground,
                    pPrivate->Setup.lcolBackground,
                    DT_CENTER | DT_VCENTER);
    }
}

/*
 *@@ MwgtPresParamChanged:
 *      implementation for WM_PRESPARAMCHANGED.
 *
 *@@changed V0.9.13 (2001-06-21) [umoeller]: changed XCM_SAVESETUP call for tray support
 */

VOID MwgtPresParamChanged(HWND hwnd,
                          ULONG ulAttrChanged,
                          PXCENTERWIDGET pWidget)
{
    PMONITORPRIVATE pPrivate = (PMONITORPRIVATE)pWidget->pUser;
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
            break; }

            default:
                fInvalidate = FALSE;
        }

        if (fInvalidate)
        {
            XSTRING strSetup;
            WinInvalidateRect(hwnd, NULL, FALSE);

            MwgtSaveSetup(&strSetup,
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
 *@@ MwgtButton1DblClick:
 *      implementation for WM_BUTTON1DBLCLK.
 *
 *@@changed V0.9.12 (2001-05-26) [umoeller]: added "power" support
 */

VOID MwgtButton1DblClick(HWND hwnd,
                         PXCENTERWIDGET pWidget)
{
    PMONITORPRIVATE pPrivate = (PMONITORPRIVATE)pWidget->pUser;
    if (pPrivate)
    {
        const char *pcszID = NULL;
        HOBJECT hobj = NULLHANDLE;
        ULONG ulView = 2; // OPEN_SETTINGS,

        switch (pPrivate->ulType)
        {
            case MWGT_DATE:
            case MWGT_TIME:
                pcszID = "<WP_CLOCK>";
            break;

            case MWGT_SWAPPER:
            case MWGT_MEMORY:
                pcszID = "<XWP_KERNEL>"; // XFOLDER_KERNELID; // "<XWP_KERNEL>";
            break;

            case MWGT_POWER:
                pcszID = "<WP_POWER>";
                ulView = 0; // OPEN_DEFAULT;
            break;
        }

        if (pcszID)
            hobj = WinQueryObject((PSZ)pcszID);

        if (hobj)
        {
            WinOpenObject(hobj,
                          ulView,
                          TRUE);
        }
    } // end if (pPrivate)
}

/*
 *@@ MwgtDestroy:
 *
 *@@added V0.9.12 (2001-05-26) [umoeller]
 */

VOID MwgtDestroy(PXCENTERWIDGET pWidget)
{
    PMONITORPRIVATE pPrivate = (PMONITORPRIVATE)pWidget->pUser;
    if (pPrivate)
    {
        if (pPrivate->ulTimerID)
            ptmrStopXTimer(pPrivate->pWidget->pGlobals->pvXTimerSet,
                           pWidget->hwndWidget,
                           pPrivate->ulTimerID);

        if (pPrivate->hfAPMSys)
            DosClose(pPrivate->hfAPMSys);

        if (pPrivate->hptrAC)
            WinDestroyPointer(pPrivate->hptrAC);
        if (pPrivate->hptrBattery)
            WinDestroyPointer(pPrivate->hptrBattery);

        free(pPrivate);
    } // end if (pPrivate)
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
 *
 *      -- clock;
 *
 *      -- swapper monitor;
 *
 *      -- available-memory monitor.
 *
 *      Supported setup strings:
 *
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
                    if (    (pPrivate->ulType == MWGT_POWER)
                         && (pPrivate->hfAPMSys)
                       )
                        ReadNewAPMValue(pPrivate);

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
            MwgtDestroy(pWidget);
            mrc = pWidget->pfnwpDefWidgetProc(hwnd, msg, mp1, mp2);
        break;

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

ULONG EXPENTRY MwgtInitModule(HAB hab,         // XCenter's anchor block
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
         ul < sizeof(G_aImports) / sizeof(G_aImports[0]);
         ul++)
    {
        if (DosQueryProcAddr(hmodXFLDR,
                             0,               // ordinal, ignored
                             (PSZ)G_aImports[ul].pcszFunctionName,
                             G_aImports[ul].ppFuncAddress)
                    != NO_ERROR)
        {
            sprintf(pszErrorMsg,
                    "Import %s failed.",
                    G_aImports[ul].pcszFunctionName);
            fImportsFailed = TRUE;
            break;
        }
    }

    if (!fImportsFailed)
    {
        if (!WinRegisterClass(hab,
                              WNDCLASS_WIDGET_MONITORS,
                              fnwpMonitorWidgets,
                              CS_PARENTCLIP | CS_SIZEREDRAW | CS_SYNCPAINT,
                              sizeof(PMONITORPRIVATE))
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
    *pulMajor = 0;
    *pulMinor = 9;
    *pulRevision = 13;
}

