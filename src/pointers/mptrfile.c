
/*
 *@@sourcefile mptrfile.c:
 *
 *      This file is ALL new with V0.9.4.
 *
 *@@added V0.9.4 [umoeller]
 *@@header "pointers\mptrfile.h"
 */

/*
 *      Copyright (C) 1996-2000 Christian Langanke.
 *      Copyright (C) 2000 Ulrich M罵ler.
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
#include "pointers\mptrptr.h"
#include "pointers\mptrcnr.h"
#include "pointers\mptrutil.h"
#include "pointers\macros.h"
#include "pointers\debug.h"

#include "..\..\001\dll\r_amptr001.h"

//efine USE_FILEEAS_IN_OPEN_DIALOG

static PSZ pszFilterAllFiles = "*.ptr;*.cur;*.ani;*.and;";

// extern variables
extern CHAR szFileTypeAll[MAX_RES_STRLEN];
extern CHAR szFileTypePointer[MAX_RES_STRLEN];
extern CHAR szFileTypeCursor[MAX_RES_STRLEN];
extern CHAR szFileTypeAniMouse[MAX_RES_STRLEN];
extern CHAR szFileTypeWinAnimation[MAX_RES_STRLEN];

/*ﾚﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄｿ
 *ｳ Name      : SubclassFileDlgProc                                        ｳ
 *ｳ Kommentar :                                                            ｳ
 *ｳ Autor     : C.Langanke                                                 ｳ
 *ｳ Datum     : 28.06.1998                                                 ｳ
 *ｳ 始derung  : 28.06.1998                                                 ｳ
 *ｳ Eingabe   : HWND ULONG MPARAM MPARAM                                   ｳ
 *ｳ Aufgaben  : - Messages bearbeiten                                      ｳ
 *ｳ R…kgabe  : MRESULT - Message Result                                   ｳ
 *ﾀﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾙ
 */


BOOL INSERTTYPE
 (
     HWND hwnd,
     PSZ pszType,
     ULONG ulHandle
)
{
    ULONG ulCurrentItem;

    ulCurrentItem = (ULONG)WinSendDlgItemMsg(hwnd,
                                             DID_FILTER_CB,
                                             LM_INSERTITEM,
                                             MPFROMLONG(LIT_END),
                                             MPFROMP(pszType));

    return (BOOL)WinSendDlgItemMsg(hwnd,
                                   DID_FILTER_CB,
                                   LM_SETITEMHANDLE,
                                   MPFROMLONG(ulCurrentItem), MPFROMLONG(ulHandle));
}
// ------- helper ends


MRESULT EXPENTRY _Export SubclassFileDlgProc
 (
     HWND hwnd,
     ULONG msg,
     MPARAM mp1,
     MPARAM mp2
)

