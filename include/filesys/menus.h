
/*
 *@@sourcefile menus.h:
 *      header file for menus.c.
 *
 *      This file is new with V0.81. The function prototypes in
 *      this file used to be in common.h and have now been exported
 *      to make their context more lucid.
 *
 *      Some declarations for menus.c are still in common.h however.
 *
 *@@include #define INCL_WINMENUS
 *@@include #include <os2.h>
 *@@include #include <wpfolder.h> // WPFolder
 *@@include #include <wppgm.h> // WPProgram, for some funcs only
 *@@include #include <wpobject.h> // only if other WPS headers are not included
 *@@include #include "shared\common.h"
 *@@include #include "shared\notebook.h" // for menu notebook callback prototypes
 *@@include #include "filesys\menus.h"
 */

/*
 *      Copyright (C) 1997-2002 Ulrich M”ller.
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

#ifndef MENUS_HEADER_INCLUDED
    #define MENUS_HEADER_INCLUDED

    #ifndef INCL_WINMENUS
        #error "INCL_WINMENUS needs to be #define'd before including menus.h"
    #endif

    /* ******************************************************************
     *
     *   Declarations
     *
     ********************************************************************/

    #define CLIPBOARDKEY "%**C"        /* code in program object's parameter list for
                                          inserting clipboard data */

    /* ******************************************************************
     *
     *   Functions for manipulating context menus
     *
     ********************************************************************/

    VOID mnuCheckDefaultSortItem(PCGLOBALSETTINGS pGlobalSettings,
                                 HWND hwndSortMenu,
                                 ULONG ulDefaultSort);

    VOID mnuModifySortMenu(WPFolder *somSelf,
                           HWND hwndMenu,
                           PCGLOBALSETTINGS pGlobalSettings,
                           PNLSSTRINGS pNLSStrings);

    BOOL mnuInsertFldrViewItems(WPFolder *somSelf,
                                HWND hwndViewSubmenu,
                                BOOL fInsertNewMenu,
                                HWND hwndCnr,
                                ULONG ulView);

    BOOL mnuAppendMi2List(WPObject *pObject, USHORT usObjType);

    VOID mnuAppendFldr2ContentList(WPFolder *pFolder, SHORT sMenuId);

    ULONG mnuInsertOneObjectMenuItem(HWND       hAddToMenu,
                                     USHORT     iPosition,
                                     PSZ        pszNewItemString,
                                     USHORT     afStyle,
                                     WPObject   *pObject,
                                     ULONG      usObjType);

    SHORT mnuPrepareContentSubmenu(WPFolder *somSelf,
                                   HWND hwndMenu,
                                   PSZ pszTitle,
                                   USHORT iPosition,
                                   BOOL fOwnerDraw);

    SHORT EXPENTRY fncbSortContentMenuItems(PVOID pItem1,
                                            PVOID pItem2,
                                            PVOID hab);

    VOID mnuFillContentSubmenu(SHORT sMenuId,
                               HWND hwndMenu,
                               PFNWP *ppfnwpFolderContentOriginal);

    VOID mnuInvalidateConfigCache(VOID);

    BOOL mnuModifyFolderPopupMenu(WPFolder *somSelf,
                                  HWND hwndMenu,
                                  HWND hwndCnr,
                                  ULONG iPosition);

    #ifdef SOM_WPDataFile_h
        BOOL mnuModifyDataFilePopupMenu(WPDataFile *somSelf,
                                        HWND hwndMenu,
                                        HWND hwndCnr,
                                        ULONG iPosition);
    #endif

    /* ******************************************************************
     *
     *   Functions for reacting to menu selections
     *
     ********************************************************************/

    BOOL mnuIsSortMenuItemSelected(WPFolder *somSelf,
                                   HWND hwndFrame,
                                   HWND hwndMenu,
                                   ULONG ulMenuId,
                                   PCGLOBALSETTINGS pGlobalSettings,
                                   PBOOL pbDismiss);

    VOID mnuCreateFromTemplate(WPObject *pTemplate,
                               WPFolder *pFolder);

    BOOL mnuMenuItemSelected(WPFolder *somSelf, HWND hwndFrame, ULONG ulMenuId);

    BOOL mnuMenuItemHelpSelected(WPObject *somSelf, ULONG MenuId);

    /* ******************************************************************
     *
     *   "Selecting" menu items functions
     *
     ********************************************************************/

    BOOL mnuFileSystemSelectingMenuItem(WPObject *somSelf,
                                        USHORT usItem,
                                        BOOL fPostCommand,
                                        HWND hwndMenu,
                                        HWND hwndCnr,
                                        ULONG ulSelection,
                                        BOOL *pfDismiss);

    BOOL mnuFolderSelectingMenuItem(WPFolder *somSelf,
                                    USHORT usItem,
                                    BOOL fPostCommand,
                                    HWND hwndMenu,
                                    HWND hwndCnr,
                                    ULONG ulSelection,
                                    BOOL *pfDismiss);

    /* ******************************************************************
     *
     *   Functions for folder content menu ownerdraw
     *
     ********************************************************************/

    VOID mnuPrepareOwnerDraw(SHORT sMenuIDMsg,
                             HWND hwndMenuMsg);

    MRESULT mnuMeasureItem(POWNERITEM poi,
                           PCGLOBALSETTINGS pGlobalSettings);

    BOOL mnuDrawItem(PCGLOBALSETTINGS pGlobalSettings,
                     MPARAM mp1, MPARAM mp2);

    /* ******************************************************************
     *
     *   Notebook callbacks (notebook.c) for XFldWPS "Menu" pages
     *
     ********************************************************************/

    #ifdef NOTEBOOK_HEADER_INCLUDED
        VOID XWPENTRY mnuAddMenusInitPage(PNOTEBOOKPAGE pnbp,
                                          ULONG flFlags);

        MRESULT XWPENTRY mnuAddMenusItemChanged(PNOTEBOOKPAGE pnbp,
                                       ULONG ulItemID,
                                       USHORT usNotifyCode,
                                       ULONG ulExtra);

        VOID XWPENTRY mnuConfigFolderMenusInitPage(PNOTEBOOKPAGE pnbp,
                                                   ULONG flFlags);

        MRESULT XWPENTRY mnuConfigFolderMenusItemChanged(PNOTEBOOKPAGE pnbp,
                                                ULONG ulItemID,
                                                USHORT usNotifyCode,
                                                ULONG ulExtra);

        VOID XWPENTRY mnuRemoveMenusInitPage(PNOTEBOOKPAGE pnbp,
                                             ULONG flFlags);

        MRESULT XWPENTRY mnuRemoveMenusItemChanged(PNOTEBOOKPAGE pnbp,
                                          ULONG ulItemID,
                                          USHORT usNotifyCode,
                                          ULONG ulExtra);
    #endif

#endif
