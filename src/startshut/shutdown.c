
/*
 *@@sourcefile shutdwn.c:
 *      this file contains all the XShutdown code, which
 *      was in xfdesk.c before V0.84.
 *
 *      XShutdown is a (hopefully) complete rewrite of what
 *      WinShutdownSystem does. This code is also used for
 *      "Restart Desktop", since it does in part the same thing.
 *
 *      This file implements (in this order):
 *
 *      --  Shutdown dialogs. See xsdConfirmShutdown and
 *          xsdConfirmRestartWPS.
 *
 *      --  Shutdown interface. See xsdInitiateShutdown,
 *          xsdInitiateRestartWPS, and xsdInitiateShutdownExt.
 *
 *      --  Shutdown settings notebook pages. See xsdShutdownInitPage
 *          and below.
 *
 *      --  The Shutdown thread itself, which does the grunt
 *          work of shutting down the system, together with
 *          the Update thread, which monitors the window list
 *          while shutting down. See fntShutdownThread and
 *          fntUpdateThread.
 *
 *      All the functions in this file have the xsd* prefix.
 *
 *@@header "startshut\shutdown.h"
 */

/*
 *      Copyright (C) 1997-2000 Ulrich M”ller.
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
#define INCL_DOSSESMGR
#define INCL_DOSSEMAPHORES
#define INCL_DOSEXCEPTIONS
#define INCL_DOSDEVICES
#define INCL_DOSDEVIOCTL
#define INCL_DOSERRORS

#define INCL_WINWINDOWMGR
#define INCL_WINMESSAGEMGR
#define INCL_WINFRAMEMGR
#define INCL_WINDIALOGS
#define INCL_WINPOINTERS
#define INCL_WINSHELLDATA
#define INCL_WINPROGRAMLIST
#define INCL_WINSWITCHLIST
#define INCL_WINCOUNTRY
#define INCL_WINSYS
#define INCL_WINMENUS
#define INCL_WINSTATICS
#define INCL_WINENTRYFIELDS
#define INCL_WINBUTTONS
#define INCL_WINLISTBOXES
#include <os2.h>

// C library headers
#include <stdio.h>              // needed for except.h
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h
#include <stdarg.h>

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\animate.h"            // icon and other animations
#include "helpers\comctl.h"             // common controls (window procs)
#include "helpers\dialog.h"             // dialog helpers
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\exeh.h"               // executable helpers
#include "helpers\gpih.h"               // GPI helper routines
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\procstat.h"           // DosQProcStat handling
#include "helpers\standards.h"          // some standard macros
#include "helpers\threads.h"            // thread helpers
#include "helpers\winh.h"               // PM helper routines
#include "helpers\wphandle.h"           // file-system object handles
#include "helpers\xprf.h"               // replacement profile (INI) functions

// SOM headers which don't crash with prec. header files
#include "xfldr.ih"                     // needed for shutdown folder

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\helppanels.h"          // all XWorkplace help panel IDs
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling
#include "xwpapi.h"                     // public XWorkplace definitions

// headers in /hook
#include "hook\xwphook.h"

#include "filesys\fdrmenus.h"           // shared folder menu logic
#include "filesys\object.h"             // XFldObject implementation
#include "filesys\xthreads.h"           // extra XWorkplace threads

#include "media\media.h"                // XWorkplace multimedia support

#include "security\xwpsecty.h"          // XWorkplace Security

#include "startshut\apm.h"              // APM power-off for XShutdown
#include "startshut\shutdown.h"         // XWorkplace eXtended Shutdown

// other SOM headers
#pragma hdrstop
#include <wpdesk.h>                     // WPDesktop; includes WPFolder also
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

// shutdown animation
static SHUTDOWNANIM        G_sdAnim;

static THREADINFO          G_tiShutdownThread = {0},
                           G_tiUpdateThread = {0};

static ULONG               G_fShutdownRunning = FALSE;
            // moved this here from KERNELGLOBALS
            // V0.9.16 (2001-11-22) [umoeller]

// forward declarations
MRESULT EXPENTRY fnwpShutdownThread(HWND hwndFrame, ULONG msg, MPARAM mp1, MPARAM mp2);
void _Optlink fntUpdateThread(PTHREADINFO pti);

/* ******************************************************************
 *
 *   XShutdown dialogs
 *
 ********************************************************************/

BOOL    G_fConfirmWindowExtended = TRUE;
BOOL    G_fConfirmDialogReady = FALSE;
ULONG   G_ulConfirmHelpPanel = NULLHANDLE;

/*
 *@@ ReformatConfirmWindow:
 *      depending on fExtended, the shutdown confirmation
 *      dialog is extended to show the "reboot to" listbox
 *      or not.
 *
 *@@added V0.9.0 [umoeller]
 *@@changed V0.9.1 (2000-01-20) [umoeller]: reformat wasn't working right; fixed.
 */

static VOID ReformatConfirmWindow(HWND hwndDlg,        // in: confirmation dlg window
                                  BOOL fExtended)      // in: if TRUE, the list box will be shown
{
    // _Pmpf(("ReformatConfirmWindow: %d, ready: %d", fExtended, G_fConfirmDialogReady));

    if (G_fConfirmDialogReady)
        if (fExtended != G_fConfirmWindowExtended)
        {
            HWND    hwndBootMgrListbox = WinWindowFromID(hwndDlg, ID_SDDI_BOOTMGR);
            SWP     swpBootMgrListbox;
            SWP     swpDlg;

            WinQueryWindowPos(hwndBootMgrListbox, &swpBootMgrListbox);
            WinQueryWindowPos(hwndDlg, &swpDlg);

            if (fExtended)
                swpDlg.cx += swpBootMgrListbox.cx;
            else
                swpDlg.cx -= swpBootMgrListbox.cx;

            WinShowWindow(hwndBootMgrListbox, fExtended);
            WinSetWindowPos(hwndDlg,
                            NULLHANDLE,
                            0, 0,
                            swpDlg.cx, swpDlg.cy,
                            SWP_SIZE);

            G_fConfirmWindowExtended = fExtended;
        }
}

/*
 * fnwpConfirm:
 *      dlg proc for XShutdown confirmation windows.
 *
 *@@changed V0.9.0 [umoeller]: redesigned the whole confirmation window.
 *@@changed V0.9.1 (2000-01-20) [umoeller]: reformat wasn't working right; fixed.
 */

static MRESULT EXPENTRY fnwpConfirm(HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = MPNULL;
    switch (msg)
    {
        case WM_CONTROL:
            /* _Pmpf(("WM_CONTROL %d: fConfirmWindowExtended: %d",
                    SHORT1FROMMP(mp1), fConfirmWindowExtended)); */

            if (    (SHORT2FROMMP(mp1) == BN_CLICKED)
                 || (SHORT2FROMMP(mp1) == BN_DBLCLICKED)
               )
                switch (SHORT1FROMMP(mp1)) // usItemID
                {
                    case ID_SDDI_SHUTDOWNONLY:
                    case ID_SDDI_STANDARDREBOOT:
                        ReformatConfirmWindow(hwndDlg, FALSE);
                    break;

                    case ID_SDDI_REBOOTTO:
                        ReformatConfirmWindow(hwndDlg, TRUE);
                    break;
                }

            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
        break;

        case WM_HELP:
            cmnDisplayHelp(NULL,
                           G_ulConfirmHelpPanel);
        break;

        default:
            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
    }
    return (mrc);
}

PFNWP G_pfnwpFrameOrig = NULL;
PXBITMAP G_pbmDim = NULLHANDLE;

/*
 *@@ fnwpDimScreen:
 *
 *@@added V0.9.16 (2002-01-04) [umoeller]
 */

static MRESULT EXPENTRY fnwpDimScreen(HWND hwndFrame, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    switch (msg)
    {
        case WM_PAINT:
        {
            HPS hps = WinBeginPaint(hwndFrame, NULLHANDLE, NULL);
            POINTL ptl = {0,0};
            if (G_pbmDim)
                WinDrawBitmap(hps,
                              G_pbmDim->hbm,
                              NULL,
                              &ptl,
                              0,
                              0,
                              DBM_NORMAL); // DBM_HALFTONE);
            WinEndPaint(hps);
        }
        break;

        case WM_DESTROY:
            gpihDestroyXBitmap(&G_pbmDim);
            mrc = G_pfnwpFrameOrig(hwndFrame, msg, mp1, mp2);
        break;

        default:
            mrc = G_pfnwpFrameOrig(hwndFrame, msg, mp1, mp2);
    }

    return (mrc);
}

/*
 *@@ CreateDimScreenWindow:
 *
 *@@added V0.9.16 (2002-01-04) [umoeller]
 */

static HWND CreateDimScreenWindow(VOID)
{
    HWND hwnd;

    FRAMECDATA  fcdata;

    HPS hpsScreen;
    RECTL rcl;
    HAB hab = winhMyAnchorBlock();      // expensive, but what the heck

    // before capturing the screen, sleep a millisecond,
    // because otherwise windows are not fully repainted
    // if the desktop menu was open, and we get that
    // into the screenshot then... so give the windows
    // time to repaint
    winhSleep(10);

    // go capture screen
    hpsScreen = WinGetScreenPS(HWND_DESKTOP);
    rcl.xLeft = 0;
    rcl.yBottom = 0;
    rcl.xRight = winhQueryScreenCX();
    rcl.yTop = winhQueryScreenCY();
    if (G_pbmDim = gpihCreateBmpFromPS(hab,
                                       hpsScreen,
                                       &rcl))
    {
        POINTL  ptl = {0, 0};
        GpiMove(G_pbmDim->hpsMem, &ptl);  // still 0, 0
        ptl.x = rcl.xRight - 1;
        ptl.y = rcl.yTop - 1;
        GpiSetColor(G_pbmDim->hpsMem, RGBCOL_BLACK);
        GpiSetPattern(G_pbmDim->hpsMem, PATSYM_HALFTONE);
        GpiBox(G_pbmDim->hpsMem,
               DRO_FILL, // interior only
               &ptl,
               0, 0);    // no corner rounding
    }
    WinReleasePS(hpsScreen);

    fcdata.cb            = sizeof(FRAMECDATA);
    fcdata.flCreateFlags = FCF_NOBYTEALIGN;
    fcdata.hmodResources = NULLHANDLE;
    fcdata.idResources   = 0;

    hwnd = WinCreateWindow(HWND_DESKTOP,
                           WC_FRAME,
                           "",
                           0,       // flags
                           0,
                           0,
                           winhQueryScreenCX(),
                           winhQueryScreenCY(),
                           NULLHANDLE,
                           HWND_TOP,
                           0,
                           &fcdata,
                           NULL);

    G_pfnwpFrameOrig = WinSubclassWindow(hwnd, fnwpDimScreen);

    WinShowWindow(hwnd, TRUE);

    return (hwnd);
}

/*
 *@@ xsdConfirmShutdown:
 *      this displays the eXtended Shutdown (not Restart Desktop)
 *      confirmation box. Returns MBID_YES/NO.
 *
 *@@changed V0.9.0 [umoeller]: redesigned the whole confirmation window.
 *@@changed V0.9.1 (2000-01-20) [umoeller]: reformat wasn't working right; fixed.
 *@@changed V0.9.1 (2000-01-30) [umoeller]: added exception handling.
 *@@changed V0.9.4 (2000-08-03) [umoeller]: added "empty trash can"
 *@@changed V0.9.7 (2000-12-13) [umoeller]: global settings weren't unlocked, fixed
 */

ULONG xsdConfirmShutdown(PSHUTDOWNPARAMS psdParms)
{
    ULONG       ulReturn = MBID_NO;
    BOOL        fStore = FALSE;
    HWND        hwndConfirm = NULLHANDLE,
                hwndDim;

    TRY_LOUD(excpt1)
    {
        // BOOL        fCanEmptyTrashCan = FALSE;
        HMODULE     hmodResource = cmnQueryNLSModuleHandle(FALSE);
        ULONG       ulKeyLength;
        PSZ         p = NULL,
                    pINI = NULL;
        ULONG       ulCheckRadioButtonID = ID_SDDI_SHUTDOWNONLY;

        HPOINTER    hptrShutdown = WinLoadPointer(HWND_DESKTOP, hmodResource,
                                                  ID_SDICON);

        hwndDim = CreateDimScreenWindow();

        G_fConfirmWindowExtended = TRUE;
        G_fConfirmDialogReady = FALSE;
        G_ulConfirmHelpPanel = ID_XSH_XSHUTDOWN_CONFIRM;
        hwndConfirm = WinLoadDlg(HWND_DESKTOP,
                                 hwndDim,
                                 fnwpConfirm,
                                 hmodResource,
                                 ID_SDD_CONFIRM,
                                 NULL);

        WinPostMsg(hwndConfirm,
                   WM_SETICON,
                   (MPARAM)hptrShutdown,
                    NULL);

        // set radio buttons
#ifndef __NOXSHUTDOWN__
        winhSetDlgItemChecked(hwndConfirm, ID_SDDI_MESSAGEAGAIN, psdParms->optConfirm);
#endif

        if (cmnTrashCanReady())
        {
            if (psdParms->optEmptyTrashCan)
                winhSetDlgItemChecked(hwndConfirm, ID_SDDI_EMPTYTRASHCAN, TRUE);
        }
        else
            // trash can not ready: disable item
            winhEnableDlgItem(hwndConfirm, ID_SDDI_EMPTYTRASHCAN, FALSE);

        if (psdParms->optReboot)
            ulCheckRadioButtonID = ID_SDDI_STANDARDREBOOT;

        // insert ext reboot items into combo box;
        // check for reboot items in OS2.INI
        if (PrfQueryProfileSize(HINI_USER,
                                (PSZ)INIAPP_XWORKPLACE,
                                (PSZ)INIKEY_BOOTMGR,
                                &ulKeyLength))
        {
            // PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
            // items exist: evaluate
            if (pINI = malloc(ulKeyLength))
            {
                PrfQueryProfileData(HINI_USER,
                                    (PSZ)INIAPP_XWORKPLACE,
                                    (PSZ)INIKEY_BOOTMGR,
                                    pINI,
                                    &ulKeyLength);
                p = pINI;
                while (strlen(p))
                {
                    WinSendDlgItemMsg(hwndConfirm, ID_SDDI_BOOTMGR,
                                      LM_INSERTITEM,
                                      (MPARAM)LIT_END,
                                      (MPARAM)p);
                     // skip description string
                    p += (strlen(p)+1);
                    // skip reboot command
                    p += (strlen(p)+1);
                }
            }

            // select reboot item from last time
            if (cmnQuerySetting(susLastRebootExt) != 0xFFFF)
            {
                if (WinSendDlgItemMsg(hwndConfirm, ID_SDDI_BOOTMGR,
                                      LM_SELECTITEM,
                                      (MPARAM)cmnQuerySetting(susLastRebootExt), // item index
                                      (MPARAM)TRUE) // select (not deselect)
                            == (MRESULT)FALSE)
                    // error:
                    // check first item then
                    WinSendDlgItemMsg(hwndConfirm, ID_SDDI_BOOTMGR,
                                      LM_SELECTITEM,
                                      (MPARAM)0,
                                      (MPARAM)TRUE); // select (not deselect)

                if (ulCheckRadioButtonID == ID_SDDI_STANDARDREBOOT)
                    ulCheckRadioButtonID = ID_SDDI_REBOOTTO;
            }
        }
        else
            // no items found: disable
            winhEnableDlgItem(hwndConfirm, ID_SDDI_REBOOTTO, FALSE);

        // check radio button
        winhSetDlgItemChecked(hwndConfirm, ulCheckRadioButtonID, TRUE);
        winhSetDlgItemFocus(hwndConfirm, ulCheckRadioButtonID);

        // make window smaller if we don't have "reboot to"
        G_fConfirmDialogReady = TRUE;       // flag for ReformatConfirmWindow
        if (ulCheckRadioButtonID != ID_SDDI_REBOOTTO)
            ReformatConfirmWindow(hwndConfirm, FALSE);

        cmnSetControlsFont(hwndConfirm, 1, 5000);
        winhCenterWindow(hwndConfirm);      // still hidden

        xsdLoadAnimation(&G_sdAnim);
        ctlPrepareAnimation(WinWindowFromID(hwndConfirm, ID_SDDI_ICON),
                            XSD_ANIM_COUNT,
                            &(G_sdAnim.ahptr[0]),
                            150,    // delay
                            TRUE);  // start now

        // go!!
        ulReturn = WinProcessDlg(hwndConfirm);

        ctlStopAnimation(WinWindowFromID(hwndConfirm, ID_SDDI_ICON));
        xsdFreeAnimation(&G_sdAnim);

        if (ulReturn == DID_OK)
        {
#ifndef __NOXSHUTDOWN__
            ULONG flShutdown = cmnQuerySetting(sflXShutdown);

            // check "show this msg again"
            if (!(winhIsDlgItemChecked(hwndConfirm, ID_SDDI_MESSAGEAGAIN)))
                flShutdown |= XSD_NOCONFIRM;
#endif

            // check empty trash
            psdParms->optEmptyTrashCan
                = (winhIsDlgItemChecked(hwndConfirm, ID_SDDI_EMPTYTRASHCAN) != 0);

            // check reboot options
            psdParms->optReboot = FALSE;
            if (winhIsDlgItemChecked(hwndConfirm, ID_SDDI_REBOOTTO))
            {
                USHORT usSelected = (USHORT)WinSendDlgItemMsg(hwndConfirm, ID_SDDI_BOOTMGR,
                                                              LM_QUERYSELECTION,
                                                              (MPARAM)LIT_CURSOR,
                                                              MPNULL);
                USHORT us;
                psdParms->optReboot = TRUE;

                p = pINI;
                for (us = 0; us < usSelected; us++)
                {
                    // skip description string
                    p += (strlen(p)+1);
                    // skip reboot command
                    p += (strlen(p)+1);
                }
                // skip description string to get to reboot command
                p += (strlen(p)+1);
                strcpy(psdParms->szRebootCommand, p);

#ifndef __NOXSHUTDOWN__
                flShutdown |= XSD_REBOOT;
#endif
                cmnSetSetting(susLastRebootExt, usSelected);
            }
            else if (winhIsDlgItemChecked(hwndConfirm, ID_SDDI_STANDARDREBOOT))
            {
                psdParms->optReboot = TRUE;
                // szRebootCommand is a zero-byte only, which will lead to
                // the standard reboot in the Shutdown thread
#ifndef __NOXSHUTDOWN__
                flShutdown |= XSD_REBOOT;
#endif
                cmnSetSetting(susLastRebootExt, 0xFFFF);
            }
#ifndef __NOXSHUTDOWN__
            else
                // standard shutdown:
                flShutdown &= ~XSD_REBOOT;
#endif

#ifndef __NOXSHUTDOWN__
            cmnSetSetting(sflXShutdown, flShutdown);
#endif
        }

        if (pINI)
            free(pINI);
    }
    CATCH(excpt1)
    {
    } END_CATCH();

    if (hwndConfirm)
        WinDestroyWindow(hwndConfirm);
    WinDestroyWindow(hwndDim);


    return (ulReturn);
}

/*
DLGTEMPLATE ID_SDD_CONFIRMWPS LOADONCALL MOVEABLE DISCARDABLE
BEGIN
    DIALOG  "Restart Desktop", ID_SDD_CONFIRMWPS, 37, 24, 178, 78, ,
            FCF_SYSMENU | FCF_TITLEBAR
    BEGIN
        ICON            ID_SDICON, ID_SDDI_ICON, 10, 53, 20, 16, WS_GROUP
        LTEXT           "Are you sure you wish to restart the Workplace Shel"
                        "l?", ID_SDDI_CONFIRM_TEXT, 35, 47, 105, 20,
                        DT_WORDBREAK
                        PRESPARAMS PP_FONTNAMESIZE, "9.WarpSans"
        DEFPUSHBUTTON   "~Yes", DID_OK, 10, 33, 50, 12
                        PRESPARAMS PP_FONTNAMESIZE, "9.WarpSans"
        PUSHBUTTON      "~No", DID_CANCEL, 65, 33, 50, 12
                        PRESPARAMS PP_FONTNAMESIZE, "9.WarpSans"
        PUSHBUTTON      "~Help", -1, 120, 33, 50, 12, BS_HELP
                        PRESPARAMS PP_FONTNAMESIZE, "9.WarpSans"
        AUTOCHECKBOX    "~Close all sessions", ID_SDDI_WPS_CLOSEWINDOWS, 10,
                        21, 160, 10
                        PRESPARAMS PP_FONTNAMESIZE, "9.WarpSans"
        AUTOCHECKBOX    "~Process all XWorkplace Startup folders",
                        ID_SDDI_WPS_STARTUPFOLDER, 10, 12, 155, 10
                        PRESPARAMS PP_FONTNAMESIZE, "9.WarpSans"
        AUTOCHECKBOX    "~Show this message next time", ID_SDDI_MESSAGEAGAIN,
                        10, 3, 160, 10
                        PRESPARAMS PP_FONTNAMESIZE, "9.WarpSans"
    END
END
*/


static CONTROLDEF
    TrafficLightIcon = CONTROLDEF_ICON(0, ID_SDDI_ICON),
    ConfirmText = CONTROLDEF_TEXT_WORDBREAK(
                            LOAD_STRING,
                            ID_SDDI_CONFIRM_TEXT,
                            200),
    CloseAllSessionsCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,
                            ID_SDDI_WPS_CLOSEWINDOWS,
                            -1,
                            -1),
#ifndef __NOXWPSTARTUP__
    StartupFoldersCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,
                            ID_SDDI_WPS_STARTUPFOLDER,
                            -1,
                            -1),
#endif
#ifndef __NOXSHUTDOWN__
    MessageAgainCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,
                            ID_SDDI_MESSAGEAGAIN,
                            -1,
                            -1),
#endif
    OKButton = CONTROLDEF_DEFPUSHBUTTON(
                            0,
                            DID_OK,
                            100,
                            30),
    CancelButton = CONTROLDEF_PUSHBUTTON(
                            0,
                            DID_CANCEL,
                            100,
                            30),
    HelpButton = CONTROLDEF_HELPPUSHBUTTON(
                            LOAD_STRING,
                            DID_HELP,
                            100,
                            30);

static const DLGHITEM dlgConfirmRestartDesktop[] =
    {
        START_TABLE,            // root table, required
            START_ROW(0),
                START_TABLE,            // root table, required
                    START_ROW(ROW_VALIGN_CENTER),       // shared settings group
                        CONTROL_DEF(&TrafficLightIcon),
                        CONTROL_DEF(&ConfirmText),
                END_TABLE,
            START_ROW(0),
                CONTROL_DEF(&CloseAllSessionsCB),
#ifndef __NOXWPSTARTUP__
            START_ROW(0),
                CONTROL_DEF(&StartupFoldersCB),
#endif
#ifndef __NOXSHUTDOWN__
            START_ROW(0),
                CONTROL_DEF(&MessageAgainCB),
#endif
            START_ROW(ROW_VALIGN_CENTER),
                CONTROL_DEF(&OKButton),
                CONTROL_DEF(&CancelButton),
                CONTROL_DEF(&HelpButton),
        END_TABLE
    };

/*
 *@@ xsdConfirmRestartWPS:
 *      this displays the "Restart Desktop"
 *      confirmation box. Returns MBID_OK/CANCEL.
 *
 *@@changed V0.9.5 (2000-08-10) [umoeller]: added XWPSHELL.EXE interface
 *@@changed V0.9.16 (2002-01-13) [umoeller]: rewritten to use dialog formatter
 */

ULONG xsdConfirmRestartWPS(PSHUTDOWNPARAMS psdParms)
{
    ULONG       ulReturn = MBID_CANCEL;
    HWND        hwndConfirm;
    HMODULE     hmodResource = cmnQueryNLSModuleHandle(FALSE);

    HPOINTER    hptrShutdown = WinLoadPointer(HWND_DESKTOP,
                                              hmodResource,
                                              ID_SDICON);

    HWND        hwndDim = CreateDimScreenWindow();

    G_ulConfirmHelpPanel = ID_XMH_RESTARTWPS;

    cmnLoadDialogStrings(dlgConfirmRestartDesktop,
                         ARRAYITEMCOUNT(dlgConfirmRestartDesktop));

    OKButton.pcszText = cmnGetString(ID_XSSI_DLG_OK);
    CancelButton.pcszText = cmnGetString(ID_XSSI_DLG_CANCEL);

    if (!dlghCreateDlg(&hwndConfirm,
                       hwndDim,
                       FCF_TITLEBAR | FCF_SYSMENU | FCF_DLGBORDER | FCF_NOBYTEALIGN,
                       fnwpConfirm,
                       cmnGetString(ID_SDDI_CONFIRMWPS_TITLE),
                       dlgConfirmRestartDesktop,
                       ARRAYITEMCOUNT(dlgConfirmRestartDesktop),
                       NULL,
                       cmnQueryDefaultFont()))
    {
        /* hwndConfirm = WinLoadDlg(HWND_DESKTOP,
                                 hwndDim,
                                 fnwpConfirm,
                                 hmodResource,
                                 ID_SDD_CONFIRMWPS,
                                 NULL); */

        WinSendMsg(hwndConfirm,
                   WM_SETICON,
                   (MPARAM)hptrShutdown,
                   NULL);

        if (psdParms->ulRestartWPS == 2)
        {
            // logoff:
            psdParms->optWPSCloseWindows = TRUE;
            psdParms->optWPSReuseStartupFolder = TRUE;
            winhEnableDlgItem(hwndConfirm, ID_SDDI_WPS_CLOSEWINDOWS, FALSE);
#ifndef __NOXSHUTDOWN__
            winhEnableDlgItem(hwndConfirm, ID_SDDI_WPS_STARTUPFOLDER, FALSE);
#endif
            // replace confirmation text
            WinSetDlgItemText(hwndConfirm, ID_SDDI_CONFIRM_TEXT,
                              cmnGetString(ID_XSSI_XSD_CONFIRMLOGOFFMSG)) ; // pszXSDConfirmLogoffMsg
        }

        winhSetDlgItemChecked(hwndConfirm, ID_SDDI_WPS_CLOSEWINDOWS, psdParms->optWPSCloseWindows);
#ifndef __NOXWPSTARTUP__
        winhSetDlgItemChecked(hwndConfirm, ID_SDDI_WPS_STARTUPFOLDER, psdParms->optWPSCloseWindows);
#endif
#ifndef __NOXSHUTDOWN__
        winhSetDlgItemChecked(hwndConfirm, ID_SDDI_MESSAGEAGAIN, psdParms->optConfirm);
#endif

        xsdLoadAnimation(&G_sdAnim);
        ctlPrepareAnimation(WinWindowFromID(hwndConfirm, ID_SDDI_ICON),
                            XSD_ANIM_COUNT,
                            G_sdAnim.ahptr,
                            150,    // delay
                            TRUE);  // start now

        // cmnSetControlsFont(hwndConfirm, 1, 5000);
        winhCenterWindow(hwndConfirm);      // still hidden
        WinShowWindow(hwndConfirm, TRUE);

        // *** go!
        ulReturn = WinProcessDlg(hwndConfirm);

        ctlStopAnimation(WinWindowFromID(hwndConfirm, ID_SDDI_ICON));
        xsdFreeAnimation(&G_sdAnim);

        if (ulReturn == DID_OK)
        {
#ifndef __NOXSHUTDOWN__
            ULONG fl = cmnQuerySetting(sflXShutdown);
#endif

            psdParms->optWPSCloseWindows = winhIsDlgItemChecked(hwndConfirm,
                                                                ID_SDDI_WPS_CLOSEWINDOWS);
            if (psdParms->ulRestartWPS != 2)
            {
                // regular restart Desktop:
                // save close windows/startup folder settings
#ifndef __NOXSHUTDOWN__
                if (psdParms->optWPSCloseWindows)
                    fl |= XSD_WPS_CLOSEWINDOWS;
                else
                    fl &= ~XSD_WPS_CLOSEWINDOWS;
#endif
#ifndef __NOXWPSTARTUP__
                psdParms->optWPSReuseStartupFolder = winhIsDlgItemChecked(hwndConfirm,
                                                                          ID_SDDI_WPS_STARTUPFOLDER);
#endif
            }
#ifndef __NOXSHUTDOWN__
            if (!(winhIsDlgItemChecked(hwndConfirm,
                                       ID_SDDI_MESSAGEAGAIN)))
                fl |= XSD_NOCONFIRM;

            cmnSetSetting(sflXShutdown, fl);
#endif
        }

        WinDestroyWindow(hwndConfirm);
    }

    WinDestroyWindow(hwndDim);

    return (ulReturn);
}

/*
 *@@ xsdLoadAutoCloseItems:
 *      this gets the list of VIO windows which
 *      are to be closed automatically from OS2.INI
 *      and appends AUTOCLOSELISTITEM's to the given
 *      list accordingly.
 *
 *      If hwndListbox != NULLHANDLE, the program
 *      titles are added to that listbox as well.
 *
 *      Returns the no. of items which were added.
 *
 *@@added V0.9.1 (99-12-10) [umoeller]
 */

