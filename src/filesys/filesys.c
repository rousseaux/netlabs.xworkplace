
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
 *      Copyright (C) 1997-2001 Ulrich M”ller.
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

#pragma strings(readonly)

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
#include "helpers\apps.h"               // application helpers
#include "helpers\cnrh.h"               // container helper routines
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\eah.h"                // extended attributes helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\nls.h"                // National Language Support helpers
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\stringh.h"            // string helper routines
#include "helpers\textview.h"           // PM XTextView control
#include "helpers\threads.h"            // thread helpers
#include "helpers\winh.h"               // PM helper routines
#include "helpers\xstring.h"            // extended string helpers

// SOM headers which don't crash with prec. header files
#include "xfldr.ih"
#include "xfpgmf.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\helppanels.h"          // all XWorkplace help panel IDs
#include "shared\notebook.h"            // generic XWorkplace notebook handling
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

#include "filesys\filesys.h"            // various file-system object implementation code
#include "filesys\program.h"            // program implementation

// other SOM headers
#pragma hdrstop                 // VAC++ keeps crashing otherwise
// #include <wpdesk.h>             // this includes wpfolder.h

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
            PEABINDING  peab;
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
 *@@ fsysQueryRefreshFlags:
 *
 *@@added V0.9.16 (2001-10-28) [umoeller]
 */

ULONG fsysQueryRefreshFlags(WPFileSystem *somSelf)
{
    static xfTD_wpQueryRefreshFlags pwpQueryRefreshFlags = NULL;

    if (!pwpQueryRefreshFlags)
        pwpQueryRefreshFlags = (xfTD_wpQueryRefreshFlags)wpshResolveFor(
                                                 somSelf,
                                                 NULL, // use somSelf's class
                                                 "wpQueryRefreshFlags");
    if (pwpQueryRefreshFlags)
        return (pwpQueryRefreshFlags(somSelf));

    return (0);
}

/*
 *@@ fsysSetRefreshFlags:
 *
 *@@added V0.9.16 (2001-10-28) [umoeller]
 */

BOOL fsysSetRefreshFlags(WPFileSystem *somSelf, ULONG ulRefreshFlags)
{
    static xfTD_wpSetRefreshFlags pwpSetRefreshFlags = NULL;

    if (!pwpSetRefreshFlags)
        pwpSetRefreshFlags = (xfTD_wpSetRefreshFlags)wpshResolveFor(
                                                 somSelf,
                                                 NULL, // use somSelf's class
                                                 "wpSetRefreshFlags");
    if (pwpSetRefreshFlags)
        return (pwpSetRefreshFlags(somSelf, ulRefreshFlags));

    return (FALSE);
}

/*
 *@@ fsysRefreshFSInfo:
 *
 *@@added V0.9.16 (2001-10-28) [umoeller]
 */

