
/*
 *@@sourcefile program.c:
 *      various program-related implementation code
 *      (especially WPProgram and WPProgramFile).
 *
 *      Presently, this implements:
 *
 *      -- Starting an executable via WinStartApp
 *         from a program object, with full in-use
 *         management and session handling. See
 *         progOpenProgram.
 *
 *      Roadmap:
 *
 *      WPProgram and WPProgramFile need to be rewritten
 *      entirely since they call into the WPS-internal
 *      file-handles routines routines all the time.
 *
 *      Rewriting them will require the following steps:
 *
 *      1)  Override and rewrite all methods which appear
 *          to call into the file handles routines. The
 *          rewrites are required to only call
 *          wpQueryProgDetails, wpSetProgDetails, and
 *          wpQueryHandle.
 *
 *      2)  After the file handles cache has been replaced
 *          at some point in the future, replace
 *          those three methods with new implementations
 *          which call the new XWP file handles routines.
 *
 *      This file is ALL new with V0.9.6.
 *
 *      Function prefix for this file:
 *      --  prog*
 *
 *@@added V0.9.6 [umoeller]
 *@@header "filesys\program.h"
 */

/*
 *      Copyright (C) 2000-2002 Ulrich M”ller.
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

#define INCL_DOSPROCESS
#define INCL_DOSEXCEPTIONS
#define INCL_DOSSEMAPHORES
#define INCL_DOSERRORS

#define INCL_WINMESSAGEMGR
#define INCL_WINPOINTERS
#define INCL_WINDIALOGS
#define INCL_WINMENUS
#define INCL_WINSTDCNR
#define INCL_WINPROGRAMLIST     // needed for wppgm.h
#define INCL_WINENTRYFIELDS
#define INCL_WINERRORS
#include <os2.h>

// C library headers
#include <stdio.h>
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h
#include <builtin.h>

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\apps.h"               // application helpers
#include "helpers\cnrh.h"               // container helper routines
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\exeh.h"               // executable helpers
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\standards.h"          // some standard macros
#include "helpers\stringh.h"            // string helper routines
#include "helpers\textview.h"           // PM XTextView control
#include "helpers\threads.h"            // thread helpers
#include "helpers\winh.h"               // PM helper routines
#include "helpers\xstring.h"            // extended string helpers

// SOM headers which don't crash with prec. header files
#include "xfobj.ih"
#include "xfdataf.ih"
#include "xfldr.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

#include "filesys\icons.h"              // icons handling
#include "filesys\object.h"             // XFldObject implementation

// other SOM headers
#pragma hdrstop
#include <wppgm.h>                      // WPProgram
#include <wppgmf.h>                     // WPProgramFile
#include "filesys\program.h"            // program implementation; WARNING: this redefines macros

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

static HMTX        G_hmtxRunning = NULLHANDLE;
static LINKLIST    G_llRunning;
        // linked list of running programs; contains RUNNINGPROGRAM structs

/* ******************************************************************
 *
 *   Helpers
 *
 ********************************************************************/

/*
 *@@ progQuerySetup:
 *      called to retrieve a setup string for programs.
 *
 *      Both XWPProgram and XWPProgramFile call
 *      fsysQueryProgramFileSetup, which calls this
 *      func in turn.
 *
 *@@added V0.9.4 (2000-08-02) [umoeller]
 *@@changed V0.9.9 (2001-04-02) [umoeller]: added a few more strings
 *@@changed V0.9.16 (2001-10-11) [umoeller]: moved this here from filesys.c
 */

BOOL progQuerySetup(WPObject *somSelf, // in: WPProgram or WPProgramFile
                    PVOID pstrSetup)   // in: string to append to (xstrcat)
{
    PSZ pszValue = NULL;
    // ULONG ulSize = 0;
    PPROGDETAILS    pDetails = NULL;

    // wpQueryProgDetails:
    // this works for both WPProgram and WPProgramFile; even though the two
    // methods are differently implemented, they both call the same implementation
    // in the WPS, so this is safe (famous last words)
    if ((pDetails = progQueryDetails(somSelf)))
    {
        // EXENAME: skip for WPProgramFile
        if (!_somIsA(somSelf, _WPProgramFile))
        {
            if (pDetails->pszExecutable)
            {
                PCSZ pcszProgString;

                xstrcat(pstrSetup, "EXENAME=", 0);
                xstrcat(pstrSetup, pDetails->pszExecutable, 0);
                xstrcatc(pstrSetup, ';');

                // add PROGTYPE=, unless this is PROG_DEFAULT
                // (moved code to helpers V0.9.16 (2001-10-06))
                if (pDetails->progt.progc != PROG_DEFAULT)
                    if (pcszProgString = appDescribeAppType(pDetails->progt.progc))
                    {
                        xstrcat(pstrSetup, "PROGTYPE=", 0);
                        xstrcat(pstrSetup, pcszProgString, 0);
                        xstrcatc(pstrSetup, ';');
                    }
            }
        } // end if (_somIsA(somSelf, _WPProgram)

        // PARAMETERS=
        if (pDetails->pszParameters)
            if (strlen(pDetails->pszParameters))
            {
                xstrcat(pstrSetup, "PARAMETERS=", 0);
                xstrcat(pstrSetup, pDetails->pszParameters, 0);
                xstrcatc(pstrSetup, ';');
            }

        // STARTUPDIR=
        if (pDetails->pszStartupDir)
            if (strlen(pDetails->pszStartupDir))
            {
                xstrcat(pstrSetup, "STARTUPDIR=", 0);
                xstrcat(pstrSetup, pDetails->pszStartupDir, 0);
                xstrcatc(pstrSetup, ';');
            }

        // SET XXX=VVV
        if (pDetails->pszEnvironment)
        {
            // this is one of those typical OS/2 environment
            // arrays, so lets parse this
            DOSENVIRONMENT Env = {0};
            if (appParseEnvironment(pDetails->pszEnvironment,
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

                        xstrcat(pstrSetup, "SET ", 0);
                        xstrcat(pstrSetup, pszThis, 0);
                        xstrcatc(pstrSetup, ';');

                        // next environment string
                        ppszThis++;
                    }
                }
                appFreeEnvironment(&Env);
            }
        }

        // following added V0.9.9 (2001-04-03) [umoeller]
        if (pDetails->swpInitial.fl & SWP_MAXIMIZE)
            xstrcat(pstrSetup, "MAXIMIZED=YES;", 0);
        else if (pDetails->swpInitial.fl & SWP_MINIMIZE)
            xstrcat(pstrSetup, "MINIMIZED=YES;", 0);

        if (pDetails->swpInitial.fl & SWP_NOAUTOCLOSE)
            xstrcat(pstrSetup, "NOAUTOCLOSE=YES;", 0);

        free(pDetails);

    } // end if _wpQueryProgDetails

    // ASSOCFILTER
    if ((pszValue = _wpQueryAssociationFilter(somSelf)))
            // wpQueryAssociationFilter:
            // supported by both WPProgram and WPProgramFile
    {
        xstrcat(pstrSetup, "ASSOCFILTER=", 0);
        xstrcat(pstrSetup, pszValue, 0);
        xstrcatc(pstrSetup, ';');
    }

    // ASSOCTYPE
    if ((pszValue = _wpQueryAssociationType(somSelf)))
            // wpQueryAssociationType:
            // supported by both WPProgram and WPProgramFile
    {
        xstrcat(pstrSetup, "ASSOCTYPE=", 0);
        xstrcat(pstrSetup, pszValue, 0);
        xstrcatc(pstrSetup, ';');
    }

    return TRUE;
}

/*
 *@@ progIsProgramOrProgramFile:
 *      returns
 *
 *      --  1 if somSelf is a WPProgram
 *
 *      --  2 if somSelf is a WPProgramFile
 *
 *      --  0 otherwise.
 *
 *@@added V0.9.16 (2001-10-19) [umoeller]
 */

ULONG progIsProgramOrProgramFile(WPObject *somSelf)
{
    if (_somIsA(somSelf, _WPProgram))
        return 1;

    if (_somIsA(somSelf, _WPProgramFile))
        return 2;

    return 0;
}

/*
 *@@ progQueryDetails:
 *      returns the PROGDETAILS of the specified
 *      object or NULL on errors.
 *
 *      This does check whether pProgObject is
 *      a WPProgram or WPProgramFile to avoid
 *      crashes.
 *
 *      Caller must free() the returned buffer.
 *
 *@@added V0.9.12 (2001-05-22) [umoeller]
 *@@changed V0.9.16 (2001-12-08) [umoeller]: now properly resolving method
 */

PPROGDETAILS progQueryDetails(WPObject *pProgObject)    // in: either WPProgram or WPProgramFile
{
    ULONG   ulSize = 0;
    PPROGDETAILS    pProgDetails = NULL;

    // make sure we really have a program
    if (progIsProgramOrProgramFile(pProgObject))
    {
        if (    (_wpQueryProgDetails(pProgObject,
                                     (PPROGDETAILS)NULL,        // query size
                                     &ulSize))
             && (pProgDetails = (PPROGDETAILS)malloc(ulSize))
           )
        {
            if (_wpQueryProgDetails(pProgObject,
                                    pProgDetails,
                                    &ulSize))
                return (pProgDetails);

            free(pProgDetails);
        }
    }

    return (NULL);
}

/*
 *@@ progFillProgDetails:
 *      fills a PROGDETAILS structure with the
 *      given data. This is the implementation
 *      for XWPProgramFile::wpQueryProgDetails
 *      and XWPProgram::wpQueryProgDetails.
 *
 *      If (pProgDetails == NULL), this sets
 *      *pulSize to the required size only.
 *      Otherwise pProgDetails is expected
 *      to point to a sufficiently large buffer,
 *      and *pulSize must have the size of that
 *      buffer on input too.
 *
 *      Returns FALSE if pcszExecutable is NULL
 *      or pProgDetails was specified, but the
 *      buffer size is too small.
 *
 *@@added V0.9.16 (2002-01-04) [umoeller]
 *@@changed V0.9.18 (2002-03-16) [umoeller]: now allowing NULL pcszExecutable
 */

