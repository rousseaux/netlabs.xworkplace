
/*
 *@@sourcefile object.c:
 *      implementation code for XFldObject class.
 *
 *      This file is ALL new with V0.9.0.
 *
 *      Function prefix for this file:
 *      --  obj*
 *
 *@@added V0.9.0 [umoeller]
 *@@header "filesys\object.h"
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

#define INCL_DOSPROCESS
#define INCL_DOSEXCEPTIONS
#define INCL_DOSSEMAPHORES
#define INCL_DOSERRORS
#define INCL_WININPUT
#define INCL_WINWINDOWMGR
#define INCL_WINPOINTERS
#define INCL_WINTIMER
#define INCL_WINMENUS
#define INCL_WINDIALOGS
#define INCL_WINBUTTONS
#define INCL_WINENTRYFIELDS
#define INCL_WINSTDCNR
#define INCL_WINSTDBOOK
#include <os2.h>

// C library headers
#include <stdio.h>
#include <malloc.h>
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\except.h"             // exception handling
#include "helpers\comctl.h"             // common controls (window procs)
#include "helpers\cnrh.h"               // container helper routines
#include "helpers\stringh.h"            // string helper routines
#include "helpers\winh.h"               // PM helper routines

#include "helpers\wphandle.h"           // Henk Kelder's HOBJECT handling

// SOM headers which don't crash with prec. header files
#include "xfobj.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\cnrsort.h"             // container sort comparison functions
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\notebook.h"            // generic XWorkplace notebook handling
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

// headers in /hook
#include "hook\xwphook.h"

#include "filesys\object.h"             // XFldObject implementation

#include "config\hookintf.h"            // daemon/hook interface

// other SOM headers
#pragma hdrstop
#include <wpdesk.h>                     // WPDesktop
#include <wpshadow.h>
#include "filesys\folder.h"             // XFolder implementation

/* ******************************************************************
 *                                                                  *
 *   Object "Internals" page                                        *
 *                                                                  *
 ********************************************************************/

VOID FillCnrWithObjectUsage(HWND hwndCnr, WPObject *pObject);

/*
 *@@ OBJECTUSAGERECORD:
 *      extended RECORDCORE structure for
 *      "Object" settings page.
 *      All inserted records are of this type.
 */

typedef struct _OBJECTUSAGERECORD
{
    RECORDCORE     recc;
    CHAR           szText[1000];        // should suffice even for setup strings
} OBJECTUSAGERECORD, *POBJECTUSAGERECORD;

/*
 *@@ OBJECTUSAGEDATA:
 *
 */

typedef struct _OBJECTUSAGEDATA
{
    WPObject *pObject;              // object to query
    CHAR    szDlgTitle[256];        // dialog title
    CHAR    szIntroText[256];       // intro text above container
    CHAR    szHelpLibrary[CCHMAXPATH]; // help library
    ULONG   ulHelpPanel;            // help panel; if 0, the "Help"
                                    // button is disabled
    HWND    hwndCnr;                // internal use, don't bother
} OBJECTUSAGEDATA, *POBJECTUSAGEDATA;

/*
 * XFOBJWINDATA:
 *      structure used with "Object" page for data
 *      exchange with XFldObject instance data.
 *      Created in WM_INITDLG.
 */

typedef struct _XFOBJWINDATA
{
    SOMAny          *somSelf;
    CHAR            szOldID[CCHMAXPATH];
    HWND            hwndCnr;
    CHAR            szOldObjectID[256];
    BOOL            fEscPressed;
    PRECORDCORE     preccExpanded;
} XFOBJWINDATA, *PXFOBJWINDATA;

#define WM_FILLCNR      (WM_USER+1)

/*
 *@@ AddObjectUsage2Cnr:
 *      shortcut for the "object usage" functions below
 *      to add one cnr record core.
 */

POBJECTUSAGERECORD AddObjectUsage2Cnr(HWND hwndCnr,     // in: container on "Object" page
                                      POBJECTUSAGERECORD preccParent, // in: parent record or NULL for root
                                      PSZ pszTitle,     // in: text to appear in cnr
                                      ULONG flAttrs)    // in: CRA_* flags for record
{
    POBJECTUSAGERECORD preccNew
        = (POBJECTUSAGERECORD)cnrhAllocRecords(hwndCnr, sizeof(OBJECTUSAGERECORD), 1);

    strhncpy0(preccNew->szText, pszTitle, sizeof(preccNew->szText));
    cnrhInsertRecords(hwndCnr,
                      (PRECORDCORE)preccParent,       // parent
                      (PRECORDCORE)preccNew,          // new record
                      TRUE, // invalidate
                      preccNew->szText, flAttrs, 1);
    return (preccNew);
}

#ifdef DEBUG_MEMORY

    LONG    lObjectCount,
            lTotalObjectSize,
            lFreedObjectCount,
            lHeapStatus;

    /*
     * fncbHeapWalk:
     *      callback func for _heap_walk function used for
     *      object usage (FillCnrWithObjectUsage)
     */

    int fncbHeapWalk(const void *pObject,
                     size_t Size,
                     int useflag,
                     int status,
                     const char *filename,
                     size_t line)
    {
        if (status != _HEAPOK) {
            lHeapStatus = status;
        }
        if (useflag == _USEDENTRY) {
            // object not freed
            lObjectCount++;
            lTotalObjectSize += Size;
        } else
            lFreedObjectCount++;

        return 0;
    }
#endif

/*
 *@@ AddFolderView2Cnr:
 *
 *@@added V0.9.1 (2000-01-17) [umoeller]
 */

VOID AddFolderView2Cnr(HWND hwndCnr,
                       POBJECTUSAGERECORD preccLevel2,
                       WPObject *pObject,
                       ULONG ulView,
                       PSZ pszView)
{
    PSZ pszTemp = 0;
    ULONG ulViewAttrs = _wpQueryFldrAttr(pObject, ulView);

    strhxcpy(&pszTemp, pszView);
    strhxcat(&pszTemp, ": ");

    if (ulViewAttrs & CV_ICON)
        strhxcat(&pszTemp, "CV_ICON ");
    if (ulViewAttrs & CV_NAME)
        strhxcat(&pszTemp, "CV_NAME ");
    if (ulViewAttrs & CV_TEXT)
        strhxcat(&pszTemp, "CV_TEXT ");
    if (ulViewAttrs & CV_TREE)
        strhxcat(&pszTemp, "CV_TREE ");
    if (ulViewAttrs & CV_DETAIL)
        strhxcat(&pszTemp, "CV_DETAIL ");
    if (ulViewAttrs & CA_DETAILSVIEWTITLES)
        strhxcat(&pszTemp, "CA_DETAILSVIEWTITLES ");

    if (ulViewAttrs & CV_MINI)
        strhxcat(&pszTemp, "CV_MINI ");
    if (ulViewAttrs & CV_FLOW)
        strhxcat(&pszTemp, "CV_FLOW ");
    if (ulViewAttrs & CA_DRAWICON)
        strhxcat(&pszTemp, "CA_DRAWICON ");
    if (ulViewAttrs & CA_DRAWBITMAP)
        strhxcat(&pszTemp, "CA_DRAWBITMAP ");
    if (ulViewAttrs & CA_TREELINE)
        strhxcat(&pszTemp, "CA_TREELINE ");

    // owner...

    AddObjectUsage2Cnr(hwndCnr,
                       preccLevel2,
                       pszTemp,
                       CRA_RECORDREADONLY);

    free(pszTemp);
}

