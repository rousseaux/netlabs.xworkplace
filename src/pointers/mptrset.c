
/*
 *@@sourcefile mptrset.c:
 *
 *      This file is ALL new with V0.9.4.
 *
 *@@added V0.9.4 [umoeller]
 *@@header "pointers\mptrset.h"
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

#include "pointers\mptrppl.h"
#include "pointers\mptrset.h"
#include "pointers\mptrptr.h"
#include "pointers\mptrcnr.h"
#include "pointers\mptrutil.h"
#include "pointers\mptranim.h"
#include "pointers\wmuser.h"
#include "pointers\pointer.h"
#include "pointers\cursor.h"
#include "pointers\dll.h"
#include "pointers\script.h"
#include "pointers\macros.h"
#include "pointers\debug.h"

// reload unterbinden
// #define SURPRESS_RELOAD

// pmprintf-Ausgaben in Animations-Threads/ im Animations-Loader erlauben
// #define DEBUG_THREAD
// #define DEBUG_LOADER

// Settings Strings
static PSZ pszSettingDefault = "DEFAULT";
static PSZ pszSettingActivateOnLoad = "ACTIVATEONLOAD";
static PSZ pszSettingActivateOnLoadYes = "YES";
static PSZ pszSettingActivateOnLoadNo = "NO";
static PSZ pszSettingAnimation = "ANIMATION";
static PSZ pszSettingAnimationOn = "ON";
static PSZ pszSettingAnimationOff = "OFF";
static PSZ pszSettingAnimationPath = "ANIMATIONPATH";
static PSZ pszSettingAnimationInitDelay = "ANIMATIONINITDELAY";
static PSZ pszSettingBlackWhite = "BLACKWHITE";
static PSZ pszSettingBlackWhiteOn = "ON";
static PSZ pszSettingBlackWhiteOff = "OFF";
static PSZ pszSettingDemo = "DEMO";
static PSZ pszSettingDemoOff = "OFF";
static PSZ pszSettingDemoOn = "ON";
static PSZ pszSettingDragPtrType = "DRAGPTRTYPE";
static PSZ pszSettingDragSetType = "DRAGSETTYPE";
static PSZ pszSettingFrameLength = "FRAMELENGTH";
static PSZ pszSettingFrameLengthAll = "ALL";
static PSZ pszSettingFrameLengthUndefined = "UNDEFINED";
static PSZ pszSettingHidePointer = "HIDEPOINTER";
static PSZ pszSettingHidePointerDelay = "HIDEPOINTERDELAY";
static PSZ pszSettingHidePointerOn = "ON";
static PSZ pszSettingHidePointerOff = "OFF";
static PSZ pszSettingPointer = "POINTER";
static PSZ pszSettingPointerAll = "ALL";
static PSZ pszSettingPointerOn = "ON";
static PSZ pszSettingPointerOff = "OFF";
static PSZ pszSettingUseMouseSetup = "USEMOUSESETUP";
static PSZ pszSettingUseMouseSetupYes = "YES";
static PSZ pszSettingUseMouseSetupNo = "NO";

// strings

// global vars
static BOOL G_fSettingsInitialized = FALSE;

// global vars: object settings
static ULONG G_ulDefaultTimeout = DEFAULT_ANIMATION_TIMEOUT;
static BOOL G_fOverrideTimeout = DEFAULT_OVERRIDETIMEOUT;
static BOOL G_fActivateOnLoad = DEFAULT_ACTIVATEONLOAD;

static BOOL G_fAnimationInitDelayInitialized = FALSE;
static BOOL G_usAnimationInitDelay = 0;

static BOOL G_fAnimationPathInitialized = FALSE;
static CHAR G_szAnimationPath[_MAX_PATH];

static ULONG G_ulDragPtrFileType = DEFAULT_DRAGPTRTYPE;
static ULONG G_ulDragSetFileType = DEFAULT_DRAGSETTYPE;

static BOOL G_fUseMouseSetup = DEFAULT_USEMOUSESETUP;
static BOOL G_fBlackWhite = DEFAULT_BLACKWHITE;
static BOOL G_fHidePointer = DEFAULT_HIDEPOINTER;
static ULONG G_ulHidePointerDelay = DEFAULT_HIDEPOINTERDELAY;

/*⁄ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒø
 *≥ Name      : Get/Set Funktionen fÅr globale Variablen                   ≥
 *¿ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒŸ
 */

BOOL IsSettingsInitialized(VOID)
{
    return G_fSettingsInitialized;
}

