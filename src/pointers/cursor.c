
/*
 *@@sourcefile cursor.c:
 *
 *      This file is ALL new with V0.9.4.
 *
 *@@added V0.9.4 [umoeller]
 *@@header "pointers\cursor.h"
 */

/*
 *      Copyright (C) 1996-2000 Christian Langanke.
 *      Copyright (C) 2000 Ulrich Mîller.
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

#include "pointers\cursor.h"
#include "pointers\info.h"
#include "pointers\mptrutil.h"
#include "pointers\debug.h"
#include "pointers\macros.h"
#include "pointers\eas.h"

// copied (!) from mptranim.h
#define DEFAULT_ANIMATION_TIMEOUT                150L

// convert JIF/MS
#define JIF2MS(j) (j * 1000 / 60)
#define MS2JIF(m) (m * 60 / 1000)

// -----------------------------------------------------------

// layout b&w Pointer
//                   bfhMask.cbSize           +    // bitmap file header
//                   sizeof( argb)            +    // bw color table
//                   2 * ulPlaneSize;              // AND and XOR mask bitmap data
//
// layout color Pointer
//                   bfhMask.cbSize           +    // mask bitmap file header
//                   sizeof( argb)            +    // bw color table
//                   bfh.cbSize               +    // color bitmap file header
//                   ulTargetColorTableLength +    // color table
//                   (2 * ulPlaneSize)        +    // AND and XOR mask bitmap data
//                   ulColorPlanesLen;             // color bitmap data

// layout b&w cursor
//                   CURFILEHEADER                 cursor file header
//                   CURFILERES                    cursor file resource
//                   DIB Data                      device independent bitmap data
//                     BITMAPINFOHEADER2 (-24)     - info header
//                     bw color table                 - color table
//                     and mask                    - and mask data
//                     xor mask                    - xor mask  data

// layout color cursor
//                   CURFILEHEADER                 cursor file header
//                   CURFILERES                    cursor file resource
//                   DIB Data                      device independent bitmap data
//                     BITMAPINFOHEADER2 (-24)     - info header
//                     color table                 - color table
//                     color bitmap                - color planes
//                     and mask                    - mask data

// remarks on ANI file:
// info list len must be an even number !!!

// -----------------------------------------------------------

typedef struct _CURFILEHEADER
{
    USHORT usReserved;          // Always 0

    USHORT usResourceType;      // 2: cursor // do not check, may be 0 !!

    USHORT usResourceCount;     // icons in the file

}
CURFILEHEADER, *PCURFILEHEADER;

typedef struct _CURFILERES
{
    BYTE bWidth;
    BYTE bHeight;
    BYTE bColorCount;           // # colors in image (2, 8, or 16)

    BYTE bReserved1;
    USHORT usXHotspot;
    USHORT usYHotspot;
    ULONG ulDIBSize;
    ULONG ulDIBOffset;
}
CURFILERES, *PCURFILERES;


#define AF_ICON     0x0001L     // Windows format icon/cursor animation

// -----------------------------------------------------------

typedef struct _SIG
{
    CHAR szSignature[4];
    ULONG cbSizeof;
}
SIG, *PSIG;

#define SIGSIZE     (LONG)sizeof( SIG)
#define SUBSIGSIZE  4
#define INFOSIZE(s) (SIGSIZE + strlen(s) + 1)

#define SIGSEGMSIZE(s) (s.cbSizeof)
#define PSIGSEGMSIZE(s) (s->cbSizeof)

#define CMPSIG(s,str) (strncmp( s.szSignature, str, sizeof( s.szSignature)))

#define SETSIG(s, str)   (memcpy( s.szSignature, str, sizeof( s.szSignature)))
#define SETPSIG(s, str)  (memcpy( s->szSignature, str, sizeof( s->szSignature)))
#define SETSIGSIZE(s, l) (s.cbSizeof = l)
#define SETPSIGSIZE(s, l) (s->cbSizeof = l)

// -----------------------------------------------------------

// RIFF keywords

static PSZ pszSigRiff = "RIFF";
static PSZ pszSigAcon = "ACON"; // animated Icon :-)

static PSZ pszSigList = "LIST";
static PSZ pszSigInfo = "INFO";
static PSZ pszSigInfoName = "INAM";
static PSZ pszSigInfoArtist = "IART";
static PSZ pszSigDisp = "DISP";
static PSZ pszSigAniHeader = "anih";
static PSZ pszSigRate = "rate";
static PSZ pszSigSeq = "seq ";
static PSZ pszSigFrame = "fram";
static PSZ pszSigIcon = "icon";

static PSZ pszDefaultInfo = "?";

// -----------------------------------------------------------

// RIFF macros
// read directly from file
#define READNEXTSIG(s)                  \
{                                       \
rc = DosRead( hfileAnimation,           \
              &s,                       \
              sizeof( s),               \
              &ulBytesRead);            \
if (rc != NO_ERROR)                     \
   break;                               \
if (!ulBytesRead)                       \
   break;                               \
if (s.szSignature[ 0] == 0)             \
   {                                    \
   MOVEFILEPTR( hfileAnimation,         \
                - SIGSIZE + 1);         \
   rc = DosRead( hfileAnimation,        \
                 &s,                    \
                 sizeof( s),            \
                 &ulBytesRead);         \
   if (rc != NO_ERROR)                  \
      break;                            \
   if (!ulBytesRead)                    \
      break;                            \
   }                                    \
                                        \
QUERYFILEPTR(  hfileAnimation);         \
}rc                                     \

#define READNEXTSIMPLESIG(s)            \
{                                       \
rc = DosRead( hfileAnimation,           \
              &s.szSignature,           \
              sizeof( s.szSignature),   \
              &ulBytesRead);            \
if (rc != NO_ERROR)                     \
   break;                               \
if (!ulBytesRead)                       \
   break;                               \
QUERYFILEPTR(  hfileAnimation);         \
}rc                                     \

#define SKIPTONEXTSIG(s)                \
{                                       \
rc = DosSetFilePtr( hfileAnimation,     \
                    SIGSEGMSIZE(s),     \
                    FILE_CURRENT,       \
                    &ulFilePtr);        \
if (rc != NO_ERROR)                     \
   break;                               \
QUERYFILEPTR(  hfileAnimation);         \
}rc                                     \


// -----------------------------------------------------------

// write SIG
#define WRITESIG(s)                     \
{                                       \
rc = DosWrite( hfileAnimation,          \
               &s,                      \
               sizeof( s),              \
               &ulBytesWritten);        \
if (rc != NO_ERROR)                     \
   break;                               \
}rc                                     \

// write simple SIG
#define WRITESIMPLESIG(s)               \
{                                       \
rc = DosWrite( hfileAnimation,          \
               &s.szSignature,          \
               sizeof( s.szSignature),  \
               &ulBytesWritten);        \
if (rc != NO_ERROR)                     \
   break;                               \
}rc                                     \


// ###########################################################################

/*⁄ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒø
 *≥ Name      : _CreatePointerFromCursor                                   ≥
 *≥ Comment   :                                                            ≥
 *≥ Author    : C.Langanke                                                 ≥
 *≥ Date      : 27.06.1996                                                 ≥
 *≥ Update    : 27.06.1996                                                 ≥
 *≥ called by : app                                                        ≥
 *≥ calls     : Dos*                                                       ≥
 *≥ Input     : ###                                                        ≥
 *≥ Tasks     : - read win cursor file                                     ≥
 *≥             - convert to color pointer                                 ≥
 *≥ returns   : APIRET - OS/2 error code                                   ≥
 *¿ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒŸ
 */
