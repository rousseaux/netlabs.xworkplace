
/*
 *@@sourcefile classlst.c:
 *      implementation of the "WPS Class List" object, whose
 *      SOM code is in src\classes\xclslist.c.
 *
 *      Function prefix for this file:
 *      --  cll* (changed V0.9.1 (99-12-10))
 *
 *      This file implements the actual "WPS Class List"
 *      object (an instance of XWPClassList), which uses the
 *      first part internally. This consists mainly of window
 *      procedures which are specified when XWPClassList::wpOpen
 *      opens a new class list view. See fnwpClassListClient for
 *      details.
 *
 *      This uses the WPS classes functions in shared\classes.c
 *      intensively.
 *
 *@@header "classlst.h"
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
 *@@todo:
 *  somIsObj (somapi.h):
 *      Test whether <obj> is a valid SOM object. This test is based solely on
 *      the fact that (on this architecture) the first word of a SOM object is a
 *      pointer to its method table. The test is therefore most correctly understood
 *      as returning true if and only if <obj> is a pointer to a pointer to a
 *      valid SOM method table. If so, then methods can be invoked on <obj>.
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
#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WINPOINTERS
#define INCL_WINTIMER
#define INCL_WINMENUS
#define INCL_WINDIALOGS
#define INCL_WINSTATICS
#define INCL_WINBUTTONS
#define INCL_WINSTDCNR
#define INCL_WINMLE
#define INCL_WINSTDFILE
#include <os2.h>

// C library headers
#include <stdio.h>
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\comctl.h"             // common controls (window procs)
#include "helpers\cnrh.h"               // container helper routines
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\stringh.h"            // string helper routines
#include "helpers\winh.h"               // PM helper routines

// SOM headers which don't crash with prec. header files
#include "xclslist.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\classes.h"             // WPS class list helper functions
#include "shared\cnrsort.h"             // container sort comparison functions
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\notebook.h"            // generic XWorkplace notebook handling

#include "config\classlst.h"            // SOM logic for "WPS Classes" page

#pragma hdrstop                     // VAC++ keeps crashing otherwise
#include <wpclsmgr.h>               // this includes SOMClassMgr
#include <wpdesk.h>                 // WPDesktop

/* ******************************************************************
 *                                                                  *
 *   Declarations                                                   *
 *                                                                  *
 ********************************************************************/

// client window class name
#define WC_CLASSLISTCLIENT      "XWPClassListWindow"

#define ID_CLASSFRAME           3000

#define ID_SPLITMAIN            3001
#define ID_SPLITRIGHT           3002

PSZ pszClassInfo = NULL;

/*
 *@@ METHODRECORD:
 *      extended record structure for
 *      "method info" cnr window.
 */

typedef struct _METHODRECORD
{
    RECORDCORE          recc;
    // the method name is in RECORDCODE.pszIcon
    ULONG               ulMethodIndex;          // unique method index
    PSZ                 pszType;                // "S"=static, "D"=dynamic, "N"=nonstatic.
                                                // This points to a static string and must not be freed.
    PSZ                 pszIntroducedBy;        // name of class which introduced the method.
                                                // This is the return value of _somGetname and must not be freed.
    ULONG               ulIntroducedBy;         // inheritance distance (see SOMMETHOD.ulIntroducedBy, classlst.h)
    PSZ                 pszOverriddenBy;        // string of classes which override this method.
                                                // This is composed of the linked list in SOMMETHOD.
    ULONG               ulOverriddenBy;         // inheritance distance (see SOMMETHOD.ulOverriddenBy, classlst.h)
    PSZ                 pszMethodProc;          // method address; always points to szMethodProc buffer below
    CHAR                szMethodProc[20];
} METHODRECORD, *PMETHODRECORD;

#pragma pack(1)

/*
 *@@ CLIENTCTLDATA:
 *      client control data passed with WM_CREATE
 *      to fnwpClassListClient.
 */

typedef struct _CLIENTCTLDATA
{
    USHORT          cb;
    XWPClassList*   somSelf;
    ULONG           ulView;
} CLIENTCTLDATA, *PCLIENTCTLDATA;

#pragma pack()

/*
 *@@ CLASSLISTCLIENTDATA:
 *      this holds data for the class list client
 *      window (fnwpClassListClient, stored in its
 *      QWL_USER).
 *
 *      A pointer to this data is also stored in
 *      the various other structured for the subwindows.
 */

typedef struct _CLASSLISTCLIENTDATA
{
    XWPClassList        *somSelf;           // pointer to class list instance
    XWPClassListData    *somThis;           // instance data with more settings
    USEITEM             UseItem;            // use item
    VIEWITEM            ViewItem;           // view item
    HWND                hwndClient,
                        hwndSplitMain,      // "split windows" (comctl.c)
                        hwndSplitRight;

    // class list container
    HWND                hwndClassCnrDlg;    // left child of hwndSplitMain
    PSELECTCLASSDATA    pscd;               // WPS class data (classlist.h)
    ULONG               ulUpdateTimerID;    // timer for delayed window updated

    // class info dlg
    HWND                hwndClassInfoDlg;   // top right child of hwndSplitRight

    // method info dlg
    HWND                hwndMethodInfoDlg;  // bottom right child of hwndSplitRight
    LINKLIST            llCnrStrings;       // linked list of container strings which must be free()'d
    PMETHODINFO         pMethodInfo;        // method info for currently selected class (classlist.h)
} CLASSLISTCLIENTDATA, *PCLASSLISTCLIENTDATA;

/*
 *@@ CLASSLISTCNRDATA:
 *      this holds data for the class list container
 *      subwindow (fnwpClassCnrDlg, stored in its QWL_USER).
 */

typedef struct _CLASSLISTCNRDATA
{
    PCLASSLISTCLIENTDATA pClientData;
    XADJUSTCTRLS        xacClassCnr;        // for winhAdjustControls
} CLASSLISTCNRDATA, *PCLASSLISTCNRDATA;

/*
 *@@ CLASSLISTINFODATA:
 *      this holds data for the class info subwindow
 *      (fnwpClassInfoDlg, stored in its QWL_USER).
 */

typedef struct _CLASSLISTINFODATA
{
    PCLASSLISTCLIENTDATA pClientData;
    XADJUSTCTRLS        xacClassInfo;       // for winhAdjustControls
} CLASSLISTINFODATA, *PCLASSLISTINFODATA;

/*
 *@@ CLASSLISTMETHODDATA:
 *      this holds data for the method info subwindow
 *      (fnwpMethodInfoDlg, stored in its QWL_USER).
 */

typedef struct _CLASSLISTMETHODDATA
{
    PCLASSLISTCLIENTDATA pClientData;
    PMETHODRECORD       pMethodReccSource;  // current source record for context menu or NULL
    XADJUSTCTRLS        xacMethodInfo;      // for winhAdjustControls
} CLASSLISTMETHODDATA, *PCLASSLISTMETHODDATA;

/*
 *@@ ampClassCnrCtls:
 *      static array used with winhAdjustControls
 *      for the class list container subwindow
 *      (fnwpClassCnrDlg).
 */

MPARAM ampClassCnrCtls[] =
    {
        MPFROM2SHORT(ID_XLDI_CNR, XAC_SIZEX | XAC_SIZEY)
    };

/*
 *@@ ampClassInfoCtls:
 *      static array used with winhAdjustControls
 *      for the class info subwindow
 *      (fnwpClassInfoDlg).
 */

MPARAM ampClassInfoCtls[] =
    {
        MPFROM2SHORT(ID_XLDI_CLASSNAMETXT, XAC_MOVEY),
        MPFROM2SHORT(ID_XLDI_CLASSNAME, XAC_MOVEY | XAC_SIZEX),
        MPFROM2SHORT(ID_XLDI_REPLACEDBYTXT, XAC_MOVEY),
        MPFROM2SHORT(ID_XLDI_REPLACEDBY, XAC_MOVEY | XAC_SIZEX),
        MPFROM2SHORT(ID_XLDI_CLASSTITLETXT, XAC_MOVEY),
        MPFROM2SHORT(ID_XLDI_CLASSTITLE, XAC_MOVEY | XAC_SIZEX),
        MPFROM2SHORT(ID_XLDI_CLASSMODULETXT, XAC_MOVEY),
        MPFROM2SHORT(ID_XLDI_CLASSMODULE, XAC_MOVEY | XAC_SIZEX),
        MPFROM2SHORT(ID_XLDI_ICONTXT, XAC_MOVEY),
        MPFROM2SHORT(ID_XLDI_ICON, XAC_MOVEY),
        MPFROM2SHORT(ID_XLDI_TEXT2, XAC_SIZEX | XAC_SIZEY)
    };

/*
 *@@ ampMethodInfoCtls:
 *      static array used with winhAdjustControls
 *      for the method info subwindow.
 *      (fnwpMethodInfoDlg).
 */

MPARAM ampMethodInfoCtls[] =
    {
        MPFROM2SHORT(ID_XLDI_CNR, XAC_SIZEX | XAC_SIZEY),
        MPFROM2SHORT(ID_XLDI_RADIO_INSTANCEMETHODS, XAC_MOVEY),
        MPFROM2SHORT(ID_XLDI_RADIO_CLASSMETHODS, XAC_MOVEY)
    };

/* ******************************************************************
 *                                                                  *
 *   "Register new class" dlg                                       *
 *                                                                  *
 ********************************************************************/

/*
 * fnwpOpenFilter:
 *      just a dummy.
 */

MRESULT EXPENTRY fnwpOpenFilter(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
   return (WinDefFileDlgProc(hwnd, msg, mp1, mp2));
}

/*
 *@@ fnwpRegisterClass:
 *      dlg func for "Register Class" dialog; use with WinLoadDlg().
 */

MRESULT EXPENTRY fnwpRegisterClass(HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc;
    PREGISTERCLASSDATA prcd = (PREGISTERCLASSDATA)WinQueryWindowULong(hwndDlg, QWL_USER);

    switch(msg)
    {
        /*
         * WM_INITDLG:
         *      set up the container data and colors
         */

        case WM_INITDLG:
        {
            // CNRINFO CnrInfo;
            WinSetWindowULong(hwndDlg, QWL_USER, (ULONG)mp2);
            prcd = (PREGISTERCLASSDATA)mp2;
            if (prcd->ulHelpPanel == 0)
                winhShowDlgItem(hwndDlg, DID_HELP, FALSE);
            WinSendDlgItemMsg(hwndDlg, ID_XLDI_CLASSNAME, EM_SETTEXTLIMIT,
                              (MPARAM)(255-1), MPNULL);
            WinSendDlgItemMsg(hwndDlg, ID_XLDI_CLASSMODULE, EM_SETTEXTLIMIT,
                              (MPARAM)(255-1), MPNULL);
            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
        break; }

        case WM_CONTROL:
        {
                if ( (     (SHORT1FROMMP(mp1) == ID_XLDI_CLASSNAME)
                        || (SHORT1FROMMP(mp1) == ID_XLDI_CLASSMODULE)
                     )
                     && (SHORT2FROMMP(mp1) == EN_CHANGE)
                   )
                {
                    BOOL fEnable = FALSE;
                    WinQueryDlgItemText(hwndDlg, ID_XLDI_CLASSNAME,
                                        sizeof(prcd->szClassName), prcd->szClassName);
                    if (strlen(prcd->szClassName))
                    {
                        WinQueryDlgItemText(hwndDlg, ID_XLDI_CLASSMODULE,
                                            sizeof(prcd->szModName), prcd->szModName);
                        if (strlen(prcd->szClassName))
                            fEnable = TRUE;
                    }
                    winhEnableDlgItem(hwndDlg, DID_OK, fEnable);
                }

        break; }

        case WM_COMMAND:
        {
            switch (SHORT1FROMMP(mp1))
            {
                case DID_OK:
                {
                    if (prcd)
                    {
                        WinQueryDlgItemText(hwndDlg, ID_XLDI_CLASSNAME,
                                            sizeof(prcd->szClassName), prcd->szClassName);
                        WinQueryDlgItemText(hwndDlg, ID_XLDI_CLASSMODULE,
                                            sizeof(prcd->szModName), prcd->szModName);
                    }
                    mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
                break; }

                case ID_XLDI_BROWSE:
                {
                    FILEDLG fd;
                    memset(&fd, 0, sizeof(FILEDLG));
                    fd.cbSize = sizeof(FILEDLG);
                    fd.fl = FDS_OPEN_DIALOG | FDS_CENTER;
                    fd.pfnDlgProc = fnwpOpenFilter;
                    // fd.pszTitle = "Blah";
                    // fd.pszOKButton = "~OK";
                    strcpy(fd.szFullFile, "*.DLL");
                    // fd.pszIDrive = "F:"
                    // fd.ulFQFCount = 1;
                    if (    WinFileDlg(HWND_DESKTOP,    // parent
                                       hwndDlg,
                                       &fd)
                        && (fd.lReturn == DID_OK)
                       )
                    {
                        WinSetDlgItemText(hwndDlg, ID_XLDI_CLASSMODULE,
                                    fd.szFullFile);
                    }
                break; }

                default:
                    mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
            }
        break; }

        case WM_HELP:
        {
            if (prcd->ulHelpPanel)
            {
                _wpDisplayHelp(_wpclsQueryActiveDesktop(_WPDesktop),
                        prcd->ulHelpPanel,
                        (PSZ)prcd->pszHelpLibrary);
            }
        break; }

        default:
            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
    }
    return (mrc);
}

