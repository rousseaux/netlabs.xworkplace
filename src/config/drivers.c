
/*
 *@@sourcefile drivers.c:
 *      implementation code for the "Drivers" page
 *      in the "OS/2 Kernel" object (XFldSystem).
 *
 *      This code was created with V0.9.0 in cfgsys.c and
 *      has been extracted to this file with V0.9.3. See cfgsys.c
 *      for introductory remarks.
 *
 *      Function prefix for this file:
 *      --  cfg*
 *
 *@@added V0.9.3 (2000-04-17) [umoeller]
 *@@header "config\cfgsys.h"
 */

/*
 *      Copyright (C) 1999-2000 Ulrich M�ller.
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
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\stringh.h"            // string helper routines
#include "helpers\textview.h"           // PM XTextView control
#include "helpers\threads.h"            // thread helpers
#include "helpers\winh.h"               // PM helper routines
#include "helpers\xstring.h"            // extended string helpers

// SOM headers which don't crash with prec. header files
#include "xfsys.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling

#include "config\cfgsys.h"              // XFldSystem CONFIG.SYS pages implementation
#include "config\drivdlgs.h"            // driver configuration dialogs

// other SOM headers
#pragma hdrstop                 // VAC++ keeps crashing otherwise

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

/*
 *@@ DRIVERPAGEDATA:
 *      page data for "Drivers" page. Stored in
 *      CREATENOTEBOOKPAGE.pUser.
 *
 *@@added V0.9.6 (2000-10-16) [umoeller]
 */

typedef struct _DRIVERPAGEDATA
{
    THREADINFO          tiDriversThread;

    // main linked list
    PLINKLIST   pllLists;

    // popup menu
    HWND        hwndDriverPopupMenu;
} DRIVERPAGEDATA, *PDRIVERPAGEDATA;

/* ******************************************************************
 *
 *   OS/2 Kernel "Drivers" page
 *
 ********************************************************************/

/*
 *@@ DRIVERRECORD:
 *      extended RECORDCORE structure for the
 *      container on the "Drivers" page.
 *
 *      This is used for both the actual driver
 *      records as well as the categories. Of
 *      course, all the extra fields are not
 *      used with categories, except for szParams.
 *
 *@@added V0.9.0 [umoeller]
 *@@changed V0.9.3 (2000-04-17) [umoeller]: added APIRET field
 */

typedef struct _DRIVERRECORD
{
    RECORDCORE  recc;
    CHAR        szDriverNameOnly[CCHMAXPATH];
            // extracted filename (w/out path)
    CHAR        szDriverNameFound[CCHMAXPATH];
            // as in CONFIG.SYS
    CHAR        szDriverNameFull[CCHMAXPATH];
            // as in CONFIG.SYS
    CHAR        szParams[500];
            // for drivers: parameters as in CONFIG.SYS;
            // for categories: buffer used for category title
    CHAR        szConfigSysLine[500];
            // full copy of CONFIG.SYS line

    // version info:
    CHAR        szVersion[100];
    CHAR        szVendor[100];
    APIRET      arc;
            // if != NO_ERROR, there was an error with the file

    // link to driver spec:
    PDRIVERSPEC pDriverSpec;
} DRIVERRECORD, *PDRIVERRECORD;

/*
 *@@ InsertDrivers:
 *      called from InsertDriverCategories for each driver
 *      category to be inserted into the "Drivers drivers"
 *      container.
 *
 *      pszHeading must have the heading for the new category
 *      (e.g. "CD-ROM drivers").
 *
 *      This function checks whether any driver is mentioned
 *      in *paDrivers; if so, the driver is appended under
 *      the header.
 *
 *@@added V0.9.0 [umoeller]
 *@@changed V0.9.1 (99-12-04) [umoeller]: fixed memory leaks
 *@@changed V0.9.3 (2000-04-10) [umoeller]: added pszVersion to DRIVERSPEC
 */

