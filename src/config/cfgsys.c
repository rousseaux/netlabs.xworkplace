
/*
 *@@sourcefile cfgsys.c:
 *      implementation code for the CONFIG.SYS pages
 *      in the "OS/2 Kernel" object (XFldSystem).
 *
 *      This file is ALL new with V0.9.0. The code in
 *      this file used to be in main\xfsys.c.
 *
 *      Function prefix for this file:
 *      --  cfg*
 *
 *      Most of the functions in this file starting with fncb* are
 *      callbacks specified with ntbInsertPage (notebook.c). There are
 *      two callbacks for each notebook page in "OS/2 Kernel", one
 *      for (re)initializing the page's controls and one for reacting
 *      to controls being changed by the user.
 *      These callbacks are specified in xfsys::xwpAddXFldSystemPages.
 *      These callbacks are all new with V0.82 and replace the awful
 *      dialog procedures which were previously used, because these
 *      became hard to maintain over time.
 *
 *@@added V0.9.0 [umoeller]
 *@@header "config\cfgsys.h"
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

#define INCL_DOSSEMAPHORES
#define INCL_DOSERRORS
#define INCL_DOSMISC

#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WINMENUS
#define INCL_WINDIALOGS
#define INCL_WINPOINTERS
#define INCL_WINENTRYFIELDS
#define INCL_WINBUTTONS
#define INCL_WINLISTBOXES
#define INCL_WINMLE
#define INCL_WINSTDSPIN
#define INCL_WINSTDSLIDER
#define INCL_WINSTDFILE
#define INCL_WINSTDCNR
#define INCL_WINSTDDRAG

#define INCL_WINPROGRAMLIST     // needed for PROGDETAILS, wppgm.h

#include <os2.h>

// C library headers
#include <stdio.h>              // needed for except.h
#include <ctype.h>
#include <direct.h>

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\configsys.h"          // CONFIG.SYS routines
#include "helpers\cnrh.h"               // container helper routines
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\level.h"              // SYSLEVEL helpers
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\procstat.h"           // DosQProcStat handling
#include "helpers\stringh.h"            // string helper routines
#include "helpers\textview.h"           // PM XTextView control
#include "helpers\threads.h"            // thread helpers
#include "helpers\winh.h"               // PM helper routines
#include "helpers\xstring.h"            // extended string helpers

// SOM headers which don't crash with prec. header files
#include "xfsys.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\cnrsort.h"             // container sort comparison functions
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling

#include "filesys\xthreads.h"           // extra XWorkplace threads

#include "config\cfgsys.h"              // XFldSystem CONFIG.SYS pages implementation
#include "config\drivdlgs.h"            // driver configuration dialogs

// other SOM headers
#pragma hdrstop                 // VAC++ keeps crashing otherwise

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

// some global variables for "Kernel" pages
CHAR    G_aszAllDrives[30][5];  // 30 strings with 5 chars each for spin button
PSZ     G_apszAllDrives[30];    // 30 pointers to the buffers
LONG    G_lDriveCount = 0;
HWND    G_hwndDlgDoubleFiles = NULLHANDLE;  // V0.9.9 (2001-02-28) [pr]

// #define SYSPATHCOUNT 5
PSZ G_apszPathNames[] =
        {
            "LIBPATH=",
            "SET PATH=",
            "SET DPATH=",
            "SET BOOKSHELF=",
            "SET HELP=",
            "SET CLASSPATH=",   // V0.9.9 (2001-03-06) [pr]
            "SET INCLUDE=",
            "SET LIB=",
            "SET SMINCLUDE="
        };

typedef struct _SYSPATH
{
    PSZ         pszPathType;
            // "LIBPATH", "SET PATH", etc.;
            // this is a pointer into the static apszPathNames[]
            // array, so this must not be free()'d
    PLINKLIST   pllPaths;
            // linked list of PSZs with the path entries
} SYSPATH, *PSYSPATH;

/*
 * pllSysPathsList:
 *      Each item in this list is a SYSPATH structure,
 *      which in turn contains a linked list of path entries.
 */

PLINKLIST   G_pllSysPathsList;

// the currently selected SYSPATH
PSYSPATH    G_pSysPathSelected = 0;

CHAR        G_szSwapperFilename[CCHMAXPATH] = "";

/* ******************************************************************
 *
 *   External APIs
 *
 ********************************************************************/

/*
 *@@ cfgParseSwapPath:
 *
 *@@added V0.9.9 (2001-02-08) [umoeller]
 */

BOOL cfgParseSwapPath(const char *pcszConfigSys,    // in: if NULL, this gets loaded
                      PSZ pszSwapPath,              // out: swapper directory
                      PULONG pulMinFree,            // out: min free
                      PULONG pulMinSize)            // out: min size
{
    BOOL brc = FALSE;

    PSZ pszConfigSysTemp = 0;

    if (!pcszConfigSys)
    {
        // not specified: load it
        if (csysLoadConfigSys(NULL, &pszConfigSysTemp) == NO_ERROR)
            pcszConfigSys = pszConfigSysTemp;
    }

    if (pcszConfigSys)
    {
        // parse SWAPPATH command
        PSZ p;
        if (p = csysGetParameter(pcszConfigSys, "SWAPPATH=", NULL, 0))
        {
            CHAR    szSwap[CCHMAXPATH];
            ULONG   ulMinFree = 2048, ulMinSize = 2048;
            // int     iScanned;
            sscanf(p,
                   "%s %d %d",
                   &szSwap, &ulMinFree, &ulMinSize);

            if (pszSwapPath)
                strcpy(pszSwapPath, szSwap);
            if (pulMinFree)
                *pulMinFree = ulMinFree;
            if (pulMinSize)
                *pulMinSize = ulMinSize;

            if (G_szSwapperFilename[0] == '\0')
            {
                // first call: copy to global so that the swapper
                // monitors will always use the old one, in case
                // the user changes this
                strcpy(G_szSwapperFilename, szSwap);
                if (G_szSwapperFilename[strlen(G_szSwapperFilename)-1] != '\\')
                    strcat(G_szSwapperFilename, "\\");
                strcat(G_szSwapperFilename, "swapper.dat");
            }

            brc = TRUE;
        }
    }

    if (pszConfigSysTemp)
        free(pszConfigSysTemp);

    return (brc);
}

/*
 *@@ cfgQuerySwapperSize:
 *
 *@@added V0.9.9 (2001-02-08) [umoeller]
 */

ULONG cfgQuerySwapperSize(VOID)
{
    ULONG ulrc = 0;

    if (G_szSwapperFilename[0] == '\0')
    {
        // first call: compose the path
        cfgParseSwapPath(NULL,
                         NULL,
                         NULL,
                         NULL);
    }

    if (G_szSwapperFilename[0])
    {
        ulrc = doshQueryPathSize(G_szSwapperFilename);
    }

    return (ulrc);
}

/* ******************************************************************
 *
 *   Notebook callbacks (notebook.c)
 *
 ********************************************************************/

/*
 *  All the following functions starting with fncbConfig* are callbacks
 *  for the common notebook dlg function in notebook.c. There are
 *  two callbacks for each notebook page in "OS/2 Kernel", one
 *  for (re)initializing the page's controls and one for reacting
 *  to controls being changed by the user.
 *  These callbacks are specified in xfsys::xwpAddXFldSystemPages.
 *  These callbacks are all new with V0.82 and replace the awful
 *  dialog procedures which were previously used, because these
 *  became hard to maintain over time.
 */

/*
 *@@ fnwpNewSystemPathDlg:
 *      dialog proc for "New system path" dialog, which
 *      pops up when the "New" button is pressed on the
 *      "System paths" page (cfgConfigItemChanged).
 *
 *      This dlg procedure expects a pointer to a buffer
 *      as a creation parameter (pCreateParams with
 *      WinLoadDlg). When "OK" is pressed, this func
 *      copies the path that was entered into that buffer,
 *      which should be CCHMAXPATH in size.
 */

MRESULT EXPENTRY fnwpNewSystemPathDlg(HWND hwndDlg,
                                      ULONG msg,
                                      MPARAM mp1,
                                      MPARAM mp2)
{
    MRESULT mrc = (MRESULT)0;

    switch (msg)
    {
        case WM_INITDLG:
            WinEnableControl(hwndDlg, DID_OK, FALSE);
            WinSendDlgItemMsg(hwndDlg, ID_XSDI_FT_ENTRYFIELD,
                              EM_SETTEXTLIMIT,
                              (MPARAM)250,
                              MPNULL);
            WinSetDlgItemText(hwndDlg, ID_XSDI_FT_ENTRYFIELD, mp2); // V0.9.9 (2001-02-28) [pr]

            // store pointer to buffer in window words
            // for later
            WinSetWindowPtr(hwndDlg, QWL_USER,
                            mp2);           // pCreateParam == pszBuffer here

            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
        break;

        /*
         * WM_CONTROL:
         *
         */

        case WM_CONTROL:
            switch (SHORT1FROMMP(mp1))  // source id
            {
                case ID_XSDI_FT_ENTRYFIELD:
                    if (SHORT2FROMMP(mp1) == EN_CHANGE)
                    {
                        // content of entry field has changed:
                        // update "OK" button, if path exists
                        CHAR szNewSysPath[CCHMAXPATH];
                        WinQueryDlgItemText(hwndDlg,
                                            ID_XSDI_FT_ENTRYFIELD,
                                            sizeof(szNewSysPath)-1, szNewSysPath);
                        WinEnableControl(hwndDlg,
                                          DID_OK,
                                          doshQueryDirExist(szNewSysPath));
                    }
                break;

                default:
                    mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
            }
        break;

        /*
         * WM_COMMAND:
         *
         */

        case WM_COMMAND:
            switch (SHORT1FROMMP(mp1))  // source id
            {
                // "Browse..." button: show file dlg
                case ID_XLDI_BROWSE:
                {
                    FILEDLG fd;
                    memset(&fd, 0, sizeof(FILEDLG));
                    fd.cbSize = sizeof(FILEDLG);
                    fd.fl = FDS_OPEN_DIALOG
                              | FDS_CENTER;
                    // fd.pfnDlgProc = fnwpOpenFilter;
                    strcpy(fd.szFullFile, "*");
                    if (    WinFileDlg(HWND_DESKTOP,    // parent
                                       hwndDlg,
                                       &fd)
                        && (fd.lReturn == DID_OK)
                       )
                    {
                        WinSetDlgItemText(hwndDlg, ID_XLDI_CLASSMODULE,
                                    fd.szFullFile);
                    }
                break; }

                case DID_OK:
                {
                    PSZ pszBuffer = WinQueryWindowPtr(hwndDlg, QWL_USER);
                    if (pszBuffer)
                        WinQueryDlgItemText(hwndDlg,
                                            ID_XSDI_FT_ENTRYFIELD,
                                            CCHMAXPATH,     // expected size of buffer
                                            pszBuffer);
                    mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
                break; }

                default:
                    mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
            }
        break;

        default:
            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
    }

    return (mrc);
}

/*
 *@@ FILERECORD:
 *      extended record core used on "Double files"
 *      dialog (fnwpDoubleFilesDlg).
 */

typedef struct _FILERECORD
{
    RECORDCORE  recc;               // standard record core
    PSZ         pszFilename;        // filename
    PSZ         pszDirName;         // directory name
    ULONG       ulSize;             // file size
    CDATE       cDate;              // file date
    CTIME       cTime;              // file time
} FILERECORD, *PFILERECORD;

/*
 *@@ DOUBLEFILESWINDATA:
 *      stored in QWL_USER of fnwpDoubleFilesDlg.
 */

