
/*
 * ximgview.c:
 *      primitive image viewer. Uses MMPM/2 to understand all its
 *      image file formats.
 *
 *      This is also a simple example of how to multithread a PM
 *      application. Loading the bitmap is passed to a second
 *      transient thread.
 *
 *      Copyright (C) 2000 Ulrich M”ller.
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, in version 2 as it comes in the COPYING
 *      file of the XWorkplace main distribution.
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 */

#define INCL_WIN
#define INCL_WINWORKPLACE
#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_GPI                // required for INCL_MMIO_CODEC
#define INCL_GPIBITMAPS         // required for INCL_MMIO_CODEC
#include <os2.h>

// multimedia includes
#define INCL_MCIOS2
#define INCL_MMIOOS2
#define INCL_MMIO_CODEC
#include <os2me.h>

#include <stdio.h>
#include <setjmp.h>

#include "setup.h"

#include "helpers\cnrh.h"
#include "helpers\eah.h"
#include "helpers\dosh.h"
#include "helpers\gpih.h"
#include "helpers\except.h"
#include "helpers\stringh.h"
#include "helpers\winh.h"
#include "helpers\threads.h"

#include "bldlevel.h"
#include "dlgids.h"

#include "ximgview.h"

/* ******************************************************************
 *                                                                  *
 *   Global variables for all threads                               *
 *                                                                  *
 ********************************************************************/

CHAR        G_szFilename[CCHMAXPATH] = "";

/* ******************************************************************
 *                                                                  *
 *   Image handling                                                 *
 *                                                                  *
 ********************************************************************/

#define LOADBMP_SUCCESS                 0
// #define LOADBMP_UNKNOWN_ERROR           1
#define LOADBMP_GOT_DOS_IOPROC          2
#define LOADBMP_NOT_IMAGE_FILE          3
#define LOADBMP_OPEN_FAILED             4
#define LOADBMP_INCOMPATIBLE_HEADER     5
#define LOADBMP_HEADER_UNAVAILABLE      6
#define LOADBMP_GPIGREATEPS_FAILED      7
#define LOADBMP_GPICREATEBITMAP_FAILED  8
#define LOADBMP_GPISETBITMAPBITS_FAILED 9
#define LOADBMP_OUT_OF_MEMORY           10
#define LOADBMP_CRASHED                 11
#define LOADBMP_MMIOREAD_FAILED         12

/*
 *@@ QueryErrorDescription:
 *
 */

PSZ QueryErrorDescription(ULONG ulError)
{
    switch (ulError)
    {
        // case LOADBMP_UNKNOWN_ERROR:
        case LOADBMP_GOT_DOS_IOPROC:
            return "Image file format not supported.";

        case LOADBMP_NOT_IMAGE_FILE:
            return "File is non-image multimedia file.";

        case LOADBMP_OPEN_FAILED:
            return "Open failed.";

        case LOADBMP_INCOMPATIBLE_HEADER:
            return "Incompatible bitmap header size.";

        case LOADBMP_HEADER_UNAVAILABLE:
            return "Bitmap header unavailable.";

        case LOADBMP_GPIGREATEPS_FAILED:
            return "Error creating memory presentation space (GpiCreatePS failed).";

        case LOADBMP_GPICREATEBITMAP_FAILED:
            return "Error creating PM bitmap (GpiCreateBitmap failed).";

        case LOADBMP_GPISETBITMAPBITS_FAILED:
            return "GpiSetBitmapBits failed.";

        case LOADBMP_OUT_OF_MEMORY:
            return "Out of memory.";

        case LOADBMP_CRASHED:
            return "Crashed reading bitmap.";

        case LOADBMP_MMIOREAD_FAILED:
            return "Error reading bitmap data.";
    }

    return ("Unknown error.");
}

/*
 *@@ CreateBitmapFromFile:
 *      this actually loads the bitmap file
 *      specified by hmmio (which must be a
 *      bitmap file handle!) and creates a
 *      regular PM bitmap from it.
 *
 *      The bitmap in stored *phbmOut. It
 *      is not selected into any HPS.
 *
 *      Returns 0 (LOADBMP_SUCCESS) on success.
 *      On error, returns one of the following:
 *
 *      -- LOADBMP_HEADER_UNAVAILABLE: mmioGetHeader failed.
 *      -- LOADBMP_OUT_OF_MEMORY: DosAllocMem failed.
 *      -- LOADBMP_GPIGREATEPS_FAILED: GpiCreatePS failed.
 *      -- LOADBMP_GPICREATEBITMAP_FAILED: GpiCreateBitmap failed.
 *      -- LOADBMP_MMIOREAD_FAILED: mmioRead failed.
 *      -- LOADBMP_GPISETBITMAPBITS_FAILED: GpiSetBitmapBits failed.
 */

