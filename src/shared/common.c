
/*
 *@@sourcefile common.c:
 *      this file contains functions that are common to all
 *      parts of XWorkplace.
 *
 *      These functions mainly deal with the following features:
 *
 *      -- module handling (cmnQueryMainModuleHandle etc.);
 *
 *      -- NLS management (cmnQueryNLSModuleHandle,
 *         cmnQueryNLSStrings);
 *
 *      -- global settings (cmnQueryGlobalSettings);
 *
 *      -- extended XWorkplace message boxes (cmnMessageBox).
 *
 *      Note that the system sound functions have been exported
 *      to helpers\syssound.c (V0.9.0).
 *
 *@@header "shared\common.h"
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

#define INCL_DOSMODULEMGR
#define INCL_DOSSEMAPHORES
#define INCL_DOSEXCEPTIONS
#define INCL_DOSPROCESS
#define INCL_DOSMISC
#define INCL_DOSERRORS

#define INCL_WINSHELLDATA       // Prf* functions
#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR        // SC_CLOSE etc.
#define INCL_WINPOINTERS
#define INCL_WININPUT
#define INCL_WINDIALOGS
#define INCL_WINMENUS
#define INCL_WINBUTTONS
#define INCL_WINSTDCNR
#define INCL_WINCOUNTRY
#define INCL_WINSYS

#define INCL_GPILOGCOLORTABLE
#define INCL_GPIBITMAPS
#include <os2.h>

// C library headers
#include <stdio.h>
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h
#include <io.h>

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\cnrh.h"               // container helper routines
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\gpih.h"               // GPI helper routines
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\stringh.h"            // string helper routines
#include "helpers\textview.h"           // PM XTextView control
#include "helpers\tmsgfile.h"           // "text message file" handling (for cmnGetMessage)
#include "helpers\winh.h"               // PM helper routines
#include "helpers\xstring.h"            // extended string helpers

// SOM headers which don't crash with prec. header files
#pragma hdrstop                         // VAC++ keeps crashing otherwise
#include "xtrash.ih"                    // XWPTrashCan; needed for empty trash
#include <wpdesk.h>                     // WPDesktop

// XWorkplace implementation headers
#include "bldlevel.h"                   // XWorkplace build level definitions
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\xsetup.h"              // XWPSetup implementation

#include "filesys\statbars.h"           // status bar translation logic
#include "filesys\xthreads.h"           // extra XWorkplace threads

#include "media\media.h"                // XWorkplace multimedia support

// other SOM headers
#include "helpers\undoc.h"              // some undocumented stuff

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

static CHAR            G_szHelpLibrary[CCHMAXPATH] = "";
static CHAR            G_szMessageFile[CCHMAXPATH] = "";

// main module (XFLDR.DLL)
static char            G_szDLLFile[CCHMAXPATH];
static HMODULE         G_hmodDLL = NULLHANDLE;

// res module (XWPRES.DLL)
static HMODULE         G_hmodRes = NULLHANDLE;

// NLS
static HMODULE         G_hmodNLS = NULLHANDLE;
static NLSSTRINGS      *G_pNLSStringsGlobal = NULL;
static GLOBALSETTINGS  *G_pGlobalSettings = NULL;

static HMODULE         G_hmodIconsDLL = NULLHANDLE;
static CHAR            G_szLanguageCode[20] = "";

static COUNTRYSETTINGS G_CountrySettings;                  // V0.9.6 (2000-11-12) [umoeller]
static BOOL            G_fCountrySettingsLoaded = FALSE;

static ULONG           G_ulCurHelpPanel = 0;      // holds help panel for dialog

static CHAR            G_szStatusBarFont[100];
static CHAR            G_szSBTextNoneSel[CCHMAXMNEMONICS],
                       G_szSBTextMultiSel[CCHMAXMNEMONICS];
static ULONG           G_ulStatusBarHeight;

// Declare C runtime prototypes, because there are no headers
// for these:

// _CRT_init is the C run-time environment initialization function.
// It will return 0 to indicate success and -1 to indicate failure.
int _CRT_init(void);

// _CRT_term is the C run-time environment termination function.
// It only needs to be called when the C run-time functions are statically
// linked, as is the case with XFolder.
void _CRT_term(void);

/********************************************************************
 *
 *   INI keys
 *
 ********************************************************************/

/*
 *  All these constants are declared as "extern" in
 *  common.h. They all used to be #define's in common.h,
 *  which put a lot of duplicates of them into the .obj
 *  files (and also stress on the compiler, since it had
 *  to do comparisons on them... and didn't even know that
 *  they were really constant).
 *
 *  These have been moved here with V0.9.7 (2001-01-17) [umoeller]
 */

/*
 * XWorkplace application:
 *
 */

// INI key used with V0.9.1 and above
const char      *INIAPP_XWORKPLACE       = "XWorkplace";

// INI key used by XFolder and XWorkplace 0.9.0;
// this is checked for if INIAPP_XWORKPLACE is not
// found and converted
// const char      *INIAPP_OLDXFOLDER       = "XFolder";
const char      *INIAPP_OLDXFOLDER       = "XFolder";

/*
 * XWorkplace keys:
 *      Add the keys you are using for storing your data here.
 *      Note: If anything has been marked as "removed" here,
 *      do not use that string, because it might still exist
 *      in a user's OS2.INI file.
 */

// const char      *INIKEY_DEFAULTTITLE     = "DefaultTitle";       removed V0.9.0
const char      *INIKEY_GLOBALSETTINGS   = "GlobalSettings";
// const char      *INIKEY_XFOLDERPATH      = "XFolderPath";        removed V0.81 (I think)
const char      *INIKEY_ACCELERATORS     = "Accelerators";
const char      *INIKEY_LANGUAGECODE     = "Language";
const char      *INIKEY_JUSTINSTALLED    = "JustInstalled";
// const char      *INIKEY_DONTDOSTARTUP    = "DontDoStartup";      removed V0.84 (I think)
// const char      *INIKEY_LASTPID          = "LastPID";            removed V0.84 (I think)
const char      *INIKEY_FAVORITEFOLDERS  = "FavoriteFolders";
const char      *INIKEY_QUICKOPENFOLDERS = "QuickOpenFolders";

const char      *INIKEY_WNDPOSSTARTUP    = "WndPosStartup";
const char      *INIKEY_WNDPOSNAMECLASH  = "WndPosNameClash";
const char      *INIKEY_NAMECLASHFOCUS   = "NameClashLastFocus";

const char      *INIKEY_STATUSBARFONT    = "SB_Font";
const char      *INIKEY_SBTEXTNONESEL    = "SB_NoneSelected";
const char      *INIKEY_SBTEXT_WPOBJECT  = "SB_WPObject";
const char      *INIKEY_SBTEXT_WPPROGRAM = "SB_WPProgram";
const char      *INIKEY_SBTEXT_WPFILESYSTEM = "SB_WPDataFile";
const char      *INIKEY_SBTEXT_WPURL        = "SB_WPUrl";
const char      *INIKEY_SBTEXT_WPDISK    = "SB_WPDisk";
const char      *INIKEY_SBTEXT_WPFOLDER  = "SB_WPFolder";
const char      *INIKEY_SBTEXTMULTISEL   = "SB_MultiSelected";
const char      *INIKEY_SB_LASTCLASS     = "SB_LastClass";
const char      *INIKEY_DLGFONT          = "DialogFont";

const char      *INIKEY_BOOTMGR          = "RebootTo";
const char      *INIKEY_AUTOCLOSE        = "AutoClose";

const char      *DEFAULT_LANGUAGECODE    = "001";

// window position of "WPS Class list" window (V0.9.0)
const char      *INIKEY_WNDPOSCLASSINFO  = "WndPosClassInfo";

// last directory used on "Sound" replacement page (V0.9.0)
const char      *INIKEY_XWPSOUNDLASTDIR  = "XWPSound:LastDir";
// last sound scheme selected (V0.9.0)
const char      *INIKEY_XWPSOUNDSCHEME   = "XWPSound:Scheme";

// boot logo .BMP file (V0.9.0)
const char      *INIKEY_BOOTLOGOFILE     = "BootLogoFile";

// last ten selections in "Select some" (V0.9.0)
const char      *INIKEY_LAST10SELECTSOME = "SelectSome";

// supported drives in XWPTrashCan (V0.9.1 (99-12-14) [umoeller])
const char      *INIKEY_TRASHCANDRIVES   = "TrashCan::Drives";

// window pos of file operations status window V0.9.1 (2000-01-30) [umoeller]
const char      *INIKEY_FILEOPSPOS       = "WndPosFileOpsStatus";

// window pos of "Partitions" view V0.9.2 (2000-02-29) [umoeller]
const char      *INIKEY_WNDPOSPARTITIONS = "WndPosPartitions";

// window position of XMMVolume control V0.9.6 (2000-11-09) [umoeller]
const char      *INIKEY_WNDPOSXMMVOLUME  = "WndPosXMMVolume";

// window position of XMMCDPlayer V0.9.7 (2000-12-20) [umoeller]
const char      *INIKEY_WNDPOSXMMCDPLAY  = "WndPosXMMCDPlayer::";
                // object handle appended

// font samples (XWPFontObject) V0.9.7 (2001-01-17) [umoeller]
const char      *INIKEY_FONTSAMPLEWNDPOS = "WndPosFontSample";
const char      *INIKEY_FONTSAMPLESTRING = "FontSampleString";
const char      *INIKEY_FONTSAMPLEHINTS  = "FontSampleHints";

/*
 * file type hierarchies:
 *
 */

// application for file type hierarchies
const char      *INIAPP_XWPFILETYPES     = "XWorkplace:FileTypes";   // added V0.9.0
const char      *INIAPP_XWPFILEFILTERS   = "XWorkplace:FileFilters"; // added V0.9.0

const char      *INIAPP_REPLACEFOLDERREFRESH = "ReplaceFolderRefresh";
                                    // V0.9.9 (2001-01-31) [umoeller]

/*
 * some default WPS INI keys:
 *
 */

const char      *WPINIAPP_LOCATION       = "PM_Workplace:Location";
const char      *WPINIAPP_FOLDERPOS      = "PM_Workplace:FolderPos";
const char      *WPINIAPP_ASSOCTYPE      = "PMWP_ASSOC_TYPE";
const char      *WPINIAPP_ASSOCFILTER    = "PMWP_ASSOC_FILTER";

/********************************************************************
 *
 *   Standard WPS object IDs
 *
 ********************************************************************/

const char *WPOBJID_DESKTOP = "<WP_DESKTOP>";

const char *WPOBJID_KEYB = "<WP_KEYB>";
const char *WPOBJID_MOUSE = "<WP_MOUSE>";
const char *WPOBJID_CNTRY = "<WP_CNTRY>";
const char *WPOBJID_SOUND = "<WP_SOUND>";
const char *WPOBJID_POWER = "<WP_POWER>";
const char *WPOBJID_WINCFG = "<WP_WINCFG>";

const char *WPOBJID_HIRESCLRPAL = "<WP_HIRESCLRPAL>";
const char *WPOBJID_LORESCLRPAL = "<WP_LORESCLRPAL>";
const char *WPOBJID_FNTPAL = "<WP_FNTPAL>";
const char *WPOBJID_SCHPAL96 = "<WP_SCHPAL96>";

const char *WPOBJID_LAUNCHPAD = "<WP_LAUNCHPAD>";
const char *WPOBJID_WARPCENTER = "<WP_WARPCENTER>";

const char *WPOBJID_SPOOL = "<WP_SPOOL>";
const char *WPOBJID_VIEWER = "<WP_VIEWER>";
const char *WPOBJID_SHRED = "<WP_SHRED>";
const char *WPOBJID_CLOCK = "<WP_CLOCK>";

const char *WPOBJID_START = "<WP_START>";
const char *WPOBJID_TEMPS = "<WP_TEMPS>";
const char *WPOBJID_DRIVES = "<WP_DRIVES>";

/********************************************************************
 *
 *   XWorkplace object IDs
 *
 ********************************************************************/

// all of these have been redone with V0.9.2

// folders
const char      *XFOLDER_MAINID          = "<XWP_MAINFLDR>";
const char      *XFOLDER_CONFIGID        = "<XWP_CONFIG>";

const char      *XFOLDER_STARTUPID       = "<XWP_STARTUP>";
const char      *XFOLDER_SHUTDOWNID      = "<XWP_SHUTDOWN>";

const char      *XFOLDER_WPSID           = "<XWP_WPS>";
const char      *XFOLDER_KERNELID        = "<XWP_KERNEL>";
const char      *XFOLDER_SCREENID        = "<XWP_SCREEN>";
const char      *XFOLDER_MEDIAID         = "<XWP_MEDIA>";

const char      *XFOLDER_CLASSLISTID     = "<XWP_CLASSLIST>";
const char      *XFOLDER_TRASHCANID      = "<XWP_TRASHCAN>";
const char      *XFOLDER_XCENTERID       = "<XWP_XCENTER>";

const char      *XFOLDER_INTROID         = "<XWP_INTRO>";
const char      *XFOLDER_USERGUIDE       = "<XWP_REF>";

const char      *XWORKPLACE_ARCHIVE_MARKER   = "xwparchv.tmp";
            // archive marker file in Desktop directory V0.9.4 (2000-08-03) [umoeller]

/********************************************************************
 *
 *   Thread object windows
 *
 ********************************************************************/

// object window class names (added V0.9.0)
const char      *WNDCLASS_WORKEROBJECT         = "XWPWorkerObject";
const char      *WNDCLASS_QUICKOBJECT          = "XWPQuickObject";
const char      *WNDCLASS_FILEOBJECT           = "XWPFileObject";

const char      *WNDCLASS_THREAD1OBJECT        = "XWPThread1Object";
const char      *WNDCLASS_SUPPLOBJECT          = "XWPSupplFolderObject";

/* ******************************************************************
 *
 *   Main module handling (XFLDR.DLL)
 *
 ********************************************************************/

/*
 *@@ _DLL_InitTerm:
 *      this function gets called automatically by the OS/2
 *      module manager during DosLoadModule processing, on
 *      the thread which invoked DosLoadModule.
 *
 *      Since this is a SOM DLL for the WPS, this gets called
 *      right when the WPS is starting and when the WPS process
 *      ends, e.g. due to a WPS restart or trap. Since the WPS
 *      is the only process loading this DLL, we need not bother
 *      with details.
 *
 *      Defining this function is my preferred way of getting the
 *      DLL's module handle, instead of querying the SOM kernel
 *      for the module name, like this is done in most WPS sample
 *      programs provided by IBM. I have found this to be much
 *      easier and less error-prone when several classes are put
 *      into one DLL (as is the case with XWorkplace).
 *
 *      Besides, this is faster, since we store the module handle
 *      in a global variable which can later quickly be retrieved
 *      using cmnQueryMainModuleHandle.
 *
 *      Since OS/2 calls this function directly, it must have
 *      _System linkage.
 *
 *      Note: You must then link using the /NOE option, because
 *      the VAC++ runtimes also contain a _DLL_Initterm, and the
 *      linker gets in trouble otherwise. The XWorkplace makefile
 *      takes care of this.
 *
 *      This function must return 0 upon errors or 1 otherwise.
 *
 *@@changed V0.9.0 [umoeller]: reworked locale initialization
 *@@changed V0.9.0 [umoeller]: moved this func here from module.c
 */

unsigned long _System _DLL_InitTerm(unsigned long hModule,
                                    unsigned long ulFlag)
{
    APIRET rc;

    switch (ulFlag)
    {
        case 0:
        {
            // DLL being loaded:
            // CHAR    szTemp[400];
            // PSZ     p = NULL;
            // ULONG   aulCPList[8] = {0},
                    // cbCPList = sizeof(aulCPList);
                    // ulListSize = 0;

            // store the DLL handle in the global variable so that
            // cmnQueryMainModuleHandle() below can return it
            G_hmodDLL = hModule;

            // now initialize the C run-time environment before we
            // call any runtime functions
            if (_CRT_init() == -1)
               return (0);  // error

            if (rc = DosQueryModuleName(hModule, CCHMAXPATH, G_szDLLFile))
                DosBeep(100, 100);
        break; }

        case 1:
            // DLL being freed: cleanup runtime
            _CRT_term();
            break;

        default:
            // other code: beep for error
            DosBeep(100, 100);
            return (0);     // error
    }

    // a non-zero value must be returned to indicate success
    return (1);
}

/*
 *@@ cmnQueryMainCodeModuleHandle:
 *      this may be used to retrieve the module handle
 *      of XFLDR.DLL, which was stored by _DLL_InitTerm.
 *
 *      Note that this returns the _main_ module handle
 *      (XFLDR.DLL). There are two more query-module
 *      functions:
 *
 *      -- To get the NLS module handle (for dialogs etc.),
 *         use cmnQueryNLSModuleHandle.
 *
 *      -- To get the main resource module handle (for icons
 *         etc.), use cmnQueryMainResModuleHandle.
 *
 *@@changed V0.9.0 [umoeller]: moved this func here from module.c
 *@@changed V0.9.7 (2000-12-13) [umoeller]: renamed from cmnQueryMainModuleHandle
 */

HMODULE cmnQueryMainCodeModuleHandle(VOID)
{
    return (G_hmodDLL);
}

/*
 *@@ cmnQueryMainModuleFilename:
 *      this may be used to retrieve the fully
 *      qualified file name of the DLL
 *      which was stored by _DLL_InitTerm.
 *
 *@@changed V0.9.0 [umoeller]: moved this func here from module.c
 */

const char* cmnQueryMainModuleFilename(VOID)
{
    return (G_szDLLFile);
}

/*
 *@@ cmnQueryMainResModuleHandle:
 *      this may be used to retrieve the module handle
 *      of XWPRES.DLL, which contains resources that
 *      are independent of language (icons, bitmaps etc.).
 *
 *      This loads the DLL on the first call.
 *
 *      This has been added with V0.9.7 to separate the
 *      resources out of the main module handle to speed
 *      up link time, which became annoyingly slow with
 *      all the resources.
 *
 *@@added V0.9.7 (2000-12-13) [umoeller]
 */

HMODULE cmnQueryMainResModuleHandle(VOID)
{
    BOOL fLocked = FALSE;
    ULONG ulNesting;
    DosEnterMustComplete(&ulNesting);

    TRY_LOUD(excpt1)
    {
        fLocked = krnLock(__FILE__, __LINE__, __FUNCTION__);
        if (fLocked)
        {
            if (G_hmodRes == NULLHANDLE)
            {
                // not loaded yet:
                CHAR    szError[100],
                        szResModule[CCHMAXPATH];

                if (cmnQueryXWPBasePath(szResModule))
                {
                    APIRET arc = NO_ERROR;
                    strcat(szResModule, "\\bin\\xwpres.dll");
                    arc = DosLoadModule(szError,
                                        sizeof(szError),
                                        szResModule,
                                        &G_hmodRes);
                    if (arc != NO_ERROR)
                        cmnLog(__FILE__, __LINE__, __FUNCTION__,
                               "Error %d occured loading \"%s\".",
                               arc, szResModule);
                }
            }
        }
    }
    CATCH(excpt1) { } END_CATCH();

    if (fLocked)
        krnUnlock();

    DosExitMustComplete(&ulNesting);

    return (G_hmodRes);
}

/* ******************************************************************
 *
 *   Error logging
 *
 ********************************************************************/

/*
 *@@ cmnLog:
 *      logs a message to the XWorkplace log file
 *      in the root directory of the boot drive.
 *
 *@@added V0.9.2 (2000-03-06) [umoeller]
 */

