
/*
 *@@sourcefile pointer.c:
 *
 *      This file is ALL new with V0.9.4.
 *
 *@@added V0.9.4 [umoeller]
 *@@header "pointers\pointer.h"
 */

/*
 *      Copyright (C) 1996-2000 Christian Langanke.
 *      Copyright (C) 2000 Ulrich M”ller.
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


#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define INCL_ERRORS
#define INCL_PM
#define INCL_DOS
#include <os2.h>

#define NEWLINE "\n"

// generic headers
#include "setup.h"              // code generation and debugging options

#include "helpers\eah.h"

#include "pointers\pointer.h"
#include "pointers\macros.h"
#include "pointers\fmacros.h"
#include "pointers\eas.h"

// layout Back&White Pointer
//                   bfhMask.cbSize           +    // bitmap file header
//                   sizeof( argb)            +    // bw color table
//                   2 * ulPlaneSize;              // AND and XOR mask bitmap data
//
//                   bfhMask.cbSize           +    // mask bitmap file header
//                   sizeof( argb)            +    // bw color table
//                   bfh.cbSize               +    // color bitmap file header
//                   ulTargetColorTableLength +    // color table
//                   (2 * ulPlaneSize)        +    // AND and XOR mask bitmap data
//                   ulColorPlanesLen;             // color bitmap data

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : _FlipBitmapDataVertical                                    ³
 *³ Comment   :                                                            ³
 *³ Author    : C.Langanke                                                 ³
 *³ Date      : 08.02.1998                                                 ³
 *³ Update    : 08.02.1998                                                 ³
 *³ called by : app                                                        ³
 *³ calls     : -                                                          ³
 *³ Input     : PVOID  - pointer to bitmap data                            ³
 *³             USHORT - pels per line                                     ³
 *³             USHORT - hight in pels                                     ³
 *³             ULONG  - bits per pel                                      ³
 *³ Tasks     : - flips pointer vertical                                   ³
 *³ returns   : APIRET - OS/2 error code                                   ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */
VOID _FlipBitmapDataVertical
 (
     PVOID pvData,
     USHORT cx,
     USHORT cy,
     ULONG ulBitsPerPel
)
{

    ULONG ulBitsPerLine = cx * ulBitsPerPel;
    ULONG ulBytesPerLine = (ulBitsPerPel / 8) + ((ulBitsPerLine % 8) == 0);
    ULONG ulBytesInBitmap = ulBytesPerLine * cy;

    ULONG i, j;

    PBYTE pbLine;
    PBYTE pByte;
    BYTE byte;
    PULONG pulLine;
    ULONG ulong;

    switch (ulBitsPerPel)
    {

            // ---------------------------------------------------------------

            //   colors  bit|bytes/pel  pels/byte  bits|bytes/line
        case 1:             //     2         1  1/8         8         32    4

            for (i = 0, pByte = pvData; i < ulBytesInBitmap; i++, pByte++)
            {
                byte = 0;
                if (*pByte & 0x01)
                    byte |= 0x80;
                if (*pByte & 0x02)
                    byte |= 0x40;
                if (*pByte & 0x04)
                    byte |= 0x20;
                if (*pByte & 0x08)
                    byte |= 0x10;
                if (*pByte & 0x10)
                    byte |= 0x08;
                if (*pByte & 0x20)
                    byte |= 0x04;
                if (*pByte & 0x40)
                    byte |= 0x02;
                if (*pByte & 0x80)
                    byte |= 0x01;
                *pByte = byte;
            }

            break;

            // ---------------------------------------------------------------

            //   colors  bit|bytes/pel  pels/byte  bits|bytes/line
        case 4:             //    16         4  1/2         2        128   16

            // flip the two pels in each byte
            for (i = 0, pByte = pvData; i < ulBytesInBitmap; i++, pByte++)
            {
                *pByte = *pByte << 4;
            }

            // no break !!! fallthru !!!

            // ---------------------------------------------------------------

            //   colors  bit|bytes/pel  pels/byte  bits|bytes/line
        case 8:             //   256         8   1          1        256   32
            // flip bytes in each line

            for (i = 0, pbLine = pvData; i < (ULONG) cy; i++, pbLine += ulBytesPerLine);
            {
                // flip bytes in line
                for (j = 0; j < (ulBytesPerLine / 2); j++)
                {
                    byte = *(pbLine + j);
                    *(pbLine + j) = *(pbLine + ulBytesPerLine - j);
                    *(pbLine + ulBytesPerLine - j) = byte;
                }
            }
            break;

            // ---------------------------------------------------------------

            //   colors  bit|bytes/pel  pels/byte  bits|bytes/line
        case 24:                //   RGB        24   4        0.25       768   96
            // we do not implement this one yet

            break;
    }

}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : FlipPointerVertical                                        ³
 *³ Comment   :                                                            ³
 *³ Author    : C.Langanke                                                 ³
 *³ Date      : 08.02.1998                                                 ³
 *³ Update    : 08.02.1998                                                 ³
 *³ called by : app                                                        ³
 *³ calls     : -                                                          ³
 *³ Input     : PVOID - PICONDATA                                          ³
 *³ Tasks     : - flips pointer vertical                                   ³
 *³ returns   : APIRET - OS/2 error code                                   ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */
