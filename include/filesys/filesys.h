
/*
 *@@sourcefile filesys.h:
 *      header file for filesys.c (extended file types implementation).
 *
 *      This file is ALL new with V0.9.0.
 *
 *@@include #include <os2.h>
 *@@include #include "shared\notebook.h"
 *@@include #include "filesys\filesys.h"
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

#ifndef FILESYS_HEADER_INCLUDED
    #define FILESYS_HEADER_INCLUDED

    /* ******************************************************************
     *
     *   File system information implementation
     *
     ********************************************************************/

    #ifdef SOM_WPFileSystem_h

        PSZ fsysQueryEASubject(WPFileSystem *somSelf);

        PSZ fsysQueryEAComments(WPFileSystem *somSelf);

        PSZ fsysQueryEAKeyphrases(WPFileSystem *somSelf);

        BOOL fsysSetEASubject(WPFileSystem *somSelf, const char *psz);

        BOOL fsysSetEAComments(WPFileSystem *somSelf, const char *psz);

        BOOL fsysSetEAKeyphrases(WPFileSystem *somSelf, const char *psz);

    #endif

    /* ******************************************************************
     *
     *   "File" pages replacement in WPDataFile/WPFolder
     *
     ********************************************************************/

    #ifdef NOTEBOOK_HEADER_INCLUDED
        VOID fsysFile1InitPage(PCREATENOTEBOOKPAGE pcnbp,
                               ULONG flFlags);

        MRESULT fsysFile1ItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                     ULONG ulItemID,
                                     USHORT usNotifyCode,
                                     ULONG ulExtra);

        VOID fsysFile2InitPage(PCREATENOTEBOOKPAGE pcnbp,
                               ULONG flFlags);

        MRESULT fsysFile2ItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                     ULONG ulItemID,
                                     USHORT usNotifyCode,
                                     ULONG ulExtra);

        ULONG fsysInsertFilePages(WPObject *somSelf,
                                  HWND hwndNotebook);

    /* ******************************************************************
     *
     *   XFldProgramFile notebook callbacks (notebook.c)
     *
     ********************************************************************/

        VOID fsysProgramInitPage(PCREATENOTEBOOKPAGE pcnbp,
                                 ULONG flFlags);

        VOID fsysResourcesInitPage(PCREATENOTEBOOKPAGE pcnbp,
                                   ULONG flFlags);

        VOID fsysProgram1InitPage(PCREATENOTEBOOKPAGE pcnbp,
                                 ULONG flFlags);

        VOID fsysProgram2InitPage(PCREATENOTEBOOKPAGE pcnbp,
                                 ULONG flFlags);

    #endif

    #ifdef XSTRING_HEADER_INCLUDED
    VOID fsysQueryProgramSetup(WPObject *somSelf,
                               PXSTRING pstrTemp);
    #endif

    ULONG fsysQueryProgramFileSetup(WPObject *somSelf,
                                    PSZ pszSetupString,
                                    ULONG cbSetupString);

#endif


