
/*
 *@@sourcefile statbars.c:
 *      this file contains status bar code:
 *
 *      -- status bar info translation logic (see stbComposeText)
 *
 *      -- status bar notebook callbacks used from XFldWPS (xfwps.c);
 *         these have been moved here with V0.9.0.
 *
 *      More status bar code can be found with the following:
 *
 *      --  The status bar is created in fdrCreateStatusBar, which
 *          gets called from XFolder::wpOpen.
 *
 *      --  The status bar's window proc (fdr_fnwpStatusBar) is in folder.c.
 *
 *      --  When selections change in a folder view (i.e.
 *          fdr_fnwpSubclassedFolderFrame receives CN_EMPHASIS
 *          notification), fdr_fnwpStatusBar is posted an STBM_UPDATESTATUSBAR
 *          message, which in turn calls XFolder::xwpUpdateStatusBar,
 *          which normally calls stbComposeText in turn (the main entry
 *          point to the mess in this file).
 *
 *      Function prefix for this file:
 *      --  stb*
 *
 *      This file is new with XFolder 0.81. V0.80 used
 *      SOM multiple inheritance to introduce new methods
 *      with XFldObject, which proved to cause too many problems.
 *      This is now all done in this file using SOM kernel functions
 *      to determine the class of a selected object.
 *
 *      It is thus now much easier to add support for new classes
 *      also, because no new class replacements have to be introduced.
 *      In order to do so, go through all the funcs below and add new
 *      "if" statements. No changes in other files should be necessary.
 *      You will have to add a #include below for that class though
 *      to be able to access the SOM class object for that new class.
 *
 *@@header "filesys\statbars.h"
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
 *@@todo:
 *
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

#define INCL_DOSERRORS
#define INCL_WINSHELLDATA       // Prf* functions
#define INCL_WINPROGRAMLIST     // needed for PROGDETAILS, wppgm.h
#define INCL_WINDIALOGS
#define INCL_WINBUTTONS
#define INCL_WINENTRYFIELDS
#include <os2.h>

// C library headers
#include <stdio.h>
#include <string.h>

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\stringh.h"            // string helper routines
#include "helpers\winh.h"               // PM helper routines
#include "helpers\xstring.h"            // extended string helpers

// SOM headers which don't crash with prec. header files
#include "xfobj.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\classes.h"             // WPS class list helper functions
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\notebook.h"            // generic XWorkplace notebook handling

#include "filesys\statbars.h"           // status bar translation logic
#include "filesys\xthreads.h"           // extra XWorkplace threads

// other SOM headers
#pragma hdrstop                         // VAC++ keeps crashing otherwise
#include <wpfolder.h>                   // WPFolder
#include <wpdisk.h>                     // WPDisk
#include <wppgm.h>                      // WPProgram
#include <wpshadow.h>                   // WPShadow
#include <wpshdir.h>                    // WPSharedDir // V0.9.5 (2000-09-20) [pr]

// finally, our own header file
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

/* ******************************************************************
 *                                                                  *
 *   Global variables                                               *
 *                                                                  *
 ********************************************************************/

CHAR    G_szXFldObjectStatusBarMnemonics[CCHMAXMNEMONICS] = "";
CHAR    G_szWPProgramStatusBarMnemonics[CCHMAXMNEMONICS] = "";
CHAR    G_szWPDiskStatusBarMnemonics[CCHMAXMNEMONICS] = "";
CHAR    G_szWPFileSystemStatusBarMnemonics[CCHMAXMNEMONICS] = "";
CHAR    G_szWPUrlStatusBarMnemonics[CCHMAXMNEMONICS] = "";

// WPUrl class object; to preserve compatibility with Warp 3,
// where this class does not exist, we call the SOM kernel
// explicitly to get the class object.
// The initial value of -1 means that we have not queried
// this class yet. After the first query, this either points
// to the class object or is NULL if the class does not exist.
SOMClass    *G_WPUrl = (SOMClass*)-1;

/* ******************************************************************
 *                                                                  *
 *   Status bar mnemonics                                           *
 *                                                                  *
 ********************************************************************/

/*
 *@@ stbVar1000Double:
 *      generates an automatically scaled disk space string.
 *      Uses 1000 as the divisor for kB and mB.
 *
 *@@added V0.9.6 (2000-11-12) [pr]
 */

PSZ stbVar1000Double(PSZ pszTarget,
                     double space,
                     PNLSSTRINGS pNLSStrings,
                     CHAR cThousands)
{
    if (space < 1000.0)
        sprintf(pszTarget, "%.0f%s", space,
                (ULONG)space == 1 ? pNLSStrings->pszByte : pNLSStrings->pszBytes);
    else
        if (space < 10000.0)
            strhVariableDouble(pszTarget, space, pNLSStrings->pszBytes, cThousands);
        else
        {
            space /= 1000;
            if (space < 10000.0)
                strhVariableDouble(pszTarget, space, " kB", cThousands);
            else
            {
                space /= 1000;
                strhVariableDouble(pszTarget, space,
                                   space < 10000.0 ? " mB" : " gB", cThousands);
            }
        }

    return(pszTarget);
}

/*
 *@@ stbVar1024Double:
 *      generates an automatically scaled disk space string.
 *      Uses 1024 as the divisor for KB and MB.
 *
 *@@added V0.9.6 (2000-11-12) [pr]
 */

PSZ stbVar1024Double(PSZ pszTarget,
                     double space,
                     PNLSSTRINGS pNLSStrings,
                     CHAR cThousands)
{
    if (space < 1000.0)
        sprintf(pszTarget, "%.0f%s", space,
                (ULONG) space == 1 ? pNLSStrings->pszByte : pNLSStrings->pszBytes);
    else
        if (space < 10240.0)
            strhVariableDouble(pszTarget, space, pNLSStrings->pszBytes, cThousands);
        else
        {
            space /= 1024;
            if (space < 10240.0)
                strhVariableDouble(pszTarget, space, " KB", cThousands);
            else
            {
                space /= 1024;
                strhVariableDouble(pszTarget, space,
                                   space < 10240.0 ? " MB" : " GB", cThousands);
            }
        }

    return(pszTarget);
}


/*
 *@@ stbClassAddsNewMnemonics:
 *      returns TRUE if a class introduces new status bar
 *      mnemonics. Used by the "Status bars" notebook page
 *      in the "Select class" dialog to enable/disable
 *      classes which may be selected to set new status
 *      bar single-object information.
 *
 *@@changed V0.9.0 [umoeller]: now using _WPDisk instead of _XFldDisk
 */

BOOL stbClassAddsNewMnemonics(SOMClass *pClassObject)
{
    return (    (pClassObject == _XFldObject)
             || (pClassObject == _WPProgram)
             || (pClassObject == _WPDisk)
             || (pClassObject == _WPFileSystem)
             || ( (G_WPUrl != NULL) && (pClassObject == G_WPUrl) )
           );
}

/*
 *@@ stbSetClassMnemonics:
 *      this changes the status bar mnemonics for "single object"
 *      info for objects of this class and subclasses to
 *      pszText. If *pszText points to null-length string, no
 *      status bar info will be displayed; if pszText is NULL,
 *      the menmonics will be reset to the default value.
 *      This is called by the "Status bars" notebook page
 *      only when the user changes status bar mnemonics.
 *
 *      pClassObject may be one of the following:
 *      --  _XFldObject
 *      --  _WPProgram
 *      --  _WPDisk
 *      --  _WPFileSystem
 *      --  _WPUrl
 *
 +      This returns FALSE upon errors.
 *
 *@@changed V0.9.0 [umoeller]: now using _WPDisk instead of _XFldDisk
 *@@changed V0.9.0 [umoeller]: now returning FALSE upon errors
 *@@changed V0.9.5 (2000-09-20) [pr]: WPSharedDir has WPDisk status bar
 */

BOOL stbSetClassMnemonics(SOMClass *pClassObject,
                          PSZ pszText)
{
    if (G_WPUrl == (SOMClass*)-1)
    {
        // WPUrl class object not queried yet: do it now
        somId    somidWPUrl = somIdFromString("WPUrl");
        G_WPUrl = _somFindClass(SOMClassMgrObject, somidWPUrl, 0, 0);
        // _WPUrl now either points to the WPUrl class object
        // or is NULL if the class is not installed (Warp 3!).
        // In this case, the object will be treated as a regular
        // file-system object.
    }

    if (G_WPUrl)
    {
        if (_somDescendedFrom(pClassObject, G_WPUrl))
        {
            // provoke a reload of the settings
            // in stbQueryClassMnemonics
            G_szWPUrlStatusBarMnemonics[0] = '\0';

            // set the class mnemonics in OS2.INI; if
            // pszText == NULL, the key will be deleted,
            // and stbQueryClassMnemonics will use
            // the default value
            return (PrfWriteProfileString(HINI_USERPROFILE,
                                          (PSZ)INIAPP_XWORKPLACE,
                                          (PSZ)INIKEY_SBTEXT_WPURL,
                                          pszText));
        }
    }

    // no WPUrl or WPUrl not installed: continue
    if (   (_somDescendedFrom(pClassObject, _WPDisk))
           // V0.9.5 (2000-09-20) [pr] WPSharedDir has disk status bar
        || (_somDescendedFrom(pClassObject, _WPSharedDir))
       )
    {
        // provoke a reload of the settings
        // in stbQueryClassMnemonics
        G_szWPDiskStatusBarMnemonics[0] = '\0';

        // set the class mnemonics in OS2.INI; if
        // pszText == NULL, the key will be deleted,
        // and stbQueryClassMnemonics will use
        // the default value
        return (PrfWriteProfileString(HINI_USERPROFILE,
                                      (PSZ)INIAPP_XWORKPLACE,
                                      (PSZ)INIKEY_SBTEXT_WPDISK,
                                      pszText));
    }
    else if (_somDescendedFrom(pClassObject, _WPFileSystem))
    {
        // provoke a reload of the settings
        // in stbQueryClassMnemonics
        G_szWPFileSystemStatusBarMnemonics[0] = '\0';

        // set the class mnemonics in OS2.INI; if
        // pszText == NULL, the key will be deleted,
        // and stbQueryClassMnemonics will use
        // the default value
        return (PrfWriteProfileString(HINI_USERPROFILE,
                                      (PSZ)INIAPP_XWORKPLACE,
                                      (PSZ)INIKEY_SBTEXT_WPFILESYSTEM,
                                      pszText));
    }
    else if (_somDescendedFrom(pClassObject, _WPProgram))
    {
        // provoke a reload of the settings
        // in stbQueryClassMnemonics
        G_szWPProgramStatusBarMnemonics[0] = '\0';

        // set the class mnemonics in OS2.INI; if
        // pszText == NULL, the key will be deleted,
        // and stbQueryClassMnemonics will use
        // the default value
        return (PrfWriteProfileString(HINI_USERPROFILE,
                                      (PSZ)INIAPP_XWORKPLACE,
                                      (PSZ)INIKEY_SBTEXT_WPPROGRAM,
                                      pszText));
    }
    else if (_somDescendedFrom(pClassObject, _XFldObject))
    {
        // provoke a reload of the settings
        // in stbQueryClassMnemonics
        G_szXFldObjectStatusBarMnemonics[0] = '\0';

        // set the class mnemonics in OS2.INI; if
        // pszText == NULL, the key will be deleted,
        // and stbQueryClassMnemonics will use
        // the default value
        return (PrfWriteProfileString(HINI_USERPROFILE,
                                      (PSZ)INIAPP_XWORKPLACE,
                                      (PSZ)INIKEY_SBTEXT_WPOBJECT,
                                      pszText));
    }

    return (FALSE);
}

