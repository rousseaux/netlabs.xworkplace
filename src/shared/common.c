
/*
 *@@sourcefile common.c:
 *      this file contains functions that are common to all
 *      parts of XWorkplace.
 *
 *      These functions mainly deal with the following features:
 *
 *      -- module handling (cmnQueryMainModuleHandle etc.);
 *
 *      -- NLS management (cmnQueryNLSModuleHandle,
 *         cmnQueryNLSStrings);
 *
 *      -- global settings (cmnQueryGlobalSettings);
 *
 *      -- extended XWorkplace message boxes (cmnMessageBox).
 *
 *      Note that the system sound functions have been exported
 *      to helpers\syssound.c (V0.9.0).
 *
 *@@header "shared\common.h"
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

#define INCL_DOSMODULEMGR
#define INCL_DOSSEMAPHORES
#define INCL_DOSEXCEPTIONS
#define INCL_DOSPROCESS
#define INCL_DOSERRORS

#define INCL_WINSHELLDATA       // Prf* functions
#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR        // SC_CLOSE etc.
#define INCL_WININPUT
#define INCL_WINDIALOGS
#define INCL_WINMENUS
#define INCL_WINBUTTONS
#define INCL_WINSTDCNR
#define INCL_WINCOUNTRY
#define INCL_WINSYS

#define INCL_GPILOGCOLORTABLE
#include <os2.h>

// C library headers
#include <stdio.h>
#include <stdlib.h>
#include <string.h>             // mem*, str* functions
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h
#include <io.h>
// #include <locale.h>
#include <malloc.h>

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\cnrh.h"               // container helper routines
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\tmsgfile.h"           // "text message file" handling (for cmnGetMessage)
#include "helpers\winh.h"               // PM helper routines

// SOM headers which don't crash with prec. header files
#pragma hdrstop                         // VAC++ keeps crashing otherwise
#include <wpdesk.h>                     // WPDesktop

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel

#include "filesys\statbars.h"           // status bar translation logic
#include "filesys\xthreads.h"           // extra XWorkplace threads

// other SOM headers
#include "helpers\undoc.h"              // some undocumented stuff

/* ******************************************************************
 *                                                                  *
 *   Global variables                                               *
 *                                                                  *
 ********************************************************************/

HMTX            hmtxCommonLock = NULLHANDLE;

CHAR            szHelpLibrary[CCHMAXPATH] = "";
CHAR            szMessageFile[CCHMAXPATH] = "";

HMODULE         hmodNLS = NULLHANDLE;
NLSSTRINGS      *pNLSStringsGlobal = NULL;
GLOBALSETTINGS  *pGlobalSettings = NULL;

HMODULE         hmodIconsDLL = NULLHANDLE;
CHAR            szLanguageCode[20] = "";

ULONG           ulCurHelpPanel = 0;      // holds help panel for dialog

CHAR            szStatusBarFont[100];
CHAR            szSBTextNoneSel[CCHMAXMNEMONICS],
                szSBTextMultiSel[CCHMAXMNEMONICS];
ULONG           ulStatusBarHeight;

// module handling:
char            szDLLFile[CCHMAXPATH];
HMODULE         hmodDLL;

// Declare runtime prototypes, because there are no headers
// for these:

// _CRT_init is the C run-time environment initialization function.
// It will return 0 to indicate success and -1 to indicate failure.
int _CRT_init(void);

// _CRT_term is the C run-time environment termination function.
// It only needs to be called when the C run-time functions are statically
// linked, as is the case with XFolder.
void _CRT_term(void);

/* ******************************************************************
 *                                                                  *
 *   XFolder debugging helpers                                      *
 *                                                                  *
 ********************************************************************/

#ifdef _PMPRINTF_

/*
 *@@ cmnDumpMemoryBlock:
 *      if _PMPRINTF_ has been #define'd in common.h,
 *      this will dump a block of memory to the PMPRINTF
 *      output window. Useful for debugging internal
 *      structures.
 *      If _PMPRINTF_ has been NOT #define'd in common.h,
 *      no code will be produced. :-)
 */

void cmnDumpMemoryBlock(PBYTE pb,       // in: start address
                        ULONG ulSize,   // in: size of block
                        ULONG ulIndent) // in: how many spaces to put
                                        //     before each output line
{
    PBYTE   pbCurrent = pb;
    ULONG   ulCount = 0,
            ulLineCount = 0;
    CHAR    szLine[400] = "",
            szAscii[30] = "         ";
    PSZ     pszLine = szLine,
            pszAscii = szAscii;

    for (pbCurrent = pb;
         ulCount < ulSize;
         pbCurrent++, ulCount++)
    {
        if (ulLineCount == 0) {
            memset(szLine, ' ', ulIndent);
            pszLine += ulIndent;
        }
        pszLine += sprintf(pszLine, "%02lX ", *pbCurrent);

        if ((*pbCurrent > 31) && (*pbCurrent < 127))
            *pszAscii = *pbCurrent;
        else
            *pszAscii = '.';
        pszAscii++;

        ulLineCount++;
        if ( (ulLineCount > 7) || (ulCount == ulSize-1) )
        {
            _Pmpf(("%04lX:  %s  %s",
                    ulCount-7,
                    szLine,
                    szAscii));
            pszLine = szLine;
            pszAscii = szAscii;
            ulLineCount = 0;
        }
    }
}
#else
    // _PMPRINTF not #define'd: do nothing
    #define cmnDumpMemoryBlock(pb, ulSize, ulIndent)
#endif

/* ******************************************************************
 *                                                                  *
 *   Heap debugging window                                          *
 *                                                                  *
 ********************************************************************/

#ifdef __DEBUG_ALLOC__
        // if XWorkplace is compiled with
        // VAC++ debug memory funcs,
        // compile the heap window also

    /*
     *@@ MEMRECORD:
     *
     *@@added V0.9.1 (99-12-04) [umoeller]
     */

    typedef struct _MEMRECORD
    {
        RECORDCORE  recc;

        ULONG       ulIndex;

        const void  *pObject;
        const char  *filename;
        ULONG       useflag,
                    status;

        PSZ         pszUseFlag,     // const string
                    pszStatus;      // const string

        PSZ         pszAddress;     // points to szAddress
        CHAR        szAddress[20];

        ULONG       ulSize;

        PSZ         pszSource;      // points to szSource
        CHAR        szSource[CCHMAXPATH];

        ULONG       ulLine;

    } MEMRECORD, *PMEMRECORD;

    ULONG       ulHeapItemsCount1,
                ulHeapItemsCount2;
    ULONG       ulTotalAllocated,
                ulTotalFreed;
    PMEMRECORD  pMemRecordThis = NULL;
    PSZ         pszMemCnrTitle = NULL;

    /*
     *@@ fncbMemHeapWalkCount:
     *      callback func for _heap_walk function used for
     *      fnwpMemDebug.
     *
     *@@added V0.9.1 (99-12-04) [umoeller]
     */

    int fncbMemHeapWalkCount(const void *pObject,
                             size_t Size,
                             int useflag,
                             int status,
                             const char *filename,
                             size_t line)
    {
        // skip all the items which seem to be
        // internal to the runtime
        if ((filename) || (useflag == _FREEENTRY))
        {
            ulHeapItemsCount1++;
            if (useflag == _FREEENTRY)
                ulTotalFreed += Size;
            else
                ulTotalAllocated += Size;
        }
        return (0);
    }

    /*
     *@@ fncbMemHeapWalkFill:
     *      callback func for _heap_walk function used for
     *      fnwpMemDebug.
     *
     *@@added V0.9.1 (99-12-04) [umoeller]
     */

    int fncbMemHeapWalkFill(const void *pObject,
                            size_t Size,
                            int useflag,
                            int status,
                            const char *filename,
                            size_t line)
    {
        // skip all the items which seem to be
        // internal to the runtime
        if ((filename) || (useflag == _FREEENTRY))
        {
            ulHeapItemsCount2++;
            if ((pMemRecordThis) && (ulHeapItemsCount2 < ulHeapItemsCount1))
            {
                pMemRecordThis->ulIndex = ulHeapItemsCount2 - 1;

                pMemRecordThis->pObject = pObject;
                pMemRecordThis->useflag = useflag;
                pMemRecordThis->status = status;
                pMemRecordThis->filename = filename;

                pMemRecordThis->pszAddress = pMemRecordThis->szAddress;

                pMemRecordThis->ulSize = Size;

                pMemRecordThis->pszSource = pMemRecordThis->szSource;

                pMemRecordThis->ulLine = line;

                pMemRecordThis = (PMEMRECORD)pMemRecordThis->recc.preccNextRecord;
            }
            else
                return (1);     // stop
        }

        return (0);
    }

    /*
     *@@ mnu_fnCompareIndex:
     *
     *@@added V0.9.1 (99-12-03) [umoeller]
     */

    SHORT EXPENTRY mnu_fnCompareIndex(PMEMRECORD pmrc1, PMEMRECORD  pmrc2, PVOID pStorage)
    {
        HAB habDesktop = WinQueryAnchorBlock(HWND_DESKTOP);
        pStorage = pStorage; // to keep the compiler happy
        if ((pmrc1) && (pmrc2))
            if (pmrc1->ulIndex < pmrc2->ulIndex)
                return (-1);
            else if (pmrc1->ulIndex > pmrc2->ulIndex)
                return (1);

        return (0);
    }

    /*
     *@@ mnu_fnCompareSourceFile:
     *
     *@@added V0.9.1 (99-12-03) [umoeller]
     */

    SHORT EXPENTRY mnu_fnCompareSourceFile(PMEMRECORD pmrc1, PMEMRECORD  pmrc2, PVOID pStorage)
    {
        HAB habDesktop = WinQueryAnchorBlock(HWND_DESKTOP);
        pStorage = pStorage; // to keep the compiler happy
        if ((pmrc1) && (pmrc2))
                switch (WinCompareStrings(habDesktop, 0, 0,
                        pmrc1->szSource, pmrc2->szSource, 0))
                {
                    case WCS_LT: return (-1);
                    case WCS_GT: return (1);
                    default:    // equal
                        if (pmrc1->ulLine < pmrc2->ulLine)
                            return (-1);
                        else if (pmrc1->ulLine > pmrc2->ulLine)
                            return (1);

                }

        return (0);
    }

    #define ID_MEMCNR   1000

    /*
     *@@ cmn_fnwpMemDebug:
     *      client window proc for the heap debugger window
     *      accessible from the Desktop context menu if
     *      __DEBUG_ALLOC__ is defined. Otherwise, this is not
     *      compiled.
     *
     *      Usage: this is a regular PM client window procedure
     *      to be used with WinRegisterClass and WinCreateStdWindow.
     *      See dtpMenuItemSelected, which uses this.
     *
     *      This creates a container with all the memory objects
     *      with the size of the client area in turn.
     *
     *@@added V0.9.1 (99-12-04) [umoeller]
     */


    MRESULT EXPENTRY cmn_fnwpMemDebug(HWND hwndClient, ULONG msg, MPARAM mp1, MPARAM mp2)
    {
        MRESULT mrc = 0;

        switch (msg)
        {
            case WM_CREATE:
            {
                TRY_LOUD(excpt1, NULL)
                {
                    PCREATESTRUCT pcs = (PCREATESTRUCT)mp2;
                    HWND hwndCnr;
                    hwndCnr = WinCreateWindow(hwndClient,        // parent
                                              WC_CONTAINER,
                                              "",
                                              WS_VISIBLE | CCS_MINIICONS | CCS_READONLY | CCS_SINGLESEL,
                                              0, 0, 0, 0,
                                              hwndClient,        // owner
                                              HWND_TOP,
                                              ID_MEMCNR,
                                              NULL, NULL);
                    if (hwndCnr)
                    {
                        XFIELDINFO      xfi[7];
                        PFIELDINFO      pfi = NULL;
                        PMEMRECORD      pMemRecordFirst;
                        int             i = 0;

                        // set up cnr details view
                        xfi[i].ulFieldOffset = FIELDOFFSET(MEMRECORD, ulIndex);
                        xfi[i].pszColumnTitle = "No.";
                        xfi[i].ulDataType = CFA_ULONG;
                        xfi[i++].ulOrientation = CFA_RIGHT;

                        xfi[i].ulFieldOffset = FIELDOFFSET(MEMRECORD, pszUseFlag);
                        xfi[i].pszColumnTitle = "useflag";
                        xfi[i].ulDataType = CFA_STRING;
                        xfi[i++].ulOrientation = CFA_CENTER;

                        xfi[i].ulFieldOffset = FIELDOFFSET(MEMRECORD, pszStatus);
                        xfi[i].pszColumnTitle = "status";
                        xfi[i].ulDataType = CFA_STRING;
                        xfi[i++].ulOrientation = CFA_CENTER;

                        xfi[i].ulFieldOffset = FIELDOFFSET(MEMRECORD, pszSource);
                        xfi[i].pszColumnTitle = "Source";
                        xfi[i].ulDataType = CFA_STRING;
                        xfi[i++].ulOrientation = CFA_LEFT;

                        xfi[i].ulFieldOffset = FIELDOFFSET(MEMRECORD, ulLine);
                        xfi[i].pszColumnTitle = "Line";
                        xfi[i].ulDataType = CFA_ULONG;
                        xfi[i++].ulOrientation = CFA_RIGHT;

                        xfi[i].ulFieldOffset = FIELDOFFSET(MEMRECORD, ulSize);
                        xfi[i].pszColumnTitle = "Size";
                        xfi[i].ulDataType = CFA_ULONG;
                        xfi[i++].ulOrientation = CFA_RIGHT;

                        xfi[i].ulFieldOffset = FIELDOFFSET(MEMRECORD, pszAddress);
                        xfi[i].pszColumnTitle = "Address";
                        xfi[i].ulDataType = CFA_STRING;
                        xfi[i++].ulOrientation = CFA_LEFT;

                        pfi = cnrhSetFieldInfos(hwndCnr,
                                                &xfi[0],
                                                i,             // array item count
                                                TRUE,          // no draw lines
                                                2);            // return column

                        {
                            PSZ pszFont = "9.WarpSans";
                            WinSetPresParam(hwndCnr,
                                            PP_FONTNAMESIZE,
                                            strlen(pszFont),
                                            pszFont);
                        }

                        // count heap items
                        ulHeapItemsCount1 = 0;
                        ulTotalFreed = 0;
                        ulTotalAllocated = 0;
                        _heap_walk(fncbMemHeapWalkCount);

                        pMemRecordFirst = (PMEMRECORD)cnrhAllocRecords(hwndCnr,
                                                                       sizeof(MEMRECORD),
                                                                       ulHeapItemsCount1);
                        if (pMemRecordFirst)
                        {
                            ulHeapItemsCount2 = 0;
                            pMemRecordThis = pMemRecordFirst;
                            _heap_walk(fncbMemHeapWalkFill);

                            // the following doesn't work while _heap_walk is running
                            pMemRecordThis = pMemRecordFirst;
                            while (pMemRecordThis)
                            {
                                switch (pMemRecordThis->useflag)
                                {
                                    case _USEDENTRY: pMemRecordThis->pszUseFlag = "Used"; break;
                                    case _FREEENTRY: pMemRecordThis->pszUseFlag = "Freed"; break;
                                }

                                switch (pMemRecordThis->status)
                                {
                                    case _HEAPBADBEGIN: pMemRecordThis->pszStatus = "heap bad begin"; break;
                                    case _HEAPBADNODE: pMemRecordThis->pszStatus = "heap bad node"; break;
                                    case _HEAPEMPTY: pMemRecordThis->pszStatus = "heap empty"; break;
                                    case _HEAPOK: pMemRecordThis->pszStatus = "OK"; break;
                                }

                                sprintf(pMemRecordThis->szAddress, "0x%lX", pMemRecordThis->pObject);
                                strcpy(pMemRecordThis->szSource,
                                        (pMemRecordThis->filename)
                                            ? pMemRecordThis->filename
                                            : "?");

                                pMemRecordThis = (PMEMRECORD)pMemRecordThis->recc.preccNextRecord;
                            }

                            cnrhInsertRecords(hwndCnr,
                                              NULL,         // parent
                                              (PRECORDCORE)pMemRecordFirst,
                                              NULL,
                                              CRA_RECORDREADONLY,
                                              ulHeapItemsCount2);
                        }

                        BEGIN_CNRINFO()
                        {
                            CHAR szCnrTitle[1000];
                            sprintf(szCnrTitle,
                                    "Total allocated: %d bytes\n"
                                    "Total freed: %d bytes",
                                    ulTotalAllocated,
                                    ulTotalFreed);
                            pszMemCnrTitle = strdup(szCnrTitle);
                            cnrhSetTitle(pszMemCnrTitle);
                            cnrhSetView(CV_DETAIL | CV_MINI | CA_DETAILSVIEWTITLES
                                            | CA_DRAWICON
                                        | CA_CONTAINERTITLE | CA_TITLEREADONLY
                                            | CA_TITLESEPARATOR | CA_TITLELEFT);
                            cnrhSetSplitBarAfter(pfi);
                            cnrhSetSplitBarPos(250);
                        } END_CNRINFO(hwndCnr);
                    }
                }
                CATCH(excpt1) {} END_CATCH();

                mrc = WinDefWindowProc(hwndClient, msg, mp1, mp2);
            break; }

            case WM_WINDOWPOSCHANGED:
            {
                PSWP pswp = (PSWP)mp1;
                mrc = WinDefWindowProc(hwndClient, msg, mp1, mp2);
                if (pswp->fl & SWP_SIZE)
                {
                    WinSetWindowPos(WinWindowFromID(hwndClient, ID_MEMCNR), // cnr
                                    HWND_TOP,
                                    0, 0, pswp->cx, pswp->cy,
                                    SWP_SIZE | SWP_MOVE | SWP_SHOW);
                }
            break; }

            case WM_CONTROL:
            {
                USHORT usItemID = SHORT1FROMMP(mp1),
                       usNotifyCode = SHORT2FROMMP(mp1);
                if (usItemID == ID_MEMCNR)       // cnr
                {
                    switch (usNotifyCode)
                    {
                        case CN_CONTEXTMENU:
                        {
                            PMEMRECORD precc = (PMEMRECORD)mp2;
                            if (precc == NULL)
                            {
                                // whitespace
                                HWND hwndMenu = WinCreateMenu(HWND_DESKTOP,
                                                              NULL); // no menu template
                                winhInsertMenuItem(hwndMenu,
                                                   MIT_END,
                                                   1001,
                                                   "Sort by index",
                                                   MIS_TEXT, 0);
                                winhInsertMenuItem(hwndMenu,
                                                   MIT_END,
                                                   1002,
                                                   "Sort by source file",
                                                   MIS_TEXT, 0);
                                cnrhShowContextMenu(WinWindowFromID(hwndClient, ID_MEMCNR),
                                                    NULL,       // record
                                                    hwndMenu,
                                                    hwndClient);
                            }
                        }
                    }
                }
            break; }

            case WM_COMMAND:
                switch (SHORT1FROMMP(mp1))
                {
                    case 1001:  // sort by index
                        WinSendMsg(WinWindowFromID(hwndClient, ID_MEMCNR),
                                   CM_SORTRECORD,
                                   (MPARAM)mnu_fnCompareIndex,
                                   0);
                    break;

                    case 1002:  // sort by source file
                        WinSendMsg(WinWindowFromID(hwndClient, ID_MEMCNR),
                                   CM_SORTRECORD,
                                   (MPARAM)mnu_fnCompareSourceFile,
                                   0);
                    break;
                }
            break;

            case WM_CLOSE:
                WinDestroyWindow(WinWindowFromID(hwndClient, ID_MEMCNR));
                WinDestroyWindow(WinQueryWindow(hwndClient, QW_PARENT));
                free(pszMemCnrTitle);
                pszMemCnrTitle = NULL;
            break;

            default:
                mrc = WinDefWindowProc(hwndClient, msg, mp1, mp2);
        }

        return (mrc);
    }

    /*
     *@@ cmnCreateMemDebugWindow:
     *
     *@@added V0.9.1 (99-12-18) [umoeller]
     */

    VOID cmnCreateMemDebugWindow(VOID)
    {
        ULONG flStyle = FCF_TITLEBAR | FCF_SYSMENU | FCF_HIDEMAX
                        | FCF_SIZEBORDER | FCF_SHELLPOSITION
                        | FCF_NOBYTEALIGN | FCF_TASKLIST;
        if (WinRegisterClass(WinQueryAnchorBlock(HWND_DESKTOP),
                             "XWPMemDebug",
                             cmn_fnwpMemDebug, 0L, 0))
        {
            HWND hwndClient, hwndCnr;
            HWND hwndMemFrame = WinCreateStdWindow(HWND_DESKTOP,
                                                   0L,
                                                   &flStyle,
                                                   "XWPMemDebug",
                                                   "Allocated XWorkplace Memory Objects",
                                                   0L,
                                                   NULLHANDLE,     // resource
                                                   0,
                                                   &hwndClient);
            if (hwndMemFrame)
            {
                WinSetWindowPos(hwndMemFrame,
                                NULLHANDLE,
                                0, 0, 0, 0,
                                SWP_SHOW | SWP_ACTIVATE);
            }
        }
    }

