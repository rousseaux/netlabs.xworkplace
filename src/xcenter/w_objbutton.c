
/*
 *@@sourcefile w_objbutton.c:
 *      XCenter "object button" widget implementation.
 *      This is built into the XCenter and not in
 *      a plugin DLL because it uses tons of WPS calls.
 *
 *      This PM window class actually implements two
 *      widget classes, the "X-Button" and the "object
 *      button", which are quite similar.
 *
 *      Function prefix for this file:
 *      --  Owgt*
 *
 *      This is all new with V0.9.7.
 *
 *@@added V0.9.7 (2000-11-27) [umoeller]
 *@@header "shared\center.h"
 */

/*
 *      Copyright (C) 2000 Ulrich M”ller.
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

#define INCL_DOSPROCESS
#define INCL_DOSEXCEPTIONS
#define INCL_DOSSEMAPHORES
#define INCL_DOSERRORS

#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WININPUT
#define INCL_WINSYS
#define INCL_WINTIMER
#define INCL_WINPOINTERS
#define INCL_WINMENUS
#define INCL_WINDIALOGS
#define INCL_WINSTATICS
#define INCL_WINBUTTONS
#define INCL_WINSTDDRAG
#define INCL_WINSHELLDATA

#define INCL_GPICONTROL
#define INCL_GPIPRIMITIVES
#define INCL_GPILOGCOLORTABLE
#define INCL_GPIREGIONS
#include <os2.h>

// C library headers
#include <stdio.h>
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\comctl.h"             // common controls (window procs)
#include "helpers\dialog.h"             // dialog helpers
#include "helpers\gpih.h"               // GPI helper routines
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\winh.h"               // PM helper routines
#include "helpers\standards.h"          // some standard macros
#include "helpers\xstring.h"            // extended string helpers

// SOM headers which don't crash with prec. header files
#include "xcenter.ih"
#include "xfobj.ih"
#include "xfldr.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\contentmenus.h"        // shared menu logic
#include "shared\helppanels.h"          // all XWorkplace help panel IDs
#include "shared\kernel.h"              // XWorkplace Kernel

#include "shared\center.h"              // public XCenter interfaces

#include "filesys\folder.h"             // XFolder implementation
#include "filesys\object.h"             // XFldObject implementation

#include "startshut\shutdown.h"         // XWorkplace eXtended Shutdown

#pragma hdrstop                     // VAC++ keeps crashing otherwise
#include <wpdesk.h>
#include <wpdisk.h>
#include <wppower.h>
#include <wpshadow.h>
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

#ifndef __XWPLITE__
static PCSZ G_pcszXButton =     "X Button";
#else
static PCSZ G_pcszXButton =     "eButton";
#endif

/* ******************************************************************
 *
 *   Private widget instance data
 *
 ********************************************************************/

#define MENUFL_NOSHUTDOWN           0x0001
#define MENUFL_NORESTARTWPS         0x0002
#define MENUFL_NOLOGOFF             0x0004
#define MENUFL_NOSUSPEND            0x0008
#define MENUFL_NOLOCKUP             0x0010
#define MENUFL_NORUNDLG             0x0020

/*
 * OBJBUTTONSETUP:
 *
 *@@changed V0.9.14 (2001-08-21) [umoeller]: added x-button menu item configuration
 */

typedef struct _OBJBUTTONSETUP
{
    HOBJECT     hobj;                   // member object's handle (for obj buttons)
    ULONG       flMenuItems;            // MENUFL_* menu item flags (for x-button);
                                        // if any one is set, the corresponding item
                                        // is removed
} OBJBUTTONSETUP, *POBJBUTTONSETUP;

/*
 *@@ OBJBUTTONPRIVATE:
 *      more window data for the "X-Button" widget.
 *
 *      An instance of this is created on WM_CREATE
 *      fnwpObjButtonWidget and stored in XCENTERWIDGET.pUser.
 */

typedef struct _OBJBUTTONPRIVATE
{
    PXCENTERWIDGET pWidget;
            // reverse ptr to general widget data ptr; we need
            // that all the time and don't want to pass it on
            // the stack with each function call

    OBJBUTTONSETUP Setup;
            // widget settings that correspond to a setup string

    ULONG       ulType;                 // either BTF_OBJBUTTON or BTF_XBUTTON

    BOOL        fMouseButton1Down;      // if TRUE, mouse button is currently down
    BOOL        fButtonSunk;            // if TRUE, button control is currently pressed;
                    // the control is painted "down" if either of the two are TRUE
    BOOL        fMouseCaptured; // if TRUE, mouse is currently captured

    BOOL        fHasDragoverEmphasis;   // TRUE only while something is dragged over obj button

    HWND        hwndMenuMain;           // if != NULLHANDLE, this has the currently
                                        // open menu (X-button and object buttons)
    HWND        hwndObjectPopup;        // if != NULLHANDLE, this has the currently
                                        // open object WPS context menu (obj button only)
    BOOL        fOpenedWPSContextMenu;  // TRUE if next WM_COMMAND could be WPS context
                                        // menu; WM_COMMAND comes in AFTER WM_MENUEND,
                                        // so we can't check hwndObjectPoup

    WPObject    *pobjButton;            // object for this button
    WPObject    *pobjNotify;            // != NULL if xwpAddWidgetNotify has been
                                        // called (to avoid duplicate notifications)

    HPOINTER    hptrXMini;              // "X" icon for painting on button,
                                        // if BTF_XBUTTON

    WPPower     *pPower;                // for X-button: if != NULLHANDLE,
                                        // power management is enabled, and this
                                        // has the "<WP_POWER>" object

} OBJBUTTONPRIVATE, *POBJBUTTONPRIVATE;

/* ******************************************************************
 *
 *   Widget setup management
 *
 ********************************************************************/

/*
 *      This section contains shared code to manage the
 *      widget's settings. This can translate a widget
 *      setup string into the fields of a binary setup
 *      structure and vice versa. This code is used by
 *      both an open widget window and a settings dialog.
 */

/*
 *@@ OwgtClearSetup:
 *      cleans up the data in the specified setup
 *      structure, but does not free the structure
 *      itself.
 */

VOID OwgtClearSetup(POBJBUTTONSETUP pSetup)
{
}

/*
 *@@ OwgtScanSetup:
 *      scans the given setup string and translates
 *      its data into the specified binary setup
 *      structure.
 *
 *      NOTE: It is assumed that pSetup is zeroed
 *      out. We do not clean up previous data here.
 *
 *@@added V0.9.7 (2000-12-07) [umoeller]
 *@@changed V0.9.16 (2001-10-15) [umoeller]: added support for OBJECTHANDLE=<OBJID>
 */

VOID OwgtScanSetup(const char *pcszSetupString,
                   POBJBUTTONSETUP pSetup)
{
    PSZ p;
    pSetup->hobj = 0;
    if (p = ctrScanSetupString(pcszSetupString,
                               "OBJECTHANDLE"))
    {
        // is this an object ID?
        if (*p == '<')
        {
            // yes: find the object handle from OS2.INI
            ULONG cb = sizeof(pSetup->hobj);
            PrfQueryProfileData(HINI_USER,
                                "PM_Workplace:Location",
                                p,
                                &pSetup->hobj,
                                &cb);
        }
        else
            // scan hex object handle
            pSetup->hobj = strtol(p, NULL, 16);

        ctrFreeSetupValue(p);
    }

    if (p = ctrScanSetupString(pcszSetupString,
                               "MENUITEMS"))
    {
        pSetup->flMenuItems = strtol(p, NULL, 16);
        ctrFreeSetupValue(p);
    }
    else
        pSetup->flMenuItems = 0;
}

/*
 *@@ OwgtSaveSetup:
 *      composes a new setup string.
 *      The caller must invoke xstrClear on the
 *      string after use.
 */

