
/*
 *@@sourcefile mptrldr.c:
 *
 *      This file is ALL new with V0.9.4.
 *
 *@@added V0.9.4 [umoeller]
 *@@header "pointers\mptrldr.h"
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
#define INCL_DOS
#define INCL_DOSSEMAPHORES
#include <os2.h>

// generic headers
#include "setup.h"              // code generation and debugging options

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS

#include "pointers\mptrldr.h"
#include "pointers\wmuser.h"
#include "pointers\macros.h"
#include "pointers\debug.h"
#include "pointers\wmuser.h"
#include "pointers\mptrcnr.h"
#include "pointers\mptrutil.h"
#include "pointers\mptrppl.h"
#include "pointers\mptrprop.h"          // V0.9.3 (2000-05-21) [umoeller]
#include "pointers\mptrset.h"
#include "pointers\mptrlset.h"          // V0.9.3 (2000-05-21) [umoeller]

#include "..\..\001\dll\r_amptr001.h"

#define SEM_LOADER_ACTIVE                     "\\SEM32\\WPAMPTR\\ACTIVE"

typedef struct _WINDOWDATA
{
    PHANDLERDATA phd;
    HMTX hmtxActive;
    PSZ pszResource;
    HMODULE hmodResource;
}
WINDOWDATA, *PWINDOWDATA;


static HWND G_hwndException = NULLHANDLE;
static HWND G_hwndHelp = NULLHANDLE;

static PFNWP G_pfnwpFrameProc;

static BOOL G_fMinimize = FALSE;

// Daten fr Settingseingabe
static HWND G_hwndEntryfield = NULLHANDLE;

#define  ID_ENTRYFIELD  512
static HWND G_hwndPushbutton = NULLHANDLE;

#define  ID_PUSHBUTTON  513

#define TABTEXT_SIZEX                 100
#define TABTEXT_SIZEY                 30
#define PAGEBUTTON_SIZEXY             21

#define NOTEBOOK_SIZEX               600
#define NOTEBOOK_SIZEY               600

// Prototypes nur fr dieses Modul
HWND CreateHelpInstance(HAB hab, HWND hwndToAssociate, HLIB hlibResource,
                      USHORT usHelpTableID, PSZ pszHelpTitle, PSZ pszHelpLib);

// Prototypes
ULONG _System SignalHandler(PEXCEPTIONREPORTRECORD pERepRec, PEXCEPTIONREGISTRATIONRECORD pERegRec,
                            PCONTEXTRECORD pCtxRec, PVOID p);
MRESULT EXPENTRY LoaderFrameWindowProc(HWND, ULONG, MPARAM, MPARAM);
MRESULT EXPENTRY Page1DlgWindowProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);

// dummy methoden fr Datensicherung
BOOL _wpSaveImmediate(PVOID * somSelf);
BOOL _wpRestoreState(PVOID * somSelf);
BOOL _wpSaveString(PVOID somSelf, PSZ pszClass, ULONG ulKey, PSZ pszString);
BOOL _wpRestoreString(PVOID somSelf, PSZ pszClass, ULONG ulKey, PSZ pszString, PULONG pulMaxlen);
BOOL _wpSaveLong(PVOID somSelf, PSZ pszClass, ULONG ulKey, ULONG ulValue);
BOOL _wpRestoreLong(PVOID somSelf, PSZ pszClass, ULONG ulKey, PULONG pulValue);
BOOL _wpDisplayHelp(PVOID somSelf, ULONG ulHelpPanelId, PSZ pHelpLibrary);
BOOL _wpMenuItemSelected(PVOID somSelf, HWND hwndFrame, ULONG ulMenuId);
BOOL _wpclsFindOneObject(PVOID somSelf, HWND hwndFrame, PSZ pszSettings);
BOOL _wpQueryFilename(PVOID somSelf, PSZ pszFilename, BOOL fQualified);
PSZ _clsQueryHelpLibrary(PVOID * somSelf);
HMODULE _clsQueryModuleHandle(PVOID self);

// dummy SOM Typen
typedef PVOID WPObject;

// dummy instance und class pointer
#define _WPAnimatedMousePointer NULL

// dummy SOM instance Daten
static PSZ G_pszCurrentSettings = NULL;
static PRECORDCORE G_pcnrrec = NULL;
static PVOID G_somSelf = NULL;
static HMODULE G_hmodResource = NULLHANDLE;

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : main                                                       ³
 *³ Kommentar :                                                            ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 24.02.1995                                                 ³
 *³ Žnderung  : 24.02.1995                                                 ³
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

    EXCEPTIONREGISTRATIONRECORD xcpthand =
    {0, &SignalHandler};

    APIRET rc, rc1;
    HAB hab;
    HMQ hmq;
    QMSG qmsg;

    WINDOWDATA wd;

    FRAMECDATA fcd;

    HMODULE hmodResource = NULLHANDLE;  // uses exe resources

    HWND hwndFrame = NULLHANDLE;
    HWND hwndNotebook = NULLHANDLE;
    HWND hwndPage1 = NULLHANDLE;
    HWND hwndPage2 = NULLHANDLE;
    CHAR szTabName[MAX_RES_STRLEN];

// exception handler setzen
    DosSetExceptionHandler(&xcpthand);


    do
    {
        // PM-Ressourcen holen
        // (ohne PM geht gar nichts !!!)

        hab = WinInitialize(0);
        if (hab == NULL)
            break;
        hmq = WinCreateMsgQueue(hab, 0);

        // zum Testen: den Mauszeiger zum Vorschein bringen
        {
            ULONG ulPointerLevel = WinQuerySysValue(HWND_DESKTOP, SV_POINTERLEVEL);
            ULONG i;

            // show the pointer again
            for (i = 0; i < ulPointerLevel; i++)
            {
                WinShowPointer(HWND_DESKTOP, TRUE);
            }
        }


        // Semaphore fr Zugriff auf gemeinsame Daten erstellen
        rc = DosCreateMutexSem(SEM_LOADER_ACTIVE,
                               &wd.hmtxActive,
                               0,   // shared !
                                FALSE);     // not owned

        if (rc != NO_ERROR)
        {
            WinMessageBox(HWND_DESKTOP, HWND_DESKTOP,
                          "The program is already active and will be aborted.",
                          __TITLE__, 0, MB_CANCEL | MB_ERROR | MB_MOVEABLE);
            break;
        }

        // Variablen initialisieren
        memset(&wd, 0, sizeof(WINDOWDATA));
        if (argc > 1)
            wd.pszResource = argv[1];

        // resource lib laden
        rc = LoadResourceLib(&wd.hmodResource);
        if (rc != NO_ERROR)
            break;
        G_hmodResource = wd.hmodResource;
        hmodResource = wd.hmodResource;

#ifdef BETA
        rc = CheckExpiration(wd.hmodResource);
#endif

        // create frame win
        fcd.cb = sizeof(FRAMECDATA);
        fcd.flCreateFlags = FCF_TITLEBAR |
            FCF_SYSMENU |
            FCF_DLGBORDER |
            FCF_SHELLPOSITION;

        fcd.hmodResources = 0;
        fcd.idResources = 0;

        // create frame
        hwndFrame = WinCreateWindow(HWND_DESKTOP,
                                    WC_FRAME,
                                    __TITLE__,
                                    FS_MOUSEALIGN,
                                    0, 0, 0, 0,
                                    HWND_DESKTOP,
                                    HWND_TOP,
                                    IDDLG_DLG_ANIMATEDWAITPOINTER,
                                    &fcd,
                                    NULL);

        if (hwndFrame == NULLHANDLE)
        {
            rc = ERRORIDERROR(WinGetLastError(hab));
            break;
        }

        // subclass frame window
        G_pfnwpFrameProc = WinSubclassWindow(hwndFrame,
                                           (PFNWP) LoaderFrameWindowProc);

        // create notebook
        hwndNotebook = WinCreateWindow(hwndFrame,
                                       WC_NOTEBOOK,
                                       NULL,
                                       BKS_BACKPAGESBR | BKS_MAJORTABRIGHT,
                                       0, 0, 0, 0,
                                       hwndFrame,
                                       HWND_BOTTOM,
                                       FID_CLIENT,
                                       NULL,
                                       NULL);

        if (hwndNotebook == NULLHANDLE)
        {
            rc = ERRORIDERROR(WinGetLastError(hab));
            break;
        }

        // setup notebook pages and save initial sizes
        hwndPage1 = CreateNotebookPage(hwndNotebook,
                                       wd.hmodResource,
                                       IDDLG_DLG_ANIMATEDWAITPOINTER_230,
                                       IDTAB_NBPAGE,
                                       0,
                                       Page1DlgWindowProc,
                                       &wd);
        if (hwndPage1 == NULLHANDLE)
        {
            rc = ERRORIDERROR(WinGetLastError(hab));
            break;
        }

        // setup the notebook
        SETUPNOTEBOOK(hwndNotebook, TABTEXT_SIZEX, TABTEXT_SIZEY, PAGEBUTTON_SIZEXY);

        // Window size setzen
        WinSetWindowPos(hwndFrame, HWND_TOP, 10, 10, NOTEBOOK_SIZEX, NOTEBOOK_SIZEY,
                        SWP_MOVE | SWP_SIZE | SWP_SHOW | SWP_ZORDER);


        // let it run
        while (WinGetMsg(hab, &qmsg, NULLHANDLE, 0, 0))
        {
            WinDispatchMsg(hab, &qmsg);
        }

// // Dialog ausfhren
        // if ( WinDlgBox( HWND_DESKTOP, HWND_DESKTOP,
        //                 &Page1DlgWindowProc,
        //                 wd.hmodResource,
        //                 IDDLG_DLG_ANIMATEDWAITPOINTER_230,
        //                 &wd) == DID_ERROR)
        //    {
        //    rc = ERRORIDERROR(WinGetLastError( hab));
        //    }

    }
    while (FALSE);


#ifndef DEBUG
    if (wd.hmtxActive)
    {
        rc1 = DosReleaseMutexSem(wd.hmtxActive);
        rc1 = DosCloseMutexSem(wd.hmtxActive);
    }
#endif

// PM Ressourcen abgeben

    if (hmq)
        WinDestroyMsgQueue(hmq);
    if (hab)
        WinTerminate(hab);


// exception handler deregistrieren
    DosUnsetExceptionHandler(&xcpthand);

    return rc;

}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : Page1DlgWindowProc                                         ³
 *³ Kommentar : Window-Procedure des Programms                             ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 17.03.1995                                                 ³
 *³ Žnderung  : 17.03.1995                                                 ³
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

MRESULT EXPENTRY Page1DlgWindowProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    // APIRET rc;

    static PWINDOWDATA pwd = NULL;
    static HSWITCH hswitch = NULLHANDLE;

    switch (msg)
    {
        case WM_INITDLG:
            {
                PFNWP pfnwpOriginal;
                HANDLERDATA hd;
                FRAMECDATA framecdata;
                SWCNTRL swctlSwitchData;
                TID tid;
                PSZ pszTitle = __TITLE__;
                CHAR szHelpLibrary[_MAX_PATH];

                // Zeiger auf Pointerliste speichern
                pwd = (PWINDOWDATA) mp2;
                WinSetWindowPtr(hwnd, QWL_USER, pwd);

                // Zielfenster fr Exception sichern
                G_hwndException = hwnd;

                // Tasklisteneintrag erstellen
                memset(&swctlSwitchData, 0, sizeof(SWCNTRL));
                swctlSwitchData.hwnd = hwnd;
                swctlSwitchData.uchVisibility = SWL_VISIBLE;
                swctlSwitchData.fbJump = SWL_JUMPABLE;
                strcpy(swctlSwitchData.szSwtitle, pszTitle);
                WinQueryWindowProcess(hwnd, &swctlSwitchData.idProcess, &tid);
                hswitch = WinCreateSwitchEntry(WinQueryAnchorBlock(hwnd), &swctlSwitchData);

//    // zus„tzliche Framecontrols erstellen
                //    memset( &framecdata, 0, sizeof( FRAMECDATA));
                //    framecdata.cb            = sizeof( FRAMECDATA);
                //    framecdata.flCreateFlags = FCF_SYSMENU | FCF_TITLEBAR | FCF_MINBUTTON;
                //    framecdata.hmodResources = pwd->hmodResource;
                //    framecdata.idResources   = IDDLG_DLG_ANIMATEDWAITPOINTER;
                //    WinCreateFrameControls( hwnd,  &framecdata, pszTitle);
                //    WinSendMsg( hwnd, WM_UPDATEFRAME, framecdata.flCreateFlags, 0);

//    // dnne Border verwenden
                //    WinSetWindowBits( hwnd, QWL_STYLE, FS_BORDER, FS_BORDER);

                // Entryfield erzeugen
                G_hwndEntryfield = WinCreateWindow(hwnd, WC_ENTRYFIELD, "",
                             ES_AUTOSCROLL | ES_MARGIN | ES_LEFT | WS_VISIBLE,
                 10, 403, 300, 24, hwnd, HWND_TOP, ID_ENTRYFIELD, NULL, NULL);
                WinSendMsg(G_hwndEntryfield, EM_SETTEXTLIMIT, MPFROMLONG(_MAX_PATH), 0);

                // Pushbutton erzeugen
                G_hwndPushbutton = WinCreateWindow(hwnd, WC_BUTTON, "~Set",
                                   BS_PUSHBUTTON | BS_SYSCOMMAND | WS_VISIBLE,
                 320, 400, 60, 32, hwnd, HWND_TOP, ID_PUSHBUTTON, NULL, NULL);
                WinSendMsg(G_hwndPushbutton, BM_SETDEFAULT, MPFROMLONG(TRUE), 0);

                // Hilfe-Instanz anlegen und assoziieren
                /* rc = */ GetHelpLibName(szHelpLibrary, sizeof(szHelpLibrary));
                // create help instance
                G_hwndHelp = CreateHelpInstance(WinQueryAnchorBlock(hwnd),
                                                hwnd,
                                                (HLIB) 0,
                                                1, // IDPNL_MAIN,     // was: 1
                                                "?!?!?", // __TITLE__ " " __VERSION__,
                                                szHelpLibrary);

                // wpRestoreState Aufruf durch wpInit simulieren
                _wpRestoreState(NULL);
                WinPostMsg(hwnd, WM_USER_DELAYEDINIT, 0, 0);

                // ein Undo holt die gespeicherten Werte
                //    WinPostMsg(hwnd, WM_COMMAND, MPFROM2SHORT(IDDLG_PB_UNDO, 0), 0L);

