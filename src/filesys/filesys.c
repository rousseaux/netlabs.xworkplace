
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

#define INCL_DOSSEMAPHORES
#define INCL_DOSPROCESS
#define INCL_DOSEXCEPTIONS
#define INCL_WINSTDCNR
#define INCL_DOSRESOURCES
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
#include <setjmp.h>

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\cnrh.h"               // container helper routines
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\eah.h"                // extended attributes helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\stringh.h"            // string helper routines
#include "helpers\threads.h"            // thread helpers
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
 *
 *   File system information implementation
 *
 ********************************************************************/

/*
 *@@ fsysQueryEASubject:
 *      returns the contents of the .SUBJECT extended
 *      attribute in a new buffer, which must be free()'d
 *      by the caller.
 *
 *      Returns NULL on errors or if the EA doesn't exist.
 *
 *      The .SUBJECT extended attribute is a plain PSZ
 *      without line breaks.
 *
 *@@added V0.9.7 (2000-11-30) [umoeller]
 */

PSZ fsysQueryEASubject(WPFileSystem *somSelf)
{
    PSZ     psz = 0;
    CHAR    szFilename[CCHMAXPATH];
    if (_wpQueryFilename(somSelf, szFilename, TRUE))
    {
        PEABINDING  peab = NULL;
        peab = eaPathReadOneByName(szFilename, ".SUBJECT");

        if (peab)
        {
            psz = eaCreatePSZFromBinding(peab);
            eaFreeBinding(peab);
        }
    }

    return (psz);
}

/*
 *@@ fsysQueryEAComments:
 *      returns the contents of the .COMMENTS extended
 *      attribute in a new buffer, which must be free()'d
 *      by the caller.
 *
 *      Returns NULL on errors or if the EA doesn't exist.
 *
 *      The .COMMENTS EA is multi-value multi-type, but all
 *      of the sub-types are EAT_ASCII. We convert all the
 *      sub-items into one string and separate the items
 *      with CR/LF.
 *
 *@@added V0.9.7 (2000-11-30) [umoeller]
 */

PSZ fsysQueryEAComments(WPFileSystem *somSelf)
{
    PSZ     psz = 0;
    CHAR    szFilename[CCHMAXPATH];
    if (_wpQueryFilename(somSelf, szFilename, TRUE))
    {
        PEABINDING  peab = NULL;

        peab = eaPathReadOneByName(szFilename, ".COMMENTS");
        if (peab)
        {
            psz = eaCreatePSZFromMVBinding(peab,
                                           "\r\n", // separator string
                                           NULL);  // codepage (not needed)
            eaFreeBinding(peab);
        }
    }

    return (psz);
}

/*
 *@@ fsysQueryEAKeyphrases:
 *      returns the contents of the .KEYPHRASES extended
 *      attribute in a new buffer, which must be free()'d
 *      by the caller.
 *
 *      Returns NULL on errors or if the EA doesn't exist.
 *
 *      The .KEYPHRASES EA is multi-value multi-type, but all
 *      of the sub-types are EAT_ASCII. We convert all the
 *      sub-items into one string and separate the items
 *      with CR/LF.
 *
 *@@added V0.9.7 (2000-11-30) [umoeller]
 */

PSZ fsysQueryEAKeyphrases(WPFileSystem *somSelf)
{
    PSZ     psz = 0;
    CHAR    szFilename[CCHMAXPATH];
    if (_wpQueryFilename(somSelf, szFilename, TRUE))
    {
        PEABINDING  peab = NULL;

        peab = eaPathReadOneByName(szFilename, ".KEYPHRASES");
        if (peab)
        {
            psz = eaCreatePSZFromMVBinding(peab,
                                           "\r\n", // separator string
                                           NULL);  // codepage (not needed)

            eaFreeBinding(peab);
        }
    }

    return (psz);
}

/*
 *@@ fsysSetEASubject:
 *      sets a new value for the .SUBJECT extended
 *      attribute.
 *
 *      If (psz == NULL), the EA is deleted.
 *
 *      This EA expects a plain PSZ string without
 *      line breaks.
 *
 *@@added V0.9.7 (2000-11-30) [umoeller]
 */

BOOL fsysSetEASubject(WPFileSystem *somSelf, const char *psz)
{
    BOOL brc = FALSE;
    CHAR    szFilename[CCHMAXPATH];

    if (_wpQueryFilename(somSelf, szFilename, TRUE))
    {
        const char *pcszEA = ".SUBJECT";
        if (psz)
        {
            PEABINDING  peab = NULL;
            if (peab = eaCreateBindingFromPSZ(pcszEA, psz))
            {
                brc = (NO_ERROR == eaPathWriteOne(szFilename, peab));
                eaFreeBinding(peab);
            }
        }
        else
            brc = (NO_ERROR == eaPathDeleteOne(szFilename, pcszEA));
    }

    return (brc);
}