#endif // __DEBUG_ALLOC__

/* ******************************************************************
 *                                                                  *
 *   Main module handling (XFLDR.DLL)                               *
 *                                                                  *
 ********************************************************************/

/*
 *@@ _DLL_InitTerm:
 *     this function gets called when the OS/2 DLL loader loads and frees
 *     this DLL. Since this is a SOM DLL for the WPS, this gets called
 *     right when the WPS is starting and when the WPS process ends, e.g.
 *     due to a WPS restart or trap. Since the WPS is the only process
 *     calling this DLL, we need not bother with details.
 *
 *     Defining this function is my preferred way of getting the DLL's module
 *     handle, instead of querying the SOM kernel for the module name, like
 *     this is done in most WPS sample programs provided by IBM. I have found
 *     this to be much easier and less error-prone when several classes are
 *     put into one DLL (as is the case with XWorkplace).
 *
 *     We store the module handle in a global variable which can later quickly
 *     be retrieved using cmnQueryMainModuleHandle.
 *
 *     Since OS/2 calls this function directly, it must have _System linkage.
 *     Note: You must then link using the /NOE option, because the VAC++ runtimes
 *     also contain a _DLL_Initterm, and the linker gets in trouble otherwise.
 *     The XWorkplace makefile takes care of this.
 *
 *     This function must return 0 upon errors or 1 otherwise.
 *
 *@@changed V0.9.0 [umoeller]: reworked locale initialization
 *@@changed V0.9.0 [umoeller]: moved this func here from module.c
 */

unsigned long _System _DLL_InitTerm(unsigned long hModule,
                                    unsigned long ulFlag)
{
    APIRET rc;

    switch (ulFlag)
    {
        case 0:
        {
            // DLL being loaded:
            CHAR    szTemp[400];
            PSZ     p = NULL;
            ULONG   aulCPList[8] = {0},
                    cbCPList = sizeof(aulCPList),
                    ulListSize = 0;

            // store the DLL handle in the global variable so that
            // cmnQueryMainModuleHandle() below can return it
            hmodDLL = hModule;

            // now initialize the C run-time environment before we
            // call any runtime functions
            if (_CRT_init() == -1)
               return (0);  // error

            if (rc = DosQueryModuleName(hModule, CCHMAXPATH, szDLLFile))
                    DosBeep(100, 100);
        break; }

        case 1:
            // DLL being freed: cleanup runtime
            _CRT_term();
            break;

        default:
            // other code: beep for error
            DosBeep(100, 100);
            return (0);     // error
    }

    // a non-zero value must be returned to indicate success
    return (1);
}

/*
 *@@ cmnQueryMainModuleHandle:
 *      this may be used to retrieve the module handle
 *      of XFLDR.DLL, which was stored by _DLL_InitTerm.
 *
 *      Note that this returns the main module handle.
 *      Use this if you need NLS-independent resources,
 *      e.g. the icons stored in XFLDR.DLL. To get the
 *      NLS module handle, use cmnQueryNLSModuleHandle.
 *
 *@@changed V0.9.0 [umoeller]: moved this func here from module.c
 */

HMODULE cmnQueryMainModuleHandle(VOID)
{
    return (hmodDLL);
}

/*
 *@@ cmnQueryMainModuleFilename:
 *      this may be used to retrieve the fully
 *      qualified file name of the DLL
 *      which was stored by _DLL_InitTerm.
 *
 *@@changed V0.9.0 [umoeller]: moved this func here from module.c
 */

const char* cmnQueryMainModuleFilename(VOID)
{
    return (szDLLFile);
}

/* ******************************************************************
 *                                                                  *
 *   Resource protection (thread safety                             *
 *                                                                  *
 ********************************************************************/

/*
 *@@ cmnLock:
 *      function to request the global
 *      hmtxCommonLock semaphore to finally
 *      make the "cmn" functions thread-safe.
 *
 *      Returns TRUE if the semaphore could be
 *      accessed within the specified timeout.
 *
 *      Usage:
 *
 +      if (cmnLock(4000))
 +      {
 +          TRY_LOUD(excpt1, cmnOnKillDuringLock)
 +          {
 +                  // ... precious code here
 +          }
 +          CATCH(excpt1) { } END_CATCH();
 +
 +          cmnUnlock();        // NEVER FORGET THIS!!
 +      }
 +
 *
 *@@added V0.9.0 (99-11-14) [umoeller]
 */

BOOL cmnLock(ULONG ulTimeout)
{
    if (hmtxCommonLock == NULLHANDLE)
        DosCreateMutexSem(NULL,         // unnamed
                          &hmtxCommonLock,
                          0,            // unshared
                          FALSE);       // unowned

    if (DosRequestMutexSem(hmtxCommonLock, ulTimeout) == NO_ERROR)
        return TRUE;
    else
        return FALSE;
}

/*
 *@@ cmnUnlock:
 *
 *@@added V0.9.0 (99-11-14) [umoeller]
 */

VOID cmnUnlock(VOID)
{
    DosReleaseMutexSem(hmtxCommonLock);
}

/*
 *@@ cmnOnKillDuringLock:
 *      function to be used with the TRY_xxx
 *      macros to release the mutex semaphore
 *      on thread kills.
 *
 *@@added V0.9.0 (99-11-14) [umoeller]
 */

VOID APIENTRY cmnOnKillDuringLock(VOID)
{
    DosReleaseMutexSem(hmtxCommonLock);
}

/* ******************************************************************
 *                                                                  *
 *   XWorkplace National Language Support (NLS)                     *
 *                                                                  *
 ********************************************************************/

/*
 *  The following routines are for querying the XFolder
 *  installation path and similiar routines, such as
 *  querying the current NLS module, changing it, loading
 *  strings, the help file and all that sort of stuff.
 */

/*
 *@@ cmnQueryXFolderBasePath:
 *      this routine returns the path of where XFolder was installed,
 *      i.e. the parent directory of where the xfldr.dll file
 *      resides, without a trailing backslash (e.g. "C:\XFolder").
 *      The buffer to copy this to is assumed to be CCHMAXPATH in size.
 *      As opposed to versions before V0.81, OS2.INI is no longer
 *      needed for this to work. The path is retrieved from the
 *      DLL directly by evaluating what was passed to _DLL_InitModule
 *      (module.c).
 */

