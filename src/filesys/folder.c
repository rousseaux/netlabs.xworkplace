
/*
 *@@sourcefile folder.c:
 *      implementation file for the XFolder class. The code
 *      in here gets called from the various XFolder methods
 *      overrides in classes\xfldr.c.
 *
 *      This file is ALL new with V0.9.0.
 *
 *      Function prefix for this file:
 *      --  fdr*
 *
 *@@added V0.9.0 [umoeller]
 *@@header "filesys\folder.h"
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

#define INCL_DOSEXCEPTIONS
#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#define INCL_DOSERRORS
#define INCL_WINWINDOWMGR
#define INCL_WINMESSAGEMGR
#define INCL_WINFRAMEMGR
#define INCL_WININPUT
#define INCL_WINRECTANGLES
#define INCL_WINSYS             // needed for presparams
#define INCL_WINMENUS
#define INCL_WINTIMER
#define INCL_WINDIALOGS
#define INCL_WINSTATICS
#define INCL_WINBUTTONS
#define INCL_WINENTRYFIELDS
#define INCL_WINLISTBOXES
#define INCL_WINSTDCNR
#define INCL_WINSHELLDATA       // Prf* functions
#define INCL_GPILOGCOLORTABLE
#define INCL_GPIPRIMITIVES
#include <os2.h>

// C library headers
#include <stdio.h>              // needed for except.h
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\cnrh.h"               // container helper routines
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\eah.h"                // extended attributes helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\gpih.h"               // GPI helper routines
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\stringh.h"            // string helper routines
#include "helpers\syssound.h"           // system sound helper routines
#include "helpers\winh.h"               // PM helper routines
#include "helpers\wphandle.h"           // Henk Kelder's HOBJECT handling

// SOM headers which don't crash with prec. header files
#include "xfldr.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\cnrsort.h"             // container sort comparison functions
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

#include "filesys\fileops.h"            // file operations implementation
#include "filesys\folder.h"             // XFolder implementation
#include "filesys\menus.h"              // shared context menu logic
#include "filesys\statbars.h"           // status bar translation logic
#include "filesys\xthreads.h"           // extra XWorkplace threads

// other SOM headers
#pragma hdrstop                     // VAC++ keeps crashing otherwise

#include <wpdesk.h>
#include <wpshadow.h>           // WPShadow
#include "xfdisk.h"
#include <wprootf.h>

#include "helpers\undoc.h"              // some undocumented stuff

/* ******************************************************************
 *                                                                  *
 *   Global variables                                               *
 *                                                                  *
 ********************************************************************/

// root of linked list to remember original window
// procedures when subclassing frame windows for folder hotkeys
LINKLIST            llSubclassed;                   // changed V0.9.0
// mutex semaphore for access to this list
HMTX                hmtxSubclassed = NULLHANDLE;

// flags for fdr_fnwpSubclassedFolderFrame;
// these are set by fdr_fnwpSubclFolderContentMenu.
// We can afford using global variables here
// because opening menus is a modal operation.
BOOL                fFldrContentMenuMoved = FALSE,
                    fFldrContentMenuButtonDown = FALSE;

// mutex semaphores for folder lists (favorite folders, quick-open)
HMTX                hmtxFolderLists = NULLHANDLE;

/* ******************************************************************
 *                                                                  *
 *   Query setup strings                                            *
 *                                                                  *
 ********************************************************************/

/*
 *@@ fdrQuerySetup:
 *      implementation of XFolder::xwpQuerySetup2.
 *      See remarks there.
 *
 *      This returns the length of the XFolder
 *      setup string part only.
 *
 *@@added V0.9.1 (2000-01-17) [umoeller]
 */

ULONG fdrQuerySetup(WPObject *somSelf,
                    PSZ pszSetupString,
                    ULONG cbSetupString)
{
    // flag defined in
    #define WP_GLOBAL_COLOR         0x40000000

    XFolderData *somThis = XFolderGetData(somSelf);

    // temporary buffer for building the setup string
    PSZ     pszTemp = NULL,
            pszView = NULL;
    ULONG   ulReturn = 0;
    ULONG   ulValue = 0;
    PSZ     pszValue = 0,
            pszDefaultValue = 0;
    SOMClass *pClassObject = 0;
    BOOL    fTreeIconsInvisible = FALSE,
            fIconViewColumns = FALSE;

    CHAR    szTemp[1000] = "";

    BOOL    fIsWarp4 = doshIsWarp4();

    /* if (pszSetupString)
    {
        _Pmpf(("Dumping %s", _wpQueryTitle(somSelf)));

        _Pmpf(("-- FldrLongArray (%d bytes)", _cbFldrLongArray));
        cmnDumpMemoryBlock((PVOID)_pFldrLongArray,
                           _cbFldrLongArray,
                           8);

        _Pmpf(("-- pszFdrCnrBackground"));
        if (_pszFdrBkgndImageFile)
            _Pmpf(("    %s", _pszFdrBkgndImageFile));

        _Pmpf(("-- pFldrBackground (%d bytes)", _cbFldrBackground));
        cmnDumpMemoryBlock((PVOID)_pFldrBackground,
                           _cbFldrBackground,
                           8);
    } */

    // WORKAREA
    if (_wpQueryFldrFlags(somSelf) & FOI_WORKAREA)
        strhxcat(&pszTemp, "WORKAREA=YES;");

    // MENUBAR
    ulValue = _xwpQueryMenuBarVisibility(somSelf);
    if (ulValue != _xwpclsQueryMenuBarVisibility(_XFolder))
        // non-default value:
        if (ulValue)
            strhxcat(&pszTemp, "MENUBAR=YES;");
        else
            strhxcat(&pszTemp, "MENUBAR=NO;");

    // SORTBYATTR

    /*
     * folder sort settings
     *
     */

    // ALWAYSSORT
    // DEFAULTSORT

    // SORTCLASS
    pClassObject = _wpQueryFldrSortClass(somSelf);
    if (pClassObject != _WPFileSystem)
    {
        sprintf(szTemp, "SORTCLASS=%s;", _somGetName(pClassObject));
        strhxcat(&pszTemp, szTemp);
    }

    /*
     * folder view settings
     *
     */

    // BACKGROUND
    if ((_pszFdrBkgndImageFile) && (_pFldrBackground))
    {
        CHAR cType = 'S';

        PBYTE pbRGB = ( (PBYTE)(&(_pFldrBackground->rgbColor)) );

        switch (_pFldrBackground->bImageType & 0x07) // ?2=Normal, ?3=tiled, ?4=scaled
        {
            case 2: cType = 'N'; break;
            case 3: cType = 'T'; break;
            default: /* 4 */ cType = 'S'; break;
        }

        sprintf(szTemp, "BACKGROUND=%s,%c,%d,%c,%d %d %d;",
                _pszFdrBkgndImageFile,  // image name
                cType,                    // N = normal, T = tiled, S = scaled
                _pFldrBackground->bScaleFactor,  // scaling factor
                (_pFldrBackground->bColorOnly == 0x28) // 0x28 Image, 0x27 Color only
                    ? 'I'
                    : 'C', // I = image, C = color only
                *(pbRGB + 2), *(pbRGB + 1), *pbRGB);  // RGB color; apparently optional
        strhxcat(&pszTemp, szTemp);
    }

    // DEFAULTVIEW: already handled by XFldObject

    /*
     * Icon view
     *
     */

    // ICONFONT
    pszValue = _wpQueryFldrFont(somSelf, OPEN_CONTENTS);
    pszDefaultValue = prfhQueryProfileData(HINI_USER,
                                           "PM_SystemFonts",
                                           "IconText", NULL);
    if (pszDefaultValue)
    {
        if (strcmp(pszValue, pszDefaultValue) != 0)
        {
            sprintf(szTemp, "ICONFONT=%s;", pszValue);
            strhxcat(&pszTemp, szTemp);
        }
        free(pszDefaultValue);
    }

    // ICONNFILE: cannot be retrieved!
    // ICONFILE: cannot be retrieved!
    // ICONNRESOURCE: cannot be retrieved!

    // ICONVIEWPOS

    // ICONGRIDSIZE

    // ICONTEXTVISIBLE

    // ICONVIEW
    ulValue = _wpQueryFldrAttr(somSelf, OPEN_CONTENTS);
    switch (ulValue & (CV_NAME | CV_FLOW | CV_ICON | CV_TEXT))
    {
        case (CV_NAME | CV_FLOW): // but not CV_ICON or CV_TEXT
            strhxcat(&pszView, "FLOWED");
            fIconViewColumns = TRUE;        // needed for colors below
        break;

        case (CV_NAME): // but not CV_ICON | CV_FLOW or CV_TEXT
            strhxcat(&pszView, "NONFLOWED");
            fIconViewColumns = TRUE;        // needed for colors below
        break;

        case (CV_TEXT): // but not CV_ICON
            strhxcat(&pszView, "INVISIBLE");
        break;
    }

    if (ulValue & CV_MINI)
        // ICONVIEW=MINI
        if (pszView)
            strhxcat(&pszView, ",MINI");
        else
            strhxcat(&pszView, "MINI");

    if (pszView)
    {
        sprintf(szTemp, "ICONVIEW=%s;", pszView);
        strhxcat(&pszTemp, szTemp);
        free(pszView);
        pszView = NULL;
    }

    // ICONTEXTBACKGROUNDCOLOR

    // ICONTEXTCOLOR
    if (_pFldrLongArray)
    {
        BYTE bUseDefault = FALSE;
        PBYTE pbArrayField = ( (PBYTE)(&(_pFldrLongArray->rgbIconViewTextColAsPlaced)) );
        if (fIconViewColumns)
            // FLOWED or NONFLOWED: use different field then
            pbArrayField = ( (PBYTE)(&(_pFldrLongArray->rgbIconViewTextColColumns)) );

        bUseDefault = *(pbArrayField + 3);
        if (!bUseDefault)
        {
            BYTE bRed   = *(pbArrayField + 2);
            BYTE bGreen = *(pbArrayField + 1);
            BYTE bBlue  = *(pbArrayField );

            sprintf(szTemp, "ICONTEXTCOLOR=%d %d %d;", bRed, bGreen, bBlue);
            strhxcat(&pszTemp, szTemp);
        }
    }

    // ICONSHADOWCOLOR
    if (_pFldrLongArray)
        // only Warp 4 has these fields, so check size of array
        if (_cbFldrLongArray >= 84)
        {
            PBYTE pbArrayField = ( (PBYTE)(&(_pFldrLongArray->rgbIconViewShadowCol)) );

            BYTE bUseDefault = *(pbArrayField + 3);
            if (!bUseDefault)
            {
                BYTE bRed   = *(pbArrayField + 2);
                BYTE bGreen = *(pbArrayField + 1);
                BYTE bBlue  = *(pbArrayField );

                sprintf(szTemp, "TREESHADOWCOLOR=%d %d %d;", bRed, bGreen, bBlue);
                strhxcat(&pszTemp, szTemp);
            }
        }

    /*
     * Tree view
     *
     */

    // TREEFONT
    pszValue = _wpQueryFldrFont(somSelf, OPEN_TREE);
    pszDefaultValue = prfhQueryProfileData(HINI_USER,
                                           "PM_SystemFonts",
                                           "IconText", NULL);
    if (pszDefaultValue)
    {
        if (strcmp(pszValue, pszDefaultValue) != 0)
        {
            sprintf(szTemp, "TREEFONT=%s;", pszValue);
            strhxcat(&pszTemp, szTemp);
        }
        free(pszDefaultValue);
    }

    // TREETEXTVISIBLE
    // if this is NO, the WPS displays no tree text in tree icon view;
    // setting this does not affect the CV_* flags, so apparently this
    // is done via record owner-draw. There must be some flag in the
    // instance data for this.

    // TREEVIEW
    ulValue = _wpQueryFldrAttr(somSelf, OPEN_TREE);
    switch (ulValue & (CV_TREE | CV_NAME | CV_ICON | CV_TEXT))
    {
        // CV_TREE | CV_TEXT means text only, no icons (INVISIBLE)
        // CV_TREE | CV_ICON means icons and text and +/- buttons (default)
        // CV_TREE | CV_NAME is apparently not used by the WPS

        case (CV_TREE | CV_TEXT):
            strhxcat(&pszView, "INVISIBLE");
            fTreeIconsInvisible = TRUE;         // needed for tree text colors below
        break;
    }

    if (fIsWarp4)
    {
        // on Warp 4, mini icons in Tree view are the default
        if ((ulValue & CV_MINI) == 0)
            // TREEVIEW=MINI
            if (pszView)
                strhxcat(&pszView, ",NORMAL");
            else
                strhxcat(&pszView, "NORMAL");
    }
    else
        // Warp 3:
        if ((ulValue & CV_MINI) != 0)
            // TREEVIEW=MINI
            if (pszView)
                strhxcat(&pszView, ",MINI");
            else
                strhxcat(&pszView, "MINI");

    if ((ulValue & CA_TREELINE) == 0)
        // TREEVIEW=NOLINES
        if (pszView)
            strhxcat(&pszView, ",NOLINES");
        else
            strhxcat(&pszView, "NOLINES");

    if (pszView)
    {
        sprintf(szTemp, "TREEVIEW=%s;", pszView);
        strhxcat(&pszTemp, szTemp);
        free(pszView);
        pszView = NULL;
    }

    // TREETEXTCOLOR
    if (_pFldrLongArray)
    {
        BYTE bUseDefault = FALSE;
        PBYTE pbArrayField = ( (PBYTE)(&(_pFldrLongArray->rgbTreeViewTextColIcons)) );

        if (fTreeIconsInvisible)
            pbArrayField = ( (PBYTE)(&(_pFldrLongArray->rgbTreeViewTextColTextOnly)) );

        bUseDefault = *(pbArrayField + 3);
        if (!bUseDefault)
        {
            BYTE bRed   = *(pbArrayField + 2);
            BYTE bGreen = *(pbArrayField + 1);
            BYTE bBlue  = *(pbArrayField );

            sprintf(szTemp, "TREETEXTCOLOR=%d %d %d;", bRed, bGreen, bBlue);
            strhxcat(&pszTemp, szTemp);
        }
    }

    // TREESHADOWCOLOR
    if (_pFldrLongArray)
        // only Warp 4 has these fields, so check size of array
        if (_cbFldrLongArray >= 84)
        {
            PBYTE pbArrayField = ( (PBYTE)(&(_pFldrLongArray->rgbTreeViewShadowCol)) );

            BYTE bUseDefault = *(pbArrayField + 3);
            if (!bUseDefault)
            {
                BYTE bRed   = *(pbArrayField + 2);
                BYTE bGreen = *(pbArrayField + 1);
                BYTE bBlue  = *(pbArrayField );

                sprintf(szTemp, "TREESHADOWCOLOR=%d %d %d;", bRed, bGreen, bBlue);
                strhxcat(&pszTemp, szTemp);
            }
        }

    // SHOWALLINTREEVIEW
    if (_pulShowAllInTreeView) // only != NULL on Warp 4
        if (*_pulShowAllInTreeView)
            strhxcat(&pszTemp, "SHOWALLINTREEVIEW=YES;");

    /*
     * Details view
     *
     */

    // DETAILSCLASS
    pClassObject = _wpQueryFldrDetailsClass(somSelf);
    if (pClassObject != _WPFileSystem)
    {
        sprintf(szTemp, "DETAILSCLASS=%s;", _somGetName(pClassObject));
        strhxcat(&pszTemp, szTemp);
    }

    // DETAILSFONT
    pszValue = _wpQueryFldrFont(somSelf, OPEN_DETAILS);
    pszDefaultValue = prfhQueryProfileData(HINI_USER,
                                           "PM_SystemFonts",
                                           "IconText", NULL);
    if (pszDefaultValue)
    {
        if (strcmp(pszValue, pszDefaultValue) != 0)
        {
            sprintf(szTemp, "DETAILSFONT=%s;", pszValue);
            strhxcat(&pszTemp, szTemp);
        }
        free(pszDefaultValue);
    }

    // DETAILSTODISPLAY

    // DETAILSVIEW

    // DETAILSTEXTCOLOR
    if (_pFldrLongArray)
    {
        BYTE bUseDefault = FALSE;
        PBYTE pbArrayField = ( (PBYTE)(&(_pFldrLongArray->rgbDetlViewTextCol)) );

        bUseDefault = *(pbArrayField + 3);
        if (!bUseDefault)
        {
            BYTE bRed   = *(pbArrayField + 2);
            BYTE bGreen = *(pbArrayField + 1);
            BYTE bBlue  = *(pbArrayField );

            sprintf(szTemp, "DETAILSTEXTCOLOR=%d %d %d;", bRed, bGreen, bBlue);
            strhxcat(&pszTemp, szTemp);
        }
    }

    // DETAILSSHADOWCOLOR
    if (_pFldrLongArray)
        // only Warp 4 has these fields, so check size of array
        if (_cbFldrLongArray >= 84)
        {
            PBYTE pbArrayField = ( (PBYTE)(&(_pFldrLongArray->rgbDetlViewShadowCol)) );

            BYTE bUseDefault = *(pbArrayField + 3);
            if (!bUseDefault)
            {
                BYTE bRed   = *(pbArrayField + 2);
                BYTE bGreen = *(pbArrayField + 1);
                BYTE bBlue  = *(pbArrayField );

                sprintf(szTemp, "DETAILSSHADOWCOLOR=%d %d %d;", bRed, bGreen, bBlue);
                strhxcat(&pszTemp, szTemp);
            }
        }

    /*
     * additional XFolder setup strings
     *
     */

    switch (_bFolderHotkeysInstance)
    {
        case 0:
            strhxcat(&pszTemp, "ACCELERATORS=NO;");
        break;

        case 1:
            strhxcat(&pszTemp, "ACCELERATORS=YES;");
        break;

        // 2 means default
    }

    switch (_bSnapToGridInstance)
    {
        case 0:
            strhxcat(&pszTemp, "SNAPTOGRID=NO;");
        break;

        case 1:
            strhxcat(&pszTemp, "SNAPTOGRID=YES;");
        break;

        // 2 means default
    }

    switch (_bFullPathInstance)
    {
        case 0:
            strhxcat(&pszTemp, "FULLPATH=NO;");
        break;

        case 1:
            strhxcat(&pszTemp, "FULLPATH=YES;");
        break;

        // 2 means default
    }

    switch (_bStatusBarInstance)
    {
        case 0:
            strhxcat(&pszTemp, "STATUSBAR=NO;");
        break;

        case 1:
            strhxcat(&pszTemp, "STATUSBAR=YES;");
        break;

        // 2 means default
    }

    if (_xwpIsFavoriteFolder(somSelf))
        strhxcat(&pszTemp, "FAVORITEFOLDER=YES;");

    /*
     * append string
     *
     */

    if (pszTemp)
    {
        // return string if buffer is given
        if ((pszSetupString) && (cbSetupString))
            strhncpy0(pszSetupString,   // target
                      pszTemp,          // source
                      cbSetupString);   // buffer size

        // always return length of string
        ulReturn = strlen(pszTemp);
        free(pszTemp);
    }

    return (ulReturn);
}

/* ******************************************************************
 *                                                                  *
 *   Folder view helpers                                            *
 *                                                                  *
 ********************************************************************/

/*
 *@@ fdrForEachOpenViewInstance:
 *      this instance method goes through all open views of a folder and calls
 *      pfnwpCallback for each them (as opposed to fdrForEachOpenGlobalView,
 *      which goes through all open folders on the system).
 *
 *      The following params are then passed to pfnwpCallback:
 *      --   HWND       hwnd: the hwnd of the view frame window;
 *      --   ULONG      mp1:  the view type (as def'd in wpOpen)
 *      --   XFolder*   mp2:  somSelf.
 *
 *      This method does not return until all views have been processed.
 *      You might want to call this method in a different thread if the task
 *      will take long.
 *
 *      This method returns TRUE if the callback returned TRUE at least once.
 *      Note on disk objects/root folders: the WPS does not maintain an open
 *      view list for root folders, but only for the corresponding disk object.
 *      xwpForEachOpenView will call open disk views also, but the callback
 *      will still be passed the root folder in pFolder!#
 *
 *@@changed V0.9.1 (2000-02-04) [umoeller]: this used to be XFolder::xwpForEachOpenView
 */

BOOL fdrForEachOpenInstanceView(WPFolder *somSelf,
                                ULONG ulMsg,
                                PFNWP pfnwpCallback)
{
    BOOL brc = FALSE;
    WPObject *somSelf2;
    // XFolderData *somThis = XFolderGetData(somSelf);
    // XFolderMethodDebug("XFolder","xf_xwpForEachOpenView");

    if (_somIsA(somSelf, _WPRootFolder))
        // for disk/root folder views: root folders have no
        // open view, instead the disk object is registered
        // to have the open view. Duh. So we need to find
        // the disk object first
        somSelf2 = _xwpclsQueryDiskObject(_XFldDisk, somSelf);
    else
        somSelf2 = somSelf;

    if (somSelf2)
    {
        if (_wpFindUseItem(somSelf2, USAGE_OPENVIEW, NULL))
        {   // folder has an open view;
            // now we go search the open views of the folder and get the
            // frame handle of the desired view (ulView) */
            PVIEWITEM   pViewItem;
            for (pViewItem = _wpFindViewItem(somSelf2, VIEW_ANY, NULL);
                 pViewItem;
                 pViewItem = _wpFindViewItem(somSelf2, VIEW_ANY, pViewItem))
            {
                if ((*pfnwpCallback)(pViewItem->handle,
                                     ulMsg,
                                     (MPARAM)pViewItem->view,
                                     // but even if we have found a disk object
                                     // above, we need to pass it the root folder
                                     // pointer, because otherwise the callback
                                     // might get into trouble
                                     (MPARAM)somSelf)
                            == (MPARAM)TRUE)
                    brc = TRUE;
            } // end for
        } // end if
    }
    return (brc);
}

/*
 *@@ fdrForEachOpenGlobalView:
 *      this class method goes through all open folder windows and calls
 *      pfnwpCallback for each open view of each open folder.
 *      As opposed to fdrForEachOpenInstanceView, this goes thru really
 *      all open folders views on the system.
 *
 *      The following params will be passed to pfnwpCallback:
 *      -- HWND       hwnd: the hwnd of the view frame window;
 *      -- ULONG      msg:  ulMsg, as passed to this method
 *      -- ULONG      mp1:  the view type (as def'd in wpOpen)
 *      -- XFolder*   mp2:  the currently open folder.
 *
 *      This method does not return until all views have been processed.
 *      You might want to call this method in a different thread if the task
 *      will take long.
 *
 *@@changed V0.9.1 (2000-02-04) [umoeller]: this used to be M_XFolder::xwpclsForEachOpenView
 */

