
/*
 *@@sourcefile xwpstring.c:
 *      This file contains SOM code for the following XWorkplace classes:
 *
 *      --  XWPMedia: a WPAbstract subclass for multimedia settings.
 *
 *      This class is new with V0.9.3.
 *
 *      Installation of XWPString is completely optional.
 *
 *      Note: Those G_mmio* and G_mci* identifiers are global
 *      variables containing MMPM/2 API entries. Those are
 *      resolved by xmmInit (mmthread.c) and must only be used
 *      after checking xmmQueryStatus.
 *
 *@@added V0.9.3 (2000-04-29) [umoeller]
 *@@somclass XWPMedia xwmm_
 *@@somclass M_XWPMedia xwmmM_
 */

/*
 *      Copyright (C) 2000 Ulrich M�ller.
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
 *  This file was generated by the SOM Compiler and Emitter Framework.
 *  Generated using:
 *      SOM Emitter emitctm: 2.41
 */

#ifndef SOM_Module_xwpmedia_Source
#define SOM_Module_xwpmedia_Source
#endif
#define XWPMedia_Class_Source
#define M_XWPMedia_Class_Source

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
#define INCL_DOSPROCESS
#define INCL_DOSEXCEPTIONS
#define INCL_DOSERRORS

#define INCL_WINWINDOWMGR
#define INCL_WINMESSAGEMGR
#define INCL_WINMENUS           // for menu helpers
#define INCL_WINDIALOGS
#define INCL_WINBUTTONS         // for button/check box helpers
#define INCL_WINSTDCNR
#define INCL_WINMLE

#define INCL_GPI                // required for INCL_MMIO_CODEC
#define INCL_GPIBITMAPS         // required for INCL_MMIO_CODEC
#include <os2.h>

// multimedia includes
#define INCL_MCIOS2
#define INCL_MMIOOS2
#define INCL_MMIO_CODEC
#include <os2me.h>

// C library headers
#include <stdio.h>              // needed for except.h
#include <setjmp.h>

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\cnrh.h"               // container helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\stringh.h"            // string helper routines
#include "helpers\threads.h"            // thread helpers

// XWorkplace implementation headers
#include "media\media.h"                // XWorkplace multimedia support

// SOM headers which don't crash with prec. header files
#include "xwpmedia.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling

// other SOM headers

#pragma hdrstop

/* ******************************************************************
 *                                                                  *
 *   Global variables                                               *
 *                                                                  *
 ********************************************************************/

PTHREADINFO     G_ptiInsertDevices = NULL,
                G_ptiInsertCodecs = NULL;

/* ******************************************************************
 *                                                                  *
 *   Shared helpers                                                 *
 *                                                                  *
 ********************************************************************/

/*
 *@@ CompareStrings:
 *      compares two strings and returns a value
 *      which can be used by container sort procs.
 */

/* SHORT EXPENTRY fnCompareName(PSZ psz1, PSZ psz2)
{
    HAB habDesktop = WinQueryAnchorBlock(HWND_DESKTOP);
    if ((psz1) && (psz2))
        switch (WinCompareStrings(habDesktop, 0, 0,
                                  psz1, psz2, 0))
        {
            case WCS_LT: return (-1);
            case WCS_GT: return (1);
        }

    return (0);
} */

/* ******************************************************************
 *                                                                  *
 *   XWPMedia "Device" page notebook callbacks (notebook.c)         *
 *                                                                  *
 ********************************************************************/

/*
 *@@ MMDEVRECORD:
 *
 *@@added V0.9.3 (2000-04-28) [umoeller]
 */

typedef struct _MMDEVRECORD
{
    RECORDCORE  recc;

    PXMMDEVICE  pDevice;

    PSZ         pszDeviceType;
    ULONG       ulDeviceIndex;
    PSZ         pszInfo;
} MMDEVRECORD, *PMMDEVRECORD;

/*
 *@@ fntInsertDevices:
 *      transient thread started by xwmmDevicesInitPage
 *      to insert devices into the "Devices" container.
 *
 *      This thread is created with a msg queue.
 */

