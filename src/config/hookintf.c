
/*
 *@@sourcefile hookintf.c:
 *      daemon/hook interface. This is responsible for
 *      configuring the hook and the daemon.
 *
 *      This has both public interface functions for
 *      configuring the hook (thru the daemon) and
 *      the "Mouse" and "Keyboard" notebook pages.
 *
 *      NEVER call the hook or the daemon directly.
 *      ALWAYS use the functions in this file, which
 *      take care of proper serialization.
 *
 *      The code in here gets called from classes\xwpkeybd.c
 *      (XWPKeyboard), classes\xwpmouse.c (XWPMouse),
 *      classes\xfobj.c (XFldObject).
 *
 *      Function prefix for this file:
 *      --  hif*
 *
 *      This file is ALL new with V0.9.0.
 *
 *@@added V0.9.0 [umoeller]
 *@@header "config\hookintf.h"
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

#define INCL_DOSSEMAPHORES
#define INCL_WINSHELLDATA       // Prf* functions
#define INCL_WINWINDOWMGR
#define INCL_WININPUT
#define INCL_WINPOINTERS
#define INCL_WINSYS
#define INCL_WINMENUS
#define INCL_WINDIALOGS
#define INCL_WINBUTTONS
#define INCL_WINLISTBOXES
#define INCL_WINSTDCNR
#define INCL_WINSTDSLIDER
#include <os2.h>

// C library headers
#include <stdio.h>

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\comctl.h"             // common controls (window procs)
#include "helpers\cnrh.h"               // container helper routines
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\winh.h"               // PM helper routines

// SOM headers which don't crash with prec. header files
#include "xfobj.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling

// headers in /hook
#include "hook\xwphook.h"

#include "filesys\xthreads.h"           // extra XWorkplace threads

#include "config\hookintf.h"            // daemon/hook interface

#pragma hdrstop
#include <wpfolder.h>                   // WPFolder
#include <wpshadow.h>                   // WPShadow

/* ******************************************************************
 *                                                                  *
 *   XWorkplace daemon/hook interface                               *
 *                                                                  *
 ********************************************************************/

/*
 *@@ hifXWPHookReady:
 *      this returns TRUE only if all of the following apply:
 *
 *      --  the XWorkplace daemon is running;
 *
 *      --  the XWorkplace hook is loaded;
 *
 *      --  the XWorkplace hook has been enabled in XWPSetup.
 *
 *      This is used by the various configuration subcomponents
 *      to check whether the hook functionality is currently
 *      available.
 *
 *@@added V0.9.0 [umoeller]
 */

BOOL hifXWPHookReady(VOID)
{
    BOOL brc = FALSE;
    PCKERNELGLOBALS pKernelGlobals = krnQueryGlobals();
    PDAEMONSHARED pDaemonShared = pKernelGlobals->pDaemonShared;
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    if (pDaemonShared)
        if (pDaemonShared->fHookInstalled)
            if (pGlobalSettings->fEnableXWPHook)
                brc = TRUE;
    return (brc);
}

/*
 *@@ hifObjectHotkeysEnabled:
 *      returns TRUE if object hotkeys have been
 *      enabled. This does not mean that the
 *      hook is running, but returns the flag
 *      in HOOKCONFIG only.
 *
 *@@added V0.9.1 (2000-02-01) [umoeller]
 */

BOOL hifObjectHotkeysEnabled(VOID)
{
    BOOL    brc = FALSE;
    HOOKCONFIG  HookConfig;
    ULONG       cb = sizeof(HookConfig);
    if (PrfQueryProfileData(HINI_USER,
                            INIAPP_XWPHOOK,
                            INIKEY_HOOK_CONFIG,
                            &HookConfig,
                            &cb))
        if (HookConfig.fGlobalHotkeys)
            brc = TRUE;

    return (brc);
}

/*
 *@@ hifEnableObjectHotkeys:
 *      enables or disables object hotkeys
 *      altogether. This does not change
 *      the hotkeys list.
 *
 *@@added V0.9.1 (2000-02-01) [umoeller]
 */

VOID hifEnableObjectHotkeys(BOOL fEnable)
{
    BOOL    brc = FALSE;
    HOOKCONFIG  HookConfig;
    ULONG       cb = sizeof(HookConfig);
    if (PrfQueryProfileData(HINI_USER,
                            INIAPP_XWPHOOK,
                            INIKEY_HOOK_CONFIG,
                            &HookConfig,
                            &cb))
    {
        HookConfig.fGlobalHotkeys = fEnable;
        // write back to OS2.INI and notify hook
        hifHookConfigChanged(&HookConfig);
    }
}

/*
 *@@ hifQueryObjectHotkeys:
 *      this returns the array of GLOBALHOTKEY structures
 *      which define _all_ global object hotkeys which are
 *      currently defined.
 *
 *      If _any_ hotkeys exist, a pointer is returned which
 *      points to an array of GLOBALHOTKEY structures, and
 *      *pcHotkeys is set to the number of items in that
 *      array (_not_ the array size!).
 *
 *      If no hotkeys exist, NULL is returned, and *pcHotkeys
 *      is left alone.
 *
 *      Use hifFreeObjectHotkeys to free the hotkeys array.
 *
 *@@added V0.9.0 [umoeller]
 */

PVOID hifQueryObjectHotkeys(PULONG pcHotkeys)   // out: hotkey count (req.)
{
    PGLOBALHOTKEY   pHotkeys = NULL;
    ULONG           cbHotkeys = 0;

    pHotkeys = (PGLOBALHOTKEY)prfhQueryProfileData(HINI_USER,
                                                   INIAPP_XWPHOOK,
                                                   INIKEY_HOOK_HOTKEYS,
                                                   &cbHotkeys);

    if (pHotkeys)
        // found: calc no. of items in array
        *pcHotkeys = cbHotkeys / sizeof(GLOBALHOTKEY);

    return (pHotkeys);
}

