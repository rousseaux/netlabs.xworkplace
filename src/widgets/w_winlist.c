
/*
 *@@sourcefile w_winlist.c:
 *      XCenter "Window list" widget.
 *
 *      This is an example of an XCenter widget plugin.
 *      This widget resides in WINLIST.DLL, which (as
 *      with all widget plugins) must be put into the
 *      plugins/xcenter directory of the XWorkplace
 *      installation directory.
 *
 *      Any XCenter widget plugin DLL must export the
 *      following procedures by ordinal:
 *
 *      -- Ordinal 1 (WwgtInitModule): this must
 *         return the widgets which this DLL provides.
 *
 *      -- Ordinal 2 (WwgtUnInitModule): this must
 *         clean up global DLL data.
 *
 *      The makefile in src\widgets compiles widgets
 *      with the VAC subsystem library. As a result,
 *      multiple threads are not supported.
 *
 *      This is all new with V0.9.7.
 *
 *@@added V0.9.7 (2000-12-06) [umoeller]
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
ew */

#define INCL_DOSMODULEMGR
#define INCL_DOSERRORS

#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WINDIALOGS
#define INCL_WININPUT
#define INCL_WINSWITCHLIST
#define INCL_WINRECTANGLES
#define INCL_WINPOINTERS
#define INCL_WINSYS
#define INCL_WINLISTBOXES
#define INCL_WINENTRYFIELDS

#define INCL_GPIPRIMITIVES
#define INCL_GPILOGCOLORTABLE
#include <os2.h>

// C library headers
#include <stdio.h>
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\gpih.h"               // GPI helper routines
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\prfh.h"               // INI file helper routines;
                                        // this include is required for some
                                        // of the structures in shared\center.h
#include "helpers\timer.h"              // replacement PM timers
#include "helpers\winh.h"               // PM helper routines
#include "helpers\xstring.h"            // extended string helpers

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\center.h"              // public XCenter interfaces
#include "shared\common.h"              // the majestic XWorkplace include file

#pragma hdrstop                     // VAC++ keeps crashing otherwise

/* ******************************************************************
 *
 *   Private definitions
 *
 ********************************************************************/

// width of outer widget border:
#define WIDGET_BORDER       1
// width of window button borders:
#define BUTTON_BORDER       2

// string used for separating filters in setup strings;
// this better not appear in window titles
const char *G_pcszFilterSeparator = "#~^ø@";

VOID EXPENTRY WwgtShowSettingsDlg(PWIDGETSETTINGSDLGDATA pData);

/* ******************************************************************
 *
 *   XCenter widget class definition
 *
 ********************************************************************/

/*
 *      This contains the name of the PM window class and
 *      the XCENTERWIDGETCLASS definition(s) for the widget
 *      class(es) in this DLL.
 */

#define WNDCLASS_WIDGET_WINLIST "XWPCenterWinlistWidget"

XCENTERWIDGETCLASS G_WidgetClasses
    = {
        WNDCLASS_WIDGET_WINLIST,
        0,
        "WindowList",
        "Window list",
        WGTF_UNIQUEPERXCENTER,
        WwgtShowSettingsDlg
      };

/* ******************************************************************
 *
 *   Function imports from XFLDR.DLL
 *
 ********************************************************************/

/*
 *      To reduce the size of the widget DLL, it is
 *      compiled with the VAC subsystem libraries.
 *      In addition, instead of linking frequently
 *      used helpers against the DLL again, we import
 *      them from XFLDR.DLL, whose module handle is
 *      given to us in the INITMODULE export.
 *
 *      For each funtion that you need, add a global
 *      function pointer variable and an entry to
 *      the G_aImports array. These better match.
 */

// resolved function pointers from XFLDR.DLL
PCMNQUERYDEFAULTFONT pcmnQueryDefaultFont = NULL;
PCMNQUERYHELPLIBRARY pcmnQueryHelpLibrary = NULL;
PCMNQUERYNLSMODULEHANDLE pcmnQueryNLSModuleHandle = NULL;
PCMNSETCONTROLSFONT pcmnSetControlsFont = NULL;

PCTRDISPLAYHELP pctrDisplayHelp = NULL;
PCTRFREESETUPVALUE pctrFreeSetupValue = NULL;
PCTRPARSECOLORSTRING pctrParseColorString = NULL;
PCTRSCANSETUPSTRING pctrScanSetupString = NULL;
PCTRSETSETUPSTRING pctrSetSetupString = NULL;

PGPIHDRAW3DFRAME pgpihDraw3DFrame = NULL;

PLSTAPPENDITEM plstAppendItem = NULL;
PLSTCLEAR plstClear = NULL;
PLSTCOUNTITEMS plstCountItems = NULL;
PLSTINIT plstInit = NULL;
PLSTMALLOC plstMalloc = NULL;
PLSTQUERYFIRSTNODE plstQueryFirstNode = NULL;
PLSTSTRDUP plstStrDup = NULL;

PTMRSTARTTIMER ptmrStartTimer = NULL;
PTMRSTOPTIMER ptmrStopTimer = NULL;

PWINHCENTERWINDOW pwinhCenterWindow = NULL;
PWINHFREE pwinhFree = NULL;
PWINHQUERYPRESCOLOR pwinhQueryPresColor = NULL;
PWINHQUERYSWITCHLIST pwinhQuerySwitchList = NULL;
PWINHQUERYWINDOWFONT pwinhQueryWindowFont = NULL;
PWINHSETWINDOWFONT pwinhSetWindowFont = NULL;

PXSTRCAT pxstrcat = NULL;
PXSTRCLEAR pxstrClear = NULL;
PXSTRINIT pxstrInit = NULL;

RESOLVEFUNCTION G_aImports[] =
    {
        "cmnQueryDefaultFont", (PFN*)&pcmnQueryDefaultFont,
        "cmnQueryHelpLibrary", (PFN*)&pcmnQueryHelpLibrary,
        "cmnQueryNLSModuleHandle", (PFN*)&pcmnQueryNLSModuleHandle,
        "cmnSetControlsFont", (PFN*)&pcmnSetControlsFont,
        "ctrDisplayHelp", (PFN*)&pctrDisplayHelp,
        "ctrFreeSetupValue", (PFN*)&pctrFreeSetupValue,
        "ctrParseColorString", (PFN*)&pctrParseColorString,
        "ctrScanSetupString", (PFN*)&pctrScanSetupString,
        "ctrSetSetupString", (PFN*)&pctrSetSetupString,
        "gpihDraw3DFrame", (PFN*)&pgpihDraw3DFrame,
        "lstAppendItem", (PFN*)&plstAppendItem,
        "lstClear", (PFN*)&plstClear,
        "lstCountItems", (PFN*)&plstCountItems,
        "lstInit", (PFN*)&plstInit,
        "lstMalloc", (PFN*)&plstMalloc,
        "lstQueryFirstNode", (PFN*)&plstQueryFirstNode,
        "lstStrDup", (PFN*)&plstStrDup,
        "tmrStartTimer", (PFN*)&ptmrStartTimer,
        "tmrStopTimer", (PFN*)&ptmrStopTimer,
        "winhCenterWindow", (PFN*)&pwinhCenterWindow,
        "winhFree", (PFN*)&pwinhFree,
        "winhQueryPresColor", (PFN*)&pwinhQueryPresColor,
        "winhQuerySwitchList", (PFN*)&pwinhQuerySwitchList,
        "winhQueryWindowFont", (PFN*)&pwinhQueryWindowFont,
        "winhSetWindowFont", (PFN*)&pwinhSetWindowFont,
        "xstrcat", (PFN*)&pxstrcat,
        "xstrClear", (PFN*)&pxstrClear,
        "xstrInit", (PFN*)&pxstrInit
    };

