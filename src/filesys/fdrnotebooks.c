
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
#define INCL_WINSTDSPIN
#define INCL_WINSHELLDATA       // Prf* functions
#include <os2.h>

// C library headers
#include <stdio.h>              // needed for except.h
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\dialog.h"             // dialog helpers
#include "helpers\standards.h"          // some standard macros
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
 *
 *   Notebook callbacks (notebook.c) for XFldWPS  "View" page
 *
 ********************************************************************/

static CONTROLDEF
    FolderViewGroup = CONTROLDEF_GROUP(
                            LOAD_STRING, // ""Folder view settings"
                            ID_XSD_FOLDERVIEWGROUP,
                            -1,
                            -1),
    FullPathCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING, // "Show full path in folder ~title",
                            ID_XSDI_FULLPATH,
                            -1,
                            -1),
    KeepTitleCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING, // "Kee~p original title",
                            ID_XSDI_KEEPTITLE,
                            -1,
                            -1),
    MaxPathCharsText1 = CONTROLDEF_TEXT(
                            LOAD_STRING, // "~Limit path to",
                            ID_XSDI_MAXPATHCHARS_TX1,
                            -1,
                            -1),
    MaxPathCharsSpin = CONTROLDEF_SPINBUTTON(
                            ID_XSDI_MAXPATHCHARS,
                            60,
                            -1),
    MaxPathCharsText2 = CONTROLDEF_TEXT(
                            LOAD_STRING, // "characters",
                            ID_XSDI_MAXPATHCHARS_TX2,
                            -1,
                            -1),
    TreeViewAutoScrollCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING, // "Auto-scroll Tree vie~ws"
                            ID_XSDI_TREEVIEWAUTOSCROLL,
                            -1,
                            -1),
#ifndef __NOFDRDEFAULTDOCS__
    FdrDefaultDocCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING, // "~Enable default documents in folders"
                            ID_XSDI_FDRDEFAULTDOC,
                            -1,
                            -1),
    FdrDefaultDocViewCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING, // "De~fault document = default folder view",
                            ID_XSDI_FDRDEFAULTDOCVIEW,
                            -1,
                            -1),
#endif
    FdrAutoRefreshCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING, // "Enable folder ~auto-refresh"
                            ID_XSDI_FDRAUTOREFRESH,
                            -1,
                            -1),
    FdrDefaultViewGroup = CONTROLDEF_GROUP(
                            LOAD_STRING, // "Default folder view"
                            ID_XSDI_FDRVIEWDEFAULT_GROUP,
                            -1,
                            -1),
    FdrViewInheritCB = CONTROLDEF_FIRST_AUTORADIO(
                            LOAD_STRING, // "Use pare~nt folder's default view",
                            ID_XSDI_FDRVIEW_INHERIT,
                            -1,
                            -1),
    FdrViewIconCB = CONTROLDEF_NEXT_AUTORADIO(
                            LOAD_STRING, // "~Icon",
                            ID_XSDI_FDRVIEW_ICON,
                            -1,
                            -1),
    FdrViewTreeCB = CONTROLDEF_NEXT_AUTORADIO(
                            LOAD_STRING, // "T~ree",
                            ID_XSDI_FDRVIEW_TREE,
                            -1,
                            -1),
    FdrViewDetailsCB = CONTROLDEF_NEXT_AUTORADIO(
                            LOAD_STRING, // "Detail~s",
                            ID_XSDI_FDRVIEW_DETAILS,
                            -1,
                            -1);