/*
 *@@ hifFreeObjectHotkeys:
 *      this frees the memory allocated with the return
 *      value of hifQueryObjectHotkeys.
 *
 *@@added V0.9.0 [umoeller]
 */

VOID hifFreeObjectHotkeys(PVOID pvHotkeys)
{
    if (pvHotkeys)
        free(pvHotkeys);
}

/*
 *@@ hifSetObjectHotkeys:
 *      this defines a new set of global object hotkeys
 *      and notifies the XWorkplace daemon (XWPDAEMON.EXE)
 *      of the change, which in turn updates the data of
 *      the XWorkplace hook (XWPHOOK.DLL).
 *
 *      Note that this function redefines all object
 *      hotkeys at once, since the hook is too dumb to
 *      handle single hotkey changes by itself.
 *
 *      Use XFldObject::xwpSetObjectHotkey to change
 *      the hotkey for a single object, which prepares a
 *      new hotkey list and calls this function in turn.
 *
 *@@added V0.9.0 [umoeller]
 */

BOOL hifSetObjectHotkeys(PVOID pvHotkeys,   // in: ptr to array of GLOBALHOTKEY structs
                         ULONG cHotkeys)    // in: no. of items in that array (_not_ array item size!)
{
    BOOL brc = FALSE;

    brc = PrfWriteProfileData(HINI_USER,
                              INIAPP_XWPHOOK,
                              INIKEY_HOOK_HOTKEYS,
                              pvHotkeys,
                              cHotkeys * sizeof(GLOBALHOTKEY));
    if (brc)
    {
        // notify daemon, which in turn notifies the hook
        brc = krnPostDaemonMsg(XDM_HOTKEYSCHANGED,
                               0, 0);
    }

    return (brc);
}

/*
 *@@ hifHookConfigChanged:
 *      this writes the HOOKCONFIG structure which
 *      pvdc points to back to OS2.INI and posts
 *      XDM_HOOKCONFIG to the daemon, which then
 *      re-reads that data for both the daemon and
 *      the hook.
 *
 *      Note that the config data does not include
 *      object hotkeys. Use hifSetObjectHotkeys for
 *      that.
 *
 *      pvdc is declared as a PVOID so that xwphook.h
 *      does not have included when including common.h,
 *      but _must_ be a PHOOKCONFIG really.
 *
 *@@added V0.9.0 [umoeller]
 */

BOOL hifHookConfigChanged(PVOID pvdc)
{
    // store HOOKCONFIG back to INI
    // and notify daemon
    BOOL            brc = FALSE;

    if (pvdc)
    {
        PHOOKCONFIG   pdc = (PHOOKCONFIG)pvdc;

        PCKERNELGLOBALS  pKernelGlobals = krnQueryGlobals();
        PDAEMONSHARED   pDaemonShared = pKernelGlobals->pDaemonShared;

        brc = PrfWriteProfileData(HINI_USER,
                                  INIAPP_XWPHOOK,
                                  INIKEY_HOOK_CONFIG,
                                  pdc,
                                  sizeof(HOOKCONFIG));

        if (brc)
            if (pDaemonShared)
                if (pDaemonShared->hwndDaemonObject)
                    // cross-process send msg: this
                    // does not return until the daemon
                    // has re-read the data
                    brc = (BOOL)WinSendMsg(pDaemonShared->hwndDaemonObject,
                                           XDM_HOOKCONFIG,
                                           0, 0);
    }

    return (brc);
}

/* ******************************************************************
 *                                                                  *
 *   XWPKeyboard notebook callbacks (notebook.c)                    *
 *                                                                  *
 ********************************************************************/

/*
 *@@ hifKeybdHotkeysInitPage:
 *      notebook callback function (notebook.c) for the
 *      "Object hotkeys" page in the "Keyboard" settings object.
 *      Sets the controls on the page according to the
 *      Global Settings.
 */

VOID hifKeybdHotkeysInitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                             ULONG flFlags)        // CBI_* flags (notebook.h)
{
    // PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    if (flFlags & CBI_INIT)
    {
        XFIELDINFO      xfi[7];
        PFIELDINFO      pfi = NULL;
        int             i = 0;
        HWND            hwndCnr = WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_HOTK_CNR);
        PNLSSTRINGS     pNLSStrings = cmnQueryNLSStrings();

        // recreate container at the same position as
        // the container in the dlg resources;
        // this is required because of the undocumented
        // CCS_MINICONS style to support mini-icons in
        // Details view (duh...)
        SWP             swpCnr;
        WinQueryWindowPos(hwndCnr, &swpCnr);
        WinDestroyWindow(hwndCnr);
        hwndCnr = WinCreateWindow(pcnbp->hwndDlgPage,        // parent
                                  WC_CONTAINER,
                                  "",
                                  CCS_MINIICONS | CCS_READONLY | CCS_SINGLESEL,
                                  swpCnr.x, swpCnr.y, swpCnr.cx, swpCnr.cy,
                                  pcnbp->hwndDlgPage,        // owner
                                  HWND_TOP,
                                  ID_XSDI_HOTK_CNR,
                                  NULL, NULL);
        WinShowWindow(hwndCnr, TRUE);

        // set up cnr details view
        xfi[i].ulFieldOffset = FIELDOFFSET(HOTKEYRECORD, ulIndex);
        xfi[i].pszColumnTitle = "";
        xfi[i].ulDataType = CFA_ULONG;
        xfi[i++].ulOrientation = CFA_RIGHT;

        xfi[i].ulFieldOffset = FIELDOFFSET(RECORDCORE, hptrMiniIcon);
        xfi[i].pszColumnTitle = "";
        xfi[i].ulDataType = CFA_BITMAPORICON;
        xfi[i++].ulOrientation = CFA_CENTER;

        xfi[i].ulFieldOffset = FIELDOFFSET(RECORDCORE, pszIcon);
        xfi[i].pszColumnTitle = pNLSStrings->pszHotkeyTitle;
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        xfi[i].ulFieldOffset = FIELDOFFSET(HOTKEYRECORD, pszFolderPath);
        xfi[i].pszColumnTitle = pNLSStrings->pszHotkeyFolder;
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        xfi[i].ulFieldOffset = FIELDOFFSET(HOTKEYRECORD, pszHandle);
        xfi[i].pszColumnTitle = pNLSStrings->pszHotkeyHandle;
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        xfi[i].ulFieldOffset = FIELDOFFSET(HOTKEYRECORD, pszHotkey);
        xfi[i].pszColumnTitle = pNLSStrings->pszHotkeyHotkey;
        xfi[i].ulDataType = CFA_STRING;
        xfi[i++].ulOrientation = CFA_LEFT;

        pfi = cnrhSetFieldInfos(hwndCnr,
                                &xfi[0],
                                i,             // array item count
                                TRUE,          // no draw lines
                                4);            // return column

        BEGIN_CNRINFO()
        {
            cnrhSetView(CV_DETAIL | CV_MINI | CA_DETAILSVIEWTITLES | CA_DRAWICON);
            cnrhSetSplitBarAfter(pfi);
            cnrhSetSplitBarPos(250);
        } END_CNRINFO(hwndCnr);
    }

    if (flFlags & CBI_SET)
    {
        // insert hotkeys into container: this takes
        // several seconds, so have this done by the
        // file thread
        xthrPostFileMsg(FIM_INSERTHOTKEYS,
                        (MPARAM)WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_HOTK_CNR),
                        (MPARAM)&pcnbp->fShowWaitPointer);
    }
}

/*
 *@@ hifKeybdHotkeysItemChanged:
 *      notebook callback function (notebook.c) for the
 *      "Object hotkeys" page in the "Keyboard" settings object.
 *      Reacts to changes of any of the dialog controls.
 *
 *@@changed V0.9.1 (99-12-06): fixed context-menu-on-whitespace bug
 */

MRESULT hifKeybdHotkeysItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                   USHORT usItemID, USHORT usNotifyCode,
                                   ULONG ulExtra)      // for checkboxes: contains new state
{
    // PGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    MRESULT mrc = (MPARAM)0;

    switch (usItemID)
    {
        case ID_XSDI_HOTK_CNR:
            switch (usNotifyCode)
            {
                /*
                 * CN_CONTEXTMENU:
                 *      ulExtra has the record core
                 */

                case CN_CONTEXTMENU:
                {
                    HWND    hPopupMenu = NULLHANDLE; // fixed V0.9.1 (99-12-06)

                    // we store the container and recc.
                    // in the CREATENOTEBOOKPAGE structure
                    // so that the notebook.c function can
                    // remove source emphasis later automatically
                    pcnbp->hwndSourceCnr = pcnbp->hwndControl;
                    pcnbp->preccSource = (PRECORDCORE)ulExtra;
                    if (pcnbp->preccSource)
                    {
                        // popup menu on container recc:
                        hPopupMenu = WinLoadMenu(pcnbp->hwndDlgPage,
                                                 cmnQueryNLSModuleHandle(FALSE),
                                                 ID_XSM_HOTKEYS_SEL);
                    }

                    if (hPopupMenu)
                        cnrhShowContextMenu(pcnbp->hwndControl,     // cnr
                                            (PRECORDCORE)pcnbp->preccSource,
                                            hPopupMenu,
                                            pcnbp->hwndDlgPage);    // owner
                break; }
            }
        break;

        /*
         * ID_XSMI_HOTKEYS_PROPERTIES:
         *      "object properties" context menu item
         */

        case ID_XSMI_HOTKEYS_PROPERTIES:
        {
            PHOTKEYRECORD precc = (PHOTKEYRECORD)pcnbp->preccSource;
                        // this has been set in CN_CONTEXTMENU above
            if (precc)
                if (precc->pObject)
                    _wpViewObject(precc->pObject,
                                  NULLHANDLE,   // hwndCnr (?!?)
                                  OPEN_SETTINGS,
                                  0);
        break; }

        /*
         * ID_XSMI_HOTKEYS_OPENFOLDER:
         *      "open folder" context menu item
         */

        case ID_XSMI_HOTKEYS_OPENFOLDER:
        {
            PHOTKEYRECORD precc = (PHOTKEYRECORD)pcnbp->preccSource;
                        // this has been set in CN_CONTEXTMENU above
            if (precc)
                if (precc->pObject)
                {
                    WPFolder *pFolder = _wpQueryFolder(precc->pObject);
                    if (pFolder)
                        _wpViewObject(pFolder,
                                      NULLHANDLE,   // hwndCnr (?!?)
                                      OPEN_DEFAULT,
                                      0);
                }
        break; }

        /*
         * ID_XSMI_HOTKEYS_REMOVE:
         *      "remove hotkey" context menu item
         */

        case ID_XSMI_HOTKEYS_REMOVE:
        {
            PHOTKEYRECORD precc = (PHOTKEYRECORD)pcnbp->preccSource;
                        // this has been set in CN_CONTEXTMENU above
            if (precc)
            {
                // string replacements
                PSZ apsz[2] = {
                                    precc->szHotkey,        // %1: hotkey
                                    precc->recc.pszIcon     // %2: object title
                              };
                if (cmnMessageBoxMsgExt(pcnbp->hwndFrame, // pcnbp->hwndPage,
                                        148,       // "XWorkplace Setup
                                        apsz, 2,   // two string replacements
                                        162,       // Sure hotkey?
                                        MB_YESNO)
                        == MBID_YES)
                    _xwpclsRemoveObjectHotkey(_XFldObject,
                                              precc->Hotkey.ulHandle);
                            // this updates the notebook in turn
            }
        break; }
    }

    return (mrc);
}