// fÅr folgende Settings werden keine extra Zugriffsfunktionen benîtigt
// da Zugriffe Åber die folgenden Funktionen implementiert sind
// DEMO               QueryDemo/EnableDemo
// ANIMATION          QueryAnimation/EnableAnimation
// POINTER            QueryCurrentSettings/LoadPointerAnimation

// fÅr folgende SETTINGS sind Zugriffsfunktionen definiert

// ACTIVATEONLOAD                get/setActivateOnLoad
// ANIMATIONINITDELAY            get/setAnimationInitDelay getDefaultAnimationInitDelay
// ANIMATIONPATH                 get/getAnimationPath
// BLACKWHITE=YES|NO             get/setBlackWhite
// DRAGPTRTYPE=PTR,CUR,ANI,AND   get/setDragPtrType
// DRAGSETTYPE=PTR,CUR,ANI,AND   get/setDragSetType
// FRAMELENGTH=n,ALL|UNDEFINED   get/setDefaultTimeout     get/SetOverrideTimeout
// HIDEPOINTER=ON|OFF            get/set/HidePointer
// HIDEPOINTERDELAY=n            get/set/HidePointerDelay
// USEMOUSESETUP=YES|NO          get/setUseMouseSetup

// ======================================================================
// ACTIVATEONLOAD=YES|NO
// ======================================================================

BOOL getActivateOnLoad(VOID)
{
    return G_fActivateOnLoad;
}
VOID setActivateOnLoad(BOOL fActivate)
{
    G_fActivateOnLoad = fActivate;
}

// ======================================================================
// ANIMATIONINITDELAY=n
// ======================================================================

ULONG _Export getDefaultAnimationInitDelay(VOID)
{
    PSZ pszDelay = NULL;
    ULONG ulDelay = 0;

    pszDelay = DEBUGSETTINGVAL(SET_ANIMINITDELAY);
    if (pszDelay != NULL)
    {
        ulDelay = atol(pszDelay);
        if (ulDelay > INITDELAY_MAX)
            ulDelay = INITDELAY_MAX;
    }
    return ulDelay;
}

// ------------------------------

ULONG _Export getAnimationInitDelay(VOID)
{

    if (G_fAnimationInitDelayInitialized)
        return G_usAnimationInitDelay;
    else
    {
        G_fAnimationInitDelayInitialized = TRUE;
        return getDefaultAnimationInitDelay();
    }
}

VOID _Export setAnimationInitDelay(ULONG ulNewInitDelay)
{
    G_fAnimationInitDelayInitialized = TRUE;
    G_usAnimationInitDelay = ulNewInitDelay;
    return;
}

// ======================================================================
// ANIMATIONPATH=...
// ======================================================================

PCSZ getAnimationPath(VOID)
{
    if (G_fAnimationPathInitialized)
        return G_szAnimationPath;
    else
        return DEFAULT_ANIMATIONPATH;
}

BOOL setAnimationPath(PSZ pszNewPath)
{
    BOOL fResult = FALSE;

    if (pszNewPath != NULL)
    {
        if (FileExist(pszNewPath, TRUE))
        {
            G_fAnimationPathInitialized = TRUE;
            strcpy(G_szAnimationPath, pszNewPath);
            fResult = TRUE;

            // store Animationpath to OS2.INI
            {
                HAB hab = WinQueryAnchorBlock(HWND_DESKTOP);

                PrfWriteProfileData(HINI_PROFILE, "WPAMPTR", "AnimationFilePath",
                                    G_szAnimationPath,
                                    strlen(G_szAnimationPath));
            }
        }
    }
    return fResult;
}

// ======================================================================
// BLACKWHITE=YES|NO
// ======================================================================

BOOL getBlackWhite(VOID)
{
    return G_fBlackWhite;
}

VOID setBlackWhite(BOOL fNewBlackWhite)
{
    G_fBlackWhite = fNewBlackWhite;
}

// ======================================================================
// DRAGPTRTYPE=PTR,CUR,ANI,AND
// use QueryResFileExt()/QueryResFileTypeFromExt() to convert
// ======================================================================
VOID setDragPtrType(ULONG ulResFileType)
{
    G_ulDragPtrFileType = ulResFileType;
    return;
}
ULONG getDragPtrType(VOID)
{
    return G_ulDragPtrFileType;
}

// ======================================================================
// DRAGSETTYPE=PTR,CUR,ANI,AND
// ======================================================================
VOID setDragSetType(ULONG ulResFileType)
{
    G_ulDragSetFileType = ulResFileType;
    return;
}
ULONG getDragSetType(VOID)
{
    return G_ulDragSetFileType;
}

// ======================================================================
// FRAMELENGTH=xxx,ALL|UNDEFINED
// ======================================================================

