
/*
 *@@sourcefile filesys.c:
 *      various implementation code related to file-system objects.
 *      This is mostly for code which is shared between folders
 *      and data files.
 *      So this code gets interfaced from XFolder, XFldDataFile,
 *      and XFldProgramFile.
 *
 *      This file is ALL new with V0.9.0.
 *
 *      Function prefix for this file:
 *      --  fsys*
 *
 *@@added V0.9.0 [umoeller]
 *@@header "filesys\filesys.h"
 */

/*
 *      Copyright (C) 1997-2000 Ulrich M”ller.
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

// #define INCL_DOSSEMAPHORES
#define INCL_DOSERRORS
#define INCL_WINDIALOGS
#define INCL_WINBUTTONS
#define INCL_WINENTRYFIELDS
#define INCL_WINLISTBOXES
#define INCL_WINMENUS
#define INCL_WINMLE
#define INCL_WINPROGRAMLIST     // needed for PROGDETAILS, wppgm.h
#include <os2.h>

// C library headers
#include <stdio.h>

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\eah.h"                // extended attributes helper routines
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\stringh.h"            // string helper routines
#include "helpers\winh.h"               // PM helper routines
#include "helpers\xstring.h"            // extended string helpers

// SOM headers which don't crash with prec. header files
#include "xfpgmf.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\notebook.h"            // generic XWorkplace notebook handling

#include "filesys\filesys.h"            // various file-system object implementation code

// other SOM headers
#pragma hdrstop                 // VAC++ keeps crashing otherwise
#include <wpdesk.h>             // this includes wpfolder.h

/* ******************************************************************
 *                                                                  *
 *   "File" pages replacement in WPDataFile/WPFolder                *
 *                                                                  *
 ********************************************************************/

/*
 *@@ FILEPAGEDATA:
 *      structure used for backing up file page data
 *      (Undo button on "File" page).
 */

typedef struct _FILEPAGEDATA
{
    // file attributes backup
    ULONG       ulAttr;
    // EA backups
    PEABINDING  peabSubject;
    PEABINDING  peabComments;
    PEABINDING  peabKeyphrases;
} FILEPAGEDATA, *PFILEPAGEDATA;

/*
 *@@ fsysFile1InitPage:
 *      first "File" page notebook callback function (notebook.c).
 *      Sets the controls on the page according to a file's
 *      filesystem characteristics.
 *
 *      This is used by both XFolder and XFldDataFile.
 *
 *      Notes about the EAs which are parsed here (also see the
 *      "Extended Attributes" section in CPREF):
 *      --  The "Subject" field on the page corresponds to
 *          the .SUBJECT EA. This is a single-type EAT_ASCII EA.
 *      --  The "Comments" field corresponds to the .COMMENTS EA.
 *          This is multi-value, multi-type (EAT_MVMT), but all the
 *          subvalues are of EAT_ASCII. All lines in the default
 *          WPS "File" multi-line entry field terminated by CR/LF
 *          are put in one of those subvalues.
 *      --  The "Keyphrases" field corresponds to .KEYPHRASES.
 *          This is also EAT_MVMT and used like .COMMENTS.
 *
 *@@changed V0.9.1 (2000-01-22) [umoeller]: renamed from fsysFileInitPage
 */

