
/*
 *@@sourcefile hk_scroll.c:
 *
 *@@added V0.9.12 (2001-05-27) [umoeller]
 *@@header "hook\hook_private.h"
 */

/*
 *      Copyright (C) 1999-2001 Ulrich M”ller.
 *      Copyright (C) 1993-1999 Roman Stangl.
 *      Copyright (C) 1995-1999 Carlos Ugarte.
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, in version 2 as it comes in the COPYING
 *      file of the XWorkplace main distribution.
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 */

#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WINMESSAGEMGR
#define INCL_WININPUT
#define INCL_WINPOINTERS
#define INCL_WINMENUS
#define INCL_WINSCROLLBARS
#define INCL_WINSYS
#define INCL_WINTIMER
#define INCL_WINHOOKS
#define INCL_DOSSEMAPHORES
#define INCL_DOSERRORS
#define INCL_DOSMODULEMGR
#include <os2.h>

#include <stdio.h>

// #include "setup.h"

#include "helpers\undoc.h"

#include "hook\xwphook.h"
#include "hook\hook_private.h"          // private hook and daemon definitions

// PMPRINTF in hooks is a tricky issue;
// avoid this unless this is really needed.
// If enabled, NEVER give the PMPRINTF window
// the focus, or your system will hang solidly...

#define DONTDEBUGATALL
#define DONT_REPLACE_MALLOC         // in case mem debug is enabled
#include "setup.h"

/******************************************************************
 *
 *  Input hook -- Scroll processing
 *
 ******************************************************************/

/*
 *@@ GetScrollBar:
 *      returns the specified scroll bar of hwndOwner.
 *      Returns NULLHANDLE if it doesn't exist or is
 *      disabled.
 *
 *@@added V0.9.1 (99-12-03) [umoeller]
 *@@changed V0.9.1 (2000-02-13) [umoeller]: fixed disabled scrollbars bug
 *@@changed V0.9.3 (2000-04-30) [umoeller]: added more error checking
 *@@changed V0.9.3 (2000-04-30) [umoeller]: fixed invisible scrollbars bug
 *@@changed V0.9.9 (2001-03-20) [lafaix]: fixed empty (size=0x0) scrollbar bug
 */

HWND GetScrollBar(HWND hwndOwner,
                  BOOL fHorizontal) // in: if TRUE, query horizontal;
                                    // if FALSE; query vertical
{
    HAB     hab = WinQueryAnchorBlock(hwndOwner);
    HWND    hwndReturn = NULLHANDLE;
    HENUM   henum; // enumeration handle for scroll bar seek
    HWND    hwndFound; // handle of found window
    CHAR    szWinClass[3]; // buffer for window class name
    ULONG   ulWinStyle; // style of the found scroll bar

    // begin window enumeration
    if (henum = WinBeginEnumWindows(hwndOwner))
    {
        while (hwndFound = WinGetNextWindow(henum))
        {
            if (!WinIsWindow(hab,
                             hwndFound))
                // error:
                break;
            else
            {
                SWP swp;

                // query class name of found window
                if (    (WinQueryClassName(hwndFound, 3, szWinClass))
                        // is it a scroll bar window?
                     && (!strcmp(szWinClass, "#8"))
                        // is it a non-empty scroll bar?
                     && (WinQueryWindowPos(hwndFound, &swp))
                     && (swp.cx)
                     && (swp.cy)
                   )
                {
                    // query style bits of this scroll bar window
                    ulWinStyle = WinQueryWindowULong(hwndFound, QWL_STYLE);

                    // is scroll bar enabled and visible?
                    if ((ulWinStyle & (WS_DISABLED | WS_VISIBLE)) == WS_VISIBLE)
                    {
                        // return window handle if it matches fHorizontal
                        if (fHorizontal)
                        {
                            // query horizonal mode:
                            if ((ulWinStyle & SBS_VERT) == 0)
                                // we must check it this way
                                // because SBS_VERT is 1 and SBS_HORZ is 0
                            {
                                hwndReturn = hwndFound;
                                break; // while
                            }
                        }
                        else
                            if (ulWinStyle & SBS_VERT)
                            {
                                hwndReturn = hwndFound;
                                break; // while
                            }
                    }
                }
            }
        } // end while ((hwndFound = WinGetNextWindow(henum)) != NULLHANDLE)

        // finish window enumeration
        WinEndEnumWindows(henum);
    }

    return (hwndReturn);
}