VOID cmnLog(const char *pcszSourceFile, // in: source file name
            ULONG ulLine,               // in: source line
            const char *pcszFunction,   // in: function name
            const char *pcszFormat,     // in: format string (like with printf)
            ...)                        // in: additional stuff (like with printf)
{
    va_list     args;
    CHAR        szLogFileName[100];
    FILE        *fileLog = 0;

    DosBeep(100, 50);

    sprintf(szLogFileName,
            "%c:\\%s",
            doshQueryBootDrive(),
            XFOLDER_LOGLOG);
    fileLog = fopen(szLogFileName, "a");  // text file, append
    if (fileLog)
    {
        DATETIME DT;
        DosGetDateTime(&DT);
        fprintf(fileLog,
                "%04d-%02d-%02d %02d:%02d:%02d "
                "%s (%s, line %d):\n    ",
                DT.year, DT.month, DT.day,
                DT.hours, DT.minutes, DT.seconds,
                pcszFunction, pcszSourceFile, ulLine);
        va_start(args, pcszFormat);
        vfprintf(fileLog, pcszFormat, args);
        va_end(args);
        fprintf(fileLog, "\n");
        fclose (fileLog);
    }
}

/* ******************************************************************
 *
 *   XWorkplace National Language Support (NLS)
 *
 ********************************************************************/

/*
 *  The following routines are for querying the XFolder
 *  installation path and similiar routines, such as
 *  querying the current NLS module, changing it, loading
 *  strings, the help file and all that sort of stuff.
 */

/*
 *@@ cmnQueryXWPBasePath:
 *      this routine returns the path of where XFolder was installed,
 *      i.e. the parent directory of where the xfldr.dll file
 *      resides, without a trailing backslash (e.g. "C:\XFolder").
 *
 *      The buffer to copy this to is assumed to be CCHMAXPATH in size.
 *
 *      As opposed to versions before V0.81, OS2.INI is no longer
 *      needed for this to work. The path is retrieved from the
 *      DLL directly by evaluating what was passed to _DLL_InitTerm.
 *
 *@@changed V0.9.7 (2000-12-02) [umoeller]: renamed from cmnQueryXFolderBasePath
 */

BOOL cmnQueryXWPBasePath(PSZ pszPath)
{
    BOOL brc = FALSE;
    const char *pszDLL = cmnQueryMainModuleFilename();
    if (pszDLL)
    {
        // copy until last backslash minus four characters
        // (leave out "\bin\xfldr.dll")
        PSZ pszLastSlash = strrchr(pszDLL, '\\');
        #ifdef DEBUG_LANGCODES
            _Pmpf(( "cmnQueryMainModuleFilename: %s", pszDLL));
        #endif
        strncpy(pszPath, pszDLL, (pszLastSlash-pszDLL)-4);
        pszPath[(pszLastSlash-pszDLL-4)] = '\0';
        brc = TRUE;
    }
    #ifdef DEBUG_LANGCODES
        _Pmpf(( "cmnQueryXWPBasePath: %s", pszPath ));
    #endif
    return (brc);
}

/*
 *@@ cmnQueryLanguageCode:
 *      returns PSZ to three-digit language code (e.g. "001").
 *      This points to a global variable, so do NOT change.
 *
 *@@changed V0.9.0 (99-11-14) [umoeller]: made this reentrant, finally
 */

const char* cmnQueryLanguageCode(VOID)
{
    BOOL fLocked = FALSE;
    ULONG ulNesting;
    DosEnterMustComplete(&ulNesting);

    TRY_LOUD(excpt1)
    {
        fLocked = krnLock(__FILE__, __LINE__, __FUNCTION__);
        if (fLocked)
        {
            if (G_szLanguageCode[0] == '\0')
                PrfQueryProfileString(HINI_USERPROFILE,
                                      (PSZ)INIAPP_XWORKPLACE,
                                      (PSZ)INIKEY_LANGUAGECODE,
                                      (PSZ)DEFAULT_LANGUAGECODE,
                                      (PVOID)G_szLanguageCode,
                                      sizeof(G_szLanguageCode));

            G_szLanguageCode[3] = '\0';
            #ifdef DEBUG_LANGCODES
                _Pmpf(( "cmnQueryLanguageCode: %s", szLanguageCode ));
            #endif
        }
    }
    CATCH(excpt1) { } END_CATCH();

    if (fLocked)
        krnUnlock();

    DosExitMustComplete(&ulNesting);

    return (G_szLanguageCode);
}

/*
 *@@ cmnSetLanguageCode:
 *      changes XFolder's language to three-digit language code in
 *      pszLanguage (e.g. "001"). This does not reload the NLS DLL,
 *      but only change the setting.
 *
 *@@changed V0.9.0 (99-11-14) [umoeller]: made this reentrant, finally
 */

BOOL cmnSetLanguageCode(PSZ pszLanguage)
{
    BOOL brc = FALSE;

    BOOL fLocked = FALSE;
    ULONG ulNesting;
    DosEnterMustComplete(&ulNesting);

    TRY_LOUD(excpt1)
    {
        fLocked = krnLock(__FILE__, __LINE__, __FUNCTION__);
        if (fLocked)
        {
            strcpy(G_szLanguageCode, pszLanguage);
            G_szLanguageCode[3] = 0;

            brc = PrfWriteProfileString(HINI_USERPROFILE,
                                        (PSZ)INIAPP_XWORKPLACE,
                                        (PSZ)INIKEY_LANGUAGECODE,
                                        G_szLanguageCode);
        }
    }
    CATCH(excpt1) { } END_CATCH();

    if (fLocked)
        krnUnlock();

    DosExitMustComplete(&ulNesting);

    return (brc);
}

/*
 *@@ cmnQueryHelpLibrary:
 *      returns PSZ to full help library path in XFolder directory,
 *      depending on where XFolder was installed and on the current
 *      language (e.g. "C:\XFolder\help\xfldr001.hlp").
 *
 *      This PSZ points to a global variable, so you better not
 *      change it.
 *
 *@@changed V0.9.0 (99-11-14) [umoeller]: made this reentrant, finally
 */

const char* cmnQueryHelpLibrary(VOID)
{
    const char *rc = 0;

    BOOL fLocked = FALSE;
    ULONG ulNesting;
    DosEnterMustComplete(&ulNesting);

    TRY_LOUD(excpt1)
    {
        fLocked = krnLock(__FILE__, __LINE__, __FUNCTION__);
        if (fLocked)
        {
            if (cmnQueryXWPBasePath(G_szHelpLibrary))
            {
                // path found: append helpfile
                sprintf(G_szHelpLibrary + strlen(G_szHelpLibrary),
                        "\\help\\xfldr%s.hlp",
                        cmnQueryLanguageCode());
                #ifdef DEBUG_LANGCODES
                    _Pmpf(( "cmnQueryHelpLibrary: %s", szHelpLibrary ));
                #endif
                rc = G_szHelpLibrary;
            }
        }
    }
    CATCH(excpt1) { } END_CATCH();

    if (fLocked)
        krnUnlock();

    DosExitMustComplete(&ulNesting);

    return (rc);
}

/*
 *@@ cmnDisplayHelp:
 *      displays an XWorkplace help panel,
 *      using wpDisplayHelp.
 *      If somSelf == NULL, we'll query the
 *      active desktop.
 */

BOOL cmnDisplayHelp(WPObject *somSelf,
                    ULONG ulPanelID)
{
    BOOL brc = FALSE;
    if (somSelf == NULL)
        somSelf = cmnQueryActiveDesktop();

    if (somSelf)
    {
        brc = _wpDisplayHelp(somSelf,
                             ulPanelID,
                             (PSZ)cmnQueryHelpLibrary());
        if (!brc)
            // complain
            cmnMessageBoxMsg(HWND_DESKTOP, 104, 134, MB_OK);
    }
    return (brc);
}

/*
 *@@ cmnQueryMessageFile:
 *      returns PSZ to full message file path in XFolder directory,
 *      depending on where XFolder was installed and on the current
 *      language (e.g. "C:\XFolder\help\xfldr001.tmf").
 *
 *@@changed V0.9.0 [umoeller]: changed, this now returns the TMF file (tmsgfile.c).
 *@@changed V0.9.0 (99-11-14) [umoeller]: made this reentrant, finally
 */

const char* cmnQueryMessageFile(VOID)
{
    const char *rc = 0;

    BOOL fLocked = FALSE;
    ULONG ulNesting;
    DosEnterMustComplete(&ulNesting);

    TRY_LOUD(excpt1)
    {
        fLocked = krnLock(__FILE__, __LINE__, __FUNCTION__);
        if (fLocked)
        {
            if (cmnQueryXWPBasePath(G_szMessageFile))
            {
                // path found: append message file
                sprintf(G_szMessageFile + strlen(G_szMessageFile),
                        "\\help\\xfldr%s.tmf",
                        cmnQueryLanguageCode());
                #ifdef DEBUG_LANGCODES
                    _Pmpf(( "cmnQueryMessageFile: %s", szMessageFile));
                #endif
                rc = G_szMessageFile;
            }
        }
    }
    CATCH(excpt1) { } END_CATCH();

    if (fLocked)
        krnUnlock();

    DosExitMustComplete(&ulNesting);

    return (rc);
}

/*
 *@@ cmnQueryIconsDLL:
 *      this returns the HMODULE of XFolder ICONS.DLL
 *      (new with V0.84).
 *
 *      If this is queried for the first time, the DLL
 *      is loaded from the /BIN directory.
 *
 *      In this case, this routine also checks if
 *      ICONS.DLL exists in the /ICONS directory. If
 *      so, it is copied to /BIN before loading the
 *      DLL. This allows for replacing the DLL using
 *      the REXX script in /ICONS.
 *
 *@@changed V0.9.0 (99-11-14) [umoeller]: made this reentrant, finally
 */

HMODULE cmnQueryIconsDLL(VOID)
{
    BOOL fLocked = FALSE;
    ULONG ulNesting;
    DosEnterMustComplete(&ulNesting);

    TRY_LOUD(excpt1)
    {
        fLocked = krnLock(__FILE__, __LINE__, __FUNCTION__);
        if (fLocked)
        {
            // first query?
            if (G_hmodIconsDLL == NULLHANDLE)
            {
                CHAR    szIconsDLL[CCHMAXPATH],
                        szNewIconsDLL[CCHMAXPATH];
                cmnQueryXWPBasePath(szIconsDLL);
                strcpy(szNewIconsDLL, szIconsDLL);

                sprintf(szIconsDLL+strlen(szIconsDLL),
                        "\\bin\\icons.dll");
                sprintf(szNewIconsDLL+strlen(szNewIconsDLL),
                        "\\icons\\icons.dll");

                #ifdef DEBUG_LANGCODES
                    _Pmpf(("cmnQueryIconsDLL: checking %s", szNewIconsDLL));
                #endif
                // first check for /ICONS/ICONS.DLL
                if (access(szNewIconsDLL, 0) == 0)
                {
                    #ifdef DEBUG_LANGCODES
                        _Pmpf(("    found, copying to %s", szIconsDLL));
                    #endif
                    // exists: move to /BIN
                    // and use that one
                    DosDelete(szIconsDLL);
                    DosMove(szNewIconsDLL,      // old
                            szIconsDLL);        // new
                    DosDelete(szNewIconsDLL);
                }

                #ifdef DEBUG_LANGCODES
                    _Pmpf(("cmnQueryIconsDLL: loading %s", szIconsDLL));
                #endif
                // now load /BIN/ICONS.DLL
                if (DosLoadModule(NULL,
                                  0,
                                  szIconsDLL,
                                  &G_hmodIconsDLL)
                        != NO_ERROR)
                    G_hmodIconsDLL = NULLHANDLE;
            }

            #ifdef DEBUG_LANGCODES
                _Pmpf(("cmnQueryIconsDLL: returning %lX", hmodIconsDLL));
            #endif

        }
    }
    CATCH(excpt1) { } END_CATCH();

    if (fLocked)
        krnUnlock();

    DosExitMustComplete(&ulNesting);

    return (G_hmodIconsDLL);
}

/*
 *@@ cmnQueryBootLogoFile:
 *      this returns the boot logo file as stored
 *      in OS2.INI. If it is not stored there,
 *      we return the default xfolder.bmp in
 *      the XFolder installation directories.
 *
 *      The return value of this function must
 *      be free()'d after use.
 *
 *@@added V0.9.0 [umoeller]
 *@@changed V0.9.0 (99-11-14) [umoeller]: made this reentrant, finally
 */

PSZ cmnQueryBootLogoFile(VOID)
{
    PSZ pszReturn = 0;

    BOOL fLocked = FALSE;
    ULONG ulNesting;
    DosEnterMustComplete(&ulNesting);

    TRY_LOUD(excpt1)
    {
        fLocked = krnLock(__FILE__, __LINE__, __FUNCTION__);
        if (fLocked)
        {
            pszReturn = prfhQueryProfileData(HINI_USER,
                                             INIAPP_XWORKPLACE,
                                             INIKEY_BOOTLOGOFILE,
                                             NULL);
            if (!pszReturn)
            {
                CHAR szBootLogoFile[CCHMAXPATH];
                // INI data not found: return default file
                cmnQueryXWPBasePath(szBootLogoFile);
                strcat(szBootLogoFile,
                        "\\bootlogo\\xfolder.bmp");
                pszReturn = strdup(szBootLogoFile);
            }
        }
    }
    CATCH(excpt1) { } END_CATCH();

    if (fLocked)
        krnUnlock();

    DosExitMustComplete(&ulNesting);

    return (pszReturn);
}

/*
 *@@ cmnLoadString:
 *      pretty similar to WinLoadString, but allocates
 *      necessary memory as well. *ppsz is a pointer
 *      to a PSZ; if this PSZ is != NULL, whatever it
 *      points to will be free()d, so you should set this
 *      to NULL if you initially call this function.
 *      This is used at WPS startup and when XFolder's
 *      language is changed later to load all the strings
 *      from a NLS DLL (cmnQueryNLSModuleHandle).
 *
 *@@changed V0.9.0 [umoeller]: "string not found" is now re-allocated using strdup (avoids crashes)
 *@@changed V0.9.0 (99-11-28) [umoeller]: added more meaningful error message
 *@@changed V0.9.2 (2000-02-26) [umoeller]: made temporary buffer larger
 */

void cmnLoadString(HAB habDesktop,
                   HMODULE hmodResource,
                   ULONG ulID,
                   PSZ *ppsz)
{
    CHAR    szBuf[500] = "";
    if (*ppsz)
        free(*ppsz);
    if (WinLoadString(habDesktop, hmodResource, ulID , sizeof(szBuf), szBuf))
        *ppsz = strdup(szBuf);
    else
    {
        sprintf(szBuf, "cmnLoadString error: string resource %d not found in module 0x%lX",
                       ulID, hmodResource);
        *ppsz = strdup(szBuf); // V0.9.0
    }
}

/*
 *@@ LoadNLSData:
 *      called from cmnQueryNLSModuleHandle when
 *      the NLS data needs to be (re)initialized.
 *
 *      Preconditions: This must be called in a
 *                     krnLock() block.
 *
 *@@added V0.9.2 (2000-02-23) [umoeller]
 */