VOID OwgtSaveSetup(PXSTRING pstrSetup,       // out: setup string (is cleared first)
                   BOOL fIsObjButton,
                   POBJBUTTONSETUP pSetup)
{
    CHAR    szTemp[100];
    xstrInit(pstrSetup, 100);

    if (fIsObjButton)
    {
        if (pSetup->hobj)
        {
            sprintf(szTemp, "OBJECTHANDLE=%lX;",
                    pSetup->hobj);
            xstrcat(pstrSetup, szTemp, 0);
        }
    }
    else
        if (pSetup->flMenuItems)
        {
            sprintf(szTemp, "MENUITEMS=%lX;",
                    pSetup->flMenuItems);
            xstrcat(pstrSetup, szTemp, 0);
        }
}

/* ******************************************************************
 *
 *   Widget settings dialog
 *
 ********************************************************************/

/*
 *@@ XBTNMENUITEMDEF:
 *
 *@@added V0.9.14 (2001-08-21) [umoeller]
 */

typedef struct _XBTNMENUITEMDEF
{
    const char  **ppcsz;
    ULONG       ulFlag,
                ulItemID;
} XBTNMENUITEMDEF, *PXBTNMENUITEMDEF;


static CONTROLDEF
            OKButton = CONTROLDEF_DEFPUSHBUTTON(NULL, DID_OK, 100, 30),
            CancelButton = CONTROLDEF_PUSHBUTTON(NULL, DID_CANCEL, 100, 30),

            ChecksGroup = CONTROLDEF_GROUP(NULL, -1),

            CheckShutdown
                        = CONTROLDEF_AUTOCHECKBOX(NULL,
                                                  1000 + MENUFL_NOSHUTDOWN,
                                                  -1, 20),
            CheckRestartWPS
                        = CONTROLDEF_AUTOCHECKBOX(NULL,
                                                  1000 + MENUFL_NORESTARTWPS,
                                                  -1, 20),
            CheckLogoff
                        = CONTROLDEF_AUTOCHECKBOX(NULL,
                                                  1000 + MENUFL_NOLOGOFF,
                                                  -1, 20),
            CheckSuspend
                        = CONTROLDEF_AUTOCHECKBOX(NULL,
                                                  1000 + MENUFL_NOSUSPEND,
                                                  -1, 20),
            CheckLockup
                        = CONTROLDEF_AUTOCHECKBOX(NULL,
                                                  1000 + MENUFL_NOLOCKUP,
                                                  -1, 20),
            CheckRunDlg
                        = CONTROLDEF_AUTOCHECKBOX(NULL,
                                                  1000 + MENUFL_NORUNDLG,
                                                  -1, 20);

static const DLGHITEM dlgXButtonSettings[] =
    {
        START_TABLE,
            START_ROW(0),
                START_GROUP_TABLE(&ChecksGroup),
                    START_ROW(0),
                        CONTROL_DEF(&CheckRunDlg),
                    START_ROW(0),
                        CONTROL_DEF(&CheckLockup),
                    START_ROW(0),
                        CONTROL_DEF(&CheckSuspend),
                    START_ROW(0),
                        CONTROL_DEF(&CheckLogoff),
                    START_ROW(0),
                        CONTROL_DEF(&CheckRestartWPS),
                    START_ROW(0),
                        CONTROL_DEF(&CheckShutdown),
                END_TABLE,
            START_ROW(0),
                CONTROL_DEF(&OKButton),
                CONTROL_DEF(&CancelButton),
        END_TABLE
    };

/*
 *@@ OwgtShowXButtonSettingsDlg:
 *
 *@@added V0.9.14 (2001-08-21) [umoeller]
 */

VOID EXPENTRY OwgtShowXButtonSettingsDlg(PWIDGETSETTINGSDLGDATA pData)
{
    HWND hwndDlg = NULLHANDLE;
    APIRET arc;

    XBTNMENUITEMDEF   aItems[] =
        {
             &CheckRunDlg.pcszText, MENUFL_NORUNDLG, ID_CRMI_RUN,
             &CheckLockup.pcszText, MENUFL_NOLOCKUP, ID_CRMI_LOCKUPNOW,
             &CheckSuspend.pcszText, MENUFL_NOSUSPEND, ID_CRMI_SUSPEND,
             &CheckLogoff.pcszText, MENUFL_NOLOGOFF, ID_CRMI_LOGOFF,
             &CheckRestartWPS.pcszText, MENUFL_NORESTARTWPS, ID_CRMI_RESTARTWPS,
             &CheckShutdown.pcszText, MENUFL_NOSHUTDOWN, ID_CRMI_SHUTDOWN
        };

    // load the standard X-button menu so we can copy
    // the strings from there
    HWND            hmenuTemp;

    if (hmenuTemp = WinLoadMenu(pData->hwndOwner,
                                cmnQueryNLSModuleHandle(FALSE),
                                ID_CRM_XCENTERBUTTON))
    {
        // for each checkbox, set its text to the
        // corresponding menu item
        ULONG   ul;
        for (ul = 0;
             ul < ARRAYITEMCOUNT(aItems);
             ul++)
        {
            PSZ p;
            *(aItems[ul].ppcsz) = winhQueryMenuItemText(hmenuTemp, aItems[ul].ulItemID);
            if ((p = strstr(*(aItems[ul].ppcsz), "...")))
                strcpy(p, p+3);
        }

        ChecksGroup.pcszText = cmnGetString(ID_CRSI_OWGT_MENUITEMS);
        OKButton.pcszText = cmnGetString(ID_XSSI_DLG_OK);
        CancelButton.pcszText = cmnGetString(ID_XSSI_DLG_CANCEL);

        if (!(arc = dlghCreateDlg(&hwndDlg,
                                  pData->hwndOwner,
                                  FCF_TITLEBAR | FCF_SYSMENU | FCF_DLGBORDER | FCF_NOBYTEALIGN,
                                  WinDefDlgProc,
                                  G_pcszXButton,
                                  dlgXButtonSettings,
                                  ARRAYITEMCOUNT(dlgXButtonSettings),
                                  NULL,
                                  cmnQueryDefaultFont())))
        {
            // go scan the setup string
            OBJBUTTONSETUP  Setup;
            OwgtScanSetup(pData->pcszSetupString, &Setup);

            _Pmpf(("Setup string is \"%s\", flMenuItems = %lX",
                        pData->pcszSetupString,
                        Setup.flMenuItems));


            // check the boxes accordingly
            for (ul = 0;
                 ul < ARRAYITEMCOUNT(aItems);
                 ul++)
            {
                BOOL fCheck = ((Setup.flMenuItems & aItems[ul].ulFlag) == 0);
                _Pmpf(("Setting %lX to %d", 1000 + aItems[ul].ulFlag, fCheck));
                winhSetDlgItemChecked(hwndDlg,
                                      1000 + aItems[ul].ulFlag,
                                      fCheck);
            }

            // go!
            winhCenterWindow(hwndDlg);
            if (DID_OK == WinProcessDlg(hwndDlg))
            {
                XSTRING strSetup;

                // now reset setup
                Setup.flMenuItems = 0;          // show all
                for (ul = 0;
                     ul < ARRAYITEMCOUNT(aItems);
                     ul++)
                {
                    if (!winhIsDlgItemChecked(hwndDlg,
                                              1000 + aItems[ul].ulFlag))
                        // disabled:
                        Setup.flMenuItems |= aItems[ul].ulFlag;
                }

                OwgtSaveSetup(&strSetup,
                              FALSE,            // X-button
                              &Setup);
                pData->pctrSetSetupString(pData->hSettings,
                                          strSetup.psz);
                xstrClear(&strSetup);
            }

            WinDestroyWindow(hwndDlg);
        }

        for (ul = 0;
             ul < ARRAYITEMCOUNT(aItems);
             ul++)
        {
            if (*(aItems[ul].ppcsz))
                free((PSZ)(*(aItems[ul].ppcsz)));
        }

        WinDestroyWindow(hmenuTemp);
    }
}

/* ******************************************************************
 *
 *   PM window class implementation
 *
 ********************************************************************/