/*
 *@@ StopMB3Scrolling:
 *      this stops MB3 scrolling and unsets all global
 *      data which has been initialized for scrolling.
 *
 *      This gets called from WM_BUTTON3UP with
 *      (fSuccessPostMsgs == TRUE) so that we can
 *      post messages to the scroll bar owner that we're
 *      done.
 *
 *      Also this gets called with (fSuccessPostMsgs == FALSE)
 *      if any errors occur during processing.
 *
 *@@added V0.9.3 (2000-04-30) [umoeller]
 *@@changed V0.9.9 (2001-03-18) [lafaix]: pointer support added
 */

VOID StopMB3Scrolling(BOOL fSuccessPostMsgs)
{
    // set scrolling mode to off
    G_HookData.hwndCurrentlyScrolling = NULLHANDLE;

    // reset pointers
    WinSendMsg(G_HookData.hwndDaemonObject,
               XDM_ENDSCROLL,
               NULL,
               NULL);

    // re-enable mouse pointer hidding if applicable
    G_HookData.HookConfig.fAutoHideMouse = G_HookData.fOldAutoHideMouse;

    // release capture (set in WM_BUTTON3DOWN)
    WinSetCapture(HWND_DESKTOP, NULLHANDLE);

    if (fSuccessPostMsgs)
    {
        if (G_HookData.SDYVert.fPostSBEndScroll)
        {
            // we did move the scroller previously:
            // send end scroll message
            WinPostMsg(G_HookData.SDYVert.hwndScrollLastOwner,
                       WM_VSCROLL,
                       MPFROMSHORT(WinQueryWindowUShort(G_HookData.SDYVert.hwndScrollBar,
                                                        QWS_ID)),
                       MPFROM2SHORT(G_HookData.SDYVert.sCurrentThumbPosUnits,
                                    SB_SLIDERPOSITION)); // SB_ENDSCROLL));
        }

        if (G_HookData.SDXHorz.fPostSBEndScroll)
        {
            // we did move the scroller previously:
            // send end scroll message
            WinPostMsg(G_HookData.SDXHorz.hwndScrollLastOwner,
                       WM_HSCROLL,
                       MPFROMSHORT(WinQueryWindowUShort(G_HookData.SDXHorz.hwndScrollBar,
                                                        QWS_ID)),
                       MPFROM2SHORT(G_HookData.SDXHorz.sCurrentThumbPosUnits,
                                    SB_SLIDERPOSITION));
        }
    }
}

/*
 *@@ WMMouseMove_MB3ScrollLineWise:
 *      this gets called from WMMouseMove_MB3OneScrollbar
 *      if "amplified" mb3-scroll is on. This is what
 *      WarpEnhancer did with MB3 scrolls.
 *
 *      This can get called twice for every WM_MOUSEMOVE,
 *      once for the vertical, once for the horizontal
 *      scroll bar of a window.
 *
 *      Returns FALSE on errors.
 *
 *      Based on code from WarpEnhancer, (C) Achim Hasenmller.
 *
 *@@added V0.9.1 (99-12-03) [umoeller]
 *@@changed V0.9.3 (2000-04-30) [umoeller]: added tons of error checking
 */

BOOL WMMouseMove_MB3ScrollLineWise(PSCROLLDATA pScrollData,   // in: scroll data structure in HOOKDATA,
                                                              // either for vertical or horizontal scroll bar
                                   LONG lDelta,               // in: X or Y delta that mouse has moved since MB3 was
                                                              // _initially_ depressed
                                   BOOL fHorizontal)          // in: TRUE: process horizontal, otherwise vertical
{
    BOOL    brc = FALSE;

    USHORT  usScrollCode;
    ULONG   ulMsg;

    // save window ID of scroll bar control
    USHORT usScrollBarID = WinQueryWindowUShort(pScrollData->hwndScrollBar,
                                                QWS_ID);

    if (!fHorizontal)
    {
        if (lDelta > 0)
            usScrollCode = SB_LINEDOWN;
        else
            usScrollCode = SB_LINEUP;

        ulMsg = WM_VSCROLL;
    }
    else
    {
        if (lDelta > 0)
            usScrollCode = SB_LINERIGHT;
        else
            usScrollCode = SB_LINELEFT;

        ulMsg = WM_HSCROLL;
    }

    // post up or down scroll message
    if (WinPostMsg(pScrollData->hwndScrollLastOwner,
                   ulMsg,
                   MPFROMSHORT(usScrollBarID),
                   MPFROM2SHORT(1,
                                usScrollCode)))
        // post end scroll message
        if (WinPostMsg(pScrollData->hwndScrollLastOwner,
                       ulMsg,
                       MPFROMSHORT(usScrollBarID),
                       MPFROM2SHORT(1,
                                    SB_ENDSCROLL)))
            brc = TRUE;

    return (brc);
}

