
/*
 *@@sourcefile iconpage.c:
 *      implementation code for the replacement object
 *      "Icon" page.
 *
 *      This file is ALL new with V0.9.20. The code used
 *      to be in icons.c before and hasn't changed.
 *
 *      Function prefix for this file:
 *      --  ico*
 *
 *@@header "filesys\icons.h"
 */

/*
 *      Copyright (C) 2001-2002 Ulrich M”ller.
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
#define INCL_DOSRESOURCES
#define INCL_DOSMODULEMGR
#define INCL_DOSMISC
#define INCL_DOSERRORS

#define INCL_WININPUT
#define INCL_WINWINDOWMGR
#define INCL_WINMESSAGEMGR
#define INCL_WINPOINTERS
#define INCL_WINSYS
#define INCL_WINTIMER
#define INCL_WINMENUS
#define INCL_WINDIALOGS
#define INCL_WINSTATICS
#define INCL_WINBUTTONS
#define INCL_WINENTRYFIELDS
#define INCL_WINMLE
#define INCL_WINSTDCNR
#define INCL_WINSTDBOOK
#define INCL_WINPROGRAMLIST     // needed for wppgm.h
#define INCL_WINSHELLDATA
#define INCL_WINERRORS

#define INCL_GPIPRIMITIVES
#define INCL_GPIBITMAPS
#include <os2.h>

// C library headers
#include <stdio.h>
// #include <malloc.h>
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\apps.h"               // application helpers
#include "helpers\comctl.h"             // common controls (window procs)
#include "helpers\dialog.h"             // dialog helpers
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\exeh.h"               // executable helpers
#include "helpers\gpih.h"               // GPI helper routines
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\standards.h"          // some standard macros
#include "helpers\stringh.h"            // string helper routines
#include "helpers\tree.h"               // red-black binary trees
#include "helpers\winh.h"               // PM helper routines
#include "helpers\xstring.h"            // extended string helpers

// SOM headers which don't crash with prec. header files
#include "xfldr.ih"
#include "xfobj.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\cnrsort.h"             // container sort comparison functions
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\errors.h"              // private XWorkplace error codes
#include "shared\helppanels.h"          // all XWorkplace help panel IDs
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

// headers in /hook
#include "hook\xwphook.h"

#include "filesys\filesys.h"            // various file-system object implementation code
#include "filesys\icons.h"              // icons handling
#include "filesys\object.h"             // XFldObject implementation
#include "filesys\program.h"            // program implementation; WARNING: this redefines macros

#include "config\hookintf.h"            // daemon/hook interface

// other SOM headers
#include "helpers\undoc.h"              // some undocumented stuff
#pragma hdrstop

/* ******************************************************************
 *
 *   Object icon management
 *
 ********************************************************************/

/*
 *@@ icoRunReplacement:
 *      returns TRUE if either turbo folders or
 *      extended associations are enabled.
 *
 *      Used mainly by XWPProgramFile and
 *      XWPProgram.
 *
 *@@added V0.9.18 (2002-03-16) [umoeller]
 */

BOOL icoRunReplacement(VOID)
{
    BOOL        fRunReplacement = FALSE;

    // turbo folders enabled?
#ifndef __NOTURBOFOLDERS__
    fRunReplacement = cmnQuerySetting(sfTurboFolders);
#endif

#ifndef __NOICONREPLACEMENTS__
    if (!fRunReplacement)
        if (cmnQuerySetting(sfIconReplacements))
            fRunReplacement = TRUE;
#endif

    return (fRunReplacement);
}

/*
 *@@ icoClsQueryMaxAnimationIcons:
 *      evil helper for calling wpclsQueryMaxAnimationIcons,
 *      which is undocumented.
 *
 *      See xfTP_wpclsQueryMaxAnimationIcons.
 *
 *      Note that this calls a class method, so pass in a
 *      class object.
 *
 *@@added V0.9.19 (2002-06-15) [umoeller]
 */

ULONG icoClsQueryMaxAnimationIcons(M_WPObject *somSelf)
{
    xfTD_wpclsQueryMaxAnimationIcons _wpclsQueryMaxAnimationIcons;
    if (_wpclsQueryMaxAnimationIcons = (xfTD_wpclsQueryMaxAnimationIcons)
                              wpshResolveFor(somSelf,
                                             NULL,
                                             "wpclsQueryMaxAnimationIcons"))
        return _wpclsQueryMaxAnimationIcons(somSelf);

    return 0;
}

/*
 *@@ icoQueryIconN:
 *      evil helper for calling wpQueryIcon
 *      or wpQueryIconN, which is undocumented.
 *
 *      Returns 0 on errors.
 *
 *@@added V0.9.16 (2001-10-15) [umoeller]
 */

HPOINTER icoQueryIconN(WPObject *pobj,    // in: object
                       ULONG ulIndex)     // in: animation index or 0 for regular icon
{
    if (!ulIndex)
        return (_wpQueryIcon(pobj));

    // index specified: this better be a folder, and we better be Warp 4
    if (    (G_fIsWarp4)
         && (_somIsA(pobj, _WPFolder))
       )
    {
        xfTD_wpQueryIconN pwpQueryIconN;

        if (pwpQueryIconN
            = (xfTD_wpQueryIconN)wpshResolveFor(pobj,
                                                NULL, // use somSelf's class
                                                "wpQueryIconN"))
            return (pwpQueryIconN(pobj, ulIndex));
    }

    return 0;
}

/*
 *@@ icoQueryIconDataN:
 *      evil helper for calling wpQueryIconData
 *      or wpQueryIconDataN, which is undocumented.
 *
 *      Returns 0 on errors.
 *
 *      The "query icon data" methods return
 *      an ICONINFO structure. This only sometimes
 *      contains the "icon data" (which is why the
 *      method name is very misleading), but sometimes
 *      instructions for the caller about the icon
 *      format:
 *
 *      --  With ICON_RESOURCE, the caller will
 *          find the icon data in the specified
 *          icon resource.
 *
 *      --  With ICON_FILE, the caller will have
 *          to load an icon file.
 *
 *      --  Only with ICON_DATA, the icon data is
 *          actually returned. This is usually
 *          returned if the object has a user icon
 *          set in OS2.INI (for abstracts) or
 *          in the .ICONx EA (for fs objects)
 *
 *      As a result, icoLoadIconData was added
 *      which actually loads the icon data if
 *      the format isn't ICON_DATA already.
 *
 *@@added V0.9.16 (2001-10-15) [umoeller]
 */

ULONG icoQueryIconDataN(WPObject *pobj,    // in: object
                        ULONG ulIndex,     // in: animation index or 0 for regular icon
                        PICONINFO pData)   // in: icon data buffer or NULL for "query size"
{
    if (!ulIndex)
        return (_wpQueryIconData(pobj, pData));

    // index specified: this better be a folder, and we better be Warp 4
    if (    (G_fIsWarp4)
         && (_somIsA(pobj, _WPFolder))
       )
    {
        xfTD_wpQueryIconDataN pwpQueryIconDataN;

        if (pwpQueryIconDataN
            = (xfTD_wpQueryIconDataN)wpshResolveFor(pobj,
                                                    NULL, // use somSelf's class
                                                    "wpQueryIconDataN"))
            return (pwpQueryIconDataN(pobj, pData, ulIndex));
    }

    return 0;
}

/*
 *@@ icoSetIconDataN:
 *      evil helper for calling wpSetIconData
 *      or wpSetIconDataN, which is undocumented.
 *
 *      While wpSetIcon only temporarily changes
 *      an object's icon (which is then returned
 *      by wpQueryIcon), wpSetIconData stores a
 *      new persistent icon as well. For abstracts,
 *      this will go into OS2.INI, for fs objects
 *      into the object's EAs.
 *
 *      Note that as a special hack, wpSetIconData
 *      supports the ICON_CLEAR "format", which
 *      resets the icon data to the class's default
 *      icon (or something else for special classes
 *      like WPDisk, WPDataFile or WPProgram).
 *
 *      Returns 0 on errors.
 *
 *@@added V0.9.16 (2001-10-15) [umoeller]
 */

BOOL icoSetIconDataN(WPObject *pobj,    // in: object
                     ULONG ulIndex,     // in: animation index or 0 for regular icon
                     PICONINFO pData)   // in: icon data to set (requried)
{
    if (!ulIndex)
        return (_wpSetIconData(pobj, pData));

    // index specified: this better be a folder, and we better be Warp 4
    if (    (G_fIsWarp4)
         && (_somIsA(pobj, _WPFolder))
       )
    {
        xfTD_wpSetIconDataN pwpSetIconDataN;

        if (pwpSetIconDataN
            = (xfTD_wpSetIconDataN)wpshResolveFor(pobj,
                                                  NULL, // use somSelf's class
                                                  "wpSetIconDataN"))
            return (pwpSetIconDataN(pobj, pData, ulIndex));
    }

    return 0;
}