VOID LoadNLSData(HAB habDesktop,
                 NLSSTRINGS *pNLSStrings)
{
    // now let's load strings
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_NOTDEFINED,
            &(pNLSStrings->pszNotDefined));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_PRODUCTINFO,
            &(pNLSStrings->pszProductInfo));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_REFRESHNOW,
            &(pNLSStrings->pszRefreshNow));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_SNAPTOGRID,
            &(pNLSStrings->pszSnapToGrid));

    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_FLDRCONTENT,
            &(pNLSStrings->pszFldrContent));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_COPYFILENAME,
            &(pNLSStrings->pszCopyFilename));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_BORED,
            &(pNLSStrings->pszBored));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_FLDREMPTY,
            &(pNLSStrings->pszFldrEmpty));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_SELECTSOME,
            &(pNLSStrings->pszSelectSome));

    cmnLoadString(habDesktop, G_hmodNLS, ID_XFSI_QUICKSTATUS,
            &(pNLSStrings->pszQuickStatus));

    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_SV_NAME,
            &(pNLSStrings->pszSortByName));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_SV_TYPE,
            &(pNLSStrings->pszSortByType));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_SV_CLASS,
            &(pNLSStrings->pszSortByClass));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_SV_REALNAME,
            &(pNLSStrings->pszSortByRealName));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_SV_SIZE,
            &(pNLSStrings->pszSortBySize));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_SV_WRITEDATE,
            &(pNLSStrings->pszSortByWriteDate));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_SV_ACCESSDATE,
            &(pNLSStrings->pszSortByAccessDate));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_SV_CREATIONDATE,
            &(pNLSStrings->pszSortByCreationDate));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_SV_EXT,
            &(pNLSStrings->pszSortByExt));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_SV_FOLDERSFIRST,
            &(pNLSStrings->pszSortFoldersFirst));

    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_KEY_CTRL,
            &(pNLSStrings->pszCtrl));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_KEY_Alt,
            &(pNLSStrings->pszAlt));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_KEY_SHIFT,
            &(pNLSStrings->pszShift));

    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_KEY_BACKSPACE,
            &(pNLSStrings->pszBackspace));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_KEY_TAB,
            &(pNLSStrings->pszTab));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_KEY_BACKTABTAB,
            &(pNLSStrings->pszBacktab));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_KEY_ENTER,
            &(pNLSStrings->pszEnter));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_KEY_ESC,
            &(pNLSStrings->pszEsc));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_KEY_SPACE,
            &(pNLSStrings->pszSpace));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_KEY_PAGEUP,
            &(pNLSStrings->pszPageup));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_KEY_PAGEDOWN,
            &(pNLSStrings->pszPagedown));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_KEY_END,
            &(pNLSStrings->pszEnd));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_KEY_HOME,
            &(pNLSStrings->pszHome));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_KEY_LEFT,
            &(pNLSStrings->pszLeft));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_KEY_UP,
            &(pNLSStrings->pszUp));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_KEY_RIGHT,
            &(pNLSStrings->pszRight));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_KEY_DOWN,
            &(pNLSStrings->pszDown));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_KEY_PRINTSCRN,
            &(pNLSStrings->pszPrintscrn));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_KEY_INSERT,
            &(pNLSStrings->pszInsert));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_KEY_DELETE,
            &(pNLSStrings->pszDelete));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_KEY_SCRLLOCK,
            &(pNLSStrings->pszScrlLock));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_KEY_NUMLOCK,
            &(pNLSStrings->pszNumLock));

    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_KEY_WINLEFT,
            &(pNLSStrings->pszWinLeft));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_KEY_WINRIGHT,
            &(pNLSStrings->pszWinRight));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_KEY_WINMENU,
            &(pNLSStrings->pszWinMenu));

    cmnLoadString(habDesktop, G_hmodNLS, ID_SDSI_FLUSHING,
            &(pNLSStrings->pszSDFlushing));
    cmnLoadString(habDesktop, G_hmodNLS, ID_SDSI_CAD,
            &(pNLSStrings->pszSDCAD));
    cmnLoadString(habDesktop, G_hmodNLS, ID_SDSI_REBOOTING,
            &(pNLSStrings->pszSDRebooting));
    cmnLoadString(habDesktop, G_hmodNLS, ID_SDSI_CLOSING,
            &(pNLSStrings->pszSDClosing));
    cmnLoadString(habDesktop, G_hmodNLS, ID_SDSI_SHUTDOWN,
            &(pNLSStrings->pszShutdown));
    cmnLoadString(habDesktop, G_hmodNLS, ID_SDSI_RESTARTWPS,
            &(pNLSStrings->pszRestartWPS));
    cmnLoadString(habDesktop, G_hmodNLS, ID_SDSI_RESTARTINGWPS,
            &(pNLSStrings->pszSDRestartingWPS));
    cmnLoadString(habDesktop, G_hmodNLS, ID_SDSI_SAVINGDESKTOP,
            &(pNLSStrings->pszSDSavingDesktop));
    cmnLoadString(habDesktop, G_hmodNLS, ID_SDSI_SAVINGPROFILES,
            &(pNLSStrings->pszSDSavingProfiles));

    cmnLoadString(habDesktop, G_hmodNLS, ID_SDSI_STARTING,
            &(pNLSStrings->pszStarting));

    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_POPULATING,
            &(pNLSStrings->pszPopulating));

    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_1GENERIC,
            &(pNLSStrings->psz1Generic));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_2REMOVEITEMS,
            &(pNLSStrings->psz2RemoveItems));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_25ADDITEMS,
            &(pNLSStrings->psz25AddItems));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_26CONFIGITEMS,
            &(pNLSStrings->psz26ConfigFolderMenus));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_27STATUSBAR,
            &(pNLSStrings->psz27StatusBar));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_3SNAPTOGRID,
            &(pNLSStrings->psz3SnapToGrid));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_4ACCELERATORS,
            &(pNLSStrings->psz4Accelerators));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_5INTERNALS,
            &(pNLSStrings->psz5Internals));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_FILEOPS,
            &(pNLSStrings->pszFileOps));

    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_SORT,
            &(pNLSStrings->pszSort));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_SV_ALWAYSSORT,
            &(pNLSStrings->pszAlwaysSort));

    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_INTERNALS,
            &(pNLSStrings->pszInternals));

    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_XWPSTATUS,
            &(pNLSStrings->pszXWPStatus));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_FEATURES,
            &(pNLSStrings->pszFeatures));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_PARANOIA,
            &(pNLSStrings->pszParanoia));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_OBJECTS,
            &(pNLSStrings->pszObjects));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_FILEPAGE,
            &(pNLSStrings->pszFilePage));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_DETAILSPAGE,
            &(pNLSStrings->pszDetailsPage));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_XSHUTDOWNPAGE,
            &(pNLSStrings->pszXShutdownPage));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_STARTUPPAGE,
            &(pNLSStrings->pszStartupPage));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_DTPMENUPAGE,
            &(pNLSStrings->pszDtpMenuPage));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_FILETYPESPAGE,
            &(pNLSStrings->pszFileTypesPage));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_SOUNDSPAGE,
            &(pNLSStrings->pszSoundsPage));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_VIEWPAGE,
            &(pNLSStrings->pszViewPage));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_ARCHIVESPAGE,
            &(pNLSStrings->pszArchivesPage));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_PGMFILE_MODULE,
            &(pNLSStrings->pszModulePage));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_OBJECTHOTKEYSPAGE,
            &(pNLSStrings->pszObjectHotkeysPage));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_FUNCTIONKEYSPAGE,
            &(pNLSStrings->pszFunctionKeysPage));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_MOUSEHOOKPAGE,
            &(pNLSStrings->pszMouseHookPage));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_MAPPINGSPAGE,
            &(pNLSStrings->pszMappingsPage));

    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_SB_CLASSMNEMONICS,
            &(pNLSStrings->pszSBClassMnemonics));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_SB_CLASSNOTSUPPORTED,
            &(pNLSStrings->pszSBClassNotSupported));

    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_WPSCLASSES,
            &(pNLSStrings->pszWpsClasses));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_WPSCLASSLOADED,
            &(pNLSStrings->pszWpsClassLoaded));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_WPSCLASSLOADINGFAILED,
            &(pNLSStrings->pszWpsClassLoadingFailed));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_WPSCLASSREPLACEDBY,
            &(pNLSStrings->pszWpsClassReplacedBy));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_WPSCLASSORPHANS,
            &(pNLSStrings->pszWpsClassOrphans));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_WPSCLASSORPHANSINFO,
            &(pNLSStrings->pszWpsClassOrphansInfo));

    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_SCHEDULER,
            &(pNLSStrings->pszScheduler));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_MEMORY,
            &(pNLSStrings->pszMemory));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_ERRORS,
            &(pNLSStrings->pszErrors));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_WPS,
            &(pNLSStrings->pszWPS));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_SYSPATHS,
            &(pNLSStrings->pszSysPaths));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_DRIVERS,
            &(pNLSStrings->pszDrivers));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_DRIVERCATEGORIES,
            &(pNLSStrings->pszDriverCategories));

    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_PROCESSCONTENT,
            &(pNLSStrings->pszProcessContent));

    cmnLoadString(habDesktop, G_hmodNLS, ID_XFSI_SETTINGS,
            &(pNLSStrings->pszSettings));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XFSI_SETTINGSNOTEBOOK,
            &(pNLSStrings->pszSettingsNotebook));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XFSI_ATTRIBUTES,
            &(pNLSStrings->pszAttributes));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XFSI_ATTR_ARCHIVE,
            &(pNLSStrings->pszAttrArchive));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XFSI_ATTR_SYSTEM,
            &(pNLSStrings->pszAttrSystem));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XFSI_ATTR_HIDDEN,
            &(pNLSStrings->pszAttrHidden));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XFSI_ATTR_READONLY,
            &(pNLSStrings->pszAttrReadOnly));

    cmnLoadString(habDesktop, G_hmodNLS, ID_XFSI_FLDRSETTINGS,
            &(pNLSStrings->pszWarp3FldrView));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XFSI_SMALLICONS,
            &(pNLSStrings->pszSmallIcons  ));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XFSI_FLOWED,
            &(pNLSStrings->pszFlowed));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XFSI_NONFLOWED,
            &(pNLSStrings->pszNonFlowed));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XFSI_NOGRID,
            &(pNLSStrings->pszNoGrid));

    // the following are new with V0.9.0
    cmnLoadString(habDesktop, G_hmodNLS, ID_XFSI_WARP4MENUBAR,
            &(pNLSStrings->pszWarp4MenuBar));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XFSI_SHOWSTATUSBAR,
            &(pNLSStrings->pszShowStatusBar));

    // "WPS Class List" (XWPClassList, new with V0.9.0)
    cmnLoadString(habDesktop, G_hmodNLS, ID_XFSI_OPENCLASSLIST,
            &(pNLSStrings->pszOpenClassList));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XFSI_XWPCLASSLIST,
            &(pNLSStrings->pszXWPClassList));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XFSI_REGISTERCLASS,
            &(pNLSStrings->pszRegisterClass));

    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_SOUNDSCHEMENONE,
            &(pNLSStrings->pszSoundSchemeNone));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_ITEMSSELECTED,
            &(pNLSStrings->pszItemsSelected));

    // Trash can (XWPTrashCan, XWPTrashObject, new with V0.9.0)
    cmnLoadString(habDesktop, G_hmodNLS, ID_XTSI_TRASHEMPTY,
            &(pNLSStrings->pszTrashEmpty));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XTSI_TRASHRESTORE,
            &(pNLSStrings->pszTrashRestore));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XTSI_TRASHDESTROY,
            &(pNLSStrings->pszTrashDestroy));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XTSI_TRASHCAN,
            &(pNLSStrings->pszTrashCan));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XTSI_TRASHOBJECT,
            &(pNLSStrings->pszTrashObject));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XTSI_TRASHSETTINGSPAGE,
            &(pNLSStrings->pszTrashSettingsPage));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XTSI_TRASHDRIVESPAGE,
            &(pNLSStrings->pszTrashDrivesPage));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XTSI_ORIGFOLDER,
            &(pNLSStrings->pszOrigFolder));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XTSI_DELDATE,
            &(pNLSStrings->pszDelDate));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XTSI_DELTIME,
            &(pNLSStrings->pszDelTime));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XTSI_SIZE,
            &(pNLSStrings->pszSize));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XTSI_ORIGCLASS,
            &(pNLSStrings->pszOrigClass));

    // trash can status bar strings; V0.9.1 (2000-02-04) [umoeller]
    cmnLoadString(habDesktop, G_hmodNLS, ID_XTSI_STB_POPULATING,
            &(pNLSStrings->pszStbPopulating));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XTSI_STB_OBJCOUNT,
            &(pNLSStrings->pszStbObjCount));

    // Details view columns on XWPKeyboard "Hotkeys" page; V0.9.1 (99-12-03)
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_HOTKEY_TITLE,
            &(pNLSStrings->pszHotkeyTitle));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_HOTKEY_FOLDER,
            &(pNLSStrings->pszHotkeyFolder));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_HOTKEY_HANDLE,
            &(pNLSStrings->pszHotkeyHandle));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_HOTKEY_HOTKEY,
            &(pNLSStrings->pszHotkeyHotkey));

    // Method info columns for XWPClassList; V0.9.1 (99-12-03)
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_CLSLIST_INDEX,
            &(pNLSStrings->pszClsListIndex));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_CLSLIST_METHOD,
            &(pNLSStrings->pszClsListMethod));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_CLSLIST_ADDRESS,
            &(pNLSStrings->pszClsListAddress));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_CLSLIST_CLASS,
            &(pNLSStrings->pszClsListClass));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_CLSLIST_OVERRIDDENBY,
            &(pNLSStrings->pszClsListOverriddenBy));

                // "Special functions" on XWPMouse "Movement" page
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_SPECIAL_WINDOWLIST,
            &(pNLSStrings->pszSpecialWindowList));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_SPECIAL_DESKTOPPOPUP,
            &(pNLSStrings->pszSpecialDesktopPopup));

    // default title of XWPScreen class V0.9.2 (2000-02-23) [umoeller]
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_XWPSCREENTITLE,
            &(pNLSStrings->pszXWPScreenTitle));

    // "Partitions" item in WPDrives "open" menu V0.9.2 (2000-02-29) [umoeller]
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_OPENPARTITIONS,
            &(pNLSStrings->pszOpenPartitions));

    // "Syslevel" page title in "OS/2 kernel" V0.9.3 (2000-04-01) [umoeller]
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_SYSLEVELPAGE,
            &(pNLSStrings->pszSyslevelPage));

    cmnLoadString(habDesktop, G_hmodNLS, ID_XTSI_CALCULATING,
            &(pNLSStrings->pszCalculating));

    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_DEVICETYPE,
            &(pNLSStrings->pszDeviceType));
    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_DEVICEINDEX,
            &(pNLSStrings->pszDeviceIndex));
    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_DEVICEINFO,
            &(pNLSStrings->pszDeviceInfo));

    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_TYPE_IMAGE,
            &(pNLSStrings->pszTypeImage));
    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_TYPE_AUDIO,
            &(pNLSStrings->pszTypeAudio));
    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_TYPE_MIDI,
            &(pNLSStrings->pszTypeMIDI));
    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_TYPE_COMPOUND,
            &(pNLSStrings->pszTypeCompound));
    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_TYPE_OTHER,
            &(pNLSStrings->pszTypeOther));
    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_TYPE_UNKNOWN,
            &(pNLSStrings->pszTypeUnknown));
    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_TYPE_VIDEO,
            &(pNLSStrings->pszTypeVideo));
    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_TYPE_ANIMATION,
            &(pNLSStrings->pszTypeAnimation));
    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_TYPE_MOVIE,
            &(pNLSStrings->pszTypeMovie));

    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_TYPE_STORAGE,
            &(pNLSStrings->pszTypeStorage));
    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_TYPE_FILE,
            &(pNLSStrings->pszTypeFile));
    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_TYPE_DATA,
            &(pNLSStrings->pszTypeData));

    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_DEVTYPE_VIDEOTAPE,
            &(pNLSStrings->pszDevTypeVideotape));
    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_DEVTYPE_VIDEODISC,
            &(pNLSStrings->pszDevTypeVideodisc));
    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_DEVTYPE_CD_AUDIO,
            &(pNLSStrings->pszDevTypeCDAudio));
    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_DEVTYPE_DAT,
            &(pNLSStrings->pszDevTypeDAT));
    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_DEVTYPE_AUDIO_TAPE,
            &(pNLSStrings->pszDevTypeAudioTape));
    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_DEVTYPE_OTHER,
            &(pNLSStrings->pszDevTypeOther));
    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_DEVTYPE_WAVEFORM_AUDIO,
            &(pNLSStrings->pszDevTypeWaveformAudio));
    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_DEVTYPE_SEQUENCER,
            &(pNLSStrings->pszDevTypeSequencer));
    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_DEVTYPE_AUDIO_AMPMIX,
            &(pNLSStrings->pszDevTypeAudioAmpmix));
    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_DEVTYPE_OVERLAY,
            &(pNLSStrings->pszDevTypeOverlay));
    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_DEVTYPE_ANIMATION,
            &(pNLSStrings->pszDevTypeAnimation));
    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_DEVTYPE_DIGITAL_VIDEO,
            &(pNLSStrings->pszDevTypeDigitalVideo));
    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_DEVTYPE_SPEAKER,
            &(pNLSStrings->pszDevTypeSpeaker));
    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_DEVTYPE_HEADPHONE,
            &(pNLSStrings->pszDevTypeHeadphone));
    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_DEVTYPE_MICROPHONE,
            &(pNLSStrings->pszDevTypeMicrophone));
    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_DEVTYPE_MONITOR,
            &(pNLSStrings->pszDevTypeMonitor));
    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_DEVTYPE_CDXA,
            &(pNLSStrings->pszDevTypeCDXA));
    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_DEVTYPE_FILTER,
            &(pNLSStrings->pszDevTypeFilter));
    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_DEVTYPE_TTS,
            &(pNLSStrings->pszDevTypeTTS));

    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_COLMN_FOURCC,
            &(pNLSStrings->pszColmnFourCC));
    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_COLMN_NAME,
            &(pNLSStrings->pszColmnName));
    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_COLMN_IOPROC_TYPE,
            &(pNLSStrings->pszColmnIOProcType));
    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_COLMN_MEDIA_TYPE,
            &(pNLSStrings->pszColmnMediaType));
    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_COLMN_EXTENSION,
            &(pNLSStrings->pszColmnExtension));
    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_COLMN_DLL,
            &(pNLSStrings->pszColmnDLL));
    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_COLMN_PROCEDURE,
            &(pNLSStrings->pszColmnProcedure));

    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_PAGETITLE_DEVICES,
            &(pNLSStrings->pszPagetitleDevices));
    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_PAGETITLE_IOPROCS,
            &(pNLSStrings->pszPagetitleIOProcs));
    cmnLoadString(habDesktop, G_hmodNLS, ID_MMSI_PAGETITLE_CODECS,
            &(pNLSStrings->pszPagetitleCodecs));

    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_PAGETITLE_PAGEMAGE,
            &(pNLSStrings->pszPagetitlePageMage));

    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_XWPSTRING_PAGE,
            &(pNLSStrings->pszXWPStringPage));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_XWPSTRING_OPENMENU,
            &(pNLSStrings->pszXWPStringOpenMenu));

    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_COLMN_SYSL_COMPONENT,
            &(pNLSStrings->pszSyslevelComponent));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_COLMN_SYSL_FILE,
            &(pNLSStrings->pszSyslevelFile));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_COLMN_SYSL_VERSION,
            &(pNLSStrings->pszSyslevelVersion));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_COLMN_SYSL_LEVEL,
            &(pNLSStrings->pszSyslevelLevel));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_COLMN_SYSL_PREVIOUS,
            &(pNLSStrings->pszSyslevelPrevious));

    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_DRIVERS_VERSION,
            &(pNLSStrings->pszDriversVersion));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_DRIVERS_VENDOR,
            &(pNLSStrings->pszDriversVendor));

    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_FUNCKEY_DESCRIPTION,
            &(pNLSStrings->pszFuncKeyDescription));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_FUNCKEY_SCANCODE,
            &(pNLSStrings->pszFuncKeyScanCode));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_FUNCKEY_MODIFIER,
            &(pNLSStrings->pszFuncKeyModifier));

    // default documents V0.9.4 (2000-06-09) [umoeller]
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_DATAFILEDEFAULTDOC,
            &(pNLSStrings->pszDataFileDefaultDoc));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_FDRDEFAULTDOC,
            &(pNLSStrings->pszFdrDefaultDoc));

    // XCenter V0.9.4 (2000-06-10) [umoeller]
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_XCENTERPAGE1,
            &(pNLSStrings->pszXCenterPage1));

    // file operations V0.9.4 (2000-07-27) [umoeller]
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_FOPS_MOVE2TRASHCAN,
            &(pNLSStrings->pszFopsMove2TrashCan));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_FOPS_RESTOREFROMTRASHCAN,
            &(pNLSStrings->pszFopsRestoreFromTrashCan));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_FOPS_TRUEDELETE,
            &(pNLSStrings->pszFopsTrueDelete));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_FOPS_EMPTYINGTRASHCAN,
            &(pNLSStrings->pszFopsEmptyingTrashCan));

    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_ICONPAGE,
            &(pNLSStrings->pszIconPage));

    // INI save methods V0.9.5 (2000-08-16) [umoeller]
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_XSD_SAVEINIS_NEW,
            &(pNLSStrings->pszXSDSaveInisNew));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_XSD_SAVEINIS_OLD,
            &(pNLSStrings->pszXSDSaveInisOld));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_XSD_SAVEINIS_NONE,
            &(pNLSStrings->pszXSDSaveInisNone));

    // logoff V0.9.5 (2000-09-28) [umoeller]
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_XSD_LOGOFF,
            &(pNLSStrings->pszXSDLogoff));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_XSD_CONFIRMLOGOFFMSG,
            &(pNLSStrings->pszXSDConfirmLogoffMsg));

    // "bytes" strings for status bars V0.9.6 (2000-11-23) [umoeller]
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_BYTE,
            &(pNLSStrings->pszByte));
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_BYTES,
            &(pNLSStrings->pszBytes));

    // (2000-12-14) [lafaix] Resources page name
    cmnLoadString(habDesktop, G_hmodNLS, ID_XSSI_PGMFILE_RESOURCES,
            &(pNLSStrings->pszResourcesPage));
}

/*
 *@@ cmnQueryNLSModuleHandle:
 *      returns the module handle of the language-dependent XFolder
 *      National Language Support DLL (XFLDRxxx.DLL).
 *
 *      This is called in two situations:
 *
 *          1) with fEnforceReload == FALSE everytime some part
 *             of XFolder needs the NLS resources (e.g. for dialogs);
 *             this only loads the NLS DLL on the very first call
 *             then, whose module handle is cached for subsequent calls.
 *
 *          2) with fEnforceReload == TRUE when the user changes
 *             XFolder's language in the "Workplace Shell" object.
 *
 *      If the DLL is (re)loaded, this function also initializes
 *      all language-dependent XWorkplace components such as the NLSSTRINGS
 *      structure, which can always be accessed using cmnQueryNLSStrings.
 *      This function also checks for whether the NLS DLL has a
 *      decent version level to support this XFolder version.
 *
 *@@changed V0.9.0 [umoeller]: added various NLS strings
 *@@changed V0.9.0 (99-11-14) [umoeller]: made this reentrant, finally
 *@@changed V0.9.7 (2000-12-09) [umoeller]: restructured to fix mutex hangs with load errors
 */