BOOL cmnQueryXFolderBasePath(PSZ pszPath)
{
    BOOL brc = FALSE;
    const char *pszDLL = cmnQueryMainModuleFilename();
    if (pszDLL)
    {
        // copy until last backslash minus four characters
        // (leave out "\bin\xfldr.dll")
        PSZ pszLastSlash = strrchr(pszDLL, '\\');
        #ifdef DEBUG_LANGCODES
            _Pmpf(( "cmnQueryMainModuleFilename: %s", pszDLL));
        #endif
        strncpy(pszPath, pszDLL, (pszLastSlash-pszDLL)-4);
        pszPath[(pszLastSlash-pszDLL-4)] = '\0';
        brc = TRUE;
    }
    #ifdef DEBUG_LANGCODES
        _Pmpf(( "cmnQueryXFolderBasePath: %s", pszPath ));
    #endif
    return (brc);
}

/*
 *@@ cmnQueryLanguageCode:
 *      returns PSZ to three-digit language code (e.g. "001").
 *      This points to a global variable, so do NOT change.
 *
 *@@changed V0.9.0 (99-11-14) [umoeller]: made this reentrant, finally
 */

const char* cmnQueryLanguageCode(VOID)
{
    if (cmnLock(5000))
    {
        TRY_LOUD(excpt1, cmnOnKillDuringLock)
        {
            if (szLanguageCode[0] == '\0')
                PrfQueryProfileString(HINI_USERPROFILE,
                                      INIAPP_XWORKPLACE, INIKEY_LANGUAGECODE,
                                      DEFAULT_LANGUAGECODE,
                                      (PVOID)szLanguageCode,
                                      sizeof(szLanguageCode));

            szLanguageCode[3] = '\0';
            #ifdef DEBUG_LANGCODES
                _Pmpf(( "cmnQueryLanguageCode: %s", szLanguageCode ));
            #endif
        }
        CATCH(excpt1) { } END_CATCH();

        cmnUnlock();
    }

    return (szLanguageCode);
}

/*
 *@@ cmnSetLanguageCode:
 *      changes XFolder's language to three-digit language code in
 *      pszLanguage (e.g. "001"). This does not reload the NLS DLL,
 *      but only change the setting.
 *
 *@@changed V0.9.0 (99-11-14) [umoeller]: made this reentrant, finally
 */

BOOL cmnSetLanguageCode(PSZ pszLanguage)
{
    BOOL brc = FALSE;

    if (cmnLock(5000))
    {
        TRY_LOUD(excpt1, cmnOnKillDuringLock)
        {
            strcpy(szLanguageCode, pszLanguage);
            szLanguageCode[3] = 0;

            brc = PrfWriteProfileString(HINI_USERPROFILE,
                                        INIAPP_XWORKPLACE, INIKEY_LANGUAGECODE,
                                        szLanguageCode);
        }
        CATCH(excpt1) { } END_CATCH();

        cmnUnlock();
    }

    return (brc);
}

/*
 *@@ cmnQueryHelpLibrary:
 *      returns PSZ to full help library path in XFolder directory,
 *      depending on where XFolder was installed and on the current
 *      language (e.g. "C:\XFolder\help\xfldr001.hlp").
 *
 *      This PSZ points to a global variable, so you better not
 *      change it.
 *
 *@@changed V0.9.0 (99-11-14) [umoeller]: made this reentrant, finally
 */

const char* cmnQueryHelpLibrary(VOID)
{
    const char *rc = 0;

    if (cmnLock(5000))
    {
        TRY_LOUD(excpt1, cmnOnKillDuringLock)
        {
            if (cmnQueryXFolderBasePath(szHelpLibrary))
            {
                // path found: append helpfile
                sprintf(szHelpLibrary+strlen(szHelpLibrary),
                        "\\help\\xfldr%s.hlp",
                        cmnQueryLanguageCode());
                #ifdef DEBUG_LANGCODES
                    _Pmpf(( "cmnQueryHelpLibrary: %s", szHelpLibrary ));
                #endif
                rc = szHelpLibrary;
            }
        }
        CATCH(excpt1) { } END_CATCH();

        cmnUnlock();
    }

    return (rc);
}

/*
 *@@ cmnDisplayHelp:
 *      displays an XWorkplace help panel,
 *      using wpDisplayHelp.
 *      If somSelf == NULL, we'll query the
 *      active desktop.
 */

BOOL cmnDisplayHelp(WPObject *somSelf,
                    ULONG ulPanelID)
{
    BOOL brc = FALSE;
    if (somSelf == NULL)
        somSelf = _wpclsQueryActiveDesktop(_WPDesktop);

    if (somSelf)
    {
        brc = _wpDisplayHelp(somSelf,
                             ulPanelID,
                             (PSZ)cmnQueryHelpLibrary());
        if (!brc)
            // complain
            cmnMessageBoxMsg(HWND_DESKTOP, 104, 134, MB_OK);
    }
    return (brc);
}

/*
 *@@ cmnQueryMessageFile:
 *      returns PSZ to full message file path in XFolder directory,
 *      depending on where XFolder was installed and on the current
 *      language (e.g. "C:\XFolder\help\xfldr001.tmf").
 *
 *@@changed V0.9.0 [umoeller]: changed, this now returns the TMF file (tmsgfile.c).
 *@@changed V0.9.0 (99-11-14) [umoeller]: made this reentrant, finally
 */

const char* cmnQueryMessageFile(VOID)
{
    const char *rc = 0;
    if (cmnLock(5000))
    {
        TRY_LOUD(excpt1, cmnOnKillDuringLock)
        {
            if (cmnQueryXFolderBasePath(szMessageFile))
            {
                // path found: append message file
                sprintf(szMessageFile+strlen(szMessageFile),
                        "\\help\\xfldr%s.tmf",
                        cmnQueryLanguageCode());
                #ifdef DEBUG_LANGCODES
                    _Pmpf(( "cmnQueryMessageFile: %s", szMessageFile));
                #endif
                rc = szMessageFile;
            }
        }
        CATCH(excpt1) { } END_CATCH();

        cmnUnlock();
    }

    return (rc);
}

/*
 *@@ cmnQueryIconsDLL:
 *      this returns the HMODULE of XFolder ICONS.DLL
 *      (new with V0.84).
 *
 *      If this is queried for the first time, the DLL
 *      is loaded from the /BIN directory.
 *
 *      In this case, this routine also checks if
 *      ICONS.DLL exists in the /ICONS directory. If
 *      so, it is copied to /BIN before loading the
 *      DLL. This allows for replacing the DLL using
 *      the REXX script in /ICONS.
 *
 *@@added V0.84 [umoeller]
 *@@changed V0.9.0 (99-11-14) [umoeller]: made this reentrant, finally
 */

HMODULE cmnQueryIconsDLL(VOID)
{
    if (cmnLock(5000))
    {
        TRY_LOUD(excpt1, cmnOnKillDuringLock)
        {
            // first query?
            if (hmodIconsDLL == NULLHANDLE)
            {
                CHAR    szIconsDLL[CCHMAXPATH],
                        szNewIconsDLL[CCHMAXPATH];
                cmnQueryXFolderBasePath(szIconsDLL);
                strcpy(szNewIconsDLL, szIconsDLL);

                sprintf(szIconsDLL+strlen(szIconsDLL),
                        "\\bin\\icons.dll");
                sprintf(szNewIconsDLL+strlen(szNewIconsDLL),
                        "\\icons\\icons.dll");

                #ifdef DEBUG_LANGCODES
                    _Pmpf(("cmnQueryIconsDLL: checking %s", szNewIconsDLL));
                #endif
                // first check for /ICONS/ICONS.DLL
                if (access(szNewIconsDLL, 0) == 0)
                {
                    #ifdef DEBUG_LANGCODES
                        _Pmpf(("    found, copying to %s", szIconsDLL));
                    #endif
                    // exists: move to /BIN
                    // and use that one
                    DosDelete(szIconsDLL);
                    DosMove(szNewIconsDLL,      // old
                            szIconsDLL);        // new
                    DosDelete(szNewIconsDLL);
                }

                #ifdef DEBUG_LANGCODES
                    _Pmpf(("cmnQueryIconsDLL: loading %s", szIconsDLL));
                #endif
                // now load /BIN/ICONS.DLL
                if (DosLoadModule(NULL,
                                  0,
                                  szIconsDLL,
                                  &hmodIconsDLL)
                        != NO_ERROR)
                    hmodIconsDLL = NULLHANDLE;
            }

            #ifdef DEBUG_LANGCODES
                _Pmpf(("cmnQueryIconsDLL: returning %lX", hmodIconsDLL));
            #endif

        }
        CATCH(excpt1) { } END_CATCH();

        cmnUnlock();
    }

    return (hmodIconsDLL);
}

/*
 *@@ cmnQueryBootLogoFile:
 *      this returns the boot logo file as stored
 *      in OS2.INI. If it is not stored there,
 *      we return the default xfolder.bmp in
 *      the XFolder installation directories.
 *
 *      The return value of this function must
 *      be free()'d after use.
 *
 *@@added V0.9.0 [umoeller]
 *@@changed V0.9.0 (99-11-14) [umoeller]: made this reentrant, finally
 */

PSZ cmnQueryBootLogoFile(VOID)
{
    PSZ pszReturn = 0;

    if (cmnLock(5000))
    {
        TRY_LOUD(excpt1, cmnOnKillDuringLock)
        {
            pszReturn = prfhQueryProfileData(HINI_USER,
                                             INIAPP_XWORKPLACE,
                                             INIKEY_BOOTLOGOFILE,
                                             NULL);
            if (!pszReturn)
            {
                CHAR szBootLogoFile[CCHMAXPATH];
                // INI data not found: return default file
                cmnQueryXFolderBasePath(szBootLogoFile);
                strcat(szBootLogoFile,
                        "\\bootlogo\\xfolder.bmp");
                pszReturn = strdup(szBootLogoFile);
            }
        }
        CATCH(excpt1) { } END_CATCH();

        cmnUnlock();
    }

    return (pszReturn);
}

/*
 *@@ cmnLoadString:
 *      pretty similar to WinLoadString, but allocates
 *      necessary memory as well. *ppsz is a pointer
 *      to a PSZ; if this PSZ is != NULL, whatever it
 *      points to will be free()d, so you should set this
 *      to NULL if you initially call this function.
 *      This is used at WPS startup and when XFolder's
 *      language is changed later to load all the strings
 *      from a NLS DLL (cmnQueryNLSModuleHandle).
 *
 *@@changed V0.9.0 [umoeller]: "string not found" is now re-allocated using strdup (avoids crashes)
 *@@changed V0.9.0 (99-11-28) [umoeller]: added more meaningful error message
 */

void cmnLoadString(HAB habDesktop,
                   HMODULE hmodResource,
                   ULONG ulID,
                   PSZ *ppsz)
{
    CHAR    szBuf[200];
    if (*ppsz)
        free(*ppsz);
    if (WinLoadString(habDesktop, hmodResource, ulID , sizeof(szBuf), szBuf))
        *ppsz = strcpy(malloc(strlen(szBuf)+1), szBuf);
    else
    {
        CHAR    szMsg[1000];
        sprintf(szMsg, "cmnLoadString error: string resource %d not found in module 0x%lX",
                       ulID, hmodResource);
        *ppsz = strdup(szMsg); // V0.9.0
    }
}

/*
 *@@ cmnQueryNLSModuleHandle:
 *      returns the module handle of the language-dependent XFolder
 *      National Language Support DLL (XFLDRxxx.DLL).
 *
 *      This is called in two situations:
 *          1) with fEnforceReload == FALSE everytime some part
 *             of XFolder needs the NLS resources (e.g. for dialogs);
 *             this only loads the NLS DLL on the very first call
 *             then, whose module handle is cached for subsequent calls.
 *
 *          2) with fEnforceReload == TRUE when the user changes
 *             XFolder's language in the "Workplace Shell" object.
 *
 *      If the DLL is (re)loaded, this function also initializes
 *      all language-dependent XWorkplace components such as the NLSSTRINGS
 *      structure, which can always be accessed using cmnQueryNLSStrings.
 *      This function also checks for whether the NLS DLL has a
 *      decent version level to support this XFolder version.
 *
 *@@changed V0.9.0 [umoeller]: added various NLS strings
 *@@changed V0.9.0 (99-11-14) [umoeller]: made this reentrant, finally
 */

