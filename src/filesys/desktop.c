
/*
 *@@sourcefile desktop.c:
 *      XFldDesktop implementation code. Note that more
 *      Desktop-related code resides in filesys\archives.c
 *      and filesys\shutdown.c.
 *
 *      This file is ALL new with V0.9.0.
 *
 *      Function prefix for this file:
 *      --  dtp*
 *
 *@@added V0.9.0 [umoeller]
 *@@header "filesys\desktop.h"
 */

/*
 *      Copyright (C) 1997-99 Ulrich M”ller.
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

// #define INCL_DOS
#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#define INCL_DOSERRORS
#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WINPOINTERS
#define INCL_WINMENUS
#define INCL_WINDIALOGS
#define INCL_WINSTATICS
#define INCL_WINBUTTONS
#define INCL_WINENTRYFIELDS
#define INCL_WINLISTBOXES
#define INCL_WINSTDCNR
#define INCL_WINSTDSPIN
#define INCL_WINSHELLDATA       // Prf* functions
#define INCL_WINSTDFILE
#define INCL_WINSYS
#define INCL_GPIBITMAPS
#include <os2.h>

// C library headers
#include <stdio.h>
#include <io.h>
#include <math.h>

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\animate.h"            // icon and other animations
#include "helpers\comctl.h"             // common controls (window procs)
#include "helpers\gpih.h"               // GPI helper routines
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\shapewin.h"           // shaped windows helper functions
#include "helpers\stringh.h"            // string helper routines
#include "helpers\winh.h"               // PM helper routines

// SOM headers which don't crash with prec. header files
#pragma hdrstop                 // VAC++ keeps crashing otherwise
#include "xfdesk.h"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling

#include "filesys\menus.h"              // shared context menu logic

#include "startshut\shutdown.h"         // XWorkplace eXtended Shutdown

// other SOM headers

/* ******************************************************************
 *                                                                  *
 *   Query setup strings                                            *
 *                                                                  *
 ********************************************************************/

/*
 *@@ dtpQuerySetup:
 *      implementation of XFldDesktop::xwpQuerySetup2.
 *      See remarks there.
 *
 *      This returns the length of the XFldDesktop
 *      setup string part only.
 *
 *@@added V0.9.1 (2000-01-20) [umoeller]
 */

ULONG dtpQuerySetup(WPDesktop *somSelf,
                    PSZ pszSetupString,
                    ULONG cbSetupString)
{
    PSZ     pszTemp = NULL;
    ULONG   ulReturn = 0;
    ULONG   ulValue = 0,
            ulDefaultValue = 0;

    // AUTOLOCKUP=YES/NO
    /* if (_wpQueryAutoLockup(somSelf))
        strhxcat(&pszTemp, "AUTOLOCKUP=YES");

    // LOCKUPAUTODIM=YES/NO
    if (_wpQueryLockupAutoDim(somSelf) == FALSE)
        strhxcat(&pszTemp, "LOCKUPAUTODIM=NO");

    // LOCKUPBACKGROUND

    // LOCKUPFULLSCREEN
    if (_wpQueryLockupFullScreen(somSelf) == FALSE)
        strhxcat(&pszTemp, "LOCKUPFULLSCREEN=NO");

    // LOCKUPONSTARTUP
    if (_wpQueryLockupOnStart(somSelf))
        strhxcat(&pszTemp, "LOCKUPONSTARTUP=YES");

    _wpQueryLockupBackground();

    // LOCKUPTIMEOUT
    ulValue = _wpQueryLockupTimeout(somSelf);
    if (ulValue != 3)
    {
        CHAR szTemp[300];
        sprintf(szTemp, "LOCKUPTIMEOUT=%d", ulValue);
        strhxcat(&pszTemp, szTemp);
    } */

    /*
     * append string
     *
     */

    if (pszTemp)
    {
        // return string if buffer is given
        if ( (pszSetupString) && (cbSetupString) )
            strhncpy0(pszSetupString,   // target
                      pszTemp,          // source
                      cbSetupString);   // buffer size

        // always return length of string
        ulReturn = strlen(pszTemp);
        free(pszTemp);
    }

    return (ulReturn);
}