typedef struct _DOUBLEFILESWINDATA
{
    DOUBLEFILES     df;
    XADJUSTCTRLS    xac;
    HWND            hwndCaller; // V0.9.9 (2001-02-28) [pr]
} DOUBLEFILESWINDATA, *PDOUBLEFILESWINDATA;

/*
 * ampDoubleFilesControls:
 *      array of controls flags for winhAdjustControls.
 */

MPARAM ampDoubleFilesControls[] =
    {
        MPFROM2SHORT(ID_OSDI_FILELISTSYSPATH1,  XAC_MOVEY),
        MPFROM2SHORT(ID_OSDI_FILELISTSYSPATH2,  XAC_MOVEY | XAC_SIZEX),
        MPFROM2SHORT(ID_OSDI_FILELISTCNR,       XAC_SIZEX | XAC_SIZEY),
        MPFROM2SHORT(DID_OK,                    XAC_MOVEX)
    };

/*
 *@@ fnwpDoubleFilesDlg:
 *      window procedure for "Double files" dialog
 *      opened when the "Double files" button is
 *      pressed on the "System paths" page
 *      (cfgConfigItemChanged).
 *
 *      This thing interoperates with the File thread
 *      to have the double files collected.
 *
 *@@changed V0.9.9 (2001-02-28) [pr]: made this modal
 */

MRESULT EXPENTRY fnwpDoubleFilesDlg(HWND hwndDlg,
                                    ULONG msg,
                                    MPARAM mp1,
                                    MPARAM mp2)
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
            XFIELDINFO      xfi[5];
            PFIELDINFO      pfi = NULL;
            int             i = 0;

            HWND            hwndCnr = WinWindowFromID(hwndDlg, ID_OSDI_FILELISTCNR);

            PDOUBLEFILESWINDATA pWinData = malloc(sizeof(DOUBLEFILESWINDATA));
            memset(pWinData, 0, sizeof(DOUBLEFILESWINDATA));
            WinSetWindowPtr(hwndDlg, QWL_USER, pWinData);
            pWinData->hwndCaller = (HWND) mp2; // V0.9.9 (2001-02-28) [pr]

            // set up cnr details view
            xfi[i].ulFieldOffset = FIELDOFFSET(RECORDCORE, pszIcon);
            xfi[i].pszColumnTitle = "File name";
            xfi[i].ulDataType = CFA_STRING;
            xfi[i++].ulOrientation = CFA_LEFT;

            xfi[i].ulFieldOffset = FIELDOFFSET(FILERECORD, pszDirName);
            xfi[i].pszColumnTitle = "Directory";
            xfi[i].ulDataType = CFA_STRING;
            xfi[i++].ulOrientation = CFA_LEFT;

            xfi[i].ulFieldOffset = FIELDOFFSET(FILERECORD, ulSize);
            xfi[i].pszColumnTitle = "Size";
            xfi[i].ulDataType = CFA_ULONG;
            xfi[i++].ulOrientation = CFA_RIGHT;

            xfi[i].ulFieldOffset = FIELDOFFSET(FILERECORD, cDate);
            xfi[i].pszColumnTitle = "Date";
            xfi[i].ulDataType = CFA_DATE;
            xfi[i++].ulOrientation = CFA_LEFT;

            xfi[i].ulFieldOffset = FIELDOFFSET(FILERECORD, cTime);
            xfi[i].pszColumnTitle = "Time";
            xfi[i].ulDataType = CFA_TIME;
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
                cnrhSetSplitBarPos(100);
                cnrhSetSortFunc(fnCompareName);
            } END_CNRINFO(hwndCnr);

            // set title
            WinSetDlgItemText(hwndDlg, ID_OSDI_FILELISTSYSPATH2,
                              G_pSysPathSelected->pszPathType);

            // have file thread collect the files
            pWinData->df.pllDirectories = G_pSysPathSelected->pllPaths;
            pWinData->df.hwndNotify = hwndDlg;
            pWinData->df.ulNotifyMsg = XM_UPDATE;
            xthrPostFileMsg(FIM_DOUBLEFILES,
                            (MPARAM)&(pWinData->df),
                            (MPARAM)0);

            // set clip children flag
            WinSetWindowBits(hwndDlg,
                             QWL_STYLE,
                             WS_CLIPCHILDREN,         // unset bit
                             WS_CLIPCHILDREN);

            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
        break; }

        /*
         * XM_UPDATE:
         *      posted back to us from the File thread
         *      when it's done collecting the files.
         */

        case XM_UPDATE:
        {
            PDOUBLEFILESWINDATA pWinData = (PDOUBLEFILESWINDATA)WinQueryWindowPtr(hwndDlg,
                                                                                  QWL_USER);
            ULONG           ulItems = lstCountItems(pWinData->df.pllDoubleFiles);
            PLISTNODE       pNode = lstQueryFirstNode(pWinData->df.pllDoubleFiles);
            HWND            hwndCnr = WinWindowFromID(hwndDlg, ID_OSDI_FILELISTCNR);
            PFILERECORD     preccFirst = (PFILERECORD)cnrhAllocRecords(hwndCnr,
                                                                       sizeof(FILERECORD),
                                                                       ulItems),
                            preccThis = preccFirst;
            if (preccThis)
            {
                while (pNode)
                {
                    PFILELISTITEM pfli = (PFILELISTITEM)pNode->pItemData;
                    preccThis->recc.pszIcon = pfli->szFilename;
                    preccThis->pszDirName = pfli->pszDirectory;
                    preccThis->ulSize = pfli->ulSize;
                    cnrhDateDos2Win(&pfli->fDate, &preccThis->cDate);
                    cnrhTimeDos2Win(&pfli->fTime, &preccThis->cTime);

                    pNode = pNode->pNext;
                    preccThis = (PFILERECORD)preccThis->recc.preccNextRecord;
                }

                cnrhInsertRecords(hwndCnr,
                                  NULL,
                                  (PRECORDCORE)preccFirst,
                                  TRUE, // invalidate
                                  NULL,
                                  CRA_RECORDREADONLY,
                                  ulItems);
            }
        break; }

        /*
         * WM_WINDOWPOSCHANGED:
         *      posted _after_ the window has been moved
         *      or resized.
         *      Since we have a sizeable dlg, we need to
         *      update the controls' sizes also, or PM
         *      will display garbage. This is the trick
         *      how to use sizeable dlgs, because these do
         *      _not_ get sent WM_SIZE messages.
         */

        case WM_WINDOWPOSCHANGED:
        {
            // this msg is passed two SWP structs:
            // one for the old, one for the new data
            // (from PM docs)
            PSWP pswpNew = PVOIDFROMMP(mp1);
            // PSWP pswpOld = pswpNew + 1;

            // resizing?
            if (pswpNew->fl & SWP_SIZE)
            {
                PDOUBLEFILESWINDATA pWinData = (PDOUBLEFILESWINDATA)WinQueryWindowPtr(hwndDlg,
                                                                                      QWL_USER);
                if (pWinData)
                {
                    PXADJUSTCTRLS pxac = &pWinData->xac;

                    winhAdjustControls(hwndDlg,
                                       ampDoubleFilesControls,
                                       sizeof(ampDoubleFilesControls) / sizeof(MPARAM),
                                       pswpNew,
                                       pxac);
                }
            }
            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
        break; }

        /*
         * WM_COMMAND:
         *
         * added V0.9.9 (2001-02-28) [pr]
         */

        case WM_COMMAND:
            /* switch (SHORT1FROMMP(mp1))  // source id
            {
                case DID_OK: */

            // V0.9.9 (2001-03-07) [umoeller]:
            // nope, paul... we must always destroy the window. If we let this
            // slip thru to the default window proc, pressing "esc" in the
            // dialog will dismiss the dialog, but never destroy it.

            WinDestroyWindow(hwndDlg);
        break;

        /*
         * WM_DESTROY:
         *
         */

        case WM_DESTROY:
        {
            PDOUBLEFILESWINDATA pWinData = (PDOUBLEFILESWINDATA)WinQueryWindowPtr(hwndDlg,
                                                                                  QWL_USER);
            lstFree(pWinData->df.pllDoubleFiles);
            winhAdjustControls(hwndDlg,
                               NULL, // cleanup
                               sizeof(ampDoubleFilesControls) / sizeof(MPARAM),
                               0,
                               &pWinData->xac);
            // V0.9.9 (2001-02-28) [pr]
            G_hwndDlgDoubleFiles = NULLHANDLE;
            WinEnableWindow(WinWindowFromID(pWinData->hwndCaller,
                                            ID_OSDI_DOUBLEFILES),
                            TRUE);
            free(pWinData);
            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
        break; }

        default:
            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
    }

    return (mrc);
}

/*
 *@@ cfgConfigInitPage:
 *      common notebook callback function (notebook.c) for
 *      all the notebook pages dealing with CONFIG.SYS settings.
 *      Sets the controls on the page according to the CONFIG.SYS
 *      statements.
 *
 *      Since this callback is shared among all the CONFIG.SYS
 *      pages, pcnbp->ulPageID is used for telling them apart by
 *      using the SP_* identifiers.
 *
 *@@changed V0.9.0 [umoeller]: added "System paths" page
 *@@changed V0.9.0 [umoeller]: adjusted function prototype
 *@@changed V0.9.7 (2000-12-17) [umoeller]: moved config.sys path composition to krnInitializeXWorkplace
 *@@changed V0.9.7 (2000-12-17) [umoeller]: raised buffer size for syspaths page
 *@@changed V0.9.7 (2001-01-17) [umoeller]: changed QSV_MAX compile problems; thanks, Martin Lafaix
 *@@changed V0.9.9 (2001-02-28) [pr]: added "edit path"
 */