/*
 *@@ FillCnrWithObjectUsage:
 *      adds all the object details into a given
 *      container window.
 *
 *@@changed V0.9.1 (2000-01-16) [umoeller]: added object setup string
 */

VOID FillCnrWithObjectUsage(HWND hwndCnr,       // in: cnr to insert into
                            WPObject *pObject)  // in: object for which to insert data
{
    POBJECTUSAGERECORD
                    preccRoot, preccLevel2, preccLevel3;
    CHAR            szTemp1[100], szText[500];
    PUSEITEM        pUseItem;

    CHAR            szObjectHandle[20];
    HOBJECT         hObject;
    PSZ             pszObjectID;
    ULONG           ul;

    // PKERNELGLOBALS   pKernelGlobals = krnQueryGlobals();

    // printf("Cnr: %d\n", hwndCnr);
    if (pObject)
    {
        sprintf(szText, "%s (Class: %s)",
                _wpQueryTitle(pObject),
                _somGetClassName(pObject));
        preccRoot = AddObjectUsage2Cnr(hwndCnr, NULL, szText,
                                       CRA_RECORDREADONLY | CRA_EXPANDED);

        // object ID
        preccLevel2 = AddObjectUsage2Cnr(hwndCnr, preccRoot,
                                         "Object ID",
                                         CRA_RECORDREADONLY | CRA_EXPANDED);
        pszObjectID = _wpQueryObjectID(pObject);
        if (pszObjectID)
            AddObjectUsage2Cnr(hwndCnr, preccLevel2,
                               pszObjectID,
                               (strcmp(pszObjectID, "<WP_DESKTOP") != 0
                                    ? 0 // editable!
                                    : CRA_RECORDREADONLY)); // for the Desktop
        else
            AddObjectUsage2Cnr(hwndCnr, preccLevel2,
                               "none set", 0); // editable!

        // object handle
        preccLevel2 = AddObjectUsage2Cnr(hwndCnr, preccRoot,
                                         "Object handle",
                                         CRA_RECORDREADONLY | CRA_EXPANDED);
        if (_somIsA(pObject, _WPFileSystem))
        {
            // for file system objects:
            // do not call wpQueryHandle, because
            // this always _creates_ a handle!
            // So instead, we search OS2SYS.INI directly.
            CHAR    szPath[CCHMAXPATH];
            _wpQueryFilename(pObject, szPath,
                             TRUE);      // fully qualified
            hObject = wphQueryHandleFromPath(HINI_USER, HINI_SYSTEM,
                                             szPath);
        }
        else // if (_somIsA(pObject, _WPAbstract))
            // not file system: that's safe
            hObject = _wpQueryHandle(pObject);

        if ((LONG)hObject > 0)
            sprintf(szObjectHandle, "0x%lX", hObject);
        else
            sprintf(szObjectHandle, "(none queried)");
        AddObjectUsage2Cnr(hwndCnr, preccLevel2,
                           szObjectHandle,
                           CRA_RECORDREADONLY);

        // object style
        preccLevel2 = AddObjectUsage2Cnr(hwndCnr, preccRoot,
                                         "Object style",
                                         CRA_RECORDREADONLY);
        ul = _wpQueryStyle(pObject);
        if (ul & OBJSTYLE_CUSTOMICON)
            AddObjectUsage2Cnr(hwndCnr, preccLevel2,
                               "Custom icon (destroy icon when object goes dormant)",
                               CRA_RECORDREADONLY);
        if (ul & OBJSTYLE_NOCOPY)
            AddObjectUsage2Cnr(hwndCnr, preccLevel2, "no copy",
                               CRA_RECORDREADONLY);
        if (ul & OBJSTYLE_NODELETE)
            AddObjectUsage2Cnr(hwndCnr, preccLevel2, "no delete",
                               CRA_RECORDREADONLY);
        if (ul & OBJSTYLE_NODRAG)
            AddObjectUsage2Cnr(hwndCnr, preccLevel2, "no drag",
                               CRA_RECORDREADONLY);
        if (ul & OBJSTYLE_NODROPON)
            AddObjectUsage2Cnr(hwndCnr, preccLevel2, "no drop-on",
                               CRA_RECORDREADONLY);
        if (ul & OBJSTYLE_NOLINK)
            AddObjectUsage2Cnr(hwndCnr, preccLevel2, "no link (cannot have shadows)",
                               CRA_RECORDREADONLY);
        if (ul & OBJSTYLE_NOMOVE)
            AddObjectUsage2Cnr(hwndCnr, preccLevel2, "no move",
                               CRA_RECORDREADONLY);
        if (ul & OBJSTYLE_NOPRINT)
            AddObjectUsage2Cnr(hwndCnr, preccLevel2, "no print",
                               CRA_RECORDREADONLY);
        if (ul & OBJSTYLE_NORENAME)
            AddObjectUsage2Cnr(hwndCnr, preccLevel2, "no rename",
                               CRA_RECORDREADONLY);
        if (ul & OBJSTYLE_NOSETTINGS)
            AddObjectUsage2Cnr(hwndCnr, preccLevel2, "no settings",
                               CRA_RECORDREADONLY);
        if (ul & OBJSTYLE_NOSETTINGS)
            AddObjectUsage2Cnr(hwndCnr, preccLevel2, "not visible",
                               CRA_RECORDREADONLY);
        if (ul & OBJSTYLE_TEMPLATE)
            AddObjectUsage2Cnr(hwndCnr, preccLevel2, "template",
                               CRA_RECORDREADONLY);
        /* if (ul & OBJSTYLE_LOCKEDINPLACE)
            AddObjectUsage2Cnr(hwndCnr, preccLevel2, "locked in place",
                               CRA_RECORDREADONLY); */

        // folder data
        if (_somIsA(pObject, _WPFolder))
        {
            preccLevel2 = AddObjectUsage2Cnr(hwndCnr, preccRoot, "Folder flags",
                                             CRA_RECORDREADONLY);
            ul = _wpQueryFldrFlags(pObject);
            if (ul & FOI_POPULATEDWITHALL)
                AddObjectUsage2Cnr(hwndCnr, preccLevel2, "Fully populated",
                                   CRA_RECORDREADONLY);
            if (ul & FOI_POPULATEDWITHFOLDERS)
                AddObjectUsage2Cnr(hwndCnr, preccLevel2, "Populated with folders",
                                   CRA_RECORDREADONLY);
            if (ul & FOI_FIRSTPOPULATE)
                AddObjectUsage2Cnr(hwndCnr, preccLevel2, "Populated with first objects",
                                   CRA_RECORDREADONLY);
            if (ul & FOI_WORKAREA)
                AddObjectUsage2Cnr(hwndCnr, preccLevel2, "Work area",
                                   CRA_RECORDREADONLY);
            if (ul & FOI_CHANGEFONT)
                AddObjectUsage2Cnr(hwndCnr, preccLevel2, "FOI_CHANGEFONT",
                                   CRA_RECORDREADONLY);
            if (ul & FOI_NOREFRESHVIEWS)
                AddObjectUsage2Cnr(hwndCnr, preccLevel2, "FOI_NOREFRESHVIEWS",
                                   CRA_RECORDREADONLY);
            if (ul & FOI_ASYNCREFRESHONOPEN)
                AddObjectUsage2Cnr(hwndCnr, preccLevel2, "FOI_ASYNCREFRESHONOPEN",
                                   CRA_RECORDREADONLY);
            if (ul & FOI_REFRESHINPROGRESS)
                AddObjectUsage2Cnr(hwndCnr, preccLevel2, "FOI_REFRESHINPROGRESS",
                                   CRA_RECORDREADONLY);
            if (ul & FOI_WAMCRINPROGRESS)
                AddObjectUsage2Cnr(hwndCnr, preccLevel2, "FOI_WAMCRINPROGRESS",
                                   CRA_RECORDREADONLY);
            if (ul & FOI_CNRBKGNDOLDFORMAT)
                AddObjectUsage2Cnr(hwndCnr, preccLevel2, "FOI_CNRBKGNDOLDFORMAT",
                                   CRA_RECORDREADONLY);
            if (ul & FOI_DELETEINPROGRESS)
                AddObjectUsage2Cnr(hwndCnr, preccLevel2, "FOI_DELETEINPROGRESS",
                                   CRA_RECORDREADONLY);

            // folder views:
            preccLevel2 = AddObjectUsage2Cnr(hwndCnr, preccRoot, "Folder view flags",
                                             CRA_RECORDREADONLY);
            AddFolderView2Cnr(hwndCnr, preccLevel2, pObject, OPEN_CONTENTS, "Icon view");
            AddFolderView2Cnr(hwndCnr, preccLevel2, pObject, OPEN_DETAILS, "Details view");
            AddFolderView2Cnr(hwndCnr, preccLevel2, pObject, OPEN_TREE, "Tree view");

            // Desktop: add WPS data
            // we use a conditional compile flag here because
            // _heap_walk adds additional overhead to malloc()
            #ifdef DEBUG_MEMORY
                if (pObject == _wpclsQueryActiveDesktop(_WPDesktop))
                {
                    /* PTIB             ptib;
                    PPIB             ppib; */
                    /* TID              tidWorkerThread = 0,
                                     tidQuickThread = 0; */

                    preccLevel2 = AddObjectUsage2Cnr(hwndCnr, preccRoot,
                                                     "Workplace Shell status",
                                                     CRA_RECORDREADONLY);

                    lObjectCount = 0;
                    lTotalObjectSize = 0;
                    lFreedObjectCount = 0;
                    lHeapStatus = _HEAPOK;

                    // get heap info using the callback above
                    _heap_walk(fncbHeapWalk);

                    strths(szTemp1, lTotalObjectSize, ',');
                    sprintf(szText, "XFolder memory consumption: %s bytes\n"
                            "(%d objects used, %d objects freed)",
                            szTemp1,
                            lObjectCount,
                            lFreedObjectCount);
                    AddObjectUsage2Cnr(hwndCnr, preccLevel2, szText,
                                       CRA_RECORDREADONLY);

                    sprintf(szText, "XFolder memory heap status: %s",
                            (lHeapStatus == _HEAPOK) ? "OK"
                            : (lHeapStatus == _HEAPBADBEGIN) ? "Invalid heap (_HEAPBADBEGIN)"
                            : (lHeapStatus == _HEAPBADNODE) ? "Damaged memory node"
                            : (lHeapStatus == _HEAPEMPTY) ? "Heap not initialized"
                            : "unknown error"
                            );
                    AddObjectUsage2Cnr(hwndCnr, preccLevel2, szText,
                                       CRA_RECORDREADONLY);
                }
            #endif // DEBUG_MEMORY
        } // end WPFolder

        // object usage:
        preccLevel2 = AddObjectUsage2Cnr(hwndCnr, preccRoot, "Object usage",
                            CRA_RECORDREADONLY);

        // 1) open views
        preccLevel3 = NULL;
        for (pUseItem = _wpFindUseItem(pObject, USAGE_OPENVIEW, NULL);
             pUseItem;
             pUseItem = _wpFindUseItem(pObject, USAGE_OPENVIEW, pUseItem))
        {
            PVIEWITEM   pViewItem = (PVIEWITEM)(pUseItem+1);
            ULONG       ulSLIIndex = 0;
            switch (pViewItem->view)
            {
                case OPEN_SETTINGS: strcpy(szTemp1, "Settings"); break;
                case OPEN_CONTENTS: strcpy(szTemp1, "Icon"); break;
                case OPEN_DETAILS:  strcpy(szTemp1, "Details"); break;
                case OPEN_TREE:     strcpy(szTemp1, "Tree"); break;
                case OPEN_RUNNING:  strcpy(szTemp1, "Program running"); break;
                case OPEN_PROMPTDLG:strcpy(szTemp1, "Prompt dialog"); break;
                case OPEN_PALETTE:  strcpy(szTemp1, "Palette"); break;
                default:            sprintf(szTemp1, "unknown (0x%lX)", pViewItem->view); break;
            }
            if (pViewItem->view != OPEN_RUNNING)
            {
                PID pid;
                TID tid;
                WinQueryWindowProcess(pViewItem->handle, &pid, &tid);
                sprintf(szText, "%s (HWND: 0x%lX, thread ID: 0x%lX)",
                        szTemp1, pViewItem->handle, tid);
            }
            else
            {
                sprintf(szText, "%s (HWND: 0x%lX)",
                        szTemp1, pViewItem->handle);
            }
            if (fdrQueryPSLI(pViewItem->handle, &ulSLIIndex))
                sprintf(szText + strlen(szText), "\nSubclassed: index %d in list", ulSLIIndex);
            else
                sprintf(szText + strlen(szText), "\nNot subclassed");
            if (!preccLevel3)
                preccLevel3 = AddObjectUsage2Cnr(hwndCnr, preccLevel2,
                                                 "Currently open views",
                                                 CRA_RECORDREADONLY | CRA_EXPANDED);
            AddObjectUsage2Cnr(hwndCnr, preccLevel3, szText, CRA_RECORDREADONLY);
        }

        // 2) allocated memory
        preccLevel3 = NULL;
        for (pUseItem = _wpFindUseItem(pObject, USAGE_MEMORY, NULL);
             pUseItem;
             pUseItem = _wpFindUseItem(pObject, USAGE_MEMORY, pUseItem))
        {
            PMEMORYITEM pMemoryItem = (PMEMORYITEM)(pUseItem+1);
            sprintf(szText, "Size: %d", pMemoryItem->cbBuffer);
            if (!preccLevel3)
                preccLevel3 = AddObjectUsage2Cnr(hwndCnr, preccLevel2,
                                                 "Allocated memory",
                                                 CRA_RECORDREADONLY);
            AddObjectUsage2Cnr(hwndCnr, preccLevel3, szText, CRA_RECORDREADONLY);
        }

        // 3) awake shadows
        preccLevel3 = NULL;
        for (pUseItem = _wpFindUseItem(pObject, USAGE_LINK, NULL);
             pUseItem;
             pUseItem = _wpFindUseItem(pObject, USAGE_LINK, pUseItem))
        {
            PLINKITEM pLinkItem = (PLINKITEM)(pUseItem+1);
            CHAR      szShadowPath[CCHMAXPATH];
            if (pLinkItem->LinkObj)
            {
                _wpQueryFilename(_wpQueryFolder(pLinkItem->LinkObj),
                                 szShadowPath,
                                 TRUE);     // fully qualified
                sprintf(szText, "%s in \n%s",
                        _wpQueryTitle(pLinkItem->LinkObj),
                        szShadowPath);
            }
            else
                // error: shouldn't happen, because pObject
                // itself is obviously valid
                strcpy(szText, "broken");

            if (!preccLevel3)
                preccLevel3 = AddObjectUsage2Cnr(hwndCnr, preccLevel2,
                                                 "Awake shadows of this object",
                                                 CRA_RECORDREADONLY | CRA_EXPANDED);
            AddObjectUsage2Cnr(hwndCnr, preccLevel3,
                               szText,
                               CRA_RECORDREADONLY);
        }

        // 4) containers into which object has been inserted
        preccLevel3 = NULL;
        for (pUseItem = _wpFindUseItem(pObject, USAGE_RECORD, NULL);
            pUseItem;
            pUseItem = _wpFindUseItem(pObject, USAGE_RECORD, pUseItem))
        {
            PRECORDITEM pRecordItem = (PRECORDITEM)(pUseItem+1);
            CHAR szFolderTitle[256];
            WinQueryWindowText(WinQueryWindow(pRecordItem->hwndCnr, QW_PARENT),
                               sizeof(szFolderTitle)-1,
                               szFolderTitle);
            sprintf(szText, "Container HWND: 0x%lX\n(\"%s\")",
                    pRecordItem->hwndCnr,
                    szFolderTitle);
            if (!preccLevel3)
                preccLevel3 = AddObjectUsage2Cnr(hwndCnr, preccLevel2,
                                                 "Folder windows containing this object",
                                                 CRA_RECORDREADONLY | CRA_EXPANDED);
            AddObjectUsage2Cnr(hwndCnr, preccLevel3, szText, CRA_RECORDREADONLY);
        }

        // 5) applications
        preccLevel3 = NULL;
        for (pUseItem = _wpFindUseItem(pObject, USAGE_OPENFILE, NULL);
            pUseItem;
            pUseItem = _wpFindUseItem(pObject, USAGE_OPENFILE, pUseItem))
        {
            PVIEWFILE pViewFile = (PVIEWFILE)(pUseItem+1);
            sprintf(szText,
                    "Open handle: 0x%lX",
                    pViewFile->handle); // this might be a HAPP;
                                        // test undocumented WinHSWitchFromHApp on this!
            if (!preccLevel3)
                preccLevel3 = AddObjectUsage2Cnr(hwndCnr, preccLevel2,
                                                 "Applications which opened this object",
                                                 CRA_RECORDREADONLY | CRA_EXPANDED);
            AddObjectUsage2Cnr(hwndCnr, preccLevel3,
                               szText,  // open handle
                               CRA_RECORDREADONLY);
        }

        preccLevel3 = NULL;
        for (ul = 0; ul < 100; ul++)
            if (    (ul != USAGE_OPENVIEW)
                 && (ul != USAGE_MEMORY)
                 && (ul != USAGE_LINK)
                 && (ul != USAGE_RECORD)
                 && (ul != USAGE_OPENFILE)
               )
            {
                for (pUseItem = _wpFindUseItem(pObject, ul, NULL);
                    pUseItem;
                    pUseItem = _wpFindUseItem(pObject, ul, pUseItem))
                {
                    sprintf(szText, "Type: 0x%lX", pUseItem->type);
                    if (!preccLevel3)
                        preccLevel3 = AddObjectUsage2Cnr(hwndCnr, preccLevel2,
                                                         "Undocumented usage types",
                                                         CRA_RECORDREADONLY | CRA_EXPANDED);
                    AddObjectUsage2Cnr(hwndCnr, preccLevel3,
                                       szText, // "undocumented:"
                                       CRA_RECORDREADONLY);
                }
            }
    } // end if (pObject)
}