HMODULE cmnQueryNLSModuleHandle(BOOL fEnforceReload)
{
    HMODULE hmodReturn = NULLHANDLE,
            hmodLoaded = NULLHANDLE;

    // load resource DLL if it's not loaded yet or a reload is enforced
    if (    (G_hmodNLS == NULLHANDLE)
         || (fEnforceReload)
       )
    {
        CHAR    szResourceModuleName[CCHMAXPATH];

        // get the XFolder path first
        if (!cmnQueryXWPBasePath(szResourceModuleName))
            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                   "cmnQueryXWPBasePath failed.");
        else
        {
            APIRET arc = NO_ERROR;
            // now compose module name from language code
            strcat(szResourceModuleName, "\\bin\\xfldr");
            strcat(szResourceModuleName, cmnQueryLanguageCode());
            strcat(szResourceModuleName, ".dll");

            // try to load the module
            arc = DosLoadModule(NULL,
                                0,
                                szResourceModuleName,
                                (PHMODULE)&hmodLoaded);
            if (arc != NO_ERROR)
            {
                // display an error string; since we don't have NLS,
                // this must be in English...
                CHAR szError[1000];
                sprintf(szError, "XWorkplace was unable to load its National "
                                 "Language Support DLL \"%s\". DosLoadModule returned "
                                 "error %d.",
                        szResourceModuleName,
                        arc);
                // log
                cmnLog(__FILE__, __LINE__, __FUNCTION__,
                       szError);
                // and display
                winhDebugBox(HWND_DESKTOP,
                             "XWorkplace: Error",
                             szError);
            }
            else
            {
                // module loaded alright!
                // hmodLoaded has the new module handle
                HAB habDesktop = WinQueryAnchorBlock(HWND_DESKTOP);

                if (fEnforceReload)
                {
                    // if fEnforceReload == TRUE, we will load a test string from
                    // the module to see if it has at least the version level which
                    // this XFolder version requires. This is done using a #define
                    // in dlgids.h: XFOLDER_VERSION is compiled as a string resource
                    // into both the NLS DLL and into the main DLL (this file),
                    // so we always have the versions in there automatically.
                    // MINIMUM_NLS_VERSION (dlgids.h too) contains the minimum
                    // NLS version level that this XFolder version requires.
                    CHAR   szTest[30] = "";
                    LONG   lLength;
                    cmnSetDlgHelpPanel(-1);
                    lLength = WinLoadString(habDesktop,
                                            G_hmodNLS,
                                            ID_XSSI_XFOLDERVERSION,
                                            sizeof(szTest), szTest);
                    #ifdef DEBUG_LANGCODES
                        _Pmpf(("%s version: %s", szResourceModuleName, szTest));
                    #endif

                    if (lLength == 0)
                    {
                        // version string not found: complain
                        winhDebugBox(HWND_DESKTOP,
                                     "XWorkplace",
                                     "The requested file is not an XWorkplace National Language Support DLL.");
                    }
                    else if (memcmp(szTest, MINIMUM_NLS_VERSION, 4) < 0)
                            // szTest has NLS version (e.g. "0.81 beta"),
                            // MINIMUM_NLS_VERSION has minimum version required
                            // (e.g. "0.9.0")
                    {
                        // version level not sufficient:
                        // load dialog from _old_ NLS DLL which says
                        // that the DLL is too old; if user presses
                        // "Cancel", we abort loading the DLL
                        if (WinDlgBox(HWND_DESKTOP,
                                      HWND_DESKTOP,
                                      (PFNWP)cmn_fnwpDlgWithHelp,
                                      G_hmodNLS,        // still the old one
                                      ID_XFD_WRONGVERSION,
                                      (PVOID)NULL)
                                == DID_CANCEL)
                        {
                            winhDebugBox(HWND_DESKTOP,
                                         "XWorkplace",
                                         "The new National Language Support DLL was not loaded.");
                        }
                        else
                            // user wants outdated module:
                            hmodReturn = hmodLoaded;
                    }
                    else
                    {
                        // new module is OK:
                        hmodReturn = hmodLoaded;
                    }
                } // end if (fEnforceReload)
                else
                    // no enfore reload: that's OK always
                    hmodReturn = hmodLoaded;
            } // end else if (arc != NO_ERROR)
        } // end if (cmnQueryXWPBasePath(szResourceModuleName))
    } // end if (    (G_hmodNLS == NULLHANDLE)  || (fEnforceReload) )
    else
        // no (re)load neccessary:
        // use old module (this must be != NULLHANDLE now)
        hmodReturn = G_hmodNLS;

    // V0.9.7 (2000-12-09) [umoeller]
    // alright, now we have:
    // --  hmodLoaded: != NULLHANDLE if we loaded a new module.
    // --  hmodReturn: != NULLHANDLE if the new module is OK.

    if (hmodLoaded)                    // new module loaded here?
    {
        if (hmodReturn == NULLHANDLE)      // but error?
            DosFreeModule(hmodLoaded);
        else
        {
            // module loaded, and OK:
            // replace the global module handle for NLS,
            // and reload all NLS strings...
            // do this safely.
            HMODULE hmodOld = G_hmodNLS;
            BOOL fLocked = FALSE;
            ULONG ulNesting;
            DosEnterMustComplete(&ulNesting);

            TRY_LOUD(excpt1)
            {
                fLocked = krnLock(__FILE__, __LINE__, __FUNCTION__);
                if (fLocked)
                {
                    NLSSTRINGS *pNLSStrings = (NLSSTRINGS*)cmnQueryNLSStrings();
                                            // this allocates and initializes the array
                    // set global handle
                    G_hmodNLS = hmodLoaded;
                    // load all the new NLS strings
                    LoadNLSData(WinQueryAnchorBlock(HWND_DESKTOP),
                                pNLSStrings);

                    krnUnlock();
                }
            }
            CATCH(excpt1) { } END_CATCH();

            if (fLocked)
                krnUnlock();

            DosExitMustComplete(&ulNesting);

            if (hmodOld)
                // after all this, unload the old resource module
                DosFreeModule(hmodOld);
        }
    }

    if (hmodReturn == NULLHANDLE)
        // error:
        cmnLog(__FILE__, __LINE__, __FUNCTION__,
               "Returning NULLHANDLE. Some error occured.");

    // return (new?) module handle
    return (hmodReturn);
}

/*
 *@@ cmnQueryNLSStrings:
 *      returns pointer to global NLSSTRINGS structure which contains
 *      all the language-dependent XFolder strings from the resource
 *      files.
 *
 *@@changed V0.9.0 (99-11-14) [umoeller]: made this reentrant, finally
 */

PNLSSTRINGS cmnQueryNLSStrings(VOID)
{
    BOOL fLocked = FALSE;
    ULONG ulNesting;
    DosEnterMustComplete(&ulNesting);

    TRY_LOUD(excpt1)
    {
        fLocked = krnLock(__FILE__, __LINE__, __FUNCTION__);
        if (fLocked)
        {
            if (G_pNLSStringsGlobal == NULL)
            {
                G_pNLSStringsGlobal = malloc(sizeof(NLSSTRINGS));
                memset(G_pNLSStringsGlobal, 0, sizeof(NLSSTRINGS));
            }
        }
    }
    CATCH(excpt1) { } END_CATCH();

    if (fLocked)
        krnUnlock();

    DosExitMustComplete(&ulNesting);

    return (G_pNLSStringsGlobal);
}

/*
 *@@ cmnQueryThousandsSeparator:
 *      returns the global COUNTRYSETTINGS (see helpers\prfh.c)
 *      as set in the "Country" object, which are cached for speed.
 *
 *      If (fReload == TRUE), the settings are re-read.
 *
 *@@added V0.9.6 (2000-11-12) [umoeller]
 */

PCOUNTRYSETTINGS cmnQueryCountrySettings(BOOL fReload)
{
    if ((!G_fCountrySettingsLoaded) || (fReload))
    {
        prfhQueryCountrySettings(&G_CountrySettings);
        G_fCountrySettingsLoaded = TRUE;
    }

    return (&G_CountrySettings);
}

/*
 *@@ cmnQueryThousandsSeparator:
 *      returns the thousands separator from the "Country"
 *      object.
 *
 *@@added V0.9.6 (2000-11-12) [umoeller]
 */

CHAR cmnQueryThousandsSeparator(VOID)
{
    PCOUNTRYSETTINGS p = cmnQueryCountrySettings(FALSE);
    return (p->cThousands);
}

/*
 *@@ cmnDescribeKey:
 *      this stores a description of a certain
 *      key into pszBuf, using the NLS DLL strings.
 *      usFlags is as in WM_CHAR.
 *      If (usFlags & KC_VIRTUALKEY), usKeyCode must
 *      be usvk of WM_CHAR (VK_* code), or usch otherwise.
 *      Returns TRUE if this was a valid key combo.
 */

BOOL cmnDescribeKey(PSZ pszBuf,
                    USHORT usFlags,
                    USHORT usKeyCode)
{
    BOOL brc = TRUE;

    PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();

    *pszBuf = 0;
    if (usFlags & KC_CTRL)
        strcpy(pszBuf, pNLSStrings->pszCtrl);
    if (usFlags & KC_SHIFT)
        strcat(pszBuf, pNLSStrings->pszShift);
    if (usFlags & KC_ALT)
        strcat(pszBuf, pNLSStrings->pszAlt);

    if (usFlags & KC_VIRTUALKEY)
    {
        switch (usKeyCode)
        {
            case VK_BACKSPACE: strcat(pszBuf, pNLSStrings->pszBackspace); break;
            case VK_TAB: strcat(pszBuf, pNLSStrings->pszTab); break;
            case VK_BACKTAB: strcat(pszBuf, pNLSStrings->pszBacktab); break;
            case VK_NEWLINE: strcat(pszBuf, pNLSStrings->pszEnter); break;
            case VK_ESC: strcat(pszBuf, pNLSStrings->pszEsc); break;
            case VK_SPACE: strcat(pszBuf, pNLSStrings->pszSpace); break;
            case VK_PAGEUP: strcat(pszBuf, pNLSStrings->pszPageup); break;
            case VK_PAGEDOWN: strcat(pszBuf, pNLSStrings->pszPagedown); break;
            case VK_END: strcat(pszBuf, pNLSStrings->pszEnd); break;
            case VK_HOME: strcat(pszBuf, pNLSStrings->pszHome); break;
            case VK_LEFT: strcat(pszBuf, pNLSStrings->pszLeft); break;
            case VK_UP: strcat(pszBuf, pNLSStrings->pszUp); break;
            case VK_RIGHT: strcat(pszBuf, pNLSStrings->pszRight); break;
            case VK_DOWN: strcat(pszBuf, pNLSStrings->pszDown); break;
            case VK_PRINTSCRN: strcat(pszBuf, pNLSStrings->pszPrintscrn); break;
            case VK_INSERT: strcat(pszBuf, pNLSStrings->pszInsert); break;
            case VK_DELETE: strcat(pszBuf, pNLSStrings->pszDelete); break;
            case VK_SCRLLOCK: strcat(pszBuf, pNLSStrings->pszScrlLock); break;
            case VK_NUMLOCK: strcat(pszBuf, pNLSStrings->pszNumLock); break;
            case VK_ENTER: strcat(pszBuf, pNLSStrings->pszEnter); break;
            case VK_F1: strcat(pszBuf, "F1"); break;
            case VK_F2: strcat(pszBuf, "F2"); break;
            case VK_F3: strcat(pszBuf, "F3"); break;
            case VK_F4: strcat(pszBuf, "F4"); break;
            case VK_F5: strcat(pszBuf, "F5"); break;
            case VK_F6: strcat(pszBuf, "F6"); break;
            case VK_F7: strcat(pszBuf, "F7"); break;
            case VK_F8: strcat(pszBuf, "F8"); break;
            case VK_F9: strcat(pszBuf, "F9"); break;
            case VK_F10: strcat(pszBuf, "F10"); break;
            case VK_F11: strcat(pszBuf, "F11"); break;
            case VK_F12: strcat(pszBuf, "F12"); break;
            case VK_F13: strcat(pszBuf, "F13"); break;
            case VK_F14: strcat(pszBuf, "F14"); break;
            case VK_F15: strcat(pszBuf, "F15"); break;
            case VK_F16: strcat(pszBuf, "F16"); break;
            case VK_F17: strcat(pszBuf, "F17"); break;
            case VK_F18: strcat(pszBuf, "F18"); break;
            case VK_F19: strcat(pszBuf, "F19"); break;
            case VK_F20: strcat(pszBuf, "F20"); break;
            case VK_F21: strcat(pszBuf, "F21"); break;
            case VK_F22: strcat(pszBuf, "F22"); break;
            case VK_F23: strcat(pszBuf, "F23"); break;
            case VK_F24: strcat(pszBuf, "F24"); break;
            default: brc = FALSE; break;
        }
    } // end if (usFlags & KC_VIRTUALKEY)
    else
    {
        switch (usKeyCode)
        {
            case 0xEC00: strcat(pszBuf, pNLSStrings->pszWinLeft); break;
            case 0xED00: strcat(pszBuf, pNLSStrings->pszWinRight); break;
            case 0xEE00: strcat(pszBuf, pNLSStrings->pszWinMenu); break;
            default:
            {
                CHAR szTemp[2];
                if (usKeyCode >= 'a')
                    szTemp[0] = (CHAR)usKeyCode-32;
                else
                    szTemp[0] = (CHAR)usKeyCode;
                szTemp[1] = '\0';
                strcat(pszBuf, szTemp);
            }
        }
    }

    #ifdef DEBUG_KEYS
        _Pmpf(("Key: %s, usKeyCode: 0x%lX, usFlags: 0x%lX", pszBuf, usKeyCode, usFlags));
    #endif

    return (brc);
}

/*
 *@@ cmnAddCloseMenuItem:
 *      adds a "Close" menu item to the given menu.
 *
 *@@added V0.9.7 (2000-12-21) [umoeller]
 */

VOID cmnAddCloseMenuItem(HWND hwndMenu)
{
    // add "Close" menu item
    winhInsertMenuSeparator(hwndMenu,
                            MIT_END,
                            (G_pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_SEPARATOR));
    winhInsertMenuItem(hwndMenu,
                       MIT_END,
                       WPMENUID_CLOSE,
                       "~Close", // ###
                       MIS_TEXT, 0);
}

/* ******************************************************************
 *
 *   XFolder Global Settings
 *
 ********************************************************************/

/*
 *@@ cmnQueryStatusBarSetting:
 *      returns a PSZ to a certain status bar setting, which
 *      may be:
 *      --      SBS_STATUSBARFONT       font (e.g. "9.WarpSans")
 *      --      SBS_TEXTNONESEL         mnemonics for no-object mode
 *      --      SBS_TEXTMULTISEL        mnemonics for multi-object mode
 *
 *      Note that there is no key for querying the mnemonics for
 *      one-object mode, because this is handled by the functions
 *      in statbars.c to provide different data depending on the
 *      class of the selected object.
 *
 *@@changed V0.9.0 (99-11-14) [umoeller]: made this reentrant, finally
 */

const char* cmnQueryStatusBarSetting(USHORT usSetting)
{
    const char *rc = 0;

    BOOL fLocked = FALSE;
    ULONG ulNesting;
    DosEnterMustComplete(&ulNesting);

    TRY_LOUD(excpt1)
    {
        fLocked = krnLock(__FILE__, __LINE__, __FUNCTION__);
        if (fLocked)
        {
            switch (usSetting)
            {
                case SBS_STATUSBARFONT:
                        rc = G_szStatusBarFont;
                    break;
                case SBS_TEXTNONESEL:
                        rc = G_szSBTextNoneSel;
                    break;
                case SBS_TEXTMULTISEL:
                        rc = G_szSBTextMultiSel;
            }
        }
    }
    CATCH(excpt1) { } END_CATCH();

    if (fLocked)
        krnUnlock();

    DosExitMustComplete(&ulNesting);

    return (rc);
}

/*
 *@@ cmnSetStatusBarSetting:
 *      sets usSetting to pszSetting. If pszSetting == NULL, the
 *      default value will be loaded from the XFolder NLS DLL.
 *      usSetting works just like in cmnQueryStatusBarSetting.
 *
 *@@changed V0.9.0 (99-11-14) [umoeller]: made this reentrant, finally
 */

BOOL cmnSetStatusBarSetting(USHORT usSetting, PSZ pszSetting)
{
    BOOL    brc = FALSE;

    BOOL fLocked = FALSE;
    ULONG ulNesting;
    DosEnterMustComplete(&ulNesting);

    TRY_LOUD(excpt1)
    {
        fLocked = krnLock(__FILE__, __LINE__, __FUNCTION__);
        if (fLocked)
        {
            HAB     habDesktop = WinQueryAnchorBlock(HWND_DESKTOP);
            HMODULE hmodResource = cmnQueryNLSModuleHandle(FALSE);

            brc = TRUE;

            switch (usSetting)
            {
                case SBS_STATUSBARFONT:
                {
                        CHAR szDummy[CCHMAXMNEMONICS];
                        if (pszSetting)
                        {
                            strcpy(G_szStatusBarFont, pszSetting);
                            PrfWriteProfileString(HINI_USERPROFILE,
                                                  (PSZ)INIAPP_XWORKPLACE,
                                                  (PSZ)INIKEY_STATUSBARFONT,
                                                  G_szStatusBarFont);
                        }
                        else
                            strcpy(G_szStatusBarFont, "8.Helv");
                        sscanf(G_szStatusBarFont, "%d.%s", &(G_ulStatusBarHeight), &szDummy);
                        G_ulStatusBarHeight += 15;
                break; }

                case SBS_TEXTNONESEL:
                {
                        if (pszSetting)
                        {
                            strcpy(G_szSBTextNoneSel, pszSetting);
                            PrfWriteProfileString(HINI_USERPROFILE,
                                                  (PSZ)INIAPP_XWORKPLACE,
                                                  (PSZ)INIKEY_SBTEXTNONESEL,
                                                  G_szSBTextNoneSel);
                        }
                        else
                            WinLoadString(habDesktop,
                                          hmodResource, ID_XSSI_SBTEXTNONESEL,
                                          sizeof(G_szSBTextNoneSel), G_szSBTextNoneSel);
                break; }

                case SBS_TEXTMULTISEL:
                {
                        if (pszSetting)
                        {
                            strcpy(G_szSBTextMultiSel, pszSetting);
                            PrfWriteProfileString(HINI_USERPROFILE,
                                                  (PSZ)INIAPP_XWORKPLACE,
                                                  (PSZ)INIKEY_SBTEXTMULTISEL,
                                                  G_szSBTextMultiSel);
                        }
                        else
                            WinLoadString(habDesktop,
                                          hmodResource, ID_XSSI_SBTEXTMULTISEL,
                                          sizeof(G_szSBTextMultiSel), G_szSBTextMultiSel);
                break; }

                default:
                    brc = FALSE;

            } // end switch(usSetting)
        }
    }
    CATCH(excpt1) { } END_CATCH();

    if (fLocked)
        krnUnlock();

    DosExitMustComplete(&ulNesting);

    return (brc);
}

/*
 *@@ cmnQueryStatusBarHeight:
 *      returns the height of the status bars according to the
 *      current settings in pixels. This was calculated when
 *      the status bar font was set.
 */

ULONG cmnQueryStatusBarHeight(VOID)
{
    return (G_ulStatusBarHeight);
}

/*
 *@@ cmnLoadGlobalSettings:
 *      this loads the Global Settings from the INI files; should
 *      not be called directly, because this is done automatically
 *      by cmnQueryGlobalSettings, if necessary.
 *
 *      Before loading the settings, all settings are initialized
 *      in case the settings in OS2.INI do not contain all the
 *      settings for this XWorkplace version. This allows for
 *      compatibility with older versions, including XFolder versions.
 *
 *      If (fResetDefaults == TRUE), this only resets all settings
 *      to the default values without loading them from OS2.INI.
 *      This does _not_ write the default settings back to OS2.INI;
 *      if this is desired, call cmnStoreGlobalSettings afterwards.
 *
 *@@changed V0.9.0 [umoeller]: added fResetDefaults to prototype
 *@@changed V0.9.0 [umoeller]: changed initializations for new settings pages
 *@@changed V0.9.0 (99-11-14) [umoeller]: made this reentrant, finally
 */