/*
 *@@ CalcScrollBarSize:
 *      this calculates the size of the scroll bar range,
 *      that is, the size of the dark background part
 *      inside the scroll bar (besides the thumb), which
 *      is returned in window coordinates.
 *
 *      Gets called with every WMMouseMove_MB3ScrollAmplified.
 *
 *      Returns 0 on errors.
 *
 *@@added V0.9.2 (2000-02-26) [umoeller]
 *@@changed V0.9.2 (2000-03-23) [umoeller]: removed lScrollBarSize from SCROLLDATA; this is returned now
 *@@changed V0.9.3 (2000-04-30) [umoeller]: added more error checking
 */

LONG CalcScrollBarSize(PSCROLLDATA pScrollData,  // in: scroll data structure in HOOKDATA,
                                                 // either for vertical or horizontal scroll bar
                       BOOL fHorizontal)         // in: TRUE: process horizontal, otherwise vertical
{
    LONG lrc = 0;

    // for "amplified" mode, we also need the size
    // of the scroll bar and the thumb size to be
    // able to correlate the mouse movement to the
    // scroller position
    SWP     swpScrollBar;
    WNDPARAMS wp;

    // get size of scroll bar
    if (WinQueryWindowPos(pScrollData->hwndScrollBar, &swpScrollBar))
    {
        // subtract the size of the scroll bar buttons
        if (fHorizontal)
        {
            lrc = swpScrollBar.cx
                  - 2 * WinQuerySysValue(HWND_DESKTOP,
                                         SV_CXHSCROLLARROW);
        }
        else
        {
            lrc = swpScrollBar.cy
                  - 2 * WinQuerySysValue(HWND_DESKTOP,
                                         SV_CYVSCROLLARROW);
        }

        // To make the MB3 scrolling more similar to regular scroll
        // bar dragging, we must take the size of the scroll bar
        // thumb into account by subtracting that size from the scroll
        // bar size; otherwise the user would have much more
        // mouse mileage with MB3 compared to the regular
        // scroll bar.

        // Unfortunately, there's no "SBM_QUERYTHUMBSIZE" msg,
        // so we need to be a bit more tricky and extract this
        // from the scroll bar's control data.

        memset(&wp, 0, sizeof(wp));

        // get size of scroll bar control data
        wp.fsStatus = WPM_CBCTLDATA;
        if (WinSendMsg(pScrollData->hwndScrollBar,
                       WM_QUERYWINDOWPARAMS,
                       (MPARAM)&wp,
                       0))
        {
            // success:
            _Pmpf(("    wp.cbCtlData: %d, sizeof SBCDATA: %d",
                        wp.cbCtlData, sizeof(SBCDATA)));
            if (wp.cbCtlData == sizeof(SBCDATA))
            {
                // allocate memory
                SBCDATA sbcd;
                wp.pCtlData = &sbcd;
                // now get control data, finally
                wp.fsStatus = WPM_CTLDATA;
                if (WinSendMsg(pScrollData->hwndScrollBar,
                               WM_QUERYWINDOWPARAMS,
                               (MPARAM)&wp,
                               0))
                {
                    // success:
                    // cVisible specifies the thumb size in
                    // units of cTotal; we now correlate
                    // the window dimensions to the dark part
                    // of the scroll bar (which is cTotal - cVisible)

                    if (    (sbcd.cTotal) // avoid division by zero
                         && (sbcd.cTotal > sbcd.cVisible)
                       )
                        lrc =   lrc
                              * (sbcd.cTotal - sbcd.cVisible)
                              / sbcd.cTotal;
                }
            }
        }
    }

    return (lrc);
}