/*
 *@@ ReportSetupString:
 *
 *@@added V0.9.1 (2000-01-17) [umoeller]
 */

VOID ReportSetupString(WPObject *somSelf, HWND hwndDlg)
{
    HWND hwndEF = WinWindowFromID(hwndDlg, ID_XSDI_DTL_SETUP_ENTRY);
    ULONG cbSetupString = _xwpQuerySetup(somSelf,
                                         NULL,
                                         0);

    WinSetWindowText(hwndEF, "");

    if (cbSetupString)
    {
        PSZ pszSetupString = malloc(cbSetupString + 1);
        if (pszSetupString)
        {
            if (_xwpQuerySetup(somSelf,
                               pszSetupString,
                               cbSetupString + 1))
            {
                winhSetEntryFieldLimit(hwndEF, cbSetupString + 1);
                WinSetWindowText(hwndEF, pszSetupString);
            }
            free(pszSetupString);
        }
    }
}

/*
 * obj_fnwpSettingsObjDetails:
 *      notebook dlg func for XFldObject "Details" page.
 *      Here's a trick how to interface the corresponding
 *      SOM (WPS) object from within this procedure:
 *      1)  declare a structure with all the data which is
 *          needed in this wnd proc (see XFOBJWINDATA below)
 *      2)  when inserting the notebook page, specify
 *          somSelf as the pCreateParams parameter in the
 *          PAGEINFO structure;
 *      3)  somSelf is then passed in mp2 of WM_INITDLG;
 *          with that message, we create our structure on
 *          the heap and store the address to it in the
 *          notebook's window words. We can then access
 *          our structure from later messages too.
 *      4)  Don't forget to free the structure at WM_DESTROY.
 *
 *@@changed 0.85 [umoeller]: fixed object ID error message
 *@@changed V0.9.0 [umoeller]: fixed memory leak (thanks Lars Erdmann)
 *@@changed V0.9.0 [umoeller]: added hotkey support
 */