ULONG CreateBitmapFromFile(HAB hab,             // in: anchor block
                           HDC hdcMem,          // in: memory device context
                           HMMIO hmmio,         // in: MMIO file handle
                           HBITMAP *phbmOut)    // out: bitmap created
{
    ULONG   ulReturn = LOADBMP_SUCCESS,
            ulrc;

    MMIMAGEHEADER mmImgHdr;
    ULONG   ulBytesRead;

    ulrc = mmioGetHeader(hmmio,
                         &mmImgHdr,
                         sizeof(MMIMAGEHEADER),
                         (PLONG)&ulBytesRead,
                         0L,
                         0L);

    if (ulrc != MMIO_SUCCESS)
        // header unavailable
        ulReturn = LOADBMP_HEADER_UNAVAILABLE;
    else
    {
        /*
         *  Determine the number of bytes required, per row.
         *      PLANES MUST ALWAYS BE = 1
         */

        ULONG dwHeight = mmImgHdr.mmXDIBHeader.BMPInfoHeader2.cy;
        ULONG dwWidth = mmImgHdr.mmXDIBHeader.BMPInfoHeader2.cx;
        SHORT wBitCount = mmImgHdr.mmXDIBHeader.BMPInfoHeader2.cBitCount;
        ULONG dwRowBits = dwWidth * mmImgHdr.mmXDIBHeader.BMPInfoHeader2.cBitCount;
        ULONG dwNumRowBytes = dwRowBits >> 3;

        ULONG dwPadBytes;
        SIZEL ImageSize;
        PBYTE pRowBuffer;

        /*
         *  Account for odd bits used in 1bpp or 4bpp images that are
         *  NOT on byte boundaries.
         */

        if (dwRowBits % 8)
        {
             dwNumRowBytes++;
        }

        /*
         *  Ensure the row length in bytes accounts for byte padding.
         *  All bitmap data rows must are aligned on LONG/4-BYTE boundaries.
         *  The data FROM an IOProc should always appear in this form.
         */

        dwPadBytes = (dwNumRowBytes % 4);

        if (dwPadBytes)
            dwNumRowBytes += 4 - dwPadBytes;

        // allocate space for ONE row of pels
        if (DosAllocMem((PPVOID)&pRowBuffer,
                        dwNumRowBytes,
                        fALLOC))
            ulReturn = LOADBMP_OUT_OF_MEMORY;
        else
        {
            // create a memory presentation space with the
            // size of the bitmap
            HPS hpsMem;

            ImageSize.cx = dwWidth;
            ImageSize.cy = dwHeight;

            hpsMem = GpiCreatePS(hab,
                                 hdcMem,
                                 &ImageSize,
                                 PU_PELS | GPIF_DEFAULT | GPIT_MICRO | GPIA_ASSOC);

            if (!hpsMem)
                ulReturn = LOADBMP_GPIGREATEPS_FAILED;
            else
            {
                // create an uninitialized bitmap -- this is where we
                // will put all of the bits once we read them in
                *phbmOut = GpiCreateBitmap(hpsMem,
                                           &mmImgHdr.mmXDIBHeader.BMPInfoHeader2,
                                           0L,
                                           NULL,
                                           NULL);
                if (!*phbmOut)
                    ulReturn = LOADBMP_GPICREATEBITMAP_FAILED;
                else
                {
                    // bitmap created:

                    if (GpiSetBitmap(hpsMem,
                                     *phbmOut)
                            == HBM_ERROR)
                        ulReturn = LOADBMP_GPICREATEBITMAP_FAILED;
                    else
                    {
                        ULONG dwRowCount;

                        // load the bitmap from the file,
                        // one line at a time, starting from the BOTTOM
                        for (dwRowCount = 0;
                             dwRowCount < dwHeight;
                             dwRowCount++)
                        {
                            LONG lrc;
                            ulBytesRead = (ULONG)mmioRead(hmmio,
                                                          pRowBuffer,
                                                          dwNumRowBytes);

                            if (ulBytesRead == 0)
                                // done:
                                break;
                            else if (ulBytesRead == MMIO_ERROR)
                            {
                                ulReturn = LOADBMP_MMIOREAD_FAILED;
                                break;
                            }

                            // allow context switching while previewing.. Couldn't get
                            // it to work. Perhaps will get to it when time is available...

                            lrc = GpiSetBitmapBits(hpsMem,
                                                   (LONG)dwRowCount,
                                                   (LONG)1,
                                                   (PBYTE)pRowBuffer,
                                                   (PBITMAPINFO2)&mmImgHdr.mmXDIBHeader.BMPInfoHeader2);
                            if (lrc == GPI_ALTERROR)
                            {
                                ulReturn = LOADBMP_GPISETBITMAPBITS_FAILED;
                                break;
                            }
                        }

                        // unset bitmap im mem PS
                        GpiSetBitmap(hpsMem,
                                     NULLHANDLE);
                    } // end if (GpiSetBitmap(hpsMem

                    if (ulReturn != LOADBMP_SUCCESS)
                    {
                        // error:
                        GpiDeleteBitmap(*phbmOut);
                        *phbmOut = NULLHANDLE;
                    }
                }

                GpiDestroyPS(hpsMem);
            } // end if GpiCreatePS

            DosFreeMem(pRowBuffer);
        } // end if (DosAllocMem((PPVOID)&pRowBuffer,
    }

    return (ulReturn);
}