/*
 *@@ WMMouseMove_MB3ScrollAmplified:
 *      this gets called from WMMouseMove_MB3OneScrollbar
 *      if "amplified" mb3-scroll is on.
 *      This is new compared to WarpEnhancer.
 *
 *      This can get called twice for every WM_MOUSEMOVE,
 *      once for the vertical, once for the horizontal
 *      scroll bar of a window.
 *
 *      Returns FALSE on errors.
 *
 *@@added V0.9.1 (99-12-03) [umoeller]
 *@@changed V0.9.2 (2000-02-26) [umoeller]: changed ugly pointer arithmetic
 *@@changed V0.9.2 (2000-03-23) [umoeller]: now recalculating scroll bar size on every WM_MOUSEMOVE
 *@@changed V0.9.3 (2000-04-30) [umoeller]: added tons of error checking
 *@@changed V0.9.9 (2001-03-19) [lafaix]: now returns TRUE if current pos = initial pos
 *@@changed V0.9.9 (2001-03-20) [lafaix]: changed ulAmpPercent to lAmpPercent and 1000 to 1000L
 */

BOOL WMMouseMove_MB3ScrollAmplified(PSCROLLDATA pScrollData,  // in: scroll data structure in HOOKDATA,
                                                              // either for vertical or horizontal scroll bar
                                    LONG lDeltaPels,          // in: X or Y delta that mouse has moved since MB3 was
                                                              // _initially_ depressed
                                    BOOL fHorizontal)         // in: TRUE: process horizontal, otherwise vertical
{
    BOOL    brc = FALSE;

    // get scroll bar size
    LONG    lScrollBarSizePels;
    if (lScrollBarSizePels = CalcScrollBarSize(pScrollData,
                                               fHorizontal))
    {
        // get scroll bar range
        MRESULT mrThumbRange;
        if (mrThumbRange = WinSendMsg(pScrollData->hwndScrollBar,
                                      SBM_QUERYRANGE,
                                      0,
                                      0))
        {
            SHORT   sThumbLoLimitUnits = (LONG)SHORT1FROMMR(mrThumbRange),
                    sThumbHiLimitUnits = (LONG)SHORT2FROMMR(mrThumbRange);

            if (sThumbHiLimitUnits > sThumbLoLimitUnits)
            {
                // here come a number of pointers to data we'll
                // need, depending on we're processing the vertical
                // or horizontal scroll bar
                ULONG   ulMsg;
                LONG    lPerMilleMoved;

                if (!fHorizontal)
                    // vertical mode:
                    ulMsg = WM_VSCROLL;
                else
                    // horizontal mode:
                    ulMsg = WM_HSCROLL;

                // We need to calculate a new _absolute_ scroller
                // position based on the _relative_ delta we got
                // as a parameter. This is a bit tricky:

                // 1) Calculate per-mille that mouse has moved
                //    since MB3 was initially depressed, relative
                //    to the window (scroll bar owner) height.
                //    This will be in the range of -1000 to +1000.
                lPerMilleMoved =   (lDeltaPels * 1000L)
                                 / lScrollBarSizePels;
                        // this correlates the y movement to the
                        // remaining window height;
                        // this is now in the range of -1000 thru +1000

                _Pmpf(("  lPerMilleMoved: %d", lPerMilleMoved));

                // 2) amplification desired? (0 is default for 100%)
                if (G_HookData.HookConfig.sAmplification)
                {
                    // yes:
                    LONG lAmpPercent = 100 + (G_HookData.HookConfig.sAmplification * 10);
                        // so we get:
                        //      0       -->  100%
                        //      2       -->  120%
                        //     10       -->  200%
                        //     -2       -->  80%
                        //     -9       -->  10%
                    lPerMilleMoved = lPerMilleMoved * lAmpPercent / 100L;
                }

                if (lPerMilleMoved)
                {
                    // 3) Correlate this to scroll bar units;
                    //    this is still a delta, but now in scroll
                    //    bar units.
                    LONG lSliderRange = ((LONG)sThumbHiLimitUnits
                                        - (LONG)sThumbLoLimitUnits);
                    SHORT sSliderMovedUnits = (SHORT)(
                                                      lSliderRange
                                                      * lPerMilleMoved
                                                      / 1000L
                                                     );

                    SHORT   sNewThumbPosUnits = 0;

                    // _Pmpf(("  lSliderRange: %d", lSliderRange));
                    // _Pmpf(("  lSliderOfs: %d", lSliderMoved));

                    // 4) Calculate new absolute scroll bar position,
                    //    from on what we stored when MB3 was initially
                    //    depressed.
                    sNewThumbPosUnits = pScrollData->sMB3InitialThumbPosUnits
                                        + sSliderMovedUnits;

                    _Pmpf(("  New sThumbPosUnits: %d", sNewThumbPosUnits));

                    // check against scroll bar limits:
                    if (sNewThumbPosUnits < sThumbLoLimitUnits)
                        sNewThumbPosUnits = sThumbLoLimitUnits;
                    if (sNewThumbPosUnits > sThumbHiLimitUnits)
                        sNewThumbPosUnits = sThumbHiLimitUnits;

                    // thumb position changed?
                    if (sNewThumbPosUnits == pScrollData->sCurrentThumbPosUnits)
                                                    // as calculated last time
                                                    // zero on first call
                        // no: do nothing, but report success
                        brc = TRUE;
                    else
                    {
                        // yes:
                        // now simulate the message flow that
                        // the scroll bar normally produces
                        // _while_ the scroll bar thumb is being
                        // dragged:

                        // save window ID of scroll bar control
                        USHORT usScrollBarID = WinQueryWindowUShort(pScrollData->hwndScrollBar,
                                                                    QWS_ID);

                        // a) adjust thumb position in the scroll bar
                        if (WinSendMsg(pScrollData->hwndScrollBar,
                                       SBM_SETPOS,
                                       MPFROMSHORT(sNewThumbPosUnits),
                                       0))
                        {
                            // b) notify scroll bar owner of the change
                            // (normally posted by the scroll bar);
                            // this will scroll the window contents
                            // depending on the owner's implementation
                            if (WinPostMsg(pScrollData->hwndScrollLastOwner,
                                           ulMsg,                   // WM_xSCROLL
                                           MPFROMSHORT(usScrollBarID),
                                           MPFROM2SHORT(sNewThumbPosUnits,
                                                        SB_SLIDERTRACK)))
                            {
                                // set flag to provoke a SB_ENDSCROLL
                                // in WM_BUTTON3UP later (hookInputHook)
                                pScrollData->fPostSBEndScroll = TRUE;

                                // store this thumb position for next time
                                pScrollData->sCurrentThumbPosUnits = sNewThumbPosUnits;

                                // hoo-yah!!!
                                brc = TRUE;
                            }
                        } // end if (WinSendMsg(pScrollData->hwndScrollBar,
                    } // end if (sNewThumbPosUnits != pScrollData->sCurrentThumbPosUnits)
                } // end if (lPerMilleMoved)
                else
                    // it's OK if the mouse hasn't moved V0.9.9 (2001-03-19) [lafaix]
                    brc = TRUE;
            } // end if (sThumbHiLimitUnits > sThumbLoLimitUnits)
        } // end if (mrThumbRange)
    } // end if (lScrollBarSizePels)

    return (brc);
}

