
/*
 *@@sourcefile filesys.h:
 *      header file for filetype.c (extended file types implementation).
 *
 *      This file is ALL new with V0.9.0.
 *
 *@@include #include <os2.h>
 *@@include #include <wpdataf.h>                    // WPDataFile
 *@@include #include "helpers\linklist.h"
 *@@include #include "shared\notebook.h"            // for notebook callbacks
 *@@include #include "filesys\filetype.h"
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

#ifndef FILETYPE_HEADER_INCLUDED
    #define FILETYPE_HEADER_INCLUDED

    /* ******************************************************************
     *
     *   XFldDataFile extended associations
     *
     ********************************************************************/

    VOID ftypInvalidateCaches(VOID);

    ULONG ftypAssocObjectDeleted(HOBJECT hobj);

    #if defined (SOM_XFldDataFile_h) && defined (LINKLIST_HEADER_INCLUDED)
        PLINKLIST ftypBuildAssocsList(WPDataFile *somSelf,
                                      BOOL fUsePlainTextAsDefault);

        ULONG ftypFreeAssocsList(PLINKLIST *ppllAssocs);

        WPObject* ftypQueryAssociatedProgram(WPDataFile *somSelf,
                                             PULONG pulView,
                                             BOOL fUsePlainTextAsDefault);

        BOOL ftypModifyDataFileOpenSubmenu(WPDataFile *somSelf,
                                           HWND hwndOpenSubmenu,
                                           BOOL fDeleteExisting);
    #endif

    #ifdef NOTEBOOK_HEADER_INCLUDED

        /* ******************************************************************
         *
         *   Notebook callbacks (notebook.c) for XFldWPS "File types" page
         *
         ********************************************************************/

        extern MPARAM *G_pampFileTypesPage;
        extern ULONG G_cFileTypesPage;

        VOID XWPENTRY ftypFileTypesInitPage(PCREATENOTEBOOKPAGE pcnbp,
                                            ULONG flFlags);

        MRESULT XWPENTRY ftypFileTypesItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                         ULONG ulItemID,
                                         USHORT usNotifyCode,
                                         ULONG ulExtra);

        MRESULT EXPENTRY fnwpImportWPSFilters(HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2);

        /* ******************************************************************
         *
         *   XFldDataFile notebook callbacks (notebook.c)
         *
         ********************************************************************/

        extern MPARAM *G_pampDatafileTypesPage;
        extern ULONG G_cDatafileTypesPage;

        VOID XWPENTRY ftypDatafileTypesInitPage(PCREATENOTEBOOKPAGE pcnbp,
                                                ULONG flFlags);

        MRESULT XWPENTRY ftypDatafileTypesItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                             ULONG ulItemID,
                                             USHORT usNotifyCode,
                                             ULONG ulExtra);

        /* ******************************************************************
         *
         *   XWPProgram/XFldProgramFile notebook callbacks (notebook.c)
         *
         ********************************************************************/

        ULONG ftypInsertAssociationsPage(WPObject *somSelf,
                                         HWND hwndNotebook);

        VOID XWPENTRY ftypAssociationsInitPage(PCREATENOTEBOOKPAGE pcnbp,
                                               ULONG flFlags);

        MRESULT XWPENTRY ftypAssociationsItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                            ULONG ulItemID,
                                            USHORT usNotifyCode,
                                            ULONG ulExtra);
    #endif

    /* ******************************************************************
     *
     *   Import/Export facility
     *
     ********************************************************************/

    APIRET ftypImportTypes(const char *pcszFilename);

    APIRET ftypExportTypes(const char *pcszFileName);

#endif


