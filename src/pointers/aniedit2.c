
/*
 *@@sourcefile aniedit2.c:
 *
 *      This file is ALL new with V0.9.4.
 *
 *@@added V0.9.4 [umoeller]
 *@@header "pointers\aniedit2.h"
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

/* C Runtime */
#define EXTERN
#include <io.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdarg.h>


/* OS/2 Toolkit */
#define INCL_ERRORS
#define INCL_DOS
#define INCL_WIN
#define INCL_PM
#include <os2.h>

// generic headers
#include "setup.h"              // code generation and debugging options

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS

#include "pointers\aniedit2.h"
#include "pointers\debug.h"
#include "pointers\macros.h"

#include "pointers\mptrppl.h"
#include "pointers\mptranim.h"
#include "pointers\mptrfile.h"  // V0.9.3 (2000-05-21) [umoeller]
#include "pointers\mptrutil.h"
#include "pointers\mptrptr.h"
#include "..\..\001\dll\r_amptr001.h"

#include "pointers\mptrcnr.h"   // V0.9.3 (2000-05-21) [umoeller]

#define NEWLINE          "\n"
#define SWITCH_CHARS     "/-"
#define SWITCH_DELIMITER ':'
#define MAX_SWITCH_ENTRIES  32

#define MAX_RES_STRLEN     32
#define JIF2MS(j) (j * 1000 / 60)
#define MS2JIF(m) (m * 60 / 1000)

#define UNIT_MS  0
#define UNIT_JIF 1

#define ANIMATION_TIMER_ID   256

// structs
typedef struct _FRAMEINFO
{
    ULONG ulIndex;
    ICONINFO iconinfo;
    ULONG ulTimeoutMS;
    ULONG ulTimeoutJIF;
}
FRAMEINFO, *PFRAMEINFO;

typedef struct _ANIEDITRECORD
        // renamed from MYRECORDCORE V0.9.3 (2000-05-21) [umoeller]
        // because this conflicts with the definition in mptrcnr.h
{
    MINIRECORDCORE rec;
    FRAMEINFO fi;
}
ANIEDITRECORD, *PANIEDITRECORD;


typedef struct _PARM
{
    HWND hwnd;
    HMODULE hmodResource;
    CHAR szAnimationName[_MAX_PATH];
    CHAR szAnimationFile[_MAX_PATH];
    CHAR szPaletteName[_MAX_PATH];
    CHAR szInfoName[_MAX_PATH];
    CHAR szInfoArtist[_MAX_PATH];
    ULONG ulFileType;
    ULONG ulPtrCount;
    POINTERLIST appl[NUM_OF_SYSCURSORS];
    ULONG ulIndex;
    BOOL fLoaded;
    BOOL fModified;
    BOOL fAnimate;
    BOOL fLoadPending;
    PMINIRECORDCORE precCurrent;
}
PARM, *PPARM;


// wenn diese Variable nicht da ist, tut EditAnimation nicht (????????)
static POINTERLIST appl2[NUM_OF_SYSCURSORS];

//      globale Variablen

static BOOL fDebug = FALSE;
static BOOL fHelp = FALSE;
static PSZ pszFilename = NULL;

static BOOL fIndexGiven = FALSE;


static PSZ pstTitleSpacer = " - ";
static CHAR szAppTitle[MAX_RES_STRLEN];
static CHAR szTitleImage[MAX_RES_STRLEN];
static CHAR szTitleFrameRate[MAX_RES_STRLEN];
static CHAR szSelectedNone[MAX_RES_STRLEN];
static CHAR szSelectedFrame[MAX_RES_STRLEN];
static CHAR szSelectedFrames[MAX_RES_STRLEN];

static CHAR szUnitMilliseconds[MAX_RES_STRLEN];
static CHAR szUnitJiffies[MAX_RES_STRLEN];

static PSZ apszUnit[] =
{szUnitMilliseconds, szUnitJiffies};
static ULONG ulCurrentUnit = 0;
static CHAR szTitleFrameRateWithUnits[MAX_RES_STRLEN * 2];

// protptypes
APIRET GetCommandlineParms(INT argc, PSZ argv[], PPSZ ppszInvalidParm);
APIRET ShowMessage(APIRET rc, PSZ pszMessage);
BOOL EditAnimation(HWND hwnd, HMODULE hmodResource, PSZ pszAnimationName);
MRESULT EXPENTRY EditDialogProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);
MRESULT SubclassStaticWindowProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);

VOID InitResources(HMODULE hmodResource);
BOOL SetContainerDetails(HWND hwnd, ULONG ulCnrId);
BOOL PointerlistToContainer(HWND hwnd, ULONG ulCnrId, PPOINTERLIST ppl, PPARM pparm);

BOOL ContainerToPointerlist(HWND hwnd, ULONG ulCnrId, PPOINTERLIST ppl, PPARM pparm);
BOOL DeleteContainerRecords(HWND hwnd, ULONG ulCnrId, PMINIRECORDCORE prec, ULONG ulRecordFlags, PULONG pulDeleted);
BOOL CountContainerRecords(HWND hwnd, ULONG ulCnrId, ULONG ulRecordFlags, PULONG pulCount);
BOOL ListContainerRecords(HWND hwnd, ULONG ulCnrId);

BOOL SetSpinbuttonValues(HWND hwnd, ULONG ulSbId,
                       ULONG ulMinValue, ULONG ulMaxValue, ULONG ulValueStep);

// *********** graphic pushbutton helper function **********

#define DLGSETIMAGE(hwnd,id,imageid) (SetPushButton(hwnd,id,imageid))

VOID                            // changed from BOOL V0.9.3 (2000-05-21) [umoeller], you're not returning anything
 SetPushButton
(
    HWND hwnd,
    ULONG ulPbId,
    ULONG ulImageId
)
{

    CHAR szImageId[20];

    sprintf(szImageId, "#%u", ulImageId);
    DLGSETSTRING(hwnd, ulPbId, szImageId);
    WinSendDlgItemMsg(hwnd, ulPbId, BM_SETHILITE, FALSE, 0);
}


/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : main                                                       ³
 *³ Kommentar :                                                            ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 24.02.1998                                                 ³
 *³ Žnderung  : 24.02.1998                                                 ³
 *³ aufgerufen: C-Runtime                                                  ³
 *³ ruft auf  : diverse                                                    ³
 *³ Eingabe   : ULONG - Anzahl der Parameter                               ³
 *³             PSZ[] - Parameter                                          ³
 *³ Aufgaben  : - Programm ausfhren                                       ³
 *³ Rckgabe  : INT - OS/2 Fehlercode                                      ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */
INT main
 (
     ULONG argc,
     PSZ argv[]
)
{


    APIRET rc = NO_ERROR;
    PSZ pszInvalidParm = NULL;

    HAB hab = NULLHANDLE;
    HMQ hmq = NULLHANDLE;
    POINTERLIST pl;
    HMODULE hmodResource;

    do
    {

        hab = WinInitialize(0);
        if (hab == NULLHANDLE)
        {
            DosBeep(700, 100);
            rc = ERROR_INVALID_FUNCTION;
            break;
        }
        hmq = WinCreateMsgQueue(hab, 0);
        if (hmq == NULLHANDLE)
        {
            DosBeep(700, 100);
            rc = ERRORIDERROR(WinGetLastError(hab));
        }

        // load resource lib
        rc = LoadResourceLib(&hmodResource);
        if (rc != NO_ERROR)
            break;

        // read some resource strings
        InitStringResources(hab, hmodResource);

        // Kommandozeilenparameter holen
        rc = GetCommandlineParms(argc, argv, &pszInvalidParm);

        // ungltiger Parameter ?
        if (pszInvalidParm != NULL)
        {
            // @@todo
            rc = ShowMessage(ERROR_INVALID_PARAMETER, "Invalid Parameter.");
            break;
        }


        // show dialog
        if (EditAnimation(HWND_DESKTOP, hmodResource, pszFilename))
        {
            // save modified animation
            // ...
        }
        else
            DosBeep(700, 400);

    }
    while (FALSE);

// cleanup
    if (hmq)
        WinDestroyMsgQueue(hmq);
    if (hab)
        WinTerminate(hab);
    return rc;

}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : GetCommandlineParms                                        ³
 *³ Comment   :                                                            ³
 *³ Author    : C.Langanke                                                 ³
 *³ Date      : 24.02.1998                                                 ³
 *³ Update    : 24.02.1998                                                 ³
 *³ called by : main                                                       ³
 *³ calls     : -                                                          ³
 *³ Input     : INT, PSZ[]  - command line parms                           ³
 *³ Tasks     : - processes command line parms and sets up flags           ³
 *³ returns   : APIRET - OS/2 error code                                   ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */
APIRET GetCommandlineParms
 (
     INT argc,
     PSZ argv[],
     PPSZ ppszInvalidParm
)
{
    COUNTRYCODE countrycode =
    {0, 0};                     // use defaults

    APIRET rc = NO_ERROR;
    ULONG i, j;

    PSZ pszThisParm;
    PSZ pszThisValue;

    static PSZ pszParmHelp, pszParmDebug;

// Parameter in Grossbuchstaben
    pszParmHelp = strupr(PARM_HELP);
    pszParmDebug = strupr(PARM_DEBUG);

// get commandline parms
    for (i = 1; i < argc; i++)
    {
        pszThisParm = argv[i];

        if (strchr(SWITCH_CHARS, *pszThisParm) != NULL)
        {
            pszThisParm++;

            // search for delimiter and separate name and value
            pszThisValue = strchr(pszThisParm, SWITCH_DELIMITER);
            if (pszThisValue == NULL)
                pszThisValue = "";
            else
            {
                *pszThisValue = 0;
                (*pszThisValue)++;
            }

            // upcase the parm name; no use of c-runtime so we can use subsystem lib !
            rc = DosMapCase(strlen(pszThisParm), &countrycode, pszThisParm);

            // process /?
            if (strstr(pszParmHelp, pszThisParm) == pszParmHelp)
            {
                fHelp = TRUE;
            }
            // process /DEBUG
            else if (strstr(pszParmDebug, pszThisParm) == pszParmDebug)
            {
                fDebug = TRUE;
            }
            else
            {
                pszThisParm--;
                rc = ERROR_INVALID_PARAMETER;
                break;
            }
        }
        else
        {
            if (!pszFilename)
                pszFilename = pszThisParm;
            else
            {
                rc = ERROR_INVALID_PARAMETER;
                break;
            }
        }

    }                           // for all argv[]

// report invalid parameter
    if (rc == ERROR_INVALID_PARAMETER)
        *ppszInvalidParm = pszThisParm;

    return rc;

}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : ShowMessage                                                ³
 *³ Comment   :                                                            ³
 *³ Author    : C.Langanke                                                 ³
 *³ Date      : 24.02.1998                                                 ³
 *³ Update    : 24.02.1998                                                 ³
 *³ called by : main                                                       ³
 *³ calls     : -                                                          ³
 *³ Input     : ###                                                        ³
 *³ Tasks     : - displays a popup                                         ³
 *³ returns   : -                                                          ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */
APIRET ShowMessage
 (
     APIRET rc,
     PSZ pszMessage
)
{

    do
    {
        // check parm
        if (!pszMessage)
            break;

        // display message
        WinMessageBox(HWND_DESKTOP, HWND_DESKTOP,
                      pszMessage, szAppTitle, 0,
        MB_MOVEABLE | ((rc) ? MB_CANCEL | MB_ERROR : MB_OK | MB_INFORMATION));
    }
    while (FALSE);

    return rc;

}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : EditAnimation                                              ³
 *³ Kommentar :                                                            ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 24.07.1996                                                 ³
 *³ Žnderung  : 24.07.1996                                                 ³
 *³ aufgerufen: diverse                                                    ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : ###                                                        ³
 *³ Aufgaben  : - Dialog zum Editieren aufrufen                            ³
 *³ Rckgabe  : BOOL - Flag, ob erfolgreich                                ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

// ------------------------------------------------------------------------

BOOL EditAnimation
 (
     HWND hwnd,
     HMODULE hmodResource,
     PSZ pszAnimationName
)
{

    BOOL fSuccess = FALSE;
    PARM parm;
    APIRET rc;
    TID tidEditDialog;

    do
    {

        // ab jetzt keinen Fehler mehr geben !
        fSuccess = TRUE;

        // setup parameters
        memset(&parm, 0, sizeof(PARM));
        parm.hwnd = hwnd;
        parm.hmodResource = hmodResource;
        if (pszAnimationName)
            strcpy(parm.szAnimationName, pszAnimationName);

        // Dialog ausfhren
        rc = WinDlgBox(HWND_DESKTOP, HWND_DESKTOP,
               &EditDialogProc, hmodResource, IDDLG_DLG_EDITANIMATION, &parm);

        if (rc == MBID_ERROR)
        {
            rc = ERRORIDERROR(WinGetLastError(WinQueryAnchorBlock(HWND_DESKTOP)));
            fSuccess = FALSE;
        }

        // post termination message
        if (hwnd)
            WinPostMsg(hwnd, WM_APPTERMINATENOTIFY, MPFROMLONG(-1), MPFROMLONG(rc));


    }
    while (FALSE);

    return fSuccess;
}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : LoadStrings                                                ³
 *³ Kommentar : l„dt Strings aus Resource Lib                              ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 12.06.1996                                                 ³
 *³ Žnderung  : 12.06.1996                                                 ³
 *³ aufgerufen: ###                                                        ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : HMODULE - Handle der Resource lib                          ³
 *³ Aufgaben  : - Strings laden                                            ³
 *³ Rckgabe  : -                                                          ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

VOID InitResources
 (
     HMODULE hmodResource
)
{

    HAB hab = WinQueryAnchorBlock(HWND_DESKTOP);

// Strings laden
    LOADSTRING(IDDLG_DLG_EDITANIMATION, szAppTitle);
    LOADSTRING(IDSTR_TITLE_IMAGE, szTitleImage);
    LOADSTRING(IDSTR_TITLE_FRAMERATE, szTitleFrameRate);
    LOADSTRING(IDSTR_SELECTED_NONE, szSelectedNone);
    LOADSTRING(IDSTR_SELECTED_FRAME, szSelectedFrame);
    LOADSTRING(IDSTR_SELECTED_FRAMES, szSelectedFrames);
    LOADSTRING(IDSTR_UNIT_MILLISECONDS, szUnitMilliseconds);
    LOADSTRING(IDSTR_UNIT_JIFFIES, szUnitJiffies);
    return;
}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : InitContainer                                              ³
 *³ Kommentar : initialisiert container                                    ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 12.06.1996                                                 ³
 *³ Žnderung  : 12.06.1996                                                 ³
 *³ aufgerufen: Window Procedure                                           ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : HWND   - frame window handle                               ³
 *³             ULONG  - ID des container controls                         ³
 *³ Aufgaben  : - Basisname ermitteln                                      ³
 *³ Rckgabe  : BOOL - Flag, ob erfolgreich                                ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

#define CNR_COLUMNS_COUNT 2
#define ANIRECORD_EXTRAMEMORY (sizeof( ANIEDITRECORD) - sizeof( MINIRECORDCORE))
            // renamed from RECORD_EXTRAMEMORY, this conflicts with mptrcnr.h

BOOL SetContainerDetails
 (
     HWND hwnd,
     ULONG ulCnrId
)
{
    BOOL fSuccess = FALSE;
    HWND hwndCnr = WinWindowFromID(hwnd, ulCnrId);
    APIRET rc;
    ULONG i;
    HAB hab = WinQueryAnchorBlock(hwndCnr);
    PMINIRECORDCORE prec = NULL;
    BOOL fInitError = FALSE;

    PFIELDINFO pfi, pfiFirst;
    FIELDINFOINSERT fii;
    CNRINFO cnri;

    do
    {
        // Parameter prfen
        if (hwndCnr == NULLHANDLE)
            break;

        // details infos entsorgen
        WinSendMsg(hwndCnr, CM_REMOVEDETAILFIELDINFO, 0, MPFROM2SHORT(0, CMA_FREE));

        // Detailsview konfigurieren
        // Speicher holen
        if ((pfi = WinSendMsg(hwndCnr,
                              CM_ALLOCDETAILFIELDINFO,
                              MPFROMLONG(CNR_COLUMNS_COUNT),
                              0L)) == NULL)
            break;

        pfiFirst = pfi;         // Zeiger auf erste Struktur festhalten
        // Details fr Symbol

        pfi->flData = CFA_BITMAPORICON | CFA_HORZSEPARATOR | CFA_CENTER | CFA_SEPARATOR;
        pfi->flTitle = CFA_CENTER | CFA_FITITLEREADONLY;
        pfi->pTitleData = szTitleImage;
        pfi->offStruct = FIELDOFFSET(ANIEDITRECORD, rec.hptrIcon);
        pfi->cxWidth = 0;

        // timeout
        strcpy(szTitleFrameRateWithUnits, apszUnit[ulCurrentUnit]);
        pfi = pfi->pNextFieldInfo;
        pfi->flData = CFA_ULONG | CFA_CENTER | CFA_HORZSEPARATOR;
        pfi->flTitle = CFA_CENTER | CFA_FITITLEREADONLY;
        pfi->pTitleData = szTitleFrameRateWithUnits;
        if (ulCurrentUnit == 1)
            pfi->offStruct = FIELDOFFSET(ANIEDITRECORD, fi.ulTimeoutJIF);
        else
            pfi->offStruct = FIELDOFFSET(ANIEDITRECORD, fi.ulTimeoutMS);
        pfi->cxWidth = 0;

        // Struktur an Container senden
        memset(&fii, 0, sizeof(FIELDINFOINSERT));
        fii.cb = sizeof(FIELDINFOINSERT);
        fii.pFieldInfoOrder = (PFIELDINFO) CMA_END;
        fii.cFieldInfoInsert = (SHORT) CNR_COLUMNS_COUNT;
        fii.fInvalidateFieldInfo = TRUE;

        if (WinSendMsg(hwndCnr,
                       CM_INSERTDETAILFIELDINFO,
                       MPFROMP(pfiFirst),
                       MPFROMP(&fii)) != (MRESULT) CNR_COLUMNS_COUNT)
            break;


        // Info holen und auf details view umschalten
        if ((ULONG) WinSendMsg(hwndCnr,
                               CM_QUERYCNRINFO,
                               MPFROMP(&cnri),
                               MPFROMLONG(sizeof(CNRINFO))) == sizeof(CNRINFO))
        {
            cnri.flWindowAttr = CA_DRAWICON | CA_DETAILSVIEWTITLES | CV_DETAIL;

//    cnri.slBitmapOrIcon.cx = WinQuerySysValue( HWND_DESKTOP, SV_CXICON) / 2;
            //    cnri.slBitmapOrIcon.cy = WinQuerySysValue( HWND_DESKTOP, SV_CYICON) / 2;

            if (!WinSendMsg(hwndCnr,
                            CM_SETCNRINFO,
                            MPFROMP(&cnri),
                            MPFROMLONG(CMA_FLWINDOWATTR | CMA_SLBITMAPORICON)))
                break;
        }

        // fertig
        fSuccess = TRUE;

    }
    while (FALSE);

// bei Fehler struktur ggfs. wieder freigeben
    if (!fSuccess)
        if (prec)
            WinSendMsg(hwndCnr,
                       CM_ALLOCRECORD,
                       MPFROMP(prec),
                       MPFROMLONG(NUM_OF_SYSCURSORS));

    return fSuccess;

}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : PointerlistToContainer                                     ³
 *³ Kommentar : baut Inhalt neu auf                                        ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 12.06.1996                                                 ³
 *³ Žnderung  : 12.06.1996                                                 ³
 *³ aufgerufen: Window Procedure                                           ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : HWND   - frame window handle                               ³
 *³             ULONG  - ID des container controls                         ³
 *³             PPOINTERLIST - Zeiger auf Aniamtionsdaten                  ³
 *³ Aufgaben  : - Inhalt des Containers neu setzen                         ³
 *³ Rckgabe  : BOOL - Flag, ob erfolgreich                                ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

