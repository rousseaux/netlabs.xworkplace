
/*
 *@@sourcefile archives.c:
 *      the purpose of this code is to examine the WPS's status
 *      by evaluating dates, directory and system INI file contents.
 *      We can then draw the conclusion (depending on user settings)
 *      whether the WPS backup functionality should be enabled.
 *
 *      These checks are performed in arcCheckIfBackupNeeded, which
 *      gets called during WPS startup.
 *
 *      This file is ALL new with V0.9.0.
 *
 *      This is based on ideas in the WPSArcO code which has kindly
 *      been provided by Stefan Milcke (Stefan.Milcke@t-online.de).
 *
 *      All funtions in this file have the arc* prefix.
 *
 *      Note that the WPSARCOSETTINGS are only manipulated by the
 *      new Desktop "Archives" notebook page, whose code is
 *      in xfdesk.c. For the archiving settings, we do not use
 *      the GLOBALSETTINGS structure (as the rest of XWorkplace
 *      does), but this separate structure instead. See arcQuerySettings.
 *
 *@@added V0.9.0 [umoeller]
 *@@header "startshut\archives.h"
 */

/*
 *      Copyright (C) 1999-2000 Ulrich M”ller.
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
 *@@todo:
 *  csm:
         Ok, no guarantee, but this is what I found out:
             Offset 0xCF: (int32)    "Dateien bei jedem Systemstart aktivieren"
                                         (0 = off, 1 = on/selected)
             Offset 0xD9: (int32)    "Anzeige der Optionen bei jedem Neustart"
                                         (0 = off, 2 = on/selected)
             Offset 0xDD: (int32)    "Zeitsperre fuer Anzeigen der Optionen"
                                         (0 to 999)
         The rest of the file I don't know (besides the path
         beginning at offset 0x06, 200 bytes long)
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

#define INCL_DOSDATETIME
#define INCL_WINDIALOGS
#define INCL_WINBUTTONS
#define INCL_WINSTDSPIN
#define INCL_WINSHELLDATA       // Prf* functions
#include <os2.h>

#include <string.h>
#include <stdio.h>
#include <io.h>
#include <fcntl.h>
#include <sys\stat.h>
#include <time.h>
#include <process.h>
#include <math.h>

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\datetime.h"           // date/time helpers
#include "helpers\dosh.h"
#include "helpers\prfh.h"
#include "helpers\stringh.h"
#include "helpers\winh.h"               // PM helper routines
#include "helpers\wphandle.h"           // Henk Kelder's HOBJECT handling
#include "helpers\xstring.h"            // extended string helpers

// SOM headers which don't crash with prec. header files
#include "xfdesk.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\notebook.h"            // generic XWorkplace notebook handling

#include "startshut\archives.h"         // WPSArcO declarations

// other SOM headers
#pragma hdrstop                 // VAC++ keeps crashing otherwise

/********************************************************************
 *                                                                  *
 *   Global variables                                               *
 *                                                                  *
 ********************************************************************/

static WPSARCOSETTINGS     G_ArcSettings;
static DATETIME            G_dtLastArchived;
static BOOL                G_fSettingsLoaded = FALSE;

static CHAR                G_szArcBaseFilename[CCHMAXPATH] = "";

#define ARC_FILE_OFFSET    0x000CF

/********************************************************************
 *                                                                  *
 *   "Archives" page replacement in WPDesktop                       *
 *                                                                  *
 ********************************************************************/

#define PERCENTAGES_COUNT 11
// 8 PSZ's for percentage spinbutton
static PSZ     G_apszPercentages[PERCENTAGES_COUNT];

/*
 * arcArchivesInitPage:
 *      notebook callback function (notebook.c) for the
 *      "Archives" page replacement in the Desktop's settings
 *      notebook.
 *      Sets the controls on the page according to the
 *      Global Settings.
 *
 *@@added V0.9.0 [umoeller]
 */