{
    MRESULT mresult;
    HWND hwndCombobox;
    PFILEDLG pfiledlg = WinQueryWindowPtr(hwnd, QWL_USER);

    switch (msg)
    {

            // ------------------------------------------------------------------------

        case WM_INITDLG:
            {
                ULONG ulCurrentItem;
                ULONG ulSelection;

                // process original
                mresult = WinDefFileDlgProc(hwnd, msg, mp1, mp2);

                // delete default item of file type combobox
                WinSendDlgItemMsg(hwnd, DID_FILTER_CB, LM_DELETEITEM, 0, 0);

                // insert own items and set the handle
                INSERTTYPE(hwnd, szFileTypePointer, RESFILETYPE_POINTER);
                INSERTTYPE(hwnd, szFileTypeCursor, RESFILETYPE_CURSOR);
                INSERTTYPE(hwnd, szFileTypeWinAnimation, RESFILETYPE_WINANIMATION);
                INSERTTYPE(hwnd, szFileTypeAniMouse, RESFILETYPE_ANIMOUSE);

#ifdef USE_FILEEAS_IN_OPEN_DIALOG
                if (pfiledlg->ulUser == RESFILETYPE_DEFAULT)
                    INSERTTYPE(hwnd, szFileTypeAll, RESFILETYPE_DEFAULT);
#endif

                // seelct the correct item
                switch (pfiledlg->ulUser)
                {
                    case RESFILETYPE_POINTER:
                        ulSelection = 0;
                        break;
                    case RESFILETYPE_CURSOR:
                        ulSelection = 1;
                        break;
                    case RESFILETYPE_WINANIMATION:
                        ulSelection = 2;
                        break;
                    case RESFILETYPE_ANIMOUSE:
                        ulSelection = 3;
                        break;

#ifdef USE_FILEEAS_IN_OPEN_DIALOG
                    case RESFILETYPE_DEFAULT:
                        ulSelection = 4;
                        break;
#endif

                }

                SETSELECTION(hwnd, DID_FILTER_CB, ulSelection);

            }
            return mresult;
            // break;              // case WM_INITDLG:

            // ------------------------------------------------------------------------

        case WM_COMMAND:

            switch (SHORT1FROMMP(mp1))
            {
                    // - - - - - - - - - - - - - - - - - - - - -
                case DID_OK:
                    {
                        ULONG ulItem;
                        ULONG ulFiletype;
                        ULONG ulNewFileType;
                        CHAR szFilename[_MAX_PATH];
                        CHAR szDllFilename[_MAX_PATH];
                        PSZ pszExtension;
                        BOOL fModifyFilename = FALSE;

                        // query current item
                        ulItem = QUERYSELECTION(hwnd,
                                                DID_FILTER_CB,
                                                MPFROMLONG(LIT_FIRST));
                                                // 0); V0.9.3 (2000-05-21) [umoeller]
                        ulFiletype = (ULONG)WinSendDlgItemMsg(hwnd,
                                                       DID_FILTER_CB,
                                                       LM_QUERYITEMHANDLE,
                                                       MPFROMLONG(ulItem), 0);

                        // query current filename and check
                        WinQueryDlgItemText(hwnd, DID_FILENAME_ED, sizeof(szFilename), szFilename);
                        pszExtension = Filespec(szFilename, FILESPEC_EXTENSION);

                        do
                        {
                            // check extension for filetype
                            if ((!pszExtension) ||
                                (!QueryResFileTypeFromExt(pszExtension, &ulNewFileType)) ||
                                (ulNewFileType != ulFiletype))
                            {
                                ChangeFilename(szFilename, CHANGE_EXTENSION,
                                               szFilename, sizeof(szFilename),
                                 QueryResFileExtension(ulFiletype) + 1, 0, 0);
                                fModifyFilename = TRUE;
                            }


                            switch (ulFiletype)
                            {
                                case RESFILETYPE_ANIMOUSE:
                                    // check 8.3 compliance for AniMouse DLLs
                                    ChangeFilename(szFilename, CHANGE_USEDLLNAME,
                                         szDllFilename, sizeof(szDllFilename),
                                                   NULL, 0, 0);

                                    if (strcmp(szFilename, szDllFilename))
                                    {
                                        strcpy(szFilename, szDllFilename);
                                        fModifyFilename = TRUE;
                                    }
                                    break;
                            }

                        }
                        while (FALSE);

                        if (fModifyFilename)
                        {
                            WinSetDlgItemText(hwnd, DID_FILENAME_ED, szFilename);
                            WinAlarm(HWND_DESKTOP, WA_ERROR);
                            return (MRESULT) TRUE;
                        }


                    }
                    break;      // case DID_OK

            }                   // switch (SHORT1FROMMP( mp1))

            break;              // case WM_COMMAND:


            // ------------------------------------------------------------------------

        case WM_CONTROL:

            if (SHORT1FROMMP(mp1) == DID_FILTER_CB)
            {
                switch (SHORT2FROMMP(mp1))
                {
                    case LN_SELECT:
                        {
                            ULONG ulItem;
                            ULONG ulFiletype;
                            CHAR szFilename[_MAX_PATH];

                            // query current item
                            ulItem = QUERYSELECTION(hwnd,
                                                    DID_FILTER_CB,
                                                    MPFROMLONG(LIT_FIRST));
                                                    // 0); V0.9.3 (2000-05-21) [umoeller]

#ifdef USE_FILEEAS_IN_OPEN_DIALOG
                            if (ulItem == 4)
                            {
                                // set filefilter !
                                WinSetDlgItemText(hwnd, DID_FILENAME_ED, pszFilterAllFiles);
                                break;
                            }
#endif

                            ulFiletype = (ULONG)WinSendDlgItemMsg(hwnd,
                                                        DID_FILTER_CB,
                                                        LM_QUERYITEMHANDLE,
                                                        MPFROMLONG(ulItem), 0);

                            // query current filename, change it and reset
                            WinQueryDlgItemText(hwnd, DID_FILENAME_ED, sizeof(szFilename), szFilename);
                            ChangeFilename(szFilename, CHANGE_EXTENSION,
                                           szFilename, sizeof(szFilename),
                                 QueryResFileExtension(ulFiletype) + 1, 0, 0);

                            switch (ulFiletype)
                            {
                                case RESFILETYPE_ANIMOUSE:
                                case RESFILETYPE_WINANIMATION:
                                    ChangeFilename(szFilename,
                                                   CHANGE_DELNUMERATION,
                                                   szFilename,
                                                   sizeof(szFilename),
                                                   NULL,
                                                   0, 0);
                                    break;

                                case RESFILETYPE_POINTER:
                                case RESFILETYPE_CURSOR:
                                    ChangeFilename(szFilename,
                                                   CHANGE_ADDNUMERATION,
                                                   szFilename,
                                                   sizeof(szFilename),
                                                   NULL,
                                                   0, 0);
                                    break;

                            }   // switch (ulFiletype)

                            // set correct name
                            WinSetDlgItemText(hwnd, DID_FILENAME_ED, szFilename);
                        }
                        break;

                }
            }

            break;              // case WM_CONTROL:

    }                           // endswitch

    return WinDefFileDlgProc(hwnd, msg, mp1, mp2);
}