ULONG getDefaultTimeout(VOID)
{
    return G_ulDefaultTimeout;
}
VOID setDefaultTimeout(ULONG ulTimeout)
{
    if ((ulTimeout >= TIMEOUT_MIN) &&
        (ulTimeout <= TIMEOUT_MAX))
        G_ulDefaultTimeout = ulTimeout;
}

// ------------------------------

BOOL getOverrideTimeout(VOID)
{
    return G_fOverrideTimeout;
}
VOID setOverrideTimeout(BOOL fOverride)
{
    G_fOverrideTimeout = fOverride;
}

// ======================================================================
// HIDEPOINTER=ON|OFF
// ======================================================================

BOOL getHidePointer(VOID)
{
    return G_fHidePointer;
}

VOID setHidePointer(BOOL fNewHidePointer)
{
    WinPostMsg(QueryAnimationHwnd(),
               WM_USER_ENABLEHIDEPOINTER,
               MPFROMSHORT(fNewHidePointer),
               0);
    G_fHidePointer = fNewHidePointer;
}

VOID overrideSetHidePointer(BOOL fNewHidePointer)
{
    G_fHidePointer = fNewHidePointer;
}

// ======================================================================
// HIDEPOINTERDELAY=n
// ======================================================================

ULONG getHidePointerDelay(VOID)
{
    return G_ulHidePointerDelay;
}

VOID setHidePointerDelay(ULONG ulNewHidePointerDelay)
{
    if ((ulNewHidePointerDelay >= HIDEPONTERDELAY_MIN) &&
        (ulNewHidePointerDelay <= HIDEPONTERDELAY_MAX))
        G_ulHidePointerDelay = ulNewHidePointerDelay;
}

// ======================================================================
// USEMOUSESETUP=YES|NO
// ======================================================================

BOOL getUseMouseSetup(VOID)
{
    return G_fUseMouseSetup;
}

VOID setUseMouseSetup(BOOL fNewUseMouseSetup)
{
    G_fUseMouseSetup = fNewUseMouseSetup;
}


/*⁄ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒø
 *≥ Name      : Helper Funktionen                                          ≥
 *¿ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒŸ
 */

BOOL isnumeric(PSZ pszString)
{
    BOOL fResult = TRUE;

    do
    {
        // check Parms
        if ((pszString == NULL) ||
            (*pszString == 0))
        {
            fResult = FALSE;
            break;
        }

        // check digits
        while (*pszString != 0)
        {
            if (!isdigit(*pszString))
            {
                fResult = FALSE;
                break;
            }
            else
                pszString++;
        }

    }
    while (FALSE);

    return fResult;
}

void stripblanks(PSZ string)
{
    PSZ p = string;

    if (p != NULL)
    {
        while ((*p != 0) && (*p <= 32))
        {
            p++;
        }
        strcpy(string, p);
    }
    if (*p != 0)
    {
        p += strlen(p) - 1;
        while ((*p <= 32) && (p >= string))
        {
            *p = 0;
            p--;
        }
    }
}


/*⁄ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒø
 *≥ Name      : ScanSetupString                                            ≥
 *≥ Comment   :                                                            ≥
 *≥ Author    : C.Langanke                                                 ≥
 *≥ Date      : 04.10.1996                                                 ≥
 *≥ Update    : 05.10.1996                                                 ≥
 *≥ called by : diverse                                                    ≥
 *≥ calls     : -                                                          ≥
 *≥ Input     : HWND    - handle of settingspage for update or NULL        ≥
 *≥             PRECORDCORE - ptr to container record for update or NULL   ≥
 *≥             PSZ     - settings string                                  ≥
 *≥             BOOL    - Flag, if valid strings are to be deleted         ≥
 *≥             BOOL    - Flag, if to call in separate thread              ≥
 *≥ Tasks     : - stores current settings                                  ≥
 *≥ returns   : APIRET - OS/2 error code                                   ≥
 *¿ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒŸ
 */

typedef struct _PARM
{
    HWND hwnd;
    PRECORDCORE pcnrrec;
    PSZ pszSetup;
}
PARM, *PPARM;