BOOL fdrForEachOpenGlobalView(ULONG ulMsg,
                              PFNWP pfnwpCallback)
{
    M_WPFolder  *pWPFolderClass = _WPFolder;
    XFolder     *pFolder;
    // M_XFolderData *somThis = M_XFolderGetData(somSelf);
    // M_XFolderMethodDebug("M_XFolder","xfM_xwpclsForEachOpenView");

    for ( pFolder = _wpclsQueryOpenFolders(pWPFolderClass, NULL, QC_FIRST, FALSE);
          pFolder;
          pFolder = _wpclsQueryOpenFolders(pWPFolderClass, pFolder, QC_NEXT, FALSE))
    {
        if (_somIsA(pFolder, pWPFolderClass))
            fdrForEachOpenInstanceView(pFolder, ulMsg, pfnwpCallback);
    }
    return TRUE;
}

/* ******************************************************************
 *                                                                  *
 *   Full path in title                                             *
 *                                                                  *
 ********************************************************************/

/*
 *@@ fdrSetOneFrameWndTitle:
 *      this changes the window title of a given folder frame window
 *      to the full path of the folder, according to the global and/or
 *      folder settings.
 *
 *@@changed V0.9.0 [umoeller]: moved this func here from xfldr.c
 */

BOOL fdrSetOneFrameWndTitle(WPFolder *somSelf,
                            HWND hwndFrame)
{
    PSZ                 pFirstSlash = 0,
                        pSrchSlash = 0,
                        pNextSlash = 0;
    CHAR                szTemp[CCHMAXPATH] = "";
    XFolderData         *somThis = XFolderGetData(somSelf);
    PCGLOBALSETTINGS     pGlobalSettings = cmnQueryGlobalSettings();
    BOOL                brc = FALSE;

    if (    (_bFullPathInstance == 1)
         || ((_bFullPathInstance == 2) && (pGlobalSettings->FullPath))
       )
    {
        // settings allow full path in title for this folder:

        // get real name (= full path), if allowed)
        _wpQueryFilename(somSelf, szTemp, TRUE);

        // now truncate path if it's longer than allowed by user
        pFirstSlash = strchr(szTemp, '\\');
        if ((pFirstSlash) && (pGlobalSettings->MaxPathChars > 10))
        {
            pSrchSlash = pFirstSlash+3;
            while (strlen(szTemp) > pGlobalSettings->MaxPathChars)
            {
                pNextSlash = strchr(pSrchSlash, '\\');
                if (pNextSlash)
                {
                    strcpy(pFirstSlash+4, pNextSlash);
                    pFirstSlash[1] = '.';
                    pFirstSlash[2] = '.';
                    pFirstSlash[3] = '.';
                    pSrchSlash = pFirstSlash+5;
                }
                else break;
            }
        }

        // now either append the full path in brackets to or replace window title
        if (pGlobalSettings->KeepTitle)
        {
            CHAR szFullPathTitle[CCHMAXPATH*2] = "";
            sprintf(szFullPathTitle, "%s (%s)",
                    _wpQueryTitle(somSelf),
                    szTemp);
            WinSetWindowText(hwndFrame, szFullPathTitle);
        }
        else
            WinSetWindowText(hwndFrame, szTemp);

        brc = TRUE;
    }
    else
    {
        // settings DON'T allow full path in title for this folder:
        // set to title only
        WinSetWindowText(hwndFrame, _wpQueryTitle(somSelf));
        brc = FALSE;
    }
    return (brc);
}

/*
 *@@ fdrUpdateAllFrameWndTitles:
 *      this function sets the frame wnd titles for all currently
 *      open views of a given folder to the folder's full path.
 *
 *      This gets called on the Worker thread after XFolder's
 *      replacements of wpMoveObject, wpSetTitle, or wpRefresh have
 *      been called.
 *
 *      This calls fdrSetOneFrameWndTitle in turn.
 *
 *@@changed V0.9.0 [umoeller]: moved this func here from xfldr.c
 */

BOOL fdrUpdateAllFrameWndTitles(WPFolder *somSelf)
{
    HWND        hwndFrame = NULLHANDLE;
    BOOL        brc = FALSE;

    if (hwndFrame = wpshQueryFrameFromView(somSelf, OPEN_CONTENTS))
    {
        fdrSetOneFrameWndTitle(somSelf, hwndFrame);
        brc = TRUE;
    }
    if (hwndFrame = wpshQueryFrameFromView(somSelf, OPEN_DETAILS))
    {
        fdrSetOneFrameWndTitle(somSelf, hwndFrame);
        brc = TRUE;
    }
    if (hwndFrame = wpshQueryFrameFromView(somSelf, OPEN_TREE))
    {
        fdrSetOneFrameWndTitle(somSelf, hwndFrame);
        brc = TRUE;
    }

    ntbUpdateVisiblePage(somSelf, SP_XFOLDER_FLDR);

    return (brc);
}

/* ******************************************************************
 *                                                                  *
 *   Snap To Grid                                                   *
 *                                                                  *
 ********************************************************************/

/*
 *@@ fdrSnapToGrid:
 *           makes all objects in the folder "snap" on a grid whose
 *           coordinates are to be defined in the GLOBALSETTINGS.
 *
 *           This function checks if an Icon view of the folder is
 *           currently open; if not and fNotify == TRUE, it displays
 *           a message box.
 *
 *@@changed V0.9.0 [umoeller]: this used to be an instance method (xfldr.c)
 */

BOOL fdrSnapToGrid(WPFolder *somSelf,
                   BOOL fNotify)
{
    // WPObject            *obj;
    HWND                hwndFrame = 0,
                        hwndCnr = 0;
    PMINIRECORDCORE     pmrc = 0;
    LONG                lNewX = 0,
                        lNewY = 0;
    BOOL                brc = FALSE;
    // if Shift is pressed, move all the objects, otherwise
    // only the selected ones
    BOOL                fShiftPressed = doshQueryShiftState();
    // BOOL                fMoveThisObject = FALSE;

    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    // XFolderData *somThis = XFolderGetData(somSelf);
    // XFolderMethodDebug("XFolder","xf_xfSnapToGrid");

    // first we need the frame handle of a currently open icon view;
    // all others don't make sense */
    hwndFrame = wpshQueryFrameFromView(somSelf, OPEN_CONTENTS);

    if (hwndFrame)
    {

        // now get the container handle
        hwndCnr = wpshQueryCnrFromFrame(hwndFrame);

        if (hwndCnr)
        {
            // now begin iteration over the folder's objects; we don't
            // use the WPS method (wpQueryContent) because this is too
            // slow. Instead, we query the container directly.

            pmrc = NULL;
            do
            {
                if (fShiftPressed)
                    // shift pressed: move all objects, so loop
                    // thru the whole container content
                    pmrc =
                        (PMINIRECORDCORE)WinSendMsg(hwndCnr,
                                CM_QUERYRECORD,
                                (MPARAM)pmrc,           // NULL at first loop
                                MPFROM2SHORT(
                                    (pmrc)
                                        ? CMA_NEXT      // not first loop: get next object
                                        : CMA_FIRST,    // first loop: get first objecct
                                    CMA_ITEMORDER)
                                );
                else
                    // shift _not_ pressed: move selected objects
                    // only, so loop thru these objects
                    pmrc =
                        (PMINIRECORDCORE)WinSendMsg(hwndCnr,
                                CM_QUERYRECORDEMPHASIS,
                                (pmrc)                  // NULL at first loop
                                    ? (MPARAM)pmrc
                                    : (MPARAM)CMA_FIRST, // flag for getting first selected
                                (MPARAM)(CRA_SELECTED)
                                );
                if (pmrc)
                {
                    // record found:
                    // the WPS shares records among views, so we need
                    // to update the record core info first
                    WinSendMsg(hwndCnr,
                                CM_QUERYRECORDINFO,
                                (MPARAM)&pmrc,
                                (MPARAM)1);         // one record only
                    // un-display the new object at the old (default) location
                    WinSendMsg(hwndCnr,
                                CM_ERASERECORD,
                                    // this only changes the visibility of the
                                    // record without changing the recordcore;
                                    // this msg is intended for drag'n'drop and such
                                (MPARAM)pmrc,
                                NULL);

                    // now play with the objects coordinates
                    lNewX =
                        ( (
                            ( (pmrc->ptlIcon.x - pGlobalSettings->GridX)
                              + (pGlobalSettings->GridCX / 2)
                            )
                        / pGlobalSettings->GridCX ) * pGlobalSettings->GridCX )
                        + pGlobalSettings->GridX;
                    lNewY =
                        ( (
                            ( (pmrc->ptlIcon.y - pGlobalSettings->GridY)
                              + (pGlobalSettings->GridCY / 2)
                            )
                        / pGlobalSettings->GridCY ) * pGlobalSettings->GridCY )
                        + pGlobalSettings->GridY;

                    // update the record core
                    if ( (lNewX) && (lNewX != pmrc->ptlIcon.x) ) {
                        pmrc->ptlIcon.x = lNewX;         // X
                    }
                    if ( (lNewY) && (lNewY != pmrc->ptlIcon.y) ) {
                        pmrc->ptlIcon.y = lNewY;         // Y
                    }

                    // repaint at new position
                    WinSendMsg(hwndCnr,
                                CM_INVALIDATERECORD,
                                (MPARAM)&pmrc,
                                MPFROM2SHORT(1,     // one record only
                                    CMA_REPOSITION | CMA_ERASE));
                }
            } while (pmrc);

            brc = TRUE; // "OK" flag
        } // end if (hwndCnr)
    } // end if (hwndFrame)
    else
    {
        // no open icon view: complain
        if (fNotify)
        {
            cmnSetHelpPanel(-1);                   // disable F1
            WinDlgBox(HWND_DESKTOP,
                         HWND_DESKTOP,             // owner is desktop
                         (PFNWP)fnwpDlgGeneric,    // common.c
                         cmnQueryNLSModuleHandle(FALSE),               // load from resource file
                         ID_XFD_NOICONVIEW,        // dialog resource id
                         (PVOID)NULL);
        }
    }
    return (brc);
}

/* ******************************************************************
 *                                                                  *
 *   Extended Folder Sort                                           *
 *                                                                  *
 ********************************************************************/

/*
 *@@ fdrQuerySortFunc:
 *      this returns the sort comparison function for
 *      the specified sort criterion. See XFolder::xwpSortViewOnce
 *      for details.
 *
 *@@changed V0.9.0 [umoeller]: moved this func here from xfldr.c
 */

PFN fdrQuerySortFunc(USHORT usSort)
{
    USHORT usSort2 = usSort;

    if (usSort == SET_DEFAULT)
    {
        PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
        usSort2 =  pGlobalSettings->DefaultSort;
    }

    switch (usSort2)
    {
        case SV_TYPE:           return ((PFN)fnCompareType);
        case SV_CLASS:          return ((PFN)fnCompareClass);
        case SV_REALNAME:       return ((PFN)fnCompareRealName);
        case SV_SIZE:           return ((PFN)fnCompareSize);
        case SV_LASTWRITEDATE:  return ((PFN)fnCompareLastWriteDate);
        case SV_LASTACCESSDATE: return ((PFN)fnCompareLastAccessDate);
        case SV_CREATIONDATE:   return ((PFN)fnCompareCreationDate);
        case SV_EXT:            return ((PFN)fnCompareExt);
        case SV_FOLDERSFIRST:   return ((PFN)fnCompareFoldersFirst);
    };

    // default:
    return ((PFN)fnCompareName);
}

/*
 * fdrSortAllViews:
 *      callback function for sorting all folder views.
 *      This is called by xf(cls)ForEachOpenView, which also passes
 *      the parameters to this func.
 *
 *@@changed V0.9.0 [umoeller]: moved this func here from xfldr.c
 */

MRESULT EXPENTRY fdrSortAllViews(HWND hwndView,    // open folder view frame hwnd
                                 ULONG ulSort,     // sort flag
                                 MPARAM mpView,    // OPEN_xxx flag
                                 MPARAM mpFolder)  // XFolder*
{
    XFolder     *somSelf = (XFolder*)mpFolder;
    MRESULT     mrc = (MPARAM)FALSE;

    if (   ((ULONG)mpView == OPEN_CONTENTS)
        || ((ULONG)mpView == OPEN_TREE)
        || ((ULONG)mpView == OPEN_DETAILS)
       )
    {
        _xwpSortViewOnce(somSelf, hwndView, ulSort);
        mrc = (MPARAM)TRUE;
    }
    return (mrc);
}

/*
 *@@ fdrSetFldrCnrSort:
 *      this is the most central function of XFolder's
 *      extended sorting capabilities. This is called
 *      every time the container in an open folder view
 *      needs to have its sort settings updated. In other
 *      words, this function evaluates the current folder
 *      sort settings and finds out the corresponding
 *      container sort comparison functions and other
 *      settings.
 *
 *      Parameters:
 *      --  HWND hwndCnr     cnr of open view of somSelf
 *      --  BOOL fForce      TRUE: always update the cnr
 *                           settings, even if they have
 *                           not changed
 *
 *      This function gets called:
 *      1)  from our wpOpen override to set folder sort
 *          when a new folder view opens;
 *      2)  from our wpSetFldrSort override (see notes
 *          there);
 *      3)  after folder sort settings have been changed
 *          using xwpSetFldrSort.
 *
 *      It is usually not necessary to call this method
 *      directly. To sort folders, you should call
 *      XFolder::xwpSetFldrSort or XFolder::xwpSortViewOnce
 *      instead.
 *
 *      Note that XFolder does not use the WPS's sort
 *      mechanisms at all. Instead, we set our own
 *      container sort comparison functions directly
 *      _always_. Those functions are all in cnrsort.c.
 *
 *      See the XWorkplace Programming Guide for an
 *      overview of how XFolder sorting works.
 *
 *      This func is prototyped in common.h because
 *      XFldDisk needs access to it also (V0.9.0).
 *
 *@@changed V0.9.0 [umoeller]: this used to be an instance method
 *@@changed V0.9.0 [umoeller]: moved this func here from xfldr.c
 */

VOID fdrSetFldrCnrSort(WPFolder *somSelf,      // in: folder to sort
                       HWND hwndCnr,           // in: container of open view of somSelf
                       BOOL fForce)            // in: always invalidate container?
{
    XFolderData *somThis = XFolderGetData(somSelf);

    if (hwndCnr)
    {
        PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
        // XFolderData     *somThis = XFolderGetData(somSelf);

        // use our macro for determining this folder's always-sort flag;
        // this is TRUE if "Always sort" is on either locally or globally
        BOOL            AlwaysSort = ALWAYS_SORT;

        // get our sort comparison func
        PFN             pfnSort =  (AlwaysSort)
                                       ? fdrQuerySortFunc(DEFAULT_SORT)
                                       : NULL
                                   ;

        CNRINFO         CnrInfo = {0};
        BOOL            Update = FALSE;

        cnrhQueryCnrInfo(hwndCnr, &CnrInfo);

        #ifdef DEBUG_SORT
            _Pmpf(( "_xfSetCnrSort: %s", _wpQueryTitle(somSelf) ));
            _Pmpf(( "  _Always: %d, Global->Always: %d", _AlwaysSort, pGlobalSettings->AlwaysSort ));
            _Pmpf(( "  _Default: %d, Global->Default: %d", _DefaultSort, pGlobalSettings->DefaultSort ));
        #endif

        // for icon views, we need extra precautions
        if ((CnrInfo.flWindowAttr & (CV_ICON | CV_TREE)) == CV_ICON)
        {
            // for some reason, cnr icon views need to have "auto arrange" on
            // when sorting, or the cnr will allow to drag'n'drop icons freely
            // within the same cnr, which is not useful when auto-sort is on

            ULONG       ulStyle = WinQueryWindowULong(hwndCnr, QWL_STYLE);

            if (AlwaysSort)
            {
                // always sort: we need to set CCS_AUTOPOSITION, if not set
                if ((ulStyle & CCS_AUTOPOSITION) == 0)
                {
                    #ifdef DEBUG_SORT
                        _Pmpf(( "  New ulStyle = %lX", ulStyle | CCS_AUTOPOSITION ));
                    #endif
                    WinSetWindowULong(hwndCnr, QWL_STYLE, (ulStyle | CCS_AUTOPOSITION));
                    Update = TRUE;
                }
            }
            else
            {
                // NO always sort: we need to unset CCS_AUTOPOSITION, if set
                if ((ulStyle & CCS_AUTOPOSITION) != 0)
                {
                    #ifdef DEBUG_SORT
                        _Pmpf(( "  New ulStyle = %lX", ulStyle & (~CCS_AUTOPOSITION) ));
                    #endif
                    WinSetWindowULong(hwndCnr, QWL_STYLE, (ulStyle & (~CCS_AUTOPOSITION)));
                    Update = TRUE;
                }
            }
        }

        // now also update the internal WPFolder sort info, because otherwise
        // the WPS will keep reverting the cnr attrs; we have obtained the pointer
        // to this structure in wpRestoreData
        if (_pFldrSortInfo)
            _pFldrSortInfo->fAlwaysSort = AlwaysSort;

        // finally, set the cnr sort function: we perform these checks
        // to avoid cnr flickering
        if (    // sort function changed?
                (CnrInfo.pSortRecord != (PVOID)pfnSort)
                // CCS_AUTOPOSITION flag changed above?
             || (Update)
             || (fForce)
           )
        {
            #ifdef DEBUG_SORT
                _Pmpf(( "  Resetting pSortRecord to %lX", CnrInfo.pSortRecord ));
            #endif

            // set the cnr sort function; if this is != NULL, the
            // container will always sort the records. If auto-sort
            // is off, pfnSort has been set to NULL above.
            CnrInfo.pSortRecord = (PVOID)pfnSort;

            // now update the CnrInfo, which will repaint the cnr also
            WinSendMsg(hwndCnr,
                       CM_SETCNRINFO,
                       (MPARAM)&CnrInfo,
                       (MPARAM)CMA_PSORTRECORD);
        }
    }
}

/*
 * fdrUpdateFolderSorts:
 *      callback function for updating all folder sorts.
 *      This is called by xf(cls)ForEachOpenView, which also passes
 *      the parameters to this func.
 *
 *@@changed V0.9.0 [umoeller]: moved this func here from xfldr.c
 */

MRESULT EXPENTRY fdrUpdateFolderSorts(HWND hwndView,   // frame wnd handle
                                      ULONG ulDummy,
                                      MPARAM mpView,   // OPEN_xxx flag for this view
                                      MPARAM mpFolder) // somSelf
{
    XFolder     *somSelf = (XFolder*)mpFolder;
    MRESULT     mrc = (MPARAM)FALSE;

    #ifdef DEBUG_SORT
        _Pmpf(( "fdrUpdateFolderSorts: %s", _wpQueryTitle(somSelf) ));
    #endif

    if (   ((ULONG)mpView == OPEN_CONTENTS)
        || ((ULONG)mpView == OPEN_TREE)
        || ((ULONG)mpView == OPEN_DETAILS)
       )
    {
        fdrSetFldrCnrSort(somSelf, wpshQueryCnrFromFrame(hwndView), FALSE);
        mrc = (MPARAM)TRUE;
    }
    return (mrc);
}

/* ******************************************************************
 *                                                                  *
 *   Status bars                                                    *
 *                                                                  *
 ********************************************************************/

/*
 * fncbUpdateStatusBars:
 *      callback func for WOM_UPDATEALLSTATUSBARS in
 *      XFolder Worker thread (xfdesk.c); should not
 *      be called directly (it must reside in this file
 *      to be able to access folder instance data).
 *      This is called by xf(cls)ForEachOpenView, which also passes
 *      the parameters to this func.
 *
 *@@changed V0.9.0 [umoeller]: moved this func here from xfldr.c
 */

MRESULT EXPENTRY fncbUpdateStatusBars(HWND hwndView,        // folder frame
                                      ULONG ulActivate,
                                              // 1: show/hide status bars according to
                                              //    folder/Global settings
                                              // 2: reformat status bars (e.g. because
                                              //    fonts have changed)
                                      MPARAM mpView,                      // OPEN_xxx flag
                                      MPARAM mpFolder)                    // folder object
{
    MRESULT             mrc = (MPARAM)FALSE;

    #ifdef DEBUG_STATUSBARS
        _Pmpf(("fncbUpdateStatusBars ulActivate = %d", ulActivate));
    #endif

    if (mpFolder != _wpclsQueryActiveDesktop(_WPDesktop))
    {
        PSUBCLASSEDLISTITEM psli = fdrQueryPSLI(hwndView, NULL);
        if (psli)
        {
            if (ulActivate == 2) // "update" flag
                WinPostMsg(psli->hwndSupplObject,
                           SOM_ACTIVATESTATUSBAR,
                           (MPARAM)2,
                           (MPARAM)hwndView);
            else
            {
                // show/hide flag:
                PCGLOBALSETTINGS     pGlobalSettings = cmnQueryGlobalSettings();
                XFolderData *somThis = XFolderGetData(mpFolder);
                BOOL fVisible = (
                                    // status bar feature enabled?
                                    (pGlobalSettings->fEnableStatusBars)
                                &&
                                    // status bars either enabled for this instance
                                    // or in global settings?
                                    (    (_bStatusBarInstance == STATUSBAR_ON)
                                      || (    (_bStatusBarInstance == STATUSBAR_DEFAULT)
                                           && (pGlobalSettings->fDefaultStatusBarVisibility)
                                         )
                                    )
                                &&
                                    // status bars enabled for current view type?
                                    (   (   ((ULONG)mpView == OPEN_CONTENTS)
                                         && (pGlobalSettings->SBForViews & SBV_ICON)
                                        )
                                     || (   ((ULONG)mpView == OPEN_TREE)
                                         && (pGlobalSettings->SBForViews & SBV_TREE)
                                        )
                                     || (   ((ULONG)mpView == OPEN_DETAILS)
                                         && (pGlobalSettings->SBForViews & SBV_DETAILS)
                                        )
                                    )
                                );

                if ( fVisible != (psli->hwndStatusBar != NULLHANDLE) )
                {
                    // visibility changed:
                    // we now post a message to the folder frame's
                    // supplementary object window;
                    // because we cannot mess with the windows on
                    // the Worker thread, we need to have the
                    // status bar created/destroyed/changed on the
                    // folder's thread
                    WinPostMsg(psli->hwndSupplObject,
                            SOM_ACTIVATESTATUSBAR,
                            (MPARAM)( (fVisible) ? 1 : 0 ),      // show or hide
                            (MPARAM)hwndView);
                    mrc = (MPARAM)TRUE;
                }
            }
        }
    }

    return (mrc);
}

/* ******************************************************************
 *                                                                  *
 *   Folder frame window subclassing                                *
 *                                                                  *
 ********************************************************************/

/*
 *@@ fdrManipulateNewView:
 *      this gets called from XFolder::wpOpen
 *      when a new Icon, Tree, or Details view
 *      has been successfully opened.
 *
 *      This manipulates the view according to
 *      our needs (subclassing, sorting, full
 *      status bar, path in title etc.).
 *
 *      This is ONLY used for folders, not for
 *      WPDisk's. This calls fdrSubclassFolderFrame
 *      in turn.
 *
 *@@added V0.9.0 (99-11-27) [umoeller]
 *@@changed V0.9.1 (2000-02-08) [umoeller]: status bars were added even if globally disabled; fixed.
 */