USHORT xsdLoadAutoCloseItems(PLINKLIST pllItems,   // in: list of AUTOCLOSELISTITEM's to append to
                             HWND hwndListbox)     // in: listbox to add items to or NULLHANDLE if none
{
    USHORT      usItemCount = 0;
    ULONG       ulKeyLength;
    PSZ         p, pINI;

    // get existing items from INI
    if (PrfQueryProfileSize(HINI_USER,
                            (PSZ)INIAPP_XWORKPLACE,
                            (PSZ)INIKEY_AUTOCLOSE,
                            &ulKeyLength))
    {
        // printf("Size: %d\n", ulKeyLength);
        // items exist: evaluate
        pINI = malloc(ulKeyLength);
        if (pINI)
        {
            PrfQueryProfileData(HINI_USER,
                                (PSZ)INIAPP_XWORKPLACE,
                                (PSZ)INIKEY_AUTOCLOSE,
                                pINI,
                                &ulKeyLength);
            p = pINI;
            //printf("%s\n", p);
            while (strlen(p))
            {
                PAUTOCLOSELISTITEM pliNew = malloc(sizeof(AUTOCLOSELISTITEM));
                strcpy(pliNew->szItemName, p);
                lstAppendItem(pllItems, // pData->pllAutoClose,
                              pliNew);

                if (hwndListbox)
                    WinSendMsg(hwndListbox,
                               LM_INSERTITEM,
                               (MPARAM)LIT_END,
                               (MPARAM)p);

                p += (strlen(p)+1);

                if (strlen(p))
                {
                    pliNew->usAction = *((PUSHORT)p);
                    p += sizeof(USHORT);
                }

                usItemCount++;
            }
            free(pINI);
        }
    }

    return (usItemCount);
}

/*
 *@@ xsdWriteAutoCloseItems:
 *      reverse to xsdLoadAutoCloseItems, this writes the
 *      auto-close items back to OS2.INI.
 *
 *      This returns 0 only if no error occured. If something
 *      != 0 is returned, that's the index of the list item
 *      which was found to be invalid.
 *
 *@@added V0.9.1 (99-12-10) [umoeller]
 */

USHORT xsdWriteAutoCloseItems(PLINKLIST pllItems)
{
    USHORT  usInvalid = 0;
    PSZ     pINI, p;
    // BOOL    fValid = TRUE;
    ULONG   ulItemCount = lstCountItems(pllItems);

    // store data in INI
    if (ulItemCount)
    {
        pINI = malloc(
                    sizeof(AUTOCLOSELISTITEM)
                  * ulItemCount);
        memset(pINI, 0,
                    sizeof(AUTOCLOSELISTITEM)
                  * ulItemCount);
        if (pINI)
        {
            PLISTNODE pNode = lstQueryFirstNode(pllItems);
            USHORT          usCurrent = 0;
            p = pINI;
            while (pNode)
            {
                PAUTOCLOSELISTITEM pli = pNode->pItemData;
                if (strlen(pli->szItemName) == 0)
                {
                    usInvalid = usCurrent;
                    break;
                }

                strcpy(p, pli->szItemName);
                p += (strlen(p)+1);
                *((PUSHORT)p) = pli->usAction;
                p += sizeof(USHORT);

                pNode = pNode->pNext;
                usCurrent++;
            }

            PrfWriteProfileData(HINI_USER,
                                (PSZ)INIAPP_XWORKPLACE,
                                (PSZ)INIKEY_AUTOCLOSE,
                                pINI,
                                (p - pINI + 2));

            free (pINI);
        }
    } // end if (pData->pliAutoClose)
    else
        // no items: delete INI key
        PrfWriteProfileData(HINI_USER,
                            (PSZ)INIAPP_XWORKPLACE,
                            (PSZ)INIKEY_AUTOCLOSE,
                            NULL, 0);

    return (usInvalid);
}

/*
 *@@ fnwpAutoCloseDetails:
 *      dlg func for "Auto-Close Details".
 *      This gets called from the notebook callbacks
 *      for the "XDesktop" notebook page
 *      (fncbDesktop1ItemChanged, xfdesk.c).
 *
 *@@changed V0.9.0 [umoeller]: adjusted for new linklist.c functions
 */

MRESULT EXPENTRY fnwpAutoCloseDetails(HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = (MPARAM)NULL;

    PAUTOCLOSEWINDATA pData = (PAUTOCLOSEWINDATA)WinQueryWindowPtr(hwndDlg, QWL_USER);

    switch (msg)
    {
        case WM_INITDLG:
        {
            // create window data in QWL_USER
            pData = malloc(sizeof(AUTOCLOSEWINDATA));
            memset(pData, 0, sizeof(AUTOCLOSEWINDATA));
            pData->pllAutoClose = lstCreate(TRUE);  // auto-free items

            // set animation
            xsdLoadAnimation(&G_sdAnim);
            ctlPrepareAnimation(WinWindowFromID(hwndDlg,
                                                ID_SDDI_ICON),
                                XSD_ANIM_COUNT,
                                &(G_sdAnim.ahptr[0]),
                                150,    // delay
                                TRUE);  // start now

            pData->usItemCount = 0;

            pData->usItemCount = xsdLoadAutoCloseItems(pData->pllAutoClose,
                                                      WinWindowFromID(hwndDlg,
                                                                      ID_XSDI_XRB_LISTBOX));

            winhSetEntryFieldLimit(WinWindowFromID(hwndDlg, ID_XSDI_XRB_ITEMNAME),
                                   100-1);

            WinSetWindowULong(hwndDlg, QWL_USER, (ULONG)pData);

            WinPostMsg(hwndDlg, XM_UPDATE, MPNULL, MPNULL);
        }
        break;

        /*
         * WM_CONTROL:
         *
         */

        case WM_CONTROL:
            switch (SHORT1FROMMP(mp1))
            {
                /*
                 * ID_XSDI_XRB_LISTBOX:
                 *      listbox was clicked on:
                 *      update other controls with new data
                 */

                case ID_XSDI_XRB_LISTBOX:
                    if (SHORT2FROMMP(mp1) == LN_SELECT)
                        WinSendMsg(hwndDlg, XM_UPDATE, MPNULL, MPNULL);
                break;

                /*
                 * ID_XSDI_XRB_ITEMNAME:
                 *      user changed item title: update data
                 */

                case ID_XSDI_XRB_ITEMNAME:
                    if (SHORT2FROMMP(mp1) == EN_KILLFOCUS)
                    {
                        if (pData)
                        {
                            if (pData->pliSelected)
                            {
                                WinQueryDlgItemText(hwndDlg, ID_XSDI_XRB_ITEMNAME,
                                                    sizeof(pData->pliSelected->szItemName)-1,
                                                    pData->pliSelected->szItemName);
                                WinSendDlgItemMsg(hwndDlg, ID_XSDI_XRB_LISTBOX,
                                                  LM_SETITEMTEXT,
                                                  (MPARAM)pData->sSelected,
                                                  (MPARAM)(pData->pliSelected->szItemName));
                            }
                        }
                    }
                break;

                // radio buttons
                case ID_XSDI_ACL_WMCLOSE:
                case ID_XSDI_ACL_CTRL_C:
                case ID_XSDI_ACL_KILLSESSION:
                case ID_XSDI_ACL_SKIP:
                    if (SHORT2FROMMP(mp1) == BN_CLICKED)
                    {
                        if (pData)
                        {
                            if (pData->pliSelected)
                            {
                                pData->pliSelected->usAction =
                                    (SHORT1FROMMP(mp1) == ID_XSDI_ACL_WMCLOSE)
                                        ? ACL_WMCLOSE
                                    : (SHORT1FROMMP(mp1) == ID_XSDI_ACL_CTRL_C)
                                        ? ACL_CTRL_C
                                    : (SHORT1FROMMP(mp1) == ID_XSDI_ACL_KILLSESSION)
                                        ? ACL_KILLSESSION
                                    : ACL_SKIP;
                            }
                        }
                    }
                break;

                default:
                    mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
            }
        break;

        case XM_UPDATE:
        {
            // posted from various locations to wholly update
            // the dlg items
            if (pData)
            {
                pData->pliSelected = NULL;
                pData->sSelected = (USHORT)WinSendDlgItemMsg(hwndDlg,
                        ID_XSDI_XRB_LISTBOX,
                        LM_QUERYSELECTION,
                        (MPARAM)LIT_CURSOR,
                        MPNULL);
                //printf("  Selected: %d\n", pData->sSelected);
                if (pData->sSelected != LIT_NONE)
                {
                    pData->pliSelected = (PAUTOCLOSELISTITEM)lstItemFromIndex(
                                               pData->pllAutoClose,
                                               pData->sSelected);
                }

                if (pData->pliSelected)
                {
                    WinSetDlgItemText(hwndDlg, ID_XSDI_XRB_ITEMNAME,
                            pData->pliSelected->szItemName);

                    switch (pData->pliSelected->usAction)
                    {
                        case ACL_WMCLOSE:
                            winhSetDlgItemChecked(hwndDlg,
                                ID_XSDI_ACL_WMCLOSE, 1); break;
                        case ACL_CTRL_C:
                            winhSetDlgItemChecked(hwndDlg,
                                ID_XSDI_ACL_CTRL_C, 1); break;
                        case ACL_KILLSESSION:
                            winhSetDlgItemChecked(hwndDlg,
                                ID_XSDI_ACL_KILLSESSION, 1); break;
                        case ACL_SKIP:
                            winhSetDlgItemChecked(hwndDlg,
                                ID_XSDI_ACL_SKIP, 1); break;
                    }
                }
                else
                    WinSetDlgItemText(hwndDlg,
                                      ID_XSDI_XRB_ITEMNAME,
                                      "");

                winhEnableDlgItem(hwndDlg, ID_XSDI_XRB_ITEMNAME,
                            (pData->pliSelected != NULL));

                winhEnableDlgItem(hwndDlg, ID_XSDI_ACL_WMCLOSE,
                            (pData->pliSelected != NULL));
                winhEnableDlgItem(hwndDlg, ID_XSDI_ACL_CTRL_C,
                            (pData->pliSelected != NULL));
                winhEnableDlgItem(hwndDlg, ID_XSDI_ACL_KILLSESSION,
                            (pData->pliSelected != NULL));
                winhEnableDlgItem(hwndDlg, ID_XSDI_ACL_SKIP,
                            (pData->pliSelected != NULL));

                winhEnableDlgItem(hwndDlg,
                                  ID_XSDI_XRB_DELETE,
                                  (   (pData->usItemCount > 0)
                                   && (pData->pliSelected)
                                  ));
            }
        }
        break;

        case WM_COMMAND:
            switch (SHORT1FROMMP(mp1))
            {

                /*
                 * ID_XSDI_XRB_NEW:
                 *      create new item
                 */

                case ID_XSDI_XRB_NEW:
                {
                    PAUTOCLOSELISTITEM pliNew = malloc(sizeof(AUTOCLOSELISTITEM));
                    strcpy(pliNew->szItemName, "???");
                    pliNew->usAction = ACL_SKIP;
                    lstAppendItem(pData->pllAutoClose,
                                  pliNew);

                    pData->usItemCount++;
                    WinSendDlgItemMsg(hwndDlg, ID_XSDI_XRB_LISTBOX,
                                      LM_INSERTITEM,
                                      (MPARAM)LIT_END,
                                      (MPARAM)pliNew->szItemName);
                    WinSendDlgItemMsg(hwndDlg, ID_XSDI_XRB_LISTBOX,
                                      LM_SELECTITEM, // will cause XM_UPDATE
                                      (MPARAM)(lstCountItems(
                                              pData->pllAutoClose)),
                                      (MPARAM)TRUE);
                    winhSetDlgItemFocus(hwndDlg, ID_XSDI_XRB_ITEMNAME);
                    WinSendDlgItemMsg(hwndDlg, ID_XSDI_XRB_ITEMNAME,
                                      EM_SETSEL,
                                      MPFROM2SHORT(0, 1000), // select all
                                      MPNULL);
                }
                break;

                /*
                 * ID_XSDI_XRB_DELETE:
                 *      delete selected item
                 */

                case ID_XSDI_XRB_DELETE:
                {
                    //printf("WM_COMMAND ID_XSDI_XRB_DELETE BN_CLICKED\n");
                    if (pData)
                    {
                        if (pData->pliSelected)
                        {
                            lstRemoveItem(pData->pllAutoClose,
                                          pData->pliSelected);
                            WinSendDlgItemMsg(hwndDlg, ID_XSDI_XRB_LISTBOX,
                                              LM_DELETEITEM,
                                              (MPARAM)pData->sSelected,
                                              MPNULL);
                        }
                        WinPostMsg(hwndDlg, XM_UPDATE, MPNULL, MPNULL);
                    }
                    winhSetDlgItemFocus(hwndDlg, ID_XSDI_XRB_LISTBOX);
                }
                break;

                /*
                 * DID_OK:
                 *      store data in INI and dismiss dlg
                 */

                case DID_OK:
                {
                    USHORT usInvalid;
                    if (usInvalid = xsdWriteAutoCloseItems(pData->pllAutoClose))
                    {
                        WinAlarm(HWND_DESKTOP, WA_ERROR);
                        WinSendDlgItemMsg(hwndDlg, ID_XSDI_XRB_LISTBOX,
                                          LM_SELECTITEM,
                                          (MPARAM)usInvalid,
                                          (MPARAM)TRUE);
                    }
                    else
                        // dismiss dlg
                        mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
                }
                break;

                default:  // includes DID_CANCEL
                    mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
                break;
            }
        break;

        case WM_HELP:
            cmnDisplayHelp(NULL,
                           ID_XFH_AUTOCLOSEDETAILS);
        break;

        /*
         * WM_DESTROY:
         *      clean up allocated memory
         */

        case WM_DESTROY:
        {
            ctlStopAnimation(WinWindowFromID(hwndDlg, ID_SDDI_ICON));
            xsdFreeAnimation(&G_sdAnim);
            lstFree(&pData->pllAutoClose);
            free(pData);
            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
        }
        break;

        default:
            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
        break;
    }
    return (mrc);
}

/*
 *@@ fnwpUserRebootOptions:
 *      dlg proc for the "Extended Reboot" options.
 *      This gets called from the notebook callbacks
 *      for the "XDesktop" notebook page
 *      (fncbDesktop1ItemChanged, xfdesk.c).
 *
 *@@changed V0.9.0 [umoeller]: adjusted for new linklist.c functions
 *@@changed V0.9.0 [umoeller]: renamed from fnwpRebootExt
 *@@changed V0.9.0 [umoeller]: added "Partitions" button
 */

MRESULT EXPENTRY fnwpUserRebootOptions(HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = (MPARAM)NULL;

    switch (msg)
    {
        /*
         * WM_INITDLG:
         *
         */

        case WM_INITDLG:
        {
            ULONG       ulKeyLength;
            PSZ         p, pINI;

            // create window data in QWL_USER
            PREBOOTWINDATA pData = malloc(sizeof(REBOOTWINDATA));
            memset(pData, 0, sizeof(REBOOTWINDATA));
            pData->pllReboot = lstCreate(TRUE);

            // set animation
            xsdLoadAnimation(&G_sdAnim);
            ctlPrepareAnimation(WinWindowFromID(hwndDlg, ID_SDDI_ICON),
                                XSD_ANIM_COUNT,
                                &(G_sdAnim.ahptr[0]),
                                150,    // delay
                                TRUE);  // start now

            pData->usItemCount = 0;

            // get existing items from INI
            if (PrfQueryProfileSize(HINI_USER,
                                    (PSZ)INIAPP_XWORKPLACE,
                                    (PSZ)INIKEY_BOOTMGR,
                                    &ulKeyLength))
            {
                // _Pmpf(( "Size: %d", ulKeyLength ));
                // items exist: evaluate
                pINI = malloc(ulKeyLength);
                if (pINI)
                {
                    PrfQueryProfileData(HINI_USER,
                                        (PSZ)INIAPP_XWORKPLACE,
                                        (PSZ)INIKEY_BOOTMGR,
                                        pINI,
                                        &ulKeyLength);
                    p = pINI;
                    // _Pmpf(( "%s", p ));
                    while (strlen(p))
                    {
                        PREBOOTLISTITEM pliNew = malloc(sizeof(REBOOTLISTITEM));
                        strcpy(pliNew->szItemName, p);
                        lstAppendItem(pData->pllReboot,
                                    pliNew);

                        WinSendDlgItemMsg(hwndDlg, ID_XSDI_XRB_LISTBOX,
                                        LM_INSERTITEM,
                                        (MPARAM)LIT_END,
                                        (MPARAM)p);
                        p += (strlen(p)+1);

                        if (strlen(p)) {
                            strcpy(pliNew->szCommand, p);
                            p += (strlen(p)+1);
                        }
                        pData->usItemCount++;
                    }
                    free(pINI);
                }
            }

            WinSendDlgItemMsg(hwndDlg, ID_XSDI_XRB_ITEMNAME,
                EM_SETTEXTLIMIT, (MPARAM)(100-1), MPNULL);
            WinSendDlgItemMsg(hwndDlg, ID_XSDI_XRB_COMMAND,
                EM_SETTEXTLIMIT, (MPARAM)(CCHMAXPATH-1), MPNULL);

            WinSetWindowULong(hwndDlg, QWL_USER, (ULONG)pData);

            // create "menu button" for "Partitions..."
            ctlMakeMenuButton(WinWindowFromID(hwndDlg, ID_XSDI_XRB_PARTITIONS),
                              // set menu resource module and ID to
                              // 0; this will cause WM_COMMAND for
                              // querying the menu handle to be
                              // displayed
                              0, 0);

            WinPostMsg(hwndDlg, XM_UPDATE, MPNULL, MPNULL);
        break; }

        /*
         * WM_CONTROL:
         *
         */

        case WM_CONTROL:
        {
            switch (SHORT1FROMMP(mp1))
            {
                /*
                 * ID_XSDI_XRB_LISTBOX:
                 *      new reboot item selected.
                 */

                case ID_XSDI_XRB_LISTBOX:
                    if (SHORT2FROMMP(mp1) == LN_SELECT)
                        WinSendMsg(hwndDlg, XM_UPDATE, MPNULL, MPNULL);
                break;

                /*
                 * ID_XSDI_XRB_ITEMNAME:
                 *      reboot item name changed.
                 */

                case ID_XSDI_XRB_ITEMNAME:
                {
                    if (SHORT2FROMMP(mp1) == EN_KILLFOCUS)
                    {
                        PREBOOTWINDATA pData =
                                (PREBOOTWINDATA)WinQueryWindowPtr(hwndDlg, QWL_USER);
                        // _Pmpf(( "WM_CONTROL ID_XSDI_XRB_ITEMNAME EN_KILLFOCUS" ));
                        if (pData)
                        {
                            if (pData->pliSelected)
                            {
                                WinQueryDlgItemText(hwndDlg, ID_XSDI_XRB_ITEMNAME,
                                                    sizeof(pData->pliSelected->szItemName)-1,
                                                    pData->pliSelected->szItemName);
                                WinSendDlgItemMsg(hwndDlg, ID_XSDI_XRB_LISTBOX,
                                                  LM_SETITEMTEXT,
                                                  (MPARAM)pData->sSelected,
                                                  (MPARAM)(pData->pliSelected->szItemName));
                            }
                        }
                    }
                break; }

                /*
                 * ID_XSDI_XRB_COMMAND:
                 *      reboot item command changed.
                 */

                case ID_XSDI_XRB_COMMAND:
                {
                    if (SHORT2FROMMP(mp1) == EN_KILLFOCUS)
                    {
                        PREBOOTWINDATA pData =
                                (PREBOOTWINDATA)WinQueryWindowPtr(hwndDlg, QWL_USER);
                        // _Pmpf(( "WM_CONTROL ID_XSDI_XRB_COMMAND EN_KILLFOCUS" ));
                        if (pData)
                            if (pData->pliSelected)
                                WinQueryDlgItemText(hwndDlg, ID_XSDI_XRB_COMMAND,
                                                    sizeof(pData->pliSelected->szCommand)-1,
                                                    pData->pliSelected->szCommand);
                    }
                break; }

                default:
                    mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
            }
        break; }

        /*
         * XM_UPDATE:
         *      updates the controls according to the
         *      currently selected list box item.
         */

        case XM_UPDATE:
        {
            PREBOOTWINDATA pData =
                    (PREBOOTWINDATA)WinQueryWindowPtr(hwndDlg, QWL_USER);
            if (pData)
            {
                pData->pliSelected = NULL;
                pData->sSelected = (USHORT)WinSendDlgItemMsg(hwndDlg,
                                                             ID_XSDI_XRB_LISTBOX,
                                                             LM_QUERYSELECTION,
                                                             (MPARAM)LIT_CURSOR,
                                                             MPNULL);
                if (pData->sSelected != LIT_NONE)
                    pData->pliSelected = (PREBOOTLISTITEM)lstItemFromIndex(
                            pData->pllReboot,
                            pData->sSelected);

                if (pData->pliSelected)
                {
                    WinSetDlgItemText(hwndDlg, ID_XSDI_XRB_ITEMNAME,
                            pData->pliSelected->szItemName);
                    WinSetDlgItemText(hwndDlg, ID_XSDI_XRB_COMMAND,
                            pData->pliSelected->szCommand);
                }
                else
                {
                    WinSetDlgItemText(hwndDlg, ID_XSDI_XRB_ITEMNAME,
                            "");
                    WinSetDlgItemText(hwndDlg, ID_XSDI_XRB_COMMAND,
                            "");
                }
                winhEnableDlgItem(hwndDlg, ID_XSDI_XRB_ITEMNAME,
                            (pData->pliSelected != NULL));
                winhEnableDlgItem(hwndDlg, ID_XSDI_XRB_COMMAND,
                            (pData->pliSelected != NULL));
                winhEnableDlgItem(hwndDlg, ID_XSDI_XRB_PARTITIONS,
                            (pData->pliSelected != NULL));
                winhEnableDlgItem(hwndDlg, ID_XSDI_XRB_UP,
                            (   (pData->pliSelected != NULL)
                             && (pData->usItemCount > 1)
                             && (pData->sSelected > 0)
                            ));
                winhEnableDlgItem(hwndDlg, ID_XSDI_XRB_DOWN,
                            (   (pData->pliSelected != NULL)
                             && (pData->usItemCount > 1)
                             && (pData->sSelected < (pData->usItemCount-1))
                            ));

                winhEnableDlgItem(hwndDlg, ID_XSDI_XRB_DELETE,
                            (   (pData->usItemCount > 0)
                             && (pData->pliSelected)
                            )
                        );
            }
        break; }

        /*
         * WM_COMMAND:
         *
         */

        case WM_COMMAND:
        {
            PREBOOTWINDATA pData =
                    (PREBOOTWINDATA)WinQueryWindowPtr(hwndDlg, QWL_USER);
            USHORT usItemID = SHORT1FROMMP(mp1);
            switch (usItemID)
            {
                /*
                 * ID_XSDI_XRB_NEW:
                 *      create new item
                 */

                case ID_XSDI_XRB_NEW:
                {
                    PREBOOTLISTITEM pliNew = malloc(sizeof(REBOOTLISTITEM));
                    // _Pmpf(( "WM_COMMAND ID_XSDI_XRB_NEW BN_CLICKED" ));
                    strcpy(pliNew->szItemName, "???");
                    strcpy(pliNew->szCommand, "???");
                    lstAppendItem(pData->pllReboot,
                                  pliNew);

                    pData->usItemCount++;
                    WinSendDlgItemMsg(hwndDlg, ID_XSDI_XRB_LISTBOX,
                                    LM_INSERTITEM,
                                    (MPARAM)LIT_END,
                                    (MPARAM)pliNew->szItemName);
                    WinSendDlgItemMsg(hwndDlg, ID_XSDI_XRB_LISTBOX,
                                    LM_SELECTITEM,
                                    (MPARAM)(lstCountItems(
                                            pData->pllReboot)
                                         - 1),
                                    (MPARAM)TRUE);
                    winhSetDlgItemFocus(hwndDlg, ID_XSDI_XRB_ITEMNAME);
                    WinSendDlgItemMsg(hwndDlg, ID_XSDI_XRB_ITEMNAME,
                            EM_SETSEL,
                            MPFROM2SHORT(0, 1000), // select all
                            MPNULL);
                break; }

                /*
                 * ID_XSDI_XRB_DELETE:
                 *      delete delected item
                 */

                case ID_XSDI_XRB_DELETE:
                {
                    if (pData)
                    {
                        if (pData->pliSelected)
                        {
                            lstRemoveItem(pData->pllReboot,
                                    pData->pliSelected);
                            WinSendDlgItemMsg(hwndDlg, ID_XSDI_XRB_LISTBOX,
                                    LM_DELETEITEM,
                                    (MPARAM)pData->sSelected,
                                    MPNULL);
                        }
                        WinPostMsg(hwndDlg, XM_UPDATE, MPNULL, MPNULL);
                    }
                    winhSetDlgItemFocus(hwndDlg, ID_XSDI_XRB_LISTBOX);
                break; }

                /*
                 * ID_XSDI_XRB_UP:
                 *      move selected item up
                 */

                case ID_XSDI_XRB_UP:
                {
                    if (pData)
                    {
                        // _Pmpf(( "WM_COMMAND ID_XSDI_XRB_UP BN_CLICKED" ));
                        if (pData->pliSelected)
                        {
                            PREBOOTLISTITEM pliNew = malloc(sizeof(REBOOTLISTITEM));
                            *pliNew = *(pData->pliSelected);
                            // remove selected
                            lstRemoveItem(pData->pllReboot,
                                    pData->pliSelected);
                            WinSendDlgItemMsg(hwndDlg, ID_XSDI_XRB_LISTBOX,
                                    LM_DELETEITEM,
                                    (MPARAM)pData->sSelected,
                                    MPNULL);
                            // insert item again
                            lstInsertItemBefore(pData->pllReboot,
                                                pliNew,
                                                (pData->sSelected-1));
                            WinSendDlgItemMsg(hwndDlg, ID_XSDI_XRB_LISTBOX,
                                            LM_INSERTITEM,
                                            (MPARAM)(pData->sSelected-1),
                                            (MPARAM)pliNew->szItemName);
                            WinSendDlgItemMsg(hwndDlg, ID_XSDI_XRB_LISTBOX,
                                            LM_SELECTITEM,
                                            (MPARAM)(pData->sSelected-1),
                                            (MPARAM)TRUE); // select flag
                        }
                        WinPostMsg(hwndDlg, XM_UPDATE, MPNULL, MPNULL);
                    }
                    winhSetDlgItemFocus(hwndDlg, ID_XSDI_XRB_LISTBOX);
                break; }

                /*
                 * ID_XSDI_XRB_DOWN:
                 *      move selected item down
                 */

                case ID_XSDI_XRB_DOWN:
                {
                    if (pData)
                    {
                        // _Pmpf(( "WM_COMMAND ID_XSDI_XRB_DOWN BN_CLICKED" ));
                        if (pData->pliSelected)
                        {
                            PREBOOTLISTITEM pliNew = malloc(sizeof(REBOOTLISTITEM));
                            *pliNew = *(pData->pliSelected);
                            // remove selected
                            // _Pmpf(( "  Removing index %d", pData->sSelected ));
                            lstRemoveItem(pData->pllReboot,
                                          pData->pliSelected);
                            WinSendDlgItemMsg(hwndDlg, ID_XSDI_XRB_LISTBOX,
                                    LM_DELETEITEM,
                                    (MPARAM)pData->sSelected,
                                    MPNULL);
                            // insert item again
                            lstInsertItemBefore(pData->pllReboot,
                                                pliNew,
                                                (pData->sSelected+1));
                            WinSendDlgItemMsg(hwndDlg, ID_XSDI_XRB_LISTBOX,
                                            LM_INSERTITEM,
                                            (MPARAM)(pData->sSelected+1),
                                            (MPARAM)pliNew->szItemName);
                            WinSendDlgItemMsg(hwndDlg, ID_XSDI_XRB_LISTBOX,
                                            LM_SELECTITEM,
                                            (MPARAM)(pData->sSelected+1),
                                            (MPARAM)TRUE); // select flag
                        }
                        WinPostMsg(hwndDlg, XM_UPDATE, MPNULL, MPNULL);
                    }
                    winhSetDlgItemFocus(hwndDlg, ID_XSDI_XRB_LISTBOX);
                break; }

                /*
                 * ID_XSDI_XRB_PARTITIONS:
                 *      "Partitions" button.
                 *      Even though this is part of the WM_COMMAND
                 *      block, this is not really a command msg
                 *      like the other messages; instead, this
                 *      is sent (!) to us and expects a HWND for
                 *      the menu to be displayed on the button.
                 *
                 *      We create a menu containing bootable
                 *      partitions.
                 */

                case ID_XSDI_XRB_PARTITIONS:
                {
                    HPOINTER       hptrOld = winhSetWaitPointer();
                    HWND           hMenu = NULLHANDLE;

                    if (!pData->fPartitionsLoaded)  // V0.9.9 (2001-04-07) [umoeller]
                    {
                        // first time:
                        USHORT         usContext = 0;
                        APIRET arc = doshGetPartitionsList(&pData->pPartitionsList,
                                                           &usContext);

                        pData->fPartitionsLoaded = TRUE;
                    }

                    _Pmpf((__FUNCTION__ ": pData->pPartitionsList is 0x%lX",
                                pData->pPartitionsList));

                    if (pData->pPartitionsList)
                    {
                        PPARTITIONINFO ppi = pData->pPartitionsList->pPartitionInfo;
                        SHORT          sItemId = ID_XSDI_PARTITIONSFIRST;
                        hMenu = WinCreateMenu(HWND_DESKTOP,
                                              NULL); // no menu template
                        while (ppi)
                        {
                            if (ppi->fBootable)
                            {
                                CHAR szMenuItem[100];
                                sprintf(szMenuItem,
                                        "%s (Drive %d, %c:)",
                                        ppi->szBootName,
                                        ppi->bDisk,
                                        ppi->cLetter);
                                winhInsertMenuItem(hMenu,
                                                   MIT_END,
                                                   sItemId++,
                                                   szMenuItem,
                                                   MIS_TEXT,
                                                   0);
                            }
                            ppi = ppi->pNext;
                        }
                    }

                    WinSetPointer(HWND_DESKTOP, hptrOld);

                    mrc = (MRESULT)hMenu;
                break; }

                /*
                 * DID_OK:
                 *      store data in INI and dismiss dlg
                 */

                case DID_OK:
                {
                    PSZ     pINI, p;
                    BOOL    fValid = TRUE;
                    ULONG   ulItemCount = lstCountItems(pData->pllReboot);

                    // _Pmpf(( "WM_COMMAND DID_OK BN_CLICKED" ));
                    // store data in INI
                    if (ulItemCount)
                    {
                        pINI = malloc(
                                    sizeof(REBOOTLISTITEM)
                                  * ulItemCount);
                        memset(pINI, 0,
                                    sizeof(REBOOTLISTITEM)
                                  * ulItemCount);

                        if (pINI)
                        {
                            PLISTNODE       pNode = lstQueryFirstNode(pData->pllReboot);
                            USHORT          usCurrent = 0;
                            p = pINI;

                            while (pNode)
                            {
                                PREBOOTLISTITEM pli = pNode->pItemData;

                                if (    (strlen(pli->szItemName) == 0)
                                     || (strlen(pli->szCommand) == 0)
                                   )
                                {
                                    WinAlarm(HWND_DESKTOP, WA_ERROR);
                                    WinSendDlgItemMsg(hwndDlg, ID_XSDI_XRB_LISTBOX,
                                                    LM_SELECTITEM,
                                                    (MPARAM)usCurrent,
                                                    (MPARAM)TRUE);
                                    fValid = FALSE;
                                    break;
                                }
                                strcpy(p, pli->szItemName);
                                p += (strlen(p)+1);
                                strcpy(p, pli->szCommand);
                                p += (strlen(p)+1);

                                pNode = pNode->pNext;
                                usCurrent++;
                            }

                            PrfWriteProfileData(HINI_USER,
                                        (PSZ)INIAPP_XWORKPLACE,
                                        (PSZ)INIKEY_BOOTMGR,
                                        pINI,
                                        (p - pINI + 2));

                            free (pINI);
                        }
                    } // end if (pData->pliReboot)
                    else
                        PrfWriteProfileData(HINI_USER,
                                    (PSZ)INIAPP_XWORKPLACE,
                                    (PSZ)INIKEY_BOOTMGR,
                                    NULL, 0);

                    // dismiss dlg
                    if (fValid)
                        mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
                break; }

                default: // includes DID_CANCEL
                    if (    (pData->pPartitionsList)
                         && (pData->pPartitionsList->cPartitions)
                       )
                    {
                        // partitions valid:
                        ULONG cPartitions = pData->pPartitionsList->cPartitions;
                        if (    (usItemID >= ID_XSDI_PARTITIONSFIRST)
                             && (usItemID < ID_XSDI_PARTITIONSFIRST + cPartitions)
                             && (pData->pliSelected)
                           )
                        {
                            // partition item from "Partitions" menu button:
                            // search partitions list then
                            PPARTITIONINFO ppi = pData->pPartitionsList->pPartitionInfo;
                            SHORT sItemIDCompare = ID_XSDI_PARTITIONSFIRST;
                            while (ppi)
                            {
                                if (ppi->fBootable)
                                {
                                    // bootable item:
                                    // then we have inserted the thing into
                                    // the menu
                                    if (sItemIDCompare == usItemID)
                                    {
                                        // found our one:
                                        // insert into entry field
                                        CHAR szItem[20];
                                        // CHAR szCommand[100];
                                        ULONG ul = 0;

                                        // strip trailing spaces
                                        strcpy(szItem, ppi->szBootName);
                                        for (ul = strlen(szItem) - 1;
                                             ul > 0;
                                             ul--)
                                            if (szItem[ul] == ' ')
                                                szItem[ul] = 0;
                                            else
                                                break;

                                        // now set reboot item's data
                                        // according to the partition item
                                        strcpy(pData->pliSelected->szItemName,
                                               szItem);
                                        // compose new command
                                        sprintf(pData->pliSelected->szCommand,
                                                "setboot /iba:\"%s\"",
                                                szItem);

                                        // update list box item
                                        WinSendDlgItemMsg(hwndDlg, ID_XSDI_XRB_LISTBOX,
                                                          LM_SETITEMTEXT,
                                                          (MPARAM)pData->sSelected,
                                                          (MPARAM)(pData->pliSelected->szItemName));
                                        // update rest of dialog
                                        WinSendMsg(hwndDlg, XM_UPDATE, MPNULL, MPNULL);

                                        break; // while (ppi)
                                    }
                                    else
                                        // next item
                                        sItemIDCompare++;
                                }
                                ppi = ppi->pNext;
                            }

                            break;
                        }
                    }
                    mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
                break;
            }
        break; }

        case WM_HELP:
            cmnDisplayHelp(NULL,
                           ID_XFH_REBOOTEXT);
        break;

        /*
         * WM_DESTROY:
         *      clean up allocated memory
         */

        case WM_DESTROY:
        {
            PREBOOTWINDATA pData = (PREBOOTWINDATA)WinQueryWindowPtr(hwndDlg, QWL_USER);
            ctlStopAnimation(WinWindowFromID(hwndDlg, ID_SDDI_ICON));
            xsdFreeAnimation(&G_sdAnim);
            lstFree(&pData->pllReboot);
            if (pData->pPartitionsList)
                doshFreePartitionsList(pData->pPartitionsList);
            free(pData);
            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
        break; }

        default:
            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
        break;
    }
    return (mrc);
}