BOOL progFillProgDetails(PPROGDETAILS pProgDetails,     // can be NULL
                         ULONG ulProgType,              // in: prog type
                         ULONG fbVisible,               // in: visibility flag
                         PSWP pSWPInitial,
                         PCSZ pcszTitle,                // in: object title (can be NULL)
                         PCSZ pcszExecutable,           // in: executable name (can be NULL)
                         USHORT usStartupDirHandle,      // in: 16-bit object handle for startup dir or NULLHANDLE
                         PCSZ pcszParameters,           // in: parameters or NULL
                         PCSZ pcszEnvironment,          // in: environment string in WinStartApp format or NULL
                         PULONG pulSize)                // in/out: buffer size
{
    BOOL        brc = FALSE;

    TRY_LOUD(excpt1)
    {
        CHAR        szStartupDir[CCHMAXPATH];

        ULONG       cbTitle = 0,
                    cbExecutable = 0,
                    cbStartupDir = 0,
                    cbParameters = 0,
                    cbEnvironment = 0,
                    ulSize;

        // 1) title
        if (pcszTitle)
            cbTitle = strlen(pcszTitle) + 1;

        // 2) executable
        if (pcszExecutable)
            cbExecutable = strlen(pcszExecutable) + 1;

        // 3) startup dir
        if (usStartupDirHandle)
        {
            // we have a startup dir: get the full path from
            // the handle
            WPFolder *pStartup;
            if (    (pStartup = _wpclsQueryObject(_WPFileSystem,
                                                  usStartupDirHandle | (G_usHiwordFileSystem << 16)))
                 && (_somIsA(pStartup, _WPFolder))
                 && (_wpQueryFilename(pStartup, szStartupDir, TRUE))
               )
            {
                cbStartupDir = strlen(szStartupDir) + 1;

                // for root folders, we get "C:" instead of "C:\", so fix this
                if (    (cbStartupDir == 3)
                     && (szStartupDir[1] == ':')
                   )
                {
                    szStartupDir[2] = '\\';
                    szStartupDir[3] = '\0';
                    cbStartupDir++;
                }
            }
        }

        // 4) parameters
        if (pcszParameters)
            cbParameters = strlen(pcszParameters) + 1;

        // 5) environment
        if (pcszEnvironment)
            cbEnvironment = appQueryEnvironmentLen(pcszEnvironment);

        ulSize =   sizeof(PROGDETAILS)
                 + cbTitle
                 + cbExecutable
                 + cbStartupDir
                 + cbParameters
                 + cbEnvironment;

        if (!pProgDetails)
            // caller wants size only:
            brc = TRUE;
        else if (ulSize <= *pulSize)
        {
            // caller has supplied sufficient buffer:
            PBYTE   pbCurrent;

            ZERO(pProgDetails);
            pProgDetails->Length = sizeof(PROGDETAILS);

            pProgDetails->progt.progc = ulProgType;
            pProgDetails->progt.fbVisible = fbVisible;
            if (pSWPInitial)
                memcpy(&pProgDetails->swpInitial, pSWPInitial, sizeof(SWP));

            // go copy after PROGDETAILS
            pbCurrent = (PBYTE)(pProgDetails + 1);

            // 1) title
            if (cbTitle)
            {
                pProgDetails->pszTitle = pbCurrent;
                memcpy(pbCurrent, pcszTitle, cbTitle);       // includes 0
                pbCurrent += cbTitle;
            }

            // 2) executable
            if (cbExecutable)
            {
                pProgDetails->pszExecutable = pbCurrent;
                memcpy(pbCurrent, pcszExecutable, cbExecutable);  // includes 0
                pbCurrent += cbExecutable;
            }

            // 3) startup dir
            if (cbStartupDir)
            {
                pProgDetails->pszStartupDir = pbCurrent;
                memcpy(pbCurrent, szStartupDir, cbStartupDir);  // includes 0
                pbCurrent += cbStartupDir;
            }

            // 4) parameters
            if (cbParameters)
            {
                pProgDetails->pszParameters = pbCurrent;
                memcpy(pbCurrent, pcszParameters, cbParameters);  // includes 0
                pbCurrent += cbParameters;
            }

            // 5) environment
            if (cbEnvironment)
            {
                pProgDetails->pszEnvironment = pbCurrent;
                memcpy(pbCurrent, pcszEnvironment, cbEnvironment);  // includes all the nulls
                pbCurrent += cbEnvironment;
            }

            brc = TRUE;
        }

        *pulSize = ulSize;
    }
    CATCH(excpt1)
    {
        brc = FALSE;
    } END_CATCH();

    return (brc);
}

/*
 *@@ progQueryProgType:
 *      returns the PROG_* flag for the given file.
 *
 *      If pvExec is specified, this is assumed to
 *      be an EXECUTABLE struct for the given file.
 *      If pvExec is NULL, we run exehOpen ourselves.
 *
 *      The PROG_* type returned is entirely our
 *      own assumption based on the executable image.
 *      This returns one of:
 *
 *      --  PROG_DEFAULT (0): error.
 *
 *      --  PROG_PM
 *
 *      --  PROG_WINDOWEDVDM (DOS session)
 *
 *      --  PROG_WINDOWABLEVIO (OS/2 session)
 *
 *      --  PROG_FULLSCREEN (OS/2 session where exe said it
 *          must be fullscreen)
 *
 *      --  PROG_31_ENHSEAMLESSCOMMON for all Win16 apps (NE)
 *
 *      --  PROG_WIN32 for all Win32 apps (PE)
 *
 *      --  PROG_DLL for all NE, LX, PE libraries
 *
 *@@added V0.9.16 (2002-01-01) [umoeller]
 */

ULONG progQueryProgType(PCSZ pszFullFile,
                        PVOID pvExec)
{
    ULONG           ulAppType = PROG_DEFAULT; // 0

    PEXECUTABLE     pExec = NULL;
    BOOL            fClose = FALSE;
    ULONG           ulDosAppType = 0;
    BOOL            fCallQueryAppType = FALSE;

    #ifdef DEBUG_ASSOCS
        _Pmpf((__FUNCTION__ ": %s, before: 0x%lX (%s)",
            pszFullFile, ulAppType, appDescribeAppType(ulAppType)));
    #endif

    TRY_LOUD(excpt1)
    {
        APIRET arc = NO_ERROR;

        ulAppType = PROG_DEFAULT;

        if (pvExec)
            // caller gave us PEXECUTABLE:
            pExec = (PEXECUTABLE)pvExec;
        else
        {
            // _Pmpf((__FUNCTION__ ": %s, calling exehOpen", pszFullFile));
            if (!(arc = exehOpen(pszFullFile, &pExec)))
                // close this again on exit
                fClose = TRUE;
        }

        if (arc)
            // cannot handle this executable:
            fCallQueryAppType = TRUE;
        else
        {
            // now we have the PEXECUTABLE:
            // check what we found
            if (pExec->fLibrary)
                ulAppType = PROG_DLL;
            else switch (pExec->ulOS)
            {
                case EXEOS_DOS3:
                case EXEOS_DOS4:
                    ulAppType = PROG_WINDOWEDVDM;
                break;

                case EXEOS_OS2:
                    switch (pExec->ulExeFormat)
                    {
                        case EXEFORMAT_LX:
                            switch (pExec->pLXHeader->ulFlags & E32APPMASK)
                            {
                                case E32PMAPI:
                                    // _Pmpf(("  LX OS2 PM"));
                                    ulAppType = PROG_PM;
                                break;

                                case E32PMW:
                                    // _Pmpf(("  LX OS2 VIO"));
                                    ulAppType = PROG_WINDOWABLEVIO;
                                break;

                                case E32NOPMW:
                                default:
                                    // _Pmpf(("  LX OS2 FULLSCREEN"));
                                    ulAppType = PROG_FULLSCREEN;
                                break;
                            }
                        break;

                        case EXEFORMAT_NE:
                            switch (pExec->pNEHeader->usFlags & NEAPPTYP)
                            {
                                case NEWINCOMPAT:
                                    // _Pmpf(("  NE OS2 VIO"));
                                    ulAppType = PROG_WINDOWABLEVIO;
                                break;

                                case NEWINAPI:
                                    // _Pmpf(("  NE OS2 PM"));
                                    ulAppType = PROG_PM;
                                break;

                                case NENOTWINCOMPAT:
                                default:
                                    // _Pmpf(("  NE OS2 FULLSCREEN"));
                                    ulAppType = PROG_FULLSCREEN;
                                break;
                            }
                        break;

                        case EXEFORMAT_COM:
                            ulAppType = PROG_WINDOWABLEVIO;
                        break;

                        default:
                            // something else: let
                            // OS/2 handle this
                            fCallQueryAppType = TRUE;
                    }
                break;

                case EXEOS_WIN16:
                case EXEOS_WIN386:
                    // _Pmpf(("  WIN16"));
                    ulAppType = PROG_31_ENHSEAMLESSCOMMON;
                break;

                case EXEOS_WIN32_GUI:
                case EXEOS_WIN32_CLI:
                    // _Pmpf(("  WIN32"));
                    ulAppType = PROG_WIN32;
                break;
            }
        }
    }
    CATCH(excpt1)
    {
        ulAppType = PROG_DEFAULT; // 0
        fCallQueryAppType = FALSE;
    } END_CATCH();

    if (    (fCallQueryAppType)
         && (pszFullFile)
       )
    {
        // we were helpless above: call
        // DosQueryAppType and translate
        // those flags
        appQueryAppType(pszFullFile,
                        &ulDosAppType,
                        &ulAppType);
    }

    if (fClose)
        // we opened the executable: free that again
        exehClose(&pExec);

    return (ulAppType);
}

/*
 *@@ progFindIcon:
 *      attempts to return an icon for the given executable
 *      and application type.
 *
 *      If icoLoadExeIcon found a suitable icon in the
 *      executable, we use that and set *pfNotDefaultIcon
 *      to TRUE. The caller should then set OBJSTYLE_NOTDEFAULTICON
 *      on the object on which the icon will be set.
 *
 *      If no custom icon was found, *pfNotDefaultIcon is set
 *      to FALSE, and a shared standard icon is returned.
 *      In that case the OBJSTYLE_NOTDEFAULTICON flag must be
 *      set clear.
 *
 *      This is shared code between XWPProgram and XWPProgramFile.
 *
 *@@added V0.9.16 (2002-01-01) [umoeller]
 */