VOID _Optlink ScanSetupStringAsync(PVOID pvParams) // ULONG ulParms)
{
    APIRET rc = NO_ERROR;
    PPARM pparm = (PPARM)pvParams;
    HAB hab = WinInitialize(0);
    HMQ hmq = WinCreateMsgQueue(hab, 0);
    PSZ pszDelay = NULL;
    ULONG ulDelay = 0;

    PSZ pszInitPriority;
    ULONG ulInitPriority = PRTYC_REGULAR;

    FUNCENTER();

// PrioritÑt des Initialisierungsthreads setzen
    pszInitPriority = DEBUGSETTINGVAL(SET_ANIMINITPRIORITY);
    if (pszInitPriority != NULL)
    {
        ulInitPriority = atol(pszInitPriority);
        if (ulInitPriority > PRTYC_FOREGROUNDSERVER)
            ulInitPriority = PRTYC_REGULAR;
    }

    SetPriority(ulInitPriority);

// ggfs. Initialisierung verzîgern
    ulDelay = getAnimationInitDelay();
    if (ulDelay > 0)
    {
        DEBUGMSG("info: delay initialization via ScanSetup - wait %u secs" NEWLINE, ulDelay);
        DosSleep(ulDelay * 1000);
    }

    rc = ScanSetupString(pparm->hwnd,
                         pparm->pcnrrec,
                         pparm->pszSetup,
                         FALSE,
                         FALSE);

// cleanup
    if (pparm->pszSetup)
        free(pparm->pszSetup);
    free(pparm);

    WinDestroyMsgQueue(hmq);
    WinTerminate(hab);

    _endthread();
}


#define DELIMITER  ';'
#define SEPARATOR  '='
#define SEPVALUE   ','

APIRET _Export ScanSetupString
 (
     HWND hwnd,
     PRECORDCORE pcnrrec,
     PSZ pszSetup,
     BOOL fModify,
     BOOL fCallAsync
)