/*
 *@@ fsysSetEAComments:
 *      sets a new value for the .COMMENTS extended
 *      attribute.
 *
 *      If (psz == NULL), the EA is deleted.
 *
 *      This EA is multi-value multi-type, but all of
 *      the sub-types are EAT_ASCII. This function
 *      expects a string where several lines are
 *      separated with CR/LF, which is then converted
 *      into the multi-value EA.
 *
 *@@added V0.9.7 (2000-11-30) [umoeller]
 */

BOOL fsysSetEAComments(WPFileSystem *somSelf, const char *psz)
{
    BOOL brc = FALSE;
    CHAR    szFilename[CCHMAXPATH];

    if (_wpQueryFilename(somSelf, szFilename, TRUE))
    {
        const char *pcszEA = ".COMMENTS";
        if (psz)
        {
            PEABINDING  peab = NULL;
            if (peab = eaCreateMVBindingFromPSZ(pcszEA,
                                                psz,
                                                "\r\n",     // separator
                                                0))         // codepage
            {
                brc = (NO_ERROR == eaPathWriteOne(szFilename, peab));
                eaFreeBinding(peab);
            }
        }
        else
            brc = (NO_ERROR == eaPathDeleteOne(szFilename, pcszEA));
    }

    return (brc);
}

/*
 *@@ fsysSetEAKeyphrases:
 *      sets a new value for the .KEYPHRASES extended
 *      attribute.
 *
 *      If (psz == NULL), the EA is deleted.
 *
 *      This EA is multi-value multi-type, but all of
 *      the sub-types are EAT_ASCII. This function
 *      expects a string where several lines are
 *      separated with CR/LF, which is then converted
 *      into the multi-value EA.
 *
 *@@added V0.9.7 (2000-11-30) [umoeller]
 */

BOOL fsysSetEAKeyphrases(WPFileSystem *somSelf, const char *psz)
{
    BOOL brc = FALSE;
    CHAR    szFilename[CCHMAXPATH];

    if (_wpQueryFilename(somSelf, szFilename, TRUE))
    {
        const char *pcszEA = ".KEYPHRASES";
        if (psz)
        {
            PEABINDING  peab = NULL;
            if (peab = eaCreateMVBindingFromPSZ(pcszEA,
                                                psz,
                                                "\r\n",     // separator
                                                0))         // codepage
            {
                brc = (NO_ERROR == eaPathWriteOne(szFilename, peab));
                eaFreeBinding(peab);
            }
        }
        else
            brc = (NO_ERROR == eaPathDeleteOne(szFilename, pcszEA));
    }

    return (brc);
}