MRESULT EXPENTRY obj_fnwpSettingsObjDetails(HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    PXFOBJWINDATA   pWinData = (PXFOBJWINDATA)WinQueryWindowULong(hwndDlg, QWL_USER);
    MRESULT         mrc = FALSE;

    switch(msg)
    {
        case WM_INITDLG:
        {
            // we need to initialize SOM stuff to be able to
            // access instance variables later
            PSZ     pszObjectID;
            CNRINFO CnrInfo;
            HWND    hwndHotkeyEntryField = WinWindowFromID(hwndDlg,
                                                           ID_XSDI_DTL_HOTKEY);

            pWinData = (PXFOBJWINDATA)_wpAllocMem((SOMAny*)mp2, sizeof(XFOBJWINDATA), NULL);
            WinSetWindowPtr(hwndDlg, QWL_USER, pWinData);

            // somSelf is given to us in mp2 (see pCreateParams
            // in XFldObject::xwpAddObjectInternalsPage below)
            pWinData->somSelf     = (SOMAny*)mp2;
            // initialize the fields in the structure
            pszObjectID = _wpQueryObjectID(pWinData->somSelf);
            if (pszObjectID)
                strcpy(pWinData->szOldID, pszObjectID);
            else
                strcpy(pWinData->szOldID, "");

            // subclass entry field for hotkeys
            ctlMakeHotkeyEntryField(hwndHotkeyEntryField);

            // disable entry field if hotkeys are not working
            {
                BOOL f = hifXWPHookReady();
                WinEnableWindow(hwndHotkeyEntryField, f);
                WinEnableControl(hwndDlg, ID_XSDI_DTL_HOTKEY_TXT, f);
            }

            // make Warp 4 notebook buttons and move controls
            winhAssertWarp4Notebook(hwndDlg,
                                    100,         // ID threshold
                                    WARP4_NOTEBOOK_OFFSET);
                                        // move other controls offset (common.h)

            // setup container
            pWinData->hwndCnr = WinWindowFromID(hwndDlg, ID_XSDI_DTL_CNR);
            WinSendMsg(pWinData->hwndCnr, CM_QUERYCNRINFO,
                        &CnrInfo, (MPARAM)sizeof(CnrInfo));
            CnrInfo.pSortRecord = (PVOID)fnCompareName;
            CnrInfo.flWindowAttr = CV_TREE | CV_TEXT | CA_TREELINE;
            CnrInfo.cxTreeIndent = 30;
            WinSendMsg(pWinData->hwndCnr, CM_SETCNRINFO,
                    &CnrInfo,
                    (MPARAM)(CMA_PSORTRECORD | CMA_FLWINDOWATTR | CMA_CXTREEINDENT));

            WinPostMsg(hwndDlg, XM_SETTINGS2DLG, 0, 0);

            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
        break; }

        /*
         * XM_SETTINGS2DLG:
         *      this user msg (common.h) gets posted when
         *      the dialog controls need to be set according
         *      to the current settings.
         */

        case XM_SETTINGS2DLG:
        {
            HPOINTER hptrOld = winhSetWaitPointer();

            USHORT      usFlags,
                        usKeyCode;
            UCHAR       ucScanCode;
            HWND        hwndEntryField = WinWindowFromID(hwndDlg,
                                                         ID_XSDI_DTL_HOTKEY);
            if (_xwpQueryObjectHotkey(pWinData->somSelf,
                                      &usFlags,
                                      &ucScanCode,
                                      &usKeyCode))
            {
                CHAR    szKeyName[200];
                cmnDescribeKey(szKeyName, usFlags, usKeyCode);
                WinSetWindowText(hwndEntryField, szKeyName);
            }
            else
                WinSetWindowText(hwndEntryField, (cmnQueryNLSStrings())->pszNotDefined);

            // object setup string
            ReportSetupString(pWinData->somSelf, hwndDlg);

            FillCnrWithObjectUsage(pWinData->hwndCnr, pWinData->somSelf);

            WinSetPointer(HWND_DESKTOP, hptrOld);
        } break;

        /*
         * WM_TIMER:
         *      timer for tree view auto-scroll
         */

        case WM_TIMER:
        {
            if (pWinData->preccExpanded->flRecordAttr & CRA_EXPANDED)
            {
                PRECORDCORE     preccLastChild;
                WinStopTimer(WinQueryAnchorBlock(hwndDlg),
                        hwndDlg,
                        1);
                // scroll the tree view properly
                preccLastChild = WinSendMsg(pWinData->hwndCnr,
                                            CM_QUERYRECORD,
                                            pWinData->preccExpanded,
                                               // expanded PRECORDCORE from CN_EXPANDTREE
                                            MPFROM2SHORT(CMA_LASTCHILD,
                                                         CMA_ITEMORDER));
                if ((preccLastChild) && (preccLastChild != (PRECORDCORE)-1))
                {
                    // ULONG ulrc;
                    cnrhScrollToRecord(pWinData->hwndCnr,
                                       (PRECORDCORE)preccLastChild,
                                       CMA_TEXT,   // record text rectangle only
                                       TRUE);      // keep parent visible
                }
            }
        } break;

        /*
         * WM_CONTROL:
         *
         */

        case WM_CONTROL:
        {
            USHORT usID = SHORT1FROMMP(mp1),
                   usNotifyCode = SHORT2FROMMP(mp1);

            switch (usID)
            {
                /*
                 * ID_XSDI_DTL_CNR:
                 *      "Internals" container
                 */

                case ID_XSDI_DTL_CNR:
                    switch (usNotifyCode)
                    {
                        /*
                         * CN_EXPANDTREE:
                         *      do tree-view auto scroll
                         */

                        case CN_EXPANDTREE:
                        {
                            PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
                            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
                            if (pGlobalSettings->TreeViewAutoScroll) {
                                pWinData->preccExpanded = (PRECORDCORE)mp2;
                                WinStartTimer(WinQueryAnchorBlock(hwndDlg),
                                        hwndDlg,
                                        1,
                                        100);
                            }
                        break; }

                        /*
                         * CN_BEGINEDIT:
                         *      when user alt-clicked on a recc
                         */

                        case CN_BEGINEDIT:
                            pWinData->fEscPressed = TRUE;
                            mrc = (MPARAM)0;
                        break;

                        /*
                         * CN_REALLOCPSZ:
                         *      just before the edit MLE is closed
                         */

                        case CN_REALLOCPSZ:
                        {
                            PCNREDITDATA pced = (PCNREDITDATA)mp2;
                            PSZ pszChanging = *(pced->ppszText);
                            strcpy(pWinData->szOldObjectID, pszChanging);
                            pWinData->fEscPressed = FALSE;
                            mrc = (MPARAM)TRUE;
                        break; }

                        /*
                         * CN_ENDEDIT:
                         *      recc text changed: update our data
                         */

                        case CN_ENDEDIT:
                            if (!pWinData->fEscPressed)
                            {
                                PCNREDITDATA pced = (PCNREDITDATA)mp2;
                                PSZ pszNew = *(pced->ppszText);
                                BOOL fChange = FALSE;
                                // has the object ID changed?
                                if (strcmp(pWinData->szOldObjectID, pszNew) != 0)
                                {
                                    // is this a valid object ID?
                                    if (    (pszNew[0] != '<')
                                         || (*(pszNew + strlen(pszNew)-1) != '>')
                                       )
                                        cmnMessageBoxMsg(hwndDlg, 104, 108, MB_OK);
                                            // fixed (V0.85)
                                    else
                                        // valid: confirm change
                                        if (cmnMessageBoxMsg(hwndDlg, 107, 109, MB_YESNO) == MBID_YES)
                                            fChange = TRUE;

                                    if (fChange)
                                        _wpSetObjectID(pWinData->somSelf, pszNew);
                                    else
                                    {
                                        // change aborted: restore old recc text
                                        strcpy(((POBJECTUSAGERECORD)(pced->pRecord))->szText,
                                                pWinData->szOldObjectID);
                                        WinSendMsg(pWinData->hwndCnr,
                                                   CM_INVALIDATERECORD,
                                                   (MPARAM)pced->pRecord,
                                                   MPFROM2SHORT(1,
                                                                CMA_TEXTCHANGED));
                                    }
                                }
                            }
                            mrc = (MPARAM)0;
                        break;

                        default:
                            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
                    } // end switch
                break; // ID_XSDI_DTL_CNR

                /*
                 * ID_XSDI_DTL_HOTKEY:
                 *      "hotkey" entry field
                 */

                case ID_XSDI_DTL_HOTKEY:
                    if (usNotifyCode == EN_HOTKEY)
                    {
                        PHOTKEYNOTIFY phkn = (PHOTKEYNOTIFY)mp2;
                        // now check if this thing makes up a valid
                        // hotkey just yet: if KC_VIRTUALKEY is down,
                        // we must filter out the sole CTRL, ALT, and
                        // SHIFT keys, because these are valid only
                        // when pressed with some other key. By doing
                        // this, we do get a display in the entry field
                        // if only, say, Ctrl is down, but we won't
                        // store this.
                        if  (  (  ((phkn->usFlags & KC_VIRTUALKEY) != 0)
                                  // Ctrl pressed?
                               || ((phkn->usFlags & KC_CTRL) != 0)
                                  // Alt pressed?
                               || ((phkn->usFlags & KC_ALT) != 0)
                                  // or one of the Win95 keys?
                               || (   ((phkn->usFlags & KC_VIRTUALKEY) == 0)
                                   && (     (phkn->usch == 0xEC00)
                                        ||  (phkn->usch == 0xED00)
                                        ||  (phkn->usch == 0xEE00)
                                      )
                               )
                            )
                            && (    ((phkn->usFlags & KC_VIRTUALKEY) == 0)
                                 || (   (phkn->usKeyCode != 0x09)     // shift
                                     && (phkn->usKeyCode != 0x0a)     // ctrl
                                     && (phkn->usKeyCode != 0x0b)     // alt
                                    )
                               )
                            )
                        {
                            // store hotkey for object;
                            // we'll now pass the scan code, which is
                            // used by the hook
                            objSetObjectHotkey(pWinData->somSelf,
                                               phkn->usFlags,
                                               phkn->ucScanCode,
                                               phkn->usKeyCode);
                            // show description
                            cmnDescribeKey(phkn->szDescription,
                                           phkn->usFlags,
                                           phkn->usKeyCode);
                            // have entry field display that (comctl.c)
                            mrc = (MPARAM)TRUE;
                        }
                        else
                        {
                            // invalid:
                            objSetObjectHotkey(pWinData->somSelf,
                                               0, 0, 0);
                            mrc = (MPARAM)FALSE;
                        }
                    }
                break;

                default:
                    mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
            } // end switch
        break; } // end of WM_CONTROL

        case WM_COMMAND:
            switch (SHORT1FROMMP(mp1))
            {
                /*
                 * DID_REFRESH:
                 *      "Refresh" button
                 */

                case DID_REFRESH:
                {
                    HPOINTER hptrOld = winhSetWaitPointer();
                    WinSendMsg(pWinData->hwndCnr,
                               CM_REMOVERECORD,
                               NULL,
                               MPFROM2SHORT(0, // remove all reccs
                                            CMA_FREE));
                    FillCnrWithObjectUsage(pWinData->hwndCnr, pWinData->somSelf);

                    ReportSetupString(pWinData->somSelf, hwndDlg);

                    WinSetPointer(HWND_DESKTOP, hptrOld);
                break; }

                /*
                 * ID_XSDI_DTL_CLEAR:
                 *      "Clear hotkey" button
                 */

                case ID_XSDI_DTL_CLEAR:
                    // remove hotkey
                    _xwpSetObjectHotkey(pWinData->somSelf,
                                       0, 0, 0);   // remove flags
                    WinSetWindowText(WinWindowFromID(hwndDlg,
                                                     ID_XSDI_DTL_HOTKEY),
                                     (cmnQueryNLSStrings())->pszNotDefined);
                break;
            }
        break; // end of WM_COMMAND

        case WM_HELP:
            // always display help for the whole page, not for single items
            cmnDisplayHelp(pWinData->somSelf,
                           ID_XSH_SETTINGS_OBJINTERNALS);
        break;

        case WM_DESTROY:
        {
            // clean up the data we allocated earlier (V1.00)
            _wpFreeMem(pWinData->somSelf, (PBYTE)pWinData);
            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
        break; }

        default:
            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
    }

    return (mrc);
}