VOID cfgConfigInitPage(PCREATENOTEBOOKPAGE pcnbp,
                        ULONG flFlags)  // notebook info struct
{
    PSZ     pszConfigSys = NULL;

    if (flFlags & CBI_INIT)
    {
        WinEnableControl(pcnbp->hwndDlgPage, DID_APPLY, TRUE);

        // on the "HPFS" page:
        // if the system has any HPFS drives,
        // we disable the "HPFS installed" item
        /* if (pcnbp->ulPageID == SP_HPFS)
        {
            CHAR szHPFSDrives[30];
            doshEnumDrives(szHPFSDrives,
                           "HPFS",
                           TRUE); // skip removeable drives
            if (strlen(szHPFSDrives) > 0)
                WinEnableControl(pcnbp->hwndDlgPage, ID_OSDI_FSINSTALLED, FALSE);
        }
        else */
        if (pcnbp->ulPageID == SP_ERRORS)
        {
            CHAR szAllDrives[30];
            PSZ p = szAllDrives;
            ULONG ul = 0;
            doshEnumDrives(szAllDrives,
                           NULL, // all drives
                           TRUE); // skip removeable drives

            while (*p)
            {
                G_aszAllDrives[ul][0] = szAllDrives[ul];
                G_aszAllDrives[ul][1] = '\0';
                G_apszAllDrives[ul] = &(G_aszAllDrives[ul][0]);
                p++;
                ul++;
            }

            G_aszAllDrives[ul][0] = '0';
            G_aszAllDrives[ul][1] = 0;
            G_apszAllDrives[ul] = &(G_aszAllDrives[ul][0]);
            G_lDriveCount = ul;
            ul++;
            WinSendDlgItemMsg(pcnbp->hwndDlgPage, ID_OSDI_SUPRESSP_DRIVE,
                              SPBM_SETARRAY,
                              (MPARAM)G_apszAllDrives,
                              (MPARAM)ul);
        }
    }

    if (flFlags & CBI_SET)
    {
        PCKERNELGLOBALS pKernelGlobals = krnQueryGlobals();

        HPOINTER hptrOld = winhSetWaitPointer();

        // now read CONFIG.SYS file to initialize the dlg items
        if (csysLoadConfigSys(NULL, &pszConfigSys) != NO_ERROR)
            winhDebugBox(pcnbp->hwndFrame,
                         "XWorkplace",
                         "XWorkplace was unable to open the CONFIG.SYS file.");
        else
        {
            // OK, file read successfully:
            switch (pcnbp->ulPageID)
            {
                /*
                 * SP_SCHEDULER:
                 *
                 */

                case SP_SCHEDULER:
                {
                    PSZ     p = 0;
                    ULONG   ul = 0;
                    BOOL    bl = TRUE;
                    if (p = csysGetParameter(pszConfigSys, "THREADS=", NULL, 0))
                        sscanf(p, "%d", &ul);
                    else // default
                        ul = 64;
                    winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_OSDI_MAXTHREADS,
                                           64, 4096,
                                           ul);

                    if (p = csysGetParameter(pszConfigSys, "MAXWAIT=", NULL, 0))
                        sscanf(p, "%d", &ul);
                    else // default
                        ul = 3;
                    winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_OSDI_MAXWAIT,
                                           1, 10,
                                           ul);

                    if (p = csysGetParameter(pszConfigSys, "PRIORITY_DISK_IO=", NULL, 0))
                        bl = (strncmp(p, "YES", 3) == 0);

                    winhSetDlgItemChecked(pcnbp->hwndDlgPage,
                                          ID_OSDI_PRIORITYDISKIO,
                                          bl);      // defaults to YES
                break; }

                /*
                 * SP_MEMORY:
                 *
                 */

                case SP_MEMORY:
                {
                    // installed physical memory
                    CHAR    szMemory[30];
                    ULONG   ulTotPhysMem = 0;
                    PSZ     p = 0;
                    CHAR    szSwapPath[CCHMAXPATH] = "Error";
                    ULONG   ulMinFree = 2048, ulMinSize = 2048;

                    DosQuerySysInfo(QSV_TOTPHYSMEM,     // changed V0.9.7 (2001-01-17) [umoeller]
                                    QSV_TOTPHYSMEM,
                                    &ulTotPhysMem,
                                    sizeof(ulTotPhysMem));
                    sprintf(szMemory, "%d",
                            ((ulTotPhysMem / 1024) + 512) / 1024); // V0.9.9 (2001-02-28) [pr]
                    WinSetDlgItemText(pcnbp->hwndDlgPage, ID_OSDI_PHYSICALMEMORY, szMemory);

                    // parse SWAPPATH command
                    if (cfgParseSwapPath(pszConfigSys,
                                         szSwapPath,
                                         &ulMinFree,
                                         &ulMinSize))
                    {
                        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_OSDI_SWAPPATH, szSwapPath);
                        winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_OSDI_MINSWAPSIZE,
                                               2, 100,
                                               (ulMinSize / 1024));
                        winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_OSDI_MINSWAPFREE,
                                               2, 1000,
                                               (ulMinFree / 1024));
                    }
                break; }

                /*
                 * SP_HPFS:
                 *
                 */

                /* case SP_HPFS:
                {
                    PSZ     p = 0;
                    CHAR    szParameter[300] = "",
                            // szTemp[300] = "",
                            szSearchKey[100],
                            szAutoCheck[200] = "";
                    ULONG   ulCacheSize = 0,
                            ulThreshold = 4,
                            ulMaxAge = 5000,
                            ulDiskIdle = 1000,
                            ulBufferIdle = 500;
                    BOOL    fLazyWrite = TRUE;

                    szAutoCheck[0] = doshQueryBootDrive();  // default value

                    // evaluate IFS=...\HPFS.IFS
                    sprintf(szSearchKey, "IFS=%c:\\OS2\\HPFS.IFS ", doshQueryBootDrive());
                    p = csysGetParameter(pszConfigSys, szSearchKey,
                            szParameter, sizeof(szParameter));

                    if (p)
                    {
                        PSZ p2;
                        if (p2 = strhistr(szParameter, "/CACHE:"))
                            sscanf(p2+7, "%d", &ulCacheSize);
                        if (p2 = strhistr(szParameter, "/CRECL:"))
                            sscanf(p2+7, "%d", &ulThreshold);
                        if (p2 = strhistr(szParameter, "/AUTOCHECK:"))
                            sscanf(p2+11, "%s", &szAutoCheck);
                    }

                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_FSINSTALLED,
                                          (p != 0));
                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_CACHESIZE_AUTO,
                                          (ulCacheSize == 0));
                    winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_OSDI_CACHESIZE,
                                           0, 2048,
                                           ulCacheSize);
                    winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_OSDI_CACHE_THRESHOLD,
                                           4, 64,
                                           ulThreshold);
                    WinSetDlgItemText(pcnbp->hwndDlgPage, ID_OSDI_AUTOCHECK, szAutoCheck);

                    // evaluate
                    // RUN=...\CACHE.EXE /MAXAGE:60000 /DISKIDLE:1000 /BUFFERIDLE:40000
                    sprintf(szSearchKey, "RUN=%c:\\OS2\\CACHE.EXE ", doshQueryBootDrive());
                    p = csysGetParameter(pszConfigSys, szSearchKey,
                                szParameter, sizeof(szParameter));

                    if (p)
                    {
                        PSZ p2;
                        if (p2 = strhistr(szParameter, "/MAXAGE:"))
                            sscanf(p2+8,  "%d", &ulMaxAge);
                        if (p2 = strhistr(szParameter, "/DISKIDLE:"))
                            sscanf(p2+10, "%d", &ulDiskIdle);
                        if (p2 = strhistr(szParameter, "/BUFFERIDLE:"))
                            sscanf(p2+12, "%d", &ulBufferIdle);
                        if (strhistr(szParameter, "/LAZY:OFF"))
                            fLazyWrite = FALSE;
                    }

                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_CACHE_LAZYWRITE,
                                fLazyWrite);
                    winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_OSDI_CACHE_MAXAGE,
                                           500, 100*1000,
                                           ulMaxAge);
                    winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_OSDI_CACHE_BUFFERIDLE,
                                           500, 100*1000,
                                           ulBufferIdle);
                    winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_OSDI_CACHE_DISKIDLE,
                                           500, 100*1000,
                                           ulDiskIdle);

                break; } */

                /*
                 * SP_FAT:
                 *
                 */

                case SP_FAT:
                {
                    CHAR    szParameter[300] = "",
                            szAutoCheck[200] = "";
                    ULONG   ulCacheSize = 512,
                            ulThreshold = 4;
                    PSZ     p2 = 0;

                    // evaluate DISKCACHE
                    p2 = csysGetParameter(pszConfigSys, "DISKCACHE=",
                                          szParameter, sizeof(szParameter));

                    // enable "Cache installed" item if DISKCACHE= found
                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_FSINSTALLED,
                                          (p2 != NULL));

                    // now set the other items according to the DISKCACHE
                    // parameters; if that was not found, the default values
                    // above will be used
                    if (szParameter[0] == 'D')
                        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_CACHESIZE_AUTO,
                                              TRUE);
                    else
                        sscanf(szParameter, "%d", &ulCacheSize);

                    p2 = strchr(szParameter, ','); // get next parameter
                    // optional "LW" parameter (lazy write)
                    if (p2)
                    {
                        if (strncmp(p2+1, "LW", 2) == 0)
                        {
                            winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_CACHE_LAZYWRITE,
                                        TRUE);
                            p2 = strchr(p2+1, ','); // get next parameter
                        }
                    }
                    // optional threshold parameter
                    if (p2)
                    {
                        sscanf(p2+1, "%d", &ulThreshold);
                        p2 = strchr(p2+1, ','); // get next parameter
                    }
                    // optional "autocheck" parameter
                    if (p2)
                    {
                        if (strncmp(p2+1, "AC:", 3) == 0)
                            strcpy(szAutoCheck, p2+4);
                    }

                    winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_OSDI_CACHESIZE,
                                           0, 14400,
                                           ulCacheSize);
                    // the threshold param is in sectors of 512 bytes
                    // each, so for getting KB, we need to divide by 2
                    winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_OSDI_CACHE_THRESHOLD,
                                           4, 64,
                                           ulThreshold / 2);
                    WinSetDlgItemText(pcnbp->hwndDlgPage, ID_OSDI_AUTOCHECK, szAutoCheck);

                break; }

                /*
                 * SP_WPS:
                 *
                 */

                case SP_WPS:
                {
                    CHAR    szParameter[300] = "";
                    PSZ p = csysGetParameter(pszConfigSys, "SET AUTOSTART=",
                                             szParameter, sizeof(szParameter));
                    BOOL fAutoRefreshFolders = TRUE;
                    if (p)
                    {
                        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_AUTO_PROGRAMS,
                            (strhistr(szParameter, "PROGRAMS") != NULL));
                        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_AUTO_TASKLIST,
                            (strhistr(szParameter, "TASKLIST") != NULL));
                        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_AUTO_CONNECTIONS,
                            (strhistr(szParameter, "CONNECTIONS") != NULL));
                        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_AUTO_LAUNCHPAD,
                            (strhistr(szParameter, "LAUNCHPAD") != NULL));
                        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_AUTO_WARPCENTER,
                            (strhistr(szParameter, "WARPCENTER") != NULL));
                    }

                    p = csysGetParameter(pszConfigSys, "SET RESTARTOBJECTS=",
                            szParameter, sizeof(szParameter));
                    if ( (p == NULL) || (strhistr(szParameter, "YES")) )
                        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_RESTART_YES, TRUE);
                    else if (strhistr(szParameter, "NO"))
                        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_RESTART_NO, TRUE);
                    else if (strhistr(szParameter, "STARTUPFOLDERSONLY"))
                        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_RESTART_FOLDERS, TRUE);

                    if (strhistr(szParameter, "REBOOTONLY"))
                        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_RESTART_REBOOT, TRUE);

                    // auto-refresh folders: cannot be disabled on Warp 3
                    p = csysGetParameter(pszConfigSys, "SET AUTOREFRESHFOLDERS=",
                            szParameter, sizeof(szParameter));
                    if (p)
                        if (strhistr(szParameter, "NO"))
                            fAutoRefreshFolders = FALSE;
                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_AUTOREFRESHFOLDERS,
                                fAutoRefreshFolders);
                break; }

                /*
                 * SP_ERRORS:
                 *
                 */

                case SP_ERRORS:
                {
                    CHAR    szParameter[300] = "";
                    BOOL fAutoFail = FALSE,
                         fReIPL = FALSE;
                    PSZ p = csysGetParameter(pszConfigSys, "AUTOFAIL=",
                            szParameter, sizeof(szParameter));
                    if (p)
                        if (strhistr(szParameter, "YES"))
                            fAutoFail = TRUE;
                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_AUTOFAIL, fAutoFail);

                    p = csysGetParameter(pszConfigSys, "REIPL=",
                            szParameter, sizeof(szParameter));
                    if (p)
                        if (strhistr(szParameter, "ON"))
                            fReIPL = TRUE;
                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_REIPL, fReIPL);

                    p = csysGetParameter(pszConfigSys, "SUPPRESSPOPUPS=",
                            szParameter, sizeof(szParameter));
                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_SUPRESSPOPUPS,
                            (p != NULL));
                    if (p)
                    {
                        LONG lIndex = 0;           // default for "0" param
                        if (*p != '0')
                        {
                            CHAR c = toupper(*p);
                            lIndex = c-'C'; // 0 for C, 1 for D etc.
                        } else
                            // "0" character:
                            lIndex = G_lDriveCount;

                        WinSendDlgItemMsg(pcnbp->hwndDlgPage, ID_OSDI_SUPRESSP_DRIVE,
                                          SPBM_SETCURRENTVALUE,
                                          (MPARAM)lIndex,
                                          (MPARAM)NULL);
                    }

                break; }

                /*
                 * SP_SYSPATHS:
                 *      new with V0.9.0
                 */

                case SP_SYSPATHS:
                {
                    ULONG ul;

                    G_pllSysPathsList = lstCreate(FALSE);
                            // items are not freeable;
                            // we'll free this manually

                    for (ul = 0;
                         ul < ARRAYITEMCOUNT(G_apszPathNames);
                         ul++)
                    {
                        // create SYSPATH structure
                        PSYSPATH pSysPath = malloc(sizeof(SYSPATH));
                        CHAR szPaths[2000];     // raised V0.9.7 (2000-12-17) [umoeller]
                        PSZ p;

                        memset(szPaths, 0, sizeof(szPaths));
                        p = csysGetParameter(pszConfigSys,
                                             G_apszPathNames[ul],
                                             szPaths,
                                             sizeof(szPaths));
                        if (p)
                        {
                            // now szPaths has the path list
                            PSZ pStart = szPaths;
                            BOOL fContinue = TRUE;

                            // skip "="
                            while ((*pStart) && (*pStart == '='))
                                pStart++;
                            // skip leading spaces
                            while ((*pStart) && (*pStart == ' '))
                                pStart++;

                            pSysPath->pszPathType = G_apszPathNames[ul];
                            pSysPath->pllPaths = lstCreate(TRUE);
                                    // items are freeable;
                                    // this holds simple PSZ's

                            do {
                                PSZ pEnd = strchr(pStart, ';'),
                                    pszPath = 0;

                                if (pEnd == 0)
                                {
                                    pEnd = pStart + strlen(pStart);
                                    fContinue = FALSE;
                                }

                                if (pStart == pEnd)
                                    fContinue = FALSE;
                                else
                                {
                                    pszPath = strhSubstr(pStart, pEnd);

                                    if (pszPath)
                                        if (strlen(pszPath))
                                        {
                                            // store path (PSZ) in list in SYSPATH
                                            lstAppendItem(pSysPath->pllPaths, pszPath);
                                            pStart = pEnd+1;
                                        }
                                        else
                                        {
                                            free(pszPath);
                                            fContinue = FALSE;
                                        }
                                }
                            } while (fContinue);

                            // store SYSPATH in global list
                            lstAppendItem(G_pllSysPathsList, pSysPath);

                            WinSendDlgItemMsg(pcnbp->hwndDlgPage, ID_OSDI_PATHDROPDOWN,
                                              LM_INSERTITEM,
                                              (MPARAM)LIT_END,
                                              (MPARAM)pSysPath->pszPathType);
                        }
                    }

                    WinSendDlgItemMsg(pcnbp->hwndDlgPage, ID_OSDI_PATHDROPDOWN,
                                      LM_SELECTITEM,
                                      (MPARAM)0,        // select first
                                      (MPARAM)TRUE);    // select

                break; }

            } // end switch (pcnbp->ulPageID)

            free(pszConfigSys);
            pszConfigSys = NULL;
        } // end else if (csysLoadConfigSys(szConfigSys, &pszConfigSys))

        WinSetPointer(HWND_DESKTOP, hptrOld);
    }

    if (flFlags & CBI_ENABLE)
    {
        // enable items
        /* if (pcnbp->ulPageID == SP_HPFS)
        {
            BOOL fLazyWrite = winhIsDlgItemChecked(pcnbp->hwndDlgPage,
                                                   ID_OSDI_CACHE_LAZYWRITE);
            WinEnableControl(pcnbp->hwndDlgPage,
                             ID_OSDI_CACHE_MAXAGE, fLazyWrite);
            WinEnableControl(pcnbp->hwndDlgPage,
                             ID_OSDI_CACHE_BUFFERIDLE, fLazyWrite);
            WinEnableControl(pcnbp->hwndDlgPage,
                             ID_OSDI_CACHE_DISKIDLE, fLazyWrite);
        } */

        if (/*     (pcnbp->ulPageID == SP_HPFS)
            || */  (pcnbp->ulPageID == SP_FAT)
           )
        {
            WinEnableControl(pcnbp->hwndDlgPage, ID_OSDI_CACHESIZE,
                             !winhIsDlgItemChecked(pcnbp->hwndDlgPage,
                                                   ID_OSDI_CACHESIZE_AUTO));
        }
        else if (pcnbp->ulPageID == SP_WPS)
        {
            WinEnableControl(pcnbp->hwndDlgPage, ID_OSDI_AUTO_WARPCENTER, (doshIsWarp4()));
            WinEnableControl(pcnbp->hwndDlgPage, ID_OSDI_AUTOREFRESHFOLDERS, (doshIsWarp4()));
        }
        else if (pcnbp->ulPageID == SP_ERRORS)
        {
            WinEnableControl(pcnbp->hwndDlgPage, ID_OSDI_SUPRESSP_DRIVE,
                             winhIsDlgItemChecked(pcnbp->hwndDlgPage,
                                                  ID_OSDI_SUPRESSPOPUPS));
        }
        else if (pcnbp->ulPageID == SP_SYSPATHS)
        {
            ULONG   ulSelCount = 0;
            ULONG   ulLastSel = LIT_FIRST;
            CHAR    szTemp[300];
            PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();

            do {
                // find out how many items are selected
                ULONG ulSel = (ULONG)WinSendDlgItemMsg(pcnbp->hwndDlgPage,
                                                       ID_OSDI_PATHLISTBOX,
                                                       LM_QUERYSELECTION,
                                                       (MPARAM)ulLastSel,
                                                       MPNULL);
                if (ulSel == LIT_NONE)
                    break;

                ulLastSel = ulSel;
                ulSelCount++;
            } while (TRUE);

            sprintf(szTemp, pNLSStrings->pszItemsSelected, ulSelCount);
            WinSetDlgItemText(pcnbp->hwndDlgPage, ID_OSDI_PATHINFOTXT, szTemp);

            switch (ulSelCount)
            {
                case 0:
                    // no items selected:
                    WinEnableControl(pcnbp->hwndDlgPage, ID_OSDI_PATHEDIT, FALSE);
                            // V0.9.9 (2001-02-28) [pr]
                    WinEnableControl(pcnbp->hwndDlgPage, ID_OSDI_PATHDELETE, FALSE);
                    WinEnableControl(pcnbp->hwndDlgPage, ID_OSDI_PATHUP, FALSE);
                    WinEnableControl(pcnbp->hwndDlgPage, ID_OSDI_PATHDOWN, FALSE);
                break;

                case 1:
                    // exactly one item selected:
                    WinEnableControl(pcnbp->hwndDlgPage, ID_OSDI_PATHEDIT, TRUE);
                            // V0.9.9 (2001-02-28) [pr]
                    WinEnableControl(pcnbp->hwndDlgPage, ID_OSDI_PATHDELETE, TRUE);
                    WinEnableControl(pcnbp->hwndDlgPage, ID_OSDI_PATHUP,
                            (ulLastSel > 0));
                    WinEnableControl(pcnbp->hwndDlgPage, ID_OSDI_PATHDOWN,
                            (ulLastSel < lstCountItems(G_pSysPathSelected->pllPaths)-1));
                break;

                default:
                    // more than one item selected:
                    WinEnableControl(pcnbp->hwndDlgPage, ID_OSDI_PATHEDIT, TRUE);
                            // V0.9.9 (2001-02-28) [pr]
                    WinEnableControl(pcnbp->hwndDlgPage, ID_OSDI_PATHDELETE, TRUE);
                    WinEnableControl(pcnbp->hwndDlgPage, ID_OSDI_PATHUP, FALSE);
                    WinEnableControl(pcnbp->hwndDlgPage, ID_OSDI_PATHDOWN, FALSE);
                break;
            }
        }
    }

    if (flFlags & CBI_DESTROY)
    {
        if (pcnbp->ulPageID == SP_SYSPATHS)
        {
            PLISTNODE pSysPathNode = lstQueryFirstNode(G_pllSysPathsList);
                    // this list holds SYSPATH entries
                 /* typedef struct _SYSPATH
                 {
                     PSZ         pszPathType;            // "LIBPATH", "SET PATH", etc.
                     PLINKLIST   pllPaths;               // linked list of PSZs with the path entries
                 } SYSPATH, *PSYSPATH; */

            // _Pmpf(("cfgConfigInitPage CBI_DESTROY: Destroying lists"));
            while (pSysPathNode)
            {
                PSYSPATH pSysPathThis = (PSYSPATH)pSysPathNode->pItemData;
                lstFree(pSysPathThis->pllPaths);        // this frees the items automatically
                free(pSysPathThis);

                pSysPathNode = pSysPathNode->pNext;
            }
            lstFree(G_pllSysPathsList);
            G_pllSysPathsList = NULL;
        }
    }
}