// ### integrated code begin

                // auf SubcommandHandler subclassen
                pfnwpOriginal = WinSubclassWindow(hwnd, DialogHandlerProc);

                // ausserdem der Subclass-Funktion auch WM_INITDLG zukommen lassen
                // Pointer auf HANDLERDATA wird an erster Stelle in WINDOWDATA bergeben
                if ((pwd->phd = malloc(sizeof(HANDLERDATA))) != 0)
                {
                    memset(pwd->phd, 0, sizeof(HANDLERDATA));
                    pwd->phd->hmodResource = pwd->hmodResource;
                    pwd->phd->somSelf = G_somSelf;
                    pwd->phd->pfnwpOriginal = pfnwpOriginal;
                    pwd->phd->ppcnrrec = &G_pcnrrec;
                }
                return DialogHandlerProc(hwnd, msg, 0, 0);

// ### integrated code end

            }
            // break;

        case WM_USER_DELAYEDINIT:
            ScanSetupString(hwnd, G_pcnrrec, G_pszCurrentSettings, FALSE, TRUE);
            break;

        case WM_SYSCOMMAND:
            if (LONGFROMMP(mp1) == ID_PUSHBUTTON)
            {
                CHAR szSettings[_MAX_PATH];

                DLGQUERYSTRING(hwnd, ID_ENTRYFIELD, szSettings);
                ScanSetupString(hwnd, G_pcnrrec, szSettings, TRUE, FALSE);
                WinSetFocus(HWND_DESKTOP, G_hwndEntryfield);
            }
            break;

        case WM_DESTROY:
            WinDestroyWindow(G_hwndHelp);
            break;