/* ******************************************************************
 *
 *   Shutdown interface
 *
 ********************************************************************/

/*
 *@@ xsdQueryShutdownSettings:
 *      this fills the specified SHUTDOWNPARAMS array with
 *      the current shutdown settings, as specified by the
 *      user in the desktop's settings notebooks.
 *
 *      Notes:
 *
 *      -- psdp->optAnimate is set to the animation setting
 *         which applies according to whether reboot is enabled
 *         or not.
 *
 *      -- There is no "current setting" for the user reboot
 *         command. As a result, psdp->szRebootCommand is
 *         zeroed.
 *
 *      -- Neither is there a "current setting" for whether
 *         to use "restart Desktop" or "logoff" instead. To later use
 *         "restart Desktop", set psdp->ulRestartWPS and maybe
 *         psdp->optWPSReuseStartupFolder also.
 *
 *@@added V0.9.7 (2001-01-25) [umoeller]
 */

VOID xsdQueryShutdownSettings(PSHUTDOWNPARAMS psdp)
{
    ULONG flShutdown = 0;
#ifndef __NOXSHUTDOWN__
    flShutdown = cmnQuerySetting(sflXShutdown);
#endif

    memset(psdp, 0, sizeof(SHUTDOWNPARAMS));
    psdp->optReboot = ((flShutdown & XSD_REBOOT) != 0);
    psdp->optConfirm = (!(flShutdown & XSD_NOCONFIRM));
    psdp->optDebug = FALSE;

    psdp->ulRestartWPS = 0;         // no, do shutdown

    psdp->optWPSCloseWindows = TRUE;
    psdp->optAutoCloseVIO = ((flShutdown & XSD_AUTOCLOSEVIO) != 0);
    psdp->optLog = ((flShutdown & XSD_LOG) != 0);

    /* if (psdp->optReboot)
        // animate on reboot? V0.9.3 (2000-05-22) [umoeller]
        psdp->optAnimate = ((flShutdown & XSD_ANIMATE_REBOOT) != 0);
    else
        psdp->optAnimate = ((flShutdown & XSD_ANIMATE_SHUTDOWN) != 0);
       */

    psdp->optAPMPowerOff = (  ((flShutdown & XSD_APMPOWEROFF) != 0)
                      && (apmPowerOffSupported())
                     );
    psdp->optAPMDelay = ((flShutdown & XSD_APM_DELAY) != 0);

    psdp->optWPSReuseStartupFolder = psdp->optWPSCloseWindows;

    psdp->optEmptyTrashCan = ((flShutdown & XSD_EMPTY_TRASH) != 0);

    psdp->optWarpCenterFirst = ((flShutdown & XSD_WARPCENTERFIRST) != 0);

    psdp->szRebootCommand[0] = 0;
}

/*
 *@@ xsdIsShutdownRunning:
 *      returns
 *
 *      --  0 if shutdown is not running;
 *
 *      --  1 if the shutdown confirmation dialogs are open;
 *
 *      --  2 if the shutdown thread is active.
 *
 *@@added V0.9.9 (2001-03-07) [umoeller]
 *@@changed V0.9.16 (2001-11-22) [umoeller]: changed return codes
 */

ULONG xsdIsShutdownRunning(VOID)
{
    if (G_fShutdownRunning)
        return 1;
    else if (thrQueryID(&G_tiShutdownThread))
        return 2;

    return 0;
}

/*
 *@@ StartShutdownThread:
 *      starts the Shutdown thread with the specified
 *      parameters.
 *
 *@@added V0.9.9 (2001-03-07) [umoeller]
 */

static VOID StartShutdownThread(BOOL fStartShutdown,
                                BOOL fPlayRestartDesktopSound,     // in: else: play shutdown sound
                                PSHUTDOWNPARAMS psdp)
{
    if (psdp)
    {
        if (fStartShutdown)
        {
            // everything OK: create shutdown thread,
            // which will handle the rest
            thrCreate(&G_tiShutdownThread,
                      fntShutdownThread,
                      NULL, // running flag
                      "Shutdown",
                      THRF_PMMSGQUEUE,    // changed V0.9.12 (2001-05-29) [umoeller]
                      (ULONG)psdp);           // pass SHUTDOWNPARAMS to thread
#ifndef __NOXSYSTEMSOUNDS__
            cmnPlaySystemSound(fPlayRestartDesktopSound
                                    ? MMSOUND_XFLD_RESTARTWPS
                                    : MMSOUND_XFLD_SHUTDOWN);
#endif
        }
        else
            free(psdp);     // fixed V0.9.1 (99-12-12)
    }

    G_fShutdownRunning = fStartShutdown;
}

/*
 *@@ xsdInitiateShutdown:
 *      common shutdown entry point; checks GLOBALSETTINGS.__flXShutdown
 *      for all the XSD_* flags (shutdown options).
 *      If compiled with XFLDR_DEBUG defined (in common.h),
 *      debug mode will also be turned on if the SHIFT key is
 *      pressed at call time (that is, when the menu item is
 *      selected).
 *
 *      This routine will display a confirmation box,
 *      if the settings want it, and then start the
 *      main shutdown thread (xsd_fntShutdownThread),
 *      which will keep running even after shutdown
 *      is complete, unless the user presses the
 *      "Cancel shutdown" button.
 *
 *      Although this method does return almost
 *      immediately (after the confirmation dlg is dismissed),
 *      the shutdown will proceed in the separate thread
 *      after this function returns.
 *
 *@@changed V0.9.0 [umoeller]: global SHUTDOWNPARAMS removed
 *@@changed V0.9.0 [umoeller]: this used to be an XFldDesktop instance method
 *@@changed V0.9.1 (99-12-10) [umoeller]: fixed KERNELGLOBALS locks
 *@@changed V0.9.1 (99-12-12) [umoeller]: fixed memory leak when shutdown was cancelled
 *@@changed V0.9.3 (2000-05-22) [umoeller]: added animate on reboot
 *@@changed V0.9.4 (2000-08-03) [umoeller]: added "empty trash can"
 *@@changed V0.9.11 (2001-04-25) [umoeller]: changed pending spool jobs msg to always abort now
 */

BOOL xsdInitiateShutdown(VOID)
{
    BOOL                fStartShutdown = TRUE;
    ULONG               ulSpooled = 0;
    PSHUTDOWNPARAMS     psdp = (PSHUTDOWNPARAMS)malloc(sizeof(SHUTDOWNPARAMS));

    if (xsdIsShutdownRunning())
        // shutdown thread already running: return!
        fStartShutdown = FALSE;

    // lock shutdown menu items
    G_fShutdownRunning = TRUE;

    if (fStartShutdown)
    {
        ULONG flShutdown = 0;
#ifndef __NOXSHUTDOWN__
        flShutdown = cmnQuerySetting(sflXShutdown);
#endif

        memset(psdp, 0, sizeof(SHUTDOWNPARAMS));
        psdp->optReboot = ((flShutdown & XSD_REBOOT) != 0);
        psdp->ulRestartWPS = 0;
        psdp->optWPSCloseWindows = TRUE;
        psdp->optWPSReuseStartupFolder = psdp->optWPSCloseWindows;
        psdp->optConfirm = (!(flShutdown & XSD_NOCONFIRM));
        psdp->optAutoCloseVIO = ((flShutdown & XSD_AUTOCLOSEVIO) != 0);
        psdp->optWarpCenterFirst = ((flShutdown & XSD_WARPCENTERFIRST) != 0);
        psdp->optLog = ((flShutdown & XSD_LOG) != 0);
        /* if (psdp->optReboot)
            // animate on reboot? V0.9.3 (2000-05-22) [umoeller]
            psdp->optAnimate = ((flShutdown & XSD_ANIMATE_REBOOT) != 0);
        else
            psdp->optAnimate = ((flShutdown & XSD_ANIMATE_SHUTDOWN) != 0);
           */

        psdp->optAPMPowerOff = (  ((flShutdown & XSD_APMPOWEROFF) != 0)
                          && (apmPowerOffSupported())
                         );
        psdp->optAPMDelay = ((flShutdown & XSD_APM_DELAY) != 0);
        #ifdef DEBUG_SHUTDOWN
            psdp->optDebug = doshQueryShiftState();
        #else
            psdp->optDebug = FALSE;
        #endif

        psdp->optEmptyTrashCan = ((flShutdown & XSD_EMPTY_TRASH) != 0);

        psdp->szRebootCommand[0] = 0;

        if (psdp->optConfirm)
        {
            ULONG ulReturn = xsdConfirmShutdown(psdp);
            if (ulReturn != DID_OK)
                fStartShutdown = FALSE;
        }

        if (fStartShutdown)
        {
            // check for pending spool jobs
            ulSpooled = winhQueryPendingSpoolJobs();
            if (ulSpooled)
            {
                // if we have any, issue a warning message and
                // tell the user to remove print jobs
                CHAR szTemp[20];
                PSZ pTable[1];
                sprintf(szTemp, "%d", ulSpooled);
                pTable[0] = szTemp;
                cmnMessageBoxMsgExt(HWND_DESKTOP,
                                    114,
                                    pTable,
                                    1,
                                    115,            // tmf file updated V0.9.11 (2001-04-25) [umoeller]
                                    MB_CANCEL);
                // changed this V0.9.11 (2001-04-25) [umoeller]:
                // never allow the user to continue here... we used
                // to have a yesno box here, but apparently continuing
                // here hangs the system, so I changed the message to
                // "please remove print jobs from the spooler".
                fStartShutdown = FALSE;
            }
        }
    }

    StartShutdownThread(fStartShutdown,
                        FALSE,  // fPlayRestartDesktopSound
                        psdp);

    return (fStartShutdown);
}

/*
 *@@ xsdInitiateRestartWPS:
 *      pretty similar to xsdInitiateShutdown, i.e. this
 *      will also show a confirmation box and start the Shutdown
 *      thread, except that flags are set differently so that
 *      after closing all windows, no shutdown is performed, but
 *      only the WPS is restarted.
 *
 *@@changed V0.9.0 [umoeller]: global SHUTDOWNPARAMS removed
 *@@changed V0.9.0 [umoeller]: this used to be an XFldDesktop instance method
 *@@changed V0.9.1 (99-12-10) [umoeller]: fixed KERNELGLOBALS locks
 *@@changed V0.9.1 (99-12-12) [umoeller]: fixed memory leak when shutdown was cancelled
 *@@changed V0.9.5 (2000-08-10) [umoeller]: added logoff support
 *@@changed V0.9.7 (2001-01-25) [umoeller]: this played the wrong sound, fixed
 */

BOOL xsdInitiateRestartWPS(BOOL fLogoff)        // in: if TRUE, perform logoff also
{
    BOOL                fStartShutdown = TRUE;
    PSHUTDOWNPARAMS     psdp = (PSHUTDOWNPARAMS)malloc(sizeof(SHUTDOWNPARAMS));

    if (xsdIsShutdownRunning())
        // shutdown thread already running: return!
        fStartShutdown = FALSE;

    // lock shutdown menu items
    G_fShutdownRunning = TRUE;

    if (fStartShutdown)
    {
        ULONG flShutdown = 0;
#ifndef __NOXSHUTDOWN__
        flShutdown = cmnQuerySetting(sflXShutdown);
#endif

        memset(psdp, 0, sizeof(SHUTDOWNPARAMS));
        psdp->optReboot =  FALSE;
        psdp->ulRestartWPS = (fLogoff) ? 2 : 1; // V0.9.5 (2000-08-10) [umoeller]
        psdp->optWPSCloseWindows = ((flShutdown & XSD_WPS_CLOSEWINDOWS) != 0);
        psdp->optWPSReuseStartupFolder = psdp->optWPSCloseWindows;
        psdp->optConfirm = (!(flShutdown & XSD_NOCONFIRM));
        psdp->optAutoCloseVIO = ((flShutdown & XSD_AUTOCLOSEVIO) != 0);
        psdp->optWarpCenterFirst = ((flShutdown & XSD_WARPCENTERFIRST) != 0);
        psdp->optLog =  ((flShutdown & XSD_LOG) != 0);
        #ifdef DEBUG_SHUTDOWN
            psdp->optDebug = doshQueryShiftState();
        #else
            psdp->optDebug = FALSE;
        #endif

        if (psdp->optConfirm)
        {
            ULONG ulReturn = xsdConfirmRestartWPS(psdp);
            if (ulReturn != DID_OK)
                fStartShutdown = FALSE;
        }
    }

    StartShutdownThread(fStartShutdown,
                        TRUE,  // fPlayRestartDesktopSound
                        psdp);

    return (fStartShutdown);
}

/*
 *@@ xsdInitiateShutdownExt:
 *      just like the XFldDesktop method, but this one
 *      allows setting all the shutdown parameters by
 *      using the SHUTDOWNPARAMS structure. This is used
 *      for calling XShutdown externally, which is done
 *      by sending T1M_EXTERNALSHUTDOWN to the thread-1
 *      object window (see kernel.c).
 *
 *      NOTE: The memory block pointed to by psdp is
 *      not released by this function.
 *
 *@@changed V0.9.2 (2000-02-28) [umoeller]: fixed KERNELGLOBALS locks
 *@@changed V0.9.7 (2001-01-25) [umoeller]: rearranged for setup strings
 */

BOOL xsdInitiateShutdownExt(PSHUTDOWNPARAMS psdpShared)
{
    BOOL                fStartShutdown = TRUE;
    // PCGLOBALSETTINGS    pGlobalSettings = cmnQueryGlobalSettings();
    PSHUTDOWNPARAMS     psdpNew = NULL;

    if (xsdIsShutdownRunning())
        // shutdown thread already running: return!
        fStartShutdown = FALSE;

    // lock shutdown menu items
    G_fShutdownRunning = TRUE;

    if (psdpShared == NULL)
        fStartShutdown = FALSE;

    if (fStartShutdown)
    {
        psdpNew = (PSHUTDOWNPARAMS)malloc(sizeof(SHUTDOWNPARAMS));
        if (!psdpNew)
            fStartShutdown = FALSE;
        else
        {
            memcpy(psdpNew, psdpShared, sizeof(SHUTDOWNPARAMS));

            if (psdpNew->optConfirm)
            {
                // confirmations are on: display proper
                // confirmation dialog
                ULONG ulReturn;
                if (psdpNew->ulRestartWPS)
                    ulReturn = xsdConfirmRestartWPS(psdpNew);
                else
                    ulReturn = xsdConfirmShutdown(psdpNew);

                if (ulReturn != DID_OK)
                    fStartShutdown = FALSE;
            }

            StartShutdownThread(fStartShutdown,
                                FALSE,  // fPlayRestartDesktopSound
                                psdpNew);
        }
    }

    return (TRUE);
}

/* ******************************************************************
 *
 *   Shutdown settings pages
 *
 ********************************************************************/

#ifndef __NOXSHUTDOWN__

static CONTROLDEF
    ShutdownGroup = CONTROLDEF_GROUP(
                            LOAD_STRING, // "File menus",
                            ID_SDDI_SHUTDOWNGROUP,
                            -1,
                            -1),
    RebootAfterwardsCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING, // "~Reboot afterwards"
                            ID_SDDI_REBOOT,
                            200,
                            -1),
    RebootActionsButton = CONTROLDEF_PUSHBUTTON(
                            LOAD_STRING, // "Reboot actio~ns..."
                            ID_SDDI_REBOOTEXT,
                            200,
                            30),
    CanDesktopAltF4 = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,
                            ID_SDDI_CANDESKTOPALTF4,
                            -1,
                            -1),
    AnimationBeforeShutdownCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING, // "An~imation before shutdown"
                            ID_SDDI_ANIMATE_SHUTDOWN,
                            -1,
                            -1),
    AnimationBeforeRebootCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING, // "An~imation before reboot"
                            ID_SDDI_ANIMATE_REBOOT,
                            -1,
                            -1),
    PowerOffCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING, // "Attempt APM ~1.2 power-off"
                            ID_SDDI_APMPOWEROFF,
                            200,
                            -1),
    PowerOffDelayCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING, // "Dela~y"
                            ID_SDDI_DELAY,
                            -1,
                            -1),
    APMLevelText1 = CONTROLDEF_TEXT(
                            LOAD_STRING, // "APM level:"
                            ID_SDDI_APMVERSION_TXT,
                            100,
                            -1),
    APMLevelText2 = CONTROLDEF_TEXT(
                            LOAD_STRING, // "unknown"
                            ID_SDDI_APMVERSION,
                            -1,
                            -1),
    APMDriverText1 = CONTROLDEF_TEXT(
                            LOAD_STRING, // "APM.SYS driver:"
                            ID_SDDI_APMSYS_TXT,
                            100,
                            -1),
    APMDriverText2 = CONTROLDEF_TEXT(
                            LOAD_STRING, // "unknown"
                            ID_SDDI_APMSYS,
                            -1,
                            -1),
#ifndef __EASYSHUTDOWN__
    SaveINIFilesText = CONTROLDEF_TEXT(
                            LOAD_STRING, // "Save INI files:"
                            ID_SDDI_SAVEINIS_TXT,
                            -1,
                            -1),
    SaveINIFilesCombo =
        {
            WC_COMBOBOX,
            "",
            CBS_DROPDOWNLIST | LS_HORZSCROLL | WS_TABSTOP | WS_VISIBLE,
            ID_SDDI_SAVEINIS_LIST,
            CTL_COMMON_FONT,
            0,
            {300, 20},
            COMMON_SPACING
        },
#endif
    SharedGroup = CONTROLDEF_GROUP(
                            LOAD_STRING, // "Shutdown and Desktop restart settings"
                            ID_SDDI_SHAREDGROUP,
                            -1,
                            -1),
    EmptyTrashCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING, // "~Empty trash can"
                            ID_SDDI_EMPTYTRASHCAN,
                            -1,
                            -1),
    ConfirmCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING, // "~Confirm"
                            ID_SDDI_CONFIRM,
                            -1,
                            -1),
    WarpCenterFirstCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING, // "Close ~WarpCenter before Desktop"
                            ID_SDDI_WARPCENTERFIRST,
                            -1,
                            -1),
    AutoCloseVIOCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING, // "Auto-close non-~PM sessions"
                            ID_SDDI_AUTOCLOSEVIO,
                            200,
                            -1),
    AutoCloseButton = CONTROLDEF_PUSHBUTTON(
                            LOAD_STRING, // "Auto-close det~ails..."
                            ID_SDDI_AUTOCLOSEDETAILS,
                            200,
                            30),
    LogCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING, // "Write XSHUTDWN.LO~G file"
                            ID_SDDI_LOG,
                            -1,
                            -1),
    CreateShutdownFldrButton = CONTROLDEF_PUSHBUTTON(
                            LOAD_STRING, // "Create XShutdown folder"
                            ID_SDDI_CREATESHUTDOWNFLDR,
                            300,
                            30);

static const DLGHITEM dlgShutdown[] =
    {
        START_TABLE,            // root table, required
            START_ROW(0),       // shared settings group
                START_GROUP_TABLE(&SharedGroup),
                    START_ROW(0),
                        CONTROL_DEF(&EmptyTrashCB),
                    START_ROW(0),
                        CONTROL_DEF(&ConfirmCB),
                    START_ROW(0),
                        CONTROL_DEF(&WarpCenterFirstCB),
                    START_ROW(ROW_VALIGN_CENTER),
                        CONTROL_DEF(&AutoCloseVIOCB),
                        CONTROL_DEF(&AutoCloseButton),
                    START_ROW(0),
                        CONTROL_DEF(&LogCB),
                    START_ROW(0),
                        CONTROL_DEF(&CreateShutdownFldrButton),
                END_TABLE,      // end of group
            START_ROW(0),       // shutdown only group
                // create group on top
                START_GROUP_TABLE(&ShutdownGroup),
                    START_ROW(0),
                        CONTROL_DEF(&CanDesktopAltF4),
                    START_ROW(ROW_VALIGN_CENTER),
                        CONTROL_DEF(&RebootAfterwardsCB),
                        CONTROL_DEF(&RebootActionsButton),
                    START_ROW(0),
                        CONTROL_DEF(&AnimationBeforeShutdownCB),
                    START_ROW(0),
                        CONTROL_DEF(&AnimationBeforeRebootCB),
                    START_ROW(0),
                        START_TABLE,
                            START_ROW(0),
                                CONTROL_DEF(&PowerOffCB),
                            START_ROW(0),
                                CONTROL_DEF(&G_Spacing),        // notebook.c
                                CONTROL_DEF(&PowerOffDelayCB),
                        END_TABLE,
                        START_TABLE,
                            START_ROW(0),
                                CONTROL_DEF(&APMLevelText1),
                                CONTROL_DEF(&APMLevelText2),
                            START_ROW(0),
                                CONTROL_DEF(&APMDriverText1),
                                CONTROL_DEF(&APMDriverText2),
                        END_TABLE,
#ifndef __EASYSHUTDOWN__
                    START_ROW(ROW_VALIGN_CENTER),
                        CONTROL_DEF(&SaveINIFilesText),
                        CONTROL_DEF(&SaveINIFilesCombo),
#endif
                END_TABLE,      // end of group
            START_ROW(0),       // notebook buttons (will be moved)
                CONTROL_DEF(&G_UndoButton),         // notebook.c
                CONTROL_DEF(&G_DefaultButton),      // notebook.c
                CONTROL_DEF(&G_HelpButton),         // notebook.c
        END_TABLE
    };

static const XWPSETTING G_ShutdownBackup[] =
    {
        sflXShutdown,
#ifndef __EASYSHUTDOWN__
        sulSaveINIS,
#endif
    };

/*
 * xsdShutdownInitPage:
 *      notebook callback function (notebook.c) for the
 *      "XShutdown" page in the Desktop's settings
 *      notebook.
 *      Sets the controls on the page according to the
 *      Global Settings.
 *
 *@@added V0.9.0 [umoeller]
 *@@changed V0.9.2 (2000-03-04) [umoeller]: added "APM delay" support
 *@@changed V0.9.3 (2000-05-22) [umoeller]: added animate on reboot
 *@@changed V0.9.4 (2000-08-03) [umoeller]: added "empty trash can"
 *@@changed V0.9.9 (2001-04-07) [pr]: added missing Undo and Default processing
 *@@changed V0.9.16 (2001-10-08) [umoeller]: now using dialog formatter
 *@@changed V0.9.16 (2002-01-04) [umoeller]: added "alt+f4 on desktop starts shutdown"
 */

VOID xsdShutdownInitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                         ULONG flFlags)        // CBI_* flags (notebook.h)
{
    ULONG fl = cmnQuerySetting(sflXShutdown);

    if (flFlags & CBI_INIT)
    {
        // PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();
        APIRET      arc = NO_ERROR;
        HWND        hwndINICombo = NULLHANDLE;
        ULONG       ul;
        PEXECUTABLE pExec;
        CHAR        szAPMVersion[30];
        CHAR        szAPMSysFile[CCHMAXPATH];

        ULONG       aulIniStrings[3] =
            {
                ID_XSSI_XSD_SAVEINIS_NEW,   // pszXSDSaveInisNew
                ID_XSSI_XSD_SAVEINIS_OLD,   // pszXSDSaveInisOld
                ID_XSSI_XSD_SAVEINIS_NONE   // pszXSDSaveInisNone
            };

        if (pcnbp->pUser == NULL)
        {
            // first call: backup Global Settings for "Undo" button;
            // this memory will be freed automatically by the
            // common notebook window function (notebook.c) when
            // the notebook page is destroyed
            /*
            pcnbp->pUser = malloc(sizeof(GLOBALSETTINGS));
            memcpy(pcnbp->pUser, pGlobalSettings, sizeof(GLOBALSETTINGS));
               */
            pcnbp->pUser = cmnBackupSettings(G_ShutdownBackup,
                                             ARRAYITEMCOUNT(G_ShutdownBackup));

            // insert the controls using the dialog formatter
            // V0.9.16 (2001-10-08) [umoeller]
            ntbFormatPage(pcnbp->hwndDlgPage,
                          dlgShutdown,
                          ARRAYITEMCOUNT(dlgShutdown));
        }

        sprintf(szAPMVersion, "APM %s", apmQueryVersion());
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_SDDI_APMVERSION, szAPMVersion);
        sprintf(szAPMSysFile,
                "%c:\\OS2\\BOOT\\APM.SYS",
                doshQueryBootDrive());
        #ifdef DEBUG_SHUTDOWN
            _Pmpf(("Opening %s", szAPMSysFile));
        #endif

        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_SDDI_APMSYS,
                          "Error");

        if (!(arc = exehOpen(szAPMSysFile,
                                 &pExec)))
        {
            if (!(arc = exehQueryBldLevel(pExec)))
            {
                if (pExec->pszVersion)
                    WinSetDlgItemText(pcnbp->hwndDlgPage, ID_SDDI_APMSYS,
                                      pExec->pszVersion);

            }

            exehClose(&pExec);
        }

