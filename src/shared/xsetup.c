
/*
 *@@sourcefile xsetup.c:
 *      this file contains the implementation of the XWPSetup
 *      class. This is in the shared directory because this
 *      affects all installed classes.
 *
 *@@header "shared\xsetup.h"
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
 *@@todo:
 *
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

#define INCL_DOSEXCEPTIONS
#define INCL_DOSPROCESS
#define INCL_DOSMODULEMGR
#define INCL_DOSSEMAPHORES
#define INCL_DOSERRORS

#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WINMESSAGEMGR
#define INCL_WINPOINTERS
#define INCL_WINMENUS
#define INCL_WINDIALOGS
#define INCL_WINBUTTONS
#define INCL_WINLISTBOXES
#define INCL_WINSTDCNR
#define INCL_WINSTDSLIDER
#define INCL_WINSYS

#define INCL_GPILOGCOLORTABLE
#define INCL_GPIBITMAPS
#include <os2.h>

// C library headers
#include <stdio.h>
#include <locale.h>
#include <setjmp.h>

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\cnrh.h"               // container helper routines
#include "helpers\comctl.h"             // common controls (window procs)
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\winh.h"               // PM helper routines
#include "helpers\procstat.h"           // DosQProcStat handling
#include "helpers\stringh.h"            // string helper routines
#include "helpers\syssound.h"           // system sound helper routines
#include "helpers\threads.h"            // thread helpers
#include "helpers\tmsgfile.h"           // "text message file" handling
#include "helpers\xstring.h"            // extended string helpers

// SOM headers which don't crash with prec. header files
#include "xwpsetup.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "xwpapi.h"                     // public XWorkplace definitions

#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

#include "config\hookintf.h"            // daemon/hook interface
#include "config\sound.h"               // XWPSound implementation

#include "filesys\fileops.h"            // file operations implementation
#include "filesys\xthreads.h"           // extra XWorkplace threads

#include "media\media.h"                // XWorkplace multimedia support

#include "startshut\apm.h"              // APM power-off for XShutdown

#include "hook\xwphook.h"

// other SOM headers
#pragma hdrstop
#include <wpfsys.h>             // WPFileSystem
#include "xtrash.h"

/* ******************************************************************
 *                                                                  *
 *   Globals                                                        *
 *                                                                  *
 ********************************************************************/

/*
 *@@ FEATURESITEM:
 *      structure used for feature checkboxes
 *      on the "Features" page. Each item
 *      represents one record in the container.
 */

typedef struct _FEATURESITEM
{
    USHORT  usFeatureID;
                // string ID (dlgids.h, *.rc file in NLS DLL) for feature;
                // this also identifies the item for processing
    USHORT  usParentID;
                // string ID of the parent record or null if root record.
                // If you specify a parent record, this must appear before
                // the child record in FeaturesItemsList.
    ULONG   ulStyle;
                // style flags for the record; OR the following:
                // -- WS_VISIBLE: record is visible
                // -- BS_AUTOCHECKBOX: give the record a checkbox
                // For parent records (without checkboxes), use 0 only.
    PSZ     pszNLSString;
                // resolved NLS string; this must be NULL initially.
} FEATURESITEM, *PFEATURESITEM;

/*
 * FeatureItemsList:
 *      array of FEATURESITEM's which are inserted into
 *      the container on the "Features" page.
 *
 *added V0.9.1 (99-12-19) [umoeller]
 */

static FEATURESITEM G_FeatureItemsList[] =
        {
            // general features
            ID_XCSI_GENERALFEATURES, 0, 0, NULL,
            ID_XCSI_REPLACEICONS, ID_XCSI_GENERALFEATURES, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,
            ID_XCSI_RESIZESETTINGSPAGES, ID_XCSI_GENERALFEATURES, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,
            ID_XCSI_ADDOBJECTPAGE, ID_XCSI_GENERALFEATURES, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,
            ID_XCSI_REPLACEFILEPAGE, ID_XCSI_GENERALFEATURES, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,
            ID_XCSI_XSYSTEMSOUNDS, ID_XCSI_GENERALFEATURES, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,

            // folder features
            ID_XCSI_FOLDERFEATURES, 0, 0, NULL,
            ID_XCSI_ENABLESTATUSBARS, ID_XCSI_FOLDERFEATURES, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,
            ID_XCSI_ENABLESNAP2GRID, ID_XCSI_FOLDERFEATURES, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,
            ID_XCSI_ENABLEFOLDERHOTKEYS, ID_XCSI_FOLDERFEATURES, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,
            ID_XCSI_EXTFOLDERSORT, ID_XCSI_FOLDERFEATURES, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,

            // mouse/keyboard features
            ID_XCSI_MOUSEKEYBOARDFEATURES, 0, 0, NULL,
#ifdef __ANIMATED_MOUSE_POINTERS__
            ID_XCSI_ANIMOUSE, ID_XCSI_MOUSEKEYBOARDFEATURES, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,
#endif
            ID_XCSI_XWPHOOK, ID_XCSI_MOUSEKEYBOARDFEATURES, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,
            ID_XCSI_GLOBALHOTKEYS, ID_XCSI_MOUSEKEYBOARDFEATURES, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,
// #ifdef __PAGEMAGE__
            ID_XCSI_PAGEMAGE, ID_XCSI_MOUSEKEYBOARDFEATURES, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,
// #endif

            // startup/shutdown features
            ID_XCSI_STARTSHUTFEATURES, 0, 0, NULL,
            ID_XCSI_ARCHIVING, ID_XCSI_STARTSHUTFEATURES, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,
            ID_XCSI_RESTARTWPS, ID_XCSI_STARTSHUTFEATURES, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,
            ID_XCSI_XSHUTDOWN, ID_XCSI_STARTSHUTFEATURES, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,

            // file operations
            ID_XCSI_FILEOPERATIONS, 0, 0, NULL,
            ID_XCSI_EXTASSOCS, ID_XCSI_FILEOPERATIONS, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,
            ID_XCSI_CLEANUPINIS, ID_XCSI_FILEOPERATIONS, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,
            ID_XCSI_REPLFILEEXISTS, ID_XCSI_FILEOPERATIONS, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,
            ID_XCSI_REPLDRIVENOTREADY, ID_XCSI_FILEOPERATIONS, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,
            ID_XCSI_XWPTRASHCAN, ID_XCSI_FILEOPERATIONS, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,
            ID_XCSI_REPLACEDELETE, ID_XCSI_FILEOPERATIONS, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,

#ifdef __REPLHANDLES__
            ID_XCSI_REPLHANDLES, ID_XCSI_FILEOPERATIONS, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,
#endif
            ID_XCSI_REPLACEREFRESH, ID_XCSI_FILEOPERATIONS, WS_VISIBLE | BS_AUTOCHECKBOX, NULL
        };

static PCHECKBOXRECORDCORE G_pFeatureRecordsList = NULL;

/*
 *@@ STANDARDOBJECT:
 *      structure used for XWPSetup "Objects" page.
 *      Each of these represents an object to be
 *      created from the menu buttons.
 *
 *@@changed V0.9.4 (2000-07-15) [umoeller]: added pExists
 */

typedef struct _STANDARDOBJECT
{
    const char  **ppcszDefaultID;   // e.g. <WP_DRIVES>;
    const char  *pcszObjectClass;   // e.g. "WPDrives"
    const char  *pcszSetupString;   // e.g. "DETAILSFONT=..."; if present, _always_
                                    // put a semicolon at the end, because "OBJECTID=xxx"
                                    // will always be added
    USHORT      usMenuID;           // corresponding menu ID in xfldr001.rc resources

    WPObject    *pExists;           // != NULL if object exists already
} STANDARDOBJECT, *PSTANDARDOBJECT;

#define OBJECTSIDFIRST 100      // first object menu ID, inclusive
#define OBJECTSIDLAST  214      // last object menu ID, inclusive

// array of objects for "Standard WPS objects" menu button
static STANDARDOBJECT
    G_WPSObjects[] = {
            &WPOBJID_KEYB, "WPKeyboard", "", 100, 0,
            &WPOBJID_MOUSE, "WPMouse", "", 101, 0,
            &WPOBJID_CNTRY, "WPCountry", "", 102, 0,
            &WPOBJID_SOUND, "WPSound", "", 103, 0,
            &WPOBJID_POWER, "WPPower", "", 104, 0,
            &WPOBJID_WINCFG, "WPWinConfig", "", 105, 0,

            &WPOBJID_HIRESCLRPAL, "WPColorPalette", "", 110, 0,
            &WPOBJID_LORESCLRPAL, "WPColorPalette", "", 111, 0,
            &WPOBJID_FNTPAL, "WPFontPalette", "", 112, 0,
            &WPOBJID_SCHPAL96, "WPSchemePalette", "", 113, 0,

            &WPOBJID_LAUNCHPAD, "WPLaunchPad", "", 120, 0,
            &WPOBJID_WARPCENTER, "SmartCenter", "", 121, 0,

            &WPOBJID_SPOOL, "WPSpool", "", 130, 0,
            &WPOBJID_VIEWER, "WPMinWinViewer", "", 131, 0,
            &WPOBJID_SHRED, "WPShredder", "", 132, 0,
            &WPOBJID_CLOCK, "WPClock", "", 133, 0,

            &WPOBJID_START, "WPStartup", "", 140, 0,
            &WPOBJID_TEMPS, "WPTemplates", "", 141, 0,
            &WPOBJID_DRIVES, "WPDrives", "", 142, 0
    },

// array of objects for "XWorkplace objects" menu button
    G_XWPObjects[] = {
            &XFOLDER_WPSID, "XFldWPS", "", 200, 0,
            &XFOLDER_KERNELID, "XFldSystem", "", 201, 0,
            &XFOLDER_SCREENID, "XWPScreen", "", 203, 0,
            &XFOLDER_MEDIAID, "XWPMedia", "", 204, 0,
            &XFOLDER_CLASSLISTID, "XWPClassList", "", 202, 0,

            &XFOLDER_CONFIGID, "WPFolder",
                    "ICONVIEW=NONFLOWED,MINI;ALWAYSSORT=NO;",
                    210, 0,
            &XFOLDER_STARTUPID, "XFldStartup",
                    "ICONVIEW=NONFLOWED,MINI;ALWAYSSORT=NO;",
                    211, 0,
            &XFOLDER_SHUTDOWNID, "XFldShutdown",
                    "ICONVIEW=NONFLOWED,MINI;ALWAYSSORT=NO;",
                    212, 0,

            &XFOLDER_TRASHCANID, "XWPTrashCan",
                    "DETAILSCLASS=XWPTrashObject;"
                    "SORTCLASS=XWPTrashObject;",
                    213, 0,
            &XFOLDER_XCENTERID, "XCenter",
                    "",
                    214, 0
    };

/* ******************************************************************
 *                                                                  *
 *   XWPSetup helper functions                                      *
 *                                                                  *
 ********************************************************************/

/*
 *@@ AddResourceDLLToLB:
 *      this loads a given DLL temporarily in order to
 *      find out if it's an XFolder NLS DLL; if so,
 *      its language string is loaded and a descriptive
 *      string is inserted into a given list box
 *      (used for "XFolder Internals" settings page).
 *
 *@@changed V0.8.5 [umoeller]: language string now initialized to ""
 */

VOID AddResourceDLLToLB(HWND hwndDlg,                   // in: dlg with listbox
                        ULONG idLB,                     // in: listbox item ID
                        PSZ pszXFolderBasePath,         // in: from cmnQueryXWPBasePath
                        PSZ pszFileName)
{
    CHAR    szLBEntry[2*CCHMAXPATH] = "",   // changed V0.85
            szResourceModuleName[2*CCHMAXPATH];
    HMODULE hmodDLL = NULLHANDLE;
    APIRET  arc;

    #ifdef DEBUG_LANGCODES
        _Pmpf(("  Entering AddResourceDLLtoLB: %s", pszFileName));
    #endif
    strcpy(szResourceModuleName, pszXFolderBasePath);
    strcat(szResourceModuleName, "\\bin\\");
    strcat(szResourceModuleName, pszFileName);

    arc = DosLoadModule(NULL, 0,
                        szResourceModuleName,
                        &hmodDLL);
    #ifdef DEBUG_LANGCODES
        _Pmpf(("    Loading module '%s', arc: %d", szResourceModuleName, arc));
    #endif

    if (arc == NO_ERROR)
    {
        #ifdef DEBUG_LANGCODES
            _Pmpf(("    Testing for language string"));
        #endif
        if (WinLoadString(WinQueryAnchorBlock(hwndDlg),
                          hmodDLL,
                          ID_XSSI_DLLLANGUAGE,
                          sizeof(szLBEntry), szLBEntry))
        {
            #ifdef DEBUG_LANGCODES
                _Pmpf(("      --> found %s", szLBEntry));
            #endif
            strcat(szLBEntry, " -- ");
            strcat(szLBEntry, pszFileName);

            WinSendDlgItemMsg(hwndDlg, idLB,
                              LM_INSERTITEM,
                              (MPARAM)LIT_SORTASCENDING,
                              (MPARAM)szLBEntry);
        }
        #ifdef DEBUG_LANGCODES
            else
                _Pmpf(("      --> language string not found"));
        #endif

        DosFreeModule(hmodDLL);
    }
    #ifdef DEBUG_LANGCODES
        else
            _Pmpf(("    Error %d", arc));
    #endif
}

/* ******************************************************************
 *                                                                  *
 *   XWPSetup "Installed classes" dialog                            *
 *                                                                  *
 ********************************************************************/

/*
 *@@ XWPCLASSES:
 *      structure used for fnwpXWorkplaceClasses
 *      (the "XWorkplace Classes" setup dialog).
 */

typedef struct _XWPCLASSES
{
    // class replacements
    BOOL    fXFldObject,
            fXFolder,
            fXFldDisk,
            fXFldDesktop,
            fXFldDataFile,
            fXFldProgramFile,
            fXWPSound,
            fXWPMouse,
            fXWPKeyboard,
    // new classes
            fXWPSetup,
            fXFldSystem,
            fXFldWPS,
            fXWPScreen,
            fXWPMedia,
            fXFldStartup,
            fXFldShutdown,
            fXWPClassList,
            fXWPTrashCan,
            fXWPString,
            fXCenter;

    HWND    hwndTooltip;
    PSZ     pszTooltipString;
} XWPCLASSES, *PXWPCLASSES;

// array of tools to be subclassed for tooltips
static USHORT G_usClassesToolIDs[] =
    {
        ID_XCDI_XWPCLS_XFLDOBJECT,
        ID_XCDI_XWPCLS_XFOLDER,
        ID_XCDI_XWPCLS_XFLDDISK,
        ID_XCDI_XWPCLS_XFLDDESKTOP,
        ID_XCDI_XWPCLS_XFLDDATAFILE,
        ID_XCDI_XWPCLS_XFLDPROGRAMFILE,
        ID_XCDI_XWPCLS_XWPSOUND,
        ID_XCDI_XWPCLS_XWPMOUSE,
        ID_XCDI_XWPCLS_XWPKEYBOARD,

        ID_XCDI_XWPCLS_XWPSETUP,
        ID_XCDI_XWPCLS_XFLDSYSTEM,
        ID_XCDI_XWPCLS_XFLDWPS,
        ID_XCDI_XWPCLS_XFLDSTARTUP,
        ID_XCDI_XWPCLS_XFLDSHUTDOWN,
        ID_XCDI_XWPCLS_XWPCLASSLIST,
        ID_XCDI_XWPCLS_XWPTRASHCAN,

        // new items with V0.9.3 (2000-04-26) [umoeller]
        ID_XCDI_XWPCLS_XWPSCREEN,
        ID_XCDI_XWPCLS_XWPSTRING,

        // new items with V0.9.5
        ID_XCDI_XWPCLS_XWPMEDIA,

        // new items with V0.9.7
        ID_XCDI_XWPCLS_XWPMEDIA,
        ID_XCDI_XWPCLS_XCENTER,

        DID_OK,
        DID_CANCEL
    };

