// C Runtime
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

// OS/2 Toolkit
#define INCL_ERRORS
#define INCL_PM
#define INCL_WIN
#define INCL_DOS
#define INCL_DOSDEVIOCTL
#define INCL_DOSMISC
#include <os2.h>

// generic headers
#include "setup.h"              // code generation and debugging options

#include "pointers\mptrppl.h"
#include "pointers\mptrset.h"
#include "pointers\macros.h"
#include "pointers\debug.h"
#include "pointers\cursor.h"
#include "pointers\pointer.h"
#include "pointers\dll.h"
#include "pointers\eas.h"
#include "pointers\mptrptr.h"
#include "pointers\mptrcnr.h"
#include "pointers\mptranim.h"
#include "pointers\mptrutil.h"
#include "pointers\fmacros.h"

// cursor als thumbnail anh„ngen
#define CREATE_CURSORTHUMBNAIL

// erstelle Cursor-Dateien aus Win Animationen
// #define CREATE_CURSORFILE

// resource file signatures
#define RESFILEHEAD_WINANIMATION                 "RIFF"
#define RESFILEHEAD_ANIMOUSE                     "MZ"
#define RESFILEHEAD_COLORPTR                     "CP"
#define RESFILEHEAD_BWPTR                        "PT"
#define RESFILEHEAD_CURSOR                       0x00020000
#define RESFILEHEAD_ANMFILE                      "Animation_Script\r\n"

// resource file extensions according to RESFILETYPE_*
static PSZ pszDirExt = "<DIR>";
static PSZ apszResFileExt[] =
{"",                            // RESFILETYPE_DEFAULT
  ".PTR",                       // RESFILETYPE_POINTER
  "<DIR>",                      // RESFILETYPE_POINTERSET
  ".CUR",                       // RESFILETYPE_CURSOR
  "",                           // RESFILETYPE_CURSORSET
  ".AND",                       // RESFILETYPE_ANIMOUSE
  "",                           // RESFILETYPE_ANIMOUSESET
  ".ANI",                       // RESFILETYPE_WINANIMATION
  "",                           // RESFILETYPE_WINANIMATIONSET
  ".ANM"};                      // RESFILETYPE_ANMFILE

// IDs fr System-Pointer
static ULONG aulSysPtrId[] =
{SPTR_ARROW, SPTR_TEXT, SPTR_WAIT, SPTR_SIZENWSE, SPTR_SIZEWE,
 SPTR_MOVE, SPTR_SIZENESW, SPTR_SIZENS, SPTR_ILLEGAL};

// pointer names according to system pointer index (0-8)
static PSZ apszPointerName[] =
{"ARROW", "TEXT", "WAIT", "SIZENWSE", "SIZEWE",
 "MOVE", "SIZENESW", "SIZENS", "ILLEGAL"};

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : QueryPointerSysId                                          ³
 *³ Comment   :                                                            ³
 *³ Author    : C.Langanke                                                 ³
 *³ Date      : 04.11.1995                                                 ³
 *³ Update    : 05.11.1995                                                 ³
 *³ called by : diverse                                                    ³
 *³ calls     : -                                                          ³
 *³ Input     : ULONG - Pointer Index                                      ³
 *³ Tasks     : -                                                          ³
 *³ returns   : ULONG - system id of Pointer                               ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */
ULONG _Export QueryPointerSysId
 (
     ULONG ulPointerIndex
)
{


    if (ulPointerIndex < NUM_OF_SYSCURSORS)
        return aulSysPtrId[ulPointerIndex];
    else
        return SPTR_ARROW;
}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : QueryPointerIndexFromName                                  ³
 *³ Comment   :                                                            ³
 *³ Author    : C.Langanke                                                 ³
 *³ Date      : 04.11.1995                                                 ³
 *³ Update    : 05.11.1995                                                 ³
 *³ called by : diverse                                                    ³
 *³ calls     : -                                                          ³
 *³ Input     : PSZ - Pointer Name (arrow, text, etc)                      ³
 *³             PULONG - ptr to index variable                             ³
 *³ Tasks     : - search name and return index                             ³
 *³ returns   : BOOL - Name is valid                                       ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */
BOOL _Export QueryPointerIndexFromName
 (
     PSZ pszPointerName,
     PULONG pulPointerNameIndex
)
{
    BOOL fValid = FALSE;
    ULONG i;

    do
    {
        // check parm
        if (pszPointerName == NULL)
            break;

        // search the name
        for (i = 0; i < (sizeof(apszPointerName) / sizeof(PSZ)); i++)
        {
            if (strnicmp(pszPointerName, apszPointerName[i], strlen(apszPointerName[i])) == 0)
            {
                fValid = TRUE;
                if (pulPointerNameIndex)
                    *pulPointerNameIndex = i;
                break;
            }
        }

    }
    while (FALSE);

    return fValid;
}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : QueryResFileExtension                                      ³
 *³ Comment   :                                                            ³
 *³ Author    : C.Langanke                                                 ³
 *³ Date      : 15.03.1998                                                 ³
 *³ Update    : 15.03.1998                                                 ³
 *³ called by : diverse                                                    ³
 *³ calls     : -                                                          ³
 *³ Input     : ULONG - file type returned from QueryResFileType           ³
 *³ Tasks     : - see return value                                         ³
 *³ returns   : PSZ  - Pointer to fileextension including period           ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