/* ******************************************************************
 *                                                                  *
 *   Desktop menus                                                  *
 *                                                                  *
 ********************************************************************/

/*
 *@@ dtpModifyPopupMenu:
 *      implementation for XFldDesktop::wpModifyPopupMenu.
 *
 *@@added V0.9.0 [umoeller]
 */

VOID dtpModifyPopupMenu(WPDesktop *somSelf,
                        HWND hwndMenu)
{
    BOOL            fRemoveDefaultShutdownItem = FALSE;
    HWND            hwndMenuInsert = hwndMenu;
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    PKERNELGLOBALS  pKernelGlobals = krnLockGlobals(5000);
    PNLSSTRINGS     pNLSStrings = cmnQueryNLSStrings();

    // position of original "Shutdown" menu item in context menu
    SHORT   sOrigShutdownPos = (SHORT)WinSendMsg(hwndMenu,
                                                 MM_ITEMPOSITIONFROMID,
                                                 MPFROM2SHORT(WPMENUID_SHUTDOWN, FALSE),
                                                 MPNULL);

    if (    (pGlobalSettings->fXShutdown)    // XShutdown enabled?
         && (pGlobalSettings->fDTMShutdown)  // menu item enabled?
         && (!pGlobalSettings->NoWorkerThread)  // Worker thread enabled?
       )
    {
        CHAR szShutdown[50];

        if (pGlobalSettings->fDTMShutdownMenu)
        {
            /*
             * create shutdown submenu (V0.9.0):
             *
             */

            if (pGlobalSettings->fDTMShutdown)
            {
                // "Shutdown" menu item enabled:
                hwndMenuInsert
                    = winhInsertSubmenu(hwndMenu,
                                        // position: after existing "Shutdown" item
                                        (SHORT)WinSendMsg(hwndMenu,
                                                          MM_ITEMPOSITIONFROMID,
                                                          MPFROM2SHORT(WPMENUID_SHUTDOWN,
                                                                       FALSE),
                                                          MPNULL) + 1,
                                        pGlobalSettings->VarMenuOffset + ID_XFM_OFS_SHUTDOWNMENU,
                                        pNLSStrings->pszShutdown,
                                        MIS_TEXT,
                                        // first item ID in "Shutdown" menu:
                                        // default OS/2 shutdown
                                        WPMENUID_SHUTDOWN,
                                        "Default OS/2 shutdown...",
                                        MIS_TEXT, 0);

                sOrigShutdownPos = MIT_END;

                strcpy(szShutdown, "~XShutdown");
                if (pGlobalSettings->ulXShutdownFlags & XSD_CONFIRM)
                    strcat(szShutdown, "...");

                fRemoveDefaultShutdownItem = TRUE;
            }
        } // end if (pGlobalSettings->DTMShutdownMenu)
        else
        {
            /*
             * replace shutdown menu item (old style):
             *
             */

            strcpy(szShutdown, pNLSStrings->pszShutdown);
            if (pGlobalSettings->ulXShutdownFlags & XSD_CONFIRM)
                strcat(szShutdown, "...");

            fRemoveDefaultShutdownItem = TRUE;
        }

        winhInsertMenuItem(hwndMenuInsert,
                           sOrigShutdownPos,
                           pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_XSHUTDOWN,
                           szShutdown,
                           MIS_TEXT,
                           // disable if Shutdown is currently running
                           (   (thrQueryID(pKernelGlobals->ptiShutdownThread)
                            || (pKernelGlobals->fShutdownRunning)
                           )
                               ? MIA_DISABLED
                               : 0));

    } // end if (pGlobalSettings->XShutdown) ...

    if (pGlobalSettings->fRestartWPS)
    {
        // insert "Restart WPS"
        winhInsertMenuItem(hwndMenuInsert,  // either main menu or "Shutdown" submenu
                           sOrigShutdownPos,  // either MIT_END or position of "Shutdown" item
                           pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_RESTARTWPS,
                           pNLSStrings->pszRestartWPS,
                           MIS_TEXT,
                           // disable if Shutdown is currently running
                           (   (thrQueryID(pKernelGlobals->ptiShutdownThread)
                            || (pKernelGlobals->fShutdownRunning)
                           )
                               ? MIA_DISABLED
                               : 0));

        if ((pGlobalSettings->ulXShutdownFlags & XSD_CONFIRM) == 0)
            // if XShutdown confirmations have been disabled,
            // remove "..." from "Restart WPS" entry
            winhMenuRemoveEllipse(hwndMenuInsert,
                                  pGlobalSettings->VarMenuOffset
                                        + ID_XFMI_OFS_RESTARTWPS);
    }

    // remove default menu items?
    if (!pGlobalSettings->fDTMLockup)
        winhRemoveMenuItem(hwndMenu, WPMENUID_LOCKUP);
    if (!pGlobalSettings->fDTMSystemSetup)
        winhRemoveMenuItem(hwndMenu, WPMENUID_SYSTEMSETUP);
    if ((fRemoveDefaultShutdownItem) || (!pGlobalSettings->fDTMShutdown))
        winhRemoveMenuItem(hwndMenu, WPMENUID_SHUTDOWN);

    #ifdef __DEBUG_ALLOC__
        // if XWorkplace is compiled with
        // VAC++ debug memory funcs,
        // add a menu item for listing all memory objects
        winhInsertMenuSeparator(hwndMenu,
                                MIT_END,
                                (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_SEPARATOR));
        winhInsertMenuItem(hwndMenu,
                           MIT_END,
                           WPMENUID_USER,
                           "List VAC++ debug heap",
                           MIS_TEXT, 0);
    #endif

    krnUnlockGlobals();
}

