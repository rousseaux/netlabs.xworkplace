
/*
 *@@sourcefile ctr_engine.c:
 *      XCenter internal engine. This does the hard work
 *      of maintaining widget classes, loading/unloading plugins,
 *      creation/destruction of widgets, widget settings management,
 *      creating/reformatting/destroying the XCenter view itself,
 *      and so on.
 *
 *      Be warned, this code is a bit complex.
 *
 *      See src\shared\center.c for an introduction to the XCenter.
 *
 *      Function prefix for this file:
 *      --  ctrp*
 *
 *      This is all new with V0.9.7.
 *
 *@@added V0.9.7 (2000-11-27) [umoeller]
 *@@header "xcenter\centerp.h"
 */

/*
 *      Copyright (C) 2000-2001 Ulrich M”ller.
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
#define INCL_DOSMODULEMGR
#define INCL_DOSEXCEPTIONS
#define INCL_DOSSEMAPHORES
#define INCL_DOSMISC
#define INCL_DOSERRORS

#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WINMESSAGEMGR
#define INCL_WINDIALOGS
#define INCL_WININPUT
#define INCL_WINTRACKRECT
#define INCL_WINPOINTERS
#define INCL_WINSYS
#define INCL_WINTIMER
#define INCL_WINMENUS

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
#include "helpers\comctl.h"             // common controls (window procs)
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\gpih.h"               // GPI helper routines
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\nls.h"                // National Language Support helpers
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\standards.h"          // some standard macros
#include "helpers\stringh.h"            // string helper routines
#include "helpers\threads.h"            // thread helpers
#include "helpers\timer.h"              // replacement PM timers
#include "helpers\winh.h"               // PM helper routines
#include "helpers\wphandle.h"           // file-system object handles
#include "helpers\xstring.h"            // extended string helpers

// SOM headers which don't crash with prec. header files
#include "xcenter.ih"
#include "xfobj.ih"                     // XFldObject

// XWorkplace implementation headers
#include "bldlevel.h"                   // XWorkplace build level definitions
#include "xwpapi.h"                     // public XWorkplace definitions

#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

#include "shared\center.h"              // public XCenter interfaces
#include "xcenter\centerp.h"            // private XCenter implementation

#include "hook\xwphook.h"

#pragma hdrstop                     // VAC++ keeps crashing otherwise
#include <wpshadow.h>

/* ******************************************************************
 *
 *   Declarations
 *
 ********************************************************************/

/*
 * WinSetDesktopWorkArea:
 *      undocumented Warp 4 API to change the "desktop work
 *      area", which, among other things, sets the size for
 *      maximized windows.
 *
 *      This is manually imported from PMMERGE.5468.
 */

BOOL APIENTRY WinSetDesktopWorkArea(HWND hwndDesktop,
                                    PRECTL pwrcWorkArea);
typedef BOOL APIENTRY WINSETDESKTOPWORKAREA(HWND hwndDesktop,
                                            PRECTL pwrcWorkArea);
typedef WINSETDESKTOPWORKAREA *PWINSETDESKTOPWORKAREA;

/*
 * WinQueryDesktopWorkArea:
 *      the reverse to the above.
 *
 *      This is manually imported from PMMERGE.5469.
 */

BOOL APIENTRY WinQueryDesktopWorkArea(HWND hwndDesktop,
                                      PRECTL pwrcWorkArea);
typedef BOOL APIENTRY WINQUERYDESKTOPWORKAREA(HWND hwndDesktop,
                                              PRECTL pwrcWorkArea);
typedef WINQUERYDESKTOPWORKAREA *PWINQUERYDESKTOPWORKAREA;

// some more forward declarations
VOID StartAutoHide(PXCENTERWINDATA pXCenterData);
VOID ClientPaint2(HWND hwndClient, HPS hps);

// width of the sizing bar... this is always drawn as
// a 3D rectangle three pixels wide, and we need one
// extra pixel on each side
#define SIZING_BAR_WIDTH        5

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

static PXTIMERSET           G_pLastXTimerSet = NULL;

static COUNTRYSETTINGS      G_CountrySettings = {0};

// array of classes created by ctrpLoadClasses
// replaced this with a linked list V0.9.9 (2001-03-09) [umoeller]
static LINKLIST             G_llWidgetClasses;
static BOOL                 G_fWidgetClassesLoaded = FALSE;
// reference count (raised with each ctrpLoadClasses call)
static ULONG                G_ulWidgetClassesRefCount = 0;
// mutex protecting the list    V0.9.12 (2001-05-20) [umoeller]
static HMTX                 G_hmtxClasses = NULLHANDLE;

// global array of plugin modules which were loaded
static LINKLIST             G_llModules;      // this contains plain HMODULEs as data
static BOOL                 G_fModulesInitialized = FALSE;

// widget being dragged
static PPRIVATEWIDGETVIEW     G_pWidgetBeingDragged = NULL;

static PWINSETDESKTOPWORKAREA   G_pWinSetDesktopWorkArea = NULL;
static PWINQUERYDESKTOPWORKAREA G_pWinQueryDesktopWorkArea = NULL;

static BOOL                     G_fWorkAreaSupported = FALSE;
                                    // set by ctrpDesktopWorkareaSupported
static LINKLIST                 G_llWorkAreaViews;
                                    // linked list of XCENTERVIEWDATA's which currently
                                    // have the "reduce workarea" flag on. This list
                                    // gets updated when XCenters are opened/closed
                                    // and/or their "reduce workarea" setting is
                                    // changed. Plain pointers, no auto-free.
static HMTX                     G_hmtxWorkAreaViews = NULLHANDLE;
                                    // mutex protecting that list;

static ULONG                    G_ulWidgetFromXY = 0;
                                    // MB1/MB2 drag about to happen?
static BOOL                     G_fSizeable = FALSE;
static PPRIVATEWIDGETVIEW         G_pViewOver = NULL;

/* ******************************************************************
 *
 *   Built-in widgets
 *
 ********************************************************************/

/*
 *@@ RegisterBuiltInWidgets:
 *      registers the built-in widget PM window classes.
 *      Gets called from ctrpCreateXCenterView on its
 *      first invocation only.
 *
 *@@changed V0.9.13 (2001-06-19) [umoeller]: added tray widget
 */

BOOL RegisterBuiltInWidgets(HAB hab)
{
    return (
                (WinRegisterClass(hab,
                                  WNDCLASS_WIDGET_OBJBUTTON,
                                  fnwpObjButtonWidget,
                                  CS_PARENTCLIP | CS_CLIPCHILDREN | CS_SIZEREDRAW | CS_SYNCPAINT,
                                  sizeof(PXCENTERWINDATA))) // additional bytes to reserve
             && (WinRegisterClass(hab,
                                  WNDCLASS_WIDGET_PULSE,
                                  fnwpPulseWidget,
                                  CS_PARENTCLIP | CS_CLIPCHILDREN | CS_SIZEREDRAW | CS_SYNCPAINT,
                                  sizeof(PXCENTERWINDATA))) // additional bytes to reserve
             && (WinRegisterClass(hab,
                                  WNDCLASS_WIDGET_TRAY,
                                  fnwpTrayWidget,
                                  CS_PARENTCLIP | CS_CLIPCHILDREN | CS_SIZEREDRAW | CS_SYNCPAINT,
                                  sizeof(PXCENTERWINDATA))) // additional bytes to reserve
           );
}

#define TRAY_WIDGET_CLASS_NAME "Tray"

/*
 * G_aBuiltInWidgets:
 *      array of the built-in widgets in src\xcenter.
 *
 *@@changed V0.9.13 (2001-06-19) [umoeller]: added tray widget
 */

static XCENTERWIDGETCLASS   G_aBuiltInWidgets[]
    = {
        // object button widget
        {
            WNDCLASS_WIDGET_OBJBUTTON,
            BTF_OBJBUTTON,
            "ObjButton",
            "Object button",
            WGTF_NOUSERCREATE | WGTF_TOOLTIP | WGTF_TRAYABLE,
            NULL        // no settings dlg
        },
        // X-button widget
        {
            WNDCLASS_WIDGET_OBJBUTTON,
            BTF_XBUTTON,
            "XButton",
            "X-Button",
            WGTF_UNIQUEPERXCENTER | WGTF_TOOLTIP,       // not trayable
            OwgtShowXButtonSettingsDlg          // V0.9.14 (2001-08-21) [umoeller]
        },
        // CPU pulse widget
        {
            WNDCLASS_WIDGET_PULSE,
            0,
            "Pulse",
            "CPU load",
            WGTF_SIZEABLE | WGTF_UNIQUEGLOBAL | WGTF_TOOLTIP, // not trayable
            NULL        // no settings dlg
        },
        // tray widget
        {
            WNDCLASS_WIDGET_TRAY,
            0,
            TRAY_WIDGET_CLASS_NAME,         // also used in strcmp in the code
            "Tray",
            WGTF_SIZEABLE | WGTF_TOOLTIP,                       // not trayable, of course
            NULL        // no settings dlg
        }
    };

/* ******************************************************************
 *
 *   Desktop workarea resizing
 *
 ********************************************************************/

/*
 *@@ LockWorkAreas:
 *      locks G_hmtxWorkAreaViews. Creates the mutex on
 *      the first call.
 *
 *      Returns TRUE if the mutex was obtained.
 *
 *@@added V0.9.7 (2001-01-18) [umoeller]
 */

BOOL LockWorkAreas(VOID)
{
    if (!G_hmtxWorkAreaViews)
        return (!DosCreateMutexSem(NULL,
                                   &G_hmtxWorkAreaViews,
                                   0,
                                   TRUE));      // request!

    return (!WinRequestMutexSem(G_hmtxWorkAreaViews, SEM_INDEFINITE_WAIT));
}

/*
 *@@ UnlockWorkAreas:
 *      the reverse to LockWorkAreas.
 *
 *@@added V0.9.7 (2001-01-18) [umoeller]
 */

VOID UnlockWorkAreas(VOID)
{
    DosReleaseMutexSem(G_hmtxWorkAreaViews);
}

/*
 *@@ ctrpDesktopWorkareaSupported:
 *      checks if changing the desktop work area is supported
 *      on this system. If so, this resolves the respective
 *      APIs from PMMERGE.DLL (on the first call only) and
 *      returns NO_ERROR, which should always be the case on
 *      Warp 4.
 *
 *      On Warp 3, this should always return ERROR_INVALID_ORDINAL.
 *
 *      Gets called from M_XCenter::wpclsInitData, thus no
 *      semaphore protection.
 *
 *@@added V0.9.7 (2001-01-18) [umoeller]
 */

APIRET ctrpDesktopWorkareaSupported(VOID)
{
    APIRET arc = NO_ERROR;

    static HMODULE      s_hmodPMMERGE = NULLHANDLE;

    if (s_hmodPMMERGE == NULLHANDLE)
    {
        // first call: load PMMERGE (we must load that again...
        // retrieving the existing module handle from the kernel
        // module table is too risky)
        CHAR        szFailure[100];
        if (!(arc = DosLoadModule(szFailure,
                                  sizeof(szFailure),
                                  "PMMERGE",
                                  &s_hmodPMMERGE)))
        {
            // alright, got it:
            arc = DosQueryProcAddr(s_hmodPMMERGE,
                                   5468,
                                   NULL,
                                   (PFN*)&G_pWinSetDesktopWorkArea);
            if (arc == NO_ERROR)
                arc = DosQueryProcAddr(s_hmodPMMERGE,
                                       5469,
                                       NULL,
                                       (PFN*)&G_pWinQueryDesktopWorkArea);

            if (arc == NO_ERROR)
            {
                lstInit(&G_llWorkAreaViews, FALSE);
                G_fWorkAreaSupported = TRUE;
            }
        }
    }

    return (arc);
}

/*
 *@@ UpdateDesktopWorkarea:
 *      updates the current desktop workarea according to
 *      the given open XCenter.
 *
 *      Gets called from an XCenter's ctrpReformatFrame.
 *
 *      This function is smart enough to update the global
 *      list of currently open XCenter views which have the
 *      "reduce workarea" setting enabled.
 *
 *      WinSetDesktopWorkArea only gets called if the
 *      workarea rectangle really has changed, and will
 *      only get called _once_ with the newly calculated
 *      work area from all active XCenters.
 *
 *      This also gets called with (fForceRemove == TRUE)
 *      when an XCenter is closed to remove that XCenter
 *      view from the list and update the desktop work area.
 *
 *      Returns TRUE only if the work area has actually changed.
 *
 *      May run on any thread.
 *
 *@@added V0.9.7 (2001-01-18) [umoeller]
 *@@changed V0.9.12 (2001-05-06) [umoeller]: fixed potential endless loop
 */

BOOL UpdateDesktopWorkarea(PXCENTERWINDATA pXCenterData,
                           BOOL fForceRemove)       // in: if TRUE, item is removed.
{
    BOOL brc = FALSE;

    if (G_fWorkAreaSupported)
    {
        // ctrpDesktopWorkareaSupported has initialized everything, so go ahead.

        BOOL fLocked = FALSE;

        TRY_LOUD(excpt1)
        {
            if (fLocked = LockWorkAreas())
            {
                XCenterData *somThis = XCenterGetData(pXCenterData->somSelf);
                BOOL        fUpdateWorkArea = (lstCountItems(&G_llWorkAreaViews) != 0);

                /*
                 *  (1)  List maintenance:
                 *
                 */

                PLISTNODE pNode = lstQueryFirstNode(&G_llWorkAreaViews);

                if (    (_fReduceDesktopWorkarea)
                     && (!fForceRemove)
                   )
                {
                    // this XCenter has "reduce workarea" enabled:
                    // it must be on the list then

                    PLISTNODE pNodeFound = NULL;

                    // _Pmpf((__FUNCTION__ ":_fReduceDesktopWorkarea enabled."));

                    while (pNode)
                    {
                        PXCENTERWINDATA pDataThis = (PXCENTERWINDATA)pNode->pItemData;
                        if (pDataThis == pXCenterData)
                        {
                            pNodeFound = pNode;
                            break;
                        }

                        pNode = pNode->pNext;
                    }

                    if (!pNodeFound)
                    {
                        // _Pmpf((__FUNCTION__ ": not on list, appending."));
                        // not on list yet:
                        lstAppendItem(&G_llWorkAreaViews,
                                      pXCenterData);
                        fUpdateWorkArea = TRUE;
                    }
                } // if ((_fReduceDesktopWorkarea) && (!fForceRemove))
                else
                {
                    // this XCenter has "reduce workarea" disabled,
                    // or "force remove" is enabled:
                    // it must not be on the list then
                    while (pNode)
                    {
                        PXCENTERWINDATA pDataThis = (PXCENTERWINDATA)pNode->pItemData;
                        if (pDataThis == pXCenterData)
                        {
                            // it's on the list:
                            // remove it then
                            lstRemoveNode(&G_llWorkAreaViews,
                                          pNode);
                            fUpdateWorkArea = TRUE;
                            break;
                        }

                        pNode = pNode->pNext;
                    }
                }

                /*
                 *  (2)  Calculate new desktop workarea:
                 *
                 */

                if (fUpdateWorkArea)
                {
                    RECTL       rclCurrent,
                                rclNew;

                    // if we have any views:
                    ULONG   ulCutBottom = 0,
                            ulCutTop = 0;

                    // get current first
                    G_pWinQueryDesktopWorkArea(HWND_DESKTOP, &rclCurrent);

                    // compose new workarea; start out with screen size
                    rclNew.xLeft = 0;
                    rclNew.yBottom = 0;
                    rclNew.xRight = WinQuerySysValue(HWND_DESKTOP, SV_CXSCREEN);
                    rclNew.yTop = WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN);
                        // the workarea APIs want inclusive rectangles...
                        // I tested this, I get 1024 and 768 for the top right.

                    // now go thru all XCenters with that setting and reduce
                    // workarea accordingly...
                    pNode = lstQueryFirstNode(&G_llWorkAreaViews);
                    while (pNode)
                    {
                        PXCENTERWINDATA pDataThat = (PXCENTERWINDATA)pNode->pItemData;

                        // only do this for XCenters which are not currently animating
                        if (pDataThat->fFrameFullyShown)
                        {
                            XCenterData *somThat = XCenterGetData(pDataThat->somSelf);
                            if (somThat->ulPosition == XCENTER_BOTTOM)
                            {
                                // XCenter on bottom:
                                if (pDataThat->cyFrame > ulCutBottom)
                                    ulCutBottom = pDataThat->cyFrame;
                            }
                            else
                                // XCenter on top:
                                if (pDataThat->cyFrame > ulCutTop)
                                    ulCutTop = pDataThat->cyFrame;

                            // pNode = pNode->pNext;
                        }

                        pNode = pNode->pNext;
                                // moved this V0.9.12 (2001-05-06) [umoeller]
                                // this might have caused endless loops
                    }

                    rclNew.yBottom += ulCutBottom;
                    rclNew.yTop -= ulCutTop;

                    if (memcmp(&rclCurrent, &rclNew, sizeof(RECTL)) != 0)
                    {
                        // rectangle has changed:

                        /*
                         *  (3)  Set new desktop workarea:
                         *
                         */

                        G_pWinSetDesktopWorkArea(HWND_DESKTOP, &rclNew);

                        brc = TRUE;
                    }
                } // if (cWorkAreaViews)
            }
        } // end if (fLocked)

        CATCH(excpt1)
        {
            brc = FALSE;
        } END_CATCH();

        if (fLocked)
            UnlockWorkAreas();

    } // end if (G_fWorkAreaSupported)

    return (brc);
}

/* ******************************************************************
 *
 *   Timer wrappers
 *
 ********************************************************************/

USHORT APIENTRY tmrStartTimer(HWND hwnd,
                              USHORT usTimerID,
                              ULONG ulTimeout)
{
    return (tmrStartXTimer(G_pLastXTimerSet,
                           hwnd,
                           usTimerID,
                           ulTimeout));
}

BOOL APIENTRY tmrStopTimer(HWND hwnd,
                           USHORT usTimerID)
{
    return (tmrStopXTimer(G_pLastXTimerSet,
                          hwnd,
                          usTimerID));
}

/* ******************************************************************
 *
 *   Widget window formatting
 *
 ********************************************************************/

/*
 *@@ ctrpBroadcastWidgetNotify:
 *      broadcasts the specified notification with
 *      WM_CONTROL to all widgets on the specified
 *      list.
 *
 *@@added V0.9.13 (2001-06-23) [umoeller]
 */

ULONG ctrpBroadcastWidgetNotify(PLINKLIST pllWidgets,   // in: linked list of PRIVATEWIDGETVIEW's
                                USHORT usNotifyCode,    // in: mp1 notify code for WM_CONTROL
                                MPARAM mp2)             // in: mp2 for WM_CONTROL
{
    ULONG ulrc = 0;
    PLISTNODE pNode;
    for (pNode = lstQueryFirstNode(pllWidgets);
         pNode;
         pNode = pNode->pNext)
    {
        PPRIVATEWIDGETVIEW pView = (PPRIVATEWIDGETVIEW)pNode->pItemData;
        WinSendMsg(pView->Widget.hwndWidget,
                   WM_CONTROL,
                   MPFROM2SHORT(ID_XCENTER_CLIENT,
                                usNotifyCode),
                   mp2);
        ulrc++;
    }

    return (ulrc);
}

/*
 *@@ CopyDisplayStyle:
 *      updates the XCenter view data struct with the data
 *      from the XCenter instance. Just a stupid wrapper
 *      because this is used from several places.
 *
 *@@added V0.9.7 (2001-01-18) [umoeller]
 */

VOID CopyDisplayStyle(PXCENTERWINDATA pXCenterData,     // target: view data
                      XCenterData *somThis)             // source: XCenter instance data
{
    PXCENTERGLOBALS pGlobals = &pXCenterData->Globals;

    pGlobals->flDisplayStyle = _flDisplayStyle;

    pGlobals->ul3DBorderWidth = _ul3DBorderWidth;       // can be 0
    pGlobals->ulBorderSpacing = _ulBorderSpacing;       // can be 0
    pGlobals->ulWidgetSpacing = _ulWidgetSpacing;

    pGlobals->ulPosition = _ulPosition;     // XCENTER_TOP or XCENTER_BOTTOM
}

/*
 *@@ ctrpPositionWidgets:
 *      positions all widget windows on the specified
 *      list.
 *
 *      This is used from two locations:
 *
 *      --  from ctrpReformat to reposition the widgets
 *          on the XCenter list;
 *
 *      --  from the tray widget to reposition subwidgets
 *          in a tray widget.
 *
 *      Preconditions:
 *
 *      --  Each widget must have been asked for its
 *          size, which must be in each widget's
 *          szlWanted field.
 *
 *      This has been extracted with 0.9.13 because it
 *      is also used by the tray widget now.
 *
 *      May only run on the XCenter GUI thread.
 *
 *@@added V0.9.13 (2001-06-19) [umoeller]
 */

ULONG ctrpPositionWidgets(PXCENTERGLOBALS pGlobals,
                          PLINKLIST pllWidgets, // in: linked list of PRIVATEWIDGETVIEW's to position
                          ULONG x,              // in: leftmost x position to start with
                          ULONG y,              // in: y position for all widgets
                          ULONG cxPerGreedy,    // in: width to give to greedy widgets
                          ULONG cyAllWidgets,   // in: height to set for all widgets
                          BOOL fShow)           // in: show widgets?
{
    ULONG   ulrc = 0,
            fl = SWP_MOVE | SWP_SIZE
                  | SWP_NOADJUST;
                      // do not give the widgets a chance to mess with us!

    PLISTNODE pNode = lstQueryFirstNode(pllWidgets);

    if (fShow)
        fl |= SWP_SHOW;

    while (pNode)
    {
        PPRIVATEWIDGETVIEW pWidgetThis = (PPRIVATEWIDGETVIEW)pNode->pItemData;

        LONG    cx = pWidgetThis->szlWanted.cx;

        if (cx == -1)
            // greedy widget:
            // use size we calculated above instead
            cx = cxPerGreedy;

        pWidgetThis->xCurrent = x;
        pWidgetThis->szlCurrent.cx = cx;
        pWidgetThis->szlCurrent.cy = cyAllWidgets;

        // _Pmpf(("  Setting widget %d, %d, %d, %d",
          //           x, 0, cx, pGlobals->cyTallestWidget));

        WinSetWindowPos(pWidgetThis->Widget.hwndWidget,
                        NULLHANDLE,
                        x,
                        y,
                        cx,
                        cyAllWidgets,
                        fl);
        WinInvalidateRect(pWidgetThis->Widget.hwndWidget, NULL, TRUE);

        x += cx + pGlobals->ulWidgetSpacing;
        if (pGlobals->flDisplayStyle & XCS_SPACINGLINES)
            x += 2;     // V0.9.13 (2001-06-19) [umoeller]

        if (    // sizing bars enabled?
                (pGlobals->flDisplayStyle & XCS_SIZINGBARS)
             && (pWidgetThis->Widget.fSizeable)
           )
        {
            // widget is sizeable: store x pos just right of the
            // widget so that WM_PAINT in the client can quickly
            // paint the sizing bar after this widget
            pWidgetThis->xSizingBar = x;
            // add space for sizing bar
            x += SIZING_BAR_WIDTH;
        }
        else
            // not sizeable:
            pWidgetThis->xSizingBar = 0;

        pNode = pNode->pNext;
        ulrc++;
    }

    return (ulrc);
}

/*
 *@@ ReformatWidgets:
 *      goes thru all widgets and repositions them according
 *      to each widget's width.
 *
 *      If (fShow == TRUE), this also shows all affected
 *      widgets, but not the frame.
 *
 *      This invalidates the client as well, since it's the
 *      client that draws the sizing bars.
 *
 *      Preconditions:
 *
 *      -- The client window must have been positioned in
 *         the frame already.
 *
 *      -- pXCenterData->Globals.cyClient must already have
 *         the client height, which must be at least
 *         pXCenterData->Globals.cyWidgetMax.
 *
 *      May only run on the XCenter GUI thread.
 */

ULONG ReformatWidgets(PXCENTERWINDATA pXCenterData,
                      BOOL fShow)               // in: show widgets?
{
    PXCENTERGLOBALS pGlobals = &pXCenterData->Globals;

    ULONG       ulrc = FALSE,
                x = 0,
                y = 0,
                cxWidgetsSpace = 0,
                cxStatics = 0,
                    // total size of static widgets
                    // (those that do not want all remaining space)
                cGreedies = 0,
                    // count of widgets that want all remaining space
                cxPerGreedy = 20;
                    // width for greedy widgets (recalc'd below)

    RECTL       rclXCenterClient;

    PLISTNODE   pNode = lstQueryFirstNode(&pXCenterData->llWidgets);

    WinQueryWindowRect(pGlobals->hwndClient,
                       &rclXCenterClient);

    // calc available space for widgets:
    // size of client rect - borders
    cxWidgetsSpace = rclXCenterClient.xRight
                      - 2 * pGlobals->ulBorderSpacing;      // can be 0

    // start at border
    x = pGlobals->ulBorderSpacing;
    y = x;

    if (pGlobals->flDisplayStyle & XCS_ALL3DBORDERS)
    {
        // all 3D borders enabled: then we have even less space
        cxWidgetsSpace -= (2 * pGlobals->ul3DBorderWidth);
        x += pGlobals->ul3DBorderWidth;
        // bottom for all widgets: same as initial X
        y = x;          // this doesn't change
    }
    else
    {
        // all 3D borders disabled: if XCenter is on top,
        // add 3D border to y then
        if (pGlobals->ulPosition == XCENTER_TOP)
            y += pGlobals->ul3DBorderWidth;
    }

    // pass 1:
    // calculate max cx of all static widgets
    // and count "greedy" widgets (that want all remaining space)
    while (pNode)
    {
        PPRIVATEWIDGETVIEW pWidgetThis = (PPRIVATEWIDGETVIEW)pNode->pItemData;

        if (pWidgetThis->szlWanted.cx == -1)
            // this widget wants all remaining space:
            cGreedies++;
        else
        {
            // static widget:
            // add its size to the size of all static widgets
            cxStatics += (pWidgetThis->szlWanted.cx + pGlobals->ulWidgetSpacing);

            // if spacing lines are enabled, add an extra 2 pixels
            if (pGlobals->flDisplayStyle & XCS_SPACINGLINES)
                cxStatics += 2;

            // if sizing bars are enabled and widget is sizeable:
            if (    (pGlobals->flDisplayStyle & XCS_SIZINGBARS)
                 && (pWidgetThis->Widget.fSizeable)
               )
                // add space for sizing bar
                cxStatics += SIZING_BAR_WIDTH;
        }

        pNode = pNode->pNext;
    }

    if (cGreedies)
    {
        // we have greedy widgets:
        // calculate space for each of them
        ULONG ulSpacing = ((cGreedies - 1) * pGlobals->ulWidgetSpacing);
        if (pGlobals->flDisplayStyle & XCS_SPACINGLINES)
            ulSpacing += 2;     // V0.9.13 (2001-06-19) [umoeller]

        cxPerGreedy = (   (cxWidgetsSpace)  // client width
                        // subtract space needed for statics:
                        - (cxStatics)
                        // subtract borders between greedy widgets:
                        - ulSpacing
                      ) / cGreedies;
    }

    // pass 2: set window positions!
    ulrc = ctrpPositionWidgets(pGlobals,
                               &pXCenterData->llWidgets,
                               x,
                               y,
                               cxPerGreedy,
                               pGlobals->cyInnerClient,
                               fShow);

    // _Pmpf((__FUNCTION__ ": leaving"));

    WinInvalidateRect(pGlobals->hwndClient, NULL, FALSE);

    return (ulrc);
}

/*
 *@@ GetWidgetSize:
 *      asks the specified widget for its desired size.
 *      If the widget does not respond, a default size is used.
 *
 *      May only run on the XCenter GUI thread.
 *
 *@@added V0.9.7 (2000-12-14) [umoeller]
 */

VOID GetWidgetSize(PPRIVATEWIDGETVIEW pWidgetThis)
{
    if (!WinSendMsg(pWidgetThis->Widget.hwndWidget,
                    WM_CONTROL,
                    MPFROM2SHORT(ID_XCENTER_CLIENT, XN_QUERYSIZE),
                    (MPARAM)&pWidgetThis->szlWanted))
    {
        // widget didn't react to this msg:
        pWidgetThis->szlWanted.cx = 10;
        pWidgetThis->szlWanted.cy = 10;
    }
}

/*
 *@@ ctrpGetWidgetSizes:
 *      calls GetWidgetSize for each widget on the list.
 *
 *      This has been extracted with 0.9.13 because it
 *      is also used by the tray widget now.
 *
 *      May only run on the XCenter GUI thread.
 *
 *@@added V0.9.13 (2001-06-19) [umoeller]
 */

VOID ctrpGetWidgetSizes(PLINKLIST pllWidgets) // in: linked list of PRIVATEWIDGETVIEW's to position
{
    PLISTNODE   pNode = lstQueryFirstNode(pllWidgets);

    // (re)get each widget's desired size
    while (pNode)
    {
        PPRIVATEWIDGETVIEW pWidgetThis = (PPRIVATEWIDGETVIEW)pNode->pItemData;

        // ask the widget for its desired size
        GetWidgetSize(pWidgetThis);

        pNode = pNode->pNext;
    }
}

/*
 *@@ ctrpQueryMaxWidgetCY:
 *      returns the height of the tallest widget
 *      on the list.
 *
 *      This has been extracted with 0.9.13 because it
 *      is also used by the tray widget now.
 *
 *      May only run on the XCenter GUI thread.
 *
 *@@added V0.9.13 (2001-06-19) [umoeller]
 */