/*
 *@@ FindObject:
 *      returns the SOM object pointer for the object handle
 *      which is stored in the widget's setup.
 *
 *      If the HOBJECT is a WPDisk, we return the root folder
 *      for the disk instead.
 *
 *      NOTE: The object is locked by this function. In addition,
 *      we set the OBJLIST_OBJWIDGET list notify flag (see
 *      XFldObject::xwpSetListNotify).
 *
 *@@added V0.9.7 (2000-12-13) [umoeller]
 */

WPObject* FindObject(POBJBUTTONPRIVATE pPrivate)
{
    WPObject *pobj = NULL;

    if (    (pPrivate->Setup.hobj)
         && (pobj = _wpclsQueryObject(_WPObject,
                                      pPrivate->Setup.hobj))
       )
    {
        // dereference shadows
        while ((pobj) && (_somIsA(pobj, _WPShadow)))
            pobj = _wpQueryShadowedObject(pobj, TRUE);

        if (pobj)
        {
            // now, if pObj is a disk object: get root folder
            if (_somIsA(pobj, _WPDisk))
                pobj = wpshQueryRootFolder(pobj, FALSE, NULL);

            if ((pobj) && (pPrivate->pobjNotify != pobj))
            {
                // set list notify so that the widget is destroyed
                // when the object goes dormant
                _wpLockObject(pobj);
                _xwpAddWidgetNotify(pobj,
                                     pPrivate->pWidget->hwndWidget);
                pPrivate->pobjNotify = pobj;
            }
        }
    }

    return (pobj);
}

/*
 *@@ OwgtCreate:
 *      implementation for WM_CREATE.
 */

MRESULT OwgtCreate(HWND hwnd, MPARAM mp1)
{
    MRESULT mrc = 0;
    PXCENTERWIDGET pWidget = (PXCENTERWIDGET)mp1;
    POBJBUTTONPRIVATE pPrivate = NEW(OBJBUTTONPRIVATE);
    ZERO(pPrivate);
    // link the two together
    pWidget->pUser = pPrivate;
    pPrivate->pWidget = pWidget;

    OwgtScanSetup(pWidget->pcszSetupString,
                  &pPrivate->Setup);

    // get type from class
    pPrivate->ulType = pWidget->pWidgetClass->ulExtra;
    if (pPrivate->ulType == BTF_XBUTTON)
        pPrivate->hptrXMini = WinLoadPointer(HWND_DESKTOP,
                                             cmnQueryMainResModuleHandle(),
#ifndef __XWPLITE__
                                             ID_ICONXMINI);
#else
                                             ID_ICONDLG);           // eCS logo
#endif

    // enable context menu help
    pWidget->pcszHelpLibrary = cmnQueryHelpLibrary();
    if (pPrivate->ulType == BTF_XBUTTON)
        pWidget->ulHelpPanelID = ID_XSH_WIDGET_XBUTTON_MAIN;
    else
        pWidget->ulHelpPanelID = ID_XSH_WIDGET_OBJBUTTON_MAIN;


    // return 0 for success
    return (mrc);
}

/*
 *@@ OwgtControl:
 *      implementation for WM_CONTROL.
 *
 *@@added V0.9.7 (2000-12-14) [umoeller]
 *@@changed V0.9.13 (2001-06-21) [umoeller]: added in-use emphasis support
 */

BOOL OwgtControl(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    BOOL brc = FALSE;

    USHORT  usID = SHORT1FROMMP(mp1),
            usNotifyCode = SHORT2FROMMP(mp1);

    PXCENTERWIDGET pWidget;
    POBJBUTTONPRIVATE pPrivate;
    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (POBJBUTTONPRIVATE)pWidget->pUser)
       )
    {
        switch (usID)
        {
            case ID_XCENTER_CLIENT:
                switch (usNotifyCode)
                {
                    /*
                     * XN_QUERYSIZE:
                     *      XCenter wants to know our size.
                     */

                    case XN_QUERYSIZE:
                    {
                        PSIZEL pszl = (PSIZEL)mp2;
                        pszl->cx = pWidget->pGlobals->cxMiniIcon
                                   + 2;    // 2*1 spacing between icon and border
                        if (0 == (pWidget->pGlobals->flDisplayStyle & XCS_FLATBUTTONS))
                            pszl->cx += 4;     // 2*2 for button borders

                        // we wanna be square
                        pszl->cy = pszl->cx;

                        brc = TRUE;
                    }
                    break;

                    /*
                     * XN_SETUPCHANGED:
                     *      XCenter has a new setup string for
                     *      us in mp2.
                     */

                    case XN_SETUPCHANGED:
                    {
                        const char *pcszNewSetupString = (const char*)mp2;

                        // reinitialize the setup data
                        OwgtClearSetup(&pPrivate->Setup);
                        OwgtScanSetup(pcszNewSetupString, &pPrivate->Setup);

                        // WinInvalidateRect(pWidget->hwndWidget, NULL, FALSE);
                    }
                    break;

                    /*
                     * XN_OBJECTDESTROYED:
                     *      member object has been destroyed.
                     *      Destroy ourselves too.
                     */

                    case XN_OBJECTDESTROYED:
                        // simulate "remove widget" context menu command...
                        // that's easier
                        WinPostMsg(pWidget->hwndWidget,
                                   WM_COMMAND,
                                   (MPARAM)ID_CRMI_REMOVEWGT,
                                   NULL);
                    break;

                    /*
                     * XN_INUSECHANGED:
                     *      just repaint.
                     */

                    case XN_INUSECHANGED:
                        WinInvalidateRect(hwnd, NULL, FALSE);
                    break;
                }
            break; // ID_XCENTER_CLIENT

            case ID_XCENTER_TOOLTIP:
                if (usNotifyCode == TTN_NEEDTEXT)
                {
                    PTOOLTIPTEXT pttt = (PTOOLTIPTEXT)mp2;
                    if (pPrivate->ulType == BTF_XBUTTON)
                        pttt->pszText = (PSZ)G_pcszXButton;
                    else
                    {
                        if (!pPrivate->pobjButton)
                            // object not queried yet:
                            pPrivate->pobjButton = FindObject(pPrivate);

                        if (pPrivate->pobjButton)
                            pttt->pszText = _wpQueryTitle(pPrivate->pobjButton);
                        else
                            pttt->pszText = "Invalid object...";
                    }

                    pttt->ulFormat = TTFMT_PSZ;
                }
            break;
        } // switch (usID)
    } // end if (pWidget)

    return (brc);
}

/*
 * OwgtPaintButton:
 *      implementation for WM_PAINT.
 *
 *@@changed V0.9.13 (2001-06-21) [umoeller]: added in-use emphasis support
 */

VOID OwgtPaintButton(HWND hwnd)
{
    RECTL rclPaint;
    PXCENTERWIDGET pWidget;
    POBJBUTTONPRIVATE pPrivate;

    HPS hps;

    if (hps = WinBeginPaint(hwnd, NULLHANDLE, &rclPaint))
    {
        if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
             && (pPrivate = (POBJBUTTONPRIVATE)pWidget->pUser)
           )
        {
            const XCENTERGLOBALS *pGlobals = pWidget->pGlobals;
            // RECTL           rclWin;
            ULONG           fl = XBF_BACKGROUND;        // paint background
            XBUTTONDATA     xbd;

            if (pWidget->pGlobals->flDisplayStyle & XCS_FLATBUTTONS)
                fl |= XBF_FLAT;
            if (    (pPrivate->fMouseButton1Down)
                 || (pPrivate->fButtonSunk)
               )
                fl |= XBF_PRESSED;

            // refresh button data
            WinQueryWindowRect(hwnd, &xbd.rcl);      // exclusive

            xbd.cxIconOrBitmap
            = xbd.cyIconOrBitmap
            = pGlobals->cxMiniIcon;
            xbd.lcol3DDark = pGlobals->lcol3DDark;
            xbd.lcol3DLight = pGlobals->lcol3DLight;
            xbd.lMiddle = pGlobals->lcolClientBackground; // WinQuerySysColor(HWND_DESKTOP, SYSCLR_BUTTONMIDDLE, 0);

            if (pPrivate->ulType == BTF_XBUTTON)
            {
                xbd.hptr = pPrivate->hptrXMini;
            }
            else
            {
                if (!pPrivate->pobjButton)
                    // object not queried yet:
                    pPrivate->pobjButton = FindObject(pPrivate);

                if (pPrivate->pobjButton)
                {
                    PMINIRECORDCORE pmrc;

                    xbd.hptr = _wpQueryIcon(pPrivate->pobjButton);

                    if (    (!(pGlobals->flDisplayStyle & XCS_NOHATCHINUSE))
                         && (pmrc = _wpQueryCoreRecord(pPrivate->pobjButton))
                         && (pmrc->flRecordAttr & CRA_INUSE)
                       )
                    {
                        // this object has in-use emphasis:
                        fl |= XBF_INUSE;
                    }
                }
            }

            ctlPaintXButton(hps,
                            fl,
                            &xbd);

        } // end if (pWidget)

        WinEndPaint(hps);
    } // end if (hps)
}