void _Optlink fntInsertDevices(PTHREADINFO pti)
{
    PCREATENOTEBOOKPAGE pcnbp = (PCREATENOTEBOOKPAGE)(pti->ulData);

    TRY_LOUD(excpt1, NULL)
    {
        ULONG       cDevices = 0,
                    ul;
        PXMMDEVICE  paDevices;

        pcnbp->fShowWaitPointer = TRUE;

        paDevices = xmmQueryDevices(&cDevices);

        if (paDevices)
        {
            HWND        hwndCnr = WinWindowFromID(pcnbp->hwndDlgPage, ID_XFDI_CNR_CNR);

            for (ul = 0;
                 ul < cDevices;
                 ul++)
            {
                PMMDEVRECORD precc
                    = (PMMDEVRECORD)cnrhAllocRecords(hwndCnr,
                                                     sizeof(MMDEVRECORD),
                                                     1);
                if (precc)
                {
                    precc->pszDeviceType = paDevices[ul].pszDeviceType;
                    precc->ulDeviceIndex = paDevices[ul].ulDeviceIndex;
                    precc->pszInfo = paDevices[ul].szInfo;
                    precc->pDevice = &paDevices[ul];

                    cnrhInsertRecords(hwndCnr,
                                      NULL,
                                      (PRECORDCORE)precc,
                                      TRUE, // invalidate
                                      NULL,
                                      CRA_RECORDREADONLY,
                                      1);
                }
            }
        }

        // store devices
        if (pcnbp->pUser)
            xmmFreeDevices(paDevices);
        pcnbp->pUser = paDevices;
    }
    CATCH(excpt1) {}  END_CATCH();

    pcnbp->fShowWaitPointer = FALSE;
}

/*
 *@@ xwmmDevicesInitPage:
 *      notebook callback function (notebook.c) for the
 *      XWPMedia "Devices" page.
 *      Sets the controls on the page according to the
 *      Global Settings.
 *
 *@@changed V0.9.4 (2000-06-13) [umoeller]: group title was missing; fixed
 */

VOID xwmmDevicesInitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                         ULONG flFlags)        // CBI_* flags (notebook.h)
{
    if (flFlags & CBI_INIT)
    {
        PNLSSTRINGS     pNLSStrings = cmnQueryNLSStrings();
        HWND hwndCnr = WinWindowFromID(pcnbp->hwndDlgPage, ID_XFDI_CNR_CNR);

        XFIELDINFO      xfi[5];
        PFIELDINFO      pfi = NULL;
        int             i = 0;

        // set group title V0.9.4 (2000-06-13) [umoeller]
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XFDI_CNR_GROUPTITLE,
                          pNLSStrings->pszPagetitleDevices);

        // set up cnr details view
        xfi[i].ulFieldOffset = FIELDOFFSET(MMDEVRECORD, pszDeviceType);
        xfi[i].pszColumnTitle = pNLSStrings->pszDeviceType;
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        xfi[i].ulFieldOffset = FIELDOFFSET(MMDEVRECORD, ulDeviceIndex);
        xfi[i].pszColumnTitle = pNLSStrings->pszDeviceIndex;
        xfi[i].ulDataType = CFA_ULONG;
        xfi[i++].ulOrientation = CFA_CENTER;

        xfi[i].ulFieldOffset = FIELDOFFSET(MMDEVRECORD, pszInfo);
        xfi[i].pszColumnTitle = pNLSStrings->pszDeviceInfo;
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        pfi = cnrhSetFieldInfos(hwndCnr,
                                xfi,
                                i,             // array item count
                                TRUE,          // draw lines
                                1);            // return first column

        BEGIN_CNRINFO()
        {
            cnrhSetView(CV_DETAIL | CA_DETAILSVIEWTITLES);
            cnrhSetSplitBarAfter(pfi);
            cnrhSetSplitBarPos(300);
        } END_CNRINFO(hwndCnr);
    }

    if (flFlags & CBI_SET)
    {
        thrCreate(&G_ptiInsertDevices,
                  fntInsertDevices,
                  NULL, // running flag
                  THRF_PMMSGQUEUE,
                  (ULONG)pcnbp);
    }

    if (flFlags & CBI_DESTROY)
    {
        if (pcnbp->pUser)
            xmmFreeDevices(pcnbp->pUser);
        pcnbp->pUser = NULL;
        thrFree(&G_ptiInsertDevices);
    }
}

/* ******************************************************************
 *                                                                  *
 *   XWPMedia "IOProcs" page notebook callbacks (notebook.c)        *
 *                                                                  *
 ********************************************************************/

/*
 *@@ DescribeMediaType:
 *
 *@@added V0.9.3 (2000-04-29) [umoeller]
 */

