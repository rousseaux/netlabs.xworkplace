
/*
 *@@sourcefile mptranim.c:
 *
 *      This file is ALL new with V0.9.4.
 *
 *@@added V0.9.4 [umoeller]
 *@@header "pointers\mptranim.h"
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

#include "pointers\title.h"
#include "pointers\mptranim.h"
#include "pointers\mptrset.h"
#include "pointers\mptrppl.h"
#include "pointers\mptrptr.h"
#include "pointers\mptrcnr.h"
#include "pointers\mptrutil.h"      // V0.9.3 (2000-05-21) [umoeller]
#include "pointers\mptrhook.h"
#include "pointers\wmuser.h"
#include "pointers\macros.h"
#include "pointers\debug.h"

// Timer IDs fÅr die 2 Timer Variante
#define ANIMATION_TIMER_ID   256
#define WATCHDOG_TIMER_ID    257
#define WATCHDOG_TIMER_TIMEOUT DEFAULT_ANIMATION_TIMEOUT
#define HIDEPOINTER_TIMER_ID 258

// MOUSEMOVED_TIMER_ID in mptrhook.h definiert

#define ANIMATION_OBJECT_CLASS   "wpamptr_animate"
#define ANIMATION_ACCESS_TIMEOUT 100L

// global data
static HAB habAnimationThread = NULLHANDLE;
static HWND hwndAnimation = NULLHANDLE;

static PFN pfnSetHookData = NULL;
static PFN pfnInputHook = NULL;

static HMODULE hmoduleHook = NULLHANDLE;

// prototypes of this module
APIRET _InitializeHook(HWND hwndNotify);
APIRET _TerminateHook(VOID);


/*⁄ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒø
 *≥ Name      : _LoadHookHook                                              ≥
 *≥ Kommentar : initialisiert den Hook                                     ≥
 *≥ Autor     : C.Langanke                                                 ≥
 *≥ Datum     : 10.10.1996                                                 ≥
 *≥ énderung  : 10.10.1996                                                 ≥
 *≥ aufgerufen: ###                                                        ≥
 *≥ ruft auf  : -                                                          ≥
 *≥ Eingabe   : HAB     - handle anchor block                              ≥
 *≥             HWND    - handle des Fensters, welches Åber Mousemoves     ≥
 *≥                       zu benachrichtigen ist                           ≥
 *≥ Aufgaben  : - Hook installieren                                        ≥
 *≥ RÅckgabe  : APIRET - OS/2 Fehlercode                                   ≥
 *¿ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒŸ
 */

APIRET _LoadHook
 (
     HWND hwndNotify
)
{
    APIRET rc = NO_ERROR;
    BOOL fSuccess;
    HAB hab = WinQueryAnchorBlock(HWND_DESKTOP);

    CHAR szModuleName[_MAX_PATH];
    CHAR szError[20];
    HMODULE hmodule = NULLHANDLE;
    ULONG ulLoaderType;
    PSZ pszDllFileName;

    HOOKDATA hookdata;

//FUNCENTER();

    do
    {
        // check parms
        if (hwndNotify == NULLHANDLE)
        {
            rc = ERROR_INVALID_PARAMETER;
            break;
        }

        if (hmoduleHook != NULLHANDLE)
        {
            rc = ERROR_ACCESS_DENIED;
            break;
        }

        // je nach Loader DLL Dateiname ermitteln
        rc = GetBaseName((PFN)&_LoadHook, szModuleName, sizeof(szModuleName));
        if (rc != NO_ERROR)
            break;
        strcat(szModuleName, "\\" HOOK_DLLNAME ".dll");

        // DLL laden.
        rc = DosLoadModule(szError, sizeof(szError), szModuleName, &hmoduleHook);
        if (rc != NO_ERROR)
            break;

        // Routine fÅr DatenÅbertragung holen
        rc = DosQueryProcAddr(hmoduleHook, 0, HOOK_SETDATAPROC, &pfnSetHookData);
        if (rc != NO_ERROR)
            break;

        // Routine fÅr Msg-Hook holen
        rc = DosQueryProcAddr(hmoduleHook, 0, HOOK_HOOKPROC, &pfnInputHook);
        if (rc != NO_ERROR)
            break;

        // gib ihm Zeit (BUG ???)
        // DosSleep(1000L);

        // Daten Åbertragen
        memset(&hookdata, 0, sizeof(hookdata));
        hookdata.hwndNotify = hwndNotify;
        rc = pfnSetHookData(BLDLEVEL_VERSION, &hookdata);
        if (rc != NO_ERROR)
            break;

        // hook installieren
        fSuccess = WinSetHook(hab, NULLHANDLE, HK_INPUT, pfnInputHook, hmoduleHook);
        if (!fSuccess)
        {
            rc = ERRORIDERROR(WinGetLastError(hab));
            break;
        }

    }
    while (FALSE);

    if (rc != NO_ERROR)
    {
        DosFreeModule(hmoduleHook);
        hmoduleHook = NULLHANDLE;
    }

    return rc;
}