APIRET _CreatePointerFromCursor
 (
     PSZ pszCursorFileName,
     ULONG ulFileOffset,
     PVOID pvIconData,
     ULONG ulDataLen,
     PULONG pulBytesReturned
)
{

    APIRET rc = NO_ERROR;
    BOOL fBwPointer;
    ULONG i, j;
    ULONG ulNeededSize;

    HFILE hfileCursor = NULLHANDLE;
    ULONG ulBytesToRead, ulBytesRead, ulAction, ulFilePtr;

    PBYTE pbCursorData = NULL;
    PBYTE pbPointerData = NULL;
    ULONG ulSourceColorTableLength, ulTargetColorTableLength, ulTargetBwTableLength,
     ulColorPlanesLen;


    CURFILEHEADER curfileheader;
    CURFILERES curfileres;
    PBITMAPINFOHEADER2 pbih2DIB;

    BITMAPFILEHEADER bfhMask;
    BITMAPFILEHEADER bfh;
    ULONG ulPlaneSize;
    ULONG ulBitsPerPel, ulPointerColors, ulPelsInPointer;
    ULONG ulDataWritten = 0;

    RGB argb[2] =
    {
        {0, 0, 0},
        {255, 255, 255}};       // bw color table

    PBYTE pbColorData = NULL;
    PBYTE pbTargetColorData = NULL;
    PRGB prgb;
    PRGB2 prgb2;

    PBYTE pbXORMask = NULL;
    PBYTE pbANDMask = NULL;
    PBYTE pbColorPlanes = NULL;

    do
    {
        // check parameters
        if ((pszCursorFileName == NULL) ||
            (*pszCursorFileName == 0) ||
            (pulBytesReturned == NULL))
        {
            rc = ERROR_INVALID_PARAMETER;
            break;
        }

#ifdef DEBUG
        memset(pvIconData, 0xff, ulDataLen);
#endif

        // Open and read the .CUR file header and the first ICONFILERES
        rc = DosOpen(pszCursorFileName,
                     &hfileCursor,
                     &ulAction,
                     0,
                     0,
                     OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS,
                     OPEN_SHARE_DENYWRITE | OPEN_ACCESS_READONLY,
                     NULL);
        if (rc != NO_ERROR)
            break;

        // position to the file offset given
        rc = DosSetFilePtr(hfileCursor,
                           ulFileOffset,
                           FILE_BEGIN,
                           &ulFilePtr);
        if (rc != NO_ERROR)
            break;

        // read file header
        rc = DosRead(hfileCursor,
                     &curfileheader,
                     sizeof(CURFILEHEADER),
                     &ulBytesRead);
        if (rc != NO_ERROR)
            break;

        // read file resource info
        rc = DosRead(hfileCursor,
                     &curfileres,
                     sizeof(CURFILERES),
                     &ulBytesRead);
        if (rc != NO_ERROR)
            break;

        // check if data is valid
        if (curfileres.bReserved1 != 0)
        {
            rc = ERROR_INVALID_DATA;
            break;
        }

        // allocate cursor buffer size
        if ((pbCursorData = malloc(curfileres.ulDIBSize)) == NULL)
        {
            rc = ERROR_NOT_ENOUGH_MEMORY;
            break;
        }

        // now read the DIB data
        rc = DosSetFilePtr(hfileCursor,
                           curfileres.ulDIBOffset + ulFileOffset,
                           FILE_BEGIN,
                           &ulFilePtr);
        if (rc != NO_ERROR)
            break;

        // read file resource info
        ulBytesRead = curfileres.ulDIBSize;
        rc = DosRead(hfileCursor,
                     pbCursorData,
                     ulBytesRead,
                     &ulBytesRead);
        if (rc != NO_ERROR)
            break;

        // erste Pointer initialisieren
        pbih2DIB = (PBITMAPINFOHEADER2)pbCursorData;
        fBwPointer = (pbih2DIB->cBitCount == 1);

        //
        // now create header infos
        //

        // header for mask bits
        memset(&bfhMask, 0, sizeof(bfhMask));
        memset(&bfh, 0, sizeof(bfh));
        bfhMask.usType = fBwPointer ? BFT_POINTER : BFT_COLORPOINTER;
        bfhMask.xHotspot = curfileres.usXHotspot;
        bfhMask.yHotspot = curfileres.bHeight - curfileres.usYHotspot;
        bfhMask.cbSize = sizeof(bfh);

        memset(&bfhMask.bmp, 0, sizeof(bfhMask.bmp));
        bfhMask.bmp.cbFix = sizeof(bfhMask.bmp);
        bfhMask.bmp.cx = (ULONG) curfileres.bHeight;    // for XOR and AND Mask

        bfhMask.bmp.cy = (ULONG) curfileres.bWidth * 2;
        bfhMask.bmp.cPlanes = 1;
        bfhMask.bmp.cBitCount = 1;

        // header for color planes
        memset(&bfh, 0, sizeof(bfh));
        bfh.usType = BFT_COLORPOINTER;
        bfh.xHotspot = curfileres.usXHotspot;
        bfh.yHotspot = curfileres.bHeight - curfileres.usYHotspot;
        bfh.cbSize = sizeof(bfh);

        memset(&bfh.bmp, 0, sizeof(bfh.bmp));
        bfh.bmp.cbFix = sizeof(bfh.bmp);
        bfh.bmp.cx = (ULONG) curfileres.bHeight;
        bfh.bmp.cy = (ULONG) curfileres.bWidth;
        bfh.bmp.cPlanes = pbih2DIB->cPlanes;
        bfh.bmp.cBitCount = pbih2DIB->cBitCount;

        // Variablen fÅr Zielbitmap
        ulPelsInPointer = bfh.bmp.cx * bfh.bmp.cy;
        ulBitsPerPel = bfh.bmp.cPlanes * bfh.bmp.cBitCount;
        ulPointerColors = 1 << ulBitsPerPel;
        ulTargetBwTableLength = 2 * sizeof(RGB);
        ulTargetColorTableLength = ulPointerColors * sizeof(RGB);
        ulPlaneSize = ulPelsInPointer / 8;
        ulColorPlanesLen = ulPlaneSize * bfh.bmp.cBitCount;

        // Variablen fÅr Quell-DIB
        ulSourceColorTableLength = ulPointerColors * sizeof(RGB2);
        pbColorData = pbCursorData + pbih2DIB->cbFix;
        pbColorPlanes = pbCursorData + pbih2DIB->cbFix + ulSourceColorTableLength;

        // Datenoffsets berechnen
        if (fBwPointer)
            bfhMask.offBits = bfhMask.cbSize +
                ulTargetBwTableLength;
        else
            bfhMask.offBits = bfhMask.cbSize +
                ulTargetBwTableLength +
                bfh.cbSize +
                ulTargetColorTableLength;
        bfh.offBits = bfhMask.offBits + (2 * ulPlaneSize);

        // --- Platzberechnung

        // calculate needed size
        if (fBwPointer)
            ulNeededSize = bfhMask.cbSize +     // mask bitmap header
                 sizeof(argb) + // bw color table
                 2 * ulPlaneSize;   // AND and XOR mask bitmap data

        else
            ulNeededSize = bfhMask.cbSize +     // mask bitmap header
                 sizeof(argb) + // bw color table
                 bfh.cbSize +   // color bitmap header
                 ulTargetColorTableLength +     // color table
                 (2 * ulPlaneSize) +    // AND and XOR mask bitmap data
                 ulColorPlanesLen;  // color bitmap data

        *pulBytesReturned = ulNeededSize;
        if ((ulDataLen < ulNeededSize) || (pvIconData == NULL))
        {
            rc = ERROR_MORE_DATA;
            break;
        }

        // --- Konvertierungen und Datenbereiche ----

        if (!fBwPointer)
        {
            // Speicher fÅr Colortable holen
            pbTargetColorData = malloc(sizeof(RGB) * ulPointerColors);

            // Colortable anlegen
            if (pbTargetColorData == NULL)
            {
                rc = ERROR_NOT_ENOUGH_MEMORY;
                break;
            }
            // und konvertieren
            for (i = 0, prgb2 = (RGB2*)pbColorData, prgb = (RGB*)pbTargetColorData;
                 i < ulPointerColors;
                 i++, prgb2++, prgb++)
            {
                memcpy(prgb, prgb2, sizeof(RGB));
            }

            // XOR Mask Daten erstellen
            pbXORMask = malloc(ulPlaneSize);
            if (pbXORMask == NULL)
            {
                rc = ERROR_NOT_ENOUGH_MEMORY;
                break;
            }
            memset(pbXORMask, 0, ulPlaneSize);

        }                       // end if (!fBwPointer)

        // Adresse der AND Mask ermitteln
        pbANDMask = pbCursorData + pbih2DIB->cbFix + ulSourceColorTableLength + (pbih2DIB->cBitCount * ulPlaneSize);

        // scratch target memory
        pbPointerData = pvIconData;

        if (fBwPointer)
        {

            // -------------------
            // mask header and bw color table
            // -------------------

            // write mask header
            MEMCOPY(pbPointerData, &bfhMask, bfhMask.cbSize);

            // write bw color table
            MEMCOPY(pbPointerData, argb, sizeof(argb));

            // -------------------
            // "color" plane contains whole data
            // -------------------
            MEMCOPY(pbPointerData, pbColorPlanes, ulColorPlanesLen * 2);

        }
        else
        {
            // -------------------
            // mask header and bw color table
            // -------------------

            // write mask header
            MEMCOPY(pbPointerData, &bfhMask, bfhMask.cbSize);

            // write bw color table
            MEMCOPY(pbPointerData, argb, sizeof(argb));

            // -------------------
            // color header and color table
            // -------------------

            // write color header
            MEMCOPY(pbPointerData, &bfh, bfh.cbSize);

            // write color table
            MEMCOPY(pbPointerData, pbTargetColorData, ulTargetColorTableLength);

            // -------------------
            // mask XOR and AND plane
            // -------------------

            // write XOR mask
            MEMCOPY(pbPointerData, pbXORMask, ulPlaneSize);

            // write AND mask
            MEMCOPY(pbPointerData, pbANDMask, ulPlaneSize);

            // -------------------
            // color plane
            // -------------------
            MEMCOPY(pbPointerData, pbColorPlanes, ulColorPlanesLen);

        }                       // end if (!fBwPointer)


    }
    while (FALSE);

// cleanup
    if (pbTargetColorData)
        free(pbTargetColorData);
    if (pbXORMask)
        free(pbXORMask);

    if (pbCursorData)
        free(pbCursorData);
    if (hfileCursor)
        DosClose(hfileCursor);

    return rc;

}

/*⁄ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒø
 *≥ Name      : _CreateCursorFromPointer                                   ≥
 *≥ Comment   :                                                            ≥
 *≥ Author    : C.Langanke                                                 ≥
 *≥ Date      : 15.03.1998                                                 ≥
 *≥ Update    : 15.03.1998                                                 ≥
 *≥ called by : app                                                        ≥
 *≥ calls     : Dos*                                                       ≥
 *≥ Input     : ###                                                        ≥
 *≥ Tasks     : - convert pointer to cursor                                ≥
 *≥ returns   : APIRET - OS/2 error code                                   ≥
 *¿ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒŸ
 */