/* ******************************************************************
 *
 *   Private widget instance data
 *
 ********************************************************************/

#define WLF_SHOWBUTTON      0x0001
#define WLF_LASTBUTTON      0x0002
#define WLF_JUMPABLE        0x0004
#define WLF_VISIBLE         0x0008

/*
 *@@ WINLISTSETUP:
 *      instance data to which setup strings correspond.
 *      This is also a member of WINLISTWIDGETDATA.
 *
 *      Putting these settings into a separate structure
 *      is no requirement, but comes in handy if you
 *      want to use the same setup string routines on
 *      both the open widget window and a settings dialog.
 */

typedef struct _WINLISTSETUP
{
    LONG        lcolBackground,         // background color
                lcolForeground;         // foreground color (for text)

    PSZ         pszFont;
            // if != NULL, non-default font (in "8.Helv" format);
            // this has been allocated using local malloc()!

    LINKLIST    llFilters;
            // linked list of window filters; this contains
            // plain swtitle PSZs allocated with lstStrDup

} WINLISTSETUP, *PWINLISTSETUP;

/*
 *@@ WINLISTWIDGETDATA:
 *      more window data for the "Winlist" widget.
 *
 *      An instance of this is created on WM_CREATE
 *      fnwpWinlistWidget and stored in XCENTERWIDGETVIEW.pUser.
 */

typedef struct _WINLISTWIDGETDATA
{
    PXCENTERWIDGETVIEW pViewData;
            // reverse ptr to general widget data ptr; we need
            // that all the time and don't want to pass it on
            // the stack with each function call

    WINLISTSETUP Setup;
            // widget settings that correspond to a setup string

    ULONG       ulTimerID;              // if != NULLHANDLE, update timer is running

    PSWBLOCK    pswBlock;
            // current window list; this is returned from winhQuerySwitchList,
            // which in turn calls WinQuerySwitchList.
            // The interesting part is pswblock->asentry[].swctl,
            // which contains the SWCNTRL structures:
            /*
              typedef struct _SWCNTRL {
                HWND         hwnd;                   window handle
                HWND         hwndIcon;               window-handle icon
                HPROGRAM     hprog;                  program handle
                PID          idProcess;              process identity
                ULONG        idSession;              session identity
                ULONG        uchVisibility;          visibility
                ULONG        fbJump;                 jump indicator
                CHAR         szSwtitle[MAXNAMEL+4];  switch-list control block title (null-terminated)
                ULONG        bProgType;              program type
              } SWCNTRL;
            */

            // Note: For each entry, we hack the fbJump field to
            // contain any combination of the following flags:
            // -- WLF_SHOWBUTTON: show button for this entry
            // -- WLF_LASTBUTTON: this is the last of the buttons
            // -- WLF_JUMPABLE: copy of original fbJump flag
            // -- WLF_VISIBLE: copy of original uchVisibility flag

            // We also hack the uchVisbility field to contain
            // the button index of the entry. We set this to -1
            // if no button is to be shown, just to make sure.

    ULONG       cShow;
            // count of items which have WLF_SHOWBUTTON set

    PSWCNTRL    pCtrlActive;
            // ptr into pswBlock for last active window or NULL if none

} WINLISTWIDGETDATA, *PWINLISTWIDGETDATA;

VOID ScanSwitchList(PWINLISTWIDGETDATA pWinlistData);

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
 *@@ WwgtClearSetup:
 *      cleans up the data in the specified setup
 *      structure, but does not free the structure
 *      itself.
 */

VOID WwgtClearSetup(PWINLISTSETUP pSetup)
{
    if (pSetup)
    {
        plstClear(&pSetup->llFilters);

        if (pSetup->pszFont)
        {
            free(pSetup->pszFont);
            pSetup->pszFont = NULL;
        }
    }
}

/*
 *@@ WwgtScanSetup:
 *      scans the given setup string and translates
 *      its data into the specified binary setup
 *      structure.
 *
 *      NOTE: It is assumed that pSetup is zeroed
 *      out. We do not clean up previous data here.
 */

VOID WwgtScanSetup(const char *pcszSetupString,
                   PWINLISTSETUP pSetup)
{
    PSZ p;

    // background color
    p = pctrScanSetupString(pcszSetupString,
                            "BGNDCOL");
    if (p)
    {
        pSetup->lcolBackground = pctrParseColorString(p);
        pctrFreeSetupValue(p);
    }
    else
        // default color:
        pSetup->lcolBackground = WinQuerySysColor(HWND_DESKTOP, SYSCLR_DIALOGBACKGROUND, 0);

    // text color:
    p = pctrScanSetupString(pcszSetupString,
                            "TEXTCOL");
    if (p)
    {
        pSetup->lcolForeground = pctrParseColorString(p);
        pctrFreeSetupValue(p);
    }
    else
        pSetup->lcolForeground = WinQuerySysColor(HWND_DESKTOP, SYSCLR_WINDOWSTATICTEXT, 0);

    // font:
    // we set the font presparam, which automatically
    // affects the cached presentation spaces
    p = pctrScanSetupString(pcszSetupString,
                            "FONT");
    if (p)
    {
        pSetup->pszFont = strdup(p);
        pctrFreeSetupValue(p);
    }
    // else: leave this field null

    // filters:
    plstInit(&pSetup->llFilters, TRUE);
    p = pctrScanSetupString(pcszSetupString,
                            "FILTERS");
    if (p)
    {
        PSZ pFilter = p;
        PSZ pSep = 0;
        ULONG ulSeparatorLength = strlen(G_pcszFilterSeparator);
        do
        {
            pSep = strstr(pFilter, G_pcszFilterSeparator);
            if (pSep)
                *pSep = '\0';
            // append copy filter; use lstStrDup to
            // allow auto-free of the list
            plstAppendItem(&pSetup->llFilters,
                           plstStrDup(pFilter));
            if (pSep)
                pFilter += (strlen(pFilter) + ulSeparatorLength);
        } while (pSep);
    }
}

/*
 *@@ WwgtSaveSetup:
 *      composes a new setup string.
 *      The caller must invoke xstrClear on the
 *      string after use.
 */

VOID WwgtSaveSetup(PXSTRING pstrSetup,       // out: setup string (is cleared first)
                   PWINLISTSETUP pSetup)
{
    CHAR    szTemp[100];
    PSZ     psz = 0;
    pxstrInit(pstrSetup, 100);

    sprintf(szTemp, "BGNDCOL=%06lX" SETUP_SEPARATOR,
            pSetup->lcolBackground);
    pxstrcat(pstrSetup, szTemp);

    sprintf(szTemp, "TEXTCOL=%06lX" SETUP_SEPARATOR,
            pSetup->lcolForeground);
    pxstrcat(pstrSetup, szTemp);

    if (pSetup->pszFont)
    {
        // non-default font:
        sprintf(szTemp, "FONT=%s" SETUP_SEPARATOR,
                pSetup->pszFont);
        pxstrcat(pstrSetup, szTemp);
    }

    if (plstCountItems(&pSetup->llFilters))
    {
        // we have items on the filters list:
        PLISTNODE   pNode = plstQueryFirstNode(&pSetup->llFilters);
        BOOL        fFirst = TRUE;
        // add keyword first
        pxstrcat(pstrSetup, "FILTERS=");

        while (pNode)
        {
            PSZ pszFilter = (PSZ)pNode->pItemData;
            if (!fFirst)
                // not first loop: add separator first
                pxstrcat(pstrSetup, G_pcszFilterSeparator);

            // append this filter
            pxstrcat(pstrSetup, pszFilter);

            pNode = pNode->pNext;
            fFirst = FALSE;
        }

        // add terminator
        pxstrcat(pstrSetup, SETUP_SEPARATOR);
    }
}