void InsertDrivers(HWND hwndCnr,              // in: container
                   PDRIVERRECORD preccRoot,   // in: root record core ("Drivers drivers")
                   PSZ pszHeading,            // in: heading for new category;
                                              // this is freed after this call, so this must
                                              // be copied
                   PSZ pszConfigSys,          // in: CONFIG.SYS file contents
                   PLINKLIST pllDriverSpecs)  // in: linked list of DRIVERSPEC structures
{
    // create category record
    PDRIVERRECORD preccHeading = (PDRIVERRECORD)cnrhAllocRecords(hwndCnr,
                                                                   sizeof(DRIVERRECORD),
                                                                   1);
    // we'll use the params buffer for the heading title
    strcpy(preccHeading->szParams, pszHeading);
    cnrhInsertRecords(hwndCnr,
                      (PRECORDCORE)preccRoot, // parent
                      (PRECORDCORE)preccHeading,
                      TRUE,     // invalidate
                      preccHeading->szParams,
                      CRA_RECORDREADONLY | CRA_COLLAPSED,
                      1);

    if (pszConfigSys)
    {
        // ULONG       ulSpec = 0;
        // walk thru drivers list
        PLISTNODE   pSpecNode = lstQueryFirstNode(pllDriverSpecs);
        while (pSpecNode)
        {
            // search CONFIG.SYS for driver spec's keyword
            BOOL fInsert = FALSE;
            PDRIVERSPEC pDriverSpec2Store = 0;
            CHAR szRestOfLine[1000] = "";

            PDRIVERSPEC pSpecThis = (PDRIVERSPEC)pSpecNode->pItemData;

            PSZ p = pszConfigSys;  // search pointer
            while (p)
            {
                if (p = csysGetParameter(p,
                                         pSpecThis->pszKeyword,
                                         szRestOfLine,
                                         sizeof(szRestOfLine)))
                {
                    // keyword found:
                    // extract first word after keyword (e.g. "F:\OS2\HPFS.IFS")
                    PSZ pEODriver = strchr(szRestOfLine, ' '),
                        pszFilename = 0;
                    if (pEODriver)
                        pszFilename = strhSubstr(szRestOfLine, pEODriver);
                    else
                        pszFilename = strdup(szRestOfLine);
                                // free'd below

                    if (pszFilename)
                    {
                        // extract pure filename w/out path
                        PSZ pLastBackslash = strrchr(pszFilename, '\\'),
                            pComp;
                        if (pLastBackslash)
                            // path specified: compare filename only
                            pComp = pLastBackslash + 1;
                        else
                            // no path specified: compare all
                            pComp = pszFilename;

                        if (stricmp(pComp,
                                    pSpecThis->pszFilename)
                               == 0)
                        {
                            // yup: mark to insert
                            fInsert = TRUE;
                            pDriverSpec2Store = pSpecThis;
                            p = 0; // break;
                        }

                        free(pszFilename);
                    } // end if (pszFilename)
                } // end if if (p = strhGetParameter(p, ...
            } // end while (p)

            if (fInsert)
            {
                PEXECUTABLE pExec = NULL;
                APIRET      arc = NO_ERROR;

                // insert driver into cnr:
                PDRIVERRECORD precc
                    = (PDRIVERRECORD)cnrhAllocRecords(hwndCnr,
                                                      sizeof(DRIVERRECORD),
                                                      1);

                // extract full file name
                PSZ pEODriver = strchr(szRestOfLine, ' ');
                if (pEODriver)
                {
                    PSZ pSpace2 = pEODriver;
                    strhncpy0(precc->szDriverNameFound,
                              szRestOfLine,
                              (pEODriver - szRestOfLine));

                    // skipp addt'l spaces
                    while (*pSpace2 == ' ')
                        pSpace2++;
                    // copy params
                    strcpy(precc->szParams, pSpace2);
                }
                else
                {
                    strcpy(precc->szDriverNameFound,
                           szRestOfLine);
                    precc->szParams[0] = 0;
                }

                // extract file name only from full name
                pEODriver = strrchr(precc->szDriverNameFound, '\\');
                if (pEODriver)
                    strcpy(precc->szDriverNameOnly, pEODriver + 1);
                else
                    // no path given: copy all
                    strcpy(precc->szDriverNameOnly, precc->szDriverNameFound);

                // create full name
                if (pDriverSpec2Store->ulFlags & DRVF_BASEDEV)
                    // BASEDEVs have no path, so we provide this
                    sprintf(precc->szDriverNameFull,
                            "%c:\\OS2\\BOOT\\",
                            doshQueryBootDrive());
                strcat(precc->szDriverNameFull,
                       precc->szDriverNameFound);

                // compose full line as in CONFIG.SYS,
                // for replacing the line later maybe
                sprintf(precc->szConfigSysLine,
                        "%s%s",
                        pDriverSpec2Store->pszKeyword,
                        szRestOfLine);

                // get BLDLEVEL
                if ((arc = doshExecOpen(precc->szDriverNameFull,
                                        &pExec))
                            == NO_ERROR)
                {
                    if ((arc = doshExecQueryBldLevel(pExec))
                                    == NO_ERROR)
                    {
                        if (pExec->pszVersion)
                        {
                            strcpy(precc->szVersion, pExec->pszVersion);
                            pDriverSpec2Store->pszVersion = strdup(pExec->pszVersion);
                        }

                        if (pExec->pszVendor)
                            strcpy(precc->szVendor, pExec->pszVendor);
                    }
                    doshExecClose(pExec);
                }
                else
                    // error:
                    precc->arc = arc;

                // store driver specs
                precc->pDriverSpec = pDriverSpec2Store; // can be NULL

                cnrhInsertRecords(hwndCnr,
                                  (PRECORDCORE)preccHeading, // parent
                                  (PRECORDCORE)precc,
                                  TRUE, // invalidate
                                  precc->pDriverSpec->pszDescription, // precc->szDriverNameOnly,
                                  CRA_RECORDREADONLY | CRA_COLLAPSED,
                                  1);
            }

            // next driver spec in list
            pSpecNode = pSpecNode->pNext;
        } // end while (pSpecNode)
    } // end if (pszConfigSys)
}

/*
 *@@ FreeDriverSpec:
 *
 *@@added V0.9.5 (2000-08-30) [umoeller]
 */