APIRET _CreateCursorFromPointer
 (
     PVOID pvIconData,
     ULONG ulconDataLen,
     PVOID pvCursorData,
     ULONG ulDataLen,
     PULONG pulBytesReturned
)
{
    APIRET rc = NO_ERROR;

    ULONG i, j;
    ULONG ulNeededSize;

    PBITMAPFILEHEADER pbfhMask;
    PBITMAPFILEHEADER2 pbfhMask2;
    PBITMAPFILEHEADER pbfh;
    PBITMAPFILEHEADER2 pbfh2;
    BOOL fIsOldHeader;
    BOOL fBwPointer;
    ULONG ulMaskHeaderLen, ulColorHeaderLen, ulHostspotX, ulHostspotY, ulSizeX,
     ulSizeY, ulPlanes, ulBitsPerPlane;
    ULONG ulPlaneSize, ulBitsPerPel, ulPointerColors, ulPelsInPointer, ulColorTableLength,
     ulBwTableLength, ulColorPlanesLen;
    ULONG ulDIBSize, ulDIBHeaderLen, ulCurHeaderSize, ulTargetColorTableLen,
     ulAndMaskPlaneLen;

    CURFILEHEADER curfileheader;
    CURFILERES curfileres;
    BITMAPINFOHEADER2 bih2DIB;

    RGB2 argb[2] =
    {
        {0, 0, 0, 0},
        {255, 255, 255, 0}};    // bw color table

    PBYTE pbColorTable = NULL;
    PBYTE pbTargetColorTable = NULL;
    PRGB prgb;
    PRGB2 prgb2;

    PBYTE pbColorPlanes;
    PBYTE pbBwPlanes;

    PBYTE pbCursorData;

    do
    {
        // Parameter prÅfen
        if ((pvIconData == NULL) ||
            (ulconDataLen == 0) ||
            (pulBytesReturned == NULL))
        {
            rc = ERROR_INVALID_PARAMETER;
            break;
        }

#ifdef DEBUG
        if (pvCursorData)
            memset(pvCursorData, '#', ulDataLen);
#endif

        // IDs prÅfen und Grî·e bestimmen

        pbfhMask = pvIconData;
        pbfhMask2 = pvIconData;
        if ((pbfhMask->usType != BFT_POINTER) &&
            (pbfhMask->usType != BFT_COLORPOINTER))
        {
            rc = ERROR_INVALID_DATA;
            break;
        }
        fBwPointer = pbfhMask->usType == BFT_POINTER;
        ulMaskHeaderLen = pbfhMask->cbSize;
        fIsOldHeader = (ulMaskHeaderLen == sizeof(BITMAPFILEHEADER));

        // nur 32x32 zulassen
        ulSizeX = (fIsOldHeader) ? (ULONG) pbfhMask->bmp.cx : pbfhMask2->bmp2.cx;
        ulSizeY = (fIsOldHeader) ? (ULONG) pbfhMask->bmp.cy / 2 : pbfhMask2->bmp2.cy / 2;
        ulHostspotX = (ULONG) pbfhMask->xHotspot;
        ulHostspotY = (ULONG) pbfhMask->yHotspot;
        if ((ulSizeX != 32) || (ulSizeY != 32))
        {
            rc = ERROR_INVALID_DATA;
            break;
        }

        // Zeiger auf Info-Header fÅr die Farbbitmap holen
        ulBwTableLength = 2 * ((fIsOldHeader) ? sizeof(RGB) : sizeof(RGB2));
        if (fBwPointer)
        {
            ulPlanes = (fIsOldHeader) ? (ULONG) pbfhMask->bmp.cPlanes : pbfhMask2->bmp2.cPlanes;
            ulBitsPerPlane = (fIsOldHeader) ? (ULONG) pbfhMask->bmp.cBitCount : pbfhMask2->bmp2.cBitCount;
        }
        else
        {
            pbfh = (PBITMAPFILEHEADER)((PBYTE) pbfhMask + ulMaskHeaderLen + ulBwTableLength);
            pbfh2 = (PBITMAPFILEHEADER2)((PBYTE) pbfhMask2 + ulMaskHeaderLen + ulBwTableLength);
            ulColorHeaderLen = (fIsOldHeader) ? pbfh->cbSize : pbfh2->cbSize;
            ulPlanes = (fIsOldHeader) ? (ULONG) pbfh->bmp.cPlanes : pbfh2->bmp2.cPlanes;
            ulBitsPerPlane = (fIsOldHeader) ? (ULONG) pbfh->bmp.cBitCount : pbfh2->bmp2.cBitCount;
        }

        // Variablen fÅr Quellbitmap
        ulPelsInPointer = ulSizeX * ulSizeY;
        ulBitsPerPel = ulPlanes * ulBitsPerPlane;
        ulPointerColors = 1 << ulBitsPerPel;
        ulColorTableLength = ulPointerColors * ((fIsOldHeader) ? sizeof(RGB) : sizeof(RGB2));
        ulPlaneSize = ulPelsInPointer / 8;
        ulColorPlanesLen = ulPlaneSize * ulBitsPerPlane;
        ulAndMaskPlaneLen = ulPlaneSize;

        ulTargetColorTableLen = ulPointerColors * sizeof(RGB2);
        ulDIBHeaderLen = (PBYTE) & bih2DIB.usUnits - (PBYTE) & bih2DIB;


        // nur maximal 16 Farben zulassen
        if (ulPointerColors > 16)
        {
            rc = ERROR_INVALID_DATA;
            break;
        }

        // --- Platzberechnung
        if (fBwPointer)
            ulDIBSize = ulDIBHeaderLen +
                ulTargetColorTableLen +
                ulAndMaskPlaneLen * 2;
        else
            ulDIBSize = ulDIBHeaderLen +
                ulTargetColorTableLen +
                ulColorPlanesLen +
                ulAndMaskPlaneLen;

        ulCurHeaderSize = sizeof(CURFILEHEADER) +
            sizeof(CURFILERES);
        ulNeededSize = ulCurHeaderSize + ulDIBSize;

        *pulBytesReturned = ulNeededSize;
        if ((ulDataLen < ulNeededSize) || (pvIconData == NULL))
        {
            rc = ERROR_MORE_DATA;
            break;
        }

        // Header vorbereiten
        curfileheader.usReserved = 0;
        curfileheader.usResourceType = 2;
        curfileheader.usResourceCount = 1;

        curfileres.bHeight = (BYTE) ulSizeX;
        curfileres.bWidth = (BYTE) ulSizeY;
        curfileres.bColorCount = ulPointerColors;
        curfileres.bReserved1 = 0;
        curfileres.usXHotspot = ulHostspotX;
        curfileres.usYHotspot = ulSizeX - ulHostspotY;
        curfileres.ulDIBSize = ulDIBSize;
        curfileres.ulDIBOffset = ulCurHeaderSize;

        // nicht alle Felder von BITMAPINFOHEADER2 verwenden
        memset(&bih2DIB, 0, ulDIBHeaderLen);
        bih2DIB.cbFix = ulDIBHeaderLen;
        bih2DIB.cx = ulSizeX;
        bih2DIB.cy = ulSizeY * 2;
        bih2DIB.cPlanes = ulPlanes;
        bih2DIB.cBitCount = ulBitsPerPlane;
        bih2DIB.cbImage = ulAndMaskPlaneLen + ulColorPlanesLen;

        // ggfs. Farbtabelle konvertieren
        if (fBwPointer)
        {
            // einfach BW Tabelle und Daten nehmen
            pbColorTable = (PBYTE)argb;
            pbBwPlanes = (PBYTE) pvIconData +
                ulMaskHeaderLen +
                ulBwTableLength;
            bih2DIB.cbImage = ulAndMaskPlaneLen * 2;
        }
        else
        {
            // mu· konvertiert werden ?
            pbColorTable = (PBYTE) pvIconData +
                ulMaskHeaderLen +
                ulBwTableLength +
                ulColorHeaderLen;

            pbBwPlanes = (PBYTE) pbColorTable +
                ulColorTableLength +
                ulAndMaskPlaneLen;

            pbColorPlanes = (PBYTE) pbBwPlanes +
                ulAndMaskPlaneLen;

            if (fIsOldHeader)
            {
                // Speicher fÅr Colortable holen
                pbTargetColorTable = malloc(sizeof(RGB2) * ulPointerColors);

                // Colortable anlegen
                if (pbTargetColorTable == NULL)
                {
                    rc = ERROR_NOT_ENOUGH_MEMORY;
                    break;
                }
                // und konvertieren
                for (i = 0, prgb = (RGB*)pbColorTable, prgb2 = (RGB2*)pbTargetColorTable;
                     i < ulPointerColors;
                     i++, prgb2++, prgb++)
                {
                    memcpy(prgb2, prgb, sizeof(RGB));
                }

                // neue Adresse und Grî·e bestimmen
                ulColorTableLength = ulPointerColors * sizeof(RGB2);
                pbColorTable = pbTargetColorTable;
            }
        }

        // scratch target memory
        pbCursorData = pvCursorData;

        // -------------------
        // Header
        // -------------------

        // write cursor file header
        MEMCOPY(pbCursorData, &curfileheader, sizeof(curfileheader));

        // write cursor resource header
        MEMCOPY(pbCursorData, &curfileres, sizeof(curfileres));

        // -------------------
        // DIB Data
        // -------------------

        // write BITMAPINFOHEADER2
        MEMCOPY(pbCursorData, &bih2DIB, ulDIBHeaderLen);

        if (fBwPointer)
        {
            // write bw color table
            MEMCOPY(pbCursorData, argb, sizeof(argb));

            // write AND and XOR bw plane
            MEMCOPY(pbCursorData, pbBwPlanes, ulAndMaskPlaneLen * 2);
        }
        else
        {
            // write color table
            MEMCOPY(pbCursorData, pbColorTable, ulColorTableLength);

            // write color bitmap
            MEMCOPY(pbCursorData, pbColorPlanes, ulColorPlanesLen);

            // write AND bw plane
            MEMCOPY(pbCursorData, pbBwPlanes, ulAndMaskPlaneLen);

        }

    }
    while (FALSE);

// cleanup
    if (pbTargetColorTable)
        free(pbTargetColorTable);
    return rc;
}

/*⁄ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒø
 *≥ Name      : _ReadWinAnimationValues                                    ≥
 *≥ Comment   :                                                            ≥
 *≥ Author    : C.Langanke                                                 ≥
 *≥ Date      : 27.06.1996                                                 ≥
 *≥ Update    : 27.06.1996                                                 ≥
 *≥ called by : app                                                        ≥
 *≥ calls     : Dos*                                                       ≥
 *≥ Input     : ###                                                        ≥
 *≥ Tasks     : - read win animation file                                  ≥
 *≥             - return values and file offsets                           ≥
 *≥ returns   : APIRET - OS/2 error code                                   ≥
 *¿ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒŸ
 */
APIRET _ReadWinAnimationValues
 (
     PSZ pszAnimationFileName,
     PULONG paulTimeout,
     PICONINFO paiconinfo,
     PULONG pulEntries
)
{

    APIRET rc = NO_ERROR;
    ULONG i, j;
    HFILE hfileAnimation = NULLHANDLE;
    ULONG ulAction;
    ULONG ulBytesRead, ulFilePtr;
    SIG sig;
    ANIHEADER aniheader;

    ULONG ulPointerCount;
    ULONG ulEntries;
    ULONG ulSeqEntries;
    ULONG ulFrames;
    ULONG ulNextSig;
    PULONG paulSequence = NULL;

    ULONG fUseDefaultTimeout = TRUE;

    PICONINFO piconinfo;

    do
    {
        // check parameters
        if ((pszAnimationFileName == NULL) ||
            (*pszAnimationFileName == 0) ||
            (paulTimeout == NULL) ||
            (paiconinfo == NULL) ||
            (pulEntries == NULL))
        {
            rc = ERROR_INVALID_PARAMETER;
            break;
        }

        // Open and read the .CUR file header and the first ICONFILERES
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

        // RIFF Sig lesen
        READNEXTSIG(sig);
        if (CMPSIG(sig, pszSigRiff))
        {
            rc = ERROR_INVALID_DATA;
            break;
        }

        // ACON Sig lesen
        READNEXTSIMPLESIG(sig);
        if (CMPSIG(sig, pszSigAcon))
        {
            rc = ERROR_INVALID_DATA;
            break;
        }

        // jetzt alles lesen, bis der ANIHEADER kommt
        READNEXTSIG(sig);
        while (CMPSIG(sig, pszSigAniHeader))
        {
            // zum nÑchsten springen
            SKIPTONEXTSIG(sig);
            READNEXTSIG(sig);
        }

        // bei Fehler Abbruch
        if (rc != NO_ERROR)
            break;

        // Animation Header lesen
        rc = DosRead(hfileAnimation,
                     &aniheader,
                     sizeof(aniheader),
                     &ulBytesRead);
        if (rc != NO_ERROR)
            break;


        // Format prÅfen
        if (!(aniheader.fl & AF_ICON))
        {
            rc = ERROR_INVALID_DATA;
            break;
        }


// DEBUGMSG( ">>> file %s" NEWLINE, pszAnimationFileName);

        // mindestens fÅr Anzahl der physisch vorhandenen Icons verwenden
        ulEntries = aniheader.cFrames;

        // nÑchstes sig lesen
        READNEXTSIG(sig);

        // rate tabelle lesen
        if (CMPSIG(sig, pszSigRate) == 0)
        {
            // Default nicht verwenden
            fUseDefaultTimeout = FALSE;

            // rate Index gefunden: sind genÅgend EintrÑge vorhanden ?
            // hier steht die Anzahl der Frames fest, wenn einige mehrfach
            // verwendet werden.
            ulFrames = SIGSEGMSIZE(sig) / sizeof(ULONG);
            if (ulFrames < *pulEntries)
                ulEntries = ulFrames;
            else
                ulEntries = *pulEntries;

            QUERYFILEPTR(hfileAnimation);
//    DEBUGMSG( "--- read rate table at %u" NEWLINE, ulFilePtr);

            // Tabelle lesen
            rc = DosRead(hfileAnimation,
                         paulTimeout,
                         ulEntries * sizeof(ULONG),
                         &ulBytesRead);
            if (rc != NO_ERROR)
                break;

            // weitere Daten Åberspringen
            if (ulFrames > *pulEntries)
            {
                ULONG ulEntriesToSkip = ulFrames - ulEntries;

                QUERYFILEPTR(hfileAnimation);
//       DEBUGMSG( "--- skip %u ULONGs at %u" NEWLINE, ulEntriesToSkip _c_ ulFilePtr);

                SKIPULONG(hfileAnimation, ulEntriesToSkip);
            }

            // Zahl wieder anpassen
            ulEntries = ulFrames;

            // nÑchstes sig lesen
            READNEXTSIG(sig);
        }


        QUERYFILEPTR(hfileAnimation);
// DEBUGMSG( "--- continue at %u" NEWLINE, ulFilePtr);

        // seq auswerten
        if (CMPSIG(sig, pszSigSeq) == 0)
        {
            // seq Index gefunden: sind genÅgend EintrÑge vorhanden ?
            ulSeqEntries = SIGSEGMSIZE(sig) / sizeof(ULONG);

            // Speicher fÅr Sequence
            if ((paulSequence = malloc(ulSeqEntries * sizeof(ULONG))) == NULL)
            {
                rc = ERROR_NOT_ENOUGH_MEMORY;
                break;
            }

            // Tabelle lesen
            rc = DosRead(hfileAnimation,
                         paulSequence,
                         ulSeqEntries * sizeof(ULONG),
                         &ulBytesRead);
            if (rc != NO_ERROR)
                break;

            // nÑchstes sig lesen
            READNEXTSIG(sig);
        }

        // ggfs. Default Timeout eintragen
        if (fUseDefaultTimeout)
            for (i = 0; i < *pulEntries; i++)
            {
                *(paulTimeout + i) = aniheader.jifRate;
            }

        // LIST und fram auswerten
        if (CMPSIG(sig, pszSigList))
        {
            rc = ERROR_INVALID_DATA;
            break;
        }
        READNEXTSIMPLESIG(sig);
        if (CMPSIG(sig, pszSigFrame))
        {
            rc = ERROR_INVALID_DATA;
            break;
        }

        // framicon lesen
        for (i = 0; ((i < aniheader.cFrames) && (i < *pulEntries)); i++)
        {
            // nÑchstes sig lesen
            READNEXTSIG(sig);
            if (CMPSIG(sig, pszSigIcon))
            {
                rc = ERROR_INVALID_DATA;
                break;
            }

            // file ptr ermitteln
            QUERYFILEPTR(hfileAnimation);

            // je nach sequence Zielfeld ermitteln
            if (paulSequence)
            {
                for (j = 0; j < ulEntries; j++)
                {
                    if (*(paulSequence + j) == i)
                    {
                        piconinfo = paiconinfo + j;
                        piconinfo->cbIconData = ulFilePtr;
                        piconinfo->resid = SIGSEGMSIZE(sig);
                    }
                }
            }
            else
            {
                piconinfo = paiconinfo + i;
                piconinfo->cbIconData = ulFilePtr;
                piconinfo->resid = SIGSEGMSIZE(sig);
            }

            // zum nÑchsten springen
            SKIPTONEXTSIG(sig);
        }

        // bei Fehler Abbruch
        if (rc != NO_ERROR)
            break;

        // Anzahl Åbergeben
        if (ulEntries < *pulEntries)
            *pulEntries = ulEntries;

        // Timeout in ms umrechnen
        for (i = 0; i < *pulEntries; i++)
        {
            *(paulTimeout + i) = JIF2MS(*(paulTimeout + i));
        }

    }
    while (FALSE);

// cleanup
    if (hfileAnimation)
        DosClose(hfileAnimation);
    if (paulSequence)
        free(paulSequence);

    return rc;

}