PCGLOBALSETTINGS cmnLoadGlobalSettings(BOOL fResetDefaults)
{
    BOOL fLocked = FALSE;
    ULONG ulNesting;
    DosEnterMustComplete(&ulNesting);

    TRY_LOUD(excpt1)
    {
        fLocked = krnLock(__FILE__, __LINE__, __FUNCTION__);
        if (fLocked)
        {
            ULONG       ulCopied1;

            if (G_pGlobalSettings == NULL)
            {
                G_pGlobalSettings = malloc(sizeof(GLOBALSETTINGS));
                memset(G_pGlobalSettings, 0, sizeof(GLOBALSETTINGS));
            }

            // first set default settings for each settings page;
            // we only load the "real" settings from OS2.INI afterwards
            // because the user might have updated XFolder, and the
            // settings struct in the INIs might be too short
            cmnSetDefaultSettings(SP_1GENERIC              );
            cmnSetDefaultSettings(SP_2REMOVEITEMS          );
            cmnSetDefaultSettings(SP_25ADDITEMS            );
            cmnSetDefaultSettings(SP_26CONFIGITEMS         );
            cmnSetDefaultSettings(SP_27STATUSBAR           );
            cmnSetDefaultSettings(SP_3SNAPTOGRID           );
            cmnSetDefaultSettings(SP_4ACCELERATORS         );
            // cmnSetDefaultSettings(SP_5INTERNALS            );  removed V0.9.0
            // cmnSetDefaultSettings(SP_DTP2                  );  removed V0.9.0
            cmnSetDefaultSettings(SP_FLDRSORT_GLOBAL       );
            // cmnSetDefaultSettings(SP_FILEOPS               );  removed V0.9.0

            // the following are new with V0.9.0
            cmnSetDefaultSettings(SP_SETUP_INFO            );
            cmnSetDefaultSettings(SP_SETUP_FEATURES        );
            cmnSetDefaultSettings(SP_SETUP_PARANOIA        );

            cmnSetDefaultSettings(SP_DTP_MENUITEMS         );
            cmnSetDefaultSettings(SP_DTP_STARTUP           );
            cmnSetDefaultSettings(SP_DTP_SHUTDOWN          );

            cmnSetDefaultSettings(SP_STARTUPFOLDER         );

            cmnSetDefaultSettings(SP_TRASHCAN_SETTINGS);

            // cmnSetDefaultSettings(SP_MOUSE_MOVEMENT); does nothing
            // cmnSetDefaultSettings(SP_MOUSE_CORNERS);  does nothing

            // reset help panels
            G_pGlobalSettings->ulIntroHelpShown = 0;

            if (fResetDefaults == FALSE)
            {
                // get global XFolder settings from OS2.INI
                PrfQueryProfileString(HINI_USERPROFILE,
                                      (PSZ)INIAPP_XWORKPLACE,
                                      (PSZ)INIKEY_STATUSBARFONT,
                                      "8.Helv",
                                      &(G_szStatusBarFont),
                                      sizeof(G_szStatusBarFont));
                sscanf(G_szStatusBarFont, "%d.*%s", &(G_ulStatusBarHeight));
                G_ulStatusBarHeight += 15;

                PrfQueryProfileString(HINI_USERPROFILE,
                                      (PSZ)INIAPP_XWORKPLACE,
                                      (PSZ)INIKEY_SBTEXTNONESEL,
                                      NULL,
                                      &(G_szSBTextNoneSel),
                                      sizeof(G_szSBTextNoneSel));
                PrfQueryProfileString(HINI_USERPROFILE,
                                      (PSZ)INIAPP_XWORKPLACE,
                                      (PSZ)INIKEY_SBTEXTMULTISEL,
                                      NULL,
                                      &(G_szSBTextMultiSel),
                                      sizeof(G_szSBTextMultiSel));

                ulCopied1 = sizeof(GLOBALSETTINGS);
                PrfQueryProfileData(HINI_USERPROFILE,
                                    (PSZ)INIAPP_XWORKPLACE,
                                    (PSZ)INIKEY_GLOBALSETTINGS,
                                    G_pGlobalSettings,
                                    &ulCopied1);
            }

        }
    }
    CATCH(excpt1) { } END_CATCH();

    if (fLocked)
        krnUnlock();

    DosExitMustComplete(&ulNesting);

    return (G_pGlobalSettings);
}

/*
 *@@ cmnQueryGlobalSettings:
 *      returns pointer to the GLOBALSETTINGS structure which
 *      contains the XWorkplace Global Settings valid for all
 *      classes. Loads the settings from the INI files if this
 *      hasn't been done yet.
 *
 *      This is used all the time throughout XWorkplace.
 *
 *      NOTE (UM 99-11-14): This now returns a const pointer
 *      to the global settings, because the settings are
 *      unprotected after this call. Never make changes to the
 *      global settings using the return value of this function;
 *      use cmnLockGlobalSettings instead.
 *
 *@@changed V0.9.0 (99-11-14) [umoeller]: made this reentrant, finally; now returning a const pointer only
 */

const GLOBALSETTINGS* cmnQueryGlobalSettings(VOID)
{
    BOOL fLocked = FALSE;
    ULONG ulNesting;
    DosEnterMustComplete(&ulNesting);

    TRY_LOUD(excpt1)
    {
        fLocked = krnLock(__FILE__, __LINE__, __FUNCTION__);
        if (fLocked)
        {
            if (G_pGlobalSettings == NULL)
                cmnLoadGlobalSettings(FALSE);       // load from INI
                        // this locks again
        }
    }
    CATCH(excpt1) { } END_CATCH();

    if (fLocked)
        krnUnlock();

    DosExitMustComplete(&ulNesting);

    return (G_pGlobalSettings);
}

/*
 *@@ cmnLockGlobalSettings:
 *      this returns a non-const pointer to the global
 *      settings and locks them, using a mutex semaphore.
 *
 *      If you use this function, ALWAYS call
 *      cmnUnlockGlobalSettings afterwards, as quickly
 *      as possible, because other threads cannot
 *      access the global settings after this call.
 *
 *      Always install an exception handler ...
 *
 *@@added V0.9.0 (99-11-14) [umoeller]
 */

GLOBALSETTINGS* cmnLockGlobalSettings(const char *pcszSourceFile,
                                      ULONG ulLine,
                                      const char *pcszFunction)
{
    if (krnLock(pcszSourceFile, ulLine, pcszFunction))
        return (G_pGlobalSettings);
    else
    {
        cmnLog(__FILE__, __LINE__, __FUNCTION__,
                       "krnLock failed.");
        return (NULL);
    }
}

/*
 *@@ cmnUnlockGlobalSettings:
 *      antagonist to cmnLockGlobalSettings.
 *
 *@@added V0.9.0 (99-11-14) [umoeller]
 */

VOID cmnUnlockGlobalSettings(VOID)
{
    krnUnlock();
}

/*
 *@@ cmnStoreGlobalSettings:
 *      stores the current Global Settings back into the INI files;
 *      returns TRUE if successful.
 *
 *@@changed V0.9.4 (2000-06-16) [umoeller]: now using Worker thread instead of File thread
 */

BOOL cmnStoreGlobalSettings(VOID)
{
    xthrPostWorkerMsg(WOM_STOREGLOBALSETTINGS, 0, 0);
    return (TRUE);
}

/*
 *@@ cmnSetDefaultSettings:
 *      resets those Global Settings which correspond to usSettingsPage
 *      in the System notebook to the default values.
 *
 *      usSettingsPage must be one of the SP_* flags def'd in common.h.
 *      This approach allows to reset the settings to default values
 *      both for a single page (in the various settings notebooks)
 *      and, when this function gets called for each of the settings
 *      pages in cmnLoadGlobalSettings, globally.
 *
 *@@changed V0.9.0 [umoeller]: greatly extended for all the new settings pages
 */

BOOL cmnSetDefaultSettings(USHORT usSettingsPage)
{
    switch(usSettingsPage)
    {
        case SP_1GENERIC:
            // pGlobalSettings->ShowInternals = 1;   // removed V0.9.0
            // pGlobalSettings->ReplIcons = 1;       // removed V0.9.0
            G_pGlobalSettings->FullPath = 1;
            G_pGlobalSettings->KeepTitle = 1;
            G_pGlobalSettings->MaxPathChars = 25;
            G_pGlobalSettings->TreeViewAutoScroll = 1;

            G_pGlobalSettings->fFdrDefaultDoc = 0;
            G_pGlobalSettings->fFdrDefaultDocView = 0;

            G_pGlobalSettings->fFdrAutoRefreshDisabled = 0;
        break;

        case SP_2REMOVEITEMS:
            G_pGlobalSettings->DefaultMenuItems = 0;
            G_pGlobalSettings->RemoveLockInPlaceItem = 0;
            G_pGlobalSettings->RemoveCheckDiskItem = 0;
            G_pGlobalSettings->RemoveFormatDiskItem = 0;
            G_pGlobalSettings->RemoveViewMenu = 0;
            G_pGlobalSettings->RemovePasteItem = 0;
            G_pGlobalSettings->fFixLockInPlace = 0;     // V0.9.7 (2000-12-10) [umoeller]
        break;

        case SP_25ADDITEMS:
            G_pGlobalSettings->FileAttribs = 1;
            G_pGlobalSettings->AddCopyFilenameItem = 1;
            G_pGlobalSettings->ExtendFldrViewMenu = 1;
            G_pGlobalSettings->MoveRefreshNow = (doshIsWarp4() ? 1 : 0);
            G_pGlobalSettings->AddSelectSomeItem = 1;
            G_pGlobalSettings->AddFolderContentItem = 1;
            G_pGlobalSettings->FCShowIcons = 0;
        break;

        case SP_26CONFIGITEMS:
            G_pGlobalSettings->MenuCascadeMode = 0;
            G_pGlobalSettings->RemoveX = 1;
            G_pGlobalSettings->AppdParam = 1;
            G_pGlobalSettings->TemplatesOpenSettings = BM_INDETERMINATE;
            G_pGlobalSettings->TemplatesReposition = 1;
        break;

        case SP_27STATUSBAR:
            G_pGlobalSettings->fDefaultStatusBarVisibility = 1;       // changed V0.9.0
            G_pGlobalSettings->SBStyle = (doshIsWarp4() ? SBSTYLE_WARP4MENU : SBSTYLE_WARP3RAISED);
            G_pGlobalSettings->SBForViews = SBV_ICON | SBV_DETAILS;
            G_pGlobalSettings->lSBBgndColor = WinQuerySysColor(HWND_DESKTOP, SYSCLR_INACTIVEBORDER, 0);
            G_pGlobalSettings->lSBTextColor = WinQuerySysColor(HWND_DESKTOP, SYSCLR_OUTPUTTEXT, 0);
            cmnSetStatusBarSetting(SBS_TEXTNONESEL, NULL);
            cmnSetStatusBarSetting(SBS_TEXTMULTISEL, NULL);
            G_pGlobalSettings->bDereferenceShadows = STBF_DEREFSHADOWS_SINGLE;
        break;

        case SP_3SNAPTOGRID:
            G_pGlobalSettings->fAddSnapToGridDefault = 1;
            G_pGlobalSettings->GridX = 15;
            G_pGlobalSettings->GridY = 10;
            G_pGlobalSettings->GridCX = 20;
            G_pGlobalSettings->GridCY = 35;
        break;

        case SP_4ACCELERATORS:
            G_pGlobalSettings->fFolderHotkeysDefault = 1;
            G_pGlobalSettings->fShowHotkeysInMenus = 1;
        break;

        case SP_FLDRSORT_GLOBAL:
            // G_pGlobalSettings->ReplaceSort = 0;        removed V0.9.0
            G_pGlobalSettings->DefaultSort = SV_NAME;
            G_pGlobalSettings->AlwaysSort = 0;
        break;

        case SP_DTP_MENUITEMS:  // extra Desktop page
            G_pGlobalSettings->fDTMSort = 1;
            G_pGlobalSettings->fDTMArrange = 1;
            G_pGlobalSettings->fDTMSystemSetup = 1;
            G_pGlobalSettings->fDTMLockup = 1;
            G_pGlobalSettings->fDTMLogoffNetwork = 1; // V0.9.7 (2000-12-13) [umoeller]
            G_pGlobalSettings->fDTMShutdown = 1;
            G_pGlobalSettings->fDTMShutdownMenu = 1;
        break;

        case SP_DTP_STARTUP:
            G_pGlobalSettings->ShowBootupStatus = 0;
            G_pGlobalSettings->BootLogo = 0;
            G_pGlobalSettings->bBootLogoStyle = 0;
            G_pGlobalSettings->fNumLockStartup = 0;
        break;

        case SP_DTP_SHUTDOWN:
            G_pGlobalSettings->ulXShutdownFlags = // changed V0.9.0
                XSD_WPS_CLOSEWINDOWS | XSD_CONFIRM | XSD_REBOOT | XSD_ANIMATE_SHUTDOWN;
            G_pGlobalSettings->bSaveINIS = 0; // new method, V0.9.5 (2000-08-16) [umoeller]
        break;

        case SP_DTP_ARCHIVES:  // all new with V0.9.0
            // no settings here, these are set elsewhere
        break;

        case SP_SETUP_FEATURES:   // all new with V0.9.0
            G_pGlobalSettings->fReplaceIcons = 0;
            G_pGlobalSettings->fResizeSettingsPages = 0;
            G_pGlobalSettings->AddObjectPage = 0;
            G_pGlobalSettings->fReplaceFilePage = 0;
            G_pGlobalSettings->fXSystemSounds = 0;

            G_pGlobalSettings->fEnableStatusBars = 0;
            G_pGlobalSettings->fEnableSnap2Grid = 0;
            G_pGlobalSettings->fEnableFolderHotkeys = 0;
            G_pGlobalSettings->ExtFolderSort = 0;

            G_pGlobalSettings->fAniMouse = 0;
            G_pGlobalSettings->fEnableXWPHook = 0;
            // global hotkeys ### V0.9.4 (2000-06-05) [umoeller]
            G_pGlobalSettings->fEnablePageMage = 0; // ### V0.9.4 (2000-06-05) [umoeller]

            G_pGlobalSettings->fReplaceArchiving = 0;
            G_pGlobalSettings->fRestartWPS = 0;
            G_pGlobalSettings->fXShutdown = 0;

            // G_pGlobalSettings->fMonitorCDRoms = 0;

            G_pGlobalSettings->fExtAssocs = 0;
            G_pGlobalSettings->CleanupINIs = 0;
#ifdef __REPLHANDLES__
            G_pGlobalSettings->fReplaceHandles = 0; // added V0.9.5 (2000-08-14) [umoeller]
#endif
            G_pGlobalSettings->fReplFileExists = 0;
            G_pGlobalSettings->fReplDriveNotReady = 0;
            G_pGlobalSettings->fTrashDelete = 0;
            G_pGlobalSettings->fReplaceTrueDelete = 0; // added V0.9.3 (2000-04-26) [umoeller]
        break;

        case SP_SETUP_PARANOIA:   // all new with V0.9.0
            G_pGlobalSettings->VarMenuOffset   = 700;     // raised (V0.9.0)
            G_pGlobalSettings->fNoFreakyMenus   = 0;
            G_pGlobalSettings->fNoSubclassing   = 0;
            G_pGlobalSettings->NoWorkerThread  = 0;
            G_pGlobalSettings->fUse8HelvFont   = (!doshIsWarp4());
            G_pGlobalSettings->fNoExcptBeeps    = 0;
            G_pGlobalSettings->bDefaultWorkerThreadPriority = 1;  // idle +31
            G_pGlobalSettings->fWorkerPriorityBeep = 0;
        break;

        case SP_STARTUPFOLDER:        // all new with V0.9.0
            G_pGlobalSettings->ShowStartupProgress = 1;
            G_pGlobalSettings->ulStartupInitialDelay = 1000;
            G_pGlobalSettings->ulStartupObjectDelay = 1000;
        break;

        case SP_TRASHCAN_SETTINGS:             // all new with V0.9.0
            // G_pGlobalSettings->fTrashDelete = 0;  // removedV0.9.3 (2000-04-10) [umoeller]
            // G_pGlobalSettings->fTrashEmptyStartup = 0;
            // G_pGlobalSettings->fTrashEmptyShutdown = 0;
            G_pGlobalSettings->ulTrashConfirmEmpty = TRSHCONF_DESTROYOBJ | TRSHCONF_EMPTYTRASH;
        break;
    }

    return (TRUE);
}

/* ******************************************************************
 *
 *   Object setup sets V0.9.9 (2001-01-29) [umoeller]
 *
 ********************************************************************/

/*
 *@@ cmnSetupInitData:
 *      setup set helper to be used in a wpInitData override.
 *      See XWPSETUPENTRY for an introduction.
 *
 *      This initializes each entry with the lDefault value
 *      from the XWPSETUPENTRY.
 *
 *      This ONLY initializes STG_LONG and STG_BOOL entries,
 *      but not STG_BITFLAG fields. For this reason, for bit
 *      fields, always define a preceding STG_LONG entry.
 *
 *@@added V0.9.9 (2001-01-29) [umoeller]
 */

VOID cmnSetupInitData(PXWPSETUPENTRY paSettings, // in: object's setup set
                      ULONG cSettings,       // in: array item count (NOT array size)
                      PVOID somThis)         // in: instance's somThis pointer
{
    ULONG   ul = 0;
    CHAR    szTemp[100];

    for (ul = 0;
         ul < cSettings;
         ul++)
    {
        PXWPSETUPENTRY pSettingThis = &paSettings[ul];

        switch (pSettingThis->ulType)
        {
            case STG_LONG:
            case STG_BOOL:
            {
                PLONG plData = (PLONG)((PBYTE)somThis + pSettingThis->ulOfsOfData);
                *plData = pSettingThis->lDefault;
            break; }

            // ignore STG_BITFIELD
        }
    }
}

/*
 *@@ cmnSetupBuildString:
 *      setup set helper to be used in an xwpQuerySetup2 override.
 *      See XWPSETUPENTRY for an introduction.
 *
 *      This adds new setup strings to the specified XSTRING, which
 *      should be safely initialized. XWPSETUPENTRY's that have
 *      (pcszSetupString == NULL) are skipped.
 *
 *      -- For STG_LONG, this appends "KEYWORD=%d;".
 *
 *      -- For STG_BOOL and STG_BITFLAG, this appends "KEYWORD={YES|NO};".
 *
 *@@added V0.9.7 (2001-01-25) [umoeller]
 */

VOID cmnSetupBuildString(PXWPSETUPENTRY paSettings, // in: object's setup set
                         ULONG cSettings,       // in: array item count (NOT array size)
                         PVOID somThis,         // in: instance's somThis pointer
                         PXSTRING pstr)         // out: setup string