/*
 * fnwpXWorkplaceClasses:
 *      dialog procedure for the "XWorkplace Classes" dialog.
 *
 *@@changed V0.9.3 (2000-04-26) [umoeller]: added generic fonts support
 *@@changed V0.9.3 (2000-04-26) [umoeller]: added new classes
 *@@changed V0.9.5 (2000-08-23) [umoeller]: XWPMedia wasn't working, fixed
 */

MRESULT EXPENTRY fnwpXWorkplaceClasses(HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    switch (msg)
    {
        /*
         * WM_INITDLG:
         *
         */

        case WM_INITDLG:
        {
            PBYTE           pObjClass;

            // allocate two XWPCLASSES structures and store them in
            // QWL_USER; initially, both structures will be the same,
            // but only the second one will be altered
            PXWPCLASSES     pxwpc = (PXWPCLASSES)malloc(2*sizeof(XWPCLASSES));
            memset(pxwpc, 0, 2*sizeof(XWPCLASSES));
            WinSetWindowPtr(hwndDlg, QWL_USER, pxwpc);

            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);

            // load WPS class list
            pObjClass = winhQueryWPSClassList();

            // query class replacements
            pxwpc->fXFldObject = (winhQueryWPSClass(pObjClass, "XFldObject") != 0);
            pxwpc->fXFolder = (winhQueryWPSClass(pObjClass, "XFolder") != 0);
            pxwpc->fXFldDisk = (winhQueryWPSClass(pObjClass, "XFldDisk") != 0);
            pxwpc->fXFldDesktop = (winhQueryWPSClass(pObjClass, "XFldDesktop") != 0);
            pxwpc->fXFldDataFile = (winhQueryWPSClass(pObjClass, "XFldDataFile") != 0);
            pxwpc->fXFldProgramFile = (winhQueryWPSClass(pObjClass, "XFldProgramFile") != 0);
            pxwpc->fXWPSound = (winhQueryWPSClass(pObjClass, "XWPSound") != 0);
            pxwpc->fXWPMouse = (winhQueryWPSClass(pObjClass, "XWPMouse") != 0);
            pxwpc->fXWPKeyboard = (winhQueryWPSClass(pObjClass, "XWPKeyboard") != 0);

            // query new classes
            pxwpc->fXWPSetup = (winhQueryWPSClass(pObjClass, "XWPSetup") != 0);
            pxwpc->fXFldSystem = (winhQueryWPSClass(pObjClass, "XFldSystem") != 0);
            pxwpc->fXFldWPS = (winhQueryWPSClass(pObjClass, "XFldWPS") != 0);
            pxwpc->fXWPScreen = (winhQueryWPSClass(pObjClass, "XWPScreen") != 0);
            pxwpc->fXWPMedia  = (winhQueryWPSClass(pObjClass, "XWPMedia") != 0);
            pxwpc->fXFldStartup = (winhQueryWPSClass(pObjClass, "XFldStartup") != 0);
            pxwpc->fXFldShutdown = (winhQueryWPSClass(pObjClass, "XFldShutdown") != 0);
            pxwpc->fXWPClassList = (winhQueryWPSClass(pObjClass, "XWPClassList") != 0);
            pxwpc->fXWPTrashCan = (     (winhQueryWPSClass(pObjClass, "XWPTrashCan") != 0)
                                     && (winhQueryWPSClass(pObjClass, "XWPTrashObject") != 0)
                                  );
            pxwpc->fXWPString = (winhQueryWPSClass(pObjClass, "XWPString") != 0);
            pxwpc->fXCenter = (winhQueryWPSClass(pObjClass, "XCenter") != 0);

            // copy first structure to second one
            memcpy(pxwpc + 1, pxwpc, sizeof(XWPCLASSES));

            // set controls accordingly:
            // class replacements
            winhSetDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XFLDOBJECT, pxwpc->fXFldObject);
            winhSetDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XFOLDER, pxwpc->fXFolder);
            winhSetDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XFLDDISK, pxwpc->fXFldDisk);
            winhSetDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XFLDDESKTOP, pxwpc->fXFldDesktop);
            winhSetDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XFLDDATAFILE, pxwpc->fXFldDataFile);
            winhSetDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XFLDPROGRAMFILE, pxwpc->fXFldProgramFile);
            winhSetDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XWPSOUND, pxwpc->fXWPSound);
            winhSetDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XWPMOUSE, pxwpc->fXWPMouse);
            winhSetDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XWPKEYBOARD, pxwpc->fXWPKeyboard);

            // new classes
            winhSetDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XWPSETUP, pxwpc->fXWPSetup);
            winhSetDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XFLDSYSTEM, pxwpc->fXFldSystem);
            winhSetDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XFLDWPS, pxwpc->fXFldWPS);
            winhSetDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XWPSCREEN, pxwpc->fXWPScreen);
            winhSetDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XWPMEDIA, pxwpc->fXWPMedia);
            winhSetDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XFLDSTARTUP, pxwpc->fXFldStartup);
            winhSetDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XFLDSHUTDOWN, pxwpc->fXFldShutdown),
            winhSetDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XWPCLASSLIST, pxwpc->fXWPClassList);
            winhSetDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XWPTRASHCAN, pxwpc->fXWPTrashCan);
            winhSetDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XWPSTRING, pxwpc->fXWPString);
            winhSetDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XCENTER, pxwpc->fXCenter);

            cmnSetControlsFont(hwndDlg, 0, 5000);
                    // added V0.9.3 (2000-04-26) [umoeller]

            free(pObjClass);

            // register tooltip class
            ctlRegisterTooltip(WinQueryAnchorBlock(hwndDlg));
            // create tooltip
            pxwpc->hwndTooltip = WinCreateWindow(HWND_DESKTOP,  // parent
                                                 COMCTL_TOOLTIP_CLASS, // wnd class
                                                 "",            // window text
                                                 XWP_TOOLTIP_STYLE,
                                                      // tooltip window style (common.h)
                                                 10, 10, 10, 10,    // window pos and size, ignored
                                                 hwndDlg,       // owner window -- important!
                                                 HWND_TOP,      // hwndInsertBehind, ignored
                                                 DID_TOOLTIP, // window ID, optional
                                                 NULL,          // control data
                                                 NULL);         // presparams

            if (pxwpc->hwndTooltip)
            {
                // tooltip successfully created:
                // add tools (i.e. controls of the dialog)
                // according to the usToolIDs array
                TOOLINFO    ti = {0};
                HWND        hwndCtl;
                ULONG       ul;
                ti.ulFlags = TTF_CENTER_X_ON_TOOL | TTF_POS_Y_BELOW_TOOL | TTF_SUBCLASS;
                ti.hwndToolOwner = hwndDlg;
                ti.pszText = PSZ_TEXTCALLBACK;  // send TTN_NEEDTEXT
                for (ul = 0;
                     ul < (sizeof(G_usClassesToolIDs) / sizeof(G_usClassesToolIDs[0]));
                     ul++)
                {
                    hwndCtl = WinWindowFromID(hwndDlg, G_usClassesToolIDs[ul]);
                    if (hwndCtl)
                    {
                        // add tool to tooltip control
                        ti.hwndTool = hwndCtl;
                        WinSendMsg(pxwpc->hwndTooltip,
                                   TTM_ADDTOOL,
                                   (MPARAM)0,
                                   &ti);
                    }
                }

                // set timers
                WinSendMsg(pxwpc->hwndTooltip,
                           TTM_SETDELAYTIME,
                           (MPARAM)TTDT_AUTOPOP,
                           (MPARAM)(20*1000));        // 20 secs for autopop (hide)
            }

            WinPostMsg(hwndDlg, XM_ENABLEITEMS, 0, 0);
        break; }

        /*
         * XM_ENABLEITEMS:
         *
         */

        case XM_ENABLEITEMS:
        {
            BOOL fXFolder = winhIsDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XFOLDER);
            BOOL fXFldDesktop = winhIsDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XFLDDESKTOP);

            WinEnableControl(hwndDlg, ID_XCDI_XWPCLS_XFLDDISK, fXFolder);
            // winhSetDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XFLDDISK, fXFolder);

            WinEnableControl(hwndDlg, ID_XCDI_XWPCLS_XFLDSTARTUP, fXFolder);
            // winhSetDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XFLDSTARTUP, fXFolder);

            WinEnableControl(hwndDlg, ID_XCDI_XWPCLS_XFLDSHUTDOWN,
                                fXFolder && fXFldDesktop);
            winhSetDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XFLDSHUTDOWN,
                                fXFolder && fXFldDesktop);
        break; }

        /*
         * WM_COMMAND:
         *
         */

        case WM_COMMAND:
        {
            BOOL    fDismiss = TRUE;

            if ((USHORT)mp1 == DID_OK)
            {
                // "OK" button pressed:
                PXWPCLASSES     pxwpcOld = WinQueryWindowPtr(hwndDlg, QWL_USER),
                                pxwpcNew = pxwpcOld + 1;
                                    // we had allocated two structures above
                XSTRING         strDereg,
                                strReg,
                                strReplace,
                                strUnreplace;
                BOOL            fDereg = FALSE,
                                fReg = FALSE;

                xstrInit(&strDereg, 0);
                xstrInit(&strReg, 0);
                xstrInit(&strReplace, 0);
                xstrInit(&strUnreplace, 0);

                // get new selections into second structure
                pxwpcNew->fXFldObject = winhIsDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XFLDOBJECT);
                pxwpcNew->fXFolder = winhIsDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XFOLDER);
                pxwpcNew->fXFldDisk = winhIsDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XFLDDISK);
                pxwpcNew->fXFldDesktop = winhIsDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XFLDDESKTOP);
                pxwpcNew->fXFldDataFile = winhIsDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XFLDDATAFILE);
                pxwpcNew->fXFldProgramFile = winhIsDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XFLDPROGRAMFILE);
                pxwpcNew->fXWPSound = winhIsDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XWPSOUND);
                pxwpcNew->fXWPMouse = winhIsDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XWPMOUSE);
                pxwpcNew->fXWPKeyboard = winhIsDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XWPKEYBOARD);

                pxwpcNew->fXWPSetup = winhIsDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XWPSETUP);
                pxwpcNew->fXFldSystem = winhIsDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XFLDSYSTEM);
                pxwpcNew->fXFldWPS = winhIsDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XFLDWPS);
                pxwpcNew->fXWPScreen = winhIsDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XWPSCREEN);
                pxwpcNew->fXWPMedia = winhIsDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XWPMEDIA);
                pxwpcNew->fXFldStartup = winhIsDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XFLDSTARTUP);
                pxwpcNew->fXFldShutdown = winhIsDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XFLDSHUTDOWN);
                pxwpcNew->fXWPClassList = winhIsDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XWPCLASSLIST);
                pxwpcNew->fXWPTrashCan = winhIsDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XWPTRASHCAN);
                pxwpcNew->fXWPString = winhIsDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XWPSTRING);
                pxwpcNew->fXCenter = winhIsDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XCENTER);

                // compare old and new selections:
                // if we have a difference, add the class name
                // to the respective string to (de)register

                if ((pxwpcOld->fXFldObject) && (!pxwpcNew->fXFldObject))
                {
                    // deregister XFldObject
                    xstrcat(&strDereg, "XFldObject\n", 0);
                    // unreplace XFldObject
                    xstrcat(&strUnreplace, "WPObject XFldObject\n", 0);
                }
                else if ((pxwpcNew->fXFldObject) && (!pxwpcOld->fXFldObject))
                {
                    // register
                    xstrcat(&strReg, "XFldObject\n", 0);
                    // replace
                    xstrcat(&strReplace, "WPObject XFldObject\n", 0);
                }

                if ((pxwpcOld->fXFolder) && (!pxwpcNew->fXFolder))
                {
                    // deregister XFolder
                    xstrcat(&strDereg, "XFolder\n", 0);
                    // unreplace XFolder
                    xstrcat(&strUnreplace, "WPFolder XFolder\n", 0);
                }
                else if ((pxwpcNew->fXFolder) && (!pxwpcOld->fXFolder))
                {
                    // register
                    xstrcat(&strReg, "XFolder\n", 0);
                    // replace
                    xstrcat(&strReplace, "WPFolder XFolder\n", 0);
                }

                if ((pxwpcOld->fXFldDisk) && (!pxwpcNew->fXFldDisk))
                {
                    // deregister XFldDisk
                    xstrcat(&strDereg, "XFldDisk\n", 0);
                    // unreplace XFldDisk
                    xstrcat(&strUnreplace, "WPDisk XFldDisk\n", 0);
                }
                else if ((pxwpcNew->fXFldDisk) && (!pxwpcOld->fXFldDisk))
                {
                    // register
                    xstrcat(&strReg, "XFldDisk\n", 0);
                    // replace
                    xstrcat(&strReplace, "WPDisk XFldDisk\n", 0);
                }

                if ((pxwpcOld->fXFldDesktop) && (!pxwpcNew->fXFldDesktop))
                {
                    // deregister XFldDesktop
                    xstrcat(&strDereg, "XFldDesktop\n", 0);
                    // unreplace XFldDesktop
                    xstrcat(&strUnreplace, "WPDesktop XFldDesktop\n", 0);
                }
                else if ((pxwpcNew->fXFldDesktop) && (!pxwpcOld->fXFldDesktop))
                {
                    // register
                    xstrcat(&strReg, "XFldDesktop\n", 0);
                    // replace
                    xstrcat(&strReplace, "WPDesktop XFldDesktop\n", 0);
                }

                if ((pxwpcOld->fXFldDataFile) && (!pxwpcNew->fXFldDataFile))
                {
                    // deregister XFldDataFile
                    xstrcat(&strDereg, "XFldDataFile\n", 0);
                    // unreplace XFldDataFile
                    xstrcat(&strUnreplace, "WPDataFile XFldDataFile\n", 0);
                }
                else if ((pxwpcNew->fXFldDataFile) && (!pxwpcOld->fXFldDataFile))
                {
                    // register
                    xstrcat(&strReg, "XFldDataFile\n", 0);
                    // replace
                    xstrcat(&strReplace, "WPDataFile XFldDataFile\n", 0);
                }

                if ((pxwpcOld->fXFldProgramFile) && (!pxwpcNew->fXFldProgramFile))
                {
                    // deregister XFldProgramFile
                    xstrcat(&strDereg, "XFldProgramFile\n", 0);
                    // unreplace XFldProgramFile
                    xstrcat(&strUnreplace, "WPProgramFile XFldProgramFile\n", 0);
                }
                else if ((pxwpcNew->fXFldProgramFile) && (!pxwpcOld->fXFldProgramFile))
                {
                    // register
                    xstrcat(&strReg, "XFldProgramFile\n", 0);
                    // replace
                    xstrcat(&strReplace, "WPProgramFile XFldProgramFile\n", 0);
                }

                if ((pxwpcOld->fXWPSound) && (!pxwpcNew->fXWPSound))
                {
                    // deregister XWPSound
                    xstrcat(&strDereg, "XWPSound\n", 0);
                    // unreplace XWPSound
                    xstrcat(&strUnreplace, "WPSound XWPSound\n", 0);
                }
                else if ((pxwpcNew->fXWPSound) && (!pxwpcOld->fXWPSound))
                {
                    // register
                    xstrcat(&strReg, "XWPSound\n", 0);
                    // replace
                    xstrcat(&strReplace, "WPSound XWPSound\n", 0);
                }

                if ((pxwpcOld->fXWPMouse) && (!pxwpcNew->fXWPMouse))
                {
                    // deregister XWPMouse
                    xstrcat(&strDereg, "XWPMouse\n", 0);
                    // unreplace XWPMouse
                    xstrcat(&strUnreplace, "WPMouse XWPMouse\n", 0);
                }
                else if ((pxwpcNew->fXWPMouse) && (!pxwpcOld->fXWPMouse))
                {
                    // register
                    xstrcat(&strReg, "XWPMouse\n", 0);
                    // replace
                    xstrcat(&strReplace, "WPMouse XWPMouse\n", 0);
                }

                if ((pxwpcOld->fXWPKeyboard) && (!pxwpcNew->fXWPKeyboard))
                {
                    // deregister XWPKeyboard
                    xstrcat(&strDereg, "XWPKeyboard\n", 0);
                    // unreplace XWPKeyboard
                    xstrcat(&strUnreplace, "WPKeyboard XWPKeyboard\n", 0);
                }
                else if ((pxwpcNew->fXWPKeyboard) && (!pxwpcOld->fXWPKeyboard))
                {
                    // register
                    xstrcat(&strReg, "XWPKeyboard\n", 0);
                    // replace
                    xstrcat(&strReplace, "WPKeyboard XWPKeyboard\n", 0);
                }

                /*
                 * new classes (no replacements):
                 *
                 */

                if ((pxwpcOld->fXWPSetup) && (!pxwpcNew->fXWPSetup))
                    // deregister XWPSetup
                    xstrcat(&strDereg, "XWPSetup\n", 0);
                else if ((pxwpcNew->fXWPSetup) && (!pxwpcOld->fXWPSetup))
                    // register
                    xstrcat(&strReg, "XWPSetup\n", 0);

                if ((pxwpcOld->fXFldSystem) && (!pxwpcNew->fXFldSystem))
                    // deregister XFldSystem
                    xstrcat(&strDereg, "XFldSystem\n", 0);
                else if ((pxwpcNew->fXFldSystem) && (!pxwpcOld->fXFldSystem))
                    // register
                    xstrcat(&strReg, "XFldSystem\n", 0);

                if ((pxwpcOld->fXFldWPS) && (!pxwpcNew->fXFldWPS))
                    // deregister XFldWPS
                    xstrcat(&strDereg, "XFldWPS\n", 0);
                else if ((pxwpcNew->fXFldWPS) && (!pxwpcOld->fXFldWPS))
                    // register
                    xstrcat(&strReg, "XFldWPS\n", 0);

                if ((pxwpcOld->fXWPScreen) && (!pxwpcNew->fXWPScreen))
                    // deregister XWPScreen
                    xstrcat(&strDereg, "XWPScreen\n", 0);
                else if ((pxwpcNew->fXWPScreen) && (!pxwpcOld->fXWPScreen))
                    // register
                    xstrcat(&strReg, "XWPScreen\n", 0);

                if ((pxwpcOld->fXWPMedia) && (!pxwpcNew->fXWPMedia))
                    // deregister XWPMedia
                    xstrcat(&strDereg, "XWPMedia\n", 0);
                else if ((pxwpcNew->fXWPMedia) && (!pxwpcOld->fXWPMedia))
                    // register
                    xstrcat(&strReg, "XWPMedia\n", 0);

                if ((pxwpcOld->fXFldStartup) && (!pxwpcNew->fXFldStartup))
                    // deregister XFldStartup
                    xstrcat(&strDereg, "XFldStartup\n", 0);
                else if ((pxwpcNew->fXFldStartup) && (!pxwpcOld->fXFldStartup))
                    // register
                    xstrcat(&strReg, "XFldStartup\n", 0);

                if ((pxwpcOld->fXFldShutdown) && (!pxwpcNew->fXFldShutdown))
                    // deregister XFldShutdown
                    xstrcat(&strDereg, "XFldShutdown\n", 0);
                else if ((pxwpcNew->fXFldShutdown) && (!pxwpcOld->fXFldShutdown))
                    // register
                    xstrcat(&strReg, "XFldShutdown\n", 0);

                if ((pxwpcOld->fXWPClassList) && (!pxwpcNew->fXWPClassList))
                    // deregister XWPClassList
                    xstrcat(&strDereg, "XWPClassList\n", 0);
                else if ((pxwpcNew->fXWPClassList) && (!pxwpcOld->fXWPClassList))
                    // register
                    xstrcat(&strReg, "XWPClassList\n", 0);

                if ((pxwpcOld->fXWPTrashCan) && (!pxwpcNew->fXWPTrashCan))
                {
                    // deregister XWPTrashCan
                    xstrcat(&strDereg, "XWPTrashCan\n", 0);
                    // deregister XWPTrashObject
                    xstrcat(&strDereg, "XWPTrashObject\n", 0);
                }
                else if ((pxwpcNew->fXWPTrashCan) && (!pxwpcOld->fXWPTrashCan))
                {
                    xstrcat(&strReg, "XWPTrashCan\n", 0);
                    xstrcat(&strReg, "XWPTrashObject\n", 0);
                }

                if ((pxwpcOld->fXWPString) && (!pxwpcNew->fXWPString))
                    // deregister XWPString
                    xstrcat(&strDereg, "XWPString\n", 0);
                else if ((pxwpcNew->fXWPString) && (!pxwpcOld->fXWPString))
                    // register
                    xstrcat(&strReg, "XWPString\n", 0);

                if ((pxwpcOld->fXCenter) && (!pxwpcNew->fXCenter))
                    // deregister XCenter
                    xstrcat(&strDereg, "XCenter\n", 0);
                else if ((pxwpcNew->fXCenter) && (!pxwpcOld->fXCenter))
                    // register
                    xstrcat(&strReg, "XCenter\n", 0);

                // check if we have anything to do
                fReg = (strReg.ulLength != 0);
                fDereg = (strDereg.ulLength != 0);

                if ((fReg) || (fDereg))
                {
                    // OK, class selections have changed:
                    XSTRING         strMessage;
                    CHAR            szTemp[1000];

                    xstrInit(&strMessage, 100);
                    // compose confirmation string
                    cmnGetMessage(NULL, 0, szTemp, sizeof(szTemp),
                                  142); // "You have made the following changes to the XWorkplace class setup:"
                    xstrcpy(&strMessage, szTemp, 0);
                    xstrcatc(&strMessage, '\n');
                    if (fReg)
                    {
                        xstrcat(&strMessage, "\n", 0);
                        cmnGetMessage(NULL, 0, szTemp, sizeof(szTemp),
                                      143); // "Register the following classes:"
                        xstrcat(&strMessage, szTemp, 0);
                        xstrcat(&strMessage, "\n\n", 0);
                        xstrcat(&strMessage, strReg.psz, strReg.ulLength);
                        if (strReplace.ulLength)
                        {
                            xstrcatc(&strMessage, '\n');
                            cmnGetMessage(NULL, 0, szTemp, sizeof(szTemp),
                                          144); // "Replace the following classes:"
                            xstrcat(&strMessage, szTemp, 0);
                            xstrcat(&strMessage, "\n\n", 0);
                            xstrcat(&strMessage, strReplace.psz, strReplace.ulLength);
                        }
                    }
                    if (fDereg)
                    {
                        xstrcatc(&strMessage, '\n');
                        cmnGetMessage(NULL, 0, szTemp, sizeof(szTemp),
                                      145); // "Deregister the following classes:"
                        xstrcat(&strMessage, szTemp, 0);
                        xstrcat(&strMessage, "\n\n", 0);
                        xstrcat(&strMessage, strDereg.psz, strDereg.ulLength);
                        if (strUnreplace.ulLength)
                        {
                            xstrcatc(&strMessage, '\n');
                            cmnGetMessage(NULL, 0, szTemp, sizeof(szTemp),
                                          146); // "Undo the following class replacements:"
                            xstrcat(&strMessage, szTemp, 0);
                            xstrcat(&strMessage, "\n\n", 0);
                            xstrcat(&strMessage, strUnreplace.psz, strUnreplace.ulLength);
                        }
                    }
                    xstrcatc(&strMessage, '\n');
                    cmnGetMessage(NULL, 0, szTemp, sizeof(szTemp),
                                  147); // "Are you sure you want to do this?"
                    xstrcat(&strMessage, szTemp, 0);

                    // confirm class list changes
                    if (cmnMessageBox(hwndDlg,
                                      "XWorkplace Setup",
                                      strMessage.psz,
                                      MB_YESNO) == MBID_YES)
                    {
                        // "Yes" pressed: go!!
                        PSZ         p = NULL;
                        XSTRING     strFailing;
                        HPOINTER    hptrOld = winhSetWaitPointer();

                        xstrInit(&strFailing, 0);

                        // unreplace classes
                        p = strUnreplace.psz;
                        if (p)
                            while (*p)
                            {
                                // string components: "OldClass NewClass\n"
                                PSZ     pSpace = strchr(p, ' '),
                                        pEOL = strchr(pSpace, '\n');
                                if ((pSpace) && (pEOL))
                                {
                                    PSZ     pszReplacedClass = strhSubstr(p, pSpace),
                                            pszReplacementClass = strhSubstr(pSpace + 1, pEOL);
                                    if ((pszReplacedClass) && (pszReplacementClass))
                                    {
                                        if (!WinReplaceObjectClass(pszReplacedClass,
                                                                   pszReplacementClass,
                                                                   FALSE))  // unreplace!
                                        {
                                            // error: append to string list
                                            xstrcat(&strFailing, pszReplacedClass, 0);
                                            xstrcatc(&strFailing, '\n');
                                        }
                                    }
                                    if (pszReplacedClass)
                                        free(pszReplacedClass);
                                    if (pszReplacementClass)
                                        free(pszReplacementClass);
                                }
                                else
                                    break;

                                p = pEOL + 1;
                            }

                        // deregister classes
                        p = strDereg.psz;
                        if (p)
                            while (*p)
                            {
                                PSZ     pEOL = strchr(p, '\n');
                                if (pEOL)
                                {
                                    PSZ     pszClass = strhSubstr(p, pEOL);
                                    if (pszClass)
                                    {
                                        if (!WinDeregisterObjectClass(pszClass))
                                        {
                                            // error: append to string list
                                            xstrcat(&strFailing, pszClass, 0);
                                            xstrcatc(&strFailing, '\n');
                                        }
                                        free(pszClass);
                                    }
                                }
                                else
                                    break;

                                p = pEOL + 1;
                            }

                        // register new classes
                        p = strReg.psz;
                        if (p)
                            while (*p)
                            {
                                APIRET  arc = NO_ERROR;
                                CHAR    szRegisterError[300];

                                PSZ     pEOL = strchr(p, '\n');
                                if (pEOL)
                                {
                                    PSZ     pszClass = strhSubstr(p, pEOL);
                                    if (pszClass)
                                    {
                                        arc = winhRegisterClass(pszClass,
                                                                cmnQueryMainModuleFilename(),
                                                                        // XFolder module
                                                                szRegisterError,
                                                                sizeof(szRegisterError));
                                        if (arc != NO_ERROR)
                                        {
                                            // error: append to string list
                                            xstrcat(&strFailing, pszClass, 0);
                                            xstrcatc(&strFailing, '\n');
                                        }
                                        free(pszClass);
                                    }
                                }
                                else
                                    break;

                                p = pEOL + 1;
                            }

                        // replace classes
                        p = strReplace.psz;
                        if (p)
                            while (*p)
                            {
                                // string components: "OldClass NewClass\n"
                                PSZ     pSpace = strchr(p, ' '),
                                        pEOL = strchr(pSpace, '\n');
                                if ((pSpace) && (pEOL))
                                {
                                    PSZ     pszReplacedClass = strhSubstr(p, pSpace),
                                            pszReplacementClass = strhSubstr(pSpace + 1, pEOL);
                                    if ((pszReplacedClass) && (pszReplacementClass))
                                    {
                                        if (!WinReplaceObjectClass(pszReplacedClass,
                                                                   pszReplacementClass,
                                                                   TRUE))  // replace!
                                        {
                                            // error: append to string list
                                            xstrcat(&strFailing, pszReplacedClass, 0);
                                            xstrcatc(&strFailing, '\n');
                                        }
                                    }
                                    if (pszReplacedClass)
                                        free(pszReplacedClass);
                                    if (pszReplacementClass)
                                        free(pszReplacementClass);
                                }
                                else
                                    break;

                                p = pEOL + 1;
                            }

                        WinSetPointer(HWND_DESKTOP, hptrOld);

                        // errors?
                        if (strFailing.ulLength)
                        {
                            cmnMessageBoxMsgExt(hwndDlg,
                                                148, // "XWorkplace Setup",
                                                &strFailing.psz, 1,
                                                149,  // "errors... %1"
                                                MB_OK);
                        }
                        else
                            cmnMessageBoxMsg(hwndDlg,
                                             148, // "XWorkplace Setup",
                                             150, // "restart WPS"
                                             MB_OK);
                    }
                    else
                        // "No" pressed:
                        fDismiss = FALSE;

                    xstrClear(&strMessage);
                    xstrClear(&strReg);
                    xstrClear(&strDereg);
                } // end if ((fReg) || (fDereg))
            } // end if ((USHORT)mp1 == DID_OK)

            if (fDismiss)
                // dismiss dialog
                mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
        break; }

        /*
         * WM_CONTROL:
         *
         */

        case WM_CONTROL:
        {
            USHORT  usItemID = SHORT1FROMMP(mp1),
                    usNotifyCode = SHORT2FROMMP(mp1);

            /*
             * DID_TOOLTIP:
             *      "toolinfo" control (comctl.c)
             */

            if (usItemID == DID_TOOLTIP)
            {
                if (usNotifyCode == TTN_NEEDTEXT)
                {
                    PXWPCLASSES     pxwpcOld = WinQueryWindowPtr(hwndDlg, QWL_USER);
                                    // pxwpcNew = pxwpcOld + 1;
                    PTOOLTIPTEXT pttt = (PTOOLTIPTEXT)mp2;
                    CHAR    szMessageID[200];
                    CHAR    szHelpString[3000];
                    const char *pszMessageFile = cmnQueryMessageFile();
                    ULONG   ulID = WinQueryWindowUShort(pttt->hwndTool,
                                                        QWS_ID);
                    ULONG   ulWritten = 0;
                    APIRET  arc = 0;

                    if (pxwpcOld->pszTooltipString)
                        free(pxwpcOld->pszTooltipString);

                    sprintf(szMessageID,
                            "XWPCLS_%04d",
                            ulID);
                    arc = tmfGetMessageExt(NULL,                   // pTable
                                           0,                      // cTable
                                           szHelpString,           // pbBuffer
                                           sizeof(szHelpString),   // cbBuffer
                                           szMessageID,            // pszMessageName
                                           (PSZ)pszMessageFile,         // pszFile
                                           &ulWritten);
                    if (arc != NO_ERROR)
                    {
                        sprintf(szHelpString, "No help item found for \"%s\" "
                                              " in \"%s\". "
                                              "TOOLTIPTEXT.hdr.idFrom: 0x%lX "
                                              "ulID: 0x%lX "
                                              "tmfGetMessage rc: %d",
                                              szMessageID,
                                              pszMessageFile,
                                              pttt->hwndTool,
                                              ulID,
                                              arc);
                    }

                    pxwpcOld->pszTooltipString = strdup(szHelpString);

                    pttt->ulFormat = TTFMT_PSZ;
                    pttt->pszText = pxwpcOld->pszTooltipString;
                }
            }
            else if (usNotifyCode == BN_CLICKED)
            {
                switch (usItemID)      // usID
                {
                    // "XFolder" item checked/unchecked: update others
                    case ID_XCDI_XWPCLS_XFOLDER:
                    case ID_XCDI_XWPCLS_XFLDDESKTOP:
                        WinPostMsg(hwndDlg, XM_ENABLEITEMS, 0, 0);
                    break;
                }

            }
        break; }

        /*
         * WM_HELP:
         *
         */

        case WM_HELP:
        {
            cmnDisplayHelp(NULL,        // active desktop
                           ID_XSH_XWP_CLASSESDLG);
        break; }

        /*
         * WM_DESTROY:
         *
         */

        case WM_DESTROY:
        {
            PXWPCLASSES     pxwpcOld = WinQueryWindowPtr(hwndDlg, QWL_USER);
            if (pxwpcOld->pszTooltipString)
                free(pxwpcOld->pszTooltipString);

            WinDestroyWindow(pxwpcOld->hwndTooltip);

            // free two XWPCLASSES structures
            free(pxwpcOld);
            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
        break; }

        default:
            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
    }

    return (mrc);
}