BOOL PointerlistToContainer
 (
     HWND hwnd,
     ULONG ulCnrId,
     PPOINTERLIST ppl,
     PPARM pparm
)
{
    BOOL fSuccess = FALSE;
    HWND hwndCnr = WinWindowFromID(hwnd, ulCnrId);
    APIRET rc;
    ULONG i;
    PMINIRECORDCORE prec = NULL;
    PANIEDITRECORD pmyrec;
    RECORDINSERT recinsert;

    do
    {

        // check parms
        if (!ppl)
            break;

        // prevent flickering
        WinEnableWindowUpdate(hwndCnr, FALSE);

        // Container l”schen
        prec = (PMINIRECORDCORE) WinSendMsg(hwndCnr,
                                            CM_REMOVERECORD,
                                            MPFROMP(NULL),
                                            MPFROM2SHORT(0, CMA_FREE));

        DEBUGMSG("* PointerlistToContainer" NEWLINE, 0);

        // nichts zum anzeigen ?
        if (!ppl->ulPtrCount)
        {
            fSuccess = TRUE;
            break;
        }

        // Speicher holen
        prec = (PMINIRECORDCORE) WinSendMsg(hwndCnr,
                                            CM_ALLOCRECORD,
                                            MPFROMLONG(ANIRECORD_EXTRAMEMORY),
                                            MPFROMLONG(ppl->ulPtrCount));


        if (prec)
        {
            for (i = 0, pmyrec = (PANIEDITRECORD) prec;
                 i < ppl->ulPtrCount;
                 i++, pmyrec = (PANIEDITRECORD) (pmyrec->rec.preccNextRecord))
            {

                DEBUGMSG("* prec init -> 0x%08x" NEWLINE, pmyrec);
                // ggfs. default timeout
                if (!ppl->aulTimer[i])
                    ppl->aulTimer[i] = DEFAULT_ANIMATION_TIMEOUT;

                // iconinfo kopieren
                memcpy(&pmyrec->fi.iconinfo, &ppl->iconinfo[i], sizeof(ICONINFO));
                pmyrec->fi.iconinfo.pIconData = malloc(pmyrec->fi.iconinfo.cbIconData);
                if (!pmyrec->fi.iconinfo.pIconData)
                    WinAlarm(HWND_DESKTOP, WA_ERROR);
                else
                {
                    memcpy(pmyrec->fi.iconinfo.pIconData, ppl->iconinfo[i].pIconData, pmyrec->fi.iconinfo.cbIconData);
                    pmyrec->rec.hptrIcon = CreatePtrFromIconInfo(&pmyrec->fi.iconinfo);
                }

                // array fllen
                pmyrec->rec.flRecordAttr = CRA_RECORDREADONLY;
                pmyrec->rec.pszIcon = NULL;
                pmyrec->fi.ulTimeoutMS = ppl->aulTimer[i];
                pmyrec->fi.ulTimeoutJIF = MS2JIF(ppl->aulTimer[i]);
                pmyrec->fi.ulIndex = i;
            }

            // Record-Array  in Container einstellen
            recinsert.cb = sizeof(RECORDINSERT);
            recinsert.pRecordOrder = (PRECORDCORE) CMA_END;
            recinsert.pRecordParent = NULL;
            recinsert.fInvalidateRecord = TRUE;
            recinsert.zOrder = CMA_TOP;
            recinsert.cRecordsInsert = ppl->ulPtrCount;
            WinSendMsg(hwndCnr, CM_INSERTRECORD, MPFROMP(prec), MPFROMP(&recinsert));
        }

        // Selektion fr erstes Element wieder l”schen
        WinSendMsg(hwndCnr,
                   CM_SETRECORDEMPHASIS,
                   MPFROMP(prec),
                   MPFROM2SHORT(FALSE,
                                CRA_SELECTED));

        // zus„tzliche Daten bertragen
        strcpy(pparm->szAnimationName, ppl->szAnimationName);
        strcpy(pparm->szAnimationFile, ppl->szAnimationFile);
        pparm->ulPtrCount = ppl->ulPtrCount;
        if (ppl->pszInfoName)
            strcpy(pparm->szInfoName, ppl->pszInfoName);
        if (ppl->pszInfoArtist)
            strcpy(pparm->szInfoName, ppl->pszInfoArtist);
        pparm->fAnimate = (pparm->ulPtrCount > 1);

        // fSuccess = TRUE;
    }
    while (FALSE);

// refresh now
    WinSendDlgItemMsg(hwnd, IDDLG_CN_FRAMESET, CM_INVALIDATERECORD, 0, 0);
    WinEnableWindowUpdate(hwndCnr, TRUE);
    return fSuccess;

}


/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : ContainerToPointerlist                                     ³
 *³ Kommentar : bertr„gt Containerinhalt in Pointerliste                  ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 12.06.1996                                                 ³
 *³ Žnderung  : 12.06.1996                                                 ³
 *³ aufgerufen: Window Procedure                                           ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : ###                                                        ³
 *³ Aufgaben  : - Inhalt des Containers bertragen                         ³
 *³ Rckgabe  : BOOL - Flag, ob erfolgreich                                ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

BOOL ContainerToPointerlist
 (
     HWND hwnd,
     ULONG ulCnrId,
     PPOINTERLIST ppl,
     PPARM pparm
)
{
    BOOL fSuccess = FALSE;
    HWND hwndCnr = WinWindowFromID(hwnd, ulCnrId);
    APIRET rc;
    ULONG i;

    PMINIRECORDCORE prec = NULL;
    PANIEDITRECORD pmyrec;
    PICONINFO piconinfo;

    do
    {

        // check parms
        if (!ppl)
            break;

        // clean list
        CopyPointerlist(NULL, ppl, FALSE);

        // adress container items
        prec = (PMINIRECORDCORE) WinSendDlgItemMsg(hwnd, IDDLG_CN_FRAMESET, CM_QUERYRECORD, 0, MPFROM2SHORT(CMA_FIRST, CMA_ITEMORDER));
        if (prec == (PMINIRECORDCORE) - 1)
            prec = NULL;

        i = 0;
        while (prec != NULL)
        {
            pmyrec = (PANIEDITRECORD) prec;

            ppl->ulPtrCount += 1;

            // copy iconinfo
            piconinfo = &ppl->iconinfo[i];
            memcpy(piconinfo, &pmyrec->fi.iconinfo, sizeof(ICONINFO));
            piconinfo->pIconData = malloc(piconinfo->cbIconData);
            if (piconinfo->pIconData)
            {
                memcpy(piconinfo->pIconData, pmyrec->fi.iconinfo.pIconData, piconinfo->cbIconData);
                ppl->hptr[i] = CreatePtrFromIconInfo(piconinfo);
            }

            //  copy timeout value
            ppl->aulTimer[i] = pmyrec->fi.ulTimeoutMS;

            // adress next record
            prec = (PMINIRECORDCORE) WinSendDlgItemMsg(hwnd, IDDLG_CN_FRAMESET, CM_QUERYRECORD, MPFROMP(prec), MPFROM2SHORT(CMA_NEXT, CMA_ITEMORDER));
            if (prec == (PMINIRECORDCORE) - 1)
                prec = NULL;

            // incr index for pointerlist
            i += 1;
        }

        // copy info
        ppl->pszInfoName = strdup(pparm->szInfoName);
        ppl->pszInfoArtist = strdup(pparm->szInfoArtist);

        // ok
        fSuccess = TRUE;

    }
    while (FALSE);

    return fSuccess;
}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : DeleteContainerRecords                                     ³
 *³ Eingabe   : HWND   - window handle                                     ³
 *³             ULONG  - container ID                                      ³
 *³             PMINIRECORDCORE - ptr to single record                     ³
 *³             ULONG  - record flags (like CRA_SELECTED), only used if    ³
 *³                      prec == NULL                                      ³
 *³             PULONG - count of deleted records                          ³
 *³ Aufgaben  : - Records l”schen                                          ³
 *³ Rckgabe  : BOOL    - Ergebnisflag                                     ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

BOOL DeleteContainerRecords
 (
     HWND hwnd,
     ULONG ulCnrId,
     PMINIRECORDCORE prec,
     ULONG ulRecordFlags,
     PULONG pulDeleted
)
{
    BOOL fSuccess = FALSE;
    PANIEDITRECORD pmyrec;
    ULONG ulDeleted = 0;

    do
    {
        // prevent flickering
        WinEnableWindowUpdate(WinWindowFromID(hwnd, ulCnrId), FALSE);

        if (prec)
        {
            // delete record
            fSuccess = (LONG) WinSendDlgItemMsg(hwnd,
                                                ulCnrId,
                                                CM_REMOVERECORD,
                                                MPFROMP(&prec),
                                                MPFROM2SHORT(1,
                                                  CMA_INVALIDATE | CMA_FREE));
            if (fSuccess)
                ulDeleted++;
            break;
        }

        // any record matching to flags
        prec = (PMINIRECORDCORE) WinSendDlgItemMsg(hwnd, ulCnrId, CM_QUERYRECORDEMPHASIS, MPFROMLONG(CMA_FIRST), MPFROMLONG(ulRecordFlags));
        if (prec == (PMINIRECORDCORE)-1)
            prec = NULL;

        DEBUGMSG(" delete: first record with %u is 0x%08x" NEWLINE, ulRecordFlags _c_ prec);

        while (prec != NULL)
        {
            pmyrec = (PANIEDITRECORD) prec;

            // free resources
            if (pmyrec->fi.iconinfo.pIconData)
                free(pmyrec->fi.iconinfo.pIconData);
            WinDestroyPointer(pmyrec->rec.hptrIcon);
            pmyrec->rec.hptrIcon = 0;

            DEBUGMSG("* delete prec -> 0x%08x" NEWLINE, prec);

            // delete record
            WinSendDlgItemMsg(hwnd, ulCnrId, CM_REMOVERECORD, MPFROMP(&prec), MPFROM2SHORT(1, CMA_INVALIDATE | CMA_FREE));
            ulDeleted++;

            // adress next record
            // adress first record, because deleted one is already invalid
            prec = (PMINIRECORDCORE) WinSendDlgItemMsg(hwnd, ulCnrId, CM_QUERYRECORDEMPHASIS, MPFROMLONG(CMA_FIRST), MPFROMLONG(ulRecordFlags));
            if (prec == (PMINIRECORDCORE)-1)
                prec = NULL;
        }

        // ok
        fSuccess = TRUE;

    }
    while (FALSE);

    if (pulDeleted)
        *pulDeleted = ulDeleted;
    WinEnableWindowUpdate(WinWindowFromID(hwnd, ulCnrId), TRUE);
    return fSuccess;

}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : CountContainerRecords                                      ³
 *³ Eingabe   : HWND   - window handle                                     ³
 *³             ULONG  - container ID                                      ³
 *³             ULONG  - record flags (like CRA_SELECTED)                  ³
 *³             PULONG - count of records                                  ³
 *³ Aufgaben  : - Records l”schen                                          ³
 *³ Rckgabe  : BOOL    - Ergebnisflag                                     ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