{
    APIRET rc = NO_ERROR;
    PSZ apszValidStrings[32];
    ULONG ulStringCount;

    ULONG i;
    PSZ pszCopy = NULL;
    PSZ pszScan, pszEnd, pszValue, pszNextValue;
    BOOL fOwnString = FALSE;
    BOOL fModified = FALSE;

    BOOL fEnableDemo = FALSE;
    BOOL fEnableAnimation = FALSE;

    BOOL fToggleAnimation = FALSE;
    BOOL fToggleDemo = FALSE;

    BOOL fToggleBlackWhite = FALSE;
    BOOL fEnableBlackWhite = FALSE;

    BOOL fToggleHidePointer = FALSE;
    BOOL fHidePointer = FALSE;

    CHAR szAnimationName[_MAX_PATH];
    ULONG ulBootDrive;

    FUNCENTER();

    do
    {

        // Note: ParameterprÅfung spÑter machen, damit
        // die Notifizierung der Mauszeiger-Seite auf jeden Fall gemacht wird !


        // ggfs. eigenen thread anlegen
        if (fCallAsync)
        {
            TID tidScanSetup;
            PPARM pparm = malloc(sizeof(PARM));

            if (pparm == NULL)
            {
                rc = ERROR_NOT_ENOUGH_MEMORY;
                break;
            }

            pparm->hwnd = hwnd;
            pparm->pcnrrec = pcnrrec;
            if (pszSetup)
                pparm->pszSetup = strdup(pszSetup);
            else
                pparm->pszSetup = NULL;

            tidScanSetup = _beginthread(ScanSetupStringAsync,
                                        NULL,
                                        16834,
                                        pparm);
            if (tidScanSetup == -1)
            {
                rc = ERROR_ACCESS_DENIED;
                break;
            }

            return NO_ERROR;
        }

        // notifizieren, dass die initialisierung durchgefÅhrt wird
        // bzw. "bald fertig ist"
        G_fSettingsInitialized = TRUE;

        // Parameter prÅfen
        if (pszSetup == NULL)
        {
            rc = ERROR_INVALID_PARAMETER;
            break;
        }

        // Kopie anlegen
        pszCopy = strdup(pszSetup);
        if (pszCopy == NULL)
        {
            DEBUGMSG("error: cannot create copy of setup string" NEWLINE, 0);
            rc = ERROR_NOT_ENOUGH_MEMORY;
            break;
        }
        pszScan = pszCopy;

        // Strings in array eintragen
        // ACHTUNG: ggfs apszValidStrings[] anpasssen
        ulStringCount = 13;
        apszValidStrings[0] = pszSettingDemo;
        apszValidStrings[1] = pszSettingAnimation;
        apszValidStrings[2] = pszSettingPointer;
        apszValidStrings[3] = pszSettingFrameLength;
        apszValidStrings[4] = pszSettingActivateOnLoad;
        apszValidStrings[5] = pszSettingAnimationPath;
        apszValidStrings[6] = pszSettingAnimationInitDelay;
        apszValidStrings[7] = pszSettingBlackWhite;
        apszValidStrings[8] = pszSettingHidePointerDelay;
        apszValidStrings[9] = pszSettingUseMouseSetup;
        apszValidStrings[10] = pszSettingDragPtrType;
        apszValidStrings[11] = pszSettingDragSetType;
        apszValidStrings[12] = pszSettingHidePointer;

        // Basisverzeichnis ermitteln
        DosQuerySysInfo(QSV_BOOT_DRIVE, QSV_BOOT_DRIVE, &ulBootDrive, sizeof(ULONG));

        // String scannen
        while (*pszScan != 0)
        {

            // Flags zurÅcksetzen
            fOwnString = FALSE;

            // Ende der Variable suchen
            pszEnd = strchr(pszScan, DELIMITER);

            // kein Ende gefunden
            if (pszEnd == NULL)
            {
                pszScan += strlen(pszScan);
                continue;
            }
            else
                *pszEnd = 0;

            DEBUGMSG("info: scan %s" NEWLINE, pszScan);

            // jetzt prÅfen, ob das unser SchlÅsselwort ist
            for (i = 0; i < ulStringCount; i++)
            {
                if (strncmp(pszScan, apszValidStrings[i], strlen(apszValidStrings[i])) == 0)
                {

                    // Wert suchen
                    fOwnString = TRUE;
                    pszValue = pszScan + strlen(apszValidStrings[i]);
                    if (*pszValue != SEPARATOR)
                        continue;
                    pszValue++;
                    if (*pszValue == 0)
                        continue;

                    stripblanks(pszValue);
                    if (strlen(pszValue) == 0)
                        continue;

                    // entsprechende Aktion durchfÅhren
                    switch (i)
                    {

                            // - - - - - - - - - - - - - - - - - -

                            // Stati nur vormerken und spÑter setzen !
                        case 0:
                            fToggleDemo = TRUE;
                            fEnableDemo = (strcmpi(pszValue, pszSettingDemoOn) == 0);
                            break;

                            // - - - - - - - - - - - - - - - - - -

                        case 1:
                            fToggleAnimation = TRUE;
                            fEnableAnimation = (strcmpi(pszValue, pszSettingAnimationOn) == 0);
                            break;

                            // - - - - - - - - - - - - - - - - - -

                        case 2:
                            {
                                BOOL fSkipLoading = TRUE;
                                BOOL fLoadSet = TRUE;
                                ULONG p;
                                ULONG ulPointerIndex = 0;
                                CHAR szThisValue[_MAX_PATH];
                                PSZ pszIndex = pszSettingPointerAll;
                                PSZ pszName = NULL;
                                PSZ pszAnimate = pszSettingPointerOn;
                                BOOL fAnimate = TRUE;
                                BOOL fAllPointer = FALSE;

                                do
                                {
                                    // erster Wert: Index
                                    pszNextValue = strchr(pszValue, SEPVALUE);
                                    if (pszNextValue != NULL)
                                    {
                                        *pszNextValue = 0;
                                        pszNextValue++;
                                    }

                                    stripblanks(pszValue);
                                    fAllPointer = ((strlen(pszValue) == 0) || (strcmpi(pszValue, pszSettingPointerAll) == 0));
                                    if (!fAllPointer)
                                    {
                                        if (isnumeric(pszValue))
                                        {
                                            ulPointerIndex = atol(pszValue);
                                            fLoadSet = FALSE;
                                        }
                                        else
                                            break;
                                    }

                                    if (pszNextValue == NULL)
                                        break;

                                    // zweiter Wert: Name
                                    pszValue = pszNextValue;
                                    pszNextValue = strchr(pszValue, SEPVALUE);
                                    if (pszNextValue != NULL)
                                    {
                                        *pszNextValue = 0;
                                        pszNextValue++;
                                    }

                                    stripblanks(pszValue);
                                    if (strlen(pszValue) > 0)
                                    {
                                        pszName = pszValue;
                                        fSkipLoading = FALSE;
                                    }
                                    else
                                        fSkipLoading = TRUE;

                                    if (pszNextValue == NULL)
                                        break;

                                    // dritter Wert: Animation
                                    pszValue = pszNextValue;
                                    pszNextValue = strchr(pszValue, SEPVALUE);
                                    if (pszNextValue != NULL)
                                    {
                                        *pszNextValue = 0;
                                        pszNextValue++;
                                    }

                                    stripblanks(pszValue);
                                    if (strcmpi(pszValue, pszSettingPointerOn) != 0)
                                        fAnimate = FALSE;

                                    if (pszNextValue == NULL)
                                        break;

                                }
                                while (FALSE);

                                // Index prÅfen
                                if (ulPointerIndex >= NUM_OF_SYSCURSORS)
                                    break;

                                // jetzt Animation laden
                                if (!fSkipLoading)
                                {

                                    if ((pszName != NULL) && (strcmpi(pszName, pszSettingDefault) == 0))
                                        pszName = NULL;

                                    // Animationsname ermitteln
                                    if (pszName != NULL)
                                    {
                                        // bei Namen ohne Laufwerksangabe: Basispfad voranstellen
                                        if (strchr(pszName, ':') == NULL)
                                            sprintf(szAnimationName,
                                                    "%s\\%s",
                                                    getAnimationPath(),
                                                    pszName);
                                        else
                                            // Name Åbernehmen
                                            strcpy(szAnimationName, pszName);

                                        // ggfs. Bootdrive eintragen
                                        if (szAnimationName[0] == '?')
                                            szAnimationName[0] = (CHAR) ulBootDrive + 'A' - 1;

                                        pszName = szAnimationName;
                                    }

                                    // Laden !
                                    LoadPointerAnimation(ulPointerIndex, QueryPointerlist(ulPointerIndex), pszName, fAllPointer, FALSE, FALSE);

                                }

                                // Animation an/ausstellen
                                ToggleAnimate(hwnd, ulPointerIndex, NULL, pcnrrec, fAllPointer, FALSE, &fAnimate);

                            }
                            break;

                            // - - - - - - - - - - - - - - - - - -

                        case 3:
                            {
                                ULONG ulNewDefaultTimeout = getDefaultTimeout();
                                BOOL fOverrideTimeout;

                                do
                                {
                                    // erster Wert: Timeout Wert
                                    pszNextValue = strchr(pszValue, SEPVALUE);
                                    if (pszNextValue != NULL)
                                    {
                                        *pszNextValue = 0;
                                        pszNextValue++;
                                    }


                                    if (*pszValue != 0)
                                    {
                                        if (strcmpi(pszValue, pszSettingDefault) == 0)
                                            ulNewDefaultTimeout = DEFAULT_ANIMATION_TIMEOUT;
                                        else if (isnumeric(pszValue))
                                            ulNewDefaultTimeout = atol(pszValue);

                                        setDefaultTimeout(ulNewDefaultTimeout);
                                    }

                                    if (pszNextValue == NULL)
                                        break;

                                    // zweiter Wert: Angabe fÅr alle oder undefinierte
                                    pszValue = pszNextValue;
                                    pszNextValue = strchr(pszValue, SEPVALUE);
                                    if (pszNextValue != NULL)
                                    {
                                        *pszNextValue = 0;
                                        pszNextValue++;
                                    }

                                    if (strcmpi(pszValue, pszSettingFrameLengthAll) == 0)
                                        fOverrideTimeout = TRUE;
                                    else if (strcmpi(pszValue, pszSettingFrameLengthUndefined) == 0)
                                        fOverrideTimeout = FALSE;
                                    else
                                        break;

                                    setOverrideTimeout(fOverrideTimeout);

                                    if (pszNextValue == NULL)
                                        break;


                                }
                                while (FALSE);

                            }
                            break;

                            // - - - - - - - - - - - - - - - - - -

                        case 4:
                            {
                                BOOL fActivateOnLoad = TRUE;

                                if (strcmpi(pszValue, pszSettingActivateOnLoadNo) == 0)
                                    fActivateOnLoad = FALSE;
                                else if (strcmpi(pszValue, pszSettingActivateOnLoadYes) == 0)
                                    fActivateOnLoad = TRUE;
                                else
                                    break;

                                setActivateOnLoad(fActivateOnLoad);

                            }
                            break;

                            // - - - - - - - - - - - - - - - - - -

                        case 5:
                            setAnimationPath(pszValue);
                            break;

                            // - - - - - - - - - - - - - - - - - -

                        case 6:
                            if (isnumeric(pszValue))
                                setAnimationInitDelay(atol(pszValue));
                            break;

                            // - - - - - - - - - - - - - - - - - -

                            // Stati nur vormerken und spÑter setzen !
                        case 7:
                            fToggleBlackWhite = TRUE;
                            fEnableBlackWhite = (strcmpi(pszValue, pszSettingBlackWhiteOn) == 0);
                            break;

                            // - - - - - - - - - - - - - - - - - -

                            // Stati nur vormerken und spÑter setzen !
                        case 8:
                            if (isnumeric(pszValue))
                                setHidePointerDelay(atol(pszValue));
                            break;

                            // - - - - - - - - - - - - - - - - - -

                        case 9:
                            {
                                BOOL fUseMouseSetup = FALSE;

                                if (strcmpi(pszValue, pszSettingUseMouseSetupNo) == 0)
                                    fUseMouseSetup = FALSE;
                                else if (strcmpi(pszValue, pszSettingUseMouseSetupYes) == 0)
                                    fUseMouseSetup = TRUE;
                                else
                                    break;

                                setUseMouseSetup(fUseMouseSetup);
                            }
                            break;

                            // - - - - - - - - - - - - - - - - - -

                        case 10:
                            {
                                BOOL ulFileType;

                                if (QueryResFileTypeFromExt(pszValue, &ulFileType))
                                {
                                    setDragPtrType(ulFileType);
                                }
                            }
                            break;

                            // - - - - - - - - - - - - - - - - - -

                        case 11:
                            {
                                BOOL ulFileType;

                                if (QueryResFileTypeFromExt(pszValue, &ulFileType))
                                {
                                    setDragSetType(ulFileType);
                                }
                            }
                            break;

                            // - - - - - - - - - - - - - - - - - -

                        case 12:
                            fToggleHidePointer = TRUE;
                            fHidePointer = (strcmpi(pszValue, pszSettingHidePointerOn) == 0);
                            break;

                            // - - - - - - - - - - - - - - - - - -


                    }           // end switch (i)

                }               // end if (strstr( apszValidStrings[ i], pszScan) == 1)

            }                   // for (i < ulStringCount)

            // eigenen String lîschen, fremden String Åberspringen
            if (fOwnString)
            {
                strcpy(pszScan, pszEnd + 1);
                fModified = TRUE;
            }
            else
            {
                *pszEnd = DELIMITER;
                pszScan = pszEnd + 1;
            }

        }                       // end while

        // Stati setzen, nur einen Refresh durchfÅhren
        DEBUGMSG("info: refreshing with hwnd=0x%08x, pcnrrec=0x%08x" NEWLINE, hwnd _c_ pcnrrec);
        if (fToggleAnimation)
            ToggleAnimate(hwnd, 0, NULL, pcnrrec, TRUE, FALSE, &fEnableAnimation);
        if (fToggleDemo)
            ToggleDemo(hwnd, pcnrrec, FALSE, &fEnableDemo);
        if (fToggleBlackWhite)
            setBlackWhite(fEnableBlackWhite);
        if (fToggleHidePointer)
            setHidePointer(fHidePointer);
        RefreshCnrItem(hwnd, pcnrrec, pcnrrec, TRUE);

        // soll das Original verÑndert werden ?
        if ((fModify) & (fModified))
            strcpy(pszSetup, pszCopy);

    }
    while (FALSE);

// cleanup
    if (pszCopy)
        free(pszCopy);

    DEBUGMSG("info:rc=%u left SetupString %s" NEWLINE, rc _c_ pszSetup);

    FUNCEXIT(rc);
    return rc;

}