APIRET progFindIcon(PEXECUTABLE pExec,          // in: executable from exehOpen
                    ULONG ulAppType,            // in: PROG_* app type
                    HPOINTER *phptr,            // out: if != NULL, icon handle
                    PICONINFO pIconInfo,        // out: if != NULL, icon info
                    PBOOL pfNotDefaultIcon)     // out: set to TRUE if non-default icon
{
    APIRET      arc = NO_ERROR;
    ULONG       ulStdIcon = 0;

    *pfNotDefaultIcon = FALSE;

    TRY_LOUD(excpt1)
    {
        /* _Pmpf((__FUNCTION__ " %s: progtype %d (%s)",
                (pExec && pExec->pFile) ? pExec->pFile->pszFilename : "NULL",
                ulAppType,
                appDescribeAppType(ulAppType))); */


        // examine the application type we have
        switch (ulAppType)
        {
            // PM:
            case PROG_PM:
            // Windows:
            case PROG_WINDOW_REAL:
            case PROG_30_STD:
            case PROG_WINDOW_AUTO:
            case PROG_30_STDSEAMLESSVDM:
            case PROG_30_STDSEAMLESSCOMMON:
            case PROG_31_STDSEAMLESSVDM:
            case PROG_31_STDSEAMLESSCOMMON:
            case PROG_31_ENHSEAMLESSVDM:
            case PROG_31_ENHSEAMLESSCOMMON:
            case PROG_31_ENH:
            case PROG_31_STD:

            case PROG_WIN32:                    // we get this for non-DLL PE!
                // try icon resource
                if (!icoLoadExeIcon(pExec,
                                    0,          // first icon found
                                    phptr,
                                    NULL,
                                    NULL))
                {
                    *pfNotDefaultIcon = TRUE;
                }
                else
                    if (ulAppType == PROG_PM)
                        ulStdIcon = STDICON_PM; // default PM prog icon
                    else if (ulAppType == PROG_WIN32)
                        ulStdIcon = STDICON_WIN32;
                    else
                        ulStdIcon = STDICON_WIN16; // default windoze
            break;

            case PROG_WINDOWABLEVIO:
                // "window compatible":
                // OS/2 window icon
                ulStdIcon = STDICON_OS2WIN;
            break;

            case PROG_FULLSCREEN:
                // "not window compatible":
                // OS/2 fullscreen icon
                ulStdIcon = STDICON_OS2FULLSCREEN;
            break;

            case PROG_WINDOWEDVDM:
                // DOS window
                ulStdIcon = STDICON_DOSWIN;
            break;

            case PROG_VDM: // == PROG_REAL
                // DOS fullscreen
                ulStdIcon = STDICON_DOSFULLSCREEN;
            break;

            case PROG_DLL:                  // can be NE, LX, PE
                // DLL flag set: load DLL icon
                ulStdIcon = STDICON_DLL;
            break;

            case PROG_PDD:
            case PROG_VDD:
                ulStdIcon = STDICON_DRIVER;
            break;

            default:
                // unknown:
                ulStdIcon = STDICON_PROG_UNKNOWN;
        }

        if (ulStdIcon)
            cmnGetStandardIcon(ulStdIcon,
                               phptr,
                               NULL);
    }
    CATCH(excpt1)
    {
        arc = ERROR_PROTECTION_VIOLATION;
    } END_CATCH();

    return (arc);
}

/* ******************************************************************
 *
 *   Running programs database
 *
 ********************************************************************/

/*
 *@@ LockRunning:
 *
 *@@added V0.9.12 (2001-05-22) [umoeller]
 */

static BOOL LockRunning(VOID)
{
    if (!G_hmtxRunning)
    {
        // first call:
        if (!DosCreateMutexSem(NULL,     // unnamed
                               &G_hmtxRunning,
                               0,        // unshared
                               TRUE))    // initially owned!
        {
            lstInit(&G_llRunning,
                    TRUE);      // auto-free

            return (TRUE);
        }
        else
            return (FALSE);
    }

    return (!WinRequestMutexSem(G_hmtxRunning, SEM_INDEFINITE_WAIT));
        // WinRequestMutexSem works even if the thread has no message queue
}

/*
 *@@ UnlockRunning:
 *
 *@@added V0.9.12 (2001-05-22) [umoeller]
 */

static VOID UnlockRunning(VOID)
{
    DosReleaseMutexSem(G_hmtxRunning);
}

/*
 *@@ RUNNINGPROGRAM:
 *
 *@@added V0.9.6 (2000-10-16) [umoeller]
 */

typedef struct _RUNNINGPROGRAM
{
    WPObject    *pObjEmphasis;       // object with use item
    PUSEITEM    pUseItemView;        // use item followed by VIEWITEM
    PUSEITEM    pUseItemFile;        // use item followed by VIEWFILE; can be NULL
} RUNNINGPROGRAM, *PRUNNINGPROGRAM;

/*
 *@@ progStoreRunningApp:
 *      registers a useitem for the specified object to give
 *      it "in-use" emphasis and stores the program in
 *      our internal list so that progAppTerminateNotify
 *      can remove it again when WM_APPTERMINATENOTIFY comes
 *      in.
 *
 *      This operates in two modes:
 *
 *      -- If pDataFile is NULL, this adds a USAGE_OPENVIEW (VIEWITEM)
 *         to pProgram, which is assumed to be a WPProgram
 *         or WPProgramFile. In that case, ulMenuID is ignored.
 *
 *      -- If pDataFile is specified, this first adds a
 *         USAGE_OPENVIEW (VIEWITEM) to the data file (!). In
 *         addition, this adds a USAGE_OPENFILE use
 *         item (VIEWFILE) to the data file which is needed
 *         by the WPS to track multiple open associations.
 *         In that case, you must also specify ulMenuID.
 *
 *@@added V0.9.6 (2000-10-16) [umoeller]
 *@@changed V0.9.6 (2000-11-12) [umoeller]: VIEWFILE items were missing, fixed.
 */

BOOL progStoreRunningApp(WPObject *pProgram,        // in: started program
                         WPFileSystem *pDataFile,   // in: data file with assoc or NULL
                         HAPP happ,                 // in: app handle from winhStartApp
                         ULONG ulMenuID)            // in: if (pDataFile != NULL), selected menu ID
                                                    // (for VIEWFILE item)
{
    BOOL    brc = FALSE;
    BOOL    fSemOwned = FALSE;

    TRY_LOUD(excpt1)
    {
        if (    (happ)
             && ((pProgram != NULL) || (pDataFile != NULL))
           )
        {
            PRUNNINGPROGRAM pRunning;
            if (pRunning = (PRUNNINGPROGRAM)malloc(sizeof(RUNNINGPROGRAM)))
            {
                // allocate view item
                PUSEITEM pUseItemView = 0,
                         pUseItemFile = 0;
                WPObject *pObjEmph = 0;

                if (pDataFile == NULL)
                    // object to work on is program object
                    pObjEmph = pProgram;
                else
                    // object to work on is datafile object
                    pObjEmph = pDataFile;

                if (pObjEmph)
                {
                    ULONG ulDummy;

                    // in any case, add "in-use" emphasis to the object
                    if (pUseItemView = (PUSEITEM)_wpAllocMem(pObjEmph,
                                                             sizeof(USEITEM) + sizeof(VIEWITEM),
                                                             &ulDummy))
                    {
                        // VIEWITEM is right behind use item
                        PVIEWITEM pViewItem = (PVIEWITEM)(pUseItemView + 1);
                        // set up data
                        memset(pUseItemView, 0, sizeof(USEITEM) + sizeof(VIEWITEM));
                        pUseItemView->type = USAGE_OPENVIEW;
                        pViewItem->view = OPEN_RUNNING;
                        pViewItem->handle = happ;

                        if (brc = _wpAddToObjUseList(pObjEmph,
                                                     pUseItemView))
                        {
                            // success:
                            // for data file associations, add VIEWFILE
                            // structure as well
                            if (pUseItemFile =  (PUSEITEM)_wpAllocMem(pObjEmph,
                                                                      sizeof(USEITEM) + sizeof(VIEWFILE),
                                                                      &ulDummy))
                            {
                                // VIEWFILE item is right behind use item
                                PVIEWFILE pViewFile = (PVIEWFILE)(pUseItemFile + 1);
                                // set up data
                                memset(pUseItemFile, 0, sizeof(USEITEM) + sizeof(VIEWFILE));
                                pUseItemFile->type = USAGE_OPENFILE;
                                pViewFile->ulMenuId = ulMenuID;
                                pViewFile->handle  = happ;

                                brc = _wpAddToObjUseList(pObjEmph,
                                                         pUseItemFile);
                            }
                            else
                                brc = FALSE;
                        }
                    }

                    if (brc)
                    {
                        // moved the lock down V0.9.18 (2002-02-13) [umoeller]
                        if (fSemOwned = LockRunning())
                        {
                            // store this in our internal list
                            // so we can find the object
                            // in progAppTerminateNotify
                            pRunning->pObjEmphasis = pObjEmph;
                            pRunning->pUseItemView = pUseItemView;
                            pRunning->pUseItemFile = pUseItemFile; // can be 0
                            lstAppendItem(&G_llRunning,
                                          pRunning);

                            // set list-notify flag on the object
                            // so that XFldObject will call
                            // progRunningAppDestroyed if
                            // the object is destroyed
                            _xwpModifyListNotify(pObjEmph,
                                                 OBJLIST_RUNNINGSTORED,
                                                 OBJLIST_RUNNINGSTORED);
                        }
                    }
                } // end if (pObjEmph)

                if (!brc)
                    free(pRunning);

            } // end if (pRunning)
        }
    }
    CATCH(excpt1)
    {
        brc = FALSE;
    } END_CATCH();

    if (fSemOwned)
        UnlockRunning();

    return (brc);
}

/*
 *@@ progAppTerminateNotify:
 *      the reverse to progStoreRunningApp. This searches
 *      the internal list of programs created by
 *      progStoreRunningApp and removes
 *
 *      --  the use item from the object;
 *
 *      --  the item from the list.
 *
 *      This normally gets called from krn_fnwpThread1Object
 *      when WM_APPTERMINATENOTIFY comes in.
 *
 *@@added V0.9.6 (2000-10-16) [umoeller]
 */

BOOL progAppTerminateNotify(HAPP happ)        // in: application handle
{
    BOOL    brc = FALSE;
    BOOL    fSemOwned = FALSE;

    TRY_LOUD(excpt1)
    {
        if (fSemOwned = LockRunning())
        {
            // go thru list
            PLISTNODE pNode = lstQueryFirstNode(&G_llRunning);
            while (pNode)
            {
                PRUNNINGPROGRAM pRunning;
                PVIEWITEM       pViewItem;

                if (    (pRunning = (PRUNNINGPROGRAM)pNode->pItemData)
                     && (pRunning->pUseItemView)
                     // VIEWITEM is right behind use item;
                     // this exists for both program and data file objects
                     && (pViewItem = (PVIEWITEM)(pRunning->pUseItemView + 1))
                     // is this ours?
                     && (pViewItem->handle == happ)
                   )
                {
                    // yes, this is ours:

                    // check if we also have a VIEWFILE item
                    if (pRunning->pUseItemFile)
                    {
                        // yes:
                        _wpDeleteFromObjUseList(pRunning->pObjEmphasis,
                                                pRunning->pUseItemFile);
                        _wpFreeMem(pRunning->pObjEmphasis,
                                   (PBYTE)pRunning->pUseItemFile);
                    }

                    // now remove "view" useitem from the object's use list
                    // (this always exists)
                    _wpDeleteFromObjUseList(pRunning->pObjEmphasis,
                                            pRunning->pUseItemView);
                    _wpFreeMem(pRunning->pObjEmphasis,
                               (PBYTE)pRunning->pUseItemView);

                    // remove this thing (auto-free!)
                    lstRemoveNode(&G_llRunning, pNode);

                    brc = TRUE;

                    // stop searching
                    break;
                }

                pNode = pNode->pNext;
            }
        }
    }
    CATCH(excpt1)
    {
        brc = FALSE;
    } END_CATCH();

    if (fSemOwned)
        UnlockRunning();

    return (brc);
}

