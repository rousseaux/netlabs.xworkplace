
/*
 *@@sourcefile icons.h:
 *      header file for icons.c.
 *
 *      This file is ALL new with V0.9.16.
 *
 *@@include #include <os2.h>
 *@@include #include <wpobject.h>   // or any other WPS SOM header
 *@@include #include "shared\notebook.h"    // for notebook headers
 *@@include #include "filesys\icon.h"
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

#ifndef ICONS_HEADER_INCLUDED
    #define ICONS_HEADER_INCLUDED

    /* ******************************************************************
     *
     *   Icon data handling
     *
     ********************************************************************/

    APIRET icoBuildPtrHandle(PBYTE pbData,
                             HPOINTER *phptr);

    APIRET icoLoadICOFile(PCSZ pcszFilename,
                          HPOINTER *phptr,
                          PULONG pcbIconData,
                          PBYTE pbIconData);

    APIRET icoBuildPtrFromFEA2List(PFEA2LIST pFEA2List,
                                   HPOINTER *phptr,
                                   PULONG pcbIconData,
                                   PBYTE pbIconData);

    APIRET icoBuildPtrFromEAs(PCSZ pcszFilename,
                              HPOINTER *phptr,
                              PULONG pcbIconInfo,
                              PICONINFO pIconInfo);

    #ifdef EXEH_HEADER_INCLUDED
        APIRET icoLoadExeIcon(PEXECUTABLE pExec,
                              ULONG idResource,
                              HPOINTER *phptr,
                              PULONG pcbIconData,
                              PBYTE pbIconData);
    #endif

    /* ******************************************************************
     *
     *   Object icon management
     *
     ********************************************************************/

    BOOL icomLockIconShares(VOID);

    VOID icomUnlockIconShares(VOID);

    HPOINTER icomShareIcon(WPObject *somSelf,
                           WPObject *pobjClient,
                           BOOL fMakeGlobal);

    VOID icomUnShareIcon(WPObject *pobjServer,
                         WPObject *pobjClient);

    BOOL icomRunReplacement(VOID);

    ULONG icomClsQueryMaxAnimationIcons(M_WPObject *somSelf);

    HPOINTER icomQueryIconN(WPObject *pobj,
                            ULONG ulIndex);

    ULONG icomQueryIconDataN(WPObject *pobj,
                             ULONG ulIndex,
                             PICONINFO pData);

    BOOL icomSetIconDataN(WPObject *pobj,
                          ULONG ulIndex,
                          PICONINFO pData);

    HPOINTER icoClsQueryIconN(SOMClass *pClassObject,
                              ULONG ulIndex);

    APIRET icomLoadIconData(WPObject *pobj,
                            ULONG ulIndex,
                            PICONINFO *ppIconInfo);

    APIRET icomCopyIconFromObject(WPObject *somSelf,
                                  WPObject *pobjSource,
                                  ULONG ulIndex);

    VOID icomResetIcon(WPObject *somSelf,
                       ULONG ulIndex);

    BOOL icomIsUsingDefaultIcon(WPObject *pobj,
                                ULONG ulAnimationIndex);

    /* ******************************************************************
     *
     *   Notebook callbacks (notebook.c) for new XFldObject "Icon" page
     *
     ********************************************************************/

    #ifdef NOTEBOOK_HEADER_INCLUDED

        #define ICONFL_TITLE            0x0001
        #define ICONFL_ICON             0x0002
        #define ICONFL_TEMPLATE         0x0004
        #define ICONFL_LOCKEDINPLACE    0x0008
        #define ICONFL_HOTKEY           0x0010
        #define ICONFL_DETAILS          0x0020

        VOID icoFormatIconPage(PNOTEBOOKPAGE pnbp,
                               ULONG flFlags);

        VOID XWPENTRY icoIcon1InitPage(PNOTEBOOKPAGE pnbp,
                                       ULONG flFlags);

        MRESULT XWPENTRY icoIcon1ItemChanged(PNOTEBOOKPAGE pnbp,
                                             ULONG ulItemID, USHORT usNotifyCode,
                                             ULONG ulExtra);
    #endif

    /* ******************************************************************
     *
     *   Additional WPObject method prototypes
     *
     ********************************************************************/

    /*
     *@@ xfTP_wpclsQueryMaxAnimationIcons:
     *      prototype for M_WPObject::wpclsQueryMaxAnimationIcons.
     *
     *      That class method is undocumented and simply returns
     *      the no. of animation icons supported by objects of
     *      this class. From my testing, this returns 0 unless
     *      somSelf is a folder class object, but who knows. We
     *      should rather use that method instead of just checking
     *      for whether an object is a folder to determine whether
     *      animation icons are supported.
     *
     *@@added V0.9.19 (2002-06-15) [umoeller]
     */

    typedef ULONG _System xfTP_wpclsQueryMaxAnimationIcons(M_WPObject*);
    typedef xfTP_wpclsQueryMaxAnimationIcons *xfTD_wpclsQueryMaxAnimationIcons;

    /* ******************************************************************
     *
     *   Additional WPFolder method prototypes
     *
     ********************************************************************/

    #ifdef SOM_WPFolder_h

        /*
         *@@ xfTP_wpQueryIconN:
         *      prototype for WPFolder::wpQueryIconN.
         *
         *@@added V0.9.16 (2001-10-15) [umoeller]
         */

        typedef HPOINTER _System xfTP_wpQueryIconN(WPFolder *somSelf,
                                                   ULONG ulIndex);
        typedef xfTP_wpQueryIconN *xfTD_wpQueryIconN;

        /*
         *@@ xfTP_wpQueryIconDataN:
         *      prototype for WPFolder::wpQueryIconDataN.
         *
         *@@added V0.9.16 (2001-10-15) [umoeller]
         */

        typedef ULONG _System xfTP_wpQueryIconDataN(WPFolder *somSelf,
                                                    PICONINFO pData,
                                                    ULONG ulIndex);
        typedef xfTP_wpQueryIconDataN *xfTD_wpQueryIconDataN;


        /*
         *@@ xfTP_wpSetIconDataN:
         *      prototype for WPFolder::wpSetIconDataN.
         *
         *@@added V0.9.16 (2001-10-15) [umoeller]
         */

        typedef BOOL _System xfTP_wpSetIconDataN(WPFolder *somSelf,
                                                 PICONINFO pData,
                                                 ULONG ulIndex);
        typedef xfTP_wpSetIconDataN *xfTD_wpSetIconDataN;


        typedef HPOINTER _System xfTP_wpclsQueryIconN(M_WPFolder *somSelf,
                                                      ULONG ulIndex);
        typedef xfTP_wpclsQueryIconN *xfTD_wpclsQueryIconN;

    #endif // SOM_WPFolder_h

#endif