VOID DescribeMediaType(PSZ pszBuf,          // out: description
                       ULONG ulMediaType)   // in: MMIO_MEDIATYPE_* id
{
    PNLSSTRINGS     pNLSStrings = cmnQueryNLSStrings();

    switch (ulMediaType)
    {
        case MMIO_MEDIATYPE_IMAGE:
            strcpy(pszBuf, pNLSStrings->pszTypeImage);
        break;

        case MMIO_MEDIATYPE_AUDIO:
            strcpy(pszBuf, pNLSStrings->pszTypeAudio);
        break;

        case MMIO_MEDIATYPE_MIDI:
            strcpy(pszBuf, pNLSStrings->pszTypeMIDI);
        break;

        case MMIO_MEDIATYPE_COMPOUND:
            strcpy(pszBuf, pNLSStrings->pszTypeCompound);
        break;

        case MMIO_MEDIATYPE_OTHER:
            strcpy(pszBuf, pNLSStrings->pszTypeOther);
        break;

        case MMIO_MEDIATYPE_UNKNOWN:
            strcpy(pszBuf, pNLSStrings->pszTypeUnknown);
        break;

        case MMIO_MEDIATYPE_DIGITALVIDEO:
            strcpy(pszBuf, pNLSStrings->pszTypeVideo);
        break;

        case MMIO_MEDIATYPE_ANIMATION:
            strcpy(pszBuf, pNLSStrings->pszTypeAnimation);
        break;

        case MMIO_MEDIATYPE_MOVIE:
            strcpy(pszBuf, pNLSStrings->pszTypeMovie);
        break;

        default:
            sprintf(pszBuf, "unknown (%d)", ulMediaType);
    }
}

/*
 *@@ IOPROCRECORD:
 *
 *@@added V0.9.3 (2000-04-29) [umoeller]
 */

typedef struct _IOPROCRECORD
{
    RECORDCORE  recc;

    ULONG       ulIndex;

    CHAR        szFourCC[5];
    PSZ         pszFourCC;      // points to szFourCC

    CHAR        szFormatName[200];
    PSZ         pszFormatName;  // points to szFormatName

    CHAR        szIOProcType[30];
    PSZ         pszIOProcType;   // points to szIOProcType

    CHAR        szMediaType[30];
    PSZ         pszMediaType;   // points to szMediaType

    CHAR        szExtension[5];
    PSZ         pszExtension;  // szExtension

} IOPROCRECORD, *PIOPROCRECORD;

/*
 *@@ fntInsertIOProcs:
 *      transient thread started by xwmmCodecsInitPage
 *      to insert devices into the "Devices" container.
 *
 *      This thread is created with a msg queue.
 */

void _Optlink fntInsertIOProcs(PTHREADINFO pti)
{
    PCREATENOTEBOOKPAGE pcnbp = (PCREATENOTEBOOKPAGE)(pti->ulData);
    TRY_LOUD(excpt1, NULL)
    {
        HWND        hwndCnr = WinWindowFromID(pcnbp->hwndDlgPage, ID_XFDI_CNR_CNR);

        MMFORMATINFO    mmfi;
        LONG            lFormatCount;

        memset(&mmfi, 0, sizeof(mmfi));     // zeroed struct means get all
        mmfi.ulStructLen = sizeof(mmfi);

        pcnbp->fShowWaitPointer = TRUE;

        if (G_mmioQueryFormatCount(&mmfi,
                                   &lFormatCount,
                                   0,     // reserved
                                   0)     // reserved
                == 0)
        {
            PMMFORMATINFO   pammfi = (PMMFORMATINFO)malloc(lFormatCount
                                                            * sizeof(MMFORMATINFO)
                                                          );
            LONG    lFormatsRead = 0;
            if (G_mmioGetFormats(&mmfi,
                                 lFormatCount,
                                 pammfi,
                                 &lFormatsRead,
                                 0,     // reserved
                                 0)     // reserved
                    == 0)
            {
                ULONG ul;
                PMMFORMATINFO pmmfiThis = pammfi;

                for (ul = 0;
                     ul < lFormatsRead;
                     ul++)
                {
                    PIOPROCRECORD precc
                        = (PIOPROCRECORD)cnrhAllocRecords(hwndCnr,
                                                         sizeof(IOPROCRECORD),
                                                         1);
                    if (precc)
                    {
                        PNLSSTRINGS     pNLSStrings = cmnQueryNLSStrings();
                        PSZ     pszFormatName;
                        // index
                        precc->ulIndex = ul;

                        // FourCC
                        memcpy(&precc->szFourCC, &pmmfiThis->fccIOProc, sizeof(ULONG));
                        precc->szFourCC[4] = 0;
                        precc->pszFourCC = precc->szFourCC;

                        // format name
                        if (pmmfiThis->lNameLength)
                        {
                            pszFormatName = malloc(pmmfiThis->lNameLength + 1); // null term.
                            if (pszFormatName)
                            {
                                LONG lWritten;
                                G_mmioGetFormatName(pmmfiThis,
                                                    pszFormatName,
                                                    &lWritten,
                                                    0,
                                                    0);
                                if (lWritten)
                                    strhncpy0(precc->szFormatName,
                                              pszFormatName,
                                              sizeof(precc->szFormatName) - 1);
                                free(pszFormatName);
                            }
                        }
                        precc->pszFormatName = precc->szFormatName;

                        // IOProc type
                        switch(pmmfiThis->ulIOProcType)
                        {
                            case MMIO_IOPROC_STORAGESYSTEM:
                                strcpy(precc->szIOProcType, pNLSStrings->pszTypeStorage);
                            break;

                            case MMIO_IOPROC_FILEFORMAT:
                                strcpy(precc->szIOProcType, pNLSStrings->pszTypeFile);
                            break;

                            case MMIO_IOPROC_DATAFORMAT:
                                strcpy(precc->szIOProcType, pNLSStrings->pszTypeData);
                            break;

                            default:
                                sprintf(precc->szIOProcType, "unknown (%d)",
                                        pmmfiThis->ulIOProcType);
                        }
                        precc->pszIOProcType = precc->szIOProcType;

                        // media type
                        DescribeMediaType(precc->szMediaType,
                                          pmmfiThis->ulMediaType);
                        precc->pszMediaType = precc->szMediaType;

                        // extension
                        memcpy(&precc->szExtension, &pmmfiThis->szDefaultFormatExt,
                                    5);
                        precc->pszExtension = precc->szExtension;

                        cnrhInsertRecords(hwndCnr,
                                          NULL,
                                          (PRECORDCORE)precc,
                                          TRUE, // invalidate
                                          NULL,
                                          CRA_RECORDREADONLY,
                                          1);
                    }

                    pmmfiThis++;
                }

            }
        }
    }
    CATCH(excpt1) {}  END_CATCH();

    pcnbp->fShowWaitPointer = FALSE;
}

