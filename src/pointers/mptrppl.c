// C Runtime
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>

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
#include "pointers\mptrptr.h"
#include "pointers\mptrcnr.h"
#include "pointers\mptrutil.h"
#include "pointers\mptranim.h"
#include "pointers\wmuser.h"
#include "pointers\r_wpamptr.h"
#include "pointers\pointer.h"
#include "pointers\cursor.h"
#include "pointers\dll.h"
#include "pointers\script.h"
#include "pointers\macros.h"
#include "pointers\debug.h"

// reload unterbinden
// #define SURPRESS_RELOAD

// global data
// Array fr Pointerlisten
static POINTERLIST appl[NUM_OF_SYSCURSORS];
static BOOL fPointerlistInitialized = FALSE;

// Daten fr Single Animationthread
static HMTX hmtxDataAccess = NULLHANDLE;
static HEV hevStartup = NULLHANDLE;
static TID tidAnimation = 0;

// global vars
static BOOL fDemoActive = FALSE;


/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ access mutex sem handle - accessed by REQUEST_DATA_ACCESS* macros      ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

HMTX QueryDataAccessSem(VOID)
{
    return hmtxDataAccess;
}

BOOL SetDataAccessSem(HMTX hmtx)
{
    hmtxDataAccess = hmtx;
    return TRUE;
}


/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : ResetAnimation                                             ³
 *³ Kommentar : Animation ausstellen und ersten Animationspointer setzen   ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 24.07.1996                                                 ³
 *³ nderung  : 24.07.1996                                                 ³
 *³ aufgerufen: diverse                                                    ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : -                                                          ³
 *³ Aufgaben  : - deinitialisieren                                         ³
 *³ Rckgabe  : APIRET - OS/2 Fehlercode                                   ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

APIRET ResetAnimation
 (
     PPOINTERLIST ppl,
     BOOL fSetPointer
)
{
    APIRET rc = NO_ERROR;
    PICONINFO piconinfo;

    do
    {
        // Parameter prfen
        if (ppl == NULL)
        {
            rc = ERROR_INVALID_PARAMETER;
            break;
        }

        // Animationsflag l”schen
        ppl->fAnimate = FALSE;

        // ersten Pointer aus Liste setzen
        piconinfo = &ppl->iconinfoStatic;
        if (piconinfo->fFormat == 0)
        {
            DEBUGMSG("warning: no static ptr available" NEWLINE, 0);
        }
        else
        {
            // etwas warten, dann klappt es immer
            DosSleep(50);
            if (!WinSetSysPointerData(HWND_DESKTOP,
                                      ppl->ulPtrId,
                                      piconinfo))
            {
                rc = ERRORIDERROR(WinGetLastError(WinQueryAnchorBlock(HWND_DESKTOP)));
//       printf( "error setting static pointer rc=%u/0x%08x" NEWLINE, rc , rc);
                //       BEEP;
            }
//    else
            //       printf( "setting static pointer ok" NEWLINE);

        }

        // Arrow-PTR direkt setzen
        if (fSetPointer)
            WinSetPointer(HWND_DESKTOP, ppl->hptrStatic);

    }
    while (FALSE);


    return rc;

}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : LoadAnimouseModule                                         ³
 *³ Kommentar : l„dt AnimouseModule (DosLoadModule)                        ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 21.03.1998                                                 ³
 *³ nderung  : 21.03.1998                                                 ³
 *³ aufgerufen: diverse                                                    ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : ULONG        - Index fr Pointer                           ³
 *³             PPOINTERLIST - Zielvariable fr PointerSet-Daten           ³
 *³             PSZ          - Name der Animationsdatei (voll              ³
 *³                            qualifiziert) oder Set-Name.                ³
 *³             ###                                                        ³
 *³ Aufgaben  : - Pointer laden                                            ³
 *³ Rckgabe  : BOOL - Flag: Pointerset geladen/nicht geladen              ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

APIRET LoadAnimouseModule
 (
     PSZ pszModuleName,
     PHMODULE phmodResource
)
{
    APIRET rc = NO_ERROR;
    CHAR szLoadError[_MAX_PATH];
    CHAR szFullname[_MAX_PATH];

    do
    {
        // is a full qualified pathname given ?
        if (!IsFilenameValid(pszModuleName, FILENAME_CONTAINSFULLNAME, NULL))
        {
            // query the full name
            rc = DosQueryPathInfo(pszModuleName,
                                  FIL_QUERYFULLNAME,
                                  szFullname,
                                  sizeof(szFullname));
            if (rc != NO_ERROR)
                break;

            pszModuleName = szFullname;
        }

        // load the module
        rc = DosLoadModule(szLoadError, sizeof(szLoadError),
                           pszModuleName, phmodResource);
    }
    while (FALSE);

    return rc;
}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : QueryFileDetails                                           ³
 *³ Kommentar : generic file detail load function                          ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 14.07.1996                                                 ³
 *³ nderung  : 14.07.1996                                                 ³
 *³ aufgerufen: diverse                                                    ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : ###                                                        ³
 *³ Aufgaben  : - Details holen                                            ³
 *³ returns   : APIRET - OS/2 error code                                   ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

APIRET _Export QueryFileDetails
 (
     PSZ pszAnimationFileName,
     ULONG ulResourceType,
     ULONG ulInfoLevel,
     PVOID pvData,
     ULONG ulBuflen,
     PULONG pulSize
)
{

    APIRET rc = NO_ERROR;

    switch (ulResourceType)
    {

        default:
            rc = ERROR_INVALID_PARAMETER;
            break;

        case RESFILETYPE_DEFAULT:
            if (pvData)
            {
                memset(pvData, 0, ulBuflen);
                *pulSize = ulBuflen;
            }
            else
                *pulSize = 0;
            rc = NO_ERROR;
            break;

        case RESFILETYPE_POINTER:
        case RESFILETYPE_POINTERSET:
            rc = QueryPointerFileDetails(pszAnimationFileName, ulInfoLevel, pvData, ulBuflen, pulSize);
            break;

        case RESFILETYPE_CURSOR:
        case RESFILETYPE_CURSORSET:
            rc = QueryCursorFileDetails(pszAnimationFileName, ulInfoLevel, pvData, ulBuflen, pulSize);
            break;

        case RESFILETYPE_ANIMOUSE:
        case RESFILETYPE_ANIMOUSESET:
            rc = QueryAnimouseFileDetails(pszAnimationFileName, ulInfoLevel, pvData, ulBuflen, pulSize);
            break;

        case RESFILETYPE_WINANIMATION:
        case RESFILETYPE_WINANIMATIONSET:
            rc = QueryWinAnimationFileDetails(pszAnimationFileName, ulInfoLevel, pvData, ulBuflen, pulSize);
            break;
    }

    return rc;
}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : LoadPointerAnimation                                       ³
 *³ Kommentar : l„dt Pointer oder Pointerset Animation                     ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 14.07.1996                                                 ³
 *³ nderung  : 14.07.1996                                                 ³
 *³ aufgerufen: diverse                                                    ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : ULONG        - Index fr Pointer                           ³
 *³             PPOINTERLIST - Zielvariable fr PointerSet-Daten           ³
 *³             PSZ          - Name der Animationsdatei (voll              ³
 *³                            qualifiziert) oder Set-Name.                ³
 *³             ###                                                        ³
 *³ Aufgaben  : - Pointer laden                                            ³
 *³ Rckgabe  : BOOL - Flag: Pointerset geladen/nicht geladen              ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