/* ******************************************************************
 *                                                                  *
 *   XWPClassList Helper functions                                  *
 *                                                                  *
 ********************************************************************/

/*
 *@@ ParseDescription:
 *      searches pszBuf for pszSrch0; if found, the line containing
 *      pszSrch is parsed as follows:
 +          #pszSrch0#  #ulFlags# #RestOfLine#
 +             PSZ       hex str     PSZ
 *
 *      Returns length of what was copied to pszDescription or
 *      zero on errors.
 *
 *      This is used for explaining a certain WPS class when it is
 *      being selected in the "WPS classes" notebook page.
 *      Returns TRUE if pszSrch0 was found.
 *
 *@@changed V0.9.0 [umoeller]: moved this func here from xfsys.c.
 *@@changed V0.9.0 [umoeller]: fixed small memory leak
 */

BOOL ParseDescription(PSZ pszBuf,           // in: complete descriptions text file
                      PSZ pszSrch0,         // in: class name (token) to search for
                      PULONG pulFlags,      // out: if found, flags for the class
                      PSZ pszDescription)   // out: if found, class's description
{
    BOOL brc = FALSE;

    // create search string for beginning of line
    PSZ pszSrch2 = malloc(strlen(pszSrch0) + 4);
    if (pszSrch2)
    {
        PSZ p1, p2;

        strcpy(pszSrch2, "\r\n");
        strcat(pszSrch2, pszSrch0);

        p1 = strstr(pszBuf, pszSrch2);
        if (p1)
        {
            p1 += 2; // skip \r\n
            p1 = strchr(p1, ' ');
            if (p1)
            {
                // p1 now has ulFlags
                sscanf(p1, "%lX ", pulFlags);
                p1 = strchr(p1+1, ' ');
                if (p1)
                {
                    brc = TRUE;
                    // p1 now has beginning of description
                    p2 = strstr(p1, "\r\n");
                    if (p2)
                    {
                        // p2 now has end of description
                        strncpy(pszDescription, p1+1, (p2-p1-1));
                    }
                    else
                        strncat(pszDescription, p1+1, 200);
                }
            }
        }
        free(pszSrch2);
    }
    return (brc);
}

/*
 *@@ RelinkWindows:
 *      this gets called to link the various split windows
 *      properly, depending on _fShowMethods.
 */

VOID RelinkWindows(PCLASSLISTCLIENTDATA pWindowData,
                   BOOL fReformat)
{
    XWPClassListData *somThis = pWindowData->somThis;

    // a) main split window:
    WinSendMsg(pWindowData->hwndSplitMain,
               SPLM_SETLINKS,
               // left window: class list container dlg
               (MPARAM)pWindowData->hwndClassCnrDlg,
               // right window:
               // depending on whether method info is to be shown:
               (MPARAM)( (_fShowMethods)
                         // right split window
                         ? pWindowData->hwndSplitRight
                         // or class info
                         : pWindowData->hwndClassInfoDlg
                       ));
    if (fReformat)
        ctlUpdateSplitWindow(pWindowData->hwndSplitMain);

    if (_fShowMethods)
    {
        // b) right split window
        WinSendMsg(pWindowData->hwndSplitRight,
                   SPLM_SETLINKS,
                   // bottom window: method info dlg
                   (MPARAM)pWindowData->hwndMethodInfoDlg,
                   // top window: class info dlg
                   (MPARAM)pWindowData->hwndClassInfoDlg);
        if (fReformat)
            ctlUpdateSplitWindow(pWindowData->hwndSplitRight);
    }

    WinShowWindow(pWindowData->hwndSplitRight, _fShowMethods);
    WinShowWindow(pWindowData->hwndMethodInfoDlg, _fShowMethods);
}

/*
 *@@ CleanupMethodsInfo:
 *      cleans up the methods container.
 */

VOID CleanupMethodsInfo(PCLASSLISTCLIENTDATA pWindowData)
{
    // clear methods container first; the container
    // uses the strings from METHODINFO, so
    // we better do this first
    WinSendMsg(WinWindowFromID(pWindowData->hwndMethodInfoDlg, ID_XLDI_CNR),
               CM_REMOVERECORD,
               NULL,
               MPFROM2SHORT(0, CMA_FREE | CMA_INVALIDATE));

    // did we allocate a method info before?
    if (pWindowData->pMethodInfo)
    {
        // yes: clean up
        clsFreeMethodInfo(&pWindowData->pMethodInfo);
    }

    // now clean up container strings
    lstClear(&pWindowData->llCnrStrings);
}

/*
 *@@ NewClassSelected:
 *      this is called every time a new class gets selected
 *      in the "WPS classes" page (CN_EMPHASIS in
 *      fnwpSettingsWpsClasses), after the update timer
 *      has elapsed in fnwpClassCnrDlg.
 *
 *      We will then update the class info display
 *      and the method information.
 *
 *@@changed V0.9.0 [umoeller]: moved this func here from xfsys.c; updated for new window hierarchy
 *@@changed V0.9.0 [umoeller]: added method information
 *@@changed V0.9.0 [umoeller]: now using a delay timer
 */

VOID NewClassSelected(PCLASSLISTCLIENTDATA pWindowData)
{
    PWPSLISTITEM    pwps = pWindowData->pscd->preccSelection->pwps;
    CHAR            szInfo[1000] = "",
                    szInfo2[256] = "";
    PNLSSTRINGS     pNLSStrings = cmnQueryNLSStrings();

    HWND            hwndMethodsCnr = WinWindowFromID(pWindowData->hwndMethodInfoDlg,
                                                     ID_XLDI_CNR);

    CleanupMethodsInfo(pWindowData);

    if ((pwps) && (pWindowData->hwndClassInfoDlg))
    {
        // pwps != NULL: valid class selected (and not the "Orphans" item)

        HPOINTER hClassIcon = NULLHANDLE;

        // first check if we have a WPS class,
        // because otherwise we better not invoke
        // WPS methods on it
        BOOL    fIsWPSClass = FALSE;
        if (pwps->pClassObject)
            fIsWPSClass = _somDescendedFrom(pwps->pClassObject, _WPObject);

        /*
         * class info:
         *
         */

        // dll name
        WinSetDlgItemText(pWindowData->hwndClassInfoDlg, ID_XLDI_CLASSMODULE, pwps->pszModName);

        // class name
        strcpy(szInfo, pwps->pszClassName);
        WinSetDlgItemText(pWindowData->hwndClassInfoDlg, ID_XLDI_CLASSNAME, szInfo);

        // "replaced with"
        WinSetDlgItemText(pWindowData->hwndClassInfoDlg, ID_XLDI_REPLACEDBY,
                (pwps->pszReplacedWithClasses)
                    ? pwps->pszReplacedWithClasses
                    : "");

        // class icon
        if (pwps->pClassObject)
            if (fIsWPSClass)
                hClassIcon = _wpclsQueryIcon(pwps->pClassObject);
        WinSendMsg(WinWindowFromID(pWindowData->hwndClassInfoDlg, ID_XLDI_ICON),
                   SM_SETHANDLE,
                   (MPARAM)hClassIcon,  // NULLHANDLE if error -> hide
                   MPNULL);

        // class title
        if (pwps->pClassObject)
        {
            PSZ pszClassTitle = NULL;
            if (fIsWPSClass)
                pszClassTitle = _wpclsQueryTitle(pwps->pClassObject);
            if (pszClassTitle)
                sprintf(szInfo2,
                    "\"%s\"",
                    pszClassTitle);
            else
                strcpy(szInfo2, "?");
        } else
            strcpy(szInfo2, pNLSStrings->pszWpsClassLoadingFailed);
        WinSetDlgItemText(pWindowData->hwndClassInfoDlg, ID_XLDI_CLASSTITLE, szInfo2);

        // class information
        if (pszClassInfo)
        {
            ULONG ulFlags;
            if (!ParseDescription(pszClassInfo,
                                  pwps->pszClassName,
                                  &ulFlags,
                                  szInfo))
            {
                // not found: search for "UnknownClass"
                ParseDescription(pszClassInfo,
                                 "UnknownClass",
                                 &ulFlags,
                                 szInfo);
            }
        }

        /*
         * method info:
         *
         */

        if (pwps->pClassObject)
        {
            // now update method info
            pWindowData->pMethodInfo = clsQueryMethodInfo(pwps->pClassObject,
                                                          // return-class-methods flag
                                                          (pWindowData->somThis->ulMethodsRadioB
                                                              == ID_XLDI_RADIO_CLASSMETHODS));
            if (pWindowData->pMethodInfo)
            {
                // allocate as many records as methods exist;
                // we get a linked list now
                PMETHODRECORD precc = (PMETHODRECORD)cnrhAllocRecords(
                                                    hwndMethodsCnr,
                                                    sizeof(METHODRECORD),
                                                    pWindowData->pMethodInfo->ulMethodCount),
                              preccThis = precc;
                PLISTNODE     pNode = lstQueryFirstNode(&pWindowData->pMethodInfo->llMethods);
                ULONG         ulIndex = 0;

                // now set up the METHODRECORD according
                // to the method data returned by clsQueryMethodInfo
                while ((preccThis) && (pNode))
                {
                    PSOMMETHOD psm = (PSOMMETHOD)pNode->pItemData;
                    PLISTNODE pnodeOverride;

                    // set up information
                    preccThis->ulMethodIndex = ulIndex;
                    preccThis->recc.pszIcon = psm->pszMethodName;

                    switch(psm->ulType)  // 0=static, 1=dynamic, 2=nonstatic
                    {
                        case 0: preccThis->pszType = "S"; break;
                        case 1: preccThis->pszType = "D"; break;
                        case 2: preccThis->pszType = "N"; break;
                        default: preccThis->pszType = "?"; break;
                    }

                    if (psm->pIntroducedBy)
                        preccThis->pszIntroducedBy = _somGetName(psm->pIntroducedBy);
                    preccThis->ulIntroducedBy = psm->ulIntroducedBy;

                    // go thru list of class objects which overrode this method
                    // and create a string from the class names
                    pnodeOverride = lstQueryFirstNode(&psm->llOverriddenBy);
                    while (pnodeOverride)
                    {
                        SOMClass *pClassObject = (SOMClass*)pnodeOverride->pItemData;

                        if (pClassObject)
                        {
                            if (preccThis->pszOverriddenBy)
                                // not first class: append comma
                                strhxcat(&preccThis->pszOverriddenBy, ", ");

                            strhxcat(&preccThis->pszOverriddenBy, _somGetName(pClassObject));
                        }

                        pnodeOverride = pnodeOverride->pNext;
                    }

                    if (preccThis->pszOverriddenBy)
                        // store this string in list of items to be freed later
                        lstAppendItem(&pWindowData->llCnrStrings,
                                      preccThis->pszOverriddenBy);

                    preccThis->ulOverriddenBy = psm->ulOverriddenBy;

                    // write method address to recc's string buffer
                    sprintf(preccThis->szMethodProc, "0x%lX",
                            psm->pMethodProc);
                    // and point PSZ to that buffer
                    preccThis->pszMethodProc = preccThis->szMethodProc;

                    // go for next
                    preccThis = (PMETHODRECORD)preccThis->recc.preccNextRecord;
                    pNode = pNode->pNext;
                    ulIndex++;
                }

                cnrhInsertRecords(hwndMethodsCnr,
                                  NULL,         // no parent record
                                  (PRECORDCORE)precc,        // first record
                                  NULL,         // no text
                                  CRA_RECORDREADONLY,
                                  pWindowData->pMethodInfo->ulMethodCount);
                                                // record count
            } // end if (pWindowData->pMethodInfo)
        } // end if (pwps->pClassObject)
    } // end if (pwps)
    else
    {
        // if (pwps == NULL), the "Orphans" item has been
        // selected: give info for this
        strcpy(szInfo, pNLSStrings->pszWpsClassOrphansInfo);
        WinSetDlgItemText(pWindowData->hwndClassInfoDlg, ID_XLDI_CLASSMODULE, "");
        WinSetDlgItemText(pWindowData->hwndClassInfoDlg, ID_XLDI_CLASSNAME, "");
        WinSetDlgItemText(pWindowData->hwndClassInfoDlg, ID_XLDI_REPLACEDBY, "");
        WinSetDlgItemText(pWindowData->hwndClassInfoDlg, ID_XLDI_CLASSTITLE, "");

        WinSendMsg(WinWindowFromID(pWindowData->hwndClassInfoDlg, ID_XLDI_ICON),
                   SM_SETHANDLE,
                   (MPARAM)NULLHANDLE,  // hide icon
                   MPNULL);
    }

    // give MLE new text
    WinSetDlgItemText(pWindowData->hwndClassInfoDlg, ID_XLDI_TEXT2, szInfo);
    // scroll MLE to top
    WinSendDlgItemMsg(pWindowData->hwndClassInfoDlg, ID_XLDI_TEXT2,
            MLM_SETFIRSTCHAR,
            (MPARAM)0,
            MPNULL);
}