/*
 *@@ xwmmIOProcsInitPage:
 *      notebook callback function (notebook.c) for the
 *      XWPMedia "Codecs" page.
 *      Sets the controls on the page according to the
 *      Global Settings.
 *
 *@@changed V0.9.4 (2000-06-13) [umoeller]: group title was missing; fixed
 */

VOID xwmmIOProcsInitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                        ULONG flFlags)        // CBI_* flags (notebook.h)
{
    if (flFlags & CBI_INIT)
    {
        PNLSSTRINGS     pNLSStrings = cmnQueryNLSStrings();
        HWND hwndCnr = WinWindowFromID(pcnbp->hwndDlgPage, ID_XFDI_CNR_CNR);

        XFIELDINFO      xfi[6];
        PFIELDINFO      pfi = NULL;
        int             i = 0;

        // set group title V0.9.4 (2000-06-13) [umoeller]
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XFDI_CNR_GROUPTITLE,
                          pNLSStrings->pszPagetitleIOProcs);

        // set up cnr details view
        xfi[i].ulFieldOffset = FIELDOFFSET(IOPROCRECORD, ulIndex);
        xfi[i].pszColumnTitle = ""; // Index";
        xfi[i].ulDataType = CFA_ULONG;
        xfi[i++].ulOrientation = CFA_CENTER;

        xfi[i].ulFieldOffset = FIELDOFFSET(IOPROCRECORD, pszFourCC);
        xfi[i].pszColumnTitle = pNLSStrings->pszColmnFourCC;
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        xfi[i].ulFieldOffset = FIELDOFFSET(IOPROCRECORD, pszFormatName);
        xfi[i].pszColumnTitle = pNLSStrings->pszColmnName;
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        xfi[i].ulFieldOffset = FIELDOFFSET(IOPROCRECORD, pszIOProcType);
        xfi[i].pszColumnTitle = pNLSStrings->pszColmnIOProcType;
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        xfi[i].ulFieldOffset = FIELDOFFSET(IOPROCRECORD, pszMediaType);
        xfi[i].pszColumnTitle = pNLSStrings->pszColmnMediaType;
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        xfi[i].ulFieldOffset = FIELDOFFSET(IOPROCRECORD, pszExtension);
        xfi[i].pszColumnTitle = pNLSStrings->pszColmnExtension;
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        pfi = cnrhSetFieldInfos(hwndCnr,
                                xfi,
                                i,             // array item count
                                TRUE,          // draw lines
                                2);            // return third column

        BEGIN_CNRINFO()
        {
            cnrhSetView(CV_DETAIL | CA_DETAILSVIEWTITLES);
            cnrhSetSplitBarAfter(pfi);
            cnrhSetSplitBarPos(300);
        } END_CNRINFO(hwndCnr);
    }

    if (flFlags & CBI_SET)
    {
        thrCreate(&G_ptiInsertCodecs,
                  fntInsertIOProcs,
                  NULL, // running flag
                  THRF_PMMSGQUEUE,
                  (ULONG)pcnbp);
    }
}