/*
 *@@ dtpMenuItemSelected:
 *      implementation for XFldDesktop::wpMenuItemSelected.
 *
 *      This returns TRUE if one of the new items was processed.
 *
 *@@added V0.9.1 (99-12-04) [umoeller]
 */

BOOL dtpMenuItemSelected(XFldDesktop *somSelf,
                         HWND hwndFrame,
                         ULONG ulMenuId)
{
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    PCKERNELGLOBALS   pKernelGlobals = krnQueryGlobals();
    if (!(pKernelGlobals->fShutdownRunning))
    {
        if ((ulMenuId - (pGlobalSettings->VarMenuOffset)) == ID_XFMI_OFS_RESTARTWPS)
        {
            xsdInitiateRestartWPS();
            return (TRUE);
        }
        else if (    ((ulMenuId - (pGlobalSettings->VarMenuOffset)) == ID_XFMI_OFS_XSHUTDOWN)
                 &&  (pGlobalSettings->fXShutdown)
                 &&  (pGlobalSettings->NoWorkerThread == 0)
                )
        {
            xsdInitiateShutdown();
            return (TRUE);
        }
    }

    #ifdef __DEBUG_ALLOC__
        // if XWorkplace is compiled with
        // VAC++ debug memory funcs,
        // check the menu item for listing all memory objects
        if (ulMenuId == WPMENUID_USER)
        {
            cmnCreateMemDebugWindow();
            return (TRUE);
        }

    #endif

    return (FALSE);
}

/* ******************************************************************
 *                                                                  *
 *   XFldDesktop notebook settings pages callbacks (notebook.c)     *
 *                                                                  *
 ********************************************************************/

/*
 * fncbWPSDesktop1InitPage:
 *      notebook callback function (notebook.c) for the
 *      "Menu items" page in the Desktop's settings
 *      notebook.
 *      Sets the controls on the page according to the
 *      Global Settings.
 *
 *@@added V0.9.0 [umoeller]
 */