APIRET FlipPointerVertical
 (
     PVOID pvIconData
)
{

    APIRET rc = NO_ERROR;

    PBITMAPFILEHEADER pbfh = pvIconData;
    PBITMAPFILEHEADER2 pbfh2 = pvIconData;

    PBITMAPINFOHEADER pbih = &pbfh->bmp;
    PBITMAPINFOHEADER2 pbih2 = &pbfh2->bmp2;

    ULONG ulMaskHeaderSize = pbfh->cbSize;
    ULONG ulHeaderSize = pbfh->cbSize;

    USHORT usSignature;

    BOOL fUseOldHeader = (ulHeaderSize == sizeof(BITMAPFILEHEADER));
    BOOL fIsColorPointer;

    USHORT cx, cy;
    PUSHORT pxHostspot;
    USHORT cPlanes, cBitCount;

    ULONG ulPelsInPointer;
    ULONG ulBitsPerPel;
    ULONG ulPointerColors;
    ULONG ulBwTableLength;
    ULONG ulColorTableLength;
    ULONG ulPlaneSize;
    ULONG ulColorPlanesLen;

    PVOID pvMaskData;
    PVOID pvBitmapData;

    do
    {

        // check parameters
        if (pvIconData == NULL)
        {
            rc = ERROR_INVALID_PARAMETER;
            break;
        }

        // check signatures
        usSignature = (fUseOldHeader) ? pbfh->usType : pbfh2->usType;
        if ((usSignature != BFT_POINTER) && (usSignature != BFT_COLORPOINTER))
        {
            rc = ERROR_INVALID_DATA;
            break;
        }

        // determine size of panels
        // only 32x32 is valid,
        // _FlipBitmapDataVertical cannot handle
        // (pels * cx) % 8 != 0  //(!!)
        fIsColorPointer = (usSignature != BFT_COLORPOINTER);
        cx = ((fUseOldHeader) ? pbfh->bmp.cx : (USHORT) pbfh2->bmp2.cx);
        cy = ((fUseOldHeader) ? pbfh->bmp.cy : (USHORT) pbfh2->bmp2.cy) / 2;
        if ((cx != 32) || (cy != 32))
        {
            rc = ERROR_INVALID_DATA;
            break;
        }

        // flip Hotspot vertical
        //   pbfh->xHotspot = cx - pbfh->xHotspot; // offsets are identical for old and new header

        // for color pointer: adress color bitmap file header
        if (fIsColorPointer)
        {
            if (fUseOldHeader)
            {
                pbfh = (PBITMAPFILEHEADER)((PBYTE) pbfh +
                    ulHeaderSize +
                    (sizeof(RGB) * 2));
                // determine size and flip hotspot
                ulHeaderSize = pbfh->cbSize;
//         pbfh->xHotspot = cx - pbfh->xHotspot;
            }
            else
            {
                pbfh2 = (PBITMAPFILEHEADER2)((PBYTE) pbfh2 +
                    ulHeaderSize +
                    (sizeof(RGB2) * 2));
                // determine size and flip hotspot
                ulHeaderSize = pbfh2->cbSize;
//         pbfh2->xHotspot = cx - pbfh2->xHotspot;
            }
        }

        ulPelsInPointer = cx * cy;
        cPlanes = ((fUseOldHeader) ? pbfh->bmp.cPlanes : pbfh2->bmp2.cPlanes);
        cBitCount = ((fUseOldHeader) ? pbfh->bmp.cBitCount : pbfh2->bmp2.cBitCount);
        ulBitsPerPel = cPlanes * cBitCount;
        ulPointerColors = 1 << ulBitsPerPel;
        ulBwTableLength = 2 * ((fUseOldHeader) ? sizeof(RGB) : sizeof(RGB2));
        ulColorTableLength = ulPointerColors * ((fUseOldHeader) ? sizeof(RGB) : sizeof(RGB2));
        ulPlaneSize = ulPelsInPointer / 8;
        ulColorPlanesLen = ulPlaneSize * cBitCount;

        // calculate offsets of mask and bitmap data
        if (fIsColorPointer)
        {
            pvMaskData = (PBYTE) pvIconData +
                ulMaskHeaderSize +  // mask bitmap file header
                 ulBwTableLength +  // bw color table
                 ulHeaderSize + // color bitmap file header
                 ulColorTableLength;    // color table;

            _FlipBitmapDataVertical(pvMaskData, cx, cy * 2, ulBitsPerPel);

            pvBitmapData = (PBYTE) pvMaskData +     // AND and XOR mask bitmap data
                 2 * ulPlaneSize;

            _FlipBitmapDataVertical(pvBitmapData, cx, cy, ulBitsPerPel);
        }
        else
        {
            pvMaskData = (PBYTE) pvIconData +
                ulMaskHeaderSize +  // mask bitmap file header
                 ulBwTableLength;   // bw color table

            _FlipBitmapDataVertical(pvMaskData, cx, cy * 2, ulBitsPerPel);
        }

    }
    while (FALSE);              /* enddo */

    return rc;
}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : GrayScalePointer                                           ³
 *³ Comment   :                                                            ³
 *³ Author    : C.Langanke                                                 ³
 *³ Date      : 08.02.1998                                                 ³
 *³ Update    : 08.02.1998                                                 ³
 *³ called by : app                                                        ³
 *³ calls     : -                                                          ³
 *³ Input     : PVOID - PICONDATA                                          ³
 *³ Tasks     : - flips pointer vertical                                   ³
 *³ returns   : APIRET - OS/2 error code                                   ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */
APIRET GrayScalePointer(PVOID pvIconData,
                        ULONG ulScaleCount,
                        ULONG ulBwTreshold)
{

    APIRET rc = NO_ERROR;

    PBITMAPFILEHEADER pbfh = pvIconData;
    PBITMAPFILEHEADER2 pbfh2 = pvIconData;

    PBITMAPINFOHEADER pbih = &pbfh->bmp;
    PBITMAPINFOHEADER2 pbih2 = &pbfh2->bmp2;

    ULONG ulMaskHeaderSize = pbfh->cbSize;
    ULONG ulHeaderSize = pbfh->cbSize;

    USHORT usSignature;

    BOOL fUseOldHeader = (ulHeaderSize == sizeof(BITMAPFILEHEADER));
    BOOL fIsColorPointer;

    USHORT cx = 0, cy = 0;
    PUSHORT pxHostspot;
    USHORT cPlanes, cBitCount;

    ULONG ulPelsInPointer;
    ULONG ulBitsPerPel;
    ULONG ulPointerColors;
    ULONG ulBwTableLength;
    ULONG ulColorTableLength;
    ULONG ulPlaneSize;
    ULONG ulColorPlanesLen;

    PVOID pvColorTable;

    ULONG i;
    PRGB prgb;
    ULONG ulColorValue;

    do
    {

        // check parameters
        if (pvIconData == NULL)
        {
            rc = ERROR_INVALID_PARAMETER;
            break;
        }

        // check signatures
        usSignature = (fUseOldHeader) ? pbfh->usType : pbfh2->usType;
        if ((usSignature != BFT_POINTER) && (usSignature != BFT_COLORPOINTER))
        {
            rc = ERROR_INVALID_DATA;
            break;
        }

        // black&white Pointers neet not to me changed
        if (usSignature != BFT_COLORPOINTER)
            break;

        // flip Hotspot vertical
        //   pbfh->xHotspot = cx - pbfh->xHotspot; // offsets are identical for old and new header

        // for color pointer: adress color bitmap file header
        if (fUseOldHeader)
        {
            pbfh = (PBITMAPFILEHEADER)((PBYTE) pbfh +
                ulHeaderSize +
                (sizeof(RGB) * 2));
            // determine size and flip hotspot
            ulHeaderSize = pbfh->cbSize;
        }
        else
        {
            pbfh2 = (PBITMAPFILEHEADER2)((PBYTE) pbfh2 +
                ulHeaderSize +
                (sizeof(RGB2) * 2));
            // determine size and flip hotspot
            ulHeaderSize = pbfh2->cbSize;
        }

        ulPelsInPointer = cx * cy;
        cPlanes = ((fUseOldHeader) ? pbfh->bmp.cPlanes : pbfh2->bmp2.cPlanes);
        cBitCount = ((fUseOldHeader) ? pbfh->bmp.cBitCount : pbfh2->bmp2.cBitCount);
        ulBitsPerPel = cPlanes * cBitCount;
        ulPointerColors = 1 << ulBitsPerPel;
        ulBwTableLength = 2 * ((fUseOldHeader) ? sizeof(RGB) : sizeof(RGB2));
        ulColorTableLength = ulPointerColors * ((fUseOldHeader) ? sizeof(RGB) : sizeof(RGB2));
        ulPlaneSize = ulPelsInPointer / 8;
        ulColorPlanesLen = ulPlaneSize * cBitCount;

        // calculate offsets of mask and bitmap data
        pvColorTable = (PBYTE) pvIconData +
            ulMaskHeaderSize +  // mask bitmap file header
             ulBwTableLength +  // bw color table
             ulHeaderSize;      // color bitmap file header
        // color table is here

        for (i = 0, prgb = pvColorTable; i < ulPointerColors; i++, prgb++)
        {

            // calculate new gray
            ulColorValue = (ULONG) prgb->bBlue +
                (ULONG) prgb->bGreen +
                (ULONG) prgb->bRed;

            ulColorValue /= 3;
            if (ulColorValue > (ulBwTreshold * 0x0ff / 100))
                ulColorValue = 0x0ff;
            else
                ulColorValue = 0;

            // write new value
            prgb->bBlue = (BYTE) ulColorValue;
            prgb->bGreen = (BYTE) ulColorValue;
            prgb->bRed = (BYTE) ulColorValue;

            // adjust for RGB2 size
            if (!fUseOldHeader)
                prgb = (PRGB)((PBYTE) prgb + 1);
        }


    }
    while (FALSE);              /* enddo */

    return rc;
}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : LoadPointerFromPointerFile                                 ³
 *³ Kommentar : l„dt Pointer aus Datei                                     ³
 *³             Es wird vorausgesetzt, das es sich um eine gltige         ³
 *³             Pointerdatei handelt. Zum Laden des Pointers wird die      ³
 *³             Funktion WinLoadFileIcon miábraucht. Unter 2.11 muá dazu   ³
 *³             das Pointer Image als EA an die Datei angeh„ngt werden, ab ³
 *³             WARP kann das Pointer Image aus der Datei selbst verwendet ³
 *³             werden, dazu wird ein eventuell vorhandenes EA Image       ³
 *³             explizit gel”scht, damit nicht dieses geladen wird.        ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 26.03.1995                                                 ³
 *³ nderung  : 26.03.1995                                                 ³
 *³ aufgerufen: diverse                                                    ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : PSZ          - Name der Pointerdatei                       ³
 *³           : PHPOINTER    - Zielvariable fr PointerHandle              ³
 *³             PICONINFO    - Zielvariable fr Pointerdaten               ³
 *³ Aufgaben  : - Pointer laden                                            ³
 *³ Rckgabe  : BOOL - Flag: Pointer geladen/nicht geladen                 ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