// ### integrated code begin
#define SETTINGS_LOADSET      "CLASSLIST=WPFolder;STARTFOLDER=%s;SUBTREESRCH=NO;DEFAULTCRITERIA=YES;"
#define SETTINGS_FINDPOINTER  "CLASSLIST=WPPointer;STARTFOLDER=%s;SUBTREESRCH=NO;DEFAULTCRITERIA=YES;"

        case WM_USER_SERVICE:
            {
                switch (LONGFROMMP(mp1))
                {
                    case SERVICE_RESTORE:
                        DEBUGMSG("SERVICE: restore" NEWLINE, 0);
//          DEBUGMSG( "SERVICE: %s" NEWLINE, _pszCurrentSettings);
                        ScanSetupString(hwnd, PVOIDFROMMP(mp2), G_pszCurrentSettings, TRUE, TRUE);
                        break;  // SERVICE_RESTORE

                    case SERVICE_SAVE:
                        DEBUGMSG("SERVICE: save" NEWLINE, 0);
                        _wpSaveImmediate(G_somSelf);
                        break;  // SERVICE_SAVE

                    case SERVICE_HELP:
                        {
                            ULONG ulPanelId = LONGFROMMP(mp2);
                            PSZ pszHelpLibrary = _clsQueryHelpLibrary(_WPAnimatedMousePointer);

                            DEBUGMSG("SERVICE: help %u in %s" NEWLINE, ulPanelId _c_ pszHelpLibrary);
                            _wpDisplayHelp(G_somSelf, ulPanelId, pszHelpLibrary);
                        }
                        break;  // SERVICE_HELP

                    case SERVICE_HELPMENU:
                        {
                            ULONG ulMenuId = LONGFROMMP(mp2);
                            BOOL fResult;
                            APIRET rc;

                            fResult = _wpMenuItemSelected(G_somSelf, hwnd, ulMenuId);
                            rc = (fResult) ? 0 : ERRORIDERROR(WinGetLastError(WinQueryAnchorBlock(hwnd)));
                            DEBUGMSG("SERVICE: menu help %u (result=%u rc=%u/0x%08x)" NEWLINE, ulMenuId _c_ fResult _c_ rc _c_ rc);

                        }
                        break;  // SERVICE_HELP

                    case SERVICE_FIND:
                        {
                            APIRET rc;
                            CHAR szFullname[_MAX_PATH];

                            DEBUGMSG("SERVICE: Find Pointer" NEWLINE, 0);
                            rc = FindFiles(_WPAnimatedMousePointer,
                                           G_somSelf,
                                           HWND_DESKTOP,
                                           hwnd,
                                           _clsQueryModuleHandle(_WPAnimatedMousePointer),
                             (mp2) ? PVOIDFROMMP(mp2) : DEFAULT_ANIMATIONPATH,
                                       szFullname, sizeof(szFullname), FALSE);
                            if (rc == NO_ERROR)
                                return strdup(szFullname);
                            else
                                return NULL;

                        }
                        // break;  // SERVICE_FIND

                    case SERVICE_LOAD:
                        {
                            APIRET rc;
                            CHAR szFullname[_MAX_PATH];

                            DEBUGMSG("SERVICE: Load Set" NEWLINE, 0);

                            rc = FindFiles(NULL,        // m_wpobject
                                           NULL,    // added V0.9.3 (2000-05-21) [umoeller]
                                           HWND_DESKTOP, // hwndParent
                                           hwnd,    // hwndOwner
                                           _clsQueryModuleHandle(_WPAnimatedMousePointer),
                                                    // hmodResource
                                           (mp2)    // pszDirectory
                                                ? PVOIDFROMMP(mp2)
                                                : DEFAULT_ANIMATIONPATH,
                                            szFullname, // pszFullName
                                            sizeof(szFullname), // ulMaxLen
                                            TRUE); // fFindSet
                            if (rc != NO_ERROR)
                                return strdup(szFullname);
                            else
                                return NULL;

                        }
                        // break;  // SERVICE_LOAD

                }               // switch (SHORT1FROMMP(mp1))

            }
            break;              // end case WM_USER_SERVICE:

