
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
ew */

#define INCL_DOSPROCESS
#define INCL_DOSMODULEMGR
#define INCL_DOSERRORS

#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WINMESSAGEMGR
#define INCL_WINDIALOGS
#define INCL_WININPUT
#define INCL_WINSWITCHLIST
#define INCL_WINRECTANGLES
#define INCL_WINPOINTERS
#define INCL_WINSYS
#define INCL_WINMENUS
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
#define DONT_REPLACE_MALLOC         // in case mem debug is enabled
#include "setup.h"                      // code generation and debugging options

// disable wrappers, because we're not linking statically
#ifdef WINH_STANDARDWRAPPERS
    #undef WINH_STANDARDWRAPPERS
#endif

// headers in /helpers
#include "helpers\comctl.h"             // common controls (window procs)
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
#define WIDGET_BORDER           1
// width of window button borders:
#define THICK_BUTTON_BORDER     2

// string used for separating filters in setup strings;
// this better not appear in window titles
static const char *G_pcszFilterSeparator = "#~^ø@";

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

static XCENTERWIDGETCLASS G_WidgetClasses[]
        = {
            WNDCLASS_WIDGET_WINLIST,
            0,
            "WindowList",
            "Window list",
            WGTF_UNIQUEPERXCENTER | WGTF_TOOLTIP_AT_MOUSE,
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
 *      Note that importing functions from XFLDR.DLL
 *      is _not_ a requirement. We only do this to
 *      avoid duplicate code.
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

PDOSHMYPID pdoshMyPID = NULL;

PGPIHDRAW3DFRAME pgpihDraw3DFrame = NULL;
PGPIHMANIPULATERGB pgpihManipulateRGB = NULL;
PGPIHSWITCHTORGB pgpihSwitchToRGB = NULL;

PLSTAPPENDITEM plstAppendItem = NULL;
PLSTCLEAR plstClear = NULL;
PLSTCOUNTITEMS plstCountItems = NULL;
PLSTCREATE plstCreate = NULL;
PLSTINIT plstInit = NULL;
PLSTFREE plstFree = NULL;
PLSTMALLOC plstMalloc = NULL;
PLSTNODEFROMINDEX plstNodeFromIndex = NULL;
PLSTQUERYFIRSTNODE plstQueryFirstNode = NULL;
PLSTREMOVENODE plstRemoveNode = NULL;
PLSTSTRDUP plstStrDup = NULL;

PTMRSTARTXTIMER ptmrStartXTimer = NULL;
PTMRSTOPXTIMER ptmrStopXTimer = NULL;

PWINHCENTERWINDOW pwinhCenterWindow = NULL;
PWINHFREE pwinhFree = NULL;
PWINHMERGEINTOSUBMENU pwinhMergeIntoSubMenu = NULL;
PWINHQUERYPRESCOLOR pwinhQueryPresColor = NULL;
PWINHQUERYSWITCHLIST pwinhQuerySwitchList = NULL;
PWINHQUERYWINDOWFONT pwinhQueryWindowFont = NULL;
PWINHSETWINDOWFONT pwinhSetWindowFont = NULL;

PXSTRCAT pxstrcat = NULL;
PXSTRCPY pxstrcpy = NULL;
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
        "doshMyPID", (PFN*)&pdoshMyPID,
        "gpihDraw3DFrame", (PFN*)&pgpihDraw3DFrame,
        "gpihManipulateRGB", (PFN*)&pgpihManipulateRGB,
        "gpihSwitchToRGB", (PFN*)&pgpihSwitchToRGB,
        "lstAppendItem", (PFN*)&plstAppendItem,
        "lstClear", (PFN*)&plstClear,
        "lstCountItems", (PFN*)&plstCountItems,
        "lstCreate", (PFN*)&plstCreate,
        "lstFree", (PFN*)&plstFree,
        "lstInit", (PFN*)&plstInit,
        "lstMalloc", (PFN*)&plstMalloc,
        "lstNodeFromIndex", (PFN*)&plstNodeFromIndex,
        "lstQueryFirstNode", (PFN*)&plstQueryFirstNode,
        "lstRemoveNode", (PFN*)&plstRemoveNode,
        "lstStrDup", (PFN*)&plstStrDup,
        "tmrStartXTimer", (PFN*)&ptmrStartXTimer,
        "tmrStopXTimer", (PFN*)&ptmrStopXTimer,
        "winhCenterWindow", (PFN*)&pwinhCenterWindow,
        "winhFree", (PFN*)&pwinhFree,
        "winhMergeIntoSubMenu", (PFN*)&pwinhMergeIntoSubMenu,
        "winhQueryPresColor", (PFN*)&pwinhQueryPresColor,
        "winhQuerySwitchList", (PFN*)&pwinhQuerySwitchList,
        "winhQueryWindowFont", (PFN*)&pwinhQueryWindowFont,
        "winhSetWindowFont", (PFN*)&pwinhSetWindowFont,
        "xstrcat", (PFN*)&pxstrcat,
        "xstrcpy", (PFN*)&pxstrcpy,
        "xstrClear", (PFN*)&pxstrClear,
        "xstrInit", (PFN*)&pxstrInit
    };

/* ******************************************************************
 *
 *   Private widget instance data
 *
 ********************************************************************/

/* #define WLF_SHOWBUTTON      0x0001
#define WLF_LASTBUTTON      0x0002
#define WLF_JUMPABLE        0x0004
#define WLF_VISIBLE         0x0008 */

/*
 *@@ WINLISTSETUP:
 *      instance data to which setup strings correspond.
 *      This is also a member of WINLISTPRIVATE.
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
            // plain swtitle PSZs allocated with plstStrDup

} WINLISTSETUP, *PWINLISTSETUP;

/*
 *@@ WINLISTPRIVATE:
 *      more window data for the "Winlist" widget.
 *
 *      An instance of this is created on WM_CREATE
 *      fnwpWinlistWidget and stored in XCENTERWIDGET.pUser.
 */

typedef struct _WINLISTPRIVATE
{
    PXCENTERWIDGET pWidget;
            // reverse ptr to general widget data ptr; we need
            // that all the time and don't want to pass it on
            // the stack with each function call

    WINLISTSETUP Setup;
            // widget settings that correspond to a setup string

    HWND        hwndContextMenuHacked,
            // != 0 after context menu has been hacked for the first time
                hwndContextMenuShowing;
            // != 0 while context menu is showing (overriding standard widget)

    ULONG       ulTimerID;              // if != NULLHANDLE, update timer is running

    LINKLIST    llSwitchEntries;
            // linked list of SWCNTRL's with items to be displayed.
            // First item is displayed left, last item right.
            // This constructed from the current switch list, however:
            // -- filtered items are not on here;
            // -- items are always added to the tail (so new items
            //    appear to the right).
            // This is now a linked list (V0.9.11) because apparently
            // WinQuerySwitchList reuses the existing items so that
            // sometimes new items appear randomly in the middle.

            /*
              typedef struct _SWCNTRL {
                HWND         hwnd;                   window handle
                HWND         hwndIcon;               window-handle icon --> hacked to contain frame icon
                HPROGRAM     hprog;                  program handle
                PID          idProcess;              process identity
                ULONG        idSession;              session identity
                ULONG        uchVisibility;          visibility --> hacked to button index
                ULONG        fbJump;                 jump indicator
                CHAR         szSwtitle[MAXNAMEL+4];  switch-list control block title (null-terminated)
                ULONG        bProgType;              program type
              } SWCNTRL;
            */

            // Note that we hack the uchVisbility field to contain
            // the button index of the entry. We set this to -1
            // if no button is to be shown, just to make sure.

    PSWCNTRL    pCtrlActive,
            // ptr into pswBlock for last active window or NULL if none
                pCtrlSourceEmphasis,
            // ptr into pswBlock for button with source emphasis or NULL if none
                pCtrlMenu;
            // same as pCtrlSourceEmphasis; second field needed because WM_COMMAND
            // might come in after WM_MENUEND (for source emphasis)

    XSTRING     strTooltip;
            // tip for the tooltip control
} WINLISTPRIVATE, *PWINLISTPRIVATE;