VOID fsysFile1InitPage(PCREATENOTEBOOKPAGE pcnbp,    // notebook info struct
                       ULONG flFlags)                // CBI_* flags (notebook.h)
{
    // PGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    if (flFlags & CBI_INIT)
    {
        if (pcnbp->pUser == NULL)
        {
            // first call: backup instance data for "Undo" button;
            // this memory will be freed automatically by the
            // common notebook window function (notebook.c) when
            // the notebook page is destroyed
            CHAR            szFilename[CCHMAXPATH];
            PFILEPAGEDATA   pfpd = (PFILEPAGEDATA)malloc(sizeof(FILEPAGEDATA));
            pcnbp->pUser = pfpd;
            pfpd->ulAttr = _wpQueryAttr(pcnbp->somSelf);
            _wpQueryFilename(pcnbp->somSelf, szFilename, TRUE);
            pfpd->peabSubject = eaPathReadOneByName(szFilename, ".SUBJECT");
            pfpd->peabComments = eaPathReadOneByName(szFilename, ".COMMENTS");
            pfpd->peabKeyphrases = eaPathReadOneByName(szFilename, ".KEYPHRASES");

            if (doshIsFileOnFAT(szFilename))
            {
                // on FAT: hide fields
                winhShowDlgItem(pcnbp->hwndDlgPage, ID_XSDI_FILES_CREATIONDATE,
                                FALSE);
                winhShowDlgItem(pcnbp->hwndDlgPage, ID_XSDI_FILES_LASTACCESSDATE,
                                FALSE);
            }

            if (!_somIsA(pcnbp->somSelf, _WPFolder))
            {
                // this page is not for a folder, but
                // a data file:
                // hide "Work area" item
                winhShowDlgItem(pcnbp->hwndDlgPage, ID_XSDI_FILES_WORKAREA, FALSE);
            }
            else if (_somIsA(pcnbp->somSelf, _WPDesktop))
                // for the Desktop, disable work area;
                // this must not be changed
                WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_FILES_WORKAREA,
                                  FALSE);
        }

        // Even though CPREF says that the .SUBJECT EA was limited to
        // 40 chars altogether, this is wrong apparently, as many users
        // have said after V0.9.1; so limit the entry field to 260 chars
        WinSendDlgItemMsg(pcnbp->hwndDlgPage, ID_XSDI_FILES_SUBJECT,
                          EM_SETTEXTLIMIT,
                          (MPARAM)(260), MPNULL);
    }

    if (flFlags & CBI_SET)
    {
        // prepare file date/time etc. for display in window
        CHAR    szFilename[CCHMAXPATH];
        CHAR    szTemp[100];
        PSZ     pszString = NULL;
        ULONG   ulAttr;
        FILESTATUS3 fs3;
        PEABINDING  peab;       // \helpers\eas.c

        COUNTRYSETTINGS cs;
        prfhQueryCountrySettings(&cs);

        // get file-system object information
        // (we don't use the WPS method because the data
        // in there is frequently outdated)
        _wpQueryFilename(pcnbp->somSelf, szFilename, TRUE);
        DosQueryPathInfo(szFilename,
                        FIL_STANDARD,
                        &fs3, sizeof(fs3));

        // real name
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XSDI_FILES_REALNAME,
                        szFilename);

        // file size
        strhThousandsULong(szTemp, fs3.cbFile, cs.cThousands);
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XSDI_FILES_FILESIZE,
                        szTemp);

        // for folders: set work-area flag
        if (_somIsA(pcnbp->somSelf, _WPFolder))
            // this page is not for a folder, but
            // a data file:
            // hide "Work area" item
            winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_FILES_WORKAREA,
                                  ((_wpQueryFldrFlags(pcnbp->somSelf) & FOI_WORKAREA) != 0));

        // creation date/time
        strhFileDate(szTemp, &(fs3.fdateCreation), cs.ulDateFormat, cs.cDateSep);
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XSDI_FILES_CREATIONDATE,
                        szTemp);
        strhFileTime(szTemp, &(fs3.ftimeCreation), cs.ulTimeFormat, cs.cTimeSep);
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XSDI_FILES_CREATIONTIME,
                        szTemp);

        // last write date/time
        strhFileDate(szTemp, &(fs3.fdateLastWrite), cs.ulDateFormat, cs.cDateSep);
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XSDI_FILES_LASTWRITEDATE,
                        szTemp);
        strhFileTime(szTemp, &(fs3.ftimeLastWrite), cs.ulTimeFormat, cs.cTimeSep);
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XSDI_FILES_LASTWRITETIME,
                        szTemp);

        // last access date/time
        strhFileDate(szTemp, &(fs3.fdateLastAccess), cs.ulDateFormat, cs.cDateSep);
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XSDI_FILES_LASTACCESSDATE,
                        szTemp);
        strhFileTime(szTemp, &(fs3.ftimeLastAccess), cs.ulTimeFormat, cs.cTimeSep);
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XSDI_FILES_LASTACCESSTIME,
                        szTemp);

        // attributes
        ulAttr = _wpQueryAttr(pcnbp->somSelf);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage,
                              ID_XSDI_FILES_ATTR_ARCHIVED,
                              ((ulAttr & FILE_ARCHIVED) != 0));
        winhSetDlgItemChecked(pcnbp->hwndDlgPage,
                              ID_XSDI_FILES_ATTR_READONLY,
                              ((ulAttr & FILE_READONLY) != 0));
        winhSetDlgItemChecked(pcnbp->hwndDlgPage,
                              ID_XSDI_FILES_ATTR_HIDDEN,
                              ((ulAttr & FILE_HIDDEN) != 0));
        winhSetDlgItemChecked(pcnbp->hwndDlgPage,
                              ID_XSDI_FILES_ATTR_SYSTEM,
                              ((ulAttr & FILE_SYSTEM) != 0));

        // .SUBJECT EA; this is plain text
        pszString = NULL;
        peab = eaPathReadOneByName(szFilename, ".SUBJECT");
        if (peab)
        {
            pszString = eaCreatePSZFromBinding(peab);
            eaFreeBinding(peab);
        }
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XSDI_FILES_SUBJECT,
                          pszString);
        if (pszString)
            free(pszString);

        // .COMMENTS EA; this is multi-value multi-type, but all
        // of the sub-types are EAT_ASCII. We need to convert
        // the sub-items into one string and separate the items
        // with CR/LF.
        pszString = NULL;
        peab = eaPathReadOneByName(szFilename, ".COMMENTS");
        if (peab)
        {
            pszString = eaCreatePSZFromMVBinding(peab,
                                                 "\r\n", // separator string
                                                 NULL);  // codepage (not needed)
            eaFreeBinding(peab);
        }
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XSDI_FILES_COMMENTS,
                          pszString);
        if (pszString)
            free(pszString);

        // .KEYPHRASES EA; this is multi-value multi-type, but all
        // of the sub-types are EAT_ASCII. We need to convert
        // the sub-items into one string and separate the items
        // with CR/LF.
        pszString = NULL;
        peab = eaPathReadOneByName(szFilename, ".KEYPHRASES");
        if (peab)
        {
            pszString = eaCreatePSZFromMVBinding(peab,
                                                 "\r\n", // separator string
                                                 NULL);  // codepage (not needed)

            eaFreeBinding(peab);
        }
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XSDI_FILES_KEYPHRASES,
                        pszString);
        if (pszString)
            free(pszString);
    }

    if (flFlags & CBI_DESTROY)
    {
        // notebook page is being destroyed:
        // free the backup EAs we created before
        PFILEPAGEDATA   pfpd = pcnbp->pUser;

        eaFreeBinding(pfpd->peabSubject);
        eaFreeBinding(pfpd->peabComments);
        eaFreeBinding(pfpd->peabKeyphrases);

        // the pcnbp->pUser field itself is free()'d automatically
    }
}

