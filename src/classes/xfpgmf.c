
/*
 *@@sourcefile xfpgmf.c:
 *      This file contains SOM code for the following XWorkplace classes:
 *
 *      --  XFldProgramFile (WPProgramFile replacement)
 *
 *      XFldProgram is only responsible for changing the
 *      default icons of executable files.
 *
 *      Installation of this class is optional.
 *
 *      Starting with V0.9.0, the files in classes\ contain only
 *      i.e. the methods themselves.
 *      The implementation for this class is in filesys\filesys.c.
 *
 *@@somclass XFldProgramFile xfpgmf_
 *@@somclass M_XFldProgramFile xfpgmfM_
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
 *  This file was generated by the SOM Compiler and Emitter Framework.
 *  Generated using:
 *      SOM Emitter emitctm: 2.41
 */

#ifndef SOM_Module_xfpgmf_Source
#define SOM_Module_xfpgmf_Source
#endif
#define XFldProgramFile_Class_Source
#define M_XFldProgramFile_Class_Source

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

#define INCL_DOSSESMGR          // DosQueryAppType
#define INCL_DOSSEMAPHORES
#define INCL_DOSERRORS
#define INCL_WINPOINTERS
#define INCL_WINPROGRAMLIST     // needed for PROGDETAILS, wppgm.h
#include <os2.h>

// C library headers
#include <stdio.h>

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\dosh.h"

// SOM headers which don't crash with prec. header files
#include "xfpgmf.ih"
#include "xfobj.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

#include "filesys\filesys.h"            // various file-system object implementation code

#pragma hdrstop                         // VAC++ keeps crashing otherwise

#include <wpcmdf.h>                     // WPCommandFile

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

static const char *G_pcszInstanceFilter = "*.ADD,*.COM,*.DLL,*.DMD,*.EXE,*.FLT,*.IFS,*.SNP,*.SYS";

/* ******************************************************************
 *
 *   XFldProgramFile instance methods
 *
 ********************************************************************/

/*
 *@@ xwpQueryProgType:
 *      this returns the PROG_* flag signalling the executable
 *      type of the program file.
 *      Note that in addition to the PROG_* flags defined in
 *      PROGDETAILS, this may also return the following:
 *               PROG_XF_DLL     for dynamic link libraries
 *               PROG_XF_DRIVER  for virtual or physical device drivers;
 *                               most files ending in .SYS will be
 *                               recognized as DLL's though.
 */