/*
 *@@ LoadBitmap:
 *      one-shot function for loading an image
 *      from a file. Understands any image file
 *      format supported by MMPM/2.
 *
 *      Returns 0 (LOADBMP_SUCCESS) on success.
 *      On error, returns one of the following:
 *
 *      -- LOADBMP_GOT_DOS_IOPROC: file format not understood
 *         by MMPM/2.
 *      -- LOADBMP_NOT_IMAGE_FILE: file format understood, but
 *         is not image file or cannot be translated.
 *      -- LOADBMP_OPEN_FAILED: mmioOpen failed.
 *      -- LOADBMP_INCOMPATIBLE_HEADER: ioproc returned invalid
 *         header.
 *      -- LOADBMP_CRASHED: exception occured.
 *
 *      plus the error codes from CreateBitmapFromFile().
 */

ULONG LoadBitmap(HAB hab,           // in: anchor block
                 HDC hdcMem,        // in: memory device context
                 HBITMAP *phbmOut,  // out: bitmap created
                 PSZ pszFileName)   // in: filename
{
    ULONG           ulReturn = LOADBMP_SUCCESS;

    TRY_LOUD(excpt1, NULL)
    {
        ULONG           ulrc = 0;

        MMFORMATINFO    mmFormatInfo;
        FOURCC          fccStorageSystem;

        // find the IOProc which can understand this file
        ulrc = mmioIdentifyFile(pszFileName,
                                NULL,       // needed for RIFF only
                                &mmFormatInfo, // out: format info (FOURCC)
                                &fccStorageSystem, // out: FOURCC of storage IOProc
                                0L,         // reserved
                                0L);        // can be:
                                /*  MMIO_FORCE_IDENTIFY_SS
                                        Forces the identification of a storage
                                        system by ignoring the file
                                        name and actually checking the MMIO Manager's
                                        I/O procedure list.
                                    MMIO_FORCE_IDENTIFY_FF
                                        Forces the identification  of a file
                                        format by ignoring the file name
                                        and actually checking the MMIO Manager's
                                        I/O procedure list.
                                */

        if (ulrc == MMIO_SUCCESS)
        {
            // if mmioIdentifyFile did not find a custom-written IO proc which
            // can understand the image file, then it will return the DOS IO Proc
            // info because the image file IS a DOS file.

            if (mmFormatInfo.fccIOProc == FOURCC_DOS)
                ulReturn = LOADBMP_GOT_DOS_IOPROC;
            else
            {
                // ensure this is an IMAGE IOproc, and that it can read
                // translated data
                if (   (mmFormatInfo.ulMediaType != MMIO_MEDIATYPE_IMAGE)
                     || ((mmFormatInfo.ulFlags & MMIO_CANREADTRANSLATED) == 0)
                   )
                {
                    ulReturn = LOADBMP_NOT_IMAGE_FILE;
                }
                else
                {
                    // remember fourcc of IOProc
                    FOURCC      fccIOProc = mmFormatInfo.fccIOProc;

                    // load file
                    MMIOINFO      mmioinfo;
                    HMMIO         hmmio;

                    memset(&mmioinfo, 0, sizeof (MMIOINFO));
                    mmioinfo.fccIOProc = fccIOProc;
                    mmioinfo.ulTranslate = MMIO_TRANSLATEHEADER | MMIO_TRANSLATEDATA;
                    hmmio = mmioOpen((PSZ)pszFileName,
                                     &mmioinfo,
                                     MMIO_READ | MMIO_DENYWRITE | MMIO_NOIDENTIFY);

                    if (!hmmio)
                    {
                        ulReturn = LOADBMP_OPEN_FAILED;
                    }
                    else
                    {
                        ULONG ulImageHeaderLength;
                        ULONG dwReturnCode = mmioQueryHeaderLength(hmmio,
                                                                   (PLONG)&ulImageHeaderLength,
                                                                   0L,
                                                                   0L);

                        if (ulImageHeaderLength != sizeof (MMIMAGEHEADER))
                            ulReturn = LOADBMP_INCOMPATIBLE_HEADER;
                        else
                        {
                            ulReturn = CreateBitmapFromFile(hab,
                                                            hdcMem,
                                                            hmmio,
                                                            phbmOut);
                        }

                        ulrc = mmioClose(hmmio, 0L);
                    } // end if hmmio
                }
            }
        }
    }
    CATCH(excpt1)
    {
        ulReturn = LOADBMP_CRASHED;
    } END_CATCH();

    return (ulReturn);
}

