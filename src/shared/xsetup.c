
/*
 *@@sourcefile xsetup.c:
 *      this file contains the implementation of the XWPSetup
 *      class.
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
#include "helpers\tmsgfile.h"           // "text message file" handling
#include "helpers\xstring.h"            // extended string helpers

// SOM headers which don't crash with prec. header files
#include "xwpsetup.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
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
    USHORT  usMenuID;               // corresponding menu ID in xfldr001.rc resources
} STANDARDOBJECT, *PSTANDARDOBJECT;

#define OBJECTSIDFIRST 100      // first object menu ID, inclusive
#define OBJECTSIDLAST  213      // last object menu ID, inclusive

STANDARDOBJECT  G_WPSObjects[] = {
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
                G_XWPObjects[] = {
                                    XFOLDER_WPSID, "XFldWPS", "", 200,
                                    XFOLDER_KERNELID, "XFldSystem", "", 201,
                                    XFOLDER_SCREENID, "XWPScreen", "", 203,
                                    XFOLDER_CLASSLISTID, "XWPClassList", "", 202,

                                    XFOLDER_CONFIGID, "WPFolder",
                                            "ICONVIEW=NONFLOWED,MINI;ALWAYSSORT=NO;",
                                            210,
                                    XFOLDER_STARTUPID, "XFldStartup",
                                            "ICONVIEW=NONFLOWED,MINI;ALWAYSSORT=NO;",
                                            211,
                                    XFOLDER_SHUTDOWNID, "XFldShutdown",
                                            "ICONVIEW=NONFLOWED,MINI;ALWAYSSORT=NO;",
                                            212,

                                    XFOLDER_TRASHCANID, "XWPTrashCan",
                                            "DETAILSCLASS=XWPTrashObject;"
                                            "SORTCLASS=XWPTrashObject;",
                                            213
                               };

/*
 *@@ setCreateStandardObject:
 *      this creates a default WPS/XWP object from the
 *      given menu item ID, after displaying a confirmation
 *      box (XWPSetup "Obejcts" page).
 *      Returns TRUE if the menu ID was found in the given array.
 *
 *@@changed V0.9.1 (2000-02-01) [umoeller]: renamed prototype; added hwndOwner
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
        pso2 = G_WPSObjects;
        ulMax = sizeof(G_WPSObjects) / sizeof(STANDARDOBJECT);
    }
    else
    {
        pso2 = G_XWPObjects;
        ulMax = sizeof(G_XWPObjects) / sizeof(STANDARDOBJECT);
    }

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
            somId       somidThis = somIdFromString(pso2->pszObjectClass);
            SOMClass    *pClassObject = _somFindClass(SOMClassMgrObject, somidThis, 0, 0);

            sprintf(szSetupString, "%sOBJECTID=%s",
                    pso2->pszSetupString,       // can be empty or ";"-terminated string
                    pso2->pszDefaultID);

            if (pClassObject)
                // get class's default title
                apsz[0] = _wpclsQueryTitle(pClassObject);

            if (apsz[0] == NULL)
                // title not found: use class name then
                apsz[0] = pso2->pszObjectClass;

            if (cmnMessageBoxMsgExt(hwndOwner,
                                    148, // "XWorkplace Setup",
                                    apsz,
                                    2,
                                    163,        // "create object?"
                                    MB_YESNO)
                         == MBID_YES)
            {
                HOBJECT hobj = WinCreateObject(pso2->pszObjectClass,       // class
                                               apsz[0], // pso2->pszObjectClass,       // title
                                               szSetupString,
                                               "<WP_DESKTOP>",
                                               CO_FAILIFEXISTS);
                if (hobj)
                {
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

VOID AddResourceDLLToLB(HWND hwndDlg,                   // in: dlg with listbox
                        ULONG idLB,                     // in: listbox item ID
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
            fXWPScreen,
            fXFldStartup,
            fXFldShutdown,
            fXWPClassList,
            fXWPTrashCan,
            fXWPString;

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

        // new items with V0.9.3 (2000-04-26) [umoeller]
        ID_XCDI_XWPCLS_XWPSCREEN,
        ID_XCDI_XWPCLS_XWPSTRING,

        DID_OK,
        DID_CANCEL
    };

/*
 * fnwpXWorkplaceClasses:
 *      dialog procedure for the "XWorkplace Classes" dialog.
 *
 *@@changed V0.9.3 (2000-04-26) [umoeller]: added generic fonts support
 *@@changed V0.9.3 (2000-04-26) [umoeller]: added new classes
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
            pxwpc->fXFldStartup = (winhQueryWPSClass(pObjClass, "XFldStartup") != 0);
            pxwpc->fXFldShutdown = (winhQueryWPSClass(pObjClass, "XFldShutdown") != 0);
            pxwpc->fXWPClassList = (winhQueryWPSClass(pObjClass, "XWPClassList") != 0);
            pxwpc->fXWPTrashCan = (     (winhQueryWPSClass(pObjClass, "XWPTrashCan") != 0)
                                     && (winhQueryWPSClass(pObjClass, "XWPTrashObject") != 0)
                                  );
            pxwpc->fXWPString = (winhQueryWPSClass(pObjClass, "XWPString") != 0);

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
            winhSetDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XFLDSTARTUP, pxwpc->fXFldStartup);
            winhSetDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XFLDSHUTDOWN, pxwpc->fXFldShutdown),
            winhSetDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XWPCLASSLIST, pxwpc->fXWPClassList);
            winhSetDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XWPTRASHCAN, pxwpc->fXWPTrashCan);
            winhSetDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XWPSTRING, pxwpc->fXWPString);

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
                pxwpcNew->fXWPScreen = winhIsDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XWPSCREEN);
                pxwpcNew->fXFldStartup = winhIsDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XFLDSTARTUP);
                pxwpcNew->fXFldShutdown = winhIsDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XFLDSHUTDOWN);
                pxwpcNew->fXWPClassList = winhIsDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XWPCLASSLIST);
                pxwpcNew->fXWPTrashCan = winhIsDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XWPTRASHCAN);
                pxwpcNew->fXWPString = winhIsDlgItemChecked(hwndDlg, ID_XCDI_XWPCLS_XWPSTRING);

                // compare old and new selections:
                // if we have a difference, add the class name
                // to the respective string to (de)register

                if ((pxwpcOld->fXFldObject) && (!pxwpcNew->fXFldObject))
                {
                    // deregister XFldObject
                    xstrcat(&pszDereg, "XFldObject\n");
                    // unreplace XFldObject
                    xstrcat(&pszUnreplace, "WPObject XFldObject\n");
                }
                else if ((pxwpcNew->fXFldObject) && (!pxwpcOld->fXFldObject))
                {
                    // register
                    xstrcat(&pszReg, "XFldObject\n");
                    // replace
                    xstrcat(&pszReplace, "WPObject XFldObject\n");
                }

                if ((pxwpcOld->fXFolder) && (!pxwpcNew->fXFolder))
                {
                    // deregister XFolder
                    xstrcat(&pszDereg, "XFolder\n");
                    // unreplace XFolder
                    xstrcat(&pszUnreplace, "WPFolder XFolder\n");
                }
                else if ((pxwpcNew->fXFolder) && (!pxwpcOld->fXFolder))
                {
                    // register
                    xstrcat(&pszReg, "XFolder\n");
                    // replace
                    xstrcat(&pszReplace, "WPFolder XFolder\n");
                }

                if ((pxwpcOld->fXFldDisk) && (!pxwpcNew->fXFldDisk))
                {
                    // deregister XFldDisk
                    xstrcat(&pszDereg, "XFldDisk\n");
                    // unreplace XFldDisk
                    xstrcat(&pszUnreplace, "WPDisk XFldDisk\n");
                }
                else if ((pxwpcNew->fXFldDisk) && (!pxwpcOld->fXFldDisk))
                {
                    // register
                    xstrcat(&pszReg, "XFldDisk\n");
                    // replace
                    xstrcat(&pszReplace, "WPDisk XFldDisk\n");
                }

                if ((pxwpcOld->fXFldDesktop) && (!pxwpcNew->fXFldDesktop))
                {
                    // deregister XFldDesktop
                    xstrcat(&pszDereg, "XFldDesktop\n");
                    // unreplace XFldDesktop
                    xstrcat(&pszUnreplace, "WPDesktop XFldDesktop\n");
                }
                else if ((pxwpcNew->fXFldDesktop) && (!pxwpcOld->fXFldDesktop))
                {
                    // register
                    xstrcat(&pszReg, "XFldDesktop\n");
                    // replace
                    xstrcat(&pszReplace, "WPDesktop XFldDesktop\n");
                }

                if ((pxwpcOld->fXFldDataFile) && (!pxwpcNew->fXFldDataFile))
                {
                    // deregister XFldDataFile
                    xstrcat(&pszDereg, "XFldDataFile\n");
                    // unreplace XFldDataFile
                    xstrcat(&pszUnreplace, "WPDataFile XFldDataFile\n");
                }
                else if ((pxwpcNew->fXFldDataFile) && (!pxwpcOld->fXFldDataFile))
                {
                    // register
                    xstrcat(&pszReg, "XFldDataFile\n");
                    // replace
                    xstrcat(&pszReplace, "WPDataFile XFldDataFile\n");
                }

                if ((pxwpcOld->fXFldProgramFile) && (!pxwpcNew->fXFldProgramFile))
                {
                    // deregister XFldProgramFile
                    xstrcat(&pszDereg, "XFldProgramFile\n");
                    // unreplace XFldProgramFile
                    xstrcat(&pszUnreplace, "WPProgramFile XFldProgramFile\n");
                }
                else if ((pxwpcNew->fXFldProgramFile) && (!pxwpcOld->fXFldProgramFile))
                {
                    // register
                    xstrcat(&pszReg, "XFldProgramFile\n");
                    // replace
                    xstrcat(&pszReplace, "WPProgramFile XFldProgramFile\n");
                }

                if ((pxwpcOld->fXWPSound) && (!pxwpcNew->fXWPSound))
                {
                    // deregister XWPSound
                    xstrcat(&pszDereg, "XWPSound\n");
                    // unreplace XWPSound
                    xstrcat(&pszUnreplace, "WPSound XWPSound\n");
                }
                else if ((pxwpcNew->fXWPSound) && (!pxwpcOld->fXWPSound))
                {
                    // register
                    xstrcat(&pszReg, "XWPSound\n");
                    // replace
                    xstrcat(&pszReplace, "WPSound XWPSound\n");
                }

                if ((pxwpcOld->fXWPMouse) && (!pxwpcNew->fXWPMouse))
                {
                    // deregister XWPMouse
                    xstrcat(&pszDereg, "XWPMouse\n");
                    // unreplace XWPMouse
                    xstrcat(&pszUnreplace, "WPMouse XWPMouse\n");
                }
                else if ((pxwpcNew->fXWPMouse) && (!pxwpcOld->fXWPMouse))
                {
                    // register
                    xstrcat(&pszReg, "XWPMouse\n");
                    // replace
                    xstrcat(&pszReplace, "WPMouse XWPMouse\n");
                }

                if ((pxwpcOld->fXWPKeyboard) && (!pxwpcNew->fXWPKeyboard))
                {
                    // deregister XWPKeyboard
                    xstrcat(&pszDereg, "XWPKeyboard\n");
                    // unreplace XWPKeyboard
                    xstrcat(&pszUnreplace, "WPKeyboard XWPKeyboard\n");
                }
                else if ((pxwpcNew->fXWPKeyboard) && (!pxwpcOld->fXWPKeyboard))
                {
                    // register
                    xstrcat(&pszReg, "XWPKeyboard\n");
                    // replace
                    xstrcat(&pszReplace, "WPKeyboard XWPKeyboard\n");
                }

                /*
                 * new classes (no replacements):
                 *
                 */

                if ((pxwpcOld->fXWPSetup) && (!pxwpcNew->fXWPSetup))
                    // deregister XWPSetup
                    xstrcat(&pszDereg, "XWPSetup\n");
                else if ((pxwpcNew->fXWPSetup) && (!pxwpcOld->fXWPSetup))
                    // register
                    xstrcat(&pszReg, "XWPSetup\n");

                if ((pxwpcOld->fXFldSystem) && (!pxwpcNew->fXFldSystem))
                    // deregister XFldSystem
                    xstrcat(&pszDereg, "XFldSystem\n");
                else if ((pxwpcNew->fXFldSystem) && (!pxwpcOld->fXFldSystem))
                    // register
                    xstrcat(&pszReg, "XFldSystem\n");

                if ((pxwpcOld->fXFldWPS) && (!pxwpcNew->fXFldWPS))
                    // deregister XFldWPS
                    xstrcat(&pszDereg, "XFldWPS\n");
                else if ((pxwpcNew->fXFldWPS) && (!pxwpcOld->fXFldWPS))
                    // register
                    xstrcat(&pszReg, "XFldWPS\n");

                if ((pxwpcOld->fXWPScreen) && (!pxwpcNew->fXWPScreen))
                    // deregister XWPScreen
                    xstrcat(&pszDereg, "XWPScreen\n");
                else if ((pxwpcNew->fXWPScreen) && (!pxwpcOld->fXWPScreen))
                    // register
                    xstrcat(&pszReg, "XWPScreen\n");

                if ((pxwpcOld->fXFldStartup) && (!pxwpcNew->fXFldStartup))
                    // deregister XFldStartup
                    xstrcat(&pszDereg, "XFldStartup\n");
                else if ((pxwpcNew->fXFldStartup) && (!pxwpcOld->fXFldStartup))
                    // register
                    xstrcat(&pszReg, "XFldStartup\n");

                if ((pxwpcOld->fXFldShutdown) && (!pxwpcNew->fXFldShutdown))
                    // deregister XFldShutdown
                    xstrcat(&pszDereg, "XFldShutdown\n");
                else if ((pxwpcNew->fXFldShutdown) && (!pxwpcOld->fXFldShutdown))
                    // register
                    xstrcat(&pszReg, "XFldShutdown\n");

                if ((pxwpcOld->fXWPClassList) && (!pxwpcNew->fXWPClassList))
                    // deregister XWPClassList
                    xstrcat(&pszDereg, "XWPClassList\n");
                else if ((pxwpcNew->fXWPClassList) && (!pxwpcOld->fXWPClassList))
                    // register
                    xstrcat(&pszReg, "XWPClassList\n");

                if ((pxwpcOld->fXWPTrashCan) && (!pxwpcNew->fXWPTrashCan))
                {
                    // deregister XWPTrashCan
                    xstrcat(&pszDereg, "XWPTrashCan\n");
                    // deregister XWPTrashObject
                    xstrcat(&pszDereg, "XWPTrashObject\n");
                }
                else if ((pxwpcNew->fXWPTrashCan) && (!pxwpcOld->fXWPTrashCan))
                {
                    xstrcat(&pszReg, "XWPTrashCan\n");
                    xstrcat(&pszReg, "XWPTrashObject\n");
                }

                if ((pxwpcOld->fXWPString) && (!pxwpcNew->fXWPString))
                    // deregister XWPString
                    xstrcat(&pszDereg, "XWPString\n");
                else if ((pxwpcNew->fXWPString) && (!pxwpcOld->fXWPString))
                    // register
                    xstrcat(&pszReg, "XWPString\n");

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
                    xstrcpy(&pszMessage, szTemp);
                    xstrcat(&pszMessage, "\n");
                    if (fReg)
                    {
                        xstrcat(&pszMessage, "\n");
                        cmnGetMessage(NULL, 0, szTemp, sizeof(szTemp),
                                      143); // "Register the following classes:"
                        xstrcat(&pszMessage, szTemp);
                        xstrcat(&pszMessage, "\n\n");
                        xstrcat(&pszMessage, pszReg);
                        if (pszReplace)
                        {
                            xstrcat(&pszMessage, "\n");
                            cmnGetMessage(NULL, 0, szTemp, sizeof(szTemp),
                                          144); // "Replace the following classes:"
                            xstrcat(&pszMessage, szTemp);
                            xstrcat(&pszMessage, "\n\n");
                            xstrcat(&pszMessage, pszReplace);
                        }
                    }
                    if (fDereg)
                    {
                        xstrcat(&pszMessage, "\n");
                        cmnGetMessage(NULL, 0, szTemp, sizeof(szTemp),
                                      145); // "Deregister the following classes:"
                        xstrcat(&pszMessage, szTemp);
                        xstrcat(&pszMessage, "\n\n");
                        xstrcat(&pszMessage, pszDereg);
                        if (pszUnreplace)
                        {
                            xstrcat(&pszMessage, "\n");
                            cmnGetMessage(NULL, 0, szTemp, sizeof(szTemp),
                                          146); // "Undo the following class replacements:"
                            xstrcat(&pszMessage, szTemp);
                            xstrcat(&pszMessage, "\n\n");
                            xstrcat(&pszMessage, pszUnreplace);
                        }
                    }
                    xstrcat(&pszMessage, "\n");
                    cmnGetMessage(NULL, 0, szTemp, sizeof(szTemp),
                                  147); // "Are you sure you want to do this?"
                    xstrcat(&pszMessage, szTemp);

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
                                            xstrcat(&pszFailing, pszReplacedClass);
                                            xstrcat(&pszFailing, "\n");
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
                                            xstrcat(&pszFailing, pszClass);
                                            xstrcat(&pszFailing, "\n");
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
                                            xstrcat(&pszFailing, pszClass);
                                            xstrcat(&pszFailing, "\n");
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
                                            xstrcat(&pszFailing, pszReplacedClass);
                                            xstrcat(&pszFailing, "\n");
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
                    PXWPCLASSES     pxwpcOld = WinQueryWindowPtr(hwndDlg, QWL_USER);
                                    // pxwpcNew = pxwpcOld + 1;
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
            // general features
            ID_XCSI_GENERALFEATURES, 0, 0, NULL,
            ID_XCSI_REPLACEICONS, ID_XCSI_GENERALFEATURES, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,
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
            ID_XCSI_ANIMOUSE, ID_XCSI_MOUSEKEYBOARDFEATURES, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,
            ID_XCSI_XWPHOOK, ID_XCSI_MOUSEKEYBOARDFEATURES, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,
            ID_XCSI_GLOBALHOTKEYS, ID_XCSI_MOUSEKEYBOARDFEATURES, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,
            ID_XCSI_PAGEMAGE, ID_XCSI_MOUSEKEYBOARDFEATURES, WS_VISIBLE | BS_AUTOCHECKBOX, NULL,

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
            ID_XCSI_REPLACEDELETE, ID_XCSI_FILEOPERATIONS, WS_VISIBLE | BS_AUTOCHECKBOX, NULL
        };