SOM_Scope ULONG  SOMLINK xfpgmf_xwpQueryProgType(XFldProgramFile *somSelf)
{
    PPROGDETAILS    pProgDetails;
    ULONG           ulSize;

    XFldProgramFileData *somThis = XFldProgramFileGetData(somSelf);
    XFldProgramFileMethodDebug("XFldProgramFile","xfpgmf_xwpQueryProgType");

    #ifdef DEBUG_ASSOCS
        _Pmpf(("  xwpQueryProgType %s, before: 0x%lX",
            _wpQueryTitle(somSelf), _ulAppType));
    #endif

    if (_ulAppType == -1)
    {
        // not queried yet:

        // get program object data
        if ((_wpQueryProgDetails(somSelf, (PPROGDETAILS)NULL, &ulSize))) {
            if ((pProgDetails = (PPROGDETAILS)malloc(ulSize)) != NULL) {
                if ((_wpQueryProgDetails(somSelf, pProgDetails, &ulSize)))
                {
                    // we base our assumptions on what OS/2 thinks
                    // the app type is
                    _ulAppType = pProgDetails->progt.progc;

                    #ifdef DEBUG_ASSOCS
                        _Pmpf(("    progdtls.progc: 0x%lX",
                                    _ulAppType));
                    #endif

                    // and we modify a few of these assumptions
                    switch (_ulAppType)
                    {
                        case PROG_PDD:
                        case PROG_VDD:
                            // these two types are documented in pmshl.h,
                            // but I'm not sure they're ever used; convert
                            // them to our own driver type
                            _ulAppType = PROG_XF_DRIVER;
                        break;

                        // Windows:
                        case PROG_WINDOW_REAL         :
                        case PROG_30_STD              :
                        case PROG_WINDOW_AUTO         :
                        case PROG_30_STDSEAMLESSVDM   :
                        case PROG_30_STDSEAMLESSCOMMON:
                        case PROG_31_STDSEAMLESSVDM   :
                        case PROG_31_STDSEAMLESSCOMMON:
                        case PROG_31_ENHSEAMLESSVDM   :
                        case PROG_31_ENHSEAMLESSCOMMON:
                        case PROG_31_ENH              :
                        case PROG_31_STD              :
                        {
                            // for all the Windows app types, we check
                            // for whether the extension of the file is
                            // DLL or 386; if so, we change the type to
                            // DLL. Otherwise,  all Windows DLL's get
                            // some default windoze icon.
                            CHAR    szProgramFile[CCHMAXPATH];
                            if (_wpQueryFilename(somSelf, szProgramFile, FALSE))
                            {
                                PSZ     pLastDot = strrchr(szProgramFile, '.');
                                strupr(szProgramFile);
                                if (strcmp(pLastDot, ".DLL") == 0)
                                    // DLL found:
                                    _ulAppType = PROG_XF_DLL;
                            }
                        break; }

                        case PROG_DEFAULT:  // == 0; OS/2 isn't sure what this is
                        {
                            CHAR        szProgramFile[CCHMAXPATH];
                            APIRET      arc;

                            if (_wpQueryFilename(somSelf, szProgramFile, TRUE))
                            {
                                // no type available: get it ourselves
                                arc = DosQueryAppType(szProgramFile, &(_ulDosAppType));
                                if (arc == NO_ERROR) {
                                    if (_ulDosAppType == 0)
                                        _ulAppType = PROG_FULLSCREEN;
                                    else if (   (_ulDosAppType & 0x40)
                                             || (_ulDosAppType & 0x80)
                                            )
                                        // some driver bit set
                                        _ulAppType = PROG_XF_DRIVER;
                                    else if ((_ulDosAppType & 0xF0) == 0x10)
                                        // DLL bit set
                                        _ulAppType = PROG_XF_DLL;
                                    else if (_ulDosAppType & 0x20)
                                        // DOS bit set?
                                        _ulAppType = PROG_WINDOWEDVDM;
                                    else if ((_ulDosAppType & 0x0003) == 0x0003) // "Window-API" == PM
                                        _ulAppType = PROG_PM;
                                    else if (   ((_ulDosAppType & 0xFFFF) == 0x1000) // windows program (?!?)
                                             || ((_ulDosAppType & 0xFFFF) == 0x0400) // windows program (?!?)
                                            )
                                        _ulAppType = PROG_31_ENH;
                                    else if ((_ulDosAppType & 0x03) == 0x02)
                                        _ulAppType = PROG_WINDOWABLEVIO;
                                    else if ((_ulDosAppType & 0x03) == 0x01)
                                        _ulAppType = PROG_FULLSCREEN;
                                }
                            }
                        break; }

                        // the other values are OK, leave them as is

                    } // end switch (_ulAppType)
                }
                free(pProgDetails);
            }
        }
    }

    #ifdef DEBUG_ASSOCS
        _Pmpf(("  End of xwpQueryProgType, returning: 0x%lX",
                _ulAppType));
    #endif

    return (_ulAppType);

}

/*
 *@@ xwpQuerySetup2:
 *      this XFldObject method is overridden to support
 *      setup strings for program files.
 *
 *      This uses code in filesys\filesys.c which is
 *      shared for WPProgram and WPProgramFile instances.
 *
 *      See XFldObject::xwpQuerySetup2 for details.
 *
 *@@added V0.9.4 (2000-08-02) [umoeller]
 */