// ###########################################################################

/*⁄ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒø
 *≥ Name      : LoadPointerFromCursorFile                                  ≥
 *≥ Kommentar : lÑdt Pointer aus Cursor-Datei                              ≥
 *≥             Es wird vorausgesetzt, das es sich um eine gÅltige         ≥
 *≥             Cursordatei handelt.                                       ≥
 *≥ Autor     : C.Langanke                                                 ≥
 *≥ Datum     : 26.03.1995                                                 ≥
 *≥ énderung  : 26.03.1995                                                 ≥
 *≥ aufgerufen: diverse                                                    ≥
 *≥ ruft auf  : -                                                          ≥
 *≥ Eingabe   : PSZ          - Name der Pointerdatei                       ≥
 *≥           : PHPOINTER    - Zielvariable fÅr PointerHandle              ≥
 *≥ Aufgaben  : - Pointer laden                                            ≥
 *≥ RÅckgabe  : BOOL - Flag: Pointer geladen/nicht geladen                 ≥
 *¿ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒŸ
 */

BOOL _Export LoadPointerFromCursorFile
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

    ULONG ulDataLen;
    PVOID pvData = NULL;
    ULONG ulFileOffset;
    CHAR szTimeout[20];

    do
    {

        // Parameter prÅfen
        if ((pszName == NULL) ||
            (pIconInfo == NULL))
            break;


        // Fileoffset wird in ICONINFO Åbergeben
        ulFileOffset = pIconInfo->cbIconData;

        // LÑnge des Speicherbereichs ermitteln
        rc = _CreatePointerFromCursor(pszName, ulFileOffset, NULL, 0, &ulDataLen);
        if (rc != ERROR_MORE_DATA)
            break;

        // Speicher holen
        if ((pvData = malloc(ulDataLen)) == NULL)
            break;

        // Cursor konvertieren
        rc = _CreatePointerFromCursor(pszName, ulFileOffset, pvData, ulDataLen, &ulDataLen);
        if (rc != NO_ERROR)
            break;

        // Daten initialisieren
        pIconInfo->cb = sizeof(ICONINFO);
        pIconInfo->fFormat = ICON_DATA;
        pIconInfo->cbIconData = ulDataLen;
        pIconInfo->pIconData = pvData;
        fSuccess = TRUE;

        if (phptr)
        {
            // Pointer erstellen
            hptr = CreatePtrFromIconInfo(pIconInfo);

            // Ergebnis zurÅckgeben
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


// cleanup on error)
    if (!fSuccess)
    {
        if (pvData)
            free(pvData);
    }

    return fSuccess;
}

/*⁄ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒø
 *≥ Name      : WriteCursorFile                                            ≥
 *≥ Comment   :                                                            ≥
 *≥ Author    : C.Langanke                                                 ≥
 *≥ Date      : 13.04.1998                                                 ≥
 *≥ Update    : 13.04.1998                                                 ≥
 *≥ called by : app                                                        ≥
 *≥ calls     : Dos*                                                       ≥
 *≥ Input     : ###                                                        ≥
 *≥ Tasks     : - write cursor file                                        ≥
 *≥ returns   : APIRET - OS/2 error code                                   ≥
 *¿ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒŸ
 */
APIRET WriteCursorFile
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

    PVOID pvData;
    ULONG ulDataLen;

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

        // in Cursor konvertieren: Speicherbereich ermitteln
        rc = _CreateCursorFromPointer(piconinfo->pIconData, piconinfo->cbIconData, NULL, 0, &ulDataLen);
        if (rc != ERROR_MORE_DATA)
            break;

        // Speicher holen
        if ((pvData = malloc(ulDataLen)) == NULL)
            break;

        // in Cursor konvertieren
        rc = _CreateCursorFromPointer(piconinfo->pIconData, piconinfo->cbIconData, pvData, ulDataLen, &ulDataLen);
        if (rc != NO_ERROR)
            break;

        // Datei îffnen
        rc = DosOpen(pszAnimationFileName,
                     &hfileAnimation,
                     &ulAction,
                     0, 0,
                     OPEN_ACTION_CREATE_IF_NEW | OPEN_ACTION_REPLACE_IF_EXISTS,
                     OPEN_SHARE_DENYREADWRITE | OPEN_ACCESS_WRITEONLY,
                     NULL);

        if (rc != NO_ERROR)
            break;

        WRITEMEMORY(hfileAnimation, pvData, ulDataLen);

        // write EAs
        _ltoa(ulTimeout, szValue, 10);
        if (ulTimeout)
            eahWriteStringEA(hfileAnimation, EANAME_TIMEFRAME, szValue);
        if (pszInfoName)
            eahWriteStringEA(hfileAnimation, EANAME_INFONAME, pszInfoName);
        if (pszInfoArtist)
            eahWriteStringEA(hfileAnimation, EANAME_INFOARTIST, pszInfoArtist);

        // set pointer as file icon
        DosClose(hfileAnimation);
        hfileAnimation = NULLHANDLE;

    }
    while (FALSE);

// cleanup
    if (hfileAnimation)
        DosClose(hfileAnimation);
    if (pvData)
        free(pvData);
    return rc;

}

/*⁄ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒø
 *≥ Name      : QueryCursorFileDetails                                     ≥
 *≥ Comment   :                                                            ≥
 *≥ Author    : C.Langanke                                                 ≥
 *≥ Date      : 27.06.1996                                                 ≥
 *≥ Update    : 27.06.1996                                                 ≥
 *≥ called by : app                                                        ≥
 *≥ calls     : Dos*                                                       ≥
 *≥ Input     : ###                                                        ≥
 *≥ Tasks     : - read cursor file                                         ≥
 *≥             - return buffer with info                                  ≥
 *≥ returns   : APIRET - OS/2 error code                                   ≥
 *¿ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒŸ
 */


APIRET _Export QueryCursorFileDetails
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

    CURFILEHEADER curfileheader;
    CURFILERES curfileres;
    PBITMAPINFOHEADER2 pbih2DIB;
    CURSORFILEINFO cursorfileinfo;

    PBYTE pbCursorData = NULL;
    ULONG ulBitsPerPel, ulPelsInPointer, ulPlaneSize, ulColorPlaneSize;

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
        memset(&cursorfileinfo, 0, sizeof(CURSORFILEINFO));
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
                *pulSize = sizeof(CURSORFILEINFO);
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
            memset(psourceinfo, 0, sizeof(SOURCEINFO));

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

        // read file header and resource header
        QUERYFILEPTR(hfileAnimation);
        cursorfileinfo.ulCurFileHeaderOfs = ulFilePtr;
        cursorfileinfo.ulCurFileHeaderLen = sizeof(CURFILEHEADER);
        READMEMORY(hfileAnimation, curfileheader, sizeof(CURFILEHEADER));

        QUERYFILEPTR(hfileAnimation);
        cursorfileinfo.ulCurFileResourceOfs = ulFilePtr;
        cursorfileinfo.ulCurFileResourceLen = sizeof(CURFILERES);
        READMEMORY(hfileAnimation, curfileres, sizeof(CURFILERES));

        // check if data is valid
        if (curfileres.bReserved1 != 0)
        {
            rc = ERROR_INVALID_DATA;
            break;
        }

        // allocate cursor buffer size
        if ((pbCursorData = malloc(curfileres.ulDIBSize)) == NULL)
        {
            rc = ERROR_NOT_ENOUGH_MEMORY;
            break;
        }

        // now read the DIB data
        rc = DosSetFilePtr(hfileAnimation,
                           curfileres.ulDIBOffset,
                           FILE_BEGIN,
                           &ulFilePtr);
        if (rc != NO_ERROR)
            break;

        // read file resource info
        QUERYFILEPTR(hfileAnimation);
        cursorfileinfo.ulDIBDataLen = curfileres.ulDIBSize;
        PREADMEMORY(hfileAnimation, pbCursorData, curfileres.ulDIBSize);

        // erste Pointer initialisieren
        pbih2DIB = (PBITMAPINFOHEADER2)pbCursorData;

        ulBitsPerPel = pbih2DIB->cPlanes * pbih2DIB->cBitCount;
        ulPelsInPointer = (ULONG) curfileres.bHeight * curfileres.bWidth;
        ulPlaneSize = ulPelsInPointer / 8;
        ulColorPlaneSize = ulPlaneSize * pbih2DIB->cBitCount;

        cursorfileinfo.ulPointerColors = 1 << ulBitsPerPel;
        cursorfileinfo.fBwPointer = (cursorfileinfo.ulPointerColors == 2);
        cursorfileinfo.ulHotspotX = curfileres.usXHotspot;
        cursorfileinfo.ulHotspotY = curfileres.bHeight - curfileres.usYHotspot;
        cursorfileinfo.ulBitsPerPlane = pbih2DIB->cBitCount;
        cursorfileinfo.ulPlanes = pbih2DIB->cPlanes;

        cursorfileinfo.ulBfhOfs = ulFilePtr;
        cursorfileinfo.ulBfhLen = pbih2DIB->cbFix;
        cursorfileinfo.ulColorTableOfs = cursorfileinfo.ulBfhOfs + cursorfileinfo.ulBfhLen;
        cursorfileinfo.ulColorTableLen = cursorfileinfo.ulPointerColors * sizeof(RGB2);
        cursorfileinfo.ulBitmapDataOfs = cursorfileinfo.ulColorTableOfs + cursorfileinfo.ulColorTableLen;
        cursorfileinfo.ulBitmapDataLen = ulColorPlaneSize;
        cursorfileinfo.ulMaskDataOfs = cursorfileinfo.ulBitmapDataOfs + cursorfileinfo.ulBitmapDataLen;
        cursorfileinfo.ulMaskDataLen = ulPlaneSize;

        // hand over result
        memcpy(pvData, &cursorfileinfo, sizeof(cursorfileinfo));

    }
    while (FALSE);