BOOL _Export LoadPointerAnimation
 (
     ULONG ulPtrIndex,
     PPOINTERLIST ppl,
     PSZ pszName,
     BOOL fLoadSet,
     BOOL fEnableIfAnimation,
     BOOL fRefresh
)
{

    BOOL fSuccess = FALSE;
    BOOL fLoaded = FALSE;

    BOOL fLoadError = FALSE;
    BOOL fPointerFound = FALSE;

    BOOL fSetComplete = FALSE;
    BOOL fIsSet = FALSE;

    BOOL fIsCursor = FALSE;
    BOOL fIsAniMouse = FALSE;
    BOOL fIsWinAnim = FALSE;

    APIRET rc;
    ULONG ulFileType = 0;

    PSZ pszMask;
    ULONG ulFirstPtr, ulLastPtr;
    BOOL fDllLoaded = FALSE;

    POINTERLIST plTmp;
    PPOINTERLIST pplTmp = &plTmp;
    PPOINTERLIST pplndx;

    CHAR szNameBackup[_MAX_PATH];

    ULONG i, j, k, l;
    CHAR szFullName[_MAX_PATH];
    CHAR szSearchMask[_MAX_PATH];
    CHAR szStaticName[_MAX_PATH];
    ULONG ulSingleSource;

    static PSZ apszSearchFormat[] =
    {"%s\\%s.and",
     "%s\\%s.ani",
     "%s\\%s%03u.ptr",
     "%s\\%s%03u.cur",
     "%s\\%s.ptr",
     "%s\\%s.cur"};

    static ULONG afIsCursor[] =
    {FALSE, FALSE, FALSE, TRUE, FALSE, TRUE};
    static ULONG afIsAniMouse[] =
    {TRUE, FALSE, FALSE, FALSE, FALSE, FALSE};
    static ULONG afIsWinAnim[] =
    {FALSE, TRUE, FALSE, FALSE, FALSE, FALSE};

    static ULONG afIsSet[] =
    {FALSE, FALSE, TRUE, TRUE, FALSE, FALSE};
    static ULONG afSetComplete[] =
    {FALSE, FALSE, FALSE, FALSE, TRUE, TRUE};

    ULONG ulFormatCount = sizeof(apszSearchFormat) / sizeof(PSZ);

    PSZ pszUsedSearchFormat;
    ULONG ulNumerationValue = 0;

    ULONG ulBootDrive = 0;
    HDIR hdirFile = HDIR_SYSTEM;
    FILEFINDBUF3 ffb3File;
    ULONG ulEntries = 1;
    BOOL fSetFound = FALSE;

    PVOID pvSourceInfo = NULL;
    PSOURCEINFO psi;
    ULONG ulSourceInfoLen;

    do
    {

        // Parameter prfen
        if (ppl == NULL)
            break;

        if (pszName == NULL)
        {
            ulFileType = RESFILETYPE_DEFAULT;
            szFullName[0] = 0;
        }
        else
        {
            if (!QueryResFileType(pszName, &ulFileType, szFullName, sizeof(szFullName)))
                break;
        }

        // prfen, ob ein Set geladen werden soll,
        // dann muá der Dateityp stimmen
        if (fLoadSet)
        {
            if ((ulFileType == RESFILETYPE_POINTER) ||
                (ulFileType == RESFILETYPE_CURSOR) ||
                (ulFileType == RESFILETYPE_WINANIMATION))
                break;
        }

        // Bootdrive und Maske fr Dateisuche ermitteln
        DosQuerySysInfo(QSV_BOOT_DRIVE, QSV_BOOT_DRIVE, &ulBootDrive, sizeof(ULONG));

        //
        // jetzt Dateien suchen und eintragen
        //

        // range fr Verarbeitung festlegen
        ulFirstPtr = ulPtrIndex;

        if (fLoadSet)
            ulLastPtr = NUM_OF_SYSCURSORS;
        else
            ulLastPtr = ulPtrIndex + 1;

        for (i = ulFirstPtr, pplndx = ppl; i < ulLastPtr; i++, pplndx++)
        {

#ifdef SURPRESS_RELOAD
            // ist das richtige schon geladen ?
            if (ulFileType != RESFILETYPE_DEFAULT)
            {
                if (strcmp(pplndx->szAnimationName, szFullName) == 0)
                    continue;
            }
#endif

            // tempor„re Pointerliste initialisieren
            memset(pplTmp, 0, sizeof(POINTERLIST));
            strcpy(pplTmp->szAnimationName, szFullName);
            strcpy(pplTmp->szAnimationFile, szFullName);
            pplTmp->ulPtrIndex = i;
            pplTmp->ulPtrId = QueryPointerSysId(i);

            fSetComplete = FALSE;
            fIsSet = FALSE;
            fPointerFound = FALSE;

            for (j = 0; (j < MAX_COUNT_POINTERS && (!fSetComplete)); j++)
            {

                switch (ulFileType)
                {

                        // -------------------------------------------------------

                    case RESFILETYPE_DEFAULT:
                        pplTmp->ulPtrCount = 1;
                        pplTmp->hptr[j] = 0;
                        fSetComplete = TRUE;
                        fPointerFound = TRUE;
                        break;

                        // -------------------------------------------------------

                    case RESFILETYPE_POINTER:
                    case RESFILETYPE_CURSOR:

                        if (IsFilenameValid(pplTmp->szAnimationName, FILENAME_CONTAINSNUMERATION, &ulNumerationValue))
                        {

                            // get next file in numeration
                            strcpy(szSearchMask, pplTmp->szAnimationName);

                            ChangeFilename(szSearchMask, CHANGE_SETNUMERATION,
                                           szSearchMask, sizeof(szSearchMask),
                                           NULL, 0, j + ulNumerationValue);

                            // does file exist ?
                            if (FileExist(szSearchMask, FALSE))
                            {

                                // load file
                                fLoaded = (ulFileType == RESFILETYPE_POINTER) ?
                                    LoadPointerFromPointerFile(szSearchMask,
                                                             &pplTmp->hptr[j],
                                                         &pplTmp->iconinfo[j],
                                                       &pplTmp->aulTimer[j]) :
                                    LoadPointerFromCursorFile(szSearchMask,
                                                              &pplTmp->hptr[j],
                                                         &pplTmp->iconinfo[j],
                                                        &pplTmp->aulTimer[j]);
                                if (fLoaded)
                                {
                                    pplTmp->ulPtrCount += 1;
                                    fPointerFound = TRUE;
                                }
                                else
                                {
                                    fLoadError = TRUE;
                                }
                            }
                            else
                            {
                                ulFileType = (ulFileType == RESFILETYPE_POINTER) ?
                                    RESFILETYPE_POINTERSET :
                                    RESFILETYPE_CURSORSET;
                                fSetComplete = TRUE;
                            }

                        }       // if (containsnumeration( pplTmp->szAnimationName))

                        else
                        {

                            // load file
                            fLoaded = (ulFileType == RESFILETYPE_POINTER) ?
                                LoadPointerFromPointerFile(pplTmp->szAnimationName,
                                                           &pplTmp->hptr[0],
                                                         &pplTmp->iconinfo[0],
                                                       &pplTmp->aulTimer[0]) :
                                LoadPointerFromCursorFile(pplTmp->szAnimationName,
                                                          &pplTmp->hptr[0],
                                                          &pplTmp->iconinfo[0],
                                                        &pplTmp->aulTimer[0]);
                            if (fLoaded)
                            {
                                pplTmp->ulPtrCount = 1;
                                fPointerFound = TRUE;
                            }
                            else
                                fLoadError = TRUE;
                            fSetComplete = TRUE;
                        }
                        break;


                        // -------------------------------------------------------

                    case RESFILETYPE_POINTERSET:
                    case RESFILETYPE_CURSORSET:
                    case RESFILETYPE_ANIMOUSESET:
                    case RESFILETYPE_WINANIMATIONSET:

                        // Pointer- oder Cursor Datei-Maske ermitteln
                        rc = ERROR_FILE_NOT_FOUND;
                        for (k = 0; ((k < ulFormatCount) && (rc != NO_ERROR)); k++)
                        {
                            // ist zuvor ein Set gefunden worden ?
                            // dann nur noch set-Masken suchen !
                            if (fIsSet)
                                if (!afIsSet[k])
                                    continue;

                            // Format bestimmen
                            pszUsedSearchFormat = apszSearchFormat[k];
                            sprintf(szSearchMask,
                                    pszUsedSearchFormat,
                                    szFullName,
                                    QueryPointerName(i),
                                    j);

                            // Pointer-Datei im Verzeichnis suchen
                            rc = (FileExist(szSearchMask, FALSE) ? NO_ERROR : ERROR_NO_MORE_FILES);

                            // Resource typ bestimmen
                            if (rc == NO_ERROR)
                            {

                                // typ herausfinden
                                ulFileType = RESFILETYPE_POINTERSET;

                                // Ist es ein Cursor ?
                                fIsCursor = afIsCursor[k];
                                if (fIsCursor)
                                    ulFileType = RESFILETYPE_CURSORSET;

                                // Ist es eine Win Animation ?
                                fIsWinAnim = afIsWinAnim[k];
                                if (fIsWinAnim)
                                    ulFileType = RESFILETYPE_WINANIMATIONSET;

                                // Ist es eine AniMouse Animation ?
                                fIsAniMouse = afIsAniMouse[k];
                                if (fIsAniMouse)
                                    ulFileType = RESFILETYPE_ANIMOUSESET;

                                // ist es ein Set ? Dann Suche begrenzen
                                fIsSet = afIsSet[k];
                            }

                            // ist alles geladen ?
                            fSetComplete = afSetComplete[k];

                            // hole statische Pointer
                            if ((fIsSet) && (!fIsAniMouse) && (pplTmp->hptrStatic == NULLHANDLE))
                            {
                                pszUsedSearchFormat = apszSearchFormat[k + 2];
                                sprintf(szStaticName,
                                        pszUsedSearchFormat,
                                        szFullName,
                                        QueryPointerName(i));

                                if (fIsCursor)
                                    LoadPointerFromCursorFile(szStaticName,
                                                          &pplTmp->hptrStatic,
                                                      &pplTmp->iconinfoStatic,
                                                              NULL);
                                else
                                    LoadPointerFromPointerFile(szStaticName,
                                                          &pplTmp->hptrStatic,
                                                      &pplTmp->iconinfoStatic,
                                                               NULL);
                            }


                        }       // end for (k < ulFormatCount)


                        if (rc == NO_ERROR)
                        {

                            // Name dieser Datei merken
                            if (pplTmp->ulPtrCount == 0)
                                strcpy(pplTmp->szAnimationFile, szSearchMask);

                            //
                            // Pointer in Liste einsetzen
                            //
                            if (fIsCursor)
                                fLoadError += (!LoadPointerFromCursorFile(szSearchMask,
                                                             &pplTmp->hptr[j],
                                                         &pplTmp->iconinfo[j],
                                                       &pplTmp->aulTimer[j]));
                            else if (fIsWinAnim)
                            {
                                // beim direkten Laden eines Sets vorherige Fehler verwerfen !
                                pplTmp->ulPtrCount = MAX_COUNT_POINTERS;
                                fLoadError = (!LoadPointerFromWinAnimationFile(szSearchMask,
                                                             &pplTmp->hptr[0],
                                                         &pplTmp->iconinfo[0],
                                                         &pplTmp->aulTimer[0],
                                                        &pplTmp->ulPtrCount));
                                fSetComplete = TRUE;
                                fPointerFound = !fLoadError;
                                break;
                            }
                            else if (fIsAniMouse)
                            {
                                // access animouse dll
                                if (pplTmp->hmodResource == NULL)
                                {
                                    rc = LoadAnimouseModule(szSearchMask, &pplTmp->hmodResource);
                                    if (rc != NO_ERROR)
                                    {
                                        fLoadError = TRUE;
                                        fSetComplete = TRUE;
                                        break;
                                    }
                                    else
                                        fDllLoaded = TRUE;
                                }

                                // beim direkten Laden eines Sets vorherige Fehler verwerfen !
                                pplTmp->ulPtrCount = MAX_COUNT_POINTERS;
                                fLoadError = (!LoadPointerFromAnimouseFile(pplTmp->hmodResource,
                                                                           i,
                                                             &pplTmp->hptr[j],
                                                         &pplTmp->iconinfo[j],
                                                          &pplTmp->hptrStatic,
                                                      &pplTmp->iconinfoStatic,
                                                         &pplTmp->aulTimer[0],
                                                        &pplTmp->ulPtrCount));
                                fSetComplete = TRUE;
                                fPointerFound = !fLoadError;
                                break;
                            }
                            else
                                fLoadError += (!LoadPointerFromPointerFile(szSearchMask,
                                                             &pplTmp->hptr[j],
                                                         &pplTmp->iconinfo[j],
                                                       &pplTmp->aulTimer[j]));

                            // Variablen weiterz„hlen
                            pplTmp->ulPtrCount += 1;
                            fPointerFound = TRUE;
                        }
                        else
                        {
                            // ist was gefunden worden ?
                            // if ((rc != ERROR_NO_MORE_FILES) || (pplTmp->ulPtrCount == 0))
                            //   fLoadError = TRUE;

                            fSetComplete = TRUE;
                            break;
                        }

                        break;  // case RESFILETYPE_POINTER RESFILETYPE_CURSOR

                        // -------------------------------------------------------

                    case RESFILETYPE_ANIMOUSE:

                        // access animouse dll
                        if (pplTmp->hmodResource == NULL)
                        {
                            CHAR szLoadError[_MAX_PATH];

                            rc = LoadAnimouseModule(pszName, &pplTmp->hmodResource);
                            if (rc != NO_ERROR)
                            {
                                fLoadError = TRUE;
                                fSetComplete = TRUE;
                                break;
                            }
                            else
                                fDllLoaded = TRUE;
                        }

                        pplTmp->ulPtrCount = MAX_COUNT_POINTERS;
                        fLoadError = (!LoadPointerFromAnimouseFile(pplTmp->hmodResource,
                                                                   i,
                                                             &pplTmp->hptr[j],
                                                         &pplTmp->iconinfo[j],
                                                          &pplTmp->hptrStatic,
                                                      &pplTmp->iconinfoStatic,
                                                         &pplTmp->aulTimer[0],
                                                        &pplTmp->ulPtrCount));

                        if (!fLoadError)
                        {
                            fPointerFound = ((pplTmp->hptrStatic != NULLHANDLE) ||
                                             (pplTmp->ulPtrCount != 0));
                        }
                        fSetComplete = TRUE;
                        break;  // case RESFILETYPE_ANIMOUSE


                        // -------------------------------------------------------
                    case RESFILETYPE_ANMFILE:
                        pplTmp->ulPtrCount = MAX_COUNT_POINTERS;
                        fLoadError = (!LoadPointerFromAnimouseScriptFile(pplTmp->szAnimationName,
                                                                         i,
                                                             &pplTmp->hptr[j],
                                                         &pplTmp->iconinfo[j],
                                                          &pplTmp->hptrStatic,
                                                      &pplTmp->iconinfoStatic,
                                                         &pplTmp->aulTimer[0],
                                                        &pplTmp->ulPtrCount));

                        if (!fLoadError)
                        {
                            fPointerFound = ((pplTmp->hptrStatic != NULLHANDLE) ||
                                             (pplTmp->ulPtrCount != 0));
                        }
                        fSetComplete = TRUE;
                        break;  // case RESFILETYPE_ANMFILE

                        // -------------------------------------------------------

                    case RESFILETYPE_WINANIMATION:
                        pplTmp->ulPtrCount = MAX_COUNT_POINTERS;
                        fLoadError += (!LoadPointerFromWinAnimationFile(pplTmp->szAnimationName,
                                                             &pplTmp->hptr[0],
                                                         &pplTmp->iconinfo[0],
                                                         &pplTmp->aulTimer[0],
                                                        &pplTmp->ulPtrCount));
                        fPointerFound = !fLoadError;
                        fSetComplete = TRUE;
                        break;  // case RESFILETYPE_WINANIMATION

                }               /* endswitch */

            }                   // end for (j < MAX_COUNT_POINTERS)

            // Resource-Typ sichern
            pplTmp->ulResourceType = ulFileType;

            // wenn alles ok ist, Pointerliste bernehmen
            if (fPointerFound)
            {
                // falls nicht extra geladen,
                // ersten Pointer der Animation als statischen Pointer verwenden
                if (pplTmp->hptrStatic == NULLHANDLE)
                {
                    pplTmp->fStaticPointerCopied = TRUE;
                    pplTmp->hptrStatic = pplTmp->hptr[0];
                    memcpy(&pplTmp->iconinfoStatic, &pplTmp->iconinfo[0], sizeof(ICONINFO));
                }

                // falls keine animierten Zeiger vorhanden sind,
                // statischen Pointer als ersten Pointer der Animation verwenden
                if ((pplTmp->hptr[0] == NULLHANDLE) && (pplTmp->ulPtrCount == 0))
                {
                    pplTmp->fStaticPointerCopied = TRUE;
                    pplTmp->hptr[0] = pplTmp->hptrStatic;
                    memcpy(&pplTmp->iconinfo[0], &pplTmp->iconinfoStatic, sizeof(ICONINFO));
                    pplTmp->ulPtrCount = 1;     // ### ist das notwendig ?

                }

                // ggfs. in Graustufen umrechnen
                if (getBlackWhite())
                {
                    GrayScalePointer(pplTmp->iconinfoStatic.pIconData, 2, 50);
                    WinDestroyPointer(pplTmp->hptrStatic);
                    pplTmp->hptrStatic = CreatePtrFromIconInfo(&pplTmp->iconinfoStatic);

                    for (l = 0; l < pplTmp->ulPtrCount; l++)
                    {
                        GrayScalePointer(pplTmp->iconinfo[l].pIconData, 2, 50);
                        WinDestroyPointer(pplTmp->hptr[l]);
                        pplTmp->hptr[l] = CreatePtrFromIconInfo(&pplTmp->iconinfo[l]);
                    }           /* endfor */
                }

                // ggfs. Animation ausstellen
                if (pplndx->fAnimate)
                    EnableAnimation(pplndx, FALSE);

                // Beschreibung laden: Gr”áe holen
                if (pplTmp->szAnimationFile[0] != 0)
                {
                    // Beschreibung laden: Gr”áe holen
                    QueryFileDetails(pplTmp->szAnimationFile, pplTmp->ulResourceType,
                                     FI_SOURCEINFO, NULL, 0, &ulSourceInfoLen);

                    if (ulSourceInfoLen)
                    {
                        pvSourceInfo = malloc(ulSourceInfoLen);
                        if (pvSourceInfo)
                        {
                            // Beschreibung laden: Daten holen und speichern
                            QueryFileDetails(pplTmp->szAnimationFile, pplTmp->ulResourceType, FI_SOURCEINFO, pvSourceInfo, ulSourceInfoLen, &ulSourceInfoLen);
                            psi = (PSOURCEINFO) pvSourceInfo;
                            if (psi->pszInfoName)
                                pplTmp->pszInfoName = strdup(psi->pszInfoName);
                            if (psi->pszInfoArtist)
                                pplTmp->pszInfoArtist = strdup(psi->pszInfoArtist);
                            free(pvSourceInfo);

                        }       // if (pvSourceinfo)

                    }           // if (ulSourceInfoLen)

                }               // if (pplTmp->szAnimationFile[ 0] != 0)

                // aktuelle Liste l”schen
                CopyPointerlist(NULL, pplndx, FALSE);

                // tempor„re Liste kopieren
                CopyPointerlist(pplndx, pplTmp, TRUE);

                // bei mehr als einem Poiner: Animation anstellen
                if (getActivateOnLoad())
                    if ((fEnableIfAnimation) && (pplndx->ulPtrCount > 1))
                        EnableAnimation(pplndx, TRUE);

            }

        }                       // end for (i < NUM_OF_SYSCURSORS)

        // Fehler aufgetreten ?
        if (!fLoadError)
            fSuccess = TRUE;

    }
    while (FALSE);

    return fSuccess;
}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : CopyPointerlist                                            ³
 *³ Kommentar : POINTERLIST Struktur kopieren oder "l”schen": Ressource    ³
 *³             freigeben.                                                 ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 20.07.1996                                                 ³
 *³ nderung  : 20.07.1996                                                 ³
 *³ aufgerufen: diverse                                                    ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : PPOINTERLIST - Zeiger auf Ziel-Struktur                    ³
 *³             PPOINTERLIST - Zeiger auf Quell-Struktur oder NULL         ³
 *³             BOOL         - Flag zum Kopieren der Resourcen             ³
 *³ Aufgaben  : - Speicher freigeben und Struktur kopieren                 ³
 *³ Rckgabe  : -                                                          ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

