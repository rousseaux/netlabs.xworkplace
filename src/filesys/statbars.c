
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
 *      Copyright (C) 1997-2002 Ulrich M”ller.
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

#define INCL_DOSERRORS
#define INCL_WINSHELLDATA       // Prf* functions
#define INCL_WINPROGRAMLIST     // needed for PROGDETAILS, wppgm.h
#define INCL_WINMENUS
#define INCL_WINPOINTERS
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
#include "helpers\comctl.h"             // common controls (window procs)
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\nls.h"                // National Language Support helpers
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\standards.h"          // some standard macros
#include "helpers\stringh.h"            // string helper routines
#include "helpers\winh.h"               // PM helper routines
#include "helpers\xstring.h"            // extended string helpers

// SOM headers which don't crash with prec. header files
#include "xfobj.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\classes.h"             // WPS class list helper functions
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\helppanels.h"          // all XWorkplace help panel IDs
#include "shared\notebook.h"            // generic XWorkplace notebook handling

#include "filesys\object.h"             // XFldObject implementation
#include "filesys\program.h"            // program implementation; WARNING: this redefines macros
#include "filesys\statbars.h"           // status bar translation logic
#include "filesys\xthreads.h"           // extra XWorkplace threads

// other SOM headers
#pragma hdrstop                         // VAC++ keeps crashing otherwise
#include <wpfolder.h>                   // WPFolder
#include <wpdisk.h>                     // WPDisk
#include <wppgm.h>                      // WPProgram
// #include <wpshadow.h>                   // WPShadow
#include <wpshdir.h>                    // WPSharedDir // V0.9.5 (2000-09-20) [pr]

// finally, our own header file
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

/* ******************************************************************
 *                                                                  *
 *   Global variables                                               *
 *                                                                  *
 ********************************************************************/

static CHAR    G_szXFldObjectStatusBarMnemonics[CCHMAXMNEMONICS] = "";
static CHAR    G_szWPProgramStatusBarMnemonics[CCHMAXMNEMONICS] = "";
static CHAR    G_szWPDiskStatusBarMnemonics[CCHMAXMNEMONICS] = "";
static CHAR    G_szWPFileSystemStatusBarMnemonics[CCHMAXMNEMONICS] = "";
static CHAR    G_szWPUrlStatusBarMnemonics[CCHMAXMNEMONICS] = "";