BOOL CountContainerRecords
 (
     HWND hwnd,
     ULONG ulCnrId,
     ULONG ulRecordFlags,
     PULONG pulCount
)
{
    BOOL fSuccess = FALSE;
    PMINIRECORDCORE prec;
    ULONG ulCount = 0;

    do
    {
        // prevent flickering
        WinEnableWindowUpdate(WinWindowFromID(hwnd, ulCnrId), FALSE);

        // any record matching to flags
        prec = (PMINIRECORDCORE) WinSendDlgItemMsg(hwnd, ulCnrId, CM_QUERYRECORDEMPHASIS, MPFROMLONG(CMA_FIRST), MPFROMLONG(ulRecordFlags));
        if (prec == (PMINIRECORDCORE)-1)
            prec = NULL;

        while (prec != NULL)
        {
            ulCount++;

            // adress next record
            prec = (PMINIRECORDCORE) WinSendDlgItemMsg(hwnd, ulCnrId, CM_QUERYRECORDEMPHASIS, MPFROMLONG(prec), MPFROMLONG(ulRecordFlags));
            if (prec == (PMINIRECORDCORE)-1)
                prec = NULL;
        }

        // ok
        fSuccess = TRUE;

    }
    while (FALSE);

    return fSuccess;

}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : ListContainerRecords                                       ³
 *³ Eingabe   : HWND   - window handle                                     ³
 *³             ULONG  - container ID                                      ³
 *³ Aufgaben  : - Records l”schen                                          ³
 *³ Rckgabe  : BOOL    - Ergebnisflag                                     ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

BOOL ListContainerRecords
 (
     HWND hwnd,
     ULONG ulCnrId
)
{
    PMINIRECORDCORE prec;
    PANIEDITRECORD pmyrec;

#ifdef DEBUG

    do
    {
        // any record matching to flags
        prec = (PMINIRECORDCORE) WinSendDlgItemMsg(hwnd, ulCnrId, CM_QUERYRECORDEMPHASIS, MPFROMLONG(CMA_FIRST), 0);
        if (prec == -1)
            prec = NULL;

        while (prec != NULL)
        {
            DEBUGMSG("* list rec -> 0x%08x" NEWLINE, prec);

            // adress next record
            prec = (PMINIRECORDCORE) WinSendDlgItemMsg(hwnd, ulCnrId, CM_QUERYRECORDEMPHASIS, MPFROMLONG(prec), 0);
            if (prec == -1)
                prec = NULL;
        }

    }
    while (FALSE);

#endif

    return TRUE;

}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : SetSpinbutton                                              ³
 *³ Kommentar :                                                            ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 12.06.1996                                                 ³
 *³ Žnderung  : 12.06.1996                                                 ³
 *³ aufgerufen: Window Procedure                                           ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : ###                                                        ³
 *³ Aufgaben  : - Inhalt des Spinbuttons neu setzen                        ³
 *³ Rckgabe  : BOOL - Flag, ob erfolgreich                                ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

#define USE_NUM_SPINBUTTON

#ifdef USE_NUM_SPINBUTTON
#define SPINBUTTON_INDEX(v,min,step)       ( v )
#else
#define SPINBUTTON_INDEX(v,min,step)       ( (v - min) / step )
#endif

#define SETSB2MS(hwnd,id)  (SetSpinButton(hwnd, id, 0, TRUE))
#define SETSB2JIF(hwnd,id) (SetSpinButton(hwnd, id, 0, TRUE))

#define SETSBVALUE(hwnd,id,v)  (SetSpinButton(hwnd, id, v, FALSE))

BOOL SetSpinButton
 (
     HWND hwnd,
     ULONG ulSbId,
     ULONG ulNewValue,          // always in MS !
      BOOL fUnitChanged
)
{
    BOOL fSuccess = FALSE;
    CHAR szValue[20];
    ULONG ulValue;
    BOOL fValue;


    ULONG ulMinValue;
    ULONG ulMaxValue;
    ULONG ulValueStep;

    do
    {
        // init unit depedant vars
        switch (ulCurrentUnit)
        {
            default:
                ulMinValue = TIMEOUT_MIN;
                ulMaxValue = TIMEOUT_MAX;
                ulValueStep = TIMEOUT_STEP;
                break;

            case UNIT_JIF:
                ulMinValue = MS2JIF(TIMEOUT_MIN);
                ulMaxValue = MS2JIF(TIMEOUT_MAX);
                ulValueStep = 1;
                break;
        }

        if (fUnitChanged)
        {

            // query current value
            WinSendDlgItemMsg(hwnd, ulSbId, SPBM_QUERYVALUE,
                              (MPARAM) szValue,
                    (MPARAM) MPFROM2SHORT(sizeof(szValue), SPBQ_DONOTUPDATE));
            fValue = (szValue[0] != 0);
            if (fValue)
                ulValue = atol(szValue);

            // value in new unit
            switch (ulCurrentUnit)
            {
                default:
                    if (fValue)
                        ulNewValue = JIF2MS(ulValue);
                    else
                        ulNewValue = DEFAULT_ANIMATION_TIMEOUT;
                    break;

                case UNIT_JIF:
                    if (fValue)
                        ulNewValue = MS2JIF(ulValue);
                    else
                        ulNewValue = MS2JIF(DEFAULT_ANIMATION_TIMEOUT);
                    break;
            }

            // set spinbutton array
            if (!SetSpinbuttonValues(hwnd, ulSbId, ulMinValue, ulMaxValue, ulValueStep))
                break;

        }                       // if (fUnitChhanged)

        else
        {
            if (ulCurrentUnit == UNIT_JIF)
                ulNewValue = MS2JIF(ulValue);
        }

        // set value
        fSuccess = (BOOL)DLGSETSPIN(hwnd,
                                    ulSbId,
                                    SPINBUTTON_INDEX(ulNewValue,
                                                     ulMinValue,
                                                     ulValueStep));

    }
    while (FALSE);

    return fSuccess;

}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : SetSpinbuttonValues                                        ³
 *³ Kommentar :                                                            ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 12.06.1996                                                 ³
 *³ Žnderung  : 12.06.1996                                                 ³
 *³ aufgerufen: Window Procedure                                           ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : HWND   - frame window handle                               ³
 *³             ULONG  - ID des spinbutton                                 ³
 *³             ULONG  - Minimalwert                                       ³
 *³             ULONG  - Maximalwert                                       ³
 *³             ULONG  - Schrittweite                                      ³
 *³ Aufgaben  : - Inhalt des Spinbuttons neu setzen                        ³
 *³ Rckgabe  : BOOL - Flag, ob erfolgreich                                ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

BOOL SetSpinbuttonValues
 (
     HWND hwnd,
     ULONG ulSbId,
     ULONG ulMinValue,
     ULONG ulMaxValue,
     ULONG ulValueStep
)
{
    BOOL fSuccess = FALSE;
    ULONG i;

    PSZ apszValues = NULL;
    PSZ pszThisValue;
    PPSZ ppszValueEntry;

    ULONG ulValueCount = ((ulMaxValue - ulMinValue) / ulValueStep) + 1;

#ifdef USE_NUM_SPINBUTTON
    WinSendDlgItemMsg(hwnd, ulSbId,
                      SPBM_SETLIMITS,
                      MPFROMLONG(ulMaxValue),
                      MPFROMLONG(ulMinValue));

    return (BOOL)(WinSendDlgItemMsg(hwnd, ulSbId,
                                    SPBM_SETCURRENTVALUE,
                                    MPFROMLONG(ulMinValue), 0));

#else


    do
    {
        // check parms
        if ((ulMinValue > 9999) ||
            (ulMaxValue > 9999))
            break;

        // initialize spin button for timeframe value
        apszValues = malloc(ulValueCount * (sizeof(PSZ) + 5));
        if (!apszValues)
            break;

        // create value array in memory
        for (i = 0,
             ppszValueEntry = (PPSZ) apszValues,
             pszThisValue = apszValues + (sizeof(PSZ) * ulValueCount);

             i < ulValueCount;

             i++,
             ppszValueEntry++,
             pszThisValue += 5)
        {
            _ltoa((i * ulValueStep) + ulMinValue, pszThisValue, 10);
            *ppszValueEntry = pszThisValue;
        }

        // activate array
        WinSendDlgItemMsg(hwnd, ulSbId,
                          SPBM_SETARRAY,
                          (MPARAM) apszValues,
                          MPFROMLONG(ulValueCount));

        fSuccess = TRUE;
    }
    while (FALSE);


// cleanup
    if (apszValues)
        free(apszValues);
    return fSuccess;

#endif

}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : Dialog Subfunctions                                        ³
 *³ Eingabe   : HWND   - window handle                                     ³
 *³             ULONG  - message id                                        ³
 *³             MPARAM - message parm 1                                    ³
 *³             MPARAM - message parm 2                                    ³
 *³ Aufgaben  : - Messages bearbeiten                                      ³
 *³ Rckgabe  : MRESULT - Message Result                                   ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

// ========================================================================

MRESULT DLG_RESTART_ANIMATION(HWND hwnd, MPARAM mp1, MPARAM mp2)

{
    PPARM pparm = WinQueryWindowPtr(hwnd, QWL_USER);

    pparm->precCurrent = (PMINIRECORDCORE) WinSendDlgItemMsg(hwnd, IDDLG_CN_FRAMESET, CM_QUERYRECORD, 0, MPFROM2SHORT(CMA_FIRST, CMA_ITEMORDER));
    if (pparm->precCurrent != (PMINIRECORDCORE)-1)
        pparm->precCurrent = NULL;

    return (MRESULT) TRUE;
}

// ========================================================================

MRESULT DLG_FUNC_ENABLE_ANIMATE(HWND hwnd, BOOL fAnimate)