/*
 *@@ StartMethodsUpdateTimer:
 *      this starts a timer upon the class cnr dlg
 *      with ID 2. When WM_TIMER comes into that window,
 *      the methods info will be re-retrieved.
 */

VOID StartMethodsUpdateTimer(PCLASSLISTCLIENTDATA pWindowData)
{
    HAB     habDlg = WinQueryAnchorBlock(pWindowData->hwndClassCnrDlg);
    // start one-shot timer to update other
    // dlgs delayed
    if (pWindowData->ulUpdateTimerID != 0)
    {
        // timer already running:
        // restart
        WinStopTimer(habDlg, pWindowData->hwndClassCnrDlg,
                     2);
    }

    // (re)start timer
    pWindowData->ulUpdateTimerID
            = WinStartTimer(habDlg,
                            pWindowData->hwndClassCnrDlg,
                            2,          // timer ID
                            100);       // ms
}

/*
 *@@ fncbReturnWPSClassAttr:
 *      this callback function is called from clsWpsClasses2Cnr
 *      for every single record core which represents a WPS class;
 *      we need to return the record core attributes.
 *
 *      For classes which have been replaced, we set the CRA_DISABLED
 *      attribute.
 *
 *@@changed V0.9.0 [umoeller]: moved this func here from xfsys.c.
 *@@changed V0.9.0 [umoeller]: added CRA_DISABLED
 *@@changed V0.9.0 [umoeller]: changed initially expanded classes
 */

MRESULT EXPENTRY fncbReturnWPSClassAttr(HWND hwndCnr,
                                        ULONG ulscd,   // SELECTCLASSDATA struct
                                        MPARAM mpwps,  // current WPSLISTITEM struct
                                        MPARAM mpreccParent) // parent record core
{
    PWPSLISTITEM        pwps = (PWPSLISTITEM)mpwps;
    // PSELECTCLASSDATA    pscd = (PSELECTCLASSDATA)ulscd;
    PRECORDCORE         preccParent = NULL;

    USHORT              usAttr = CRA_RECORDREADONLY
                                    | CRA_COLLAPSED;

    if (pwps)
    {
        #ifdef DEBUG_SETTINGS
            _Pmpf(("Checking %s\n", pwps->pszClassName));
        #endif

        // if the class is one of the following,
        // expand all the parent records of the new record
        // so that these classes are initially visible
        if (    (strcmp(pwps->pszClassName, "WPAbstract") == 0)
             || (strcmp(pwps->pszClassName, "WPDataFile") == 0) // V0.9.0

             || (strcmp(pwps->pszClassName, "WPFolder") == 0)   // V0.9.0
           )
        {
            preccParent = (PRECORDCORE)mpreccParent;
            while (preccParent)
            {
                WinSendMsg(hwndCnr, CM_EXPANDTREE, (MPARAM)preccParent, MPNULL);

                preccParent = WinSendMsg(hwndCnr, CM_QUERYRECORD,
                        preccParent,
                        MPFROM2SHORT(CMA_PARENT, CMA_ITEMORDER));

                if (preccParent == (PRECORDCORE)-1)
                    preccParent = NULL;
            }
        }

        // class replaced:
        if (pwps->pszReplacedWithClasses)
            usAttr |= CRA_DISABLED;
    }
    return (MPARAM)(usAttr);
}

/*
 *@@ fncbReplaceClassSelected:
 *      callback func for class selected in the "Replace
 *      with subclass" dlg;
 *      mphwndInfo has been set to the static control hwnd.
 *      Returns TRUE if the selection is valid; the dlg func
 *      will then enable the OK button.
 *
 *@@changed V0.9.0 [umoeller]: moved this func here from xfsys.c.
 */

MRESULT EXPENTRY fncbReplaceClassSelected(HWND hwndCnr,
                                          ULONG ulpsbsc,
                                          MPARAM mpwps,
                                          MPARAM mphwndInfo)
{
    PWPSLISTITEM pwps = (PWPSLISTITEM)mpwps;
    // PSZ pszClassTitle;
    CHAR szInfo[2000] = "";

    if (pwps->pClassObject)
    {
        if (pszClassInfo)
        {
            ULONG ulFlags;
            if (!ParseDescription(pszClassInfo,
                        pwps->pszClassName,
                        &ulFlags,
                        szInfo))
            {
                // not found: search for "UnknownClass"
                ParseDescription(pszClassInfo,
                        "UnknownClass",
                        &ulFlags,
                        szInfo);
            }
        }
    }

    WinSetWindowText((HWND)mphwndInfo, szInfo);

    return (MRESULT)TRUE;
}

/*
 *@@ fnCompareMethodIndex:
 *      container sort function for sorting by
 *      method index.
 *
 *      Note: the information in the PM reference is flat out wrong.
 *      Container sort functions need to return the following:
 +          0   pmrc1 == pmrc2
 +         -1   pmrc1 <  pmrc2
 +         +1   pmrc1 >  pmrc2
 */

SHORT EXPENTRY fnCompareMethodIndex(PMETHODRECORD precc1,
                                    PMETHODRECORD precc2,
                                    PVOID pStorage)
{
    SHORT src = 0;
    if ((precc1) && (precc2))
    {
        if (precc1->ulMethodIndex < precc2->ulMethodIndex)
            src = -1;
        else if (precc1->ulMethodIndex > precc2->ulMethodIndex)
            src = 1;
        // else equal: return 0
    }
    return (src);
}

/*
 *@@ fnCompareMethodName:
 *      container sort function for sorting by
 *      method name.
 */

SHORT EXPENTRY fnCompareMethodName(PMETHODRECORD precc1,
                                   PMETHODRECORD precc2,
                                   PVOID pStorage)
{
    SHORT src = 0;
    if ((precc1) && (precc2))
    {
        int i  = strcmp(precc1->recc.pszIcon, precc2->recc.pszIcon);
        if (i < 0)
            src = -1;
        else if (i > 0)
            src = 1;
        // else equal: return 0
    }
    return (src);
}

/*
 *@@ fnCompareMethodIntro:
 *      container sort function for sorting by
 *      the class which introduced a method.
 */

SHORT EXPENTRY fnCompareMethodIntro(PMETHODRECORD precc1,
                                    PMETHODRECORD precc2,
                                    PVOID pStorage)
{
    SHORT src = 0;
    if ((precc1) && (precc2))
    {
        if (precc1->ulIntroducedBy < precc2->ulIntroducedBy)
            src = -1;            // put newest methods on top (reverse)
        else if (precc1->ulIntroducedBy > precc2->ulIntroducedBy)
            src = 1;
        // else equal: return 0
    }
    return (src);
}

/*
 *@@ fnCompareMethodOverride:
 *      container sort function for sorting by
 *      the class which overrode a method.
 */

SHORT EXPENTRY fnCompareMethodOverride(PMETHODRECORD precc1,
                                       PMETHODRECORD precc2,
                                       PVOID pStorage)
{
    SHORT src = 0;
    if ((precc1) && (precc2))
    {
        if (precc1->ulOverriddenBy < precc2->ulOverriddenBy)
            src = -1;            // put newest methods on top (reverse)
        else if (precc1->ulOverriddenBy > precc2->ulOverriddenBy)
            src = 1;
        else
            // equal: compare method intro
            src = fnCompareMethodIntro(precc1, precc2, pStorage);
    }
    return (src);
}

/*
 *@@ QueryMethodsSortFunc:
 *      returns the method container's sort function
 *      according to the current instance data.
 */

PFNCNRSORT QueryMethodsSortFunc(PCLASSLISTCLIENTDATA pWindowData)
{
    PFNCNRSORT  pfnCnrSort = NULL;

    switch (pWindowData->somThis->ulSortID)
    {
        // "Sort by" commands
        case ID_XLMI_METHOD_SORT_INDEX:
            pfnCnrSort = (PFNCNRSORT)fnCompareMethodIndex;
        break;

        case ID_XLMI_METHOD_SORT_NAME:
            pfnCnrSort = (PFNCNRSORT)fnCompareMethodName;
        break;

        case ID_XLMI_METHOD_SORT_INTRO:
            pfnCnrSort = (PFNCNRSORT)fnCompareMethodIntro;
        break;

        case ID_XLMI_METHOD_SORT_OVERRIDE:
            pfnCnrSort = (PFNCNRSORT)fnCompareMethodOverride;
        break;
    }

    return (pfnCnrSort);
}

/* ******************************************************************
 *                                                                  *
 *   Class list window procedures                                   *
 *                                                                  *
 ********************************************************************/

MRESULT EXPENTRY fnwpClassCnrDlg(HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2);
MRESULT EXPENTRY fnwpClassInfoDlg(HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2);
MRESULT EXPENTRY fnwpMethodInfoDlg(HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2);

/*
 *  Here come the window procedures for the various
 *  class list windows: the client, and the two
 *  subdialogs.
 */