VOID fdrManipulateNewView(WPFolder *somSelf,        // in: folder with new view
                          HWND hwndNewFrame,        // in: new view (frame) of folder
                          ULONG ulView)             // in: OPEN_CONTENTS, OPEN_TREE, or OPEN_DETAILS
{
    PSUBCLASSEDLISTITEM psli = 0;
    PCGLOBALSETTINGS    pGlobalSettings = cmnQueryGlobalSettings();
    XFolderData         *somThis = XFolderGetData(somSelf);

    // subclass the new folder frame window
    psli = fdrSubclassFolderFrame(hwndNewFrame, somSelf, somSelf, ulView);

    // change the window title to full path, if allowed
    if (    (_bFullPathInstance == 1)
         || ((_bFullPathInstance == 2) && (pGlobalSettings->FullPath))
       )
        fdrSetOneFrameWndTitle(somSelf, hwndNewFrame);

    // add status bar, if allowed:
    // 1) status bar only if allowed for the current folder
    if (   (pGlobalSettings->fEnableStatusBars)
        && (    (_bStatusBarInstance == STATUSBAR_ON)
             || (   (_bStatusBarInstance == STATUSBAR_DEFAULT)
                 && (pGlobalSettings->fDefaultStatusBarVisibility)
                )
           )
       )
        // 2) no status bar for active Desktop
        if (somSelf != _wpclsQueryActiveDesktop(_WPDesktop))
            // 3) check that subclassed list item is valid
            if (psli)
                // 4) status bar only if allowed for the current view type
                if (    (   (ulView == OPEN_CONTENTS)
                         && (pGlobalSettings->SBForViews & SBV_ICON)
                        )
                     || (   (ulView == OPEN_TREE)
                         && (pGlobalSettings->SBForViews & SBV_TREE)
                        )
                     || (   (ulView == OPEN_DETAILS)
                         && (pGlobalSettings->SBForViews & SBV_DETAILS)
                        )
                    )
                    fdrCreateStatusBar(somSelf, psli, TRUE);

    // replace sort stuff
    if (pGlobalSettings->ExtFolderSort)
    {
        HWND hwndCnr = wpshQueryCnrFromFrame(hwndNewFrame);
        if (hwndCnr)
            fdrSetFldrCnrSort(somSelf, hwndCnr, FALSE);
    }
}

/*
 *@@ fdrSLIOnKill:
 *      thread "exit list" func registered with
 *      the TRY_xxx macros (helpers\except.c).
 *      In case the calling thread gets killed
 *      during execution of one of the SLI functions,
 *      this function gets called. As opposed to
 *      real exit list functions, which always get
 *      called on thread 1, this gets called on
 *      the thread which registered the exception
 *      handler.
 *
 *      We release mutex semaphores here, which we
 *      must do, because otherwise the system might
 *      hang if another thread is waiting on the
 *      same semaphore.
 *
 *@@added V0.9.0 [umoeller]
 */

VOID APIENTRY fdrSLIOnKill(VOID)
{
    DosReleaseMutexSem(hmtxSubclassed);
}

/*
 *@@ fdrInitPSLI:
 *      this is called once from M_XFolder::wpclsInitData
 *      to initialize the SUBCLASSEDLISTITEM list.
 *
 *@@changed V0.9.0 [umoeller]: adjusted for new linklist functions
 *@@changed V0.9.0 [umoeller]: moved this func here from common.c
 */

VOID fdrInitPSLI(VOID)
{
    lstInit(&llSubclassed, TRUE);
    if (DosCreateMutexSem(NULL,
                          &hmtxSubclassed, 0, FALSE) != NO_ERROR)
    {
        DosBeep(100, 300);
        hmtxSubclassed = -1;
    }
}

/*
 *@@ fdrQueryPSLI:
 *      this retrieves the PSUBCLASSEDLISTITEM from the
 *      global linked list of subclassed frame windows,
 *      according to a given frame wnd handle. One of these
 *      structs is maintained for each open folder view
 *      to store window data which is needed everywhere.
 *
 *      Returns NULL if not found.
 *
 *      If pulIndex != NULL, the index of the item is stored
 *      in *pulIndex (counting from 0), if an item is returned.
 *
 *@@changed V0.9.0 [umoeller]: adjusted for new linklist functions
 *@@changed V0.9.0 [umoeller]: pulIndex added to function prototype
 *@@changed V0.9.0 [umoeller]: moved this func here from common.c
 *@@changed V0.9.1 (2000-02-14) [umoeller]: reversed order of functions; now subclassing is last
 */

PSUBCLASSEDLISTITEM fdrQueryPSLI(HWND hwndFrame,        // in: folder frame to find
                                 PULONG pulIndex)       // out: index in linked list if found
{
    PLISTNODE           pNode = 0;
    PSUBCLASSEDLISTITEM psliThis = 0,
                        psliFound = 0;
    BOOL                fSemOwned = FALSE;
    ULONG               ulIndex = 0;

    TRY_QUIET(excpt1, fdrSLIOnKill)
    {
        if (hwndFrame)
        {
            fSemOwned = (WinRequestMutexSem(hmtxSubclassed, 4000) == NO_ERROR);
            if (fSemOwned)
            {
                pNode = lstQueryFirstNode(&llSubclassed);
                while (pNode)
                {
                    psliThis = pNode->pItemData;
                    if (psliThis->hwndFrame == hwndFrame)
                    {
                        // item found:
                        psliFound = psliThis;
                        if (pulIndex)
                            *pulIndex = ulIndex;
                        break; // while
                    }

                    pNode = pNode->pNext;
                    ulIndex++;
                }

                DosReleaseMutexSem(hmtxSubclassed);
                fSemOwned = FALSE;
            }
        }
    }
    CATCH(excpt1)
    {
        if (fSemOwned)
        {
            DosReleaseMutexSem(hmtxSubclassed);
            fSemOwned = FALSE;
        }
    } END_CATCH();

    return (psliFound);
}

/*
 *@@ fdrSubclassFolderFrame:
 *      this routine is used by both XFolder::wpOpen and
 *      XFldDisk::wpOpen to subclass a new frame window
 *      to allow for extra XFolder features.
 *
 *      For this, this routines maintains a global linked
 *      list of subclassed windows (SUBCLASSEDLISTITEM
 *      structures, common.h). The list is stored in the global
 *      llSubclassed variable. This is protected internally
 *      by a mutex semaphore; you should therefore always
 *      use fdrQueryPSLI to access that list.
 *
 *      Every list item contains (among other things) the
 *      original wnd proc of the subclassed window, which the
 *      subclassed wnd proc (fdr_fnwpSubclassedFolderFrame, xfldr.c)
 *      needs to call the the original folder wnd proc for a
 *      given frame window, because these procs might differ
 *      depending on the class or view type or installed WPS
 *      enhancers.
 *
 *      These list items are also used for various other frame
 *      data, such as the handles of the status bar and
 *      supplementary object windows.
 *      We cannot store this data elsewhere, because QWL_USER
 *      in the wnd data seems to be used by the WPS already.
 *
 *      Note: when called from XFolder::wpOpen, somSelf and
 *      pRealObject both point to the folder. However, when
 *      called from XFldDisk::wpOpen, somSelf has the
 *      disk object's root folder, while pRealObject has the
 *      disk object. Both are stored in the SUBCLASSEDLISTITEM
 *      structure.
 *
 *      This func returns the newly created SUBCLASSEDLISTITEM.
 *
 *@@changed V0.9.0 [umoeller]: adjusted for new linklist functions
 *@@changed V0.9.0 [umoeller]: now initializing SUBCLASSEDLISTITEM properly
 *@@changed V0.9.0 [umoeller]: moved this func here from common.c
 */

PSUBCLASSEDLISTITEM fdrSubclassFolderFrame(HWND hwndFrame,
                                                // in: frame wnd of new view (returned by wpOpen)
                                           WPFolder *somSelf,
                                                // in: folder; either XFolder's somSelf
                                                // or XFldDisk's root folder
                                           WPObject *pRealObject,
                                                // in: the "real" object; for XFolder, this is == somSelf,
                                                // for XFldDisk, this is the disk object (needed for object handles)
                                           ULONG ulView)
                                                // OPEN_CONTENTS et al.
{
    BOOL                fSemOwned = FALSE;
    PFNWP               pfnwpCurrent;
    HWND                hwndCnr;
    PSUBCLASSEDLISTITEM psliNew = NULL;
    PCGLOBALSETTINGS     pGlobalSettings = cmnQueryGlobalSettings();

    TRY_QUIET(excpt1, fdrSLIOnKill)
    {
        // subclass only if the user hasn't disabled
        // subclassing globally
        if (pGlobalSettings->NoSubclassing == 0)
        {
            // exclude other views, such as settings notebooks and
            // possible user views
            if (    (hwndFrame)
                 && (   (ulView == OPEN_CONTENTS)
                     || (ulView == OPEN_TREE)
                     || (ulView == OPEN_DETAILS)
                    )
               )
            {
                // get container wnd handle
                hwndCnr = wpshQueryCnrFromFrame(hwndFrame);

                // only subclass frame window if it contains a container;
                // just a security check
                if (hwndCnr)
                {
                    // now check if frame wnd has already been subclassed;
                    // just another security check
                    pfnwpCurrent = (PFNWP)WinQueryWindowPtr(hwndFrame, QWP_PFNWP);
                    if (pfnwpCurrent != (PFNWP)fdr_fnwpSubclassedFolderFrame)
                    {
                        psliNew = malloc(sizeof(SUBCLASSEDLISTITEM));
                        if (psliNew)
                        {
                            memset(psliNew, 0, sizeof(SUBCLASSEDLISTITEM)); // V0.9.0

                            // append to global list
                            fSemOwned = (WinRequestMutexSem(hmtxSubclassed, 4000) == NO_ERROR);
                            if (fSemOwned)
                            {
                                lstAppendItem(&llSubclassed,
                                              psliNew);
                                DosReleaseMutexSem(hmtxSubclassed);
                                fSemOwned = FALSE;
                            }

                            // store various other data here
                            psliNew->hwndFrame = hwndFrame;
                            psliNew->somSelf = somSelf;
                            psliNew->pRealObject = pRealObject;
                            psliNew->hwndCnr = hwndCnr;
                            psliNew->ulView = ulView;
                            psliNew->fRemoveSrcEmphasis = FALSE;
                            // set status bar hwnd to zero at this point;
                            // this will be created elsewhere
                            psliNew->hwndStatusBar = NULLHANDLE;
                            // create a supplementary object window
                            // for this folder frame (see
                            // fdr_fnwpSupplFolderObject for details)
                            psliNew->hwndSupplObject = WinCreateWindow(
                                           HWND_OBJECT,
                                           WNDCLASS_SUPPLOBJECT, // class name
                                           (PSZ)"SupplObject",     // title
                                           0,           // style
                                           0,0,0,0,     // position
                                           0,           // owner
                                           HWND_BOTTOM, // z-order
                                           0,           // window id
                                           psliNew,    // pass the struct to WM_CREATE
                                           NULL);       // pres params

                            // finally, subclass folder frame
                            psliNew->pfnwpOriginal
                                = WinSubclassWindow(hwndFrame,
                                                    fdr_fnwpSubclassedFolderFrame);
                        }
                    }
                }
            }
        }
    }
    CATCH(excpt1)
    {
        if (fSemOwned)
        {
            DosReleaseMutexSem(hmtxSubclassed);
            fSemOwned = FALSE;
        }
    } END_CATCH();
    return (psliNew);
}

/*
 *@@ fdrRemovePSLI:
 *      reverse to fdrSubclassFolderFrame, this removes
 *      a PSUBCLASSEDLISTITEM from the global list again.
 *      Called upon WM_CLOSE in folder frames.
 *
 *@@changed V0.9.0 [umoeller]: adjusted for new linklist functions
 *@@changed V0.9.0 [umoeller]: moved this func here from common.c
 */

VOID fdrRemovePSLI(PSUBCLASSEDLISTITEM psli)
{
    BOOL fSemOwned = FALSE;

    TRY_QUIET(excpt1, fdrSLIOnKill)
    {
        fSemOwned = (WinRequestMutexSem(hmtxSubclassed, 4000) == NO_ERROR);
        if (fSemOwned)
        {
            lstRemoveItem(&llSubclassed,
                          psli);
            DosReleaseMutexSem(hmtxSubclassed);
            fSemOwned = FALSE;
        }
    }
    CATCH(excpt1)
    {
        if (fSemOwned)
        {
            DosReleaseMutexSem(hmtxSubclassed);
            fSemOwned = FALSE;
        }
    } END_CATCH();
}

/*
 * CalcFrameRect:
 *      this gets called from fdr_fnwpSubclassedFolderFrame
 *      when WM_CALCFRAMERECT is received. This implements
 *      folder status bars.
 *
 *@@changed V0.9.0 [umoeller]: moved this func here from xfldr.c
 */

VOID CalcFrameRect(MPARAM mp1, MPARAM mp2)
{
    PRECTL prclPassed = (PRECTL)mp1;
    ULONG ulStatusBarHeight = cmnQueryStatusBarHeight();

    if (SHORT1FROMMP(mp2))
        //     TRUE:  Frame rectangle provided, calculate client
        //     FALSE: Client area rectangle provided, calculate frame
    {
        //  TRUE: calculate the rectl of the client;
        //  call default window procedure to subtract child frame
        //  controls from the rectangle's height
        LONG lClientHeight;

        //  position the static text frame extension below the client
        lClientHeight = prclPassed->yTop - prclPassed->yBottom;
        if ( ulStatusBarHeight  > lClientHeight  )
        {
            // extension is taller than client, so set client height to 0
            prclPassed->yTop = prclPassed->yBottom;
        }
        else
        {
            //  set the origin of the client and shrink it based upon the
            //  static text control's height
            prclPassed->yBottom += ulStatusBarHeight;
            prclPassed->yTop -= ulStatusBarHeight;
        }
    }
    else
    {
        //  FALSE: calculate the rectl of the frame;
        //  call default window procedure to subtract child frame
        //  controls from the rectangle's height;
        //  set the origin of the frame and increase it based upon the
        //  static text control's height
        prclPassed->yBottom -= ulStatusBarHeight;
        prclPassed->yTop += ulStatusBarHeight;
    }
}

/*
 * FormatFrame:
 *      this gets called from fdr_fnwpSubclassedFolderFrame
 *      when WM_FORMATFRAME is received. This implements
 *      folder status bars.
 *
 *@@changed V0.9.0 [umoeller]: moved this func here from xfldr.c
 */

VOID FormatFrame(PSUBCLASSEDLISTITEM psli, // in: frame information
                 MPARAM mp1,            // in: mp1 from WM_FORMATFRAME (points to SWP array)
                 ULONG *pulCount)       // in/out: frame control count from default wnd proc
{
    // access the SWP array that is passed to us
    // and search all the controls for the container child window,
    // which for folders always has the ID 0x8008
    ULONG       ul;
    PSWP        swpArr = (PSWP)mp1;
    CNRINFO     CnrInfo;

    for (ul = 0; ul < *pulCount; ul++)
    {
        if (WinQueryWindowUShort( swpArr[ul].hwnd, QWS_ID ) == 0x8008 )
                                                         // FID_CLIENT
        {
            // container found: reduce size of container by
            // status bar height
            POINTL          ptlBorderSizes;
            // XFolderData     *somThis = XFolderGetData(psli->somSelf);
            ULONG ulStatusBarHeight = cmnQueryStatusBarHeight();
            WinSendMsg(psli->hwndFrame, WM_QUERYBORDERSIZE,
                    (MPARAM)&ptlBorderSizes, MPNULL);

            // first initialize the _new_ SWP for the status bar.
            // Since the SWP array for the std frame controls is
            // zero-based, and the standard frame controls occupy
            // indices 0 thru ulCount-1 (where ulCount is the total
            // count), we use ulCount for our static text control.
            swpArr[*pulCount].fl = SWP_MOVE | SWP_SIZE | SWP_NOADJUST | SWP_ZORDER;
            swpArr[*pulCount].x  = ptlBorderSizes.x;
            swpArr[*pulCount].y  = ptlBorderSizes.y;
            swpArr[*pulCount].cx = swpArr[ul].cx;
            swpArr[*pulCount].cy = ulStatusBarHeight;
            swpArr[*pulCount].hwndInsertBehind = HWND_BOTTOM; // HWND_TOP;
            swpArr[*pulCount].hwnd = psli->hwndStatusBar;

            // adjust the origin and height of the container to
            // accomodate our static text control
            swpArr[ul].y  += swpArr[*pulCount].cy;
            swpArr[ul].cy -= swpArr[*pulCount].cy;

            // now we need to adjust the workspace origin of the cnr
            // accordingly, or otherwise the folder icons will appear
            // outside the visible cnr workspace and scroll bars will
            // show up.
            // We only do this the first time we're arriving here
            // (which should be before the WPS is populating the folder);
            // psli->fNeedCnrScroll has been initially set to TRUE
            // by xfCreateStatusBar.
            if (psli->fNeedCnrScroll)
            {
                cnrhQueryCnrInfo(swpArr[ul].hwnd, &CnrInfo);

                #ifdef DEBUG_STATUSBARS
                    _Pmpf(( "Old CnrInfo.ptlOrigin.y: %lX", CnrInfo.ptlOrigin.y ));
                #endif

                if (    ((LONG)CnrInfo.ptlOrigin.y >= (LONG)ulStatusBarHeight)
                   )
                {
                    CnrInfo.ptlOrigin.y -= ulStatusBarHeight;

                    #ifdef DEBUG_STATUSBARS
                        _Pmpf(( "New CnrInfo.ptlOrigin.y: %lX", CnrInfo.ptlOrigin.y ));
                    #endif

                    WinSendMsg(swpArr[ul].hwnd,
                               CM_SETCNRINFO,
                               (MPARAM)&CnrInfo,
                               (MPARAM)CMA_PTLORIGIN);
                    // set flag to FALSE to prevent a second adjustment
                    psli->fNeedCnrScroll = FALSE;
                }
            } // end if (psli->fNeedCnrScroll)
        } // end if WinQueryWindowUShort
    } // end for (ul = 0; ul < ulCount; ul++)
}

// original wnd proc for folder content menus,
// which we must subclass
PFNWP   pfnwpFolderContentMenuOriginal = NULL;

/*
 * InitMenu:
 *      this gets called from fdr_fnwpSubclassedFolderFrame
 *      when WM_INITMENU is received, _after_ the parent
 *      window proc has been given a chance to process this.
 *
 *      WM_INITMENU is sent to a menu owner right before a
 *      menu is about to be displayed. This applies to both
 *      pulldown and context menus.
 *
 *      This is needed for various menu features:
 *
 *      1)  for getting the object which currently has source
 *          emphasis in the folder container, because at the
 *          point WM_COMMAND comes in (and thus wpMenuItemSelected
 *          gets called in response), source emphasis has already
 *          been removed by the WPS.
 *
 *          This is needed for file operations on all selected
 *          objects in the container. We call wpshQuerySourceObject
 *          to find out more about this and store the result in
 *          our SUBCLASSEDLISTITEM.
 *
 *      2)  for folder content menus, because these are
 *          inserted as empty stubs only in wpModifyPopupMenu
 *          and only filled after the user clicks on them.
 *          We will query a bunch of data first, which we need
 *          later for drawing our items, and then call
 *          mnuFillContentSubmenu, which populates the folder
 *          and fills the menu with the items therein.
 *
 *      3)  for the menu system sounds.
 *
 *      4)  for manipulating Warp 4 folder menu _bars_. We
 *          cannot use the Warp 4 WPS methods defined for
 *          that because we want XWorkplace to run on Warp 3
 *          also.
 *
 *      WM_INITMENU parameters:
 *          SHORT mp1   menu item id
 *          HWND  mp2   menu window handle
 *      Returns: NULL always.
 *
 *@@changed V0.9.0 [umoeller]: moved this func here from xfldr.c
 */

VOID InitMenu(PSUBCLASSEDLISTITEM psli, // in: frame information
              ULONG sMenuIDMsg,         // in: mp1 from WM_INITMENU
              HWND hwndMenuMsg)         // in: mp2 from WM_INITMENU
{
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    // get XFolder instance data
    XFolderData     *somThis = XFolderGetData(psli->somSelf);

    #ifdef DEBUG_MENUS
        _Pmpf(( "WM_INITMENU: sMenuIDMsg = %lX, hwndMenuMsg = %lX",
                (ULONG)sMenuIDMsg,
                hwndMenuMsg ));
        _Pmpf(( "  psli->hwndCnr: 0x%lX", psli->hwndCnr));
    #endif

    // store object with source emphasis for later use;
    // this gets lost before WM_COMMAND otherwise
    psli->pSourceObject = wpshQuerySourceObject(psli->somSelf,
                                                psli->hwndCnr,
                                                FALSE,      // menu mode
                                                &psli->ulSelection);

    // store the container window handle in instance
    // data for wpModifyPopupMenu workaround;
    // buggy Warp 3 keeps setting hwndCnr to NULLHANDLE in there
    _hwndCnrSaved = psli->hwndCnr;

    // play system sound
    if (    (sMenuIDMsg < 0x8000) // avoid system menu
         || (sMenuIDMsg == 0x8020) // but include context menu
       )
        xthrPlaySystemSound(MMSOUND_XFLD_CTXTOPEN);

    // find out whether the menu of which we are notified
    // is a folder content menu; if so (and it is not filled
    // yet), the first menu item is ID_XFMI_OFS_DUMMY
    if ((ULONG)WinSendMsg(hwndMenuMsg,
                          MM_ITEMIDFROMPOSITION,
                          (MPARAM)0,        // menu item index
                          MPNULL)
               == (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_DUMMY))
    {
        // okay, let's go
        if (pGlobalSettings->FCShowIcons)
        {
            // show folder content icons ON:

            #ifdef DEBUG_MENUS
                _Pmpf(( "  preparing owner draw"));
            #endif

            mnuPrepareOwnerDraw(sMenuIDMsg, hwndMenuMsg);
        }

        // add menu items according to folder contents
        mnuFillContentSubmenu(sMenuIDMsg, hwndMenuMsg,
                              // this func subclasses the folder content
                              // menu wnd and stores the result here
                              &pfnwpFolderContentMenuOriginal);
    }
    else
    {
        // no folder content menu:

        // on Warp 4, check if the folder has a menu bar
        if (doshIsWarp4())
        {
            #ifdef DEBUG_MENUS
                _Pmpf(( "  checking for menu bar"));
            #endif

            if (sMenuIDMsg == 0x8005)
            {
                // seems to be some WPS menu item;
                // since the WPS seems to be using this
                // same ID for all the menu bar submenus,
                // we need to check the last selected
                // menu item, which was stored in the psli
                // structure by WM_MENUSELECT (below).
                PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();

                switch (psli->ulLastSelMenuItem)
                {
                    case 0x2D0: // "Edit" submenu
                    {
                        // find position of "Deselect all" item
                        SHORT sPos = (SHORT)WinSendMsg(hwndMenuMsg,
                                                       MM_ITEMPOSITIONFROMID,
                                                       MPFROM2SHORT(0x73,
                                                                    FALSE),
                                                       MPNULL);
                        #ifdef DEBUG_MENUS
                            _Pmpf(("  'Edit' menu found"));
                        #endif
                        // insert "Select by name" after that item
                        winhInsertMenuItem(hwndMenuMsg,
                                           sPos+1,
                                           (pGlobalSettings->VarMenuOffset
                                                   + ID_XFMI_OFS_SELECTSOME),
                                           pNLSStrings->pszSelectSome,
                                           MIS_TEXT, 0);
                    break; }

                    case 0x2D1: // "View" submenu
                    {
                        CNRINFO             CnrInfo;
                        #ifdef DEBUG_MENUS
                            _Pmpf(("  'View' menu found"));
                        #endif
                        // modify the "Sort" menu, as we would
                        // do it for context menus also
                        mnuModifySortMenu(psli->somSelf,
                                          hwndMenuMsg,
                                          pGlobalSettings,
                                          pNLSStrings);
                        cnrhQueryCnrInfo(psli->hwndCnr, &CnrInfo);
                        // and now insert the "folder view" items
                        winhInsertMenuSeparator(hwndMenuMsg,
                                                MIT_END,
                                                (pGlobalSettings->VarMenuOffset
                                                        + ID_XFMI_OFS_SEPARATOR));
                        mnuInsertFldrViewItems(psli->somSelf,
                                               hwndMenuMsg,  // hwndViewSubmenu
                                               FALSE,
                                               psli->hwndCnr,
                                               psli->ulView);
                                               // wpshQueryView(psli->somSelf,
                                                  //            psli->hwndFrame));
                    break; }

                    case 0x2D2:     // "Selected" submenu:
                    {
                    break; }

                    case 0x2D3: // "Help" submenu: add XFolder product info
                        #ifdef DEBUG_MENUS
                            _Pmpf(("  'Help' menu found"));
                        #endif
                        winhInsertMenuSeparator(hwndMenuMsg, MIT_END,
                                               (pGlobalSettings->VarMenuOffset
                                                       + ID_XFMI_OFS_SEPARATOR));
                        winhInsertMenuItem(hwndMenuMsg, MIT_END,
                                           (pGlobalSettings->VarMenuOffset
                                                   + ID_XFMI_OFS_PRODINFO),
                                           pNLSStrings->pszProductInfo,
                                           MIS_TEXT, 0);
                    break;

                } // end switch (psli->usLastSelMenuItem)
            } // end if (SHORT1FROMMP(mp1) == 0x8005)
        } // end if (doshIsWarp4())
    }
}