/*
 *@@ icoClsQueryIconN:
 *      evil helper for calling wpclsQueryIconN,
 *      which is undocumented.
 *
 *      WARNING: This always builds a new HPOINTER,
 *      so the caller should use WinDestroyPointer
 *      to avoid resource leaks.
 *
 *@@added V0.9.16 (2001-10-19) [umoeller]
 */

HPOINTER icoClsQueryIconN(SOMClass *pClassObject,
                          ULONG ulIndex)
{
    if (!ulIndex)
        return (_wpclsQueryIcon(pClassObject));

    // index specified: this better be a folder, and we better be Warp 4
    if (    (G_fIsWarp4)
         && (_somDescendedFrom(pClassObject, _WPFolder))
       )
    {
        xfTD_wpclsQueryIconN pwpclsQueryIconN;

        if (pwpclsQueryIconN
            = (xfTD_wpclsQueryIconN)wpshResolveFor(pClassObject,
                                                   NULL, // use somSelf's class
                                                   "wpclsQueryIconN"))
            return (pwpclsQueryIconN(pClassObject, ulIndex));
    }

    return 0;
}

/*
 *@@ icoLoadIconData:
 *      retrieves the ICONINFO for the specified
 *      object and animation index in a new buffer.
 *
 *      If ulIndex == 0, this retrieves the standard
 *      icon. Otherwise this returns the animation icon.
 *      Even though the WPS always uses this stupid
 *      index with the icon method calls, I don't think
 *      any index besides 1 is actually supported.
 *
 *      If this returns NO_ERROR, the given PICONINFO*
 *      will receive a pointer to a newly allocated
 *      ICONINFO buffer whose format is always ICON_DATA.
 *      This will properly load an icon resource if the
 *      object has the icon format set to ICON_RESOURCE
 *      or ICON_FILE.
 *
 *      If NO_ERROR is returned, the caller must free()
 *      this pointer.
 *
 *      Otherwise this might return the following errors:
 *
 *      --  ERROR_NO_DATA: icon format not understood.
 *
 *      --  ERROR_FILE_NOT_FOUND: icon file doesn't exist.
 *
 *      plus those of doshMalloc and DosGetResource, such as
 *      ERROR_NOT_ENOUGH_MEMORY.
 *
 *      This is ICONINFO:
 *
 +      typedef struct _ICONINFO {
 +        ULONG       cb;           // Length of the ICONINFO structure.
 +        ULONG       fFormat;      // Indicates where the icon resides.
 +        PSZ         pszFileName;  // Name of the file containing icon data (ICON_FILE)
 +        HMODULE     hmod;         // Module containing the icon resource (ICON_RESOURCE)
 +        ULONG       resid;        // Identity of the icon resource (ICON_RESOURCE)
 +        ULONG       cbIconData;   // Length of the icon data in bytes (ICON_DATA)
 +        PVOID       pIconData;    // Pointer to the buffer containing icon data (ICON_DATA)
 +      } ICONINFO;
 *
 *@@added V0.9.16 (2001-10-15) [umoeller]
 *@@changed V0.9.18 (2002-03-19) [umoeller]: no longer recursing, which didn't work anyway
 */

APIRET icoLoadIconData(WPObject *pobj,             // in: object whose icon to query
                       ULONG ulIndex,              // in: animation index or 0 for regular icon
                       PICONINFO *ppIconInfo)      // out: ICONINFO allocated via _wpAllocMem
{
    APIRET arc = NO_ERROR;
    PICONINFO pData = NULL;
    ULONG cbIconInfo;

    arc = ERROR_NO_DATA;     // whatever, this shouldn't fail

    *ppIconInfo = NULL;

    // find out how much memory the object needs for this
    if (    (cbIconInfo = icoQueryIconDataN(pobj,
                                            ulIndex,
                                            NULL))        // query size
                                // if this fails, arc is still ERROR_NO_DATA
            // allocate the memory
         && (pData = doshMalloc(cbIconInfo, &arc))
       )
    {
#ifdef DEBUG_ICONREPLACEMENTS
        _PmpfF(("allocated %d bytes", cbIconInfo));
#endif

        // ask the object again
        if (icoQueryIconDataN(pobj,
                              ulIndex,
                              pData))
        {
#ifdef DEBUG_ICONREPLACEMENTS
            _Pmpf(("   got %d bytes data", cbIconInfo));
#endif

            // get the icon data depending on the format
            switch (pData->fFormat)
            {
                case ICON_RESOURCE:
                {
                    ULONG   cbResource;
                    PVOID   pvResourceTemp;
#ifdef DEBUG_ICONREPLACEMENTS
                    _Pmpf(("   ICON_RESOURCE 0x%lX, %d", pData->hmod, pData->resid));
#endif

                    // object has specified icon resource:
                    // load resource data...
                    if (    (!(arc = DosQueryResourceSize(pData->hmod,
                                                          RT_POINTER,
                                                          pData->resid,
                                                          &cbResource)))
                       )
                    {
                        if (!cbResource)
                            arc = ERROR_NO_DATA;
                        else if (!(arc = DosGetResource(pData->hmod,
                                                        RT_POINTER,
                                                        pData->resid,
                                                        &pvResourceTemp)))
                        {
                            // loaded OK:
                            // return a new ICONINFO then
                            PICONINFO pData2;
                            ULONG     cb2 = sizeof(ICONINFO) + cbResource;
                            if (pData2 = doshMalloc(cb2, &arc))
                            {
                                // point icon data to after ICONINFO struct
                                PBYTE pbIconData = (PBYTE)pData2 + sizeof(ICONINFO);
                                pData2->cb = sizeof(ICONINFO);
                                pData2->fFormat = ICON_DATA;
                                pData2->cbIconData = cbResource;
                                pData2->pIconData = pbIconData;
                                // copy icon data there
                                memcpy(pbIconData, pvResourceTemp, cbResource);

                                // and return the new struct
                                *ppIconInfo = pData2;
                            }

                            // in any case, free the original
                            free(pData);
                            pData = NULL;       // do not free again below

                            DosFreeResource(pvResourceTemp);
                        }
                    }
                }
                break;

                case ICON_DATA:
#ifdef DEBUG_ICONREPLACEMENTS
                    _Pmpf(("   ICON_DATA"));
#endif

                    // this is OK, no conversion needed
                    *ppIconInfo = pData;
                    arc = NO_ERROR;
                break;

                case ICON_FILE:
                {
                    WPFileSystem *pfs;
#ifdef DEBUG_ICONREPLACEMENTS
                    _Pmpf(("   ICON_FILE \"%s\"", pData->pszFileName));
#endif

                    if (    (pData->pszFileName)
                         && (pfs = _wpclsQueryObjectFromPath(_WPFileSystem,
                                                             pData->pszFileName))
                       )
                    {
                        ULONG cbRequired;
                        if (!(cbRequired = _wpQueryIconData(pfs, NULL)))
                            arc = ERROR_NO_DATA;
                        else
                        {
                            PICONINFO pData2;
                            if (pData2 = doshMalloc(cbRequired, &arc))
                            {
                                if (!_wpQueryIconData(pfs, pData2))
                                {
                                    arc = ERROR_NO_DATA;
                                    free(pData2);
                                }
                                else
                                {
                                    free(pData);
                                    *ppIconInfo = pData2;
                                    arc = NO_ERROR;
                                }
                            }
                        }

                        _wpUnlockObject(pfs);
                    }
                    else
                        arc = ERROR_FILE_NOT_FOUND;
                }
                break;

                default:
                    // any other format:
#ifdef DEBUG_ICONREPLACEMENTS
                    _Pmpf(("    invalid format %d", pData->fFormat));
#endif

                    arc = ERROR_INVALID_DATA;
            } // end switch (pData->Format)
        } // end if (_wpQueryIconData(pobj, pData))
        else
            arc = ERROR_NO_DATA;

        if (arc && pData)
            free(pData);
    }

#ifdef DEBUG_ICONREPLACEMENTS
    _PmpfF(("returning %d", arc));
#endif

    return arc;
}

/*
 *@@ icoCopyIconFromObject:
 *      sets a new persistent icon for somSelf
 *      by copying the icon data from pobjSource.
 *
 *@@added V0.9.16 (2001-10-15) [umoeller]
 */