SOM_Scope ULONG  SOMLINK xfpgmf_xwpQuerySetup2(XFldProgramFile *somSelf,
                                               PSZ pszSetupString,
                                               ULONG cbSetupString)
{
    ULONG ulReturn = 0;

    // method pointer for parent class
    somTD_XFldObject_xwpQuerySetup pfn_xwpQuerySetup2 = 0;

    // XFldProgramFileData *somThis = XFldProgramFileGetData(somSelf);
    XFldProgramFileMethodDebug("XFldProgramFile","xfpgmf_xwpQuerySetup2");

    // call implementation
    ulReturn = fsysQueryProgramFileSetup(somSelf, pszSetupString, cbSetupString);

    // manually resolve parent method
    pfn_xwpQuerySetup2
        = (somTD_XFldObject_xwpQuerySetup)wpshResolveFor(somSelf,
                                                         _somGetParent(_XFldProgramFile),
                                                         "xwpQuerySetup2");
    if (pfn_xwpQuerySetup2)
    {
        // now call XFldObject method
        if ( (pszSetupString) && (cbSetupString) )
            // string buffer already specified:
            // tell XFldObject to append to that string
            ulReturn += pfn_xwpQuerySetup2(somSelf,
                                           pszSetupString + ulReturn, // append to existing
                                           cbSetupString - ulReturn); // remaining size
        else
            // string buffer not yet specified: return length only
            ulReturn += pfn_xwpQuerySetup2(somSelf, 0, 0);
    }

    return (ulReturn);
}

/*
 *@@ wpInitData:
 *      this WPObject instance method gets called when the
 *      object is being initialized (on wake-up or creation).
 *      We initialize our additional instance data here.
 *      Always call the parent method first.
 */

SOM_Scope void  SOMLINK xfpgmf_wpInitData(XFldProgramFile *somSelf)
{
    XFldProgramFileData *somThis = XFldProgramFileGetData(somSelf);
    XFldProgramFileMethodDebug("XFldProgramFile","xfpgmf_wpInitData");

    // initialize instance data
    _ulDosAppType = -1;
    _ulAppType = -1;
    _fProgIconSet = FALSE;
    _hptrThis = NULLHANDLE;

    XFldProgramFile_parent_WPProgramFile_wpInitData(somSelf);
}

/*
 *@@ wpObjectReady:
 *      this WPObject notification method gets called by the
 *      WPS when object instantiation is complete, for any reason.
 *      ulCode and refObject signify why and where from the
 *      object was created.
 *      The parent method must be called first.
 *
 *      See XFldObject::wpObjectReady for remarks about using
 *      this method as a copy constructor.
 */

SOM_Scope void  SOMLINK xfpgmf_wpObjectReady(XFldProgramFile *somSelf,
                                             ULONG ulCode, WPObject* refObject)
{
    XFldProgramFileData *somThis = XFldProgramFileGetData(somSelf);
    XFldProgramFileMethodDebug("XFldProgramFile","xfpgmf_wpObjectReady");

    XFldProgramFile_parent_WPProgramFile_wpObjectReady(somSelf,
                                                       ulCode,
                                                       refObject);

    if (ulCode & OR_REFERENCE)
    {
        // according to wpobject.h, this flag is set for
        // OR_FROMTEMPLATE, OR_FROMCOPY, OR_SHADOW; this
        // means that refObject is valid

        // reset our app type flags, because when program files
        // are _copied_, wpSetProgIcon gets called one time too early
        // (between wpInitData and wpObjectReady; at this point,
        // the PROGDETAILS have no meaningful values, and xwpQueryProgType
        // has returned garbage data),
        // and a second time in time (after wpObjectReady);
        // for this second time we need to pretend that we haven't
        // queried the app type yet, because for the second call
        // of wpSetProgIcon, the icon will then be set correctly
        _ulDosAppType = -1;
        _ulAppType = -1;
        _fProgIconSet = FALSE;
        _hptrThis = NULLHANDLE;
    }
}