ULONG ctrpQueryMaxWidgetCY(PLINKLIST pllWidgets) // in: linked list of PRIVATEWIDGETVIEW's to position
{
    ULONG cyMax = 0;
    PLISTNODE pNode = lstQueryFirstNode(pllWidgets);
    while (pNode)
    {
        PPRIVATEWIDGETVIEW pViewThis = (PPRIVATEWIDGETVIEW)pNode->pItemData;

        if (pViewThis->szlWanted.cy > cyMax)
            cyMax = pViewThis->szlWanted.cy;

        pNode = pNode->pNext;
    }

    return (cyMax);
}

/*
 *@@ StopAutoHide:
 *      stops all auto-hide timers.
 *
 *      May only run on the XCenter GUI thread.
 *
 *@@added V0.9.7 (2001-01-19) [umoeller]
 */

VOID StopAutoHide(PXCENTERWINDATA pXCenterData)
{
    PXCENTERGLOBALS pGlobals = &pXCenterData->Globals;

    if (pXCenterData->idTimerAutohideRun)
    {
        tmrStopXTimer((PXTIMERSET)pGlobals->pvXTimerSet,
                      pGlobals->hwndFrame,
                      TIMERID_AUTOHIDE_RUN);
        pXCenterData->idTimerAutohideRun = 0;
    }
    if (pXCenterData->idTimerAutohideStart)
    {
        tmrStopXTimer((PXTIMERSET)pGlobals->pvXTimerSet,
                      pGlobals->hwndFrame,
                      TIMERID_AUTOHIDE_START);
        pXCenterData->idTimerAutohideStart = 0;
    }
}

/*
 *@@ StartAutoHide:
 *      starts the auto-hide timer if enabled.
 *
 *      May only run on the XCenter GUI thread.
 *
 *@@added V0.9.7 (2000-12-04) [umoeller]
 *@@changed V0.9.14 (2001-08-21) [umoeller]: added "hide on click" support
 */

VOID StartAutoHide(PXCENTERWINDATA pXCenterData)
{
    PXCENTERGLOBALS pGlobals = &pXCenterData->Globals;
    XCenterData *somThis = XCenterGetData(pXCenterData->somSelf);
    if (_ulAutoHide)
    {
        // auto-hide enabled:
        // (re)start timer
        pXCenterData->idTimerAutohideStart
                = tmrStartXTimer((PXTIMERSET)pGlobals->pvXTimerSet,
                                 pGlobals->hwndFrame,
                                 TIMERID_AUTOHIDE_START,
                                 _ulAutoHide);

        if (_fHideOnClick) // V0.9.14 (2001-08-21) [umoeller]
        {
            HWND hwnd;
            if (hwnd = krnQueryDaemonObject())
                if (WinSendMsg(hwnd,
                               XDM_ADDCLICKWATCH,
                               (MPARAM)pXCenterData->Globals.hwndClient,
                               (MPARAM)XCM_MOUSECLICKED))           // msg to be used
                    // was added:
                    pXCenterData->fClickWatchRunning = TRUE;
        }
    }
    else
        // auto-hide disabled:
        // make sure timer is not running
        StopAutoHide(pXCenterData);
}

/*
 *@@ StartAutohideNow:
 *      begins auto-hiding the XCenter immediately. Call
 *      this only if auto-hide is actually enabled.
 *
 *      This was extracted from FrameTimer() with V0.9.14
 *      because we can now enforce auto-hide if "hide on click"
 *      is enabled.
 *
 *@@added V0.9.14 (2001-08-21) [umoeller]
 */

BOOL StartAutohideNow(PXCENTERWINDATA pXCenterData)
{
    PXCENTERGLOBALS pGlobals = &pXCenterData->Globals;
    PXTIMERSET pTimerSet = (PXTIMERSET)pGlobals->pvXTimerSet;

    BOOL fStart = TRUE;
    HWND hwndFocus = WinQueryFocus(HWND_DESKTOP);

    // this is only for the delay
    tmrStopXTimer(pTimerSet,
                  pGlobals->hwndFrame,
                  TIMERID_AUTOHIDE_START);
    pXCenterData->idTimerAutohideStart = 0;

    // disable auto-hide if a settings dialog is currently open
    if (pXCenterData->fShowingSettingsDlg)
        fStart = FALSE;
    // disable auto-hide if any menu is currently open
    else if (hwndFocus)
    {
        CHAR szWinClass[100];
        if (WinQueryClassName(hwndFocus, sizeof(szWinClass), szWinClass))
        {
            if (!strcmp(szWinClass, "#4"))
                // it's a menu:
                fStart = FALSE;
        }
    }

    if (fStart)
    {
        // second check: if mouse is still over XCenter,
        // forget it
        POINTL ptlMouseDtp;
        WinQueryPointerPos(HWND_DESKTOP, &ptlMouseDtp);
        if (WinWindowFromPoint(HWND_DESKTOP,
                               &ptlMouseDtp,
                               FALSE)           // no check children
                == pGlobals->hwndFrame)
            fStart = FALSE;
    }

    if (fStart)
    {
        // broadcast XN_BEGINANIMATE notification
        ctrpBroadcastWidgetNotify(&pXCenterData->llWidgets,
                                  XN_BEGINANIMATE,
                                  MPFROMSHORT(XAF_HIDE));

        pXCenterData->idTimerAutohideRun
            = tmrStartXTimer(pTimerSet,
                             pGlobals->hwndFrame,
                             TIMERID_AUTOHIDE_RUN,
                             50);
    }
    else
        StartAutoHide(pXCenterData);

    return (fStart);
}

/*
 *@@ ctrpReformat:
 *      one-shot function for refreshing the XCenter
 *      display.
 *
 *      Calling this is necessary if new widgets have
 *      been added and the max client cy might have
 *      changed, or if you want to put the XCenter back
 *      on top, or if view settings have been changed.
 *
 *      ulFlags is used to optimize repainting. This
 *      can be any combination of XFMF_* flags or 0:
 *
 *      --  0 will only re-show the frame (see below).
 *
 *      --  XFMF_DISPLAYSTYLECHANGED will recalculate
 *          everything. Specify this only if a display
 *          style (XCenter on top or bottom or animation
 *          or border sizings) really changed.
 *          This implies XFMF_GETWIDGETSIZES.
 *
 *      --  XFMF_GETWIDGETSIZES will (re)ask each widget
 *          for its desired size.
 *          This implies XFMF_RECALCHEIGHT.
 *
 *      --  XFMF_RECALCHEIGHT will recalculate the XCenter's
 *          height based on the widget heights.
 *          This implies XFMF_REPOSITIONWIDGETS.
 *
 *      --  XFMF_REPOSITIONWIDGETS will reposition all
 *          widgets inside the client.
 *
 *      --  XFMF_SHOWWIDGETS will also show all widgets.
 *          Widgets are initially invisible when the
 *          XCenter is created and will be shown only
 *          later, especially when animations are on.
 *
 *      --  XFMF_RESURFACE will put the XCenter on top
 *          of the desktop windows' Z-order.
 *
 *      As you can see, in the above list, each flag
 *      causes the next flag to be automatically enabled.
 *      As a result, the top setting (XFMF_DISPLAYSTYLECHANGED)
 *      causes a cascade of all other calculations as well.
 *      These calculations are possibly expensive, so use
 *      these flags economically.
 *      The exceptions are XFMF_SHOWWIDGETS and XFMF_RESURFACE,
 *      which are never implied.
 *
 *      If the frame is currently auto-hidden (i.e. moved
 *      mostly off the screen), calling this function will
 *      re-show it and restart the auto-hide timer, even
 *      if ulFlags is 0.
 *
 *      This function also takes care of desktop resizing
 *      if this XCenter has the "Reduce desktop workarea"
 *      flag set (and this is supported on the user's system)
 *      and the XCenter's height has changed.
 *
 *      This does _not_
 *
 *      --  change the frame's visibility flag (WS_VISIBLE);
 *          however, if the frame is already visible and
 *          XFMF_RESURFACE is set, the XCenter is put on top
 *          of the Z-order.
 *
 *      This may only run on the XCenter GUI thread. If
 *      you're not sure which thread you're on, post
 *      XCM_REFORMAT with the format flags to the client,
 *      which will in turn call this function properly.
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
 *@@changed V0.9.13 (2001-06-19) [umoeller]: extracted various functions for tray support
 *@@changed V0.9.13 (2001-06-21) [umoeller]: added XN_DISPLAYSTYLECHANGED widget broadcast
 */

VOID ctrpReformat(PXCENTERWINDATA pXCenterData,
                  ULONG ulFlags)                // in: XFMF_* flags (shared\center.h)
{
    PXCENTERGLOBALS pGlobals = &pXCenterData->Globals;
    XCenterData *somThis = XCenterGetData(pXCenterData->somSelf);

    // NOTE: We must not request the XCenter mutex here because
    // this function can get called during message sends from
    // thread 1 while thread 1 is holding the mutex. Requesting
    // the mutex here again would cause deadlocks.

    ULONG flSWP = SWP_MOVE | SWP_SIZE;

    // refresh the style bits; these might have changed
    // (mask out only the ones we're interested in changing)
    WinSetWindowBits(pGlobals->hwndFrame,
                     QWL_STYLE,
                     _ulWindowStyle,            // flags
                     WS_TOPMOST | WS_ANIMATE);  // mask

    if (ulFlags & XFMF_DISPLAYSTYLECHANGED)
    {
        // display style changed:

        // copy data from XCenter instance data to view globals...
        CopyDisplayStyle(pXCenterData, somThis);

        // broadcast XN_DISPLAYSTYLECHANGED to widgets
        // V0.9.13 (2001-06-21) [umoeller]
        ctrpBroadcastWidgetNotify(&pXCenterData->llWidgets,
                                  XN_DISPLAYSTYLECHANGED,
                                  NULL);

        ulFlags |=    XFMF_GETWIDGETSIZES;
                    // reget all widget sizes; if display style changes,
                    // widgets may wish to get different size now
    }

    if (ulFlags & XFMF_GETWIDGETSIZES)
    {
        ctrpGetWidgetSizes(&pXCenterData->llWidgets);

        ulFlags |= XFMF_RECALCHEIGHT;
                    // widget sizes might have changed, so recalc max cy
    }

    if (ulFlags & XFMF_RECALCHEIGHT)
    {
        // recalculate tallest widget
        pGlobals->cyWidgetMax = ctrpQueryMaxWidgetCY(&pXCenterData->llWidgets);

        // now we know the tallest widget... check if the client
        // height is smaller than that
        if (pGlobals->cyWidgetMax > pGlobals->cyInnerClient)
            // yes: adjust client height
            pGlobals->cyInnerClient = pGlobals->cyWidgetMax;

        // recalc cyFrame
        pXCenterData->cyFrame =   pGlobals->ul3DBorderWidth
                                + 2 * pGlobals->ulBorderSpacing
                                + pGlobals->cyInnerClient;
        if (pGlobals->flDisplayStyle & XCS_ALL3DBORDERS)
            pXCenterData->cyFrame += pGlobals->ul3DBorderWidth;

        ulFlags |= XFMF_REPOSITIONWIDGETS;
    }

    if (ulFlags & (XFMF_REPOSITIONWIDGETS | XFMF_SHOWWIDGETS))
    {
        ReformatWidgets(pXCenterData,
                        // show?
                        ((ulFlags & XFMF_SHOWWIDGETS) != 0));
    }

    if (pGlobals->ulPosition == XCENTER_TOP)
        pXCenterData->yFrame = winhQueryScreenCY() - pXCenterData->cyFrame;
    else
        pXCenterData->yFrame = 0;

    if (ulFlags & XFMF_RESURFACE)
        flSWP |= SWP_ZORDER;

    StopAutoHide(pXCenterData);

    WinSetWindowPos(pGlobals->hwndFrame,
                    HWND_TOP,       // only relevant if SWP_ZORDER was added above
                    0,
                    pXCenterData->yFrame,
                    pXCenterData->cxFrame,
                    pXCenterData->cyFrame,
                    flSWP);

    pXCenterData->fFrameAutoHidden = FALSE;
            // we must set this to FALSE before calling StartAutoHide
            // because otherwise StartAutoHide calls us again...

    if (pXCenterData->fFrameFullyShown)
        // frame is completely ready:
        // check if desktop workarea needs updating
        UpdateDesktopWorkarea(pXCenterData,
                              FALSE);           // no force remove

    // (re)start auto-hide timer
    StartAutoHide(pXCenterData);
}

/* ******************************************************************
 *
 *   Settings dialogs
 *
 ********************************************************************/

/*
 *@@ ctrpShowSettingsDlg:
 *      displays the settings dialog for a widget.
 *
 *      This gets called
 *
 *      a)  when the respective widget context menu item is selected by
 *          the user (and its WM_COMMAND is caught by ctrDefWidgetProc);
 *
 *      b)  when the widget settings dlg is requested from the "Widgets"
 *          page in an XCenter settings notebook.
 *
 *      This may be called on any thread.
 *
 *      Note that the index parameters work in two modes;
 *      see ctrpFindWidgetSetting for details.
 *
 *@@added V0.9.7 (2000-12-07) [umoeller]
 *@@changed V0.9.11 (2001-04-25) [umoeller]: rewritten, prototype changed
 *@@changed V0.9.12 (2001-05-20) [umoeller]: moved show dlg outside locks
 *@@changed V0.9.14 (2001-07-14) [lafaix]: fixed possible null dereference
 *@@changed V0.9.14 (2001-08-01) [umoeller]: this never worked for tray subwidgets, fixed (major rework)
 */

VOID ctrpShowSettingsDlg(XCenter *somSelf,
                         HWND hwndOwner,    // proposed owner of settings dlg
                         ULONG ulTrayWidgetIndex,   // in: tray widget index or -1
                         ULONG ulTrayIndex,     // in: tray index in tray widget
                         ULONG ulWidgetIndex)    // in: subwidget index in tray or root widget index
{
    // these two structures get filled by the sick code below
    WGTSETTINGSTEMP Temp = {0};
    WIDGETSETTINGSDLGDATA DlgData = {0};

    PWGTSHOWSETTINGSDLG pShowSettingsDlg = NULL;

    XCenterData *somThis = XCenterGetData(somSelf);
    PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)_pvOpenView;
                            // can be NULL if we have no open view

    WPSHLOCKSTRUCT Lock;
    TRY_LOUD(excpt1)
    {
        if (LOCK_OBJECT(Lock, somSelf))
        {
            PPRIVATEWIDGETSETTING pSetting;
            PXCENTERWIDGET pViewData;

            if (!(pSetting = ctrpFindWidgetSetting(somSelf,
                                                   ulTrayWidgetIndex,
                                                   ulTrayIndex,
                                                   ulWidgetIndex,
                                                   &pViewData)))
                cmnLog(__FILE__, __LINE__, __FUNCTION__,
                       "Cannot find widget index %d.%d.%d",
                       ulTrayWidgetIndex,
                       ulTrayIndex,
                       ulWidgetIndex);
            else
            {
                PXCENTERWIDGETCLASS pClass;

                if (    (pClass = ctrpFindClass(pSetting->Public.pszWidgetClass))
                     && (pClass->pShowSettingsDlg)
                   )
                {
                    // compose the "ugly hack" structure
                    // represented by the obscure "hSettings" field
                    // in the DlgData... this implementation is
                    // hidden from the widget, but the handle is
                    // passed to ctrSetSetupString
                    Temp.somSelf = somSelf;         // always valid
                    Temp.pSetting = pSetting;       // always valid
                    Temp.pWidget = pViewData;       // NULL if no open view

                    // compose the public dlgdata structure, which
                    // the widget will use
                    DlgData.hwndOwner = hwndOwner;
                    DlgData.pcszSetupString = pSetting->Public.pszSetupString;       // can be 0
                    *(PULONG)&DlgData.hSettings = (LHANDLE)&Temp;         // ugly hack
                    // XCenter globals: NULL if no open view
                    DlgData.pGlobals = (pXCenterData) ? &pXCenterData->Globals : NULL;
                    DlgData.pView = pViewData;      // widget view: NULL if no open view
                    DlgData.pUser = NULL;
                    DlgData.pctrSetSetupString = ctrSetSetupString;       // func pointer V0.9.9 (2001-02-06) [umoeller]

                    // set the function pointer for below
                    pShowSettingsDlg = pClass->pShowSettingsDlg;

                } // end if (pViewData->pShowSettingsDlg)
            } // end if (pClass);
        } // end if (wpshLockObject)
    }
    CATCH(excpt1) {} END_CATCH();

    if (Lock.fLocked)
        _wpReleaseObjectMutexSem(Lock.pObject);

    if (pShowSettingsDlg)
    {
        // disable auto-hide while we're showing the dlg
        if (pXCenterData)
            pXCenterData->fShowingSettingsDlg = TRUE;

        // disable the XCenter while doing this V0.9.11 (2001-04-18) [umoeller]
        if (hwndOwner)
            WinEnableWindow(hwndOwner, FALSE);

        TRY_LOUD(excpt1)
        {
            pShowSettingsDlg(&DlgData);
        }
        CATCH(excpt1) {} END_CATCH();

        if (hwndOwner)
            WinEnableWindow(hwndOwner, TRUE);

        if (pXCenterData) // V0.9.14 (2001-07-14) [lafaix]
            pXCenterData->fShowingSettingsDlg = FALSE;
    }
}

/* ******************************************************************
 *
 *   Drag'n'drop
 *
 ********************************************************************/

/*
 *@@ ctrpDrawEmphasis:
 *      draws emphasis on the XCenter client.
 *
 *      This operates in several modes...
 *
 *      If (fRemove == TRUE), emphasis is removed.
 *      In that case, all other parameters except hps
 *      are ignored. This simply repaints the client
 *      synchronously using hps.
 *
 *      Otherwise source emphasis is added....:
 *
 *      --  If (pWidget != NULL), a vertical line is
 *          painted AFTER that widget. This is to signal
 *          target emphasis between widgets.
 *
 *      --  If (pWidget == NULL && hwnd == pGlobals->hwndClient),
 *          emphasis is added to the entire XCenter client
 *          by painting a black rectangle around it. This is
 *          used for target emphasis for the entire client and
 *          for source emphasis for the main context menu.
 *
 *      --  If (pWidget == NULL) and (hwnd != pGlobals->hwndClient),
 *          hwnd is assumed to be a widget window. This then
 *          paints emphasis AROUND a widget. This is used for
 *          widget source emphasis for a widget context menu.
 *
 *      In all cases (even for remove), hpsPre _may_ be specified.
 *
 *      If (hpsPre == NULLHANDLE), we request a presentation
 *      space from the client and release it. If you specify
 *      hpsPre yourself (e.g. during dragging, using DrgGetPS),
 *      you must request it for the client, and it must be
 *      in RGB mode.
 *
 *      May only run on the XCenter GUI thread.
 *
 *@@added V0.9.7 (2000-12-07) [umoeller]
 *@@changed V0.9.7 (2001-01-18) [umoeller]: fixed wrong emphasis with new view settings
 *@@changed V0.9.9 (2001-03-10) [umoeller]: mostly rewritten for widget target emphasis
 */

VOID ctrpDrawEmphasis(PXCENTERWINDATA pXCenterData,
                      BOOL fRemove,     // in: if TRUE, invalidate.
                      PPRIVATEWIDGETVIEW pWidget, // in: widget or NULL
                      HWND hwnd,        // in: client or widget
                      HPS hpsPre)       // in: presentation space; if NULLHANDLE,
                                        // we use WinGetPS in here
{
    PXCENTERGLOBALS pGlobals = &pXCenterData->Globals;
    HPS     hps;
    BOOL    fFree = FALSE;
    if (hpsPre)
        // presentation space given: use that
        hps = hpsPre;
    else
    {
        // no presentation space given: get one
        hps = WinGetPS(pGlobals->hwndClient);
        gpihSwitchToRGB(hps);
        fFree = TRUE;
    }

    if (hps)
    {
        if (fRemove)
        {
            // remove emphasis:
            ClientPaint2(pGlobals->hwndClient, hps);
            pXCenterData->fHasEmphasis = FALSE;
        }
        else
        {
            // add emphasis:

            // before we do anything, calculate the width
            // we can use on the client
            ULONG ulClientSpacing = pGlobals->ulBorderSpacing;
            if (pGlobals->flDisplayStyle & XCS_ALL3DBORDERS)
                // then we can also draw on the 3D border
                ulClientSpacing += pGlobals->ul3DBorderWidth;

            if (pXCenterData->fHasEmphasis)
                // client has emphasis already:
                // remove that first
                ctrpDrawEmphasis(pXCenterData,
                                 TRUE,              // remove
                                 NULL,
                                 NULLHANDLE,
                                 hps);

            GpiSetColor(hps, RGBCOL_BLACK);

            if (pWidget)
            {
                // widget specified: draw vertical line before
                // that widget
                // ULONG ul;

                POINTL ptl;
                ptl.x = pWidget->xCurrent + pWidget->szlCurrent.cx;
                ptl.y = ulClientSpacing;
                GpiMove(hps, &ptl);
                ptl.x += pGlobals->ulWidgetSpacing - 1; // inclusive!
                if (pGlobals->flDisplayStyle & XCS_SPACINGLINES)
                    ptl.x += 2;     // V0.9.13 (2001-06-19) [umoeller]
                ptl.y = pXCenterData->cyFrame - ulClientSpacing - 1; // inclusive!

                GpiSetPattern(hps, PATSYM_HALFTONE);
                GpiBox(hps,
                       DRO_FILL,
                       &ptl,
                       0,
                       0);

                pXCenterData->fHasEmphasis = TRUE;
            } // end if (pWidget)
            else
            {
                RECTL   rcl;
                ULONG   ulEmphasisWidth;

                GpiSetLineType(hps, LINETYPE_DOT);
                // get window's rectangle
                WinQueryWindowRect(hwnd,
                                   &rcl);
                if (hwnd != pGlobals->hwndClient)
                    // widget window specified: convert to client coords
                    WinMapWindowPoints(hwnd,
                                       pGlobals->hwndClient,
                                       (PPOINTL)&rcl,
                                       2);

                if (hwnd != pGlobals->hwndClient)
                {
                    // widget window given:
                    // we draw emphasis _around_ the widget window
                    // on the client, so add the border spacing or
                    // the widget spacing, whichever is smaller,
                    // to the widget rect
                    ulEmphasisWidth = pGlobals->ulWidgetSpacing;
                    if (ulEmphasisWidth > ulClientSpacing)
                        ulEmphasisWidth = ulClientSpacing;

                    rcl.xLeft -= ulEmphasisWidth;
                    rcl.yBottom -= ulEmphasisWidth;
                    rcl.xRight += ulEmphasisWidth;
                    rcl.yTop += ulEmphasisWidth;
                }
                else
                    // client window given: we just use that
                    ulEmphasisWidth = ulClientSpacing;

                if (ulEmphasisWidth)
                {
                    // WinQueryWindowRect returns an inclusive-exclusive
                    // rectangle; since GpiBox uses inclusive-inclusive
                    // rectangles, fix the top right
                    rcl.xRight--;
                    rcl.yTop--;
                    gpihDrawThickFrame(hps,
                                       &rcl,
                                       ulEmphasisWidth);
                    pXCenterData->fHasEmphasis = TRUE;
                }
            }
        }

        if (fFree)
            // allocated here:
            WinReleasePS(hps);
    }
}

/*
 *@@ RemoveDragoverEmphasis:
 *      shortcut to ctrpDrawEmphasis.
 *
 *      May only run on the XCenter GUI thread.
 *
 *@@added V0.9.7 (2000-12-08) [umoeller]
 */

VOID ctrpRemoveDragoverEmphasis(HWND hwndClient)
{
    PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(hwndClient, QWL_USER);
    HPS hps = DrgGetPS(pXCenterData->Globals.hwndClient);
    if (hps)
    {
        gpihSwitchToRGB(hps);
        ctrpDrawEmphasis(pXCenterData,
                         TRUE,     // remove emphasis
                         NULL,
                         NULLHANDLE,
                         hps);
        DrgReleasePS(hps);
    }
}

/*
 *@@ ctrpDragWidget:
 *      starts dragging a widget (implementation for
 *      WM_BEGINDRAG).
 *
 *      May only run on the XCenter GUI thread.
 *
 *@@added V0.9.9 (2001-03-09) [umoeller]
 *@@changed V0.9.13 (2001-06-27) [umoeller]: fixes for tray widgets
 *@@changed V0.9.14 (2001-07-31) [lafaix]: defines a default name for the widget
 *@@changed V0.9.14 (2001-08-05) [lafaix]: widgets can be copied too, now
 */

HWND ctrpDragWidget(HWND hwnd,
                    PXCENTERWIDGET pWidget)     // in: widget or subwidget
{
    HWND hwndDrop = NULLHANDLE;
    // HWND hwndClient = pWidget->pGlobals->hwndClient;
    // PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(hwndClient, QWL_USER);
    if (pWidget)
    {
        HPS hpsScreen = WinGetScreenPS(HWND_DESKTOP);
        if (hpsScreen)
        {
            SWP swpWidget;
            RECTL rclWidget;
            HBITMAP hbmWidget;

            HWND hwndParent = WinQueryWindow(hwnd, QW_PARENT);  // XCenter client or tray

            WinQueryWindowPos(hwnd, &swpWidget);
            rclWidget.xLeft = swpWidget.x;
            rclWidget.xRight = swpWidget.x + swpWidget.cx;
            rclWidget.yBottom = swpWidget.y;
            rclWidget.yTop = swpWidget.y + swpWidget.cy;

            WinMapWindowPoints(hwndParent,
                               HWND_DESKTOP,
                               (PPOINTL)&rclWidget,
                               2);
            hbmWidget = gpihCreateBmpFromPS(WinQueryAnchorBlock(hwnd),
                                            hpsScreen,
                                            &rclWidget);

            WinReleasePS(hpsScreen);

            if (hbmWidget)
            {
                PDRAGINFO pdrgInfo = DrgAllocDraginfo(1);
                if (pdrgInfo)
                {
                    DRAGITEM  drgItem = {0};

                    drgItem.hwndItem = hwnd;       // Conversation partner
                    drgItem.ulItemID = (ULONG)pWidget;
                    drgItem.hstrType = DrgAddStrHandle(DRT_UNKNOWN);
                    // as the RMF, use our private mechanism and format
                    drgItem.hstrRMF = DrgAddStrHandle(WIDGET_DRAG_RMF);
                    drgItem.hstrContainerName = 0;
                    drgItem.hstrSourceName = 0;
                    drgItem.hstrTargetName = DrgAddStrHandle((PSZ)pWidget->pcszWidgetClass);
                    drgItem.cxOffset = 0;          // X-offset of the origin of
                                                 // the image from the pointer
                                                 // hotspot
                    drgItem.cyOffset = 0;          // Y-offset of the origin of
                                                 // the image from the pointer
                                                 // hotspot
                    drgItem.fsControl = 0;         // Source item control flags
                                                 // object is open
                    drgItem.fsSupportedOps = DO_MOVEABLE | DO_COPYABLE;

                    if (DrgSetDragitem(pdrgInfo,
                                       &drgItem,
                                       sizeof(drgItem),
                                       0))
                    {
                        DRAGIMAGE drgImage;             // DRAGIMAGE structure
                        drgImage.cb = sizeof(DRAGIMAGE);
                        drgImage.cptl = 0;
                        drgImage.hImage = hbmWidget;
                        drgImage.fl = DRG_BITMAP;
                        drgImage.cxOffset = 0;           // Offset of the origin of
                        drgImage.cyOffset = 0;           // the image from the pointer

                        // source is always XCenter client (or tray)
                        pdrgInfo->hwndSource = hwndParent;

                        hwndDrop = DrgDrag(pdrgInfo->hwndSource,
                                           pdrgInfo,      // Pointer to DRAGINFO structure
                                           &drgImage,     // Drag image
                                           1,             // Size of the pdimg array
                                           VK_ENDDRAG,    // Release of direct-manipulation
                                                          // button ends the drag
                                           NULL);         // Reserved
                                // this func is modal and does not return
                                // until things have been dropped or drag
                                // has been cancelled
                    }

                    DrgFreeDraginfo(pdrgInfo);
                }

                GpiDeleteBitmap(hbmWidget);
            }
        }
    }

    return (hwndDrop);
}

/*
 *@@ ctrpVerifyType:
 *      just like DrgVerifyType, except that it handles the WPS
 *      bug that cause line feed to be used in hstrType instead
 *      of comma.
 *
 *      Current restriction: pszType must be just one type.
 *
 *@@added V0.9.14 (2001-08-06) [lafaix]
 */

BOOL ctrpVerifyType(PDRAGITEM pdrgItem,
                    const char *pszType)
{
    BOOL  brc = FALSE;

    PSZ   pBuffer;
    ULONG ulStrlen;

    // len(hstrType) + 3 (final 0, plus leading and trailing comma)
    ulStrlen = DrgQueryStrNameLen(pdrgItem->hstrType) + 3;

    if (pBuffer = (PSZ)malloc(ulStrlen))
    {
        PSZ pNeedle;

        pBuffer[0] = ',';

        DrgQueryStrName(pdrgItem->hstrType,
                        ulStrlen,
                        pBuffer+1);

        strcat(pBuffer, ",");

        if (pNeedle = malloc(strlen(pszType)+3))
        {
            PSZ pszCur = pBuffer;
            pNeedle[0] = ',';
            strcpy(pNeedle+1, pszType);
            strcat(pNeedle, ",");

            while (*pszCur)
            {
                if (*pszCur == '\n')
                    *pszCur = ',';
                pszCur++;
            }

            brc = (strstr(pBuffer, pNeedle) != NULL);

            free(pNeedle);
        }

        free(pBuffer);
    }

    return (brc);
}