APIRET icoCopyIconFromObject(WPObject *somSelf,       // in: target
                             WPObject *pobjSource,    // in: source
                             ULONG ulIndex)           // in: animation index or 0 for regular icon
{
    APIRET arc = NO_ERROR;

    if (pobjSource = objResolveIfShadow(pobjSource))
    {
        PICONINFO pData;
        if (arc = icoLoadIconData(pobjSource, ulIndex, &pData))
            // error loading that icon:
            // if we're trying to load an animation icon
            // (i.e. ulIndex != 0), try loading index 0 and
            // set that on the animation icon... the user
            // might be trying to drag a regular icon on
            // the animation page
            // V0.9.16 (2001-12-08) [umoeller]
            arc = icoLoadIconData(pobjSource, 0, &pData);

        if (!arc)
        {
            // now set this icon for the target object
            icoSetIconDataN(somSelf, ulIndex, pData);
            free(pData);

            // the standard WPS behavior is that
            // if a folder icon is copied onto the
            // standard icon of a folder, the animation
            // icon is copied too... so check:
            if (    (_somIsA(somSelf, _WPFolder))
                 && (_somIsA(pobjSource, _WPFolder))
                 // but don't do this if we're setting the
                 // animation icon explicitly
                 && (!ulIndex)
               )
            {
                // alright, copy animation icon too,
                // but if this fails, don't return an error
                if (!icoLoadIconData(pobjSource, 1, &pData))
                {
                    icoSetIconDataN(somSelf, 1, pData);
                    free(pData);
                }
            }
        }
    }
    else
        arc = ERROR_FILE_NOT_FOUND;

    return arc;
}

/*
 *@@ objResetIcon:
 *      resets an object's icon to its default.
 *
 *      See the explanations about ICON_CLEAR
 *      with icoSetIconDataN.
 *
 *@@added V0.9.16 (2001-10-15) [umoeller]
 */

VOID objResetIcon(WPObject *somSelf,
                  ULONG ulIndex)
{
    ICONINFO Data;
    memset(&Data, 0, sizeof(Data));
    Data.fFormat = ICON_CLEAR;
    icoSetIconDataN(somSelf,
                    ulIndex,
                    &Data);
}

/*
 *@@ icoIsUsingDefaultIcon:
 *      returns TRUE if the object is using
 *      a default icon for the specified
 *      animation index.
 *
 *      This call is potentially expensive so
 *      don't use it excessively.
 *
 *@@added V0.9.16 (2001-10-19) [umoeller]
 *@@changed V0.9.18 (2002-03-19) [umoeller]: fixed disappearing animation icon
 */

BOOL icoIsUsingDefaultIcon(WPObject *pobj,
                           ULONG ulAnimationIndex)
{
    if (ulAnimationIndex)
    {
        // caller wants animation icon checked:
        // compare this object's icon to the class's
        // default animation icon
        BOOL brc = FALSE;
        HPOINTER hptrClass = icoClsQueryIconN(_somGetClass(pobj),
                                              ulAnimationIndex);
        if (    hptrClass
             && (icoQueryIconN(pobj,
                               ulAnimationIndex)
                 == hptrClass)
           )
            brc = TRUE;

#ifndef __NOICONREPLACEMENTS__
        // only destroy the pointer if we're not
        // running our replacement because then
        // it is shared
        // V0.9.18 (2002-03-19) [umoeller]
        if (!cmnQuerySetting(sfIconReplacements))
#endif
            WinDestroyPointer(hptrClass);

        return brc;
    }

    // caller wants regular icon checked:
    // do NOT compare it to the class default icon
    // but check the object style instead (there are
    // many default icons in the WPS which are _not_
    // class default icons, e.g. WPProgram and WPDataFile
    // association icons)
    return ((0 == (_wpQueryStyle(pobj)
                            & OBJSTYLE_NOTDEFAULTICON)));
}

/* ******************************************************************
 *
 *   Replacement object "Icon" page
 *
 ********************************************************************/

#define ICON_WIDTH          40
#define BUTTON_WIDTH        45
#define GROUPS_WIDTH       175
#define EF_HEIGHT           25
#define HOTKEY_EF_WIDTH     50

static const CONTROLDEF
    TitleGroup = LOADDEF_GROUP(ID_XSDI_ICON_TITLE_TEXT, SZL_AUTOSIZE),
    TitleEF = CONTROLDEF_MLE(
                            NULL,
                            ID_XSDI_ICON_TITLE_EF,
                            GROUPS_WIDTH - 2 * COMMON_SPACING,
                            MAKE_SQUARE_CY(EF_HEIGHT)),
    IconGroup = CONTROLDEF_GROUP(
                            LOAD_STRING,
                            ID_XSDI_ICON_GROUP,
                            GROUPS_WIDTH,
                            -1),
    IconStatic =
        {
            WC_STATIC,
            NULL,
            WS_VISIBLE | SS_TEXT | DT_LEFT | DT_VCENTER | DT_MNEMONIC,
            ID_XSDI_ICON_STATIC,
            CTL_COMMON_FONT,
            0,
            {ICON_WIDTH, MAKE_SQUARE_CY(ICON_WIDTH)},
            COMMON_SPACING
        },
    IconExplanationText = CONTROLDEF_TEXT_WORDBREAK(
                            LOAD_STRING,
                            ID_XSDI_ICON_EXPLANATION_TXT,
                            -100),      // use table width
    IconEditButton = CONTROLDEF_PUSHBUTTON(
                            LOAD_STRING,
                            ID_XSDI_ICON_EDIT_BUTTON,
                            -1,
                            STD_BUTTON_HEIGHT),
    IconBrowseButton = CONTROLDEF_PUSHBUTTON(
                            LOAD_STRING,
                            DID_BROWSE,
                            -1,
                            STD_BUTTON_HEIGHT),
    IconResetButton = CONTROLDEF_PUSHBUTTON(
                            LOAD_STRING,
                            ID_XSDI_ICON_RESET_BUTTON,
                            -1,
                            STD_BUTTON_HEIGHT),
    ExtrasGroup = CONTROLDEF_GROUP(
                            LOAD_STRING,
                            ID_XSDI_ICON_EXTRAS_GROUP,
                            GROUPS_WIDTH,
                            -1),
    HotkeyText = CONTROLDEF_TEXT(
                            LOAD_STRING,
                            ID_XSDI_ICON_HOTKEY_TEXT,
                            -1,
                            -1),
    HotkeyEF = CONTROLDEF_ENTRYFIELD(
                            NULL,
                            ID_XSDI_ICON_HOTKEY_EF,
                            HOTKEY_EF_WIDTH,
                            -1),
    HotkeyClearButton = CONTROLDEF_PUSHBUTTON(
                            LOAD_STRING,
                            ID_XSDI_ICON_HOTKEY_CLEAR,
                            -1,
                            STD_BUTTON_HEIGHT),
    HotkeySetButton = CONTROLDEF_PUSHBUTTON(
                            LOAD_STRING,
                            ID_XSDI_ICON_HOTKEY_SET,
                            -1,
                            STD_BUTTON_HEIGHT),
    LockPositionCB = LOADDEF_AUTOCHECKBOX(ID_XSDI_ICON_LOCKPOSITION_CB),
    TemplateCB = LOADDEF_AUTOCHECKBOX(ID_XSDI_ICON_TEMPLATE_CB),
    DetailsButton = CONTROLDEF_PUSHBUTTON(
                            LOAD_STRING,
                            DID_DETAILS,
                            -1,
                            STD_BUTTON_HEIGHT);

static const DLGHITEM dlgObjIconFront[] =
    {
        START_TABLE,            // root table, required
    };

static const DLGHITEM dlgObjIconTitle[] =
    {
             START_ROW(ROW_VALIGN_TOP),       // row 1 in the root table, required
            START_ROW(ROW_VALIGN_CENTER),
                START_GROUP_TABLE(&TitleGroup),
                // CONTROL_DEF(&TitleText),
                    START_ROW(0),
                        CONTROL_DEF(&TitleEF),
                END_TABLE,
    };

static const DLGHITEM dlgObjIconIcon[] =
    {
            START_ROW(ROW_VALIGN_CENTER),
                START_GROUP_TABLE(&IconGroup),
                    START_ROW(0),
                        CONTROL_DEF(&IconStatic),
                    START_TABLE,
                        START_ROW(0),
                            CONTROL_DEF(&IconExplanationText),
                        START_ROW(0),
                            CONTROL_DEF(&IconEditButton),
                            CONTROL_DEF(&IconBrowseButton),
                            CONTROL_DEF(&IconResetButton),
                    END_TABLE,
                END_TABLE,
    };

static const DLGHITEM dlgObjIconExtrasFront[] =
    {
            START_ROW(ROW_VALIGN_CENTER),
                START_GROUP_TABLE(&ExtrasGroup),
                    START_ROW(0)
    };

static const DLGHITEM dlgObjIconHotkey[] =
    {
                        START_ROW(ROW_VALIGN_CENTER),
                            CONTROL_DEF(&HotkeyText),
                            CONTROL_DEF(&HotkeyEF),
                            CONTROL_DEF(&HotkeySetButton),
                            CONTROL_DEF(&HotkeyClearButton),
                        START_ROW(ROW_VALIGN_CENTER)
    };

static const DLGHITEM dlgObjIconTemplate[] =
    {
                            CONTROL_DEF(&TemplateCB)
    };

static const DLGHITEM dlgObjIconLockedInPlace[] =
    {
                            CONTROL_DEF(&LockPositionCB)
    };

static const DLGHITEM dlgObjIconExtrasTail[] =
    {
                END_TABLE
    };