PSZ _Export QueryResFileExtension
 (
     ULONG ulResFileType
)
{
    if (ulResFileType > RESFILETYPE__LAST)
        ulResFileType = 0;

    return apszResFileExt[ulResFileType];
}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : QueryResFileTypeFromExt                                    ³
 *³ Comment   :                                                            ³
 *³ Author    : C.Langanke                                                 ³
 *³ Date      : 15.03.1998                                                 ³
 *³ Update    : 15.03.1998                                                 ³
 *³ called by : diverse                                                    ³
 *³ calls     : -                                                          ³
 *³ Input     : PSZ    - file extension                                    ³
 *³           : PULONG - ptr to type variable                              ³
 *³ Tasks     : - see return value                                         ³
 *³ returns   : BOOL - extension is valid                                  ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

BOOL _Export QueryResFileTypeFromExt
 (
     PSZ pszFileExtension,
     PULONG pulResFileType
)
{
    BOOL fExtensionValid = FALSE;
    ULONG i;
    ULONG ulExtensionCount = sizeof(apszResFileExt) / sizeof(ULONG);
    ULONG ulSkipBytes = 0;

    do
    {
        // check parms
        if ((pszFileExtension == NULL) ||
            (pulResFileType == NULL))
            break;

        // wenn die bergebene Extension keinen
        // Punkt am Anfang hat, diesen bergehen
        if (*pszFileExtension != '.')
            ulSkipBytes = 1;

        for (i = 1; i < ulExtensionCount; i++)
        {
            if (stricmp(pszFileExtension, (apszResFileExt[i]) + ulSkipBytes) == 0)
            {
                *pulResFileType = i;
                fExtensionValid = TRUE;
                break;
            }
        }

    }
    while (FALSE);

    return fExtensionValid;
}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : QueryPointerName                                           ³
 *³ Comment   :                                                            ³
 *³ Author    : C.Langanke                                                 ³
 *³ Date      : 15.03.1998                                                 ³
 *³ Update    : 15.03.1998                                                 ³
 *³ called by : diverse                                                    ³
 *³ calls     : -                                                          ³
 *³ Input     : ULONG index of system pointer                              ³
 *³ Tasks     : - returns a pointer to pointer name                        ³
 *³ returns   : PSZ  - Pointer to pointername                              ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */
PSZ _Export QueryPointerName
 (
     ULONG ulPtrIndex
)
{
    PSZ pszResult = NULL;

    do
    {
        // check parm
        if (ulPtrIndex >= NUM_OF_SYSCURSORS)
            break;

        pszResult = apszPointerName[ulPtrIndex];

    }
    while (FALSE);

    return pszResult;

}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : QueryResFileType                                           ³
 *³ Kommentar : bestimmt den Typ einer Datei- bzw. Verzeichnis.            ³
 *³             Ist der Animationsname der Name eines Unterverzeichnisses  ³
 *³             von \OS2\POINTERS, so wird Typ PTR zurckgegeben.          ³
 *³             Es wird nicht nach PTR oder CUR-Dateien in diesem          ³
 *³             Verzeichnis gesucht, dies geschieht durch die Laderoutine. ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 15.07.1996                                                 ³
 *³ nderung  : 15.07.1996                                                 ³
 *³ aufgerufen: diverse                                                    ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : PSZ          - Name  Pointerdatei / Verzeichnis            ³
 *³           : PULONG       - Zielvariable fr Dateityp                   ³
 *³           : BOOL         - Flag fr Pointerset                         ³
 *³ Aufgaben  : - Vollen Namen ermitteln                                   ³
 *³             - Resource-Type bestimmen                                  ³
 *³ Rckgabe  : BOOL - Flag: Dateityp bestimmt                             ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