/*
 *@@ fnwpClassListClient:
 *      this is the window proc for the client window of
 *      a "class list" view of the XWPClassList object.
 *
 *      Upon WM_CREATE, this function creates all the
 *      class list subwindows, as listed below, and
 *      adds the frame to the class list object's
 *      in-use list. The view is removed again upon
 *      WM_CLOSE.
 *
 *      The main client window (with this window procedure)
 *      is created from cllCreateClassListView and gets passed
 *      a CLIENTCTLDATA structure with WM_CREATE, upon which
 *      we'll create all the subwindows here.
 *
 *      The window hierarchy is as follows (lines signify
 *      parentship):
 *
 +      ID_CLASSFRAME (standard WC_FRAME window)
 +        |
 +        +-- ... default frame controls
 +        |
 +        +-- FID_CLIENT   (this window proc)
 +              |
 +              +-- ID_SPLITMAIN (vertical split, fnwpSplitWindow, comctl.c)
 +                    |
 +                    +-- ID_XLD_CLASSLIST  (fnwpClassCnrDlg, loaded from resources)
 +                    |      |
 +                    |      +-- container with actual class list
 +                    |
 +                    +-- ID_SPLITBAR     (fnwpSplitBar, comctl.c)
 +                    |
 +                    +-- ID_SPLITRIGHT (horizontal split on the right, fnwpSplitWindow)
 +                           |
 +                           +-- ID_XLD_CLASSINFO (fnwpClassInfoDlg, loaded from resources)
 +                           |      |
 +                           |      +-- ... all the class info subwindows
 +                           |
 +                           +-- ID_SPLITBAR     (fnwpSplitBar, comctl.c)
 +                           |
 +                           +-- ID_XLD_METHODINFO (fnwpMethodInfoDlg, loaded from resources)
 +                                  |
 +                                  +-- ... all the method info subwindows
 *
 *      Visually, this appears like this:
 *
 +      +-------------------------+-----------------------------+
 +      |                         |                             |
 +      |                         |  ID_XLD_CLASSINFO           |
 +      |                         |  (fnwpClassInfoDlg,         |
 +      |                         |   loaded from NLS resrcs)   |
 +      |                         |                             |
 +      |  ID_XLD_CLASSLIST       +-----------------------------+
 +      |  (fnwpClassCnrDlg,      |                             |
 +      |   loaded from NLS       |  ID_XLD_METHODINFO          |
 +      |   resources)            |  (fnwpMethodInfoDlg,        |
 +      |                         |   loaded from NLS rescrs)   |
 +      |                         |                             |
 +      +-------------------------+-----------------------------+
 *
 *      Note that to keep all the different data apart, we create a
 *      slightly complex system of data structures with each of the
 *      frame windows which are created here:
 *
 *      --  The client window (fnwpClassListClient) creates a
 *          CLASSLISTCLIENTDATA structure in its QWL_USER
 *          window word.
 *
 *      --  The class list container dialog (fnwpClassCnrDlg)
 *          gets the client's CLASSLISTCLIENTDATA with WM_INITDLG
 *          and creates a CLASSLISTCNRDATA in its own QWL_USER
 *          window word.
 *
 *      --  The class info dialog (fnwpClassInfoDlg)
 *          gets the client's CLASSLISTCLIENTDATA with WM_INITDLG
 *          and creates a CLASSLISTINFODATA in its own QWL_USER
 *          window word.
 *
 *      --  The method info dialog (fnwpMethodInfoDlg)
 *          gets the client's CLASSLISTCLIENTDATA with WM_INITDLG
 *          and creates a CLASSLISTMETHODDATA in its own QWL_USER
 *          window word.
 *
 *      When the main window (the frame of the view) is resized,
 *      fnwpClassListClient gets a WM_WINDOWPOSCHANGED message,
 *      as usual. We then call WinSetWindowPos on the main split
 *      window (ID_SPLITMAIN), whose window proc (fnwpSplitWindow,
 *      comctl.c) will then automatically resize all the subwindows.
 *
 *      When any of the three subdialogs receive WM_WINDOWPOSCHANGED
 *      in turn, they'll use winhAdjustControls to update their
 *      controls' positions and sizes.
 *
 *@@added V0.9.0 [umoeller]
 */

MRESULT EXPENTRY fnwpClassListClient(HWND hwndClient, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = (MRESULT)0;
    PCLASSLISTCLIENTDATA pWindowData
            = (PCLASSLISTCLIENTDATA)WinQueryWindowPtr(hwndClient, QWL_USER);

    switch(msg)
    {
        /*
         * WM_CREATE:
         *
         */

        case WM_CREATE:
        {
            // frame window successfully created:
            RECTL           rcl;
            SPLITBARCDATA   sbcd;
            PNLSSTRINGS     pNLSStrings = cmnQueryNLSStrings();
            PCLIENTCTLDATA  pCData = (PCLIENTCTLDATA)mp1;
            HWND            hwndFrame = WinQueryWindow(hwndClient, QW_PARENT);
            HAB             hab = WinQueryAnchorBlock(hwndClient);
            // now add the view to the object's use list;
            // this use list is used by wpViewObject and
            // wpClose to check for existing open views.

            // get storage for and initialize a use list item
            pWindowData = (PCLASSLISTCLIENTDATA)_wpAllocMem(pCData->somSelf,
                                                            sizeof(CLASSLISTCLIENTDATA),
                                                            NULL);
            memset((PVOID)pWindowData, 0, sizeof(CLASSLISTCLIENTDATA));
            pWindowData->somSelf         = pCData->somSelf;
            pWindowData->UseItem.type    = USAGE_OPENVIEW;
            pWindowData->ViewItem.view   = pCData->ulView;
            pWindowData->ViewItem.handle = hwndFrame;

            // add the use list item to the object's use list
            _wpAddToObjUseList(pCData->somSelf, &(pWindowData->UseItem));
            _wpRegisterView(pCData->somSelf, hwndFrame, pNLSStrings->pszOpenClassList);

            // initialize list of cnr items to be freed later
            lstInit(&pWindowData->llCnrStrings, TRUE);

            // save the pointer to the use item in the window words so that
            // the window procedure can remove it from the list when the window
            // is closed
            WinSetWindowPtr(hwndClient, QWL_USER, pWindowData);

            // hab = WinQueryAnchorBlock(hwndFrame);

            // store instance data
            pWindowData->somThis = XWPClassListGetData(pCData->somSelf);

            // store client HWND in window data too
            pWindowData->hwndClient = hwndClient;

            /*
             * "Class info" dlg subwindow (top right):
             *
             */

            // this will be linked to the right split window later,
            // which will in turn be linked to the main split window.
            pWindowData->hwndClassInfoDlg = WinLoadDlg(hwndClient, hwndClient,
                                                         // parent and owner;
                                                         // the parent will be changed
                                                         // by ctlCreateSplitWindow
                                                       fnwpClassInfoDlg,
                                                       cmnQueryNLSModuleHandle(FALSE),
                                                       ID_XLD_CLASSINFO,
                                                       pWindowData);     // create param
            WinSetWindowBits(pWindowData->hwndClassInfoDlg,
                             QWL_STYLE,
                             WS_CLIPCHILDREN,         // set bit
                             WS_CLIPCHILDREN);

            /*
             * "Method info" dlg subwindow (bottom right):
             *
             */

            pWindowData->hwndMethodInfoDlg = WinLoadDlg(hwndClient, hwndClient,
                                                            // parent and owner;
                                                            // the parent will be changed
                                                            // by ctlCreateSplitWindow
                                                        fnwpMethodInfoDlg,
                                                        cmnQueryNLSModuleHandle(FALSE), ID_XLD_METHODINFO,
                                                        pWindowData);     // create param
            WinSetWindowBits(pWindowData->hwndMethodInfoDlg,
                             QWL_STYLE,
                             WS_CLIPCHILDREN,         // set bit
                             WS_CLIPCHILDREN);

            /*
             * container dlg subwindow (left half of window);:
             *
             */

            // this will be linked to the split window later.
            // This is a sizeable dialog defined in the NLS resources.
            pWindowData->hwndClassCnrDlg = WinLoadDlg(hwndClient, hwndClient,
                                    // parent and owner;
                                    // the parent will be changed by ctlCreateSplitWindow
                              fnwpClassCnrDlg,
                              cmnQueryNLSModuleHandle(FALSE), ID_XLD_CLASSLIST,
                              pWindowData);     // create param
            WinSetWindowBits(pWindowData->hwndClassCnrDlg,
                             QWL_STYLE,
                             WS_CLIPCHILDREN,         // set bit
                             WS_CLIPCHILDREN);

            // create main split window (vertical split bar)
            memset(&sbcd, 0, sizeof(SPLITBARCDATA));
            sbcd.ulCreateFlags = SBCF_VERTICAL          // vertical split bar
                                    | SBCF_PERCENTAGE   // lPos has split bar pos
                                                        // in percent of the client
                                    | SBCF_3DSUNK       // draw 3D "sunk" frame
                                    | SBCF_MOVEABLE;    // moveable split bar
            sbcd.lPos = 50;
            sbcd.hwndParentAndOwner = hwndClient;
            sbcd.ulLeftOrBottomLimit = 100;
            sbcd.ulRightOrTopLimit = 100;
            sbcd.ulSplitWindowID = ID_SPLITMAIN;
            pWindowData->hwndSplitMain = ctlCreateSplitWindow(hab,
                                                              &sbcd);

            // create right split window (horizontal split bar)
            memset(&sbcd, 0, sizeof(SPLITBARCDATA));
            sbcd.ulCreateFlags = SBCF_HORIZONTAL        // horizontal split bar
                                    | SBCF_PERCENTAGE   // lPos has split bar pos
                                                        // in percent of the client
                                    // | SBCF_3DSUNK       // we already have one
                                    | SBCF_MOVEABLE;    // moveable split bar
            sbcd.lPos = 50;
            sbcd.hwndParentAndOwner = pWindowData->hwndSplitMain;
            sbcd.ulLeftOrBottomLimit = 100;
            sbcd.ulRightOrTopLimit = 100;
            sbcd.ulSplitWindowID = ID_SPLITRIGHT;
            pWindowData->hwndSplitRight = ctlCreateSplitWindow(hab,
                                                               &sbcd);



            // now set the "split links"
            RelinkWindows(pWindowData, FALSE);

            // and fill the container with the classes;
            // this is handled in fnwpClassCnrDlg
            WinPostMsg(pWindowData->hwndClassCnrDlg, WM_FILLCNR, MPNULL, MPNULL);

            // show help panel if opened for the first time
            /* if ((pGlobalSettings->ulIntroHelpShown & HLPS_CLASSLIST) == 0)
            {
                WinPostMsg(hwndClient, WM_HELP, 0, 0);
                pGlobalSettings->ulIntroHelpShown |= HLPS_CLASSLIST;
                cmnStoreGlobalSettings();
            } */

            mrc = (MPARAM)FALSE;
        break; }

        /*
         * WM_WINDOWPOSCHANGED:
         *
         */

        case WM_WINDOWPOSCHANGED:
        {
            // this msg is passed two SWP structs:                    WM_SIZE
            // one for the old, one for the new data
            // (from PM docs)
            PSWP pswpNew = PVOIDFROMMP(mp1);
            PSWP pswpOld = pswpNew + 1;

            // resizing?
            if (pswpNew->fl & SWP_SIZE)
            {
                if (pWindowData)
                    WinSetWindowPos(pWindowData->hwndSplitMain, HWND_TOP,
                                    0, 0,
                                    pswpNew->cx, pswpNew->cy, // sCXNew, sCYNew,
                                    SWP_SIZE);
            }

            // return default NULL
        break; }

        /*
         * WM_MINMAXFRAME:
         *      when minimizing, we hide the "split window",
         *      because otherwise the child dialogs will
         *      display garbage
         */

        case WM_MINMAXFRAME:
        {
            PSWP pswp = (PSWP)mp1;
            if (pswp->fl & SWP_MINIMIZE)
                WinShowWindow(pWindowData->hwndSplitMain, FALSE);
            else if (pswp->fl & SWP_RESTORE)
                WinShowWindow(pWindowData->hwndSplitMain, TRUE);
        break; }

        /*
         * WM_CLOSE:
         *      window list is being closed:
         *      store the window position
         */

        case WM_CLOSE:
        {
            HWND hwndFrame = WinQueryWindow(hwndClient, QW_PARENT);
            // get the object pointer and the use list item from the window

            // save window position
            winhSaveWindowPos(hwndFrame,
                              HINI_USER,
                              INIAPP_XFOLDER,
                              INIKEY_WNDPOSCLASSINFO);
            // destroy the window and return
            WinDestroyWindow(hwndFrame);

            // return default NULL
        break; }

        /*
         * WM_DESTROY:
         *      clean up.
         */

        case WM_DESTROY:
            if (pWindowData)
            {
                // remove this window from the object's use list
                _wpDeleteFromObjUseList(pWindowData->somSelf,
                                        &pWindowData->UseItem);
                // free the use list item
                _wpFreeMem(pWindowData->somSelf, (PBYTE)pWindowData);

                pWindowData->somThis->hwndOpenView = NULLHANDLE;
            }

            // return default NULL
        break;

        /*
         * WM_HELP:
         *
         */

        case WM_HELP:
            cmnDisplayHelp(pWindowData->somSelf,
                           ID_XSH_SETTINGS_WPSCLASSES);
        break;

        default:
           mrc = WinDefWindowProc(hwndClient, msg, mp1, mp2);
    }

    return (mrc);
}