ULONG ScanSwitchList(PWINLISTPRIVATE pPrivate,
                     PLINKLIST pllDirty);

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
            // append copy filter; use plstStrDup to
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

    sprintf(szTemp, "BGNDCOL=%06lX;",
            pSetup->lcolBackground);
    pxstrcat(pstrSetup, szTemp, 0);

    sprintf(szTemp, "TEXTCOL=%06lX;",
            pSetup->lcolForeground);
    pxstrcat(pstrSetup, szTemp, 0);

    if (pSetup->pszFont)
    {
        // non-default font:
        sprintf(szTemp, "FONT=%s;",
                pSetup->pszFont);
        pxstrcat(pstrSetup, szTemp, 0);
    }

    if (plstCountItems(&pSetup->llFilters))
    {
        // we have items on the filters list:
        PLISTNODE   pNode = plstQueryFirstNode(&pSetup->llFilters);
        BOOL        fFirst = TRUE;
        // add keyword first
        pxstrcat(pstrSetup, "FILTERS=", 0);

        while (pNode)
        {
            PSZ pszFilter = (PSZ)pNode->pItemData;
            if (!fFirst)
                // not first loop: add separator first
                pxstrcat(pstrSetup, G_pcszFilterSeparator, 0);

            // append this filter
            pxstrcat(pstrSetup, pszFilter, 0);

            pNode = pNode->pNext;
            fFirst = FALSE;
        }

        // add terminator
        pxstrcat(pstrSetup, ";", 0);
    }
}

/* ******************************************************************
 *
 *   Widget settings dialog
 *
 ********************************************************************/

/*
 *@@ DumpSwitchList:
 *      puts the current switch list entries into the listbox.
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
 *      sets up the dialog according to the current settings.
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
 *      enables the items in the dialog according to the
 *      current selections.
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
 *      retrieves the new settings from the dialog.
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
        // append copy filter; use plstStrDup to
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
                            pData->pctrSetSetupString(pData->hSettings,
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
                                            ID_XSH_WIDGET_WINLIST_SETTINGS);
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
 *   Callbacks stored in XCENTERWIDGETCLASS
 *
 ********************************************************************/

/*
 *@@ WwgtShowSettingsDlg:
 *      this displays the winlist widget's settings
 *      dialog.
 *
 *      This procedure's address is stored in
 *      XCENTERWIDGET so that the XCenter knows that
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

VOID GetPaintableRect(PWINLISTPRIVATE pPrivate,
                      PRECTL prcl)
{
    WinQueryWindowRect(pPrivate->pWidget->hwndWidget,
                       prcl);

    // if (pPrivate->pWidget->pGlobals->flDisplayStyle & XCS_SUNKBORDERS)
    {
        prcl->xLeft += WIDGET_BORDER;
        prcl->xRight -= WIDGET_BORDER;
        prcl->yBottom += WIDGET_BORDER;
        prcl->yTop -= WIDGET_BORDER;
    }

    // we won't use WinInflateRect... what the hell
    // does this API need an anchor block for to
    // do a couple of additions?!?
    /*
    WinInflateRect(pWidget->hab,
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
 *
 *@@changed V0.9.12 (2001-04-28) [umoeller]: now matching first chars of filter only
 */