/*
 *@@ cfgConfigItemChanged:
 *      common notebook callback function (notebook.c) for
 *      all the notebook pages dealing with CONFIG.SYS settings.
 *      This monster function reacts to changes of any of the
 *      dialog controls and reads/writes CONFIG.SYS settings.
 *      Since this callback is shared among all the CONFIG.SYS
 *      pages, pcnbp->ulPageID is used for telling them apart by
 *      using the SP_* identifiers.
 *
 *@@changed V0.9.0 [umoeller]: added "System paths" page handling
 *@@changed V0.9.0 [umoeller]: adjusted function prototype
 *@@changed V0.9.9 (2001-02-28) [pr]: added "edit path"
 *@@todo localize
 */

MRESULT cfgConfigItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                             USHORT usItemID,
                             USHORT usNotifyCode,
                             ULONG ulExtra)      // for checkboxes: contains new state
{
    MRESULT mrc = (MPARAM)0;
    LONG    lGrid = 0;

    switch (usItemID)
    {

        case ID_OSDI_MAXTHREADS:
            // "Scheduler" page; 64 steps
            lGrid = 64;
            goto adjustspin;

        case ID_OSDI_MINSWAPSIZE:
        case ID_OSDI_MINSWAPFREE:
            lGrid = 2;
            goto adjustspin;

        case ID_OSDI_CACHESIZE:
            // HPFS and FAT pages; 64 KB steps
            lGrid = 64;
            goto adjustspin;

        case ID_OSDI_CACHE_THRESHOLD:
            // HPFS and FAT pages; 4 KB steps
            lGrid = 4;
            goto adjustspin;

        case ID_OSDI_CACHE_MAXAGE:
        case ID_OSDI_CACHE_BUFFERIDLE:
        case ID_OSDI_CACHE_DISKIDLE:
            // HPFS page; 1000 ms steps
            lGrid = 1000;

            adjustspin:
            if (   (usNotifyCode == SPBN_UPARROW)
                || (usNotifyCode == SPBN_DOWNARROW)
               )
            {
                winhAdjustDlgItemSpinData(pcnbp->hwndDlgPage, usItemID,
                                          lGrid, usNotifyCode);
            }
        break;

        case ID_OSDI_AUTOCHECK_PROPOSE:
        {
            // "Propose" button for auto-chkdsk (HPFS/FAT pages):
            // enumerate all HPFS or FAT drives on the system
            CHAR szHPFSDrives[30];
            doshEnumDrives(szHPFSDrives,
                           /* (pcnbp->ulPageID == SP_HPFS)
                                ? "HPFS"
                                : */ "FAT",
                           TRUE); // skip removeable drives
            WinSetDlgItemText(pcnbp->hwndDlgPage, ID_OSDI_AUTOCHECK, szHPFSDrives);
        break; }

        /*
         * ID_OSDI_PATHDROPDOWN:
         *      "system paths" drop-down (V0.9.0)
         */

        case ID_OSDI_PATHDROPDOWN:
        {
            if (usNotifyCode == LN_SELECT)
            {
                // system path selection changed:
                // update the listbox below
                ULONG ulSelection = (ULONG)WinSendDlgItemMsg(pcnbp->hwndDlgPage,
                                                             ID_OSDI_PATHDROPDOWN,
                                                             LM_QUERYSELECTION,
                                                             MPNULL,
                                                             MPNULL);

                if (ulSelection != LIT_NONE)
                {
                    // V0.9.9 (2001-02-28) [pr]
                    if (G_hwndDlgDoubleFiles)
                    {
                        WinDestroyWindow(G_hwndDlgDoubleFiles);
                        G_hwndDlgDoubleFiles = NULLHANDLE;
                        WinEnableWindow(WinWindowFromID(pcnbp->hwndDlgPage,
                                                        ID_OSDI_DOUBLEFILES),
                                        TRUE);
                    }

                    G_pSysPathSelected = lstItemFromIndex(G_pllSysPathsList, ulSelection);
                    // clear listbox
                    WinSendDlgItemMsg(pcnbp->hwndDlgPage, ID_OSDI_PATHLISTBOX,
                                      LM_DELETEALL,
                                      MPNULL, MPNULL);

                    if (G_pSysPathSelected)
                    {
                        PLISTNODE pPathNode = lstQueryFirstNode(G_pSysPathSelected->pllPaths);
                        // insert new items
                        while (pPathNode)
                        {
                            WinSendDlgItemMsg(pcnbp->hwndDlgPage, ID_OSDI_PATHLISTBOX,
                                              LM_INSERTITEM,
                                              (MPARAM)LIT_END,
                                              (MPARAM)pPathNode->pItemData);
                                                      // item data is the path string (PSZ)
                            pPathNode = pPathNode->pNext;
                        }
                    }
                }
            }
        break; }

        /*
         * ID_OSDI_PATHLISTBOX:
         *      "system paths" listbox (V0.9.0)
         */

        case ID_OSDI_PATHLISTBOX:
        {
            if (usNotifyCode == LN_SELECT)
                cfgConfigInitPage(pcnbp, CBI_ENABLE); // re-enable items
        break; }

        /*
         * buttons on "System path" page:
         *
         */

        case ID_OSDI_PATHNEW:
        {
            CHAR szNewPath[CCHMAXPATH];
            szNewPath[0] = 0; // V0.9.9 (2001-02-28) [pr]

            if (WinDlgBox(HWND_DESKTOP,         // parent
                          pcnbp->hwndFrame,      // owner
                          fnwpNewSystemPathDlg,
                          cmnQueryNLSModuleHandle(FALSE),
                          ID_OSD_NEWSYSPATH,
                          szNewPath)            // pass buffer as create param;
                                                // this will have the directory
                == DID_OK)
            {
                PSZ     pszPathCopy = strdup(szNewPath);
                SHORT   sInserted = 0;
                // insert the item
                lstAppendItem(G_pSysPathSelected->pllPaths,
                              pszPathCopy);
                sInserted = (SHORT)WinSendDlgItemMsg(pcnbp->hwndDlgPage, ID_OSDI_PATHLISTBOX,
                                                     LM_INSERTITEM,
                                                     (MPARAM)LIT_END,
                                                     (MPARAM)pszPathCopy);
                WinSendDlgItemMsg(pcnbp->hwndDlgPage, ID_OSDI_PATHLISTBOX,
                                  LM_SELECTITEM,
                                  (MPARAM)sInserted,
                                  (MPARAM)TRUE); // select flag
            }
        break; }

        // V0.9.9 (2001-02-28) [pr]: added path edit capability
        case ID_OSDI_PATHEDIT:
        {
            PLISTNODE pNode = 0;
            ULONG ulNextSel = LIT_FIRST;

            do {
                // go thru all selected items
                ulNextSel = (ULONG)WinSendDlgItemMsg(pcnbp->hwndDlgPage, ID_OSDI_PATHLISTBOX,
                                                     LM_QUERYSELECTION,
                                                     (MPARAM) ulNextSel,
                                                     MPNULL);
                if (ulNextSel == LIT_NONE)
                    break;

                pNode = lstNodeFromIndex(G_pSysPathSelected->pllPaths, ulNextSel);
                if (pNode)
                {
                    CHAR szNewPath[CCHMAXPATH];
                    // make a copy of the item
                    strcpy (szNewPath, pNode->pItemData);

                    if (WinDlgBox(HWND_DESKTOP,         // parent
                                  pcnbp->hwndFrame,     // owner
                                  fnwpNewSystemPathDlg,
                                  cmnQueryNLSModuleHandle(FALSE),
                                  ID_OSD_NEWSYSPATH,
                                  szNewPath)            // pass buffer as create param;
                                                        // this will have the directory
                        == DID_OK)
                    {
                        PSZ     pszPathCopy = strdup(szNewPath);

                        // update listbox and linked list
                        WinSendDlgItemMsg(pcnbp->hwndDlgPage,
                                          ID_OSDI_PATHLISTBOX,
                                          LM_SETITEMTEXT,
                                          (MPARAM)ulNextSel,
                                          pszPathCopy);
                        free(pNode->pItemData);
                        pNode->pItemData = pszPathCopy;
                    }
                }
            } while (TRUE);

        break; }

        case ID_OSDI_PATHDELETE:
        {
            do {
                // go thru all selected items (for "delete", this can be several)
                ULONG ulNextSel = (ULONG)WinSendDlgItemMsg(pcnbp->hwndDlgPage, ID_OSDI_PATHLISTBOX,
                                                           LM_QUERYSELECTION,
                                                           (MPARAM)LIT_FIRST,
                                                           MPNULL);
                if (ulNextSel == LIT_NONE)
                    break;

                // delete selected from listbox
                WinSendDlgItemMsg(pcnbp->hwndDlgPage, ID_OSDI_PATHLISTBOX,
                                  LM_DELETEITEM,
                                  (MPARAM)ulNextSel,
                                  MPNULL);

                // and from linked list
                lstRemoveNode(G_pSysPathSelected->pllPaths,
                              lstNodeFromIndex(G_pSysPathSelected->pllPaths,
                                               ulNextSel));

            } while (TRUE);

            cfgConfigInitPage(pcnbp, 100); // re-enable items
        break; }

        case ID_OSDI_PATHUP:
        case ID_OSDI_PATHDOWN:
        {
            // move item up / down
            PLISTNODE pNode = 0;
            ULONG ulSel = (ULONG)WinSendDlgItemMsg(pcnbp->hwndDlgPage, ID_OSDI_PATHLISTBOX,
                                                   LM_QUERYSELECTION,
                                                   (MPARAM)LIT_FIRST,
                                                   MPNULL);
                                           // this can only be one selection here

            // delete selected from listbox
            WinSendDlgItemMsg(pcnbp->hwndDlgPage, ID_OSDI_PATHLISTBOX,
                              LM_DELETEITEM,
                              (MPARAM)ulSel,
                              MPNULL);

            pNode = lstNodeFromIndex(G_pSysPathSelected->pllPaths, ulSel);
            if (pNode)
            {
                // make a backup of the item
                PSZ pszPathCopy = strdup(pNode->pItemData);
                // remove it from the linked list
                lstRemoveNode(G_pSysPathSelected->pllPaths,
                              pNode);

                // new position to insert at
                if (usItemID == ID_OSDI_PATHUP)
                    ulSel--;
                else
                    ulSel++;

                // and insert the item again
                lstInsertItemBefore(G_pSysPathSelected->pllPaths,
                                    pszPathCopy,
                                    ulSel);
                WinSendDlgItemMsg(pcnbp->hwndDlgPage, ID_OSDI_PATHLISTBOX,
                                  LM_INSERTITEM,
                                  (MPARAM)ulSel,
                                  (MPARAM)pszPathCopy);
                WinSendDlgItemMsg(pcnbp->hwndDlgPage, ID_OSDI_PATHLISTBOX,
                                  LM_SELECTITEM,
                                  (MPARAM)ulSel,
                                  (MPARAM)TRUE); // select flag
            }
        break; }

        /*
         * ID_OSDI_VALIDATE:
         *      "select invalid" button ("System paths" page).
         */

        case ID_OSDI_VALIDATE:
        {
            PLISTNODE pNode = lstQueryFirstNode(G_pSysPathSelected->pllPaths);
            ULONG     ulCount = 0;
            HPOINTER hptrOld = winhSetWaitPointer();

            while (pNode)
            {
                BOOL fSelect = (!doshQueryDirExist(pNode->pItemData));
                    // if dir doesn't exist, select it

                WinSendDlgItemMsg(pcnbp->hwndDlgPage, ID_OSDI_PATHLISTBOX,
                                  LM_SELECTITEM,
                                  (MPARAM)ulCount,
                                  (MPARAM)fSelect);

                ulCount++;
                pNode = pNode->pNext;
            }
            WinSetPointer(HWND_DESKTOP, hptrOld);
        break; }

        /*
         * ID_OSDI_DOUBLEFILES:
         *      "Double files.." button ("System paths" page).
         */

        case ID_OSDI_DOUBLEFILES:
        {
            if (G_pSysPathSelected)
            {
                // V0.9.9 (2001-02-28) [pr]
                G_hwndDlgDoubleFiles = WinLoadDlg(HWND_DESKTOP,        // parent
                                                  pcnbp->hwndFrame, // pcnbp->hwndPage,     // owner
                                                  fnwpDoubleFilesDlg,
                                                  cmnQueryNLSModuleHandle(FALSE),
                                                  ID_OSD_FILELIST,
                                                  (PVOID) pcnbp->hwndDlgPage);
                if (G_hwndDlgDoubleFiles)
                {
                    winhCenterWindow(G_hwndDlgDoubleFiles);
                    cmnSetControlsFont(G_hwndDlgDoubleFiles, 0, 5000);
                    WinShowWindow(G_hwndDlgDoubleFiles, TRUE);
                    WinEnableWindow(WinWindowFromID(pcnbp->hwndDlgPage,
                                                    ID_OSDI_DOUBLEFILES),
                                    FALSE);
                }
            }

        break; }

        /*
         * DID_APPLY:
         *      "Apply" button
         */

        case DID_APPLY:
        {
            PCKERNELGLOBALS   pKernelGlobals = krnQueryGlobals();
            // have the user confirm this
            if (cmnMessageBoxMsg(pcnbp->hwndFrame, // pcnbp->hwndPage,
                                 100, 101,
                                 MB_YESNO | MB_DEFBUTTON2)
                              == MBID_YES)
            {
                PSZ     pszConfigSys = NULL;

                if (pszConfigSys == NULL)
                {
                    if (csysLoadConfigSys(NULL, &pszConfigSys))
                        winhDebugBox(pcnbp->hwndFrame,
                                 "XWorkplace",
                                 "XWorkplace was unable to open the CONFIG.SYS file.");
                }

                if (pszConfigSys)
                {
                    // PSZ     p;
                    ULONG   ul = 0, ulMinFree = 0, ulMinSize = 0;
                    CHAR    szSwapPath[CCHMAXPATH],
                            szBackup[CCHMAXPATH];

                    switch (pcnbp->ulPageID)
                    {
                        /*
                         * SP_SCHEDULER:
                         *      write "Scheduler" settings back to CONFIG.SYS
                         */

                        case SP_SCHEDULER:
                        {
                            CHAR    szTemp[100];
                            WinSendDlgItemMsg(pcnbp->hwndDlgPage, ID_OSDI_MAXTHREADS,
                                              SPBM_QUERYVALUE,
                                              (MPARAM)&ul,
                                              MPFROM2SHORT(0,
                                                           SPBQ_UPDATEIFVALID));
                            sprintf(szTemp, "%d", ul);
                            csysSetParameter(&pszConfigSys, "THREADS=", szTemp,
                                             TRUE); // convert to upper case if necessary

                            WinSendDlgItemMsg(pcnbp->hwndDlgPage, ID_OSDI_MAXWAIT,
                                              SPBM_QUERYVALUE,
                                              (MPARAM)&ul,
                                              MPFROM2SHORT(0,
                                                           SPBQ_UPDATEIFVALID));
                            sprintf(szTemp, "%d", ul);
                            csysSetParameter(&pszConfigSys, "MAXWAIT=", szTemp,
                                             TRUE); // convert to upper case if necessary

                            csysSetParameter(&pszConfigSys, "PRIORITY_DISK_IO=",
                                    (winhIsDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_PRIORITYDISKIO)
                                            ? "yes" : "no"),
                                    TRUE); // convert to upper case if necessary
                        break; }

                        /*
                         * SP_MEMORY:
                         *      write "Memory" settings back to CONFIG.SYS
                         */

                        case SP_MEMORY:
                        {
                            CHAR    szTemp[500];
                            WinQueryDlgItemText(pcnbp->hwndDlgPage, ID_OSDI_SWAPPATH,
                                                sizeof(szSwapPath)-1, szSwapPath);
                            WinSendDlgItemMsg(pcnbp->hwndDlgPage, ID_OSDI_MINSWAPSIZE,
                                              SPBM_QUERYVALUE,
                                              (MPARAM)&ulMinSize,
                                              MPFROM2SHORT(0,
                                                           SPBQ_UPDATEIFVALID));
                            WinSendDlgItemMsg(pcnbp->hwndDlgPage, ID_OSDI_MINSWAPFREE,
                                              SPBM_QUERYVALUE,
                                              (MPARAM)&ulMinFree,
                                              MPFROM2SHORT(0,
                                                           SPBQ_UPDATEIFVALID));
                            sprintf(szTemp, "%s %d %d", szSwapPath, ulMinFree*1024, ulMinSize*1024);
                            csysSetParameter(&pszConfigSys, "SWAPPATH=", szTemp,
                                             TRUE); // convert to upper case if necessary

                        break; }

                        /*
                         * SP_HPFS:
                         *      write HPFS settings back to CONFIG.SYS
                         */

                        /* case SP_HPFS:
                        {
                            CHAR    szTemp[300] = "",
                                    szAutoCheck[200] = "",
                                    szSearchKey[100] = "";
                            ULONG   ulCacheSize = 0,
                                    ulThreshold = 4,
                                    ulMaxAge = 5000,
                                    ulDiskIdle = 1000,
                                    ulBufferIdle = 500;
                            // BOOL    fLazyWrite = TRUE;

                            WinSendDlgItemMsg(pcnbp->hwndDlgPage, ID_OSDI_CACHE_THRESHOLD,
                                              SPBM_QUERYVALUE,
                                              (MPARAM)&ulThreshold,
                                              MPFROM2SHORT(0,
                                                           SPBQ_UPDATEIFVALID));
                            WinQueryDlgItemText(pcnbp->hwndDlgPage,
                                                ID_OSDI_AUTOCHECK,
                                                sizeof(szAutoCheck)-1,
                                                szAutoCheck);

                            if (winhIsDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_CACHESIZE_AUTO))
                            {
                                // auto-size cache: leave out /CACHE
                                sprintf(szTemp,
                                        "/crecl:%d /autocheck:%s",
                                        doshQueryBootDrive(),
                                        ulThreshold,
                                        szAutoCheck);
                            }
                            else
                            {
                                // no auto-size cache
                                WinSendDlgItemMsg(pcnbp->hwndDlgPage, ID_OSDI_CACHESIZE,
                                                  SPBM_QUERYVALUE,
                                                  (MPARAM)&ulCacheSize,
                                                  MPFROM2SHORT(0,
                                                               SPBQ_UPDATEIFVALID));
                                sprintf(szTemp,
                                        "/cache:%d /crecl:%d /autocheck:%s",
                                        ulCacheSize,
                                        ulThreshold,
                                        szAutoCheck);
                            }
                            sprintf(szSearchKey, "IFS=%c:\\OS2\\HPFS.IFS ",
                                        doshQueryBootDrive());
                            csysSetParameter(&pszConfigSys, szSearchKey, szTemp,
                                    TRUE); // convert to upper case if necessary

                            if (winhIsDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_CACHE_LAZYWRITE))
                            {
                                WinSendDlgItemMsg(pcnbp->hwndDlgPage, ID_OSDI_CACHE_MAXAGE,
                                                  SPBM_QUERYVALUE,
                                                  (MPARAM)&ulMaxAge,
                                                  MPFROM2SHORT(0,
                                                               SPBQ_UPDATEIFVALID));
                                WinSendDlgItemMsg(pcnbp->hwndDlgPage, ID_OSDI_CACHE_DISKIDLE,
                                                  SPBM_QUERYVALUE,
                                                  (MPARAM)&ulDiskIdle,
                                                  MPFROM2SHORT(0,
                                                               SPBQ_UPDATEIFVALID));
                                WinSendDlgItemMsg(pcnbp->hwndDlgPage, ID_OSDI_CACHE_BUFFERIDLE,
                                                  SPBM_QUERYVALUE,
                                                  (MPARAM)&ulBufferIdle,
                                                  MPFROM2SHORT(0,
                                                               SPBQ_UPDATEIFVALID));
                                sprintf(szTemp,
                                        "/maxage:%d /diskidle:%d /bufferidle:%d "
                                        "/readahead:on /lazy:1",
                                        ulMaxAge,
                                        ulDiskIdle,
                                        ulBufferIdle);
                            }
                            else
                                strcpy(szTemp, "/lazy:off");

                            // compose the key with CACHE;
                            sprintf(szSearchKey, "RUN=%c:\\OS2\\CACHE.EXE ",
                                        doshQueryBootDrive());
                            csysSetParameter(&pszConfigSys, szSearchKey, szTemp,
                                    TRUE); // convert to upper case if necessary

                        break; } */

                        /*
                         * SP_FAT:
                         *      write FAT settings back to CONFIG.SYS
                         */

                        case SP_FAT:
                        {
                            // "Cache installed" checked?
                            if (winhIsDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_FSINSTALLED))
                            {
                                CHAR    // szParameter[300] = "",
                                        szTemp[300] = "",
                                        szAutoCheck[200] = "";
                                ULONG   ulCacheSize = 512,
                                        ulThreshold = 4;
                                // PSZ     p2;

                                if (winhIsDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_CACHESIZE_AUTO))
                                    strcpy(szTemp, "d");
                                else
                                {
                                    // no auto-size cache
                                    WinSendDlgItemMsg(pcnbp->hwndDlgPage, ID_OSDI_CACHESIZE,
                                                      SPBM_QUERYVALUE,
                                                      (MPARAM)&ulCacheSize,
                                                      MPFROM2SHORT(0,
                                                                   SPBQ_UPDATEIFVALID));
                                    sprintf(szTemp, "%d", ulCacheSize);
                                }

                                if (winhIsDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_CACHE_LAZYWRITE))
                                    strcat(szTemp, ",lw");

                                WinSendDlgItemMsg(pcnbp->hwndDlgPage, ID_OSDI_CACHE_THRESHOLD,
                                                  SPBM_QUERYVALUE,
                                                  (MPARAM)&ulThreshold,
                                                  MPFROM2SHORT(0,
                                                               SPBQ_UPDATEIFVALID));
                                // again, convert KB to sectors for the threshold
                                sprintf(szTemp+strlen(szTemp), ",%d", ulThreshold*2);

                                WinQueryDlgItemText(pcnbp->hwndDlgPage, ID_OSDI_AUTOCHECK,
                                                sizeof(szAutoCheck)-1, szAutoCheck);
                                if (strlen(szAutoCheck))
                                    sprintf(szTemp+strlen(szTemp), ",ac:%s", szAutoCheck, TRUE);

                                csysSetParameter(&pszConfigSys, "DISKCACHE=", szTemp,
                                    TRUE); // convert to upper case if necessary

                            }
                            else
                            {
                                // no "Cache installed":
                                csysDeleteLine(pszConfigSys, "DISKCACHE=");
                            }
                        break; }

                        /*
                         * SP_WPS:
                         *      write WPS settings to CONFIG.SYS
                         */

                        case SP_WPS:
                        {
                            CHAR   szTemp[300] = "";
                            BOOL   fCopied = FALSE;
                            if (winhIsDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_AUTO_PROGRAMS))
                            {
                                strcpy(szTemp, "programs");
                                fCopied = TRUE;
                            }
                            if (winhIsDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_AUTO_TASKLIST))
                            {
                                if (fCopied)
                                    strcat(szTemp, ",");
                                strcat(szTemp, "tasklist");
                                fCopied = TRUE;
                            }
                            if (fCopied)
                                strcat(szTemp, ",");
                            strcat(szTemp, "folders");
                            if (winhIsDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_AUTO_CONNECTIONS))
                            {
                                strcat(szTemp, ",connections");
                            }
                            if (winhIsDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_AUTO_LAUNCHPAD))
                            {
                                strcat(szTemp, ",launchpad");
                            }
                            if (winhIsDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_AUTO_WARPCENTER))
                            {
                                strcat(szTemp, ",warpcenter");
                            }
                            csysSetParameter(&pszConfigSys, "SET AUTOSTART=", szTemp,
                                    TRUE); // convert to upper case if necessary

                            if (winhIsDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_RESTART_FOLDERS))
                                strcpy(szTemp, "STARTUPFOLDERSONLY");
                            else if (winhIsDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_RESTART_NO))
                                strcpy(szTemp, "no");
                            else
                                strcpy(szTemp, "yes");
                            if (winhIsDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_RESTART_REBOOT))
                                strcat(szTemp, ",rebootonly");
                            csysSetParameter(&pszConfigSys, "SET RESTARTOBJECTS=", szTemp,
                                    TRUE); // convert to upper case if necessary

                            if (doshIsWarp4())
                                if (winhIsDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_AUTOREFRESHFOLDERS))
                                    csysDeleteLine(pszConfigSys, "SET AUTOREFRESHFOLDERS=");
                                else
                                    csysSetParameter(&pszConfigSys,
                                                     "SET AUTOREFRESHFOLDERS=",
                                                     "no",
                                                     TRUE); // convert to upper case if necessary
                        break; }

                        /*
                         * SP_ERRORS:
                         *      write error settings to CONFIG.SYS
                         */

                        case SP_ERRORS:
                        {
                            if (winhIsDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_AUTOFAIL))
                                csysSetParameter(&pszConfigSys, "AUTOFAIL=", "yes",
                                    TRUE); // convert to upper case if necessary
                            else
                                csysDeleteLine(pszConfigSys, "AUTOFAIL=");

                            if (winhIsDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_REIPL))
                                csysSetParameter(&pszConfigSys, "REIPL=", "on",
                                    TRUE); // convert to upper case if necessary
                            else
                                csysDeleteLine(pszConfigSys, "REIPL=");

                            if (winhIsDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_SUPRESSPOPUPS)) {
                                CHAR szSpinButtonValue[5];
                                WinSendDlgItemMsg(pcnbp->hwndDlgPage, ID_OSDI_SUPRESSP_DRIVE,
                                                  SPBM_QUERYVALUE,
                                                  (MPARAM)szSpinButtonValue,
                                                  MPFROM2SHORT(sizeof(szSpinButtonValue)-1,
                                                               SPBQ_UPDATEIFVALID));
                                csysSetParameter(&pszConfigSys, "SUPPRESSPOPUPS=",
                                        szSpinButtonValue,
                                        TRUE); // convert to upper case if necessary
                            }
                            else
                                csysDeleteLine(pszConfigSys, "SUPPRESSPOPUPS=");


                        break; }

                        /*
                         * SP_SYSPATHS:
                         *      write _all_ system paths to CONFIG.SYS
                         */

                        case SP_SYSPATHS:
                        {
                            PLISTNODE pSysPathNode = lstQueryFirstNode(G_pllSysPathsList);

                            while (pSysPathNode)
                            {
                                PSYSPATH pSysPathThis = pSysPathNode->pItemData;
                                CHAR szPathType[100] = "";
                                CHAR szPaths[1000] = {0};
                                PLISTNODE pPathNode = lstQueryFirstNode(pSysPathThis->pllPaths);
                                PSZ p = szPaths;
                                ULONG ulCount = 0;

                                strcpy(szPathType, pSysPathThis->pszPathType);

                                while (pPathNode)
                                {
                                    if (ulCount)
                                    {
                                        *p = ';';
                                        p++;
                                    }
                                    p += sprintf(p, "%s", pPathNode->pItemData);
                                    pPathNode = pPathNode->pNext;
                                    ulCount++;
                                }

                                csysSetParameter(&pszConfigSys, szPathType,
                                                 szPaths,
                                                 FALSE); // never convert to upper case
                                // next path
                                pSysPathNode = pSysPathNode->pNext;
                            }

                        break; }
                    } // end switch

                    // write file!
                    if (csysWriteConfigSys(NULL,
                                           pszConfigSys, // contents
                                           szBackup)     // backup
                                == NO_ERROR)
                        // "file written" msg
                        cmnMessageBoxMsg(pcnbp->hwndFrame, // pcnbp->hwndPage,
                                         100,
                                         136,       // *@@todo
                                         MB_OK);
                    else
                        winhDebugBox(NULLHANDLE, "Error", "Error writing CONFIG.SYS");
                                // @@todo localize

                    if (pszConfigSys)
                    {
                        free(pszConfigSys);
                        pszConfigSys = NULL;
                    }
                }
            }
        break; }

        /*
         * DID_OPTIMIZE:
         *      "Optimize" button
         */

        case DID_OPTIMIZE:
        {
            switch (pcnbp->ulPageID)
            {
                case SP_SCHEDULER:
                {
                    PQPROCSTAT16 pps = prc16GetInfo(NULL);
                    // THREADS=
                    winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_OSDI_MAXTHREADS,
                                           128, 4096,
                                           // get current thread count, add 50% for safety,
                                           // and round up to the next multiple of 128
                                           (( (    (prc16QueryThreadCount(pps, 0)  // whole system
                                                 * 3) / 2) + 127 ) / 128) * 128
                                          );
                    prc16FreeInfo(pps);

                    // MAXWAIT=2
                    winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_OSDI_MAXWAIT,
                                           1, 10,
                                           2);

                    winhSetDlgItemChecked(pcnbp->hwndDlgPage,
                                          ID_OSDI_PRIORITYDISKIO,
                                          TRUE);
                break; }

                case SP_MEMORY:
                {
                    // minsize: get current size, add 50% and
                    // round up to the next multiple of 2 MB
                    if (strlen(G_szSwapperFilename) != 0)
                    {
                        ULONG ulSize = doshQueryPathSize(G_szSwapperFilename)/1024/1024;
                        winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_OSDI_MINSWAPSIZE,
                                               2, 100,
                                               ( (((ulSize*3)/2)+1) / 2 ) * 2
                                       );
                    }

                    // minfree = 2
                    winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_OSDI_MINSWAPFREE,
                                           2, 1000,
                                           2);
                break; }

                /* case SP_HPFS:
                {
                    ULONG   aulSysInfo[QSV_MAX] = {0},
                            ulInstalledMB;
                    CHAR szHPFSDrives[30];
                    DosQuerySysInfo(1L, QSV_MAX,
                                        (PVOID)aulSysInfo, sizeof(ULONG)*QSV_MAX);
                    ulInstalledMB =
                               (aulSysInfo[QSV_TOTPHYSMEM-1] + (512*1000)) / 1024 / 1024;
                    doshEnumDrives(szHPFSDrives,
                                   "HPFS",
                                   TRUE); // skip removeable drives

                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_FSINSTALLED,
                                (strlen(szHPFSDrives) > 0));
                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_CACHESIZE_AUTO,
                                FALSE);
                    winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_OSDI_CACHESIZE,
                                           0, 2048,
                                           (ulInstalledMB > 16) ? 2048 : 1024);
                    winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_OSDI_CACHE_THRESHOLD,
                                           4, 64,
                                           64);
                    WinSetDlgItemText(pcnbp->hwndDlgPage, ID_OSDI_AUTOCHECK, szHPFSDrives);

                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_CACHE_LAZYWRITE,
                                TRUE);
                    winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_OSDI_CACHE_MAXAGE,
                                           500, 100*1000,
                                           60*1000);
                    winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_OSDI_CACHE_BUFFERIDLE,
                                           500, 100*1000,
                                           30*1000);
                    winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_OSDI_CACHE_DISKIDLE,
                                           500, 100*1000,
                                           60*1000);

                break; } */

                case SP_FAT:
                {
                    ULONG   aulSysInfo[QSV_MAX] = {0},
                            ulInstalledMB;
                    CHAR szFATDrives[30];
                    DosQuerySysInfo(1L, QSV_MAX,
                                        (PVOID)aulSysInfo, sizeof(ULONG)*QSV_MAX);
                    ulInstalledMB =
                               (aulSysInfo[QSV_TOTPHYSMEM-1] + (512*1000)) / 1024 / 1024;
                    doshEnumDrives(szFATDrives,
                                   "FAT",
                                   TRUE); // skip removeable drives

                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_FSINSTALLED,
                                (strlen(szFATDrives) > 0));

                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_CACHESIZE_AUTO, FALSE);
                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_CACHE_LAZYWRITE, TRUE);
                    winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_OSDI_CACHESIZE,
                                           0, 14400,
                                           (ulInstalledMB > 16)
                                                ? 2048
                                                : 1024);
                    winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_OSDI_CACHE_THRESHOLD,
                                           4, 64,
                                           64);
                    // do not auto-check FAT drives
                    WinSetDlgItemText(pcnbp->hwndDlgPage, ID_OSDI_AUTOCHECK, "");

                break; }

                case SP_WPS:
                {
                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_AUTO_PROGRAMS, TRUE);
                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_AUTO_TASKLIST, TRUE);
                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_AUTO_CONNECTIONS, TRUE);
                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_AUTO_LAUNCHPAD, FALSE);
                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_AUTO_WARPCENTER, FALSE);
                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_RESTART_FOLDERS, TRUE);
                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_RESTART_REBOOT, FALSE);
                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_AUTOREFRESHFOLDERS,
                                FALSE);
                break; }

                case SP_ERRORS:
                {
                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_AUTOFAIL, TRUE);
                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_REIPL, FALSE);
                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_SUPRESSPOPUPS, FALSE);
                break; }
            } // end switch

            cfgConfigInitPage(pcnbp, 100); // re-enable items
        break; }

        /*
         * DID_DEFAULT:
         *      "Default" button
         */

        case DID_DEFAULT:
        {
            switch (pcnbp->ulPageID)
            {
                case SP_SCHEDULER:
                {
                    winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_OSDI_MAXTHREADS,
                                           128, 4096,
                                           (doshIsWarp4())
                                                ? 512
                                                : 256);
                    winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_OSDI_MAXWAIT,
                                           1, 10,
                                           3);
                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_PRIORITYDISKIO,
                                          TRUE);
                break; }

                case SP_MEMORY:
                {
                    winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_OSDI_MINSWAPSIZE,
                                           2, 100,
                                           2);
                    winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_OSDI_MINSWAPFREE,
                                           2, 1000,
                                           2);
                break; }

                /* case SP_HPFS:
                {
                    CHAR szHPFSDrives[30];
                    doshEnumDrives(szHPFSDrives,
                                   "HPFS",
                                   TRUE); // skip removeable drives

                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_FSINSTALLED, TRUE);
                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_CACHESIZE_AUTO, FALSE);
                    winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_OSDI_CACHESIZE,
                                           0, 2048,
                                           1024);
                    winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_OSDI_CACHE_THRESHOLD,
                                           4, 64,
                                           4);
                    WinSetDlgItemText(pcnbp->hwndDlgPage, ID_OSDI_AUTOCHECK, szHPFSDrives);

                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_CACHE_LAZYWRITE,
                                TRUE);
                    winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_OSDI_CACHE_MAXAGE,
                                           500, 100*1000,
                                           5*1000);
                    winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_OSDI_CACHE_BUFFERIDLE,
                                           500, 100*1000,
                                           500);
                    winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_OSDI_CACHE_DISKIDLE,
                                           500, 100*1000,
                                           1000);

                break; } */

                case SP_FAT:
                {
                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_FSINSTALLED, TRUE);
                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_CACHESIZE_AUTO, TRUE);
                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_CACHE_LAZYWRITE, TRUE);
                    winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_OSDI_CACHESIZE,
                                           0, 14400,
                                           1024);
                    winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_OSDI_CACHE_THRESHOLD,
                                           4, 64,
                                           4);
                    WinSetDlgItemText(pcnbp->hwndDlgPage, ID_OSDI_AUTOCHECK, "");
                break; }

                case SP_WPS:
                {
                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_AUTO_PROGRAMS, TRUE);
                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_AUTO_TASKLIST, TRUE);
                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_AUTO_CONNECTIONS, TRUE);
                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_AUTO_LAUNCHPAD, TRUE);
                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_AUTO_WARPCENTER,
                                    (doshIsWarp4()));
                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_RESTART_YES, TRUE);
                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_RESTART_REBOOT, FALSE);
                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_AUTOREFRESHFOLDERS,
                                    (doshIsWarp4()));
                break; }

                case SP_ERRORS:
                {
                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_AUTOFAIL, FALSE);
                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_REIPL, FALSE);
                    winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_OSDI_SUPRESSPOPUPS, FALSE);
                break; }
            } // end switch

            cfgConfigInitPage(pcnbp, 100); // re-enable items
        break; }

    } // end switch (usItemID)

    if (    (usNotifyCode == SPBN_CHANGE)
         || (usNotifyCode == BN_CLICKED)
       )
       // if we had any changes, we might need to
       // re-enable controls
       cfgConfigInitPage(pcnbp, 100);

    return (mrc);
}