/*
 *@@ BuildXButtonMenu:
 *      called from OwgtButton1Down to build the
 *      X-button menu before displaying it.
 *
 *      After this, pPrivate->hwndMenuMain has
 *      the new context menu.
 *
 *@@added V0.9.14 (2001-08-21) [umoeller]
 *@@changed V0.9.14 (2001-08-21) [umoeller]: added selective disable of menu items
 */

VOID BuildXButtonMenu(HWND hwnd,
                      POBJBUTTONPRIVATE pPrivate)
{
    WPDesktop *pActiveDesktop = cmnQueryActiveDesktop();
    PSZ pszDesktopTitle = _wpQueryTitle(pActiveDesktop);
    PCKERNELGLOBALS  pKernelGlobals = krnQueryGlobals();
    BOOL fShutdownRunning = xsdIsShutdownRunning();

    HWND hMenu
     = pPrivate->hwndMenuMain
     = WinLoadMenu(hwnd,
                   cmnQueryNLSModuleHandle(FALSE),
                   ID_CRM_XCENTERBUTTON);

    // nuke run if the menu item has been disabled V0.9.14 (2001-08-21) [umoeller]
    if (pPrivate->Setup.flMenuItems & MENUFL_NORUNDLG)
    {
        winhRemoveMenuItem(hMenu, ID_CRMI_RUN);
        winhRemoveMenuItem(hMenu, ID_CRMI_SEP2);
    }

#ifndef __NOXSHUTDOWN__
    if (cmnQuerySetting(sfXShutdown))
        if (cmnQuerySetting(sflXShutdown) & XSD_NOCONFIRM)
        {
            // if XShutdown confirmations have been disabled,
            // remove "..." from the shutdown menu entries
            winhMenuRemoveEllipse(hMenu,
                                  ID_CRMI_RESTARTWPS);
            winhMenuRemoveEllipse(hMenu,
                                  ID_CRMI_SHUTDOWN);
        }
#endif

    if (pPrivate->Setup.flMenuItems & MENUFL_NOSHUTDOWN)
        winhRemoveMenuItem(hMenu, ID_CRMI_SHUTDOWN);
    else
        WinEnableMenuItem(hMenu,
                          ID_CRMI_SHUTDOWN,
                          !fShutdownRunning);
    if (pPrivate->Setup.flMenuItems & MENUFL_NORESTARTWPS)
        winhRemoveMenuItem(hMenu, ID_CRMI_RESTARTWPS);
    else
        WinEnableMenuItem(hMenu,
                          ID_CRMI_RESTARTWPS,
                          !fShutdownRunning);


    if (    (!pKernelGlobals->pXWPShellShared)
         || (pPrivate->Setup.flMenuItems & MENUFL_NOLOGOFF)
       )
    {
        // XWPShell not running:
        // remove "logoff"
        winhRemoveMenuItem(hMenu, ID_CRMI_LOGOFF);
    }
    else
    {
#ifndef __NOXSHUTDOWN__
        if (cmnQuerySetting(sflXShutdown) & XSD_NOCONFIRM)
            // if XShutdown confirmations have been disabled,
            // remove "..." from menu entry
            winhMenuRemoveEllipse(hMenu,
                                  ID_CRMI_LOGOFF);
#endif

        WinEnableMenuItem(hMenu,
                          ID_CRMI_LOGOFF,
                          !fShutdownRunning);
    }

    // nuke lockup if the menu item has been disabled V0.9.14 (2001-08-21) [umoeller]
    if (pPrivate->Setup.flMenuItems & MENUFL_NOLOCKUP)
        winhRemoveMenuItem(hMenu, ID_CRMI_LOCKUPNOW);

    if (pPrivate->Setup.flMenuItems & MENUFL_NOSUSPEND)
        // power has been disabled:
        pPrivate->pPower = NULL;
    else
        // check if we can find the "Power" object
        if (pPrivate->pPower = wpshQueryObjectFromID("<WP_POWER>", NULL))
        {
            if (_somIsA(pPrivate->pPower, _WPPower))
            {
                // is power management enabled?
                if (!_wpQueryPowerManagement(pPrivate->pPower))
                    // no:
                    pPrivate->pPower = NULL;
            }
            else
                pPrivate->pPower = NULL;
        }

    if (!pPrivate->pPower)
        winhRemoveMenuItem(hMenu, ID_CRMI_SUSPEND);
    else
        // power exists:
        if (!_wpQueryPowerConfirmation(pPrivate->pPower))
            // if power confirmations have been disabled,
            // remove "..." from menu entry
            winhMenuRemoveEllipse(hMenu,
                                  ID_CRMI_SUSPEND);

    // prepare folder content submenu for Desktop
    cmnuPrepareContentSubmenu(pActiveDesktop,
                              hMenu,
                              pszDesktopTitle,
                              0,        // top item
                              FALSE); // no owner draw in main context menu
}

/*
 * OwgtButton1Down:
 *      implementation for WM_BUTTON1DOWN.
 *
 *@@changed V0.9.14 (2001-07-24) [lafaix]: fixed menu position
 */