VOID arcArchivesInitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                          ULONG flFlags)        // CBI_* flags (notebook.h)
{
    PWPSARCOSETTINGS pArcSettings = arcQuerySettings();

    if (flFlags & CBI_INIT)
    {
        if (pcnbp->pUser == NULL)
        {
            // first call: backup Global Settings for "Undo" button;
            // this memory will be freed automatically by the
            // common notebook window function (notebook.c) when
            // the notebook page is destroyed
            pcnbp->pUser = malloc(sizeof(WPSARCOSETTINGS));
            memcpy(pcnbp->pUser, pArcSettings, sizeof(WPSARCOSETTINGS));

            memset(&G_apszPercentages, 0, sizeof(G_apszPercentages));
            G_apszPercentages[0]  = "0.010";
            G_apszPercentages[1]  = "0.025";
            G_apszPercentages[2]  = "0.050";
            G_apszPercentages[3]  = "0.075";
            G_apszPercentages[4]  = "0.100";
            G_apszPercentages[5]  = "0.250";
            G_apszPercentages[6]  = "0.500";
            G_apszPercentages[7]  = "0.750";
            G_apszPercentages[8]  = "1.000";
            G_apszPercentages[9]  = "2.500";
            G_apszPercentages[10] = "5.000";
        }

        WinSendDlgItemMsg(pcnbp->hwndDlgPage, ID_XSDI_ARC_INI_SPIN,
                          SPBM_SETARRAY,
                          (MPARAM)&G_apszPercentages,
                          (MPARAM)PERCENTAGES_COUNT);       // array size
    }

    if (flFlags & CBI_SET)
    {
        CHAR        cArchivesCount = 0;
        ULONG       ul = 0;

        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_ARC_ENABLE,
                              (pArcSettings->ulArcFlags & ARCF_ENABLED) != 0);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_ARC_ALWAYS,
                              (pArcSettings->ulArcFlags & ARCF_ALWAYS) != 0);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_ARC_NEXT,
                              (pArcSettings->ulArcFlags & ARCF_NEXT) != 0);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_ARC_INI,
                              (pArcSettings->ulArcFlags & ARCF_INI) != 0);

        // INI files percentage:
        // go thru the spin button array and find
        // percentage that matches
        for (ul = 0; ul < PERCENTAGES_COUNT; ul++)
        {
            float dTemp;
            CHAR szTempx[100];
            // convert current array item to float
            sscanf(G_apszPercentages[ul], "%f", &dTemp);

            // same?
            if (fabs(dTemp - pArcSettings->dIniFilesPercent) < 0.00001)
                                // prevent rounding errors
            {
                // yes: set this spin button array item
                WinSendDlgItemMsg(pcnbp->hwndDlgPage, ID_XSDI_ARC_INI_SPIN,
                                  SPBM_SETCURRENTVALUE,
                                  (MPARAM)ul,
                                  (MPARAM)NULL);
                break;
            }
        }

        // every xxx days
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_ARC_DAYS,
                              (pArcSettings->ulArcFlags & ARCF_DAYS) != 0);
        winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_XSDI_ARC_DAYS_SPIN,
                               1, 50,       // spin button limits
                               pArcSettings->ulEveryDays);

        // status
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_ARC_SHOWSTATUS,
                              pArcSettings->fShowStatus);

        // no. of archives
        arcSetNumArchives(&cArchivesCount,
                          FALSE);       // query
        winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_XSDI_ARC_ARCHIVES_SPIN,
                               1, 9,        // spin button limits
                               cArchivesCount);
    }

    if (flFlags & CBI_ENABLE)
    {
        BOOL    fEnabled = ((pArcSettings->ulArcFlags & ARCF_ENABLED) != 0),
                fAlways = ((pArcSettings->ulArcFlags & ARCF_ALWAYS) != 0),
                fINI = winhIsDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_ARC_INI),
                fDays = winhIsDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_ARC_DAYS);
        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_ARC_ALWAYS,
                          fEnabled);
        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_ARC_NEXT,
                          fEnabled && !fAlways);
        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_ARC_INI,
                          fEnabled && !fAlways);
        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_ARC_INI_SPIN,
                          ( (fEnabled) && (fINI) ) && !fAlways);
        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_ARC_INI_SPINTXT1,
                          ( (fEnabled) && (fINI) ) && !fAlways);
        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_ARC_DAYS,
                          fEnabled && !fAlways);
        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_ARC_DAYS_SPIN,
                          ( (fEnabled) && (fDays) ) && !fAlways);
        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_ARC_DAYS_SPINTXT1,
                          ( (fEnabled) && (fDays) ) && !fAlways);

        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_ARC_SHOWSTATUS,
                          fEnabled);
    }
}

/*
 * arcArchivesItemChanged:
 *      notebook callback function (notebook.c) for the
 *      "Archives" page replacement in the Desktop's settings
 *      notebook.
 *      Reacts to changes of any of the dialog controls.
 *
 *@@added V0.9.0 [umoeller]
 */