/*
 *@@ fntLoadBitmap:
 *      transient thread created by WMCommand_FileOpen
 *      to load the bitmap file.
 *
 *      Posts WM_DONELOADINGBMP to the client when done.
 */

void _Optlink fntLoadBitmap(PTHREADINFO pti)
{
    // create a memory PS
    HWND        hwndClient = (HWND)(pti->ulData);
    HBITMAP     hbmNew;

    HDC hdcMem = DevOpenDC(pti->hab,
                           OD_MEMORY,
                           "*",
                           0L,
                           NULL,
                           0);

    ULONG ulrc = LoadBitmap(pti->hab,
                            hdcMem,
                            &hbmNew,
                            G_szFilename);
    WinPostMsg(hwndClient,
               WM_DONELOADINGBMP,
               (MPARAM)ulrc,
               (MPARAM)hbmNew);

    DevCloseDC(hdcMem);
}

/* ******************************************************************
 *                                                                  *
 *   Global variables for main thread                               *
 *                                                                  *
 ********************************************************************/

HAB         G_habMain = NULLHANDLE;
HMQ         G_hmqMain = NULLHANDLE;
HWND        G_hwndMain = NULLHANDLE;
HBITMAP     G_hbmLoaded = NULLHANDLE;

PFNWP       G_pfnwpFrameOrig = NULL;

BOOL        G_fStartingUp = FALSE;
BOOL        G_fUseLastOpenDir = FALSE;

// load-bitmap thread
BOOL        G_fLoadingBitmap = FALSE;
THREADINFO  G_tiLoadBitmap = {0};

// scroll bar data
BITMAPINFOHEADER G_bmihBitmapLoaded;
LONG        G_lVertScrollOfs,
            G_lHorzScrollOfs;

#define VERT_SCROLL_UNIT 10
#define HORZ_SCROLL_UNIT 10

/*
 *@@ G_GlobalSettings:
 *      global settings.
 */

struct
{
    BOOL    fResizeAfterLoad,
            fConstrain2Screen,
            fScale2WinSize;
} G_GlobalSettings;

/* ******************************************************************
 *                                                                  *
 *   Command handler                                                *
 *                                                                  *
 ********************************************************************/

VOID UpdateTitle(VOID)
{
    CHAR szTitle[500] = "ximgview";

    if (G_hbmLoaded)
    {
        CHAR szName[CCHMAXPATH] = "",
             szExt[CCHMAXPATH] = "";
        _splitpath(G_szFilename, NULL, NULL, szName, szExt);
        sprintf(szTitle + strlen(szTitle),
                " - %s%s (%dx%d, %d bpp)",
                szName, szExt,
                G_bmihBitmapLoaded.cx,
                G_bmihBitmapLoaded.cy,
                G_bmihBitmapLoaded.cBitCount);

    }

    WinSetWindowText(G_hwndMain, szTitle);
}

/*
 *@@ CalcViewportSize:
 *
 */

VOID CalcViewportSize(HWND hwndClient,
                      SIZEL *pszl)
{
    SWP swpClient;
    WinQueryWindowPos(hwndClient, &swpClient);
            // this already has the scroll bars subtracted;
            // apparently, these are frame controls...
    pszl->cx = swpClient.cx; //  - WinQuerySysValue(HWND_DESKTOP, SV_CXVSCROLL);
    pszl->cy = swpClient.cy; //  - WinQuerySysValue(HWND_DESKTOP, SV_CYHSCROLL);
}

/*
 *@@ UpdateScrollBars:
 *
 */