static const DLGHITEM dlgObjIconDetails[] =
    {
                CONTROL_DEF(&DetailsButton)
    };

static const DLGHITEM dlgObjIconTail[] =
    {
            START_ROW(0),
                CONTROL_DEF(&G_UndoButton),         // common.c
                CONTROL_DEF(&G_DefaultButton),      // common.c
                CONTROL_DEF(&G_HelpButton),         // common.c
        END_TABLE
    };

/*
 *@@ icoFormatIconPage:
 *      calls ntbFormatPage for the given empty dialog
 *      frame to insert the formatted controls for
 *      the "Icon" page.
 *
 *      flFlags determines the special controls to be added.
 *      This can be any combination of the following:
 *
 *      --  ICONFL_TITLE:       add the "Title" MLE.
 *
 *      --  ICONFL_ICON:        add the "Icon" controls.
 *
 *      --  ICONFL_TEMPLATE:    add the "Template" checkbox.
 *
 *      --  ICONFL_LOCKEDINPLACE: add the "Locked in place" checkbox.
 *
 *      --  ICONFL_HOTKEY:      add the "Hotkey" entry field.
 *
 *      --  ICONFL_DETAILS:     add the "Details" pushbutton.
 *
 *      The "Template" checkbox is automatically added only
 *      if the object's class doesn't have the CLSSTYLE_NEVERTEMPLATE
 *      class style flag set.
 *
 *@@added V0.9.16 (2001-10-15) [umoeller]
 *@@changed V0.9.18 (2002-03-03) [umoeller]: changed ordering
 */

VOID icoFormatIconPage(PNOTEBOOKPAGE pnbp,
                       ULONG flFlags)
{
    APIRET arc;

    PDLGARRAY pArrayIcon = NULL;
    if (!(arc = dlghCreateArray(   ARRAYITEMCOUNT(dlgObjIconFront)
                                 + ARRAYITEMCOUNT(dlgObjIconTitle)
                                 + ARRAYITEMCOUNT(dlgObjIconIcon)
                                 + ARRAYITEMCOUNT(dlgObjIconExtrasFront)
                                 + ARRAYITEMCOUNT(dlgObjIconHotkey)
                                 + ARRAYITEMCOUNT(dlgObjIconTemplate)
                                 + ARRAYITEMCOUNT(dlgObjIconLockedInPlace)
                                 + ARRAYITEMCOUNT(dlgObjIconDetails)
                                 + ARRAYITEMCOUNT(dlgObjIconExtrasTail)
                                 + ARRAYITEMCOUNT(dlgObjIconTail),
                                &pArrayIcon)))
    {
        if (!(arc = dlghAppendToArray(pArrayIcon,
                                      dlgObjIconFront,
                                      ARRAYITEMCOUNT(dlgObjIconFront))))
        {
            // "icon title" fields
            if (flFlags & ICONFL_TITLE)
                arc = dlghAppendToArray(pArrayIcon,
                                        dlgObjIconTitle,
                                        ARRAYITEMCOUNT(dlgObjIconTitle));

            if (    (!arc)
                 && (flFlags & (  ICONFL_TEMPLATE
                                | ICONFL_LOCKEDINPLACE
                                | ICONFL_HOTKEY))
               )
            {
                arc = dlghAppendToArray(pArrayIcon,
                                        dlgObjIconExtrasFront,
                                        ARRAYITEMCOUNT(dlgObjIconExtrasFront));

                // "Template" checkbox
                if ( (!arc) && (flFlags & ICONFL_TEMPLATE) )
                    arc = dlghAppendToArray(pArrayIcon,
                                            dlgObjIconTemplate,
                                            ARRAYITEMCOUNT(dlgObjIconTemplate));

                // "Lock in place" checkbox
                if ( (!arc) && (flFlags & ICONFL_LOCKEDINPLACE) )
                    arc = dlghAppendToArray(pArrayIcon,
                                            dlgObjIconLockedInPlace,
                                            ARRAYITEMCOUNT(dlgObjIconLockedInPlace));

                // "hotkey" group
                if ( (!arc) && (flFlags & ICONFL_HOTKEY) )
                    arc = dlghAppendToArray(pArrayIcon,
                                            dlgObjIconHotkey,
                                            ARRAYITEMCOUNT(dlgObjIconHotkey));

                if (!arc)
                    arc = dlghAppendToArray(pArrayIcon,
                                            dlgObjIconExtrasTail,
                                            ARRAYITEMCOUNT(dlgObjIconExtrasTail));
            }

            // icon control fields
            // moved these to the bottom V0.9.18 (2002-03-03) [umoeller]
            if ( (!arc) && (flFlags & ICONFL_ICON) )
                arc = dlghAppendToArray(pArrayIcon,
                                        dlgObjIconIcon,
                                        ARRAYITEMCOUNT(dlgObjIconIcon));

            // "Details" button
            if ( (!arc) && (flFlags & ICONFL_DETAILS) )
                arc = dlghAppendToArray(pArrayIcon,
                                        dlgObjIconDetails,
                                        ARRAYITEMCOUNT(dlgObjIconDetails));

            // main tail
            if (    (!arc)
                 && (!(arc = dlghAppendToArray(pArrayIcon,
                                               dlgObjIconTail,
                                               ARRAYITEMCOUNT(dlgObjIconTail))))
               )
            {
                ntbFormatPage(pnbp->hwndDlgPage,
                              pArrayIcon->paDlgItems,
                              pArrayIcon->cDlgItemsNow);
            }
        }

        if (arc)
            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                   "dlghAppendToArray returned %d",
                   arc);

        dlghFreeArray(&pArrayIcon);
    }
}

/*
 *@@ OBJICONPAGEDATA:
 *
 *@@added V0.9.16 (2001-10-15) [umoeller]
 */

typedef struct _OBJICONPAGEDATA
{
    ULONG           flIconPageFlags;            // flags for icoFormatIconPage so we
                                            // know which fields are available

    HWND            hwndIconStatic,
                    hwndHotkeyEF;

    ULONG           ulAnimationIndex;           // 0 if main icon page
                                                // 1 if folder animation page

    // backup for "Undo"
    PICONINFO       pIconDataBackup;            // backup of icon data... if this is NULL,
                                            // the object was using a default icon
    PSZ             pszTitleBackup;             // backup of object title
    BOOL            fNoRename,                  // TRUE if object has OBJSTYLE_NORENAME
                    fTemplateBackup,
                    fLockedInPlaceBackup;

    // data for subclassed icon
    PNOTEBOOKPAGE   pnbp;              // reverse linkage for subclassed icon
    PFNWP           pfnwpIconOriginal;
    WPObject        *pobjDragged;

    // data for subclassed hotkey EF
    // function keys
    PFUNCTIONKEY    paFuncKeys;
    ULONG           cFuncKeys;

    BOOL            fHotkeysWorking,
                    fHasHotkey,
                    fHotkeyPending;

    // object hotkey
    XFldObject_OBJECTHOTKEY Hotkey;
    BOOL            fInitiallyHadHotkey;
    XFldObject_OBJECTHOTKEY HotkeyBackup;   // if (fInitiallyHadHotkey)

} OBJICONPAGEDATA, *POBJICONPAGEDATA;

/*
 *@@ PaintIcon:
 *
 *@@added V0.9.16 (2001-10-15) [umoeller]
 */

static VOID PaintIcon(POBJICONPAGEDATA pData,
                      HWND hwndStatic,
                      HPS hps)
{
    RECTL       rclStatic;
    LONG        cxIcon = WinQuerySysValue(HWND_DESKTOP, SV_CXICON);
    LONG        lcolBackground = winhQueryPresColor(hwndStatic,
                                                    PP_BACKGROUNDCOLOR,
                                                    TRUE,
                                                    SYSCLR_DIALOGBACKGROUND);
    HPOINTER    hptr = icoQueryIconN(pData->pnbp->inbp.somSelf,
                                     pData->ulAnimationIndex);

    gpihSwitchToRGB(hps);
    WinQueryWindowRect(hwndStatic, &rclStatic);
    WinFillRect(hps,
                &rclStatic,
                lcolBackground);

    WinDrawPointer(hps,
                   (rclStatic.xRight - rclStatic.xLeft - cxIcon) / 2,
                   (rclStatic.yTop - rclStatic.yBottom - cxIcon) / 2,
                   hptr,
                   DP_NORMAL);
}

/*
 *@@ RemoveTargetEmphasis:
 *
 *@@added V0.9.16 (2001-10-15) [umoeller]
 */

static VOID RemoveTargetEmphasis(POBJICONPAGEDATA pData,
                                 HWND hwndStatic)
{
    HPS hps = DrgGetPS(hwndStatic);
    PaintIcon(pData, hwndStatic, hps);
    DrgReleasePS(hps);
}

/*
 *@@ ReportError:
 *
 *@@added V0.9.16 (2001-10-19) [umoeller]
 */

