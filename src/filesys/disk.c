
/*
 *@@sourcefile disk.c:
 *      implementation code for XFldDisk class.
 *
 *      This file is ALL new with V0.9.0.
 *
 *      Function prefix for this file:
 *      --  dsk*
 *
 *@@added V0.9.0 [umoeller]
 *@@header "filesys\disk.h"
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

#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_WIN
#include  <os2.h>

// C library headers
#include <stdio.h>

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\comctl.h"             // common controls (window procs)
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\stringh.h"            // string helper routines
#include "helpers\winh.h"               // PM helper routines

// SOM headers which don't crash with prec. header files
#include "xfdisk.ih"
#pragma hdrstop
#include "xfldr.h"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

#include "filesys\folder.h"             // XFolder implementation
#include "filesys\menus.h"              // shared context menu logic
#include "filesys\statbars.h"           // status bar translation logic

// other SOM headers
#include "helpers\undoc.h"              // some undocumented stuff

/* ******************************************************************
 *                                                                  *
 *   Disk implementation                                            *
 *                                                                  *
 ********************************************************************/

/*
 *@@ dskCheckDriveReady:
 *      this attempts to return the root folder for
 *      the given disk object. Since wpQueryRootFolder
 *      normally results in those not-so-beautiful white
 *      hard-error popups, this can prompt the user with
 *      an "abort-retry" box.
 *
 *      Returns NULL if the drive is not ready and the
 *      user has chosen "Abort" afterwards.
 *
 *      This code used to be in XFldDisk::wpViewObject.
 *
 *@@added V0.9.0 (99-11-27) [umoeller]
 */

WPFolder* dskCheckDriveReady(WPDisk *somSelf)
{
    WPFolder    *pRootFolder = 0;
    ULONG        mbrc = MBID_OK;

    // query root folder (WPRootFolder class, which is a descendant
    // of WPFolder/XFolder); each WPDisk is paired with one of those,
    // and the actual display is done by this class, so we will pass
    // pRootFolder instead of somSelf to most following method calls.
    // We use wpshQueryRootFolder instead of wpQueryRootFolder to
    // avoid the "Drive not ready" popups which the WPS/OS/2 normally
    // displays in this method.
    do
    {
        APIRET      arc = NO_ERROR;
        HPOINTER    hptrOld = winhSetWaitPointer();

        mbrc = MBID_OK;

        pRootFolder = wpshQueryRootFolder(somSelf,
                                          &arc);    // out: DOS error code

        if (pRootFolder == NULL)
        {
            // drive not ready:
            CHAR    szTitle[400];

            // reset mouse pointer
            WinSetPointer(HWND_DESKTOP, hptrOld);

            cmnGetMessage(NULL, 0,
                          szTitle, sizeof(szTitle),
                          104);  // "XFolder: Error"
            sprintf(szTitle + strlen(szTitle),
                    " (\"%s\")", _wpQueryTitle(somSelf));
                            // append drive name in brackets

            mbrc = cmnDosErrorMsgBox(HWND_DESKTOP,
                                     _wpQueryLogicalDrive(somSelf) + 'A' - 1,
                                     szTitle,
                                     arc,
                                     MB_RETRYCANCEL,
                                     TRUE); // show explanation
        }

        WinSetPointer(HWND_DESKTOP, hptrOld);
    } while (mbrc == MBID_RETRY);

    return (pRootFolder);
}

/* ******************************************************************
 *                                                                  *
 *   XFldDisk notebook callbacks (notebook.c)                       *
 *                                                                  *
 ********************************************************************/

/*
 *@@ dskDetailsInitPage:
 *      "Details" page notebook callback function (notebook.c).
 *      Sets the controls on the page according to the disk's
 *      characteristics.
 *
 *@@added V0.9.0 [umoeller]
 */