/*
 *@@ wpQueryStyle:
 *      this instance method returns the object style flags.
 */

SOM_Scope ULONG  SOMLINK xfpgmf_wpQueryStyle(XFldProgramFile *somSelf)
{
    ULONG ulStyle = 0;
    // XFldProgramFileData *somThis = XFldProgramFileGetData(somSelf);
    XFldProgramFileMethodDebug("XFldProgramFile","xfpgmf_wpQueryStyle");

    ulStyle = XFldProgramFile_parent_WPProgramFile_wpQueryStyle(somSelf);

    #ifdef DEBUG_ASSOCS
        _Pmpf(("%s: wpQueryStyle: 0x%lX %s %s",
                    _wpQueryTitle(somSelf),
                    ulStyle,
                    (ulStyle & OBJSTYLE_NOTDEFAULTICON) ? "OBJSTYLE_NOTDEFAULTICON" : "",
                    (ulStyle & OBJSTYLE_CUSTOMICON) ? "OBJSTYLE_CUSTOMICON" : ""));
    #endif
    return (ulStyle /* & ~OBJSTYLE_CUSTOMICON */);
}

/*
 *@@ wpSetIcon:
 *      this instance method sets the current icon for
 *      an object. As opposed to with wpSetIconData,
 *      this does not change the icon permanently.
 *
 *      Note: If the OBJSTYLE_NOTDEFAULTICON object style
 *      flag has been set with wpSetStyle, the old icon
 *      (HPOINTER) will be destroyed.
 *      As a result, that flag needs to be unset if
 *      icons are shared between objects, as with class
 *      default icons. The OBJSTYLE_CUSTOMICON flag does
 *      NOT work, even if the WPS lists it with wpQueryStyle.
 *
 *      Also, the WPS annoyingly resets the icon to its
 *      default when a settings notebook is opened.
 */

SOM_Scope BOOL  SOMLINK xfpgmf_wpSetIcon(XFldProgramFile *somSelf,
                                         HPOINTER hptrNewIcon)
{
    BOOL brc = TRUE;
    // PGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    XFldProgramFileMethodDebug("XFldProgramFile","xfpgmf_wpSetIcon");

    #ifdef DEBUG_ASSOCS
        _Pmpf(("    %s wpSetIcon", _wpQueryTitle(somSelf) ));
    #endif
    // icon replacements allowed?
    /* if (pGlobalSettings->ReplIcons) {
        XFldProgramFileData *somThis = XFldProgramFileGetData(somSelf);
        // we only allow the icon to be changed if
        // this is a call from _wpSetProgIcon (which
        // we have overriden) or if the WPS calls this
        // method for the first time, e.g. because it is
        // being set by some method other than wpSetProgIcon,
        // which happens if the WPS has determined the icon
        // from the executable or .ICON EA's.
        if (    (_hptrThis == NULLHANDLE)
             || (_fProgIconSet)
           )
        {
            _hptrThis = hptrNewIcon;
            brc = XFldProgramFile_parent_WPProgramFile_wpSetIcon(somSelf,
                                                                   hptrNewIcon);
            // prohibit subsequent calls
            _fProgIconSet = FALSE;
        }
        // else: ignore the call. The WPS keeps messing with
        // program icons from methods other than _wpSetProgIcon,
        // which we must ignore.

    } else */
        brc = XFldProgramFile_parent_WPProgramFile_wpSetIcon(somSelf,
                                                               hptrNewIcon);

    return (brc);
}

