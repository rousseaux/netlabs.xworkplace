
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
#define INCL_WINSTDSLIDER
#define INCL_WINSHELLDATA       // Prf* functions
#include <os2.h>

// C library headers
#include <stdio.h>              // needed for except.h
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\threads.h"            // thread helpers
#include "helpers\winh.h"               // PM helper routines

// SOM headers which don't crash with prec. header files
#include "xfldr.ih"
#include "xfstart.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling

#include "filesys\folder.h"             // XFolder implementation
#include "filesys\xthreads.h"           // extra XWorkplace threads

// other SOM headers
#pragma hdrstop                     // VAC++ keeps crashing otherwise

// #include <wpdesk.h>

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
 *@@changed V0.9.4 (2000-06-09) [umoeller]: added default documents
 *@@changed V0.9.9 (2001-02-06) [umoeller]: added folder auto-refresh
 *@@changed V0.9.12 (2001-04-30) [umoeller]: added default folder views
 */

VOID fdrViewInitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                     ULONG flFlags)        // CBI_* flags (notebook.h)
{
    PCKERNELGLOBALS pKernelGlobals = krnQueryGlobals();
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
        ULONG ulid;

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

        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_FDRDEFAULTDOC,
                              pGlobalSettings->fFdrDefaultDoc);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_FDRDEFAULTDOCVIEW,
                              pGlobalSettings->fFdrDefaultDocView);

        if (pKernelGlobals->fAutoRefreshReplaced)
            winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_FDRAUTOREFRESH,
                                  !pGlobalSettings->fFdrAutoRefreshDisabled);

        // folder default views V0.9.12 (2001-04-30) [umoeller]
        switch (pGlobalSettings->bDefaultFolderView)
        {
            case OPEN_CONTENTS: ulid = ID_XSDI_FDRVIEW_ICON; break;
            case OPEN_TREE:     ulid = ID_XSDI_FDRVIEW_TREE; break;
            case OPEN_DETAILS:  ulid = ID_XSDI_FDRVIEW_DETAILS; break;

            default: /* 0 */ ulid = ID_XSDI_FDRVIEW_INHERIT; break;
        }

        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ulid, TRUE);
    }

    if (flFlags & CBI_ENABLE)
    {
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_TREEVIEWAUTOSCROLL,
                (    (pGlobalSettings->NoWorkerThread == FALSE)
                  && (pGlobalSettings->fNoSubclassing == FALSE)
                ));
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_FDRDEFAULTDOCVIEW,
                         pGlobalSettings->fFdrDefaultDoc);

        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_FDRAUTOREFRESH,
                         pKernelGlobals->fAutoRefreshReplaced);
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
 *@@changed V0.9.4 (2000-06-09) [umoeller]: added default documents
 *@@changed V0.9.9 (2001-02-06) [umoeller]: added folder auto-refresh
 *@@changed V0.9.12 (2001-04-30) [umoeller]: added default folder views
 */