{
    ULONG   ul = 0;
    CHAR    szTemp[100];

    for (ul = 0;
         ul < cSettings;
         ul++)
    {
        PXWPSETUPENTRY pSettingThis = &paSettings[ul];

        // setup string supported for this?
        if (pSettingThis->pcszSetupString)
        {
            // yes:

            switch (pSettingThis->ulType)
            {
                case STG_LONG:
                {
                    PLONG plData = (PLONG)((PBYTE)somThis + pSettingThis->ulOfsOfData);
                    if (*plData != pSettingThis->lDefault)
                    {
                        sprintf(szTemp,
                                "%s=%d;",
                                pSettingThis->pcszSetupString,
                                *plData);
                        xstrcat(pstr, szTemp, 0);
                    }
                break; }

                case STG_BOOL:
                {
                    PBOOL plData = (PBOOL)((PBYTE)somThis + pSettingThis->ulOfsOfData);
                    if (*plData != (BOOL)pSettingThis->lDefault)
                    {
                        sprintf(szTemp,
                                "%s=%s;",
                                pSettingThis->pcszSetupString,
                                (*plData == TRUE)
                                    ? "YES"
                                    : "NO");
                        xstrcat(pstr, szTemp, 0);
                    }
                break; }

                case STG_BITFLAG:
                {
                    PULONG pulData = (PULONG)((PBYTE)somThis + pSettingThis->ulOfsOfData);
                    if (    ((*pulData) & pSettingThis->ulBitflag)
                         != (ULONG)pSettingThis->lDefault
                       )
                    {
                        sprintf(szTemp,
                                "%s=%s;",
                                pSettingThis->pcszSetupString,
                                ((*pulData) & pSettingThis->ulBitflag)
                                    ? "YES"
                                    : "NO");
                        xstrcat(pstr, szTemp, 0);
                    }
                break; }
            }
        } // end if (pSettingThis->pcszSetupString)
    }
}

/*
 *@@ cmnSetupScanString:
 *      setup set helper to be used in a wpSetup override.
 *      See XWPSETUPENTRY for an introduction.
 *
 *      -- For STG_LONG, this expects "KEYWORD=%d;" strings.
 *
 *      -- For STG_BOOL and STG_BITFLAG, this expects
 *         "KEYWORD={YES|NO};" strings.
 *
 *      Returns FALSE if values were not set properly.
 *
 *@@added V0.9.7 (2001-01-25) [umoeller]
 */

BOOL cmnSetupScanString(WPObject *somSelf,
                        PXWPSETUPENTRY paSettings, // in: object's setup set
                        ULONG cSettings,         // in: array item count (NOT array size)
                        PVOID somThis,           // in: instance's somThis pointer
                        PSZ pszSetupString,      // in: setup string from wpSetup
                        PULONG pcSuccess)        // out: items successfully parsed and set
{
    BOOL    brc = TRUE;
    CHAR    szValue[500];
    ULONG   cbValue;
    ULONG   ul = 0;

    for (ul = 0;
         ul < cSettings;
         ul++)
    {
        PXWPSETUPENTRY pSettingThis = &paSettings[ul];

        // setup string supported for this?
        if (pSettingThis->pcszSetupString)
        {
            // yes:
            cbValue = sizeof(szValue);
            if (_wpScanSetupString(somSelf,
                                   pszSetupString,
                                   (PSZ)pSettingThis->pcszSetupString,
                                   szValue,
                                   &cbValue))
            {
                // setting found:
                // see what to do with it
                switch (pSettingThis->ulType)
                {
                    case STG_LONG:
                    {
                        LONG lValue = atoi(szValue);
                        if (    (lValue >= pSettingThis->lMin)
                             && (lValue <= pSettingThis->lMax)
                           )
                        {
                            PLONG plData = (PLONG)((PBYTE)somThis + pSettingThis->ulOfsOfData);
                            if (*plData != lValue)
                            {
                                // data changed:
                                *plData = lValue;
                                (*pcSuccess)++;
                            }
                        }
                        else
                            brc = FALSE;
                    break; }

                    case STG_BOOL:
                    {
                        BOOL fNew;
                        if (!stricmp(szValue, "YES"))
                            fNew = TRUE;
                        else if (!stricmp(szValue, "NO"))
                            fNew = FALSE;
                        else
                            brc = FALSE;

                        if (brc)
                        {
                            PBOOL plData = (PBOOL)((PBYTE)somThis + pSettingThis->ulOfsOfData);
                            if (*plData != fNew)
                            {
                                *plData = fNew;
                                (*pcSuccess)++;
                            }
                        }
                    break; }

                    case STG_BITFLAG:
                    {
                        ULONG   ulNew = 0;
                        if (!stricmp(szValue, "YES"))
                            ulNew = pSettingThis->ulBitflag;
                        else if (!stricmp(szValue, "NO"))
                            ulNew = 0;
                        else
                            brc = FALSE;

                        if (brc)
                        {
                            PULONG pulData = (PULONG)((PBYTE)somThis + pSettingThis->ulOfsOfData);
                            if (    ((*pulData) & pSettingThis->ulBitflag)
                                 != ulNew
                               )
                            {
                                *pulData = (  // clear the bit first:
                                              ((*pulData) & ~pSettingThis->ulBitflag)
                                              // set it if set
                                            | ulNew);

                                (*pcSuccess)++;
                            }
                        }
                    break; }
                }
            }

            if (!brc)
                // error occured:
                break;
        } // end if (pSettingThis->pcszSetupString)
    }

    return (brc);
}

/*
 *@@ cmnSetupSave:
 *      setup set helper to be used in a wpSaveState override.
 *      See XWPSETUPENTRY for an introduction.
 *
 *      This invokes wpSaveLong on each setup set entry.
 *
 *      Returns FALSE if values were not saved properly.
 *
 *@@added V0.9.9 (2001-01-29) [umoeller]
 */

BOOL cmnSetupSave(WPObject *somSelf,
                  PXWPSETUPENTRY paSettings, // in: object's setup set
                  ULONG cSettings,         // in: array item count (NOT array size)
                  const char *pcszClassName, // in: class name to be used with wpSave*
                  PVOID somThis)           // in: instance's somThis pointer
{
    BOOL    brc = TRUE;
    ULONG   ul = 0;

    for (ul = 0;
         ul < cSettings;
         ul++)
    {
        PXWPSETUPENTRY pSettingThis = &paSettings[ul];

        if (pSettingThis->ulKey)
        {
            switch (pSettingThis->ulType)
            {
                case STG_LONG:
                case STG_BOOL:
                case STG_BITFLAG:
                {
                    PULONG pulData = (PULONG)((PBYTE)somThis + pSettingThis->ulOfsOfData);
                    if (!_wpSaveLong(somSelf,
                                     (PSZ)pcszClassName,
                                     pSettingThis->ulKey,
                                     *pulData))
                    {
                        // error:
                        brc = FALSE;
                        break;
                    }
                break; }
            }
        }
    }

    return (brc);
}

/*
 *@@ cmnSetupRestore:
 *      setup set helper to be used in a wpRestoreState override.
 *      See XWPSETUPENTRY for an introduction.
 *
 *      This invokes wpRestoreLong on each setup set entry.
 *
 *      For each entry, only if wpRestoreLong succeeded,
 *      the corresponding value in the instance data is
 *      overwritten. Otherwise it is undefined, so this
 *      does not replace the need to call cmnSetupInitData
 *      on wpInitData.
 *
 *@@added V0.9.9 (2001-01-29) [umoeller]
 */

BOOL cmnSetupRestore(WPObject *somSelf,
                     PXWPSETUPENTRY paSettings, // in: object's setup set
                     ULONG cSettings,         // in: array item count (NOT array size)
                     const char *pcszClassName, // in: class name to be used with wpRestore*
                     PVOID somThis)           // in: instance's somThis pointer
{
    BOOL    brc = TRUE;
    ULONG   ul = 0;

    for (ul = 0;
         ul < cSettings;
         ul++)
    {
        PXWPSETUPENTRY pSettingThis = &paSettings[ul];

        if (pSettingThis->ulKey)
        {
            switch (pSettingThis->ulType)
            {
                case STG_LONG:
                case STG_BOOL:
                case STG_BITFLAG:
                {
                    ULONG   ulTemp = 0;
                    if (_wpRestoreLong(somSelf,
                                       (PSZ)pcszClassName,
                                       pSettingThis->ulKey,
                                       &ulTemp))
                    {
                        // only if found,
                        // replace value
                        PULONG pulData = (PULONG)((PBYTE)somThis + pSettingThis->ulOfsOfData);
                        *pulData = ulTemp;
                    }
                break; }
            }
        }
    }

    return (brc);
}

/*
 *@@ cmnSetupSetDefaults:
 *      resets part of an object's setup set to default
 *      values. Useful for the "Default" button on a
 *      notebook page.
 *
 *      This requires two arrays as input:
 *
 *      -- PXWPSETUPENTRY paSettings specifies the object's
 *         setup set, as with the other cmnSetup* functions.
 *         This is used to retrieve the default values.
 *
 *      -- PULONG paulOffsets specifies an array of ULONG's.
 *         Each ULONG in that array must specify the
 *         FIELDOFFSET matching one of the items in the
 *         XWPSETUPENTRY array.
 *
 *      This function goes thru the "paulOffsets" array and
 *      finds the corresponding offset in the "paSettings"
 *      setup set array. If found, the corresponding value
 *      (offset to the somThis pointer) is reset to the default.
 *
 *      Again, this ignores STG_BITFIELD entries.
 *
 *      Returns the no. of values successfully changed,
 *      which should match cOffsets.
 *
 *@@added V0.9.9 (2001-01-29) [umoeller]
 */

ULONG cmnSetupSetDefaults(PXWPSETUPENTRY paSettings, // in: object's setup set
                          ULONG cSettings,          // in: array item count (NOT array size)
                          PULONG paulOffsets,
                          ULONG cOffsets,           // in: array item count (NOT array size)
                          PVOID somThis)            // in: instance's somThis pointer
{
    ULONG   ulrc = 0,
            ulOfsThis = 0;

    // go thru the offsets array
    for (ulOfsThis = 0;
         ulOfsThis < cOffsets;
         ulOfsThis++)
    {
        PULONG pulOfsOfDataThis = &paulOffsets[ulOfsThis];

        // now go thru the setup set and find the first entry
        // which matches this offset
        ULONG ulSettingThis = 0;
        for (ulSettingThis = 0;
             ulSettingThis < cSettings;
             ulSettingThis++)
        {
            PXWPSETUPENTRY pSettingThis = &paSettings[ulSettingThis];

            if (    (pSettingThis->ulType == STG_LONG)
                 || (pSettingThis->ulType == STG_BOOL)
                 // but skip STG_BITFLAG
               )
            {
                if (pSettingThis->ulOfsOfData == *pulOfsOfDataThis)
                {
                    // found:
                    // reset value
                    PLONG plData = (PLONG)((PBYTE)somThis + pSettingThis->ulOfsOfData);
                    *plData = pSettingThis->lDefault;
                    // raise return count
                    ulrc++;
                    // and quit inner search block... we'll only modify
                    // the first entry, since there may be others following
                    break; // for (ulSettingThis = 0;
                }
            }
        } // for (ulSettingThis = 0;
    } // for (ulOfsThis = 0;

    return (ulrc);
}

/*
 *@@ cmnSetupRestoreBackup:
 *      resets part of an object's setup set to values
 *      that have been backed up before.
 *      Useful for the "Undo" button on a notebook page.
 *
 *      As opposed to cmnSetupSetDefaults, this only needs
 *      the "paulOffsets" array.
 *
 *      As with cmnSetupSetDefaults, "PULONG paulOffsets"
 *      specifies an array of ULONG's. Each ULONG in that
 *      array must specify the FIELDOFFSET of a setting
 *      from somThis.
 *
 *      This function goes thru the "paulOffsets" array and
 *      copies the value at each offset from pBackup to
 *      somThis.
 *
 *      Returns the no. of values successfully changed,
 *      which should match cOffsets.
 *
 *@@added V0.9.9 (2001-01-29) [umoeller]
 */

ULONG cmnSetupRestoreBackup(PULONG paulOffsets,
                            ULONG cOffsets,           // in: array item count (NOT array size)
                            PVOID somThis,            // in: instance's somThis pointer
                            PVOID pBackup)            // in: backup of somThis
{
    ULONG   ulrc = 0,
            ulOfsThis = 0;

    // go thru the offsets array
    for (ulOfsThis = 0;
         ulOfsThis < cOffsets;
         ulOfsThis++)
    {
        PULONG pulOfsOfDataThis = &paulOffsets[ulOfsThis];

        // restore value
        PLONG plTarget = (PLONG)((PBYTE)somThis + *pulOfsOfDataThis);
        PLONG plSource = (PLONG)((PBYTE)pBackup + *pulOfsOfDataThis);
        *plTarget = *plSource;
        // raise return count
        ulrc++;
    } // for (ulOfsThis = 0;

    return (ulrc);
}

/* ******************************************************************
 *
 *   Trash can setup
 *
 ********************************************************************/

/*
 *@@ cmnTrashCanReady:
 *      returns TRUE if the trash can classes are
 *      installed and the default trash can exists.
 *
 *      This does not check for whether "delete to
 *      trash can" is enabled. Query
 *      GLOBALSETTINGS.fTrashDelete to find out.
 *
 *@@added V0.9.1 (2000-02-01) [umoeller]
 *@@changed V0.9.4 (2000-08-03) [umoeller]: moved this here from fileops.c
 */

BOOL cmnTrashCanReady(VOID)
{
    BOOL brc = FALSE;
    PCKERNELGLOBALS pKernelGlobals = krnQueryGlobals();
    M_XWPTrashCan *pTrashCanClass = _XWPTrashCan;
    if (pTrashCanClass)
    {
        if (_xwpclsQueryDefaultTrashCan(_XWPTrashCan))
            brc = TRUE;
    }

    return (brc);
}

/*
 *@@ cmnEnableTrashCan:
 *      enables or disables the XWorkplace trash can
 *      altogether after displaying a confirmation prompt.
 *
 *      This does all of the following:
 *      -- (de)register XWPTrashCan and XWPTrashObject;
 *      -- enable "delete into trashcan" support;
 *      -- create or destroy the default trash can.
 *
 *@@added V0.9.1 (2000-02-01) [umoeller]
 *@@changed V0.9.4 (2000-08-03) [umoeller]: moved this here from fileops.c
 */

BOOL cmnEnableTrashCan(HWND hwndOwner,     // for message boxes
                       BOOL fEnable)
{
    BOOL    brc = FALSE;

    if (fEnable)
    {
        // enable:
        M_XWPTrashCan       *pXWPTrashCanClass = _XWPTrashCan;

        BOOL    fCreateObject = FALSE;

        if (    (!winhIsClassRegistered("XWPTrashCan"))
             || (!winhIsClassRegistered("XWPTrashObject"))
           )
        {
            // classes not registered yet:
            if (cmnMessageBoxMsg(hwndOwner,
                                 148,       // XWPSetup
                                 170,       // "register trash can?"
                                 MB_YESNO)
                    == MBID_YES)
            {
                CHAR szRegisterError[500];

                HPOINTER hptrOld = winhSetWaitPointer();

                if (WinRegisterObjectClass("XWPTrashCan",
                                           (PSZ)cmnQueryMainModuleFilename()))
                    if (WinRegisterObjectClass("XWPTrashObject",
                                               (PSZ)cmnQueryMainModuleFilename()))
                    {
                        fCreateObject = TRUE;
                        brc = TRUE;
                    }

                WinSetPointer(HWND_DESKTOP, hptrOld);

                if (!brc)
                    // error:
                    cmnMessageBoxMsg(hwndOwner,
                                     148,
                                     171, // "error"
                                     MB_CANCEL);
            }
        }
        else
            fCreateObject = TRUE;

        if (fCreateObject)
        {
            XWPTrashCan *pDefaultTrashCan = NULL;

            if (NULLHANDLE == WinQueryObject((PSZ)XFOLDER_TRASHCANID))
            {
                brc = setCreateStandardObject(hwndOwner,
                                              213,        // XWPTrashCan
                                              FALSE);     // XWP object
            }
            else
                brc = TRUE;

            if (brc)
            {
                GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(__FILE__, __LINE__, __FUNCTION__);
                if (pGlobalSettings)
                {
                    pGlobalSettings->fTrashDelete = TRUE;
                    cmnUnlockGlobalSettings();
                }
            }
        }
    } // end if (fEnable)
    else
    {
        GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(__FILE__, __LINE__, __FUNCTION__);
        if (pGlobalSettings)
        {
            pGlobalSettings->fTrashDelete = FALSE;
            cmnUnlockGlobalSettings();
        }

        if (krnQueryLock())
            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                       "Global lock already requested.");
        else
        {
            // disable:
            if (cmnMessageBoxMsg(hwndOwner,
                                 148,       // XWPSetup
                                 172,       // "deregister trash can?"
                                 MB_YESNO | MB_DEFBUTTON2)
                    == MBID_YES)
            {
                XWPTrashCan *pDefaultTrashCan = _xwpclsQueryDefaultTrashCan(_XWPTrashCan);
                if (pDefaultTrashCan)
                    _wpFree(pDefaultTrashCan);
                WinDeregisterObjectClass("XWPTrashCan");
                WinDeregisterObjectClass("XWPTrashObject");

                cmnMessageBoxMsg(hwndOwner,
                                 148,       // XWPSetup
                                 173,       // "done, restart WPS"
                                 MB_OK);
            }
        }
    }

    cmnStoreGlobalSettings();

    return (brc);
}

/*
 *@@ cmnDeleteIntoDefTrashCan:
 *      moves a single object to the default trash can.
 *
 *@@added V0.9.4 (2000-08-03) [umoeller]
 *@@changed V0.9.9 (2000-02-06) [umoeller]: renamed from cmnMove2DefTrashCan
 */

BOOL cmnDeleteIntoDefTrashCan(WPObject *pObject)
{
    BOOL brc = FALSE;
    XWPTrashCan *pDefaultTrashCan = _xwpclsQueryDefaultTrashCan(_XWPTrashCan);
    if (pDefaultTrashCan)
        brc = _xwpDeleteIntoTrashCan(pDefaultTrashCan,
                                     pObject);
    return (brc);
}

/*
 *@@ cmnEmptyDefTrashCan:
 *      quick interface to empty the default trash can
 *      without having to include all the trash can headers.
 *
 *      See XWPTrashCan::xwpEmptyTrashCan for the description
 *      of the parameters.
 *
 *@@added V0.9.4 (2000-08-03) [umoeller]
 *@@changed V0.9.7 (2001-01-17) [umoeller]: now returning ULONG
 */

APIRET cmnEmptyDefTrashCan(HAB hab,        // in: synchronously?
                           PULONG pulDeleted, // out: if TRUE is returned, no. of deleted objects; can be 0
                           HWND hwndConfirmOwner) // in: if != NULLHANDLE, confirm empty
{
    LONG ulrc = FALSE;
    XWPTrashCan *pDefaultTrashCan = _xwpclsQueryDefaultTrashCan(_XWPTrashCan);
    if (pDefaultTrashCan)
    {
        ulrc = _xwpEmptyTrashCan(pDefaultTrashCan,
                                 hab,
                                 pulDeleted,
                                 hwndConfirmOwner);
    }

    return (ulrc);
}

/* ******************************************************************
 *
 *   Miscellaneae
 *
 ********************************************************************/

/*
 *@@ cmnPlaySystemSound:
 *      this posts a msg to the XFolder Media thread to
 *      have it play a system sound. This does sufficient
 *      error checking and returns FALSE if playing the
 *      sound failed.
 *
 *      usIndex may be any of the MMSOUND_* values defined
 *      in helpers\syssound.h and shared\common.h.
 *
 *@@changed V0.9.3 (2000-04-10) [umoeller]: "Sounds" setting in XWPSetup wasn't respected; fixed
 */

BOOL cmnPlaySystemSound(USHORT usIndex)
{
    BOOL brc = FALSE;
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    if (pGlobalSettings->fXSystemSounds)    // V0.9.3 (2000-04-10) [umoeller]
    {
        PCKERNELGLOBALS pKernelGlobals = krnQueryGlobals();

        // check if the XWPMedia subsystem is working
        if (xmmQueryStatus() == MMSTAT_WORKING)
        {
            brc = xmmPostMediaMsg(XMM_PLAYSYSTEMSOUND,
                                  (MPARAM)usIndex,
                                  MPNULL);
        }
    }

    return (brc);
}