/*
 *@@ WMMouseMove_MB3OneScrollbar:
 *      this gets called twice from WMMouseMove_MB3Scroll
 *      only when MB3 is currently depressed to do the
 *      scroll bar and scroll bar owner processing and
 *      messaging every time WM_MOUSEMOVE comes in.
 *
 *      Since we pretty much do the same thing twice,
 *      once for the vertical, once for the horizontal
 *      scroll bar, we can put this into a common function
 *      to reduce code size and cache load.
 *
 *      Depending on the configuration and mouse movement,
 *      this calls either WMMouseMove_MB3ScrollLineWise or
 *      WMMouseMove_MB3ScrollAmplified.
 *
 *      Returns TRUE if we were successful and the msg is
 *      to be swallowed. Otherwise FALSE is returned.
 *
 *      Initially based on code from WarpEnhancer, (C) Achim Hasenmller,
 *      but largely rewritten.
 *
 *@@added V0.9.2 (2000-02-26) [umoeller]
 *@@changed V0.9.2 (2000-03-23) [umoeller]: now recalculating scroll bar size on every WM_MOUSEMOVE
 *@@changed V0.9.3 (2000-04-30) [umoeller]: added tons of error checking
 *@@changed V0.9.9 (2001-03-21) [lafaix]: do not move scrollbars if AutoScroll is on
 */

