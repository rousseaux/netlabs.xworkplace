
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
#include "helpers\apmh.h"               // Advanced Power Management helpers
#include "helpers\comctl.h"             // common controls (window procs)
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\gpih.h"               // GPI helper routines
#include "helpers\prfh.h"               // INI file helper routines;
                                        // this include is required for some
                                        // of the structures in shared\center.h
#include "helpers\standards.h"          // some standard macros
#include "helpers\stringh.h"            // string helper routines
#include "helpers\timer.h"              // replacement PM timers
#include "helpers\winh.h"               // PM helper routines
#include "helpers\xstring.h"            // extended string helpers

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\center.h"              // public XCenter interfaces
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\helppanels.h"          // all XWorkplace help panel IDs

#include "config\drivdlgs.h"            // driver configuration dialogs

#include "filesys\disk.h"               // XFldDisk implementation

#include "hook\xwphook.h"               // hook and daemon definitions

// #include "startshut\apm.h"              // APM power-off for XShutdown

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
#define MWGT_DISKFREE           6       // V0.9.14 (2001-08-01) [umoeller]

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
                },
                {
                    WNDCLASS_WIDGET_MONITORS,
                    MWGT_DISKFREE,
                    "DiskFreeCondensed",
                    "Diskfree (condensed)",
                    WGTF_TOOLTIP | WGTF_TRAYABLE | WGTF_SIZEABLE,
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
PAPMHOPEN papmhOpen = NULL;
PAPMHREADSTATUS papmhReadStatus = NULL;
PAPMHCLOSE papmhClose = NULL;

PCMNQUERYDEFAULTFONT pcmnQueryDefaultFont = NULL;
PCMNQUERYHELPLIBRARY pcmnQueryHelpLibrary = NULL;
PCMNQUERYMAINRESMODULEHANDLE pcmnQueryMainResModuleHandle = NULL;

PCTRFREESETUPVALUE pctrFreeSetupValue = NULL;
PCTRPARSECOLORSTRING pctrParseColorString = NULL;
PCTRSCANSETUPSTRING pctrScanSetupString = NULL;

PDRV_SPRINTF pdrv_sprintf = NULL;

PDSKQUERYINFO pdskQueryInfo = NULL;

PGPIHDRAW3DFRAME pgpihDraw3DFrame = NULL;
PGPIHSWITCHTORGB pgpihSwitchToRGB = NULL;

PKRNQUERYDAEMONOBJECT pkrnQueryDaemonObject = NULL;

PSTRHDATETIME pstrhDateTime = NULL;
PSTRHTHOUSANDSULONG pstrhThousandsULong = NULL;

PTMRSTARTXTIMER ptmrStartXTimer = NULL;
PTMRSTOPXTIMER ptmrStopXTimer = NULL;

PWINHFREE pwinhFree = NULL;
PWINHINSERTMENUITEM pwinhInsertMenuItem = NULL;
PWINHINSERTSUBMENU pwinhInsertSubmenu = NULL;
PWINHQUERYPRESCOLOR pwinhQueryPresColor = NULL;
PWINHQUERYWINDOWFONT pwinhQueryWindowFont = NULL;
PWINHSETWINDOWFONT pwinhSetWindowFont = NULL;

PXSTRCAT pxstrcat = NULL;
PXSTRCATC pxstrcatc = NULL;
PXSTRCLEAR pxstrClear = NULL;
PXSTRINIT pxstrInit = NULL;

RESOLVEFUNCTION G_aImports[] =
    {
        "apmhOpen", (PFN*)&papmhOpen,
        "apmhReadStatus", (PFN*)&papmhReadStatus,
        "apmhClose", (PFN*)&papmhClose,
        "cmnQueryDefaultFont", (PFN*)&pcmnQueryDefaultFont,
        "cmnQueryHelpLibrary", (PFN*)&pcmnQueryHelpLibrary,
        "cmnQueryMainResModuleHandle", (PFN*)&pcmnQueryMainResModuleHandle,
        "ctrFreeSetupValue", (PFN*)&pctrFreeSetupValue,
        "ctrParseColorString", (PFN*)&pctrParseColorString,
        "ctrScanSetupString", (PFN*)&pctrScanSetupString,
        "drv_sprintf", (PFN*)&pdrv_sprintf,
        "dskQueryInfo", (PFN*)&pdskQueryInfo,
        "gpihDraw3DFrame", (PFN*)&pgpihDraw3DFrame,
        "gpihSwitchToRGB", (PFN*)&pgpihSwitchToRGB,
        "krnQueryDaemonObject", (PFN*)&pkrnQueryDaemonObject,
        "strhDateTime", (PFN*)&pstrhDateTime,
        "strhThousandsULong", (PFN*)&pstrhThousandsULong,
        "tmrStartXTimer", (PFN*)&ptmrStartXTimer,
        "tmrStopXTimer", (PFN*)&ptmrStopXTimer,
        "winhFree", (PFN*)&pwinhFree,
        "winhInsertMenuItem", (PFN*)&pwinhInsertMenuItem,
        "winhInsertSubmenu", (PFN*)&pwinhInsertSubmenu,
        "winhQueryPresColor", (PFN*)&pwinhQueryPresColor,
        "winhQueryWindowFont", (PFN*)&pwinhQueryWindowFont,
        "winhSetWindowFont", (PFN*)&pwinhSetWindowFont,
        "xstrcat", (PFN*)&pxstrcat,
        "xstrcatc", (PFN*)&pxstrcatc,
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

    ULONG           cxCurrent,
                    cyCurrent;

    /*
     *  disks to monitor (if MWGT_DISKFREE)
     *
     */

    PSZ             pszDisks;       // array of plain drive letters to monitor

} MONITORSETUP, *PMONITORSETUP;

#define DFFL_REPAINT            0x0001
#define DFFL_FLASH              0x0002
#define DFFL_BACKGROUND         0x0004

#define FLASH_MAX               30
            // count of 100 ms intervals that the flash should
            // last; 20 means 2 seconds then

/*
 *@@ DISKDATA:
 *
 *@@added V0.9.14 (2001-08-03) [umoeller]
 */

typedef struct _DISKDATA
{
    LONG        lKB;            // currently free KBS;
                                // if negative, this is an APIRET from xwpdaemon
    ULONG       fl;             // DFFL_* flags

    ULONG       ulFade;         // fade percentage while (fl & DFFL_FLASH);
                                // initially set to 100 and fading to 0

    LONG        lX;             // cached x position of this drive in PaintDiskfree

} DISKDATA, *PDISKDATA;

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
                // -- MWGT_DISKFREE: condensed diskfree widget
                // this is copied from the widget class on WM_CREATE

    MONITORSETUP    Setup;
            // widget settings that correspond to a setup string

    ULONG           ulTimerID;      // if != NULLHANDLE, update timer is running

    /*
     *  date/time (for MWGT_DATE, MWGT_TIME)
     *
     */

    CHAR            szDateTime[200];

    /*
     *  APM data (if MWGT_POWER)
     *
     */

    APIRET          arcAPM;         // error code if APM.SYS open failed

    PAPM            pApm;           // APM status data

    HPOINTER        hptrAC,         // "AC" icon
                    hptrBattery;    // "battery" icon

    /*
     *  diskfree data (if MWGT_DISKFREE)
     *
     */

    PDISKDATA       paDiskDatas;    // array of 26 DISKDATA structures
                                    // holding temporary information

    /* PLONG           palKBs;          // array of 26 LONG values for the
                                     // disks being monitored; only those
                                     // values are valid for which a corresponding
                                     // drive letter exists in Setup

    PULONG          paulFlags;       // array of 26 ULONGs;
                                     // one ULONG of DFFL_* flags for each disk

    PLONG           palXPoses;       // array of 26 LONG values for the
                                     // X positions of each disk display (speed)
    */

    BOOL            fContextMenuHacked;

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

        if (pSetup->pszDisks)
        {
            free(pSetup->pszDisks);
            pSetup->pszDisks = NULL;
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
                   PMONITORSETUP pSetup,
                   BOOL fIsDiskfree)
{
    PSZ p;
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
    // else: leave this field null

    if (fIsDiskfree)
    {
        // disks to monitor:
        if (p = pctrScanSetupString(pcszSetupString,
                                    "DISKS"))
        {
            pSetup->pszDisks = strdup(p);
            pctrFreeSetupValue(p);
        }
        else
            pSetup->pszDisks = strdup("F");     // @@todo

        // width
        if (p = pctrScanSetupString(pcszSetupString,
                                    "WIDTH"))
        {
            pSetup->cxCurrent = atoi(p);
            pctrFreeSetupValue(p);
        }
        else
            pSetup->cxCurrent = 100;
    }
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
                   PMONITORSETUP pSetup,
                   BOOL fIsDiskfree)
{
    CHAR    szTemp[100];
    // PSZ     psz = 0;
    pxstrInit(pstrSetup, 100);

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

    if (fIsDiskfree)
    {
        pdrv_sprintf(szTemp, "WIDTH=%d;",
                pSetup->cxCurrent);
        pxstrcat(pstrSetup, szTemp, 0);

        pdrv_sprintf(szTemp, "DISKS=%s",
                pSetup->pszDisks);
        pxstrcat(pstrSetup, szTemp, 0);
    }
}

/*
 *@@ MwgtSaveSetupAndSend:
 *
 *@@added V0.9.14 (2001-08-01) [umoeller]
 */

VOID MwgtSaveSetupAndSend(HWND hwnd,
                          PMONITORPRIVATE pPrivate)
{
    XSTRING strSetup;
    MwgtSaveSetup(&strSetup,
                  &pPrivate->Setup,
                  (pPrivate->ulType == MWGT_DISKFREE));
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

/* ******************************************************************
 *
 *   Widget settings dialog
 *
 ********************************************************************/

// None currently.

/* ******************************************************************
 *
 *   Helpers
 *
 ********************************************************************/

/*
 *@@ UpdateDiskMonitors:
 *      notifies XWPDAEMN.EXE of the new disks to
 *      be monitored.
 *
 *@@added V0.9.14 (2001-08-01) [umoeller]
 */

BOOL UpdateDiskMonitors(HWND hwnd,
                        PMONITORPRIVATE pPrivate)
{
    HWND    hwndDaemon;
    PID     pidDaemon;
    TID     tidDaemon;

    ULONG   cDisks;

    if (    (cDisks = strlen(pPrivate->Setup.pszDisks))
         && (hwndDaemon = pkrnQueryDaemonObject())
         && (WinQueryWindowProcess(hwndDaemon,
                                   &pidDaemon,
                                   &tidDaemon))
       )
    {
        PADDDISKWATCH pAddDiskWatch;

        // remove all existing disk watches
        WinSendMsg(hwndDaemon,
                   XDM_REMOVEDISKWATCH,
                   (MPARAM)hwnd,
                   (MPARAM)-1);     // all watches

        if (    (!DosAllocSharedMem((PPVOID)&pAddDiskWatch,
                                    NULL,
                                    sizeof(ADDDISKWATCH),
                                    PAG_COMMIT | OBJ_GIVEABLE | PAG_READ | PAG_WRITE))
             && (!DosGiveSharedMem(pAddDiskWatch,
                                   pidDaemon,
                                   PAG_READ))
            )
        {
            ULONG ul;

            _Pmpf((__FUNCTION__ ": gave 0x%lX to pid 0x%lX", pAddDiskWatch, pidDaemon));

            ZERO(pAddDiskWatch);
            pAddDiskWatch->hwndNotify = hwnd;
            pAddDiskWatch->ulMessage = WM_USER;

            for (ul = 0;
                 ul < cDisks;
                 ul++)
            {
                pAddDiskWatch->ulLogicalDrive = pPrivate->Setup.pszDisks[ul] - 'A' + 1;
                WinSendMsg(hwndDaemon,
                           XDM_ADDDISKWATCH,
                           (MPARAM)pAddDiskWatch,
                           0);
            }

            DosFreeMem(pAddDiskWatch);
        }
    }

    return (FALSE);
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

    // PSZ p;

    PMONITORPRIVATE pPrivate = malloc(sizeof(MONITORPRIVATE));
    memset(pPrivate, 0, sizeof(MONITORPRIVATE));
    // link the two together
    pWidget->pUser = pPrivate;
    pPrivate->pWidget = pWidget;

    // get widget type (clock, memory, ...) from class setting;
    // this is lost after WM_CREATE
    if (pWidget->pWidgetClass)
        pPrivate->ulType = pWidget->pWidgetClass->ulExtra;

    pPrivate->Setup.cxCurrent = 10;          // we'll resize ourselves later
    pPrivate->Setup.cyCurrent = 10;

    // initialize binary setup structure from setup string
    MwgtScanSetup(pWidget->pcszSetupString,
                  &pPrivate->Setup,
                  (pPrivate->ulType == MWGT_DISKFREE));

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

            if (!(pPrivate->arcAPM = papmhOpen(&pPrivate->pApm)))
            {
                if (pPrivate->pApm->usLowestAPMVersion < 0x101)  // version 1.1 or above
                    pPrivate->arcAPM = -1;
                else
                {
                    // no error at all:
                    // read first value, because next update
                    // won't be before 1 minute from now
                    if (!(pPrivate->arcAPM = papmhReadStatus(pPrivate->pApm)))
                    {
                        // and load the icons
                        pPrivate->hptrAC = WinLoadPointer(HWND_DESKTOP,
                                                          pcmnQueryMainResModuleHandle(),
                                                          ID_POWER_AC);
                        pPrivate->hptrBattery = WinLoadPointer(HWND_DESKTOP,
                                                               pcmnQueryMainResModuleHandle(),
                                                               ID_POWER_BATTERY);

                        pPrivate->Setup.cyCurrent = pWidget->pGlobals->cxMiniIcon + 2;
                    }
                }
            }

            if (pPrivate->arcAPM)
                ulUpdateFreq = 0;
        }
        break;

        case MWGT_DISKFREE:
            ulUpdateFreq = 0;           // no timer here

            pWidget->ulHelpPanelID = ID_XSH_WIDGET_DISKFREE_COND;

            // allocate new array of LONGs for the KB values
            pPrivate->paDiskDatas = malloc(sizeof(DISKDATA) * 27);
            memset(pPrivate->paDiskDatas, 0, sizeof(DISKDATA) * 27);

            /* pPrivate->palKBs = malloc(sizeof(LONG) * 27);
            memset(pPrivate->palKBs, 0, sizeof(LONG) * 27);

            pPrivate->paulFlags = malloc(sizeof(ULONG) * 27);
            memset(pPrivate->paulFlags, 0, sizeof(ULONG) * 27);

            pPrivate->palXPoses = malloc(sizeof(LONG) * 27);
            memset(pPrivate->palXPoses, 0, sizeof(LONG) * 27); */

            // tell XWPDaemon about ourselves
            UpdateDiskMonitors(hwnd,
                               pPrivate);
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
    PXCENTERWIDGET pWidget;
    PMONITORPRIVATE pPrivate;

    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (PMONITORPRIVATE)pWidget->pUser)
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
                        pszl->cx = pPrivate->Setup.cxCurrent;
                        pszl->cy = pPrivate->Setup.cyCurrent;
                        brc = TRUE;
                    }
                    break;
                }
            }
            break;

            case ID_XCENTER_TOOLTIP:
                if (usNotifyCode == TTN_NEEDTEXT)
                {
                    PTOOLTIPTEXT pttt = (PTOOLTIPTEXT)mp2;
                    switch (pPrivate->ulType)
                    {
                        case MWGT_DATE:
                        case MWGT_TIME:
                        {
                             PCOUNTRYSETTINGS pCountrySettings
                                 = (PCOUNTRYSETTINGS)pWidget->pGlobals->pCountrySettings;
                             DATETIME dt;
                             DosGetDateTime(&dt);
                             pstrhDateTime(pPrivate->szDateTime,
                                           NULL,
                                           &dt,
                                           pCountrySettings->ulDateFormat,
                                           pCountrySettings->cDateSep,
                                           pCountrySettings->ulTimeFormat,
                                           pCountrySettings->cTimeSep);

                            pttt->pszText = pPrivate->szDateTime;
                        }
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

                        case MWGT_DISKFREE:
                            pttt->pszText = "Free space on disks";
                        break;
                    }

                    pttt->ulFormat = TTFMT_PSZ;
                }
            break;
        } // end switch (usID)
    } // end if (pPrivate)

    return (brc);
}