/* ******************************************************************
 *                                                                  *
 *   Object hotkeys                                                 *
 *                                                                  *
 ********************************************************************/

/*
 *@@ objFindHotkey:
 *      this searches an array of GLOBALHOTKEY structures
 *      for the given object handle (as returned by
 *      hifQueryObjectHotkeys) and returns a pointer
 *      to the array item or NULL if not found.
 *
 *      Used by objQueryObjectHotkey and objSetObjectHotkey.
 */

PGLOBALHOTKEY objFindHotkey(PGLOBALHOTKEY pHotkeys, // in: array returned by hifQueryObjectHotkeys
                            ULONG cHotkeys,        // in: array item count (_not_ array size!)
                            HOBJECT hobj)
{
    PGLOBALHOTKEY   prc = NULL;
    ULONG   ul = 0;
    // go thru array of hotkeys and find matching item
    PGLOBALHOTKEY pHotkeyThis = pHotkeys;
    for (ul = 0;
         ul < cHotkeys;
         ul++)
    {
        if (pHotkeyThis->ulHandle == hobj)
        {
            prc = pHotkeyThis;
            break;
        }
        pHotkeyThis++;
    }

    return (prc);
}

/*
 *@@ objQueryObjectHotkey:
 *      implementation for XFldObject::xwpQueryObjectHotkey.
 */