VOID OwgtButton1Down(HWND hwnd)
{
    PXCENTERWIDGET pWidget;
    POBJBUTTONPRIVATE pPrivate;
    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (POBJBUTTONPRIVATE)pWidget->pUser)
       )
    {
        if (WinIsWindowEnabled(hwnd))
        {
            pPrivate->fMouseButton1Down = TRUE;
            WinInvalidateRect(hwnd, NULL, FALSE);

            // since we're not passing the message
            // to WinDefWndProc, we need to give
            // ourselves the focus; this will also
            // dismiss the button's menu, if open
            WinSetFocus(HWND_DESKTOP, hwnd);

            if (!pPrivate->fMouseCaptured)
            {
                // capture mouse events while the
                // mouse button is down
                WinSetCapture(HWND_DESKTOP, hwnd);
                pPrivate->fMouseCaptured = TRUE;
            }

            if (!pPrivate->fButtonSunk)
            {
                // PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
                // toggle state is still UP (i.e. button pressed
                // for the first time): create menu

                // prepare globals in fdrmenus.c
                cmnuInitItemCache();

                if (pPrivate->ulType == BTF_XBUTTON)
                    // it's an X-button: load default menu
                    BuildXButtonMenu(hwnd, pPrivate);
                else
                {
                    // regular object button:
                    // check if this is a folder...

                    if (!pPrivate->pobjButton)
                        // object not queried yet:
                        pPrivate->pobjButton = FindObject(pPrivate);

                    if (pPrivate->pobjButton)
                    {
                        if (_somIsA(pPrivate->pobjButton, _WPFolder))
                        {
                            // yes, it's a folder:
                            // prepare folder content menu by inserting
                            // a dummy menu item... the actual menu
                            // fill is done for WM_INITMENU, which comes
                            // in right afterwards
                            pPrivate->hwndMenuMain = WinCreateMenu(hwnd, NULL);
                            WinSetWindowBits(pPrivate->hwndMenuMain,
                                             QWL_STYLE,
                                             MIS_OWNERDRAW,
                                             MIS_OWNERDRAW);

                            // insert a dummy menu item so that cmnuPrepareOwnerDraw
                            // can measure its size
                            winhInsertMenuItem(pPrivate->hwndMenuMain,
                                               0,
                                               cmnQuerySetting(sulVarMenuOffset) + ID_XFMI_OFS_DUMMY,
                                               "test",
                                               MIS_TEXT,
                                               0);
                        }
                        // else no folder:
                        // do nothing at this point, just paint the
                        // button depressed... we'll open the object
                        // on button-up
                    }
                }

                if (pPrivate->hwndMenuMain)
                {
/*                    RECTL rclButton;
                    WinQueryWindowRect(hwnd, &rclButton);
                    // rclButton now has button coordinates;
                    // convert this to screen coordinates:
                    WinMapWindowPoints(hwnd,
                                       HWND_DESKTOP,
                                       (PPOINTL)&rclButton,
                                       2);          // rectl == 2 points

                    if (pWidget->pGlobals->ulPosition == XCENTER_TOP)
                        cmnuSetPositionBelow((PPOINTL)&rclButton); */

                    if (pPrivate->ulType == BTF_OBJBUTTON)
                    {
                        // object buttons needs special treatment here
                        // so that the menu is correctly positionned

                        RECTL rclButton;
                        WinQueryWindowRect(hwnd, &rclButton);
                        // rclButton now has button coordinates;
                        // convert this to screen coordinates:
                        WinMapWindowPoints(hwnd,
                                           HWND_DESKTOP,
                                           (PPOINTL)&rclButton,
                                           2);          // rectl == 2 points

                        if (pWidget->pGlobals->ulPosition == XCENTER_TOP)
                            cmnuSetPositionBelow((PPOINTL)&rclButton);
                    }

                    ctrPlaceAndPopupMenu(hwnd,
                                         pPrivate->hwndMenuMain,
                                         pWidget->pGlobals->ulPosition == XCENTER_BOTTOM);
                }
            } // end if (!pPrivate->fButtonSunk)
        }
    } // end if (pWidget)
}

/*
 * OwgtButton1Up:
 *      implementation for WM_BUTTON1UP.
 */

VOID OwgtButton1Up(HWND hwnd)
{
    PXCENTERWIDGET pWidget;
    POBJBUTTONPRIVATE pPrivate;
    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (POBJBUTTONPRIVATE)pWidget->pUser)
       )
    {
        if (WinIsWindowEnabled(hwnd))
        {
            // un-capture the mouse first
            if (pPrivate->fMouseCaptured)
            {
                WinSetCapture(HWND_DESKTOP, NULLHANDLE);
                pPrivate->fMouseCaptured = FALSE;
            }

            pPrivate->fMouseButton1Down = FALSE;

            // toggle state with each WM_BUTTON1UP
            pPrivate->fButtonSunk = !pPrivate->fButtonSunk;

            if (pPrivate->ulType == BTF_OBJBUTTON)
                // we have an object button (not X-button):
                if (pPrivate->pobjButton)
                    // object was successfully retrieved on button-down:
                    if (!_somIsA(pPrivate->pobjButton, _WPFolder))
                    {
                        // object is not a folder:
                        // open it on button up!
                        // V0.9.16 (2002-01-04) [umoeller]: do this on thread 1
                        // always, or we get very strange system hangs with
                        // some executables
                        krnPostThread1ObjectMsg(T1M_OPENOBJECTFROMPTR,
                                                (MPARAM)pPrivate->pobjButton,
                                                (MPARAM)OPEN_DEFAULT);

                        // unset button sunk state
                        // (no toggle)
                        pPrivate->fButtonSunk = FALSE;
                    }
                    // else folder: we do nothing, the work for the menu
                    // has been set up in button-down and init-menu

            // repaint sunk button state
            WinInvalidateRect(hwnd, NULL, FALSE);
        }
    } // end if (pWidget)
}

/*
 * OwgtInitMenu:
 *      implementation for WM_INITMENU.
 *
 *      Note that this comes only in for...
 *
 *      -- the X-button;
 *
 *      -- an object button if the object button represents
 *         a folder (or disk).
 */

VOID OwgtInitMenu(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    PXCENTERWIDGET pWidget;
    POBJBUTTONPRIVATE pPrivate;
    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (POBJBUTTONPRIVATE)pWidget->pUser)
       )
    {
        // PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
        SHORT sMenuIDMsg = (SHORT)mp1;
        HWND hwndMenuMsg = (HWND)mp2;

        if (   (pPrivate->ulType == BTF_OBJBUTTON)
            && (hwndMenuMsg == pPrivate->hwndMenuMain)
           )
        {
            // WM_INITMENU for object's button main menu:
            // we then need to load the objects directly into
            // the menu

            cmnuPrepareOwnerDraw(hwndMenuMsg);

            // remove dummy item
            winhRemoveMenuItem(pPrivate->hwndMenuMain,
                               cmnQuerySetting(sulVarMenuOffset) + ID_XFMI_OFS_DUMMY);

            if (!pPrivate->pobjButton)
                // object not queried yet:
                pPrivate->pobjButton = FindObject(pPrivate);

            if (pPrivate->pobjButton)
            {
                // just to make sure it's a folder:
                if (_somIsA(pPrivate->pobjButton, _WPFolder))
                {
                    // show "Wait" pointer
                    HPOINTER    hptrOld = winhSetWaitPointer();

                    // populate
                    fdrCheckIfPopulated(pPrivate->pobjButton,
                                        FALSE);    // full populate

                    WinSetPointer(HWND_DESKTOP, hptrOld);

                    if (_wpQueryContent(pPrivate->pobjButton, NULL, QC_FIRST))
                    {
                        // folder does contain objects: go!
                        // insert all objects (this takes a long time)...
                        cmnuInsertObjectsIntoMenu(pPrivate->pobjButton,
                                                  pPrivate->hwndMenuMain);
                    }
                }
            } // end if (pobj)

            // mark this as non-WPS context menu
            pPrivate->fOpenedWPSContextMenu = FALSE;

        } // end if (   (pPrivate->ulType == BTF_OBJBUTTON) ...
        else
        {
            // find out whether the menu of which we are notified
            // is a folder content menu; if so (and it is not filled
            // yet), the first menu item is ID_XFMI_OFS_DUMMY
            if ((ULONG)WinSendMsg(hwndMenuMsg,
                                  MM_ITEMIDFROMPOSITION,
                                  (MPARAM)0,        // menu item index
                                  MPNULL)
                       == (cmnQuerySetting(sulVarMenuOffset) + ID_XFMI_OFS_DUMMY))
            {
               // okay, let's go
#ifndef __NOFOLDERCONTENTS__
               if (cmnQuerySetting(sfFolderContentShowIcons))
#endif
               {
                   // show folder content icons ON:
                   cmnuPrepareOwnerDraw(hwndMenuMsg);
               }

               // add menu items according to folder contents
               cmnuFillContentSubmenu(sMenuIDMsg,
                                      hwndMenuMsg);
            }
        }
    }
}

/*
 * OwgtMenuEnd:
 *      implementation for WM_MENUEND.
 */

VOID OwgtMenuEnd(HWND hwnd, MPARAM mp2)
{
    PXCENTERWIDGET pWidget;
    POBJBUTTONPRIVATE pPrivate;
    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (POBJBUTTONPRIVATE)pWidget->pUser)
       )
    {
        if ((HWND)mp2 == pPrivate->hwndMenuMain)
        {
            // main menu is ending:
            if (!pPrivate->fMouseButton1Down)
            {
                // mouse button not currently down
                // --> menu dismissed for some other reason:
                pPrivate->fButtonSunk = FALSE;
                WinInvalidateRect(hwnd, NULL, FALSE);
            }

            WinDestroyWindow(pPrivate->hwndMenuMain);
            pPrivate->hwndMenuMain = NULLHANDLE;
        }

        if ((HWND)mp2 == pPrivate->hwndObjectPopup)
        {
            // object popup (copy of WPS context menu for object button):
            WinDestroyWindow(pPrivate->hwndObjectPopup);
            pPrivate->hwndObjectPopup = NULLHANDLE;
            // remove source emphasis
            WinInvalidateRect(pWidget->pGlobals->hwndClient, NULL, FALSE);
        }
    } // end if (pWidget)
}

