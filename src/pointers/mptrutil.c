
/*
 *@@sourcefile mptrutil.c:
 *
 *      This file is ALL new with V0.9.4.
 *
 *@@added V0.9.4 [umoeller]
 *@@header "pointers\mptrutil.h"
 */

/*
 *      Copyright (C) 1996-2000 Christian Langanke.
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

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS

#include "pointers\mptrcnr.h"
#include "pointers\macros.h"
#include "pointers\debug.h"

#include "pointers\mptrutil.h"
#include "pointers\mptrptr.h"
#include "pointers\title.h"

#include "pointers\r_amptreng.h"

/*������������������������������������������������������������������������Ŀ
 *� Name      : GetModuleHandle                                            �
 *� Kommentar : ermittelt ModuleHandle aus Funktionszeiger                 �
 *� Autor     : C.Langanke                                                 �
 *� Datum     : 11.10.1996                                                 �
 *� �nderung  : 11.10.1996                                                 �
 *� aufgerufen: Window Procedure                                           �
 *� ruft auf  : -                                                          �
 *� Eingabe   : PVOID    - Funktionsadresse                                �
 *�             PHMODULE - Zielvariable f�r moudle handle                  �
 *� Aufgaben  : - Modulehandle ermitteln                                   �
 *� R�ckgabe  : APIRET - OS/2 Fehlercode                                   �
 *��������������������������������������������������������������������������
 */

APIRET _Export GetModuleInfo(PFN pfn,
                             PHMODULE phmod,
                             PSZ pszBuffer,
                             ULONG ulBufLen)
{
    APIRET rc = NO_ERROR;
    ULONG ulObjectNumber = 0;
    ULONG ulOffset = 0;
    CHAR szDummy[_MAX_PATH];

    do
    {
        // check parms
        if ((pfn == NULL) || (phmod == NULL))
        {
            rc = ERROR_INVALID_PARAMETER;
            break;
        }

        // ggfs. eigenen Buffer f�r Namen setzen
        if (pszBuffer == NULL)
        {
            pszBuffer = szDummy;
            ulBufLen = sizeof(szDummy);
        }

        // get module handle from function adress
        *pszBuffer = 0;
        rc = DosQueryModFromEIP(phmod, &ulObjectNumber,
                                ulBufLen, pszBuffer,
                                &ulOffset, (PVOID)pfn);

        if (rc != NO_ERROR)
            break;

        // path specified ?
        if (strstr(pszBuffer, ":\\") == NULL)
            rc = DosQueryModuleName(*phmod,
                                    ulBufLen, pszBuffer);

    }
    while (FALSE);

    return rc;

}

/*������������������������������������������������������������������������Ŀ
 *� Name      : GetBaseName                                                �
 *� Kommentar : ermittelt Basisname                                        �
 *� Autor     : C.Langanke                                                 �
 *� Datum     : 12.06.1996                                                 �
 *� �nderung  : 12.06.1996                                                 �
 *� aufgerufen: Window Procedure                                           �
 *� ruft auf  : -                                                          �
 *� Eingabe   : HMOD   - frame window handle                               �
 *�             PSZ    - Speicherbereich                                   �
 *�             ULONG  - L�nge des Speicherbereichs                        �
 *� Aufgaben  : - Basisname ermitteln                                      �
 *� R�ckgabe  : APIRET - OS/2 Fehlercode                                   �
 *��������������������������������������������������������������������������
 */

APIRET _Export GetBaseName
 (
     PFN pfn,
     PSZ pszBuffer,
     ULONG ulBufLen
)
{
    APIRET rc = NO_ERROR;
    PSZ p;
    HMODULE hmod = NULLHANDLE;

    do
    {
        // check parms
        if ((pfn == NULL) || (pszBuffer == NULL) || (ulBufLen < 2))
        {
            rc = ERROR_INVALID_PARAMETER;
            break;
        }

        // get Module handle of IP adress
        rc = GetModuleInfo(pfn, &hmod, pszBuffer, ulBufLen);
        if (rc != NO_ERROR)
            break;

        // get executable name
        rc = DosQueryModuleName(hmod,
                                ulBufLen,
                                pszBuffer);
        if (rc != NO_ERROR)
            break;

        // remove last extension or complete name if there
        p = strrchr(pszBuffer, '\\');
        if (p)
            *p = 0;

    }
    while (FALSE);

    return rc;

}

/*������������������������������������������������������������������������Ŀ
 *� Name      : GetHelpLibName                                             �
 *� Kommentar : ermitteln Name der Hilfe-Bibliothek                        �
 *� Autor     : C.Langanke                                                 �
 *� Datum     : 12.06.1996                                                 �
 *� �nderung  : 12.06.1996                                                 �
 *� aufgerufen: Window Procedure                                           �
 *� ruft auf  : -                                                          �
 *� Eingabe   : PSZ   - Zeiger auf Speicherbereich                         �
 *�             ULONG - Gr��e des Speicehrbereiches                        �
 *� Aufgaben  : - Name ermitteln                                           �
 *� R�ckgabe  : APIRET - OS/2 Fehlercode                                   �
 *��������������������������������������������������������������������������
 */

APIRET _Export GetHelpLibName
 (
     PSZ pszBuffer,
     ULONG ulMaxLen
)
{
    APIRET rc = NO_ERROR;
    CHAR szHelpLibName[_MAX_PATH];

    do
    {
        // check parms
        if ((pszBuffer == NULL) || (ulMaxLen < 1))
        {
            rc = ERROR_INVALID_PARAMETER;
            break;
        }
        *pszBuffer = 0;

        // Basispfad holen
        rc = GetBaseName((PFN)GetHelpLibName,
                         szHelpLibName,
                         sizeof(szHelpLibName));
        if (rc != NO_ERROR)
            break;

        strcat(&szHelpLibName[strlen(szHelpLibName)], "\\wpamptr.hlp");

        // ist Buffer lang genug ?
        if (ulMaxLen < (strlen(szHelpLibName) + 1))
            rc = ERROR_MORE_DATA;
        else
            strcpy(pszBuffer, szHelpLibName);

    }
    while (FALSE);

    return rc;
}

/*������������������������������������������������������������������������Ŀ
 *� Name      : IsRemoveableMedia                                          �
 *� Kommentar : Pr�fen, ob Laufwerk removeable media enth�lt               �
 *� Autor     : C.Langanke                                                 �
 *� Datum     : 24.07.1996                                                 �
 *� �nderung  : 24.07.1996                                                 �
 *� aufgerufen: diverse                                                    �
 *� ruft auf  : -                                                          �
 *� Eingabe   : PSZ   - Device Name eines logischen Laufwerks (z.b. C:)    �
 *�             PBOOL - Zeiger auf Ergebnisvariable                        �
 *� Aufgaben  : - Flag aus BPB lesen                                       �
 *� R�ckgabe  : APIRET - OS/2 Fehlercode                                   �
 *��������������������������������������������������������������������������
 */