BOOL objQueryObjectHotkey(WPObject *somSelf,
                          PUSHORT pusFlags,
                          PUCHAR  pucScanCode,
                          PUSHORT pusKeyCode)
{
    PGLOBALHOTKEY   pHotkeys;
    ULONG           cHotkeys = 0;
    BOOL            brc = FALSE;

    // XFldObjectData *somThis = XFldObjectGetData(somSelf);

    pHotkeys = hifQueryObjectHotkeys(&cHotkeys);
    if (pHotkeys)
    {
        PGLOBALHOTKEY pHotkeyThis = objFindHotkey(pHotkeys,
                                                  cHotkeys,
                                                  _wpQueryHandle(somSelf));
        if (pHotkeyThis)
        {
            // found:
            *pusFlags = pHotkeyThis->usFlags;
            *pucScanCode = pHotkeyThis->ucScanCode;
            *pusKeyCode = pHotkeyThis->usKeyCode;
            brc = TRUE;     // success
        }

        hifFreeObjectHotkeys(pHotkeys);
    }

    return (brc);
}

/*
 *@@ objSetObjectHotkey:
 *      implementation for XFldObject::xwpSetObjectHotkey.
 */

BOOL objSetObjectHotkey(WPObject *somSelf,
                        USHORT usFlags,
                        UCHAR ucScanCode,
                        USHORT usKeyCode)
{
    PGLOBALHOTKEY   pHotkeys;
    ULONG           cHotkeys = 0;
    BOOL            brc = FALSE;
    HOBJECT         hobjSelf = _wpQueryHandle(somSelf);

    // XFldObjectData *somThis = XFldObjectGetData(somSelf);

    TRY_LOUD(excpt1, NULL)
    {
        #ifdef DEBUG_KEYS
            _Pmpf(("Entering xwpSetObjectHotkey usFlags = 0x%lX, usKeyCode = 0x%lX",
                    usFlags, usKeyCode));
        #endif

        pHotkeys = hifQueryObjectHotkeys(&cHotkeys);

        #ifdef DEBUG_KEYS
            _Pmpf(("  hifQueryObjectHotkeys returned 0x%lX, %d items",
                    pHotkeys, cHotkeys));
        #endif

        if (pHotkeys)
        {
            // hotkeys list exists:
            PGLOBALHOTKEY pHotkeyThis = objFindHotkey(pHotkeys,
                                                      cHotkeys,
                                                      _wpQueryHandle(somSelf));

            if ((usFlags == 0) && (usKeyCode == 0))
            {
                // "delete hotkey" mode:
                if (pHotkeyThis)
                {
                    // found (already exists): delete
                    // by copying the following item(s)
                    // in the array over the current one
                    ULONG   ulpofs = 0,
                            uliofs = 0;
                    ulpofs = ((PBYTE)pHotkeyThis - (PBYTE)pHotkeys);

                    #ifdef DEBUG_KEYS
                        _Pmpf(("  pHotkeyThis - pHotkeys: 0x%lX", ulpofs));
                    #endif

                    uliofs = (ulpofs / sizeof(GLOBALHOTKEY));
                                // 0 for first, 1 for second, ...

                    #ifdef DEBUG_KEYS
                        _Pmpf(("  Deleting existing hotkey @ ofs %d", uliofs));
                    #endif

                    if (uliofs < (cHotkeys - 1))
                    {
                        ULONG cb = (cHotkeys - uliofs - 1) * sizeof(GLOBALHOTKEY);

                        #ifdef DEBUG_KEYS
                            _Pmpf(("  Copying 0x%lX to 0x%lX, %d bytes (%d per item)",
                                    pHotkeyThis + 1, pHotkeyThis,
                                    cb, sizeof(GLOBALHOTKEY)));
                        #endif

                        // not last item:
                        memcpy(pHotkeyThis,
                               pHotkeyThis + 1,
                               cb);
                    }

                    brc = hifSetObjectHotkeys(pHotkeys, cHotkeys-1);
                }
                // else: does not exist, so it can't be deleted either
            }
            else
            {
                // "set hotkey" mode:
                if (pHotkeyThis)
                {
                    // found (already exists): overwrite
                    #ifdef DEBUG_KEYS
                        _Pmpf(("  Overwriting existing hotkey"));
                    #endif

                    if (    (pHotkeyThis->usFlags != usFlags)
                         || (pHotkeyThis->usKeyCode != usKeyCode)
                       )
                    {
                        pHotkeyThis->usFlags = usFlags;
                        pHotkeyThis->ucScanCode = ucScanCode;
                        pHotkeyThis->usKeyCode = usKeyCode;
                        pHotkeyThis->ulHandle = hobjSelf;
                        // set new objects list, which is the modified old list
                        brc = hifSetObjectHotkeys(pHotkeys, cHotkeys);
                    }
                }
                else
                {
                    // not found: append new item after copying
                    // the entire list
                    PGLOBALHOTKEY pHotkeysNew = (PGLOBALHOTKEY)malloc(sizeof(GLOBALHOTKEY)
                                                                        * (cHotkeys+1));
                    #ifdef DEBUG_KEYS
                        _Pmpf(("  Appending new hotkey"));
                    #endif

                    if (pHotkeysNew)
                    {
                        PGLOBALHOTKEY pNewItem = pHotkeysNew + cHotkeys;
                        // copy old array
                        memcpy(pHotkeysNew, pHotkeys, sizeof(GLOBALHOTKEY) * cHotkeys);
                        // append new item
                        pNewItem->usFlags = usFlags;
                        pNewItem->ucScanCode = ucScanCode;
                        pNewItem->usKeyCode = usKeyCode;
                        pNewItem->ulHandle = hobjSelf;
                        brc = hifSetObjectHotkeys(pHotkeysNew, cHotkeys+1);
                        free(pHotkeysNew);
                    }
                }
            }

            hifFreeObjectHotkeys(pHotkeys);
        }
        else
        {
            // hotkey list doesn't exist yet:
            if ((usFlags != 0) && (usKeyCode != 0))
            {
                // "set hotkey" mode:
                GLOBALHOTKEY HotkeyNew;

                #ifdef DEBUG_KEYS
                    _Pmpf(("  Creating single new hotkey"));
                #endif

                HotkeyNew.usFlags = usFlags;
                HotkeyNew.usKeyCode = usKeyCode;
                HotkeyNew.ulHandle = hobjSelf;
                brc = hifSetObjectHotkeys(&HotkeyNew,
                                          1);     // one item only
            }
            // else "delete hotkey" mode: do nothing
        }
    }
    CATCH(excpt1)
    {
    } END_CATCH();

    if (brc)
        // updated: update the "Hotkeys" settings page
        // in XWPKeyboard, if it's open
        ntbUpdateVisiblePage(NULL,      // any somSelf
                             SP_KEYB_OBJHOTKEYS);

    #ifdef DEBUG_KEYS
        _Pmpf(("Leaving xwpSetObjectHotkey"));
    #endif

    return (brc);
}