BOOL _Export QueryResFileType
 (
     PSZ pszName,
     PULONG pulFileType,
     PSZ pszFullName,
     ULONG ulMaxLen
)
{
    BOOL fSuccess = FALSE;
    APIRET rc;

    PSZ pszPointerDirectory = POINTER_BASEPATH;
    CHAR szFullName[_MAX_PATH];
    ULONG ulBootDrive = 0;
    ULONG ulResourceDrive;

    CHAR szDeviceName[_MAX_PATH];
    PSZ pszColon;
    BOOL fIsLocalDrive;
    BOOL fIsRemoveableMedia;

    FILESTATUS3 fs3;

    HFILE hfile = NULL;
    ULONG ulAction;
    ULONG ulBytesRead;
    CHAR szHeader[19];

    PSZ pszExtension;

    PSZ pszFileHeader;
    PSZ pszFileExt;
    ULONG ulHeader;
    ULONG ulFileType;

    do
    {

        // Parameter prfen
        if ((pszName == NULL) ||
            (pulFileType == NULL))
            break;
        *pulFileType = 0;

        // vollen Namen holen
        rc = DosQueryPathInfo(pszName,
                              FIL_QUERYFULLNAME,
                              szFullName,
                              sizeof(szFullName));
        if (rc != NO_ERROR)
            break;
        strupr(szFullName);

        // weitere Infos holen
        DosQueryPathInfo(pszName, FIL_STANDARD, &fs3, sizeof(fs3));

        // Datei oder Verzeichnisname ?
        if (fs3.attrFile & FILE_DIRECTORY)
        {
            *pulFileType = RESFILETYPE_POINTERSET;
        }
        else
        {
            // Erweiterung suchen
            pszExtension = strrchr(szFullName, '.');
            if (pszExtension == NULL)
                pszExtension = "";

            // Datei untersuchen: prfen, was fr eine Datei es ist
            rc = DosOpen(szFullName,
                         &hfile,
                         &ulAction,
                         0, 0,
                         OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS,
                         OPEN_SHARE_DENYNONE | OPEN_ACCESS_READONLY,
                         NULL);
            if (rc != NO_ERROR)
                break;

            // header einlesen
            rc = DosRead(hfile,
                         szHeader,
                         sizeof(szHeader),
                         &ulBytesRead);
            if (rc != NO_ERROR)
                break;

            // type suchen
            do
            {
                // prfe auf Cursor-Datei, bin„r-Header !
                ulHeader = *((PULONG) szHeader);
                ulFileType = RESFILETYPE_CURSOR;
                pszFileExt = QueryResFileExtension(RESFILETYPE_CURSOR);

                if ((strcmp(pszFileExt, pszExtension) == 0) &&
                    (ulHeader == RESFILEHEAD_CURSOR))
                {
                    *pulFileType = ulFileType;
                    break;
                }

                // - prfe auf Win Animation file
                pszFileHeader = RESFILEHEAD_WINANIMATION;
                ulFileType = RESFILETYPE_WINANIMATION;
                pszFileExt = QueryResFileExtension(RESFILETYPE_WINANIMATION);

                if ((strcmp(pszFileExt, pszExtension) == 0) &&
                (memcmp(pszFileHeader, szHeader, strlen(pszFileHeader)) == 0))
                {
                    *pulFileType = ulFileType;
                    break;
                }

                // - prfe auf DLL  (Anmimouse)
                pszFileHeader = RESFILEHEAD_ANIMOUSE;
                ulFileType = RESFILETYPE_ANIMOUSE;
                pszFileExt = QueryResFileExtension(RESFILETYPE_ANIMOUSE);

                if ((strcmp(pszFileExt, pszExtension) == 0) &&
                (memcmp(pszFileHeader, szHeader, strlen(pszFileHeader)) == 0))
                {
                    *pulFileType = ulFileType;
                    break;
                }

                // - prfe auf Color Pointer und bw Pointer
                // - Extension nicht vergleichen (warum nicht ?)
                pszFileHeader = RESFILEHEAD_COLORPTR;
                ulFileType = RESFILETYPE_POINTER;

                if (memcmp(pszFileHeader, szHeader, strlen(pszFileHeader)) == 0)
                {
                    *pulFileType = ulFileType;
                    break;
                }

                pszFileHeader = RESFILEHEAD_BWPTR;
                ulFileType = RESFILETYPE_POINTER;
                if (memcmp(pszFileHeader, szHeader, strlen(pszFileHeader)) == 0)
                {
                    *pulFileType = ulFileType;
                    break;
                }

                // prfe auf AniMouse ScriptFile
                pszFileHeader = RESFILEHEAD_ANMFILE;
                ulFileType = RESFILETYPE_ANMFILE;
                pszFileExt = QueryResFileExtension(RESFILETYPE_ANMFILE);
                if ((strcmp(pszFileExt, pszExtension) == 0) &&
                (memcmp(pszFileHeader, szHeader, strlen(pszFileHeader)) == 0))
                {
                    *pulFileType = ulFileType;
                    break;
                }

            }
            while (FALSE);

        }                       // else if (fs3.attrFile & FILE_DIRECTORY)

        // Ergenis bergeben
        fSuccess = (*pulFileType != 0);
        if (pszFullName)
        {
            if (ulMaxLen < strlen(szFullName) + 1)
            {
                rc = ERROR_MORE_DATA;
                break;
            }
            strcpy(pszFullName, szFullName);
        }

    }
    while (FALSE);


// cleanup
    if (hfile)
        DosClose(hfile);

    return fSuccess;
}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : CreatePtrFromIconInfo                                      ³
 *³ Kommentar : Pointer erstellen                                          ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 24.07.1996                                                 ³
 *³ nderung  : 24.07.1996                                                 ³
 *³ aufgerufen: diverse                                                    ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : PHPOINTER - Zeiger auf Variable fr ptr handle             ³
 *³             PICONINFO - Zeiger auf ICONINFO                            ³
 *³ Aufgaben  : - deinitialisieren                                         ³
 *³ Rckgabe  : APIRET - OS/2 Fehlercode                                   ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

HPOINTER _Export CreatePtrFromIconInfo
 (
     PICONINFO piconinfo
)