BOOL IsCtrlFiltered(PLINKLIST pllFilters,   // in: pPrivate->Setup.llFilters
                    HWND hwndXCenterFrame,  // in: pPrivate->pWidget->pGlobals->hwndFrame
                    PSWCNTRL pCtrl)
{
    BOOL brc = FALSE;

    // rule out invisible tasklist entries
    if ((pCtrl->uchVisibility & SWL_VISIBLE) == 0)
        brc = TRUE;
    // rule out the XCenter frame
    else if (pCtrl->hwnd == hwndXCenterFrame)
        brc = TRUE;
    else
    {
        // go thru user-defined filters
        PLISTNODE pNode = plstQueryFirstNode(pllFilters);
        while (pNode)
        {
            PSZ pszFilterThis = (PSZ)pNode->pItemData;
            if (!strncmp(pCtrl->szSwtitle,
                         pszFilterThis,
                         strlen(pszFilterThis)))    // match length of filter only
                                                    // V0.9.12 (2001-04-28) [umoeller]
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
 *@@ SuperQuerySwitchList:
 *      calls winhQuerySwitchList and hacks the entries
 *      so that we know about what to display right.
 *
 *      Returns a new SWBLOCK or NULL on errors.
 *
 *      Gets called from ScanSwitchList and UpdateSwitchList.
 *
 *@@added V0.9.7 (2001-01-03) [umoeller]
 */

/* PSWBLOCK SuperQuerySwitchList(HAB hab,          // in: pPrivate->pWidget->habWidget
                              PLINKLIST pllFilters,   // in: pPrivate->Setup.llFilters
                              HWND hwndXCenterFrame,  // in: pPrivate->pWidget->pGlobals->hwndFrame
                              PULONG pcShow)    // out: count of visible swentries
{
    ULONG ul = 0;
    PSWBLOCK pswBlock = pwinhQuerySwitchList(hab);
                        // calls WinQuerySwitchList

    *pcShow = 0;

    if (pswBlock)
    {
        // we must mark the last valid item to paint
        // so that the paint routines can make it a bit wider
        PSWCNTRL    pCtrlLast2Paint = NULL;

        // now go filter and get icons
        for (ul = 0;
             ul < pswBlock->cswentry;
             ul++)
        {
            PSWCNTRL pCtrlThis = &pswBlock->aswentry[ul].swctl;

            // now set up the fbJump field, which we use
            // for our own display flags... ugly, but this
            // saves us from reallocating a second array
            // for our own data
            if (pCtrlThis->fbJump & SWL_JUMPABLE)
                pCtrlThis->fbJump = WLF_JUMPABLE;
            else
                pCtrlThis->fbJump = 0;

            // apply filter
            if (!IsCtrlFiltered(pllFilters, hwndXCenterFrame, pCtrlThis))
            {
                // item will be painted:
                // mark it so (hack field)
                pCtrlThis->fbJump |= WLF_SHOWBUTTON;

                // store button index (hack field)
                // and raise it for next one (will
                // also be count in the end)
                pCtrlThis->uchVisibility = (*pcShow)++;
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

        if (pCtrlLast2Paint)
            // we had at least one button:
            // mark the last one
            pCtrlLast2Paint->fbJump |= WLF_LASTBUTTON;
    } // if (pswBlock)

    return (pswBlock);
} */

/*
 *@@ FindSwitchNodeFromHWND:
 *      searches the given list of SWCNTRL entries
 *      for the specified HWND.
 *
 *@@added V0.9.11 (2001-04-18) [umoeller]
 */

PLISTNODE FindSwitchNodeFromHWND(PLINKLIST pll,
                                 HWND hwnd)
{
    PLISTNODE pNode;
    for (pNode = plstQueryFirstNode(pll);
         pNode;
         pNode = pNode->pNext)
    {
        PSWCNTRL pCtrl = (PSWCNTRL)pNode->pItemData;
        if (pCtrl->hwnd == hwnd)
            return (pNode);
    }

    return (NULL);
}

#define SCANF_REDRAWSOME         0x0001
#define SCANF_REDRAWALL             0x1000

/*
 *@@ ScanSwitchList:
 *      scans or rescans the PM task list (switch list)
 *      and initializes all data in WINLISTPRIVATE
 *      accordingly.
 *
 *      Gets called from WwgtCreate.
 *
 *      This does not repaint the widget, but instead
 *      returns a bitfield with the following flags:
 *
 *      -- SCANF_REDRAWSOME: only some nodes changed,
 *         and pllDirty has received a list of SWCNTRL
 *         nodes (pointing into the main list) to be
 *         repainted. This also happens if only the
 *         active window changed.
 *
 *      -- SCANF_REDRAWALL: redraw all (items added or
 *         removed or icons or texts changed).
 *
 *      -- 0: no repaint necessary.
 *
 *@@changed V0.9.11 (2001-04-18) [umoeller]: rewritten
 *@@changed V0.9.12 (2001-04-28) [umoeller]: didn't pick up changes in the filters, fixed
 */

ULONG ScanSwitchList(PWINLISTPRIVATE pPrivate,
                     PLINKLIST pllDirty)
{
    ULONG       flReturn = 0;
    PSWBLOCK    pswBlock = pwinhQuerySwitchList(pPrivate->pWidget->habWidget);

    PLINKLIST   pllFilters = &pPrivate->Setup.llFilters;

    if (pswBlock)
    {
        // loop 1: go thru all items in the "show" list
        // and see if they are still in the switch list.

        ULONG       ul;
        ULONG       ulIndexThis = 0;            // item counter
        PSWCNTRL    pNewActive = NULL;
        HWND        hwndCurrentActive = WinQueryActiveWindow(HWND_DESKTOP);

        PLISTNODE pNode = plstQueryFirstNode(&pPrivate->llSwitchEntries);
        while (pNode)
        {
            PSWCNTRL    pCtrlInList = (PSWCNTRL)pNode->pItemData;
            BOOL        fRemove = TRUE;
            PLISTNODE   pNodeNext = pNode->pNext;

            // check if this is still in the current switchlist
            for (ul = 0;
                 ul < pswBlock->cswentry;
                 ul++)
            {
                PSWCNTRL pCtrlThis = &pswBlock->aswentry[ul].swctl;
                if (    (pCtrlThis->hwnd == pCtrlInList->hwnd)
                     && (pCtrlThis->idProcess == pCtrlInList->idProcess)
                     && (pCtrlThis->idSession == pCtrlInList->idSession)
                     && (pCtrlThis->bProgType == pCtrlInList->bProgType)
                   )
                {
                    HWND hwndIcon;
                    BOOL fDirty = FALSE;

                    // OK, this list node is still in the switchlist:
                    // check if all the data is still valid
                    if (strcmp(pCtrlThis->szSwtitle, pCtrlInList->szSwtitle))
                    {
                        // session title changed:
                        memcpy(pCtrlInList->szSwtitle,
                               pCtrlThis->szSwtitle,
                               sizeof(pCtrlThis->szSwtitle));
                        fDirty = TRUE;
                    }

                    // check icon
                    hwndIcon = (HWND)WinSendMsg(pCtrlInList->hwnd,
                                                WM_QUERYICON, 0, 0);
                    if (hwndIcon != pCtrlInList->hwndIcon)
                    {
                        // icon changed:
                        pCtrlInList->hwndIcon = hwndIcon;
                        fDirty = TRUE;
                    }

                    // check if this item is now filtered;
                    // maybe filters changed, or the title changed
                    // in a way that a filter now applies
                    // V0.9.12 (2001-04-28) [umoeller]
                    if (!IsCtrlFiltered(pllFilters,
                                        pPrivate->pWidget->pGlobals->hwndFrame, // XCenter frame
                                        pCtrlThis))
                    {
                        if (fDirty)
                        {
                            if (pllDirty)
                                // caller wants dirty windows:
                                plstAppendItem(pllDirty, pCtrlInList);
                            flReturn |= SCANF_REDRAWSOME;
                        }

                        // so do not remove this list node
                        fRemove = FALSE;
                    }
                    // else item filtered now: fRemove is still FALSE

                    break;      // for switch list
                }
            }

            if (fRemove)
            {
                if (pCtrlInList == pPrivate->pCtrlActive)
                    // this was marked as active:
                    pPrivate->pCtrlActive = NULL;

                // alright, remove this
                plstRemoveNode(&pPrivate->llSwitchEntries,
                               pNode);           // auto-free
                flReturn = SCANF_REDRAWALL;
            }

            // remember currently active switch entry
            if (pCtrlInList->hwnd == hwndCurrentActive)
                pNewActive = pCtrlInList;       // checked below

            // re-number all list entries
            if (!fRemove)
                pCtrlInList->uchVisibility = ulIndexThis++;

            pNode = pNodeNext;
        } // end while (pNode)

        // we can skip this check if we're redrawing anyway
        if (0 == (flReturn & SCANF_REDRAWALL))
        {
            // loop 2: go thru all items in the switch list
            // and see if they have already been added to
            // the "show" list, if not filtered
            ulIndexThis = plstCountItems(&pPrivate->llSwitchEntries);

            for (ul = 0;
                 ul < pswBlock->cswentry;
                 ul++)
            {
                PSWCNTRL pCtrlThis = &pswBlock->aswentry[ul].swctl;

                // apply filter
                if (!IsCtrlFiltered(pllFilters,
                                    pPrivate->pWidget->pGlobals->hwndFrame, // XCenter frame
                                    pCtrlThis))
                {
                    if (!FindSwitchNodeFromHWND(&pPrivate->llSwitchEntries,
                                                pCtrlThis->hwnd))
                    {
                        // not in list yet:
                        // append
                        PSWCNTRL pNew = plstMalloc(sizeof(SWCNTRL));
                                    // use XFLDR.DLL heap, this is auto-free
                        if (pNew)
                        {
                            memcpy(pNew, pCtrlThis, sizeof(SWCNTRL));

                            // hack fields
                            pNew->uchVisibility = ulIndexThis++;

                            // get its icon
                            pNew->hwndIcon = (HWND)WinSendMsg(pCtrlThis->hwnd,
                                                              WM_QUERYICON, 0, 0);

                            plstAppendItem(&pPrivate->llSwitchEntries,
                                           pNew);

                            flReturn = SCANF_REDRAWALL;

                            // check if this is active
                            if (pNew->hwnd == hwndCurrentActive)
                                pPrivate->pCtrlActive = pNew;
                        }
                    } // end if (!FindSwitchNodeFromHWND(pPrivate,
                } // end if (!IsCtrlFiltered(pllFilters,
            } // end for (ul = 0; ul < pswBlock->cswentry;

            if (0 == (flReturn & SCANF_REDRAWALL))
            {
                // we're not redrawing all yet:
                // check if active window changed
                if (pPrivate->pCtrlActive != pNewActive)
                {
                    // changed:
                    flReturn |= SCANF_REDRAWSOME;

                    if (pllDirty)
                    {
                        // caller wants dirty windows:
                        if (pPrivate->pCtrlActive)
                            if (!FindSwitchNodeFromHWND(pllDirty, pPrivate->pCtrlActive->hwnd))
                                // old active is not on dirty list yet: append
                                plstAppendItem(pllDirty, pPrivate->pCtrlActive);

                        if (pNewActive)
                            if (!FindSwitchNodeFromHWND(pllDirty, pNewActive->hwnd))
                                // new active is not on dirty list yet: append
                                plstAppendItem(pllDirty, pNewActive);
                    }

                    pPrivate->pCtrlActive = pNewActive;
                }
            }
        } // end if (0 == (flReturn & SCANF_REDRAWALL))

        pwinhFree(pswBlock);
    } // end if (pswBlock)

    return (flReturn);
}

/*
 *@@ CalcButtonCX:
 *      returns the width of the specified button.
 *      *pcxRegular receives the regular width;
 *      this is the same as the returned size of
 *      the current button, except if the button
 *      is the last one.
 *
 *@@added V0.9.9 (2001-01-29) [umoeller]
 */

ULONG CalcButtonCX(PWINLISTPRIVATE pPrivate,
                   PRECTL prclSubclient,
                   PSWCNTRL pCtrlThis,      // in: switch list entry; can be NULL
                   PULONG pcxRegular)
{
    ULONG cxPerButton = 0,
          cShow = plstCountItems(&pPrivate->llSwitchEntries);

    // avoid division by zero
    if (cShow)
    {
        // max paint space:
        ULONG   ulCX = (prclSubclient->xRight - prclSubclient->xLeft);
        // calc width for this button...
        // use standard with, except for last button
        cxPerButton = ulCX / cShow;

        // limit size per button V0.9.9 (2001-01-29) [umoeller]
        if (cxPerButton > 130)
        {
            cxPerButton = 130;
            *pcxRegular = cxPerButton;
        }
        else
        {
            // no limit:
            *pcxRegular = cxPerButton;
            /* if ((pCtrlThis) && (pCtrlThis->fbJump & WLF_LASTBUTTON))
                // last button: add leftover space
                cxPerButton += (ulCX % cShow); */ // @@todo
        }
    }

    return (cxPerButton);
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
 *@@changed V0.9.9 (2001-02-08) [umoeller]: now centering icons on button vertically
 */

VOID DrawOneCtrl(PWINLISTPRIVATE pPrivate,
                 HPS hps,
                 PRECTL prclSubclient,     // in: paint area (exclusive)
                 PSWCNTRL pCtrlThis,       // in: switch list entry to paint
                 HWND hwndActive,          // in: currently active window
                 PULONG pulNextX)          // out: next X pos (ptr can be NULL)
{
    // colors for borders
    LONG    lLeft,
            lRight;
    LONG    xText = 0;
    ULONG   cShow = plstCountItems(&pPrivate->llSwitchEntries);

    const XCENTERGLOBALS *pGlobals = pPrivate->pWidget->pGlobals;

    // avoid division by zero
    if (cShow)
    {
        RECTL   rclButtonArea;

        LONG    lButtonColor = pPrivate->Setup.lcolBackground;

        ULONG   cxRegular = 0,
                cxThis = CalcButtonCX(pPrivate,
                                      prclSubclient,
                                      pCtrlThis,
                                      &cxRegular);

        LONG    lButtonBorder
            = ((pGlobals->flDisplayStyle & XCS_FLATBUTTONS) == 0)
                            ? THICK_BUTTON_BORDER   // raised buttons
                            : 1;        // flat buttons

        if ((hwndActive) && (pCtrlThis->hwnd == hwndActive))
        {
            // active window: paint rect lowered
            lLeft = pGlobals->lcol3DDark;
            lRight = pGlobals->lcol3DLight;

            // and paint button lighter:
            pgpihManipulateRGB(&lButtonColor, 1.1);
        }
        else
        {
            lLeft = pGlobals->lcol3DLight;
            lRight = pGlobals->lcol3DDark;
        }

        // draw frame
        rclButtonArea.yBottom = prclSubclient->yBottom;
        // calculate X coordinate: uchVisibility has
        // the button index as calculated by ScanSwitchList,
        // so we can multiply
        rclButtonArea.xLeft = prclSubclient->xLeft
                              + (pCtrlThis->uchVisibility * cxRegular);
        rclButtonArea.yTop = prclSubclient->yTop - 1;
        rclButtonArea.xRight = rclButtonArea.xLeft + cxThis - 1;

        // draw button frame
        pgpihDraw3DFrame(hps,
                         &rclButtonArea,        // inclusive
                         lButtonBorder,
                         lLeft,
                         lRight);

        // store next X if caller wants it
        if (pulNextX)
            *pulNextX = rclButtonArea.xRight + 1;

        // source emphasis for this control?
        if (    (pPrivate->pCtrlSourceEmphasis)
             && (pCtrlThis == pPrivate->pCtrlSourceEmphasis)
           )
        {
            // yes:
            POINTL ptl;
            GpiSetColor(hps, RGBCOL_BLACK);
            GpiSetLineType(hps, LINETYPE_DOT);
            ptl.x = rclButtonArea.xLeft;
            ptl.y = rclButtonArea.yBottom;
            GpiMove(hps, &ptl);
            ptl.x = rclButtonArea.xRight;
            ptl.y = rclButtonArea.yTop;
            GpiBox(hps,
                   DRO_OUTLINE,
                   &ptl,
                   0, 0);
        }

        // draw button middle
        rclButtonArea.xLeft += lButtonBorder;
        rclButtonArea.yBottom += lButtonBorder;
        rclButtonArea.xRight -= (lButtonBorder - 1);
        rclButtonArea.yTop -= (lButtonBorder - 1);
        WinFillRect(hps,
                    &rclButtonArea,         // exclusive
                    lButtonColor);

        if (pCtrlThis->hwndIcon)
        {
            // V0.9.9 (2001-02-08) [umoeller] now centering icons
            // on buttons vertically, if the widget is really tall
            ULONG   cy = rclButtonArea.yTop - rclButtonArea.yBottom;
            LONG    y = rclButtonArea.yBottom + (cy - pGlobals->cxMiniIcon) / 2;
            if (y < rclButtonArea.yBottom + 1)
                y = rclButtonArea.yBottom + 1;
            WinDrawPointer(hps,
                           rclButtonArea.xLeft + 1,
                           y,
                           pCtrlThis->hwndIcon,
                           DP_MINI);
            rclButtonArea.xLeft += pGlobals->cxMiniIcon;
        }

        // add another pixel for the text
        rclButtonArea.xLeft++;
        // dec right for clipping
        rclButtonArea.xRight--;

        // draw switch list title
        WinDrawText(hps,
                    strlen(pCtrlThis->szSwtitle),
                    pCtrlThis->szSwtitle,
                    &rclButtonArea,
                    pPrivate->Setup.lcolForeground,
                    0,
                    DT_LEFT | DT_VCENTER);
    }
}

/*
 *@@ DrawAllCtrls:
 *      redraws all visible switch list controls by invoking
 *      DrawOneCtrl on them. Gets called from WwgtPaint.
 *
 *@@changed V0.9.11 (2001-04-18) [umoeller]: adjusted for new linklist
 */

VOID DrawAllCtrls(PWINLISTPRIVATE pPrivate,
                  HPS hps,
                  PRECTL prclSubclient)       // in: max available space (exclusive)
{
    ULONG   ul = 0;

    LONG    lcolBackground = pPrivate->Setup.lcolBackground;

    // count of entries in switch list:
    ULONG   cEntries = plstCountItems(&pPrivate->llSwitchEntries);

    // make background color darker:
    pgpihManipulateRGB(&lcolBackground, 0.91);

    if (!cEntries)      // avoid division by zero
    {
        // draw no buttons
        WinFillRect(hps,
                    prclSubclient,
                    lcolBackground);
    }
    else
    {
        HWND    hwndActive = WinQueryActiveWindow(HWND_DESKTOP);

        ULONG   ulNextX = 0;

        PLISTNODE pNode;
        for (pNode = plstQueryFirstNode(&pPrivate->llSwitchEntries);
             pNode;
             pNode = pNode->pNext)
        {
            PSWCNTRL pCtrlThis = (PSWCNTRL)pNode->pItemData;
            DrawOneCtrl(pPrivate,
                        hps,
                        prclSubclient,
                        pCtrlThis,
                        hwndActive,
                        &ulNextX);
        }

        if (ulNextX < prclSubclient->xRight)
        {
            // we have leftover space at the right:
            RECTL rcl2;
            rcl2.xLeft = ulNextX;
            rcl2.yBottom = prclSubclient->yBottom;
            rcl2.xRight = prclSubclient->xRight;
            rcl2.yTop = prclSubclient->yTop;
            WinFillRect(hps,
                        &rcl2,
                        lcolBackground);
        }
    } // end if (cWins)
}

/*
 *@@ UpdateSwitchList:
 *      updates the switch list. Gets called from WwgtTimer
 *      to check if the switch list has changed... if so,
 *      we repaint parts of the window, or the entire window
 *      maybe.
 *
 *      This is now pretty efficient... it can even repaint
 *      single switch entries if its icon or title has changed.
 *
 *@@added V0.9.7 (2001-01-03) [umoeller]
 *@@changed V0.9.11 (2001-04-18) [umoeller]: rewritten
 */

VOID UpdateSwitchList(HWND hwnd,
                      PWINLISTPRIVATE pPrivate)
{
    // re-scan the system switchlist and update the
    // internal list... flRepaint tells us if and what
    // we need to repaint, and llDirty receives a list
    // of SWCNTRL nodes to repaint

    LINKLIST llDirty;
    ULONG flRepaint;
    plstInit(&llDirty, FALSE);
    flRepaint = ScanSwitchList(pPrivate,
                               &llDirty);

    if (flRepaint & SCANF_REDRAWALL)
    {
        // redraw all:
        HPS hps = WinGetPS(hwnd);
        if (hps)
        {
            RECTL rclSubclient;
            GetPaintableRect(pPrivate, &rclSubclient);
            pgpihSwitchToRGB(hps);
            DrawAllCtrls(pPrivate,
                         hps,
                         &rclSubclient);
            WinReleasePS(hps);
        }
    }
    else if (    (flRepaint & SCANF_REDRAWSOME)
              && (plstCountItems(&llDirty))
            )
    {
        // redraw only some: go thru list then
        HPS hps = WinGetPS(hwnd);
        if (hps)
        {
            RECTL       rclSubclient;
            PLISTNODE   pNode;
            HWND        hwndActive = WinQueryActiveWindow(HWND_DESKTOP);

            GetPaintableRect(pPrivate, &rclSubclient);
            pgpihSwitchToRGB(hps);

            for (pNode = plstQueryFirstNode(&llDirty);
                 pNode;
                 pNode = pNode->pNext)
            {
                PSWCNTRL pCtrl = (PSWCNTRL)pNode->pItemData;
                DrawOneCtrl(pPrivate,
                            hps,
                            &rclSubclient,     // in: paint area (exclusive)
                            pCtrl,
                            hwndActive,
                            NULL);
            }

            WinReleasePS(hps);
        }
    }

    plstClear(&llDirty);
}

/*
 *@@ FindCtrlFromPoint:
 *      returns the SWCNTRL from the global list
 *      which matches the specified point, which
 *      must be in the widget's window coordinates.
 *
 *      Returns NULL if not found.
 *
 *@@changed V0.9.11 (2001-04-18) [umoeller]: adjusted for new linklist
 */

PSWCNTRL FindCtrlFromPoint(PWINLISTPRIVATE pPrivate,
                           PPOINTL pptl,
                           PRECTL prclSubclient)    // in: from GetPaintableRect
{
    PSWCNTRL pCtrl = NULL;

    if (WinPtInRect(pPrivate->pWidget->habWidget,
                    prclSubclient,
                    pptl))
    {
        // ULONG   ulCX = (prclSubclient->xRight - prclSubclient->xLeft);
        // calc width for this button...
        // use standard with, except for last button
        ULONG   cxRegular = 0,
                cxThis = CalcButtonCX(pPrivate,
                                      prclSubclient,
                                      NULL,
                                      &cxRegular);
        // avoid division by zero
        if (cxRegular)
        {
            ULONG   ulButtonIndex = pptl->x / cxRegular;
            // go find button with that index
            PLISTNODE pNode = plstNodeFromIndex(&pPrivate->llSwitchEntries,
                                                ulButtonIndex);
            if (pNode)
                pCtrl = (PSWCNTRL)pNode->pItemData;
        }
    }

    return (pCtrl);
}

/*
 *@@ WwgtCreate:
 *      implementation for WM_CREATE.
 *
 *@@changed V0.9.11 (2001-04-18) [umoeller]: adjusted for new linklist
 */

MRESULT WwgtCreate(HWND hwnd,
                   PXCENTERWIDGET pWidget)
{
    MRESULT mrc = 0;
    PSZ p;
    PWINLISTPRIVATE pPrivate = malloc(sizeof(WINLISTPRIVATE));
    memset(pPrivate, 0, sizeof(WINLISTPRIVATE));
    // link the two together
    pWidget->pUser = pPrivate;
    pPrivate->pWidget = pWidget;

    pxstrInit(&pPrivate->strTooltip, 0);

    plstInit(&pPrivate->llSwitchEntries,
             TRUE);         // auto-free

    // initialize binary setup structure from setup string
    WwgtScanSetup(pWidget->pcszSetupString,
                  &pPrivate->Setup);

    // set window font (this affects all the cached presentation
    // spaces we use)
    pwinhSetWindowFont(hwnd,
                       (pPrivate->Setup.pszFont)
                        ? pPrivate->Setup.pszFont
                        // default font: use the same as in the rest of XWorkplace:
                        : pcmnQueryDefaultFont());

    // enable context menu help
    pWidget->pcszHelpLibrary = pcmnQueryHelpLibrary();
    pWidget->ulHelpPanelID = ID_XSH_WIDGET_WINLIST_MAIN;

    // initialize switch list data
    ScanSwitchList(pPrivate,
                   NULL);

    // start update timer
    pPrivate->ulTimerID = ptmrStartXTimer(pWidget->pGlobals->pvXTimerSet,
                                         hwnd,
                                         1,
                                         300);

    return (mrc);
}

/*
 *@@ MwgtControl:
 *      implementation for WM_CONTROL.
 *
 *@@added V0.9.7 (2000-12-14) [umoeller]
 */

BOOL WwgtControl(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    BOOL brc = FALSE;

    PXCENTERWIDGET pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER);
    if (pWidget)
    {
        PWINLISTPRIVATE pPrivate = (PWINLISTPRIVATE)pWidget->pUser;
        if (pPrivate)
        {
            USHORT  usID = SHORT1FROMMP(mp1),
                    usNotifyCode = SHORT2FROMMP(mp1);

            if (usID == ID_XCENTER_CLIENT)
            {
                switch (usNotifyCode)
                {
                    /*
                     * XN_QUERYSIZE:
                     *      XCenter wants to know our size.
                     */

                    case XN_QUERYSIZE:
                    {
                        PSIZEL pszl = (PSIZEL)mp2;
                        pszl->cx = -1;
                        pszl->cy = 2 * WIDGET_BORDER        // thin border
                                   + 2                      // space around icon
                                   + pWidget->pGlobals->cxMiniIcon;
                        if ((pWidget->pGlobals->flDisplayStyle & XCS_FLATBUTTONS) == 0)
                            // raised buttons
                            pszl->cy += 2 * THICK_BUTTON_BORDER;
                        else
                            // flat buttons
                            pszl->cy += 2;      // 2*1 pixel for thin border
                        brc = TRUE;
                    break; }

                    /*
                     * XN_SETUPCHANGED:
                     *      XCenter has a new setup string for
                     *      us in mp2.
                     */

                    case XN_SETUPCHANGED:
                    {
                        const char *pcszNewSetupString = (const char*)mp2;

                        // reinitialize the setup data
                        WwgtClearSetup(&pPrivate->Setup);
                        WwgtScanSetup(pcszNewSetupString, &pPrivate->Setup);

                        // rescan switch list, because filters might have changed
                        ScanSwitchList(pPrivate,
                                       NULL);
                        WinInvalidateRect(pWidget->hwndWidget, NULL, FALSE);
                    break; }


                }
            } // if (usID == ID_XCENTER_CLIENT)
            else
            {
                if (usID == ID_XCENTER_TOOLTIP)
                {
                    switch (usNotifyCode)
                    {
                        case TTN_NEEDTEXT:
                        {
                            PTOOLTIPTEXT pttt = (PTOOLTIPTEXT)mp2;
                            RECTL       rclSubclient;
                            POINTL      ptlPointer;
                            PSWCNTRL    pCtrlClicked = 0;

                            // find the winlist item under the mouse
                            WinQueryPointerPos(HWND_DESKTOP,
                                               &ptlPointer);
                            WinMapWindowPoints(HWND_DESKTOP,    // from
                                               hwnd,
                                               &ptlPointer,
                                               1);
                            GetPaintableRect(pPrivate, &rclSubclient);
                            pCtrlClicked = FindCtrlFromPoint(pPrivate,
                                                             &ptlPointer,
                                                             &rclSubclient);
                            pxstrClear(&pPrivate->strTooltip);
                            if (pCtrlClicked)
                                pxstrcpy(&pPrivate->strTooltip,
                                         pCtrlClicked->szSwtitle,
                                         0);
                            else
                                pxstrcpy(&pPrivate->strTooltip,
                                         "Window list",
                                         0);

                            pttt->pszText = pPrivate->strTooltip.psz;
                            pttt->ulFormat = TTFMT_PSZ;
                        break; }
                    }
                }
            }
        } // end if (pPrivate)
    } // end if (pWidget)

    return (brc);
}

/*
 *@@ WwgtPaint:
 *      implementation for WM_PAINT.
 */

VOID WwgtPaint(HWND hwnd)
{
    PXCENTERWIDGET pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER);
    if (pWidget)
    {
        PWINLISTPRIVATE pPrivate = (PWINLISTPRIVATE)pWidget->pUser;
        if (pPrivate)
        {
            HPS hps = WinBeginPaint(hwnd, NULLHANDLE, NULL);
            if (hps)
            {
                RECTL       rclWin;

                // now paint frame
                WinQueryWindowRect(hwnd,
                                   &rclWin);        // exclusive
                pgpihSwitchToRGB(hps);

                // if (pPrivate->pWidget->pGlobals->flDisplayStyle & XCS_SUNKBORDERS)
                {
                    rclWin.xRight--;
                    rclWin.yTop--;
                    pgpihDraw3DFrame(hps,
                                     &rclWin,           // inclusive
                                     WIDGET_BORDER,
                                     pWidget->pGlobals->lcol3DDark,
                                     pWidget->pGlobals->lcol3DLight);

                    rclWin.xLeft++;
                    rclWin.yBottom++;
                }

                // now paint buttons in the middle
                DrawAllCtrls(pPrivate,
                             hps,
                             &rclWin);

                WinEndPaint(hps);
            }
        }
    }
}

/*
 *@@ WwgtTimer:
 *      implementation for WM_TIMER. This checks whether
 *      the switch list has changed and repaints everything
 *      or only the parts that have changed.
 */

VOID WwgtTimer(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    PXCENTERWIDGET pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER);
    if (pWidget)
    {
        PWINLISTPRIVATE pPrivate = (PWINLISTPRIVATE)pWidget->pUser;
        if (pPrivate)
        {
            if ((USHORT)mp1 == 1)
                UpdateSwitchList(hwnd, pPrivate);
            else
                pWidget->pfnwpDefWidgetProc(hwnd, WM_TIMER, mp1, mp2);
        } // end if (pPrivate)
    }
}

/*
 *@@ WwgtButton1Down:
 *      implementation for WM_BUTTON1DOWN.
 *
 *@@changed V0.9.11 (2001-04-18) [umoeller]: fixed minimized to maximized, which never worked
 *@@changed V0.9.12 (2001-05-12) [umoeller]: fixed showing hidden windows
 */

VOID WwgtButton1Down(HWND hwnd,
                     MPARAM mp1)
{
    PXCENTERWIDGET pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER);
    if (pWidget)
    {
        PWINLISTPRIVATE pPrivate = (PWINLISTPRIVATE)pWidget->pUser;
        if (pPrivate)
        {
            POINTL      ptlClick;
            PSWCNTRL    pCtrlClicked = 0;
            RECTL       rclSubclient;

            ptlClick.x = SHORT1FROMMP(mp1);
            ptlClick.y = SHORT2FROMMP(mp1);
            GetPaintableRect(pPrivate, &rclSubclient);

            // find the button the user clicked on
            pCtrlClicked = FindCtrlFromPoint(pPrivate,
                                             &ptlClick,
                                             &rclSubclient);
            if (pCtrlClicked)
            {
                // check the window's state...
                SWP swp;
                BOOL fRedrawActive = FALSE;
                if (WinQueryWindowPos(pCtrlClicked->hwnd, &swp))
                {
                    if (swp.fl & (SWP_HIDE | SWP_MINIMIZE))
                    {
                        // window is hidden or minimized:
                        // restore and activate
                        /* if (WinSetWindowPos(pCtrlClicked->hwnd,
                                            HWND_TOP,
                                            0, 0, 0, 0,
                                            SWP_SHOW | SWP_RESTORE | SWP_ACTIVATE | SWP_ZORDER)) */

                        // the above never worked for maximized windows which
                        // were minimized... the trick is to switch to the
                        // program instead! V0.9.11 (2001-04-18) [umoeller]
                        HSWITCH hsw = WinQuerySwitchHandle(pCtrlClicked->hwnd,
                                                           pCtrlClicked->idProcess);
                        if (hsw)
                        {
                            // first check if the thing is hidden V0.9.12 (2001-05-12) [umoeller]
                            if (swp.fl & SWP_HIDE)
                                // yes: show V0.9.12 (2001-05-12) [umoeller]
                                WinSetWindowPos(pCtrlClicked->hwnd,
                                                0, 0, 0, 0, 0,
                                                SWP_SHOW);

                            if (!WinSwitchToProgram(hsw))   // this returns 0 on success... sigh
                            {
                                // OK, now we have the program active, but it's
                                // probably in the background...
                                WinSetWindowPos(pCtrlClicked->hwnd,
                                                HWND_TOP,
                                                0, 0, 0, 0,
                                                SWP_SHOW | SWP_ACTIVATE | SWP_ZORDER);
                                fRedrawActive = TRUE;
                            }
                        }
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
                {
                    HPS hps = WinGetPS(hwnd);
                    if (hps)
                    {
                        PSWCNTRL    pOldActive = pPrivate->pCtrlActive;
                        pgpihSwitchToRGB(hps);

                        // store new active ctrl
                        pPrivate->pCtrlActive = pCtrlClicked;

                        if (pOldActive)
                            // un-activate old button
                            DrawOneCtrl(pPrivate,
                                        hps,
                                        &rclSubclient,
                                        pOldActive,         // old active
                                        pCtrlClicked->hwnd,     // active wnd
                                        NULL);

                        DrawOneCtrl(pPrivate,
                                    hps,
                                    &rclSubclient,
                                    pCtrlClicked,       // new active
                                    pCtrlClicked->hwnd,     // active wnd
                                    NULL);

                        WinReleasePS(hps);
                    }
                }
            }
        }
    }
}

/*
 *@@ IsEnabled:
 *
 */

BOOL IsEnabled(HWND hwndMenu, USHORT usItem)
{
    BOOL brc = FALSE;
    if ((SHORT)WinSendMsg(hwndMenu,
                          MM_ITEMPOSITIONFROMID,
                          MPFROM2SHORT(usItem, TRUE),
                          0)
            != MIT_NONE)
    {
        // item exists:
        if ((USHORT)WinSendMsg(hwndMenu,
                               MM_QUERYITEMATTR,
                               MPFROM2SHORT(usItem, TRUE),
                               (MPARAM)MIA_DISABLED)
                    == 0)
            brc = TRUE;
    }

    return (brc);
}

/*
 *@@ HackContextMenu:
 *
 *@@added V0.9.7 (2001-01-10) [umoeller]
 */

VOID HackContextMenu(PWINLISTPRIVATE pPrivate)
{
    PSZ apszMenuItems[] =
            {
                "~Restore",     // 1000
                "~Move",        // 1001
                "~Size",        // 1002
                "Mi~nimize",    // 1003
                "Ma~ximize",    // 1004
                "~Hide",        // 1005
                "",             // separator
                "~Close",       // 1007
                "",             // separator
                "~Kill process ", // 1009
                ""              // separator
            };

    pPrivate->hwndContextMenuHacked = WinCreateMenu(HWND_DESKTOP, NULL);
    if (pPrivate->hwndContextMenuHacked)
    {
        // insert window-related menu items from above array
        MENUITEM mi = {0};
        SHORT src = 0;
        SHORT s = 0;
        mi.iPosition = MIT_END;
        mi.afStyle = MIS_TEXT;

        for (s = 0;
             s < sizeof(apszMenuItems) / sizeof(apszMenuItems[0]);
             s++)
        {
            mi.id = 1000 + s;
            if (apszMenuItems[s][0] == '\0')
                // separator:
                mi.afStyle = MIS_SEPARATOR;
            else
                mi.afStyle = MIS_TEXT;

            src = SHORT1FROMMR(WinSendMsg(pPrivate->hwndContextMenuHacked,
                                          MM_INSERTITEM,
                                          (MPARAM)&mi,
                                          (MPARAM)apszMenuItems[s]));
        }

        // now copy the old context menu as submenu;
        // we can't just use the old item and insert it
        // because we still display the original menu if
        // the window list is empty, and PMMERGE crashes then
        pwinhMergeIntoSubMenu(pPrivate->hwndContextMenuHacked,
                              MIT_END,
                              "Window list widget",
                              2000,
                              pPrivate->pWidget->hwndContextMenu);
    }
}

/*
 *@@ WwgtContextMenu:
 *      implementation for WM_CONTEXTMENU.
 *
 *      We override the default behavior of the standard
 *      widget window proc.
 *
 *@@added V0.9.8 (2001-01-10) [umoeller]
 */

MRESULT WwgtContextMenu(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    PXCENTERWIDGET pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER);
    if (pWidget)
    {
        PWINLISTPRIVATE pPrivate = (PWINLISTPRIVATE)pWidget->pUser;
        if (pPrivate)
        {
            if (pWidget->hwndContextMenu)
            {
                POINTL  ptlScreen,
                        ptlWidget;
                RECTL   rclSubclient;
                PSWCNTRL pCtlUnderMouse = NULL;

                WinQueryPointerPos(HWND_DESKTOP, &ptlScreen);
                // convert to widget coords
                memcpy(&ptlWidget, &ptlScreen, sizeof(ptlWidget));
                WinMapWindowPoints(HWND_DESKTOP,        // from
                                   hwnd,                // to (widget)
                                   &ptlWidget,
                                   1);

                if (!pPrivate->hwndContextMenuHacked)
                {
                    // first call:
                    // hack the context menu given to us
                    HackContextMenu(pPrivate);
                } // if (!pPrivate->fContextMenuHacked)

                GetPaintableRect(pPrivate, &rclSubclient);
                pCtlUnderMouse = FindCtrlFromPoint(pPrivate,
                                                   &ptlWidget,
                                                   &rclSubclient);
                pPrivate->hwndContextMenuShowing = NULLHANDLE;
                if (pCtlUnderMouse)
                {
                    // draw source emphasis
                    // ULONG ulStyle;
                    HWND hwndSysMenu;
                    BOOL fEnableRestore = FALSE,
                         fEnableMove = FALSE,
                         fEnableSize = FALSE,
                         fEnableMinimize = FALSE,
                         fEnableMaximize = FALSE,
                         fEnableHide = FALSE,
                         fEnableKill = FALSE;
                    PPIB ppib;
                    PTIB ptib;
                    CHAR szKillText[60] = "~Kill process";

                    HPS hps = WinGetPS(hwnd);
                    if (hps)
                    {
                        pgpihSwitchToRGB(hps);
                        pPrivate->pCtrlSourceEmphasis = pCtlUnderMouse;
                        pPrivate->pCtrlMenu = pCtlUnderMouse;
                        DrawOneCtrl(pPrivate,
                                    hps,
                                    &rclSubclient,
                                    pCtlUnderMouse,
                                    WinQueryActiveWindow(HWND_DESKTOP),
                                    NULL);
                        WinReleasePS(hps);
                    }

                    // enable items...
                    // we do a mixture of copying the state from the item's
                    // system menu and from querying the window's style. This
                    // is kinda sick, but we have the following limitations:
                    // -- There is no easy cross-process way of getting the
                    //    frame control flags to find out whether the frame
                    //    actually has minimize and maximize buttons in the
                    //    first place.
                    // -- We can't enumerate the frame controls either because
                    //    the minimize and maximize button(s) are a single control
                    //    which always has the same ID (FID_MINMAX), and we don't
                    //    know what it looks like.
                    // -- In general, the system menu has the correct items
                    //    enabled (SC_MINIMIZE and SC_MAXIMIZE). Howver, these
                    //    items are only refreshed when the sysmenu is made
                    //    visible... so we have to query QWL_STYLE as well.
                    hwndSysMenu = WinWindowFromID(pCtlUnderMouse->hwnd, FID_SYSMENU);
                    if (hwndSysMenu)
                    {
                        ULONG ulStyle = WinQueryWindowULong(pCtlUnderMouse->hwnd,
                                                            QWL_STYLE);
                        if (    (ulStyle & (WS_MINIMIZED | WS_MAXIMIZED))
                             || ((ulStyle & WS_VISIBLE) == 0)
                           )
                            fEnableRestore = TRUE;

                        fEnableMove     = IsEnabled(hwndSysMenu, SC_MOVE    );
                        fEnableSize     = IsEnabled(hwndSysMenu, SC_SIZE    );

                        if ((ulStyle & WS_MINIMIZED) == 0)
                            fEnableMinimize = IsEnabled(hwndSysMenu, SC_MINIMIZE);
                        if ((ulStyle & WS_MAXIMIZED) == 0)
                            fEnableMaximize = IsEnabled(hwndSysMenu, SC_MAXIMIZE);

                        if ((ulStyle & WS_VISIBLE))
                            fEnableHide     = IsEnabled(hwndSysMenu, SC_HIDE);
                    }

                    WinEnableMenuItem(pPrivate->hwndContextMenuHacked,
                                      1000,     // restore
                                      fEnableRestore);
                    WinEnableMenuItem(pPrivate->hwndContextMenuHacked,
                                      1001,     // move
                                      fEnableMove);
                    WinEnableMenuItem(pPrivate->hwndContextMenuHacked,
                                      1002,     // size
                                      fEnableSize);
                    WinEnableMenuItem(pPrivate->hwndContextMenuHacked,
                                      1003,     // minimize
                                      fEnableMinimize);
                    WinEnableMenuItem(pPrivate->hwndContextMenuHacked,
                                      1004,     // maximize
                                      fEnableMaximize);
                    WinEnableMenuItem(pPrivate->hwndContextMenuHacked,
                                      1005,     // hide
                                      fEnableHide);

                    // enable "kill process" only if this is not the WPS
                    if (pCtlUnderMouse->idProcess != pdoshMyPID())
                    {
                        // not WPS:
                        fEnableKill = TRUE;
                        sprintf(szKillText,
                                "~Kill process (PID 0x%lX)",
                                pCtlUnderMouse->idProcess);
                    }

                    WinSetMenuItemText(pPrivate->hwndContextMenuHacked,
                                       1009,
                                       szKillText);
                    WinEnableMenuItem(pPrivate->hwndContextMenuHacked,
                                      1009,
                                      fEnableKill);

                    if (WinPopupMenu(HWND_DESKTOP,
                                     hwnd,
                                     pPrivate->hwndContextMenuHacked,
                                     ptlScreen.x,
                                     ptlScreen.y,
                                     0,
                                     PU_HCONSTRAIN | PU_VCONSTRAIN | PU_MOUSEBUTTON1
                                        | PU_MOUSEBUTTON2 | PU_KEYBOARD))
                        pPrivate->hwndContextMenuShowing = pPrivate->hwndContextMenuHacked;


                    mrc = (MPARAM)TRUE;
                }
                else
                    // no control under mouse:
                    // call default
                    mrc = pWidget->pfnwpDefWidgetProc(hwnd, WM_CONTEXTMENU, mp1, mp2);

            } // if (pWidget->hwndContextMenu)
        }
    }

    return (mrc);
}

/*
 *@@ WwgtMenuEnd:
 *      implementation for WM_MENUEND.
 *
 *@@added V0.9.7 (2001-01-10) [umoeller]
 */

VOID WwgtMenuEnd(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    PXCENTERWIDGET pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER);
    if (pWidget)
    {
        BOOL fCallDefault = TRUE;
        PWINLISTPRIVATE pPrivate = (PWINLISTPRIVATE)pWidget->pUser;
        if (pPrivate)
        {
            if (pPrivate->hwndContextMenuShowing)
                // we're showing our private context menu:
                if (pPrivate->hwndContextMenuShowing == (HWND)mp2)
                {
                    // this menu is ending:
                    fCallDefault = FALSE;
                    pPrivate->hwndContextMenuShowing = NULLHANDLE;
                    pPrivate->pCtrlSourceEmphasis = NULL;
                    // remove source emphasis... the win list might have
                    // changed in between, so just repaint all
                    WinInvalidateRect(hwnd, NULL, FALSE);
                }
        }

        if (fCallDefault)
            pWidget->pfnwpDefWidgetProc(hwnd, WM_MENUEND, mp1, mp2);
    }
}