/*
 * MenuSelect:
 *      this gets called from fdr_fnwpSubclassedFolderFrame
 *      when WM_MENUSELECT is received.
 *      We need this for three reasons:
 *
 *      1) we will play a system sound, if desired;
 *
 *      2) we need to swallow this for very large folder
 *         content menus, because for some reason, PM will
 *         select a random menu item after we have repositioned
 *         a menu window on the screen (duh);
 *
 *      3) we can intercept certain menu items so that
 *         these don't get passed to wpMenuItemSelected,
 *         which appears to get called when the WPS folder window
 *         procedure responds to WM_COMMAND (which comes after
 *         WM_MENUSELECT only).
 *         This is needed for menu items such as those in
 *         the "Sort" menu so that the menu is not dismissed
 *         after selection.
 *
 *      WM_MENUSELECT parameters:
 *          mp1 USHORT usItem - selected menu item
 *              USHORT usPostCommand - TRUE: if we return TRUE,
 *                  a message will be posted to the owner.
 *          mp2 HWND - menu control wnd handle
 *
 *      If we set pfDismiss to TRUE, wpMenuItemSelected will be
 *      called, and the menu will be dismissed.
 *      Otherwise the message will be swallowed.
 *      We return TRUE if the menu item has been handled here.
 *      Otherwise the default wnd proc will be used.
 *
 *@@changed V0.9.0 [umoeller]: moved this func here from xfldr.c
 */

BOOL MenuSelect(PSUBCLASSEDLISTITEM psli, // in: frame information
                MPARAM mp1,               // in: mp1 from WM_MENUSELECT
                MPARAM mp2,               // in: mp2 from WM_MENUSELECT
                BOOL *pfDismiss)          // out: dismissal flag
{
    BOOL fHandled = FALSE;

    // return value for WM_MENUSELECT;
    // TRUE means dismiss menu
    psli->ulLastSelMenuItem = SHORT1FROMMP(mp1);

    // check if we have moved a folder content menu
    // (this flag is set by fdr_fnwpSubclFolderContentMenu); for
    // some reason, PM gets confused with the menu items
    // then and automatically tries to select the menu
    // item under the mouse, so we swallow this one
    // message if (a) the folder content menu has been
    // moved by fdr_fnwpSubclFolderContentMenu and (b) no mouse
    // button is pressed. These flags are all set by
    // fdr_fnwpSubclFolderContentMenu.
    if (fFldrContentMenuMoved)
    {
        #ifdef DEBUG_MENUS
            _Pmpf(( "  FCMenuMoved set!"));
        #endif
        if (!fFldrContentMenuButtonDown)
        {
            // mouse was not pressed: swallow this
            // menu selection
            *pfDismiss = FALSE;
        }
        else
        {
            // mouse was pressed: allow selection
            // and unset flags
            *pfDismiss = TRUE;
            fFldrContentMenuButtonDown = FALSE;
            fFldrContentMenuMoved = FALSE;
        }
        fHandled = TRUE;
    }
    else
    {
        USHORT      usItem = SHORT1FROMMP(mp1),
                    usPostCommand = SHORT2FROMMP(mp1);

        if (    (usPostCommand)
            && (    (usItem <  0x8000) // avoid system menu
                 || (usItem == 0x8020) // include context menu
               )
           )
        {
            HWND hwndCnr = wpshQueryCnrFromFrame(psli->hwndFrame);

            // play system sound
            xthrPlaySystemSound(MMSOUND_XFLD_CTXTSELECT);

            // Now check if we have a menu item which we don't
            // want to see dismissed.

            if (hwndCnr)
            {
                // first find out what kind of objects we have here
                /* ULONG ulSelection = 0;
                WPObject *pObject1 =
                     wpshQuerySourceObject(psli->somSelf,
                                           hwndCnr,
                                           &ulSelection); */

                WPObject *pObject = psli->pSourceObject;

                #ifdef DEBUG_MENUS
                    _Pmpf(( "  Object selections: %d", ulSelection));
                #endif

                // dereference shadows
                if (pObject)
                    if (_somIsA(pObject, _WPShadow))
                        pObject = _wpQueryShadowedObject(pObject, TRUE);

                // now call the functions in menus.c for this,
                // depending on the class of the object for which
                // the menu was opened
                if (pObject)
                {
                    if (_somIsA(pObject, _WPFileSystem))
                    {
                        fHandled = mnuFileSystemSelectingMenuItem(
                                       psli->pSourceObject,
                                            // note that we're passing
                                            // psli->pSourceObject instead of pObject;
                                            // psli->pSourceObject might be a shadow!
                                       usItem,
                                       (BOOL)usPostCommand,
                                       (HWND)mp2,               // hwndMenu
                                       hwndCnr,
                                       psli->ulSelection,       // SEL_* flags
                                       pfDismiss);              // dismiss-menu flag

                        if (    (!fHandled)
                             && (_somIsA(pObject, _WPFolder))
                           )
                        {
                            fHandled = mnuFolderSelectingMenuItem(pObject,
                                           usItem,
                                           (BOOL)usPostCommand, // fPostCommand
                                           (HWND)mp2,               // hwndMenu
                                           hwndCnr,
                                           psli->ulSelection,       // SEL_* flags
                                           pfDismiss);              // dismiss-menu flag
                        }
                    }

                    if (    (fHandled)
                         && (!(*pfDismiss))
                       )
                    {
                        // menu not to be dismissed: set the flag
                        // which will remove cnr source
                        // emphasis when the menu is dismissed
                        // later (WM_ENDMENU msg here)
                        psli->fRemoveSrcEmphasis = TRUE;
                    }
                }
            }
        }
    }

    return (fHandled);
}

/*
 *@@ WMChar_Delete:
 *      this gets called if "delete into trash can"
 *      is enabled and WM_CHAR has detected with the
 *      "Delete" key.
 *
 *@@added V0.9.1 (2000-01-31) [umoeller]
 */

VOID WMChar_Delete(PSUBCLASSEDLISTITEM psli)
{
    ULONG   ulSelection = 0;
    WPObject *pSelected = wpshQuerySourceObject(psli->somSelf,
                                                psli->hwndCnr,
                                                TRUE,       // keyboard mode
                                                &ulSelection);
    if (    (pSelected)
         && (ulSelection != SEL_NONEATALL)
       )
    {
        fopsStartCnrDelete2Trash(psli->somSelf, // source folder
                                 pSelected,    // first selected object
                                 ulSelection,  // can only be SEL_SINGLESEL
                                                // or SEL_MULTISEL
                                 psli->hwndCnr);
    }
}

/*
 *@@ fdr_fnwpSubclassedFolderFrame:
 *      New window proc for subclassed folder frame windows.
 *      Folder frame windows are subclassed in fdrSubclassFolderFrame
 *      (which gets called from XFolder::wpOpen or XFldDisk::wpOpen
 *      for Disk views) with the address of this window procedure.
 *
 *      This is maybe the most central (and most complex) part of
 *      XWorkplace. Since most WPS methods are really just reacting
 *      to messages in the default WPS frame window proc, but for
 *      some features methods are just not sufficient, basically we
 *      simulate what the WPS does here by intercepting _lots_
 *      of messages before the WPS gets them.
 *
 *      Unfortunately, this leads to quite a mess, but we have
 *      no choice.
 *
 *      Things we do in this proc:
 *
 *      --  frame control manipulation for status bars
 *
 *      --  Warp 4 folder menu bar manipulation (WM_INITMENU)
 *
 *      --  handling of certain menu items w/out dismissing
 *          the menu; this calls functions in menus.c
 *
 *      --  menu owner draw (folder content menus w/ icons);
 *          this calls functions in menus.c also
 *
 *      --  complete interception of file-operation menu items
 *          such as "delete" for deleting all objects into the
 *          XWorkplace trash can
 *
 *      --  container control messages: tree view auto-scroll,
 *          updating status bars etc.
 *
 *      --  playing the new system sounds for menus and such.
 *
 *      Note that this function calls lots of "external" functions
 *      spread across all over the XFolder code. I have tried to
 *      reduce the size of this window procedure to an absolute
 *      minimum because this function gets called very often (for
 *      every single folder message) and large message procedures
 *      may thrash the processor caches.
 *
 *@@changed V0.9.0 [umoeller]: moved cleanup code from WM_CLOSE to WM_DESTROY; un-subclassing removed
 *@@changed V0.9.0 [umoeller]: moved this func here from xfldr.c
 *@@changed V0.9.1 (2000-01-29) [umoeller]: added WPMENUID_DELETE support
 *@@changed V0.9.1 (2000-01-31) [umoeller]: added "Del" key support
 */

MRESULT EXPENTRY fdr_fnwpSubclassedFolderFrame(HWND hwndFrame,
                                               ULONG msg,
                                               MPARAM mp1,
                                               MPARAM mp2)
{
    PSUBCLASSEDLISTITEM psli = NULL;
    PFNWP           pfnwpOriginal = NULL;
    MRESULT         mrc = MRFALSE;
    BOOL            fCallDefault = FALSE;

    TRY_LOUD(excpt1, NULL)
    {
        // find the original wnd proc in the
        // global linked list, so we can pass messages
        // on to it
        psli = fdrQueryPSLI(hwndFrame, NULL);
        if (psli)
        {
            pfnwpOriginal = psli->pfnwpOriginal;
            // somSelf = psli->somSelf;
        }

        if (pfnwpOriginal)
        {
            switch(msg)
            {
                /* *************************
                 *                         *
                 * Status bar:             *
                 *                         *
                 **************************/

                /*
                 *  The following code adds status bars to folder frames.
                 *  The XFolder status bars are implemented as frame controls
                 *  (similar to the title-bar buttons and menus). In order
                 *  to do this, we need to intercept the following messages
                 *  which are sent to the folder's frame window when the
                 *  frame is updated as a reaction to WM_UPDATEFRAME or WM_SIZE.
                 *
                 *  Note that wpOpen has created the status bar window (which
                 *  is a static control subclassed with fdr_fnwpStatusBar) already
                 *  and stored the HWND in the SUBCLASSEDLISTITEM.hwndStatusBar
                 *  structure member (which psli points to now).
                 *
                 *  Here we only relate the status bar to the frame. The actual
                 *  painting etc. is done in fdr_fnwpStatusBar.
                 */

                /*
                 * WM_QUERYFRAMECTLCOUNT:
                 *      this gets sent to the frame when PM wants to find out
                 *      how many frame controls the frame has. We call the
                 *      "parent" window proc and add one for the status bar.
                 */

                case WM_QUERYFRAMECTLCOUNT:
                {
                    // query the standard frame controls count
                    ULONG ulrc = (ULONG)((*pfnwpOriginal)(hwndFrame, msg, mp1, mp2));

                    // if we have a status bar, increment the count
                    if (psli->hwndStatusBar)
                        ulrc++;

                    mrc = (MPARAM)ulrc;
                break; }

                /*
                 * WM_FORMATFRAME:
                 *    this message is sent to a frame window to calculate the sizes
                 *    and positions of all of the frame controls and the client window.
                 *
                 *    Parameters:
                 *          mp1     PSWP    pswp        structure array; for each frame
                 *                                      control which was announced with
                 *                                      WM_QUERYFRAMECTLCOUNT, PM has
                 *                                      allocated one SWP structure.
                 *          mp2     PRECTL  pprectl     pointer to client window
                 *                                      rectangle of the frame
                 *          returns USHORT  ccount      count of the number of SWP
                 *                                      arrays returned
                 *
                 *    It is the responsibility of this message code to set up the
                 *    SWP structures in the given array to position the frame controls.
                 *    We call the "parent" window proc and then set up our status bar.
                 */

                case WM_FORMATFRAME:
                {
                    //  query the number of standard frame controls
                    ULONG ulCount = (ULONG)((*pfnwpOriginal)(hwndFrame, msg, mp1, mp2));

                    #ifdef DEBUG_STATUSBARS
                        _Pmpf(( "WM_FORMATFRAME ulCount = %d", ulCount ));
                    #endif

                    if (psli->hwndStatusBar)
                    {
                        // we have a status bar:
                        // format the frame
                        FormatFrame(psli, mp1, &ulCount);

                        // increment the number of frame controls
                        // to include our status bar
                        mrc = (MRESULT)(ulCount + 1);
                    } // end if (psli->hwndStatusBar)
                    else
                        mrc = (MRESULT)ulCount;
                break; }

                /*
                 * WM_CALCFRAMERECT:
                 *     this message occurs when an application uses the
                 *     WinCalcFrameRect function.
                 *
                 *     Parameters:
                 *          mp1     PRECTL  pRect      rectangle structure
                 *          mp2     USHORT  usFrame    frame indicator
                 *          returns BOOL    rc         rectangle-calculated indicator
                 */

                case WM_CALCFRAMERECT:
                {
                    mrc = (*pfnwpOriginal)(hwndFrame, msg, mp1, mp2);

                    if (psli->hwndStatusBar)
                        // we have a status bar: calculate its rectangle
                        CalcFrameRect(mp1, mp2);
                break; }

                /* *************************
                 *                         *
                 * Menu items:             *
                 *                         *
                 **************************/

                /*
                 * WM_INITMENU:
                 *      this message is sent to a frame whenever a menu
                 *      is about to be displayed. This is needed for
                 *      various menu features; see InitMenu() above.
                 */

                case WM_INITMENU:
                    // call the default, in case someone else
                    // is subclassing folders (ObjectDesktop?!?);
                    // from what I've checked, the WPS does NOTHING
                    // with this message, not even for menu bars...
                    mrc = (MRESULT)(*pfnwpOriginal)(hwndFrame, msg, mp1, mp2);

                    InitMenu(psli,
                             (ULONG)mp1,
                             (HWND)mp2);
                break;

                /*
                 * WM_MENUSELECT:
                 *      this is SENT to a menu owner by the menu
                 *      control to determine what to do right after
                 *      a menu item has been selected. If we return
                 *      TRUE, the menu will be dismissed.
                 *
                 *      See MenuSelect() above.
                 */

                case WM_MENUSELECT:
                {
                    BOOL fDismiss = TRUE;

                    #ifdef DEBUG_MENUS
                        _Pmpf(( "WM_MENUSELECT: mp1 = %lX/%lX, mp2 = %lX",
                                SHORT1FROMMP(mp1),
                                SHORT2FROMMP(mp1),
                                mp2 ));
                    #endif

                    // always call the default, in case someone else
                    // is subclassing folders (ObjectDesktop?!?)
                    mrc = (MRESULT)(*pfnwpOriginal)(hwndFrame, msg, mp1, mp2);

                    // now handle our stuff; this might modify mrc to
                    // have the menu stay on the screen
                    if (MenuSelect(psli, mp1, mp2, &fDismiss))
                        // processed: return the modified flag instead
                        mrc = (MRESULT)fDismiss;
                break; }

                /*
                 * WM_MENUEND:
                 *      this message occurs when a menu control is about to
                 *      terminate. We need to remove cnr source emphasis
                 *      if the user has requested a context menu from a
                 *      status bar.
                 */

                case WM_MENUEND:
                {
                    #ifdef DEBUG_MENUS
                        _Pmpf(( "WM_MENUEND: mp1 = %lX, mp2 = %lX",
                                mp1, mp2 ));
                        /* _Pmpf(( "  fFolderContentWindowPosChanged: %d",
                                fFolderContentWindowPosChanged));
                        _Pmpf(( "  fFolderContentButtonDown: %d",
                                fFolderContentButtonDown)); */
                    #endif
                    // menu opened from status bar?
                    if (psli->fRemoveSrcEmphasis)
                    {
                        // if so, remove cnr source emphasis
                        WinSendMsg(psli->hwndCnr,
                                   CM_SETRECORDEMPHASIS,
                                   (MPARAM)NULL,   // undocumented: if precc == NULL,
                                                   // the whole cnr is given emphasis
                                   MPFROM2SHORT(FALSE,  // remove emphasis
                                           CRA_SOURCE));
                        // and make sure the container has the
                        // focus
                        WinSetFocus(HWND_DESKTOP, psli->hwndCnr);
                        // reset flag for next context menu
                        psli->fRemoveSrcEmphasis = FALSE;
                    }

                    // unset flag for WM_MENUSELECT above
                    fFldrContentMenuMoved = FALSE;

                    mrc = (MRESULT)(*pfnwpOriginal)(hwndFrame, msg, mp1, mp2);
                break; }

                /*
                 * WM_MEASUREITEM:
                 *      this msg is sent only once per owner-draw item when
                 *      PM needs to know its size. This gets sent to us for
                 *      items in folder content menus (if icons are on); the
                 *      height of our items will be the same as with
                 *      non-owner-draw ones, but we need to calculate the width
                 *      according to the item text.
                 *
                 *      Return value: check mnuMeasureItem (menus.c)
                 */

                case WM_MEASUREITEM:
                {
                    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
                    if ( (SHORT)mp1 > (pGlobalSettings->VarMenuOffset+ID_XFMI_OFS_VARIABLE) )
                    {
                        // call the measure-item func in menus.c
                        mrc = mnuMeasureItem((POWNERITEM)mp2, pGlobalSettings);
                    }
                    else
                        // none of our items: pass to original wnd proc
                        mrc = (MRESULT)(*pfnwpOriginal)(hwndFrame, msg, mp1, mp2);
                break; }

                /*
                 * WM_DRAWITEM:
                 *      this msg is sent for each item every time it
                 *      needs to be redrawn. This gets sent to us for
                 *      items in folder content menus (if icons are on).
                 */

                case WM_DRAWITEM:
                {
                    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
                    if ( (SHORT)mp1 > (pGlobalSettings->VarMenuOffset+ID_XFMI_OFS_VARIABLE) )
                    {
                        // variable menu item: this must be a folder-content
                        // menu item, because for others no WM_DRAWITEM is sent
                        // (menus.c)
                        if (mnuDrawItem(pGlobalSettings,
                                        mp1, mp2))
                            mrc = (MRESULT)TRUE;
                        else // error occured:
                            mrc = (MRESULT)(*pfnwpOriginal)(hwndFrame, msg, mp1, mp2);
                    }
                    else
                        mrc = (MRESULT)(*pfnwpOriginal)(hwndFrame, msg, mp1, mp2);
                break; }

                /*
                 * WM_COMMAND:
                 *
                 *
                 */

                case WM_COMMAND:
                {
                    USHORT usCommand = SHORT1FROMMP(mp1);

                    #ifdef DEBUG_TRASHCAN
                        _Pmpf(("fdr_fnwpSubclassedFolderFrame WM_COMMAND 0x%lX", usCommand));
                    #endif
                    if (usCommand == WPMENUID_DELETE)
                    {
                        PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
                        if (pGlobalSettings->fTrashDelete)
                        {
                            fopsStartCnrDelete2Trash(psli->somSelf,        // source folder
                                                     psli->pSourceObject,  // first source object
                                                     psli->ulSelection,
                                                     psli->hwndCnr);
                            break;
                        }
                    }

                    fCallDefault = TRUE;
                    #ifdef DEBUG_TRASHCAN
                        _Pmpf(("    WM_COMMAND: calling default"));
                    #endif
                break; }

                /* *************************
                 *                         *
                 * Miscellaneae:           *
                 *                         *
                 **************************/

                /*
                 * WM_CHAR:
                 *      this is intercepted to provide folder hotkeys
                 *      and "Del" key support if "delete into trashcan"
                 *      is on.
                 */

                case WM_CHAR:
                {
                    USHORT usFlags    = SHORT1FROMMP(mp1);
                    #ifdef DEBUG_TRASHCAN
                        _Pmpf(("fdr_fnwpSubclassedFolderFrame WM_CHAR usFlags = 0x%lX", usFlags));
                    #endif
                    // whatever happens, we're only interested
                    // in key-down messages
                    if ((usFlags & KC_KEYUP) == 0)
                    {
                        XFolderData         *somThis = XFolderGetData(psli->somSelf);
                        PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

                        USHORT usch       = SHORT1FROMMP(mp2);
                        USHORT usvk       = SHORT2FROMMP(mp2);

                        // check whether "delete to trash can" is on
                        if (pGlobalSettings->fTrashDelete)
                        {
                            // yes: intercept "Del" key
                            if (usFlags & KC_VIRTUALKEY)
                                if (usvk == VK_DELETE)
                                {
                                    WMChar_Delete(psli);
                                    // swallow this key,
                                    // do not process default winproc
                                    break;
                                }
                        }

                        // check whether folder hotkeys are allowed at all
                        if (    (pGlobalSettings->fEnableFolderHotkeys)
                            // yes: check folder and global settings
                             && (   (_bFolderHotkeysInstance == 1)
                                ||  (   (_bFolderHotkeysInstance == 2)   // use global settings:
                                     && (pGlobalSettings->fFolderHotkeysDefault)
                                    )
                                )
                           )
                        {
                            if (fdrProcessFldrHotkey(hwndFrame,
                                                     usFlags,
                                                     usch,
                                                     usvk))
                            {
                                // was a hotkey:
                                mrc = (MRESULT)TRUE;
                                // swallow this key,
                                // do not process default winproc
                                break;
                            }
                        }
                    }

                    #ifdef DEBUG_TRASHCAN
                        _Pmpf(("    WM_CHAR: calling default"));
                    #endif
                    fCallDefault = TRUE;
                break; }

                /*
                 * WM_CONTROL:
                 *      this is intercepted to check for container
                 *      notifications we might be interested in.
                 */

                case WM_CONTROL:
                {
                    if (SHORT1FROMMP(mp1) /* id */ == 0x8008) // container!!
                    {
                        #ifdef DEBUG_CNRCNTRL
                            CHAR szTemp2[30];
                            sprintf(szTemp2, "unknown: %d", SHORT2FROMMP(mp1));
                            _Pmpf(("Cnr cntrl msg: %s, mp2: %lX",
                                (SHORT2FROMMP(mp1) == CN_BEGINEDIT) ? "CN_BEGINEDIT"
                                    : (SHORT2FROMMP(mp1) == CN_COLLAPSETREE) ? "CN_COLLAPSETREE"
                                    : (SHORT2FROMMP(mp1) == CN_CONTEXTMENU) ? "CN_CONTEXTMENU"
                                    : (SHORT2FROMMP(mp1) == CN_DRAGAFTER) ? "CN_DRAGAFTER"
                                    : (SHORT2FROMMP(mp1) == CN_DRAGLEAVE) ? "CN_DRAGLEAVE"
                                    : (SHORT2FROMMP(mp1) == CN_DRAGOVER) ? "CN_DRAGOVER"
                                    : (SHORT2FROMMP(mp1) == CN_DROP) ? "CN_DROP"
                                    : (SHORT2FROMMP(mp1) == CN_DROPNOTIFY) ? "CN_DROPNOTIFY"
                                    : (SHORT2FROMMP(mp1) == CN_DROPHELP) ? "CN_DROPHELP"
                                    : (SHORT2FROMMP(mp1) == CN_EMPHASIS) ? "CN_EMPHASIS"
                                    : (SHORT2FROMMP(mp1) == CN_ENDEDIT) ? "CN_ENDEDIT"
                                    : (SHORT2FROMMP(mp1) == CN_ENTER) ? "CN_ENTER"
                                    : (SHORT2FROMMP(mp1) == CN_EXPANDTREE) ? "CN_EXPANDTREE"
                                    : (SHORT2FROMMP(mp1) == CN_HELP) ? "CN_HELP"
                                    : (SHORT2FROMMP(mp1) == CN_INITDRAG) ? "CN_INITDRAG"
                                    : (SHORT2FROMMP(mp1) == CN_KILLFOCUS) ? "CN_KILLFOCUS"
                                    : (SHORT2FROMMP(mp1) == CN_PICKUP) ? "CN_PICKUP"
                                    : (SHORT2FROMMP(mp1) == CN_QUERYDELTA) ? "CN_QUERYDELTA"
                                    : (SHORT2FROMMP(mp1) == CN_REALLOCPSZ) ? "CN_REALLOCPSZ"
                                    : (SHORT2FROMMP(mp1) == CN_SCROLL) ? "CN_SCROLL"
                                    : (SHORT2FROMMP(mp1) == CN_SETFOCUS) ? "CN_SETFOCUS"
                                    : szTemp2,
                                mp2));
                        #endif

                        switch (SHORT2FROMMP(mp1))      // usNotifyCode
                        {
                            /*
                             * CN_BEGINEDIT:
                             *      this is sent by the container control
                             *      when direct text editing is about to
                             *      begin, that is, when the user alt-clicks
                             *      on an object title.
                             *      We'll select the file stem of the object.
                             */

                            /* case CN_BEGINEDIT: {
                                PCNREDITDATA pced = (PCNREDITDATA)mp2;
                                mrc = (MRESULT)(*pfnwpOriginal)(hwndFrame, msg, mp1, mp2);
                                if (pced) {
                                    PMINIRECORDCORE pmrc = (PMINIRECORDCORE)pced->pRecord;
                                    if (pmrc) {
                                        // editing WPS record core, not title etc.:
                                        // get the window ID of the MLE control
                                        // in the cnr window
                                        HWND hwndMLE = WinWindowFromID(pced->hwndCnr,
                                                            CID_MLE);
                                        if (hwndMLE) {
                                            ULONG cbText = WinQueryWindowTextLength(
                                                                    hwndMLE)+1;
                                            PSZ pszText = malloc(cbText);
                                            _Pmpf(("textlen: %d", cbText));
                                            if (WinQueryWindowText(hwndMLE,
                                                                   cbText,
                                                                   pszText))
                                            {
                                                PSZ pszLastDot = strrchr(pszText, '.');
                                                _Pmpf(("text: %s", pszText));
                                                WinSendMsg(hwndMLE,
                                                        EM_SETSEL,
                                                        MPFROM2SHORT(
                                                            // first char: 0
                                                            0,
                                                            // last char:
                                                            (pszLastDot)
                                                                ? (pszLastDot-pszText)
                                                                : 10000
                                                        ), MPNULL);
                                            }
                                            free(pszText);
                                        }
                                    }
                                }
                            break; } */

                            /*
                             * CN_ENTER:
                             *      double-click or enter key:
                             *      play sound
                             */

                            case CN_ENTER:
                                xthrPlaySystemSound(MMSOUND_XFLD_CNRDBLCLK);
                                mrc = (MRESULT)(*pfnwpOriginal)(hwndFrame, msg, mp1, mp2);
                            break;

                            /*
                             * CN_EMPHASIS:
                             *      selection changed:
                             *      update status bar
                             */

                            case CN_EMPHASIS:
                                mrc = (MRESULT)(*pfnwpOriginal)(hwndFrame, msg, mp1, mp2);
                                if (psli->hwndStatusBar)
                                {
                                    #ifdef DEBUG_STATUSBARS
                                        _Pmpf(( "CN_EMPHASIS: posting PM_UPDATESTATUSBAR to hwnd %lX", psli->hwndStatusBar ));
                                    #endif
                                    WinPostMsg(psli->hwndStatusBar,
                                               STBM_UPDATESTATUSBAR,
                                               MPNULL,
                                               MPNULL);
                                }
                            break;

                            /*
                             * CN_EXPANDTREE:
                             *      tree view has been expanded:
                             *      do cnr auto-scroll in Worker thread
                             */

                            case CN_EXPANDTREE:
                            {
                                PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
                                mrc = (MRESULT)(*pfnwpOriginal)(hwndFrame, msg, mp1, mp2);
                                if (pGlobalSettings->TreeViewAutoScroll)
                                    xthrPostFileMsg(FIM_TREEVIEWAUTOSCROLL,
                                                    (MPARAM)hwndFrame,
                                                    mp2); // PMINIRECORDCORE
                            break; }

                            default:
                                fCallDefault = TRUE;
                            break;
                        } // end switch (SHORT2FROMMP(mp1))      // usNotifyCode
                    }
                break; }

                /*
                 * WM_DESTROY:
                 *      upon this, we need to clean up our linked list
                 *      of subclassed windows
                 */

                case WM_DESTROY:
                {
                    HWND hwndSuppl = psli->hwndSupplObject;

                    // upon closing the window, undo the subclassing, in case
                    // some other message still comes in
                    WinSubclassWindow(hwndFrame, pfnwpOriginal);

                    // and remove this window from our subclassing linked list
                    fdrRemovePSLI(psli);

                    // destroy the supplementary object window for this folder
                    // frame window
                    WinDestroyWindow(hwndSuppl);

                    // do the default stuff
                    fCallDefault = TRUE;
                break; }

                default:
                    fCallDefault = TRUE;
                break;

            } // end switch
        } // end if (pfnwpOriginal)
        else
        {
            // original window procedure not found:
            // that's an error
            fCallDefault = TRUE;
        }

        if (fCallDefault)
        {
            // this has only been set to TRUE for "default" in
            // the switch statement above; we then call the
            // default window procedure.
            // This is either the original folder frame window proc
            // of the WPS itself or maybe the one of other WPS enhancers
            // which have subclassed folder windows (ObjectDesktop
            // and the like).
            // We do this outside the TRY/CATCH stuff above so that
            // we don't get blamed for exceptions which we are not
            // responsible for, which was the case with XFolder < 0.85
            // (i.e. exceptions in PMWP.DLL or Object Desktop or whatever).
            if (pfnwpOriginal)
                mrc = (MRESULT)(*pfnwpOriginal)(hwndFrame, msg, mp1, mp2);
            else
            {
                _Pmpf(("WARNING fdr_fnwpSubclassedFolderFrame: pfnwpOriginal not found!!"));
                mrc = WinDefWindowProc(hwndFrame, msg, mp1, mp2);
            }
        }
    } // end TRY_LOUD
    CATCH(excpt1)
    {
        // exception occured:
        return (0);
    } END_CATCH();

    return (mrc);
}