/*⁄ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒø
 *≥ Name      : _UnloadHook                                                ≥
 *≥ Kommentar : deinitialisiert den Hook                                   ≥
 *≥ Autor     : C.Langanke                                                 ≥
 *≥ Datum     : 10.10.1996                                                 ≥
 *≥ énderung  : 10.10.1996                                                 ≥
 *≥ aufgerufen: ###                                                        ≥
 *≥ ruft auf  : -                                                          ≥
 *≥ Eingabe   : HAB     - handle anchor block                              ≥
 *≥ Aufgaben  : - Hook deinstallieren                                      ≥
 *≥ RÅckgabe  : APIRET - OS/2 Fehlercode                                   ≥
 *¿ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒŸ
 */

APIRET _UnloadHook
 (
     BOOL fForce
)
{

    APIRET rc = NO_ERROR;
    BOOL fSuccess;
    HAB hab = WinQueryAnchorBlock(HWND_DESKTOP);
    CHAR szModuleName[_MAX_PATH];
    CHAR szError[20];

    HOOKDATA hookdata;

// FUNCENTER();

    do
    {
        if ((hmoduleHook == NULLHANDLE) && (!fForce))
            break;

        if ((pfnInputHook == NULL) || (hmoduleHook == NULLHANDLE))
        {
            // je nach Loader DLL Dateiname ermitteln
            rc = GetBaseName((PFN)&_LoadHook, szModuleName, sizeof(szModuleName));
            if (rc != NO_ERROR)
                break;
            strcat(szModuleName, "\\" HOOK_DLLNAME ".dll");

            // DLL laden.
            rc = DosLoadModule(szError, sizeof(szError), szModuleName, &hmoduleHook);
            if (rc != NO_ERROR)
                break;

            // Routine fÅr Msg-Hook holen
            rc = DosQueryProcAddr(hmoduleHook, 0, HOOK_HOOKPROC, &pfnInputHook);
            if (rc != NO_ERROR)
                break;
        }

        // hook deinstallieren
        fSuccess = WinReleaseHook(hab, NULLHANDLE, HK_INPUT, pfnInputHook, hmoduleHook);
        if (!fSuccess)
        {
            rc = ERRORIDERROR(WinGetLastError(hab));
            break;
        }

    }
    while (FALSE);

// DLL freigeben
    DosFreeModule(hmoduleHook);
    hmoduleHook = NULLHANDLE;
    return rc;

}

/*⁄ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒø
 *≥ Name      : IsDragPending                                              ≥
 *≥ Kommentar : prÅft, ob Drag&Drop aktiv ist. Lazy Drag wird ignoriert.   ≥
 *≥ Autor     : C.Langanke                                                 ≥
 *≥ Datum     : 10.02.1997                                                 ≥
 *≥ énderung  : 10.02.1997                                                 ≥
 *≥ aufgerufen: diverse                                                    ≥
 *≥ ruft auf  : -                                                          ≥
 *≥ Eingabe   : -                                                          ≥
 *≥ Aufgaben  : - Variable zurÅckgeben                                     ≥
 *≥ RÅckgabe  : BOOL - Drag in Process                                     ≥
 *¿ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒŸ
 */