BOOL fsysRefreshFSInfo(WPFileSystem *somSelf,
                       PFILEFINDBUF3 pfb3)      // in: new file info or NULL
{
    BOOL brc = FALSE;

    xfTD_wpRefreshFSInfo pwpRefreshFSInfo;

    if (pwpRefreshFSInfo = (xfTD_wpRefreshFSInfo)wpshResolveFor(
                                                 somSelf,
                                                 NULL, // use somSelf's class
                                                 "wpRefreshFSInfo"))
        brc = pwpRefreshFSInfo(somSelf,
                               0,
                               pfb3,
                               TRUE);

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
            else if (cmnIsADesktop(pcnbp->somSelf))
                // for the Desktop, disable work area;
                // this must not be changed
                winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_FILES_WORKAREA,
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
        nlsThousandsULong(szTemp, fs3.cbFile, pcs->cThousands);
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XSDI_FILES_FILESIZE, szTemp);

        // for folders: set work-area flag
        if (_somIsA(pcnbp->somSelf, _WPFolder))
            // this page is not for a folder, but
            // a data file:
            // hide "Work area" item
            winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_FILES_WORKAREA,
                                  ((_wpQueryFldrFlags(pcnbp->somSelf) & FOI_WORKAREA) != 0));

        // creation date/time
        nlsFileDate(szTemp, &(fs3.fdateCreation), pcs->ulDateFormat, pcs->cDateSep);
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XSDI_FILES_CREATIONDATE, szTemp);
        nlsFileTime(szTemp, &(fs3.ftimeCreation), pcs->ulTimeFormat, pcs->cTimeSep);
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XSDI_FILES_CREATIONTIME, szTemp);

        // last write date/time
        nlsFileDate(szTemp, &(fs3.fdateLastWrite), pcs->ulDateFormat, pcs->cDateSep);
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XSDI_FILES_LASTWRITEDATE, szTemp);
        nlsFileTime(szTemp, &(fs3.ftimeLastWrite), pcs->ulTimeFormat, pcs->cTimeSep);
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XSDI_FILES_LASTWRITETIME, szTemp);

        // last access date/time
        nlsFileDate(szTemp, &(fs3.fdateLastAccess), pcs->ulDateFormat, pcs->cDateSep);
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XSDI_FILES_LASTACCESSDATE, szTemp);
        nlsFileTime(szTemp, &(fs3.ftimeLastAccess), pcs->ulTimeFormat, pcs->cTimeSep);
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
                             ULONG ulItemID,
                             USHORT usNotifyCode,
                             ULONG ulExtra)      // for checkboxes: contains new state
{
    BOOL fUpdate = TRUE;

    // CHAR    szFilename[CCHMAXPATH];

    switch (ulItemID)
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
                      (ulItemID == ID_XSDI_FILES_ATTR_ARCHIVED) ? FILE_ARCHIVED
                    : (ulItemID == ID_XSDI_FILES_ATTR_SYSTEM  ) ? FILE_SYSTEM
                    : (ulItemID == ID_XSDI_FILES_ATTR_HIDDEN  ) ? FILE_HIDDEN
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
                HWND    hwndMLE = WinWindowFromID(pcnbp->hwndDlgPage, ulItemID);
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
                HWND    hwndMLE = WinWindowFromID(pcnbp->hwndDlgPage, ulItemID);
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

#ifndef __NOFILEPAGE2__

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
                             ULONG ulItemID,
                             USHORT usNotifyCode,
                             ULONG ulExtra)      // for checkboxes: contains new state
{
    switch (ulItemID)
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
                                xstrcpy(&strInfo, pszEAName, 0);

                                switch (usEAType)
                                {
                                    case EAT_BINARY:
                                        xstrcat(&strInfo, " (EAT_BINARY", 0);
                                    break;

                                    case EAT_ASCII:
                                        xstrcat(&strInfo, " (EAT_ASCII", 0);
                                        pszContents = eaCreatePSZFromBinding(peab);
                                        fDumpBinary = FALSE;
                                    break;

                                    case EAT_BITMAP:
                                        xstrcat(&strInfo, " (EAT_BITMAP", 0);
                                    break;

                                    case EAT_METAFILE:
                                        xstrcat(&strInfo, " (EAT_METAFILE", 0);
                                    break;

                                    case EAT_ICON:
                                        xstrcat(&strInfo, " (EAT_ICON", 0);
                                    break;

                                    case EAT_EA:
                                        xstrcat(&strInfo, " (EAT_EA", 0);
                                    break;

                                    case EAT_MVMT:
                                        xstrcat(&strInfo, " (EAT_MVMT", 0);
                                    break;

                                    case EAT_MVST:
                                        xstrcat(&strInfo, " (EAT_MVST", 0);
                                    break;

                                    case EAT_ASN1:
                                        xstrcat(&strInfo, " (EAT_ASN1", 0);
                                    break;

                                    default:
                                    {
                                        sprintf(szTemp, " (type 0x%lX", usEAType);
                                        xstrcat(&strInfo, szTemp, 0);
                                    }
                                }

                                sprintf(szTemp, ", %d bytes)", peab->usValueLength);
                                xstrcat(&strInfo, szTemp, 0);

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

#endif

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
    PCREATENOTEBOOKPAGE pcnbp;

#ifndef __NOFILEPAGE2__
    // page 2
    pcnbp = malloc(sizeof(CREATENOTEBOOKPAGE));
    memset(pcnbp, 0, sizeof(CREATENOTEBOOKPAGE));
    pcnbp->somSelf = somSelf;
    pcnbp->hwndNotebook = hwndNotebook;
    pcnbp->hmod = cmnQueryNLSModuleHandle(FALSE);
    pcnbp->ulDlgID = ID_XSD_FILESPAGE2;
    pcnbp->ulPageID = SP_FILE2;
    pcnbp->pszName = cmnGetString(ID_XSSI_FILEPAGE);  // pszFilePage
    pcnbp->fEnumerate = TRUE;
    pcnbp->ulDefaultHelpPanel  = ID_XSH_SETTINGS_FILEPAGE2;

    pcnbp->pfncbInitPage    = (PFNCBACTION)fsysFile2InitPage;
    pcnbp->pfncbItemChanged = (PFNCBITEMCHANGED)fsysFile2ItemChanged;

    ntbInsertPage(pcnbp);
#endif

    // page 1
    pcnbp = malloc(sizeof(CREATENOTEBOOKPAGE));
    memset(pcnbp, 0, sizeof(CREATENOTEBOOKPAGE));
    pcnbp->somSelf = somSelf;
    pcnbp->hwndNotebook = hwndNotebook;
    pcnbp->hmod = cmnQueryNLSModuleHandle(FALSE);
    pcnbp->ulDlgID = ID_XSD_FILESPAGE1;
    pcnbp->ulPageID = SP_FILE1;
    pcnbp->usPageStyleFlags = BKA_MAJOR;
    pcnbp->pszName = cmnGetString(ID_XSSI_FILEPAGE);  // pszFilePage
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

#ifndef __NOMODULEPAGES__

/*
 *@@ fsysProgramInitPage:
 *      "Program" page notebook callback function (notebook.c).
 *      Sets the controls on the page according to the disk's
 *      characteristics.
 *
 *@@added V0.9.0 [umoeller]
 *@@changed V0.9.12 (2001-05-19) [umoeller]: now using textview control for description, added extended info
 */

VOID fsysProgramInitPage(PCREATENOTEBOOKPAGE pcnbp,    // notebook info struct
                         ULONG flFlags)                // CBI_* flags (notebook.h)
{
    // PGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    if (flFlags & CBI_INIT)
    {
        // replace the static "description" control
        // with a text view control
        HWND hwndNew;
        txvRegisterTextView(WinQueryAnchorBlock(pcnbp->hwndDlgPage));
        hwndNew = txvReplaceWithTextView(pcnbp->hwndDlgPage,
                                         ID_XSDI_PROG_DESCRIPTION,
                                         WS_VISIBLE | WS_TABSTOP,
                                         XTXF_VSCROLL | XTXF_AUTOVHIDE
                                            | XTXF_HSCROLL | XTXF_AUTOHHIDE,
                                         2);
        winhSetWindowFont(hwndNew, cmnQueryDefaultFont());
    }

    if (flFlags & CBI_SET)
    {
        CHAR            szFilename[CCHMAXPATH] = "";

        // ULONG           cbProgDetails = 0;
        // PPROGDETAILS    pProgDetails;

        if (_wpQueryFilename(pcnbp->somSelf, szFilename, TRUE))
        {
            PEXECUTABLE     pExec = NULL;
            HWND            hwndTextView = WinWindowFromID(pcnbp->hwndDlgPage,
                                                           ID_XSDI_PROG_DESCRIPTION);

            WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XSDI_PROG_FILENAME,
                              szFilename);
            WinSetWindowText(hwndTextView, "\n");

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
                    XSTRING str;
                    xstrInit(&str, 100);

                    if (pExec->pszVendor)
                    {
                        // has BLDLEVEL info:
                        xstrcpy(&str, "Vendor: ", 0);
                        xstrcat(&str, pExec->pszVendor, 0);
                        xstrcat(&str, "\nVersion: ", 0);
                        xstrcat(&str, pExec->pszVersion, 0);
                        if (pExec->pszRevision)
                        {
                            xstrcat(&str, "\nRevision: ", 0);
                            xstrcat(&str, pExec->pszRevision, 0);
                        }
                        xstrcat(&str, "\nDescription: ", 0);
                        xstrcat(&str, pExec->pszInfo, 0);
                        if (pExec->pszBuildDateTime)
                        {
                            xstrcat(&str, "\nBuild date/time: ", 0);
                            xstrcat(&str, pExec->pszBuildDateTime, 0);
                        }
                        if (pExec->pszBuildMachine)
                        {
                            xstrcat(&str, "\nBuild machine: ", 0);
                            xstrcat(&str, pExec->pszBuildMachine, 0);
                        }
                        if (pExec->pszASD)
                        {
                            xstrcat(&str, "\nASD Feature ID: ", 0);
                            xstrcat(&str, pExec->pszASD, 0);
                        }
                        if (pExec->pszLanguage)
                        {
                            xstrcat(&str, "\nLanguage: ", 0);
                            xstrcat(&str, pExec->pszLanguage, 0);
                        }
                        if (pExec->pszCountry)
                        {
                            xstrcat(&str, "\nCountry: ", 0);
                            xstrcat(&str, pExec->pszCountry, 0);
                        }
                        if (pExec->pszFixpak)
                        {
                            xstrcat(&str, "\nFixpak: ", 0);
                            xstrcat(&str, pExec->pszFixpak, 0);
                        }
                    }
                    else
                    {
                        // no BLDLEVEL info:
                        xstrcpy(&str, pExec->pszDescription, 0);
                    }

                    xstrcatc(&str, '\n');
                    WinSetWindowText(hwndTextView, str.psz);
                    xstrClear(&str);
                }

                doshExecClose(pExec);
            }
        } // end if (_wpQueryFilename...

        // V0.9.12 (2001-05-19) [umoeller]
        // gee, what was this code doing in here?!?
        /* if ((_wpQueryProgDetails(pcnbp->somSelf, (PPROGDETAILS)NULL, &cbProgDetails)))
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
        */
    }
}