/*
 *@@ objRemoveObjectHotkey:
 *      implementation for M_XFldObject::xwpclsRemoveObjectHotkey.
 *
 *@@added V0.9.0 (99-11-12) [umoeller]
 */

BOOL objRemoveObjectHotkey(HOBJECT hobj)
{
    PGLOBALHOTKEY   pHotkeys;
    ULONG           cHotkeys = 0;
    BOOL            brc = FALSE;

    pHotkeys = hifQueryObjectHotkeys(&cHotkeys);

    if (pHotkeys)
    {
        // hotkeys list exists:
        PGLOBALHOTKEY pHotkeyThis = objFindHotkey(pHotkeys,
                                                  cHotkeys,
                                                  hobj);

        if (pHotkeyThis)
        {
            // found (already exists): delete
            // by copying the following item(s)
            // in the array over the current one
            ULONG   ulpofs = 0,
                    uliofs = 0;
            ulpofs = ((PBYTE)pHotkeyThis - (PBYTE)pHotkeys);

            uliofs = (ulpofs / sizeof(GLOBALHOTKEY));
                        // 0 for first, 1 for second, ...
            if (uliofs < (cHotkeys - 1))
            {
                ULONG cb = (cHotkeys - uliofs - 1) * sizeof(GLOBALHOTKEY);

                #ifdef DEBUG_KEYS
                    _Pmpf(("  Copying 0x%lX to 0x%lX, %d bytes (%d per item)",
                            pHotkeyThis + 1, pHotkeyThis,
                            cb, sizeof(GLOBALHOTKEY)));
                #endif

                // not last item:
                memcpy(pHotkeyThis,
                       pHotkeyThis + 1,
                       cb);
            }

            brc = hifSetObjectHotkeys(pHotkeys, cHotkeys-1);
        }
        // else: does not exist, so it can't be deleted either

        hifFreeObjectHotkeys(pHotkeys);
    }

    if (brc)
        // updated: update the "Hotkeys" settings page
        // in XWPKeyboard, if it's open
        ntbUpdateVisiblePage(NULL,      // any somSelf
                             SP_KEYB_OBJHOTKEYS);

    return (brc);
}