/*
 *@@ stbQueryClassMnemonics:
 *      this returns the status bar mnemonics for "single object"
 *      info for objects of pClassObject and subclasses. This string
 *      is either what has been specified on the "Status bar"
 *      notebook page or a default string from the XFolder NLS DLL.
 *      This is called every time the status bar needs to be updated
 *      (stbComposeText) and by the "Status bars" notebook page.
 *
 *      pClassObject may be _any_ valid WPS class. This function
 *      will automatically determine the correct class mnemonics
 *      from it.
 *
 *      Note: The return value of this function points to a _static_
 *      buffer, which must not be free()'d.
 *
 *@@changed V0.8.5 [umoeller]: fixed problem when WPProgram was replaced
 *@@changed V0.9.0 [umoeller]: function prototype changed to return PSZ instead of ULONG
 *@@changed V0.9.0 [umoeller]: now using _WPDisk instead of _XFldDisk
 *@@changed V0.9.5 (2000-09-20) [pr]: WPSharedDir has WPDisk status bar
 */

PSZ stbQueryClassMnemonics(SOMClass *pClassObject)    // in: class object of selected object
{
    PSZ     pszReturn = NULL;

    if (G_WPUrl == (SOMClass*)-1)
    {
        // WPUrl class object not queried yet: do it now
        somId    somidWPUrl = somIdFromString("WPUrl");
        G_WPUrl = _somFindClass(SOMClassMgrObject, somidWPUrl, 0, 0);
        // _WPUrl now either points to the WPUrl class object
        // or is NULL if the class is not installed (Warp 3!).
    }

    if (G_WPUrl)
        if (_somDescendedFrom(pClassObject, G_WPUrl))
        {
            if (G_szWPUrlStatusBarMnemonics[0] == '\0')
                // load string if this is the first time
                if (PrfQueryProfileString(HINI_USERPROFILE,
                                          (PSZ)INIAPP_XWORKPLACE,
                                          (PSZ)INIKEY_SBTEXT_WPURL,
                                          NULL,
                                          &(G_szWPUrlStatusBarMnemonics),
                                          sizeof(G_szWPUrlStatusBarMnemonics))
                        == 0)
                    // string not found in profile: set default
                    strcpy(G_szWPUrlStatusBarMnemonics, "\"$U\"$x(70%)$D $T");

            pszReturn = G_szWPUrlStatusBarMnemonics;
            // get out of here
            return (pszReturn);
        }

    if (    (_somDescendedFrom(pClassObject, _WPDisk))
            // V0.9.5 (2000-09-20) [pr] WPSharedDir has disk status bar
         || (_somDescendedFrom(pClassObject, _WPSharedDir))
       )
    {
        if (G_szWPDiskStatusBarMnemonics[0] == '\0')
            // load string if this is the first time
            if (PrfQueryProfileString(HINI_USERPROFILE,
                                      (PSZ)INIAPP_XWORKPLACE,
                                      (PSZ)INIKEY_SBTEXT_WPDISK,
                                      NULL,
                                      &(G_szWPDiskStatusBarMnemonics),
                                      sizeof(G_szWPDiskStatusBarMnemonics))
                    == 0)
                // string not found in profile: load default from NLS resources
                WinLoadString(WinQueryAnchorBlock(HWND_DESKTOP),
                              cmnQueryNLSModuleHandle(FALSE),
                              ID_XSSI_SBTEXTWPDISK,
                              sizeof(G_szWPDiskStatusBarMnemonics),
                              G_szWPDiskStatusBarMnemonics);

        pszReturn = G_szWPDiskStatusBarMnemonics;
    }
    else if (_somDescendedFrom(pClassObject, _WPFileSystem))
    {
        if (G_szWPFileSystemStatusBarMnemonics[0] == '\0')
            // load string if this is the first time
            if (PrfQueryProfileString(HINI_USERPROFILE,
                        (PSZ)INIAPP_XWORKPLACE,
                        (PSZ)INIKEY_SBTEXT_WPFILESYSTEM,
                        NULL, &(G_szWPFileSystemStatusBarMnemonics),
                        sizeof(G_szWPFileSystemStatusBarMnemonics))
                    == 0)
                // string not found in profile: load default from NLS resources
                WinLoadString(WinQueryAnchorBlock(HWND_DESKTOP),
                              cmnQueryNLSModuleHandle(FALSE),
                              ID_XSSI_SBTEXTWPDATAFILE,
                              sizeof(G_szWPFileSystemStatusBarMnemonics),
                              G_szWPFileSystemStatusBarMnemonics);

        pszReturn = G_szWPFileSystemStatusBarMnemonics;
    }
    //
    else if (_somDescendedFrom(pClassObject, _WPProgram))  // fixed V0.85
    {
        if (G_szWPProgramStatusBarMnemonics[0] == '\0')
            // load string if this is the first time
            if (PrfQueryProfileString(HINI_USERPROFILE,
                                      (PSZ)INIAPP_XWORKPLACE,
                                      (PSZ)INIKEY_SBTEXT_WPPROGRAM,
                                      NULL,
                                      &(G_szWPProgramStatusBarMnemonics),
                                      sizeof(G_szWPProgramStatusBarMnemonics))
                    == 0)
                // string not found in profile: load default from NLS resources
                WinLoadString(WinQueryAnchorBlock(HWND_DESKTOP),
                              cmnQueryNLSModuleHandle(FALSE),
                              ID_XSSI_SBTEXTWPPROGRAM,
                              sizeof(G_szWPProgramStatusBarMnemonics),
                              G_szWPProgramStatusBarMnemonics);

        pszReturn = G_szWPProgramStatusBarMnemonics;
    }
    // subsidiarily: XFldObject
    else if (_somDescendedFrom(pClassObject, _XFldObject))
    {
        // should always be TRUE
        if (G_szXFldObjectStatusBarMnemonics[0] == '\0')
            // load string if this is the first time
            if (PrfQueryProfileString(HINI_USERPROFILE,
                                      (PSZ)INIAPP_XWORKPLACE,
                                      (PSZ)INIKEY_SBTEXT_WPOBJECT,
                                      NULL,
                                      &(G_szXFldObjectStatusBarMnemonics),
                                      sizeof(G_szXFldObjectStatusBarMnemonics))
                        == 0)
                // string not found in profile: load default from NLS resources
                WinLoadString(WinQueryAnchorBlock(HWND_DESKTOP),
                              cmnQueryNLSModuleHandle(FALSE),
                              ID_XSSI_SBTEXTWPOBJECT,
                              sizeof(G_szXFldObjectStatusBarMnemonics),
                              G_szXFldObjectStatusBarMnemonics);

        pszReturn = G_szXFldObjectStatusBarMnemonics;
    } else
        pszReturn = "???";     // should not occur

    return (pszReturn);
}

/*
 *@@ stbTranslateSingleMnemonics:
 *      this method is called on an object by stbComposeText
 *      after the status bar mnemonics have been queried
 *      for the object's class using stbQueryClassMnemonics,
 *      which is passed to this function in pszText.
 *
 *      Starting with V0.9.0, this function no longer gets
 *      shadows, but the dereferenced object in pObject,
 *      if "Dereference Shadows" is enabled in XFldWPS.
 *
 *      As opposed to the functions above, this does not
 *      take a class object, but an _instance_ as a parameter,
 *      because the info has to be displayed for each object
 *      differently.
 *
 *      Note that stbComposeText has changed all the '$' characters
 *      in pszText to the tabulator char ('\t') to avoid
 *      unwanted results if text inserted during the translations
 *      contains '$' chars. So we search for '\t' keys in this
 *      function.
 *
 *      This returns the number of keys that were translated.
 *
 *@@changed V0.9.0 [umoeller]: now using _WPDisk instead of _XFldDisk
 *@@changed V0.9.5 (2000-09-20) [pr]: WPSharedDir has WPDisk status bar
 *@@changed V0.9.6 (2000-11-01) [umoeller]: now using all new xstrFindReplace
 *@@changed V0.9.6 (2000-11-12) [umoeller]: removed cThousands param, now using cached country settings
 *@@changed V0.9.6 (2000-11-12) [pr]: new status bar mnemonics
 */