HMODULE cmnQueryNLSModuleHandle(BOOL fEnforceReload)
{
    if (cmnLock(5000))
    {
        TRY_LOUD(excpt1, cmnOnKillDuringLock)
        {
            CHAR    szResourceModuleName[CCHMAXPATH];
            // ULONG   ulCopied;
            HMODULE hmodOldResource = hmodNLS;

            // load resource DLL if it's not loaded yet or a reload is enforced
            if ((hmodNLS == NULLHANDLE) || (fEnforceReload))
            {
                NLSSTRINGS *pNLSStrings = (NLSSTRINGS*)cmnQueryNLSStrings();

                // get the XFolder path first
                if (cmnQueryXFolderBasePath(szResourceModuleName))
                {
                    // now compose module name from language code
                    strcat(szResourceModuleName, "\\bin\\xfldr");
                    strcat(szResourceModuleName, cmnQueryLanguageCode());
                    strcat(szResourceModuleName, ".dll");

                    // try to load the module
                    if (DosLoadModule(NULL,
                                       0,
                                       szResourceModuleName,
                                       (PHMODULE)&hmodNLS))
                    {
                        DebugBox("XFolder: Couldn't Find Resource DLL",
                            szResourceModuleName);
                    }
                    else
                    {
                        // module loaded alright!
                        HAB habDesktop = WinQueryAnchorBlock(HWND_DESKTOP);

                        if (habDesktop)
                        {
                            if (fEnforceReload)
                            {
                                // if fEnforceReload == TRUE, we will load a test string from
                                // the module to see if it has at least the version level which
                                // this XFolder version requires. This is done using a #define
                                // in dlgids.h: XFOLDER_VERSION is compiled as a string resource
                                // into both the NLS DLL and into the main DLL (this file),
                                // so we always have the versions in there automatically.
                                // MINIMUM_NLS_VERSION (dlgids.h too) contains the minimum
                                // NLS version level that this XFolder version requires.
                                CHAR   szTest[30] = "";
                                LONG   lLength;
                                lLength = WinLoadString(habDesktop,
                                                        hmodNLS,
                                                        ID_XSSI_XFOLDERVERSION,
                                                        sizeof(szTest), szTest);
                                #ifdef DEBUG_LANGCODES
                                    _Pmpf(("%s version: %s", szResourceModuleName, szTest));
                                #endif

                                if (    (lLength == 0)
                                    ||  (memcmp(szTest, MINIMUM_NLS_VERSION, 4) < 0)
                                            // szTest has NLS version (e.g. "0.81 beta"),
                                            // MINIMUM_NLS_VERSION has minimum version required
                                            // (e.g. "0.9.0")
                                   )
                                {
                                    DosBeep(1500, 100);
                                    cmnSetHelpPanel(-1);
                                    if (lLength == 0)
                                    {
                                        // version string not found: complain
                                        DebugBox("XFolder", "The requested file is not an XFolder National Language Support DLL.");
                                        DosFreeModule(hmodNLS);
                                        hmodNLS = hmodOldResource;
                                        return NULLHANDLE;
                                    }
                                    else
                                    {
                                        // version level not sufficient:
                                        // load dialog from previous NLS DLL which says
                                        // that the DLL is too old; if user presses
                                        // "Cancel", we abort loading the DLL
                                        DosBeep(1700, 100);
                                        if (WinDlgBox(HWND_DESKTOP,
                                                      HWND_DESKTOP,
                                                      (PFNWP)fnwpDlgGeneric,
                                                      hmodOldResource,
                                                      ID_XFD_WRONGVERSION,
                                                      (PVOID)NULL)
                                                == DID_CANCEL)
                                        {
                                            DebugBox("XFolder", "The new National Language Support DLL was not loaded.");
                                            // unload new NLS DLL
                                            DosFreeModule(hmodNLS);
                                            hmodNLS = hmodOldResource;
                                            return NULLHANDLE;
                                        }
                                    }
                                }
                            }

                            // now let's load strings
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_NOTDEFINED,
                                    &(pNLSStrings->pszNotDefined));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_PRODUCTINFO,
                                    &(pNLSStrings->pszProductInfo));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_REFRESHNOW,
                                    &(pNLSStrings->pszRefreshNow));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_SNAPTOGRID,
                                    &(pNLSStrings->pszSnapToGrid));

                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_FLDRCONTENT,
                                    &(pNLSStrings->pszFldrContent));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_COPYFILENAME,
                                    &(pNLSStrings->pszCopyFilename));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_BORED,
                                    &(pNLSStrings->pszBored));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_FLDREMPTY,
                                    &(pNLSStrings->pszFldrEmpty));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_SELECTSOME,
                                    &(pNLSStrings->pszSelectSome));

                            cmnLoadString(habDesktop, hmodNLS, ID_XFSI_QUICKSTATUS,
                                    &(pNLSStrings->pszQuickStatus));

                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_SV_NAME,
                                    &(pNLSStrings->pszSortByName));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_SV_TYPE,
                                    &(pNLSStrings->pszSortByType));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_SV_CLASS,
                                    &(pNLSStrings->pszSortByClass));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_SV_REALNAME,
                                    &(pNLSStrings->pszSortByRealName));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_SV_SIZE,
                                    &(pNLSStrings->pszSortBySize));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_SV_WRITEDATE,
                                    &(pNLSStrings->pszSortByWriteDate));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_SV_ACCESSDATE,
                                    &(pNLSStrings->pszSortByAccessDate));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_SV_CREATIONDATE,
                                    &(pNLSStrings->pszSortByCreationDate));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_SV_EXT,
                                    &(pNLSStrings->pszSortByExt));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_SV_FOLDERSFIRST,
                                    &(pNLSStrings->pszSortFoldersFirst));

                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_KEY_CTRL,
                                    &(pNLSStrings->pszCtrl));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_KEY_Alt,
                                    &(pNLSStrings->pszAlt));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_KEY_SHIFT,
                                    &(pNLSStrings->pszShift));

                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_KEY_BACKSPACE,
                                    &(pNLSStrings->pszBackspace));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_KEY_TAB,
                                    &(pNLSStrings->pszTab));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_KEY_BACKTABTAB,
                                    &(pNLSStrings->pszBacktab));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_KEY_ENTER,
                                    &(pNLSStrings->pszEnter));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_KEY_ESC,
                                    &(pNLSStrings->pszEsc));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_KEY_SPACE,
                                    &(pNLSStrings->pszSpace));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_KEY_PAGEUP,
                                    &(pNLSStrings->pszPageup));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_KEY_PAGEDOWN,
                                    &(pNLSStrings->pszPagedown));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_KEY_END,
                                    &(pNLSStrings->pszEnd));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_KEY_HOME,
                                    &(pNLSStrings->pszHome));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_KEY_LEFT,
                                    &(pNLSStrings->pszLeft));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_KEY_UP,
                                    &(pNLSStrings->pszUp));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_KEY_RIGHT,
                                    &(pNLSStrings->pszRight));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_KEY_DOWN,
                                    &(pNLSStrings->pszDown));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_KEY_PRINTSCRN,
                                    &(pNLSStrings->pszPrintscrn));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_KEY_INSERT,
                                    &(pNLSStrings->pszInsert));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_KEY_DELETE,
                                    &(pNLSStrings->pszDelete));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_KEY_SCRLLOCK,
                                    &(pNLSStrings->pszScrlLock));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_KEY_NUMLOCK,
                                    &(pNLSStrings->pszNumLock));

                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_KEY_WINLEFT,
                                    &(pNLSStrings->pszWinLeft));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_KEY_WINRIGHT,
                                    &(pNLSStrings->pszWinRight));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_KEY_WINMENU,
                                    &(pNLSStrings->pszWinMenu));

                            cmnLoadString(habDesktop, hmodNLS, ID_SDSI_FLUSHING,
                                    &(pNLSStrings->pszSDFlushing));
                            cmnLoadString(habDesktop, hmodNLS, ID_SDSI_CAD,
                                    &(pNLSStrings->pszSDCAD));
                            cmnLoadString(habDesktop, hmodNLS, ID_SDSI_REBOOTING,
                                    &(pNLSStrings->pszSDRebooting));
                            cmnLoadString(habDesktop, hmodNLS, ID_SDSI_CLOSING,
                                    &(pNLSStrings->pszSDClosing));
                            cmnLoadString(habDesktop, hmodNLS, ID_SDSI_SHUTDOWN,
                                    &(pNLSStrings->pszShutdown));
                            cmnLoadString(habDesktop, hmodNLS, ID_SDSI_RESTARTWPS,
                                    &(pNLSStrings->pszRestartWPS));
                            cmnLoadString(habDesktop, hmodNLS, ID_SDSI_RESTARTINGWPS,
                                    &(pNLSStrings->pszSDRestartingWPS));
                            cmnLoadString(habDesktop, hmodNLS, ID_SDSI_SAVINGDESKTOP,
                                    &(pNLSStrings->pszSDSavingDesktop));
                            cmnLoadString(habDesktop, hmodNLS, ID_SDSI_SAVINGPROFILES,
                                    &(pNLSStrings->pszSDSavingProfiles));

                            cmnLoadString(habDesktop, hmodNLS, ID_SDSI_STARTING,
                                    &(pNLSStrings->pszStarting));

                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_POPULATING,
                                    &(pNLSStrings->pszPopulating));

                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_1GENERIC,
                                    &(pNLSStrings->psz1Generic));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_2REMOVEITEMS,
                                    &(pNLSStrings->psz2RemoveItems));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_25ADDITEMS,
                                    &(pNLSStrings->psz25AddItems));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_26CONFIGITEMS,
                                    &(pNLSStrings->psz26ConfigFolderMenus));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_27STATUSBAR,
                                    &(pNLSStrings->psz27StatusBar));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_3SNAPTOGRID,
                                    &(pNLSStrings->psz3SnapToGrid));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_4ACCELERATORS,
                                    &(pNLSStrings->psz4Accelerators));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_5INTERNALS,
                                    &(pNLSStrings->psz5Internals));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_FILEOPS,
                                    &(pNLSStrings->pszFileOps));

                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_SORT,
                                    &(pNLSStrings->pszSort));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_SV_ALWAYSSORT,
                                    &(pNLSStrings->pszAlwaysSort));

                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_INTERNALS,
                                    &(pNLSStrings->pszInternals));

                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_XWPSTATUS,
                                    &(pNLSStrings->pszXWPStatus));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_FEATURES,
                                    &(pNLSStrings->pszFeatures));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_PARANOIA,
                                    &(pNLSStrings->pszParanoia));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_OBJECTS,
                                    &(pNLSStrings->pszObjects));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_FILEPAGE,
                                    &(pNLSStrings->pszFilePage));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_DETAILSPAGE,
                                    &(pNLSStrings->pszDetailsPage));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_XSHUTDOWNPAGE,
                                    &(pNLSStrings->pszXShutdownPage));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_STARTUPPAGE,
                                    &(pNLSStrings->pszStartupPage));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_DTPMENUPAGE,
                                    &(pNLSStrings->pszDtpMenuPage));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_FILETYPESPAGE,
                                    &(pNLSStrings->pszFileTypesPage));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_SOUNDSPAGE,
                                    &(pNLSStrings->pszSoundsPage));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_VIEWPAGE,
                                    &(pNLSStrings->pszViewPage));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_ARCHIVESPAGE,
                                    &(pNLSStrings->pszArchivesPage));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_PGMFILE_MODULE,
                                    &(pNLSStrings->pszModulePage));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_OBJECTHOTKEYSPAGE,
                                    &(pNLSStrings->pszObjectHotkeysPage));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_MOUSEHOOKPAGE,
                                    &(pNLSStrings->pszMouseHookPage));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_MAPPINGSPAGE,
                                    &(pNLSStrings->pszMappingsPage));

                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_SB_CLASSMNEMONICS,
                                    &(pNLSStrings->pszSBClassMnemonics));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_SB_CLASSNOTSUPPORTED,
                                    &(pNLSStrings->pszSBClassNotSupported));

                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_WPSCLASSES,
                                    &(pNLSStrings->pszWpsClasses));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_WPSCLASSLOADED,
                                    &(pNLSStrings->pszWpsClassLoaded));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_WPSCLASSLOADINGFAILED,
                                    &(pNLSStrings->pszWpsClassLoadingFailed));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_WPSCLASSREPLACEDBY,
                                    &(pNLSStrings->pszWpsClassReplacedBy));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_WPSCLASSORPHANS,
                                    &(pNLSStrings->pszWpsClassOrphans));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_WPSCLASSORPHANSINFO,
                                    &(pNLSStrings->pszWpsClassOrphansInfo));

                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_SCHEDULER,
                                    &(pNLSStrings->pszScheduler));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_MEMORY,
                                    &(pNLSStrings->pszMemory));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_ERRORS,
                                    &(pNLSStrings->pszErrors));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_WPS,
                                    &(pNLSStrings->pszWPS));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_SYSPATHS,
                                    &(pNLSStrings->pszSysPaths));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_DRIVERS,
                                    &(pNLSStrings->pszDrivers));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_DRIVERCATEGORIES,
                                    &(pNLSStrings->pszDriverCategories));

                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_PROCESSCONTENT,
                                    &(pNLSStrings->pszProcessContent));

                            cmnLoadString(habDesktop, hmodNLS, ID_XFSI_SETTINGS,
                                    &(pNLSStrings->pszSettings));
                            cmnLoadString(habDesktop, hmodNLS, ID_XFSI_SETTINGSNOTEBOOK,
                                    &(pNLSStrings->pszSettingsNotebook));
                            cmnLoadString(habDesktop, hmodNLS, ID_XFSI_ATTRIBUTES,
                                    &(pNLSStrings->pszAttributes));
                            cmnLoadString(habDesktop, hmodNLS, ID_XFSI_ATTR_ARCHIVED,
                                    &(pNLSStrings->pszAttrArchived));
                            cmnLoadString(habDesktop, hmodNLS, ID_XFSI_ATTR_SYSTEM,
                                    &(pNLSStrings->pszAttrSystem));
                            cmnLoadString(habDesktop, hmodNLS, ID_XFSI_ATTR_HIDDEN,
                                    &(pNLSStrings->pszAttrHidden));
                            cmnLoadString(habDesktop, hmodNLS, ID_XFSI_ATTR_READONLY,
                                    &(pNLSStrings->pszAttrReadOnly));

                            cmnLoadString(habDesktop, hmodNLS, ID_XFSI_FLDRSETTINGS,
                                    &(pNLSStrings->pszWarp3FldrView));
                            cmnLoadString(habDesktop, hmodNLS, ID_XFSI_SMALLICONS,
                                    &(pNLSStrings->pszSmallIcons  ));
                            cmnLoadString(habDesktop, hmodNLS, ID_XFSI_FLOWED,
                                    &(pNLSStrings->pszFlowed));
                            cmnLoadString(habDesktop, hmodNLS, ID_XFSI_NONFLOWED,
                                    &(pNLSStrings->pszNonFlowed));
                            cmnLoadString(habDesktop, hmodNLS, ID_XFSI_NOGRID,
                                    &(pNLSStrings->pszNoGrid));

                            // the following are new with V0.9.0
                            cmnLoadString(habDesktop, hmodNLS, ID_XFSI_WARP4MENUBAR,
                                    &(pNLSStrings->pszWarp4MenuBar));
                            cmnLoadString(habDesktop, hmodNLS, ID_XFSI_SHOWSTATUSBAR,
                                    &(pNLSStrings->pszShowStatusBar));

                            // "WPS Class List" (XWPClassList, new with V0.9.0)
                            cmnLoadString(habDesktop, hmodNLS, ID_XFSI_OPENCLASSLIST,
                                    &(pNLSStrings->pszOpenClassList));
                            cmnLoadString(habDesktop, hmodNLS, ID_XFSI_XWPCLASSLIST,
                                    &(pNLSStrings->pszXWPClassList));
                            cmnLoadString(habDesktop, hmodNLS, ID_XFSI_REGISTERCLASS,
                                    &(pNLSStrings->pszRegisterClass));

                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_SOUNDSCHEMENONE,
                                    &(pNLSStrings->pszSoundSchemeNone));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_ITEMSSELECTED,
                                    &(pNLSStrings->pszItemsSelected));

                            // Trash can (XWPTrashCan, XWPTrashObject, new with V0.9.0)
                            cmnLoadString(habDesktop, hmodNLS, ID_XTSI_TRASHEMPTY,
                                    &(pNLSStrings->pszTrashEmpty));
                            cmnLoadString(habDesktop, hmodNLS, ID_XTSI_TRASHRESTORE,
                                    &(pNLSStrings->pszTrashRestore));
                            cmnLoadString(habDesktop, hmodNLS, ID_XTSI_TRASHDESTROY,
                                    &(pNLSStrings->pszTrashDestroy));
                            cmnLoadString(habDesktop, hmodNLS, ID_XTSI_TRASHCAN,
                                    &(pNLSStrings->pszTrashCan));
                            cmnLoadString(habDesktop, hmodNLS, ID_XTSI_TRASHOBJECT,
                                    &(pNLSStrings->pszTrashObject));
                            cmnLoadString(habDesktop, hmodNLS, ID_XTSI_TRASHSETTINGS,
                                    &(pNLSStrings->pszTrashSettings));
                            cmnLoadString(habDesktop, hmodNLS, ID_XTSI_ORIGFOLDER,
                                    &(pNLSStrings->pszOrigFolder));
                            cmnLoadString(habDesktop, hmodNLS, ID_XTSI_DELDATE,
                                    &(pNLSStrings->pszDelDate));
                            cmnLoadString(habDesktop, hmodNLS, ID_XTSI_DELTIME,
                                    &(pNLSStrings->pszDelTime));
                            cmnLoadString(habDesktop, hmodNLS, ID_XTSI_SIZE,
                                    &(pNLSStrings->pszSize));

                            // Details view columns on XWPKeyboard "Hotkeys" page; V0.9.1 (99-12-03)
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_HOTKEY_TITLE,
                                    &(pNLSStrings->pszHotkeyTitle));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_HOTKEY_FOLDER,
                                    &(pNLSStrings->pszHotkeyFolder));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_HOTKEY_HANDLE,
                                    &(pNLSStrings->pszHotkeyHandle));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_HOTKEY_HOTKEY,
                                    &(pNLSStrings->pszHotkeyHotkey));

                            // Method info columns for XWPClassList; V0.9.1 (99-12-03)
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_CLSLIST_INDEX,
                                    &(pNLSStrings->pszClsListIndex));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_CLSLIST_METHOD,
                                    &(pNLSStrings->pszClsListMethod));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_CLSLIST_ADDRESS,
                                    &(pNLSStrings->pszClsListAddress));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_CLSLIST_CLASS,
                                    &(pNLSStrings->pszClsListClass));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_CLSLIST_OVERRIDDENBY,
                                    &(pNLSStrings->pszClsListOverriddenBy));

                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_SPECIAL_WINDOWLIST,
                                    &(pNLSStrings->pszSpecialWindowList));
                            cmnLoadString(habDesktop, hmodNLS, ID_XSSI_SPECIAL_DESKTOPPOPUP,
                                    &(pNLSStrings->pszSpecialDesktopPopup));
                        }

                        // after all this, unload the old resource module
                        DosFreeModule(hmodOldResource);

                    } // end else

                }
            }
        }
        CATCH(excpt1) { } END_CATCH();

        cmnUnlock();
    }

    // return (new?) module handle
    return (hmodNLS);
}