/* ******************************************************************
 *
 *   XWPSetup "Logo" page notebook callbacks (notebook.c)
 *
 ********************************************************************/

/*
 *@@ XWPSETUPLOGODATA:
 *      window data structure for XWPSetup "Logo" page.
 *
 *@@added V0.9.6 (2000-11-04) [umoeller]
 */

typedef struct _XWPSETUPLOGODATA
{
    HBITMAP     hbmLogo;
    SIZEL       szlLogo;
} XWPSETUPLOGODATA, *PXWPSETUPLOGODATA;

/*
 *@@ setLogoInitPage:
 *      notebook callback function (notebook.c) for the
 *      XWPSetup "Logo" page.
 *      Sets the controls on the page according to the
 *      Global Settings.
 *
 *@@added V0.9.6 (2000-11-04) [umoeller]
 */

VOID setLogoInitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                     ULONG flFlags)        // CBI_* flags (notebook.h)
{
    if (flFlags & CBI_INIT)
    {
        HPS hpsTemp = WinGetPS(pcnbp->hwndDlgPage);
        if (hpsTemp)
        {
            PXWPSETUPLOGODATA pLogoData = malloc(sizeof(XWPSETUPLOGODATA));
            if (pLogoData)
            {
                memset(pLogoData, 0, sizeof(XWPSETUPLOGODATA));
                pcnbp->pUser = pLogoData;

                pLogoData->hbmLogo = GpiLoadBitmap(hpsTemp,
                                                   cmnQueryMainResModuleHandle(),
                                                   ID_XWPBIGLOGO,
                                                   0, 0);   // no stretch;
                if (pLogoData->hbmLogo)
                {
                    BITMAPINFOHEADER2 bmih2;
                    bmih2.cbFix = sizeof(bmih2);
                    if (GpiQueryBitmapInfoHeader(pLogoData->hbmLogo, &bmih2))
                    {
                        pLogoData->szlLogo.cx = bmih2.cx;
                        pLogoData->szlLogo.cy = bmih2.cy;
                    }
                }

                WinReleasePS(hpsTemp);
            }
        }
    }

    if (flFlags & CBI_DESTROY)
    {
        PXWPSETUPLOGODATA pLogoData = (PXWPSETUPLOGODATA)pcnbp->pUser;
        if (pLogoData)
        {
            GpiDeleteBitmap(pLogoData->hbmLogo);
        }
        // pLogoData is freed automatically
    }
}