static VOID ReportError(PNOTEBOOKPAGE pnbp,
                        APIRET arc,
                        PCSZ pcszContext)
{
    if (arc)
        cmnDosErrorMsgBox(pnbp->hwndDlgPage,
                          NULL,
                          _wpQueryTitle(pnbp->inbp.somSelf),
                          NULL,
                          arc,
                          pcszContext,
                          MB_CANCEL,
                          TRUE);
}

/*
 *@@ EditIcon:
 *      starts the icon editor with the current icon.
 *
 *@@added V0.9.16 (2001-10-19) [umoeller]
 */

static VOID EditIcon(POBJICONPAGEDATA pData)
{
    APIRET arc;

    PICONINFO pIconInfo = NULL;
    HPOINTER hptrOld = winhSetWaitPointer();

    PCSZ pcszContext = NULL;

    TRY_LOUD(excpt1)
    {
        pcszContext = "Context: loading icon data";
        if (!(arc = icoLoadIconData(pData->pnbp->inbp.somSelf,
                                    pData->ulAnimationIndex,
                                    &pIconInfo)))
        {
            // create a temp file and dump the icon data
            // into it in one flush
            CHAR szTempFile[CCHMAXPATH];
            pcszContext = "Context: creating temp file name";
            if (!(arc = doshCreateTempFileName(szTempFile,
                                               NULL,        // use TEMP
                                               "WP!",       // prefix
                                               "ICO")))     // extension
            {
                PXFILE pFile;
                ULONG cb = pIconInfo->cbIconData;
                pcszContext = "Context: opening temp file for writing";
                if (!cb)
                    arc = ERROR_NO_DATA;
                else if (!(arc = doshOpen(szTempFile,
                                          XOPEN_READWRITE_NEW | XOPEN_BINARY,
                                          &cb,
                                          &pFile)))
                {
                    arc = doshWrite(pFile,
                                    pIconInfo->cbIconData,
                                    pIconInfo->pIconData);
                    doshClose(&pFile);
                }

                if (!arc)
                {
                    // temp icon file created:
                    CHAR szIconEdit[CCHMAXPATH];
                    FILESTATUS3 fs3Old, fs3New;

                    // get the date/time of the file so we
                    // can check whether the file was changed
                    // by iconedit
                    pcszContext = "Context: finding ICONEDIT.EXE";
                    if (    (!(arc = DosQueryPathInfo(szTempFile,
                                                      FIL_STANDARD,
                                                      &fs3Old,
                                                      sizeof(fs3Old))))

                            // find ICONEDIT
                         && (!(arc = doshSearchPath("PATH",
                                                    "ICONEDIT.EXE",
                                                    szIconEdit,
                                                    sizeof(szIconEdit))))
                       )
                    {
                        // open the icon editor with this new icon file
                        HAPP happ;
                        ULONG ulExitCode;
                        pcszContext = "Context: starting ICONEDIT.EXE";
                        if (happ = appQuickStartApp(szIconEdit,
                                                    PROG_DEFAULT,
                                                    szTempFile,
                                                    &ulExitCode))
                        {
                            pcszContext = "Context: setting new icon data";
                            // has the file changed?
                            if (    (!(arc = DosQueryPathInfo(szTempFile,
                                                              FIL_STANDARD,
                                                              &fs3New,
                                                              sizeof(fs3New))))
                                 && (memcmp(&fs3New.ftimeLastWrite,
                                            &fs3Old.ftimeLastWrite,
                                            sizeof(FTIME)))
                               )
                            {
                                // alright, file changed:
                                // set the new icon then
                                ICONINFO NewIcon;
                                NewIcon.cb = sizeof(ICONINFO);
                                NewIcon.fFormat = ICON_FILE;
                                NewIcon.pszFileName = szTempFile;
                                if (!icoSetIconDataN(pData->pnbp->inbp.somSelf,
                                                     pData->ulAnimationIndex,
                                                     &NewIcon))
                                    arc = ERROR_FILE_NOT_FOUND;

                                // repaint icon
                                WinInvalidateRect(pData->hwndIconStatic, NULL, FALSE);
                            }
                        }
                        else
                            arc = ERROR_INVALID_EXE_SIGNATURE;
                    }

                    DosForceDelete(szTempFile);
                }
            }

            if (pIconInfo)
                free(pIconInfo);
        }
    }
    CATCH(excpt1)
    {
        arc = ERROR_PROTECTION_VIOLATION;
    } END_CATCH();

    WinSetPointer(HWND_DESKTOP, hptrOld);

    ReportError(pData->pnbp, arc, pcszContext);
}

/*
 *@@ fnwpSubclassedIconStatic:
 *
 *@@added V0.9.16 (2001-10-15) [umoeller]
 */

static MRESULT EXPENTRY fnwpSubclassedIconStatic(HWND hwndStatic, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    POBJICONPAGEDATA  pData = (POBJICONPAGEDATA)WinQueryWindowPtr(hwndStatic, QWL_USER);
    MRESULT         mrc = FALSE;

    switch(msg)
    {
        case WM_PAINT:
        {
            HPS     hps = WinBeginPaint(hwndStatic, NULLHANDLE, NULL);
            PaintIcon(pData, hwndStatic, hps);
            WinEndPaint(hps);
        }
        break;

        case DM_DRAGOVER:
        {
            USHORT      usDrop = DOR_NEVERDROP,
                        usDefaultOp = DO_LINK;
            ULONG       ulItemNow = 0;

            PDRAGINFO   pdrgInfo = (PDRAGINFO)mp1;

            if (DrgAccessDraginfo(pdrgInfo))
            {
                // invalidate valid drag object
                pData->pobjDragged = NULL;

                if (    (pdrgInfo->usOperation != DO_LINK)
                     && (pdrgInfo->usOperation != DO_DEFAULT)
                   )
                {
                    usDrop = DOR_NODROPOP;      // invalid drop operation, but send msg again
                }
                else
                {
                    // we can handle only one object
                    if (pdrgInfo->cditem == 1)
                    {
                        DRAGITEM    drgItem;
                        if (DrgQueryDragitem(pdrgInfo,
                                             sizeof(drgItem),
                                             &drgItem,
                                             0))        // first item
                        {
                            if (wpshQueryDraggedObject(&drgItem,
                                                       &pData->pobjDragged))
                            {
                                HPS hps = DrgGetPS(hwndStatic);
                                RECTL rclStatic;
                                POINTL ptl;
                                WinQueryWindowRect(hwndStatic, &rclStatic);
                                // draw target emphasis (is stricly rectangular
                                // with OS/2)
                                GpiSetColor(hps,
                                            CLR_BLACK);       // no RGB here
                                ptl.x = rclStatic.xLeft;
                                ptl.y = rclStatic.yBottom;
                                GpiMove(hps,
                                        &ptl);
                                ptl.x = rclStatic.xRight - 1;
                                ptl.y = rclStatic.yTop - 1;
                                GpiBox(hps,
                                       DRO_OUTLINE,
                                       &ptl,
                                       0,
                                       0);
                                DrgReleasePS(hps);

                                usDrop = DOR_DROP;
                            }
                        }
                    }
                }

                DrgFreeDraginfo(pdrgInfo);
            }

            // compose 2SHORT return value
            mrc = (MRFROM2SHORT(usDrop, usDefaultOp));
        }
        break;

        case DM_DRAGLEAVE:
            RemoveTargetEmphasis(pData, hwndStatic);
        break;

        case DM_DROP:
        {
            // dragover was valid above:
            APIRET arc;
            if (arc = icoCopyIconFromObject(pData->pnbp->inbp.somSelf,
                                            pData->pobjDragged,
                                            pData->ulAnimationIndex))
                // do not display a msg box during drop,
                // this nukes PM
                WinPostMsg(hwndStatic,
                           XM_DISPLAYERROR,
                           (MPARAM)arc,
                           0);
            // and repaint
            RemoveTargetEmphasis(pData, hwndStatic);
            // re-enable controls
            pData->pnbp->inbp.pfncbInitPage(pData->pnbp, CBI_ENABLE);
        }
        break;

        case XM_DISPLAYERROR:
            ReportError(pData->pnbp,
                        (APIRET)mp1,
                        NULL);
        break;

        default:
            mrc = pData->pfnwpIconOriginal(hwndStatic, msg, mp1, mp2);
    }

    return mrc;
}

/*
 *@@ icoIcon1InitPage:
 *      "Icon" page notebook callback function (notebook.c).
 *      Sets the controls on the page according to the
 *      object's instance settings.
 *
 *      This is a shared callback for the various types of
 *      icon pages that we replace on the system. See
 *      XFldObject::xwpAddReplacementIconPage for the
 *      situations where this is used.
 *
 *      The smart thing is that we evaluate
 *      INSERTNOTEBOOKPAGE.ulPageID here and format the
 *      page's dialog according to what appears to be
 *      supported by setting OBJICONPAGEDATA.flIconPageFlags
 *      to a bit field with various flags. All control checks
 *      must take that field into account.
 *
 *@@added V0.9.16 (2001-10-15) [umoeller]
 *@@changed V0.9.19 (2002-06-15) [umoeller]: added support for SP_OBJECT_ICONPAGE1_X
 */