BOOL WMMouseMove_MB3OneScrollbar(HWND hwnd,                  // in: window with WM_MOUSEMOVE
                                 PSCROLLDATA pScrollData,
                                 SHORT sCurrentScreenMousePos,  // in: current mouse X or Y
                                                             // (in screen coordinates)
                                 BOOL fHorizontal)           // in: TRUE: process horizontal, otherwise vertical
{
    BOOL    brc = FALSE;        // swallow?

    // define initial coordinates if not set yet;
    // this is only -1 if this is the first WM_MOUSEMOVE
    // after MB3 was just depressed
    if (pScrollData->sMB3InitialScreenMousePos == -1)
    {
        /*
         * first call after MB3 was depressed
         *
         */

        // 1) query window handle of vertical scroll bar
        if (!(pScrollData->hwndScrollBar = GetScrollBar(hwnd,
                                                        fHorizontal)))
            // if not found, then try if parent has a scroll bar
            pScrollData->hwndScrollBar = GetScrollBar(WinQueryWindow(hwnd,
                                                                     QW_PARENT),
                                                      fHorizontal);

        if (pScrollData->hwndScrollBar)
        {
            // found a scroll bar:

            // save initial mouse position
            pScrollData->sMB3InitialScreenMousePos = sCurrentScreenMousePos;
            // save initial scroller position
            pScrollData->sMB3InitialThumbPosUnits
                = (SHORT)WinSendMsg(pScrollData->hwndScrollBar,
                                    SBM_QUERYPOS,
                                    0,
                                    0);
            // cache that
            pScrollData->sCurrentThumbPosUnits = pScrollData->sMB3InitialThumbPosUnits;

            brc = TRUE;
        }
        // else: error
    } // if (pScrollData->sMB3InitialMousePos == -1)
    else
        if (    (G_HookData.bAutoScroll)
             && (pScrollData->hwndScrollBar)
           )
        {
            /*
             * subsequent calls, AutoScroll enabled
             *
             */

            if (pScrollData->hwndScrollLastOwner = WinQueryWindow(pScrollData->hwndScrollBar,
                                                                  QW_OWNER))
                brc = TRUE;
        }

    if (pScrollData->hwndScrollBar && (!G_HookData.bAutoScroll))
    {
        /*
         * subsequent calls, AutoScroll non active
         *
         */

        if (!WinIsWindow(WinQueryAnchorBlock(hwnd),
                         pScrollData->hwndScrollBar))
            brc = FALSE;        // error
        else
        {
            // check if the scroll bar (still) has an owner;
            // otherwise all this doesn't make sense
            if (pScrollData->hwndScrollLastOwner = WinQueryWindow(pScrollData->hwndScrollBar,
                                                                  QW_OWNER))
            {
                // calculate difference between initial
                // mouse pos (when MB3 was depressed) and
                // current mouse pos to get the _absolute_
                // delta since MB3 was depressed
                LONG    lDeltaPels = (LONG)pScrollData->sMB3InitialScreenMousePos
                                     - (LONG)sCurrentScreenMousePos;

                // now check if we need to change the sign of
                // the delta;
                // for a vertical scroll bar,
                // a value of "0" means topmost position
                // (as opposed to screen coordinates...),
                // while for a horizontal scroll bar,
                // "0" means leftmost position (as with
                // screen coordinates...);
                // but when "reverse scrolling" is enabled,
                // we must do this just the other way round
                if (   (    (fHorizontal)
                         && (!G_HookData.HookConfig.fMB3ScrollReverse)
                       )
                        // horizontal scroll bar and _not_ reverse mode:
                    ||
                       (    (!fHorizontal)
                         && (G_HookData.HookConfig.fMB3ScrollReverse)
                       )
                        // vertical scroll bar _and_ reverse mode:
                   )
                    // change sign for all subsequent processing
                    lDeltaPels = -lDeltaPels;

                if (G_HookData.HookConfig.usScrollMode == SM_AMPLIFIED)
                {
                    // amplified mode:
                    if (    (pScrollData->fPostSBEndScroll)
                                // not first call
                         || (abs(lDeltaPels) >= (G_HookData.HookConfig.usMB3ScrollMin + 1))
                                // or movement is large enough:
                       )
                        brc = WMMouseMove_MB3ScrollAmplified(pScrollData,
                                                             lDeltaPels,
                                                             fHorizontal);
                    else
                        // swallow anyway
                        brc = TRUE;
                }
                else
                    // line-wise mode:
                    if (abs(lDeltaPels) >= (G_HookData.HookConfig.usMB3ScrollMin + 1))
                    {
                        // movement is large enough:
                        brc = WMMouseMove_MB3ScrollLineWise(pScrollData,
                                                            lDeltaPels,
                                                            fHorizontal);
                        pScrollData->sMB3InitialScreenMousePos = sCurrentScreenMousePos;
                    }
                    else
                        // swallow anyway
                        brc = TRUE;
            }
        }
    }

    return (brc);
}