MRESULT arcArchivesItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                               ULONG ulItemID,
                               USHORT usNotifyCode,
                               ULONG ulExtra)      // for checkboxes: contains new state
{
    PWPSARCOSETTINGS pArcSettings = arcQuerySettings();
    ULONG           ulSetFlags = 0;
    BOOL            fSave = TRUE;

    switch (ulItemID)
    {
        case ID_XSDI_ARC_ENABLE:
            ulSetFlags |= ARCF_ENABLED;
        break;

        case ID_XSDI_ARC_ALWAYS:
            ulSetFlags |= ARCF_ALWAYS;
        break;

        case ID_XSDI_ARC_NEXT:
            ulSetFlags |= ARCF_NEXT;
        break;

        case ID_XSDI_ARC_INI:
            ulSetFlags |= ARCF_INI;
        break;

        case ID_XSDI_ARC_INI_SPIN:
            if (    (pcnbp->fPageInitialized)       // skip this during initialization
                 && (usNotifyCode == SPBN_CHANGE)
               )
            {
                CHAR    szTemp[100];
                float   flTemp = 0;
                // query current spin button array
                // item as a string
                WinSendDlgItemMsg(pcnbp->hwndDlgPage, ulItemID,
                                  SPBM_QUERYVALUE,
                                  (MPARAM)szTemp,
                                  MPFROM2SHORT(sizeof(szTemp),
                                               SPBQ_ALWAYSUPDATE));
                sscanf(szTemp, "%f", &flTemp);
                pArcSettings->dIniFilesPercent = (double)flTemp;
            }
        break;

        case ID_XSDI_ARC_DAYS:
            ulSetFlags |= ARCF_DAYS;
        break;

        case ID_XSDI_ARC_DAYS_SPIN:
            pArcSettings->ulEveryDays = winhAdjustDlgItemSpinData(pcnbp->hwndDlgPage,
                                                                  ulItemID,
                                                                  0,              // no grid
                                                                  usNotifyCode);
        break;

        case ID_XSDI_ARC_SHOWSTATUS:
            pArcSettings->fShowStatus = ulExtra;
        break;

        case ID_XSDI_ARC_ARCHIVES_SPIN:
        {
            CHAR    cArchivesCount = (CHAR)winhAdjustDlgItemSpinData(pcnbp->hwndDlgPage,
                                                                     ulItemID,
                                                                     0,              // no grid
                                                                     usNotifyCode);
            arcSetNumArchives(&cArchivesCount,
                              TRUE);        // set
            fSave = FALSE;
        break; }

        case DID_UNDO:
        {
            // "Undo" button: get pointer to backed-up Global Settings
            PWPSARCOSETTINGS pGSBackup = (PWPSARCOSETTINGS)(pcnbp->pUser);

            // and restore the settings for this page
            pArcSettings->ulArcFlags = pGSBackup->ulArcFlags;
            // pArcSettings->ulIniFilesPercent = pGSBackup->ulIniFilesPercent;
            pArcSettings->ulEveryDays = pGSBackup->ulEveryDays;

            // update the display by calling the INIT callback
            (*(pcnbp->pfncbInitPage))(pcnbp, CBI_SET | CBI_ENABLE);
        break; }

        case DID_DEFAULT:
        {
            // set the default settings for this settings page
            // (this is in common.c because it's also used at
            // WPS startup)
            arcSetDefaultSettings();
            // update the display by calling the INIT callback
            (*(pcnbp->pfncbInitPage))(pcnbp, CBI_SET | CBI_ENABLE);
        break; }

        default:
            fSave = FALSE;
    }

    if (ulSetFlags)
    {
        if (ulExtra)    // checkbox checked
            pArcSettings->ulArcFlags |= ulSetFlags;
        else            // checkbox unchecked
            pArcSettings->ulArcFlags &= ~ulSetFlags;
        // re-enable dlg items by calling the INIT callback
        (*(pcnbp->pfncbInitPage))(pcnbp, CBI_ENABLE);
    }

    if (fSave)
        // enable/disable items
        arcSaveSettings();

    return ((MPARAM)0);
}

/********************************************************************
 *                                                                  *
 *   WPSArcO settings                                               *
 *                                                                  *
 ********************************************************************/

/*
 *@@ arcSetDefaultSettings:
 *      this initializes the global WPSARCOSETTINGS
 *      structure with default values.
 */

VOID arcSetDefaultSettings(VOID)
{
    #ifdef DEBUG_STARTUP
        _Pmpf(("**** Settings defaults"));
    #endif
    G_ArcSettings.ulArcFlags = 0;
    G_ArcSettings.dIniFilesPercent = .1;
    G_ArcSettings.ulEveryDays = 1;
    G_ArcSettings.dAppsSizeLast = 0;
    G_ArcSettings.dKeysSizeLast = 0;
    G_ArcSettings.dDataSumLast = 0;
    G_ArcSettings.fShowStatus = TRUE;
}

/*
 *@@ arcQuerySettings:
 *      this returns the global WPSARCOSETTINGS
 *      structure, which is filled with the data
 *      from OS2.INI if this is queried for the
 *      first time.
 */