/*
 *@@ cmnQueryNLSStrings:
 *      returns pointer to global NLSSTRINGS structure which contains
 *      all the language-dependent XFolder strings from the resource
 *      files.
 *
 *@@changed V0.9.0 (99-11-14) [umoeller]: made this reentrant, finally
 */

PNLSSTRINGS cmnQueryNLSStrings(VOID)
{
    if (cmnLock(5000))
    {
        TRY_LOUD(excpt1, cmnOnKillDuringLock)
        {
            if (pNLSStringsGlobal == NULL)
            {
                pNLSStringsGlobal = malloc(sizeof(NLSSTRINGS));
                memset(pNLSStringsGlobal, 0, sizeof(NLSSTRINGS));
            }
        }
        CATCH(excpt1) { } END_CATCH();

        cmnUnlock();
    }
    return (pNLSStringsGlobal);
}

/*
 *@@ cmnDescribeKey:
 *      this stores a description of a certain
 *      key into pszBuf, using the NLS DLL strings.
 *      usFlags is as in WM_CHAR.
 *      If (usFlags & KC_VIRTUALKEY), usKeyCode must
 *      be usvk of WM_CHAR (VK_* code), or usch otherwise.
 *      Returns TRUE if this was a valid key combo.
 */

BOOL cmnDescribeKey(PSZ pszBuf,
                    USHORT usFlags,
                    USHORT usKeyCode)
{
    BOOL brc = TRUE;

    PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();

    *pszBuf = 0;
    if (usFlags & KC_CTRL)
        strcpy(pszBuf, pNLSStrings->pszCtrl);
    if (usFlags & KC_SHIFT)
        strcat(pszBuf, pNLSStrings->pszShift);
    if (usFlags & KC_ALT)
        strcat(pszBuf, pNLSStrings->pszAlt);

    if (usFlags & KC_VIRTUALKEY)
    {
        switch (usKeyCode)
        {
            case VK_BACKSPACE: strcat(pszBuf, pNLSStrings->pszBackspace); break;
            case VK_TAB: strcat(pszBuf, pNLSStrings->pszTab); break;
            case VK_BACKTAB: strcat(pszBuf, pNLSStrings->pszBacktab); break;
            case VK_NEWLINE: strcat(pszBuf, pNLSStrings->pszEnter); break;
            case VK_ESC: strcat(pszBuf, pNLSStrings->pszEsc); break;
            case VK_SPACE: strcat(pszBuf, pNLSStrings->pszSpace); break;
            case VK_PAGEUP: strcat(pszBuf, pNLSStrings->pszPageup); break;
            case VK_PAGEDOWN: strcat(pszBuf, pNLSStrings->pszPagedown); break;
            case VK_END: strcat(pszBuf, pNLSStrings->pszEnd); break;
            case VK_HOME: strcat(pszBuf, pNLSStrings->pszHome); break;
            case VK_LEFT: strcat(pszBuf, pNLSStrings->pszLeft); break;
            case VK_UP: strcat(pszBuf, pNLSStrings->pszUp); break;
            case VK_RIGHT: strcat(pszBuf, pNLSStrings->pszRight); break;
            case VK_DOWN: strcat(pszBuf, pNLSStrings->pszDown); break;
            case VK_PRINTSCRN: strcat(pszBuf, pNLSStrings->pszPrintscrn); break;
            case VK_INSERT: strcat(pszBuf, pNLSStrings->pszInsert); break;
            case VK_DELETE: strcat(pszBuf, pNLSStrings->pszDelete); break;
            case VK_SCRLLOCK: strcat(pszBuf, pNLSStrings->pszScrlLock); break;
            case VK_NUMLOCK: strcat(pszBuf, pNLSStrings->pszNumLock); break;
            case VK_ENTER: strcat(pszBuf, pNLSStrings->pszEnter); break;
            case VK_F1: strcat(pszBuf, "F1"); break;
            case VK_F2: strcat(pszBuf, "F2"); break;
            case VK_F3: strcat(pszBuf, "F3"); break;
            case VK_F4: strcat(pszBuf, "F4"); break;
            case VK_F5: strcat(pszBuf, "F5"); break;
            case VK_F6: strcat(pszBuf, "F6"); break;
            case VK_F7: strcat(pszBuf, "F7"); break;
            case VK_F8: strcat(pszBuf, "F8"); break;
            case VK_F9: strcat(pszBuf, "F9"); break;
            case VK_F10: strcat(pszBuf, "F10"); break;
            case VK_F11: strcat(pszBuf, "F11"); break;
            case VK_F12: strcat(pszBuf, "F12"); break;
            case VK_F13: strcat(pszBuf, "F13"); break;
            case VK_F14: strcat(pszBuf, "F14"); break;
            case VK_F15: strcat(pszBuf, "F15"); break;
            case VK_F16: strcat(pszBuf, "F16"); break;
            case VK_F17: strcat(pszBuf, "F17"); break;
            case VK_F18: strcat(pszBuf, "F18"); break;
            case VK_F19: strcat(pszBuf, "F19"); break;
            case VK_F20: strcat(pszBuf, "F20"); break;
            case VK_F21: strcat(pszBuf, "F21"); break;
            case VK_F22: strcat(pszBuf, "F22"); break;
            case VK_F23: strcat(pszBuf, "F23"); break;
            case VK_F24: strcat(pszBuf, "F24"); break;
            default: brc = FALSE; break;
        }
    } // end if (usFlags & KC_VIRTUALKEY)
    else
    {
        switch (usKeyCode)
        {
            case 0xEC00: strcat(pszBuf, pNLSStrings->pszWinLeft); break;
            case 0xED00: strcat(pszBuf, pNLSStrings->pszWinRight); break;
            case 0xEE00: strcat(pszBuf, pNLSStrings->pszWinMenu); break;
            default:
            {
                CHAR szTemp[2];
                if (usKeyCode >= 'a')
                    szTemp[0] = (CHAR)usKeyCode-32;
                else
                    szTemp[0] = (CHAR)usKeyCode;
                szTemp[1] = '\0';
                strcat(pszBuf, szTemp);
            }
        }
    }

    #ifdef DEBUG_KEYS
        _Pmpf(("Key: %s, usKeyCode: 0x%lX, usFlags: 0x%lX", pszBuf, usKeyCode, usFlags));
    #endif

    return (brc);
}

/* ******************************************************************
 *                                                                  *
 *   XFolder Global Settings                                        *
 *                                                                  *
 ********************************************************************/

/*
 *@@ cmnQueryStatusBarSetting:
 *      returns a PSZ to a certain status bar setting, which
 *      may be:
 *      --      SBS_STATUSBARFONT       font (e.g. "9.WarpSans")
 *      --      SBS_TEXTNONESEL         mnemonics for no-object mode
 *      --      SBS_TEXTMULTISEL        mnemonics for multi-object mode
 *
 *      Note that there is no key for querying the mnemonics for
 *      one-object mode, because this is handled by the functions
 *      in statbars.c to provide different data depending on the
 *      class of the selected object.
 *
 *@@changed V0.9.0 (99-11-14) [umoeller]: made this reentrant, finally
 */

const char* cmnQueryStatusBarSetting(USHORT usSetting)
{
    const char *rc = 0;

    if (cmnLock(5000))
    {
        TRY_LOUD(excpt1, cmnOnKillDuringLock)
        {
            switch (usSetting)
            {
                case SBS_STATUSBARFONT:
                        rc = szStatusBarFont;
                    break;
                case SBS_TEXTNONESEL:
                        rc = szSBTextNoneSel;
                    break;
                case SBS_TEXTMULTISEL:
                        rc = szSBTextMultiSel;
            }
        }
        CATCH(excpt1) { } END_CATCH();

        cmnUnlock();
    }

    return (rc);
}

/*
 *@@ cmnSetStatusBarSetting:
 *      sets usSetting to pszSetting. If pszSetting == NULL, the
 *      default value will be loaded from the XFolder NLS DLL.
 *      usSetting works just like in cmnQueryStatusBarSetting.
 *
 *@@changed V0.9.0 (99-11-14) [umoeller]: made this reentrant, finally
 */