#ifndef __EASYSHUTDOWN__
        hwndINICombo = WinWindowFromID(pcnbp->hwndDlgPage, ID_SDDI_SAVEINIS_LIST);
        for (ul = 0;
             ul < 3;
             ul++)
        {
            WinInsertLboxItem(hwndINICombo,
                              ul,
                              cmnGetString(aulIniStrings[ul]));
        }
#endif
    }

    if (flFlags & CBI_SET)
    {
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_SDDI_REBOOT,
                              (fl & XSD_REBOOT) != 0);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_SDDI_CANDESKTOPALTF4,
                              (fl & XSD_CANDESKTOPALTF4) != 0);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_SDDI_ANIMATE_SHUTDOWN,
                              (fl & XSD_ANIMATE_SHUTDOWN) != 0);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_SDDI_ANIMATE_REBOOT,
                              (fl & XSD_ANIMATE_REBOOT) != 0);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_SDDI_APMPOWEROFF,
                              (apmPowerOffSupported())
                                  ? ((fl & XSD_APMPOWEROFF) != 0)
                                  : FALSE
                              );
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_SDDI_DELAY,
                              (fl & XSD_APM_DELAY) != 0);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_SDDI_EMPTYTRASHCAN,
                              (fl & XSD_EMPTY_TRASH) != 0);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_SDDI_CONFIRM,
                              ((fl & XSD_NOCONFIRM) == 0));
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_SDDI_AUTOCLOSEVIO,
                              (fl & XSD_AUTOCLOSEVIO) != 0);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_SDDI_WARPCENTERFIRST,
                              (fl & XSD_WARPCENTERFIRST) != 0);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_SDDI_LOG,
                              (fl & XSD_LOG) != 0);

#ifndef __EASYSHUTDOWN__
        WinSendDlgItemMsg(pcnbp->hwndDlgPage, ID_SDDI_SAVEINIS_LIST,
                          LM_SELECTITEM,
                          (MPARAM)(cmnQuerySetting(sulSaveINIS)),
                          (MPARAM)TRUE);        // select
#endif
    }

    if (flFlags & CBI_ENABLE)
    {
        PCKERNELGLOBALS pKernelGlobals = krnQueryGlobals();
        BOOL fXShutdownValid = TRUE; // (cmnQuerySetting(sNoWorkerThread) == 0);
        BOOL fXShutdownEnabled =
                (   (fXShutdownValid)
                 && (cmnQuerySetting(sfXShutdown))
                );
        BOOL fXShutdownOrWPSValid =
                (   (   (cmnQuerySetting(sfXShutdown))
                     || (cmnQuerySetting(sfRestartDesktop))
                    )
                 // && (cmnQuerySetting(sNoWorkerThread) == 0)
                );

        // winhEnableDlgItem(pcnbp->hwndDlgPage, ID_SDDI_ENABLED, fXShutdownValid);
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_SDDI_CANDESKTOPALTF4, fXShutdownEnabled);

        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_SDDI_REBOOT,  fXShutdownEnabled);
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_SDDI_REBOOTEXT, fXShutdownEnabled);

        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_SDDI_ANIMATE_SHUTDOWN, fXShutdownEnabled);
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_SDDI_ANIMATE_REBOOT, fXShutdownEnabled);

        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_SDDI_APMPOWEROFF,
                          ( fXShutdownEnabled && apmPowerOffSupported() ) );
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_SDDI_DELAY,
                          (      fXShutdownEnabled
                              && (apmPowerOffSupported())
                              && ((fl & XSD_APMPOWEROFF) != 0)
                          ));

        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_SDDI_EMPTYTRASHCAN,
                          ( fXShutdownEnabled && (cmnTrashCanReady()) ) );

        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_SDDI_CONFIRM, fXShutdownOrWPSValid);
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_SDDI_AUTOCLOSEVIO, fXShutdownOrWPSValid);
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_SDDI_AUTOCLOSEDETAILS, fXShutdownOrWPSValid);

        // enable "warpcenter first" if shutdown or WPS have been enabled
        // AND if the WarpCenter was found
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_SDDI_WARPCENTERFIRST,
                          ((fXShutdownOrWPSValid)
                          && (pKernelGlobals->pAwakeWarpCenter != NULL)));
                                 // @@todo this doesn't find the WarpCenter
                                 // if started thru CONFIG.SYS

        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_SDDI_LOG, fXShutdownOrWPSValid);

#ifndef __EASYSHUTDOWN__
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_SDDI_SAVEINIS_TXT, fXShutdownEnabled);
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_SDDI_SAVEINIS_LIST, fXShutdownEnabled);
#endif

        if (WinQueryObject((PSZ)XFOLDER_SHUTDOWNID))
            // shutdown folder exists already: disable button
            winhEnableDlgItem(pcnbp->hwndDlgPage, ID_SDDI_CREATESHUTDOWNFLDR, FALSE);
    }
}

/*
 * xsdShutdownItemChanged:
 *      notebook callback function (notebook.c) for the
 *      "XShutdown" page in the Desktop's settings
 *      notebook.
 *      Reacts to changes of any of the dialog controls.
 *
 *@@added V0.9.0 [umoeller]
 *@@changed V0.9.2 (2000-03-04) [umoeller]: added "APM delay" support
 *@@changed V0.9.3 (2000-05-22) [umoeller]: added animate on reboot
 *@@changed V0.9.4 (2000-08-03) [umoeller]: added "empty trash can"
 *@@changed V0.9.9 (2001-04-07) [pr]: added missing Undo and Default processing
 */

MRESULT xsdShutdownItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                               ULONG ulItemID,
                               USHORT usNotifyCode,
                               ULONG ulExtra)      // for checkboxes: contains new state
{
    ULONG ulChange = 1;
    ULONG ulFlag = -1;
#ifndef __EASYSHUTDOWN__
    ULONG ulSaveINIS = -1;
#endif

    switch (ulItemID)
    {
        case ID_SDDI_REBOOT:
            ulFlag = XSD_REBOOT;
        break;

        case ID_SDDI_CANDESKTOPALTF4:
            ulFlag = XSD_CANDESKTOPALTF4;
        break;

        case ID_SDDI_ANIMATE_SHUTDOWN:
            ulFlag = XSD_ANIMATE_SHUTDOWN;
        break;

        case ID_SDDI_ANIMATE_REBOOT:
            ulFlag = XSD_ANIMATE_REBOOT;
        break;

        case ID_SDDI_APMPOWEROFF:
            ulFlag = XSD_APMPOWEROFF;
        break;

        case ID_SDDI_DELAY:
            ulFlag = XSD_APM_DELAY;
        break;

        case ID_SDDI_EMPTYTRASHCAN:
            ulFlag = XSD_EMPTY_TRASH;
        break;

        case ID_SDDI_CONFIRM:
            ulFlag = XSD_NOCONFIRM;
            ulExtra = 1 - ulExtra;          // this one is reverse now
                                            // V0.9.16 (2002-01-13) [umoeller]
        break;

        case ID_SDDI_AUTOCLOSEVIO:
            ulFlag = XSD_AUTOCLOSEVIO;
        break;

        case ID_SDDI_WARPCENTERFIRST:
            ulFlag = XSD_WARPCENTERFIRST;
        break;

        case ID_SDDI_LOG:
            ulFlag = XSD_LOG;
        break;

#ifndef __EASYSHUTDOWN__
        case ID_SDDI_SAVEINIS_LIST:
        {
            ULONG ul = (ULONG)WinSendDlgItemMsg(pcnbp->hwndDlgPage, ID_SDDI_SAVEINIS_LIST,
                                                LM_QUERYSELECTION,
                                                MPFROMSHORT(LIT_FIRST),
                                                0);
            if (ul >= 0 && ul <= 2)
                ulSaveINIS = ul;
        break; }
#endif

        // Reboot Actions (Desktop page 1)
        case ID_SDDI_REBOOTEXT:
            WinDlgBox(HWND_DESKTOP,         // parent is desktop
                      pcnbp->hwndFrame,                  // owner
                      (PFNWP)fnwpUserRebootOptions,     // dialog procedure
                      cmnQueryNLSModuleHandle(FALSE),
                      ID_XSD_REBOOTEXT,        // dialog resource id
                      (PVOID)NULL);            // no dialog parameters
            ulChange = 0;
        break;

        // Auto-close details (Desktop page 1)
        case ID_SDDI_AUTOCLOSEDETAILS:
            WinDlgBox(HWND_DESKTOP,         // parent is desktop
                      pcnbp->hwndFrame,             // owner
                      (PFNWP)fnwpAutoCloseDetails,    // dialog procedure
                      cmnQueryNLSModuleHandle(FALSE),  // from resource file
                      ID_XSD_AUTOCLOSE,        // dialog resource id
                      (PVOID)NULL);            // no dialog parameters
            ulChange = 0;
        break;

        // "Create shutdown folder"
        case ID_SDDI_CREATESHUTDOWNFLDR:
        {
            CHAR    szSetup[500];
            HOBJECT hObj = 0;
            sprintf(szSetup,
                "DEFAULTVIEW=ICON;ICONVIEW=NONFLOWED,MINI;"
                "OBJECTID=%s;",
                XFOLDER_SHUTDOWNID);
            if (hObj = WinCreateObject((PSZ)G_pcszXFldShutdown,
                                       "XFolder Shutdown",
                                       szSetup,
                                       (PSZ)WPOBJID_DESKTOP, // "<WP_DESKTOP>",
                                       CO_UPDATEIFEXISTS))
                winhEnableDlgItem(pcnbp->hwndDlgPage, ID_SDDI_CREATESHUTDOWNFLDR, FALSE);
            else
                cmnMessageBoxMsg(pcnbp->hwndFrame,
                                 104, 106,
                                 MB_OK);
            ulChange = 0;
        break; }

        case DID_UNDO:
            // "Undo" button: get pointer to backed-up Global Settings
            cmnRestoreSettings(pcnbp->pUser,
                               ARRAYITEMCOUNT(G_ShutdownBackup));
            // update the display by calling the INIT callback
            pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
        break;

        case DID_DEFAULT:
            // set the default settings for this settings page
            // (this is in common.c because it's also used at
            // Desktop startup)
            cmnSetDefaultSettings(pcnbp->ulPageID);
            // update the display by calling the INIT callback
            pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
        break;

        default:
            ulChange = 0;
    }

    if (   (ulFlag != -1)
#ifndef __EASYSHUTDOWN__
        || (ulSaveINIS != -1)
#endif
       )
    {
        if (ulFlag != -1)
        {
            ULONG flShutdown = cmnQuerySetting(sflXShutdown);
            if (ulExtra)
                flShutdown |= ulFlag;
            else
                flShutdown &= ~ulFlag;
            cmnSetSetting(sflXShutdown, flShutdown);
        }
#ifndef __EASYSHUTDOWN__
        if (ulSaveINIS != -1)
            cmnSetSetting(sulSaveINIS, ulSaveINIS);;
#endif
    }

    if (ulChange)
    {
        // enable/disable items
        xsdShutdownInitPage(pcnbp, CBI_ENABLE);
        // cmnStoreGlobalSettings();
    }

    return ((MPARAM)0);
}

#endif

/* ******************************************************************
 *
 *   Shutdown helper functions
 *
 ********************************************************************/

/*
 * xsdLog:
 *      common function for writing into the XSHUTDWN.LOG file.
 *
 *added V0.9.0 [umoeller]
 *removed V0.9.16 (2001-11-22) [umoeller]
 */

/* void xsdLog(FILE *File,
            const char* pcszFormatString,
            ...)
{
    if (File)
    {
        va_list vargs;

        DATETIME dt;
        // CHAR szTemp[2000];
        // ULONG   cbWritten;
        DosGetDateTime(&dt);
        fprintf(File, "Time: %02d:%02d:%02d.%02d ",
                dt.hours, dt.minutes, dt.seconds, dt.hundredths);

        // get the variable parameters
        va_start(vargs, pcszFormatString);
        vfprintf(File, pcszFormatString, vargs);
        va_end(vargs);
        fflush(File);
    }
} */

/*
 *@@ xsdLoadAnimation:
 *      this loads the shutdown (traffic light) animation
 *      as an array of icons from the XFLDR.DLL module.
 */

VOID xsdLoadAnimation(PSHUTDOWNANIM psda)
{
    HMODULE hmod = cmnQueryMainResModuleHandle();
    (psda->ahptr)[0] = WinLoadPointer(HWND_DESKTOP, hmod, ID_ICONSDANIM1);
    (psda->ahptr)[1] = WinLoadPointer(HWND_DESKTOP, hmod, ID_ICONSDANIM2);
    (psda->ahptr)[2] = WinLoadPointer(HWND_DESKTOP, hmod, ID_ICONSDANIM3);
    (psda->ahptr)[3] = WinLoadPointer(HWND_DESKTOP, hmod, ID_ICONSDANIM4);
    (psda->ahptr)[4] = WinLoadPointer(HWND_DESKTOP, hmod, ID_ICONSDANIM5);
    (psda->ahptr)[5] = WinLoadPointer(HWND_DESKTOP, hmod, ID_ICONSDANIM4);
    (psda->ahptr)[6] = WinLoadPointer(HWND_DESKTOP, hmod, ID_ICONSDANIM3);
    (psda->ahptr)[7] = WinLoadPointer(HWND_DESKTOP, hmod, ID_ICONSDANIM2);
}

/*
 *@@ xsdFreeAnimation:
 *      this frees the animation loaded by xsdLoadAnimation.
 */

VOID xsdFreeAnimation(PSHUTDOWNANIM psda)
{
    USHORT us;
    for (us = 0; us < XSD_ANIM_COUNT; us++)
    {
        WinDestroyPointer((psda->ahptr)[us]);
        (psda->ahptr)[us] = NULLHANDLE;
    }
}

/*
 *@@ xsdRestartWPS:
 *      terminated the WPS process, which will lead
 *      to a Desktop restart.
 *
 *      If (fLogoff == TRUE), this will perform a logoff
 *      as well. If XWPShell is not running, this flag
 *      has no effect.
 *
 *      Runs on the Shutdown thread.
 *
 *@@changed V0.9.5 (2000-08-10) [umoeller]: added XWPSHELL.EXE interface
 */

VOID xsdRestartWPS(HAB hab,
                   BOOL fLogoff)    // in: if TRUE, perform logoff as well.
{
    ULONG ul;

    PCKERNELGLOBALS pcKernelGlobals = krnQueryGlobals();

    // wait a maximum of 2 seconds while there's still
    // a system sound playing
    for (ul = 0; ul < 20; ul++)
        if (xmmIsBusy())
            DosSleep(100);
        else
            break;

    // close leftover open devices
    xmmCleanup();

    if (pcKernelGlobals->pXWPShellShared)
    {
        // XWPSHELL.EXE running:
        PXWPSHELLSHARED pXWPShellShared
            = (PXWPSHELLSHARED)pcKernelGlobals->pXWPShellShared;
        // set flag in shared memory; XWPSHELL
        // will check this once the WPS has terminated
        pXWPShellShared->fNoLogonButRestart = !fLogoff;
    }

    // terminate the current process,
    // which is PMSHELL.EXE. We shouldn't use DosExit()
    // directly, because this might mess up the
    // C runtime library... even though this doesn't
    // help much with the rest of the WPS.
    exit(0);        // 0 == no error
}

/*
 *@@ xsdFlushWPS2INI:
 *      this forces the WPS to flush its internal buffers
 *      into OS2.INI/OS2SYS.INI. We call this function
 *      after we have closed all the WPS windows, before
 *      we actually save the INI files.
 *
 *      This undocumented semaphore was published in some
 *      newsgroup ages ago, I don't remember.
 *
 *      Returns APIRETs of event semaphore calls.
 *
 *@@added V0.9.0 (99-10-22) [umoeller]
 */

APIRET xsdFlushWPS2INI(VOID)
{
    APIRET arc  = 0;
    HEV hev = NULLHANDLE;

    arc = DosOpenEventSem("\\SEM32\\WORKPLAC\\LAZYWRIT.SEM", &hev);
    if (arc == NO_ERROR)
    {
        arc = DosPostEventSem(hev);
        DosCloseEventSem(hev);
    }

    return (arc);
}

/* ******************************************************************
 *
 *   Additional declarations for Shutdown thread
 *
 ********************************************************************/

// current shutdown status
#define XSD_IDLE                0       // not started yet
#define XSD_CLOSING             1       // currently closing windows
#define XSD_ALLDONEOK           2       // all windows closed
#define XSD_CANCELLED           3       // user pressed cancel

/*
 *@@ SHUTDOWNDATA:
 *      shutdown instance data allocated from the heap
 *      while the shutdown thread (fntShutdown) is
 *      running.
 *
 *      This replaces the sick set of global variables
 *      which used to be all over the place before V0.9.9
 *      and fixes a number of serialization problems on
 *      the way.
 *
 *@@added V0.9.9 (2001-03-07) [umoeller]
 */

typedef struct _SHUTDOWNDATA
{
    // shutdown parameters
    SHUTDOWNPARAMS  sdParams;

    ULONG           ulMaxItemCount,
                    ulLastItemCount;

    PXFILE          ShutdownLogFile;        // changed V0.9.16 (2001-11-22) [umoeller]

    ULONG           ulStatus;
                // one of:
                // -- XSD_IDLE: shutdown still preparing, not closing yet
                // -- XSD_CLOSING: currently closing applications
                // -- XSD_SAVINGWPS: all windows closed, probably saving WPS now
                // -- XSD_ALLDONEOK: done saving WPS too, fnwpShutdownThread is exiting
                // -- XSD_CANCELLED: shutdown has been cancelled by user

    /* BOOL            fAllWindowsClosed,
                    fClosingApps,
                    fShutdownBegun; */

    HAB             habShutdownThread;
    // HMQ             hmqShutdownThread;       // removed V0.9.12 (2001-05-29) [umoeller]

    HMODULE         hmodResource;

    HWND            hwndProgressBar;        // progress bar in status window

    // flags for whether we're currently owning semaphores
    BOOL            // fAwakeObjectsSemOwned,           // removed V0.9.9 (2001-04-04) [umoeller]
                    fShutdownSemOwned,
                    fSkippedSemOwned;

    // BOOL            fPrepareSaveWPSPosted;

    ULONG           hPOC;

    // this is the global list of items to be closed (SHUTLISTITEMs)
    LINKLIST        llShutdown,
    // and the list of items that are to be skipped
                    llSkipped;

    HMTX            hmtxShutdown,
                    hmtxSkipped;
    HEV             hevUpdated;

    // temporary storage for closing VIOs
    CHAR            szVioTitle[1000];
    SHUTLISTITEM    VioItem;

    // global linked list of auto-close VIO windows (AUTOCLOSELISTITEM)
    LINKLIST        llAutoClose;

    ULONG           sidWPS,
                    sidPM;

    SHUTDOWNCONSTS  SDConsts;

} SHUTDOWNDATA, *PSHUTDOWNDATA;

VOID xsdFinishShutdown(PSHUTDOWNDATA pShutdownData);
VOID xsdFinishStandardMessage(PSHUTDOWNDATA pShutdownData);
VOID xsdFinishStandardReboot(PSHUTDOWNDATA pShutdownData);
VOID xsdFinishUserReboot(PSHUTDOWNDATA pShutdownData);
VOID xsdFinishAPMPowerOff(PSHUTDOWNDATA pShutdownData);

/* ******************************************************************
 *
 *   XShutdown data maintenance
 *
 ********************************************************************/

/*
 *@@ xsdGetShutdownConsts:
 *      prepares a number of constants in the specified
 *      SHUTDOWNCONSTS structure which are used throughout
 *      XShutdown.
 *
 *      SHUTDOWNCONSTS is part of SHUTDOWNDATA, so this
 *      func gets called once when fntShutdownThread starts
 *      up. However, since this is also used externally,
 *      we have put these fields into a separate structure.
 *
 *@@added V0.9.9 (2001-03-07) [umoeller]
 */

VOID xsdGetShutdownConsts(PSHUTDOWNCONSTS pConsts)
{
    pConsts->pKernelGlobals = krnQueryGlobals();
    pConsts->pWPDesktop = _WPDesktop;
    pConsts->pActiveDesktop = _wpclsQueryActiveDesktop(pConsts->pWPDesktop);
    pConsts->hwndActiveDesktop = _wpclsQueryActiveDesktopHWND(pConsts->pWPDesktop);
    pConsts->hwndOpenWarpCenter = NULLHANDLE;

    WinQueryWindowProcess(pConsts->hwndActiveDesktop,
                          &pConsts->pidWPS,
                          NULL);
    WinQueryWindowProcess(HWND_DESKTOP,
                          &pConsts->pidPM,
                          NULL);

    if (pConsts->pKernelGlobals->pAwakeWarpCenter)
    {
        // WarpCenter is awake: check if it's open
        PUSEITEM pUseItem;
        for (pUseItem = _wpFindUseItem(pConsts->pKernelGlobals->pAwakeWarpCenter,
                                       USAGE_OPENVIEW, NULL);
             pUseItem;
             pUseItem = _wpFindUseItem(pConsts->pKernelGlobals->pAwakeWarpCenter, USAGE_OPENVIEW,
                                       pUseItem))
        {
            PVIEWITEM pViewItem = (PVIEWITEM)(pUseItem+1);
            if (pViewItem->view == OPEN_RUNNING)
            {
                pConsts->hwndOpenWarpCenter = pViewItem->handle;
                break;
            }
        }
    }
}

/*
 *@@ xsdItemFromPID:
 *      searches a given LINKLIST of SHUTLISTITEMs
 *      for a process ID.
 *
 *@@changed V0.9.0 [umoeller]: adjusted for new linklist.c functions
 */

PSHUTLISTITEM xsdItemFromPID(PLINKLIST pList,
                             PID pid,
                             HMTX hmtx)
{
    PSHUTLISTITEM   pItem = NULL;
    BOOL            fAccess = FALSE,
                    fSemOwned = FALSE;

    TRY_QUIET(excpt1)
    {
        if (hmtx)
        {
            fSemOwned = (WinRequestMutexSem(hmtx, SEM_INDEFINITE_WAIT) == NO_ERROR);
            fAccess = fSemOwned;
        }
        else
            fAccess = TRUE;

        if (fAccess)
        {
            PLISTNODE pNode = lstQueryFirstNode(pList);
            while (pNode)
            {
                pItem = pNode->pItemData;
                if (pItem->swctl.idProcess == pid)
                    break;

                pNode = pNode->pNext;
                pItem = 0;
            }
        }
    }
    CATCH(excpt1) { } END_CATCH();

    if (fSemOwned)
    {
        DosReleaseMutexSem(hmtx);
        fSemOwned = FALSE;
    }

    return (pItem);
}

/*
 *@@ xsdItemFromSID:
 *      searches a given LINKLIST of SHUTLISTITEMs
 *      for a session ID.
 *
 *@@changed V0.9.0 [umoeller]: adjusted for new linklist.c functions
 */

PSHUTLISTITEM xsdItemFromSID(PLINKLIST pList,
                             ULONG sid,
                             HMTX hmtx,
                             ULONG ulTimeout)
{
    PSHUTLISTITEM pItem = NULL;
    BOOL          fAccess = FALSE,
                  fSemOwned = FALSE;

    TRY_QUIET(excpt1)
    {
        if (hmtx)
        {
            fSemOwned = (WinRequestMutexSem(hmtx, ulTimeout) == NO_ERROR);
            fAccess = fSemOwned;
        }
        else
            fAccess = TRUE;

        if (fAccess)
        {
            PLISTNODE pNode = lstQueryFirstNode(pList);
            while (pNode)
            {
                pItem = pNode->pItemData;
                if (pItem->swctl.idSession == sid)
                    break;

                pNode = pNode->pNext;
                pItem = 0;
            }
        }
    }
    CATCH(excpt1) { } END_CATCH();

    if (fSemOwned)
    {
        DosReleaseMutexSem(hmtx);
        fSemOwned = FALSE;
    }

    return (pItem);
}

/*
 *@@ xsdCountRemainingItems:
 *      counts the items left to be closed by counting
 *      the window list items and subtracting the items
 *      which were skipped by the user.
 *
 *@@changed V0.9.0 [umoeller]: adjusted for new linklist.c functions
 */

ULONG xsdCountRemainingItems(PSHUTDOWNDATA pData)
{
    ULONG   ulrc = 0;
    BOOL    fShutdownSemOwned = FALSE,
            fSkippedSemOwned = FALSE;

    TRY_QUIET(excpt1)
    {
        fShutdownSemOwned = (WinRequestMutexSem(pData->hmtxShutdown, 4000) == NO_ERROR);
        fSkippedSemOwned = (WinRequestMutexSem(pData->hmtxSkipped, 4000) == NO_ERROR);
        if ( (fShutdownSemOwned) && (fSkippedSemOwned) )
            ulrc = (
                        lstCountItems(&pData->llShutdown)
                      - lstCountItems(&pData->llSkipped)
                   );
    }
    CATCH(excpt1) { } END_CATCH();

    if (fShutdownSemOwned)
    {
        DosReleaseMutexSem(pData->hmtxShutdown);
        fShutdownSemOwned = FALSE;
    }

    if (fSkippedSemOwned)
    {
        DosReleaseMutexSem(pData->hmtxSkipped);
        fSkippedSemOwned = FALSE;
    }

    return (ulrc);
}

/*
 *@@ xsdLongTitle:
 *      creates a descriptive string in pszTitle from pItem
 *      (for the main (debug) window listbox).
 */

void xsdLongTitle(PSZ pszTitle,
                  PSHUTLISTITEM pItem)
{
    sprintf(pszTitle, "%s%s",
            pItem->swctl.szSwtitle,
            (pItem->swctl.uchVisibility == SWL_VISIBLE)
                ? (", visible")
                : ("")
        );

    strcat(pszTitle, ", ");
    switch (pItem->swctl.bProgType)
    {
        case PROG_DEFAULT: strcat(pszTitle, "default"); break;
        case PROG_FULLSCREEN: strcat(pszTitle, "OS/2 FS"); break;
        case PROG_WINDOWABLEVIO: strcat(pszTitle, "OS/2 win"); break;
        case PROG_PM:
            strcat(pszTitle, "PM, class: ");
            strcat(pszTitle, pItem->szClass);
        break;
        case PROG_VDM: strcat(pszTitle, "VDM"); break;
        case PROG_WINDOWEDVDM: strcat(pszTitle, "VDM win"); break;
        default:
            sprintf(pszTitle+strlen(pszTitle), "? (%lX)");
        break;
    }
    sprintf(pszTitle+strlen(pszTitle),
            ", hwnd: 0x%lX, pid: 0x%lX, sid: 0x%lX, pObj: 0x%lX",
            (ULONG)pItem->swctl.hwnd,
            (ULONG)pItem->swctl.idProcess,
            (ULONG)pItem->swctl.idSession,
            (ULONG)pItem->pObject
        );
}

/*
 *@@ xsdQueryCurrentItem:
 *      returns the next PSHUTLISTITEM to be
 *      closed (skipping the items that were
 *      marked to be skipped).
 *
 *@@changed V0.9.0 [umoeller]: adjusted for new linklist.c functions
 */

PSHUTLISTITEM xsdQueryCurrentItem(PSHUTDOWNDATA pData)
{
    CHAR            szShutItem[1000],
                    szSkipItem[1000];
    BOOL            fShutdownSemOwned = FALSE,
                    fSkippedSemOwned = FALSE;
    PSHUTLISTITEM   pliShutItem = 0;

    TRY_QUIET(excpt1)
    {
        fShutdownSemOwned = (WinRequestMutexSem(pData->hmtxShutdown, 4000) == NO_ERROR);
        fSkippedSemOwned = (WinRequestMutexSem(pData->hmtxSkipped, 4000) == NO_ERROR);

        if ((fShutdownSemOwned) && (fSkippedSemOwned))
        {
            PLISTNODE pShutNode = lstQueryFirstNode(&pData->llShutdown);
            // pliShutItem = pliShutdownFirst;
            while (pShutNode)
            {
                PLISTNODE pSkipNode = lstQueryFirstNode(&pData->llSkipped);
                pliShutItem = pShutNode->pItemData;

                while (pSkipNode)
                {
                    PSHUTLISTITEM pliSkipItem = pSkipNode->pItemData;
                    xsdLongTitle(szShutItem, pliShutItem);
                    xsdLongTitle(szSkipItem, pliSkipItem);
                    if (!strcmp(szShutItem, szSkipItem))
                        /* current shut item is on skip list:
                           break (==> take next shut item */
                        break;

                    pSkipNode = pSkipNode->pNext;
                    pliSkipItem = 0;
                }

                if (pSkipNode == NULL)
                    // current item is not on the skip list:
                    // return this item
                    break;

                // current item was skipped: take next one
                pShutNode = pShutNode->pNext;
                pliShutItem = 0;
            }
        }
    }
    CATCH(excpt1) { } END_CATCH();

    if (fShutdownSemOwned)
    {
        DosReleaseMutexSem(pData->hmtxShutdown);
        fShutdownSemOwned = FALSE;
    }
    if (fSkippedSemOwned)
    {
        DosReleaseMutexSem(pData->hmtxSkipped);
        fSkippedSemOwned = FALSE;
    }

    return (pliShutItem);
}