BOOL IsDragPending(VOID)
{
    BOOL fDragPending = FALSE;

    ULONG ulKeyState = WinGetKeyState(HWND_DESKTOP, VK_BUTTON2);

    if ((ulKeyState & 0x8000) > 0)
        fDragPending = (DrgQueryDragStatus() == DGS_DRAGINPROGRESS);

    return fDragPending;

}

/*⁄ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒø
 *≥ Name      : QueryAnimationHab, QueryAnimationHwnd                      ≥
 *≥ Kommentar : Queryfuntkionen fÅr statische Variablen                    ≥
 *≥ Autor     : C.Langanke                                                 ≥
 *≥ Datum     : 07.06.1996                                                 ≥
 *≥ énderung  : 07.06.1996                                                 ≥
 *≥ aufgerufen: diverse                                                    ≥
 *≥ ruft auf  : -                                                          ≥
 *≥ Eingabe   : -                                                          ≥
 *≥ Aufgaben  : - Variable zurÅckgeben                                     ≥
 *≥ RÅckgabe  : HAB / HWND                                                 ≥
 *¿ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒŸ
 */

HAB QueryAnimationHab(VOID)
{
    return habAnimationThread;
}
HWND QueryAnimationHwnd(VOID)
{
    return hwndAnimation;
}

/*⁄ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒø
 *≥ Name      : AnimationThread                                            ≥
 *≥ Kommentar : Thread-Funktion fÅr den Thread, der die Animation          ≥
 *≥             durchfÅhrt.                                                ≥
 *≥ Autor     : C.Langanke                                                 ≥
 *≥ Datum     : 07.06.1996                                                 ≥
 *≥ énderung  : 07.06.1996                                                 ≥
 *≥ aufgerufen: diverse                                                    ≥
 *≥ ruft auf  : -                                                          ≥
 *≥ Eingabe   : ULONG        - Pointer auf Parameterstruktur               ≥
 *≥ Aufgaben  : - Animation durchfÅhren                                    ≥
 *≥ RÅckgabe  : -                                                          ≥
 *¿ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒŸ
 */

VOID _Optlink AnimationThread(PVOID pvParams) // ULONG ulParms)
{
    HAB hab;
    HMQ hmq;
    APIRET rc;

    ULONG i;

    QMSG qmsg;
    PSZ pszWindowClass = ANIMATION_OBJECT_CLASS;
    ULONG ulMustCompleteNesting = 0;

    BOOL fDragHook = FALSE;
    HMODULE hmodHook;
    PHEV phevStartup = (PHEV)pvParams;

    PSZ pszAnimPriority;
    ULONG ulAnimPriority = PRTYC_FOREGROUNDSERVER;

    do
    {

        // dieser Thread muss sauber beendet werden
        // rc = DosEnterMustComplete( &ulMustCompleteNesting);

        // PrioritÑt setzen
        pszAnimPriority = DEBUGSETTINGVAL(SET_ANIMTHREADPRIORITY);
        if (pszAnimPriority != NULL)
        {
            ulAnimPriority = atol(pszAnimPriority);
            if (ulAnimPriority > PRTYC_FOREGROUNDSERVER)
                ulAnimPriority = PRTYC_FOREGROUNDSERVER;
        }
        SetPriority(ulAnimPriority);

        // PM-Ressourcen holen
        hab = WinInitialize(0);
        if (hab == NULL)
            break;
        hmq = WinCreateMsgQueue(hab, 200);
        if (hmq == NULL)
            break;

// // msg-Queue von Shutdown Processing abtrennen
        // *** nicht durchfÅhren, sonst wird thread einfach gekillt,
        // *** anstatt da· ein WM_CLOSE/WM_QUIT geschickt wird !!!
        // WinCancelShutdown( hmq, TRUE);

        // hab sichern
        habAnimationThread = hab;

        // Register class
        if (!WinRegisterClass(hab, pszWindowClass, ObjectWindowProc, 0, sizeof(ULONG)))
            break;

        // create object window
        hwndAnimation = WinCreateWindow(HWND_OBJECT,
                                        pszWindowClass,
                                        "",
                                        0,
                                        0, 0, 0, 0,
                                        HWND_DESKTOP,
                                        HWND_TOP,
                                        0,
                                        phevStartup,
                                        NULL);

        if (hwndAnimation == NULLHANDLE)
            break;

        // run the object window
        while (WinGetMsg(hab, &qmsg, (HWND) NULL, 0, 0))
            WinDispatchMsg(hab, &qmsg);

        // cleanup
        DeinitializePointerlist();

    }
    while (FALSE);

// PM Ressourcen abgeben
    if (hwndAnimation)
    {
        WinDestroyWindow(hwndAnimation);
        hwndAnimation = NULLHANDLE;
    }

#ifdef USE_HOOK
    if (fDragHook)
        TerminateHook(hab);
#endif

    if (hmq)
    {
        WinDestroyMsgQueue(hmq);
    }

    if (hab)
    {
        WinTerminate(hab);
        hab = NULLHANDLE;
        habAnimationThread = NULLHANDLE;
    }

    DEBUGMSG("info: terminating animation thread" NEWLINE, 0);
// rc = DosExitMustComplete( &ulMustCompleteNesting);

    _endthread();

}


