
/*
 *@@sourcefile wpamptr.c:
 *
 *      This file is ALL new with V0.9.4.
 *
 *@@added V0.9.4 [umoeller]
 *@@header "pointers\wpamptr.h"
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

/*
 *  Suggested #include order:
 *  1)  os2.h
 *  2)  C library headers
 *  3)  setup.h (code generation and debugging options)
 *  4)  headers in helpers\
 *  5)  at least one SOM implementation header (*.ih)
 *  6)  dlgids.h, headers in shared\ (as needed)
 *  7)  headers in implementation dirs (e.g. filesys\, as needed)
 *  8)  #pragma hdrstop and then more SOM headers which crash with precompiled headers
 */

#define INCL_ERRORS
#define INCL_PM
#define INCL_DOS
#define INCL_DEV
#define INCL_WPCLASS
#define INCL_WINWORKPLACE
#include <os2.h>

// C library headers
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <somobj.h>
#include <setjmp.h>

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\except.h"             // exception handling

// SOM headers which don't crash with prec. header files
#include "xwpmouse.ih"

// XWorkplace implementation headers
#include "shared\common.h"

#include "pointers\macros.h"
#include "pointers\debug.h"
#include "pointers\wmuser.h"
#include "pointers\mptrcnr.h"
#include "pointers\mptrutil.h"
#include "pointers\mptrppl.h"
#include "pointers\mptrset.h"
#include "pointers\mptrlset.h"
#include "pointers\warp4.h"

#include "..\..\001\dll\r_amptr001.h"

#pragma hdrstop
// fÅr wpQueryFileName
#include <wpfsys.h>

// =====================================================================================

typedef struct _WPSWINDOWDATA
{
    PHANDLERDATA phd;
    HMODULE hmodResource;
    SOMAny *somSelf;
    PSZ pszResource;            // analog zum Testprogramm

}
WPSWINDOWDATA, *PWPSWINDOWDATA;


// -------------------------------------------------------------------------------------