{
    HPOINTER hptr = NULLHANDLE;

    APIRET rc;
    HFILE hfile = NULLHANDLE;
    ULONG ulAction;
    ULONG ulBytesRead;
    FILESTATUS3 fs3;

    HPS hps = NULLHANDLE;

    BOOL fEnhancedHeader = FALSE;
    ULONG ulBwColorTableLen;
    PBITMAPFILEHEADER2 pbfh2Pointer;
    PBITMAPINFOHEADER pbihPointer;
    PBITMAPINFOHEADER2 pbih2Pointer;
    PBITMAPFILEHEADER2 pbfh2Bitmap;
    PBITMAPINFOHEADER pbihBitmap;
    PBITMAPINFOHEADER2 pbih2Bitmap;

    PBYTE pbDataPointer;
    PBYTE pbDataBitmap;

    HBITMAP hbmPointer = NULLHANDLE;
    HBITMAP hbmBitmap = NULLHANDLE;
    BOOL fFormatChanged = FALSE;

    POINTERINFO ptri;

    do
    {
        // Parameter prfen
        if ((piconinfo == NULL) ||
            (piconinfo->cb != sizeof(ICONINFO)))
            break;

        // Pointer erstellen
        switch (piconinfo->fFormat)
        {
            case ICON_FILE:

                do
                {
                    // L„nge der PTR-Datei ermitteln
                    rc = DosQueryPathInfo(piconinfo->pszFileName, FIL_STANDARD, &fs3, sizeof(fs3));
                    if (rc != NO_ERROR)
                    {
                        DEBUGMSG("error: cannot determine file info %s,%u" NEWLINE, __FUNCTION__ _c_ __LINE__);
                        break;
                    }
                    piconinfo->cbIconData = fs3.cbFile;

                    // Speicher holen
                    if ((piconinfo->pIconData = malloc(piconinfo->cbIconData)) == NULL)
                    {
                        DEBUGMSG("error: cannot allocate memory line %s,%u" NEWLINE, __FUNCTION__ _c_ __LINE__);
                        rc = ERROR_NOT_ENOUGH_MEMORY;
                        break;
                    }

                    // Datei ”ffnen
                    rc = DosOpen(piconinfo->pszFileName,
                                 &hfile,
                                 &ulAction,
                                 0, 0,
                         OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS,
                                 OPEN_SHARE_DENYNONE | OPEN_ACCESS_READONLY,
                                 NULL);
                    if (rc != NO_ERROR)
                    {
                        DEBUGMSG("error: cannot open file %s,%u" NEWLINE, __FUNCTION__ _c_ __LINE__);
                        break;
                    }

                    // Datei lesen
                    rc = DosRead(hfile,
                                 piconinfo->pIconData,
                                 piconinfo->cbIconData,
                                 &ulBytesRead);
                    if (rc != NO_ERROR)
                    {
                        DEBUGMSG("error: cannot read file %s,%u" NEWLINE, __FUNCTION__ _c_ __LINE__);
                        break;
                    }

                }
                while (FALSE);

                // cleanup
                if (hfile)
                    DosClose(hfile);
                if ((rc != NO_ERROR) && (piconinfo->pIconData))
                    free(piconinfo->pIconData);

                // break only on error !
                if (rc != NO_ERROR)
                    break;

                // fall thru ! no break !!!
                piconinfo->fFormat = ICON_DATA;
                fFormatChanged = TRUE;

            case ICON_DATA:
                do
                {
                    // Presentation space holen
                    hps = WinBeginPaint(HWND_DESKTOP, NULLHANDLE, NULL);
                    if (hps == NULLHANDLE)
                    {
                        DEBUGMSG("error: cannot get presentation space %s,%u" NEWLINE, __FUNCTION__ _c_ __LINE__);
                        break;
                    }

                    // Dateityp prfen
                    pbfh2Pointer = (PBITMAPFILEHEADER2) piconinfo->pIconData;
                    if ((pbfh2Pointer->usType != BFT_COLORPOINTER) && (pbfh2Pointer->usType != BFT_POINTER))
                        break;

                    // eine Datei mit mehreren Headern wird nicht untersttzt
                    if ((pbfh2Pointer->cbSize == sizeof(BITMAPARRAYFILEHEADER)) ||
                     (pbfh2Pointer->cbSize == sizeof(BITMAPARRAYFILEHEADER2)))
                        break;
                    // hier sollte die erste Bitmap im Array verwendet werden

                    // ist es ein neuer Header ?
                    fEnhancedHeader = (pbfh2Pointer->cbSize != sizeof(BITMAPFILEHEADER));
                    if (fEnhancedHeader)
                        ulBwColorTableLen = 2 * sizeof(RGB2);
                    else
                        ulBwColorTableLen = 2 * sizeof(RGB);

                    // Bitmaps erstellen
                    switch (pbfh2Pointer->usType)
                    {
                        case BFT_COLORPOINTER:

                            pbfh2Bitmap = (PBITMAPFILEHEADER2)
                                ((PBYTE) piconinfo->pIconData +
                                 pbfh2Pointer->cbSize +
                                 ulBwColorTableLen);

                            pbihBitmap = (PBITMAPINFOHEADER) & pbfh2Bitmap->bmp2;
                            pbih2Bitmap = &pbfh2Bitmap->bmp2;
                            pbDataBitmap = (PBYTE) piconinfo->pIconData + pbfh2Bitmap->offBits;
                            hbmBitmap = GpiCreateBitmap(hps,
                                                        pbih2Bitmap,
                                                        CBM_INIT,
                                                        pbDataBitmap,
                                                  (PBITMAPINFO2) pbih2Bitmap);

                            // fall thru !!!

                        case BFT_POINTER:
                            pbihPointer = (PBITMAPINFOHEADER) & pbfh2Pointer->bmp2;
                            pbih2Pointer = &pbfh2Pointer->bmp2;
                            pbDataPointer = (PBYTE) piconinfo->pIconData + pbfh2Pointer->offBits;

                            hbmPointer = GpiCreateBitmap(hps,
                                                         pbih2Pointer,
                                                         CBM_INIT,
                                                         pbDataPointer,
                                                 (PBITMAPINFO2) pbih2Pointer);

                            break;  // end case BFT_POINTER || BFT_COLORPOINTER

                    }           // end switch (pbfh2Pointer->usType)

                    // create Pointer
                    switch (pbfh2Pointer->usType)
                    {
                        case BFT_COLORPOINTER:

                            if ((hbmPointer == NULLHANDLE) || (hbmBitmap == NULLHANDLE))
                                break;

                            // create color pointer
                            ptri.fPointer = TRUE;
                            ptri.xHotspot = pbfh2Pointer->xHotspot;
                            ptri.yHotspot = pbfh2Pointer->yHotspot;
                            ptri.hbmPointer = hbmPointer;
                            ptri.hbmColor = hbmBitmap;

                            hptr = WinCreatePointerIndirect(HWND_DESKTOP, &ptri);
                            break;  // end case BFT_COLORPOINTER

                        case BFT_POINTER:

                            hptr = WinCreatePointer(HWND_DESKTOP, hbmPointer, TRUE,
                              pbfh2Pointer->xHotspot, pbfh2Pointer->yHotspot);

                            break;  // end case BFT_POINTER

                    }           // end switch (pbfh2Pointer->usType)

                }
                while (FALSE);

                // clean up
                if (hps)
                    WinEndPaint(hps);
                if (hbmPointer)
                    GpiDeleteBitmap(hbmPointer);
                if (hbmBitmap)
                    GpiDeleteBitmap(hbmBitmap);
                // cleanup for fallthru
                if ((fFormatChanged) && (hptr == NULLHANDLE))
                {
                    // Struktur zurcksetzen
                    piconinfo->fFormat = ICON_FILE;
                    if (piconinfo->pIconData)
                    {
                        free(piconinfo->pIconData);
                        piconinfo->pIconData = NULL;
                    }
                }

                break;          // end case ICON_DATA

            case ICON_RESOURCE:
                hptr = WinLoadPointer(HWND_DESKTOP, piconinfo->hmod, piconinfo->resid);
                break;

            default:
                break;

        }                       // end switch (piconifo->fFormat)

    }
    while (FALSE);

// handle zurckgeben
    return hptr;

}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : WritePointerToPointerFile                                  ³
 *³ Kommentar : Schreibt Pointer in PTR Datei                              ³
 *³             Die Struktur ICONINFO muá dazu die Daten enthalten, d.h.   ³
 *³             fFormat = ICON_DATA sein.                                  ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 16.03.1998                                                 ³
 *³ nderung  : 16.03.1998                                                 ³
 *³ aufgerufen: diverse                                                    ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : PSZ          - Name der Pointerdatei                       ³
 *³             PICONINFO    - Pointerdaten                                ³
 *³             PSZ          - Name der Animation                          ³
 *³             PSZ          - Name des Authors                            ³
 *³ Aufgaben  : - Pointer laden                                            ³
 *³ Rckgabe  : APIRET - OS/2 Fehlercode                                   ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