/*⁄ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒø
 *≥ Name      : ObjectWindowProc                                           ≥
 *≥ Kommentar : Window-Procedure des Object Window fÅr die Animation       ≥
 *≥ Autor     : C.Langanke                                                 ≥
 *≥ Datum     : 28.07.1996                                                 ≥
 *≥ énderung  : 28.07.1996                                                 ≥
 *≥ aufgerufen: PM System Message Queue                                    ≥
 *≥ ruft auf  : -                                                          ≥
 *≥ Eingabe   : HWND   - window handle                                     ≥
 *≥             ULONG  - message id                                        ≥
 *≥             MPARAM - message parm 1                                    ≥
 *≥             MPARAM - message parm 2                                    ≥
 *≥ Aufgaben  : - Messages bearbeiten                                      ≥
 *≥ RÅckgabe  : MRESULT - Message Result                                   ≥
 *¿ƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒƒŸ
 */

MRESULT EXPENTRY ObjectWindowProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    // APIRET rc;
    // ULONG i;

    PHEV phevStartup;
    static BOOL fUse9Timer = FALSE;

    switch (msg)
    {

            // ======================================================================================================

        case WM_CREATE:
            // startup sem  posten
            phevStartup = (PHEV) mp1;
            if (phevStartup != NULLHANDLE)
            {
                DosPostEventSem(*phevStartup);
            }

            // Timer Variante ermitteln
            fUse9Timer = DEBUGSETTING(SET_ANIM9TIMER);

            break;


            // ======================================================================================================

        case WM_USER_ENABLEHIDEPOINTER:
            {
                BOOL fEnable = SHORT1FROMMP(mp1);
                HAB hab = WinQueryAnchorBlock(hwnd);
                ULONG ulHidePointerDelay = getHidePointerDelay();
                APIRET rc = NO_ERROR;

                if (fEnable)
                {
                    // enable hook
                    rc = _LoadHook(hwnd);
                    if (rc != NO_ERROR)
                    {
                        // reset flag
                        overrideSetHidePointer(FALSE);
                        break;
                    }

                    // enable timer
                    WinStartTimer(hab,
                                  hwnd,
                                  HIDEPOINTER_TIMER_ID,
                                  ulHidePointerDelay * 1000);
                }
                else
                {
                    // disable timer
                    WinStopTimer(hab,
                                 hwnd,
                                 HIDEPOINTER_TIMER_ID);

                    // unload hook
                    rc = _UnloadHook(FALSE);

                    // make pointer visible again
                    WinSendMsg(hwnd, WM_TIMER, MPFROMLONG(MOUSEMOVED_TIMER_ID), MPFROMLONG(TRUE));
                }

                return (MRESULT) (rc == NO_ERROR);
            }
            // break;



            // ======================================================================================================

        case WM_USER_ENABLEANIMATION:

            {
                PPOINTERLIST ppl = PVOIDFROMMP(mp1);
                BOOL fEnable = SHORT1FROMMP(mp2);
                ULONG ulTimerId = 0;
                ULONG ulTimeout = getDefaultTimeout();
                APIRET rc = NO_ERROR;
                BOOL fSuccess = FALSE;

                // Parameter prÅfen
                if (ppl == NULL)
                    break;


                if (!fUse9Timer)
                {
                    // einfach als "animiert" kennzeichnen
                    ppl->fAnimate = fEnable;
                    fSuccess = TRUE;

                    if (fEnable)
                    {
                        // watchdog timer mit default timeframe value starten
                        WinStartTimer(WinQueryAnchorBlock(hwnd),
                                      hwnd,
                                      WATCHDOG_TIMER_ID,
                                      WATCHDOG_TIMER_TIMEOUT);

                        // eine Dummy Timer Message versenden
                        // um die erste Animation anzuwerfen
                        //          WinSendMsg( hwnd, WM_TIMER, MPFROMLONG( ANIMATION_TIMER_ID), 0);
                        // ersten Timer einstellen
                        ulTimeout = ppl->aulTimer[ppl->ulPtrIndex];
                        if ((ulTimeout == 0) || (getOverrideTimeout()))
                            ulTimeout = getDefaultTimeout();
                        WinStartTimer(WinQueryAnchorBlock(hwnd), hwnd, ANIMATION_TIMER_ID, ulTimeout);

                    }
                    else
                    {
                        // wird nichts mehr animiert ?
                        // dann Watchdog Timer ausschalten
                        if (!QueryAnimate(0, TRUE))
                        {
                            WinStopTimer(WinQueryAnchorBlock(hwnd),
                                         hwnd,
                                         WATCHDOG_TIMER_ID);
                        }

                        // ausserdem Demo und Animation zurÅcksetzen
                        ppl->ulPtrIndex = 0;
                        ppl->ulPtrIndexCnr = 0;
                        ResetAnimation(ppl, (ppl->ulPtrId == SPTR_ARROW));

                    }

                }
                // ------------------------------------------------------------------------------------------------------

                else
                {

                    // timer Id fÅr den betreffenden Mauszeiger ermitteln
                    ulTimerId = (ppl - (QueryPointerlist(0))) + 1;
                    if (ulTimerId > NUM_OF_SYSCURSORS)
                        break;
                    switch (fEnable)
                    {

                        case TRUE:
                            // timeout ermitteln
                            ulTimeout = ppl->aulTimer[ppl->ulPtrIndex];
                            if (ulTimeout == 0)
                                ulTimeout = getDefaultTimeout();

                            // Timer immer erst stoppen, sonst gehen die Resourcen aus
                            WinStopTimer(WinQueryAnchorBlock(hwnd), hwnd, ulTimerId);

                            // timer starten
                            ulTimerId = WinStartTimer(WinQueryAnchorBlock(hwnd),
                                                      hwnd,
                                                      ulTimerId,
                                                      ulTimeout);
                            fSuccess = (ulTimerId != 0);
                            break;

                        case FALSE:
                            // timer stoppen
                            fSuccess = WinStopTimer(WinQueryAnchorBlock(hwnd), hwnd, ulTimerId);

                            // ausserdem Demo und Animation zurÅcksetzen
                            ppl->ulPtrIndex = 0;
                            ppl->ulPtrIndexCnr = 0;
                            ResetAnimation(ppl, (ppl->ulPtrId == SPTR_ARROW));
                            break;

                    }           // end switch

                    // Flag Ñndern
                    if (fSuccess)
                        ppl->fAnimate = fEnable;

                }


                return (MRESULT) fSuccess;
            }
            // break;              // end case WM_USER_ENABLEANIMATION

            // ======================================================================================================

        case WM_TIMER:
            {
                ULONG ulTimerId = LONGFROMMP(mp1);
                ULONG ulPtrId = 0;
                PPOINTERLIST ppl;
                ULONG ulTimeout;
                BOOL fIsActive;
                BOOL fDragPending = FALSE;
                HAB hab = WinQueryAnchorBlock(hwnd);
                APIRET rc = NO_ERROR;

                switch (ulTimerId)
                {

                        // --------------------------------------------------------

                    case HIDEPOINTER_TIMER_ID:
                        {
                            ULONG ulPointerLevel = WinQuerySysValue(HWND_DESKTOP, SV_POINTERLEVEL);

                            // Zeiger verstecken
                            WinShowPointer(HWND_DESKTOP, FALSE);

                            // disable timer
                            WinStopTimer(hab,
                                         hwnd,
                                         HIDEPOINTER_TIMER_ID);
                        }
                        break;

                        // MOUSEMOVED_TIMER_ID  ist kein echter Timer,
                        // sondern wird vom hook gesendet, wenn
                        // WM_MOUSEMOVE Åber den Hook geht
                    case MOUSEMOVED_TIMER_ID:
                        {
                            ULONG ulPointerLevel = WinQuerySysValue(HWND_DESKTOP, SV_POINTERLEVEL);
                            ULONG i;
                            ULONG ulHidePointerDelay = getHidePointerDelay();
                            BOOL fDontRestart = LONGFROMMP(mp2);

                            // show the pointer again
                            for (i = 0; i < ulPointerLevel; i++)
                            {
                                WinShowPointer(HWND_DESKTOP, TRUE);
                            }

                            if (!fDontRestart)
                                // reenable timer
                                WinStartTimer(hab,
                                              hwnd,
                                              HIDEPOINTER_TIMER_ID,
                                              ulHidePointerDelay * 1000);
                        }
                        break;

                        // --------------------------------------------------------

                        // zwei cases fÅr die 2 timer Variante

                    case ANIMATION_TIMER_ID:
                        // Timer anhalten
                        WinStopTimer(WinQueryAnchorBlock(hwnd), hwnd, ANIMATION_TIMER_ID);

                        // Zugriff auf Daten holen
                        // bei Timeout Message verwerfen
                        rc = REQUEST_DATA_ACCESS_TIMEOUT(ANIMATION_ACCESS_TIMEOUT);
                        if (rc == ERROR_TIMEOUT)
                            break;
                        if (rc != NO_ERROR)
                        {
                            RELEASE_DATA_ACCESS();
                            break;
                        }

                        // zuletzt animierten Zeiger ermitteln
                        ulPtrId = WinQueryWindowULong(hwnd, QWL_USER);
                        ppl = QueryPointerlist(ulPtrId);
                        ulPtrId = ppl->ulPtrId;

                        // prÅfen, ob es der Pfeil Zeiger ist
                        // und Mausknopf 2 gedrÅckt ist
                        if (ulPtrId == SPTR_ARROW)
                            fDragPending = IsDragPending();


                        // nÑchsten Pointer setzen
                        if (ppl->fAnimate)
                        {
                            if (!fDragPending)
                                SetNextAnimatedPointer(habAnimationThread, ppl, FALSE);

                            // ggfs. nÑchste Timeout-Zeit setzen
                            {
                                // jetzt neue Zeit einstellen
                                ulTimeout = ppl->aulTimer[ppl->ulPtrIndex];
                                if ((ulTimeout == 0) || (getOverrideTimeout()))
                                    ulTimeout = getDefaultTimeout();
                                WinStartTimer(WinQueryAnchorBlock(hwnd), hwnd, ANIMATION_TIMER_ID, ulTimeout);
                            }
                        }

                        // Zugriff freigeben
                        rc = RELEASE_DATA_ACCESS();

                        break;  // case ANIMATION_TIMER_ID

                    case WATCHDOG_TIMER_ID:
                        // Timer anhalten
                        WinStopTimer(WinQueryAnchorBlock(hwnd), hwnd, WATCHDOG_TIMER_ID);

                        // Zugriff auf Daten holen
                        // bei Timeout Message verwerfen
                        rc = REQUEST_DATA_ACCESS_TIMEOUT(ANIMATION_ACCESS_TIMEOUT);
                        if (rc == ERROR_TIMEOUT)
                            break;
                        if (rc != NO_ERROR)
                        {
                            RELEASE_DATA_ACCESS();
                            break;
                        }

                        // zuletzt animierten Zeiger ermitteln
                        ulPtrId = WinQueryWindowULong(hwnd, QWL_USER);
                        if (ulPtrId > NUM_OF_SYSCURSORS)
                        {
                            fIsActive = FALSE;
                        }
                        else
                        {
                            ppl = QueryPointerlist(ulPtrId);
                            ulPtrId = ppl->ulPtrId;

                            // ist der zuletzt animierte Zeiger nicht mehr aktiv ?
                            fIsActive = IsPointerActive(ulPtrId, NULL);
                        }

                        if (!fIsActive)
                        {
                            ULONG i;

                            // aktuellen Zeiger ermitteln
                            for (i = 0; i < NUM_OF_SYSCURSORS; i++)
                            {
                                ppl = QueryPointerlist(i);
                                ulPtrId = ppl->ulPtrId;

                                fIsActive = IsPointerActive(ulPtrId, NULL);
                                if (fIsActive)
                                {
                                    // und sichern
                                    WinSetWindowULong(hwnd, QWL_USER, i);

                                    // ggfs. nÑchste Timeout-Zeit setzen
                                    if (ppl->fAnimate)
                                    {
                                        // Animation sofort weiterlaufen lassen
                                        WinStartTimer(WinQueryAnchorBlock(hwnd), hwnd, ANIMATION_TIMER_ID, 0);
                                    }
                                    else
                                    {
                                        WinStopTimer(WinQueryAnchorBlock(hwnd), hwnd, ANIMATION_TIMER_ID);
                                    }

                                    // nicht weitersuchen
                                    break;
                                }
                            }   // end for

                        }       // (!fIsActive)

                        // Watchdog Timer mit niedrigster Zeitdauer wieder starten
                        WinStartTimer(WinQueryAnchorBlock(hwnd),
                                      hwnd,
                                      WATCHDOG_TIMER_ID,
                                      WATCHDOG_TIMER_TIMEOUT);

                        // Zugriff freigeben
                        rc = RELEASE_DATA_ACCESS();

                        break;  // case WATCHDOG_TIMER_ID

                        // --------------------------------------------------------

                        // default case fÅr die 9 timer Variante

                    default:
                        if (!fUse9Timer)
                        {
                            DosBeep(1000, 10);  // this may not occur ! (if (!fUse9Timer))

                            break;
                        }

                        // ist es unser Timer ?
                        if (ulTimerId <= NUM_OF_SYSCURSORS)
                        {

                            // Timer anhalten
                            WinStopTimer(WinQueryAnchorBlock(hwnd), hwnd, ulTimerId);

                            // Zugriff auf Daten holen
                            // bei Timeout Message verwerfen
                            rc = REQUEST_DATA_ACCESS_TIMEOUT(ANIMATION_ACCESS_TIMEOUT);
                            if (rc == ERROR_TIMEOUT)
                                break;
                            if (rc != NO_ERROR)
                            {
                                RELEASE_DATA_ACCESS();
                                break;
                            }

                            // Daten ermitteln
                            ppl = QueryPointerlist(ulTimerId - 1);

                            // prÅfen, ob es der Pfeil Zeiger ist
                            // und Mausknopf 2 gedrÅckt ist
                            if (ppl->ulPtrId == SPTR_ARROW)
                                fDragPending = IsDragPending();

                            // nÑchsten Pointer setzen
                            if (!fDragPending)
                                SetNextAnimatedPointer(habAnimationThread, ppl, FALSE);

                            // ggfs. nÑchste Timeout-Zeit setzen
                            if (ppl->fAnimate)
                            {
                                // jetzt neue Zeit einstellen
                                ulTimeout = ppl->aulTimer[ppl->ulPtrIndex];
                                if ((ulTimeout == 0) || (getOverrideTimeout()))
                                    ulTimeout = getDefaultTimeout();
                                WinStartTimer(WinQueryAnchorBlock(hwnd), hwnd, ulTimerId, ulTimeout);
                            }


                            // Zugriff freigeben
                            rc = RELEASE_DATA_ACCESS();

                        }

                        break;  // default

                        // --------------------------------------------------------

                }               // end select

            }

            return (MRESULT) FALSE;
            // break;              // end case WM_TIMER

    }                           // endswitch

    return (WinDefWindowProc(hwnd, msg, mp1, mp2));

}
