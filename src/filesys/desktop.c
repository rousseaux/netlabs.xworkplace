
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
// #include "helpers\memdebug.h"           // memory debugging
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

#include "filesys\desktop.h"            // XFldDesktop implementation
#include "filesys\menus.h"              // shared context menu logic
#include "filesys\xthreads.h"           // extra XWorkplace threads

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
    // ULONG   ulValue = 0;
            // ulDefaultValue = 0;

    /*

      ### This is the complete list of all WPDesktop setup
      strings, as documented by WPSREF. However, method
      implementations only exist for Warp 4.

      We'd need to manually decode what all the settings
      in PM_Lockup in OS2.INI are good for.

    */


    // AUTOLOCKUP=YES/NO
    /* if (_wpQueryAutoLockup(somSelf))
        xstrcat(&pszTemp, "AUTOLOCKUP=YES");

    // LOCKUPAUTODIM=YES/NO
    if (_wpQueryLockupAutoDim(somSelf) == FALSE)
        xstrcat(&pszTemp, "LOCKUPAUTODIM=NO");

    // LOCKUPBACKGROUND

    // LOCKUPFULLSCREEN
    if (_wpQueryLockupFullScreen(somSelf) == FALSE)
        xstrcat(&pszTemp, "LOCKUPFULLSCREEN=NO");

    // LOCKUPONSTARTUP
    if (_wpQueryLockupOnStart(somSelf))
        xstrcat(&pszTemp, "LOCKUPONSTARTUP=YES");

    _wpQueryLockupBackground();

    // LOCKUPTIMEOUT
    ulValue = _wpQueryLockupTimeout(somSelf);
    if (ulValue != 3)
    {
        CHAR szTemp[300];
        sprintf(szTemp, "LOCKUPTIMEOUT=%d", ulValue);
        xstrcat(&pszTemp, szTemp);
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
 *@@changed V0.9.3 (2000-04-26) [umoeller]: changed shutdown menu IDs for launchpad
 */

