
/*
 *@@sourcefile xwphook.c:
 *      this has the all the code for XWPHOOK.DLL, which implements
 *      all the XWorkplace PM system hooks, as well as a few exported
 *      interface functions which may only be called by the XWorkplace
 *      daemon process (XWPDAEMN.EXE).
 *
 *      There are four hooks at present:
 *      -- hookSendMsgHook
 *      -- hookLockupHook
 *      -- hookInputHook
 *      -- hookPreAccelHook
 *      See the remarks in the respective functions.
 *
 *      Most of the hook features started out with code taken
 *      from ProgramCommander/2 and WarpEnhancer and were then
 *      extended.
 *
 *      I have tried to design the hooks for clarity and lucidity with
 *      respect to interfaces and configuration --
 *      as if that was possible with system hooks at all, since they
 *      are so messy by nature.
 *
 *      XWorkplace implementation notes:
 *
 *      --  System hooks are possibly running in the context of each PM
 *          thread which is processing a message -- that is, possibly each
 *          thread on the system which has created a message queue.
 *
 *          As a result, great care must be taken when accessing global
 *          data, and one must keep in mind for each function which thread
 *          it might run in, or otherwise we'd be asking for system hangs.
 *          Don't expect OS/2 to handle exceptions correctly if all PM
 *          programs crash at the same time.
 *
 *          It _is_ possible to access static global variables of
 *          this DLL from every function, because the DLL is linked
 *          with the STATIC SHARED flags (see xwphook.def).
 *          This means that the single data segment of the DLL becomes
 *          part of each process which is using the DLL
 *          (that is, each PM process on the system). We use this
 *          for the HOOKDATA structure, which is a static global structure
 *          in the DLL's shared data segment and is returned to the daemon
 *          when it calls hookInit in the DLL.
 *
 *          HOOKDATA includes a HOOKCONFIG structure. As a result, to
 *          configure the basic hook flags, the daemon can simply
 *          modify the fields in the HOOKDATA structure to configure
 *          the hook's behavior.
 *
 *          It is however _not_ possible to use malloc() to allocate global
 *          memory and use it between several calls of the hooks, because
 *          that memory will belong to one process only, even if the pointer
 *          is stored in a global DLL variable. The next time the hook gets
 *          called and accesses that memory, some fairly random application
 *          will crash (the one the hook is currently called for), or the
 *          system will hang completely.
 *
 *          For this reason, the hooks use another block of shared memory
 *          internally, which is protected by a mutex semaphore, for storing
 *          the list of object hotkeys (which is variable in size).
 *          This block is (re)allocated in hookSetGlobalHotkeys and
 *          requested in the hook when WM_CHAR comes in. The mutex is
 *          necessary because when hotkeys are changed, the daemon changes
 *          the structure by calling hookSetGlobalHotkeys.
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
 *          for details). Also, if something goes wrong, it's only
 *          the daemon which crashes, and not the entire WPS.
 *
 *          When any configuration data changes which is of importance
 *          to the hook, XFLDR.DLL writes this data to OS2.INI and
 *          then posts the daemon a message. The daemon then reads
 *          the new data and notifies the hook thru defined interface
 *          functions. All configuration structures are declared in
 *          xwphook.h.
 *
 *@@added V0.9.0 [umoeller]
 *@@header "hook\hook_private.h"
 */

/*
 *      Copyright (C) 1999-2000 Ulrich Mîller.
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

#include "setup.h"

/******************************************************************
 *                                                                *
 *  Global variables                                              *
 *                                                                *
 ******************************************************************/

/*
 *      We must initialize all these variables to
 *      something, because if we don't, the compiler
 *      won't put them in the DLL's shared data segment
 *      (see introductory notes).
 */

HOOKDATA        G_HookData = {0};
    // this includes HOOKCONFIG; its address is
    // returned from hookInit and stored within the
    // daemon to configure the hook

PGLOBALHOTKEY   G_paGlobalHotkeys = NULL;
    // pointer to block of shared memory containing
    // the hotkeys; maintained by hookSetGlobalHotkeys,
    // requested by the pre-accel hook
ULONG           G_cGlobalHotkeys = 0;
    // count of items in that array (_not_ array size!)

PFUNCTIONKEY    G_paFunctionKeys = NULL;
    // pointer to block of shared memory containing
    // the function keys; maintained by hookSetGlobalHotkeys,
    // requested by the pre-accel hook
ULONG           G_cFunctionKeys = 0;
    // count of items in that array (_not_ array size!)

HMTX            G_hmtxGlobalHotkeys = NULLHANDLE;
    // mutex for protecting the keys arrays

/*
 * Prototypes:
 *
 */

VOID EXPENTRY hookSendMsgHook(HAB hab, PSMHSTRUCT psmh, BOOL fInterTask);
VOID EXPENTRY hookLockupHook(HAB hab, HWND hwndLocalLockupFrame);
BOOL EXPENTRY hookInputHook(HAB hab, PQMSG pqmsg, USHORT fs);
BOOL EXPENTRY hookPreAccelHook(HAB hab, PQMSG pqmsg, ULONG option);

// _CRT_init is the C run-time environment initialization function.
// It will return 0 to indicate success and -1 to indicate failure.
int _CRT_init(void);

// _CRT_term is the C run-time environment termination function.
// It only needs to be called when the C run-time functions are statically
// linked, as is the case with XWorkplace.
void _CRT_term(void);

/******************************************************************
 *                                                                *
 *  Helper functions                                              *
 *                                                                *
 ******************************************************************/

/*
 *@@ _DLL_InitTerm:
 *      this function gets called automatically by the OS/2 DLL
 *      during DosLoadModule processing, on the thread which
 *      invoked DosLoadModule.
 *
 *      We override this function (which is normally provided by
 *      the runtime library) to intercept this DLL's module handle.
 *
 *      Since OS/2 calls this function directly, it must have
 *      _System linkage.
 *
 *      Note: You must then link using the /NOE option, because
 *      the VAC++ runtimes also contain a _DLL_Initterm, and the
 *      linker gets in trouble otherwise.
 *      The XWorkplace makefile takes care of this.
 *
 *      This function must return 0 upon errors or 1 otherwise.
 *
 *@@changed V0.9.0 [umoeller]: reworked locale initialization
 */

unsigned long _System _DLL_InitTerm(unsigned long hModule,
                                    unsigned long ulFlag)
{
    switch (ulFlag)
    {
        case 0:
        {
            // store the DLL handle in the global variable
            G_HookData.hmodDLL = hModule;

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
    G_HookData.lCXScreen = WinQuerySysValue(HWND_DESKTOP, SV_CXSCREEN);
    G_HookData.lCYScreen = WinQuerySysValue(HWND_DESKTOP, SV_CYSCREEN);

    // PM desktop (the WPS desktop is handled by the daemon)
    G_HookData.hwndPMDesktop = WinQueryDesktopWindow(G_HookData.habDaemonObject,
                                                     NULLHANDLE);

    WinQueryWindowProcess(HWND_DESKTOP, &G_HookData.pidPM, NULL);
            // V0.9.7 (2001-01-21) [umoeller]

    // enumerate desktop window to find the window list:
    // according to PMTREE, the window list has the following
    // window hierarchy:
    //      WC_FRAME
    //        +--- WC_TITLEBAR
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
                            G_HookData.hwndWindowList = hwndThis;
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
    CHAR    szClass[100];
    HWND    hwndPrevious = NULLHANDLE;
    // climb up the parents tree until we get a frame
    while (    (hwndTemp)
            && (hwndTemp != G_HookData.hwndPMDesktop)
          )
    {
        hwndPrevious = hwndTemp;
        hwndTemp = WinQueryWindow(hwndTemp, QW_PARENT);
    }

    return (hwndPrevious);
}

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
    henum = WinBeginEnumWindows(hwndOwner);
    if (henum)
    {
        while ((hwndFound = WinGetNextWindow(henum)) != NULLHANDLE)
        {
            if (!WinIsWindow(hab,
                             hwndFound))
                // error:
                break;
            else
            {
                // query class name of found window
                if (WinQueryClassName(hwndFound, 3, szWinClass))
                {
                    // is it a scroll bar window?
                    if (strcmp(szWinClass, "#8") == 0)
                    {
                        SWP swp;

                        // is it a non-empty scroll bar?
                        if (WinQueryWindowPos(hwndFound, &swp) && swp.cx && swp.cy)
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
                        } // end if (WinQueryWindowPos(hwndFound, &swp) && ...)
                    } // end if (strcmp(szWinClass, "#8") == 0)
                }
            }
        } // end while ((hwndFound = WinGetNextWindow(henum)) != NULLHANDLE)

        // finish window enumeration
        WinEndEnumWindows(henum);
    }

    return (hwndReturn);
}

/*
 *@@ HiliteMenuItem:
 *      this un-hilites the currently hilited menu
 *      item in hwndMenu and optionally hilites the
 *      specified one instead.
 *
 *      Used during delayed sliding menu processing.
 *
 *@@added V0.9.2 (2000-02-26) [umoeller]
 *@@changed V0.9.6 (2000-10-27) [pr]: fixed some un-hilite problems; changed prototype
 */

VOID HiliteMenuItem(HWND hwndMenu,
                    SHORT sItemCount,   // in: item count in hwndMenu
                    SHORT sItemIndex,   // in: item index to hilite
                    BOOL fHiLite)       // in: to hilite or not
{
    SHORT       sItemIndex2;
    // first, un-hilite the item which currently
    // has hilite state; otherwise, we'd successively
    // hilite _all_ items in the menu... doesn't look
    // too good.
    // We cannot use a global variable for storing
    // the last menu hilite, because there can be more
    // than one open (sub)menu, and we'd get confused
    // otherwise.
    // So go thru all menu items again and un-hilite
    // the first hilited menu item we find. This will
    // be the one either PM has hilited when something
    // was selected, or the one we might have hilited
    // here previously:
    for (sItemIndex2 = 0;
         sItemIndex2 < sItemCount;
         sItemIndex2++)
    {
        SHORT sCurrentItemIdentity2
            = (SHORT)WinSendMsg(hwndMenu,
                                MM_ITEMIDFROMPOSITION,
                                MPFROMSHORT(sItemIndex2),
                                NULL);
        ULONG ulAttr
            = (ULONG)WinSendMsg(hwndMenu,
                                MM_QUERYITEMATTR,
                                MPFROM2SHORT(sCurrentItemIdentity2,
                                             FALSE), // no search submenus
                                MPFROMSHORT(0xFFFF));
        if (ulAttr & MIA_HILITED)
        {
            WinSendMsg(hwndMenu,
                       MM_SETITEMATTR,
                       MPFROM2SHORT(sCurrentItemIdentity2,
                                    FALSE), // no search submenus
                       MPFROM2SHORT(MIA_HILITED,
                                    0));    // unset attribute
            // stop second loop
            // break;
        }
    }

    if (fHiLite)
        // now hilite the new item (the one under the mouse)
        WinSendMsg(hwndMenu,
                   MM_SETITEMATTR,
                   MPFROM2SHORT(sItemIndex,
                                FALSE), // no search submenus
                   MPFROM2SHORT(MIA_HILITED, MIA_HILITED));
}

/*
 *@@ SelectMenuItem:
 *      this gets called to implement the "sliding menu" feature.
 *
 *@@added V0.9.2 (2000-02-26) [umoeller]
 *@@changed V0.9.6 (2000-10-29) [pr]: fixed submenu behavior
 */