{
    PPARM pparm = WinQueryWindowPtr(hwnd, QWL_USER);
    HAB hab = WinQueryAnchorBlock(hwnd);
    HWND hwndIcon = WinWindowFromID(hwnd, IDDLG_ST_PREVIEW);

    if (!fAnimate)
    {
        // stop animation
        WinStopTimer(hab, hwnd, ANIMATION_TIMER_ID);

#ifdef USE_BITMAP_BUTTON
        DLGSETIMAGE(hwnd, DID_OK, IDDLG_BMP_START);
#else
        DLGSETSTRING(hwnd, DID_OK, "~Start");
#endif

        DLG_RESTART_ANIMATION(hwnd, 0, 0);
        if (pparm->precCurrent)
            if (pparm->precCurrent->hptrIcon)
                WinSendMsg(hwndIcon, SM_SETHANDLE, MPFROMLONG(pparm->precCurrent->hptrIcon), 0);

        if (pparm->ulPtrCount == 1)
            ENABLECONTROL(hwnd, DID_OK, FALSE);

        // reset to first animation icon
        // if no record present, clear image
        pparm->precCurrent = (PMINIRECORDCORE) WinSendDlgItemMsg(hwnd, IDDLG_CN_FRAMESET, CM_QUERYRECORD, 0, MPFROM2SHORT(CMA_FIRST, CMA_ITEMORDER));
        if (pparm->precCurrent)
            WinSendMsg(hwndIcon, SM_SETHANDLE, MPFROMLONG(pparm->precCurrent->hptrIcon), 0);
        else
            WinSendMsg(hwndIcon, SM_SETHANDLE, 0, 0);
    }
    else
    {
        // start animation
        WinPostMsg(hwnd, WM_TIMER, MPFROMLONG(ANIMATION_TIMER_ID), 0);

#ifdef USE_BITMAP_BUTTON
        DLGSETIMAGE(hwnd, DID_OK, IDDLG_BMP_STOP);
#else
        DLGSETSTRING(hwnd, DID_OK, "~Stop");
#endif

        ENABLECONTROL(hwnd, DID_OK, TRUE);
    }

// save new state
    pparm->fAnimate = fAnimate;

    return (MRESULT) TRUE;
}

// ========================================================================

MRESULT DLG_SETTITLE(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    CHAR szTitle[3 * _MAX_PATH];
    PPARM pparm = WinQueryWindowPtr(hwnd, QWL_USER);

    strcpy(szTitle, szAppTitle);
    strcat(szTitle, pstTitleSpacer);

// add filename
    if (pparm->szAnimationName[0] == 0)
        strcat(szTitle, "(Untitled)");
    else
        strcat(szTitle, pparm->szAnimationName);

// add pallettename
    if (pparm->szPaletteName[0] != 0)
    {
        strcat(szTitle, pstTitleSpacer);
        strcat(szTitle, "(");
        strcpy(szTitle, pparm->szPaletteName);
        strcat(szTitle, ")");
    }

// add modified flag
    if (pparm->fModified)
    {
        strcat(szTitle, pstTitleSpacer);
        strcat(szTitle, "(modified)");
    }

// now set title
    WinSetDlgItemText(hwnd, FID_TITLEBAR, szTitle);
    return (MRESULT) FALSE;
}

// ========================================================================

MRESULT DLG_MODIFIED(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    BOOL fModified = LONGFROMMP(mp1);
    PPARM pparm = WinQueryWindowPtr(hwnd, QWL_USER);

    pparm->fModified = fModified;
    DLG_SETTITLE(hwnd, 0, 0);
    return (MRESULT) FALSE;

}

// ========================================================================

MRESULT DLG_ENABLECONTROLS(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    BOOL fEnable = LONGFROMMP(mp1);

    ENABLECONTROL(hwnd, IDDLG_ST_SHOWFOR, fEnable);
    ENABLECONTROL(hwnd, IDDLG_SB_FRAMELENGTH, fEnable);
    ENABLECONTROL(hwnd, IDDLG_ST_UNIT, fEnable);

    return (MRESULT) FALSE;
}

// ========================================================================

MRESULT DLG_FILE_SAVE(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    BOOL fSuccess = FALSE;
    PPARM pparm = WinQueryWindowPtr(hwnd, QWL_USER);
    PPOINTERLIST ppl = &pparm->appl[pparm->ulIndex];
    BOOL fSaveAllPointers = FALSE;

    do
    {
        // anything there being modified ?
        if ((!pparm->fModified) || (!pparm->fLoaded))
            break;

        // copy container items to pointerlist
        ContainerToPointerlist(hwnd, IDDLG_CN_FRAMESET,
                               &pparm->appl[pparm->ulIndex], pparm);

        // write the file
        ppl->fModifyNameOnSave = TRUE;
        fSaveAllPointers = (pparm->ulFileType == RESFILETYPE_ANIMOUSE);
        fSuccess = WriteTargetFiles(pparm->szAnimationName,
                                    pparm->ulFileType,
                                    ppl, fSaveAllPointers);
        if (!fSuccess)
            ShowMessage(ERROR_INVALID_PARAMETER, "Cannot save animation.");
        else
        {
            // get name of first animation file
            ppl = &pparm->appl[pparm->ulIndex];
            strcpy(pparm->szAnimationName, ppl->szAnimationFile);

            // delete modify flag and refresh title
            DLG_MODIFIED(hwnd, MPFROMLONG(FALSE), 0);
        }

    }
    while (FALSE);

    return (MRESULT) fSuccess;
}

// ========================================================================

MRESULT DLG_FILE_SAVEAS(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    APIRET rc = NO_ERROR;
    BOOL fSuccess = TRUE;

    CHAR szOldAnimationName[_MAX_PATH];
    ULONG ulFileType;
    BOOL fModified;

    PPARM pparm = WinQueryWindowPtr(hwnd, QWL_USER);
    PPOINTERLIST ppl = &pparm->appl[pparm->ulIndex];


// backup old values
    fModified = pparm->fModified;
    ulFileType = pparm->ulFileType;
    strcpy(szOldAnimationName, pparm->szAnimationName);

    if (SelectFile(hwnd, FDS_SAVEAS_DIALOG,
                   pparm->szAnimationName, &pparm->ulFileType,
                   pparm->szAnimationName, sizeof(pparm->szAnimationName)))
    {

        // delete numeraion for certain taarget file types
        if ((pparm->ulFileType == RESFILETYPE_WINANIMATION) ||
            (pparm->ulFileType == RESFILETYPE_ANIMOUSE))
        {
            ChangeFilename(pparm->szAnimationName,
                           CHANGE_DELNUMERATION,
                           pparm->szAnimationName,
                           sizeof(pparm->szAnimationName),
                           NULL,
                           0, 0);
        }

        // save the animation
        pparm->fModified = TRUE;
        ppl->fModifyNameOnSave = TRUE;
        fSuccess = (BOOL)DLG_FILE_SAVE(hwnd, 0, 0);
        if (!fSuccess)
        {
            // restore old values
            pparm->fModified = fModified;
            pparm->ulFileType = ulFileType;
            strcpy(pparm->szAnimationName, szOldAnimationName);
        }
    }

    return (MRESULT) fSuccess;
}

// ========================================================================

MRESULT DLG_QUERY_SAVE(HWND hwnd, MPARAM mp1, MPARAM mp2)

{
    APIRET rc = NO_ERROR;
    BOOL fSuccess = TRUE;
    PPARM pparm = WinQueryWindowPtr(hwnd, QWL_USER);
    PPOINTERLIST ppl = &pparm->appl[pparm->ulIndex];

    if (pparm->fModified)
    {
        // display message // @@todo
        rc = WinMessageBox(HWND_DESKTOP, HWND_DESKTOP,
                           "Animation has been modified. "
                           "Save current animation ?", szAppTitle, 0,
                           MB_MOVEABLE | MB_YESNOCANCEL | MB_QUERY);
        switch (rc)
        {
            case MBID_YES:
                if (pparm->szAnimationFile[0] == 0)
                    rc = (APIRET)DLG_FILE_SAVEAS(hwnd, 0, 0);
                else
                    rc = (APIRET)DLG_FILE_SAVE(hwnd, 0, 0);
                break;

            case MBID_NO:
                break;

            case MBID_CANCEL:
                fSuccess = FALSE;
                break;

        }
    }
    return (MRESULT) rc;
}

// ========================================================================

MRESULT DLG_FILE_NEW(HWND hwnd, MPARAM mp1, MPARAM mp2)

{
    APIRET rc = NO_ERROR;
    BOOL fSuccess = TRUE;
    PPARM pparm = WinQueryWindowPtr(hwnd, QWL_USER);
    PPOINTERLIST ppl = &pparm->appl[pparm->ulIndex];

    do
    {
        // check for unsaved data
        if ((ULONG)DLG_QUERY_SAVE(hwnd, 0, 0) == MBID_CANCEL)
            break;

        // stop animation
        DLG_FUNC_ENABLE_ANIMATE(hwnd, FALSE);
        WinSendDlgItemMsg(hwnd, IDDLG_ST_PREVIEW, SM_SETHANDLE, 0, 0);

        // reset pointerlist
        memset(ppl, 0, sizeof(POINTERLIST));
        pparm->szAnimationName[0] = 0;

        // update container
        WinSendMsg(hwnd, WM_COMMAND, MPFROM2SHORT(IDDLG_PB_UNDO, 0), 0L);

        pparm->fLoaded = FALSE;
        DLG_MODIFIED(hwnd, MPFROMLONG(FALSE), 0);

    }
    while (FALSE);

    return (MRESULT) rc;
}

// ========================================================================

MRESULT DLG_FILE_OPEN(HWND hwnd, MPARAM mp1, MPARAM mp2)

{
    APIRET rc = NO_ERROR;
    BOOL fSuccess = TRUE;
    PPARM pparm = WinQueryWindowPtr(hwnd, QWL_USER);
    PPOINTERLIST ppl = &pparm->appl[pparm->ulIndex];

    CHAR szNewAnimationName[_MAX_PATH];
    CHAR szFullName[_MAX_PATH];

    ULONG ulFileType = 0;

    do
    {
        // check for unsaved data
        if ((ULONG)DLG_QUERY_SAVE(hwnd, 0, 0) == MBID_CANCEL)
            break;

        // open file dialog
        if ((pparm->szAnimationName[0] == 0) || (pparm->fLoaded))
        {

            if (!SelectFile(hwnd, FDS_OPEN_DIALOG, "", &ulFileType,
                            szNewAnimationName, sizeof(szNewAnimationName)))
                break;
            if (szNewAnimationName[0] == 0)
                break;
        }
        else
            strcpy(szNewAnimationName, pparm->szAnimationName);

        // delete old animation
        if (pparm->fLoaded)
        {
            // delete animation
            DLG_FILE_NEW(hwnd, 0, 0);
        }

        // query filetype
        QueryResFileType(szNewAnimationName, &pparm->ulFileType, NULL, 0);

        // load animation
        memset(ppl, 0, sizeof(POINTERLIST));
        if (!LoadPointerAnimation(0, &pparm->appl[0],
                                  szNewAnimationName,
                                  (pparm->ulFileType == RESFILETYPE_ANIMOUSE),
                                  FALSE, FALSE))
        {
            rc = ShowMessage(ERROR_INVALID_PARAMETER, "Cannot load animation.");
            break;
        }
        else
        {
            ppl = &pparm->appl[pparm->ulIndex];
            strcpy(pparm->szAnimationName, szNewAnimationName);
            pparm->fLoaded = TRUE;
        }

        // update dialog
        WinSendMsg(hwnd, WM_COMMAND, MPFROM2SHORT(IDDLG_PB_UNDO, 0), 0L);

    }
    while (FALSE);

    return (MRESULT) FALSE;

}

