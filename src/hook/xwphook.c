
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
 *      Most of the hook features started out with code and ideas
 *      taken from ProgramCommander/2 and WarpEnhancer and were then
 *      extended.
 *
 *      I have tried to design the hooks for clarity and lucidity with
 *      respect to interfaces and configuration --
 *      as if that was possible with system hooks at all, since they
 *      are so messy by nature.
 *
 *      With V0.9.12, the hook code was spread across several files
 *
 *      1)  to group the actual hook functions together for a better
 *          code path,
 *
 *      2)  to make all this more readable.
 *
 *      A couple words of wisdom about the hook code:
 *
 *      --  System hooks are possibly running in the context of each PM
 *          thread which is processing a message -- that is, possibly each
 *          thread on the system which has created a message queue.
 *
 *          As a result, great care must be taken when accessing global
 *          data, and one must keep in mind for each function which thread
 *          it might run in, or otherwise we'd be asking for system hangs.
 *          Don't expect OS/2 to handle exceptions properly if all PM
 *          programs crash at the same time.
 *
 *          It _is_ possible to access static global variables of
 *          this DLL from every function, because the DLL is
 *          linked with the STATIC SHARED flags (see xwphook.def).
 *          This means that the single data segment of the DLL
 *          becomes part of each process which is using the DLL
 *          (that is, each PM process on the system). We use this
 *          for the HOOKDATA structure, which is a static global
 *          structure in the DLL's shared data segment and is
 *          returned to the daemon when it calls hookInit in the DLL.
 *
 *          HOOKDATA includes a HOOKCONFIG structure. As a result,
 *          to configure the basic hook flags, the daemon can simply
 *          modify the fields in the HOOKDATA structure to configure
 *          the hook's behavior.
 *
 *          It is however _not_ possible to use malloc() to allocate
 *          global memory and use it between several calls of the
 *          hooks, because that memory will belong to one process
 *          only, even if the pointer is stored in a global DLL
 *          variable. The next time the hook gets called and accesses
 *          that memory, some fairly random application will crash
 *          (the one the hook is currently called for), or the system
 *          will hang completely.
 *
 *          For this reason, the hooks use another block of shared
 *          memory internally, which is protected by a named mutex
 *          semaphore, for storing the list of object hotkeys (which
 *          is variable in size). This block is (re)allocated in
 *          hookSetGlobalHotkeys and requested in the hook when
 *          WM_CHAR comes in. The mutex is necessary because when
 *          hotkeys are changed, the daemon changes the structure by
 *          calling hookSetGlobalHotkeys.
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

/******************************************************************
 *
 *  System-wide global variables for input hook
 *
 ******************************************************************/

HWND    G_hwndUnderMouse = NULLHANDLE;
HWND    G_hwndLastFrameUnderMouse = NULLHANDLE;
HWND    G_hwndLastSubframeUnderMouse = NULLHANDLE;
POINTS  G_ptsMousePosWin = {0};
POINTL  G_ptlMousePosDesktop = {0};

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
 *
 *  Helper functions
 *
 ******************************************************************/

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

/******************************************************************
 *
 *  Hook interface
 *
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
            G_paGlobalHotkeys = 0;
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
            G_paFunctionKeys = 0;
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
 *
 *  Send-Message Hook
 *
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

/******************************************************************
 *
 *  Lockup Hook
 *
 ******************************************************************/

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
 *
 *  Input hook (main function)
 *
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
         *      Based on ideas from WarpEnhancer by Achim Hasenmller.
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
         *      Based on ideas from WarpEnhancer by Achim Hasenmller.
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
         *      Based on ideas from WarpEnhancer by Achim Hasenmller.
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
         *      Based on ideas from WarpEnhancer by Achim Hasenmller.
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
 *
 *  Pre-accelerator hook
 *
 ******************************************************************/

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