/*
 *@@ GetDragoverObjects:
 *      returns a linked list of plain WPObject* pointers
 *      for the objects being dragged over the XCenter.
 *
 *      If the returned linked list is empty (i.e., it is not
 *      null, but it contains no elements), then it means
 *      dragged over objects are all DRT_WIDGET type files.
 *
 *      The list is not in auto-free mode and must be freed
 *      by the caller.
 *
 *@@added V0.9.7 (2000-12-13) [umoeller]
 *@@changed V0.9.14 (2001-07-31) [lafaix]: fixed incorrect behavior with multiple objects
 *@@changed V0.9.14 (2001-07-31) [lafaix]: accepts copy for DRT_WIDGET type files
 */

PLINKLIST GetDragoverObjects(PDRAGINFO pdrgInfo,
                             PUSHORT pusIndicator,  // out: indicator for DM_DRAGOVER (ptr can be NULL)
                             PUSHORT pusOp)         // out: def't op for DM_DRAGOVER (ptr can be NULL)
{
    PLINKLIST pllObjects = NULL;
    // assert default drop operation (no
    // modifier keys are pressed)
    if (    (    (pdrgInfo->usOperation == DO_DEFAULT)
              || (pdrgInfo->usOperation == DO_LINK)
            )
         && (pdrgInfo->cditem)
       )
    {
        // go thru dragitems
        ULONG ul = 0;
        for (;
             ul < pdrgInfo->cditem;
             ul++)
        {
            // access that drag item
            PDRAGITEM pdrgItem = DrgQueryDragitemPtr(pdrgInfo, ul); // V0.9.14 (2001-07-31) [lafaix]
            WPObject *pobjDragged;
            if (!wpshQueryDraggedObject(pdrgItem, &pobjDragged))
            {
                // invalid object:
                if (pusIndicator)
                    *pusIndicator = DOR_NEVERDROP;
                // and stop
                break; // for
            }
            else
            {
                // valid object:
                WPObject *pReal = pobjDragged;

                if (!pllObjects)
                    // list not created yet:
                    pllObjects = lstCreate(FALSE);

                // dereference shadows
                while ((pReal) && (_somIsA(pReal, _WPShadow)))
                    pReal = _wpQueryShadowedObject(pReal,
                                                   TRUE); // lock

                if (pReal)
                    lstAppendItem(pllObjects, pReal);

                if (pusIndicator)
                    *pusIndicator = DOR_DROP;
                if (pusOp)
                    *pusOp = DO_LINK;
            }
        }
    }
    // we accept copies if they all are DTR_WIDGET files
    else if (    (pdrgInfo->usOperation == DO_COPY)
              && (pdrgInfo->cditem)
            )
    {
        ULONG ul = 0;
        for (;
             ul < pdrgInfo->cditem;
             ul++)
        {
            // access that drag item
            PDRAGITEM pdrgItem = DrgQueryDragitemPtr(pdrgInfo, ul);

            if (    (DrgVerifyRMF(pdrgItem, "DRM_OS2FILE", NULL))
                 && (ctrpVerifyType(pdrgItem, DRT_WIDGET))
               )
            {
                if (!pllObjects)
                    // list not created yet:
                    pllObjects = lstCreate(FALSE);

                if (pusIndicator)
                    *pusIndicator = DOR_DROP;
                if (pusOp)
                    *pusOp = DO_COPY;
            }
            else
            {
                // invalid object
                if (*pusIndicator)
                    *pusIndicator = DOR_NODROP;
                // and stop
                break; // for
            }
        }
    }
    else
        // non-default drop op:
        if (pusIndicator)
            *pusIndicator = DOR_NODROP;
        // but do send DM_DRAGOVER again

    return (pllObjects);
}

/*
 *@@ FindWidgetFromClientXY:
 *      attempts to find a widget from the specified mouse
 *      position (in window coordinates).
 *
 *      If hwndTrayWidget != NULLHANDLE, this searches
 *      the specified tray widget for a subwidget.
 *      The coordinates are assumed to be tray widget
 *      coordinates.              &ouml;
 *
 *      If hwndTrayWidget == NULLHANDLE, this searches
 *      the XCenter itself for a widget.
 *      The coordinates are assumed to be XCenter client
 *      coordinates.
 *
 *      --  Returns 1 if sx and sy specify coordinates
 *          which are on the right border of a (sub)widget.
 *
 *          In that case, *ppViewOver is set to that widget.
 *          *pulIndexOver receives that widget's index.
 *
 *          If that widget is sizeable, *pfIsSizable is set
 *          to TRUE, FALSE otherwise.
 *
 *      --  Returns 2 if sx and sy specify coordinates
 *          over the XCenter's sizing border.
 *          (This is the top border if the XCenter sits
 *          on the bottom of the screen, and vice versa.)
 *          In that case, none of the output fields are
 *          set.
 *
 *      --  Returns 0 if none of the above.
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
 *@@changed V0.9.13 (2001-06-21) [umoeller]: added hwndTrayWidget for tray support
 *@@changed V0.9.14 (2001-08-05) [lafaix]: added widget transparency support
 */

ULONG FindWidgetFromClientXY(PXCENTERWINDATA pXCenterData,
                             HWND hwndTrayWidget,           // in: tray widget or NULLHANDLE
                             SHORT sx,
                             SHORT sy,
                             PBOOL pfIsSizeable,              // out: TRUE if widget sizing border
                             PPRIVATEWIDGETVIEW *ppViewOver,    // out: widget mouse is over
                             PULONG pulIndexOver)             // out: that widget's index
{
    PXCENTERGLOBALS pGlobals = &pXCenterData->Globals;
    ULONG ul = 0;
    PLINKLIST pllWidgets = NULL;
    PLISTNODE pNode;

    if (hwndTrayWidget)
    {
        PPRIVATEWIDGETVIEW pView;
        if (pView = (PPRIVATEWIDGETVIEW)WinQueryWindowPtr(hwndTrayWidget, QWL_USER))
        {
            pllWidgets = pView->pllSubwidgetViews;
        }
    }
    else
    {
        // client mode: check for sizing border
        // the full height of the client has been calculated in cyFrame
        // already... pGlobals->cyInnerClient contains the "inner" client
        // minus borders/spacings only
        if (    (   (pGlobals->ulPosition == XCENTER_TOP)
                 && (sy < pGlobals->ul3DBorderWidth)
                )
             || (   (pGlobals->ulPosition == XCENTER_BOTTOM)
                 && (sy >= pXCenterData->cyFrame - pGlobals->ul3DBorderWidth)
                )
           )
            // mouse is over sizing border:
            return (2);

        // not over sizing border:
        // use the main widgets list for searching
        pllWidgets = &pXCenterData->llWidgets;
    }

    pNode = lstQueryFirstNode(pllWidgets);
    while (pNode)
    {
        PPRIVATEWIDGETVIEW pView = (PPRIVATEWIDGETVIEW)pNode->pItemData;
        ULONG   ulRightEdge =   pView->xCurrent
                              + pView->szlCurrent.cx;
        ULONG   ulSpacingThis = pGlobals->ulWidgetSpacing;
        if (pGlobals->flDisplayStyle & XCS_SPACINGLINES)
            ulSpacingThis += 2;     // V0.9.13 (2001-06-19) [umoeller]

        if (pView->xSizingBar)
            // widget is sizeable and sizing bars are on: then
            // include the sizing bar (actually between two widgets,
            // painted by the client) in the rectangle of this widget
            ulSpacingThis += SIZING_BAR_WIDTH;

        if (sx >= ulRightEdge)
        {
            if (sx <= ulRightEdge + ulSpacingThis)
            {
                // we're over the spacing border to the right of this view:
                if (pfIsSizeable)
                    *pfIsSizeable = pView->Widget.fSizeable;
                if (ppViewOver)
                    *ppViewOver = pView;
                if (pulIndexOver)
                    *pulIndexOver = ul;
                return (1);
            }
        }
        else
        if (pView->Widget.ulClassFlags & WGTF_TRANSPARENT)
        {
            // widget supports transparency and we are over a transparent
            // part of it (or else we wouldn't be there)
            // V0.9.14 (2001-08-05) [lafaix]

            if (pfIsSizeable)
                *pfIsSizeable = pView->Widget.fSizeable;
            if (ppViewOver)
                *ppViewOver = pView;

            // if we are over the left half, let's return the index of
            // the preceeding widget, so that drops can behave somewhat
            // intuitively
            if (sx <= ((pView->xCurrent + ulRightEdge + ulSpacingThis) / 2))
                ul--;
            if (pulIndexOver)
                *pulIndexOver = ul;

            return (1);
        }
        else
            break;

        pNode = pNode->pNext;
        ul++;
    }

    return (0);
}

/*
 *@@ CheckIfPresent:
 *      tests is a widget of a specified class is already present in
 *      the XCenter
 *
 *@@added V0.9.14 (2001-08-06) [lafaix]
 */

BOOL CheckIfPresent(XCenter *somSelf,
                    PXCENTERWIDGET pWidget,
                    BOOL bMove)
{
    BOOL            brc = FALSE;
    PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(pWidget->pGlobals->hwndClient,
                                                                      QWL_USER);

    if (somSelf == pXCenterData->somSelf)
    {
        // it's an operation local to the XCenter, no need to check
        // further
        brc = !bMove;
    }
    else
    {
        PLINKLIST pllWidgetSettings = ctrpQuerySettingsList(somSelf);
        PLISTNODE pNode = lstQueryFirstNode(pllWidgetSettings);

        while (pNode)
        {
            PPRIVATEWIDGETSETTING pSettingThis = (PPRIVATEWIDGETSETTING)pNode->pItemData;
            if (strcmp(pSettingThis->Public.pszWidgetClass,
                       pWidget->pcszWidgetClass)
                   == 0)
            {
                // widget of this class exists:
                brc = TRUE;
                break;
            }
            pNode = pNode->pNext;
        }
    }
    return (brc);
}

/*
 *@@ ctrpDragOver:
 *      implementation for DM_DRAGOVER in fnwpXCenterMainClient
 *      and for the tray widget as well.
 *
 *      Handles Desktop objects being dragged over the client.
 *
 *@@changed V0.9.9 (2001-03-10) [umoeller]: target emphasis was never drawn
 *@@changed V0.9.9 (2001-03-10) [umoeller]: made target emphasis clearer
 *@@changed V0.9.13 (2001-06-21) [umoeller]: renamed from ClientDragOver, added tray support
 *@@changed V0.9.14 (2001-08-05) [lafaix]: drop acceptance part rewritten
 */

MRESULT ctrpDragOver(HWND hwndClient,
                     HWND hwndTrayWidget,       // in: tray widget window or NULLHANDLE
                     PDRAGINFO pdrgInfo)
{
    PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(hwndClient, QWL_USER);
    PXCENTERGLOBALS pGlobals = &pXCenterData->Globals;
    // draw target emph?
    BOOL            fDrawTargetEmph = FALSE;
    // default return values
    USHORT          usIndicator = DOR_NEVERDROP,
                        // cannot be dropped, and don't send
                        // DM_DRAGOVER again
                    usOp = DO_UNKNOWN;
                        // target-defined drop operation:
                        // user operation (we don't want
                        // the WPS to copy anything)

    HWND hwndOver;
    if (hwndTrayWidget)
        hwndOver = hwndTrayWidget;
    else
        hwndOver = hwndClient;

    G_pWidgetBeingDragged = NULL;

    // go!
    if (DrgAccessDraginfo(pdrgInfo))
    {
        // rewrite:
        if (pdrgInfo->cditem)
        {
            PDRAGITEM pdrgItem = DrgQueryDragitemPtr(pdrgInfo, 0);

            if (    (pdrgInfo->cditem == 1)
                 && (DrgVerifyRMF(pdrgItem,
                                  WIDGET_DRAG_MECH,
                                  NULL))
                 && (    (pdrgInfo->usOperation == DO_DEFAULT)
                      || (pdrgInfo->usOperation == DO_MOVE)
                      || (pdrgInfo->usOperation == DO_COPY)
                    )
               )
            {
                // this is a widget being dragged from some XCenter
                // or tray, that's fine with us, so we accept it

                ULONG ulClassFlags = ((PXCENTERWIDGET)pdrgItem->ulItemID)->ulClassFlags;

                if  (    // trying to drop a non-trayable widget onto a tray
                         (    (hwndTrayWidget)
                           && ((ulClassFlags & WGTF_TRAYABLE) == 0)
                         )
                      || // trying to copy a unique global widget
                         (    (pdrgInfo->usOperation == DO_COPY)
                           && ((ulClassFlags & WGTF_UNIQUEGLOBAL) == WGTF_UNIQUEGLOBAL)
                         )
                      || // trying to copy or move a unique per xcenter
                         // widget with one instance already present
                         (    (ulClassFlags & WGTF_UNIQUEPERXCENTER)
                           && (CheckIfPresent(pXCenterData->somSelf,
                                              (PXCENTERWIDGET)pdrgItem->ulItemID,
                                              (pdrgInfo->usOperation != DO_COPY))
                              )
                         )
                    )
                {
                    // refuse drop, but send dragover again if a
                    // modifier changes
                    usIndicator = DOR_NODROP;
                }
                else
                {
                    G_pWidgetBeingDragged = (PPRIVATEWIDGETVIEW)pdrgItem->ulItemID;
                    fDrawTargetEmph = TRUE;
                    usIndicator = DOR_DROP;

                    if (pdrgInfo->usOperation == DO_COPY)
                        usOp = DO_COPY;
                    else
                        usOp = DO_MOVE;
                }
            }
            else
            {
                // this is something else, lets see if we can handle it
                PLINKLIST pll = GetDragoverObjects(pdrgInfo,
                                                   &usIndicator,
                                                   &usOp);

                if (pll)
                {
                    // Desktop object(s) being dragged over client:
                    fDrawTargetEmph = TRUE;
                    lstFree(&pll);
                }
            }
        }

        DrgFreeDraginfo(pdrgInfo);
    }

    if (fDrawTargetEmph)
    {
        HPS hps = DrgGetPS(pGlobals->hwndClient);       // V0.9.9 (2001-03-09) [umoeller]
        if (hps)
        {
            PPRIVATEWIDGETVIEW pViewOver = NULL;

            // convert coordinates to client
            POINTL ptlDrop;
            ptlDrop.x = pdrgInfo->xDrop;
            ptlDrop.y = pdrgInfo->yDrop;     // dtp coords
            WinMapWindowPoints(HWND_DESKTOP,
                               hwndOver,            // to client or tray widget
                               &ptlDrop,
                               1);

            gpihSwitchToRGB(hps);
            // find the widget this is being dragged over
            if (    (!FindWidgetFromClientXY(pXCenterData,
                                             hwndTrayWidget,
                                             ptlDrop.x,
                                             ptlDrop.y,
                                             NULL,
                                             &pViewOver,
                                             NULL))
                 || (!pViewOver)
               )
            {
                // not over any specified widget:
                // widget will be added to the right then...
                // draw emphasis over entire client then
                ctrpDrawEmphasis(pXCenterData,
                                 FALSE,     // draw, not remove emphasis
                                 NULL,      // no widget
                                 hwndOver,  // around client
                                 hps);
            }
            else
            {
                // draw a line between widgets to mark insertion...
                // FindWidgetFromClientXY has returned the
                // widget whose _right_ border matched the
                // coordinates...
                ctrpDrawEmphasis(pXCenterData,
                                 FALSE,     // draw, not remove emphasis
                                 pViewOver, // widget
                                 NULLHANDLE,
                                 hps);
            }

            DrgReleasePS(hps);
        }
    }

    // and return the drop flags
    return (MRFROM2SHORT(usIndicator, usOp));
}

/*
 *@@ ctrpDrop:
 *      implementation for DM_DROP in fnwpXCenterMainClient
 *      and for the tray widget as well.
 *
 *      Creates object button widgets for Desktop objects dropped
 *      on the client.
 *
 *@@changed V0.9.9 (2001-02-08) [umoeller]: fixed wrong leftmost widget add
 *@@changed V0.9.13 (2001-06-21) [umoeller]: renamed from ClientDrop, added tray support
 *@@changed V0.9.13 (2001-06-21) [umoeller]: tooltips never worked for widgets which were added later, fixed
 *@@changed V0.9.14 (2001-07-31) [lafaix]: accepts DRT_WIDGET drops too
 *@@changed V0.9.14 (2001-08-05) [lafaix]: widget drop part rewritten
 */

VOID ctrpDrop(HWND hwndClient,          // in: XCenter client
              HWND hwndTrayWidget,      // in: tray widget window or NULLHANDLE
              PDRAGINFO pdrgInfo)
{
    PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(hwndClient, QWL_USER);
    PXCENTERGLOBALS pGlobals = &pXCenterData->Globals;

    HWND hwndOver;
    if (hwndTrayWidget)
        hwndOver = hwndTrayWidget;
    else
        hwndOver = hwndClient;

    if (DrgAccessDraginfo(pdrgInfo))
    {
        PPRIVATEWIDGETVIEW pViewOver = NULL;
        ULONG ulIndex = 0;

        // convert coordinates to client
        POINTL ptlDrop;
        ptlDrop.x = pdrgInfo->xDrop;
        ptlDrop.y = pdrgInfo->yDrop;     // dtp coords
        WinMapWindowPoints(HWND_DESKTOP,
                           hwndOver,            // to client
                           &ptlDrop,
                           1);

        if (    (!FindWidgetFromClientXY(pXCenterData,
                                         hwndTrayWidget,
                                         ptlDrop.x,
                                         ptlDrop.y,
                                         NULL,
                                         &pViewOver,
                                         &ulIndex))
             || (!pViewOver)
           )
                // ulIndex = 0;        // leftmost
                // WROOOONG V0.9.9 (2001-02-08) [umoeller]
                ulIndex = -1;        // rightmost
            else
                // FindWidgetFromClientXY has returned the
                // widget whose _right_ border matched the
                // coordinates... however, we must specify
                // the index _before_ which we want to insert
                ulIndex++;

        if (G_pWidgetBeingDragged)
        {
            // ctrpDragOver found a widget being dragged, so we
            // face two possibilities: (1) if the widget is a
            // top-level one, and is being moved to some other
            // position within the same XCenter, we simply move
            // it; (2) in all other cases, we simply copy it
            // to its new location, and remove it from its
            // initial position if a move operation is going on.
            // V0.9.14 (2001-08-05) [lafaix]
            ULONG ulTrayWidgetIndex = 0,
                  ulTrayIndex = 0,
                  ulWidgetIndex = 0;

            if (    (hwndTrayWidget == NULLHANDLE)
                 && (pdrgInfo->usOperation == DO_MOVE)
                 && (hwndClient == WinQueryWindow(G_pWidgetBeingDragged->Widget.hwndWidget,
                                                  QW_PARENT))
               )
            {
                // case (1): this is an optimization (i.e., case 2 could
                // handle it just fine, but calling xwpMoveWidget is faster)
                if (ctrpQueryWidgetIndexFromHWND(pXCenterData->somSelf,
                                                 G_pWidgetBeingDragged->Widget.hwndWidget,
                                                 &ulTrayWidgetIndex,
                                                 &ulTrayIndex,
                                                 &ulWidgetIndex))
                    _xwpMoveWidget(pXCenterData->somSelf,
                                   ulWidgetIndex,
                                   ulIndex);
            }
            else
            {
                // case (2): generic handling
                PXCENTERWINDATA pSourceXCenterData =
                    (PXCENTERWINDATA)WinQueryWindowPtr(G_pWidgetBeingDragged->Widget.pGlobals->hwndClient,
                                                       QWL_USER);

                if (ctrpQueryWidgetIndexFromHWND(pSourceXCenterData->somSelf,
                                                 G_pWidgetBeingDragged->Widget.hwndWidget,
                                                 &ulTrayWidgetIndex,
                                                 &ulTrayIndex,
                                                 &ulWidgetIndex))
                {
                    PPRIVATEWIDGETSETTING pSetting;

                    if ((pSetting = ctrpFindWidgetSetting(pSourceXCenterData->somSelf,
                                                          ulTrayWidgetIndex,
                                                          ulTrayIndex,
                                                          ulWidgetIndex,
                                                          NULL)))
                    {
                        PSZ pszClass = (PSZ)G_pWidgetBeingDragged->Widget.pcszWidgetClass;
                        PSZ pszSetup = pSetting->Public.pszSetupString;

                        if (hwndTrayWidget)
                        {
                            // we are copying a widget to a tray; we hence
                            // must build a temporary buffer to hold the
                            // packed widget setup definition

                            PSZ pszBuff = (PSZ)malloc(   strlen(pszClass)
                                                       + ((pszSetup)
                                                              ? strlen(pszSetup)
                                                              : 0 )
                                                       + 3); // "\r\n" + final 0

                            if (pszBuff)
                            {
                                strcpy(pszBuff, pszClass);
                                strcat(pszBuff, "\r\n");
                                if (pszSetup)
                                    strcat(pszBuff, pszSetup);

                                WinSendMsg(hwndTrayWidget,
                                           XCM_CREATESUBWIDGET,
                                           (MPARAM)pszBuff,
                                           (MPARAM)ulIndex);
                                free(pszBuff);
                            }
                        }
                        else
                            _xwpInsertWidget(pXCenterData->somSelf,
                                             ulIndex,
                                             pszClass,
                                             pszSetup);
                    }

                    // if the operation was a move, remove the
                    // widget from its initial location
                    // (No need to invalidate the source XCenter,
                    // this will do that too.)
                    if (pdrgInfo->usOperation == DO_MOVE)
                        WinSendMsg(G_pWidgetBeingDragged->Widget.hwndWidget,
                                   DM_DISCARDOBJECT,
                                   NULL,
                                   NULL);
                }
            }
        }
        else
        {
            PLINKLIST pll = GetDragoverObjects(pdrgInfo,
                                               NULL,
                                               NULL);
            PLISTNODE pNode = lstQueryFirstNode(pll);
            WPObject *pObjDragged;
            HOBJECT hobjDragged;

            // two possibilities here: (1) if pll contains no elements,
            // then DRT_WIDGETS files were dropped.  Otherwise, (2),
            // objects were requesting DO_LINK.

            if (lstCountItems(pll) == 0)
            {
                // case (1): create widgets for the files being
                // dropped
                ULONG ul = 0;

                for (;
                     ul < pdrgInfo->cditem;
                     ul++)
                {
                    // access that drag item, and create the new widget
                    PDRAGITEM pdrgItem = DrgQueryDragitemPtr(pdrgInfo, ul);
                    CHAR        achCnr[CCHMAXPATH],
                                achSrc[CCHMAXPATH];

                    // the dnd part guaranties the following
                    // will not overflow
                    DrgQueryStrName(pdrgItem->hstrContainerName,
                                    CCHMAXPATH,
                                    achCnr);
                    DrgQueryStrName(pdrgItem->hstrSourceName,
                                    CCHMAXPATH,
                                    achSrc);

                    if ((strlen(achCnr)+strlen(achSrc)) < (CCHMAXPATH-1))
                    {
                        CHAR achAll[CCHMAXPATH];
                        PSZ  pszBuff;

                        strcpy(achAll, achCnr);
                        strcat(achAll, achSrc);

                        ctrpReadFromFile(achAll, &pszBuff);

                        if (pszBuff)
                        {
                            PSZ pszSetupString = strstr(pszBuff, "\r\n");

                            if (pszSetupString)
                            {
                                if (hwndTrayWidget)
                                {
                                    WinSendMsg(hwndTrayWidget,
                                               XCM_CREATESUBWIDGET,
                                               (MPARAM)pszBuff,
                                               (MPARAM)ulIndex);
                                }
                                else
                                {
                                    *pszSetupString = 0;
                                    pszSetupString += 2;
                                    _xwpInsertWidget(pXCenterData->somSelf,
                                                     ulIndex,
                                                     pszBuff,
                                                     pszSetupString);
                                }
                            }

                            free(pszBuff);
                        }
                    }
                }
            }
            else
            {
                // case (2): insert object button widgets for the objects
                // being dropped; this sets up the object further
                while (pNode)
                {
                    if (    (pObjDragged = (WPObject*)pNode->pItemData)
                         && (hobjDragged = _wpQueryHandle(pObjDragged))
                       )
                    {
                        CHAR szSetup[100];
                        sprintf(szSetup, "OBJECTHANDLE=%lX;", hobjDragged);

                        if (hwndTrayWidget)
                        {
                            WinSendMsg(hwndTrayWidget,
                                       XCM_CREATEOBJECTBUTTON,
                                       (MPARAM)szSetup,
                                       (MPARAM)ulIndex);
                        }
                        else
                            // client mode: insert as main widget
                            _xwpInsertWidget(pXCenterData->somSelf,
                                             ulIndex,
                                             "ObjButton",    // widget class
                                             szSetup);
                    }

                    // next object
                    pNode = pNode->pNext;
                }
            }

            lstFree(&pll);
        }

        DrgFreeDraginfo(pdrgInfo);
    }

    WinInvalidateRect(pGlobals->hwndClient, NULL, FALSE);
}

/* ******************************************************************
 *
 *   Widget window creation
 *
 ********************************************************************/

/*
 *@@ ctrpCreateWidgetWindow:
 *      creates a new widget window from the specified
 *      PRIVATEWIDGETSETTING.
 *
 *      Gets called
 *
 *      --  from CreateWidgets and ctrpInsertWidget to create
 *          a widget as a child of the XCenter client;
 *
 *      --  from the Tray widget to create a subwidget in a
 *          tray.
 *
 *      This is the only place that actually calls
 *      WinCreateWindow to create a widget window.
 *      In addition, this creates the PRIVATEWIDGETVIEW
 *      and gives the member XCENTERWIDGET to the new
 *      widget with WM_CREATE.
 *
 *      Postconditions:
 *
 *      -- The position of the widget is arbitrary after
 *         this, and the widget window is NOT shown (WS_VISIBLE
 *         not set). The caller must call ReformatWidgets
 *         afterwards and make the widget visible.
 *
 *      -- This raises pXCenterData->Globals.cyClient
 *         if the new widget wants a height larger
 *         than that.
 *
 *      -- This returns a PRIVATEWIDGETVIEW which was malloc'd.
 *         This structure is inserted into the specified
 *         LINKLIST at the specified position.
 *         ctrDefWidgetProc takes care of cleanup.
 *
 *      May only run on the XCenter GUI thread.
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
 *@@changed V0.9.12 (2001-05-20) [umoeller]: added classes mutex protection
 *@@changed V0.9.13 (2001-06-09) [pr]: fixed context menu font
 *@@changed V0.9.13 (2001-06-19) [umoeller]: prototype changed for tray support
 *@@changed V0.9.13 (2001-06-21) [umoeller]: fixed tooltips
 */