/*
 *@@ fdr_fnwpSupplFolderObject:
 *      this is the wnd proc for the "Supplementary Object wnd"
 *      which is created for each folder frame window when it's
 *      subclassed. We need this window to handle additional
 *      messages which are not part of the normal message set,
 *      which is handled by fdr_fnwpSubclassedFolderFrame.
 *
 *      This window gets created in fdrSubclassFolderFrame, when
 *      the folder frame is also subclassed.
 *
 *      If we processed additional messages in fdr_fnwpSubclassedFolderFrame,
 *      we'd probably ruin other WPS enhancers which might use the same
 *      message in a different context (ObjectDesktop?), so we use a
 *      different window, which we own all alone.
 *
 *      We cannot use the global XFolder object window either
 *      (krn_fnwpThread1Object, kernel.c) because sometimes
 *      folder windows do not run in the main PM thread
 *      (TID 1), esp. when they're opened using WinOpenObject or
 *      REXX functions. I have found that manipulating windows
 *      from threads other than the one which created the window
 *      can produce really funny results or bad PM hangs.
 *
 *      This wnd proc always runs in the same thread as the folder
 *      frame wnd does.
 *
 *      This func is new with XFolder V0.82.
 *
 *@@changed V0.9.0 [umoeller]: moved this func here from xfldr.c
 */

MRESULT EXPENTRY fdr_fnwpSupplFolderObject(HWND hwndObject, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MPARAM mrc = NULL;
    PSUBCLASSEDLISTITEM psli = (PSUBCLASSEDLISTITEM)
                WinQueryWindowULong(hwndObject, QWL_USER);

    switch (msg)
    {
        case WM_CREATE:
            // set the USER window word to the SUBCLASSEDLISTITEM
            // structure which is passed to us upon window
            // creation (see cmnSubclassFrameWnd, which creates us)
            mrc = WinDefWindowProc(hwndObject, msg, mp1, mp2);
            psli = (PSUBCLASSEDLISTITEM)mp1;
            WinSetWindowULong(hwndObject, QWL_USER, (ULONG)psli);
        break;

        /*
         * SOM_ACTIVATESTATUSBAR:
         *      add / remove / repaint the folder status bar;
         *      this is posted every time XFolder needs to change
         *      anything about status bars. We must not play with
         *      frame controls from threads other than the thread
         *      in which the status bar was created, i.e. the thread
         *      in which the folder frame is running (which, in most
         *      cases, is thread 1, the main PM thread of the WPS),
         *      because reformatting frame controls from other
         *      threads will cause PM hangs or WPS crashes.
         *
         *      Parameters:
         *      ULONG mp1   0: disable (destroy) status bar
         *                  1: enable (create) status bar
         *                  2: update (reformat) status bar
         *      HWND  mp2:  hwndView (frame) to update
         */

        case SOM_ACTIVATESTATUSBAR:
        {
            HWND hwndFrame = (HWND)mp2;
            #ifdef DEBUG_STATUSBARS
                _Pmpf(( "SOM_ACTIVATESTATUSBAR, mp1: %lX, psli: %lX", mp1, psli));
            #endif

            if (psli)
                switch ((ULONG)mp1)
                {
                    case 0:
                        fdrCreateStatusBar(psli->somSelf, psli, FALSE);
                    break;

                    case 1:
                        fdrCreateStatusBar(psli->somSelf, psli, TRUE);
                    break;

                    default:
                    {
                        // == 2 => update status bars; this is
                        // necessary if the font etc. has changed
                        const char* pszStatusBarFont =
                                cmnQueryStatusBarSetting(SBS_STATUSBARFONT);
                        // avoid recursing
                        WinSendMsg(psli->hwndStatusBar, STBM_PROHIBITBROADCASTING,
                                   (MPARAM)TRUE, MPNULL);
                        // set font
                        WinSetPresParam(psli->hwndStatusBar, PP_FONTNAMESIZE,
                                        (ULONG)strlen(pszStatusBarFont) + 1, (PVOID)pszStatusBarFont);
                        // update frame controls
                        WinSendMsg(hwndFrame, WM_UPDATEFRAME, MPNULL, MPNULL);
                        // update status bar text synchronously
                        WinSendMsg(psli->hwndStatusBar, STBM_UPDATESTATUSBAR, MPNULL, MPNULL);
                    break; }
                }
        break; }

        default:
            mrc = WinDefWindowProc(hwndObject, msg, mp1, mp2);
    }
    return (mrc);
}

/* ******************************************************************
 *                                                                  *
 *   Folder linked lists                                            *
 *                                                                  *
 ********************************************************************/

/*
 *@@ fdrAddToList:
 *      appends or removes the given folder to or from
 *      the given folder list.
 *
 *      A "folder list" is an abstract concept of a list
 *      of folders which is stored in OS2.INI (in the
 *      "XWorkplace" section under the pcszIniKey key).
 *
 *      If fInsert == TRUE, somSelf is added to the list.
 *      If fInsert == FALSE, somSelf is removed from the list.
 *
 *      Returns TRUE if the list was changed. In that case,
 *      the list is rewritten to the specified INI key.
 *
 *      The list is protected by a global mutex semaphore,
 *      so this is thread-safe.
 *
 *      This is used from the XFolder methods for
 *      favorite and quick-open folders. The linked
 *      list roots are stored in classes\xfldr.c.
 *
 *@@added V0.9.0 (99-11-16) [umoeller]
 */

BOOL fdrAddToList(WPFolder *somSelf,
                  PLINKLIST pllFolders,     // in: linked list of CONTENTMENULISTITEM's
                  BOOL fInsert,
                  const char *pcszIniKey)
{
    BOOL    brc = FALSE,
            fSemOwned = FALSE;
    CHAR    szFavoriteFolders[1000] = "";
    PLISTNODE   pNode = 0;
    PCONTENTMENULISTITEM pcmli = 0,
                         pcmliFound = 0;

    // create mutex semaphores for serialized access
    if (hmtxFolderLists == NULLHANDLE)
        DosCreateMutexSem(NULL,
                          &hmtxFolderLists,
                          0,
                          FALSE);

    TRY_LOUD(excpt1, NULL)
    {
        fSemOwned = (WinRequestMutexSem(hmtxFolderLists, 4000) == NO_ERROR);
        if (fSemOwned)
        {

            // find folder on list
            pNode = lstQueryFirstNode(pllFolders);
            while (pNode)
            {
                pcmli = pNode->pItemData;

                if (pcmli->pFolder == somSelf)
                {
                    pcmliFound = pcmli;
                    break;
                }

                pNode = pNode->pNext;
            }

            if (fInsert)
            {
                // insert mode:
                if (!pcmliFound)
                {
                    // not on list yet: append
                    pcmli = malloc(sizeof(CONTENTMENULISTITEM));
                    pcmli->pFolder = somSelf;
                    lstAppendItem(pllFolders,
                                  pcmli);
                    brc = TRUE;
                }
            }
            else
            {
                // remove mode:
                if (pcmliFound)
                {
                    lstRemoveItem(pllFolders,
                                  pcmliFound);
                    brc = TRUE;
                }
            }

            if (brc)
            {
                // list changed:
                // write list to INI as a string of handles
                pNode = lstQueryFirstNode(pllFolders);
                if (pNode)
                {
                    // list is not empty: recompose string
                    while (pNode)
                    {
                        pcmli = pNode->pItemData;
                        sprintf(szFavoriteFolders+strlen(szFavoriteFolders),
                                "%lX ",
                                _wpQueryHandle(pcmli->pFolder));
                        pNode = pNode->pNext;
                    }

                    PrfWriteProfileString(HINI_USERPROFILE,
                                          INIAPP_XWORKPLACE, (PSZ)pcszIniKey,
                                          szFavoriteFolders);
                }
                else
                {
                    // list is empty: remove
                    PrfWriteProfileData(HINI_USERPROFILE,
                                        INIAPP_XWORKPLACE, (PSZ)pcszIniKey,
                                        NULL, 0);
                }
            }
        }
    }
    CATCH(excpt1) {} END_CATCH();

    if (fSemOwned)
    {
        DosReleaseMutexSem(hmtxFolderLists);
        fSemOwned = FALSE;
    }

    return (brc);
}

/*
 *@@ fdrIsOnList:
 *      returns TRUE if the given folder is on the
 *      given list of CONTENTMENULISTITEM's.
 *
 *      The list is protected by a global mutex semaphore,
 *      so this is thread-safe.
 *      This is used from the XFolder methods for
 *      favorite and quick-open folders. The linked
 *      lists are stored in classes\xfldr.c.
 *
 *@@added V0.9.0 (99-11-16) [umoeller]
 */

BOOL fdrIsOnList(WPFolder *somSelf,
                 PLINKLIST pllFolders)     // in: linked list of CONTENTMENULISTITEM's
{
    PCONTENTMENULISTITEM pcmli;
    BOOL                 rc = FALSE,
                         fSemOwned = FALSE;

    // create mutex semaphores for serialized access
    if (hmtxFolderLists == NULLHANDLE)
        DosCreateMutexSem(NULL,
                          &hmtxFolderLists,
                          0,
                          FALSE);

    TRY_LOUD(excpt1, NULL)
    {
        fSemOwned = (WinRequestMutexSem(hmtxFolderLists, 4000) == NO_ERROR);
        if (fSemOwned)
        {
            PLISTNODE pNode = lstQueryFirstNode(pllFolders);
            while (pNode)
            {
                pcmli = pNode->pItemData;
                if (pcmli->pFolder == somSelf)
                {
                    rc = TRUE;
                    break;
                }
                else
                    pNode = pNode->pNext;
            }
        }
    }
    CATCH(excpt1) { } END_CATCH();

    if (fSemOwned)
    {
        DosReleaseMutexSem(hmtxFolderLists);
        fSemOwned = FALSE;
    }
    return (rc);
}

/*
 *@@ fdrEnumList:
 *      this enumerates folders on the given list of
 *      CONTENTMENULISTITEM's.
 *
 *      If pFolder == NULL, the first folder on the list
 *      is returned, otherwise the folder which comes
 *      after pFolder in the list. If no (more) folders
 *      are found, NULL is returned.
 *
 *      The list is protected by a global mutex semaphore,
 *      so this is thread-safe.
 *      This is used from the XFolder methods for
 *      favorite and quick-open folders. The linked
 *      lists are stored in classes\xfldr.c.
 *
 *@@added V0.9.0 (99-11-16) [umoeller]
 */

WPFolder* fdrEnumList(PLINKLIST pllFolders,     // in: linked list of CONTENTMENULISTITEM's
                      WPFolder *pFolder,
                      const char *pcszIniKey)
{
    BOOL        fSemOwned = FALSE;
    PLISTNODE               pNode = lstQueryFirstNode(pllFolders);
    PCONTENTMENULISTITEM    pItemFound = 0;

    // create mutex semaphores for serialized access
    if (hmtxFolderLists == NULLHANDLE)
        DosCreateMutexSem(NULL,
                          &hmtxFolderLists,
                          0,
                          FALSE);

    TRY_LOUD(excpt1, NULL)
    {
        fSemOwned = (WinRequestMutexSem(hmtxFolderLists, 4000) == NO_ERROR);
        if (fSemOwned)
        {
            if (pNode == NULL)
            {
                // if the list of favorite folders has not yet been built
                // yet, we will do this now
                CHAR        szFavorites[1000],
                            szDummy[1000];
                PSZ         pszFavorites;

                PrfQueryProfileString(HINI_USERPROFILE,
                                      INIAPP_XWORKPLACE, (PSZ)pcszIniKey,
                                      "", &szFavorites, sizeof(szFavorites));
                pszFavorites = szFavorites;

                do
                {
                    HOBJECT          hObject;
                    WPFolder         *pFolder2;
                    sscanf(pszFavorites, "%lX %s", &hObject, &szDummy);
                    pFolder2 = _wpclsQueryObject(_WPFolder, hObject);
                    if (pFolder2)
                    {
                        if (wpshCheckObject(pFolder2))
                            if (_somIsA(pFolder2, _WPFolder))
                            {
                                 PCONTENTMENULISTITEM pNew = malloc(sizeof(CONTENTMENULISTITEM));
                                 pNew->pFolder = pFolder2;
                                 lstAppendItem(pllFolders,
                                               pNew);
                            }
                    }
                    pszFavorites += 6;
                } while (strlen(pszFavorites) > 0);
            }

            // OK, now we have a valid list
            pNode = lstQueryFirstNode(pllFolders);

            if (pFolder)
            {
                // folder given as param: look for this folder
                // and return the following in the list
                while (pNode)
                {
                    PCONTENTMENULISTITEM pItem = pNode->pItemData;
                    if (pItem->pFolder == pFolder)
                    {
                        pNode = pNode->pNext;
                        break;
                    }

                    pNode = pNode->pNext;
                }
            } // else: pNode still points to first

            if (pNode)
                pItemFound = pNode->pItemData;
        }
    }
    CATCH(excpt1) { } END_CATCH();

    if (fSemOwned)
    {
        DosReleaseMutexSem(hmtxFolderLists);
        fSemOwned = FALSE;
    }

    if (pItemFound)
        return (pItemFound->pFolder);
    else
        return (NULL);

}

/* ******************************************************************
 *                                                                  *
 *   Folder status bars                                             *
 *                                                                  *
 ********************************************************************/

/*
 *@@ fdrCreateStatusBar:
 *      depending on fShow, this creates or destroys the folder status
 *      bar for a certain folder.
 *
 *      This gets called in the following situations:
 *      a)   from XFolder::wpOpen, when a folder view is created
 *           which needs a status bar;
 *      b)   later if status bar visibility settings are changed
 *           either for the folder or globally.
 *
 *      Parameters:
 *      --  psli:      pointer to SUBCLASSEDLISTITEM structure of
 *                     current folder frame; this contains the
 *                     folder frame window handle
 *      --  fShow:     create / destroy flag
 *
 *      This func returns the hwnd of the status bar or NULL if calling
 *      the func was useless, because the status bar was already
 *      (de)activated.
 *
 *      The status bar always has the ID 0x9001 so you can get the
 *      status bar HWND later by calling
 +          WinQueryWindow(hwndFrame, 0x9001)
 *      If this returns NULLHANDLE, then there is no status bar.
 *
 *      Note that this does _not_ change the folder's visibility
 *      settings, but only shows or hides the status bar "factually"
 *      by reformatting the folder frame's PM windows.
 *
 *      This function _must_ be called only from the same thread
 *      where the folder frame window is running (normally TID 1),
 *      otherwise the WPS will crash or PM will hang.
 *
 *      Because of all this, call XFolder::xwpSetStatusBarVisibility
 *      instead if you wish to change folder status bars. That method
 *      will do the display also.
 *
 *@@changed V0.9.0 [umoeller]: this used to be an instance method (xfldr.c)
 */