VOID XWPENTRY icoIcon1InitPage(PNOTEBOOKPAGE pnbp,
                               ULONG flFlags)
{
    POBJICONPAGEDATA pData = (POBJICONPAGEDATA)pnbp->pUser;
    ULONG flIconPageFlags = 0;
    if (pData)
        flIconPageFlags = pData->flIconPageFlags;

    if (flFlags & CBI_INIT)
    {
        // backup data for "undo"
        if (pData = NEW(OBJICONPAGEDATA))
        {
            ULONG   ulStyle = _wpQueryStyle(pnbp->inbp.somSelf);

            ZERO(pData);

            pData->pnbp = pnbp;

            // store this in notebook page data
            pnbp->pUser = pData;

            // now set up the flIconPageFlags according to
            // the page ID; this is only a first step, we
            // rule out specific flags based on the object
            // style below
            switch (pnbp->inbp.ulPageID)
            {
                case SP_OBJECT_ICONPAGE2:
                    // folder animation icon page 2:
                    // display icon only
                    pData->flIconPageFlags = ICONFL_ICON;
                            // and nothing else!

                    // make all method calls use animation index 1
                    pData->ulAnimationIndex = 1;
                break;

                case SP_TRASHCAN_ICON:
                    // trash can icon page:
                    pData->flIconPageFlags =    ICONFL_TITLE
                                              // | ICONFL_ICON
                                              | ICONFL_HOTKEY
                                              | ICONFL_DETAILS
                                              // | ICONFL_TEMPLATE
                                              | ICONFL_LOCKEDINPLACE;
                break;

                case SP_OBJECT_ICONPAGE1_X:
                    // added V0.9.19 (2002-06-15) [umoeller]:
                    // reduced icon page for classes which have
                    // removed the icon page
                    // (see XFldObject::wpAddSettingsPages):
                    // do not add the titles field because we
                    // shouldn't rename something like \\server\resource.
                    // Unfortunately WPSharedDir does not set
                    // OBJSTYLE_NORENAME, so the below check won't
                    // work.
                    pData->flIconPageFlags =    // ICONFL_TITLE
                                              // | ICONFL_ICON
                                              ICONFL_HOTKEY
                                              | ICONFL_DETAILS
                                              // | ICONFL_TEMPLATE
                                              | ICONFL_LOCKEDINPLACE;
                break;

                default:
                    // SP_OBJECT_ICONPAGE1
                    // standard object page flags:
                    pData->flIconPageFlags =    ICONFL_TITLE
                                              | ICONFL_ICON
                                              | ICONFL_HOTKEY
                                              | ICONFL_DETAILS
                                              | ICONFL_TEMPLATE
                                              | ICONFL_LOCKEDINPLACE;
                break;
            }

            // now rule out invalid flags:

            // disable "lock in place" on Warp 3
            if (!G_fIsWarp4)
                pData->flIconPageFlags &= ~ICONFL_LOCKEDINPLACE;
            else
                // backup old "locked in place" for "undo"
                pData->fLockedInPlaceBackup = !!(ulStyle & OBJSTYLE_LOCKEDINPLACE);

            flIconPageFlags = pData->flIconPageFlags;

            // insert the controls using the dialog formatter
            icoFormatIconPage(pnbp,
                              flIconPageFlags);

            // set up controls some more

            if (flIconPageFlags & ICONFL_TITLE)
            {
                HWND hwndMLE = WinWindowFromID(pnbp->hwndDlgPage, ID_XSDI_ICON_TITLE_EF);

                // backup for undo
                pData->pszTitleBackup = strhdup(_wpQueryTitle(pnbp->inbp.somSelf), NULL);

                WinSendMsg(hwndMLE,
                           MLM_SETTEXTLIMIT,
                           (MPARAM)255,
                           0);

                // if the object must not be renamed, set
                // the MLE read-only
                if (pData->fNoRename = !!(_wpQueryStyle(pnbp->inbp.somSelf)
                                                & OBJSTYLE_NORENAME))
                {
                    _PmpfF(("no rename"));
                    WinSendMsg(hwndMLE,
                               MLM_SETREADONLY,
                               (MPARAM)TRUE,
                               0);
                }
            }

            if (flIconPageFlags & ICONFL_ICON)
            {
                BOOL fUsingDefaultIcon = icoIsUsingDefaultIcon(pnbp->inbp.somSelf,
                                                               pData->ulAnimationIndex);

                // go subclass the icon static control
                pData->hwndIconStatic = WinWindowFromID(pnbp->hwndDlgPage, ID_XSDI_ICON_STATIC);
                winhSetPresColor(pData->hwndIconStatic,
                                 // PP_BACKGROUNDCOLOR
                                 3L,
                                 RGBCOL_WHITE);       // white

                WinSetWindowPtr(pData->hwndIconStatic,
                                QWL_USER,
                                pData);
                pData->pfnwpIconOriginal = WinSubclassWindow(pData->hwndIconStatic,
                                                             fnwpSubclassedIconStatic);

                if (!fUsingDefaultIcon)
                    // not default icon: load a copy for "undo"
                    icoLoadIconData(pnbp->inbp.somSelf,
                                    pData->ulAnimationIndex,
                                    &pData->pIconDataBackup);
            }

            if (flIconPageFlags & ICONFL_HOTKEY)
            {
                pData->hwndHotkeyEF = WinWindowFromID(pnbp->hwndDlgPage,
                                                      ID_XSDI_ICON_HOTKEY_EF);

                if (pData->fHotkeysWorking = hifXWPHookReady())
                {
                    // load function keys
                    pData->paFuncKeys = hifQueryFunctionKeys(&pData->cFuncKeys);

                    // subclass entry field for hotkeys
                    ctlMakeHotkeyEntryField(pData->hwndHotkeyEF);

                    // backup original hotkey
                    if (_xwpQueryObjectHotkey(pnbp->inbp.somSelf,
                                              &pData->HotkeyBackup))
                        pData->fInitiallyHadHotkey = TRUE;
                }
            }

            // disable template checkbox if the class style
            // has NEVERTEMPLATE
            if (    (flIconPageFlags & ICONFL_TEMPLATE)
                 && (_wpclsQueryStyle(_somGetClass(pnbp->inbp.somSelf))
                                    & CLSSTYLE_NEVERTEMPLATE)
               )
            {
                // keep the "Template" checkbox, but disable it
                WinEnableControl(pnbp->hwndDlgPage,
                                 ID_XSDI_ICON_TEMPLATE_CB,
                                 FALSE);
                // unset the flag so the code below won't play
                // with the template setting
                pData->flIconPageFlags &= ~ICONFL_TEMPLATE;
            }
            else
                // backup old template flag for "undo"
                pData->fTemplateBackup = !!(ulStyle & OBJSTYLE_TEMPLATE);
        }
    }

    if (flFlags & CBI_SET)
    {
        ULONG ulStyle = _wpQueryStyle(pnbp->inbp.somSelf);

        _PmpfF(("[%s] setting icon text, NBFL_PAGE_INITED %d",
            _wpQueryTitle(pnbp->inbp.somSelf),
                !!(pnbp->flPage & NBFL_PAGE_INITED)
                ));

        if (flIconPageFlags & ICONFL_TITLE)
            WinSetDlgItemText(pnbp->hwndDlgPage,
                              ID_XSDI_ICON_TITLE_EF,
                              _wpQueryTitle(pnbp->inbp.somSelf));

        // no need to set icon handle, this is subclassed and can
        // do this itself; just have it repaint itself to be sure
        if (flIconPageFlags & ICONFL_ICON)
            WinInvalidateRect(pData->hwndIconStatic, NULL, FALSE);

        if (flIconPageFlags & ICONFL_TEMPLATE)
            winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_XSDI_ICON_TEMPLATE_CB,
                                  (!!(ulStyle & OBJSTYLE_TEMPLATE)));

        if (flIconPageFlags & ICONFL_LOCKEDINPLACE)
            winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_XSDI_ICON_LOCKPOSITION_CB,
                                  (!!(ulStyle & OBJSTYLE_LOCKEDINPLACE)));

        if (flIconPageFlags & ICONFL_HOTKEY)
        {
            USHORT      usFlags,
                        usKeyCode;
            UCHAR       ucScanCode;

            XFldObject_OBJECTHOTKEY Hotkey;

            if (_xwpQueryObjectHotkey(pnbp->inbp.somSelf,
                                      &Hotkey))
            {
                CHAR    szKeyName[200];
                // check if maybe this is a function key
                // V0.9.3 (2000-04-19) [umoeller]
                PFUNCTIONKEY pFuncKey;
                if (pFuncKey = hifFindFunctionKey(pData->paFuncKeys,
                                                  pData->cFuncKeys,
                                                  Hotkey.ucScanCode))
                {
                    // it's a function key:
                    sprintf(szKeyName,
                            "\"%s\"",
                            pFuncKey->szDescription);
                }
                else
                    cmnDescribeKey(szKeyName, Hotkey.usFlags, Hotkey.usKeyCode);

                // set entry field
                WinSetWindowText(pData->hwndHotkeyEF, szKeyName);

                pData->fHasHotkey = TRUE;
            }
            else
            {
                WinSetWindowText(pData->hwndHotkeyEF,
                                 cmnGetString(ID_XSSI_NOTDEFINED));
                            // (cmnQueryNLSStrings())->pszNotDefined);

                pData->fHasHotkey = FALSE;
            }
        }
    }

    if (flFlags & CBI_ENABLE)
    {
        if (flIconPageFlags & ICONFL_ICON)
        {
            // disable "Reset icon" button if
            // the object has a default icon anyway
            WinEnableControl(pnbp->hwndDlgPage,
                             ID_XSDI_ICON_RESET_BUTTON,
                             !icoIsUsingDefaultIcon(pnbp->inbp.somSelf,
                                                    pData->ulAnimationIndex));
        }

        if (flIconPageFlags & ICONFL_HOTKEY)
        {
            // disable entry field if hotkeys are not working
            WinEnableWindow(pData->hwndHotkeyEF,
                            pData->fHotkeysWorking);
            WinEnableControl(pnbp->hwndDlgPage,
                             ID_XSDI_ICON_HOTKEY_TEXT,
                             pData->fHotkeysWorking);
            WinEnableControl(pnbp->hwndDlgPage,
                             ID_XSDI_ICON_HOTKEY_CLEAR,
                                (pData->fHotkeysWorking)
                             && (pData->fHasHotkey));

            WinEnableControl(pnbp->hwndDlgPage,
                             ID_XSDI_ICON_HOTKEY_SET,
                                (pData->fHotkeysWorking)
                             && (pData->fHotkeyPending));
        }
    }

    if (flFlags & CBI_DESTROY)
    {
        if (pData)
        {
            if (pData->pszTitleBackup)
                free(pData->pszTitleBackup);

            if (pData->pIconDataBackup)
                free(pData->pIconDataBackup);

            if (pData->paFuncKeys)
                hifFreeFunctionKeys(pData->paFuncKeys);

            // pData itself is automatically freed
        }
    }
}