// ### integrated code end

    }                           // endswitch

    return (WinDefDlgProc(hwnd, msg, mp1, mp2));
}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : LoaderFrameWindowProc                                      ³
 *³ Comment   : subclass of frame dialog proc for handling SC_CLOSE        ³
 *³ Author    : C.Langanke                                                 ³
 *³ Date      : 07.02.1998                                                 ³
 *³ Update    : 07.02.1998                                                 ³
 *³ called by : diverse                                                    ³
 *³ calls     : -                                                          ³
 *³ Input     : HWND, ULONG, MPARAM, MPARAM                                ³
 *³ Tasks     : - handle pm messages for notebook frame                    ³
 *³ returns   : MRESULT - result of message handling                       ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

MRESULT EXPENTRY LoaderFrameWindowProc
 (
     HWND hwnd,
     ULONG msg,
     MPARAM mp1,
     MPARAM mp2
)

{


    switch (msg)
    {
        case WM_SYSCOMMAND:

            switch (SHORT1FROMMP(mp1))
            {

                case SC_CLOSE:  // is close allowed ? check data of all notebook pages !

                    break;

            }                   // switch (SHORT2FROMMP( mp1))

            break;

    }

    return G_pfnwpFrameProc(hwnd, msg, mp1, mp2);
}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : SignalHandler                                              ³
 *³ Kommentar : Exception Handler                                          ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 27.02.1995                                                 ³
 *³ Žnderung  : 27.02.1995                                                 ³
 *³ aufgerufen: diverse                                                    ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : -                                                          ³
 *³ Aufgaben  : - signal exceptions abfangen und Flag setzen               ³
 *³ Rckgabe  : -                                                          ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