/*
 *@@ OwgtCommand:
 *      implementation for WM_COMMAND.
 *
 *      If this returns FALSE, the parent winproc is called.
 *
 *@@changed V0.9.9 (2001-03-07) [umoeller]: added "run" to X-button
 *@@changed V0.9.11 (2001-04-18) [umoeller]: now opening objects on thread 1 always
 *@@changed V0.9.11 (2001-04-25) [umoeller]: fixed broken standard widget menu items
 *@@changed V0.9.12 (2001-04-28) [umoeller]: moved XShutdown init to thread 1 object window
 *@@changed V0.9.12 (2001-05-01) [umoeller]: added lockup
 */

BOOL OwgtCommand(HWND hwnd, MPARAM mp1)
{
    BOOL fProcessed = FALSE;
    ULONG ulMenuId = (ULONG)mp1;

    PXCENTERWIDGET pWidget;
    POBJBUTTONPRIVATE pPrivate;
    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (POBJBUTTONPRIVATE)pWidget->pUser)
       )
    {
        if (pPrivate->ulType == BTF_XBUTTON)
        {
            fProcessed = TRUE;

            switch (ulMenuId)
            {
                case ID_CRMI_LOCKUPNOW:     // added V0.9.12 (2001-05-01) [umoeller]
                    // we won't bother with the details,
                    // just post the menu command to the desktop
                    WinPostMsg(cmnQueryActiveDesktopHWND(),
                               WM_COMMAND,
                               MPFROMSHORT(WPMENUID_LOCKUP), // 705,
                               MPFROM2SHORT(CMDSRC_MENU,
                                            FALSE));            // who cares
                break;

                case ID_CRMI_SUSPEND:
                    // check if the "Power" object has been set up
                    // in OwgtInitMenu
                    if (pPrivate->pPower)
                    {
                        BOOL fGo = FALSE;
                        if (_wpQueryPowerConfirmation(pPrivate->pPower))
                            // yeah, that's funny: why do they export this function
                            // and have this setting, and wpChangePowerState doesn't
                            // confirm anything?!?
                            // so do it now
                            fGo = (cmnMessageBoxMsg(pWidget->hwndWidget,
                                                    197,        // xcenter
                                                    198,        // sure suspend?
                                                    MB_YESNO)
                                            == MBID_YES);
                        else
                            fGo = TRUE;

                        if (fGo)
                        {
                            // sleep a little while... otherwise the
                            // "key up" or tiny "mouse move" will immediately
                            // wake up the system again
                            winhSleep(300);
                            // tell "Power" object to suspend
                            _wpChangePowerState(pPrivate->pPower,
                                                MAKEULONG(6,        // set power state
                                                          0),       // reserved
                                                MAKEULONG(1,        // all devices
                                                          2));      // suspend
                        }
                    }
                break;

                case ID_CRMI_LOGOFF:
                case ID_CRMI_RESTARTWPS:
                case ID_CRMI_SHUTDOWN:
                    // do this on thread 1, or otherwise we'll
                    // get problems with the XCenter thread
                    // V0.9.12 (2001-04-28) [umoeller]
                    krnPostThread1ObjectMsg(T1M_INITIATEXSHUTDOWN,
                                            (MPARAM)ulMenuId,
                                            0);
                break;

                case ID_CRMI_RUN:       // V0.9.9 (2001-03-07) [umoeller]
                    krnPostThread1ObjectMsg(T1M_OPENRUNDIALOG,
                                            MPNULL,
                                            MPNULL);        // boot drive
                    /* cmnRunCommandLine(pWidget->pGlobals->hwndFrame,
                                      NULL);        // boot drive
                    */
                break;

                default:
                    fProcessed = FALSE;
            }
        } // if (pPrivate->ulType == BTF_XBUTTON)

        if (!fProcessed)
        {
            // we get here...
            // -- for object buttons; fProcessed is still FALSE
            // -- for the x-button if none of the standard items
            //    was selected; this can be a subitem of "desktop" folder contents too
            // PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
            ULONG ulFirstVarMenuId = cmnQuerySetting(sulVarMenuOffset) + ID_XFMI_OFS_VARIABLE;
            if (     (ulMenuId >= ulFirstVarMenuId)
                  && (ulMenuId <  ulFirstVarMenuId + G_ulVarItemCount)
                  && (ulMenuId <  0x7f00)       // standard widget menu IDs
               )
            {
                // yes, variable menu item selected:
                // get corresponding menu list item from the list that
                // was created by mnuModifyFolderPopupMenu
                PVARMENULISTITEM pItem = cmnuGetVarItem(ulMenuId - ulFirstVarMenuId);
                WPObject    *pObject = NULL;

                if (pItem)
                    pObject = pItem->pObject;

                if (pObject)    // defaults to NULL
                {
                    // _wpViewObject(pObject, NULLHANDLE, OPEN_DEFAULT, 0);
                    // no.... we're running on the XCenter thread here
                    // and we don't want the objects to open on the XCenter
                    // thread because if the XCenter is closed, all those
                    // views will go away (because the thread's msgq is
                    // destroyed)... redirect this to thread 1
                    // V0.9.11 (2001-04-18) [umoeller]
                    krnPostThread1ObjectMsg(T1M_OPENOBJECTFROMPTR,
                                           (MPARAM)pObject,
                                           (MPARAM)OPEN_DEFAULT);

                    fProcessed = TRUE;
                }
            } // end if ((ulMenuId >= ID_XFM_VARIABLE) && (ulMenuId < ID_XFM_VARIABLE+varItemCount))
            else
                // other:
                // this MIGHT be a command from a WPS context menu...
                if (    pPrivate->ulType == BTF_OBJBUTTON
                     && pPrivate->fOpenedWPSContextMenu
                     && pPrivate->pobjButton
                     && (ulMenuId <  0x7f00)       // standard widget menu IDs
                   )
                {
                    // invoke the command on the object...
                    // this is probably "open" or "help"

                    // but do this on thread 1 also V0.9.11 (2001-04-18) [umoeller]
                    krnPostThread1ObjectMsg(T1M_MENUITEMSELECTED,
                                            (MPARAM)pPrivate->pobjButton,
                                            (MPARAM)ulMenuId);
                    fProcessed = TRUE;
                    /* fProcessed = _wpMenuItemSelected(pPrivate->pobjButton,
                                                     NULLHANDLE,     // hwndFrame
                                                     ulMenuId); */
                }
        }

        pPrivate->fOpenedWPSContextMenu = FALSE;
    }

    return (fProcessed);
}

/*
 *@@ OwgtContextMenu:
 *      implementation for WM_CONTEXTMENU.
 *
 *      For object buttons, this does some major hacks to
 *      display part of the object's WPS context menu on
 *      the object button.
 *
 *      For X-buttons, this calls ctrDefWidgetProc only.
 *
 *@@added V0.9.9 (2001-03-07) [umoeller]
 *@@changed V0.9.11 (2001-04-25) [umoeller]: fixed context menus for broken objects
 */