VOID FreeDriverSpec(PDRIVERSPEC pSpecThis)
{
    if (pSpecThis->pszKeyword)
        free(pSpecThis->pszKeyword);
    if (pSpecThis->pszFilename)
        free(pSpecThis->pszFilename);
    if (pSpecThis->pszDescription)
        free(pSpecThis->pszDescription);
    if (pSpecThis->pszVersion)
        free(pSpecThis->pszVersion);

    free (pSpecThis);
}

/*
 *@@ InsertDriverCategories:
 *      this gets called ONCE from cfgDriversInitPage
 *      with the contents of the DRVRSxxx.TXT file.
 *      This calls InsertDrivers in turn for each
 *      driver category which was found.
 *
 *      At this point, cfgDriversInitPage has only
 *      created the root ("Driver categories") record
 *      in hwndCnr.
 *
 *      We use pcnbp->pUser as a PLINKLIST, which
 *      holds other PLINKLIST's in turn which in
 *      turn hold the DRIVERSPEC's. Only this way
 *      we can properly free the items.
 *
 *@@changed V0.9.1 (99-12-04) [umoeller]: fixed memory leaks
 *@@changed V0.9.3 (2000-04-01) [umoeller]: added DRVF_CMDREF support, finally
 *@@changed V0.9.3 (2000-04-10) [umoeller]: now zeroing DRIVERSPEC properly
 *@@changed V0.9.3 (2000-05-21) [umoeller]: DRVF_NOPARAMS wasn't recognized, fixed
 */

PLINKLIST InsertDriverCategories(HWND hwndCnr,
                                 PDRIVERRECORD preccRoot,
                                        // in: root record core ("Drivers drivers")
                                 PSZ pszConfigSys,
                                        // in: CONFIG.SYS file contents
                                 PSZ pszDriverSpecsFile)
                                        // in: DRVRSxxx.TXT file contents
{
    // linked list of PLINKLIST items to return
    PLINKLIST pllReturn = lstCreate(FALSE);

    PSZ     pSearch = pszDriverSpecsFile;

    BOOL    fCrashed = FALSE;

    // parse the thing
    while (    (pSearch = strstr(pSearch, "CATEGORY"))
            && (!fCrashed)
          )
    {
        // category found:
        // go for the drivers

        // PSZ     p1 = pSearch;
                // pStartOfBlock = NULL;
        PSZ     pszCategoryTitle = strhQuote(pSearch,
                                             '"',     // extract title string
                                             &pSearch); // out: char after closing char
        // extract stuff between "{...}"
        PSZ     pszBlock = strhExtract(pSearch,
                                       '{',
                                       '}',
                                       &pSearch); // out: char after closing char
        if (pszBlock)
        {
            PSZ pSearch2 = pszBlock;

            // create linked list of DRIVERSPEC's;
            // this list gets in turn stored in
            // pllReturn, so all the DRIVERSPEC's
            // can be freed in cfgDriversInitPage
            PLINKLIST   pllDriverSpecsForCategory = lstCreate(FALSE);
            ULONG       ulDriversFound = 0;

            // for-each-DRIVER loop
            while ((pSearch2) && (!fCrashed))
            {
                PSZ pszDriverSpec = strhExtract(pSearch2,
                                                '(',
                                                ')',
                                                &pSearch2);
                if (pszDriverSpec)
                do {
                    // (...) block found (after "DRIVER" keyword):
                    // tokenize that
                    // BOOL        fOK = TRUE;
                    PSZ         pSearch3 = pszDriverSpec;
                    PDRIVERSPEC pSpec = malloc(sizeof(DRIVERSPEC));
                    memset(pSpec, 0, sizeof(DRIVERSPEC));   // V0.9.3 (2000-04-10) [umoeller]

                    pSpec->ulFlags = 0;

                    // get BASEDEV etc.
                    pSpec->pszKeyword = strhQuote(pSearch3,
                                                  '"',
                                                  &pSearch3);
                    if (!pSpec->pszKeyword)
                        break;  // do

                    // get driver filename
                    pSpec->pszFilename = strhQuote(pSearch3,
                                                   '"',
                                                   &pSearch3);
                    if (!pSpec->pszFilename)
                        break;  // do

                    // get driver description
                    pSpec->pszDescription = strhQuote(pSearch3,
                                                      '"',
                                                      &pSearch3);
                    if (!pSpec->pszDescription)
                        break;  // do

                    if (!stricmp(pSpec->pszKeyword, "BASEDEV="))
                        pSpec->ulFlags |= DRVF_BASEDEV;
                    else if (!stricmp(pSpec->pszKeyword, "DEVICE="))
                        pSpec->ulFlags |= DRVF_DEVICE;
                    else if (!stricmp(pSpec->pszKeyword, "IFS="))
                        pSpec->ulFlags |= DRVF_IFS;
                    else
                        // RUN=, CALL=, etc.
                        pSpec->ulFlags |= DRVF_OTHER;

                    if (strstr(pszDriverSpec, "DRVF_CMDREF"))
                        // DRVF_CMDREF specified:
                        pSpec->ulFlags |= DRVF_CMDREF;

                    if (strstr(pszDriverSpec, "DRVF_NOPARAMS"))
                        // DRVF_CMDREF specified:
                        pSpec->ulFlags |= DRVF_NOPARAMS;

                    // the following routine sets up
                    // the driver configuration dialog
                    // based on the filename, if one
                    // exists; this is in drivdlgs.c
                    // so it can be extended more easily
                    if (!drvConfigSupported(pSpec))
                    {
                        // crash:
                        fCrashed = TRUE;
                        break;
                    }

                    lstAppendItem(pllDriverSpecsForCategory, pSpec);

                    ulDriversFound++;

                    free(pszDriverSpec);
                } while (FALSE);
                else
                    // (...) block not found:
                    pSearch2 = 0;

            } // end while (pSearch2) (DRIVER loop)

            if ((ulDriversFound) && (!fCrashed))
            {
                // any drivers found for this category:
                // insert into container
                InsertDrivers(hwndCnr,
                              preccRoot,
                              (pszCategoryTitle)
                                    ? pszCategoryTitle
                                    : "Syntax error with CATEGORY title",
                              pszConfigSys,
                              pllDriverSpecsForCategory);

                // store the linked list of DRIVERSPEC's
                // in pllReturn
                lstAppendItem(pllReturn, pllDriverSpecsForCategory);
            }
            else
                // no drivers found: destroy the list
                // again, it's empty anyway
                lstFree(&pllDriverSpecsForCategory);

            free(pszBlock);

        } // end if (pszBlock)
        else
        {
            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                   "Block after DRIVERSPEC not found.");
            pSearch++;
        }

        // free category title; this has
        // been copied to the DRIVERRECORD by InsertDrivers
        if (pszCategoryTitle)
            free(pszCategoryTitle);

    } // while (pSearch = strstr(pSearch, "CATEGORY"))

    return (pllReturn);
}