BOOL _Export LoadPointerFromPointerFile
 (
     PSZ pszName,
     PHPOINTER phptr,
     PICONINFO pIconInfo,
     PULONG paulTimeout
)
{
    BOOL fSuccess = FALSE;
    APIRET rc;

    HPOINTER hptr = NULLHANDLE;
    CHAR szTimeout[20];
    ULONG ulDataLen;

// Dos Version Array indexes
#define VI_MAJOR     (QSV_VERSION_MAJOR - QSV_VERSION_MAJOR)
#define VI_MINOR     (QSV_VERSION_MAJOR - QSV_VERSION_MINOR)
#define VI_REVISION  (QSV_VERSION_MAJOR - QSV_VERSION_REVISION)

    ULONG aulDosVersion[3];

    do
    {

        // Parameter prfen
        if ((pszName == NULL) ||
            (pIconInfo == NULL))
            break;

        // ICONINFO initialisieren
        pIconInfo->cb = sizeof(ICONINFO);
        pIconInfo->fFormat = ICON_FILE;
        pIconInfo->pszFileName = strdup(pszName);
        if (pIconInfo->pszFileName == NULL)
            break;
        fSuccess = TRUE;

        if (phptr)
        {
            // Pointer erstellen
            hptr = CreatePtrFromIconInfo(pIconInfo);

            // Ergebnis zurckgeben
            fSuccess = (hptr != NULLHANDLE);
            *phptr = hptr;
        }

        if (paulTimeout)
        {
            // Timeframe EA lesen
            ulDataLen = sizeof(szTimeout);
            rc = eahReadStringEA(pszName, EANAME_TIMEFRAME, szTimeout, &ulDataLen);
            if (rc == NO_ERROR)
                *paulTimeout = atol(szTimeout);
        }

    }
    while (FALSE);


    return fSuccess;
}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : QueryPointerFileDetails                                    ³
 *³ Comment   :                                                            ³
 *³ Author    : C.Langanke                                                 ³
 *³ Date      : 27.06.1996                                                 ³
 *³ Update    : 27.06.1996                                                 ³
 *³ called by : app                                                        ³
 *³ calls     : Dos*                                                       ³
 *³ Input     : ###                                                        ³
 *³ Tasks     : - read pointer file                                        ³
 *³             - return buffer with info                                  ³
 *³ returns   : APIRET - OS/2 error code                                   ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