APIRET _Export IsRemoveableMedia
 (
     PSZ pszDeviceName,
     PBOOL pfIsRemoveableMedia
)
{
    APIRET rc = NO_ERROR;
    HFILE hfileDevice = NULLHANDLE;
    ULONG ulAction;

    ULONG ulParmLen;
    ULONG ulDataLen;

    UCHAR uchParms[2] =
    {0, -1};                    // 0: recommended BPB, -1: use file handle

    BOOL fIsFixedDisk;


    do
    {
        // Parameter pr�fen
        if ((pszDeviceName == NULL) ||
            (pfIsRemoveableMedia == NULL))
        {
            rc = ERROR_INVALID_PARAMETER;
            break;
        }

        // BIOS Parameter Block abfragen: ist es removeable media ?
        rc = DosOpen(pszDeviceName,
                     &hfileDevice,
                     &ulAction,
                     0, 0,
                     OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS,
                     OPEN_FLAGS_DASD | OPEN_FLAGS_FAIL_ON_ERROR | OPEN_SHARE_DENYNONE | OPEN_ACCESS_READONLY,
                     NULL);
        if (rc != NO_ERROR)
            break;

        // Bios Parameter Block lesen
        ulParmLen = 0;
        ulDataLen = 0;
        rc = DosDevIOCtl(hfileDevice,
                         IOCTL_DISK,    // category
                          DSK_BLOCKREMOVABLE,   // function
                          uchParms,
                         sizeof(uchParms),
                         &ulParmLen,
                         &fIsFixedDisk,
                         sizeof(fIsFixedDisk),
                         &ulDataLen);
        if (rc != NO_ERROR)
            break;

        // Ergebnis zur�ckgeben
        *pfIsRemoveableMedia = !fIsFixedDisk;

    }
    while (FALSE);

// aufr�umen
    if (hfileDevice)
        DosClose(hfileDevice);
    return rc;

}

/*������������������������������������������������������������������������Ŀ
 *� Name      : IsLocalDrive                                               �
 *� Kommentar : Pr�fen, ob Laufwerk ein lokales Laufwerk ist               �
 *� Autor     : C.Langanke                                                 �
 *� Datum     : 24.07.1996                                                 �
 *� �nderung  : 24.07.1996                                                 �
 *� aufgerufen: diverse                                                    �
 *� ruft auf  : -                                                          �
 *� Eingabe   : PSZ   - Device Name eines logischen Laufwerks (z.b. C:)    �
 *�             PBOOL - Zeiger auf Ergebnisvariable                        �
 *� Aufgaben  : - Flag aus BPB lesen                                       �
 *� R�ckgabe  : APIRET - OS/2 Fehlercode                                   �
 *��������������������������������������������������������������������������
 */

APIRET _Export IsLocalDrive
 (
     PSZ pszDeviceName,
     PBOOL pfIsLocalDrive
)
{

    APIRET rc = NO_ERROR;
    PFSQBUFFER2 pfsqb2 = NULL;
    ULONG ulInfoLen;

    do
    {
        // Parameter pr�fen
        if ((pszDeviceName == NULL) ||
            (pfIsLocalDrive == NULL))
        {
            rc = ERROR_INVALID_PARAMETER;
            break;
        }

        // FS Info holen
        ulInfoLen = sizeof(FSQBUFFER2) + _MAX_PATH;
        pfsqb2 = malloc(ulInfoLen);
        if (pfsqb2 == NULL)
            break;
        rc = DosQueryFSAttach(pszDeviceName, 0, FSAIL_QUERYNAME, pfsqb2, &ulInfoLen);
        if (rc != NO_ERROR)
            break;

        // nun Flag pr�fen
        *pfIsLocalDrive = (pfsqb2->iType == FSAT_LOCALDRV);

    }
    while (FALSE);

// aufr�umen
    if (pfsqb2)
        free(pfsqb2);
    return rc;

}

/*������������������������������������������������������������������������Ŀ
 *� Name      : OpenTmpFile                                                �
 *� Kommentar : tempor�re Datei in %TMP% oder in Root des Bootlaufwerkes   �
 *�             anlegen.                                                   �
 *� Autor     : C.Langanke                                                 �
 *� Datum     : 24.07.1996                                                 �
 *� �nderung  : 20.03.1998                                                 �
 *� aufgerufen: diverse                                                    �
 *� ruft auf  : -                                                          �
 *� Eingabe   : PSZ    - Basisname                                         �
 *�             PSZ    - Erweiterung ohne Punkt                            �
 *�             PHFILE - Handle auf offene Datei                           �
 *�             PSZ    - Zeiger auf Speicherbereich f�r Namen              �
 *�             ULONG  - Gr��e des Speicherbereichs                        �
 *� Aufgaben  : - Datei erzeugen und anlegen                               �
 *� R�ckgabe  : APIRET - OS/2 Fehlercode                                   �
 *��������������������������������������������������������������������������
 */

APIRET OpenTmpFile
 (
     PSZ pszBaseName,
     PSZ pszExtension,
     PHFILE phfile,
     PSZ pszFileName,
     ULONG ulMaxlen
)
{

    APIRET rc = NO_ERROR;
    PFSQBUFFER2 pfsqb2 = NULL;
    ULONG ulInfoLen;

    PSZ pszTmpDir;

    CHAR szRootDrive[] = "?:";
    ULONG ulBootDrive;

    CHAR szSearchMask[_MAX_PATH];
    HDIR hdirFile = HDIR_SYSTEM;
    FILEFINDBUF3 ffb3File;
    ULONG ulEntries = 1;

    ULONG ulFileCounter;
    ULONG ulMaxFileCounter = 0;
    ULONG ulAction;

    do
    {
        // Parameter pr�fen
        if ((pszBaseName == NULL) ||
            (*pszBaseName == 0) ||
            (pszExtension == NULL) ||
            (*pszExtension == 0) ||
            (phfile == NULL) ||
            (pszFileName == NULL) ||
            (ulMaxlen == 0))
        {
            rc = ERROR_INVALID_PARAMETER;
            break;
        }

        // Zielvariablen initialisieren
        *phfile = NULLHANDLE;
        *pszFileName = 0;

        // Verzeichnis ermitteln
        rc = DosScanEnv("TMP", &pszTmpDir);
        if (rc != NO_ERROR)
        {
            // Bootdrive ermitteln
            DosQuerySysInfo(QSV_BOOT_DRIVE, QSV_BOOT_DRIVE, &ulBootDrive, sizeof(ULONG));
            szRootDrive[0] = (CHAR) ulBootDrive + 'A' - 1;
            pszTmpDir = szRootDrive;
        }

        // Suchname zusammensetzen, dabei L�nge des Basisnamens merken
        sprintf(szSearchMask, "%s\\%s*.%s", pszTmpDir, pszBaseName, pszExtension);

        // Datei mit h�chstem Z�hler im Verzeichnis suchen
        ulEntries = 1;
        hdirFile = HDIR_SYSTEM;
        rc = DosFindFirst(szSearchMask,
                          &hdirFile,    // find handle
                           FILE_ARCHIVED |
                          FILE_READONLY,    // Pointerdateien suchen
                           &ffb3File,   // Speicherbereich
                           sizeof(FILEFINDBUF3),    // Gr��e des Bereichs
                           &ulEntries,  // Anzahl der Elemente
                           FIL_STANDARD);   // Infolevel

        while (rc == NO_ERROR)
        {
            // Z�hler ermitteln
            ulFileCounter = atol(&ffb3File.achName[strlen(pszBaseName)]);
            if (ulFileCounter > ulMaxFileCounter)
                ulMaxFileCounter = ulFileCounter;

            // n�chste Datei suchen
            rc = DosFindNext(hdirFile, &ffb3File, sizeof(FILEFINDBUF3), &ulEntries);

        }

        // Z�hler um 1 erh�hen
        ulMaxFileCounter++;

        // Dateinamen zusammensetzen
        sprintf(szSearchMask, "%s\\%s%u.%s", pszTmpDir, pszBaseName, ulMaxFileCounter, pszExtension);
        if (strlen(szSearchMask) + 1 > ulMaxlen)
        {
            rc = ERROR_MORE_DATA;
            break;
        }
        strcpy(pszFileName, szSearchMask);

        // Datei erstellen
        rc = DosOpen(szSearchMask,
                     phfile,
                     &ulAction,
                     0, 0,
                     OPEN_ACTION_CREATE_IF_NEW | OPEN_ACTION_FAIL_IF_EXISTS,
                     OPEN_SHARE_DENYREADWRITE | OPEN_ACCESS_WRITEONLY,
                     NULL);
        if (rc != NO_ERROR)
            break;


    }
    while (FALSE);

    return rc;

}

