
/*
 *@@sourcefile fdrnotebooks.c:
 *      implementation for folder-related notebook pages,
 *      both in XFolder instances and in the "Workplace Shell"
 *      object, as far as they're folder-specific.
 *
 *      This file is ALL new with V0.9.3. The code in here used
 *      to be in folder.c.
 *
 *      Function prefix for this file:
 *      --  fdr*
 *
 *@@added V0.9.3 [umoeller]
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
#include <os2.h>

// C library headers
#include <stdio.h>              // needed for except.h
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\winh.h"               // PM helper routines

// SOM headers which don't crash with prec. header files
#include "xfldr.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\notebook.h"            // generic XWorkplace notebook handling

#include "filesys\folder.h"             // XFolder implementation
#include "filesys\xthreads.h"           // extra XWorkplace threads

// other SOM headers
#pragma hdrstop                     // VAC++ keeps crashing otherwise

#include <wpdesk.h>

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