BOOL _Export CopyPointerlist
 (
     PPOINTERLIST pplTarget,
     PPOINTERLIST pplSource,
     BOOL fCopyResources
)
{
    BOOL fSuccess = FALSE;
    ULONG i;
    PICONINFO pIconInfo;
    ULONG ulCopyLen;
    APIRET rc;

    do
    {
        // Parameter prfen
        if (pplSource == NULL)
            break;

        // Zeiger drfen nicht gleich sein
        // sonst wird beim Kopieren gel”scht
        if (pplSource == pplTarget)
            break;

        // wenn nicht kopiert werden soll, Resourcen l”schen
        if (pplTarget == NULL)
        {

            // statischen Zeiger l”schen
            pplSource->fStaticPointerCopied = FALSE;
            if (pplSource->hptrStatic)
                WinDestroyPointer(pplSource->hptrStatic);

            // info freigeben
            if (pplSource->pszInfoName)
                free(pplSource->pszInfoName);

            if (pplSource->pszInfoArtist)
                free(pplSource->pszInfoArtist);

            // ggfs. AniMouse DLL freigeben
            if (pplSource->hmodResource)
                DosFreeModule(pplSource->hmodResource);

            // Animation l”schen
            for (i = 0; i < pplSource->ulPtrCount; i++)
            {
                // Pointer freigeben
                if (pplSource->hptr[i] != 0)
                    WinDestroyPointer(pplSource->hptr[i]);

                // ICONINFO freigeben
                pIconInfo = &pplSource->iconinfo[i];
                if (pIconInfo->pszFileName)
                    free(pIconInfo->pszFileName);
                if (pIconInfo->hmod)
                    DosFreeResource(pIconInfo->pIconData);
                if (pIconInfo->pIconData)
                    free(pIconInfo->pIconData);

                // timeout l”schen
                pplSource->aulTimer[i] = 0;

            }                   // end for i < pplSource->ulPtrCount

            // komplette Struktur zurcksetzen
            memset(pplSource, 0, sizeof(POINTERLIST));

        }                       // end if

        else
        {
            // Struktur kopieren
            memset(pplTarget, 0, sizeof(POINTERLIST));

            ulCopyLen = (fCopyResources) ?
                sizeof(POINTERLIST) : FIELDOFFSET(POINTERLIST, fReplaceActive);
            memcpy(pplTarget, pplSource, ulCopyLen);

            // info kopieren
            if (pplSource->pszInfoName)
                pplTarget->pszInfoName = strdup(pplSource->pszInfoName);
            if (pplSource->pszInfoArtist)
                pplTarget->pszInfoArtist = strdup(pplSource->pszInfoArtist);

            // alles ok
            fSuccess = TRUE;

        }                       // end else

    }
    while (FALSE);

    return fSuccess;

}