/*
 *@@ cmnQueryActiveDesktop:
 *      wrapper for wpclsQueryActiveDesktop. This
 *      has been implemented so that this method
 *      gets called only once (speed). Also, this
 *      saves us from including wpdesk.h in every
 *      source file.
 *
 *@@added V0.9.3 (2000-04-17) [umoeller]
 */

WPObject* cmnQueryActiveDesktop(VOID)
{
    return (_wpclsQueryActiveDesktop(_WPDesktop));
}

/*
 *@@ cmnQueryActiveDesktopHWND:
 *      wrapper for wpclsQueryActiveDesktopHWND. This
 *      has been implemented so that this method
 *      gets called only once (speed). Also, this
 *      saves us from including wpdesk.h in every
 *      source file.
 *
 *@@added V0.9.3 (2000-04-17) [umoeller]
 */

HWND cmnQueryActiveDesktopHWND(VOID)
{
    return (_wpclsQueryActiveDesktopHWND(_WPDesktop));
}

/*
 * PRODUCTINFODATA:
 *      small struct for QWL_USER in cmn_fnwpProductInfo.
 */

typedef struct _PRODUCTINFODATA
{
    HBITMAP hbm;
    POINTL  ptlBitmap;
} PRODUCTINFODATA, *PPRODUCTINFODATA;

/*
 *@@ cmn_fnwpProductInfo:
 *      dialog func which paints the XWP logo directly
 *      as a bitmap, instead of using a static control.
 *
 *@@added V0.9.5 (2000-10-07) [umoeller]
 */

MRESULT EXPENTRY cmn_fnwpProductInfo(HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    BOOL fCallDefault = TRUE;

    switch (msg)
    {
        case WM_INITDLG:
        {
            PPRODUCTINFODATA ppd = (PPRODUCTINFODATA)malloc(sizeof(PRODUCTINFODATA));
            if (ppd)
            {
                HPS hpsTemp = 0;
                memset(ppd, 0, sizeof(PRODUCTINFODATA));

                hpsTemp = WinGetPS(hwndDlg);
                if (hpsTemp)
                {
                    ppd->hbm = GpiLoadBitmap(hpsTemp,
                                             cmnQueryNLSModuleHandle(FALSE),
                                             ID_XFLDRBITMAP,
                                             0, 0); // no stretch
                    if (ppd->hbm)
                    {
                        HWND    hwndStatic = WinWindowFromID(hwndDlg, ID_XFD_PRODLOGO);
                        if (hwndStatic)
                        {
                            SWP     swpFrame;
                            BITMAPINFOHEADER2 bmih2;
                            bmih2.cbFix = sizeof(bmih2);
                            if (GpiQueryBitmapInfoHeader(ppd->hbm, &bmih2))
                            {
                                if (WinQueryWindowPos(hwndStatic,
                                                      &swpFrame))
                                {
                                    ppd->ptlBitmap.x = swpFrame.x
                                                       + ((swpFrame.cx - (LONG)bmih2.cx) / 2);
                                    ppd->ptlBitmap.y = swpFrame.y
                                                       + ((swpFrame.cy - (LONG)bmih2.cy) / 2);
                                }

                                WinDestroyWindow(hwndStatic);
                            }
                        }
                    }

                    WinReleasePS(hpsTemp);
                }
            }

            WinSetWindowPtr(hwndDlg, QWL_USER, ppd);
        break; }

        case WM_PAINT:
        {
            HPS hpsPaint = 0;
            mrc = cmn_fnwpDlgWithHelp(hwndDlg, msg, mp1, mp2);

            hpsPaint = WinGetPS(hwndDlg);
            if (hpsPaint)
            {
                PPRODUCTINFODATA ppd = (PPRODUCTINFODATA)WinQueryWindowPtr(hwndDlg, QWL_USER);
                if (ppd)
                {
                    if (ppd->hbm)
                    {
                        POINTL ptlDest;
                        ptlDest.x = ppd->ptlBitmap.x;
                        ptlDest.y = ppd->ptlBitmap.y;
                        WinDrawBitmap(hpsPaint,
                                      ppd->hbm,
                                      NULL,
                                      &ptlDest,
                                      0, 0, DBM_NORMAL);
                    }
                }

                WinReleasePS(hpsPaint);
            }
        break; }

        case WM_DESTROY:
        {
            PPRODUCTINFODATA ppd = (PPRODUCTINFODATA)WinQueryWindowPtr(hwndDlg, QWL_USER);
            if (ppd)
            {
                if (ppd->hbm)
                    GpiDeleteBitmap(ppd->hbm);
                free(ppd);
            }
        break; }
    }

    if (fCallDefault)
        mrc = cmn_fnwpDlgWithHelp(hwndDlg, msg, mp1, mp2);

    return (mrc);
}

/*
 *@@ cmnShowProductInfo:
 *      shows the XWorkplace "Product info" dlg.
 *      This calls WinProcessDlg in turn.
 *
 *@@added V0.9.1 (2000-02-13) [umoeller]
 *@@changed V0.9.5 (2000-10-07) [umoeller]: now using cmn_fnwpProductInfo
 */

VOID cmnShowProductInfo(ULONG ulSound) // in: sound intex to play
{
    // advertise for myself
    CHAR    szGPLInfo[2000];
    PSZ     pszGPLInfo;
    LONG    lBackClr = CLR_WHITE;
    HWND hwndInfo = WinLoadDlg(HWND_DESKTOP, HWND_DESKTOP,
                               cmn_fnwpProductInfo,
                               cmnQueryNLSModuleHandle(FALSE),
                               ID_XFD_PRODINFO,
                               NULL),
         hwndTextView;

    // text view (GPL info)
    txvRegisterTextView(WinQueryAnchorBlock(hwndInfo));
    hwndTextView = txvReplaceWithTextView(hwndInfo,
                                          ID_XLDI_TEXT2,
                                          WS_VISIBLE | WS_TABSTOP,
                                          XTXF_VSCROLL,
                                          2);
    WinSendMsg(hwndTextView, TXM_SETWORDWRAP, (MPARAM)TRUE, 0);
    WinSetPresParam(hwndTextView,
                    PP_BACKGROUNDCOLOR,
                    sizeof(ULONG),
                    &lBackClr);

    cmnSetControlsFont(hwndInfo, 1, 10000);

    cmnPlaySystemSound(ulSound);

    // load and convert info text
    cmnGetMessage(NULL, 0,
                  szGPLInfo, sizeof(szGPLInfo),
                  140);
    pszGPLInfo = strdup(szGPLInfo);
    txvStripLinefeeds(&pszGPLInfo, 4);
    WinSetWindowText(hwndTextView, pszGPLInfo);
    free(pszGPLInfo);

    // version string
    sprintf(szGPLInfo, "XWorkplace V%s (%s)", BLDLEVEL_VERSION, __DATE__);
    WinSetDlgItemText(hwndInfo, ID_XFDI_XFLDVERSION, szGPLInfo);

    cmnSetDlgHelpPanel(0);
    winhCenterWindow(hwndInfo);
    WinProcessDlg(hwndInfo);
    WinDestroyWindow(hwndInfo);
}

/*
 *@@ cmnQueryDefaultFont:
 *      this returns the font to be used for dialogs.
 *      If the "Use 8.Helv" checkbox is enabled on
 *      the "Paranoia" page, we return "8.Helv",
 *      otherwise "9.WarpSans". The returned font
 *      string is static, so don't attempt to free it.
 *
 *@@added V0.9.0 [umoeller]
 */

const char* cmnQueryDefaultFont(VOID)
{
    if (G_pGlobalSettings->fUse8HelvFont)
        return ("8.Helv");
    else
        return ("9.WarpSans");
}

/*
 *@@ cmnSetControlsFont:
 *      this sets the font presentation parameters for a dialog
 *      window. See winhSetControlsFont for the parameters.
 *      This calls cmnQueryDefaultFont in turn.
 *
 *@@added V0.9.0 [umoeller]
 *@@changed V0.9.7 (2000-12-13) [umoeller]: removed krnLock(), which wasn't needed here
 */

VOID cmnSetControlsFont(HWND hwnd,
                        SHORT usIDMin,
                        SHORT usIDMax)
{
    winhSetControlsFont(hwnd,
                        usIDMin,
                        usIDMax,
                        (PSZ)cmnQueryDefaultFont());
}

PFNWP pfnwpOrigStatic = NULL;

/*
 *@@ fnwpAutoSizeStatic:
 *      dlg proc for the subclassed static text control in msg box;
 *      automatically resizes the msg box window when text changes.
 *
 *@@changed V0.9.0 [umoeller]: the default font is now queried using cmnQueryDefaultFont
 */

MRESULT EXPENTRY fnwpAutoSizeStatic(HWND hwndStatic, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = NULL;
    switch (msg)
    {

        /*
         * XM_SETLONGTEXT:
         *      this arrives here when the window text changes,
         *      especially when fnwpMessageBox sets the window
         *      text; we will now reposition all the controls.
         *      mp1 is a PSZ to the new text.
         */

        case XM_SETLONGTEXT:
        {
            CHAR szFont[300];
            PSZ p = (PSZ)WinQueryWindowULong(hwndStatic, QWL_USER);
            if (p)
                free(p);
            p = strdup((PSZ)mp1);
            WinSetWindowULong(hwndStatic, QWL_USER, (ULONG)p);
            PrfQueryProfileString(HINI_USER,
                                  (PSZ)INIAPP_XWORKPLACE,
                                  (PSZ)INIKEY_DLGFONT,
                                  (PSZ)cmnQueryDefaultFont(),
                                  szFont, sizeof(szFont)-1);
            WinSetPresParam(hwndStatic, PP_FONTNAMESIZE,
                            (ULONG)strlen(szFont) + 1,
                            (PVOID)szFont);
            // this will also update the display
        break; }

        /*
         * WM_PRESPARAMCHANGED:
         *      if a font has been dropped on the window, store
         *      the information and enforce control repositioning
         *      also
         */

        case WM_PRESPARAMCHANGED:
        {
            // _Pmpf(( "PRESPARAMCHANGED" ));
            switch ((ULONG)mp1)
            {
                case PP_FONTNAMESIZE:
                {
                    ULONG attrFound; //, abValue[32];
                    CHAR  szFont[100];
                    HWND  hwndDlg = WinQueryWindow(hwndStatic, QW_PARENT);
                    WinQueryPresParam(hwndStatic,
                                PP_FONTNAMESIZE,
                                0,
                                &attrFound,
                                (ULONG)sizeof(szFont),
                                (PVOID)&szFont,
                                0);
                    PrfWriteProfileString(HINI_USER,
                                          (PSZ)INIAPP_XWORKPLACE,
                                          (PSZ)INIKEY_DLGFONT,
                                          szFont);

                    // now also change the buttons
                    WinSetPresParam(WinWindowFromID(hwndDlg, 1),
                                    PP_FONTNAMESIZE,
                                    (ULONG)strlen(szFont) + 1, (PVOID)szFont);
                    WinSetPresParam(WinWindowFromID(hwndDlg, 2),
                                    PP_FONTNAMESIZE,
                                    (ULONG)strlen(szFont) + 1, (PVOID)szFont);
                    WinSetPresParam(WinWindowFromID(hwndDlg, 3),
                                    PP_FONTNAMESIZE,
                                    (ULONG)strlen(szFont) + 1, (PVOID)szFont);

                    WinPostMsg(hwndStatic, XM_UPDATE, MPNULL, MPNULL);
                break; }
            }
        break; }

        /*
         * XM_UPDATE:
         *      this actually does all the calculations to reposition
         *      all the msg box controls
         */

        case XM_UPDATE:
        {
            RECTL   rcl, rcl2;
            SWP     swp;
            HWND    hwndIcon, hwndDlg;
            PSZ     pszText;

            HPS hps = WinGetPS(hwndStatic);

            // _Pmpf(( "XM_UPDATE" ));

            pszText = (PSZ)WinQueryWindowULong(hwndStatic, QWL_USER);
            if (pszText)
            {
                WinQueryWindowRect(hwndStatic, &rcl);         // get window dimensions
                // rcl.yTop = 1000;
             winhDrawFormattedText(hps,
                                      &rcl,
                                      pszText,
                                      DT_LEFT | DT_TOP | DT_QUERYEXTENT);
                WinEndPaint(hps);

                if ((rcl.yTop - rcl.yBottom) < 40)
                    rcl.yBottom -= 40 - (rcl.yTop - rcl.yBottom);

                // printfRtl("  Original rect:", &rcl2);
                // printfRtl("  New rect:     ", &rcl);

                // reposition the text
                WinQueryWindowPos(hwndStatic, &swp);
                swp.cy -= rcl.yBottom;
                WinSetWindowPos(hwndStatic, 0,
                                swp.x, swp.y, swp.cx, swp.cy,
                                SWP_SIZE);

                // reposition the icon
                hwndIcon = WinWindowFromID(WinQueryWindow(hwndStatic, QW_PARENT),
                                           ID_XFDI_GENERICDLGICON);
                WinQueryWindowPos(hwndIcon, &swp);
                swp.y -= rcl.yBottom;
                WinSetWindowPos(hwndIcon, 0,
                                swp.x, swp.y, swp.cx, swp.cy,
                                SWP_MOVE);

                // resize the dlg frame window
                hwndDlg = WinQueryWindow(hwndStatic, QW_PARENT);
                WinQueryWindowPos(hwndDlg, &swp);
                swp.cy -= rcl.yBottom;
                swp.y  += (rcl.yBottom / 2);
                WinInvalidateRect(hwndDlg, NULL, FALSE);
                WinSetWindowPos(hwndDlg, 0,
                                swp.x, swp.y, swp.cx, swp.cy,
                                SWP_SIZE | SWP_MOVE);
            }
        break; }

        /*
         * WM_PAINT:
         *      draw the formatted text
         */

        case WM_PAINT:
        {
            RECTL   rcl;
            PSZ pszText = (PSZ)WinQueryWindowULong(hwndStatic, QWL_USER);
            HPS hps = WinBeginPaint(hwndStatic, NULLHANDLE, &rcl);
            if (pszText)
            {
                WinQueryWindowRect(hwndStatic, &rcl);         // get window dimensions
                // switch to RGB mode
                GpiCreateLogColorTable(hps, 0, LCOLF_RGB, 0, 0, NULL);
                // set "static text" color
                GpiSetColor(hps,
                            WinQuerySysColor(HWND_DESKTOP,
                                             SYSCLR_WINDOWSTATICTEXT,
                                             0));
                winhDrawFormattedText(hps, &rcl, pszText, DT_LEFT | DT_TOP);
            }
            WinEndPaint(hps);
        break; }

        case WM_DESTROY:
        {
            PSZ pszText = (PSZ)WinQueryWindowULong(hwndStatic, QWL_USER);
            free(pszText);
            WinSubclassWindow(hwndStatic, pfnwpOrigStatic);
            mrc = (*pfnwpOrigStatic)(hwndStatic, msg, mp1, mp2);
        break; }

        default:
            mrc = (*pfnwpOrigStatic)(hwndStatic, msg, mp1, mp2);
    }

    return (mrc);
}

/*
 *@@ cmnLoadMessageBoxDlg:
 *      wrapper to load the common message box dialog
 *      (WinLoadDlg) and set up all controls according
 *      to the given flags.
 *
 *      This only sets up the windows. This does not
 *      yet call WinProcessDlg.
 *
 *      The complete call sequence should be:
 +
 +          ULONG ulAlarmFlag,
 +                ulrc
 +          HWND hwndDlg = cmnLoadMessageBoxDlg(hwndOwner,
 +                                              pszTitle,
 +                                              pszMessage,
 +                                              flStyle,
 +                                              &ulAlarmFlag);
 +
 +          ulrc = cmnProcessMessageBox(hwndDlg,
 +                                      ulAlarmFlag,
 +                                      flStyle);
 +          WinDestroyWindow(hwndDlg);
 *
 *      Use this function if cmnMessageBox does too
 *      much for you, especially if you wish to add
 *      additional controls to the message box window.
 *      You can also subclass the dialog window. QWL_USER
 *      is not used.
 *
 *      The dialog has the following controls:
 *
 *      -- ICON ID_XFDI_GENERICDLGICON
 *
 *      -- LTEXT ID_XFDI_GENERICDLGTEXT (subclassed with fnwpAutoSizeStatic)
 *
 *      -- PUSHBUTTON DID_OK (always visible, NLS text is set by this function
 *                    according to flStyle)
 *
 *      -- PUSHBUTTON DID_CANCEL (might be invisible, NLS text is set by this
 *                    function according to flStyle)
 *
 *      -- PUSHBUTTON 3 (might be invisible, NLS text is set by this function
 *                    according to flStyle)
 *
 *@@added V0.9.3 (2000-05-05) [umoeller]
 *@@changed V0.9.3 (2000-05-05) [umoeller]: removed fnwpMessageBox
 *@@changed V0.9.4 (2000-07-27) [umoeller]: added "yes to all"
 */