ULONG _System SignalHandler
 (
     PEXCEPTIONREPORTRECORD pERepRec,
     PEXCEPTIONREGISTRATIONRECORD pERegRec,
     PCONTEXTRECORD pCtxRec,
     PVOID p
)
{

// im Debug-Modus Exception nicht abfangen !
    if (pERepRec->ExceptionNum == XCPT_SIGNAL)
    {
        WinPostMsg(G_hwndException, WM_QUIT, 0L, 0L);
        return (XCPT_CONTINUE_EXECUTION);
    }
    else
        return (XCPT_CONTINUE_SEARCH);
}

/* ------------------------------------------------- */
/* ------------- dummy SOM methods ----------------- */
/* ------------------------------------------------- */

BOOL _wpSaveImmediate(PVOID * somSelf)
{
    APIRET rc = NO_ERROR;
    PSZ pszSettings = NULL;
    ULONG ulMaxLen = 0;

    do
    {
        // Settings holen
        rc = QueryCurrentSettings(&pszSettings);
        if (rc != NO_ERROR)
            break;

        // alte Settings verwerfen
        if (G_pszCurrentSettings != NULL)
            free(G_pszCurrentSettings);
        G_pszCurrentSettings = pszSettings;

        // jetzt speichern
        _wpSaveString(somSelf, "WPAnimatedMousePointer", 1, pszSettings);

        if (getAnimationInitDelay() != getDefaultAnimationInitDelay())
            _wpSaveLong(somSelf, "WPAnimatedMousePointer", 2, getAnimationInitDelay());
        else
            _wpSaveLong(somSelf, "WPAnimatedMousePointer", 2, -1);

    }
    while (FALSE);

    return TRUE;
}