/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : IsPointerlistInitialized                                   ³
 *³ Kommentar : POINTERLIST Struktur Initialisierung bestimmen             ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 24.07.1996                                                 ³
 *³ nderung  : 24.07.1996                                                 ³
 *³ aufgerufen: diverse                                                    ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : -                                                          ³
 *³ Aufgaben  : - Initialisierung prfen                                   ³
 *³ Rckgabe  : BOO -  Flag, ob initialisiert                              ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

BOOL IsPointerlistInitialized(VOID)
{
    BOOL fInitialized;

    DosEnterCritSec();
    fInitialized = fPointerlistInitialized;
    DosExitCritSec();
    return fInitialized;
}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : InitializePointerlist                                      ³
 *³ Kommentar : POINTERLIST Struktur initialisieren                        ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 24.07.1996                                                 ³
 *³ nderung  : 24.07.1996                                                 ³
 *³ aufgerufen: diverse                                                    ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : -                                                          ³
 *³ Aufgaben  : - Initialisieren                                           ³
 *³ Rckgabe  : APIRET - OS/2 Fehlercode                                   ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

APIRET InitializePointerlist
 (
     HAB hab,
     HMODULE hmodResource
)

{
    APIRET rc = NO_ERROR;
    ULONG i;
    BOOL fLoadError = FALSE;
    PPOINTERLIST ppl;

    do
    {
        // ist bereits initialisiert ?
        if (IsPointerlistInitialized())
        {
            rc = ERROR_ACCESS_DENIED;
            break;
        }

        // Flag setzen
        DosEnterCritSec();
        fPointerlistInitialized = TRUE;
        DosExitCritSec();

        // Resourcen fr Container laden
        InitStringResources(hab, hmodResource);

        // Zugriffs-Semaphore erstellen
        rc = DosCreateMutexSem(NULL,
                               &hmtxDataAccess,
                               0,
                               TRUE);   // Thread erst mal blokieren

        if (rc != NO_ERROR)
            break;

        if (!DEBUGSETTING(SET_INACTIVEANIMATETHREAD))
        {

            // Startup-Semaphore erstellen
            rc = DosCreateEventSem(NULL,
                                   &hevStartup,
                                   0,
                                   FALSE);  // Direkt erst mal blokieren

            if (rc != NO_ERROR)
                break;

            // Animations-Thread starten
            tidAnimation = _beginthread(AnimationThread,
                                        NULL,
                                        16384,
                                        &hevStartup);
            rc = (tidAnimation == -1);
            if (rc != NO_ERROR)
                break;


            // startup abwarten
            rc = DosWaitEventSem(hevStartup, 20000L);
            DosCloseEventSem(hevStartup);
            if (rc != NO_ERROR)
            {
                BEEP4;          // waiting for startup sem failed on InitializePointerlist

                break;
            }
        }

        // alle POINTERLISTen initialisieren
        for (i = 0, ppl = QueryPointerlist(i);
             i < NUM_OF_SYSCURSORS;
             i++, ppl++)
        {
            // Variablen setzen
            memset(ppl, 0, sizeof(POINTERLIST));
            ppl->ulPtrId = QueryPointerSysId(i);

        }                       // end for (i < NUM_OF_SYSCURSORS)

        // ist ein Fehler aufgetreten ?
        if (fLoadError)
            rc = ERROR_GEN_FAILURE;

        // Zugriff freigeben
        rc = RELEASE_DATA_ACCESS();

    }
    while (FALSE);

    return rc;

}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : DeinitializePointerlist                                    ³
 *³ Kommentar : POINTERLIST Struktur deinitialisieren                      ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 24.07.1996                                                 ³
 *³ nderung  : 24.07.1996                                                 ³
 *³ aufgerufen: diverse                                                    ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : -                                                          ³
 *³ Aufgaben  : - deinitialisieren                                         ³
 *³ Rckgabe  : APIRET - OS/2 Fehlercode                                   ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