APIRET _Export QueryPointerFileDetails
 (
     PSZ pszAnimationFileName,
     ULONG ulInfoLevel,
     PVOID pvData,
     ULONG ulBuflen,
     PULONG pulSize
)
{
    APIRET rc = NO_ERROR;
    PBYTE pbData = pvData;
    ULONG i, j;
    HFILE hfileAnimation = NULLHANDLE;
    ULONG ulAction;
    ULONG ulBytesRead, ulFilePtr;

    BITMAPARRAYFILEHEADER bafh;
    ULONG ulSmallHeaderLen = FIELDOFFSET(BITMAPARRAYFILEHEADER, offNext);
    ULONG ulHeaderLen;

    PBITMAPARRAYFILEHEADER pbafh;
    PBITMAPARRAYFILEHEADER2 pbafh2;
    PBITMAPFILEHEADER pbfhMask;
    PBITMAPFILEHEADER2 pbfhMask2;
    PBITMAPFILEHEADER pbfh;
    PBITMAPFILEHEADER2 pbfh2;

    POINTERFILEINFO pointerfileinfo;

    PBYTE pbMaskHeader = NULL;
    PBYTE pbColorHeader = NULL;
    BOOL f32x32Found = FALSE;
    BOOL fIsOldHeader = FALSE;
    ULONG ulSizeX, ulSizeY;
    ULONG ulPelsInPointer;
    ULONG ulBitsPerPel;
    ULONG ulPlaneSize;

    PSOURCEINFO psourceinfo;
    ULONG ulEaValueLen;
    ULONG ulInfoNameLen;
    ULONG ulInfoArtistLen;

    do
    {

        // check parameters
        if ((pszAnimationFileName == NULL) ||
            (pulSize == NULL))
        {
            rc = ERROR_INVALID_PARAMETER;
            break;
        }

        // initialize
        memset(&pointerfileinfo, 0, sizeof(pointerfileinfo));
        if (pvData)
            memset(pvData, 0, ulBuflen);

        // enough space provided ?
        switch (ulInfoLevel)
        {
            case FI_SOURCEINFO:
                do
                {
                    // get len of eas
                    *pulSize = 0;
                    ulInfoNameLen = 0;
                    ulInfoArtistLen = 0;

                    rc = eahReadStringEA(pszAnimationFileName, EANAME_INFONAME, NULL, &ulInfoNameLen);
                    if (rc == ERROR_BUFFER_OVERFLOW)
                        *pulSize += ulInfoNameLen;
                    else
                        *pulSize += 1;

                    rc = eahReadStringEA(pszAnimationFileName, EANAME_INFOARTIST, NULL, &ulInfoArtistLen);
                    if (rc == ERROR_BUFFER_OVERFLOW)
                        *pulSize += ulInfoArtistLen;
                    else
                        *pulSize += 1;

                    // do EAs exist ?
                    if (*pulSize == 2)
                        *pulSize = 0;
                    else
                        *pulSize += sizeof(SOURCEINFO);


                }
                while (FALSE);

                break;

            case FI_DETAILINFO:
                *pulSize = sizeof(POINTERFILEINFO);
                break;
        }

        // no data available
        if (*pulSize == 0)
            break;

        // insufficient buffer size
        if (ulBuflen < *pulSize)
        {
            rc = ERROR_MORE_DATA;
            break;
        }

        // read EAs only
        if (ulInfoLevel == FI_SOURCEINFO)
        {
            psourceinfo = (PSOURCEINFO) pvData;

            // read name
            psourceinfo->pszInfoName = (PSZ) psourceinfo + sizeof(SOURCEINFO);
            if (ulInfoNameLen)
            {
                ulEaValueLen = ulInfoNameLen;
                rc = eahReadStringEA(pszAnimationFileName, EANAME_INFONAME, psourceinfo->pszInfoName, &ulEaValueLen);
                if (rc != NO_ERROR)
                    break;
            }

            // read artist
            psourceinfo->pszInfoArtist = NEXTSTR(psourceinfo->pszInfoName);
            if (ulInfoArtistLen)
            {
                ulEaValueLen = ulInfoArtistLen;
                rc = eahReadStringEA(pszAnimationFileName, EANAME_INFOARTIST, psourceinfo->pszInfoArtist, &ulEaValueLen);
                if (rc != NO_ERROR)
                    break;
            }

            // end here
            break;

        }

        // Open and read the .PTR file header and the first ICONFILERES
        rc = DosOpen(pszAnimationFileName,
                     &hfileAnimation,
                     &ulAction,
                     0,
                     0,
                     OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS,
                     OPEN_SHARE_DENYWRITE | OPEN_ACCESS_READONLY,
                     NULL);
        if (rc != NO_ERROR)
            break;

        // read sig and size fields
        memset(&bafh, 0, sizeof(bafh));
        QUERYFILEPTR(hfileAnimation);
        READMEMORY(hfileAnimation, bafh, ulSmallHeaderLen);

        // -------------------------------------------------------------

        // process array field
        if (bafh.usType == BFT_BITMAPARRAY)
        {
            pointerfileinfo.fMultResolutions = TRUE;
            while (TRUE)
            {
                // is it a 32x32 image ?
                if ((bafh.cxDisplay == 32) &&
                    (bafh.cyDisplay == 32))
                {
                    f32x32Found = TRUE;
                    break;
                }

                // no more headers ?
                if (bafh.offNext == 0)
                    break;

                // move to next file header
                SETFILEPTR(hfileAnimation, bafh.offNext);

                // read next header
                READMEMORY(hfileAnimation, bafh, ulSmallHeaderLen);
                if (bafh.usType != BFT_BITMAPARRAY)
                {
                    rc = ERROR_INVALID_DATA;
                    break;
                }

            }                   // while (TRUE)

            if (rc != NO_ERROR)
                break;

            // no 32x32 image found ?
            if (!f32x32Found)
            {
                rc = ERROR_INVALID_DATA;
                break;
            }

        }                       // if (bafh->usType == BFT_BITMAPARRAY)

        // -------------------------------------------------------------

        // get memory for mask header
        ulHeaderLen = bafh.cbSize;
        if ((pbMaskHeader = malloc(ulHeaderLen)) == NULL)
        {
            rc = ERROR_NOT_ENOUGH_MEMORY;
            break;
        }

        // read mask bitmap file header
        MOVEFILEPTR(hfileAnimation, -(LONG)ulSmallHeaderLen);
        PREADMEMORY(hfileAnimation, pbMaskHeader, ulHeaderLen);

        pbfhMask = (PBITMAPFILEHEADER) pbMaskHeader;
        pbfhMask2 = (PBITMAPFILEHEADER2) pbMaskHeader;
        if (pbfhMask->usType == BFT_BITMAPARRAY)
        {
            // skip array file header
            pbafh = (PBITMAPARRAYFILEHEADER) pbMaskHeader;
            pbfhMask = (PBITMAPFILEHEADER) & pbafh->bfh;
            pbfhMask2 = (PBITMAPFILEHEADER2) & pbafh->bfh;
            ulHeaderLen = pbfhMask->cbSize;
            ulFilePtr += (PBYTE) pbfhMask - (PBYTE) pbMaskHeader;
        }

        // -------------------------------------------------------------

        // get some first information from mask header

        fIsOldHeader = (ulHeaderLen == sizeof(BITMAPFILEHEADER));
        pointerfileinfo.fBwPointer = pbfhMask->usType == BFT_POINTER;
        pointerfileinfo.f13Header = fIsOldHeader;
        pointerfileinfo.ulBfhMaskOfs = ulFilePtr;
        pointerfileinfo.ulBfhMaskLen = ulHeaderLen;

        pointerfileinfo.ulBWColorTableOfs = ulFilePtr + ulHeaderLen;
        pointerfileinfo.ulBWColorTableLen = 2 * ((fIsOldHeader) ? sizeof(RGB) : sizeof(RGB2));

        pointerfileinfo.ulHotspotX = (ULONG) pbfhMask->xHotspot;
        pointerfileinfo.ulHotspotY = (ULONG) pbfhMask->yHotspot;

        ulSizeX = (fIsOldHeader) ? (ULONG) pbfhMask->bmp.cx : pbfhMask2->bmp2.cx;
        ulSizeY = (fIsOldHeader) ? (ULONG) pbfhMask->bmp.cy / 2 : pbfhMask2->bmp2.cy / 2;

        if ((ulSizeX != 32) || (ulSizeY != 32))
        {
            rc = ERROR_INVALID_DATA;
            break;
        }

        if (pointerfileinfo.fBwPointer)
        {
            pointerfileinfo.ulPlanes = (fIsOldHeader) ? (ULONG) pbfhMask->bmp.cPlanes : pbfhMask2->bmp2.cPlanes;
            pointerfileinfo.ulBitsPerPlane = (fIsOldHeader) ? (ULONG) pbfhMask->bmp.cBitCount : pbfhMask2->bmp2.cBitCount;
        }
        else
        {
            // read size of color header
            pointerfileinfo.ulBfhOfs = pointerfileinfo.ulBWColorTableOfs + pointerfileinfo.ulBWColorTableLen;
            SETFILEPTR(hfileAnimation, pointerfileinfo.ulBfhOfs);
            READMEMORY(hfileAnimation, bafh, ulSmallHeaderLen);


            // get memory for color header
            ulHeaderLen = bafh.cbSize;
            if ((pbColorHeader = malloc(ulHeaderLen)) == NULL)
            {
                rc = ERROR_NOT_ENOUGH_MEMORY;
                break;
            }
            // read color bitmap file header
            MOVEFILEPTR(hfileAnimation, -(LONG)ulSmallHeaderLen);
            PREADMEMORY(hfileAnimation, pbColorHeader, ulHeaderLen);
            pointerfileinfo.ulBfhLen = ulHeaderLen;

            // adress color bitmap file header
            pbfh = (PBITMAPFILEHEADER) pbColorHeader;
            pbfh2 = (PBITMAPFILEHEADER2) pbColorHeader;

            pointerfileinfo.ulPlanes = (fIsOldHeader) ? (ULONG) pbfh->bmp.cPlanes : pbfh2->bmp2.cPlanes;
            pointerfileinfo.ulBitsPerPlane = (fIsOldHeader) ? (ULONG) pbfh->bmp.cBitCount : pbfh2->bmp2.cBitCount;
        }

        // some more values
        ulPelsInPointer = ulSizeX * ulSizeY;
        ulBitsPerPel = pointerfileinfo.ulPlanes * pointerfileinfo.ulBitsPerPlane;
        pointerfileinfo.ulPointerColors = 1 << ulBitsPerPel;
        pointerfileinfo.ulColorTableOfs = pointerfileinfo.ulBfhOfs + pointerfileinfo.ulBfhLen;
        pointerfileinfo.ulColorTableLen = pointerfileinfo.ulPointerColors * ((fIsOldHeader) ? sizeof(RGB) : sizeof(RGB2));
        ulPlaneSize = ulPelsInPointer / 8;

        pointerfileinfo.ulMaskDataOfs = pointerfileinfo.ulColorTableOfs + pointerfileinfo.ulColorTableLen;
        pointerfileinfo.ulMaskDataLen = 2 * ulPlaneSize;    // AND and XOR plane !

        pointerfileinfo.ulColorDataOfs = pointerfileinfo.ulMaskDataOfs + pointerfileinfo.ulMaskDataLen;
        pointerfileinfo.ulColorDataLen = ulPlaneSize * pointerfileinfo.ulBitsPerPlane;

        // hand over result
        memcpy(pvData, &pointerfileinfo, sizeof(pointerfileinfo));

    }
    while (FALSE);

// cleanup
    if (hfileAnimation)
        DosClose(hfileAnimation);
    if (pbMaskHeader)
        free(pbMaskHeader);
    if (pbColorHeader)
        free(pbColorHeader);
    return rc;

}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : WritePointerFile                                           ³
 *³ Comment   :                                                            ³
 *³ Author    : C.Langanke                                                 ³
 *³ Date      : 13.04.1998                                                 ³
 *³ Update    : 13.04.1998                                                 ³
 *³ called by : app                                                        ³
 *³ calls     : Dos*                                                       ³
 *³ Input     : ###                                                        ³
 *³ Tasks     : - write pointer file                                       ³
 *³ returns   : APIRET - OS/2 error code                                   ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */
APIRET WritePointerFile
 (
     PSZ pszAnimationFileName,
     ULONG ulTimeout,
     PICONINFO piconinfo,
     PSZ pszInfoName,
     PSZ pszInfoArtist
)
{

    APIRET rc = NO_ERROR;
    ULONG i, j;
    HFILE hfileAnimation = NULLHANDLE;
    ULONG ulAction;
    ULONG ulBytesWritten, ulFilePtr;

    CHAR szValue[20];

    do
    {
        // check parameters
        if ((pszAnimationFileName == NULL) ||
            (*pszAnimationFileName == 0) ||
            (piconinfo == NULL))
        {
            rc = ERROR_INVALID_PARAMETER;
            break;
        }

        if (piconinfo->fFormat != ICON_DATA)
        {
            rc = ERROR_INVALID_DATA;
            break;
        }

        // Datei ”ffnen
        rc = DosOpen(pszAnimationFileName,
                     &hfileAnimation,
                     &ulAction,
                     0, 0,
                     OPEN_ACTION_CREATE_IF_NEW | OPEN_ACTION_REPLACE_IF_EXISTS,
                     OPEN_SHARE_DENYREADWRITE | OPEN_ACCESS_WRITEONLY,
                     NULL);

        if (rc != NO_ERROR)
            break;

        WRITEMEMORY(hfileAnimation, piconinfo->pIconData, piconinfo->cbIconData);

        // write EAs
        _ltoa(ulTimeout, szValue, 10);
        if (ulTimeout)
            eahWriteStringEA(hfileAnimation, EANAME_TIMEFRAME, szValue);
        if (pszInfoName)
            eahWriteStringEA(hfileAnimation, EANAME_INFONAME, pszInfoName);
        if (pszInfoArtist)
            eahWriteStringEA(hfileAnimation, EANAME_INFOARTIST, pszInfoArtist);

    }
    while (FALSE);

// cleanup
    if (hfileAnimation)
        DosClose(hfileAnimation);
    return rc;

}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : SetPointerFileInfo                                         ³
 *³ Comment   :                                                            ³
 *³ Author    : C.Langanke                                                 ³
 *³ Date      : 13.04.1998                                                 ³
 *³ Update    : 13.04.1998                                                 ³
 *³ called by : app                                                        ³
 *³ calls     : Dos*                                                       ³
 *³ Input     : ###                                                        ³
 *³ Tasks     : - write pointer file info                                  ³
 *³ returns   : APIRET - OS/2 error code                                   ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