/*
 *@@ setFeaturesMessages:
 *      notebook callback function (notebook.c) for the
 *      XWPSetup "Logo" page.
 *      This gets really all the messages from the dlg.
 *
 *@@added V0.9.6 (2000-11-04) [umoeller]
 */

BOOL setLogoMessages(PCREATENOTEBOOKPAGE pcnbp,
                     ULONG msg, MPARAM mp1, MPARAM mp2,
                     MRESULT *pmrc)
{
    BOOL    brc = FALSE;

    switch (msg)
    {
        case WM_COMMAND:
            switch ((USHORT)mp1)
            {
                case DID_HELP:
                    cmnShowProductInfo(MMSOUND_SYSTEMSTARTUP);
            }
        break;

        case WM_PAINT:
        {
            PXWPSETUPLOGODATA pLogoData = (PXWPSETUPLOGODATA)pcnbp->pUser;
            RECTL   rclDlg,
                    rclPaint;
            HPS     hps = WinBeginPaint(pcnbp->hwndDlgPage,
                                        NULLHANDLE,
                                        &rclPaint);
            // switch to RGB
            GpiCreateLogColorTable(hps, 0, LCOLF_RGB, 0, 0, NULL);
            WinFillRect(hps,
                        &rclPaint,
                        0x00CCCCCC); // 204, 204, 204 -> light gray; that's in the bitmap,
                                     // and it's also the SYSCLR_DIALOGBACKGROUND,
                                     // but just in case the user changed it...
            WinQueryWindowRect(pcnbp->hwndDlgPage,
                               &rclDlg);
            if (pLogoData)
            {
                POINTL ptl;
                // center bitmap:
                ptl.x       =   ((rclDlg.xRight - rclDlg.xLeft)
                                  - pLogoData->szlLogo.cx) / 2;
                ptl.y       =   ((rclDlg.yTop - rclDlg.yBottom)
                                  - pLogoData->szlLogo.cy) / 2;
                WinDrawBitmap(hps,
                              pLogoData->hbmLogo,
                              NULL,
                              &ptl,
                              0, 0, DBM_NORMAL);
            }
            WinReleasePS(hps);
            brc = TRUE;
        break; }
    }

    return (brc);
}

/* ******************************************************************
 *
 *   XWPSetup "Features" page notebook callbacks (notebook.c)
 *
 ********************************************************************/

/*
 *@@ setFeaturesInitPage:
 *      notebook callback function (notebook.c) for the
 *      XWPSetup "Features" page.
 *      Sets the controls on the page according to the
 *      Global Settings.
 *
 *@@changed V0.9.1 (2000-02-01) [umoeller]: added global hotkeys flag
 *@@changed V0.9.9 (2001-01-31) [umoeller]: added "replace folder refresh"
 */

VOID setFeaturesInitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                         ULONG flFlags)        // CBI_* flags (notebook.h)
{
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    PCKERNELGLOBALS  pKernelGlobals = krnQueryGlobals();

    HWND hwndFeaturesCnr = WinWindowFromID(pcnbp->hwndDlgPage,
                                           ID_XCDI_CONTAINER);

    if (flFlags & CBI_INIT)
    {
        PCHECKBOXRECORDCORE preccThis,
                            preccParent;
        ULONG               ul,
                            cRecords;
        HAB                 hab = WinQueryAnchorBlock(pcnbp->hwndDlgPage);
        HMODULE             hmodNLS = cmnQueryNLSModuleHandle(FALSE);

        if (pcnbp->pUser == NULL)
        {
            // first call: backup Global Settings for "Undo" button;
            // this memory will be freed automatically by the
            // common notebook window function (notebook.c) when
            // the notebook page is destroyed
            pcnbp->pUser = malloc(sizeof(GLOBALSETTINGS));
            memcpy(pcnbp->pUser, pGlobalSettings, sizeof(GLOBALSETTINGS));
        }

        if (!ctlMakeCheckboxContainer(pcnbp->hwndDlgPage,
                                      ID_XCDI_CONTAINER))
            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                   "ctlMakeCheckboxContainer failed.");
        else
        {
            cRecords = sizeof(G_FeatureItemsList) / sizeof(FEATURESITEM);

            G_pFeatureRecordsList
                = (PCHECKBOXRECORDCORE)cnrhAllocRecords(hwndFeaturesCnr,
                                                        sizeof(CHECKBOXRECORDCORE),
                                                        cRecords);
            // insert feature records:
            // start for-each-record loop
            preccThis = G_pFeatureRecordsList;
            ul = 0;
            while (preccThis)
            {
                // load NLS string for feature
                cmnLoadString(hab,
                              hmodNLS,
                              G_FeatureItemsList[ul].usFeatureID, // in: string ID
                              &(G_FeatureItemsList[ul].pszNLSString)); // out: NLS string

                // copy FEATURESITEM to record core
                preccThis->ulStyle = G_FeatureItemsList[ul].ulStyle;
                preccThis->ulItemID = G_FeatureItemsList[ul].usFeatureID;
                preccThis->usCheckState = 0;        // unchecked
                preccThis->recc.pszTree = G_FeatureItemsList[ul].pszNLSString;

                preccParent = NULL;

                // find parent record if != 0
                if (G_FeatureItemsList[ul].usParentID)
                {
                    // parent specified:
                    // search records we have prepared so far
                    ULONG ul2 = 0;
                    PCHECKBOXRECORDCORE preccThis2 = G_pFeatureRecordsList;
                    for (ul2 = 0; ul2 < ul; ul2++)
                    {
                        if (preccThis2->ulItemID == G_FeatureItemsList[ul].usParentID)
                        {
                            preccParent = preccThis2;
                            break;
                        }
                        preccThis2 = (PCHECKBOXRECORDCORE)preccThis2->recc.preccNextRecord;
                    }
                }

                cnrhInsertRecords(hwndFeaturesCnr,
                                  (PRECORDCORE)preccParent,
                                  (PRECORDCORE)preccThis,
                                  TRUE, // invalidate
                                  NULL,
                                  CRA_RECORDREADONLY,
                                  1);

                // next record
                preccThis = (PCHECKBOXRECORDCORE)preccThis->recc.preccNextRecord;
                ul++;
            }
        } // end if (ctlMakeCheckboxContainer(pcnbp->hwndPage,

        // register tooltip class
        if (!ctlRegisterTooltip(WinQueryAnchorBlock(pcnbp->hwndDlgPage)))
            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                   "ctlRegisterTooltip failed.");
        else
        {
            // create tooltip
            pcnbp->hwndTooltip = WinCreateWindow(HWND_DESKTOP,  // parent
                                                 COMCTL_TOOLTIP_CLASS, // wnd class
                                                 "",            // window text
                                                 XWP_TOOLTIP_STYLE,
                                                      // tooltip window style (common.h)
                                                 10, 10, 10, 10,    // window pos and size, ignored
                                                 pcnbp->hwndDlgPage, // owner window -- important!
                                                 HWND_TOP,      // hwndInsertBehind, ignored
                                                 DID_TOOLTIP, // window ID, optional
                                                 NULL,          // control data
                                                 NULL);         // presparams

            if (!pcnbp->hwndTooltip)
               cmnLog(__FILE__, __LINE__, __FUNCTION__,
                      "WinCreateWindow failed creating tooltip.");
            else
            {
                // tooltip successfully created:
                // add tools (i.e. controls of the dialog)
                // according to the usToolIDs array
                TOOLINFO    ti = {0};
                ti.ulFlags = /* TTF_CENTERBELOW | */ TTF_SUBCLASS;
                ti.hwndToolOwner = pcnbp->hwndDlgPage;
                ti.pszText = PSZ_TEXTCALLBACK;  // send TTN_NEEDTEXT
                // add cnr as tool to tooltip control
                ti.hwndTool = hwndFeaturesCnr;
                WinSendMsg(pcnbp->hwndTooltip,
                           TTM_ADDTOOL,
                           (MPARAM)0,
                           &ti);

                // set timers
                WinSendMsg(pcnbp->hwndTooltip,
                           TTM_SETDELAYTIME,
                           (MPARAM)TTDT_AUTOPOP,
                           (MPARAM)(40*1000));        // 40 secs for autopop (hide)
            }
        }
    }

    if (flFlags & CBI_SET)
    {
        ctlSetRecordChecked(hwndFeaturesCnr, ID_XCSI_REPLACEICONS,
                pGlobalSettings->fReplaceIcons);
        ctlSetRecordChecked(hwndFeaturesCnr, ID_XCSI_RESIZESETTINGSPAGES,
                pGlobalSettings->fResizeSettingsPages);
        ctlSetRecordChecked(hwndFeaturesCnr, ID_XCSI_ADDOBJECTPAGE,
                pGlobalSettings->AddObjectPage);
        ctlSetRecordChecked(hwndFeaturesCnr, ID_XCSI_REPLACEFILEPAGE,
                pGlobalSettings->fReplaceFilePage);
        ctlSetRecordChecked(hwndFeaturesCnr, ID_XCSI_XSYSTEMSOUNDS,
                pGlobalSettings->fXSystemSounds);

        ctlSetRecordChecked(hwndFeaturesCnr, ID_XCSI_ENABLESTATUSBARS,
                pGlobalSettings->fEnableStatusBars);
        ctlSetRecordChecked(hwndFeaturesCnr, ID_XCSI_ENABLESNAP2GRID,
                pGlobalSettings->fEnableSnap2Grid);
        ctlSetRecordChecked(hwndFeaturesCnr, ID_XCSI_ENABLEFOLDERHOTKEYS,
                pGlobalSettings->fEnableFolderHotkeys);
        ctlSetRecordChecked(hwndFeaturesCnr, ID_XCSI_EXTFOLDERSORT,
                pGlobalSettings->ExtFolderSort);
        // ctlSetRecordChecked(hwndFeaturesCnr, ID_XCSI_MONITORCDROMS,
           //      pGlobalSettings->MonitorCDRoms);

        ctlSetRecordChecked(hwndFeaturesCnr, ID_XCSI_ANIMOUSE,
                pGlobalSettings->fAniMouse);
        ctlSetRecordChecked(hwndFeaturesCnr, ID_XCSI_XWPHOOK,
                pGlobalSettings->fEnableXWPHook);
        ctlSetRecordChecked(hwndFeaturesCnr, ID_XCSI_GLOBALHOTKEYS,
                hifObjectHotkeysEnabled());
        ctlSetRecordChecked(hwndFeaturesCnr, ID_XCSI_PAGEMAGE,
                pGlobalSettings->fEnablePageMage);

        ctlSetRecordChecked(hwndFeaturesCnr, ID_XCSI_ARCHIVING,
                pGlobalSettings->fReplaceArchiving);
        ctlSetRecordChecked(hwndFeaturesCnr, ID_XCSI_RESTARTWPS,
                pGlobalSettings->fRestartWPS);
        ctlSetRecordChecked(hwndFeaturesCnr, ID_XCSI_XSHUTDOWN,
                pGlobalSettings->fXShutdown);

        ctlSetRecordChecked(hwndFeaturesCnr, ID_XCSI_EXTASSOCS,
                pGlobalSettings->fExtAssocs);
        ctlSetRecordChecked(hwndFeaturesCnr, ID_XCSI_CLEANUPINIS,
                pGlobalSettings->CleanupINIs);
#ifdef __REPLHANDLES__
        ctlSetRecordChecked(hwndFeaturesCnr, ID_XCSI_REPLHANDLES,
                pGlobalSettings->fReplaceHandles);
#endif
        ctlSetRecordChecked(hwndFeaturesCnr, ID_XCSI_REPLFILEEXISTS,
                pGlobalSettings->fReplFileExists);
        ctlSetRecordChecked(hwndFeaturesCnr, ID_XCSI_REPLDRIVENOTREADY,
                pGlobalSettings->fReplDriveNotReady);
        ctlSetRecordChecked(hwndFeaturesCnr, ID_XCSI_XWPTRASHCAN,
                (cmnTrashCanReady() && pGlobalSettings->fTrashDelete));
        ctlSetRecordChecked(hwndFeaturesCnr, ID_XCSI_REPLACEDELETE,
                pGlobalSettings->fReplaceTrueDelete);

        ctlSetRecordChecked(hwndFeaturesCnr, ID_XCSI_REPLACEREFRESH,
                krnReplaceRefreshEnabled());
    }

    if (flFlags & CBI_ENABLE)
    {
        PXWPGLOBALSHARED pXwpGlobalShared = pKernelGlobals->pXwpGlobalShared;

        ctlEnableRecord(hwndFeaturesCnr, ID_XCSI_REPLACEFILEPAGE,
                (  (pKernelGlobals->fXFolder)
                || (pKernelGlobals->fXFldDataFile)
                ));
        ctlEnableRecord(hwndFeaturesCnr, ID_XCSI_ENABLESTATUSBARS,
                (pKernelGlobals->fXFolder));
        ctlEnableRecord(hwndFeaturesCnr, ID_XCSI_ENABLESNAP2GRID,
                (pKernelGlobals->fXFolder));
        ctlEnableRecord(hwndFeaturesCnr, ID_XCSI_ENABLEFOLDERHOTKEYS,
                (pKernelGlobals->fXFolder));
        ctlEnableRecord(hwndFeaturesCnr, ID_XCSI_EXTFOLDERSORT,
                (pKernelGlobals->fXFolder));

        ctlEnableRecord(hwndFeaturesCnr, ID_XCSI_XWPHOOK,
                (pXwpGlobalShared->hwndDaemonObject != NULLHANDLE));
        ctlEnableRecord(hwndFeaturesCnr, ID_XCSI_GLOBALHOTKEYS,
                hifXWPHookReady());
        ctlEnableRecord(hwndFeaturesCnr, ID_XCSI_PAGEMAGE,
                hifXWPHookReady());

        ctlEnableRecord(hwndFeaturesCnr, ID_XCSI_XSYSTEMSOUNDS,
                (   (pKernelGlobals->fXFolder)
                 || (pKernelGlobals->fXFldDesktop)
                ));
        ctlEnableRecord(hwndFeaturesCnr, ID_XCSI_RESTARTWPS,
                (pKernelGlobals->fXFldDesktop));
        ctlEnableRecord(hwndFeaturesCnr, ID_XCSI_XSHUTDOWN,
                (pKernelGlobals->fXFldDesktop));

        ctlEnableRecord(hwndFeaturesCnr, ID_XCSI_EXTASSOCS,
                (pKernelGlobals->fXFldDataFile));
        ctlEnableRecord(hwndFeaturesCnr, ID_XCSI_CLEANUPINIS,
                !(pGlobalSettings->NoWorkerThread));

        ctlEnableRecord(hwndFeaturesCnr, ID_XCSI_REPLDRIVENOTREADY,
                (pKernelGlobals->fXFldDisk));

    }
}

