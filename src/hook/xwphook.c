
/*
 *@@sourcefile xwphook.c:
 *      this has the all the code for XWPHOOK.DLL, which implements
 *      all the XWorkplace PM system hooks, as well as a few exported
 *      interface functions which may only be called by the XWorkplace
 *      daemon process (XWPDAEMN.EXE).
 *
 *      There are two hooks at present, hookInputHook and
 *      hookPreAccelHook. See remarks there.
 *
 *      Most of the hook features started out with code taken
 *      from ProgramCommander/2 and WarpEnhancer and were then
 *      extended.
 *
 *      The hooks have been designed for greatest possible clarity
 *      and lucidity with respect to interfaces and configuration --
 *      as if that was possible with system hooks at all, since they
 *      are messy by nature.
 *
 *      XWorkplace implementation notes:
 *
 *      --  System hooks are running in the context of each PM thread
 *          which is processing a message -- that is, possibly each
 *          thread on the system which has created a message queue.
 *
 *          As a result, great care must be taken when accessing global
 *          data, and one must keep in mind for each function which process
 *          it belongs to, because otherwise access violations will
 *          be inevitable.
 *
 *          It _is_ possible to access static global variables of
 *          this DLL from every function, because the DLL is linked
 *          with the STATIC SHARED flags (see xwphook.def).
 *          This means that the data segment of the DLL becomes
 *          part of each process which is using the DLL
 *          (that is, each PM process on the system). We use this
 *          for the HOOKDATA structure, which is returned to the
 *          daemon from hookInit, which is a static global structure
 *          and includes a HOOKCONFIG structure. As a result, to
 *          configure the basic hook flags, the daemon can simply
 *          modify the fields in the HOOKDATA structure to configure
 *          the hook's behavior.
 *
 *          It is however _not_ possible to use malloc() to allocate
 *          memory and use it across several calls of the hooks, because
 *          that memory will belong to one process only, even if the pointer
 *          is stored in a global DLL variable. The next time the hook gets
 *          called and accesses that memory, some fairly random application
 *          will crash (the one the hook is currently called for), or the
 *          system will hang completely.
 *
 *          For this reason, the hooks use another block of shared memory
 *          internally, which is protected by a mutex semaphore, for storing
 *          the list of object hotkeys (which can be dynamic in size).
 *          This block is (re)allocated in hookSetGlobalHotkeys and
 *          requested in the hook when WM_CHAR comes in.
 *
 *      --  The exported hook* functions may only be used by one
 *          single process. It is not possible for one process to
 *          call hookInit and for another to call another hook*
 *          function, because the shared memory which is allocated
 *          here must be "owned" by a certain process and would not be
 *          properly freed if several processes called the hook
 *          interfaces.
 *
 *          So, per definition, it is only the XWorkplace daemon's
 *          (XWPDAEMN.EXE) responsibility to interface and configure
 *          the hook. Theoretically, we could have done this from
 *          XFLDR.DLL in PMSHELL.EXE also, but using the daemon has a
 *          number of advantages to that, since it can be terminated
 *          and restarted independently of the WPS (see xwpdaemn.c
 *          for details).
 *
 *          When any configuration data changes which is of importance
 *          to the hook, XFLDR.DLL writes this data to OS2.INI and
 *          then posts the daemon a message. The daemon then reads
 *          the new data and notifies the hook thru defined interface
 *          functions. All configuration structures are declared in
 *          xwphook.h.
 */

/*
 *      Copyright (C) 1999 Ulrich Mîller,
 *                         Roman Stangl,
 *                         Achim HasenmÅller.
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, in version 2 as it comes in the COPYING
 *      file of the XFolder main distribution.
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
#include <stdlib.h>
#include <string.h>

#include "helpers\undoc.h"

#include "hook\xwphook.h"

// PMPRINTF in hooks is a tricky issue;
// avoid this unless this is really needed...
#define DONTDEBUGATALL

#include "setup.h"

/*
 * Global variables:
 *      We must initialize all these variables to
 *      something, because if we don't, the compiler
 *      won't put them in the DLL's shared data segment
 *      (see introductory notes).
 */

HOOKDATA    HookData = {0};
    // this includes HOOKCONFIG; its address is
    // returned from hookInit and stored within the
    // daemon to configure the hook

PGLOBALHOTKEY pGlobalHotkeysMain = NULL;
    // pointer to block of shared memory containing
    // the hotkeys; allocated by hookInit, requested
    // by hooks, changed by hookSetGlobalHotkeys

ULONG         cGlobalHotkeys = 0;
    // count of items in that array (_not_ array size!)

HMTX        hmtxGlobalHotkeys = NULLHANDLE;
    // mutex for protecting that array

/*
 * Prototypes:
 *
 */

BOOL EXPENTRY hookInputHook(HAB hab, PQMSG pqmsg, USHORT fs);
BOOL EXPENTRY hookPreAccelHook(HAB hab, PQMSG pqmsg, ULONG option);

// _CRT_init is the C run-time environment initialization function.
// It will return 0 to indicate success and -1 to indicate failure.
int _CRT_init(void);

// _CRT_term is the C run-time environment termination function.
// It only needs to be called when the C run-time functions are statically
// linked, as is the case with XFolder.
void _CRT_term(void);

/******************************************************************
 *                                                                *
 *  Helper functions                                              *
 *                                                                *
 ******************************************************************/

/*
 *@@ _DLL_InitTerm:
 *     this function gets called when the OS/2 DLL loader loads
 *     and frees this DLL. We override this function (which is
 *     normally provided by the runtime library) to intercept
 *     this DLL's module handle.
 *
 *     Since OS/2 calls this function directly, it must have
 *     _System linkage.
 *
 *     Note: You must then link using the /NOE option, because
 *     the VAC++ runtimes also contain a _DLL_Initterm, and the
 *     linker gets in trouble otherwise.
 *     The XWorkplace makefile takes care of this.
 *
 *     This function must return 0 upon errors or 1 otherwise.
 *
 *@@changed V0.9.0 [umoeller]: reworked locale initialization
 */

unsigned long _System _DLL_InitTerm(unsigned long hModule,
                                    unsigned long ulFlag)
{
    APIRET rc;

    switch (ulFlag)
    {
        case 0:
        {
            // store the DLL handle in the global variable
            HookData.hmodDLL = hModule;

            // now initialize the C run-time environment before we
            // call any runtime functions
            if (_CRT_init() == -1)
               return (0);  // error

        break; }

        case 1:
            // DLL being freed: cleanup runtime
            _CRT_term();
            break;

        default:
            // other code: beep for error
            DosBeep(100, 100);
            return (0);     // error
    }

    // a non-zero value must be returned to indicate success
    return (1);
}

/*
 *@@ InitializeGlobalsForHooks:
 *      this gets called from hookInit to initialize
 *      the global variables. We query the PM desktop,
 *      the window list, and other stuff we need all
 *      the time.
 */

VOID InitializeGlobalsForHooks(VOID)
{
    HENUM   henum;
    HWND    hwndThis;
    BOOL    fFound;

    // screen dimensions
    HookData.ulCXScreen = WinQuerySysValue(HWND_DESKTOP, SV_CXSCREEN);
    HookData.ulCYScreen = WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN);

    // PM desktop (the WPS desktop is handled by the daemon)
    HookData.hwndPMDesktop = WinQueryDesktopWindow(HookData.habDaemonObject,
                                                   NULLHANDLE);

    // enumerate desktop window to find the window list:
    // according to PMTREE, the window list has the following
    // window hierarchy:
    //      FRAME
    //        +--- TITLEBAR
    //        +--- Menu
    //        +--- WindowList
    //        +---
    fFound = FALSE;
    henum = WinBeginEnumWindows(HWND_DESKTOP);
    while (     (!fFound)
             && (hwndThis = WinGetNextWindow(henum))
          )
    {
        CHAR    szClass[200];
        if (WinQueryClassName(hwndThis, sizeof(szClass), szClass))
        {
            if (strcmp(szClass, "#1") == 0)
            {
                // frame window: check the children
                HENUM   henumFrame;
                HWND    hwndChild;
                henumFrame = WinBeginEnumWindows(hwndThis);
                while (    (!fFound)
                        && (hwndChild = WinGetNextWindow(henumFrame))
                      )
                {
                    CHAR    szChildClass[200];
                    if (WinQueryClassName(hwndChild, sizeof(szChildClass), szChildClass))
                    {
                        if (strcmp(szChildClass, "WindowList") == 0)
                        {
                            // yup, found:
                            HookData.hwndWindowList = hwndThis;
                            fFound = TRUE;
                        }
                    }
                }
                WinEndEnumWindows(henumFrame);
            }
        }
    }
    WinEndEnumWindows(henum);

}