MRESULT fdrViewItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                           ULONG ulItemID, USHORT usNotifyCode,
                           ULONG ulExtra)      // for checkboxes: contains new state
{
    GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(__FILE__, __LINE__, __FUNCTION__);
    MRESULT mrc = (MPARAM)0;
    BOOL fSave = TRUE,
         fUpdate = FALSE;

    // LONG lTemp;

    switch (ulItemID)
    {
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

        case ID_XSDI_FDRDEFAULTDOC:
            pGlobalSettings->fFdrDefaultDoc = ulExtra;
            // update the display by calling the INIT callback
            (*(pcnbp->pfncbInitPage))(pcnbp, CBI_SET | CBI_ENABLE);
        break;

        case ID_XSDI_FDRDEFAULTDOCVIEW:
            pGlobalSettings->fFdrDefaultDocView = ulExtra;
        break;

        case ID_XSDI_FDRAUTOREFRESH:
            pGlobalSettings->fFdrAutoRefreshDisabled = (ulExtra == 0);
        break;

        case ID_XSDI_FDRVIEW_ICON:
            pGlobalSettings->bDefaultFolderView = OPEN_CONTENTS;
        break;

        case ID_XSDI_FDRVIEW_TREE:
            pGlobalSettings->bDefaultFolderView = OPEN_TREE;
        break;

        case ID_XSDI_FDRVIEW_DETAILS:
            pGlobalSettings->bDefaultFolderView = OPEN_DETAILS;
        break;

        case ID_XSDI_FDRVIEW_INHERIT:
            pGlobalSettings->bDefaultFolderView = 0;
        break;


        case DID_UNDO:
        {
            // "Undo" button: get pointer to backed-up Global Settings
            PCGLOBALSETTINGS pGSBackup = (PCGLOBALSETTINGS)(pcnbp->pUser);

            // and restore the settings for this page
            pGlobalSettings->FullPath  = pGSBackup->FullPath ;
            pGlobalSettings->KeepTitle = pGSBackup->KeepTitle;
            pGlobalSettings->TreeViewAutoScroll = pGSBackup->TreeViewAutoScroll;
            pGlobalSettings->MaxPathChars = pGSBackup->MaxPathChars;

            pGlobalSettings->fFdrDefaultDoc = pGSBackup->fFdrDefaultDoc;
            pGlobalSettings->fFdrDefaultDocView = pGSBackup->fFdrDefaultDocView;

            pGlobalSettings->fFdrAutoRefreshDisabled = pGSBackup->fFdrAutoRefreshDisabled;

            pGlobalSettings->bDefaultFolderView = pGSBackup->bDefaultFolderView;

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
                           ULONG ulItemID,
                           USHORT usNotifyCode,
                           ULONG ulExtra)      // for checkboxes: contains new state
{
    GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(__FILE__, __LINE__, __FUNCTION__);
    MRESULT mrc = (MPARAM)0;
    BOOL fSave = TRUE;

    switch (ulItemID)
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
 *@@changed V0.9.4 (2000-08-02) [umoeller]: added "keep title" instance setting
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

        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_FULLPATH,
                              (     ((_bFullPathInstance == 2)
                                       ? pGlobalSettings->FullPath
                                       : _bFullPathInstance )
                                 != 0));
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_KEEPTITLE,
                              (     ((_bKeepTitleInstance == 2)
                                       ? pGlobalSettings->KeepTitle
                                       : _bKeepTitleInstance )
                                 != 0));
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_SNAPTOGRID,
                              (     ((_bSnapToGridInstance == 2)
                                       ? pGlobalSettings->fAddSnapToGridDefault
                                       : _bSnapToGridInstance )
                                 != 0));

        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_ACCELERATORS,
                              (     ((_bFolderHotkeysInstance == 2)
                                       ? pGlobalSettings->fFolderHotkeysDefault
                                       : _bFolderHotkeysInstance )
                                 != 0));
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_ENABLESTATUSBAR,
                              (   (     ((_bStatusBarInstance == STATUSBAR_DEFAULT)
                                           ? pGlobalSettings->fDefaultStatusBarVisibility
                                           : _bStatusBarInstance )
                                     != 0)
                                  // always uncheck for Desktop
                                && (pcnbp->somSelf != cmnQueryActiveDesktop())
                              ));
    }

    if (flFlags & CBI_ENABLE)
    {
        // disable items
        winhEnableDlgItem(pcnbp->hwndDlgPage,
                         ID_XSDI_ACCELERATORS,
                         (    !(pGlobalSettings->fNoSubclassing)
                           && (pGlobalSettings->fEnableFolderHotkeys)
                         ));

        winhEnableDlgItem(pcnbp->hwndDlgPage,
                         ID_XSDI_KEEPTITLE,
                         ( (_bFullPathInstance == 2)
                             ? pGlobalSettings->FullPath
                             : _bFullPathInstance ));

        winhEnableDlgItem(pcnbp->hwndDlgPage,
                         ID_XSDI_SNAPTOGRID,  // added V0.9.1 (99-12-28) [umoeller]
                         (pGlobalSettings->fEnableSnap2Grid));

        winhEnableDlgItem(pcnbp->hwndDlgPage,
                         ID_XSDI_ENABLESTATUSBAR,
                         // always disable for Desktop
                         (   (pcnbp->somSelf != cmnQueryActiveDesktop())
                          && (!(pGlobalSettings->fNoSubclassing))
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
 *@@changed V0.9.4 (2000-08-02) [umoeller]: added "keep title" instance setting
 */

MRESULT fdrXFolderItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                              ULONG ulItemID,
                              USHORT usNotifyCode,
                              ULONG ulExtra)      // for checkboxes: contains new state
{
    XFolderData *somThis = XFolderGetData(pcnbp->somSelf);
    BOOL fUpdate = TRUE;

    switch (ulItemID)
    {
        case ID_XSDI_SNAPTOGRID:
            _bSnapToGridInstance = ulExtra;
        break;

        case ID_XSDI_FULLPATH:
            _bFullPathInstance = ulExtra;
            // update the display by calling the INIT callback
            (*(pcnbp->pfncbInitPage))(pcnbp, CBI_SET | CBI_ENABLE);
            fdrUpdateAllFrameWndTitles(pcnbp->somSelf);
        break;

        case ID_XSDI_KEEPTITLE:
            _bKeepTitleInstance = ulExtra;
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
                _bKeepTitleInstance = Backup->bKeepTitleInstance;
                _bSnapToGridInstance = Backup->bSnapToGridInstance;
                _bFolderHotkeysInstance = Backup->bFolderHotkeysInstance;
                _xwpSetStatusBarVisibility(pcnbp->somSelf,
                                           Backup->bStatusBarInstance,
                                           TRUE);  // update open folder views
                // update the display by calling the INIT callback
                (*(pcnbp->pfncbInitPage))(pcnbp, CBI_SET | CBI_ENABLE);
                fdrUpdateAllFrameWndTitles(pcnbp->somSelf);
            }
        break;

        case DID_DEFAULT:
            // "Default" button:
            _bFullPathInstance = 2;
            _bKeepTitleInstance = 2;
            _bSnapToGridInstance = 2;
            _bFolderHotkeysInstance = 2;
            _xwpSetStatusBarVisibility(pcnbp->somSelf,
                        STATUSBAR_DEFAULT,
                        TRUE);  // update open folder views
            // update the display by calling the INIT callback
            (*(pcnbp->pfncbInitPage))(pcnbp, CBI_SET | CBI_ENABLE);
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

/* ******************************************************************
 *
 *   Notebook callbacks (notebook.c) for "Sort" pages
 *
 ********************************************************************/

/*
 *@@ InsertSortItem:
 *
 *@@added V0.9.12 (2001-05-18) [umoeller]
 */

VOID InsertSortItem(HWND hwndListbox,
                    PULONG pulIndex,
                    const char *pcsz,
                    LONG lItemHandle)
{
    WinInsertLboxItem(hwndListbox,
                      *pulIndex,
                      (PSZ)pcsz);
    winhSetLboxItemHandle(hwndListbox,
                          (*pulIndex)++,
                          lItemHandle);
}

/*
 *@@ winhLboxFindItemFromHandle:
 *      finds the list box item with the specified
 *      handle.
 *
 *      Of course this only makes sense if each item
 *      has a unique handle indeed.
 *
 *      Returns the index of the item found or -1.
 *
 *@@added V0.9.12 (2001-05-18) [umoeller]
 */

ULONG winhLboxFindItemFromHandle(HWND hwndListBox,
                                 ULONG ulHandle)
{
    LONG cItems = WinQueryLboxCount(hwndListBox);
    if (cItems)
    {
        ULONG ul;
        for (ul = 0;
             ul < cItems;
             ul++)
        {
            if (ulHandle == winhQueryLboxItemHandle(hwndListBox,
                                                    ul))
                return (ul);
        }
    }

    return (-1);
}

/*
 * fdrSortInitPage:
 *      "Sort" page notebook callback function (notebook.c).
 *      Sets the controls on the page.
 *
 *      The "Sort" callbacks are used both for the folder settings
 *      notebook page AND the respective "Sort" page in the "Workplace
 *      Shell" object, so we need to keep instance data and the
 *      Global Settings apart.
 *
 *      We do this by examining the page ID in the notebook info struct.
 *
 *@@changed V0.9.0 [umoeller]: updated settings page
 *@@changed V0.9.0 [umoeller]: adjusted function prototype
 *@@changed V0.9.0 [umoeller]: moved this func here from xfldr.c
 */

VOID fdrSortInitPage(PCREATENOTEBOOKPAGE pcnbp,
                     ULONG flFlags)
{
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    HWND        hwndListbox = WinWindowFromID(pcnbp->hwndDlgPage,
                                              ID_XSDI_SORTLISTBOX);

    if (pcnbp->ulPageID == SP_FLDRSORT_FLDR)
    {
        // if we're being called from a folder's notebook,
        // get instance data
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
    }

    if (flFlags & CBI_INIT)
    {
        M_WPObject *pSortClass;
        ULONG       ulIndex = 0,
                    cColumns = 0,
                    ul;
        PCLASSFIELDINFO   pcfi;

        if (pcnbp->ulPageID == SP_FLDRSORT_FLDR)
            // instance notebook:
            // get folder's sort class
            pSortClass = _wpQueryFldrSortClass(pcnbp->somSelf);
        else
            // "Workplace Shell" page: always use _WPFileSystem
            pSortClass = _WPFileSystem;

        // 1) insert the new XWP sort criteria
        InsertSortItem(hwndListbox,
                       &ulIndex,
                       cmnGetString(ID_XSSI_SV_NAME),
                       -2);
        InsertSortItem(hwndListbox,
                       &ulIndex,
                       cmnGetString(ID_XSSI_SV_TYPE),
                       -1);
        InsertSortItem(hwndListbox,
                       &ulIndex,
                       cmnGetString(ID_XSSI_SV_CLASS),
                       -3);
        InsertSortItem(hwndListbox,
                       &ulIndex,
                       cmnGetString(ID_XSSI_SV_EXT),
                       -4);

        // 2) next, insert the class-specific sort criteria
        cColumns = _wpclsQueryDetailsInfo(pSortClass,
                                          &pcfi,
                                          NULL);
        for (ul = 0;
             ul < cColumns;
             ul++, pcfi = pcfi->pNextFieldInfo)
        {
            BOOL fSortable = TRUE;

            if (pcnbp->ulPageID == SP_FLDRSORT_FLDR)
                // call this method only if somSelf is really a folder!
                fSortable = _wpIsSortAttribAvailable(pcnbp->somSelf, ul);

            if (    (fSortable)
                    // sortable columns only:
                 && (pcfi->flCompare & SORTBY_SUPPORTED)
                    // rule out the images:
                 && (0 == (pcfi->flTitle & CFA_BITMAPORICON))
               )
            {
                // OK, usable sort column:
                // add this to the list box
                InsertSortItem(hwndListbox,
                               &ulIndex,
                               pcfi->pTitleData,
                               ul);     // column number as handle
            }
        }
    }

    if (flFlags & CBI_SET)
    {
        LONG lDefaultSort,
             lAlwaysSort;
        ULONG ulIndex;

        if (pcnbp->ulPageID == SP_FLDRSORT_FLDR)
        {
            // instance notebook:
            XFolderData *somThis = XFolderGetData(pcnbp->somSelf);
            lDefaultSort = (_lDefaultSort == SET_DEFAULT)
                                ? pGlobalSettings->lDefaultSort
                                : _lDefaultSort;
            lAlwaysSort = (_lAlwaysSort == SET_DEFAULT)
                                  ? pGlobalSettings->AlwaysSort
                                  : _lAlwaysSort;
        }
        else
        {
            // "Workplace Shell":
            lDefaultSort = pGlobalSettings->lDefaultSort;
            lAlwaysSort = pGlobalSettings->AlwaysSort;
        }

        // find the list box entry with the matching handle
        ulIndex = winhLboxFindItemFromHandle(hwndListbox,
                                             lDefaultSort);
        if (ulIndex != -1)
            winhSetLboxSelectedItem(hwndListbox,
                                    ulIndex,
                                    TRUE);

        winhSetDlgItemChecked(pcnbp->hwndDlgPage,
                              ID_XSDI_ALWAYSSORT,
                              lAlwaysSort);
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
                           ULONG ulItemID,
                           USHORT usNotifyCode,
                           ULONG ulExtra)      // for checkboxes: contains new state
{
    BOOL fUpdate = TRUE;

    switch (ulItemID)
    {
        case ID_XSDI_ALWAYSSORT:
        case ID_XSDI_SORTLISTBOX:
        {
            HWND        hwndListbox = WinWindowFromID(pcnbp->hwndDlgPage,
                                                      ID_XSDI_SORTLISTBOX);

            ULONG ulSortIndex = (USHORT)(WinSendMsg(hwndListbox,
                                                    LM_QUERYSELECTION,
                                                    (MPARAM)LIT_CURSOR,
                                                    MPNULL));
            LONG lDefaultSort = winhQueryLboxItemHandle(hwndListbox, ulSortIndex);

            BOOL fFoldersFirst = FALSE;     // @@todo

            BOOL fAlways = (USHORT)(winhIsDlgItemChecked(pcnbp->hwndDlgPage,
                                                         ID_XSDI_ALWAYSSORT));

            if (pcnbp->ulPageID == SP_FLDRSORT_FLDR)
            {
                // if we're being called from a folder's notebook,
                // change instance data
                _xwpSetFldrSort(pcnbp->somSelf,
                                lDefaultSort,
                                fFoldersFirst,
                                fAlways);
            }
            else
            {
                GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(__FILE__, __LINE__, __FUNCTION__);

                pGlobalSettings->lDefaultSort = lDefaultSort;

                if (fAlways)
                {
                    LONG lFoldersFirst,
                         lAlwaysSort;
                    _xwpQueryFldrSort(cmnQueryActiveDesktop(),
                                      &lDefaultSort,
                                      &lFoldersFirst,
                                      &lAlwaysSort);
                    if (lAlwaysSort != 0)
                    {
                        // issue warning that this might also sort the Desktop
                        if (cmnMessageBoxMsg(pcnbp->hwndFrame,
                                             116, 133,
                                             MB_YESNO)
                                       == MBID_YES)
                            _xwpSetFldrSort(cmnQueryActiveDesktop(),
                                            lDefaultSort,
                                            lFoldersFirst,
                                            0);
                    }
                }
                pGlobalSettings->AlwaysSort = fAlways;

                cmnUnlockGlobalSettings();
            }
        break; }

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
                                    Backup->lDefaultSort,
                                    Backup->lFoldersFirst,
                                    Backup->lAlwaysSort);
                }
            }
            else
            {
                // global sort page:
                 cmnSetDefaultSettings(SP_FLDRSORT_GLOBAL);
            }
            // update the display by calling the INIT callback
            (*(pcnbp->pfncbInitPage))(pcnbp, CBI_SET | CBI_ENABLE);
        break;

        case DID_DEFAULT:
            // "Default" button:
            if (pcnbp->ulPageID == SP_FLDRSORT_FLDR)
                _xwpSetFldrSort(pcnbp->somSelf,
                                SET_DEFAULT,
                                SET_DEFAULT,
                                SET_DEFAULT);
            else
                cmnSetDefaultSettings(SP_FLDRSORT_GLOBAL);

            // update the display by calling the INIT callback
            (*(pcnbp->pfncbInitPage))(pcnbp, CBI_SET | CBI_ENABLE);
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
                                       fdrUpdateFolderSorts);
        }
        else
        {
            // global:
            cmnStoreGlobalSettings();
            // update all open folders
            fdrForEachOpenGlobalView(0,
                                     fdrUpdateFolderSorts);
        }
    }

    return 0;
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
 *@@changed V0.9.4 (2000-08-02) [umoeller]: now using sliders; added initial delay
 *@@changed V0.9.9 (2001-03-19) [pr]: multiple startup folder mods.
 */