/*
 *@@ xsdAppendShutListItem:
 *      this appends a new PSHUTLISTITEM to the given list
 *      and returns the address of the new item; the list
 *      to append to must be specified in *ppFirst / *ppLast.
 *
 *      NOTE: It is entirely the job of the caller to serialize
 *      access to the list, using mutex semaphores.
 *      The item to add is to be specified by swctl and possibly
 *      *pObject (if swctl describes an open Desktop object).
 *
 *      Since this gets called from xsdBuildShutList, this runs
 *      on both the Shutdown and Update threads.
 *
 *@@changed V0.9.0 [umoeller]: adjusted for new linklist.c functions
 *@@changed V0.9.4 (2000-07-11) [umoeller]: fixed bug in window class detection
 *@@changed V0.9.6 (2000-10-27) [umoeller]: fixed WarpCenter detection
 */

PSHUTLISTITEM xsdAppendShutListItem(PSHUTDOWNDATA pShutdownData,
                                    PLINKLIST pList,    // in/out: linked list to work on
                                    SWCNTRL* pswctl,    // in: tasklist entry to add
                                    WPObject *pObject,  // in: !=NULL: Desktop object
                                    LONG lSpecial)
{
    PSHUTLISTITEM  pNewItem = NULL;

    pNewItem = malloc(sizeof(SHUTLISTITEM));

    if (pNewItem)
    {
        pNewItem->pObject = pObject;
        memcpy(&pNewItem->swctl, pswctl, sizeof(SWCNTRL));

        strcpy(pNewItem->szClass, "unknown");

        pNewItem->lSpecial = lSpecial;

        if (pObject)
        {
            // for Desktop objects, store additional data
            if (wpshCheckObject(pObject))
            {
                strncpy(pNewItem->szClass,
                        (PSZ)_somGetClassName(pObject),
                        sizeof(pNewItem->szClass)-1);

                pNewItem->swctl.szSwtitle[0] = '\0';
                strncpy(pNewItem->swctl.szSwtitle,
                        _wpQueryTitle(pObject),
                        sizeof(pNewItem->swctl.szSwtitle)-1);

                // always set PID and SID to that of the WPS,
                // because the tasklist returns garbage for
                // Desktop objects
                pNewItem->swctl.idProcess = pShutdownData->SDConsts.pidWPS;
                pNewItem->swctl.idSession = pShutdownData->sidWPS;

                // set HWND to object-in-use-list data,
                // because the tasklist also returns garbage
                // for that
                if (_wpFindUseItem(pObject, USAGE_OPENVIEW, NULL))
                    pNewItem->swctl.hwnd = (_wpFindViewItem(pObject,
                                                            VIEW_ANY,
                                                            NULL))->handle;
            }
            else
            {
                // invalid object:
                pNewItem->pObject = NULL;
                strcpy(pNewItem->szClass, "wpshCheckObject failed");
            }
        }
        else
        {
            // no Desktop object: get window class name
            WinQueryClassName(pswctl->hwnd,               // old V0.9.3 code
                              sizeof(pNewItem->szClass)-1,
                              pNewItem->szClass);
            /* PCKERNELGLOBALS pKernelGlobals = krnQueryGlobals();
            // strcpy(pNewItem->szClass, "pObj is NULL");

            if (pObject == pKernelGlobals->pAwakeWarpCenter)
            {
                strcpy(pNewItem->szClass, "!!WarpCenter");
            }
            else
                // no Desktop object: get window class name
                WinQueryClassName(pswctl->hwnd,
                                  sizeof(pNewItem->szClass)-1,
                                  pNewItem->szClass); */
        }

        // append to list
        lstAppendItem(pList, pNewItem);
    }

    return (pNewItem);
}

/*
 *@@ xsdIsClosable:
 *      examines the given switch list entry and returns
 *      an XSD_* constant telling the caller what that
 *      item represents.
 *
 *      While we're at it, we also change some of the
 *      data in the switch list entry if needed.
 *
 *      If the switch list entry represents a Desktop object,
 *      *ppObject is set to that object's SOM pointer.
 *      Otherwise *ppObject receives NULL.
 *
 *      This returns:
 *      -- a value < 0 if the item must not be closed;
 *      -- 0 if the item is a regular item to be closed;
 *      -- XSD_DESKTOP (1) for the Desktop;
 *      -- XSD_WARPCENTER (2) for the WarpCenter.
 *
 *@@added V0.9.4 (2000-07-15) [umoeller]
 *@@changed V0.9.6 (2000-10-27) [umoeller]: fixed WarpCenter detection
 */

LONG xsdIsClosable(HAB hab,                 // in: caller's anchor block
                   PSHUTDOWNCONSTS pConsts,
                   SWENTRY *pSwEntry,       // in/out: switch entry
                   WPObject **ppObject)     // out: the WPObject*, really, or NULL if the window is no object
{
    LONG           lrc = 0;
    CHAR           szSwUpperTitle[100];

    *ppObject = NULL;

    strcpy(szSwUpperTitle,
           pSwEntry->swctl.szSwtitle);
    WinUpper(hab, 0, 0, szSwUpperTitle);

    if (    // skip if PID == 0:
            (pSwEntry->swctl.idProcess == 0)
            // skip the Shutdown windows:
         || (pSwEntry->swctl.hwnd == pConsts->hwndMain)
         || (pSwEntry->swctl.hwnd == pConsts->hwndVioDlg)
         || (pSwEntry->swctl.hwnd == pConsts->hwndShutdownStatus)
       )
        return (XSD_SYSTEM);
    // skip invisible tasklist entries; this
    // includes a PMWORKPLACE cmd.exe:
    else if (pSwEntry->swctl.uchVisibility != SWL_VISIBLE)
        return (XSD_INVISIBLE);
    // open WarpCenter (WarpCenter bar only):
    else if (   (pSwEntry->swctl.hwnd == pConsts->hwndOpenWarpCenter)
             && (pConsts->pKernelGlobals)
            )
    {
        *ppObject = pConsts->pKernelGlobals->pAwakeWarpCenter;
        return (XSD_WARPCENTER);
    }
#ifdef __DEBUG__
    // if we're in debug mode, skip the PMPRINTF window
    // because we want to see debug output
    else if (!strncmp(szSwUpperTitle, "PMPRINTF", 8))
        return (XSD_DEBUGNEED);
    // skip VAC debugger, which is probably debugging
    // PMSHELL.EXE
    else if (!strcmp(szSwUpperTitle, "ICSDEBUG.EXE"))
        return (XSD_DEBUGNEED);
#endif

    // now fix the data in the switch list entries,
    // if necessary
    if (pSwEntry->swctl.bProgType == PROG_DEFAULT)
    {
        // in this case, we need to find out what
        // type the program really has
        PQPROCSTAT16 pps;
        if (!prc16GetInfo(&pps))
        {
            PRCPROCESS prcp;
            // default for errors
            pSwEntry->swctl.bProgType = PROG_WINDOWABLEVIO;
            if (prc16QueryProcessInfo(pps, pSwEntry->swctl.idProcess, &prcp))
                // according to bsedos.h, the PROG_* types are identical
                // to the SSF_TYPE_* types, so we can use the data from
                // DosQProcStat
                pSwEntry->swctl.bProgType = prcp.ulSessionType;
            prc16FreeInfo(pps);
        }
    }

    if (pSwEntry->swctl.bProgType == PROG_WINDOWEDVDM)
        // DOS/Win-OS/2 window: get real PID/SID, because
        // the tasklist contains false data
        WinQueryWindowProcess(pSwEntry->swctl.hwnd,
                              &(pSwEntry->swctl.idProcess),
                              &(pSwEntry->swctl.idSession));

    if (pSwEntry->swctl.idProcess == pConsts->pidWPS)
    {
        // is Desktop window?
        if (pSwEntry->swctl.hwnd == pConsts->hwndActiveDesktop)
        {
            *ppObject = pConsts->pActiveDesktop;
            lrc = XSD_DESKTOP;
        }
        else
        {
            // PID == Workplace Shell PID: get SOM pointer from hwnd
            *ppObject = _wpclsQueryObjectFromFrame(pConsts->pWPDesktop, // _WPDesktop
                                                   pSwEntry->swctl.hwnd);

            if (*ppObject == pConsts->pActiveDesktop)
                lrc = XSD_DESKTOP;
        }
    }

    return (lrc);
}

/*
 *@@ xsdBuildShutList:
 *      this routine builds a new ShutList by evaluating the
 *      system task list; this list is built in pList.
 *      NOTE: It is entirely the job of the caller to serialize
 *      access to the list, using mutex semaphores.
 *      We call xsdAppendShutListItem for each task list entry,
 *      if that entry is to be closed, so we're doing a few checks.
 *
 *      This gets called from both xsdUpdateListBox (on the
 *      Shutdown thread) and the Update thread (fntUpdateThread)
 *      directly.
 *
 *@@changed V0.9.0 [umoeller]: adjusted for new linklist.c functions
 *@@changed V0.9.0 [umoeller]: PSHUTDOWNPARAMS added to prototype
 *@@changed V0.9.4 (2000-07-15) [umoeller]: PSHUTDOWNCONSTS added to prototype
 *@@changed V0.9.4 (2000-07-15) [umoeller]: extracted xsdIsClosable; fixed WarpCenter detection
 */

void xsdBuildShutList(HAB hab,
                      PSHUTDOWNDATA pShutdownData,
                      PLINKLIST pList)
{
    PSWBLOCK        pSwBlock   = NULL;         // Pointer to information returned
    ULONG           ul;
                    // cbItems    = 0,            // Number of items in list
                    // ulBufSize  = 0;            // Size of buffer for information

    // HAB             habDesktop = WinQueryAnchorBlock(HWND_DESKTOP);
    WPObject        *pObj;
    BOOL            Append;

    // get all the tasklist entries into a buffer
    /* cbItems = WinQuerySwitchList(NULLHANDLE, NULL, 0);
    ulBufSize = (cbItems * sizeof(SWENTRY)) + sizeof(HSWITCH);
    pSwBlock = (PSWBLOCK)malloc(ulBufSize);
    cbItems = WinQuerySwitchList(NULLHANDLE, pSwBlock, ulBufSize); */
    pSwBlock = winhQuerySwitchList(hab);

    // loop through all the tasklist entries
    for (ul = 0;
         ul < pSwBlock->cswentry;
         ul++)
    {
        // now we check which windows we add to the shutdown list
        LONG lrc = xsdIsClosable(hab,
                                 &pShutdownData->SDConsts,
                                 &pSwBlock->aswentry[ul],
                                 &pObj);
        if (lrc >= 0)
        {
            // closeable -> add window:
            Append = TRUE;

            // check for special objects
            if (    (lrc == XSD_DESKTOP)
                 || (lrc == XSD_WARPCENTER)
               )
                // Desktop and WarpCenter need special handling,
                // will be closed last always;
                // note that we NEVER append the WarpCenter to
                // the close list
                Append = FALSE;

            if (Append)
                // if we have (Restart Desktop) && ~(close all windows), append
                // only open Desktop objects and not all windows
                if (   (!(pShutdownData->sdParams.optWPSCloseWindows))
                    && (pObj == NULL)
                   )
                    Append = FALSE;

            if (Append)
                xsdAppendShutListItem(pShutdownData,
                                      pList,
                                      &pSwBlock->aswentry[ul].swctl,
                                      pObj,
                                      lrc);
        }
    }

    free(pSwBlock);
}

/*
 *@@ xsdUpdateListBox:
 *      this routine builds a new PSHUTITEM list from the
 *      pointer to the pointer of the first item (*ppliShutdownFirst)
 *      by setting its value to xsdBuildShutList's return value;
 *      it also fills the listbox in the "main" window, which
 *      is only visible in Debug mode.
 *      But even if it's invisible, the listbox is used for closing
 *      windows. Ugly, but nobody can see it. ;-)
 *      If *ppliShutdownFirst is != NULL the old shutlist is cleared
 *      also.
 *
 *      Runs on the Shutdown thread.
 *
 *@@changed V0.9.0 [umoeller]: adjusted for new linklist.c functions
 *@@changed V0.9.0 [umoeller]: PSHUTDOWNPARAMS added to prototype
 *@@changed V0.9.4 (2000-07-15) [umoeller]: PSHUTDOWNCONSTS added to prototype
 */

void xsdUpdateListBox(HAB hab,
                      PSHUTDOWNDATA pShutdownData,
                      HWND hwndListbox)
{
    PSHUTLISTITEM   pItem;
    CHAR            szTitle[1024];

    BOOL            fSemOwned = FALSE;

    TRY_QUIET(excpt1)
    {
        fSemOwned = (WinRequestMutexSem(pShutdownData->hmtxShutdown, 4000) == NO_ERROR);
        if (fSemOwned)
        {
            PLISTNODE pNode = 0;
            lstClear(&pShutdownData->llShutdown);
            xsdBuildShutList(hab,
                             pShutdownData,
                             &pShutdownData->llShutdown);

            // clear list box
            WinEnableWindowUpdate(hwndListbox, FALSE);
            WinSendMsg(hwndListbox, LM_DELETEALL, MPNULL, MPNULL);

            // and insert all list items as strings
            pNode = lstQueryFirstNode(&pShutdownData->llShutdown);
            while (pNode)
            {
                pItem = pNode->pItemData;
                xsdLongTitle(szTitle, pItem);
                WinInsertLboxItem(hwndListbox, 0, szTitle);
                pNode = pNode->pNext;
            }
            WinEnableWindowUpdate(hwndListbox, TRUE);
        }
    }
    CATCH(excpt1) { } END_CATCH();

    if (fSemOwned)
    {
        DosReleaseMutexSem(pShutdownData->hmtxShutdown);
        fSemOwned = FALSE;
    }
}

/*
 *@@ xsdUpdateClosingStatus:
 *      this gets called from fnwpShutdownThread to
 *      set the Shutdown status wnd text to "Closing xxx".
 *
 *      Runs on the Shutdown thread.
 */

VOID xsdUpdateClosingStatus(HWND hwndShutdownStatus,
                            PCSZ pcszProgTitle)   // in: window title from SHUTLISTITEM
{
    CHAR szTitle[300];
    strcpy(szTitle,
           cmnGetString(ID_SDSI_CLOSING)); // (cmnQueryNLSStrings())->pszSDClosing);
    strcat(szTitle, " \"");
    strcat(szTitle, pcszProgTitle);
    strcat(szTitle, "\"...");
    WinSetDlgItemText(hwndShutdownStatus, ID_SDDI_STATUS,
                      szTitle);

    WinSetActiveWindow(HWND_DESKTOP, hwndShutdownStatus);
}

/*
 *@@ xsdWaitForExceptions:
 *      checks for whether helpers/except.c is currently
 *      busy processing an exception and, if so, waits
 *      until that thread is done.
 *
 *      Gets called several times during fntShutdownThread
 *      because we don't want to lose the trap logs.
 *
 *@@added V0.9.13 (2001-06-19) [umoeller]
 */

VOID xsdWaitForExceptions(PSHUTDOWNDATA pShutdownData)
{
    // check the global variable exported from except.h,
    // which is > 0 if some exception is currently running
    if (G_ulExplainExceptionRunning)
    {
        ULONG ulSlept = 1;

        while (G_ulExplainExceptionRunning)
        {
            CHAR szTemp[500];
            sprintf(szTemp,
                    "Urgh, %d exception(s) running, waiting... (%d)",
                    G_ulExplainExceptionRunning,
                    ulSlept++);
            WinSetDlgItemText(pShutdownData->SDConsts.hwndShutdownStatus,
                              ID_SDDI_STATUS,
                              szTemp);

            // wait half a second
            winhSleep(500);
        }

        WinSetDlgItemText(pShutdownData->SDConsts.hwndShutdownStatus,
                          ID_SDDI_STATUS,
                          "OK");
    }
}

/*
 *@@ fncbSaveImmediate:
 *      callback for objForAllDirtyObjects to save
 *      the WPS.
 *
 *@@added V0.9.9 (2001-04-04) [umoeller]
 *@@changed V0.9.16 (2001-12-06) [umoeller]: now skipping saving objects from foreign desktops
 */

BOOL _Optlink fncbSaveImmediate(WPObject *pobjThis,
                                ULONG ulIndex,
                                ULONG cObjects,
                                PVOID pvUser)
{
    BOOL    brc = FALSE;
    PSHUTDOWNDATA pShutdownData = (PSHUTDOWNDATA)pvUser;

    // update progress bar
    WinSendMsg(pShutdownData->hwndProgressBar,
               WM_UPDATEPROGRESSBAR,
               (MPARAM)ulIndex,
               (MPARAM)cObjects);

    TRY_QUIET(excpt1)
    {
        if (pobjThis == pShutdownData->SDConsts.pActiveDesktop)
                        // we already saved the desktop, so skip this
                        // V0.9.16 (2001-10-25) [umoeller]
            brc = TRUE;
        else if (cmnIsObjectFromForeignDesktop(pobjThis))
                        // never save objects which do not belong to
                        // a foreign desktop
                        // V0.9.16 (2001-12-06) [umoeller]
        {
            /* CHAR szFolderPath[CCHMAXPATH];
            _wpQueryFilename(_wpQueryFolder(pobjThis),
                             szFolderPath,
                             TRUE);
            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                   "skipping save of object %s (class: %s) in folder %s",
                   _wpQueryTitle(pobjThis),
                   _somGetClassName(pobjThis),
                   szFolderPath);
            */
            brc = TRUE;
        }
        else
            brc = _wpSaveImmediate(pobjThis);
    }
    CATCH(excpt1)
    {
        brc = FALSE;
    } END_CATCH();

    return (brc);
}

/* ******************************************************************
 *
 *   Shutdown thread
 *
 ********************************************************************/

/*
 *@@ fntShutdownThread:
 *      this is the main shutdown thread which is created by
 *      xsdInitiateShutdown / xsdInitiateRestartWPS when shutdown is about
 *      to begin.
 *
 *      Parameters: this thread must be created using thrCreate
 *      (/helpers/threads.c), so it is passed a pointer
 *      to a THREADINFO structure. In that structure you
 *      must set ulData to point to a SHUTDOWNPARAMS structure.
 *
 *      Note: if you're trying to understand what's going on here,
 *      I recommend rebuilding XFolder with DEBUG_SHUTDOWN
 *      #define'd (see include/setup.h for that). This will allow you
 *      to switch XShutdown into "Debug" mode by holding down
 *      the "Shift" key while selecting "Shutdown" from the
 *      Desktop's context menu.
 *
 *      Shutdown / Restart Desktop runs in the following phases:
 *
 *      1)  First, all necessary preparations are done, i.e. two
 *          windows are created (the status window with the progress
 *          bar and the "main" window, which is only visible in debug
 *          mode, but processes all the messages). These two windows
 *          daringly share the same msg proc (fnwpShutdownThread below),
 *          but receive different messages, so this shan't hurt.
 *
 *          After these windows have been created, fntShutdown will also
 *          create the Update thread (fntUpdateThread below).
 *          This Update thread is responsible for monitoring the
 *          task list; every time an item is closed (or even opened!),
 *          it will post a ID_SDMI_UPDATESHUTLIST command to fnwpShutdownThread,
 *          which will then start working again.
 *
 *      2)  fntShutdownThread then remains in a standard PM message
 *          loop until shutdown is cancelled by the user or all
 *          windows have been closed.
 *          In both cases, fnwpShutdownThread posts a WM_QUIT then.
 *
 *          The order of msg processing in fntShutdownThread / fnwpShutdownThread
 *          is the following:
 *
 *          a)  ID_SDMI_UPDATESHUTLIST will update the list of currently
 *              open windows (which is not touched by any other thread)
 *              by calling xsdUpdateListBox.
 *
 *              Unless we're in debug mode (where shutdown has to be
 *              started manually), the first time ID_SDMI_UPDATESHUTLIST
 *              is received, we will post ID_SDDI_BEGINSHUTDOWN (go to c)).
 *              Otherwise (subsequent calls), we post ID_SDMI_CLOSEITEM
 *              (go to d)).
 *
 *          b)  ID_SDDI_BEGINSHUTDOWN will begin processing the contents
 *              of the Shutdown folder and empty the trash can. After this
 *              is done, ID_SDMI_BEGINCLOSINGITEMS is posted.
 *
 *          c)  ID_SDMI_BEGINCLOSINGITEMS will prepare closing all windows
 *              by setting flagClosingItems to TRUE and then post the
 *              first ID_SDMI_CLOSEITEM.
 *
 *          d)  ID_SDMI_CLOSEITEM will now undertake the necessary
 *              actions for closing the first / next item on the list
 *              of items to close, that is, post WM_CLOSE to the window
 *              or kill the process or whatever.
 *              If no more items are left to close, we post
 *              ID_SDMI_PREPARESAVEWPS (go to g)).
 *              Otherwise, after this, the Shutdown thread is idle.
 *
 *          e)  When the window has actually closed, the Update thread
 *              realizes this because the task list will have changed.
 *              The next ID_SDMI_UPDATESHUTLIST will be posted by the
 *              Update thread then. Go back to b).
 *
 *          f)  ID_SDMI_PREPARESAVEWPS will save the state of all currently
 *              awake Desktop objects by using the list which was maintained
 *              by the Worker thread all the while during the whole WPS session.
 *
 *          g)  After this, ID_SDMI_FLUSHBUFFERS is posted, which will
 *              set fAllWindowsClosed to TRUE and post WM_QUIT, so that
 *              the PM message loop in fntShutdownThread will exit.
 *
 *      3)  Depending on whether fnwpShutdownThread set fAllWindowsClosed to
 *          TRUE, we will then actually restart the WPS or shut down the system
 *          or exit this thread (= shutdown cancelled), and the user may continue work.
 *
 *          Shutting down the system is done by calling xsdFinishShutdown,
 *          which will differentiate what needs to be done depending on
 *          what the user wants (new with V0.84).
 *          We will then either reboot the machine or run in an endless
 *          loop, if no reboot was desired, or call the functions for an
 *          APM 1.2 power-off in apm.c (V0.82).
 *
 *          When shutdown was cancelled by pressing the respective button,
 *          the Update thread is killed, all shutdown windows are closed,
 *          and then this thread also terminates.
 *
 *@@changed V0.9.0 [umoeller]: adjusted for new linklist.c functions
 *@@changed V0.9.0 [umoeller]: changed shutdown logging to stdio functions (fprintf etc.)
 *@@changed V0.9.0 [umoeller]: code has been re-ordered for semaphore safety.
 *@@changed V0.9.1 (99-12-10) [umoeller]: extracted auto-close list code to xsdLoadAutoCloseItems
 *@@changed V0.9.9 (2001-04-04) [umoeller]: moved all post-close stuff from fnwpShutdownThread here
 *@@changed V0.9.9 (2001-04-04) [umoeller]: rewrote "save Desktop objects" to use dirty list from object.c
 *@@changed V0.9.11 (2001-04-18) [umoeller]: fixed logoff
 *@@changed V0.9.12 (2001-04-29) [umoeller]: deferred update thread startup to fnwpShutdownThread; this fixes shutdown folder
 *@@changed V0.9.12 (2001-05-15) [umoeller]: now telling PageMage to recover windows first
 *@@changed V0.9.12 (2001-05-29) [umoeller]: now broadcasting WM_SAVEAPPLICATION here
 *@@changed V0.9.12 (2001-05-29) [umoeller]: StartShutdownThread now uses THRF_PMMSGQUEUE so Wininitialize etc. has been removed here
 *@@changed V0.9.13 (2001-06-17) [umoeller]: no longer broadcasting WM_SAVEAPPLICATION, going back to old code
 *@@changed V0.9.13 (2001-06-19) [umoeller]: now pausing while exception handler is still running somewhere
 *@@changed V0.9.16 (2001-10-25) [umoeller]: couple of extra hacks for saving desktop
 */