VOID dtpMenuItemsInitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
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
        winhSetDlgItemChecked(pcnbp->hwndPage, ID_XSDI_DTP_SORT,
                              pGlobalSettings->fDTMSort);
        winhSetDlgItemChecked(pcnbp->hwndPage, ID_XSDI_DTP_ARRANGE,
                              pGlobalSettings->fDTMArrange);
        winhSetDlgItemChecked(pcnbp->hwndPage, ID_XSDI_DTP_SYSTEMSETUP,
                              pGlobalSettings->fDTMSystemSetup);
        winhSetDlgItemChecked(pcnbp->hwndPage, ID_XSDI_DTP_LOCKUP  ,
                              pGlobalSettings->fDTMLockup);
        winhSetDlgItemChecked(pcnbp->hwndPage, ID_XSDI_DTP_SHUTDOWN,
                              pGlobalSettings->fDTMShutdown);
        winhSetDlgItemChecked(pcnbp->hwndPage, ID_XSDI_DTP_SHUTDOWNMENU,
                              pGlobalSettings->fDTMShutdownMenu);
    }

    if (flFlags & CBI_ENABLE)
    {
        WinEnableControl(pcnbp->hwndPage, ID_XSDI_DTP_SHUTDOWNMENU,
                         (     (pGlobalSettings->fXShutdown)
                           &&  (pGlobalSettings->fDTMShutdown)
                           &&  (!pGlobalSettings->NoWorkerThread)
                         ));
    }
}

/*
 * dtpMenuItemsItemChanged:
 *      notebook callback function (notebook.c) for the
 *      "Menu items" page in the Desktop's settings
 *      notebook.
 *      Reacts to changes of any of the dialog controls.
 *
 *@@added V0.9.0 [umoeller]
 */