BOOL cmnSetStatusBarSetting(USHORT usSetting, PSZ pszSetting)
{
    BOOL    brc = FALSE;

    if (cmnLock(5000))
    {
        TRY_LOUD(excpt1, cmnOnKillDuringLock)
        {
            HAB     habDesktop = WinQueryAnchorBlock(HWND_DESKTOP);
            HMODULE hmodResource = cmnQueryNLSModuleHandle(FALSE);

            brc = TRUE;

            switch (usSetting)
            {
                case SBS_STATUSBARFONT:
                {
                        CHAR szDummy[CCHMAXMNEMONICS];
                        if (pszSetting)
                        {
                            strcpy(szStatusBarFont, pszSetting);
                            PrfWriteProfileString(HINI_USERPROFILE, INIAPP_XWORKPLACE, INIKEY_STATUSBARFONT,
                                     szStatusBarFont);
                        }
                        else
                            strcpy(szStatusBarFont, "8.Helv");
                        sscanf(szStatusBarFont, "%d.%s", &(ulStatusBarHeight), &szDummy);
                        ulStatusBarHeight += 15;
                break; }

                case SBS_TEXTNONESEL:
                {
                        if (pszSetting)
                        {
                            strcpy(szSBTextNoneSel, pszSetting);
                            PrfWriteProfileString(HINI_USERPROFILE, INIAPP_XWORKPLACE, INIKEY_SBTEXTNONESEL,
                                     szSBTextNoneSel);
                        }
                        else
                            WinLoadString(habDesktop, hmodResource, ID_XSSI_SBTEXTNONESEL,
                                sizeof(szSBTextNoneSel), szSBTextNoneSel);
                break; }

                case SBS_TEXTMULTISEL:
                {
                        if (pszSetting)
                        {
                            strcpy(szSBTextMultiSel, pszSetting);
                            PrfWriteProfileString(HINI_USERPROFILE, INIAPP_XWORKPLACE, INIKEY_SBTEXTMULTISEL,
                                     szSBTextMultiSel);
                        }
                        else
                            WinLoadString(habDesktop, hmodResource, ID_XSSI_SBTEXTMULTISEL,
                                sizeof(szSBTextMultiSel), szSBTextMultiSel);
                break; }

                default:
                    brc = FALSE;

            } // end switch(usSetting)
        }
        CATCH(excpt1) { } END_CATCH();

        cmnUnlock();
    }

    return (brc);
}

/*
 *@@ cmnQueryStatusBarHeight:
 *      returns the height of the status bars according to the
 *      current settings in pixels. This was calculated when
 *      the status bar font was set.
 */

ULONG cmnQueryStatusBarHeight(VOID)
{
    return (ulStatusBarHeight);
}

/*
 *@@ cmnLoadGlobalSettings:
 *      this loads the Global Settings from the INI files; should
 *      not be called directly, because this is done automatically
 *      by cmnQueryGlobalSettings, if necessary.
 *
 *      Before loading the settings, all settings are initialized
 *      in case the settings in OS2.INI do not contain all the
 *      settings for this XWorkplace version. This allows for
 *      compatibility with older versions, including XFolder versions.
 *
 *      If (fResetDefaults == TRUE), this only resets all settings
 *      to the default values without loading them from OS2.INI.
 *      This does _not_ write the default settings back to OS2.INI;
 *      if this is desired, call cmnStoreGlobalSettings afterwards.
 *
 *@@changed V0.9.0 [umoeller]: added fResetDefaults to prototype
 *@@changed V0.9.0 [umoeller]: changed initializations for new settings pages
 *@@changed V0.9.0 (99-11-14) [umoeller]: made this reentrant, finally
 */

PCGLOBALSETTINGS cmnLoadGlobalSettings(BOOL fResetDefaults)
{
    if (cmnLock(5000))
    {
        TRY_LOUD(excpt1, cmnOnKillDuringLock)
        {
            ULONG       ulCopied1;

            if (pGlobalSettings == NULL)
            {
                pGlobalSettings = malloc(sizeof(GLOBALSETTINGS));
                memset(pGlobalSettings, 0, sizeof(GLOBALSETTINGS));
            }

            // first set default settings for each settings page;
            // we only load the "real" settings from OS2.INI afterwards
            // because the user might have updated XFolder, and the
            // settings struct in the INIs might be too short
            cmnSetDefaultSettings(SP_1GENERIC              );
            cmnSetDefaultSettings(SP_2REMOVEITEMS          );
            cmnSetDefaultSettings(SP_25ADDITEMS            );
            cmnSetDefaultSettings(SP_26CONFIGITEMS         );
            cmnSetDefaultSettings(SP_27STATUSBAR           );
            cmnSetDefaultSettings(SP_3SNAPTOGRID           );
            cmnSetDefaultSettings(SP_4ACCELERATORS         );
            // cmnSetDefaultSettings(SP_5INTERNALS            );  removed V0.9.0
            // cmnSetDefaultSettings(SP_DTP2                  );  removed V0.9.0
            cmnSetDefaultSettings(SP_FLDRSORT_GLOBAL       );
            // cmnSetDefaultSettings(SP_FILEOPS               );  removed V0.9.0

            // the following are new with V0.9.0
            cmnSetDefaultSettings(SP_SETUP_INFO            );
            cmnSetDefaultSettings(SP_SETUP_FEATURES        );
            cmnSetDefaultSettings(SP_SETUP_PARANOIA        );

            cmnSetDefaultSettings(SP_DTP_MENUITEMS         );
            cmnSetDefaultSettings(SP_DTP_STARTUP           );
            cmnSetDefaultSettings(SP_DTP_SHUTDOWN          );

            cmnSetDefaultSettings(SP_STARTUPFOLDER         );

            cmnSetDefaultSettings(SP_TRASHCAN_SETTINGS);

            cmnSetDefaultSettings(SP_MOUSEHOOK);

            // reset help panels
            pGlobalSettings->ulIntroHelpShown = 0;

            if (fResetDefaults == FALSE)
            {
                // get global XFolder settings from OS2.INI
                PrfQueryProfileString(HINI_USERPROFILE, INIAPP_XWORKPLACE, INIKEY_STATUSBARFONT,
                         "8.Helv", &(szStatusBarFont), sizeof(szStatusBarFont));
                sscanf(szStatusBarFont, "%d.*%s", &(ulStatusBarHeight));
                ulStatusBarHeight += 15;

                PrfQueryProfileString(HINI_USERPROFILE, INIAPP_XWORKPLACE, INIKEY_SBTEXTNONESEL,
                            NULL, &(szSBTextNoneSel), sizeof(szSBTextNoneSel));
                PrfQueryProfileString(HINI_USERPROFILE, INIAPP_XWORKPLACE, INIKEY_SBTEXTMULTISEL,
                            NULL, &(szSBTextMultiSel), sizeof(szSBTextMultiSel));

                ulCopied1 = sizeof(GLOBALSETTINGS);
                PrfQueryProfileData(HINI_USERPROFILE,
                                    INIAPP_XWORKPLACE,
                                    INIKEY_GLOBALSETTINGS,
                                    pGlobalSettings, &ulCopied1);
            }

        }
        CATCH(excpt1) { } END_CATCH();

        cmnUnlock();
    }

    return (pGlobalSettings);
}

/*
 *@@ cmnQueryGlobalSettings:
 *      returns pointer to the GLOBALSETTINGS structure which
 *      contains the XWorkplace Global Settings valid for all
 *      classes. Loads the settings from the INI files if this
 *      hasn't been done yet.
 *
 *      This is used all the time throughout XWorkplace.
 *
 *      NOTE (UM 99-11-14): This now returns a const pointer
 *      to the global settings, because the settings are
 *      unprotected after this call. Never make changes to the
 *      global settings using the return value of this function;
 *      use cmnLockGlobalSettings instead.
 *
 *@@changed V0.9.0 (99-11-14) [umoeller]: made this reentrant, finally; now returning a const pointer only
 */

const GLOBALSETTINGS* cmnQueryGlobalSettings(VOID)
{
    if (cmnLock(5000))
    {
        TRY_LOUD(excpt1, cmnOnKillDuringLock)
        {
            if (pGlobalSettings == NULL)
                cmnLoadGlobalSettings(FALSE);       // load from INI
        }
        CATCH(excpt1) { } END_CATCH();

        cmnUnlock();
    }
    return (pGlobalSettings);
}

/*
 *@@ cmnLockGlobalSettings:
 *      this returns a non-const pointer to the global
 *      settings and locks them, using a mutex semaphore.
 *
 *      If you use this function, ALWAYS call
 *      cmnUnlockGlobalSettings afterwards, as quickly
 *      as possible, because other threads cannot
 *      access the global settings after this call.
 *
 *@@added V0.9.0 (99-11-14) [umoeller]
 */

GLOBALSETTINGS* cmnLockGlobalSettings(ULONG ulTimeout)
{
    if (cmnLock(ulTimeout))
        return (pGlobalSettings);
    else
        return (NULL);
}

/*
 *@@ cmnUnlockGlobalSettings:
 *      antagonist to cmnLockGlobalSettings.
 *
 *@@added V0.9.0 (99-11-14) [umoeller]
 */

VOID cmnUnlockGlobalSettings(VOID)
{
    cmnUnlock();
}

/*
 *@@ cmnStoreGlobalSettings:
 *      stores the current Global Settings back into the INI files;
 *      returns TRUE if successful.
 */

BOOL cmnStoreGlobalSettings(VOID)
{
    xthrPostFileMsg(FIM_STOREGLOBALSETTINGS, 0, 0);
    return (TRUE);
}

/*
 *@@ cmnSetDefaultSettings:
 *      resets those Global Settings which correspond to usSettingsPage
 *      in the System notebook to the default values.
 *
 *      usSettingsPage must be one of the SP_* flags def'd in common.h.
 *      This approach allows to reset the settings to default values
 *      both for a single page (in the various settings notebooks)
 *      and, when this function gets called for each of the settings
 *      pages in cmnLoadGlobalSettings, globally.
 *
 *@@changed V0.9.0 [umoeller]: greatly extended for all the new settings pages
 */

BOOL cmnSetDefaultSettings(USHORT usSettingsPage)
{
    switch(usSettingsPage)
    {
        case SP_1GENERIC:
            // pGlobalSettings->ShowInternals = 1;   // removed V0.9.0
            // pGlobalSettings->ReplIcons = 1;       // removed V0.9.0
            pGlobalSettings->FullPath = 1;
            pGlobalSettings->KeepTitle = 1;
            pGlobalSettings->MaxPathChars = 25;
            pGlobalSettings->TreeViewAutoScroll = 1;
        break;

        case SP_2REMOVEITEMS:
            pGlobalSettings->DefaultMenuItems = 0;
            pGlobalSettings->RemoveLockInPlaceItem = 0;
            pGlobalSettings->RemoveCheckDiskItem = 0;
            pGlobalSettings->RemoveFormatDiskItem = 0;
            pGlobalSettings->RemoveViewMenu = 0;
            pGlobalSettings->RemovePasteItem = 0;
        break;

        case SP_25ADDITEMS:
            pGlobalSettings->FileAttribs = 1;
            pGlobalSettings->AddCopyFilenameItem = 1;
            pGlobalSettings->ExtendFldrViewMenu = 1;
            pGlobalSettings->MoveRefreshNow = (doshIsWarp4() ? 1 : 0);
            pGlobalSettings->AddSelectSomeItem = 1;
            pGlobalSettings->AddFolderContentItem = 1;
            pGlobalSettings->FCShowIcons = 0;
        break;

        case SP_26CONFIGITEMS:
            pGlobalSettings->MenuCascadeMode = 0;
            pGlobalSettings->RemoveX = 1;
            pGlobalSettings->AppdParam = 1;
            pGlobalSettings->TemplatesOpenSettings = BM_INDETERMINATE;
            pGlobalSettings->TemplatesReposition = 1;
        break;

        case SP_27STATUSBAR:
            pGlobalSettings->fDefaultStatusBarVisibility = 1;       // changed V0.9.0
            pGlobalSettings->SBStyle = (doshIsWarp4() ? SBSTYLE_WARP4MENU : SBSTYLE_WARP3RAISED);
            pGlobalSettings->SBForViews = SBV_ICON | SBV_DETAILS;
            pGlobalSettings->lSBBgndColor = WinQuerySysColor(HWND_DESKTOP, SYSCLR_INACTIVEBORDER, 0);
            pGlobalSettings->lSBTextColor = WinQuerySysColor(HWND_DESKTOP, SYSCLR_OUTPUTTEXT, 0);
            cmnSetStatusBarSetting(SBS_TEXTNONESEL, NULL);
            cmnSetStatusBarSetting(SBS_TEXTMULTISEL, NULL);
            pGlobalSettings->fDereferenceShadows = 1;
        break;

        case SP_3SNAPTOGRID:
            pGlobalSettings->fAddSnapToGridDefault = 1;
            pGlobalSettings->GridX = 15;
            pGlobalSettings->GridY = 10;
            pGlobalSettings->GridCX = 20;
            pGlobalSettings->GridCY = 35;
        break;

        case SP_4ACCELERATORS:
            pGlobalSettings->fFolderHotkeysDefault = 1;
        break;

        case SP_FLDRSORT_GLOBAL:
            // pGlobalSettings->ReplaceSort = 0;        removed V0.9.0
            pGlobalSettings->DefaultSort = SV_NAME;
            pGlobalSettings->AlwaysSort = 0;
        break;

        case SP_DTP_MENUITEMS:  // extra Desktop page
            pGlobalSettings->fDTMSort = 1;
            pGlobalSettings->fDTMArrange = 1;
            pGlobalSettings->fDTMSystemSetup = 1;
            pGlobalSettings->fDTMLockup = 1;
            pGlobalSettings->fDTMShutdown = 1;
            pGlobalSettings->fDTMShutdownMenu = 1;
        break;

        case SP_DTP_STARTUP:
            pGlobalSettings->ShowBootupStatus = 0;
            pGlobalSettings->BootLogo = 0;
            pGlobalSettings->bBootLogoStyle = 0;
        break;

        case SP_DTP_SHUTDOWN:
            pGlobalSettings->ulXShutdownFlags = // changed V0.9.0
                XSD_WPS_CLOSEWINDOWS | XSD_CONFIRM | XSD_REBOOT | XSD_ANIMATE;
        break;

        case SP_DTP_ARCHIVES:  // all new with V0.9.0
            // no settings here, these are set elsewhere
        break;

        case SP_SETUP_FEATURES:   // all new with V0.9.0
            pGlobalSettings->fReplaceIcons = 0;
            pGlobalSettings->AddObjectPage = 0;
            pGlobalSettings->fReplaceFilePage = 0;
            pGlobalSettings->fXSystemSounds = 0;

            pGlobalSettings->fEnableStatusBars = 0;
            pGlobalSettings->fEnableSnap2Grid = 0;
            pGlobalSettings->fEnableFolderHotkeys = 0;
            pGlobalSettings->ExtFolderSort = 0;

            pGlobalSettings->fAniMouse = 0;
            pGlobalSettings->fEnableXWPHook = 0;
            pGlobalSettings->fNumLockStartup = 0;

            pGlobalSettings->fMonitorCDRoms = 0;
            pGlobalSettings->fRestartWPS = 0;
            pGlobalSettings->fXShutdown = 0;
            pGlobalSettings->fReplaceArchiving = 0;

            pGlobalSettings->fExtAssocs = 0;
            pGlobalSettings->CleanupINIs = 0;
            pGlobalSettings->fReplFileExists = 0;
            pGlobalSettings->fReplDriveNotReady = 0;
            pGlobalSettings->fTrashDelete = 0;
        break;

        case SP_SETUP_PARANOIA:   // all new with V0.9.0
            pGlobalSettings->VarMenuOffset   = 700;     // raised (V0.9.0)
            pGlobalSettings->NoSubclassing   = 0;
            pGlobalSettings->NoWorkerThread  = 0;
            pGlobalSettings->fUse8HelvFont   = (!doshIsWarp4());
            pGlobalSettings->fNoExcptBeeps    = 0;
            pGlobalSettings->bDefaultWorkerThreadPriority = 1;  // idle +31
            pGlobalSettings->fWorkerPriorityBeep = 0;
        break;

        case SP_STARTUPFOLDER:        // all new with V0.9.0
            pGlobalSettings->ShowStartupProgress = 1;
            pGlobalSettings->ulStartupDelay = 1000;
        break;

        case SP_TRASHCAN_SETTINGS:             // all new with V0.9.0
            pGlobalSettings->fTrashDelete = 0;
            pGlobalSettings->fTrashEmptyStartup = 0;
            pGlobalSettings->fTrashEmptyShutdown = 0;
            pGlobalSettings->fTrashConfirmEmpty = 1;
        break;
    }

    return (TRUE);
}