/*
 *@@ wpSetProgIcon:
 *      this instance method sets the visual icon for
 *      this program file to the appropriate custom
 *      or default icon. We override this to change
 *      the default icons for program files using
 *      /ICONS/ICONS.DLL, if the global settings allow
 *      this.
 *      The following methods are called by the WPS when
 *      an icon is to be determined for a WPProgramFile:
 *      -- if the file has its own icon, e.g. from the
 *              .ICON EAs: only wpSetIcon, this method does
 *              _not_ get called then.
 *      -- otherwise (no .ICON):
 *              1)  wpQueryIcon, which calls
 *              2)  wpSetProgIcon, which per default calls
 *              3)  wpSetIcon
 *      We must return an individual icon in this method,
 *      so we load the icons from /ICONS/ICONS.DLL every
 *      time we arrive here.
 *      If we return a "global" icon handle which could
 *      have been loaded in wpclsInitData, unfortunately
 *      the WPS will destroy that "global" icon, ruining
 *      the icons of all other program files also.
 */

SOM_Scope BOOL  SOMLINK xfpgmf_wpSetProgIcon(XFldProgramFile *somSelf,
                                             PFEA2LIST pfeal)
{
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    HPOINTER    hptr = NULLHANDLE;

    XFldProgramFileData *somThis = XFldProgramFileGetData(somSelf);
    XFldProgramFileMethodDebug("XFldProgramFile","xfpgmf_wpSetProgIcon");

    // icon replacements allowed?
    if (pGlobalSettings->fReplaceIcons)
    {
        CHAR        szProgramFile[CCHMAXPATH];
        HMODULE     hmodIconsDLL = cmnQueryIconsDLL();

        #ifdef DEBUG_ASSOCS
            _Pmpf(("%s: wpSetProgIcon",
                        _wpQueryTitle(somSelf) ));
        #endif

        if (_wpQueryFilename(somSelf, szProgramFile, TRUE))
        {
            // first: check if an .ICO file of the same filestem
            // exists in the folder. If so, _always_ use that icon.
            PSZ p = strrchr(szProgramFile, '.');
            if (p)
            {
                CHAR szIconFile[CCHMAXPATH];
                strncpy(szIconFile, szProgramFile, (p-szProgramFile));
                strcpy(szIconFile + (p-szProgramFile), ".ico");
                #ifdef DEBUG_ASSOCS
                    _Pmpf(("   Icon file: %s", szIconFile));
                #endif
                hptr = WinLoadFileIcon(szIconFile,
                                       FALSE);      // no private copy
            }

            if (!hptr)
            {
                // no such .ICO file exists:
                // examine the application type we have
                switch (_xwpQueryProgType(somSelf))
                {

                    // PM:
                    case PROG_PM                  :
                    // Windows:
                    case PROG_WINDOW_REAL         :
                    case PROG_30_STD              :
                    case PROG_WINDOW_AUTO         :
                    case PROG_30_STDSEAMLESSVDM   :
                    case PROG_30_STDSEAMLESSCOMMON:
                    case PROG_31_STDSEAMLESSVDM   :
                    case PROG_31_STDSEAMLESSCOMMON:
                    case PROG_31_ENHSEAMLESSVDM   :
                    case PROG_31_ENHSEAMLESSCOMMON:
                    case PROG_31_ENH              :
                    case PROG_31_STD              :
                        // have PM determine the icon
                        hptr = WinLoadFileIcon(szProgramFile,
                                               FALSE);
                    break;

                    case PROG_WINDOWABLEVIO:
                        // "window compatible":
                        // OS/2 window icon
                        hptr = WinLoadPointer(HWND_DESKTOP, hmodIconsDLL,
                                              108);
                    break;

                    case PROG_FULLSCREEN:
                        // "not window compatible":
                        // OS/2 fullscreen icon
                        hptr = WinLoadPointer(HWND_DESKTOP, hmodIconsDLL,
                                              107);
                    break;

                    case PROG_WINDOWEDVDM:
                        // DOS window
                        hptr = WinLoadPointer(HWND_DESKTOP, hmodIconsDLL,
                                              105);
                    break;

                    case PROG_VDM: // == PROG_REAL
                        // DOS fullscreen
                        hptr = WinLoadPointer(HWND_DESKTOP, hmodIconsDLL,
                                              104);
                    break;

                    case PROG_XF_DLL:
                        // DLL flag set: load DLL icon
                        hptr = WinLoadPointer(HWND_DESKTOP, hmodIconsDLL,
                                              103);
                    break;

                    case PROG_XF_DRIVER:
                        hptr = WinLoadPointer(HWND_DESKTOP, hmodIconsDLL,
                                              106);
                    break;

                    default:
                        // unknown:
                        hptr = WinLoadPointer(HWND_DESKTOP, hmodIconsDLL,
                                              102);
                }
            }
        }
    }

    if (hptr == NULLHANDLE)
        // replacement icon does not exist in /ICONS
        // or Global Settings do not allow replacements:
        // call default parent method
        return (XFldProgramFile_parent_WPProgramFile_wpSetProgIcon(somSelf,
                                                                   pfeal));

    // else:

    // set a flag for our wpSetIcon override
    _fProgIconSet = TRUE;

    // some icon from /ICONS found: set it
    _wpSetIcon(somSelf, hptr);
    // make sure this icon is not destroyed;
    // the WPS destroys the icon when the OBJSTYLE_NOTDEFAULTICON
    // bit is set (do not use OBJSTYLE_CUSTOMICON, it is ignored by the WPS)
    _wpModifyStyle(somSelf, OBJSTYLE_NOTDEFAULTICON, 0);
    // _wpModifyStyle(somSelf, OBJSTYLE_CUSTOMICON, 0);
    // _wpModifyStyle(somSelf, OBJSTYLE_NOTDEFAULTICON, 0);

    #ifdef DEBUG_ASSOCS
        _Pmpf(("End of xfpgmf_wpSetProgIcon"));
    #endif

    return (TRUE);
}