/*������������������������������������������������������������������������Ŀ
 *� Name      : LoadStringResource                                         �
 *� Comment   :                                                            �
 *� Author    : C.Langanke                                                 �
 *� Date      : 04.11.1995                                                 �
 *� Update    : 05.11.1995                                                 �
 *� called by : diverse                                                    �
 *� calls     : -                                                          �
 *� Input     : ULONG   - resource type: RT_MESSAGE or RT_STRING           �
 *�             HAB     - handle anchor block                              �
 *�             HLIB    - handle for resource DLL                          �
 *�             ULONG   - messsage resource id                             �
 *�             PSZ     - buffer                                           �
 *�             ULONG   - buffer length                                    �
 *� Tasks     : - loads message into buffer                                �
 *� returns   : PSZ - Pointer to buffer                                    �
 *��������������������������������������������������������������������������
 */

PSZ _Export LoadStringResource
 (
     ULONG ulResourceType,
     HAB hab,
     HLIB hlibResource,
     ULONG ulResId,
     PSZ pszBuffer,
     ULONG ulBufLen
)
{

    ULONG ulMsgLen;
    APIRET rc = NO_ERROR;
    BOOL fSuccess = TRUE;
    PSZ pszResourceType;
    static PSZ pszInternalError = "***";

// check parameters
    if (pszBuffer == NULL)
        return "";

    if (ulBufLen < 2)
        return pszBuffer;

// now load the string
    switch (ulResourceType)
    {
        case RT_STRING:
            pszResourceType = "string";
            fSuccess = WinLoadString(hab,
                                     hlibResource,
                                     ulResId,
                                     ulBufLen,
                                     pszBuffer);
            break;

        case RT_MESSAGE:
            pszResourceType = "message";
            fSuccess = WinLoadMessage(hab,
                                      hlibResource,
                                      ulResId,
                                      ulBufLen,
                                      pszBuffer);
            break;

        default:
            return "";
            // break;
    }

// errors ?
    if (!fSuccess)
    {
        rc = ERRORIDERROR(WinGetLastError(hab));
        DEBUGMSG("error loading %s #%u rc=0%08x" NEWLINE, pszResourceType _c_ ulResId _c_ rc);
        strcpy(pszBuffer, pszInternalError);
    }

    return pszBuffer;
}

/*������������������������������������������������������������������������Ŀ
 *� Name      : IsWARP3                                                    �
 *� Comment   :                                                            �
 *� Author    : C.Langanke                                                 �
 *� Date      : 01.11.1996                                                 �
 *� Update    : 01.11.1996                                                 �
 *� called by : diverse                                                    �
 *� calls     : -                                                          �
 *� Input     : -                                                          �
 *� Tasks     : - check wether OS is WARP 3                                �
 *� returns   : BOOL - OS is WARP 3 (2.30)                                 �
 *��������������������������������������������������������������������������
 */

#define VERSION_MAJOR 0
#define VERSION_MINOR 1

BOOL _Export IsWARP3(VOID)
{
    APIRET rc = NO_ERROR;
    ULONG aulOsVersion[2];
    BOOL fIsWARP3 = TRUE;       // Default: WARP 3 !

    do
    {
        // get os version
        rc = DosQuerySysInfo(QSV_VERSION_MAJOR,
                             QSV_VERSION_MINOR,
                             &aulOsVersion,
                             sizeof(aulOsVersion));

        if (rc != NO_ERROR)
            break;

        switch (aulOsVersion[VERSION_MAJOR])
        {

            case 20:
                // alles gr�sser 20.30 ist auf jeden Fall kein WARP 3
                if (aulOsVersion[VERSION_MINOR] > 30)
                    fIsWARP3 = FALSE;
                break;

            default:
                // alles gr�sser 20 ist auf jeden Fall kein WARP 3
                fIsWARP3 = FALSE;
                break;

        }                       // end switch

    }
    while (FALSE);

    return fIsWARP3;
}


/*������������������������������������������������������������������������Ŀ
 *� Name      : GetLoaderType                                              �
 *� Kommentar : ermittelt den Typ des Loaders                              �
 *� Autor     : C.Langanke                                                 �
 *� Datum     : 11.10.1996                                                 �
 *� �nderung  : 11.10.1996                                                 �
 *� aufgerufen: diverse                                                    �
 *� ruft auf  : Dos*                                                       �
 *� Eingabe   : -                                                          �
 *� Aufgaben  : - Loader Typ ermitteln                                     �
 *� R�ckgabe  : 0: DLL, 1: EXE, -1: unknown                                �
 *��������������������������������������������������������������������������
 */

ULONG GetLoaderType(VOID)
{

    APIRET rc = NO_ERROR;
    CHAR szModuleName[_MAX_PATH];
    HMODULE hmodule;
    ULONG ulResult = -1;
    PSZ pszExt;

    do
    {

        // Modulname holen und untersuchen
        rc = GetModuleInfo((PFN)&GetLoaderType, &hmodule, NULL, 0);
        if (rc != NO_ERROR)
            break;

        // get executable name
        rc = DosQueryModuleName(hmodule,
                                sizeof(szModuleName),
                                szModuleName);
        if (rc != NO_ERROR)
            break;

        pszExt = strrchr(szModuleName, '.');
        if (pszExt == NULL)
            break;

        if (strcmp(pszExt, ".DLL") == 0)
            ulResult = 0;
        if (strcmp(pszExt, ".EXE") == 0)
            ulResult = 1;

    }
    while (FALSE);

    return ulResult;

}