APIRET DeinitializePointerlist(VOID)

{
    APIRET rc = NO_ERROR;
    ULONG i;
    BOOL fLoadError = FALSE;
    PPOINTERLIST ppl;
    TID tidKilled = 0;
    HWND hwndAnimation;

    FUNCENTER();

    do
    {
        // ist noch nicht initialisiert ?
        if (!IsPointerlistInitialized())
        {
            rc = ERROR_ACCESS_DENIED;
            break;
        }

        // get data access
        REQUEST_DATA_ACCESS();

        // Flag setzen
        DosEnterCritSec();
        fPointerlistInitialized = FALSE;
        DosExitCritSec();

        // alle POINTERLISTen deinitialisieren
        for (i = 0, ppl = QueryPointerlist(i);
             i < NUM_OF_SYSCURSORS;
             i++, ppl++)
        {
            // Animation ausschalten
            EnableAnimation(ppl, FALSE);

//    // wird bereits in EnableAnimation() durchgefhrt
            //    // Animation zurcksetzen
            //    ResetAnimation( ppl, (i == 0));

            // Resourcen l”schen
            CopyPointerlist(NULL, ppl, FALSE);

        }                       // end for (i < NUM_OF_SYSCURSORS)

        // end animation thread, if not yet done
        WinPostMsg(QueryAnimationHwnd(), WM_CLOSE, 0L, 0L);

    }
    while (FALSE);

    return rc;
}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : QueryPointerlist                                           ³
 *³ Kommentar : Zeiger auf Pointerliste(n) zurckgeben                     ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 24.07.1996                                                 ³
 *³ nderung  : 24.07.1996                                                 ³
 *³ aufgerufen: diverse                                                    ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : -                                                          ³
 *³ Aufgaben  : - deinitialisieren                                         ³
 *³ Rckgabe  : HMTX - Handle der Semaphore                                ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