// cleanup
    if (hfileAnimation)
        DosClose(hfileAnimation);
    if (pbCursorData)
        free(pbCursorData);
    return rc;
}

/*⁄ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒø
 *≥ Name      : SetCursorFileInfo                                          ≥
 *≥ Comment   :                                                            ≥
 *≥ Author    : C.Langanke                                                 ≥
 *≥ Date      : 13.04.1998                                                 ≥
 *≥ Update    : 13.04.1998                                                 ≥
 *≥ called by : app                                                        ≥
 *≥ calls     : Dos*                                                       ≥
 *≥ Input     : ###                                                        ≥
 *≥ Tasks     : - write cursor file info                                   ≥
 *≥ returns   : APIRET - OS/2 error code                                   ≥
 *¿ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒŸ
 */

APIRET _Export SetCursorFileInfo
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

        // Datei îffnen
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


/*⁄ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒø
 *≥ Name      : LoadPointerFromWinAnimationFile                            ≥
 *≥ Kommentar : lÑdt Pointer aus Win Animation File                        ≥
 *≥ Autor     : C.Langanke                                                 ≥
 *≥ Datum     : 23.07.1995                                                 ≥
 *≥ énderung  : 23.07.1995                                                 ≥
 *≥ aufgerufen: diverse                                                    ≥
 *≥ ruft auf  : -                                                          ≥
 *≥ Eingabe   : PSZ          - Name der Pointerdatei                       ≥
 *≥             PHPOINTER    - Array von Handle-Variablen                  ≥
 *≥             PICONINFO    - Array von ICONINFO                          ≥
 *≥             ULONG        - I/O: ZÑhlvariable                           ≥
 *≥ Aufgaben  : - Pointer laden                                            ≥
 *≥ RÅckgabe  : BOOL - Flag: Pointer geladen/nicht geladen                 ≥
 *¿ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒŸ
 */

BOOL _Export LoadPointerFromWinAnimationFile
 (
     PSZ pszName,
     PHPOINTER pahptr,
     PICONINFO paiconinfo,
     PULONG paulTimeout,
     PULONG pulEntries
)
{
    BOOL fSuccess = FALSE;
    APIRET rc = NO_ERROR;
    BOOL fLoadError = FALSE;
    ULONG i;
    PICONINFO piconinfo, paiconinfoUsed;
    ICONINFO aiconinfoTmp[MAX_COUNT_POINTERS];
    PHPOINTER phptr;

    do
    {

        // initialisieren
        memset(&aiconinfoTmp, 0, sizeof(aiconinfoTmp));

        // Parameter prÅfen
        if ((pszName == NULL) ||
            (*pszName == 0) ||
            (pulEntries == NULL) ||
            (*pulEntries > MAX_COUNT_POINTERS))
            break;


        // ggfs. temporÑre iconinfoliste benutzen
        paiconinfoUsed = paiconinfo ? paiconinfo : aiconinfoTmp;

        // jetzt Animation File Daten lesen
        rc = _ReadWinAnimationValues(pszName, paulTimeout, paiconinfoUsed, pulEntries);
        DEBUGMSG("_ReadWinAnimationValues %s rc=%u" NEWLINE, pszName _c_ rc);
        if (rc != NO_ERROR)
            break;

        for (i = 0; i < *pulEntries; i++)
        {
            piconinfo = paiconinfoUsed + i;
            phptr = pahptr ? pahptr + i : NULL;

            // Pointer erstellen,
            // das Fileoffset steht in cbIconData
            fLoadError += !LoadPointerFromCursorFile(pszName, phptr, piconinfo, NULL);
            DEBUGMSG("LoadPointerFromCursorFile #%u loaderror=%u" NEWLINE, i _c_ fLoadError);

            // Fehler ?
            if (fLoadError)
                break;

            // ggfs. temporÑre Iconinfo Daten wieder freigeben
            if ((!paiconinfo) && (piconinfo->pIconData))
                free(piconinfo->pIconData);

        }

        // alles klar
        if (!fLoadError)
            fSuccess = TRUE;

    }
    while (FALSE);

    if (!fSuccess)
        *pulEntries = 0;

    return fSuccess;

}


/*⁄ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒø
 *≥ Name      : LoadFirstPointerFromWinAnimationFile                       ≥
 *≥ Kommentar : lÑdt ersten Pointer aus Win Animation File                 ≥
 *≥ Autor     : C.Langanke                                                 ≥
 *≥ Datum     : 23.07.1995                                                 ≥
 *≥ énderung  : 23.07.1995                                                 ≥
 *≥ aufgerufen: diverse                                                    ≥
 *≥ ruft auf  : -                                                          ≥
 *≥ Eingabe   : PSZ          - Name der Datei                              ≥
 *≥             PHPOINTER    - Zeiger auf handle-Variable                  ≥
 *≥             PICONINFO    - Zeiger auf ICONINFO                         ≥
 *≥ Aufgaben  : - Pointer laden                                            ≥
 *≥ RÅckgabe  : BOOL - Flag: Pointer geladen/nicht geladen                 ≥
 *¿ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒŸ
 */

BOOL LoadFirstPointerFromWinAnimationFile
 (
     PSZ pszAnimationFile,
     PHPOINTER phpointer,
     PICONINFO piconinfo
)
{
    BOOL fSuccess = FALSE;
    ULONG ulTimeout;
    ULONG ulEntries = 1;

    do
    {

        // Parameter prÅfen
        if ((pszAnimationFile == NULL) ||
            (phpointer == NULL) ||
            (piconinfo == NULL))
            break;

        fSuccess = LoadPointerFromWinAnimationFile(pszAnimationFile,
                                                   phpointer,
                                                   piconinfo,
                                                   &ulTimeout,
                                                   &ulEntries);

        DEBUGMSG("--- LoadPointerFromWinAnimationFile: %s success=%u" NEWLINE, pszAnimationFile _c_ fSuccess);
    }
    while (FALSE);

    return fSuccess;

}

/*
 *@@ LoadFirstAnimationFromWinAnimationFile:
 *      loads first pointer from an ANI file.
 *      Returns TRUE or FALSE, signalling success.
 */

BOOL LoadFirstAnimationFromWinAnimationFile(PSZ pszAnimationFile, // in: file name
                                            PHPOINTER pahpointer, // ptr to handle
                                            PULONG pulTimeout,
                                            PULONG pulEntries)
{
    BOOL fSuccess = FALSE;
    APIRET rc;
    ULONG i;
    ULONG ulResourceId;
    HMODULE hmodResource = NULLHANDLE;
    CHAR szError[20];
    ICONINFO iconinfo;
    HPOINTER hptrStatic = NULLHANDLE;

    do
    {

        // Parameter prÅfen
        if ((pszAnimationFile == NULL) ||
            (pahpointer == NULL) ||
            (pulEntries == NULL))
            break;

        // load animation
        fSuccess = LoadPointerFromWinAnimationFile(pszAnimationFile,
                                                   pahpointer,
                                                   NULL,
                                                   pulTimeout,
                                                   pulEntries);

    }
    while (FALSE);

    return fSuccess;
}

/*⁄ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒø
 *≥ Name      : WriteWinAnimationFile                                      ≥
 *≥ Comment   :                                                            ≥
 *≥ Author    : C.Langanke                                                 ≥
 *≥ Date      : 27.06.1996                                                 ≥
 *≥ Update    : 27.06.1996                                                 ≥
 *≥ called by : app                                                        ≥
 *≥ calls     : Dos*                                                       ≥
 *≥ Input     : ###                                                        ≥
 *≥ Tasks     : - write win animation file                                 ≥
 *≥ returns   : APIRET - OS/2 error code                                   ≥
 *¿ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒŸ
 */