/*������������������������������������������������������������������������Ŀ
 *� Name      : SetPriority                                                �
 *� Kommentar : stellt die Priorit�tsklasse f�r den aktuellen Thread ein   �
 *� Autor     : C.Langanke                                                 �
 *� Datum     : 03.08.1996                                                 �
 *� �nderung  : 03.08.1996                                                 �
 *� aufgerufen: Window Procedure                                           �
 *� ruft auf  : -                                                          �
 *� Eingabe   : ULONG - Priorit�tsklasse                                   �
 *�                0 : PRTYC_NOCHANGE         - No change, leave as is     �
 *�                1 : PRTYC_IDLETIME         - Idle-time                  �
 *�                2 : PRTYC_REGULAR          - Regular                    �
 *�                3 : PRTYC_TIMECRITICAL     - Time-critical              �
 *�                4 : PRTYC_FOREGROUNDSERVER - Server                     �
 *� Aufgaben  : - Priorit�t einstellen                                     �
 *� R�ckgabe  : BOOL - Flag, ob erfolgreich                                �
 *��������������������������������������������������������������������������
 */

BOOL _Export SetPriority
 (
     ULONG ulPriorityClass
)

{
    APIRET rc = NO_ERROR;
    ULONG ulDelta;


    switch (ulPriorityClass)
    {
        case PRTYC_IDLETIME:
            ulDelta = PRTYD_MINIMUM;
            break;
        case PRTYC_REGULAR:
            ulDelta = 0;
            break;
        case PRTYC_TIMECRITICAL:
            ulDelta = PRTYD_MAXIMUM;
            break;
        case PRTYC_FOREGROUNDSERVER:
            ulDelta = PRTYD_MAXIMUM;
            break;
    }

// Priorit�t setzen
    rc = DosSetPriority(PRTYS_THREAD,
                        ulPriorityClass,
                        PRTYD_MAXIMUM,
                        0);

    return (rc == NO_ERROR);

}

/*������������������������������������������������������������������������Ŀ
 *� Name      : LoadResourceLib                                            �
 *� Kommentar : l�dt Resource DLL                                          �
 *� Autor     : C.Langanke                                                 �
 *� Datum     : 12.06.1996                                                 �
 *� �nderung  : 12.06.1996                                                 �
 *� aufgerufen: Window Procedure                                           �
 *� ruft auf  : -                                                          �
 *� Eingabe   : PHMODULE - Zeiger auf Ziel-Variable                        �
 *� Aufgaben  : - Library laden                                            �
 *� R�ckgabe  : APIRET - OS/2 Fehlercode                                   �
 *��������������������������������������������������������������������������
 */

APIRET _Export LoadResourceLib
 (
     PHMODULE phmodResource
)
{
    APIRET rc = NO_ERROR;
    HMODULE hmod = NULLHANDLE;
    CHAR szModuleName[_MAX_PATH];
    CHAR szLoadError[20];
    PSZ pszResourceTag;

    do
    {
        // check parms
        if (phmodResource == NULL)
        {
            rc = ERROR_INVALID_PARAMETER;
            break;
        }

        *phmodResource = NULLHANDLE;

        // Basispfad holen
        rc = GetBaseName((PFN)LoadResourceLib,
                         szModuleName,
                         sizeof(szModuleName));
        if (rc != NO_ERROR)
            break;

        strcat(&szModuleName[strlen(szModuleName)], "\\wpamptrs.dll");

        // Library laden
        rc = DosLoadModule(szLoadError, sizeof(szLoadError), szModuleName, phmodResource);
        if (rc != NO_ERROR)
            break;

        // Version pr�fen
        {
            HAB hab = WinQueryAnchorBlock(HWND_DESKTOP);
            HMODULE hmodResource = *phmodResource;
            CHAR szVersion[20];

            LOADSTRING(IDSTR_VERSION, szVersion);
            if (strcmp(szVersion, BLDLEVEL_VERSION) != 0)
                rc = ERROR_INVALID_DATA;
        }


    }
    while (FALSE);

    if (rc != NO_ERROR)
    {
        WinMessageBox(HWND_DESKTOP, HWND_DESKTOP,
        "The program cannot be initialized and will be aborted (LOAD_RESLIB)",
                      __TITLE__, 0, MB_CANCEL | MB_ERROR | MB_MOVEABLE);
        if (*phmodResource)
            DosFreeModule(*phmodResource);
    }

    return rc;

}

/*������������������������������������������������������������������������Ŀ
 *� Name      : ReloadClassLib                                             �
 *� Kommentar : l�dt Klassen-DLL nocheinmal, damit sie auch f�r statisches �
 *�             Linken im Speicher steht.                                  �
 *� Autor     : C.Langanke                                                 �
 *� Datum     : 12.06.1996                                                 �
 *� �nderung  : 12.06.1996                                                 �
 *� aufgerufen: Window Procedure                                           �
 *� ruft auf  : -                                                          �
 *� Eingabe   : PHMODULE - Zeiger auf Ziel-Variable                        �
 *� Aufgaben  : - Library laden                                            �
 *� R�ckgabe  : APIRET - OS/2 Fehlercode                                   �
 *��������������������������������������������������������������������������
 */

APIRET ReloadClassLib
 (
     PHMODULE phmodResource
)
{
    APIRET rc = NO_ERROR;
    HMODULE hmod = NULLHANDLE;
    CHAR szModulePath[_MAX_PATH];

    ULONG ulCurrentDisk;
    ULONG ulDiskmap;

    CHAR szCurrentDirectory[_MAX_PATH];
    ULONG ulNameLen;

    BOOL fDirectoryChanged = FALSE;
    BOOL fDiskChanged = FALSE;


    CHAR szLoadError[20];
    PSZ pszResourceTag;

    do
    {
        // check parms
        if (phmodResource == NULL)
        {
            rc = ERROR_INVALID_PARAMETER;
            break;
        }

        *phmodResource = NULLHANDLE;

        // Basispfad holen
        rc = GetBaseName((PFN)LoadResourceLib,
                         szModulePath,
                         sizeof(szModulePath));
        if (rc != NO_ERROR)
            break;

        // aktuelles Laufwerk sichern und wechseln
        DosQueryCurrentDisk(&ulCurrentDisk, &ulDiskmap);
        DosSetDefaultDisk(szModulePath[0] - 'A' + 1);
        fDiskChanged = TRUE;

        // aktuelles Verzeichnis sichern und wechseln
        ulNameLen = sizeof(szCurrentDirectory);
        rc = DosQueryCurrentDir(0, szCurrentDirectory, &ulNameLen);
        if (rc != NO_ERROR)
            break;
        rc = DosSetCurrentDir(szModulePath);
        if (rc != NO_ERROR)
            break;
        fDirectoryChanged = TRUE;

        // Library laden
        rc = DosLoadModule(szLoadError, sizeof(szLoadError), "WPAMPTR", phmodResource);
        if (rc != NO_ERROR)
            break;

    }
    while (FALSE);

// cleanup
    if (fDirectoryChanged)
        DosSetCurrentDir(szCurrentDirectory);
    if (fDiskChanged)
        DosSetDefaultDisk(ulCurrentDisk);
    return rc;
}