/*
 *@@ GetFrameWindow:
 *      this finds the desktop window (child of
 *      HWND_DESKTOP) to which the specified window
 *      belongs.
 */

HWND GetFrameWindow(HWND hwndTemp)
{
    HWND    hwndPrevious = NULLHANDLE;
    // climb up the parents tree until we get the Desktop;
    // the previous window will then be the frame (this saves us
    // from querying the window classes and also works
    // for frames within frames)
    while (    (hwndTemp)
            && (hwndTemp != HookData.hwndPMDesktop)
          )
    {
        hwndPrevious = hwndTemp;
        hwndTemp = WinQueryWindow(hwndTemp, QW_PARENT);
    }

    return (hwndPrevious);
}

/*
 *@@ GetScrollBar:
 *
 *@@added V0.9.1 (99-12-03) [umoeller]
 */

HWND GetScrollBar(HWND hwndOwner,
                  BOOL fHorizontal) // in: if TRUE, query horizontal;
                                    // if FALSE; query vertical
{
    HWND    hwndReturn = NULLHANDLE;
    HENUM   henum; // enumeration handle for scroll bar seek
    HWND    hwndFound; // handle of found window
    CHAR    szWinClass[3]; // buffer for window class name
    ULONG   ulWinStyle; // style of the found scroll bar

    // begin window enumeration
    henum = WinBeginEnumWindows(hwndOwner);
    while ((hwndFound = WinGetNextWindow(henum)) != NULLHANDLE)
    {
        // query class name of found window
        WinQueryClassName(hwndFound, 3, szWinClass);

        // is it a scroll bar window?
        if (strcmp(szWinClass, "#8") == 0)
        {
            // query style bits of this scroll bar window
            ulWinStyle = WinQueryWindowULong(hwndFound, QWL_STYLE);

            // return window handle if it matches fHorizontal
            if (fHorizontal)
            {
                // query horizonal mode:
                if ((ulWinStyle & SBS_VERT) == 0)
                        // we must check for this, because SBS_VERT is 1
                        // and SBS_HORZ is 0
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

    // finish window enumeration
    WinEndEnumWindows(henum);

    return (hwndReturn);
}

/******************************************************************
 *                                                                *
 *  Hook interface                                                *
 *                                                                *
 ******************************************************************/

/*
 *@@ hookInit:
 *      registers the hooks (hookInputHook and hookPreAccelHook)
 *      and initializes data.
 *
 *      In any case, a pointer to the DLL's static HOOKDATA
 *      structure is returned. In this struct, the caller
 *      can examine the two flags for whether the hooks
 *      were successfully installed.
 *
 *      Note: All the exported hook* interface functions must
 *      only be called by the same process, which is the
 *      XWorkplace daemon (XWPDAEMN.EXE).
 */

PHOOKDATA EXPENTRY hookInit(HWND hwndDaemonObject, // in: window to post msgs to
                            HMQ hmq)            // in: caller's HMQ (for queue hook)
                                                //     or NULLHANDLE (for system hook)
{
    APIRET arc = NO_ERROR;

    _Pmpf(("Entering hookInit"));

    if (hmtxGlobalHotkeys == NULLHANDLE)
        arc = DosCreateMutexSem(NULL,        // unnameed
                                &hmtxGlobalHotkeys,
                                DC_SEM_SHARED,
                                FALSE);      // initially unowned

    if (arc == NO_ERROR)
    {
        _Pmpf(("Storing data"));
        HookData.hwndDaemonObject = hwndDaemonObject;
        HookData.habDaemonObject = WinQueryAnchorBlock(hwndDaemonObject);
        _Pmpf(("  Done storing data"));

        if (HookData.hmodDLL)       // initialized by _DLL_InitTerm
        {
            _Pmpf(("hookInit: hwndCaller = 0x%lX, hmq = 0x%lX", hwndDaemonObject, hmq));
            _Pmpf(("  Current HookData.fInputHooked: %d", HookData.fInputHooked));

            // initialize globals needed by the hook
            InitializeGlobalsForHooks();

            if (!HookData.fInputHooked)
            {
                // hook not yet installed:

                // check if shared mem for hotkeys
                // was already acquired
                arc = DosGetNamedSharedMem((PVOID*)(&pGlobalHotkeysMain),
                                           IDSHMEM_HOTKEYS,
                                           PAG_READ | PAG_WRITE);
                while (arc == NO_ERROR)
                {
                    // exists already:
                    arc = DosFreeMem(pGlobalHotkeysMain);
                    _Pmpf(("DosFreeMem arc: %d", arc));
                };

                // initialize hotkeys
                arc = DosAllocSharedMem((PVOID*)(&pGlobalHotkeysMain),
                                        IDSHMEM_HOTKEYS,
                                        sizeof(GLOBALHOTKEY), // rounded up to 4KB
                                        PAG_COMMIT | PAG_READ | PAG_WRITE);
                _Pmpf(("DosAllocSharedMem arc: %d", arc));

                if (arc == NO_ERROR)
                {
                    BOOL fSuccess = FALSE;

                    // one default hotkey, let's make this better
                    pGlobalHotkeysMain->usKeyCode = '#';
                    pGlobalHotkeysMain->usFlags = KC_CTRL | KC_ALT;
                    pGlobalHotkeysMain->ulHandle = 100;

                    cGlobalHotkeys = 1;

                    // install hooks
                    fSuccess = WinSetHook(HookData.habDaemonObject,
                                          hmq,         // callers HMQ or NULLHANDLE
                                          HK_INPUT,    // input hook
                                          (PFN)hookInputHook,
                                          HookData.hmodDLL);

                    if (fSuccess)
                    {
                        HookData.fInputHooked = TRUE;
                        fSuccess = WinSetHook(HookData.habDaemonObject,
                                              hmq,
                                              HK_PREACCEL, // pre-accelerator table hook (undocumented)
                                              (PFN)hookPreAccelHook,
                                              HookData.hmodDLL);
                    }

                    if (fSuccess)
                    {
                        // hooks successfully installed:
                        HookData.fPreAccelHooked = TRUE;
                    }
                    else
                        HookData.fInputHooked = FALSE;
                }

            }

            _Pmpf(("  New HookData.fInputHooked: %d", HookData.fInputHooked));

        }

        _Pmpf(("Leaving hookInit"));
    }

    return (&HookData);
}

/*
 *@@ hookKill:
 *      deregisters the hook function and frees allocated
 *      resources.
 *
 *      Note: This function must only be called by the same
 *      process which called hookInit (that is, the daemon),
 *      or resources cannot be properly freed.
 */

BOOL EXPENTRY hookKill(void)
{
    BOOL brc = FALSE;
    PHOOKDATA   pHookData = 0;

    _Pmpf(("hookKill"));

    // if (arc == NO_ERROR)
    {
        if (HookData.fInputHooked)
        {
            WinReleaseHook(HookData.habDaemonObject,
                           NULLHANDLE,
                           HK_INPUT,
                           (PFN)hookInputHook,
                           HookData.hmodDLL);
            brc = TRUE;
        }

        if (HookData.fPreAccelHooked)
        {
            WinReleaseHook(HookData.habDaemonObject,
                           NULLHANDLE,
                           HK_PREACCEL, // pre-accelerator table hook (undocumented)
                           (PFN)hookPreAccelHook,
                           HookData.hmodDLL);
            brc = TRUE;
        }

        if (pGlobalHotkeysMain)
            DosFreeMem(pGlobalHotkeysMain);

        if (hmtxGlobalHotkeys)
        {
            DosCloseMutexSem(hmtxGlobalHotkeys);
            hmtxGlobalHotkeys = NULLHANDLE;
        }
    }

    return (brc);
}

/*
 *@@ hookSetGlobalHotkeys:
 *      this exported function gets called to update the
 *      hook's list of global hotkeys.
 *
 *      This is the only interface to the global hotkeys
 *      list. If we wish to change any of the hotkeys,
 *      we must always pass a complete array of new
 *      hotkeys to this function. This is not terribly
 *      comfortable, but we're dealing with global PM
 *      hooks here, so comfort is not really the main
 *      objective.
 *
 *      XFldObject::xwpSetObjectHotkey can be used for
 *      a more convenient interface. That method will
 *      automatically recompose the complete list when
 *      a single hotkey changes and call this func in turn.
 *
 *      This function copies the given list into a newly
 *      allocated block of shared memory, which is used
 *      by the hook. If a previous such block exists, it
 *      is freed. Access to this block is protected by
 *      a mutex semaphore internally in case WM_CHAR
 *      comes in while the list is being modified
 *      (the hook will not be running on the same thread
 *      as this function, which gets called by the daemon).
 *
 *      Note: This function must only be called by the same
 *      process which called hookInit (that is, the
 *      daemon), because otherwise the shared memory cannot
 *      be properly freed.
 *
 *      This returns the DOS error code of the various
 *      semaphore and shared mem API calls.
 */

APIRET EXPENTRY hookSetGlobalHotkeys(PGLOBALHOTKEY pNewHotkeys, // in: new hotkeys array
                                     ULONG cNewHotkeys)         // in: count of items in array --
                                                              // _not_ array size!!
{
    BOOL    fSemOpened = FALSE,
            fSemOwned = FALSE;
    // request access to the hotkeys mutex
    APIRET arc = DosOpenMutexSem(NULL,       // unnamed
                                 &hmtxGlobalHotkeys);

    if (arc == NO_ERROR)
    {
        fSemOpened = TRUE;
        arc = DosRequestMutexSem(hmtxGlobalHotkeys,
                                 SEM_TIMEOUT);
    }

    _Pmpf(("hookSetGlobalHotkeys: DosRequestMutexSem arc: %d", arc));

    if (arc == NO_ERROR)
    {
        fSemOwned = TRUE;

        if (pGlobalHotkeysMain)
        {
            // hotkeys defined already: free the old ones
            DosFreeMem(pGlobalHotkeysMain);
        }

        arc = DosAllocSharedMem((PVOID*)(&pGlobalHotkeysMain),
                                IDSHMEM_HOTKEYS,
                                sizeof(GLOBALHOTKEY) * cNewHotkeys, // rounded up to 4KB
                                PAG_COMMIT | PAG_READ | PAG_WRITE);
        _Pmpf(("DosAllocSharedMem arc: %d", arc));

        if (arc == NO_ERROR)
        {
            // copy hotkeys to shared memory
            memcpy(pGlobalHotkeysMain,
                   pNewHotkeys,
                   sizeof(GLOBALHOTKEY) * cNewHotkeys);
            cGlobalHotkeys = cNewHotkeys;
        }

    }

    if (fSemOwned)
        DosReleaseMutexSem(hmtxGlobalHotkeys);
    if (fSemOpened)
        DosCloseMutexSem(hmtxGlobalHotkeys);

    return (arc);
}

/******************************************************************
 *                                                                *
 *  System-wide global variables for input hook                   *
 *                                                                *
 ******************************************************************/

HWND    hwndUnderMouse = NULLHANDLE;
HWND    hwndLastFrameUnderMouse = NULLHANDLE;
POINTS  ptsMousePosWin = {0};
POINTL  ptlMousePosAll = {0};

/******************************************************************
 *                                                                *
 *  Input hook function                                           *
 *                                                                *
 ******************************************************************/

/*
 *@@ MoveSlider_Linewise:
 *      this gets called from WMMouseMove_MB3Scroll
 *      if "amplified" mb3-scroll is on. This is what
 *      WarpEnhancer did with MB3 scrolls.
 *
 *      This can get called twice for every WM_MOUSEMOVE,
 *      once for the vertical, once for the horizontal
 *      scroll bar of a window.
 *
 *      Based on code from WarpEnhancer, (C) Achim HasenmÅller.
 *
 *@@added V0.9.1 (99-12-03) [umoeller]
 */

VOID MoveSlider_Linewise(HWND hwndScrollBar,        // in: vertical or horizontal scroll bar wnd
                         USHORT usScrollBarID,      // in: ID of that scroll bar
                         LONG lDelta,               // in: X or Y delta that mouse has moved
                         BOOL fHorizontal)          // in: TRUE: process horizontal, otherwise vertical
{
    USHORT  usScrollCode;

    if (lDelta)
    {
        ULONG   ulMsg;
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
        WinPostMsg(HookData.hwndScrollBarsOwner,
                   ulMsg,
                   (MPARAM)usScrollBarID,
                   MPFROM2SHORT(1, usScrollCode));
        // post end scroll message
        WinPostMsg(HookData.hwndScrollBarsOwner,
                   ulMsg,
                   (MPARAM)usScrollBarID,
                   MPFROM2SHORT(1, SB_ENDSCROLL));
    }
}

/*
 *@@ MoveSlider_Amplified:
 *      this gets called from WMMouseMove_MB3Scroll
 *      if "amplified" mb3-scroll is on.
 *      This is new compared to WarpEnhancer.
 *
 *      This can get called twice for every WM_MOUSEMOVE,
 *      once for the vertical, once for the horizontal
 *      scroll bar of a window.
 *
 *@@added V0.9.1 (99-12-03) [umoeller]
 */

VOID MoveSlider_Amplified(HWND hwndScrollBar,       // in: vertical or horizontal scroll bar wnd
                          USHORT usScrollBarID,     // in: ID of that scroll bar
                          LONG lDelta,              // in: X or Y delta that mouse has moved
                          BOOL fHorizontal)         // in: TRUE: process horizontal, otherwise vertical
{
    SWP     swpOwner;

    // get scroll bar range
    MRESULT mrThumbRange = WinSendMsg(hwndScrollBar,
                                      SBM_QUERYRANGE, 0, 0);
    LONG    lThumbLoLimit = SHORT1FROMMR(mrThumbRange),
            lThumbHiLimit = SHORT2FROMMR(mrThumbRange);

    SHORT   sThumbPos = 1;

    // here come a number of pointers to data we'll
    // need, depending on we're processing the vertical
    // or horizontal scroll bar
    PLONG   plInitialThumbPos;
    PBOOL   pfPostEndScroll;
    PLONG   plWinDim;
    PSHORT  psPreviousThumbPos;
    ULONG   ulMsg;

    if (!fHorizontal)
    {
        // vertical mode:
        plInitialThumbPos = &HookData.lMB3InitialYThumbPos;
        pfPostEndScroll = &HookData.fPostVertEndScroll;
        plWinDim = &swpOwner.cy;
        psPreviousThumbPos = &HookData.sPreviousThumbYPos;
        ulMsg = WM_VSCROLL;
    }
    else
    {
        // horizontal mode:
        plInitialThumbPos = &HookData.lMB3InitialXThumbPos;
        pfPostEndScroll = &HookData.fPostHorzEndScroll;
        plWinDim = &swpOwner.cx;
        psPreviousThumbPos = &HookData.sPreviousThumbXPos;
        ulMsg = WM_HSCROLL;
    }

    // We need to calculate a new _absolute_ scroller
    // position based on the _relative_ delta we got
    // as a parameter. This is a bit tricky:

    // 1) Calculate per-mille that mouse has moved
    //    since MB3 was initially depressed, relative
    //    to the window (scroll bar owner) height.
    //    This will be in the range of -1000 to +1000.

    WinQueryWindowPos(HookData.hwndScrollBarsOwner, &swpOwner);
    // avoid division by zero
    if (*plWinDim)          // cx or cy in swpOwner
    {
        SHORT sPerMilleMoved;

        // We will correlate the mouse movement to the
        // window height. However, to make the MB3 scrolling
        // more similar to regular scroll bar dragging, we
        // must take the height of the scroll bar thumb into
        // account; otherwise the user would have much more
        // mouse mileage with MB3 compared to the regular
        // scroll bar.

        // Unfortunately, there's no "SBM_QUERYTHUMBSIZE" msg,
        // so we need to be a bit more tricky and extract this
        // from the scroll bar's control data.

        WNDPARAMS wp;
        memset(&wp, 0, sizeof(wp));

        // get size of scroll bar control data
        wp.fsStatus = WPM_CBCTLDATA;
        if (WinSendMsg(hwndScrollBar,
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
                if (WinSendMsg(hwndScrollBar,
                               WM_QUERYWINDOWPARAMS,
                               (MPARAM)&wp,
                               0))
                {
                    // success:
                    // cVisible specifies the thumb size in
                    // units of cTotal; we now correlate
                    // the window dimensions to the dark part
                    // of the scroll bar (which is cTotal - cVisible)

                    // avoid division by zero
                    if (    (sbcd.cTotal)
                         && (sbcd.cTotal > sbcd.cVisible)
                       )
                        *plWinDim = *plWinDim
                                    * (sbcd.cTotal - sbcd.cVisible)
                                    / sbcd.cTotal;
                }
            }
        }

        sPerMilleMoved = (lDelta * 1000)
                         / *plWinDim;        // cx or cy in swpOwner
                // this correlates the y movement to the
                // remaining window height;
                // this is now in the range of -1000 thru +1000

        _Pmpf(("  sPerMilleMoved: %d", sPerMilleMoved));

        // 2) amplification desired? (0 is default for 100%)
        if (HookData.HookConfig.sAmplification)
        {
            // yes:
            USHORT usAmpPercent = 100 + (HookData.HookConfig.sAmplification * 10);
                // so we get:
                //      0       -->  100%
                //      2       -->  120%
                //     10       -->  200%
                //     -2       -->  80%
                //     -9       -->  10%
            sPerMilleMoved = sPerMilleMoved * usAmpPercent / 100;
        }

        if (sPerMilleMoved)
        {
            // 3) Correlate this to scroll bar units.
            LONG lSliderRange = (lThumbHiLimit - lThumbLoLimit);
            LONG lSliderMoved =
                    lSliderRange
                    * sPerMilleMoved
                    / 1000;

            _Pmpf(("  lSliderRange: %d", lSliderRange));
            _Pmpf(("  lSliderOfs: %d", lSliderMoved));

            // 4) Calculate new absolute scroll bar position,
            //    from on what we stored when MB3 was initially
            //    depressed.
            sThumbPos = *plInitialThumbPos + lSliderMoved;

            _Pmpf(("  New sThumbPos: %d", sThumbPos));

            // check scroll bar limits:
            if (sThumbPos < lThumbLoLimit)
                sThumbPos = lThumbLoLimit;
            if (sThumbPos > lThumbHiLimit)
                sThumbPos = lThumbHiLimit;

            if (sThumbPos != *psPreviousThumbPos)
            {
                // position changed:
                // now simulate the message flow that
                // the scroll bar normally produces
                // _while_ the scroll bar thumb is being
                // held down:
                // a) adjust scroll bar thumb
                WinSendMsg(hwndScrollBar,
                           SBM_SETPOS,
                           (MPARAM)sThumbPos,
                           0);
                // b) notify owner of the change;
                // this will scroll the
                WinPostMsg(HookData.hwndScrollBarsOwner,
                           ulMsg,                   // WM_xSCROLL
                           (MPARAM)usScrollBarID,
                           MPFROM2SHORT(sThumbPos, SB_SLIDERTRACK));
                *pfPostEndScroll = TRUE;
                        // this flag will provoke a SB_ENDSCROLL
                        // in WM_BUTTON3UP later (hookInputHook)

                // store this thumb position for next time
                *psPreviousThumbPos = sThumbPos;
            }
        }
    }
}

/*
 *@@ WMMouseMove_MB3Scroll:
 *      this gets called when hookInputHook intercepts
 *      WM_MOUSEMOVE to do the "MB3 scrolling" processing.
 *
 *      Returns TRUE if the msg is to be swallowed.
 *
 *      Based on code from WarpEnhancer, (C) Achim HasenmÅller.
 *
 *@@added V0.9.1 (99-12-03) [umoeller]
 */

BOOL WMMouseMove_MB3Scroll(PQMSG pqmsg)      // in: from hookInputHook
{
    BOOL    brc = FALSE;        // no swallow msg
    SHORT   sCurrentMouseX = SHORT1FROMMP(pqmsg->mp1),
            sCurrentMouseY = SHORT2FROMMP(pqmsg->mp1);

    // define initial coordinates if not set yet;
    // this is only -1 if MB3 was just depressed
    if (HookData.sMB3InitialMouseYPos == -1)
    {
        // 1) query window handle of vertical scroll bar
        HookData.hwndVertScrollBar = GetScrollBar(pqmsg->hwnd,
                                                      FALSE);       // vertical
        if (HookData.hwndVertScrollBar == NULLHANDLE)
            // if not found, then try if parent has scroll bars
            HookData.hwndVertScrollBar = GetScrollBar(WinQueryWindow(pqmsg->hwnd,
                                                                     QW_PARENT),
                                                      FALSE);        // vertical

        if (HookData.hwndVertScrollBar)
        {
            // save window ID of scroll bar control
            HookData.usVertScrollBarID = WinQueryWindowUShort(HookData.hwndVertScrollBar, QWS_ID);
            // save owner
            HookData.hwndScrollBarsOwner = WinQueryWindow(HookData.hwndVertScrollBar, QW_OWNER);

            // save initial mouse position
            HookData.sMB3InitialMouseYPos = sCurrentMouseY;
            // save initial scroller position
            HookData.lMB3InitialYThumbPos = (SHORT)WinSendMsg(HookData.hwndVertScrollBar,
                                                              SBM_QUERYPOS, 0, 0);
            // cache that
            HookData.sPreviousThumbYPos = HookData.lMB3InitialYThumbPos;

            brc = FALSE;
        }

        // 2) query window handle of horintal scroll bar
        HookData.hwndHorzScrollBar = GetScrollBar(pqmsg->hwnd,
                                                  TRUE);       // horizontal
        if (HookData.hwndHorzScrollBar == NULLHANDLE)
            // if not found, then try if parent has scroll bars
            HookData.hwndHorzScrollBar = GetScrollBar(WinQueryWindow(pqmsg->hwnd,
                                                                     QW_PARENT),
                                                      TRUE);       // horizontal

        if (HookData.hwndHorzScrollBar)
        {
            // save window ID of scroll bar control
            HookData.usHorzScrollBarID = WinQueryWindowUShort(HookData.hwndHorzScrollBar, QWS_ID);
            // save owner
            HookData.hwndScrollBarsOwner = WinQueryWindow(HookData.hwndHorzScrollBar, QW_OWNER);

            // save initial mouse position
            HookData.sMB3InitialMouseXPos = sCurrentMouseX;
            // save initial scroller position
            HookData.lMB3InitialXThumbPos = (SHORT)WinSendMsg(HookData.hwndHorzScrollBar,
                                                              SBM_QUERYPOS, 0, 0);
            // cache that
            HookData.sPreviousThumbXPos = HookData.lMB3InitialXThumbPos;

            // we use LONGs because we do large
            // no. calcs below
            brc = FALSE;
        }
    }
    else
    {
        // subsequent calls:
        if (HookData.hwndScrollBarsOwner)
        {
            // 1) process vertical (Y) scroll bar
            if (HookData.hwndVertScrollBar)
            {
                // calculate differences between coordinates
                LONG    lDeltaY = HookData.sMB3InitialMouseYPos - sCurrentMouseY;

                if (HookData.HookConfig.fMB3ScrollReverse)
                    // reverse mode: change sign of delta
                    // for subsequent processing
                    lDeltaY = -lDeltaY;

                if (HookData.HookConfig.usScrollMode == SM_AMPLIFIED)
                {
                    if (    (HookData.fPostVertEndScroll)
                                // not first call
                         || (abs(lDeltaY) >= (HookData.HookConfig.usMB3ScrollMin + 1))
                                // or movement is large enough:
                       )
                        MoveSlider_Amplified(HookData.hwndVertScrollBar,
                                             HookData.usVertScrollBarID,
                                             lDeltaY,
                                             FALSE);     // vertical
                }
                else
                    if (abs(lDeltaY) >= (HookData.HookConfig.usMB3ScrollMin + 1))
                    {
                        MoveSlider_Linewise(HookData.hwndVertScrollBar,
                                            HookData.usVertScrollBarID,
                                            lDeltaY,
                                            FALSE);     // vertical
                        HookData.sMB3InitialMouseYPos = sCurrentMouseY;
                    }
            }

            // 2) process horizontal (X) scroll bar
            if (HookData.hwndHorzScrollBar)
            {
                // calculate differences between coordinates
                LONG    lDeltaX = HookData.sMB3InitialMouseXPos - sCurrentMouseX;

                if (!HookData.HookConfig.fMB3ScrollReverse)
                    // not reverse mode: change sign of delta
                    // for subsequent processing
                    lDeltaX = -lDeltaX;

                if (HookData.HookConfig.usScrollMode == SM_AMPLIFIED)
                {
                    if (    (HookData.fPostHorzEndScroll)
                                // not first call
                         || (abs(lDeltaX) >= (HookData.HookConfig.usMB3ScrollMin + 1))
                                // or movement is large enough:
                       )
                        MoveSlider_Amplified(HookData.hwndHorzScrollBar,
                                             HookData.usHorzScrollBarID,
                                             lDeltaX,
                                             TRUE);      // horizontal
                }
                else
                    if (abs(lDeltaX) >= (HookData.HookConfig.usMB3ScrollMin + 1))
                    {
                        MoveSlider_Linewise(HookData.hwndHorzScrollBar,
                                            HookData.usHorzScrollBarID,
                                            lDeltaX,
                                            TRUE);      // horizontal
                        HookData.sMB3InitialMouseXPos = sCurrentMouseX;
                    }
            }
        }
    }

    return (brc);
}

/*
 *@@ WMMouseMove_SlidingFocus:
 *      this gets called when hookInputHook intercepts
 *      WM_MOUSEMOVE to do the "sliding focus" processing.
 *
 *      This has been exported to a separate function both
 *      for clarity and speed. The code in the hook function
 *      itself should be as short as possible to avoid
 *      refilling the processor caches all the time, because
 *      that function gets called for every single message on
 *      the system.
 *
 *      This function evaluates the data in pqmsg to find out
 *      whether the mouse has moved over a new frame window.
 *      If so, XDM_SLIDINGFOCUS is posted to the daemon's
 *      object window, which will then do the actual focus
 *      and active window processing.
 */

VOID WMMouseMove_SlidingFocus(PQMSG pqmsg,      // in: from hookInputHook
                              BOOL fMouseMoved) // in: TRUE if mouse has been moved since previous WM_MOUSEMOVE
                                                // (determined by hookInputHook)
{
    // get currently active window; this can only
    // be a frame window (WC_FRAME)
    HWND    hwndActiveNow = WinQueryActiveWindow(HWND_DESKTOP);

    // check 1: check if the active window is still the
    //          the one which was activated by ourselves
    //          previously (either by the hook during WM_BUTTON1DOWN
    //          or by the daemon in sliding focus processing):
    if (hwndActiveNow)
    {
        if (hwndActiveNow != HookData.hwndActivatedByUs)
        {
            // active window is not the one we set active:
            // this probably means that some new
            // window popped up which we haven't noticed
            // and was explicitly made active either by the
            // shell or by an application, so we use this
            // for the below checks. Otherwise, sliding focus
            // would be disabled after a new window has popped
            // up until the mouse was moved over a new frame window.
            hwndLastFrameUnderMouse = hwndActiveNow;
            HookData.hwndActivatedByUs = hwndActiveNow;
        }
    }

    if (pqmsg->hwnd != hwndUnderMouse)
        // mouse has moved to a different window:
        hwndUnderMouse = pqmsg->hwnd;

    if (   (fMouseMoved)            // has mouse moved?
        && (HookData.HookConfig.fSlidingFocus)  // sliding focus enabled?
       )
    {
        // OK:
        HWND    hwndFrame;
        BOOL    fIsSeamless = FALSE;
        CHAR    szClassUnderMouse[MAXNAMEL+4] = "";
        HWND    hwndFocusNow = WinQueryFocus(HWND_DESKTOP);

        // check 2: make sure mouse is not captured
        if (WinQueryCapture(HWND_DESKTOP) != NULLHANDLE)
            return;

        // check 3: exclude certain window types from
        //          sliding focus, because these don't
        //          work right; we do this by checking
        //          the window classes

        WinQueryClassName(pqmsg->hwnd,
                          sizeof(szClassUnderMouse),
                          szClassUnderMouse);

        // skip menus with focus
        if (hwndFocusNow)
        {
            CHAR    szFocusClass[MAXNAMEL+4] = "";
            WinQueryClassName(hwndFocusNow,
                              sizeof(szFocusClass),
                              szFocusClass);

            if (strcmp(szFocusClass, "#4") == 0)
                return;
        }

        // skip dropped-down combo boxes under mouse
        if (strcmp(szClassUnderMouse, "#7") == 0)
            // listbox: check if the parent is the PM desktop
            if (WinQueryWindow(pqmsg->hwnd, QW_PARENT) == HookData.hwndPMDesktop)
                // yes: then it's the open list box window of
                // a dropped-down combo box --> ignore
                return;

        // skip seamless Win-OS/2 frames
        // (we must check the actual window under the
        // mouse, because seamless windows have a
        // regular WC_FRAME desktop window, which
        // is apparently invisible though)
        if (strcmp(szClassUnderMouse, "SeamlessClass") == 0)
            // Win-OS/2 window:
            fIsSeamless = TRUE;

        // OK, enough checks. Now let's do the
        // sliding focus:

        // get the frame window parent of the window
        // under the mouse
        hwndFrame = GetFrameWindow(pqmsg->hwnd);

        if (hwndFrame != hwndLastFrameUnderMouse)
        {
            // OK, mouse moved to a new desktop window:
            // store that for next time
            hwndLastFrameUnderMouse = hwndFrame;

            // notify daemon of the change;
            // it is the daemon which does the rest
            // (timer handling, window activation etc.)
            // DosBeep(5000, 10);
            WinPostMsg(HookData.hwndDaemonObject,
                       XDM_SLIDINGFOCUS,
                       (MPARAM)hwndFrame,
                       (MPARAM)fIsSeamless);
        }
    }
}

/*
 *@@ WMMouseMove_AutoHideMouse:
 *      this gets called when hookInputHook intercepts
 *      WM_MOUSEMOVE to automatically hide the mouse
 *      pointer.
 *
 *      We start a timer here which posts WM_TIMER
 *      to fnwpDaemonObject in the daemon. So it's the
 *      daemon which actually hides the mouse.
 *
 *      Based on code from WarpEnhancer, (C) Achim HasenmÅller.
 *
 *@@added V0.9.1 (99-12-03) [umoeller]
 */

VOID WMMouseMove_AutoHideMouse(VOID)
{
    // is the timer currently running?
    if (HookData.idAutoHideTimer != NULLHANDLE)
    {
        // stop the running async timer
        WinStopTimer(HookData.habDaemonObject,
                     HookData.hwndDaemonObject,
                     HookData.idAutoHideTimer);
        HookData.idAutoHideTimer = NULLHANDLE;
    }

    // show the mouse pointer now
    if (HookData.fMousePointerHidden)
    {
        WinShowPointer(HWND_DESKTOP, TRUE);
        HookData.fMousePointerHidden = FALSE;
    }

    // (re)start timer
    HookData.idAutoHideTimer =
        WinStartTimer(HookData.habDaemonObject,
                      HookData.hwndDaemonObject,
                      TIMERID_AUTOHIDEMOUSE,
                      (HookData.HookConfig.ulAutoHideDelay + 1) * 1000);
}

/*
 *@@ WMButton_SystemMenuContext:
 *      this gets called from hookInputHook upon
 *      MB2 clicks to copy a window's system menu
 *      and display it as a popup menu on the
 *      title bar.
 *
 *      Based on code from WarpEnhancer, (C) Achim HasenmÅller.
 */

VOID WMButton_SystemMenuContext(PQMSG pqmsg)     // of WM_BUTTON2CLICK
{
    POINTL      ptlMouse; // mouse coordinates
    HWND        hwndFrame; // handle of the frame window (parent)

    // get mouse coordinates (absolute coordinates)
    WinQueryPointerPos(HWND_DESKTOP, &ptlMouse);
    // query parent of title bar window (frame window)
    hwndFrame = WinQueryWindow(pqmsg->hwnd, QW_PARENT);
    if (hwndFrame)
    {
        // query handle of system menu icon (action bar style)
        HWND hwndSysMenuIcon = WinWindowFromID(hwndFrame, FID_SYSMENU);
        if (hwndSysMenuIcon)
        {
            HWND        hNewMenu; // handle of our copied menu
            HWND        SysMenuHandle;

            // query item id of action bar system menu
            SHORT id = (SHORT)(USHORT)(ULONG)WinSendMsg(hwndSysMenuIcon,
                                                        MM_ITEMIDFROMPOSITION,
                                                        MPFROMSHORT(0), 0);
            // query item id of action bar system menu
            MENUITEM    mi = {0};
            CHAR        szItemText[100]; // buffer for menu text
            WinSendMsg(hwndSysMenuIcon, MM_QUERYITEM,
                       MPFROM2SHORT(id, FALSE),
                       MPFROMP(&mi));
            // submenu is our system menu
            SysMenuHandle = mi.hwndSubMenu;

            // create a new empty menu
            hNewMenu = WinCreateMenu(HWND_OBJECT, NULL);
            if (hNewMenu)
            {
                // query how menu entries the original system menu has
                SHORT SysMenuItems = (SHORT)WinSendMsg(SysMenuHandle,
                                                       MM_QUERYITEMCOUNT,
                                                       0, 0);
                ULONG i;
                // loop through all entries in the original system menu
                for (i = 0; i < SysMenuItems; i++)
                {
                    id = (SHORT)(USHORT)(ULONG)WinSendMsg(SysMenuHandle,
                                                          MM_ITEMIDFROMPOSITION,
                                                          MPFROMSHORT(i),
                                                          0);
                    // get this menu item into mi buffer
                    WinSendMsg(SysMenuHandle,
                               MM_QUERYITEM,
                               MPFROM2SHORT(id, FALSE),
                               MPFROMP(&mi));
                    // query text of this menu entry into our buffer
                    WinSendMsg(SysMenuHandle,
                               MM_QUERYITEMTEXT,
                               MPFROM2SHORT(id, sizeof(szItemText)-1),
                               MPFROMP(szItemText));
                    // add this entry to our new menu
                    WinSendMsg(hNewMenu,
                               MM_INSERTITEM,
                               MPFROMP(&mi),
                               MPFROMP(szItemText));
                }

                // display popup menu
                WinPopupMenu(HWND_DESKTOP, hwndFrame, hNewMenu,
                             ptlMouse.x, ptlMouse.y, 0x8007, PU_HCONSTRAIN |
                             PU_VCONSTRAIN | PU_MOUSEBUTTON1 |
                             PU_MOUSEBUTTON2 | PU_KEYBOARD);
            }
        }
    }
}

/*
 *@@ WMChord_WinList:
 *      this displays the window list at the current
 *      mouse position when WM_CHORD comes in.
 *
 *      Based on code from WarpEnhancer, (C) Achim HasenmÅller.
 */

VOID WMChord_WinList(VOID)
{
    POINTL  ptlMouse;       // mouse coordinates
    SWP     WinListPos;     // position of window list window
    LONG WinListX, WinListY; // new ordinates of window list window
    LONG DesktopCX, DesktopCY; // width and height of screen
    // get mouse coordinates (absolute coordinates)
    WinQueryPointerPos(HWND_DESKTOP, &ptlMouse);
    // get position of window list window
    WinQueryWindowPos(HookData.hwndWindowList,
                      &WinListPos);
    // calculate window list position (mouse pointer is center)
    WinListX = ptlMouse.x - (WinListPos.cx / 2);
    if (WinListX < 0)
        WinListX = 0;
    WinListY = ptlMouse.y - (WinListPos.cy / 2);
    if (WinListY < 0)
        WinListY = 0;
    if (WinListX + WinListPos.cx > HookData.ulCXScreen)
        WinListX = HookData.ulCXScreen - WinListPos.cx;
    if (WinListY + WinListPos.cy > HookData.ulCYScreen)
        WinListY = HookData.ulCYScreen - WinListPos.cy;
    // set window list window to calculated position
    WinSetWindowPos(HookData.hwndWindowList, HWND_TOP,
                    WinListX, WinListY, 0, 0,
                    SWP_MOVE | SWP_SHOW | SWP_ZORDER);
    // now make it the active window
    WinSetActiveWindow(HWND_DESKTOP,
                       HookData.hwndWindowList);

}

/*
 *@@ hookInputHook:
 *      Input queue hook. According to PMREF, this hook gets
 *      called just before the system returns from a WinGetMsg
 *      or WinPeekMsg call -- so this does _not_ get called as
 *      a result of WinPostMsg, but before the msg is _retrieved_.
 *
 *      However, only _posted_ messages go thru this hook. _Sent_
 *      messages never get here; there is a separate hook type
 *      for that.
 *
 *      This implements the "hot corner" and "sliding focus"
 *      features and might call WMMouseMove_SlidingFocus in turn.
 *
 *      Note: WM_CHAR messages (hotkeys) are not processed here,
 *      but rather in hookPreAccelHook. See remarks there.
 *
 *      We return TRUE if the message is to be swallowed, or FALSE
 *      if the current application (or the next hook in the hook
 *      chain) should still receive the message.
 *
 *@@changed V0.9.1 (99-12-03) [umoeller]: added MB3 mouse scroll
 */

BOOL EXPENTRY hookInputHook(HAB hab,        // in: anchor block of receiver wnd
                            PQMSG pqmsg,    // in/out: msg data
                            USHORT fs)      // in: either PM_REMOVE or PM_NOREMOVE
{
    // set return value:
    // per default, pass message on to next hook or application
    BOOL brc = FALSE;

    if (pqmsg)
        switch(pqmsg->msg)
        {
            /*
             * WM_BUTTON1DOWN:
             *      if "sliding focus" is on and "bring to top" is off,
             *      we need to explicitly raise the window under the mouse,
             *      because this fails otherwise. Apparently, PM checks
             *      internally when the mouse is clicked whether the window
             *      is active; if so, it thinks it must be on top already.
             *      Since the active and topmost window can now be different,
             *      we need to implement raising the window ourselves.
             *
             *      Parameters:
             *      -- POINTS mp1: ptsPointerPos
             *      -- USHORT SHORT1FROMMP(mp2): fsHitTestResult
             *      -- USHORT SHORT2FROMMP(mp2): fsFlags (KC_* as with WM_CHAR)
             */

            case WM_BUTTON1DOWN:
                if (    (HookData.HookConfig.fSlidingFocus)
                     && (!HookData.HookConfig.fBring2Top)
                   )
                {
                    WinSetWindowPos(GetFrameWindow(pqmsg->hwnd),
                                    HWND_TOP,
                                    0,
                                    0,
                                    0,
                                    0,
                                    SWP_ZORDER);
                }
            break;

            /*
             *@@ WM_BUTTON2DOWN:
             *      if mb2 is clicked on titlebar,
             *      show system menu as context menu.
             *
             *      This is from WarpEnhancer, (C) Achim HasenmÅller.
             */

            case WM_BUTTON2DOWN:
            {
                if (HookData.HookConfig.fSysMenuMB2TitleBar)
                {
                    CHAR szWindowClass[3];
                    // get class name of window being created
                    WinQueryClassName(pqmsg->hwnd,
                                      sizeof(szWindowClass),
                                      szWindowClass);
                    // mouse button 2 was pressed over a title bar control
                    if (strcmp(szWindowClass, "#9") == 0)
                    {
                        // copy system menu and display it as context menu
                        WMButton_SystemMenuContext(pqmsg);
                        brc = TRUE; // swallow message
                    }
                }
            break; }

            /*
             * WM_BUTTON3DOWN:
             *      start scrolling.
             *
             *      This is from WarpEnhancer, (C) Achim HasenmÅller.
             */

            case WM_BUTTON3DOWN: // mouse button 3 was pressed down
            {
                if (HookData.HookConfig.fMB3Scroll)
                {
                    // set scrolling mode to on
                    HookData.fMB3Scrolling = TRUE;
                    // indicate that initial mouse positions have to be recalculated
                    HookData.sMB3InitialMouseYPos = -1;
                    // reset flags for WM_BUTTON3UP below
                    HookData.fPostVertEndScroll = FALSE;
                    HookData.fPostHorzEndScroll = FALSE;

                    // capture messages for that window
                    WinSetCapture(HWND_DESKTOP, pqmsg->hwnd);
                }
            break; }

            /*
             * WM_BUTTON3UP:
             *      stop scrolling.
             *
             *      This is from WarpEnhancer, (C) Achim HasenmÅller.
             */

            case WM_BUTTON3UP: // mouse button 3 has been released
            {
                if (HookData.fMB3Scrolling)
                {
                    // set scrolling mode to off
                    HookData.fMB3Scrolling = FALSE;

                    // release capture
                    WinSetCapture(HWND_DESKTOP, NULLHANDLE);

                    if (HookData.fPostVertEndScroll)
                    {
                        // we did move the scoller previously:
                        // send end scroll message
                        WinPostMsg(HookData.hwndScrollBarsOwner,
                                   WM_VSCROLL,
                                   (MPARAM)(HookData.usVertScrollBarID),
                                   MPFROM2SHORT(HookData.sPreviousThumbYPos,
                                                SB_ENDSCROLL));
                    }

                    if (HookData.fPostHorzEndScroll)
                    {
                        // we did move the scoller previously:
                        // send end scroll message
                        WinPostMsg(HookData.hwndScrollBarsOwner,
                                   WM_HSCROLL,
                                   (MPARAM)(HookData.usHorzScrollBarID),
                                   MPFROM2SHORT(HookData.sPreviousThumbXPos,
                                                SB_ENDSCROLL));
                    }
                }
            break; }

            /*
             * WM_CHORD:
             *      MB 1 and 2 pressed simultaneously.
             *      If enabled, we show the window list at the
             *      current mouse position.
             *
             *      This is from WarpEnhancer, (C) Achim HasenmÅller.
             */

            case WM_CHORD:
            {
                // feature enabled?
                if (HookData.HookConfig.fChordWinList)
                {
                    WMChord_WinList();
                    brc = TRUE;         // swallow message
                }
                else
                    brc = FALSE;
            break; }

            /*
             * WM_MOUSEMOVE:
             *      "hot corners" and "sliding focus" support.
             *      In the hook, we "only" check for whether the mouse
             *      has moved to a new desktop window (a new child of
             *      HWND_DESKTOP);
             *      if so, we notify the daemon of the change by
             *      posting XDM_SLIDINGFOCUS. It is the daemon
             *      which actually activates windows.
             *
             *      WM_MOUSEMOVE parameters:
             *      -- SHORT SHORT1FROMMP(mp1): sxMouse (in win coords).
             *      -- SHORT SHORT2FROMMP(mp1): syMouse (in win coords).
             *      -- USHORT SHORT1FROMMP(mp2): uswHitTest: NULL if mouse
             *                  is currently captured; otherwise result
             *                  of WM_HITTEST message.
             *      -- USHORT SHORT2FROMMP(mp2): KC_* flags as with WM_CHAR
             *                  or KC_NONE (no key pressed).
             */

            case WM_MOUSEMOVE:
            {
                BOOL    fMouseMoved = FALSE;
                SHORT   sxMouse = SHORT1FROMMP(pqmsg->mp1),
                        syMouse = SHORT2FROMMP(pqmsg->mp1);

                // store mouse pos in win coords
                ptsMousePosWin.x = sxMouse;
                ptsMousePosWin.y = syMouse;

                // store mouse pos in Desktop coords
                if (ptlMousePosAll.x != pqmsg->ptl.x)
                {
                    ptlMousePosAll.x = pqmsg->ptl.x;
                    fMouseMoved = TRUE;
                }
                if (ptlMousePosAll.y != pqmsg->ptl.y)
                {
                    ptlMousePosAll.y = pqmsg->ptl.y;
                    fMouseMoved = TRUE;
                }

                if (fMouseMoved)
                {
                    BYTE    bHotCorner = 0;

                    /*
                     * MB3 scroll
                     *
                     */

                    // are we currently in scrolling mode?
                    if (    (HookData.HookConfig.fMB3Scroll)
                         && (HookData.fMB3Scrolling)
                       )
                    {
                        brc = WMMouseMove_MB3Scroll(pqmsg);

                        break;  // skip the rest
                    }

                    /*
                     * hot corners:
                     *
                     */

                    if (ptlMousePosAll.x == 0)
                    {
                        if (ptlMousePosAll.y == 0)
                            // lower left corner:
                            bHotCorner = 1;
                        else if (ptlMousePosAll.y == HookData.ulCYScreen - 1)
                            // top left corner:
                            bHotCorner = 2;
                    }
                    else if (ptlMousePosAll.x == HookData.ulCXScreen - 1)
                    {
                        if (ptlMousePosAll.y == 0)
                            // lower right corner:
                            bHotCorner = 3;
                        else if (ptlMousePosAll.y == HookData.ulCYScreen - 1)
                            // top right corner:
                            bHotCorner = 4;
                    }

                    if (bHotCorner != 0)
                        // notify thread-1 object window
                        WinPostMsg(HookData.hwndDaemonObject,
                                   XDM_HOTCORNER,
                                   (MPARAM)bHotCorner,
                                   (MPARAM)NULL);

                    // is hide pointer set to on?
                    if (HookData.HookConfig.fAutoHideMouse)
                        WMMouseMove_AutoHideMouse();
                }

                /*
                 * sliding focus:
                 *
                 */

                WMMouseMove_SlidingFocus(pqmsg, fMouseMoved);

            break; } // WM_MOUSEMOVE
        }

    return (brc);                           // msg not processed if FALSE
}

/*
 *@@ WMChar_Hotkeys:
 *      this gets called from hookPreAccelHook to
 *      process WM_CHAR messages.
 *
 *      The WM_CHAR message is one of the most complicated
 *      messages which exists.
 *
 *      As opposed to folder hotkeys (folder.c),
 *      for the global object hotkeys, we need a more
 *      sophisticated processing, because we cannot rely
 *      on the usch field, which is different between
 *      PM and VIO sessions (apparently the translation
 *      is taking place differently for VIO sessions).
 *
 *      As a result, we can only use the scan code to
 *      identify hotkeys. See GLOBALHOTKEY for details.
 *
 *      Since we use a dynamically allocated array of
 *      GLOBALHOTKEY structures, to make this thread-safe,
 *      we have to use shared memory and a global mutex
 *      semaphore. See hookSetGlobalHotkeys.
 */

BOOL WMChar_Hotkeys(USHORT usFlags,  // in: SHORT1FROMMP(mp1) from WM_CHAR
                    UCHAR ucScanCode, // in: CHAR4FROMMP(mp1) from WM_CHAR
                    USHORT usch,     // in: SHORT1FROMMP(mp2) from WM_CHAR
                    USHORT usvk)     // in: SHORT2FROMMP(mp2) from WM_CHAR
{
    // set return value:
    // per default, pass message on to next hook or application
    BOOL brc = FALSE;

    // request access to the hotkeys mutex:
    // first we need to open it, because this
    // code can be running in any PM thread in
    // any process
    APIRET arc = DosOpenMutexSem(NULL,       // unnamed
                                 &hmtxGlobalHotkeys);
    if (arc == NO_ERROR)
    {
        // OK, semaphore opened: request access
        arc = DosRequestMutexSem(hmtxGlobalHotkeys,
                                 SEM_TIMEOUT);

        _Pmpf(("WM_CHAR DosRequestMutexSem arc: %d", arc));

        if (arc == NO_ERROR)
        {
            // OK, we got the mutex:
            PGLOBALHOTKEY pGlobalHotkeysShared = NULL;

            // request access to shared memory
            // with hotkey definitions:
            arc = DosGetNamedSharedMem((PVOID*)(&pGlobalHotkeysShared),
                                       IDSHMEM_HOTKEYS,
                                       PAG_READ | PAG_WRITE);

            _Pmpf(("  DosGetNamedSharedMem arc: %d", arc));

            if (arc == NO_ERROR)
            {
                // OK, we got the shared hotkeys:
                USHORT  us;

                PGLOBALHOTKEY pKeyThis = pGlobalHotkeysShared;

                        /*
           In PM session:
                                        usFlags         usvk usch       ucsk
                Ctrl alone              VK SC CTRL       0a   0          1d
                Ctrl-A                     SC CTRL        0  61          1e
                Ctrl-Alt                VK SC CTRL ALT   0b   0          38
                Ctrl-Alt-A                 SC CTRL ALT    0  61          1e

           In VIO session:
                Ctrl alone                 SC CTRL       07   0          1d
                Ctrl-A                     SC CTRL        0   1e01       1e
                Ctrl-Alt                   SC CTRL ALT   07   0          38
                Ctrl-Alt-A                 SC CTRL ALT   20   1e00       1e

                Alt-A                      SC      ALT   20   1e00
                Ctrl-E                     SC CTRL        0   3002

                        */

                #ifdef _PMPRINTF_
                        CHAR    szFlags[2000] = "";
                        if (usFlags & KC_CHAR)                      // 0x0001
                            strcat(szFlags, "KC_CHAR ");
                        if (usFlags & KC_VIRTUALKEY)                // 0x0002
                            strcat(szFlags, "KC_VIRTUALKEY ");
                        if (usFlags & KC_SCANCODE)                  // 0x0004
                            strcat(szFlags, "KC_SCANCODE ");
                        if (usFlags & KC_SHIFT)                     // 0x0008
                            strcat(szFlags, "KC_SHIFT ");
                        if (usFlags & KC_CTRL)                      // 0x0010
                            strcat(szFlags, "KC_CTRL ");
                        if (usFlags & KC_ALT)                       // 0x0020
                            strcat(szFlags, "KC_ALT ");
                        if (usFlags & KC_KEYUP)                     // 0x0040
                            strcat(szFlags, "KC_KEYUP ");
                        if (usFlags & KC_PREVDOWN)                  // 0x0080
                            strcat(szFlags, "KC_PREVDOWN ");
                        if (usFlags & KC_LONEKEY)                   // 0x0100
                            strcat(szFlags, "KC_LONEKEY ");
                        if (usFlags & KC_DEADKEY)                   // 0x0200
                            strcat(szFlags, "KC_DEADKEY ");
                        if (usFlags & KC_COMPOSITE)                 // 0x0400
                            strcat(szFlags, "KC_COMPOSITE ");
                        if (usFlags & KC_INVALIDCOMP)               // 0x0800
                            strcat(szFlags, "KC_COMPOSITE ");

                        _Pmpf(("  usFlags: 0x%lX -->", usFlags));
                        _Pmpf(("    %s", szFlags));
                        _Pmpf(("  ucScanCode: 0x%lX", ucScanCode));
                        _Pmpf(("  usvk: 0x%lX", usvk));
                        _Pmpf(("  usch: 0x%lX", usch));
                #endif

                // filter out unwanted flags
                usFlags &= (KC_VIRTUALKEY | KC_CTRL | KC_ALT | KC_SHIFT);

                _Pmpf(("  Checking %d hotkeys", cGlobalHotkeys));

                // now go through the shared hotkey list and check
                // if the pressed key was assigned an action to
                us = 0;
                for (us = 0;
                     us < cGlobalHotkeys;
                     us++)
                {
                    _Pmpf(("    item %d: scan code %lX", us, pKeyThis->ucScanCode));

                    if (   (pKeyThis->usFlags == usFlags)
                        && (pKeyThis->ucScanCode == ucScanCode)
                       )
                    {
                        // hotkey found:
                        // notify daemon
                        _Pmpf(("Hotkey found, hwndNotify: 0x%lX", HookData.hwndNotify));
                        WinPostMsg(HookData.hwndDaemonObject,
                                   XDM_HOTKEYPRESSED,
                                   (MPARAM)(pKeyThis->ulHandle),
                                   (MPARAM)0);

                        // reset return code: swallow this message
                        brc = TRUE;

                        break;
                    }

                    pKeyThis++;
                } // end for

                DosFreeMem(pGlobalHotkeysShared);
            } // end if DosGetNamedSharedMem

            DosReleaseMutexSem(hmtxGlobalHotkeys);
        } // end if DosRequestMutexSem

        DosCloseMutexSem(hmtxGlobalHotkeys);
    } // end if DosOpenMutexSem

    return (brc);
}

/*
 *@@ hookPreAccelHook:
 *      this is the pre-accelerator-table hook. Like
 *      hookInputHook, this gets called when messages
 *      are coming in from the system input queue, but
 *      as opposed to a "regular" input hook, this hook
 *      gets called even before translation from accelerator
 *      tables occurs.
 *
 *      Pre-accel hooks are not documented in the PM Reference.
 *      I have found this idea (and part of the implementation)
 *      in the source code of ProgramCommander/2, so thanks
 *      go out (once again) to Roman Stangl.
 *
 *      In this hook, we check for the global object hotkeys
 *      so that we can use certain key combinations regardless
 *      of whether they might be currently used in application
 *      accelerator tables. This is especially useful for
 *      "Alt" key combinations, which are usually intercepted
 *      when menus have corresponding shortcuts.
 *
 *      As a result, as opposed to other hotkey software you
 *      might know, XWorkplace does properly react to "Ctrl+Alt"
 *      keyboard combinations, even if a menu would get called
 *      with the "Alt" key. ;-)
 *
 *      We return TRUE if the message is to be swallowed, or FALSE
 *      if the current application (or the next hook in the hook
 *      chain) should still receive the message.
 */

BOOL EXPENTRY hookPreAccelHook(HAB hab, PQMSG pqmsg, ULONG option)
{
    // set return value:
    // per default, pass message on to next hook or application
    BOOL brc = FALSE;

    if (pqmsg == NULL)
        return (FALSE);

    switch(pqmsg->msg)
    {
        /*
         * WM_CHAR:
         *      keyboard activity. We check for
         *      object hotkeys; if one is found,
         *      we post XWPDAEMN's object window
         *      a notification.
         */

        case WM_CHAR:
        {
            // if (HookData.HookConfig.fObjectHotkeys) // ### !!!
            {
                USHORT usFlags    = SHORT1FROMMP(pqmsg->mp1);
                UCHAR  ucRepeat   = CHAR3FROMMP(pqmsg->mp1);
                UCHAR  ucScanCode = CHAR4FROMMP(pqmsg->mp1);
                USHORT usch       = SHORT1FROMMP(pqmsg->mp2);
                USHORT usvk       = SHORT2FROMMP(pqmsg->mp2);

                if (
                        // process only key-down messages
                        ((usFlags & KC_KEYUP) == 0)
                        // check flags:
                    &&  (     ((usFlags & KC_VIRTUALKEY) != 0)
                              // Ctrl pressed?
                           || ((usFlags & KC_CTRL) != 0)
                              // Alt pressed?
                           || ((usFlags & KC_ALT) != 0)
                              // or one of the Win95 keys?
                           || (   ((usFlags & KC_VIRTUALKEY) == 0)
                               && (     (usch == 0xEC00)
                                    ||  (usch == 0xED00)
                                    ||  (usch == 0xEE00)
                                  )
                              )
                        )
                        // filter out those ugly composite key (accents etc.)
                   &&   ((usFlags & (KC_DEADKEY | KC_COMPOSITE | KC_INVALIDCOMP)) == 0)
                   )
                {
                    brc = WMChar_Hotkeys(usFlags, ucScanCode, usch, usvk);
                            // returns TRUE (== swallow) if hotkey was found
                }
            }
        break; }    // WM_CHAR
    } // end switch(msg)

    return (brc);
}