/*
 *@@ WwgtCommand:
 *
 *@@added V0.9.7 (2001-01-10) [umoeller]
 */

VOID WwgtCommand(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    PXCENTERWIDGET pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER);
    if (pWidget)
    {
        BOOL fCallDefault = TRUE;
        PWINLISTPRIVATE pPrivate = (PWINLISTPRIVATE)pWidget->pUser;
        if (pPrivate)
        {
            USHORT usCmd = (USHORT)mp1;
            if (    (usCmd >= 1000)
                 && (usCmd <= 1009)
               )
            {
                fCallDefault = FALSE;

                if (pPrivate->pCtrlMenu)
                {
                    USHORT usSysCommand = 0;

                    switch (usCmd)
                    {
                            /*
                                "~Restore",     // 1000
                                "~Move",        // 1001
                                "~Size",        // 1002
                                "Mi~nimize",    // 1003
                                "Ma~ximize",    // 1004
                                "Hide"          // 1005
                                "~Close",       // 1007
                            */

                        case 1000:
                            usSysCommand = SC_RESTORE;
                        break;

                        case 1001:
                            usSysCommand = SC_MOVE;
                        break;

                        case 1002:
                            usSysCommand = SC_SIZE;
                        break;

                        case 1003:
                            usSysCommand = SC_MINIMIZE;
                        break;

                        case 1004:
                            usSysCommand = SC_MAXIMIZE;
                        break;

                        case 1005:
                            usSysCommand = SC_HIDE;
                        break;

                        case 1007:
                            usSysCommand = SC_CLOSE;
                        break;

                        case 1009:
                            DosKillProcess(DKP_PROCESS,
                                           pPrivate->pCtrlMenu->idProcess);
                        break;
                    }

                    if (usSysCommand)
                        WinPostMsg(pPrivate->pCtrlMenu->hwnd,
                                   WM_SYSCOMMAND,
                                   (MPARAM)usSysCommand,
                                   MPFROM2SHORT(CMDSRC_OTHER,
                                                TRUE));      // mouse
                }
            }
        }

        if (fCallDefault)
            pWidget->pfnwpDefWidgetProc(hwnd, WM_COMMAND, mp1, mp2);
    }
}