ULONG  stbTranslateSingleMnemonics(SOMClass *pObject,  // in: object
                                   PXSTRING pstrText)  // in/out: status bar text
{
    ULONG       ulrc = 0;
    CHAR        szTemp[300];        // must be at least CCHMAXPATH!
    PSZ         p;

    PCOUNTRYSETTINGS pcs = cmnQueryCountrySettings(FALSE);
    PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();

    /*
     * WPUrl:
     *
     */

    /* first check if the thing is a URL object;
       in addition to the normal WPFileSystem mnemonics,
       URL objects also support the $U mnemonic for
       displaying the URL */

    if (G_WPUrl == (SOMClass*)-1)
    {
        // WPUrl class object not queried yet: do it now
        somId    somidWPUrl = somIdFromString("WPUrl");
        G_WPUrl = _somFindClass(SOMClassMgrObject, somidWPUrl, 0, 0);
        // _WPUrl now either points to the WPUrl class object
        // or is NULL if the class is not installed (Warp 3!).
    }

    if (G_WPUrl)
        if (_somIsA(pObject, G_WPUrl))
        {
            // yes, we have a URL object:
            if (p = strstr(pstrText->psz, "\tU")) // URL mnemonic
            {
                CHAR szFilename[CCHMAXPATH];
                PSZ pszFilename = _wpQueryFilename(pObject, szFilename, TRUE);

                // read in the contents of the file, which
                // contain the URL
                PSZ pszURL = NULL;
                ULONG ulOfs = 0;
                if (pszFilename)
                    doshLoadTextFile(pszFilename, &pszURL);
                    if (pszURL)
                    {
                        if (strlen(pszURL) > 100)
                            strcpy(pszURL+97, "...");
                        xstrFindReplaceC(pstrText, &ulOfs, "\tU", pszURL);
                        free(pszURL);
                    }
                if (!pszURL)
                    xstrFindReplaceC(pstrText, &ulOfs, "\tU", "?");
                ulrc++;
            }
        }

    /*
     * WPDisk:
     *
     */

    if (    (_somIsA(pObject, _WPDisk))
            // V0.9.5 (2000-09-20) [pr] WPSharedDir has disk status bar
         || (_somIsA(pObject, _WPSharedDir))
       )
    {
        ULONG ulLogicalDrive = -1;

        /* single-object status bar text mnemonics understood by WPDisk:

             $F      file system type (HPFS, FAT, CDFS, ...)

             $fb     free space on drive in bytes
             $fk     free space on drive in kBytes
             $fK     free space on drive in KBytes
             $fm     free space on drive in mBytes
             $fM     free space on drive in MBytes

             $fa     free space on drive in bytes/kBytes/mBytes/gBytes V0.9.6
             $fA     free space on drive in bytes/KBytes/MBytes/GBytes V0.9.6

            NOTE: the $f keys are also handled by stbComposeText, but
                  those only work properly for file-system objects, so we need
                  to calculate these values for these (abstract) disk objects

         */

        // the following are for free space on drive
        if (p = strstr(pstrText->psz, "\tfb"))
        {
            double dbl;
            ULONG ulOfs = 0;

            if (ulLogicalDrive == -1)
            {
                ulLogicalDrive = _wpQueryLogicalDrive(pObject);
                if (doshAssertDrive(ulLogicalDrive) != NO_ERROR)
                    ulLogicalDrive = 0;
            }

            if (doshQueryDiskFree(ulLogicalDrive, &dbl))
                strcpy(szTemp, "?");
            else
                strhThousandsDouble(szTemp, dbl,
                                    pcs->cThousands);
            xstrFindReplaceC(pstrText, &ulOfs, "\tfb", szTemp);
            ulrc++;
        }

        if (p = strstr(pstrText->psz, "\tfk"))
        {
            double dbl;
            ULONG ulOfs = 0;

            if (ulLogicalDrive == -1)
            {
                ulLogicalDrive = _wpQueryLogicalDrive(pObject);
                if (doshAssertDrive(ulLogicalDrive) != NO_ERROR)
                    ulLogicalDrive = 0;
            }

            if (doshQueryDiskFree(ulLogicalDrive, &dbl))
                strcpy(szTemp, "?");
            else
                strhThousandsDouble(szTemp,
                                    ((dbl + 500) / 1000),
                                    pcs->cThousands);
            xstrFindReplaceC(pstrText, &ulOfs, "\tfk", szTemp);
            ulrc++;
        }

        if (p = strstr(pstrText->psz, "\tfK"))
        {
            double dbl;
            ULONG ulOfs = 0;

            if (ulLogicalDrive == -1)
            {
                ulLogicalDrive = _wpQueryLogicalDrive(pObject);
                if (doshAssertDrive(ulLogicalDrive) != NO_ERROR)
                    ulLogicalDrive = 0;
            }

            if (doshQueryDiskFree(ulLogicalDrive, &dbl))
            if (dbl == -1)
                strcpy(szTemp, "?");
            else
                strhThousandsDouble(szTemp,
                                    ((dbl + 512) / 1024),
                                    pcs->cThousands);
            xstrFindReplaceC(pstrText, &ulOfs, "\tfK", szTemp);
            ulrc++;
        }

        if (p = strstr(pstrText->psz, "\tfm"))
        {
            double dbl;
            ULONG ulOfs = 0;

            if (ulLogicalDrive == -1)
            {
                ulLogicalDrive = _wpQueryLogicalDrive(pObject);
                if (doshAssertDrive(ulLogicalDrive) != NO_ERROR)
                    ulLogicalDrive = 0;
            }

            if (doshQueryDiskFree(ulLogicalDrive, &dbl))
                strcpy(szTemp, "?");
            else
                strhThousandsDouble(szTemp,
                                    ((dbl +500000) / 1000000),
                                    pcs->cThousands);
            xstrFindReplaceC(pstrText, &ulOfs, "\tfm", szTemp);
            ulrc++;
        }

        if (p = strstr(pstrText->psz, "\tfM"))
        {
            double dbl;
            ULONG ulOfs = 0;

            if (ulLogicalDrive == -1)
            {
                ulLogicalDrive = _wpQueryLogicalDrive(pObject);
                if (doshAssertDrive(ulLogicalDrive) != NO_ERROR)
                    ulLogicalDrive = 0;
            }

            if (doshQueryDiskFree(ulLogicalDrive, &dbl))
                strcpy(szTemp, "?");
            else
               strhThousandsDouble(szTemp,
                                   ((dbl + (1024*1024/2)) / (1024*1024)),
                                   pcs->cThousands);
            xstrFindReplaceC(pstrText, &ulOfs, "\tfM", szTemp);
            ulrc++;
        }

        // V0.9.6

        if (p = strstr(pstrText->psz, "\tfa"))
        {
            double dbl;
            ULONG ulOfs = 0;

            if (ulLogicalDrive == -1)
            {
                ulLogicalDrive = _wpQueryLogicalDrive(pObject);
                if (doshAssertDrive(ulLogicalDrive) != NO_ERROR)
                    ulLogicalDrive = 0;
            }

            if (doshQueryDiskFree(ulLogicalDrive, &dbl))
                strcpy(szTemp, "?");
            else
                stbVar1000Double(szTemp,
                                 dbl,
                                 pNLSStrings,
                                 pcs->cThousands);
            xstrFindReplaceC(pstrText, &ulOfs, "\tfa", szTemp);
            ulrc++;
        }

        if (p = strstr(pstrText->psz, "\tfA"))
        {
            double dbl;
            ULONG ulOfs = 0;

            if (ulLogicalDrive == -1)
            {
                ulLogicalDrive = _wpQueryLogicalDrive(pObject);
                if (doshAssertDrive(ulLogicalDrive) != NO_ERROR)
                    ulLogicalDrive = 0;
            }

            if (doshQueryDiskFree(ulLogicalDrive, &dbl))
                strcpy(szTemp, "?");
            else
                stbVar1024Double(szTemp,
                                 dbl,
                                 pNLSStrings,
                                 pcs->cThousands);
            xstrFindReplaceC(pstrText, &ulOfs, "\tfA", szTemp);
            ulrc++;
        }
        // end V0.9.6

        if (p = strstr(pstrText->psz, "\tF"))  // file-system type (HPFS, ...)
        {
            CHAR szBuffer[200];
            ULONG ulOfs = 0;

            if (ulLogicalDrive == -1)
            {
                ulLogicalDrive = _wpQueryLogicalDrive(pObject);
                if (doshAssertDrive(ulLogicalDrive) != NO_ERROR)
                    ulLogicalDrive = 0;
            }

            if (doshQueryDiskFSType(ulLogicalDrive,
                                    szBuffer,
                                    sizeof(szBuffer))
                         == NO_ERROR)
                xstrFindReplaceC(pstrText, &ulOfs, "\tF", szBuffer);
            else
                xstrFindReplaceC(pstrText, &ulOfs, "\tF", "?");
            ulrc++;
        }
    }

    /*
     * WPFileSystem:
     *
     */

    else if (_somIsA(pObject, _WPFileSystem))
    {
        FILEFINDBUF4    ffbuf4;
        BOOL            fBufLoaded = FALSE;     // V0.9.6 (2000-11-12) [umoeller]

        /* single-object status bar text mnemonics understood by WPFileSystem
           (in addition to those introduced by XFldObject):

             $r      object's real name

             $y      object type (.TYPE EA)
             $D      object creation date
             $T      object creation time
             $a      object attributes

             $Eb     EA size in bytes
             $Ek     EA size in kBytes
             $EK     EA size in KBytes

             $Ea     EA size in bytes/kBytes (V0.9.6)
             $EA     EA size in bytes/KBytes (V0.9.6)
         */

        if (p = strstr(pstrText->psz, "\ty")) // attribs
        {
            PSZ p2 = NULL;
            ULONG ulOfs = 0;
            p2 = _wpQueryType(pObject);
            xstrFindReplaceC(pstrText, &ulOfs, "\ty", (p2) ? p2 : "?");
            ulrc++;
        }

        if (p = strstr(pstrText->psz, "\tD"))  // date
        {
            ULONG ulOfs = 0;
            strcpy(szTemp, "?");
            _wpQueryDateInfo(pObject, &ffbuf4);
            fBufLoaded = TRUE;
            strhFileDate(szTemp, &(ffbuf4.fdateLastWrite), pcs->ulDateFormat, pcs->cDateSep);
            xstrFindReplaceC(pstrText, &ulOfs, "\tD", szTemp);
            ulrc++;
        }

        if (p = strstr(pstrText->psz, "\tT"))  // time
        {
            ULONG ulOfs = 0;
            strcpy(szTemp, "?");
            if (!fBufLoaded)
                _wpQueryDateInfo(pObject, &ffbuf4);
            strhFileTime(szTemp, &(ffbuf4.ftimeLastWrite), pcs->ulTimeFormat, pcs->cTimeSep);
            xstrFindReplaceC(pstrText, &ulOfs, "\tT", szTemp);
            ulrc++;
        }

        if (p = strstr(pstrText->psz, "\ta")) // attribs
        {
            ULONG fAttr = _wpQueryAttr(pObject);
            ULONG ulOfs = 0;
            szTemp[0] = (fAttr & FILE_ARCHIVED) ? 'A' : 'a';
            szTemp[1] = (fAttr & FILE_HIDDEN  ) ? 'H' : 'h';
            szTemp[2] = (fAttr & FILE_READONLY) ? 'R' : 'r';
            szTemp[3] = (fAttr & FILE_SYSTEM  ) ? 'S' : 's';
            szTemp[4] = '\0';
            xstrFindReplaceC(pstrText, &ulOfs, "\ta", szTemp);
            ulrc++;
        }

        if (p = strstr(pstrText->psz, "\tEb")) // easize
        {
            ULONG ulEASize = _wpQueryEASize(pObject);
            ULONG ulOfs = 0;
            strhThousandsDouble(szTemp, ulEASize, pcs->cThousands);
            xstrFindReplaceC(pstrText, &ulOfs, "\tEb", szTemp);
            ulrc++;
        }

        if (p = strstr(pstrText->psz, "\tEk"))
        {
            ULONG ulEASize = _wpQueryEASize(pObject);
            ULONG ulOfs = 0;
            strhThousandsDouble(szTemp, ((ulEASize+500)/1000), pcs->cThousands);
            xstrFindReplaceC(pstrText, &ulOfs, "\tEk", szTemp);
            ulrc++;
        }

        if (p = strstr(pstrText->psz, "\tEK"))
        {
            ULONG ulEASize = _wpQueryEASize(pObject);
            ULONG ulOfs = 0;
            strhThousandsDouble(szTemp, ((ulEASize+512)/1024), pcs->cThousands);
            xstrFindReplaceC(pstrText, &ulOfs, "\tEK", szTemp);
            ulrc++;
        }

        // V0.9.6
        if (p = strstr(pstrText->psz, "\tEa")) // easize
        {
            ULONG ulEASize = _wpQueryEASize(pObject);
            ULONG ulOfs = 0;
            stbVar1000Double(szTemp, ulEASize, pNLSStrings, pcs->cThousands);
            xstrFindReplaceC(pstrText, &ulOfs, "\tEa", szTemp);
            ulrc++;
        }

        if (p = strstr(pstrText->psz, "\tEA")) // easize
        {
            ULONG ulEASize = _wpQueryEASize(pObject);
            ULONG ulOfs = 0;
            stbVar1024Double(szTemp, ulEASize, pNLSStrings, pcs->cThousands);
            xstrFindReplaceC(pstrText, &ulOfs, "\tEA", szTemp);
            ulrc++;
        }
        // end V0.9.6

        if (p = strstr(pstrText->psz, "\tr")) // real name
        {
            ULONG ulOfs = 0;
            strcpy(szTemp, "?");
            _wpQueryFilename(pObject, szTemp, FALSE);
            xstrFindReplaceC(pstrText, &ulOfs, "\tr", szTemp);
            ulrc++;
        }
    }

    /*
     * WPProgram:
     *
     */

    else if (_somIsA(pObject, _WPProgram))
    {
        PPROGDETAILS pProgDetails = NULL;
        ULONG       ulSize;

        /* single-object status bar text mnemonics understood by WPFileSystem
           (in addition to those introduced by XFldObject):

            $p      executable program file (as specified in the Settings)
            $P      parameter list (as specified in the Settings)
            $d      working directory (as specified in the Settings)
         */

        if (p = strstr(pstrText->psz, "\tp"))  // program executable
        {
            ULONG ulOfs = 0;
            strcpy(szTemp, "?");
            if (!pProgDetails)
                if ((_wpQueryProgDetails(pObject, (PPROGDETAILS)NULL, &ulSize)))
                    if ((pProgDetails = (PPROGDETAILS)_wpAllocMem(pObject, ulSize, NULL)) != NULL)
                        _wpQueryProgDetails(pObject, pProgDetails, &ulSize);

            if (pProgDetails)
                if (pProgDetails->pszExecutable)
                    strcpy(szTemp, pProgDetails->pszExecutable);
                else strcpy(szTemp, "");

            xstrFindReplaceC(pstrText, &ulOfs, "\tp", szTemp);
            ulrc++;
        }

        if (p = strstr(pstrText->psz, "\tP"))  // program executable
        {
            ULONG ulOfs = 0;
            strcpy(szTemp, "?");
            if (!pProgDetails)
                if ((_wpQueryProgDetails(pObject, (PPROGDETAILS)NULL, &ulSize)))
                    if ((pProgDetails = (PPROGDETAILS)_wpAllocMem(pObject, ulSize, NULL)) != NULL)
                        _wpQueryProgDetails(pObject, pProgDetails, &ulSize);

            if (pProgDetails)
                if (pProgDetails->pszParameters)
                    strcpy(szTemp, pProgDetails->pszParameters);
                else strcpy(szTemp, "");

            xstrFindReplaceC(pstrText, &ulOfs, "\tP", szTemp);
            ulrc++;
        }

        if (p = strstr(pstrText->psz, "\td"))  // startup dir
        {
            ULONG ulOfs = 0;
            strcpy(szTemp, "?");
            if (!pProgDetails)
                if ((_wpQueryProgDetails(pObject, (PPROGDETAILS)NULL, &ulSize)))
                    if ((pProgDetails = (PPROGDETAILS)_wpAllocMem(pObject, ulSize, NULL)) != NULL)
                        _wpQueryProgDetails(pObject, pProgDetails, &ulSize);

            if (pProgDetails)
                if (pProgDetails->pszStartupDir)
                    strcpy(szTemp, pProgDetails->pszStartupDir);
                else strcpy(szTemp, "");

            xstrFindReplaceC(pstrText, &ulOfs, "\td", szTemp);
            ulrc++;
        }

        if (pProgDetails)
            _wpFreeMem(pObject, (PBYTE)pProgDetails);
    }

    /*
     * XFldObject:
     *
     */

    if (_somIsA(pObject, _WPObject))      // should always be TRUE
    {
        /* single-object status bar text mnemonics understood by WPObject:
             $t      object title
             $w      WPS class default title (e.g. "Data file")
             $W      WPS class name (e.g. WPDataFile)
         */

        if (p = strstr(pstrText->psz, "\tw"))     // class default title
        {
            SOMClass *pClassObject = _somGetClass(pObject);
            ULONG ulOfs = 0;
            if (pClassObject)
                xstrFindReplaceC(pstrText, &ulOfs, "\tw", _wpclsQueryTitle(pClassObject));
            else
                xstrFindReplaceC(pstrText, &ulOfs, "\tw", "?");
            ulrc++;
        }

        if (p = strstr(pstrText->psz, "\tW"))     // class name
        {
            ULONG ulOfs = 0;
            strcpy(szTemp, "?");
            xstrFindReplaceC(pstrText, &ulOfs, "\tW", _somGetClassName(pObject));
            ulrc++;
        }

        if (p = strstr(pstrText->psz, "\tt"))          // object title
        {
            ULONG ulOfs = 0;
            strcpy(szTemp, "?");
            strcpy(szTemp, _wpQueryTitle(pObject));
            strhBeautifyTitle(szTemp);
            xstrFindReplaceC(pstrText, &ulOfs, "\tt", szTemp);
            ulrc++;
        }
    }

    return (ulrc);
}