APIRET _Export WritePointerToPointerFile
 (
     PSZ pszName,
     ULONG ulTimeout,
     PICONINFO pIconInfo,
     PSZ pszInfoName,
     PSZ pszInfoArtist
)
{
    return WritePointerFile(pszName, ulTimeout, pIconInfo, pszInfoName, pszInfoArtist);
}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : WritePointerToCursorFile                                   ³
 *³ Kommentar : Schreibt Pointer in CUR Datei                              ³
 *³             Die Struktur ICONINFO muá dazu die Daten enthalten, d.h.   ³
 *³             fFormat = ICON_DATA sein.                                  ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 16.03.1998                                                 ³
 *³ nderung  : 16.03.1998                                                 ³
 *³ aufgerufen: diverse                                                    ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : PSZ          - Name der Pointerdatei                       ³
 *³             PICONINFO    - Pointerdaten                                ³
 *³             PSZ          - Name der Animation                          ³
 *³             PSZ          - Name des Authors                            ³
 *³ Aufgaben  : - Pointer laden                                            ³
 *³ Rckgabe  : APIRET - OS/2 Fehlercode                                   ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

APIRET _Export WritePointerToCursorFile
 (
     PSZ pszName,
     ULONG ulTimeout,
     PICONINFO pIconInfo,
     PSZ pszInfoName,
     PSZ pszInfoArtist
)
{
    return WriteCursorFile(pszName, ulTimeout, pIconInfo, pszInfoName, pszInfoArtist);
}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : WritePointerlistToPointerFile                              ³
 *³ Kommentar : Schreibt Pointerliste in PTR Datei                         ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 16.03.1998                                                 ³
 *³ nderung  : 16.03.1998                                                 ³
 *³ aufgerufen: diverse                                                    ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : PSZ          - Name der ANI Datei                          ³
 *³             PPOINTERLIST - Pointerliste                                ³
 *³             PSZ          - Speicher fr Dateiname im Fehlerfall        ³
 *³             ULONG        - Gr”áe des Speicherbereichs                  ³
 *³ Aufgaben  : - Pointer laden                                            ³
 *³ Rckgabe  : APIRET - OS/2 Fehlercode                                   ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