/* ******************************************************************
 *
 *   Widget settings dialog
 *
 ********************************************************************/

/*
 *@@ DumpSwitchList:
 *
 */

VOID DumpSwitchList(HWND hwnd,
                    PXCENTERGLOBALS pGlobals)
{
    PSWBLOCK pswBlock = pwinhQuerySwitchList(WinQueryAnchorBlock(hwnd));
                        // calls WinQuerySwitchList
    if (pswBlock)
    {
        HWND hwndCombo = WinWindowFromID(hwnd, ID_CRDI_FILTERS_NEWCOMBO);
        ULONG ul = 0;
        for (;
             ul < pswBlock->cswentry;
             ul++)
        {
            PSWCNTRL    pCtrlThis = &pswBlock->aswentry[ul].swctl;
            BOOL        fIsXCenterFrame = FALSE;

            if (pGlobals)
                if (pCtrlThis->hwnd == pGlobals->hwndFrame)
                    fIsXCenterFrame = TRUE;

            if (    (pCtrlThis->uchVisibility & SWL_VISIBLE)
                 && (!fIsXCenterFrame)
               )
            {
                // visible item:
                WinInsertLboxItem(hwndCombo,
                                  LIT_SORTASCENDING,
                                  pCtrlThis->szSwtitle);
            }
        }

        pwinhFree(pswBlock);
    }
}

/*
 *@@ Settings2Dlg:
 *
 */

VOID Settings2Dlg(HWND hwnd,
                  PWINLISTSETUP pSetup)
{
    HWND hwndFiltersLB = WinWindowFromID(hwnd, ID_CRDI_FILTERS_CURRENTLB);
    PLISTNODE pNode = plstQueryFirstNode(&pSetup->llFilters);

    winhDeleteAllItems(hwnd); // macro
    while (pNode)
    {
        PSZ pszFilterThis = (PSZ)pNode->pItemData;
        WinInsertLboxItem(hwndFiltersLB,
                          LIT_SORTASCENDING,
                          pszFilterThis);

        pNode = pNode->pNext;
    }
}

/*
 *@@ IsSwlistItemAddable:
 *      returns TRUE if the specified combo's
 *      entry field is not empty and not yet
 *      present in the filters list box.
 */

BOOL IsSwlistItemAddable(HWND hwndCombo,
                         PWINLISTSETUP pSetup)
{
    BOOL fEnableComboAdd = FALSE;
    if (WinQueryWindowTextLength(hwndCombo) != 0)
    {
        // combo has text:
        // check if this is already in the list box...
        CHAR szFilter[MAXNAMEL + 4];
        fEnableComboAdd = TRUE;
        if (WinQueryWindowText(hwndCombo,
                               sizeof(szFilter),
                               szFilter))
        {
            PLISTNODE pNode = plstQueryFirstNode(&pSetup->llFilters);
            while (pNode)
            {
                PSZ pszFilterThis = (PSZ)pNode->pItemData;
                if (strcmp(pszFilterThis, szFilter) == 0)
                {
                    fEnableComboAdd = FALSE;
                    break;
                }
                pNode = pNode->pNext;
            }
        }
    }

    return (fEnableComboAdd);
}

/*
 *@@ EnableItems:
 *
 */

VOID EnableItems(HWND hwnd,
                 PWINLISTSETUP pSetup)
{
    HWND hwndFiltersLB = WinWindowFromID(hwnd, ID_CRDI_FILTERS_CURRENTLB);
    HWND hwndCombo = WinWindowFromID(hwnd, ID_CRDI_FILTERS_NEWCOMBO);

    // "remove" from list box
    WinEnableControl(hwnd,
                     ID_CRDI_FILTERS_REMOVE,
                     (winhQueryLboxSelectedItem(hwndFiltersLB,
                                                LIT_FIRST) // macro
                            != LIT_NONE));

    // "add" from combo box
    WinEnableControl(hwnd,
                     ID_CRDI_FILTERS_ADD,
                     IsSwlistItemAddable(hwndCombo, pSetup));
}

/*
 *@@ Dlg2Settings:
 *
 */

VOID Dlg2Settings(HWND hwnd,
                  PWINLISTSETUP pSetup)
{
    HWND hwndFiltersLB = WinWindowFromID(hwnd, ID_CRDI_FILTERS_CURRENTLB);
    SHORT sItemCount = WinQueryLboxCount(hwndFiltersLB),
          s = 0;
    // clear previous list
    plstClear(&pSetup->llFilters);

    for (s = 0;
         s < sItemCount;
         s++)
    {
        CHAR szFilter[MAXNAMEL+4];  // same as in SWCNTRL.szSwtitle[];
        WinQueryLboxItemText(hwndFiltersLB,
                             s,
                             szFilter,
                             sizeof(szFilter));
        // append copy filter; use lstStrDup to
        // allow auto-free of the list
        plstAppendItem(&pSetup->llFilters,
                       plstStrDup(szFilter));
    }
}

/*
 *@@ fnwpSettingsDlg:
 *      dialog proc for the winlist settings dialog.
 */