/* ------------------------------------------------- */

BOOL _wpRestoreState(PVOID * somSelf)
{

    APIRET rc = NO_ERROR;
    PSZ pszSettings = NULL;
    ULONG ulMaxLen = 0;
    ULONG ulAnimationInitDelay = 0;

    do
    {
        // numerischen Wert fr Animtion In*it delay holen
        if ((!_wpRestoreLong(somSelf, "WPAnimatedMousePointer", 2, &ulAnimationInitDelay)) ||
            (ulAnimationInitDelay == -1))
            ulAnimationInitDelay = getDefaultAnimationInitDelay();
        setAnimationInitDelay(ulAnimationInitDelay);

        // ben”tigte L„nge abfragen
        if (!_wpRestoreString(somSelf, "WPAnimatedMousePointer", 1, NULL, &ulMaxLen))
            break;

        if (ulMaxLen == 0)
            break;

        // Speicher holen
        if ((pszSettings = malloc(ulMaxLen)) == NULL)
        {
            rc = ERROR_NOT_ENOUGH_MEMORY;
            break;
        }

        // Settings holen
        _wpRestoreString(somSelf, "WPAnimatedMousePointer", 1, pszSettings, &ulMaxLen);

        // alte Settings verwerfen
        if (G_pszCurrentSettings != NULL)
            free(G_pszCurrentSettings);
        G_pszCurrentSettings = pszSettings;


    }
    while (FALSE);

    return TRUE;
}

