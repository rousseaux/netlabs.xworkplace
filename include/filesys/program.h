
/*
 *@@sourcefile program.h:
 *      header file for program.c (program implementation).
 *
 *      This file is ALL new with V0.9.6.
 *
 *@@include #define INCL_WINPROGRAMLIST     // for progStartApp
 *@@include #include <os2.h>
 *@@include #include <wpfsys.h>             // for progOpenProgram
 *@@include #include "filesys\program.h"
 */

/*
 *      Copyright (C) 2000 Ulrich M”ller.
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

#ifndef PROGRAM_HEADER_INCLUDED
    #define PROGRAM_HEADER_INCLUDED

    /* ******************************************************************
     *
     *   Helpers
     *
     ********************************************************************/

    BOOL progQuerySetup(WPObject *somSelf,
                        PVOID pstrSetup);

    ULONG progIsProgramOrProgramFile(WPObject *somSelf);

    #ifdef INCL_WINPROGRAMLIST
        PPROGDETAILS progQueryDetails(WPObject *pProgObject);

        BOOL progFillProgDetails(PPROGDETAILS pProgDetails,
                                 ULONG ulProgType,
                                 ULONG fbVisible,
                                 PSWP pswpInitial,
                                 PCSZ pcszTitle,
                                 PCSZ pcszExecutable,
                                 USHORT usStartupDirHandle,
                                 PCSZ pcszParameters,
                                 PCSZ pcszEnvironment,
                                 PULONG pulSize);
    #endif

    #ifdef EXEH_HEADER_INCLUDED

        ULONG progQueryProgType(PCSZ pszFullFile,
                                PVOID pvExec);

        APIRET progFindIcon(PEXECUTABLE pExec,
                            ULONG ulAppType,
                            HPOINTER *phptr,
                            PICONINFO pIconInfo,
                            PBOOL pfNotDefaultIcon);
    #endif

    /* ******************************************************************
     *
     *   Running programs database
     *
     ********************************************************************/

    #ifdef SOM_WPDataFile_h
    BOOL progStoreRunningApp(WPObject *pProgram,
                             WPFileSystem *pArgDataFile,
                             HAPP happ,
                             ULONG ulMenuID);
    #endif

    BOOL progAppTerminateNotify(HAPP happ);

    BOOL progRunningAppDestroyed(WPObject *pObjEmphasis);

    /* ******************************************************************
     *
     *   Run programs
     *
     ********************************************************************/

    APIRET progOpenProgramThread1(PVOID pvData);

    #ifdef SOM_WPDataFile_h
    APIRET progOpenProgram(WPObject *pProgObject,
                           WPFileSystem *pArgDataFile,
                           ULONG ulMenuID,
                           HAPP *phapp,
                           ULONG cbFailingName,
                           PSZ pszFailingName);
    #endif

    /* ******************************************************************
     *
     *   Method redefinitions
     *
     ********************************************************************/

    // if both WPProgram and WPProgramFile are included, we
    // better redefine the method macros to call the correct
    // methods
    // V0.9.16 (2001-12-08) [umoeller]

    #if ( defined(SOM_WPProgramFile_h) && defined(SOM_WPProgram_h) )
        // handy macro for redefining a method name
        #define REDEFINEMETHOD(name, pobj)                                      \
            ((somTD_WPProgram_ ## name)(                                        \
                _somIsA(pobj, _WPProgramFile)                                   \
                ? somResolve(pobj,                                              \
                             WPProgramFileClassData.name)                       \
                : somResolve(pobj,                                              \
                             WPProgramClassData.name)                           \
            ))

        // now the actual method redefinitions:
        #undef _wpQueryAssociationFilter
        #define _wpQueryAssociationFilter(pobj)                                 \
            REDEFINEMETHOD(wpQueryAssociationFilter, pobj)(pobj)

        #undef _wpQueryAssociationType
        #define _wpQueryAssociationType(pobj)                                   \
            REDEFINEMETHOD(wpQueryAssociationType, pobj)(pobj)

        #undef _wpQueryProgDetails
        #define _wpQueryProgDetails(pobj, pd, pul)                              \
            REDEFINEMETHOD(wpQueryProgDetails, pobj)(pobj, pd, pul)

        #undef _wpSetAssociationFilter
        #define _wpSetAssociationFilter(pobj, psz)                              \
            REDEFINEMETHOD(wpSetAssociationFilter, pobj)(pobj, psz)

        #undef _wpSetAssociationType
        #define _wpSetAssociationType(pobj, psz)                                \
            REDEFINEMETHOD(wpSetAssociationType, pobj)(pobj, psz)

        #undef _wpSetProgDetails
        #define _wpSetProgDetails(pobj, p)                                      \
            REDEFINEMETHOD(wpSetProgDetails, pobj)(pobj, p)
    #endif

    /* ******************************************************************
     *
     *   XWPProgramFile notebook callbacks (notebook.c)
     *
     ********************************************************************/

    PCSZ progGetWinResourceTypeName(ULONG ulTypeThis);

    PCSZ progGetOS2ResourceTypeName(ULONG ulResourceType);

#ifdef NOTEBOOK_HEADER_INCLUDED
#ifndef __NOMODULEPAGES__
        VOID XWPENTRY progFileInitPage(PCREATENOTEBOOKPAGE pcnbp,
                                       ULONG flFlags);

        VOID XWPENTRY progResourcesInitPage(PCREATENOTEBOOKPAGE pcnbp,
                                            ULONG flFlags);

        BOOL XWPENTRY progResourcesMessage(PCREATENOTEBOOKPAGE pcnbp,
                                           ULONG msg, MPARAM mp1, MPARAM mp2,
                                           MRESULT *pmrc);

        VOID XWPENTRY progFile1InitPage(PCREATENOTEBOOKPAGE pcnbp,
                                        ULONG flFlags);

        VOID XWPENTRY progFile2InitPage(PCREATENOTEBOOKPAGE pcnbp,
                                        ULONG flFlags);
#endif
#endif

#endif