VOID dtpModifyPopupMenu(WPDesktop *somSelf,
                        HWND hwndMenu)
{
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

                // remove original shutdown item
                winhRemoveMenuItem(hwndMenu, WPMENUID_SHUTDOWN);

                hwndMenuInsert
                    = winhInsertSubmenu(hwndMenu,
                                        // position: after existing "Shutdown" item
                                        sOrigShutdownPos + 1,
                                        pGlobalSettings->VarMenuOffset + ID_XFM_OFS_SHUTDOWNMENU,
                                        pNLSStrings->pszShutdown,
                                        MIS_TEXT,
                                        // first item ID in "Shutdown" menu:
                                        // default OS/2 shutdown
                                        pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_OS2_SHUTDOWN,
                                                    // WPMENUID_SHUTDOWN,
                                                    // changed V0.9.3 (2000-04-26) [umoeller]
                                        "Default OS/2 shutdown...",
                                        MIS_TEXT, 0);

                sOrigShutdownPos = MIT_END;

                strcpy(szShutdown, "~XShutdown");
                if (pGlobalSettings->ulXShutdownFlags & XSD_CONFIRM)
                    strcat(szShutdown, "...");

                winhInsertMenuItem(hwndMenuInsert,
                                   sOrigShutdownPos,
                                   WPMENUID_SHUTDOWN,
                                   // pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_XSHUTDOWN,
                                   szShutdown,
                                   MIS_TEXT,
                                   // disable if Shutdown is currently running
                                   (   (thrQueryID(&pKernelGlobals->tiShutdownThread)
                                    || (pKernelGlobals->fShutdownRunning)
                                   )
                                       ? MIA_DISABLED
                                       : 0));
            }
        } // end if (pGlobalSettings->DTMShutdownMenu)
        else
        {
            /*
             * replace shutdown menu item (old style):
             *
             */

            /* strcpy(szShutdown, pNLSStrings->pszShutdown);
            if (pGlobalSettings->ulXShutdownFlags & XSD_CONFIRM)
                strcat(szShutdown, "...");

            fRemoveDefaultShutdownItem = TRUE; */
        }

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
                           (   (thrQueryID(&pKernelGlobals->tiShutdownThread)
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


    #ifdef __XWPMEMDEBUG__ // setup.h, helpers\memdebug.c
        // if XWorkplace is compiled with
        // VAC++ debug memory funcs,
        // add a menu item for listing all memory objects
        winhInsertMenuSeparator(hwndMenu,
                                MIT_END,
                                (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_SEPARATOR));
        winhInsertMenuItem(hwndMenu,
                           MIT_END,
                           DEBUG_MENUID_LISTHEAP,
                           "List VAC++ debug heap",
                           MIS_TEXT, 0);
        winhInsertMenuItem(hwndMenu,
                           MIT_END,
                           DEBUG_MENUID_RELEASEFREED,
                           "Release freed memory",
                           MIS_TEXT, 0);
    #endif

    #ifdef __DEBUG__
        // if we have a debug compile,
        // add "crash" items
        winhInsertMenuSeparator(hwndMenu,
                                MIT_END,
                                (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_SEPARATOR));
        hwndMenuInsert = winhInsertSubmenu(hwndMenu,
                                           MIT_END,
                                           DEBUG_MENUID_CRASH_MENU,
                                           "Crash WPS",
                                           MIS_TEXT,
                                           // first item ID in "Shutdown" menu:
                                           // crash thread 1
                                           DEBUG_MENUID_CRASH_THR1,
                                           "Thread 1",
                                           MIS_TEXT, 0);
        winhInsertMenuItem(hwndMenuInsert,
                           MIT_END,
                           DEBUG_MENUID_CRASH_WORKER,
                           "Worker thread",
                           MIS_TEXT, 0);
        winhInsertMenuItem(hwndMenuInsert,
                           MIT_END,
                           DEBUG_MENUID_CRASH_QUICK,
                           "Speedy thread",
                           MIS_TEXT, 0);
        winhInsertMenuItem(hwndMenuInsert,
                           MIT_END,
                           DEBUG_MENUID_CRASH_FILE,
                           "File thread",
                           MIS_TEXT, 0);

        // add "Dump window list"
        winhInsertMenuItem(hwndMenu,
                           MIT_END,
                           DEBUG_MENUID_DUMPWINLIST,
                           "Dump window list",
                           MIS_TEXT, 0);
    #endif

    krnUnlockGlobals();
}

/*
 *@@ dtpMenuItemSelected:
 *      implementation for XFldDesktop::wpMenuItemSelected.
 *
 *      This returns TRUE if one of the new items was processed
 *      or FALSE if the parent method should be called. We may
 *      change *pulMenuId and return FALSE.
 *
 *@@added V0.9.1 (99-12-04) [umoeller]
 *@@changed V0.9.3 (2000-04-26) [umoeller]: changed shutdown menu item IDs; changed prototype
 */

BOOL dtpMenuItemSelected(XFldDesktop *somSelf,
                         HWND hwndFrame,
                         PULONG pulMenuId) // in/out: menu item ID (can be changed)
{
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    PCKERNELGLOBALS   pKernelGlobals = krnQueryGlobals();

    if (!(pKernelGlobals->fShutdownRunning))
    {
        if ((*pulMenuId - (pGlobalSettings->VarMenuOffset)) == ID_XFMI_OFS_RESTARTWPS)
        {
            xsdInitiateRestartWPS();
            return (TRUE);
        }
        else if (    (pGlobalSettings->fXShutdown)
                 &&  (pGlobalSettings->NoWorkerThread == 0)
                )
        {
            // shutdown enabled:
            if (*pulMenuId == WPMENUID_SHUTDOWN)
            {
                xsdInitiateShutdown();
                return (TRUE);
            }
            else if ((*pulMenuId - (pGlobalSettings->VarMenuOffset))
                    == ID_XFMI_OFS_OS2_SHUTDOWN)
            {
                // default OS/2 shutdown (in submenu):
                // have parent method called with default shutdown menu item ID
                // to start OS/2 shutdown...
                *pulMenuId = WPMENUID_SHUTDOWN;
                return (FALSE);
            }
        }
    }

    #ifdef __XWPMEMDEBUG__ // setup.h, helpers\memdebug.c
        // if XWorkplace is compiled with
        // VAC++ debug memory funcs,
        // check the menu item for listing all memory objects
        if (ulMenuId == DEBUG_MENUID_LISTHEAP)
        {
            memdCreateMemDebugWindow();
            return (TRUE);
        }
        else if (ulMenuId == DEBUG_MENUID_RELEASEFREED)
        {
            HPOINTER hptrOld = winhSetWaitPointer();
            memdReleaseFreed();
            WinSetPointer(HWND_DESKTOP, hptrOld);
            return (TRUE);
        }
    #endif

    #ifdef __DEBUG__
        switch (*pulMenuId)
        {
            case DEBUG_MENUID_CRASH_THR1:
                krnPostThread1ObjectMsg(XM_CRASH, 0, 0);
            break;
            case DEBUG_MENUID_CRASH_WORKER:
                xthrPostWorkerMsg(XM_CRASH, 0, 0);
            break;
            case DEBUG_MENUID_CRASH_QUICK:
                xthrPostSpeedyMsg(XM_CRASH, 0, 0);
            break;
            case DEBUG_MENUID_CRASH_FILE:
                xthrPostFileMsg(XM_CRASH, 0, 0);
            break;

            case DEBUG_MENUID_DUMPWINLIST:
                winlCreateWinListWindow();
            break;
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
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_DTP_SORT,
                              pGlobalSettings->fDTMSort);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_DTP_ARRANGE,
                              pGlobalSettings->fDTMArrange);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_DTP_SYSTEMSETUP,
                              pGlobalSettings->fDTMSystemSetup);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_DTP_LOCKUP  ,
                              pGlobalSettings->fDTMLockup);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_DTP_SHUTDOWN,
                              pGlobalSettings->fDTMShutdown);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_DTP_SHUTDOWNMENU,
                              pGlobalSettings->fDTMShutdownMenu);
    }

    if (flFlags & CBI_ENABLE)
    {
        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_DTP_SHUTDOWNMENU,
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
 *@@changed V0.9.1 (2000-02-09) [umoeller]: added NumLock support to this page
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
            ctlPrepareStretchedBitmap(WinWindowFromID(pcnbp->hwndDlgPage,
                                                      ID_XSDI_DTP_LOGOBITMAP),
                                      TRUE);    // preserve proportions

            // set entry field limit
            winhSetEntryFieldLimit(WinWindowFromID(pcnbp->hwndDlgPage,
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

        SIZEL       szlPage = {0, 0};
        PSZ         pszBootLogoFile = cmnQueryBootLogoFile();

        // "boot logo enabled"
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_DTP_BOOTLOGO,
                              pGlobalSettings->BootLogo);

        // "boot logo style"
        if (pGlobalSettings->bBootLogoStyle == 0)
            usRadioID = ID_XSDI_DTP_LOGO_TRANSPARENT;
        else
            usRadioID = ID_XSDI_DTP_LOGO_BLOWUP;
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, usRadioID,
                              BM_CHECKED);

        // set boot logo file entry field
        WinSetDlgItemText(pcnbp->hwndDlgPage,
                          ID_XSDI_DTP_LOGOFILE,
                          pszBootLogoFile);

        // attempt to display the boot logo
        if (gpihCreateMemPS(WinQueryAnchorBlock(pcnbp->hwndDlgPage),
                            &szlPage,
                            &hdcMem,
                            &hpsMem))
        {
            if (hbmBootLogo = gpihLoadBitmapFile(hpsMem,
                                                 pszBootLogoFile,
                                                 &ulError))
            {
                // and have the subclassed static control display the thing
                WinSendMsg(WinWindowFromID(pcnbp->hwndDlgPage,
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

        // bootup status
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_DTP_BOOTUPSTATUS,
                              pGlobalSettings->ShowBootupStatus);

        // numlock on
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_DTP_NUMLOCKON,
                              pGlobalSettings->fNumLockStartup);
    }

    if (flFlags & CBI_ENABLE)
    {
        PSZ     pszBootLogoFile = cmnQueryBootLogoFile();
        BOOL    fBootLogoFileExists = (access(pszBootLogoFile, 0) == 0);
        free(pszBootLogoFile);

        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_DTP_LOGOBITMAP,
                         pGlobalSettings->BootLogo);

        if (WinQueryObject(XFOLDER_STARTUPID))
            WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_DTP_CREATESTARTUPFLDR, FALSE);

        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_DTP_TESTLOGO, fBootLogoFileExists);
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
 *@@changed V0.9.1 (2000-02-09) [umoeller]: added NumLock support to this page
 *@@changed V0.9.3 (2000-04-11) [umoeller]: fixed major resource leak; the bootlogo bitmap was never freed
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

            case ID_XSDI_DTP_BOOTUPSTATUS:
                pGlobalSettings->ShowBootupStatus = ulExtra;
            break;

            case ID_XSDI_DTP_NUMLOCKON:
                pGlobalSettings->fNumLockStartup = ulExtra;
                winhSetNumLock(ulExtra);
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
                    WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_DTP_TESTLOGO,
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
                PSZ pszNewBootLogoFile = winhQueryWindowText(WinWindowFromID(pcnbp->hwndDlgPage,
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
                                   pcnbp->hwndFrame, // owner
                                   &fd)
                    && (fd.lReturn == DID_OK)
                   )
                {
                    // copy file from FOD to page
                    WinSetDlgItemText(pcnbp->hwndDlgPage,
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
                SIZEL       szlPage = {0, 0};
                HPOINTER    hptrOld = winhSetWaitPointer();

                PSZ         pszBootLogoFile = cmnQueryBootLogoFile();

                // attempt to load the boot logo
                if (gpihCreateMemPS(WinQueryAnchorBlock(pcnbp->hwndDlgPage),
                                    &szlPage,
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

                            sf.hab = WinQueryAnchorBlock(pcnbp->hwndDlgPage);
                            sf.hps = hpsMem;
                            sf.hbm = hbmBootLogo;
                            sf.bmi.cbFix = sizeof(sf.bmi);
                            GpiQueryBitmapInfoHeader(sf.hbm, &sf.bmi);

                            // set ptlLowerLeft so that the bitmap
                            // is centered on the screen
                            WinQueryWindowPos(HWND_DESKTOP, &swpScreen);
                            sf.ptlLowerLeft.x = (swpScreen.cx - sf.bmi.cx) / 2;
                            sf.ptlLowerLeft.y = (swpScreen.cy - sf.bmi.cy) / 2;

                            if (shpCreateWindows(&sf)) // this selects the bitmap into the HPS
                            {
                                DosSleep(2000);

                                GpiSetBitmap(sf.hps, NULLHANDLE); // V0.9.3 (2000-04-11) [umoeller]

                                WinDestroyWindow(sf.hwndShapeFrame) ;
                                WinDestroyWindow(sf.hwndShape);
                            }
                        }
                        // delete the bitmap again
                        if (!GpiDeleteBitmap(hbmBootLogo))
                            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                                   "Unable to free bootlogo bitmap.");
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
                if (hObj = WinCreateObject("XFldStartup", "XWorkplace Startup",
                                           szSetup,
                                           "<WP_DESKTOP>",
                                           CO_UPDATEIFEXISTS))
                    WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_DTP_CREATESTARTUPFLDR, FALSE);
                else
                    cmnMessageBoxMsg(pcnbp->hwndFrame,
                                     104, 105,
                                     MB_OK);
            break; }

        }
    }
    return ((MPARAM)0);
}


