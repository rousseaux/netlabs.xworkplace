
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
 *      Copyright (C) 1997-2001 Ulrich M�ller.
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

#define INCL_DOSEXCEPTIONS
#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#define INCL_DOSMISC
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
#include "helpers\comctl.h"             // common controls (window procs)
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\eah.h"                // extended attributes helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\gpih.h"               // GPI helper routines
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\nls.h"                // National Language Support helpers
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\stringh.h"            // string helper routines
#include "helpers\syssound.h"           // system sound helper routines
#include "helpers\threads.h"            // thread helpers
#include "helpers\winh.h"               // PM helper routines
#include "helpers\wphandle.h"           // file-system object handles
#include "helpers\xstring.h"            // extended string helpers

// SOM headers which don't crash with prec. header files
#include "xfldr.ih"
#include "xfobj.ih"
#include "xfdisk.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\cnrsort.h"             // container sort comparison functions
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\helppanels.h"          // all XWorkplace help panel IDs
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

#include "filesys\fileops.h"            // file operations implementation
#include "filesys\folder.h"             // XFolder implementation
#include "filesys\fdrmenus.h"           // shared folder menu logic
#include "filesys\statbars.h"           // status bar translation logic
#include "filesys\xthreads.h"           // extra XWorkplace threads

// other SOM headers
#pragma hdrstop                     // VAC++ keeps crashing otherwise

// #include <wpdesk.h>
#include <wpshadow.h>           // WPShadow
#include <wprootf.h>

#include "helpers\undoc.h"              // some undocumented stuff

/* ******************************************************************
 *
 *   Setup strings
 *
 ********************************************************************/

/*
 *@@ fdrHasShowAllInTreeView:
 *      returns TRUE if the folder has the
 *      SHOWALLINTREEVIEW setting set. On
 *      Warp 3, always returns FALSE.
 *
 *@@added V0.9.14 (2001-07-28) [umoeller]
 */

BOOL fdrHasShowAllInTreeView(WPFolder *somSelf)
{
    XFolderData *somThis = XFolderGetData(somSelf);
    return (    (_pulFolderShowAllInTreeView) // only != NULL on Warp 4
             && (*_pulFolderShowAllInTreeView)
           );
}


/*
 *@@ fdrSetup:
 *      implementation for XFolder::wpSetup.
 *
 *@@added V0.9.9 (2001-04-04) [umoeller]
 *@@changed V0.9.12 (2001-05-20) [umoeller]: adjusted for new folder sorting
 */

BOOL fdrSetup(WPFolder *somSelf,
              const char *pszSetupString)
{
    PCGLOBALSETTINGS     pGlobalSettings = cmnQueryGlobalSettings();
    XFolderData *somThis = XFolderGetData(somSelf);

    BOOL        rc = TRUE,
                fChanged = FALSE;       // instance data changed

    LONG        lDefaultSort,
                lFoldersFirst,
                lAlwaysSort;

    CHAR        szValue[CCHMAXPATH + 1];
    ULONG       cbValue = sizeof(szValue);
    if (_wpScanSetupString(somSelf, (PSZ)pszSetupString,
                           "SNAPTOGRID", szValue, &cbValue))
    {
        rc = TRUE;
        fChanged = TRUE;
        if (!strnicmp(szValue, "NO", 2))
            _bSnapToGridInstance = 0;
        else if (!strnicmp(szValue, "YES", 3))
            _bSnapToGridInstance = 1;
        else if (!strnicmp(szValue, "DEFAULT", 7))
            _bSnapToGridInstance = 2;
        else if (!strnicmp(szValue, "EXEC", 4))
        {
            fdrSnapToGrid(somSelf, FALSE);
            fChanged = FALSE;
        }
        else
        {
            fChanged = FALSE;
            rc = FALSE;
        }
    }

    cbValue = sizeof(szValue);
    if (_wpScanSetupString(somSelf, (PSZ)pszSetupString,
                           "FULLPATH", szValue, &cbValue))
    {
        fChanged = TRUE;
        rc = TRUE;
        if (!strnicmp(szValue, "NO", 2))
            _bFullPathInstance = 0;
        else if (!strnicmp(szValue, "YES", 3))
            _bFullPathInstance = 1;
        else if (!strnicmp(szValue, "DEFAULT", 7))
            _bFullPathInstance = 2;

        fdrUpdateAllFrameWndTitles(somSelf);
    }

    cbValue = sizeof(szValue);
    if (_wpScanSetupString(somSelf, (PSZ)pszSetupString,
                           "ACCELERATORS", szValue, &cbValue))
    {
        fChanged = TRUE;
        rc = TRUE;
        if (!strnicmp(szValue, "NO", 2))
            _bFolderHotkeysInstance = 0;
        else if (!strnicmp(szValue, "YES", 3))
            _bFolderHotkeysInstance = 1;
        else if (!strnicmp(szValue, "DEFAULT", 7))
            _bFolderHotkeysInstance = 2;
    }

    cbValue = sizeof(szValue);
    if (_wpScanSetupString(somSelf, (PSZ)pszSetupString,
                           "FAVORITEFOLDER", szValue, &cbValue))
    {
        rc = TRUE;
        if (!strnicmp(szValue, "NO", 2))
            _xwpMakeFavoriteFolder(somSelf, FALSE);
        else if (!strnicmp(szValue, "YES", 3))
            _xwpMakeFavoriteFolder(somSelf, TRUE);
        // fChanged = TRUE;
    }

    cbValue = sizeof(szValue);
    if (_wpScanSetupString(somSelf, (PSZ)pszSetupString,
                           "QUICKOPEN", szValue, &cbValue))
    {
        rc = TRUE;
        if (!strnicmp(szValue, "NO", 2))
            _xwpSetQuickOpen(somSelf, FALSE);
        else if (!strnicmp(szValue, "YES", 3))
            _xwpSetQuickOpen(somSelf, TRUE);
        else if (!strnicmp(szValue, "IMMEDIATE", 3))  // V0.9.6 (2000-10-16) [umoeller]
            fdrQuickOpen(somSelf,
                         NULL,
                         0);     // no callback
    }

    if (somSelf != cmnQueryActiveDesktop())
    {
        cbValue = sizeof(szValue);
        if (_wpScanSetupString(somSelf, (PSZ)pszSetupString,
                               "STATUSBAR", szValue, &cbValue))
        {
            rc = TRUE;
            if (!strnicmp(szValue, "NO", 2))
                _bStatusBarInstance = STATUSBAR_OFF;
            else if (!strnicmp(szValue, "YES", 3))
                _bStatusBarInstance = STATUSBAR_ON;
            else if (!strnicmp(szValue, "DEFAULT", 7))
                _bStatusBarInstance = STATUSBAR_DEFAULT;
        }
        xthrPostWorkerMsg(WOM_UPDATEALLSTATUSBARS,
                          (MPARAM)1,  // show/hide flag
                          MPNULL);
        fChanged = TRUE;
    }

#ifndef __ALWAYSEXTSORT__
    if (cmnIsFeatureEnabled(ExtendedSorting))
#endif
    {
        cbValue = sizeof(szValue);
        if (_wpScanSetupString(somSelf, (PSZ)pszSetupString,
                               "ALWAYSSORT", szValue, &cbValue))
        {
            rc = TRUE;
            _xwpQueryFldrSort(somSelf,
                              &lDefaultSort,
                              &lFoldersFirst,
                              &lAlwaysSort);

            if (!strnicmp(szValue, "NO", 2))
                lAlwaysSort = 0;
            else if (!strnicmp(szValue, "YES", 3))
                lAlwaysSort = 1;
            else if (!strnicmp(szValue, "DEFAULT", 7))
                lAlwaysSort = SET_DEFAULT;
            _xwpSetFldrSort(somSelf,
                            lDefaultSort,
                            lFoldersFirst,
                            lAlwaysSort);
        }
        cbValue = sizeof(szValue);
        if (_wpScanSetupString(somSelf, (PSZ)pszSetupString,
                               "SORTFOLDERSFIRST", szValue, &cbValue))
        {
            rc = TRUE;
            _xwpQueryFldrSort(somSelf,
                              &lDefaultSort,
                              &lFoldersFirst,
                              &lAlwaysSort);

            if (!strnicmp(szValue, "NO", 2))
                lFoldersFirst = 0;
            else if (!strnicmp(szValue, "YES", 3))
                lFoldersFirst = 1;
            else if (!strnicmp(szValue, "DEFAULT", 7))
                lFoldersFirst = SET_DEFAULT;
            _xwpSetFldrSort(somSelf,
                            lDefaultSort,
                            lFoldersFirst,
                            lAlwaysSort);
        }

        cbValue = sizeof(szValue);
        if (_wpScanSetupString(somSelf, (PSZ)pszSetupString,
                               "DEFAULTSORT", szValue, &cbValue))
        {
            LONG lValue;

            rc = TRUE;
            _xwpQueryFldrSort(somSelf,
                              &lDefaultSort,
                              &lFoldersFirst,
                              &lAlwaysSort);

            sscanf(szValue, "%d", &lValue);
            if ( (lValue >= -4) && (lValue <= 100) )
                lDefaultSort = lValue;
            else
                lDefaultSort = SET_DEFAULT;
            _xwpSetFldrSort(somSelf,
                            lDefaultSort,
                            lFoldersFirst,
                            lAlwaysSort);
        }

        cbValue = sizeof(szValue);
        if (_wpScanSetupString(somSelf,
                               (PSZ)pszSetupString,
                               "SORTNOW", szValue, &cbValue))
        {
            USHORT usSort;
            LONG lValue;

            rc = TRUE;

            sscanf(szValue, "%d", &lValue);
            if ( (lValue >= -4) && (lValue <= 100) )
                usSort = lValue;
            else
                usSort = SET_DEFAULT;

            fdrForEachOpenInstanceView(somSelf,
                                       usSort,
                                       fdrSortAllViews);
        }

#ifdef __DEBUG__
        cbValue = sizeof(szValue);
        if (_wpScanSetupString(somSelf,
                               (PSZ)pszSetupString,
                               "ISFSAWAKE",
                               szValue,
                               &cbValue))
        {
            rc = !!fdrQueryAwakeFSObject(szValue);
        }
#endif
    }

    if (fChanged)
        _wpSaveDeferred(somSelf);

    return (rc);
}