PWPSARCOSETTINGS arcQuerySettings(VOID)
{
    if (!G_fSettingsLoaded)
    {
        ULONG   cbData = sizeof(G_ArcSettings);
        BOOL    brc = FALSE;
        // first call:
        G_fSettingsLoaded = TRUE;
        // load settings
        brc = PrfQueryProfileData(HINI_USER,
                                  WPSARCO_INIAPP, WPSARCO_INIKEY_SETTINGS,    // archives.h
                                  &G_ArcSettings,
                                  &cbData);
        if ((!brc) || cbData != sizeof(G_ArcSettings))
            // data not found:
            arcSetDefaultSettings();

        cbData = sizeof(G_dtLastArchived);
        brc = PrfQueryProfileData(HINI_USER,
                                  WPSARCO_INIAPP, WPSACRO_INIKEY_LASTBACKUP,
                                  &G_dtLastArchived,
                                  &cbData);
        if ((!brc) || cbData != sizeof(G_dtLastArchived))
        {
            // data not found:
            DosGetDateTime(&G_dtLastArchived);
            G_dtLastArchived.year = 1990;       // enfore backup then
        }

        // initialize szArcBaseFilename
        sprintf(G_szArcBaseFilename,
                "%c:\\OS2\\BOOT\\ARCHBASE.$$$",
                doshQueryBootDrive());
    }

    return (&G_ArcSettings);
}

/*
 *@@ arcSaveSettings:
 *      this writes the WPSARCOSETTINGS structure
 *      back to OS2.INI.
 */

BOOL arcSaveSettings(VOID)
{
    return (PrfWriteProfileData(HINI_USER,
                                WPSARCO_INIAPP, WPSARCO_INIKEY_SETTINGS,    // archives.h
                                &G_ArcSettings,
                                sizeof(G_ArcSettings)));
}

/********************************************************************
 *                                                                  *
 *   Archiving Enabling                                             *
 *                                                                  *
 ********************************************************************/

/*
 * GetMarkerFilename:
 *
 */

BOOL GetMarkerFilename(PSZ pszFilename) // should be 2*CCHMAXPATH in size
{
    BOOL brc = FALSE;

    ULONG ulHandle;
    ULONG cbHandle = sizeof(ulHandle);
    if (PrfQueryProfileData(HINI_USER,
                            (PSZ)WPINIAPP_LOCATION, // "PM_Workplace:Location",
                            (PSZ)WPOBJID_DESKTOP, // "<WP_DESKTOP>",
                            &ulHandle,
                            &cbHandle))
    {
        // ulHandle now has handle of Desktop;
        // get its path from OS2SYS.INI
        if (wphQueryPathFromHandle(HINI_SYSTEM,
                                   ulHandle,
                                   pszFilename,
                                   2*CCHMAXPATH))
        {
            strcat(pszFilename, "\\");
            strcat(pszFilename, XWORKPLACE_ARCHIVE_MARKER);
            _Pmpf(("Marker file: %s", pszFilename));
            brc = TRUE;
        }
    }

    return (brc);
}

/*
 *@@ arcCheckIfBackupNeeded:
 *      this checks the system according to the settings
 *      in WPSARCOSETTINGS (i.e. always, next bootup,
 *      date, INI changes -- see arcCheckINIFiles),
 *      and calls arcSwitchArchivingOn according to the
 *      result of these checks.
 *
 *      This gets called from krnInitializeXWorkplace
 *      while the WPS is booting up (see remarks there).
 *      If we enable WPS archiving here, the WPS will
 *      archive the Desktop soon afterwards.
 *
 *      If (WPSARCOSETTINGS.fShowStatus == TRUE), hwndNotify
 *      will receive a msg with the value ulMsg and the
 *      HWND of the notification window in mp1 to be able
 *      to destroy it later.
 *
 *      krnInitializeXWorkplace sets this to the XWorkplace
 *      Thread-1 object window (krn_fnwpThread1Object), which in
 *      turn starts a timer to destroy the window later.
 *
 *@@changed V0.9.4 (2000-07-22) [umoeller]: archiving wasn't always disabled if turned off completely; fixed
 */