/*
 *@@ wpQueryDefaultView:
 *      this WPObject method returns the default view of an object,
 *      that is, which view is opened if the program file is
 *      double-clicked upon. This is also used to mark
 *      the default view in the "Open" context submenu.
 *
 *      For WPProgramFile, the WPS always returns
 *      OPEN_RUNNING (0x04), which doesn't make sense
 *      for DLL's and drivers, which cannot be executed.
 *      We therefore return the value for "first associated
 *      program", which is 0x1000.
 *      Oh yes, as usual, this value is undocumented. ;-)
 */

SOM_Scope ULONG  SOMLINK xfpgmf_wpQueryDefaultView(XFldProgramFile *somSelf)
{
    ULONG ulView;
    ULONG ulProgType = _xwpQueryProgType(somSelf);

    // XFldProgramFileData *somThis = XFldProgramFileGetData(somSelf);
    XFldProgramFileMethodDebug("XFldProgramFile","xfpgmf_wpQueryDefaultView");

    #ifdef DEBUG_ASSOCS
        _Pmpf(( "wpQueryDefaultView for %s", _wpQueryTitle(somSelf) ));
    #endif

    ulView = XFldProgramFile_parent_WPProgramFile_wpQueryDefaultView(somSelf);

    if (    (ulProgType == PROG_XF_DLL)
         || (ulProgType == PROG_XF_DRIVER)
         || (ulProgType == PROG_DEFAULT)
       )
        if (ulView == OPEN_RUNNING)
            // we can neither run DLL's nor drivers, so we
            // pass the first context menu item
            ulView = 0x1000;        // this is the WPS internal code
                                    // for the first association view

    return (ulView);
}

/*
 *@@ wpAddProgramSessionPage:
 *      this instance method adds the "Session" page to
 *      an executable's settings notebook.
 *
 *      We override this method to add our additional
 *      "Module" page after that page, displaying
 *      additional information from the executable module.
 *
 *@@added V0.9.0 [umoeller]
 *@@changed V0.9.5 (2000-08-27) [umoeller]: now skipping for WPCommandFiles
 */

