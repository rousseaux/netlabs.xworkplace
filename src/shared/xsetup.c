
/*
 *@@sourcefile xsetup.c:
 *      this file contains the implementation of the XWPSetup
 *      class.
 *
 *@@header "shared\xsetup.h"
 */

/*
 *      Copyright (C) 1997-99 Ulrich M”ller.
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

#define INCL_DOSPROCESS
#define INCL_DOSMODULEMGR
#define INCL_DOSERRORS
#define INCL_WIN
#include <os2.h>

// C library headers
#include <stdio.h>
#include <locale.h>

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\cnrh.h"               // container helper routines
#include "helpers\comctl.h"             // common controls (window procs)
#include "helpers\winh.h"               // PM helper routines
#include "helpers\procstat.h"           // DosQProcStat handling
#include "helpers\stringh.h"            // string helper routines
#include "helpers\tmsgfile.h"           // "text message file" handling

// SOM headers which don't crash with prec. header files
#include "xwpsetup.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

#include "config\sound.h"               // XWPSound implementation

#include "filesys\xthreads.h"           // extra XWorkplace threads

#include "startshut\apm.h"              // APM power-off for XShutdown

#include "hook\xwphook.h"

// other SOM headers
#pragma hdrstop
#include <wpfsys.h>             // WPFileSystem

/* ******************************************************************
 *                                                                  *
 *   Globals                                                        *
 *                                                                  *
 ********************************************************************/

/*
 *@@ STANDARDOBJECT:
 *      structure used for XWPSetup "Objects" page.
 */

typedef struct _STANDARDOBJECT
{
    PSZ     pszDefaultID;           // e.g. &lt;WP_DRIVES&gt;
    PSZ     pszObjectClass;         // e.g. "WPDrives"
    PSZ     pszSetupString;         // e.g. "DETAILSFONT=..."; if present, _always_
                                    // put a semicolon at the end, because "OBJECTID=xxx"
                                    // will always be added
    USHORT  usMenuID;               // corresponding menu ID in resources
} STANDARDOBJECT, *PSTANDARDOBJECT;

#define OBJECTSIDFIRST 100      // first object menu ID, inclusive
#define OBJECTSIDLAST  213      // last object menu ID, inclusive

STANDARDOBJECT  WPSObjects[] = {
                                    "<WP_KEYB>", "WPKeyboard", "", 100,
                                    "<WP_MOUSE>", "WPMouse", "", 101,
                                    "<WP_CNTRY>", "WPCountry", "", 102,
                                    "<WP_SOUND>", "WPSound", "", 103,
                                    "<WP_POWER>", "WPPower", "", 104,
                                    "<WP_WINCFG>", "WPWinConfig", "", 105,

                                    "<WP_HIRESCLRPAL>", "WPColorPalette", "", 110,
                                    "<WP_LORESCLRPAL>", "WPColorPalette", "", 111,
                                    "<WP_FNTPAL>", "WPFontPalette", "", 112,
                                    "<WP_SCHPAL96>", "WPSchemePalette", "", 113,

                                    "<WP_LAUNCHPAD>", "WPLaunchPad", "", 120,
                                    "<WP_WARPCENTER>", "SmartCenter", "", 121,

                                    "<WP_SPOOL>", "WPSpool", "", 130,
                                    "<WP_VIEWER>", "WPMinWinViewer", "", 131,
                                    "<WP_SHRED>", "WPShredder", "", 132,
                                    "<WP_CLOCK>", "WPClock", "", 133,

                                    "<WP_START>", "WPStartup", "", 140,
                                    "<WP_TEMPS>", "WPTemplates", "", 141,
                                    "<WP_DRIVES>", "WPDrives", "", 142
                               },
                XWPObjects[] = {
                                    XFOLDER_WPSID, "XFldWPS", "", 200,
                                    XFOLDER_KERNELID, "XFldSystem", "", 201,
                                    XFOLDER_CLASSLISTID, "XWPClassList", "", 202,

                                    XFOLDER_CONFIGID, "WPFolder", "", 210,
                                    XFOLDER_STARTUPID, "XFldStartup", "", 211,
                                    XFOLDER_SHUTDOWNID, "XFldShutdown", "", 212,

                                    XFOLDER_TRASHCANID, "XWPTrashCan",
                                            "DETAILSCLASS=XWPTrashObject;"
                                            "SORTCLASS=XWPTrashObject;",
                                            213
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
 *@@changed 0.85 Language string now initialized to ""
 */

VOID AddResourceDLLToLB(HWND hwndDlg,
                        ULONG idLB,
                        PSZ pszXFolderBasePath,         // in: from cmnQueryXFolderBasePath
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
            fXFldStartup,
            fXFldShutdown,
            fXWPClassList,
            fXWPTrashCan;

    HWND    hwndTooltip;
    PSZ     pszTooltipString;
} XWPCLASSES, *PXWPCLASSES;

// array of tools to be subclassed for tooltips
USHORT usClassesToolIDs[] =
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

        DID_OK,
        DID_CANCEL
    };