BOOL arcCheckIfBackupNeeded(HWND hwndNotify,        // in: window to notify
                            ULONG ulMsg)            // in: msg to post to hwndNotify
{
    // PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();
    BOOL    fBackup = FALSE,
            fDisableArchiving = FALSE;

    // force loading of settings
    arcQuerySettings();

    if (G_ArcSettings.ulArcFlags & ARCF_ENABLED)
    {
        CHAR    szMarkerFilename[2*CCHMAXPATH];
        HWND    hwndStatus = NULLHANDLE;
        BOOL    fShowWindow = TRUE;
        CHAR    szTemp[100];
        XSTRING strMsg;
        BOOL    fWasJustRestored = FALSE;

        xstrInit(&strMsg, 300);

        if (G_ArcSettings.fShowStatus)
        {
            hwndStatus = WinLoadDlg(HWND_DESKTOP, HWND_DESKTOP,
                                    WinDefDlgProc,
                                    cmnQueryNLSModuleHandle(FALSE),
                                    ID_XFD_ARCHIVINGSTATUS,
                                    NULL);
            cmnSetControlsFont(hwndStatus, 1, 10000);
        }

        if (GetMarkerFilename(szMarkerFilename))
        {
            if (access(szMarkerFilename, 0) == 0)
                // exists:
                // this means that this archive was just restored,
                // so disable archiving...
                fWasJustRestored = TRUE;
        }

        if (fWasJustRestored)
        {
            xstrcpy(&strMsg, cmnGetString(ID_XSSI_ARCRESTORED),  0); // pszArcRestored
            xstrcatc(&strMsg, '\n');
            fBackup = FALSE;
            fDisableArchiving = TRUE;
        }
        else
        {
            if (G_ArcSettings.ulArcFlags & ARCF_ALWAYS)
            {
                fBackup = TRUE;

                if (G_ArcSettings.fShowStatus)
                {
                    WinSetDlgItemText(hwndStatus, ID_XFDI_GENERICDLGTEXT,
                                      "WPS archiving enabled\n");
                    WinShowWindow(hwndStatus, TRUE);
                }
            }
            else
            {
                BOOL    fCheckINIs = (G_ArcSettings.ulArcFlags & ARCF_INI);

                if (G_ArcSettings.ulArcFlags & ARCF_NEXT)
                {
                    if (G_ArcSettings.fShowStatus)
                    {
                        WinSetDlgItemText(hwndStatus, ID_XFDI_GENERICDLGTEXT,
                                          "WPS archiving enabled once\n");
                        WinShowWindow(hwndStatus, TRUE);
                    }

                    fBackup = TRUE;
                    fCheckINIs = FALSE;     // not necessary

                    // unset this settings flag for next time
                    G_ArcSettings.ulArcFlags &= ~ARCF_NEXT;
                    arcSaveSettings();
                }
                else if (G_ArcSettings.ulArcFlags & ARCF_DAYS)
                {
                    // days-based:
                    DATETIME    dtNow;
                    LONG        lDaysNow,
                                lDaysLast,
                                lDaysPassed;

                    DosGetDateTime(&dtNow);
                    lDaysNow = dtDate2Scalar(dtNow.year,
                                             dtNow.month,
                                             dtNow.day);
                    lDaysLast= dtDate2Scalar(G_dtLastArchived.year,
                                             G_dtLastArchived.month,
                                             G_dtLastArchived.day);
                    lDaysPassed = lDaysNow - lDaysLast;

                    if (lDaysPassed >= G_ArcSettings.ulEveryDays)
                    {
                        fBackup = TRUE;
                        fCheckINIs = FALSE;     // not necessary
                    }

                    if (G_ArcSettings.fShowStatus)
                    {
                        sprintf(szTemp,
                                cmnGetString(ID_XSSI_ARCDAYSPASSED),  // "%d days passed since last backup." // pszArcDaysPassed
                                lDaysPassed);
                        xstrcpy(&strMsg, szTemp, 0);
                        xstrcatc(&strMsg, '\n');
                        sprintf(szTemp,
                                cmnGetString(ID_XSSI_ARCDAYSLIMIT),  // "Limit: %d days." // pszArcDaysLimit
                                G_ArcSettings.ulEveryDays);
                        xstrcat(&strMsg, szTemp, 0);
                        xstrcatc(&strMsg, '\n');
                    }
                }

                if (fCheckINIs)
                {
                    // INI-based:
                    double  dMaxDifferencePercent = 0;

                    if (G_ArcSettings.fShowStatus)
                    {
                        sprintf(szTemp,
                                "%s\n",
                                cmnGetString(ID_XSSI_ARCINICHECKING)) ; // pszArcINIChecking
                        WinSetDlgItemText(hwndStatus, ID_XFDI_GENERICDLGTEXT,
                                          szTemp);
                        WinShowWindow(hwndStatus, TRUE);
                    }

                    fBackup = arcCheckINIFiles(&G_ArcSettings.dIniFilesPercent,
                                               WPSARCO_INIAPP,  // ignore this
                                               &G_ArcSettings.dAppsSizeLast,
                                               &G_ArcSettings.dKeysSizeLast,
                                               &G_ArcSettings.dDataSumLast,
                                               &dMaxDifferencePercent);

                    if (G_ArcSettings.fShowStatus)
                    {
                        sprintf(szTemp,
                                cmnGetString(ID_XSSI_ARCINICHANGED),  // "INI files changed %f %%", // pszArcINIChanged
                                dMaxDifferencePercent);
                        xstrcpy(&strMsg, szTemp, 0);
                        xstrcatc(&strMsg, '\n');
                        sprintf(szTemp,
                                cmnGetString(ID_XSSI_ARCINILIMIT),  // "Limit: %f %%." // pszArcINILimit
                                G_ArcSettings.dIniFilesPercent);
                        xstrcat(&strMsg, szTemp, 0);
                        xstrcatc(&strMsg, '\n');
                    }
                } // end if (fCheckINIs)
            } // end else if (G_ArcSettings.ulArcFlags & ARCF_ALWAYS)
        } // else if (fWasJustRestored)

        if (strMsg.ulLength)
        {
            if (G_ArcSettings.fShowStatus)
            {
                if (fBackup)
                {
                    // archiving to be turned on:
                    // save "last app" etc. data so we won't get this twice
                    arcSaveSettings();
                    xstrcat(&strMsg,
                            cmnGetString(ID_XSSI_ARCENABLED),  // "WPS archiving enabled", // pszArcEnabled
                            0);
                }
                else
                    xstrcat(&strMsg,
                            cmnGetString(ID_XSSI_ARCNOTNECC),  // "WPS archiving not necessary", // pszArcNotNecc
                            0);

                WinSetDlgItemText(hwndStatus, ID_XFDI_GENERICDLGTEXT, strMsg.psz);
                WinShowWindow(hwndStatus, TRUE);
            }
        }

        xstrClear(&strMsg);

        if (G_ArcSettings.fShowStatus)
            WinPostMsg(hwndNotify, ulMsg, (MPARAM)hwndStatus, (MPARAM)NULL);
    }

    arcSwitchArchivingOn(fBackup); // moved V0.9.4 (2000-07-22) [umoeller]

    if (fDisableArchiving)
    {
        G_ArcSettings.ulArcFlags &= ~ARCF_ENABLED;
        arcSaveSettings();
        cmnMessageBoxMsg(NULLHANDLE,
                         116,
                         188,
                         MB_OK);
    }

    return (fBackup);
}