/* ******************************************************************
 *                                                                  *
 *   XWPMouse notebook callbacks (notebook.c)                       *
 *                                                                  *
 ********************************************************************/

/*
 *@@ hifMouseMappings2InitPage:
 *      notebook callback function (notebook.c) for the
 *      new second "Mappings" page in the "Mouse" settings object.
 *      Sets the controls on the page according to the
 *      Global Settings.
 *
 *@@added V0.9.1 (99-12-10) [umoeller]
 */

VOID hifMouseMappings2InitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                               ULONG flFlags)        // CBI_* flags (notebook.h)
{
    if (flFlags & CBI_INIT)
    {
        if (pcnbp->pUser == 0)
        {
            // first call: create HOOKCONFIG
            // structure;
            // this memory will be freed automatically by the
            // common notebook window function (notebook.c) when
            // the notebook page is destroyed
            pcnbp->pUser = malloc(sizeof(HOOKCONFIG));
            if (pcnbp->pUser)
            {
                ULONG cb = sizeof(HOOKCONFIG);
                memset(pcnbp->pUser, 0, sizeof(HOOKCONFIG));
                // overwrite from INI, if found
                PrfQueryProfileData(HINI_USER,
                                    INIAPP_XWPHOOK,
                                    INIKEY_HOOK_CONFIG,
                                    pcnbp->pUser,
                                    &cb);
            }

            // make backup for "undo"
            pcnbp->pUser2 = malloc(sizeof(HOOKCONFIG));
            if (pcnbp->pUser2)
                memcpy(pcnbp->pUser2, pcnbp->pUser, sizeof(HOOKCONFIG));
        }

        // set up sliders
        winhSetSliderTicks(WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MB3PIXELS_SLIDER),
                           0, 3);      // six pixels high
        winhSetSliderTicks(WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MB3PIXELS_SLIDER),
                           MPFROM2SHORT(9, 10),
                           6);      // six pixels high
        winhSetSliderTicks(WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MB3AMP_SLIDER),
                           0, 3);      // six pixels high
        winhSetSliderTicks(WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MB3AMP_SLIDER),
                           MPFROM2SHORT(9, 10),
                           6);      // six pixels high
    }

    if (flFlags & CBI_SET)
    {
        PHOOKCONFIG pdc = (PHOOKCONFIG)pcnbp->pUser;
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_CHORDWINLIST,
                              pdc->fChordWinList);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_SYSMENUMB2,
                              pdc->fSysMenuMB2TitleBar);

        // mb3 scroll
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MB3SCROLL,
                              pdc->fMB3Scroll);

        winhSetSliderArmPosition(WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MB3PIXELS_SLIDER),
                                 SMA_INCREMENTVALUE,
                                 pdc->usMB3ScrollMin);

        if (pdc->usScrollMode == SM_AMPLIFIED)
            winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MB3AMPLIFIED,
                                  TRUE);
        else
            winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MB3LINEWISE,
                                  TRUE);

        winhSetSliderArmPosition(WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MB3AMP_SLIDER),
                                 SMA_INCREMENTVALUE,
                                 pdc->sAmplification + 9);
            // 0 = 10%, 11 = 100%, 13 = 120%, ...
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MB3SCROLLREVERSE,
                              pdc->fMB3ScrollReverse);

    }

    if (flFlags & CBI_ENABLE)
    {
        PHOOKCONFIG pdc = (PHOOKCONFIG)pcnbp->pUser;
        BOOL        fEnableAmp = (   (pdc->fMB3Scroll)
                                  && (pdc->usScrollMode == SM_AMPLIFIED)
                                 );
        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MB3PIXELS_TXT1,
                          pdc->fMB3Scroll);
        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MB3PIXELS_SLIDER,
                          pdc->fMB3Scroll);
        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MB3PIXELS_TXT2,
                          pdc->fMB3Scroll);

        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MB3LINEWISE,
                          pdc->fMB3Scroll);
        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MB3AMPLIFIED,
                          pdc->fMB3Scroll);

        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MB3AMP_TXT1,
                          fEnableAmp);
        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MB3AMP_SLIDER,
                          fEnableAmp);
        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MB3AMP_TXT2,
                          fEnableAmp);

        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_MB3SCROLLREVERSE,
                          pdc->fMB3Scroll);
    }
}

/*
 *@@ hifMouseMappings2ItemChanged:
 *      notebook callback function (notebook.c) for the
 *      new second "Mappings" page in the "Mouse" settings object.
 *      Reacts to changes of any of the dialog controls.
 *
 *@@added V0.9.1 (99-12-10) [umoeller]
 */