/*
 *@@ WwgtPresParamChanged:
 *      implementation for WM_PRESPARAMCHANGED.
 *
 */

VOID WwgtPresParamChanged(HWND hwnd,
                          ULONG ulAttrChanged)
{
    PXCENTERWIDGET pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER);
    if (pWidget)
    {
        PWINLISTPRIVATE pPrivate = (PWINLISTPRIVATE)pWidget->pUser;
        if (pPrivate)
        {
            BOOL fInvalidate = TRUE;
            switch (ulAttrChanged)
            {
                case 0:     // layout palette thing dropped
                case PP_BACKGROUNDCOLOR:
                case PP_FOREGROUNDCOLOR:
                    pPrivate->Setup.lcolBackground
                        = pwinhQueryPresColor(hwnd,
                                              PP_BACKGROUNDCOLOR,
                                              FALSE,
                                              SYSCLR_DIALOGBACKGROUND);
                    pPrivate->Setup.lcolForeground
                        = pwinhQueryPresColor(hwnd,
                                              PP_FOREGROUNDCOLOR,
                                              FALSE,
                                              SYSCLR_WINDOWSTATICTEXT);
                break;

                case PP_FONTNAMESIZE:
                {
                    PSZ pszFont = 0;
                    if (pPrivate->Setup.pszFont)
                    {
                        free(pPrivate->Setup.pszFont);
                        pPrivate->Setup.pszFont = NULL;
                    }

                    pszFont = pwinhQueryWindowFont(hwnd);
                    if (pszFont)
                    {
                        // we must use local malloc() for the font
                        pPrivate->Setup.pszFont = strdup(pszFont);
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
                              &pPrivate->Setup);
                if (strSetup.ulLength)
                    WinSendMsg(pPrivate->pWidget->pGlobals->hwndClient,
                               XCM_SAVESETUP,
                               (MPARAM)hwnd,
                               (MPARAM)strSetup.psz);
                pxstrClear(&strSetup);
            }
        } // end if (pPrivate)
    }
}