APIRET _Export WritePointerlistToPointerFile
 (
     PSZ pszName,
     PPOINTERLIST ppl,
     PSZ pszError,
     ULONG ulBuflen
)
{
    APIRET rc = NO_ERROR;
    ULONG i;
    CHAR szName[_MAX_PATH];
    BOOL fWriteMultipleFiles;

    do
    {

        // check parms
        if ((pszName == NULL) ||
            (ppl == NULL))
        {
            rc = ERROR_INVALID_PARAMETER;
            break;
        }

        // save animation name
        if (ppl->fModifyNameOnSave)
            strcpy(ppl->szAnimationFile, pszName);


        // write static pointer, if it was not
        // copied from 1st pointer of animation
        strcpy(szName, pszName);
        if ((ppl->hptrStatic) && (!ppl->fStaticPointerCopied))
        {
            // write the file
            rc = WritePointerToPointerFile(szName,
                                           0,
                                           &ppl->iconinfoStatic,
                                           ppl->pszInfoName,
                                           ppl->pszInfoArtist);
            if (rc != NO_ERROR)
                break;
        }

        // change filename only if more than one
        // pointer or static pointer was not copied
        fWriteMultipleFiles = ((ppl->ulPtrCount > 1) || (!ppl->fStaticPointerCopied));

        if (fWriteMultipleFiles)
        {
            // add numeration to filename
            rc = ChangeFilename(pszName,
                                CHANGE_ADDNUMERATION,
                                szName,
                                sizeof(szName),
                                NULL, 0, 0);
            if (rc != NO_ERROR)
                break;

        }                       // if (fWriteMultipleFiles)


        // save animation file name
        if (ppl->fModifyNameOnSave)
            strcpy(ppl->szAnimationFile, szName);

        // write pointerlist to PTR file
        for (i = 0; i < ppl->ulPtrCount; i++)
        {
            if (fWriteMultipleFiles)
            {
                // count up pointername
                rc = ChangeFilename(szName,
                                    CHANGE_SETNUMERATION,
                                    szName,
                                    sizeof(szName),
                                    NULL, 0, i);
                if (rc != NO_ERROR)
                    break;

            }                   // if (fWriteMultipleFiles)

            // write the file
            rc = WritePointerToPointerFile(szName,
                                           0,
                                           &ppl->iconinfo[i],
                                           ppl->pszInfoName,
                                           ppl->pszInfoArtist);
            if (rc != NO_ERROR)
                break;
        }


    }
    while (FALSE);


// report file name
    if ((rc != NO_ERROR) && (pszError != NULL))
    {
        strncpy(pszError, szName, ulBuflen);
        *(pszError + ulBuflen - 1) = 0;
    }
    return rc;
}


/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : WritePointerlistToCursorFile                               ³
 *³ Kommentar : Schreibt Pointerliste in CUR Datei                         ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 16.03.1998                                                 ³
 *³ nderung  : 16.03.1998                                                 ³
 *³ aufgerufen: diverse                                                    ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : PSZ          - Name der ANI Datei                          ³
 *³             PPOINTERLIST - Pointerliste                                ³
 *³             PSZ          - Speicher fr Dateiname im Fehlerfall        ³
 *³             ULONG        - Gr”áe des Speicherbereichs                  ³
 *³ Aufgaben  : - Pointer laden                                            ³
 *³ Rckgabe  : APIRET - OS/2 Fehlercode                                   ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

APIRET _Export WritePointerlistToCursorFile
 (
     PSZ pszName,
     PPOINTERLIST ppl,
     PSZ pszError,
     ULONG ulBuflen
)
{
    APIRET rc = NO_ERROR;
    ULONG i;
    CHAR szName[_MAX_PATH];
    BOOL fWriteMultipleFiles;

    do
    {

        // check parms
        if ((pszName == NULL) ||
            (ppl == NULL))
        {
            rc = ERROR_INVALID_PARAMETER;
            break;
        }

        // save animation name
        if (ppl->fModifyNameOnSave)
            strcpy(ppl->szAnimationFile, pszName);

        // write static pointer, if it was not
        // copied from 1st pointer of animation
        strcpy(szName, pszName);
        if ((ppl->hptrStatic) && (!ppl->fStaticPointerCopied))
        {
            // write the file
            rc = WritePointerToCursorFile(szName,
                                          0,
                                          &ppl->iconinfoStatic,
                                          ppl->pszInfoName,
                                          ppl->pszInfoArtist);
            if (rc != NO_ERROR)
                break;
        }

        // change filename only if more than one
        // pointer or static pointer was not copied
        fWriteMultipleFiles = ((ppl->ulPtrCount > 1) || (!ppl->fStaticPointerCopied));

        if (fWriteMultipleFiles)
        {
            // add numeration to filename
            rc = ChangeFilename(pszName,
                                CHANGE_ADDNUMERATION,
                                szName,
                                sizeof(szName),
                                NULL, 0, 0);
            if (rc != NO_ERROR)
                break;

        }                       // if (fWriteMultipleFiles)

        // save animation file name
        if (ppl->fModifyNameOnSave)
            strcpy(ppl->szAnimationFile, szName);

        // write pointerlist to CIR file
        for (i = 0; i < ppl->ulPtrCount; i++)
        {
            if (fWriteMultipleFiles)
            {
                // count up pointername
                rc = ChangeFilename(szName,
                                    CHANGE_SETNUMERATION,
                                    szName,
                                    sizeof(szName),
                                    NULL, 0, i);
                if (rc != NO_ERROR)
                    break;

            }                   // if (fWriteMultipleFiles)

            // write the file
            rc = WritePointerToCursorFile(szName,
                                          0,
                                          &ppl->iconinfo[i],
                                          ppl->pszInfoName,
                                          ppl->pszInfoArtist);
            if (rc != NO_ERROR)
                break;
        }


    }
    while (FALSE);


// report file name
    if ((rc != NO_ERROR) && (pszError != NULL))
    {
        strncpy(pszError, szName, ulBuflen);
        *(pszError + ulBuflen - 1) = 0;
    }
    return rc;

}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : WritePointerlistToWinAnimationFile                         ³
 *³ Kommentar : Schreibt Pointerliste in ANI Datei                         ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 16.03.1998                                                 ³
 *³ nderung  : 16.03.1998                                                 ³
 *³ aufgerufen: diverse                                                    ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : PSZ          - Name der ANI Datei                          ³
 *³             PPOINTERLIST - Pointerliste                                ³
 *³             PSZ          - Speicher fr Dateiname im Fehlerfall        ³
 *³             ULONG        - Gr”áe des Speicherbereichs                  ³
 *³ Aufgaben  : - Pointer laden                                            ³
 *³ Rckgabe  : APIRET - OS/2 Fehlercode                                   ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