VOID dskDetailsInitPage(PCREATENOTEBOOKPAGE pcnbp,    // notebook info struct
                        ULONG flFlags)                // CBI_* flags (notebook.h)
{
    // PGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    if (flFlags & CBI_INIT)
    {
        // prepare chart control
        CHARTSTYLE  cs;
        HWND        hwndChart = WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_DISK_CHART);
        ctlChartFromStatic(hwndChart);
        cs.ulStyle = CHS_3D_DARKEN | CHS_DESCRIPTIONS_3D;
        cs.ulThickness = 20;
        cs.dPieSize = 1;            // all of window
        cs.dDescriptions = .66;     // 2/3 of window
        WinSendMsg(hwndChart, CHTM_SETCHARTSTYLE, &cs, NULL);

        // set entry-field limit (drive labels can only have 11 chars)
        winhSetEntryFieldLimit(WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_DISK_LABEL),
                               11);
    }

    if (flFlags & CBI_SET)
    {
        ULONG ulLogicalDrive = _wpQueryLogicalDrive(pcnbp->somSelf);

        if (ulLogicalDrive)
        {
            FSALLOCATE      fsa;
            CHAR            szTemp[100];
            CHAR            szVolumeLabel[20];

            // get thousands separator from "Country" object
            CHAR            cThousands = cmnQueryThousandsSeparator();

            if (doshQueryDiskLabel(ulLogicalDrive, &szVolumeLabel[0])
                    == NO_ERROR)
                // label entry field
                WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XSDI_DISK_LABEL,
                                  szVolumeLabel);

            // file-system type (HPFS, ...)
            if (doshQueryDiskFSType(ulLogicalDrive,
                                    szTemp,
                                    sizeof(szTemp))
                        == NO_ERROR)
            {
                WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XSDI_DISK_FILESYSTEM,
                                  szTemp);
            }

            if (DosQueryFSInfo(ulLogicalDrive, FSIL_ALLOC, &fsa, sizeof(fsa))
                    == NO_ERROR)
            {
                double      dTotal = 0,
                            dAllocated = 0,
                            dAvailable = 0;
                double      dDisplay[2];
                CHAR        szAvailable[50] = "",
                            szAllocated[50] = "";
                PSZ         apszDescriptions[2];
                CHARTDATA   pcd;
                LONG        alColors[2] = {
                                                0x00FF0000,     // color for allocated (RGB)
                                                0x0000FF00      // color for free
                                          };

                // bytes per sector
                WinSetDlgItemText(pcnbp->hwndDlgPage,
                                  ID_XSDI_DISK_SECTORSIZE,
                                  strhThousandsULong(szTemp,
                                                     fsa.cbSector,
                                                     cThousands));

                // total size
                WinSetDlgItemText(pcnbp->hwndDlgPage,
                                  ID_XSDI_DISK_TOTAL_SECTORS,
                                  strhThousandsULong(szTemp,
                                                     fsa.cSectorUnit * fsa.cUnit,
                                                     cThousands));
                dTotal = (double)fsa.cbSector
                              * fsa.cSectorUnit
                              * fsa.cUnit;
                WinSetDlgItemText(pcnbp->hwndDlgPage,
                                  ID_XSDI_DISK_TOTAL_BYTES,
                                  strhThousandsDouble(szTemp,
                                                      dTotal,
                                                      cThousands));

                // free space
                WinSetDlgItemText(pcnbp->hwndDlgPage,
                                  ID_XSDI_DISK_AVAILABLE_SECTORS,
                                  strhThousandsULong(szTemp,
                                                     fsa.cSectorUnit * fsa.cUnitAvail,
                                                     cThousands));
                dAvailable = (double)fsa.cbSector
                                  * fsa.cSectorUnit
                                  * fsa.cUnitAvail,
                WinSetDlgItemText(pcnbp->hwndDlgPage,
                                  ID_XSDI_DISK_AVAILABLE_BYTES,
                                  strhThousandsDouble(szTemp,
                                                      dAvailable,
                                                      cThousands));

                // allocated space
                WinSetDlgItemText(pcnbp->hwndDlgPage,
                                  ID_XSDI_DISK_ALLOCATED_SECTORS,
                                  strhThousandsULong(szTemp,
                                                     fsa.cSectorUnit
                                                        * (fsa.cUnit - fsa.cUnitAvail),
                                                     cThousands));
                dAllocated = dTotal - dAvailable;  // allocated
                WinSetDlgItemText(pcnbp->hwndDlgPage,
                                  ID_XSDI_DISK_ALLOCATED_BYTES,
                                  strhThousandsDouble(szTemp,
                                                      dAllocated,
                                                      cThousands));

                // reset pie chart control with those values
                pcd.usStartAngle = 270 + 15;
                pcd.usSweepAngle = 360 - 30;       // draw 7/8 pie
                pcd.cValues = 2;            // value count
                dDisplay[0] = dAllocated;
                dDisplay[1] = dAvailable;
                pcd.padValues = &dDisplay[0];

                pcd.palColors = (LONG*)(&alColors);

                // compose description strings array
                strhThousandsDouble(szAllocated,
                                    dDisplay[0] / 1024 / 1024,
                                    cThousands);
                strcat(szAllocated, " MB");
                strhThousandsDouble(szAvailable,
                                    dDisplay[1] / 1024 / 1024,
                                    cThousands);
                strcat(szAvailable, " MB");
                apszDescriptions[0] = szAllocated;
                apszDescriptions[1] = szAvailable;
                pcd.papszDescriptions = &apszDescriptions[0];

                WinSendMsg(WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_DISK_CHART),
                           CHTM_SETCHARTDATA,
                           &pcd,
                           NULL);
            }
        }
    }
}

/*
 *@@ dskDetailsItemChanged:
 *      "Details" page notebook callback function (notebook.c).
 *      Reacts to changes of any of the dialog controls.
 *
 *@@added V0.9.0 [umoeller]
 */

MRESULT dskDetailsItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                              USHORT usItemID,
                              USHORT usNotifyCode,
                              ULONG ulExtra)      // for checkboxes: contains new state
{
    MRESULT mrc = 0;

    switch (usItemID)
    {
        /*
         * ID_XSDI_DISK_LABEL:
         *      disk label entry field.
         */

        case ID_XSDI_DISK_LABEL:
            if (usNotifyCode == EN_KILLFOCUS)
            {
                HWND hwndEF = WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_DISK_LABEL);
                if (winhHasEntryFieldChanged(hwndEF))
                {
                    PSZ pszNewLabel = winhQueryWindowText(hwndEF);
                    if (pszNewLabel)
                    {
                        ULONG           ulLogicalDrive = _wpQueryLogicalDrive(pcnbp->somSelf);
                        doshSetDiskLabel(ulLogicalDrive, pszNewLabel);
                            // use helper function for bugfix
                        free(pszNewLabel);
                    }
                }
            }
        break;

    }

    return (mrc);
}