/*
 *@@ progRunningAppDestroyed:
 *      this gets called from XFldObject::wpUnInitData
 *      when an object is being destroyed which is stored
 *      on the list maintained by progStoreRunningApp.
 *
 *      We must then remove that item from the list because
 *      otherwise we get a trap when progAppTerminateNotify
 *      attempts to remove in-use emphasis on the object.
 *
 *      Note that this does not remove the use items, nor
 *      does it free the memory allocated for the use items.
 *      Since this is done by the WPS wpUnInitData method,
 *      this is not necessary anyway.
 *
 *@@added V0.9.6 (2000-10-23) [umoeller]
 *@@changed V0.9.12 (2001-05-22) [umoeller]: fixed list search error
 */

BOOL progRunningAppDestroyed(WPObject *pObjEmphasis)    // in: destroyed object
{
    BOOL    brc = FALSE;
    BOOL    fSemOwned = FALSE;

    TRY_LOUD(excpt1)
    {
        if (fSemOwned = LockRunning())
        {
            // go thru list
            PLISTNODE pNode = lstQueryFirstNode(&G_llRunning);
            while (pNode)
            {
                PRUNNINGPROGRAM pRunning = (PRUNNINGPROGRAM)pNode->pItemData;
                PLISTNODE pNext = pNode->pNext;         // V0.9.12 (2001-05-22) [umoeller]
                if (pRunning->pObjEmphasis == pObjEmphasis)
                {
                    // found:
                    // remove this thing (auto-free!)
                    lstRemoveNode(&G_llRunning, pNode);
                            // since the memory for the useitems has been
                            // allocated using wpAllocMem, this will be
                            // freed automatically...

                    brc = TRUE;

                    // continue searching, because we can have more than one item
                }

                pNode = pNext;
            }
        }
    }
    CATCH(excpt1)
    {
        brc = FALSE;
    } END_CATCH();

    if (fSemOwned)
        UnlockRunning();

    return (brc);
}

/* ******************************************************************
 *
 *   Run programs
 *
 ********************************************************************/

/*
 *@@ progDisplayParamsPrompt:
 *      this gets called from progSetupArgs for each of
 *      those "[prompt]" strings that is found in the
 *      arguments of a program object.
 *
 *      On input, pstrPrompt is set to the string in
 *      between the "[]" characters. For "[prompt]",
 *      pstrPrompt would thus contain "prompt".
 *
 *      This then displays a dialog.
 *
 *      If this function returns TRUE, the caller assumes
 *      that pstrPrompt has been replaced with whatever
 *      the user entered. This function may clear pstrPrompt
 *      if the user entered nothing; in that case, the
 *      "[prompt]" is simply removed from the parameters.
 *
 *      If this function returns FALSE, the caller will
 *      assume the user has pressed "Cancel" and abort
 *      processing.
 *
 *@@added V0.9.6 (2000-10-16) [umoeller]
 */

static BOOL DisplayParamsPrompt(PXSTRING pstrPrompt)   // in: prompt string,
                                                // out: what user entered
{
    BOOL brc = FALSE;

    HWND hwndDlg = WinLoadDlg(HWND_DESKTOP,     // parent
                              NULLHANDLE,       // owner
                              WinDefDlgProc,
                              cmnQueryNLSModuleHandle(FALSE),
                              ID_XSD_NEWFILETYPE,   // "New File Type" dlg
                              NULL);            // pCreateParams
    if (hwndDlg)
    {
        // PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();
        HWND hwndEntryField = WinWindowFromID(hwndDlg, ID_XSDI_FT_ENTRYFIELD);
        WinSetWindowText(hwndDlg, cmnGetString(ID_XSSI_PARAMETERS)) ; // pszParameters
        WinSetDlgItemText(hwndDlg,
                          ID_XSDI_FT_TITLE,
                          pstrPrompt->psz);
        winhSetEntryFieldLimit(hwndEntryField, 255);
        if (WinProcessDlg(hwndDlg) == DID_OK)
        {
            CHAR    szNew[300] = "";
            // ULONG   ulOfs = 0;
            WinQueryWindowText(hwndEntryField,
                               sizeof(szNew),
                               szNew);
            // replace output buffer
            xstrcpy(pstrPrompt, szNew, 0);
            // return "OK pressed"
            brc = TRUE;
        }
        // else: user pressed "Cancel":

        WinDestroyWindow(hwndDlg);
    }

    return (brc);
}

/*
 *@@ FixSpacesInFilename:
 *      checks if pstr contains spaces and, if so,
 *      encloses the string in psz in quotes.
 *      It is assumes that there is enough room
 *      in psz to hold two more characters.
 *
 *      Otherwise psz is not changed.
 *
 *@@added V0.9.7 (2000-12-10) [umoeller]
 *@@changed V0.9.18 (2002-02-13) [umoeller]: now using XSTRING
 */

static VOID FixSpacesInFilename(PXSTRING pstr)
{
    if (pstr && pstr->psz)
    {
        if (strchr(pstr->psz, ' '))
        {
            // we have spaces:
            // compose temporary
            XSTRING strTemp;
            xstrInit(&strTemp,
                     pstr->ulLength + 3);  // preallocate: length of original
                                           // plus two quotes plus null terminator
            xstrcpy(&strTemp, "\"", 1);
            xstrcats(&strTemp, pstr);
            xstrcatc(&strTemp, '"');

            // copy back
            xstrcpys(pstr, &strTemp);
            xstrClear(&strTemp);
        }
    }
}

/*
 *@@ HandlePlaceholder:
 *      checks *p if it is a WPS placeholder for file name
 *      components. Returns TRUE if *p is a valid placeholder
 *      or FALSE if it is not.
 *
 *      Gets called with *p pointing to the "%" char.
 *
 *      If this returns TRUE, it must:
 *
 *      -- set pxstr to the replacement string (e.g. a filename).
 *         pxstr can be set to an empty string if the placeholder
 *         is to be deleted.
 *
 *      -- set pcReplace to the no. of characters that should be
 *         skipped in the source string. This must be the length
 *         of the placeholder, excluding any null terminator
 *         (e.g. 4 for "%**F").
 *
 *      -- set *pfAppendDataFilename to FALSE if the caller should
 *         _not_ append the data file name at the end of the params
 *         string. Otherwise it should leave this flag alone.
 *
 *         From my testing, the WPS always appends the full path
 *         specification (%*) of the data file to the end of the
 *         program's arguments list, UNLESS "%*" or one of the
 *         "%**X" parameters are specified at least once.
 *
 *         As a result, this function sets *pfAppendDataFilename
 *         to FALSE for all "%*" and "%**X" placeholders, but
 *         not for parameter strings.
 *
 *      If this returns FALSE, *p is simply left alone (copied).
 *      All output parameters are then ignored.
 *
 *      Preconditions: pstrTemp has been initialized (xstrInit).
 *
 *      Postconditions: pstrTemp is freed by the caller.
 *
 *@@added V0.9.7 (2000-12-10) [umoeller]
 *@@changed V0.9.18 (2002-02-13) [umoeller]: fixed possible buffer overflows
 */

static BOOL HandlePlaceholder(PCSZ p,           // in: placeholder (starting with "%")
                              PCSZ pcszFilename, // in: data file name;
                                                        // ptr is always valid, but can point to ""
                              PXSTRING pstrTemp,       // out: replacement string (e.g. filename)
                              PULONG pcReplace,        // out: no. of chars to replace (w/o null terminator)
                              PBOOL pfAppendDataFilename)  // out: if FALSE, do not append full filename
{
    BOOL brc = FALSE;

    // p points to "%" char; check next char
    switch (*(p+1))
    {
        case ' ':
        case '\0':
            // "%": disable passing full path
            *pfAppendDataFilename = FALSE;
            *pcReplace = 1;     // % only
            brc = TRUE;
        break;

        case '*':
        {
            XSTRING strTemp;
            xstrInit(&strTemp, 0);

            if ( (*(p+2)) == '*' )      // "%**" ?
            {
                // we have a placeholder:
                // check next char (P, D, N, F, E)
                switch (*(p+3))
                {
                    case 'P':
                    {
                        // "%**P": drive and path information without the
                        // last backslash (\)
                        PSZ pLastBackslash;
                        if (pLastBackslash = strrchr(pcszFilename, '\\'))
                        {
                            xstrcpy(&strTemp,
                                    pcszFilename,
                                    pLastBackslash - pcszFilename);
                            FixSpacesInFilename(&strTemp); // V0.9.7 (2000-12-10) [umoeller]
                        }

                        *pfAppendDataFilename = FALSE;
                        *pcReplace = 4;     // "%**?"
                        brc = TRUE;
                    }
                    break;

                    case 'D':
                    {
                        //  "%**D": drive with ':' or UNC name
                        ULONG ulDriveSpecLen;
                        if (!doshGetDriveSpec(pcszFilename,
                                              NULL,
                                              &ulDriveSpecLen,
                                              NULL))
                                // works on "C:\blah" or "\\unc\blah"
                        {
                            xstrcpy(&strTemp,
                                    pcszFilename,
                                    ulDriveSpecLen);
                            FixSpacesInFilename(&strTemp); // V0.9.7 (2000-12-10) [umoeller]
                        }

                        *pfAppendDataFilename = FALSE;
                        *pcReplace = 4;     // "%**?"
                        brc = TRUE;
                    }
                    break;

                    case 'N':
                    {
                        //  "%**N": file name without extension.
                        // first find filename
                        PSZ pLastBackslash;
                            // works on "C:\blah" or "\\unc\blah"
                        if (pLastBackslash = strrchr(pcszFilename + 2, '\\'))
                        {
                            // find last dot in filename
                            PSZ pLastDot;
                            ULONG ulLength = 0;
                            if (pLastDot = strrchr(pLastBackslash + 1, '.'))
                            {
                                // extension found:
                                // copy from pLastBackslash + 1 to pLastDot
                                ulLength = pLastDot - (pLastBackslash + 1);
                            }
                            // else no extension:
                            // copy entire name

                            xstrcpy(&strTemp,
                                    pLastBackslash + 1,
                                    ulLength);
                            FixSpacesInFilename(&strTemp); // V0.9.7 (2000-12-10) [umoeller]
                        }

                        *pfAppendDataFilename = FALSE;
                        *pcReplace = 4;     // "%**?"
                        brc = TRUE;
                    }
                    break;

                    case 'F':
                    {
                        // "%**F": file name with extension.
                        // find filename
                        PSZ pLastBackslash;
                            // works on "C:\blah" or "\\unc\blah"
                        if (pLastBackslash = strrchr(pcszFilename + 2, '\\'))
                        {
                            xstrcpy(&strTemp, pLastBackslash + 1, 0);
                            FixSpacesInFilename(&strTemp); // V0.9.7 (2000-12-10) [umoeller]
                        }

                        *pfAppendDataFilename = FALSE;
                        *pcReplace = 4;     // "%**?"
                        brc = TRUE;
                    }
                    break;

                    case 'E':
                    {
                        //  "%**E": extension without leading dot.
                        // In HPFS, the extension always comes after the last dot.
                        PSZ pExt;
                        if (pExt = doshGetExtension(pcszFilename))
                        {
                            xstrcpy(&strTemp, pExt, 0);
                            FixSpacesInFilename(&strTemp);
                                    // improbable, but possible
                        }

                        *pfAppendDataFilename = FALSE;
                        *pcReplace = 4;     // "%**?"
                        brc = TRUE;
                    }
                    break;
                }
            } // end of all those "%**?" cases
            else
            {
                // third character is not '*':
                // we then assume it's "%*" only...

                // "%*": full path of data file
                xstrcpy(&strTemp, pcszFilename, 0);
                FixSpacesInFilename(&strTemp); // V0.9.7 (2000-12-10) [umoeller]

                *pfAppendDataFilename = FALSE;
                *pcReplace = 2;     // "%**?"
                brc = TRUE;
            }

            if (strTemp.ulLength)
                // something was copied to replace:
                xstrcpys(pstrTemp, &strTemp);
            // else: pstrTemp has been zeroed by caller, leave it

            xstrClear(&strTemp);

        }
        break;  // case '*': (second character)
    } // end switch (*(p+1))

    return (brc);
}