/*������������������������������������������������������������������������Ŀ
 *� Name      : FileExist                                                  �
 *� Comment   :                                                            �
 *� Author    : C.Langanke                                                 �
 *� Date      : 01.11.1996                                                 �
 *� Update    : 01.11.1996                                                 �
 *� called by : diverse                                                    �
 *� calls     : -                                                          �
 *� Input     : -                                                          �
 *� Tasks     : - checks wether file or directory exists                   �
 *� returns   : BOOL - OS is WARP 3 (2.30)                                 �
 *��������������������������������������������������������������������������
 */
BOOL _Export FileExist
 (
     PSZ pszName,
     BOOL fCheckDirectory
)
{
    APIRET rc = NO_ERROR;
    BOOL fResult = FALSE;
    CHAR szName[_MAX_PATH];
    ULONG ulBootDrive;
    FILESTATUS3 fs3;
    BOOL fIsDirectory = FALSE;


    do
    {
        // check parameters
        if ((pszName == NULL) ||
            (*pszName == 0))
            break;

        // Namen kopieren und ggfs. ?: ersetzen
        strcpy(szName, pszName);
        if (strncmp(szName, "?:", 2) == 0)
        {
            DosQuerySysInfo(QSV_BOOT_DRIVE, QSV_BOOT_DRIVE, &ulBootDrive, sizeof(ULONG));
            szName[0] = (CHAR) ulBootDrive + 'A' - 1;
        }

        // search entry
        rc = DosQueryPathInfo(szName,
                              FIL_STANDARD,
                              &fs3,
                              sizeof(fs3));
        if (rc != NO_ERROR)
            break;

        // auf Directory oder Datei pr�fen
        fIsDirectory = ((fs3.attrFile & FILE_DIRECTORY) > 0);
        fResult = (fIsDirectory == fCheckDirectory);


    }
    while (FALSE);

    return fResult;
}

/*������������������������������������������������������������������������Ŀ
 *� Name      : FileMaskExists                                             �
 *� Comment   :                                                            �
 *� Author    : C.Langanke                                                 �
 *� Date      : 11.03.1998                                                 �
 *� Update    : 11.03.1998                                                 �
 *� called by : diverse                                                    �
 *� calls     : -                                                          �
 *� Input     : -                                                          �
 *� Tasks     : - checks wether file or directory exists                   �
 *� returns   : BOOL - File exists                                         �
 *��������������������������������������������������������������������������
 */
BOOL _Export FileMaskExists
 (
     PSZ pszFilename,
     PSZ pszFirstFile,
     ULONG ulBuflen
)
{
    APIRET rc = NO_ERROR;
    HDIR hdir = HDIR_CREATE;

    CHAR szFirstName[_MAX_PATH];
    PSZ pszFilenamePart;

    do
    {
        // check parameters
        if ((pszFilename == NULL) ||
            (*pszFilename == 0) ||
            (strlen(pszFilename) > _MAX_PATH))
            break;


        rc = GetNextFile(pszFilename,
                         &hdir,
                         pszFirstFile,
                         ulBuflen);
        if (rc == NO_ERROR)
            DosFindClose(hdir);
        else
            break;


    }
    while (FALSE);

    return (rc == NO_ERROR);
}

/*������������������������������������������������������������������������Ŀ
 *� Name      : GetNextFile                                                �
 *� Comment   :                                                            �
 *� Author    : C.Langanke                                                 �
 *� Date      : 15.06.1998                                                 �
 *� Update    : 15.03.1998                                                 �
 *� called by : diverse                                                    �
 *� calls     : -                                                          �
 *� Input     : ###                                                        �
 *� Tasks     : - gets next file in directory                              �
 *� returns   : APIRE - OS/2 error code                                    �
 *��������������������������������������������������������������������������
 */
APIRET _Export GetNextFile
 (
     PSZ pszFileMask,
     PHDIR phdir,
     PSZ pszNextFile,
     ULONG ulBufLen
)
{
    APIRET rc = NO_ERROR;
    PSZ pszFilenamePart;
    FILEFINDBUF3 ffb3;
    ULONG ulFilecount = 1;
    CHAR szNextFile[_MAX_PATH];

    do
    {
        // check parms
        if ((pszFileMask == NULL) ||
            (phdir == NULL) ||
            (pszNextFile == NULL))
        {
            rc = ERROR_INVALID_PARAMETER;
            break;
        }


        // get first/next file
        if (*phdir == HDIR_CREATE)
            rc = DosFindFirst(pszFileMask,
                              phdir,
                    FILE_ARCHIVED | FILE_SYSTEM | FILE_HIDDEN | FILE_READONLY,
                              &ffb3,
                              sizeof(ffb3),
                              &ulFilecount,
                              FIL_STANDARD);
        else
            rc = DosFindNext(*phdir,
                             &ffb3,
                             sizeof(ffb3),
                             &ulFilecount);

        if (rc != NO_ERROR)
            break;

        // Namensteil isolieren
        strcpy(szNextFile, pszFileMask);
        pszFilenamePart = Filespec(szNextFile, FILESPEC_NAME);

        strcpy(pszFilenamePart, ffb3.achName);
        if ((strlen(szNextFile) + 1) > ulBufLen)
            rc = ERROR_MORE_DATA;
        else
            strcpy(pszNextFile, szNextFile);

    }
    while (FALSE);


// cleanup
    if (phdir)
        if (rc == ERROR_NO_MORE_FILES)
            DosFindClose(*phdir);

    return rc;
}

/*������������������������������������������������������������������������Ŀ
 *� Name      : IsFilenameValid                                            �
 *� Kommentar : pr�ft Dateinamen                                           �
 *� Autor     : C.Langanke                                                 �
 *� Datum     : 24.02.1998                                                 �
 *� �nderung  : 24.02.1998                                                 �
 *� aufgerufen: main                                                       �
 *� ruft auf  : -                                                          �
 *� Eingabe   : PSZ   - Dateiname                                          �
 *�             ULONG - Pr�fflags                                          �
 *� Aufgaben  : - Dateiname pr�fen                                         �
 *� R�ckgabe  : BOOL - G�ltigkeitsflag                                     �
 *��������������������������������������������������������������������������
 */
BOOL _Export IsFilenameValid
 (
     PSZ pszFilename,
     ULONG ulCheckType,
     PULONG pulValue
)