/*
 *@@ setFeaturesChanged:
 *      notebook callback function (notebook.c) for the
 *      XWPSetup "Features" page.
 *      Reacts to changes of any of the dialog controls.
 *
 *@@changed V0.9.1 (2000-02-01) [umoeller]: added global hotkeys flag
 *@@changed V0.9.1 (2000-02-09) [umoeller]: fixed mutex hangs while dialogs were displayed
 *@@changed V0.9.2 (2000-03-19) [umoeller]: "Undo" and "Default" created duplicate records; fixed
 *@@changed V0.9.6 (2000-11-11) [umoeller]: removed extassocs warning
 *@@changed V0.9.7 (2001-01-18) [umoeller]: removed pagemage warning
 *@@changed V0.9.7 (2001-01-22) [umoeller]: now enabling "object" page with hotkeys automatically
 *@@changed V0.9.9 (2001-01-31) [umoeller]: added "replace folder refresh"
 *@@changed V0.9.9 (2001-03-27) [umoeller]: adjusted for notebook.c change with CHECKBOXRECORDCORE notifications
 */

MRESULT setFeaturesItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                               ULONG ulItemID, USHORT usNotifyCode,
                               ULONG ulExtra)      // for checkboxes: contains new state
{
    if (    (ulItemID == ID_XCDI_CONTAINER)
         && (usNotifyCode == CN_RECORDCHECKED)
       )
    {
        // lock global settings to get write access;
        // WARNING: do not show any dialogs when reacting to
        // controls BEFORE these are not unlocked!!!
        GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(__FILE__, __LINE__, __FUNCTION__);

        BOOL fSave = TRUE;

        // flags for delayed dialog showing (after unlocking)
        BOOL fShowHookInstalled = FALSE,
             fShowHookDeinstalled = FALSE,
             fShowClassesSetup = FALSE,
             fShowWarnXShutdown = FALSE,
             fUpdateMouseMovementPage = FALSE,
             fShowRefreshEnabled = FALSE,
             fShowRefreshDisabled = FALSE,
             fShowExtAssocsWarning = FALSE;
        signed char cAskSoundsInstallMsg = -1,      // 1 = installed, 0 = deinstalled
                    cEnableTrashCan = -1;       // 1 = installed, 0 = deinstalled

        ULONG ulUpdateFlags = 0;
                // if set to != 0, this will run the INIT callback with
                // the specified CBI_* flags

        PCHECKBOXRECORDCORE precc = (PCHECKBOXRECORDCORE)ulExtra;

        switch (precc->ulItemID)
        {
            case ID_XCSI_REPLACEICONS:
                pGlobalSettings->fReplaceIcons = precc->usCheckState;
            break;

            case ID_XCSI_RESIZESETTINGSPAGES:
                pGlobalSettings->fResizeSettingsPages = precc->usCheckState;
            break;

            case ID_XCSI_ADDOBJECTPAGE:
                pGlobalSettings->AddObjectPage = precc->usCheckState;
            break;

            case ID_XCSI_XWPHOOK:
            {
                if (hifEnableHook(precc->usCheckState) == precc->usCheckState)
                {
                    // success:
                    pGlobalSettings->fEnableXWPHook = precc->usCheckState;

                    if (precc->usCheckState)
                        fShowHookInstalled = TRUE;
                    else
                        fShowHookDeinstalled = TRUE;

                    // re-enable controls on this page
                    ulUpdateFlags = CBI_SET | CBI_ENABLE;
                }
            break; }

            case ID_XCSI_REPLACEFILEPAGE:
                pGlobalSettings->fReplaceFilePage = precc->usCheckState;
            break;

            case ID_XCSI_XSYSTEMSOUNDS:
                pGlobalSettings->fXSystemSounds = precc->usCheckState;
                // check if sounds are to be installed or de-installed:
                if (sndAddtlSoundsInstalled(WinQueryAnchorBlock(pcnbp->hwndDlgPage))
                             != precc->usCheckState)
                    // yes: set msg for "ask for sound install"
                    // at the bottom when the global semaphores
                    // are unlocked
                    cAskSoundsInstallMsg = (precc->usCheckState);
            break;

            case ID_XCSI_ANIMOUSE:
                pGlobalSettings->fAniMouse = precc->usCheckState;
            break;

            case ID_XCSI_ENABLESTATUSBARS:
                pGlobalSettings->fEnableStatusBars = precc->usCheckState;
                // update status bars for open folders
                xthrPostWorkerMsg(WOM_UPDATEALLSTATUSBARS,
                                  (MPARAM)1,
                                  MPNULL);
                // and open settings notebooks
                ntbUpdateVisiblePage(NULL,   // all somSelf's
                                     SP_XFOLDER_FLDR);
            break;

            case ID_XCSI_ENABLESNAP2GRID:
                pGlobalSettings->fEnableSnap2Grid = precc->usCheckState;
                // update open settings notebooks
                ntbUpdateVisiblePage(NULL,   // all somSelf's
                                     SP_XFOLDER_FLDR);
            break;

            case ID_XCSI_ENABLEFOLDERHOTKEYS:
                pGlobalSettings->fEnableFolderHotkeys = precc->usCheckState;
                // update open settings notebooks
                ntbUpdateVisiblePage(NULL,   // all somSelf's
                                     SP_XFOLDER_FLDR);
            break;

            case ID_XCSI_EXTFOLDERSORT:
                pGlobalSettings->ExtFolderSort = precc->usCheckState;
            break;

            case ID_XCSI_GLOBALHOTKEYS:
                hifEnableObjectHotkeys(precc->usCheckState);
                pGlobalSettings->AddObjectPage = precc->usCheckState;
                                // V0.9.7 (2001-01-22) [umoeller]
                ulUpdateFlags = CBI_SET | CBI_ENABLE;
            break;

            case ID_XCSI_PAGEMAGE:
                if (hifEnablePageMage(precc->usCheckState) == precc->usCheckState)
                {
                    pGlobalSettings->fEnablePageMage = precc->usCheckState;
                    // update "Mouse movement" page
                    fUpdateMouseMovementPage = TRUE;
                }

                ulUpdateFlags = CBI_SET | CBI_ENABLE;
            break;

            case ID_XCSI_ARCHIVING:
                pGlobalSettings->fReplaceArchiving = precc->usCheckState;
            break;

            case ID_XCSI_RESTARTWPS:
                pGlobalSettings->fRestartWPS = precc->usCheckState;
            break;

            case ID_XCSI_XSHUTDOWN:
                pGlobalSettings->fXShutdown = precc->usCheckState;
                // update "Desktop" menu page
                ntbUpdateVisiblePage(NULL,   // all somSelf's
                                     SP_DTP_MENUITEMS);
                if (precc->usCheckState)
                    // show warning at the bottom (outside the
                    // mutex section)
                    fShowWarnXShutdown = TRUE;
            break;

            case ID_XCSI_EXTASSOCS:
                pGlobalSettings->fExtAssocs = precc->usCheckState;
                // re-enable controls on this page
                ulUpdateFlags = CBI_ENABLE;

                if (precc->usCheckState)
                    fShowExtAssocsWarning = TRUE;       // V0.9.9 (2001-02-06) [umoeller]
            break;

            case ID_XCSI_REPLFILEEXISTS:
                pGlobalSettings->fReplFileExists = precc->usCheckState;
            break;

            case ID_XCSI_REPLDRIVENOTREADY:
                pGlobalSettings->fReplDriveNotReady = precc->usCheckState;
            break;

            case ID_XCSI_CLEANUPINIS:
                pGlobalSettings->CleanupINIs = precc->usCheckState;
            break;

            case ID_XCSI_XWPTRASHCAN:
                cEnableTrashCan = precc->usCheckState;
            break;

            case ID_XCSI_REPLACEDELETE:
                pGlobalSettings->fReplaceTrueDelete = precc->usCheckState;
            break;

    #ifdef __REPLHANDLES__
            case ID_XCSI_REPLHANDLES:
                pGlobalSettings->fReplaceHandles = precc->usCheckState;
            break;
    #endif

            case ID_XCSI_REPLACEREFRESH:
                krnEnableReplaceRefresh(precc->usCheckState);
                if (precc->usCheckState)
                    fShowRefreshEnabled = TRUE;
                else
                    fShowRefreshDisabled = TRUE;
            break;

            /*
             * ID_XCDI_SETUP:
             *      "Classes" button
             */

            case ID_XCDI_SETUP:
                fShowClassesSetup = TRUE;
                fSave = FALSE;
            break;

            case DID_UNDO:
            {
                // "Undo" button: get pointer to backed-up Global Settings
                PCGLOBALSETTINGS pGSBackup = (PCGLOBALSETTINGS)(pcnbp->pUser);

                // and restore the settings for this page
                pGlobalSettings->fReplaceIcons = pGSBackup->fReplaceIcons;
                pGlobalSettings->fResizeSettingsPages = pGSBackup->fResizeSettingsPages;
                pGlobalSettings->AddObjectPage = pGSBackup->AddObjectPage;
                pGlobalSettings->fReplaceFilePage = pGSBackup->fReplaceFilePage;
                pGlobalSettings->fXSystemSounds = pGSBackup->fXSystemSounds;

                pGlobalSettings->fEnableStatusBars = pGSBackup->fEnableStatusBars;
                pGlobalSettings->fEnableSnap2Grid = pGSBackup->fEnableSnap2Grid;
                pGlobalSettings->fEnableFolderHotkeys = pGSBackup->fEnableFolderHotkeys;
                pGlobalSettings->ExtFolderSort = pGSBackup->ExtFolderSort;
                // pGlobalSettings->fMonitorCDRoms = pGSBackup->fMonitorCDRoms;

                pGlobalSettings->fAniMouse = pGSBackup->fAniMouse;
                pGlobalSettings->fEnableXWPHook = pGSBackup->fEnableXWPHook;
                // global hotkeys... @@todo V0.9.4 (2000-06-05) [umoeller]
                // pagemage... @@todo V0.9.4 (2000-06-05) [umoeller]

                pGlobalSettings->fReplaceArchiving = pGSBackup->fReplaceArchiving;
                pGlobalSettings->fRestartWPS = pGSBackup->fRestartWPS;
                pGlobalSettings->fXShutdown = pGSBackup->fXShutdown;

                pGlobalSettings->fExtAssocs = pGSBackup->fExtAssocs;
                pGlobalSettings->CleanupINIs = pGSBackup->CleanupINIs;
    #ifdef __REPLHANDLES__
                pGlobalSettings->fReplaceHandles = pGSBackup->fReplaceHandles;
    #endif
                pGlobalSettings->fReplFileExists = pGSBackup->fReplFileExists;
                pGlobalSettings->fReplDriveNotReady = pGSBackup->fReplDriveNotReady;
                // trash can @@todo V0.9.4 (2000-06-05) [umoeller]
                pGlobalSettings->fReplaceTrueDelete = pGSBackup->fReplaceTrueDelete;

                // update the display by calling the INIT callback
                ulUpdateFlags = CBI_SET | CBI_ENABLE;
            break; }

            case DID_DEFAULT:
            {
                // set the default settings for this settings page
                // (this is in common.c because it's also used at
                // WPS startup)
                cmnSetDefaultSettings(pcnbp->ulPageID);
                // update the display by calling the INIT callback
                ulUpdateFlags = CBI_SET | CBI_ENABLE;
            break; }

            default:
                fSave = FALSE;
        }

        // now unlock the global settings for the following
        // stuff; otherwise we block other threads
        cmnUnlockGlobalSettings();

        if (fSave)
            // settings need to be saved:
            cmnStoreGlobalSettings();

        if (fShowClassesSetup)
        {
            // "classes" dialog to be shown (classes button):
            HWND hwndClassesDlg = WinLoadDlg(HWND_DESKTOP,     // parent
                                             pcnbp->hwndFrame,  // owner
                                             fnwpXWorkplaceClasses,
                                             cmnQueryNLSModuleHandle(FALSE),
                                             ID_XCD_XWPINSTALLEDCLASSES,
                                             NULL);
            winhCenterWindow(hwndClassesDlg);
            WinProcessDlg(hwndClassesDlg);
            WinDestroyWindow(hwndClassesDlg);
        }
        else if (fShowHookInstalled)
            // "hook installed" msg
            cmnMessageBoxMsg(pcnbp->hwndFrame,
                             148, 157, MB_OK);
        else if (fShowHookDeinstalled)
            // "hook deinstalled" msg
            cmnMessageBoxMsg(pcnbp->hwndFrame,
                             148, 158, MB_OK);
        else if (fShowWarnXShutdown)
            cmnMessageBoxMsg(pcnbp->hwndFrame,
                             148,       // "XWorkplace Setup"
                             190,
                             MB_OK);
        else if (cAskSoundsInstallMsg != -1)
        {
            if (cmnMessageBoxMsg(pcnbp->hwndFrame,
                                 148,       // "XWorkplace Setup"
                                 (cAskSoundsInstallMsg)
                                    ? 166   // "install?"
                                    : 167,  // "de-install?"
                                 MB_YESNO)
                    == MBID_YES)
            {
                sndInstallAddtlSounds(WinQueryAnchorBlock(pcnbp->hwndDlgPage),
                                      precc->usCheckState);
            }
        }
        else if (cEnableTrashCan != -1)
        {
            cmnEnableTrashCan(pcnbp->hwndFrame,
                               precc->usCheckState);
            pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
        }
        else if (fShowRefreshEnabled)
            // "enabled, warning, unstable" msg
            cmnMessageBoxMsg(pcnbp->hwndFrame,
                             148, 212, MB_OK);
        else if (fShowRefreshDisabled)
            // "must restart wps" msg
            cmnMessageBoxMsg(pcnbp->hwndFrame,
                             148, 207, MB_OK);
        else if (fShowExtAssocsWarning)
            // "warning: assocs gone" msg
            cmnMessageBoxMsg(pcnbp->hwndFrame,
                             148, 208, MB_OK);

        if (ulUpdateFlags)
            pcnbp->pfncbInitPage(pcnbp, ulUpdateFlags);

        if (fUpdateMouseMovementPage)
            // update "Mouse movement" page
            ntbUpdateVisiblePage(NULL,
                                 SP_MOUSE_MOVEMENT);

    } // end if (ulItemID == ID_XCDI_CONTAINER)

    return (0);
}