/*
 *@@ PaintDiskfree:
 *      called from MwgtPaint for diskfree type only.
 *
 *      This is another major mess because this also
 *      handles the flashing for each drive rectangle
 *      individually.
 *
 *@@added V0.9.14 (2001-08-01) [umoeller]
 */

VOID PaintDiskfree(HWND hwnd,
                   PMONITORPRIVATE pPrivate,
                   PCOUNTRYSETTINGS pCountrySettings,
                   PRECTL prclWin,          // exclusive!
                   HPS hps,
                   BOOL fPaintAll)
{
    FONTMETRICS fm;

    LONG    lcolBackground = pPrivate->Setup.lcolBackground;

    // color to be used for hatch on the first flash;
    // we'll calculate a value in between lcolHatchMax
    // and lcolBackground for each flash that comes in...
    // the max color is the inverse of the background
    // color, so we get white for black and vice versa
    // (we use LONGs to avoid overflows below)
    LONG lBackRed = GET_RED(lcolBackground),
         lBackGreen = GET_GREEN(lcolBackground),
         lBackBlue = GET_BLUE(lcolBackground);
    LONG lHatchMaxRed    = 255 - lBackRed,
         lHatchMaxGreen  = 255 - lBackGreen,
         lHatchMaxBlue   = 255 - lBackBlue;

    GpiQueryFontMetrics(hps,
                        sizeof(fm),
                        &fm);

    // make rclWin inclusive
    (prclWin->xRight)--;
    (prclWin->yTop)--;

    GpiIntersectClipRectangle(hps,
                              prclWin);

    if (pPrivate->Setup.pszDisks)
    {
        ULONG cDisks = strlen(pPrivate->Setup.pszDisks),
              ul;
        CHAR szTemp2[50];

        POINTL ptl,
               ptlNow;
        ptl.x = prclWin->xLeft;
        ptl.y =   prclWin->yBottom
                + (   (prclWin->yTop - prclWin->yBottom)
                    // - (fm.lMaxAscender)
                  ) / 2;
        GpiMove(hps, &ptl);

        GpiSetTextAlignment(hps,
                            TA_LEFT,
                            TA_HALF);

        GpiSetColor(hps,
                    pPrivate->Setup.lcolForeground);
        GpiSetBackColor(hps,
                        pPrivate->Setup.lcolBackground);

        for (ul = 0;
             ul < cDisks;
             ul++)
        {
            CHAR c = pPrivate->Setup.pszDisks[ul];
            if ((c > 'A') && (c <= 'Z'))
            {
                ULONG ulOfs = c - 'A' + 1;
                PDISKDATA pDataThis = &pPrivate->paDiskDatas[ulOfs];

                if (    (fPaintAll)
                     || ((pDataThis->fl & DFFL_REPAINT) != 0)
                   )
                {
                    ULONG ulLength;
                    LONG l = pDataThis->lKB;

                    if (l >= 0)
                    {
                        CHAR szTemp3[50];
                        pdrv_sprintf(szTemp2,
                                "%c:%sM ",
                                // drive letter:
                                c,
                                // free KB:
                                pstrhThousandsULong(szTemp3,
                                                    (l + 512) / 1024,
                                                    pCountrySettings->cThousands));
                    }
                    else
                        // error:
                        pdrv_sprintf(szTemp2,
                                "%c: E%d ",
                                // drive letter:
                                c,
                                // error (negative):
                                -l);

                    ulLength = strlen(szTemp2);

                    if (!fPaintAll)
                        GpiSetBackMix(hps, BM_OVERPAINT);

                    if (pDataThis->fl & (DFFL_FLASH | DFFL_BACKGROUND))
                    {
                        POINTL      aptlText[TXTBOX_COUNT];
                        POINTL      ptlRect,
                                    ptlNow2;
                        // flash the thing:
                        GpiQueryTextBox(hps,
                                        ulLength,
                                        szTemp2,
                                        TXTBOX_COUNT,
                                        aptlText);
                        GpiQueryCurrentPosition(hps, &ptlNow2);
                        if (fPaintAll)
                            ptlRect.x = ptlNow2.x - 3;
                        else
                            ptlRect.x = pPrivate->paDiskDatas[ulOfs].lX - 3;
                        ptlRect.y = prclWin->yBottom;

                        GpiMove(hps, &ptlRect);

                        ptlRect.x += aptlText[TXTBOX_TOPRIGHT].x;
                        ptlRect.y = prclWin->yTop;

                        if (pDataThis->fl & DFFL_FLASH)
                        {
                            LONG    lDiffRed,
                                    lDiffGreen,
                                    lDiffBlue;

                            // flash, but not background:

                            // calculate the hatch color
                            // based on the current flash value...
                            // if we're at FLASH_MAX, we'll use
                            // lHatchMax, if we're at 0, we'll
                            // use lcolBackground
                            lDiffRed   =   (lHatchMaxRed - lBackRed)
                                         * pDataThis->ulFade
                                         / FLASH_MAX;
                            lDiffGreen =   (lHatchMaxGreen - lBackGreen)
                                         * pDataThis->ulFade
                                         / FLASH_MAX;
                            lDiffBlue  =   (lHatchMaxBlue - lBackBlue)
                                         * pDataThis->ulFade
                                         / FLASH_MAX;

                            GpiSetColor(hps,
                                        MAKE_RGB(lBackRed + lDiffRed,
                                                 lBackGreen + lDiffGreen,
                                                 lBackBlue + lDiffBlue));

                            GpiSetPattern(hps, PATSYM_DIAG1);
                        }
                        else
                            GpiSetColor(hps,
                                        pPrivate->Setup.lcolBackground);

                        GpiBox(hps,
                               DRO_FILL,
                               &ptlRect,
                               0,
                               0);

                        GpiMove(hps, &ptlNow2);

                        GpiSetColor(hps,
                                    pPrivate->Setup.lcolForeground);

                        if (!fPaintAll)
                            GpiSetBackMix(hps, BM_LEAVEALONE);

                        // unset background flag, if this was set
                        pDataThis->fl &= ~DFFL_BACKGROUND;

                        GpiSetPattern(hps, PATSYM_DEFAULT);
                    }

                    // if we're not painting all,
                    // we must manually set the xpos now
                    if (!fPaintAll)
                    {
                        ptl.x = pDataThis->lX;
                                    // this was stored in a previous run
                        GpiMove(hps,
                                &ptl);
                    }

                    GpiCharString(hps,
                                  ulLength,
                                  szTemp2);

                    // unset repaint flag
                    pDataThis->fl &= ~DFFL_REPAINT;

                    {
                        CHAR c2 = pPrivate->Setup.pszDisks[ul+1];
                        if ((c2 > 'A') && (c2 <= 'Z'))
                        {
                            GpiQueryCurrentPosition(hps,
                                                    &ptlNow);
                            pPrivate->paDiskDatas[c2 - 'A' + 1].lX = ptlNow.x;
                        }
                    }
                }
            }
        }
    }
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
    ULONG       ulExtraWidth = 0;           // for battery icon only

    BOOL        fSharedPaint = TRUE;

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
            ULONG ulMem = 0;
               //    ulLogicalSwapDrive = 8;
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
                pdrv_sprintf(szPaint, "E %d", pPrivate->arcAPM);
            else
            {
                // APM is OK:
                const char *pcsz = NULL;
                switch (pPrivate->pApm->ulBatteryStatus)
                {
                    case 0x00: pcsz = "High"; break;
                    case 0x01: pcsz = "Low"; break;
                    case 0x02: pcsz = "Critical"; break;
                    case 0x03: pcsz = "Charging"; break;

                    default: pdrv_sprintf(szPaint, "No battery");
                }
                if (pcsz)
                {
                    ULONG cxMiniIcon = pPrivate->pWidget->pGlobals->cxMiniIcon;

                    pdrv_sprintf(szPaint, "%d%%", pPrivate->pApm->ulBatteryLife);

                    WinDrawPointer(hps,
                                   0,
                                   (    (rclWin.yTop - rclWin.yBottom)
                                      - cxMiniIcon
                                   ) / 2,
                                   (pPrivate->pApm->ulBatteryStatus == 3)
                                        ? pPrivate->hptrAC
                                        : pPrivate->hptrBattery,
                                   DP_MINI);

                    // add offset for painting text
                    ulExtraWidth = cxMiniIcon;
                }
            }
        break;

        case MWGT_DISKFREE:
            #ifdef DEBUG_XTIMERS
                _Pmpf((__FUNCTION__ ": calling PaintDiskfree"));
            #endif
            PaintDiskfree(hwnd,
                          pPrivate,
                          pCountrySettings,
                          &rclWin,
                          hps,
                          fDrawFrame);        // paint all?
            fSharedPaint = FALSE;
        break;
    }

    // fSharedPaint is true for all except diskfree V0.9.14 (2001-08-01) [umoeller]
    if (fSharedPaint)
    {
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
            pPrivate->Setup.cxCurrent = (aptlText[TXTBOX_TOPRIGHT].x + ulExtraWidth + 2*ulBorder + 4);
            WinPostMsg(WinQueryWindow(hwnd, QW_PARENT),
                       XCM_SETWIDGETSIZE,
                       (MPARAM)hwnd,
                       (MPARAM)pPrivate->Setup.cxCurrent
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
}

/*
 *@@ ForceRepaint:
 *
 *@@added V0.9.14 (2001-08-01) [umoeller]
 */

VOID ForceRepaint(PMONITORPRIVATE pPrivate)
{
    HWND hwnd = pPrivate->pWidget->hwndWidget;
    HPS hps = WinGetPS(hwnd);
    if (hps)
    {
        // _Pmpf((__FUNCTION__ ": calling MwgtPaint"));
        MwgtPaint(hwnd,
                  pPrivate,
                  hps,
                  FALSE);   // text only

        WinReleasePS(hps);
    } // end if (pPrivate)
}

/*
 *@@ MwgtTimer:
 *
 *@@added V0.9.14 (2001-08-01) [umoeller]
 */

VOID MwgtTimer(PXCENTERWIDGET pWidget, MPARAM mp1, MPARAM mp2)
{
    USHORT usTimerID = (USHORT)mp1;

    PMONITORPRIVATE pPrivate;
    if (pPrivate = (PMONITORPRIVATE)pWidget->pUser)
    {
        if (usTimerID == 1)
        {
            #ifdef DEBUG_XTIMERS
            _Pmpf(("  " __FUNCTION__ ": timer 1"));
            #endif

            if (    (pPrivate->ulType == MWGT_POWER)
                 && (pPrivate->pApm)
                 && (!pPrivate->arcAPM)
               )
                pPrivate->arcAPM = papmhReadStatus(pPrivate->pApm);

            ForceRepaint(pPrivate);
        }
        else if ((usTimerID > 2000) && (usTimerID <= 2026))
        {
            // diskfree flag timer for a logical drive:
            // timer id is 2000 + logical drive num
            ULONG ulLogicalDrive = usTimerID - 2000;
            PDISKDATA pThis = &pPrivate->paDiskDatas[ulLogicalDrive];

            if (pThis->ulFade)
            {
                // still fading:
                (pThis->ulFade)--;
                pThis->fl |= (DFFL_REPAINT | DFFL_BACKGROUND);
                if (!(pThis->ulFade))
                {
                    // now zero:
                    pThis->fl &= ~DFFL_FLASH;
                    ptmrStopXTimer(pWidget->pGlobals->pvXTimerSet,
                                   pWidget->hwndWidget,
                                   usTimerID);
                }
            }

            ForceRepaint(pPrivate);
        }
        else
            pWidget->pfnwpDefWidgetProc(pWidget->hwndWidget, WM_TIMER, mp1, mp2);
    }
}

/*
 *@@ UpdateLogicalDrive:
 *
 *@@added V0.9.14 (2001-08-01) [umoeller]
 */

VOID UpdateLogicalDrive(PXCENTERWIDGET pWidget, MPARAM mp1, MPARAM mp2)
{
    if ((ULONG)mp1 < 27)
    {
        PMONITORPRIVATE pPrivate = (PMONITORPRIVATE)pWidget->pUser;
        PDISKDATA       pThis = &pPrivate->paDiskDatas[(ULONG)mp1];

        _Pmpf((__FUNCTION__ ": drive %d", (ULONG)mp1));

        // flash the rectangle
        pThis->fl |= (DFFL_FLASH | DFFL_REPAINT);
        pThis->ulFade = FLASH_MAX;

        pThis->lKB = (LONG)mp2;

        ptmrStartXTimer(pWidget->pGlobals->pvXTimerSet,
                        pWidget->hwndWidget,
                        // timer ID: 2000 + logical drive num
                        2000 + (ULONG)mp1,
                        100);

        ForceRepaint(pPrivate);
    }
}

/*
 *@@ MwgtWindowPosChanged:
 *      implementation for WM_WINDOWPOSCHANGED.
 *
 *@@added V0.9.14 (2001-08-01) [umoeller]
 */

VOID MwgtWindowPosChanged(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    PXCENTERWIDGET pWidget;
    PMONITORPRIVATE pPrivate;
    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (PMONITORPRIVATE)pWidget->pUser)
         && (pPrivate->ulType == MWGT_DISKFREE)
       )
    {
        PSWP pswpNew = (PSWP)mp1,
             pswpOld = pswpNew + 1;
        if (pswpNew->fl & SWP_SIZE)
        {
            // window was resized:
            pPrivate->Setup.cxCurrent = pswpNew->cx;
            MwgtSaveSetupAndSend(hwnd,
                                 pPrivate);

            WinInvalidateRect(hwnd, NULL, FALSE);
        } // end if (pswpNew->fl & SWP_SIZE)
    } // end if (pPrivate)
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

            WinInvalidateRect(hwnd, NULL, FALSE);

            MwgtSaveSetupAndSend(hwnd, pPrivate);
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
 *@@ HackContextMenu:
 *
 *@@added V0.9.14 (2001-08-01) [umoeller]
 */