/* ------------------------------------------------- */

BOOL _wpSaveString(PVOID somSelf, PSZ pszClass, ULONG ulKey, PSZ pszString)
{
    CHAR szKey[9];

    _ltoa(ulKey, szKey, 10);

    return PrfWriteProfileString(HINI_PROFILE, pszClass, szKey, pszString);
}

/* ------------------------------------------------- */

BOOL _wpRestoreString(PVOID somSelf, PSZ pszClass, ULONG ulKey, PSZ pszString, PULONG pulMaxlen)
{
    CHAR szKey[9];
    ULONG ulLen, ulStoredLen;
    BOOL fOverflow;

    _ltoa(ulKey, szKey, 10);

// Paramter prfen
    if (pulMaxlen == NULL)
        return FALSE;

// ggfs. nur L„nge zurckgeben
    if (pszString == NULL)
    {
        return PrfQueryProfileSize(HINI_PROFILE, pszClass, szKey, pulMaxlen);
    }

// String holen
    *pulMaxlen = PrfQueryProfileString(HINI_PROFILE, pszClass, szKey, NULL, pszString, *pulMaxlen);

// String gefunden ?
    return (*pulMaxlen != 0);
}


/* ------------------------------------------------- */
BOOL _wpSaveLong(PVOID somSelf, PSZ pszClass, ULONG ulKey, ULONG ulValue)
{
    CHAR szKey[9];

    _ltoa(ulKey, szKey, 10);

    return PrfWriteProfileData(HINI_PROFILE, pszClass, szKey, &ulValue, sizeof(ULONG));

}


/* ------------------------------------------------- */

BOOL _wpRestoreLong(PVOID somSelf, PSZ pszClass, ULONG ulKey, PULONG pulValue)
{

    ULONG ulDataLen = 0;
    CHAR szKey[9];

    _ltoa(ulKey, szKey, 10);

// Paramter prfen
    if (pulValue == NULL)
        return FALSE;

    if ((!PrfQueryProfileSize(HINI_PROFILE, pszClass, szKey, &ulDataLen)) ||
        (ulDataLen != sizeof(ULONG)))
        return FALSE;

    return PrfQueryProfileData(HINI_PROFILE, pszClass, szKey, pulValue, &ulDataLen);
}