{
    BOOL fIsValid = FALSE;
    ULONG i;
    PSZ pszNumeration;
    PSZ pszFilenamePart;
    PSZ pszExtension;

    CHAR szNumerationValue[20];
    ULONG ulNumerationDigits;
    ULONG ulNumerationValue;

    ULONG ulPointerNameIndex;

    do
    {
        if ((pszFilename == NULL) ||
            (ulCheckType == 0))
            break;


        // Namensteil isolieren
        pszFilenamePart = Filespec(pszFilename, FILESPEC_NAME);

        // Erweiterung suchen
        pszExtension = Filespec(pszFilename, FILESPEC_EXTENSION);
        if (pszExtension == NULL)
            break;

        // Pr�fung durchf�hren
        switch (ulCheckType)
        {
            case FILENAME_CONTAINSNUMERATION:

                // check for numeration of at las one digit
                pszNumeration = pszExtension - 2;
                if (pszNumeration < pszFilenamePart)
                    break;

                if (!isdigit(*pszNumeration))
                    break;

                if (pulValue)
                {
                    // get numeration value
                    ulNumerationDigits = 0;
                    pszNumeration = pszExtension - 2;
                    while ((pszNumeration > pszFilename) && (isdigit(*pszNumeration)))
                    {
                        pszNumeration--;
                        ulNumerationDigits++;
                    }
                    pszNumeration++;

                    strncpy(szNumerationValue, pszNumeration, ulNumerationDigits);
                    *pulValue = atol(szNumerationValue);
                }

                fIsValid = TRUE;
                break;

            case FILENAME_CONTAINSWILDCARDS:
                if ((strchr(pszFilenamePart, '?') != 0) ||
                    (strchr(pszFilenamePart, '*') != 0))
                    fIsValid = TRUE;
                break;

            case FILENAME_CONTAINSPOINTERNAME:
                // check parms
                if (pulValue == NULL)
                    break;

                // Name suchen
                if (QueryPointerIndexFromName(pszFilenamePart, &ulPointerNameIndex))
                {
                    if (pulValue)
                        *pulValue = ulPointerNameIndex;
                    fIsValid = TRUE;
                }

                break;

            case FILENAME_CONTAINSFULLNAME:
                fIsValid = (strstr(pszFilename, ":\\") != NULL);
                break;
        }

    }
    while (FALSE);

    return fIsValid;
}


/*������������������������������������������������������������������������Ŀ
 *� Name      : ChangeFilename                                             �
 *� Comment   :                                                            �
 *� Author    : C.Langanke                                                 �
 *� Date      : 15.06.1998                                                 �
 *� Update    : 15.03.1998                                                 �
 *� called by : diverse                                                    �
 *� calls     : -                                                          �
 *� Input     : ###                                                        �
 *� Tasks     : - gets next file in directory                              �
 *� returns   : APIRE - OS/2 error code                                    �
 *��������������������������������������������������������������������������
 */
APIRET _Export ChangeFilename
 (
     PSZ pszFileName,
     ULONG ulChangeType,
     PSZ pszNewName,
     ULONG ulBufLen,
     PSZ pszNewNamePart,
     ULONG ulPointerIndex,
     ULONG ulNumeration
)
{
    APIRET rc = NO_ERROR;
    PSZ pszExtension;
    PSZ pszName;
    CHAR szNewName[_MAX_PATH];
    PSZ pszTmp;

    CHAR szNumeration[10];
    PSZ pszNumeration;
    ULONG ulNumerationDigits = 0;

    ULONG ulChangeTypeMasked;
    PSZ pszCheck;

    do
    {
        // check parms
        if ((pszFileName == NULL) ||
            (pszNewName == NULL) ||
            (ulBufLen == 0) ||
            (ulPointerIndex >= NUM_OF_SYSCURSORS))
        {
            rc = ERROR_INVALID_PARAMETER;
            break;
        }

        strcpy(szNewName, pszFileName);

        // search name & extension in coied field
        pszName = Filespec(szNewName, FILESPEC_NAME);
        pszExtension = Filespec(szNewName, FILESPEC_EXTENSION);

        // ---------------------------------------------------------------

        ulChangeTypeMasked = ulChangeType & CHANGE__MASK;
        if (ulChangeTypeMasked)
        {
            switch (ulChangeTypeMasked)
            {

//       case CHANGE_DRIVE_:
                    //          break;

//       case CHANGE_PATH_:
                    //          break;

                case CHANGE_DRIVEPATH:
                    pszTmp = strdup(pszName);
                    strcpy(szNewName, pszNewNamePart);
                    if (*(pszNewNamePart + (strlen(pszNewNamePart) - 1)) != '\\')
                        strcat(szNewName, "\\");
                    strcat(szNewName, pszTmp);
                    free(pszTmp);
                    break;

//       case CHANGE_NAME_:
                    //          break;

                case CHANGE_EXTENSION:
                    if (pszNewNamePart == NULL)
                    {
                        rc = ERROR_INVALID_PARAMETER;
                        break;
                    }
                    if (!pszExtension)
                    {
                        strcat(pszName, ".");
                        pszExtension = pszName + strlen(pszName);
                    }
                    strcpy(pszExtension, pszNewNamePart);
                    strlwr(pszExtension);
                    break;

                case CHANGE_USEPOINTERNAME:

                    if (pszExtension)
                        pszTmp = strdup(pszExtension - 1);

                    strcpy(pszName, QueryPointerName(ulPointerIndex));
                    strlwr(pszName);

                    if (pszExtension)
                    {
                        strcat(pszName, pszTmp);
                        free(pszTmp);
                    }
                    break;

                case CHANGE_USEDLLNAME:

                    if (pszExtension)
                    {
                        // cut of extension
                        pszTmp = strdup(pszExtension - 1);
                        *(pszExtension - 1) = 0;
                    }

                    // cut of at 9th byte
                    if (strlen(pszName) > 8)
                        *(pszName + 8) = 0;

                    // change blanks to underscore
                    pszCheck = pszName;
                    while (*pszCheck != 0)
                    {
                        if (*pszCheck == 0x20)
                            *pszCheck = '_';
                        pszCheck++;
                    }

                    // reappend extension
                    if (pszExtension)
                    {
                        strcat(pszName, pszTmp);
                        free(pszTmp);
                    }
                    break;

                default:
                    rc = ERROR_INVALID_PARAMETER;
                    break;

            }                   // switch (ulChangeTypeMasked)

            if (rc != NO_ERROR)
                break;

            // recalculate name and extension ptrs
            pszName = Filespec(szNewName, FILESPEC_NAME);
            pszExtension = Filespec(szNewName, FILESPEC_EXTENSION);
        }

        // ---------------------------------------------------------------

        ulChangeTypeMasked = ulChangeType & CHANGE_DELETE__MASK;
        if (ulChangeTypeMasked)
        {
            switch (ulChangeTypeMasked)
            {

//       case CHANGE_DELETEDRIVE_:
                    //          break;

//       case CHANGE_DELETEPATH_:
                    //          break;

//       case CHANGE_DELETEDRIVEPATH_:
                    //          break;


                case CHANGE_DELETEFILENAME:
                    pszName = Filespec(szNewName, FILESPEC_NAME);
                    if (pszName == szNewName)
                        strcpy(szNewName, ".");
                    else if (*(pszName - 1) == '\\')
                        *(pszName - 1) = 0;
                    else if (*(pszName - 1) == ':')
                        strcpy(pszName, "\\");
                    break;

                default:
                    rc = ERROR_INVALID_PARAMETER;
                    break;

//       case CHANGE_DELETEEXTENSION_:
                    //          break;

            }                   // switch (ulChangeTypeMasked)

            if (rc != NO_ERROR)
                break;

            // recalculate name and extension ptrs
            pszName = Filespec(szNewName, FILESPEC_NAME);
            pszExtension = Filespec(szNewName, FILESPEC_EXTENSION);
        }


        // ---------------------------------------------------------------

        ulChangeTypeMasked = ulChangeType & CHANGE_NUMERATION__MASK;
        if (ulChangeTypeMasked)
        {

            // Numerationen nur bei vorhandener Erweiterung !
            if (!pszExtension)
            {
                rc = ERROR_INVALID_DATA;
                break;
            }

            // count numeration digits
            ulNumerationDigits = 0;
            pszNumeration = pszExtension - 2;
            while ((pszNumeration > pszName) && (isdigit(*pszNumeration)))
            {
                pszNumeration--;
                ulNumerationDigits++;
            }

            switch (ulChangeTypeMasked)
            {

                case CHANGE_ADDNUMERATION:
                    if (strncmp(pszExtension - 4, "000", 3) != 0)
                    {
                        pszTmp = strdup(pszExtension - 1);
                        strcpy(pszExtension - 1, "000");
                        strcat(pszExtension - 1, pszTmp);
                        pszExtension += 3;
                        free(pszTmp);
                    }
                    break;

                case CHANGE_DELNUMERATION:
                    strcpy(pszExtension - ulNumerationDigits - 1, pszExtension - 1);
                    pszExtension -= ulNumerationDigits;
                    break;

                case CHANGE_SETNUMERATION:
                    sprintf(szNumeration, "%0*u", ulNumerationDigits, ulNumeration);
                    strncpy(pszExtension - ulNumerationDigits - 1, szNumeration, strlen(szNumeration));
                    break;

                default:
                    rc = ERROR_INVALID_PARAMETER;
                    break;

            }                   // switch (ulChangeTypeMasked)

            if (rc != NO_ERROR)
                break;

            // recalculate name and extension ptrs
            pszName = Filespec(szNewName, FILESPEC_NAME);
            pszExtension = Filespec(szNewName, FILESPEC_EXTENSION);
        }

        // ---------------------------------------------------------------

        // transfer result
        if (strlen(szNewName) + 1 > ulBufLen)
        {
            rc = ERROR_MORE_DATA;
            break;
        }
        else
            strcpy(pszNewName, szNewName);

    }
    while (FALSE);

    return rc;
}