VOID UpdateScrollBars(HWND hwndClient)
{
    HWND hwndVScroll = WinWindowFromID(G_hwndMain, FID_VERTSCROLL);
    HWND hwndHScroll = WinWindowFromID(G_hwndMain, FID_HORZSCROLL);

    if (G_GlobalSettings.fScale2WinSize)
    {
        WinEnableWindow(hwndVScroll, FALSE);
        WinEnableWindow(hwndHScroll, FALSE);
    }
    else
    {
        SIZEL szlViewport;
        CalcViewportSize(hwndClient, &szlViewport);

        _Pmpf(("bitmap cx: %d", G_bmihBitmapLoaded.cx));
        _Pmpf(("viewport cx: %d", szlViewport.cx));

        // vertical (height)
        winhUpdateScrollBar(hwndVScroll,
                            szlViewport.cy,
                            G_bmihBitmapLoaded.cy,
                            G_lVertScrollOfs,
                            FALSE);      // auto-hide
        // horizontal (width)
        winhUpdateScrollBar(hwndHScroll,
                            szlViewport.cx,
                            G_bmihBitmapLoaded.cx,
                            G_lHorzScrollOfs,
                            FALSE);      // auto-hide
    }
}

/*
 *@@ UpdateMenuItems:
 *
 */

VOID UpdateMenuItems(VOID)
{
    HWND    hmenuMain = WinWindowFromID(G_hwndMain, FID_MENU);

    // file menu
    WinEnableMenuItem(hmenuMain, IDM_FILE,
                      // disable file menu if load-bitmap thread is running
                      !G_fLoadingBitmap);

    // view menu
    WinEnableMenuItem(hmenuMain, IDM_VIEW,
                      // disable view menu if load-bitmap thread is running
                      // or if we have no bitmap yet
                      (     (!G_fLoadingBitmap)
                        && (G_hbmLoaded != NULLHANDLE)
                      ));
    // options menu
    WinCheckMenuItem(hmenuMain, IDMI_OPT_RESIZEAFTERLOAD,
                     G_GlobalSettings.fResizeAfterLoad);
    WinCheckMenuItem(hmenuMain, IDMI_OPT_CONSTRAIN2SCREEEN,
                     G_GlobalSettings.fConstrain2Screen);
    WinCheckMenuItem(hmenuMain, IDMI_OPT_SCALE2WINSIZE,
                     G_GlobalSettings.fScale2WinSize);
}

/*
 *@@ SetWinSize2BmpSize:
 *
 */

VOID SetWinSize2BmpSize(VOID)
{
    SWP swpOld;
    RECTL rcl;

    LONG   lNewCX,
           lNewCY,
           lNewY;

    // calculate new frame size needed:
    // WinCalcFrameRect really wants screen
    // coordinates, but we don't care here
    rcl.xLeft = 0;
    rcl.xRight = rcl.xLeft + G_bmihBitmapLoaded.cx;
    rcl.yBottom = 0;
    rcl.yTop = rcl.yBottom + G_bmihBitmapLoaded.cy;
    WinCalcFrameRect(G_hwndMain,
                     &rcl,
                     FALSE);    // calc frame from client

    // new frame window size:
    WinQueryWindowPos(G_hwndMain,
                      &swpOld);
    lNewCX = rcl.xRight - rcl.xLeft;
    lNewCY = rcl.yTop - rcl.yBottom;

    lNewY = swpOld.y
                  // calculate difference between old
                  // and new cy:
                  // if the new bitmap is larger, we
                  // need to move y DOWN
                  + swpOld.cy
                  - lNewCY;

    // limit window size if off screen
    if (G_GlobalSettings.fConstrain2Screen)
    {
        if ((swpOld.x + lNewCX) > winhQueryScreenCX())
            lNewCX = winhQueryScreenCX() - swpOld.x;
        if (lNewY < 0)
        {
            lNewY = 0;
            lNewCY = swpOld.y + swpOld.cy;
        }
    }

    // now move/resize window:
    // make the top left corner constant
    WinSetWindowPos(G_hwndMain,
                    HWND_TOP,
                    // pos
                    swpOld.x,         // don't change
                    lNewY,
                    // width
                    lNewCX,
                    lNewCY,
                    SWP_MOVE | SWP_SIZE
                        // in case the bitmap is loaded
                        // as a startup parameter:
                        | SWP_SHOW | SWP_ACTIVATE);
}

/*
 *@@ WMCommand_FileOpen:
 *
 */

BOOL WMCommand_FileOpen(HWND hwndClient)
{
    BOOL        brc = FALSE;

    CHAR        szFile[CCHMAXPATH] = "*";

    ULONG       flFlags = WINH_FOD_INISAVEDIR;

    if (G_fUseLastOpenDir)
        // this is FALSE if we were loaded with a
        // image file on the cmd line; in that case,
        // use the startup directory
        flFlags |= WINH_FOD_INILOADDIR;

    if (winhFileDlg(hwndClient,
                    szFile,       // in: file mask; out: fully q'd filename
                    flFlags,
                    HINI_USER,
                    INIAPP,
                    INIKEY_LASTDIR))
    {
        strcpy(G_szFilename, szFile);
        thrCreate(&G_tiLoadBitmap,
                  fntLoadBitmap,
                  &G_fLoadingBitmap,        // running flag
                  THRF_PMMSGQUEUE | THRF_WAIT,
                  hwndClient);      // user param
        UpdateMenuItems();

        // for subsequent opens, load from INI
        G_fUseLastOpenDir = TRUE;
    }

    return (brc);
}