/*
 *@@ progSetupArgs:
 *      sets up a new "parameters" string from the specified
 *      program parameters string and data file.
 *
 *      This returns a new buffer which must be free()'d after
 *      use.
 *
 *      All the WPS argument placeholders are supported, which
 *      are:
 *
 *      --  "%": do not pass any arguments at all. In that
 *          case, pArgDataFile is always ignored.
 *
 *      --  [string]: create a popup dialog for variable
 *          data. This does not prevent the WPS from appending
 *          the data file name.
 *
 *      --  "%*": full path of data file.
 *
 *      --  "%**P": drive and path information without the
 *          last backslash (\).
 *
 *      --  "%**D": drive with ':' or UNC name.
 *
 *      --  "%**N": file name without extension.
 *
 *      --  "%**F": file name with extension.
 *
 *      --  "%**E": extension without leading dot.  In HPFS,
 *          the extension always comes after the last dot.
 *
 *      The WPS doesn't care for spaces. These placeholders
 *      are even replaced if not surrounded by spaces.
 *
 *      NOTE: Since this program can possibly wait for a prompt
 *      dialog to be filled by the user, this requires a message
 *      queue. Also, this will not return until the dialog has
 *      been dismissed.
 *
 *      This returns TRUE if all parameters were successfully
 *      converted. This returns FALSE only on fatal internal
 *      errors or if a "prompt" dlg was displayed and the user
 *      pressed "Cancel".
 *
 *@@added V0.9.6 (2000-10-16) [umoeller]
 *@@changed V0.9.6 (2000-11-04) [umoeller]: fixed null string search
 *@@changed V0.9.7 (2000-12-10) [umoeller]: fixed spaces-in-filename problem (new implementation)
 *@@changed V0.9.7 (2000-12-10) [umoeller]: fixed params prompt hangs (new implementation)
 *@@changed V0.9.7 (2000-12-10) [umoeller]: extracted HandlePlaceholder
 */

BOOL progSetupArgs(PCSZ pcszParams,
                   WPFileSystem *pFile,         // in: file or NULL
                   PSZ *ppszParamsNew)          // out: new params
{
    BOOL    brc = TRUE;

    XSTRING strParamsNew;
    BOOL    fAppendDataFilename = TRUE;
    CHAR    szDataFilename[2*CCHMAXPATH] = "";

    if (pFile)
    {
        // we have a data file as argument:
        // get fully q'fied data file name
        _wpQueryFilename(pFile,
                         szDataFilename,
                         TRUE);
    }
    // else no file: szDataFilename is empty

    xstrInit(&strParamsNew, 500);

    if (pcszParams)
    {
        // we have a params string in the program object:
        // copy it character per character and replace
        // special keywords
        PCSZ p = pcszParams;

        while (    (*p)  // not end of string
                && (brc == TRUE)        // no error, not cancelled
              )
        {
            switch (*p)
            {
                /*
                 *   '%':  handle filename placeholder
                 *
                 */

                case '%':
                {
                    XSTRING strTemp;
                    ULONG cReplace = 0;
                    xstrInit(&strTemp, 0);
                    // might be a placeholder, so check
                    if (    (HandlePlaceholder(p,
                                               szDataFilename,
                                               &strTemp,
                                               &cReplace,
                                               &fAppendDataFilename))
                         && (cReplace)
                       )
                    {
                        // handled:
                        // append replacement
                        xstrcat(&strParamsNew, strTemp.psz, strTemp.ulLength);
                        // skip placeholder (cReplace has been set by HandlePlaceholder)
                        p += cReplace;
                        // disable appending the full filename
                        // at the end
                        // fAppendDataFilename = FALSE;
                    }
                    else
                    {
                        // not handled:
                        // just append
                        xstrcatc(&strParamsNew, *p);
                        p++;
                    }
                    xstrClear(&strTemp);
                }
                break;

                /*
                 *   '[': handle prompt placeholder
                 *
                 */

                case '[':
                {
                    PCSZ pEnd;
                    if (pEnd = strchr(p + 1, ']'))
                    {
                        XSTRING strPrompt;
                        // copy stuff between brackets
                        PSZ pPrompt = strhSubstr(p + 1, pEnd);
                        xstrInitSet(&strPrompt, pPrompt);
                        if (DisplayParamsPrompt(&strPrompt))
                        {
                            // user pressed "OK":
                            xstrcat(&strParamsNew, strPrompt.psz, strPrompt.ulLength);
                            // next character to copy is
                            // character after closing bracket
                            p = pEnd + 1;
                        }
                        else
                            // user pressed "Cancel":
                            // get outta here!!
                            brc = FALSE;

                        xstrClear(&strPrompt);
                    }
                    else
                        // no closing bracket: just copy
                        p++;
                }
                break;

                default:
                    // any other character: append
                    xstrcatc(&strParamsNew, *p);
                    p++;
            } // end switch (*p)
        } // end while ( (*p) && (brc == TRUE) )
    } // end if (pcszParams)

    if (brc)
    {
        // no error, not cancelled:

        // we are now either done with copying the existing params string,
        // or no params string existed in the first place...

        // now check if we should still add the filename

        if (fAppendDataFilename)
        {
            // this is still TRUE if none of the "%" placeholders have
            // been found;
            // append filename to the end
            XSTRING strDataFilename;
            xstrInitCopy(&strDataFilename, szDataFilename, 0);

            // if we have parameters already, append space
            // if the last char isn't a space yet
            if (strParamsNew.ulLength)
                // space as last character?
                if (    *(strParamsNew.psz + strParamsNew.ulLength - 1)
                     != ' ')
                    xstrcatc(&strParamsNew, ' ');

            FixSpacesInFilename(&strDataFilename); // V0.9.7 (2000-12-10) [umoeller]
            xstrcats(&strParamsNew, &strDataFilename);
        }

        // return new params to caller
        *ppszParamsNew = strParamsNew.psz;

    } // end if (brc)
    else
        // cancelled or error:
        xstrClear(&strParamsNew);

    return (brc);
}

/*
 *@@ progSetupEnv:
 *      sets up the environment for progOpenProgram.
 *
 *      The caller must free the PSZ which is returned.
 *
 *@@added V0.9.7 (2000-12-17) [umoeller]
 *@@changed V0.9.12 (2001-05-22) [umoeller]: fixed invalid pointer return
 *@@changed V0.9.18 (2002-02-27) [umoeller]: added two codepage variables
 */

PSZ progSetupEnv(WPObject *pProgObject,        // in: WPProgram or WPProgramFile
                 PCSZ pcszEnv,          // in: its environment string (or NULL)
                 WPFileSystem *pFile)          // in: file or NULL
{
    PSZ             pszNewEnv = NULL;
    APIRET          arc = NO_ERROR;
    DOSENVIRONMENT  Env = {0};

    // _Pmpf((__FUNCTION__ ": pcszEnv is %s", (pcszEnv) ? pcszEnv : "NULL"));

    if (pcszEnv)
        // environment specified:
        arc = appParseEnvironment(pcszEnv,
                                  &Env);
    else
        // no environment specified:
        // get the one from the WPS process
        arc = appGetEnvironment(&Env);

    if (arc == NO_ERROR)
    {
        // we got the environment:
        PSZ         *pp = 0;
        CHAR        szTemp[100];
        HOBJECT     hobjProgram = _wpQueryHandle(pProgObject);

        // 1) change WORKPLACE_PROCESS=YES to WORKPLACE__PROCESS=NO
        if (pp = appFindEnvironmentVar(&Env, "WORKPLACE_PROCESS"))
        {
            // _Pmpf(("  found %s", *pp));
            // variable was set (should always be the case, since
            // this is set for the second PMSHELL.EXE):
            // we can simply overwrite the string in there,
            // because WORKPLACE_PROCESS=YES has the same length
            // as      WORKPLACE__PROCESS=NO
            strcpy(*pp, "WORKPLACE__PROCESS=NO");
        }

        // 2) set WP_OBJHANDLE

        if (pFile)
            // file as argument: use WP_OBJHANDLE=xxx,yyy with
            // the handle of the file _and_ the program
            sprintf(szTemp,
                    "WP_OBJHANDLE=%d,%d",
                    _wpQueryHandle(pFile),
                    hobjProgram);
        else
            // no file specified: use WP_OBJHANDLE=xxx with
            // the handle of the program only
            sprintf(szTemp, "WP_OBJHANDLE=%d", hobjProgram);

        if (!(arc = appSetEnvironmentVar(&Env,
                                         szTemp,
                                         TRUE)))     // add as first entry
        {
            // V0.9.18 (2002-02-23) [umoeller]
            // apparently the WPS also sets these two environment
            // variables always too
            appSetEnvironmentVar(&Env,
                                 "WORKPLACE_PRIMARY_CP=1",      // or 0?
                                 FALSE);
            appSetEnvironmentVar(&Env,
                                 "WORKPLACE_NATIVE=0",          // or 1?
                                 FALSE);

            // rebuild environment
            if (arc = appConvertEnvironment(&Env,
                                            &pszNewEnv,
                                            NULL))
                // error:
                if (pszNewEnv)
                {
                    free(pszNewEnv);
                    pszNewEnv = NULL;       // was missing V0.9.12 (2001-05-22) [umoeller]
                }
        }

        appFreeEnvironment(&Env);
    }

    return (pszNewEnv);
}