// WPUrl class object; to preserve compatibility with Warp 3,
// where this class does not exist, we call the SOM kernel
// explicitly to get the class object.
// The initial value of -1 means that we have not queried
// this class yet. After the first query, this either points
// to the class object or is NULL if the class does not exist.
static SOMClass    *G_WPUrl = (SOMClass*)-1;

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
                     CHAR cThousands)
{
    if (space < 1000.0)
        sprintf(pszTarget,
                "%.0f%s",
                space,
                cmnGetString( ((ULONG)space == 1)
                                 ? ID_XSSI_BYTE
                                 : ID_XSSI_BYTES));  // pszBytes
    else
        if (space < 10000.0)
            nlsVariableDouble(pszTarget, space, cmnGetString(ID_XSSI_BYTES),  cThousands); // pszBytes
        else
        {
            space /= 1000;
            if (space < 10000.0)
                nlsVariableDouble(pszTarget, space, " kB", cThousands);
            else
            {
                space /= 1000;
                nlsVariableDouble(pszTarget, space,
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
                     CHAR cThousands)
{
    if (space < 1000.0)
        sprintf(pszTarget,
                "%.0f%s",
                space,
                cmnGetString(   (ULONG)(space == 1)
                                  ? ID_XSSI_BYTE
                                  : ID_XSSI_BYTES));  // pszBytes
    else
        if (space < 10240.0)
            nlsVariableDouble(pszTarget, space, cmnGetString(ID_XSSI_BYTES),  cThousands); // pszBytes
        else
        {
            space /= 1024;
            if (space < 10240.0)
                nlsVariableDouble(pszTarget, space, " KB", cThousands);
            else
            {
                space /= 1024;
                nlsVariableDouble(pszTarget, space,
                                   space < 10240.0 ? " MB" : " GB", cThousands);
            }
        }

    return(pszTarget);
}

/*
 *@@ ResolveWPUrl:
 *      if called for the first time, sets G_WPUrl
 *      to either the WPUrl class object or NULL
 *      if the class doesn't exist (Warp 3).
 *
 *@@added V0.9.14 (2001-07-31) [umoeller]
 *@@changed V0.9.16 (2001-10-28) [umoeller]: fixed SOM resource leak
 */

static VOID ResolveWPUrl(VOID)
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
        SOMFree(somidWPUrl);        // V0.9.16 (2001-10-28) [umoeller]
    }
}

#ifndef __NOCFGSTATUSBARS__

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
    ResolveWPUrl();

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

#endif

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

    ResolveWPUrl();     // V0.9.16 (2001-10-28) [umoeller]

    if (G_WPUrl)
        if (_somDescendedFrom(pClassObject, G_WPUrl))
        {
            if (G_szWPUrlStatusBarMnemonics[0] == '\0')
#ifndef __NOCFGSTATUSBARS__
                // load string if this is the first time
                if (PrfQueryProfileString(HINI_USERPROFILE,
                                          (PSZ)INIAPP_XWORKPLACE,
                                          (PSZ)INIKEY_SBTEXT_WPURL,
                                          NULL,
                                          &(G_szWPUrlStatusBarMnemonics),
                                          sizeof(G_szWPUrlStatusBarMnemonics))
                        == 0)
#endif
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
#ifndef __NOCFGSTATUSBARS__
            // load string if this is the first time
            if (PrfQueryProfileString(HINI_USERPROFILE,
                                      (PSZ)INIAPP_XWORKPLACE,
                                      (PSZ)INIKEY_SBTEXT_WPDISK,
                                      NULL,
                                      &(G_szWPDiskStatusBarMnemonics),
                                      sizeof(G_szWPDiskStatusBarMnemonics))
                    == 0)
#endif
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
#ifndef __NOCFGSTATUSBARS__
            // load string if this is the first time
            if (PrfQueryProfileString(HINI_USERPROFILE,
                        (PSZ)INIAPP_XWORKPLACE,
                        (PSZ)INIKEY_SBTEXT_WPFILESYSTEM,
                        NULL, &(G_szWPFileSystemStatusBarMnemonics),
                        sizeof(G_szWPFileSystemStatusBarMnemonics))
                    == 0)
                // string not found in profile: load default from NLS resources
#endif
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
#ifndef __NOCFGSTATUSBARS__
            // load string if this is the first time
            if (PrfQueryProfileString(HINI_USERPROFILE,
                                      (PSZ)INIAPP_XWORKPLACE,
                                      (PSZ)INIKEY_SBTEXT_WPPROGRAM,
                                      NULL,
                                      &(G_szWPProgramStatusBarMnemonics),
                                      sizeof(G_szWPProgramStatusBarMnemonics))
                    == 0)
#endif
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
#ifndef __NOCFGSTATUSBARS__
            // load string if this is the first time
            if (PrfQueryProfileString(HINI_USERPROFILE,
                                      (PSZ)INIAPP_XWORKPLACE,
                                      (PSZ)INIKEY_SBTEXT_WPOBJECT,
                                      NULL,
                                      &(G_szXFldObjectStatusBarMnemonics),
                                      sizeof(G_szXFldObjectStatusBarMnemonics))
                        == 0)
#endif
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
 *@@ GetDivisor:
 *      returns a divisor value for the given character,
 *      which should be the third character in a typical
 *      \tX mnemonic. This was added with V0.9.11 to get
 *      rid of the terrible spaghetti code from the
 *      XFolder days in the "translate mnemnonic" funcs.
 *
 *      Valid format characters will produce the following
 *      return values:
 *
 *      -- 'b': 1, display bytes
 *
 *      -- 'k': 1000, display kBytes
 *
 *      -- 'K': 1024, display KBytes
 *
 *      -- 'm': 1000*1000, display mBytes
 *
 *      -- 'M': 1024*1024, display MBytes
 *
 *      -- 'a': special, -1, display bytes/kBytes/mBytes/gBytes
 *
 *      -- 'A': special, -2 display bytes/KBytes/MBytes/GBytes
 *
 *      *pcReplace receives the no. of characters to
 *      replace in the mnemonic string, which will
 *      be 3 if "c" was a valid format character.
 *
 *      If this returns 0, the "c" character was invalid,
 *      and *pcReplace is set to 2. Do not divide by zero,
 *      of course.
 *
 *@@added V0.9.11 (2001-04-22) [umoeller]
 */

static ULONG GetDivisor(CHAR c,
                        PULONG pcReplace)      // out: chars to replace (2 or 3)
{
    *pcReplace = 3;

    switch (c)
    {
        case 'b':
            return 1;

        case 'k':
            return 1000;

        case 'K':
            return 1024;

        case 'm':
            return 1000*1000;

        case 'M':
            return 1024*1024;

        case 'a':
            return -1;          // special for stbVar1000Double

        case 'A':
            return -2;          // special for stbVar1024Double
    }

    // invalid code:
    *pcReplace = 2;

    return (0);
}

/*
 *@@ FormatDoubleValue:
 *      writes the formatted given "double" value
 *      to the given string buffer.
 *
 *      With ulDivisor, specify the value of GetDivisor().
 *
 *      pszBuf will ALWAYS contain something after
 *      this call.
 *
 *@@added V0.9.11 (2001-04-22) [umoeller]
 */

static VOID FormatDoubleValue(PSZ pszBuf,              // out: formatted string
                              ULONG ulDivisor,         // in: divisor from GetDivisor()
                              double dbl,              // in: value to format
                              PCOUNTRYSETTINGS pcs)    // in: country settings for formatting
{
    switch (ulDivisor)
    {
        case -1:
            stbVar1000Double(pszBuf,
                             dbl,
                             pcs->cThousands);
        break;

        case -2:
            stbVar1024Double(pszBuf,
                             dbl,
                             pcs->cThousands);
        break;

        case 1:     // no division needed, avoid the calc below
            nlsThousandsDouble(pszBuf,
                               dbl,
                               pcs->cThousands);
        break;

        case 0:     // GetDivisor() has detected a syntax error,
                    // avoid division by zero
            strcpy(pszBuf, "Syntax error.");
        break;

        default:        // "real" divisor specified:
        {
            double dValue = (dbl + (ulDivisor / 2)) / ulDivisor;
            nlsThousandsDouble(pszBuf,
                                dValue,
                                pcs->cThousands);
        }
        break;
    }
}

/*
 *@@ CheckLogicalDrive:
 *      returns TRUE if the specified drive is ready
 *      to avoid the stupid "drive not ready" popups.
 *
 *      This expects that *pulLogicalDrive is -1 if
 *      the drive has not been checked for pDisk.
 *      If so, *pulLogicalDrive is set to pDisk's
 *      drive number if the drive is ready, or 0
 *      otherwise.
 *
 *      Returns TRUE if *pulLogicalDrive is != 0.
 *
 *@@added V0.9.13 (2001-06-14) [umoeller]
 *@@changed V0.9.16 (2001-10-04) [umoeller]: stopped the disk A: clicking for remote disks
 */

static BOOL CheckLogicalDrive(PULONG pulLogicalDrive,
                              WPDisk *pDisk)
{
    if (*pulLogicalDrive == -1)
    {
        // make sure this disk is local;
        // _wpQueryLogicalDrive returns 1 for remote disks
        // and therefore used to provoke a click in drive A:
        // all the time... (how much stupidity can there be?!?)
        // V0.9.16 (2001-10-04) [umoeller]
        if (_somIsA(pDisk, _WPSharedDir))
            *pulLogicalDrive = 0;
        else
        {
            *pulLogicalDrive = _wpQueryLogicalDrive(pDisk);
            if (doshAssertDrive(*pulLogicalDrive, 0) != NO_ERROR)
                *pulLogicalDrive = 0;
        }

        _Pmpf((__FUNCTION__ ": *pulLogicalDrive = %d", *pulLogicalDrive));
    }

    return (*pulLogicalDrive != 0);
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
 *@@changed V0.9.9 (2001-03-19) [pr]: fixed drive space problems
 *@@changed V0.9.11 (2001-04-22) [umoeller]: fixed most of the spaghetti in here
 *@@changed V0.9.11 (2001-04-22) [umoeller]: replaced excessive string searches with xstrrpl
 *@@changed V0.9.11 (2001-04-22) [umoeller]: added $zX mnemonics for total disk size
 *@@changed V0.9.11 (2001-04-22) [umoeller]: added $L mnemonic for disk label
 *@@changed V0.9.13 (2001-06-14) [umoeller]: fixed missing SOMFree
 */

ULONG  stbTranslateSingleMnemonics(SOMClass *pObject,       // in: object
                                   PXSTRING pstrText,       // in/out: status bar text
                                   PCOUNTRYSETTINGS pcs)    // in: country settings
{
    ULONG       ulrc = 0;
    CHAR        szTemp[300];        // must be at least CCHMAXPATH!
    PSZ         p;
    ULONG       ulDivisor,
                cReplace;

    /*
     * WPUrl:
     *
     */

    // first check if the thing is a URL object;
    // in addition to the normal WPFileSystem mnemonics,
    // URL objects also support the $U mnemonic for
    // displaying the URL

    ResolveWPUrl();

    if (    (G_WPUrl)
         && (_somIsA(pObject, G_WPUrl))
       )
    {
        // yes, we have a URL object:
        if (p = strstr(pstrText->psz, "\tU")) // URL mnemonic
        {
            CHAR szFilename[CCHMAXPATH];
            PSZ pszFilename = _wpQueryFilename(pObject, szFilename, TRUE);

            // read in the contents of the file, which
            // contains the URL
            PSZ pszURL = NULL;
            // offset where we found this:
            ULONG   ulFoundOfs = (p - pstrText->psz);

            if (pszFilename)
            {
                doshLoadTextFile(pszFilename, &pszURL, NULL);
                if (pszURL)
                {
                    if (strlen(pszURL) > 100)
                        strcpy(pszURL+97, "...");
                    xstrrpl(pstrText,
                            ulFoundOfs,
                            2,              // chars to replace
                            pszURL,
                            strlen(pszURL));
                    free(pszURL);
                }
            }

            if (!pszURL)
                xstrrpl(pstrText,
                        ulFoundOfs,
                        2,              // chars to replace
                        "?",
                        1);
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
        ULONG   ulLogicalDrive = -1;

        /* single-object status bar text mnemonics understood by WPDisk:

             $F      file system type (HPFS, FAT, CDFS, ...)

             $L      drive label V0.9.11 (2001-04-22) [umoeller]

             $fb     free space on drive in bytes
             $fk     free space on drive in kBytes
             $fK     free space on drive in KBytes
             $fm     free space on drive in mBytes
             $fM     free space on drive in MBytes
             $fa     free space on drive in bytes/kBytes/mBytes/gBytes V0.9.6
             $fA     free space on drive in bytes/KBytes/MBytes/GBytes V0.9.6

             added the following: V0.9.11 (2001-04-22) [umoeller]
             $zb     total space on drive in bytes
             $zk     total space on drive in kBytes
             $zK     total space on drive in KBytes
             $zm     total space on drive in mBytes
             $zM     total space on drive in MBytes
             $za     total space on drive in bytes/kBytes/mBytes/gBytes
             $zA     total space on drive in bytes/KBytes/MBytes/GBytes

            NOTE: the $f keys are also handled by stbComposeText, but
                  those only work properly for file-system objects, so we need
                  to calculate these values for these (abstract) disk objects

         */

        // the following are for free space on drive

        // removed spaghetti here V0.9.11 (2001-04-22) [umoeller]
        p = pstrText->psz;
        while (p = strstr(p, "\tf"))
        {
            double  dbl;
            // offset where we found this:
            ULONG   ulFoundOfs = (p - pstrText->psz);
            // get divisor from third mnemonic char
            ulDivisor = GetDivisor(*(p + 2),
                                      // one of 'b', 'k', 'K', 'm', 'M', 'a', or 'A'
                                      // ulDivisor is 0 if none of these
                                   &cReplace);  // out: chars to replace

            // get the value and format it
            if (    (CheckLogicalDrive(&ulLogicalDrive, pObject))
                            // sets ulLogicalDrive to 0 if drive not ready
                 && (!doshQueryDiskFree(ulLogicalDrive, &dbl))
               )
            {
                // we got a value: format it
                FormatDoubleValue(szTemp,
                                  ulDivisor,
                                  dbl,                  // value
                                  pcs);                 // country settings
                ulrc++;
            }
            else
                strcpy(szTemp, "?");

            // now replace: since we have found the string
            // already, we can safely use xstrrpl directly
            xstrrpl(pstrText,
                    // ofs of first char to replace:
                    ulFoundOfs,
                    // chars to replace:
                    cReplace,
                    // replacement string:
                    szTemp,
                    strlen(szTemp));

            // now adjust search pointer; pstrText might have been
            // reallocated, so search on in new buffer from here
            p = pstrText->psz + ulFoundOfs;  // szTemp could have been anything

            ulrc++;
        }

        // the following are for TOTAL space on drive V0.9.11 (2001-04-22) [umoeller]
        p = pstrText->psz;
        while (p = strstr(p, "\tz"))
        {
            double  dbl;
            // offset where we found this:
            ULONG   ulFoundOfs = (p - pstrText->psz);
            // get divisor from third mnemonic char
            ulDivisor = GetDivisor(*(p + 2),
                                      // one of 'b', 'k', 'K', 'm', 'M', 'a', or 'A'
                                      // ulDivisor is 0 if none of these
                                   &cReplace);  // out: chars to replace

            // get the value and format it
            if (    (CheckLogicalDrive(&ulLogicalDrive, pObject))
                            // sets ulLogicalDrive to 0 if drive not ready
                 && (!doshQueryDiskSize(ulLogicalDrive, &dbl))
               )
            {
                // we got a value: format it
                FormatDoubleValue(szTemp,
                                  ulDivisor,
                                  dbl,                  // value
                                  pcs);                 // country settings
                ulrc++;
            }
            else
                strcpy(szTemp, "?");

            // now replace: since we have found the string
            // already, we can safely use xstrrpl directly
            xstrrpl(pstrText,
                    // ofs of first char to replace:
                    ulFoundOfs,
                    // chars to replace:
                    cReplace,
                    // replacement string:
                    szTemp,
                    strlen(szTemp));

            // now adjust search pointer; pstrText might have been
            // reallocated, so search on in new buffer from here
            p = pstrText->psz + ulFoundOfs;  // szTemp could have been anything

            ulrc++;
        }
        // end total space V0.9.11 (2001-04-22) [umoeller]

        if (p = strstr(pstrText->psz, "\tF"))  // file-system type (HPFS, ...)
        {
            if (    (!CheckLogicalDrive(&ulLogicalDrive, pObject))
                            // sets ulLogicalDrive to 0 if drive not ready
                 || (doshQueryDiskFSType(ulLogicalDrive,
                                        szTemp,
                                        sizeof(szTemp)))
               )
                strcpy(szTemp, "?");

            xstrrpl(pstrText,
                    // ofs of first char to replace:
                    (p - pstrText->psz),
                    // chars to replace:
                    2,
                    // replacement string:
                    szTemp,
                    strlen(szTemp));

            ulrc++;
        }

        // drive label V0.9.11 (2001-04-22) [umoeller]
        if (p = strstr(pstrText->psz, "\tL"))
        {
            if (    (!CheckLogicalDrive(&ulLogicalDrive, pObject))
                            // sets ulLogicalDrive to 0 if drive not ready
                 || (doshQueryDiskLabel(ulLogicalDrive,
                                        szTemp))
               )
                strcpy(szTemp, "?");

            xstrrpl(pstrText,
                    // ofs of first char to replace:
                    (p - pstrText->psz),
                    // chars to replace:
                    2,
                    // replacement string:
                    szTemp,
                    strlen(szTemp));

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

        if (p = strstr(pstrText->psz, "\ty")) // type
        {
            // offset where we found this:
            PSZ p2 = _wpQueryType(pObject);
            if (!p2)
                p2 = "?";
            xstrrpl(pstrText,
                    // ofs of first char to replace:
                    (p - pstrText->psz),
                    // chars to replace:
                    2,
                    // replacement string:
                    p2,
                    strlen(p2));
            ulrc++;
        }

        if (p = strstr(pstrText->psz, "\tD"))  // date
        {
            strcpy(szTemp, "?");
            _wpQueryDateInfo(pObject, &ffbuf4);
            fBufLoaded = TRUE;
            nlsFileDate(szTemp,
                         &(ffbuf4.fdateLastWrite),
                         pcs->ulDateFormat,
                         pcs->cDateSep);
            xstrrpl(pstrText,
                    // ofs of first char to replace:
                    (p - pstrText->psz),
                    // chars to replace:
                    2,
                    // replacement string:
                    szTemp,
                    strlen(szTemp));
            ulrc++;
        }

        if (p = strstr(pstrText->psz, "\tT"))  // time
        {
            strcpy(szTemp, "?");
            if (!fBufLoaded)
                _wpQueryDateInfo(pObject, &ffbuf4);
            nlsFileTime(szTemp,
                         &(ffbuf4.ftimeLastWrite),
                         pcs->ulTimeFormat,
                         pcs->cTimeSep);
            xstrrpl(pstrText,
                    // ofs of first char to replace:
                    (p - pstrText->psz),
                    // chars to replace:
                    2,
                    // replacement string:
                    szTemp,
                    strlen(szTemp));
            ulrc++;
        }

        if (p = strstr(pstrText->psz, "\ta")) // attribs
        {
            ULONG fAttr = _wpQueryAttr(pObject);
            szTemp[0] = (fAttr & FILE_ARCHIVED) ? 'A' : 'a';
            szTemp[1] = (fAttr & FILE_HIDDEN  ) ? 'H' : 'h';
            szTemp[2] = (fAttr & FILE_READONLY) ? 'R' : 'r';
            szTemp[3] = (fAttr & FILE_SYSTEM  ) ? 'S' : 's';
            szTemp[4] = '\0';
            xstrrpl(pstrText,
                    // ofs of first char to replace:
                    (p - pstrText->psz),
                    // chars to replace:
                    2,
                    // replacement string:
                    szTemp,
                    strlen(szTemp));
            ulrc++;
        }

        // removed spaghetti here V0.9.11 (2001-04-22) [umoeller]
        // note: this makes the $E codes support megabyte format
        // chars as well, but this would be silly to specify...
        p = pstrText->psz;
        while (p = strstr(p, "\tE")) // easize
        {
            double dEASize = (double)_wpQueryEASize(pObject);
            // offset where we found this:
            ULONG   ulFoundOfs = (p - pstrText->psz);

            // get divisor from third mnemonic char
            ulDivisor = GetDivisor(*(p + 2),
                                      // one of 'b', 'k', 'K', 'm', 'M', 'a', or 'A'
                                      // ulDivisor is 0 if none of these
                                   &cReplace);  // out: chars to replace

            FormatDoubleValue(szTemp,
                              ulDivisor,
                              dEASize,
                              pcs);

            // now replace: since we have found the string
            // already, we can safely use xstrrpl directly
            xstrrpl(pstrText,
                    // ofs of first char to replace:
                    ulFoundOfs,
                    // chars to replace:
                    cReplace,
                    // replacement string:
                    szTemp,
                    strlen(szTemp));

            // now adjust search pointer; pstrText might have been
            // reallocated, so search on in new buffer from here
            p = pstrText->psz + ulFoundOfs;  // szTemp could have been anything

            ulrc++;
        }

        if (p = strstr(pstrText->psz, "\tr")) // real name
        {
            strcpy(szTemp, "?");
            _wpQueryFilename(pObject, szTemp, FALSE);
            xstrrpl(pstrText,
                    // ofs of first char to replace:
                    (p - pstrText->psz),
                    // chars to replace:
                    2,
                    // replacement string:
                    szTemp,
                    strlen(szTemp));
            ulrc++;
        }
    }

    /*
     * WPProgram:
     *
     */

    else if (_somIsA(pObject, _WPProgram))
    {
        PPROGDETAILS pDetails = NULL;
        ULONG       ulSize;

        /* single-object status bar text mnemonics understood by WPProgram
           (in addition to those introduced by XFldObject):

            $p      executable program file (as specified in the Settings)
            $P      parameter list (as specified in the Settings)
            $d      working directory (as specified in the Settings)
         */

        if (p = strstr(pstrText->psz, "\tp"))  // program executable
        {
            strcpy(szTemp, "?");
            if (!pDetails)
                pDetails = progQueryDetails(pObject);

            if (pDetails)
                if (pDetails->pszExecutable)
                    strcpy(szTemp, pDetails->pszExecutable);
                else strcpy(szTemp, "");

            xstrrpl(pstrText,
                    // ofs of first char to replace:
                    (p - pstrText->psz),
                    // chars to replace:
                    2,
                    // replacement string:
                    szTemp,
                    strlen(szTemp));
            ulrc++;
        }

        if (p = strstr(pstrText->psz, "\tP"))  // program parameters
        {
            strcpy(szTemp, "?");
            if (!pDetails)
                pDetails = progQueryDetails(pObject);

            if (pDetails)
                if (pDetails->pszParameters)
                    strcpy(szTemp, pDetails->pszParameters);
                else
                    strcpy(szTemp, "");

            xstrrpl(pstrText,
                    // ofs of first char to replace:
                    (p - pstrText->psz),
                    // chars to replace:
                    2,
                    // replacement string:
                    szTemp,
                    strlen(szTemp));
            ulrc++;
        }

        if (p = strstr(pstrText->psz, "\td"))  // startup dir
        {
            strcpy(szTemp, "?");
            if (!pDetails)
                pDetails = progQueryDetails(pObject);

            if (pDetails)
                if (pDetails->pszStartupDir)
                    strcpy(szTemp, pDetails->pszStartupDir);
                else
                    strcpy(szTemp, "");

            xstrrpl(pstrText,
                    // ofs of first char to replace:
                    (p - pstrText->psz),
                    // chars to replace:
                    2,
                    // replacement string:
                    szTemp,
                    strlen(szTemp));
            ulrc++;
        }

        if (pDetails)
            free(pDetails);
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
            SOMClass    *pClassObject = _somGetClass(pObject);
            PSZ         pszValue = NULL;

            if (pClassObject)
                pszValue = _wpclsQueryTitle(pClassObject);
            if (!pszValue)
                pszValue = "?";

            xstrrpl(pstrText,
                    // ofs of first char to replace:
                    (p - pstrText->psz),
                    // chars to replace:
                    2,
                    // replacement string:
                    pszValue,
                    strlen(pszValue));
            ulrc++;
        }

        if (p = strstr(pstrText->psz, "\tW"))     // class name
        {
            PSZ pszValue = _somGetClassName(pObject);
            if (!pszValue)
                pszValue = "?";
            xstrrpl(pstrText,
                    // ofs of first char to replace:
                    (p - pstrText->psz),
                    // chars to replace:
                    2,
                    // replacement string:
                    pszValue,
                    strlen(pszValue));
            ulrc++;
        }

        if (p = strstr(pstrText->psz, "\tt"))          // object title
        {
            strcpy(szTemp, "?");
            strcpy(szTemp, _wpQueryTitle(pObject));
            strhBeautifyTitle(szTemp);
            xstrrpl(pstrText,
                    // ofs of first char to replace:
                    (p - pstrText->psz),
                    // chars to replace:
                    2,
                    // replacement string:
                    szTemp,
                    strlen(szTemp));
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
 *@@changed V0.9.11 (2001-04-22) [umoeller]: fixed most of the spaghetti in here
 *@@changed V0.9.11 (2001-04-22) [umoeller]: replaced excessive string searches with xstrrpl
 *@@changed V0.9.11 (2001-04-22) [umoeller]: merged three cnr record loops into one (speed)
 *@@changed V0.9.11 (2001-04-22) [umoeller]: added $zX mnemonics for total disk size
 *@@changed V0.9.11 (2001-04-25) [umoeller]: fixed tree view bug introduced on 2001-04-22
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

        $zb     total size of drive in bytes V0.9.11 (2001-04-22) [umoeller]
        $zk     total size of drive in kBytes
        $zK     total size of drive in KBytes
        $zm     total size of drive in mBytes
        $zM     total size of drive in MBytes
        $za     total size of drive in bytes/kBytes/mBytes/gBytes
        $zA     total size of drive in bytes/KBytes/MBytes/GBytes

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

    ULONG       cVisibleRecords = 0,
                cSelectedRecords = 0,
                cbSelected = 0,
                cbTotal = 0,
                ulDivisor,
                cReplace;
    USHORT      cmd;
    WPObject    *pobj = NULL,
                *pobjSelected = NULL;
    PSZ         p;
    CHAR        *p2;
    // PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    PMINIRECORDCORE prec;

    // get country settings from "Country" object
    PCOUNTRYSETTINGS pcs = cmnQueryCountrySettings(FALSE);
    CHAR        // cThousands = pcs->cThousands,
                szTemp[300];

    // pre-resolve class pointers for speed V0.9.11 (2001-04-22) [umoeller]
    /* SOMClass    *pclsWPShadow = _WPShadow,
                *pclsWPFileSystem = _WPFileSystem; */

    xstrInit(&strText, 300);

    // V0.9.11 (2001-04-22) [umoeller]: now, we used to have
    // THREE loops here which ran through the objects in the
    // container, producing a WinSendMsg for each record...
    // since that code ALWAYS ran through all objects at least
    // once (Paul's fix for the invisible object count), I
    // merged the three loops into one now. This can make a
    // noticeable difference when 1000 objects have been
    // selected in the container and the user selects a large
    // number of objects.

    // run-through-all-objects loop...
    for (prec = NULL, cmd = CMA_FIRST;
         ;
         cmd = CMA_NEXT)
    {
        prec = (PMINIRECORDCORE)WinSendMsg(hwndCnr,
                                           CM_QUERYRECORD,
                                           (MPARAM)prec,
                                           MPFROM2SHORT(cmd,     // CMA_FIRST or CMA_NEXT
                                                        CMA_ITEMORDER));
        if (    (prec == NULL)
             || ((ULONG)prec == -1)
           )
            // error, or last:
            break;

        // count all visible objects
        if (!(prec->flRecordAttr & CRA_FILTERED))
        {
            BOOL    fThisSelected = ((prec->flRecordAttr & CRA_SELECTED) != 0);

            // count all visible records
            cVisibleRecords++;

            // now get the object from the MINIRECORDCORE
            if (    (pobj = OBJECT_FROM_PREC(prec))
                 && (wpshCheckObject(pobj))
               )
            {
                WPObject    *pDeref = pobj;

                if (fThisSelected)
                {
                    // count all selected records
                    cSelectedRecords++;
                    pobjSelected = pobj;        // NOT dereferenced if shadow!
                }

#ifndef __NOCFGSTATUSBARS__
                if (cmnQuerySetting(sflDereferenceShadows) & STBF_DEREFSHADOWS_MULTIPLE)
                {
                    // deref multiple shadows
                    pDeref = objResolveIfShadow(pDeref);
                }
#endif

                if (pDeref && _somIsA(pDeref, _WPFileSystem))
                {
                    // OK, we got a WPFileSystem:
                    ULONG cbThis = _wpQueryFileSize(pDeref);

                    if (fThisSelected)
                        // count bytes of selected objects
                        cbSelected += cbThis;

                    cbTotal += cbThis;
                }
            }
            else
                pobj = NULL;

        } // end if (!(prec->flRecordAttr & CRA_FILTERED))
    } // end for (prec = NULL, cmd = CMA_FIRST; ...

    // pobjSelected is now NULL for single-object mode
    // or contains the last selected object we had in the loop;
    // in one-object mode, it'll be the only object
    // and might still point to a shadow.

    // However, in tree view, there might be a selected record
    // which was not found in the above loop because the loop
    // didn't go thru child records... so do an additional check:
    // V0.9.11 (2001-04-25) [umoeller]
    if (!pobjSelected)
    {
        // send an extra "find selected record" msg; this is
        // sufficient for all cases, since
        // a)  if we're in a tree view, only one record can
        //     be selected in the first place;
        // b)  if we're not in a tree view and pobjSelected
        //     is NULL, we won't find any records with this
        //     message either, because there can't be any
        //     selected records which weren't already found
        //     in the above loop.
        // As a result, we can save ourselves from querying
        // whether the cnr is in tree view in the first place.

        prec = WinSendMsg(hwndCnr,
                          CM_QUERYRECORDEMPHASIS,
                          (MPARAM)CMA_FIRST,
                          (MPARAM)CRA_SELECTED);
        if (    (prec)
             && ((ULONG)prec != -1)
           )
        {
            // if we got a selected record HERE, we MUST
            // be in tree view, so there can be only one
            pobjSelected = OBJECT_FROM_PREC(prec);
            cSelectedRecords = 1;
        }
    }

    // now get the mnemonics which have been set by the
    // user on the "Status bar" page, depending on how many
    // objects are selected
    if ( (cSelectedRecords == 0) || (pobjSelected == NULL) )
        // "no object" mode:
        xstrcpy(&strText, cmnQueryStatusBarSetting(SBS_TEXTNONESEL), 0);
    else if (cSelectedRecords == 1)
    {
        // "single-object" mode: query the text to translate
        // from the object, because we can implement
        // different mnemonics for different WPS classes

        // dereference shadows (V0.9.0)
#ifndef __NOCFGSTATUSBARS__
        if (cmnQuerySetting(sflDereferenceShadows) & STBF_DEREFSHADOWS_SINGLE)
#endif
            pobjSelected = objResolveIfShadow(pobjSelected);

        if (pobjSelected == NULL)
            return(strdup(""));

        xstrcpy(&strText, stbQueryClassMnemonics(_somGetClass(pobjSelected)), 0);
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
    // V0.9.11 (2001-04-22) [umoeller]: sped it up a bit
    /* while (p2 = strchr(strText.psz, '$'))
        *p2 = '\t'; */
    p2 = strText.psz;
    while (p2 = strchr(p2, '$'))
        *p2++ = '\t';

    if (cSelectedRecords == 1)
    {
        // "single-object" mode: translate
        // object-specific mnemonics first
        stbTranslateSingleMnemonics(pobjSelected,
                                    &strText,
                                    pcs);
        if (_somIsA(pobjSelected, _WPFileSystem))
            // if we have a file-system object, we
            // need to re-query its size, because
            // we might have dereferenced a shadow
            // above (whose size was 0 -- V0.9.0)
            cbSelected = _wpQueryFileSize(pobjSelected);
    }

    // NOW GO TRANSLATE COMMON CODES

    if (p = strstr(strText.psz, "\tc")) // selected objs count
    {
        sprintf(szTemp, "%u", cSelectedRecords);
        xstrrpl(&strText,
                // ofs of first char to replace:
                (p - strText.psz),
                // chars to replace:
                2,
                // replacement string:
                szTemp,
                strlen(szTemp));
    }

    if (p = strstr(strText.psz, "\tC")) // total obj count
    {
        sprintf(szTemp, "%u", cVisibleRecords);
        xstrrpl(&strText,
                // ofs of first char to replace:
                (p - strText.psz),
                // chars to replace:
                2,
                // replacement string:
                szTemp,
                strlen(szTemp));
    }

    // the following are for free space on drive

    // removed spaghetti here V0.9.11 (2001-04-22) [umoeller]
    p = strText.psz;
    while (p = strstr(p, "\tf"))
    {
        // offset where we found this:
        ULONG   ulFoundOfs = (p - strText.psz);
        // get divisor from third mnemonic char
        ulDivisor = GetDivisor(*(p + 2),
                                  // one of 'b', 'k', 'K', 'm', 'M', 'a', or 'A'
                                  // ulDivisor is 0 if none of these
                               &cReplace);  // out: chars to replace

        // get the value and format it
        FormatDoubleValue(szTemp,
                          ulDivisor,
                          wpshQueryDiskFreeFromFolder(somSelf),
                          pcs);                 // country settings

        // now replace: since we have found the string
        // already, we can safely use xstrrpl directly
        xstrrpl(&strText,
                // ofs of first char to replace:
                ulFoundOfs,
                // chars to replace:
                cReplace,
                // replacement string:
                szTemp,
                strlen(szTemp));

        // now adjust search pointer; pstrText might have been
        // reallocated, so search on in new buffer from here
        p = strText.psz + ulFoundOfs;  // szTemp could have been anything
    }

    // total disk size, added V0.9.11 (2001-04-22) [umoeller]
    p = strText.psz;
    while (p = strstr(p, "\tz"))
    {
        // offset where we found this:
        ULONG   ulFoundOfs = (p - strText.psz);
        // get divisor from third mnemonic char
        ulDivisor = GetDivisor(*(p + 2),
                                  // one of 'b', 'k', 'K', 'm', 'M', 'a', or 'A'
                                  // ulDivisor is 0 if none of these
                               &cReplace);  // out: chars to replace

        // get the value and format it
        FormatDoubleValue(szTemp,
                          ulDivisor,
                          wpshQueryDiskSizeFromFolder(somSelf),
                          pcs);                 // country settings

        // now replace: since we have found the string
        // already, we can safely use xstrrpl directly
        xstrrpl(&strText,
                // ofs of first char to replace:
                ulFoundOfs,
                // chars to replace:
                cReplace,
                // replacement string:
                szTemp,
                strlen(szTemp));

        // now adjust search pointer; pstrText might have been
        // reallocated, so search on in new buffer from here
        p = strText.psz + ulFoundOfs;  // szTemp could have been anything
    }

    // the following are for SELECTED size

    // removed spaghetti here V0.9.11 (2001-04-22) [umoeller]
    p = strText.psz;
    while (p = strstr(p, "\ts"))
    {
        // offset where we found this:
        ULONG   ulFoundOfs = (p - strText.psz);
        // get divisor from third mnemonic char
        ulDivisor = GetDivisor(*(p + 2),
                                  // one of 'b', 'k', 'K', 'm', 'M', 'a', or 'A'
                                  // ulDivisor is 0 if none of these
                               &cReplace);  // out: chars to replace

        // get the value and format it
        FormatDoubleValue(szTemp,
                          ulDivisor,
                          (double)cbSelected,
                          pcs);                 // country settings

        // now replace: since we have found the string
        // already, we can safely use xstrrpl directly
        xstrrpl(&strText,
                // ofs of first char to replace:
                ulFoundOfs,
                // chars to replace:
                cReplace,
                // replacement string:
                szTemp,
                strlen(szTemp));

        // now adjust search pointer; pstrText might have been
        // reallocated, so search on in new buffer from here
        p = strText.psz + ulFoundOfs;  // szTemp could have been anything
    }

    // the following are for TOTAL folder size

    // removed spaghetti here V0.9.11 (2001-04-22) [umoeller]
    p = strText.psz;
    while (p = strstr(p, "\tS"))
    {
        // offset where we found this:
        ULONG   ulFoundOfs = (p - strText.psz);
        // get divisor from third mnemonic char
        ulDivisor = GetDivisor(*(p + 2),
                                  // one of 'b', 'k', 'K', 'm', 'M', 'a', or 'A'
                                  // ulDivisor is 0 if none of these
                               &cReplace);  // out: chars to replace

        // get the value and format it
        FormatDoubleValue(szTemp,
                          ulDivisor,
                          (double)cbTotal,
                          pcs);                 // country settings

        // now replace: since we have found the string
        // already, we can safely use xstrrpl directly
        xstrrpl(&strText,
                // ofs of first char to replace:
                ulFoundOfs,
                // chars to replace:
                cReplace,
                // replacement string:
                szTemp,
                strlen(szTemp));

        // now adjust search pointer; pstrText might have been
        // reallocated, so search on in new buffer from here
        p = strText.psz + ulFoundOfs;  // szTemp could have been anything
    }

    // dump icon position in debug mode
    #ifdef DEBUG_STATUSBARS
    if (    (cSelectedRecords == 1)
         && (pobjSelected)
         && (prec = _wpQueryCoreRecord(pobjSelected))
       )
    {
        XSTRING strNew;
        CHAR sz[100];
        sprintf(sz, "(%d, %d) ", prec->ptlIcon.x, prec->ptlIcon.y);
        xstrInitCopy(&strNew,
                     sz,
                     0);
        xstrcats(&strNew, &strText);
        xstrcpys(&strText, &strNew);
        xstrClear(&strNew);
    }
    #endif

    if (strText.ulLength)
    {
        // alright, we have something:

        // now translate remaining '\t' characters back into
        // '$' characters; this might happen if the user actually
        // wanted to see a '$' character displayed for his bank account
        while (p2 = strchr(strText.psz, '\t'))
            *p2 = '$';

        return (strText.psz);       // do not free the string!
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

#ifndef __NOCFGSTATUSBARS__

/*
 *@@ STATUSBARPAGEDATA:
 *      data for status bar pages while they're open.
 *
 *      Finally turned this into a heap structure
 *      with V0.9.14 to get rid of the global variables.
 *
 *@@added V0.9.14 (2001-07-31) [umoeller]
 */

typedef struct _STATUSBARPAGEDATA
{
    CHAR                    szSBTextNoneSelBackup[CCHMAXMNEMONICS],
                            szSBText1SelBackup[CCHMAXMNEMONICS],
                            szSBTextMultiSelBackup[CCHMAXMNEMONICS],
                            szSBClassSelected[256];
    SOMClass                *pSBClassObjectSelected;
    // somId                   somidClassSelected;

    HWND                    hwndKeysMenu;
                // if != NULLHANDLE, the menu for the last
                // "Keys" button pressed
    ULONG                   ulLastKeysID;

} STATUSBARPAGEDATA, *PSTATUSBARPAGEDATA;

// struct passed to callbacks
typedef struct _STATUSBARSELECTCLASS
{
    HWND            hwndOKButton;
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

static MRESULT EXPENTRY fncbWPSStatusBarReturnClassAttr(HWND hwndCnr,
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
                if (!strcmp(pwps->pszClassName, pscd->szClassSelected))
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

static MRESULT EXPENTRY fncbWPSStatusBarClassSelected(HWND hwndCnr,
                                                      ULONG ulpsbsc,
                                                      MPARAM mpwps,
                                                      MPARAM mphwndInfo)
{
    PWPSLISTITEM pwps = (PWPSLISTITEM)mpwps;
    // PSTATUSBARSELECTCLASS psbsc = (PSTATUSBARSELECTCLASS)ulpsbsc;
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
        sprintf(szInfo,
                "%s\n%s",
                cmnGetString(ID_XSSI_SB_CLASSMNEMONICS),
                stbQueryClassMnemonics(pwps->pClassObject));
        mrc = (MPARAM)TRUE;
    }
    else
        strcpy(szInfo,
               cmnGetString(ID_XSSI_SB_CLASSNOTSUPPORTED));  // pszSBClassNotSupported

    WinSetWindowText((HWND)mphwndInfo, szInfo);

    return (mrc);
}

#endif

static const XWPSETTING G_StatusBar1Backup[] =
    {
        sfDefaultStatusBarVisibility,
        sflSBForViews,
        sulSBStyle
    };

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

VOID stbStatusBar1InitPage(PNOTEBOOKPAGE pnbp,   // notebook info struct
                           ULONG flFlags)        // CBI_* flags (notebook.h)
{
    // PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    if (flFlags & CBI_INIT)
    {
        if (pnbp->pUser == NULL)
        {
            // first call: backup Global Settings for "Undo" button;
            // this memory will be freed automatically by the
            // common notebook window function (notebook.c) when
            // the notebook page is destroyed
            /*
            pnbp->pUser = malloc(sizeof(GLOBALSETTINGS));
            memcpy(pnbp->pUser, pGlobalSettings, sizeof(GLOBALSETTINGS));
            */
            pnbp->pUser = cmnBackupSettings(G_StatusBar1Backup,
                                             ARRAYITEMCOUNT(G_StatusBar1Backup));
        }
    }

    if (flFlags & CBI_SET)
    {
        ULONG fl;
        winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_XSDI_ENABLESTATUSBAR,
                              cmnQuerySetting(sfDefaultStatusBarVisibility));

        fl = cmnQuerySetting(sflSBForViews);
        winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_XSDI_SBFORICONVIEWS,
                              (fl & SBV_ICON) != 0);
        winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_XSDI_SBFORTREEVIEWS,
                              (fl & SBV_TREE) != 0);
        winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_XSDI_SBFORDETAILSVIEWS,
                              (fl & SBV_DETAILS) != 0);

        fl = cmnQuerySetting(sulSBStyle);
        switch (fl)
        {
            case SBSTYLE_WARP3RAISED:
                winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_XSDI_SBSTYLE_3RAISED, TRUE);
            break;

            case SBSTYLE_WARP3SUNKEN:
                winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_XSDI_SBSTYLE_3SUNKEN, TRUE);
            break;

            case SBSTYLE_WARP4RECT:
                winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_XSDI_SBSTYLE_4RECT, TRUE);
            break;

            default:
                winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_XSDI_SBSTYLE_4MENU, TRUE);
        }
    }

    if (flFlags & CBI_ENABLE)
    {
#ifndef __ALWAYSSUBCLASS__
        BOOL fEnable = !cmnQuerySetting(sfNoSubclassing);
        winhEnableDlgItem(pnbp->hwndDlgPage, ID_XSDI_ENABLESTATUSBAR, fEnable);
        winhEnableDlgItem(pnbp->hwndDlgPage, ID_XSDI_SBSTYLE_3RAISED, fEnable);
        winhEnableDlgItem(pnbp->hwndDlgPage, ID_XSDI_SBSTYLE_3SUNKEN, fEnable);
        winhEnableDlgItem(pnbp->hwndDlgPage, ID_XSDI_SBSTYLE_4MENU,   fEnable);
        winhEnableDlgItem(pnbp->hwndDlgPage, ID_XSDI_SBSTYLE_4RECT,   fEnable);

        winhEnableDlgItem(pnbp->hwndDlgPage, ID_XSDI_SBFORICONVIEWS,   fEnable);
        winhEnableDlgItem(pnbp->hwndDlgPage, ID_XSDI_SBFORTREEVIEWS,   fEnable);
        winhEnableDlgItem(pnbp->hwndDlgPage, ID_XSDI_SBFORDETAILSVIEWS,   fEnable);
#endif
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

MRESULT stbStatusBar1ItemChanged(PNOTEBOOKPAGE pnbp,
                                 ULONG ulItemID,
                                 USHORT usNotifyCode,
                                 ULONG ulExtra)      // for checkboxes: contains new state
{
    // GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(__FILE__, __LINE__, __FUNCTION__);
    MRESULT mrc = (MRESULT)0;
    BOOL    fSave = TRUE,
            fShowStatusBars = FALSE,
            fRefreshStatusBars = FALSE;

    ULONG   flStyleChanged = 0;

    switch (ulItemID)
    {
        case ID_XSDI_ENABLESTATUSBAR:
            cmnSetSetting(sfDefaultStatusBarVisibility, ulExtra);
            fShowStatusBars = TRUE;
        break;

        case ID_XSDI_SBFORICONVIEWS:
            flStyleChanged = SBV_ICON;
            fShowStatusBars = TRUE;
        break;

        case ID_XSDI_SBFORTREEVIEWS:
            flStyleChanged = SBV_TREE;
            fShowStatusBars = TRUE;
        break;

        case ID_XSDI_SBFORDETAILSVIEWS:
            flStyleChanged = SBV_DETAILS;
            fShowStatusBars = TRUE;
        break;

        case ID_XSDI_SBSTYLE_3RAISED:
            cmnSetSetting(sulSBStyle, SBSTYLE_WARP3RAISED);
            fRefreshStatusBars = TRUE;
        break;

        case ID_XSDI_SBSTYLE_3SUNKEN:
            cmnSetSetting(sulSBStyle, SBSTYLE_WARP3SUNKEN);
            fRefreshStatusBars = TRUE;
        break;

        case ID_XSDI_SBSTYLE_4RECT:
            cmnSetSetting(sulSBStyle, SBSTYLE_WARP4RECT);
            fRefreshStatusBars = TRUE;
        break;

        case ID_XSDI_SBSTYLE_4MENU:
            cmnSetSetting(sulSBStyle, SBSTYLE_WARP4MENU);
            fRefreshStatusBars = TRUE;
        break;

        case DID_UNDO:
        {
            // "Undo" button: get pointer to backed-up Global Settings
            /*
            PCGLOBALSETTINGS pGSBackup = (PCGLOBALSETTINGS)(pnbp->pUser);

            // and restore the settings for this page
            cmnSetSetting(sfDefaultStatusBarVisibility, pGSBackup->fDefaultStatusBarVisibility);
            cmnSetSetting(sflSBForViews, pGSBackup->SBForViews);
            cmnSetSetting(sulSBStyle, pGSBackup->SBStyle);
               */

            cmnRestoreSettings(pnbp->pUser,
                               ARRAYITEMCOUNT(G_StatusBar1Backup));
            // update the display by calling the INIT callback
            pnbp->inbp.pfncbInitPage(pnbp, CBI_SET | CBI_ENABLE);
            fRefreshStatusBars = TRUE;
            fShowStatusBars = TRUE;
        }
        break;

        case DID_DEFAULT:
        {
            // set the default settings for this settings page
            // (this is in common.c because it's also used at
            // Desktop startup)
            cmnSetDefaultSettings(pnbp->inbp.ulPageID);
            // update the display by calling the INIT callback
            pnbp->inbp.pfncbInitPage(pnbp, CBI_SET | CBI_ENABLE);
            fRefreshStatusBars = TRUE;
            fShowStatusBars = TRUE;
        }
        break;

        default:
            fSave = FALSE;
    }

    if (flStyleChanged)
    {
        ULONG fl = cmnQuerySetting(sflSBForViews);
        if (ulExtra)
            fl |= flStyleChanged;
        else
            fl &= ~flStyleChanged;
        cmnSetSetting(sflSBForViews, fl);
    }

    // cmnUnlockGlobalSettings();

    /* if (fSave)
        cmnStoreGlobalSettings(); */

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

#ifndef __NOCFGSTATUSBARS__

/*
 *@@ RefreshClassObject:
 *      returns the class object for szSBClassSelected.
 *      Added with V0.9.16 to fix the SOM string resource
 *      leaks finally.
 *
 *@@added V0.9.16 (2001-10-28) [umoeller]
 */

static VOID RefreshClassObject(PSTATUSBARPAGEDATA psbpd)
{
    somId somidClassSelected;
    if (somidClassSelected = somIdFromString(psbpd->szSBClassSelected))
    {
        // get pointer to class object (e.g. M_WPObject)
        psbpd->pSBClassObjectSelected = _somFindClass(SOMClassMgrObject,
                                                      somidClassSelected,
                                                      0,
                                                      0);
        SOMFree(somidClassSelected);        // V0.9.16 (2001-10-28) [umoeller]
    }
}

static const XWPSETTING G_StatusBar2Backup[] =
    {
        sflDereferenceShadows
    };

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
 *@@changed V0.9.14 (2001-07-31) [umoeller]: added "Keys" buttons support
 */

VOID stbStatusBar2InitPage(PNOTEBOOKPAGE pnbp,   // notebook info struct
                               ULONG flFlags)        // CBI_* flags (notebook.h)
{
    // PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    PSTATUSBARPAGEDATA psbpd = (PSTATUSBARPAGEDATA)pnbp->pUser2;

    if (flFlags & CBI_INIT)
    {


        if (pnbp->pUser == NULL)
        {
            // first call: backup Global Settings for "Undo" button;
            // this memory will be freed automatically by the
            // common notebook window function (notebook.c) when
            // the notebook page is destroyed
            /*
            pnbp->pUser = NEW(GLOBALSETTINGS);
            memcpy(pnbp->pUser, pGlobalSettings, sizeof(GLOBALSETTINGS));
               */
            pnbp->pUser = cmnBackupSettings(G_StatusBar2Backup,
                                             ARRAYITEMCOUNT(G_StatusBar2Backup));

            pnbp->pUser2
            = psbpd
                = NEW(STATUSBARPAGEDATA);
            ZERO(psbpd);
        }

        strcpy(psbpd->szSBTextNoneSelBackup,
               cmnQueryStatusBarSetting(SBS_TEXTNONESEL));
        strcpy(psbpd->szSBTextMultiSelBackup,
               cmnQueryStatusBarSetting(SBS_TEXTMULTISEL));
        // status bar settings page: get last selected
        // class from INIs (for single-object mode)
        // and query the SOM class object from this string
        PrfQueryProfileString(HINI_USER,
                              (PSZ)INIAPP_XWORKPLACE,
                              (PSZ)INIKEY_SB_LASTCLASS,
                              (PSZ)G_pcszXFldObject,     // "XFldObject", default
                              psbpd->szSBClassSelected,
                              sizeof(psbpd->szSBClassSelected));
        if (psbpd->pSBClassObjectSelected == NULL)
            RefreshClassObject(psbpd);

        if (psbpd->pSBClassObjectSelected)
            strcpy(psbpd->szSBText1SelBackup,
                   stbQueryClassMnemonics(psbpd->pSBClassObjectSelected));

        ctlMakeMenuButton(WinWindowFromID(pnbp->hwndDlgPage, ID_XSDI_SBKEYSNONESEL), 0, 0);
        ctlMakeMenuButton(WinWindowFromID(pnbp->hwndDlgPage, ID_XSDI_SBKEYS1SEL), 0, 0);
        ctlMakeMenuButton(WinWindowFromID(pnbp->hwndDlgPage, ID_XSDI_SBKEYSMULTISEL), 0, 0);

    }

    if (flFlags & CBI_SET)
    {
        // current class
        WinSetDlgItemText(pnbp->hwndDlgPage,
                          ID_XSDI_SBCURCLASS,
                          psbpd->szSBClassSelected);

        // no-object mode
        WinSendDlgItemMsg(pnbp->hwndDlgPage, ID_XSDI_SBTEXTNONESEL,
                          EM_SETTEXTLIMIT,
                          (MPARAM)(CCHMAXMNEMONICS-1),
                          MPNULL);
        WinSetDlgItemText(pnbp->hwndDlgPage,
                          ID_XSDI_SBTEXTNONESEL ,
                          (PSZ)cmnQueryStatusBarSetting(SBS_TEXTNONESEL));

        // one-object mode
        WinSendDlgItemMsg(pnbp->hwndDlgPage,
                          ID_XSDI_SBTEXT1SEL,
                          EM_SETTEXTLIMIT,
                          (MPARAM)(CCHMAXMNEMONICS-1),
                          MPNULL);
        if (psbpd->pSBClassObjectSelected == NULL)
            RefreshClassObject(psbpd);

        if (psbpd->pSBClassObjectSelected)
            WinSetDlgItemText(pnbp->hwndDlgPage,
                              ID_XSDI_SBTEXT1SEL,
                              stbQueryClassMnemonics(psbpd->pSBClassObjectSelected));

        // dereference shadows
        winhSetDlgItemChecked(pnbp->hwndDlgPage,
                              ID_XSDI_DEREFSHADOWS_SINGLE,
                              (cmnQuerySetting(sflDereferenceShadows) & STBF_DEREFSHADOWS_SINGLE)
                                    != 0);

        // multiple-objects mode
        WinSendDlgItemMsg(pnbp->hwndDlgPage,
                          ID_XSDI_SBTEXTMULTISEL,
                          EM_SETTEXTLIMIT,
                          (MPARAM)(CCHMAXMNEMONICS-1),
                          MPNULL);
        WinSetDlgItemText(pnbp->hwndDlgPage,
                          ID_XSDI_SBTEXTMULTISEL,
                          (PSZ)cmnQueryStatusBarSetting(SBS_TEXTMULTISEL));

        winhSetDlgItemChecked(pnbp->hwndDlgPage,
                              ID_XSDI_DEREFSHADOWS_MULTIPLE,
                              (cmnQuerySetting(sflDereferenceShadows) & STBF_DEREFSHADOWS_MULTIPLE)
                                    != 0);
    }

    if (flFlags & CBI_DESTROY)
    {
        if (psbpd)
        {
            if (psbpd->pSBClassObjectSelected)
                psbpd->pSBClassObjectSelected = NULL;

            if (psbpd->hwndKeysMenu)
                WinDestroyWindow(psbpd->hwndKeysMenu),

            free(psbpd);

            pnbp->pUser2 = NULL;
        }
    }
}

/*
 *@@ KEYARRAYITEM:
 *      specifies one status bar mnemonic
 *      to be inserted into a "Keys" menu.
 *
 *      If the first character oc pcszKey is '@',
 *      we insert a submenu with the format
 *      specifiers.
 *
 *@@added V0.9.14 (2001-07-31) [umoeller]
 */

typedef struct _KEYARRAYITEM
{
    ULONG       ulItemID;               // menu item ID (hard-coded)
    const char  *pcszKey;               // actual mnemonic (e.g. "$C")
    ULONG       ulDescription;          // string ID for description
} KEYARRAYITEM, *PKEYARRAYITEM;

static KEYARRAYITEM G_aFormatSubKeys[] =
    {
        1, "b", ID_XSSI_SBMNC_1,       // "in bytes"
        2, "k", ID_XSSI_SBMNC_2,       // "in kBytes"
        3, "K", ID_XSSI_SBMNC_3,       // "in KBytes"
        4, "m", ID_XSSI_SBMNC_4,       // "in mBytes"
        5, "M", ID_XSSI_SBMNC_5,       // "in MBytes"
        6, "a", ID_XSSI_SBMNC_6,       // "in bytes/kBytes/mBytes/gBytes"
        7, "A", ID_XSSI_SBMNC_7       // "in bytes/KBytes/MBytes/GBytes"
    },

    G_aAllModeKeys[] =
    {
        31000, "$c", ID_XSSI_SBMNC_000,       // "no. of selected objects"
        31001, "$C", ID_XSSI_SBMNC_001,       // "total object count"

        31010, "@$f", ID_XSSI_SBMNC_010,       // "free space on drive"
        31020, "@$z", ID_XSSI_SBMNC_020,       // "total size of drive"
        31030, "@$s", ID_XSSI_SBMNC_030,       // "size of selected objects in bytes"
        31040, "@$S", ID_XSSI_SBMNC_040       // "size of folder content in bytes"
    },

    G_a1ObjectCommonKeys[] =
    {
        31100, "$t", ID_XSSI_SBMNC_100,       // "object title"
        31110, "$w", ID_XSSI_SBMNC_110,       // "WPS class default title"
        31120, "$W", ID_XSSI_SBMNC_120       // "WPS class name"
    },

    G_a1ObjectWPDiskKeys[] =
    {
        31200, "$F", ID_XSSI_SBMNC_200,       // "file system type (HPFS, FAT, CDFS, ...)"

        31210, "$L", ID_XSSI_SBMNC_210,       // "drive label"

        // skipping "free space on drive in bytes" which is redefined

        31220, "@$z", ID_XSSI_SBMNC_220       // "total space on drive in bytes"
    },

    G_a1ObjectWPFileSystemKeys[] =
    {
        31300, "$r", ID_XSSI_SBMNC_300,       // "object's real name"

        31310, "$y", ID_XSSI_SBMNC_310,       // "object type (.TYPE EA)"
        31320, "$D", ID_XSSI_SBMNC_320,       // "object creation date"
        31330, "$T", ID_XSSI_SBMNC_330,       // "object creation time"
        31340, "$a", ID_XSSI_SBMNC_340,       // "object attributes"

        31350, "$Eb", ID_XSSI_SBMNC_350,       // "EA size in bytes"
        31360, "$Ek", ID_XSSI_SBMNC_360,       // "EA size in kBytes"
        31370, "$EK", ID_XSSI_SBMNC_370,       // "EA size in KBytes"
        31380, "$Ea", ID_XSSI_SBMNC_380,       // "EA size in bytes/kBytes"
        31390, "$EA", ID_XSSI_SBMNC_390       // "EA size in bytes/KBytes"
    },

    G_a1ObjectWPUrlKeys[] =
    {
        31400, "$U", ID_XSSI_SBMNC_400       // "URL"
    },

    G_a1ObjectWPProgramKeys[] =
    {
        31500, "$p", ID_XSSI_SBMNC_500,       // "executable program file"
        31510, "$P", ID_XSSI_SBMNC_510,       // "parameter list"
        31520, "$d", ID_XSSI_SBMNC_520       // "working directory"
    };

/*
 *@@ InsertKeysIntoMenu:
 *      helper for building the "Keys" menu button menus.
 *
 *@@added V0.9.14 (2001-07-31) [umoeller]
 */

static VOID InsertKeysIntoMenu(HWND hwndMenu,
                               PKEYARRAYITEM paKeys,
                               ULONG cKeys,
                               BOOL fSeparatorBefore)
{
    ULONG ul;
    XSTRING str;
    xstrInit(&str, 100);

    if (fSeparatorBefore)
        winhInsertMenuSeparator(hwndMenu,
                                MIT_END,
                                0);

    for (ul = 0;
         ul < cKeys;
         ul++)
    {
        xstrcpy(&str, cmnGetString(paKeys[ul].ulDescription), 0);

        // if the first char is '@', build a submenu
        // with the various format characters
        if (*(paKeys[ul].pcszKey) == '@')
        {
            HWND hwndSubmenu = winhInsertSubmenu(hwndMenu,
                                                 MIT_END,
                                                 paKeys[ul].ulItemID,
                                                 str.psz,
                                                 MIS_TEXT,
                                                 0, NULL, 0, 0);
            ULONG ul2;
            for (ul2 = 0;
                 ul2 < ARRAYITEMCOUNT(G_aFormatSubKeys);
                 ul2++)
            {
                xstrcpy(&str, cmnGetString(G_aFormatSubKeys[ul2].ulDescription), 0);
                xstrcatc(&str, '\t');
                xstrcat(&str, paKeys[ul].pcszKey + 1, 0);
                xstrcat(&str, G_aFormatSubKeys[ul2].pcszKey, 0);

                winhInsertMenuItem(hwndSubmenu,
                                   MIT_END,
                                   G_aFormatSubKeys[ul2].ulItemID
                                        + paKeys[ul].ulItemID,
                                   str.psz,
                                   MIS_TEXT,
                                   0);
            }
        }
        else
        {
            // first character is not '@':
            // simply insert as such
            xstrcatc(&str, '\t');
            xstrcat(&str, paKeys[ul].pcszKey, 0);

            winhInsertMenuItem(hwndMenu,
                               MIT_END,
                               paKeys[ul].ulItemID,
                               str.psz,
                               MIS_TEXT,
                               0);
        }
    }

    xstrClear(&str);
}

/*
 *@@ CreateKeysMenu:
 *
 *@@added V0.9.14 (2001-07-31) [umoeller]
 */

static MRESULT CreateKeysMenu(PSTATUSBARPAGEDATA psbpd,
                              ULONG ulItemID)
{
    HPOINTER hptrOld = winhSetWaitPointer();

    if (psbpd->hwndKeysMenu)
        WinDestroyWindow(psbpd->hwndKeysMenu);

    // different button or first call:
    // build menu then
    psbpd->hwndKeysMenu = WinCreateMenu(HWND_DESKTOP, NULL);

    InsertKeysIntoMenu(psbpd->hwndKeysMenu,
                       G_aAllModeKeys,
                       ARRAYITEMCOUNT(G_aAllModeKeys),
                       FALSE);
    if (ulItemID == ID_XSDI_SBKEYS1SEL)
    {
        InsertKeysIntoMenu(psbpd->hwndKeysMenu,
                           G_a1ObjectCommonKeys,
                           ARRAYITEMCOUNT(G_a1ObjectCommonKeys),
                           TRUE);

        if (_somDescendedFrom(psbpd->pSBClassObjectSelected,
                              _WPFileSystem))
        {
            InsertKeysIntoMenu(psbpd->hwndKeysMenu,
                               G_a1ObjectWPFileSystemKeys,
                               ARRAYITEMCOUNT(G_a1ObjectWPFileSystemKeys),
                               TRUE);

            ResolveWPUrl();
            if (_somDescendedFrom(psbpd->pSBClassObjectSelected,
                                  G_WPUrl))
                InsertKeysIntoMenu(psbpd->hwndKeysMenu,
                                   G_a1ObjectWPUrlKeys,
                                   ARRAYITEMCOUNT(G_a1ObjectWPUrlKeys),
                                   TRUE);
        }
        else if (_somDescendedFrom(psbpd->pSBClassObjectSelected,
                                   _WPProgram))
                InsertKeysIntoMenu(psbpd->hwndKeysMenu,
                                   G_a1ObjectWPProgramKeys,
                                   ARRAYITEMCOUNT(G_a1ObjectWPProgramKeys),
                                   TRUE);
        else if (_somDescendedFrom(psbpd->pSBClassObjectSelected,
                                   _WPDisk))
                InsertKeysIntoMenu(psbpd->hwndKeysMenu,
                                   G_a1ObjectWPDiskKeys,
                                   ARRAYITEMCOUNT(G_a1ObjectWPDiskKeys),
                                   TRUE);
    }

    // store what we had so we can reuse the menu
    psbpd->ulLastKeysID = ulItemID;

    WinSetPointer(HWND_DESKTOP, hptrOld);

    return (MRESULT)psbpd->hwndKeysMenu;
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
 *@@changed V0.9.14 (2001-07-31) [umoeller]: added "Keys" buttons support
 *@@changed V0.9.14 (2001-07-31) [umoeller]: "Undo" didn't undo everything, fixed
 */

MRESULT stbStatusBar2ItemChanged(PNOTEBOOKPAGE pnbp,
                                 ULONG ulItemID,
                                 USHORT usNotifyCode,
                                 ULONG ulExtra)      // for checkboxes: contains new state
{
    MRESULT     mrc = (MPARAM)0;
    BOOL        fSave = TRUE,
                fReadEFs = FALSE;           // read codes from entry fields?
                                            // V0.9.14 (2001-07-31) [umoeller]
    CHAR        szDummy[CCHMAXMNEMONICS];
    PSTATUSBARPAGEDATA psbpd = (PSTATUSBARPAGEDATA)pnbp->pUser2;

    ULONG       flChanged = 0;

    switch (ulItemID)
    {
        case ID_XSDI_SBTEXTNONESEL:
            if (usNotifyCode == EN_KILLFOCUS)   // changed V0.9.0
                fReadEFs = TRUE;
            else
                fSave = FALSE;
        break;

        case ID_XSDI_SBTEXT1SEL:
            if (usNotifyCode == EN_KILLFOCUS)   // changed V0.9.0
                fReadEFs = TRUE;
            else
                fSave = FALSE;
        break;

        case ID_XSDI_DEREFSHADOWS_SINGLE:    // added V0.9.0
            flChanged = STBF_DEREFSHADOWS_SINGLE;
        break;

        case ID_XSDI_SBTEXTMULTISEL:
            if (usNotifyCode == EN_KILLFOCUS)   // changed V0.9.0
                fReadEFs = TRUE;
            else
                fSave = FALSE;
        break;

        case ID_XSDI_DEREFSHADOWS_MULTIPLE:    // added V0.9.5 (2000-10-07) [umoeller]
            flChanged = STBF_DEREFSHADOWS_MULTIPLE;
        break;

        // "Select class" on "Status Bars" page:
        // set up WPS classes dialog
        case ID_XSDI_SBSELECTCLASS:
        {
            SELECTCLASSDATA         scd;
            STATUSBARSELECTCLASS    sbsc;

            XSTRING strTitle,
                    strIntroText;
            xstrInit(&strTitle, 0);
            xstrInit(&strIntroText, 0);
            cmnGetMessage(NULL, 0, &strTitle, 112);
            cmnGetMessage(NULL, 0, &strIntroText, 113);
            scd.pszDlgTitle = strTitle.psz;
            scd.pszIntroText = strIntroText.psz;
            scd.pcszRootClass = G_pcszWPObject;
            scd.pcszOrphans = NULL;
            strcpy(scd.szClassSelected, psbpd->szSBClassSelected);

            // these callback funcs are defined way more below
            scd.pfnwpReturnAttrForClass = fncbWPSStatusBarReturnClassAttr;
            scd.pfnwpClassSelected = fncbWPSStatusBarClassSelected;
            // the folllowing data will be passed to the callbacks
            // so the callback can display NLS messages
            scd.ulUserClassSelected = (ULONG)&sbsc;

            scd.pszHelpLibrary = cmnQueryHelpLibrary();
            scd.ulHelpPanel = ID_XFH_SELECTCLASS;

            // classlst.c
            if (clsSelectWpsClassDlg(pnbp->hwndFrame, // owner
                                     cmnQueryNLSModuleHandle(FALSE),
                                     ID_XLD_SELECTCLASS,
                                     &scd)
                          == DID_OK)
            {
                strcpy(psbpd->szSBClassSelected, scd.szClassSelected);
                WinSetDlgItemText(pnbp->hwndDlgPage,
                                  ID_XSDI_SBCURCLASS,
                                  psbpd->szSBClassSelected);
                PrfWriteProfileString(HINI_USER,
                                      (PSZ)INIAPP_XWORKPLACE,
                                      (PSZ)INIKEY_SB_LASTCLASS,
                                      psbpd->szSBClassSelected);
                if (psbpd->pSBClassObjectSelected)
                    psbpd->pSBClassObjectSelected = NULL;
                    // this will provoke the following func to re-read
                    // the class's status bar mnemonics

                // update the display by calling the INIT callback
                pnbp->inbp.pfncbInitPage(pnbp, CBI_SET | CBI_ENABLE);

                // refresh the "Undo" data for this
                strcpy(psbpd->szSBText1SelBackup,
                       stbQueryClassMnemonics(psbpd->pSBClassObjectSelected));

            }

            xstrClear(&strTitle);
            xstrClear(&strIntroText);
        }
        break;

        // "Keys" buttons next to entry field
        // V0.9.14 (2001-07-31) [umoeller]
        case ID_XSDI_SBKEYSNONESEL:
        case ID_XSDI_SBKEYS1SEL:
        case ID_XSDI_SBKEYSMULTISEL:
            mrc = CreateKeysMenu(psbpd,
                                 ulItemID);
        break;

        case DID_UNDO:
        {
            // "Undo" button: get pointer to backed-up Global Settings
            // GLOBALSETTINGS *pGSBackup = (GLOBALSETTINGS*)(pnbp->pUser);

            // and restore the settings for this page
            // GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(__FILE__, __LINE__, __FUNCTION__);
            // cmnSetSetting(sflDereferenceShadows, pGSBackup->bDereferenceShadows);
                        // V0.9.14 (2001-07-31) [umoeller]
            // cmnUnlockGlobalSettings();

            cmnRestoreSettings(pnbp->pUser,
                               ARRAYITEMCOUNT(G_StatusBar2Backup));

            cmnSetStatusBarSetting(SBS_TEXTNONESEL,
                                   psbpd->szSBTextNoneSelBackup);
            cmnSetStatusBarSetting(SBS_TEXTMULTISEL,
                                   psbpd->szSBTextMultiSelBackup);

            if (!psbpd->pSBClassObjectSelected)
                RefreshClassObject(psbpd);

            if (psbpd->pSBClassObjectSelected)
                stbSetClassMnemonics(psbpd->pSBClassObjectSelected,
                                     psbpd->szSBText1SelBackup);

            // update the display by calling the INIT callback
            pnbp->inbp.pfncbInitPage(pnbp, CBI_SET | CBI_ENABLE);
        }
        break;

        case DID_DEFAULT:
        {
            // set the default settings for this settings page
            // (this is in common.c because it's also used at
            // Desktop startup)
            // GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(__FILE__, __LINE__, __FUNCTION__);

            cmnSetStatusBarSetting(SBS_TEXTNONESEL, NULL);  // load default
            cmnSetStatusBarSetting(SBS_TEXTMULTISEL, NULL); // load default

            cmnSetSetting(sflDereferenceShadows, STBF_DEREFSHADOWS_SINGLE);
                        // V0.9.14 (2001-07-31) [umoeller]
            // cmnUnlockGlobalSettings();

            if (!psbpd->pSBClassObjectSelected)
                RefreshClassObject(psbpd);

            if (psbpd->pSBClassObjectSelected)
                stbSetClassMnemonics(psbpd->pSBClassObjectSelected,
                                     NULL);  // load default

            // update the display by calling the INIT callback
            pnbp->inbp.pfncbInitPage(pnbp, CBI_SET | CBI_ENABLE);
        }
        break;

        default:
        {
            fSave = FALSE;

            if (ulItemID >= 31000)
            {
                // one of the menu item IDs from the "Keys" menu:
                // look up the item in the menu we built then
                PSZ psz = NULL;
                if (    (psbpd->hwndKeysMenu)
                     && (psz = winhQueryMenuItemText(psbpd->hwndKeysMenu,
                                                     ulItemID))
                   )
                {
                    // alright, now we got the menu item text...
                    // find the tab character, after which is the
                    // key we need to insert
                    PCSZ p;
                    if (p = strchr(psz, '\t'))
                    {
                        // p points to the key to insert now...
                        // find the entry field to insert into,
                        // this can come in for any of the three
                        // buttons

                        ULONG ulEFID = 0;
                        switch (psbpd->ulLastKeysID)
                        {
                            case ID_XSDI_SBKEYSNONESEL:
                                ulEFID = ID_XSDI_SBTEXTNONESEL;
                            break;

                            case ID_XSDI_SBKEYS1SEL:
                                ulEFID = ID_XSDI_SBTEXT1SEL;
                            break;

                            case ID_XSDI_SBKEYSMULTISEL:
                                ulEFID = ID_XSDI_SBTEXTMULTISEL;
                            break;
                        }

                        if (ulEFID)
                        {
                            HWND hwndEF = WinWindowFromID(pnbp->hwndDlgPage,
                                                          ulEFID);
                            MRESULT mr = WinSendMsg(hwndEF,
                                                    EM_QUERYSEL,
                                                    0,
                                                    0);
                            SHORT s1 = SHORT1FROMMR(mr),
                                  s2 = SHORT2FROMMR(mr);

                            PSZ pszOld;

                            if (pszOld = winhQueryWindowText(hwndEF))
                            {
                                XSTRING strNew;
                                ULONG   ulNewLength = strlen(p + 1);

                                xstrInitSet(&strNew, pszOld);

                                xstrrpl(&strNew,
                                        // first char to replace:
                                        s1,
                                        // no. of chars to replace:
                                        s2 - s1,
                                        // string to replace chars with:
                                        p + 1,
                                        ulNewLength);

                                WinSetWindowText(hwndEF,
                                                 strNew.psz);

                                WinSendMsg(hwndEF,
                                           EM_SETSEL,
                                           MPFROM2SHORT(s1,
                                                        s1 + ulNewLength),
                                           0);

                                WinSetFocus(HWND_DESKTOP, hwndEF);

                                xstrClear(&strNew);

                                fSave = TRUE;
                                fReadEFs = TRUE;
                            }
                        }
                    }

                    free(psz);
                }
            }
        }
    }

    if (flChanged)
    {
        ULONG fl = cmnQuerySetting(sflDereferenceShadows);
        if (ulExtra)
            fl |= flChanged;
        else
            fl &= ~flChanged;
        cmnSetSetting(sflDereferenceShadows, fl);
    }

    if (fSave)
    {
        if (fReadEFs)
        {
            // "none selected" codes:

            WinQueryDlgItemText(pnbp->hwndDlgPage,
                                ID_XSDI_SBTEXTNONESEL,
                                sizeof(szDummy)-1, szDummy);
            cmnSetStatusBarSetting(SBS_TEXTNONESEL, szDummy);

            if (!psbpd->pSBClassObjectSelected)
                RefreshClassObject(psbpd);

            // "one selected" codes:
            if (psbpd->pSBClassObjectSelected)
            {
                WinQueryDlgItemText(pnbp->hwndDlgPage, ID_XSDI_SBTEXT1SEL,
                                    sizeof(szDummy)-1, szDummy);
                stbSetClassMnemonics(psbpd->pSBClassObjectSelected,
                                     szDummy);
            }

            // "multiple selected" codes:
            WinQueryDlgItemText(pnbp->hwndDlgPage, ID_XSDI_SBTEXTMULTISEL,
                                sizeof(szDummy)-1, szDummy);
            cmnSetStatusBarSetting(SBS_TEXTMULTISEL, szDummy);
        }

        // cmnStoreGlobalSettings();

        // have the Worker thread update the
        // status bars for all currently open
        // folders
        xthrPostWorkerMsg(WOM_UPDATEALLSTATUSBARS,
                          (MPARAM)2, // update display
                          MPNULL);
    }

    return (mrc);
}

#endif // XWPLITE
