
/*
 *@@sourcefile fileops.c:
 *      file operations code. This gets called from various
 *      method overrides to implement certain file-operations
 *      features.
 *
 *      This file is ALL new with V0.9.0.
 *
 *      Function prefix for this file:
 *      --  fops*
 *
 *@@added V0.9.0 [umoeller]
 *@@header "filesys\fileops.h"
 */

/*
 *      Copyright (C) 1997-99 Ulrich M”ller.
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

#define INCL_WININPUT
#define INCL_WINDIALOGS
#define INCL_WINBUTTONS
#define INCL_WINENTRYFIELDS
#define INCL_WINSHELLDATA       // Prf* functions
#include <os2.h>

// C library headers
#include <stdio.h>              // needed for except.h
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\stringh.h"            // string helper routines
#include "helpers\winh.h"               // PM helper routines

// SOM headers which don't crash with prec. header files
#include "xfobj.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file

// other SOM headers
#pragma hdrstop                         // VAC++ keeps crashing otherwise
#include <wpfolder.h>
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

#include "helpers\undoc.h"              // some undocumented stuff

/********************************************************************
 *                                                                  *
 *   "File exists" (title clash) dialog                             *
 *                                                                  *
 ********************************************************************/

/*
 *@@ fnwpTitleClashDlg:
 *      dlg proc for the "File exists" dialog.
 *      Most of the logic for that dialog is in the
 *      fopsConfirmObjectTitle fungion actually; this
 *      function is only responsible for adjusting the
 *      dialog items' keyboard focus and such.
 */

MRESULT EXPENTRY fnwpTitleClashDlg(HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    #define WM_DELAYEDFOCUS WM_USER+1
    MRESULT mrc = (MRESULT)0;

    switch (msg) {

        /*
         * WM_CONTROL:
         *      intercept focus messages and
         *      set focus to what we really want
         */

        case WM_CONTROL:
        {
            switch (SHORT2FROMMP(mp1)) // usNotifyCode
            {
                case EN_SETFOCUS: // == BN_CLICKED
                {

                    if (SHORT1FROMMP(mp1) == ID_XFDI_CLASH_RENAMENEWTXT)
                        winhSetDlgItemChecked(hwndDlg, ID_XFDI_CLASH_RENAMENEW, TRUE);
                    else if (SHORT1FROMMP(mp1) == ID_XFDI_CLASH_RENAMEOLDTXT)
                        winhSetDlgItemChecked(hwndDlg, ID_XFDI_CLASH_RENAMEOLD, TRUE);
                    else if (SHORT1FROMMP(mp1) == ID_XFDI_CLASH_RENAMENEW)
                        WinPostMsg(hwndDlg, WM_DELAYEDFOCUS,
                                   (MPARAM)ID_XFDI_CLASH_RENAMENEWTXT, MPNULL);
                    else if (SHORT1FROMMP(mp1) == ID_XFDI_CLASH_RENAMEOLD)
                        WinPostMsg(hwndDlg, WM_DELAYEDFOCUS,
                                   (MPARAM)ID_XFDI_CLASH_RENAMEOLDTXT, MPNULL);
                }
            }
        break; }

        case WM_DELAYEDFOCUS:
            winhSetDlgItemFocus(hwndDlg, (HWND)mp1);
        break;

        /*
         * WM_COMMAND:
         *      intercept "OK"/"Cancel" buttons
         */

        case WM_COMMAND:
        {
            mrc = (MRESULT)0;
            switch ((ULONG)mp1)
            {
                case DID_OK:
                {
                    ULONG ulSelection = DID_CANCEL,
                          ulLastFocusID = 0;
                    CHAR szLastFocusID[20] = "";
                    if (winhIsDlgItemChecked(hwndDlg, ID_XFDI_CLASH_RENAMENEW))
                    {
                        ulSelection = ID_XFDI_CLASH_RENAMENEW;
                        ulLastFocusID = ID_XFDI_CLASH_RENAMENEWTXT;
                    }
                    else if (winhIsDlgItemChecked(hwndDlg, ID_XFDI_CLASH_RENAMEOLD))
                    {
                        ulSelection = ID_XFDI_CLASH_RENAMEOLD;
                        ulLastFocusID = ID_XFDI_CLASH_RENAMEOLDTXT;
                    }
                    else
                    {
                        ulSelection = ID_XFDI_CLASH_REPLACE;
                        ulLastFocusID = ID_XFDI_CLASH_REPLACE;
                    }

                    // store focus
                    sprintf(szLastFocusID, "%d",
                            ulLastFocusID);
                    PrfWriteProfileString(HINI_USER, INIAPP_XWORKPLACE,
                                          INIKEY_NAMECLASHFOCUS,
                                          szLastFocusID);
                    // store window pos
                    winhSaveWindowPos(hwndDlg,
                                HINI_USER,
                                INIAPP_XWORKPLACE, INIKEY_WNDPOSNAMECLASH);
                    WinDismissDlg(hwndDlg, ulSelection);
                break; }

                case DID_CANCEL:
                    WinDismissDlg(hwndDlg, DID_CANCEL);
                break;
            }
        break; }

        case WM_HELP:
            cmnDisplayHelp(NULL,    // active Desktop
                           ID_XFH_TITLECLASH);
        break;

        default:
            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
    }

    return (mrc);
}