/*
 *@@ cfgConfigTimer:
 *      common callback func for the "Memory" and "Scheduler"
 *      pages, which have a 2-sec timer set for updating the
 *      display.
 *
 *@@changed V0.9.0 [umoeller]: adjusted function prototype
 */

VOID cfgConfigTimer(PCREATENOTEBOOKPAGE pcnbp,
                     ULONG ulTimer)
{
    CHAR szTemp[50];

    switch (pcnbp->ulPageID)
    {
        case SP_SCHEDULER:
        {
            PQPROCSTAT16 pps = prc16GetInfo(NULL);
            sprintf(szTemp, "%d", prc16QueryThreadCount(pps, 0));
            prc16FreeInfo(pps);
            WinSetDlgItemText(pcnbp->hwndDlgPage, ID_OSDI_CURRENTTHREADS, szTemp);
        break; }

        case SP_MEMORY:
            if (G_szSwapperFilename[0])
            {
                ULONG ulSize = doshQueryPathSize(G_szSwapperFilename)/1024/1024;
                if (ulSize)
                {
                    sprintf(szTemp, "%d", ulSize);
                    WinSetDlgItemText(pcnbp->hwndDlgPage, ID_OSDI_CURRENTSWAPSIZE,
                        (szTemp));
                } else
                    WinSetDlgItemText(pcnbp->hwndDlgPage, ID_OSDI_CURRENTSWAPSIZE, "???");
            }
        break;
    }
}