/*
 *@@ IMPORTEDMODULERECORD:
 *
 *@@added V0.9.9 (2001-03-11) [lafaix]
 *@@changed V0.9.9 (2001-04-02) [umoeller]: made PSZ's const
 */

typedef struct _IMPORTEDMODULERECORD
{
    RECORDCORE  recc;

    const char  *pcszModuleName;
} IMPORTEDMODULERECORD, *PIMPORTEDMODULERECORD;

/*
 *@@ fntInsertModules:
 *      transient thread started by fsysProgram1InitPage
 *      to insert modules into the "Imported modules" container.
 *
 *      This thread is created with a msg queue.
 *
 *@@added V0.9.9 (2001-03-26) [lafaix]
 *@@changed V0.9.9 (2001-03-30) [umoeller]: replaced dialog resource with generic cnr page
 *@@changed V0.9.9 (2001-03-30) [umoeller]: sped up display
 */

void _Optlink fntInsertModules(PTHREADINFO pti)
{
    PCREATENOTEBOOKPAGE pcnbp = (PCREATENOTEBOOKPAGE)(pti->ulData);

    TRY_LOUD(excpt1)
    {
        ULONG         cModules = 0,
                      ul;
        PFSYSMODULE   paModules = NULL;
        CHAR          szFilename[CCHMAXPATH] = "";

        pcnbp->fShowWaitPointer = TRUE;

        if (_wpQueryFilename(pcnbp->somSelf, szFilename, TRUE))
        {
            PEXECUTABLE     pExec = NULL;

            if (doshExecOpen(szFilename, &pExec) == NO_ERROR)
            {
                if (    (!doshExecQueryImportedModules(pExec,
                                                       &paModules,
                                                       &cModules))
                     && (paModules)
                   )
                {
                    HWND hwndCnr = WinWindowFromID(pcnbp->hwndDlgPage, ID_XFDI_CNR_CNR);

                    // V0.9.9 (2001-03-30) [umoeller]
                    // changed all this to allocate all records at once...
                    // this is way faster, because
                    // 1) inserting records into a details view always causes
                    //    a full container repaint (dumb cnr control)
                    // 2) each insert record causes a cross-thread WinSendMsg,
                    //    which is pretty slow

                    PIMPORTEDMODULERECORD preccFirst
                        = (PIMPORTEDMODULERECORD)cnrhAllocRecords(hwndCnr,
                                                                  sizeof(IMPORTEDMODULERECORD),
                                                                  cModules);
                                // the container gives us a linked list of
                                // records here, whose head we store in preccFirst

                    if (preccFirst)
                    {
                        // start with first record and follow the linked list
                        PIMPORTEDMODULERECORD preccThis = preccFirst;
                        ULONG cRecords = 0;

                        for (ul = 0;
                             ul < cModules;
                             ul++)
                        {
                            if (preccThis)
                            {
                                preccThis->pcszModuleName = paModules[ul].achModuleName;
                                preccThis = (PIMPORTEDMODULERECORD)preccThis->recc.preccNextRecord;
                                cRecords++;
                            }
                            else
                                break;
                        }

                        cnrhInsertRecords(hwndCnr,
                                          NULL,
                                          (PRECORDCORE)preccFirst,
                                          TRUE, // invalidate
                                          NULL,
                                          CRA_RECORDREADONLY,
                                          cRecords);
                    }
                }

                // store resources
                if (pcnbp->pUser)
                    doshExecFreeImportedModules(pcnbp->pUser);
                pcnbp->pUser = paModules;

                doshExecClose(pExec);
            }
        }
    }
    CATCH(excpt1) {}  END_CATCH();

    pcnbp->fShowWaitPointer = FALSE;
}

