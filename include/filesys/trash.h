
/*
 *@@sourcefile trash.h:
 *      header file for trash.c (trash can).
 *
 *      This file is ALL new with V0.9.1.
 *
 *@@include #include <os2.h>
 *@@include #include <wpobject.h>
 *@@include #include "helpers\linklist.h"
 *@@include #include "helpers\tree.h"       // for mappings
 *@@include #include "shared\notebook.h"
 *@@include #include "classes\xtrash.h"
 *@@include #include "classes\xtrashobj.h"
 *@@include #include "filesys\trash.h"
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

#ifndef TRASH_HEADER_INCLUDED
    #define TRASH_HEADER_INCLUDED

    #ifndef SOM_XWPTrashCan_h
        #error trash.h requires xtrash.h to be included.
    #endif

    #define CB_SUPPORTED_DRIVES     24  // fixed V0.9.4 (2000-06-08) [umoeller]

    #define XTRC_INVALID            0
    #define XTRC_SUPPORTED          1
    #define XTRC_UNSUPPORTED        2

    /* ******************************************************************
     *
     *   Trash dir mappings
     *
     ********************************************************************/

    #ifdef SFLTREE_INCLUDED

        /*
         *@@ TRASHMAPPINGTREENODE:
         *      entry for the linked list in G_allTranslationTables.
         *
         *      Assuming that C:\Documents\Important\myfile.txt
         *      gets moved into the trash can. This would result
         *      in the following:
         *
         *      1) Creation of the hidden C:\TRASH directory,
         *         if that doesn't exist yet.
         *
         *      2) Now, instead of creating "C:\TRASH\Documents\Important"
         *         as we used to do, we create a new dir with a random
         *         name (e.g. "C:\TRASH\01234567") and move myfile.txt
         *         there.
         *
         *      3) We then create a TRASHMAPPINGTREENODE for "C:\TRASH\01234567"
         *         and store "C:\TRASH\Documents\Important" with pcszRealTitle.
         *
         *      If "C:\Documents\Important\myfile2.txt" gets deleted, we
         *      use that same mapping.
         *
         *      By contrast, if the entire folder "C:\Documents\Important"
         *      gets deleted, we do this:
         *
         *      1) Again, creation of the hidden C:\TRASH directory,
         *         if that doesn't exist yet.
         *
         *      2) Create a new dir with a random name (e.g. "C:\TRASH\01234568")
         *         and move "C:\Documents\Important" there.
         *
         *      3) We then create a TRASHMAPPINGTREENODE for "C:\TRASH\01234568"
         *         and store "C:\TRASH\Documents" with pcszRealTitle.
         *
         *      This allows us to keep deleted files and folders separate.
         *
         *@@added V0.9.9 (2001-02-06) [umoeller]
         */

        typedef struct _TRASHMAPPINGTREENODE
        {
            TREE            Tree;

            WPFolder        *pFolderInTrash;
                        // folder in \trash; this is a DIRECT subdirectory
                        // of \trash on each drive and contains the related
                        // objects which were put here. This is ALWAYS HIDDEN.

            PSZ             pszRealName;
                        // full path of where that folder originally was;
                        // for example, if pFolderInTrash is "C:\TRASH\01234567",
                        // this could be "C:\Documents\Important".
                        // This PSZ is allocated using malloc().

        } TRASHMAPPINGTREENODE, *PTRASHMAPPINGTREENODE;

        VOID trshInitMappings(XWPTrashCan *somSelf,
                              PBOOL pfNeedSave);

        PTRASHMAPPINGTREENODE trshGetMapping(XWPTrashCan *pTrashCan,
                                             WPFolder *pFolder);
    #endif

    VOID trshSaveMappings(XWPTrashCan *pTrashCan);

    /* ******************************************************************
     *
     *   Trash can populating
     *
     ********************************************************************/

    XWPTrashObject* trshCreateTrashObject(M_XWPTrashObject *somSelf,
                                          XWPTrashCan* pTrashCan,
                                          WPObject* pRelatedObject);

    BOOL trshSetupOnce(XWPTrashObject *somSelf,
                       PSZ pszSetupString);

    VOID trshCalcTrashObjectSize(XWPTrashObject *pTrashObject,
                                 XWPTrashCan *pTrashCan);

    BOOL trshPopulateFirstTime(XWPTrashCan *somSelf,
                               ULONG ulFldrFlags);

    BOOL trshRefresh(XWPTrashCan *somSelf);

    /* ******************************************************************
     *
     *   Trash can / trash object operations
     *
     ********************************************************************/

    BOOL trshDeleteIntoTrashCan(XWPTrashCan *pTrashCan,
                                WPObject *pObject);

    BOOL trshRestoreFromTrashCan(XWPTrashObject *pTrashObject,
                                 WPFolder *pTargetFolder);

    MRESULT trshDragOver(XWPTrashCan *somSelf,
                         PDRAGINFO pdrgInfo);

    MRESULT trshMoveDropped2TrashCan(XWPTrashCan *somSelf,
                                     PDRAGINFO pdrgInfo);

    ULONG trshEmptyTrashCan(XWPTrashCan *somSelf,
                            HAB hab,
                            HWND hwndConfirmOwner,
                            PULONG pulDeleted);

    APIRET trshValidateTrashObject(XWPTrashObject *somSelf);

    BOOL trshProcessObjectCommand(WPFolder *somSelf,
                                  USHORT usCommand,
                                  HWND hwndCnr,
                                  WPObject* pFirstObject,
                                  ULONG ulSelectionFlags);

    /* ******************************************************************
     *
     *   Trash can drives support
     *
     ********************************************************************/

    BOOL trshSetDrivesSupport(PBYTE pabSupportedDrives);

    BOOL trshQueryDrivesSupport(PBYTE pabSupportedDrives);

    VOID trshLoadDrivesSupport(M_XWPTrashCan *somSelf);

    BOOL trshIsOnSupportedDrive(WPObject *pObject);

    /* ******************************************************************
     *
     *   XWPTrashCan notebook callbacks (notebook.c)
     *
     ********************************************************************/

    #ifdef NOTEBOOK_HEADER_INCLUDED
        VOID trshTrashCanSettingsInitPage(PCREATENOTEBOOKPAGE pcnbp,
                                          ULONG flFlags);

        MRESULT trshTrashCanSettingsItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                                ULONG ulItemID, USHORT usNotifyCode,
                                                ULONG ulExtra);

        VOID trshTrashCanDrivesInitPage(PCREATENOTEBOOKPAGE pcnbp,
                                        ULONG flFlags);

        MRESULT trshTrashCanDrivesItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                              ULONG ulItemID, USHORT usNotifyCode,
                                              ULONG ulExtra);

        VOID trshTrashCanIconInitPage(PCREATENOTEBOOKPAGE pcnbp,
                                      ULONG flFlags);

        MRESULT trshTrashCanIconItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                            ULONG ulItemID, USHORT usNotifyCode,
                                            ULONG ulExtra);
    #endif

#endif