static const DLGHITEM dlgView[] =
    {
        START_TABLE,            // root table, required
            START_ROW(0),       // row 1 in the root table, required
                // create group on top
                START_GROUP_TABLE(&FolderViewGroup),
                    START_ROW(0),
                        CONTROL_DEF(&FullPathCB),
                    START_ROW(0),
                        CONTROL_DEF(&G_Spacing),
                        CONTROL_DEF(&KeepTitleCB),
                    START_ROW(ROW_VALIGN_CENTER),
                        CONTROL_DEF(&G_Spacing),
                        CONTROL_DEF(&MaxPathCharsText1),
                        CONTROL_DEF(&MaxPathCharsSpin),
                        CONTROL_DEF(&MaxPathCharsText2),
                    START_ROW(0),
                        CONTROL_DEF(&TreeViewAutoScrollCB),
#ifndef __NOFDRDEFAULTDOCS__
                    START_ROW(0),
                        CONTROL_DEF(&FdrDefaultDocCB),
                    START_ROW(0),
                        CONTROL_DEF(&G_Spacing),
                        CONTROL_DEF(&FdrDefaultDocViewCB),
#endif
                    START_ROW(0),
                        CONTROL_DEF(&FdrAutoRefreshCB),
                END_TABLE,
            START_ROW(0),
                START_GROUP_TABLE(&FdrDefaultViewGroup),
                    START_ROW(0),
                        CONTROL_DEF(&FdrViewInheritCB),
                    START_ROW(0),
                        CONTROL_DEF(&FdrViewIconCB),
                    START_ROW(0),
                        CONTROL_DEF(&FdrViewTreeCB),
                    START_ROW(0),
                        CONTROL_DEF(&FdrViewDetailsCB),
                END_TABLE,
            START_ROW(0),       // notebook buttons (will be moved)
                CONTROL_DEF(&G_UndoButton),         // notebook.c
                CONTROL_DEF(&G_DefaultButton),      // notebook.c
                CONTROL_DEF(&G_HelpButton),         // notebook.c
        END_TABLE
    };

static const XWPSETTING G_ViewBackup[] =
    {
        sfFullPath,
        sfKeepTitle,
        sulMaxPathChars,
        sfTreeViewAutoScroll,
#ifndef __NOFDRDEFAULTDOCS__
        sfFdrDefaultDoc,
        sfFdrDefaultDocView,
#endif
        sfFdrAutoRefreshDisabled,
        sulDefaultFolderView
    };

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
 *@@changed V0.9.16 (2001-10-11) [umoeller]: now using dialog formatter
 */

VOID fdrViewInitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                     ULONG flFlags)        // CBI_* flags (notebook.h)
{
    PCKERNELGLOBALS pKernelGlobals = krnQueryGlobals();
    // PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    if (flFlags & CBI_INIT)
    {
        if (pcnbp->pUser == NULL)
        {
            // first call: backup Global Settings for "Undo" button;
            // this memory will be freed automatically by the
            // common notebook window function (notebook.c) when
            // the notebook page is destroyed
            pcnbp->pUser = cmnBackupSettings(G_ViewBackup,
                                             ARRAYITEMCOUNT(G_ViewBackup));
            /* malloc(sizeof(GLOBALSETTINGS));
            memcpy(pcnbp->pUser, pGlobalSettings, sizeof(GLOBALSETTINGS));
               */

            // insert the controls using the dialog formatter
            // V0.9.16 (2001-10-11) [umoeller]
            ntbFormatPage(pcnbp->hwndDlgPage,
                          dlgView,
                          ARRAYITEMCOUNT(dlgView));
        }
    }

    if (flFlags & CBI_SET)
    {
        ULONG ulid;

        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_FULLPATH,
                              cmnQuerySetting(sfFullPath));
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_KEEPTITLE,
                              cmnQuerySetting(sfKeepTitle));
        // maximum path chars spin button
        winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_XSDI_MAXPATHCHARS,
                               11, 200,        // limits
                               cmnQuerySetting(sulMaxPathChars));  // data
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_TREEVIEWAUTOSCROLL,
                              cmnQuerySetting(sfTreeViewAutoScroll));

#ifndef __NOFDRDEFAULTDOCS__
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_FDRDEFAULTDOC,
                              cmnQuerySetting(sfFdrDefaultDoc));
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_FDRDEFAULTDOCVIEW,
                              cmnQuerySetting(sfFdrDefaultDocView));
#endif

        if (pKernelGlobals->fAutoRefreshReplaced)
            winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_FDRAUTOREFRESH,
                                  !cmnQuerySetting(sfFdrAutoRefreshDisabled));

        // folder default views V0.9.12 (2001-04-30) [umoeller]
        switch (cmnQuerySetting(sulDefaultFolderView))
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
#ifndef __ALWAYSSUBCLASS__
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_TREEVIEWAUTOSCROLL,
                (    // (cmnQuerySetting(sNoWorkerThread) == FALSE)
                        // // removed this setting V0.9.16 (2002-01-04) [umoeller]
                   (!cmnQuerySetting(sfNoSubclassing))
                ));
#endif
#ifndef __NOFDRDEFAULTDOCS__
        winhEnableDlgItem(pcnbp->hwndDlgPage, ID_XSDI_FDRDEFAULTDOCVIEW,
                         cmnQuerySetting(sfFdrDefaultDoc));