VOID fdrStartupFolderInitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                              ULONG flFlags)        // CBI_* flags (notebook.h)
{
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    XFldStartupData *somThis = NULL;

    somThis = XFldStartupGetData(pcnbp->somSelf);

    if (flFlags & CBI_INIT)
    {
        if (pcnbp->pUser == 0)
        {
            // first call: backup Global Settings and instance
            // variables for "Undo" button;
            // this memory will be freed automatically by the
            // common notebook window function (notebook.c) when
            // the notebook page is destroyed
            pcnbp->pUser = malloc(sizeof(XFldStartupData) + sizeof(GLOBALSETTINGS));
            memcpy(pcnbp->pUser, somThis, sizeof(XFldStartupData));
            memcpy((char *) pcnbp->pUser + sizeof(XFldStartupData),
                   pGlobalSettings, sizeof(GLOBALSETTINGS));
        }

        // set up sliders
        winhSetSliderTicks(WinWindowFromID(pcnbp->hwndDlgPage, ID_SDDI_STARTUP_INITDELAY_SLID),
                           (MPARAM)0, 3,
                           MPFROM2SHORT(9, 10), 6);
        winhSetSliderTicks(WinWindowFromID(pcnbp->hwndDlgPage, ID_SDDI_STARTUP_OBJDELAY_SLID),
                           (MPARAM)0, 3,
                           MPFROM2SHORT(9, 10), 6);
    }

    if (flFlags & CBI_SET)
    {
        // the range of valid startup delays is
        // 500 ms to 10,000 ms in steps of 500 ms;
        // that gives us 20 - 1 = 19 positions

        // initial delay
        winhSetSliderArmPosition(WinWindowFromID(pcnbp->hwndDlgPage, ID_SDDI_STARTUP_INITDELAY_SLID),
                                 SMA_INCREMENTVALUE,
                                 (pGlobalSettings->ulStartupInitialDelay / 500) - 1);
        // per-object delay
        winhSetSliderArmPosition(WinWindowFromID(pcnbp->hwndDlgPage, ID_SDDI_STARTUP_OBJDELAY_SLID),
                                 SMA_INCREMENTVALUE,
                                 (_ulObjectDelay / 500) - 1);

        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_SDDI_SHOWSTARTUPPROGRESS,
                              pGlobalSettings->ShowStartupProgress);
        if (_ulType == XSTARTUP_REBOOTSONLY)
            winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_SDDI_STARTUP_REBOOTSONLY, TRUE);

        if (_ulType == XSTARTUP_EVERYWPSRESTART)
            winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_SDDI_STARTUP_EVERYWPSRESTART, TRUE);
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
 *@@changed V0.9.4 (2000-08-02) [umoeller]: now using sliders; added initial delay
 *@@changed V0.9.9 (2001-03-19) [pr]: multiple startup folder mods.; add Undo & Default processing
 */