MRESULT EXPENTRY fnwpSettingsDlg(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    switch (msg)
    {
        /*
         * WM_INITDLG:
         *      fill the control. We get the
         *      WIDGETSETTINGSDLGDATA in mp2.
         */

        case WM_INITDLG:
        {
            PWIDGETSETTINGSDLGDATA pData = (PWIDGETSETTINGSDLGDATA)mp2;
            PWINLISTSETUP pSetup = malloc(sizeof(WINLISTSETUP));
            WinSetWindowPtr(hwnd, QWL_USER, pData);
            if (pSetup)
            {
                memset(pSetup, 0, sizeof(*pSetup));
                // store this in WIDGETSETTINGSDLGDATA
                pData->pUser = pSetup;

                WwgtScanSetup(pData->pcszSetupString, pSetup);

                // limit entry field to max len of switch titles
                winhSetEntryFieldLimit(WinWindowFromID(hwnd,
                                                       ID_CRDI_FILTERS_NEWCOMBO),
                                       MAXNAMEL + 4 - 1); // null terminator!

                DumpSwitchList(hwnd, pData->pGlobals);
                Settings2Dlg(hwnd, pSetup);
                EnableItems(hwnd, pSetup);
            }
            mrc = WinDefDlgProc(hwnd, msg, mp1, mp2);
        break; }

        /*
         * WM_COMMAND:
         *
         */

        case WM_COMMAND:
        {
            PWIDGETSETTINGSDLGDATA pData = (PWIDGETSETTINGSDLGDATA)WinQueryWindowPtr(hwnd, QWL_USER);
            if (pData)
            {
                PWINLISTSETUP pSetup = (PWINLISTSETUP)pData->pUser;
                if (pSetup)
                {
                    USHORT usCmd = (USHORT)mp1;
                    HWND hwndFiltersLB = WinWindowFromID(hwnd, ID_CRDI_FILTERS_CURRENTLB);
                    HWND hwndCombo = WinWindowFromID(hwnd, ID_CRDI_FILTERS_NEWCOMBO);

                    switch (usCmd)
                    {
                        /*
                         * ID_CRDI_FILTERS_REMOVE:
                         *      remove selected filter
                         */

                        case ID_CRDI_FILTERS_REMOVE:
                        {
                            SHORT sSel = winhQueryLboxSelectedItem(hwndFiltersLB,
                                                                   LIT_FIRST); // macro
                            if (sSel != LIT_NONE)
                            {
                                WinDeleteLboxItem(hwndFiltersLB, sSel);
                                Dlg2Settings(hwnd, pSetup);
                                EnableItems(hwnd, pSetup);

                                // reset focus to listbox
                                WinSetFocus(HWND_DESKTOP, hwndFiltersLB);
                            }
                        break; }

                        /*
                         * ID_CRDI_FILTERS_ADD:
                         *      add specified filter
                         */

                        case ID_CRDI_FILTERS_ADD:
                        {
                            if (IsSwlistItemAddable(hwndCombo, pSetup))
                            {
                                CHAR szFilter[MAXNAMEL + 4];
                                // we have an entry:
                                if (WinQueryWindowText(hwndCombo,
                                                       sizeof(szFilter),
                                                       szFilter))
                                {
                                    SHORT sSel;
                                    WinInsertLboxItem(hwndFiltersLB,
                                                      LIT_SORTASCENDING,
                                                      szFilter);
                                    winhSetLboxSelectedItem(hwndFiltersLB,
                                                            LIT_NONE,
                                                            0);
                                    // clean up combo
                                    WinSetWindowText(hwndCombo, NULL);
                                    winhSetLboxSelectedItem(hwndCombo,
                                                            LIT_NONE,
                                                            0);

                                    // get dlg items
                                    Dlg2Settings(hwnd, pSetup);
                                    EnableItems(hwnd, pSetup);

                                    // reset focus to combo
                                    WinSetFocus(HWND_DESKTOP, hwndCombo);
                                }
                            }
                        break; }

                        /*
                         * DID_OK:
                         *      OK button -> recompose settings
                         *      and get outta here.
                         */

                        case DID_OK:
                        {
                            XSTRING strSetup;
                            WwgtSaveSetup(&strSetup,
                                          pSetup);
                            pctrSetSetupString(pData->hSettings,
                                               strSetup.psz);
                            pxstrClear(&strSetup);
                            WinDismissDlg(hwnd, DID_OK);
                        break; }

                        /*
                         * DID_CANCEL:
                         *      cancel button...
                         */

                        case DID_CANCEL:
                            WinDismissDlg(hwnd, DID_CANCEL);
                        break;

                        case DID_HELP:
                            pctrDisplayHelp(pData->pGlobals,
                                            pcmnQueryHelpLibrary(),
                                            ID_XSD_WIDGET_WINLIST_SETTINGS);
                        break;
                    } // end switch (usCmd)
                } // end if (pSetup)
            } // end if (pData)
        break; } // WM_COMMAND

        /*
         * WM_CONTROL:
         *
         */

        case WM_CONTROL:
        {
            PWIDGETSETTINGSDLGDATA pData = (PWIDGETSETTINGSDLGDATA)WinQueryWindowPtr(hwnd, QWL_USER);
            if (pData)
            {
                PWINLISTSETUP pSetup = (PWINLISTSETUP)pData->pUser;
                if (pSetup)
                {
                    USHORT usItemID = SHORT1FROMMP(mp1),
                           usNotifyCode = SHORT2FROMMP(mp1);
                    switch (usItemID)
                    {
                        // current filters listbox:
                        case ID_CRDI_FILTERS_CURRENTLB:
                            if (usNotifyCode == LN_SELECT)
                                // change "remove" button state
                                EnableItems(hwnd, pSetup);
                        break;

                        // switchlist combobox:
                        case ID_CRDI_FILTERS_NEWCOMBO:
                            if (usNotifyCode == CBN_EFCHANGE)
                                // entry field has changed:
                                // change "add" button state
                                EnableItems(hwnd, pSetup);
                            else if (usNotifyCode == CBN_ENTER)
                                // double-click on list item:
                                // simulate "add" button
                                WinPostMsg(hwnd,
                                           WM_COMMAND,
                                           (MPARAM)ID_CRDI_FILTERS_ADD,
                                           MPFROM2SHORT(CMDSRC_OTHER, TRUE));
                        break;
                    }
                }
            }
        break; }

        /*
         * WM_DESTROY:
         *
         */

        case WM_DESTROY:
        {
            PWIDGETSETTINGSDLGDATA pData = (PWIDGETSETTINGSDLGDATA)WinQueryWindowPtr(hwnd, QWL_USER);
            if (pData)
            {
                PWINLISTSETUP pSetup = (PWINLISTSETUP)pData->pUser;
                if (pSetup)
                {
                    WwgtClearSetup(pSetup);
                    free(pSetup);
                } // end if (pSetup)
            } // end if (pData)

            mrc = WinDefDlgProc(hwnd, msg, mp1, mp2);
        break; }

        default:
            mrc = WinDefDlgProc(hwnd, msg, mp1, mp2);
    }

    return (mrc);
}

/* ******************************************************************
 *
 *   Callbacks stored in XCENTERWIDGETVIEW
 *
 ********************************************************************/

/*
 *@@ WwgtSetupStringChanged:
 *      this gets called from ctrSetSetupString if
 *      the setup string for an open widget has changed.
 *
 *      This procedure's address is stored in
 *      XCENTERWIDGETVIEW so that the XCenter knows that
 *      we can do this.
 */

VOID EXPENTRY WwgtSetupStringChanged(PXCENTERWIDGETVIEW pViewData,
                                     const char *pcszNewSetupString)
{
    PWINLISTWIDGETDATA pWinlistData = (PWINLISTWIDGETDATA)pViewData->pUser;
    if (pWinlistData)
    {
        // reinitialize the setup data
        WwgtClearSetup(&pWinlistData->Setup);
        WwgtScanSetup(pcszNewSetupString, &pWinlistData->Setup);

        // rescan switch list, because filters might have changed
        ScanSwitchList(pWinlistData);
        WinInvalidateRect(pViewData->hwndWidget, NULL, FALSE);
    }
}

/*
 *@@ WwgtShowSettingsDlg:
 *      this displays the winlist widget's settings
 *      dialog.
 *
 *      This procedure's address is stored in
 *      XCENTERWIDGETVIEW so that the XCenter knows that
 *      we can do this.
 *
 *      When calling this function, the XCenter expects
 *      it to display a modal dialog and not return
 *      until the dialog is destroyed. While the dialog
 *      is displaying, it would be nice to have the
 *      widget dynamically update itself.
 */

VOID EXPENTRY WwgtShowSettingsDlg(PWIDGETSETTINGSDLGDATA pData)
{
    HWND hwnd = WinLoadDlg(HWND_DESKTOP,         // parent
                           pData->hwndOwner,
                           fnwpSettingsDlg,
                           pcmnQueryNLSModuleHandle(FALSE),
                           ID_CRD_WINLISTWGT_SETTINGS,
                           // pass original setup string with WM_INITDLG
                           (PVOID)pData);
    if (hwnd)
    {
        pcmnSetControlsFont(hwnd,
                            1,
                            10000);

        pwinhCenterWindow(hwnd);         // invisibly

        // go!!
        WinProcessDlg(hwnd);

        WinDestroyWindow(hwnd);
    }
}

/* ******************************************************************
 *
 *   PM window class implementation
 *
 ********************************************************************/

/*
 *      This code has the actual PM window class.
 *
 */

/*
 *@@ GetPaintableRect:
 *      retrieves the widget's client rectangle
 *      and shrinks it by the thin outer widget
 *      border. The rectangle put into prcl can
 *      be used for painting the buttons.
 */