#endif

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
    // GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(__FILE__, __LINE__, __FUNCTION__);
    MRESULT mrc = (MRESULT)0;
    BOOL fSave = TRUE,
         fUpdate = FALSE;

    // LONG lTemp;

    switch (ulItemID)
    {
        case ID_XSDI_FULLPATH:
            cmnSetSetting(sfFullPath, ulExtra);
            fUpdate = TRUE;
        break;

        case ID_XSDI_KEEPTITLE:
            cmnSetSetting(sfKeepTitle, ulExtra);
            fUpdate = TRUE;
        break;

        case ID_XSDI_TREEVIEWAUTOSCROLL:
            cmnSetSetting(sfTreeViewAutoScroll, ulExtra);
        break;

        case ID_XSDI_MAXPATHCHARS:  // spinbutton
            cmnSetSetting(sulMaxPathChars, ulExtra);
            fUpdate = TRUE;
        break;

#ifndef __NOFDRDEFAULTDOCS__
        case ID_XSDI_FDRDEFAULTDOC:
            cmnSetSetting(sfFdrDefaultDoc, ulExtra);
            // update the display by calling the INIT callback
            pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
        break;

        case ID_XSDI_FDRDEFAULTDOCVIEW:
            cmnSetSetting(sfFdrDefaultDocView, ulExtra);
        break;
#endif

        case ID_XSDI_FDRAUTOREFRESH:
            cmnSetSetting(sfFdrAutoRefreshDisabled, (ulExtra == 0));
        break;

        case ID_XSDI_FDRVIEW_ICON:
            cmnSetSetting(sulDefaultFolderView, OPEN_CONTENTS);
        break;

        case ID_XSDI_FDRVIEW_TREE:
            cmnSetSetting(sulDefaultFolderView, OPEN_TREE);
        break;

        case ID_XSDI_FDRVIEW_DETAILS:
            cmnSetSetting(sulDefaultFolderView, OPEN_DETAILS);
        break;

        case ID_XSDI_FDRVIEW_INHERIT:
            cmnSetSetting(sulDefaultFolderView, 0);
        break;


        case DID_UNDO:
        {
            // "Undo" button: get pointer to backed-up Global Settings
            // PCGLOBALSETTINGS pGSBackup = (PCGLOBALSETTINGS)(pcnbp->pUser);

            // and restore the settings for this page
            cmnRestoreSettings(pcnbp->pUser,
                               ARRAYITEMCOUNT(G_ViewBackup));
            /*
            cmnQuerySetting(sfFullPath)  = pGSBackup->FullPath ;
            cmnSetSetting(sfKeepTitle, pGSBackup->KeepTitle);
            cmnSetSetting(sfTreeViewAutoScroll, pGSBackup->TreeViewAutoScroll);
            cmnSetSetting(sulMaxPathChars, pGSBackup->MaxPathChars);

#ifndef __NOFDRDEFAULTDOCS__
            cmnSetSetting(sfFdrDefaultDoc, pGSBackup->_fFdrDefaultDoc);
            cmnSetSetting(sfFdrDefaultDocView, pGSBackup->_fFdrDefaultDocView);
#endif

            cmnSetSetting(sfFdrAutoRefreshDisabled, pGSBackup->fFdrAutoRefreshDisabled);

            cmnSetSetting(sulDefaultFolderView, pGSBackup->bDefaultFolderView);
               */
            // update the display by calling the INIT callback
            pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
            fUpdate = TRUE;
        break; }

        case DID_DEFAULT:
        {
            // set the default settings for this settings page
            // (this is in common.c because it's also used at
            // Desktop startup)
            cmnSetDefaultSettings(pcnbp->ulPageID);
            // update the display by calling the INIT callback
            pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
            fUpdate = TRUE;
        break; }

        default:
            fSave = FALSE;
    }

    // cmnUnlockGlobalSettings();

    /* if (fSave)
        cmnStoreGlobalSettings(); */

    if (fUpdate)
        // have Worker thread update all open folder windows
        // with the full-path-in-title settings
        xthrPostWorkerMsg(WOM_REFRESHFOLDERVIEWS,
                          (MPARAM)NULL, // update all, not just children
                          MPNULL);

    return (mrc);
}

/* ******************************************************************
 *
 *   Notebook callbacks (notebook.c) for XFldWPS"Grid" page
 *
 ********************************************************************/

#ifndef __NOSNAPTOGRID__