APIRET WriteWinAnimationFile
 (
     PSZ pszAnimationFileName,
     PULONG paulTimeout,
     PICONINFO paiconinfo,
     ULONG ulEntries,
     PSZ pszInfoName,
     PSZ pszInfoArtist
)
{

    APIRET rc = NO_ERROR;
    ULONG i, j;
    HFILE hfileAnimation = NULLHANDLE;
    ULONG ulAction;
    ULONG ulBytesWritten, ulFilePtr;
    SIG sig;
    PSIG psig;
    ANIHEADER aniheader;

    BOOL fWriteInfoPadByte = FALSE;
    PICONINFO piconinfo;
    ULONG ulRate;
    ULONG ulPhysEntries = ulEntries;

    PBYTE *ppbFrames = NULL;
    ULONG ulFrameTableLen;
    PBYTE pbFrame;
    ULONG ulCursorLen;

    BOOL fUseRateTable = FALSE;
    ULONG ulDefaultTimeout;
    BOOL fUseSequenceTable = FALSE;
    PULONG paulSequence = NULL;
    PICONINFO piconinfo1, piconinfo2;
    ULONG ulPointerSkipped = 0;
    ULONG ulDuplicates = 0;

    ULONG ulRiffLen, ulInfoLen, ulHeaderLen, ulRateLen, ulSequenceLen, ulFrameLen,
     ulFramelistLen;

    do
    {
        // check parameters
        if ((pszAnimationFileName == NULL) ||
            (*pszAnimationFileName == 0) ||
            (paulTimeout == NULL) ||
            (paiconinfo == NULL) ||
            (ulEntries == 0))
        {
            rc = ERROR_INVALID_PARAMETER;
            break;
        }

        if (!pszInfoName)
            pszInfoName = pszDefaultInfo;
        if (!pszInfoArtist)
            pszInfoArtist = pszDefaultInfo;

        // -----------------------------------------------------------

        // check timeouts, are all the same ?
        ulDefaultTimeout = *paulTimeout;
        for (i = 1; i < ulEntries; i++)
        {
            if (ulDefaultTimeout != *(paulTimeout + i))
            {
                fUseRateTable = TRUE;
                break;
            }
        }

        if (!fUseRateTable)
            ulDefaultTimeout = DEFAULT_ANIMATION_TIMEOUT;

        // -----------------------------------------------------------

        // check pointerdata, are pointers the same ?
        if ((paulSequence = calloc(2 * ulEntries, sizeof(ULONG))) == NULL)
        {
            rc = ERROR_NOT_ENOUGH_MEMORY;
            break;
        }

        ulPointerSkipped = 0;
        for (i = 0; i < ulEntries; i++)
        {
            for (j = i; j < ulEntries; j++)     // j = i needed, so that the last ptr is examined too

            {
                // is pointer already identified as duplicate ?
                if ((*(paulSequence + i) != 0) && (*(paulSequence + i) != i - ulPointerSkipped))
                {
                    ulPointerSkipped++;
                    break;
                }

                // don't compare ptr with itself
                if (i == j)
                    continue;

                // mark pointer as itself
                *(paulSequence + i) = i - ulPointerSkipped;

                // check len
                if ((paiconinfo + i)->cbIconData != (paiconinfo + j)->cbIconData)
                    continue;

                // check icon data
                if (memcmp((paiconinfo + i)->pIconData, (paiconinfo + j)->pIconData, (paiconinfo + j)->cbIconData))
                    continue;

                // same pointer found
                fUseSequenceTable = TRUE;
                ulDuplicates++;
                *(paulSequence + j) = i - ulPointerSkipped;
            }
        }

        // calculate number of physical entries
        ulPhysEntries = ulEntries - ulDuplicates;

        // -----------------------------------------------------------

        //  get len of info chunk
        ulInfoLen = 0;
        if ((pszInfoName) || (pszInfoArtist))
        {
            // determine size
            ulInfoLen = SUBSIGSIZE;     // SUBSIGSIZE mu· in der LIST sig enthalten sein

            if (pszInfoName)
                ulInfoLen += INFOSIZE(pszInfoName);
            if (pszInfoArtist)
                ulInfoLen += INFOSIZE(pszInfoArtist);

            // is it an odd number ?
            if (ulInfoLen % 2 > 0)
            {
                fWriteInfoPadByte = TRUE;
                ulInfoLen++;
            }

        }                       // if ((pszInfoName) || (pszInfoArtist))

        // -----------------------------------------------------------

        //  get len of  aniheader
        ulHeaderLen = sizeof(ANIHEADER);

        // -----------------------------------------------------------

        //  get len of rate chunk
        ulRateLen = ulEntries * sizeof(ULONG);

        // -----------------------------------------------------------

        //  get len of sequence chunk
        ulSequenceLen = ulEntries * sizeof(ULONG);

        // -----------------------------------------------------------

        //  create LIST frame chunk
        ulFramelistLen = SUBSIGSIZE;    // SUBSIGSIZE mu· in der LIST sig enthalten sein

        // create frame table space
        ulFrameTableLen = ulPhysEntries * sizeof(PBYTE);
        ppbFrames = malloc(ulFrameTableLen);
        memset(ppbFrames, 0, ulFrameTableLen);
        if (ppbFrames == NULL)
        {
            rc = ERROR_NOT_ENOUGH_MEMORY;
            break;
        }

        // create frame segments
        ulPointerSkipped = 0;
        for (i = 0, j = 0; ((rc == NO_ERROR) && (i < ulEntries)); i++)
        {

            // if sequence given and item is a duplicate, then skip
            if (fUseSequenceTable)
            {
                if (*(paulSequence + i) != i - ulPointerSkipped)
                {
                    ulPointerSkipped++;
                    continue;
                }
            }

            // determine cursor size
            rc = _CreateCursorFromPointer((paiconinfo + i)->pIconData,
                                          (paiconinfo + i)->cbIconData,
                                          NULL,
                                          0,
                                          &ulCursorLen);
            if (rc != ERROR_MORE_DATA)
                break;

            // get memory for Cursor frame
            ulFrameLen = SIGSIZE + ulCursorLen;
            pbFrame = malloc(ulFrameLen);
            if (pbFrame == NULL)
            {
                rc = ERROR_NOT_ENOUGH_MEMORY;
                break;
            }

            *(ppbFrames + j) = pbFrame;     // save this frames ptr

            ulFramelistLen += ulFrameLen;   // sum up len of all frames

            psig = (PSIG) pbFrame;
            j++;

            // convert pointer to cursor
            rc = _CreateCursorFromPointer((paiconinfo + i)->pIconData,
                                          (paiconinfo + i)->cbIconData,
                                          pbFrame + sizeof(SIG),
                                          ulCursorLen,
                                          &ulCursorLen);
            if (rc != NO_ERROR)
                break;

            // write frame sig
            SETPSIG(psig, pszSigIcon);
            SETPSIGSIZE(psig, ulCursorLen);

        }                       // for (i = 0; ((rc == NO_ERROR) && (i <  ulEntries); i++);

        if (rc != NO_ERROR)
            break;


        // ###########################################################################

        // calculate len of RIFF chunk

        ulRiffLen =             // determine length of chunks
             SIGSIZE +          // RIFF
             SUBSIGSIZE +       // - ACON

            ((ulInfoLen > 0) ?
             (SIGSIZE + ulInfoLen) : 0) +   // - 'LIST' 'INFO''INAM'  'IART'

            SIGSIZE + ulHeaderLen +     // - 'anih' (ANIHEADER)

            ((fUseRateTable) ?  // rate table used ?
              (SIGSIZE + ulRateLen) : 0) +  // - 'rate' (DWORD per logical frame)

            ((fUseSequenceTable) ?  // sequence table used ?
              (SIGSIZE + ulSequenceLen) : 0) +  // - ['seq '] (DWORD per logical frame)

            SIGSIZE + ulFramelistLen;   // - 'LIST' 'fram''icon'


        // ###########################################################################

        // Open ani file
        rc = DosOpen(pszAnimationFileName,
                     &hfileAnimation,
                     &ulAction,
                     0,
                     0,
                     OPEN_ACTION_CREATE_IF_NEW | OPEN_ACTION_REPLACE_IF_EXISTS,
                     OPEN_SHARE_DENYREADWRITE | OPEN_ACCESS_WRITEONLY,
                     NULL);
        if (rc != NO_ERROR)
            break;

        // write RIFF sig
        SETSIG(sig, pszSigRiff);
        SETSIGSIZE(sig, ulRiffLen);
        WRITESIG(sig);

        // write simple ACON sig
        SETSIG(sig, pszSigAcon);
        WRITESIMPLESIG(sig);

        // write info list chunk
        if (ulInfoLen > 0)
        {
            // write LIST sig
            SETSIG(sig, pszSigList);
            SETSIGSIZE(sig, ulInfoLen);
            WRITESIG(sig);

            // write simple INFO sig
            SETSIG(sig, pszSigInfo);
            WRITESIMPLESIG(sig);

            if (pszInfoName)
            {
                SETSIG(sig, pszSigInfoName);
                SETSIGSIZE(sig, strlen(pszInfoName) + 1);
                WRITESIG(sig);
                WRITESZ(hfileAnimation, pszInfoName);
            }

            if (pszInfoArtist)
            {
                SETSIG(sig, pszSigInfoArtist);
                SETSIGSIZE(sig, strlen(pszInfoArtist) + 1);
                WRITESIG(sig);
                WRITESZ(hfileAnimation, pszInfoArtist);
            }

            // odd info len ?
            if (fWriteInfoPadByte)
            {
                SETSIG(sig, "");
                WRITEMEMORY(hfileAnimation, &sig, 1);
            }
        }

        // write anim header
        SETSIG(sig, pszSigAniHeader);
        SETSIGSIZE(sig, ulHeaderLen);
        WRITESIG(sig);

        memset(&aniheader, 0, sizeof(ANIHEADER));
        aniheader.cbSizeof = sizeof(ANIHEADER);     // Num. bytes in aniheader (incl. cbSizeof)

        aniheader.cFrames = ulPhysEntries;  // Number of unique icons in the ani. cursor

        aniheader.cSteps = ulEntries;   // number of frames in the animation

        aniheader.jifRate = MS2JIF(ulDefaultTimeout);   // default timeout

        aniheader.fl = AF_ICON; // flag

        WRITESTRUCT(hfileAnimation, aniheader);


        // write rate list
        if (fUseRateTable)
        {
            SETSIG(sig, pszSigRate);
            SETSIGSIZE(sig, ulRateLen);
            WRITESIG(sig);

            for (i = 0; i < ulEntries; i++)
            {
                ulRate = MS2JIF(*(paulTimeout + i));
                WRITESTRUCT(hfileAnimation, ulRate);
            }
        }

        // write sequence list
        if (fUseSequenceTable)
        {
            SETSIG(sig, pszSigSeq);
            SETSIGSIZE(sig, ulRateLen);
            WRITESIG(sig);
            WRITEMEMORY(hfileAnimation, paulSequence, ulSequenceLen);
        }

        // write frame list chunk
        // write LIST sig
        SETSIG(sig, pszSigList);
        SETSIGSIZE(sig, ulFramelistLen);
        WRITESIG(sig);

        // write simple fram sig
        SETSIG(sig, pszSigFrame);
        WRITESIMPLESIG(sig);

        for (i = 0; i < ulPhysEntries; i++)
        {
            pbFrame = *(ppbFrames + i);
            psig = (PSIG) pbFrame;
            WRITEMEMORY(hfileAnimation, pbFrame, PSIGSEGMSIZE(psig) + SIGSIZE);
        }

        // set first pointer as file icon
        DosClose(hfileAnimation);
        hfileAnimation = NULLHANDLE;

    }
    while (FALSE);

// cleanup
    if (hfileAnimation)
        DosClose(hfileAnimation);

    if (ppbFrames)
    {
        for (i = 0; i < ulPhysEntries; i++)
        {
            pbFrame = *(ppbFrames + i);
            if (pbFrame)
                free(pbFrame);
        }
        free(ppbFrames);
    }

// cleanup
    if (hfileAnimation)
        DosClose(hfileAnimation);
    if (paulSequence)
        free(paulSequence);

    return rc;

}

/*⁄ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒø
 *≥ Name      : QueryWinAnimationFileDetails                               ≥
 *≥ Comment   :                                                            ≥
 *≥ Author    : C.Langanke                                                 ≥
 *≥ Date      : 27.06.1996                                                 ≥
 *≥ Update    : 27.06.1996                                                 ≥
 *≥ called by : app                                                        ≥
 *≥ calls     : Dos*                                                       ≥
 *≥ Input     : ###                                                        ≥
 *≥ Tasks     : - read win animation file                                  ≥
 *≥             - return buffer with info                                  ≥
 *≥ returns   : APIRET - OS/2 error code                                   ≥
 *¿ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒŸ
 */

#define SETSIGSTATUS(v)   ulSigStatus = v
#define CHECKSIGSTATUS(before,after) \
if ((ulSigStatus < before) ||        \
    (ulSigStatus > after))           \
   {                                 \
   rc = ERROR_INVALID_DATA;          \
   break;                            \
   }                                 \

#define  STATUS_RIFF        1
#define  STATUS_ACON        2
#define  STATUS_INFO        3
#define  STATUS_DISPLAY     4
#define  STATUS_ANIHEADER   5
#define  STATUS_RATE        6
#define  STATUS_SEQUENCE    7
#define  STATUS_FRAME       8