MRESULT hifMouseMappings2ItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                     USHORT usItemID, USHORT usNotifyCode,
                                     ULONG ulExtra)      // for checkboxes: contains new state
{
    MRESULT mrc = 0;
    PHOOKCONFIG pdc = (PHOOKCONFIG)pcnbp->pUser;
    BOOL    fSave = TRUE;

    switch (usItemID)
    {
        case ID_XSDI_MOUSE_CHORDWINLIST:
            pdc->fChordWinList = ulExtra;
        break;

        case ID_XSDI_MOUSE_SYSMENUMB2:
            pdc->fSysMenuMB2TitleBar = ulExtra;
        break;

        case ID_XSDI_MOUSE_MB3SCROLL:
            pdc->fMB3Scroll = ulExtra;
            (pcnbp->pfncbInitPage)(pcnbp, CBI_ENABLE);
        break;

        case ID_XSDI_MOUSE_MB3PIXELS_SLIDER:
        {
            LONG lSliderIndex = winhQuerySliderArmPosition(
                                            pcnbp->hwndControl,
                                            SMA_INCREMENTVALUE);

            WinSetDlgItemShort(pcnbp->hwndDlgPage,
                               ID_XSDI_MOUSE_MB3PIXELS_TXT2,
                               lSliderIndex + 1,
                               FALSE);      // unsigned

            pdc->usMB3ScrollMin = lSliderIndex;
        break; }

        case ID_XSDI_MOUSE_MB3LINEWISE:
            pdc->usScrollMode = SM_LINEWISE;
            (pcnbp->pfncbInitPage)(pcnbp, CBI_ENABLE);
        break;

        case ID_XSDI_MOUSE_MB3AMPLIFIED:
            pdc->usScrollMode = SM_AMPLIFIED;
            (pcnbp->pfncbInitPage)(pcnbp, CBI_ENABLE);
        break;

        case ID_XSDI_MOUSE_MB3AMP_SLIDER:
        {
            CHAR    szText[20];
            LONG lSliderIndex = winhQuerySliderArmPosition(
                                            pcnbp->hwndControl,
                                            SMA_INCREMENTVALUE);

            sprintf(szText, "%d%%", 100 + ((lSliderIndex - 9) * 10) );
            WinSetDlgItemText(pcnbp->hwndDlgPage,
                              ID_XSDI_MOUSE_MB3AMP_TXT2,
                              szText);

            pdc->sAmplification = lSliderIndex - 9;
        break; }

        case ID_XSDI_MOUSE_MB3SCROLLREVERSE:
            pdc->fMB3ScrollReverse = ulExtra;
        break;

        /*
         * DID_DEFAULT:
         *
         */

        case DID_DEFAULT:
            pdc->fChordWinList = 0;
            pdc->fSysMenuMB2TitleBar = 0;
            pdc->fMB3Scroll = 0;
            pdc->usMB3ScrollMin = 0;
            pdc->usScrollMode = SM_LINEWISE; // 0
            pdc->sAmplification = 0;
            pdc->fMB3ScrollReverse = 0;
            (pcnbp->pfncbInitPage)(pcnbp, CBI_SET | CBI_ENABLE);
        break;

        /*
         * DID_UNDO:
         *
         */

        case DID_UNDO:
            // restore data which was backed up in INIT callback
            if (pcnbp->pUser2)
                memcpy(pdc, pcnbp->pUser2, sizeof(HOOKCONFIG));
            (pcnbp->pfncbInitPage)(pcnbp, CBI_SET | CBI_ENABLE);
        break;

        default:
            fSave = FALSE;
    }

    if (fSave)
        hifHookConfigChanged(pdc);

    return (mrc);
}

ULONG   ulScreenCornerSelectedID = ID_XSDI_MOUSE_RADIO_TOPLEFT;
ULONG   ulScreenCornerSelectedIndex = 0;
                            // 0 = lower left,
                            // 1 = top left,
                            // 2 = lower right,
                            // 3 = top right

// screen corner object container d'n'd
HOBJECT hobjBeingDragged = NULLHANDLE;
            // NULLHANDLE means dropping is invalid;
            // in between CN_DRAGOVER and CN_DROP, this
            // contains the object handle being dragged

/*
 *@@ UpdateScreenCornerIndex:
 *
 */

VOID UpdateScreenCornerIndex(USHORT usItemID)
{
    switch (usItemID)
    {
        case ID_XSDI_MOUSE_RADIO_TOPLEFT:
            ulScreenCornerSelectedIndex = 1; break;
        case ID_XSDI_MOUSE_RADIO_TOPRIGHT:
            ulScreenCornerSelectedIndex = 3; break;
        case ID_XSDI_MOUSE_RADIO_BOTTOMLEFT:
            ulScreenCornerSelectedIndex = 0; break;
        case ID_XSDI_MOUSE_RADIO_BOTTOMRIGHT:
            ulScreenCornerSelectedIndex = 2; break;
    }
}

/*
 *@@ hifMouseMovementInitPage:
 *      notebook callback function (notebook.c) for the
 *      "Mouse hook" page in the "Mouse" settings object.
 *      Sets the controls on the page according to the
 *      Global Settings.
 */