/*
 *@@ stbComposeText:
 *      this is the main entry point to the status bar
 *      logic which gets called by the status bar wnd
 *      proc (fdr_fnwpStatusBar) when the status
 *      bar text needs updating (CN_EMPHASIS).
 *
 *      This function does the following:
 *
 *      1)  First, it finds out which mode the status bar
 *          should operate in (depending on how many objects
 *          are currently selected in hwndCnr).
 *
 *      2)  Then, we find out which mnemonic string to use,
 *          depending on the mode we found out above.
 *          If we're in one-object mode, we call
 *          stbQueryClassMnemonics for the mnemonics to
 *          use depending on the class of the one object
 *          which is selected.
 *
 *      3)  If we're in one-object mode, we call
 *          stbTranslateSingleMnemonics to have class-dependend
 *          mnemonics translated.
 *
 *      4)  Finally, for all modes, we translate the
 *          mnemonics which are valid for all modes in this
 *          function directly.
 *
 *      This returns the new status bar text or NULL upon errors.
 *      The return PSZ must be free()'d by the caller.
 *
 *@@changed V0.9.0 [umoeller]: prototype changed to return new buffer instead of BOOL
 *@@changed V0.9.0 [umoeller]: added shadow dereferencing
 *@@changed V0.9.3 (2000-04-08) [umoeller]: added cnr error return code check
 *@@changed V0.9.5 (2000-10-07) [umoeller]: added "Dereference shadows" for multiple mode
 *@@changed V0.9.6 (2000-11-01) [umoeller]: now using all new xstrFindReplace
 *@@changed V0.9.6 (2000-10-30) [pr]: fixed container item counts
 *@@changed V0.9.6 (2000-11-12) [pr]: new status bar mnemonics
 */