HWND fdrCreateStatusBar(WPFolder *somSelf,
                        PSUBCLASSEDLISTITEM psli2,
                        BOOL fShow)
{
    HWND hrc = NULLHANDLE;

    CHAR    szFolderPosKey[50];
    ULONG   ulIni;
    ULONG   cbIni;

    // XFolderData *somThis = XFolderGetData(somSelf);

    #ifdef DEBUG_STATUSBARS
        _Pmpf(("xfCreateStatusBar %d", fShow));
    #endif

    if (psli2)
    {
        if (fShow)
        {
            // show status bar

            if (psli2->hwndStatusBar) // already activated: update only
                WinPostMsg(psli2->hwndStatusBar, STBM_UPDATESTATUSBAR, MPNULL, MPNULL);
                // and quit
            // else create status bar as a static control
            // (which will be subclassed below)
            else if (psli2->hwndStatusBar
                        = WinCreateWindow(psli2->hwndFrame,        // parent
                                          WC_STATIC,              // wnd class
                                          (cmnQueryNLSStrings())->pszPopulating, // title
                                          (SS_TEXT | DT_LEFT | DT_VCENTER // wnd style flags
                                              | DT_ERASERECT
                                              | WS_VISIBLE | WS_CLIPSIBLINGS),
                                          0L, 0L, -1L, -1L,
                                          psli2->hwndFrame,        // owner
                                          HWND_BOTTOM,
                                          0x9001,                 // ID
                                          (PVOID)NULL,
                                          (PVOID)NULL))
            {
                BOOL    fInflate = FALSE;
                SWP     swp;
                const char* pszStatusBarFont = cmnQueryStatusBarSetting(SBS_STATUSBARFONT);

                // set up window data (QWL_USER) for status bar
                PSTATUSBARDATA psbd = malloc(sizeof(STATUSBARDATA));
                psbd->somSelf    = somSelf;
                psbd->psli       = psli2;
                psbd->idTimer    = 0;
                psbd->fDontBroadcast = TRUE;
                            // prevents broadcasting of WM_PRESPARAMSCHANGED
                psbd->fFolderPopulated = FALSE;
                            // suspends updating until folder is populated;
                WinSetWindowULong(psli2->hwndStatusBar, QWL_USER, (ULONG)psbd);

                // subclass static control to make it a status bar
                psbd->pfnwpStatusBarOriginal = WinSubclassWindow(psli2->hwndStatusBar,
                                                                 fdr_fnwpStatusBar);
                WinSetPresParam(psli2->hwndStatusBar,
                                PP_FONTNAMESIZE,
                                (ULONG)strlen(pszStatusBarFont) + 1,
                                (PVOID)pszStatusBarFont);

                // now "inflate" the folder frame window if this is the first
                // time that this folder view has been opened with a status bar;
                // if we didn't do this, the folder frame would be too small
                // for the status bar and scroll bars would appear. We store
                // a flag in the folder's instance data for each different view
                // that we've inflated the folder frame, so that this happens
                // only once per view.

                // From wpobject.h:
                // #define VIEW_CONTENTS      0x00000001
                // #define VIEW_SETTINGS      0x00000002
                // #define VIEW_HELP          0x00000004
                // #define VIEW_RUNNING       0x00000008
                // #define VIEW_DETAILS       0x00000010
                // #define VIEW_TREE          0x00000020

                sprintf(szFolderPosKey, "%d@XFSB",
                        _wpQueryHandle(psli2->pRealObject));
                cbIni = sizeof(ulIni);
                if (PrfQueryProfileData(HINI_USER,
                                        WPINIAPP_FOLDERPOS,
                                        szFolderPosKey,
                                        &ulIni,
                                        &cbIni) == FALSE)
                    ulIni = 0;

                if (ulIni & (   (psli2->ulView == OPEN_CONTENTS) ? VIEW_CONTENTS
                              : (psli2->ulView == OPEN_TREE) ? VIEW_TREE
                              : VIEW_DETAILS
                            ))
                    fInflate = FALSE;
                else
                    fInflate = TRUE;

                #ifdef DEBUG_STATUSBARS
                    _Pmpf(("xfCreateStatusBar, fInflate: %d", fInflate));
                #endif

                // set a flag for the subclassed folder frame
                // window proc that this folder view needs no additional scrolling
                // (this is evaluated in WM_FORMATFRAME msgs)
                psli2->fNeedCnrScroll = FALSE;

                if (fInflate)
                {
                    // this view has not been inflated yet:
                    // inflate now and set flag for this view

                    ULONG ulStatusBarHeight = cmnQueryStatusBarHeight();
                    WinQueryWindowPos(psli2->hwndFrame, &swp);
                    // inflate folder frame
                    WinSetWindowPos(psli2->hwndFrame, 0,
                                    swp.x, (swp.y - ulStatusBarHeight),
                                    swp.cx, (swp.cy + ulStatusBarHeight),
                                    SWP_MOVE | SWP_SIZE);

                    // mark this folder view as "inflated" in OS2.INI
                    ulIni |= (   (psli2->ulView == OPEN_CONTENTS) ? VIEW_CONTENTS
                               : (psli2->ulView == OPEN_TREE) ? VIEW_TREE
                               : VIEW_DETAILS
                             );
                    PrfWriteProfileData(HINI_USER,
                                        WPINIAPP_FOLDERPOS,
                                        szFolderPosKey,
                                        &ulIni,
                                        sizeof(ulIni));
                }
                else
                    // if the folder frame has been inflated already,
                    // WM_FORMATFRAME _will_ have to scroll the container.
                    // We do this for icon views only.
                    if (psli2->ulView == OPEN_CONTENTS)
                        psli2->fNeedCnrScroll = TRUE;

                // _Pmpf(("  psli2->fNeedCnrScroll: %d", psli2->fNeedCnrScroll));

                // enforce reformatting / repaint of frame window
                WinSendMsg(psli2->hwndFrame, WM_UPDATEFRAME, (MPARAM)0, MPNULL);

                // update status bar contents
                WinPostMsg(psli2->hwndStatusBar, STBM_UPDATESTATUSBAR, MPNULL, MPNULL);

                hrc = psli2->hwndStatusBar;
            }
        }
        else
        {
            // hide status bar:
            if (psli2->hwndStatusBar)
            {
                SWP     swp;
                HWND    hwndStatus = psli2->hwndStatusBar;
                BOOL    fDeflate = FALSE;

                psli2->hwndStatusBar = 0;
                WinSendMsg(psli2->hwndFrame, WM_UPDATEFRAME, (MPARAM)0, MPNULL);
                WinDestroyWindow(hwndStatus);

                // decrease the size of the frame window by the status bar height,
                // if we did this before
                sprintf(szFolderPosKey, "%d@XFSB",
                        _wpQueryHandle(psli2->pRealObject));
                cbIni = sizeof(ulIni);
                if (PrfQueryProfileData(HINI_USER,
                                        WPINIAPP_FOLDERPOS,     // "PM_Workplace:FolderPos"
                                        szFolderPosKey,
                                        &ulIni,
                                        &cbIni) == FALSE)
                    ulIni = 0;

                if (ulIni & (   (psli2->ulView == OPEN_CONTENTS) ? VIEW_CONTENTS
                              : (psli2->ulView == OPEN_TREE) ? VIEW_TREE
                              : VIEW_DETAILS
                            ))
                    fDeflate = TRUE;
                else
                    fDeflate = TRUE;

                if (fDeflate)
                {
                    ULONG ulStatusBarHeight = cmnQueryStatusBarHeight();
                    WinQueryWindowPos(psli2->hwndFrame, &swp);
                    WinSetWindowPos(psli2->hwndFrame, 0,
                                    swp.x, (swp.y + ulStatusBarHeight),
                                    swp.cx, (swp.cy - ulStatusBarHeight),
                                    SWP_MOVE | SWP_SIZE);

                    ulIni &= ~(   (psli2->ulView == OPEN_CONTENTS) ? VIEW_CONTENTS
                                : (psli2->ulView == OPEN_TREE) ? VIEW_TREE
                                : VIEW_DETAILS
                              );
                    PrfWriteProfileData(HINI_USER,
                                        WPINIAPP_FOLDERPOS,     // "PM_Workplace:FolderPos"
                                        szFolderPosKey,
                                        &ulIni,
                                        sizeof(ulIni));
                }

                hrc = psli2->hwndStatusBar;
            }
        }
    }

    return (hrc);
}

/* ******************************************************************
 *                                                                  *
 *   XFolder window procedures                                      *
 *                                                                  *
 ********************************************************************/

/*
 *@@ fdr_fnwpStatusBar:
 *      since the status bar is just created as a static frame
 *      control, it is subclassed (by fdrCreateStatusBar),
 *      and this is the wnd proc for this.
 *      This handles the new STBM_UPDATESTATUSBAR message (which
 *      is posted any time the status bar needs to be updated)
 *      and intercepts WM_TIMER and WM_PAINT.
 *
 *@@changed V0.9.0 [umoeller]: fixed context menu problem (now open view)
 *@@changed V0.9.0 [umoeller]: now using new XFolder methods to encapsulate status bar updates
 *@@changed V0.9.0 [umoeller]: adjusted to new gpihDraw3DFrame
 *@@changed V0.9.0 [umoeller]: moved this func here from xfldr.c
 *@@changed V0.9.1 (99-12-19) [umoeller]: finally fixed the context menu problems with MB2 right-click on status bar
 */

MRESULT EXPENTRY fdr_fnwpStatusBar(HWND hwndBar, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    PSTATUSBARDATA psbd = (PSTATUSBARDATA)WinQueryWindowULong(hwndBar, QWL_USER);
    MRESULT        mrc = 0;

    if (psbd)
    {
        PFNWP      pfnwpStatusBarOriginal = psbd->pfnwpStatusBarOriginal;

        switch(msg)
        {
            /*
             * STBM_UPDATESTATUSBAR:
             *      mp1: MPNULL,
             *      mp2: MPNULL
             *      Update status bar text. We will set a timer
             *      for a short delay to filter out repetitive
             *      messages here.
             *      This timer is "one-shot" in that it will be
             *      started here and stopped as soon as WM_TIMER
             *      is received.
             */

            case STBM_UPDATESTATUSBAR:
                if (psbd->idTimer == 0)
                {
                    // only if timer is not yet running: start it now
                    psbd->idTimer = WinStartTimer(WinQueryAnchorBlock(hwndBar),
                                                  hwndBar,
                                                  1,
                                                  100); // delay: 100 ms
                }
            break;

            /*
             * WM_TIMER:
             *      a timer is started by STBM_UPDATESTATUSBAR
             *      to avoid flickering;
             *      we now compose the actual text to be displayed
             */

            case WM_TIMER:
            {
                TRY_LOUD(excpt1, NULL)
                {
                    XFolderData *somThis = XFolderGetData(psbd->somSelf);

                    // stop timer (it's just for one shot)
                    WinStopTimer(WinQueryAnchorBlock(hwndBar), hwndBar, 1);
                    psbd->idTimer = 0;

                    // if we're not fully populated yet, start timer again and quit;
                    // otherwise we would display false information.
                    // We need an extra flag in the status bar data because the
                    // FOI_POPULATEDWITHALL is reset to 0 by the WPS for some reason
                    // when an object is deleted from an open folder, and no further
                    // status bar updates would occur then
                    if (psbd->fFolderPopulated == FALSE)
                    {
                        ULONG ulFlags = _wpQueryFldrFlags(psbd->somSelf);
                        #ifdef DEBUG_STATUSBARS
                            _Pmpf(( "  Folder flags: %lX", ulFlags ));
                            _Pmpf(( "  View: %s",
                                    (psbd->psli->ulView == OPEN_TREE) ? "Tree"
                                    : (psbd->psli->ulView == OPEN_CONTENTS) ? "Content"
                                    : (psbd->psli->ulView == OPEN_DETAILS) ? "Details"
                                    : "unknown"
                                 ));
                        #endif

                        // for tree views, check if folder is populated with folders;
                        // otherwise check for populated with all
                        if (    (   (psbd->psli->ulView == OPEN_TREE)
                                 && ((ulFlags & FOI_POPULATEDWITHFOLDERS) !=0)
                                )
                            || ((ulFlags & FOI_POPULATEDWITHALL) != 0)
                           )
                        {
                            psbd->fFolderPopulated = TRUE;
                        }
                        else
                        {
                            // folder not yet populated:
                            // restart timer with a lower frequency
                            // to have this checked again
                            psbd->idTimer = WinStartTimer(WinQueryAnchorBlock(hwndBar),
                                    hwndBar,
                                    1,
                                    300);   // this time, use 300 ms
                            // and stop
                            break;
                        }
                    }

                    // OK:
                    // do SOM name-lookup resolution for xwpUpdateStatusBar
                    // (which will per default find the XFolder method, V0.9.0)
                    // if we have not done that for this instance already;
                    // we store the method pointer in the instance data
                    // for speed
                    if (_pfnUpdateStatusBar == NULL)
                        // not yet resolved:
                        _pfnUpdateStatusBar = (PVOID)somResolveByName(psbd->somSelf,
                                                                      "xwpUpdateStatusBar");
                    // and compose the text
                    if (_pfnUpdateStatusBar)
                        ((somTD_XFolder_xwpUpdateStatusBar)_pfnUpdateStatusBar)
                                            (psbd->somSelf,
                                             hwndBar,
                                             psbd->psli->hwndCnr);
                    else
                        WinSetWindowText(hwndBar,
                                         "*** error in name-lookup resolution");
                }
                CATCH(excpt1)
                {
                    WinSetWindowText(hwndBar, "*** error composing text");
                } END_CATCH();
            break; }

            /*
             * WM_PAINT:
             *      this, well, paints the status bar.
             *      Since the status bar is just a static control,
             *      we want to do some extra stuff to make it prettier.
             *      At this point, the window text (of the status bar)
             *      contains the fully translated status bar mnemonics,
             *      except for the tabulators ("$x" flags), which we'll
             *      deal with here.
             */

            case WM_PAINT:
            {
                // preparations:
                RECTL   rclBar, rcl;
                HPS     hps = WinBeginPaint(hwndBar, NULLHANDLE, &rcl);
                PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

                TRY_LOUD(excpt1, NULL)
                {
                    POINTL  ptl1;
                    CHAR    szText[1000], szTemp[100] = "0";
                    USHORT  usLength;
                    LONG    lNextX;
                    PSZ     p1, p2, p3;
                    LONG    lHiColor = WinQuerySysColor(HWND_DESKTOP, SYSCLR_BUTTONLIGHT, 0),
                            lLoColor = WinQuerySysColor(HWND_DESKTOP, SYSCLR_BUTTONDARK, 0);

                    WinQueryWindowRect(hwndBar, &rclBar);
                    // switch to RGB mode
                    gpihSwitchToRGB(hps);

                    // 1) draw background
                    WinFillRect(hps, &rclBar, pGlobalSettings->lSBBgndColor);

                    // 2) draw 3D frame in selected style
                    if (pGlobalSettings->SBStyle == SBSTYLE_WARP3RAISED)
                        // Warp 3 style, raised
                        gpihDraw3DFrame(hps, &rclBar, 1,
                                        lHiColor, lLoColor);
                    else if (pGlobalSettings->SBStyle == SBSTYLE_WARP3SUNKEN)
                        // Warp 3 style, sunken
                        gpihDraw3DFrame(hps, &rclBar, 1,
                                        lLoColor, lHiColor);
                    else if (pGlobalSettings->SBStyle == SBSTYLE_WARP4MENU)
                    {
                        // Warp 4 menu style: draw 3D line at top only
                        rcl = rclBar;
                        rcl.yBottom = rcl.yTop-2;
                        gpihDraw3DFrame(hps, &rcl, 1,
                                        lLoColor, lHiColor);
                    }
                    else
                    {
                        // Warp 4 button style
                        rcl = rclBar;
                        // draw "sunken" outer rect
                        gpihDraw3DFrame(hps, &rclBar, 2,
                                        lLoColor, lHiColor);
                        // draw "raised" inner rect
                        WinInflateRect(WinQueryAnchorBlock(hwndBar),
                                &rcl, -1, -1);
                        gpihDraw3DFrame(hps, &rcl, 2,
                                        lHiColor, lLoColor);
                    }

                    // 3) start working on text; we do "simple" GpiCharString
                    //    if no tabulators are defined, but calculate some
                    //    subrectangles otherwise
                    WinQueryWindowText(hwndBar, sizeof(szText)-1, &szText[0]);
                            // szText now has the translated status bar text
                            // except for the tabulators ("$x" keys)
                    p1 = szText;
                    p2 = NULL;
                    ptl1.x = 7;

                    do {    // while tabulators are present

                        // search for tab mnemonic
                        if (p2 = strstr(p1, "$x("))
                        {
                            // tab found: calculate next x position into lNextX
                            usLength = (p2-p1);
                            strcpy(szTemp, "100");
                            if (p3 = strchr(p2, ')'))
                            {
                                PSZ p4 = strchr(p2, '%');
                                strncpy(szTemp, p2+3, p3-p2-3);
                                // get the parameter
                                sscanf(szTemp, "%d", &lNextX);

                                if (lNextX < 0)
                                {
                                    // if the result is negative, it's probably
                                    // meant to be an offset from the right
                                    // status bar border
                                    lNextX = (rclBar.xRight + lNextX); // lNextX is negative
                                }
                                else if ((p4) && (p4 < p3))
                                {
                                    // if we have a '%' char before the closing
                                    // bracket, consider lNextX a percentage
                                    // parameter and now translate it into an
                                    // absolute x position using the status bar's
                                    // width
                                    lNextX = (rclBar.xRight * lNextX) / 100;
                                }
                            } else
                                p2 = NULL;
                        } else
                            usLength = strlen(p1);

                        ptl1.y = (pGlobalSettings->SBStyle == SBSTYLE_WARP4MENU) ? 5 : 7;
                        // set the text color to the global value;
                        // this might have changed via color drag'n'drop
                        GpiSetColor(hps, pGlobalSettings->lSBTextColor);
                            // the font is already preset by the static
                            // text control (phhhh...)

                        if (p2)
                        {
                            // did we have tabs? if so, print text clipped to rect
                            rcl.xLeft   = 0;
                            rcl.yBottom = 0; // ptl1.y;
                            rcl.xRight  = lNextX-10; // 10 pels space
                            rcl.yTop    = rclBar.yTop;
                            GpiCharStringPosAt(hps, &ptl1, &rcl, CHS_CLIP,
                                    usLength, p1, NULL);
                        }
                        else
                        {
                            // no (more) tabs: just print
                            GpiMove(hps, &ptl1);
                            GpiCharString(hps, usLength, p1);
                        }

                        if (p2)
                        {   // "tabulator" was found: set next x pos
                            ptl1.x = lNextX;
                            p1 = p3+1;
                        }

                    } while (p2);       // go for next tabulator, if we had one

                }
                CATCH(excpt1)
                {
                    PSZ pszErr = "*** error painting status bar";
                    POINTL ptl = {5, 5};
                    GpiMove(hps, &ptl);
                    GpiCharString(hps, strlen(pszErr), pszErr);
                } END_CATCH();

                WinEndPaint(hps);

            break; }

            /*
             * STBM_PROHIBITBROADCASTING:
             *      this msg is sent to us to tell us we will
             *      be given new presentation parameters soon;
             *      we will set a flag to TRUE in order to
             *      prevent broadcasting the pres params again.
             *      We need this flag because there are two
             *      situations in which we are given
             *      WM_PRESPARAMCHANGED messages:
             *      1)   the user has dropped something on this
             *           status bar window; in this case, we're
             *           getting WM_PRESPARAMCHANGED directly,
             *           without STBM_PROHIBITBROADCASTING, which
             *           means that we should notify the Worker
             *           thread to broadcast WM_PRESPARAMCHANGED
             *           to all status bars;
             *      2)   a status bar other than this one had been
             *           dropped something upon; in this case, we'll
             *           get WM_PRESPARAMCHANGED also (from the
             *           Worker thread), but STBM_PROHIBITBROADCASTING
             *           beforehand so that we know we should not
             *           notify the Worker thread again.
             *      See WM_PRESPARAMCHANGED below.
             */

            case STBM_PROHIBITBROADCASTING:
            {
                psbd->fDontBroadcast = TRUE;
            break; }

            /*
             * WM_PRESPARAMCHANGED:
             *      if fonts or colors were dropped on the bar, update
             *      GlobalSettings and have this message broadcast to all
             *      other status bars on the system.
             *      This is also posted to us by the Worker thread
             *      after some OTHER status bar has been dropped
             *      fonts or colors upon; in this case, psbd->fDontBroadcast
             *      is TRUE (see above), and we will only update our
             *      own display and NOT broadcast, because this is
             *      already being done.
             */

            case WM_PRESPARAMCHANGED:
            {
                mrc = (MRESULT)(*pfnwpStatusBarOriginal)(hwndBar, msg, mp1, mp2);

                if (psbd->fDontBroadcast) {
                    // this flag has been set if it was not this status
                    // bar whose presparams have changed, but some other
                    // status bar; in this case, update only
                    psbd->fDontBroadcast = FALSE;
                    // update parent's frame controls (because font size
                    // might have changed)
                    WinSendMsg(WinQueryWindow(hwndBar, QW_PARENT), WM_UPDATEFRAME, MPNULL, MPNULL);
                    // update ourselves
                    WinPostMsg(hwndBar, STBM_UPDATESTATUSBAR, MPNULL, MPNULL);
                    // and quit
                    break;
                }

                // else: it was us that the presparam has been set for
                #ifdef DEBUG_STATUSBARS
                    _Pmpf(( "WM_PRESPARAMCHANGED: %lX", mp1 ));
                #endif

                // now check what has changed
                switch ((ULONG)mp1)
                {
                    case PP_FONTNAMESIZE:
                    {
                        ULONG attrFound;
                        // CHAR  szDummy[200];
                        CHAR  szNewFont[100];
                        WinQueryPresParam(hwndBar,
                                          PP_FONTNAMESIZE,
                                          0,
                                          &attrFound,
                                          (ULONG)sizeof(szNewFont),
                                          (PVOID)&szNewFont,
                                          0);
                        cmnSetStatusBarSetting(SBS_STATUSBARFONT,
                                               szNewFont);
                        // update parent's frame controls (because font size
                        // might have changed)
                        WinSendMsg(WinQueryWindow(hwndBar, QW_PARENT),
                                WM_UPDATEFRAME, MPNULL, MPNULL);
                    break; }

                    case PP_FOREGROUNDCOLOR:
                    case PP_BACKGROUNDCOLOR:
                    {
                        ULONG   ul = 0,
                                attrFound = 0;
                        GLOBALSETTINGS* pGlobalSettings = cmnLockGlobalSettings(5000);

                        WinQueryPresParam(hwndBar,
                                          (ULONG)mp1,
                                          0,
                                          &attrFound,
                                          (ULONG)sizeof(ul),
                                          (PVOID)&ul,
                                          0);
                        if ((ULONG)mp1 == PP_FOREGROUNDCOLOR)
                            pGlobalSettings->lSBTextColor = ul;
                        else
                            pGlobalSettings->lSBBgndColor = ul;

                        cmnStoreGlobalSettings();

                        cmnUnlockGlobalSettings();

                        WinPostMsg(hwndBar, STBM_UPDATESTATUSBAR, MPNULL, MPNULL);
                    break; }
                }

                // finally, broadcast this message to all other status bars;
                // this is handled by the Worker thread
                xthrPostWorkerMsg(WOM_UPDATEALLSTATUSBARS,
                        (MPARAM)2,      // update display
                        MPNULL);
            break; }

            /*
             * WM_BUTTON1CLICK:
             *      if button1 is clicked on the status
             *      bar, we should reset the focus to the
             *      container.
             */

            case WM_BUTTON1CLICK:
                // mrc = (MRESULT)(*pfnwpStatusBarOriginal)(hwndBar, msg, mp1, mp2);
                WinSetFocus(HWND_DESKTOP, psbd->psli->hwndCnr);
            break;

            /*
             * WM_BUTTON1DBLCLK:
             *      on double-click on the status bar,
             *      open the folder's settings notebook.
             *      If DEBUG_RESTOREDATA is on (common.h),
             *      dump some internal folder data to
             *      PMPRINTF instead.
             */

            case WM_BUTTON1DBLCLK:
            {
                WinPostMsg(psbd->psli->hwndFrame,
                           WM_COMMAND,
                           (MPARAM)WPMENUID_PROPERTIES,
                           MPFROM2SHORT(CMDSRC_MENU,
                                   FALSE) );     // results from keyboard operation
            break; }

            /*
             * WM_CONTEXTMENU:
             *      if the user right-clicks on status bar,
             *      display folder's context menu. Parameters:
             *      mp1:
             *          POINTS mp1      pointer position, win coords
             *      mp2:
             *          USHORT usReserved  should be 0
             *          USHORT fPointer    if TRUE: results from keyboard
             */

            case WM_CONTEXTMENU:
            {
                if (psbd->psli)
                {
                    POINTL  ptl;

                    ptl.x = SHORT1FROMMP(mp1);
                    ptl.y = SHORT2FROMMP(mp1)-20; // this is in cnr coords!
                    _wpDisplayMenu(psbd->somSelf,
                                   psbd->psli->hwndFrame, // hwndFolderView, // owner
                                   psbd->psli->hwndCnr, // NULLHANDLE, // hwndFolderView, // parent
                                   &ptl,
                                   MENU_OPENVIEWPOPUP, // was: MENU_OBJECTPOPUP, V0.9.0
                                   0);
                }
            break; }

            /*
             * WM_DESTROY:
             *      call original wnd proc, then free psbd
             */

            case WM_DESTROY:
                mrc = (MRESULT)(*pfnwpStatusBarOriginal)(hwndBar, msg, mp1, mp2);
                WinSetWindowULong(hwndBar, QWL_USER, 0);
                free(psbd);
            break;

            default:
                mrc = (MRESULT)(*pfnwpStatusBarOriginal)(hwndBar, msg, mp1, mp2);
            break;
        } // end switch
    } // end if (psbd)
    /* else // error: call default
        mrc = (MRESULT)(*pfnwpStatusBarOriginal)(hwndBar, msg, mp1, mp2); */

    return (mrc);
}