/*
 *@@ fdrQuerySetup:
 *      implementation of XFolder::xwpQuerySetup2.
 *      See remarks there.
 *
 *      This returns the length of the XFolder
 *      setup string part only.
 *
 *@@added V0.9.1 (2000-01-17) [umoeller]
 *@@changed V0.9.3 (2000-04-09) [umoeller]: bitmaps on boot drive are returned with "?:\" now
 *@@changed V0.9.3 (2000-05-30) [umoeller]: ICONSHADOWCOLOR was reported as TREESHADOWCOLOR. Fixed.
 *@@changed V0.9.12 (2001-05-20) [umoeller]: adjusted for new folder sorting
 */

BOOL fdrQuerySetup(WPObject *somSelf,
                   PVOID pstrSetup)
{
    BOOL brc = TRUE;

    TRY_LOUD(excpt1)
    {
        // flag defined in
        #define WP_GLOBAL_COLOR         0x40000000

        XFolderData *somThis = XFolderGetData(somSelf);
        PCGLOBALSETTINGS     pGlobalSettings = cmnQueryGlobalSettings();

        // temporary buffer for building the setup string
        XSTRING // strTemp,
                strView;
        ULONG   ulValue = 0;
        PSZ     pszValue = 0,
                pszDefaultValue = 0;
        SOMClass *pClassObject = 0;
        BOOL    fTreeIconsInvisible = FALSE,
                fIconViewColumns = FALSE;

        CHAR    szTemp[1000] = "";

        BOOL    fIsWarp4 = doshIsWarp4();

        // some settings better have this extra check,
        // not sure if it's really needed
        BOOL    fInitialized = _wpIsObjectInitialized(somSelf);

        // xstrInit(pstrSetup, 400);
        xstrInit(&strView, 200);

        // WORKAREA
        if (_wpQueryFldrFlags(somSelf) & FOI_WORKAREA)
            xstrcat(pstrSetup, "WORKAREA=YES;", 0);

        // MENUBAR
        ulValue = _xwpQueryMenuBarVisibility(somSelf);
        if (ulValue != _xwpclsQueryMenuBarVisibility(_XFolder))
            // non-default value:
            if (ulValue)
                xstrcat(pstrSetup, "MENUBAR=YES;", 0);
            else
                xstrcat(pstrSetup, "MENUBAR=NO;", 0);

        /*
         * folder sort settings
         *
         */

        // SORTBYATTR... don't think we need this

#ifndef __ALWAYSEXTSORT__
        if (cmnIsFeatureEnabled(ExtendedSorting))
#endif
        {
            if (_lAlwaysSort != SET_DEFAULT)
            {
                if (_lAlwaysSort)
                    xstrcat(pstrSetup, "ALWAYSSORT=YES;", 0);
                else
                    xstrcat(pstrSetup, "ALWAYSSORT=NO;", 0);
            }

            if (_lFoldersFirst != SET_DEFAULT)
            {
                if (_lFoldersFirst)
                    xstrcat(pstrSetup, "SORTFOLDERSFIRST=YES;", 0);
                else
                    xstrcat(pstrSetup, "SORTFOLDERSFIRST=NO;", 0);
            }

            if (_lDefSortCrit != SET_DEFAULT)
            {
                sprintf(szTemp, "DEFAULTSORT=%d;", _lDefSortCrit);
                xstrcat(pstrSetup, szTemp, 0);
            }
        } // end V0.9.12 (2001-05-20) [umoeller]

        // SORTCLASS
        pClassObject = _wpQueryFldrSortClass(somSelf);
        if (pClassObject != _WPFileSystem)
        {
            sprintf(szTemp, "SORTCLASS=%s;", _somGetName(pClassObject));
            xstrcat(pstrSetup, szTemp, 0);
        }

        /*
         * folder view settings
         *
         */

        // BACKGROUND
        if (fInitialized) // V0.9.3 (2000-04-29) [umoeller]
            if ((_pszFolderBkgndImageFile) && (_pFolderBackground))
            {
                CHAR cType = 'S';

                PBYTE pbRGB = ( (PBYTE)(&(_pFolderBackground->rgbColor)) );

                PSZ pszBitmapFile = strdup(_pszFolderBkgndImageFile);
                CHAR cBootDrive = doshQueryBootDrive();

                nlsUpper(pszBitmapFile, 0);
                if (*pszBitmapFile == cBootDrive)
                    // file on boot drive:
                    // replace with '?' to make it portable
                    *pszBitmapFile = '?';

                switch (_pFolderBackground->bImageType & 0x07) // ?2=Normal, ?3=tiled, ?4=scaled
                {
                    case 2: cType = 'N'; break;
                    case 3: cType = 'T'; break;
                    default: /* 4 */ cType = 'S'; break;
                }

                sprintf(szTemp, "BACKGROUND=%s,%c,%d,%c,%d %d %d;",
                        pszBitmapFile,  // image name
                        cType,                    // N = normal, T = tiled, S = scaled
                        _pFolderBackground->bScaleFactor,  // scaling factor
                        (_pFolderBackground->bColorOnly == 0x28) // 0x28 Image, 0x27 Color only
                            ? 'I'
                            : 'C', // I = image, C = color only
                        *(pbRGB + 2), *(pbRGB + 1), *pbRGB);  // RGB color; apparently optional
                xstrcat(pstrSetup, szTemp, 0);

                free(pszBitmapFile);
            }

        // DEFAULTVIEW: already handled by XFldObject

        /*
         * Icon view
         *
         */

        // ICONFONT
        pszValue = _wpQueryFldrFont(somSelf, OPEN_CONTENTS);
        if (pszDefaultValue = prfhQueryProfileData(HINI_USER,
                                                   PMINIAPP_SYSTEMFONTS, // "PM_SystemFonts",
                                                   PMINIKEY_ICONTEXTFONT, // "IconText",
                                                   NULL))
        {
            if (strcmp(pszValue, pszDefaultValue) != 0)
            {
                sprintf(szTemp, "ICONFONT=%s;", pszValue);
                xstrcat(pstrSetup, szTemp, 0);
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
                xstrcat(&strView, "FLOWED", 0);
                fIconViewColumns = TRUE;        // needed for colors below
            break;

            case (CV_NAME): // but not CV_ICON | CV_FLOW or CV_TEXT
                xstrcat(&strView, "NONFLOWED", 0);
                fIconViewColumns = TRUE;        // needed for colors below
            break;

            case (CV_TEXT): // but not CV_ICON
                xstrcat(&strView, "INVISIBLE", 0);
            break;
        }

        if (ulValue & CV_MINI)
        {
            // ICONVIEW=MINI
            if (strView.ulLength)
                xstrcatc(&strView, ',');

            xstrcat(&strView, "MINI", 0);
        }

        if (strView.ulLength)
        {
            sprintf(szTemp, "ICONVIEW=%s;", strView.psz);
            xstrcat(pstrSetup, szTemp, 0);
        }

        xstrClear(&strView);

        // ICONTEXTBACKGROUNDCOLOR

        // ICONTEXTCOLOR
        if (fInitialized) // V0.9.3 (2000-04-29) [umoeller]
            if (_pFolderLongArray)
            {
                BYTE bUseDefault = FALSE;
                PBYTE pbArrayField = ( (PBYTE)(&(_pFolderLongArray->rgbIconViewTextColAsPlaced)) );
                if (fIconViewColumns)
                    // FLOWED or NONFLOWED: use different field then
                    pbArrayField = ( (PBYTE)(&(_pFolderLongArray->rgbIconViewTextColColumns)) );

                bUseDefault = *(pbArrayField + 3);
                if (!bUseDefault)
                {
                    BYTE bRed   = *(pbArrayField + 2);
                    BYTE bGreen = *(pbArrayField + 1);
                    BYTE bBlue  = *(pbArrayField );

                    sprintf(szTemp, "ICONTEXTCOLOR=%d %d %d;", bRed, bGreen, bBlue);
                    xstrcat(pstrSetup, szTemp, 0);
                }
            }

        // ICONSHADOWCOLOR
        if (fInitialized) // V0.9.3 (2000-04-29) [umoeller]
            if (_pFolderLongArray)
                // only Warp 4 has these fields, so check size of array
                if (_cbFolderLongArray >= 84)
                {
                    PBYTE pbArrayField = ( (PBYTE)(&(_pFolderLongArray->rgbIconViewShadowCol)) );

                    BYTE bUseDefault = *(pbArrayField + 3);
                    if (!bUseDefault)
                    {
                        BYTE bRed   = *(pbArrayField + 2);
                        BYTE bGreen = *(pbArrayField + 1);
                        BYTE bBlue  = *(pbArrayField );

                        sprintf(szTemp, "ICONSHADOWCOLOR=%d %d %d;", bRed, bGreen, bBlue);
                        xstrcat(pstrSetup, szTemp, 0);
                    }
                }

        /*
         * Tree view
         *
         */

        // TREEFONT
        pszValue = _wpQueryFldrFont(somSelf, OPEN_TREE);
        if (pszDefaultValue = prfhQueryProfileData(HINI_USER,
                                                   PMINIAPP_SYSTEMFONTS, // "PM_SystemFonts",
                                                   PMINIKEY_ICONTEXTFONT, // "IconText",
                                                   NULL))
        {
            if (strcmp(pszValue, pszDefaultValue) != 0)
            {
                sprintf(szTemp, "TREEFONT=%s;", pszValue);
                xstrcat(pstrSetup, szTemp, 0);
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
                xstrcat(&strView, "INVISIBLE", 0);
                fTreeIconsInvisible = TRUE;         // needed for tree text colors below
            break;
        }

        if (fIsWarp4)
        {
            // on Warp 4, mini icons in Tree view are the default
            if ((ulValue & CV_MINI) == 0)
            {
                // TREEVIEW=MINI
                if (strView.ulLength)
                    xstrcatc(&strView, ',');

                xstrcat(&strView, "NORMAL", 0);
            }
        }
        else
            // Warp 3:
            if ((ulValue & CV_MINI) != 0)
            {
                // TREEVIEW=MINI
                if (strView.ulLength)
                    xstrcatc(&strView, ',');

                xstrcat(&strView, "MINI", 0);
            }

        if ((ulValue & CA_TREELINE) == 0)
        {
            // TREEVIEW=NOLINES
            if (strView.ulLength)
                xstrcatc(&strView, ',');

            xstrcat(&strView, "NOLINES", 0);
        }

        if (strView.ulLength)
        {
            sprintf(szTemp, "TREEVIEW=%s;", strView);
            xstrcat(pstrSetup, szTemp, 0);
        }

        xstrClear(&strView);

        if (fInitialized) // V0.9.3 (2000-04-29) [umoeller]
            if (_pFolderLongArray)
            {
                // TREETEXTCOLOR
                BYTE bUseDefault = FALSE;
                PBYTE pbArrayField = ( (PBYTE)(&(_pFolderLongArray->rgbTreeViewTextColIcons)) );

                if (fTreeIconsInvisible)
                    pbArrayField = ( (PBYTE)(&(_pFolderLongArray->rgbTreeViewTextColTextOnly)) );

                bUseDefault = *(pbArrayField + 3);
                if (!bUseDefault)
                {
                    BYTE bRed   = *(pbArrayField + 2);
                    BYTE bGreen = *(pbArrayField + 1);
                    BYTE bBlue  = *(pbArrayField );

                    sprintf(szTemp, "TREETEXTCOLOR=%d %d %d;", bRed, bGreen, bBlue);
                    xstrcat(pstrSetup, szTemp, 0);
                }

                // TREESHADOWCOLOR
                // only Warp 4 has these fields, so check size of array
                if (_cbFolderLongArray >= 84)
                {
                    pbArrayField = ( (PBYTE)(&(_pFolderLongArray->rgbTreeViewShadowCol)) );

                    bUseDefault = *(pbArrayField + 3);
                    if (!bUseDefault)
                    {
                        BYTE bRed   = *(pbArrayField + 2);
                        BYTE bGreen = *(pbArrayField + 1);
                        BYTE bBlue  = *(pbArrayField );

                        sprintf(szTemp, "TREESHADOWCOLOR=%d %d %d;", bRed, bGreen, bBlue);
                        xstrcat(pstrSetup, szTemp, 0);
                    }
                }
            }

        // SHOWALLINTREEVIEW
        if (    (fInitialized)
             && (fdrHasShowAllInTreeView(somSelf))
           )
            xstrcat(pstrSetup, "SHOWALLINTREEVIEW=YES;", 0);

        /*
         * Details view
         *
         */

        // DETAILSCLASS
        pClassObject = _wpQueryFldrDetailsClass(somSelf);
        if (pClassObject != _WPFileSystem)
        {
            sprintf(szTemp, "DETAILSCLASS=%s;", _somGetName(pClassObject));
            xstrcat(pstrSetup, szTemp, 0);
        }

        // DETAILSFONT
        pszValue = _wpQueryFldrFont(somSelf, OPEN_DETAILS);
        if (pszDefaultValue = prfhQueryProfileData(HINI_USER,
                                                   PMINIAPP_SYSTEMFONTS, // "PM_SystemFonts",
                                                   PMINIKEY_ICONTEXTFONT, // "IconText",
                                                   NULL))
        {
            if (strcmp(pszValue, pszDefaultValue) != 0)
            {
                sprintf(szTemp, "DETAILSFONT=%s;", pszValue);
                xstrcat(pstrSetup, szTemp, 0);
            }
            free(pszDefaultValue);
        }

        // DETAILSTODISPLAY

        // DETAILSVIEW

        // DETAILSTEXTCOLOR
        if (fInitialized) // V0.9.3 (2000-04-29) [umoeller]
            if (_pFolderLongArray)
            {
                BYTE bUseDefault = FALSE;
                PBYTE pbArrayField = ( (PBYTE)(&(_pFolderLongArray->rgbDetlViewTextCol)) );

                bUseDefault = *(pbArrayField + 3);
                if (!bUseDefault)
                {
                    BYTE bRed   = *(pbArrayField + 2);
                    BYTE bGreen = *(pbArrayField + 1);
                    BYTE bBlue  = *(pbArrayField );

                    sprintf(szTemp, "DETAILSTEXTCOLOR=%d %d %d;", bRed, bGreen, bBlue);
                    xstrcat(pstrSetup, szTemp, 0);
                }

                // DETAILSSHADOWCOLOR
                // only Warp 4 has these fields, so check size of array
                if (_cbFolderLongArray >= 84)
                {
                    pbArrayField = ( (PBYTE)(&(_pFolderLongArray->rgbDetlViewShadowCol)) );

                    bUseDefault = *(pbArrayField + 3);
                    if (!bUseDefault)
                    {
                        BYTE bRed   = *(pbArrayField + 2);
                        BYTE bGreen = *(pbArrayField + 1);
                        BYTE bBlue  = *(pbArrayField );

                        sprintf(szTemp, "DETAILSSHADOWCOLOR=%d %d %d;", bRed, bGreen, bBlue);
                        xstrcat(pstrSetup, szTemp, 0);
                    }
                }
            }

        /*
         * additional XFolder setup strings
         *
         */

        switch (_bFolderHotkeysInstance)
        {
            case 0:
                xstrcat(pstrSetup, "ACCELERATORS=NO;", 0);
            break;

            case 1:
                xstrcat(pstrSetup, "ACCELERATORS=YES;", 0);
            break;

            // 2 means default
        }

        switch (_bSnapToGridInstance)
        {
            case 0:
                xstrcat(pstrSetup, "SNAPTOGRID=NO;", 0);
            break;

            case 1:
                xstrcat(pstrSetup, "SNAPTOGRID=YES;", 0);
            break;

            // 2 means default
        }

        switch (_bFullPathInstance)
        {
            case 0:
                xstrcat(pstrSetup, "FULLPATH=NO;", 0);
            break;

            case 1:
                xstrcat(pstrSetup, "FULLPATH=YES;", 0);
            break;

            // 2 means default
        }

        switch (_bStatusBarInstance)
        {
            case 0:
                xstrcat(pstrSetup, "STATUSBAR=NO;", 0);
            break;

            case 1:
                xstrcat(pstrSetup, "STATUSBAR=YES;", 0);
            break;

            // 2 means default
        }

        if (_xwpIsFavoriteFolder(somSelf))
            xstrcat(pstrSetup, "FAVORITEFOLDER=YES;", 0);

        /*
         * append string
         *
         */

        /* if (strTemp.ulLength)
        {
            // return string if buffer is given
            if ((pszSetupString) && (cbSetupString))
                strhncpy0(pszSetupString,   // target
                          strTemp.psz,      // source
                          cbSetupString);   // buffer size

            // always return length of string
            ulReturn = strTemp.ulLength;
        }

        xstrClear(&strTemp); */
    }
    CATCH(excpt1)
    {
        brc = FALSE;
    } END_CATCH();

    return (brc);
}

/* ******************************************************************
 *
 *   Folder view helpers
 *
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

    /* if (_somIsA(somSelf, _WPRootFolder))
        // for disk/root folder views: root folders have no
        // open view, instead the disk object is registered
        // to have the open view. Duh. So we need to find
        // the disk object first
        somSelf2 = _xwpclsQueryDiskObject(_XFldDisk, somSelf);
    else */
        somSelf2 = somSelf;

    if (somSelf2)
    {
        if (_wpFindUseItem(somSelf2, USAGE_OPENVIEW, NULL))
        {
            // folder has an open view;
            // now we go search the open views of the folder and get the
            // frame handle of the desired view (ulView)
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
 *
 *   Full path in title
 *
 ********************************************************************/

/*
 *@@ fdrSetOneFrameWndTitle:
 *      this changes the window title of a given folder frame window
 *      to the full path of the folder, according to the global and/or
 *      folder settings.
 *
 *@@changed V0.9.0 [umoeller]: moved this func here from xfldr.c
 *@@changed V0.9.4 (2000-08-02) [umoeller]: added "keep title" instance setting
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
                if (pNextSlash = strchr(pSrchSlash, '\\'))
                {
                    strcpy(pFirstSlash+4, pNextSlash);
                    pFirstSlash[1] = '.';
                    pFirstSlash[2] = '.';
                    pFirstSlash[3] = '.';
                    pSrchSlash = pFirstSlash+5;
                }
                else
                    break;
            }
        }

        // now either append the full path in brackets to or replace window title
        if (    (_bKeepTitleInstance == 1)
             || ((_bKeepTitleInstance == 2) && (pGlobalSettings->KeepTitle))
           ) // V0.9.4 (2000-08-02) [umoeller]
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
 *@@changed V0.9.2 (2000-03-04) [umoeller]: this didn't work for multiple identical views
 *@@changed V0.9.2 (2000-03-06) [umoeller]: added object mutex protection
 */

BOOL fdrUpdateAllFrameWndTitles(WPFolder *somSelf)
{
    // HWND        hwndFrame = NULLHANDLE;
    BOOL        brc = FALSE;

    WPSHLOCKSTRUCT Lock = {0};
    TRY_LOUD(excpt1)
    {
        if (LOCK_OBJECT(Lock, somSelf))
        {
            PUSEITEM    pUseItem = NULL;
            for (pUseItem = _wpFindUseItem(somSelf, USAGE_OPENVIEW, NULL);
                 pUseItem;
                 pUseItem = _wpFindUseItem(somSelf, USAGE_OPENVIEW, pUseItem))
            {
                PVIEWITEM pViewItem = (PVIEWITEM)(pUseItem+1);
                if (    (pViewItem->view == OPEN_CONTENTS)
                     || (pViewItem->view == OPEN_DETAILS)
                     || (pViewItem->view == OPEN_TREE)
                   )
                {
                    fdrSetOneFrameWndTitle(somSelf,
                                           pViewItem->handle); // hwndFrame);
                    break;
                }
            }
        } // end if fFolderLocked
    }
    CATCH(excpt1) {} END_CATCH();

    if (Lock.fLocked)
        _wpReleaseObjectMutexSem(Lock.pObject);

    ntbUpdateVisiblePage(somSelf, SP_XFOLDER_FLDR);

    return (brc);
}

/* ******************************************************************
 *
 *   Quick Open
 *
 ********************************************************************/

/*
 *@@ fdrQuickOpen:
 *      implementation for the "Quick open" feature.
 *      This populates the specified folder and loads
 *      the icons for all its objects.
 *
 *      This gets called on the Worker thread upon
 *      WOM_QUICKOPEN, but can be called separately
 *      as well. For example, XFolder::wpSetup calls
 *      this on "QUICKOPEN=IMMEDIATE" now.
 *
 *@@added V0.9.6 (2000-10-16) [umoeller]
 */

BOOL fdrQuickOpen(WPFolder *pFolder,
                  PFNCBQUICKOPEN pfnCallback,
                  ULONG ulCallbackParam)
{
    BOOL        brc = TRUE;
    WPObject    *pObject = NULL;
    ULONG       ulNow = 0,
                ulMax = 0;
    BOOL        fFolderLocked = FALSE;

    // ULONG       ulNesting = 0;

    // pre-resolve _wpQueryContent for speed V0.9.3 (2000-04-28) [umoeller]
    // somTD_WPFolder_wpQueryContent rslv_wpQueryContent
            // = SOM_Resolve(pFolder, WPFolder, wpQueryContent);

    // populate folder
    fdrCheckIfPopulated(pFolder,
                        FALSE);        // full populate

    // DosEnterMustComplete(&ulNesting);

    TRY_LOUD(excpt1)
    {
        // lock folder contents
        fFolderLocked = !fdrRequestFolderMutexSem(pFolder, 5000);

        // count objects
        // V0.9.16 (2001-11-01) [umoeller]: now using wpshGetNextObjPointer
        for (   pObject = _wpQueryContent(pFolder, NULL, (ULONG)QC_FIRST);
                (pObject);
                pObject = *wpshGetNextObjPointer(pObject)
            )
        {
            ulMax++;
        }

        // collect icons for all objects
        // V0.9.16 (2001-11-01) [umoeller]: now using wpshGetNextObjPointer
        for (   pObject = _wpQueryContent(pFolder, NULL, (ULONG)QC_FIRST);
                (pObject);
                pObject = *wpshGetNextObjPointer(pObject)
            )
        {
            // get the icon
            _wpQueryIcon(pObject);

            if (pfnCallback)
            {
                // callback
                brc = pfnCallback(pFolder,
                                  pObject,
                                  ulNow,
                                  ulMax,
                                  ulCallbackParam);
                if (!brc)
                    break;
            }
            ulNow++;
        }
    }
    CATCH(excpt1)
    {
        brc = FALSE;
    } END_CATCH();

    if (fFolderLocked)
        fdrReleaseFolderMutexSem(pFolder);

    // DosExitMustComplete(&ulNesting);

    return (brc);
}

/* ******************************************************************
 *
 *   Snap To Grid
 *
 ********************************************************************/

/*
 *@@ fdrSnapToGrid:
 *      makes all objects in the folder "snap" on a grid whose
 *      coordinates are to be defined in the GLOBALSETTINGS.
 *
 *      This function checks if an Icon view of the folder is
 *      currently open; if not and fNotify == TRUE, it displays
 *      a message box.
 *
 *@@changed V0.9.0 [umoeller]: this used to be an instance method (xfldr.c)
 */

BOOL fdrSnapToGrid(WPFolder *somSelf,
                   BOOL fNotify)
{
    HWND                hwndFrame = 0,
                        hwndCnr = 0;
    PMINIRECORDCORE     pmrc = 0;
    LONG                lNewX = 0,
                        lNewY = 0;
    BOOL                brc = FALSE;
    // if Shift is pressed, move all the objects, otherwise
    // only the selected ones
    BOOL                fShiftPressed = doshQueryShiftState();

    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    // first we need the frame handle of a currently open icon view;
    // all others don't make sense
    if (hwndFrame = wpshQueryFrameFromView(somSelf, OPEN_CONTENTS))
    {

        // now get the container handle
        if (hwndCnr = wpshQueryCnrFromFrame(hwndFrame))
        {
            // now begin iteration over the folder's objects; we don't
            // use the WPS method (wpQueryContent) because this is too
            // slow. Instead, we query the container directly.

            _Pmpf((__FUNCTION__ ": x= %d, y = %d, cx = %d, cy = %d",
                        pGlobalSettings->GridX,
                        pGlobalSettings->GridY,
                        pGlobalSettings->GridCX,
                        pGlobalSettings->GridCY));

            pmrc = NULL;
            do
            {
                if (fShiftPressed)
                    // shift pressed: move all objects, so loop
                    // thru the whole container content
                    pmrc
                        = (PMINIRECORDCORE)WinSendMsg(hwndCnr,
                                                      CM_QUERYRECORD,
                                                      (MPARAM)pmrc,  // NULL at first loop
                                                      MPFROM2SHORT(
                                                          (pmrc)
                                                              ? CMA_NEXT  // not first loop: get next object
                                                              : CMA_FIRST, // first loop: get first objecct
                                                          CMA_ITEMORDER)
                                                      );
                else
                    // shift _not_ pressed: move selected objects
                    // only, so loop thru these objects
                    pmrc
                        = (PMINIRECORDCORE)WinSendMsg(hwndCnr,
                                                      CM_QUERYRECORDEMPHASIS,
                                                      (pmrc)   // NULL at first loop
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
                    lNewX = ( ( (   (pmrc->ptlIcon.x - pGlobalSettings->GridX)
                                  + (pGlobalSettings->GridCX / 2)
                                ) / pGlobalSettings->GridCX
                              ) * pGlobalSettings->GridCX
                            ) + pGlobalSettings->GridX;
                    lNewY = ( ( (   (pmrc->ptlIcon.y - pGlobalSettings->GridY)
                                  + (pGlobalSettings->GridCY / 2)
                                ) / pGlobalSettings->GridCY
                              ) * pGlobalSettings->GridCY
                            ) + pGlobalSettings->GridY;

                    // update the record core
                    if ( (lNewX) && (lNewX != pmrc->ptlIcon.x) )
                        pmrc->ptlIcon.x = lNewX;         // X
                    if ( (lNewY) && (lNewY != pmrc->ptlIcon.y) )
                        pmrc->ptlIcon.y = lNewY;         // Y

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

    return (brc);
}

/* ******************************************************************
 *
 *   Status bars
 *
 ********************************************************************/

/*
 *@@ fdrCreateStatusBar:
 *      depending on fShow, this creates or destroys the folder status
 *      bar for a certain folder.
 *
 *      This gets called in the following situations:
 *      a)   from fdrManipulateNewView, when a folder view is
 *           created which needs a status bar;
 *      b)   later if status bar visibility settings are changed
 *           either for the folder or globally.
 *
 *      Parameters:
 *      --  psfv:      pointer to SUBCLASSEDFOLDERVIEW structure of
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
                        PSUBCLASSEDFOLDERVIEW psli2,
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
                                          cmnGetString(ID_XSSI_POPULATING),
                                                // "Collecting objects..."
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
                ULONG   ulView = wpshQueryView(psli2->somSelf,
                                               psli2->hwndFrame);

                // set up window data (QWL_USER) for status bar
                PSTATUSBARDATA psbd = malloc(sizeof(STATUSBARDATA));
                psbd->somSelf    = somSelf;
                psbd->psfv       = psli2;
                psbd->habStatusBar = WinQueryAnchorBlock(psli2->hwndStatusBar);
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
                                        (PSZ)WPINIAPP_FOLDERPOS,
                                        szFolderPosKey,
                                        &ulIni,
                                        &cbIni) == FALSE)
                    ulIni = 0;

                if (ulIni & (   (ulView == OPEN_CONTENTS) ? VIEW_CONTENTS
                              : (ulView == OPEN_TREE) ? VIEW_TREE
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
                    ulIni |= (   (ulView == OPEN_CONTENTS) ? VIEW_CONTENTS
                               : (ulView == OPEN_TREE) ? VIEW_TREE
                               : VIEW_DETAILS
                             );
                    PrfWriteProfileData(HINI_USER,
                                        (PSZ)WPINIAPP_FOLDERPOS,
                                        szFolderPosKey,
                                        &ulIni,
                                        sizeof(ulIni));
                }
                else
                    // if the folder frame has been inflated already,
                    // WM_FORMATFRAME _will_ have to scroll the container.
                    // We do this for icon views only.
                    if (ulView == OPEN_CONTENTS)
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
                ULONG   ulView = wpshQueryView(psli2->somSelf,
                                               psli2->hwndFrame);

                psli2->hwndStatusBar = 0;
                WinSendMsg(psli2->hwndFrame, WM_UPDATEFRAME, (MPARAM)0, MPNULL);
                WinDestroyWindow(hwndStatus);

                // decrease the size of the frame window by the status bar height,
                // if we did this before
                sprintf(szFolderPosKey, "%d@XFSB",
                        _wpQueryHandle(psli2->pRealObject));
                cbIni = sizeof(ulIni);
                if (PrfQueryProfileData(HINI_USER,
                                        (PSZ)WPINIAPP_FOLDERPOS,     // "PM_Workplace:FolderPos"
                                        szFolderPosKey,
                                        &ulIni,
                                        &cbIni) == FALSE)
                    ulIni = 0;

                if (ulIni & (   (ulView == OPEN_CONTENTS) ? VIEW_CONTENTS
                              : (ulView == OPEN_TREE) ? VIEW_TREE
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

                    ulIni &= ~(   (ulView == OPEN_CONTENTS) ? VIEW_CONTENTS
                                : (ulView == OPEN_TREE) ? VIEW_TREE
                                : VIEW_DETAILS
                              );
                    PrfWriteProfileData(HINI_USER,
                                        (PSZ)WPINIAPP_FOLDERPOS,     // "PM_Workplace:FolderPos"
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

    if (mpFolder != cmnQueryActiveDesktop())
    {
        PSUBCLASSEDFOLDERVIEW psfv = fdrQuerySFV(hwndView, NULL);
        if (psfv)
        {
            if (ulActivate == 2) // "update" flag
                WinPostMsg(psfv->hwndSupplObject,
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
#ifndef __NOCFGSTATUSBARS__
                                    (cmnIsFeatureEnabled(StatusBars))
                                &&
#endif
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

                if ( fVisible != (psfv->hwndStatusBar != NULLHANDLE) )
                {
                    // visibility changed:
                    // we now post a message to the folder frame's
                    // supplementary object window;
                    // because we cannot mess with the windows on
                    // the Worker thread, we need to have the
                    // status bar created/destroyed/changed on the
                    // folder's thread
                    WinPostMsg(psfv->hwndSupplObject,
                               SOM_ACTIVATESTATUSBAR,
                               (MPARAM)( (fVisible)
                                            ? 1
                                            : 0 ),      // show or hide
                               (MPARAM)hwndView);
                    mrc = (MPARAM)TRUE;
                }
            }
        }
    }

    return (mrc);
}

/*
 * fncbStatusBarPost:
 *      this posts a message to each view's status bar if it exists.
 *      This is a callback to fdrForEachOpenInstanceView, which gets
 *      called from XFolder::wpAddToContent and XFolder::wpDeleteFromContent.
 *
 *@@added V0.9.6 (2000-10-26) [pr]
 */

MRESULT EXPENTRY fncbStatusBarPost(HWND hwndView,        // folder frame
                                   ULONG msg,            // message
                                   MPARAM mpView,        // OPEN_xxx flag
                                   MPARAM mpFolder)      // folder object
{
    PSUBCLASSEDFOLDERVIEW psfv;

    if (   ((ULONG) mpView == OPEN_CONTENTS)
        || ((ULONG) mpView == OPEN_DETAILS)
        || ((ULONG) mpView == OPEN_TREE)
       )
    {
        psfv = fdrQuerySFV(hwndView, NULL);
        if (psfv && psfv->hwndStatusBar)
            WinPostMsg (psfv->hwndStatusBar, msg, MPNULL, MPNULL);
    }

    return ((MPARAM) TRUE);
}

/*
 *@@ fdrCallResolvedUpdateStatusBar:
 *      resolves XFolder::xwpUpdateStatusBar via name-lookup
 *      resolution on pFolder and, if found, calls that method
 *      implementation.
 *
 *      Always use this function instead of calling the method
 *      directly... because that would probably lead to confusion.
 *
 *@@added V0.9.7 (2001-01-13) [umoeller]
 */

VOID fdrCallResolvedUpdateStatusBar(WPFolder *pFolder,
                                    HWND hwndStatusBar,
                                    HWND hwndCnr)
{
    BOOL fObjectInitialized = _wpIsObjectInitialized(pFolder);
    XFolderData *somThis = XFolderGetData(pFolder);
    somTD_XFolder_xwpUpdateStatusBar pfnResolvedUpdateStatusBar = NULL;

    // do SOM name-lookup resolution for xwpUpdateStatusBar
    // (which will per default find the XFolder method, V0.9.0)
    // if we have not done that for this instance already;
    // we store the method pointer in the instance data
    // for speed
    if (fObjectInitialized) // V0.9.3 (2000-04-29) [umoeller]
        if (_pfnResolvedUpdateStatusBar)
            // already resolved: get resolved address
            // from instance data
            pfnResolvedUpdateStatusBar
                = (somTD_XFolder_xwpUpdateStatusBar)_pfnResolvedUpdateStatusBar;

    if (!pfnResolvedUpdateStatusBar)
    {
        // not resolved yet:
        pfnResolvedUpdateStatusBar
                = (somTD_XFolder_xwpUpdateStatusBar)somResolveByName(
                                          pFolder,
                                          "xwpUpdateStatusBar");
        if (fObjectInitialized)
            // object initialized:
            _pfnResolvedUpdateStatusBar = (PVOID)pfnResolvedUpdateStatusBar;
    }

    // finally, compose the text by calling
    // the resolved method
    if (pfnResolvedUpdateStatusBar)
        pfnResolvedUpdateStatusBar(pFolder,
                                   hwndStatusBar,
                                   hwndCnr);
    else
        WinSetWindowText(hwndStatusBar,
                         "*** error in name-lookup resolution");
}

/*
 *@@ fdrUpdateStatusBars:
 *      updates the status bars of all open views of the
 *      specified folder.
 *
 *@@added V0.9.7 (2001-01-13) [umoeller]
 */

VOID fdrUpdateStatusBars(WPFolder *pFolder)
{
    if (_wpFindUseItem(pFolder, USAGE_OPENVIEW, NULL))
    {
        // folder has an open view;
        // now we go search the open views of the folder and get the
        // frame handle of the desired view (ulView)
        PVIEWITEM   pViewItem;
        for (pViewItem = _wpFindViewItem(pFolder, VIEW_ANY, NULL);
             pViewItem;
             pViewItem = _wpFindViewItem(pFolder, VIEW_ANY, pViewItem))
        {
            switch (pViewItem->view)
            {
                case OPEN_CONTENTS:
                case OPEN_DETAILS:
                case OPEN_TREE:
                {
                    HWND hwndStatusBar = WinWindowFromID(pViewItem->handle, 0x9001);
                    if (hwndStatusBar)
                        fdrCallResolvedUpdateStatusBar(pFolder,
                                                       hwndStatusBar,
                                                       wpshQueryCnrFromFrame(pViewItem->handle));
                }
            }
        }
    }
}

/* ******************************************************************
 *
 *   XFolder window procedures
 *
 ********************************************************************/

/*
 *@@ StatusTimer:
 *
 *@@added V0.9.16 (2001-10-28) [umoeller]
 *@@changed V0.9.16 (2001-10-28) [umoeller]: fixed bad view check for tree views
 *@@changed V0.9.16 (2001-10-28) [umoeller]: fixed bad excpt handler cleanup
 */

VOID StatusTimer(HWND hwndBar,
                 PSTATUSBARDATA psbd)
{
    TRY_LOUD(excpt1)
    {
        // XFolderData *somThis = XFolderGetData(psbd->somSelf);
        BOOL fUpdate = TRUE;

        // stop timer (it's just for one shot)
        WinStopTimer(psbd->habStatusBar, // anchor block,
                     hwndBar,
                     1);
        psbd->idTimer = 0;

        // if we're not fully populated yet, start timer again and quit;
        // otherwise we would display false information.
        // We need an extra flag in the status bar data because the
        // FOI_POPULATEDWITHALL is reset to 0 by the WPS for some reason
        // when an object is deleted from an open folder, and no further
        // status bar updates would occur then
        if (psbd->fFolderPopulated == FALSE)
        {
            ULONG   ulFlags = _wpQueryFldrFlags(psbd->somSelf);
            ULONG   ulView = wpshQueryView(// psbd->psfv->somSelf,
                                           // V0.9.16 (2001-10-28) [umoeller]: we
                                           // can't use somSelf because root folders
                                           // never hold the view information... use
                                           // the "real object" instead, which, for
                                           // root folders, holds the disk object
                                           psbd->psfv->pRealObject,
                                           psbd->psfv->hwndFrame);
            #ifdef DEBUG_STATUSBARS
                PCSZ pcszView;
                CHAR szView[100];
                switch (ulView)
                {
                    case OPEN_TREE: pcszView = "Tree"; break;
                    case OPEN_CONTENTS: pcszView = "Contents"; break;
                    case OPEN_DETAILS: pcszView = "Details"; break;
                    default:
                        sprintf(szView, "unknown %d", ulView);
                        pcszView = szView;
                }
                fdrDebugDumpFolderFlags(psbd->somSelf);
                _Pmpf(( "  View: %s", pcszView));
            #endif

            // for tree views, check if folder is populated with folders;
            // otherwise check for populated with all
            if (    (   (ulView == OPEN_TREE)
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
                psbd->idTimer = WinStartTimer(psbd->habStatusBar, // anchor block
                                              hwndBar,
                                              1,
                                              300);   // this time, use 300 ms
                // and stop
                // break;       // whoa, this didn't deregister the excpt handler
                                // V0.9.16 (2001-10-28) [umoeller]
                fUpdate = FALSE;
            }
        }

        if (fUpdate)
            // OK:
            fdrCallResolvedUpdateStatusBar(psbd->somSelf,
                                           hwndBar,
                                           psbd->psfv->hwndCnr);
    }
    CATCH(excpt1)
    {
        WinSetWindowText(hwndBar, "*** error composing text");
    } END_CATCH();

}

/*
 *@@ StatusPaint:
 *
 *@@added V0.9.16 (2001-10-28) [umoeller]
 */

VOID StatusPaint(HWND hwndBar)
{
    // preparations:
    HPS     hps = WinBeginPaint(hwndBar, NULLHANDLE, NULL);
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    TRY_LOUD(excpt1)
    {
        RECTL   rclBar,
                rclPaint;
        POINTL  ptl1;
        PSZ     pszText;
        CHAR    szTemp[100] = "0";
        USHORT  usLength;
        LONG    lNextX;
        PSZ     p1, p2, p3;
        LONG    lHiColor = WinQuerySysColor(HWND_DESKTOP, SYSCLR_BUTTONLIGHT, 0),
                lLoColor = WinQuerySysColor(HWND_DESKTOP, SYSCLR_BUTTONDARK, 0);

        WinQueryWindowRect(hwndBar,
                           &rclBar);        // exclusive
        // switch to RGB mode
        gpihSwitchToRGB(hps);

        // 1) draw background
        WinFillRect(hps,
                    &rclBar,                // exclusive
                    pGlobalSettings->lSBBgndColor);

        rclPaint.xLeft = rclBar.xLeft;
        rclPaint.yBottom = rclBar.yBottom;
        rclPaint.xRight = rclBar.xRight - 1;
        rclPaint.yTop = rclBar.yTop - 1;

        // 2) draw 3D frame in selected style
        if (pGlobalSettings->SBStyle == SBSTYLE_WARP3RAISED)
            // Warp 3 style, raised
            gpihDraw3DFrame(hps,
                            &rclPaint,
                            1,
                            lHiColor,
                            lLoColor);
        else if (pGlobalSettings->SBStyle == SBSTYLE_WARP3SUNKEN)
            // Warp 3 style, sunken
            gpihDraw3DFrame(hps,
                            &rclPaint,
                            1,
                            lLoColor,
                            lHiColor);
        else if (pGlobalSettings->SBStyle == SBSTYLE_WARP4MENU)
        {
            // Warp 4 menu style: draw 3D line at top only
            rclPaint.yBottom = rclPaint.yTop - 1;
            gpihDraw3DFrame(hps,
                            &rclPaint,
                            1,
                            lLoColor,
                            lHiColor);
        }
        else
        {
            // Warp 4 button style
            // draw "sunken" outer rect
            gpihDraw3DFrame(hps,
                            &rclPaint,
                            2,
                            lLoColor,
                            lHiColor);
            // draw "raised" inner rect
            rclPaint.xLeft++;
            rclPaint.yBottom++;
            rclPaint.xRight--;
            rclPaint.yTop--;
            gpihDraw3DFrame(hps,
                            &rclPaint,
                            2,
                            lHiColor,
                            lLoColor);
        }

        // 3) start working on text; we do "simple" GpiCharString
        //    if no tabulators are defined, but calculate some
        //    subrectangles otherwise
        pszText = winhQueryWindowText(hwndBar);
                // pszText now has the translated status bar text
                // except for the tabulators ("$x" keys)
        if (pszText)
        {
            p1 = pszText;
            p2 = NULL;
            ptl1.x = 7;

            do  // while tabulators are present
            {
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
                    }
                    else
                        p2 = NULL;
                }
                else
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
                    RECTL rcl;
                    rcl.xLeft   = 0;
                    rcl.yBottom = 0; // ptl1.y;
                    rcl.xRight  = lNextX-10; // 10 pels space
                    rcl.yTop    = rclBar.yTop;
                    GpiCharStringPosAt(hps,
                                       &ptl1,
                                       &rcl,
                                       CHS_CLIP,
                                       usLength,
                                       p1,
                                       NULL);
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

            free(pszText);
        } // end if (pszText)
    }
    CATCH(excpt1)
    {
        PSZ pszErr = "*** error painting status bar";
        POINTL ptl = {5, 5};
        GpiMove(hps, &ptl);
        GpiCharString(hps, strlen(pszErr), pszErr);
    } END_CATCH();

    WinEndPaint(hps);
}

/*
 *@@ StatusPresParamChanged:
 *
 *@@added V0.9.16 (2001-10-28) [umoeller]
 */

VOID StatusPresParamChanged(HWND hwndBar,
                            PSTATUSBARDATA psbd,
                            MPARAM mp1)
{
    if (psbd->fDontBroadcast)
    {
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
        return;
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
            GLOBALSETTINGS* pGlobalSettings = NULL;
            // ULONG ulNesting;
            // DosEnterMustComplete(&ulNesting);
            TRY_LOUD(excpt1)
            {
                ULONG   ul = 0,
                        attrFound = 0;
                pGlobalSettings = cmnLockGlobalSettings(__FILE__, __LINE__, __FUNCTION__);

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
            }
            CATCH(excpt1) {} END_CATCH();

            if (pGlobalSettings)
                cmnUnlockGlobalSettings();

            // DosExitMustComplete(&ulNesting);

            cmnStoreGlobalSettings();

            WinPostMsg(hwndBar, STBM_UPDATESTATUSBAR, MPNULL, MPNULL);
        break; }
    }

    // finally, broadcast this message to all other status bars;
    // this is handled by the Worker thread
    xthrPostWorkerMsg(WOM_UPDATEALLSTATUSBARS,
                      (MPARAM)2,      // update display
                      MPNULL);
}

/*
 *@@ fdr_fnwpStatusBar:
 *      since the status bar is just created as a static frame
 *      control, it is subclassed (by fdrCreateStatusBar),
 *      and this is the wnd proc for this.
 *
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
    PSTATUSBARDATA psbd = (PSTATUSBARDATA)WinQueryWindowPtr(hwndBar, QWL_USER);
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
                    // only if timer is not yet running: start it now
                    psbd->idTimer = WinStartTimer(psbd->habStatusBar, // anchor block
                                                  hwndBar,
                                                  1,
                                                  100); // delay: 100 ms
            break;

            /*
             * WM_TIMER:
             *      a timer is started by STBM_UPDATESTATUSBAR
             *      to avoid flickering;
             *      we now compose the actual text to be displayed
             */

            case WM_TIMER:
                if ((ULONG)mp1 == 1)
                    StatusTimer(hwndBar, psbd);
                else
                    mrc = pfnwpStatusBarOriginal(hwndBar, msg, mp1, mp2);
            break;

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
                StatusPaint(hwndBar);
            break;

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
                psbd->fDontBroadcast = TRUE;
            break;

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
                mrc = pfnwpStatusBarOriginal(hwndBar, msg, mp1, mp2);
                StatusPresParamChanged(hwndBar, psbd, mp1);
            break;

            /*
             * WM_BUTTON1CLICK:
             *      if button1 is clicked on the status
             *      bar, we should reset the focus to the
             *      container.
             */

            case WM_BUTTON1CLICK:
                // mrc = (MRESULT)(*pfnwpStatusBarOriginal)(hwndBar, msg, mp1, mp2);
                WinSetFocus(HWND_DESKTOP, psbd->psfv->hwndCnr);
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
                WinPostMsg(psbd->psfv->hwndFrame,
                           WM_COMMAND,
                           (MPARAM)WPMENUID_PROPERTIES,
                           MPFROM2SHORT(CMDSRC_MENU,
                                   FALSE) );     // results from keyboard operation
            break;

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
                if (psbd->psfv)
                {
                    POINTL  ptl;
                    ptl.x = SHORT1FROMMP(mp1);
                    ptl.y = SHORT2FROMMP(mp1)-20; // this is in cnr coords!
                    _wpDisplayMenu(psbd->somSelf,
                                   psbd->psfv->hwndFrame, // hwndFolderView, // owner
                                   psbd->psfv->hwndCnr, // NULLHANDLE, // hwndFolderView, // parent
                                   &ptl,
                                   MENU_OPENVIEWPOPUP, // was: MENU_OBJECTPOPUP, V0.9.0
                                   0);
                }
            break;

            /*
             * WM_DESTROY:
             *      call original wnd proc, then free psbd
             */

            case WM_DESTROY:
                mrc = pfnwpStatusBarOriginal(hwndBar, msg, mp1, mp2);
                WinSetWindowULong(hwndBar, QWL_USER, 0);
                free(psbd);
            break;

            default:
                mrc = pfnwpStatusBarOriginal(hwndBar, msg, mp1, mp2);

        } // end switch
    } // end if (psbd)

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

            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
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
                                        if (doshMatch(szMask, pmrc->pszIcon))
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
                                                                    (pmrc)
                                                                        ? CMA_NEXT
                                                                        : CMA_FIRST,
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
                 */

                default:
                    mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
                break;
            }
        break; }

        case WM_HELP:
            cmnDisplayHelp(NULL,
                           ID_XFH_SELECTSOME);
        break;

        /*
         * WM_CLOSE:
         *
         */

        case WM_CLOSE:
            fWriteAndClose = TRUE;
        break;

        default:
            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
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
                            0,
                            &cbToSave);
        }

        if (cbToSave)
            PrfWriteProfileData(HINI_USER,
                                (PSZ)INIAPP_XWORKPLACE,
                                (PSZ)INIKEY_LAST10SELECTSOME, // "SelectSome"
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

            if (!stricmp(p, poli->szIdentity))
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
 *
 *   Start folder contents
 *
 ********************************************************************/

/*
 *@@ PROCESSFOLDER:
 *      structure on the stack of fdrStartFolderContents
 *      while the "start folder contents" thread is
 *      running synchronously.
 *
 *@@added V0.9.12 (2001-04-29) [umoeller]
 */

typedef struct _PROCESSFOLDER
{
    // input parameters:
    WPFolder        *pFolder;
    ULONG           ulTiming;
    HWND            hwndStatus;             // status window or NULLHANDLE

    BOOL            fCancelled;

    // private data:
    WPObject        *pObject;
    ULONG           henum;
    ULONG           cTotalObjects;
    ULONG           ulObjectThis;
    ULONG           ulFirstTime;            // sysinfo first time
} PROCESSFOLDER, *PPROCESSFOLDER;

/*
 *@@ fnwpStartupDlg:
 *      dlg proc for the Startup status window, which
 *      runs on the main PM thread (fnwpThread1Object).
 */

MRESULT EXPENTRY fnwpStartupDlg(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc;

    switch (msg)
    {
        case WM_INITDLG:
        {
            // WinSetWindowULong(hwnd, QWL_USER, (ULONG)mp2);
                // we don't need this here, it's done by fnwpThread1Object
            ctlProgressBarFromStatic(WinWindowFromID(hwnd, ID_SDDI_PROGRESSBAR),
                                     PBA_ALIGNCENTER | PBA_BUTTONSTYLE);
            mrc = WinDefDlgProc(hwnd, msg, mp1, mp2);
        break; }

        case WM_COMMAND:
            switch (SHORT1FROMMP(mp1))
            {
                case DID_CANCEL:
                {
                    PPROCESSFOLDER ppf = (PPROCESSFOLDER)WinQueryWindowPtr(hwnd, QWL_USER);
                    if (ppf)
                        ppf->fCancelled = TRUE;
                break; }
            }
        break;

        case WM_SYSCOMMAND:
        {
            switch (SHORT1FROMMP(mp1))
            {
                case SC_CLOSE:
                case SC_HIDE:
                {
                    GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(__FILE__, __LINE__, __FUNCTION__);
                    if (pGlobalSettings)
                    {
                        pGlobalSettings->ShowStartupProgress = 0;
                        cmnUnlockGlobalSettings();
                        cmnStoreGlobalSettings();
                    }
                    mrc = WinDefDlgProc(hwnd, msg, mp1, mp2);
                break; }

                default:
                    mrc = WinDefDlgProc(hwnd, msg, mp1, mp2);
            }
        break; }

        default:
            mrc = WinDefDlgProc(hwnd, msg, mp1, mp2);
    }

    return (mrc);
}

/*
 *@@ fntProcessStartupFolder:
 *      synchronous thread started from fdrStartFolderContents
 *      for each startup folder that is to be processed.
 *
 *@@added V0.9.12 (2001-04-29) [umoeller]
 */

void _Optlink fntProcessStartupFolder(PTHREADINFO ptiMyself)
{
    PPROCESSFOLDER      ppf = (PPROCESSFOLDER)ptiMyself->ulData;
    WPFolder            *pFolder = ppf->pFolder;
    PCGLOBALSETTINGS    pGlobalSettings = cmnQueryGlobalSettings();

    while (!ppf->fCancelled)
    {
        BOOL    fOKGetNext = FALSE;
        HWND    hwndCurrentView = NULLHANDLE;       // for wait mode

        if (ppf->ulObjectThis == 0)
        {
            // first iteration: initialize structure
            ppf->cTotalObjects = 0;
            fdrCheckIfPopulated(pFolder,
                                FALSE);        // full populate
            // now count objects
            for (   ppf->pObject = _wpQueryContent(pFolder, NULL, QC_FIRST);
                    (ppf->pObject);
                    ppf->pObject = _wpQueryContent(pFolder, ppf->pObject, QC_NEXT)
                )
            {
                ppf->cTotalObjects++;
            }

            // get first object
            ppf->henum = _xwpBeginEnumContent(pFolder);
        }

        if (ppf->henum)
        {
            // in any case, get first or next object
            ppf->pObject = _xwpEnumNext(pFolder, ppf->henum);

            ppf->ulObjectThis++;

            // now process that object
            if (ppf->pObject)
            {
                // this is not the last object: start it

                // resolve shadows... this never worked for
                // shadows V0.9.12 (2001-04-29) [umoeller]
                if (_somIsA(ppf->pObject, _WPShadow))
                    ppf->pObject = _wpQueryShadowedObject(ppf->pObject, TRUE);

                if (wpshCheckObject(ppf->pObject))
                {
                    // open the object:

                    // 1) update the status window
                    if (pGlobalSettings->ShowStartupProgress)
                    {
                        CHAR szStarting2[500], szTemp[500];
                        // update status text ("Starting xxx")
                        strcpy(szTemp, _wpQueryTitle(ppf->pObject));
                        strhBeautifyTitle(szTemp);
                        sprintf(szStarting2,
                                cmnGetString(ID_SDSI_STARTING), // ->pszStarting,
                                szTemp);
                        WinSetDlgItemText(ppf->hwndStatus, ID_SDDI_STATUS, szStarting2);
                    }

                    // have the object opened on thread-1
                    hwndCurrentView = (HWND)krnSendThread1ObjectMsg(T1M_OPENOBJECTFROMPTR,
                                                                    (MPARAM)ppf->pObject,
                                                                    (MPARAM)OPEN_DEFAULT);

                    // update status bar
                    if (pGlobalSettings->ShowStartupProgress)
                        WinSendDlgItemMsg(ppf->hwndStatus, ID_SDDI_PROGRESSBAR,
                                          WM_UPDATEPROGRESSBAR,
                                          MPFROMLONG(ppf->ulObjectThis),
                                          MPFROMLONG(ppf->cTotalObjects));
                }

                ppf->ulFirstTime = doshQuerySysUptime();
            }
            else
                // no more objects:
                // break out of the while loop
                break;
        }
        else
            // error:
            break;

        // now wait until we should process the
        // next object
        while (    (!fOKGetNext)
                && (!ppf->fCancelled)
              )
        {
            if (ppf->ulTiming == 0)
            {
                // "wait for close" mode:
                // check if the view we opened is still alive

                // now, for all XFolder versions up to now
                // we used wpWaitForClose... I am very unsure
                // what this method does, so I have now replaced
                // this with my own loop here.
                // V0.9.12 (2001-04-28) [umoeller]
                BOOL fStillOpen = FALSE;
                WPSHLOCKSTRUCT Lock = {0};

                TRY_LOUD(excpt1)
                {
                    if (LOCK_OBJECT(Lock, ppf->pObject))
                    {
                        PUSEITEM    pUseItem = NULL;

                        #ifdef DEBUG_STARTUP
                            _Pmpf(("  WOM_WAITFORPROCESSNEXT: checking open views"));
                            _Pmpf(("  obj %s, hwnd 0x%lX",
                                        _wpQueryTitle(ppf->pObject),
                                        ppf->hwndView));
                        #endif

                        for (pUseItem = _wpFindUseItem(ppf->pObject, USAGE_OPENVIEW, NULL);
                             pUseItem;
                             pUseItem = _wpFindUseItem(ppf->pObject, USAGE_OPENVIEW, pUseItem))
                        {
                            PVIEWITEM pvi = (PVIEWITEM)(pUseItem + 1);

                            #ifdef DEBUG_STARTUP
                                _Pmpf(("    got view 0x%lX", pvi->handle));
                            #endif

                            if (pvi->handle == hwndCurrentView)
                            {
                                fStillOpen = TRUE;
                                break;
                            }
                        }
                    }
                }
                CATCH(excpt1) {} END_CATCH();

                if (Lock.fLocked)
                    _wpReleaseObjectMutexSem(Lock.pObject);

                fOKGetNext = !fStillOpen;
            } // end if (ppf->ulTiming == 0)
            else
            {
                // timing mode
                ULONG ulNowTime = doshQuerySysUptime();
                if (ulNowTime > (ppf->ulFirstTime + ppf->ulTiming))
                    fOKGetNext = TRUE;
            }

            // fOKGetNext is TRUE if the next object should be
            // processed now
            if (fOKGetNext)
            {
                // removed lock here V0.9.12 (2001-04-28) [umoeller]
                PCKERNELGLOBALS pKernelGlobals = krnQueryGlobals();
                if (pKernelGlobals)
                {
                    // ready to go for next object:
                    // make sure the damn PM hard error windows are not visible,
                    // because this locks any other PM activity
                    if (    (WinIsWindowVisible(pKernelGlobals->hwndHardError))
                         || (WinIsWindowVisible(pKernelGlobals->hwndSysError))
                       )
                    {
                        DosBeep(250, 100);
                        // wait a little more and try again
                        fOKGetNext = FALSE;
                    }
                }
            }

            if (!fOKGetNext)
                // not ready yet:
                // sleep awhile (we could simply sleep for the
                // wait time, but then "cancel" would not be
                // very responsive)
                DosSleep(100);
        }
    } // end while !ppf->fCancelled

    // done or cancelled:
    _xwpEndEnumContent(pFolder, ppf->henum);

    // tell thrRunSync that we're done
    WinPostMsg(ptiMyself->hwndNotify,
               WM_USER,
               0, 0);
}

/*
 *@@ fdrStartFolderContents:
 *      implementation for XFolder::xwpStartFolderContents.
 *
 *@@added V0.9.12 (2001-04-29) [umoeller]
 *@@changed V0.9.13 (2001-06-27) [umoeller]: now setting status title to folder's
 */

ULONG fdrStartFolderContents(WPFolder *pFolder,
                             ULONG ulTiming)
{
    ULONG ulrc = 0;

    PROCESSFOLDER       pf;
    PCGLOBALSETTINGS    pGlobalSettings = cmnQueryGlobalSettings();

    memset(&pf, 0, sizeof(pf));

    pf.ulTiming = ulTiming;

    pf.hwndStatus = WinLoadDlg(HWND_DESKTOP, NULLHANDLE,
                               fnwpStartupDlg,
                               cmnQueryNLSModuleHandle(FALSE),
                               ID_XFD_STARTUPSTATUS,
                               NULL);
    // store struct in window words so the dialog can cancel
    WinSetWindowPtr(pf.hwndStatus, QWL_USER, &pf);

    // set title V0.9.13 (2001-06-27) [umoeller]
    WinSetWindowText(pf.hwndStatus, _wpQueryTitle(pFolder));

    if (pGlobalSettings->ShowStartupProgress)
    {
        // get last window position from INI
        winhRestoreWindowPos(pf.hwndStatus,
                             HINI_USER,
                             INIAPP_XWORKPLACE, INIKEY_WNDPOSSTARTUP,
                             // move only, no resize
                             SWP_MOVE | SWP_SHOW | SWP_ACTIVATE);
    }

    pf.pFolder = pFolder;

    ulrc = thrRunSync(WinQueryAnchorBlock(pf.hwndStatus),
                      fntProcessStartupFolder,
                      "ProcessStartupFolder",
                      (ULONG)&pf);

    winhSaveWindowPos(pf.hwndStatus, HINI_USER, INIAPP_XWORKPLACE, INIKEY_WNDPOSSTARTUP);
    WinDestroyWindow(pf.hwndStatus);

    return (ulrc);
}