/*
 *@@ fsysFile1ItemChanged:
 *      first "File" page notebook callback function (notebook.c).
 *      Reacts to changes of any of the dialog controls.
 *
 *      This is used by both XFolder and XFldDataFile.
 *
 *@@changed V0.9.1 (2000-01-22) [umoeller]: renamed from fsysFile1InitPage
 */

MRESULT fsysFile1ItemChanged(PCREATENOTEBOOKPAGE pcnbp,    // notebook info struct
                             USHORT usItemID,
                             USHORT usNotifyCode,
                             ULONG ulExtra)      // for checkboxes: contains new state
{
    BOOL fUpdate = TRUE;

    // CHAR    szFilename[CCHMAXPATH];

    switch (usItemID)
    {

        case ID_XSDI_FILES_WORKAREA:
            if (_somIsA(pcnbp->somSelf, _WPFolder))
            {
                ULONG ulFlags = _wpQueryFldrFlags(pcnbp->somSelf);
                if (ulExtra)
                    // checked:
                    ulFlags |= FOI_WORKAREA;
                else
                    ulFlags &= ~FOI_WORKAREA;
                _wpSetFldrFlags(pcnbp->somSelf, ulFlags);
            }
        break;

        /*
         * ID_XSDI_FILES_ATTR_ARCHIVED:
         * ID_XSDI_FILES_ATTR_READONLY:
         * ID_XSDI_FILES_ATTR_HIDDEN:
         * ID_XSDI_FILES_ATTR_SYSTEM:
         *      any "Attributes" flag clicked:
         *      change file attributes
         */

        case ID_XSDI_FILES_ATTR_ARCHIVED:
        case ID_XSDI_FILES_ATTR_READONLY:
        case ID_XSDI_FILES_ATTR_HIDDEN:
        case ID_XSDI_FILES_ATTR_SYSTEM:
        {
            ULONG       ulFileAttr;

            ulFileAttr = _wpQueryAttr(pcnbp->somSelf);

            // toggle file attribute
            ulFileAttr ^= // XOR flag depending on item checked
                      (usItemID == ID_XSDI_FILES_ATTR_ARCHIVED) ? FILE_ARCHIVED
                    : (usItemID == ID_XSDI_FILES_ATTR_SYSTEM  ) ? FILE_SYSTEM
                    : (usItemID == ID_XSDI_FILES_ATTR_HIDDEN  ) ? FILE_HIDDEN
                    : FILE_READONLY;

            _wpSetAttr(pcnbp->somSelf, ulFileAttr);

        break; }

        /*
         * ID_XSDI_FILES_SUBJECT:
         *      when focus leaves .SUBJECT entry field,
         *      rewrite plain EAT_ASCII EA
         */

        case ID_XSDI_FILES_SUBJECT:
            if (usNotifyCode == EN_KILLFOCUS)
            {
                PSZ         pszSubject = NULL;
                PEABINDING  peab;
                CHAR        szFilename[CCHMAXPATH];
                pszSubject = winhQueryWindowText(WinWindowFromID(pcnbp->hwndDlgPage,
                                                                 ID_XSDI_FILES_SUBJECT));
                if (peab = eaCreateBindingFromPSZ(".SUBJECT", pszSubject))
                {
                    _wpQueryFilename(pcnbp->somSelf, szFilename, TRUE);
                    eaPathWriteOne(szFilename, peab);
                    eaFreeBinding(peab);
                    if (pszSubject)
                        free(pszSubject);
                }
            }
        break;

        /*
         * ID_XSDI_FILES_COMMENTS:
         *      when focus leaves .COMMENTS MLE,
         *      rewrite EAT_MVMT EA
         */

        case ID_XSDI_FILES_COMMENTS:
            if (usNotifyCode == MLN_KILLFOCUS)
            {
                HWND    hwndMLE = WinWindowFromID(pcnbp->hwndDlgPage, usItemID);
                PSZ     pszText = winhQueryWindowText(hwndMLE);
                if (pszText)
                {
                    PEABINDING  peab;
                    CHAR        szFilename[CCHMAXPATH];
                    _wpQueryFilename(pcnbp->somSelf, szFilename, TRUE);

                    if (peab = eaCreateMVBindingFromPSZ(".COMMENTS",
                                                        pszText,
                                                        "\r\n",     // separator
                                                        0))         // codepage
                    {
                        // cmnDumpMemoryBlock(peab->pszValue, peab->usValueLength, 4);
                        eaPathWriteOne(szFilename, peab);
                        eaFreeBinding(peab);
                    }
                    free(pszText);
                }
            }
        break;

        /*
         * ID_XSDI_FILES_KEYPHRASES:
         *      when focus leaves .KEYPHRASES MLE,
         *      rewrite EAT_MVMT EA
         */

        case ID_XSDI_FILES_KEYPHRASES:
            if (usNotifyCode == MLN_KILLFOCUS)
            {
                HWND    hwndMLE = WinWindowFromID(pcnbp->hwndDlgPage, usItemID);
                PSZ     pszText = winhQueryWindowText(hwndMLE);
                if (pszText)
                {
                    PEABINDING  peab;
                    CHAR        szFilename[CCHMAXPATH];
                    _wpQueryFilename(pcnbp->somSelf, szFilename, TRUE);

                    if (peab = eaCreateMVBindingFromPSZ(".KEYPHRASES",
                                                        pszText,
                                                        "\r\n",     // separator
                                                        0))         // codepage
                    {
                        // cmnDumpMemoryBlock(peab->pszValue, peab->usValueLength, 4);
                        eaPathWriteOne(szFilename, peab);
                        eaFreeBinding(peab);
                    }
                    free(pszText);
                }
            }
        break;

        case DID_UNDO:
            if (pcnbp->pUser)
            {
                // restore the file's data from the backup data
                CHAR            szFilename[CCHMAXPATH];
                PFILEPAGEDATA   pfpd = (PFILEPAGEDATA)pcnbp->pUser;

                // reset attributes
                _wpSetAttr(pcnbp->somSelf, pfpd->ulAttr);

                // reset EAs
                _wpQueryFilename(pcnbp->somSelf, szFilename, TRUE);

                if (pfpd->peabSubject)
                    eaPathWriteOne(szFilename, pfpd->peabSubject);
                else
                    eaPathDeleteOne(szFilename, ".SUBJECT");

                if (pfpd->peabComments)
                    eaPathWriteOne(szFilename, pfpd->peabComments);
                else
                    eaPathDeleteOne(szFilename, ".COMMENTS");

                if (pfpd->peabKeyphrases)
                    eaPathWriteOne(szFilename, pfpd->peabKeyphrases);
                else
                    eaPathDeleteOne(szFilename, ".KEYPHRASES");

                // have the page updated by calling the callback above
                fsysFile1InitPage(pcnbp, CBI_SET | CBI_ENABLE);
            }
        break;

        case DID_DEFAULT:
        {
            // "Default" button:
            CHAR            szFilename[CCHMAXPATH];
            ULONG           ulAttr = 0;
            // EABINDING       eab;
            if (_somIsA(pcnbp->somSelf, _WPFolder))
                ulAttr = FILE_DIRECTORY;
            // reset attributes
            _wpSetAttr(pcnbp->somSelf, ulAttr);

            // delete EAs
            _wpQueryFilename(pcnbp->somSelf, szFilename, TRUE);
            eaPathDeleteOne(szFilename, ".SUBJECT");
            eaPathDeleteOne(szFilename, ".COMMENTS");
            eaPathDeleteOne(szFilename, ".KEYPHRASES");
            // have the page updated by calling the callback above
            fsysFile1InitPage(pcnbp, CBI_SET | CBI_ENABLE);
        break; }

        default:
            fUpdate = FALSE;
        break;
    }

    if (fUpdate)
        _wpSaveDeferred(pcnbp->somSelf);

    return ((MPARAM)-1);
}