APIRET _Export QueryWinAnimationFileDetails
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
    SIG sig;
    ANIFILEINFO anifileinfo;
    SOURCEINFO sourceinfo;

    PANIFILEINFO panifileinfo;
    PSOURCEINFO psourceinfo;

    ULONG ulSubChunkOfs;

    ULONG ulSigStatus = 0;
    ULONG ulInfoStatus = 0;
    ULONG ulListLen;
    ULONG ulNeededLen;



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
        memset(&anifileinfo, 0, sizeof(ANIFILEINFO));
        memset(&sourceinfo, 0, sizeof(SOURCEINFO));
        if (pvData)
            memset(pvData, 0, ulBuflen);

        // Open and read the .CUR file header and the first ICONFILERES
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


        // read RIFF sig
        READNEXTSIG(sig);

        // read mandandory sigs
        if (CMPSIG(sig, pszSigRiff))
        {
            rc = ERROR_INVALID_DATA;
            break;
        }

        READNEXTSIMPLESIG(sig);
        if (CMPSIG(sig, pszSigAcon))
        {
            rc = ERROR_INVALID_DATA;
            break;
        }

        SETSIGSTATUS(STATUS_ACON);

        // read all following sigs here
        while (TRUE)
        {

            // get current file ptr and read next sig
            QUERYFILEPTR(hfileAnimation);
            READNEXTSIG(sig);

            // ---------------------------------------------
            // process list
      /**/ if (!CMPSIG(sig, pszSigList))
         {
         // get list len
         ulListLen = SIGSEGMSIZE( sig) - SUBSIGSIZE;

         // get list subsig
         READNEXTSIMPLESIG( sig);
         // - - - - - - - - - - - - - - - - - - - - -
         /**/ if (!CMPSIG(sig, pszSigInfo))
            {

                CHECKSIGSTATUS(STATUS_ACON, STATUS_INFO);
                SETSIGSTATUS(STATUS_INFO);

                // store chunk offset
                anifileinfo.ulInfoListOfs = ulFilePtr;
                anifileinfo.ulInfoListLen = SIGSEGMSIZE(sig) - SUBSIGSIZE;
                while ((ulListLen > 0) && (ulInfoStatus < 2))
                {
                    // get current file ptr and read next sig
                    QUERYFILEPTR(hfileAnimation);
                    READNEXTSIG(sig);
                    ulListLen -= SIGSEGMSIZE(sig) + SIGSIZE;

                    // . . . . . . . . . . . . . . . . . . .
                    // process name
               /**/ if (!CMPSIG(sig,pszSigInfoName))
                  {
                  // store sub chunk offset
                  anifileinfo.ulInfoNameOfs = ulFilePtr;
                  anifileinfo.ulInfoNameLen = SIGSEGMSIZE( sig) + SIGSIZE;
                  ulInfoStatus++;

                  // is data wanted ?
                  if (pbData)
                     {
                     // allocate one zero byte extra for safety
                     if ((sourceinfo.pszInfoName = calloc( SIGSEGMSIZE( sig) + 1, 1)) == NULL)
                        {
                        rc = ERROR_NOT_ENOUGH_MEMORY;
                        break;
                        }
                     PREADMEMORY( hfileAnimation, sourceinfo.pszInfoName, SIGSEGMSIZE( sig));

                     // throw away default value
                     if (strcmp( pszDefaultInfo, sourceinfo.pszInfoName) == 0)
                        {
                        free( sourceinfo.pszInfoName);
                        sourceinfo.pszInfoName = NULL;
                        }
                     }
                  else
                     SKIPBYTES( hfileAnimation, SIGSEGMSIZE( sig));
                  }
               else if (!CMPSIG(sig,pszSigInfoArtist))
                  {
                  // store sub chunk offset
                  anifileinfo.ulInfoArtistOfs = ulFilePtr;
                  anifileinfo.ulInfoArtistLen = SIGSEGMSIZE( sig) + SIGSIZE;
                  ulInfoStatus++;

                  // is data wanted ?
                  if (pbData)
                     {
                     // allocate one zero byte extra for safety
                     if ((sourceinfo.pszInfoArtist = calloc( SIGSEGMSIZE( sig)+ 1, 1)) == NULL)
                        {
                        rc = ERROR_NOT_ENOUGH_MEMORY;
                        break;
                        }
                     PREADMEMORY( hfileAnimation, sourceinfo.pszInfoArtist, SIGSEGMSIZE( sig));

                     // throw away default value
                     if (strcmp( pszDefaultInfo, sourceinfo.pszInfoArtist) == 0)
                        {
                        free( sourceinfo.pszInfoArtist);
                        sourceinfo.pszInfoArtist = NULL;
                        }
                     }
                  else
                     SKIPBYTES( hfileAnimation, SIGSEGMSIZE( sig));
                  }
               else
                  {
                  rc = ERROR_INVALID_DATA;
                  break;
                  }

               } // while (TRUE)
            if (rc != NO_ERROR)
               break;
            }
         // - - - - - - - - - - - - - - - - - - - - -
         else if (!CMPSIG(sig, pszSigFrame))
            {

            CHECKSIGSTATUS( STATUS_ANIHEADER, STATUS_FRAME);
            SETSIGSTATUS( STATUS_FRAME);

            // store chunk offset
            anifileinfo.ulFrameListOfs = ulFilePtr;
            anifileinfo.ulFrameListLen = SIGSEGMSIZE( sig);

            // get memory for frame offset table
            if (pbData)
               {
               // allocate one zero byte extra for safety
               if ((anifileinfo.paulFrameOfs = malloc( sizeof( ULONG) * anifileinfo.ulFrameCount)) == NULL)
                  {
                  rc = ERROR_NOT_ENOUGH_MEMORY;
                  break;
                  }
               }

            // get all offsets
            for (i = 0; i < anifileinfo.ulPhysFrameCount; i++)
               {
               ulListLen -= SIGSEGMSIZE( sig) + SIGSIZE;

               // read and check sig
               READNEXTSIG( sig);
               if (CMPSIG(sig, pszSigIcon))
                  {
                  rc = ERROR_INVALID_DATA;
                  break;
                  }

               // store file offset
               if (anifileinfo.paulFrameOfs)
                  {
                  if (anifileinfo.paulSequence)
                     {
                     for (j = 0; j < anifileinfo.ulFrameCount; j++)
                        {
                        if (*(anifileinfo.paulSequence + j) == i)
                           *(anifileinfo.paulFrameOfs + j) = ulFilePtr;
                        }
                     }
                  else
                     *(anifileinfo.paulFrameOfs + i) = ulFilePtr;
                  }

               // skip data bytes
               SKIPBYTES( hfileAnimation, SIGSEGMSIZE( sig));

               } // while (TRUE)

            if (rc != NO_ERROR)
               break;
            }
         else
         // invalid sig found
            {
            rc = ERROR_INVALID_DATA;
            break;
            }

         }
      // ---------------------------------------------
      // process display chunk
      else if (!CMPSIG(sig, pszSigDisp))
         {

         CHECKSIGSTATUS( STATUS_ACON, STATUS_DISPLAY);
         SETSIGSTATUS( STATUS_DISPLAY);

         // store chunk offset
         anifileinfo.ulDisplayOfs = ulFilePtr;
         anifileinfo.ulDisplayLen = SIGSEGMSIZE( sig);

         // ignore chunk data
         MOVEFILEPTR( hfileAnimation, SIGSEGMSIZE( sig));
         }
      // ---------------------------------------------
      // process aniheader
      else if (!CMPSIG(sig, pszSigAniHeader))
         {

         CHECKSIGSTATUS( STATUS_ACON, STATUS_ANIHEADER);
         SETSIGSTATUS( STATUS_ANIHEADER);

         // store chunk offset
         anifileinfo.ulAniheaderOfs = ulFilePtr;

         // read animation header
         rc = DosRead( hfileAnimation,
                       &anifileinfo.aniheader,
                       sizeof( ANIHEADER),
                       &ulBytesRead);
         if (rc != NO_ERROR)
            break;

         // check aniheader format flag
         if (!(anifileinfo.aniheader.fl & AF_ICON))
            {
            rc = ERROR_INVALID_DATA;
            break;
            }

         // transfer relevant aniheader infos
         anifileinfo.ulFrameCount     = anifileinfo.aniheader.cFrames;
         anifileinfo.ulPhysFrameCount = anifileinfo.aniheader.cFrames;
         anifileinfo.ulDefaultTimeout = JIF2MS( anifileinfo.aniheader.jifRate);
         }
      // ---------------------------------------------
      // process rate table
      else if (!CMPSIG(sig, pszSigRate))
         {

         CHECKSIGSTATUS( STATUS_ANIHEADER, STATUS_RATE);
         SETSIGSTATUS( STATUS_RATE);

         // store chunk offset
         anifileinfo.ulRateinfoOfs = ulFilePtr;
         anifileinfo.ulRateinfoLen = SIGSEGMSIZE( sig);

         // recalculate frame count: frames may be used more than once
         // rate table includes entries for each logical frame
         anifileinfo.ulFrameCount = SIGSEGMSIZE(sig) / sizeof( ULONG);

         // is data wanted ?
         if (pbData)
            {
            // get memory
            if ((anifileinfo.paulTimeout = malloc( SIGSEGMSIZE(sig))) == NULL)
               {
               rc = ERROR_NOT_ENOUGH_MEMORY;
               break;
               }

            // read rate table
            rc = DosRead( hfileAnimation,
                          anifileinfo.paulTimeout,
                          SIGSEGMSIZE(sig),
                          &ulBytesRead);
            if (rc != NO_ERROR)
               break;

            // convert to ms
            for (i = 0; i < anifileinfo.ulFrameCount; i++)
               {
               *(anifileinfo.paulTimeout + i) = JIF2MS( *(anifileinfo.paulTimeout + i));
               }
            }
         else
            SKIPBYTES( hfileAnimation, SIGSEGMSIZE( sig));
         }

      // ---------------------------------------------
      // process sequence table
      else if (!CMPSIG(sig, pszSigSeq))
         {
         CHECKSIGSTATUS( STATUS_ANIHEADER, STATUS_SEQUENCE);
         SETSIGSTATUS( STATUS_SEQUENCE);

         // store chunk offset
         anifileinfo.ulSequenceinfoOfs = ulFilePtr;
         anifileinfo.ulSequenceinfoLen = SIGSEGMSIZE( sig);

         // recalculate frame count: frames may be used more than once
         // rate table includes entries for each logical frame
         anifileinfo.ulFrameCount = SIGSEGMSIZE(sig) / sizeof( ULONG);

         // is data wanted ?
         if (pbData)
            {
            // get memory
            if ((anifileinfo.paulSequence = malloc( SIGSEGMSIZE(sig))) == NULL)
               {
               rc = ERROR_NOT_ENOUGH_MEMORY;
               break;
               }

            // Tabelle lesen
            rc = DosRead( hfileAnimation,
                          anifileinfo.paulSequence,
                          SIGSEGMSIZE(sig),
                          &ulBytesRead);
            if (rc != NO_ERROR)
               break;
            }
         else
            SKIPBYTES( hfileAnimation, SIGSEGMSIZE( sig));
         }
      else
      // ---------------------------------------------
      // invalid sig found
         {
         rc = ERROR_INVALID_DATA;
         break;
         }

      } // while (TRUE)

   if (rc != NO_ERROR)
      break;

   switch (ulInfoLevel)
      {
      case FI_SOURCEINFO:
         ulNeededLen = sizeof( SOURCEINFO)                       +
                       anifileinfo.ulInfoNameLen                 +
                       anifileinfo.ulInfoArtistLen;
         break;

      case FI_DETAILINFO:
         // hand over results
         ulNeededLen = sizeof( ANIFILEINFO)                      +
                       anifileinfo.ulRateinfoLen                 +
                       anifileinfo.ulSequenceinfoLen             +
                       sizeof( ULONG) * anifileinfo.ulFrameCount;
         break;
      }


   *pulSize = ulNeededLen;
   if (ulNeededLen > ulBuflen)
      {
      rc = ERROR_MORE_DATA;
      break;
      }
   if (!pbData)
      {
      rc = ERROR_INVALID_PARAMETER;
      break;
      }


   switch (ulInfoLevel)
      {
      case FI_SOURCEINFO:
         psourceinfo = (PSOURCEINFO) pbData;
         MEMCOPY( pbData, &sourceinfo, sizeof( SOURCEINFO));

         // animation name and artist name
         if (sourceinfo.pszInfoName)
            {
            psourceinfo->pszInfoName = (PSZ) pbData;
            MEMCOPY( pbData, sourceinfo.pszInfoName, anifileinfo.ulInfoNameLen);
            }
         else
            psourceinfo->pszInfoName = NULL;

         if (sourceinfo.pszInfoArtist)
            {
            psourceinfo->pszInfoArtist = (PSZ) pbData;
            MEMCOPY( pbData, sourceinfo.pszInfoArtist, anifileinfo.ulInfoArtistLen);
            }
         else
            psourceinfo->pszInfoArtist = NULL;

         break;

      case FI_DETAILINFO:
         panifileinfo = (PANIFILEINFO) pbData;
         MEMCOPY( pbData, &anifileinfo, sizeof( ANIFILEINFO));

         // copy rate table
         if (anifileinfo.paulTimeout)
            {
            panifileinfo->paulTimeout = (PULONG)(PSZ) pbData;
            MEMCOPY( pbData, anifileinfo.paulTimeout, anifileinfo.ulRateinfoLen);
            }

         // copy sequence table
         if (anifileinfo.paulSequence)
            {
            panifileinfo->paulSequence = (PULONG) pbData;
            MEMCOPY( pbData, anifileinfo.paulSequence, anifileinfo.ulSequenceinfoLen);
            }

         // copy frame offset table
         panifileinfo->paulFrameOfs = (PULONG) pbData;
         MEMCOPY( pbData, anifileinfo.paulFrameOfs, sizeof( ULONG) * anifileinfo.ulFrameCount);
         break;
      }


   } while (FALSE);