PPRIVATEWIDGETVIEW ctrpCreateWidgetWindow(PXCENTERWINDATA pXCenterData,      // in: instance data
                                        PPRIVATEWIDGETVIEW pOwningTray,      // in: owning tray or NULL if root
                                        PLINKLIST pllWidgetViews,          // in: linked list to append to
                                                                           // (either in XCenter or tray data)
                                        PPRIVATEWIDGETSETTING pSetting,    // in: widget setting to create from
                                        ULONG ulIndex)                     // in: index of this widget (or -1)
{
    PPRIVATEWIDGETVIEW pNewView = NULL;

    BOOL fLocked = FALSE;
    TRY_LOUD(excpt2)
    {
        if (fLocked = ctrpLockClasses())
        {
            PXCENTERGLOBALS pGlobals = &pXCenterData->Globals;

            // NOTE: We must not request the XCenter mutex here because
            // this function can get called during message sends from
            // thread 1 while thread 1 is holding the mutex. Requesting
            // the mutex here again would cause deadlocks.

            if (!pSetting)
                cmnLog(__FILE__, __LINE__, __FUNCTION__,
                       "PPRIVATEWIDGETSETTING is NULL.");
            else
            {
                pNewView = NEW(PRIVATEWIDGETVIEW);
                                // this includes the public XCENTERWIDGET struct
                if (pNewView)
                {
                    // get ptr to XCENTERWIDGET struct
                    PXCENTERWIDGET pWidget = &pNewView->Widget;
                    // find the widget class for this
                    PXCENTERWIDGETCLASS pWidgetClass
                        = ctrpFindClass(pSetting->Public.pszWidgetClass);

                    ZERO(pNewView);

                    if (pWidgetClass)
                    {
                        // get the parent window and the list to append to,
                        // depending on whether this is in a tray or not
                        HWND      hwndParent;

                        if (pOwningTray)
                            // this is part of a tray:
                            // parent is tray widget
                            hwndParent = pOwningTray->Widget.hwndWidget;
                        else
                            // "root" widget: parent is XCenter client
                            hwndParent = pGlobals->hwndClient;

                        // set private data
                        pNewView->xCurrent = 0;         // changed later
                        pNewView->szlWanted.cx = 20;
                        pNewView->szlWanted.cy = 20;

                        pNewView->pOwningTray = pOwningTray;

                        // set public XCENTERWIDGET data
                        pWidget->habWidget = WinQueryAnchorBlock(pGlobals->hwndClient);

                        pWidget->pfnwpDefWidgetProc = ctrDefWidgetProc;

                        pWidget->pGlobals = pGlobals;

                        pWidget->pWidgetClass = pWidgetClass;
                        pWidget->pcszWidgetClass = strdup(pWidgetClass->pcszWidgetClass);
                                    // cleaned up in WM_DESTROY of ctrDefWidgetProc
                        pWidget->ulClassFlags = pWidgetClass->ulClassFlags;

                        pWidget->fSizeable = ((pWidgetClass->ulClassFlags & WGTF_SIZEABLE) != 0);

                        pWidget->pcszSetupString = pSetting->Public.pszSetupString;
                                    // can be NULL

                        // set the hack for the tray widget so it can
                        // access the subwidgets
                        pWidget->pvWidgetSetting = pSetting;
                                // V0.9.13 (2001-06-21) [umoeller]

                        pWidget->hwndWidget = WinCreateWindow(hwndParent, // V0.9.13 (2001-06-19) [umoeller]
                                                              (PSZ)pWidgetClass->pcszPMClass,
                                                              "",        // title
                                                              0, // WS_VISIBLE,
                                                              // x, y:
                                                              0,
                                                              0,
                                                              // cx, cy:
                                                              20,
                                                              20,
                                                              // owner:
                                                              hwndParent, // pGlobals->hwndClient,
                                                              HWND_TOP,
                                                              ulIndex,                // ID
                                                              pWidget, // pNewView,
                                                              0);              // presparams
                                // this sends WM_CREATE to the new widget window

                        // clean up setup string
                        pWidget->pcszSetupString = NULL;

                        // unset widget class ptr; this is valid only during WM_CREATE
                        pWidget->pWidgetClass = NULL;

                        if (pWidget->hwndWidget)
                        {
                            // V0.9.13 (2001-06-09) [pr]
                            PSZ pszStdMenuFont;
                            if (!(pszStdMenuFont = prfhQueryProfileData(HINI_USER,
                                                                        PMINIAPP_SYSTEMFONTS, // "PM_SystemFonts",
                                                                        PMINIKEY_MENUSFONT, // "Menus",
                                                                        NULL)))
                                pszStdMenuFont = prfhQueryProfileData(HINI_USER,
                                                                      PMINIAPP_SYSTEMFONTS, // "PM_SystemFonts",
                                                                      PMINIKEY_DEFAULTFONT, // "DefaultFont",
                                                                      NULL);
                                        // @@todo this still doesn't work on eCS

                            // store view data in widget's QWL_USER,
                            // in case the widget forgot; but this won't help
                            // much since the widget gets messages before WinCreateWindow
                            // returns....
                            WinSetWindowPtr(pWidget->hwndWidget, QWL_USER, pNewView);

                            // ask the widget for its size
                            GetWidgetSize(pNewView);
                            if (pNewView->szlWanted.cy > pGlobals->cyWidgetMax)
                                pGlobals->cyWidgetMax = pNewView->szlWanted.cy;
                            // now we know the tallest widget... check if the client
                            // height is smaller than that V0.9.9 (2001-03-09) [umoeller]
                            if (pGlobals->cyWidgetMax > pGlobals->cyInnerClient)
                                // yes: adjust client height
                                pGlobals->cyInnerClient = pGlobals->cyWidgetMax;

                            // load standard context menu
                            pWidget->hwndContextMenu = WinLoadMenu(pWidget->hwndWidget,
                                                                   cmnQueryNLSModuleHandle(FALSE),
                                                                   ID_CRM_WIDGET);
                            if (pszStdMenuFont)
                            {
                                // set a font presparam for this menu because
                                // otherwise it will inherit the widget's font
                                // presparam (duh)
                                winhSetWindowFont(pWidget->hwndContextMenu,
                                                  pszStdMenuFont);
                                free(pszStdMenuFont);
                            }

                            // store view
                            if (    (ulIndex == -1)
                                 || (ulIndex >= lstCountItems(pllWidgetViews))
                               )
                                // append at the end:
                                lstAppendItem(pllWidgetViews,
                                              pNewView);
                            else
                                lstInsertItemBefore(pllWidgetViews,
                                                    pNewView,
                                                    ulIndex);

                            // does this widget want a tooltip?
                            // (moved this here from ctrp_fntXCenter
                            // V0.9.13 (2001-06-21) [umoeller])
                            if (pWidgetClass->ulClassFlags & WGTF_TOOLTIP)
                            {
                                // yes: add the widget as a tool to
                                // XCenter's tooltip control
                                TOOLINFO ti = {0};

                                ti.hwndToolOwner = pGlobals->hwndClient;
                                ti.pszText = PSZ_TEXTCALLBACK;  // send TTN_NEEDTEXT

                                ti.hwndTool = pNewView->Widget.hwndWidget;

                                if (pGlobals->ulPosition == XCENTER_BOTTOM)
                                    ti.ulFlags =   TTF_POS_Y_ABOVE_TOOL
                                                 | TTF_SUBCLASS;
                                else
                                    ti.ulFlags =   TTF_POS_Y_BELOW_TOOL
                                                 | TTF_SUBCLASS;
                                if (  (0 == (  pNewView->Widget.ulClassFlags
                                             & WGTF_TOOLTIP_AT_MOUSE)
                                   ))
                                    ti.ulFlags |= TTF_CENTER_X_ON_TOOL;

                                // add tool to tooltip control
                                WinSendMsg(pGlobals->hwndTooltip,
                                           TTM_ADDTOOL,
                                           (MPARAM)0,
                                           &ti);
                            } // end V0.9.13 (2001-06-21) [umoeller]

                        } // end if (pWidget->hwndWidget)
                    } // end if pWidgetClass

                    if (!pWidget->hwndWidget)
                    {
                        free(pNewView);
                        pNewView = NULL;
                    }

                } // end if pNewView
            } // end if pSetting
        }
    }
    CATCH(excpt2) {} END_CATCH();

    if (fLocked)
        ctrpUnlockClasses();

    return (pNewView);
}

/*
 *@@ CreateWidgets:
 *      gets called from ctrpCreateXCenterView to create
 *      the widgets for that instance when the XCenter is
 *      initially opened.
 *
 *      This also resizes the frame window, if any of the widgets
 *      has indicated that it needs a larger height than the frame's
 *      client currently has. This does not change the frame's
 *      visibility though.
 *
 *      May only run on the XCenter GUI thread.
 *
 *      Preconditions:
 *
 *      -- The client window must have been positioned in
 *         the frame already.
 *
 *      -- The caller must have requested the XCenter mutex.
 *
 *@@changed V0.9.9 (2001-02-01) [umoeller]: added "remove setting" on errors
 */

ULONG CreateWidgets(PXCENTERWINDATA pXCenterData)
{
    ULONG   ulrc = 0;

    // NOTE: We must not request the XCenter mutex here because
    // this function can get called during message sends from
    // thread 1 while thread 1 is holding the mutex. Requesting
    // the mutex here again would cause deadlocks.

    // ULONG   ul = 0;

    PLINKLIST pllWidgetSettings = ctrpQuerySettingsList(pXCenterData->somSelf);

    PLISTNODE pNode = lstQueryFirstNode(pllWidgetSettings);

    pXCenterData->Globals.cyWidgetMax = 0;     // raised by ctrpCreateWidgetWindow

    while (pNode)
    {
        PPRIVATEWIDGETSETTING pSetting = (PPRIVATEWIDGETSETTING)pNode->pItemData;
        PLISTNODE pNodeSaved = pNode;
        pNode = pNode->pNext;

        if (ctrpCreateWidgetWindow(pXCenterData,
                                   NULL,        // no owning tray
                                   &pXCenterData->llWidgets,
                                   pSetting,
                                   -1))        // at the end
            ulrc++;
        else
        {
            // error creating window: V0.9.9 (2001-02-01) [umoeller]
            PSZ     apsz[2];
            apsz[0] = (PSZ)pSetting->Public.pszWidgetClass;
            if (cmnMessageBoxMsgExt(NULLHANDLE,
                                    194,        // XCenter Error
                                    apsz,
                                    1,
                                    195,
                                    MB_YESNO)
                == MBID_YES)
            {
                // remove:
                lstRemoveNode(pllWidgetSettings, pNodeSaved);
                _wpSaveDeferred(pXCenterData->somSelf);
            }
        }
    } // end for widgets

    // this is the first "reformat frame" when the XCenter
    // is created, so get max cy and reposition
    ctrpReformat(pXCenterData,
                 XFMF_DISPLAYSTYLECHANGED);

    return (ulrc);
}

/*
 *@@ DestroyWidgets:
 *      reversely to CreateWidgets, this cleans up.
 *
 *      May only run on the XCenter GUI thread.
 *
 *      This does not affect the PRIVATEWIDGETSETTINGS.
 *
 *@@changed V0.9.9 (2001-03-09) [umoeller]: fixed list maintenance
 */

VOID DestroyWidgets(PXCENTERWINDATA pXCenterData)
{
    PLISTNODE pNode = lstQueryFirstNode(&pXCenterData->llWidgets);
    while (pNode)
    {
        PLISTNODE pNext = pNode->pNext;
                    // fixed V0.9.9 (2001-03-09) [umoeller];
                    // ctrDefWidgetProc alters the list
        PPRIVATEWIDGETVIEW pView = (PPRIVATEWIDGETVIEW)pNode->pItemData;

        WinDestroyWindow(pView->Widget.hwndWidget);
                // the window is responsible for cleaning up pView->pUser;
                // ctrDefWidgetProc will also free pView and remove it
                // from the widget views list

        pNode = pNext;
    }

    lstClear(&pXCenterData->llWidgets);
        // the list is in auto-free mode
}

/*
 *@@ SetWidgetSize:
 *      implementation for XCM_SETWIDGETSIZE in
 *      fnwpXCenterMainClient. Also gets called
 *      from other locations.
 *
 *      May only run on the XCenter GUI thread.
 */

BOOL SetWidgetSize(PXCENTERWINDATA pXCenterData,
                   HWND hwndWidget,
                   ULONG ulNewWidth)
{
    BOOL brc = FALSE;

    PLISTNODE pNode = lstQueryFirstNode(&pXCenterData->llWidgets);
    while (pNode)
    {
        PPRIVATEWIDGETVIEW pWidgetThis = (PPRIVATEWIDGETVIEW)pNode->pItemData;
        if (pWidgetThis->Widget.hwndWidget == hwndWidget)
        {
            // found it:
            // set new width
            pWidgetThis->szlWanted.cx = ulNewWidth;

            ctrpReformat(pXCenterData,
                         XFMF_RECALCHEIGHT | XFMF_REPOSITIONWIDGETS);
            brc = TRUE;
            break;
        }

        pNode = pNode->pNext;
    }

    return (brc);
}

/* ******************************************************************
 *
 *   XCenter frame window proc
 *
 ********************************************************************/

/*
 *@@ FrameCommand:
 *      implementation for WM_COMMAND in fnwpXCenterMainFrame.
 *      This handles "Close" and "Add widget" items.
 *
 *      Returns TRUE if the msg was processed. On FALSE,
 *      the parent winproc should be called.
 */

BOOL FrameCommand(HWND hwnd, USHORT usCmd)
{
    BOOL                fProcessed = TRUE;
    PXCENTERWINDATA     pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER);
    // PCGLOBALSETTINGS    pGlobalSettings = cmnQueryGlobalSettings();
    // XCenterData         *somThis = XCenterGetData(pXCenterData->somSelf);

    switch (usCmd)
    {
        case WPMENUID_CLOSE:
            // our custom "close" menu item
            WinPostMsg(hwnd, WM_SYSCOMMAND, (MPARAM)SC_CLOSE, 0);
        break;

        default:
        {
            // is it one of the items in the "add widget" submenu?
            PPRIVATEWIDGETCLASS pClass;
            if (pClass = ctrpFindClassFromMenuCommand(usCmd))
            {
                _xwpInsertWidget(pXCenterData->somSelf,
                                 -1,        // at the end
                                 (PSZ)pClass->Public.pcszWidgetClass,
                                 NULL);     // no setup string yet
            }
            else
                // some other command (probably WPS menu items):
                fProcessed = FALSE;
        }
        break;
    }

    return (fProcessed);
}

/*
 *@@ FrameTimer:
 *      implementation for WM_TIMER in fnwpXCenterMainFrame.
 *
 *      Returns TRUE if the msg was processed. On FALSE,
 *      the parent winproc should be called.
 *
 *@@changed V0.9.7 (2000-12-08) [umoeller]: got rid of dtGetULongTime
 *@@changed V0.9.9 (2001-02-28) [umoeller]: adjusted for new helpers\timer.c
 *@@changed V0.9.13 (2001-06-23) [lafaix]: added XN_{BEGIN|END}ANIMATE support
 */

BOOL FrameTimer(HWND hwnd,
                PXCENTERWINDATA pXCenterData,
                USHORT usTimerID)
{
    PXCENTERGLOBALS pGlobals = &pXCenterData->Globals;
    PXTIMERSET pTimerSet = (PXTIMERSET)pGlobals->pvXTimerSet;

    BOOL brc = TRUE;            // processed

    switch (usTimerID)
    {
        /*
         * TIMERID_XTIMERSET:
         *      this is the PM timer for the XTIMERSET.
         */

        case TIMERID_XTIMERSET:
            tmrTimerTick(pTimerSet);
        break;

        /*
         * TIMERID_UNFOLDFRAME:
         *      animation timer for unfolding the frame
         *      when the XCenter is first opened.
         */

        case TIMERID_UNFOLDFRAME:
        {
            ULONG cxCurrent = 0;
            ULONG ulNowTime = doshQuerySysUptime();
            if (pXCenterData->ulStartTime == 0)
            {
                // first call:
                pXCenterData->ulStartTime = ulNowTime;
                // initial value for XCenter:
                // start with a square
                cxCurrent = pXCenterData->cyFrame;
            }
            else
            {
                ULONG ulMSPassed = ulNowTime - pXCenterData->ulStartTime;

                // total animation should last two seconds,
                // so check where we are now compared to
                // the time the animation started
                cxCurrent = (pXCenterData->cxFrame) * ulMSPassed / MAX_UNFOLDFRAME;

                if (cxCurrent >= pXCenterData->cxFrame)
                {
                    // we're on the last step:
                    cxCurrent = pXCenterData->cxFrame;

                    // stop this timer
                    tmrStopXTimer(pTimerSet,
                                  hwnd,
                                  usTimerID);
                    // start second timer for showing the widgets
                    tmrStartXTimer(pTimerSet,
                                   hwnd,
                                   TIMERID_SHOWWIDGETS,
                                   100);
                    pXCenterData->ulStartTime = 0;
                }

                WinSetWindowPos(hwnd,
                                HWND_TOP,
                                0,
                                pXCenterData->yFrame,
                                cxCurrent,
                                pXCenterData->cyFrame,
                                SWP_MOVE | SWP_SIZE | SWP_SHOW | SWP_ZORDER);
            }

        break; } // case TIMERID_UNFOLDFRAME

        /*
         * TIMERID_SHOWWIDGETS:
         *      animation timer for successively displaying
         *      the widgets after the frame has unfolded.
         */

        case TIMERID_SHOWWIDGETS:
        {
            PPRIVATEWIDGETVIEW pWidgetThis
                = (PPRIVATEWIDGETVIEW)lstItemFromIndex(&pXCenterData->llWidgets,
                                                     (pXCenterData->ulWidgetsShown)++);
            if (pWidgetThis)
                WinShowWindow(pWidgetThis->Widget.hwndWidget, TRUE);
            else
            {
                // XCenterData *somThis = XCenterGetData(pXCenterData->somSelf);
                // no more widgets: we're done
                tmrStopXTimer(pTimerSet,
                              hwnd,
                              usTimerID);
                pXCenterData->ulWidgetsShown = 0;

                // OK, XCenter is ready now.
                pXCenterData->fFrameFullyShown = TRUE;

                // invalidate the client so it can paint the sizing bars
                WinInvalidateRect(pGlobals->hwndClient, NULL, FALSE);

                // resize the desktop
                UpdateDesktopWorkarea(pXCenterData, FALSE);
                // start auto-hide now
                StartAutoHide(pXCenterData);
            }
        break; }

        /*
         * TIMERID_AUTOHIDE_START:
         *      delay timer if _ulAutoHide > 0, which is
         *      a one-shot timer (mostly) to delay hiding
         *      the XCenter frame. Started exclusively by
         *      StartAutoHide().
         */

        case TIMERID_AUTOHIDE_START:
            StartAutohideNow(pXCenterData);
        break;

        /*
         * TIMERID_AUTOHIDE_RUN:
         *      animation timer which actually hides the
         *      frame once TIMERID_AUTOHIDE_START has
         *      elapsed.
         */

        case TIMERID_AUTOHIDE_RUN:
        {
            LONG cySubtractCurrent;
                        // cySubtractCurrent receives the currently visible
                        // width; this starts out with the full frame width

            if (pXCenterData->ulStartTime == 0)
            {
                // first call:
                pXCenterData->ulStartTime = doshQuerySysUptime();
                // initially, subtract nothing
                cySubtractCurrent = 0;
            }
            else
            {
                XCenterData *somThis = XCenterGetData(pXCenterData->somSelf);
                ULONG   ulNowTime = doshQuerySysUptime();
                ULONG   cySubtractMax;
                ULONG   ulMinSize = pGlobals->ul3DBorderWidth;
                if (ulMinSize == 0)
                    // if user has set 3D border width to 0,
                    // we must not use that, or the frame will
                    // disappear completely...
                    ulMinSize = 1;

                // cySubtractMax has the width that should be
                // subtracted from the full frame width to
                // get only a single dlg frame
                cySubtractMax =   pXCenterData->cyFrame
                                - ulMinSize;

                // total animation should last two seconds,
                // so check where we are now compared to
                // the time the animation started
                cySubtractCurrent = cySubtractMax
                                    * (ulNowTime - pXCenterData->ulStartTime)
                                    / MAX_AUTOHIDE;

                if (cySubtractCurrent >= cySubtractMax)
                {
                    // last step:
                    cySubtractCurrent = cySubtractMax;
                    // stop this timer
                    tmrStopXTimer(pTimerSet,
                                  hwnd,
                                  usTimerID);
                    pXCenterData->idTimerAutohideRun = 0;
                    pXCenterData->ulStartTime = 0;

                    // mark frame as "hidden"
                    pXCenterData->fFrameAutoHidden = TRUE;

                    // broadcast XN_ENDANIMATE notification
                    ctrpBroadcastWidgetNotify(&pXCenterData->llWidgets,
                                              XN_ENDANIMATE,
                                              MPFROMSHORT(XAF_HIDE));

                    if (_ulWindowStyle & WS_TOPMOST)
                        if (WinIsWindowVisible(pGlobals->hwndFrame))
                            // repaint the XCenter, because if we're topmost,
                            // we sometimes have display errors that will never
                            // go away
                            WinInvalidateRect(pGlobals->hwndClient, NULL, FALSE);

                    if (pXCenterData->fClickWatchRunning)
                    {
                        // if we had a click watch running, stop it now
                        HWND hwndDaemon;
                        if (hwndDaemon = krnQueryDaemonObject())
                            WinSendMsg(hwndDaemon,
                                       XDM_ADDCLICKWATCH,
                                       (MPARAM)pXCenterData->Globals.hwndClient,
                                       (MPARAM)-1);         // remove
                    }
                }

                if (pGlobals->ulPosition == XCENTER_BOTTOM)
                {
                    // XCenter sits at bottom:
                    // move downwards
                    pXCenterData->yFrame = -cySubtractCurrent;
                }
                else
                {
                    // XCenter sits at top:
                    // move upwards
                    pXCenterData->yFrame =   winhQueryScreenCY()
                                           - pXCenterData->cyFrame
                                           + cySubtractCurrent;
                }
            }

            WinSetWindowPos(hwnd,
                            HWND_TOP,
                            0,
                            pXCenterData->yFrame,
                            pXCenterData->cxFrame,
                            pXCenterData->cyFrame,
                            SWP_MOVE | SWP_SIZE | SWP_SHOW /* | SWP_ZORDER */ );
        break; }

        default:
            // other timer:
            // call parent winproc
            brc = FALSE;
    }

    return (brc);
}

/*
 *@@ FrameDestroy:
 *      implementation for WM_DESTROY in fnwpXCenterMainFrame.
 *
 *@@added V0.9.13 (2001-06-19) [umoeller]
 */

VOID FrameDestroy(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER);
    XCenterData     *somThis = XCenterGetData(pXCenterData->somSelf);
    // save orig window proc because pXCenterData is freed here
    PFNWP           pfnwpOrig = pXCenterData->pfnwpFrameOrig;

    _pvOpenView = NULL;

    // last chance... update desktop workarea in case
    // this hasn't been done that. This has probably been
    // called before, but we better make sure.
    UpdateDesktopWorkarea(pXCenterData,
                          TRUE);            // force remove

    DestroyWidgets(pXCenterData);

    if (pXCenterData->Globals.hwndTooltip)
        WinDestroyWindow(pXCenterData->Globals.hwndTooltip);

    // remove this window from the object's use list
    _wpDeleteFromObjUseList(pXCenterData->somSelf,
                            &pXCenterData->UseItem);

    // kill XTimers
    tmrDestroySet((PXTIMERSET)(pXCenterData->Globals.pvXTimerSet));
    pXCenterData->Globals.pvXTimerSet = NULL;
            // added V0.9.12 (2001-05-21) [umoeller]

    ctrpFreeClasses();

    // stop the XCenter thread we're running on
    WinPostMsg(NULLHANDLE,      // post into the queue
               WM_QUIT,
               0,
               0);

    _wpFreeMem(pXCenterData->somSelf,
               (PBYTE)pXCenterData);

    // call parent... we can't do that in the main func
    // because XCenterData has been freed
    pfnwpOrig(hwnd, msg, mp1, mp2);
}

/*
 *@@ fnwpXCenterMainFrame:
 *      window proc for the XCenter frame.
 *
 *      See fnwpXCenterMainClient for an overview.
 *
 *      This window proc is used for subclassing the
 *      regular PM WC_FRAME which is created for the
 *      XCenter view in ctrpCreateXCenterView. The
 *      original WC_FRAME window proc is stored in
 *      the XCenter view's XCENTERWINDATA, which
 *      in turn resides in the frame's QWL_USER.
 *
 *      The frame is really not visible because it
 *      is completely covered by the client. But the
 *      WPS requires us to use a WC_FRAME for the view.
 *
 *@@changed V0.9.7 (2001-01-19) [umoeller]: fixed active window bugs
 *@@changed V0.9.9 (2001-02-06) [umoeller]: fixed WM_CLOSE problems on wpClose
 *@@changed V0.9.9 (2001-03-07) [umoeller]: fixed crashes on destroy... WM_DESTROY cleanup is now handled here
 *@@changed V0.9.10 (2001-04-11) [umoeller]: added WM_HITTEST, as requested by Alessandro Cantatore
 *@@changed V0.9.13 (2001-06-19) [umoeller]: extracted FrameDestroy
 *@@changed V0.9.14 (2001-08-21) [umoeller]: added WM_QUERYFRAMEINFO, which returns 0
 *@@changed V0.9.16 (2001-10-02) [umoeller]: fixed 100% CPU load while mouse was over disabled XCenter (settings dlg etc)
 */

MRESULT EXPENTRY fnwpXCenterMainFrame(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT             mrc = 0;
    BOOL                fCallDefault = FALSE;

    TRY_LOUD(excpt1)
    {
        switch (msg)
        {
            /*
             * WM_SYSCOMMAND:
             *      we must intercept this; since we don't have
             *      a system menu, closing from the task list
             *      won't work otherwise.
             *
             *      This comes in for the "Window->close" syscommand.
             *      In addition, this gets posted from FrameCommand
             *      when it intercepts WPMENUID_CLOSE.
             *
             *      NOTE: Apparently, since we're running on a
             *      separate thread, the tasklist posts WM_QUIT
             *      directly into the queue. Anyway, this msg
             *      does NOT come in for the tasklist close.
             *
             *
             */

            case WM_SYSCOMMAND:
                switch ((USHORT)mp1)
                {
                    case SC_CLOSE:
                        WinPostMsg(hwnd, WM_CLOSE, 0, 0);
                                // changed V0.9.9 (2001-02-06) [umoeller]
                    break;

                    default:
                        fCallDefault = TRUE;
                                // was missing V0.9.16 (2001-10-02) [umoeller]
                }
            break;

            /*
             * WM_CLOSE:
             *      we now do the "close" processing in WM_CLOSE
             *      instead of WM_SYSCOMMAND with SC_CLOSE because
             *      apparently wpClose sends (!) WM_CLOSE messages
             *      directly, which didn't quite clean up previously.
             */

            case WM_CLOSE:
                // update desktop workarea
                UpdateDesktopWorkarea((PXCENTERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER),
                                      TRUE);            // force remove
                WinDestroyWindow(hwnd);
                        // after this pXCenterData is INVALID!
            break;

            case WM_QUERYFOCUSCHAIN:
            {
                PCSZ pcsz;
                switch (SHORT1FROMMP(mp1))
                {
                    case QFC_NEXTINCHAIN: pcsz = "QFC_NEXTINCHAIN"; break;

                    case QFC_ACTIVE: pcsz = "QFC_ACTIVE"; break;

                    case QFC_FRAME: pcsz = "QFC_FRAME"; break;

                    case QFC_SELECTACTIVE: pcsz = "QFC_SELECTACTIVE"; break;

                    case QFC_PARTOFCHAIN: pcsz = "QFC_PARTOFCHAIN"; break;

                    default:
                        pcsz = "unknown mp1";
                }

                if (SHORT1FROMMP(mp1) == QFC_ACTIVE)
                    mrc = 0;            // not supported
                else
                {
                    PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER);
                    mrc = pXCenterData->pfnwpFrameOrig(hwnd, msg, mp1, mp2);
                }

                _Pmpf((__FUNCTION__ ": WM_QUERYFOCUSCHAIN 0x%lX", hwnd));
                _Pmpf(("   %d (%s) --> 0x%lX",
                        mp1,
                        pcsz,
                        mrc));
            break; }

            /*
             * WM_ADJUSTWINDOWPOS:
             *      we never want to become active.
             */

            /* case WM_ADJUSTWINDOWPOS:
            {
                PSWP pswp = (PSWP)mp1;
                if (pswp->fl & SWP_ACTIVATE)
                {
                    pswp->fl &= ~SWP_ACTIVATE;
                    pswp->fl |= SWP_DEACTIVATE;
                }
                mrc = (MRESULT)AWP_DEACTIVATE;
            }
            break; */       // @@disabled V0.9.16 (2001-10-02) [umoeller]

            /*
             * WM_QUERYFRAMEINFO:
             *
             *added V0.9.14 (2001-08-21) [umoeller]
             */

            /* case WM_QUERYFRAMEINFO:
                mrc = (MPARAM)(0);
                        // default frame window proc returns
                        // FI_FRAME | FI_OWNERHIDE | FI_NOMOVEWITHOWNER | FI_ACTIVATEOK
            break; */ // @@disabled V0.9.16 (2001-10-02) [umoeller]

            /*
             * WM_FOCUSCHANGE:
             *      the default frame window proc sends out
             *      the flurry of WM_SETFOCUS and WM_ACTIVATE
             *      messages, which is not what we want...
             *      we never want the XCenter to become active.
             *
             *      The default winproc sends this message to
             *      its owner or parent if it's not the desktop.
             *      As a result, when the user clicks on a widget
             *      which does not intercept this, NORMALLY
             *
             *      1)  the default winproc called by the widget
             *          winproc sends it to the XCenter client;
             *
             *      2)  the default winproc called by the client
             *          would send it here;
             *
             *      3)  the default frame proc called from here
             *          would send out WM_SETFOCUS and WM_ACTIVATE.
             *
             *      NOW, we intercept this message in the client
             *      already (2) in order to swallow it. Still,
             *      we get this message for the frame directly
             *      sometimes too, so we swallow it here as well.
             *
             *      If we don't do all this, the XCenter becomes
             *      active when the user clicks on a widget. That
             *      would make the window list widget's checks
             *      for the currently active window fail, which
             *      is needed for painting the respective window
             *      button depressed.
             */

            /* case WM_FOCUSCHANGE:
                // do nothing!
                // DosBeep(100, 100);
            break;

            case WM_ADJUSTFRAMEPOS:
                // do NOTHING!
            break; */ // @@disabled V0.9.16 (2001-10-02) [umoeller]

            /*
             * WM_HITTEST:
             *      according to PMREF, the frame usually
             *      returns TF_MOVE here, even though this
             *      is not listed in the list of valid
             *      WM_HITTEST return codes. Whatever this
             *      means.
             *
             *      Alessandro Cantatore asked for this to
             *      return HT_NORMAL here in order not to
             *      break Styler/2 sliding focus (and tooltips),
             *      so this was now added (V0.9.10 (2001-04-11) [umoeller]).
             */

            case WM_HITTEST:
                // fixed: V0.9.11 (2001-04-18) [umoeller]
                if (WinIsWindowEnabled(hwnd))
                    // enabled:
                    mrc = (MPARAM)HT_NORMAL;
                else
                    // disabled (e.g. settings dialog showing):
                    mrc = (MPARAM)HT_ERROR;
            break;

            /*
             * WM_ACTIVATE:
             *      we must swallow this because we _never_
             *      want to become active, no matter what.
             *      If we don't do this, the frame will
             *      be invalidated with WPS context menus,
             *      for example.
             */

            case WM_ACTIVATE:
                // test added V0.9.16 (2001-10-02) [umoeller]
                _Pmpf((__FUNCTION__ ": WM_ACTIVATE %d", mp1));
                if (mp1)
                    DosBeep(5000, 30);
                else
                    DosBeep(3000, 30);
                fCallDefault = TRUE;
            break;

            /*
             * WM_COMMAND:
             *      menu command posted to the client's owner
             *      == the frame (yes, that's us here).
             *
             *      Note that since we subclassed the frame,
             *      we get all menu commands before the WPS
             *      -- even from the WPS menu we displayed
             *      using wpDisplayMenu from the client.
             */

            case WM_COMMAND:
                if (!FrameCommand(hwnd, (USHORT)mp1))
                    // not processed:
                    fCallDefault = TRUE;
            break;

            /*
             * WM_MOUSEMOVE:
             *      if mouse moves over the frame, re-show it
             *      if it's hidden. This is what makes the
             *      frame reappear if the mouse moves over the
             *      tiny line at the screen edge if the XCenter
             *      is currently auto-hidden.
             */

            case WM_MOUSEMOVE:
                // do this only if the frame is actually enabled;
                // we got a 100% CPU load if the mouse was over the
                // XCenter while a settings dialog was open...
                // V0.9.16 (2001-10-02) [umoeller]
                if (WinIsWindowEnabled(hwnd))
                    ctrpReformat((PXCENTERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER),
                                 0);        // just show
                fCallDefault = TRUE;
            break;

            /*
             * WM_TIMER:
             *      handle the various timers related to animations
             *      and such.
             */

            case WM_TIMER:
                if (!FrameTimer(hwnd,
                                (PXCENTERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER),
                                (USHORT)mp1))
                    // not processed:
                    fCallDefault = TRUE;
            break;

            /*
             * WM_DESTROY:
             *      stop all running timers.
             *
             *      Implementation changed with V0.9.9. The client gets this
             *      before the frame... we used to clean up all data in
             *      WM_DESTROY of the client, which gave us problems here
             *      because this came in for the frame after XCENTERWINDATA
             *      was freed. Not a good idea, so we clean up here now.
             */

            case WM_DESTROY:
                FrameDestroy(hwnd, msg, mp1, mp2);
            break;

            default:
                fCallDefault = TRUE;
        }
    }
    CATCH(excpt1) {} END_CATCH();

    // do the call to the original WC_FRAME window proc outside
    // the exception handler
    if (fCallDefault)
    {
        PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER);
        mrc = pXCenterData->pfnwpFrameOrig(hwnd, msg, mp1, mp2);
    }

    return (mrc);
}