/*
 *@@ PROGOPENDATA:
 *
 *@@added V0.9.16 (2001-12-02) [umoeller]
 */

typedef struct _PROGOPENDATA
{
    PPROGDETAILS    pProgDetails;
    HAPP            *phapp;         // out: HAPP
    ULONG           cbFailingName;
    PSZ             pszFailingName;

    HWND            hwndNotify;
} PROGOPENDATA, *PPROGOPENDATA;

/*
 *@@ progOpenProgramThread1:
 *      actual handler for progOpenProgram, which is
 *      guaranteed to run on thread 1.
 *
 *@@added V0.9.16 (2001-12-02) [umoeller]
 *@@changed V0.9.16 (2002-01-04) [umoeller]: fixed bad startup dir if no arg data file was given
 */

APIRET progOpenProgramThread1(PVOID pvData)
{
    APIRET          arc = NO_ERROR;
    PSZ             pszParams = NULL;

    PPROGOPENDATA   pData = (PPROGOPENDATA)pvData;

    TRY_LOUD(excpt1)
    {
        PCKERNELGLOBALS pKernelGlobals = krnQueryGlobals();

        // start the app (more hacks in appStartApp,
        // which calls WinStartApp in turn)
        arc = appStartApp(pKernelGlobals->hwndThread1Object, // notify window: t1 obj wnd
                          pData->pProgDetails,
                          0,              // APP_RUN_* flags V0.9.14
                          pData->phapp,
                          pData->cbFailingName,
                          pData->pszFailingName);
                    // V0.9.16 (2001-12-02) [umoeller]
    }
    CATCH(excpt1)
    {
        arc = ERROR_PROTECTION_VIOLATION;
    } END_CATCH();

    // handle notify window that progOpenProgram
    // is waiting on
    if (pData->hwndNotify)
        WinPostMsg(pData->hwndNotify,
                   WM_USER,
                   (MPARAM)arc,
                   0);

    return (arc);
}

/*
 *@@ progOpenProgram:
 *      this opens the specified program object, which
 *      must be of the WPProgram or WPProgramFile class.
 *      This implements workarounds to WinStartApp to
 *      mimic the typical WPS behavior on program objects.
 *      Even better, this can handle data files as arguments
 *      to program objects, with full support for the various
 *      parameter placeholders.
 *
 *      This is a complete implementation of running
 *      programs in the WPS. Since IBM was not kind
 *      enough to export a method interface for doing
 *      this, and since there is _no way_ of starting a
 *      program object with a data file as a parameter
 *      using the standard WPS methods, I had to rewrite
 *      all this.
 *
 *      Presently, this gets called from:
 *
 *      -- XFldDataFile::wpOpen to open a program (file)
 *         object which has been associated with a data
 *         file. In that case, pArgDataFile is set to
 *         the data file whose association is to be started
 *         with the data file as an argument.
 *
 *      Features:
 *
 *      1)  This handles special executable flags as with
 *          WPProgram (e.g. "*" for a command line). See
 *          appStartApp for details, which gets called from
 *          here to call WinStartApp in turn.
 *
 *      2)  If pArgDataFile is != NULL, this function handles
 *          data files as arguments to a program file. In that
 *          case, we also evaluate placeholders in the program
 *          object "parameters" data field (see wpQueryProgDetails).
 *          See progSetupArgs for supported placeholders, which
 *          gets called from here.
 *
 *          In addition, if the associated program does not have
 *          a startup directory, this is set to the data file's
 *          directory.
 *
 *      3)  This creates a use item to take care of "in use"
 *          emphasis on the icon exactly as the WPS does it.
 *          If pArgDataFile is specified, "in use" emphasis
 *          is added to the data file's icon. Otherwise it is
 *          added to the program object icon. See progStoreRunningApp,
 *          which gets called from here.
 *
 *          Note: The XWorkplace thread-1 object window
 *          (krn_fnwpThread1Object) is used as the notify
 *          window to WinStartApp to receive WM_APPTERMINATENOTIFY
 *          so we can properly remove source emphasis later.
 *          krn_fnwpThread1Object then calls progAppTerminateNotify.
 *
 *      Since this calls progSetupArgs, this might display modal
 *      dialogs before returning. As a result (and because this
 *      calls winhStartApp in turn), the calling thread must
 *      have a message queue.
 *
 *@@added V0.9.6 (2000-10-16) [umoeller]
 *@@changed V0.9.7 (2000-12-10) [umoeller]: fixed startup dir with data files
 *@@changed V0.9.7 (2000-12-17) [umoeller]: now building environment correctly
 *@@changed V0.9.12 (2001-05-22) [umoeller]: extracted progQueryDetails
 *@@changed V0.9.12 (2001-05-22) [umoeller]: fixed crash cleanup
 *@@changed V0.9.16 (2001-10-19) [umoeller]: fixed prototype for APIRET
 *@@changed V0.9.16 (2001-10-19) [umoeller]: fixed root folder problems
 *@@changed V0.9.16 (2001-12-02) [umoeller]: moved all code to progOpenProgramThread1; added thread-1 sync
 */

APIRET progOpenProgram(WPObject *pProgObject,       // in: WPProgram or WPProgramFile
                       WPFileSystem *pArgDataFile,  // in: data file as arg or NULL
                       ULONG ulMenuID,              // in: with data files, menu ID that was used
                       HAPP *phapp,                 // out: HAPP
                       ULONG cbFailingName,
                       PSZ pszFailingName)
{
    APIRET          arc = FOPSERR_NOT_HANDLED_ABORT;
    PPROGDETAILS    pProgDetails = NULL;
    PSZ             pszParams = NULL;
    PSZ             pszNewStartupDir = NULL;
    PSZ             pszNewEnvironment = NULL;

    // INT3();

    TRY_LOUD(excpt1)
    {
        // get program data
        // (progQueryDetails checks for whether this is a valid object)
        if (!(pProgDetails = progQueryDetails(pProgObject)))
            arc = ERROR_NOT_ENOUGH_MEMORY;
        else if (    (!pProgDetails->pszExecutable)
                  || (!pProgDetails->pszExecutable[0])
                )
            arc = ERROR_INVALID_PARAMETER;
        else
        {
            // OK, now we got the program object data....
            /*
            typedef struct _PROGDETAILS {
              ULONG        Length;          //  Length of structure.
              PROGTYPE     progt;           //  Program type.
              PSZ          pszTitle;        //  Title.
              PSZ          pszExecutable;   //  Executable file name (program name).
              PSZ          pszParameters;   //  Parameter string.
              PSZ          pszStartupDir;   //  Start-up directory.
              PSZ          pszIcon;         //  Icon-file name.
              PSZ          pszEnvironment;  //  Environment string.
              SWP          swpInitial;      //  Initial window position and size.
            } PROGDETAILS; */

            // fix parameters (placeholders etc.)
            if (!(progSetupArgs(pProgDetails->pszParameters,
                                pArgDataFile,
                                &pszParams)))
                arc = ERROR_INTERRUPT;
            else
            {
                PROGOPENDATA Data;

                // set the new params
                pProgDetails->pszParameters = pszParams;

                // if startup dir is empty, set a default startup dir:
                if (    (!pProgDetails->pszStartupDir)
                     || (pProgDetails->pszStartupDir[0] == '\0')
                   )
                {
                    CHAR    szNewStartupDir[CCHMAXPATH] = "";
                    ULONG cb;
                    // no startup dir:
                    // if we have a data file argument, use its directory
                    if (pArgDataFile)
                    {
                        WPFolder *pStartupFolder;
                        if (pStartupFolder = _wpQueryFolder(pArgDataFile))
                            _wpQueryFilename(pStartupFolder, szNewStartupDir, TRUE);
                    }
                    else
                    {
                        // no data file: use executable's directory
                        // V0.9.16 (2002-01-04) [umoeller]
                        PSZ p;
                        if (p = strrchr(pProgDetails->pszExecutable + 2, '\\'))
                        {
                            cb = _min(p - pProgDetails->pszExecutable,
                                      CCHMAXPATH - 1);
                            memcpy(szNewStartupDir,
                                   pProgDetails->pszExecutable,
                                   cb);
                            szNewStartupDir[cb] = '\0';
                        }
                    }

                    // this returns "K:" only for root folders, so check!!
                    if (    (cb = strlen(szNewStartupDir))
                         && (cb < 3)
                       )
                        strcpy(szNewStartupDir + 1, ":\\");

                    pszNewStartupDir = strdup(szNewStartupDir);
                    pProgDetails->pszStartupDir = pszNewStartupDir;
                }

                // build the new environment V0.9.7 (2000-12-17) [umoeller]
                pszNewEnvironment
                    = progSetupEnv(pProgObject,
                                   pProgDetails->pszEnvironment,
                                   pArgDataFile);
                pProgDetails->pszEnvironment = pszNewEnvironment;

                // _Pmpf((__FUNCTION__ ": hacked exec: \"%s\"", (pProgDetails->pszExecutable) ? pProgDetails->pszExecutable : "NULL"));
                // _Pmpf(("  hacked params: \"%s\"", (pProgDetails->pszParameters) ? pProgDetails->pszParameters : "NULL"));
                // _Pmpf(("  hacked startup: \"%s\"", (pProgDetails->pszStartupDir) ? pProgDetails->pszStartupDir : "NULL"));

                Data.pProgDetails = pProgDetails;
                Data.phapp = phapp;
                Data.cbFailingName = cbFailingName;
                Data.pszFailingName = pszFailingName;
                Data.hwndNotify = NULLHANDLE;

                if (doshMyTID() == 1)
                {
                    // if we're running on thread 1, we don't need
                    // the below overhead
                    // _Pmpf((__FUNCTION__ ": calling progOpenProgramThread1 directly"));
                    arc = progOpenProgramThread1(&Data);
                }
                else
                {
                    HAB hab;
                    PCKERNELGLOBALS pKernelGlobals;

                    // _Pmpf((__FUNCTION__ ": entering msg loop"));

                    // not thread 1:
                    // create notify window for progOpenProgramThread1;
                    // using WinSendMsg hangs the system (for god's sake)
                    if (    (Data.hwndNotify = winhCreateObjectWindow(WC_STATIC, NULL))
                         && (hab = WinQueryAnchorBlock(Data.hwndNotify))
                         && (pKernelGlobals = krnQueryGlobals())
                         && (WinPostMsg(pKernelGlobals->hwndThread1Object,
                                        T1M_PROGOPENPROGRAM,
                                        (MPARAM)&Data,
                                        0))
                       )
                    {
                        // alright, all this succeeded:
                        // wait for thread-1 to finish all this
                        QMSG qmsg;
                        BOOL fQuit = FALSE;
                        while (WinGetMsg(hab,
                                         &qmsg, 0, 0, 0))
                        {
                            // current message for our object window?
                            if (    (qmsg.hwnd == Data.hwndNotify)
                                 && (qmsg.msg == WM_USER)
                               )
                            {
                                // _Pmpf((__FUNCTION__ ": got WM_USER"));
                                fQuit = TRUE;
                                arc = (ULONG)qmsg.mp1;
                            }

                            WinDispatchMsg(hab, &qmsg);

                            if (fQuit)
                                break;
                        }
                    }

                    if (Data.hwndNotify)
                        WinDestroyWindow(Data.hwndNotify);

                    // _Pmpf((__FUNCTION__ ": left message loop"));
                }

                if (!arc)
                    // app started OK:
                    // set in-use emphasis on either
                    // the data file or the program object
                    progStoreRunningApp(pProgObject,
                                        pArgDataFile,
                                        *phapp,
                                        ulMenuID);
            }
        }
    }
    CATCH(excpt1)
    {
        arc = ERROR_PROTECTION_VIOLATION;
    } END_CATCH();

    if (pszParams)
        free(pszParams);
    if (pszNewStartupDir)
        free(pszNewStartupDir);
    if (pszNewEnvironment)
        free(pszNewEnvironment);
    if (pProgDetails)
        free(pProgDetails);

    // _Pmpf((__FUNCTION__ ": leaving, rc = %d", arc));

    return (arc);
}