/*
 *@@ WMMouseMove_MB3Scroll:
 *      this gets called when hookInputHook intercepts
 *      WM_MOUSEMOVE only if MB3 is currently down to do
 *      the "MB3 scrolling" processing.
 *
 *      This calls WMMouseMove_MB3OneScrollbar twice,
 *      once for the vertical, once for the horizontal
 *      scroll bar.
 *
 *      Returns TRUE if the msg is to be swallowed.
 *
 *@@added V0.9.1 (99-12-03) [umoeller]
 *@@changed V0.9.2 (2000-02-25) [umoeller]: HScroll not working when VScroll disabled; fixed
 *@@changed V0.9.2 (2000-02-25) [umoeller]: extracted WMMouseMove_MB3OneScrollbar for speed
 *@@changed V0.9.3 (2000-04-30) [umoeller]: recalculating more data with every call
 *@@changed V0.9.3 (2000-04-30) [umoeller]: switched processing to screen coords to avoid mouse capturing
 *@@changed V0.9.3 (2000-04-30) [umoeller]: added tons of error checking
 *@@changed V0.9.4 (2000-06-26) [umoeller]: changed error beep to debug mode only
 *@@changed V0.9.9 (2001-03-18) [lafaix]: added pointer change support
 */

BOOL WMMouseMove_MB3Scroll(HWND hwnd)       // in: window with WM_MOUSEMOVE
{
    BOOL    brc = FALSE;        // no swallow msg
    BOOL    bFirst = (G_HookData.SDYVert.sMB3InitialScreenMousePos == -1)
                  && (G_HookData.SDXHorz.sMB3InitialScreenMousePos == -1);

    // process vertical scroll bar
    if (WMMouseMove_MB3OneScrollbar(hwnd,
                                    &G_HookData.SDYVert,
                                    G_ptlMousePosDesktop.y,
                                    FALSE))      // vertical
        // msg to be swallowed:
        brc = TRUE;

    // process horizontal scroll bar
    if (WMMouseMove_MB3OneScrollbar(hwnd,
                                    &G_HookData.SDXHorz,
                                    G_ptlMousePosDesktop.x,
                                    TRUE))       // horizontal
        // msg to be swallowed:
        brc = TRUE;

    if (!brc)
    {
        #ifdef __DEBUG__
            DosBeep(100, 100);
        #endif
        // any error found:
        StopMB3Scrolling(FALSE);   // don't post messages
    }
    else
    {
        if (bFirst)
            // update the pointer now to indicate scrolling in action
            // V0.9.9 (2001-03-18) [lafaix]
            WinSendMsg(G_HookData.hwndDaemonObject,
                       XDM_BEGINSCROLL,
                       MPFROM2SHORT(G_HookData.SDXHorz.sMB3InitialScreenMousePos,
                                    G_HookData.SDYVert.sMB3InitialScreenMousePos),
                       NULL);
        else
            // set adequate pointer
            WinPostMsg(G_HookData.hwndDaemonObject,
                       XDM_SETPOINTER,
                       MPFROM2SHORT(G_ptlMousePosDesktop.x,
                                    G_ptlMousePosDesktop.y),
                       0);
    }

    return (brc);
}