MRESULT OwgtContextMenu(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    PXCENTERWIDGET pWidget;
    POBJBUTTONPRIVATE pPrivate;
    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (POBJBUTTONPRIVATE)pWidget->pUser)
       )
    {
        pPrivate->fOpenedWPSContextMenu = FALSE;

        if (pPrivate->ulType == BTF_OBJBUTTON)
        {
            // for object buttons, show the WPS context menu

            if (!pPrivate->pobjButton)
                // object not queried yet:
                pPrivate->pobjButton = FindObject(pPrivate);

            if (pPrivate->pobjButton)
            {
                // SHORT sIndex;
                POINTL ptl;
                HWND hmenuTemp;
                ptl.x = SHORT1FROMMP(mp1);
                ptl.y = SHORT2FROMMP(mp1);

#ifndef MENU_NODISPLAY
#define MENU_NODISPLAY            0x40000000
#endif
                // compose the object menu... we can't let
                // the WPS display it because the WPS expects
                // the object to be in a PM container. But
                // the half-documented MENU_NODISPLAY flag
                // takes care of this: it will only build the
                // menu, but not display it.
                // NOTE: Apparently this flag is not supported
                // on Warp 3 (tested Warp 3 without fixpaks
                // V0.9.11 (2001-04-25) [umoeller]).
                hmenuTemp = _wpDisplayMenu(pPrivate->pobjButton,
                                           hwnd,            // owner
                                           NULLHANDLE,
                                           &ptl,
                                           MENU_OBJECTPOPUP | MENU_NODISPLAY,
                                           0);

                if (hmenuTemp)  // V0.9.11 (2001-04-25) [umoeller]
                {
                    // NOW... we still can't use this because there's
                    // many menu items in there which will cause the
                    // WPS to hang if the owner is not a container.
                    // The WPS simply expects popups to show in containers
                    // only, so there ain't much we can do about this.
                    // While "copy", "move" etc. will simply not work,
                    // "Pickup" will even hang the WPS solidly.

                    // SOOOO.... what we do is make a COPY of the
                    // WPS context menu with only the items that we support.
                    pPrivate->hwndObjectPopup = WinCreateMenu(HWND_DESKTOP, NULL);
                    if (pPrivate->hwndObjectPopup)
                    {
                        HWND hwndWidgetSubmenu;

                        winhCopyMenuItem(pPrivate->hwndObjectPopup,
                                         hmenuTemp,
                                         WPMENUID_OPEN, // 1, "open" submenu
                                         MIT_END);
                        winhCopyMenuItem(pPrivate->hwndObjectPopup,
                                         hmenuTemp,
                                         WPMENUID_PROPERTIES, // 0x70, properties
                                         MIT_END);
                        winhCopyMenuItem(pPrivate->hwndObjectPopup,
                                         hmenuTemp,
                                         WPMENUID_HELP, // 2, "help" submenu
                                         MIT_END);

                        winhInsertMenuSeparator(pPrivate->hwndObjectPopup,
                                                MIT_END,
                                                1234);

                        // add standard widget menu as submenu
                        hwndWidgetSubmenu
                            = winhMergeIntoSubMenu(pPrivate->hwndObjectPopup,
                                                   MIT_END,
                                                   cmnGetString(ID_XSSI_XC_OBJBUTTONWIDGET),
                                                   2000,    // submenu ID
                                                   pPrivate->pWidget->hwndContextMenu);
                        if (hwndWidgetSubmenu)
                            // disable "Properties"... we have none
                            WinEnableMenuItem(hwndWidgetSubmenu,
                                              ID_CRMI_PROPERTIES,
                                              FALSE);

                        ctrShowContextMenu(pWidget, pPrivate->hwndObjectPopup);
                                // destroyed on WM_MENUEND;
                                // we need not care about the WPS context
                                // menu we built because the WPS has its
                                // internal management for that
                        pPrivate->fOpenedWPSContextMenu = TRUE;
                    } // end if (pPrivate->hwndObjectPopup)
                } // end if (hmenuTemp)  // V0.9.11 (2001-04-25) [umoeller]
            } // end if (pPrivate->pobjButton)
        } // end if (pPrivate->ulType == BTF_OBJBUTTON)

        if (!pPrivate->fOpenedWPSContextMenu) // V0.9.11 (2001-04-25) [umoeller]
            // x-button, or cannot build WPS popup (Warp 3),
            // or object broken: show default widget menu
            mrc = ctrDefWidgetProc(hwnd, WM_CONTEXTMENU, mp1, mp2);
    }

    return (mrc);
}

#define WIDGET_DRAG_MECH "DRM_XCENTERWIDGET"

/*
 *@@ OwgtDragover:
 *      implementation for DM_DRAGOVER.
 *
 *@@added V0.9.13 (2001-06-19) [umoeller]
 *@@changed V0.9.14 (2001-08-05) [lafaix]: refuses move/default for DRM_XCENTERWIDGET
 */

MRESULT OwgtDragover(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    PDRAGINFO   pdrgInfo = (PDRAGINFO)mp1;
    // default return values
    MRESULT     mrc = MRFROM2SHORT(DOR_NEVERDROP,
                    // cannot be dropped, and don't send
                    // DM_DRAGOVER again
                                   DO_UNKNOWN);
                    // target-defined drop operation:
                    // user operation (we don't want
                    // the WPS to copy anything)

    PXCENTERWIDGET pWidget;
    POBJBUTTONPRIVATE pPrivate;
    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (POBJBUTTONPRIVATE)pWidget->pUser)
       )
    {
        // only object button can accept this
        if (pPrivate->ulType == BTF_OBJBUTTON)
        {
            if (!pPrivate->pobjButton)
                // object not queried yet:
                pPrivate->pobjButton = FindObject(pPrivate);

            if (pPrivate->pobjButton)
            {
                BOOL bDragOver = TRUE;

                ctrDrawWidgetEmphasis(pWidget,
                                      FALSE);
                pPrivate->fHasDragoverEmphasis = TRUE;

                // if the user is currently dragging a widget, we
                // must reject either a default or a move drop
                // (so that moving widgets remain easy).
                // V0.9.14 (2001-08-05) [lafaix]
                if (DrgAccessDraginfo(pdrgInfo))
                {
                    if (    (    (pdrgInfo->usOperation == DO_DEFAULT)
                              || (pdrgInfo->usOperation == DO_MOVE)
                            )
                         && (pdrgInfo->cditem == 1)
                       )
                    {
                        PDRAGITEM pdrgItem = DrgQueryDragitemPtr(pdrgInfo, 0);
                        if (DrgVerifyRMF(pdrgItem,
                                         WIDGET_DRAG_MECH, // mechanism
                                         NULL))            // any format
                        {
                            mrc = MRFROM2SHORT(DOR_NODROP, DO_DEFAULT);
                            bDragOver = FALSE;
                        }
                    }

                    DrgFreeDraginfo(pdrgInfo);
                }

                if (bDragOver)
                    mrc = _wpDragOver(pPrivate->pobjButton,
                                      NULLHANDLE,           // cnr
                                      pdrgInfo);
            }
        }
    }

    // and return the drop flags
    return (mrc);
}

/*
 *@@ OwgtDragLeave:
 *      implementaton for DM_DRAGLEAVE. Always returns 0.
 *
 *@@added V0.9.13 (2001-06-19) [umoeller]
 */

VOID OwgtDragLeave(HWND hwnd)
{
    PXCENTERWIDGET pWidget;
    POBJBUTTONPRIVATE pPrivate;
    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (POBJBUTTONPRIVATE)pWidget->pUser)
       )
    {
        // only object button can accept this
        if (pPrivate->ulType == BTF_OBJBUTTON)
        {
            if (!pPrivate->pobjButton)
                // object not queried yet:
                pPrivate->pobjButton = FindObject(pPrivate);

            if (pPrivate->pobjButton)
            {
                if (pPrivate->fHasDragoverEmphasis)
                {
                    ctrDrawWidgetEmphasis(pWidget,
                                          TRUE);        // remove
                    pPrivate->fHasDragoverEmphasis = FALSE;
                }
            }
        }
    }
}

/*
 *@@ OwgtDrop:
 *      implementaton for DM_DROP. Always returns 0.
 *
 *@@added V0.9.13 (2001-06-19) [umoeller]
 *@@changed V0.9.14 (2001-08-05) [lafaix]: fixed incorrect draginfo usage
 *@@changed V0.9.14 (2001-08-05) [lafaix]: refuses move/default drops for DRM_XCENTERWIDGET
 */