/* ******************************************************************
 *
 *   "File" pages replacement in WPDataFile/WPFolder
 *
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
    /* PEABINDING  peabSubject;
    PEABINDING  peabComments;
    PEABINDING  peabKeyphrases; */
    PSZ         pszSubject,
                pszComments,
                pszKeyphrases;
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
 *
 *      --  The "Subject" field on the page corresponds to
 *          the .SUBJECT EA. This is a single-type EAT_ASCII EA.
 *
 *      --  The "Comments" field corresponds to the .COMMENTS EA.
 *          This is multi-value, multi-type (EAT_MVMT), but all the
 *          subvalues are of EAT_ASCII. All lines in the default
 *          WPS "File" multi-line entry field terminated by CR/LF
 *          are put in one of those subvalues.
 *
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
            memset(pfpd, 0, sizeof(FILEPAGEDATA));
            pcnbp->pUser = pfpd;
            pfpd->ulAttr = _wpQueryAttr(pcnbp->somSelf);
            _wpQueryFilename(pcnbp->somSelf, szFilename, TRUE);
            pfpd->pszSubject = fsysQueryEASubject(pcnbp->somSelf);
            pfpd->pszComments = fsysQueryEAComments(pcnbp->somSelf);
            pfpd->pszKeyphrases = fsysQueryEAKeyphrases(pcnbp->somSelf);

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
        // PEABINDING  peab;       // \helpers\eas.c

        PCOUNTRYSETTINGS pcs = cmnQueryCountrySettings(FALSE);

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
        strhThousandsULong(szTemp, fs3.cbFile, pcs->cThousands);
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XSDI_FILES_FILESIZE, szTemp);

        // for folders: set work-area flag
        if (_somIsA(pcnbp->somSelf, _WPFolder))
            // this page is not for a folder, but
            // a data file:
            // hide "Work area" item
            winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_FILES_WORKAREA,
                                  ((_wpQueryFldrFlags(pcnbp->somSelf) & FOI_WORKAREA) != 0));

        // creation date/time
        strhFileDate(szTemp, &(fs3.fdateCreation), pcs->ulDateFormat, pcs->cDateSep);
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XSDI_FILES_CREATIONDATE, szTemp);
        strhFileTime(szTemp, &(fs3.ftimeCreation), pcs->ulTimeFormat, pcs->cTimeSep);
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XSDI_FILES_CREATIONTIME, szTemp);

        // last write date/time
        strhFileDate(szTemp, &(fs3.fdateLastWrite), pcs->ulDateFormat, pcs->cDateSep);
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XSDI_FILES_LASTWRITEDATE, szTemp);
        strhFileTime(szTemp, &(fs3.ftimeLastWrite), pcs->ulTimeFormat, pcs->cTimeSep);
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XSDI_FILES_LASTWRITETIME, szTemp);

        // last access date/time
        strhFileDate(szTemp, &(fs3.fdateLastAccess), pcs->ulDateFormat, pcs->cDateSep);
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XSDI_FILES_LASTACCESSDATE, szTemp);
        strhFileTime(szTemp, &(fs3.ftimeLastAccess), pcs->ulTimeFormat, pcs->cTimeSep);
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XSDI_FILES_LASTACCESSTIME, szTemp);

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
        pszString = fsysQueryEASubject(pcnbp->somSelf);
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XSDI_FILES_SUBJECT,
                          pszString);
        if (pszString)
            free(pszString);

        // .COMMENTS EA; this is multi-value multi-type, but all
        // of the sub-types are EAT_ASCII. We need to convert
        // the sub-items into one string and separate the items
        // with CR/LF.
        pszString = fsysQueryEAComments(pcnbp->somSelf);
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XSDI_FILES_COMMENTS,
                          pszString);
        if (pszString)
            free(pszString);

        // .KEYPHRASES EA; this is multi-value multi-type, but all
        // of the sub-types are EAT_ASCII. We need to convert
        // the sub-items into one string and separate the items
        // with CR/LF.
        pszString = fsysQueryEAKeyphrases(pcnbp->somSelf);
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

        if (pfpd->pszSubject)
            free(pfpd->pszSubject);
        if (pfpd->pszComments)
            free(pfpd->pszComments);
        if (pfpd->pszKeyphrases)
            free(pfpd->pszKeyphrases);

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
                pszSubject = winhQueryWindowText(WinWindowFromID(pcnbp->hwndDlgPage,
                                                                 ID_XSDI_FILES_SUBJECT));
                fsysSetEASubject(pcnbp->somSelf, pszSubject);
                if (pszSubject)
                    free(pszSubject);
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
                fsysSetEAComments(pcnbp->somSelf, pszText);
                if (pszText)
                    free(pszText);
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
                fsysSetEAKeyphrases(pcnbp->somSelf, pszText);
                if (pszText)
                    free(pszText);
            }
        break;

        case DID_UNDO:
            if (pcnbp->pUser)
            {
                // restore the file's data from the backup data
                PFILEPAGEDATA   pfpd = (PFILEPAGEDATA)pcnbp->pUser;

                // reset attributes
                _wpSetAttr(pcnbp->somSelf, pfpd->ulAttr);

                // reset EAs
                fsysSetEASubject(pcnbp->somSelf, pfpd->pszSubject); // can be NULL
                fsysSetEAComments(pcnbp->somSelf, pfpd->pszComments); // can be NULL
                fsysSetEAKeyphrases(pcnbp->somSelf, pfpd->pszKeyphrases); // can be NULL

                // have the page updated by calling the callback above
                fsysFile1InitPage(pcnbp, CBI_SET | CBI_ENABLE);
            }
        break;

        case DID_DEFAULT:
        {
            // "Default" button:
            ULONG           ulAttr = 0;
            // EABINDING       eab;
            if (_somIsA(pcnbp->somSelf, _WPFolder))
                ulAttr = FILE_DIRECTORY;
            // reset attributes
            _wpSetAttr(pcnbp->somSelf, ulAttr);

            // delete EAs
            fsysSetEASubject(pcnbp->somSelf, NULL);
            fsysSetEAComments(pcnbp->somSelf, NULL);
            fsysSetEAKeyphrases(pcnbp->somSelf, NULL);

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
                                PSZ     pszContents = NULL;
                                XSTRING strInfo;
                                USHORT  usEAType = eaQueryEAType(peab);
                                CHAR    szTemp[100];
                                BOOL    fDumpBinary = TRUE;

                                xstrInit(&strInfo, 200);
                                xstrcpy(&strInfo, pszEAName);

                                switch (usEAType)
                                {
                                    case EAT_BINARY:
                                        xstrcat(&strInfo, " (EAT_BINARY");
                                    break;

                                    case EAT_ASCII:
                                        xstrcat(&strInfo, " (EAT_ASCII");
                                        pszContents = eaCreatePSZFromBinding(peab);
                                        fDumpBinary = FALSE;
                                    break;

                                    case EAT_BITMAP:
                                        xstrcat(&strInfo, " (EAT_BITMAP");
                                    break;

                                    case EAT_METAFILE:
                                        xstrcat(&strInfo, " (EAT_METAFILE");
                                    break;

                                    case EAT_ICON:
                                        xstrcat(&strInfo, " (EAT_ICON");
                                    break;

                                    case EAT_EA:
                                        xstrcat(&strInfo, " (EAT_EA");
                                    break;

                                    case EAT_MVMT:
                                        xstrcat(&strInfo, " (EAT_MVMT");
                                    break;

                                    case EAT_MVST:
                                        xstrcat(&strInfo, " (EAT_MVST");
                                    break;

                                    case EAT_ASN1:
                                        xstrcat(&strInfo, " (EAT_ASN1");
                                    break;

                                    default:
                                    {
                                        sprintf(szTemp, " (type 0x%lX", usEAType);
                                        xstrcat(&strInfo, szTemp);
                                    }
                                }

                                sprintf(szTemp, ", %d bytes)", peab->usValueLength);
                                xstrcat(&strInfo, szTemp);

                                if (fDumpBinary)
                                {
                                    pszContents = strhCreateDump(peab->pszValue,
                                                                 peab->usValueLength,
                                                                 0);
                                }

                                // set static above MLE
                                WinSetDlgItemText(pcnbp->hwndDlgPage,
                                                  ID_XSDI_FILES_EAINFO,
                                                  strInfo.psz);

                                // set MLE; this might be empty
                                WinSetDlgItemText(pcnbp->hwndDlgPage,
                                                  ID_XSDI_FILES_EACONTENTS,
                                                  pszContents);

                                eaFreeBinding(peab);
                                xstrClear(&strInfo);
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

/*
 *@@ fsysInsertFilePages:
 *      shared code between XFldDataFile and XFolder
 *      to insert the new "File" pages if this is
 *      enabled. This gets called from the respective
 *      wpAddFile1Page methods.
 *
 *@@added V0.9.5 (2000-08-14) [umoeller]
 */

ULONG fsysInsertFilePages(WPObject *somSelf,    // in: must be a WPFileSystem, really
                          HWND hwndNotebook)    // in: from wpAddFile1Page
{
    // page 2
    PCREATENOTEBOOKPAGE pcnbp = malloc(sizeof(CREATENOTEBOOKPAGE));
    PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();

    memset(pcnbp, 0, sizeof(CREATENOTEBOOKPAGE));
    pcnbp->somSelf = somSelf;
    pcnbp->hwndNotebook = hwndNotebook;
    pcnbp->hmod = cmnQueryNLSModuleHandle(FALSE);
    pcnbp->ulDlgID = ID_XSD_FILESPAGE2;
    pcnbp->ulPageID = SP_FILE2;
    pcnbp->pszName = pNLSStrings->pszFilePage;
    pcnbp->fEnumerate = TRUE;
    pcnbp->ulDefaultHelpPanel  = ID_XSH_SETTINGS_FILEPAGE2;

    pcnbp->pfncbInitPage    = (PFNCBACTION)fsysFile2InitPage;
    pcnbp->pfncbItemChanged = (PFNCBITEMCHANGED)fsysFile2ItemChanged;

    ntbInsertPage(pcnbp);

    // page 1
    pcnbp = malloc(sizeof(CREATENOTEBOOKPAGE));
    memset(pcnbp, 0, sizeof(CREATENOTEBOOKPAGE));
    pcnbp->somSelf = somSelf;
    pcnbp->hwndNotebook = hwndNotebook;
    pcnbp->hmod = cmnQueryNLSModuleHandle(FALSE);
    pcnbp->ulDlgID = ID_XSD_FILESPAGE1;
    pcnbp->ulPageID = SP_FILE1;
    pcnbp->usPageStyleFlags = BKA_MAJOR;
    pcnbp->pszName = pNLSStrings->pszFilePage;
    pcnbp->fEnumerate = TRUE;
    pcnbp->ulDefaultHelpPanel  = ID_XSH_SETTINGS_FILEPAGE1;

    pcnbp->pfncbInitPage    = (PFNCBACTION)fsysFile1InitPage;
    pcnbp->pfncbItemChanged = (PFNCBITEMCHANGED)fsysFile1ItemChanged;

    return (ntbInsertPage(pcnbp));
}

/* ******************************************************************
 *
 *   XFldProgramFile notebook callbacks (notebook.c)
 *
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
                                                          NULL))
                    != NULL)
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

/*
 *@@ RESOURCERECORD:
 *
 *@@added V0.9.7 (2000-12-17) [lafaix]
 */

typedef struct _RESOURCERECORD
{
    RECORDCORE  recc;

    ULONG ulResourceID; // !!! Could be a string with Windows or Open32 execs
    PSZ   pszResourceType;
    ULONG ulResourceSize;
} RESOURCERECORD, *PRESOURCERECORD;

/*
 *@@ FSYSRESOURCE:
 *
 *@@added V0.9.7 (2000-12-18) [lafaix]
 */

typedef struct _FSYSRESOURCE
{
    ULONG ulID;
    ULONG ulType;
    ULONG ulSize;
} FSYSRESOURCE, *PFSYSRESOURCE;

/*
 *@@ fsysQueryResources:
 *      returns an array of FSYSRESOURCE structures describing all
 *      available resources in the module.
 *
 *      *pcResources receives the no. of items in the array
 *      (not the array size!). Use fsysFreeResources to clean up.
 *
 *@@added V0.9.7 (2000-12-18) [lafaix]
 */

PFSYSRESOURCE fsysQueryResources(PEXECUTABLE pExec, PULONG pcResources)
{
    ULONG         cResources = 0;
    PFSYSRESOURCE paResources = NULL;
    int i;

    if (pExec)
    {
        if (pExec->ulOS == EXEOS_OS2)
        {
            ULONG ulDummy;

            if (pExec->ulExeFormat == EXEFORMAT_LX)
            {
                // It's a 32bit OS/2 executable
                cResources = pExec->pLXHeader->ulResTblCnt;

                if (cResources)
                {
                    struct rsrc32                  /* Resource Table Entry */
                    {
                        unsigned short      type;   /* Resource type */
                        unsigned short      name;   /* Resource name */
                        unsigned long       cb;     /* Resource size */
                        unsigned short      obj;    /* Object number */
                        unsigned long       offset; /* Offset within object */
                    } rs;

                    paResources = malloc(sizeof(FSYSRESOURCE) * cResources);

                    DosSetFilePtr(pExec->hfExe,
                                  pExec->pLXHeader->ulResTblOfs + pExec->pDosExeHeader->ulNewHeaderOfs,
                                  FILE_BEGIN,
                                  &ulDummy);

                    for (i = 0; i < cResources; i++)
                    {
                        DosRead(pExec->hfExe, &rs, 14, &ulDummy);
                        paResources[i].ulID = rs.name;
                        paResources[i].ulType = rs.type;
                        paResources[i].ulSize = rs.cb;
                    }
                }
            }
            else
            if (pExec->ulExeFormat == EXEFORMAT_NE)
            {
               // It's a 16bit OS/2 executable
               cResources = pExec->pNEHeader->usResSegmCount;

               if (cResources)
               {
                   struct {unsigned short type; unsigned short name;} rti;
                   struct new_seg                          /* New .EXE segment table entry */
                   {
                       unsigned short      ns_sector;      /* File sector of start of segment */
                       unsigned short      ns_cbseg;       /* Number of bytes in file */
                       unsigned short      ns_flags;       /* Attribute flags */
                       unsigned short      ns_minalloc;    /* Minimum allocation in bytes */
                   } ns;

                   paResources = malloc(sizeof(FSYSRESOURCE) * cResources);

                   // We first read the resources IDs and types
                   DosSetFilePtr(pExec->hfExe,
                                 pExec->pNEHeader->usResTblOfs + pExec->pDosExeHeader->ulNewHeaderOfs,
                                 FILE_BEGIN,
                                 &ulDummy);

                   for (i = 0; i < cResources; i++)
                   {
                       DosRead(pExec->hfExe, &rti, sizeof(rti), &ulDummy);
                       paResources[i].ulID = rti.name;
                       paResources[i].ulType = rti.type;
                   }

                   // And we then read their sizes
                   for (i = 0; i < cResources; i++)
                   {
                       DosSetFilePtr(pExec->hfExe,
                                     pExec->pDosExeHeader->ulNewHeaderOfs+pExec->pNEHeader->usSegTblOfs+sizeof(ns)*i,
                                     FILE_BEGIN,
                                     &ulDummy);
                       DosRead(pExec->hfExe, &ns, sizeof(ns), &ulDummy);
                       paResources[i].ulSize = ns.ns_cbseg;
                   }
               }
            }

            *pcResources = cResources;
        }

        doshExecClose(pExec);
    }

    return (paResources);
}

/*
 *@@ fsysFreeResources:
 *      frees resources allocated by fsysQueryResources.
 *
 *@@added V0.9.7 (2000-12-18) [lafaix]
 */

BOOL fsysFreeResources(PFSYSRESOURCE paResources)
{
    free(paResources);
    return (TRUE);
}

/*
 *@@fsysGetResourceTypeName:
 *      returns a human-readable name from a resource type.
 *
 *@@added V0.9.7 (2000-12-20) [lafaix]
 */
PSZ fsysGetResourceTypeName(ULONG ulResourceType)
{
    if (ulResourceType == RT_POINTER)
        return "RT_POINTER";
    if (ulResourceType == RT_BITMAP)
        return "RT_BITMAP";
    if (ulResourceType == RT_MENU)
        return "RT_MENU";
    if (ulResourceType == RT_DIALOG)
        return "RT_DIALOG";
    if (ulResourceType == RT_STRING)
        return "RT_STRING";
    if (ulResourceType == RT_FONTDIR)
        return "RT_FONTDIR";
    if (ulResourceType == RT_FONT)
        return "RT_FONT";
    if (ulResourceType == RT_ACCELTABLE)
        return "RT_ACCELTABLE";
    if (ulResourceType == RT_RCDATA)
        return "RT_RCDATA";
    if (ulResourceType == RT_MESSAGE)
        return "RT_MESSAGE";
    if (ulResourceType == RT_DLGINCLUDE)
        return "RT_DLGINCLUDE";
    if (ulResourceType == RT_VKEYTBL)
        return "RT_VKEYTBL";
    if (ulResourceType == RT_KEYTBL)
        return "RT_KEYTBL";
    if (ulResourceType == RT_CHARTBL)
        return "RT_CHARTBL";
    if (ulResourceType == RT_DISPLAYINFO)
        return "RT_DISPLAYINFO";

    if (ulResourceType == RT_FKASHORT)
        return "RT_FKASHORT";
    if (ulResourceType == RT_FKALONG)
        return "RT_FKALONG";

    if (ulResourceType == RT_HELPTABLE)
        return "RT_HELPTABLE";
    if (ulResourceType == RT_HELPSUBTABLE)
        return "RT_HELPSUBTABLE";

    if (ulResourceType == RT_FDDIR)
        return "RT_FDDIR";
    if (ulResourceType == RT_FD)
        return "RT_FD";

    return "Unknown";
}

/*
 *@@ fntInsertResources:
 *      transient thread started by fsysResourcesInitPage
 *      to insert resources into the "Resources" container.
 *
 *      This thread is created with a msg queue.
 *
 *@@added V0.9.7 (2000-12-17) [lafaix]
 */

void _Optlink fntInsertResources(PTHREADINFO pti)
{
    PCREATENOTEBOOKPAGE pcnbp = (PCREATENOTEBOOKPAGE)(pti->ulData);

    TRY_LOUD(excpt1)
    {
        ULONG         cResources = 0,
                      ul;
        PFSYSRESOURCE paResources;
        CHAR          szFilename[CCHMAXPATH] = "";

        pcnbp->fShowWaitPointer = TRUE;

        if (_wpQueryFilename(pcnbp->somSelf, szFilename, TRUE))
        {
            PEXECUTABLE     pExec = NULL;

            if (doshExecOpen(szFilename, &pExec) == NO_ERROR)
            {
                paResources = fsysQueryResources(pExec, &cResources);

                if (paResources)
                {
                    HWND hwndCnr = WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_PROG_RESOURCES);

                    for (ul = 0;
                         ul < cResources;
                         ul++)
                    {
                        PRESOURCERECORD precc
                            = (PRESOURCERECORD)cnrhAllocRecords(hwndCnr,
                                                                sizeof(RESOURCERECORD),
                                                                1);
                        if (precc)
                        {
                            precc->ulResourceID = paResources[ul].ulID;
                            precc->pszResourceType = fsysGetResourceTypeName(paResources[ul].ulType);
                            precc->ulResourceSize = paResources[ul].ulSize;

                            cnrhInsertRecords(hwndCnr,
                                              NULL,
                                              (PRECORDCORE)precc,
                                              TRUE, // invalidate
                                              NULL,
                                              CRA_RECORDREADONLY,
                                              1);
                        }
                    }
                }

                // store resources
                if (pcnbp->pUser)
                    fsysFreeResources(pcnbp->pUser);
                pcnbp->pUser = paResources;
            }
        }
    }
    CATCH(excpt1) {}  END_CATCH();

    pcnbp->fShowWaitPointer = FALSE;
}