/*
 *@@ WwgtDestroy:
 *      implementation for WM_DESTROY.
 */

MRESULT WwgtDestroy(HWND hwnd)
{
    MRESULT mrc = 0;
    PXCENTERWIDGET pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER);
    if (pWidget)
    {
        PWINLISTPRIVATE pPrivate = (PWINLISTPRIVATE)pWidget->pUser;
        if (pPrivate)
        {
            WwgtClearSetup(&pPrivate->Setup);

            pxstrClear(&pPrivate->strTooltip);

            if (pPrivate->hwndContextMenuHacked)
                WinDestroyWindow(pPrivate->hwndContextMenuHacked);
            if (pPrivate->ulTimerID)
                ptmrStopXTimer(pWidget->pGlobals->pvXTimerSet,
                              hwnd,
                              pPrivate->ulTimerID);
            plstClear(&pPrivate->llSwitchEntries);      // auto-free
            free(pPrivate);
                    // pWidget is cleaned up by DestroyWidgets

        }

        mrc = pWidget->pfnwpDefWidgetProc(hwnd, WM_DESTROY, 0, 0);
    }

    return (mrc);
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
         *      WINLISTPRIVATE for our own stuff.
         */

        case WM_CREATE:
        {
            PXCENTERWIDGET pWidget = (PXCENTERWIDGET)mp1;
                    // this ptr is valid after WM_CREATE
            WinSetWindowPtr(hwnd, QWL_USER, mp1);
            if ((pWidget) && (pWidget->pfnwpDefWidgetProc))
                mrc = WwgtCreate(hwnd, pWidget);
            else
                // stop window creation!!
                mrc = (MPARAM)TRUE;
        break; }

        /*
         * WM_CONTROL:
         *      process notifications/queries from the XCenter.
         */

        case WM_CONTROL:
            mrc = (MPARAM)WwgtControl(hwnd, mp1, mp2);
        break;

        /*
         * WM_PAINT:
         *
         */

        case WM_PAINT:
            WwgtPaint(hwnd);
        break;

        /*
         * WM_TIMER:
         *      update timer --> check for updates.
         */

        case WM_TIMER:
            WwgtTimer(hwnd, mp1, mp2);
        break;

        /*
         * WM_BUTTON1DOWN:
         *      prevent the XCenter from getting the focus...
         */

        case WM_BUTTON1DOWN:
            WwgtButton1Down(hwnd, mp1);
            mrc = (MPARAM)FALSE;
        break;

        /*
         * WM_CONTEXTMENU:
         *      modify standard context menu behavior.
         */

        case WM_CONTEXTMENU:
            mrc = WwgtContextMenu(hwnd, mp1, mp2);
        break;

        /*
         * WM_MENUEND:
         *
         */

        case WM_MENUEND:
            WwgtMenuEnd(hwnd, mp1, mp2);
        break;

        /*
         * WM_COMMAND:
         *      process menu commands.
         */

        case WM_COMMAND:
            WwgtCommand(hwnd, mp1, mp2);
        break;

        /*
         * WM_PRESPARAMCHANGED:
         *
         */

        case WM_PRESPARAMCHANGED:
            // this gets sent before this is set!
            WwgtPresParamChanged(hwnd, (ULONG)mp1);
        break;

        /*
         * WM_DESTROY:
         *      clean up. This _must_ be passed on to
         *      ctrDefWidgetProc.
         */

        case WM_DESTROY:
            mrc = WwgtDestroy(hwnd);
        break;

        default:
        {
            PXCENTERWIDGET pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER);
            if (pWidget)
                mrc = pWidget->pfnwpDefWidgetProc(hwnd, msg, mp1, mp2);
        break; }
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
 +          CS_PARENTCLIP | CS_SIZEREDRAW | CS_SYNCPAINT
 *
 *      Your widget window _will_ be resized, even if you're
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
 *      There will ever be only one load occurence of the DLL.
 *      The XCenter manages sharing the DLL between several
 *      XCenters. As a result, it doesn't matter if the DLL
 *      has INITINSTANCE etc. set or not.
 *
 *      If this returns 0, this is considered an error, and the
 *      DLL will be unloaded again immediately.
 *
 *      If this returns any value > 0, *ppaClasses must be
 *      set to a static array (best placed in the DLL's
 *      global data) of XCENTERWIDGETCLASS structures,
 *      which must have as many entries as the return value.
 */