BOOL fFillingCnr = FALSE;

/*
 *@@ fnwpClassCnrDlg:
 *      this is the window proc for the dialog window
 *      with the actual WPS class list container (in Tree view).
 *      This dlg is a child of the abstract "split window"
 *      and has in turn one child, the container window.
 *      See fnwpClassListClient for a window hierarchy.
 *
 *      This calls in clsWpsClasses2Cnr in classlst.c to
 *      have the WPS class tree inserted, which in turn
 *      calls the fncbReturnWPSClassAttr and fncbReplaceClassSelected
 *      callbacks above for configuration.
 *
 *      This is a sizeable dialog. Since dialogs do not
 *      receive WM_SIZE, we need to handle WM_WINDOWPOSCHANGED
 *      instead. This gets sent to us when the "split window"
 *      calls WinSetWindowPos when the frame window has been
 *      resized.
 *
 *      Also, we support container owner-draw here for the
 *      "disabled" (i.e. replaced) classes.
 *
 *@@added V0.9.0 [umoeller]
 */

MRESULT EXPENTRY fnwpClassCnrDlg(HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = MPNULL;

    PCLASSLISTCNRDATA pWindowData = (PCLASSLISTCNRDATA)WinQueryWindowULong(hwndDlg, QWL_USER);

    TRY_LOUD(excpt1, NULL)
    {
        switch(msg)
        {

            /*
             * WM_INITDLG:
             *      set up the container data and colors.
             *      mp2 (pCreateParam) points to the frame's
             *      CLASSLISTCLIENTDATA structure.
             */

            case WM_INITDLG:
            {
                // create a structure for the functions in
                // classlst.c, which we'll store in the client's
                // window words
                PSELECTCLASSDATA pscd = (PSELECTCLASSDATA)malloc(sizeof(SELECTCLASSDATA));
                memset(pscd, 0, sizeof(SELECTCLASSDATA));

                // create our own structure for QWL_USER
                pWindowData = malloc(sizeof(CLASSLISTCNRDATA));
                memset(pWindowData, 0, sizeof(CLASSLISTCNRDATA));
                WinSetWindowULong(hwndDlg, QWL_USER, (ULONG)pWindowData);

                // store the client data (create param from mp2)
                pWindowData->pClientData = (PCLASSLISTCLIENTDATA)mp2;

                pWindowData->pClientData->pscd = pscd;

                // setup container child
                pscd->hwndCnr = WinWindowFromID(hwndDlg, ID_XLDI_CNR);
                BEGIN_CNRINFO()
                {
                    cnrhSetView(CV_TREE | CA_TREELINE | CV_TEXT
                                    | CA_OWNERDRAW);
                    cnrhSetTreeIndent(30);
                    cnrhSetSortFunc(fnCompareName);
                } END_CNRINFO(pscd->hwndCnr);

                // fill container later
                fFillingCnr = FALSE;

                // initialize XADJUSTCTRLS structure
                winhAdjustControls(hwndDlg,             // dialog
                                   ampClassCnrCtls,    // MPARAMs array
                                   sizeof(ampClassCnrCtls) / sizeof(MPARAM), // items count
                                   NULL,                // pswpNew == NULL: initialize
                                   &pWindowData->xacClassCnr);  // storage area

                mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
            break; }

            /*
             * WM_DRAWITEM:
             *      container control owner draw: the
             *      cnr only allows the same color for
             *      all text items, so we need to draw
             *      the text ourselves
             */

            case WM_DRAWITEM:
                if ((USHORT)mp1 == ID_XLDI_CNR)
                    mrc = cnrhOwnerDrawRecord(mp2);
                else
                    mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
            break;

            /*
             * WM_WINDOWPOSCHANGED:
             *      posted _after_ the window has been moved
             *      or resized.
             *      Since we have a sizeable dlg, we need to
             *      update the controls' sizes also, or PM
             *      will display garbage. This is the trick
             *      how to use sizeable dlgs, because these do
             *      _not_ get sent WM_SIZE messages.
             */

            case WM_WINDOWPOSCHANGED:
            {
                // this msg is passed two SWP structs:
                // one for the old, one for the new data
                // (from PM docs)
                PSWP pswpNew = PVOIDFROMMP(mp1);
                PSWP pswpOld = pswpNew + 1;

                // resizing?
                if (pswpNew->fl & SWP_SIZE)
                {
                    if (pWindowData)
                    {
                        BOOL brc = winhAdjustControls(hwndDlg,             // dialog
                                           ampClassCnrCtls,    // MPARAMs array
                                           sizeof(ampClassCnrCtls) / sizeof(MPARAM), // items count
                                           pswpNew,             // mp1
                                           &pWindowData->xacClassCnr);  // storage area
                    }
                }
                mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
            break; }

            /*
             * WM_FILLCNR:
             *      this fills the cnr with all the WPS classes.
             *      This uses the generic (and complex, I admit)
             *      functions in classlst.c, which can take
             *      any container window and fill it up with
             *      WPS classes.
             */

            case WM_FILLCNR:
            {
                CHAR szClassInfoFile[CCHMAXPATH];
                // ULONG ulCopied;
                PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();

                HPOINTER hptrOld = winhSetWaitPointer();

                // pscd is the SELECTCLASSDATA structure in our window words;
                // this is used for communicating with the classlist functions
                PSELECTCLASSDATA pscd = pWindowData->pClientData->pscd;

                fFillingCnr = TRUE;

                // class to start with: either WPObject or SOMObject,
                // depending on instance data
                pscd->pszRootClass =
                    (pWindowData->pClientData->somThis->fShowSOMObject)
                        ? "SOMObject"
                        : "WPObject";
                // class to select: the same
                strcpy(pscd->szClassSelected,
                       pscd->pszRootClass);

                pscd->pfnwpReturnAttrForClass = fncbReturnWPSClassAttr;
                    // callback for cnr recc attributes
                pscd->pfnwpClassSelected = NULL;
                    // we don't need no callback for class selections

                // prepare class info text file path
                if (cmnQueryXFolderBasePath(szClassInfoFile))
                {
                    sprintf(szClassInfoFile+strlen(szClassInfoFile),
                            "\\help\\xfcls%s.txt",
                            cmnQueryLanguageCode());
                    doshReadTextFile(szClassInfoFile, &pszClassInfo);
                }
                else
                    return (NULL);

                // add orphans; this is done by setting the title
                // for the "Orphans" recc tree
                pscd->pszOrphans = pNLSStrings->pszWpsClassOrphans;

                // finally, fill container with WPS data (classlst.c)
                pscd->pwpsc = clsWpsClasses2Cnr(pscd->hwndCnr,
                                                pscd->pszRootClass,
                                                pscd);  // also contains callback

                fFillingCnr = FALSE;
                WinSetPointer(HWND_DESKTOP, hptrOld);
            break; }

            /*
             * WM_CONTROL:
             *      capture cnr notifications
             */

            case WM_CONTROL:

                if (SHORT1FROMMP(mp1) == ID_XLDI_CNR)
                {

                    // pscd is the SELECTCLASSDATA structure in our window words;
                    // this is used for communicating with the classlist functions
                    PSELECTCLASSDATA pscd = pWindowData->pClientData->pscd;

                    switch (SHORT2FROMMP(mp1)) // notify code
                    {
                        /*
                         * CN_EXPANDTREE:
                         *      tree view has been expanded:
                         *      do cnr auto-scroll if we're
                         *      not initially filling the cnr
                         */

                        case CN_EXPANDTREE:
                        {
                            if (!fFillingCnr)
                            {
                                PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
                                if (pGlobalSettings->TreeViewAutoScroll)
                                {
                                    pscd->preccExpanded = (PRECORDCORE)mp2;
                                    WinStartTimer(WinQueryAnchorBlock(hwndDlg),
                                                  hwndDlg,
                                                  1,
                                                  100);
                                }
                            }
                            mrc = MPNULL;
                        break; }

                        /*
                         * CN_CONTEXTMENU:
                         *      context menu requested
                         */

                        case CN_CONTEXTMENU:
                        {
                            HWND   hPopupMenu;
                            CHAR szDummy[2000];

                            pscd->preccSource = (PCLASSRECORDCORE)mp2;
                            if (pscd->preccSource)
                            {
                                // we have a selection
                                LONG lrc2;
                                hPopupMenu = WinLoadMenu(hwndDlg,
                                                         cmnQueryNLSModuleHandle(FALSE),
                                                         ID_XLM_CLASS_SEL);

                                if (pscd->preccSource->pwps)
                                {
                                    BOOL fAllowDeregister = TRUE;
                                    BOOL fAllowCreateObjects = TRUE;

                                    // allow deregistering?
                                    if (   (pscd->preccSource->pwps->pszModName == NULL)
                                                   // DLL == NULL: not in WPS class list,
                                                   // so we better not allow touching this
                                        || (pscd->preccSource->pwps->pszReplacesClass)
                                        || (pscd->preccSource->pwps->pszReplacedWithClasses)
                                                   // or replacement class;
                                                   // the user should undo the replacement first
                                       )
                                        fAllowDeregister = FALSE;

                                    if (pszClassInfo)
                                    {
                                        ULONG ulFlagsSelected = 0;
                                        // parse class info text whether this
                                        // class may be deregistered
                                        if (ParseDescription(pszClassInfo,
                                                pscd->preccSource->pwps->pszClassName,
                                                &ulFlagsSelected,
                                                szDummy))
                                        {
                                            // bit 0 signifies whether this class may
                                            // be deregistered
                                            if ((ulFlagsSelected & 1) == 0)
                                                fAllowDeregister = FALSE;

                                            // bit 2 signifies whether this class
                                            // can have instances created from it
                                            if ((ulFlagsSelected & 4) != 0)
                                                            // bit set
                                                fAllowCreateObjects = FALSE;
                                        }
                                    }

                                    if (   (pscd->preccSource->pwps->pszModName == NULL)
                                        || (pscd->preccSource->pwps->pszReplacesClass)
                                        || (pscd->preccSource->pwps->pClassObject
                                               == 0)   // class object invalid
                                        || (pscd->preccSource->pwps->pszReplacesClass)
                                                   // or replacement class;
                                                   // we'll only allow creation
                                                   // of objects of the _replaced_
                                                   // class
                                       )
                                        fAllowCreateObjects = FALSE;

                                    if (!fAllowDeregister)
                                        // disable "Deregister" menu item
                                        WinEnableMenuItem(hPopupMenu,
                                                ID_XLMI_DEREGISTER, FALSE);

                                    // allow replacements only if the
                                    // class has subclasses
                                    lrc2 = (LONG)WinSendMsg(
                                            pscd->hwndCnr, CM_QUERYRECORD,
                                            (MPARAM)pscd->preccSource,
                                            MPFROM2SHORT(CMA_FIRSTCHILD, CMA_ITEMORDER));
                                    if (    (lrc2 == 0)
                                         || (lrc2 == -1)
                                            // disallow if the class replaces something itself
                                         || (pscd->preccSource->pwps->pszReplacesClass)
                                       )
                                        WinEnableMenuItem(hPopupMenu,
                                            ID_XLMI_REPLACE, FALSE);

                                    // allow un-replacement only if the class
                                    // replaces another class
                                    if (pscd->preccSource->pwps->pszReplacesClass == NULL)
                                        WinEnableMenuItem(hPopupMenu,
                                            ID_XLMI_UNREPLACE, FALSE);

                                    // allow creating objects?
                                    if (!fAllowCreateObjects)
                                        // disable "Create Objects" menu item
                                        WinEnableMenuItem(hPopupMenu,
                                                ID_XLMI_CREATEOBJECT, FALSE);

                                } // end if (pscd->preccSource->pwps)
                                else
                                {
                                    WinEnableMenuItem(hPopupMenu,
                                        ID_XLMI_DEREGISTER, FALSE);
                                    WinEnableMenuItem(hPopupMenu,
                                        ID_XLMI_REPLACE, FALSE);
                                    WinEnableMenuItem(hPopupMenu,
                                        ID_XLMI_UNREPLACE, FALSE);
                                }

                            } else
                                // no selection: different menu
                                hPopupMenu = WinLoadMenu(hwndDlg, cmnQueryNLSModuleHandle(FALSE),
                                        ID_XLM_CLASS_NOSEL);

                            cnrhShowContextMenu(pscd->hwndCnr,
                                                (PRECORDCORE)(pscd->preccSource),
                                                hPopupMenu,
                                                hwndDlg);
                        break; }

                        /*
                         * CN_EMPHASIS:
                         *      selection changed: call NewClassSelected
                         */

                        case CN_EMPHASIS:
                        {
                            // get cnr notification struct
                            PNOTIFYRECORDEMPHASIS pnre = (PNOTIFYRECORDEMPHASIS)mp2;

                            if (pnre)
                                if (    (pnre->fEmphasisMask & CRA_SELECTED)
                                     && (pnre->pRecord)
                                   )
                                {
                                    if (     (PCLASSRECORDCORE)(pnre->pRecord)
                                          != pscd->preccSelection
                                       )
                                    {
                                        // store currently selected class record
                                        // in SELECTCLASSDATA
                                        pscd->preccSelection = (PCLASSRECORDCORE)(pnre->pRecord);

                                        StartMethodsUpdateTimer(pWindowData->pClientData);
                                    }
                                }
                        break; }

                        /*
                         * CN_HELP:
                         *
                         */

                        case CN_HELP:
                            WinPostMsg(pWindowData->pClientData->hwndClient,
                                       WM_HELP, 0, 0);
                        break;
                    }
                } // end if (SHORT1FROMMP(mp1) == ID_XLDI_CNR) {
            break;

            /*
             * WM_TIMER:
             *      we have two timers here.
             *      -- ID 1: timer for tree view auto-scroll (we don't
             *               use the Worker thread here).
             *      -- ID 2: timer for delayed window updates when a new
             *               class record gets selected. With a delay
             *               of 100 ms, we update the other windows.
             */

            case WM_TIMER:
                // stop timer (it's just for one shot)
                WinStopTimer(WinQueryAnchorBlock(hwndDlg), hwndDlg,
                             (ULONG)mp1);       // timer ID

                switch ((ULONG)mp1)        // timer ID
                {
                    /*
                     * Timer 1:
                     *      container auto-scroll timer.
                     */

                    case 1:
                    {
                        // pscd is the SELECTCLASSDATA structure in our window words;
                        // this is used for communicating with the classlist functions
                        PSELECTCLASSDATA pscd = pWindowData->pClientData->pscd;

                        if ( ( pscd->preccExpanded->flRecordAttr & CRA_EXPANDED) != 0 )
                        {
                            PRECORDCORE     preccLastChild;
                            // scroll the tree view properly
                            preccLastChild = WinSendMsg(pscd->hwndCnr,
                                                        CM_QUERYRECORD,
                                                        pscd->preccExpanded,
                                                                // expanded PRECORDCORE from CN_EXPANDTREE
                                                        MPFROM2SHORT(CMA_LASTCHILD,
                                                                     CMA_ITEMORDER));
                            if (preccLastChild)
                            {
                                // ULONG ulrc;
                                cnrhScrollToRecord(pscd->hwndCnr,
                                        (PRECORDCORE)preccLastChild,
                                        CMA_TEXT,   // record text rectangle only
                                        TRUE);      // keep parent visible
                            }
                        }
                    break; }

                    /*
                     * Timer 2:
                     *      update methods info timer.
                     */

                    case 2:
                        pWindowData->pClientData->ulUpdateTimerID = 0;
                        // update other windows with class info
                        NewClassSelected(pWindowData->pClientData);
                    break;
                }
            break;

            /*
             * WM_COMMAND:
             *      this handles the commands from the context menu.
             */

            case WM_COMMAND:
            {
                // pscd is the SELECTCLASSDATA structure in our window words;
                // this is used for communicating with the classlist functions
                PSELECTCLASSDATA pscd = pWindowData->pClientData->pscd;

                switch (SHORT1FROMMP(mp1))  // menu command
                {
                    // "Help" menu command
                    case ID_XFMI_HELP:
                        WinPostMsg(pWindowData->pClientData->hwndClient,
                                   WM_HELP, 0, 0);
                    break;

                    // "Register" menu command
                    case ID_XLMI_REGISTER:
                    {
                        REGISTERCLASSDATA rcd;
                        rcd.pszHelpLibrary = cmnQueryHelpLibrary();
                        rcd.ulHelpPanel = ID_XFH_REGISTERCLASS;
                        if (WinDlgBox(HWND_DESKTOP, hwndDlg,
                                                 fnwpRegisterClass,
                                                 cmnQueryNLSModuleHandle(FALSE),
                                                 ID_XLD_REGISTERCLASS,
                                                 &rcd) == DID_OK)
                        {
                            PSZ pTable[1];
                            HPOINTER hptrOld = winhSetWaitPointer();
                            pTable[0] = rcd.szClassName;

                            WinSendMsg(pscd->hwndCnr,
                                       CM_REMOVERECORD,
                                       (MPARAM)NULL,
                                       MPFROM2SHORT(0, // remove all records
                                                CMA_FREE | CMA_INVALIDATE));
                            clsCleanupWpsClasses(pscd->pwpsc);
                            WinSetPointer(HWND_DESKTOP, hptrOld);
                            free(pszClassInfo);
                            pszClassInfo = NULL;

                            if (WinRegisterObjectClass(rcd.szClassName, rcd.szModName))
                                // success
                                cmnMessageBoxMsgExt(hwndDlg,
                                        121,
                                        pTable, 1, 131,
                                        MB_OK);
                            else
                                // error
                                cmnMessageBoxMsgExt(hwndDlg,
                                        104,
                                        pTable, 1, 132,
                                        MB_OK);

                            // fill cnr again
                            WinPostMsg(hwndDlg, WM_FILLCNR, MPNULL, MPNULL);
                        }
                    break; }

                    // "Deregister" menu command
                    case ID_XLMI_DEREGISTER:
                    {
                        if (pscd->preccSource)
                            if (pscd->preccSource->pwps)
                            {
                                BOOL fAllow = FALSE;
                                CHAR szTemp[CCHMAXPATH];
                                PSZ pTable[2];
                                pTable[0] = szTemp;
                                pTable[1] = pscd->preccSource->pwps->pszReplacedWithClasses;

                                strcpy(szTemp, pscd->preccSource->pwps->pszClassName);
                                    // save for later

                                // do not allow deregistering if the class is currently
                                // replaced by another class
                                if ( pscd->preccSource->pwps->pszReplacedWithClasses)
                                {
                                    // show warning
                                    cmnMessageBoxMsgExt(hwndDlg,
                                                116,
                                                pTable, 2, 139,
                                                MB_OK);
                                    // and stop
                                    break;
                                }

                                if (strncmp(pscd->preccSource->pwps->pszClassName,
                                                "XFld", 4) == 0) {
                                    // XFolder class
                                    if (cmnMessageBoxMsgExt(hwndDlg,
                                                116,
                                                pTable, 1, 120,
                                                MB_YESNO | MB_DEFBUTTON2)
                                            == MBID_YES)
                                        fAllow = TRUE;
                                } else
                                    if (cmnMessageBoxMsgExt(hwndDlg,
                                                116,
                                                pTable, 1, 118,
                                                MB_YESNO | MB_DEFBUTTON2)
                                            == MBID_YES)
                                        fAllow = TRUE;

                                if (fAllow)
                                {
                                    if (WinDeregisterObjectClass(pscd->preccSource->pwps->pszClassName))
                                    {
                                        // success
                                        WinSendMsg(pscd->hwndCnr,
                                                   CM_REMOVERECORD,
                                                   (MPARAM)&(pscd->preccSource),
                                                   MPFROM2SHORT(1, // remove one record
                                                           CMA_FREE | CMA_INVALIDATE));

                                        lstRemoveItem(pscd->pwpsc->pllClassList,
                                                      pscd->preccSource->pwps);
                                                        // remove item from list

                                        // free(pscd->pRecordSelected->pwps);

                                        cmnMessageBoxMsgExt(hwndDlg,
                                                121,
                                                pTable, 1, 122,
                                                MB_OK);
                                    } else
                                        // error
                                        cmnMessageBoxMsgExt(hwndDlg,
                                                104,
                                                pTable, 1, 119,
                                                MB_OK);
                                }
                            }
                    break; }

                    // "Replace class" menu command:
                    // show yet another WPS classes dlg
                    case ID_XLMI_REPLACE:
                    {
                        if (pscd->preccSource)
                        {
                            SELECTCLASSDATA         scd;
                            // STATUSBARSELECTCLASS    sbsc;
                            PSZ                     pszClassName =
                                        pscd->preccSource->pwps->pszClassName;

                            CHAR szTitle[CCHMAXPATH] = "title";
                            CHAR szIntroText[2000] = "intro";

                            cmnGetMessage(NULL, 0,
                                    szTitle, sizeof(szTitle), 112);
                            // replace "%1" by class name which is to be
                            // replaced
                            cmnGetMessage(&pszClassName, 1,
                                    szIntroText, sizeof(szIntroText), 123);
                            scd.pszDlgTitle = szTitle;
                            scd.pszIntroText = szIntroText;
                            scd.pszRootClass = pszClassName;
                            scd.pszOrphans = NULL;
                            strcpy(scd.szClassSelected, scd.pszRootClass);

                            scd.pfnwpReturnAttrForClass = NULL; // fncbStatusBarReturnClassAttr;
                            scd.pfnwpClassSelected = fncbReplaceClassSelected;
                            scd.ulUserClassSelected = 0; //(ULONG)&sbsc;

                            scd.pszHelpLibrary = cmnQueryHelpLibrary();
                            scd.ulHelpPanel = 0;

                            if (clsSelectWpsClassDlg(hwndDlg,
                                                     cmnQueryNLSModuleHandle(FALSE),
                                                     ID_XLD_SELECTCLASS,
                                                     &scd) == DID_OK)
                            {
                                PSZ pTable[2];
                                pTable[0] = pscd->preccSource->pwps->pszClassName;
                                pTable[1] = scd.szClassSelected;
                                if (cmnMessageBoxMsgExt(hwndDlg,
                                            116,
                                            pTable, 2, 124,
                                            MB_YESNO | MB_DEFBUTTON2)
                                        == MBID_YES)
                                    if (WinReplaceObjectClass(
                                            pscd->preccSource->pwps->pszClassName,
                                            scd.szClassSelected,
                                            TRUE))
                                        // success
                                        cmnMessageBoxMsgExt(hwndDlg,
                                                121,
                                                pTable, 2, 129,
                                                MB_OK);
                                    else
                                        // error
                                        cmnMessageBoxMsgExt(hwndDlg,
                                                104,
                                                pTable, 2, 130,
                                                MB_OK);
                            }
                        }
                    break; }

                    // "Unreplace class" menu command
                    case ID_XLMI_UNREPLACE:
                    {
                        if (pscd->preccSource)
                            if (pscd->preccSource->pwps)
                            {
                                BOOL fAllow = FALSE;
                                PSZ pTable[2];
                                pTable[0] = pscd->preccSource->pwps->pszReplacesClass;
                                pTable[1] = pscd->preccSource->pwps->pszClassName;

                                if (strncmp(pscd->preccSource->pwps->pszClassName,
                                                "XFld", 4) == 0)
                                {
                                    // some XFolder class
                                    if (cmnMessageBoxMsgExt(hwndDlg,
                                                116,
                                                pTable, 2, 125,
                                                MB_YESNO | MB_DEFBUTTON2)
                                            == MBID_YES)
                                        fAllow = TRUE;
                                } else
                                    if (cmnMessageBoxMsgExt(hwndDlg,
                                                116,
                                                pTable, 2, 126,
                                                MB_YESNO | MB_DEFBUTTON2)
                                            == MBID_YES)
                                        fAllow = TRUE;

                                if (fAllow)
                                {
                                    if (WinReplaceObjectClass(
                                            pscd->preccSource->pwps->pszReplacesClass,
                                            pscd->preccSource->pwps->pszClassName,
                                            FALSE))
                                        // success
                                        cmnMessageBoxMsgExt(hwndDlg,
                                                121,
                                                pTable, 2, 127,
                                                MB_OK);
                                    else
                                        // error
                                        cmnMessageBoxMsgExt(hwndDlg,
                                                104,
                                                pTable, 2, 128,
                                                MB_OK);
                                }
                            }
                    break; }

                    // "Create object" menu command
                    // (new with V0.9.0)
                    case ID_XLMI_CREATEOBJECT:
                    {
                        if (pscd->preccSource)
                            if (pscd->preccSource->pwps)
                            {
                                // BOOL fAllow = FALSE;
                                PSZ pTable[2];
                                pTable[0] = pscd->preccSource->pwps->pszClassName;
                                pTable[1] = _wpclsQueryTitle(pscd->preccSource->pwps->pClassObject);

                                if (cmnMessageBoxMsgExt(hwndDlg,
                                            116,
                                            pTable, 2, 141,
                                            MB_YESNO | MB_DEFBUTTON2)
                                        == MBID_YES)
                                {
                                    WinCreateObject(pscd->preccSource->pwps->pszClassName,
                                                    _wpclsQueryTitle(pscd->preccSource->pwps->pClassObject),
                                                    "",        // setup string
                                                    "<WP_DESKTOP>", // location
                                                    CO_FAILIFEXISTS);
                                }

                            }
                    break; }

                } // end switch (SHORT1FROMMP(mp1))
                pscd->preccSource = NULL;
            break; }

            /*
             * WM_MENUEND:
             *      if the context menu is dismissed, we'll need
             *      to remove the cnr source emphasis which was
             *      set above when showing the context menu.
             */

            case WM_MENUEND:
            {
                // pscd is the SELECTCLASSDATA structure in our window words;
                // this is used for communicating with the classlist functions
                PSELECTCLASSDATA pscd = pWindowData->pClientData->pscd;
                cnrhSetSourceEmphasis(pscd->hwndCnr,
                                      pscd->preccSource,
                                      FALSE);
            break; }

            /*
             * WM_SYSCOMMAND:
             *      pass on to frame
             */

            case WM_SYSCOMMAND:
                WinPostMsg(WinQueryWindow(hwndDlg, QW_OWNER),
                           msg, mp1, mp2);
            break;

            /*
             * WM_HELP:
             *      pass on to client
             */

            case WM_HELP:
                WinPostMsg(pWindowData->pClientData->hwndClient, msg, mp1, mp2);
            break;

            /*
             * WM_DESTROY:
             *      clean up big time
             */

            case WM_DESTROY:
            {
                HPOINTER hptrOld = winhSetWaitPointer();
                // free cnr records
                WinSendMsg(WinWindowFromID(hwndDlg, ID_XLDI_CNR),
                           CM_REMOVERECORD,
                           (MPARAM)NULL,
                           MPFROM2SHORT(0, // remove all records
                                   CMA_FREE | CMA_INVALIDATE));

                // cleanup allocated WPS data (classlst.c)
                clsCleanupWpsClasses(pWindowData->pClientData->pscd->pwpsc);
                free(pWindowData->pClientData->pscd);

                free(pszClassInfo);
                pszClassInfo = NULL;

                // clean up window positions
                winhAdjustControls(hwndDlg,             // dialog
                                   NULL,                // clean up
                                   0,                   // items count
                                   NULL,                // clean up
                                   &pWindowData->xacClassCnr); // storage area

                free(pWindowData);

                mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
                WinSetPointer(HWND_DESKTOP, hptrOld);
            break; }

            default:
                mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
        }
    }
    CATCH(excpt1)
    {
        mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
    } END_CATCH();

    return (mrc);
}