PSZ stbComposeText(WPFolder* somSelf,      // in:  open folder with status bar
                   HWND hwndCnr)           // in:  cnr hwnd of that folder's open view
{
   /* Generic status bar mnemonics handled here (for other than
      "single object" mode too):

        $c      no. of selected objects
        $C      total object count

        $fb     free space on drive in bytes
        $fk     free space on drive in kBytes
        $fK     free space on drive in KBytes
        $fm     free space on drive in mBytes
        $fM     free space on drive in MBytes
        $fa     free space on drive in bytes/kBytes/mBytes/gBytes (V0.9.6)
        $fA     free space on drive in bytes/KBytes/MBytes/GBytes (V0.9.6)

        $sb     size of selected objects in bytes
        $sk     size of selected objects in kBytes
        $sK     size of selected objects in KBytes
        $sm     size of selected objects in mBytes
        $sM     size of selected objects in MBytes
        $sa     size of selected objects in bytes/kBytes/mBytes/gBytes (V0.9.6)
        $sA     size of selected objects in bytes/KBytes/MBytes/GBytes (V0.9.6)

        $Sb     size of folder content in bytes
        $Sk     size of folder content in kBytes
        $SK     size of folder content in KBytes
        $Sm     size of folder content in mBytes
        $SM     size of folder content in MBytes
        $Sa     size of folder content in bytes/kBytes/mBytes/gBytes (V0.9.6)
        $SA     size of folder content in bytes/KBytes/MBytes/GBytes (V0.9.6)

       The "single object" mode mnemonics are translated using
       the above functions afterwards.
    */

    XSTRING     strText;

    // CNRINFO     CnrInfo;
    PMINIRECORDCORE pmrcSelected, pmrc2;
    ULONG       ulSelectedCount = 0,
                ulSizeSelected = 0,
                ulSizeTotal = 0,
                ulCount = 0;
    USHORT      cmd;
    WPObject    *pObject = NULL,
                *pObject2 = NULL;
    PSZ         p;
    CHAR        *p2;
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();
    PRECORDCORE pRecCore;

    // get thousands separator from "Country" object
    CHAR        cThousands = cmnQueryThousandsSeparator(),
                szTemp[300];

    xstrInit(&strText, 300);

    // go thru all the selected objects in the container
    // and sum up the size of the corresponding objects in
    // ulSizeSelected
    pmrcSelected = (PMINIRECORDCORE)CMA_FIRST;
    do {
        pmrcSelected =
            (PMINIRECORDCORE)WinSendMsg(hwndCnr,
                                        CM_QUERYRECORDEMPHASIS,
                                        (MPARAM)pmrcSelected, // CMA_FIRST at first loop
                                        (MPARAM)CRA_SELECTED);
        if ((LONG)pmrcSelected == -1)
        {
            // error: V0.9.3 (2000-04-08) [umoeller]
            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                       "Unable to query container records for status bar.");
            break;
        }

        if (pmrcSelected)
        {
            ulSelectedCount++;
            // get the object from the record core
            pObject = OBJECT_FROM_PREC(pmrcSelected);
            if (pObject)
                if (wpshCheckObject(pObject))
                {
                    WPObject *pDeref = pObject;
                    if (pGlobalSettings->bDereferenceShadows & STBF_DEREFSHADOWS_MULTIPLE)
                    {
                        // deref multiple shadows
                        while ((pDeref) && (_somIsA(pDeref, _WPShadow)))
                            pDeref = _wpQueryShadowedObject(pDeref, TRUE);
                    }

                    if (_somIsA(pDeref, _WPFileSystem))
                        ulSizeSelected += _wpQueryFileSize(pDeref);
                }
                else
                    pObject = NULL;
        }
    } while (pmrcSelected);

    // now get the mnemonics which have been set by the
    // user on the "Status bar" page, depending on how many
    // objects are selected
    if ( (ulSelectedCount == 0) || (pObject == NULL) )
        // "no object" mode
        xstrcpy(&strText, cmnQueryStatusBarSetting(SBS_TEXTNONESEL), 0);
    else if (ulSelectedCount == 1)
    {
        // "single-object" mode: query the text to translate
        // from the object, because we can implement
        // different mnemonics for different WPS classes

        // dereference shadows (V0.9.0)
        if (pGlobalSettings->bDereferenceShadows & STBF_DEREFSHADOWS_SINGLE)
            if (_somIsA(pObject, _WPShadow))
                pObject = _wpQueryShadowedObject(pObject, TRUE);

        if (pObject == NULL)
            return(strdup(""));

        xstrcpy(&strText, stbQueryClassMnemonics(_somGetClass(pObject)), 0);
                                                  // object's class object
    }
    else
        // "multiple objects" mode
        xstrcpy(&strText, cmnQueryStatusBarSetting(SBS_TEXTMULTISEL), 0);

    // before actually translating any "$" keys, all the
    // '$' characters in the mnemonic string are changed to
    // the tabulator character ('\t') to avoid having '$'
    // characters translated which might only have been
    // inserted during the translation, e.g. because a
    // filename contains a '$' character.
    // All the translation logic will then only search for
    // those tab characters.
    while (p2 = strchr(strText.psz, '$'))
        *p2 = '\t';

    if (ulSelectedCount == 1)
    {
        // "single-object" mode: translate
        // object-specific mnemonics first
        stbTranslateSingleMnemonics(pObject,               // the object itself
                                    &strText);
        if (_somIsA(pObject, _WPFileSystem))
            // if we have a file-system object, we
            // need to re-query its size, because
            // we might have dereferenced a shadow
            // above (whose size was 0 -- V0.9.0)
            ulSizeSelected = _wpQueryFileSize(pObject);
    }

    // query total object count (CnrInfo.cRecords)
    /* WinSendMsg(hwndCnr,
               CM_QUERYCNRINFO,
               (MPARAM)(&CnrInfo),
               (MPARAM)(sizeof(CnrInfo))); */
     // query total visible object count
     // V0.9.6 (2000-10-30) [pr]
     for (pRecCore = NULL, cmd = CMA_FIRST;
          ;
          cmd = CMA_NEXT)
     {
         pRecCore = WinSendMsg(hwndCnr,
                               CM_QUERYRECORD,
                               (MPARAM) pRecCore,
                               MPFROM2SHORT(cmd, CMA_ITEMORDER));
         if (pRecCore == NULL || (ULONG) pRecCore == -1)
             break;

         if (!(pRecCore->flRecordAttr & CRA_FILTERED))
             ulCount++;
     }

    // if we have a "total size" query, also sum up the size of
    // the whole folder into ulSizeTotal, which will be handled
    // in detail below
    if (strstr(strText.psz, "\tS"))
    {
        #ifdef DEBUG_STATUSBARS
            _Pmpf(("Calculating total size"));
        #endif
        pmrc2 = NULL;
        do {
            pmrc2 =
                (PMINIRECORDCORE)WinSendMsg(hwndCnr,
                                            CM_QUERYRECORD,
                                            (MPARAM)pmrc2,
                                            MPFROM2SHORT((pmrc2)
                                                            ? CMA_NEXT
                                                            : CMA_FIRST, // for first loop
                                                        CMA_ITEMORDER)  // doesn't matter
                                           );
            if (pmrc2)
            {
                pObject2 = OBJECT_FROM_PREC(pmrc2);
                if (pObject2)
                    if (wpshCheckObject(pObject2))
                        if (_somIsA(pObject2, _WPFileSystem))
                            ulSizeTotal += _wpQueryFileSize(pObject2);
            }
        } while (pmrc2);

        #ifdef DEBUG_STATUSBARS
            _Pmpf(("  Result: %u", ulSizeTotal));
        #endif
    }

    if (p = strstr(strText.psz, "\tc")) // selected objs count
    {
        ULONG ulOfs = 0;
        sprintf(szTemp, "%u", ulSelectedCount);
        xstrFindReplaceC(&strText, &ulOfs, "\tc", szTemp);
    }

    if (p = strstr(strText.psz, "\tC")) // total obj count
    {
        ULONG ulOfs = 0;
        sprintf(szTemp, "%u", ulCount);
        xstrFindReplaceC(&strText, &ulOfs, "\tC", szTemp);
    }

    // the following are for free space on drive

    if (p = strstr(strText.psz, "\tfb"))
    {
        ULONG ulOfs = 0;
        strhThousandsDouble(szTemp,
                            wpshQueryDiskFreeFromFolder(somSelf),
                            cThousands);
        xstrFindReplaceC(&strText, &ulOfs, "\tfb", szTemp);
    }

    if (p = strstr(strText.psz, "\tfk"))
    {
        ULONG ulOfs = 0;
        strhThousandsDouble(szTemp,
                            ((wpshQueryDiskFreeFromFolder(somSelf) + 500) / 1000),
                            cThousands);
        xstrFindReplaceC(&strText, &ulOfs, "\tfk", szTemp);
    }

    if (p = strstr(strText.psz, "\tfK"))
    {
        ULONG ulOfs = 0;
        strhThousandsDouble(szTemp,
                            ((wpshQueryDiskFreeFromFolder(somSelf) + 512) / 1024),
                            cThousands);
        xstrFindReplaceC(&strText, &ulOfs, "\tfK", szTemp);
    }

    if (p = strstr(strText.psz, "\tfm"))
    {
        ULONG ulOfs = 0;
        strhThousandsDouble(szTemp,
                            ((wpshQueryDiskFreeFromFolder(somSelf) +500000) / 1000000),
                            cThousands);
        xstrFindReplaceC(&strText, &ulOfs, "\tfm", szTemp);
    }

    if (p = strstr(strText.psz, "\tfM"))
    {
        ULONG ulOfs = 0;
        strhThousandsDouble(szTemp,
                            ((wpshQueryDiskFreeFromFolder(somSelf) + (1024*1024/2)) / (1024*1024)),
                            cThousands);
        xstrFindReplaceC(&strText, &ulOfs, "\tfM", szTemp);
    }

    // V0.9.6
    if (p = strstr(strText.psz, "\tfa"))
    {
        ULONG ulOfs = 0;
        stbVar1000Double(szTemp,
                         wpshQueryDiskFreeFromFolder(somSelf),
                         pNLSStrings,
                         cThousands);
        xstrFindReplaceC(&strText, &ulOfs, "\tfa", szTemp);
    }

    if (p = strstr(strText.psz, "\tfA"))
    {
        ULONG ulOfs = 0;
        stbVar1024Double(szTemp,
                         wpshQueryDiskFreeFromFolder(somSelf),
                         pNLSStrings,
                         cThousands);
        xstrFindReplaceC(&strText, &ulOfs, "\tfA", szTemp);
    }
    // end V0.9.6

    // the following are for SELECTED size
    if (p = strstr(strText.psz, "\tsb"))
    {
        ULONG ulOfs = 0;
        strhThousandsULong(szTemp, ulSizeSelected, cThousands);
        xstrFindReplaceC(&strText, &ulOfs, "\tsb", szTemp);
    }

    if (p = strstr(strText.psz, "\tsk"))
    {
        ULONG ulOfs = 0;
        strhThousandsULong(szTemp, ((ulSizeSelected+500) / 1000), cThousands);
        xstrFindReplaceC(&strText, &ulOfs, "\tsk", szTemp);
    }

    if (p = strstr(strText.psz, "\tsK"))
    {
        ULONG ulOfs = 0;
        strhThousandsULong(szTemp, ((ulSizeSelected+512) / 1024), cThousands);
        xstrFindReplaceC(&strText, &ulOfs, "\tsK", szTemp);
    }

    if (p = strstr(strText.psz, "\tsm"))
    {
        ULONG ulOfs = 0;
        strhThousandsULong(szTemp, ((ulSizeSelected+500000) / 1000000), cThousands);
        xstrFindReplaceC(&strText, &ulOfs, "\tsm", szTemp);
    }

    if (p = strstr(strText.psz, "\tsM"))
    {
        ULONG ulOfs = 0;
        strhThousandsULong(szTemp, ((ulSizeSelected+(1024*1024/2)) / (1024*1024)), cThousands);
        xstrFindReplaceC(&strText, &ulOfs, "\tsM", szTemp);
    }

    // V0.9.6
    if (p = strstr(strText.psz, "\tsa"))
    {
        ULONG ulOfs = 0;
        stbVar1000Double(szTemp, (double) ulSizeSelected, pNLSStrings, cThousands);
        xstrFindReplaceC(&strText, &ulOfs, "\tsa", szTemp);
    }

    if (p = strstr(strText.psz, "\tsA"))
    {
        ULONG ulOfs = 0;
        stbVar1024Double(szTemp, (double) ulSizeSelected, pNLSStrings, cThousands);
        xstrFindReplaceC(&strText, &ulOfs, "\tsA", szTemp);
    }
    // end V0.9.6

    // the following are for TOTAL folder size
    if (p = strstr(strText.psz, "\tSb"))
    {
        ULONG ulOfs = 0;
        strhThousandsULong(szTemp, ulSizeTotal, cThousands);
        xstrFindReplaceC(&strText, &ulOfs, "\tSb", szTemp);
    }

    if (p = strstr(strText.psz, "\tSk"))
    {
        ULONG ulOfs = 0;
        strhThousandsULong(szTemp, ((ulSizeTotal+500) / 1000), cThousands);
        xstrFindReplaceC(&strText, &ulOfs, "\tSk", szTemp);
    }

    if (p = strstr(strText.psz, "\tSK"))
    {
        ULONG ulOfs = 0;
        strhThousandsULong(szTemp, ((ulSizeTotal+512) / 1024), cThousands);
        xstrFindReplaceC(&strText, &ulOfs, "\tSK", szTemp);
    }

    if (p = strstr(strText.psz, "\tSm"))
    {
        ULONG ulOfs = 0;
        strhThousandsULong(szTemp, ((ulSizeTotal+500000) / 1000000), cThousands);
        xstrFindReplaceC(&strText, &ulOfs, "\tSm", szTemp);
    }

    if (p = strstr(strText.psz, "\tSM"))
    {
        ULONG ulOfs = 0;
        strhThousandsULong(szTemp, ((ulSizeTotal+(1024*1024/2)) / (1024*1024)), cThousands);
        xstrFindReplaceC(&strText, &ulOfs, "\tSM", szTemp);
    }

    // V0.9.6
    if (p = strstr(strText.psz, "\tSa"))
    {
        ULONG ulOfs = 0;
        stbVar1000Double(szTemp, (double) ulSizeTotal, pNLSStrings, cThousands);
        xstrFindReplaceC(&strText, &ulOfs, "\tSa", szTemp);
    }

    if (p = strstr(strText.psz, "\tSA"))
    {
        ULONG ulOfs = 0;
        stbVar1024Double(szTemp, (double) ulSizeTotal, pNLSStrings, cThousands);
        xstrFindReplaceC(&strText, &ulOfs, "\tSA", szTemp);
    }
    // end V0.9.6

    if (strText.ulLength)
    {
        // we have something:

        // now translate remaining '\t' characters back into
        // '$' characters; this might happen if the user actually
        // wanted to see a '$' character displayed
        while (p2 = strchr(strText.psz, '\t'))
            *p2 = '$';

        return (strText.psz);
    }

    // else:
    xstrClear(&strText);
    return (NULL);
}