VOID SelectMenuItem(HWND hwndMenu,
                    SHORT sItemIndex)
{
    MENUITEM    menuitemCurrent;
    USHORT      usSelItem;

    // query the current menuentry's menuitem structure
    if (WinSendMsg(hwndMenu,
                   MM_QUERYITEM,
                   MPFROM2SHORT(sItemIndex, FALSE),
                   MPFROMP(&menuitemCurrent)))
    {
        // If it's a submenu and the item we are trying to select is
        // already selected, then deselect it first - this prevents problems
        // with multiple cascading menus.
        if (menuitemCurrent.afStyle & MIS_SUBMENU)
        {
            usSelItem = (USHORT) WinSendMsg(hwndMenu,
                                            MM_QUERYSELITEMID,
                                            MPFROM2SHORT(0, FALSE),
                                            MPVOID);
            if (usSelItem == sItemIndex)
                WinSendMsg(hwndMenu,
                           MM_SELECTITEM,
                           MPFROM2SHORT(MIT_NONE, FALSE),
                           MPFROM2SHORT(0, FALSE));
        }

        WinSendMsg(hwndMenu,
                   MM_SELECTITEM,
                   MPFROM2SHORT(sItemIndex, FALSE),
                   MPFROM2SHORT(0, FALSE));
    }
}

/******************************************************************
 *                                                                *
 *  Hook interface                                                *
 *                                                                *
 ******************************************************************/

/*
 *@@ hookInit:
 *      registers (sets) all the hooks and initializes data.
 *
 *      In any case, a pointer to the DLL's static HOOKDATA
 *      structure is returned. In this struct, the caller
 *      can examine the two flags for whether the hooks
 *      were successfully installed.
 *
 *      Note: All the exported hook* interface functions must
 *      only be called by the same process, which is the
 *      XWorkplace daemon (XWPDAEMN.EXE).
 *
 *      This gets called by XWPDAEMN.EXE when
 *
 *@@changed V0.9.1 (2000-02-01) [umoeller]: fixed missing global updates
 *@@changed V0.9.2 (2000-02-21) [umoeller]: added new system hooks
 */

PHOOKDATA EXPENTRY hookInit(HWND hwndDaemonObject)  // in: daemon object window (receives notifications)
{
    APIRET arc = NO_ERROR;

    _Pmpf(("Entering hookInit"));

    if (G_hmtxGlobalHotkeys == NULLHANDLE)
        arc = DosCreateMutexSem(NULL,        // unnamed
                                &G_hmtxGlobalHotkeys,
                                DC_SEM_SHARED,
                                FALSE);      // initially unowned

    if (arc == NO_ERROR)
    {
        _Pmpf(("Storing data"));
        G_HookData.hwndDaemonObject = hwndDaemonObject;
        G_HookData.habDaemonObject = WinQueryAnchorBlock(hwndDaemonObject);

        // G_HookData.hmtxPageMage = hmtxPageMage;

        _Pmpf(("  Done storing data"));

        if (G_HookData.hmodDLL)       // initialized by _DLL_InitTerm
        {
            BOOL fSuccess = FALSE;

            _Pmpf(("hookInit: hwndCaller = 0x%lX", hwndDaemonObject));

            // initialize globals needed by the hook
            InitializeGlobalsForHooks();

            // install hooks, but only once...
            if (!G_HookData.fSendMsgHooked)
                G_HookData.fSendMsgHooked = WinSetHook(G_HookData.habDaemonObject,
                                                       NULLHANDLE, // system hook
                                                       HK_SENDMSG, // send-message hook
                                                       (PFN)hookSendMsgHook,
                                                       G_HookData.hmodDLL);

            if (!G_HookData.fLockupHooked)
                G_HookData.fLockupHooked = WinSetHook(G_HookData.habDaemonObject,
                                                      NULLHANDLE, // system hook
                                                      HK_LOCKUP,  // lockup hook
                                                      (PFN)hookLockupHook,
                                                      G_HookData.hmodDLL);

            if (!G_HookData.fInputHooked)
                G_HookData.fInputHooked = WinSetHook(G_HookData.habDaemonObject,
                                                     NULLHANDLE, // system hook
                                                     HK_INPUT,    // input hook
                                                     (PFN)hookInputHook,
                                                     G_HookData.hmodDLL);

            if (!G_HookData.fPreAccelHooked)
                G_HookData.fPreAccelHooked = WinSetHook(G_HookData.habDaemonObject,
                                                        NULLHANDLE, // system hook
                                                        HK_PREACCEL,  // pre-accelerator table hook (undocumented)
                                                        (PFN)hookPreAccelHook,
                                                        G_HookData.hmodDLL);
        }

        _Pmpf(("Leaving hookInit"));
    }

    return (&G_HookData);
}

/*
 *@@ hookKill:
 *      deregisters the hook function and frees allocated
 *      resources.
 *
 *      Note: This function must only be called by the same
 *      process which called hookInit (that is, the daemon),
 *      or resources cannot be properly freed.
 *
 *@@changed V0.9.1 (2000-02-01) [umoeller]: fixed missing global updates
 *@@changed V0.9.2 (2000-02-21) [umoeller]: added new system hooks
 *@@changed V0.9.3 (2000-04-20) [umoeller]: added function keys support
 */

BOOL EXPENTRY hookKill(void)
{
    BOOL brc = FALSE;

    _Pmpf(("hookKill"));

    if (G_HookData.fInputHooked)
    {
        WinReleaseHook(G_HookData.habDaemonObject,
                       NULLHANDLE,
                       HK_INPUT,
                       (PFN)hookInputHook,
                       G_HookData.hmodDLL);
        G_HookData.fInputHooked = FALSE;
        brc = TRUE;
    }

    if (G_HookData.fPreAccelHooked)
    {
        WinReleaseHook(G_HookData.habDaemonObject,
                       NULLHANDLE,
                       HK_PREACCEL,     // pre-accelerator table hook (undocumented)
                       (PFN)hookPreAccelHook,
                       G_HookData.hmodDLL);
        brc = TRUE;
        G_HookData.fPreAccelHooked = FALSE;
    }

    if (G_HookData.fLockupHooked)
    {
        WinReleaseHook(G_HookData.habDaemonObject,
                       NULLHANDLE,
                       HK_LOCKUP,       // lockup hook
                       (PFN)hookLockupHook,
                       G_HookData.hmodDLL);
        brc = TRUE;
        G_HookData.fLockupHooked = FALSE;
    }

    if (G_HookData.fSendMsgHooked)
    {
        WinReleaseHook(G_HookData.habDaemonObject,
                       NULLHANDLE,
                       HK_SENDMSG,       // lockup hook
                       (PFN)hookSendMsgHook,
                       G_HookData.hmodDLL);
        brc = TRUE;
        G_HookData.fSendMsgHooked = FALSE;
    }

    // free shared mem
    if (G_paGlobalHotkeys)
        DosFreeMem(G_paGlobalHotkeys);
    if (G_paFunctionKeys) // V0.9.3 (2000-04-20) [umoeller]
        DosFreeMem(G_paFunctionKeys);

    if (G_hmtxGlobalHotkeys)
    {
        DosCloseMutexSem(G_hmtxGlobalHotkeys);
        G_hmtxGlobalHotkeys = NULLHANDLE;
    }

    return (brc);
}

/*
 *@@ hookSetGlobalHotkeys:
 *      this exported function gets called to update the
 *      hook's lists of global hotkeys and XWP function keys.
 *
 *      This is the only interface to the hotkeys lists.
 *      If we wish to change any of the keys configurations,
 *      we must always pass two complete arrays of new
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
 *      hifSetFunctionKeys can be used to reconfigure the
 *      function keys.
 *
 *      This function copies the given lists into two newly
 *      allocated blocks of shared memory, which are used
 *      by the hook. If previous such blocks exist, they
 *      are freed and reallocated. Access to those blocks
 *      is protected by a mutex semaphore internally in case
 *      WM_CHAR comes in while the lists are being modified,
 *      which will cause the hook functions to be running
 *      on some application thread, while this function
 *      must only be called by the daemon.
 *
 *      Note: This function must only be called by the same
 *      process which called hookInit (that is, the
 *      daemon), because otherwise the shared memory cannot
 *      be properly freed.
 *
 *      This returns the DOS error code of the various
 *      semaphore and shared mem API calls.
 *
 *@@changed V0.9.3 (2000-04-20) [umoeller]: added function keys support; changed prototype
 */

APIRET EXPENTRY hookSetGlobalHotkeys(PGLOBALHOTKEY pNewHotkeys, // in: new hotkeys array
                                     ULONG cNewHotkeys,         // in: count of items in array -- _not_ array size!!
                                     PFUNCTIONKEY pNewFunctionKeys, // in: new hotkeys array
                                     ULONG cNewFunctionKeys)         // in: count of items in array -- _not_ array size!!
{
    BOOL    fSemOpened = FALSE,
            fSemOwned = FALSE;
    // request access to the hotkeys mutex
    APIRET arc = DosOpenMutexSem(NULL,       // unnamed
                                 &G_hmtxGlobalHotkeys);

    if (arc == NO_ERROR)
    {
        fSemOpened = TRUE;
        arc = WinRequestMutexSem(G_hmtxGlobalHotkeys,
                                 TIMEOUT_HMTX_HOTKEYS);
    }

    _Pmpf(("hookSetGlobalHotkeys: WinRequestMutexSem arc: %d", arc));

    if (arc == NO_ERROR)
    {
        fSemOwned = TRUE;

        // 1) hotkeys

        if (G_paGlobalHotkeys)
        {
            // hotkeys defined already: free the old ones
            DosFreeMem(G_paGlobalHotkeys);
        }

        arc = DosAllocSharedMem((PVOID*)(&G_paGlobalHotkeys),
                                SHMEM_HOTKEYS,
                                sizeof(GLOBALHOTKEY) * cNewHotkeys, // rounded up to 4KB
                                PAG_COMMIT | PAG_READ | PAG_WRITE);
        if (arc == NO_ERROR)
        {
            // copy hotkeys to shared memory
            memcpy(G_paGlobalHotkeys,
                   pNewHotkeys,
                   sizeof(GLOBALHOTKEY) * cNewHotkeys);
            G_cGlobalHotkeys = cNewHotkeys;
        }

        // 2) function keys V0.9.3 (2000-04-20) [umoeller]

        if (G_paFunctionKeys)
        {
            // function keys defined already: free the old ones
            DosFreeMem(G_paFunctionKeys);
        }

        arc = DosAllocSharedMem((PVOID*)(&G_paFunctionKeys),
                                SHMEM_FUNCTIONKEYS,
                                sizeof(FUNCTIONKEY) * cNewFunctionKeys, // rounded up to 4KB
                                PAG_COMMIT | PAG_READ | PAG_WRITE);

        if (arc == NO_ERROR)
        {
            // copy function keys to shared memory
            memcpy(G_paFunctionKeys,
                   pNewFunctionKeys,
                   sizeof(FUNCTIONKEY) * cNewFunctionKeys);
            G_cFunctionKeys = cNewFunctionKeys;
            _Pmpf(("hookSetGlobalHotkeys: G_cFunctionKeys = %d", G_cFunctionKeys));
        }
    }

    if (fSemOwned)
        DosReleaseMutexSem(G_hmtxGlobalHotkeys);
    if (fSemOpened)
        DosCloseMutexSem(G_hmtxGlobalHotkeys);

    return (arc);
}

/******************************************************************
 *                                                                *
 *  System-wide global variables for input hook                   *
 *                                                                *
 ******************************************************************/

HWND    G_hwndUnderMouse = NULLHANDLE;
HWND    G_hwndLastFrameUnderMouse = NULLHANDLE;
HWND    G_hwndLastSubframeUnderMouse = NULLHANDLE;
POINTS  G_ptsMousePosWin = {0};
POINTL  G_ptlMousePosDesktop = {0};

/******************************************************************
 *                                                                *
 *  Send-Message Hook                                             *
 *                                                                *
 ******************************************************************/

/*
 *@@ ProcessMsgsForPageMage:
 *      message processing which is needed for both
 *      hookInputHook and hookSendMsgHook.
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 *@@changed V0.9.4 (2000-07-10) [umoeller]: fixed float-on-top
 *@@changed V0.9.7 (2001-01-15) [dk]: WM_SETWINDOWPARAMS added
 *@@changed V0.9.7 (2001-01-18) [umoeller]: removed PGMG_LOCKUP call, pagemage doesn't need this
 *@@changed V0.9.7 (2001-01-18) [umoeller]: fixed sticky odin windows
 *@@changed V0.9.7 (2001-01-18) [umoeller]: fixed sticky EPM
 */