/*
 *@@ HandleENHotkey:
 *
 *@@added V0.9.16 (2001-12-08) [umoeller]
 */

static MRESULT HandleENHotkey(POBJICONPAGEDATA pData,
                              ULONG ulExtra)
{
    PNOTEBOOKPAGE   pnbp = pData->pnbp;

    ULONG           flReturn = 0;
    PHOTKEYNOTIFY   phkn = (PHOTKEYNOTIFY)ulExtra;
    BOOL            fStore = FALSE;
    USHORT          usFlags = phkn->usFlags;

    // check if maybe this is a function key
    // V0.9.3 (2000-04-19) [umoeller]
    PFUNCTIONKEY pFuncKey;
    if (pFuncKey = hifFindFunctionKey(pData->paFuncKeys,
                                      pData->cFuncKeys,
                                      phkn->ucScanCode))
    {
        // key code is one of the XWorkplace user-defined
        // function keys:
        // add KC_INVALIDCOMP flag (used by us for
        // user-defined function keys)
        usFlags |= KC_INVALIDCOMP;
        sprintf(phkn->szDescription,
                "\"%s\"",
                pFuncKey->szDescription);
        flReturn = HEFL_SETTEXT;
        fStore = TRUE;
    }
    else
    {
        // no function key:

        // check if this is one of the mnemonics of
        // the "Set" or "Clear" buttons; we better
        // not store those, or the user won't be
        // able to use this page with the keyboard

        if (    (    (phkn->usFlags & KC_VIRTUALKEY)
                  && (    (phkn->usvk == VK_TAB)
                       || (phkn->usvk == VK_BACKTAB)
                     )
                )
             || (    ((usFlags & (KC_CTRL | KC_SHIFT | KC_ALT)) == KC_ALT)
                  && (   (WinSendDlgItemMsg(pnbp->hwndDlgPage,
                                            ID_XSDI_ICON_HOTKEY_SET,
                                            WM_MATCHMNEMONIC,
                                            (MPARAM)phkn->usch,
                                            0))
                      || (WinSendDlgItemMsg(pnbp->hwndDlgPage,
                                            ID_XSDI_ICON_HOTKEY_CLEAR,
                                            WM_MATCHMNEMONIC,
                                            (MPARAM)phkn->usch,
                                            0))
                     )
                )
           )
            // pass those to owner
            flReturn = HEFL_FORWARD2OWNER;
        else
        {
            // have this key combo checked if this thing
            // makes up a valid hotkey just yet:
            // if KC_VIRTUALKEY is down,
            // we must filter out the sole CTRL, ALT, and
            // SHIFT keys, because these are valid only
            // when pressed with some other key
            if (cmnIsValidHotkey(phkn->usFlags,
                                 phkn->usKeyCode))
            {
                // valid hotkey:
                cmnDescribeKey(phkn->szDescription,
                               phkn->usFlags,
                               phkn->usKeyCode);
                flReturn = HEFL_SETTEXT;

                fStore = TRUE;
            }
        }
    }

    if (fStore)
    {
        // store hotkey for object,
        // which can then be set using the "Set" button
        // we'll now pass the scan code, which is
        // used by the hook
        pData->Hotkey.ucScanCode = phkn->ucScanCode;
        pData->Hotkey.usFlags = usFlags;
        pData->Hotkey.usKeyCode = phkn->usKeyCode;

        pData->fHotkeyPending = TRUE;

        WinEnableControl(pnbp->hwndDlgPage,
                         ID_XSDI_ICON_HOTKEY_SET,
                         TRUE);

        // and have entry field display that (comctl.c)
    }

    return ((MPARAM)flReturn);
}

/*
 *@@ icoIcon1ItemChanged:
 *      "Icon" page notebook callback function (notebook.c).
 *      Reacts to changes of any of the dialog controls.
 *
 *@@added V0.9.16 (2001-10-15) [umoeller]
 *@@changed V0.9.16 (2001-12-08) [umoeller]: now disabling hotkeys while entryfield has the focus
 *@@changed V0.9.19 (2002-04-25) [umoeller]: this didn't allow empty titles, fixed
 *@@changed V0.9.19 (2002-05-23) [umoeller]: title was read before page was ready, fixed
 *@@changed V0.9.20 (2002-07-16) [umoeller]: fixed excessive rename when page was inited
 */