SOM_Scope ULONG  SOMLINK xfpgmf_wpAddProgramSessionPage(XFldProgramFile *somSelf,
                                                        HWND hwndNotebook)
{
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    // XFldProgramFileData *somThis = XFldProgramFileGetData(somSelf);
    XFldProgramFileMethodDebug("XFldProgramFile","xfpgmf_wpAddProgramSessionPage");

    // insert "Module" settings page, but not for command files
    if (!_somIsA(somSelf, _WPCommandFile))
    {
        if (pGlobalSettings->fReplaceFilePage)
        {
            PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();
            PCREATENOTEBOOKPAGE pcnbp = malloc(sizeof(CREATENOTEBOOKPAGE));

            // (2000-12-14) [lafaix] inserting "Resources" page
            memset(pcnbp, 0, sizeof(CREATENOTEBOOKPAGE));
            pcnbp->somSelf = somSelf;
            pcnbp->hwndNotebook = hwndNotebook;
            pcnbp->hmod = cmnQueryNLSModuleHandle(FALSE);
            pcnbp->usPageStyleFlags = BKA_MAJOR;
            pcnbp->pszName = pNLSStrings->pszResourcesPage;
            pcnbp->ulDlgID = ID_XSD_PGMFILE_RESOURCES;
            pcnbp->ulDefaultHelpPanel  = ID_XSH_SETTINGS_PGMFILE_RESOURCES;
            pcnbp->ulPageID = SP_PROG_RESOURCES;
            pcnbp->pfncbInitPage    = fsysResourcesInitPage;
            ntbInsertPage(pcnbp);
            // (2000-12-14) [lafaix] end changes

            // insert "Module" page
            pcnbp = malloc(sizeof(CREATENOTEBOOKPAGE));
            memset(pcnbp, 0, sizeof(CREATENOTEBOOKPAGE));
            pcnbp->somSelf = somSelf;
            pcnbp->hwndNotebook = hwndNotebook;
            pcnbp->hmod = cmnQueryNLSModuleHandle(FALSE);
            pcnbp->usPageStyleFlags = BKA_MAJOR;
            pcnbp->pszName = pNLSStrings->pszModulePage;
            pcnbp->ulDlgID = ID_XSD_PGMFILE_MODULE;
            // pcnbp->usFirstControlID = ID_SDDI_ARCHIVES;
            // pcnbp->ulFirstSubpanel = ID_XSH_SETTINGS_DTP_SHUTDOWN_SUB;   // help panel for "System setup"
            pcnbp->ulDefaultHelpPanel  = ID_XSH_SETTINGS_PGMFILE_MODULE;
            pcnbp->ulPageID = SP_PROG_DETAILS;
            pcnbp->pfncbInitPage    = fsysProgramInitPage;
            ntbInsertPage(pcnbp);

            /* PCREATENOTEBOOKPAGE pcnbp = malloc(sizeof(CREATENOTEBOOKPAGE));
            PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();

            memset(pcnbp, 0, sizeof(CREATENOTEBOOKPAGE));
            // insert "Module" page
            pcnbp = malloc(sizeof(CREATENOTEBOOKPAGE));
            memset(pcnbp, 0, sizeof(CREATENOTEBOOKPAGE));
            pcnbp->somSelf = somSelf;
            pcnbp->hwndNotebook = hwndNotebook;
            pcnbp->hmod = cmnQueryNLSModuleHandle(FALSE);
            pcnbp->usPageStyleFlags = BKA_MAJOR;
            pcnbp->pszName = pNLSStrings->pszModulePage;
            pcnbp->ulDlgID = ID_XSD_PGMFILE_MODULE;
            // pcnbp->usFirstControlID = ID_SDDI_ARCHIVES;
            // pcnbp->ulFirstSubpanel = ID_XSH_SETTINGS_DTP_SHUTDOWN_SUB;   // help panel for "System setup"
            pcnbp->ulDefaultHelpPanel  = ID_XSH_SETTINGS_PGMFILE_MODULE;
            pcnbp->ulPageID = SP_PROG_DETAILS;
            pcnbp->pfncbInitPage    = fsysProgramInitPage;
            ntbInsertPage(pcnbp); */
        }
    }

    return (XFldProgramFile_parent_WPProgramFile_wpAddProgramSessionPage(somSelf,
                                                                         hwndNotebook));
}