/*
 *@@ fnwpClassInfoDlg:
 *      this is the window proc for the dialog window
 *      with all the different class info.
 *      This dlg is the second child of the abstract "split window"
 *      and has many more subcontrols for the class info.
 *      See fnwpClassListClient for a window hierarchy.
 *
 *      This is a sizeable dialog. Since dialogs do not
 *      receive WM_SIZE, we need to handle WM_WINDOWPOSCHANGED
 *      instead. This gets sent to us when the "split window"
 *      calls WinSetWindowPos when the frame window has been
 *      resized.
 *
 *      This function does not update the class info controls.
 *      This is done in NewClassSelected only, which gets called
 *      from fnwpClassListDlg.
 *      We only need this function to be able to resize that
 *      dialog window and reposition the subcontrols.
 *
 *@@added V0.9.0 [umoeller]
 */

MRESULT EXPENTRY fnwpClassInfoDlg(HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    PCLASSLISTINFODATA pWindowData
            = (PCLASSLISTINFODATA)WinQueryWindowULong(hwndDlg, QWL_USER);

    switch (msg)
    {

        /*
         * WM_INITDLG:
         *      mp2 (pCreateParam) points to the frame's
         *      CLASSLISTCLIENTDATA structure.
         */

        case WM_INITDLG:
            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
            ctlPrepareStaticIcon(WinWindowFromID(hwndDlg, ID_XLDI_ICON), 1);
                // even though we have no animation, we use
                // the animation funcs to subclass the icon control;
                // otherwise we'll get garbage with transparent
                // class icons

            // create our own structure for QWL_USER
            pWindowData = malloc(sizeof(CLASSLISTINFODATA));
            memset(pWindowData, 0, sizeof(CLASSLISTINFODATA));
            WinSetWindowULong(hwndDlg, QWL_USER, (ULONG)pWindowData);

            // store the client data (create param from mp2)
            pWindowData->pClientData = (PCLASSLISTCLIENTDATA)mp2;

            // initialize XADJUSTCTRLS structure
            winhAdjustControls(hwndDlg,             // dialog
                               ampClassInfoCtls,    // MPARAMs array
                               sizeof(ampClassInfoCtls) / sizeof(MPARAM), // items count
                               NULL,                // pswpNew == NULL: initialize
                               &pWindowData->xacClassInfo);  // storage area
        break;

        /*
         * WM_WINDOWPOSCHANGED:
         *      posted _after_ the window has been moved
         *      or resized.
         *      Since we have a sizeable dlg, we need to
         *      update the controls' sizes also, or PM
         *      will display garbage. This is the trick
         *      how to use sizeable dlgs, because these do
         *      _not_ get sent WM_SIZE messages.
         */

        case WM_WINDOWPOSCHANGED:
        {
            // this msg is passed two SWP structs:
            // one for the old, one for the new data
            // (from PM docs)
            PSWP pswpNew = PVOIDFROMMP(mp1);
            PSWP pswpOld = pswpNew + 1;

            // resizing?
            if (pswpNew->fl & SWP_SIZE)
            {
                if (pWindowData)
                {
                    BOOL brc = winhAdjustControls(hwndDlg,             // dialog
                                       ampClassInfoCtls,    // MPARAMs array
                                       sizeof(ampClassInfoCtls) / sizeof(MPARAM), // items count
                                       pswpNew,             // mp1
                                       &pWindowData->xacClassInfo);  // storage area
                }
            }
            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
        break; }

        /*
         * WM_SYSCOMMAND:
         *      pass on to frame
         */

        case WM_SYSCOMMAND:
            WinPostMsg(WinQueryWindow(hwndDlg, QW_OWNER),
                       msg, mp1, mp2);
        break;

        /*
         * WM_HELP:
         *      pass on to client
         */

        case WM_HELP:
        {
            WinPostMsg(pWindowData->pClientData->hwndClient, msg, mp1, mp2);
        break; }

        /*
         * WM_DESTROY:
         *      clean up.
         */

        case WM_DESTROY:
        {
            // clean up window positions
            winhAdjustControls(hwndDlg,             // dialog
                               NULL,                // clean up
                               0,                   // items count
                               NULL,                // clean up
                               &pWindowData->xacClassInfo); // storage area
            free(pWindowData);
            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
        break; }

        default:
            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
        break;
    }

    return (mrc);
}