static const XWPSETTING G_GridBackup[] =
    {
        sfAddSnapToGridDefault,
        sulGridX,
        sulGridY,
        sulGridCX,
        sulGridCY
    };

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
    // PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    if (flFlags & CBI_INIT)
    {
        if (pcnbp->pUser == NULL)
        {
            // first call: backup Global Settings for "Undo" button;
            // this memory will be freed automatically by the
            // common notebook window function (notebook.c) when
            // the notebook page is destroyed
            /* pcnbp->pUser = malloc(sizeof(GLOBALSETTINGS));
            memcpy(pcnbp->pUser, pGlobalSettings, sizeof(GLOBALSETTINGS));
            */
            pcnbp->pUser = cmnBackupSettings(G_GridBackup,
                                             ARRAYITEMCOUNT(G_GridBackup));
        }
    }

    if (flFlags & CBI_SET)
    {
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_SNAPTOGRID,
                              cmnQuerySetting(sfAddSnapToGridDefault));
        winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_XSDI_GRID_X,
                               0, 500,
                               cmnQuerySetting(sulGridX));
        winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_XSDI_GRID_Y,
                               0, 500,
                               cmnQuerySetting(sulGridY));
        winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_XSDI_GRID_CX,
                               1, 500,
                               cmnQuerySetting(sulGridCX));
        winhSetDlgItemSpinData(pcnbp->hwndDlgPage, ID_XSDI_GRID_CY,
                               1, 500,
                               cmnQuerySetting(sulGridCY));
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
    // GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(__FILE__, __LINE__, __FUNCTION__);
    MRESULT mrc = (MRESULT)0;
    BOOL fSave = TRUE;

    switch (ulItemID)
    {
        case ID_XSDI_SNAPTOGRID:
            cmnSetSetting(sfAddSnapToGridDefault, ulExtra);
        break;

        case ID_XSDI_GRID_X:
            cmnSetSetting(sulGridX, ulExtra);
        break;

        case ID_XSDI_GRID_Y:
            cmnSetSetting(sulGridY, ulExtra);
        break;

        case ID_XSDI_GRID_CX:
            cmnSetSetting(sulGridCX, ulExtra);
        break;

        case ID_XSDI_GRID_CY:
            cmnSetSetting(sulGridCY, ulExtra);
        break;

        case DID_UNDO:
        {
            // "Undo" button: get pointer to backed-up Global Settings
            // PCGLOBALSETTINGS pGSBackup = (PCGLOBALSETTINGS)(pcnbp->pUser);

            // and restore the settings for this page
            cmnRestoreSettings(pcnbp->pUser,
                               ARRAYITEMCOUNT(G_GridBackup));
            /*
            cmnSetSetting(sfAddSnapToGridDefault, pGSBackup->fAddSnapToGridDefault);
            cmnSetSetting(sulGridX, pGSBackup->GridX);
            cmnSetSetting(sulGridY, pGSBackup->GridY);
            cmnSetSetting(sulGridCX, pGSBackup->GridCX);
            cmnSetSetting(sulGridCY, pGSBackup->GridCY);
               */
            // update the display by calling the INIT callback
            pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
        break; }

        case DID_DEFAULT:
        {
            // set the default settings for this settings page
            // (this is in common.c because it's also used at
            // Desktop startup)
            cmnSetDefaultSettings(pcnbp->ulPageID);
            // update the display by calling the INIT callback
            pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
        break; }

        default:
            fSave = FALSE;
    }

    // cmnUnlockGlobalSettings();

    /* if (fSave)
        cmnStoreGlobalSettings(); */

    return (mrc);
}

#endif

/* ******************************************************************
 *
 *   Notebook callbacks (notebook.c) for "XFolder" instance page
 *
 ********************************************************************/

static CONTROLDEF
#ifndef __NOFOLDERCONTENTS__
    FavoriteFolderCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,
                            ID_XSDI_FAVORITEFOLDER,
                            -1,
                            -1),
#endif
#ifndef __NOQUICKOPEN__
    QuickOpenCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,
                            ID_XSDI_QUICKOPEN,
                            -1,
                            -1),
#endif
    /* FullPathCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,
                            ID_XSDI_FULLPATH,
                            -1,
                            -1),
    KeepTitleCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,
                            ID_XSDI_KEEPTITLE,
                            -1,
                            -1), */
            // already defined in "View" page above
#ifndef __NOSNAPTOGRID__
    SnapToGridCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,
                            ID_XSDI_SNAPTOGRID,
                            -1,
                            -1),
#endif
    FdrHotkeysCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,
                            ID_XSDI_ACCELERATORS,
                            -1,
                            -1),
    StatusBarCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,
                            ID_XSDI_ENABLESTATUSBAR,
                            -1,
                            -1);