VOID HackContextMenu(PMONITORPRIVATE pPrivate)
{
    XDISKINFO aDiskInfos[26];

    HPOINTER hptrOld = WinQueryPointer(HWND_DESKTOP);
    WinSetPointer(HWND_DESKTOP,
                  WinQuerySysPointer(HWND_DESKTOP,
                                     SPTR_WAIT,
                                     FALSE));   // no copy

    // ask xwpdaemn.exe about available drives
    // (this call can take a while)

    if (pdskQueryInfo(aDiskInfos,
                      -1))           // all disks
    {
        // insert the "Drives" submenu after "Properties"

        HWND hwndSubmenu;
        SHORT s = (SHORT)WinSendMsg(pPrivate->pWidget->hwndContextMenu,
                                    MM_ITEMPOSITIONFROMID,
                                    MPFROM2SHORT(ID_CRMI_PROPERTIES,
                                                 FALSE),
                                    0);
        if (hwndSubmenu = pwinhInsertSubmenu(pPrivate->pWidget->hwndContextMenu,
                                             s + 2,
                                             1999,
                                             "Drives",
                                             MIS_TEXT,
                                             0, NULL, 0, 0))
        {
            ULONG ul;
            for (ul = 0;
                 ul < 26;
                 ul++)
            {
                PXDISKINFO pThis = &aDiskInfos[ul];
                if (pThis->flType & DFL_FIXED)
                {
                    // fixed drive:
                    CHAR szText[10];
                    ULONG afAttr = 0;
                    pdrv_sprintf(szText, "%c:", pThis->cDriveLetter);
                    if (    (pPrivate->Setup.pszDisks)
                         && (strchr(pPrivate->Setup.pszDisks, pThis->cDriveLetter))
                       )
                        afAttr |= MIA_CHECKED;

                    pwinhInsertMenuItem(hwndSubmenu,
                                        MIT_END,
                                        2000 + pThis->cLogicalDrive,
                                        szText,
                                        MIS_TEXT,
                                        afAttr);
                }
            }

            pwinhInsertMenuItem(pPrivate->pWidget->hwndContextMenu,
                                s + 3,
                                0,
                                "",
                                MIS_SEPARATOR,
                                0);

            pPrivate->fContextMenuHacked = TRUE;
        }
    }

    WinSetPointer(HWND_DESKTOP, hptrOld);
}