static void _Optlink fntShutdownThread(PTHREADINFO ptiMyself)
{
    /*************************************************
     *
     *      data setup:
     *
     *************************************************/

    PSZ             pszErrMsg = NULL;
    QMSG            qmsg;
    APIRET          arc;
    HAB             hab = ptiMyself->hab;
    PXFILE          LogFile = NULL;

    // allocate shutdown data V0.9.9 (2001-03-07) [umoeller]
    PSHUTDOWNDATA pShutdownData = (PSHUTDOWNDATA)malloc(sizeof(SHUTDOWNDATA));
    if (!pShutdownData)
        return;

    // CLEAR ALL FIELDS -- this is essential!
    memset(pShutdownData, 0, sizeof(SHUTDOWNDATA));

    // get shutdown params from thread info
    memcpy(&pShutdownData->sdParams,
           (PSHUTDOWNPARAMS)ptiMyself->ulData,
           sizeof(SHUTDOWNPARAMS));

    xsdGetShutdownConsts(&pShutdownData->SDConsts);

    // copy anchor block so subfuncs can use it
    // V0.9.12 (2001-05-29) [umoeller]
    pShutdownData->habShutdownThread = hab;

    // set some global data for all the following
    pShutdownData->hmodResource = cmnQueryNLSModuleHandle(FALSE);

    WinCancelShutdown(ptiMyself->hmq, TRUE);

    // open shutdown log file for writing, if enabled
    if (pShutdownData->sdParams.optLog)
    {
        CHAR    szLogFileName[CCHMAXPATH];
        ULONG   cbFile = 0;
        sprintf(szLogFileName, "%c:\\%s", doshQueryBootDrive(), XFOLDER_SHUTDOWNLOG);
        if (arc = doshOpen(szLogFileName,
                           XOPEN_READWRITE_APPEND,        // not XOPEN_BINARY
                           &cbFile,
                           &LogFile))
            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                   "Cannot create log file %s", szLogFileName);

        pShutdownData->ShutdownLogFile = LogFile;
    }

    if (LogFile)
    {
        // write log header
        DATETIME DT;
        DosGetDateTime(&DT);
        doshWriteLogEntry(LogFile,
                "XWorkplace Shutdown Log -- Date: %04d-%02d-%02d, Time: %02d:%02d:%02d",
                DT.year, DT.month, DT.day,
                DT.hours, DT.minutes, DT.seconds);
        doshWriteLogEntry(LogFile, "-----------------------------------------------------------\n");
        doshWriteLogEntry(LogFile, "XWorkplace version: %s", XFOLDER_VERSION);
        doshWriteLogEntry(LogFile, "Shutdown thread started, TID: 0x%lX",
                thrQueryID(ptiMyself));
        doshWriteLogEntry(LogFile, "Settings: RestartWPS %d, Confirm %s, Reboot %s, WPSCloseWnds %s, CloseVIOs %s, WarpCenterFirst %s, APMPowerOff %s",
                pShutdownData->sdParams.ulRestartWPS,
                (pShutdownData->sdParams.optConfirm) ? "ON" : "OFF",
                (pShutdownData->sdParams.optReboot) ? "ON" : "OFF",
                (pShutdownData->sdParams.optWPSCloseWindows) ? "ON" : "OFF",
                (pShutdownData->sdParams.optAutoCloseVIO) ? "ON" : "OFF",
                (pShutdownData->sdParams.optWarpCenterFirst) ? "ON" : "OFF",
                (pShutdownData->sdParams.optAPMPowerOff) ? "ON" : "OFF");
    }

    // raise our own priority; we will
    // still use the REGULAR class, but
    // with the maximum delta, so we can
    // get above nasty (DOS?) sessions
    DosSetPriority(PRTYS_THREAD,
                   PRTYC_REGULAR,
                   PRTYD_MAXIMUM, // priority delta
                   0);

    TRY_LOUD(excpt1)
    {
        SWCNTRL     swctl;
        HSWITCH     hswitch;
        ULONG       // ulKeyLength = 0,
                    ulAutoCloseItemsFound = 0;
        HPOINTER    hptrShutdown = WinLoadPointer(HWND_DESKTOP, pShutdownData->hmodResource,
                                                  ID_SDICON);

        // create an event semaphore which signals to the Update thread
        // that the Shutlist has been updated by fnwpShutdownThread
        DosCreateEventSem(NULL,         // unnamed
                          &pShutdownData->hevUpdated,
                          0,            // unshared
                          FALSE);       // not posted

        // create mutex semaphores for linked lists
        if (pShutdownData->hmtxShutdown == NULLHANDLE)
        {
            DosCreateMutexSem("\\sem32\\ShutdownList",
                              &pShutdownData->hmtxShutdown, 0, FALSE);     // unnamed, unowned
            DosCreateMutexSem("\\sem32\\SkippedList",
                              &pShutdownData->hmtxSkipped, 0, FALSE);      // unnamed, unowned
        }

        lstInit(&pShutdownData->llShutdown, TRUE);      // auto-free items
        lstInit(&pShutdownData->llSkipped, TRUE);       // auto-free items
        lstInit(&pShutdownData->llAutoClose, TRUE);     // auto-free items

        doshWriteLogEntry(LogFile,
               __FUNCTION__ ": Getting auto-close items from OS2.INI...");

        // check for auto-close items in OS2.INI
        // and build pliAutoClose list accordingly
        ulAutoCloseItemsFound = xsdLoadAutoCloseItems(&pShutdownData->llAutoClose,
                                                      NULLHANDLE); // no list box

        doshWriteLogEntry(LogFile,
               "  Found %d auto-close items.", ulAutoCloseItemsFound);

        doshWriteLogEntry(LogFile,
               "  Creating shutdown windows...");

        /*************************************************
         *
         *      shutdown windows setup:
         *
         *************************************************/

        // setup main (debug) window; this is hidden
        // if we're not in debug mode
        pShutdownData->SDConsts.hwndMain
                = WinLoadDlg(HWND_DESKTOP, NULLHANDLE,
                             fnwpShutdownThread,
                             pShutdownData->hmodResource,
                             ID_SDD_MAIN,
                             NULL);
        WinSetWindowPtr(pShutdownData->SDConsts.hwndMain,
                        QWL_USER,
                        pShutdownData); // V0.9.9 (2001-03-07) [umoeller]
        WinSendMsg(pShutdownData->SDConsts.hwndMain,
                   WM_SETICON,
                   (MPARAM)hptrShutdown,
                    NULL);

        doshWriteLogEntry(LogFile,
               "  Created main window (hwnd: 0x%lX)",
               pShutdownData->SDConsts.hwndMain);
        doshWriteLogEntry(LogFile,
               "  HAB: 0x%lX, HMQ: 0x%lX, pidWPS: 0x%lX, pidPM: 0x%lX",
               hab,
               ptiMyself->hmq,
               pShutdownData->SDConsts.pidWPS,
               pShutdownData->SDConsts.pidPM);

        pShutdownData->ulMaxItemCount = 0;
        pShutdownData->ulLastItemCount = -1;

        pShutdownData->hPOC = 0;

        pShutdownData->sidPM = 1;  // should always be this, I hope

        // add ourselves to the tasklist
        swctl.hwnd = pShutdownData->SDConsts.hwndMain;                  // window handle
        swctl.hwndIcon = hptrShutdown;               // icon handle
        swctl.hprog = NULLHANDLE;               // program handle
        swctl.idProcess = pShutdownData->SDConsts.pidWPS;               // PID
        swctl.idSession = 0;                    // SID
        swctl.uchVisibility = SWL_VISIBLE;      // visibility
        swctl.fbJump = SWL_JUMPABLE;            // jump indicator
        WinQueryWindowText(pShutdownData->SDConsts.hwndMain, sizeof(swctl.szSwtitle), (PSZ)&swctl.szSwtitle);
        swctl.bProgType = PROG_DEFAULT;         // program type

        hswitch = WinAddSwitchEntry(&swctl);
        WinQuerySwitchEntry(hswitch, &swctl);
        pShutdownData->sidWPS = swctl.idSession;   // get the "real" WPS SID

        // setup status window (always visible)
        pShutdownData->SDConsts.hwndShutdownStatus
                = WinLoadDlg(HWND_DESKTOP,
                             NULLHANDLE,
                             fnwpShutdownThread,
                             pShutdownData->hmodResource,
                             ID_SDD_STATUS,
                             NULL);
        WinSetWindowPtr(pShutdownData->SDConsts.hwndShutdownStatus,
                        QWL_USER,
                        pShutdownData); // V0.9.9 (2001-03-07) [umoeller]
        WinSendMsg(pShutdownData->SDConsts.hwndShutdownStatus,
                   WM_SETICON,
                   (MPARAM)hptrShutdown,
                   NULL);

        doshWriteLogEntry(LogFile,
               "  Created status window (hwnd: 0x%lX)",
               pShutdownData->SDConsts.hwndShutdownStatus);

        // subclass the static rectangle control in the dialog to make
        // it a progress bar
        pShutdownData->hwndProgressBar
            = WinWindowFromID(pShutdownData->SDConsts.hwndShutdownStatus,
                              ID_SDDI_PROGRESSBAR);
        ctlProgressBarFromStatic(pShutdownData->hwndProgressBar,
                                 PBA_ALIGNCENTER | PBA_BUTTONSTYLE);

        // set status window to top
        WinSetWindowPos(pShutdownData->SDConsts.hwndShutdownStatus,
                        HWND_TOP,
                        0, 0, 0, 0,
                        SWP_SHOW | SWP_ZORDER | SWP_ACTIVATE);

        // animate the traffic light
        xsdLoadAnimation(&G_sdAnim);
        ctlPrepareAnimation(WinWindowFromID(pShutdownData->SDConsts.hwndShutdownStatus,
                                            ID_SDDI_ICON),
                            XSD_ANIM_COUNT,
                            &(G_sdAnim.ahptr[0]),
                            150,    // delay
                            TRUE);  // start now

        // create update thread (moved here V0.9.9 (2001-03-07) [umoeller])
        /* removed again; moved this down into fnwpShutdownThread
            it is now started after the shutdown folder has
            finished processing V0.9.12 (2001-04-29) [umoeller]

        if (thrQueryID(&G_tiUpdateThread) == NULLHANDLE)
        {
            thrCreate(&G_tiUpdateThread,
                      fntUpdateThread,
                      NULL, // running flag
                      "ShutdownUpdate",
                      THRF_PMMSGQUEUE,
                      (ULONG)pShutdownData);  // V0.9.9 (2001-03-07) [umoeller]

            doshWriteLogEntry(LogFile,
                   __FUNCTION__ ": Update thread started, tid: 0x%lX",
                   thrQueryID(&G_tiUpdateThread));
        }
        */

        if (pShutdownData->sdParams.optDebug)
        {
            // debug mode: show "main" window, which
            // is invisible otherwise
            winhCenterWindow(pShutdownData->SDConsts.hwndMain);
            WinShowWindow(pShutdownData->SDConsts.hwndMain, TRUE);
        }

        doshWriteLogEntry(LogFile,
               __FUNCTION__ ": Now entering shutdown message loop...");

        // tell PageMage to recover all windows to the current screen
        // V0.9.12 (2001-05-15) [umoeller]
        if (pShutdownData->SDConsts.pKernelGlobals)
        {
            PXWPGLOBALSHARED pXwpGlobalShared = pShutdownData->SDConsts.pKernelGlobals->pXwpGlobalShared;

            if (pXwpGlobalShared)
            {
                doshWriteLogEntry(LogFile,
                       __FUNCTION__ ": Recovering all PageMage windows...");

                if (pXwpGlobalShared->hwndDaemonObject)
                    WinSendMsg(pXwpGlobalShared->hwndDaemonObject,
                               XDM_RECOVERWINDOWS,
                               0, 0);
            }
        }

        if (!pShutdownData->sdParams.optDebug)
        {
            // if we're not in debug mode, begin shutdown
            // automatically; ID_SDDI_BEGINSHUTDOWN will
            // first empty the trash can, process the
            // shutdown folder, and finally start closing
            // windows
            doshWriteLogEntry(LogFile, __FUNCTION__ ": Posting ID_SDDI_BEGINSHUTDOWN");
            WinPostMsg(pShutdownData->SDConsts.hwndMain,
                       WM_COMMAND,
                       MPFROM2SHORT(ID_SDDI_BEGINSHUTDOWN, 0),
                       MPNULL);
        }

        // if we're closing all windows,
        // broadcast WM_SAVEAPPLICATION to all frames which
        // are children of the desktop
        // V0.9.12 (2001-05-29) [umoeller]
        // V0.9.13 (2001-06-17) [umoeller]: nope, go back to doing this for each window
        /* if (    (0 == pShutdownData->sdParams.ulRestartWPS) // restart Desktop (1) or logoff (2)
             || (pShutdownData->sdParams.optWPSCloseWindows)
           )
        {
            doshWriteLogEntry(LogFile, __FUNCTION__ ": Broadcasting WM_SAVEAPPLICATION");
        } */

        // pShutdownData->ulStatus is still XSD_IDLE at this point

        /*************************************************
         *
         *      standard PM message loop:
         *          here we are closing the windows
         *
         *************************************************/

        // now enter the common message loop for the main (debug) and
        // status windows (fnwpShutdownThread); this will keep running
        // until closing all windows is complete or cancelled, upon
        // both of which fnwpShutdownThread will post WM_QUIT
        while (WinGetMsg(hab, &qmsg, NULLHANDLE, 0, 0))
            WinDispatchMsg(hab, &qmsg);

        doshWriteLogEntry(LogFile,
               __FUNCTION__ ": Done with message loop.");

        /*************************************************
         *
         *      done closing windows:
         *
         *************************************************/

// all the following has been moved here from fnwpShutdownThread
// with V0.9.9 (2001-04-04) [umoeller]

        // in any case,
        // close the Update thread to prevent it from interfering
        // with what we're doing now
        if (thrQueryID(&G_tiUpdateThread))
        {
            #ifdef DEBUG_SHUTDOWN
                WinSetDlgItemText(G_hwndShutdownStatus, ID_SDDI_STATUS,
                                  "Waiting for the Update thread to end...");
            #endif
            doshWriteLogEntry(LogFile,
                   __FUNCTION__ ": Closing Update thread, tid: 0x%lX...",
                   thrQueryID(&G_tiUpdateThread));

            thrFree(&G_tiUpdateThread);  // close and wait
            doshWriteLogEntry(LogFile,
                   __FUNCTION__ ": Update thread closed.");
        }

        // check if shutdown was cancelled (XSD_CANCELLED)
        // or if we should proceed (XSD_ALLDONEOK)
        if (pShutdownData->ulStatus == XSD_ALLDONEOK)
        {
            ULONG   cObjectsToSave = 0,
                    cObjectsSaved = 0;
            CHAR    szTitle[400];
            PSWBLOCK psw;

            /*************************************************
             *
             *      close desktop and WarpCenter
             *
             *************************************************/

            // save the window list position and fonts
            // V0.9.16 (2002-01-13) [umoeller]
            // this doesn't work... apparently we must
            // be in the shell process for WinStoreWindowPos
            // to work correctly
            /* if (psw = winhQuerySwitchList(hab))
            {
                CHAR sz[1000];
                sprintf(sz,
                        "Storing window list 0x%lX",
                        psw->aswentry[0].swctl.hwnd);
                winhDebugBox(NULLHANDLE,
                             "Shutdown thread",
                             sz);
                // window list frame is always the first entry
                // in the switch list
                if (!WinStoreWindowPos("PM_Workplace:WindowListPos",
                                       "SavePos",
                                       psw->aswentry[0].swctl.hwnd))
                    cmnLog(__FILE__, __LINE__, __FUNCTION__,
                           "WinStoreWindowPos for tasklist failed");
                free(psw);
            } */

            WinSetActiveWindow(HWND_DESKTOP,
                               pShutdownData->SDConsts.hwndShutdownStatus);

            // disable buttons in status window... we can't stop now!
            winhEnableDlgItem(pShutdownData->SDConsts.hwndShutdownStatus,
                             ID_SDDI_CANCELSHUTDOWN,
                             FALSE);
            winhEnableDlgItem(pShutdownData->SDConsts.hwndShutdownStatus,
                             ID_SDDI_SKIPAPP,
                             FALSE);

            // close Desktop window (which we excluded from
            // the regular SHUTLISTITEM list)
            xsdUpdateClosingStatus(pShutdownData->SDConsts.hwndShutdownStatus,
                                   _wpQueryTitle(pShutdownData->SDConsts.pActiveDesktop));
            doshWriteLogEntry(LogFile,
                   __FUNCTION__ ": Closing Desktop window");

            // sleep a little while... XCenter might have resized
            // the desktop, and this will hang otherwise
            // V0.9.9 (2001-04-04) [umoeller]
            winhSleep(300);

            if (pShutdownData->SDConsts.pActiveDesktop)
            {
                CHAR szDesktop[CCHMAXPATH];
                // set <WP_DESKTOP> ID on desktop; sometimes this gets
                // lost during shutdown
                // V0.9.16 (2001-10-25) [umoeller]
                if (_wpQueryFilename(pShutdownData->SDConsts.pActiveDesktop,
                                     szDesktop,
                                     TRUE))
                {
                    // save last active desktop in OS2.INI in case
                    // <WP_DESKTOP> gets broken so we can get the
                    // path in the new panic dialog; see
                    // initRepairDesktopIfBroken
                    // V0.9.16 (2001-10-25) [umoeller]
                    PrfWriteProfileString(HINI_USER,
                                          (PSZ)INIAPP_XWORKPLACE,
                                          (PSZ)INIKEY_LASTDESKTOPPATH,
                                          szDesktop);
                }

                _wpSetObjectID(pShutdownData->SDConsts.pActiveDesktop,
                               (PSZ)WPOBJID_DESKTOP); // "<WP_DESKTOP>",

                _wpSaveImmediate(pShutdownData->SDConsts.pActiveDesktop);
                _wpClose(pShutdownData->SDConsts.pActiveDesktop);
                _wpWaitForClose(pShutdownData->SDConsts.pActiveDesktop,
                                NULLHANDLE,     // all views
                                0xFFFFFFFF,
                                5*1000,     // timeout value
                                TRUE);      // force close for new views
                            // added V0.9.4 (2000-07-11) [umoeller]

                // give the desktop time to save icon positions
                winhSleep(300); // V0.9.12 (2001-04-29) [umoeller]
            }

            // close WarpCenter next (V0.9.5, from V0.9.3)
            if (somIsObj(pShutdownData->SDConsts.pKernelGlobals->pAwakeWarpCenter))
            {
                // WarpCenter still open?
                if (_wpFindUseItem(pShutdownData->SDConsts.pKernelGlobals->pAwakeWarpCenter,
                                   USAGE_OPENVIEW,
                                   NULL)       // get first useitem
                    )
                {
                    // if open: close it
                    xsdUpdateClosingStatus(pShutdownData->SDConsts.hwndShutdownStatus,
                                           _wpQueryTitle(pShutdownData->SDConsts.pKernelGlobals->pAwakeWarpCenter));
                    doshWriteLogEntry(LogFile,
                           __FUNCTION__ ": Found open WarpCenter USEITEM, closing...");

                    _wpSaveImmediate(pShutdownData->SDConsts.pKernelGlobals->pAwakeWarpCenter);
                    // _wpClose(pShutdownData->SDConsts.pKernelGlobals->pAwakeWarpCenter);
                    WinPostMsg(pShutdownData->SDConsts.hwndOpenWarpCenter,
                               WM_COMMAND,
                               MPFROMSHORT(0x66F7),
                                    // "Close" menu item in WarpCenter context menu...
                                    // nothing else works right!
                               MPFROM2SHORT(CMDSRC_OTHER,
                                            FALSE));     // keyboard?!?

                    _wpWaitForClose(pShutdownData->SDConsts.pKernelGlobals->pAwakeWarpCenter,
                                    pShutdownData->SDConsts.hwndOpenWarpCenter,
                                    VIEW_ANY,
                                    SEM_INDEFINITE_WAIT,
                                    TRUE);

                    pShutdownData->SDConsts.hwndOpenWarpCenter = NULLHANDLE;
                }
            }

            // if some thread is currently in an exception handler,
            // wait until the handler is done; otherwise the trap
            // log won't be written and we can't find out what
            // happened V0.9.13 (2001-06-19) [umoeller]
            xsdWaitForExceptions(pShutdownData);

            // set progress bar to the max
            WinSendMsg(pShutdownData->hwndProgressBar,
                       WM_UPDATEPROGRESSBAR,
                       (MPARAM)1,
                       (MPARAM)1);

            winhSleep(300);

            // now we need a blank screen so that it looks
            // as if we had closed all windows, even if we
            // haven't; we do this by creating a "fake
            // desktop", which is just an empty window w/out
            // title bar which takes up the whole screen and
            // has the color of the PM desktop
            if (    (!(pShutdownData->sdParams.ulRestartWPS))
                 && (!(pShutdownData->sdParams.optDebug))
               )
                winhCreateFakeDesktop(pShutdownData->SDConsts.hwndShutdownStatus);

            /*************************************************
             *
             *      save Desktop objects
             *
             *************************************************/

            cObjectsToSave = objQueryDirtyObjectsCount();

            sprintf(szTitle,
                    cmnGetString(ID_SDSI_SAVINGDESKTOP), // cmnQueryNLSStrings()->pszSDSavingDesktop,
                        // "Saving xxx awake Desktop objects..."
                    cObjectsToSave);
            WinSetDlgItemText(pShutdownData->SDConsts.hwndShutdownStatus,
                              ID_SDDI_STATUS,
                              szTitle);

            doshWriteLogEntry(LogFile,
                   __FUNCTION__ ": Saving %d awake Desktop objects...",
                   cObjectsToSave);

            // reset progress bar
            WinSendMsg(pShutdownData->hwndProgressBar,
                       WM_UPDATEPROGRESSBAR,
                       (MPARAM)0,
                       (MPARAM)cObjectsToSave);

            // have the WPS flush its buffers
            xsdFlushWPS2INI();  // added V0.9.0 (UM 99-10-22)

            // and wait a while
            winhSleep(500);

            // if some thread is currently in an exception handler,
            // wait until the handler is done; otherwise the trap
            // log won't be written and we can't find out what
            // happened V0.9.13 (2001-06-19) [umoeller]
            xsdWaitForExceptions(pShutdownData);

            // finally, save WPS!!

            // now using proper "dirty" list V0.9.9 (2001-04-04) [umoeller]
            cObjectsSaved = objForAllDirtyObjects(fncbSaveImmediate,
                                                  pShutdownData);  // user param

            // if some thread is currently in an exception handler,
            // wait until the handler is done; otherwise the trap
            // log won't be written and we can't find out what
            // happened V0.9.13 (2001-06-19) [umoeller]
            xsdWaitForExceptions(pShutdownData);

            // set progress bar to max
            WinSendMsg(pShutdownData->hwndProgressBar,
                       WM_UPDATEPROGRESSBAR,
                       (MPARAM)1,
                       (MPARAM)1);

            doshWriteLogEntry(LogFile,
                   __FUNCTION__ ": Done saving WPS, %d objects saved.",
                   cObjectsSaved);

            winhSleep(200);

            if (pShutdownData->sdParams.ulRestartWPS)
            {
                // "Restart Desktop" mode

                WinSetDlgItemText(pShutdownData->SDConsts.hwndShutdownStatus,
                                  ID_SDDI_STATUS,
                                  cmnGetString(ID_SDSI_RESTARTINGWPS)); // (cmnQueryNLSStrings())->pszSDRestartingWPS);

                // reuse startup folder?
                krnSetProcessStartupFolder(pShutdownData->sdParams.optWPSReuseStartupFolder);
            }

        } // end if (pShutdownData->ulStatus == XSD_ALLDONEOK)

// end moved code with V0.9.9 (2001-04-04) [umoeller]

    } // end TRY_LOUD(excpt1
    CATCH(excpt1)
    {
        // exception occured:
        krnUnlockGlobals();     // just to make sure
        // fExceptionOccured = TRUE;

        if (pszErrMsg == NULL)
        {
            // only report the first error, or otherwise we will
            // jam the system with msg boxes
            pszErrMsg = malloc(2000);
            if (pszErrMsg)
            {
                strcpy(pszErrMsg, "An error occured in the XFolder Shutdown thread. "
                        "In the root directory of your boot drive, you will find a "
                        "file named XFLDTRAP.LOG, which contains debugging information. "
                        "If you had shutdown logging enabled, you will also find the "
                        "file XSHUTDWN.LOG there. If not, please enable shutdown "
                        "logging in the Desktop's settings notebook. "
                        "\n\nThe XShutdown procedure will be terminated now. We can "
                        "now also restart the Workplace Shell. This is recommended if "
                        "your Desktop has already been closed or if "
                        "the error occured during the saving of the INI files. In these "
                        "cases, please disable XShutdown and perform a regular OS/2 "
                        "shutdown to prevent loss of your WPS data."
                        "\n\nRestart the Workplace Shell now?");
                krnPostThread1ObjectMsg(T1M_EXCEPTIONCAUGHT, (MPARAM)pszErrMsg,
                                        (MPARAM)1); // enforce Desktop restart

                doshWriteLogEntry(LogFile,
                       "\n*** CRASH\n%s\n", pszErrMsg);
            }
        }
    } END_CATCH();

    /*
     * Cleanup:
     *
     */

    // we arrive here if
    //      a) fnwpShutdownThread successfully closed all windows;
    //         only in that case, fAllWindowsClosed is TRUE;
    //      b) shutdown was cancelled by the user;
    //      c) an exception occured.
    // In any of these cases, we need to clean up big time now.

    // close "main" window, but keep the status window for now
    WinDestroyWindow(pShutdownData->SDConsts.hwndMain);

    doshWriteLogEntry(LogFile,
           __FUNCTION__ ": Entering cleanup...");

    {
        // PKERNELGLOBALS pKernelGlobals = krnLockGlobals(__FILE__, __LINE__, __FUNCTION__);
        // check for whether we're owning semaphores;
        // if we do (e.g. after an exception), release them now
        if (pShutdownData->fShutdownSemOwned)
        {
            doshWriteLogEntry(LogFile, "  Releasing shutdown mutex");
            DosReleaseMutexSem(pShutdownData->hmtxShutdown);
            pShutdownData->fShutdownSemOwned = FALSE;
        }
        if (pShutdownData->fSkippedSemOwned)
        {
            doshWriteLogEntry(LogFile, "  Releasing skipped mutex");
            DosReleaseMutexSem(pShutdownData->hmtxSkipped);
            pShutdownData->fSkippedSemOwned = FALSE;
        }

        doshWriteLogEntry(LogFile, "  Done releasing semaphores.");

        // get rid of the Update thread;
        // this got closed by fnwpShutdownThread normally,
        // but with exceptions, this might not have happened
        if (thrQueryID(&G_tiUpdateThread))
        {
            doshWriteLogEntry(LogFile, "  Closing Update thread...");
            thrFree(&G_tiUpdateThread); // fixed V0.9.0
            doshWriteLogEntry(LogFile, "  Update thread closed.");
        }

        // krnUnlockGlobals();
    }

    doshWriteLogEntry(LogFile, "  Closing semaphores...");
    DosCloseEventSem(pShutdownData->hevUpdated);
    if (pShutdownData->hmtxShutdown != NULLHANDLE)
    {
        if (arc = DosCloseMutexSem(pShutdownData->hmtxShutdown))
        {
            DosBeep(100, 1000);
            doshWriteLogEntry(LogFile, "    Error %d closing hmtxShutdown!",
                              arc);
        }
        pShutdownData->hmtxShutdown = NULLHANDLE;
    }
    if (pShutdownData->hmtxSkipped != NULLHANDLE)
    {
        if (arc = DosCloseMutexSem(pShutdownData->hmtxSkipped))
        {
            DosBeep(100, 1000);
            doshWriteLogEntry(LogFile, "    Error %d closing hmtxSkipped!",
                              arc);
        }
        pShutdownData->hmtxSkipped = NULLHANDLE;
    }
    doshWriteLogEntry(LogFile, "  Done closing semaphores.");

    doshWriteLogEntry(LogFile, "  Freeing lists...");
    TRY_LOUD(excpt1)
    {
        // destroy all global lists; this time, we need
        // no mutex semaphores, because the Update thread
        // is already down
        lstClear(&pShutdownData->llShutdown);
        lstClear(&pShutdownData->llSkipped);
    }
    CATCH(excpt1) {} END_CATCH();
    doshWriteLogEntry(LogFile, "  Done freeing lists.");

    /*
     * Restart Desktop or shutdown:
     *
     */

    if (pShutdownData->ulStatus == XSD_ALLDONEOK)
    {
        // (fAllWindowsClosed = TRUE) only if shutdown was
        // not cancelled; this means that all windows have
        // been successfully closed and we can actually shut
        // down the system

        if (pShutdownData->sdParams.ulRestartWPS) // restart Desktop (1) or logoff (2)
        {
            // here we will actually restart the WPS
            doshWriteLogEntry(LogFile, "Preparing Desktop restart...");

            ctlStopAnimation(WinWindowFromID(pShutdownData->SDConsts.hwndShutdownStatus, ID_SDDI_ICON));
            WinDestroyWindow(pShutdownData->SDConsts.hwndShutdownStatus);
            xsdFreeAnimation(&G_sdAnim);

            doshWriteLogEntry(LogFile, "Restarting WPS: Calling DosExit(), closing log.");
            doshClose(&LogFile);
            LogFile = NULL;

            xsdRestartWPS(hab,
                          (pShutdownData->sdParams.ulRestartWPS == 2)
                                ? TRUE
                                : FALSE);  // fLogoff
                            // V0.9.11 (2001-04-18) [umoeller]
            // this will not return, I think
        }
        else
        {
            // *** no restart Desktop:
            // call the termination routine, which
            // will do the rest

            xsdFinishShutdown(pShutdownData);
            // this will not return, except in debug mode

            if (pShutdownData->sdParams.optDebug)
                // in debug mode, restart Desktop
                pShutdownData->sdParams.ulRestartWPS = 1;     // V0.9.0
        }
    } // end if (fAllWindowsClosed)

    // the following code is only reached if
    // shutdown was cancelled...

    // set the global flag for whether shutdown is
    // running to FALSE; this will re-enable the
    // items in the Desktop's context menu
    G_fShutdownRunning = FALSE;

    // close logfile
    if (LogFile)
    {
        doshWriteLogEntry(LogFile, "Reached cleanup, closing log.");
        doshClose(&LogFile);
        LogFile = NULL;
    }

    // moved this down, because we need a msg queue for restart Desktop
    // V0.9.3 (2000-04-26) [umoeller]

    WinDestroyWindow(pShutdownData->SDConsts.hwndShutdownStatus);

    free(pShutdownData);        // V0.9.9 (2001-03-07) [umoeller]

    // end of Shutdown thread
    // thread exits!
}

/*
 *@@ xsdCloseVIO:
 *      this gets called upon ID_SDMI_CLOSEVIO in
 *      fnwpShutdownThread when a VIO window is encountered.
 *      (To be precise, this gets called for all non-PM
 *      sessions, not just VIO windows.)
 *
 *      This function queries the list of auto-close
 *      items and closes the VIO window accordingly.
 *      If no corresponding item was found, we display
 *      a dialog, querying the user what to do with this.
 *
 *      Runs on the Shutdown thread.
 *
 *@@added V0.9.1 (99-12-10) [umoeller]
 */