/*
 *@@ WMDoneLoadingBitmap:
 *      handler for WM_DONELOADINGBITMAP when
 *      load-bitmap thread is done. ulError
 *      has the error code (0 if none).
 */

VOID WMDoneLoadingBitmap(HWND hwndClient,
                         ULONG ulError,
                         HBITMAP hbmNew)
{
    if (ulError)
    {
        CHAR szMsg[1000];
        sprintf(szMsg,
                "Error %d loading \"%s\": %s",
                ulError,
                G_szFilename,
                QueryErrorDescription(ulError));

        if (G_fStartingUp)
            // first call:
            WinSetWindowPos(G_hwndMain, 0, 0,0,0,0, SWP_SHOW | SWP_ACTIVATE);
        winhDebugBox(hwndClient, "Error", szMsg);
    }
    else
    {
        // free old bitmap
        if (G_hbmLoaded)
            GpiDeleteBitmap(G_hbmLoaded);

        G_hbmLoaded = hbmNew;

        GpiQueryBitmapParameters(G_hbmLoaded,
                                 &G_bmihBitmapLoaded);
        // reset scroller
        G_lVertScrollOfs = 0;
        G_lHorzScrollOfs = 0;

        if (G_GlobalSettings.fResizeAfterLoad)
            SetWinSize2BmpSize();

        UpdateScrollBars(hwndClient);
        WinInvalidateRect(hwndClient, NULL, FALSE);
    }

    // wait till load-bitmap thread has really exited
    thrWait(&G_tiLoadBitmap);

    UpdateTitle();
    UpdateMenuItems();

    G_fStartingUp = FALSE;
}

/*
 *@@ WMClose_CanClose:
 *      if this returns TRUE, the main window is
 *      closed and the application is terminated.
 *      Prompt for saving data here.
 */

BOOL WMClose_CanClose(HWND hwndClient)
{
    return (TRUE);
}

/* ******************************************************************
 *                                                                  *
 *   Main window                                                    *
 *                                                                  *
 ********************************************************************/

/*
 *@@ PaintClient:
 *
 */

VOID PaintClient(HWND hwndClient,
                 HPS hps,
                 PRECTL prclPaint)
{
    BOOL fFillBackground = TRUE;

    if (G_hbmLoaded)
    {
        POINTL ptlDest = {0, 0};
        RECTL rclDest;
        SIZEL szlViewport;
        CalcViewportSize(hwndClient, &szlViewport);

        if (G_GlobalSettings.fScale2WinSize)
        {
            // "scale-to-window" mode: that's easy
            RECTL rclClient;
            WinQueryWindowRect(hwndClient, &rclClient);
            WinDrawBitmap(hps,
                          G_hbmLoaded,
                          NULL,     // all
                          (PPOINTL)&rclClient, // in stretch mode, this is
                                               // used for the rectangle
                          CLR_BLACK,    // in case we have a b/w bmp
                          CLR_WHITE,    // in case we have a b/w bmp
                          DBM_NORMAL | DBM_STRETCH);
            fFillBackground = FALSE;
        }
        else
        {
            // original size mode:
            if (G_bmihBitmapLoaded.cy < szlViewport.cy)
                // center vertically:
                ptlDest.y = (szlViewport.cy - G_bmihBitmapLoaded.cy) / 2;
            else
            {
                // use scroller offset:
                // calc leftover space
                ULONG ulClipped = G_bmihBitmapLoaded.cy - szlViewport.cy;
                // this is a positive value if the scroller is
                // down from the top or 0 if its at the top.
                // So if it's zero, we must use -ulClipped.
                ptlDest.y -= ulClipped;
                // if it's above zero, we must add to y.
                ptlDest.y += G_lVertScrollOfs;
            }

            if (G_bmihBitmapLoaded.cx < szlViewport.cx)
                // center horizontally:
                ptlDest.x = (szlViewport.cx - G_bmihBitmapLoaded.cx) / 2;
            else
            {
                // use scroller offset:
                // calc leftover space
                ULONG ulClipped = G_bmihBitmapLoaded.cx - szlViewport.cx;
                // this is a positive value if the scroller is
                // right from the left or 0 if its at the left.
                // So if it's zero, we must use -ulClipped.
                // ptlDest.x -= ulClipped;
                // if it's above zero, we must further subtract from x.
                ptlDest.x -= G_lHorzScrollOfs;
            }

            WinDrawBitmap(hps,
                          G_hbmLoaded,
                          NULL,        // subrectangle to be drawn
                          &ptlDest,
                          0, 0,
                          DBM_NORMAL);

            // cut out the rectangle of the bitmap we just
            // painted from the clipping region so that
            // WinFillRect below cannot overwrite it
            rclDest.xLeft = ptlDest.x;
            rclDest.yBottom = ptlDest.y;
            rclDest.xRight = ptlDest.x + G_bmihBitmapLoaded.cx;
            rclDest.yTop = ptlDest.y + G_bmihBitmapLoaded.cy;
            GpiExcludeClipRectangle(hps,
                                    &rclDest);
        }
    }

    if (fFillBackground)
        // fill the remainder white
        WinFillRect(hps, prclPaint, CLR_WHITE);
}