/*
 *@@ MwgtContextMenu:
 *      implementation for WM_CONTEXTMENU.
 *
 *      We override the default behavior of the standard
 *      widget window proc for the diskfree widget.
 *
 *@@added V0.9.8 (2001-01-10) [umoeller]
 */

MRESULT MwgtContextMenu(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    PXCENTERWIDGET pWidget;
    PMONITORPRIVATE pPrivate;
    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (PMONITORPRIVATE)pWidget->pUser)
       )
    {
        if (    (pPrivate->ulType == MWGT_DISKFREE)
             && (pWidget->hwndContextMenu)
             && (!pPrivate->fContextMenuHacked)
           )
        {
            // first call for diskfree:
            // hack the context menu given to us
            HackContextMenu(pPrivate);
        }

        return (pWidget->pfnwpDefWidgetProc(hwnd, WM_CONTEXTMENU, mp1, mp2));
    }

    return 0;
}

/*
 *@@ MwgtMenuSelect:
 *
 *@@added V0.9.14 (2001-08-01) [umoeller]
 */

MRESULT MwgtMenuSelect(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    PXCENTERWIDGET pWidget;
    PMONITORPRIVATE pPrivate;
    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (PMONITORPRIVATE)pWidget->pUser)
       )
    {
        USHORT usItem = SHORT1FROMMP(mp1);
        if (    (pPrivate->ulType == MWGT_DISKFREE)
                // one of the drive letter items?
             && (usItem >= 2000)
             && (usItem <= 2026)
           )
        {
            if (SHORT2FROMMP(mp1) == TRUE)      // WM_COMMAND to be posted:
            {
                ULONG ulLogicalDrive = usItem - 2000;

                // recompose the string of drive letters to
                // be monitored...
                CHAR szNew[30],
                     szNew2[30];
                PSZ pTarget = szNew2;
                ULONG ul;
                BOOL fInList = FALSE;

                memset(szNew, 0, sizeof(szNew));

                if (pPrivate->Setup.pszDisks)
                {
                    for (ul = 1;
                         ul < 27;
                         ul++)
                    {
                        if (strchr(pPrivate->Setup.pszDisks,
                                   ul + 'A' - 1))
                        {
                            if (ul == ulLogicalDrive)
                                fInList = TRUE;
                                // drop it
                            else
                                szNew[ul] = 1;
                        }
                    }

                    free(pPrivate->Setup.pszDisks);
                    pPrivate->Setup.pszDisks = NULL;
                }

                if (!fInList)
                    szNew[ulLogicalDrive] = 1;

                for (ul = 1;
                     ul < 27;
                     ul++)
                {
                    if (szNew[ul])
                        *pTarget++ = ul + 'A' - 1;
                }
                *pTarget = '\0';

                if (szNew2[0])
                    pPrivate->Setup.pszDisks = strdup(szNew2);

                WinCheckMenuItem((HWND)mp2,
                                 usItem,
                                 !fInList);

                UpdateDiskMonitors(hwnd,
                                   pPrivate);

                MwgtSaveSetupAndSend(hwnd, pPrivate);

                // full repaint
                WinInvalidateRect(hwnd, NULL, FALSE);

                return (MRESULT)FALSE;
            }
        }

        return (pWidget->pfnwpDefWidgetProc(hwnd, WM_MENUSELECT, mp1, mp2));
    }

    return 0;
}