/* ******************************************************************
 *                                                                  *
 *   XWPMedia "Codecs" page notebook callbacks (notebook.c)         *
 *                                                                  *
 ********************************************************************/

/*
 *@@ CODECRECORD:
 *
 *@@added V0.9.3 (2000-04-29) [umoeller]
 */

typedef struct _CODECRECORD
{
    RECORDCORE  recc;

    ULONG       ulIndex;

    CHAR        szFourCC[5];
    PSZ         pszFourCC;      // points to szFourCC

    CHAR        szCodecName[200];
    PSZ         pszCodecName;      // points to szCodecName

    CHAR        szMediaType[30];
    PSZ         pszMediaType;   // points to szMediaType

    CHAR        szDLLName[DLLNAME_SIZE];
    PSZ         pszDLLName;     // points to szDLLName

    CHAR        szProcName[PROCNAME_SIZE];
    PSZ         pszProcName;    // points to szProcName

} CODECRECORD, *PCODECRECORD;

/*
 *@@ fntInsertCodecs:
 *      transient thread started by xwmmCodecsInitPage
 *      to insert devices into the "Devices" container.
 *
 *      This thread is created with a msg queue.
 */

void _Optlink fntInsertCodecs(PTHREADINFO pti)
{
    PCREATENOTEBOOKPAGE pcnbp = (PCREATENOTEBOOKPAGE)(pti->ulData);
    TRY_LOUD(excpt1, NULL)
    {
        HWND        hwndCnr = WinWindowFromID(pcnbp->hwndDlgPage, ID_XFDI_CNR_CNR);

        CODECINIFILEINFO    cifi;
        BOOL                fContinue = TRUE;
        ULONG               ulIndex = 0,
                            ulFlags = MMIO_FINDPROC | MMIO_MATCHFIRST;

        pcnbp->fShowWaitPointer = TRUE;

        memset(&cifi, 0, sizeof(cifi));
        cifi.ulStructLen = sizeof(cifi);

        _Pmpf(("Entering DumpCodecs"));

        //
        do
        {
            if (G_mmioIniFileCODEC(&cifi,
                                   ulFlags) // initially MMIO_FINDPROC | MMIO_MATCHFIRST
                        != MMIO_SUCCESS)
                fContinue = FALSE;
            else
            {
                // success:
                PCODECRECORD precc
                    = (PCODECRECORD)cnrhAllocRecords(hwndCnr,
                                                     sizeof(CODECRECORD),
                                                     1);
                if (precc)
                {
                    ULONG   cb = 0;

                    // index
                    precc->ulIndex = ulIndex;

                    // FourCC
                    memcpy(&precc->szFourCC, &cifi.fcc, sizeof(ULONG));
                    precc->szFourCC[4] = 0;
                    precc->pszFourCC = precc->szFourCC;

                    // codec name
                    if (G_mmioQueryCODECNameLength(&cifi,
                                                   &cb)
                            == MMIO_SUCCESS)
                    {
                        PSZ pszCodecName = malloc(cb + 1); // null term.
                        if (pszCodecName)
                        {
                            LONG lWritten;
                            if (G_mmioQueryCODECName(&cifi,
                                                     pszCodecName,
                                                     &cb) // excluding null term.
                                        == MMIO_SUCCESS)
                                strhncpy0(precc->szCodecName,
                                          pszCodecName,
                                          sizeof(precc->szCodecName) - 1);
                            #ifdef __DEBUG__
                            else
                                strcpy(precc->szCodecName, "mmioQueryCODECName failed.");
                            #endif

                            free(pszCodecName);
                        }
                    }
                    #ifdef __DEBUG__
                    else
                        strcpy(precc->szCodecName, "mmioQueryCODECNameLength failed.");
                    #endif

                    precc->pszCodecName = precc->szCodecName;

                    // media type
                    DescribeMediaType(precc->szMediaType,
                                      cifi.ulMediaType);
                    precc->pszMediaType = precc->szMediaType;

                    strhncpy0(precc->szDLLName, cifi.szDLLName, sizeof(precc->szDLLName));
                    precc->pszDLLName = precc->szDLLName;

                    strhncpy0(precc->szProcName, cifi.szProcName, sizeof(precc->szProcName));
                    precc->pszProcName = precc->szProcName;

                    cnrhInsertRecords(hwndCnr,
                                      NULL,
                                      (PRECORDCORE)precc,
                                      TRUE, // invalidate
                                      NULL,
                                      CRA_RECORDREADONLY,
                                      1);
                }
            }

            ulIndex++;
            ulFlags = MMIO_FINDPROC | MMIO_MATCHNEXT;
        } while (fContinue);
    }
    CATCH(excpt1) {}  END_CATCH();

    pcnbp->fShowWaitPointer = FALSE;
}