/*
 * fnwpXWorkplaceClasses:
 *      dialog procedure for the "XWorkplace Classes" dialog.
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
            pxwpc->fXFldStartup = (winhQueryWPSClass(pObjClass, "XFldStartup") != 0);
            pxwpc->fXFldShutdown = (winhQueryWPSClass(pObjClass, "XFldShutdown") != 0);
            pxwpc->fXWPClassList = (winhQueryWPSClass(pObjClass, "XWPClassList") != 0);
            pxwpc->fXWPTrashCan = (     (winhQueryWPSClass(pObjClass, "XWPTrashCan") != 0)
                                     && (winhQueryWPSClass(pObjClass, "XWPTrashObject") != 0)
                                  );
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
            winhSetDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XFLDSTARTUP, pxwpc->fXFldStartup);
            winhSetDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XFLDSHUTDOWN, pxwpc->fXFldShutdown),
            winhSetDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XWPCLASSLIST, pxwpc->fXWPClassList);
            winhSetDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XWPTRASHCAN, pxwpc->fXWPTrashCan);

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
                TOOLINFO    ti;
                HWND        hwndCtl;
                ULONG       ul;
                ti.cbSize = sizeof(TOOLINFO);
                ti.uFlags = TTF_IDISHWND | TTF_CENTERTIP | TTF_SUBCLASS;
                ti.hwnd = hwndDlg;
                ti.hinst = NULLHANDLE;
                ti.lpszText = LPSTR_TEXTCALLBACK;  // send TTN_NEEDTEXT
                for (ul = 0;
                     ul < (sizeof(usClassesToolIDs) / sizeof(usClassesToolIDs[0]));
                     ul++)
                {
                    hwndCtl = WinWindowFromID(hwndDlg, usClassesToolIDs[ul]);
                    if (hwndCtl)
                    {
                        // add tool to tooltip control
                        ti.uId = hwndCtl;
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

            WinPostMsg(hwndDlg, WM_ENABLEITEMS, 0, 0);
        break; }

        /*
         * WM_ENABLEITEMS:
         *
         */

        case WM_ENABLEITEMS:
        {
            BOOL fXFolder = winhIsDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XFOLDER);
            BOOL fXFldDesktop = winhIsDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XFLDDESKTOP);

            WinEnableControl(hwndDlg, ID_XCDI_XWPCLS_XFLDDISK, fXFolder);
            winhSetDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XFLDDISK, fXFolder);

            WinEnableControl(hwndDlg, ID_XCDI_XWPCLS_XFLDSTARTUP, fXFolder);
            winhSetDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XFLDSTARTUP, fXFolder);

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
                PSZ             pszDereg = NULL,
                                pszReg = NULL,
                                pszReplace = NULL,
                                pszUnreplace = NULL;
                BOOL            fDereg = FALSE,
                                fReg = FALSE;

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
                pxwpcNew->fXFldStartup = winhIsDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XFLDSTARTUP);
                pxwpcNew->fXFldShutdown = winhIsDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XFLDSHUTDOWN);
                pxwpcNew->fXWPClassList = winhIsDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XWPCLASSLIST);
                pxwpcNew->fXWPTrashCan = winhIsDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XWPTRASHCAN);

                // compare old and new selections:
                // if we have a difference, add the class name
                // to the respective string to (de)register

                if ((pxwpcOld->fXFldObject) && (!pxwpcNew->fXFldObject))
                {
                    // deregister XFldObject
                    strhxcat(&pszDereg, "XFldObject\n");
                    // unreplace XFldObject
                    strhxcat(&pszUnreplace, "WPObject XFldObject\n");
                }
                else if ((pxwpcNew->fXFldObject) && (!pxwpcOld->fXFldObject))
                {
                    // register
                    strhxcat(&pszReg, "XFldObject\n");
                    // replace
                    strhxcat(&pszReplace, "WPObject XFldObject\n");
                }

                if ((pxwpcOld->fXFolder) && (!pxwpcNew->fXFolder))
                {
                    // deregister XFolder
                    strhxcat(&pszDereg, "XFolder\n");
                    // unreplace XFolder
                    strhxcat(&pszUnreplace, "WPFolder XFolder\n");
                }
                else if ((pxwpcNew->fXFolder) && (!pxwpcOld->fXFolder))
                {
                    // register
                    strhxcat(&pszReg, "XFolder\n");
                    // replace
                    strhxcat(&pszReplace, "WPFolder XFolder\n");
                }

                if ((pxwpcOld->fXFldDisk) && (!pxwpcNew->fXFldDisk))
                {
                    // deregister XFldDisk
                    strhxcat(&pszDereg, "XFldDisk\n");
                    // unreplace XFldDisk
                    strhxcat(&pszUnreplace, "WPDisk XFldDisk\n");
                }
                else if ((pxwpcNew->fXFldDisk) && (!pxwpcOld->fXFldDisk))
                {
                    // register
                    strhxcat(&pszReg, "XFldDisk\n");
                    // replace
                    strhxcat(&pszReplace, "WPDisk XFldDisk\n");
                }

                if ((pxwpcOld->fXFldDesktop) && (!pxwpcNew->fXFldDesktop))
                {
                    // deregister XFldDesktop
                    strhxcat(&pszDereg, "XFldDesktop\n");
                    // unreplace XFldDesktop
                    strhxcat(&pszUnreplace, "WPDesktop XFldDesktop\n");
                }
                else if ((pxwpcNew->fXFldDesktop) && (!pxwpcOld->fXFldDesktop))
                {
                    // register
                    strhxcat(&pszReg, "XFldDesktop\n");
                    // replace
                    strhxcat(&pszReplace, "WPDesktop XFldDesktop\n");
                }

                if ((pxwpcOld->fXFldDataFile) && (!pxwpcNew->fXFldDataFile))
                {
                    // deregister XFldDataFile
                    strhxcat(&pszDereg, "XFldDataFile\n");
                    // unreplace XFldDataFile
                    strhxcat(&pszUnreplace, "WPDataFile XFldDataFile\n");
                }
                else if ((pxwpcNew->fXFldDataFile) && (!pxwpcOld->fXFldDataFile))
                {
                    // register
                    strhxcat(&pszReg, "XFldDataFile\n");
                    // replace
                    strhxcat(&pszReplace, "WPDataFile XFldDataFile\n");
                }

                if ((pxwpcOld->fXFldProgramFile) && (!pxwpcNew->fXFldProgramFile))
                {
                    // deregister XFldProgramFile
                    strhxcat(&pszDereg, "XFldProgramFile\n");
                    // unreplace XFldProgramFile
                    strhxcat(&pszUnreplace, "WPProgramFile XFldProgramFile\n");
                }
                else if ((pxwpcNew->fXFldProgramFile) && (!pxwpcOld->fXFldProgramFile))
                {
                    // register
                    strhxcat(&pszReg, "XFldProgramFile\n");
                    // replace
                    strhxcat(&pszReplace, "WPProgramFile XFldProgramFile\n");
                }

                if ((pxwpcOld->fXWPSound) && (!pxwpcNew->fXWPSound))
                {
                    // deregister XWPSound
                    strhxcat(&pszDereg, "XWPSound\n");
                    // unreplace XWPSound
                    strhxcat(&pszUnreplace, "WPSound XWPSound\n");
                }
                else if ((pxwpcNew->fXWPSound) && (!pxwpcOld->fXWPSound))
                {
                    // register
                    strhxcat(&pszReg, "XWPSound\n");
                    // replace
                    strhxcat(&pszReplace, "WPSound XWPSound\n");
                }

                if ((pxwpcOld->fXWPMouse) && (!pxwpcNew->fXWPMouse))
                {
                    // deregister XWPMouse
                    strhxcat(&pszDereg, "XWPMouse\n");
                    // unreplace XWPMouse
                    strhxcat(&pszUnreplace, "WPMouse XWPMouse\n");
                }
                else if ((pxwpcNew->fXWPMouse) && (!pxwpcOld->fXWPMouse))
                {
                    // register
                    strhxcat(&pszReg, "XWPMouse\n");
                    // replace
                    strhxcat(&pszReplace, "WPMouse XWPMouse\n");
                }

                if ((pxwpcOld->fXWPKeyboard) && (!pxwpcNew->fXWPKeyboard))
                {
                    // deregister XWPKeyboard
                    strhxcat(&pszDereg, "XWPKeyboard\n");
                    // unreplace XWPKeyboard
                    strhxcat(&pszUnreplace, "WPKeyboard XWPKeyboard\n");
                }
                else if ((pxwpcNew->fXWPKeyboard) && (!pxwpcOld->fXWPKeyboard))
                {
                    // register
                    strhxcat(&pszReg, "XWPKeyboard\n");
                    // replace
                    strhxcat(&pszReplace, "WPKeyboard XWPKeyboard\n");
                }

                /*
                 * new classes (no replacements):
                 *
                 */

                if ((pxwpcOld->fXWPSetup) && (!pxwpcNew->fXWPSetup))
                    // deregister XWPSetup
                    strhxcat(&pszDereg, "XWPSetup\n");
                else if ((pxwpcNew->fXWPSetup) && (!pxwpcOld->fXWPSetup))
                    // register
                    strhxcat(&pszReg, "XWPSetup\n");

                if ((pxwpcOld->fXFldSystem) && (!pxwpcNew->fXFldSystem))
                    // deregister XFldSystem
                    strhxcat(&pszDereg, "XFldSystem\n");
                else if ((pxwpcNew->fXFldSystem) && (!pxwpcOld->fXFldSystem))
                    // register
                    strhxcat(&pszReg, "XFldSystem\n");

                if ((pxwpcOld->fXFldWPS) && (!pxwpcNew->fXFldWPS))
                    // deregister XFldWPS
                    strhxcat(&pszDereg, "XFldWPS\n");
                else if ((pxwpcNew->fXFldWPS) && (!pxwpcOld->fXFldWPS))
                    // register
                    strhxcat(&pszReg, "XFldWPS\n");

                if ((pxwpcOld->fXFldStartup) && (!pxwpcNew->fXFldStartup))
                    // deregister XFldStartup
                    strhxcat(&pszDereg, "XFldStartup\n");
                else if ((pxwpcNew->fXFldStartup) && (!pxwpcOld->fXFldStartup))
                    // register
                    strhxcat(&pszReg, "XFldStartup\n");

                if ((pxwpcOld->fXFldShutdown) && (!pxwpcNew->fXFldShutdown))
                    // deregister XFldShutdown
                    strhxcat(&pszDereg, "XFldShutdown\n");
                else if ((pxwpcNew->fXFldShutdown) && (!pxwpcOld->fXFldShutdown))
                    // register
                    strhxcat(&pszReg, "XFldShutdown\n");

                if ((pxwpcOld->fXWPClassList) && (!pxwpcNew->fXWPClassList))
                    // deregister XWPClassList
                    strhxcat(&pszDereg, "XWPClassList\n");
                else if ((pxwpcNew->fXWPClassList) && (!pxwpcOld->fXWPClassList))
                    // register
                    strhxcat(&pszReg, "XWPClassList\n");

                if ((pxwpcOld->fXWPTrashCan) && (!pxwpcNew->fXWPTrashCan))
                {
                    // deregister XWPTrashCan
                    strhxcat(&pszDereg, "XWPTrashCan\n");
                    // deregister XWPTrashObject
                    strhxcat(&pszDereg, "XWPTrashObject\n");
                }
                else if ((pxwpcNew->fXWPTrashCan) && (!pxwpcOld->fXWPTrashCan))
                {
                    strhxcat(&pszReg, "XWPTrashCan\n");
                    strhxcat(&pszReg, "XWPTrashObject\n");
                }

                // check if we have anything to do
                fReg = (pszReg != NULL);
                fDereg = (pszDereg != NULL);

                if ((fReg) || (fDereg))
                {
                    // OK, class selections have changed:
                    PSZ             pszMessage = NULL;
                    CHAR            szTemp[1000];

                    // compose confirmation string
                    cmnGetMessage(NULL, 0, szTemp, sizeof(szTemp),
                                  142); // "You have made the following changes to the XWorkplace class setup:"
                    strhxcpy(&pszMessage, szTemp);
                    strhxcat(&pszMessage, "\n");
                    if (fReg)
                    {
                        strhxcat(&pszMessage, "\n");
                        cmnGetMessage(NULL, 0, szTemp, sizeof(szTemp),
                                      143); // "Register the following classes:"
                        strhxcat(&pszMessage, szTemp);
                        strhxcat(&pszMessage, "\n\n");
                        strhxcat(&pszMessage, pszReg);
                        if (pszReplace)
                        {
                            strhxcat(&pszMessage, "\n");
                            cmnGetMessage(NULL, 0, szTemp, sizeof(szTemp),
                                          144); // "Replace the following classes:"
                            strhxcat(&pszMessage, szTemp);
                            strhxcat(&pszMessage, "\n\n");
                            strhxcat(&pszMessage, pszReplace);
                        }
                    }
                    if (fDereg)
                    {
                        strhxcat(&pszMessage, "\n");
                        cmnGetMessage(NULL, 0, szTemp, sizeof(szTemp),
                                      145); // "Deregister the following classes:"
                        strhxcat(&pszMessage, szTemp);
                        strhxcat(&pszMessage, "\n\n");
                        strhxcat(&pszMessage, pszDereg);
                        if (pszUnreplace)
                        {
                            strhxcat(&pszMessage, "\n");
                            cmnGetMessage(NULL, 0, szTemp, sizeof(szTemp),
                                          146); // "Undo the following class replacements:"
                            strhxcat(&pszMessage, szTemp);
                            strhxcat(&pszMessage, "\n\n");
                            strhxcat(&pszMessage, pszUnreplace);
                        }
                    }
                    strhxcat(&pszMessage, "\n");
                    cmnGetMessage(NULL, 0, szTemp, sizeof(szTemp),
                                  147); // "Are you sure you want to do this?"
                    strhxcat(&pszMessage, szTemp);

                    // confirm class list changes
                    if (cmnMessageBox(hwndDlg,
                                      "XWorkplace Setup",
                                      pszMessage,
                                      MB_YESNO) == MBID_YES)
                    {
                        // "Yes" pressed: go!!
                        PSZ         p = NULL,
                                    pszFailing = NULL;
                        HPOINTER    hptrOld = winhSetWaitPointer();

                        // unreplace classes
                        p = pszUnreplace;
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
                                            strhxcat(&pszFailing, pszReplacedClass);
                                            strhxcat(&pszFailing, "\n");
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
                        p = pszDereg;
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
                                            strhxcat(&pszFailing, pszClass);
                                            strhxcat(&pszFailing, "\n");
                                        }
                                        free(pszClass);
                                    }
                                }
                                else
                                    break;

                                p = pEOL + 1;
                            }

                        // register new classes
                        p = pszReg;
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
                                            strhxcat(&pszFailing, pszClass);
                                            strhxcat(&pszFailing, "\n");
                                        }
                                        free(pszClass);
                                    }
                                }
                                else
                                    break;

                                p = pEOL + 1;
                            }

                        // replace classes
                        p = pszReplace;
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
                                            strhxcat(&pszFailing, pszReplacedClass);
                                            strhxcat(&pszFailing, "\n");
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
                        if (pszFailing)
                        {
                            cmnMessageBoxMsgExt(hwndDlg,
                                                148, // "XWorkplace Setup",
                                                &pszFailing, 1,
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

                    free(pszMessage);
                    if (pszReg)
                        free(pszReg);
                    if (pszDereg)
                        free(pszDereg);
                }
            }

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
                    PXWPCLASSES     pxwpcOld = WinQueryWindowPtr(hwndDlg, QWL_USER),
                                    pxwpcNew = pxwpcOld + 1;
                    PTOOLTIPTEXT pttt = (PTOOLTIPTEXT)mp2;
                    CHAR    szMessageID[200];
                    CHAR    szHelpString[3000];
                    const char *pszMessageFile = cmnQueryMessageFile();
                    ULONG   ulID = WinQueryWindowUShort(pttt->hdr.idFrom,
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
                                              pttt->hdr.idFrom,
                                              ulID,
                                              arc);
                    }

                    pxwpcOld->pszTooltipString = strdup(szHelpString);

                    pttt->lpszText = pxwpcOld->pszTooltipString;
                }
            }
            else if (usNotifyCode == BN_CLICKED)
            {
                switch (usItemID)      // usID
                {
                    // "XFolder" item checked/unchecked: update others
                    case ID_XCDI_XWPCLS_XFOLDER:
                    case ID_XCDI_XWPCLS_XFLDDESKTOP:
                        WinPostMsg(hwndDlg, WM_ENABLEITEMS, 0, 0);
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
 *                                                                  *
 *   XWPSetup "Status" page notebook callbacks (notebook.c)         *
 *                                                                  *
 ********************************************************************/

/*
 *@@ setStatusInitPage:
 *      notebook callback function (notebook.c) for the
 *      XWPSetup "Status" page.
 *      Sets the controls on the page according to the
 *      Global Settings.
 *
 *@@todo: language drop-down box
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
            pMsg += sprintf(pMsg, "pXFldStartup: 0x%lX\n", pKernelGlobals->pXFldStartup);
            pMsg += sprintf(pMsg, "pXFldShutdown: 0x%lX\n", pKernelGlobals->pXFldShutdown);
            pMsg += sprintf(pMsg, "pXWPClassList: 0x%lX\n", pKernelGlobals->pXWPClassList);

            DebugBox("XWorkplace Class Objects", szMsg);
        #endif
    }

    if (flFlags & CBI_SET)
    {
        HMODULE         hmodNLS = cmnQueryNLSModuleHandle(FALSE);
        CHAR            szXFolderBasePath[CCHMAXPATH],
                        szSearchMask[2*CCHMAXPATH];
        HDIR            hdirFindHandle = HDIR_SYSTEM;
        FILEFINDBUF3    FindBuffer     = {0};      // Returned from FindFirst/Next
        ULONG           ulResultBufLen = sizeof(FILEFINDBUF3);
        ULONG           ulFindCount    = 1;        // Look for 1 file at a time
        APIRET          rc             = NO_ERROR; // Return code

        PCKERNELGLOBALS  pKernelGlobals = krnQueryGlobals();

        HAB             hab = WinQueryAnchorBlock(pcnbp->hwndPage);

        // kernel version number
        WinSetDlgItemText(pcnbp->hwndPage, ID_XCDI_INFO_KERNEL_RELEASE,
                          XFOLDER_VERSION);

        // kernel build (automatically increased with each build
        // by xfldr.mak; string resource in xfldr.rc)
        if (WinLoadString(hab,
                    cmnQueryMainModuleHandle(),       // main module (XFLDR.DLL)
                    ID_XSSI_KERNEL_BUILD,
                    sizeof(szSearchMask), szSearchMask))
            sprintf(szSearchMask+strlen(szSearchMask), " (%s)", __DATE__);
        WinSetDlgItemText(pcnbp->hwndPage, ID_XCDI_INFO_KERNEL_BUILD,
                        szSearchMask);

        // C runtime locale
        /* WinSetDlgItemText(pcnbp->hwndPage, ID_XCDI_INFO_KERNEL_LOCALE,
                        // query the locale
                        cmnQueryLocale()); */

        /* sprintf(szText, "Last Workplace Shell startup: %02d:%02d:%02d",
                            pKernelGlobals->StartupDateTime.hours,
                            pKernelGlobals->StartupDateTime.minutes,
                            pKernelGlobals->StartupDateTime.seconds);
        AddObjectUsage2Cnr(hwndCnr, preccLevel2, szText,
                            CRA_RECORDREADONLY); */

        // sound status
        strcpy(szSearchMask,
                        (pKernelGlobals->ulMMPM2Working == MMSTAT_UNKNOWN)
                                ? "not initialized"
                        : (pKernelGlobals->ulMMPM2Working == MMSTAT_WORKING)
                                ? "OK"
                        : (pKernelGlobals->ulMMPM2Working == MMSTAT_MMDIRNOTFOUND)
                                ? "MMPM/2 directory not found"
                        : (pKernelGlobals->ulMMPM2Working == MMSTAT_SOUNDLLNOTFOUND)
                                ? "SOUND.DLL not found"
                        : (pKernelGlobals->ulMMPM2Working == MMSTAT_SOUNDLLNOTLOADED)
                                ? "SOUND.DLL could not be loaded"
                        : (pKernelGlobals->ulMMPM2Working == MMSTAT_SOUNDLLFUNCERROR)
                                ? "SOUND.DLL functions could not be imported"
                        : (pKernelGlobals->ulMMPM2Working == MMSTAT_CRASHED)
                                ? "Quick thread crashed"
                        : "unknown"
               );
        WinSetDlgItemText(pcnbp->hwndPage, ID_XCDI_INFO_SOUNDSTATUS,
                          szSearchMask);

        // language drop-down box
        WinSendDlgItemMsg(pcnbp->hwndPage, ID_XCDI_INFO_LANGUAGE, LM_DELETEALL, 0, 0);

        if (cmnQueryXFolderBasePath(szXFolderBasePath))
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
                DebugBox("XFolder", "XFolder was unable to find any National Language Support DLLs. You need to re-install XFolder.");
            else
            {
                // no error:
                #ifdef DEBUG_LANGCODES
                    _Pmpf(("  Found file: %s", FindBuffer.achName));
                #endif
                AddResourceDLLToLB(pcnbp->hwndPage,
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
                        AddResourceDLLToLB(pcnbp->hwndPage,
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
                    DebugBox("XFolder", "DosFindClose error");

                #ifdef DEBUG_LANGCODES
                    _Pmpf(("  Selecting: %s", cmnQueryLanguageCode()));
                #endif
                WinSendDlgItemMsg(pcnbp->hwndPage, ID_XCDI_INFO_LANGUAGE,
                                  LM_SELECTITEM,
                                  WinSendDlgItemMsg(pcnbp->hwndPage,
                                                    ID_XCDI_INFO_LANGUAGE, // find matching item
                                                    LM_SEARCHSTRING,
                                                    MPFROM2SHORT(LSS_SUBSTRING, LIT_FIRST),
                                                    (MPARAM)cmnQueryLanguageCode()),
                                  (MPARAM)TRUE); // select
            }
        } // end if (cmnQueryXFolderBasePath...

        // NLS info
        if (WinLoadString(hab, hmodNLS, ID_XSSI_XFOLDERVERSION,
                    sizeof(szSearchMask), szSearchMask))
            WinSetDlgItemText(pcnbp->hwndPage, ID_XCDI_INFO_NLS_RELEASE,
                    szSearchMask);

        if (WinLoadString(hab, hmodNLS, ID_XSSI_NLS_AUTHOR,
                    sizeof(szSearchMask), szSearchMask))
            WinSetDlgItemText(pcnbp->hwndPage, ID_XCDI_INFO_NLS_AUTHOR,
                    szSearchMask);
    } // end if (flFlags & CBI_SET)

    if (flFlags & CBI_ENABLE)
    {
    }
}

/*
 *@@ setStatusItemChanged:
 *      notebook callback function (notebook.c) for the
 *      XWPSetup "Status" page.
 *      Reacts to changes of any of the dialog controls.
 */

MRESULT setStatusItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                             USHORT usItemID, USHORT usNotifyCode,
                             ULONG ulExtra)      // for checkboxes: contains new state
{
    CHAR szTemp[200];

    switch (usItemID)
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

                WinQueryDlgItemText(pcnbp->hwndPage, ID_XCDI_INFO_LANGUAGE,
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
                        (*(pcnbp->pfncbInitPage))(pcnbp, CBI_SET | CBI_ENABLE);
                    }
                    else
                    {
                        HWND      hwndSystemFrame,
                                  hwndCurrent = pcnbp->hwndPage;
                        HWND      hwndDesktop = WinQueryDesktopWindow(
                                    WinQueryAnchorBlock(HWND_DESKTOP), NULLHANDLE);

                        // "closing system window"
                        cmnMessageBoxMsg(pcnbp->hwndPage,
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
    sprintf(szTemp, "%d", pKernelGlobals->lAwakeObjectsCount);
    WinSetDlgItemText(pcnbp->hwndPage, ID_XCDI_INFO_AWAKEOBJECTS,
                    szTemp);

    if (DosGetInfoBlocks(&ptib, &ppib) == NO_ERROR)
    {
        PRCPROCESS       prcp;
        // WPS thread count
        prcQueryProcessInfo(ppib->pib_ulpid, &prcp);
        WinSetDlgItemShort(pcnbp->hwndPage, ID_XCDI_INFO_WPSTHREADS,
                           prcp.usThreads,
                           FALSE);  // unsigned

        // Worker thread status
        tid = thrQueryID(pKernelGlobals->ptiWorkerThread);
        sprintf(szTemp, "TID 0x%lX, prty 0x%04lX, %d msgs",
                tid,
                prcQueryThreadPriority(ppib->pib_ulpid, tid),
                pKernelGlobals->ulWorkerMsgCount);
        WinSetDlgItemText(pcnbp->hwndPage, ID_XCDI_INFO_WORKERSTATUS,
                        szTemp);

        // File thread status
        tid = thrQueryID(pKernelGlobals->ptiFileThread);
        sprintf(szTemp, "TID 0x%lX, prty 0x%04lX",
                tid,
                prcQueryThreadPriority(ppib->pib_ulpid, tid));
        WinSetDlgItemText(pcnbp->hwndPage, ID_XCDI_INFO_FILESTATUS,
                        szTemp);

        // Quick thread status
        tid = thrQueryID(pKernelGlobals->ptiQuickThread);
        sprintf(szTemp, "TID 0x%lX, prty 0x%04lX",
                tid,
                prcQueryThreadPriority(ppib->pib_ulpid, tid));
        WinSetDlgItemText(pcnbp->hwndPage, ID_XCDI_INFO_QUICKSTATUS,
                        szTemp);
    }

    // XWPHook status
    {
        PDAEMONSHARED pDaemonShared = pKernelGlobals->pDaemonShared;
        PSZ         psz = "Disabled";
        if (pDaemonShared)
        {
            // WPS restarts V0.9.1 (99-12-29) [umoeller]
            WinSetDlgItemShort(pcnbp->hwndPage, ID_XCDI_INFO_WPSRESTARTS,
                               pDaemonShared->ulWPSStartupCount,
                               FALSE);  // unsigned

            if (pDaemonShared->fHookInstalled)
                psz = "Loaded, OK";
            else
                psz = "Not loaded";
        }
        WinSetDlgItemText(pcnbp->hwndPage, ID_XCDI_INFO_HOOKSTATUS,
                          psz);

    }
}

/* ******************************************************************
 *                                                                  *
 *   XWPSetup "Features" page notebook callbacks (notebook.c)       *
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
 *      array of FEATURESITEM which are inserted into
 *      the container on the "Features" page.
 *
 *added V0.9.1 (99-12-19) [umoeller]
 */

FEATURESITEM FeatureItemsList[] =
        {
            ID_XCSI_GENERALFEATURES, 0, 0, NULL,
            ID_XCSI_REPLACEICONS, ID_XCSI_GENERALFEATURES, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,
            ID_XCSI_ADDOBJECTPAGE, ID_XCSI_GENERALFEATURES, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,
            ID_XCSI_REPLACEFILEPAGE, ID_XCSI_GENERALFEATURES, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,
            ID_XCSI_XSYSTEMSOUNDS, ID_XCSI_GENERALFEATURES, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,

            ID_XCSI_FOLDERFEATURES, 0, 0, NULL,
            ID_XCSI_ENABLESTATUSBARS, ID_XCSI_FOLDERFEATURES, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,
            ID_XCSI_ENABLESNAP2GRID, ID_XCSI_FOLDERFEATURES, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,
            ID_XCSI_ENABLEFOLDERHOTKEYS, ID_XCSI_FOLDERFEATURES, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,
            ID_XCSI_EXTFOLDERSORT, ID_XCSI_FOLDERFEATURES, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,

            ID_XCSI_MOUSEKEYBOARDFEATURES, 0, 0, NULL,
            ID_XCSI_ANIMOUSE, ID_XCSI_MOUSEKEYBOARDFEATURES, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,
            ID_XCSI_XWPHOOK, ID_XCSI_MOUSEKEYBOARDFEATURES, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,
            ID_XCSI_NUMLOCKON, ID_XCSI_MOUSEKEYBOARDFEATURES, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,

            ID_XCSI_STARTSHUTFEATURES, 0, 0, NULL,
            ID_XCSI_ARCHIVING, ID_XCSI_STARTSHUTFEATURES, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,
            ID_XCSI_RESTARTWPS, ID_XCSI_STARTSHUTFEATURES, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,
            ID_XCSI_XSHUTDOWN, ID_XCSI_STARTSHUTFEATURES, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,

            ID_XCSI_FILEOPERATIONS, 0, 0, NULL,
            ID_XCSI_EXTASSOCS, ID_XCSI_FILEOPERATIONS, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,
            ID_XCSI_CLEANUPINIS, ID_XCSI_FILEOPERATIONS, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,
            ID_XCSI_REPLFILEEXISTS, ID_XCSI_FILEOPERATIONS, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,
            ID_XCSI_REPLDRIVENOTREADY, ID_XCSI_FILEOPERATIONS, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,
            ID_XCSI_DELETEINTOTRASHCAN, ID_XCSI_FILEOPERATIONS, WS_VISIBLE | BS_AUTOCHECKBOX, NULL
        };

PCHECKBOXRECORDCORE pFeatureRecordsList = NULL;

/*
 *@@ setFeaturesInitPage:
 *      notebook callback function (notebook.c) for the
 *      XWPSetup "File Operations" page.
 *      Sets the controls on the page according to the
 *      Global Settings.
 */

VOID setFeaturesInitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                         ULONG flFlags)        // CBI_* flags (notebook.h)
{
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    PCKERNELGLOBALS  pKernelGlobals = krnQueryGlobals();

    HWND hwndFeaturesCnr = WinWindowFromID(pcnbp->hwndPage,
                                           ID_XCDI_CONTAINER);

    if (flFlags & CBI_INIT)
    {
        PCHECKBOXRECORDCORE preccThis,
                            preccParent;
        ULONG               ul,
                            cRecords;
        HAB                 hab = WinQueryAnchorBlock(pcnbp->hwndPage);
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

        if (ctlMakeCheckboxContainer(pcnbp->hwndPage,
                                     ID_XCDI_CONTAINER))
        {
            cRecords = sizeof(FeatureItemsList) / sizeof(FEATURESITEM);

            pFeatureRecordsList
                = (PCHECKBOXRECORDCORE)cnrhAllocRecords(hwndFeaturesCnr,
                                                        sizeof(CHECKBOXRECORDCORE),
                                                        cRecords);
            // insert feature records:
            // start for-each-record loop
            preccThis = pFeatureRecordsList;
            ul = 0;
            while (preccThis)
            {
                // load NLS string for feature
                cmnLoadString(hab,
                              hmodNLS,
                              FeatureItemsList[ul].usFeatureID, // in: string ID
                              &(FeatureItemsList[ul].pszNLSString)); // out: NLS string

                // copy FEATURESITEM to record core
                preccThis->ulStyle = FeatureItemsList[ul].ulStyle;
                preccThis->usItemID = FeatureItemsList[ul].usFeatureID;
                preccThis->usCheckState = 0;        // unchecked
                preccThis->recc.pszTree = FeatureItemsList[ul].pszNLSString;

                preccParent = NULL;

                // find parent record if != 0
                if (FeatureItemsList[ul].usParentID)
                {
                    // parent specified:
                    // search records we have prepared so far
                    ULONG ul2 = 0;
                    PCHECKBOXRECORDCORE preccThis2 = pFeatureRecordsList;
                    for (ul2 = 0; ul2 < ul; ul2++)
                    {
                        if (preccThis2->usItemID == FeatureItemsList[ul].usParentID)
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
                                  NULL,
                                  CRA_RECORDREADONLY,
                                  1);

                // next record
                preccThis = (PCHECKBOXRECORDCORE)preccThis->recc.preccNextRecord;
                ul++;
            }
        } // end if (ctlMakeCheckboxContainer(pcnbp->hwndPage,

        // register tooltip class
        if (ctlRegisterTooltip(WinQueryAnchorBlock(pcnbp->hwndPage)))
        {
            // create tooltip
            pcnbp->hwndTooltip = WinCreateWindow(HWND_DESKTOP,  // parent
                                                 COMCTL_TOOLTIP_CLASS, // wnd class
                                                 "",            // window text
                                                 XWP_TOOLTIP_STYLE,
                                                      // tooltip window style (common.h)
                                                 10, 10, 10, 10,    // window pos and size, ignored
                                                 pcnbp->hwndPage, // owner window -- important!
                                                 HWND_TOP,      // hwndInsertBehind, ignored
                                                 DID_TOOLTIP, // window ID, optional
                                                 NULL,          // control data
                                                 NULL);         // presparams

            if (pcnbp->hwndTooltip)
            {
                // tooltip successfully created:
                // add tools (i.e. controls of the dialog)
                // according to the usToolIDs array
                TOOLINFO    ti;
                ti.cbSize = sizeof(TOOLINFO);
                ti.uFlags = TTF_IDISHWND | /* TTF_CENTERTIP | */ TTF_SUBCLASS;
                ti.hwnd = pcnbp->hwndPage;
                ti.hinst = NULLHANDLE;
                ti.lpszText = LPSTR_TEXTCALLBACK;  // send TTN_NEEDTEXT
                // add cnr as tool to tooltip control
                ti.uId = hwndFeaturesCnr;
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
        ctlSetRecordChecked(hwndFeaturesCnr, ID_XCSI_NUMLOCKON,
                pGlobalSettings->fNumLockStartup);

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
        ctlSetRecordChecked(hwndFeaturesCnr, ID_XCSI_REPLFILEEXISTS,
                pGlobalSettings->fReplFileExists);
        ctlSetRecordChecked(hwndFeaturesCnr, ID_XCSI_REPLDRIVENOTREADY,
                pGlobalSettings->fReplDriveNotReady);
        ctlSetRecordChecked(hwndFeaturesCnr, ID_XCSI_DELETEINTOTRASHCAN,
                pGlobalSettings->fTrashDelete);
    }

    if (flFlags & CBI_ENABLE)
    {
        PDAEMONSHARED pDaemonShared = pKernelGlobals->pDaemonShared;

        ctlEnableRecord(hwndFeaturesCnr, ID_XCSI_XWPHOOK,
                (    (pDaemonShared->hwndDaemonObject)
                  && (pDaemonShared->hwndDaemonObject)
                ));

        ctlEnableRecord(hwndFeaturesCnr, ID_XCSI_REPLACEFILEPAGE,
                (pKernelGlobals->fXFolder)
                || (pKernelGlobals->fXFldDataFile));
        ctlEnableRecord(hwndFeaturesCnr, ID_XCSI_ENABLESTATUSBARS,
                (pKernelGlobals->fXFolder));
        ctlEnableRecord(hwndFeaturesCnr, ID_XCSI_ENABLESNAP2GRID,
                (pKernelGlobals->fXFolder));
        ctlEnableRecord(hwndFeaturesCnr, ID_XCSI_ENABLEFOLDERHOTKEYS,
                (pKernelGlobals->fXFolder));
        ctlEnableRecord(hwndFeaturesCnr, ID_XCSI_EXTFOLDERSORT,
                (pKernelGlobals->fXFolder));

        ctlEnableRecord(hwndFeaturesCnr, ID_XCSI_XSYSTEMSOUNDS,
                ( (pKernelGlobals->fXFolder) || (pKernelGlobals->fXFldDesktop) ));
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

        ctlEnableRecord(hwndFeaturesCnr, ID_XCSI_DELETEINTOTRASHCAN,
                wpshQueryObjectFromID(XFOLDER_TRASHCANID, NULL) != NULL);
    }
}

/*
 *@@ setFeaturesChanged:
 *      notebook callback function (notebook.c) for the
 *      XWPSetup "File Operations" page.
 *      Reacts to changes of any of the dialog controls.
 */

MRESULT setFeaturesItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                               USHORT usItemID, USHORT usNotifyCode,
                               ULONG ulExtra)      // for checkboxes: contains new state
{
    GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(5000);
    BOOL fSave = TRUE;
    BOOL fShowHookInstalled = FALSE,
         fShowHookDeinstalled = FALSE,
         fShowClassesSetup = FALSE;

    switch (usItemID)
    {
        case ID_XCSI_REPLACEICONS:
            pGlobalSettings->fReplaceIcons = ulExtra;
        break;

        case ID_XCSI_ADDOBJECTPAGE:
            pGlobalSettings->AddObjectPage = ulExtra;
        break;

        case ID_XCSI_XWPHOOK:
        {
            PCKERNELGLOBALS  pKernelGlobals = krnQueryGlobals();
            PDAEMONSHARED pDaemonShared = pKernelGlobals->pDaemonShared;
            pGlobalSettings->fEnableXWPHook = ulExtra;
            // (de)install the hook by notifying the daemon
            if (pDaemonShared)
            {
                if (pDaemonShared->hwndDaemonObject)
                {
                    if (WinSendMsg(pDaemonShared->hwndDaemonObject,
                                   XDM_HOOKINSTALL,
                                   (MPARAM)(pGlobalSettings->fEnableXWPHook),
                                   0))
                        fShowHookInstalled = TRUE;
                    else
                        fShowHookDeinstalled = TRUE;
                }
            }
        break; }

        case ID_XCSI_REPLACEFILEPAGE:
            pGlobalSettings->fReplaceFilePage = ulExtra;
        break;

        case ID_XCSI_XSYSTEMSOUNDS:
            pGlobalSettings->fXSystemSounds = ulExtra;
            // check if sounds are to be installed or de-installed:
            if (sndAddtlSoundsInstalled() != ulExtra)
                if (cmnMessageBoxMsg(pcnbp->hwndPage,
                                     148,       // "XWorkplace Setup"
                                     (ulExtra)
                                        ? 166   // "install?"
                                        : 167,  // "de-install?"
                                     MB_YESNO)
                        == MBID_YES)
                {
                    sndInstallAddtlSounds(ulExtra);
                }
        break;

        case ID_XCSI_ANIMOUSE:
            pGlobalSettings->fAniMouse = ulExtra;
        break;

        case ID_XCSI_ENABLESTATUSBARS:
            pGlobalSettings->fEnableStatusBars = ulExtra;
            // update status bars for open folders
            xthrPostWorkerMsg(WOM_UPDATEALLSTATUSBARS,
                              (MPARAM)1,
                              MPNULL);
            // and open settings notebooks
            ntbUpdateVisiblePage(NULL,   // all somSelf's
                                 SP_XFOLDER_FLDR);
        break;

        case ID_XCSI_ENABLESNAP2GRID:
            pGlobalSettings->fEnableSnap2Grid = ulExtra;
            // update open settings notebooks
            ntbUpdateVisiblePage(NULL,   // all somSelf's
                                 SP_XFOLDER_FLDR);
        break;

        case ID_XCSI_ENABLEFOLDERHOTKEYS:
            pGlobalSettings->fEnableFolderHotkeys = ulExtra;
            // update open settings notebooks
            ntbUpdateVisiblePage(NULL,   // all somSelf's
                                 SP_XFOLDER_FLDR);
        break;

        case ID_XCSI_EXTFOLDERSORT:
            pGlobalSettings->ExtFolderSort = ulExtra;
        break;

        case ID_XCSI_NUMLOCKON:
            pGlobalSettings->fNumLockStartup = ulExtra;
            winhSetNumLock(ulExtra);
        break;

        /* case ID_XCSI_MONITORCDROMS:
            pGlobalSettings->MonitorCDRoms = ulExtra;
            break; */

        case ID_XCSI_ARCHIVING:
            pGlobalSettings->fReplaceArchiving = ulExtra;
        break;

        case ID_XCSI_RESTARTWPS:
            pGlobalSettings->fRestartWPS = ulExtra;
        break;

        case ID_XCSI_XSHUTDOWN:
            pGlobalSettings->fXShutdown = ulExtra;
            // update "Desktop" menu page
            ntbUpdateVisiblePage(NULL,   // all somSelf's
                                 SP_DTP_MENUITEMS);
        break;

        case ID_XCSI_EXTASSOCS:
            pGlobalSettings->fExtAssocs = ulExtra;
            (*(pcnbp->pfncbInitPage))(pcnbp, CBI_ENABLE);
            if (ulExtra)
                DebugBox("XWorkplace",
                         "Warning: Extended file associations don't work properly yet. Daily use is NOT recommended.");
        break;

        /* case ID_XCDI_IGNOREFILTERS:
            pGlobalSettings->fIgnoreFilters = ulExtra;
            break; */

        case ID_XCSI_REPLFILEEXISTS:
            pGlobalSettings->fReplFileExists = ulExtra;
        break;

        case ID_XCSI_REPLDRIVENOTREADY:
            pGlobalSettings->fReplDriveNotReady = ulExtra;
        break;

        case ID_XCSI_CLEANUPINIS:
            pGlobalSettings->CleanupINIs = ulExtra;
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
            pGlobalSettings->AddObjectPage = pGSBackup->AddObjectPage;
            pGlobalSettings->fEnableXWPHook = pGSBackup->fEnableXWPHook;
            pGlobalSettings->fReplaceFilePage = pGSBackup->fReplaceFilePage;
            pGlobalSettings->fXSystemSounds = pGSBackup->fXSystemSounds;
            pGlobalSettings->fAniMouse = pGSBackup->fAniMouse;

            pGlobalSettings->fEnableStatusBars = pGSBackup->fEnableStatusBars;
            pGlobalSettings->fEnableSnap2Grid = pGSBackup->fEnableSnap2Grid;
            pGlobalSettings->fEnableFolderHotkeys = pGSBackup->fEnableFolderHotkeys;
            pGlobalSettings->ExtFolderSort = pGSBackup->ExtFolderSort;
            pGlobalSettings->fMonitorCDRoms = pGSBackup->fMonitorCDRoms;

            pGlobalSettings->fReplaceArchiving = pGSBackup->fReplaceArchiving;
            pGlobalSettings->fRestartWPS = pGSBackup->fRestartWPS;
            pGlobalSettings->fXShutdown = pGSBackup->fXShutdown;

            pGlobalSettings->fExtAssocs = pGSBackup->fExtAssocs;
            // pGlobalSettings->fIgnoreFilters = pGSBackup->fIgnoreFilters;
            pGlobalSettings->CleanupINIs = pGSBackup->CleanupINIs;
            pGlobalSettings->fReplFileExists = pGSBackup->fReplFileExists;
            pGlobalSettings->fReplDriveNotReady = pGSBackup->fReplDriveNotReady;

            // update the display by calling the INIT callback
            (*(pcnbp->pfncbInitPage))(pcnbp, CBI_SET | CBI_ENABLE);
        break; }

        case DID_DEFAULT:
        {
            // set the default settings for this settings page
            // (this is in common.c because it's also used at
            // WPS startup)
            cmnSetDefaultSettings(pcnbp->ulPageID);
            // update the display by calling the INIT callback
            (*(pcnbp->pfncbInitPage))(pcnbp, CBI_SET | CBI_ENABLE);
        break; }

        default:
            fSave = FALSE;
    }

    cmnUnlockGlobalSettings();

    if (fSave)
        cmnStoreGlobalSettings();

    if (fShowClassesSetup)
    {
        // "classes" dialog
        HWND hwndClassesDlg = WinLoadDlg(HWND_DESKTOP,     // parent
                                         pcnbp->hwndPage,  // owner
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
        cmnMessageBoxMsg(pcnbp->hwndPage, 148, 157, MB_OK);
    else if (fShowHookDeinstalled)
        // "hook deinstalled" msg
        cmnMessageBoxMsg(pcnbp->hwndPage, 148, 158, MB_OK);

    return ((MPARAM)-1);
}

/*
 *@@ setFeaturesMessages:
 *      notebook callback function (notebook.c) for the
 *      XWPSetup "File Operations" page.
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
                    if (usNotifyCode == TTN_NEEDTEXT)
                    {
                        PTOOLTIPTEXT pttt = (PTOOLTIPTEXT)mp2;
                        PCHECKBOXRECORDCORE precc;
                        HWND         hwndCnr = WinWindowFromID(pcnbp->hwndPage,
                                                               ID_XCDI_CONTAINER);
                        POINTL       ptlMouse;

                        // we use pUser2 for the Tooltip string
                        if (pcnbp->pUser2)
                        {
                            free(pcnbp->pUser2);
                            pcnbp->pUser2 = NULL;
                        }

                        // find record under mouse
                        WinQueryMsgPos(WinQueryAnchorBlock(pcnbp->hwndPage),
                                       &ptlMouse);
                        precc = (PCHECKBOXRECORDCORE)cnrhFindRecordFromPoint(
                                                            hwndCnr,
                                                            &ptlMouse,
                                                            NULL,
                                                            CMA_ICON | CMA_TEXT,
                                                            FRFP_SCREENCOORDS);
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
                                        precc->usItemID);
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
                                                          pttt->hdr.idFrom,
                                                          precc->usItemID,
                                                          arc);
                                }

                                pcnbp->pUser2 = strdup(szHelpString);

                                pttt->lpszText = pcnbp->pUser2;
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
 *                                                                  *
 *   XWPSetup "Objects" page notebook callbacks (notebook.c)        *
 *                                                                  *
 ********************************************************************/

/*
 *@@ DisableObjectMenuItems:
 *      helper function for setObjectsItemChanged
 *      (XWPSetup "Objects" page) to disable items
 *      in the "Objects" menu buttons if objects
 *      exist already.
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
        WPObject* pObject = wpshQueryObjectFromID(pso2->pszDefaultID,
                                                  NULL);        // pulErrorCode
        PSZ         pszMenuItemText = winhQueryMenuItemText(hwndMenu,
                                                            pso2->usMenuID);
        strhxcat(&pszMenuItemText, " (");
        strhxcat(&pszMenuItemText, pso2->pszObjectClass);

        if (pObject != NULL)
        {
            // object found:
            // append the path to the menu item
            WPFolder    *pFolder = _wpQueryFolder(pObject);
            CHAR        szFolderPath[CCHMAXPATH] = "";
            _wpQueryFilename(pFolder, szFolderPath, TRUE);      // fully qualified
            strhxcat(&pszMenuItemText, ", ");
            strhxcat(&pszMenuItemText, szFolderPath);

            // disable menu item
            WinEnableMenuItem(hwndMenu, pso2->usMenuID, FALSE);
        }

        strhxcat(&pszMenuItemText, ")");
        if (pObject == NULL)
            // append "...", because we'll have a message box then
            strhxcat(&pszMenuItemText, "...");

        WinSetMenuItemText(hwndMenu, pso2->usMenuID, pszMenuItemText);
        free(pszMenuItemText);

        pso2++;
    }
}

/*
 *@@ CreateObjectFromMenuID:
 *      this creates a default WPS/XWP object from the
 *      given menu item ID, after displaying a confirmation
 *      box (XWPSetup "Obejcts" page).
 *      Returns TRUE if the menu ID was found in the given array.
 */

BOOL CreateObjectFromMenuID(USHORT usMenuID,        // in: selected menu item
                            PSTANDARDOBJECT pso,    // in: first menu array item
                            ULONG ulMax)            // in: size of menu array
{
    BOOL    brc = FALSE;
    ULONG   ul = 0;
    PSTANDARDOBJECT pso2 = pso;

    for (ul = 0;
         ul < ulMax;
         ul++)
    {
        if (pso2->usMenuID == usMenuID)
        {
            CHAR    szSetupString[2000];
            PSZ     apsz[2] = {  pso2->pszObjectClass,
                                 szSetupString
                              };
            sprintf(szSetupString, "%sOBJECTID=%s",
                    pso2->pszSetupString,       // can be empty or ";"-terminated string
                    pso2->pszDefaultID);
            if (cmnMessageBoxMsgExt(HWND_DESKTOP,
                                    148, // "XWorkplace Setup",
                                    apsz,
                                    2,
                                    163,        // "create object?"
                                    MB_YESNO)
                         == MBID_YES)
            {
                HOBJECT hobj;
                hobj = WinCreateObject(pso2->pszObjectClass,       // class
                                       pso2->pszObjectClass,       // title
                                       szSetupString,
                                       "<WP_DESKTOP>",
                                       CO_FAILIFEXISTS);
                if (hobj)
                    cmnMessageBoxMsg(HWND_DESKTOP,
                                     148, // "XWorkplace Setup",
                                     164, // "success"
                                     MB_OK);
                else
                    cmnMessageBoxMsg(HWND_DESKTOP,
                                     148, // "XWorkplace Setup",
                                     165, // "failed!"
                                     MB_OK);
            }
            brc = TRUE;
            break;
        }
        // not found: try next item
        pso2++;
    }

    return (brc);
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
        ctlMakeMenuButton(WinWindowFromID(pcnbp->hwndPage, ID_XCD_OBJECTS_SYSTEM),
                          0, 0);        // query for menu

        ctlMakeMenuButton(WinWindowFromID(pcnbp->hwndPage, ID_XCD_OBJECTS_XWORKPLACE),
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
                              USHORT usItemID, USHORT usNotifyCode,
                              ULONG ulExtra)      // for checkboxes: contains new state
{
    MRESULT mrc = 0;

    switch (usItemID)
    {
        /*
         * ID_XCD_OBJECTS_SYSTEM:
         *      system objects menu button
         */

        case ID_XCD_OBJECTS_SYSTEM:
        {
            HPOINTER hptrOld = winhSetWaitPointer();
            HWND    hwndMenu = WinLoadMenu(WinWindowFromID(pcnbp->hwndPage, usItemID),
                                           cmnQueryNLSModuleHandle(FALSE),
                                           ID_XSM_OBJECTS_SYSTEM);
            DisableObjectMenuItems(hwndMenu,
                                   &WPSObjects[0],
                                   sizeof(WPSObjects) / sizeof(STANDARDOBJECT)); // array item count
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
            HWND    hwndMenu = WinLoadMenu(WinWindowFromID(pcnbp->hwndPage, usItemID),
                                           cmnQueryNLSModuleHandle(FALSE),
                                           ID_XSM_OBJECTS_XWORKPLACE);
            DisableObjectMenuItems(hwndMenu,
                                   &XWPObjects[0],
                                   sizeof(XWPObjects) / sizeof(STANDARDOBJECT)); // array item count
            mrc = (MRESULT)hwndMenu;
            WinSetPointer(HWND_DESKTOP, hptrOld);
        break; }

        case ID_XCD_OBJECTS_CONFIGFOLDER:
            if (cmnMessageBoxMsg(pcnbp->hwndPage,
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
            if (    (usItemID >= OBJECTSIDFIRST)
                 && (usItemID <= OBJECTSIDLAST)
               )
            {
                if (!CreateObjectFromMenuID(usItemID,
                                            &WPSObjects[0],
                                            sizeof(WPSObjects) / sizeof(STANDARDOBJECT)))
                    // WPS objects not found:
                    // try XWPS objects
                    CreateObjectFromMenuID(usItemID,
                                           &XWPObjects[0],
                                           sizeof(XWPObjects) / sizeof(STANDARDOBJECT));
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
        winhSetSliderTicks(WinWindowFromID(pcnbp->hwndPage, ID_XCDI_WORKERPRTY_SLIDER),
                           0, 6);      // six pixels high
    }

    if (flFlags & CBI_SET)
    {
        // variable menu ID offset spin button
        winhSetDlgItemSpinData(pcnbp->hwndPage, ID_XCDI_VARMENUOFFSET,
                                                100, 2000,
                                                pGlobalSettings->VarMenuOffset);
        winhSetDlgItemChecked(pcnbp->hwndPage, ID_XCDI_NOSUBCLASSING,
                                               pGlobalSettings->NoSubclassing);
        winhSetDlgItemChecked(pcnbp->hwndPage, ID_XCDI_NOWORKERTHREAD,
                                               pGlobalSettings->NoWorkerThread);
        winhSetDlgItemChecked(pcnbp->hwndPage, ID_XCDI_USE8HELVFONT,
                                               pGlobalSettings->fUse8HelvFont);
        winhSetDlgItemChecked(pcnbp->hwndPage, ID_XCDI_NOEXCPTBEEPS,
                                               pGlobalSettings->fNoExcptBeeps);
        winhSetDlgItemChecked(pcnbp->hwndPage, ID_XCDI_WORKERPRTY_BEEP,
                                               pGlobalSettings->fWorkerPriorityBeep);

        winhSetSliderArmPosition(WinWindowFromID(pcnbp->hwndPage, ID_XCDI_WORKERPRTY_SLIDER),
                                 SMA_INCREMENTVALUE,
                                 pGlobalSettings->bDefaultWorkerThreadPriority);
    }

    if (flFlags & CBI_ENABLE)
    {
        WinEnableControl(pcnbp->hwndPage, ID_XCDI_WORKERPRTY_TEXT1,
                        !(pGlobalSettings->NoWorkerThread));
        WinEnableControl(pcnbp->hwndPage, ID_XCDI_WORKERPRTY_SLIDER,
                        !(pGlobalSettings->NoWorkerThread));
        WinEnableControl(pcnbp->hwndPage, ID_XCDI_WORKERPRTY_TEXT2,
                        !(pGlobalSettings->NoWorkerThread));
        WinEnableControl(pcnbp->hwndPage, ID_XCDI_WORKERPRTY_BEEP,
                        !(pGlobalSettings->NoWorkerThread));
    }
}

/*
 *@@ setParanoiaItemChanged:
 *      notebook callback function (notebook.c) for the
 *      XWPSetup "Paranoia" page.
 *      Reacts to changes of any of the dialog controls.
 */

MRESULT setParanoiaItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                               USHORT usItemID, USHORT usNotifyCode,
                               ULONG ulExtra)      // for checkboxes: contains new state
{
    GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(5000);
    BOOL fSave = TRUE,
         fUpdateOtherPages = FALSE;

    switch (usItemID)
    {
        case ID_XCDI_VARMENUOFFSET:
            pGlobalSettings->VarMenuOffset = ulExtra;
        break;

        case ID_XCDI_NOSUBCLASSING:
            pGlobalSettings->NoSubclassing   = ulExtra;
            // set flag to iterate over other notebook pages
            fUpdateOtherPages = TRUE;
        break;

        case ID_XCDI_NOWORKERTHREAD:
            pGlobalSettings->NoWorkerThread  = ulExtra;
            // update the display by calling the INIT callback
            (*(pcnbp->pfncbInitPage))(pcnbp, CBI_ENABLE);
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
                                            WinWindowFromID(pcnbp->hwndPage, ID_XCDI_WORKERPRTY_SLIDER),
                                            SMA_INCREMENTVALUE);

            switch (lSliderIndex)
            {
                case 0:     pszNewInfo = "Idle ñ0"; break;
                case 1:     pszNewInfo = "Idle +31"; break;
                case 2:     pszNewInfo = "Regular ñ0"; break;
            }

            WinSetDlgItemText(pcnbp->hwndPage,
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
            pGlobalSettings->NoSubclassing   = pGSBackup->NoSubclassing;
            pGlobalSettings->NoWorkerThread  = pGSBackup->NoWorkerThread;
            pGlobalSettings->fUse8HelvFont   = pGSBackup->fUse8HelvFont;
            pGlobalSettings->fNoExcptBeeps    = pGSBackup->fNoExcptBeeps;
            pGlobalSettings->bDefaultWorkerThreadPriority
                                             = pGSBackup->bDefaultWorkerThreadPriority;
            pGlobalSettings->fWorkerPriorityBeep = pGSBackup->fWorkerPriorityBeep;

            // update the display by calling the INIT callback
            (*(pcnbp->pfncbInitPage))(pcnbp, CBI_SET | CBI_ENABLE);
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
            (*(pcnbp->pfncbInitPage))(pcnbp, CBI_SET | CBI_ENABLE);
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