/*������������������������������������������������������������������������Ŀ
 *� Name      : CreateTmpFile                                              �
 *� Comment   :                                                            �
 *� Author    : C.Langanke                                                 �
 *� Date      : 15.03.1998                                                 �
 *� Update    : 15.03.1998                                                 �
 *� called by : diverse                                                    �
 *� calls     : -                                                          �
 *� Input     : PSZ     - source filename                                  �
 *�             PHFILE                                                     �
 *�             PSZ                                                        �
 *�             ULONG                                                      �
 *� Tasks     : - creates a temporary file                                 �
 *� returns   : APIRET - OS/2 error code                                   �
 *��������������������������������������������������������������������������
 */
APIRET _Export CreateTmpFile
 (
     PSZ pszMainFile,
     PHFILE phfile,
     PSZ pszBuffer,
     ULONG ulBufferlen
)
{
    APIRET rc = NO_ERROR;
    ULONG ulAdjust = 0;

    ULONG ulTimer;
    CHAR szTimer[20];
    PSZ pszLastSlash;

    ULONG ulAction;
    ULONG ulMaxTries = 5;

    do
    {
        // check parm
        if ((pszMainFile == NULL) ||
            (phfile == NULL) ||
            (pszBuffer == NULL) ||
            (ulBufferlen == 0))
            break;

        // get the full name of the file
        rc = DosQueryPathInfo(pszMainFile,
                              FIL_QUERYFULLNAME,
                              pszBuffer,
                              ulBufferlen);
        if (rc != NO_ERROR)
            break;


        while (ulMaxTries)
        {
            // now patch in a number to make file unique
            DosQuerySysInfo(QSV_MS_COUNT, QSV_MS_COUNT, &ulTimer, sizeof(ulTimer));
            pszLastSlash = strrchr(pszBuffer, '\\');
            if (pszLastSlash - pszBuffer + 10 > ulBufferlen)
            {
                rc = ERROR_BUFFER_OVERFLOW;
                break;
            }
            sprintf(szTimer, "%u", ulTimer);
            strncpy(pszLastSlash + 1, szTimer, 8);

            // now create the file
            rc = DosOpen(pszBuffer,
                         phfile,
                         &ulAction,
                         0,
                         0,
                       OPEN_ACTION_CREATE_IF_NEW | OPEN_ACTION_FAIL_IF_EXISTS,
                         OPEN_SHARE_DENYREADWRITE | OPEN_ACCESS_READWRITE,
                         NULL);

            // break if successful
            if (rc == NO_ERROR)
                break;

            // break on other errors
            if (rc != ERROR_OPEN_FAILED)
                break;

            // next try
            ulMaxTries++;
        }

    }
    while (FALSE);

    return rc;

}

/*������������������������������������������������������������������������Ŀ
 *� Name      : Filespec                                                   �
 *� Comment   :                                                            �
 *� Author    : C.Langanke                                                 �
 *� Date      : 15.03.1998                                                 �
 *� Update    : 15.03.1998                                                 �
 *� called by : diverse                                                    �
 *� calls     : -                                                          �
 *� Input     : ###                                                        �
 *� Tasks     : - gets a pointer to a filenamme part                       �
 *� returns   : PSZ  - Pointer to the part or NULL                         �
 *��������������������������������������������������������������������������
 */
PSZ _Export Filespec
 (
     PSZ pszFilename,
     ULONG ulPart
)
{
    PSZ pszFilenamePart = NULL;
    ULONG ulAdjust = 0;

    do
    {
        // check parm
        if (pszFilename == NULL)
            break;

        switch (ulPart)
        {
            case FILESPEC_EXTENSION:
                pszFilenamePart = strrchr(pszFilename, '.');
                ulAdjust = 1;
                break;

            case FILESPEC_NAME:
                if (pszFilenamePart == NULL)
                {
                    pszFilenamePart = strrchr(pszFilename, '\\');
                    ulAdjust = 1;
                }

                // fall thru !!!

            case FILESPEC_PATHNAME:
                if (pszFilenamePart == NULL)
                {
                    pszFilenamePart = strrchr(pszFilename, ':');
                    ulAdjust = 1;
                    if (pszFilenamePart == NULL)
                    {
                        pszFilenamePart = pszFilename;
                        ulAdjust = 0;
                    }
                }
                break;

            default:
                break;
        }

        // adjust if necessary
        if (pszFilenamePart != NULL)
            pszFilenamePart += ulAdjust;

    }
    while (FALSE);

    return pszFilenamePart;

}

/*������������������������������������������������������������������������Ŀ
 *� Name      : CopyFromFileToFile                                         �
 *� Comment   :                                                            �
 *� Author    : C.Langanke                                                 �
 *� Date      : 13.04.1998                                                 �
 *� Update    : 13.04.1998                                                 �
 *� called by : app                                                        �
 *� calls     : Dos*                                                       �
 *� Input     : ###                                                        �
 *� Tasks     : - copy amount of bytes from one file to other              �
 *� returns   : APIRET - OS/2 error code                                   �
 *��������������������������������������������������������������������������
 */