VOID GetPaintableRect(HWND hwnd,
                      PRECTL prcl)
{
    WinQueryWindowRect(hwnd,
                       prcl);
    prcl->xLeft += WIDGET_BORDER;
    prcl->xRight -= WIDGET_BORDER;
    prcl->yBottom += WIDGET_BORDER;
    prcl->yTop -= WIDGET_BORDER;

    // we won't use WinInflateRect... what the hell
    // does this API need an anchor block for to
    // do a couple of additions?!?
    /*
    WinInflateRect(pViewData->hab,
                   prcl,
                   -WIDGET_BORDER,
                   -WIDGET_BORDER); */
}

/*
 *@@ IsCtrlFiltered:
 *      returns TRUE if the control is filtered
 *      and should therefore not be displayed
 *      as a button in the window list.
 *
 *      This is the one place which is used by
 *      all other parts to determine whether the
 *      specified switch control is filtered.
 */

BOOL IsCtrlFiltered(PWINLISTWIDGETDATA pWinlistData,
                    PSWCNTRL pCtrl)
{
    BOOL brc = FALSE;

    // rule out invisible tasklist entries
    if ((pCtrl->uchVisibility & SWL_VISIBLE) == 0)
        brc = TRUE;
    // rule out the XCenter frame
    else if (pCtrl->hwnd == pWinlistData->pViewData->pGlobals->hwndFrame)
        brc = TRUE;
    else
    {
        // go thru user-defined filters
        PLISTNODE pNode = plstQueryFirstNode(&pWinlistData->Setup.llFilters);
        while (pNode)
        {
            PSZ pszFilterThis = (PSZ)pNode->pItemData;
            if (strcmp(pCtrl->szSwtitle, pszFilterThis) == 0)
            {
                // filtered:
                brc = TRUE;
                break;
            }
            pNode = pNode->pNext;
        }
    }
    return (brc);
}

/*
 *@@ ScanSwitchList:
 *      scans or rescans the PM task list (switch list)
 *      and initializes all data in WINLISTWIDGETDATA
 *      accordingly.
 *
 *      This does not repaint the widget.
 */

VOID ScanSwitchList(PWINLISTWIDGETDATA pWinlistData)
{
    ULONG       ul;
    // we must mark the last valid item to paint
    // so that the paint routines can make it a bit wider
    PSWCNTRL    pCtrlLast2Paint = NULL;

    if (pWinlistData->pswBlock)
    {
        // not first call: free previous
        pwinhFree(pWinlistData->pswBlock);
        pWinlistData->pCtrlActive = NULL;
    }

    pWinlistData->pswBlock = pwinhQuerySwitchList(pWinlistData->pViewData->habWidget);
                        // calls WinQuerySwitchList

    pWinlistData->cShow = 0;

    if (pWinlistData->pswBlock)
    {
        // now go filter and get icons
        for (ul = 0;
             ul < pWinlistData->pswBlock->cswentry;
             ul++)
        {
            PSWCNTRL pCtrlThis = &pWinlistData->pswBlock->aswentry[ul].swctl;

            // now set up the fbJump field, which we use
            // for our own display flags... ugly, but this
            // saves us from reallocating a second array
            // for our own data
            if (pCtrlThis->fbJump & SWL_JUMPABLE)
                pCtrlThis->fbJump = WLF_JUMPABLE;
            else
                pCtrlThis->fbJump = 0;

            // apply filter
            if (!IsCtrlFiltered(pWinlistData, pCtrlThis))
            {
                // item will be painted:
                // mark it so (hack field)
                pCtrlThis->fbJump |= WLF_SHOWBUTTON;

                // store button index (hack field)
                // and raise it for next one (will
                // also be count in the end)
                pCtrlThis->uchVisibility = (pWinlistData->cShow)++;
                        // this is a ULONG, so we're safe... who came
                        // up with this naming, IBM?

                // remember this item; we must mark
                // the last valid button for painting!
                pCtrlLast2Paint = pCtrlThis;
                // get its icon
                pCtrlThis->hwndIcon = (HWND)WinSendMsg(pCtrlThis->hwnd,
                                                       WM_QUERYICON, 0, 0);
                    // can be NULLHANDLE
            }
            else
                // not to be shown:
                pCtrlThis->uchVisibility = -1;
        }
    }

    if (pCtrlLast2Paint)
        // we had at least one button:
        // mark the last one
        pCtrlLast2Paint->fbJump |= WLF_LASTBUTTON;
}

/*
 *@@ DrawOneCtrl:
 *      paints one window button.
 *
 *      Gets called from DrawAllCtrls and from
 *      RedrawActiveChanged.
 *
 *      Preconditions: The switch list should have
 *      been scanned (ScanSwitchList).
 *
 *      This changes pWinlistData->pCtrlActive if
 *      pCtrlThis points to hwndActive.
 */

VOID DrawOneCtrl(PWINLISTWIDGETDATA pWinlistData,
                 HPS hps,
                 PRECTL prclSubclient,     // in: paint area (exclusive)
                 PSWCNTRL pCtrlThis,       // in: switch list entry to paint
                 HWND hwndActive)          // in: currently active window
{
    // colors for borders
    LONG    lLeft,
            lRight;
    LONG    xText = 0;
    RECTL   rclSub;

    const XCENTERGLOBALS *pGlobals = pWinlistData->pViewData->pGlobals;

    // avoid division by zero
    if (pWinlistData->cShow)
    {
        // max paint space:
        ULONG   ulCX = (prclSubclient->xRight - prclSubclient->xLeft);
        // calc width for this button...
        // use standard with, except for last button
        ULONG   cxRegular = ulCX / pWinlistData->cShow;
        ULONG   cxThis = cxRegular;

        if (pCtrlThis->fbJump & WLF_LASTBUTTON)
            // last button: add leftover space
            cxThis += (ulCX % pWinlistData->cShow);

        if ((hwndActive) && (pCtrlThis->hwnd == hwndActive))
        {
            // active window: paint rect lowered
            lLeft = pGlobals->lcol3DDark;
            lRight = pGlobals->lcol3DLight;
            // mark this so we can quickly undo
            // the change without repainting everything
            pWinlistData->pCtrlActive = pCtrlThis;
        }
        else
        {
            lLeft = pGlobals->lcol3DLight;
            lRight = pGlobals->lcol3DDark;
        }

        // draw frame
        rclSub.yBottom = prclSubclient->yBottom;
        rclSub.yTop = prclSubclient->yTop;
        // calculate X coordinate: uchVisibility has
        // the button index as calculated by ScanSwitchList,
        // so we can multiply
        rclSub.xLeft = prclSubclient->xLeft + (pCtrlThis->uchVisibility * cxRegular);
        rclSub.xRight = rclSub.xLeft + cxThis;

        // draw button frame
        pgpihDraw3DFrame(hps,
                         &rclSub,
                         BUTTON_BORDER,
                         lLeft,
                         lRight);

        // draw button middle
        WinInflateRect(pWinlistData->pViewData->habWidget,
                       &rclSub,
                       -BUTTON_BORDER,
                       -BUTTON_BORDER);
        WinFillRect(hps,
                    &rclSub,
                    pWinlistData->Setup.lcolBackground);

        if (pCtrlThis->hwndIcon)
        {
            WinDrawPointer(hps,
                           rclSub.xLeft + 1,
                           rclSub.yBottom + 1,
                           pCtrlThis->hwndIcon,
                           DP_MINI);
            rclSub.xLeft += pGlobals->cxMiniIcon;
        }

        // add another pixel for the text
        (rclSub.xLeft)++;
        (rclSub.xRight)--;

        // draw switch list title
        WinDrawText(hps,
                    strlen(pCtrlThis->szSwtitle),
                    pCtrlThis->szSwtitle,
                    &rclSub,
                    pWinlistData->Setup.lcolForeground,
                    0,
                    DT_LEFT | DT_VCENTER);
    }
}