/* ******************************************************************
 *                                                                  *
 *   Notebook callbacks (notebook.c) for "Status bars" pages        *
 *                                                                  *
 ********************************************************************/

CHAR                    szSBTextNoneSelBackup[CCHMAXMNEMONICS],
                        szSBText1SelBackup[CCHMAXMNEMONICS],
                        szSBTextMultiSelBackup[CCHMAXMNEMONICS],
                        szSBClassSelected[256];
SOMClass                *pSBClassObjectSelected = NULL;
somId                   somidClassSelected;

// struct passed to callbacks
typedef struct _STATUSBARSELECTCLASS
{
    HWND            hwndOKButton;
    PNLSSTRINGS     pNLSStrings;
} STATUSBARSELECTCLASS, *PSTATUSBARSELECTCLASS;

/*
 *@@ fncbWPSStatusBarReturnClassAttr:
 *      this callback function is called for every single
 *      record core which represents a WPS class; we need
 *      to return the record core attributes.
 *
 *      This gets called from the class list functions in
 *      classlst.c.
 *
 *@@changed V0.9.0 [umoeller]: moved this func here from xfwps.c
 */

MRESULT EXPENTRY fncbWPSStatusBarReturnClassAttr(HWND hwndCnr,
                                                 ULONG ulscd,   // SELECTCLASSDATA struct
                                                 MPARAM mpwps,  // current WPSLISTITEM struct
                                                 MPARAM mpreccParent) // parent record core
{
    USHORT              usAttr = CRA_RECORDREADONLY | CRA_COLLAPSED | CRA_DISABLED;
    PWPSLISTITEM        pwps = (PWPSLISTITEM)mpwps;
    PSELECTCLASSDATA    pscd = (PSELECTCLASSDATA)ulscd;
    PRECORDCORE         preccParent = NULL;

    if (pwps)
    {
        #ifdef DEBUG_SETTINGS
            _Pmpf(("Checking %s", pwps->pszClassName));
        #endif
        if (pwps->pClassObject)
        {
            // now check if the class supports new sort mnemonics
            if (stbClassAddsNewMnemonics(pwps->pClassObject))
            {
                // class _does_ support mnemonics: give the
                // new recc attr the "in use" flag
                usAttr = CRA_RECORDREADONLY | CRA_COLLAPSED | CRA_INUSE;

                // and select it if the settings notebook wants it
                if (strcmp(pwps->pszClassName, pscd->szClassSelected) == 0)
                    usAttr |= CRA_SELECTED;

                // expand all the parent records of the new record
                // so that classes with status bar mnemonics are
                // all initially visible in the container
                preccParent = (PRECORDCORE)mpreccParent;
                while (preccParent)
                {
                    WinSendMsg(hwndCnr,
                               CM_EXPANDTREE,
                               (MPARAM)preccParent,
                               MPNULL);

                    // get next higher parent
                    preccParent = WinSendMsg(hwndCnr,
                                             CM_QUERYRECORD,
                                             preccParent,
                                             MPFROM2SHORT(CMA_PARENT,
                                                          CMA_ITEMORDER));
                    if (preccParent == (PRECORDCORE)-1)
                        // none: stop
                        preccParent = NULL;
                } // end while (preccParent)
            }
        } // end if if (pwps->pClassObject)
        else
            // invalid class: hide in cnr
            usAttr = CRA_FILTERED;
    }
    return (MPARAM)(usAttr);
}

/*
 *@@ fncbWPSStatusBarClassSelected:
 *      callback func for class selected;
 *      mphwndInfo has been set to the static control hwnd.
 *      Returns TRUE if the selection is valid; the dlg func
 *      will then enable the OK button.
 *
 *      This gets called from the class list functions in
 *      classlst.c.
 *
 *@@changed V0.9.0 [umoeller]: moved this func here from xfwps.c
 */

MRESULT EXPENTRY fncbWPSStatusBarClassSelected(HWND hwndCnr,
                                               ULONG ulpsbsc,
                                               MPARAM mpwps,
                                               MPARAM mphwndInfo)
{
    PWPSLISTITEM pwps = (PWPSLISTITEM)mpwps;
    PSTATUSBARSELECTCLASS psbsc = (PSTATUSBARSELECTCLASS)ulpsbsc;
    CHAR szInfo[2000];
    MRESULT mrc = (MPARAM)FALSE;
    PSZ pszClassTitle;

    strcpy(szInfo, pwps->pszClassName);

    if (pwps->pClassObject)
    {
        pszClassTitle = _wpclsQueryTitle(pwps->pClassObject);
        if (pszClassTitle)
            sprintf(szInfo, "%s (\"%s\")\n",
                    pwps->pszClassName,
                    pszClassTitle);
    }

    if (pwps->pRecord->flRecordAttr & CRA_INUSE)
    {
        sprintf(szInfo, "%s\n%s",
                        (psbsc->pNLSStrings)->pszSBClassMnemonics,
                        stbQueryClassMnemonics(pwps->pClassObject));
        mrc = (MPARAM)TRUE;
    } else
        strcpy(szInfo,
               psbsc->pNLSStrings->pszSBClassNotSupported);

    WinSetWindowText((HWND)mphwndInfo, szInfo);

    return (mrc);
}

/*
 *@@ stbStatusBar1InitPage:
 *      notebook callback function (notebook.c) for the
 *      first "Status bars" page in the "Workplace Shell" object.
 *      Sets the controls on the page according to the
 *      Global Settings.
 *
 *@@changed V0.9.0 [umoeller]: adjusted function prototype
 *@@changed V0.9.0 [umoeller]: moved this func here from xfwps.c
 */