PCHECKBOXRECORDCORE pFeatureRecordsList = NULL;

/*
 *@@ setFeaturesInitPage:
 *      notebook callback function (notebook.c) for the
 *      XWPSetup "File Operations" page.
 *      Sets the controls on the page according to the
 *      Global Settings.
 *
 *@@changed V0.9.1 (2000-02-01) [umoeller]: added global hotkeys flag
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

        if (ctlMakeCheckboxContainer(pcnbp->hwndDlgPage,
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
        if (ctlRegisterTooltip(WinQueryAnchorBlock(pcnbp->hwndDlgPage)))
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

            if (pcnbp->hwndTooltip)
            {
                // tooltip successfully created:
                // add tools (i.e. controls of the dialog)
                // according to the usToolIDs array
                TOOLINFO    ti;
                ti.cbSize = sizeof(TOOLINFO);
                ti.uFlags = TTF_IDISHWND | /* TTF_CENTERTIP | */ TTF_SUBCLASS;
                ti.hwnd = pcnbp->hwndDlgPage;
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
        ctlSetRecordChecked(hwndFeaturesCnr, ID_XCSI_REPLFILEEXISTS,
                pGlobalSettings->fReplFileExists);
        ctlSetRecordChecked(hwndFeaturesCnr, ID_XCSI_REPLDRIVENOTREADY,
                pGlobalSettings->fReplDriveNotReady);
        ctlSetRecordChecked(hwndFeaturesCnr, ID_XCSI_XWPTRASHCAN,
                fopsTrashCanReady() && pGlobalSettings->fTrashDelete);
        ctlSetRecordChecked(hwndFeaturesCnr, ID_XCSI_REPLACEDELETE,
                pGlobalSettings->fReplaceTrueDelete);
    }

    if (flFlags & CBI_ENABLE)
    {
        PDAEMONSHARED pDaemonShared = pKernelGlobals->pDaemonShared;

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

        ctlEnableRecord(hwndFeaturesCnr, ID_XCSI_ANIMOUSE,
                FALSE);     // ### not implemented yet
        ctlEnableRecord(hwndFeaturesCnr, ID_XCSI_XWPHOOK,
                (pDaemonShared->hwndDaemonObject != NULLHANDLE));
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
 *      XWPSetup "File Operations" page.
 *      Reacts to changes of any of the dialog controls.
 *
 *@@changed V0.9.1 (2000-02-01) [umoeller]: added global hotkeys flag
 *@@changed V0.9.1 (2000-02-09) [umoeller]: fixed mutex hangs while dialogs were displayed
 *@@changed V0.9.2 (2000-03-19) [umoeller]: "Undo" and "Default" created duplicate records; fixed
 */

MRESULT setFeaturesItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                               USHORT usItemID, USHORT usNotifyCode,
                               ULONG ulExtra)      // for checkboxes: contains new state
{
    // lock global settings to get write access;
    // WARNING: do not show any dialogs when reacting to
    // controls BEFORE these are not unlocked!!!
    GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(5000);

    BOOL fSave = TRUE;

    // flags for delayed dialog showing (after unlocking)
    BOOL fShowHookInstalled = FALSE,
         fShowHookDeinstalled = FALSE,
         fShowClassesSetup = FALSE,
         fShowWarnExtAssocs = FALSE,
         fUpdateMouseMovementPage = FALSE;
    signed char cAskSoundsInstallMsg = -1,      // 1 = installed, 0 = deinstalled
                cEnableTrashCan = -1;       // 1 = installed, 0 = deinstalled
    ULONG ulUpdateFlags = 0;

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
            if (hifEnableHook(ulExtra) == ulExtra)
            {
                // success:
                pGlobalSettings->fEnableXWPHook = ulExtra;

                if (ulExtra)
                    fShowHookInstalled = TRUE;
                else
                    fShowHookDeinstalled = TRUE;

                // re-enable controls on this page
                ulUpdateFlags = CBI_SET | CBI_ENABLE;
            }
        break; }

        case ID_XCSI_REPLACEFILEPAGE:
            pGlobalSettings->fReplaceFilePage = ulExtra;
        break;

        case ID_XCSI_XSYSTEMSOUNDS:
            pGlobalSettings->fXSystemSounds = ulExtra;
            // check if sounds are to be installed or de-installed:
            if (sndAddtlSoundsInstalled() != ulExtra)
                // yes: set msg for "ask for sound install"
                // at the bottom when the global semaphores
                // are unlocked
                cAskSoundsInstallMsg = (ulExtra);
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

        case ID_XCSI_GLOBALHOTKEYS:
            hifEnableObjectHotkeys(ulExtra);
        break;

        case ID_XCSI_PAGEMAGE:
            if (hifEnablePageMage(ulExtra) == ulExtra)
            {
                pGlobalSettings->fEnablePageMage = ulExtra;
                ulUpdateFlags = CBI_SET | CBI_ENABLE;
                // update "Mouse movement" page
                fUpdateMouseMovementPage = TRUE;
            }
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
            // re-enable controls on this page
            ulUpdateFlags = CBI_ENABLE;
            if (ulExtra)
                // show warning at the bottom (outside the
                // mutex section)
                fShowWarnExtAssocs = TRUE;
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

        case ID_XCSI_XWPTRASHCAN:
            // pGlobalSettings->fTrashDelete = ulExtra;
            cEnableTrashCan = ulExtra;
        break;

        case ID_XCSI_REPLACEDELETE:
            pGlobalSettings->fReplaceTrueDelete = ulExtra;
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
    else if (fShowWarnExtAssocs)
        cmnMessageBox(pcnbp->hwndFrame,
                      "XWorkplace",
                      "Warning: Extended file associations don't work properly yet. Daily use is NOT recommended.",
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
            sndInstallAddtlSounds(ulExtra);
        }
    }
    else if (cEnableTrashCan != -1)
    {
        fopsEnableTrashCan(pcnbp->hwndFrame,
                           ulExtra);
        (*(pcnbp->pfncbInitPage))(pcnbp, CBI_SET | CBI_ENABLE);
    }

    if (ulUpdateFlags)
        (*(pcnbp->pfncbInitPage))(pcnbp, ulUpdateFlags);

    if (fUpdateMouseMovementPage)
        // update "Mouse movement" page
        ntbUpdateVisiblePage(NULL,
                             SP_MOUSE_MOVEMENT);

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
                        HWND         hwndCnr = WinWindowFromID(pcnbp->hwndDlgPage,
                                                               ID_XCDI_CONTAINER);
                        POINTL       ptlMouse;

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
 *@@changed V0.9.2 (2000-02-20) [umoeller]: changed build string handling
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
            pMsg += sprintf(pMsg, "pXFldStartup: 0x%lX\n", pKernelGlobals->pXFldStartup);
            pMsg += sprintf(pMsg, "pXFldShutdown: 0x%lX\n", pKernelGlobals->pXFldShutdown);
            pMsg += sprintf(pMsg, "pXWPClassList: 0x%lX\n", pKernelGlobals->pXWPClassList);
            pMsg += sprintf(pMsg, "pXWPString: 0x%lX\n", pKernelGlobals->pXWPString);

            DebugBox("XWorkplace Class Objects", szMsg);
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

        // kernel version number
        strcpy(szSearchMask, XFOLDER_VERSION);
        // append kernel build (automatically increased with each build
        // by xfldr.mak; string resource in xfldr.rc)
        if (WinLoadString(hab,
                          cmnQueryMainModuleHandle(),       // main module (XFLDR.DLL)
                          ID_XSSI_KERNEL_BUILD,
                          sizeof(szTemp),
                          szTemp))
            sprintf(szSearchMask+strlen(szSearchMask), ", build %s (%s)", szTemp, __DATE__);

        WinSetDlgItemText(pcnbp->hwndDlgPage,
                          ID_XCDI_INFO_KERNEL_RELEASE,
                          szSearchMask);

        // C runtime locale
        /* WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XCDI_INFO_KERNEL_LOCALE,
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
                       ? "Speedy thread crashed"
               : "unknown"
               );
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XCDI_INFO_SOUNDSTATUS,
                          szSearchMask);

        // language drop-down box
        WinSendDlgItemMsg(pcnbp->hwndDlgPage, ID_XCDI_INFO_LANGUAGE, LM_DELETEALL, 0, 0);

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
                DebugBox(pcnbp->hwndFrame,
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
                    DebugBox(pcnbp->hwndFrame,
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
        } // end if (cmnQueryXFolderBasePath...

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
                        (*(pcnbp->pfncbInitPage))(pcnbp, CBI_SET | CBI_ENABLE);
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
    WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XCDI_INFO_AWAKEOBJECTS,
                    szTemp);

    if (DosGetInfoBlocks(&ptib, &ppib) == NO_ERROR)
    {
        PRCPROCESS       prcp;
        // WPS thread count
        prc16QueryProcessInfo(ppib->pib_ulpid, &prcp);
        WinSetDlgItemShort(pcnbp->hwndDlgPage, ID_XCDI_INFO_WPSTHREADS,
                           prcp.usThreads,
                           FALSE);  // unsigned

        // Worker thread status
        tid = thrQueryID(pKernelGlobals->ptiWorkerThread);
        sprintf(szTemp, "TID 0x%lX, prty 0x%04lX, %d msgs",
                tid,
                prc16QueryThreadPriority(ppib->pib_ulpid, tid),
                pKernelGlobals->ulWorkerMsgCount);
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XCDI_INFO_WORKERSTATUS,
                          szTemp);

        // File thread status
        tid = thrQueryID(pKernelGlobals->ptiFileThread);
        sprintf(szTemp, "TID 0x%lX, prty 0x%04lX",
                tid,
                prc16QueryThreadPriority(ppib->pib_ulpid, tid));
        if (xthrIsFileThreadBusy())
            strcat(szTemp, ", busy");
        else
            strcat(szTemp, ", idle");
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XCDI_INFO_FILESTATUS,
                        szTemp);

        // Quick thread status
        tid = thrQueryID(pKernelGlobals->ptiSpeedyThread);
        sprintf(szTemp, "TID 0x%lX, prty 0x%04lX",
                tid,
                prc16QueryThreadPriority(ppib->pib_ulpid, tid));
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XCDI_INFO_QUICKSTATUS,
                        szTemp);
    }

    // XWPHook status
    {
        PDAEMONSHARED pDaemonShared = pKernelGlobals->pDaemonShared;
        PSZ         psz = "Disabled";
        if (pDaemonShared)
        {
            // WPS restarts V0.9.1 (99-12-29) [umoeller]
            WinSetDlgItemShort(pcnbp->hwndDlgPage, ID_XCDI_INFO_WPSRESTARTS,
                               pDaemonShared->ulWPSStartupCount,
                               FALSE);  // unsigned

            if (pDaemonShared->fAllHooksInstalled)
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
        xstrcat(&pszMenuItemText, " (");
        xstrcat(&pszMenuItemText, pso2->pszObjectClass);

        if (pObject != NULL)
        {
            // object found:
            // append the path to the menu item
            WPFolder    *pFolder = _wpQueryFolder(pObject);
            CHAR        szFolderPath[CCHMAXPATH] = "";
            _wpQueryFilename(pFolder, szFolderPath, TRUE);      // fully qualified
            xstrcat(&pszMenuItemText, ", ");
            xstrcat(&pszMenuItemText, szFolderPath);

            // disable menu item
            WinEnableMenuItem(hwndMenu, pso2->usMenuID, FALSE);
        }

        xstrcat(&pszMenuItemText, ")");
        if (pObject == NULL)
            // append "...", because we'll have a message box then
            xstrcat(&pszMenuItemText, "...");

        // on Warp 3, disable WarpCenter also
        if (   (!doshIsWarp4())
            && (strcmp(pso2->pszDefaultID, "<WP_WARPCENTER>") == 0)
           )
            WinEnableMenuItem(hwndMenu, pso2->usMenuID, FALSE);

        WinSetMenuItemText(hwndMenu, pso2->usMenuID, pszMenuItemText);
        free(pszMenuItemText);

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
            HWND    hwndMenu = WinLoadMenu(WinWindowFromID(pcnbp->hwndDlgPage, usItemID),
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
            HWND    hwndMenu = WinLoadMenu(WinWindowFromID(pcnbp->hwndDlgPage, usItemID),
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
            if (    (usItemID >= OBJECTSIDFIRST)
                 && (usItemID <= OBJECTSIDLAST)
               )
            {
                if (!setCreateStandardObject(pcnbp->hwndFrame,
                                             usItemID,
                                             FALSE))
                    // WPS objects not found:
                    // try XWPS objects
                    setCreateStandardObject(pcnbp->hwndFrame,
                                            usItemID,
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
                           0, 6);      // six pixels high
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
            pGlobalSettings->fNoSubclassing   = pGSBackup->fNoSubclassing;
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