APIRET _Export WritePointerlistToWinAnimationFile
 (
     PSZ pszName,
     PPOINTERLIST ppl
)
{
    APIRET rc = NO_ERROR;
    ULONG i;

    do
    {

        // check parms
        if ((pszName == NULL) ||
            (ppl == NULL))
        {
            rc = ERROR_INVALID_PARAMETER;
            break;
        }

        // save animation name
        if (ppl->fModifyNameOnSave)
            strcpy(ppl->szAnimationFile, pszName);

        // is pointerlist empty ?
        if (ppl->ulPtrCount == 0)
            break;

        // replace zero timeouts with default animation rate
        // !!! do not use current setting to have reproducable results !!!
        for (i = 0; i < ppl->ulPtrCount; i++)
        {
            if (ppl->aulTimer[i] == 0)
                ppl->aulTimer[i] = DEFAULT_ANIMATION_TIMEOUT;
        }

        // write pointerlist to ANI file
        rc = WriteWinAnimationFile(pszName,
                                   &ppl->aulTimer[0],
                                   &ppl->iconinfo[0],
                                   ppl->ulPtrCount,
                                   ppl->pszInfoName,
                                   ppl->pszInfoArtist);

    }
    while (FALSE);

// cleanup
    return rc;

}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : WriteAnimationSetToPointerFile                             ³
 *³ Kommentar : Schreibt Pointer in diverse Zeildateien                    ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 16.03.1998                                                 ³
 *³ nderung  : 16.03.1998                                                 ³
 *³ aufgerufen: diverse                                                    ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : PSZ          - Name der Pointerdatei                       ³
 *³             PPOINTERLIST - Pointerliste                                ³
 *³             PSZ          - Speicher fr Dateiname im Fehlerfall        ³
 *³             ULONG        - Gr”áe des Speicherbereichs                  ³
 *³ Aufgaben  : - Pointer laden                                            ³
 *³ Rckgabe  : APIRET - OS/2 Fehlercode                                   ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

APIRET _Export WriteAnimationSetToPointerFile
 (
     PSZ pszName,
     PPOINTERLIST papl,
     PSZ pszError,
     ULONG ulBuflen
)
{
    APIRET rc = NO_ERROR;
    ULONG i;
    CHAR szName[_MAX_PATH];
    PPOINTERLIST ppl = papl;

    do
    {
        // check parms
        if ((pszName == NULL) ||
            (papl == NULL))
        {
            rc = ERROR_INVALID_PARAMETER;
            break;
        }

        // save animation name
        if (ppl->fModifyNameOnSave)
            strcpy(ppl->szAnimationFile, pszName);

        // write all pointerlists to PTR file
        for (i = 0; i < NUM_OF_SYSCURSORS; i++)
        {

            // change name to pointername
            rc = ChangeFilename(pszName,
                                CHANGE_USEPOINTERNAME,
                                szName,
                                sizeof(szName),
                                NULL, i, 0);
            if (rc != NO_ERROR)
                break;

            // write single pointerlist
            rc = WritePointerlistToPointerFile(szName,
                                               papl + i,
                                               pszError,
                                               ulBuflen);
            if (rc != NO_ERROR)
                break;
        }

    }
    while (FALSE);


    return rc;

}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : WriteAnimationSetToCursorFile                              ³
 *³ Kommentar : Schreibt Pointer in diverse Zeildateien                    ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 16.03.1998                                                 ³
 *³ nderung  : 16.03.1998                                                 ³
 *³ aufgerufen: diverse                                                    ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : PSZ          - Name der Pointerdatei                       ³
 *³             PPOINTERLIST - Pointerliste                                ³
 *³             PSZ          - Speicher fr Dateiname im Fehlerfall        ³
 *³             ULONG        - Gr”áe des Speicherbereichs                  ³
 *³ Aufgaben  : - Pointer laden                                            ³
 *³ Rckgabe  : APIRET - OS/2 Fehlercode                                   ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

APIRET _Export WriteAnimationSetToCursorFile
 (
     PSZ pszName,
     PPOINTERLIST papl,
     PSZ pszError,
     ULONG ulBuflen
)
{
    APIRET rc = NO_ERROR;
    ULONG i;
    CHAR szName[_MAX_PATH];
    PPOINTERLIST ppl = papl;

    do
    {
        // check parms
        if ((pszName == NULL) ||
            (papl == NULL))
        {
            rc = ERROR_INVALID_PARAMETER;
            break;
        }

        // save animation name
        if (ppl->fModifyNameOnSave)
            strcpy(ppl->szAnimationFile, pszName);

        for (i = 0; i < NUM_OF_SYSCURSORS; i++)
        {

            // change name to pointername
            rc = ChangeFilename(pszName,
                                CHANGE_USEPOINTERNAME,
                                szName,
                                sizeof(szName),
                                NULL, i, 0);
            if (rc != NO_ERROR)
                break;

            // write single pointerlist
            rc = WritePointerlistToCursorFile(szName,
                                              papl + i,
                                              pszError,
                                              ulBuflen);
            if (rc != NO_ERROR)
                break;
        }

    }
    while (FALSE);


// cleanup;
    return rc;

}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : WriteAnimationSetToWinAnimationFile                        ³
 *³ Kommentar : Schreibt Pointer in diverse Zeildateien                    ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 16.03.1998                                                 ³
 *³ nderung  : 16.03.1998                                                 ³
 *³ aufgerufen: diverse                                                    ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : PSZ          - Name der Pointerdatei                       ³
 *³             PPOINTERLIST - Pointerliste                                ³
 *³             PSZ          - Speicher fr Dateiname im Fehlerfall        ³
 *³             ULONG        - Gr”áe des Speicherbereichs                  ³
 *³ Aufgaben  : - Pointer laden                                            ³
 *³ Rckgabe  : APIRET - OS/2 Fehlercode                                   ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