/*⁄ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒø
 *≥ Name      : QueryCurrentSettings                                       ≥
 *≥ Comment   :                                                            ≥
 *≥ Author    : C.Langanke                                                 ≥
 *≥ Date      : 04.11.1995                                                 ≥
 *≥ Update    : 05.11.1995                                                 ≥
 *≥ called by : diverse                                                    ≥
 *≥ calls     : -                                                          ≥
 *≥ Input     : PSZ     - Storage for settings string                      ≥
 *≥             ULONG   - storage length                                   ≥
 *≥ Tasks     : - stores current settings                                  ≥
 *≥ returns   : APIRET - OS/2 error code                                   ≥
 *¿ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒŸ
 */

APIRET _Export QueryCurrentSettings
 (
     PSZ * ppszSettings
)
{
    APIRET rc = NO_ERROR;
    ULONG i;

    PPOINTERLIST ppl, pplndx;

    PSZ pszBuffer = malloc(1024);
    PSZ pszSettings;

    do
    {
        // Parameter prÅfen
        if (ppszSettings == NULL)
        {
            rc = ERROR_INVALID_PARAMETER;
            break;
        }

        // did we get the memory needed
        if (pszBuffer == NULL)
        {
            rc = ERROR_NOT_ENOUGH_MEMORY;
            break;
        }
        pszSettings = pszBuffer;
        *pszSettings = 0;

        // ------------------------------------------------------------------------------

        // jetzt String zusammensetzen
        *pszSettings = 0;

        if (QueryDemo() == DEFAULT_DEMO)
        {
            sprintf(pszSettings,
                    "%s=%s;",
                    pszSettingDemo,
                    (QueryDemo())? pszSettingDemoOn : pszSettingDemoOff);
            pszSettings = ENDSTRING(pszSettings);
        }

        // -----------------

        if (getBlackWhite() != DEFAULT_BLACKWHITE)
        {
            sprintf(pszSettings,
                    "%s=%s;",
                    pszSettingBlackWhite,
                    (getBlackWhite())? pszSettingBlackWhiteOn : pszSettingBlackWhiteOff);
            pszSettings = ENDSTRING(pszSettings);
        }

        // -----------------

        if (getUseMouseSetup() != DEFAULT_USEMOUSESETUP)
        {
            sprintf(pszSettings,
                    "%s=%s;",
                    pszSettingUseMouseSetup,
                    (getUseMouseSetup())? pszSettingUseMouseSetupYes : pszSettingUseMouseSetupNo);
            pszSettings = ENDSTRING(pszSettings);
        }

        // -----------------

        if (getDragPtrType() != DEFAULT_DRAGPTRTYPE)
        {
            sprintf(pszSettings,
                    "%s=%s;",
                    pszSettingDragPtrType,
                    QueryResFileExtension(getDragPtrType()) + 1);
            pszSettings = ENDSTRING(pszSettings);
        }

        // -----------------

        if (getDragSetType() != DEFAULT_DRAGSETTYPE)
        {
            sprintf(pszSettings,
                    "%s=%s;",
                    pszSettingDragSetType,
                    QueryResFileExtension(getDragSetType()) + 1);
            pszSettings = ENDSTRING(pszSettings);
        }

        // -----------------

        if (getHidePointer() != DEFAULT_HIDEPOINTER)
        {
            sprintf(pszSettings,
                    "%s=%s;",
                    pszSettingHidePointer,
                    getHidePointer()? pszSettingHidePointerOn : pszSettingHidePointerOff);
            pszSettings = ENDSTRING(pszSettings);
        }
        // -----------------

        if (getHidePointerDelay() != DEFAULT_HIDEPOINTERDELAY)
        {
            sprintf(pszSettings,
                    "%s=%u;",
                    pszSettingHidePointerDelay,
                    getHidePointerDelay());
            pszSettings = ENDSTRING(pszSettings);
        }

        // -----------------

        // dieses Setting nicht speichern, da der Animationsstatus
        // mit jedem Pointer einzeln gespeichert wird.
        // sprintf( ENDSTRING( pszSettings),
        //          "%s=%s;",
        //          pszSettingAnimation,
        //          (fAnimationActive) ? pszSettingAnimationOn : pszSettingAnimationOff);

        // -----------------

        if ((getDefaultTimeout() != DEFAULT_ANIMATION_TIMEOUT) ||
            (getOverrideTimeout() != DEFAULT_OVERRIDETIMEOUT))
        {
            sprintf(pszSettings,
                    "%s=%u,%s;",
                    pszSettingFrameLength,
                    getDefaultTimeout(),
                    (getOverrideTimeout())? pszSettingFrameLengthAll : pszSettingFrameLengthUndefined);
            pszSettings = ENDSTRING(pszSettings);
        }

        // -----------------

        if (getActivateOnLoad() != DEFAULT_ACTIVATEONLOAD)
        {
            sprintf(pszSettings,
                    "%s=%s;",
                    pszSettingActivateOnLoad,
                    (getActivateOnLoad())? pszSettingActivateOnLoadYes : pszSettingActivateOnLoadNo);
            pszSettings = ENDSTRING(pszSettings);
        }

        // -----------------

        if (strcmp(getAnimationPath(), DEFAULT_ANIMATIONPATH) != 0)
        {
            sprintf(pszSettings,
                    "%s=%s;",
                    pszSettingAnimationPath,
                    getAnimationPath());
            pszSettings = ENDSTRING(pszSettings);
        }

        // -----------------

        if (getAnimationInitDelay() != getDefaultAnimationInitDelay())
        {
            sprintf(pszSettings,
                    "%s=%u;",
                    pszSettingAnimationInitDelay,
                    getAnimationInitDelay());
            pszSettings = ENDSTRING(pszSettings);
        }

        // -----------------

        for (i = 0, ppl = QueryPointerlist(0); i < NUM_OF_SYSCURSORS; i++, ppl++)
        {
            if (strlen(ppl->szAnimationName) > 0)
            {
                sprintf(pszSettings,
                        "%s=%u,%s,%s;",
                        pszSettingPointer,
                        i,
                        ppl->szAnimationName,
                (ppl->fAnimate) ? pszSettingPointerOn : pszSettingPointerOff);
                pszSettings = ENDSTRING(pszSettings);
            }
        }

        // hand over result
        *ppszSettings = realloc(pszBuffer, strlen(pszBuffer) + 1);

    }
    while (FALSE);

// aufrÑumen

    return rc;

}