VOID hifMouseMovementInitPage(PCREATENOTEBOOKPAGE pcnbp,   // notebook info struct
                              ULONG flFlags)        // CBI_* flags (notebook.h)
{
    if (flFlags & CBI_INIT)
    {
        if (pcnbp->pUser == 0)
        {
            // first call: create HOOKCONFIG
            // structure;
            // this memory will be freed automatically by the
            // common notebook window function (notebook.c) when
            // the notebook page is destroyed
            pcnbp->pUser = malloc(sizeof(HOOKCONFIG));
            if (pcnbp->pUser)
            {
                ULONG cb = sizeof(HOOKCONFIG);
                memset(pcnbp->pUser, 0, sizeof(HOOKCONFIG));
                // overwrite from INI, if found
                PrfQueryProfileData(HINI_USER,
                                    INIAPP_XWPHOOK,
                                    INIKEY_HOOK_CONFIG,
                                    pcnbp->pUser,
                                    &cb);
            }

            // make backup for "undo"
            pcnbp->pUser2 = malloc(sizeof(HOOKCONFIG));
            if (pcnbp->pUser2)
                memcpy(pcnbp->pUser2, pcnbp->pUser, sizeof(HOOKCONFIG));
        }

        // setup sliders
        winhSetSliderTicks(WinWindowFromID(pcnbp->hwndDlgPage,
                                           ID_XSDI_MOUSE_FOCUSDELAY_SLIDER),
                           MPFROM2SHORT(5, 10),
                           3);
        winhSetSliderTicks(WinWindowFromID(pcnbp->hwndDlgPage,
                                           ID_XSDI_MOUSE_FOCUSDELAY_SLIDER),
                           MPFROM2SHORT(0, 10),
                           6);
        winhSetSliderTicks(WinWindowFromID(pcnbp->hwndDlgPage,
                                           ID_XSDI_MOUSE_AUTOHIDE_SLIDER),
                           MPFROM2SHORT(4, 10),
                           3);
        winhSetSliderTicks(WinWindowFromID(pcnbp->hwndDlgPage,
                                           ID_XSDI_MOUSE_AUTOHIDE_SLIDER),
                           MPFROM2SHORT(9, 10),
                           6);

        // check top left screen corner
        ulScreenCornerSelectedID = ID_XSDI_MOUSE_RADIO_TOPLEFT;
        ulScreenCornerSelectedIndex = 1;        // top left
        winhSetDlgItemChecked(pcnbp->hwndDlgPage,
                              ulScreenCornerSelectedID,
                              TRUE);

        // fill drop-down box
        {
            PNLSSTRINGS     pNLSStrings = cmnQueryNLSStrings();
            // ULONG   ul;
            HWND    hwndDrop = WinWindowFromID(pcnbp->hwndDlgPage,
                                               ID_XSDI_MOUSE_SPECIAL_DROP);

            WinInsertLboxItem(hwndDrop, LIT_END, pNLSStrings->pszSpecialWindowList);
            WinInsertLboxItem(hwndDrop, LIT_END, pNLSStrings->pszSpecialDesktopPopup);
        }

        // setup container
        BEGIN_CNRINFO()
        {
            cnrhSetView(CV_NAME | CV_MINI | CA_DRAWICON);
        } END_CNRINFO(WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_OPEN_CNR));
    }

    if (flFlags & CBI_SET)
    {
        PHOOKCONFIG pdc = (PHOOKCONFIG)pcnbp->pUser;
        HWND    hwndCnr = WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_OPEN_CNR);
        HWND    hwndDrop = WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_SPECIAL_DROP);

        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_SLIDINGFOCUS,
                              pdc->fSlidingFocus);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_BRING2TOP,
                              pdc->fBring2Top);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_IGNORESEAMLESS,
                              pdc->fIgnoreSeamless);
        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_IGNOREDESKTOP,
                              pdc->fIgnoreDesktop);

        winhSetSliderArmPosition(WinWindowFromID(pcnbp->hwndDlgPage,
                                                 ID_XSDI_MOUSE_FOCUSDELAY_SLIDER),
                                 SMA_INCREMENTVALUE,
                                 // slider uses .1 seconds ticks
                                 pdc->ulSlidingFocusDelay / 100);

        // auto-hide mouse pointer

        winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_AUTOHIDE,
                              (pdc->fAutoHideMouse != 0));
        winhSetSliderArmPosition(WinWindowFromID(pcnbp->hwndDlgPage,
                                                 ID_XSDI_MOUSE_AUTOHIDE_SLIDER),
                                 SMA_INCREMENTVALUE,
                                 pdc->ulAutoHideDelay);

        // screen corners objects

        if (pdc->ahobjHotCornerObjects[ulScreenCornerSelectedIndex] == 0)
        {
            // "Inactive" corner:
            winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_INACTIVEOBJ, TRUE);
            cnrhRemoveAll(hwndCnr);
            winhSetLboxSelectedItem(hwndDrop, LIT_NONE, TRUE);
        }
        else if (   (ULONG)(pdc->ahobjHotCornerObjects[ulScreenCornerSelectedIndex])
                  >= 0xFFFF0000)
        {
            // special function for this corner:
            winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_SPECIAL_CHECK, TRUE);
            cnrhRemoveAll(hwndCnr);
            winhSetLboxSelectedItem(hwndDrop,
                                    (pdc->ahobjHotCornerObjects[ulScreenCornerSelectedIndex]
                                         & 0xFFFF),
                                    TRUE);
        }
        else
        {
            // actual object for this corner:
            HOBJECT hobj = pdc->ahobjHotCornerObjects[ulScreenCornerSelectedIndex];

            winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_OPEN_CHECK, TRUE);

            winhSetLboxSelectedItem(hwndDrop,
                                    LIT_NONE,
                                    TRUE);

            cnrhRemoveAll(hwndCnr);
            if (hobj != 1)
            {
                WPObject *pobj = _wpclsQueryObject(_WPObject,
                                                   hobj);
                if (pobj)
                {
                    PRECORDCORE precc = cnrhAllocRecords(hwndCnr,
                                                         sizeof(RECORDCORE),
                                                         1);
                    precc->pszName = _wpQueryTitle(pobj);
                    precc->hptrMiniIcon = _wpQueryIcon(pobj);
                    cnrhInsertRecords(hwndCnr,
                                      NULL,         // parent
                                      precc,
                                      TRUE, // invalidate
                                      NULL,
                                      CRA_RECORDREADONLY,
                                      1);
                }
            }
        }
    }

    if (flFlags & CBI_ENABLE)
    {
        PHOOKCONFIG pdc = (PHOOKCONFIG)pcnbp->pUser;
        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_BRING2TOP,
                          pdc->fSlidingFocus);
        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_IGNORESEAMLESS,
                          pdc->fSlidingFocus);
        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_IGNOREDESKTOP,
                          pdc->fSlidingFocus);
        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_FOCUSDELAY_TXT1,
                          pdc->fSlidingFocus);
        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_FOCUSDELAY_SLIDER,
                          pdc->fSlidingFocus);
        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_FOCUSDELAY_TXT2,
                          pdc->fSlidingFocus);

        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_AUTOHIDE_TXT1,
                          pdc->fAutoHideMouse);
        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_AUTOHIDE_SLIDER,
                          pdc->fAutoHideMouse);
        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_AUTOHIDE_TXT2,
                          pdc->fAutoHideMouse);

        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_SPECIAL_DROP,
                          winhIsDlgItemChecked(pcnbp->hwndDlgPage,
                                               ID_XSDI_MOUSE_SPECIAL_CHECK));
        WinEnableControl(pcnbp->hwndDlgPage, ID_XSDI_MOUSE_OPEN_CNR,
                          winhIsDlgItemChecked(pcnbp->hwndDlgPage,
                                               ID_XSDI_MOUSE_OPEN_CHECK));
    }
}