/*
 *@@ fnwpMethodInfoDlg:
 *
 */

MRESULT EXPENTRY fnwpMethodInfoDlg(HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = MPNULL;

    PCLASSLISTMETHODDATA pWindowData = (PCLASSLISTMETHODDATA)WinQueryWindowULong(hwndDlg, QWL_USER);

    switch(msg)
    {

        /*
         * WM_INITDLG:
         *      set up the container data and colors.
         *      mp2 (pCreateParam) points to the frame's
         *      CLASSLISTWINDOWDATA structure.
         */

        case WM_INITDLG:
        {
            PNLSSTRINGS     pNLSStrings = cmnQueryNLSStrings();
            XFIELDINFO      xfi[7];
            PFIELDINFO      pfi = NULL;
            HWND            hwndCnr = WinWindowFromID(hwndDlg, ID_XLDI_CNR);
            int             i = 0;
            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);

            // create our own structure for QWL_USER
            pWindowData = (PCLASSLISTMETHODDATA)malloc(sizeof(CLASSLISTMETHODDATA));
            memset(pWindowData, 0, sizeof(CLASSLISTMETHODDATA));
            WinSetWindowULong(hwndDlg, QWL_USER, (ULONG)pWindowData);

            // store the client data (create param from mp2)
            pWindowData->pClientData = (PCLASSLISTCLIENTDATA)mp2;

            // initialize XADJUSTCTRLS structure
            winhAdjustControls(hwndDlg,             // dialog
                               ampMethodInfoCtls,    // MPARAMs array
                               sizeof(ampMethodInfoCtls) / sizeof(MPARAM), // items count
                               NULL,                // pswpNew == NULL: initialize
                               &pWindowData->xacMethodInfo);  // storage area

            // set up cnr details view
            xfi[i].ulFieldOffset = FIELDOFFSET(METHODRECORD, ulMethodIndex);
            xfi[i].pszColumnTitle = pNLSStrings->pszClsListIndex;
            xfi[i].ulDataType = CFA_ULONG;
            xfi[i++].ulOrientation = CFA_RIGHT;

            xfi[i].ulFieldOffset = FIELDOFFSET(RECORDCORE, pszIcon);
            xfi[i].pszColumnTitle = pNLSStrings->pszClsListMethod;
            xfi[i].ulDataType = CFA_STRING;
            xfi[i++].ulOrientation = CFA_LEFT;

            xfi[i].ulFieldOffset = FIELDOFFSET(METHODRECORD, pszMethodProc);
            xfi[i].pszColumnTitle = pNLSStrings->pszClsListAddress;
            xfi[i].ulDataType = CFA_STRING;
            xfi[i++].ulOrientation = CFA_LEFT;

            xfi[i].ulFieldOffset = FIELDOFFSET(METHODRECORD, pszIntroducedBy);
            xfi[i].pszColumnTitle = pNLSStrings->pszClsListClass;
            xfi[i].ulDataType = CFA_STRING;
            xfi[i++].ulOrientation = CFA_LEFT;

            xfi[i].ulFieldOffset = FIELDOFFSET(METHODRECORD, pszOverriddenBy);
            xfi[i].pszColumnTitle = pNLSStrings->pszClsListOverriddenBy;
            xfi[i].ulDataType = CFA_STRING;
            xfi[i++].ulOrientation = CFA_LEFT;

            pfi = cnrhSetFieldInfos(hwndCnr,
                                    &xfi[0],
                                    i,             // array item count
                                    TRUE,          // no draw lines
                                    2);            // return second column

            BEGIN_CNRINFO()
            {
                cnrhSetView(CV_DETAIL | CA_DETAILSVIEWTITLES);
                cnrhSetSplitBarAfter(pfi);
                cnrhSetSplitBarPos(200);
                cnrhSetSortFunc(QueryMethodsSortFunc(pWindowData->pClientData));
            } END_CNRINFO(hwndCnr);

            // check "instance methods"
            winhSetDlgItemChecked(hwndDlg,
                                  pWindowData->pClientData->somThis->ulMethodsRadioB,
                                  TRUE);
        break; }

        /*
         * WM_WINDOWPOSCHANGED:
         *      posted _after_ the window has been moved
         *      or resized.
         *      Since we have a sizeable dlg, we need to
         *      update the controls' sizes also, or PM
         *      will display garbage. This is the trick
         *      how to use sizeable dlgs, because these do
         *      _not_ get sent WM_SIZE messages.
         */

        case WM_WINDOWPOSCHANGED:
        {
            // this msg is passed two SWP structs:
            // one for the old, one for the new data
            // (from PM docs)
            PSWP pswpNew = PVOIDFROMMP(mp1);
            PSWP pswpOld = pswpNew + 1;

            // resizing?
            if (pswpNew->fl & SWP_SIZE)
            {
                if (pWindowData)
                {
                    BOOL brc = winhAdjustControls(hwndDlg,             // dialog
                                       ampMethodInfoCtls,    // MPARAMs array
                                       sizeof(ampMethodInfoCtls) / sizeof(MPARAM), // items count
                                       pswpNew,             // mp1
                                       &pWindowData->xacMethodInfo);  // storage area
                }
            }
            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
        break; }

        /*
         * WM_CONTROL:
         *
         */

        case WM_CONTROL:
            switch (SHORT1FROMMP(mp1))  // usID
            {
                case ID_XLDI_RADIO_INSTANCEMETHODS:
                case ID_XLDI_RADIO_CLASSMETHODS:
                    if (    (SHORT2FROMMP(mp1) == BN_CLICKED)
                         || (SHORT2FROMMP(mp1) == BN_DBLCLICKED)
                       )
                        if (pWindowData->pClientData->somThis->ulMethodsRadioB != SHORT1FROMMP(mp1))
                        {
                            // radio button selection changed:
                            pWindowData->pClientData->somThis->ulMethodsRadioB = SHORT1FROMMP(mp1);
                            _wpSaveDeferred(pWindowData->pClientData->somSelf);
                            // update method info
                            StartMethodsUpdateTimer(pWindowData->pClientData);
                        }
                break;

                case ID_XLDI_CNR:
                    switch (SHORT2FROMMP(mp1)) // notify code
                    {
                        case CN_HELP:
                            WinPostMsg(pWindowData->pClientData->hwndClient,
                                       WM_HELP, 0, 0);
                        break;

                        case CN_CONTEXTMENU:
                        {
                            HWND            hPopupMenu = 0;
                            pWindowData->pMethodReccSource = (PMETHODRECORD)mp2;
                            if (pWindowData->pMethodReccSource)
                            {
                                // we have a selection
                            }
                            else
                            {
                                // whitespace:
                                hPopupMenu = WinLoadMenu(hwndDlg, cmnQueryNLSModuleHandle(FALSE),
                                                         ID_XLM_METHOD_NOSEL);
                                // check current sort item
                                WinCheckMenuItem(hPopupMenu,
                                                 pWindowData->pClientData->somThis->ulSortID, TRUE);
                            }
                            if (hPopupMenu)
                                cnrhShowContextMenu(WinWindowFromID(hwndDlg, ID_XLDI_CNR),
                                                    (PRECORDCORE)pWindowData->pMethodReccSource,
                                                    hPopupMenu,
                                                    hwndDlg);
                        break; }
                    }
                break; // case ID_XLDI_CNR
            }
        break;

        /*
         * WM_COMMAND:
         *      this handles the commands from the context menu.
         */

        case WM_COMMAND:
        {
            PFNCNRSORT  pfnCnrSort = NULL;

            switch (SHORT1FROMMP(mp1))  // menu command
            {
                // "Help" menu command
                case ID_XFMI_HELP:
                    WinPostMsg(pWindowData->pClientData->hwndClient,
                               WM_HELP, 0, 0);
                break;

                // "Sort by" commands
                case ID_XLMI_METHOD_SORT_INDEX:
                case ID_XLMI_METHOD_SORT_NAME:
                case ID_XLMI_METHOD_SORT_INTRO:
                case ID_XLMI_METHOD_SORT_OVERRIDE:
                    // store sort item in window data for
                    // menu item checks above
                    pWindowData->pClientData->somThis->ulSortID = SHORT1FROMMP(mp1);
                    _wpSaveDeferred(pWindowData->pClientData->somSelf);
                    // update container's sort func
                    BEGIN_CNRINFO()
                    {
                        cnrhSetSortFunc(QueryMethodsSortFunc(pWindowData->pClientData));
                    } END_CNRINFO(WinWindowFromID(hwndDlg, ID_XLDI_CNR));
                break;
            }
        break; }

        /*
         * WM_MENUEND:
         *      if the context menu is dismissed, we'll need
         *      to remove the cnr source emphasis which was
         *      set above when showing the context menu.
         */

        case WM_MENUEND:
            cnrhSetSourceEmphasis(WinWindowFromID(hwndDlg, ID_XLDI_CNR),
                                  pWindowData->pMethodReccSource,
                                  FALSE);
        break;

        /*
         * WM_SYSCOMMAND:
         *      pass on to frame
         */

        case WM_SYSCOMMAND:
            WinPostMsg(WinQueryWindow(hwndDlg, QW_OWNER),
                       msg, mp1, mp2);
        break;

        /*
         * WM_HELP:
         *      pass on to client
         */

        case WM_HELP:
            WinPostMsg(pWindowData->pClientData->hwndClient, msg, mp1, mp2);
        break;

        case WM_DESTROY:
            // clean up window positions
            winhAdjustControls(hwndDlg,             // dialog
                               NULL,                // clean up
                               0,                   // items count
                               NULL,                // clean up
                               &pWindowData->xacMethodInfo); // storage area
            CleanupMethodsInfo(pWindowData->pClientData);
            free(pWindowData);
        break;

        default:
            mrc = WinDefDlgProc(hwndDlg, msg, mp1, mp2);
        break;
    }

    return (mrc);
}