static const DLGHITEM dlgXFolder[] =
    {
        START_TABLE,            // root table, required
#ifndef __NOFOLDERCONTENTS__
            START_ROW(0),       // row 1 in the root table, required
                CONTROL_DEF(&FavoriteFolderCB),
#endif
#ifndef __NOQUICKOPEN__
            START_ROW(0),
                CONTROL_DEF(&QuickOpenCB),
#endif
            START_ROW(0),
                CONTROL_DEF(&FullPathCB),
            START_ROW(0),
                CONTROL_DEF(&G_Spacing),
                CONTROL_DEF(&KeepTitleCB),
#ifndef __NOSNAPTOGRID__
            START_ROW(0),
                CONTROL_DEF(&SnapToGridCB),
#endif
            START_ROW(0),
                CONTROL_DEF(&FdrHotkeysCB),
            START_ROW(0),
                CONTROL_DEF(&StatusBarCB),
            START_ROW(0),
                CONTROL_DEF(&G_UndoButton),         // notebook.c
                CONTROL_DEF(&G_DefaultButton),      // notebook.c
                CONTROL_DEF(&G_HelpButton),         // notebook.c
        END_TABLE
    };

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
 *@@changed V0.9.16 (2001-09-29) [umoeller]: now using dialog formatter
 */

VOID fdrXFolderInitPage(PCREATENOTEBOOKPAGE pcnbp,  // notebook info struct
                        ULONG flFlags)              // CBI_* flags (notebook.h)
{
    // PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
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

            // insert the controls using the dialog formatter
            // V0.9.16 (2001-09-29) [umoeller]
            ntbFormatPage(pcnbp->hwndDlgPage,
                          dlgXFolder,
                          ARRAYITEMCOUNT(dlgXFolder));
        }
    }

    if (flFlags & CBI_SET)
    {
#ifndef __NOFOLDERCONTENTS__
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_FAVORITEFOLDER,
                              _xwpIsFavoriteFolder(pcnbp->somSelf));
#endif

#ifndef __NOQUICKOPEN__
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_QUICKOPEN,
                              _xwpQueryQuickOpen(pcnbp->somSelf));
#endif

        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_FULLPATH,
                              (     ((_bFullPathInstance == 2)
                                       ? cmnQuerySetting(sfFullPath)
                                       : _bFullPathInstance )
                                 != 0));
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_KEEPTITLE,
                              (     ((_bKeepTitleInstance == 2)
                                       ? cmnQuerySetting(sfKeepTitle)
                                       : _bKeepTitleInstance )
                                 != 0));
#ifndef __NOSNAPTOGRID__
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_SNAPTOGRID,
                              (     ((_bSnapToGridInstance == 2)
                                       ? cmnQuerySetting(sfAddSnapToGridDefault)
                                       : _bSnapToGridInstance )
                                 != 0));