/* ******************************************************************
 *
 *   OS/2 Kernel "Syslevel" page
 *
 ********************************************************************/

typedef struct _SYSLEVELRECORD
{
    RECORDCORE  recc;

    CHAR        szComponent[100],
                szFile[100],
                szCSDCurrent[40],
                szCSDPrevious[40],
                szVersion[40];
    PSZ         pszComponent,
                pszFile,
                pszCSDCurrent,
                pszCSDPrevious,
                pszVersion;
} SYSLEVELRECORD, *PSYSLEVELRECORD;

/*
 *@@ AddOneSyslevel2Cnr:
 *
 *@@added V0.9.2 (2000-03-08) [umoeller]
 */

VOID AddOneSyslevel2Cnr(HWND hwndCnr,
                        HFILE hfSysLevel,
                        PSZ pszFilename)
{
    PSYSLEVELRECORD precc
        = (PSYSLEVELRECORD)cnrhAllocRecords(hwndCnr,
                                            sizeof(SYSLEVELRECORD),
                                            1);

    if (precc)
    {
        BYTE    abBuf[100];
        ULONG   ulWritten = 0;

        strcpy(precc->szFile, pszFilename);
        precc->pszFile = precc->szFile;

        // component name
        if (lvlQueryLevelFileData(hfSysLevel,
                                  QLD_TITLE,
                                  abBuf,
                                  sizeof(abBuf),
                                  &ulWritten)
            == NO_ERROR)
        {
            abBuf[ulWritten] = 0;
            strcpy(precc->szComponent, abBuf);
            precc->pszComponent = precc->szComponent;
        }

        // current CSD
        if (lvlQueryLevelFileData(hfSysLevel,
                                  QLD_CURRENTCSD,
                                  abBuf,
                                  sizeof(abBuf),
                                  &ulWritten)
            == NO_ERROR)
        {
            abBuf[ulWritten] = 0;
            strcpy(precc->szCSDCurrent, abBuf);
            precc->pszCSDCurrent = precc->szCSDCurrent;
        }

        // previous CSD
        if (lvlQueryLevelFileData(hfSysLevel,
                                  QLD_PREVIOUSCSD,
                                  abBuf,
                                  sizeof(abBuf),
                                  &ulWritten)
            == NO_ERROR)
        {
            abBuf[ulWritten] = 0;
            strcpy(precc->szCSDPrevious, abBuf);
            precc->pszCSDPrevious = precc->szCSDPrevious;
        }

        // version
        if (lvlQueryLevelFileData(hfSysLevel,
                                  QLD_MAJORVERSION,
                                  abBuf,
                                  sizeof(abBuf),
                                  &ulWritten)
            == NO_ERROR)
        {
            // OK, we got the major version:
            abBuf[ulWritten] = 0;
            strcpy(precc->szVersion, abBuf);

            // minor version:
            if (lvlQueryLevelFileData(hfSysLevel,
                                      QLD_MINORVERSION,
                                      abBuf,
                                      sizeof(abBuf),
                                      &ulWritten)
                == NO_ERROR)
            {
                abBuf[ulWritten] = 0;
                strcat(precc->szVersion, ".");
                strcat(precc->szVersion, abBuf);

                // revision:
                if (lvlQueryLevelFileData(hfSysLevel,
                                          QLD_REVISION,
                                          abBuf,
                                          sizeof(abBuf),
                                          &ulWritten)
                    == NO_ERROR)
                {
                    abBuf[ulWritten] = 0;
                    strcat(precc->szVersion, ".");
                    strcat(precc->szVersion, abBuf);
                }
            }

            precc->pszVersion = precc->szVersion;
        }

        cnrhInsertRecords(hwndCnr,
                          NULL,
                          (PRECORDCORE)precc,
                          TRUE, // invalidate
                          NULL,
                          CRA_RECORDREADONLY,
                          1);
    }
}