APIRET _Export SetPointerFileInfo
 (
     PSZ pszAnimationFileName,
     PSOURCEINFO psourceinfo
)
{

    APIRET rc = NO_ERROR;
    HFILE hfileAnimation = NULLHANDLE;
    ULONG i, j;
    ULONG ulAction;

    do
    {
        // check parameters
        if ((pszAnimationFileName == NULL) ||
            (*pszAnimationFileName == 0) ||
            (psourceinfo == NULL))
        {
            rc = ERROR_INVALID_PARAMETER;
            break;
        }

        // Datei ”ffnen
        rc = DosOpen(pszAnimationFileName,
                     &hfileAnimation,
                     &ulAction,
                     0, 0,
                     OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS,
                     OPEN_SHARE_DENYREADWRITE | OPEN_ACCESS_WRITEONLY,
                     NULL);

        if (rc != NO_ERROR)
            break;

        // write EAs
        if (psourceinfo->pszInfoName)
            eahWriteStringEA(hfileAnimation, EANAME_INFONAME, psourceinfo->pszInfoName);
        if (psourceinfo->pszInfoArtist)
            eahWriteStringEA(hfileAnimation, EANAME_INFOARTIST, psourceinfo->pszInfoArtist);

    }
    while (FALSE);

// cleanup
    if (hfileAnimation)
        DosClose(hfileAnimation);
    return rc;

}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : GetPalette                                                 ³
 *³ Comment   :                                                            ³
 *³ Author    : C.Langanke                                                 ³
 *³ Date      : 13.04.1998                                                 ³
 *³ Update    : 13.04.1998                                                 ³
 *³ called by : app                                                        ³
 *³ calls     : Dos*                                                       ³
 *³ Input     : ###                                                        ³
 *³ Tasks     : - extract inter file info                                  ³
 *³ returns   : APIRET - OS/2 error code                                   ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */


APIRET GetPalette
 (
     PICONINFO piconinfo,
     PBYTE pbData,
     ULONG ulBuflen
)

{
    APIRET rc = NO_ERROR;

    PVOID pvIconData = (piconinfo) ? piconinfo->pIconData : NULL;

    PBITMAPFILEHEADER pbfh = pvIconData;
    PBITMAPFILEHEADER2 pbfh2 = pvIconData;

    PBITMAPINFOHEADER pbih = &pbfh->bmp;
    PBITMAPINFOHEADER2 pbih2 = &pbfh2->bmp2;

    ULONG ulMaskHeaderSize = pbfh->cbSize;
    ULONG ulHeaderSize = pbfh->cbSize;

    USHORT usSignature;

    BOOL fUseOldHeader = (ulHeaderSize == sizeof(BITMAPFILEHEADER));
    BOOL fIsColorPointer;

    USHORT cx = 0, cy = 0;
    PUSHORT pxHostspot;
    USHORT cPlanes, cBitCount;

    ULONG ulPelsInPointer;
    ULONG ulBitsPerPel;
    ULONG ulPointerColors;
    ULONG ulBwTableLength;
    ULONG ulColorTableLength;
    ULONG ulPlaneSize;
    ULONG ulColorPlanesLen;

    PVOID pvColorTable;

    ULONG i;
    PRGB prgb;
    ULONG ulColorValue;

    do
    {

        // check parameters
        if (pvIconData == NULL)
        {
            rc = ERROR_INVALID_PARAMETER;
            break;
        }

        // check signatures
        usSignature = (fUseOldHeader) ? pbfh->usType : pbfh2->usType;
        if ((usSignature != BFT_POINTER) && (usSignature != BFT_COLORPOINTER))
        {
            rc = ERROR_INVALID_DATA;
            break;
        }

        // for color pointer: adress color bitmap file header
        if (fUseOldHeader)
        {
            pbfh = (PBITMAPFILEHEADER)((PBYTE) pbfh +
                ulHeaderSize +
                (sizeof(RGB) * 2));
            // determine size and flip hotspot
            ulHeaderSize = pbfh->cbSize;
        }
        else
        {
            pbfh2 = (PBITMAPFILEHEADER2)((PBYTE) pbfh2 +
                ulHeaderSize +
                (sizeof(RGB2) * 2));
            // determine size and flip hotspot
            ulHeaderSize = pbfh2->cbSize;
        }

        ulPelsInPointer = cx * cy;
        cPlanes = ((fUseOldHeader) ? pbfh->bmp.cPlanes : pbfh2->bmp2.cPlanes);
        cBitCount = ((fUseOldHeader) ? pbfh->bmp.cBitCount : pbfh2->bmp2.cBitCount);
        ulBitsPerPel = cPlanes * cBitCount;
        ulPointerColors = 1 << ulBitsPerPel;
        ulBwTableLength = 2 * ((fUseOldHeader) ? sizeof(RGB) : sizeof(RGB2));
        ulColorTableLength = ulPointerColors * ((fUseOldHeader) ? sizeof(RGB) : sizeof(RGB2));
        ulPlaneSize = ulPelsInPointer / 8;
        ulColorPlanesLen = ulPlaneSize * cBitCount;

        // calculate offsets of mask and bitmap data
        pvColorTable = (PBYTE) pvIconData +
            ulMaskHeaderSize +  // mask bitmap file header
             ulBwTableLength +  // bw color table
             ulHeaderSize;      // color bitmap file header
        // color table is here


        // transform to RGB2 color table
        if (fUseOldHeader)
        {

        }

    }
    while (FALSE);

    return (rc);        // V0.9.3 (2000-05-21) [umoeller]

}