/* ******************************************************************
 *                                                                  *
 *   Miscellaneae                                                   *
 *                                                                  *
 ********************************************************************/

/*
 *@@ cmnQueryDefaultFont:
 *      this returns the font to be used for dialogs.
 *      If the "Use 8.Helv" checkbox is enabled on
 *      the "Paranoia" page, we return "8.Helv",
 *      otherwise "9.WarpSans" (which are static values).
 *
 *@@added V0.9.0 [umoeller]
 */

const char* cmnQueryDefaultFont(VOID)
{
    if (pGlobalSettings->fUse8HelvFont)
        return ("8.Helv");
    else
        return ("9.WarpSans");
}

/*
 *@@ cmnSetControlsFont:
 *      this sets the font presentation parameters for a dialog
 *      window. See winhSetControlsFont for the parameters.
 *      This calls cmnQueryDefaultFont in turn.
 *
 *@@added V0.9.0 [umoeller]
 */

VOID cmnSetControlsFont(HWND hwnd,
                        SHORT usIDMin,
                        SHORT usIDMax)
{
    cmnLock(5000);
    winhSetControlsFont(hwnd, usIDMin, usIDMax,
                        (PSZ)cmnQueryDefaultFont());
    cmnUnlock();
}

PFNWP pfnwpOrigStatic = NULL;

/*
 *@@ fnwpAutoSizeStatic:
 *      dlg proc for the subclassed static text control in msg box;
 *      automatically resizes the msg box window when text changes.
 *
 *@@changed V0.9.0 [umoeller]: the default font is now queried using cmnQueryDefaultFont
 */

MRESULT EXPENTRY fnwpAutoSizeStatic(HWND hwndStatic, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = NULL;
    switch (msg)
    {

        /*
         * WM_SETLONGTEXT:
         *      this arrives here when the window text changes,
         *      especially when fnwpMessageBox sets the window
         *      text; we will now reposition all the controls.
         *      mp1 is a PSZ to the new text.
         */

        case WM_SETLONGTEXT:
        {
            CHAR szFont[300];
            PSZ p = (PSZ)WinQueryWindowULong(hwndStatic, QWL_USER);
            if (p)
                free(p);
            p = strdup((PSZ)mp1);
            WinSetWindowULong(hwndStatic, QWL_USER, (ULONG)p);
            PrfQueryProfileString(HINI_USER, INIAPP_XWORKPLACE, INIKEY_DLGFONT,
                                  (PSZ)cmnQueryDefaultFont(),
                                  szFont, sizeof(szFont)-1);
            WinSetPresParam(hwndStatic, PP_FONTNAMESIZE,
                 (ULONG)strlen(szFont) + 1, (PVOID)szFont);
            // this will also update the display
        break; }

        /*
         * WM_PRESPARAMCHANGED:
         *      if a font has been dropped on the window, store
         *      the information and enforce control repositioning
         *      also
         */

        case WM_PRESPARAMCHANGED:
        {
            // _Pmpf(( "PRESPARAMCHANGED" ));
            switch ((ULONG)mp1)
            {
                case PP_FONTNAMESIZE:
                {
                    ULONG attrFound; //, abValue[32];
                    CHAR  szFont[100];
                    HWND  hwndDlg = WinQueryWindow(hwndStatic, QW_PARENT);
                    WinQueryPresParam(hwndStatic,
                                PP_FONTNAMESIZE,
                                0,
                                &attrFound,
                                (ULONG)sizeof(szFont),
                                (PVOID)&szFont,
                                0);
                    PrfWriteProfileString(HINI_USER, INIAPP_XWORKPLACE, INIKEY_DLGFONT,
                        szFont);

                    // now also change the buttons
                    WinSetPresParam(WinWindowFromID(hwndDlg, 1),
                        PP_FONTNAMESIZE,
                        (ULONG)strlen(szFont) + 1, (PVOID)szFont);
                    WinSetPresParam(WinWindowFromID(hwndDlg, 2),
                        PP_FONTNAMESIZE,
                        (ULONG)strlen(szFont) + 1, (PVOID)szFont);
                    WinSetPresParam(WinWindowFromID(hwndDlg, 3),
                        PP_FONTNAMESIZE,
                        (ULONG)strlen(szFont) + 1, (PVOID)szFont);

                    WinPostMsg(hwndStatic, WM_UPDATE, MPNULL, MPNULL);
                break; }
            }
        break; }

        /*
         * WM_UPDATE:
         *      this actually does all the calculations to reposition
         *      all the msg box controls
         */

        case WM_UPDATE:
        {
            RECTL   rcl, rcl2;
            SWP     swp;
            HWND    hwndIcon, hwndDlg;
            PSZ     pszText;

            HPS hps = WinGetPS(hwndStatic);

            // _Pmpf(( "WM_UPDATE" ));

            pszText = (PSZ)WinQueryWindowULong(hwndStatic, QWL_USER);
            if (pszText)
            {
                WinQueryWindowRect(hwndStatic, &rcl);         // get window dimensions
                // rcl.yTop = 1000;
                memcpy(&rcl2, &rcl, sizeof(rcl));

                winhDrawFormattedText(hps,
                                      &rcl,
                                      pszText,
                                      DT_LEFT | DT_TOP | DT_QUERYEXTENT);
                WinEndPaint(hps);

                if ((rcl.yTop - rcl.yBottom) < 40)
                    rcl.yBottom -= 40 - (rcl.yTop - rcl.yBottom);

                // printfRtl("  Original rect:", &rcl2);
                // printfRtl("  New rect:     ", &rcl);

                // reposition the text
                WinQueryWindowPos(hwndStatic, &swp);
                swp.cy -= rcl.yBottom;
                WinSetWindowPos(hwndStatic, 0,
                    swp.x, swp.y, swp.cx, swp.cy,
                    SWP_SIZE);

                // reposition the icon
                hwndIcon = WinWindowFromID(WinQueryWindow(hwndStatic, QW_PARENT),
                                ID_XFDI_GENERICDLGICON);
                WinQueryWindowPos(hwndIcon, &swp);
                swp.y -= rcl.yBottom;
                WinSetWindowPos(hwndIcon, 0,
                    swp.x, swp.y, swp.cx, swp.cy,
                    SWP_MOVE);

                // resize the dlg frame window
                hwndDlg = WinQueryWindow(hwndStatic, QW_PARENT);
                WinQueryWindowPos(hwndDlg, &swp);
                swp.cy -= rcl.yBottom;
                swp.y  += (rcl.yBottom / 2);
                WinInvalidateRect(hwndDlg, NULL, FALSE);
                WinSetWindowPos(hwndDlg, 0,
                    swp.x, swp.y, swp.cx, swp.cy,
                    SWP_SIZE | SWP_MOVE);
            }
        break; }

        /*
         * WM_PAINT:
         *      draw the formatted text
         */

        case WM_PAINT:
        {
            RECTL   rcl;
            PSZ pszText = (PSZ)WinQueryWindowULong(hwndStatic, QWL_USER);
            HPS hps = WinBeginPaint(hwndStatic, NULLHANDLE, &rcl);
            if (pszText)
            {
                WinQueryWindowRect(hwndStatic, &rcl);         // get window dimensions
                // switch to RGB mode
                GpiCreateLogColorTable(hps, 0, LCOLF_RGB, 0, 0, NULL);
                // set "static text" color
                GpiSetColor(hps,
                        WinQuerySysColor(HWND_DESKTOP, SYSCLR_WINDOWSTATICTEXT, 0));
                winhDrawFormattedText(hps, &rcl, pszText, DT_LEFT | DT_TOP);
            }
            WinEndPaint(hps);
        break; }

        case WM_DESTROY:
        {
            PSZ pszText = (PSZ)WinQueryWindowULong(hwndStatic, QWL_USER);
            free(pszText);
            mrc = (*pfnwpOrigStatic)(hwndStatic, msg, mp1, mp2);
        break; }
        default:
            mrc = (*pfnwpOrigStatic)(hwndStatic, msg, mp1, mp2);
    }
    return (mrc);
}

/*
 *@@ fnwpMessageBox:
 *      dlg proc for cmnMessageBox below. This subclasses
 *      the static text control with fnwpAutoSizeStatic.
 */

MRESULT EXPENTRY fnwpMessageBox(HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = NULL;
    switch (msg)
    {
        case WM_INITDLG:
        {
            // CHAR szFont[CCHMAXPATH];
            HWND hwndStatic = WinWindowFromID(hwndDlg,
                                              ID_XFDI_GENERICDLGTEXT);

            /* PrfQueryProfileString(HINI_USER, INIAPP_XWORKPLACE, INIKEY_DLGFONT,
                    "9.WarpSans", szFont, sizeof(szFont)-1);
            WinSetPresParam(hwndStatic, PP_FONTNAMESIZE,
                 (ULONG)strlen(szFont)+1, (PVOID)szFont); */

            // set string to 0
            WinSetWindowULong(hwndStatic, QWL_USER, (ULONG)NULL);
            pfnwpOrigStatic = WinSubclassWindow(hwndStatic,
                    fnwpAutoSizeStatic);
            mrc = fnwpDlgGeneric(hwndDlg, msg, mp1, mp2);
        break; }

        default:
            mrc = fnwpDlgGeneric(hwndDlg, msg, mp1, mp2);
        break;
    }
    return (mrc);
}

/*
 *@@ cmnMessageBox:
 *      this is the generic function for displaying XFolder
 *      message boxes. This is very similar to WinMessageBox,
 *      but a few new features are introduced:
 *      -- an XFolder icon is displayed;
 *      -- fonts can be dropped on the window, upon which the
 *         window will resize itself and store the font in OS2.INI.
 *
 *      Returns MBID_* codes like WinMessageBox.
 *
 *@@changed V0.9.0 [umoeller]: added support for MB_YESNOCANCEL
 *@@changed V0.9.0 [umoeller]: fixed default button bugs
 *@@changed V0.9.0 [umoeller]: added WinAlarm sound support
 */