/* ******************************************************************
 *
 *   XCenter client window proc
 *
 ********************************************************************/

/*
 *@@ ClientPaint2:
 *      called from ClientPaint and ctrpDrawEmphasis
 *      to redraw the client with a given HPS.
 *
 *@@added V0.9.7 (2001-01-18) [umoeller]
 *@@changed V0.9.13 (2001-06-19) [umoeller]: added spacing lines painting
 */

VOID ClientPaint2(HWND hwndClient,
                  HPS hps)
{
    PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(hwndClient, QWL_USER);
    PXCENTERGLOBALS pGlobals = &pXCenterData->Globals;
    XCenterData     *somThis = XCenterGetData(pXCenterData->somSelf);
    RECTL           rclWin;
    ULONG           ul3DBorderWidth = pGlobals->ul3DBorderWidth;

    // draw 3D frame
    WinQueryWindowRect(hwndClient,
                       &rclWin);        // exclusive

    // make rect inclusive
    rclWin.xRight--;
    rclWin.yTop--;

    if (ul3DBorderWidth)
    {
        // 3D border enabled at all:
        // check if we want all four borders
        if (pGlobals->flDisplayStyle & XCS_ALL3DBORDERS)
        {
            // yes: draw 3D frame
            gpihDraw3DFrame(hps,
                            &rclWin,            // inclusive
                            ul3DBorderWidth,      // width
                            pGlobals->lcol3DLight,
                            pGlobals->lcol3DDark);

            rclWin.xLeft += ul3DBorderWidth;
            rclWin.xRight -= ul3DBorderWidth;
            rclWin.yBottom += ul3DBorderWidth;
            rclWin.yTop -= ul3DBorderWidth;
        }
        else
        {
            // "all four borders" disabled: draw line only,
            // either on top or at the bottom, depending on
            // the position
            // XCenterData *somThis = XCenterGetData(pXCenterData->somSelf);
            RECTL       rcl2;
            LONG        yLine = 0;

            if (pGlobals->ulPosition == XCENTER_TOP)
            {
                // XCenter on top:
                // draw dark line on bottom
                GpiSetColor(hps, pGlobals->lcol3DDark);
                yLine = rclWin.yBottom;
                // exclude bottom from client rectangle for later
                rclWin.yBottom += ul3DBorderWidth;
            }
            else
            {
                // XCenter on bottom:
                // draw light line on top
                GpiSetColor(hps, pGlobals->lcol3DLight);
                yLine =   rclWin.yTop         // inclusive
                        - ul3DBorderWidth
                        + 1;
                // exclude top from client rectangle for later
                rclWin.yTop -= ul3DBorderWidth;
            }

            // draw 3D border
            rcl2.xLeft = rclWin.xLeft;
            rcl2.yBottom = yLine;
            rcl2.xRight = rclWin.xRight;                // inclusive
            rcl2.yTop = yLine + ul3DBorderWidth - 1;    // inclusive
            gpihBox(hps,
                    DRO_FILL,
                    &rcl2);
        }
    } // end if (ul3DBorderWidth)

    // fill client; rclWin has been reduced properly above
    GpiSetColor(hps,
                _lcolClientBackground);
                        // from the XCenter instance data
                        // V0.9.9 (2001-03-07) [umoeller]
    gpihBox(hps,
            DRO_FILL,
            &rclWin);

    // check if we're currently doing the "unfold frame"
    // animation
    if (pXCenterData->fFrameFullyShown)
    {
        // go thru all widgets and check if we have sizeables;
        // draw sizing bar after each sizeable widget
        PLISTNODE pNode = lstQueryFirstNode(&pXCenterData->llWidgets);

        // pre-calculate constant y and yTop for sizing bars
        RECTL rcl2;
        rcl2.yBottom =   rclWin.yBottom
                       + pGlobals->ulBorderSpacing
                       + 1;
        rcl2.yTop =   rclWin.yTop           // inclusive!
                    - pGlobals->ulBorderSpacing
                    - 1;

        while (pNode)
        {
            PPRIVATEWIDGETVIEW pWidgetThis = (PPRIVATEWIDGETVIEW)pNode->pItemData;

            // if spacing lines are enabled, draw these behind each
            // widget, except the last one
            if (    (pGlobals->flDisplayStyle & XCS_SPACINGLINES)
                 && (pNode->pNext)
               )
            {
                POINTL ptl;
                // draw dark line
                GpiSetColor(hps, pGlobals->lcol3DDark);
                ptl.x =    pWidgetThis->xCurrent
                         + pWidgetThis->szlCurrent.cx
                         + (pGlobals->ulWidgetSpacing / 2);
                ptl.y = rclWin.yBottom;
                GpiMove(hps, &ptl);
                ptl.y = rclWin.yTop;
                GpiLine(hps, &ptl);
                // draw light line
                GpiSetColor(hps, pGlobals->lcol3DLight);
                ptl.x++;
                ptl.y = rclWin.yBottom;
                GpiMove(hps, &ptl);
                ptl.y = rclWin.yTop;
                GpiLine(hps, &ptl);
            }

            // check pWidgetThis->xSizingBar... this has been set
            // to the xpos of the sizing bar by ReformatWidgets
            // if the widget is sizeable, otherwise it's 0
            if (pWidgetThis->xSizingBar)
            {
                rcl2.xLeft = pWidgetThis->xSizingBar;
                rcl2.xRight = rcl2.xLeft + 2;       // inclusive!
                gpihDraw3DFrame(hps,
                                &rcl2,
                                1,          // width
                                pGlobals->lcol3DLight,
                                pGlobals->lcol3DDark);
            }

            pNode = pNode->pNext;
        }
    }
}

/*
 *@@ ClientPaint:
 *      implementation for WM_PAINT in fnwpXCenterMainClient.
 */

VOID ClientPaint(HWND hwnd)
{
    RECTL rclPaint;
    HPS hps = WinBeginPaint(hwnd, NULLHANDLE, &rclPaint);
    if (hps)
    {
        // switch to RGB
        gpihSwitchToRGB(hps);

        ClientPaint2(hwnd, hps);

        WinEndPaint(hps);
    }
}

/*
 *@@ ClientPresParamChanged:
 *      implementation for WM_PRESPARAMCHANGED in fnwpXCenterMainClient.
 *
 *      We are now even saving fonts and background colors
 *      being dropped on the XCenter client.
 *
 *@@added V0.9.9 (2001-03-07) [umoeller]
 */

VOID ClientPresParamChanged(HWND hwnd,
                            ULONG idAttrChanged)
{
    PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER);
    XCenterData *somThis = XCenterGetData(pXCenterData->somSelf);
    BOOL fSave = TRUE;

    switch (idAttrChanged)
    {
        case 0:     // layout palette thing dropped
        case PP_BACKGROUNDCOLOR:
        // case PP_FOREGROUNDCOLOR:
            _lcolClientBackground
                = winhQueryPresColor(hwnd,
                                     PP_BACKGROUNDCOLOR,
                                     FALSE,
                                     SYSCLR_DIALOGBACKGROUND);
        break;

        case PP_FONTNAMESIZE:
            if (_pszClientFont)
            {
                free(_pszClientFont);
                _pszClientFont = NULL;
            }

            _pszClientFont = winhQueryWindowFont(hwnd);
        break;

        default:
            fSave = FALSE;
    }

    if (fSave)
        _wpSaveDeferred(pXCenterData->somSelf);
}

/*
 *@@ ClientMouseMove:
 *      implementation for WM_MOUSEMOVE in fnwpXCenterMainClient.
 *
 *      This does the usual stuff (setting the mouse pointer).
 *      In addition, since this message also comes in when the
 *      XCenter is auto-hidden (i.e. only exists as a minimal
 *      line at the edge of the screen), we unhide the frame if
 *      it's auto-hidden.
 *
 *@@changed V0.9.9 (2001-03-09) [umoeller]: added sizing border
 *@@changed V0.9.11 (2001-04-18) [umoeller]: with auto-hide, now putting XCenter on z-order top
 *@@changed V0.9.13 (2001-06-19) [umoeller]: fixed pointer change if XCenter is disabled
 */

MRESULT ClientMouseMove(HWND hwnd, MPARAM mp1)
{
    PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER);
    PXCENTERGLOBALS pGlobals = &pXCenterData->Globals;

    ULONG           idPtr = SPTR_ARROW;

    if (WinIsWindowEnabled(pGlobals->hwndFrame))    // V0.9.13 (2001-06-19) [umoeller]
    {
        SHORT           sxMouse = SHORT1FROMMP(mp1),  // win coords!
                        syMouse = SHORT2FROMMP(mp1);  // win coords!
        BOOL            fSizeable = FALSE;
        ULONG ulrc = FindWidgetFromClientXY(pXCenterData,
                                            NULLHANDLE,     // tray widget
                                            sxMouse,
                                            syMouse,
                                            &fSizeable,
                                            NULL,
                                            NULL);
        switch (ulrc)
        {
            case 2:
                // on sizing border:
                idPtr = SPTR_SIZENS;
            break;

            case 1:
                // on widget: check if it's sizeable
                // check if it's on the border of a sizeable widget (WE pointer)
                if (fSizeable)
                    idPtr = SPTR_SIZEWE;
            break;
        }
    }

    WinSetPointer(HWND_DESKTOP,
                  WinQuerySysPointer(HWND_DESKTOP,
                                     idPtr,
                                     FALSE));   // no copy

    // if the frame is currently auto-hidden, re-show it
    if (pXCenterData->fFrameAutoHidden)
    {
        // broadcast XN_BEGINANIMATE notification
        ctrpBroadcastWidgetNotify(&pXCenterData->llWidgets,
                                  XN_BEGINANIMATE,
                                  MPFROMSHORT(XAF_SHOW));

        ctrpReformat(pXCenterData,
                     XFMF_RESURFACE);        // show and put on top
                                // V0.9.11 (2001-04-18) [umoeller]

        // broadcast XN_ENDANIMATE notification
        ctrpBroadcastWidgetNotify(&pXCenterData->llWidgets,
                                  XN_ENDANIMATE,
                                  MPFROMSHORT(XAF_SHOW));
    }

    return ((MPARAM)TRUE);      // message processed
}

/*
 *@@ ClientButton12Drag:
 *      implementation for WM_BUTTON1DOWN and WM_BUTTON2DOWN
 *      in fnwpXCenterMainClient.
 *
 *@@changed V0.9.9 (2001-03-09) [umoeller]: added XCenter resize
 */

MRESULT ClientButton12Drag(HWND hwnd, MPARAM mp1)
{
    PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER);
    PXCENTERGLOBALS pGlobals = &pXCenterData->Globals;
    // SHORT           sx = SHORT1FROMMP(mp1);
    // SHORT           sy = SHORT2FROMMP(mp1);
    PPRIVATEWIDGETVIEW pViewOver = G_pViewOver;
    BOOL            fSizeable = G_fSizeable;

    if (G_ulWidgetFromXY == 2)
    {
        // mouse over sizing border:
        // resize XCenter's height then V0.9.9 (2001-03-09) [umoeller]
        TRACKINFO ti;
        SWP swpXCenter;

        // calculate height of all borders (this is the
        // difference between the client and the frame heights);
        // this depends on the display settings
        ULONG   cyAllBorders =   pGlobals->ul3DBorderWidth
                               + 2 * pGlobals->ulBorderSpacing;
        if (pGlobals->flDisplayStyle & XCS_ALL3DBORDERS)
            cyAllBorders += pGlobals->ul3DBorderWidth;

        WinQueryWindowPos(pGlobals->hwndFrame,
                          &swpXCenter);
        memset(&ti, 0, sizeof(ti));
        ti.cxBorder = max(1, pGlobals->ulBorderSpacing); // V0.9.12 (2001-05-28) [lafaix]
        ti.cyBorder = max(1, pGlobals->ulBorderSpacing); // V0.9.12 (2001-05-28) [lafaix]
        // ti.cxGrid = 0; V0.9.14 (2001-07-14) [lafaix]
        // ti.cyGrid = 0;
        ti.rclTrack.xLeft = swpXCenter.x;
        ti.rclTrack.xRight = swpXCenter.x + swpXCenter.cx;
        ti.rclTrack.yBottom = swpXCenter.y;
        ti.rclTrack.yTop = swpXCenter.y + swpXCenter.cy;
        // boundary (tracking rcl cannot leave that)
        ti.rclBoundary.xLeft = ti.rclTrack.xLeft;
        ti.rclBoundary.xRight = ti.rclTrack.xRight;
        if (pGlobals->ulPosition == XCENTER_BOTTOM)
        {
            ti.rclBoundary.yBottom = ti.rclTrack.yBottom;
            ti.rclBoundary.yTop = ti.rclTrack.yTop + 100;
            ti.fs = TF_ALLINBOUNDARY
                    | TF_TOP;
                    // | TF_SETPOINTERPOS; V0.9.12 (2001-05-28) [lafaix]
        }
        else
        {
            ti.rclBoundary.yBottom = ti.rclTrack.yBottom - 100;
            ti.rclBoundary.yTop = ti.rclTrack.yTop;
            ti.fs = TF_ALLINBOUNDARY
                    | TF_BOTTOM;
                    // | TF_SETPOINTERPOS; V0.9.12 (2001-05-28) [lafaix]
        }
        ti.ptlMinTrackSize.x = ti.rclBoundary.xRight;
        // do not allow height to become smaller than tallest widget
        ti.ptlMinTrackSize.y =   cyAllBorders
                               + pGlobals->cyWidgetMax;
        ti.ptlMaxTrackSize.x = ti.rclBoundary.xRight;
        ti.ptlMaxTrackSize.y = ti.ptlMinTrackSize.y + 100;

        // reset the mouse pointer V0.9.12 (2001-05-28) [lafaix]
        WinSetPointer(HWND_DESKTOP,
                      WinQuerySysPointer(HWND_DESKTOP,
                                         SPTR_SIZENS,
                                         FALSE));   // no copy

        if (WinTrackRect(HWND_DESKTOP,
                         NULLHANDLE,    //hps?
                         &ti))
        {
            // set new client height
            ULONG cyFrameNew = ti.rclTrack.yTop - ti.rclTrack.yBottom;
            pGlobals->cyInnerClient =   cyFrameNew
                                      - cyAllBorders;
            ctrpReformat(pXCenterData,
                         XFMF_RECALCHEIGHT);
        }
    }
    else if (    (G_ulWidgetFromXY == 1)
              && (fSizeable)
            )
    {
        // mouse over sizeable widget:
        if (pViewOver)
        {
            TRACKINFO ti;
            SWP swpWidget;

            // reset the mouse pointer V0.9.12 (2001-05-28) [lafaix]
            WinSetPointer(HWND_DESKTOP,
                          WinQuerySysPointer(HWND_DESKTOP,
                                             SPTR_SIZEWE,
                                             FALSE));   // no copy

            WinQueryWindowPos(pViewOver->Widget.hwndWidget,
                              &swpWidget);
            memset(&ti, 0, sizeof(ti));
            ti.cxBorder = pGlobals->ulWidgetSpacing;
            ti.cyBorder = pGlobals->ulWidgetSpacing;
            ti.cxGrid = 0;
            ti.cyGrid = 0;
            ti.rclTrack.xLeft = swpWidget.x;
            ti.rclTrack.xRight = swpWidget.x + swpWidget.cx;
            ti.rclTrack.yBottom = swpWidget.y;
            ti.rclTrack.yTop = swpWidget.y + swpWidget.cy;
            WinQueryWindowRect(hwnd,
                               &ti.rclBoundary);
            ti.ptlMinTrackSize.x = 30; // pViewOver->cxMinimum;
            ti.ptlMaxTrackSize.x = ti.rclBoundary.xRight;
            ti.ptlMaxTrackSize.y = ti.rclBoundary.yTop;
            ti.fs = TF_ALLINBOUNDARY
                    | TF_RIGHT
                    | TF_SETPOINTERPOS;

            if (WinTrackRect(hwnd,
                             NULLHANDLE,    //hps?
                             &ti))
            {
                // tracking not cancelled:
                // set new width
                SetWidgetSize(pXCenterData,
                              pViewOver->Widget.hwndWidget,
                              ti.rclTrack.xRight - ti.rclTrack.xLeft);
            }
        }
    }
    else
        return ((MPARAM)FALSE); // V0.9.14 (2001-07-14) [lafaix]

     return ((MPARAM)TRUE);      // message processed
}

/*
 *@@ ClientContextMenu:
 *      implementation for WM_CONTEXTMENU in fnwpXCenterMainClient.
 */

MRESULT ClientContextMenu(HWND hwnd, MPARAM mp1)
{
    PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER);
    PXCENTERGLOBALS pGlobals = &pXCenterData->Globals;
    XCenterData     *somThis = XCenterGetData(pXCenterData->somSelf);
    POINTL          ptlPopup;
    // HPS             hps;

    ptlPopup.x = SHORT1FROMMP(mp1);
    ptlPopup.y = SHORT2FROMMP(mp1);
    // set flag for wpModifyPopupMenu so that it will
    // add the "close" menu item; there's no other way
    // to get this to work since we don't have a container
    // as our client... sigh
    _fShowingOpenViewMenu = TRUE;
            // this will be unset in wpModifyPopupMenu

    // draw source emphasis
    ctrpDrawEmphasis(pXCenterData,
                     FALSE,     // draw, not remove emphasis
                     NULL,
                     hwnd,      // the client
                     NULLHANDLE);

    pXCenterData->hwndContextMenu
            = _wpDisplayMenu(pXCenterData->somSelf,
                             // owner: the frame
                             pGlobals->hwndFrame,
                             // client: our client
                             pGlobals->hwndClient,
                             &ptlPopup,
                             MENU_OPENVIEWPOPUP,
                             0);

    return ((MPARAM)TRUE);      // message processed
}

/*
 *@@ ClientMenuEnd:
 *      implementation for WM_MENUEND in fnwpXCenterMainClient.
 */

VOID ClientMenuEnd(HWND hwnd, MPARAM mp2)
{
    PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER);
    PXCENTERGLOBALS pGlobals = &pXCenterData->Globals;

    if (   (pXCenterData->hwndContextMenu)
        && (((HWND)mp2) == pXCenterData->hwndContextMenu)
       )
    {
        // context menu closing:
        // remove emphasis
        WinInvalidateRect(pGlobals->hwndClient, NULL, FALSE);
        pXCenterData->hwndContextMenu = NULLHANDLE;
    }
}

/*
 *@@ ClientControl:
 *      implementation for WM_CONTROL.
 *
 *@@added V0.9.7 (2001-01-03) [umoeller]
 *@@changed V0.9.13 (2001-06-21) [umoeller]: added TTN_SHOW, TTN_POP widget support
 */

MRESULT ClientControl(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    USHORT  usID = SHORT1FROMMP(mp1),
            usNotifyCode = SHORT2FROMMP(mp1);

    // _Pmpf((__FUNCTION__ ": id 0x%lX, notify 0x%lX", usID, usNotifyCode));

    if (usID == ID_XCENTER_TOOLTIP)
    {
        switch (usNotifyCode)
        {
            case TTN_NEEDTEXT:
            {
                // forward tooltip "need text" to widget
                PTOOLTIPTEXT pttt = (PTOOLTIPTEXT)mp2;
                mrc = WinSendMsg(pttt->hwndTool,      // widget window (tool)
                                 WM_CONTROL,
                                 mp1,
                                 mp2);
            }
            break;

            case TTN_SHOW:
            case TTN_POP:
            {
                PTOOLINFO pti = (PTOOLINFO)mp2;
                mrc = WinSendMsg(pti->hwndTool,     // widget window (tool)
                                 WM_CONTROL,
                                 mp1,
                                 mp2);
            }
        }
    }

    return (mrc);
}

/*
 *@@ ClientSaveSetup:
 *      implementation for XCM_SAVESETUP to save a widget's
 *      new setup string.
 *
 *      This calls _wpSaveDeferred in turn.
 *
 *@@added V0.9.7 (2000-12-04) [umoeller]
 */

BOOL ClientSaveSetup(HWND hwndClient,
                     HWND hwndWidget,
                     const char *pcszSetupString)    // can be NULL
{
    PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(hwndClient, QWL_USER);
    BOOL brc = FALSE;

    ULONG ulTrayWidgetIndex = 0,
          ulTrayIndex = 0,
          ulWidgetIndex = 0;
    if (ctrpQueryWidgetIndexFromHWND(pXCenterData->somSelf,
                                     hwndWidget,
                                     &ulTrayWidgetIndex,
                                     &ulTrayIndex,
                                     &ulWidgetIndex))
        /* cmnLog(__FILE__, __LINE__, __FUNCTION__,
               "Cannot find widget index for hwnd 0x%lX", hwndWidget);
    else */
    {
        PLINKLIST pllWidgetSettings = ctrpQuerySettingsList(pXCenterData->somSelf);
        PLISTNODE pSettingsNode = lstNodeFromIndex(pllWidgetSettings, ulWidgetIndex);
        if (pSettingsNode)
        {
            // got the settings:
            PPRIVATEWIDGETSETTING pSetting = (PPRIVATEWIDGETSETTING)pSettingsNode->pItemData;

            if (pSetting->Public.pszSetupString)
                // we already had a setup string:
                free(pSetting->Public.pszSetupString);

            pSetting->Public.pszSetupString = strhdup(pcszSetupString);
                                // can be NULL

            brc = TRUE;

            _wpSaveDeferred(pXCenterData->somSelf);
        }
    }

    return (brc);
}

/*
 * ClientDestroy:
 *      implementation for WM_DESTROY in fnwpXCenterMainClient.
 *      V0.9.9: removed, we now do this in WM_DESTROY for the
 *      frame... the client gets this before the frame
 */

/*
 *@@ fnwpXCenterMainClient:
 *      window proc for the client window of the XCenter bar.
 *
 *      The XCenter has the following window hierarchy (where
 *      lines signify both parent- and ownership):
 *
 +      WC_FRAME (fnwpXCenterMainFrame)
 +         |
 +         +----- FID_CLIENT (fnwpXCenterMainClient)
 +                    |
 +                    +---- widget 1
 +                    +---- widget 2
 +                    +---- ...
 *
 *      Widgets may of course create subchildren, of which
 *      the XCenter need not know.
 *
 *      The frame is created without any border. As a result,
 *      the client has the exact same size as the frame. It
 *      is actually the client which draws the 3D frame for
 *      the XCenter frame, because the frame is completely
 *      covered by it.
 *
 *      This may not sound effective, but the problem is that
 *      _wpRegisterView works only with frames, from my testing.
 *      So we can't just use our own window class in the
 *      first place. Besides, this mechanism gives the XWPHook
 *      a quick way to check if the window under the mouse is
 *      an XCenter, simply by testing FID_CLIENT for the window
 *      class. In other words, not using a frame just causes
 *      way too much trouble.
 *
 *@@changed V0.9.7 (2001-01-19) [umoeller]: fixed active window bugs
 *@@changed V0.9.9 (2001-03-07) [umoeller]: fixed crashes on destroy
 *@@changed V0.9.14 (2001-07-14) [lafaix]: fixed MB2 click on border/resizing bar
 *@@changed V0.9.14 (2001-08-21) [umoeller]: added "hide on click" support
 */

MRESULT EXPENTRY fnwpXCenterMainClient(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT             mrc = 0;

    TRY_LOUD(excpt1)
    {
        switch (msg)
        {
            /*
             * WM_CREATE:
             *
             */

            case WM_CREATE:
                // mp1 has control data
                WinSetWindowPtr(hwnd, QWL_USER, mp1);

                // return 0
            break;

            /*
             * WM_PAINT:
             *
             */

            case WM_PAINT:
                ClientPaint(hwnd);
            break;

            case WM_PRESPARAMCHANGED:
                ClientPresParamChanged(hwnd,
                                       (ULONG)mp1);
            break;

            /*
             * WM_FOCUSCHANGE:
             *      the default winproc sends this msg to
             *      the owner or the parent, which is not
             *      what we want. We just swallow this in
             *      order no NEVER let the XCenter become
             *      active.
             *
             *      See WM_FOCUSCHANGE in fnwpXCenterMainFrame for details.
             */

            /* case WM_FOCUSCHANGE:
                // do nothing!
                // DosBeep(200, 100);
            break; */ // @@disabled V0.9.16 (2001-10-02) [umoeller]

            /*
             * WM_MOUSEMOVE:
             *
             */

            case WM_MOUSEMOVE:
                mrc = ClientMouseMove(hwnd, mp1);
            break;

            /*
             * WM_BUTTON1DOWN:
             *
             */

            case WM_BUTTON1DOWN:
            case WM_BUTTON2DOWN:
            {
                PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER);
                G_fSizeable = FALSE;
                G_pViewOver = NULL;
                G_ulWidgetFromXY = FindWidgetFromClientXY(pXCenterData,
                                                          NULLHANDLE,
                                                          SHORT1FROMMP(mp1),
                                                          SHORT2FROMMP(mp1),
                                                          &G_fSizeable,
                                                          &G_pViewOver,
                                                          NULL);
                mrc = WinDefWindowProc(hwnd, msg, mp1, mp2);
            break; }

            /*
             * WM_BUTTON1MOTIONSTART:
             * WM_BUTTON2MOTIONSTART:
             *
             */

            case WM_BUTTON1MOTIONSTART:
            case WM_BUTTON2MOTIONSTART: // V0.9.14 (2001-07-14) [lafaix]
                mrc = ClientButton12Drag(hwnd, mp1);
            break;

            /*
             * WM_CONTEXTMENU:
             *
             */

            case WM_CONTEXTMENU:
                mrc = ClientContextMenu(hwnd, mp1);
            break;

            /*
             * WM_MENUEND:
             *
             */

            case WM_MENUEND:
                ClientMenuEnd(hwnd, mp2);
            break;

            /*
             * DM_DRAGOVER:
             *
             */

            case DM_DRAGOVER:
                mrc = ctrpDragOver(hwnd,
                                   NULLHANDLE,      // tray widget window
                                   (PDRAGINFO)mp1);
            break;

            /*
             * DM_DRAGLEAVE:
             *
             */

            case DM_DRAGLEAVE:
                // remove emphasis
                ctrpRemoveDragoverEmphasis(hwnd);
            break;

            /*
             * DM_DROP:
             *
             */

            case DM_DROP:
                ctrpDrop(hwnd,
                         NULLHANDLE,      // tray widget window
                         (PDRAGINFO)mp1);
            break;

            /*
             * WM_CONTROL:
             *
             */

            case WM_CONTROL:
                ClientControl(hwnd, mp1, mp2);
            break;

            /*
             * WM_CLOSE:
             *      this is sent to the client by the frame if
             *      it receives WM_CLOSE itself.
             */

            case WM_CLOSE:
                // destroy parent (the frame)
                WinDestroyWindow(WinQueryWindow(hwnd, QW_PARENT));
                            // after this pXCenterData is invalid!!
            break;

            /*
             * WM_DESTROY:
             *      moved this to frame V0.9.9 (2001-03-07) [umoeller]
             */

            // case WM_DESTROY:
                // ClientDestroy(hwnd);
            // break;

            case XCM_SETWIDGETSIZE:
            {
                PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER);
                SetWidgetSize(pXCenterData, (HWND)mp1, (ULONG)mp2);
            break; }

            case XCM_REFORMAT:
            {
                PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER);
                if (pXCenterData->fFrameFullyShown) // V0.9.9 (2001-02-08) [umoeller]
                    ctrpReformat(pXCenterData,
                                 (ULONG)mp1);       // flags
            break; }

            case XCM_SAVESETUP:
                mrc = (MRESULT)ClientSaveSetup(hwnd, (HWND)mp1, (PSZ)mp2);
            break;

            case XCM_CREATEWIDGET:
            {
                // this msg is only used for creating the window on the
                // GUI thread because method calls may run on any thread...
                PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)WinQueryWindowPtr(hwnd, QWL_USER);
                ctrpCreateWidgetWindow(pXCenterData,
                                       NULL,        // no owning tray
                                       &pXCenterData->llWidgets,
                                       (PPRIVATEWIDGETSETTING)mp1,
                                       (ULONG)mp2);        // ulWidgetIndex
            break; }

            case XCM_DESTROYWIDGET:
                // this msg is only used for destroying the window on the
                // GUI thread because method calls may run on any thread...
                WinDestroyWindow((HWND)mp1);
                        // this better be a widget window...
                        // ctrDefWidgetProc takes care of cleanup
            break;

            /*
             * XCM_MOUSECLICKED:
             *      special msg posted from XWPDAEMN.EXE for
             *      mouse click notifies. The mouse click watch
             *      is only added in auto-hide mode if "hide on
             *      click" is enabled also, and only while the
             *      XCenter is still visible. So if we get this
             *      we should start the auto-hide immediately.
             *
             *      See XDM_ADDCLICKWATCH for the params we
             *      get here.
             *
             *      V0.9.14 (2001-08-21) [umoeller]
             */

            case XCM_MOUSECLICKED:
                switch ((ULONG)mp1)
                {
                    // ignore button 2 and 3
                    case WM_BUTTON1DOWN:
                    case WM_BUTTON1UP:
                    case WM_BUTTON1DBLCLK:
                        StartAutohideNow((PXCENTERWINDATA)WinQueryWindowPtr(hwnd,
                                                                            QWL_USER));
                }
            break;

            default:
                mrc = WinDefWindowProc(hwnd, msg, mp1, mp2);
        }
    }
    CATCH(excpt1) {} END_CATCH();

    return (mrc);
}