/*
 *@@ arcSwitchArchivingOn:
 *      depending on switchOn, this switches WPS archiving on or off
 *      by manipulating the \OS2\BOOT\ARCHBASE.$$$ file.
 *
 *      In addition, this stores the date of the last archive in OS2.INI
 *      and creates a file on the desktop to mark this as an archive.
 *
 *      This should only be called by arcCheckIfBackupNeeded.
 */

int arcSwitchArchivingOn(BOOL switchOn)
{
    int             file;
    INT             rc;
    char            byte[1];

    if (switchOn == TRUE)
        byte[0] = (char)1;
    else
        byte[0] = (char)0;

    // because the archive information file is write protected we must switch
    // write protection of to write informations to it
    chmod(G_szArcBaseFilename, S_IREAD | S_IWRITE);
    // now open the file
    file = open(G_szArcBaseFilename, O_BINARY | O_RDWR, 0);
    if (file != -1)
    {
        // seek to the position of the archiving flag
        if (lseek(file, ARC_FILE_OFFSET, SEEK_SET) != -1)
        {
            // write new archive flag
            if (write(file, byte, 1))
            {
                // print message to stdout
                if (switchOn)
                {
                    CHAR szMarkerFilename[2*CCHMAXPATH];
                    #ifdef DEBUG_STARTUP
                        _Pmpf(("WPS Archiving activated"));
                    #endif
                    // store date of backup in OS2.INI
                    DosGetDateTime(&G_dtLastArchived);
                    PrfWriteProfileData(HINI_USER,
                                        WPSARCO_INIAPP, WPSACRO_INIKEY_LASTBACKUP,
                                        &G_dtLastArchived,
                                        sizeof(G_dtLastArchived));

                    // create simple text file in desktop main directory
                    // to mark it as a backup...
                    if (GetMarkerFilename(szMarkerFilename))
                    {
                        FILE *MarkerFile;
                        MarkerFile = fopen(szMarkerFilename, "w");
                        if (MarkerFile)
                        {
                            fprintf(MarkerFile, "XWorkplace archive marker file.");
                            fclose(MarkerFile);
                        }
                    }
                }
                #ifdef DEBUG_STARTUP
                    else
                        _Pmpf(("WPS Archiving deactivated"));
                #endif
            }
        }
        // close the file
        rc = close(file);
    }
    else
        rc = -1;

    // switch write protection on
    chmod(G_szArcBaseFilename, S_IREAD);
    return rc;
}