ULONG cmnMessageBox(HWND hwndOwner,     // in: owner
                    PSZ pszTitle,       // in: msgbox title
                    PSZ pszMessage,     // in: msgbox text
                    ULONG flStyle)      // in: MB_* flags
{
    HAB     hab = WinQueryAnchorBlock(hwndOwner);
    HMODULE hmod = cmnQueryNLSModuleHandle(FALSE);
    HWND    hwndDlg = NULLHANDLE, hwndStatic = NULLHANDLE;
    PSZ     pszButton = NULL;
    ULONG   ulrcDlg,
            ulrc = DID_CANCEL;

    // set our extended exception handler
    TRY_LOUD(excpt1, NULL)
    {
        ULONG flButtons = flStyle & 0xF;        // low nibble contains MB_YESNO etc.
        ULONG ulAlarmFlag = WA_NOTE;            // default sound for WinAlarm
        ULONG ulDefaultButtonID = 1;

        hwndDlg = WinLoadDlg(HWND_DESKTOP, hwndOwner,
                             fnwpMessageBox,
                             hmod,
                             ID_XFD_GENERICDLG,
                             NULL);

        hwndStatic = WinWindowFromID(hwndDlg, ID_XFDI_GENERICDLGTEXT);

        // now work on the three buttons of the dlg template:
        // give them proper titles or hide them
        if (flButtons == MB_YESNO)
        {
            cmnLoadString(hab, hmod, ID_XSSI_DLG_YES, &pszButton);
            WinSetDlgItemText(hwndDlg, 1, pszButton);
            cmnLoadString(hab, hmod, ID_XSSI_DLG_NO, &pszButton);
            WinSetDlgItemText(hwndDlg, 2, pszButton);
            winhShowDlgItem(hwndDlg, 3, FALSE);
            free(pszButton);
        }
        else if (flButtons == MB_YESNOCANCEL)
        {
            cmnLoadString(hab, hmod, ID_XSSI_DLG_YES, &pszButton);
            WinSetDlgItemText(hwndDlg, 1, pszButton);
            cmnLoadString(hab, hmod, ID_XSSI_DLG_NO, &pszButton);
            WinSetDlgItemText(hwndDlg, 2, pszButton);
            cmnLoadString(hab, hmod, ID_XSSI_DLG_CANCEL, &pszButton);
            WinSetDlgItemText(hwndDlg, 3, pszButton);
            free(pszButton);
        }
        else if (flButtons == MB_OK)
        {
            cmnLoadString(hab, hmod, ID_XSSI_DLG_OK, &pszButton);
            WinSetDlgItemText(hwndDlg, 1, pszButton);
            winhShowDlgItem(hwndDlg, 2, FALSE);
            winhShowDlgItem(hwndDlg, 3, FALSE);
            free(pszButton);
        }
        else if (flButtons == MB_CANCEL)
        {
            cmnLoadString(hab, hmod, ID_XSSI_DLG_CANCEL, &pszButton);
            WinSetDlgItemText(hwndDlg, 1, pszButton);
            winhShowDlgItem(hwndDlg, 2, FALSE);
            winhShowDlgItem(hwndDlg, 3, FALSE);
            free(pszButton);
        }
        else if (flButtons == MB_OKCANCEL)
        {
            cmnLoadString(hab, hmod, ID_XSSI_DLG_OK, &pszButton);
            WinSetDlgItemText(hwndDlg, 1, pszButton);
            cmnLoadString(hab, hmod, ID_XSSI_DLG_CANCEL, &pszButton);
            WinSetDlgItemText(hwndDlg, 2, pszButton);
            winhShowDlgItem(hwndDlg, 3, FALSE);
            free(pszButton);
        }
        else if (flButtons == MB_RETRYCANCEL)
        {
            cmnLoadString(hab, hmod, ID_XSSI_DLG_RETRY, &pszButton);
            WinSetDlgItemText(hwndDlg, 1, pszButton);
            cmnLoadString(hab, hmod, ID_XSSI_DLG_CANCEL, &pszButton);
            WinSetDlgItemText(hwndDlg, 2, pszButton);
            winhShowDlgItem(hwndDlg, 3, FALSE);
            free(pszButton);
        }
        else if (flButtons == MB_ABORTRETRYIGNORE)
        {
            cmnLoadString(hab, hmod, ID_XSSI_DLG_ABORT, &pszButton);
            WinSetDlgItemText(hwndDlg, 1, pszButton);
            cmnLoadString(hab, hmod, ID_XSSI_DLG_RETRY, &pszButton);
            WinSetDlgItemText(hwndDlg, 2, pszButton);
            cmnLoadString(hab, hmod, ID_XSSI_DLG_IGNORE, &pszButton);
            WinSetDlgItemText(hwndDlg, 3, pszButton);
            free(pszButton);
        }

        WinSetWindowBits(WinWindowFromID(hwndDlg, 1), QWL_STYLE,
                0,
                BS_DEFAULT);

        if (flStyle & MB_DEFBUTTON2)
            ulDefaultButtonID = 2;
        else if (flStyle & MB_DEFBUTTON3)
            ulDefaultButtonID = 3;

        {
            HWND    hwndDefaultButton = WinWindowFromID(hwndDlg, ulDefaultButtonID);
            WinSetWindowBits(hwndDefaultButton, QWL_STYLE,
                             BS_DEFAULT,
                             BS_DEFAULT);
            WinSetFocus(HWND_DESKTOP, hwndDefaultButton);
        }

        if (flStyle & (MB_ICONHAND | MB_ERROR))
            ulAlarmFlag = WA_ERROR;
        else if (flStyle & (MB_ICONEXCLAMATION | MB_WARNING))
            ulAlarmFlag = WA_WARNING;

        WinSetWindowText(hwndDlg, pszTitle);
        WinSendMsg(hwndStatic, WM_SETLONGTEXT, pszMessage, MPNULL);

        winhCenterWindow(hwndDlg);
        if (flStyle & MB_SYSTEMMODAL)
            WinSetSysModalWindow(HWND_DESKTOP, hwndDlg);
        WinAlarm(HWND_DESKTOP, ulAlarmFlag);
        // go!!!
        ulrcDlg = WinProcessDlg(hwndDlg);
        WinDestroyWindow(hwndDlg);

        if (flButtons == MB_YESNO)
            switch (ulrcDlg)
            {
                case 1:     ulrc = MBID_YES; break;
                default:    ulrc = MBID_NO;  break;
            }
        if (flButtons == MB_YESNOCANCEL)
            switch (ulrcDlg)
            {
                case 1:     ulrc = MBID_YES; break;
                case 2:     ulrc = MBID_NO; break;
                default:    ulrc = MBID_CANCEL;  break;
            }
        else if (flButtons == MB_OK)
            ulrc = MBID_OK;
        else if (flButtons == MB_CANCEL)
            ulrc = MBID_CANCEL;
        else if (flButtons == MB_OKCANCEL)
            switch (ulrcDlg)
            {
                case 1:     ulrc = MBID_OK; break;
                default:    ulrc = MBID_CANCEL;  break;
            }
        else if (flButtons == MB_RETRYCANCEL)
            switch (ulrcDlg)
            {
                case 1:     ulrc = MBID_RETRY; break;
                default:    ulrc = MBID_CANCEL;  break;
            }
        else if (flButtons == MB_ABORTRETRYIGNORE)
            switch (ulrcDlg)
            {
                case 2:     ulrc = MBID_RETRY;  break;
                case 3:     ulrc = MBID_IGNORE; break;
                default:    ulrc = MBID_ABORT; break;
            }
    }
    CATCH(excpt1) { } END_CATCH();

    return (ulrc);
}

/*
 *@@ cmnGetMessage:
 *      like DosGetMessage, but automatically uses the
 *      (NLS) XFolder message file.
 *      The parameters are exactly like with DosGetMessage.
 *
 *      <B>Returns:</B> the error code of tmfGetMessage.
 *
 *@@changed V0.9.0 [umoeller]: changed, this now uses the TMF file format (tmsgfile.c).
 */

APIRET cmnGetMessage(PCHAR *pTable,     // in: replacement PSZ table or NULL
                     ULONG ulTable,     // in: size of that table or 0
                     PSZ pszBuf,        // out: buffer to hold message string
                     ULONG cbBuf,       // in: size of pszBuf
                     ULONG ulMsgNumber) // in: msg number to retrieve
{
    APIRET  arc = NO_ERROR;

    TRY_LOUD(excpt1, NULL)
    {
        const char *pszMessageFile = cmnQueryMessageFile();
        CHAR    szMessageName[40] = "error";
        ULONG   ulReturned;
        /* arc = DosGetMessage(pTable, ulTable,
                            pszBuf, cbBuf,
                            ulMsgNumber, pszMessageFile, &ulReturned); */

        // create string message identifier from ulMsgNumber
        sprintf(szMessageName, "XFL%04d", ulMsgNumber);

        #ifdef DEBUG_LANGCODES
            _Pmpf(("cmnGetMessage %s %s", pszMessageFile, szMessageName));
        #endif

        arc = tmfGetMessage(pTable,
                            ulTable,
                            pszBuf,
                            cbBuf,
                            szMessageName,      // string (!) message identifier
                            (PSZ)pszMessageFile,     // .TMF file
                            &ulReturned);

        #ifdef DEBUG_LANGCODES
            _Pmpf(("  tmfGetMessage rc: %d", arc));
        #endif

        if (arc == NO_ERROR)
        {
            pszBuf[ulReturned] = '\0';

            // remove trailing newlines
            while (TRUE)
            {
                PSZ p = pszBuf + strlen(pszBuf) - 1;
                if (    (*p == '\n')
                     || (*p == '\r')
                   )
                    *p = '\0';
                else
                    break; // while (TRUE)
            }
        }
        else
            sprintf(pszBuf, "Message %s not found in %s",
                            szMessageName, pszMessageFile);
    }
    CATCH(excpt1) { } END_CATCH();

    return (arc);
}

/*
 *@@ cmnMessageBoxMsg:
 *      calls cmnMessageBox, but this one accepts ULONG indices
 *      into the XFolder message file (XFLDRxxx.MSG) instead
 *      of real PSZs. This calls cmnGetMessage for retrieving
 *      the messages, but placeholder replacement does not work
 *      here (use cmnMessageBoxMsgExt for that).
 */

ULONG cmnMessageBoxMsg(HWND hwndOwner,
                       ULONG ulTitle,       // in: msg index for dlg title
                       ULONG ulMessage,     // in: msg index for message
                       ULONG flStyle)       // in: like cmnMsgBox
{
    CHAR    szTitle[200], szMessage[2000];

    cmnGetMessage(NULL, 0,
            szTitle, sizeof(szTitle)-1,
            ulTitle);
    cmnGetMessage(NULL, 0,
            szMessage, sizeof(szMessage)-1,
            ulMessage);

    return (cmnMessageBox(hwndOwner, szTitle, szMessage, flStyle));
}

/*
 *@@ cmnMessageBoxMsgExt:
 *      like cmnMessageBoxMsg, but with string substitution
 *      (see cmnGetMessage for more); substitution only
 *      takes place for the message specified with ulMessage,
 *      not for the title.
 */

ULONG cmnMessageBoxMsgExt(HWND hwndOwner,   // in: owner window
                          ULONG ulTitle,    // in: msg number for title
                          PCHAR *pTable,    // in: replacement table for ulMessage
                          ULONG ulTable,    // in: sizeof *pTable
                          ULONG ulMessage,  // in: msg number for message
                          ULONG flStyle)    // in: msg box style flags (cmnMessageBox)
{
    CHAR    szTitle[200], szMessage[2000];

    cmnGetMessage(NULL, 0,
            szTitle, sizeof(szTitle)-1,
            ulTitle);
    cmnGetMessage(pTable, ulTable,
            szMessage, sizeof(szMessage)-1,
            ulMessage);

    return (cmnMessageBox(hwndOwner, szTitle, szMessage, flStyle));
}

/*
 *@@ cmnSetHelpPanel:
 *      sets help panel before calling fnwpDlgGeneric.
 */

VOID cmnSetHelpPanel(ULONG ulHelpPanel)
{
    ulCurHelpPanel = ulHelpPanel;
}

/*
 *@@  fnwpDlgGeneric:
 *          this is the dlg procedure for XFolder dlg boxes;
 *          it can process WM_HELP messages
 */

MRESULT EXPENTRY fnwpDlgGeneric(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    HOBJECT hobjRef = 0;
    HWND    hwndItem;
    MRESULT mrc = NULL;

    switch (msg)
    {
        case WM_INITDLG:
            if (hwndItem = WinWindowFromID(hwnd, ID_XFDI_XFLDVERSION))
            {
                CHAR    szTemp[100];
                WinQueryWindowText(hwndItem, sizeof(szTemp), szTemp);
                strcpy(strstr(szTemp, "%a"), XFOLDER_VERSION);
                WinSetWindowText(hwndItem, szTemp);
            }
            mrc = WinDefDlgProc(hwnd, msg, mp1, mp2);
        break;

        case WM_HELP:
        {
            // HMODULE hmodResource = cmnQueryNLSModuleHandle(FALSE);
            /* WM_HELP is received by this function when F1 or a "help" button
               is pressed in a dialog window. */
            if (ulCurHelpPanel > 0)
            {
                WPObject    *pHelpSomSelf = _wpclsQueryActiveDesktop(_WPDesktop);
                /* ulCurHelpPanel is set by instance methods before creating a
                   dialog box in order to link help topics to the displayed
                   dialog box. Possible values are:
                        0: open online reference ("<XFOLDER_REF>", INF book)
                      > 0: open help topic in xfldr.hlp
                       -1: ignore WM_HELP */
                if (pHelpSomSelf)
                {
                    const char* pszHelpLibrary;
                    BOOL fProcessed = FALSE;
                    if (pszHelpLibrary = cmnQueryHelpLibrary())
                        // path found: display help panel
                        if (_wpDisplayHelp(pHelpSomSelf, ulCurHelpPanel, (PSZ)pszHelpLibrary))
                            fProcessed = TRUE;

                    if (!fProcessed)
                        cmnMessageBoxMsg(HWND_DESKTOP, 104, 134, MB_OK);
                }
            }
            else if (ulCurHelpPanel == 0)
            {
                // open online reference
                ulCurHelpPanel = -1; // ignore further WM_HELP messages: this one suffices
                hobjRef = WinQueryObject("<XFOLDER_REF>");
                if (hobjRef)
                    WinOpenObject(hobjRef, OPEN_DEFAULT, TRUE);
                else
                    cmnMessageBoxMsg(HWND_DESKTOP, 104, 137, MB_OK);

            } // end else; if ulCurHelpPanel is < 0, nothing happens
            mrc = NULL;
        break; } // end case WM_HELP

        default:
            mrc = WinDefDlgProc(hwnd, msg, mp1, mp2);
        break;
    }

    return (mrc);
}