/*
 *@@ fsysFile2InitPage:
 *      second "File" page notebook callback function (notebook.c).
 *      Sets the controls on the page according to a file's
 *      filesystem characteristics.
 *
 *      This is used by both XFolder and XFldDataFile.
 *
 *@@added V0.9.1 (2000-01-22) [umoeller]
 */

VOID fsysFile2InitPage(PCREATENOTEBOOKPAGE pcnbp,    // notebook info struct
                       ULONG flFlags)                // CBI_* flags (notebook.h)
{
    if (flFlags & CBI_INIT)
    {
        HWND hwndContents = WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_FILES_EACONTENTS);
        winhSetWindowFont(hwndContents, "8.Courier");
        WinSendMsg(hwndContents, MLM_SETREADONLY, (MPARAM)TRUE, 0);
    }

    if (flFlags & CBI_SET)
    {
        CHAR szFilename[CCHMAXPATH];
        if (_wpQueryFilename(pcnbp->somSelf, szFilename, TRUE))
        {
            PEALIST peal = eaPathReadAll(szFilename),
                    pealThis = peal;
            HWND hwndEAList = WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_FILES_EALIST);
            while (pealThis)
            {
                PEABINDING peabThis = pealThis->peab;
                if (peabThis)
                {
                    WinInsertLboxItem(hwndEAList,
                                      LIT_END,
                                      peabThis->pszName);
                }

                pealThis = pealThis->next;
            }

            eaFreeList(peal);
        }
    }
}