#endif
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_ACCELERATORS,
                              (     ((_bFolderHotkeysInstance == 2)
                                       ? cmnQuerySetting(sfFolderHotkeysDefault)
                                       : _bFolderHotkeysInstance )
                                 != 0));
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_ENABLESTATUSBAR,
                              (   (     ((_bStatusBarInstance == STATUSBAR_DEFAULT)
                                           ? cmnQuerySetting(sfDefaultStatusBarVisibility)
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
                         (
                              1
#ifndef __ALWAYSSUBCLASS__
                           &&
                              !cmnQuerySetting(sfNoSubclassing)
                           &&
#endif
#ifndef __ALWAYSFDRHOTKEYS__
                              (cmnQuerySetting(sfFolderHotkeys))
#endif
                         ));

        winhEnableDlgItem(pcnbp->hwndDlgPage,
                         ID_XSDI_KEEPTITLE,
                         ( (_bFullPathInstance == 2)
                             ? cmnQuerySetting(sfFullPath)
                             : _bFullPathInstance ));

#ifndef __NOSNAPTOGRID__
        winhEnableDlgItem(pcnbp->hwndDlgPage,
                         ID_XSDI_SNAPTOGRID,  // added V0.9.1 (99-12-28) [umoeller]
                         cmnQuerySetting(sfSnap2Grid));
#endif
        winhEnableDlgItem(pcnbp->hwndDlgPage,
                         ID_XSDI_ENABLESTATUSBAR,
                         // always disable for Desktop
                         (   (pcnbp->somSelf != cmnQueryActiveDesktop())
#ifndef __ALWAYSSUBCLASS__
                          && (!cmnQuerySetting(sfNoSubclassing))
#endif
#ifndef __NOCFGSTATUSBARS__
                          && (cmnQuerySetting(sfStatusBars))
#endif
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
#ifndef __NOSNAPTOGRID__
        case ID_XSDI_SNAPTOGRID:
            _bSnapToGridInstance = ulExtra;
        break;
#endif

        case ID_XSDI_FULLPATH:
            _bFullPathInstance = ulExtra;
            // update the display by calling the INIT callback
            pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
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

#ifndef __NOFOLDERCONTENTS__
        case ID_XSDI_FAVORITEFOLDER:
            _xwpMakeFavoriteFolder(pcnbp->somSelf, ulExtra);
        break;
#endif

#ifndef __NOQUICKOPEN__
        case ID_XSDI_QUICKOPEN:
            _xwpSetQuickOpen(pcnbp->somSelf, ulExtra);
        break;
#endif

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
                pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
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
            pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
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

#ifndef __NOXWPSTARTUP__

/* ******************************************************************
 *
 *   XFldStartup notebook callbacks (notebook.c)
 *
 ********************************************************************/

static const XWPSETTING G_StartupFolderBackup[] =
    {
        sulStartupInitialDelay,
        sfShowStartupProgress
    };

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
    // PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
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
            /* pcnbp->pUser = malloc(sizeof(XFldStartupData) + sizeof(GLOBALSETTINGS));
            memcpy(pcnbp->pUser, somThis, sizeof(XFldStartupData));
            memcpy((char *) pcnbp->pUser + sizeof(XFldStartupData),
                   pGlobalSettings, sizeof(GLOBALSETTINGS)); */
            pcnbp->pUser = cmnBackupSettings(G_StartupFolderBackup,
                                             ARRAYITEMCOUNT(G_StartupFolderBackup));
            pcnbp->pUser2 = malloc(sizeof(XFldStartupData));
            memcpy(pcnbp->pUser2, somThis, sizeof(XFldStartupData));
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
                                 (cmnQuerySetting(sulStartupInitialDelay) / 500) - 1);
        // per-object delay
        winhSetSliderArmPosition(WinWindowFromID(pcnbp->hwndDlgPage, ID_SDDI_STARTUP_OBJDELAY_SLID),
                                 SMA_INCREMENTVALUE,
                                 (_ulObjectDelay / 500) - 1);

        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_SDDI_SHOWSTARTUPPROGRESS,
                              cmnQuerySetting(sfShowStartupProgress));
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
    // GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(__FILE__, __LINE__, __FUNCTION__);
    ULONG   ulChange = 1;
    BOOL fUpdate = TRUE;
    XFldStartupData *somThis = XFldStartupGetData(pcnbp->somSelf);

    switch (ulItemID)
    {
        case ID_SDDI_SHOWSTARTUPPROGRESS:
            cmnSetSetting(sfShowStartupProgress, ulExtra);
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

            cmnSetSetting(sulStartupInitialDelay, lMS);
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
                XFldStartupData *Backup = pcnbp->pUser2;
                // PCGLOBALSETTINGS pGSBackup = (PCGLOBALSETTINGS)((char *) pcnbp->pUser + sizeof(XFldStartupData));
                // "Undo" button: restore backed up instance & global data
                _ulType = Backup->ulType;
                _ulObjectDelay = Backup->ulObjectDelay;
                cmnRestoreSettings(pcnbp->pUser,
                                   ARRAYITEMCOUNT(G_StartupFolderBackup));
                /* cmnSetSetting(sfShowStartupProgress, pGSBackup->ShowStartupProgress);
                cmnSetSetting(sulStartupInitialDelay, pGSBackup->ulStartupInitialDelay);
                */
                // update the display by calling the INIT callback
                pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
            }
        break;

        case DID_DEFAULT:
            // "Default" button:
            _ulType = XSTARTUP_REBOOTSONLY;
            _ulObjectDelay = XSTARTUP_DEFAULTOBJECTDELAY;
            cmnSetDefaultSettings(SP_STARTUPFOLDER);
            // update the display by calling the INIT callback
            pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
        break;

        default:
            ulChange = 0;
    }

    // cmnUnlockGlobalSettings();

    /* if (ulChange)
        cmnStoreGlobalSettings(); */

    if (fUpdate)
        _wpSaveDeferred(pcnbp->somSelf);

    return ((MPARAM)0);
}

#endif