VOID xsdCloseVIO(PSHUTDOWNDATA pShutdownData,
                 HWND hwndFrame)
{
    PSHUTLISTITEM   pItem;
    ULONG           ulReply;
    PAUTOCLOSELISTITEM pliAutoCloseFound = NULL; // fixed V0.9.0
    ULONG           ulSessionAction = 0;
                        // this will become one of the ACL_* flags
                        // if something is to be done with this session

    doshWriteLogEntry(pShutdownData->ShutdownLogFile,
           "  ID_SDMI_CLOSEVIO, hwnd: 0x%lX; entering xsdCloseVIO",
           hwndFrame);

    // get VIO item to close
    pItem = xsdQueryCurrentItem(pShutdownData);

    if (pItem)
    {
        // valid item: go thru list of auto-close items
        // if this item is on there
        PLISTNODE   pAutoCloseNode = 0;
        xsdLongTitle(pShutdownData->szVioTitle, pItem);
        pShutdownData->VioItem = *pItem;
        doshWriteLogEntry(pShutdownData->ShutdownLogFile, "    xsdCloseVIO: VIO item: %s", pShutdownData->szVioTitle);

        // activate VIO window
        WinSetActiveWindow(HWND_DESKTOP, pShutdownData->VioItem.swctl.hwnd);

        // check if VIO window is on auto-close list
        pAutoCloseNode = lstQueryFirstNode(&pShutdownData->llAutoClose);
        while (pAutoCloseNode)
        {
            PAUTOCLOSELISTITEM pliThis = pAutoCloseNode->pItemData;
            doshWriteLogEntry(pShutdownData->ShutdownLogFile, "      Checking %s", pliThis->szItemName);
            // compare first characters
            if (strnicmp(pShutdownData->VioItem.swctl.szSwtitle,
                         pliThis->szItemName,
                         strlen(pliThis->szItemName))
                   == 0)
            {
                // item found:
                doshWriteLogEntry(pShutdownData->ShutdownLogFile, "        Matching item found, auto-closing item");
                pliAutoCloseFound = pliThis;
                break;
            }

            pAutoCloseNode = pAutoCloseNode->pNext;
        }

        if (pliAutoCloseFound)
        {
            // item was found on auto-close list:
            doshWriteLogEntry(pShutdownData->ShutdownLogFile, "    Found on auto-close list");

            // store this item's action for later
            ulSessionAction = pliAutoCloseFound->usAction;
        } // end if (pliAutoCloseFound)
        else
        {
            // not on auto-close list:
            if (pShutdownData->sdParams.optAutoCloseVIO)
            {
                // auto-close enabled globally though:
                doshWriteLogEntry(pShutdownData->ShutdownLogFile, "    Not found on auto-close list, auto-close is on:");

                // "auto close VIOs" is on
                if (pShutdownData->VioItem.swctl.idSession == 1)
                {
                    // for some reason, DOS/Windows sessions always
                    // run in the Shell process, whose SID == 1
                    WinPostMsg((WinWindowFromID(pShutdownData->VioItem.swctl.hwnd, FID_SYSMENU)),
                               WM_SYSCOMMAND,
                               (MPARAM)SC_CLOSE, MPNULL);
                    doshWriteLogEntry(pShutdownData->ShutdownLogFile, "      Posted SC_CLOSE to hwnd 0x%lX",
                           WinWindowFromID(pShutdownData->VioItem.swctl.hwnd, FID_SYSMENU));
                }
                else
                {
                    // OS/2 windows: kill
                    DosKillProcess(DKP_PROCESS, pShutdownData->VioItem.swctl.idProcess);
                    doshWriteLogEntry(pShutdownData->ShutdownLogFile, "      Killed pid 0x%lX",
                           pShutdownData->VioItem.swctl.idProcess);
                }
            } // end if (psdParams->optAutoCloseVIO)
            else
            {
                CHAR            szText[500];

                // no auto-close: confirmation wnd
                doshWriteLogEntry(pShutdownData->ShutdownLogFile, "    Not found on auto-close list, auto-close is off, query-action dlg:");

                cmnSetDlgHelpPanel(ID_XFH_CLOSEVIO);
                pShutdownData->SDConsts.hwndVioDlg
                    = WinLoadDlg(HWND_DESKTOP,
                                 pShutdownData->SDConsts.hwndShutdownStatus,
                                 cmn_fnwpDlgWithHelp,
                                 cmnQueryNLSModuleHandle(FALSE),
                                 ID_SDD_CLOSEVIO,
                                 NULL);

                // ID_SDDI_VDMAPPTEXT has "\"cannot be closed automatically";
                // prefix session title
                strcpy(szText, "\"");
                strcat(szText, pShutdownData->VioItem.swctl.szSwtitle);
                WinQueryDlgItemText(pShutdownData->SDConsts.hwndVioDlg, ID_SDDI_VDMAPPTEXT,
                                    100, &(szText[strlen(szText)]));
                WinSetDlgItemText(pShutdownData->SDConsts.hwndVioDlg, ID_SDDI_VDMAPPTEXT,
                                  szText);

                cmnSetControlsFont(pShutdownData->SDConsts.hwndVioDlg, 1, 5000);
                winhCenterWindow(pShutdownData->SDConsts.hwndVioDlg);
                ulReply = WinProcessDlg(pShutdownData->SDConsts.hwndVioDlg);

                if (ulReply == DID_OK)
                {
                    doshWriteLogEntry(pShutdownData->ShutdownLogFile, "      'OK' pressed");

                    // "OK" button pressed: check the radio buttons
                    // for what to do with this session
                    if (winhIsDlgItemChecked(pShutdownData->SDConsts.hwndVioDlg, ID_XSDI_ACL_SKIP))
                    {
                        ulSessionAction = ACL_SKIP;
                        doshWriteLogEntry(pShutdownData->ShutdownLogFile, "      xsdCloseVIO: 'Skip' selected");
                    }
                    else if (winhIsDlgItemChecked(pShutdownData->SDConsts.hwndVioDlg, ID_XSDI_ACL_WMCLOSE))
                    {
                        ulSessionAction = ACL_WMCLOSE;
                        doshWriteLogEntry(pShutdownData->ShutdownLogFile, "      xsdCloseVIO: 'WM_CLOSE' selected");
                    }
                    else if (winhIsDlgItemChecked(pShutdownData->SDConsts.hwndVioDlg, ID_XSDI_ACL_KILLSESSION))
                    {
                        ulSessionAction = ACL_KILLSESSION;
                        doshWriteLogEntry(pShutdownData->ShutdownLogFile, "      xsdCloseVIO: 'Kill' selected");
                    }

                    // "store item" checked?
                    if (ulSessionAction)
                        if (winhIsDlgItemChecked(pShutdownData->SDConsts.hwndVioDlg, ID_XSDI_ACL_STORE))
                        {
                            ULONG ulInvalid = 0;
                            // "store" checked:
                            // add item to list
                            PAUTOCLOSELISTITEM pliNew = malloc(sizeof(AUTOCLOSELISTITEM));
                            strncpy(pliNew->szItemName,
                                    pShutdownData->VioItem.swctl.szSwtitle,
                                    sizeof(pliNew->szItemName)-1);
                            pliNew->szItemName[99] = 0;
                            pliNew->usAction = ulSessionAction;
                            lstAppendItem(&pShutdownData->llAutoClose,
                                          pliNew);

                            // write list back to OS2.INI
                            ulInvalid = xsdWriteAutoCloseItems(&pShutdownData->llAutoClose);
                            doshWriteLogEntry(pShutdownData->ShutdownLogFile, "         Updated auto-close list in OS2.INI, rc: %d",
                                        ulInvalid);
                        }
                }
                else if (ulReply == ID_SDDI_CANCELSHUTDOWN)
                {
                    // "Cancel shutdown" pressed:
                    // pass to main window
                    WinPostMsg(pShutdownData->SDConsts.hwndMain, WM_COMMAND,
                               MPFROM2SHORT(ID_SDDI_CANCELSHUTDOWN, 0),
                               MPNULL);
                    doshWriteLogEntry(pShutdownData->ShutdownLogFile, "      'Cancel shutdown' pressed");
                }
                // else: we could also get DID_CANCEL; this means
                // that the dialog was closed because the Update
                // thread determined that a session was closed
                // manually, so we just do nothing...

                WinDestroyWindow(pShutdownData->SDConsts.hwndVioDlg);
                pShutdownData->SDConsts.hwndVioDlg = NULLHANDLE;
            } // end else (optAutoCloseVIO)
        }

        doshWriteLogEntry(pShutdownData->ShutdownLogFile,
                          "      xsdCloseVIO: ulSessionAction is %d", ulSessionAction);

        // OK, let's see what to do with this session
        switch (ulSessionAction)
                    // this is 0 if nothing is to be done
        {
            case ACL_WMCLOSE:
                WinPostMsg((WinWindowFromID(pShutdownData->VioItem.swctl.hwnd, FID_SYSMENU)),
                           WM_SYSCOMMAND,
                           (MPARAM)SC_CLOSE, MPNULL);
                doshWriteLogEntry(pShutdownData->ShutdownLogFile, "      xsdCloseVIO: Posted SC_CLOSE to sysmenu, hwnd: 0x%lX",
                       WinWindowFromID(pShutdownData->VioItem.swctl.hwnd, FID_SYSMENU));
            break;

            case ACL_CTRL_C:
                DosSendSignalException(pShutdownData->VioItem.swctl.idProcess,
                                       XCPT_SIGNAL_INTR);
                doshWriteLogEntry(pShutdownData->ShutdownLogFile, "      xsdCloseVIO: Sent INTR signal to pid 0x%lX",
                       pShutdownData->VioItem.swctl.idProcess);
            break;

            case ACL_KILLSESSION:
                DosKillProcess(DKP_PROCESS, pShutdownData->VioItem.swctl.idProcess);
                doshWriteLogEntry(pShutdownData->ShutdownLogFile, "      xsdCloseVIO: Killed pid 0x%lX",
                       pShutdownData->VioItem.swctl.idProcess);
            break;

            case ACL_SKIP:
                WinPostMsg(pShutdownData->SDConsts.hwndMain, WM_COMMAND,
                           MPFROM2SHORT(ID_SDDI_SKIPAPP, 0),
                           MPNULL);
                doshWriteLogEntry(pShutdownData->ShutdownLogFile, "      xsdCloseVIO: Posted ID_SDDI_SKIPAPP");
            break;
        }
    }

    doshWriteLogEntry(pShutdownData->ShutdownLogFile, "  Done with xsdCloseVIO");
}

/*
 *@@ CloseOneItem:
 *      implementation for ID_SDMI_CLOSEITEM in
 *      ID_SDMI_CLOSEITEM. Closes one item on
 *      the shutdown list, depending on the
 *      item's type.
 *
 *@@added V0.9.9 (2001-04-04) [umoeller]
 */

static VOID CloseOneItem(PSHUTDOWNDATA pShutdownData,
                         HWND hwndListbox,
                         PSHUTLISTITEM pItem)
{
    CHAR        szTitle[1024];
    USHORT      usItem;

    // compose string from item
    xsdLongTitle(szTitle, pItem);

    doshWriteLogEntry(pShutdownData->ShutdownLogFile, "    Item: %s", szTitle);

    // find string in the invisible list box
    usItem = (USHORT)WinSendMsg(hwndListbox,
                                LM_SEARCHSTRING,
                                MPFROM2SHORT(0, LIT_FIRST),
                                (MPARAM)szTitle);
    // and select it
    if ((usItem != (USHORT)LIT_NONE))
        WinPostMsg(hwndListbox,
                   LM_SELECTITEM,
                   (MPARAM)usItem,
                   (MPARAM)TRUE);

    // update status window: "Closing xxx"
    xsdUpdateClosingStatus(pShutdownData->SDConsts.hwndShutdownStatus,
                           pItem->swctl.szSwtitle);

    // now check what kind of action needs to be done
    if (pItem->pObject)
    {
        // we have a WPS window
        // (cannot be WarpCenter, cannot be Desktop):
        _wpClose(pItem->pObject);
                    // WPObject::goes thru the list of USAGE_OPENVIEW
                    // useitems and sends WM_CLOSE to each of them

        doshWriteLogEntry(pShutdownData->ShutdownLogFile,
               "      Open Desktop object, called wpClose(pObject)");
    }
    else if (pItem->swctl.hwnd)
    {
        // no WPS window: differentiate further
        doshWriteLogEntry(pShutdownData->ShutdownLogFile,
               "      swctl.hwnd found: 0x%lX",
               pItem->swctl.hwnd);

        if (    (pItem->swctl.bProgType == PROG_VDM)
             || (pItem->swctl.bProgType == PROG_WINDOWEDVDM)
             || (pItem->swctl.bProgType == PROG_FULLSCREEN)
             || (pItem->swctl.bProgType == PROG_WINDOWABLEVIO)
             // || (pItem->swctl.bProgType == PROG_DEFAULT)
           )
        {
            // not a PM session: ask what to do (handled below)
            doshWriteLogEntry(pShutdownData->ShutdownLogFile, "      Seems to be VIO, swctl.bProgType: 0x%lX", pItem->swctl.bProgType);
            if (    (pShutdownData->SDConsts.hwndVioDlg == NULLHANDLE)
                 || (strcmp(szTitle, pShutdownData->szVioTitle) != 0)
               )
            {
                // "Close VIO window" not currently open:
                doshWriteLogEntry(pShutdownData->ShutdownLogFile, "      Posting ID_SDMI_CLOSEVIO");
                WinPostMsg(pShutdownData->SDConsts.hwndMain, WM_COMMAND,
                           MPFROM2SHORT(ID_SDMI_CLOSEVIO, 0),
                           MPNULL);
            }
            /// else do nothing
        }
        else
            // if (WinWindowFromID(pItem->swctl.hwnd, FID_SYSMENU))
            // removed V0.9.0 (UM 99-10-22)
        {
            // window has system menu: close PM application;
            // WM_SAVEAPPLICATION and WM_QUIT is what WinShutdown
            // does too for every message queue per process;
            // re-enabled this here per window V0.9.13 (2001-06-17) [umoeller]
            doshWriteLogEntry(pShutdownData->ShutdownLogFile,
                   "      Posting WM_SAVEAPPLICATION to hwnd 0x%lX",
                   pItem->swctl.hwnd);

            WinPostMsg(pItem->swctl.hwnd,
                       WM_SAVEAPPLICATION,
                       MPNULL, MPNULL);

            doshWriteLogEntry(pShutdownData->ShutdownLogFile,
                   "      Posting WM_QUIT to hwnd 0x%lX",
                   pItem->swctl.hwnd);

            WinPostMsg(pItem->swctl.hwnd,
                       WM_QUIT,
                       MPNULL, MPNULL);
        }
        /* else
        {
            // no system menu: try something more brutal
            doshWriteLogEntry(pShutdownData->ShutdownLogFile, "      Has no sys menu, posting WM_CLOSE to hwnd 0x%lX",
                        pItem->swctl.hwnd);

            WinPostMsg(pItem->swctl.hwnd,
                       WM_CLOSE,
                       MPNULL,
                       MPNULL);
        } */
    }
    else
    {
        doshWriteLogEntry(pShutdownData->ShutdownLogFile,
               "      Helpless... leaving item alone, pid: 0x%lX",
               pItem->swctl.idProcess);
        // DosKillProcess(DKP_PROCESS, pItem->swctl.idProcess);
    }
}

/*
 *@@ fnwpShutdownThread:
 *      window procedure for both the main (debug) window and
 *      the status window; it receives messages from the Update
 *      threads, so that it can update the windows' contents
 *      accordingly; it also controls this thread by suspending
 *      or killing it, if necessary, and setting semaphores.
 *      Note that the main (debug) window with the listbox is only
 *      visible in debug mode (signalled to xfInitiateShutdown).
 *
 *      Runs on the Shutdown thread.
 *
 *@@changed V0.9.0 [umoeller]: adjusted for new linklist.c functions
 *@@changed V0.9.0 [umoeller]: fixed some inconsistensies in ID_SDMI_CLOSEVIO
 *@@changed V0.9.0 [umoeller]: changed shutdown logging to stdio functions (fprintf etc.)
 *@@changed V0.9.0 [umoeller]: changed "reuse Desktop startup folder" to use XWPGLOBALSHARED
 *@@changed V0.9.0 [umoeller]: added xsdFlushWPS2INI call
 *@@changed V0.9.1 (99-12-10) [umoeller]: extracted VIO code to xsdCloseVIO
 *@@changed V0.9.4 (2000-07-11) [umoeller]: added wpWaitForClose for Desktop
 *@@changed V0.9.4 (2000-07-15) [umoeller]: added special treatment for WarpCenter
 *@@changed V0.9.6 (2000-10-27) [umoeller]: fixed special treatment for WarpCenter
 *@@changed V0.9.7 (2000-12-08) [umoeller]: now taking WarpCenterFirst setting into account
 *@@changed V0.9.9 (2001-03-07) [umoeller]: now using all settings in SHUTDOWNDATA
 *@@changed V0.9.9 (2001-03-07) [umoeller]: fixed race condition on closing XCenter
 *@@changed V0.9.9 (2001-04-04) [umoeller]: extracted CloseOneItem
 *@@changed V0.9.9 (2001-04-04) [umoeller]: moved all post-close stuff to fntShutdownThread
 *@@changed V0.9.12 (2001-04-29) [umoeller]: now starting update thread after shutdown folder processing
 *@@changed V0.9.12 (2001-04-29) [umoeller]: now using new shutdown folder implementation
 *@@changed V0.9.16 (2001-10-22) [pr]: fixed bug trying to close the first switch list item twice
 */

static MRESULT EXPENTRY fnwpShutdownThread(HWND hwndFrame, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT         mrc = MRFALSE;

    switch(msg)
    {
        // case WM_INITDLG:     // both removed V0.9.9 (2001-03-07) [umoeller]
        // case WM_CREATE:

        case WM_COMMAND:
        {
            PSHUTDOWNDATA   pShutdownData = (PSHUTDOWNDATA)WinQueryWindowPtr(hwndFrame,
                                                                             QWL_USER);
                                    // V0.9.9 (2001-03-07) [umoeller]
            HWND            hwndListbox = WinWindowFromID(pShutdownData->SDConsts.hwndMain,
                                                          ID_SDDI_LISTBOX);

            if (!pShutdownData)
                break;

            switch (SHORT1FROMMP(mp1))
            {
                /*
                 * ID_SDDI_BEGINSHUTDOWN:
                 *
                 */

                case ID_SDDI_BEGINSHUTDOWN:
                {
                    // this is either posted by the "Begin shutdown"
                    // button (in debug mode) or otherwise automatically
                    // after the first update initiated by the Update
                    // thread

                    // pShutdownData->ulStatus is still XSD_IDLE at this point

                    XFolder         *pShutdownFolder;

                    _Pmpf((" ---> ID_SDDI_BEGINSHUTDOWN"));

                    doshWriteLogEntry(pShutdownData->ShutdownLogFile, "  ID_SDDI_BEGINSHUTDOWN, hwnd: 0x%lX", hwndFrame);

                    WinShowWindow(pShutdownData->SDConsts.hwndShutdownStatus, TRUE);

                    // empty trash can?
                    if (    (pShutdownData->sdParams.optEmptyTrashCan)
                         && (cmnTrashCanReady())
                       )
                    {
                        APIRET arc = NO_ERROR;
                        WinSetDlgItemText(pShutdownData->SDConsts.hwndShutdownStatus, ID_SDDI_STATUS,
                                          cmnGetString(ID_XSSI_FOPS_EMPTYINGTRASHCAN)) ; // pszFopsEmptyingTrashCan
                        doshWriteLogEntry(pShutdownData->ShutdownLogFile, "    Emptying trash can...");

                        arc = cmnEmptyDefTrashCan(pShutdownData->habShutdownThread,
                                                            // synchronously
                                                  NULL,
                                                  NULLHANDLE);   // no confirm
                        if (arc == NO_ERROR)
                            // success:
                            doshWriteLogEntry(pShutdownData->ShutdownLogFile, "    Done emptying trash can.");
                        else
                        {
                            doshWriteLogEntry(pShutdownData->ShutdownLogFile, "    Emptying trash can failed, rc: %d.",
                                    arc);
                            if (cmnMessageBoxMsg(pShutdownData->SDConsts.hwndShutdownStatus,
                                                 104, // "error"
                                                 189, // "empty failed"
                                                 MB_YESNO)
                                    != MBID_YES)
                                // stop:
                                WinPostMsg(pShutdownData->SDConsts.hwndMain, WM_COMMAND,
                                           MPFROM2SHORT(ID_SDDI_CANCELSHUTDOWN, 0),
                                           MPNULL);
                        }
                    }

                    // now look for a Shutdown folder
                    if (pShutdownFolder = _wpclsQueryFolder(_WPFolder,
                                                            (PSZ)XFOLDER_SHUTDOWNID,
                                                            TRUE))
                    {
                        doshWriteLogEntry(pShutdownData->ShutdownLogFile, "    Processing shutdown folder...");

                        // using new implementation V0.9.12 (2001-04-29) [umoeller]
                        _xwpStartFolderContents(pShutdownFolder,
                                                0);         // wait mode

                        doshWriteLogEntry(pShutdownData->ShutdownLogFile, "    Started processing shutdown folder.");
                    }

                    // now build our shutlist V0.9.12 (2001-04-29) [umoeller]
                    xsdUpdateListBox(pShutdownData->habShutdownThread,
                                     pShutdownData,
                                     hwndListbox);

                    // create the Update thread now
                    // V0.9.12 (2001-04-28) [umoeller]
                    // moved this down here, we shouldn't start this
                    // before the shutdown folder has finished
                    // processing, or we'll close the windows we've
                    // started ourselves
                    if (thrQueryID(&G_tiUpdateThread) == NULLHANDLE)
                    {
                        thrCreate(&G_tiUpdateThread,
                                  fntUpdateThread,
                                  NULL, // running flag
                                  "ShutdownUpdate",
                                  THRF_PMMSGQUEUE | THRF_WAIT_EXPLICIT,
                                        // but wait explicit V0.9.12 (2001-04-29) [umoeller]
                                        // added msgq V0.9.16 (2002-01-13) [umoeller]
                                  (ULONG)pShutdownData);  // V0.9.9 (2001-03-07) [umoeller]

                        doshWriteLogEntry(pShutdownData->ShutdownLogFile,
                               __FUNCTION__ ": Update thread started, tid: 0x%lX",
                               thrQueryID(&G_tiUpdateThread));
                    }

                    // set progress bar to the left
                    WinSendMsg(pShutdownData->hwndProgressBar,
                               WM_UPDATEPROGRESSBAR,
                               (MPARAM)0,
                               (MPARAM)1);

                    // close open WarpCenter first, if desired
                    // V0.9.7 (2000-12-08) [umoeller]
                    doshWriteLogEntry(pShutdownData->ShutdownLogFile, "  WarpCenter treatment:");
                    if (somIsObj(pShutdownData->SDConsts.pKernelGlobals->pAwakeWarpCenter))
                    {
                        if (pShutdownData->SDConsts.hwndOpenWarpCenter)
                        {
                            doshWriteLogEntry(pShutdownData->ShutdownLogFile, "      WarpCenter found, has HWND 0x%lX",
                                   pShutdownData->SDConsts.hwndOpenWarpCenter);
                            if (pShutdownData->sdParams.optWarpCenterFirst)
                            {
                                doshWriteLogEntry(pShutdownData->ShutdownLogFile, "      WarpCenterFirst is ON, posting WM_COMMAND 0x66F7");
                                xsdUpdateClosingStatus(pShutdownData->SDConsts.hwndShutdownStatus,
                                                       "WarpCenter");
                                WinPostMsg(pShutdownData->SDConsts.hwndOpenWarpCenter,
                                           WM_COMMAND,
                                           MPFROMSHORT(0x66F7),
                                                // "Close" menu item in WarpCenter context menu...
                                                // nothing else works right!
                                           MPFROM2SHORT(CMDSRC_OTHER,
                                                        FALSE));     // keyboard?!?
                                winhSleep(400);
                                pShutdownData->SDConsts.hwndOpenWarpCenter = NULLHANDLE;
                            }
                            else
                                doshWriteLogEntry(pShutdownData->ShutdownLogFile, "      WarpCenterFirst is OFF, skipping...");
                        }
                        else
                            doshWriteLogEntry(pShutdownData->ShutdownLogFile, "      WarpCenter not found.");
                    }

                    // mark status as "closing windows now"
                    pShutdownData->ulStatus = XSD_CLOSING;
                    winhEnableDlgItem(pShutdownData->SDConsts.hwndMain,
                                      ID_SDDI_BEGINSHUTDOWN,
                                      FALSE);

                    // V0.9.16 (2001-10-22) [pr]: ID_SDMI_UPDATESHUTLIST kicks this off
                    /* WinPostMsg(pShutdownData->SDConsts.hwndMain,
                               WM_COMMAND,
                               MPFROM2SHORT(ID_SDMI_CLOSEITEM, 0),
                               MPNULL); */
                break; }

                /*
                 * ID_SDMI_CLOSEITEM:
                 *     this msg is posted first upon receiving
                 *     ID_SDDI_BEGINSHUTDOWN and subsequently for every
                 *     window that is to be closed; we only INITIATE
                 *     closing the window here by posting messages
                 *     or killing the window; we then rely on the
                 *     update thread to realize that the window has
                 *     actually been removed from the Tasklist. The
                 *     Update thread then posts ID_SDMI_UPDATESHUTLIST,
                 *     which will then in turn post another ID_SDMI_CLOSEITEM.
                 */

                case ID_SDMI_CLOSEITEM:
                {
                    PSHUTLISTITEM pItem;

                    doshWriteLogEntry(pShutdownData->ShutdownLogFile,
                           "  ID_SDMI_CLOSEITEM");

                    // get task list item to close from linked list
                    pItem = xsdQueryCurrentItem(pShutdownData);
                    if (pItem)
                    {
                        CloseOneItem(pShutdownData,
                                     hwndListbox,
                                     pItem);
                    } // end if (pItem)
                    else
                    {
                        // no more items left: enter phase 2 (save WPS)
                        doshWriteLogEntry(pShutdownData->ShutdownLogFile,
                               "    All items closed. Posting WM_QUIT...");

                        pShutdownData->ulStatus = XSD_ALLDONEOK;

                        WinPostMsg(pShutdownData->SDConsts.hwndMain,
                                   WM_QUIT,     // V0.9.9 (2001-04-04) [umoeller]
                                   0,
                                   0);
                    }
                break; }

                /*
                 * ID_SDDI_SKIPAPP:
                 *     comes from the "Skip" button
                 */

                case ID_SDDI_SKIPAPP:
                {
                    PSHUTLISTITEM   pItem,
                                    pSkipItem;
                    doshWriteLogEntry(pShutdownData->ShutdownLogFile, "  ID_SDDI_SKIPAPP, hwnd: 0x%lX", hwndFrame);

                    pItem = xsdQueryCurrentItem(pShutdownData);
                    if (pItem)
                    {
                        doshWriteLogEntry(pShutdownData->ShutdownLogFile, "    Adding %s to the list of skipped items",
                               pItem->swctl.szSwtitle);

                        pShutdownData->fSkippedSemOwned = !WinRequestMutexSem(pShutdownData->hmtxSkipped, 4000);
                        if (pShutdownData->fSkippedSemOwned)
                        {
                            pSkipItem = malloc(sizeof(SHUTLISTITEM));
                            memcpy(pSkipItem, pItem, sizeof(SHUTLISTITEM));

                            lstAppendItem(&pShutdownData->llSkipped,
                                          pSkipItem);
                            DosReleaseMutexSem(pShutdownData->hmtxSkipped);
                            pShutdownData->fSkippedSemOwned = FALSE;
                        }
                    }

                    // shutdown running (started and not cancelled)?
                    if (pShutdownData->ulStatus == XSD_CLOSING)
                        WinPostMsg(pShutdownData->SDConsts.hwndMain,
                                   WM_COMMAND,
                                   MPFROM2SHORT(ID_SDMI_UPDATEPROGRESSBAR, 0),
                                   MPNULL);
                break; }

                /*
                 * ID_SDMI_CLOSEVIO:
                 *     this is posted by ID_SDMI_CLOSEITEM when
                 *     a non-PM session is encountered; we will now
                 *     either close this session automatically or
                 *     open up a confirmation dlg
                 */

                case ID_SDMI_CLOSEVIO:
                {
                    xsdCloseVIO(pShutdownData,
                                hwndFrame);
                break; }

                /*
                 * ID_SDMI_UPDATESHUTLIST:
                 *    this cmd comes from the Update thread when
                 *    the task list has changed. This happens when
                 *    1)    something was closed by this function
                 *    2)    the user has closed something
                 *    3)    and even if the user has OPENED something
                 *          new.
                 *    We will then rebuilt the pliShutdownFirst list and
                 *    continue closing items, if Shutdown is currently in progress
                 */

                case ID_SDMI_UPDATESHUTLIST:
                {
                    // SHUTDOWNCONSTS  SDConsts;
                    #ifdef DEBUG_SHUTDOWN
                        DosBeep(10000, 50);
                    #endif

                    doshWriteLogEntry(pShutdownData->ShutdownLogFile,
                           "  ID_SDMI_UPDATESHUTLIST, hwnd: 0x%lX",
                           hwndFrame);

                    xsdUpdateListBox(pShutdownData->habShutdownThread,
                                     pShutdownData,
                                     hwndListbox);
                        // this updates the Shutdown linked list
                    DosPostEventSem(pShutdownData->hevUpdated);
                        // signal update to Update thread

                    doshWriteLogEntry(pShutdownData->ShutdownLogFile,
                           "    Rebuilt shut list, %d items remaining",
                           xsdCountRemainingItems(pShutdownData));

                    if (pShutdownData->SDConsts.hwndVioDlg)
                    {
                        USHORT usItem;
                        // "Close VIO" confirmation window is open:
                        // check if the item that was to be closed
                        // is still in the listbox; if so, exit,
                        // if not, close confirmation window and
                        // continue
                        usItem = (USHORT)WinSendMsg(hwndListbox,
                                                    LM_SEARCHSTRING,
                                                    MPFROM2SHORT(0, LIT_FIRST),
                                                    (MPARAM)pShutdownData->szVioTitle);

                        if ((usItem != (USHORT)LIT_NONE))
                            break;
                        else
                        {
                            WinPostMsg(pShutdownData->SDConsts.hwndVioDlg,
                                       WM_CLOSE,
                                       MPNULL,
                                       MPNULL);
                            // this will result in a DID_CANCEL return code
                            // for WinProcessDlg in ID_SDMI_CLOSEVIO above
                            doshWriteLogEntry(pShutdownData->ShutdownLogFile,
                                   "    Closed open VIO confirm dlg' dlg");
                        }
                    }
                    goto updateprogressbar;
                    // continue with update progress bar
                }

                /*
                 * ID_SDMI_UPDATEPROGRESSBAR:
                 *     well, update the progress bar in the
                 *     status window
                 */

                case ID_SDMI_UPDATEPROGRESSBAR:
                updateprogressbar:
                {
                    ULONG           ulItemCount, ulMax, ulNow;

                    ulItemCount = xsdCountRemainingItems(pShutdownData);
                    if (ulItemCount > pShutdownData->ulMaxItemCount)
                    {
                        pShutdownData->ulMaxItemCount = ulItemCount;
                        pShutdownData->ulLastItemCount = -1; // enforce update
                    }

                    doshWriteLogEntry(pShutdownData->ShutdownLogFile, "  ID_SDMI_UPDATEPROGRESSBAR, hwnd: 0x%lX, remaining: %d, total: %d",
                           hwndFrame, ulItemCount, pShutdownData->ulMaxItemCount);

                    if ((ulItemCount) != pShutdownData->ulLastItemCount)
                    {
                        ulMax = pShutdownData->ulMaxItemCount;
                        ulNow = (ulMax - ulItemCount);

                        WinSendMsg(pShutdownData->hwndProgressBar,
                                   WM_UPDATEPROGRESSBAR,
                                   (MPARAM)ulNow,
                                   (MPARAM)(ulMax + 1));        // add one extra for desktop
                        pShutdownData->ulLastItemCount = (ulItemCount);
                    }

                    if (pShutdownData->ulStatus == XSD_CLOSING)
                        // if we're already in the process of shutting down, we will
                        // initiate closing the next item
                        WinPostMsg(pShutdownData->SDConsts.hwndMain,
                                   WM_COMMAND,
                                   MPFROM2SHORT(ID_SDMI_CLOSEITEM, 0),
                                   MPNULL);
                break; }

                /*
                 * DID_CANCEL:
                 * ID_SDDI_CANCELSHUTDOWN:
                 *
                 */

                case DID_CANCEL:
                case ID_SDDI_CANCELSHUTDOWN:
                    // results from the "Cancel shutdown" buttons in both the
                    // main (debug) and the status window; we set a semaphore
                    // upon which the Update thread will terminate itself,
                    // and WM_QUIT is posted to the main (debug) window, so that
                    // the message loop in the main Shutdown thread function ends
                    if (winhIsDlgItemEnabled(pShutdownData->SDConsts.hwndShutdownStatus,
                                             ID_SDDI_CANCELSHUTDOWN))
                    {
                        // mark shutdown status as cancelled
                        pShutdownData->ulStatus = XSD_CANCELLED;

                        doshWriteLogEntry(pShutdownData->ShutdownLogFile,
                                "  DID_CANCEL/ID_SDDI_CANCELSHUTDOWN, hwnd: 0x%lX");

                        /* if (pShutdownData->hPOC)
                            ((PPROCESSCONTENTINFO)pShutdownData->hPOC)->fCancelled = TRUE; */
                        winhEnableDlgItem(pShutdownData->SDConsts.hwndShutdownStatus,
                                         ID_SDDI_CANCELSHUTDOWN,
                                         FALSE);
                        winhEnableDlgItem(pShutdownData->SDConsts.hwndShutdownStatus,
                                         ID_SDDI_SKIPAPP,
                                         FALSE);
                        winhEnableDlgItem(pShutdownData->SDConsts.hwndMain,
                                         ID_SDDI_BEGINSHUTDOWN,
                                         TRUE);

                        WinPostMsg(pShutdownData->SDConsts.hwndMain,
                                   WM_QUIT,     // V0.9.9 (2001-04-04) [umoeller]
                                   0,
                                   MPNULL);
                    }
                break;

                default:
                    mrc = WinDefDlgProc(hwndFrame, msg, mp1, mp2);
            } // end switch;
        break; } // end case WM_COMMAND

        // other msgs: have them handled by the usual WC_FRAME wnd proc
        default:
           mrc = WinDefDlgProc(hwndFrame, msg, mp1, mp2);
        break;
    }

    return (mrc);
}