/*
 *@@ AddSyslevelsForDir:
 *
 *@@added V0.9.2 (2000-03-08) [umoeller]
 */

VOID AddSyslevelsForDir(HWND hwndCnr,
                        PSZ pszDir)     // in: directory to search (with terminating \)
{
    HDIR          hdirFindHandle = HDIR_CREATE;
    FILEFINDBUF3  ffb3     = {0};      /* Returned from FindFirst/Next */
    ULONG         cbFFB3 = sizeof(FILEFINDBUF3);
    ULONG         ulFindCount    = 1;        /* Look for 1 file at a time    */
    APIRET        rc             = NO_ERROR; /* Return code                  */

    HFILE   hfSysLevel = 0;
    CHAR    szCurDir[400],
            szSearchMask[400];

    strcpy(szCurDir, pszDir);
    if (szCurDir[0] == '?')
        szCurDir[0] = doshQueryBootDrive();
    sprintf(szSearchMask, "%sSYSLEVEL.*", szCurDir);

    // now go for the first directory entry in our directory (szCurrentDir):
    rc = DosFindFirst(szSearchMask,
                      &hdirFindHandle,
                      // find eeeeverything
                      FILE_ARCHIVED | FILE_HIDDEN | FILE_SYSTEM | FILE_READONLY,
                      &ffb3,
                      cbFFB3,
                      &ulFindCount,
                      FIL_STANDARD);

    // and start looping
    while (rc == NO_ERROR)
    {
        CHAR    szFile[CCHMAXPATH];
        ULONG   ulAction = 0;

        sprintf(szFile, "%s%s", szCurDir, // always has trailing "\"
                                ffb3.achName);

        // open file
        if (DosOpen(szFile,
                    &hfSysLevel,
                    &ulAction,
                    0,
                    FILE_NORMAL,
                    OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS,
                    OPEN_FLAGS_FAIL_ON_ERROR | OPEN_FLAGS_RANDOM | OPEN_SHARE_DENYWRITE | OPEN_ACCESS_READONLY,
                    NULL)
            == NO_ERROR)
        {
            AddOneSyslevel2Cnr(hwndCnr,
                               hfSysLevel,
                               szFile);
            DosClose(hfSysLevel);
        }

        rc = DosFindNext(hdirFindHandle,
                         &ffb3,
                         cbFFB3,
                         &ulFindCount);

    } /* endwhile */

    DosFindClose(hdirFindHandle);
}