/*
 *@@ fsysFile2ItemChanged:
 *      second "File" page notebook callback function (notebook.c).
 *      Reacts to changes of any of the dialog controls.
 *
 *      This is used by both XFolder and XFldDataFile.
 *
 *@@added V0.9.1 (2000-01-22) [umoeller]
 */

MRESULT fsysFile2ItemChanged(PCREATENOTEBOOKPAGE pcnbp,    // notebook info struct
                             USHORT usItemID,
                             USHORT usNotifyCode,
                             ULONG ulExtra)      // for checkboxes: contains new state
{
    switch (usItemID)
    {
        /*
         * ID_XSDI_FILES_EALIST:
         *      EAs list box.
         */

        case ID_XSDI_FILES_EALIST:
            if (usNotifyCode == LN_SELECT)
            {
                HWND hwndEAList = WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_FILES_EALIST);
                ULONG ulSelection = (ULONG)WinSendMsg(hwndEAList,
                                                      LM_QUERYSELECTION,
                                                      MPNULL,
                                                      MPNULL);
                if (ulSelection != LIT_NONE)
                {
                    CHAR szFilename[CCHMAXPATH];
                    if (_wpQueryFilename(pcnbp->somSelf, szFilename, TRUE))
                    {
                        PSZ pszEAName = winhQueryLboxItemText(hwndEAList,
                                                              ulSelection);
                        if (pszEAName)
                        {
                            PEABINDING peab = eaPathReadOneByName(szFilename,
                                                                  pszEAName);
                            if (peab)
                            {
                                PSZ     pszInfo = NULL,
                                        pszContents = NULL;
                                USHORT  usEAType = eaQueryEAType(peab);
                                CHAR    szTemp[100];
                                BOOL    fDumpBinary = TRUE;

                                xstrcpy(&pszInfo, pszEAName);

                                switch (usEAType)
                                {
                                    case EAT_BINARY:
                                        xstrcat(&pszInfo, " (EAT_BINARY");
                                    break;

                                    case EAT_ASCII:
                                        xstrcat(&pszInfo, " (EAT_ASCII");
                                        pszContents = eaCreatePSZFromBinding(peab);
                                        fDumpBinary = FALSE;
                                    break;

                                    case EAT_BITMAP:
                                        xstrcat(&pszInfo, " (EAT_BITMAP");
                                    break;

                                    case EAT_METAFILE:
                                        xstrcat(&pszInfo, " (EAT_METAFILE");
                                    break;

                                    case EAT_ICON:
                                        xstrcat(&pszInfo, " (EAT_ICON");
                                    break;

                                    case EAT_EA:
                                        xstrcat(&pszInfo, " (EAT_EA");
                                    break;

                                    case EAT_MVMT:
                                        xstrcat(&pszInfo, " (EAT_MVMT");
                                    break;

                                    case EAT_MVST:
                                        xstrcat(&pszInfo, " (EAT_MVST");
                                    break;

                                    case EAT_ASN1:
                                        xstrcat(&pszInfo, " (EAT_ASN1");
                                    break;

                                    default:
                                    {
                                        sprintf(szTemp, " (type 0x%lX", usEAType);
                                        xstrcat(&pszInfo, szTemp);
                                    }
                                }

                                sprintf(szTemp, ", %d bytes)", peab->usValueLength);
                                xstrcat(&pszInfo, szTemp);

                                if (fDumpBinary)
                                {
                                    pszContents = strhCreateDump(peab->pszValue,
                                                                 peab->usValueLength,
                                                                 0);
                                }

                                // set static above MLE
                                WinSetDlgItemText(pcnbp->hwndDlgPage,
                                                  ID_XSDI_FILES_EAINFO,
                                                  pszInfo);

                                // set MLE; this might be empty
                                WinSetDlgItemText(pcnbp->hwndDlgPage,
                                                  ID_XSDI_FILES_EACONTENTS,
                                                  pszContents);

                                eaFreeBinding(peab);
                                if (pszInfo)
                                    free(pszInfo);
                                if (pszContents)
                                    free(pszContents);
                            }
                            free(pszEAName);
                        }
                    }
                }
            }
        break;
    }

    return (0);
}