APIRET _Export WriteAnimationSetToWinAnimationFile
 (
     PSZ pszName,
     PPOINTERLIST papl,
     PSZ pszError,
     ULONG ulBuflen
)
{
    APIRET rc = NO_ERROR;
    ULONG i;
    CHAR szName[_MAX_PATH];
    PPOINTERLIST ppl = papl;

    do
    {
        // check parms
        if ((pszName == NULL) ||
            (papl == NULL))
        {
            rc = ERROR_INVALID_PARAMETER;
            break;
        }

        // save animation name
        if (ppl->fModifyNameOnSave)
            strcpy(ppl->szAnimationFile, pszName);

        for (i = 0; i < NUM_OF_SYSCURSORS; i++)
        {
            // is pointerlist empty ?
            if ((papl + i)->ulPtrCount == 0)
                continue;

            // change name to pointername
            rc = ChangeFilename(pszName,
                                CHANGE_USEPOINTERNAME,
                                szName,
                                sizeof(szName),
                                NULL, i, 0);
            if (rc != NO_ERROR)
                break;

            // write single pointerlist
            rc = WritePointerlistToWinAnimationFile(szName,
                                                    papl + i);
            if (rc != NO_ERROR)
                break;
        }


    }
    while (FALSE);


// report file name
    if ((rc != NO_ERROR) && (pszError != NULL))
    {
        strncpy(pszError, szName, ulBuflen);
        *(pszError + ulBuflen - 1) = 0;
    }
    return rc;

}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : WritePointerlistToAnimouseFile                             ³
 *³ Kommentar : Schreibt eine Animation in eine AnimouseDll                ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 16.03.1998                                                 ³
 *³ nderung  : 16.03.1998                                                 ³
 *³ aufgerufen: diverse                                                    ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : PSZ          - Name der Pointerdatei                       ³
 *³             ULONG        - index des pointers                          ³
 *³             PPOINTERLIST - Pointerliste                                ³
 *³             PSZ          - Speicher fr Dateiname im Fehlerfall        ³
 *³             ULONG        - Gr”áe des Speicherbereichs                  ³
 *³ Aufgaben  : - Pointer laden                                            ³
 *³ Rckgabe  : APIRET - OS/2 Fehlercode                                   ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

APIRET _Export WritePointerlistToAnimouseFile
 (
     PSZ pszName,
     PPOINTERLIST ppl
)
{
    APIRET rc = NO_ERROR;
    ULONG i, j;

    do
    {
        // check parms
        if ((pszName == NULL) ||
            (ppl == NULL))
        {
            rc = ERROR_INVALID_PARAMETER;
            break;
        }

        // save animation name
        if (ppl->fModifyNameOnSave)
            strcpy(ppl->szAnimationFile, pszName);

        // fill in default timeout
        // !!! do not use current setting to have reproducable results !!!
        for (j = 0; j < ppl->ulPtrCount; j++)
        {
            if (ppl->aulTimer[j] == 0)
                ppl->aulTimer[j] = DEFAULT_ANIMATION_TIMEOUT;
        }

        // write resource dll
        rc = WriteAnimouseDllFile(pszName, FALSE, ppl, ppl->pszInfoName, ppl->pszInfoArtist);
        if (rc != NO_ERROR)
            break;

    }
    while (FALSE);


// cleanup;
    return rc;

}


/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : WriteAnimationSetToAnimouseFile                            ³
 *³ Kommentar : Schreibt alle Animationen in eine Animouse DLL             ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 16.03.1998                                                 ³
 *³ nderung  : 16.03.1998                                                 ³
 *³ aufgerufen: diverse                                                    ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : PSZ          - Name der Pointerdatei                       ³
 *³             PPOINTERLIST - Pointerliste                                ³
 *³ Aufgaben  : - Pointer laden                                            ³
 *³ Rckgabe  : APIRET - OS/2 Fehlercode                                   ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

APIRET _Export WriteAnimationSetToAnimouseFile
 (
     PSZ pszName,
     PPOINTERLIST papl
)
{
    APIRET rc = NO_ERROR;
    ULONG i, j;
    PPOINTERLIST ppl = papl;

    do
    {
        // check parms
        if ((pszName == NULL) ||
            (papl == NULL))
        {
            rc = ERROR_INVALID_PARAMETER;
            break;
        }

        // save animation name
        if (ppl->fModifyNameOnSave)
            strcpy(ppl->szAnimationFile, pszName);

        for (i = 0; i < NUM_OF_SYSCURSORS; i++)
        {
            ppl = papl + i;

            // replace zero timeouts with default animation rate
            // !!! do not use current setting to have reproducable results !!!
            for (j = 0; j < ppl->ulPtrCount; j++)
            {
                if (ppl->aulTimer[j] == 0)
                    ppl->aulTimer[j] = DEFAULT_ANIMATION_TIMEOUT;
            }
        }

        // write resource dll
        rc = WriteAnimouseDllFile(pszName, TRUE, papl, ppl->pszInfoName, ppl->pszInfoArtist);
        if (rc != NO_ERROR)
            break;

    }
    while (FALSE);


// cleanup;
    return rc;

}