MRESULT dtpMenuItemsItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                    USHORT usItemID,
                                    USHORT usNotifyCode,
                                    ULONG ulExtra)      // for checkboxes: contains new state
{
    GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(5000);
    ULONG ulChange = 1;

    // LONG lTemp;

    switch (usItemID)
    {
        case ID_XSDI_DTP_SORT:
            pGlobalSettings->fDTMSort = ulExtra;
        break;

        case ID_XSDI_DTP_ARRANGE:
            pGlobalSettings->fDTMArrange = ulExtra;
        break;

        case ID_XSDI_DTP_SYSTEMSETUP:
            pGlobalSettings->fDTMSystemSetup = ulExtra;
        break;

        case ID_XSDI_DTP_LOCKUP:
            pGlobalSettings->fDTMLockup = ulExtra;
        break;

        case ID_XSDI_DTP_SHUTDOWN:
            pGlobalSettings->fDTMShutdown = ulExtra;
            dtpMenuItemsInitPage(pcnbp, CBI_ENABLE);
        break;

        case ID_XSDI_DTP_SHUTDOWNMENU:
            pGlobalSettings->fDTMShutdownMenu = ulExtra;
        break;

        case DID_UNDO:
        {
            // "Undo" button: get pointer to backed-up Global Settings
            GLOBALSETTINGS *pGSBackup = (GLOBALSETTINGS*)(pcnbp->pUser);

            // and restore the settings for this page
            pGlobalSettings->fDTMSystemSetup = pGSBackup->fDTMSystemSetup;
            pGlobalSettings->fDTMLockup = pGSBackup->fDTMLockup;
            pGlobalSettings->fDTMShutdown = pGSBackup->fDTMShutdown;
            pGlobalSettings->fDTMShutdownMenu = pGSBackup->fDTMShutdownMenu;

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
            ulChange = 0;
    }

    cmnUnlockGlobalSettings();

    if (ulChange)
        // enable/disable items
        cmnStoreGlobalSettings();

    return ((MPARAM)0);
}

/*
 * dtpStartupInitPage:
 *      notebook callback function (notebook.c) for the
 *      "Startup" page in the Desktop's settings
 *      notebook.
 *      Sets the controls on the page according to the
 *      Global Settings.
 *
 *@@added V0.9.0 [umoeller]
 */

VOID dtpStartupInitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
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

            // backup old boot logo file
            pcnbp->pUser2 = prfhQueryProfileData(HINI_USER,
                                                 INIAPP_XWORKPLACE,
                                                 INIKEY_BOOTLOGOFILE,
                                                 NULL);

            // prepare the control to properly display
            // stretched bitmaps
            ctlPrepareStretchedBitmap(WinWindowFromID(pcnbp->hwndPage,
                                                      ID_XSDI_DTP_LOGOBITMAP),
                                      TRUE);    // preserve proportions

            // set entry field limit
            winhSetEntryFieldLimit(WinWindowFromID(pcnbp->hwndPage,
                                                   ID_XSDI_DTP_LOGOFILE),
                                   CCHMAXPATH);
        }
    }

    if (flFlags & CBI_SET)
    {
        USHORT      usRadioID;
        ULONG       ulError;
        HDC         hdcMem;
        HPS         hpsMem;
        HBITMAP     hbmBootLogo;

        HPOINTER hptrOld = winhSetWaitPointer();

        PSZ         pszBootLogoFile = cmnQueryBootLogoFile();

        winhSetDlgItemChecked(pcnbp->hwndPage, ID_XSDI_DTP_BOOTUPSTATUS,
                              pGlobalSettings->ShowBootupStatus);

        // "boot logo enabled"
        winhSetDlgItemChecked(pcnbp->hwndPage, ID_XSDI_DTP_BOOTLOGO,
                              pGlobalSettings->BootLogo);

        // "boot logo style"
        if (pGlobalSettings->bBootLogoStyle == 0)
            usRadioID = ID_XSDI_DTP_LOGO_TRANSPARENT;
        else
            usRadioID = ID_XSDI_DTP_LOGO_BLOWUP;
        winhSetDlgItemChecked(pcnbp->hwndPage, usRadioID,
                              BM_CHECKED);

        // set boot logo file entry field
        WinSetDlgItemText(pcnbp->hwndPage,
                          ID_XSDI_DTP_LOGOFILE,
                          pszBootLogoFile);

        // attempt to display the boot logo
        if (gpihCreateMemPS(WinQueryAnchorBlock(pcnbp->hwndPage),
                            &hdcMem,
                            &hpsMem))
        {
            if (hbmBootLogo = gpihLoadBitmapFile(hpsMem,
                                                 pszBootLogoFile,
                                                 &ulError))
            {
                // and have the subclassed static control display the thing
                WinSendMsg(WinWindowFromID(pcnbp->hwndPage,
                                           ID_XSDI_DTP_LOGOBITMAP),
                           SM_SETHANDLE,
                           (MPARAM)(hbmBootLogo),
                           MPNULL);

                // delete the bitmap again
                // (the static control has made a private copy
                // of the bitmap, so this is safe)
                GpiDeleteBitmap(hbmBootLogo);
            }
            GpiDestroyPS(hpsMem);
            DevCloseDC(hdcMem);
        }
        free(pszBootLogoFile);

        WinSetPointer(HWND_DESKTOP, hptrOld);
    }

    if (flFlags & CBI_ENABLE)
    {
        PSZ     pszBootLogoFile = cmnQueryBootLogoFile();
        BOOL    fBootLogoFileExists = (access(pszBootLogoFile, 0) == 0);
        free(pszBootLogoFile);

        WinEnableControl(pcnbp->hwndPage, ID_XSDI_DTP_LOGOBITMAP,
                         pGlobalSettings->BootLogo);

        if (WinQueryObject(XFOLDER_STARTUPID))
            WinEnableControl(pcnbp->hwndPage, ID_XSDI_DTP_CREATESTARTUPFLDR, FALSE);

        WinEnableControl(pcnbp->hwndPage, ID_XSDI_DTP_TESTLOGO, fBootLogoFileExists);
    }
}

/*
 * dtpStartupItemChanged:
 *      notebook callback function (notebook.c) for the
 *      "Startup" page in the Desktop's settings
 *      notebook.
 *      Reacts to changes of any of the dialog controls.
 *
 *@@added V0.9.0 [umoeller]
 */