/*
 *@@ fntDriversThread:
 *      "insert drivers" thread started by cfgDriversInitPage.
 *      This fills the drivers tree view with drivers. Since
 *      this may take several seconds, we rather not block
 *      the user interface, but do this in a second thread.
 *
 *      This thread is created with a PM message queue.
 *
 *@@added V0.9.1 (2000-02-11) [umoeller]
 */

void _Optlink fntDriversThread(PTHREADINFO pti)
{
    PCREATENOTEBOOKPAGE pcnbpDrivers = (PCREATENOTEBOOKPAGE)pti->ulData;
    PDRIVERPAGEDATA pPageData = pcnbpDrivers->pUser;

    HWND            hwndDriversCnr = WinWindowFromID(pcnbpDrivers->hwndDlgPage,
                                                     ID_OSDI_DRIVR_CNR);
    PSZ             pszConfigSys = NULL;
    PDRIVERRECORD   preccRoot = 0;
                    // precc = 0;
    // PNLSSTRINGS     pNLSStrings = cmnQueryNLSStrings();
    // PCKERNELGLOBALS pKernelGlobals = krnQueryGlobals();

    // set wait pointer; this is handled by notebook.c
    pcnbpDrivers->fShowWaitPointer = TRUE;

    // clear container
    WinSendMsg(hwndDriversCnr,
               CM_REMOVERECORD,
               (MPARAM)0,
               MPFROM2SHORT(0, // all records
                            CMA_FREE | CMA_INVALIDATE));

    // create root record; freed automatically
    preccRoot = (PDRIVERRECORD)cnrhAllocRecords(hwndDriversCnr,
                                                sizeof(DRIVERRECORD),
                                                1);
    cnrhInsertRecords(hwndDriversCnr,
                      NULL,  // parent
                      (PRECORDCORE)preccRoot,
                      TRUE, // invalidate
                      cmnGetString(ID_XSSI_DRIVERCATEGORIES),  // pszDriverCategories
                      CRA_SELECTED | CRA_RECORDREADONLY | CRA_EXPANDED,
                      1);

    // load CONFIG.SYS text; freed below
    if (csysLoadConfigSys(NULL, &pszConfigSys) != NO_ERROR)
        winhDebugBox(HWND_DESKTOP,
                 "XWorkplace",
                 "XWorkplace was unable to open the CONFIG.SYS file.");
    else
    {
        // now parse DRVRSxxx.TXT in XWorkplace /HELP dir
        CHAR    szDriverSpecsFilename[CCHMAXPATH];
        PSZ     pszDriverSpecsFile = NULL;

        cmnQueryXWPBasePath(szDriverSpecsFilename);
        sprintf(szDriverSpecsFilename + strlen(szDriverSpecsFilename),
                "\\help\\drvrs%s.txt",
                cmnQueryLanguageCode());

        // load drivers.txt file; freed below
        if (doshLoadTextFile(szDriverSpecsFilename,
                             &pszDriverSpecsFile)
                != NO_ERROR)
            winhDebugBox(HWND_DESKTOP,
                     szDriverSpecsFilename,
                     "XWorkplace was unable to open the driver specs file.");
        else
        {
            // drivers file successfully loaded:
            // parse file
            pPageData->pllLists = InsertDriverCategories(hwndDriversCnr,
                                                         preccRoot,
                                                         pszConfigSys,
                                                         pszDriverSpecsFile);
                // this returns a PLINKLIST containing LINKLIST's
                // containing DRIVERSPEC's...

            free(pszDriverSpecsFile);
        }

        free(pszConfigSys);
    }

    pcnbpDrivers->fShowWaitPointer = FALSE;
}