/*
 *@@ arcSetNumArchives:
 *      queries or sets the maximum no. of archives
 *      which are maintained by the WPS.
 *
 *      If (fSet == TRUE), the number of archives is set
 *      to *pcArchives. This must be > 0 and < 10, otherwise
 *      FALSE is returned.
 *
 *      If (fSet == FALSE), the current number of archives
 *      is queried and written into *pcArchives, but not
 *      changed.
 */

BOOL arcSetNumArchives(PCHAR pcArchives,        // in/out: number of archives
                       BOOL fSet)               // if TRUE, archive number is set
{
    int             file;
    BOOL            brc = FALSE;

    if (pcArchives)
        if (    (fSet)
             && ((*pcArchives < 0) || (*pcArchives > 9))
           )
            brc = FALSE;     // (*UM)
        else
        {
            // because the archive information file is write protected we must switch
            // write protection of to write informations to it
            chmod(G_szArcBaseFilename, S_IREAD | S_IWRITE);
            file = open(G_szArcBaseFilename, O_BINARY | O_RDWR, 0);
            if (file != -1)
            {
                // seek to the position of number of archives
                if (lseek(file, ARC_FILE_OFFSET + 8, SEEK_SET) != -1)
                {
                    if (fSet)
                        write(file, (PVOID)pcArchives, 1);
                    else
                        read(file, pcArchives, 1);

                    brc = TRUE;
                }
                close(file);
            }
            chmod(G_szArcBaseFilename, S_IREAD);
        }

    return (brc);
}

/*
 *@@ arcCheckINIFiles:
 *      this function goes thru both OS2.INI and OS2SYS.INI and
 *      checks for changes. This gets called from arcCheckIfBackupNeeded.
 *
 *      To find this out, we check the following data:
 *      --  the size of the applications list;
 *      --  the size of the keys list;
 *      --  the actual profile data, for which all values are summed up.
 *
 *      If any of these values differs more than cPercent from the old
 *      data passed to this function, we update that data and return TRUE,
 *      which means that WPS archiving should be turned on.
 *
 *      Otherwise FALSE is returned, and the data is not changed.
 *
 *      In any case, *pcMaxDifferencePercent is set to the maximum
 *      percentage of difference which was computed.
 *
 *      If (cPercent == 0), all data is checked for, and TRUE is always
 *      returned. This might be useful for retrieving the "last app" etc.
 *      data once.
 *
 *      NOTE: It is the responsibility of the caller to save the
 *      old data somewhere. This function does _not_ check the
 *      INI files.
 *
 *      Set pszIgnoreApp to any INI application which should be ignored
 *      when checking for changes. This is useful if the "last app" etc.
 *      data is stored in the profiles too. If (pszIgnoreApp == NULL),
 *      all applications are checked for.
 */