PPOINTERLIST QueryPointerlist
 (
     ULONG ulIndex
)
{

    if (ulIndex > NUM_OF_SYSCURSORS)
        return 0;
    else
        return &appl[ulIndex];
}


/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : EnableAnimation                                            ³
 *³ Kommentar : (de)aktiviert den Animationsthread fr einen Pointer       ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 20.07.1996                                                 ³
 *³ nderung  : 20.07.1996                                                 ³
 *³ aufgerufen: Window Procedure                                           ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : PPOINTERLIST - Zeiger auf Pointerliste                     ³
 *³ Aufgaben  : - Animation ein/ausstellen                                 ³
 *³ Rckgabe  : BOOL - Flag, ob erfolgreich                                ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

BOOL EnableAnimation
 (
     PPOINTERLIST ppl,
     BOOL fEnable
)

{
    BOOL fSuccess = FALSE;
    APIRET rc;
    ULONG ulPostCount;
    ULONG ulTimerId;
    ULONG ulTimeout;
    HWND hwndAnimation = QueryAnimationHwnd();
    HAB habAnimationThread = QueryAnimationHab();

    do
    {

        // Parameter prfen
        if (ppl == NULL)
            break;

        // ist Animation m”glich ?
        if (ppl->ulPtrCount <= 1)
            break;


// // Prfen, ob Animation bereits ein/ausgeschaltet ist
        // if (ppl->fAnimate == fEnable)
        //    {
        //    fSuccess = TRUE;
        //    break;
        //    }

        // timer Id ermitteln
        ulTimerId = (ppl - (QueryPointerlist(0))) + 1;
        if (ulTimerId > NUM_OF_SYSCURSORS)
            break;

        // prfen, ob Thread da ist
        if (tidAnimation == 0)
        {
            // kein Fehler, wenn deaktiviert werden soll
            fSuccess = !fEnable;
            break;
        }

        // prfen, ob Object Fenster noch da ist
        if ((hwndAnimation == NULLHANDLE) || (!WinIsWindow(habAnimationThread, hwndAnimation)))
        {
            // kein Fehler, wenn deaktiviert werden soll
            fSuccess = !fEnable;
            break;
        }

        // Timer starten / stoppen
        if (DEBUGSETTING(SET_ANIMCMDASYNC))
        {
            fSuccess = (BOOL) WinPostMsg(hwndAnimation, WM_USER_ENABLEANIMATION, MPFROMP(ppl), MPFROMSHORT(fEnable));
            if (!fSuccess)
            {
                DosBeep(1000, 50);
                DosBeep(700, 50);
            }
        }
        else
            fSuccess = (BOOL) WinSendMsg(hwndAnimation, WM_USER_ENABLEANIMATION, MPFROMP(ppl), MPFROMSHORT(fEnable));

// // Animation zurcksetzen
        // if (!fEnable)
        //    {
        //    BEEP
        //    ResetAnimation( ppl, FALSE);
        //    }

    }
    while (FALSE);

    return fSuccess;
}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : SetNextAnimatedPointer                                     ³
 *³ Kommentar : prft, ob die Animation ein. bzw auszuschalten ist und     ³
 *³             l„dt ggfs. n„chsten Pointer aus dem Set                    ³
 *³             l„uft bereits mit exclusiv-Rechten auf die Datenstrukturen!³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 07.06.1996                                                 ³
 *³ nderung  : 07.06.1996                                                 ³
 *³ aufgerufen: diverse                                                    ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : HAB          - handle anchorblock                          ³
 *³           : PPOINTERLIST - Pointerliste                                ³
 *³ Aufgaben  : - Animation aktivieren / deaktivieren                      ³
 *³ Rckgabe  : BOOL - Flag: Erfolgsflag                                   ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