/*
 *@@ fsysResourcesInitPage:
 *      "Resources" page notebook callback function (notebook.c).
 *
 *@@added V0.9.7 (2000-12-17) [lafaix]
 *@@todo: corresponding ItemChanged page
 */

VOID fsysResourcesInitPage(PCREATENOTEBOOKPAGE pcnbp,    // notebook info struct
                           ULONG flFlags)                // CBI_* flags (notebook.h)
{
    // PGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    HWND hwndCnr = WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_PROG_RESOURCES);

    /*
     * CBI_INIT:
     *      initialize page (called only once)
     */

    if (flFlags & CBI_INIT)
    {
        XFIELDINFO xfi[5];
        PFIELDINFO pfi = NULL;
        int        i = 0;

        // set up cnr details view
/* !!! not yet implemented [lafaix]
        xfi[i].ulFieldOffset = FIELDOFFSET(RESOURCERECORD, pszDeviceType);
        xfi[i].pszColumnTitle = "Icon"; // !!! to be localized
        xfi[i].ulDataType = CFA_BITMAPORICON;
        xfi[i++].ulOrientation = CFA_LEFT;
*/

        xfi[i].ulFieldOffset = FIELDOFFSET(RESOURCERECORD, ulResourceID);
        xfi[i].pszColumnTitle = "ID"; // !!! to be localized
        xfi[i].ulDataType = CFA_ULONG;
        xfi[i++].ulOrientation = CFA_RIGHT;

        xfi[i].ulFieldOffset = FIELDOFFSET(RESOURCERECORD, pszResourceType);
        xfi[i].pszColumnTitle = "Type"; // !!! to be localized
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        xfi[i].ulFieldOffset = FIELDOFFSET(RESOURCERECORD, ulResourceSize);
        xfi[i].pszColumnTitle = "Size"; // !!! to be localized
        xfi[i].ulDataType = CFA_ULONG;
        xfi[i++].ulOrientation = CFA_LEFT;

        pfi = cnrhSetFieldInfos(hwndCnr,
                                xfi,
                                i,             // array item count
                                TRUE,          // draw lines
                                1);            // return first column

        BEGIN_CNRINFO()
        {
            cnrhSetView(CV_DETAIL | CA_DETAILSVIEWTITLES);
        } END_CNRINFO(hwndCnr);
    }

    /*
     * CBI_SET:
     *      set controls' data
     */

    if (flFlags & CBI_SET)
    {
        // fill container with resources
        thrCreate(NULL,
                  fntInsertResources,
                  NULL, // running flag
                  THRF_PMMSGQUEUE | THRF_TRANSIENT,
                  (ULONG)pcnbp);
    }

    /*
     * CBI_DESTROY:
     *      clean up page before destruction
     */

    if (flFlags & CBI_DESTROY)
    {
        if (pcnbp->pUser)
            fsysFreeResources(pcnbp->pUser);
        pcnbp->pUser = NULL;
    }
}