/*ﾚﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄｿ
 *ｳ Name      : SelectFile                                                 ｳ
 *ｳ Kommentar :                                                            ｳ
 *ｳ Autor     : C.Langanke                                                 ｳ
 *ｳ Datum     : 28.06.1998                                                 ｳ
 *ｳ 始derung  : 28.06.1998                                                 ｳ
 *ｳ Eingabe   : HWND   - hwnd of owner                                     ｳ
 *ｳ             ULONG  - Dialog style flags                                ｳ
 *ｳ             PSZ    - Initial filename                                  ｳ
 *ｳ             PULONG - ptr to filetype variable  (in/out)                ｳ
 *ｳ             PSZ    - ptr to buffer                                     ｳ
 *ｳ             ULONG  - bufferlen                                         ｳ
 *ｳ Aufgaben  : - Messages bearbeiten                                      ｳ
 *ｳ R…kgabe  : MRESULT - Message Result                                   ｳ
 *ﾀﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾙ
 */

BOOL _Export SelectFile
 (
     HWND hwndOwner,
     ULONG ulDialogStyle,
     PSZ pszInitialFilename,
     PULONG pulTargetFileType,
     PSZ pszFileName,
     ULONG ulBuflen
)


{
    APIRET rc;
    HAB hab = WinQueryAnchorBlock(hwndOwner);
    FILEDLG filedlg;
    HWND hwndFiledlg;
    PSZ apszTypelist[] =
    {szFileTypePointer,
     szFileTypeCursor,
     szFileTypeAniMouse,
     szFileTypeWinAnimation,
     NULL};
    BOOL fFileSelected = FALSE;
    PSZ pszFileExt;

    do
    {
        // check parms
        if ((pszFileName == NULL) ||
            (ulBuflen < 1))
            break;

        // setup dlg info
        memset(&filedlg, 0, sizeof(filedlg));
        filedlg.cbSize = sizeof(filedlg);
        filedlg.fl = FDS_CENTER | ulDialogStyle;    // FDS_SAVEAS_DIALOG;

        filedlg.ulUser = *pulTargetFileType;    // save filetype for subclass

#ifndef USE_FILEEAS_IN_OPEN_DIALOG
        if (ulDialogStyle & FDS_SAVEAS_DIALOG)
#endif
            filedlg.pfnDlgProc = SubclassFileDlgProc;

        if (pszInitialFilename)
            strcpy(filedlg.szFullFile, pszInitialFilename);

        // select default filetype
        if (!filedlg.ulUser)
        {
            filedlg.ulUser = RESFILETYPE_DEFAULT;
            strcpy(filedlg.szFullFile, pszFilterAllFiles);
        }

        // prompt the user
        hwndFiledlg = WinFileDlg(HWND_DESKTOP, hwndOwner, &filedlg);
        if ((!hwndFiledlg) || (filedlg.lReturn != DID_OK))
        {
            memset(pszFileName, 0, ulBuflen);
            rc = ERRORIDERROR(WinGetLastError(hab));
            break;
        }
        else
            fFileSelected = TRUE;

        // is buffer large enough ?
        if (ulBuflen < strlen(filedlg.szFullFile) + 1)
            break;

        // hand over result
        strcpy(pszFileName, filedlg.szFullFile);
        pszFileExt = Filespec(pszFileName, FILESPEC_EXTENSION);
        QueryResFileTypeFromExt(pszFileExt, pulTargetFileType);

    }
    while (FALSE);

    return fFileSelected;

}