/*
 *@@ G_ampDriversPage:
 *      resizing information for "Drives" page.
 *      Stored in CREATENOTEBOOKPAGE of the
 *      respective "add notebook page" method.
 *
 *@@added V0.9.4 (2000-08-08) [umoeller]
 */

MPARAM G_ampDriversPage[] =
    {
        MPFROM2SHORT(ID_OSDI_DRIVR_CNR, XAC_SIZEX | XAC_SIZEY),
        MPFROM2SHORT(ID_OSDI_DRIVR_GROUP1, XAC_SIZEX | XAC_SIZEY),
        MPFROM2SHORT(ID_OSDI_DRIVR_GROUP2, XAC_MOVEX | XAC_MOVEY),
        MPFROM2SHORT(ID_OSDI_DRIVR_STATICDATA, XAC_MOVEX | XAC_SIZEY),
        MPFROM2SHORT(ID_OSDI_DRIVR_PARAMS_TXT, XAC_MOVEX),
        MPFROM2SHORT(ID_OSDI_DRIVR_PARAMS, XAC_MOVEX),
        MPFROM2SHORT(ID_OSDI_DRIVR_CONFIGURE, XAC_MOVEX),
        MPFROM2SHORT(ID_OSDI_DRIVR_APPLYTHIS, XAC_MOVEX)
    };

extern MPARAM *G_pampDriversPage = G_ampDriversPage;
extern ULONG G_cDriversPage = sizeof(G_ampDriversPage) / sizeof(G_ampDriversPage[0]);

/*
 *@@ cfgDriversInitPage:
 *      notebook callback function (notebook.c) for the
 *      "Drivers" page in the "OS/2 Kernel" object.
 *      Sets the controls on the page according to the CONFIG.SYS
 *      statements.
 *
 *      See cfgDriversItemChanged for information how the
 *      driver dialogs must interact with the main "Drivers"
 *      settings page.
 *
 *@@added V0.9.0 [umoeller]
 *@@changed V0.9.1 (99-12-04) [umoeller]: fixed memory leaks
 *@@changed V0.9.3 (2000-04-11) [umoeller]: added pszVersion to DRIVERSPEC
 *@@changed V0.9.6 (2000-10-16) [umoeller]: fixed excessive menu creation
 */

VOID cfgDriversInitPage(PCREATENOTEBOOKPAGE pcnbp,
                        ULONG flFlags)  // notebook info struct
{
    HWND hwndCnr = WinWindowFromID(pcnbp->hwndDlgPage, ID_OSDI_DRIVR_CNR);

    if (flFlags & CBI_INIT)
    {
        HWND hwndTextView;
        // SWP  swpMLE;
        // XTEXTVIEWCDATA xtxCData;
        PDRIVERPAGEDATA pPageData = 0;

        // load the driver dialog definitions
        drvLoadPlugins(WinQueryAnchorBlock(pcnbp->hwndDlgPage));

        BEGIN_CNRINFO()
        {
            cnrhSetView(CV_TREE | CA_TREELINE | CV_TEXT
                            | CA_OWNERDRAW);
            cnrhSetTreeIndent(20);
            // cnrhSetSortFunc(fnCompareName);
        } END_CNRINFO(hwndCnr);

        // replace MLE with XTextView control
        txvRegisterTextView(WinQueryAnchorBlock(pcnbp->hwndDlgPage));
        hwndTextView = txvReplaceWithTextView(pcnbp->hwndDlgPage,
                                              ID_OSDI_DRIVR_STATICDATA,
                                              WS_VISIBLE | WS_TABSTOP,
                                              XTXF_VSCROLL | XTXF_AUTOVHIDE
                                                | XTXF_HSCROLL | XTXF_AUTOHHIDE,
                                              2);
        winhSetWindowFont(hwndTextView, (PSZ)cmnQueryDefaultFont());

        // create page data
        pPageData = pcnbp->pUser = malloc(sizeof(DRIVERPAGEDATA));
        memset(pPageData, 0, sizeof(DRIVERPAGEDATA));

        // load popup
        pPageData->hwndDriverPopupMenu = WinLoadMenu(HWND_OBJECT,
                                                     cmnQueryNLSModuleHandle(FALSE),
                                                     ID_XSM_DRIVERS_SEL);
    }

    if (flFlags & CBI_SET)
    {
        PDRIVERPAGEDATA pPageData = (PDRIVERPAGEDATA)pcnbp->pUser;
        // set data: create drivers thread, which inserts
        // the drivers tree
        if (!thrQueryID(&pPageData->tiDriversThread))
        {
            thrCreate(&pPageData->tiDriversThread,
                      fntDriversThread,
                      NULL, // running flag
                      "InsertDrivers",
                      THRF_PMMSGQUEUE,        // msgq
                      (ULONG)pcnbp);        // data
                // this creates a PLINKLIST in pcnbp->pUser
        }
    }

    if (flFlags & CBI_DESTROY)
    {
        PDRIVERPAGEDATA pPageData = (PDRIVERPAGEDATA)pcnbp->pUser;
        // clean up the linked list of linked lists
        // which was created above
        if (pPageData->pllLists)
        {
            PLISTNODE pListNode = lstQueryFirstNode(pPageData->pllLists);
            while (pListNode)
            {
                PLINKLIST pllSpecsForCategory = (PLINKLIST)pListNode->pItemData;
                if (pllSpecsForCategory)
                {
                    PLISTNODE pSpecNode = lstQueryFirstNode(pllSpecsForCategory);
                    while (pSpecNode)
                    {
                        PDRIVERSPEC pSpecThis = (PDRIVERSPEC)pSpecNode->pItemData;
                        if (pSpecThis)
                        {
                            FreeDriverSpec(pSpecThis);
                        }

                        pSpecNode = pSpecNode->pNext;
                    }

                    lstFree(&pllSpecsForCategory);
                }
                pListNode = pListNode->pNext;
            }

            lstFree(&pPageData->pllLists);
        }

        WinDestroyWindow(pPageData->hwndDriverPopupMenu);

        // unload the driver plugins
        drvUnloadPlugins();
    }
}