/*
 *@@ fopsConfirmObjectTitle:
 *      this gets called from XFldObject::wpConfirmObjectTitle
 *      to actually implement the "File exists" dialog. This
 *      performs a number of checks and loads a dialog from
 *      the NLS resources, if necessary, using fnwpTitleClashDlg.
 *
 *      See XFldObject::wpConfirmObjectTitle for a detailed
 *      description. The parameters are exactly the same.
 */

ULONG fopsConfirmObjectTitle(WPObject *somSelf,
                             WPFolder* Folder,
                             WPObject** ppDuplicate,
                             PSZ pszTitle,
                             ULONG cbTitle,
                             ULONG menuID)
{
    ULONG ulrc = NAMECLASH_NONE;

    // nameclashs are only a problem for file-system objects,
    // and only if we're not creating shadows
    if (    (_somIsA(somSelf, _WPFileSystem))
         && (menuID != 0x13C) // "create shadow" code
       )
    {
        WPFileSystem *pobjExisting = NULL;
        CHAR    szTemp[1000],
                szNewRealName[CCHMAXPATH],
                szFolder[CCHMAXPATH];
        BOOL    fFAT;

        // check whether the target folder is valid
        if (Folder)
        {
            if (!_wpQueryFilename(Folder, szFolder, TRUE))
                return (NAMECLASH_CANCEL);
        }
        else
            return (NAMECLASH_CANCEL);

        // before checking names, determine whether
        // the folder is on a FAT drive; we need this
        // flag for title-to-realname translations later
        fFAT = doshIsFileOnFAT(szFolder);

        // this is not trivial, since we need to take
        // in account all the FAT and HPFS file name
        // conversion.
        // 1)   get real name, unless we're renaming;
        //      we then need to check for pszTitle
        if (    (menuID == 0x6E)    // "rename" code
             || (menuID == 0x65)    // "create another" code (V0.9.0)
           )
            strcpy(szTemp, pszTitle);
            // (V0.84) this now does work for FAT to FAT;
            // HPFS to FAT needs to be checked
        else
            _wpQueryFilename(somSelf, szTemp, FALSE);

        // 2)   if the file is on FAT, make this 8+3
        doshMakeRealName(szNewRealName,   // buffer
                         szTemp,
                         '!',              // replace-with char
                         fFAT);
        // this should mimic the default WPS behavior close enough

        #ifdef DEBUG_TITLECLASH
            _Pmpf(("  Checking for existence of %s", szNewRealName));
        #endif

        // does this file exist in pFolder?
        pobjExisting = wpshContainsFile(Folder, szNewRealName);
        if (pobjExisting)
        {
            #ifdef DEBUG_TITLECLASH
                CHAR szFolder2[CCHMAXPATH];
                _Pmpf(("  pObjExisting == 0x%lX", pobjExisting));
                _Pmpf(("    Title: %s", _wpQueryTitle(pobjExisting) ));
                _wpQueryFilename(_wpQueryFolder(pobjExisting), szFolder2, TRUE);
                _Pmpf(("    in folder: %s", szFolder2));
            #endif

            // file exists: now we need to differentiate.
            // 1)   If we're copying, we need to always
            //      display the confirmation.
            // 2)   If we're renaming somSelf, we only
            //      display the confirmation if the
            //      existing object is != somSelf, because
            //      otherwise we have no problem.
            /* if  (   (menuID != 0x6E)    // not "rename" code
                 || (   (menuID == 0x6E)
                     && (somSelf != pobjExisting)
                    )
                ) */
            if (    (somSelf != pobjExisting)
                 || (menuID != 0x6E)    // not "rename"
               )
            {
                CHAR    szProposeTitle[CCHMAXPATH],
                        szFileCount[20],
                        szSelfFilename[CCHMAXPATH],
                        szExistingFilename[CCHMAXPATH];
                ULONG   ulDlgReturn = 0,
                        ulLastFocusID = 0,
                        ulLastDot = 0;
                PSZ     p, p2,
                        pszTemp;
                BOOL    fFileExists = FALSE;
                LONG    lFileCount = -1;
                HWND    hwndConfirm;

                // prepare file date/time etc. for
                // display in window
                FILESTATUS3         fs3;
                COUNTRYSETTINGS     cs;
                prfhQueryCountrySettings(&cs);

                // *** load confirmation dialog
                hwndConfirm = WinLoadDlg(HWND_DESKTOP, HWND_DESKTOP,
                                  fnwpTitleClashDlg,
                                  cmnQueryNLSModuleHandle(FALSE), ID_XFD_TITLECLASH,
                                  NULL);

                // disable window updates for the following changes
                WinEnableWindowUpdate(hwndConfirm, FALSE);

                pszTemp = winhQueryWindowText(WinWindowFromID(hwndConfirm, ID_XFDI_CLASH_TXT1));
                strhxrpl(&pszTemp, 0, "%1", szNewRealName, 0);
                strhxrpl(&pszTemp, 0, "%2", szFolder, 0);
                WinSetDlgItemText(hwndConfirm, ID_XFDI_CLASH_TXT1,
                                pszTemp);
                free(pszTemp);

                // set object information fields
                _wpQueryFilename(pobjExisting, szExistingFilename, TRUE);
                DosQueryPathInfo(szExistingFilename,
                                FIL_STANDARD,
                                &fs3, sizeof(fs3));
                strhFileDate(szTemp, &(fs3.fdateLastWrite),
                             cs.ulDateFormat, cs.cDateSep);
                WinSetDlgItemText(hwndConfirm, ID_XFDI_CLASH_DATEOLD, szTemp);
                strhFileTime(szTemp, &(fs3.ftimeLastWrite),
                             cs.ulTimeFormat, cs.cTimeSep);
                WinSetDlgItemText(hwndConfirm, ID_XFDI_CLASH_TIMEOLD, szTemp);

                strhThousandsULong(szTemp, fs3.cbFile, // )+512) / 1024 ,
                                   cs.cThousands);
                WinSetDlgItemText(hwndConfirm, ID_XFDI_CLASH_SIZEOLD, szTemp);

                if (pobjExisting != somSelf)
                {
                    // if we're not copying within the same folder,
                    // i.e. if the two objects are different,
                    // give info on ourselves too
                    _wpQueryFilename(somSelf, szSelfFilename, TRUE);
                    DosQueryPathInfo(szSelfFilename,
                                     FIL_STANDARD,
                                     &fs3, sizeof(fs3));
                    strhFileDate(szTemp, &(fs3.fdateLastWrite),
                                 cs.ulDateFormat, cs.cDateSep);
                    WinSetDlgItemText(hwndConfirm, ID_XFDI_CLASH_DATENEW, szTemp);
                    strhFileTime(szTemp, &(fs3.ftimeLastWrite),
                                 cs.ulTimeFormat, cs.cTimeSep);
                    WinSetDlgItemText(hwndConfirm, ID_XFDI_CLASH_TIMENEW, szTemp);
                    strhThousandsULong(szTemp, fs3.cbFile, // )+512) / 1024,
                                       cs.cThousands);
                    WinSetDlgItemText(hwndConfirm, ID_XFDI_CLASH_SIZENEW, szTemp);
                }
                else
                {
                    // if we're copying within the same folder,
                    // set the "new object" fields empty
                    WinSetDlgItemText(hwndConfirm, ID_XFDI_CLASH_DATENEW, "");
                    WinSetDlgItemText(hwndConfirm, ID_XFDI_CLASH_TIMENEW, "");
                    WinSetDlgItemText(hwndConfirm, ID_XFDI_CLASH_SIZENEW, "");
                }

                // propose new name; we will use the title and
                // add a ":num" before the extension and then
                // transform that one into a real name and keep
                // increasing "num" until no file with that
                // real name exists (as the WPS does it too)
                strcpy(szTemp, pszTitle);
                // check if title contains a ':num' already
                p2 = strchr(szTemp, ':');
                if (p2)
                {
                    lFileCount = atoi(p2+1);
                    if (lFileCount)
                    {
                        // if we have a valid number following ":", remove it
                        p = strchr(szTemp, '.');
                        if (p)
                            strcpy(p2, p);
                        else
                            *p2 = '\0';
                    }
                }

                // insert new number
                p = strrchr(szTemp, '.');       // last dot == extension
                lFileCount = 1;
                do
                {
                    WPFileSystem *pExistingFile2;

                    sprintf(szFileCount, ":%d", lFileCount);
                    if (p)
                    {
                        // has extension: insert
                        ULONG ulBeforeDot = (p - szTemp);
                        // we don't care about FAT, because
                        // we propose titles, not real names
                        // (V0.84)
                        /* if (   (fFAT)
                            && ((ulBeforeDot+strlen(szFileCount)) > 8)
                           )
                            ulBeforeDot = 8 - strlen(szFileCount); */
                        strncpy(szProposeTitle, szTemp, ulBeforeDot);
                        szProposeTitle[ulBeforeDot] = '\0';
                        strcat(szProposeTitle, szFileCount);
                        strcat(szProposeTitle, p);
                    }
                    else
                    {
                        // has no extension: append
                        strncpy(szProposeTitle, szTemp, 255);
                        // we don't care about FAT, because
                        // we propose titles, not real names
                        // (V0.84)
                        /*     (fFAT) ? 8 : 255);
                        if (fFAT)
                            szProposeTitle[8] = '\0'; */
                        strcat(szProposeTitle, szFileCount);
                    }
                    lFileCount++;

                    if (lFileCount > 99)
                        // avoid endless loops
                        break;

                    // now go through the folder content and
                    // check whether we already have a file
                    // with the proposed title (not real name!)
                    // (V0.84)
                    fFileExists = FALSE;
                    for (   pExistingFile2 = _wpQueryContent(Folder, NULL, (ULONG)QC_FIRST);
                            (pExistingFile2);
                            pExistingFile2 = _wpQueryContent(Folder, pExistingFile2, (ULONG)QC_NEXT)
                        )
                    {
                        fFileExists = (stricmp(_wpQueryTitle(pExistingFile2),
                                              szProposeTitle) == 0);
                        if (fFileExists)
                            break;
                    }
                } while (fFileExists);

                // OK, we've found a new filename: set dlg items
                WinSendDlgItemMsg(hwndConfirm, ID_XFDI_CLASH_RENAMENEWTXT,
                        EM_SETTEXTLIMIT,
                        (MPARAM)(250), MPNULL);
                WinSetDlgItemText(hwndConfirm, ID_XFDI_CLASH_RENAMENEWTXT,
                        szProposeTitle);
                WinSendDlgItemMsg(hwndConfirm, ID_XFDI_CLASH_RENAMEOLDTXT,
                        EM_SETTEXTLIMIT,
                        (MPARAM)(250), MPNULL);
                WinSetDlgItemText(hwndConfirm, ID_XFDI_CLASH_RENAMEOLDTXT,
                        szProposeTitle);

                // select the first characters up to the extension
                // in the edit field
                p = strrchr(szProposeTitle, '.');       // last dot == extension
                if (p)
                    ulLastDot = (p-szProposeTitle);
                else
                    ulLastDot = 300;                    // too large == select all

                WinSendDlgItemMsg(hwndConfirm, ID_XFDI_CLASH_RENAMENEWTXT,
                                  EM_SETSEL,
                                  MPFROM2SHORT(0, ulLastDot),
                                  MPNULL);
                WinSendDlgItemMsg(hwndConfirm, ID_XFDI_CLASH_RENAMEOLDTXT,
                                  EM_SETSEL,
                                  MPFROM2SHORT(0, ulLastDot),
                                  MPNULL);

                // find the selection the user has made last time;
                // this INI key item is maintained by fnwpTitleClashDlg above
                ulLastFocusID = PrfQueryProfileInt(HINI_USER, INIAPP_XWORKPLACE,
                                    INIKEY_NAMECLASHFOCUS,
                                    ID_XFDI_CLASH_RENAMENEWTXT); // default value if not set

                // disable "Replace" and "Rename old"
                // if we're copying within the same folder
                // or if we're copying a folder to its parent
                if (    (pobjExisting == somSelf)
                     || (pobjExisting == _wpQueryFolder(somSelf))
                   )
                {
                    WinEnableControl(hwndConfirm, ID_XFDI_CLASH_REPLACE, FALSE);
                    WinEnableControl(hwndConfirm, ID_XFDI_CLASH_RENAMEOLD, FALSE);
                    WinEnableControl(hwndConfirm, ID_XFDI_CLASH_RENAMEOLDTXT, FALSE);
                    // if the last focus is one of the disabled items,
                    // change it
                    if (   (ulLastFocusID == ID_XFDI_CLASH_REPLACE)
                        || (ulLastFocusID == ID_XFDI_CLASH_RENAMEOLDTXT)
                       )
                        ulLastFocusID = ID_XFDI_CLASH_RENAMENEWTXT;
                }
                else if (menuID == 0x006E)
                {
                    // disable "Replace" for "Rename" mode; this is
                    // not allowed
                    WinEnableControl(hwndConfirm, ID_XFDI_CLASH_REPLACE, FALSE);
                    // if the last focus is one of the disabled items,
                    // change it
                    if (ulLastFocusID == ID_XFDI_CLASH_REPLACE)
                        ulLastFocusID = ID_XFDI_CLASH_RENAMENEWTXT;
                }

                // set focus to that item
                winhSetDlgItemFocus(hwndConfirm, ulLastFocusID);
                    // this will automatically select the corresponding
                    // radio button, see fnwpTitleClashDlg above

                // *** go!
                winhRestoreWindowPos(hwndConfirm,
                        HINI_USER,
                        INIAPP_XWORKPLACE, INIKEY_WNDPOSNAMECLASH,
                        // move only, no resize
                        SWP_MOVE | SWP_SHOW | SWP_ACTIVATE);
                ulDlgReturn = WinProcessDlg(hwndConfirm);

                // check return value
                switch (ulDlgReturn)
                {
                    case ID_XFDI_CLASH_RENAMENEW:
                    {
                        // rename new: copy user's entry to buffer
                        // provided by the method
                        WinQueryDlgItemText(hwndConfirm, ID_XFDI_CLASH_RENAMENEWTXT,
                                        cbTitle-1, pszTitle);
                        ulrc = NAMECLASH_RENAME;
                    break; }

                    case ID_XFDI_CLASH_RENAMEOLD:
                    {
                        // rename old: use wpSetTitle on existing object
                        WinQueryDlgItemText(hwndConfirm, ID_XFDI_CLASH_RENAMEOLDTXT,
                                        sizeof(szTemp)-1, szTemp);
                        _wpSetTitleAndRenameFile(pobjExisting, szTemp, 0);

                        if (menuID == 0x6E)     // "rename" code
                        {
                            CHAR szNewRealName2[CCHMAXPATH];
                            doshMakeRealName(szNewRealName2, pszTitle, '!', TRUE);
                            _wpSetTitleAndRenameFile(somSelf, pszTitle, 0);
                        }
                        ulrc = NAMECLASH_NONE;
                    break; }

                    case ID_XFDI_CLASH_REPLACE:
                        *ppDuplicate = pobjExisting;
                        ulrc = NAMECLASH_REPLACE;
                    break;

                    case DID_CANCEL:
                        ulrc = NAMECLASH_CANCEL;
                    break;
                }
                WinDestroyWindow(hwndConfirm);
            } // end if  (   (menuID != 0x6E) // "rename" code
              // || (somSelf != pobjExisting)
              // )
        }
        else if (menuID == 0x6E)     // "rename" code
        {
            // if (!pobjExisting) and we're renaming
            CHAR szNewRealName2[CCHMAXPATH];
            doshMakeRealName(szNewRealName2, pszTitle, '!', TRUE);
            if (_wpSetTitleAndRenameFile(somSelf,
                                         pszTitle,
                                         0))
                // no error:
                ulrc = NAMECLASH_NONE;
            else
                ulrc = NAMECLASH_CANCEL;
        }
    } // end if (_somIsA(somSelf, _WPFileSystem))

    return (ulrc);
}

