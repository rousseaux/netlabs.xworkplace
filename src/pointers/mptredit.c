
/*
 *@@sourcefile mptredit.c:
 *
 *      This file is ALL new with V0.9.4.
 *
 *@@added V0.9.4 [umoeller]
 *@@header "pointers\mptredit.h"
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

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS

#include "pointers\mptredit.h"
#include "pointers\mptrcnr.h"
#include "pointers\mptrutil.h"
#include "pointers\pointer.h"
#include "pointers\wmuser.h"
#include "pointers\macros.h"
#include "pointers\debug.h"

#include "..\..\001\dll\r_amptr001.h"

// Daten fr Editierfunktion
static BOOL fEditPending = FALSE;
static CHAR szEditFilename[_MAX_PATH];
static HAPP happEdit;

// Prototypen
MRESULT EXPENTRY EditDialogProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : QueryEditPending                                           ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */
BOOL QueryEditPending(VOID)
{
    return fEditPending;
}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : BeginEditPointer                                           ³
 *³ Kommentar : Pointer in tempor„re Datei schreiben und in den Icon Editor³
 *³             laden. Frame wird ber WM_APPTERMINATENOTIFY notifiziert,  ³
 *³             wenn der Editvorgang beendet ist.                          ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 24.07.1996                                                 ³
 *³ Žnderung  : 24.07.1996                                                 ³
 *³ aufgerufen: diverse                                                    ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : ###                                                        ³
 *³ Aufgaben  : - Datei erzeugen und anlegen                               ³
 *³ Rckgabe  : BOOL - Flag, ob erfolgreich                                ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

BOOL BeginEditPointer
 (
     HWND hwnd,
     HMODULE hmodResource,
     PPOINTERLIST ppl
)
{

    BOOL fSuccess = FALSE;
    APIRET rc = NO_ERROR;
    HFILE hfileTmp = NULLHANDLE;
    ULONG ulBytesWritten;

    ULONG ulPtrId;
    PROGDETAILS progdetails;
    CHAR szPrgFullName[_MAX_PATH];

    PICONINFO piconinfo;
    ICONINFO iconinfo;

    FILESTATUS3 fs3;

    do
    {
        // Pointerliste ok ?
        if (!ppl)
            break;

        // merhmaliges Edit verhindern (besser: mit sempahore !)
        if (fEditPending)
            break;
        else
            fEditPending = TRUE;

        // check for single pointer or sytem default
        if (ppl->ulPtrCount < 2)
        {

            if (ppl->ulPtrCount == 1)
                piconinfo = &ppl->iconinfo[0];
            else
            {
                // System ID des Pointers ermitteln
                ulPtrId = ppl->ulPtrId;

                // Pointerdaten-L„nge und dann Pointerdaten holen
                piconinfo = &iconinfo;
                memset(&iconinfo, 0, sizeof(ICONINFO));
                iconinfo.cb = sizeof(ICONINFO);
                iconinfo.fFormat = ICON_DATA;
                if (!WinQuerySysPointerData(HWND_DESKTOP, ulPtrId, piconinfo))
                {
                    rc = ERRORIDERROR(WinGetLastError(WinQueryAnchorBlock(hwnd)));
                    break;
                }
                if ((piconinfo->pIconData = malloc(piconinfo->cbIconData)) == NULL)
                    break;
                else if (!WinQuerySysPointerData(HWND_DESKTOP, ulPtrId, piconinfo))
                    break;
            }

            // tempor„re Datei erstellen
            rc = OpenTmpFile("WP!!", "PTR", &hfileTmp, szEditFilename, sizeof(szEditFilename));
            if (rc != NO_ERROR)
                break;

            // Daten in Datei schreiben
            rc = DosWrite(hfileTmp,
                          piconinfo->pIconData,
                          piconinfo->cbIconData,
                          &ulBytesWritten);
            if (rc != NO_ERROR)
                break;

            // Datei schlieáen
            rc = DosClose(hfileTmp);

            // Archiv Attribut dediziert l”schen, da DosOpen dies nicht tut
            rc = DosQueryPathInfo(szEditFilename, FIL_STANDARD, &fs3, sizeof(fs3));
            if (rc != NO_ERROR)
                break;
            fs3.attrFile = 0;
            rc = DosSetPathInfo(szEditFilename, FIL_STANDARD, &fs3, sizeof(fs3), DSPI_WRTTHRU);
            if (rc != NO_ERROR)
                break;

            // icon editor suchen
            rc = DosSearchPath(SEARCH_CUR_DIRECTORY |
                               SEARCH_ENVIRONMENT |
                               SEARCH_IGNORENETERRS,
                               "PATH",
                               "ICONEDIT.EXE",
                               (PBYTE) szPrgFullName,
                               sizeof(szPrgFullName));
            if (rc != NO_ERROR)
                break;

            // jetzt icon editor starten
            memset(&progdetails, 0, sizeof(PROGDETAILS));
            progdetails.Length = sizeof(PROGDETAILS);
            progdetails.progt.progc = PROG_PM;
            progdetails.progt.progc = SHE_VISIBLE;
            progdetails.pszExecutable = szPrgFullName;
            progdetails.pszParameters = szEditFilename;

            happEdit = WinStartApp(hwnd, &progdetails, szEditFilename, NULL, 0);
            if (happEdit == NULLHANDLE)
                break;

        }                       // if (ppl->ulPtrCount == 1)

        // -------------------------------------------------------------------------------------
        else                    // if (ppl->ulPtrCount > 1)

        {

        }

        // -------------------------------------------------------------------------------------

        // alles ok
        fSuccess = TRUE;

    }
    while (FALSE);


// aufr„umen
    if (hfileTmp)
    {
        DosClose(hfileTmp);
        if (!fSuccess)
            DosDelete(szEditFilename);
    }

    if (!fSuccess)
        fEditPending = FALSE;

    return fSuccess;

}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : EndEditPointer                                             ³
 *³ Kommentar : Zuvor editierten pointer als                               ³
 *³             laden. Frame wird ber WM_APPTERMINATENOTIFY notifiziert,  ³
 *³             wenn der Editvorgang beendet ist.                          ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 24.07.1996                                                 ³
 *³ Žnderung  : 24.07.1996                                                 ³
 *³ aufgerufen: diverse                                                    ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : ###                                                        ³
 *³ Aufgaben  : - Datei erzeugen und anlegen                               ³
 *³ Rckgabe  : BOOL - Flag, ob erfolgreich                                ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