/*
 *@@ cfgDriversItemChanged:
 *      notebook callback function (notebook.c) for the
 *      "Drivers" page in the "OS/2 Kernel" object.
 *      Reacts to changes of any of the dialog controls.
 *
 *      When the "Configure" button is pressed on this
 *      page, this function attempts to load the dialog
 *      which is specified with the DRIVERSPEC structure
 *      of the currently selected driver (initialized
 *      by cfgDriversInitPage). In that structure, we
 *      have entries for the dialog template as well as
 *      the dialog func.
 *
 *      Currently, all the driver dialog funcs are in
 *      the drivdlgs.c file. See remarks there for
 *      details.
 *
 *@@added V0.9.0 [umoeller]
 *@@changed V0.9.3 (2000-04-17) [umoeller]: added error messages display
 *@@changed V0.9.12 (2001-04-28) [umoeller]: fixed vendor display
 *@@changed V0.9.12 (2001-05-03) [umoeller]: removed stupid SYSxxx messages in display
 */

MRESULT cfgDriversItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                              ULONG ulItemID,
                              USHORT usNotifyCode,
                              ULONG ulExtra)      // for checkboxes: contains new state
{
    MRESULT mrc = (MRESULT)0;

    switch (ulItemID)
    {
        /*
         * ID_OSDI_DRIVR_CNR:
         *      drivers container
         */

        case ID_OSDI_DRIVR_CNR:
            switch (usNotifyCode)
            {

                /*
                 * CN_EMPHASIS:
                 *      container record selection changed
                 *      (new driver selected):
                 *      update the other fields on the page
                 */

                case CN_EMPHASIS:
                {
                    PSZ     pszParams = "";
                    XSTRING strText2MLE;
                    BOOL fEnable = FALSE,
                         fAcceptsParams = FALSE;
                    // PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();

                    xstrInit(&strText2MLE, 200);

                    if (pcnbp->preccLastSelected)
                    {
                        PDRIVERRECORD precc = (PDRIVERRECORD)pcnbp->preccLastSelected;

                        // filename
                        pszParams = precc->szParams;

                        if (precc->pDriverSpec)
                        {
                            xstrcpy(&strText2MLE,
                                    precc->pDriverSpec->pszDescription,
                                    0);
                            xstrcatc(&strText2MLE, '\n');
                            xstrcat(&strText2MLE, "File: ", 0);
                            xstrcat(&strText2MLE,
                                    precc->szDriverNameFull, 0);
                            xstrcatc(&strText2MLE, '\n');

                            if (precc->arc == NO_ERROR)
                            {
                                // driver description
                                xstrcat(&strText2MLE,
                                        cmnGetString(ID_XSSI_DRIVERVERSION),  0); // pszDriverVersion
                                xstrcat(&strText2MLE,
                                         precc->szVersion, 0);

                                xstrcatc(&strText2MLE, '\n');
                                xstrcat(&strText2MLE,
                                        cmnGetString(ID_XSSI_DRIVERVENDOR),  0); // pszDriverVendor
                                xstrcat(&strText2MLE,
                                         precc->szVendor, 0);
                            }
                            /*
                                removed this for the release verion
                                V0.9.12 (2001-05-03) [umoeller]
                                this kept displaying "cannot be run in an os2 session",
                                which wasn't very helpful.
                            */
                            #ifdef __DEBUG__
                            else
                            {
                                // error:
                                CHAR szErr[100];
                                sprintf(szErr, "[error %u]", precc->arc);
                                xstrcat(&strText2MLE, szErr, 0);
                            }
                            #endif

                            xstrcatc(&strText2MLE, '\n'); // fixed V0.9.12 (2001-04-28) [umoeller]

                            // enable "Configure" button if dialog defined
                            if (precc->pDriverSpec->pfnShowDriverDlg)
                                fEnable = TRUE;
                            // accepts parameters?
                            if ((precc->pDriverSpec->ulFlags & DRVF_NOPARAMS) == 0)
                                fAcceptsParams = TRUE;
                        }

                        // disable "Apply" button
                        winhEnableDlgItem(pcnbp->hwndDlgPage,
                                          ID_OSDI_DRIVR_APPLYTHIS,
                                          FALSE);
                    }

                    if (strText2MLE.ulLength)
                    {
                        WinSetDlgItemText(pcnbp->hwndDlgPage,
                                          ID_OSDI_DRIVR_STATICDATA,
                                          strText2MLE.psz);
                    }
                    else
                        WinSetDlgItemText(pcnbp->hwndDlgPage,
                                          ID_OSDI_DRIVR_STATICDATA,
                                          "");

                    xstrClear(&strText2MLE);

                    WinSetDlgItemText(pcnbp->hwndDlgPage,
                                      ID_OSDI_DRIVR_PARAMS,
                                      pszParams);
                    winhEnableDlgItem(pcnbp->hwndDlgPage,
                                      ID_OSDI_DRIVR_PARAMS,
                                      fAcceptsParams);
                    winhEnableDlgItem(pcnbp->hwndDlgPage,
                                      ID_OSDI_DRIVR_CONFIGURE,
                                      fEnable);
                break; } // CN_EMPHASIS

                /*
                 * CN_ENTER:
                 *      enter or double-click on record
                 */

                case CN_ENTER:
                {
                    PDRIVERRECORD precc = (PDRIVERRECORD)pcnbp->preccLastSelected;
                    if (precc)
                        if (precc->pDriverSpec->pfnShowDriverDlg)
                            // simulate "configure" button
                            WinPostMsg(pcnbp->hwndDlgPage,
                                       WM_COMMAND,
                                       (MPARAM)ID_OSDI_DRIVR_CONFIGURE,
                                       MPFROM2SHORT(CMDSRC_OTHER, TRUE));
                break; }

                /*
                 * CN_CONTEXTMENU:
                 *      cnr context menu requested
                 *      for driver
                 */

                case CN_CONTEXTMENU:
                {
                    HWND    hPopupMenu = NULLHANDLE;

                    // we store the container and recc.
                    // in the CREATENOTEBOOKPAGE structure
                    // so that the notebook.c function can
                    // remove source emphasis later automatically
                    pcnbp->hwndSourceCnr = pcnbp->hwndControl;
                    pcnbp->preccSource = (PRECORDCORE)ulExtra;
                    if (pcnbp->preccSource)
                    {
                        PDRIVERPAGEDATA pPageData = (PDRIVERPAGEDATA)pcnbp->pUser;
                        BOOL fEnableCmdref = FALSE;
                        PDRIVERRECORD precc = (PDRIVERRECORD)pcnbp->preccSource;
                        if (precc->pDriverSpec)
                            if (precc->pDriverSpec->ulFlags & DRVF_CMDREF)
                                // help available in CMDREF.INF:
                                fEnableCmdref = TRUE;
                        // popup menu on container recc:
                        hPopupMenu = pPageData->hwndDriverPopupMenu;
                        WinEnableMenuItem(hPopupMenu,
                                          ID_XSMI_DRIVERS_CMDREFHELP,
                                          fEnableCmdref);

                    }
                    else
                    {
                        // popup menu on cnr whitespace
                    }

                    if (hPopupMenu)
                        cnrhShowContextMenu(pcnbp->hwndControl,  // cnr
                                            (PRECORDCORE)pcnbp->preccSource,
                                            hPopupMenu,
                                            pcnbp->hwndDlgPage);    // owner
                break; } // CN_CONTEXTMENU
            }
        break;

        /*
         * ID_OSDI_DRIVR_CONFIGURE:
         *      "Configure..." button (only enabled
         *      if dialog has been set up in DRIVERSPECs):
         *      open that dialog then
         */

        case ID_OSDI_DRIVR_CONFIGURE:
            if (pcnbp->preccLastSelected)
            {
                PDRIVERRECORD precc = (PDRIVERRECORD)pcnbp->preccLastSelected;
                if (    (precc->pDriverSpec)
                     && (precc->pDriverSpec->pfnShowDriverDlg)
                   )
                {
                    // OK, we have a valid dialog specification:
                    DRIVERDLGDATA ddd = {0};
                    HWND hwndMLE = WinWindowFromID(pcnbp->hwndDlgPage,
                                                   ID_OSDI_DRIVR_PARAMS);
                    PSZ  pszParamsBackup = NULL;

                    // set up DRIVERDLGDATA structure
                    ddd.pvKernel = (PVOID)pcnbp->somSelf;
                    ddd.pcszKernelTitle = _wpQueryTitle(pcnbp->somSelf);
                    ddd.pDriverSpec = precc->pDriverSpec;
                    WinQueryWindowText(hwndMLE,
                                       sizeof(ddd.szParams),
                                       ddd.szParams);

                    // backup parameters
                    if (    (pszParamsBackup = strdup(ddd.szParams))
                         && (precc->pDriverSpec->pfnShowDriverDlg(pcnbp->hwndDlgPage,
                                                                  &ddd))
                       )
                    {
                        // "OK" pressed:
                        // the dialog func should have modified
                        // szParams now,
                        // transfer szParams to MLE on page
                        if (strcmp(pszParamsBackup, ddd.szParams))
                        {
                            // something changed:
                            WinSetWindowText(hwndMLE,
                                             ddd.szParams);
                            // re-enable the "Apply" button also
                            winhEnableDlgItem(pcnbp->hwndDlgPage,
                                              ID_OSDI_DRIVR_APPLYTHIS,
                                              TRUE);
                        }
                    }

                    if (pszParamsBackup)
                        free(pszParamsBackup);
                }
            }
        break;

        /*
         * ID_OSDI_DRIVR_PARAMS:
         *      "Parameters" MLE
         */

        case ID_OSDI_DRIVR_PARAMS:
            if (usNotifyCode == MLN_CHANGE)
                // enable "Apply" button
                winhEnableDlgItem(pcnbp->hwndDlgPage,
                                  ID_OSDI_DRIVR_APPLYTHIS,
                                  TRUE);
        break;

        /*
         * ID_XSMI_DRIVERS_CMDREFHELP:
         *      "show CMDREF help" context menu item
         */

        case ID_XSMI_DRIVERS_CMDREFHELP:
        {
            CHAR szParams[200] = "cmdref.inf ";
            PROGDETAILS pd;
            memset(&pd, 0, sizeof(PROGDETAILS));
            pd.Length = sizeof(PROGDETAILS);
            pd.progt.progc = PROG_PM;
            pd.progt.fbVisible = SHE_VISIBLE;
            pd.pszExecutable = "view.exe";
            // append short driver name to params (cmdref.inf)
            strcat(szParams, ((PDRIVERRECORD)pcnbp->preccSource)->szDriverNameOnly);

            WinStartApp(NULLHANDLE,         // hwndNotify
                        &pd,
                        szParams,
                        NULL,               // reserved
                        0);                 // options
        break; }

        /*
         * ID_OSDI_DRIVR_APPLYTHIS:
         *      "Apply" button: write current item
         *      back to CONFIG.SYS
         */

        case ID_OSDI_DRIVR_APPLYTHIS:
        {
            PDRIVERRECORD precc = (PDRIVERRECORD)pcnbp->preccLastSelected;
            // PCKERNELGLOBALS   pKernelGlobals = krnQueryGlobals();
            CHAR szNewParams[500];
            CHAR szNewLine[1500];
            WinQueryDlgItemText(pcnbp->hwndDlgPage, ID_OSDI_DRIVR_PARAMS,
                                sizeof(szNewParams),
                                szNewParams);
            sprintf(szNewLine, "%s%s %s",
                    precc->pDriverSpec->pszKeyword,
                    precc->szDriverNameFound,
                    szNewParams);

            if (!strcmp(precc->szConfigSysLine, szNewLine))
            {
                // no changes made:
                PSZ  apszTable = precc->szDriverNameOnly;
                cmnMessageBoxMsgExt(pcnbp->hwndFrame, // pcnbp->hwndPage,
                                    100,
                                    &apszTable,
                                    1,
                                    156,        // "no changes made"
                                    MB_OK);
            }
            else
            {
                PSZ  apszTable[2];
                // have the user confirm this
                apszTable[0] = precc->szConfigSysLine;
                apszTable[1] = szNewLine;

                if (cmnMessageBoxMsgExt(pcnbp->hwndFrame, // pcnbp->hwndPage,
                                        100,
                                        apszTable,
                                        2,   // entries
                                        155, // "sure?"
                                        MB_YESNO | MB_DEFBUTTON2)
                     == MBID_YES)
                {
                    PSZ     pszConfigSys = NULL;

                    if (csysLoadConfigSys(NULL, &pszConfigSys))
                        winhDebugBox(pcnbp->hwndFrame,
                                 "XWorkplace",
                                 "XWorkplace was unable to open the CONFIG.SYS file.");
                    else
                    {
                        CHAR    szBackup[CCHMAXPATH];
                        ULONG   ulOfs = 0;
                        strhFindReplace(&pszConfigSys,
                                        &ulOfs,
                                        precc->szConfigSysLine,
                                        szNewLine);
                        // update record core
                        strcpy(precc->szConfigSysLine, szNewLine);
                        strcpy(precc->szParams, szNewParams);
                        // write file!
                        if (csysWriteConfigSys(NULL,
                                               pszConfigSys,
                                               szBackup)
                                == NO_ERROR)
                            // "file written" msg
                            cmnMessageBoxMsg(pcnbp->hwndFrame, // pcnbp->hwndPage,
                                             100,
                                             136,   // @@todo
                                             MB_OK);
                        else
                            winhDebugBox(NULLHANDLE, "Error", "Error writing CONFIG.SYS");

                        free(pszConfigSys);
                    }
                }
            }
        break; }
    }

    return (mrc);
}