/*
 *@@ DrawAllCtrls:
 *      redraws all visible switch list controls by invoking
 *      DrawOneCtrl on them. Gets called from WwgtPaint.
 */

VOID DrawAllCtrls(PWINLISTWIDGETDATA pWinlistData,
                  HPS hps,
                  PRECTL prcl)       // in: max available space (exclusive)
{
    ULONG   ul = 0;

    if (!pWinlistData->cShow)      // avoid division by zero
    {
        // draw no buttons
        WinFillRect(hps,
                    prcl,
                    pWinlistData->Setup.lcolBackground);
    }
    else
    {
        // count of entries in switch list:
        ULONG   cEntries = pWinlistData->pswBlock->cswentry;

        HWND    hwndActive = WinQueryActiveWindow(HWND_DESKTOP);

        for (;
             ul < cEntries;
             ul++)
        {
            PSWCNTRL pCtrlThis = &pWinlistData->pswBlock->aswentry[ul].swctl;
            // was this item marked as paintable?
            if (pCtrlThis->fbJump & WLF_SHOWBUTTON)
            {
                // yes:
                DrawOneCtrl(pWinlistData,
                            hps,
                            prcl,
                            pCtrlThis,
                            hwndActive);
            }
        } // end for ul
    } // end if (cWins)
}

/*
 *@@ RedrawActiveChanged:
 *      gets called to repaint the buttons only which
 *      are affected by an active window change. We
 *      don't want to redraw the entire bar all the time.
 */

VOID RedrawActiveChanged(PWINLISTWIDGETDATA pWinlistData,
                         HWND hwndActive)
{
    HWND hwndWidget = pWinlistData->pViewData->hwndWidget;
    HPS hps = WinGetPS(hwndWidget);
    if (hps)
    {
        RECTL rclSubclient;
        gpihSwitchToRGB(hps);
        GetPaintableRect(hwndWidget, &rclSubclient);

        if (pWinlistData->pCtrlActive)
        {
            // unpaint old active
            DrawOneCtrl(pWinlistData,
                        hps,
                        &rclSubclient,
                        pWinlistData->pCtrlActive,
                        hwndActive);
        }

        if (hwndActive)
        {
            ULONG   cEntries = pWinlistData->pswBlock->cswentry;
            ULONG   ul = 0;
            for (;
                 ul < cEntries;
                 ul++)
            {
                PSWCNTRL pCtrlThis = &pWinlistData->pswBlock->aswentry[ul].swctl;
                // was this item marked as paintable?
                if (    (pCtrlThis->hwnd == hwndActive)
                     && (pCtrlThis->fbJump & WLF_SHOWBUTTON)
                   )
                {
                    DrawOneCtrl(pWinlistData,
                                hps,
                                &rclSubclient,
                                pCtrlThis,
                                hwndActive);
                    break;
                }
            }
        }

        WinReleasePS(hps);
    } // end if (hps)
}

/*
 *@@ FindCtrlFromPoint:
 *      returns the SWCNTRL from the global list
 *      which matches the specified point, which
 *      must be in the widget's window coordinates.
 *
 *      Returns NULL if not found.
 */

PSWCNTRL FindCtrlFromPoint(PWINLISTWIDGETDATA pWinlistData,
                           PPOINTL pptl,
                           PRECTL prclSubclient)
{
    PSWCNTRL pCtrl = NULL;
    // avoid division by zero
    if (pWinlistData->cShow)
    {
        if (WinPtInRect(pWinlistData->pViewData->habWidget,
                        prclSubclient,
                        pptl))
        {
            ULONG   ulCX = (prclSubclient->xRight - prclSubclient->xLeft);
            // calc width for this button...
            // use standard with, except for last button
            ULONG   cxRegular = ulCX / pWinlistData->cShow;
            // avoid division by zero
            if (cxRegular)
            {
                ULONG   ulButtonIndex = pptl->x / cxRegular;
                // go find button with that index
                ULONG   cEntries = pWinlistData->pswBlock->cswentry;
                ULONG   ul = 0;
                for (;
                     ul < cEntries;
                     ul++)
                {
                    PSWCNTRL pCtrlThis = &pWinlistData->pswBlock->aswentry[ul].swctl;
                    if (    (pCtrlThis->uchVisibility /* index */ == ulButtonIndex)
                         && (pCtrlThis->fbJump & WLF_SHOWBUTTON)
                       )
                    {
                        pCtrl = pCtrlThis;
                        break;
                    }
                }
            }
        }
    }

    return (pCtrl);
}

/*
 *@@ WwgtCreate:
 *      implementation for WM_CREATE.
 */

MRESULT WwgtCreate(HWND hwnd,
                   PXCENTERWIDGETVIEW pViewData)
{
    MRESULT mrc = 0;
    PSZ p;
    PWINLISTWIDGETDATA pWinlistData = malloc(sizeof(WINLISTWIDGETDATA));
    memset(pWinlistData, 0, sizeof(WINLISTWIDGETDATA));
    // link the two together
    pViewData->pUser = pWinlistData;
    pWinlistData->pViewData = pViewData;

    // tell the XCenter about our desired width:
    // use all remaining
    pViewData->cxWanted = -1;
    // the height: system mini icon size
    pViewData->cyWanted =       2 * WIDGET_BORDER
                              + 2 * BUTTON_BORDER
                              + 2
                              + pViewData->pGlobals->cxMiniIcon;

    // initialize binary setup structure from setup string
    WwgtScanSetup(pViewData->pcszSetupString,
                  &pWinlistData->Setup);

    // set window font (this affects all the cached presentation
    // spaces we use)
    pwinhSetWindowFont(hwnd,
                       (pWinlistData->Setup.pszFont)
                        ? pWinlistData->Setup.pszFont
                        // default font: use the same as in the rest of XWorkplace:
                        : pcmnQueryDefaultFont());

    // enable setup string notifications
    pViewData->pSetupStringChanged = WwgtSetupStringChanged;

    // enable context menu help
    pViewData->pcszHelpLibrary = pcmnQueryHelpLibrary();
    pViewData->ulHelpPanelID = ID_XSH_WIDGET_WINLIST_MAIN;

    // initialize switch list data
    ScanSwitchList(pWinlistData);

    // start update timer
    pWinlistData->ulTimerID = ptmrStartTimer(hwnd,
                                             1,
                                             300);

    return (mrc);
}

/*
 *@@ WwgtPaint:
 *      implementation for WM_PAINT.
 */

VOID WwgtPaint(HWND hwnd,
               PXCENTERWIDGETVIEW pViewData)
{
    HPS hps = WinBeginPaint(hwnd, NULLHANDLE, NULL);
    if (hps)
    {
        PWINLISTWIDGETDATA pWinlistData = (PWINLISTWIDGETDATA)pViewData->pUser;
        if (pWinlistData)
        {
            RECTL       rclWin;

            // now paint frame
            WinQueryWindowRect(hwnd, &rclWin);
            gpihSwitchToRGB(hps);

            pgpihDraw3DFrame(hps,
                             &rclWin,
                             WIDGET_BORDER,
                             pViewData->pGlobals->lcol3DDark,
                             pViewData->pGlobals->lcol3DLight);

            // now paint buttons in the middle
            WinInflateRect(pViewData->habWidget,
                           &rclWin,
                           -WIDGET_BORDER,
                           -WIDGET_BORDER);
            DrawAllCtrls(pWinlistData,
                         hps,
                         &rclWin);
        }
        WinEndPaint(hps);
    }
}