/*
 *@@ objQuerySetup:
 *      implementation of XFldObject::xwpQuerySetup.
 *      See remarks there.
 *
 *      This returns the length of the XFldObject
 *      setup string part only.
 *
 *@@added V0.9.1 (2000-01-16) [umoeller]
 */

ULONG objQuerySetup(WPObject *somSelf,
                    PSZ pszSetupString,     // in: buffer for setup string or NULL
                    ULONG cbSetupString)    // in: size of that buffer or 0
{
    // temporary buffer for building the setup string
    PSZ     pszTemp = NULL;
    ULONG   ulReturn = 0;
    ULONG   ulValue = 0,
            // ulDefaultValue = 0,
            ulStyle = 0;
    PSZ     pszValue = 0;

    XFldObjectData *somThis = XFldObjectGetData(somSelf);

    /* _Pmpf(("Dumping WPOBJECTDATA (%d bytes", _cbWPObjectData));
    cmnDumpMemoryBlock((PVOID)_pWPObjectData, _cbWPObjectData, 8); */

    // CCVIEW
    ulValue = _wpQueryConcurrentView(somSelf);
    switch (ulValue)
    {
        case CCVIEW_ON:
            strhxcat(&pszTemp, "CCVIEW=YES;");
        break;

        case CCVIEW_OFF:
            strhxcat(&pszTemp, "CCVIEW=NO;");
        break;
        // ignore CCVIEW_DEFAULT
    }

    // DEFAULTVIEW
    if (_pWPObjectData)
        if (    (_pWPObjectData->lDefaultView != 0x67)      // default view for folders
             && (_pWPObjectData->lDefaultView != 0x1000)    // default view for data files
             && (_pWPObjectData->lDefaultView != -1)        // OPEN_DEFAULT
           )
        {
            switch (_pWPObjectData->lDefaultView)
            {
                case OPEN_SETTINGS:
                    strhxcat(&pszTemp, "DEFAULTVIEW=SETTINGS;");
                break;

                case OPEN_CONTENTS:
                    strhxcat(&pszTemp, "DEFAULTVIEW=ICON;");
                break;

                case OPEN_TREE:
                    strhxcat(&pszTemp, "DEFAULTVIEW=TREE;");
                break;

                case OPEN_DETAILS:
                    strhxcat(&pszTemp, "DEFAULTVIEW=DETAILS;");
                break;

                case OPEN_DEFAULT:
                    // ignore
                break;

                default:
                {
                    // any other: that's user defined, add decimal ID
                    CHAR szTemp[30];
                    sprintf(szTemp, "DEFAULTVIEW=%d;", _pWPObjectData->lDefaultView);
                    strhxcat(&pszTemp, szTemp);
                break; }
            }
        }

    // HELPLIBRARY

    // HELPPANEL
    if (_pWPObjectData)
        if (_pWPObjectData->ulHelpPanel)
        {
            CHAR szTemp[40];
            sprintf(szTemp, "HELPPANEL=%d;", _pWPObjectData->ulHelpPanel);
            strhxcat(&pszTemp, szTemp);
        }

    // HIDEBUTTON
    ulValue = _wpQueryButtonAppearance(somSelf);
    switch (ulValue)
    {
        case HIDEBUTTON:
            strhxcat(&pszTemp, "HIDEBUTTON=YES;");
        break;

        case MINBUTTON:
            strhxcat(&pszTemp, "HIDEBUTTON=NO;");
        break;

        // ignore DEFAULTBUTTON
    }

    // ICONFILE: cannot be queried!
    // ICONRESOURCE: cannot be queried!

    // ICONPOS: x, y in percentage of folder coordinates

    // LOCKEDINPLACE: Warp 4 only

    // MENUS: Warp 4 only

    // MINWIN
    ulValue = _wpQueryMinWindow(somSelf);
    switch (ulValue)
    {
        case MINWIN_HIDDEN:
            strhxcat(&pszTemp, "MINWIN=HIDE;");
        break;

        case MINWIN_VIEWER:
            strhxcat(&pszTemp, "MINWIN=VIEWER;");
        break;

        case MINWIN_DESKTOP:
            strhxcat(&pszTemp, "MINWIN=DESKTOP;");
        break;

        // ignore MINWIN_DEFAULT
    }

    // NOCOPY:
    // NODELETE:
    // NODRAG:
    // NODROP:
    // NOLINK:
    // NOMOVE:
    // NOPRINT:
    // NORENAME:
    // NOSETTINGS:
    // NOSHADOW:
    // NOTVISIBLE: compare wpQueryStyle with clsStyle
    ulStyle = _wpQueryStyle(somSelf);

    if (ulStyle & OBJSTYLE_TEMPLATE)
        strhxcat(&pszTemp, "TEMPLATE=YES;");

    // TITLE
    /* pszValue = _wpQueryTitle(somSelf);
    {
        strhxcat(&pszTemp, "TITLE=");
            strhxcat(&pszTemp, pszValue);
            strhxcat(&pszTemp, ";");
    } */

    // OBJECTID: always append this LAST!
    pszValue = _wpQueryObjectID(somSelf);
    if (pszValue)
        if (strlen(pszValue))
        {
            strhxcat(&pszTemp, "OBJECTID=");
            strhxcat(&pszTemp, pszValue);
            strhxcat(&pszTemp, ";");
        }

    /*
     * append string
     *
     */

    if (pszTemp)
    {
        // return string if buffer is given
        if ((pszSetupString) && (cbSetupString))
            strhncpy0(pszSetupString,   // target
                      pszTemp,          // source
                      cbSetupString);   // buffer size

        // always return length of string
        ulReturn = strlen(pszTemp);
        free(pszTemp);
    }

    return (ulReturn);
}