MRESULT fdrStartupFolderItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                    ULONG ulItemID, USHORT usNotifyCode,
                                    ULONG ulExtra)      // for checkboxes: contains new state
{
    GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(__FILE__, __LINE__, __FUNCTION__);
    ULONG   ulChange = 1;
    BOOL fUpdate = TRUE;
    XFldStartupData *somThis = XFldStartupGetData(pcnbp->somSelf);

    switch (ulItemID)
    {
        case ID_SDDI_SHOWSTARTUPPROGRESS:
            pGlobalSettings->ShowStartupProgress = ulExtra;
            fUpdate = FALSE;
        break;

        case ID_SDDI_STARTUP_INITDELAY_SLID:
        {
            LONG lSliderIndex = winhQuerySliderArmPosition(
                                            pcnbp->hwndControl,
                                            SMA_INCREMENTVALUE);
            LONG lMS = (lSliderIndex + 1) * 500;
            CHAR szMS[30];
            sprintf(szMS, "%d ms", lMS);
            WinSetDlgItemText(pcnbp->hwndDlgPage,
                              ID_SDDI_STARTUP_INITDELAY_TXT2,
                              szMS);

            pGlobalSettings->ulStartupInitialDelay = lMS;
            fUpdate = FALSE;
        break; }

        case ID_SDDI_STARTUP_OBJDELAY_SLID:
        {
            LONG lSliderIndex = winhQuerySliderArmPosition(
                                            pcnbp->hwndControl,
                                            SMA_INCREMENTVALUE);
            LONG lMS = (lSliderIndex + 1) * 500;
            CHAR szMS[30];
            sprintf(szMS, "%d ms", lMS);
            WinSetDlgItemText(pcnbp->hwndDlgPage,
                              ID_SDDI_STARTUP_OBJDELAY_TXT2,
                              szMS);

            _ulObjectDelay = lMS;
            ulChange = 0;
        break; }

        case ID_SDDI_STARTUP_REBOOTSONLY:
            _ulType = XSTARTUP_REBOOTSONLY;
            ulChange = 0;
        break;

        case ID_SDDI_STARTUP_EVERYWPSRESTART:
            _ulType = XSTARTUP_EVERYWPSRESTART;
            ulChange = 0;
        break;

        case DID_UNDO:
            if (pcnbp->pUser)
            {
                XFldStartupData *Backup = (pcnbp->pUser);
                PCGLOBALSETTINGS pGSBackup = (PCGLOBALSETTINGS)((char *) pcnbp->pUser + sizeof(XFldStartupData));
                // "Undo" button: restore backed up instance & global data
                _ulType = Backup->ulType;
                _ulObjectDelay = Backup->ulObjectDelay;
                pGlobalSettings->ShowStartupProgress = pGSBackup->ShowStartupProgress;
                pGlobalSettings->ulStartupInitialDelay = pGSBackup->ulStartupInitialDelay;
                // update the display by calling the INIT callback
                (*(pcnbp->pfncbInitPage))(pcnbp, CBI_SET | CBI_ENABLE);
            }
        break;

        case DID_DEFAULT:
            // "Default" button:
            _ulType = XSTARTUP_REBOOTSONLY;
            _ulObjectDelay = XSTARTUP_DEFAULTOBJECTDELAY;
            cmnSetDefaultSettings(SP_STARTUPFOLDER);
            // update the display by calling the INIT callback
            (*(pcnbp->pfncbInitPage))(pcnbp, CBI_SET | CBI_ENABLE);
        break;

        default:
            ulChange = 0;
    }

    cmnUnlockGlobalSettings();

    if (ulChange)
        cmnStoreGlobalSettings();

    if (fUpdate)
        _wpSaveDeferred(pcnbp->somSelf);

    return ((MPARAM)0);
}