ULONG EXPENTRY WwgtInitModule(HAB hab,         // XCenter's anchor block
                              HMODULE hmodPlugin, // module handle of the widget DLL
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
                              CS_HITTEST | CS_PARENTCLIP | CS_SIZEREDRAW | CS_SYNCPAINT,
                              sizeof(PWINLISTPRIVATE))
                                    // extra memory to reserve for QWL_USER
                             )
            strcpy(pszErrorMsg, "WinRegisterClass failed.");
        else
        {
            // no error:
            // return classes
            *ppaClasses = G_WidgetClasses;

            // one class in this DLL:
            // no. of classes in this DLL:
            ulrc = sizeof(G_WidgetClasses) / sizeof(G_WidgetClasses[0]);
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

/*
 *@@ WwgtQueryVersion:
 *      this new export with ordinal 3 can return the
 *      XWorkplace version number which is required
 *      for this widget to run. For example, if this
 *      returns 0.9.10, this widget will not run on
 *      earlier XWorkplace versions.
 *
 *      NOTE: This export was mainly added because the
 *      prototype for the "Init" export was changed
 *      with V0.9.9. If this returns 0.9.9, it is
 *      assumed that the INIT export understands
 *      the new FNWGTINITMODULE_099 format (see center.h).
 *
 *@@added V0.9.9 (2001-02-06) [umoeller]
 */

VOID EXPENTRY WwgtQueryVersion(PULONG pulMajor,
                               PULONG pulMinor,
                               PULONG pulRevision)
{
    // report 0.9.11
    *pulMajor = 0;
    *pulMinor = 9;
    *pulRevision = 12;
}