/*
 *@@ setFeaturesMessages:
 *      notebook callback function (notebook.c) for the
 *      XWPSetup "Features" page.
 *      This gets really all the messages from the dlg.
 *
 *@@added V0.9.1 (99-11-30) [umoeller]
 */

BOOL setFeaturesMessages(PCREATENOTEBOOKPAGE pcnbp,
                         ULONG msg, MPARAM mp1, MPARAM mp2,
                         MRESULT *pmrc)
{
    BOOL    brc = FALSE;

    switch (msg)
    {
        case WM_CONTROL:
        {
            USHORT  usItemID = SHORT1FROMMP(mp1),
                    usNotifyCode = SHORT2FROMMP(mp1);

            switch (usItemID)
            {
                case DID_TOOLTIP:

                    _Pmpf((__FUNCTION__ ": got WM_CONTROL for DID_TOOLTIP"));

                    if (usNotifyCode == TTN_NEEDTEXT)
                    {
                        PTOOLTIPTEXT pttt = (PTOOLTIPTEXT)mp2;
                        PCHECKBOXRECORDCORE precc;
                        HWND         hwndCnr = WinWindowFromID(pcnbp->hwndDlgPage,
                                                               ID_XCDI_CONTAINER);
                        POINTL       ptlMouse;

                        _Pmpf(("  is TTN_NEEDTEXT"));

                        // we use pUser2 for the Tooltip string
                        if (pcnbp->pUser2)
                        {
                            free(pcnbp->pUser2);
                            pcnbp->pUser2 = NULL;
                        }

                        // find record under mouse
                        WinQueryMsgPos(WinQueryAnchorBlock(pcnbp->hwndDlgPage),
                                       &ptlMouse);
                        precc = (PCHECKBOXRECORDCORE)cnrhFindRecordFromPoint(
                                                            hwndCnr,
                                                            &ptlMouse,
                                                            NULL,
                                                            CMA_ICON | CMA_TEXT,
                                                            FRFP_SCREENCOORDS);
                        // _Pmpf(("    precc is 0x%lX", precc));
                        if (precc)
                        {
                            if (precc->ulStyle & WS_VISIBLE)
                            {
                                CHAR        szMessageID[200];
                                CHAR        szHelpString[3000] = "?";
                                const char* pszMessageFile = cmnQueryMessageFile();

                                ULONG       ulWritten = 0;
                                APIRET      arc = 0;

                                sprintf(szMessageID,
                                        "FEATURES_%04d",
                                        precc->ulItemID);
                                arc = tmfGetMessageExt(NULL,                   // pTable
                                                       0,                      // cTable
                                                       szHelpString,           // pbBuffer
                                                       sizeof(szHelpString),   // cbBuffer
                                                       szMessageID,            // pszMessageName
                                                       (PSZ)pszMessageFile,    // pszFile
                                                       &ulWritten);

                                if (arc != NO_ERROR)
                                {
                                    sprintf(szHelpString, "No help item found for \"%s\" "
                                                          " in \"%s\". "
                                                          "TOOLTIPTEXT.hdr.idFrom: 0x%lX "
                                                          "ulID: 0x%lX "
                                                          "tmfGetMessage rc: %d",
                                                          szMessageID,
                                                          pszMessageFile,
                                                          pttt->hwndTool,
                                                          precc->ulItemID,
                                                          arc);
                                }

                                pcnbp->pUser2 = strdup(szHelpString);

                                pttt->ulFormat = TTFMT_PSZ;
                                pttt->pszText = pcnbp->pUser2;
                            }
                        }

                        brc = TRUE;
                    }
                break;
            }
        }
    }

    return (brc);
}

/* ******************************************************************
 *
 *   XWPSetup "Threads" page notebook callbacks (notebook.c)
 *
 ********************************************************************/

/*
 *@@ THREADRECORD:
 *
 *@@added V0.9.9 (2001-03-07) [umoeller]
 */

typedef struct _THREADRECORD
{
    RECORDCORE      recc;
    PSZ             pszThreadName;
    PSZ             pszTID;
    PSZ             pszPriority;
} THREADRECORD, *PTHREADRECORD;

/*
 *@@ ClearThreads:
 *
 *@@added V0.9.9 (2001-03-07) [umoeller]
 */

VOID ClearThreads(HWND hwndCnr)
{
    PTHREADRECORD prec;
    while (    (prec = (PTHREADRECORD)WinSendMsg(hwndCnr,
                                                 CM_QUERYRECORD,
                                                 (MPARAM)NULL,
                                                 MPFROM2SHORT(CMA_FIRST,
                                                              CMA_ITEMORDER)))
            && ((LONG)prec != -1)
          )
    {
        if (prec->pszThreadName)
            free(prec->pszThreadName);
        if (prec->pszTID)
            free(prec->pszTID);
        if (prec->pszPriority)
            free(prec->pszPriority);

        WinSendMsg(hwndCnr,
                   CM_REMOVERECORD,
                   (MPARAM)&prec,
                   MPFROM2SHORT(1,
                                CMA_FREE));
    }
    cnrhInvalidateAll(hwndCnr);
}

/*
 *@@ setThreadsInitPage:
 *      notebook callback function (notebook.c) for the
 *      XWPSetup "Threads" page.
 *      Sets the controls on the page according to the
 *      Global Settings.
 *
 *@@added V0.9.9 (2001-03-07) [umoeller]
 */

VOID setThreadsInitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                        ULONG flFlags)        // CBI_* flags (notebook.h)
{
    HWND hwndCnr = WinWindowFromID(pcnbp->hwndDlgPage,
                                   ID_XFDI_CNR_CNR);

    if (flFlags & CBI_INIT)
    {
        PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();
        XFIELDINFO      xfi[5];
        PFIELDINFO      pfi = NULL;
        int             i = 0;

        // set up cnr details view
        xfi[i].ulFieldOffset = FIELDOFFSET(THREADRECORD, pszThreadName);
        xfi[i].pszColumnTitle = pNLSStrings->pszThreadsThread; // "Thread";
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        xfi[i].ulFieldOffset = FIELDOFFSET(THREADRECORD, pszTID);
        xfi[i].pszColumnTitle = pNLSStrings->pszThreadsTID; // "TID";
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        xfi[i].ulFieldOffset = FIELDOFFSET(THREADRECORD, pszPriority);
        xfi[i].pszColumnTitle = pNLSStrings->pszThreadsPriority; // "Priority";
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        pfi = cnrhSetFieldInfos(hwndCnr,
                                xfi,
                                i,             // array item count
                                TRUE,          // draw lines
                                0);            // return first column

        BEGIN_CNRINFO()
        {
            cnrhSetView(CV_DETAIL | CA_DETAILSVIEWTITLES);
            cnrhSetSplitBarAfter(pfi);
            cnrhSetSplitBarPos(200);
        } END_CNRINFO(hwndCnr);

        WinSetDlgItemText(pcnbp->hwndDlgPage,
                          ID_XFDI_CNR_GROUPTITLE,
                          pNLSStrings->pszThreadsGroupTitle); // "XWorkplace threads")
    }

    if (flFlags & (CBI_SET | CBI_SHOW))
    {
        CHAR szTemp[1000];
        PTHREADINFO paThreadInfos;
        ULONG cThreadInfos = 0;

        ClearThreads(hwndCnr);

        paThreadInfos = thrListThreads(&cThreadInfos);
        if (paThreadInfos)
        {
            // we got thread infos:
            PQPROCSTAT16 pps = prc16GetInfo(NULL);
            ULONG ul;

            for (ul = 0;
                 ul < cThreadInfos;
                 ul++)
            {
                PTHREADINFO pThis = &paThreadInfos[ul];
                PTHREADRECORD prec = (PTHREADRECORD)cnrhAllocRecords(hwndCnr,
                                                                     sizeof(THREADRECORD),
                                                                     1);
                if (prec)
                {
                    ULONG ulpri = prc16QueryThreadPriority(pps,
                                                           doshMyPID(),
                                                           pThis->tid);
                    XSTRING str;
                    prec->pszThreadName = strdup(pThis->pcszThreadName);
                    sprintf(szTemp, "%d (%02lX)", pThis->tid, pThis->tid);
                    prec->pszTID = strdup(szTemp);

                    sprintf(szTemp, "0x%04lX (", ulpri);
                    xstrInitCopy(&str, szTemp, 0);
                    switch (ulpri & 0x0F00)
                    {
                        case 0x0100:
                            xstrcat(&str, "Idle", 0);
                        break;

                        case 0x0200:
                            xstrcat(&str, "Regular", 0);
                        break;

                        case 0x0300:
                            xstrcat(&str, "Time-critical", 0);
                        break;

                        case 0x0400:
                            xstrcat(&str, "Foreground server", 0);
                        break;
                    }

                    sprintf(szTemp, " +%d)", ulpri & 0xFF);
                    xstrcat(&str, szTemp, 0);

                    prec->pszPriority = str.psz;
                }
                else
                    break;

                cnrhInsertRecords(hwndCnr,
                                  NULL,
                                  (PRECORDCORE)prec,
                                  FALSE,        // invalidate?
                                  NULL,
                                  CRA_RECORDREADONLY,
                                  1);
            }

            cnrhInvalidateAll(hwndCnr);

            free(paThreadInfos);
            prc16FreeInfo(pps);
        }
    } // end if (flFlags & CBI_SET)

    if (flFlags & CBI_ENABLE)
    {
    }

    if (flFlags & CBI_DESTROY)
    {
        ClearThreads(hwndCnr);
    }
}

/* ******************************************************************
 *
 *   XWPSetup "Status" page notebook callbacks (notebook.c)
 *
 ********************************************************************/

/*
 *@@ setStatusInitPage:
 *      notebook callback function (notebook.c) for the
 *      XWPSetup "Status" page.
 *      Sets the controls on the page according to the
 *      Global Settings.
 *
 *@@changed V0.9.2 (2000-02-20) [umoeller]: changed build string handling
 *@@changed V0.9.7 (2000-12-14) [umoeller]: removed kernel build
 *@@changed V0.9.9 (2001-03-07) [umoeller]: extracted "Threads" page
 */