/*
 *@@ fnwpMainClient:
 *
 */

MRESULT EXPENTRY fnwpMainClient(HWND hwndClient, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    switch (msg)
    {
        /*
         * WM_COMMAND:
         *
         */

        case WM_COMMAND:
        {
            USHORT usCmd = SHORT1FROMMP(mp1);
            switch (usCmd)
            {
                case IDMI_FILE_OPEN:
                    WMCommand_FileOpen(hwndClient);
                break;

                case IDMI_FILE_EXIT:
                    WinPostMsg(G_hwndMain, WM_CLOSE, 0, 0);
                break;

                case IDMI_VIEW_SIZEORIG:
                    SetWinSize2BmpSize();
                break;

                case IDMI_OPT_RESIZEAFTERLOAD:
                    G_GlobalSettings.fResizeAfterLoad = !G_GlobalSettings.fResizeAfterLoad;
                    UpdateMenuItems();
                break;

                case IDMI_OPT_CONSTRAIN2SCREEEN:
                    G_GlobalSettings.fConstrain2Screen = !G_GlobalSettings.fConstrain2Screen;
                    UpdateMenuItems();
                break;

                case IDMI_OPT_SCALE2WINSIZE:
                    G_GlobalSettings.fScale2WinSize = !G_GlobalSettings.fScale2WinSize;
                    UpdateMenuItems();
                    UpdateScrollBars(hwndClient);
                    WinInvalidateRect(hwndClient, NULL, FALSE);
                break;
            }
        break; }

        /*
         *@@ WM_DONELOADINGBMP:
         *      load-bitmap thread is done.
         *      Parameters:
         *      -- ULONG mp1: error code.
         *      -- HBITMAP mp2: new bitmap handle.
         */

        case WM_DONELOADINGBMP:
            WMDoneLoadingBitmap(hwndClient,
                                (ULONG)mp1,
                                (HBITMAP)mp2);
        break;

        /*
         * WM_SIZE:
         *
         */

        case WM_SIZE:
            UpdateScrollBars(hwndClient);
            WinInvalidateRect(hwndClient, NULL, FALSE);
        break;

        /*
         * WM_PAINT:
         *
         */

        case WM_PAINT:
        {
            RECTL rclPaint;
            HPS hps = WinBeginPaint(hwndClient,
                                    NULLHANDLE,
                                    &rclPaint);
            PaintClient(hwndClient,
                        hps,
                        &rclPaint);
            WinEndPaint(hps);
        break; }

        case WM_VSCROLL:
        {
            RECTL rcl;
            WinQueryWindowRect(hwndClient, &rcl);
            winhHandleScrollMsg(hwndClient,
                                WinWindowFromID(G_hwndMain, FID_VERTSCROLL),
                                &G_lVertScrollOfs,
                                &rcl,
                                G_bmihBitmapLoaded.cy,
                                10,
                                msg,
                                mp2);
        break; }

        case WM_HSCROLL:
        {
            RECTL rcl;
            WinQueryWindowRect(hwndClient, &rcl);
            _Pmpf(("WM_HSCROLL"));
            winhHandleScrollMsg(hwndClient,
                                WinWindowFromID(G_hwndMain, FID_HORZSCROLL),
                                &G_lHorzScrollOfs,
                                &rcl,
                                G_bmihBitmapLoaded.cx,
                                10,
                                msg,
                                mp2);
            _Pmpf(("End of WM_HSCROLL"));
        break; }

        case WM_CLOSE:
            if (WMClose_CanClose(hwndClient))
                winhSaveWindowPos(G_hwndMain,
                                  HINI_USER,
                                  INIAPP,
                                  INIKEY_MAINWINPOS);
            WinPostMsg(hwndClient, WM_QUIT, 0, 0);
        break;

        case WM_CHAR:
            mrc = (MRESULT)winhProcessScrollChars(hwndClient,
                                                  WinWindowFromID(G_hwndMain, FID_VERTSCROLL),
                                                  WinWindowFromID(G_hwndMain, FID_HORZSCROLL),
                                                  mp1,
                                                  mp2,
                                                  G_bmihBitmapLoaded.cy,
                                                  G_bmihBitmapLoaded.cx);
        break;

        default:
            mrc = WinDefWindowProc(hwndClient, msg, mp1, mp2);
    }
    return (mrc);
}

