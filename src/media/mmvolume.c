
/*
 *@@sourcefile mmvolume.c:
 *      implementation for the XMMVolume control.
 *
 *      Note: Those G_mmio* and G_mci* identifiers are global
 *      variables containing MMPM/2 API entries. Those are
 *      resolved by xmmInit (mmthread.c) and must only be used
 *      after checking xmmQueryStatus.
 *
 *@@added V0.9.6 (2000-11-09) [umoeller]
 *@@header "media\media.h"
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
 *  7)  headers in filesys\ (as needed)
 *  8)  #pragma hdrstop and then more SOM headers which crash with precompiled headers
 */

#define INCL_DOSEXCEPTIONS
#define INCL_DOSPROCESS

#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WINCIRCULARSLIDER

#define INCL_GPI                // required for INCL_MMIO_CODEC
#define INCL_GPIBITMAPS         // required for INCL_MMIO_CODEC
#include <os2.h>

// multimedia includes
#define INCL_MCIOS2
#define INCL_MMIOOS2
#define INCL_MMIO_CODEC
#include <os2me.h>

// C library headers
#include <stdio.h>              // needed for except.h
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h
#include <io.h>                 // access etc.

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\except.h"             // exception handling
#include "helpers\winh.h"               // PM helper routines

// SOM headers which don't crash with prec. header files
#include "xmmvolume.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file

#include "media\media.h"                // XWorkplace multimedia support

/* ******************************************************************
 *
 *   Private definitions
 *
 ********************************************************************/

typedef struct _VOLUMEWINDATA
{
    XMMVolume           *somSelf;
    PFNWP               pfnwpFrameOriginal;
    USEITEM             UseItem;            // use item; immediately followed by view item
    VIEWITEM            ViewItem;           // view item
} VOLUMEWINDATA, *PVOLUMEWINDATA;

MRESULT EXPENTRY fnwpSubclVolumeFrame(HWND hwndFrame, ULONG msg, MPARAM mp1, MPARAM mp2);

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

/* ******************************************************************
 *
 *   Volume Control Windows
 *
 ********************************************************************/

/*
 *@@ xmmCreateVolumeView:
 *      this gets called from XMMVolume::wpOpen to
 *      create a new "volume" view. The parameters
 *      are just passed on from wpOpen.
 *
 *      The volume window is a regular standard PM
 *      frame with a circular slider inside.
 */

HWND xmmCreateVolumeView(WPObject *somSelf,
                         HWND hwndCnr,
                         ULONG ulView)
{
    HWND            hwndFrame = 0;

    TRY_LOUD(excpt1)
    {
        HAB         hab = WinQueryAnchorBlock(HWND_DESKTOP);
        HWND        hwndClient = NULLHANDLE;
        ULONG       flCreate;                      // Window creation flags
        SWP         swpFrame;

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

        hwndFrame = winhCreateStdWindow(HWND_DESKTOP,           // frame parent
                                        &swpFrame,
                                        flCreate,
                                        WS_ANIMATE,
                                        _wpQueryTitle(somSelf), // title bar
                                        0,                      // res IDs
                                        WC_CIRCULARSLIDER,      // client class: circular slider
                                        WS_VISIBLE              // slider wnd style:
                                            | CSS_CIRCULARVALUE     // draw knob instead of triangle
                                            | CSS_NOTEXT            // no title text under dial
                                            // | CSS_POINTSELECT       // click-to-value
                                            ,
                                        1000,                   // ID
                                        NULL,
                                        &hwndClient);

        if ((hwndFrame) && (hwndClient))
        {
            // frame and client successfully created:

            // allocate window data (use item and such)
            PVOLUMEWINDATA  pWinData = (PVOLUMEWINDATA)_wpAllocMem(somSelf,
                                                                   sizeof(VOLUMEWINDATA),
                                                                   0);

            if (pWinData)
            {

                memset(pWinData, 0, sizeof(*pWinData));

                pWinData->somSelf = somSelf;

                WinSetWindowPtr(hwndFrame, QWL_USER, pWinData);

                // set up slider more
                WinSendMsg(hwndClient,
                           CSM_SETRANGE,
                           (MPARAM)0,
                           (MPARAM)100);
                WinSendMsg(hwndClient,
                           CSM_SETINCREMENT,
                           (MPARAM)1,           // scoll increment
                           (MPARAM)10);         // tick marks increment


                // subclass frame
                pWinData->pfnwpFrameOriginal = WinSubclassWindow(hwndFrame,
                                                                 fnwpSubclVolumeFrame);
                if (pWinData->pfnwpFrameOriginal)
                {
                    // PNLSSTRINGS     pNLSStrings = cmnQueryNLSStrings();
                    // view title: we remove "~" later
                    /* PSZ             pszViewTitle = strdup(cmnGetString(ID_XSSI_VOLUMEVIEW)),
                                    p = NULL; */

                    // now position the frame and the client:
                    // 1) frame
                    if (!winhRestoreWindowPos(hwndFrame,
                                              HINI_USER,
                                              INIAPP_XWORKPLACE,
                                              INIKEY_WNDPOSXMMVOLUME,
                                              SWP_MOVE | SWP_SIZE | SWP_ACTIVATE))
                        // INI data not found:
                        WinSetWindowPos(hwndFrame,
                                        HWND_TOP,
                                        100, 100,
                                        500, 500,
                                        SWP_MOVE | SWP_SIZE | SWP_ACTIVATE);

                    // finally, show window
                    WinShowWindow(hwndFrame, TRUE);

                    // add the use list item to the object's use list
                    cmnRegisterView(somSelf,
                                    &pWinData->UseItem,
                                    ulView,
                                    hwndFrame,
                                    cmnGetString(ID_XSSI_VOLUMEVIEW));
                }
            }
        }
    }
    CATCH(excpt1)
    {
        hwndFrame = NULLHANDLE;
    } END_CATCH();

    return (hwndFrame);
}

/*
 *@@ fnwpSubclVolumeFrame:
 *
 */

MRESULT EXPENTRY fnwpSubclVolumeFrame(HWND hwndFrame, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    PVOLUMEWINDATA pWinData = (PVOLUMEWINDATA)WinQueryWindowPtr(hwndFrame, QWL_USER);

    MRESULT mrc = 0;

    switch (msg)
    {
        case WM_SYSCOMMAND:
            switch ((ULONG)mp1)
            {
                case SC_CLOSE:
                {
                    // on SC_CLOSE, frames post WM_CLOSE to the client, which
                    // is not understood by the slider here...

                    HWND hwndSlider = WinWindowFromID(hwndFrame, FID_CLIENT);
                    // save window position
                    winhSaveWindowPos(hwndFrame,
                                      HINI_USER,
                                      INIAPP_XWORKPLACE,
                                      INIKEY_WNDPOSXMMVOLUME);
                    WinDestroyWindow(hwndSlider);
                    WinDestroyWindow(hwndFrame);
                break; }

                default:
                    mrc = pWinData->pfnwpFrameOriginal(hwndFrame, msg, mp1, mp2);
            }
        break;

        case WM_DESTROY:
            // remove this window from the object's use list
            _wpDeleteFromObjUseList(pWinData->somSelf,
                                    &pWinData->UseItem);
            _wpFreeMem(pWinData->somSelf, (PBYTE)pWinData);
            WinSetWindowPtr(hwndFrame, QWL_USER, 0);
            mrc = pWinData->pfnwpFrameOriginal(hwndFrame, msg, mp1, mp2);
        break;

        default:
            mrc = pWinData->pfnwpFrameOriginal(hwndFrame, msg, mp1, mp2);
    }

    return (mrc);
}