HWND cmnLoadMessageBoxDlg(HWND hwndOwner,
                          PSZ pszTitle,
                          PSZ pszMessage,
                          ULONG flStyle,
                          PULONG pulAlarmFlag)  // out: alarm sound to be played
{
    HAB     hab = WinQueryAnchorBlock(hwndOwner);
    HMODULE hmod = cmnQueryNLSModuleHandle(FALSE);
    PSZ     pszButton = NULL;
    ULONG flButtons = flStyle & 0xF;        // low nibble contains MB_YESNO etc.
    ULONG ulDefaultButtonID = 1;

    HWND hwndDlg = WinLoadDlg(HWND_DESKTOP, hwndOwner,
                              WinDefDlgProc,
                              hmod,
                              ID_XFD_GENERICDLG,
                              NULL);

    HWND hwndStatic = WinWindowFromID(hwndDlg, ID_XFDI_GENERICDLGTEXT);
    // set string to 0
    WinSetWindowULong(hwndStatic, QWL_USER, (ULONG)NULL);
    // subclass static control
    pfnwpOrigStatic = WinSubclassWindow(hwndStatic,
                                        fnwpAutoSizeStatic);

    *pulAlarmFlag = WA_NOTE; // default sound for WinAlarm

    // now work on the three buttons of the dlg template:
    // give them proper titles or hide them
    if (flButtons == MB_OK)
    {
        cmnLoadString(hab, hmod, ID_XSSI_DLG_OK, &pszButton);
        WinSetDlgItemText(hwndDlg, 1, pszButton);
        winhShowDlgItem(hwndDlg, 2, FALSE);
        winhShowDlgItem(hwndDlg, 3, FALSE);
        free(pszButton);
    }
    else if (flButtons == MB_OKCANCEL)
    {
        cmnLoadString(hab, hmod, ID_XSSI_DLG_OK, &pszButton);
        WinSetDlgItemText(hwndDlg, 1, pszButton);
        cmnLoadString(hab, hmod, ID_XSSI_DLG_CANCEL, &pszButton);
        WinSetDlgItemText(hwndDlg, 2, pszButton);
        winhShowDlgItem(hwndDlg, 3, FALSE);
        free(pszButton);
    }
    else if (flButtons == MB_RETRYCANCEL)
    {
        cmnLoadString(hab, hmod, ID_XSSI_DLG_RETRY, &pszButton);
        WinSetDlgItemText(hwndDlg, 1, pszButton);
        cmnLoadString(hab, hmod, ID_XSSI_DLG_CANCEL, &pszButton);
        WinSetDlgItemText(hwndDlg, 2, pszButton);
        winhShowDlgItem(hwndDlg, 3, FALSE);
        free(pszButton);
    }
    else if (flButtons == MB_ABORTRETRYIGNORE)
    {
        cmnLoadString(hab, hmod, ID_XSSI_DLG_ABORT, &pszButton);
        WinSetDlgItemText(hwndDlg, 1, pszButton);
        cmnLoadString(hab, hmod, ID_XSSI_DLG_RETRY, &pszButton);
        WinSetDlgItemText(hwndDlg, 2, pszButton);
        cmnLoadString(hab, hmod, ID_XSSI_DLG_IGNORE, &pszButton);
        WinSetDlgItemText(hwndDlg, 3, pszButton);
        free(pszButton);
    }
    else if (flButtons == MB_YESNO)
    {
        cmnLoadString(hab, hmod, ID_XSSI_DLG_YES, &pszButton);
        WinSetDlgItemText(hwndDlg, 1, pszButton);
        cmnLoadString(hab, hmod, ID_XSSI_DLG_NO, &pszButton);
        WinSetDlgItemText(hwndDlg, 2, pszButton);
        winhShowDlgItem(hwndDlg, 3, FALSE);
        free(pszButton);
    }
    else if (flButtons == MB_YESNOCANCEL)
    {
        cmnLoadString(hab, hmod, ID_XSSI_DLG_YES, &pszButton);
        WinSetDlgItemText(hwndDlg, 1, pszButton);
        cmnLoadString(hab, hmod, ID_XSSI_DLG_NO, &pszButton);
        WinSetDlgItemText(hwndDlg, 2, pszButton);
        cmnLoadString(hab, hmod, ID_XSSI_DLG_CANCEL, &pszButton);
        WinSetDlgItemText(hwndDlg, 3, pszButton);
        free(pszButton);
    }
    else if (flButtons == MB_CANCEL)
    {
        cmnLoadString(hab, hmod, ID_XSSI_DLG_CANCEL, &pszButton);
        WinSetDlgItemText(hwndDlg, 1, pszButton);
        winhShowDlgItem(hwndDlg, 2, FALSE);
        winhShowDlgItem(hwndDlg, 3, FALSE);
        free(pszButton);
    }
    // else if (flButtons == MB_ENTER)
    // else if (flButtons == MB_ENTERCANCEL)
    else if (flButtons == MB_YES_YES2ALL_NO)
    {
        cmnLoadString(hab, hmod, ID_XSSI_DLG_YES, &pszButton);
        WinSetDlgItemText(hwndDlg, 1, pszButton);
        cmnLoadString(hab, hmod, ID_XSSI_DLG_YES2ALL, &pszButton);
        WinSetDlgItemText(hwndDlg, 2, pszButton);
        cmnLoadString(hab, hmod, ID_XSSI_DLG_NO, &pszButton);
        WinSetDlgItemText(hwndDlg, 3, pszButton);
    }

    // unset predefined default button
    WinSetWindowBits(WinWindowFromID(hwndDlg, 1), QWL_STYLE,
                     0,
                     BS_DEFAULT);

    // query default button IDs
    if (flStyle & MB_DEFBUTTON2)
        ulDefaultButtonID = 2;
    else if (flStyle & MB_DEFBUTTON3)
        ulDefaultButtonID = 3;

    // set new default button
    {
        HWND    hwndDefaultButton = WinWindowFromID(hwndDlg, ulDefaultButtonID);
        WinSetWindowBits(hwndDefaultButton, QWL_STYLE,
                         BS_DEFAULT,
                         BS_DEFAULT);
        WinSetFocus(HWND_DESKTOP, hwndDefaultButton);
    }

    if (flStyle & (MB_ICONHAND | MB_ERROR))
        *pulAlarmFlag = WA_ERROR;
    else if (flStyle & (MB_ICONEXCLAMATION | MB_WARNING))
        *pulAlarmFlag = WA_WARNING;

    WinSetWindowText(hwndDlg, pszTitle);
    WinSendMsg(hwndStatic, XM_SETLONGTEXT, pszMessage, MPNULL);

    return (hwndDlg);
}

/*
 *@@ cmnProcessMessageBox:
 *      this invokes WinProcessDlg on the dialog
 *      and converts the return value into those
 *      MB_* values. See cmnLoadMessageBoxDlg for
 *      the call sequence.
 *
 *      This does _not_ call WinDestroyWindow.
 *
 *@@added V0.9.3 (2000-05-05) [umoeller]
 *@@changed V0.9.4 (2000-07-27) [umoeller]: added "yes to all"
 */

ULONG cmnProcessMessageBox(HWND hwndDlg,        // in: message box dialog
                           ULONG ulAlarmFlag,
                           ULONG flStyle)
{
    ULONG   ulrc = MB_CANCEL,
            ulrcDlg = 0;
    ULONG   flButtons = flStyle & 0xF;        // low nibble contains MB_YESNO etc.

    winhCenterWindow(hwndDlg);
    if (flStyle & MB_SYSTEMMODAL)
        WinSetSysModalWindow(HWND_DESKTOP, hwndDlg);
    WinAlarm(HWND_DESKTOP, ulAlarmFlag);

    // go!!!
    ulrcDlg = WinProcessDlg(hwndDlg);

    if (flButtons == MB_OK)
        ulrc = MBID_OK;
    else if (flButtons == MB_OKCANCEL)
        switch (ulrcDlg)
        {
            case 1:     ulrc = MBID_OK; break;
            default:    ulrc = MBID_CANCEL;  break;
        }
    else if (flButtons == MB_RETRYCANCEL)
        switch (ulrcDlg)
        {
            case 1:     ulrc = MBID_RETRY; break;
            default:    ulrc = MBID_CANCEL;  break;
        }
    else if (flButtons == MB_ABORTRETRYIGNORE)
        switch (ulrcDlg)
        {
            case 2:     ulrc = MBID_RETRY;  break;
            case 3:     ulrc = MBID_IGNORE; break;
            default:    ulrc = MBID_ABORT; break;
        }
    else if (flButtons == MB_YESNO)
        switch (ulrcDlg)
        {
            case 1:     ulrc = MBID_YES; break;
            default:    ulrc = MBID_NO;  break;
        }
    else if (flButtons == MB_YESNOCANCEL)
        switch (ulrcDlg)
        {
            case 1:     ulrc = MBID_YES; break;
            case 2:     ulrc = MBID_NO; break;
            default:    ulrc = MBID_CANCEL;  break;
        }
    else if (flButtons == MB_CANCEL)
        ulrc = MBID_CANCEL;
    // else if (flButtons == MB_ENTER)
    // else if (flButtons == MB_ENTERCANCEL)
    else if (flButtons == MB_YES_YES2ALL_NO)
        switch (ulrcDlg)
        {
            case 1:     ulrc = MBID_YES; break;
            case 2:     ulrc = MBID_YES2ALL; break;
            default:    ulrc = MBID_NO; break;
        }

    return (ulrc);
}

/*
 *@@ cmnMessageBox:
 *      this is the generic function for displaying XFolder
 *      message boxes. This is very similar to WinMessageBox,
 *      but a few new features are introduced:
 *
 *      -- an XFolder icon is displayed;
 *
 *      -- fonts can be dropped on the window, upon which the
 *         window will resize itself and store the font in OS2.INI.
 *
 *      Currently the following flStyle's are supported:
 *
 *      -- MB_OK                      0x0000
 *      -- MB_OKCANCEL                0x0001
 *      -- MB_RETRYCANCEL             0x0002
 *      -- MB_ABORTRETRYIGNORE        0x0003
 *      -- MB_YESNO                   0x0004
 *      -- MB_YESNOCANCEL             0x0005
 *      -- MB_CANCEL                  0x0006
 *      -- MB_ENTER                   0x0007 (not implemented yet)
 *      -- MB_ENTERCANCEL             0x0008 (not implemented yet)
 *
 *      -- MB_YES_YES2ALL_NO          0x0009
 *          This is new: this has three buttons called "Yes"
 *          (MBID_YES), "Yes to all" (MBID_YES2ALL), "No" (MBID_NO).
 *
 *      -- MB_DEFBUTTON2            (for two-button styles)
 *      -- MB_DEFBUTTON3            (for three-button styles)
 *
 *      -- MB_ICONHAND
 *      -- MB_ICONEXCLAMATION
 *
 *      Returns MBID_* codes like WinMessageBox.
 *
 *@@changed V0.9.0 [umoeller]: added support for MB_YESNOCANCEL
 *@@changed V0.9.0 [umoeller]: fixed default button bugs
 *@@changed V0.9.0 [umoeller]: added WinAlarm sound support
 *@@changed V0.9.3 (2000-05-05) [umoeller]: extracted cmnLoadMessageBoxDlg
 */

ULONG cmnMessageBox(HWND hwndOwner,     // in: owner
                    PSZ pszTitle,       // in: msgbox title
                    PSZ pszMessage,     // in: msgbox text
                    ULONG flStyle)      // in: MB_* flags
{
    ULONG   ulrc = DID_CANCEL;

    // set our extended exception handler
    TRY_LOUD(excpt1)
    {
        ULONG ulAlarmFlag = 0;
        HWND hwndDlg = cmnLoadMessageBoxDlg(hwndOwner,
                                            pszTitle,
                                            pszMessage,
                                            flStyle,
                                            &ulAlarmFlag);

        ulrc = cmnProcessMessageBox(hwndDlg,
                                    ulAlarmFlag,
                                    flStyle);
        WinDestroyWindow(hwndDlg);
    }
    CATCH(excpt1) { } END_CATCH();

    return (ulrc);
}

/*
 *@@ cmnGetMessageExt:
 *      retrieves a message string from the XWorkplace
 *      TMF message file. The message is specified
 *      using the TMF message ID string directly.
 *      This gets called from cmnGetMessage.
 *
 *@@added V0.9.4 (2000-06-17) [umoeller]
 */

APIRET cmnGetMessageExt(PCHAR *pTable,     // in: replacement PSZ table or NULL
                        ULONG ulTable,     // in: size of that table or 0
                        PSZ pszBuf,        // out: buffer to hold message string
                        ULONG cbBuf,       // in: size of pszBuf
                        PSZ pszMsgID)      // in: msg ID to retrieve
{
    APIRET  arc = NO_ERROR;

    TRY_LOUD(excpt1)
    {
        const char *pszMessageFile = cmnQueryMessageFile();
        ULONG   ulReturned;

        #ifdef DEBUG_LANGCODES
            _Pmpf(("cmnGetMessage %s %s", pszMessageFile, pszMsgId));
        #endif

        arc = tmfGetMessage(pTable,
                            ulTable,
                            pszBuf,
                            cbBuf,
                            pszMsgID,      // string (!) message identifier
                            (PSZ)pszMessageFile,     // .TMF file
                            &ulReturned);

        #ifdef DEBUG_LANGCODES
            _Pmpf(("  tmfGetMessage rc: %d", arc));
        #endif

        if (arc == NO_ERROR)
        {
            pszBuf[ulReturned] = '\0';

            // remove trailing newlines
            while (TRUE)
            {
                PSZ p = pszBuf + strlen(pszBuf) - 1;
                if (    (*p == '\n')
                     || (*p == '\r')
                   )
                    *p = '\0';
                else
                    break; // while (TRUE)
            }
        }
        else
            sprintf(pszBuf, "Message %s not found in %s",
                            pszMsgID, pszMessageFile);
    }
    CATCH(excpt1) { } END_CATCH();

    return (arc);
}

/*
 *@@ cmnGetMessage:
 *      like DosGetMessage, but automatically uses the
 *      (NLS) XFolder message file.
 *      The parameters are exactly like with DosGetMessage.
 *      The message code (ulMsgNumber) is automatically
 *      converted to a TMF message ID.
 *
 *      <B>Returns:</B> the error code of tmfGetMessage.
 *
 *@@changed V0.9.0 [umoeller]: changed, this now uses the TMF file format (tmsgfile.c).
 *@@changed V0.9.4 (2000-06-18) [umoeller]: extracted cmnGetMessageExt
 */

APIRET cmnGetMessage(PCHAR *pTable,     // in: replacement PSZ table or NULL
                     ULONG ulTable,     // in: size of that table or 0
                     PSZ pszBuf,        // out: buffer to hold message string
                     ULONG cbBuf,       // in: size of pszBuf
                     ULONG ulMsgNumber) // in: msg number to retrieve
{
    CHAR szMessageName[40];
    // create string message identifier from ulMsgNumber
    sprintf(szMessageName, "XFL%04d", ulMsgNumber);

    return (cmnGetMessageExt(pTable, ulTable, pszBuf, cbBuf, szMessageName));
}

/*
 *@@ cmnMessageBoxMsg:
 *      calls cmnMessageBox, but this one accepts ULONG indices
 *      into the XFolder message file (XFLDRxxx.MSG) instead
 *      of real PSZs. This calls cmnGetMessage for retrieving
 *      the messages, but placeholder replacement does not work
 *      here (use cmnMessageBoxMsgExt for that).
 */

ULONG cmnMessageBoxMsg(HWND hwndOwner,
                       ULONG ulTitle,       // in: msg index for dlg title
                       ULONG ulMessage,     // in: msg index for message
                       ULONG flStyle)       // in: like cmnMsgBox
{
    CHAR    szTitle[200], szMessage[2000];

    cmnGetMessage(NULL, 0,
                  szTitle, sizeof(szTitle)-1,
                  ulTitle);
    cmnGetMessage(NULL, 0,
                  szMessage, sizeof(szMessage)-1,
                  ulMessage);

    return (cmnMessageBox(hwndOwner, szTitle, szMessage, flStyle));
}

/*
 *@@ cmnMessageBoxMsgExt:
 *      like cmnMessageBoxMsg, but with string substitution
 *      (see cmnGetMessage for more); substitution only
 *      takes place for the message specified with ulMessage,
 *      not for the title.
 */

ULONG cmnMessageBoxMsgExt(HWND hwndOwner,   // in: owner window
                          ULONG ulTitle,    // in: msg number for title
                          PCHAR *pTable,    // in: replacement table for ulMessage
                          ULONG ulTable,    // in: array count in *pTable
                          ULONG ulMessage,  // in: msg number for message
                          ULONG flStyle)    // in: msg box style flags (cmnMessageBox)
{
    CHAR    szTitle[200], szMessage[2000];

    cmnGetMessage(NULL, 0,
                  szTitle, sizeof(szTitle)-1,
                  ulTitle);
    cmnGetMessage(pTable, ulTable,
                  szMessage, sizeof(szMessage)-1,
                  ulMessage);

    return (cmnMessageBox(hwndOwner, szTitle, szMessage, flStyle));
}

/*
 *@@ cmnDosErrorMsgBox:
 *      displays a DOS error message.
 *      This calls cmnMessageBox in turn.
 *
 *@@added V0.9.1 (2000-02-08) [umoeller]
 *@@changed V0.9.3 (2000-04-09) [umoeller]: added error explanation
 */

ULONG cmnDosErrorMsgBox(HWND hwndOwner,     // in: owner window.
                        CHAR cDrive,        // in: drive letter
                        PSZ pszTitle,       // in: msgbox title
                        APIRET arc,         // in: DOS error code to get msg for
                        ULONG ulFlags,      // in: as in cmnMessageBox flStyle
                        BOOL fShowExplanation) // in: if TRUE, we'll retrieve an explanation as with the HELP command
{
    ULONG   mbrc = 0;
    CHAR    szError[1000],
            szError2[2000];
    ULONG   ulLen = 0;
    APIRET  arc2 = NO_ERROR;

    // get error message for APIRET
    CHAR    szDrive[3] = "?:";
    PSZ     pszTable = szDrive;
    szDrive[0] = cDrive;

    arc2 = DosGetMessage(&pszTable, 1,
                         szError, sizeof(szError),
                         arc,
                         "OSO001.MSG",        // default OS/2 message file
                         &ulLen);
    szError[ulLen] = 0;

    if (arc2 != NO_ERROR)
    {
        sprintf(szError2,
                "%s: DosGetMessage returned error %d",
                __FUNCTION__, arc2);
    }
    else
    {
        sprintf(szError2, "(%d) %s", arc, szError);

        if (fShowExplanation)
        {
            CHAR szErrorExpl[1000];
            arc2 = DosGetMessage(NULL, 0,
                                 szErrorExpl, sizeof(szErrorExpl),
                                 arc,
                                 "OSO001H.MSG",        // default OS/2 help message file
                                 &ulLen);
            if (arc2 == NO_ERROR)
            {
                szErrorExpl[ulLen] = 0;
                strcat(szError2, "\n");
                strcat(szError2, szErrorExpl);
            }
        }
    }

    mbrc = cmnMessageBox(HWND_DESKTOP,
                         pszTitle,
                         szError2,
                         ulFlags);
    return (mbrc);
}

/*
 *@@ cmnSetDlgHelpPanel:
 *      sets help panel before calling fnwpDlgGeneric.
 */

VOID cmnSetDlgHelpPanel(ULONG ulHelpPanel)
{
    G_ulCurHelpPanel = ulHelpPanel;
}

/*
 *@@  cmn_fnwpDlgWithHelp:
 *          this is the dlg procedure for XFolder dlg boxes;
 *          it can process WM_HELP messages. All other messages
 *          are passed to WinDefDlgProc.
 *
 *          Use cmnSetDlgHelpPanel to set the help panel before
 *          using this dlg proc.
 *
 *@@changed V0.9.2 (2000-03-04) [umoeller]: renamed from fnwpDlgGeneric
 */

MRESULT EXPENTRY cmn_fnwpDlgWithHelp(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = NULL;

    switch (msg)
    {
        case WM_HELP:
        {
            // HMODULE hmodResource = cmnQueryNLSModuleHandle(FALSE);
            /* WM_HELP is received by this function when F1 or a "help" button
               is pressed in a dialog window. */
            if (G_ulCurHelpPanel > 0)
            {
                WPObject    *pHelpSomSelf = cmnQueryActiveDesktop();
                /* ulCurHelpPanel is set by instance methods before creating a
                   dialog box in order to link help topics to the displayed
                   dialog box. Possible values are:
                        0: open online reference ("<XWP_REF>", INF book)
                      > 0: open help topic in xfldr.hlp
                       -1: ignore WM_HELP */
                if (pHelpSomSelf)
                {
                    const char* pszHelpLibrary;
                    BOOL fProcessed = FALSE;
                    if (pszHelpLibrary = cmnQueryHelpLibrary())
                        // path found: display help panel
                        if (_wpDisplayHelp(pHelpSomSelf, G_ulCurHelpPanel, (PSZ)pszHelpLibrary))
                            fProcessed = TRUE;

                    if (!fProcessed)
                        cmnMessageBoxMsg(HWND_DESKTOP, 104, 134, MB_OK);
                }
            }
            else if (G_ulCurHelpPanel == 0)
            {
                HOBJECT     hobjRef = 0;
                // open online reference
                // G_ulCurHelpPanel = -1; // ignore further WM_HELP messages: this one suffices
                hobjRef = WinQueryObject((PSZ)XFOLDER_USERGUIDE);
                if (hobjRef)
                    WinOpenObject(hobjRef, OPEN_DEFAULT, TRUE);
                else
                    cmnMessageBoxMsg(HWND_DESKTOP, 104, 137, MB_OK);

            } // end else; if ulCurHelpPanel is < 0, nothing happens
            mrc = NULL;
        break; } // end case WM_HELP

        default:
            mrc = WinDefDlgProc(hwnd, msg, mp1, mp2);
        break;
    }

    return (mrc);
}

