
/*
 *@@sourcefile drivdlgs.h:
 *      header file for drivdlgs.c. See remarks there.
 *
 *@@added V0.9.0
 *@@include #include <os2.h>
 *@@include #include "setup\drivdlgs.h"
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

#define DRVF_CMDREF     0x0001      // driver is documented in CMDREF
#define DRVF_NOPARAMS   0x0002      // driver accepts no parameters

#define DRVF_BASEDEV    0x00010000  // BASEDEV=
#define DRVF_DEVICE     0x00020000  // DEVICE=
#define DRVF_IFS        0x00040000  // IFS=
#define DRVF_OTHER      0x00080000  // ?!?

/*
 *@@ DRIVERSPEC:
 *      structure for declaring drivers known to
 *      the "OS/2 Kernel" object. One of these
 *      is created for each driver which can
 *      be displayed on the "Drivers" page,
 *      according to the NLS DRVRSxxx.TXT file
 *      in the XWorkplace /BIN/ subdirectory.
 *      This is done by cfgDriversInitPage (xfsys.c).
 *
 *@@added V0.9.0
 */

typedef struct _DRIVERSPEC
{
    PSZ        pszKeyword;          // e.g. "BASEDEV="
    PSZ        pszFilename;         // e.g. "IBM1S506.ADD"
    PSZ        pszDescription;      // e.g. "IBM IDE driver"
    ULONG      ulFlags;             // DRVF_* flags
    HMODULE    hmodConfigDlg;       // module handle with dlg template or null if no dlg exists
    ULONG      idConfigDlg;         // resource ID of config dlg or null if no dlg exists
    PFNWP      pfnwpConfigure;      // dialog proc for "Configure" dialog or null if none
} DRIVERSPEC, *PDRIVERSPEC;

/*
 *@@ DRIVERDLGDATA:
 *      structure for communication between a
 *      driver dialog func and the main "Drivers"
 *      notebook page in "OS/2 Kernel".
 *      See the top of drivdlgs.c for details.
 *
 *@@added V0.9.0
 */

typedef struct _DRIVERDLGDATA
{
    PDRIVERSPEC pDriverSpec;                    // in: driver specs; do not modify
    CHAR        szParams[500];                  // in/out: as in CONFIG.SYS
    PVOID       pvUser;                         // for data needed in dialog
} DRIVERDLGDATA, *PDRIVERDLGDATA;

VOID drvConfigSupported(PDRIVERSPEC pSpec);

// driver configuration dialogs
MRESULT EXPENTRY drv_fnwpConfigHPFS(HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2);
MRESULT EXPENTRY drv_fnwpConfigCDFS(HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2);
MRESULT EXPENTRY drv_fnwpConfigIBM1S506(HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2);