/*
 *@@ xwmmCodecsInitPage:
 *      notebook callback function (notebook.c) for the
 *      XWPMedia "Codecs" page.
 *      Sets the controls on the page according to the
 *      Global Settings.
 *
 *@@changed V0.9.4 (2000-06-13) [umoeller]: group title was missing; fixed
 */

VOID xwmmCodecsInitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                        ULONG flFlags)        // CBI_* flags (notebook.h)
{
    if (flFlags & CBI_INIT)
    {
        PNLSSTRINGS     pNLSStrings = cmnQueryNLSStrings();
        HWND hwndCnr = WinWindowFromID(pcnbp->hwndDlgPage, ID_XFDI_CNR_CNR);

        XFIELDINFO      xfi[6];
        PFIELDINFO      pfi = NULL;
        int             i = 0;

        // set group title V0.9.4 (2000-06-13) [umoeller]
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XFDI_CNR_GROUPTITLE,
                          pNLSStrings->pszPagetitleCodecs);

        // set up cnr details view
        xfi[i].ulFieldOffset = FIELDOFFSET(CODECRECORD, ulIndex);
        xfi[i].pszColumnTitle = ""; // Index"; // ###
        xfi[i].ulDataType = CFA_ULONG;
        xfi[i++].ulOrientation = CFA_CENTER;

        xfi[i].ulFieldOffset = FIELDOFFSET(CODECRECORD, pszFourCC);
        xfi[i].pszColumnTitle = pNLSStrings->pszColmnFourCC;
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        xfi[i].ulFieldOffset = FIELDOFFSET(CODECRECORD, pszCodecName);
        xfi[i].pszColumnTitle = pNLSStrings->pszColmnName;
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        xfi[i].ulFieldOffset = FIELDOFFSET(CODECRECORD, pszMediaType);
        xfi[i].pszColumnTitle = pNLSStrings->pszColmnMediaType;
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        xfi[i].ulFieldOffset = FIELDOFFSET(CODECRECORD, pszDLLName);
        xfi[i].pszColumnTitle = pNLSStrings->pszColmnDLL;
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        xfi[i].ulFieldOffset = FIELDOFFSET(CODECRECORD, pszProcName);
        xfi[i].pszColumnTitle = pNLSStrings->pszColmnProcedure;
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        pfi = cnrhSetFieldInfos(hwndCnr,
                                xfi,
                                i,             // array item count
                                TRUE,          // draw lines
                                2);            // return third column

        BEGIN_CNRINFO()
        {
            cnrhSetView(CV_DETAIL | CA_DETAILSVIEWTITLES);
            cnrhSetSplitBarAfter(pfi);
            cnrhSetSplitBarPos(300);
        } END_CNRINFO(hwndCnr);
    }

    if (flFlags & CBI_SET)
    {
        thrCreate(&G_ptiInsertCodecs,
                  fntInsertCodecs,
                  NULL, // running flag
                  THRF_PMMSGQUEUE,
                  (ULONG)pcnbp);
    }
}

/* ******************************************************************
 *                                                                  *
 *   XWPMedia Instance Methods                                      *
 *                                                                  *
 ********************************************************************/