/* ******************************************************************
 *                                                                  *
 *   here comes the "Finish" routines                               *
 *                                                                  *
 ********************************************************************/

/*
 *  The following routines are called to finish shutdown
 *  after all windows have been closed and the system is
 *  to be shut down or APM power-off'ed or rebooted or
 *  whatever.
 *
 *  All of these routines run on the Shutdown thread.
 */

/*
 *@@ fncbUpdateINIStatus:
 *      this is a callback func for prfhSaveINIs for
 *      updating the progress bar.
 *
 *      Runs on the Shutdown thread.
 */

static MRESULT EXPENTRY fncbUpdateINIStatus(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    if (hwnd)
        WinSendMsg(hwnd, msg, mp1, mp2);
    return ((MPARAM)TRUE);
}

/*
 *@@ fncbINIError:
 *      callback func for prfhSaveINIs (/helpers/winh.c).
 *
 *      Runs on the Shutdown thread.
 */

static MRESULT EXPENTRY fncbSaveINIError(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    return ((MRESULT)cmnMessageBox(HWND_DESKTOP,
                                   "XShutdown: Error",
                                  (PSZ)mp1,     // error text
                                  (ULONG)mp2)   // MB_ABORTRETRYIGNORE or something
       );
}

/*
 *@@ xsd_fnSaveINIsProgress:
 *
 *@@added V0.9.5 (2000-08-13) [umoeller]
 */

BOOL _Optlink xsd_fnSaveINIsProgress(ULONG ulUser,
                                           // in: user param specified with
                                           // xprfCopyProfile and xprfSaveINIs
                                           // (progress bar HWND)
                                     ULONG ulProgressNow,
                                           // in: current progress
                                     ULONG ulProgressMax)
                                           // in: maximum progress
{
    HWND hwndProgress = (HWND)ulUser;
    if (hwndProgress)
        WinSendMsg(hwndProgress,
                   WM_UPDATEPROGRESSBAR,
                   (MPARAM)ulProgressNow,
                   (MPARAM)ulProgressMax);
    return (TRUE);
}

/*
 *@@ xsdFinishShutdown:
 *      this is the generic routine called by
 *      fntShutdownThread after closing all windows
 *      is complete and the system is to be shut
 *      down, depending on the user settings.
 *
 *      This evaluates the current shutdown settings and
 *      calls xsdFinishStandardReboot, xsdFinishUserReboot,
 *      xsdFinishAPMPowerOff, or xsdFinishStandardMessage.
 *
 *      Note that this is only called for "real" shutdown.
 *      For "Restart Desktop", xsdRestartWPS is called instead.
 *
 *      Runs on the Shutdown thread.
 *
 *@@changed V0.9.3 (2000-05-22) [umoeller]: added reboot animation
 *@@changed V0.9.5 (2000-08-13) [umoeller]: now using new save-INI routines (xprfSaveINIs)
 *@@changed V0.9.12 (2001-05-12) [umoeller]: animations frequently didn't show up, fixed
 */

VOID xsdFinishShutdown(PSHUTDOWNDATA pShutdownData) // HAB hab)
{
    APIRET      arc = NO_ERROR;
    ULONG       ulSaveINIs = 0;

#ifndef __NOXSHUTDOWN__
    ulSaveINIs = cmnQuerySetting(sulSaveINIS);
#endif

    // change the mouse pointer to wait state
    winhSetWaitPointer();

    // enforce saving of INIs
    WinSetDlgItemText(pShutdownData->SDConsts.hwndShutdownStatus, ID_SDDI_STATUS,
                      cmnGetString(ID_SDSI_SAVINGPROFILES)) ; // pszSDSavingProfiles

#ifndef __NOXSHUTDOWN__
    switch (ulSaveINIs)
    {
        case 2:         // do nothing:
            doshWriteLogEntry(pShutdownData->ShutdownLogFile, "Saving INIs has been disabled, skipping.");
        break;

        case 1:     // old method:
            doshWriteLogEntry(pShutdownData->ShutdownLogFile,
                              "Saving INIs, old method (prfhSaveINIs)...");
            arc = prfhSaveINIs(pShutdownData->habShutdownThread,
                               NULL, // pShutdownData->ShutdownLogFile, @@todo
                               (PFNWP)fncbUpdateINIStatus,
                                   // callback function for updating the progress bar
                               pShutdownData->hwndProgressBar,
                                   // status window handle passed to callback
                               WM_UPDATEPROGRESSBAR,
                                   // message passed to callback
                               (PFNWP)fncbSaveINIError);
                                   // callback for errors
            doshWriteLogEntry(pShutdownData->ShutdownLogFile, "Done with prfhSaveINIs.");
        break;

        default:    // includes 0, new method
#endif
            doshWriteLogEntry(pShutdownData->ShutdownLogFile, "Saving INIs, new method (xprfSaveINIs)...");

            arc = xprfSaveINIs(pShutdownData->habShutdownThread,
                               xsd_fnSaveINIsProgress,
                               pShutdownData->hwndProgressBar);
            doshWriteLogEntry(pShutdownData->ShutdownLogFile, "Done with xprfSaveINIs.");
#ifndef __NOXSHUTDOWN__
        break;
    }
#endif

    if (arc != NO_ERROR)
    {
        doshWriteLogEntry(pShutdownData->ShutdownLogFile, "--- Error %d was reported!", arc);

        // error occured: ask whether to restart the WPS
        if (cmnMessageBoxMsg(pShutdownData->SDConsts.hwndShutdownStatus,
                             110,
                             111,
                             MB_YESNO)
                    == MBID_YES)
        {
            doshWriteLogEntry(pShutdownData->ShutdownLogFile, "User requested to restart Desktop.");
            xsdRestartWPS(pShutdownData->habShutdownThread,
                          FALSE);
                    // doesn't return
        }
    }

    doshWriteLogEntry(pShutdownData->ShutdownLogFile, "Now preparing shutdown...");

    // always say "releasing filesystems"
    WinShowPointer(HWND_DESKTOP, FALSE);
    WinSetDlgItemText(pShutdownData->SDConsts.hwndShutdownStatus,
                      ID_SDDI_STATUS,
                      cmnGetString(ID_SDSI_FLUSHING)) ; // pszSDFlushing

    // here comes the settings-dependent part:
    // depending on what we are to do after shutdown,
    // we switch to different routines which handle
    // the rest.
    // I guess this is a better solution than putting
    // all this stuff in the same routine because after
    // DosShutdown(), even the swapper file is blocked,
    // and if the following code is in the swapper file,
    // it cannot be executed. From user reports, I suspect
    // this has happened on some memory-constrained
    // systems with XFolder < V0.84. So we better put
    // all the code together which will be used together.

    /* if (pShutdownData->sdParams.optAnimate)
        // hps for animation later
        // moved this here V0.9.12 (2001-05-12) [umoeller]
        hpsScreen = WinGetScreenPS(HWND_DESKTOP); */

    if (pShutdownData->sdParams.optReboot)
    {
        // reboot:
        if (strlen(pShutdownData->sdParams.szRebootCommand) == 0)
            // no user reboot action:
            xsdFinishStandardReboot(pShutdownData);
        else
            // user reboot action:
            xsdFinishUserReboot(pShutdownData);
    }
    else if (pShutdownData->sdParams.optDebug)
    {
        // debug mode: just sleep a while
        DosSleep(2000);
    }
    else if (pShutdownData->sdParams.optAPMPowerOff)
    {
        // no reboot, but APM power off?
        xsdFinishAPMPowerOff(pShutdownData);
    }
    else
        // normal shutdown: show message
        xsdFinishStandardMessage(pShutdownData);

    // the xsdFinish* functions never return;
    // so we only get here in debug mode and
    // return
}

/*
 *@@ PowerOffAnim:
 *      calls anmPowerOff with the proper timings
 *      to display the cute power-off animation.
 *
 *@@added V0.9.7 (2000-12-13) [umoeller]
 */

static VOID PowerOffAnim(HPS hpsScreen)
{
    anmPowerOff(hpsScreen,
                500, 800, 200, 300);
}

/*
 *@@ xsdFinishStandardMessage:
 *      this finishes the shutdown procedure,
 *      displaying the "Shutdown is complete..."
 *      window and halting the system.
 *
 *      Runs on the Shutdown thread.
 *
 *@@changed V0.9.12 (2001-05-12) [umoeller]: animations frequently didn't show up, fixed
 *@@changed V0.9.17 (2002-02-05) [pr]: fix text not displaying by making it a 2 stage message
 */

VOID xsdFinishStandardMessage(PSHUTDOWNDATA pShutdownData)
{
    ULONG flShutdown = 0;
    PSZ pszComplete = cmnGetString(ID_SDDI_COMPLETE);
    PSZ pszSwitchOff = cmnGetString(ID_SDDI_SWITCHOFF);

    HPS hpsScreen = WinGetScreenPS(HWND_DESKTOP);

    // setup Ctrl+Alt+Del message window; this needs to be done
    // before DosShutdown, because we won't be able to reach the
    // resource files after that
    HWND hwndCADMessage = WinLoadDlg(HWND_DESKTOP, NULLHANDLE,
                                     WinDefDlgProc,
                                     pShutdownData->hmodResource,
                                     ID_SDD_CAD,
                                     NULL);
    winhCenterWindow(hwndCADMessage);  // wnd is still invisible

#ifndef __NOXSHUTDOWN__
    flShutdown = cmnQuerySetting(sflXShutdown);
#endif

    if (pShutdownData->ShutdownLogFile)
    {
        doshWriteLogEntry(pShutdownData->ShutdownLogFile, "xsdFinishStandardMessage: Calling DosShutdown(0), closing log.");
        doshClose(&pShutdownData->ShutdownLogFile);
        pShutdownData->ShutdownLogFile = NULL;
    }

    if (flShutdown & XSD_ANIMATE_SHUTDOWN)
        // cute power-off animation
        PowerOffAnim(hpsScreen);
    else
        // only hide the status window if
        // animation is off, because otherwise
        // PM will repaint part of the animation
        WinShowWindow(pShutdownData->SDConsts.hwndShutdownStatus, FALSE);

    // now, this is fun:
    // here's a fine example of how to completely
    // block the system without ANY chance to escape.
    // -- show "Shutdown in progress..." window
    WinShowWindow(hwndCADMessage, TRUE);
    // -- make the CAD message system-modal
    WinSetSysModalWindow(HWND_DESKTOP, hwndCADMessage);
    // -- kill the tasklist (/helpers/winh.c)
    // so that Ctrl+Esc fails
    winhKillTasklist();
    // -- block all other WPS threads
    DosEnterCritSec();
    // -- block all file access
    DosShutdown(0); // V0.9.16 (2002-02-03) [pr]: moved this down
    // -- update the message
    WinSetDlgItemText(hwndCADMessage, ID_SDDI_PROGRESS1, pszComplete);
    WinSetDlgItemText(hwndCADMessage, ID_SDDI_PROGRESS2, pszSwitchOff);
    // -- and now loop forever!
    while (TRUE)
        DosSleep(10000);
}

/*
 *@@ xsdFinishStandardReboot:
 *      this finishes the shutdown procedure,
 *      rebooting the computer using the standard
 *      reboot procedure (DOS.SYS).
 *      There's no shutdown animation here.
 *
 *      Runs on the Shutdown thread.
 *
 *@@changed V0.9.3 (2000-05-22) [umoeller]: added reboot animation
 *@@changed V0.9.12 (2001-05-12) [umoeller]: animations frequently didn't show up, fixed
 *@@changed V0.9.16 (2002-01-05) [umoeller]: fixed hang on loading string resource
 */

VOID xsdFinishStandardReboot(PSHUTDOWNDATA pShutdownData)
{
    ULONG       flShutdown = 0;
    HFILE       hIOCTL;
    ULONG       ulAction;
    BOOL        fShowRebooting = TRUE;
    // load string resource before shutting down
    // V0.9.16 (2002-01-05) [umoeller]
    PSZ         pszRebooting = cmnGetString(ID_SDSI_REBOOTING);
    HPS         hpsScreen = WinGetScreenPS(HWND_DESKTOP);

#ifndef __NOXSHUTDOWN__
    flShutdown = cmnQuerySetting(sflXShutdown);
#endif

    // if (optReboot), open DOS.SYS; this
    // needs to be done before DosShutdown() also
    if (DosOpen("\\DEV\\DOS$", &hIOCTL, &ulAction, 0L,
               FILE_NORMAL,
               OPEN_ACTION_OPEN_IF_EXISTS,
               OPEN_SHARE_DENYNONE | OPEN_ACCESS_READWRITE,
               0L)
        != NO_ERROR)
    {
        winhDebugBox(HWND_DESKTOP, "XShutdown", "The DOS.SYS device driver could not be opened. "
                 "XShutdown will be unable to reboot your computer. "
                 "Please consult the XFolder Online Reference for a remedy of this problem.");
    }

    if (pShutdownData->ShutdownLogFile)
    {
        doshWriteLogEntry(pShutdownData->ShutdownLogFile, "xsdFinishStandardReboot: Opened DOS.SYS, hFile: 0x%lX", hIOCTL);
        doshWriteLogEntry(pShutdownData->ShutdownLogFile, "xsdFinishStandardReboot: Calling DosShutdown(0), closing log.");
        doshClose(&pShutdownData->ShutdownLogFile);
        pShutdownData->ShutdownLogFile = NULL;
    }

    if (flShutdown & XSD_ANIMATE_REBOOT)  // V0.9.3 (2000-05-22) [umoeller]
    {
        // cute power-off animation
        PowerOffAnim(hpsScreen);
        fShowRebooting = FALSE;
    }

    DosShutdown(0);
        // @@todo what to do if this fails?

    // say "Rebooting..." if we had no animation
    if (fShowRebooting)
    {
        WinSetDlgItemText(pShutdownData->SDConsts.hwndShutdownStatus, ID_SDDI_STATUS,
                          pszRebooting) ; // pszSDRebooting
        DosSleep(500);
    }

    // restart the machine via DOS.SYS (opened above)
    DosDevIOCtl(hIOCTL, 0xd5, 0xab, NULL, 0, NULL, NULL, 0, NULL);

    // don't know if this function returns, but
    // we better make sure we don't return from this
    // function
    while (TRUE)
        DosSleep(10000);
}

/*
 *@@ xsdFinishUserReboot:
 *      this finishes the shutdown procedure,
 *      rebooting the computer by starting a
 *      user reboot command.
 *      There's no shutdown animation here.
 *
 *      Runs on the Shutdown thread.
 *
 *@@changed V0.9.3 (2000-05-22) [umoeller]: added reboot animation
 *@@changed V0.9.12 (2001-05-12) [umoeller]: animations frequently didn't show up, fixed
 */

VOID xsdFinishUserReboot(PSHUTDOWNDATA pShutdownData)
{
    // user reboot item: in this case, we don't call
    // DosShutdown(), which is supposed to be done by
    // the user reboot command
    ULONG   flShutdown = 0;
    CHAR    szTemp[CCHMAXPATH];
    PID     pid;
    ULONG   sid;
    HPS hpsScreen = WinGetScreenPS(HWND_DESKTOP);

#ifndef __NOXSHUTDOWN__
    flShutdown = cmnQuerySetting(sflXShutdown);
#endif

    if (flShutdown & XSD_ANIMATE_REBOOT)        // V0.9.3 (2000-05-22) [umoeller]
        // cute power-off animation
        PowerOffAnim(hpsScreen);
    else
    {
        sprintf(szTemp,
                cmnGetString(ID_SDSI_STARTING),  // pszStarting
                pShutdownData->sdParams.szRebootCommand);
        WinSetDlgItemText(pShutdownData->SDConsts.hwndShutdownStatus, ID_SDDI_STATUS,
                          szTemp);
    }

    if (pShutdownData->ShutdownLogFile)
    {
        doshWriteLogEntry(pShutdownData->ShutdownLogFile, "Trying to start user reboot cmd: %s, closing log.",
               pShutdownData->sdParams.szRebootCommand);
        doshClose(&pShutdownData->ShutdownLogFile);
        pShutdownData->ShutdownLogFile = NULL;
    }

    sprintf(szTemp, "/c %s", pShutdownData->sdParams.szRebootCommand);
    if (doshQuickStartSession("cmd.exe",
                              szTemp,
                              FALSE, // background
                              SSF_CONTROL_INVISIBLE, // but auto-close
                              TRUE,  // wait flag
                              &sid,
                              &pid,
                              NULL)
               != NO_ERROR)
    {
        winhDebugBox(HWND_DESKTOP,
                     "XShutdown",
                     "The user-defined restart command failed. We will now restart the WPS.");
        xsdRestartWPS(pShutdownData->habShutdownThread,
                      FALSE);
    }
    else
        // no error:
        // we'll always get here, since the user reboot
        // is now taking place in a different process, so
        // we just stop here
        while (TRUE)
            DosSleep(10000);
}

/*
 *@@ xsdFinishAPMPowerOff:
 *      this finishes the shutdown procedure,
 *      using the functions in apm.c.
 *
 *      Runs on the Shutdown thread.
 *
 *@@changed V0.9.12 (2001-05-12) [umoeller]: animations frequently didn't show up, fixed
 */

VOID xsdFinishAPMPowerOff(PSHUTDOWNDATA pShutdownData)
{
    CHAR        szAPMError[500];
    ULONG       ulrcAPM = 0;
    ULONG       flShutdown = 0;
    HPS         hpsScreen = WinGetScreenPS(HWND_DESKTOP);

#ifndef __NOXSHUTDOWN__
    flShutdown = cmnQuerySetting(sflXShutdown);
#endif

    // prepare APM power off
    ulrcAPM = apmPreparePowerOff(szAPMError);
    doshWriteLogEntry(pShutdownData->ShutdownLogFile, "xsdFinishAPMPowerOff: apmPreparePowerOff returned 0x%lX",
            ulrcAPM);

    if (ulrcAPM & APM_IGNORE)
    {
        // if this returns APM_IGNORE, we continue
        // with the regular shutdown w/out APM
        doshWriteLogEntry(pShutdownData->ShutdownLogFile, "APM_IGNORE, continuing with normal shutdown",
               ulrcAPM);

        xsdFinishStandardMessage(pShutdownData);
        // this does not return
    }
    else if (ulrcAPM & APM_CANCEL)
    {
        // APM_CANCEL means cancel shutdown
        WinShowPointer(HWND_DESKTOP, TRUE);
        doshWriteLogEntry(pShutdownData->ShutdownLogFile, "  APM error message: %s",
               szAPMError);

        cmnMessageBox(HWND_DESKTOP, "APM Error", szAPMError,
                MB_CANCEL | MB_SYSTEMMODAL);
        // restart Desktop
        xsdRestartWPS(pShutdownData->habShutdownThread,
                      FALSE);
        // this does not return
    }
    // else: APM_OK means preparing went alright

    /* if (ulrcAPM & APM_DOSSHUTDOWN_0)
    {
        // shutdown request by apm.c:
        if (pShutdownData->ShutdownLogFile)
        {
            doshWriteLogEntry(pShutdownData->ShutdownLogFile, "xsdFinishAPMPowerOff: Calling DosShutdown(0), closing log.");
            fclose(pShutdownData->ShutdownLogFile);
            pShutdownData->ShutdownLogFile = NULL;
        }

        DosShutdown(0);
    } */
    // if apmPreparePowerOff requested this,
    // do DosShutdown(1)
    if (ulrcAPM & APM_DOSSHUTDOWN_1)
    {
        if (pShutdownData->ShutdownLogFile)
        {
            doshWriteLogEntry(pShutdownData->ShutdownLogFile, "xsdFinishAPMPowerOff: Calling DosShutdown(1), closing log.");
            doshClose(&pShutdownData->ShutdownLogFile);
            pShutdownData->ShutdownLogFile = NULL;
        }

        DosShutdown(1);
    }
    // or if apmPreparePowerOff requested this,
    // do no DosShutdown()

    if (pShutdownData->ShutdownLogFile)
    {
        doshClose(&pShutdownData->ShutdownLogFile);
        pShutdownData->ShutdownLogFile = NULL;
    }

    if (flShutdown & XSD_ANIMATE_SHUTDOWN)
        // cute power-off animation
        PowerOffAnim(hpsScreen);
    else
        // only hide the status window if
        // animation is off, because otherwise
        // we get screen display errors
        // (hpsScreen...)
        WinShowWindow(pShutdownData->SDConsts.hwndShutdownStatus, FALSE);

    // if APM power-off was properly prepared
    // above, this flag is still TRUE; we
    // will now call the function which
    // actually turns the power off
    apmDoPowerOff(pShutdownData->sdParams.optAPMDelay);
    // we do _not_ return from that function
}

/* ******************************************************************
 *                                                                  *
 *   Update thread                                                  *
 *                                                                  *
 ********************************************************************/

/*
 *@@ fntUpdateThread:
 *          this thread is responsible for monitoring the window list
 *          while XShutdown is running and windows are being closed.
 *
 *          It is created with a PM message queue from fntShutdown
 *          when shutdown is about to begin.
 *
 *          It builds an internal second PSHUTLISTITEM linked list
 *          from the PM window list every 100 ms and then compares
 *          it to the global pliShutdownFirst.
 *
 *          If they are different, this means that windows have been
 *          closed (or even maybe opened), so fnwpShutdownThread is posted
 *          ID_SDMI_UPDATESHUTLIST so that it can rebuild pliShutdownFirst
 *          and react accordingly, in most cases, close the next window.
 *
 *          This thread monitors its THREADINFO.fExit flag and exits if
 *          it is set to TRUE. See thrClose (threads.c) for how this works.
 *
 *          Global resources used by this thread:
 *          -- HEV hevUpdated: event semaphore posted by Shutdown thread
 *                             when it's done updating its shutitem list.
 *                             We wait for this semaphore after having
 *                             posted ID_SDMI_UPDATESHUTLIST.
 *          -- PLINKLIST pllShutdown: the global list of SHUTLISTITEMs
 *                                    which are left to be closed. This is
 *                                    also used by the Shutdown thread.
 *          -- HMTX hmtxShutdown: mutex semaphore for protecting pllShutdown.
 *
 *@@changed V0.9.0 [umoeller]: code has been re-ordered for semaphore safety.
 */

static void _Optlink fntUpdateThread(PTHREADINFO ptiMyself)
{
    PSZ             pszErrMsg = NULL;

    PSHUTDOWNDATA   pShutdownData = (PSHUTDOWNDATA)ptiMyself->ulData;

    BOOL            fSemOwned = FALSE;
    LINKLIST        llTestList;

    DosSetPriority(PRTYS_THREAD,
                   PRTYC_REGULAR,
                   +31,          // priority delta
                   0);           // this thread


    lstInit(&llTestList, TRUE);     // auto-free items

    TRY_LOUD(excpt1)
    {
        ULONG           ulShutItemCount = 0,
                        ulTestItemCount = 0;
        BOOL            fUpdated = FALSE;

        WinCancelShutdown(ptiMyself->hmq, TRUE);

        while (!G_tiUpdateThread.fExit)
        {
            ULONG ulDummy;

            // this is the first loop: we arrive here every time
            // the task list has changed */
            #ifdef DEBUG_SHUTDOWN
                _Pmpf(( "UT: Waiting for update..." ));
            #endif

            DosResetEventSem(pShutdownData->hevUpdated, &ulDummy);
                        // V0.9.9 (2001-04-04) [umoeller]

            // have Shutdown thread update its list of items then
            WinPostMsg(pShutdownData->SDConsts.hwndMain, WM_COMMAND,
                       MPFROM2SHORT(ID_SDMI_UPDATESHUTLIST, 0),
                       MPNULL);
            fUpdated = FALSE;

            // shutdown thread is waiting for us to post the
            // "ready" semaphore, so do this now
            DosPostEventSem(ptiMyself->hevRunning);

            // now wait until Shutdown thread is done updating its
            // list; it then posts an event semaphore
            while (     (!fUpdated)
                    &&  (!G_tiUpdateThread.fExit)
                  )
            {
                if (G_tiUpdateThread.fExit)
                {
                    // we're supposed to exit:
                    fUpdated = TRUE;
                    #ifdef DEBUG_SHUTDOWN
                        _Pmpf(( "UT: Exit recognized" ));
                    #endif
                }
                else
                {
                    ULONG   ulUpdate;
                    // query event semaphore post count
                    DosQueryEventSem(pShutdownData->hevUpdated, &ulUpdate);
                    fUpdated = (ulUpdate > 0);
                    #ifdef DEBUG_SHUTDOWN
                        _Pmpf(( "UT: update recognized" ));
                    #endif
                    DosSleep(100);
                }
            } // while (!fUpdated);

            ulTestItemCount = 0;
            ulShutItemCount = 0;

            #ifdef DEBUG_SHUTDOWN
                _Pmpf(( "UT: Waiting for task list change, loop 2..." ));
            #endif

            while (     (ulTestItemCount == ulShutItemCount)
                     && (!G_tiUpdateThread.fExit)
                  )
            {
                // ULONG ulNesting = 0;

                // this is the second loop: we stay in here until the
                // task list has changed; for monitoring this, we create
                // a second task item list similar to the pliShutdownFirst
                // list and compare the two
                lstClear(&llTestList);

                // create a test list for comparing the task list;
                // this is our private list, so we need no mutex
                // semaphore
                xsdBuildShutList(ptiMyself->hab,
                                 pShutdownData,
                                 &llTestList);

                // count items in the test list
                ulTestItemCount = lstCountItems(&llTestList);

                // count items in the list of the Shutdown thread;
                // here we need a mutex semaphore, because the
                // Shutdown thread might be working on this too
                TRY_LOUD(excpt2)
                {
                    fSemOwned = (WinRequestMutexSem(pShutdownData->hmtxShutdown, 4000) == NO_ERROR);
                    if (fSemOwned)
                    {
                        ulShutItemCount = lstCountItems(&pShutdownData->llShutdown);
                        DosReleaseMutexSem(pShutdownData->hmtxShutdown);
                        fSemOwned = FALSE;
                    }
                }
                CATCH(excpt2) {} END_CATCH();

                if (!G_tiUpdateThread.fExit)
                    DosSleep(100);
            } // end while; loop until either the Shutdown thread has set the
              // Exit flag or the list has changed

            #ifdef DEBUG_SHUTDOWN
                _Pmpf(( "UT: Change or exit recognized" ));
            #endif
        }  // end while; loop until exit flag set
    } // end TRY_LOUD(excpt1)
    CATCH(excpt1)
    {
        // exception occured:
        // complain to the user
        if (pszErrMsg == NULL)
        {
            if (fSemOwned)
            {
                DosReleaseMutexSem(pShutdownData->hmtxShutdown);
                fSemOwned = FALSE;
            }

            // only report the first error, or otherwise we will
            // jam the system with msg boxes
            pszErrMsg = malloc(1000);
            if (pszErrMsg)
            {
                strcpy(pszErrMsg, "An error occured in the XFolder Update thread. "
                        "In the root directory of your boot drive, you will find a "
                        "file named XFLDTRAP.LOG, which contains debugging information. "
                        "If you had shutdown logging enabled, you will also find the "
                        "file XSHUTDWN.LOG there. If not, please enable shutdown "
                        "logging in the Desktop's settings notebook. ");
                krnPostThread1ObjectMsg(T1M_EXCEPTIONCAUGHT, (MPARAM)pszErrMsg,
                        (MPARAM)0); // don't enforce Desktop restart

                doshWriteLogEntry(pShutdownData->ShutdownLogFile, "\n*** CRASH\n%s\n", pszErrMsg);
            }
        }
    } END_CATCH();

    // clean up
    #ifdef DEBUG_SHUTDOWN
        _Pmpf(( "UT: Exiting..." ));
    #endif

    if (fSemOwned)
    {
        // release our mutex semaphore
        DosReleaseMutexSem(pShutdownData->hmtxShutdown);
        fSemOwned = FALSE;
    }

    lstClear(&llTestList);

    #ifdef DEBUG_SHUTDOWN
        DosBeep(100, 100);
    #endif
}