/*
 *@@ fdr_fnwpSubclFolderContentMenu:
 *      this is the subclassed wnd proc for folder content menus;
 *      we need to intercept mouse button 2 msgs to open a folder
 *      (WarpCenter behavior).
 *
 *      This function resides in folder.c because we need access
 *      to some folder globals. Subclassing happens in menus.c though.
 *
 *@@changed V0.9.0 [umoeller]: moved this func here from xfldr.c
 */

MRESULT EXPENTRY fdr_fnwpSubclFolderContentMenu(HWND hwndMenu, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    // PCKERNELGLOBALS pKernelGlobals = krnQueryGlobals();
    // ULONG       *pulWorkplaceFunc2 = &(pKernelGlobals->ulWorkplaceFunc2);
    // ULONG       ulOldWorkplaceFunc2 = *pulWorkplaceFunc2;
    MRESULT     mrc = 0;

    TRY_LOUD(excpt1, NULL)
    {
        USHORT  sSelected;
        POINTL  ptlMouse;
        RECTL   rtlItem;

        switch(msg)
        {
            case WM_ADJUSTWINDOWPOS:
            {
                PSWP pswp = (PSWP)mp1;
                BOOL fAdjusted = FALSE;
                #ifdef DEBUG_MENUS
                    _Pmpf(("WM_ADJUSTWINDOWPOS"));
                #endif

                if ((pswp->fl & (SWP_MOVE)) == (SWP_MOVE))
                {
                    #ifdef DEBUG_MENUS
                        _Pmpf(("  SWP_MOVE set"));
                    #endif

                    if (!fFldrContentMenuMoved)
                    {
                        #ifdef DEBUG_MENUS
                            _Pmpf(("    Checking bounds"));
                        #endif
                        if ((pswp->x + pswp->cx)
                               > WinQuerySysValue(HWND_DESKTOP, SV_CXSCREEN))
                        {
                            pswp->x = 0;
                            #ifdef DEBUG_MENUS
                                _Pmpf(("    Changed x pos"));
                            #endif
                            // avoid several changes for this menu;
                            // this flag is reset by WM_INITMENU in
                            // fdr_fnwpSubclassedFolderFrame
                            fFldrContentMenuMoved = TRUE;
                            fAdjusted = TRUE;
                        }
                        if ((pswp->y + pswp->cy) >
                                WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN))
                        {
                            pswp->y = 0;
                            #ifdef DEBUG_MENUS
                                _Pmpf(("    Changed y pos"));
                            #endif
                            // avoid several changes for this menu;
                            // this flag is reset by WM_INITMENU in
                            // fdr_fnwpSubclassedFolderFrame
                            fFldrContentMenuMoved = TRUE;
                            fAdjusted = TRUE;
                        }
                    }
                }
                if (fAdjusted)
                    pswp->fl |= (SWP_NOADJUST);
                mrc = (MRESULT)(*pfnwpFolderContentMenuOriginal)(hwndMenu, msg, mp1, mp2);
                fFldrContentMenuButtonDown = FALSE;
            break; }

            #ifdef DEBUG_MENUS
                case MM_SELECTITEM:
                {
                    _Pmpf(( "MM_SELECTITEM: mp1 = %lX/%lX, mp2 = %lX",
                        SHORT1FROMMP(mp1),
                        SHORT2FROMMP(mp1),
                        mp2 ));
                    mrc = (MRESULT)(*pfnwpFolderContentMenuOriginal)(hwndMenu, msg, mp1, mp2);
                break; }
            #endif

            case WM_BUTTON2DOWN:
                #ifdef DEBUG_MENUS
                    _Pmpf(("WM_BUTTON2DOWN"));
                #endif

                ptlMouse.x = SHORT1FROMMP(mp1);
                ptlMouse.y = SHORT2FROMMP(mp1);
                WinSendMsg(hwndMenu, MM_SELECTITEM,
                           MPFROM2SHORT(MIT_NONE, FALSE),
                           MPFROM2SHORT(0, FALSE));
                sSelected = winhQueryItemUnderMouse(hwndMenu, &ptlMouse, &rtlItem);
                WinSendMsg(hwndMenu, MM_SETITEMATTR,
                           MPFROM2SHORT(sSelected,
                                        FALSE),
                           MPFROM2SHORT(MIA_HILITED,
                                        MIA_HILITED)
                    );
            break;

            case WM_BUTTON1DOWN:
                // let this be handled by the default proc
                #ifdef DEBUG_MENUS
                    _Pmpf(("WM_BUTTON1DOWN"));
                #endif
                fFldrContentMenuButtonDown = TRUE;
                mrc = (MRESULT)(*pfnwpFolderContentMenuOriginal)(hwndMenu, msg, mp1, mp2);
            break;

            case WM_BUTTON1DBLCLK:
            case WM_BUTTON2UP:
            {
                // upon receiving these, we will open the object directly; we need to
                // cheat a little bit because sending MM_SELECTITEM would open the submenu
                #ifdef DEBUG_MENUS
                    _Pmpf(("WM_BUTTON2UP"));
                #endif
                fFldrContentMenuButtonDown = TRUE;
                ptlMouse.x = SHORT1FROMMP(mp1);
                ptlMouse.y = SHORT2FROMMP(mp1);
                WinSendMsg(hwndMenu, MM_SELECTITEM,
                           MPFROM2SHORT(MIT_NONE, FALSE),
                           MPFROM2SHORT(0, FALSE));
                sSelected = winhQueryItemUnderMouse(hwndMenu, &ptlMouse, &rtlItem);

                xthrPlaySystemSound(MMSOUND_XFLD_CTXTSELECT);

                WinPostMsg(WinQueryWindow(hwndMenu, QW_OWNER),
                           WM_COMMAND,
                           (MPARAM)sSelected,
                           MPFROM2SHORT(CMDSRC_MENU, FALSE));
            break; }

            default:
                mrc = (MRESULT)(*pfnwpFolderContentMenuOriginal)(hwndMenu, msg, mp1, mp2);
            break;
        } // end switch
    }
    CATCH(excpt1)
    {
        // exception occured:
        return (MRESULT)0; // keep compiler happy
    } END_CATCH();

    return (mrc);
}

/*
 *@@ fdr_fnwpSelectSome:
 *      dlg proc for "Select by name" window.
 *
 *      This selects or deselects items in the corresponding
 *      folder window, which is stored in QWL_USER.
 *
 *@@changed V0.9.0 [umoeller]: added drop-down box with history
 *@@changed V0.9.0 [umoeller]: moved this func here from xfldr.c
 *@@changed V0.9.1 (2000-02-01) [umoeller]: WinDestroyWindow was never called. Fixed.
 */

MRESULT EXPENTRY fdr_fnwpSelectSome(HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = MPNULL;
    BOOL    fWriteAndClose = FALSE;

    switch (msg)
    {
        /*
         * WM_INITDLG:
         *
         */

        case WM_INITDLG:
        {
            CHAR    szTitle[CCHMAXPATH];
            PSZ     pszLast10 = NULL;
            ULONG   cbLast10 = 0;

            HWND    hwndDropDown = WinWindowFromID(hwndDlg, ID_XFDI_SOME_ENTRYFIELD);

            WinSetWindowULong(hwndDlg, QWL_USER, (ULONG)mp2); // Owner frame hwnd;
            WinQueryWindowText((HWND)mp2,
                               sizeof(szTitle),
                               szTitle);
            WinSetWindowText(hwndDlg, szTitle);

            // load last 10 selections from OS2.INI (V0.9.0)
            pszLast10 = prfhQueryProfileData(HINI_USER,
                                             INIAPP_XWORKPLACE,
                                             INIKEY_LAST10SELECTSOME, // "SelectSome"
                                             &cbLast10);
            if (pszLast10)
            {
                // something found:
                PSZ     p = pszLast10;
                while (p < (pszLast10 + cbLast10))
                {
                    WinInsertLboxItem(hwndDropDown, LIT_END, p);

                    p += strlen(p) + 1; // go beyond null byte
                }

                free(pszLast10);
            }

            // select entire string in drop-down
            winhEntryFieldSelectAll(hwndDropDown);

            mrc = fnwpDlgGeneric(hwndDlg, msg, mp1, mp2);
        break; }

        /*
         * WM_COMMAND:
         *
         */

        case WM_COMMAND:
        {
            switch (SHORT1FROMMP(mp1))
            {
                /*
                 * ID_XFDI_SOME_SELECT / DESELECT:
                 *      these are the "select" / "deselect" buttons
                 */

                case ID_XFDI_SOME_SELECT:
                case ID_XFDI_SOME_DESELECT:
                {
                    CHAR szMask[CCHMAXPATH];

                    HWND hwndFrame = WinQueryWindowULong(hwndDlg, QWL_USER);

                    if (hwndFrame)
                    {
                        HWND    hwndDropDown = WinWindowFromID(hwndDlg, ID_XFDI_SOME_ENTRYFIELD);
                        HWND    hwndCnr = wpshQueryCnrFromFrame(hwndFrame);

                        if (hwndCnr)
                        {
                            SHORT sLBIndex = 0;

                            WinQueryWindowText(hwndDropDown,
                                               sizeof(szMask),
                                               szMask);

                            if (strlen(szMask))
                            {
                                // now go through all the container items in hwndCnr
                                // and select / deselct them accordingly
                                PMINIRECORDCORE pmrc = NULL;
                                do
                                {
                                    pmrc =
                                        (PMINIRECORDCORE)WinSendMsg(hwndCnr,
                                                                    CM_QUERYRECORD,
                                                                    (MPARAM)pmrc,
                                                                    MPFROM2SHORT(
                                                                        (pmrc)
                                                                            ? CMA_NEXT
                                                                            : CMA_FIRST,
                                                                        CMA_ITEMORDER)
                                                                    );
                                    if (pmrc)
                                    {
                                        if (strhMatchOS2(szMask, pmrc->pszIcon))
                                        {
                                            WinSendMsg(hwndCnr,
                                                       CM_SETRECORDEMPHASIS,
                                                       pmrc,
                                                       MPFROM2SHORT(
                                                           // select or deselect flag
                                                           (SHORT1FROMMP(mp1)
                                                                == ID_XFDI_SOME_SELECT),
                                                           CRA_SELECTED
                                                       ));
                                        }
                                    }
                                } while (pmrc);
                            }

                            WinSetFocus(HWND_DESKTOP, hwndDropDown);
                            // select entire string in drop-down
                            winhEntryFieldSelectAll(hwndDropDown);

                            // add file mask to list box (V0.9.0);
                            // if it exists, delete the old one first
                            sLBIndex = (SHORT)WinSendMsg(hwndDropDown,
                                                         LM_SEARCHSTRING,
                                                         MPFROM2SHORT(0,       // no flags
                                                                      LIT_FIRST),
                                                         szMask);
                            if (sLBIndex != LIT_NONE)
                                // found: remove
                                WinDeleteLboxItem(hwndDropDown,
                                                  sLBIndex);

                            WinInsertLboxItem(hwndDropDown,
                                              0,            // at beginning
                                              szMask);
                        }
                    }
                break; }

                case ID_XFDI_SOME_SELECTALL:
                case ID_XFDI_SOME_DESELECTALL:
                {
                    HWND hwndFrame = WinQueryWindowULong(hwndDlg, QWL_USER);
                    if (hwndFrame)
                    {
                        HWND hwndCnr = wpshQueryCnrFromFrame(hwndFrame);
                        if (hwndCnr)
                        {
                            PMINIRECORDCORE pmrc = NULL;
                            do {
                                pmrc =
                                    (PMINIRECORDCORE)WinSendMsg(hwndCnr,
                                            CM_QUERYRECORD,
                                            (MPARAM)pmrc,
                                            MPFROM2SHORT(
                                                (pmrc) ? CMA_NEXT : CMA_FIRST,
                                                CMA_ITEMORDER)
                                            );
                                if (pmrc)
                                {
                                    WinSendMsg(hwndCnr,
                                        CM_SETRECORDEMPHASIS,
                                        pmrc,
                                        MPFROM2SHORT(
                                            // select or deselect flag
                                            (SHORT1FROMMP(mp1) == ID_XFDI_SOME_SELECTALL),
                                            CRA_SELECTED
                                        ));
                                }
                            } while (pmrc);

                            winhSetDlgItemFocus(hwndDlg, ID_XFDI_SOME_ENTRYFIELD);
                            WinSendDlgItemMsg(hwndDlg, ID_XFDI_SOME_ENTRYFIELD,
                                    EM_SETSEL,
                                    MPFROM2SHORT(0, 1000), // select all
                                    MPNULL);
                        }
                    }
                break; }

                case DID_CANCEL:
                    WinPostMsg(hwndDlg, WM_CLOSE, 0, 0);
                break;

                /*
                 * default:
                 *      this includes "Help"
                 */

                default:
                    mrc = fnwpDlgGeneric(hwndDlg, msg, mp1, mp2);
                break;
            }
        break; }

        /*
         * WM_CLOSE:
         *
         */

        case WM_CLOSE:
            fWriteAndClose = TRUE;
        break;

        default:
            mrc = fnwpDlgGeneric(hwndDlg, msg, mp1, mp2);
    }

    if (fWriteAndClose)
    {
        // dialog closing:
        // write drop-down entries back to OS2.INI (V0.9.0)
        ULONG   ul = 0;
        PSZ     pszToSave = NULL;
        ULONG   cbToSave = 0;
        HWND    hwndDropDown = WinWindowFromID(hwndDlg, ID_XFDI_SOME_ENTRYFIELD);

        for (ul = 0;
             ul < 10;
             ul++)
        {
            CHAR    szEntry[CCHMAXPATH];
            if (WinQueryLboxItemText(hwndDropDown,
                                     ul,
                                     szEntry,
                                     sizeof(szEntry))
                < 1)
                break;

            strhArrayAppend(&pszToSave,
                            szEntry,
                            &cbToSave);
        }

        if (cbToSave)
            PrfWriteProfileData(HINI_USER,
                                INIAPP_XWORKPLACE,
                                INIKEY_LAST10SELECTSOME, // "SelectSome"
                                pszToSave,
                                cbToSave);

        WinDestroyWindow(hwndDlg);
    }

    return (mrc);
}


/*
 * GetICONPOS:
 *
 *
 *@@changed V0.9.0 [umoeller]: moved this func here from xfldr.c
 */

PICONPOS GetICONPOS(PORDEREDLISTITEM poli,
                    PSORTBYICONPOS psip)
{
    PICONPOS                    pip;
    CHAR                        *p;
    USHORT usStartPos = 21;  // OS/2 2.1 and above, says Henk

    // ICONPOS is defined in PMWP.H as folllows:
    //     typedef struct _ICONPOS
    //     {
    //        POINTL  ptlIcon;
    //        CHAR    szIdentity[1];
    //     } ICONPOS;
    //     typedef ICONPOS *PICONPOS;

    // now compare all the objects in the .ICONPOS structure
    // to the identity string of the search object

    for (   pip = (PICONPOS)( psip->pICONPOS + usStartPos );
            (PBYTE)pip < (psip->pICONPOS + psip->usICONPOSSize);
        )
    {   // pip now points to an ICONPOS structure

        // go beyond the class name
        p = strchr(pip->szIdentity, ':');
        if (p) {

            /* #ifdef DEBUG_ORDEREDLIST
                _Pmpf(("      Identities: %s and %s...", p, poli->szIdentity));
            #endif */

            if (stricmp(p, poli->szIdentity) == 0)
                // object found: return the ICONPOS address
                return (pip);
            else
                // not identical: go to next ICONPOS structure
                pip = (PICONPOS)( (PBYTE)pip + sizeof(POINTL) + strlen(pip->szIdentity) + 1 );
        } else
            break;
    }
    return (NULL);
}

/*
 * fdrSortByICONPOS:
 *      callback sort function for lstSort to sort the
 *      menu items according to a folder's ICONPOS EAs.
 *      pICONPOS points to the ICONPOS data.
 *
 *@@changed V0.9.0 [umoeller]: moved this func here from xfldr.c
 */

SHORT EXPENTRY fdrSortByICONPOS(PVOID pItem1, PVOID pItem2, PVOID psip)
{
    /* #ifdef DEBUG_ORDEREDLIST
        _Pmpf(("    Comparing %s and %s...",
            _wpQueryTitle(((PORDEREDLISTITEM)pItem1)->pObj),
            _wpQueryTitle(((PORDEREDLISTITEM)pItem2)->pObj)
        ));
    #endif */
    if ((pItem1) && (pItem2)) {
        PICONPOS pip1 = GetICONPOS(((PORDEREDLISTITEM)pItem1), psip),
                 pip2 = GetICONPOS(((PORDEREDLISTITEM)pItem2), psip);

        if ((pip1) && (pip2))
            if (pip1 < pip2)
                return (-1);
            else return (1);
        else if (pip1)
            return (-1);
        else if (pip2)
            return (1);
    }

    return (0);
}

/* ******************************************************************
 *                                                                  *
 *   Notebook callbacks (notebook.c) for XFldWPS  "View" page       *
 *                                                                  *
 ********************************************************************/

/*
 *@@ fdrViewInitPage:
 *      notebook callback function (notebook.c) for the
 *      "Folder views" page in the "Workplace Shell" object.
 *      Sets the controls on the page according to the
 *      Global Settings.
 *
 *@@changed V0.9.0 [umoeller]: adjusted function prototype, renamed function
 *@@changed V0.9.0 [umoeller]: moved this func here from xfwps.c
 */

VOID fdrViewInitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
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
        /* winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_ADDINTERNALS,
                pGlobalSettings->ShowInternals); */
        /* winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_REPLICONS,
                pGlobalSettings->ReplIcons); */
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_FULLPATH,
                              pGlobalSettings->FullPath);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_KEEPTITLE,
                              pGlobalSettings->KeepTitle);
        // maximum path chars spin button
        winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_XSDI_MAXPATHCHARS,
                               11, 200,        // limits
                               pGlobalSettings->MaxPathChars);  // data
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_TREEVIEWAUTOSCROLL,
                              pGlobalSettings->TreeViewAutoScroll);
    }

    if (flFlags & CBI_ENABLE)
    {
        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_TREEVIEWAUTOSCROLL,
                (    (pGlobalSettings->NoWorkerThread == FALSE)
                  && (pGlobalSettings->NoSubclassing == FALSE)
                ));
    }
}

/*
 *@@ fdrViewItemChanged:
 *      notebook callback function (notebook.c) for the
 *      "Folder views" page in the "Workplace Shell" object.
 *      Reacts to changes of any of the dialog controls.
 *
 *@@changed V0.9.0 [umoeller]: adjusted function prototype
 *@@changed V0.9.0 [umoeller]: moved this func here from xfwps.c
 */