BOOL SetNextAnimatedPointer
 (
     HAB hab,
     PPOINTERLIST ppl,
     BOOL fForceSet
)

{
    HPOINTER hptrCurrent;
    BOOL fSuccess = FALSE;
    DATETIME dt;
    ULONG ulLastPointer;
    APIRET rc;
    PICONINFO piconinfo;
    BOOL fPointerActive = FALSE;

    do
    {

        // ist der Mauszeiger sichtbar ?
        if (!WinIsPointerVisible(HWND_DESKTOP))
        {
            fSuccess = TRUE;
            break;
        }

        // Parameter prfen
        if (ppl == NULL)
            break;

        // wenn die Ptr-Liste leer ist oder
        // nur einen Pointer hat, abbrechen
        if (ppl->ulPtrCount <= 1)
            break;

        // prfen, ob dieser Pointer der aktuelle ist
        fPointerActive = IsPointerActive(ppl->ulPtrId, &hptrCurrent);

        // Animation aktivieren bzw. deaktivieren
        if (!ppl->fReplaceActive)
        {

            // falls der pointer des systems aktiv ist, Animation aktivieren
            if (fPointerActive)
            {
                ppl->fReplaceActive = TRUE;
            }
        }
        else                    // if (ppl->fReplaceActive)

        {
            // prfen, ob deaktiviert werden muá

            BOOL fNotOwnPointer = TRUE;
            ULONG i;

            // wenn es nicht der sys ptr ist
            if (!fPointerActive)
            {

                // ist es ein eigener pointer ?
                for (i = 0; i < ppl->ulPtrCount; i++)
                {
                    if (hptrCurrent == ppl->hptr[i])
                    {
                        fNotOwnPointer = FALSE;
                        break;
                    }
                }

                if (fNotOwnPointer)
                {
                    // wurde von jemandem anderen ein Pointer gesetzt:
                    // jetzt Animation deaktivieren

                    ppl->fReplaceActive = FALSE;

                }
            }
        }

        // ************************************************

        if ((ppl->fReplaceActive) || (fForceSet))
        {
            // n„chsten Pointer in der Animation ausw„hlen
            ppl->ulPtrIndex++;
            if (ppl->ulPtrIndex >= ppl->ulPtrCount)
                ppl->ulPtrIndex = 0;

            if (ppl->hptr[ppl->ulPtrIndex] != NULL)
            {
                // wait pointer data setzen
                piconinfo = &(ppl->iconinfo[ppl->ulPtrIndex]);

                fSuccess = WinSetSysPointerData(HWND_DESKTOP, ppl->ulPtrId, piconinfo);
                rc = (fSuccess) ? NO_ERROR : ERRORIDERROR(hab);

                // set new pointer
                WinSetPointer(HWND_DESKTOP, ppl->hptr[ppl->ulPtrIndex]);
            }

        }                       // else if (ppl->fReplaceActive)

        else
            // Pointer handle ungltig
            fSuccess = FALSE;

    }
    while (FALSE);              // end do

    return fSuccess;

}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : QueryAnimate                                               ³
 *³ Kommentar : fragt das Animation Flag fr einen oder alle Ptr aktiv ist ³
 *³             Wenn eine Animation aktiv ist, wird TRUE zurckgegeben     ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 24.07.1996                                                 ³
 *³ nderung  : 24.07.1996                                                 ³
 *³ aufgerufen: Window Procedure                                           ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : ULONG - pointer index                                      ³
 *³             BOOL  - query all ptrs                                     ³
 *³ Aufgaben  : - Animate abfragen                                         ³
 *³ Rckgabe  : BOOL - Flag, ob eine Animation aktiv ist                   ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

BOOL QueryAnimate
 (
     ULONG ulPointerIndex,
     BOOL fQueryAll
)