APIRET CopyFromFileToFile
 (
     HFILE hfileTarget,
     HFILE hfileSource,
     ULONG ulDataLen
)
{
    APIRET rc = NO_ERROR;
    ULONG ulBufferSize = 16 * 1024;
    PVOID pvData = malloc(ulBufferSize);

    ULONG ulBytesToRead;
    ULONG ulBytesRead;
    ULONG ulBytesWritten;

    do
    {
        if (pvData == NULL)
        {
            rc = ERROR_NOT_ENOUGH_MEMORY;
            break;
        }

        while (ulDataLen > 0)
        {
            // determine amount of data to read
            if (ulDataLen == -1)
                ulBytesToRead = ulBufferSize;
            else
                ulBytesToRead = MIN(ulDataLen, ulBufferSize);

            // read data
            PREADMEMORY(hfileSource, pvData, ulBytesToRead);

            // write data
            WRITEMEMORY(hfileTarget, pvData, ulBytesRead);

            // adjust datalen
            if (ulDataLen == -1)
            {
                // no more bytes to read ?
                if (ulBytesRead < ulBufferSize)
                    ulDataLen = 0;
            }
            else
                ulDataLen -= ulBytesRead;

        }

    }
    while (FALSE);

    return rc;
}

/*������������������������������������������������������������������������Ŀ
 *� Name      : IsFileModified                                             �
 *� Comment   : on any error, TRUE is returned                             �
 *� Author    : C.Langanke                                                 �
 *� Date      : 13.04.1998                                                 �
 *� Update    : 13.04.1998                                                 �
 *� called by : app                                                        �
 *� calls     : Dos*                                                       �
 *� Input     : PSZ          - name of file                                �
 *�             PFILESTATUS3 - file status of previous check               �
 *� Tasks     : - check wether file is modified                            �
 *� returns   : APIRET - OS/2 error code                                   �
 *��������������������������������������������������������������������������
 */

BOOL IsFileModified
 (
     PSZ pszFilename,
     PFILESTATUS3 pfs3
)
{
    BOOL fIsModified = TRUE;
    APIRET rc = NO_ERROR;
    ULONG ulBytesWritten;
    FILESTATUS3 fs3Current;

    do
    {
        // check parms
        if ((pszFilename == NULL) ||
            (pfs3 == NULL))
            break;

        // aktuellen Status holen
        rc = DosQueryPathInfo(pszFilename,
                              FIL_STANDARD,
                              &fs3Current,
                              sizeof(fs3Current));
        if (rc != NO_ERROR)
            break;

        // check date & time fields
        if (memcmp(pfs3, &fs3Current, sizeof(FILESTATUS3)) == 0)
        {
            // file has not changed
            fIsModified = FALSE;
        }
        else
        {
            // copy file status
            memcpy(pfs3, &fs3Current, sizeof(FILESTATUS3));
        }

    }
    while (FALSE);

    return fIsModified;
}

/*������������������������������������������������������������������������Ŀ
 *� Name      : ConvertFile                                                �
 *� Comment   : target file is written to source directory                 �
 *� Author    : C.Langanke                                                 �
 *� Date      : 13.04.1998                                                 �
 *� Update    : 13.04.1998                                                 �
 *� called by : app                                                        �
 *� calls     : Dos*                                                       �
 *� Input     : PSZ   - name of file to convert                            �
 *�             ULONG - target filetype                                    �
 *� Tasks     : - convert file                                             �
 *� returns   : APIRET - OS/2 error code                                   �
 *��������������������������������������������������������������������������
 */

APIRET ConvertFile
 (
     PSZ pszFilename,
     ULONG ulTargetFiletype
)
{
    APIRET rc = NO_ERROR;
    PSZ pszTargetExtension = QueryResFileExtension(ulTargetFiletype);

    CHAR szModuleName[_MAX_PATH];
    CHAR szParms[_MAX_PATH];

    STARTDATA startdata;
    CHAR szError[20];
    ULONG ulIdSession;
    PID pid;

    FUNCENTER();

    do
    {
        // check parms
        if ((pszFilename == NULL) ||
            (pszTargetExtension == NULL))
            rc = ERROR_INVALID_PARAMETER;

        // Konverter im Basisverzeichnis suchen
        rc = GetBaseName((PFN)ConvertFile, szModuleName, sizeof(szModuleName));
        if (rc != NO_ERROR)
            break;
        strcat(szModuleName, "\\PTRCONV\\PTRCONV.EXE");

        if (!FileExist(szModuleName, FALSE))
        {
            rc = DosSearchPath(SEARCH_CUR_DIRECTORY |
                               SEARCH_IGNORENETERRS |
                               SEARCH_ENVIRONMENT,
                               "PATH",
                               "PTRCONV.EXE",
                               szModuleName,
                               sizeof(szModuleName));
            if (rc != NO_ERROR)
                break;
        }

        // Parameter zusammensetzen
        sprintf(szParms, "%s %s", pszFilename, pszTargetExtension + 1);

        // - start the session
        memset(&startdata, 0, sizeof(startdata));
        startdata.Length = sizeof(startdata);
        startdata.PgmName = szModuleName;
        startdata.PgmInputs = szParms;
        startdata.InheritOpt = SSF_INHERTOPT_PARENT;
        startdata.SessionType = SSF_TYPE_PM;
        startdata.ObjectBuffer = szError;
        startdata.ObjectBuffLen = sizeof(szError);
        rc = DosStartSession(&startdata,
                             &ulIdSession,
                             &pid);
        DEBUGMSG("info: start converter: %s %s, rc=%u" NEWLINE,
                 startdata.PgmName _c_ startdata.PgmInputs _c_ rc);
    }
    while (FALSE);

    return rc;
}

/*������������������������������������������������������������������������Ŀ
 *� Name      : SubclassStaticWindowProc                                   �
 *� Comment   : subclass of static window to erase background before       �
 *�           : executing WM_PAINT                                         �
 *� Author    : C.Langanke                                                 �
 *� Date      : 13.04.1998                                                 �
 *� Update    : 13.04.1998                                                 �
 *� called by : app                                                        �
 *� calls     : Dos*                                                       �
 *� Input     : HWND,ULONG,MPARAM,MPARAM                                   �
 *� Tasks     : - process WM_PAINT                                         �
 *� returns   : MRESULT - message processing result                        �
 *��������������������������������������������������������������������������
 */

MRESULT _Export EXPENTRY SubclassStaticWindowProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{

    CLASSINFO classinfo;

// query classinfo for procedure
    WinQueryClassInfo(WinQueryAnchorBlock(hwnd), WC_STATIC, &classinfo);

    switch (msg)

    {
        case WM_PAINT:
            {
                HPS hps = WinBeginPaint(hwnd, NULLHANDLE, NULL);
                RECTL rcl;
                HPOINTER hptr = (HPOINTER)WinSendMsg(hwnd, SM_QUERYHANDLE, 0, 0);

                // reset whole window to background color
                if (!WinQueryWindowRect(hwnd, &rcl))
                    break;
                WinFillRect(hps, &rcl, SYSCLR_DIALOGBACKGROUND);

                // draw pointer
                if (hptr)
                    WinDrawPointer(hps, 0, 0, hptr, DP_NORMAL);

                WinEndPaint(hps);

            }
            break;

    }

    return classinfo.pfnWindowProc(hwnd, msg, mp1, mp2);

}