/*
 *@@ fnwpMainFrame:
 *
 */

MRESULT EXPENTRY fnwpMainFrame(HWND hwndFrame, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    switch (msg)
    {
        case WM_QUERYTRACKINFO:
        {
            PTRACKINFO pti = (PTRACKINFO)mp2;

            G_pfnwpFrameOrig(hwndFrame, msg, mp1, mp2);

            if (pti->ptlMinTrackSize.x < 100)
                pti->ptlMinTrackSize.x = 100;
            if (pti->ptlMinTrackSize.y < 100)
                pti->ptlMinTrackSize.y = 100;
            mrc = (MRESULT)TRUE;
        break; }

        default:
            mrc = G_pfnwpFrameOrig(hwndFrame, msg, mp1, mp2);
    }

    return (mrc);
}

/*
 * main:
 *      program entry point.
 */

int main(int argc,
         char *argv[])
{
    QMSG        qmsg;
    HWND        hwndClient;
    ULONG       cbGlobalSettings = sizeof(G_GlobalSettings);
    ULONG       flSwpFlags;

    G_fStartingUp = TRUE;

    if (!(G_habMain = WinInitialize(0)))
        return FALSE;

    if (!(G_hmqMain = WinCreateMsgQueue(G_habMain, 0)))
        return FALSE;

    // initialize global settings
    memset(&G_GlobalSettings, 0, sizeof(G_GlobalSettings));
    PrfQueryProfileData(HINI_USER,
                        INIAPP,
                        INIKEY_SETTINGS,
                        &G_GlobalSettings,
                        &cbGlobalSettings);

    WinRegisterClass(G_habMain,
                     WC_MY_CLIENT_CLASS,
                     fnwpMainClient,
                     CS_SIZEREDRAW,
                     sizeof(ULONG));

    G_hwndMain = winhCreateStdWindow(HWND_DESKTOP,
                                     0,
                                     FCF_TITLEBAR
                                        | FCF_SYSMENU
                                        | FCF_MINMAX
                                        | FCF_VERTSCROLL
                                        | FCF_HORZSCROLL
                                        | FCF_SIZEBORDER
                                        | FCF_ICON
                                        | FCF_MENU
                                        | FCF_TASKLIST,
                                     0, // WS_VISIBLE,
                                     "Title",
                                     1,     // icon resource
                                     WC_MY_CLIENT_CLASS,
                                     WS_VISIBLE,
                                     0,
                                     NULL,
                                     &hwndClient);

    G_pfnwpFrameOrig = WinSubclassWindow(G_hwndMain,
                                         fnwpMainFrame);

    // standard SWP flags
    flSwpFlags = SWP_MOVE | SWP_SIZE;

    // parse args
    if (argc > 1)
    {
        HDC hdcMem;
        HBITMAP hbmNew;
        HWND hwndShape;
        ULONG ulrc;
        strcpy(G_szFilename, argv[1]);
        thrCreate(&G_tiLoadBitmap,
                  fntLoadBitmap,
                  &G_fLoadingBitmap,        // running flag
                  THRF_PMMSGQUEUE | THRF_WAIT,
                  hwndClient);      // user param
    }
    else
    {
        // no parameter:
        // show window initially
        flSwpFlags |= SWP_SHOW | SWP_ACTIVATE;
        // and retrieve last open dir from OS2.INI
        G_fUseLastOpenDir = TRUE;
    }

    UpdateTitle();
    UpdateMenuItems();

    if (!winhRestoreWindowPos(G_hwndMain,
                              HINI_USER,
                              INIAPP,
                              INIKEY_MAINWINPOS,
                              flSwpFlags))
        // standard pos
        WinSetWindowPos(G_hwndMain,
                        HWND_TOP,
                        10, 10, 500, 500,
                        flSwpFlags);

    //  standard PM message loop
    while (WinGetMsg(G_habMain, &qmsg, NULLHANDLE, 0, 0))
    {
        WinDispatchMsg(G_habMain, &qmsg);
    }

    // initialize global settings
    PrfWriteProfileData(HINI_USER,
                        INIAPP,
                        INIKEY_SETTINGS,
                        &G_GlobalSettings,
                        sizeof(G_GlobalSettings));

    // clean up on the way out
    WinDestroyMsgQueue(G_hmqMain);
    WinTerminate(G_habMain);

    return TRUE;
}