/* ******************************************************************
 *                                                                  *
 *   WPProgram / XFldProgramFile setup strings                      *
 *                                                                  *
 ********************************************************************/

/*
 *@@ fsysQueryProgramSetup:
 *      called to retrieve a setup string for programs.
 *
 *      -- For WPProgram, this gets called from objQuerySetup
 *         directly because at this point we have no class
 *         replacement for WPProgram.
 *
File *      -- For XFldProgramFile, this gets called by
 *         fsysQueryProgramFileSetup.
 *
 *@@added V0.9.4 (2000-08-02) [umoeller]
 */

VOID fsysQueryProgramSetup(WPObject *somSelf, // in: WPProgram or WPProgramFile
                           PXSTRING pstrTemp)   // in: string to append to (xstrcat)
{
    PSZ pszValue = NULL;
    ULONG ulSize = 0;

    // wpQueryProgDetails: supported by both WPProgram and WPProgramFile
    if ((_wpQueryProgDetails(somSelf, (PPROGDETAILS)NULL, &ulSize)))
    {
        PPROGDETAILS    pProgDetails = NULL;
        if ((pProgDetails = (PPROGDETAILS)malloc(ulSize)) != NULL)
        {
            if ((_wpQueryProgDetails(somSelf, pProgDetails, &ulSize)))
            {
                PSZ pszProgString = NULL;

                // EXENAME: skip for WPProgramFile
                if (!_somIsA(somSelf, _WPProgramFile))
                {
                    if (pProgDetails->pszExecutable)
                    {
                        xstrcat(pstrTemp, "EXENAME=");
                        xstrcat(pstrTemp, pProgDetails->pszExecutable);
                        xstrcat(pstrTemp, ";");

                        switch (pProgDetails->progt.progc)
                        {
                            // PROGTYPE=DOSMODE
                            // PROGTYPE=PROG_30_STD
                            // PROGTYPE=SEPARATEWIN
                            // PROGTYPE=WIN
                            // PROGTYPE=WINDOWEDWIN

                            // case PROG_DEFAULT:
                                    // Default application, no setup string
                            case PROG_PM:
                                    // Presentation Manager application.
                                    // PROGTYPE=PM
                                pszProgString = "PROGTYPE=PM;";
                                break;
                            case PROG_WINDOWABLEVIO:
                                    // Text-windowed application.
                                    // PROGTYPE=WINDOWABLEVIO
                                pszProgString = "PROGTYPE=WINDOWABLEVIO;";
                                break;
                            case PROG_FULLSCREEN:
                                    // Full-screen application.
                                    // PROGTYPE=FULLSCREEN
                                pszProgString = "PROGTYPE=FULLSCREEN;";
                                break;
                            case PROG_WINDOWEDVDM:
                                    // PC DOS executable process (windowed).
                                    // PROGTYPE=WINDOWEDVDM
                                pszProgString = "PROGTYPE=WINDOWEDVDM;";
                                break;
                            case PROG_VDM:
                                    // PC DOS executable process (full screen).
                                    // PROGTYPE=VDM
                                pszProgString = "PROGTYPE=VDM;";
                                break;
                            // case PROG_REAL:
                                    // PC DOS executable process (full screen). Same as PROG_VDM.
                            case PROG_31_STDSEAMLESSVDM:
                                    // Windows 3.1 program that will execute in its own windowed
                                    // WINOS2 session.
                                    // PROGTYPE=PROG_31_STDSEAMLESSVDM
                                pszProgString = "PROGTYPE=PROG_31_STDSEAMLESSVDM;";
                                break;
                            case PROG_31_STDSEAMLESSCOMMON:
                                    // Windows 3.1 program that will execute in a common windowed
                                    // WINOS2 session.
                                    // PROGTYPE=PROG_31_STDSEAMLESSCOMMON
                                pszProgString = "PROGTYPE=PROG_31_STDSEAMLESSCOMMON;";
                                break;
                            case PROG_31_ENHSEAMLESSVDM:
                                    // Windows 3.1 program that will execute in enhanced compatibility
                                    // mode in its own windowed WINOS2 session.
                                    // PROGTYPE=PROG_31_ENHSEAMLESSVDM
                                pszProgString = "PROGTYPE=PROG_31_ENHSEAMLESSVDM;";
                                break;
                            case PROG_31_ENHSEAMLESSCOMMON:
                                    // Windows 3.1 program that will execute in enhanced compatibility
                                    // mode in a common windowed WINOS2 session.
                                    // PROGTYPE=PROG_31_ENHSEAMLESSCOMMON
                                pszProgString = "PROGTYPE=PROG_31_ENHSEAMLESSCOMMON;";
                                break;
                            case PROG_31_ENH:
                                    // Windows 3.1 program that will execute in enhanced compatibility
                                    // mode in a full-screen WINOS2 session.
                                    // PROGTYPE=PROG_31_ENH
                                pszProgString = "PROGTYPE=PROG_31_ENH;";
                                break;
                            case PROG_31_STD:
                                    // Windows 3.1 program that will execute in a full-screen WINOS2
                                    // session.
                                    // PROGTYPE=PROG_31_STD
                                pszProgString = "PROGTYPE=PROG_31_STD;";
                        }

                        if (pszProgString)
                            xstrcat(pstrTemp, pszProgString);
                    }
                } // end if (_somIsA(somSelf, _WPProgram)

                // PARAMETERS=
                if (pProgDetails->pszParameters)
                    if (strlen(pProgDetails->pszParameters))
                    {
                        xstrcat(pstrTemp, "PARAMETERS=");
                        xstrcat(pstrTemp, pProgDetails->pszParameters);
                        xstrcat(pstrTemp, ";");
                    }

                // STARTUPDIR=
                if (pProgDetails->pszStartupDir)
                    if (strlen(pProgDetails->pszStartupDir))
                    {
                        xstrcat(pstrTemp, "STARTUPDIR=");
                        xstrcat(pstrTemp, pProgDetails->pszStartupDir);
                        xstrcat(pstrTemp, ";");
                    }

                // SET XXX=VVV
                if (pProgDetails->pszEnvironment)
                {
                    // this is one of those typical OS/2 environment
                    // arrays, so lets parse this
                    DOSENVIRONMENT Env = {0};
                    if (doshParseEnvironment(pProgDetails->pszEnvironment,
                                             &Env)
                            == NO_ERROR)
                    {
                        if (Env.papszVars)
                        {
                            // got the strings: parse them
                            ULONG ul = 0;
                            PSZ *ppszThis = Env.papszVars;
                            for (ul = 0;
                                 ul < Env.cVars;
                                 ul++)
                            {
                                PSZ pszThis = *ppszThis;

                                xstrcat(pstrTemp, "SET ");
                                xstrcat(pstrTemp, pszThis);
                                xstrcat(pstrTemp, ";");

                                // next environment string
                                ppszThis++;
                            }
                        }
                        doshFreeEnvironment(&Env);
                    }
                }

                // MAXIMIZED=YES|NO

                // MINIMIZED=YES|NO

                // NOAUTOCLOSE=YES|NO
            }

            free(pProgDetails);
        }
    } // end if _wpQueryProgDetails

    // ASSOCFILTER
    if ((pszValue = _wpQueryAssociationFilter(somSelf)))
            // wpQueryAssociationFilter:
            // supported by both WPProgram and WPProgramFile
    {
        xstrcat(pstrTemp, "ASSOCFILTER=");
        xstrcat(pstrTemp, pszValue);
        xstrcat(pstrTemp, ";");
    }

    // ASSOCTYPE
    if ((pszValue = _wpQueryAssociationType(somSelf)))
            // wpQueryAssociationType:
            // supported by both WPProgram and WPProgramFile
    {
        xstrcat(pstrTemp, "ASSOCTYPE=");
        xstrcat(pstrTemp, pszValue);
        xstrcat(pstrTemp, ";");
    }
}

/*
 *@@ fsysQueryProgramFileSetup:
 *      called by XFldProgramFile::xwpQuerySetup2.
 *
 *@@added V0.9.4 (2000-08-02) [umoeller]
 */

ULONG fsysQueryProgramFileSetup(WPObject *somSelf,
                                PSZ pszSetupString,     // in: buffer for setup string or NULL
                                ULONG cbSetupString)    // in: size of that buffer or 0
{
    // temporary buffer for building the setup string
    XSTRING strTemp;
    ULONG   ulReturn = 0;

    xstrInit(&strTemp, 1000);

    fsysQueryProgramSetup(somSelf,
                          &strTemp);

    /*
     * append string
     *
     */

    if (strTemp.ulLength)
    {
        // return string if buffer is given
        if ((pszSetupString) && (cbSetupString))
            strhncpy0(pszSetupString,   // target
                      strTemp.psz,      // source
                      cbSetupString);   // buffer size

        // always return length of string
        ulReturn = strTemp.ulLength;
    }

    xstrClear(&strTemp);

    return (ulReturn);
}