/* ******************************************************************
 *                                                                  *
 *   XFldProgramFile notebook callbacks (notebook.c)                *
 *                                                                  *
 ********************************************************************/

/*
 *@@ fsysProgramInitPage:
 *      "Program" page notebook callback function (notebook.c).
 *      Sets the controls on the page according to the disk's
 *      characteristics.
 *
 *@@added V0.9.0 [umoeller]
 *@@todo: corresponding ItemChanged page
 */

VOID fsysProgramInitPage(PCREATENOTEBOOKPAGE pcnbp,    // notebook info struct
                         ULONG flFlags)                // CBI_* flags (notebook.h)
{
    // PGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    if (flFlags & CBI_INIT)
    {
    }

    if (flFlags & CBI_SET)
    {
        CHAR            szFilename[CCHMAXPATH] = "";

        ULONG           cbProgDetails = 0;
        PPROGDETAILS    pProgDetails;

        if (_wpQueryFilename(pcnbp->somSelf, szFilename, TRUE))
        {
            PEXECUTABLE     pExec = NULL;

            WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XSDI_PROG_FILENAME,
                              szFilename);

            if (doshExecOpen(szFilename, &pExec) == NO_ERROR)
            {
                PSZ     pszExeFormat = NULL,
                        pszOS = NULL;
                CHAR    szOS[400] = "";

                switch (pExec->ulExeFormat)
                {
                    case EXEFORMAT_OLDDOS:
                        pszExeFormat = "DOS 3.x";
                    break;

                    case EXEFORMAT_NE:
                        pszExeFormat = "New Executable (NE)";
                    break;

                    case EXEFORMAT_PE:
                        pszExeFormat = "Portable Executable (PE)";
                    break;

                    case EXEFORMAT_LX:
                        pszExeFormat = "Linear Executable (LX)";
                    break;
                }

                if (pszExeFormat)
                    WinSetDlgItemText(pcnbp->hwndDlgPage,
                                      ID_XSDI_PROG_EXEFORMAT,
                                      pszExeFormat);

                switch (pExec->ulOS)
                {
                    case EXEOS_DOS3:
                        pszOS = "DOS 3.x";
                    break;

                    case EXEOS_DOS4:
                        pszOS = "DOS 4.x";
                    break;

                    case EXEOS_OS2:
                        pszOS = "OS/2";
                    break;

                    case EXEOS_WIN16:
                        pszOS = "Win16";
                    break;

                    case EXEOS_WIN386:
                        pszOS = "Win386";
                    break;

                    case EXEOS_WIN32:
                        pszOS = "Win32";
                    break;
                }

                strcpy(szOS, pszOS);
                if (pExec->f32Bits)
                    strcat(szOS, " (32-bit)");
                else
                    strcat(szOS, " (16-bit)");

                if (pszOS)
                    WinSetDlgItemText(pcnbp->hwndDlgPage,
                                      ID_XSDI_PROG_TARGETOS,
                                      szOS);

                // now get buildlevel info
                if (doshExecQueryBldLevel(pExec) == NO_ERROR)
                {
                    if (pExec->pszVendor)
                    {
                        // has BLDLEVEL info:
                        WinSetDlgItemText(pcnbp->hwndDlgPage,
                                          ID_XSDI_PROG_VENDOR,
                                          pExec->pszVendor);
                        WinSetDlgItemText(pcnbp->hwndDlgPage,
                                          ID_XSDI_PROG_VERSION,
                                          pExec->pszVersion);
                        WinSetDlgItemText(pcnbp->hwndDlgPage,
                                          ID_XSDI_PROG_DESCRIPTION,
                                          pExec->pszInfo);
                    }
                    else
                        // no BLDLEVEL info:
                        WinSetDlgItemText(pcnbp->hwndDlgPage,
                                          ID_XSDI_PROG_DESCRIPTION,
                                          pExec->pszDescription);
                }

                doshExecClose(pExec);
            }
        } // end if (_wpQueryFilename...

        if ((_wpQueryProgDetails(pcnbp->somSelf, (PPROGDETAILS)NULL, &cbProgDetails)))
            if ((pProgDetails = (PPROGDETAILS)_wpAllocMem(pcnbp->somSelf,
                                                          cbProgDetails,
                                                          NULL)) != NULL)
            {
                _wpQueryProgDetails(pcnbp->somSelf, pProgDetails, &cbProgDetails);

                if (pProgDetails->pszParameters)
                    WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XSDI_PROG_PARAMETERS,
                                      pProgDetails->pszParameters);
                if (pProgDetails->pszStartupDir)
                    WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XSDI_PROG_WORKINGDIR,
                                      pProgDetails->pszStartupDir);

                _wpFreeMem(pcnbp->somSelf, (PBYTE)pProgDetails);
            }
    }
}