MRESULT EXPENTRY AnimatedMousePointerPageProc(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;
    BOOL    fCallDefault = TRUE;

    TRY_LOUD(excpt1)
    {
        PWPSWINDOWDATA pwd = WinQueryWindowPtr(hwnd, QWL_USER);
        SOMAny *somSelf;
        XWPMouseData *somThis;

        if (pwd)
        {
            // Zugriff auf Klassenvariablen immer ermîglichen
            somSelf = pwd->somSelf;
            somThis = XWPMouseGetData(somSelf);
        }

        switch (msg)
        {

            case WM_INITDLG:
            {
                PFNWP pfnwpOriginal;

                // store windowdata
                pwd = malloc(sizeof(WPSWINDOWDATA));
                memset(pwd, 0, sizeof(WPSWINDOWDATA));
                pwd->somSelf = (SOMAny *) mp2;
                WinSetWindowPtr(hwnd, QWL_USER, pwd);
                somSelf = pwd->somSelf;
                somThis = XWPMouseGetData(somSelf);

                // store window handle
                _hwndNotebookPage = hwnd;

                // Resource Module handle speichern
                pwd->hmodResource = cmnQueryNLSModuleHandle(FALSE);
                        // cmnQueryNLSModuleHandle(FALSE); // V0.9.3 (2000-05-21) [umoeller] V0.9.3 (2000-05-21) [umoeller]

// ### integrated code begin

                // auf SubcommandHandler subclassen
                pfnwpOriginal = WinSubclassWindow(hwnd, DialogHandlerProc);

                // ausserdem der Subclass-Funktion auch WM_INITDLG zukommen lassen
                // Pointer auf HANDLERDATA wird an erster Stelle in WINDOWDATA Åbergeben
                if ((pwd->phd = malloc(sizeof(HANDLERDATA))) != 0)
                {
                    memset(pwd->phd, 0, sizeof(HANDLERDATA));
                    pwd->phd->hmodResource = pwd->hmodResource;
                    pwd->phd->somSelf = somSelf;
                    pwd->phd->pfnwpOriginal = pfnwpOriginal;
                    pwd->phd->ppcnrrec = &_pcnrrec;
                }
                mrc = DialogHandlerProc(hwnd, msg, 0, 0);
                fCallDefault = FALSE;
// ### integrated code end
            }
            break;              // WM_INITDLG

    // ### integrated code begin
    #define SETTINGS_LOADSET      "CLASSLIST=WPFolder;STARTFOLDER=%s;SUBTREESRCH=NO;DEFAULTCRITERIA=YES;"
    #define SETTINGS_FINDPOINTER  "CLASSLIST=WPPointer;STARTFOLDER=%s;SUBTREESRCH=NO;DEFAULTCRITERIA=YES;"

            case WM_USER_SERVICE:
            {
                switch (LONGFROMMP(mp1))
                {
                    case SERVICE_RESTORE:
                        DEBUGMSG("SERVICE: restore" NEWLINE, 0);
                        ScanSetupString(hwnd, PVOIDFROMMP(mp2), _pszCurrentSettings, FALSE, FALSE);
                        break;  // SERVICE_RESTORE

                    case SERVICE_SAVE:
                        DEBUGMSG("SERVICE: save" NEWLINE, 0);
                        _wpSaveImmediate(somSelf);
                        break;  // SERVICE_SAVE

                    case SERVICE_HELP:
                        {
                            ULONG ulPanelId = LONGFROMMP(mp2);
                            PSZ pszHelpLibrary = (PSZ)cmnQueryHelpLibrary();
                                    // (PSZ)cmnQueryHelpLibrary(); // V0.9.3 (2000-05-21) [umoeller] V0.9.3 (2000-05-21) [umoeller]

                            DEBUGMSG("SERVICE: help %u in %s" NEWLINE, ulPanelId _c_ pszHelpLibrary);
                            _wpDisplayHelp(somSelf, ulPanelId, pszHelpLibrary);
                        }
                        break;  // SERVICE_HELP

                    case SERVICE_HELPMENU:
                        {
                            ULONG ulMenuId = LONGFROMMP(mp2);
                            BOOL fResult;
                            APIRET rc;

                            fResult = _wpMenuItemSelected(somSelf, hwnd, ulMenuId);
                            rc = (fResult) ? 0 : ERRORIDERROR(WinGetLastError(WinQueryAnchorBlock(hwnd)));
                            DEBUGMSG("SERVICE: menu help %u (result=%u rc=%u/0x%08x)" NEWLINE, ulMenuId _c_ fResult _c_ rc _c_ rc);

                        }
                        break;  // SERVICE_HELP

                    case SERVICE_FIND:
                        {
                            APIRET rc;
                            CHAR szFullname[_MAX_PATH];

                            DEBUGMSG("SERVICE: Find Pointer" NEWLINE, 0);
                            rc = FindFiles(_XWPMouse,
                                            somSelf,
                                            HWND_DESKTOP,
                                            hwnd,
                                            cmnQueryNLSModuleHandle(FALSE),
                                                // _clsQueryResourceModuleHandle(_XWPMouse), V0.9.3 (2000-05-21) [umoeller]
                             (mp2) ? PVOIDFROMMP(mp2) : DEFAULT_ANIMATIONPATH,
                                       szFullname, sizeof(szFullname), FALSE);
                            if (rc == NO_ERROR)
                                mrc = strdup(szFullname);
                            else
                                mrc = NULL;
                            fCallDefault = FALSE;
                        }
                        break;  // SERVICE_FIND

                    case SERVICE_LOAD:
                        {
                            APIRET rc;
                            CHAR szFullname[_MAX_PATH];

                            DEBUGMSG("SERVICE: Load Set" NEWLINE, 0);

                            rc = FindFiles(_XWPMouse, somSelf, HWND_DESKTOP, hwnd,
                                           cmnQueryNLSModuleHandle(FALSE),
                                            // _clsQueryResourceModuleHandle(_XWPMouse), V0.9.3 (2000-05-21) [umoeller]
                             (mp2) ? PVOIDFROMMP(mp2) : DEFAULT_ANIMATIONPATH,
                                        szFullname, sizeof(szFullname), TRUE);
                            if (rc == NO_ERROR)
                                mrc = strdup(szFullname);
                            else
                                mrc = NULL;
                            fCallDefault = FALSE;
                        }
                        break;  // SERVICE_LOAD

                } // switch (SHORT1FROMMP(mp1))
            }
            break;              // end case WM_USER_SERVICE:

    // ### integrated code end

            case WM_DESTROY:
                // reset window handle
                _hwndNotebookPage = NULLHANDLE;
                _pcnrrec = NULL;
                break;
        }
    }
    CATCH(excpt1) {} END_CATCH();

   if (fCallDefault)
       mrc = WinDefDlgProc(hwnd, msg, mp1, mp2);

    return (mrc);
}

// -------------------------------------------------------------------------------------

int APIENTRY InitDll(void)
{
    BOOL fSuccess;
    APIRET rc = NO_ERROR;
    HMODULE hmodResource = NULLHANDLE;

    FUNCENTER();

    do
    {
        // Resource lib laden
        rc = LoadResourceLib(&hmodResource);
        if (rc != NO_ERROR)
            break;

#ifdef BETA
        // expiration date prÅfen
        rc = CheckExpiration(hmodResource);
        if (rc != NO_ERROR)
            break;
#endif

        // nun initilalisieren
        rc = InitializePointerlist(WinQueryAnchorBlock(HWND_DESKTOP), hmodResource);

    }
    while (FALSE);

// cleanup
    if (hmodResource)
        DosFreeModule(hmodResource);

    fSuccess = (rc == NO_ERROR);
    return fSuccess;

}

int APIENTRY UnInitDll(void)
{
// never called on shutdown, so TRUE in anyway
    return TRUE;
}


// disabled V0.9.4 (2000-07-22) [umoeller]

/* BOOL DisplayHelp(PVOID somSelf,
                 ULONG ulHelpPanelId)
{
    PSZ pszHelpLibrary = (PSZ)cmnQueryHelpLibrary();
        // (PSZ)cmnQueryHelpLibrary(); // V0.9.3 (2000-05-21) [umoeller] V0.9.3 (2000-05-21) [umoeller]

    return _wpDisplayHelp(somSelf, ulHelpPanelId, pszHelpLibrary);
} */