// ========================================================================

MRESULT DLG_FUNC_EDIT_DELETE(HWND hwnd, PVOID pvrec, ULONG ulRecordStyle)

{
    APIRET rc = NO_ERROR;
    BOOL fSuccess = FALSE;
    PMINIRECORDCORE prec;
    PANIEDITRECORD pmyrec;
    PPARM pparm = WinQueryWindowPtr(hwnd, QWL_USER);
    ULONG ulDeleted = 0;


    rc = DeleteContainerRecords(hwnd, IDDLG_CN_FRAMESET, pvrec, ulRecordStyle, &ulDeleted);
    if (ulDeleted)
        pparm->fModified = TRUE;
    pparm->ulPtrCount -= ulDeleted;
    if (pparm->fAnimate)
        pparm->fAnimate = (pparm->ulPtrCount > 1);
    return (MRESULT) rc;
}

// ========================================================================

// mp2: record for context menu
BOOL DLG_FUNC_SELECT_SOURCE(HWND hwnd, PVOID pvrec)
{
    BOOL fSuccess = FALSE;
    PMINIRECORDCORE prec = pvrec;
    ULONG ulCount;

    do
    {
        // select only record being clicked on,
        // if this one is not selected
        if ((prec) && ((prec->flRecordAttr & CRA_SELECTED) == 0))
        {
            // set source emphasis
            fSuccess = (BOOL)WinSendDlgItemMsg(hwnd,
                                               IDDLG_CN_FRAMESET,
                                               CM_SETRECORDEMPHASIS,
                                               MPFROMP(prec),
                                               MPFROM2SHORT(TRUE,
                                                            CRA_SOURCE));
            DEBUGMSG(" set source emphasis on 0x%08x: %u" NEWLINE, prec _c_ fSuccess);
            break;
        }

        // query items
        prec = (PMINIRECORDCORE) WinSendDlgItemMsg(hwnd, IDDLG_CN_FRAMESET, CM_QUERYRECORDEMPHASIS, MPFROMLONG(CMA_FIRST), MPFROMLONG(CRA_SELECTED));
        if (prec == (PMINIRECORDCORE)-1)
            prec = NULL;

        DEBUGMSG(" set source emphasis: first selected record is 0x%08x" NEWLINE, prec);

        while (prec != NULL)
        {
            // set source emphasis
            fSuccess = (BOOL)WinSendDlgItemMsg(hwnd, IDDLG_CN_FRAMESET,
                                               CM_SETRECORDEMPHASIS,
                                               MPFROMP(prec),
                                               MPFROM2SHORT(TRUE, CRA_SOURCE));
            DEBUGMSG(" set source emphasis on 0x%08x: %u" NEWLINE, prec _c_ fSuccess);

            // next entry
            prec = (PMINIRECORDCORE) WinSendDlgItemMsg(hwnd, IDDLG_CN_FRAMESET, CM_QUERYRECORDEMPHASIS, MPFROMLONG(prec), MPFROMLONG(CRA_SELECTED));
            if (prec == (PMINIRECORDCORE)-1)
                prec = NULL;
        }

        fSuccess = TRUE;

    }
    while (FALSE);

    return fSuccess;
}

// ========================================================================

MRESULT DLG_DESELECT_SOURCE(HWND hwnd, MPARAM mp1, MPARAM mp2)

{
    BOOL fSuccess = FALSE;
    PMINIRECORDCORE prec;

// query items
    prec = (PMINIRECORDCORE) WinSendDlgItemMsg(hwnd, IDDLG_CN_FRAMESET, CM_QUERYRECORDEMPHASIS, MPFROMLONG(CMA_FIRST), MPFROMLONG(CRA_SOURCE));
    if (prec == (PMINIRECORDCORE)-1)
        prec = NULL;

    while (prec != NULL)
    {
        // remove source emphasis
        fSuccess = (BOOL)WinSendDlgItemMsg(hwnd, IDDLG_CN_FRAMESET, CM_SETRECORDEMPHASIS,
                                     MPFROMP(prec),
                                     MPFROM2SHORT(FALSE, CRA_SOURCE));
        DEBUGMSG(" remove source emphasis from 0x%08x: %u" NEWLINE, prec _c_ fSuccess);

        // next entry
        prec = (PMINIRECORDCORE) WinSendDlgItemMsg(hwnd, IDDLG_CN_FRAMESET, CM_QUERYRECORDEMPHASIS, MPFROMLONG(prec), MPFROMLONG(CRA_SOURCE));
        if (prec == (PMINIRECORDCORE)-1)
            prec = NULL;
    }

    return (MRESULT)fSuccess;
}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : EditDialogProc                                             ³
 *³ Kommentar : Window-Procedure fr den Dialog EDITANIMATION              ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 14.11.1996                                                 ³
 *³ Žnderung  : 14.11.1996                                                 ³
 *³ aufgerufen: PM System Message Queue                                    ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : HWND   - window handle                                     ³
 *³             ULONG  - message id                                        ³
 *³             MPARAM - message parm 1                                    ³
 *³             MPARAM - message parm 2                                    ³
 *³ Aufgaben  : - Messages bearbeiten                                      ³
 *³ Rckgabe  : MRESULT - Message Result                                   ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