/*
 *@@ WwgtTimer:
 *      implementation for WM_TIMER. This checks whether
 *      the switch list has changed. If so, the entire
 *      widget is invalidated. If not, we check if maybe
 *      only the active window has changed. In that case,
 *      we can call RedrawActiveChanged.
 */

VOID WwgtTimer(HWND hwnd,
               PXCENTERWIDGETVIEW pViewData)
{
    PWINLISTWIDGETDATA pWinlistData = (PWINLISTWIDGETDATA)pViewData->pUser;
    if (pWinlistData)
    {
        // check current switch list count
        ULONG cItems = WinQuerySwitchList(pViewData->habWidget,
                                          NULL,
                                          0);
        if (cItems != pWinlistData->pswBlock->cswentry)
        {
            HPS hps;
            // switch list item count has changed:
            // yo, rescan the entire thing!
            ScanSwitchList(pWinlistData);
            // redraw!
            hps = WinGetPS(hwnd);
            if (hps)
            {
                RECTL rclSubclient;
                GetPaintableRect(hwnd, &rclSubclient);
                gpihSwitchToRGB(hps);
                DrawAllCtrls(pWinlistData,
                             hps,
                             &rclSubclient);
                WinReleasePS(hps);
            }
        }
        else
        {
            // switch list item count has not changed:
            // check if maybe active window has changed
            HWND    hwndActive = WinQueryActiveWindow(HWND_DESKTOP);
            BOOL    fUpdateActive = FALSE;
            if (pWinlistData->pCtrlActive)
            {
                if (pWinlistData->pCtrlActive->fbJump & WLF_SHOWBUTTON)
                    if (pWinlistData->pCtrlActive->hwnd != hwndActive)
                        // active window changed:
                        fUpdateActive = TRUE;
            }
            else
                // we didn't have an active window previously:
                if (hwndActive)
                    // but we have one now:
                    fUpdateActive = TRUE;

            if (fUpdateActive)
                RedrawActiveChanged(pWinlistData,
                                    hwndActive);
        } // end else if (cItems != pWinlistData->pswBlock->cswentry)
    } // end if (pWinlistData)
}

/*
 *@@ WwgtButton1Click:
 *      implementation for WM_BUTTON1CLICK.
 */

VOID WwgtButton1Click(HWND hwnd,
                      MPARAM mp1,
                      PXCENTERWIDGETVIEW pViewData)
{
    PWINLISTWIDGETDATA pWinlistData = (PWINLISTWIDGETDATA)pViewData->pUser;
    if (pWinlistData)
    {
        POINTL      ptlClick;
        PSWCNTRL    pCtrlClicked = 0;
        RECTL       rclSubclient;

        ptlClick.x = SHORT1FROMMP(mp1);
        ptlClick.y = SHORT2FROMMP(mp1);
        GetPaintableRect(hwnd, &rclSubclient);
        pCtrlClicked = FindCtrlFromPoint(pWinlistData,
                                         &ptlClick,
                                         &rclSubclient);
        if (pCtrlClicked)
        {
            // check the window's state...
            SWP swp;
            BOOL fRedrawActive = FALSE;
            if (WinQueryWindowPos(pCtrlClicked->hwnd, &swp))
            {
                if (swp.fl & SWP_MINIMIZE)
                {
                    // window is minimized:
                    // restore and activate
                    if (WinSetWindowPos(pCtrlClicked->hwnd,
                                        HWND_TOP,
                                        0, 0, 0, 0,
                                        SWP_RESTORE | SWP_ACTIVATE | SWP_ZORDER))
                    /* if (WinPostMsg(pCtrlClicked->hwnd,
                                   WM_SYSCOMMAND,
                                   (MPARAM)SC_RESTORE,
                                   MPFROM2SHORT(CMDSRC_OTHER, TRUE))) */
                        fRedrawActive = TRUE;
                }
                else
                {
                    // not minimized:
                    // see if it's active
                    HWND hwndActive = WinQueryActiveWindow(HWND_DESKTOP);
                    if (hwndActive == pCtrlClicked->hwnd)
                    {
                        // window is active:
                        // try minimize...
                        // we better not use WinSetWindowPos directly,
                        // because this solidly messes up some windows;
                        // SC_MINIMIZE will only minimize the window
                        // if the window actually supports that (usually
                        // the PM frame class ignores this if the window
                        // doesn't have the minimize button).
                        if (WinPostMsg(pCtrlClicked->hwnd,
                                       WM_SYSCOMMAND,
                                       (MPARAM)SC_MINIMIZE,
                                       MPFROM2SHORT(CMDSRC_OTHER, TRUE)))
                            fRedrawActive = TRUE;
                    }
                    else
                        // not minimized, not active:
                        if (WinSetActiveWindow(HWND_DESKTOP, pCtrlClicked->hwnd))
                            fRedrawActive = TRUE;
                }
            }

            if (fRedrawActive)
                RedrawActiveChanged(pWinlistData,
                                    pCtrlClicked->hwnd);
        }
    }
}

/*
 *@@ WwgtPresParamChanged:
 *      implementation for WM_PRESPARAMCHANGED.
 *
 */

VOID WwgtPresParamChanged(HWND hwnd,
                          ULONG ulAttrChanged,
                          PXCENTERWIDGETVIEW pViewData)
{
    PWINLISTWIDGETDATA pWinlistData = (PWINLISTWIDGETDATA)pViewData->pUser;
    if (pWinlistData)
    {
        BOOL fInvalidate = TRUE;
        switch (ulAttrChanged)
        {
            case 0:     // layout palette thing dropped
            case PP_BACKGROUNDCOLOR:
            case PP_FOREGROUNDCOLOR:
                pWinlistData->Setup.lcolBackground
                    = pwinhQueryPresColor(hwnd,
                                          PP_BACKGROUNDCOLOR,
                                          FALSE,
                                          SYSCLR_DIALOGBACKGROUND);
                pWinlistData->Setup.lcolForeground
                    = pwinhQueryPresColor(hwnd,
                                          PP_FOREGROUNDCOLOR,
                                          FALSE,
                                          SYSCLR_WINDOWSTATICTEXT);
            break;

            case PP_FONTNAMESIZE:
            {
                PSZ pszFont = 0;
                if (pWinlistData->Setup.pszFont)
                {
                    free(pWinlistData->Setup.pszFont);
                    pWinlistData->Setup.pszFont = NULL;
                }

                pszFont = pwinhQueryWindowFont(hwnd);
                if (pszFont)
                {
                    // we must use local malloc() for the font
                    pWinlistData->Setup.pszFont = strdup(pszFont);
                    pwinhFree(pszFont);
                }
            break; }

            default:
                fInvalidate = FALSE;
        }

        if (fInvalidate)
        {
            XSTRING strSetup;
            WinInvalidateRect(hwnd, NULL, FALSE);

            WwgtSaveSetup(&strSetup,
                          &pWinlistData->Setup);
            if (strSetup.ulLength)
                WinSendMsg(pWinlistData->pViewData->pGlobals->hwndClient,
                           XCM_SAVESETUP,
                           (MPARAM)hwnd,
                           (MPARAM)strSetup.psz);
            pxstrClear(&strSetup);
        }
    } // end if (pWinlistData)
}

/*
 *@@ WwgtDestroy:
 *      implementation for WM_DESTROY.
 */