BOOL arcCheckINIFiles(double* pdPercent,
                      PSZ pszIgnoreApp,        // in: this application will not be checked
                      double* pdAppsSizeLast,
                      double* pdKeysSizeLast,
                      double* pdDataSumLast,
                      double* pdMaxDifferencePercent) // out: maximum difference found
{
    BOOL            brc = FALSE;        // return value
    double          dDataSum = 0,
                    dTotalAppsSize = 0,
                    dTotalKeysSize = 0;
    double          dMaxDifferencePercent = 0;

    // 1) Applications loop
    PSZ pszAppsList = prfhQueryKeysForApp(HINI_PROFILE, // both OS2.INI and OS2SYS.INI
                                          NULL);        // return applications

    #ifdef DEBUG_STARTUP
        _Pmpf(("Checking INI files"));
    #endif

    if (pszAppsList)
    {
        PSZ pApp2 = pszAppsList;

        while (*pApp2 != 0)
        {
            BOOL    fIgnore = FALSE;
            // pApp2 has the current app now

            // ignore this app?
            if (pszIgnoreApp)
                if (strcmp(pszIgnoreApp, pApp2) == 0)
                    fIgnore = TRUE;

            if (!fIgnore)
            {
                // 2) keys loop for this app
                PSZ pszKeysList = prfhQueryKeysForApp(HINI_PROFILE, // both OS2.INI and OS2SYS.INI
                                                      pApp2);       // return keys
                if (pszKeysList)
                {
                    PSZ pKey2 = pszKeysList;

                    while (*pKey2 != 0)
                    {
                        // pKey2 has the current key now

                        // 3) get key data
                        ULONG   cbData = 0;
                        PSZ pszData = prfhQueryProfileData(HINI_PROFILE,
                                                           pApp2,
                                                           pKey2,
                                                           &cbData);
                        if (pszData)
                        {
                            // sum up all values
                            PSZ     p = pszData;
                            ULONG   ul = 0;
                            for (ul = 0;
                                 ul < cbData;
                                 ul++, p++)
                                dDataSum += (double)*p;

                            free(pszData);
                        }

                        pKey2 += strlen(pKey2)+1; // next key
                    } // end while (*pKey2 != 0)

                    // add size of keys list to total size
                    dTotalKeysSize += (pKey2 - pszKeysList);

                    free(pszKeysList);
                } // end if (pszKeysList)

                /* sprintf(szTemp, "data sum: %f", dDataSum);
                _Pmpf(("%s", szTemp)); */
            } // end if (!fIgnore)

            pApp2 += strlen(pApp2)+1; // next app
        } // end while (*pApp2 != 0)

        // add size of apps list to total size
        dTotalAppsSize += (pApp2 - pszAppsList);
        free(pszAppsList);
    } // end if (pszAppsList)

    if (*pdPercent != 0)
    {
        #ifdef DEBUG_STARTUP
            CHAR szTemp[1000];

            _Pmpf(("Last, now"));
            sprintf(szTemp, "Apps size: %f, %f", *pdAppsSizeLast, dTotalAppsSize);
            _Pmpf(("%s", szTemp));
            sprintf(szTemp, "Keys size: %f, %f", *pdKeysSizeLast, dTotalKeysSize);
            _Pmpf(("%s", szTemp));
            sprintf(szTemp, "Data sum: %f, %f", *pdDataSumLast, dDataSum);
            _Pmpf(("%s", szTemp));
        #endif

        if ((*pdAppsSizeLast) && (dTotalAppsSize))
        {
            // if so, check if more than the given percentage of application strings
            // where modified
            double dPercentThis =
                        fabs(dTotalAppsSize - *pdAppsSizeLast)     // difference (we need fabs, abs returns 0 always)
                        * 100                               // in percent
                        / *pdAppsSizeLast;

            #ifdef DEBUG_STARTUP
                sprintf(szTemp, "%f", dPercentThis);
                _Pmpf(("dPercent Apps: %s", szTemp));
            #endif

            if (dPercentThis > *pdPercent)
            {
                // yes: store new value (for later writing to log-file)
                // and switch archiving on
                *pdAppsSizeLast = dTotalAppsSize;
                brc = TRUE;
            }

            if (dPercentThis > dMaxDifferencePercent)
                dMaxDifferencePercent = dPercentThis;
        }

        // same logic for keys size
        if ((*pdKeysSizeLast) && (dTotalKeysSize))
        {
            double dPercentThis =
                        fabs(dTotalKeysSize - *pdKeysSizeLast)     // difference (we need fabs, abs returns 0 always)
                        * 100                               // in percent
                        / *pdKeysSizeLast;

            #ifdef DEBUG_STARTUP
                sprintf(szTemp, "%f", dPercentThis);
                _Pmpf(("dPercent Keys: %s", szTemp));
            #endif

            if (dPercentThis > *pdPercent)
            {
                // yes: store new value (for later writing to log-file)
                // and switch archiving on
                *pdKeysSizeLast = dTotalKeysSize;
                brc = TRUE;
            }

            if (dPercentThis > dMaxDifferencePercent)
                dMaxDifferencePercent = dPercentThis;
        }

        // same logic for profile data
        if ((*pdDataSumLast) && (dDataSum))
        {
            double dPercentThis =
                        fabs(dDataSum - *pdDataSumLast)     // difference (we need fabs, abs returns 0 always)
                        * 100                               // in percent
                        / *pdDataSumLast;

            #ifdef DEBUG_STARTUP
                sprintf(szTemp, "%f", dPercentThis);
                _Pmpf(("dPercent Data: %s", szTemp));
            #endif

            if (dPercentThis > *pdPercent)
            {
                // yes: store new value (for later writing to log-file)
                // and switch archiving on
                *pdDataSumLast = dDataSum;
                brc = TRUE;
            }

            if (dPercentThis > dMaxDifferencePercent)
                dMaxDifferencePercent = dPercentThis;
        }

        if (pdMaxDifferencePercent)
            *pdMaxDifferencePercent = dMaxDifferencePercent;

    } // end if (cPercent)

    // check if any size is 0 so the values MUST be written and archiving MUST
    // be switched on
    if (!*pdAppsSizeLast || !*pdKeysSizeLast || !*pdDataSumLast)
    {
        *pdAppsSizeLast = dTotalAppsSize;
        *pdKeysSizeLast = dTotalKeysSize;
        *pdDataSumLast = dDataSum;
        brc = TRUE;
    }

    #ifdef DEBUG_STARTUP
        _Pmpf(("  Done checking INI files, returning %d", brc));
    #endif

    return (brc);
}


