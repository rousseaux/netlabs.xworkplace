
/*
 *@@sourcefile mptrpagl.c:
 *
 *      This file is ALL new with V0.9.4.
 *
 *@@added V0.9.4 [umoeller]
 *@@header "pointers\mptrpagl.h"
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

#include "pointers\mptrfile.h"
#include "pointers\mptrpag1.h"
#include "pointers\mptrptr.h"
#include "pointers\mptrprop.h"
#include "pointers\mptrutil.h"
#include "pointers\mptrcnr.h"
#include "pointers\mptredit.h"
#include "pointers\mptrset.h"
#include "pointers\wmuser.h"
#include "pointers\macros.h"
#include "pointers\debug.h"

#include "..\..\001\dll\r_amptr001.h"

#define WPMENUID_HELP               2
#define WPMENUID_HELP_FOR_HELP      600
#define WPMENUID_HELPKEYS           603
#define WPMENUID_HELPINDEX          604

// DialogHandlerproc in *.h

typedef struct _HANDLERDATAINTERNAL
{
    PRECORDCORE pcnrrec;
    PRECORDCORE precEdit;
    HWND hwndMenuFolder;
    HWND hwndMenuItem;
    PVOID pvEmphasis;
    BOOL fEmphasisSet;
    CHAR szDragSource[_MAX_PATH];
    CHAR szDragTarget[_MAX_PATH];
}
HANDLERDATAINTERNAL, *PHANDLERDATAINTERNAL;

// Struktur fr Zugriff auf Window Daten des Dialog Subclass Handlers
// festgelegt ist nur, daá die ersten 4 Byte der Pointer auf HANDLERDATA sind
typedef struct _WINDOWDATA
{
    PHANDLERDATA phd;
}
WINDOWDATA, *PWINDOWDATA;


// alphabetic order of filetype strings
static ULONG aulFiletypeSorted = 1;


/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : DialogHandlerProc                                          ³
 *³ Kommentar : Window-SubProcedure zum Kapseln der Anmiationsressourcen   ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 28.07.1996                                                 ³
 *³ Žnderung  : 28.07.1996                                                 ³
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
MRESULT EXPENTRY _Export DialogHandlerProc
 (
     HWND hwnd,
     ULONG msg,
     MPARAM mp1,
     MPARAM mp2
)
{


    PWINDOWDATA pwd = WinQueryWindowPtr(hwnd, QWL_USER);
    PHANDLERDATA phd = (pwd) ? pwd->phd : NULL;
    PHANDLERDATAINTERNAL phdi = (phd) ? phd->pvReserved : NULL;

    PFNWP pfnwpOriginal = (phd) ? phd->pfnwpOriginal : WinDefDlgProc;

    switch (msg)
    {

            // ------------------------------------------------------------------------

        case WM_INITDLG:
            {
                HAB hab = WinQueryAnchorBlock(hwnd);
                HWND hwndCnr = WinWindowFromID(hwnd, IDDLG_CN_POINTERSET);
                MENUITEM mi;
                PRECORDCORE pcnrrec = NULL;
                HMODULE hmodResource = (phd) ? phd->hmodResource : NULLHANDLE;

                // Fensterdaten holen
                pwd = WinQueryWindowPtr(hwnd, QWL_USER);
                if (!pwd)
                    break;
                phd = pwd->phd;

                // Struktur fr interne Daten anlegen
                if ((phdi = malloc(sizeof(PHANDLERDATAINTERNAL))) == NULL)
                    break;
                phd->pvReserved = phdi;
                memset(phdi, 0, sizeof(PHANDLERDATAINTERNAL));

                // Wenn keine SOM-Klasse, dann Pointerlist initialisieren
                if (phd->somSelf == NULL)
                    InitializePointerlist(WinQueryAnchorBlock(hwnd), phd->hmodResource);

                // container initialisieren
                InitPtrSetContainer(hwnd, &pcnrrec);
                phdi->pcnrrec = pcnrrec;
                if (phd->ppcnrrec)
                    *(phd->ppcnrrec) = pcnrrec;

                // Load context menu for later use
                phdi->hwndMenuFolder = WinLoadMenu(hwndCnr, phd->hmodResource, IDMEN_FOLDER);
                phdi->hwndMenuItem = WinLoadMenu(hwndCnr, phd->hmodResource, IDMEN_ITEM);

                // Meneintr„ge ggfs fr WARP 3 ab„ndern
                if (IsWARP3())
                {
                    CHAR szMenuEntry[MAX_RES_STRLEN];

                    // Name fr WARP 3 Version laden
                    LOADSTRING(IDMEN_FOLDER_SETTINGS_230, szMenuEntry);

                    // Text setzen
                    WinSendMsg(phdi->hwndMenuFolder, MM_SETITEMTEXT, MPFROMSHORT(IDMEN_FOLDER_SETTINGS), szMenuEntry);
                }

                // make submenus cascaded
                if (WinSendMsg(phdi->hwndMenuFolder, MM_QUERYITEM, MPFROM2SHORT(IDMEN_FOLDER_VIEW, TRUE), &mi))
                    if (WinSetWindowBits(mi.hwndSubMenu, QWL_STYLE, MS_CONDITIONALCASCADE, MS_CONDITIONALCASCADE))
                        WinSendMsg(mi.hwndSubMenu, MM_SETDEFAULTITEMID, MPFROMSHORT(IDMEN_FOLDER_VIEW_ICON), NULL);
                if (WinSendMsg(phdi->hwndMenuFolder, MM_QUERYITEM, MPFROM2SHORT(IDMEN_FOLDER_HELP, TRUE), &mi))
                    if (WinSetWindowBits(mi.hwndSubMenu, QWL_STYLE, MS_CONDITIONALCASCADE, MS_CONDITIONALCASCADE))
                        WinSendMsg(mi.hwndSubMenu, MM_SETDEFAULTITEMID, MPFROMSHORT(IDMEN_FOLDER_HELP_GENERAL), NULL);
                if (WinSendMsg(phdi->hwndMenuItem, MM_QUERYITEM, MPFROM2SHORT(IDMEN_ITEM_HELP, TRUE), &mi))
                    if (WinSetWindowBits(mi.hwndSubMenu, QWL_STYLE, MS_CONDITIONALCASCADE, MS_CONDITIONALCASCADE))
                        WinSendMsg(mi.hwndSubMenu, MM_SETDEFAULTITEMID, MPFROMSHORT(IDMEN_ITEM_HELP_GENERAL), NULL);

                // initialisierung testen
                WinStartTimer(WinQueryAnchorBlock(hwnd), hwnd, INIT_TIMER_ID, INIT_TIMER_TIMEOUT);

                // Focus wird ver„ndert
                return (MRESULT) TRUE;
            }
            // break;              // case WM_INITDLG

            // ------------------------------------------------------------------------

        case WM_USER_ENABLECONTAINER:
            {
                BOOL fEnable = LONGFROMMP(mp1);

                WinEnableWindow(WinWindowFromID(hwnd, IDDLG_CN_POINTERSET), fEnable);
                if (fEnable)
                    WinSendMsg(hwnd, WM_USER_ENABLECONTROLS, 0, 0);
                else
                    WinEnableWindow(WinWindowFromID(hwnd, IDDLG_PB_EDIT), fEnable);

                WinEnableWindow(WinWindowFromID(hwnd, IDDLG_PB_FIND), fEnable);
                WinEnableWindow(WinWindowFromID(hwnd, IDDLG_PB_LOAD), fEnable);
                WinEnableWindow(WinWindowFromID(hwnd, IDDLG_PB_UNDO), fEnable);
                WinEnableWindow(WinWindowFromID(hwnd, IDDLG_PB_DEFAULT), fEnable);
                WinEnableWindow(WinWindowFromID(hwnd, IDDLG_PB_HELP), fEnable);
            }
            break;

            // ------------------------------------------------------------------------

        case WM_TIMER:
            switch (LONGFROMMP(mp1))
            {
                case DEMO_TIMER_ID:
                    WinStopTimer(WinQueryAnchorBlock(hwnd), hwnd, DEMO_TIMER_ID);
                    ProceedWithDemo(hwnd, phdi->pcnrrec, TRUE, FALSE);
                    WinStartTimer(WinQueryAnchorBlock(hwnd),
                                  hwnd,
                                  DEMO_TIMER_ID,
                                  getDefaultTimeout());
                    break;

                case INIT_TIMER_ID:
                    if (IsSettingsInitialized())
                    {
                        WinSendMsg(hwnd,
                                   WM_USER_ENABLECONTAINER,
                                   (MPARAM)TRUE,
                                   0);
                        WinStopTimer(WinQueryAnchorBlock(hwnd), hwnd, INIT_TIMER_ID);
                    }
                    else
                        WinSendMsg(hwnd, WM_USER_ENABLECONTAINER, FALSE, 0);
                    break;
            }

            break;              // end case WM_TIMER

            // ------------------------------------------------------------------------

        case WM_COMMAND:

            switch (SHORT1FROMMP(mp1))
            {
                    // - - - - - - - - - - - - - - - - - - - - -
                case IDMEN_ITEM_EDIT:
                case IDDLG_PB_EDIT:
                    {
                        HWND hwndCnrPointerSet = WinWindowFromID(hwnd, IDDLG_CN_POINTERSET);
                        PRECORDCORE precSelected;
                        PMYRECORDCORE pmyrec;
                        BOOL fEditDone = FALSE;
                        PPOINTERLIST ppl;

                        // wird gerade editiert oder ist es ein Set ?
                        if (!QueryEditPending())
                        {
                            // selektieres Item ermitteln
                            if (SHORT1FROMMP(mp1) == IDMEN_ITEM_EDIT)
                                precSelected = phdi->pvEmphasis;
                            else
                            {
                                // selektieres Item ermitteln
                                precSelected = WinSendMsg(hwndCnrPointerSet, CM_QUERYRECORDEMPHASIS,
                                    phdi->pcnrrec, MPFROMSHORT(CRA_SELECTED));
                            }

                            if (    (precSelected == (PRECORDCORE)-1)
                                 || (precSelected == 0)
                               )
                                precSelected = phdi->pcnrrec;

//                if ((precSelected) && (!QueryItemSet( precSelected)))
                            if (precSelected)
                            {
                                // pointerliste holen
                                pmyrec = (PMYRECORDCORE) precSelected;
                                ppl = QueryPointerlist(pmyrec->ulPointerIndex);

                                // starten
                                if (BeginEditPointer(hwnd, phd->hmodResource, ppl))
                                {
                                    // record speichern
                                    phdi->precEdit = precSelected;

                                    // Pushbutton disablen
                                    WinEnableWindow(WinWindowFromID(hwnd, IDDLG_PB_EDIT), FALSE);
                                    // container item in use markieren
                                    WinSendMsg(hwndCnrPointerSet, CM_SETRECORDEMPHASIS,
                                               MPFROMP(precSelected), MPFROM2SHORT(TRUE, CRA_INUSE));

                                    fEditDone = TRUE;
                                }

                            }

                        }       // end if (!QueryEditPending())

                        if (!fEditDone)
                            WinAlarm(HWND_DESKTOP, WA_WARNING);

                    }
                    break;      // end case IDMEN_ITEM_EDIT IDDLG_PB_EDIT

                    // - - - - - - - - - - - - - - - - - - - - -

                case IDMEN_ITEM_FIND:
                case IDDLG_PB_FIND:
                    {
                        HWND hwndCnrPointerSet = WinWindowFromID(hwnd, IDDLG_CN_POINTERSET);
                        PRECORDCORE precSelected;
                        PSZ pszResource;

                        DEBUGMSG("COMMAND: find" NEWLINE, 0);

                        // selektieres Item ermitteln
                        if (SHORT1FROMMP(mp1) == IDMEN_ITEM_FIND)
                            precSelected = phdi->pvEmphasis;
                        else
                        {
                            // selektieres Item ermitteln
                            precSelected = WinSendMsg(hwndCnrPointerSet,
                                                     CM_QUERYRECORDEMPHASIS,
                                                     (MPARAM)phdi->pcnrrec,
                                                     MPFROMSHORT(CRA_SELECTED));
                        }

                        if (    (precSelected == (PRECORDCORE)-1)
                             || (precSelected == 0))
                            precSelected = phdi->pcnrrec;

                        pszResource = WinSendMsg(hwnd,
                                                 WM_USER_SERVICE,
                                                 MPFROMLONG(SERVICE_FIND),
                                                 (MPARAM)getAnimationPath());
                        if (pszResource)
                            // asynchron verarbeiten !
                            WinPostMsg(hwnd, WM_USER_LOADRESOURCE, MPFROMP(pszResource), MPFROMP(precSelected));
                    }
                    break;

                    // - - - - - - - - - - - - - - - - - - - - -

                case IDDLG_PB_DEFAULT:
                    DEBUGMSG("COMMAND: default" NEWLINE, 0);
                    // asynchron verarbeiten !
                    WinPostMsg(hwnd, WM_USER_LOADRESOURCE, 0L, 0L);
                    break;

                    // - - - - - - - - - - - - - - - - - - - - -

                case IDDLG_PB_UNDO:
                    DEBUGMSG("COMMAND: undo" NEWLINE, 0);
                    WinSendMsg(hwnd,
                               WM_USER_SERVICE,
                               MPFROMLONG(SERVICE_RESTORE),
                               MPFROMP(phdi->pcnrrec));
                    break;

                    // - - - - - - - - - - - - - - - - - - - - -

                case IDMEN_FOLDER_FIND:
                case IDDLG_PB_LOAD:
                    {
                        PSZ pszResource;

                        DEBUGMSG("COMMAND: load" NEWLINE, 0);
                        pszResource = WinSendMsg(hwnd,
                                                 WM_USER_SERVICE,
                                                 MPFROMLONG(SERVICE_LOAD),
                                                 (MPARAM)getAnimationPath());
                        if (pszResource)
                            // asynchron verarbeiten !
                            WinPostMsg(hwnd,
                                       WM_USER_LOADRESOURCE,
                                       MPFROMP(pszResource),
                                       0);
                    }
                    break;

                    // - - - - - - - - - - - - - - - - - - - - -

                case IDMEN_FOLDER_VIEW_ICON:
                    SetContainerView(hwnd, CV_ICON);
                    break;

                    // - - - - - - - - - - - - - - - - - - - - -

                case IDMEN_FOLDER_VIEW_DETAIL:
                    SetContainerView(hwnd, CV_DETAIL);
                    break;

                    // - - - - - - - - - - - - - - - - - - - - -

                case IDMEN_ITEM_DEFAULT:
                case IDMEN_FOLDER_DEFAULT:
                    // asynchron verarbeiten
                    WinPostMsg(hwnd, WM_USER_LOADRESOURCE,
                               0L,
                               MPFROMP(phdi->pvEmphasis));
                    break;

                    // - - - - - - - - - - - - - - - - - - - - -

                case IDMEN_FOLDER_SAVEAS:
                case IDMEN_ITEM_SAVEAS:

                    {
                        BOOL fSuccess = FALSE;
                        BOOL fSaveAllPointers = (phdi->pvEmphasis == NULL);
                        PMYRECORDCORE pmyrec = (PMYRECORDCORE) phdi->pvEmphasis;
                        PPOINTERLIST ppl;

                        ULONG ulPointerIndex;
                        ULONG ulTargetFileType;
                        CHAR szFileName[_MAX_PATH];

                        // what do we have to save ?
                        if (fSaveAllPointers)
                            ulPointerIndex = 0;
                        else
                            ulPointerIndex = pmyrec->ulPointerIndex;

                        // determine target file type
                        ulTargetFileType = (fSaveAllPointers) ?
                            getDragSetType() :
                            getDragPtrType();

                        // get ptr to POINTERLIST
                        ppl = QueryPointerlist(ulPointerIndex);

                        // get initial name fior file selection
                        strcpy(szFileName, QueryPointerName(ulPointerIndex));
                        strcat(szFileName, QueryResFileExtension(ulTargetFileType));
                        strlwr(szFileName);

                        if (!SelectFile(hwnd, FDS_SAVEAS_DIALOG,
                                        szFileName, &ulTargetFileType,
                                        szFileName, sizeof(szFileName)))
                            break;

                        // write the file
                        fSuccess = WriteTargetFiles(szFileName, ulTargetFileType,
                                                    ppl, fSaveAllPointers);
                        if (!fSuccess)
                            WinAlarm(HWND_DESKTOP, WA_ERROR);
                    }
                    break;      // case IDMEN_FOLDER_SAVEAS:
                    // case IDMEN_ITEM_SAVEAS:

                    // - - - - - - - - - - - - - - - - - - - - -

                case IDMEN_FOLDER_DEMO:
                    ToggleDemo(hwnd, phdi->pcnrrec, TRUE, NULL);
                    break;

                    // - - - - - - - - - - - - - - - - - - - - -

                case IDMEN_ITEM_ANIMATE:
                case IDMEN_FOLDER_ANIMATE:
                    // asynchron verarbeiten
                    ToggleAnimate(hwnd, 0, phdi->pvEmphasis, phdi->pcnrrec, (phdi->pvEmphasis == NULL), TRUE, NULL);
                    break;

                    // - - - - - - - - - - - - - - - - - - - - -

                case IDMEN_FOLDER_HIDEPOINTER:
                    setHidePointer(!getHidePointer());
                    break;

                    // - - - - - - - - - - - - - - - - - - - - -

                case IDMEN_FOLDER_BLACKWHITE:
                    setBlackWhite(!getBlackWhite());
                    break;

                    // - - - - - - - - - - - - - - - - - - - - -

                case IDMEN_ITEM_HELP_INDEX:
                case IDMEN_FOLDER_HELP_INDEX:
                    WinSendMsg(hwnd, WM_USER_SERVICE, MPFROMLONG(SERVICE_HELPMENU), MPFROMP(WPMENUID_HELPINDEX));
                    break;

                    // - - - - - - - - - - - - - - - - - - - - -

                case IDMEN_FOLDER_HELP_GENERAL:
                case IDMEN_ITEM_HELP_GENERAL:
                case IDDLG_PB_HELP:
                    WinSendMsg(hwnd, WM_USER_SERVICE, MPFROMLONG(SERVICE_HELP),
                                    MPFROMP(1 /* IDPNL_USAGE_NBPAGE */));
                    break;

                    // - - - - - - - - - - - - - - - - - - - - -

                case IDMEN_FOLDER_HELP_USING:
                case IDMEN_ITEM_HELP_USING:
                    WinSendMsg(hwnd, WM_USER_SERVICE, MPFROMLONG(SERVICE_HELPMENU), MPFROMP(WPMENUID_HELP_FOR_HELP));
                    break;

                    // - - - - - - - - - - - - - - - - - - - - -

                case IDMEN_FOLDER_HELP_KEYS:
                case IDMEN_ITEM_HELP_KEYS:
                    WinSendMsg(hwnd, WM_USER_SERVICE, MPFROMLONG(SERVICE_HELPMENU), MPFROMP(WPMENUID_HELPKEYS));
                    break;

                    // - - - - - - - - - - - - - - - - - - - - -

                case IDMEN_ITEM_HELP_ABOUT:
                case IDMEN_FOLDER_HELP_ABOUT:
                    // Dialog ausfhren
                    WinDlgBox(HWND_DESKTOP, hwnd,
                    &WinDefDlgProc, phd->hmodResource, IDDLG_DLG_ABOUT, NULL);
                    break;

                    // - - - - - - - - - - - - - - - - - - - - -

                case IDMEN_FOLDER_SETTINGS:
                    {
                        BOOL fFramelengthChanged = FALSE;
                        SETTINGSHANDLERDATA shd;

                        // Settings handler Daten initialisieren
                        memset(&shd, 0, sizeof(shd));
                        shd.hmodResource = (phd) ? phd->hmodResource : NULLHANDLE;
                        shd.somSelf = (phd) ? phd->somSelf : NULL;

                        ExecPropertiesNotebook(hwnd, shd.hmodResource, &shd);

                        // change hide pointer
                        if (shd.fToggleHidePointer)
                            WinPostMsg(hwnd, WM_COMMAND, MPFROMLONG(IDMEN_FOLDER_HIDEPOINTER), 0);

                        // change hide pointer delay
                        // if (shd.fHidePointerDelayChanged)
                        //    WinPostMsg( hwnd, WM_USER_HIDEPOINTERDELAY_CHANGED, 0, 0);

                        // do a refresh
                        if (shd.fRefreshView)
                            RefreshCnrItem(hwnd, NULL, phdi->pcnrrec, FALSE);
                    }
                    break;

                    // - - - - - - - - - - - - - - - - - - - - -

                case DID_OK:

                    WinDismissDlg(hwnd, 0L);
                    break;

                    // - - - - - - - - - - - - - - - - - - - - -

                default:
                    break;
            }                   // end switch (SHORT1FROMMP(mp1))

            // Msg-Queue nicht beenden !
            return (MRESULT) TRUE;
            // break;

            // break;              // case WM_COMMAND

            // ------------------------------------------------------------------------

        case WM_APPTERMINATENOTIFY:
            {
                HWND hwndCnrPointerSet = WinWindowFromID(hwnd, IDDLG_CN_POINTERSET);
                PMYRECORDCORE pmyrec = (PMYRECORDCORE) phdi->precEdit;
                PPOINTERLIST ppl = QueryPointerlist(pmyrec->ulPointerIndex);

                if (EndEditPointer(hwnd, LONGFROMMP(mp1), LONGFROMMP(mp2), ppl))
                {
                    // Edit Pushbutton wieder enablen
                    WinSendMsg(hwnd, WM_USER_ENABLECONTROLS, 0, 0);
                    // container item in use Markierung l”schen
                    if (phdi->precEdit)
                        WinSendMsg(hwndCnrPointerSet, CM_SETRECORDEMPHASIS,
                                   MPFROMP(phdi->precEdit), MPFROM2SHORT(FALSE, CRA_INUSE));

                    // Container item refreshen
                    RefreshCnrItem(hwnd,
                                   (PRECORDCORE)pmyrec,
                                   phdi->pcnrrec,
                                   TRUE);
                }
            }
            break;

            // ------------------------------------------------------------------------

        case WM_USER_ENABLECONTROLS:
            {
                HWND hwndCnrPointerSet = WinWindowFromID(hwnd, IDDLG_CN_POINTERSET);
                PRECORDCORE precSelected;
                BOOL fEnable;

                // nichts tun, wenn der Container noch gar nicht sichtbar ist
                if (!WinIsWindowEnabled(hwndCnrPointerSet))
                    break;

                // selektiertes Item ermitteln
                precSelected = WinSendMsg(hwndCnrPointerSet, CM_QUERYRECORDEMPHASIS,
                                    phdi->pcnrrec, MPFROMSHORT(CRA_SELECTED));

                if ((precSelected == (PRECORDCORE)-1) || (precSelected == 0))
                    precSelected = phdi->pcnrrec;

//    fEnable = ((!QueryItemSet( precSelected)) && (!QueryEditPending()));
                fEnable = !QueryEditPending();

                // Pushbutton enablen/disablen
                WinEnableWindow(WinWindowFromID(hwnd, IDDLG_PB_EDIT), fEnable);
            }
            break;

            // ------------------------------------------------------------------------

        case WM_USER_LOADRESOURCE:
            {
                PSZ pszSourceName = PVOIDFROMMP(mp1);
                PRECORDCORE prec = PVOIDFROMMP(mp2);

                // Resource laden
                LoadAnimationResource(hwnd, pszSourceName, prec, phdi->pcnrrec);

                // String wieder freigeben
                free(pszSourceName);

            }
            break;

            // ------------------------------------------------------------------------

        case WM_MENUEND:
            {
                HWND hwndCnrPointerSet = WinWindowFromID(hwnd, IDDLG_CN_POINTERSET);

                if (phdi->fEmphasisSet)
                {
                    // remove emphasis
                    phdi->fEmphasisSet = !WinSendMsg(hwndCnrPointerSet, CM_SETRECORDEMPHASIS,
                                                     MPFROMP(phdi->pvEmphasis),
                                             MPFROM2SHORT(FALSE, CRA_SOURCE));
                }
            }
            break;

            // ------------------------------------------------------------------------

        case WM_CONTROL:

            if (SHORT1FROMMP(mp1) == IDDLG_CN_POINTERSET)
            {
                switch (SHORT2FROMMP(mp1))
                {

                        // ...............................................................

                    case CN_HELP:
                        WinSendMsg(hwnd, WM_USER_SERVICE, MPFROMLONG(SERVICE_HELP),
                                        MPFROMP(1 /* IDPNL_USAGE_NBPAGE */));
                        break;  // case  CN_HELP

                        // ...............................................................

                    case CN_ENTER:
                        {
                            PNOTIFYRECORDENTER pnre = PVOIDFROMMP(mp2);

                            if (pnre->pRecord != NULL)
                                WinPostMsg(hwnd, WM_COMMAND, MPFROMLONG(IDDLG_PB_EDIT),
                                       MPFROM2SHORT(CMDSRC_PUSHBUTTON, TRUE));
                        }
                        break;  // case  CN_ENTER

                        // ...............................................................

                    case CN_EMPHASIS:
                        {
                            PNOTIFYRECORDEMPHASIS pnre = PVOIDFROMMP(mp2);
                            PMYRECORDCORE pmyrec;

                            // wurde etwas selektiert ?
                            // dann je nach item pushbuttons ("Edit usw.")
                            // aktivieren/deaktivieren
                            if ((pnre->fEmphasisMask & CRA_SELECTED) > 0)
                                WinSendMsg(hwnd, WM_USER_ENABLECONTROLS, 0, 0);
                        }
                        break;  // case CN_EMPHASIS

                        // ...............................................................

                    case CN_CONTEXTMENU:
                        {
                            POINTL pointl;
                            HWND hwndCnr = WinWindowFromID(hwnd, IDDLG_CN_POINTERSET);
                            BOOL fItemSelected = (PVOIDFROMMP(mp2) != NULL);
                            BOOL fIsSet;
                            BOOL fAnimate;
                            BOOL fSaveable;
                            BOOL fDemoActive, fDemoEnabled;
                            ULONG ulViewStyle;

                            // set emphasis
                            phdi->pvEmphasis = mp2;
                            phdi->fEmphasisSet = (BOOL)WinSendMsg(hwndCnr,
                                                            CM_SETRECORDEMPHASIS,
                                                            MPFROMP(phdi->pvEmphasis),
                                                            MPFROM2SHORT(TRUE, CRA_SOURCE));
                            fIsSet = QueryItemSet(phdi->pvEmphasis);

                            // abfragen, ob ein Set geladen ist
                            // - Menitem "Animate" aktivieren/deaktivieren
                            WinSendMsg((fItemSelected) ? phdi->hwndMenuItem : phdi->hwndMenuFolder,
                                       MM_SETITEMATTR,
                                       MPFROM2SHORT((fItemSelected) ? IDMEN_ITEM_ANIMATE : IDMEN_FOLDER_ANIMATE, FALSE),
                                       MPFROM2SHORT(MIA_DISABLED, fIsSet ? ~MIA_DISABLED : MIA_DISABLED));

                            // - Menitem "Animate" Check setzen/entfernen
                            fAnimate = fIsSet ? QueryItemAnimate(phdi->pvEmphasis) : FALSE;
                            WinSendMsg((fItemSelected) ? phdi->hwndMenuItem : phdi->hwndMenuFolder,
                                       MM_SETITEMATTR,
                                       MPFROM2SHORT((fItemSelected) ? IDMEN_ITEM_ANIMATE : IDMEN_FOLDER_ANIMATE, FALSE),
                                       MPFROM2SHORT(MIA_CHECKED, fAnimate ? MIA_CHECKED : ~MIA_CHECKED));

                            // - Menuitem "Save As..." aktivieren/deaktivieren
                            fSaveable = QueryItemLoaded(phdi->pvEmphasis);
                            WinSendMsg((fItemSelected) ? phdi->hwndMenuItem : phdi->hwndMenuFolder,
                                       MM_SETITEMATTR,
                                       MPFROM2SHORT((fItemSelected) ? IDMEN_ITEM_SAVEAS : IDMEN_FOLDER_SAVEAS, FALSE),
                                       MPFROM2SHORT(MIA_DISABLED, fAnimate ? ~MIA_DISABLED : MIA_DISABLED));

                            // Men fr den Container
                            if (!fItemSelected)
                            {
                                // Situation bestimmen
                                QueryContainerView(hwnd, &ulViewStyle);
                                fDemoActive = (fIsSet) ? QueryDemo() : FALSE;
                                fDemoEnabled = (ulViewStyle & CV_DETAIL) ? FALSE : fIsSet;

                                // - Menitem "Hide Pointer" Check setzen/entfernen
                                WinSendMsg(phdi->hwndMenuFolder,
                                           MM_SETITEMATTR,
                                MPFROM2SHORT(IDMEN_FOLDER_HIDEPOINTER, FALSE),
                                           MPFROM2SHORT(MIA_CHECKED, (getHidePointer())? MIA_CHECKED : ~MIA_CHECKED));

                                // - Menitem "Black&White" Check setzen/entfernen
                                WinSendMsg(phdi->hwndMenuFolder,
                                           MM_SETITEMATTR,
                                 MPFROM2SHORT(IDMEN_FOLDER_BLACKWHITE, FALSE),
                                           MPFROM2SHORT(MIA_CHECKED, (getBlackWhite())? MIA_CHECKED : ~MIA_CHECKED));

                                // - Menitem "Demo" Check setzen/entfernen
                                WinSendMsg(phdi->hwndMenuFolder,
                                           MM_SETITEMATTR,
                                       MPFROM2SHORT(IDMEN_FOLDER_DEMO, FALSE),
                                           MPFROM2SHORT(MIA_CHECKED, (fDemoActive) ? MIA_CHECKED : ~MIA_CHECKED));

                                // - Menitem "Demo" Check aktivieren/deaktivieren
                                WinSendMsg(phdi->hwndMenuFolder,
                                           MM_SETITEMATTR,
                                       MPFROM2SHORT(IDMEN_FOLDER_DEMO, FALSE),
                                           MPFROM2SHORT(MIA_DISABLED, (fDemoEnabled) ? ~MIA_DISABLED : MIA_DISABLED));
                            }
                            // Men fr ein Item
                            else
                            {
                                // - Menitem "Edit" aktivieren/deaktivieren
                                WinSendMsg(phdi->hwndMenuItem,
                                           MM_SETITEMATTR,
                                         MPFROM2SHORT(IDMEN_ITEM_EDIT, FALSE),
//                            MPFROM2SHORT(MIA_DISABLED, ((!fIsSet) && (!QueryEditPending())) ? ~MIA_DISABLED : MIA_DISABLED));
                                           MPFROM2SHORT(MIA_DISABLED, (!QueryEditPending())? ~MIA_DISABLED : MIA_DISABLED));
                            }

                            // Pop up the menu
                            WinQueryPointerPos(HWND_DESKTOP, &pointl);
                            WinPopupMenu(HWND_DESKTOP,
                                         hwnd,
                                         (fItemSelected) ? phdi->hwndMenuItem : phdi->hwndMenuFolder,
                                         (SHORT) pointl.x,
                                         (SHORT) pointl.y,
                                         0,
                                         PU_HCONSTRAIN | PU_VCONSTRAIN |
                                         PU_MOUSEBUTTON1 | PU_MOUSEBUTTON2 |
                                         PU_MOUSEBUTTON3 | PU_KEYBOARD);

                        }
                        break;  // case  CN_CONTEXTMENU

                        // ...............................................................

                    case CN_DROP:
                        {
                            PCNRDRAGINFO pcnrdraginfo = PVOIDFROMMP(mp2);
                            PDRAGINFO pdraginfo = pcnrdraginfo->pDragInfo;
                            PDRAGITEM pdragitem;
                            CHAR szFullSourceName[_MAX_PATH];

                            // Zugriff holen
                            if (DrgAccessDraginfo(pdraginfo))
                            {
                                do
                                {
                                    // Drag Item holen
                                    pdragitem = DrgQueryDragitemPtr(pdraginfo, 0);
                                    if (pdragitem == NULL)
                                        break;

                                    // Verzeichnis holen
                                    if (DrgQueryStrName(pdragitem->hstrContainerName,
                                                     sizeof(szFullSourceName),
                                                        szFullSourceName) == 0)
                                        break;

                                    // Dateiname holen
                                    if (DrgQueryStrName(pdragitem->hstrSourceName,
                                                        sizeof(szFullSourceName) - strlen(szFullSourceName),
                                                        &szFullSourceName[strlen(szFullSourceName)]) == 0)
                                        break;

                                    // asynchron verarbeiten
                                    WinPostMsg(hwnd, WM_USER_LOADRESOURCE,
                                            MPFROMP(strdup(szFullSourceName)),
                                               MPFROMP(pcnrdraginfo->pRecord));

                                }
                                while (FALSE);

                                // Memory wieder freigeben
                                DrgFreeDraginfo(pdraginfo);

                            }   // end if (DrgAccessDraginfo( pdraginfo))

                        }
                        break;  // case CN_CONTEXTMENU

                        // ...............................................................

                    case CN_DRAGOVER:
                        {
                            PCNRDRAGINFO pcnrdraginfo = PVOIDFROMMP(mp2);
                            PDRAGINFO pdraginfo = pcnrdraginfo->pDragInfo;
                            ULONG ulDropCode = DOR_NEVERDROP;
                            PDRAGITEM pdragitem;
                            CHAR szFullSourceName[_MAX_PATH];
                            ULONG ulFileType;
                            BOOL fIsOS2File;

                            // Zugriff holen
                            if (DrgAccessDraginfo(pdraginfo))
                            {
                                do
                                {
                                    // mehr als ein einzelnes Objekt
                                    // ist nicht erlaubt
                                    if (pdraginfo->cditem != 1)
                                        break;

                                    // Drag Item holen
                                    pdragitem = DrgQueryDragitemPtr(pdraginfo, 0);
                                    if (pdragitem == NULL)
                                        break;

                                    // nur Dateien sind gltig
                                    fIsOS2File = DrgVerifyRMF(pdragitem, "DRM_OS2FILE", NULL);
                                    if (!fIsOS2File)
                                        break;

                                    // Verzeichnis holen
                                    if (DrgQueryStrName(pdragitem->hstrContainerName,
                                                     sizeof(szFullSourceName),
                                                        szFullSourceName) == 0)
                                        break;

                                    // Dateiname holen
                                    if (DrgQueryStrName(pdragitem->hstrSourceName,
                                                        sizeof(szFullSourceName) - strlen(szFullSourceName),
                                                        &szFullSourceName[strlen(szFullSourceName)]) == 0)
                                        break;

                                    // Ressource-Typ bestimmen
                                    if (!QueryResFileType(szFullSourceName,
                                                          &ulFileType,
                                                          szFullSourceName,
                                                    sizeof(szFullSourceName)))
                                        break;


                                    // Drag erlauben oder verbieten
                                    switch (ulFileType)
                                    {
                                            // fr Einzeldateien kein Drag auf Container zulassen
                                        case RESFILETYPE_POINTER:
                                        case RESFILETYPE_CURSOR:
                                        case RESFILETYPE_WINANIMATION:
                                            ulDropCode = (pcnrdraginfo->pRecord) ? DOR_DROP : DOR_NODROP;
                                            break;

                                        default:
                                            ulDropCode = DOR_DROP;
                                            break;
                                    }   // end switch (ulFileType)

                                }
                                while (FALSE);

                                // Memory wieder freigeben
                                DrgFreeDraginfo(pdraginfo);

                            }   // end if (DrgAccessDraginfo( pdraginfo))

                            return MRFROM2SHORT(ulDropCode, DO_COPY);
                        }

                        // break;  // case CN_DRAGOVER

                        // ...............................................................

                    case CN_INITDRAG:
                        {
                            PCNRDRAGINIT pcnrdraginit = PVOIDFROMMP(mp2);
                            PMYRECORDCORE pmyrec = (PMYRECORDCORE)pcnrdraginit->pRecord;
                            HWND hwndCnr = WinWindowFromID(hwnd, IDDLG_CN_POINTERSET);

                            CHAR szSourceFile[20];

                            PSZ apszSourceFiles[1];
                            PSZ apszTargetFiles[1];
                            PSZ apszTypes[1];

                            PPOINTERLIST ppl;
                            PPOINTERLIST pplArrow = QueryPointerlist(0);
                            ULONG i;
                            HPOINTER hptrDrag = NULLHANDLE;
                            BOOL fAnimated = QueryAnimate(0, FALSE);


                            // we do not use real filenames here, just placeholders,
                            // because we render ourselves later anyway

                            if (pmyrec == NULL)
                            {
                                // save animations for all pointers
                                strcpy(szSourceFile, "ALL");

                                // determine drag image
                                // use first loaded resource
                                for (i = 0, ppl = pplArrow; i < NUM_OF_SYSCURSORS; i++, ppl++)
                                {
                                    if (ppl->hptrStatic)
                                    {
                                        hptrDrag = ppl->hptrStatic;
                                        break;
                                    }
                                }
                            }
                            else
                            {
                                // save one pointer only
                                _ltoa(pmyrec->ulPointerIndex, szSourceFile, 10);

                                // take the static pointer of this item
                                ppl = QueryPointerlist(pmyrec->ulPointerIndex);
                                hptrDrag = ppl->hptrStatic;
                            }

                            // nothing found ? then cancel the drag
                            if (hptrDrag == NULL)
                                return (MRESULT) FALSE;

                            // set up data and initiate drag
                            apszSourceFiles[0] = szSourceFile;
                            apszTargetFiles[0] = NULL;
                            apszTypes[0] = NULL;

                            // set source emphasis for drag operation
                            // (not for container, does mess up with target emphasis)
                            if (pmyrec != NULL)
                                WinSendMsg(hwndCnr, CM_SETRECORDEMPHASIS, MPFROMP(pmyrec), MPFROM2SHORT(TRUE, CRA_SOURCE));

                            // disable animation for arrow pointer
                            // and set default pointer
                            if (fAnimated)
                            {
                                // disable animation
                                EnableAnimation(pplArrow, FALSE);
                                // set default system pointer during drag process
                                WinSetSysPointerData(HWND_DESKTOP, QueryPointerSysId(i), NULL);
                            }

                            // do the drag
                            DrgDragFiles(hwnd,
                                         /* & */ apszSourceFiles, /* & */ apszTypes,
                                         /* & */ apszTargetFiles, 1,
                                         hptrDrag, VK_ENDDRAG,
                                         TRUE, 0);

                            // remove source emphasis
                            if (pmyrec != NULL)
                                WinSendMsg(hwndCnr, CM_SETRECORDEMPHASIS, MPFROMP(pmyrec), MPFROM2SHORT(FALSE, CRA_SOURCE));

                            // reenable animation for arrow pointer
                            if (fAnimated)
                            {
                                // reset to our first pointer image
                                WinSetSysPointerData(HWND_DESKTOP, QueryPointerSysId(i), &pplArrow->iconinfoStatic);
                                // reenable animation
                                EnableAnimation(pplArrow, TRUE);
                            }

                        }
                        // message was processed
                        return (MRESULT) TRUE;
                        // break;  // case CN_INITDRAG

                        // ...............................................................

                }               // end switch (SHORT2FROMMP(mp1))

            }                   // end if (IDDLG_CN_POINTERSET)

            break;              // case WM_CONTROL

            // ------------------------------------------------------------------------

        case DM_RENDERFILE:
            {
                PRENDERFILE prenderfile = PVOIDFROMMP(mp1);
                CHAR szFileName[_MAX_PATH];
                PSZ pszSource;
                PSZ pszTarget;

                // query source: the name
                if (DrgQueryStrName(prenderfile->hstrSource,
                                    sizeof(szFileName),
                                    szFileName) > 0)
                    strcpy(phdi->szDragSource, szFileName);
                else
                    phdi->szDragSource[0] = 0;

                // query target
                if (DrgQueryStrName(prenderfile->hstrTarget,
                                    sizeof(szFileName),
                                    szFileName) > 0)
                    strcpy(phdi->szDragTarget, szFileName);
                else
                    phdi->szDragTarget[0] = 0;

                // create files after completion of conversation
                WinPostMsg(hwnd, WM_USER_CREATEDROPPEDFILES, 0, 0);

                // rendering is done, end drag&drop conversation
                WinSendMsg(prenderfile->hwndDragFiles, DM_FILERENDERED, mp1, MPFROMLONG(TRUE));

            }
            // we did the rendering
            return (MRESULT) TRUE;
            // break;              // case DM_RENDERFILE:

            // ------------------------------------------------------------------------

        case WM_USER_CREATEDROPPEDFILES:
            {
                BOOL fSuccess;
                PSZ pszSource = phdi->szDragSource;
                PSZ pszTarget = phdi->szDragTarget;
                PPOINTERLIST ppl;


                BOOL fSaveAllPointers = FALSE;
                ULONG ulPointerIndex;
                PSZ pszFileNamePart;
                CHAR szFileName[_MAX_PATH];

                ULONG ulTargetFileType;

                do
                {
                    // check parms
                    if ((*pszSource == 0) ||
                        (*pszTarget == 0))
                        break;

                    // what do we have to save ?
                    if (strcmp("ALL", pszSource) == 0)
                    {
                        fSaveAllPointers = TRUE;
                        ulPointerIndex = 0;
                    }
                    else
                        ulPointerIndex = atol(pszSource);


                    // determine target file type
                    ulTargetFileType = (fSaveAllPointers) ?
                        getDragSetType() :
                        getDragPtrType();

                    // get ptr to POINTERLIST
                    ppl = QueryPointerlist(ulPointerIndex);

                    // cut of filename and append real name
                    strcpy(szFileName, pszTarget);
                    pszFileNamePart = Filespec(szFileName, FILESPEC_NAME);
                    *pszFileNamePart = 0;
                    strcat(szFileName, QueryPointerName(ulPointerIndex));
                    strcat(szFileName, QueryResFileExtension(ulTargetFileType));
                    strlwr(szFileName);

                    // write the file
                    fSuccess = WriteTargetFiles(szFileName, ulTargetFileType,
                                                ppl, fSaveAllPointers);

                }
                while (FALSE);

            }
            break;              // case WM_USER_CREATEDROPPEDFILES;

            // ------------------------------------------------------------------------

        case WM_DESTROY:

            // Settings sichern
            WinSendMsg(hwnd, WM_USER_SERVICE, MPFROMLONG(SERVICE_SAVE), 0);

            // destroy menus
            if (phdi->hwndMenuFolder)
                WinDestroyWindow(phdi->hwndMenuFolder);
            if (phdi->hwndMenuItem)
                WinDestroyWindow(phdi->hwndMenuItem);

            // Wenn keine SOM-Klasse, dann Pointerlist deinitialisieren
            if (phd->somSelf == NULL)
                DeinitializePointerlist();


            break;              // case WM_DESTROY

            // ------------------------------------------------------------------------

    }                           // endswitch

    return ((pfnwpOriginal) (hwnd, msg, mp1, mp2));
}