SOM_Scope ULONG  SOMLINK xwmm_xwpAddXWPMediaPages(XWPMedia *somSelf,
                                                  HWND hwndDlg)
{
    PCREATENOTEBOOKPAGE pcnbp;
    HMODULE         savehmod = cmnQueryNLSModuleHandle(FALSE);
    PNLSSTRINGS     pNLSStrings = cmnQueryNLSStrings();
    /* XWPMediaData *somThis = XWPMediaGetData(somSelf); */
    XWPMediaMethodDebug("XWPMedia","xwmm_xwpAddXWPMediaPages");

    // insert "Multimedia" page
    if (xmmQueryStatus() == MMSTAT_WORKING)
    {
        // Codecs
        pcnbp = malloc(sizeof(CREATENOTEBOOKPAGE));
        memset(pcnbp, 0, sizeof(CREATENOTEBOOKPAGE));
        pcnbp->somSelf = somSelf;
        pcnbp->hwndNotebook = hwndDlg;
        pcnbp->hmod = savehmod;
        pcnbp->usPageStyleFlags = BKA_MAJOR;
        pcnbp->pszName = pNLSStrings->pszPagetitleCodecs;
        pcnbp->ulDlgID = ID_XFD_CONTAINERPAGE; // generic cnr page
        pcnbp->ulDefaultHelpPanel  = ID_XSH_MEDIA_CODECS;
        pcnbp->ulPageID = SP_MEDIA_CODECS;
        pcnbp->pfncbInitPage    = xwmmCodecsInitPage;
        ntbInsertPage(pcnbp);

        // IOProcs
        pcnbp = malloc(sizeof(CREATENOTEBOOKPAGE));
        memset(pcnbp, 0, sizeof(CREATENOTEBOOKPAGE));
        pcnbp->somSelf = somSelf;
        pcnbp->hwndNotebook = hwndDlg;
        pcnbp->hmod = savehmod;
        pcnbp->usPageStyleFlags = BKA_MAJOR;
        pcnbp->pszName = pNLSStrings->pszPagetitleIOProcs;
        pcnbp->ulDlgID = ID_XFD_CONTAINERPAGE; // generic cnr page
        pcnbp->ulDefaultHelpPanel  = ID_XSH_MEDIA_IOPROCS;
        pcnbp->ulPageID = SP_MEDIA_IOPROCS;
        pcnbp->pfncbInitPage    = xwmmIOProcsInitPage;
        ntbInsertPage(pcnbp);

        // "Devices" above that
        pcnbp = malloc(sizeof(CREATENOTEBOOKPAGE));
        memset(pcnbp, 0, sizeof(CREATENOTEBOOKPAGE));
        pcnbp->somSelf = somSelf;
        pcnbp->hwndNotebook = hwndDlg;
        pcnbp->hmod = savehmod;
        pcnbp->usPageStyleFlags = BKA_MAJOR;
        pcnbp->pszName = pNLSStrings->pszPagetitleDevices;
        pcnbp->ulDlgID = ID_XFD_CONTAINERPAGE; // generic cnr page
        pcnbp->ulDefaultHelpPanel  = ID_XSH_MEDIA_DEVICES;
        pcnbp->ulPageID = SP_MEDIA_DEVICES;
        pcnbp->pfncbInitPage    = xwmmDevicesInitPage;
        ntbInsertPage(pcnbp);
    }

    return (TRUE);
}

/*
 *@@ wpFilterPopupMenu:
 *      this WPObject instance method allows the object to
 *      filter out unwanted menu items from the context menu.
 *      This gets called before wpModifyPopupMenu.
 *
 *      We remove the "Create another" menu item.
 */

SOM_Scope ULONG  SOMLINK xwmm_wpFilterPopupMenu(XWPMedia *somSelf,
                                                ULONG ulFlags,
                                                HWND hwndCnr,
                                                BOOL fMultiSelect)
{
    /* XWPMediaData *somThis = XWPMediaGetData(somSelf); */
    XWPMediaMethodDebug("XWPMedia","xwmm_wpFilterPopupMenu");

    return (XWPMedia_parent_WPAbstract_wpFilterPopupMenu(somSelf,
                                                         ulFlags,
                                                         hwndCnr,
                                                         fMultiSelect)
            & ~CTXT_NEW
           );
}

/*
 *@@ wpQueryDefaultHelp:
 *      this WPObject instance method specifies the default
 *      help panel for an object (when "Extended help" is
 *      selected from the object's context menu). This should
 *      describe what this object can do in general.
 *      We must return TRUE to report successful completion.
 *
 *      We'll display some help for the XWPMedia class.
 */

SOM_Scope BOOL  SOMLINK xwmm_wpQueryDefaultHelp(XWPMedia *somSelf,
                                                PULONG pHelpPanelId,
                                                PSZ HelpLibrary)
{
    /* XWPMediaData *somThis = XWPMediaGetData(somSelf); */
    XWPMediaMethodDebug("XWPMedia","xwmm_wpQueryDefaultHelp");

    return (XWPMedia_parent_WPAbstract_wpQueryDefaultHelp(somSelf,
                                                          pHelpPanelId,
                                                          HelpLibrary));
}

/*
 *@@ wpQueryDefaultView:
 *      this WPObject method returns the default view of an object,
 *      that is, which view is opened if the program file is
 *      double-clicked upon. This is also used to mark
 *      the default view in the "Open" context submenu.
 *
 *      This must be overridden for direct WPAbstract subclasses,
 *      because otherwise double-clicks on the object won't
 *      work.
 *
 *      We return the Settings view always.
 */

SOM_Scope ULONG  SOMLINK xwmm_wpQueryDefaultView(XWPMedia *somSelf)
{
    /* XWPMediaData *somThis = XWPMediaGetData(somSelf); */
    XWPMediaMethodDebug("XWPMedia","xwmm_wpQueryDefaultView");

    return (OPEN_SETTINGS);
}