/*
 *@@ cfgSyslevelInitPage:
 *
 *@@added V0.9.2 (2000-03-08) [umoeller]
 *@@changed V0.9.4 (2000-06-13) [umoeller]: group title was missing; fixed
 */

VOID cfgSyslevelInitPage(PCREATENOTEBOOKPAGE pcnbp,
                         ULONG flFlags)
{
    if (flFlags & CBI_INIT)
    {
        HWND hwndCnr = WinWindowFromID(pcnbp->hwndDlgPage, ID_XFDI_CNR_CNR);

        XFIELDINFO      xfi[5];
        PFIELDINFO      pfi = NULL;
        int             i = 0;

        // set group title V0.9.4 (2000-06-13) [umoeller]
        PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XFDI_CNR_GROUPTITLE,
                          pNLSStrings->pszSyslevelPage);

        // set up cnr details view
        xfi[i].ulFieldOffset = FIELDOFFSET(SYSLEVELRECORD, pszComponent);
        xfi[i].pszColumnTitle = pNLSStrings->pszSyslevelComponent;
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        xfi[i].ulFieldOffset = FIELDOFFSET(SYSLEVELRECORD, pszFile);
        xfi[i].pszColumnTitle = pNLSStrings->pszSyslevelFile;
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        xfi[i].ulFieldOffset = FIELDOFFSET(SYSLEVELRECORD, pszVersion);
        xfi[i].pszColumnTitle = pNLSStrings->pszSyslevelVersion;
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        xfi[i].ulFieldOffset = FIELDOFFSET(SYSLEVELRECORD, pszCSDCurrent);
        xfi[i].pszColumnTitle = pNLSStrings->pszSyslevelLevel;
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        xfi[i].ulFieldOffset = FIELDOFFSET(SYSLEVELRECORD, pszCSDPrevious);
        xfi[i].pszColumnTitle = pNLSStrings->pszSyslevelPrevious;
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        pfi = cnrhSetFieldInfos(hwndCnr,
                                xfi,
                                i,             // array item count
                                TRUE,          // draw lines
                                1);            // return second column

        BEGIN_CNRINFO()
        {
            cnrhSetView(CV_DETAIL | CA_DETAILSVIEWTITLES);
            cnrhSetSplitBarAfter(pfi);
            cnrhSetSplitBarPos(300);
        } END_CNRINFO(hwndCnr);
    }

    if (flFlags & CBI_SET)
    {
        HWND hwndCnr = WinWindowFromID(pcnbp->hwndDlgPage, ID_XFDI_CNR_CNR);

        AddSyslevelsForDir(hwndCnr,
                           "?:\\OS2\\INSTALL\\");
        AddSyslevelsForDir(hwndCnr,
                           "?:\\MMOS2\\INSTALL\\");
        AddSyslevelsForDir(hwndCnr,
                           "?:\\OS2\\DLL\\");
        AddSyslevelsForDir(hwndCnr,
                           "?:\\IBMI18N\\");
        AddSyslevelsForDir(hwndCnr,
                           "?:\\IBMCOM\\");
        AddSyslevelsForDir(hwndCnr,
                           "?:\\MPTN\\");
        AddSyslevelsForDir(hwndCnr,
                           "?:\\TCPIP\\");
        AddSyslevelsForDir(hwndCnr,
                           "?:\\IBMINST\\");    // networking installation
        AddSyslevelsForDir(hwndCnr,
                           "?:\\DMISL\\");
        AddSyslevelsForDir(hwndCnr,
                           "?:\\IBMLAN\\");    // peer
        AddSyslevelsForDir(hwndCnr,
                           "?:\\MUGLIB\\");    // peer
    }
}

/*
 *@@ cfgSyslevelItemChanged:
 *
 *@@added V0.9.2 (2000-03-08) [umoeller]
 */

MRESULT cfgSyslevelItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                               USHORT usItemID,
                               USHORT usNotifyCode,
                               ULONG ulExtra)
{
    MRESULT mrc = (MPARAM)0;

    /* switch (usItemID)
    {
    } */

    return (mrc);
}