/* ******************************************************************
 *
 *   XWPProgramFile notebook callbacks (notebook.c)
 *
 ********************************************************************/

#ifndef __NOMODULEPAGES__

/*
 *@@ progFileInitPage:
 *      "Program" page notebook callback function (notebook.c).
 *      Sets the controls on the page according to the disk's
 *      characteristics.
 *
 *@@added V0.9.0 [umoeller]
 *@@changed V0.9.12 (2001-05-19) [umoeller]: now using textview control for description, added extended info
 *@@changed V0.9.16 (2001-12-08) [umoeller]: fixed crash for OS code
 *@@changed V0.9.16 (2002-01-05) [umoeller]: moved this here from fsys.c, renamed from fsysProgramInitPage
 */

VOID progFileInitPage(PNOTEBOOKPAGE pnbp,    // notebook info struct
                      ULONG flFlags)                // CBI_* flags (notebook.h)
{
    // PGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    if (flFlags & CBI_INIT)
    {
        // replace the static "description" control
        // with a text view control
        HWND hwndNew;
        txvRegisterTextView(WinQueryAnchorBlock(pnbp->hwndDlgPage));
        hwndNew = txvReplaceWithTextView(pnbp->hwndDlgPage,
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

        if (_wpQueryFilename(pnbp->inbp.somSelf, szFilename, TRUE))
        {
            PEXECUTABLE     pExec = NULL;
            HWND            hwndTextView = WinWindowFromID(pnbp->hwndDlgPage,
                                                           ID_XSDI_PROG_DESCRIPTION);

            WinSetDlgItemText(pnbp->hwndDlgPage, ID_XSDI_PROG_FILENAME,
                              szFilename);
            WinSetWindowText(hwndTextView, "\n");

            if (!(exehOpen(szFilename, &pExec)))
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

                    case EXEFORMAT_COM:         // V0.9.16 (2002-01-09) [umoeller]
                        pszExeFormat = "COM";
                    break;
                }

                if (pszExeFormat)
                    WinSetDlgItemText(pnbp->hwndDlgPage,
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

                    case EXEOS_WIN32_GUI:
                        pszOS = "Win32 GUI";
                    break;

                    case EXEOS_WIN32_CLI:
                        pszOS = "Win32 CLI";
                    break;
                }

                // fixed crash here V0.9.16 (2001-12-08) [umoeller]
                if (pszOS)
                    strcpy(szOS, pszOS);
                else
                    sprintf(szOS, "unknown OS code %d", pExec->ulOS);

                if (pExec->f32Bits)
                    strcat(szOS, " (32-bit)");
                else
                    strcat(szOS, " (16-bit)");

                WinSetDlgItemText(pnbp->hwndDlgPage,
                                  ID_XSDI_PROG_TARGETOS,
                                  szOS);

                // now get buildlevel info
                if (exehQueryBldLevel(pExec) == NO_ERROR)
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

                exehClose(&pExec);
            }
        } // end if (_wpQueryFilename...

        // V0.9.12 (2001-05-19) [umoeller]
        // gee, what was this code doing in here?!?
        /* if ((_wpQueryProgDetails(pnbp->inbp.somSelf, (PPROGDETAILS)NULL, &cbProgDetails)))
            if ((pProgDetails = (PPROGDETAILS)_wpAllocMem(pnbp->inbp.somSelf,
                                                          cbProgDetails,
                                                          NULL))
                    != NULL)
            {
                _wpQueryProgDetails(pnbp->inbp.somSelf, pProgDetails, &cbProgDetails);

                if (pProgDetails->pszParameters)
                    WinSetDlgItemText(pnbp->hwndDlgPage, ID_XSDI_PROG_PARAMETERS,
                                      pProgDetails->pszParameters);
                if (pProgDetails->pszStartupDir)
                    WinSetDlgItemText(pnbp->hwndDlgPage, ID_XSDI_PROG_WORKINGDIR,
                                      pProgDetails->pszStartupDir);

                _wpFreeMem(pnbp->inbp.somSelf, (PBYTE)pProgDetails);
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
 *      transient thread started by progFile1InitPage
 *      to insert modules into the "Imported modules" container.
 *
 *      This thread is created with a msg queue.
 *
 *@@added V0.9.9 (2001-03-26) [lafaix]
 *@@changed V0.9.9 (2001-03-30) [umoeller]: replaced dialog resource with generic cnr page
 *@@changed V0.9.9 (2001-03-30) [umoeller]: sped up display
 */

static void _Optlink fntInsertModules(PTHREADINFO pti)
{
    PNOTEBOOKPAGE pnbp = (PNOTEBOOKPAGE)(pti->ulData);

    TRY_LOUD(excpt1)
    {
        ULONG         cModules = 0,
                      ul;
        PFSYSMODULE   paModules = NULL;
        CHAR          szFilename[CCHMAXPATH] = "";

        pnbp->fShowWaitPointer = TRUE;

        if (_wpQueryFilename(pnbp->inbp.somSelf, szFilename, TRUE))
        {
            PEXECUTABLE     pExec = NULL;

            if (!(exehOpen(szFilename, &pExec)))
            {
                if (    (!exehQueryImportedModules(pExec,
                                                       &paModules,
                                                       &cModules))
                     && (paModules)
                   )
                {
                    HWND hwndCnr = WinWindowFromID(pnbp->hwndDlgPage, ID_XFDI_CNR_CNR);

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
                if (pnbp->pUser)
                    exehFreeImportedModules(pnbp->pUser);
                pnbp->pUser = paModules;

                exehClose(&pExec);
            }
        }
    }
    CATCH(excpt1) {}  END_CATCH();

    pnbp->fShowWaitPointer = FALSE;
}

/*
 *@@ progFile1InitPage:
 *      "Imported modules" page notebook callback function (notebook.c).
 *
 *@@added V0.9.9 (2001-03-11) [lafaix]
 *@@changed V0.9.9 (2001-03-30) [umoeller]: replaced dialog resource with generic cnr page
 *@@todo: corresponding ItemChanged page
 *@@changed V0.9.16 (2002-01-05) [umoeller]: moved this here from fsys.c, renamed from fsysProgram1InitPage
 */

VOID progFile1InitPage(PNOTEBOOKPAGE pnbp,    // notebook info struct
                          ULONG flFlags)                // CBI_* flags (notebook.h)
{
    // PGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    HWND hwndCnr = WinWindowFromID(pnbp->hwndDlgPage, ID_XFDI_CNR_CNR);

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

        WinSetDlgItemText(pnbp->hwndDlgPage,
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
                  (ULONG)pnbp);
    }

    /*
     * CBI_DESTROY:
     *      clean up page before destruction
     */

    if (flFlags & CBI_DESTROY)
    {
        if (pnbp->pUser)
            exehFreeImportedModules(pnbp->pUser);
         pnbp->pUser = NULL;
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
 *      transient thread started by progFile2InitPage
 *      to insert functions into the "Exported functions" container.
 *
 *      This thread is created with a msg queue.
 *
 *@@added V0.9.9 (2001-03-28) [lafaix]
 *@@changed V0.9.9 (2001-03-30) [umoeller]: replaced dialog resource with generic cnr page
 *@@changed V0.9.9 (2001-03-30) [umoeller]: sped up display
 */

static void _Optlink fntInsertFunctions(PTHREADINFO pti)
{
    PNOTEBOOKPAGE pnbp = (PNOTEBOOKPAGE)(pti->ulData);

    TRY_LOUD(excpt1)
    {
        ULONG         cFunctions = 0,
                      ul;
        PFSYSFUNCTION paFunctions = NULL;
        CHAR          szFilename[CCHMAXPATH] = "";

        pnbp->fShowWaitPointer = TRUE;

        if (_wpQueryFilename(pnbp->inbp.somSelf, szFilename, TRUE))
        {
            PEXECUTABLE     pExec = NULL;

            if (!(exehOpen(szFilename, &pExec)))
            {
                if (    (!exehQueryExportedFunctions(pExec, &paFunctions, &cFunctions))
                     && (paFunctions)
                   )
                {
                    HWND hwndCnr = WinWindowFromID(pnbp->hwndDlgPage, ID_XFDI_CNR_CNR);

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
                if (pnbp->pUser)
                    exehFreeExportedFunctions(pnbp->pUser);
                pnbp->pUser = paFunctions;

                exehClose(&pExec);
            }
        }
    }
    CATCH(excpt1) {}  END_CATCH();

    pnbp->fShowWaitPointer = FALSE;
}

/*
 *@@ progFile2InitPage:
 *      "Exported functions" page notebook callback function (notebook.c).
 *
 *@@added V0.9.9 (2001-03-11) [lafaix]
 *@@changed V0.9.9 (2001-03-30) [umoeller]: replaced dialog resource with generic cnr page
 *@@todo: corresponding ItemChanged page
 *@@changed V0.9.16 (2002-01-05) [umoeller]: moved this here from fsys.c, renamed from fsysProgram2InitPage
 */

VOID progFile2InitPage(PNOTEBOOKPAGE pnbp,    // notebook info struct
                       ULONG flFlags)                // CBI_* flags (notebook.h)
{
    // PGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    HWND hwndCnr = WinWindowFromID(pnbp->hwndDlgPage, ID_XFDI_CNR_CNR);

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

        WinSetDlgItemText(pnbp->hwndDlgPage,
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
                  (ULONG)pnbp);
    }

    /*
     * CBI_DESTROY:
     *      clean up page before destruction
     */

    if (flFlags & CBI_DESTROY)
    {
        if (pnbp->pUser)
            exehFreeExportedFunctions(pnbp->pUser);
        pnbp->pUser = NULL;
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

    CHAR szBuf[100];

    ULONG       ulResourceID; // !!! Could be a string with Windows or Open32 execs
    const char  *pcszResourceType;
    ULONG       ulResourceSize;
    const char  *pcszResourceFlag;

    HPOINTER    hptrResource;           // for resource type == icon, converted icon

} RESOURCERECORD, *PRESOURCERECORD;

/*
 *@@ fsysGetResourceFlagName:
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
 *@@ progGetWinResourceTypeName:
 *      returns a human-readable name for a Win
 *      resource type.
 *
 *@@added V0.9.16 (2001-12-18) [umoeller]
 */

PSZ progGetWinResourceTypeName(PSZ pszBuf,
                               ULONG ulTypeThis)
{
    PCSZ pcsz = "unknown";
    switch (ulTypeThis)
    {
        case WINRT_ACCELERATOR: pcsz = "WINRT_ACCELERATOR"; break;
        case WINRT_BITMAP: pcsz =  "WINRT_BITMAP"; break;
        case WINRT_CURSOR: pcsz =  "WINRT_CURSOR"; break;
        case WINRT_DIALOG: pcsz =  "WINRT_DIALOG"; break;
        case WINRT_FONT: pcsz =  "WINRT_FONT"; break;
        case WINRT_FONTDIR: pcsz =  "WINRT_FONTDIR"; break;
        case WINRT_ICON: pcsz =  "WINRT_ICON"; break;
        case WINRT_MENU: pcsz =  "WINRT_MENU"; break;
        case WINRT_RCDATA: pcsz =  "WINRT_RCDATA"; break;
        case WINRT_STRING: pcsz =  "WINRT_STRING"; break;
        case WINRT_MESSAGELIST: pcsz = "WINRT_MESSAGELIST"; break;
        case WINRT_GROUP_CURSOR: pcsz = "WINRT_GROUP_CURSOR"; break;
        case WINRT_GROUP_ICON: pcsz = "WINRT_GROUP_ICON"; break;
    }

    sprintf(pszBuf, "%d (%s)", ulTypeThis, pcsz);

    return (pszBuf);
}

/*
 *@@ progGetOS2ResourceTypeName:
 *      returns a human-readable name for an OS/2
 *      resource type.
 *
 *@@added V0.9.7 (2000-12-20) [lafaix]
 *@@changed V0.9.9 (2001-04-02) [umoeller]: now returning const char*
 *@@changed V0.9.16 (2002-01-05) [umoeller]: moved this here from fsys.c, renamed from fsysGetOS2ResourceTypeName
 *@@changed V0.9.16 (2002-01-05) [umoeller]: added icons display
 */

PCSZ progGetOS2ResourceTypeName(ULONG ulResourceType)
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
 *      transient thread started by progResourcesInitPage
 *      to insert resources into the "Resources" container.
 *
 *      This thread is created with a msg queue.
 *
 *@@added V0.9.7 (2000-12-17) [lafaix]
 *@@changed V0.9.9 (2001-03-30) [umoeller]: replaced dialog resource with generic cnr page
 *@@changed V0.9.9 (2001-03-30) [umoeller]: sped up display
 *@@changed V0.9.9 (2001-04-03) [umoeller]: fixed cnr crash introduced by 03-30 change
 *@@changed V0.9.16 (2002-01-05) [umoeller]: fixed bad resource nameing for win resources
 */

static void _Optlink fntInsertResources(PTHREADINFO pti)
{
    PNOTEBOOKPAGE pnbp = (PNOTEBOOKPAGE)(pti->ulData);

    TRY_LOUD(excpt1)
    {
        CHAR          szFilename[CCHMAXPATH] = "";
        PEXECUTABLE   pExec = NULL;
        ULONG         cResources = 0;
        PFSYSRESOURCE paResources = NULL;

        pnbp->fShowWaitPointer = TRUE;

        if (    (_wpQueryFilename(pnbp->inbp.somSelf, szFilename, TRUE))
             && (!(exehOpen(szFilename, &pExec)))
             && (!(exehQueryResources(pExec,
                                      &paResources,
                                      &cResources)))
             && (cResources)
             && (paResources)
           )
        {
            HWND hwndCnr = WinWindowFromID(pnbp->hwndDlgPage, ID_XFDI_CNR_CNR);

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
                        ULONG ulType = paResources[ul].ulType;
                        BOOL fLoad = FALSE;

                        preccThis->ulResourceID = paResources[ul].ulID;
                        preccThis->ulResourceSize = paResources[ul].ulSize;

                        // fixed bad resource naming for Windows resources
                        if (pExec->ulOS == EXEOS_WIN16)
                        {
                            preccThis->pcszResourceType = progGetWinResourceTypeName(preccThis->szBuf,
                                                                                     ulType);
                            if (ulType == WINRT_ICON)
                                fLoad = TRUE;
                        }
                        else
                        {
                            // V0.9.16 (2001-12-18) [umoeller]
                            preccThis->pcszResourceType = progGetOS2ResourceTypeName(ulType);
                            if (ulType == RT_POINTER)
                                fLoad = TRUE;
                            preccThis->pcszResourceFlag
                                   = fsysGetResourceFlagName(paResources[ul].ulFlag);
                        }

                        if (fLoad)
                            // try to load this icon
                            icoLoadExeIcon(pExec,
                                           preccThis->ulResourceID,
                                           &preccThis->hptrResource,
                                           NULL,
                                           NULL);

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
        if (pnbp->pUser)
            exehFreeResources(pnbp->pUser);
        // store resources
        pnbp->pUser = paResources; // can be NULL

        exehClose(&pExec);
    }
    CATCH(excpt1) {}  END_CATCH();

    pnbp->fShowWaitPointer = FALSE;
}

/*
 *@@ KillPointersInRecords:
 *
 *@@added V0.9.16 (2002-01-05) [umoeller]
 */

static ULONG EXPENTRY KillPointersInRecords(HWND hwndCnr,
                                            PRECORDCORE precc,
                                            ULONG ulUser1,
                                            ULONG ulUser2)
{
    HPOINTER hptr;
    if (hptr = ((PRESOURCERECORD)precc)->hptrResource)
        WinFreeFileIcon(hptr);

    return 0;
}

/*
 *@@ progResourcesInitPage:
 *      "Resources" page notebook callback function (notebook.c).
 *
 *@@added V0.9.7 (2000-12-17) [lafaix]
 *@@changed V0.9.9 (2001-03-30) [umoeller]: replaced dialog resource with generic cnr page
 *@@todo: corresponding ItemChanged page
 *@@changed V0.9.16 (2002-01-05) [umoeller]: moved this here from fsys.c, renamed from fsysResourcesInitPage
 */

VOID progResourcesInitPage(PNOTEBOOKPAGE pnbp,    // notebook info struct
                           ULONG flFlags)                // CBI_* flags (notebook.h)
{
    // PGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    HWND hwndCnr = WinWindowFromID(pnbp->hwndDlgPage, ID_XFDI_CNR_CNR);

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

        WinSetDlgItemText(pnbp->hwndDlgPage,
                          ID_XFDI_CNR_GROUPTITLE,
                          cmnGetString(ID_XSSI_PGMFILE_RESOURCES)) ; // pszResourcesPage

        // set up cnr details view
        // added resource V0.9.16 (2002-01-05) [umoeller]
        xfi[i].ulFieldOffset = FIELDOFFSET(RESOURCERECORD, hptrResource);
        xfi[i].pszColumnTitle = cmnGetString(ID_XSSI_COLMN_RESOURCEICON); // pszColmnResourceIcon
        xfi[i].ulDataType = CFA_BITMAPORICON;
        xfi[i++].ulOrientation = CFA_LEFT;

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
                                2);            // return third column

        BEGIN_CNRINFO()
        {
            cnrhSetView(CV_DETAIL | CA_DETAILSVIEWTITLES | CA_OWNERDRAW);
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
                  (ULONG)pnbp);
    }

    /*
     * CBI_DESTROY:
     *      clean up page before destruction
     */

    if (flFlags & CBI_DESTROY)
    {
        // we need to nuke all the pointers that were created
        // V0.9.16 (2002-01-05) [umoeller]

        cnrhForAllRecords(hwndCnr,
                          NULL,     // root records
                          KillPointersInRecords,
                          0,
                          0);

        if (pnbp->pUser)
            exehFreeResources(pnbp->pUser);
        pnbp->pUser = NULL;
    }
}

/*
 *@@ progResourcesMessage:
 *      notebook callback function (notebook.c) for the
 *      "Resources" page.
 *      This gets really all the messages from the dlg.
 *
 *@@added V0.9.16 (2002-01-05) [umoeller]
 */

BOOL XWPENTRY progResourcesMessage(PNOTEBOOKPAGE pnbp,
                                   ULONG msg, MPARAM mp1, MPARAM mp2,
                                   MRESULT *pmrc)
{
    BOOL brc = FALSE;       // not processed

    switch (msg)
    {
        case WM_DRAWITEM:
            // return value: let cnr draw the item
            *pmrc = (MRESULT)FALSE;

            if ((USHORT)mp1 == ID_XFDI_CNR_CNR)
            {
                POWNERITEM poi = (POWNERITEM)mp2;
                PCNRDRAWITEMINFO pcdii = (PCNRDRAWITEMINFO)poi->hItem;

                if (pcdii->pFieldInfo->flData == CFA_BITMAPORICON)
                {
                    HPOINTER hptr;
                    if (hptr = ((PRESOURCERECORD)pcdii->pRecord)->hptrResource)
                    {
                        // let us handle this
                        WinDrawPointer(poi->hps,
                                       poi->rclItem.xLeft,
                                       poi->rclItem.yBottom,
                                       hptr,
                                       DP_NORMAL);
                    }

                    // @@todo os/2 bitmaps
                }
            }

            brc = TRUE;     // msg processed
        break;
    }

    return (brc);
}

#endif // __NOMODULEPAGES__



