
/*
 *@@sourcefile fonts.h:
 *      header file for fonts.c (font folder implementation).
 *
 *      This file is ALL new with V0.9.7.
 *
 *@@include #include <os2.h>
 *@@include #include "classes\xfont.h"
 *@@include #include "config\fonts.h"
 */

/*
 *      Copyright (C) 2001 Ulrich M”ller.
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

#ifndef FONTS_HEADER_INCLUDED
    #define FONTS_HEADER_INCLUDED

    /* ******************************************************************
     *
     *   General font management
     *
     ********************************************************************/

    APIRET fonGetFontDescription(HAB hab,
                                 const char *pcszFilename,
                                 PSZ pszFamily,
                                 PSZ pszFace);

    #if defined (SOM_XWPFontFile_h) && defined (SOM_XWPFontFolder_h)
        APIRET fonInstallFont(HAB hab,
                              XWPFontFolder *pFontFolder,
                              XWPFontFile *pNewFontFile,
                              WPObject **ppNewFontObj);
    #endif

    #if defined (SOM_XWPFontObject_h) && defined (SOM_XWPFontFolder_h)
        APIRET fonDeInstallFont(HAB hab,
                                XWPFontFolder *pFontFolder,
                                XWPFontObject *pFontObject);
    #endif

    /* ******************************************************************
     *
     *   Font folder implementation
     *
     ********************************************************************/

    #ifdef SOM_XWPFontFolder_h
        BOOL fonFillWithFontObjects(XWPFontFolder *pFontFolder);

        MRESULT fonDragOver(XWPFontFolder *pFontFolder,
                            PDRAGINFO pdrgInfo);

        MRESULT fonDrop(XWPFontFolder *pFontFolder,
                        PDRAGINFO pdrgInfo);

        BOOL fonProcessObjectCommand(WPFolder *somSelf,
                                     USHORT usCommand,
                                     HWND hwndCnr,
                                     WPObject* pFirstObject,
                                     ULONG ulSelectionFlags);
    #endif

    /* ******************************************************************
     *
     *   Font object implementation
     *
     ********************************************************************/

    #ifdef SOM_XWPFontObject_h
        VOID fonModifyFontPopupMenu(XWPFontObject *somSelf,
                                    HWND hwndMenu);

        HWND fonCreateFontSampleView(XWPFontObject *somSelf,
                                     HAB hab,
                                     ULONG ulView);
    #endif
#endif