VOID ProcessMsgsForPageMage(HWND hwnd,
                            ULONG msg,
                            MPARAM mp1,
                            MPARAM mp2)
{
    // first check, just for speed
    if (    (msg == WM_CREATE)
         || (msg == WM_DESTROY)
         || (msg == WM_ACTIVATE)
         || (msg == WM_WINDOWPOSCHANGED)
         // respect WM_SETWINDOWPARAMS only if text has changed
         || (   (msg == WM_SETWINDOWPARAMS)
             && (((PWNDPARAMS)mp1)->fsStatus & WPM_TEXT) // 0x0001
            )
       )
    {
        if (    (WinQueryWindow(hwnd, QW_PARENT) == G_HookData.hwndPMDesktop)
             && (hwnd != G_HookData.hwndPageMageFrame)
                    // V0.9.7 (2001-01-23) [umoeller]
           )
        {
            CHAR    szClass[200];

            if (WinQueryClassName(hwnd, sizeof(szClass), szClass))
            {
                if (    (strcmp(szClass, "#1") == 0)
                     || (strcmp(szClass, "wpFolder window") == 0)
                     || (strcmp(szClass, "Win32FrameClass") == 0)
                                // that's for Odin V0.9.7 (2001-01-18) [umoeller]
                     || (strcmp(szClass, "EFrame") == 0)
                                // that's for EPM V0.9.7 (2001-01-19) [dk]
                   )
                {
                    // window creation/destruction:

                    switch (msg)
                    {
                        case WM_CREATE:
                        case WM_DESTROY:
                        case WM_SETWINDOWPARAMS:
                            WinPostMsg(G_HookData.hwndPageMageClient,
                                       PGMG_WNDCHANGE,
                                       MPFROMHWND(hwnd),
                                       MPFROMLONG(msg));
                        break;

                        case WM_ACTIVATE:
                            if (mp1)        // window being activated:
                            {
                                // it's a top-level window:
                                WinPostMsg(G_HookData.hwndPageMageClient,
                                           PGMG_INVALIDATECLIENT,
                                           (MPARAM)FALSE,   // delayed
                                           0);
                                WinPostMsg(G_HookData.hwndPageMageMoveThread,
                                           PGOM_FOCUSCHANGE,
                                           0,
                                           0);
                            }
                        break;

                        case WM_WINDOWPOSCHANGED:
                            WinPostMsg(G_HookData.hwndPageMageClient,
                                       PGMG_INVALIDATECLIENT,
                                       (MPARAM)FALSE,   // delayed
                                       0);
                        break;
                    }
                } // end if (    (strcmp(szClass, ...
            } // end if (WinQueryClassName(hwnd, sizeof(szClass), szClass))
        } // end if (WinQueryWindow(hwnd, QW_PARENT) == HookData.hwndPMDesktop)
    }

    if (    (msg == WM_DESTROY)
         && (hwnd == G_HookData.hwndLockupFrame)
       )
    {
        // current lockup frame being destroyed
        // (system is being unlocked):
        G_HookData.hwndLockupFrame = NULLHANDLE;
        /* WinPostMsg(G_HookData.hwndPageMageClient,
                   PGMG_LOCKUP,
                   MPFROMLONG(FALSE),
                   MPVOID); */
            // removed V0.9.7 (2001-01-18) [umoeller], PageMage doesn't
            // need this
    }
}

/*
 *@@ hookSendMsgHook:
 *      send-message hook.
 *
 *      We must not do any complex processing in here, especially
 *      calling WinSendMsg(). Instead, we post msgs to other places,
 *      because sending messages will recurse into this hook forever.
 *
 *      Be warned that many PM API functions send messages also.
 *      This applies especially to WinSetWindowPos and such.
 *      They are a no-no in here because they would lead to
 *      infinite recursion also.
 *
 *      SMHSTRUCT is defined as follows:
 *
 *          typedef struct _SMHSTRUCT {
 *            MPARAM     mp2;
 *            MPARAM     mp1;
 *            ULONG      msg;
 *            HWND       hwnd;
 *            ULONG      model;  //  Message identity ?!?
 *          } SMHSTRUCT;
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 *@@changed V0.9.9 (2001-03-10) [umoeller]: fixed errant sliding menu behavior
 */

VOID EXPENTRY hookSendMsgHook(HAB hab,
                              PSMHSTRUCT psmh,
                              BOOL fInterTask)
{
    if (    // PageMage running?
            (G_HookData.hwndPageMageFrame)
            // switching not disabled?
         && (!G_HookData.fDisableSwitching)
                // this flag is set frequently when PageMage
                // is doing tricky stuff; we must not process
                // messages then, or we'll recurse forever
       )
    {
        // OK, go ahead:
        ProcessMsgsForPageMage(psmh->hwnd,
                               psmh->msg,
                               psmh->mp1,
                               psmh->mp2);

        // V0.9.7 (2001-01-23) [umoeller]
        if (    (G_HookData.PageMageConfig.fStayOnTop)
             // && (psmh->msg == WM_ADJUSTWINDOWPOS)    doesn't work
             && (psmh->msg == WM_WINDOWPOSCHANGED)
             && (WinIsWindowVisible(G_HookData.hwndPageMageFrame))
           )
        {
            PSWP pswp = (PSWP)psmh->mp1;
            if (    (pswp)
                 // && (pswp->fl & SWP_ZORDER)
                 && (pswp->hwndInsertBehind == HWND_TOP)
                 && (WinQueryWindow(psmh->hwnd, QW_PARENT) == G_HookData.hwndPMDesktop)
               )
            {
                // hack this to move behind PageMage frame
                // G_HookData.fDisableSwitching = TRUE;
                WinSetWindowPos(G_HookData.hwndPageMageFrame,
                                HWND_TOP,
                                0, 0, 0, 0,
                                SWP_ZORDER | SWP_SHOW);
                // G_HookData.fDisableSwitching = FALSE;
            }
        }
    }

    // special case check... if a menu control has been hidden
    // and it is the menu for which we have a sliding menu
    // timer running in the daemon, stop that timer NOW
    // V0.9.9 (2001-03-10) [umoeller]
    // this happens if the user quickly selects a menu item
    // by clicking on it before the delayed-select timer has
    // elapsed
    if (    (psmh->msg == WM_WINDOWPOSCHANGED)          // comes in for menu hide
            // sliding menus enabled?
         && (G_HookData.HookConfig.fSlidingMenus)
            // delay enabled?
         && (G_HookData.HookConfig.ulSubmenuDelay)
            // changing window is the menu control that we
            // previously started a timer for?
         && (psmh->hwnd == G_HookData.hwndMenuUnderMouse)
            // menu control is hiding?
         && ((PSWP)(psmh->mp1))
         && (((PSWP)(psmh->mp1))->fl & SWP_HIDE)
       )
    {
        // yes: stop the delayed-select timer NOW
        WinPostMsg(G_HookData.hwndDaemonObject,
                   XDM_SLIDINGMENU,
                   (MPARAM)-1,          // stop timer
                   0);
    }
}

/*
 *@@ hookLockupHook:
 *
 *@@added V0.9.2 (2000-02-21) [umoeller]
 *@@changed V0.9.6 (2000-11-05) [pr]: fix for hotkeys not working after Lockup
 *@@changed V0.9.7 (2001-01-18) [umoeller]: removed PGMG_LOCKUP call, pagemage doesn't need this
 */

VOID EXPENTRY hookLockupHook(HAB hab,
                             HWND hwndLocalLockupFrame)
{
    if (G_HookData.hwndPageMageFrame)
    {
        G_HookData.hwndLockupFrame = hwndLocalLockupFrame;
        /* WinPostMsg(G_HookData.hwndPageMageClient,
                   PGMG_LOCKUP,
                   MPFROMLONG(TRUE),
                   MPVOID); */
            // removed V0.9.7 (2001-01-18) [umoeller], PageMage doesn't
            // need this
    }
    /* G_HookData.hwndLockupFrame = hwndLocalLockupFrame;
    WinPostMsg(G_HookData.hwndPageMageClient,
               PGMG_LOCKUP,
               MPFROMLONG(TRUE),
               MPVOID); */
}