VOID setStatusInitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                       ULONG flFlags)        // CBI_* flags (notebook.h)
{
    if (flFlags & CBI_INIT)
    {
        #ifdef DEBUG_XWPSETUP_CLASSES
            // this debugging flag will produce a message box
            // showing the class object addresses in KERNELGLOBALS
            PKERNELGLOBALS  pKernelGlobals = krnQueryGlobals();
            CHAR            szMsg[2000];
            PSZ             pMsg = szMsg;

            pMsg += sprintf(pMsg, "pXFldObject: 0x%lX\n", pKernelGlobals->pXFldObject);
            pMsg += sprintf(pMsg, "pXFolder: 0x%lX\n", pKernelGlobals->pXFolder);
            pMsg += sprintf(pMsg, "pXFldDisk: 0x%lX\n", pKernelGlobals->pXFldDisk);
            pMsg += sprintf(pMsg, "pXFldDesktop: 0x%lX\n", pKernelGlobals->pXFldDesktop);
            pMsg += sprintf(pMsg, "pXFldDataFile: 0x%lX\n", pKernelGlobals->pXFldDataFile);
            pMsg += sprintf(pMsg, "pXFldProgramFile: 0x%lX\n", pKernelGlobals->pXFldProgramFile);
            pMsg += sprintf(pMsg, "pXWPSound: 0x%lX\n", pKernelGlobals->pXWPSound);
            pMsg += sprintf(pMsg, "pXWPMouse: 0x%lX\n", pKernelGlobals->pXWPMouse);
            pMsg += sprintf(pMsg, "pXWPKeyboard: 0x%lX\n", pKernelGlobals->pXWPKeyboard);
            pMsg += sprintf(pMsg, "pXWPSetup: 0x%lX\n", pKernelGlobals->pXWPSetup);
            pMsg += sprintf(pMsg, "pXFldSystem: 0x%lX\n", pKernelGlobals->pXFldSystem);
            pMsg += sprintf(pMsg, "pXFldWPS: 0x%lX\n", pKernelGlobals->pXFldWPS);
            pMsg += sprintf(pMsg, "pXWPScreen: 0x%lX\n", pKernelGlobals->pXWPScreen);
            pMsg += sprintf(pMsg, "pXWPMedia: 0x%lX\n", pKernelGlobals->pXWPMedia);
            pMsg += sprintf(pMsg, "pXFldStartup: 0x%lX\n", pKernelGlobals->pXFldStartup);
            pMsg += sprintf(pMsg, "pXFldShutdown: 0x%lX\n", pKernelGlobals->pXFldShutdown);
            pMsg += sprintf(pMsg, "pXWPClassList: 0x%lX\n", pKernelGlobals->pXWPClassList);
            pMsg += sprintf(pMsg, "pXWPString: 0x%lX\n", pKernelGlobals->pXWPString);
            pMsg += sprintf(pMsg, "pXCenter: 0x%lX\n", pKernelGlobals->pXCenter);

            winhDebugBox("XWorkplace Class Objects", szMsg);
        #endif
    }

    if (flFlags & CBI_SET)
    {
        HMODULE         hmodNLS = cmnQueryNLSModuleHandle(FALSE);
        CHAR            szXFolderBasePath[CCHMAXPATH],
                        szSearchMask[2*CCHMAXPATH],
                        szTemp[200];
        HDIR            hdirFindHandle = HDIR_SYSTEM;
        FILEFINDBUF3    FindBuffer     = {0};      // Returned from FindFirst/Next
        ULONG           ulResultBufLen = sizeof(FILEFINDBUF3);
        ULONG           ulFindCount    = 1;        // Look for 1 file at a time
        APIRET          rc             = NO_ERROR; // Return code

        PCKERNELGLOBALS  pKernelGlobals = krnQueryGlobals();

        HAB             hab = WinQueryAnchorBlock(pcnbp->hwndDlgPage);

        ULONG           ulSoundStatus = xmmQueryStatus();

        // kernel version number V0.9.7 (2000-12-14) [umoeller]
        sprintf(szSearchMask,
                "%s (%s)",
                XFOLDER_VERSION,
                __DATE__);

        WinSetDlgItemText(pcnbp->hwndDlgPage,
                          ID_XCDI_INFO_KERNEL_RELEASE,
                          szSearchMask);

        // sound status
        strcpy(szSearchMask,
               (ulSoundStatus == MMSTAT_UNKNOWN)
                       ? "not initialized"
               : (ulSoundStatus == MMSTAT_WORKING)
                       ? "OK"
               : (ulSoundStatus == MMSTAT_MMDIRNOTFOUND)
                       ? "MMPM/2 directory not found"
               : (ulSoundStatus == MMSTAT_DLLNOTFOUND)
                       ? "MMPM/2 DLLs not found"
               : (ulSoundStatus == MMSTAT_IMPORTSFAILED)
                       ? "MMPM/2 imports failed"
               : (ulSoundStatus == MMSTAT_CRASHED)
                       ? "Media thread crashed"
               : (ulSoundStatus == MMSTAT_DISABLED)
                       ? "Disabled"
               : "unknown"
               );
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XCDI_INFO_SOUNDSTATUS,
                          szSearchMask);

        // language drop-down box
        WinSendDlgItemMsg(pcnbp->hwndDlgPage, ID_XCDI_INFO_LANGUAGE, LM_DELETEALL, 0, 0);

        if (cmnQueryXWPBasePath(szXFolderBasePath))
        {
            sprintf(szSearchMask, "%s\\bin\\xfldr*.dll", szXFolderBasePath);

            #ifdef DEBUG_LANGCODES
                _Pmpf(("  szSearchMask: %s", szSearchMask));
                _Pmpf(("  DosFindFirst"));
            #endif
            rc = DosFindFirst(szSearchMask,         // file pattern
                              &hdirFindHandle,      // directory search handle
                              FILE_NORMAL,          // search attribute
                              &FindBuffer,          // result buffer
                              ulResultBufLen,       // result buffer length
                              &ulFindCount,         // number of entries to find
                              FIL_STANDARD);        // return level 1 file info

            if (rc != NO_ERROR)
                winhDebugBox(pcnbp->hwndFrame,
                         "XFolder",
                         "XFolder was unable to find any National Language Support DLLs. You need to re-install XFolder.");
            else
            {
                // no error:
                #ifdef DEBUG_LANGCODES
                    _Pmpf(("  Found file: %s", FindBuffer.achName));
                #endif
                AddResourceDLLToLB(pcnbp->hwndDlgPage,
                                   ID_XCDI_INFO_LANGUAGE,
                                   szXFolderBasePath,
                                   FindBuffer.achName);

                // keep finding files
                while (rc != ERROR_NO_MORE_FILES)
                {
                    ulFindCount = 1;                      // reset find count

                    rc = DosFindNext(hdirFindHandle,      // directory handle
                                     &FindBuffer,         // result buffer
                                     ulResultBufLen,      // result buffer length
                                     &ulFindCount);       // number of entries to find

                    if (rc == NO_ERROR)
                    {
                        AddResourceDLLToLB(pcnbp->hwndDlgPage,
                                           ID_XCDI_INFO_LANGUAGE,
                                           szXFolderBasePath,
                                           FindBuffer.achName);
                        #ifdef DEBUG_LANGCODES
                            _Pmpf(("  Found next: %s", FindBuffer.achName));
                        #endif
                    }
                } // endwhile

                rc = DosFindClose(hdirFindHandle);    // close our find handle
                if (rc != NO_ERROR)
                    winhDebugBox(pcnbp->hwndFrame,
                             "XFolder",
                             "DosFindClose error");

                #ifdef DEBUG_LANGCODES
                    _Pmpf(("  Selecting: %s", cmnQueryLanguageCode()));
                #endif
                WinSendDlgItemMsg(pcnbp->hwndDlgPage, ID_XCDI_INFO_LANGUAGE,
                                  LM_SELECTITEM,
                                  WinSendDlgItemMsg(pcnbp->hwndDlgPage,
                                                    ID_XCDI_INFO_LANGUAGE, // find matching item
                                                    LM_SEARCHSTRING,
                                                    MPFROM2SHORT(LSS_SUBSTRING, LIT_FIRST),
                                                    (MPARAM)cmnQueryLanguageCode()),
                                  (MPARAM)TRUE); // select
            }
        } // end if (cmnQueryXWPBasePath...

        // NLS info
        if (WinLoadString(hab, hmodNLS, ID_XSSI_XFOLDERVERSION,
                    sizeof(szSearchMask), szSearchMask))
            WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XCDI_INFO_NLS_RELEASE,
                    szSearchMask);

        if (WinLoadString(hab, hmodNLS, ID_XSSI_NLS_AUTHOR,
                    sizeof(szSearchMask), szSearchMask))
            WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XCDI_INFO_NLS_AUTHOR,
                    szSearchMask);
    } // end if (flFlags & CBI_SET)
}

/*
 *@@ setStatusItemChanged:
 *      notebook callback function (notebook.c) for the
 *      XWPSetup "Status" page.
 *      Reacts to changes of any of the dialog controls.
 */

MRESULT setStatusItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                             ULONG ulItemID, USHORT usNotifyCode,
                             ULONG ulExtra)      // for checkboxes: contains new state
{
    CHAR szTemp[200];

    switch (ulItemID)
    {
        // language drop-down box: load/unload resource modules
        case ID_XCDI_INFO_LANGUAGE:
        {
            if (usNotifyCode == LN_SELECT)
            {
                CHAR   szOldLanguageCode[LANGUAGECODELENGTH];
                // LONG   lTemp2 = 0;
                CHAR   szTemp2[10];
                PSZ    p;

                strcpy(szOldLanguageCode, cmnQueryLanguageCode());

                WinQueryDlgItemText(pcnbp->hwndDlgPage, ID_XCDI_INFO_LANGUAGE,
                                    sizeof(szTemp),
                                    szTemp);
                p = strhistr(szTemp, " -- XFLDR")+9; // my own case-insensitive routine
                if (p)
                {
                    strncpy(szTemp2, p, 3);
                    szTemp2[3] = '\0';
                    cmnSetLanguageCode(szTemp2);
                }

                // did language really change?
                if (strncmp(szOldLanguageCode, cmnQueryLanguageCode(), 3) != 0)
                {
                    // enforce reload of resource DLL
                    if (cmnQueryNLSModuleHandle(TRUE)    // reload flag
                             == NULLHANDLE)
                    {
                        // error occured loading the module: restore
                        //   old language
                        cmnSetLanguageCode(szOldLanguageCode);
                        // update display
                        pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
                    }
                    else
                    {
                        HWND      hwndSystemFrame,
                                  hwndCurrent = pcnbp->hwndDlgPage;
                        HWND      hwndDesktop
                                = WinQueryDesktopWindow(WinQueryAnchorBlock(HWND_DESKTOP),
                                                        NULLHANDLE);

                        // "closing system window"
                        cmnMessageBoxMsg(pcnbp->hwndFrame,
                                         102, 103,
                                         MB_OK);

                        // find frame window handle of "Workplace Shell" window
                        while ((hwndCurrent) && (hwndCurrent != hwndDesktop))
                        {
                            hwndSystemFrame = hwndCurrent;
                            hwndCurrent = WinQueryWindow(hwndCurrent, QW_PARENT);
                        }

                        if (hwndCurrent)
                            WinPostMsg(hwndSystemFrame,
                                       WM_SYSCOMMAND,
                                       (MPARAM)SC_CLOSE,
                                       MPFROM2SHORT(0, 0));
                    }
                } // end if (strcmp(szOldLanguageCode, szLanguageCode) != 0)
            } // end if (usNotifyCode == LN_SELECT)
        break; }
    }

    return ((MPARAM)-1);
}

/*
 *@@ setStatusTimer:
 *      notebook callback function (notebook.c) for the
 *      XWPSetup "Status" page.
 *      This gets called every two seconds to update
 *      variable data on the page.
 *
 *@@changed V0.9.1 (99-12-29) [umoeller]: added "WPS restarts" field
 *@@changed V0.9.4 (2000-07-03) [umoeller]: "WPS restarts" was 1 too large; fixed
 */

VOID setStatusTimer(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                    ULONG ulTimer)
{
    PCKERNELGLOBALS  pKernelGlobals = krnQueryGlobals();
    PTIB            ptib;
    PPIB            ppib;
    TID             tid;

    CHAR            szTemp[200];

    // awake WPS objects

    sprintf(szTemp, "%d", xthrQueryAwakeObjectsCount());
    WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XCDI_INFO_AWAKEOBJECTS,
                      szTemp);

    if (DosGetInfoBlocks(&ptib, &ppib) == NO_ERROR)
    {
        PQPROCSTAT16 pps = prc16GetInfo(NULL);
        PRCPROCESS       prcp;
        // WPS thread count
        prc16QueryProcessInfo(pps, doshMyPID(), &prcp);
        WinSetDlgItemShort(pcnbp->hwndDlgPage, ID_XCDI_INFO_WPSTHREADS,
                           prcp.usThreads,
                           FALSE);  // unsigned
        prc16FreeInfo(pps);
    }

    // XWPHook status
    {
        PXWPGLOBALSHARED pXwpGlobalShared = pKernelGlobals->pXwpGlobalShared;
        PSZ         psz = "Disabled";
        if (pXwpGlobalShared)
        {
            // WPS restarts V0.9.1 (99-12-29) [umoeller]
            WinSetDlgItemShort(pcnbp->hwndDlgPage, ID_XCDI_INFO_WPSRESTARTS,
                               pXwpGlobalShared->ulWPSStartupCount - 1,
                               FALSE);  // unsigned

            if (pXwpGlobalShared->fAllHooksInstalled)
                psz = "Loaded, OK";
            else
                psz = "Not loaded";
        }
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XCDI_INFO_HOOKSTATUS,
                          psz);

    }
}

/* ******************************************************************
 *                                                                  *
 *   XWPSetup "Objects" page notebook callbacks (notebook.c)        *
 *                                                                  *
 ********************************************************************/

/*
 *@@ setFindExistingObjects:
 *      this goes thru one of the STANDARDOBJECT arrays
 *      and checks all objects for their existence by
 *      setting each pExists member pointer to the object
 *      or NULL.
 *
 *@@added V0.9.4 (2000-07-15) [umoeller]
 */

VOID setFindExistingObjects(BOOL fStandardObj)      // in: if FALSE, XWorkplace objects;
                                                    // if TRUE, standard WPS objects
{
    PSTANDARDOBJECT pso2;
    ULONG ul, ulMax;

    if (fStandardObj)
    {
        // WPS objects:
        pso2 = G_WPSObjects;
        ulMax = sizeof(G_WPSObjects) / sizeof(STANDARDOBJECT);
    }
    else
    {
        // XWorkplace objects:
        pso2 = G_XWPObjects;
        ulMax = sizeof(G_XWPObjects) / sizeof(STANDARDOBJECT);
    }

    // go thru array
    for (ul = 0;
         ul < ulMax;
         ul++)
    {
        pso2->pExists = wpshQueryObjectFromID(*(pso2->ppcszDefaultID),
                                              NULL);        // pulErrorCode

        // next item
        pso2++;
    }
}

/*
 *@@ setCreateStandardObject:
 *      this creates a default WPS/XWP object from the
 *      given menu item ID, after displaying a confirmation
 *      box (XWPSetup "Obejcts" page).
 *      Returns TRUE if the menu ID was found in the given array.
 *
 *@@changed V0.9.1 (2000-02-01) [umoeller]: renamed prototype; added hwndOwner
 *@@changed V0.9.4 (2000-07-15) [umoeller]: now storing object pointer to disable menu item in time
 */

BOOL setCreateStandardObject(HWND hwndOwner,         // in: for dialogs
                             USHORT usMenuID,        // in: selected menu item
                             BOOL fStandardObj)      // in: if FALSE, XWorkplace object;
                                                     // if TRUE, standard WPS object
{
    BOOL    brc = FALSE;
    ULONG   ul = 0;

    PSTANDARDOBJECT pso2;
    ULONG ulMax;

    if (fStandardObj)
    {
        // WPS objects:
        pso2 = G_WPSObjects;
        ulMax = sizeof(G_WPSObjects) / sizeof(STANDARDOBJECT);
    }
    else
    {
        // XWorkplace objects:
        pso2 = G_XWPObjects;
        ulMax = sizeof(G_XWPObjects) / sizeof(STANDARDOBJECT);
    }

    // go thru array
    for (ul = 0;
         ul < ulMax;
         ul++)
    {
        if (pso2->usMenuID == usMenuID)
        {
            CHAR    szSetupString[2000];
            PSZ     apsz[2] = {  NULL,              // will be title
                                 szSetupString
                              };

            // get class's class object
            somId       somidThis = somIdFromString((PSZ)pso2->pcszObjectClass);
            SOMClass    *pClassObject = _somFindClass(SOMClassMgrObject, somidThis, 0, 0);

            sprintf(szSetupString, "%sOBJECTID=%s",
                    pso2->pcszSetupString,       // can be empty or ";"-terminated string
                    *(pso2->ppcszDefaultID));

            if (pClassObject)
                // get class's default title
                apsz[0] = _wpclsQueryTitle(pClassObject);

            if (apsz[0] == NULL)
                // title not found: use class name then
                apsz[0] = (PSZ)(*(pso2->pcszObjectClass));

            if (cmnMessageBoxMsgExt(hwndOwner,
                                    148, // "XWorkplace Setup",
                                    apsz,
                                    2,
                                    163,        // "create object?"
                                    MB_YESNO)
                         == MBID_YES)
            {
                HOBJECT hobj = WinCreateObject((PSZ)pso2->pcszObjectClass,       // class
                                               apsz[0], // pso2->pszObjectClass,       // title
                                               szSetupString,
                                               (PSZ)WPOBJID_DESKTOP,
                                               CO_FAILIFEXISTS);
                if (hobj)
                {
                    // success:
                    // store in array so the menu item will be
                    // disabled next time
                    pso2->pExists = _wpclsQueryObject(_WPObject,
                                                      hobj);

                    cmnMessageBoxMsg(hwndOwner,
                                     148, // "XWorkplace Setup",
                                     164, // "success"
                                     MB_OK);
                    brc = TRUE;
                }
                else
                    cmnMessageBoxMsg(hwndOwner,
                                     148, // "XWorkplace Setup",
                                     165, // "failed!"
                                     MB_OK);
            }

            SOMFree(somidThis);
            break;
        }
        // not found: try next item
        pso2++;
    }

    return (brc);
}