/*
 *@@ wpAddObjectWindowPage:
 *      this WPObject instance method normally adds the
 *      "Standard Options" page to the settings notebook
 *      (that's what the WPS reference calls it; it's actually
 *      the "Window" page).
 *
 *      We don't want that page in XWPMedia, so we remove it.
 */

SOM_Scope ULONG  SOMLINK xwmm_wpAddObjectWindowPage(XWPMedia *somSelf,
                                                    HWND hwndNotebook)
{
    /* XWPMediaData *somThis = XWPMediaGetData(somSelf); */
    XWPMediaMethodDebug("XWPMedia","xwmm_wpAddObjectWindowPage");

    return (SETTINGS_PAGE_REMOVED);
}

/*
 *@@ wpAddSettingsPages:
 *
 */

SOM_Scope BOOL  SOMLINK xwmm_wpAddSettingsPages(XWPMedia *somSelf,
                                                HWND hwndNotebook)
{
    BOOL brc;
    /* XWPMediaData *somThis = XWPMediaGetData(somSelf); */
    XWPMediaMethodDebug("XWPMedia","xwmm_wpAddSettingsPages");

    brc = (XWPMedia_parent_WPAbstract_wpAddSettingsPages(somSelf,
                                                         hwndNotebook));
    if (brc)
        _xwpAddXWPMediaPages(somSelf, hwndNotebook);

    return (brc);
}

/* ******************************************************************
 *                                                                  *
 *   XWPMedia Class Methods                                         *
 *                                                                  *
 ********************************************************************/

/*
 *@@ wpclsInitData:
 *
 */

SOM_Scope void  SOMLINK xwmmM_wpclsInitData(M_XWPMedia *somSelf)
{
    /* M_XWPMediaData *somThis = M_XWPMediaGetData(somSelf); */
    M_XWPMediaMethodDebug("M_XWPMedia","xwmmM_wpclsInitData");

    M_XWPMedia_parent_M_WPAbstract_wpclsInitData(somSelf);

    {
        PKERNELGLOBALS   pKernelGlobals = krnLockGlobals(5000);
        // store the class object in KERNELGLOBALS
        pKernelGlobals->fXWPMedia = TRUE;
        krnUnlockGlobals();
    }
}

/*
 *@@ wpclsQueryStyle:
 *
 */

SOM_Scope ULONG  SOMLINK xwmmM_wpclsQueryStyle(M_XWPMedia *somSelf)
{
    /* M_XWPMediaData *somThis = M_XWPMediaGetData(somSelf); */
    M_XWPMediaMethodDebug("M_XWPMedia","xwmmM_wpclsQueryStyle");

    return (M_XWPMedia_parent_M_WPAbstract_wpclsQueryStyle(somSelf)
                | CLSSTYLE_NEVERPRINT
                | CLSSTYLE_NEVERCOPY
                | CLSSTYLE_NEVERDELETE);
}

/*
 *@@ wpclsQueryTitle:
 *
 */

SOM_Scope PSZ  SOMLINK xwmmM_wpclsQueryTitle(M_XWPMedia *somSelf)
{
    /* M_XWPMediaData *somThis = M_XWPMediaGetData(somSelf); */
    M_XWPMediaMethodDebug("M_XWPMedia","xwmmM_wpclsQueryTitle");

    return ("XWorkplace Multimedia");
}

/*
 *@@ wpclsQueryIconData:
 *
 */

SOM_Scope ULONG  SOMLINK xwmmM_wpclsQueryIconData(M_XWPMedia *somSelf,
                                                  PICONINFO pIconInfo)
{
    /* M_XWPMediaData *somThis = M_XWPMediaGetData(somSelf); */
    M_XWPMediaMethodDebug("M_XWPMedia","xwmmM_wpclsQueryIconData");

    if (pIconInfo) {
        pIconInfo->fFormat = ICON_RESOURCE;
        pIconInfo->resid   = ID_ICONXWPMEDIA;
        pIconInfo->hmod    = cmnQueryMainModuleHandle();
    }

    return (sizeof(ICONINFO));
}

/*
 *@@ wpclsQuerySettingsPageSize:
 *
 */

SOM_Scope BOOL  SOMLINK xwmmM_wpclsQuerySettingsPageSize(M_XWPMedia *somSelf,
                                                         PSIZEL pSizl)
{
    /* M_XWPMediaData *somThis = M_XWPMediaGetData(somSelf); */
    M_XWPMediaMethodDebug("M_XWPMedia","xwmmM_wpclsQuerySettingsPageSize");

    return (M_XWPMedia_parent_M_WPAbstract_wpclsQuerySettingsPageSize(somSelf,
                                                                      pSizl));
}