MRESULT dtpStartupItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                              USHORT usItemID,
                              USHORT usNotifyCode,
                              ULONG ulExtra)      // for checkboxes: contains new state
{
    BOOL fProcessed = TRUE;

    {
        GLOBALSETTINGS *pGlobalSettings = cmnLockGlobalSettings(5000);
        ULONG ulChange = 1;

        // LONG lTemp;

        switch (usItemID)
        {
            case ID_XSDI_DTP_BOOTUPSTATUS:
                pGlobalSettings->ShowBootupStatus = ulExtra;
            break;

            case ID_XSDI_DTP_BOOTLOGO:
                pGlobalSettings->BootLogo = ulExtra;
                ulChange = 2;       // re-enable items
            break;

            case ID_XSDI_DTP_LOGO_TRANSPARENT:
                pGlobalSettings->bBootLogoStyle = 0;
            break;

            case ID_XSDI_DTP_LOGO_BLOWUP:
                pGlobalSettings->bBootLogoStyle = 1;
            break;

            case DID_UNDO:
            {
                // "Undo" button: get pointer to backed-up Global Settings
                GLOBALSETTINGS *pGSBackup = (GLOBALSETTINGS*)(pcnbp->pUser);

                // and restore the settings for this page
                pGlobalSettings->ShowBootupStatus = pGSBackup->ShowBootupStatus;
                pGlobalSettings->BootLogo = pGSBackup->BootLogo;
                pGlobalSettings->bBootLogoStyle = pGSBackup->bBootLogoStyle;

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
                ulChange = 0;
                fProcessed = FALSE;
        }

        cmnUnlockGlobalSettings();

        if (ulChange)
        {
            cmnStoreGlobalSettings();
            if (ulChange == 2)
                // enable/disable items
                (*(pcnbp->pfncbInitPage))(pcnbp, CBI_ENABLE);
        }
    }

    if (!fProcessed)
    {
        // not processed above:
        // second switch-case with non-global settings stuff
        PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

        switch (usItemID)
        {
            /*
             * ID_XSDI_DTP_LOGOFILE:
             *      focus leaves "file" entry field:
             *      update OS2.INI
             */

            case ID_XSDI_DTP_LOGOFILE:
            {
                PSZ pszNewBootLogoFile = winhQueryWindowText(pcnbp->hwndControl);
                if (usNotifyCode == EN_CHANGE)
                    WinEnableControl(pcnbp->hwndPage, ID_XSDI_DTP_TESTLOGO,
                                     (access(pszNewBootLogoFile, 0) == 0));
                else if (usNotifyCode == EN_KILLFOCUS)
                {
                    // query new file name from entry field
                    PrfWriteProfileString(HINI_USER,
                                          INIAPP_XWORKPLACE,
                                          INIKEY_BOOTLOGOFILE,
                                          pszNewBootLogoFile);
                    // update the display by calling the INIT callback
                    (*(pcnbp->pfncbInitPage))(pcnbp, CBI_SET | CBI_ENABLE);
                }
                if (pszNewBootLogoFile)
                    free(pszNewBootLogoFile);
            break; }

            /*
             * ID_XSDI_DTP_LOGO_BROWSE:
             *      "Browse" button: open file dialog
             */

            case ID_XSDI_DTP_LOGO_BROWSE:
            {
                FILEDLG fd;
                PSZ pszNewBootLogoFile = winhQueryWindowText(WinWindowFromID(pcnbp->hwndPage,
                                                                             ID_XSDI_DTP_LOGOFILE));

                memset(&fd, 0, sizeof(FILEDLG));
                fd.cbSize = sizeof(FILEDLG);
                fd.fl = FDS_OPEN_DIALOG
                          | FDS_CENTER;

                if (pszNewBootLogoFile)
                {
                    // get last directory used
                    PSZ p = strrchr(pszNewBootLogoFile, '\\');
                    if (p)
                    {
                        // contains directory:
                        PSZ pszDir = strhSubstr(pszNewBootLogoFile, p + 1);
                        strcpy(fd.szFullFile, pszDir);
                        free(pszDir);
                    }
                    free(pszNewBootLogoFile);
                }
                strcat(fd.szFullFile, "*.bmp");

                if (    WinFileDlg(HWND_DESKTOP,    // parent
                                   pcnbp->hwndPage, // owner
                                   &fd)
                    && (fd.lReturn == DID_OK)
                   )
                {
                    // copy file from FOD to page
                    WinSetDlgItemText(pcnbp->hwndPage,
                                      ID_XSDI_DTP_LOGOFILE,
                                      fd.szFullFile);
                    PrfWriteProfileString(HINI_USER,
                                          INIAPP_XWORKPLACE,
                                          INIKEY_BOOTLOGOFILE,
                                          fd.szFullFile);
                    // update the display by calling the INIT callback
                    (*(pcnbp->pfncbInitPage))(pcnbp, CBI_SET | CBI_ENABLE);
                }
            break; }

            /*
             * ID_XSDI_DTP_TESTLOGO:
             *
             */

            case ID_XSDI_DTP_TESTLOGO:
            {
                HDC         hdcMem;
                HPS         hpsMem;
                HBITMAP     hbmBootLogo;
                ULONG       ulError;

                HPOINTER    hptrOld = winhSetWaitPointer();

                PSZ         pszBootLogoFile = cmnQueryBootLogoFile();

                // attempt to load the boot logo
                if (gpihCreateMemPS(WinQueryAnchorBlock(pcnbp->hwndPage),
                                    &hdcMem,
                                    &hpsMem))
                {
                    if (hbmBootLogo = gpihLoadBitmapFile(hpsMem,
                                                         pszBootLogoFile,
                                                         &ulError))
                    {
                        if (pGlobalSettings->bBootLogoStyle == 1)
                        {
                            // blow-up mode:
                            HPS     hpsScreen = WinGetScreenPS(HWND_DESKTOP);
                            anmBlowUpBitmap(hpsScreen,
                                            hbmBootLogo,
                                            1000);  // total animation time

                            DosSleep(2000);

                            WinReleasePS(hpsScreen);

                            // repaint all windows
                            winhRepaintWindows(HWND_DESKTOP);
                        }
                        else
                        {
                            // transparent mode:
                            SHAPEFRAME sf;
                            SWP     swpScreen;

                            sf.hab = WinQueryAnchorBlock(pcnbp->hwndPage);
                            sf.hps = hpsMem;
                            sf.hbm = hbmBootLogo;
                            sf.bmi.cbFix = sizeof(sf.bmi);
                            GpiQueryBitmapInfoHeader(sf.hbm, &sf.bmi);

                            // set ptlLowerLeft so that the bitmap
                            // is centered on the screen
                            WinQueryWindowPos(HWND_DESKTOP, &swpScreen);
                            sf.ptlLowerLeft.x = (swpScreen.cx - sf.bmi.cx) / 2;
                            sf.ptlLowerLeft.y = (swpScreen.cy - sf.bmi.cy) / 2;

                            if (shpCreateWindows(&sf))
                            {
                                DosSleep(2000);

                                WinDestroyWindow(sf.hwndShapeFrame) ;
                                WinDestroyWindow(sf.hwndShape);
                            }
                        }
                        // delete the bitmap again
                        GpiDeleteBitmap(hbmBootLogo);
                    }
                    GpiDestroyPS(hpsMem);
                    DevCloseDC(hdcMem);
                }
                free(pszBootLogoFile);

                WinSetPointer(HWND_DESKTOP, hptrOld);
            break; }

            /*
             *@@ ID_XSDI_DTP_CREATESTARTUPFLDR:
             *      "Create startup folder"
             */

            case ID_XSDI_DTP_CREATESTARTUPFLDR:
            {
                CHAR        szSetup[200];
                HOBJECT     hObj;
                sprintf(szSetup,
                    "DEFAULTVIEW=ICON;ICONVIEW=NONFLOWED,MINI;"
                    "OBJECTID=%s;",
                    XFOLDER_STARTUPID);
                if (hObj = WinCreateObject("XFldStartup", "XFolder Startup",
                                           szSetup,
                                           "<WP_DESKTOP>",
                                           CO_UPDATEIFEXISTS))
                    WinEnableControl(pcnbp->hwndPage, ID_XSDI_DTP_CREATESTARTUPFLDR, FALSE);
                else
                    cmnMessageBoxMsg(pcnbp->hwndPage, 104, 105, MB_OK);
            break; }

        }
    }
    return ((MPARAM)0);
}