/*
 *@@ MwgtDestroy:
 *
 *@@added V0.9.12 (2001-05-26) [umoeller]
 *@@changed V0.9.14 (2001-08-01) [umoeller]: fixed memory leak
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

        if (pPrivate->pApm)
            papmhClose(&pPrivate->pApm);

        if (pPrivate->hptrAC)
            WinDestroyPointer(pPrivate->hptrAC);
        if (pPrivate->hptrBattery)
            WinDestroyPointer(pPrivate->hptrBattery);

        MwgtFreeSetup(&pPrivate->Setup);        // V0.9.14 (2001-08-01) [umoeller]

        if (pPrivate->paDiskDatas)
            free(pPrivate->paDiskDatas);

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
 *@@changed V0.9.14 (2001-08-01) [umoeller]: added diskfree widget
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
            MwgtTimer(pWidget, mp1, mp2);
        break;

        /*
         * WM_WINDOWPOSCHANGED:
         *      on window resize, allocate new bitmap.
         */

        case WM_WINDOWPOSCHANGED:
            MwgtWindowPosChanged(hwnd, mp1, mp2);
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
         *@@ WM_BUTTON1DBLCLK:
         *      on double-click on clock, open
         *      system clock settings.
         */

        case WM_BUTTON1DBLCLK:
            MwgtButton1DblClick(hwnd, pWidget);
            mrc = (MPARAM)TRUE;     // message processed
        break;

        /*
         * WM_CONTEXTMENU:
         *      modify standard context menu behavior.
         */

        case WM_CONTEXTMENU:
            mrc = MwgtContextMenu(hwnd, mp1, mp2);
        break;

        /*
         * WM_MENUSELECT:
         *      intercept items for diskfree widget
         *      and do not dismiss.
         */

        case WM_MENUSELECT:
            mrc = MwgtMenuSelect(hwnd, mp1, mp2);
        break;

        /*
         * WM_USER:
         *      this message value is given to XWPDaemon
         *      if we're operating in diskfree mode and
         *      thus gets posted from xwpdaemon when a
         *      diskfree value changes.
         *
         *      Parameters:
         *
         *      --  ULONG mp1: ulLogicalDrive which changed.
         *
         *      --  ULONG mp2: new free space on the disk in KB.
         */

        case WM_USER:
            UpdateLogicalDrive(pWidget, mp1, mp2);
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
    *pulMajor = XFOLDER_MAJOR;              // dlgids.h
    *pulMinor = XFOLDER_MINOR;
    *pulRevision = XFOLDER_REVISION;
}