// cleanup
if (hfileAnimation) DosClose( hfileAnimation);
if (anifileinfo.paulSequence)   free( anifileinfo.paulSequence);
if (anifileinfo.paulTimeout)    free( anifileinfo.paulTimeout);
if (anifileinfo.paulFrameOfs)   free( anifileinfo.paulFrameOfs);
if (sourceinfo.pszInfoName)     free( sourceinfo.pszInfoName);
if (sourceinfo.pszInfoArtist)   free( sourceinfo.pszInfoArtist);
return rc;

}

/*⁄ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒø
 *≥ Name      : SetWinAnimationFileInfo                                    ≥
 *≥ Comment   :                                                            ≥
 *≥ Author    : C.Langanke                                                 ≥
 *≥ Date      : 13.04.1998                                                 ≥
 *≥ Update    : 13.04.1998                                                 ≥
 *≥ called by : app                                                        ≥
 *≥ calls     : Dos*                                                       ≥
 *≥ Input     : ###                                                        ≥
 *≥ Tasks     : - write cursor file info                                   ≥
 *≥ returns   : APIRET - OS/2 error code                                   ≥
 *¿ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒŸ
 */

                    APIRET _Export SetWinAnimationFileInfo
                     (
                         PSZ pszAnimationFileName,
                         PSOURCEINFO psourceinfo
                    )
                    {

                        APIRET rc = NO_ERROR;
                        HFILE hfileAnimation = NULLHANDLE;
                        ULONG i, j;
                        ULONG ulAction;
                        ULONG ulBytesWritten;
                        ULONG ulFilePtr;

                        PVOID pvData = NULL;
                        ULONG ulDataLen = 0;

                        PVOID pvSourceInfo = NULL;
                        ULONG ulSourceInfoLen = 0;
                        SOURCEINFO sourceinfo;

                        PVOID pvNewInfoChunk = NULL;
                        ULONG ulNewInfoChunkLen;
                        ULONG ulNewInfoNameLen;
                        ULONG ulNewInfoArtistLen;
                        ULONG ulNewFileSize;

                        PSOURCEINFO psourceinfoNew = &sourceinfo;
                        PANIFILEINFO panifileinfo;
                        PSIG psig;

                        BOOL fInfoChunkPresent = FALSE;

                        HFILE hfileTmp = NULLHANDLE;
                        CHAR szTmpFile[_MAX_PATH];

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

                            // initialize structs
                            memset(&sourceinfo, 0, sizeof(sourceinfo));

                            // --------------------------------------------------------------------

                            // get current info size and info
                            QueryWinAnimationFileDetails(pszAnimationFileName, FI_SOURCEINFO, NULL, 0, &ulSourceInfoLen);

                            if (ulSourceInfoLen)
                            {
                                if ((pvSourceInfo = malloc(ulSourceInfoLen)) == NULL)
                                {
                                    rc = ERROR_NOT_ENOUGH_MEMORY;
                                    break;
                                }

                                // get the info
                                rc = QueryWinAnimationFileDetails(pszAnimationFileName, FI_SOURCEINFO, pvSourceInfo, ulSourceInfoLen, &ulSourceInfoLen);
                                memcpy(psourceinfoNew, pvSourceInfo, sizeof(SOURCEINFO));
                            }

                            // merge original info with new one
                            if (psourceinfo->pszInfoName != NULL)
                            {
                                if (*psourceinfo->pszInfoName != 0)
                                {
                                    psourceinfoNew->pszInfoName = psourceinfo->pszInfoName;
                                }
                                else
                                {
                                    psourceinfoNew->pszInfoName = 0;
                                }
                            }

                            if (psourceinfo->pszInfoArtist != NULL)
                            {
                                if (*psourceinfo->pszInfoArtist != 0)
                                {
                                    psourceinfoNew->pszInfoArtist = psourceinfo->pszInfoArtist;
                                }
                                else
                                {
                                    psourceinfoNew->pszInfoArtist = 0;
                                }
                            }


                            // use default for empty values
                            if (!psourceinfoNew->pszInfoName)
                                psourceinfoNew->pszInfoName = pszDefaultInfo;
                            if (!psourceinfoNew->pszInfoArtist)
                                psourceinfoNew->pszInfoArtist = pszDefaultInfo;

                            // --------------------------------------------------------------------

                            // read file info to obtain file offsets
                            rc = QueryWinAnimationFileDetails(pszAnimationFileName, FI_DETAILINFO, NULL, 0, &ulDataLen);
                            if (rc != ERROR_MORE_DATA)
                                break;

                            if ((pvData = malloc(ulDataLen)) == NULL)
                            {
                                rc = ERROR_NOT_ENOUGH_MEMORY;
                                break;
                            }

                            rc = QueryWinAnimationFileDetails(pszAnimationFileName, FI_DETAILINFO, pvData, ulDataLen, &ulDataLen);
                            if (rc != NO_ERROR)
                                break;

                            // --------------------------------------------------------------------

                            // create new info chunk in memory, each chunk aligned to USHORT boundary

                            ulNewInfoNameLen = (psourceinfoNew->pszInfoName ? strlen(psourceinfoNew->pszInfoName) + 1 : 0);
                            ulNewInfoArtistLen = (psourceinfoNew->pszInfoArtist ? strlen(psourceinfoNew->pszInfoArtist) + 1 : 0);

                            if (ulNewInfoNameLen + ulNewInfoArtistLen > 0)
                            {
                                if (ulNewInfoNameLen % 2 != 0)
                                    ulNewInfoNameLen++;
                                if (ulNewInfoArtistLen % 2 != 0)
                                    ulNewInfoArtistLen++;

                                ulNewInfoChunkLen = SIGSIZE + SUBSIGSIZE +
                                    (ulNewInfoNameLen ? SIGSIZE + ulNewInfoNameLen : 0) +
                                    (ulNewInfoArtistLen ? SIGSIZE + ulNewInfoArtistLen : 0);


                                if ((pvNewInfoChunk = malloc(ulNewInfoChunkLen)) == NULL)
                                {
                                    rc = ERROR_NOT_ENOUGH_MEMORY;
                                    break;
                                }

                                // write LIST sig
                                psig = (PSIG) pvNewInfoChunk;
                                SETPSIG(psig, pszSigList);
                                SETPSIGSIZE(psig, ulNewInfoChunkLen - SIGSIZE);
                                psig = (PSIG)(PBYTE) psig + SIGSIZE;

                                SETPSIG(psig, pszSigInfo);
                                // write simplesig
                                psig = (PSIG)(PBYTE) psig + SUBSIGSIZE;

                                // write info name
                                if (psourceinfoNew->pszInfoName)
                                {
                                    SETPSIG(psig, pszSigInfoName);
                                    SETPSIGSIZE(psig, ulNewInfoNameLen);
                                    psig = (PSIG)(PBYTE) psig + sizeof(SIG);
                                    MEMCOPY(psig,
                                            psourceinfoNew->pszInfoName,
                                            ulNewInfoNameLen);
                                    *(((PBYTE) psig) - 1) = 0;
                                }

                                // write info artist
                                if (psourceinfoNew->pszInfoArtist)
                                {
                                    SETPSIG(psig, pszSigInfoArtist);
                                    SETPSIGSIZE(psig, ulNewInfoArtistLen);
                                    psig = (PSIG)(PBYTE) psig + sizeof(SIG);
                                    MEMCOPY(psig,
                                            psourceinfoNew->pszInfoArtist,
                                            ulNewInfoArtistLen);
                                    *(((PBYTE) psig) - 1) = 0;
                                }

                            }   // if (ulNewInfoNameLen + ulNewInfoArtistLen > 0)

                            // --------------------------------------------------------------------

                            // create temporary file
                            rc = CreateTmpFile(pszAnimationFileName, &hfileTmp, szTmpFile, sizeof(szTmpFile));
                            if (rc != NO_ERROR)
                                break;

                            // --------------------------------------------------------------------

                            // Datei îffnen
                            rc = DosOpen(pszAnimationFileName,
                                         &hfileAnimation,
                                         &ulAction,
                                         0, 0,
                                         OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS,
                             OPEN_SHARE_DENYREADWRITE | OPEN_ACCESS_READWRITE,
                                         NULL);
                            if (rc != NO_ERROR)
                                break;

                            // kopiere alles bis info block oder bis ani header
                            panifileinfo = (PANIFILEINFO) pvData;
                            fInfoChunkPresent = (panifileinfo->ulInfoListOfs != 0);
                            if (fInfoChunkPresent)
                                ulDataLen = panifileinfo->ulInfoListOfs - SIGSIZE - SUBSIGSIZE;
                            else
                                ulDataLen = panifileinfo->ulAniheaderOfs - SIGSIZE;

                            rc = CopyFromFileToFile(hfileTmp, hfileAnimation, ulDataLen);
                            if (rc != NO_ERROR)
                                break;

                            // skip existing info header
                            if (fInfoChunkPresent)
                                MOVEFILEPTR(hfileAnimation, panifileinfo->ulInfoListLen + SIGSIZE + SUBSIGSIZE);

                            // write own info chunk
                            if (pvNewInfoChunk)
                                WRITEMEMORY(hfileTmp, pvNewInfoChunk, ulNewInfoChunkLen);

                            // write rest of the file
                            rc = CopyFromFileToFile(hfileTmp, hfileAnimation, -1);
                            if (rc != NO_ERROR)
                                break;

                            // write new size to the file
                            QUERYFILEPTR(hfileTmp);
                            ulNewFileSize = ulFilePtr;
                            SETFILEPTR(hfileTmp, strlen(pszSigRiff));
                            WRITEMEMORY(hfileTmp, &ulNewFileSize, sizeof(ULONG));

                            // close files and do the copy
                            DosClose(hfileAnimation);
                            DosClose(hfileTmp);
                            rc = DosCopy(szTmpFile, pszAnimationFileName, DCPY_EXISTING);
                            if (rc != NO_ERROR)
                                break;

                            rc = DosDelete(szTmpFile);

                        }
                        while (FALSE);

// cleanup
                        if (hfileAnimation)
                            DosClose(hfileAnimation);
                        if (hfileTmp)
                            DosClose(hfileTmp);
                        if (pvData)
                            free(pvData);
                        if (pvSourceInfo)
                            free(pvSourceInfo);
                        if (pvNewInfoChunk)
                            free(pvNewInfoChunk);
                        return rc;

                    }