/******************************************************************
 *                                                                *
 *  Input hook -- WM_MOUSEMOVE processing                         *
 *                                                                *
 ******************************************************************/

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
 *      Based on code from WarpEnhancer, (C) Achim HasenmÅller.
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
                        lrc = lrc
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
    LONG    lScrollBarSizePels = CalcScrollBarSize(pScrollData,
                                                   fHorizontal);

    if (lScrollBarSizePels)
    {
        // get scroll bar range
        MRESULT mrThumbRange = WinSendMsg(pScrollData->hwndScrollBar,
                                          SBM_QUERYRANGE, 0, 0);
        if (mrThumbRange)
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
                lPerMilleMoved = (lDeltaPels * 1000L)
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
 *      Initially based on code from WarpEnhancer, (C) Achim HasenmÅller,
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
        pScrollData->hwndScrollBar = GetScrollBar(hwnd,
                                                  fHorizontal);
        if (pScrollData->hwndScrollBar == NULLHANDLE)
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

            pScrollData->hwndScrollLastOwner = WinQueryWindow(pScrollData->hwndScrollBar,
                                                              QW_OWNER);
            if (pScrollData->hwndScrollLastOwner)
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
            pScrollData->hwndScrollLastOwner = WinQueryWindow(pScrollData->hwndScrollBar,
                                                              QW_OWNER);
            if (pScrollData->hwndScrollLastOwner)
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

/*
 *@@ WMMouseMove_SlidingFocus:
 *      this gets called when hookInputHook intercepts
 *      WM_MOUSEMOVE to do the "sliding focus" processing.
 *
 *      This function evaluates hwnd to find out whether
 *      the mouse has moved over a new frame window.
 *      If so, XDM_SLIDINGFOCUS is posted to the daemon's
 *      object window, which will then do the actual focus
 *      and active window processing (starting a timer, if
 *      delayed focus is active).
 *
 *      Processing is fairly complicated because special
 *      case checks are needed for certain window classes
 *      (combo boxes, Win-OS/2 windows). Basically, this
 *      function finds the frame window to which hwnd
 *      (the window under the mouse, from the hook) belongs
 *      and passes that frame to the daemon. If another
 *      frame is found between the main frame and hwnd,
 *      that frame is passed to the daemon also.
 *
 *      Frame activation is then not done from the hook, but
 *      from the daemon instead.
 *
 *      Inspired by code from ProgramCommander (W) Roman Stangl.
 *      Fixed a few problems with MDI frames.
 *
 *@@changed V0.9.3 (2000-05-22) [umoeller]: fixed combobox problems
 *@@changed V0.9.3 (2000-05-22) [umoeller]: fixed MDI frames problems
 *@@changed V0.9.4 (2000-06-12) [umoeller]: fixed Win-OS/2 menu problems
 *@@changed V0.9.9 (2001-03-14) [lafaix]: disabling sliding when processing mouse switch
 */

VOID WMMouseMove_SlidingFocus(HWND hwnd,        // in: wnd under mouse, from hookInputHook
                              BOOL fMouseMoved, // in: TRUE if mouse has been moved since previous WM_MOUSEMOVE
                                                // (determined by hookInputHook)
                              PSZ pszClassUnderMouse) // in: window class of hwnd (determined
                                                      // by hookInputHook)
{
    BOOL    fStopTimers = FALSE;    // setting this to TRUE will stop timers

    if (G_HookData.fDisableMouseSwitch)
        return;

    do      // just a do for breaking, no loop
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
            if (hwndActiveNow != G_HookData.hwndActivatedByUs)
            {
                // active window is not the one we set active:
                // this probably means that some new
                // window popped up which we haven't noticed
                // and was explicitly made active either by the
                // shell or by an application, so we use this
                // for the below checks. Otherwise, sliding focus
                // would be disabled after a new window has popped
                // up until the mouse was moved over a new frame window.
                G_hwndLastFrameUnderMouse = hwndActiveNow;
                G_hwndLastSubframeUnderMouse = NULLHANDLE;
                G_HookData.hwndActivatedByUs = hwndActiveNow;

                fStopTimers = TRUE;
                        // this will be overridden if we start a new
                        // timer below; but just in case an old timer
                        // might still be running V0.9.4 (2000-08-03) [umoeller]
            }
        }

        if (fMouseMoved)            // has mouse really moved?
        {
            // OK:
            HWND    hwndDesktopChild = hwnd,
                    hwndTempParent = NULLHANDLE,
                    hwndFrameInBetween = NULLHANDLE;
            HWND    hwndFocusNow = WinQueryFocus(HWND_DESKTOP);
            CHAR    szClassName[200],
                    szWindowText[200];

            // check 2: make sure mouse is not captured
            if (WinQueryCapture(HWND_DESKTOP) != NULLHANDLE)
            {
                // stop timers and quit
                fStopTimers = TRUE;
                break;
            }

            // check 3: quit if menu has the focus
            if (hwndFocusNow)
            {
                CHAR    szFocusClass[MAXNAMEL+4] = "";
                WinQueryClassName(hwndFocusNow,
                                  sizeof(szFocusClass),
                                  szFocusClass);

                if (strcmp(szFocusClass, "#4") == 0)
                {
                    // menu:
                    // stop timers and quit
                    fStopTimers = TRUE;
                    break;
                }
            }

            // now climb up the parent window hierarchy of the
            // window under the mouse (first: hwndDesktopChild)
            // until we reach the Desktop; the last window we
            // had is then in hwndDesktopChild
            hwndTempParent = WinQueryWindow(hwndDesktopChild, QW_PARENT);
            while (     (hwndTempParent != G_HookData.hwndPMDesktop)
                     && (hwndTempParent != NULLHANDLE)
                  )
            {
                WinQueryClassName(hwndDesktopChild,
                                  sizeof(szClassName), szClassName);
                if (strcmp(szClassName, "#1") == 0)
                    // it's a frame:
                    hwndFrameInBetween = hwndDesktopChild;
                hwndDesktopChild = hwndTempParent;
                hwndTempParent = WinQueryWindow(hwndDesktopChild, QW_PARENT);
            }

            if (hwndFrameInBetween == hwndDesktopChild)
                hwndFrameInBetween = NULLHANDLE;

            // hwndDesktopChild now has the window which we need to activate
            // (the topmost parent under the desktop of the window under the mouse)

            WinQueryClassName(hwndDesktopChild,
                              sizeof(szClassName), szClassName);

            // check 4: skip certain window classes

            if (    (!strcmp(szClassName, "#4"))
                            // menu
                 || (!strcmp(szClassName, "#7"))
                         // listbox: as a desktop child, this must be a
                         // a drop-down box which is currently open
               )
            {
                // stop timers and quit
                fStopTimers = TRUE;
                break;
            }

            WinQueryWindowText(hwndDesktopChild, sizeof(szWindowText), szWindowText);
            if (strstr(szWindowText, "Seamless"))
                // ignore seamless Win-OS/2 menus; these are separate windows!
            {
                // stop timers and quit
                fStopTimers = TRUE;
                break;
            }

            // OK, enough checks.
            // Now let's do the sliding focus if
            // 1) the desktop window (hwndDesktopChild, highest parent) changed or
            // 2) if hwndDesktopChild has several subframes and the subframe changed:

            if (hwndDesktopChild)
                if (    (hwndDesktopChild != G_hwndLastFrameUnderMouse)
                     || (   (hwndFrameInBetween != NULLHANDLE)
                         && (hwndFrameInBetween != G_hwndLastSubframeUnderMouse)
                        )
                   )
                {
                    // OK, mouse moved to a new desktop window:
                    // store that for next time
                    G_hwndLastFrameUnderMouse = hwndDesktopChild;
                    G_hwndLastSubframeUnderMouse = hwndFrameInBetween;

                    // notify daemon of the change;
                    // it is the daemon which does the rest
                    // (timer handling, window activation etc.)
                    WinPostMsg(G_HookData.hwndDaemonObject,
                               XDM_SLIDINGFOCUS,
                               (MPARAM)hwndFrameInBetween,  // can be NULLHANDLE
                               (MPARAM)hwndDesktopChild);
                    fStopTimers = FALSE;
                }
        }
    } while (FALSE); // end do

    if (fStopTimers)
        WinPostMsg(G_HookData.hwndDaemonObject,
                   XDM_SLIDINGFOCUS,
                   (MPARAM)NULLHANDLE,
                   (MPARAM)NULLHANDLE);     // stop timers
}

/*
 *@@ WMMouseMove_SlidingMenus:
 *      this gets called when hookInputHook intercepts
 *      WM_MOUSEMOVE and the window under the mouse is
 *      a menu control to do automatic menu selection.
 *
 *      This implementation is slightly tricky. ;-)
 *      We cannot query the MENUITEM data of a menu
 *      which does not belong to the calling process
 *      because this message needs access to the caller's
 *      storage. I have tried this, this causes every
 *      application to crash as soon as a menu item
 *      gets selected.
 *
 *      Fortunately, Roman Stangl has found a cool way
 *      of doing this, so I could build on that.
 *
 *      So now we start a timer in the daemon which will
 *      post a special WM_MOUSEMOVE message back to us
 *      which can only be received by us
 *      (with the same HWND and MP1 as the original
 *      WM_MOUSEMOVE, but a special flag in MP2). When
 *      that special message comes in, we can select
 *      the menu item.
 *
 *      In addition to Roman's code, we always change the
 *      MIA_HILITE attribute of the current menu item
 *      under the mouse, so the menu hilite follows the
 *      mouse immediately, even though submenus may be
 *      opened delayed.
 *
 *      We return TRUE if the msg is to be swallowed.
 *
 *      Inspired by code from ProgramCommander (W) Roman Stangl.
 *
 *@@added V0.9.2 (2000-02-26) [umoeller]
 *@@changed V0.9.6 (2000-10-27) [pr]: fixed un-hilite problems
 *@@changed V0.9.6 (2000-10-27) [umoeller]: added optional NPSWPS-like submenu behavior
 *@@changed V0.9.9 (2001-03-10) [umoeller]: fixed cc sensitivity and various selection issues
 */