VOID stbStatusBar1InitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                           ULONG flFlags)        // CBI_* flags (notebook.h)
{
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    if (flFlags & CBI_INIT)
    {
        if (pcnbp->pUser == NULL)
        {
            // first call: backup Global Settings for "Undo" button;
            // this memory will be freed automatically by the
            // common notebook window function (notebook.c) when
            // the notebook page is destroyed
            pcnbp->pUser = malloc(sizeof(GLOBALSETTINGS));
            memcpy(pcnbp->pUser, pGlobalSettings, sizeof(GLOBALSETTINGS));
        }
    }

    if (flFlags & CBI_SET)
    {
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_ENABLESTATUSBAR,
                              pGlobalSettings->fDefaultStatusBarVisibility);

        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_SBFORICONVIEWS,
                              (pGlobalSettings->SBForViews & SBV_ICON) != 0);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_SBFORTREEVIEWS,
                              (pGlobalSettings->SBForViews & SBV_TREE) != 0);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_SBFORDETAILSVIEWS,
                              (pGlobalSettings->SBForViews & SBV_DETAILS) != 0);

        if (pGlobalSettings->SBStyle == SBSTYLE_WARP3RAISED)
            winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_SBSTYLE_3RAISED, TRUE);
        else if (pGlobalSettings->SBStyle == SBSTYLE_WARP3SUNKEN)
            winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_SBSTYLE_3SUNKEN, TRUE);
        else if (pGlobalSettings->SBStyle == SBSTYLE_WARP4RECT)
            winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_SBSTYLE_4RECT, TRUE);
        else
            winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_SBSTYLE_4MENU, TRUE);
    }

    if (flFlags & CBI_ENABLE)
    {
        BOOL fEnable = !(pGlobalSettings->fNoSubclassing);
        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_ENABLESTATUSBAR, fEnable);
        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_SBSTYLE_3RAISED, fEnable);
        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_SBSTYLE_3SUNKEN, fEnable);
        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_SBSTYLE_4MENU,   fEnable);
        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_SBSTYLE_4RECT,   fEnable);

        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_SBFORICONVIEWS,   fEnable);
        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_SBFORTREEVIEWS,   fEnable);
        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_SBFORDETAILSVIEWS,   fEnable);
    }
}

/*
 *@@ stbStatusBar1ItemChanged:
 *      notebook callback function (notebook.c) for the
 *      second "Status bars" page in the "Workplace Shell" object.
 *      Reacts to changes of any of the dialog controls.
 *
 *@@changed V0.9.0 [umoeller]: adjusted function prototype
 *@@changed V0.9.0 [umoeller]: moved this func here from xfwps.c
 */

MRESULT stbStatusBar1ItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                     USHORT usItemID,
                                     USHORT usNotifyCode,
                                     ULONG ulExtra)      // for checkboxes: contains new state
{
    GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(__FILE__, __LINE__, __FUNCTION__);
    MRESULT mrc = (MPARAM)0;
    BOOL fSave = TRUE,
         fShowStatusBars = FALSE,
         fRefreshStatusBars = FALSE;

    switch (usItemID)
    {
        case ID_XSDI_ENABLESTATUSBAR:
            pGlobalSettings->fDefaultStatusBarVisibility = ulExtra;
            fShowStatusBars = TRUE;
        break;

        case ID_XSDI_SBFORICONVIEWS:
            if (ulExtra)
                pGlobalSettings->SBForViews |= SBV_ICON;
            else
                pGlobalSettings->SBForViews &= ~SBV_ICON;
            fShowStatusBars = TRUE;
        break;

        case ID_XSDI_SBFORTREEVIEWS:
            if (ulExtra)
                pGlobalSettings->SBForViews |= SBV_TREE;
            else
                pGlobalSettings->SBForViews &= ~SBV_TREE;
            fShowStatusBars = TRUE;
        break;

        case ID_XSDI_SBFORDETAILSVIEWS:
            if (ulExtra)
                pGlobalSettings->SBForViews |= SBV_DETAILS;
            else
                pGlobalSettings->SBForViews &= ~SBV_DETAILS;
            fShowStatusBars = TRUE;
        break;

        case ID_XSDI_SBSTYLE_3RAISED:
            pGlobalSettings->SBStyle = SBSTYLE_WARP3RAISED;
            fRefreshStatusBars = TRUE;
        break;

        case ID_XSDI_SBSTYLE_3SUNKEN:
            pGlobalSettings->SBStyle = SBSTYLE_WARP3SUNKEN;
            fRefreshStatusBars = TRUE;
        break;

        case ID_XSDI_SBSTYLE_4RECT:
            pGlobalSettings->SBStyle = SBSTYLE_WARP4RECT;
            fRefreshStatusBars = TRUE;
        break;

        case ID_XSDI_SBSTYLE_4MENU:
            pGlobalSettings->SBStyle = SBSTYLE_WARP4MENU;
            fRefreshStatusBars = TRUE;
        break;

        case DID_UNDO:
        {
            // "Undo" button: get pointer to backed-up Global Settings
            PCGLOBALSETTINGS pGSBackup = (PCGLOBALSETTINGS)(pcnbp->pUser);

            // and restore the settings for this page
            pGlobalSettings->fDefaultStatusBarVisibility  = pGSBackup->fDefaultStatusBarVisibility;
            pGlobalSettings->SBForViews = pGSBackup->SBForViews;
            pGlobalSettings->SBStyle = pGSBackup->SBStyle;

            // update the display by calling the INIT callback
            (*(pcnbp->pfncbInitPage))(pcnbp, CBI_SET | CBI_ENABLE);
            fRefreshStatusBars = TRUE;
            fShowStatusBars = TRUE;
        break; }

        case DID_DEFAULT:
        {
            // set the default settings for this settings page
            // (this is in common.c because it's also used at
            // WPS startup)
            cmnSetDefaultSettings(pcnbp->ulPageID);
            // update the display by calling the INIT callback
            (*(pcnbp->pfncbInitPage))(pcnbp, CBI_SET | CBI_ENABLE);
            fRefreshStatusBars = TRUE;
            fShowStatusBars = TRUE;
        break; }

        default:
            fSave = FALSE;
    }

    cmnUnlockGlobalSettings();

    if (fSave)
        cmnStoreGlobalSettings();

    // have the Worker thread update the
    // status bars for all currently open
    // folders
    if (fRefreshStatusBars)
        xthrPostWorkerMsg(WOM_UPDATEALLSTATUSBARS,
                          (MPARAM)2,
                          MPNULL);

    if (fShowStatusBars)
    {
        xthrPostWorkerMsg(WOM_UPDATEALLSTATUSBARS,
                          (MPARAM)1,
                          MPNULL);
        ntbUpdateVisiblePage(NULL,   // all somSelf's
                             SP_XFOLDER_FLDR);
    }

    return (mrc);
}

/*
 *@@ stbStatusBar2InitPage:
 *      notebook callback function (notebook.c) for the
 *      second "Status bars" page in the "Workplace Shell" object.
 *      Sets the controls on the page according to the
 *      Global Settings.
 *
 *@@changed V0.9.0 [umoeller]: adjusted function prototype
 *@@changed V0.9.0 [umoeller]: added "Dereference shadows"
 *@@changed V0.9.0 [umoeller]: moved this func here from xfwps.c
 *@@changed V0.9.5 (2000-10-07) [umoeller]: added "Dereference shadows" for multiple mode
 */

VOID stbStatusBar2InitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                               ULONG flFlags)        // CBI_* flags (notebook.h)
{
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    if (flFlags & CBI_INIT)
    {
        if (pcnbp->pUser == NULL)
        {
            // first call: backup Global Settings for "Undo" button;
            // this memory will be freed automatically by the
            // common notebook window function (notebook.c) when
            // the notebook page is destroyed
            pcnbp->pUser = malloc(sizeof(GLOBALSETTINGS));
            memcpy(pcnbp->pUser, pGlobalSettings, sizeof(GLOBALSETTINGS));
        }

        strcpy(szSBTextNoneSelBackup,
               cmnQueryStatusBarSetting(SBS_TEXTNONESEL));
        strcpy(szSBTextMultiSelBackup,
               cmnQueryStatusBarSetting(SBS_TEXTMULTISEL));
        // status bar settings page: get last selected
        // class from INIs (for single-object mode)
        // and query the SOM class object from this string
        PrfQueryProfileString(HINI_USER,
                              (PSZ)INIAPP_XWORKPLACE,
                              (PSZ)INIKEY_SB_LASTCLASS,
                              "XFldObject",     // default
                              szSBClassSelected, sizeof(szSBClassSelected));
        if (pSBClassObjectSelected == NULL)
        {
            somidClassSelected = somIdFromString(szSBClassSelected);
            if (somidClassSelected)
                // get pointer to class object (e.g. M_WPObject)
                pSBClassObjectSelected = _somFindClass(SOMClassMgrObject, somidClassSelected, 0, 0);
        }
        if (pSBClassObjectSelected)
            strcpy(szSBText1SelBackup, stbQueryClassMnemonics(pSBClassObjectSelected));
    }

    if (flFlags & CBI_SET)
    {
        // CHAR    szTemp[CCHMAXMNEMONICS];

        // current class
        WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XSDI_SBCURCLASS, szSBClassSelected);

        // no-object mode
        WinSendDlgItemMsg(pcnbp->hwndDlgPage, ID_XSDI_SBTEXTNONESEL,
                          EM_SETTEXTLIMIT,
                          (MPARAM)(CCHMAXMNEMONICS-1),
                          MPNULL);
        WinSetDlgItemText(pcnbp->hwndDlgPage,
                          ID_XSDI_SBTEXTNONESEL ,
                          (PSZ)cmnQueryStatusBarSetting(SBS_TEXTNONESEL));

        // one-object mode
        WinSendDlgItemMsg(pcnbp->hwndDlgPage,
                          ID_XSDI_SBTEXT1SEL,
                          EM_SETTEXTLIMIT,
                          (MPARAM)(CCHMAXMNEMONICS-1),
                          MPNULL);
        if (pSBClassObjectSelected == NULL)
        {
            somidClassSelected = somIdFromString(szSBClassSelected);
            if (somidClassSelected)
                // get pointer to class object (e.g. M_WPObject)
                pSBClassObjectSelected = _somFindClass(SOMClassMgrObject,
                                                       somidClassSelected, 0, 0);
        }
        if (pSBClassObjectSelected)
            WinSetDlgItemText(pcnbp->hwndDlgPage,
                              ID_XSDI_SBTEXT1SEL,
                              stbQueryClassMnemonics(pSBClassObjectSelected));

        // dereference shadows
        winhSetDlgItemChecked(pcnbp->hwndDlgPage,
                              ID_XSDI_DEREFSHADOWS_SINGLE,
                              (pGlobalSettings->bDereferenceShadows & STBF_DEREFSHADOWS_SINGLE)
                                    != 0);

        // multiple-objects mode
        WinSendDlgItemMsg(pcnbp->hwndDlgPage,
                          ID_XSDI_SBTEXTMULTISEL,
                          EM_SETTEXTLIMIT,
                          (MPARAM)(CCHMAXMNEMONICS-1),
                          MPNULL);
        WinSetDlgItemText(pcnbp->hwndDlgPage,
                          ID_XSDI_SBTEXTMULTISEL,
                          (PSZ)cmnQueryStatusBarSetting(SBS_TEXTMULTISEL));

        winhSetDlgItemChecked(pcnbp->hwndDlgPage,
                              ID_XSDI_DEREFSHADOWS_MULTIPLE,
                              (pGlobalSettings->bDereferenceShadows & STBF_DEREFSHADOWS_MULTIPLE)
                                    != 0);
    }

    if (flFlags & CBI_DESTROY)
        if (pSBClassObjectSelected)
        {
            SOMFree(somidClassSelected);
            pSBClassObjectSelected = NULL;
        }
}