{
    BOOL fSuccess = FALSE;
    APIRET rc;
    ULONG i;
    BOOL fAnimate = FALSE;
    PPOINTERLIST ppl;

    ULONG ulFirstPtr, ulLastPtr;

    do
    {

        // range fr Verarbeitung festlegen
        ulFirstPtr = ulPointerIndex;

        if (fQueryAll)
            ulLastPtr = NUM_OF_SYSCURSORS;
        else
            ulLastPtr = ulPointerIndex + 1;

        ppl = QueryPointerlist(ulPointerIndex);

        for (i = ulFirstPtr; i < ulLastPtr; i++, ppl++)
        {
            // abfragen
            if (ppl->fAnimate)
                fAnimate = TRUE;
        }

    }
    while (FALSE);

    return fAnimate;
}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : QueryDemo                                                  ³
 *³ Kommentar : fragt das Demo Flag ab                                     ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 24.07.1996                                                 ³
 *³ nderung  : 24.07.1996                                                 ³
 *³ aufgerufen: Window Procedure                                           ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : -                                                          ³
 *³ Aufgaben  : - Flag zurckgeben                                         ³
 *³ Rckgabe  : BOOL - Flag, ob Demo aktiv ist                             ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

BOOL QueryDemo(VOID)
{
    BOOL fResult;

    DosEnterCritSec();
    fResult = fDemoActive;
    DosExitCritSec();
    return fResult;
}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : ToggleDemo                                                 ³
 *³ Kommentar : (de-)aktiviert Demo                                        ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 24.07.1996                                                 ³
 *³ nderung  : 24.07.1996                                                 ³
 *³ aufgerufen: Window Procedure                                           ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : BOOL        - Enable Flag                                  ³
 *³             PRECORDCORE - Zeiger auf Cnr-Daten                         ³
 *³             PBOOL       - Zeiger auf ForceFlag                         ³
 *³ Aufgaben  : - Demo (de-)aktivieren                                     ³
 *³ Rckgabe  : BOOL - Flag, ob erfolgreich                                ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

BOOL ToggleDemo
 (
     HWND hwnd,
     PRECORDCORE pcnrrec,
     BOOL fRefresh,
     PBOOL pfEnable
)
{
    BOOL fSuccess = FALSE;
    APIRET rc;
    ULONG i;
    ULONG ulTimerId;
    PPOINTERLIST ppl;

    do
    {

        // Demo aus-/anschalten
        // ggfs. Flag overrulen
        DosEnterCritSec();
        if (pfEnable)
            fDemoActive = *pfEnable;
        else
            fDemoActive = !fDemoActive;
        DosExitCritSec();

        // ggfs. kein Update
        if (hwnd == NULLHANDLE)
            break;

        // Timer fr Demo aktivieren/deaktivieren
        switch (fDemoActive)
        {
            case TRUE:
                ulTimerId = WinStartTimer(WinQueryAnchorBlock(hwnd),
                                          hwnd,
                                          DEMO_TIMER_ID,
                                          getDefaultTimeout());
                fSuccess = (ulTimerId != 0);
                break;

            case FALSE:
                // timer stoppen
                fSuccess = WinStopTimer(WinQueryAnchorBlock(hwnd), hwnd, DEMO_TIMER_ID);

                // Pointer zurcksetzen
                if (pcnrrec)
                    ProceedWithDemo(hwnd, pcnrrec, fRefresh, TRUE);
                break;

        }                       /* endswitch */

    }
    while (FALSE);

    return fSuccess;
}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : ProceedWithDemo                                            ³
 *³ Kommentar : fhrt einen Schritt in der Demo-Animation durch            ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 24.07.1996                                                 ³
 *³ nderung  : 24.07.1996                                                 ³
 *³ aufgerufen: Window Procedure                                           ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : HWND        - window handle                                ³
 *³             PRECORDCORE - Zeiger auf Cnr-Daten                         ³
 *³             BOOL        - Flag, ob nur zurckgesetzt werden soll       ³
 *³ Aufgaben  : - Demo einen Schritt weiter durchfhren                    ³
 *³ Rckgabe  : BOOL - Flag, ob erfolgreich                                ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

BOOL ProceedWithDemo
 (
     HWND hwnd,
     PRECORDCORE pcnrrec,
     BOOL fRefresh,
     BOOL fReset
)
{
    BOOL fSuccess = FALSE;
    APIRET rc;
    ULONG i;
    PPOINTERLIST ppl;
    ULONG ulViewStyle;
    BOOL fResetRefresh = FALSE;
    BOOL fPointerChanged = FALSE;

    do
    {

        // nicht im Default View ausfhren
        QueryContainerView(hwnd, &ulViewStyle);
        if (ulViewStyle & CV_DETAIL)
            break;

        // Pointer ausw„hlen
        ppl = QueryPointerlist(0);

        for (i = 0; i < NUM_OF_SYSCURSORS; i++, ppl++)
        {
            if (fReset)
            {
                ppl->ulPtrIndexCnr = 0;
                fResetRefresh = TRUE;
                fPointerChanged = TRUE;
            }
            else
            {
                if (ppl->fAnimate)
                {
                    // n„chsten Zeiger anw„hlen
                    fRefresh = TRUE;
                    ppl->ulPtrIndexCnr++;
                    if (ppl->ulPtrIndexCnr >= ppl->ulPtrCount)
                        ppl->ulPtrIndexCnr = 0;
                    fPointerChanged = TRUE;
                }
            }
        }

        // Refresh durchfhren
        if (fPointerChanged)
            if ((fResetRefresh) || (fRefresh))
                RefreshCnrItem(hwnd, NULL, pcnrrec, FALSE);

    }
    while (FALSE);

    return fSuccess;
}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : IsPointerActive                                            ³
 *³ Kommentar : Prft, ob der Pointer mit der angegebenen ID aktiv ist     ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 24.07.1996                                                 ³
 *³ nderung  : 24.07.1996                                                 ³
 *³ aufgerufen: Window Procedure                                           ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : ULONG - Ptr Id                                             ³
 *³             PHPTR - Zeiger auf Handle Variable des aktiven Zeigers     ³
 *³ Aufgaben  : - prfen, ob Zeiger aktiv ist                              ³
 *³ Rckgabe  : BOOL - Flag, ob aktiv                                      ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */
BOOL IsPointerActive
 (
     ULONG ulPtrId,
     HPOINTER * phptrCurrent
)
{
    HPOINTER hptrSys;
    HPOINTER hptrCurrent;
    BOOL fResult = FALSE;



// check pointer
    DosEnterCritSec();
    hptrSys = WinQuerySysPointer(HWND_DESKTOP, ulPtrId, FALSE);
    hptrCurrent = WinQueryPointer(HWND_DESKTOP);
    DosExitCritSec();


    if (phptrCurrent != NULL)
        *phptrCurrent = hptrCurrent;
    return (hptrSys == hptrCurrent);

}