BOOL WMMouseMove_SlidingMenus(HWND hwndCurrentMenu,  // in: menu wnd under mouse, from hookInputHook
                              MPARAM mp1,
                              MPARAM mp2)
{
    static SHORT    sLastHiliteID = MIT_NONE,
                    sLastSelectID = MIT_NONE;

    BOOL            brc = FALSE;        // per default, don't swallow msg

    // check for special message which was posted from
    // the daemon when the "delayed sliding menu" timer
    // elapsed:
    if (    // current mp1 the same as timer mp1?
            (mp1 == G_HookData.mpDelayedSlidingMenuMp1)
            // special code from daemon in mp2?
         && (SHORT1FROMMP(mp2) == HT_DELAYEDSLIDINGMENU)
       )
    {
        // yes, special message:

        // get current window under mouse pointer; we
        // must ignore the timer msg if the user has
        // moved the mouse away from the menu since
        // the timer was started
        HWND    hwndUnderMouse = WinWindowFromPoint(HWND_DESKTOP,
                                                    &G_ptlMousePosDesktop,
                                                    TRUE);  // enum desktop children
        CHAR    szClassUnderMouse[100];
        if (WinQueryClassName(hwndUnderMouse, sizeof(szClassUnderMouse), szClassUnderMouse))
        {
            if (strcmp(szClassUnderMouse, "#4") == 0)
            {
                if (    // timer menu hwnd same as last menu under mouse?
                        (hwndCurrentMenu == G_HookData.hwndMenuUnderMouse)
                        // last WM_MOUSEMOVE hwnd same as last menu under mouse?
                     && (G_hwndUnderMouse == G_HookData.hwndMenuUnderMouse)
                        // timer menu hwnd same as current window under mouse?
                     && (hwndUnderMouse == G_HookData.hwndMenuUnderMouse)
                    )
                {
                    // check if the menu still exists
                    // if (WinIsWindow(hwndCurrentMenu))
                    // and if it's visible; the menu
                    // might have been hidden by now because
                    // the user has already proceeded to
                    // another submenu
                    if (WinIsWindowVisible(hwndCurrentMenu))
                    {
                        // OK:
                        brc = TRUE;
                    }
                }
            }
        }

        if (brc)
        {
            if (sLastSelectID != G_HookData.sMenuItemUnderMouse)
            {
                // select menu item under the mouse
                SelectMenuItem(hwndCurrentMenu,
                               G_HookData.sMenuItemUnderMouse);
                               // stored from last run
                               // when timer was started...
                sLastSelectID = G_HookData.sMenuItemUnderMouse;
            }
        }
        else
        {
            if (G_HookData.hwndMenuUnderMouse)
            {
                // remove previously hilited but not selected entry,
                // as it is unknown to PM
                SHORT   sItemCount
                    = (SHORT)WinSendMsg(G_HookData.hwndMenuUnderMouse,
                                        MM_QUERYITEMCOUNT,
                                        0, 0);
                HiliteMenuItem(G_HookData.hwndMenuUnderMouse,
                               sItemCount,
                               G_HookData.sMenuItemUnderMouse,
                               FALSE);
                G_HookData.hwndMenuUnderMouse = 0;
                sLastHiliteID = MIT_NONE;
            }
        }
    }
    else
    {
        // no, regular WM_MOUSEMOVE:

        // check if anything is currently selected in the menu
        // (this rules out main menu bars if user hasn't clicked
        // into them yet)
        SHORT   sCurrentlySelectedItem
            = (SHORT)WinSendMsg(hwndCurrentMenu,
                                MM_QUERYSELITEMID,
                                MPFROMLONG(FALSE),
                                NULL);

        if (sCurrentlySelectedItem == MIT_NONE)
        {
            sLastHiliteID = MIT_NONE;
            sLastSelectID = MIT_NONE;
        }
        else
        {
            // we have a selection:

            SHORT   sItemCount = (SHORT)WinSendMsg(hwndCurrentMenu,
                                                   MM_QUERYITEMCOUNT,
                                                   0, 0),
                    sItemIndex = 0;

            // loop through all items in the current menu
            // and query each item's rectangle
            for (sItemIndex = 0;
                 sItemIndex < sItemCount;
                 sItemIndex++)
            {
                RECTL       rectlItem;
                LONG        rectLimit;

                SHORT sCurrentItemIdentity = (SHORT)WinSendMsg(hwndCurrentMenu,
                                                               MM_ITEMIDFROMPOSITION,
                                                               MPFROMSHORT(sItemIndex),
                                                               NULL);
                // as MIT_ERROR == MIT_END == MIT_NONE == MIT_MEMERROR
                // and may also be used for separators, just ignore this menuentries
                if (sCurrentItemIdentity == MIT_ERROR)
                    continue;

                // get the menuentry's rectangle to test if it covers the
                // current mouse pointer position
                WinSendMsg(hwndCurrentMenu,
                           MM_QUERYITEMRECT,
                           MPFROM2SHORT(sCurrentItemIdentity,
                                        FALSE),
                           MPFROMP(&rectlItem));

                // standard sensitivity area: entire menu item from the left
                rectLimit = rectlItem.xLeft;

                // V0.9.9 (2001-03-10) [umoeller]: moved "conditional casc." stuff down

                if (    (G_ptsMousePosWin.x > rectLimit)
                     && (G_ptsMousePosWin.x <= rectlItem.xRight)
                     && (G_ptsMousePosWin.y > rectlItem.yBottom)
                     && (G_ptsMousePosWin.y <= rectlItem.yTop)
                   )
                {
                    BOOL    fSelect = TRUE;

                    // do extra checks if "cc sensitivity" on;
                    // moved this here V0.9.9 (2001-03-10) [umoeller]
                    // so that we can always hilite, but we do not always
                    // select
                    if (G_HookData.HookConfig.fConditionalCascadeSensitive)
                    {
                        // check if the pointer position is within the item's
                        // rectangle or just the right hand half if it is a popup
                        // menu and has conditional cascade submenus
                        MENUITEM    menuitemCurrent;
                        if (WinSendMsg(hwndCurrentMenu,
                                       MM_QUERYITEM,
                                       MPFROM2SHORT(sCurrentItemIdentity, FALSE),
                                       MPFROMP(&menuitemCurrent)))
                        {
                            if (   (menuitemCurrent.afStyle & MIS_SUBMENU)
                                && (WinQueryWindow(hwndCurrentMenu, QW_PARENT)
                                        == G_HookData.hwndPMDesktop)
                               )
                            {
                                ULONG ulStyle =
                                    WinQueryWindowULong(menuitemCurrent.hwndSubMenu,
                                                        QWL_STYLE);
                                if (ulStyle & MS_CONDITIONALCASCADE)
                                    // is "cc" menu:
                                    // select only if mouse is on the right
                                    // (approximately above the cc button)
                                    if (G_ptsMousePosWin.x < rectlItem.xRight - 20)
                                        // not over button: do not select then
                                        fSelect = FALSE;
                            }
                        }
                    }

                    // do we have a submenu delay?
                    if (G_HookData.HookConfig.ulSubmenuDelay)
                    {
                        // delayed:
                        // this is a three-step process:
                        // 1)  If we used MM_SELECTITEM on the item, this
                        //     would immediately open the subwindow (if the
                        //     item represents a submenu).
                        //     So instead, we first need to manually change
                        //     the hilite attribute of the menu item under
                        //     the mouse so that the item under the mouse is
                        //     always immediately hilited (without being
                        //     "selected"; PM doesn't know what we're doing here!)

                        // V0.9.9 (2001-03-10) [umoeller]
                        // Note that we even hilite the menu item for
                        // conditional cascade submenus if "cc sensitivity"
                        // is on. We only differentiate for _selection_ below now.

                        // has item changed since last time?
                        if (sLastHiliteID != sCurrentItemIdentity)
                            if (G_HookData.HookConfig.fMenuImmediateHilite)
                            {
                                HiliteMenuItem(hwndCurrentMenu,
                                               sItemCount,
                                               sCurrentItemIdentity,
                                               TRUE);
                                sLastHiliteID = sCurrentItemIdentity;
                            }

                        // 2)  We then post the daemon a message to start
                        //     a timer. Before that, we store the menu item
                        //     data in HOOKDATA so we can re-use it when
                        //     the timer elapses.

                        if (fSelect) // V0.9.9 (2001-03-10) [umoeller]
                        {
                            // prepare data for delayed selection:
                            // when the special WM_MOUSEMOVE comes in,
                            // we check against all these.
                            // a) store mp1 for comparison later
                            G_HookData.mpDelayedSlidingMenuMp1 = mp1;
                            // b) store menu
                            G_HookData.hwndMenuUnderMouse = hwndCurrentMenu;
                            // c) store menu item
                            G_HookData.sMenuItemUnderMouse = sCurrentItemIdentity;
                            // d) notify daemon of the change, which
                            // will start the timer and post WM_MOUSEMOVE
                            // back to us
                            WinPostMsg(G_HookData.hwndDaemonObject,
                                       XDM_SLIDINGMENU,
                                       mp1,
                                       0);

                            // 3)  When the timer elapses, the daemon posts a special
                            //     WM_MOUSEMOVE to the same menu control for which
                            //     the timer was started. See the "special message"
                            //     processing on top. We then immediately select
                            //     the menu item.
                        }
                        else
                            // do not select: we must stop the
                            // timer then in case it is still running
                            // V0.9.9 (2001-03-10) [umoeller]
                            WinPostMsg(G_HookData.hwndDaemonObject,
                                       XDM_SLIDINGMENU,
                                       (MPARAM)-1, // stop timer
                                       0);
                    } // end if (HookData.HookConfig.ulSubmenuDelay)
                    else
                        // no delay, but immediately:
                        if (sLastSelectID != sCurrentItemIdentity)
                            if (fSelect)
                            {
                                SelectMenuItem(hwndCurrentMenu,
                                               sCurrentItemIdentity);
                                sLastSelectID = sCurrentItemIdentity;
                            }

                    break;
                }
            } // end for (sItemIndex = 0; ...
        }
    }

    return (brc);
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
 *@@changed V0.9.5 (2000-09-20) [pr]: fixed auto-hide bug
 */

VOID WMMouseMove_AutoHideMouse(VOID)
{
    // is the timer currently running?
    if (G_HookData.idAutoHideTimer != NULLHANDLE)
    {
        // stop the running async timer
        WinStopTimer(G_HookData.habDaemonObject,
                     G_HookData.hwndDaemonObject,
                     G_HookData.idAutoHideTimer);
        G_HookData.idAutoHideTimer = NULLHANDLE;
    }

    // show the mouse pointer now
    if (G_HookData.fMousePointerHidden)
    {
        WinShowPointer(HWND_DESKTOP, TRUE);
        G_HookData.fMousePointerHidden = FALSE;
    }

    // (re)start timer
    if (G_HookData.HookConfig.fAutoHideMouse) // V0.9.5 (2000-09-20) [pr] fix auto-hide mouse bug
        G_HookData.idAutoHideTimer =
            WinStartTimer(G_HookData.habDaemonObject,
                          G_HookData.hwndDaemonObject,
                          TIMERID_AUTOHIDEMOUSE,
                          (G_HookData.HookConfig.ulAutoHideDelay + 1) * 1000);
}

/*
 *@@ WMMouseMove:
 *
 *@@added V0.9.3 (2000-04-30) [umoeller]
 *@@changed V0.9.3 (2000-04-30) [umoeller]: pointer was hidden while MB3-dragging; fixed
 *@@changed V0.9.4 (2000-08-03) [umoeller]: fixed sliding menus without mouse moving
 *@@changed V0.9.5 (2000-08-22) [umoeller]: sliding focus was always on, working half way; fixed
 *@@changed V0.9.5 (2000-09-20) [pr]: fixed auto-hide bug
 *@@changed V0.9.9 (2001-03-15) [lafaix]: now uses corner sensitivity
 *@@changed V0.9.9 (2001-03-15) [lafaix]: added AutoScroll support
 */

BOOL WMMouseMove(PQMSG pqmsg,
                 PBOOL pfRestartAutoHide)      // out: set to TRUE if auto-hide must be processed
{
    BOOL    brc = FALSE;        // swallow?
    BOOL    fGlobalMouseMoved = FALSE,
            fWinChanged = FALSE;

    do // allow break's
    {
        HWND    hwndCaptured = WinQueryCapture(HWND_DESKTOP);

        // store mouse pos in win coords
        G_ptsMousePosWin.x = SHORT1FROMMP(pqmsg->mp1);
        G_ptsMousePosWin.y = SHORT2FROMMP(pqmsg->mp1);

        // has position changed since last WM_MOUSEMOVE?
        // PM keeps posting WM_MOUSEMOVE even if the
        // mouse hasn't moved, so we can drop unnecessary
        // processing...
        if (G_ptlMousePosDesktop.x != pqmsg->ptl.x)
        {
            // store x mouse pos in Desktop coords
            G_ptlMousePosDesktop.x = pqmsg->ptl.x;
            fGlobalMouseMoved = TRUE;
        }
        if (G_ptlMousePosDesktop.y != pqmsg->ptl.y)
        {
            // store y mouse pos in Desktop coords
            G_ptlMousePosDesktop.y = pqmsg->ptl.y;
            fGlobalMouseMoved = TRUE;
        }
        if (pqmsg->hwnd != G_hwndUnderMouse)
        {
            // mouse has moved to a different window:
            // this can happen even if the coordinates
            // are the same, because PM posts WM_MOUSEMOVE
            // with changing window positions also to
            // allow an application to change pointers
            G_hwndUnderMouse = pqmsg->hwnd;
            fWinChanged = TRUE;
        }

        if (    (fGlobalMouseMoved)
             || (fWinChanged)
           )
        {
            BYTE    bHotCorner = 0;

            /*
             * MB3 scroll
             *
             */

            // are we currently in scrolling mode
            if (    (    (G_HookData.HookConfig.fMB3Scroll)
                      || (G_HookData.bAutoScroll)
                    )
                 && (G_HookData.hwndCurrentlyScrolling)
               )
            {
                // simulate mouse capturing by passing the scrolling
                // window, no matter what hwnd came in with WM_MOUSEMOVE
                brc = WMMouseMove_MB3Scroll(G_HookData.hwndCurrentlyScrolling);
                break;  // skip all the rest
            }

            // make sure that the mouse is not currently captured
            if (hwndCaptured == NULLHANDLE)
            {
                CHAR    szClassUnderMouse[200];

                WinQueryClassName(pqmsg->hwnd,
                                  sizeof(szClassUnderMouse),
                                  szClassUnderMouse);

                /*
                 * sliding focus:
                 *
                 */

                if (G_HookData.HookConfig.fSlidingFocus)
                {
                    // sliding focus enabled?
                    // V0.9.5 (2000-08-22) [umoeller]
                    WMMouseMove_SlidingFocus(pqmsg->hwnd,
                                             fGlobalMouseMoved,
                                             szClassUnderMouse);
                }

                if (fGlobalMouseMoved)
                {
                    // only if mouse has moved, not
                    // on window change:

                    /*
                     * hot corners:
                     *
                     *changed V0.9.9 (2001-03-15) [lafaix]: now uses corner sensitivity
                     */

                    // check if mouse is in one of the screen
                    // corners
                    if (G_ptlMousePosDesktop.x == 0)
                    {
                        if (G_ptlMousePosDesktop.y == 0)
                            // lower left corner:
                            bHotCorner = 1;
                        else if (G_ptlMousePosDesktop.y == G_HookData.lCYScreen - 1)
                            // top left corner:
                            bHotCorner = 2;
                        // or maybe left screen border:
                        // make sure mouse y is in the middle third of the screen
                        else if (    (G_ptlMousePosDesktop.y >= G_HookData.lCYScreen * G_HookData.HookConfig.ulCornerSensitivity / 100)
                                  && (G_ptlMousePosDesktop.y <= G_HookData.lCYScreen * (100 - G_HookData.HookConfig.ulCornerSensitivity) / 100)
                                )
                            bHotCorner = 6; // left border
                    }
                    else if (G_ptlMousePosDesktop.x == G_HookData.lCXScreen - 1)
                    {
                        if (G_ptlMousePosDesktop.y == 0)
                            // lower right corner:
                            bHotCorner = 3;
                        else if (G_ptlMousePosDesktop.y == G_HookData.lCYScreen - 1)
                            // top right corner:
                            bHotCorner = 4;
                        // or maybe right screen border:
                        // make sure mouse y is in the middle third of the screen
                        else if (    (G_ptlMousePosDesktop.y >= G_HookData.lCYScreen * G_HookData.HookConfig.ulCornerSensitivity / 100)
                                  && (G_ptlMousePosDesktop.y <= G_HookData.lCYScreen * (100 - G_HookData.HookConfig.ulCornerSensitivity) / 100)
                                )
                            bHotCorner = 7; // right border
                    }
                    else
                        // more checks for top and bottom screen border:
                        if (    (G_ptlMousePosDesktop.y == 0)   // bottom
                             || (G_ptlMousePosDesktop.y == G_HookData.lCYScreen - 1) // top
                           )
                        {
                            if (    (G_ptlMousePosDesktop.x >= G_HookData.lCXScreen * G_HookData.HookConfig.ulCornerSensitivity / 100)
                                 && (G_ptlMousePosDesktop.x <= G_HookData.lCXScreen * (100 - G_HookData.HookConfig.ulCornerSensitivity) / 100)
                               )
                                if (G_ptlMousePosDesktop.y == 0)
                                    // bottom border:
                                    bHotCorner = 8;
                                else
                                    // top border:
                                    bHotCorner = 5;
                        }

                    // is mouse in a screen corner?
                    if (bHotCorner != 0)
                        // yes:
                        // notify thread-1 object window, which
                        // will start the user-configured action
                        // (if any)
                        WinPostMsg(G_HookData.hwndDaemonObject,
                                   XDM_HOTCORNER,
                                   (MPARAM)bHotCorner,
                                   (MPARAM)NULL);

                    /*
                     * sliding menus:
                     *    only if mouse has moved globally
                     *    V0.9.4 (2000-08-03) [umoeller]
                     */

                    if (G_HookData.HookConfig.fSlidingMenus)
                        if (strcmp(szClassUnderMouse, "#4") == 0)
                            // window under mouse is a menu:
                            WMMouseMove_SlidingMenus(pqmsg->hwnd,
                                                     pqmsg->mp1,
                                                     pqmsg->mp2);
                } // end if (fMouseMoved)

            } // if (WinQueryCapture(HWND_DESKTOP) == NULLHANDLE)
        } // end if (fMouseMoved)
    } while (FALSE);

    if (fGlobalMouseMoved)
    {
        /*
         * auto-hide pointer:
         *
         */

        // V0.9.9 (2001-01-29) [umoeller]
        *pfRestartAutoHide = TRUE;
    }

    return (brc);
}

/******************************************************************
 *                                                                *
 *  Input hook -- miscellaneous processing                        *
 *                                                                *
 ******************************************************************/

/*
 *@@ WMButton_SystemMenuContext:
 *      this gets called from hookInputHook upon
 *      MB2 clicks to copy a window's system menu
 *      and display it as a popup menu on the
 *      title bar.
 *
 *      Based on code from WarpEnhancer, (C) Achim HasenmÅller.
 */

VOID WMButton_SystemMenuContext(HWND hwnd)     // of WM_BUTTON2CLICK
{
    POINTL      ptlMouse; // mouse coordinates
    HWND        hwndFrame; // handle of the frame window (parent)

    // get mouse coordinates (absolute coordinates)
    WinQueryPointerPos(HWND_DESKTOP, &ptlMouse);
    // query parent of title bar window (frame window)
    hwndFrame = WinQueryWindow(hwnd, QW_PARENT);
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
    // LONG DesktopCX, DesktopCY; // width and height of screen
    // get mouse coordinates (absolute coordinates)
    WinQueryPointerPos(HWND_DESKTOP, &ptlMouse);
    // get position of window list window
    WinQueryWindowPos(G_HookData.hwndWindowList,
                      &WinListPos);
    // calculate window list position (mouse pointer is center)
    WinListX = ptlMouse.x - (WinListPos.cx / 2);
    if (WinListX < 0)
        WinListX = 0;
    WinListY = ptlMouse.y - (WinListPos.cy / 2);
    if (WinListY < 0)
        WinListY = 0;
    if (WinListX + WinListPos.cx > G_HookData.lCXScreen)
        WinListX = G_HookData.lCXScreen - WinListPos.cx;
    if (WinListY + WinListPos.cy > G_HookData.lCYScreen)
        WinListY = G_HookData.lCYScreen - WinListPos.cy;
    // set window list window to calculated position
    WinSetWindowPos(G_HookData.hwndWindowList, HWND_TOP,
                    WinListX, WinListY, 0, 0,
                    SWP_MOVE | SWP_SHOW | SWP_ZORDER);
    // now make it the active window
    WinSetActiveWindow(HWND_DESKTOP,
                       G_HookData.hwndWindowList);
}

/******************************************************************
 *                                                                *
 *  Input hook (main function)                                    *
 *                                                                *
 ******************************************************************/

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
 *      This implements the XWorkplace mouse features such as
 *      hot corners, sliding focus, mouse-button-3 scrolling etc.
 *
 *      Note: WM_CHAR messages (hotkeys) are not processed here,
 *      but rather in hookPreAccelHook, which gets called even
 *      before this hook. See remarks there.
 *
 *      We return TRUE if the message is to be swallowed, or FALSE
 *      if the current application (or the next hook in the hook
 *      chain) should still receive the message.
 *
 *      I have tried to keep the actual hook function as short as
 *      possible. Several separate subfunctions have been extracted
 *      which only get called if the respective functionality is
 *      actually needed (depending on the message and whether the
 *      respective feature has been enabled), both for clarity and
 *      speed. This is faster because the hook function gets called
 *      thousands of times (for every posted message...), and this
 *      way it is more likely that the hook code can completely remain
 *      in the processor caches all the time when lots of messages are
 *      processed.
 *
 *@@changed V0.9.1 (99-12-03) [umoeller]: added MB3 mouse scroll
 *@@changed V0.9.1 (2000-01-31) [umoeller]: fixed end-scroll bug
 *@@changed V0.9.2 (2000-02-21) [umoeller]: added PageMage processing
 *@@changed V0.9.2 (2000-02-25) [umoeller]: HScroll not working when VScroll disabled; fixed
 *@@changed V0.9.2 (2000-02-25) [umoeller]: added checks for mouse capture
 *@@changed V0.9.4 (2000-06-11) [umoeller]: changed MB3 scroll to WM_BUTTON3MOTIONSTART/END
 *@@changed V0.9.4 (2000-06-11) [umoeller]: added MB3 single-click
 *@@changed V0.9.4 (2000-06-14) [umoeller]: fixed PMMail win-list-as-pointer bug
 *@@changed V0.9.9 (2001-01-29) [umoeller]: auto-hide now respects button clicks too
 *@@changed V0.9.9 (2001-03-20) [lafaix]: fixed MB3 scroll for EPM clients
 *@@changed V0.9.9 (2001-03-20) [lafaix]: added many KC_NONE checks
 *@@changed V0.9.9 (2001-03-20) [lafaix]: added MB3 Autoscroll support
 *@@changed V0.9.9 (2001-03-21) [lafaix]: added MB3 Push2Bottom support
 */

BOOL EXPENTRY hookInputHook(HAB hab,        // in: anchor block of receiver wnd
                            PQMSG pqmsg,    // in/out: msg data
                            USHORT fs)      // in: either PM_REMOVE or PM_NOREMOVE
{
    // set return value:
    // per default, pass message on to next hook or application
    BOOL        brc = FALSE;

    BOOL        fRestartAutoHide = FALSE;
                            // set to TRUE if auto-hide mouse should be handles

    BOOL        bAutoScroll = FALSE;
                            // set to TRUE if autoscroll requested
    HWND        hwnd;
    ULONG       msg;
    MPARAM      mp1, mp2;

    if (pqmsg == NULL)
        return (FALSE);

    hwnd = pqmsg->hwnd;
    msg = pqmsg->msg;
    mp1 = pqmsg->mp1;
    mp2 = pqmsg->mp2;

    if (    // PageMage running?
            (G_HookData.hwndPageMageFrame)
            // switching not disabled?
         && (!G_HookData.fDisableSwitching)
                // this flag is set frequently when PageMage
                // is doing tricky stuff; we must not process
                // messages then, or we'll recurse forever
       )
    {
        // OK, go ahead:
        ProcessMsgsForPageMage(hwnd, msg, mp1, mp2);
    }

    switch(msg)
    {
        /*****************************************************************
         *                                                               *
         *  mouse button 1                                               *
         *                                                               *
         *****************************************************************/

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
            if (    (G_HookData.bAutoScroll)
                 && (G_HookData.hwndCurrentlyScrolling)
               )
            {
                StopMB3Scrolling(TRUE);

                // swallow msg
                brc = TRUE;
            }
            else
                if (    (G_HookData.HookConfig.fSlidingFocus)
                     && (!G_HookData.HookConfig.fSlidingBring2Top)
                   )
                {
                    // make sure that the mouse is not currently captured
                    if (WinQueryCapture(HWND_DESKTOP) == NULLHANDLE)
                        WinSetWindowPos(GetFrameWindow(hwnd),
                                        HWND_TOP,
                                        0,
                                        0,
                                        0,
                                        0,
                                        SWP_NOADJUST | SWP_ZORDER);
                            // @@todo yoo-hoo, this doesn't really work...
                            // this activates the frame, which is not really
                            // what we want in every case... this breaks XCenter too
                }

            // un-hide mouse if auto-hidden
            fRestartAutoHide = TRUE;
        break;

        /*****************************************************************
         *                                                               *
         *  mouse button 2                                               *
         *                                                               *
         *****************************************************************/

        /*
         * WM_BUTTON2DOWN:
         *      if mb2 is clicked on titlebar,
         *      show system menu as context menu.
         *
         *      Based on code from WarpEnhancer, (C) Achim HasenmÅller.
         */

        case WM_BUTTON2DOWN:
        {
            if (    (G_HookData.bAutoScroll)
                 && (G_HookData.hwndCurrentlyScrolling)
               )
            {
                StopMB3Scrolling(TRUE);

                // swallow msg
                brc = TRUE;
            }
            else
                if (    (G_HookData.HookConfig.fSysMenuMB2TitleBar)
                     && (SHORT2FROMMP(mp2) == KC_NONE)
                   )
                {
                    // make sure that the mouse is not currently captured
                    if (WinQueryCapture(HWND_DESKTOP) == NULLHANDLE)
                    {
                        CHAR szWindowClass[3];
                        // get class name of window being created
                        WinQueryClassName(hwnd,
                                          sizeof(szWindowClass),
                                          szWindowClass);
                        // mouse button 2 was pressed over a title bar control
                        if (strcmp(szWindowClass, "#9") == 0)
                        {
                            // copy system menu and display it as context menu
                            WMButton_SystemMenuContext(hwnd);
                            brc = TRUE; // swallow message
                        }
                    }
                }

            // un-hide mouse if auto-hidden
            fRestartAutoHide = TRUE;
        break; }

        /*****************************************************************
         *                                                               *
         *  mouse button 3                                               *
         *                                                               *
         *****************************************************************/

        /*
         * WM_BUTTON3MOTIONSTART:
         *      start MB3 scrolling. This prepares the hook
         *      data for tracking mouse movements in WM_MOUSEMOVE
         *      so that the window under the mouse will be scrolled.
         *
         *      Changed this from WM_BUTTON3DOWN with V0.9.4 to avoid
         *      the Netscape and PMMail hangs. However, this message
         *      is never received in VIO windows, so for those, we'll
         *      still have to use WM_BUTTON3DOWN.
         *
         *      Based on code from WarpEnhancer, (C) Achim HasenmÅller.
         */

        case WM_BUTTON3MOTIONSTART: // mouse button 3 was pressed down
            // MB3-scroll enabled?
            if (    (G_HookData.HookConfig.fMB3Scroll)
                 && (SHORT2FROMMP(mp2) == KC_NONE)
               )
                // yes:
                // make sure that the mouse is not currently captured
                if (WinQueryCapture(HWND_DESKTOP) == NULLHANDLE)
                    // OK:
                    goto BEGIN_MB3_SCROLL;
        break;

        /*
         * WM_BUTTON3DOWN:
         *      start MB3-scrolling.
         */

        case WM_BUTTON3DOWN:
        {
            CHAR szClassName[200];

            if (    (G_HookData.bAutoScroll)
                 && (G_HookData.hwndCurrentlyScrolling)
               )
            {
                StopMB3Scrolling(TRUE);

                // swallow msg
                brc = TRUE;
            }
            else
            // MB3 scrolling enabled?
            if (    (G_HookData.HookConfig.fMB3Scroll)
                 && (SHORT2FROMMP(mp2) == KC_NONE)
                 && (WinQueryClassName(hwnd, sizeof(szClassName), szClassName))
               )
            {
                // VIO, EPM, or UMAIL EPM client window?
                if (    (strcmp(szClassName, "Shield") == 0)
                     || (strcmp(szClassName, "NewEditWndClass") == 0)
                     || (strcmp(szClassName, "UMAILEPM") == 0)
                   )
                {
                    // yes:

                    // if EPM client window, swallow
                    if (szClassName[0] != 'S')
                        brc = TRUE;

                    // prepare MB3 scrolling for WM_MOUSEMOVE later:
                    BEGIN_MB3_SCROLL:

                    // set window that we're currently scrolling
                    // (this enables scroll processing during WM_MOUSEMOVE)
                    G_HookData.hwndCurrentlyScrolling = hwnd;
                    // indicate that initial mouse positions have to be recalculated
                    // (checked by first call to WMMouseMove_MB3OneScrollbar)
                    G_HookData.SDXHorz.sMB3InitialScreenMousePos = -1;
                    G_HookData.SDYVert.sMB3InitialScreenMousePos = -1; // V0.9.2 (2000-02-25) [umoeller]
                    // reset flags for WM_BUTTON3UP below; these
                    // will be set to TRUE by WMMouseMove_MB3OneScrollbar
                    G_HookData.SDYVert.fPostSBEndScroll = FALSE;
                    G_HookData.SDXHorz.fPostSBEndScroll = FALSE;
                    // specify what scrolling is about to happen
                    G_HookData.bAutoScroll = bAutoScroll;

                    // capture messages for window under mouse until
                    // MB3 is released again; this makes sure that scrolling
                    // works even if the mouse pointer is moved out of the
                    // window while MB3 is depressed.
                    // Also, if we don't do this, we cannot communicate
                    // with the window under the mouse with some messages
                    // (thumb size) which need to pass memory buffers, because
                    // then WM_MOUSEMOVE (and thus the hook) runs in a different
                    // process...
                    WinSetCapture(HWND_DESKTOP, hwnd);

                    // disabling auto-hide mouse pointer
                    G_HookData.fOldAutoHideMouse = G_HookData.HookConfig.fAutoHideMouse;
                    G_HookData.HookConfig.fAutoHideMouse = FALSE;

                    // if AutoScroll, don't wait for a WM_MOUSESCROLL
                    // to update pointers and display, so that the user
                    // is aware of the mode change
                    if (bAutoScroll)
                    {
                        G_ptlMousePosDesktop.x = pqmsg->ptl.x;
                        G_ptlMousePosDesktop.y = pqmsg->ptl.y;
                        WMMouseMove_MB3Scroll(hwnd);
                    }
                }

                // swallow msg
                // brc = TRUE;
            }

            // un-hide mouse if auto-hidden
            fRestartAutoHide = TRUE;
        break; }

        /*
         * WM_BUTTON3MOTIONEND:
         *      stop MB3 scrolling.
         *
         *      Also needed for MB3 double-clicks on minimized windows.
         *
         *      Based on code from WarpEnhancer, (C) Achim HasenmÅller.
         *      Contributed for V0.9.4 by Lars Erdmann.
         */

        case WM_BUTTON3UP: // mouse button 3 has been released
        {
            if (    (G_HookData.HookConfig.fMB3Scroll)
                 && (G_HookData.hwndCurrentlyScrolling)
               )
            {
                StopMB3Scrolling(TRUE);     // success, post msgs

                // if the mouse has not moved, and if BUTTON3DOWN
                // was swallowed, then fake a BUTTON3CLICK.
                if (    (G_HookData.SDXHorz.sMB3InitialScreenMousePos == -1)
                     && (G_HookData.SDYVert.sMB3InitialScreenMousePos == -1)
                   )
                {
                    CHAR szClassName[200];

                    if (WinQueryClassName(hwnd, sizeof(szClassName), szClassName))
                    {
                        if (    (strcmp(szClassName, "NewEditWndClass") == 0)
                             || (strcmp(szClassName, "UMAILEPM") == 0)
                           )
                            WinPostMsg(hwnd, WM_BUTTON3CLICK, mp1, mp2);
                    }
                }

                // swallow msg
                // brc = TRUE;
            }
            else
            // MB3 click conversion enabled?
            if (G_HookData.HookConfig.fMB3Click2MB1DblClk)
            {
                // is window under mouse minimized?
                SWP swp;
                WinQueryWindowPos(hwnd,&swp);
                if (swp.fl & SWP_MINIMIZE)
                {
                    // yes:
                    WinPostMsg(hwnd, WM_BUTTON1DOWN, mp1, mp2);
                    WinPostMsg(hwnd, WM_BUTTON1UP, mp1, mp2);
                    WinPostMsg(hwnd, WM_SINGLESELECT, mp1, mp2);
                    WinPostMsg(hwnd, WM_BUTTON1DBLCLK, mp1, mp2);
                    WinPostMsg(hwnd, WM_OPEN, mp1, mp2);
                    WinPostMsg(hwnd, WM_BUTTON1UP, mp1, mp2);
                }
                // brc = FALSE;   // pass on to next hook in chain (if any)
                // you HAVE TO return FALSE so that the OS
                // can translate a sequence of WM_BUTTON3DOWN
                // WM_BUTTON3UP to WM_BUTTON3CLICK
            }
        break; }

        /*
         * WM_BUTTON3CLICK:
         *      convert MB3 single-clicks to MB1 double-clicks, AutoScroll,
         *      or Push2Bottom.
         *
         *      Contributed for V0.9.4 by Lars Erdmann.
         */

        case WM_BUTTON3CLICK:
            // MB3 click conversion enabled?
            if (G_HookData.HookConfig.fMB3Click2MB1DblClk)
            {
                // if we would post a WM_BUTTON1DOWN message to the titlebar,
                // it would not receive WM_BUTTON1DBLCLK, WM_OPEN, WM_BUTTON1UP
                // for some strange reason (I think it has something to do with
                // the window tracking that is initiated when WM_BUTTON1DOWN
                // is posted to the titlebar, because it does not make sense
                // to prepare any window tracking when we really want to maximize
                // or restore the window, we just skip this);
                // for all other windows, pass this on
                if (WinQueryWindowUShort(hwnd, QWS_ID) != FID_TITLEBAR)
                {
                    WinPostMsg(hwnd, WM_BUTTON1DOWN, mp1, mp2);
                    WinPostMsg(hwnd, WM_BUTTON1UP, mp1, mp2);
                }
                WinPostMsg(hwnd, WM_SINGLESELECT, mp1, mp2);
                WinPostMsg(hwnd, WM_BUTTON1DBLCLK, mp1, mp2);
                WinPostMsg(hwnd, WM_OPEN, mp1, mp2);
                WinPostMsg(hwnd, WM_BUTTON1UP, mp1, mp2);
            }
            else
            // MB3 autoscroll enabled?
            if (    (G_HookData.HookConfig.fMB3AutoScroll)
                 && (SHORT2FROMMP(mp2) == KC_NONE)
               )
            {
                // yes:
                // make sure that the mouse is not currently captured
                if (WinQueryCapture(HWND_DESKTOP) == NULLHANDLE)
                {
                    // OK:
                    bAutoScroll = TRUE;

                    goto BEGIN_MB3_SCROLL;
                }
            }
            else
            // MB3 push to bottom enabled?
            if (    (G_HookData.HookConfig.fMB3Push2Bottom)
                 && (SHORT2FROMMP(mp2) == KC_NONE)
               )
            {
                // make sure that the mouse is not currently captured
                if (WinQueryCapture(HWND_DESKTOP) == NULLHANDLE)
                    WinSetWindowPos(GetFrameWindow(hwnd),
                                    HWND_BOTTOM,
                                    0,
                                    0,
                                    0,
                                    0,
                                    SWP_NOADJUST | SWP_ZORDER);
            }
            // brc = FALSE;   // pass on to next hook in chain (if any)
        break;

        /*
         * WM_CHORD:
         *      MB 1 and 2 pressed simultaneously:
         *      if enabled, we show the window list at the
         *      current mouse position.
         *
         *      Based on code from WarpEnhancer, (C) Achim HasenmÅller.
         */

        case WM_CHORD:
            brc = FALSE;
            // feature enabled?
            if (G_HookData.HookConfig.fChordWinList)
                // make sure that the mouse is not currently captured
                if (WinQueryCapture(HWND_DESKTOP) == NULLHANDLE)
                {
                    WinPostMsg(G_HookData.hwndDaemonObject,
                               XDM_WMCHORDWINLIST,
                               0, 0);
                    // WMChord_WinList();
                    // brc = TRUE;         // swallow message
                    // we must not swallow the message, or PMMail
                    // will cause the "win list as cursor" bug
                }
        break;

        /*
         * WM_MOUSEMOVE:
         *      "hot corners", "sliding focus", "MB3 scrolling" support.
         *      This is the most complex part of the hook and calls
         *      several subfunctions in turn.
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
            brc = WMMouseMove(pqmsg,
                              &fRestartAutoHide);
        break; // WM_MOUSEMOVE
    }

    if (fRestartAutoHide)
        // handle auto-hide V0.9.9 (2001-01-29) [umoeller]
        WMMouseMove_AutoHideMouse();

    return (brc);                           // msg not processed if FALSE
}

/******************************************************************
 *                                                                *
 *  Pre-accelerator hook                                          *
 *                                                                *
 ******************************************************************/

/*
 *@@ WMChar_FunctionKeys:
 *      returns TRUE if the specified key is one of
 *      the user-defined XWP function keys.
 *
 *@@added V0.9.3 (2000-04-20) [umoeller]
 */

BOOL WMChar_FunctionKeys(USHORT usFlags,  // in: SHORT1FROMMP(mp1) from WM_CHAR
                         UCHAR ucScanCode) // in: CHAR4FROMMP(mp1) from WM_CHAR
{
    // set return value:
    // per default, pass message on to next hook or application
    BOOL brc = FALSE;

    // scan code valid?
    if (usFlags & KC_SCANCODE)
    {
        // request access to shared memory
        // with function key definitions:
        PFUNCTIONKEY paFunctionKeysShared = NULL;
        APIRET arc = DosGetNamedSharedMem((PVOID*)(&paFunctionKeysShared),
                                          SHMEM_FUNCTIONKEYS,
                                          PAG_READ | PAG_WRITE);

        // _Pmpf(("  DosGetNamedSharedMem arc: %d", arc));

        _Pmpf(("WMChar_FunctionKeys: Searching for scan code 0x%lX", ucScanCode));

        if (arc == NO_ERROR)
        {
            // search function keys array
            ULONG   ul = 0;
            PFUNCTIONKEY pKeyThis = paFunctionKeysShared;
            for (ul = 0;
                 ul < G_cFunctionKeys;
                 ul++)
            {
                _Pmpf(("  funckey %d is scan code 0x%lX", ul, pKeyThis->ucScanCode));

                if (pKeyThis->ucScanCode == ucScanCode)
                {
                    // scan codes match:
                    // return "found" flag
                    brc = TRUE;
                    break;  // for
                }

                pKeyThis++;
            }

            DosFreeMem(paFunctionKeysShared);
        } // end if DosGetNamedSharedMem

    } // end if (usFlags & KC_SCANCODE)

    return (brc);
}

/*
 *@@ WMChar_Hotkeys:
 *      this gets called from hookPreAccelHook to
 *      process WM_CHAR messages.
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
 *      semaphore. See hookSetGlobalHotkeys. While this
 *      function is being called, the global mutex is
 *      requested, so this is protected.
 *
 *      If this returns TRUE, the msg is swallowed. We
 *      return TRUE if we found a hotkey.
 *
 *      This function gets called for BOTH the key-down and
 *      the key-up message. As usual with PM, applications
 *      should react to key-down messages only. However, since
 *      we swallow the key-down message if a hotkey was found
 *      (and post XDM_HOTKEYPRESSED to the daemon object window),
 *      we should swallow the key-up msg also, because otherwise
 *      the application gets a key-up msg without a previous
 *      key-down event, which might lead to confusion.
 *
 *@@changed V0.9.3 (2000-04-10) [umoeller]: moved debug code to hook
 *@@changed V0.9.3 (2000-04-10) [umoeller]: removed usch and usvk params
 *@@changed V0.9.3 (2000-04-19) [umoeller]: added XWP function keys support
 *@@changed V0.9.9 (2001-03-05) [umoeller]: removed break for multiple hotkey assignments
 */

BOOL WMChar_Hotkeys(USHORT usFlagsOrig,  // in: SHORT1FROMMP(mp1) from WM_CHAR
                    UCHAR ucScanCode) // in: CHAR4FROMMP(mp1) from WM_CHAR
{
    // set return value:
    // per default, pass message on to next hook or application
    BOOL brc = FALSE;

    // request access to shared memory
    // with hotkey definitions:
    PGLOBALHOTKEY pGlobalHotkeysShared = NULL;
    APIRET arc = DosGetNamedSharedMem((PVOID*)(&pGlobalHotkeysShared),
                                      SHMEM_HOTKEYS,
                                      PAG_READ | PAG_WRITE);

    _Pmpf(("  DosGetNamedSharedMem arc: %d", arc));

    if (arc == NO_ERROR)
    {
        // OK, we got the shared hotkeys:
        USHORT  us;

        PGLOBALHOTKEY pKeyThis = pGlobalHotkeysShared;

        // filter out unwanted flags;
        // we used to check for KC_VIRTUALKEY also, but this doesn't
        // work with VIO sessions, where this is never set V0.9.3 (2000-04-10) [umoeller]
        USHORT usFlags = usFlagsOrig & (KC_CTRL | KC_ALT | KC_SHIFT);

        _Pmpf(("  Checking %d hotkeys for scan code 0x%lX",
                G_cGlobalHotkeys,
                ucScanCode));

        // now go through the shared hotkey list and check
        // if the pressed key was assigned an action to
        us = 0;
        for (us = 0;
             us < G_cGlobalHotkeys;
             us++)
        {
            _Pmpf(("    item %d: scan code %lX", us, pKeyThis->ucScanCode));

            // when comparing,
            // filter out KC_VIRTUALKEY, because this is never set
            // in VIO sessions... V0.9.3 (2000-04-10) [umoeller]
            if (   ((pKeyThis->usFlags & (KC_CTRL | KC_ALT | KC_SHIFT))
                            == usFlags)
                && (pKeyThis->ucScanCode == ucScanCode)
               )
            {
                // hotkey found:

                // only for the key-down event,
                // notify daemon
                if ((usFlagsOrig & KC_KEYUP) == 0)
                    WinPostMsg(G_HookData.hwndDaemonObject,
                               XDM_HOTKEYPRESSED,
                               (MPARAM)(pKeyThis->ulHandle),
                               (MPARAM)0);

                // reset return code: swallow this message
                // (both key-down and key-up)
                brc = TRUE;
                // get outta here
                // break; // for
                        // removed this V0.9.9 (2001-03-05) [umoeller]
                        // to allow for multiple hotkey assignments...
                        // whoever wants to use this!
            }

            // not found: go for next key to check
            pKeyThis++;
        } // end for

        DosFreeMem(pGlobalHotkeysShared);
    } // end if DosGetNamedSharedMem

    return (brc);
}

/*
 *@@ WMChar_Main:
 *      WM_CHAR processing in hookPreAccelHook.
 *      This has been extracted with V0.9.3 (2000-04-20) [umoeller].
 *
 *      This requests the global hotkeys semaphore to make the
 *      whole thing thread-safe.
 *
 *      Then, we call WMChar_FunctionKeys to see if the key is
 *      a function key. If so, the message is swallowed in any
 *      case.
 *
 *      Then, we call WMChar_Hotkeys to check if some global hotkey
 *      has been defined for the key (be it a function key or some
 *      other key combo). If so, the message is swallowed.
 *
 *      Third, we check for the PageMage switch-desktop hotkeys.
 *      If one of those is found, we swallow the msg also and
 *      notify PageMage.
 *
 *      That is, the msg is swallowed if it's a function key or
 *      a global hotkey or a PageMage hotkey.
 *
 *@@added V0.9.3 (2000-04-20) [umoeller]
 */

BOOL WMChar_Main(PQMSG pqmsg)       // in/out: from hookPreAccelHook
{
    // set return value:
    // per default, pass message on to next hook or application
    BOOL brc = FALSE;

    USHORT usFlags    = SHORT1FROMMP(pqmsg->mp1);
    // UCHAR  ucRepeat   = CHAR3FROMMP(mp1);
    UCHAR  ucScanCode = CHAR4FROMMP(pqmsg->mp1);
    USHORT usch       = SHORT1FROMMP(pqmsg->mp2);
    USHORT usvk       = SHORT2FROMMP(pqmsg->mp2);

    // request access to the hotkeys mutex:
    // first we need to open it, because this
    // code can be running in any PM thread in
    // any process
    APIRET arc = DosOpenMutexSem(NULL,       // unnamed
                                 &G_hmtxGlobalHotkeys);
    if (arc == NO_ERROR)
    {
        // OK, semaphore opened: request access
        arc = WinRequestMutexSem(G_hmtxGlobalHotkeys,
                                 TIMEOUT_HMTX_HOTKEYS);

        // _Pmpf(("WM_CHAR WinRequestMutexSem arc: %d", arc));

        if (arc == NO_ERROR)
        {
            // OK, we got the mutex:
            // search the list of function keys
            BOOL    fIsFunctionKey = WMChar_FunctionKeys(usFlags, ucScanCode);
                        // returns TRUE if ucScanCode represents one of the
                        // function keys

            // global hotkeys enabled? This also gets called if PageMage hotkeys are on!
            if (G_HookData.HookConfig.fGlobalHotkeys)
            {
                if (    // process only key-down messages
                        // ((usFlags & KC_KEYUP) == 0)
                        // check flags:
                        // do the list search only if the key could be
                        // a valid hotkey, that is:
                        (
                            // 1) it's a function key
                            (fIsFunctionKey)

                            // or 2) it's a typical virtual key or Ctrl/alt/etc combination
                        ||  (     ((usFlags & KC_VIRTUALKEY) != 0)
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
                        )
                        // always filter out those ugly composite key (accents etc.),
                        // but make sure the scan code is valid V0.9.3 (2000-04-10) [umoeller]
                   &&   ((usFlags & (KC_DEADKEY
                                     | KC_COMPOSITE
                                     | KC_INVALIDCOMP
                                     | KC_SCANCODE))
                          == KC_SCANCODE)
                   )
                {
                      /*
                 In PM session:
                                              usFlags         usvk usch       ucsk
                      Ctrl alone              VK SC CTRL       0a     0        1d
                      Ctrl-A                     SC CTRL        0    61        1e
                      Ctrl-Alt                VK SC CTRL ALT   0b     0        38
                      Ctrl-Alt-A                 SC CTRL ALT    0    61        1e

                      F11 alone               VK SC toggle     2a  8500        57
                      Ctrl alone              VK SC CTRL       0a     0        1d
                      Ctrl-Alt                VK SC CTRL ALT   0b     0        38
                      Ctrl-Alt-F11            VK SC CTRL ALT   2a  8b00        57

                 In VIO session:
                      Ctrl alone                 SC CTRL       07   0          1d
                      Ctrl-A                     SC CTRL        0   1e01       1e
                      Ctrl-Alt                   SC CTRL ALT   07   0          38
                      Ctrl-Alt-A                 SC CTRL ALT   20   1e00       1e

                      Alt-A                      SC      ALT   20   1e00
                      Ctrl-E                     SC CTRL        0   3002

                      F11 alone               ignored...
                      Ctrl alone              VK SC CTRL       07!    0        1d
                      Ctrl-Alt                VK SC CTRL ALT   07!    0        38
                      Ctrl-Alt-F11            !! SC CTRL ALT   20! 8b00        57

                      So apparently, for these keyboard combinations, in VIO
                      sessions, the KC_VIRTUALKEY flag is missing. Strange.

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
                                strcat(szFlags, "KC_INVALIDCOMP ");
                            if (usFlags & KC_TOGGLE)                    // 0x1000
                                strcat(szFlags, "KC_TOGGLE ");
                            if (usFlags & KC_INVALIDCHAR)               // 0x2000
                                strcat(szFlags, "KC_INVALIDCHAR ");
                            if (usFlags & KC_DBCSRSRVD1)                // 0x4000
                                strcat(szFlags, "KC_DBCSRSRVD1 ");
                            if (usFlags & KC_DBCSRSRVD2)                // 0x8000
                                strcat(szFlags, "KC_DBCSRSRVD2 ");

                            _Pmpf(("  usFlags: 0x%lX -->", usFlags));
                            _Pmpf(("    %s", szFlags));
                            _Pmpf(("  usvk: 0x%lX", usvk));
                            _Pmpf(("  usch: 0x%lX", usch));
                            _Pmpf(("  ucScanCode: 0x%lX", ucScanCode));
                    #endif

/* #ifdef __DEBUG__
                    // debug code:
                    // enable Ctrl+Alt+Delete emergency exit
                    if (    (   (usFlags & (KC_CTRL | KC_ALT | KC_KEYUP))
                                 == (KC_CTRL | KC_ALT)
                            )
                          && (ucScanCode == 0x0e)    // delete
                       )
                    {
                        ULONG ul;
                        for (ul = 5000;
                             ul > 100;
                             ul -= 200)
                            DosBeep(ul, 20);

                        WinPostMsg(G_HookData.hwndDaemonObject,
                                   WM_QUIT,
                                   0, 0);
                        brc = TRUE;     // swallow
                    }
                    else
#endif */

                    if (WMChar_Hotkeys(usFlags, ucScanCode))
                        // returns TRUE (== swallow) if hotkey was found
                        brc = TRUE;
                }
            }

            // PageMage hotkeys:
            if (!brc)       // message not swallowed yet:
            {
                if (    (G_HookData.PageMageConfig.fEnableArrowHotkeys)
                            // arrow hotkeys enabled?
                     && (G_HookData.hwndPageMageClient)
                            // PageMage active?
                   )
                {
                    // PageMage hotkeys enabled:
                    // key-up only
                    if ((usFlags & KC_KEYUP) == 0)
                    {
                        // check KC_CTRL etc. flags
                        if (   (G_HookData.PageMageConfig.ulKeyShift | KC_SCANCODE)
                                    == (usFlags & (G_HookData.PageMageConfig.ulKeyShift
                                                   | KC_SCANCODE))
                           )
                            // OK: check scan codes
                            if (    (ucScanCode == 0x61)
                                 || (ucScanCode == 0x66)
                                 || (ucScanCode == 0x63)
                                 || (ucScanCode == 0x64)
                               )
                            {
                                // cursor keys:
                                /* ULONG ulRequest = PGMGQENCODE(PGMGQ_HOOKKEY,
                                                              ucScanCode,
                                                              ucScanCode); */
                                WinPostMsg(G_HookData.hwndPageMageMoveThread,
                                           PGOM_HOOKKEY,
                                           (MPARAM)ucScanCode,
                                           0);
                                // swallow
                                brc = TRUE;
                            }

                    }
                }
            } // end if (!brc)       // message not swallowed yet

            DosReleaseMutexSem(G_hmtxGlobalHotkeys);
        } // end if WinRequestMutexSem

        DosCloseMutexSem(G_hmtxGlobalHotkeys);
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
 *      As with hookInputHook, we return TRUE if the message is
 *      to be swallowed, or FALSE if the current application (or
 *      the next hook in the hook chain) should still receive the
 *      message.
 *
 *@@changed V0.9.3 (2000-04-09) [umoeller]: added check for system lockup
 *@@changed V0.9.3 (2000-04-09) [umoeller]: added PageMage hotkeys
 *@@changed V0.9.3 (2000-04-09) [umoeller]: added KC_SCANCODE check
 *@@changed V0.9.3 (2000-04-10) [umoeller]: moved debug code to hook
 */

BOOL EXPENTRY hookPreAccelHook(HAB hab, PQMSG pqmsg, ULONG option)
{
    // set return value:
    // per default, pass message on to next hook or application
    BOOL        brc = FALSE;

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
            if (   (    (G_HookData.HookConfig.fGlobalHotkeys)
                    ||  (G_HookData.PageMageConfig.fEnableArrowHotkeys)
                   )
               && (G_HookData.hwndLockupFrame == NULLHANDLE)    // system not locked up
               )
            {
                brc = WMChar_Main(pqmsg);
            }

        break; // WM_CHAR
    } // end switch(msg)

    return (brc);
}

