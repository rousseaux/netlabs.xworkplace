
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
 *      --  When selections change in a folder view (i.e. fdr_fnwpSubclassedFolderFrame
 *          receives CN_EMPHASIS notification), fdr_fnwpStatusBar is notified,
 *          which in turn calls stbComposeText, the main entry point to the
 *          mess in this file.
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
 *      Copyright (C) 1997-2000 Ulrich M�ller.
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

// finally, our own header file
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

/* ******************************************************************
 *                                                                  *
 *   Global variables                                               *
 *                                                                  *
 ********************************************************************/

CHAR    szXFldObjectStatusBarMnemonics[CCHMAXMNEMONICS] = "";
CHAR    szWPProgramStatusBarMnemonics[CCHMAXMNEMONICS] = "";
CHAR    szWPDiskStatusBarMnemonics[CCHMAXMNEMONICS] = "";
CHAR    szWPFileSystemStatusBarMnemonics[CCHMAXMNEMONICS] = "";
CHAR    szWPUrlStatusBarMnemonics[CCHMAXMNEMONICS] = "";

// WPUrl class object; to preserve compatibility with Warp 3,
// where this class does not exist, we call the SOM kernel
// explicitly to get the class object.
// The initial value of -1 means that we have not queried
// this class yet. After the first query, this either points
// to the class object or is NULL if the class does not exist.
SOMClass    *_WPUrl = (SOMClass*)-1;

/* ******************************************************************
 *                                                                  *
 *   Status bar mnemonics                                           *
 *                                                                  *
 ********************************************************************/

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
             || ( (_WPUrl != NULL) && (pClassObject == _WPUrl) )
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
 */