/* ******************************************************************
 *                                                                  *
 *   XWPClassList notebook callbacks (notebook.c)                   *
 *                                                                  *
 ********************************************************************/

/*
 *@@ cllClassListInitPage:
 *      "Class list" page notebook callback function (notebook.c).
 *      Sets the controls on the page according to the instance
 *      settings.
 *
 */

VOID cllClassListInitPage(PCREATENOTEBOOKPAGE pcnbp,  // notebook info struct
                           ULONG flFlags)              // CBI_* flags (notebook.h)
{
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    XWPClassListData *somThis = XWPClassListGetData(pcnbp->somSelf);

    if (flFlags & CBI_INIT)
    {
        if (pcnbp->pUser == NULL)
        {
            // first call: backup instance data for "Undo" button;
            // this memory will be freed automatically by the
            // common notebook window function (notebook.c) when
            // the notebook page is destroyed
            pcnbp->pUser = malloc(sizeof(XWPClassListData));
            memcpy(pcnbp->pUser, somThis, sizeof(XWPClassListData));
        }
    }

    if (flFlags & CBI_SET)
    {
        winhSetDlgItemChecked(pcnbp->hwndPage, ID_XLDI_SHOWSOMOBJECT,
                _fShowSOMObject);
        winhSetDlgItemChecked(pcnbp->hwndPage, ID_XLDI_SHOWMETHODS,
                _fShowMethods);
    }

    if (flFlags & CBI_ENABLE)
    {
    }
}

/*
 * cllClassListItemChanged:
 *      "XFolder" page notebook callback function (notebook.c).
 *      Reacts to changes of any of the dialog controls.
 *
 *@@changed V0.9.0 [umoeller]: adjusted function prototype
 */

MRESULT cllClassListItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                 USHORT usItemID,
                                 USHORT usNotifyCode,
                                 ULONG ulExtra)      // for checkboxes: contains new state
{
    XWPClassListData *somThis = XWPClassListGetData(pcnbp->somSelf);
    BOOL    fUpdate = TRUE,
            fOldShowMethods = _fShowMethods;

    switch (usItemID)
    {
        case ID_XLDI_SHOWSOMOBJECT:
            _fShowSOMObject = ulExtra;
        break;

        case ID_XLDI_SHOWMETHODS:
            _fShowMethods = ulExtra;
        break;

        case DID_UNDO:
            if (pcnbp->pUser) {
                XWPClassListData *Backup = (pcnbp->pUser);
                // "Undo" button: restore backed up instance data
                _fShowSOMObject = Backup->fShowSOMObject;
                _fShowMethods = Backup->fShowMethods;
                // have the page updated by calling the callback above
                cllClassListInitPage(pcnbp, CBI_SHOW | CBI_ENABLE);
            }
        break;

        case DID_DEFAULT:
            // "Default" button:
            _fShowSOMObject = 0;
            _fShowMethods = 1;
            // have the page updated by calling the callback above
            cllClassListInitPage(pcnbp, CBI_SHOW | CBI_ENABLE);
        break;

        default:
            fUpdate = FALSE;
        break;
    }

    if (fUpdate)
        _wpSaveDeferred(pcnbp->somSelf);

    if (fOldShowMethods != _fShowMethods)
        if (_hwndOpenView)
        {
            // reformat open view:
            HWND hwndClient = WinWindowFromID(_hwndOpenView, FID_CLIENT);
            PCLASSLISTCLIENTDATA pWindowData = WinQueryWindowPtr(hwndClient, QWL_USER);
            RelinkWindows(pWindowData, TRUE);
        }

    return ((MPARAM)0);
}

/*
 *@@ cllCreateClassListView:
 *      this gets called from XWPClassList::wpOpen to
 *      create a new "class list" view. The parameters
 *      are just passed on from wpOpen.
 *
 *      The class list window is a regular standard PM
 *      frame, using fnwpClassListClient as its client
 *      window procedure. In that procedure, upon WM_CREATE,
 *      the subwindows are created and linked.
 */

HWND cllCreateClassListView(WPObject *somSelf,
                            HWND hwndCnr,
                            ULONG ulView)
{
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    HWND            hwndFrame = 0;

    TRY_LOUD(excpt1, NULL)
    {
        HAB         hab = WinQueryAnchorBlock(HWND_DESKTOP);
        HWND        hwndClient;
        ULONG       flCreate;                      // Window creation flags
        SWP         swpFrame;
        CLIENTCTLDATA ClientCData;

        // register the frame class, adding a user word to the window data to
        // anchor the object use list item for this open view
        WinRegisterClass(hab,
                         WC_CLASSLISTCLIENT,
                         fnwpClassListClient,
                         CS_SIZEREDRAW | CS_SYNCPAINT,
                         sizeof(PCLASSLISTCLIENTDATA)); // additional bytes to reserve

        // create the frame window
        flCreate = FCF_SYSMENU
                    | FCF_SIZEBORDER
                    | FCF_TITLEBAR
                    | FCF_MINMAX
                    // | FCF_TASKLIST
                    | FCF_NOBYTEALIGN;

        swpFrame.x = 100;
        swpFrame.y = 100;
        swpFrame.cx = 500;
        swpFrame.cy = 500;
        swpFrame.hwndInsertBehind = HWND_TOP;
        swpFrame.fl = SWP_MOVE | SWP_SIZE;

        memset(&ClientCData, 0, sizeof(ClientCData));
        ClientCData.cb = sizeof(ClientCData);
        ClientCData.somSelf = somSelf;
        ClientCData.ulView = ulView;
        hwndFrame = winhCreateStdWindow(HWND_DESKTOP,           // frame parent
                                        &swpFrame,
                                        flCreate,
                                        WS_ANIMATE,
                                        _wpQueryTitle(somSelf), // title bar
                                        0,                      // res IDs
                                        WC_CLASSLISTCLIENT,     // client class
                                        0L,                     // client wnd style
                                        ID_CLASSFRAME,          // ID
                                        &ClientCData,
                                        &hwndClient);

        if (hwndFrame)
        {
            // now position the frame and the client:
            // 1) frame
            if (!winhRestoreWindowPos(hwndFrame,
                              HINI_USER,
                              INIAPP_XFOLDER,
                              INIKEY_WNDPOSCLASSINFO,
                              SWP_MOVE | SWP_SIZE))
                // INI data not found:
                WinSetWindowPos(hwndFrame,
                                HWND_TOP,
                                100, 100,
                                500, 500,
                                SWP_MOVE | SWP_SIZE);

            // finally, show window
            WinShowWindow(hwndFrame, TRUE);
        }
    }
    CATCH(excpt1) { } END_CATCH();

    return (hwndFrame);
}