/*
 *@@ hifMouseMovementItemChanged:
 *      notebook callback function (notebook.c) for the
 *      "Mouse hook" page in the "Mouse" settings object.
 *      Reacts to changes of any of the dialog controls.
 */

MRESULT hifMouseMovementItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                    USHORT usItemID, USHORT usNotifyCode,
                                    ULONG ulExtra)      // for checkboxes: contains new state
{
    MRESULT mrc = 0;
    PHOOKCONFIG pdc = (PHOOKCONFIG)pcnbp->pUser;
    BOOL    fSave = TRUE;

    switch (usItemID)
    {
        /*
         * ID_XSDI_MOUSE_SLIDINGFOCUS:
         *      "sliding focus"
         */

        case ID_XSDI_MOUSE_SLIDINGFOCUS:
            pdc->fSlidingFocus = ulExtra;
            (pcnbp->pfncbInitPage)(pcnbp, CBI_ENABLE);
        break;

        case ID_XSDI_MOUSE_BRING2TOP:
            pdc->fBring2Top = ulExtra;
        break;

        case ID_XSDI_MOUSE_IGNORESEAMLESS:
            pdc->fIgnoreSeamless = ulExtra;
        break;

        case ID_XSDI_MOUSE_IGNOREDESKTOP:
            pdc->fIgnoreDesktop = ulExtra;
        break;

        case ID_XSDI_MOUSE_FOCUSDELAY_SLIDER:
        {
            CHAR szTemp[30];
            // get .1 seconds offset
            LONG lSliderIndex = winhQuerySliderArmPosition(pcnbp->hwndControl,
                                                           SMA_INCREMENTVALUE);
            // convert to ms
            pdc->ulSlidingFocusDelay = lSliderIndex * 100;
            sprintf(szTemp, "%d ms", pdc->ulSlidingFocusDelay);
            WinSetDlgItemText(pcnbp->hwndDlgPage,
                              ID_XSDI_MOUSE_FOCUSDELAY_TXT2,
                              szTemp);
        break; }

        case ID_XSDI_MOUSE_AUTOHIDE:
            pdc->fAutoHideMouse = ulExtra;
            (pcnbp->pfncbInitPage)(pcnbp, CBI_ENABLE);
        break;

        case ID_XSDI_MOUSE_AUTOHIDE_SLIDER:
        {
            CHAR szTemp[30];
            // get delay
            LONG lSliderIndex = winhQuerySliderArmPosition(pcnbp->hwndControl,
                                                           SMA_INCREMENTVALUE);
            // convert to seconds
            pdc->ulAutoHideDelay = lSliderIndex;
            sprintf(szTemp, "%d s", pdc->ulAutoHideDelay + 1);
            WinSetDlgItemText(pcnbp->hwndDlgPage,
                              ID_XSDI_MOUSE_AUTOHIDE_TXT2,
                              szTemp);
        break; }

        /*
         * ID_XSDI_MOUSE_RADIO_TOPLEFT:
         *      new screen corner selected
         */

        case ID_XSDI_MOUSE_RADIO_TOPLEFT:
        case ID_XSDI_MOUSE_RADIO_TOPRIGHT:
        case ID_XSDI_MOUSE_RADIO_BOTTOMLEFT:
        case ID_XSDI_MOUSE_RADIO_BOTTOMRIGHT:
            // check if the old current corner's object
            // is 1 (our "pseudo" object)
            if (pdc->ahobjHotCornerObjects[ulScreenCornerSelectedIndex] == 1)
                // that's an invalid object, so set to 0 (no function)
                pdc->ahobjHotCornerObjects[ulScreenCornerSelectedIndex] = 0;

            // update global and then update controls on page
            ulScreenCornerSelectedID = usItemID;

            UpdateScreenCornerIndex(usItemID);

            /* _Pmpf(("ctrl: %lX, index: %lX",
                   ulScreenCornerSelectedID,
                   ulScreenCornerSelectedIndex)); */

            (pcnbp->pfncbInitPage)(pcnbp, CBI_SET | CBI_ENABLE);
            fSave = FALSE;
        break;

        case ID_XSDI_MOUSE_INACTIVEOBJ:
            // disable hot corner
            pdc->ahobjHotCornerObjects[ulScreenCornerSelectedIndex] = 0;
            (pcnbp->pfncbInitPage)(pcnbp, CBI_SET | CBI_ENABLE);
        break;

        /*
         * ID_XSDI_MOUSE_OPEN_CHECK:
         *      "open object"
         */

        case ID_XSDI_MOUSE_OPEN_CHECK:
            if (    (pdc->ahobjHotCornerObjects[ulScreenCornerSelectedIndex] == 0)
                 ||  (pdc->ahobjHotCornerObjects[ulScreenCornerSelectedIndex] >= 0xFFFF0000)
                )
                // mode changed to object mode: store a pseudo-object
                pdc->ahobjHotCornerObjects[ulScreenCornerSelectedIndex] = 1;
                    // an object handle of 1 does not exist, however
                    // we need this for the INIT callback to work

            (pcnbp->pfncbInitPage)(pcnbp, CBI_SET | CBI_ENABLE);
        break;

        case ID_XSDI_MOUSE_OPEN_CNR:
            switch (usNotifyCode)
            {
                case CN_DRAGOVER:
                {
                    PCNRDRAGINFO pcdi = (PCNRDRAGINFO)ulExtra;
                    PDRAGITEM   pdrgItem;
                    USHORT      usIndicator = DOR_NODROP,
                                    // cannot be dropped, but send
                                    // DM_DRAGOVER again
                                usOp = DO_UNKNOWN;
                                    // target-defined drop operation:
                                    // user operation (we don't want
                                    // the WPS to copy anything)

                    // reset global variable
                    hobjBeingDragged = NULLHANDLE;

                    // OK so far:
                    // get access to the drag'n'drop structures
                    if (DrgAccessDraginfo(pcdi->pDragInfo))
                    {
                        if (
                                // accept no more than one single item at a time;
                                // we cannot move more than one file type
                                (pcdi->pDragInfo->cditem != 1)
                            )
                        {
                            usIndicator = DOR_NEVERDROP;
                        }
                        else
                        {

                            // accept only default drop operation
                            if (    (pcdi->pDragInfo->usOperation == DO_DEFAULT)
                               )
                            {
                                // get the item being dragged (PDRAGITEM)
                                if (pdrgItem = DrgQueryDragitemPtr(pcdi->pDragInfo, 0))
                                {
                                    // WPS object?
                                    if (DrgVerifyRMF(pdrgItem, "DRM_OBJECT", NULL))
                                    {
                                        // the WPS stores the MINIRECORDCORE of the
                                        // object in ulItemID of the DRAGITEM structure;
                                        // we use OBJECT_FROM_PREC to get the SOM pointer
                                        WPObject *pSourceObject
                                                    = OBJECT_FROM_PREC(pdrgItem->ulItemID);
                                        if (pSourceObject)
                                        {
                                            // dereference shadows
                                            while (     (pSourceObject)
                                                     && (_somIsA(pSourceObject, _WPShadow))
                                                  )
                                                pSourceObject = _wpQueryShadowedObject(pSourceObject,
                                                                    TRUE);  // lock

                                            // store object handle for CN_DROP later
                                            hobjBeingDragged
                                                    = _wpQueryHandle(pSourceObject);
                                            if (hobjBeingDragged)
                                                usIndicator = DOR_DROP;
                                        }
                                    }
                                }
                            }
                        }

                        DrgFreeDraginfo(pcdi->pDragInfo);
                    }

                    // and return the drop flags
                    mrc = (MRFROM2SHORT(usIndicator, usOp));
                break; } // CN_DRAGOVER

                case CN_DROP:
                    if (hobjBeingDragged)
                    {
                        pdc->ahobjHotCornerObjects[ulScreenCornerSelectedIndex]
                                = hobjBeingDragged;
                        hobjBeingDragged = NULLHANDLE;
                        hifMouseMovementInitPage(pcnbp, CBI_SET | CBI_ENABLE);
                    }
                break;
            }
        break;

        /*
         * ID_XSDI_MOUSE_SPECIAL_CHECK:
         *      "special features"
         */

        case ID_XSDI_MOUSE_SPECIAL_CHECK:
            pdc->ahobjHotCornerObjects[ulScreenCornerSelectedIndex] = 0xFFFF0000;
            (pcnbp->pfncbInitPage)(pcnbp, CBI_SET | CBI_ENABLE);
        break;

        case ID_XSDI_MOUSE_SPECIAL_DROP:
        {
            // new special function selected from drop-down box:
            HWND hwndDrop = WinWindowFromID(pcnbp->hwndDlgPage, usItemID);
            LONG lIndex = winhQueryLboxSelectedItem(hwndDrop, LIT_FIRST);
            if (lIndex == LIT_NONE)
                // disable hot corner
                pdc->ahobjHotCornerObjects[ulScreenCornerSelectedIndex] = 0;
            else
                // store special function, which has the hiword as FFFF
                pdc->ahobjHotCornerObjects[ulScreenCornerSelectedIndex]
                    = 0xFFFF0000 | lIndex;
        break; }

        /*
         * DID_DEFAULT:
         *
         */

        case DID_DEFAULT:
            memset(pdc, 0, sizeof(HOOKCONFIG));
            (pcnbp->pfncbInitPage)(pcnbp, CBI_SET | CBI_ENABLE);
        break;

        /*
         * DID_UNDO:
         *
         */

        case DID_UNDO:
            // restore data which was backed up in INIT callback
            if (pcnbp->pUser2)
                memcpy(pdc, pcnbp->pUser2, sizeof(HOOKCONFIG));
            (pcnbp->pfncbInitPage)(pcnbp, CBI_SET | CBI_ENABLE);
        break;

        default:
            fSave = FALSE;
    }

    if (fSave)
        hifHookConfigChanged(pdc);

    return (mrc);
}