MRESULT fdrViewItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                      USHORT usItemID, USHORT usNotifyCode,
                                      ULONG ulExtra)      // for checkboxes: contains new state
{
    GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(5000);
    MRESULT mrc = (MPARAM)0;
    BOOL fSave = TRUE,
         fUpdate = FALSE;

    // LONG lTemp;

    switch (usItemID)
    {
        /* case ID_XSDI_ADDINTERNALS:
            pGlobalSettings->ShowInternals = ulExtra;
        break; */

        case ID_XSDI_FULLPATH:
            pGlobalSettings->FullPath  = ulExtra;
            fUpdate = TRUE;
        break;

        case ID_XSDI_KEEPTITLE:
            pGlobalSettings->KeepTitle = ulExtra;
            fUpdate = TRUE;
        break;

        case ID_XSDI_TREEVIEWAUTOSCROLL:
            pGlobalSettings->TreeViewAutoScroll = ulExtra;
        break;

        case ID_XSDI_MAXPATHCHARS:  // spinbutton
            pGlobalSettings->MaxPathChars = ulExtra;
            fUpdate = TRUE;
        break;

        case DID_UNDO:
        {
            // "Undo" button: get pointer to backed-up Global Settings
            PCGLOBALSETTINGS pGSBackup = (PCGLOBALSETTINGS)(pcnbp->pUser);

            // and restore the settings for this page
            // pGlobalSettings->ShowInternals = pGSBackup->ShowInternals;
            // pGlobalSettings->ReplIcons = pGSBackup->ReplIcons;
            pGlobalSettings->FullPath  = pGSBackup->FullPath ;
            pGlobalSettings->KeepTitle = pGSBackup->KeepTitle;
            pGlobalSettings->TreeViewAutoScroll = pGSBackup->TreeViewAutoScroll;
            pGlobalSettings->MaxPathChars = pGSBackup->MaxPathChars;

            // update the display by calling the INIT callback
            (*(pcnbp->pfncbInitPage))(pcnbp, CBI_SET | CBI_ENABLE);
            fUpdate = TRUE;
        break; }

        case DID_DEFAULT:
        {
            // set the default settings for this settings page
            // (this is in common.c because it's also used at
            // WPS startup)
            cmnSetDefaultSettings(pcnbp->ulPageID);
            // update the display by calling the INIT callback
            (*(pcnbp->pfncbInitPage))(pcnbp, CBI_SET | CBI_ENABLE);
            fUpdate = TRUE;
        break; }

        default:
            fSave = FALSE;
    }

    cmnUnlockGlobalSettings();

    if (fSave)
        cmnStoreGlobalSettings();

    if (fUpdate)
        // have Worker thread update all open folder windows
        // with the full-path-in-title settings
        xthrPostWorkerMsg(WOM_REFRESHFOLDERVIEWS,
                          (MPARAM)NULL, // update all, not just children
                          MPNULL);

    return (mrc);
}

/* ******************************************************************
 *                                                                  *
 *   Notebook callbacks (notebook.c) for XFldWPS"Grid" page         *
 *                                                                  *
 ********************************************************************/

/*
 *@@ fdrGridInitPage:
 *      notebook callback function (notebook.c) for the
 *      "Snap to grid" page in the "Workplace Shell" object.
 *      Sets the controls on the page according to the
 *      Global Settings.
 *
 *@@changed V0.9.0 [umoeller]: adjusted function prototype
 */

VOID fdrGridInitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
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
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_SNAPTOGRID,
                              pGlobalSettings->fAddSnapToGridDefault);
        winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_XSDI_GRID_X,
                               0, 500,
                               pGlobalSettings->GridX);
        winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_XSDI_GRID_Y,
                               0, 500,
                               pGlobalSettings->GridY);
        winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_XSDI_GRID_CX,
                               1, 500,
                               pGlobalSettings->GridCX);
        winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_XSDI_GRID_CY,
                               1, 500,
                               pGlobalSettings->GridCY);
    }
}

/*
 *@@ fdrGridItemChanged:
 *      notebook callback function (notebook.c) for the
 *      "Snap to grid" page in the "Workplace Shell" object.
 *      Reacts to changes of any of the dialog controls.
 *
 *@@changed V0.9.0 [umoeller]: adjusted function prototype
 */

MRESULT fdrGridItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                    USHORT usItemID,
                                    USHORT usNotifyCode,
                                    ULONG ulExtra)      // for checkboxes: contains new state
{
    GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(5000);
    MRESULT mrc = (MPARAM)0;
    BOOL fSave = TRUE;

    switch (usItemID)
    {
        case ID_XSDI_SNAPTOGRID:
            pGlobalSettings->fAddSnapToGridDefault = ulExtra;
        break;

        case ID_XSDI_GRID_X:
            pGlobalSettings->GridX = ulExtra;
        break;

        case ID_XSDI_GRID_Y:
            pGlobalSettings->GridY = ulExtra;
        break;

        case ID_XSDI_GRID_CX:
            pGlobalSettings->GridCX = ulExtra;
        break;

        case ID_XSDI_GRID_CY:
            pGlobalSettings->GridCY = ulExtra;
        break;

        case DID_UNDO:
        {
            // "Undo" button: get pointer to backed-up Global Settings
            PCGLOBALSETTINGS pGSBackup = (PCGLOBALSETTINGS)(pcnbp->pUser);

            // and restore the settings for this page
            pGlobalSettings->fAddSnapToGridDefault = pGSBackup->fAddSnapToGridDefault;
            pGlobalSettings->GridX = pGSBackup->GridX;
            pGlobalSettings->GridY = pGSBackup->GridY;
            pGlobalSettings->GridCX = pGSBackup->GridCX;
            pGlobalSettings->GridCY = pGSBackup->GridCY;

            // update the display by calling the INIT callback
            (*(pcnbp->pfncbInitPage))(pcnbp, CBI_SET | CBI_ENABLE);
        break; }

        case DID_DEFAULT:
        {
            // set the default settings for this settings page
            // (this is in common.c because it's also used at
            // WPS startup)
            cmnSetDefaultSettings(pcnbp->ulPageID);
            // update the display by calling the INIT callback
            (*(pcnbp->pfncbInitPage))(pcnbp, CBI_SET | CBI_ENABLE);
        break; }

        default:
            fSave = FALSE;
    }

    cmnUnlockGlobalSettings();

    if (fSave)
        cmnStoreGlobalSettings();

    return (mrc);
}

/* ******************************************************************
 *                                                                  *
 *   Notebook callbacks (notebook.c) for "XFolder" instance page    *
 *                                                                  *
 ********************************************************************/

/*
 * fdrXFolderInitPage:
 *      "XFolder" page notebook callback function (notebook.c).
 *      Sets the controls on the page according to a folder's
 *      instance settings.
 *
 *@@changed V0.9.0 [umoeller]: adjusted function prototype
 *@@changed V0.9.0 [umoeller]: moved this func here from xfldr.c
 *@@changed V0.9.1 (99-12-28) [umoeller]: "snap to grid" was enabled even if disabled globally; fixed
 */

VOID fdrXFolderInitPage(PCREATENOTEBOOKPAGE pcnbp,  // notebook info struct
                         ULONG flFlags)              // CBI_* flags (notebook.h)
{
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    XFolderData *somThis = XFolderGetData(pcnbp->somSelf);

    if (flFlags & CBI_INIT)
    {
        if (pcnbp->pUser == NULL)
        {
            // first call: backup instance data for "Undo" button;
            // this memory will be freed automatically by the
            // common notebook window function (notebook.c) when
            // the notebook page is destroyed
            pcnbp->pUser = malloc(sizeof(XFolderData));
            memcpy(pcnbp->pUser, somThis, sizeof(XFolderData));
        }
    }

    if (flFlags & CBI_SET)
    {
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_FAVORITEFOLDER,
                _xwpIsFavoriteFolder(pcnbp->somSelf));

        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_QUICKOPEN,
                _xwpQueryQuickOpen(pcnbp->somSelf));

        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_SNAPTOGRID,
               (MPARAM)( (_bSnapToGridInstance == 2)
                        ? pGlobalSettings->fAddSnapToGridDefault
                        : _bSnapToGridInstance ));
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_FULLPATH,
               (MPARAM)( (_bFullPathInstance == 2)
                        ? pGlobalSettings->FullPath
                        : _bFullPathInstance ));
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_ACCELERATORS,
               (MPARAM)( (_bFolderHotkeysInstance == 2)
                        ? pGlobalSettings->fFolderHotkeysDefault
                        : _bFolderHotkeysInstance ));
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_ENABLESTATUSBAR,
               (MPARAM)( ( (_bStatusBarInstance == STATUSBAR_DEFAULT)
                                    ? pGlobalSettings->fDefaultStatusBarVisibility
                                    : _bStatusBarInstance )
                         // always uncheck for Desktop
                         && (pcnbp->somSelf != _wpclsQueryActiveDesktop(_WPDesktop))
                       ));
    }

    if (flFlags & CBI_ENABLE)
    {
        // disable items
        WinEnableControl(pcnbp->hwndDlgPage,
                         ID_XSDI_ACCELERATORS,
                         (    !(pGlobalSettings->NoSubclassing)
                           && (pGlobalSettings->fEnableFolderHotkeys)
                         ));

        WinEnableControl(pcnbp->hwndDlgPage,
                         ID_XSDI_SNAPTOGRID,  // added V0.9.1 (99-12-28) [umoeller]
                         (pGlobalSettings->fEnableSnap2Grid));

        WinEnableControl(pcnbp->hwndDlgPage,
                         ID_XSDI_ENABLESTATUSBAR,
                         // always disable for Desktop
                         (   (pcnbp->somSelf != _wpclsQueryActiveDesktop(_WPDesktop))
                          && (!(pGlobalSettings->NoSubclassing))
                          && (pGlobalSettings->fEnableStatusBars)
                         ));
    }
}

/*
 * fdrXFolderItemChanged:
 *      "XFolder" page notebook callback function (notebook.c).
 *      Reacts to changes of any of the dialog controls.
 *
 *@@changed V0.9.0 [umoeller]: adjusted function prototype
 *@@changed V0.9.0 [umoeller]: moved this func here from xfldr.c
 */

MRESULT fdrXFolderItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                              USHORT usItemID,
                              USHORT usNotifyCode,
                              ULONG ulExtra)      // for checkboxes: contains new state
{
    XFolderData *somThis = XFolderGetData(pcnbp->somSelf);
    BOOL fUpdate = TRUE;

    switch (usItemID)
    {
        case ID_XSDI_SNAPTOGRID:
            _bSnapToGridInstance = ulExtra;
        break;

        case ID_XSDI_FULLPATH:
            _bFullPathInstance = ulExtra;
            fdrUpdateAllFrameWndTitles(pcnbp->somSelf);
        break;

        case ID_XSDI_ACCELERATORS:
            _bFolderHotkeysInstance = ulExtra;
        break;

        case ID_XSDI_ENABLESTATUSBAR:
            _xwpSetStatusBarVisibility(pcnbp->somSelf,
                        ulExtra,
                        TRUE);  // update open folder views
        break;

        case ID_XSDI_FAVORITEFOLDER:
            _xwpMakeFavoriteFolder(pcnbp->somSelf, ulExtra);
        break;

        case ID_XSDI_QUICKOPEN:
            _xwpSetQuickOpen(pcnbp->somSelf, ulExtra);
        break;

        case DID_UNDO:
            if (pcnbp->pUser)
            {
                XFolderData *Backup = (pcnbp->pUser);
                // "Undo" button: restore backed up instance data
                _bFullPathInstance = Backup->bFullPathInstance;
                _bSnapToGridInstance = Backup->bSnapToGridInstance;
                _bFolderHotkeysInstance = Backup->bFolderHotkeysInstance;
                _xwpSetStatusBarVisibility(pcnbp->somSelf,
                                           Backup->bStatusBarInstance,
                                           TRUE);  // update open folder views
                // have the page updated by calling the callback above
                fdrXFolderInitPage(pcnbp, CBI_SHOW | CBI_ENABLE);
                fdrUpdateAllFrameWndTitles(pcnbp->somSelf);
            }
        break;

        case DID_DEFAULT:
            // "Default" button:
            _bFullPathInstance = 2;
            _bSnapToGridInstance = 2;
            _bFolderHotkeysInstance = 2;
            _xwpSetStatusBarVisibility(pcnbp->somSelf,
                        STATUSBAR_DEFAULT,
                        TRUE);  // update open folder views
            // have the page updated by calling the callback above
            fdrXFolderInitPage(pcnbp, CBI_SET | CBI_ENABLE);
            fdrUpdateAllFrameWndTitles(pcnbp->somSelf);
        break;

        default:
            fUpdate = FALSE;
        break;
    }

    if (fUpdate)
        _wpSaveDeferred(pcnbp->somSelf);

    return ((MPARAM)0);
}

/*
 * fdrSortInitPage:
 *      "Sort" page notebook callback function (notebook.c).
 *      Sets the controls on the page.
 *      The "Sort" callbacks are used both for the folder settings notebook page
 *      AND the respective "Sort" page in the "Workplace Shell" object, so we
 *      need to keep instance data and the XFolder Global Settings apart.
 *      We do this by examining the page ID in the notebook info struct.
 *
 *@@changed V0.9.0 [umoeller]: updated settings page
 *@@changed V0.9.0 [umoeller]: adjusted function prototype
 *@@changed V0.9.0 [umoeller]: moved this func here from xfldr.c
 */

VOID fdrSortInitPage(PCREATENOTEBOOKPAGE pcnbp,
                     ULONG flFlags)
{
    PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    HWND        hwndListbox = WinWindowFromID(pcnbp->hwndDlgPage,
                    ID_XSDI_SORTLISTBOX);
    XFolderData *somThis = NULL;

    if (pcnbp->ulPageID == SP_FLDRSORT_FLDR)
    {
        // if we're being called from a folder's notebook,
        // get instance data
        somThis = XFolderGetData(pcnbp->somSelf);

        if (flFlags & CBI_INIT)
        {
            if (pcnbp->pUser == NULL)
            {
                // first call: backup instance data for "Undo" button;
                // this memory will be freed automatically by the
                // common notebook window function (notebook.c) when
                // the notebook page is destroyed
                pcnbp->pUser = malloc(sizeof(XFolderData));
                memcpy(pcnbp->pUser, somThis, sizeof(XFolderData));
            }

            // hide the "Enable extended sort" checkbox, which is
            // only visible in the "Workplace Shell" object
            // WinShowWindow(WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_REPLACESORT), FALSE);
                // removed (V0.9.0)
        }
    }

    if (flFlags & CBI_INIT)
    {
        WinInsertLboxItem(hwndListbox, LIT_END, pNLSStrings->pszSortByName);
        WinInsertLboxItem(hwndListbox, LIT_END, pNLSStrings->pszSortByType);
        WinInsertLboxItem(hwndListbox, LIT_END, pNLSStrings->pszSortByClass);
        WinInsertLboxItem(hwndListbox, LIT_END, pNLSStrings->pszSortByRealName);
        WinInsertLboxItem(hwndListbox, LIT_END, pNLSStrings->pszSortBySize);
        WinInsertLboxItem(hwndListbox, LIT_END, pNLSStrings->pszSortByWriteDate);
        WinInsertLboxItem(hwndListbox, LIT_END, pNLSStrings->pszSortByAccessDate);
        WinInsertLboxItem(hwndListbox, LIT_END, pNLSStrings->pszSortByCreationDate);
        WinInsertLboxItem(hwndListbox, LIT_END, pNLSStrings->pszSortByExt);
        WinInsertLboxItem(hwndListbox, LIT_END, pNLSStrings->pszSortFoldersFirst);
    }

    if (somThis)
    {
        // "folder" mode:
        if (flFlags & CBI_SET)
        {
            winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_ALWAYSSORT,
                                  ALWAYS_SORT);
            WinSendMsg(hwndListbox,
                       LM_SELECTITEM,
                       (MPARAM)DEFAULT_SORT,
                       (MPARAM)TRUE);
        }
    }
    else
    {
        // "global" mode:
        if (flFlags & CBI_SET)
        {
            winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_ALWAYSSORT,
                                  pGlobalSettings->AlwaysSort);

            WinSendMsg(hwndListbox,
                       LM_SELECTITEM,
                       (MPARAM)(pGlobalSettings->DefaultSort),
                       (MPARAM)TRUE);
        }

        if (flFlags & CBI_ENABLE)
        {
                // removed (V0.9.0);
                // the page now isn't even inserted
        }
    }
}

/*
 * fdrSortItemChanged:
 *      "Sort" page notebook callback function (notebook.c).
 *      Reacts to changes of any of the dialog controls.
 *
 *      The "Sort" callbacks are used both for the folder settings notebook page
 *      AND the respective "Sort" page in the "Workplace Shell" object, so we
 *      need to keep instance data and the XFolder GLobal Settings apart.
 *
 *@@changed V0.9.0 [umoeller]: updated settings page
 *@@changed V0.9.0 [umoeller]: adjusted function prototype
 *@@changed V0.9.0 [umoeller]: moved this func here from xfldr.c
 */

MRESULT fdrSortItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                           USHORT usItemID,
                           USHORT usNotifyCode,
                           ULONG ulExtra)      // for checkboxes: contains new state
{
    BOOL fUpdate = TRUE;

    switch (usItemID)
    {
        case ID_XSDI_ALWAYSSORT:
        case ID_XSDI_SORTLISTBOX:
        {
            HWND        hwndListbox = WinWindowFromID(pcnbp->hwndDlgPage,
                                                      ID_XSDI_SORTLISTBOX);

            if (pcnbp->ulPageID == SP_FLDRSORT_FLDR)
            {
                // if we're being called from a folder's notebook,
                // change instance data
                _xwpSetFldrSort(pcnbp->somSelf,
                            // DefaultSort:
                                (USHORT)(WinSendMsg(hwndListbox,
                                                    LM_QUERYSELECTION,
                                                    (MPARAM)LIT_CURSOR,
                                                    MPNULL)),
                            // AlwaysSort:
                                (USHORT)(winhIsDlgItemChecked(pcnbp->hwndDlgPage,
                                                              ID_XSDI_ALWAYSSORT))
                            );
            }
            else
            {
                GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(5000);

                BOOL bTemp;
                /* pGlobalSettings->ReplaceSort =
                    winhIsDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_REPLACESORT); */
                    // removed (V0.9.0)
                pGlobalSettings->DefaultSort =
                                      (BYTE)WinSendMsg(hwndListbox,
                                          LM_QUERYSELECTION,
                                          (MPARAM)LIT_CURSOR,
                                          MPNULL);
                bTemp = winhIsDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_ALWAYSSORT);
                if (bTemp)
                {
                    USHORT usDefaultSort, usAlwaysSort;
                    _xwpQueryFldrSort(_wpclsQueryActiveDesktop(_WPDesktop),
                                      &usDefaultSort, &usAlwaysSort);
                    if (usAlwaysSort != 0)
                    {
                        // issue warning that this might also sort the Desktop
                        if (cmnMessageBoxMsg(pcnbp->hwndFrame,
                                             116, 133,
                                             MB_YESNO)
                                       == MBID_YES)
                            _xwpSetFldrSort(_wpclsQueryActiveDesktop(_WPDesktop),
                                            usDefaultSort, 0);
                    }
                }
                pGlobalSettings->AlwaysSort = bTemp;

                cmnUnlockGlobalSettings();
            }
        break; }

        /* case ID_XSDI_REPLACESORT: {
            PGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
            // "extended sorting on": exists on global page only
            pGlobalSettings->ReplaceSort = ulExtra;
            fdrSortInitPage(pnbi, CBI_ENABLE);
        break; } */ // removed (V0.9.0)

        // control other than listbox:
        case DID_UNDO:
            // "Undo" button: restore backed up instance/global data
            if (pcnbp->ulPageID == SP_FLDRSORT_FLDR)
            {
                // if we're being called from a folder's notebook,
                // restore instance data
                if (pcnbp->pUser)
                {
                    XFolderData *Backup = (pcnbp->pUser);
                    _xwpSetFldrSort(pcnbp->somSelf,
                                    Backup->bDefaultSortInstance,
                                    Backup->bAlwaysSortInstance);
                }
            }
            else
            {
                // global sort page:
                 cmnSetDefaultSettings(SP_FLDRSORT_GLOBAL);
            }
            fdrSortInitPage(pcnbp, CBI_SET | CBI_ENABLE);
        break;

        case DID_DEFAULT:
            // "Default" button:
            if (pcnbp->ulPageID == SP_FLDRSORT_FLDR)
                _xwpSetFldrSort(pcnbp->somSelf, SET_DEFAULT, SET_DEFAULT);
            else
                cmnSetDefaultSettings(SP_FLDRSORT_GLOBAL);

            fdrSortInitPage(pcnbp, CBI_SET | CBI_ENABLE);
        break;

        default:
            fUpdate = FALSE;
        break;
    }

    if (fUpdate)
    {
        if (pcnbp->ulPageID == SP_FLDRSORT_FLDR)
        {
            // instance:
            _wpSaveDeferred(pcnbp->somSelf);
            // update our folder only
            fdrForEachOpenInstanceView(pcnbp->somSelf,
                                       0,
                                       &fdrUpdateFolderSorts);
        }
        else
        {
            // global:
            cmnStoreGlobalSettings();
            // update all open folders
            fdrForEachOpenGlobalView(0,
                                     &fdrUpdateFolderSorts);
        }
    }
    return ((MPARAM)-1);
}

/* ******************************************************************
 *                                                                  *
 *   XFldStartup notebook callbacks (notebook.c)                    *
 *                                                                  *
 ********************************************************************/

/*
 * fdrStartupFolderInitPage:
 *      notebook callback function (notebook.c) for the
 *      "Startup" page in the startup folder's settings notebook.
 *      (This used to be page 2 in the Desktop's settings notebook
 *      before V0.9.0.)
 *      Sets the controls on the page according to the
 *      Global Settings.
 *
 *@@changed V0.9.0 [umoeller]: moved this here from xfdesk.c
 *@@changed V0.9.0 [umoeller]: adjusted function prototype
 */

VOID fdrStartupFolderInitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                              ULONG flFlags)        // CBI_* flags (notebook.h)
{
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    if (flFlags & CBI_SET)
    {
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_SDDI_SHOWSTARTUPPROGRESS,
                              pGlobalSettings->ShowStartupProgress);
        winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_SDDI_STARTUPDELAY,
                               500, 10000,
                               pGlobalSettings->ulStartupDelay);
    }

    if (flFlags & CBI_ENABLE)
    {
    }
}

/*
 * fdrStartupFolderItemChanged:
 *      notebook callback function (notebook.c) for the
 *      "Startup" page in the startup folder's settings notebook.
 *      (This used to be page 2 in the Desktop's settings notebook
 *      before V0.9.0.)
 *      Reacts to changes of any of the dialog controls.
 *
 *@@changed V0.9.0 [umoeller]: moved this here from xfdesk.c
 *@@changed V0.9.0 [umoeller]: adjusted function prototype
 */

MRESULT fdrStartupFolderItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                    USHORT usItemID, USHORT usNotifyCode,
                                    ULONG ulExtra)      // for checkboxes: contains new state
{
    GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(5000);
    ULONG   ulChange = 1;

    MRESULT mrc = (MRESULT)-1;

    switch (usItemID)
    {
        case ID_SDDI_SHOWSTARTUPPROGRESS:
            pGlobalSettings->ShowStartupProgress = ulExtra;
        break;

        case ID_SDDI_STARTUPDELAY:
            pGlobalSettings->ulStartupDelay = winhAdjustDlgItemSpinData(pcnbp->hwndDlgPage,
                                                                        usItemID,
                                                                        250, usNotifyCode);
        break;

        default:
            ulChange = 0;
    }

    cmnUnlockGlobalSettings();

    if (ulChange)
        cmnStoreGlobalSettings();

    return (mrc);
}