BOOL stbSetClassMnemonics(SOMClass *pClassObject,
                          PSZ pszText)
{
    if (_WPUrl == (SOMClass*)-1)
    {
        // WPUrl class object not queried yet: do it now
        somId    somidWPUrl = somIdFromString("WPUrl");
        _WPUrl = _somFindClass(SOMClassMgrObject, somidWPUrl, 0, 0);
        // _WPUrl now either points to the WPUrl class object
        // or is NULL if the class is not installed (Warp 3!).
        // In this case, the object will be treated as a regular
        // file-system object.
    }

    if (_WPUrl)
    {
        if (_somDescendedFrom(pClassObject, _WPUrl))
        {
            // provoke a reload of the settings
            // in stbQueryClassMnemonics
            szWPUrlStatusBarMnemonics[0] = '\0';

            // set the class mnemonics in OS2.INI; if
            // pszText == NULL, the key will be deleted,
            // and stbQueryClassMnemonics will use
            // the default value
            return (PrfWriteProfileString(HINI_USERPROFILE,
                                          INIAPP_XWORKPLACE, INIKEY_SBTEXT_WPURL,
                                          pszText));
        }
    }

    // no WPUrl or WPUrl not installed: continue
    if (_somDescendedFrom(pClassObject, _WPFileSystem))
    {
        // provoke a reload of the settings
        // in stbQueryClassMnemonics
        szWPFileSystemStatusBarMnemonics[0] = '\0';

        // set the class mnemonics in OS2.INI; if
        // pszText == NULL, the key will be deleted,
        // and stbQueryClassMnemonics will use
        // the default value
        return (PrfWriteProfileString(HINI_USERPROFILE,
                                      INIAPP_XWORKPLACE, INIKEY_SBTEXT_WPFILESYSTEM,
                                      pszText));
    }
    else if (_somDescendedFrom(pClassObject, _WPDisk))
    {
        // provoke a reload of the settings
        // in stbQueryClassMnemonics
        szWPDiskStatusBarMnemonics[0] = '\0';

        // set the class mnemonics in OS2.INI; if
        // pszText == NULL, the key will be deleted,
        // and stbQueryClassMnemonics will use
        // the default value
        return (PrfWriteProfileString(HINI_USERPROFILE,
                                      INIAPP_XWORKPLACE, INIKEY_SBTEXT_WPDISK,
                                      pszText));
    }
    else if (_somDescendedFrom(pClassObject, _WPProgram))
    {
        // provoke a reload of the settings
        // in stbQueryClassMnemonics
        szWPProgramStatusBarMnemonics[0] = '\0';

        // set the class mnemonics in OS2.INI; if
        // pszText == NULL, the key will be deleted,
        // and stbQueryClassMnemonics will use
        // the default value
        return (PrfWriteProfileString(HINI_USERPROFILE,
                                      INIAPP_XWORKPLACE, INIKEY_SBTEXT_WPPROGRAM,
                                      pszText));
    }
    else if (_somDescendedFrom(pClassObject, _XFldObject))
    {
        // provoke a reload of the settings
        // in stbQueryClassMnemonics
        szXFldObjectStatusBarMnemonics[0] = '\0';

        // set the class mnemonics in OS2.INI; if
        // pszText == NULL, the key will be deleted,
        // and stbQueryClassMnemonics will use
        // the default value
        return (PrfWriteProfileString(HINI_USERPROFILE,
                                      INIAPP_XWORKPLACE, INIKEY_SBTEXT_WPOBJECT,
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
 *@@changed V0.85 [umoeller]: fixed problem when WPProgram was replaced
 *@@changed V0.9.0 [umoeller]: function prototype changed to return PSZ instead of ULONG
 *@@changed V0.9.0 [umoeller]: now using _WPDisk instead of _XFldDisk
 */

PSZ stbQueryClassMnemonics(SOMClass *pClassObject)    // in: class object of selected object
{
    PSZ     pszReturn = NULL;

    if (_WPUrl == (SOMClass*)-1)
    {
        // WPUrl class object not queried yet: do it now
        somId    somidWPUrl = somIdFromString("WPUrl");
        _WPUrl = _somFindClass(SOMClassMgrObject, somidWPUrl, 0, 0);
        // _WPUrl now either points to the WPUrl class object
        // or is NULL if the class is not installed (Warp 3!).
    }

    if (_WPUrl)
        if (_somDescendedFrom(pClassObject, _WPUrl))
        {
            if (szWPUrlStatusBarMnemonics[0] == '\0')
                // load string if this is the first time
                if (PrfQueryProfileString(HINI_USERPROFILE,
                                          INIAPP_XWORKPLACE, INIKEY_SBTEXT_WPURL,
                                          NULL,
                                          &(szWPUrlStatusBarMnemonics),
                                          sizeof(szWPUrlStatusBarMnemonics))
                        == 0)
                    // string not found in profile: set default
                    strcpy(szWPUrlStatusBarMnemonics, "\"$U\"$x(70%)$D $T");

            pszReturn = szWPUrlStatusBarMnemonics;
            // get out of here
            return (pszReturn);
        }

    if (_somDescendedFrom(pClassObject, _WPFileSystem))
    {
        if (szWPFileSystemStatusBarMnemonics[0] == '\0')
            // load string if this is the first time
            if (PrfQueryProfileString(HINI_USERPROFILE,
                        INIAPP_XWORKPLACE, INIKEY_SBTEXT_WPFILESYSTEM,
                        NULL, &(szWPFileSystemStatusBarMnemonics),
                        sizeof(szWPFileSystemStatusBarMnemonics))
                    == 0)
                // string not found in profile: load default from NLS resources
                WinLoadString(WinQueryAnchorBlock(HWND_DESKTOP),
                              cmnQueryNLSModuleHandle(FALSE),
                              ID_XSSI_SBTEXTWPDATAFILE,
                              sizeof(szWPFileSystemStatusBarMnemonics),
                              szWPFileSystemStatusBarMnemonics);

        pszReturn = szWPFileSystemStatusBarMnemonics;
    }
    //
    else if (_somDescendedFrom(pClassObject, _WPDisk))
    {
        if (szWPDiskStatusBarMnemonics[0] == '\0')
            // load string if this is the first time
            if (PrfQueryProfileString(HINI_USERPROFILE,
                                      INIAPP_XWORKPLACE, INIKEY_SBTEXT_WPDISK,
                                      NULL,
                                      &(szWPDiskStatusBarMnemonics),
                                      sizeof(szWPDiskStatusBarMnemonics))
                    == 0)
                // string not found in profile: load default from NLS resources
                WinLoadString(WinQueryAnchorBlock(HWND_DESKTOP),
                              cmnQueryNLSModuleHandle(FALSE),
                              ID_XSSI_SBTEXTWPDISK,
                              sizeof(szWPDiskStatusBarMnemonics),
                              szWPDiskStatusBarMnemonics);

        pszReturn = szWPDiskStatusBarMnemonics;
    }
    //
    else if (_somDescendedFrom(pClassObject, _WPProgram))  // fixed V0.85
    {
        if (szWPProgramStatusBarMnemonics[0] == '\0')
            // load string if this is the first time
            if (PrfQueryProfileString(HINI_USERPROFILE,
                                      INIAPP_XWORKPLACE, INIKEY_SBTEXT_WPPROGRAM,
                                      NULL,
                                      &(szWPProgramStatusBarMnemonics),
                                      sizeof(szWPProgramStatusBarMnemonics))
                    == 0)
                // string not found in profile: load default from NLS resources
                WinLoadString(WinQueryAnchorBlock(HWND_DESKTOP),
                              cmnQueryNLSModuleHandle(FALSE),
                              ID_XSSI_SBTEXTWPPROGRAM,
                              sizeof(szWPProgramStatusBarMnemonics),
                              szWPProgramStatusBarMnemonics);

        pszReturn = szWPProgramStatusBarMnemonics;
    }
    // subsidiarily: XFldObject
    else if (_somDescendedFrom(pClassObject, _XFldObject))
    {
        // should always be TRUE
        if (szXFldObjectStatusBarMnemonics[0] == '\0')
            // load string if this is the first time
            if (PrfQueryProfileString(HINI_USERPROFILE,
                                      INIAPP_XWORKPLACE, INIKEY_SBTEXT_WPOBJECT,
                                      NULL,
                                      &(szXFldObjectStatusBarMnemonics),
                                      sizeof(szXFldObjectStatusBarMnemonics))
                        == 0)
                // string not found in profile: load default from NLS resources
                WinLoadString(WinQueryAnchorBlock(HWND_DESKTOP),
                              cmnQueryNLSModuleHandle(FALSE),
                              ID_XSSI_SBTEXTWPOBJECT,
                              sizeof(szXFldObjectStatusBarMnemonics),
                              szXFldObjectStatusBarMnemonics);

        pszReturn = szXFldObjectStatusBarMnemonics;
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
 */

ULONG  stbTranslateSingleMnemonics(SOMClass *pObject,  // in: object
                                   PSZ* ppszText,    // in/out: status bar text
                                   CHAR cThousands)  // in: thousands separator
{
    ULONG       ulrc = 0;
    CHAR        szTemp[300];
    PSZ         p;

    /*
     * WPUrl:
     *
     */

    /* first check if the thing is a URL object;
       in addition to the normal WPFileSystem mnemonics,
       URL objects also support the $U mnemonic for
       displaying the URL */

    if (_WPUrl == (SOMClass*)-1)
    {
        // WPUrl class object not queried yet: do it now
        somId    somidWPUrl = somIdFromString("WPUrl");
        _WPUrl = _somFindClass(SOMClassMgrObject, somidWPUrl, 0, 0);
        // _WPUrl now either points to the WPUrl class object
        // or is NULL if the class is not installed (Warp 3!).
    }

    if (_WPUrl)
        if (_somIsA(pObject, _WPUrl))
        {
            // yes, we have a URL object:
            if (p = strstr(*ppszText, "\tU")) // URL mnemonic
            {
                CHAR szFilename[CCHMAXPATH];
                PSZ pszFilename = _wpQueryFilename(pObject, szFilename, TRUE);

                // read in the contents of the file, which
                // contain the URL
                PSZ pszURL = NULL;
                if (pszFilename)
                    doshReadTextFile(pszFilename, &pszURL);
                    if (pszURL)
                    {
                        if (strlen(pszURL) > 100)
                            strcpy(pszURL+97, "...");
                        xstrrpl(ppszText, 0, "\tU", pszURL, 0);
                        free(pszURL);
                    }
                if (!pszURL)
                    xstrrpl(ppszText, 0, "\tU", "?", 0);
                ulrc++;
            }
        }

    /*
     * WPFileSystem:
     *
     */

    if (_somIsA(pObject, _WPFileSystem))
    {
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
         */

        if (p = strstr(*ppszText, "\ty")) // attribs
        {
            PSZ p2 = NULL;
            p2 = _wpQueryType(pObject);
            xstrrpl(ppszText, 0, "\ty", (p2) ? p2 : "?", 0);
            ulrc++;
        }

        if (p = strstr(*ppszText, "\tD"))  // date
        {
            FILEFINDBUF4 ffb4;
            ULONG ulDateFormat = PrfQueryProfileInt(HINI_USER, "PM_National", "iDate", 0);
            CHAR szDateSep[10];
            PrfQueryProfileString(HINI_USER,
                                  "PM_National", "sDate", "/",
                                  szDateSep,
                                  sizeof(szDateSep)-1);
            strcpy(szTemp, "?");
            _wpQueryDateInfo(pObject, &ffb4);
            strhFileDate(szTemp, &(ffb4.fdateLastWrite), ulDateFormat, szDateSep[0]);
            xstrrpl(ppszText, 0, "\tD", szTemp, 0);
            ulrc++;
        }

        if (p = strstr(*ppszText, "\tT"))  // time
        {
            FILEFINDBUF4 ffb4;
            ULONG ulTimeFormat = PrfQueryProfileInt(HINI_USER, "PM_National", "iTime", 0);
            CHAR szTimeSep[10];
            PrfQueryProfileString(HINI_USER,
                                  "PM_National", "sTime", ":",
                                  szTimeSep,
                                  sizeof(szTimeSep)-1);

            strcpy(szTemp, "?");
            _wpQueryDateInfo(pObject, &ffb4);
            strhFileTime(szTemp, &(ffb4.ftimeLastWrite), ulTimeFormat, szTimeSep[0]);
            xstrrpl(ppszText, 0, "\tT", szTemp, 0);
            ulrc++;
        }

        if (p = strstr(*ppszText, "\ta")) // attribs
        {
            ULONG fAttr = _wpQueryAttr(pObject);
            szTemp[0] = (fAttr & FILE_ARCHIVED) ? 'A' : 'a';
            szTemp[1] = (fAttr & FILE_HIDDEN  ) ? 'H' : 'h';
            szTemp[2] = (fAttr & FILE_READONLY) ? 'R' : 'r';
            szTemp[3] = (fAttr & FILE_SYSTEM  ) ? 'S' : 's';
            szTemp[4] = '\0';
            xstrrpl(ppszText, 0, "\ta", szTemp, 0);
            ulrc++;
        }

        if (p = strstr(*ppszText, "\tEb")) // easize
        {
            ULONG ulEASize;
            ulEASize = _wpQueryEASize(pObject);
            strhThousandsDouble(szTemp, ulEASize, cThousands);
            xstrrpl(ppszText, 0, "\tEb", szTemp, 0);
            ulrc++;
        }

        if (p = strstr(*ppszText, "\tEk"))
        {
            ULONG ulEASize;
            ulEASize = _wpQueryEASize(pObject);
            strhThousandsDouble(szTemp, ((ulEASize+500)/1000), cThousands);
            xstrrpl(ppszText, 0, "\tEk", szTemp, 0);
            ulrc++;
        }

        if (p = strstr(*ppszText, "\tEK"))
        {
            ULONG ulEASize;
            ulEASize = _wpQueryEASize(pObject);
            strhThousandsDouble(szTemp, ((ulEASize+512)/1024), cThousands);
            xstrrpl(ppszText, 0, "\tEK", szTemp, 0);
            ulrc++;
        }

        if (p = strstr(*ppszText, "\tr")) // real name
        {
            strcpy(szTemp, "?");
            _wpQueryFilename(pObject, szTemp, FALSE);
            xstrrpl(ppszText, 0, "\tr", szTemp, 0);
            ulrc++;
        }
    }

    /*
     * WPDisk:
     *
     */

    else if (_somIsA(pObject, _WPDisk))
    {
        ULONG ulLogicalDrive = -1;

        /* single-object status bar text mnemonics understood by WPDisk:

             $F      file system type (HPFS, FAT, CDFS, ...)

             $fb     free space on drive in bytes
             $fk     free space on drive in kBytes
             $fK     free space on drive in KBytes
             $fm     free space on drive in mBytes
             $fM     free space on drive in MBytes

            NOTE: the $f keys are also handled by stbComposeText, but
                  those only work properly for file-system objects, so we need
                  to calculate these values for these (abstract) disk objects

         */

        // the following are for free space on drive
        if (p = strstr(*ppszText, "\tfb"))
        {
            double dbl;

            if (ulLogicalDrive == -1)
            {
                ulLogicalDrive = _wpQueryLogicalDrive(pObject);
                if (doshAssertDrive(ulLogicalDrive) != NO_ERROR)
                    ulLogicalDrive = 0;
            }

            dbl = doshQueryDiskFree(ulLogicalDrive);
            if (dbl == -1)
                strcpy(szTemp, "?");
            else
                strhThousandsDouble(szTemp, dbl,
                                    cThousands);
            xstrrpl(ppszText, 0, "\tfb", szTemp, 0);
            ulrc++;
        }

        if (p = strstr(*ppszText, "\tfk"))
        {
            double dbl;

            if (ulLogicalDrive == -1)
            {
                ulLogicalDrive = _wpQueryLogicalDrive(pObject);
                if (doshAssertDrive(ulLogicalDrive) != NO_ERROR)
                    ulLogicalDrive = 0;
            }

            dbl = doshQueryDiskFree(ulLogicalDrive);
            if (dbl == -1)
                strcpy(szTemp, "?");
            else
                strhThousandsDouble(szTemp,
                                    ((dbl + 500) / 1000),
                                    cThousands);
            xstrrpl(ppszText, 0, "\tfk", szTemp, 0);
            ulrc++;
        }

        if (p = strstr(*ppszText, "\tfK"))
        {
            double dbl;

            if (ulLogicalDrive == -1)
            {
                ulLogicalDrive = _wpQueryLogicalDrive(pObject);
                if (doshAssertDrive(ulLogicalDrive) != NO_ERROR)
                    ulLogicalDrive = 0;
            }

            dbl = doshQueryDiskFree(ulLogicalDrive);
            if (dbl == -1)
                strcpy(szTemp, "?");
            else
                strhThousandsDouble(szTemp,
                                    ((dbl + 512) / 1024),
                                    cThousands);
            xstrrpl(ppszText, 0, "\tfK", szTemp, 0);
            ulrc++;
        }

        if (p = strstr(*ppszText, "\tfm"))
        {
            double dbl;

            if (ulLogicalDrive == -1)
            {
                ulLogicalDrive = _wpQueryLogicalDrive(pObject);
                if (doshAssertDrive(ulLogicalDrive) != NO_ERROR)
                    ulLogicalDrive = 0;
            }

            dbl = doshQueryDiskFree(ulLogicalDrive);
            if (dbl == -1)
                strcpy(szTemp, "?");
            else
                strhThousandsDouble(szTemp,
                                    ((dbl +500000) / 1000000),
                                    cThousands);
            xstrrpl(ppszText, 0, "\tfm", szTemp, 0);
            ulrc++;
        }

        if (p = strstr(*ppszText, "\tfM"))
        {
            double dbl;

            if (ulLogicalDrive == -1)
            {
                ulLogicalDrive = _wpQueryLogicalDrive(pObject);
                if (doshAssertDrive(ulLogicalDrive) != NO_ERROR)
                    ulLogicalDrive = 0;
            }

            dbl = doshQueryDiskFree(ulLogicalDrive);
            if (dbl == -1)
                strcpy(szTemp, "?");
            else
               strhThousandsDouble(szTemp,
                                   ((dbl + (1024*1024/2)) / (1024*1024)),
                                   cThousands);
            xstrrpl(ppszText, 0, "\tfM", szTemp, 0);
            ulrc++;
        }

        if (p = strstr(*ppszText, "\tF"))  // file-system type (HPFS, ...)
        {
            CHAR szBuffer[200];

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
                xstrrpl(ppszText, 0, "\tF", szBuffer, 0);
            else
                xstrrpl(ppszText, 0, "\tF", "?", 0);
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

        if (p = strstr(*ppszText, "\tp"))  // program executable
        {
            strcpy(szTemp, "?");
            if (!pProgDetails)
                if ((_wpQueryProgDetails(pObject, (PPROGDETAILS)NULL, &ulSize)))
                    if ((pProgDetails = (PPROGDETAILS)_wpAllocMem(pObject, ulSize, NULL)) != NULL)
                        _wpQueryProgDetails(pObject, pProgDetails, &ulSize);

            if (pProgDetails)
                if (pProgDetails->pszExecutable)
                    strcpy(szTemp, pProgDetails->pszExecutable);
                else strcpy(szTemp, "");

            xstrrpl(ppszText, 0, "\tp", szTemp, 0);
            ulrc++;
        }

        if (p = strstr(*ppszText, "\tP"))  // program executable
        {
            strcpy(szTemp, "?");
            if (!pProgDetails)
                if ((_wpQueryProgDetails(pObject, (PPROGDETAILS)NULL, &ulSize)))
                    if ((pProgDetails = (PPROGDETAILS)_wpAllocMem(pObject, ulSize, NULL)) != NULL)
                        _wpQueryProgDetails(pObject, pProgDetails, &ulSize);

            if (pProgDetails)
                if (pProgDetails->pszParameters)
                    strcpy(szTemp, pProgDetails->pszParameters);
                else strcpy(szTemp, "");

            xstrrpl(ppszText, 0, "\tP", szTemp, 0);
            ulrc++;
        }

        if (p = strstr(*ppszText, "\td"))  // startup dir
        {
            strcpy(szTemp, "?");
            if (!pProgDetails)
                if ((_wpQueryProgDetails(pObject, (PPROGDETAILS)NULL, &ulSize)))
                    if ((pProgDetails = (PPROGDETAILS)_wpAllocMem(pObject, ulSize, NULL)) != NULL)
                        _wpQueryProgDetails(pObject, pProgDetails, &ulSize);

            if (pProgDetails)
                if (pProgDetails->pszStartupDir)
                    strcpy(szTemp, pProgDetails->pszStartupDir);
                else strcpy(szTemp, "");

            xstrrpl(ppszText, 0, "\td", szTemp, 0);
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

        if (p = strstr(*ppszText, "\tw"))     // class default title
        {
            SOMClass *pClassObject = _somGetClass(pObject);
            if (pClassObject)
                xstrrpl(ppszText, 0, "\tw", _wpclsQueryTitle(pClassObject), 0);
            else
                xstrrpl(ppszText, 0, "\tw", "?", 0);
            ulrc++;
        }

        if (p = strstr(*ppszText, "\tW"))     // class name
        {
            strcpy(szTemp, "?");
            xstrrpl(ppszText, 0, "\tW", _somGetClassName(pObject), 0);
            ulrc++;
        }

        if (p = strstr(*ppszText, "\tt"))          // object title
        {
            strcpy(szTemp, "?");
            strcpy(szTemp, _wpQueryTitle(pObject));
            strhBeautifyTitle(szTemp);
            xstrrpl(ppszText, 0, "\tt", szTemp, 0);
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

        $sb     size of selected objects in bytes
        $sk     size of selected objects in kBytes
        $sK     size of selected objects in KBytes
        $sm     size of selected objects in mBytes
        $sM     size of selected objects in MBytes

        $Sb     size of folder content in bytes
        $Sk     size of folder content in kBytes
        $SK     size of folder content in KBytes
        $Sm     size of folder content in mBytes
        $SM     size of folder content in MBytes

       The "single object" mode mnemonics are translated using
       the above functions afterwards.
    */

    PSZ         pszReturn = NULL;

    CNRINFO     CnrInfo;
    PMINIRECORDCORE pmrcSelected, pmrc2;
    ULONG       ulSelectedCount = 0,
                ulSizeSelected = 0,
                ulSizeTotal = 0;
    WPObject    *pObject = NULL,
                *pObject2 = NULL;
    PSZ         p;
    CHAR        *p2;

    // get thousands separator from "Country" object
    CHAR        cThousands = prfhQueryProfileChar(HINI_USER, "PM_National", "sThousand", ','),
                szTemp[300];

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
                   "Unable to query container records for status bar.",
                   _wpQueryTitle(somSelf));
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
                    if (_somIsA(pObject, _WPFileSystem))
                        ulSizeSelected += _wpQueryFileSize(pObject);
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
        pszReturn = strdup(cmnQueryStatusBarSetting(SBS_TEXTNONESEL));
    else if (ulSelectedCount == 1)
    {
        // "single-object" mode: query the text to translate
        // from the object, because we can implement
        // different mnemonics for different WPS classes

        // dereference shadows (V0.9.0)
        PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
        if (pGlobalSettings->fDereferenceShadows)
            if (_somIsA(pObject, _WPShadow))
                pObject = _wpQueryShadowedObject(pObject, TRUE);

        if (pObject == NULL)
            return(strdup(""));

        pszReturn = strdup(stbQueryClassMnemonics(_somGetClass(pObject)));
                                                  // object's class object
    }
    else
        // "multiple objects" mode
        pszReturn = strdup(cmnQueryStatusBarSetting(SBS_TEXTMULTISEL));

    // before actually translating any "$" keys, all the
    // '$' characters in the mnemonic string are changed to
    // the tabulator character ('\t') to avoid having '$'
    // characters translated which might only have been
    // inserted during the translation, e.g. because a
    // filename contains a '$' character.
    // All the translation logic will then only search for
    // those tab characters.
    while (p2 = strchr(pszReturn, '$'))
        *p2 = '\t';

    if (ulSelectedCount == 1)
    {
        // "single-object" mode: translate
        // object-specific mnemonics first
        stbTranslateSingleMnemonics(pObject,               // the object itself
                                    &pszReturn,
                                    cThousands);
        if (_somIsA(pObject, _WPFileSystem))
            // if we have a file-system object, we
            // need to re-query its size, because
            // we might have dereferenced a shadow
            // above (whose size was 0 -- V0.9.0)
            ulSizeSelected = _wpQueryFileSize(pObject);
    }

    // query total object count (CnrInfo.cRecords)
    WinSendMsg(hwndCnr,
               CM_QUERYCNRINFO,
               (MPARAM)(&CnrInfo),
               (MPARAM)(sizeof(CnrInfo)));

    // if we have a "total size" query, also sum up the size of
    // the whole folder into ulSizeTotal, which will be handled
    // in detail below
    if (strstr(pszReturn, "\tS"))
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
            _Pmpf(("  Result: %d", ulSizeTotal));
        #endif
    }

    if (p = strstr(pszReturn, "\tc")) // selected objs count
    {
        sprintf(szTemp, "%d", ulSelectedCount);
        xstrrpl(&pszReturn, 0, "\tc", szTemp, 0);
    }

    if (p = strstr(pszReturn, "\tC")) // total obj count
    {
        sprintf(szTemp, "%d", CnrInfo.cRecords);
        xstrrpl(&pszReturn, 0, "\tC", szTemp, 0);
    }

    // the following are for free space on drive

    if (p = strstr(pszReturn, "\tfb"))
    {
        strhThousandsDouble(szTemp,
                            wpshQueryDiskFreeFromFolder(somSelf),
                            cThousands);
        xstrrpl(&pszReturn, 0, "\tfb", szTemp, 0);
    }

    if (p = strstr(pszReturn, "\tfk"))
    {
        strhThousandsDouble(szTemp,
                            ((wpshQueryDiskFreeFromFolder(somSelf) + 500) / 1000),
                            cThousands);
        xstrrpl(&pszReturn, 0, "\tfk", szTemp, 0);
    }

    if (p = strstr(pszReturn, "\tfK"))
    {
        strhThousandsDouble(szTemp,
                            ((wpshQueryDiskFreeFromFolder(somSelf) + 512) / 1024),
                            cThousands);
        xstrrpl(&pszReturn, 0, "\tfK", szTemp, 0);
    }

    if (p = strstr(pszReturn, "\tfm"))
    {
        strhThousandsDouble(szTemp,
                            ((wpshQueryDiskFreeFromFolder(somSelf) +500000) / 1000000),
                            cThousands);
        xstrrpl(&pszReturn, 0, "\tfm", szTemp, 0);
    }

    if (p = strstr(pszReturn, "\tfM"))
    {
        strhThousandsDouble(szTemp,
                            ((wpshQueryDiskFreeFromFolder(somSelf) + (1024*1024/2)) / (1024*1024)),
                            cThousands);
        xstrrpl(&pszReturn, 0, "\tfM", szTemp, 0);
    }

    // the following are for SELECTED size
    if (p = strstr(pszReturn, "\tsb"))
    {
        strhThousandsULong(szTemp, ulSizeSelected, cThousands);
        xstrrpl(&pszReturn, 0, "\tsb", szTemp, 0);
    }

    if (p = strstr(pszReturn, "\tsk"))
    {
        strhThousandsULong(szTemp, ((ulSizeSelected+500) / 1000), cThousands);
        xstrrpl(&pszReturn, 0, "\tsk", szTemp, 0);
    }

    if (p = strstr(pszReturn, "\tsK"))
    {
        strhThousandsULong(szTemp, ((ulSizeSelected+512) / 1024), cThousands);
        xstrrpl(&pszReturn, 0, "\tsK", szTemp, 0);
    }

    if (p = strstr(pszReturn, "\tsm"))
    {
        strhThousandsULong(szTemp, ((ulSizeSelected+500000) / 1000000), cThousands);
        xstrrpl(&pszReturn, 0, "\tsm", szTemp, 0);
    }

    if (p = strstr(pszReturn, "\tsM"))
    {
        strhThousandsULong(szTemp, ((ulSizeSelected+(1024*1024/2)) / (1024*1024)), cThousands);
        xstrrpl(&pszReturn, 0, "\tsM", szTemp, 0);
    }

    // the following are for TOTAL folder size
    if (p = strstr(pszReturn, "\tSb"))
    {
        strhThousandsULong(szTemp, ulSizeTotal, cThousands);
        xstrrpl(&pszReturn, 0, "\tSb", szTemp, 0);
    }

    if (p = strstr(pszReturn, "\tSk"))
    {
        strhThousandsULong(szTemp, ((ulSizeTotal+500) / 1000), cThousands);
        xstrrpl(&pszReturn, 0, "\tSk", szTemp, 0);
    }

    if (p = strstr(pszReturn, "\tSK"))
    {
        strhThousandsULong(szTemp, ((ulSizeTotal+512) / 1024), cThousands);
        xstrrpl(&pszReturn, 0, "\tSK", szTemp, 0);
    }

    if (p = strstr(pszReturn, "\tSm"))
    {
        strhThousandsULong(szTemp, ((ulSizeTotal+500000) / 1000000), cThousands);
        xstrrpl(&pszReturn, 0, "\tSm", szTemp, 0);
    }

    if (p = strstr(pszReturn, "\tSM"))
    {
        strhThousandsULong(szTemp, ((ulSizeTotal+(1024*1024/2)) / (1024*1024)), cThousands);
        xstrrpl(&pszReturn, 0, "\tSM", szTemp, 0);
    }

    // now translate remaining '\t' characters back into
    // '$' characters; this might happen if the user actually
    // wanted to see a '$' character displayed
    while (p2 = strchr(pszReturn, '\t'))
        *p2 = '$';

    return (pszReturn);
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
        BOOL fEnable = !(pGlobalSettings->NoSubclassing);
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
    GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(5000);
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
        PrfQueryProfileString(HINI_USER, INIAPP_XWORKPLACE,
                              INIKEY_SB_LASTCLASS,
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
                              ID_XSDI_DEREFERENCESHADOWS,
                              pGlobalSettings->fDereferenceShadows);

        // multiple-objects mode
        WinSendDlgItemMsg(pcnbp->hwndDlgPage,
                          ID_XSDI_SBTEXTMULTISEL,
                          EM_SETTEXTLIMIT,
                          (MPARAM)(CCHMAXMNEMONICS-1),
                          MPNULL);
        WinSetDlgItemText(pcnbp->hwndDlgPage,
                          ID_XSDI_SBTEXTMULTISEL,
                          (PSZ)cmnQueryStatusBarSetting(SBS_TEXTMULTISEL));
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

        case ID_XSDI_DEREFERENCESHADOWS:    // added V0.9.0
        {
            GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(5000);
            pGlobalSettings->fDereferenceShadows = (BOOL)ulExtra;
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
                                      INIAPP_XWORKPLACE, INIKEY_SB_LASTCLASS,
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