/*
 *@@ DisableObjectMenuItems:
 *      helper function for setObjectsItemChanged
 *      (XWPSetup "Objects" page) to disable items
 *      in the "Objects" menu buttons if objects
 *      exist already.
 *
 *@@changed V0.9.4 (2000-07-15) [umoeller]: now storing object pointer to disable menu item in time
 */

VOID DisableObjectMenuItems(HWND hwndMenu,          // in: button menu handle
                            PSTANDARDOBJECT pso,    // in: first menu array item
                            ULONG ulMax)            // in: size of menu array
{
    ULONG   ul = 0;
    PSTANDARDOBJECT pso2 = pso;

    for (ul = 0;
         ul < ulMax;
         ul++)
    {
        /* WPObject* pObject = wpshQueryObjectFromID(pso2->pszDefaultID,
                                                  NULL);        // pulErrorCode
                */
        XSTRING strMenuItemText;
        xstrInit(&strMenuItemText, 300);
        xstrset(&strMenuItemText, winhQueryMenuItemText(hwndMenu,
                                                        pso2->usMenuID));
        xstrcat(&strMenuItemText, " (", 0);
        xstrcat(&strMenuItemText, pso2->pcszObjectClass, 0);

        if (pso2->pExists)
        {
            // object found (exists already):
            // append the path to the menu item
            WPFolder    *pFolder = _wpQueryFolder(pso2->pExists);
            CHAR        szFolderPath[CCHMAXPATH] = "";
            _wpQueryFilename(pFolder, szFolderPath, TRUE);      // fully qualified
            xstrcat(&strMenuItemText, ", ", 0);
            xstrcat(&strMenuItemText, szFolderPath, 0);

            // disable menu item
            WinEnableMenuItem(hwndMenu, pso2->usMenuID, FALSE);
        }

        xstrcatc(&strMenuItemText, ')');
        if (pso2->pExists == NULL)
            // append "...", because we'll have a message box then
            xstrcat(&strMenuItemText, "...", 0);

        // on Warp 3, disable WarpCenter also
        if (   (!doshIsWarp4())
            && (strcmp(*(pso2->ppcszDefaultID), WPOBJID_WARPCENTER) == 0)
           )
            WinEnableMenuItem(hwndMenu, pso2->usMenuID, FALSE);

        WinSetMenuItemText(hwndMenu,
                           pso2->usMenuID,
                           strMenuItemText.psz);
        xstrClear(&strMenuItemText);

        pso2++;
    }
}

/*
 *@@ setObjectsInitPage:
 *      notebook callback function (notebook.c) for the
 *      XWPSetup "Objects" page.
 *      Sets the controls on the page according to the
 *      Global Settings.
 */

VOID setObjectsInitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                        ULONG flFlags)        // CBI_* flags (notebook.h)
{
    if (flFlags & CBI_INIT)
    {
        // collect all existing objects
        setFindExistingObjects(FALSE);  // XWorkplace objects
        setFindExistingObjects(TRUE);   // WPS objects

        ctlMakeMenuButton(WinWindowFromID(pcnbp->hwndDlgPage, ID_XCD_OBJECTS_SYSTEM),
                          0, 0);        // query for menu

        ctlMakeMenuButton(WinWindowFromID(pcnbp->hwndDlgPage, ID_XCD_OBJECTS_XWORKPLACE),
                          0, 0);
    }
}

/*
 *@@ setObjectsItemChanged:
 *      notebook callback function (notebook.c) for the
 *      XWPSetup "Objects" page.
 *      Reacts to changes of any of the dialog controls.
 */

MRESULT setObjectsItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                              ULONG ulItemID, USHORT usNotifyCode,
                              ULONG ulExtra)      // for checkboxes: contains new state
{
    MRESULT mrc = 0;

    switch (ulItemID)
    {
        /*
         * ID_XCD_OBJECTS_SYSTEM:
         *      system objects menu button
         */

        case ID_XCD_OBJECTS_SYSTEM:
        {
            HPOINTER hptrOld = winhSetWaitPointer();
            HWND    hwndMenu = WinLoadMenu(WinWindowFromID(pcnbp->hwndDlgPage, ulItemID),
                                           cmnQueryNLSModuleHandle(FALSE),
                                           ID_XSM_OBJECTS_SYSTEM);
            DisableObjectMenuItems(hwndMenu,
                                   &G_WPSObjects[0],
                                   sizeof(G_WPSObjects) / sizeof(STANDARDOBJECT)); // array item count
            mrc = (MRESULT)hwndMenu;
            WinSetPointer(HWND_DESKTOP, hptrOld);
        break; }

        /*
         * ID_XCD_OBJECTS_XWORKPLACE:
         *      XWorkplace objects menu button
         */

        case ID_XCD_OBJECTS_XWORKPLACE:
        {
            HPOINTER hptrOld = winhSetWaitPointer();
            HWND    hwndMenu = WinLoadMenu(WinWindowFromID(pcnbp->hwndDlgPage, ulItemID),
                                           cmnQueryNLSModuleHandle(FALSE),
                                           ID_XSM_OBJECTS_XWORKPLACE);
            DisableObjectMenuItems(hwndMenu,
                                   &G_XWPObjects[0],
                                   sizeof(G_XWPObjects) / sizeof(STANDARDOBJECT)); // array item count
            mrc = (MRESULT)hwndMenu;
            WinSetPointer(HWND_DESKTOP, hptrOld);
        break; }

        /*
         * ID_XCD_OBJECTS_CONFIGFOLDER:
         *      recreate config folder
         */

        case ID_XCD_OBJECTS_CONFIGFOLDER:
            if (cmnMessageBoxMsg(pcnbp->hwndFrame,
                                 148,       // XWorkplace Setup
                                 161,       // config folder?
                                 MB_YESNO)
                    == MBID_YES)
                xthrPostFileMsg(FIM_RECREATECONFIGFOLDER,
                                (MPARAM)RCF_DEFAULTCONFIGFOLDER,
                                MPNULL);
        break;

        /*
         * default:
         *      check for button menu item IDs
         */

        default:
            if (    (ulItemID >= OBJECTSIDFIRST)
                 && (ulItemID <= OBJECTSIDLAST)
               )
            {
                if (!setCreateStandardObject(pcnbp->hwndFrame,
                                             ulItemID,
                                             FALSE))
                    // WPS objects not found:
                    // try XWPS objects
                    setCreateStandardObject(pcnbp->hwndFrame,
                                            ulItemID,
                                            TRUE);
            }
    }

    return (mrc);
}

/* ******************************************************************
 *                                                                  *
 *   XWPSetup "Paranoia" page notebook callbacks (notebook.c)       *
 *                                                                  *
 ********************************************************************/

/*
 *@@ setParanoiaInitPage:
 *      notebook callback function (notebook.c) for the
 *      XWPSetup "Paranoia" page.
 *      Sets the controls on the page according to the
 *      Global Settings.
 *
 *@@changed V0.9.2 (2000-03-28) [umoeller]: added freaky menus setting
 */

VOID setParanoiaInitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                         ULONG flFlags)        // CBI_* flags (notebook.h)
{
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    if (flFlags & CBI_INIT)
    {
        if (pcnbp->pUser == NULL)
        {
            // first call: backup Global Settings for "Undo" button;
            // this memory will be freed automatically by the
            // common notebook window function (notebook.c) when
            // the notebook page is destroyed
            pcnbp->pUser = malloc(sizeof(GLOBALSETTINGS));
            memcpy(pcnbp->pUser, pGlobalSettings, sizeof(GLOBALSETTINGS));
        }

        // set up slider
        winhSetSliderTicks(WinWindowFromID(pcnbp->hwndDlgPage, ID_XCDI_WORKERPRTY_SLIDER),
                           (MPARAM)0, 6,
                           (MPARAM)-1, -1);
    }

    if (flFlags & CBI_SET)
    {
        // variable menu ID offset spin button
        winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_XCDI_VARMENUOFFSET,
                                                100, 2000,
                                                pGlobalSettings->VarMenuOffset);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XCDI_NOFREAKYMENUS,
                                               pGlobalSettings->fNoFreakyMenus);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XCDI_NOSUBCLASSING,
                                               pGlobalSettings->fNoSubclassing);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XCDI_NOWORKERTHREAD,
                                               pGlobalSettings->NoWorkerThread);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XCDI_USE8HELVFONT,
                                               pGlobalSettings->fUse8HelvFont);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XCDI_NOEXCPTBEEPS,
                                               pGlobalSettings->fNoExcptBeeps);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XCDI_WORKERPRTY_BEEP,
                                               pGlobalSettings->fWorkerPriorityBeep);

        winhSetSliderArmPosition(WinWindowFromID(pcnbp->hwndDlgPage, ID_XCDI_WORKERPRTY_SLIDER),
                                 SMA_INCREMENTVALUE,
                                 pGlobalSettings->bDefaultWorkerThreadPriority);
    }

    if (flFlags & CBI_ENABLE)
    {
        WinEnableControl(pcnbp->hwndDlgPage, ID_XCDI_WORKERPRTY_TEXT1,
                        !(pGlobalSettings->NoWorkerThread));
        WinEnableControl(pcnbp->hwndDlgPage, ID_XCDI_WORKERPRTY_SLIDER,
                        !(pGlobalSettings->NoWorkerThread));
        WinEnableControl(pcnbp->hwndDlgPage, ID_XCDI_WORKERPRTY_TEXT2,
                        !(pGlobalSettings->NoWorkerThread));
        WinEnableControl(pcnbp->hwndDlgPage, ID_XCDI_WORKERPRTY_BEEP,
                        !(pGlobalSettings->NoWorkerThread));
    }
}

/*
 *@@ setParanoiaItemChanged:
 *      notebook callback function (notebook.c) for the
 *      XWPSetup "Paranoia" page.
 *      Reacts to changes of any of the dialog controls.
 *
 *@@changed V0.9.2 (2000-03-28) [umoeller]: added freaky menus setting
 *@@changed V0.9.9 (2001-04-01) [pr]: fixed freaky menus undo
 */

MRESULT setParanoiaItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                               ULONG ulItemID, USHORT usNotifyCode,
                               ULONG ulExtra)      // for checkboxes: contains new state
{
    GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(__FILE__, __LINE__, __FUNCTION__);
    BOOL fSave = TRUE,
         fUpdateOtherPages = FALSE;

    switch (ulItemID)
    {
        case ID_XCDI_VARMENUOFFSET:
            pGlobalSettings->VarMenuOffset = ulExtra;
        break;

        case ID_XCDI_NOFREAKYMENUS:
            pGlobalSettings->fNoFreakyMenus   = ulExtra;
        break;

        case ID_XCDI_NOSUBCLASSING:
            pGlobalSettings->fNoSubclassing   = ulExtra;
            // set flag to iterate over other notebook pages
            fUpdateOtherPages = TRUE;
        break;

        case ID_XCDI_NOWORKERTHREAD:
            pGlobalSettings->NoWorkerThread  = ulExtra;
            // update the display by calling the INIT callback
            pcnbp->pfncbInitPage(pcnbp, CBI_ENABLE);
            // set flag to iterate over other notebook pages
            fUpdateOtherPages = TRUE;
        break;

        case ID_XCDI_USE8HELVFONT:
            pGlobalSettings->fUse8HelvFont  = ulExtra;
        break;

        case ID_XCDI_NOEXCPTBEEPS:
            pGlobalSettings->fNoExcptBeeps = ulExtra;
        break;

        case ID_XCDI_WORKERPRTY_SLIDER:
        {
            PSZ pszNewInfo = "error";

            LONG lSliderIndex = winhQuerySliderArmPosition(
                                            WinWindowFromID(pcnbp->hwndDlgPage, ID_XCDI_WORKERPRTY_SLIDER),
                                            SMA_INCREMENTVALUE);

            switch (lSliderIndex)
            {
                case 0:     pszNewInfo = "Idle ñ0"; break;
                case 1:     pszNewInfo = "Idle +31"; break;
                case 2:     pszNewInfo = "Regular ñ0"; break;
            }

            WinSetDlgItemText(pcnbp->hwndDlgPage,
                              ID_XCDI_WORKERPRTY_TEXT2,
                              pszNewInfo);

            if (lSliderIndex != pGlobalSettings->bDefaultWorkerThreadPriority)
            {
                // update the global settings
                pGlobalSettings->bDefaultWorkerThreadPriority = lSliderIndex;

                xthrResetWorkerThreadPriority();
            }
        break; }

        case ID_XCDI_WORKERPRTY_BEEP:
            pGlobalSettings->fWorkerPriorityBeep = ulExtra;
            break;

        case DID_UNDO:
        {
            // "Undo" button: get pointer to backed-up Global Settings
            PCGLOBALSETTINGS pGSBackup = (PCGLOBALSETTINGS)(pcnbp->pUser);

            // and restore the settings for this page
            pGlobalSettings->VarMenuOffset   = pGSBackup->VarMenuOffset;
            pGlobalSettings->fNoFreakyMenus   = pGSBackup->fNoFreakyMenus;
            pGlobalSettings->fNoSubclassing   = pGSBackup->fNoSubclassing;
            pGlobalSettings->NoWorkerThread  = pGSBackup->NoWorkerThread;
            pGlobalSettings->fUse8HelvFont   = pGSBackup->fUse8HelvFont;
            pGlobalSettings->fNoExcptBeeps    = pGSBackup->fNoExcptBeeps;
            pGlobalSettings->bDefaultWorkerThreadPriority
                                             = pGSBackup->bDefaultWorkerThreadPriority;
            pGlobalSettings->fWorkerPriorityBeep = pGSBackup->fWorkerPriorityBeep;

            // update the display by calling the INIT callback
            pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
            // set flag to iterate over other notebook pages
            fUpdateOtherPages = TRUE;
        break; }

        case DID_DEFAULT:
        {
            // set the default settings for this settings page
            // (this is in common.c because it's also used at
            // WPS startup)
            cmnSetDefaultSettings(pcnbp->ulPageID);
            // update the display by calling the INIT callback
            pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
            // set flag to iterate over other notebook pages
            fUpdateOtherPages = TRUE;
        break; }

        default:
            fSave = FALSE;
    }

    cmnUnlockGlobalSettings();

    if (fSave)
        cmnStoreGlobalSettings();

    if (fUpdateOtherPages)
    {
        PCREATENOTEBOOKPAGE pcnbp2 = NULL;
        // iterate over all currently open notebook pages
        while (pcnbp2 = ntbQueryOpenPages(pcnbp2))
        {
            if (pcnbp2->fPageVisible)
                if (pcnbp2->pfncbInitPage)
                    // enable/disable items on visible page
                    (*(pcnbp2->pfncbInitPage))(pcnbp2, CBI_ENABLE);
        }
    }

    return ((MPARAM)-1);
}