BOOL EndEditPointer
 (
     HWND hwnd,
     HAPP happ,
     ULONG ulReturncode,
     PPOINTERLIST ppl
)
{

    BOOL fSuccess = FALSE;
    APIRET rc;
    ICONINFO iconinfo;
    ULONG ulPtrId;
    FILESTATUS3 fs3;
    BOOL fFileChanged = FALSE;
    HPOINTER hptrTmp = NULLHANDLE;

    do
    {
        // wird gerade editiert ?
        if (!fEditPending)
            break;

        // ist es unser handle
        if (happ != happEdit)
            break;

        // Pointerliste ok ?
        if (!ppl)
            break;

        // ab jetzt keinen Fehler mehr geben !
        fSuccess = TRUE;

        // bei Fehler abbrechen
        if (ulReturncode != NO_ERROR)
            break;

        // -------------------------------------------------------------------------------------

        // check for single pointer or sytem default
        if (ppl->ulPtrCount < 2)
        {

            // Dateiinfo holen
            rc = DosQueryPathInfo(szEditFilename, FIL_STANDARD, &fs3, sizeof(fs3));
            if (rc != NO_ERROR)
                break;

            // prfen, ob die Datei sich ver„ndert hat
            fFileChanged = ((fs3.attrFile & FILE_ARCHIVED) == FILE_ARCHIVED);
            if (!fFileChanged)
                break;

            // System ID des Pointers ermitteln
            ulPtrId = ppl->ulPtrId;

            // Pointer laden
            if (!LoadPointerFromPointerFile(szEditFilename, &hptrTmp, &iconinfo, NULL))
                break;

            // Pointer setzen
            CopyPointerlist(NULL, ppl, FALSE);
            ppl->hptrStatic = hptrTmp;
            ppl->ulPtrCount = 0;
            memcpy(&ppl->iconinfoStatic, &iconinfo, sizeof(ICONINFO));

            WinSetSysPointerData(HWND_DESKTOP, ulPtrId, &iconinfo);
            if (ulPtrId == SPTR_ARROW)
                WinSetPointer(HWND_DESKTOP, hptrTmp);

        }                       // if (ppl->ulPtrCount == 1)

        // -------------------------------------------------------------------------------------
        else                    // if (ppl->ulPtrCount > 1)

        {
            // no change needed
        }

        // -------------------------------------------------------------------------------------

    }
    while (FALSE);

// cleanup
    fEditPending = FALSE;
    DosDelete(szEditFilename);

    return fSuccess;

}