/*
 *@@ stbStatusBar2ItemChanged:
 *      notebook callback function (notebook.c) for the
 *      second "Status bars" page in the "Workplace Shell" object.
 *      Reacts to changes of any of the dialog controls.
 *
 *@@changed V0.9.0 [umoeller]: adjusted entry field handling to new notebook.c handling
 *@@changed V0.9.0 [umoeller]: adjusted function prototype
 *@@changed V0.9.0 [umoeller]: added "Dereference shadows"
 *@@changed V0.9.0 [umoeller]: moved this func here from xfwps.c
 *@@changed V0.9.5 (2000-10-07) [umoeller]: added "Dereference shadows" for multiple mode
 */

MRESULT stbStatusBar2ItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                 USHORT usItemID,
                                 USHORT usNotifyCode,
                                 ULONG ulExtra)      // for checkboxes: contains new state
{
    MRESULT mrc = (MPARAM)0;
    BOOL fSave = TRUE;
    CHAR szDummy[CCHMAXMNEMONICS];

    switch (usItemID)
    {
        case ID_XSDI_SBTEXTNONESEL:
            if (usNotifyCode == EN_KILLFOCUS)   // changed V0.9.0
            {
                WinQueryDlgItemText(pcnbp->hwndDlgPage,
                                    ID_XSDI_SBTEXTNONESEL,
                                    sizeof(szDummy)-1, szDummy);
                cmnSetStatusBarSetting(SBS_TEXTNONESEL, szDummy);

                if (pSBClassObjectSelected == NULL)
                {
                    somidClassSelected = somIdFromString(szSBClassSelected);

                    if (somidClassSelected)
                        // get pointer to class object (e.g. M_WPObject)
                        pSBClassObjectSelected = _somFindClass(SOMClassMgrObject,
                                                               somidClassSelected, 0, 0);
                }
            }
        break;

        case ID_XSDI_SBTEXT1SEL:
            if (usNotifyCode == EN_KILLFOCUS)   // changed V0.9.0
            {
                if (pSBClassObjectSelected)
                {
                    WinQueryDlgItemText(pcnbp->hwndDlgPage, ID_XSDI_SBTEXT1SEL,
                                        sizeof(szDummy)-1, szDummy);
                    stbSetClassMnemonics(pSBClassObjectSelected,
                                         szDummy);
                }
            }
        break;

        case ID_XSDI_DEREFSHADOWS_SINGLE:    // added V0.9.0
        {
            GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(__FILE__, __LINE__, __FUNCTION__);
            if (ulExtra)
                pGlobalSettings->bDereferenceShadows |= STBF_DEREFSHADOWS_SINGLE;
            else
                pGlobalSettings->bDereferenceShadows &= ~STBF_DEREFSHADOWS_SINGLE;
            cmnUnlockGlobalSettings();
        break; }

        case ID_XSDI_SBTEXTMULTISEL:
            if (usNotifyCode == EN_KILLFOCUS)   // changed V0.9.0
            {
                WinQueryDlgItemText(pcnbp->hwndDlgPage, ID_XSDI_SBTEXTMULTISEL,
                                    sizeof(szDummy)-1, szDummy);
                cmnSetStatusBarSetting(SBS_TEXTMULTISEL, szDummy);
            }
        break;

        case ID_XSDI_DEREFSHADOWS_MULTIPLE:    // added V0.9.5 (2000-10-07) [umoeller]
        {
            GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(__FILE__, __LINE__, __FUNCTION__);
            if (ulExtra)
                pGlobalSettings->bDereferenceShadows |= STBF_DEREFSHADOWS_MULTIPLE;
            else
                pGlobalSettings->bDereferenceShadows &= ~STBF_DEREFSHADOWS_MULTIPLE;
            cmnUnlockGlobalSettings();
        break; }

        // "Select class" on "Status Bars" page:
        // set up WPS classes dialog
        case ID_XSDI_SBSELECTCLASS:
        {
            SELECTCLASSDATA         scd;
            STATUSBARSELECTCLASS    sbsc;

            CHAR szTitle[100] = "title";
            CHAR szIntroText[2000] = "intro";

            cmnGetMessage(NULL, 0, szTitle, sizeof(szTitle), 112);
            cmnGetMessage(NULL, 0, szIntroText, sizeof(szIntroText), 113);
            scd.pszDlgTitle = szTitle;
            scd.pszIntroText = szIntroText;
            scd.pszRootClass = "WPObject";
            scd.pszOrphans = NULL;
            strcpy(scd.szClassSelected, szSBClassSelected);

            // these callback funcs are defined way more below
            scd.pfnwpReturnAttrForClass = fncbWPSStatusBarReturnClassAttr;
            scd.pfnwpClassSelected = fncbWPSStatusBarClassSelected;
            // the folllowing data will be passed to the callbacks
            // so the callback can display NLS messages
            sbsc.pNLSStrings = cmnQueryNLSStrings();
            scd.ulUserClassSelected = (ULONG)&sbsc;

            scd.pszHelpLibrary = cmnQueryHelpLibrary();
            scd.ulHelpPanel = ID_XFH_SELECTCLASS;

            // classlst.c
            if (clsSelectWpsClassDlg(pcnbp->hwndFrame, // owner
                                     cmnQueryNLSModuleHandle(FALSE),
                                     ID_XLD_SELECTCLASS,
                                     &scd)
                          == DID_OK)
            {
                strcpy(szSBClassSelected, scd.szClassSelected);
                WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XSDI_SBCURCLASS, szSBClassSelected);
                PrfWriteProfileString(HINI_USER,
                                      (PSZ)INIAPP_XWORKPLACE,
                                      (PSZ)INIKEY_SB_LASTCLASS,
                                      szSBClassSelected);
                if (pSBClassObjectSelected)
                {
                    SOMFree(somidClassSelected);
                    pSBClassObjectSelected = NULL;
                    // this will provoke the following func to re-read
                    // the class's status bar mnemonics
                }

                // update the display by calling the INIT callback
                (*(pcnbp->pfncbInitPage))(pcnbp, CBI_SET | CBI_ENABLE);

                // refresh the "Undo" data for this
                strcpy(szSBText1SelBackup,
                       stbQueryClassMnemonics(pSBClassObjectSelected));

            }
        break; }

        case DID_UNDO:
        {
            // "Undo" button: get pointer to backed-up Global Settings
            // PGLOBALSETTINGS pGSBackup = (PGLOBALSETTINGS)(pcnbp->pUser);

            // and restore the settings for this page
            cmnSetStatusBarSetting(SBS_TEXTNONESEL,
                                   szSBTextNoneSelBackup);
            cmnSetStatusBarSetting(SBS_TEXTMULTISEL,
                                   szSBTextMultiSelBackup);

            if (pSBClassObjectSelected == NULL)
            {
                somidClassSelected = somIdFromString(szSBClassSelected);
                if (somidClassSelected)
                    // get pointer to class object (e.g. M_WPObject)
                    pSBClassObjectSelected = _somFindClass(SOMClassMgrObject,
                                                           somidClassSelected, 0, 0);
            }
            if (pSBClassObjectSelected)
                stbSetClassMnemonics(pSBClassObjectSelected,
                                     szSBText1SelBackup);

            // update the display by calling the INIT callback
            (*(pcnbp->pfncbInitPage))(pcnbp, CBI_SET | CBI_ENABLE);
        break; }

        case DID_DEFAULT:
        {
            // set the default settings for this settings page
            // (this is in common.c because it's also used at
            // WPS startup)
            cmnSetStatusBarSetting(SBS_TEXTNONESEL, NULL);  // load default
            cmnSetStatusBarSetting(SBS_TEXTMULTISEL, NULL); // load default

            if (pSBClassObjectSelected == NULL)
            {
                somidClassSelected = somIdFromString(szSBClassSelected);
                if (somidClassSelected)
                    // get pointer to class object (e.g. M_WPObject)
                    pSBClassObjectSelected = _somFindClass(SOMClassMgrObject, somidClassSelected, 0, 0);
            }
            if (pSBClassObjectSelected)
                stbSetClassMnemonics(pSBClassObjectSelected,
                                     NULL);  // load default

            // update the display by calling the INIT callback
            (*(pcnbp->pfncbInitPage))(pcnbp, CBI_SET | CBI_ENABLE);
        break; }

        default:
            fSave = FALSE;
    }

    if (fSave)
    {
        cmnStoreGlobalSettings();
        // have the Worker thread update the
        // status bars for all currently open
        // folders
        xthrPostWorkerMsg(WOM_UPDATEALLSTATUSBARS,
                          (MPARAM)2, // update display
                          MPNULL);
    }

    return (mrc);
}