/*ﾚﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄｿ
 *ｳ Name      : WriteTargetFiles                                           ｳ
 *ｳ Kommentar :                                                            ｳ
 *ｳ Autor     : C.Langanke                                                 ｳ
 *ｳ Datum     : 28.06.1998                                                 ｳ
 *ｳ 始derung  : 28.06.1998                                                 ｳ
 *ｳ Eingabe   : PSZ          - Name of taregt file                         ｳ
 *ｳ             PPOINTERLIST - ptr to pointerlist                          ｳ
 *ｳ             BOOL         - Flag for saving all/single pointer          ｳ
 *ｳ Aufgaben  : - Messages bearbeiten                                      ｳ
 *ｳ R…kgabe  : MRESULT - Message Result                                   ｳ
 *ﾀﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾄﾙ
 */

BOOL _Export WriteTargetFiles
 (
     PSZ pszFileName,
     ULONG ulTargetFileType,
     PPOINTERLIST ppl,
     BOOL fSaveAllPointers
)
{
    APIRET rc = NO_ERROR;

    do
    {
        // check parms
        if ((pszFileName == NULL) ||
            (ppl == NULL))
            break;

        // write the file
        switch (ulTargetFileType)
        {
            case RESFILETYPE_POINTER:
                rc = (fSaveAllPointers) ?
                    WriteAnimationSetToPointerFile(pszFileName, ppl, NULL, 0) :
                    WritePointerlistToPointerFile(pszFileName, ppl, NULL, 0);
                break;

            case RESFILETYPE_CURSOR:
                rc = (fSaveAllPointers) ?
                    WriteAnimationSetToCursorFile(pszFileName, ppl, NULL, 0) :
                    WritePointerlistToCursorFile(pszFileName, ppl, NULL, 0);
                break;

            case RESFILETYPE_WINANIMATION:
                rc = (fSaveAllPointers) ?
                    WriteAnimationSetToWinAnimationFile(pszFileName, ppl, NULL, 0) :
                    WritePointerlistToWinAnimationFile(pszFileName, ppl);
                break;

            case RESFILETYPE_ANIMOUSE:
                // always use adress of arrow POINTERLIST,
                // because for storage in an AND file the
                // pointer index must always be used
                rc = (fSaveAllPointers) ?
                    WriteAnimationSetToAnimouseFile(pszFileName, ppl) :
                    WritePointerlistToAnimouseFile(pszFileName, ppl);
                break;
        }

    }
    while (FALSE);

    return (rc == NO_ERROR);

}