/*
 *@@ wpFilterPopupMenu:
 *      this WPObject instance method allows the object to
 *      filter out unwanted menu items from the context menu.
 *      This gets called before wpModifyPopupMenu.
 *
 *      We remove the "Program" context menu entry for
 *      DLL's and drivers.
 */

SOM_Scope ULONG  SOMLINK xfpgmf_wpFilterPopupMenu(XFldProgramFile *somSelf,
                                                  ULONG ulFlags,
                                                  HWND hwndCnr,
                                                  BOOL fMultiSelect)
{
    ULONG ulFilter;
    ULONG ulProgType = _xwpQueryProgType(somSelf);

    /* XFldProgramFileData *somThis = XFldProgramFileGetData(somSelf); */
    XFldProgramFileMethodDebug("XFldProgramFile","xfpgmf_wpFilterPopupMenu");

    ulFilter = XFldProgramFile_parent_WPProgramFile_wpFilterPopupMenu(somSelf,
                                                                      ulFlags,
                                                                      hwndCnr,
                                                                      fMultiSelect);
    if (    (ulProgType == PROG_XF_DLL)
         || (ulProgType == PROG_XF_DRIVER)
         || (ulProgType == PROG_DEFAULT)
       )
        ulFilter &= ~ CTXT_PROGRAM;

    return (ulFilter);
}

/* ******************************************************************
 *
 *   XFldProgramFile class methods
 *
 ********************************************************************/

/*
 *@@ wpclsInitData:
 *      initialize XFldProgramFile class data.
 *
 *@@changed V0.9.0 [umoeller]: added class object to KERNELGLOBALS
 */

SOM_Scope void  SOMLINK xfpgmfM_wpclsInitData(M_XFldProgramFile *somSelf)
{
    // M_XFldProgramFileData *somThis = M_XFldProgramFileGetData(somSelf);
    M_XFldProgramFileMethodDebug("M_XFldProgramFile","xfpgmfM_wpclsInitData");

    M_XFldProgramFile_parent_M_WPProgramFile_wpclsInitData(somSelf);

    {
        // store the class object in KERNELGLOBALS
        PKERNELGLOBALS   pKernelGlobals = krnLockGlobals(__FILE__, __LINE__, __FUNCTION__);
        if (pKernelGlobals)
        {
           pKernelGlobals->fXFldProgramFile = TRUE;
           krnUnlockGlobals();
        }
    }
}

/*
 *@@ wpclsQueryInstanceFilter:
 *      this WPDataFile class method determines which file-system
 *      objects will be instances of a certain class according
 *      to a file filter.
 *
 *      To avoid the annoying behavior of OS/2 Warp 4 that some
 *      DLL's are instances of WPProgramFile and some are not,
 *      we make all DLL's instances of WPProgramFile by specifying
 *      "*.DLL" too.
 *
 *      This does not work using wpclsQueryInstance type,
 *      because the WPS seems to be using some default file
 *      type of "Executable", which is determined in some hidden
 *      place.
 */

SOM_Scope PSZ  SOMLINK xfpgmfM_wpclsQueryInstanceFilter(M_XFldProgramFile *somSelf)
{
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    /* M_XFldProgramFileData *somThis = M_XFldProgramFileGetData(somSelf); */
    M_XFldProgramFileMethodDebug("M_XFldProgramFile","xfpgmfM_wpclsQueryInstanceFilter");

    if (    (somSelf == _XFldProgramFile)
         && (pGlobalSettings->fReplaceIcons)
       )
    {
        return ((PSZ)G_pcszInstanceFilter);
    }
    else
        return (M_XFldProgramFile_parent_M_WPProgramFile_wpclsQueryInstanceFilter(somSelf));
}