/* ******************************************************************
 *
 *   XCenter widget class management
 *
 ********************************************************************/

/*
 *@@ ctrpLockClasses:
 *      locks the XCenter widget classes list.
 *
 *@@added V0.9.12 (2001-05-20) [umoeller]
 */

BOOL ctrpLockClasses(VOID)
{
    if (!G_hmtxClasses)
        return (!DosCreateMutexSem(NULL,
                                   &G_hmtxClasses,
                                   0,
                                   TRUE));       // request now

    return (!WinRequestMutexSem(G_hmtxClasses, SEM_INDEFINITE_WAIT));
}

/*
 *@@ ctrpUnlockClasses:
 *
 *@@added V0.9.12 (2001-05-20) [umoeller]
 */

VOID ctrpUnlockClasses(VOID)
{
    DosReleaseMutexSem(G_hmtxClasses);
}

/*
 *@@ ctrFreeModule:
 *      wrapper around DosFreeModule which
 *      attempts to call the "UnInitModule"
 *      export from the widget DLL beforehand.
 *
 *      May run on any thread. Caller must hold
 *      classes mutex.
 *
 *@@added V0.9.7 (2000-12-07) [umoeller]
 *@@changed V0.9.9 (2001-02-06) [umoeller]: added fCallUnInit
 */

APIRET ctrpFreeModule(HMODULE hmod,
                      BOOL fCallUnInit)     // in: if TRUE, "uninit" export gets called
{
    if (fCallUnInit)
    {
        // the following might crash
        TRY_QUIET(excpt2)
        {
            PFNWGTUNINITMODULE pfnWgtUnInitModule = NULL;
            APIRET arc2 = DosQueryProcAddr(hmod,
                                           2,      // ordinal
                                           NULL,
                                           (PFN*)(&pfnWgtUnInitModule));
            if ((arc2 == NO_ERROR) && (pfnWgtUnInitModule))
            {
                pfnWgtUnInitModule();
            }
        }
        CATCH(excpt2) {} END_CATCH();
    }

    return (DosFreeModule(hmod));
}

/*
 *@@ ctrpLoadClasses:
 *      initializes the global array of widget classes.
 *
 *      This also goes thru the plugins\xcenter subdirectory
 *      of the XWorkplace installation directory and tests
 *      the DLLs in there for widget plugins.
 *
 *      Note: For each ctrpLoadClasses call, there must be
 *      a matching ctrpFreeClasses call, or the plugin DLLs
 *      will never be unloaded. This function maintains a
 *      reference count to the global data so calls to this
 *      function may be nested.
 *
 *      May run on any thread.
 *
 *@@changed V0.9.9 (2001-02-06) [umoeller]: added version management
 *@@changed V0.9.9 (2001-03-09) [umoeller]: added PRIVATEWIDGETCLASS wrapping
 *@@changed V0.9.9 (2001-03-09) [umoeller]: converted global array to linked list
 *@@changed V0.9.12 (2001-05-20) [umoeller]: added mutex protection to fix multiple loads
 */

VOID ctrpLoadClasses(VOID)
{
    BOOL fLocked = FALSE;
    TRY_LOUD(excpt1)
    {
        if (fLocked = ctrpLockClasses()) // V0.9.12 (2001-05-20) [umoeller]
        {
            if (!G_fModulesInitialized)
            {
                // very first call:
                lstInit(&G_llWidgetClasses, FALSE);
                lstInit(&G_llModules, FALSE);
                G_fModulesInitialized = TRUE;
            }

            if (!G_fWidgetClassesLoaded)
            {
                // widget classes not loaded yet (or have been released again):

                HAB             hab = WinQueryAnchorBlock(cmnQueryActiveDesktopHWND());

                HMODULE         hmodXFLDR = cmnQueryMainCodeModuleHandle();

                // built-in widget classes:
                APIRET          arc = NO_ERROR;
                CHAR            szPluginsDir[2*CCHMAXPATH],
                                szSearchMask[2*CCHMAXPATH];
                HDIR            hdirFindHandle = HDIR_CREATE;
                FILEFINDBUF3    ffb3 = {0};      // returned from FindFirst/Next
                ULONG           cbFFB3 = sizeof(FILEFINDBUF3);
                ULONG           ulFindCount = 1;  // look for 1 file at a time
                ULONG           ul;

                // step 1: append built-in widgets to list
                for (ul = 0;
                     ul < ARRAYITEMCOUNT(G_aBuiltInWidgets);
                     ul++)
                {
                    PPRIVATEWIDGETCLASS pClass = (PPRIVATEWIDGETCLASS)malloc(sizeof(*pClass));
                    memset(pClass,
                           0,
                           sizeof(*pClass));
                    memcpy(&pClass->Public,
                           &G_aBuiltInWidgets[ul],
                           sizeof(XCENTERWIDGETCLASS));
                    lstAppendItem(&G_llWidgetClasses,
                                  pClass);
                }

                // step 2: append plugin DLLs to list
                // compose path for widget plugin DLLs
                cmnQueryXWPBasePath(szPluginsDir);
                strcat(szPluginsDir, "\\plugins\\xcenter");
                sprintf(szSearchMask, "%s\\%s", szPluginsDir, "*.dll");

                // _Pmpf((__FUNCTION__ ": searching for '%s'", szSearchMask));

                arc = DosFindFirst(szSearchMask,
                                   &hdirFindHandle,
                                   // find everything except directories
                                   FILE_ARCHIVED | FILE_HIDDEN | FILE_SYSTEM | FILE_READONLY,
                                   &ffb3,
                                   cbFFB3,
                                   &ulFindCount,
                                   FIL_STANDARD);
                // and start looping...
                while (arc == NO_ERROR)
                {
                    // alright... we got the file's name in ffb3.achName
                    CHAR            szDLL[2*CCHMAXPATH],
                                    szError[CCHMAXPATH] = "";
                    HMODULE         hmod = NULLHANDLE;
                    APIRET          arc2 = NO_ERROR;

                    sprintf(szDLL, "%s\\%s", szPluginsDir, ffb3.achName);

                    arc2 = DosLoadModule(szError,
                                         sizeof(szError),
                                         szDLL,
                                         &hmod);

                    if (arc2 != NO_ERROR)
                    {
                        // error loading module:
                        // log this, but we'd rather not have a message box here
                        cmnLog(__FILE__, __LINE__, __FUNCTION__,
                               "Unable to load plugin DLL \"%s\"."
                               "\n    DosLoadModule returned code %d and string: \"%s\"",
                               szDLL,
                               arc2,
                               szError);

                    }
                    else
                    {
                        CHAR    szErrorMsg[500] = "nothing.";
                                // room for error msg by DLL

                        // OK, since we've changed the prototype for the init module,
                        // it's time to do version management.
                        // V0.9.9 (2001-02-06) [umoeller]

                        // Check if the widget has the "query version" export.
                        PFNWGTQUERYVERSION pfnWgtQueryVersion = NULL;
                        // standard version if this fails: 0.9.8
                        ULONG       ulMajor = 0,
                                    ulMinor = 9,
                                    ulRevision = 8;
                        arc2 = DosQueryProcAddr(hmod,
                                                3,      // ordinal
                                                NULL,
                                                (PFN*)(&pfnWgtQueryVersion));

                        // (protect this with an exception handler, because
                        // this might crash)
                        TRY_QUIET(excpt2)
                        {
                            BOOL    fSufficientVersion = TRUE;

                            if ((arc2 == NO_ERROR) && (pfnWgtQueryVersion))
                            {
                                ULONG   ulXCenterMajor,
                                        ulXCenterMinor,
                                        ulXCenterRevision;
                                // we got the export:
                                pfnWgtQueryVersion(&ulMajor,
                                                   &ulMinor,
                                                   &ulRevision);

                                // check if this widget can live with this
                                // XCenter build level
                                sscanf(BLDLEVEL_VERSION,
                                       "%d.%d.%d",
                                       &ulXCenterMajor,
                                       &ulXCenterMinor,
                                       &ulXCenterRevision);

                                if (    (ulMajor > ulXCenterMajor)
                                     || (    (ulMajor == ulXCenterMajor)
                                          && (    (ulMinor > ulXCenterMinor)
                                               || (    (ulMinor == ulXCenterMinor)
                                                    && (ulRevision > ulXCenterRevision)
                                                  )
                                             )
                                        )
                                   )
                                    fSufficientVersion = FALSE;
                            }

                            if (fSufficientVersion)
                            {
                                PXCENTERWIDGETCLASS paClasses = NULL;
                                ULONG   cClassesThis = 0;

                                // now check which INIT we can call
                                if (    (ulMajor > 0)
                                     || (ulMinor > 9)
                                     || (ulRevision > 8)
                                   )
                                {
                                    // new widget:
                                    // we can then afford the new prototype
                                    PFNWGTINITMODULE_099 pfnWgtInitModule = NULL;
                                    arc2 = DosQueryProcAddr(hmod,
                                                            1,      // ordinal
                                                            NULL,
                                                            (PFN*)(&pfnWgtInitModule));
                                    if ((arc2 == NO_ERROR) && (pfnWgtInitModule))
                                        cClassesThis = pfnWgtInitModule(hab,
                                                                        hmod,       // new!
                                                                        hmodXFLDR,
                                                                        &paClasses,
                                                                        szErrorMsg);
                                }
                                else
                                {
                                    // use the old prototype:
                                    PFNWGTINITMODULE_OLD pfnWgtInitModule = NULL;
                                    arc2 = DosQueryProcAddr(hmod,
                                                            1,      // ordinal
                                                            NULL,
                                                            (PFN*)(&pfnWgtInitModule));
                                    if ((arc2 == NO_ERROR) && (pfnWgtInitModule))
                                        cClassesThis = pfnWgtInitModule(hab,
                                                                        hmodXFLDR,
                                                                        &paClasses,
                                                                        szErrorMsg);
                                }

                                if (cClassesThis)
                                {
                                    // paClasses must now point to an array of
                                    // cClassesThis XCENTERWIDGETCLASS structures;
                                    // copy these
                                    for (ul = 0;
                                         ul < cClassesThis;
                                         ul++)
                                    {
                                        PPRIVATEWIDGETCLASS pClass = (PPRIVATEWIDGETCLASS)malloc(sizeof(*pClass));
                                        memset(pClass,
                                               0,
                                               sizeof(*pClass));
                                        memcpy(&pClass->Public,
                                               &paClasses[ul],
                                               sizeof(XCENTERWIDGETCLASS));
                                        // store version
                                        pClass->ulVersionMajor = ulMajor;
                                        pClass->ulVersionMinor = ulMinor;
                                        pClass->ulVersionRevision = ulRevision;

                                        // store module
                                        pClass->hmod = hmod;
                                        lstAppendItem(&G_llWidgetClasses,
                                                      pClass);
                                    }

                                    // append this module to the global list of
                                    // loaded modules
                                    lstAppendItem(&G_llModules,
                                                  (PVOID)hmod);
                                } // end if (cClassesThis)
                                else
                                    // no classes in module or error:
                                    arc2 = ERROR_INVALID_DATA;
                            }
                        }
                        CATCH(excpt2)
                        {
                            arc2 = ERROR_INVALID_ORDINAL;
                        } END_CATCH();

                        if (arc2)
                        {
                            // error occured (or crash):
                            // unload the module again
                            ctrpFreeModule(hmod,
                                           FALSE);

                            if (arc2 == ERROR_INVALID_DATA)
                                cmnLog(__FILE__, __LINE__, __FUNCTION__,
                                       "InitModule call (export @1) failed for plugin DLL"
                                       "\n        \"%s\"."
                                       "\n    DLL returned error msg:"
                                       "\n        %s",
                                       szDLL,
                                       szErrorMsg);
                        }
                    } // end if DosLoadModule

                    // find next DLL
                    ulFindCount = 1;
                    arc = DosFindNext(hdirFindHandle,
                                      &ffb3,
                                      cbFFB3,
                                      &ulFindCount);
                } // while (arc == NO_ERROR)

                DosFindClose(hdirFindHandle);

                G_fWidgetClassesLoaded = TRUE;
            }

            G_ulWidgetClassesRefCount++;
        }
    }
    CATCH(excpt1) {} END_CATCH();

    if (fLocked)
        ctrpUnlockClasses();
}

/*
 *@@ ctrpQueryClasses:
 *      returns the global array of currently loaded
 *      widget classes. The linked list contains
 *      pointers to PRIVATEWIDGETCLASS structures.
 *
 *      For this to work, you must only use this
 *      function in a block between ctrpLoadClasses
 *      and ctrpFreeClasses. Do not modify the items
 *      on the list. Do not work on the list after
 *      you have called ctrpFreeClasses because
 *      the list might then have been freed.
 *
 *@@added V0.9.9 (2001-03-09) [umoeller]
 */

PLINKLIST ctrpQueryClasses(VOID)
{
    return (&G_llWidgetClasses);
}

/*
 *@@ ctrpFreeClasses:
 *      decreases the reference count for the global
 *      widget classes array by one. If 0 is reached,
 *      all allocated resources are freed, and plugin
 *      DLL's are unloaded.
 *
 *      See ctrpLoadClasses().
 *
 *      May run on any thread.
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
 *@@changed V0.9.9 (2001-03-09) [umoeller]: added PRIVATEWIDGETCLASS wrapping
 *@@changed V0.9.9 (2001-03-09) [umoeller]: converted global array to linked list
 *@@changed V0.9.12 (2001-05-20) [umoeller]: added mutex protection to fix multiple loads
 */

VOID ctrpFreeClasses(VOID)
{
    BOOL fLocked = FALSE;
    TRY_LOUD(excpt2)
    {
        if (fLocked = ctrpLockClasses())
        {
            if (G_ulWidgetClassesRefCount == 0)
                cmnLog(__FILE__, __LINE__, __FUNCTION__,
                               "G_ulWidgetClassesRefCount is already 0!");
            else
            {
                G_ulWidgetClassesRefCount--;
                if (G_ulWidgetClassesRefCount == 0)
                {
                    // no more references to the data:
                    PLISTNODE pNode = lstQueryFirstNode(&G_llWidgetClasses);
                    while (pNode)
                    {
                        PPRIVATEWIDGETCLASS pClass
                            = (PPRIVATEWIDGETCLASS)pNode->pItemData;

                        free(pClass);

                        pNode = pNode->pNext;
                    }

                    // unload modules
                    pNode = lstQueryFirstNode(&G_llModules);
                    while (pNode)
                    {
                        HMODULE hmod = (HMODULE)pNode->pItemData;
                        // _Pmpf((__FUNCTION__ ": Unloading hmod %lX", hmod));
                        ctrpFreeModule(hmod,
                                       TRUE);

                        pNode = pNode->pNext;
                    }

                    lstClear(&G_llModules);
                    lstClear(&G_llWidgetClasses);

                    G_fWidgetClassesLoaded = FALSE;
                }
            }

            // _Pmpf((__FUNCTION__ ": leaving, G_ulWidgetClassesRefCount is %d", G_ulWidgetClassesRefCount));
        }
    }
    CATCH(excpt2) {} END_CATCH();

    if (fLocked)
        ctrpUnlockClasses();
}

/*
 *@@ ctrpFindClass:
 *      finds the XCENTERWIDGETCLASS entry from the
 *      global array which has the given widget class
 *      name (_not_ PM window class name!).
 *
 *      Preconditions:
 *
 *      --  The widget classes must have been loaded
 *          first (by calling ctrpLoadClasses).
 *
 *      --  Caller must hold the classes mutex.
 *
 *      Postconditions:
 *
 *      --  This returns a plain pointer to an item
 *          in the global classes array. Once the
 *          classes are unloaded, the pointer must
 *          no longer be used.
 *
 *      May run on any thread.
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
 *@@changed V0.9.9 (2001-03-09) [umoeller]: added PRIVATEWIDGETCLASS wrapping
 *@@changed V0.9.9 (2001-03-09) [umoeller]: converted global array to linked list
 *@@changed V0.9.12 (2001-05-12) [umoeller]: added extra non-null check
 */

PXCENTERWIDGETCLASS ctrpFindClass(const char *pcszWidgetClass)
{
    PXCENTERWIDGETCLASS pReturn = NULL;

    // ULONG ul = 0;

    PLISTNODE pNode = lstQueryFirstNode(&G_llWidgetClasses);
    while (pNode)
    {
        PPRIVATEWIDGETCLASS pClass
            = (PPRIVATEWIDGETCLASS)pNode->pItemData;

        if (    (pClass)        // V0.9.12 (2001-05-12) [umoeller]
             && (!strcmp(pClass->Public.pcszWidgetClass,
                         pcszWidgetClass))
           )
        {
            // found:
            pReturn = &pClass->Public;
            break;
        }

        pNode = pNode->pNext;
    }

    return (pReturn);    // can be 0
}

/* ******************************************************************
 *
 *   Widget settings management
 *
 ********************************************************************/

/*
 *@@ FreeSettingData:
 *      frees the data in the specified widget setting,
 *      including tray and subwidget definitions, but
 *      not the pSetting itself (so this can be used
 *      for PRIVATEWIDGETSETTING members too).
 *
 *@@added V0.9.13 (2001-06-21) [umoeller]
 */

VOID FreeSettingData(PPRIVATEWIDGETSETTING pSetting)
{
    if (pSetting->Public.pszWidgetClass)
        free(pSetting->Public.pszWidgetClass);
    if (pSetting->Public.pszSetupString)
        free(pSetting->Public.pszSetupString);

    if (pSetting->pllTraySettings)
    {
        // this is for a tray widget and has tray settings:
        // whoa, go for all those lists too
        PLISTNODE pTrayNode = lstQueryFirstNode(pSetting->pllTraySettings);
        while (pTrayNode)
        {
            PTRAYSETTING pTraySetting = (PTRAYSETTING)pTrayNode->pItemData;
            PLISTNODE pSubwidgetNode;

            if (pTraySetting->pszTrayName)
                free(pTraySetting->pszTrayName);

            // go for subwidgets list
            pSubwidgetNode = lstQueryFirstNode(&pTraySetting->llSubwidgets);
            while (pSubwidgetNode)
            {
                PPRIVATEWIDGETSETTING pSubwidget = (PPRIVATEWIDGETSETTING)pSubwidgetNode->pItemData;
                // recurse into that subwidget; this won't have
                // a linked list of tray again, but we can reuse
                // the free() code above
                FreeSettingData(pSubwidget);

                pSubwidgetNode = pSubwidgetNode->pNext;
            }

            free(pTraySetting);

            pTrayNode = pTrayNode->pNext;
        }

        // nuke the trays list (nodes left, item data has been freed)
        lstFree(&pSetting->pllTraySettings);
    }
}

/*
 *@@ ctrpCreateTray:
 *      creates a new (empty) tray with the specified
 *      name. Does not switch to the tray yet since
 *      the engine doesn't care about the tray's
 *      current tray.
 *
 *      This does not save the widget settings.
 *
 *@@added V0.9.13 (2001-06-21) [umoeller]
 */

PTRAYSETTING ctrpCreateTray(PPRIVATEWIDGETSETTING ppws, // in: private tray widget setting
                            const char *pcszTrayName,   // in: tray name
                            PULONG pulIndex)            // out: index of new tray
{
    PTRAYSETTING pNewTray;
    // PPRIVATEWIDGETSETTING ppws = (PPRIVATEWIDGETSETTING)pPrivate->pWidget->pvWidgetSetting;

    if (pNewTray = NEW(TRAYSETTING))
    {
        ZERO(pNewTray);

        pNewTray->pszTrayName = strhdup(pcszTrayName);

        lstInit(&pNewTray->llSubwidgets, FALSE);

        // append to widget setup
        lstAppendItem(ppws->pllTraySettings, pNewTray);

        // InvalidateMenu(pPrivate);

        if (pulIndex)
            *pulIndex = lstCountItems(ppws->pllTraySettings) - 1;
    }

    return (pNewTray);
}

/*
 *@@ ctrpDeleteTray:
 *      deletes the specified tray completely, including
 *      all its subwidgets.
 *
 *      The caller must make sure that the tray is not
 *      the current tray. It must also destroy all widgets
 *      first, if it is.
 *
 *      This does not save the widget settings.
 *
 *@@added V0.9.13 (2001-06-21) [umoeller]
 */

BOOL ctrpDeleteTray(PPRIVATEWIDGETSETTING ppws, // in: private tray widget setting
                    ULONG ulIndex)             // in: tray to delete
{
    if (ppws)
    {
        PLISTNODE pTrayNode,
                  pSubwidgetNode;
        if (pTrayNode = lstNodeFromIndex(ppws->pllTraySettings,
                                         ulIndex))
        {
            PTRAYSETTING pTray = (PTRAYSETTING)pTrayNode->pItemData;

            // delete all subwidgets in the tray
            pSubwidgetNode = lstQueryFirstNode(&pTray->llSubwidgets);
            while (pSubwidgetNode)
            {
                PLISTNODE pNext = pSubwidgetNode->pNext;

                ctrpDeleteWidgetSetting((PPRIVATEWIDGETSETTING)pSubwidgetNode->pItemData);

                pSubwidgetNode = pNext;
            }

            lstRemoveNode(ppws->pllTraySettings, pTrayNode);

            if (pTray->pszTrayName)
                free(pTray->pszTrayName);

            free(pTray);

            return (TRUE);
        }
    }

    return (FALSE);
}

/*
 *@@ ctrpFindWidgetSetting:
 *      returns a PRIVATEWIDGETSETTING for the given widget
 *      index or NULL if not found.
 *
 *      If a widget setting is found and (ppViewData != NULL),
 *      this also attempts to find the current view for the
 *      widget (see below).
 *
 *      Note that the index parameters work in two modes:
 *
 *      --  If (ulTrayWidgetIndex == -1), this shows the settings
 *          dialog for a "root" widget, which must be specified
 *          in ulWidgetIndex. ulTrayIndex is ignored.
 *
 *      --  If (ulTrayWidgetIndex != -1), it is assumed to
 *          contain the index of a "root" tray widget. In that
 *          case ulTrayIndex specifies the tray in that tray
 *          widget and ulWidgetIndex the subwidget index in
 *          that tray.
 *
 *      *ppViewData receives a pointer to the currently open
 *      widget view, if any. This will be set to NULL if
 *
 *      --  NULL is returned in the first place;
 *
 *      --  the XCenter isn't currently open at all;
 *
 *      --  the widget doesn't currently exist as a view,
 *          e.g. because it is in a tray that is not currently
 *          switched to.
 *
 *@@added V0.9.14 (2001-08-01) [umoeller]
 */

PPRIVATEWIDGETSETTING ctrpFindWidgetSetting(XCenter *somSelf,
                                            ULONG ulTrayWidgetIndex,   // in: tray widget index or -1
                                            ULONG ulTrayIndex,     // in: tray index in tray widget
                                            ULONG ulWidgetIndex,    // in: subwidget index in tray or root widget index
                                            PXCENTERWIDGET *ppViewData) // out: view data or NULL; ptr can be NULL
{
    PLINKLIST   pllWidgets = ctrpQuerySettingsList(somSelf);
    PPRIVATEWIDGETSETTING pReturn = NULL;
    XCenterData *somThis = XCenterGetData(somSelf);
    PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)_pvOpenView;
                            // can be NULL if we have no open view

    if (ppViewData)
        *ppViewData = 0;

    if (ulTrayWidgetIndex == -1)
    {
        // root widget:
        if (    (pReturn = lstItemFromIndex(pllWidgets,
                                            ulWidgetIndex))
             && (ppViewData)
             && (pXCenterData)          // we have an open view:
           )
        {
            // found, and caller wants the open view:
            *ppViewData = (PXCENTERWIDGET)lstItemFromIndex(&pXCenterData->llWidgets,
                                                           ulWidgetIndex);
        }
    }
    else
    {
        PPRIVATEWIDGETSETTING pTrayWidgetSetting;
        PTRAYSETTING pTraySetting;
        if (    (pTrayWidgetSetting = lstItemFromIndex(pllWidgets,
                                                       ulTrayWidgetIndex))
             && (pTrayWidgetSetting->pllTraySettings)
             && (pTraySetting = lstItemFromIndex(pTrayWidgetSetting->pllTraySettings,
                                                 ulTrayIndex))
           )
        {
            if (    (pReturn = lstItemFromIndex(&pTraySetting->llSubwidgets,
                                                ulWidgetIndex))
                 && (ppViewData)
                 && (pXCenterData)          // we have an open view:
               )
            {
                // found, and caller wants the open view:
                PPRIVATEWIDGETVIEW pTrayWidgetView;
                if (    (pTrayWidgetView = lstItemFromIndex(&pXCenterData->llWidgets,
                                                            ulTrayWidgetIndex))
                        // is the desired tray active?
                     && (pTrayWidgetSetting->ulCurrentTray == ulTrayIndex)
                     && (pTrayWidgetView->pllSubwidgetViews)
                   )
                {
                    *ppViewData = (PXCENTERWIDGET)lstItemFromIndex(pTrayWidgetView->pllSubwidgetViews,
                                                                   ulWidgetIndex);
                }
            }
        }
    }

    return (pReturn);
}

/*
 *@@ ctrpCreateWidgetSetting:
 *      creates a new subwidget setting in the specified
 *      tray. Does not create a subwidget window though.
 *
 *      This does not save the widget settings.
 *
 *@@added V0.9.13 (2001-06-21) [umoeller]
 */

PPRIVATEWIDGETSETTING ctrpCreateWidgetSetting(PTRAYSETTING pTray,              // in: tray to create subwidget in
                                              const char *pcszWidgetClass,     // in: new subwidget's class
                                              const char *pcszSetupString,     // in: new subwidget's setup string
                                              ULONG ulIndex)           // in: index (-1 for rightmost)
{
    PPRIVATEWIDGETSETTING pNew;
    if (pNew = NEW(PRIVATEWIDGETSETTING))
    {
        ZERO(pNew);

        pNew->pOwningTray = pTray;

        pNew->Public.pszWidgetClass = strhdup(pcszWidgetClass);
        pNew->Public.pszSetupString = strhdup(pcszSetupString);

        if (    (ulIndex == -1)
             || (ulIndex >= lstCountItems(&pTray->llSubwidgets))
           )
            lstAppendItem(&pTray->llSubwidgets,
                          pNew);
        else
            lstInsertItemBefore(&pTray->llSubwidgets,
                                pNew,
                                ulIndex);
    }

    return (pNew);
}

/*
 *@@ ctrpDeleteWidgetSetting:
 *      deletes the specified subwidget from its
 *      owning tray.
 *
 *      This does not save the widget settings.
 *
 *      Preconditions:
 *
 *      -- The subwidget's window must have been destroyed first.
 *
 *@@added V0.9.13 (2001-06-21) [umoeller]
 */

BOOL ctrpDeleteWidgetSetting(PPRIVATEWIDGETSETTING pSubwidget)     // in: subwidget to delete
{
    if (    (pSubwidget)
         && (pSubwidget->pOwningTray)
            // remove subwidget from owning tray's list
         && (lstRemoveItem(&pSubwidget->pOwningTray->llSubwidgets,
                           pSubwidget))
       )
    {
        // now clean up
        FreeSettingData(pSubwidget);

        free(pSubwidget);

        return (TRUE);
    }

    return (FALSE);
}