/*
 *@@ fsysProgram1InitPage:
 *      "Imported modules" page notebook callback function (notebook.c).
 *
 *@@added V0.9.9 (2001-03-11) [lafaix]
 *@@changed V0.9.9 (2001-03-30) [umoeller]: replaced dialog resource with generic cnr page
 *@@todo: corresponding ItemChanged page
 */

VOID fsysProgram1InitPage(PCREATENOTEBOOKPAGE pcnbp,    // notebook info struct
                          ULONG flFlags)                // CBI_* flags (notebook.h)
{
    // PGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    HWND hwndCnr = WinWindowFromID(pcnbp->hwndDlgPage, ID_XFDI_CNR_CNR);

    /*
     * CBI_INIT:
     *      initialize page (called only once)
     */

    if (flFlags & CBI_INIT)
    {
        XFIELDINFO xfi[2];
        // PFIELDINFO pfi = NULL;
        int        i = 0;
        // PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();

        WinSetDlgItemText(pcnbp->hwndDlgPage,
                          ID_XFDI_CNR_GROUPTITLE,
                          cmnGetString(ID_XSSI_PGMFILE_MODULE1)) ; // pszModule1Page

        // set up cnr details view
        xfi[i].ulFieldOffset = FIELDOFFSET(IMPORTEDMODULERECORD, pcszModuleName);
        xfi[i].pszColumnTitle = cmnGetString(ID_XSSI_COLMN_MODULENAME);  // pszColmnModuleName
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        /* pfi = */ cnrhSetFieldInfos(hwndCnr,
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
        // fill container with imported modules
        thrCreate(NULL,
                  fntInsertModules,
                  NULL, // running flag
                  "InsertModules",
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
            doshExecFreeImportedModules(pcnbp->pUser);
         pcnbp->pUser = NULL;
    }
}

/*
 *@@ EXPORTEDFUNCTIONRECORD:
 *
 *@@added V0.9.9 (2001-03-11) [lafaix]
 *@@changed V0.9.9 (2001-04-02) [umoeller]: made PSZ's const
 */

typedef struct _EXPORTEDFUNCTIONRECORD
{
    RECORDCORE  recc;

    ULONG       ulFunctionOrdinal;
    const char  *pcszFunctionType;
    const char  *pcszFunctionName;
} EXPORTEDFUNCTIONRECORD, *PEXPORTEDFUNCTIONRECORD;

/*
 *@@fsysGetExportedFunctionTypeName:
 *      returns a human-readable name from an exported function type.
 *
 *@@added V0.9.9 (2001-03-28) [lafaix]
 *@@changed V0.9.9 (2001-04-02) [umoeller]: return type is const char* now
 */

const char* fsysGetExportedFunctionTypeName(ULONG ulType)
{
    switch (ulType)
    {
        case 1:
            return "ENTRY16";
        case 2:
            return "CALLBACK";
        case 3:
            return "ENTRY32";
        case 4:
            return "FORWARDER";
    }

    return "Unknown export type"; // !!! Should return value too
}

/*
 *@@ fntInsertFunctions:
 *      transient thread started by fsysProgram2InitPage
 *      to insert functions into the "Exported functions" container.
 *
 *      This thread is created with a msg queue.
 *
 *@@added V0.9.9 (2001-03-28) [lafaix]
 *@@changed V0.9.9 (2001-03-30) [umoeller]: replaced dialog resource with generic cnr page
 *@@changed V0.9.9 (2001-03-30) [umoeller]: sped up display
 */

void _Optlink fntInsertFunctions(PTHREADINFO pti)
{
    PCREATENOTEBOOKPAGE pcnbp = (PCREATENOTEBOOKPAGE)(pti->ulData);

    TRY_LOUD(excpt1)
    {
        ULONG         cFunctions = 0,
                      ul;
        PFSYSFUNCTION paFunctions = NULL;
        CHAR          szFilename[CCHMAXPATH] = "";

        pcnbp->fShowWaitPointer = TRUE;

        if (_wpQueryFilename(pcnbp->somSelf, szFilename, TRUE))
        {
            PEXECUTABLE     pExec = NULL;

            if (doshExecOpen(szFilename, &pExec) == NO_ERROR)
            {
                if (    (!doshExecQueryExportedFunctions(pExec, &paFunctions, &cFunctions))
                     && (paFunctions)
                   )
                {
                    HWND hwndCnr = WinWindowFromID(pcnbp->hwndDlgPage, ID_XFDI_CNR_CNR);

                    // V0.9.9 (2001-03-30) [umoeller]
                    // changed all this to allocate all records at once...
                    // this is way faster, because
                    // 1) inserting records into a details view always causes
                    //    a full container repaint (dumb cnr control)
                    // 2) each insert record causes a cross-thread WinSendMsg,
                    //    which is pretty slow

                    PEXPORTEDFUNCTIONRECORD preccFirst
                        = (PEXPORTEDFUNCTIONRECORD)cnrhAllocRecords(hwndCnr,
                                                                    sizeof(EXPORTEDFUNCTIONRECORD),
                                                                    cFunctions);
                                // the container gives us a linked list of
                                // records here, whose head we store in preccFirst

                    if (preccFirst)
                    {
                        // start with first record and follow the linked list
                        PEXPORTEDFUNCTIONRECORD preccThis = preccFirst;
                        ULONG cRecords = 0;

                        for (ul = 0;
                             ul < cFunctions;
                             ul++)
                        {
                            if (preccThis)
                            {
                                preccThis->ulFunctionOrdinal = paFunctions[ul].ulOrdinal;
                                preccThis->pcszFunctionType
                                    = fsysGetExportedFunctionTypeName(paFunctions[ul].ulType);
                                preccThis->pcszFunctionName = paFunctions[ul].achFunctionName;

                                preccThis = (PEXPORTEDFUNCTIONRECORD)preccThis->recc.preccNextRecord;
                                cRecords++;
                            }
                            else
                                break;
                        }

                        cnrhInsertRecords(hwndCnr,
                                          NULL,
                                          (PRECORDCORE)preccFirst,
                                          TRUE, // invalidate
                                          NULL,
                                          CRA_RECORDREADONLY,
                                          cRecords);
                    }
                }

                // store functions
                if (pcnbp->pUser)
                    doshExecFreeExportedFunctions(pcnbp->pUser);
                pcnbp->pUser = paFunctions;

                doshExecClose(pExec);
            }
        }
    }
    CATCH(excpt1) {}  END_CATCH();

    pcnbp->fShowWaitPointer = FALSE;
}

/*
 *@@ fsysProgram2InitPage:
 *      "Exported functions" page notebook callback function (notebook.c).
 *
 *@@added V0.9.9 (2001-03-11) [lafaix]
 *@@changed V0.9.9 (2001-03-30) [umoeller]: replaced dialog resource with generic cnr page
 *@@todo: corresponding ItemChanged page
 */

VOID fsysProgram2InitPage(PCREATENOTEBOOKPAGE pcnbp,    // notebook info struct
                          ULONG flFlags)                // CBI_* flags (notebook.h)
{
    // PGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    HWND hwndCnr = WinWindowFromID(pcnbp->hwndDlgPage, ID_XFDI_CNR_CNR);

    /*
     * CBI_INIT:
     *      initialize page (called only once)
     */

    if (flFlags & CBI_INIT)
    {
        XFIELDINFO xfi[4];
        // PFIELDINFO pfi = NULL;
        int        i = 0;
        // PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();

        WinSetDlgItemText(pcnbp->hwndDlgPage,
                          ID_XFDI_CNR_GROUPTITLE,
                          cmnGetString(ID_XSSI_PGMFILE_MODULE2)) ; // pszModule2Page

        // set up cnr details view
        xfi[i].ulFieldOffset = FIELDOFFSET(EXPORTEDFUNCTIONRECORD, ulFunctionOrdinal);
        xfi[i].pszColumnTitle = cmnGetString(ID_XSSI_COLMN_EXPORTORDINAL);  // pszColmnExportOrdinal
        xfi[i].ulDataType = CFA_ULONG;
        xfi[i++].ulOrientation = CFA_RIGHT;

        xfi[i].ulFieldOffset = FIELDOFFSET(EXPORTEDFUNCTIONRECORD, pcszFunctionType);
        xfi[i].pszColumnTitle = cmnGetString(ID_XSSI_COLMN_EXPORTTYPE);  // pszColmnExportType
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        xfi[i].ulFieldOffset = FIELDOFFSET(EXPORTEDFUNCTIONRECORD, pcszFunctionName);
        xfi[i].pszColumnTitle = cmnGetString(ID_XSSI_COLMN_EXPORTNAME);  // pszColmnExportName
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        /* pfi = */ cnrhSetFieldInfos(hwndCnr,
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
        // fill container with functions
        thrCreate(NULL,
                  fntInsertFunctions,
                  NULL, // running flag
                  "InsertFunctions",
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
            doshExecFreeExportedFunctions(pcnbp->pUser);
        pcnbp->pUser = NULL;
    }
}

/*
 *@@ RESOURCERECORD:
 *
 *@@added V0.9.7 (2000-12-17) [lafaix]
 *@@changed V0.9.9 (2001-04-02) [umoeller]: made PSZ's const
 */

typedef struct _RESOURCERECORD
{
    RECORDCORE  recc;

    ULONG       ulResourceID; // !!! Could be a string with Windows or Open32 execs
    const char  *pcszResourceType;
    ULONG       ulResourceSize;
    const char  *pcszResourceFlag;

} RESOURCERECORD, *PRESOURCERECORD;

/*
 *@@fsysGetResourceFlagName:
 *      returns a human-readable name from a resource flag.
 *
 *@@added V0.9.7 (2001-01-10) [lafaix]
 *@@changed V0.9.9 (2001-04-02) [umoeller]: now returning const char*
 */

const char* fsysGetResourceFlagName(ULONG ulResourceFlag)
{
    #define FLAG_MASK 0x1050

    if ((ulResourceFlag & FLAG_MASK) == 0x1050)
        return "Preload";
    if ((ulResourceFlag & FLAG_MASK) == 0x1040)
        return "Preload, fixed";
    if ((ulResourceFlag & FLAG_MASK) == 0x1010)
        return "Default"; // default flag
    if ((ulResourceFlag & FLAG_MASK) == 0x1000)
        return "Fixed";
    if ((ulResourceFlag & FLAG_MASK) == 0x0050)
        return "Preload, not discardable";
    if ((ulResourceFlag & FLAG_MASK) == 0x0040)
        return "Preload, fixed, not discardable";
    if ((ulResourceFlag & FLAG_MASK) == 0x0010)
        return "Not discardable";
    if ((ulResourceFlag & FLAG_MASK) == 0)
        return "Fixed, not discardable";

    return ("Unknown");
}

/*
 *@@fsysGetResourceTypeName:
 *      returns a human-readable name from a resource type.
 *
 *@@added V0.9.7 (2000-12-20) [lafaix]
 *@@changed V0.9.9 (2001-04-02) [umoeller]: now returning const char*
 */

const char* fsysGetResourceTypeName(ULONG ulResourceType)
{
    switch (ulResourceType)
    {
        case RT_POINTER:
            return "Mouse pointer shape (RT_POINTER)";
        case RT_BITMAP:
            return "Bitmap (RT_BITMAP)";
        case RT_MENU:
            return "Menu template (RT_MENU)";
        case RT_DIALOG:
            return "Dialog template (RT_DIALOG)";
        case RT_STRING:
            return "String table (RT_STRING)";
        case RT_FONTDIR:
            return "Font directory (RT_FONTDIR)";
        case RT_FONT:
            return "Font (RT_FONT)";
        case RT_ACCELTABLE:
            return "Accelerator table (RT_ACCELTABLE)";
        case RT_RCDATA:
            return "Binary data (RT_RCDATA)";
        case RT_MESSAGE:
            return "Error message table (RT_MESSAGE)";
        case RT_DLGINCLUDE:
            return "Dialog include file name (RT_DLGINCLUDE)";
        case RT_VKEYTBL:
            return "Virtual key table (RT_VKEYTBL)";
        case RT_KEYTBL:
            return "Key table (RT_KEYTBL)";
        case RT_CHARTBL:
            return "Character table (RT_CHARTBL)";
        case RT_DISPLAYINFO:
            return "Display information (RT_DISPLAYINFO)";

        case RT_FKASHORT:
            return "Short-form function key area (RT_FKASHORT)";
        case RT_FKALONG:
            return "Long-form function key area (RT_FKALONG)";

        case RT_HELPTABLE:
            return "Help table (RT_HELPTABLE)";
        case RT_HELPSUBTABLE:
            return "Help subtable (RT_HELPSUBTABLE)";

        case RT_FDDIR:
            return "DBCS uniq/font driver directory (RT_FDDIR)";
        case RT_FD:
            return "DBCS uniq/font driver (RT_FD)";

        #ifndef RT_RESNAMES
            #define RT_RESNAMES         255
        #endif

        case RT_RESNAMES:
            return "String ID table (RT_RESNAMES)";
    }

    return "Application specific"; // !!! Should return value too
}

/*
 *@@ fntInsertResources:
 *      transient thread started by fsysResourcesInitPage
 *      to insert resources into the "Resources" container.
 *
 *      This thread is created with a msg queue.
 *
 *@@added V0.9.7 (2000-12-17) [lafaix]
 *@@changed V0.9.9 (2001-03-30) [umoeller]: replaced dialog resource with generic cnr page
 *@@changed V0.9.9 (2001-03-30) [umoeller]: sped up display
 *@@changed V0.9.9 (2001-04-03) [umoeller]: fixed cnr crash introduced by 03-30 change
 */

void _Optlink fntInsertResources(PTHREADINFO pti)
{
    PCREATENOTEBOOKPAGE pcnbp = (PCREATENOTEBOOKPAGE)(pti->ulData);

    TRY_LOUD(excpt1)
    {
        CHAR          szFilename[CCHMAXPATH] = "";

        pcnbp->fShowWaitPointer = TRUE;

        if (_wpQueryFilename(pcnbp->somSelf, szFilename, TRUE))
        {
            PEXECUTABLE     pExec = NULL;

            if (doshExecOpen(szFilename, &pExec) == NO_ERROR)
            {
                ULONG         cResources = 0;
                PFSYSRESOURCE paResources = NULL;

                if (    (!doshExecQueryResources(pExec,
                                                 &paResources,
                                                 &cResources))
                     && (cResources)
                     && (paResources)
                   )
                {
                    HWND hwndCnr = WinWindowFromID(pcnbp->hwndDlgPage, ID_XFDI_CNR_CNR);

                    // V0.9.9 (2001-03-30) [umoeller]
                    // changed all this to allocate all records at once...
                    // this is way faster, because
                    // 1) inserting records into a details view always causes
                    //    a full container repaint (dumb cnr control)
                    // 2) each insert record causes a cross-thread WinSendMsg,
                    //    which is pretty slow

                    PRESOURCERECORD preccFirst
                        = (PRESOURCERECORD)cnrhAllocRecords(hwndCnr,
                                                            sizeof(RESOURCERECORD),
                                            // duh, wrong size here V0.9.9 (2001-04-03) [umoeller];
                                            // this was sizeof(IMPORTEDMODULERECORD)
                                                            cResources);
                                // the container gives us a linked list of
                                // records here, whose head we store in preccFirst

                    if (preccFirst)
                    {
                        // start with first record and follow the linked list
                        PRESOURCERECORD preccThis = preccFirst;
                        ULONG   cRecords = 0,
                                ul;

                        for (ul = 0;
                             ul < cResources;
                             ul++)
                        {
                            if (preccThis)
                            {
                                preccThis->ulResourceID = paResources[ul].ulID;
                                preccThis->pcszResourceType
                                       = fsysGetResourceTypeName(paResources[ul].ulType);
                                preccThis->ulResourceSize = paResources[ul].ulSize;
                                preccThis->pcszResourceFlag
                                       = fsysGetResourceFlagName(paResources[ul].ulFlag);

                                preccThis = (PRESOURCERECORD)preccThis->recc.preccNextRecord;
                                cRecords++;
                            }
                            else
                                break;
                        }

                        if (cRecords == cResources)
                            cnrhInsertRecords(hwndCnr,
                                              NULL,
                                              (PRECORDCORE)preccFirst,
                                              TRUE, // invalidate
                                              NULL,
                                              CRA_RECORDREADONLY,
                                              cRecords);
                    } // if (preccFirst)

                } // if (paResources)

                // clean up existing resources, if any
                if (pcnbp->pUser)
                    doshExecFreeResources(pcnbp->pUser);
                // store resources
                pcnbp->pUser = paResources; // can be NULL

                doshExecClose(pExec);
            } // end if (doshExecOpen(szFilename, &pExec) == NO_ERROR)
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
 *@@changed V0.9.9 (2001-03-30) [umoeller]: replaced dialog resource with generic cnr page
 *@@todo: corresponding ItemChanged page
 */

VOID fsysResourcesInitPage(PCREATENOTEBOOKPAGE pcnbp,    // notebook info struct
                           ULONG flFlags)                // CBI_* flags (notebook.h)
{
    // PGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    HWND hwndCnr = WinWindowFromID(pcnbp->hwndDlgPage, ID_XFDI_CNR_CNR);

    /*
     * CBI_INIT:
     *      initialize page (called only once)
     */

    if (flFlags & CBI_INIT)
    {
        XFIELDINFO xfi[6];
        PFIELDINFO pfi = NULL;
        int        i = 0;
        // PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();

        WinSetDlgItemText(pcnbp->hwndDlgPage,
                          ID_XFDI_CNR_GROUPTITLE,
                          cmnGetString(ID_XSSI_PGMFILE_RESOURCES)) ; // pszResourcesPage

        // set up cnr details view
/* !!! not yet implemented [lafaix]
        xfi[i].ulFieldOffset = FIELDOFFSET(RESOURCERECORD, pszDeviceType);
        xfi[i].pszColumnTitle = cmnGetString(ID_XSSI_COLMN_RESOURCEICON); // pszColmnResourceIcon
        xfi[i].ulDataType = CFA_BITMAPORICON;
        xfi[i++].ulOrientation = CFA_LEFT;
*/

        xfi[i].ulFieldOffset = FIELDOFFSET(RESOURCERECORD, ulResourceID);
        xfi[i].pszColumnTitle = cmnGetString(ID_XSSI_COLMN_RESOURCEID);  // pszColmnResourceID
        xfi[i].ulDataType = CFA_ULONG;
        xfi[i++].ulOrientation = CFA_RIGHT;

        xfi[i].ulFieldOffset = FIELDOFFSET(RESOURCERECORD, pcszResourceType);
        xfi[i].pszColumnTitle = cmnGetString(ID_XSSI_COLMN_RESOURCETYPE);  // pszColmnResourceType
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        xfi[i].ulFieldOffset = FIELDOFFSET(RESOURCERECORD, ulResourceSize);
        xfi[i].pszColumnTitle = cmnGetString(ID_XSSI_COLMN_RESOURCESIZE);  // pszColmnResourceSize
        xfi[i].ulDataType = CFA_ULONG;
        xfi[i++].ulOrientation = CFA_RIGHT;

        xfi[i].ulFieldOffset = FIELDOFFSET(RESOURCERECORD, pcszResourceFlag);
        xfi[i].pszColumnTitle = cmnGetString(ID_XSSI_COLMN_RESOURCEFLAGS);  // pszColmnResourceFlags
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        pfi = cnrhSetFieldInfos(hwndCnr,
                                xfi,
                                i,             // array item count
                                TRUE,          // draw lines
                                1);            // return second column

        BEGIN_CNRINFO()
        {
            cnrhSetView(CV_DETAIL | CA_DETAILSVIEWTITLES);
            cnrhSetSplitBarAfter(pfi);  // V0.9.7 (2001-01-18) [umoeller]
            cnrhSetSplitBarPos(250);
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
                  "InsertResources",
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
            doshExecFreeResources(pcnbp->pUser);
        pcnbp->pUser = NULL;
    }
}

#endif // __NOMODULEPAGES__