VOID OwgtDrop(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    PDRAGINFO   pdrgInfo = (PDRAGINFO)mp1;

    PXCENTERWIDGET pWidget;
    POBJBUTTONPRIVATE pPrivate;
    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (POBJBUTTONPRIVATE)pWidget->pUser)
       )
    {
        // only object button can accept this
        if (    (pPrivate->ulType == BTF_OBJBUTTON)
                // pdrgInfo must be accessed before use V0.9.14 (2001-08-05)
             && (DrgAccessDraginfo(pdrgInfo))
           )
        {
            if (!pPrivate->pobjButton)
                // object not queried yet:
                pPrivate->pobjButton = FindObject(pPrivate);

            if (pPrivate->pobjButton)
            {
                PDRAGITEM pdrgItem = DrgQueryDragitemPtr(pdrgInfo,
                                                         0);

                if (pPrivate->fHasDragoverEmphasis)
                {
                    ctrDrawWidgetEmphasis(pWidget,
                                          TRUE);        // remove
                    pPrivate->fHasDragoverEmphasis = FALSE;
                }

                // if it is a widget drop, rejects default or move drop
                // (so that moving widgets remain easy)
                // V0.9.14 (2001-08-05) [lafaix]
                if (    (    (pdrgInfo->usOperation == DO_DEFAULT)
                          || (pdrgInfo->usOperation == DO_MOVE)
                        )
                     && (pdrgInfo->cditem == 1)
                     && (DrgVerifyRMF(pdrgItem,
                                      WIDGET_DRAG_MECH, // mechanism
                                      NULL))            // any format
                   )
                {
                    // do nothing
                }
                else
                    _wpDrop(pPrivate->pobjButton,
                            NULLHANDLE,           // cnr
                            pdrgInfo,
                            pdrgItem);
            }

            DrgFreeDraginfo(pdrgInfo);
        }
    }
}

/*
 *@@ OwgtDestroy:
 *      implementation for WM_DESTROY.
 *
 *@@added V0.9.12 (2001-05-24) [umoeller]
 */

VOID OwgtDestroy(HWND hwnd)
{
    PXCENTERWIDGET pWidget;
    POBJBUTTONPRIVATE pPrivate;
    if (    (pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER))
         && (pPrivate = (POBJBUTTONPRIVATE)pWidget->pUser)
       )
    {
        if (pPrivate->hptrXMini)
            WinDestroyPointer(pPrivate->hptrXMini);

        if (pPrivate->pobjNotify)
        {
            // @@todo memory leak still here... does this
            // even get called?!? V0.9.12 (2001-05-24) [umoeller]
            _xwpRemoveDestroyNotify(pPrivate->pobjNotify,
                                    hwnd);
            _wpUnlockObject(pPrivate->pobjNotify);
        }

        // free private data
        free(pPrivate);
                // pWidget is cleaned up by DestroyWidgets
    }
}

/*
 *@@ fnwpObjButtonWidget:
 *      window procedure for the desktop button widget class.
 *
 *      This is also the owner for the menu it displays and
 *      therefore handles control of the folder content menus.
 *
 *@@changed V0.9.13 (2001-06-19) [umoeller]: added d'n'd support
 */

MRESULT EXPENTRY fnwpObjButtonWidget(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    switch (msg)
    {
        /*
         * WM_CREATE:
         *      as with all widgets, we receive a pointer to the
         *      XCENTERWIDGET in mp1, which was created for us.
         *
         *      The first thing the widget MUST do on WM_CREATE
         *      is to store the XCENTERWIDGET pointer (from mp1)
         *      in the QWL_USER window word by calling:
         *
         *          WinSetWindowPtr(hwnd, QWL_USER, mp1);
         *
         *      We use XCENTERWIDGET.pUser for allocating
         *      OBJBUTTONPRIVATE for our own stuff.
         *
         *      Each widget must write its desired width into
         *      XCENTERWIDGET.cx and cy.
         */

        case WM_CREATE:
            WinSetWindowPtr(hwnd, QWL_USER, mp1);
            mrc = OwgtCreate(hwnd, mp1);
        break;

        /*
         * WM_CONTROL:
         *      process notifications/queries from the XCenter.
         */

        case WM_CONTROL:
            mrc = (MPARAM)OwgtControl(hwnd, mp1, mp2);
        break;

        /*
         * WM_PAINT:
         *
         */

        case WM_PAINT:
            OwgtPaintButton(hwnd);
        break;

        /*
         * WM_BUTTON1DOWN:
         * WM_BUTTON1UP:
         *      these show/hide the menu.
         *
         *      Showing the menu follows these steps:
         *          a)  first WM_BUTTON1DOWN hilites the button;
         *          b)  first WM_BUTTON1UP shows the menu.
         *
         *      When the button is pressed again, the open
         *      menu loses focus, which results in WM_MENUEND
         *      and destroys the window automatically.
         */

        case WM_BUTTON1DOWN:
        case WM_BUTTON1DBLCLK:
            OwgtButton1Down(hwnd);
            mrc = (MPARAM)TRUE;     // message processed
        break;

        /*
         * WM_BUTTON1UP:
         *
         */

        case WM_BUTTON1UP:
            OwgtButton1Up(hwnd);
            mrc = (MPARAM)TRUE;     // message processed
        break;

        /*
         * WM_BUTTON1CLICK:
         *      swallow this
         */

        case WM_BUTTON1CLICK:
            mrc = (MPARAM)TRUE;
        break;

        /*
         * WM_INITMENU:
         *
         */

        case WM_INITMENU:
            OwgtInitMenu(hwnd, mp1, mp2);
        break;

        /*
         * WM_MEASUREITEM:
         *      this msg is sent only once per owner-draw item when
         *      PM needs to know its size. This gets sent to us for
         *      items in folder content menus (if icons are on); the
         *      height of our items will be the same as with
         *      non-owner-draw ones, but we need to calculate the width
         *      according to the item text.
         *
         *      (SHORT)mp1 is supposed to contain a "menu identifier",
         *      but from my testing this contains some random value.
         *
         *      Return value: check mnuMeasureItem.
         */

        case WM_MEASUREITEM:
            mrc = cmnuMeasureItem((POWNERITEM)mp2);
        break;

        /*
         * WM_DRAWITEM:
         *      this msg is sent for each item every time it
         *      needs to be redrawn. This gets sent to us for
         *      items in folder content menus (if icons are on).
         *
         *      (SHORT)mp1 is supposed to contain a "menu identifier",
         *      but from my testing this contains some random value.
         */

        case WM_DRAWITEM:
            if (cmnuDrawItem(mp1,
                             mp2))
                mrc = (MRESULT)TRUE;
        break;

        /*
         * WM_MENUEND:
         *
         */

        case WM_MENUEND:
            OwgtMenuEnd(hwnd, mp2);
            mrc = ctrDefWidgetProc(hwnd, msg, mp1, mp2);
        break;

        /*
         * WM_COMMAND:
         *      handle command from menus.
         */

        case WM_COMMAND:
            if (!OwgtCommand(hwnd, mp1))
                // not processed:
                mrc = ctrDefWidgetProc(hwnd, msg, mp1, mp2);
        break;

        /*
         * WM_CONTEXTMENU:
         *
         */

        case WM_CONTEXTMENU:
            mrc = OwgtContextMenu(hwnd, mp1, mp2);
        break;

        /*
         * DM_DRAGOVER:
         */

        case DM_DRAGOVER:
            mrc = OwgtDragover(hwnd, mp1, mp2);
        break;

        /*
         * DM_DRAGLEAVE:
         *
         */

        case DM_DRAGLEAVE:
            OwgtDragLeave(hwnd);
        break;

        /*
         * DM_DROP:
         *
         */

        case DM_DROP:
            OwgtDrop(hwnd, mp1, mp2);
        break;

        /*
         * WM_DESTROY:
         *      clean up. This _must_ be passed on to
         *      ctrDefWidgetProc.
         */

        case WM_DESTROY:
            OwgtDestroy(hwnd);
            mrc = ctrDefWidgetProc(hwnd, msg, mp1, mp2);
        break;

        default:
            mrc = ctrDefWidgetProc(hwnd, msg, mp1, mp2);
    } // end switch(msg)

    return (mrc);
}