/*
 *@@ AddWidgetSetting:
 *      adds a new PRIVATEWIDGETSETTING to the internal
 *      list of widget settings.
 *
 *      pSetting is assumed to be dynamic (i.e. allocated
 *      with malloc()).
 *
 *      This does not update open views. It neither saves
 *      the XCenter instance data. This has only been
 *      put into a separate procedure because it's both
 *      needed by ctrpInsertWidget and ctrpUnstuffSettings.
 *
 *      The caller must lock the XCenter himself.
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
 *@@changed V0.9.7 (2000-12-13) [umoeller]: changed prototype
 *@@changed V0.9.13 (2001-06-21) [umoeller]: now using PRIVATEWIDGETSETTING
 */

VOID AddWidgetSetting(XCenter *somSelf,
                      PPRIVATEWIDGETSETTING pSetting,  // in: new setting
                      ULONG ulBeforeIndex,             // in: index to insert before or -1 for end
                      PULONG pulNewItemCount,          // out: new settings count (ptr can be NULL)
                      PULONG pulNewWidgetIndex)        // out: index of new widget (ptr can be NULL)
{
    PLINKLIST pllWidgetSettings = ctrpQuerySettingsList(somSelf);
    if (    (ulBeforeIndex == -1)
         || (ulBeforeIndex >= lstCountItems(pllWidgetSettings))
       )
        // append to the end:
        lstAppendItem(pllWidgetSettings,
                      pSetting);
    else
        // append at specified position:
        lstInsertItemBefore(pllWidgetSettings,
                            pSetting,
                            ulBeforeIndex);
    if (pulNewItemCount)
        *pulNewItemCount = lstCountItems(pllWidgetSettings);
    if (pulNewWidgetIndex)
        *pulNewWidgetIndex = lstIndexFromItem(pllWidgetSettings, pSetting);
}

#define TRAYSETTINGSMARKER "(#@@TRAY&_/)!"        // whatever

/*
 *@@ ctrpStuffSettings:
 *      packs all the settings into a binary array
 *      which can be saved with wpSaveState.
 *
 *      The format is defined as two strings alternating
 *      for each widget: the PM window class name, and
 *      then the setup string (or a space if there's none).
 *
 *      The caller must free() the return value.
 *
 *      The XCenter stores all the widget settings (that is:
 *      descriptions of the widget instances plus their
 *      respective setup strings) in its instance data.
 *
 *      The problem is that the widget settings require
 *      that the plugin DLLs be loaded, but we don't want
 *      to keep these DLLs loaded unless an XCenter view
 *      is currently open. However, XCenter::wpRestoreState
 *      is called already when the XCenter gets awakened
 *      by the WPS (i.e. when its folder is populated).
 *
 *      The solution is that XCenter::wpSaveState saves
 *      a binary block of memory only, and XCenter::wpRestoreState
 *      loads that block. Only when the widget settings
 *      are really needed, they are unpacked into a linked
 *      list (using ctrpUnstuffSettings, which also loads the
 *      DLLs). XCenter::wpSaveState then packs the linked list
 *      again by calling this function.
 *
 *      In other words, this is a mechanism to defer loading
 *      the plugin DLLs until they are really needed.
 *
 *      The caller must lock the XCenter himself.
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
 *@@changed V0.9.13 (2001-06-21) [umoeller]: added tray saving
 */

PSZ ctrpStuffSettings(XCenter *somSelf,
                      PULONG pcbSettingsArray)   // out: array byte count
{
    PSZ     psz = 0;
    ULONG   cb = 0;
    XCenterData *somThis = XCenterGetData(somSelf);

    PLISTNODE   pWidgetNode,
                pTrayNode,
                pSubwidgetNode;

    ULONG   ulTray;
    XSTRING strEncoded;
    XSTRING strTray;
    xstrInit(&strTray, 200);
    xstrInit(&strEncoded, 200);

    // _Pmpf((__FUNCTION__ ": entering, _pszPackedWidgetSettings is %lX", _pszPackedWidgetSettings));

    if (_pllAllWidgetSettings)
    {
        for (pWidgetNode = lstQueryFirstNode(_pllAllWidgetSettings);
             pWidgetNode;
             pWidgetNode = pWidgetNode->pNext)
        {
            PPRIVATEWIDGETSETTING pSetting = (PPRIVATEWIDGETSETTING)pWidgetNode->pItemData;
            strhArrayAppend(&psz,
                            pSetting->Public.pszWidgetClass,
                            0,
                            &cb);
            if (pSetting->Public.pszSetupString)
                strhArrayAppend(&psz,
                                pSetting->Public.pszSetupString,
                                0,
                                &cb);
            else
                // no setup string: use a space
                strhArrayAppend(&psz,
                                " ",
                                1,
                                &cb);

            if (    (pSetting->pllTraySettings)
                 && (lstCountItems(pSetting->pllTraySettings))
               )
            {
                // this is a tray widget and has tray settings:
                // compose a single extra string with a special
                // dumb marker first and all the other settings
                // next... this is not pretty, but we must be
                // backward compatible with the old packed settings
                // format V0.9.13 (2001-06-21) [umoeller]

                xstrcpy(&strTray, TRAYSETTINGSMARKER, 0);
                // when ctrpUnstuffSettings encounters this
                // marker, it will know that this entry is
                // not yet a widget class, but a tray definition
                // and set up the linked list accordingly

                // compose TRAYS string
                for (pTrayNode = lstQueryFirstNode(pSetting->pllTraySettings), ulTray = 0;
                     pTrayNode;
                     pTrayNode = pTrayNode->pNext, ulTray++)
                {
                    PTRAYSETTING pTray = (PTRAYSETTING)pTrayNode->pItemData;

                    if (ulTray)
                        // not first one:
                        xstrcatc(&strTray, ',');

                    // add encoded tray name
                    xstrcpy(&strEncoded,
                            pTray->pszTrayName,
                            0);
                    xstrEncode(&strEncoded,
                               "%,()[]");
                    xstrcats(&strTray,
                             &strEncoded);

                    // if this tray has subwidgets, add them in square brackets
                    if (pSubwidgetNode = lstQueryFirstNode(&pTray->llSubwidgets))
                    {
                        ULONG ulSubwidget;
                        xstrcatc(&strTray, '[');
                        for (ulSubwidget = 0;
                             pSubwidgetNode;
                             pSubwidgetNode = pSubwidgetNode->pNext, ulSubwidget++)
                        {
                            PPRIVATEWIDGETSETTING pSubwidget = (PPRIVATEWIDGETSETTING)pSubwidgetNode->pItemData;
                            if (ulSubwidget)
                                // not first one:
                                xstrcatc(&strTray, ',');

                            xstrcat(&strTray,
                                    pSubwidget->Public.pszWidgetClass,
                                    0);

                            // if this subwidget has a setup string,
                            // add it in angle brackets (encoded)
                            if (pSubwidget->Public.pszSetupString)
                            {
                                // copy and encode the subwidget's setup string
                                xstrcpy(&strEncoded,
                                        pSubwidget->Public.pszSetupString,
                                        0);
                                xstrEncode(&strEncoded,
                                           "%,()[]");

                                xstrcatc(&strTray, '(');
                                xstrcats(&strTray,
                                         &strEncoded);
                                xstrcatc(&strTray, ')');
                            }
                        }
                        xstrcatc(&strTray, ']');
                    } // end if (pSubwidgetNode
                } // end for (pTrayNode

                if (ulTray)
                    strhArrayAppend(&psz,
                                    strTray.psz,
                                    strTray.ulLength,
                                    &cb);

            } // end if (    (pSetting->pllTraySettings)
        } // end for pWidgetNode
    } // end if (_pllAllWidgetSettings)

    *pcbSettingsArray = cb;

    xstrClear(&strEncoded);
    xstrClear(&strTray);

    return (psz);
}

/*
 *@@ DecodeSubwidgets:
 *
 *@@added V0.9.13 (2001-06-21) [umoeller]
 */

VOID DecodeSubwidgets(PSZ p,                    // in: entire subwidgets substring (between [])
                      PTRAYSETTING pTray)       // in: tray setting to append widgets to
{
    PSZ pClassName = p;
    // alright, go ahead...
    while (pClassName)
    {
        // get the tray name
        PSZ         pNextOpenBracket = strchr(pClassName, '('),
                    pNextComma = strchr(pClassName, ','),
                    pNextCloseBracket = NULL;
        ULONG cbClassName = 0;

        _Pmpf((__FUNCTION__ ": decoding widgets for tray %s",
                                        pTray->pszTrayName));

        if (pNextOpenBracket)
        {
            // this tray has subwidgets:
            if (    (pNextComma)
                 && (pNextComma < pNextOpenBracket)
               )
            {
                // comma before next '(':
                // go up to comma then
                cbClassName = pNextComma - pClassName;
            }
            else
            {
                // '(' before comma:
                // we then have subwidgets in the square brackets
                cbClassName = pNextOpenBracket - pClassName;

                if (pNextCloseBracket = strchr(pNextOpenBracket, ')'))
                {
                    // re-search next comma because there can be
                    // commas in widget strings too
                    pNextComma = strchr(pNextCloseBracket, ',');
                }
                else
                {
                    // error:
                    cmnLog(__FILE__, __LINE__, __FUNCTION__,
                           "Confused widget brackets.");
                    break;
                }
            }
        }
        else
            // no more '(':
            // check if we have a comma still
            if (pNextComma)
                // yes: go up to that comma
                cbClassName = pNextComma - pClassName;
            else
                // no: use all remaining
                cbClassName = strlen(pClassName);

        if (cbClassName)
        {
            PPRIVATEWIDGETSETTING pNewSubwidget;

            // create a new TRAYSETTING for this
            if (pNewSubwidget = NEW(PRIVATEWIDGETSETTING))
            {
                ZERO(pNewSubwidget);

                pNewSubwidget->pOwningTray = pTray;

                pNewSubwidget->Public.pszWidgetClass
                        = strhSubstr(pClassName,
                                     pClassName + cbClassName);

                // does this widget have a setup string?
                if (pNextCloseBracket)
                {
                    XSTRING str;
                    xstrInit(&str, 0);
                    xstrcpy(&str,
                            pNextOpenBracket + 1,
                            pNextCloseBracket - (pNextOpenBracket + 1));
                    xstrDecode(&str);
                    pNewSubwidget->Public.pszSetupString = str.psz;
                                // malloc'd, do not free
                }

                _Pmpf(("  got subwidget %s, %s",
                        (pNewSubwidget->Public.pszWidgetClass)
                            ? pNewSubwidget->Public.pszWidgetClass
                            : "NULL",
                        (pNewSubwidget->Public.pszSetupString)
                            ? pNewSubwidget->Public.pszSetupString
                            : "NULL"));

                lstAppendItem(&pTray->llSubwidgets,
                              pNewSubwidget);
            }
            else
                break;

            if (pNextComma)
                pClassName = pNextComma + 1;
            else
                break;
        }
        else
            // no more tray names:
            break;
    }
}

/*
 *@@ DecodeTraySetting:
 *      decodes the given tray settings string
 *      (as composed by ctrpStuffSettings) and
 *      creates the various substructures and
 *      linked lists in the given tray setting.
 *
 *@@added V0.9.13 (2001-06-21) [umoeller]
 */

VOID DecodeTraySettings(PSZ p,
                        ULONG ulLength,
                        PPRIVATEWIDGETSETTING pSetting)
{
    PSZ pTrayName = p;

    PLINKLIST pllTraySettings = lstCreate(FALSE);
    pSetting->pllTraySettings = pllTraySettings;

    // string has the following format:
    //      tray,tray,tray
    // with each "tray" being:
    // -- trayname only or
    // -- trayname[subwidget,subwidget,subwidget]
    // with each subwidget being:
    // -- subwidgetclassname only or
    // -- subwidgetclassname(setup)

    // alright, go ahead...
    while (pTrayName)
    {
        // get the tray name
        PSZ         pNextOpenSquare = strchr(pTrayName, '['),
                    pNextComma = strchr(pTrayName, ','),
                    pNextCloseSquare = NULL;
        ULONG cbTrayName = 0;

        if (pNextOpenSquare)
        {
            // this tray has subwidgets:
            if (    (pNextComma)
                 && (pNextComma < pNextOpenSquare)
               )
            {
                // comma before next '[':
                // go up to comma then
                cbTrayName = pNextComma - pTrayName;
            }
            else
            {
                // '[' before comma:
                // we then have subwidgets in the square brackets
                cbTrayName = pNextOpenSquare - pTrayName;

                if (pNextCloseSquare = strchr(pNextOpenSquare, ']'))
                {
                    // re-search next comma because there can be
                    // commas in widget strings too
                    pNextComma = strchr(pNextCloseSquare, ',');
                }
                else
                {
                    // error:
                    cmnLog(__FILE__, __LINE__, __FUNCTION__,
                           "Confused square brackets.");
                    break;
                }
            }
        }
        else
            // no more '[':
            // check if we have a comma still
            if (pNextComma)
                // yes: go up to that comma
                cbTrayName = pNextComma - pTrayName;
            else
                // no: use all remaining
                cbTrayName = strlen(pTrayName);

        if (cbTrayName)
        {
            PTRAYSETTING pNewTray;

            XSTRING strTrayName;
            xstrInit(&strTrayName, 0);
            xstrcpy(&strTrayName,
                    pTrayName,
                    cbTrayName);
            xstrDecode(&strTrayName);

            // create a new TRAYSETTING for this
            if (pNewTray = NEW(TRAYSETTING))
            {
                pNewTray->pszTrayName = strTrayName.psz;
                                // malloc'd, do not free strTrayName

                lstInit(&pNewTray->llSubwidgets, FALSE);

                // append to widget setup
                lstAppendItem(pllTraySettings, pNewTray);

                // does this tray have widgets?
                if (pNextCloseSquare)
                {
                    // zero-terminate the subwidgets string
                    *pNextCloseSquare = '\0';
                    DecodeSubwidgets(pNextOpenSquare + 1,
                                     pNewTray);
                    *pNextCloseSquare = ']';
                }
            }
            else
                break;

            if (pNextComma)
                pTrayName = pNextComma + 1;
            else
                break;
        }
        else
            // no more tray names:
            break;
    }
}

/*
 *@@ ctrpUnstuffSettings:
 *      unpacks a settings array previously packed
 *      with ctrpStuffSettings.
 *
 *      After this has been called, a linked list
 *      of PRIVATEWIDGETSETTING exists in the XCenter's
 *      instance data (in the _pllWidgetSettings field).
 *
 *      See ctrpStuffSettings for details.
 *
 *      The caller must lock the XCenter himself.
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
 */

ULONG ctrpUnstuffSettings(XCenter *somSelf)
{
    ULONG ulrc = 0;
    XCenterData *somThis = XCenterGetData(somSelf);

    // _Pmpf((__FUNCTION__": entering, _pszPackedWidgetSettings is %lX", _pszPackedWidgetSettings));

    if (_pszPackedWidgetSettings)
    {
        PSZ p = _pszPackedWidgetSettings;
        PPRIVATEWIDGETSETTING pPrevSetting = NULL;

        ctrpLoadClasses();      // just to make sure; ctrpFindClass would otherwise
                                // load them for each loop

        while (    (*p)
                && (p < _pszPackedWidgetSettings + _cbPackedWidgetSettings)
              )
        {
            ULONG ulClassLen = strlen(p),
                  ulSetupLen = 0;

            // find class from PM class name
            PPRIVATEWIDGETSETTING pSetting = NEW(PRIVATEWIDGETSETTING);
            if (pSetting)
            {
                ZERO(pSetting);

                // special check: if the class name starts with
                // the special TRAY marker, it's not a class name
                // but a list of tray settings for the previous
                // tray widget.... so process this instead
                if (    (ulClassLen > sizeof(TRAYSETTINGSMARKER) - 1)
                     && (!memcmp(p, TRAYSETTINGSMARKER, sizeof(TRAYSETTINGSMARKER) - 1))
                   )
                {
                    // this is the marker:
                    // decode it and add the tray settings to the
                    // PRIVATEWIDGETSETTING from the last loop
                    _Pmpf((__FUNCTION__ ": found tray settings %s",
                            p));

                    if (pPrevSetting)
                        DecodeTraySettings(p + sizeof(TRAYSETTINGSMARKER) - 1,
                                           ulClassLen,
                                           pPrevSetting);

                    // go beyond the terribly long tray string
                    p += ulClassLen + 1;    // go beyond null byte

                    // now we're at the real class name... get
                    // its length
                    ulClassLen = strlen(p);
                }

                pSetting->Public.pszWidgetClass = strdup(p);
            }

            // after class name, we have the setup string
            p += ulClassLen + 1;    // go beyond null byte

            ulSetupLen = strlen(p);
            if (pSetting)
            {
                if (ulSetupLen > 1)
                    pSetting->Public.pszSetupString = strdup(p);
                else
                    pSetting->Public.pszSetupString = NULL;

                // store in list
                AddWidgetSetting(somSelf,
                                 pSetting,
                                 -1,
                                 NULL,
                                 NULL);
            }

            // store this setting for the next loop,
            // in case it's for a tray
            pPrevSetting = pSetting;

            // go for next setting
            p += ulSetupLen + 1;    // go beyond null byte
        }

        // remove the packed settings;
        // from now on we'll use the list
        free(_pszPackedWidgetSettings);
        _pszPackedWidgetSettings = NULL;

        ctrpFreeClasses();
    }

    return (ulrc);
}

/*
 *@@ ctrpQuerySettingsList:
 *      returns the list of XWPWIDGETSETTING structures
 *      from the XCenter instance data. This unpacks
 *      the binary array if called for the first time.
 *
 *      The caller must lock the XCenter himself.
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
 */

PLINKLIST ctrpQuerySettingsList(XCenter *somSelf)
{
    XCenterData *somThis = XCenterGetData(somSelf);

    if (!_pllAllWidgetSettings)
    {
        // list not created yet:
        _pllAllWidgetSettings = lstCreate(FALSE);       // changed

        ctrpUnstuffSettings(somSelf);
    }

    return (_pllAllWidgetSettings);
}

/* ******************************************************************
 *
 *   XCenter view implementation
 *
 ********************************************************************/

/*
 *@@ ctrpQueryWidgetIndexFromHWND:
 *      returns the index (or indices) of the widget with the
 *      specified window handle. All indices are zero-based,
 *      i.e. the first widget has the 0 index.
 *
 *      If the specified hwnd is a subwidget which resides
 *      in a tray,
 *
 *      --  *pulTrayWidgetIndex receives the "root"
 *          index of the tray widget;
 *
 *      --  *pulTrayIndex receives the index of the tray that the
 *          widget resides in (which will always be the
 *          current tray, because per definition, the
 *          subwidget wouldn't exist as a view otherwise);
 *
 *      --  *pulWidgetIndex receives the index of the subwidget
 *          within that tray.
 *
 *      Otherwise, if hwndWidget is a "root" widget,
 *      *pulTrayWidgetIndex and *pulTrayIndex are set to -1,
 *      and *pulWidgetIndex receives the index of the widget.
 *
 *      This only works if an XCenter view is currently
 *      open, of course.
 *
 *      Returns FALSE if the widget view was not found, e.g.
 *      because the window handle is invalid or the XCenter
 *      isn't even currently open.
 *
 *      May run on any thread.
 *
 *@@added V0.9.7 (2000-12-04) [umoeller]
 *@@changed V0.9.13 (2001-06-19) [umoeller]: added tray support
 *@@changed V0.9.14 (2001-08-01) [umoeller]: this never worked, fixed prototype for proper indices
 */

BOOL ctrpQueryWidgetIndexFromHWND(XCenter *somSelf,
                                  HWND hwndWidget,
                                  PULONG pulTrayWidgetIndex,
                                  PULONG pulTrayIndex,
                                  PULONG pulWidgetIndex)
{
    BOOL        fFound = FALSE;

    WPSHLOCKSTRUCT Lock;
    TRY_LOUD(excpt1)
    {
        if (LOCK_OBJECT(Lock, somSelf))
        {
            XCenterData *somThis = XCenterGetData(somSelf);
            if (_pvOpenView)
            {
                // XCenter view currently open:
                PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)_pvOpenView;

                // go thru the root widgets
                PLISTNODE   pViewNode = lstQueryFirstNode(&pXCenterData->llWidgets);
                ULONG       ulRootThis = 0;

                while (    (pViewNode)
                        && (!fFound)
                      )
                {
                    PPRIVATEWIDGETVIEW pView = (PPRIVATEWIDGETVIEW)pViewNode->pItemData;
                    if (pView->Widget.hwndWidget == hwndWidget)
                    {
                        // root widget found:
                        *pulTrayWidgetIndex = -1;
                        *pulTrayIndex = -1;
                        *pulWidgetIndex = ulRootThis;
                        fFound = TRUE;
                        break;
                    }
                    else
                    {
                        // if the current widget is a tray widget,
                        // we need to seach its member widgets too
                        // V0.9.13 (2001-06-19) [umoeller]
                        if (pView->pllSubwidgetViews)
                        {
                            // this is a tray:
                            // search the member widgets
                            ULONG ulSubThis = 0;
                            PLISTNODE pSubNode = lstQueryFirstNode(pView->pllSubwidgetViews);
                            while (pSubNode)
                            {
                                PPRIVATEWIDGETVIEW pSubView = (PPRIVATEWIDGETVIEW)pSubNode->pItemData;
                                if (pSubView->Widget.hwndWidget == hwndWidget)
                                {
                                    // OK, this is it:
                                    // to find the current tray, we must also
                                    // query the tray widget's _settings_... sigh
                                    PLINKLIST pllSettings;
                                    PPRIVATEWIDGETSETTING pTraySetting;
                                    if (    (pllSettings = ctrpQuerySettingsList(somSelf))
                                         && (pTraySetting = lstItemFromIndex(pllSettings,
                                                                             ulRootThis))
                                       )
                                    {
                                        *pulTrayWidgetIndex = ulRootThis;
                                        *pulTrayIndex = pTraySetting->ulCurrentTray;
                                        *pulWidgetIndex = ulSubThis;
                                        fFound = TRUE;
                                        break;
                                    }
                                }

                                pSubNode = pSubNode->pNext;
                                ulSubThis++;
                            }

                            if (fFound)
                                // found it: exit outer loop too
                                break;
                        }
                    }

                    pViewNode = pViewNode->pNext;
                    ulRootThis++;
                } // end while (    (pViewNode)  ...
            } // end if (_pvOpenView)
        }
    }
    CATCH(excpt1) {} END_CATCH();

    if (Lock.fLocked)
        _wpReleaseObjectMutexSem(Lock.pObject);

    return (fFound);
}

/*
 *@@ ctrpFreeWidgets:
 *      frees the widget settings data. Called from
 *      XCenter::wpUnInitData.
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
 *@@changed V0.9.13 (2001-06-21) [umoeller]: fixed memory leak, this wasn't freed properly
 *@@changed V0.9.14 (2001-08-20) [pr]: fixed endless loop/crash
 */

VOID ctrpFreeWidgets(XCenter *somSelf)
{
    WPSHLOCKSTRUCT Lock;
    TRY_LOUD(excpt1)
    {
        if (LOCK_OBJECT(Lock, somSelf))
        {
            XCenterData *somThis = XCenterGetData(somSelf);
            // _Pmpf((__FUNCTION__ ": entering, _pllWidgetSettings is %lX", _pllWidgetSettings));
            if (_pllAllWidgetSettings)
            {
                PLINKLIST pll = _pllAllWidgetSettings;
                PLISTNODE pNode = lstQueryFirstNode(pll);
                while (pNode)
                {
                    PPRIVATEWIDGETSETTING pSetting = (PPRIVATEWIDGETSETTING)pNode->pItemData;
                    FreeSettingData(pSetting);
                    free(pSetting);

                    pNode = pNode->pNext; // V0.9.14
                }

                // now nuke the main list; the LISTNODEs are still left,
                // but the item data has been removed above
                lstFree((LINKLIST**)&_pllAllWidgetSettings);
            }
        }
    }
    CATCH(excpt1) {} END_CATCH();

    if (Lock.fLocked)
        _wpReleaseObjectMutexSem(Lock.pObject);
}

/*
 *@@ ctrpQueryWidgets:
 *      implementation for XCenter::xwpQueryWidgets.
 *
 *@@added V0.9.7 (2000-12-08) [umoeller]
 */

PVOID ctrpQueryWidgets(XCenter *somSelf,
                       PULONG pulCount)
{
    PXCENTERWIDGETSETTING paSettings = NULL;

    WPSHLOCKSTRUCT Lock;
    TRY_LOUD(excpt1)
    {
        if (LOCK_OBJECT(Lock, somSelf))
        {
            PLINKLIST pllSettings = ctrpQuerySettingsList(somSelf);
            PLISTNODE pNode = lstQueryFirstNode(pllSettings);
            ULONG cSettings = lstCountItems(pllSettings);
            paSettings = malloc(sizeof(XCENTERWIDGETSETTING) * cSettings);
            if (paSettings)
            {
                ULONG ul = 0;
                for (;
                     ul < cSettings;
                     ul++)
                {
                    PPRIVATEWIDGETSETTING pSource = (PPRIVATEWIDGETSETTING)pNode->pItemData;
                    paSettings[ul].pszWidgetClass = strhdup(pSource->Public.pszWidgetClass);
                    paSettings[ul].pszSetupString = strhdup(pSource->Public.pszSetupString);

                    pNode = pNode->pNext;
                }
            }
        }
        else
        {
            // we get here on crashes
            if (paSettings)
            {
                free(paSettings);
                paSettings = NULL;
            }
        }
    }
    CATCH(excpt1) {} END_CATCH();

    if (Lock.fLocked)
        _wpReleaseObjectMutexSem(Lock.pObject);

    return (paSettings);
}

/*
 *@@ ctrpFreeWidgetsBuf:
 *      implementation for XCenter::FreeWidgetsBuf.
 *
 *      May run on any thread.
 *
 *@@added V0.9.7 (2000-12-08) [umoeller]
 */

VOID ctrpFreeWidgetsBuf(PVOID pBuf,
                        ULONG ulCount)
{
    // no semaphore needed here, we created a copy above
    ULONG ul = 0;
    PXCENTERWIDGETSETTING paSettings = (PXCENTERWIDGETSETTING)pBuf;
    for (;
         ul < ulCount;
         ul++)
    {
        PXCENTERWIDGETSETTING pThis = &paSettings[ul];
        if (pThis->pszWidgetClass)
            free(pThis->pszWidgetClass);
        if (pThis->pszSetupString)
            free(pThis->pszSetupString);
    }

    free(paSettings);
}

/*
 *@@ ctrpInsertWidget:
 *      implementation for XCenter::xwpInsertWidget.
 *
 *      May run on any thread.
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
 *@@changed V0.9.9 (2001-03-09) [umoeller]: fixed mutex deadlock
 */

BOOL ctrpInsertWidget(XCenter *somSelf,
                      ULONG ulBeforeIndex,
                      const char *pcszWidgetClass,
                      const char *pcszSetupString)
{
    BOOL brc = FALSE;

    PPRIVATEWIDGETSETTING pSetting = NULL;
    ULONG   ulNewItemCount = 0,
            ulWidgetIndex = 0;

    XCenterData *somThis = XCenterGetData(somSelf);

    WPSHLOCKSTRUCT Lock;
    TRY_LOUD(excpt1)
    {
        if (LOCK_OBJECT(Lock, somSelf))
        {
            // _Pmpf((__FUNCTION__ ": entering with %s, %s",
            //         (pcszWidgetClass) ? pcszWidgetClass : "NULL",
               //      (pcszSetupString) ? pcszSetupString : "NULL"));

            if (pcszWidgetClass)
            {
                pSetting = NEW(PRIVATEWIDGETSETTING);

                if (pSetting)
                {
                    ZERO(pSetting);

                    pSetting->Public.pszWidgetClass = strhdup(pcszWidgetClass);
                    pSetting->Public.pszSetupString = strhdup(pcszSetupString);
                                            // can be NULL

                    // add new widget setting to internal linked list
                    AddWidgetSetting(somSelf,
                                     pSetting,
                                     ulBeforeIndex,
                                     &ulNewItemCount,
                                     &ulWidgetIndex);

                    brc = TRUE;
                }
            }
        }
    }
    CATCH(excpt1) {} END_CATCH();

    if (Lock.fLocked)
        _wpReleaseObjectMutexSem(Lock.pObject);

    if (pSetting)
    {
        // added successfully:
        // we must update the view outside the mutex block
        // V0.9.9 (2001-03-09) [umoeller]
        if (_pvOpenView)
        {
            // XCenter view currently open:
            PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)_pvOpenView;
            PXCENTERGLOBALS pGlobals = &pXCenterData->Globals;
            // we must send msg instead of calling ctrpCreateWidgetWindow
            // directly, because that func may only run on the
            // XCenter GUI thread
            WinSendMsg(pGlobals->hwndClient,
                       XCM_CREATEWIDGET,
                       (MPARAM)pSetting,
                       (MPARAM)ulWidgetIndex);
                    // new widget is invisible, and at a random position
            WinPostMsg(pGlobals->hwndClient,
                       XCM_REFORMAT,
                       (MPARAM)(XFMF_RECALCHEIGHT
                                | XFMF_REPOSITIONWIDGETS
                                | XFMF_SHOWWIDGETS),
                       0);
        }

        // save instance data (with that linked list)
        _wpSaveDeferred(somSelf);

        // update the "widgets" notebook page, if open
        ntbUpdateVisiblePage(somSelf, SP_XCENTER_WIDGETS);
    }

    return (brc);
}