VOID WwgtDestroy(HWND hwnd,
                 PXCENTERWIDGETVIEW pViewData)
{
    PWINLISTWIDGETDATA pWinlistData = (PWINLISTWIDGETDATA)pViewData->pUser;
    if (pWinlistData)
    {
        WwgtClearSetup(&pWinlistData->Setup);

        if (pWinlistData->ulTimerID)
            ptmrStopTimer(hwnd,
                          pWinlistData->ulTimerID);
        if (pWinlistData->pswBlock)
            pwinhFree(pWinlistData->pswBlock);
        free(pWinlistData);
                // pViewData is cleaned up by DestroyWidgets
    }
}

/*
 *@@ fnwpWinlistWidget:
 *      window procedure for the winlist widget class.
 *
 *      There are a few rules which widget window procs
 *      must follow. See ctrDefWidgetProc in src\shared\center.c
 *      for details.
 *
 *      Other than that, this is a regular window procedure
 *      which follows the basic rules for a PM window class.
 */

MRESULT EXPENTRY fnwpWinlistWidget(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;
    PXCENTERWIDGETVIEW pViewData = (PXCENTERWIDGETVIEW)WinQueryWindowPtr(hwnd, QWL_USER);
                    // this ptr is valid after WM_CREATE

    switch (msg)
    {
        /*
         * WM_CREATE:
         *      as with all widgets, we receive a pointer to the
         *      XCENTERWIDGETVIEW in mp1, which was created for us.
         *
         *      The first thing the widget MUST do on WM_CREATE
         *      is to store the XCENTERWIDGETVIEW pointer (from mp1)
         *      in the QWL_USER window word by calling:
         *
         *          WinSetWindowPtr(hwnd, QWL_USER, mp1);
         *
         *      We use XCENTERWIDGETVIEW.pUser for allocating
         *      WINLISTWIDGETDATA for our own stuff.
         *
         *      Each widget must write its desired width into
         *      XCENTERWIDGETVIEW.cx and cy.
         */

        case WM_CREATE:
            WinSetWindowPtr(hwnd, QWL_USER, mp1);
            pViewData = (PXCENTERWIDGETVIEW)mp1;
            if ((pViewData) && (pViewData->pfnwpDefWidgetProc))
                mrc = WwgtCreate(hwnd, pViewData);
            else
                // stop window creation!!
                mrc = (MPARAM)TRUE;
        break;

        /*
         * WM_PAINT:
         *
         */

        case WM_PAINT:
            WwgtPaint(hwnd, pViewData);
        break;

        /*
         * WM_TIMER:
         *      update timer --> check for updates.
         */

        case WM_TIMER:
            if ((USHORT)mp1 == 1)
                WwgtTimer(hwnd, pViewData);
            else
                mrc = pViewData->pfnwpDefWidgetProc(hwnd, msg, mp1, mp2);
        break;

        /*
         * WM_BUTTON1CLICK:
         *
         */

        case WM_BUTTON1CLICK:
            WwgtButton1Click(hwnd, mp1, pViewData);
            mrc = (MPARAM)TRUE;
        break;

        /*
         * WM_PRESPARAMCHANGED:
         *
         */

        case WM_PRESPARAMCHANGED:
            if (pViewData)
                // this gets sent before this is set!
                WwgtPresParamChanged(hwnd, (ULONG)mp1, pViewData);
        break;

        /*
         * WM_DESTROY:
         *      clean up. This _must_ be passed on to
         *      ctrDefWidgetProc.
         */

        case WM_DESTROY:
            WwgtDestroy(hwnd, pViewData);
            mrc = pViewData->pfnwpDefWidgetProc(hwnd, msg, mp1, mp2);
        break;

        default:
            mrc = pViewData->pfnwpDefWidgetProc(hwnd, msg, mp1, mp2);
    } // end switch(msg)

    return (mrc);
}

/* ******************************************************************
 *
 *   Exported procedures
 *
 ********************************************************************/

/*
 *@@ WwgtInitModule:
 *      required export with ordinal 1, which must tell
 *      the XCenter how many widgets this DLL provides,
 *      and give the XCenter an array of XCENTERWIDGETCLASS
 *      structures describing the widgets.
 *
 *      With this call, you are given the module handle of
 *      XFLDR.DLL. For convenience, you may resolve imports
 *      for some useful functions which are exported thru
 *      src\shared\xwp.def. See the code below.
 *
 *      This function must also register the PM window classes
 *      which are specified in the XCENTERWIDGETCLASS array
 *      entries. For this, you are given a HAB which you
 *      should pass to WinRegisterClass. For the window
 *      class style (4th param to WinRegisterClass),
 *      you should specify
 *
 +          CS_PARENTCLIP | CS_CLIPCHILDREN | CS_SIZEREDRAW | CS_SYNCPAINT
 *
 *      You're widget window _will_ be resized, even if you're
 *      not planning it to be.
 *
 *      This function only gets called _once_ when the widget
 *      DLL has been successfully loaded by the XCenter. If
 *      there are several instances of a widget running (in
 *      the same or in several XCenters), this function does
 *      not get called again. However, since the XCenter unloads
 *      the widget DLLs again if they are no longer referenced
 *      by any XCenter, this might get called again when the
 *      DLL is re-loaded.
 *
 *      If this returns 0, this is considered an error. If
 *      this returns any value > 0, *ppaClasses must be
 *      set to a static array (best placed in the DLL's
 *      global data) of XCENTERWIDGETCLASS structures,
 *      which must have as many entries as the return value.
 */

ULONG EXPENTRY WwgtInitModule(HAB hab,         // XCenter's anchor block
                              HMODULE hmodXFLDR,    // XFLDR.DLL module handle
                              PXCENTERWIDGETCLASS *ppaClasses,
                              PSZ pszErrorMsg)  // if 0 is returned, 500 bytes of error msg
{
    ULONG   ulrc = 0,
            ul = 0;
    BOOL    fImportsFailed = FALSE;

    // resolve imports from XFLDR.DLL (this is basically
    // a copy of the doshResolveImports code, but we can't
    // use that before resolving...)
    for (ul = 0;
         ul < sizeof(G_aImports) / sizeof(G_aImports[0]);
         ul++)
    {
        if (DosQueryProcAddr(hmodXFLDR,
                             0,               // ordinal, ignored
                             (PSZ)G_aImports[ul].pcszFunctionName,
                             G_aImports[ul].ppFuncAddress)
                    != NO_ERROR)
        {
            sprintf(pszErrorMsg,
                    "Import %s failed.",
                    G_aImports[ul].pcszFunctionName);
            fImportsFailed = TRUE;
            break;
        }
    }

    if (!fImportsFailed)
    {
        if (!WinRegisterClass(hab,
                              WNDCLASS_WIDGET_WINLIST,
                              fnwpWinlistWidget,
                              CS_PARENTCLIP | CS_CLIPCHILDREN | CS_SIZEREDRAW | CS_SYNCPAINT,
                              sizeof(PWINLISTWIDGETDATA))
                                    // extra memory to reserve for QWL_USER
                             )
            strcpy(pszErrorMsg, "WinRegisterClass failed.");
        else
        {
            *ppaClasses = &G_WidgetClasses;

            // no error:
            // one class in this DLL:
            ulrc = 1;
        }
    }

    return (ulrc);
}

/*
 *@@ WwgtUnInitModule:
 *      optional export with ordinal 2, which can clean
 *      up global widget class data.
 *
 *      This gets called by the XCenter right before
 *      a widget DLL gets unloaded. Note that this
 *      gets called even if the "init module" export
 *      returned 0 (meaning an error) and the DLL
 *      gets unloaded right away.
 */

VOID EXPENTRY WwgtUnInitModule(VOID)
{
}