MRESULT EXPENTRY EditDialogProc
 (
     HWND hwnd,
     ULONG msg,
     MPARAM mp1,
     MPARAM mp2
)
{

    PPARM pparm0        // V0.9.3 (2000-05-21) [umoeller]
        = WinQueryWindowPtr(hwnd, QWL_USER);
    PPOINTERLIST ppl;
    HMODULE hmodResource;
    APIRET rc0;         // V0.9.3 (2000-05-21) [umoeller]

    PMINIRECORDCORE prec0;
    PANIEDITRECORD pmyrec;

    static HWND hwndMenu0 = NULLHANDLE;
    static HWND hwndContextMenu = NULLHANDLE;
    static PVOID pvContextSource = NULL;

// determine dynamic vars
    if (pparm0)
    {
        hmodResource = pparm0->hmodResource;
        ppl = &pparm0->appl[pparm0->ulIndex];
    }

    switch (msg)
    {

            // ==========================================================

        case WM_INITDLG:
            {
                APIRET rc;
                PPARM pparm;
                HWND hptrIcon;
                HWND hwndPushbutton = WinWindowFromID(hwnd, DID_OK);
                HWND hwndIcon = WinWindowFromID(hwnd, IDDLG_ST_PREVIEW);
                HWND hwndGB = WinWindowFromID(hwnd, IDDLG_GB_PREVIEW);
                SWP swpGB;
                MENUITEM mi;

                // save pointer to result
                pparm = PVOIDFROMMP(mp2);
                if (!pparm)
                {
                    DosBeep(700, 1100);
                    WinDismissDlg(hwnd, MBID_ERROR);
                    break;
                }
                WinSetWindowPtr(hwnd, QWL_USER, pparm);

                // set frame icon
                hptrIcon = WinLoadPointer(HWND_DESKTOP, pparm->hmodResource, IDDLG_DLG_ABOUT);
                WinSendMsg(hwnd, WM_SETICON, (MPARAM) hptrIcon, 0L);

                // load menus
                hwndContextMenu = WinLoadMenu(hwnd, pparm->hmodResource, IDMEN_EDITITEM);
                hwndMenu0 = WinLoadMenu(hwnd, pparm->hmodResource, IDDLG_DLG_EDITANIMATION);
                WinSendMsg(hwnd, WM_UPDATEFRAME, MPFROMLONG(FCF_MENU), 0L);

                // make certain submenus cascaded
                if (WinSendMsg(hwndMenu0, MM_QUERYITEM, MPFROM2SHORT(IDMEN_AE_OPTION_UNIT, TRUE), &mi))
                    WinSetWindowBits(mi.hwndSubMenu,
                                     QWL_STYLE,
                                     MS_CONDITIONALCASCADE,
                                     MS_CONDITIONALCASCADE);

                // Load strings
                InitResources(pparm->hmodResource);

                // init all unit dependant controls
                WinSendMsg(hwnd, WM_COMMAND, MPFROM2SHORT(IDMEN_AE_OPTION_UNIT_MS, 0), 0L);

                // init groupbox text
                DLGSETSTRING(hwnd, IDDLG_GB_SELECTEDFRAME, szSelectedNone);

                // init container once
                SetContainerDetails(hwnd, IDDLG_CN_FRAMESET);

#ifdef USE_BITMAP_BUTTON
                // pushbutton style auf bitmap „ndern
                WinSetWindowULong(hwndPushbutton,
                                  QWL_STYLE,
                                  WinQueryWindowULong(hwndPushbutton,
                                                      QWL_STYLE) | BS_ICON);
#endif


                // preview icon init
                // static text fr ICON  auf SS_ICON „ndern
                WinSetWindowULong(hwndIcon,
                                  QWL_STYLE,
                                  WinQueryWindowULong(hwndIcon,
                                                      QWL_STYLE) | SS_ICON);

                // Window subclassen fr sauberes redraw
                WinSubclassWindow(hwndIcon, SubclassStaticWindowProc);

                // ICON innerhalb der Groupbox positionieren
                if (WinQueryWindowPos(hwndGB, &swpGB))
                {
                    WinSetWindowPos(hwndIcon, HWND_TOP,
                                    swpGB.x + 15,
                                    swpGB.y + ((swpGB.cy - 32) / 2),
                                    32, 32,
                                    SWP_MOVE | SWP_SIZE);
                }


                // initialize dialog values
                WinSendMsg(hwnd, WM_COMMAND, MPFROM2SHORT(IDDLG_PB_UNDO, 0), 0L);

                return (MRESULT) FALSE;
            }
            // break;              // end case WM_INITDLG

            // ==========================================================

        case WM_INITMENU:
            {
                HWND hwndMenu = LONGFROMMP(mp2);

                // reset source context, if not context menu
                if (hwndMenu != hwndContextMenu)
                    pvContextSource = NULL;

                switch (SHORT1FROMMP(mp1))
                {

                    case IDMEN_EDITITEM:    // tut nicht, mp1 ist 0x00008005 ! ?????

                        break;


                        // -------------------------------------------------

                    case IDMEN_AE_FILE:
                        WinSendMsg(hwndMenu, MM_SETITEMATTR,
                                   MPFROM2SHORT(IDMEN_AE_FILE_SAVE, TRUE),
                                   MPFROM2SHORT(MIA_DISABLED,
                          (pparm0->fModified) ? ~MIA_DISABLED : MIA_DISABLED));
                        break;

                        // -------------------------------------------------

                    case IDMEN_AE_EDIT:
                        prec0 = (PMINIRECORDCORE) WinSendDlgItemMsg(hwnd, IDDLG_CN_FRAMESET, CM_QUERYRECORDEMPHASIS, MPFROMLONG(CMA_FIRST), MPFROMLONG(CRA_SELECTED));
                        if (prec0 == (PMINIRECORDCORE)-1)
                            prec0 = NULL;

                        // nothing selected ?
                        WinSendMsg(hwndMenu, MM_SETITEMATTR,
                                   MPFROM2SHORT(IDMEN_AE_EDIT_DELETE, TRUE),
                                   MPFROM2SHORT(MIA_DISABLED,
                                      (prec0) ? ~MIA_DISABLED : MIA_DISABLED));
                        break;

                        // -------------------------------------------------

                    case IDMEN_AE_PALETTE:
                        break;

                        // -------------------------------------------------

                    case IDMEN_AE_OPTION:
                        {
                            MENUITEM mi;
                            ULONG ulOldDefault;
                            ULONG ulNewDefault;

                            WinSendMsg(hwndMenu, MM_SETITEMATTR,
                                  MPFROM2SHORT(IDMEN_AE_OPTION_UNIT_MS, TRUE),
                                       MPFROM2SHORT(MIA_DISABLED,
                                                    (ulCurrentUnit == UNIT_JIF) ? ~MIA_DISABLED : MIA_DISABLED));

                            WinSendMsg(hwndMenu, MM_SETITEMATTR,
                                 MPFROM2SHORT(IDMEN_AE_OPTION_UNIT_JIF, TRUE),
                                       MPFROM2SHORT(MIA_DISABLED,
                                                    (ulCurrentUnit == UNIT_MS) ? ~MIA_DISABLED : MIA_DISABLED));

                            // change default for cascaded menu
                            ulOldDefault = (ulCurrentUnit == UNIT_MS) ?
                                IDMEN_AE_OPTION_UNIT_MS : IDMEN_AE_OPTION_UNIT_JIF;
                            ulNewDefault = (ulCurrentUnit == UNIT_MS) ?
                                IDMEN_AE_OPTION_UNIT_JIF : IDMEN_AE_OPTION_UNIT_MS;

                            if (WinSendMsg(hwndMenu, MM_QUERYITEM, MPFROM2SHORT(IDMEN_AE_OPTION_UNIT, TRUE), &mi))
                            {
                                WinSendMsg(mi.hwndSubMenu, MM_SETDEFAULTITEMID, MPFROMSHORT(ulNewDefault), NULL);

                                // Bug-Workaround: remove the check from old default,
                                // because MM_SETDEFAULTITEMID won't do this always
                                WinSendMsg(hwndMenu, MM_SETITEMATTR,
                                           MPFROM2SHORT(ulOldDefault, TRUE),
                                     MPFROM2SHORT(MIA_CHECKED, ~MIA_CHECKED));
                            }
                        }
                        break;

                }
            }
            break;              // WM_INITMENU:

            // ==========================================================

        case WM_MENUEND:
            DLG_DESELECT_SOURCE(hwnd, 0, 0);
            break;


            // ==========================================================

        case WM_CONTROL:
            switch (SHORT1FROMMP(mp1))
            {

                    // -------------------------------------------------

                case IDDLG_CN_FRAMESET:

                    switch (SHORT2FROMMP(mp1))
                    {

                            // -------------------------------------------------

                        case CN_CONTEXTMENU:
                            {
                                POINTL pointl;
                                PMINIRECORDCORE prec = (PMINIRECORDCORE)mp2;

                                // set source emphasis, if record is not selected
                                // if it is selected, all selected records
                                // are being manipulated
                                if ((prec->flRecordAttr & CRA_SELECTED) == 0)
                                    pvContextSource = prec;
                                else
                                    pvContextSource = NULL;

                                // set source emphasis
                                DLG_FUNC_SELECT_SOURCE(hwnd, mp2);

                                // Pop up the menu
                                WinQueryPointerPos(HWND_DESKTOP, &pointl);
                                WinPopupMenu(HWND_DESKTOP,
                                             hwnd,
                                             hwndContextMenu,
                                             (SHORT) pointl.x,
                                             (SHORT) pointl.y,
                                             0,
                                             PU_HCONSTRAIN | PU_VCONSTRAIN |
                                           PU_MOUSEBUTTON1 | PU_MOUSEBUTTON2 |
                                             PU_MOUSEBUTTON3 | PU_KEYBOARD);

                            }
                            break;  // case CN_CONTEXTMENU:

                            // -------------------------------------------------

                        case CN_EMPHASIS:
                            {
                                ULONG ulFirstItem = 0;
                                ULONG ulCurrentItem = 0;
                                ULONG ulSelected = 0;
                                CHAR szSelection[MAX_RES_STRLEN * 2];
                                PNOTIFYRECORDEMPHASIS pnre = PVOIDFROMMP(mp2);

                                // is ist a SELECT notification ?
                                if (!(pnre->fEmphasisMask & CRA_SELECTED))
                                    break;

                                // search any selected record
                                prec0 = (PMINIRECORDCORE) WinSendDlgItemMsg(hwnd, IDDLG_CN_FRAMESET, CM_QUERYRECORDEMPHASIS, MPFROMLONG(CMA_FIRST), MPFROMLONG(CRA_SELECTED));
                                if (prec0 == (PMINIRECORDCORE)-1)
                                    prec0 = NULL;

                                // nothing selected ?
                                if (!prec0)
                                {
                                    DLG_ENABLECONTROLS(hwnd, MPFROMLONG(FALSE), 0);
                                    DLGSETSTRING(hwnd, IDDLG_GB_SELECTEDFRAME, szSelectedNone);
                                    break;
                                }

                                // enable change of framerate
                                DLG_ENABLECONTROLS(hwnd, MPFROMLONG(TRUE), 0);

                                // set spinbutton to value of first frame
                                pmyrec = (PANIEDITRECORD) prec0;
                                SETSBVALUE(hwnd, IDDLG_SB_FRAMELENGTH, pmyrec->fi.ulTimeoutMS);

                                // query items in order
                                prec0 = (PMINIRECORDCORE) WinSendDlgItemMsg(hwnd, IDDLG_CN_FRAMESET, CM_QUERYRECORD, 0, MPFROM2SHORT(CMA_FIRST, CMA_ITEMORDER));
                                if (prec0 == (PMINIRECORDCORE)-1)
                                    prec0 = NULL;

                                while (prec0 != NULL)
                                {
                                    ulCurrentItem++;

                                    // check for selected items
                                    if (prec0->flRecordAttr & CRA_SELECTED)
                                    {
                                        ulSelected++;
                                        // check for first item
                                        if (!ulFirstItem)
                                            ulFirstItem = ulCurrentItem;
                                    }

                                    // next entry
                                    prec0 = (PMINIRECORDCORE)WinSendDlgItemMsg(
                                                    hwnd,
                                                    IDDLG_CN_FRAMESET,
                                                    CM_QUERYRECORD,
                                                    MPFROMP(prec0),
                                                    MPFROM2SHORT(CMA_NEXT,
                                                                 CMA_ITEMORDER));
                                    if (prec0 == (PMINIRECORDCORE)-1)
                                        prec0 = NULL;
                                }

                                // determine selection text
                                switch (ulSelected)
                                {
                                    case 1:
                                        sprintf(szSelection, szSelectedFrame, ulFirstItem, ulCurrentItem);
                                        break;

                                    default:
                                        sprintf(szSelection, szSelectedFrames, ulSelected);
                                        break;
                                }

                                // display text
                                DLGSETSTRING(hwnd, IDDLG_GB_SELECTEDFRAME, szSelection);
                            }

                            break;  // case CN_EMPHASIS:

                    }           // switch (SHORT2FROMMP( mp1))

                    break;      // case IDDLG_CN_FRAMESET:

                    // -------------------------------------------------

                case IDDLG_SB_FRAMELENGTH:

                    switch (SHORT2FROMMP(mp1))
                    {

                            // -------------------------------------------------

                        case SPBN_CHANGE:
                            {
                                CHAR szValue[20];
                                ULONG ulValue;

                                // search any selected record
                                prec0 = (PMINIRECORDCORE) WinSendDlgItemMsg(hwnd, IDDLG_CN_FRAMESET, CM_QUERYRECORDEMPHASIS, MPFROMLONG(CMA_FIRST), MPFROMLONG(CRA_SELECTED));
                                if (prec0 == (PMINIRECORDCORE)-1)
                                    prec0 = NULL;

                                // nothing selected ?
                                if (!prec0)
                                    break;

                                // query selected timout value - exit, if out of range
                                if (!WinSendDlgItemMsg(hwnd, IDDLG_SB_FRAMELENGTH, SPBM_QUERYVALUE,
                                                       MPFROMP(szValue),
                                                       MPFROM2SHORT(sizeof(szValue), SPBQ_UPDATEIFVALID)))
                                    break;

                                ulValue = atol(szValue);

                                // process all selected items
                                prec0 = (PMINIRECORDCORE) WinSendDlgItemMsg(hwnd, IDDLG_CN_FRAMESET, CM_QUERYRECORDEMPHASIS, MPFROMLONG(CMA_FIRST), MPFROMLONG(CRA_SELECTED));
                                if (prec0 == (PMINIRECORDCORE)-1)
                                    prec0 = NULL;

                                while (prec0)
                                {
                                    pmyrec = (PANIEDITRECORD) prec0;

                                    switch (ulCurrentUnit)
                                    {
                                        default:
                                            pmyrec->fi.ulTimeoutMS = ulValue;
                                            pmyrec->fi.ulTimeoutJIF = MS2JIF(ulValue);
                                            break;

                                        case UNIT_JIF:
                                            pmyrec->fi.ulTimeoutMS = JIF2MS(ulValue);
                                            pmyrec->fi.ulTimeoutJIF = ulValue;
                                            break;
                                    }

                                    // get next record
                                    prec0 = (PMINIRECORDCORE)WinSendDlgItemMsg(
                                                            hwnd,
                                                            IDDLG_CN_FRAMESET,
                                                            CM_QUERYRECORDEMPHASIS,
                                                            prec0,
                                                            MPFROMLONG(CRA_SELECTED));
                                    if (prec0 == (PMINIRECORDCORE)-1)
                                        prec0 = NULL;
                                }

                                // invalidate container
                                WinSendDlgItemMsg(hwnd, IDDLG_CN_FRAMESET, CM_INVALIDATERECORD, 0, 0);

                                // use new value at once
                                WinPostMsg(hwnd, WM_TIMER, MPFROMLONG(ANIMATION_TIMER_ID), 0);

                                // animation has been changed
                                if (!pparm0->fLoadPending)
                                    DLG_MODIFIED(hwnd, MPFROMLONG(TRUE), 0);
                            }
                            break;  // case LN_SELECT:

                    }           // switch (SHORT2FROMMP( mp1))

                    break;      // case IDDLG_SB_FRAMELENGTH:

            }                   // end if (SHORT1FROMMP(mp1) == 1)

            break;              // end case WM_CONTROL



            // ==========================================================

        case WM_TIMER:
            {
                HWND hwndIcon;

                if (SHORT1FROMMP(mp1) == ANIMATION_TIMER_ID)
                {
                    hwndIcon = WinWindowFromID(hwnd, IDDLG_ST_PREVIEW);
                    if (WinIsWindowVisible(hwndIcon))
                    {
                        HPOINTER hptr;
                        ULONG ulTimeout;
                        HAB hab = WinQueryAnchorBlock(hwnd);

                        // stop the timer
                        WinStopTimer(hab, hwnd, ANIMATION_TIMER_ID);

                        // get next record
                        if (pparm0->precCurrent)
                            pparm0->precCurrent = (PMINIRECORDCORE) WinSendDlgItemMsg(hwnd,
                                                    IDDLG_CN_FRAMESET,
                                                    CM_QUERYRECORD,
                                                    pparm0->precCurrent,
                                                    MPFROM2SHORT(CMA_NEXT,
                                                                 CMA_ITEMORDER));
                        if (    (!pparm0->precCurrent)
                             || (pparm0->precCurrent == (PMINIRECORDCORE)-1))
                            pparm0->precCurrent = (PMINIRECORDCORE)WinSendDlgItemMsg(
                                                            hwnd,
                                                            IDDLG_CN_FRAMESET,
                                                            CM_QUERYRECORD,
                                                            0,
                                                            MPFROM2SHORT(CMA_FIRST,
                                                                         CMA_ITEMORDER));
                        if (    (!pparm0->precCurrent)
                             || (pparm0->precCurrent == (PMINIRECORDCORE)-1)
                           )
                            break;

                        // set new handle
                        if (pparm0->precCurrent->hptrIcon)
                            WinSendMsg(hwndIcon,
                                       SM_SETHANDLE,
                                       MPFROMLONG(pparm0->precCurrent->hptrIcon),
                                       0);

                        // restart timer
                        pmyrec = (PANIEDITRECORD)pparm0->precCurrent;
                        WinStartTimer(WinQueryAnchorBlock(hwnd),
                                      hwnd,
                                      ANIMATION_TIMER_ID,
                                      pmyrec->fi.ulTimeoutMS);

                    }           // if (WinIsWindowVisible( hwndIcon))

                }               // if (ulTimerId == ANIMATION_TIMER_ID)

            }
            break;              // end case WM_TIMER

            // ==========================================================

        case WM_COMMAND:
            switch (SHORT1FROMMP(mp1))
            {

                    // -------------------------------------------------

                case DID_OK:
                    DLG_FUNC_ENABLE_ANIMATE(hwnd, !pparm0->fAnimate);
                    break;

                    // -------------------------------------------------

                case IDDLG_PB_UNDO:
                    {
                        PSZ pszString;
                        ULONG ulTimeout = DEFAULT_ANIMATION_TIMEOUT;

                        // check if anything to load
                        if ((pparm0->szAnimationName[0]) && (!pparm0->fLoaded))
                        {
                            DLG_FILE_OPEN(hwnd, 0, 0);
                            break;
                        }

                        // refresh container
                        pparm0->fLoadPending = TRUE;
                        PointerlistToContainer(hwnd,
                                               IDDLG_CN_FRAMESET,
                                               &pparm0->appl[pparm0->ulIndex],
                                               pparm0);
                        pparm0->fLoadPending = FALSE;

                        // save pointer to first frame
                        DLG_RESTART_ANIMATION(hwnd, 0, 0);

                        // set info
                        DLGSETSTRING(hwnd, IDDLG_EF_INFONAME, pparm0->szInfoName);
                        DLGSETSTRING(hwnd, IDDLG_EF_INFOARTIST, pparm0->szInfoArtist);

                        // disable change of frame rate per default
                        DLG_ENABLECONTROLS(hwnd, MPFROMLONG(FALSE), 0);

                        // setup title
                        DLG_SETTITLE(hwnd, 0, 0);

                        // reset flags
                        pparm0->fModified = FALSE;

                        // start animation
                        DLG_FUNC_ENABLE_ANIMATE(hwnd, pparm0->fAnimate);
                    }
                    break;      // case IDDLG_PB_UNDO:

                    // -------------------------------------------------

                case IDMEN_AE_FILE_NEW:
                    DLG_FILE_NEW(hwnd, 0, 0);
                    break;

                    // -------------------------------------------------

                case IDMEN_AE_FILE_OPEN:
                    DLG_FILE_OPEN(hwnd, 0, 0);
                    break;

                    // -------------------------------------------------

                case IDMEN_AE_FILE_SAVE:
                    if (pparm0->szAnimationFile[0] == 0)
                        DLG_FILE_SAVEAS(hwnd, 0, 0);
                    else
                        DLG_FILE_SAVE(hwnd, 0, 0);
                    break;


                    // -------------------------------------------------

                case IDMEN_AE_FILE_SAVEAS:
                    DLG_FILE_SAVEAS(hwnd, 0, 0);
                    break;

                    // -------------------------------------------------

                case IDMEN_AE_FILE_IMPORT:
                    break;

                    // -------------------------------------------------

                case IDMEN_AE_FILE_EXIT:
                    WinPostMsg(hwnd, WM_QUIT, 0, 0);
                    break;

                    // -------------------------------------------------

                case IDMEN_AE_EDIT_COPY:
                case IDMEN_AE_EDIT_CUT:
                case IDMEN_AE_EDIT_PASTE:
                    break;

                    // -------------------------------------------------

                case IDMEN_AE_EDIT_DELETE:
                    DLG_FUNC_EDIT_DELETE(hwnd, pvContextSource, CRA_SELECTED);
                    DLG_FUNC_ENABLE_ANIMATE(hwnd, pparm0->fAnimate);
                    break;

                    // -------------------------------------------------

                case IDMEN_AE_EDIT_SELECTALL:
                case IDMEN_AE_EDIT_DESELECTALL:
                    prec0 = (PMINIRECORDCORE) WinSendDlgItemMsg(hwnd, IDDLG_CN_FRAMESET, CM_QUERYRECORD, 0, MPFROM2SHORT(CMA_FIRST, CMA_ITEMORDER));
                    if (prec0 == (PMINIRECORDCORE)-1)
                        prec0 = NULL;
                    while (prec0 != NULL)
                    {
                        // set selection
                        if (SHORT1FROMMP(mp1) == IDMEN_AE_EDIT_SELECTALL)
                            prec0->flRecordAttr |= CRA_SELECTED;
                        else
                            prec0->flRecordAttr &= ~CRA_SELECTED;

                        prec0 = (PMINIRECORDCORE) WinSendDlgItemMsg(hwnd,
                                                        IDDLG_CN_FRAMESET,
                                                        CM_QUERYRECORD,
                                                        MPFROMP(prec0),
                                                        MPFROM2SHORT(CMA_NEXT,
                                                                     CMA_ITEMORDER));
                    }

                    // invalidate container
                    WinSendDlgItemMsg(hwnd, IDDLG_CN_FRAMESET, CM_INVALIDATERECORD, 0, 0);

                    // notify dialog of selection change
                    WinUpdateWindow(WinWindowFromID(hwnd, IDDLG_CN_FRAMESET));
                    break;

                case IDMEN_AE_PALETTE_OPEN:
                case IDMEN_AE_PALETTE_SAVEAS:
                case IDMEN_AE_PALETTE_COPY:
                case IDMEN_AE_PALETTE_PASTE:
                    break;

                    // -------------------------------------------------

                case IDMEN_AE_OPTION_UNIT_MS:
                case IDMEN_AE_OPTION_UNIT_JIF:
                    {

                        // change current unit
                        if (SHORT1FROMMP(mp1) == IDMEN_AE_OPTION_UNIT_MS)
                            ulCurrentUnit = UNIT_MS;
                        else
                            ulCurrentUnit = UNIT_JIF;

                        // toggle unit for spinbutton value
                        SetSpinButton(hwnd, IDDLG_SB_FRAMELENGTH, 0, TRUE);

                        // update unit within static controls
                        DLGSETSTRING(hwnd, IDDLG_ST_UNIT, apszUnit[ulCurrentUnit]);

                        // set containerentries to new unit
                        SetContainerDetails(hwnd, IDDLG_CN_FRAMESET);

                    }
                    break;

                    // -------------------------------------------------

                case IDMEN_AE_HELP_INDEX:
                case IDMEN_AE_HELP_GENERAL:
                case IDMEN_AE_HELP_USING:
                case IDMEN_AE_HELP_KEYS:
                case IDMEN_AE_HELP_ABOUT:
                    break;

            }                   // end if (SHORT1FROMMP(mp1) == 1)

            return (MRESULT) TRUE;

            // break;              // end case WM_COMMAND

            // ==========================================================

        case WM_CLOSE:
            if (pparm0->fModified)
                if ((ULONG)DLG_QUERY_SAVE(hwnd, 0, 0) == MBID_CANCEL)
                    return (MRESULT) TRUE;
            break;

            // ==========================================================

        case WM_DESTROY:
            CopyPointerlist(NULL, &pparm0->appl[pparm0->ulIndex], FALSE);
            break;

    }                           // endswitch

    return (WinDefDlgProc(hwnd, msg, mp1, mp2));

}