/*
 *@@ ctrpRemoveWidget:
 *      implementation for XCenter::xwpRemoveWidget.
 *
 *      This can run on any thread.
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
 *@@changed V0.9.13 (2001-06-21) [umoeller]: adjusted for tray support
 */

BOOL ctrpRemoveWidget(XCenter *somSelf,
                      ULONG ulIndex)
{
    BOOL brc = FALSE;
    WPSHLOCKSTRUCT Lock;
    TRY_LOUD(excpt1)
    {
        if (LOCK_OBJECT(Lock, somSelf))
        {
            XCenterData *somThis = XCenterGetData(somSelf);
            PLINKLIST   pllWidgetSettings = ctrpQuerySettingsList(somSelf);
            PLISTNODE   pSettingsNode = lstNodeFromIndex(pllWidgetSettings, ulIndex);
            HWND        hwndXCenterClient = NULLHANDLE;

            if (!pSettingsNode)
                cmnLog(__FILE__, __LINE__, __FUNCTION__,
                       "Invalid widget index 0x%lX.", ulIndex);
            else
            {
                // widget exists:

                PPRIVATEWIDGETSETTING pSetting = (PPRIVATEWIDGETSETTING)pSettingsNode->pItemData;

                if (_pvOpenView)
                {
                    // XCenter view currently open:
                    PPRIVATEWIDGETVIEW pView;
                    PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)_pvOpenView;
                    PXCENTERGLOBALS pGlobals = &pXCenterData->Globals;

                    // store XCenter client so we can update the client
                    // at the bottom
                    hwndXCenterClient = pGlobals->hwndClient;

                    pView = lstItemFromIndex(&pXCenterData->llWidgets,
                                             ulIndex);
                    if (pView)
                    {
                        // we must send a msg instead of doing WinDestroyWindow
                        // directly because only the XCenter GUI thread can
                        // destroy the window

                        // unlock the object first!! otherwise we get a deadlock
                        _wpReleaseObjectMutexSem(Lock.pObject);

                        WinSendMsg(pGlobals->hwndClient,
                                   XCM_DESTROYWIDGET,
                                   (MPARAM)pView->Widget.hwndWidget,
                                   0);
                            // the window is responsible for cleaning up pView->pUser;
                            // ctrDefWidgetProc will also free pView and remove it
                            // from the widget views list
                        _wpRequestObjectMutexSem(somSelf, SEM_INDEFINITE_WAIT);
                    }
                } // end if (_hwndOpenView)

                // clear all the setting members, including
                // tray and subwidget stuff
                FreeSettingData(pSetting);      // V0.9.13 (2001-06-21) [umoeller]

                brc = lstRemoveNode(pllWidgetSettings, pSettingsNode);

                // no auto-free, so free setting
                // V0.9.13 (2001-06-21) [umoeller]
                free(pSetting);

                if (brc)
                    // save instance data (with that linked list)
                    _wpSaveDeferred(somSelf);

                // update the "widgets" notebook page, if open
                ntbUpdateVisiblePage(somSelf, SP_XCENTER_WIDGETS);

            } // end if (pSettingsNode)

            if (hwndXCenterClient)
            {
                // we found the open view above:
                WinPostMsg(hwndXCenterClient,
                           XCM_REFORMAT,
                           (MPARAM)(XFMF_RECALCHEIGHT | XFMF_REPOSITIONWIDGETS),
                           0);
            }
        }
    }
    CATCH(excpt1) {} END_CATCH();

    if (Lock.fLocked)
        _wpReleaseObjectMutexSem(Lock.pObject);

    return (brc);
}

/*
 *@@ ctrpMoveWidget:
 *      implementation for XCenter::xwpMoveWidget.
 *
 *@@added V0.9.7 (2000-12-10) [umoeller]
 *@@changed V0.9.9 (2001-03-10) [umoeller]: this confused the settings with ulBeforeIndex, fixed
 */

BOOL ctrpMoveWidget(XCenter *somSelf,
                    ULONG ulIndex2Move,
                    ULONG ulBeforeIndex)
{
    BOOL brc = FALSE;

    WPSHLOCKSTRUCT Lock;
    TRY_LOUD(excpt1)
    {
        if (LOCK_OBJECT(Lock, somSelf))
        {
            PLINKLIST pllWidgetSettings = ctrpQuerySettingsList(somSelf);
            PLISTNODE pSettingsNode = lstNodeFromIndex(pllWidgetSettings, ulIndex2Move);
            PXCENTERWINDATA pXCenterData = NULL;

            if (!pSettingsNode)
                // index invalid: use last widget instead
                pSettingsNode = lstNodeFromIndex(pllWidgetSettings,
                                                 lstCountItems(pllWidgetSettings) - 1);

            if (pSettingsNode)
            {
                // widget exists:
                XCenterData *somThis = XCenterGetData(somSelf);
                PPRIVATEWIDGETSETTING pSetting = (PPRIVATEWIDGETSETTING)pSettingsNode->pItemData,
                                      pNewSetting = NULL;

                if (_pvOpenView)
                {
                    // XCenter view currently open:
                    PLISTNODE pViewNode;
                    pXCenterData = (PXCENTERWINDATA)_pvOpenView;
                    pViewNode = lstNodeFromIndex(&pXCenterData->llWidgets,
                                                 ulIndex2Move);
                    if (pViewNode)
                    {
                        // the list is NOT in auto-free mode;
                        // re-insert the node at the new position
                        lstInsertItemBefore(&pXCenterData->llWidgets,
                                            pViewNode->pItemData,
                                            ulBeforeIndex);
                        // remove old node
                        lstRemoveNode(&pXCenterData->llWidgets, pViewNode);
                    }
                } // end if (_hwndOpenView)

                // move the setting...
                pNewSetting = (PPRIVATEWIDGETSETTING)malloc(sizeof(PRIVATEWIDGETSETTING));
                // cheap trick: we simply overwrite the pointers
                // in the new setting with the existing ones so
                // we won't have to reallocate everything
                memcpy(pNewSetting, pSetting, sizeof(*pNewSetting));
                // fixed order of calls here V0.9.9 (2001-03-10) [umoeller]...
                // above, we did "insert" and then "remove"; this used to be reverse,
                // which got the items badly confused...
                lstInsertItemBefore(pllWidgetSettings,
                                    pNewSetting,
                                    ulBeforeIndex);
                brc = lstRemoveNode(pllWidgetSettings, pSettingsNode);
                // and free the old setting V0.9.13 (2001-06-21) [umoeller]
                // the member pointers are now all in the new setting
                free(pSetting);

                if (brc)
                    // save instance data (with that linked list)
                    _wpSaveDeferred(somSelf);

                // update the "widgets" notebook page, if open
                ntbUpdateVisiblePage(somSelf, SP_XCENTER_WIDGETS);

            } // end if (pSettingsNode)

            if (pXCenterData)
            {
                // we found the open view above:
                WinPostMsg(pXCenterData->Globals.hwndClient,
                           XCM_REFORMAT,
                           (MPARAM)(XFMF_REPOSITIONWIDGETS),
                           0);
            }
        }
    }
    CATCH(excpt1) {} END_CATCH();

    if (Lock.fLocked)
        _wpReleaseObjectMutexSem(Lock.pObject);

    return (brc);
}

/*
 *@@ ctrpSetPriority:
 *
 *@@added V0.9.7 (2001-01-03) [umoeller]
 */

BOOL ctrpSetPriority(XCenter *somSelf,
                     ULONG ulClass,
                     LONG lDelta)
{
    BOOL brc = FALSE;

    WPSHLOCKSTRUCT Lock;
    TRY_LOUD(excpt1)
    {
        if (LOCK_OBJECT(Lock, somSelf))
        {
            XCenterData *somThis = XCenterGetData(somSelf);
            _ulPriorityClass = ulClass;
            _lPriorityDelta = lDelta;

            if (_pvOpenView)
            {
                // PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)_pvOpenView;
                DosSetPriority(PRTYS_THREAD,
                               ulClass,
                               lDelta,
                               _tidRunning);   // tid of XCenter GUI thread
            }
        }
        else
            brc = FALSE;
    }
    CATCH(excpt1) {} END_CATCH();

    if (Lock.fLocked)
        _wpReleaseObjectMutexSem(Lock.pObject);

    return (brc);
}

/*
 *@@ ctrpAddWidgetsMenu:
 *      adds the "Add widget" menu to the specified
 *      menu and inserts menu items for all the available
 *      widget classes into that new submenu.
 *
 *      Returns the submenu window handle.
 *
 *@@added V0.9.13 (2001-06-19) [umoeller]
 */

HWND ctrpAddWidgetsMenu(XCenter *somSelf,
                        HWND hwndMenu,
                        SHORT sPosition,        // in: position where to insert (can be MIT_END)
                        BOOL fTrayableOnly)     // in: if TRUE, only classes with WGTF_TRAYABLE will be shown
{
    HWND hwndWidgetsSubmenu;

    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    PLISTNODE       pClassNode;
    ULONG           ulIndex = 0;

    ctrpLoadClasses();

    hwndWidgetsSubmenu =  winhInsertSubmenu(hwndMenu,
                                            sPosition,
                                            (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_XWPVIEW),
                                                    // fixed ID for sliding menus
                                                    // V0.9.13 (2001-06-14) [umoeller]
                                            cmnGetString(ID_XSSI_ADDWIDGET),
                                            MIS_TEXT,
                                            0, NULL, 0, 0);

    pClassNode = lstQueryFirstNode(&G_llWidgetClasses);
    while (pClassNode)
    {
        ULONG ulAttr = 0;
        PPRIVATEWIDGETCLASS pClass
            = (PPRIVATEWIDGETCLASS)pClassNode->pItemData;

        // should this be added?
        if (pClass->Public.ulClassFlags & WGTF_NOUSERCREATE)
            ulAttr |= MIA_DISABLED;
        else
        {
            // if caller wants trayable widgets only,
            // disable non-trayable classes
            if (    (fTrayableOnly)
                 && ((pClass->Public.ulClassFlags & WGTF_TRAYABLE) == 0)
               )
                ulAttr = MIA_DISABLED;
            else
                // if a widget of the same class already exists
                // and the class doesn't allow multiple instances,
                // disable this menu item
                if (pClass->Public.ulClassFlags & WGTF_UNIQUEPERXCENTER)
                {
                    // "unique" flag set: then check all widgets
                    PLINKLIST pllWidgetSettings = ctrpQuerySettingsList(somSelf);
                    PLISTNODE pNode = lstQueryFirstNode(pllWidgetSettings);
                    while (pNode)
                    {
                        PPRIVATEWIDGETSETTING pSettingThis = (PPRIVATEWIDGETSETTING)pNode->pItemData;
                        if (strcmp(pSettingThis->Public.pszWidgetClass,
                                   pClass->Public.pcszWidgetClass)
                               == 0)
                        {
                            // widget of this class exists:
                            ulAttr = MIA_DISABLED;
                            break;
                        }
                        pNode = pNode->pNext;
                    }
                }
        }

        winhInsertMenuItem(hwndWidgetsSubmenu,
                           MIT_END,
                           pGlobalSettings->VarMenuOffset +
                                 ID_XFMI_OFS_VARIABLE
                                 + (ulIndex++),
                           (PSZ)pClass->Public.pcszClassTitle,
                           MIS_TEXT,
                           ulAttr);

        pClassNode = pClassNode->pNext;
    }

    ctrpFreeClasses();

    return (hwndWidgetsSubmenu);
}

/*
 *@@ ctrpModifyPopupMenu:
 *      implementation for XCenter::wpModifyPopupMenu.
 *
 *      This can run on any thread.
 *
 *@@added V0.9.7 (2000-12-02) [umoeller]
 *@@changed V0.9.13 (2001-06-14) [umoeller]: fixed "add widget" submenu with sliding focus
 *@@changed V0.9.13 (2001-06-19) [umoeller]: extracted ctrpAddWidgetsMenu
 */

BOOL ctrpModifyPopupMenu(XCenter *somSelf,
                         HWND hwndMenu)
{
    BOOL brc = TRUE;
    WPSHLOCKSTRUCT Lock;
    TRY_LOUD(excpt1)
    {
        if (LOCK_OBJECT(Lock, somSelf))
        {
            XCenterData *somThis = XCenterGetData(somSelf);
            MENUITEM mi;
            PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
            // get handle to the "Open" submenu in the
            // the popup menu
            if (winhQueryMenuItem(hwndMenu,
                                  WPMENUID_OPEN,
                                  TRUE,
                                  &mi))
            {
                // mi.hwndSubMenu now contains "Open" submenu handle,
                // which we add items to now
                // PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();
                winhInsertMenuItem(mi.hwndSubMenu, MIT_END,
                                   (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_XWPVIEW),
                                   ENTITY_XCENTER,
                                   MIS_TEXT, 0);
            }

            if (_fShowingOpenViewMenu)
            {
                // context menu for open XCenter client:

                winhInsertMenuSeparator(hwndMenu,
                                        MIT_END,
                                        (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_SEPARATOR));

                // add the "Add widget" submenu with all the available widget classes
                ctrpAddWidgetsMenu(somSelf,
                                   hwndMenu,
                                   MIT_END,
                                   FALSE);      // all widgets

                // add "Close"
                cmnAddCloseMenuItem(hwndMenu);

                _fShowingOpenViewMenu = FALSE;
            }
        }
    }
    CATCH(excpt1) {} END_CATCH();

    if (Lock.fLocked)
        _wpReleaseObjectMutexSem(Lock.pObject);

    return (brc);
}

/*
 *@@ ctrpFindClassFromMenuCommand:
 *      returns the PRIVATEWIDGETCLASS which corresponds
 *      to the given menu command value from the "add widget"
 *      submenu.
 *
 *@@added V0.9.13 (2001-06-19) [umoeller]
 */

PPRIVATEWIDGETCLASS ctrpFindClassFromMenuCommand(USHORT usCmd)
{
    PPRIVATEWIDGETCLASS pClass = NULL;
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    if (    (usCmd >= pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_VARIABLE)
         && (usCmd < (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_VARIABLE
                        + lstCountItems(&G_llWidgetClasses))
            )
       )
    {
        // yes: get the corresponding item
        USHORT usIndex = usCmd - (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_VARIABLE);
        pClass = (PPRIVATEWIDGETCLASS)lstItemFromIndex(&G_llWidgetClasses,
                                                       usIndex);
    }

    return (pClass);
}

/* ******************************************************************
 *
 *   XCenter view thread
 *
 ********************************************************************/

/*
 *@@ ctrp_fntXCenter:
 *      the XCenter thread. One of these gets created
 *      by ctrpCreateXCenterView for each XCenter that
 *      is opened and KEEPS RUNNING while the XCenter
 *      is open. The XCenter frame posts WM_QUIT when
 *      the XCenter is closed.
 *
 *      On input, ctrpCreateXCenterView specifies the
 *      somSelf field in XCENTERWINDATA. Also, the USEITEM
 *      ulView is set.
 *
 *      ctrpCreateXCenterView waits on XCENTERWINDATA.hevRunning
 *      and will then expect the XCenter's frame window in
 *      XCENTERWINDATA.Globals.hwndFrame so it can return to
 *      XCenter::wpOpen once the XCenter has been created
 *      (while this thread keeps running after that).
 *
 *      All this is nicely complex.
 *
 *      This thread is created with a PM message queue.
 *
 *@@added V0.9.7 (2001-01-03) [umoeller]
 */

void _Optlink ctrp_fntXCenter(PTHREADINFO ptiMyself)
{
    BOOL fCreated = FALSE;
    PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)ptiMyself->ulData;

    /* #ifdef __DEBUG__
        DosBeep(3000, 30);
    #endif */

    if (pXCenterData)
    {
        XCenterData *somThis = XCenterGetData(pXCenterData->somSelf);
        PXCENTERGLOBALS pGlobals = &pXCenterData->Globals;
        SWP swpFrame;

        TRY_LOUD(excpt1)
        {
            // set priority to what user wants
            DosSetPriority(PRTYS_THREAD,
                           _ulPriorityClass,
                           _lPriorityDelta,
                           0);      // current thread

            // pXCenterData->tidXCenter = ptiMyself->tid;
            // _tid = ptiMyself->tid;          // moved this here V0.9.9 (2001-04-04) [umoeller]
                   // removed, thrCreate has set this

            // initialize various data:

            ctrpLoadClasses();
                    // matching "unload" is in WM_DESTROY of the client

            lstInit(&pXCenterData->llWidgets,
                    FALSE);      // no auto-free

            pGlobals->pCountrySettings = &G_CountrySettings;
            nlsQueryCountrySettings(&G_CountrySettings);

            pXCenterData->cxFrame = winhQueryScreenCX();

            pGlobals->cxMiniIcon = WinQuerySysValue(HWND_DESKTOP, SV_CXICON) / 2;

            pGlobals->lcol3DDark = WinQuerySysColor(HWND_DESKTOP, SYSCLR_BUTTONDARK, 0);
            pGlobals->lcol3DLight = WinQuerySysColor(HWND_DESKTOP, SYSCLR_BUTTONLIGHT, 0);

            // copy client background color V0.9.13 (2001-06-19) [umoeller]
            pGlobals->lcolClientBackground = _lcolClientBackground;

            // copy somSelf
            pGlobals->pvSomSelf = pXCenterData->somSelf;

            // copy display style so that the widgets can see it
            CopyDisplayStyle(pXCenterData, somThis);

            memset(&swpFrame, 0, sizeof(swpFrame));
            swpFrame.x = 0;
            swpFrame.cx = pXCenterData->cxFrame;
            if (_ulPosition == XCENTER_BOTTOM)
            {
                swpFrame.y = 0;
                swpFrame.cy = 20;       // changed later
            }
            else
            {
                // at the top:
                swpFrame.cy = 20;
                swpFrame.y = winhQueryScreenCY() - swpFrame.cy;
            }

            // now go create XCenter frame and client
            pGlobals->hwndFrame
                = winhCreateStdWindow(HWND_DESKTOP, // frame's parent
                                      &swpFrame,
                                      FCF_NOBYTEALIGN,
                                      _ulWindowStyle,   // frame style
                                      _wpQueryTitle(pXCenterData->somSelf),
                                      0,            // resources ID
                                      WC_XCENTER_CLIENT, // client class
                                      WS_VISIBLE,       // client style
                                      0,                // client ID
                                      pXCenterData,     // client control data
                                      &pGlobals->hwndClient);
                                            // out: client window

            if ((pGlobals->hwndFrame) && (pGlobals->hwndClient))
                // frame and client created:
                fCreated = TRUE;
        }
        CATCH(excpt1) {} END_CATCH();

        // in any case (even if we crashed), post the event
        // semaphore to notify thread 1 that we're done; this
        // is waiting in XCenter::wpOpen to return the
        // frame window handle
        DosPostEventSem(pXCenterData->hevRunning);

        if (fCreated)
        {
            // successful above:
            // protect the entire thread with the exception
            // handler while the XCenter is running
            // (this includes the thread's message loop)
            TRY_LOUD(excpt1)
            {
                PXTIMERSET pTimerSet;
                SWP swpClient;
                pGlobals->hab = WinQueryAnchorBlock(pGlobals->hwndFrame);

                // store win data in frame's QWL_USER
                // (client has done this in its WM_CREATE)
                WinSetWindowPtr(pGlobals->hwndFrame,
                                QWL_USER,
                                pXCenterData);

                // create XTimerSet
                G_pLastXTimerSet
                = pTimerSet
                = pGlobals->pvXTimerSet = tmrCreateSet(pGlobals->hwndFrame,
                                                       TIMERID_XTIMERSET);

                // store client area;
                // we must use WinQueryWindowPos because WinQueryWindowRect
                // returns a 0,0,0,0 rectangle for invisible windows
                WinQueryWindowPos(pGlobals->hwndClient, &swpClient);
                pGlobals->cyInnerClient = swpClient.cy;
                            // this will change if CreateWidgets creates
                            // any widgets at all

                // if a font was set by the user for the entire XCenter,
                // set it on the client now... this was maybe saved in
                // the XCenter instance data from a previous WM_PRESPARAMCHANGED
                // V0.9.9 (2001-03-07) [umoeller]
                if (_pszClientFont)
                    winhSetWindowFont(pGlobals->hwndClient,
                                      _pszClientFont);
                // we need not care about the color because WM_PAINT
                // would use the instance variable directly also...
                // but we can make the widgets inherit that color
                // so let's set that also V0.9.9 (2001-03-07) [umoeller]
                WinSetPresParam(pGlobals->hwndClient,
                                PP_BACKGROUNDCOLOR,
                                sizeof(ULONG),
                                &_lcolClientBackground);

                // add the use list item to the object's use list;
                // this struct has been zeroed above
                cmnRegisterView(pXCenterData->somSelf,
                                &pXCenterData->UseItem,
                                pXCenterData->ViewItem.view,
                                            // has been set already, so we pass this in
                                pGlobals->hwndFrame,
                                ENTITY_XCENTER);

                // subclass the frame AGAIN (the WPS has subclassed it
                // with wpRegisterView)
                if (pXCenterData->pfnwpFrameOrig = WinSubclassWindow(pGlobals->hwndFrame,
                                                                     fnwpXCenterMainFrame))
                {
                    // subclassing succeeded:
                    QMSG qmsg;

                    // register tooltip class
                    ctlRegisterTooltip(pGlobals->hab);

                    // create tooltip window
                    pGlobals->hwndTooltip
                            = WinCreateWindow(HWND_DESKTOP,  // parent
                                              COMCTL_TOOLTIP_CLASS, // wnd class
                                              "",            // window text
                                              XWP_TOOLTIP_STYLE,
                                                   // tooltip window style (common.h)
                                              10, 10, 10, 10,    // window pos and size, ignored
                                              pGlobals->hwndClient,
                                                        // owner window -- important!
                                              HWND_TOP,      // hwndInsertBehind, ignored
                                              ID_XCENTER_TOOLTIP, // window ID, optional
                                              NULL,          // control data
                                              NULL);         // presparams
                                    // destroyed in WM_DESTROY of client

                    if (!pGlobals->hwndTooltip)
                        cmnLog(__FILE__, __LINE__, __FUNCTION__,
                               "Cannot create tooltip.");
                    else
                    {
                        // tooltip successfully created:
                        // PLISTNODE pNode;
                        // TOOLINFO    ti = {0};
                        // ULONG ulStdFlags = 0;

                        // go create widgets
                        CreateWidgets(pXCenterData);
                                // this sets the frame's size, invisibly

                        // go thru all widgets and add them as tools
                        // V0.9.13 (2001-06-21) [umoeller]
                        // moved this to ctrpCreateWidgetWindow where it
                        // belongs; otherwise tooltips won't work for widgets
                        // which are added later

                        // set tooltip timers
                        WinSendMsg(pGlobals->hwndTooltip,
                                   TTM_SETDELAYTIME,
                                   (MPARAM)TTDT_AUTOPOP,
                                   (MPARAM)(40*1000));        // 40 secs for autopop (hide)
                    }

                    if (_ulWindowStyle & WS_ANIMATE)
                    {
                        // user wants animation:
                        // start timer on the frame which will unfold
                        // the XCenter from the left by repositioning it
                        // with each timer tick... when this is done,
                        // the next timer(s) will be started automatically
                        // until the frame is fully showing and ctrpReformat
                        // will eventually be called
                        tmrStartXTimer(pTimerSet,
                                       pGlobals->hwndFrame,
                                       TIMERID_UNFOLDFRAME,
                                       50);
                    }
                    else
                    {
                        // no animation: format the widgets NOW
                        ctrpReformat(pXCenterData,
                                     XFMF_RECALCHEIGHT
                                       | XFMF_REPOSITIONWIDGETS
                                       | XFMF_SHOWWIDGETS);
                                // frame is still hidden

                        WinSetWindowPos(pGlobals->hwndFrame,
                                        HWND_TOP,
                                        0,
                                        pXCenterData->yFrame,
                                        pXCenterData->cxFrame,
                                        pXCenterData->cyFrame,
                                        SWP_MOVE | SWP_SIZE | SWP_SHOW | SWP_ZORDER);

                        // OK, XCenter is ready now.
                        pXCenterData->fFrameFullyShown = TRUE;
                        // this resizes the desktop now and starts auto-hide:
                        ctrpReformat(pXCenterData, 0);
                    }

                    // now enter the message loop;
                    // we stay in here until fnwpXCenterMainFrame
                    // posts WM_QUIT on destroy or something bad
                    // happens
                    while (WinGetMsg(ptiMyself->hab, &qmsg, NULLHANDLE, 0, 0))
                        // loop until WM_QUIT
                        WinDispatchMsg(ptiMyself->hab, &qmsg);

                    // WM_QUIT came in: checks below...
                } // end if (pXCenterData->pfnwpFrameOrig)
            }
            CATCH(excpt1) {} END_CATCH();

            // We can get here for several reasons.
            // 1)   The user used the tasklist to kill the XCenter.
            //      Apparently, the task list posts WM_QUIT directly...
            //      so at this point, the XCenter might not have been
            //      destroyed.
            // 2)   By contrast, our WM_DESTROY in fnwpFrame explicitly
            //      posts WM_QUIT too. At that point, the XCenter is
            //      already destroyed.
            // 3)   The XCenter crashed. At that point, the window still
            //      exists.

            // So check...
            if (_pvOpenView)
            {
                // not zeroed yet (zeroed by ClientDestroy):
                // this means the window still exists... clean up
                TRY_QUIET(excpt1)
                {
                    // update desktop workarea
                    UpdateDesktopWorkarea(pXCenterData,
                                          TRUE);            // force remove

                    WinDestroyWindow(pGlobals->hwndFrame);
                    // ClientDestroy sets _pvOpenView to NULL
                }
                CATCH(excpt1) {} END_CATCH();
            }
        } // end if (fCreated)

        // _tid = NULLHANDLE;          // wpClose is waiting on this!!
                // removed, thread functions handle this now

    } // if (pXCenterData)

    /* #ifdef __DEBUG__
        DosBeep(1500, 30);
    #endif */

    // exit! end of thread!
}

/*
 *@@ ctrpCreateXCenterView:
 *      creates a new XCenter view on the screen.
 *      This is the implementation for XCenter::wpOpen
 *      and most probably runs on thread 1.
 *
 *      To be precise, this starts a new thread for the
 *      XCenter view, since we want to allow the user to
 *      specify the XCenter's priority. The new thread is
 *      in ctrp_fntXCenter.
 *
 *      This creates an event semaphore which gets posted
 *      by ctrp_fntXCenter when the XCenter view has been
 *      created on the new thread so that we can return
 *      the HWND of the XCenter frame here.
 *
 *@@changed V0.9.12 (2001-05-20) [umoeller]: added _tid check to fix duplicate open
 */

HWND ctrpCreateXCenterView(XCenter *somSelf,
                           HAB hab,             // in: caller's anchor block
                           ULONG ulView,        // in: view (from wpOpen)
                           PVOID *ppvOpenView)  // out: ptr to PXCENTERWINDATA
{
    XCenterData *somThis = XCenterGetData(somSelf);
    HWND hwndFrame = NULLHANDLE;

    if (!_tidRunning)          // XCenter thread not currently running?
                        // V0.9.12 (2001-05-20) [umoeller]
    {
        TRY_LOUD(excpt1)
        {
            static BOOL     s_fXCenterClassRegistered = FALSE;

            if (!s_fXCenterClassRegistered)
            {
                // get window proc for WC_FRAME
                if (   (WinRegisterClass(hab,
                                         WC_XCENTER_CLIENT,
                                         fnwpXCenterMainClient,
                                         CS_PARENTCLIP | CS_CLIPCHILDREN | CS_SIZEREDRAW | CS_SYNCPAINT,
                                         sizeof(PXCENTERWINDATA))) // additional bytes to reserve
                    && (RegisterBuiltInWidgets(hab))
                   )
                {
                    s_fXCenterClassRegistered = TRUE;
                }
                else
                    cmnLog(__FILE__, __LINE__, __FUNCTION__,
                           "Error registering PM window classes.");
            }

            if (s_fXCenterClassRegistered)
            {
                PXCENTERWINDATA pXCenterData =
                    (PXCENTERWINDATA)_wpAllocMem(somSelf,
                                                 sizeof(XCENTERWINDATA),
                                                 NULL);
                if (pXCenterData)
                {
                    memset(pXCenterData, 0, sizeof(*pXCenterData));

                    pXCenterData->cbSize = sizeof(*pXCenterData);

                    pXCenterData->somSelf = somSelf;
                    pXCenterData->ViewItem.view = ulView;

                    DosCreateEventSem(NULL,     // unnamed
                                      &pXCenterData->hevRunning,
                                      0,        // unshared
                                      FALSE);   // not posted
                    if (thrCreate(NULL,
                                  ctrp_fntXCenter,
                                  &_tidRunning,     // V0.9.12 (2001-05-20) [umoeller]
                                  ENTITY_XCENTER,
                                  THRF_TRANSIENT | THRF_PMMSGQUEUE | THRF_WAIT,
                                  (ULONG)pXCenterData))
                    {
                        // thread created:
                        // wait until it's created the XCenter frame
                        WinWaitEventSem(pXCenterData->hevRunning,
                                        10*1000);
                        // return the frame HWND from this (which will
                        // return it from wpOpen)
                        hwndFrame = pXCenterData->Globals.hwndFrame;
                        if (hwndFrame)
                           *ppvOpenView = pXCenterData;
                    }

                    DosCloseEventSem(pXCenterData->hevRunning);
                }
            } // end if (s_fXCenterClassRegistered)
        }
        CATCH(excpt1) {} END_CATCH();
    } // end if (!_tid)

    return (hwndFrame);
}