/* ------------------------------------------------- */
BOOL _wpDisplayHelp(PVOID somSelf, ULONG ulHelpPanelId, PSZ pHelpLibrary)
{
    WinSendMsg(G_hwndHelp, HM_DISPLAY_HELP, (MPARAM) ulHelpPanelId, (MPARAM) HM_RESOURCEID);
    return FALSE;
}

/* ------------------------------------------------- */
BOOL _wpMenuItemSelected(PVOID somSelf, HWND hwndFrame, ULONG ulMenuId)
{
    return FALSE;
}


/* ------------------------------------------------- */
BOOL _wpclsFindOneObject(PVOID somSelf, HWND hwndFrame, PSZ pszSettings)
{
    return NULL;
}

/* ------------------------------------------------- */
BOOL _wpQueryFilename(PVOID somSelf, PSZ pszFilename, BOOL fQualified)
{
    return NULL;
}

/* ------------------------------------------------- */
PSZ _clsQueryHelpLibrary(PVOID * somSelf)
{
    return "";
}

/* ------------------------------------------------- */
HMODULE _clsQueryModuleHandle(PVOID self)
{
    return G_hmodResource;
}

/* ------------------------------------------------- */
/* BOOL DisplayHelp(PVOID somSelf, ULONG ulHelpPanelId)

{
    WinSendMsg(G_hwndHelp, HM_DISPLAY_HELP, (MPARAM) ulHelpPanelId, (MPARAM) HM_RESOURCEID);
    return FALSE;
}       this is never used, and wpamptr.c has a function of the same name
        V0.9.4 (2000-07-22) [umoeller]

*/


/* ------------------------------------------------- */
/* ------------- Ersatzfunktionen  ----------------- */
/* ------------------------------------------------- */


/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : CreateHelpInstance                                         ³
 *³ Comment   :                                                            ³
 *³ Author    : C.Langanke                                                 ³
 *³ Date      : 10.12.1995                                                 ³
 *³ Update    : 10.12.1995                                                 ³
 *³ called by : diverse                                                    ³
 *³ calls     : -                                                          ³
 *³ Input     : HMODULE - handle of resource lib                           ³
 *³             USHORT  - ID of help table                                 ³
 *³             PSZ     - titel of help panel                              ³
 *³             PSZ     - name of help library                             ³
 *³ Tasks     : - create and associate help instance                       ³
 *³ returns   : HWND    - window handle of help instance                   ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */

HWND CreateHelpInstance(HAB hab,
                        HWND hwndToAssociate,
                        HLIB hlibResource,
                        USHORT usHelpTableID,
                        PSZ pszHelpTitle,
                        PSZ pszHelpLib)
{
    HELPINIT hinit;
    HWND hwndHelp = NULLHANDLE;
    BOOL fSuccess = TRUE;


    do
    {
        // setup init data and create help instance
        hinit.cb = sizeof(HELPINIT);
        hinit.ulReturnCode = 0L;
        hinit.pszTutorialName = NULL;
        hinit.phtHelpTable = (PHELPTABLE) (0xFFFF0000 | usHelpTableID);
        hinit.hmodHelpTableModule = hlibResource;
        hinit.hmodAccelActionBarModule = (HMODULE) NULL;
        hinit.idAccelTable = 0;
        hinit.idActionBar = 0;
        hinit.pszHelpWindowTitle = pszHelpTitle;
        hinit.fShowPanelId = CMIC_HIDE_PANEL_ID;
        hinit.pszHelpLibraryName = pszHelpLib;
        hwndHelp = WinCreateHelpInstance(hab, &hinit);

        // check for creation error
        fSuccess = (hinit.ulReturnCode == NO_ERROR);
        if (!fSuccess)
            break;

        // associate help instance to window
        fSuccess = WinAssociateHelpInstance(hwndHelp, hwndToAssociate);
        if (!fSuccess)
            break;

    }
    while (FALSE);

// handle errors
    if (!fSuccess)
    {
        WinDestroyHelpInstance(hwndHelp);
        hwndHelp = NULLHANDLE;
    }

// done ...
    return hwndHelp;
}