MRESULT XWPENTRY icoIcon1ItemChanged(PNOTEBOOKPAGE pnbp,
                                     ULONG ulItemID, USHORT usNotifyCode,
                                     ULONG ulExtra)
{
    MRESULT mrc = (MRESULT)0;

    POBJICONPAGEDATA pData;

    ULONG   flIconPageFlags;

    ULONG   ulStyleFlags = 0,       // style flags to be affected
            ulStyleMask = 0;        // style bits to be set or cleared;
                                    // to set a bit, set it in both flags and mask
                                    // to clear a bit set it in flags only
    BOOL    fRefresh = TRUE;

    APIRET  arc;

    if (    (pData = (POBJICONPAGEDATA)pnbp->pUser)
         && (flIconPageFlags = pData->flIconPageFlags)
       )
    {
        switch (ulItemID)
        {
            case ID_XSDI_ICON_TITLE_EF:
                fRefresh = FALSE;

                if (    (usNotifyCode == MLN_KILLFOCUS)
                     // title controls available?
                     && (flIconPageFlags & ICONFL_TITLE)
                     // object must not have OBJSTYLE_NORENAME:
                     && (!pData->fNoRename)
                     // page already initialized? V0.9.19 (2002-05-23) [umoeller]
                     && (pnbp->flPage & NBFL_PAGE_INITED)
                   )
                {
                    PSZ  pszNewTitle = winhQueryWindowText(pnbp->hwndControl);

                    // only do ANYTHING if the title actually changed,
                    // we get MLN_KILLFOCUS from the initial WinSetWindowText
                    // too V0.9.20 (2002-07-12) [umoeller]
                    if (strhcmp(_wpQueryTitle(pnbp->inbp.somSelf),
                                pszNewTitle))
                    {
                        BOOL rc;

                        _PmpfF(("[%s] MLN_KILLFOCUS new: \"%s\"",
                             _wpQueryTitle(pnbp->inbp.somSelf),
                             STRINGORNULL(pszNewTitle)));

                        if (pszNewTitle)
                        {
                            rc = _wpSetTitle(pnbp->inbp.somSelf, pszNewTitle);
                        }
                        else
                            // empty titles are valid, but we can't pass in
                            // NULL strings, so use an empty string
                            // V0.9.19 (2002-04-25) [umoeller]
                            rc = _wpSetTitle(pnbp->inbp.somSelf, "");

                        if (!rc)
                            // failed: restore old
                            cmnMessageBoxExt(pnbp->hwndDlgPage,
                                             104,   // error
                                             NULL, 0,
                                             187,   // old name restored
                                             MB_OK);
                    }

                    if (pszNewTitle)
                        free(pszNewTitle);

                    fRefresh = TRUE;
                }
            break;

            // no need for static icon control here,
            // this is subclassed and handles all the drag'n'drop

            case ID_XSDI_ICON_EDIT_BUTTON:
                EditIcon(pData);
            break;

            case DID_BROWSE:
            {
                CHAR szFilemask[CCHMAXPATH] = "*.ico";
                if (cmnFileDlg(pnbp->hwndDlgPage,
                               szFilemask,
                               WINH_FOD_INILOADDIR | WINH_FOD_INISAVEDIR,
                               HINI_USER,
                               INIAPP_XWORKPLACE,
                               INIKEY_ICONPAGE_LASTDIR))
                {
                    WPFileSystem *pfs;
                    if (!(pfs = _wpclsQueryObjectFromPath(_WPFileSystem,
                                                          szFilemask)))
                        arc = ERROR_FILE_NOT_FOUND;
                    else
                    {
                        arc = icoCopyIconFromObject(pnbp->inbp.somSelf,
                                                    pfs,
                                                    pData->ulAnimationIndex);
                        WinInvalidateRect(pData->hwndIconStatic, NULL, FALSE);
                    }

                    ReportError(pnbp, arc, NULL);
                }
            }
            break;

            case ID_XSDI_ICON_RESET_BUTTON:
                if (pData->flIconPageFlags & ICONFL_ICON)
                    objResetIcon(pnbp->inbp.somSelf, pData->ulAnimationIndex);
            break;

            case ID_XSDI_ICON_TEMPLATE_CB:
                if (flIconPageFlags & ICONFL_TEMPLATE)
                {
                    ulStyleFlags |= OBJSTYLE_TEMPLATE;
                    if (ulExtra)
                        ulStyleMask |= OBJSTYLE_TEMPLATE;
                }
            break;

            case ID_XSDI_ICON_LOCKPOSITION_CB:
                if (flIconPageFlags & ICONFL_LOCKEDINPLACE)
                {
                    ulStyleFlags |= OBJSTYLE_LOCKEDINPLACE;
                    if (ulExtra)
                        ulStyleMask |= OBJSTYLE_LOCKEDINPLACE;
                }
            break;

            /*
             * ID_XSDI_ICON_HOTKEY_EF:
             *      subclassed "hotkey" entry field;
             *      this sends EN_HOTKEY when a new
             *      hotkey has been entered
             */

            case ID_XSDI_ICON_HOTKEY_EF:
                switch (usNotifyCode)
                {
                    case EN_SETFOCUS:
                    case EN_KILLFOCUS:
                        // if we're getting the focus, disable
                        // hotkeys; if we're losing the focus,
                        // enable hotkeys again (otherwise the
                        // user can't set any hotkeys that are
                        // already occupied)
                        // V0.9.16 (2001-12-06) [umoeller]
                        krnPostDaemonMsg(XDM_DISABLEHOTKEYSTEMP,
                                         (MPARAM)(usNotifyCode == EN_SETFOCUS),
                                             // TRUE: disable (on set focus)
                                             // FALSE: re-enable (on kill focus)
                                         0);
                    break;

                    /*
                     * EN_HOTKEY:
                     *      new hotkey has been entered:
                     */

                    case EN_HOTKEY:
                        mrc = HandleENHotkey(pData,
                                             ulExtra);
                    break;
                }

                fRefresh = FALSE;
                        // or we'll hang

            break;

            /*
             * ID_XSDI_ICON_HOTKEY_SET:
             *      "Set hotkey" button
             */

            case ID_XSDI_ICON_HOTKEY_SET:
                // set hotkey
                _xwpSetObjectHotkey(pnbp->inbp.somSelf,
                                    &pData->Hotkey);
                pData->fHotkeyPending = FALSE;
                pData->fHasHotkey = TRUE;
            break;

            /*
             * ID_XSDI_ICON_HOTKEY_CLEAR:
             *      "Clear hotkey" button
             */

            case ID_XSDI_ICON_HOTKEY_CLEAR:
                // remove hotkey
                _xwpSetObjectHotkey(pnbp->inbp.somSelf,
                                    NULL);   // remove
                WinSetWindowText(pData->hwndHotkeyEF,
                                 cmnGetString(ID_XSSI_NOTDEFINED)); // (cmnQueryNLSStrings())->pszNotDefined);
                pData->fHotkeyPending = FALSE;
                pData->fHasHotkey = FALSE;
            break;

            /*
             * DID_DETAILS:
             *      display object details.
             */

            case DID_DETAILS:
                objShowObjectDetails(pnbp->inbp.hwndNotebook,
                                     pnbp->inbp.somSelf);
            break;

            /*
             * DID_UNDO:
             *
             */

            case DID_UNDO:
                if (    (flIconPageFlags & ICONFL_TITLE)
                     && (pData->pszTitleBackup)
                     && (!pData->fNoRename)
                   )
                    // set backed-up title
                    _wpSetTitle(pnbp->inbp.somSelf, pData->pszTitleBackup);

                if (flIconPageFlags & ICONFL_ICON)
                {
                    // restore icon backup
                    if (pData->pIconDataBackup)
                        // was using non-default icon:
                        icoSetIconDataN(pnbp->inbp.somSelf,
                                        pData->ulAnimationIndex,
                                        pData->pIconDataBackup);
                    else
                        // was using default icon:
                        objResetIcon(pnbp->inbp.somSelf, pData->ulAnimationIndex);
                }

                if (flIconPageFlags & ICONFL_TEMPLATE)
                {
                    ulStyleFlags |= OBJSTYLE_TEMPLATE;
                    if (pData->fTemplateBackup)
                        ulStyleMask |= OBJSTYLE_TEMPLATE;
                    // else: bit is still zero
                }

                if (flIconPageFlags & ICONFL_LOCKEDINPLACE)
                {
                    ulStyleFlags |= OBJSTYLE_LOCKEDINPLACE;
                    if (pData->fLockedInPlaceBackup)
                        ulStyleMask |= OBJSTYLE_LOCKEDINPLACE;
                    // else: bit is still zero
                }

                if (flIconPageFlags & ICONFL_HOTKEY)
                {
                    if (pData->fInitiallyHadHotkey)
                    {
                        _xwpSetObjectHotkey(pnbp->inbp.somSelf,
                                            &pData->HotkeyBackup);
                        pData->fHasHotkey = TRUE;
                    }
                    else
                    {
                        // no hotkey: delete
                        _xwpSetObjectHotkey(pnbp->inbp.somSelf,
                                            NULL);
                        pData->fHasHotkey = FALSE;
                    }
                    pData->fHotkeyPending = FALSE;
                }
            break;

            /*
             * DID_DEFAULT:
             *
             */

            case DID_DEFAULT:
                if (    (flIconPageFlags & ICONFL_TITLE)
                     && (!pData->fNoRename)
                   )
                    // set class default title
                    _wpSetTitle(pnbp->inbp.somSelf,
                                _wpclsQueryTitle(_somGetClass(pnbp->inbp.somSelf)));

                if (flIconPageFlags & ICONFL_ICON)
                    // reset standard icon
                    objResetIcon(pnbp->inbp.somSelf, pData->ulAnimationIndex);

                if (flIconPageFlags & ICONFL_TEMPLATE)
                    // clear template bit
                    ulStyleFlags |= OBJSTYLE_TEMPLATE;

                if (flIconPageFlags & ICONFL_LOCKEDINPLACE)
                    // clear locked-in-place bit
                    ulStyleFlags |= OBJSTYLE_LOCKEDINPLACE;

                if (flIconPageFlags & ICONFL_HOTKEY)
                {
                    // delete hotkey
                    _xwpSetObjectHotkey(pnbp->inbp.somSelf,
                                        NULL);
                    pData->fHasHotkey = FALSE;
                    pData->fHotkeyPending = FALSE;
                }
            break;

            default:
                fRefresh = FALSE;
        }
    }

    if (ulStyleFlags)
    {
        // object style flags are to be changed:
        _wpModifyStyle(pnbp->inbp.somSelf,
                       ulStyleFlags,        // affected flags
                       ulStyleMask);        // bits to be set or cleared
        fRefresh = TRUE;
    }

    if (fRefresh)
    {
        // update the display by calling the INIT callback
        pnbp->inbp.pfncbInitPage(pnbp, CBI_SET | CBI_ENABLE);
        // and save the object (to be on the safe side)
        _wpSaveDeferred(pnbp->inbp.somSelf);
    }

    return mrc;
}


