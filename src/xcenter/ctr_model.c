
/*
 *@@sourcefile ctr_model.c:
 *      XCenter "model", i.e. settings management that is
 *      supposed to be independent of the currently open
 *      XCenter views.
 *
 *      See src\shared\center.c for an introduction to the XCenter.
 *
 *      This file is new with V0.9.16 and contains code
 *      formerly in ctr_engine.c. With 8000 lines of code it
 *      was becoming hard to overlook which code was view-
 *      specific and which wasn't. Since I was also trying
 *      to finally get setup strings to work properly, the
 *      model had to be properly separated from the views.
 *
 *      Function prefix for this file:
 *      --  ctrp*
 *
 *@@header "xcenter\centerp.h"
 */

/*
 *      Copyright (C) 2000-2002 Ulrich M”ller.
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

#define INCL_WINMESSAGEMGR
#define INCL_WINCOUNTRY
#define INCL_WINMENUS

#include <os2.h>

// C library headers
#include <stdio.h>
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\standards.h"          // some standard macros
#include "helpers\stringh.h"            // string helper routines
#include "helpers\winh.h"               // PM helper routines
#include "helpers\xstring.h"            // extended string helpers

// SOM headers which don't crash with prec. header files
#include "xcenter.ih"

// XWorkplace implementation headers
#include "bldlevel.h"                   // XWorkplace build level definitions

#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\errors.h"              // private XWorkplace error codes
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

#include "shared\center.h"              // public XCenter interfaces
#include "xcenter\centerp.h"            // private XCenter implementation

#pragma hdrstop                     // VAC++ keeps crashing otherwise

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

/*
 * G_aBuiltInWidgets:
 *      array of the built-in widgets in src\xcenter.
 *
 *@@changed V0.9.13 (2001-06-19) [umoeller]: added tray widget
 */

static const XCENTERWIDGETCLASS   G_aBuiltInWidgets[]
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
#ifdef __EWORKPLACE__
            "eButton",
#else
            "X-Button",
#endif
            WGTF_UNIQUEPERXCENTER | WGTF_TOOLTIP       // not trayable
                 | WGTF_CANTAKECENTERHOTKEY,        // V0.9.19 (2002-04-17) [umoeller]
            OwgtShowXButtonSettingsDlg          // V0.9.14 (2001-08-21) [umoeller]
        },
        // CPU pulse widget
        {
            WNDCLASS_WIDGET_PULSE,
            0,
            "Pulse",
            "Pulse (CPU load)",
            WGTF_SIZEABLE | WGTF_UNIQUEGLOBAL | WGTF_TOOLTIP, // not trayable
            PwgtShowSettingsDlg                 // V0.9.16 (2002-01-05) [umoeller]
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
        // WinRequestMutexSem works even if the thread has no message queue
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
 *@@ FreeModule:
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

static APIRET FreeModule(HMODULE hmod,
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

                    // go load this!
                    if (arc2 = DosLoadModule(szError,
                                             sizeof(szError),
                                             szDLL,
                                             &hmod))
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
                                PCXCENTERWIDGETCLASS paClasses = NULL;
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
                            FreeModule(hmod,
                                       FALSE);      // do not call uninit

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
 *@@changed V0.9.16 (2001-12-08) [umoeller]: added logging if unload fails
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
                        APIRET  arc;
                        CHAR    szModuleName[CCHMAXPATH];

                        // added logging if unload fails
                        // V0.9.16 (2001-12-08) [umoeller]
                        if (!DosQueryModuleName(hmod,
                                                sizeof(szModuleName),
                                                szModuleName))
                            if (arc = FreeModule(hmod,
                                                 TRUE))       // call uninit
                                cmnLog(__FILE__, __LINE__, __FUNCTION__,
                                       "FreeModule returned %d for %s (hmod %lX)",
                                       arc,
                                       szModuleName,
                                       hmod);

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
 *      Returns:
 *
 *      --  NO_ERROR: *ppClass has been set to the
 *          class struct.
 *
 *      --  XCERR_INVALID_CLASS_NAME: class doesn't exist.
 *
 *      --  XCERR_CLASS_NOT_TRAYABLE: class exists, but is
 *          not trayable.
 *
 *      --  ERROR_INVALID_PARAMETER
 *
 *      Preconditions:
 *
 *      --  Use this in a block between ctrpLoadClasses
 *          and ctrpFreeClasses.
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
 *@@changed V0.9.19 (2002-04-25) [umoeller]: changed prototype, added fMustBeTrayable.
 */

APIRET ctrpFindClass(PCSZ pcszWidgetClass,
                     BOOL fMustBeTrayable,
                     PCXCENTERWIDGETCLASS *ppClass)
{
    APIRET arc = XCERR_INVALID_CLASS_NAME;

    PLISTNODE pNode = lstQueryFirstNode(&G_llWidgetClasses);

    if (!pcszWidgetClass || !ppClass)
        return ERROR_INVALID_PARAMETER;

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
            if (    (fMustBeTrayable)
                 && (!(pClass->Public.ulClassFlags & WGTF_TRAYABLE))
               )
                arc = XCERR_CLASS_NOT_TRAYABLE;
            else
            {
                *ppClass = &pClass->Public;
                arc = NO_ERROR;
            }

            break;
        }

        pNode = pNode->pNext;
    }

    return (arc);    // can be NULL
}

typedef struct _CLASSTOINSERT
{
    ULONG               ulMenuID;
    PPRIVATEWIDGETCLASS pClass;
    ULONG               ulAttr;
} CLASSTOINSERT, *PCLASSTOINSERT;

static signed short XWPENTRY SortClasses(void* pItem1,
                                         void* pItem2,
                                         void* pStorage)       // HAB really
{
    switch (WinCompareStrings((HAB)pStorage,
                              0,
                              0,
                              (PSZ)((PCLASSTOINSERT)pItem1)->pClass->Public.pcszClassTitle,
                              (PSZ)((PCLASSTOINSERT)pItem2)->pClass->Public.pcszClassTitle,
                              0))
    {
        case WCS_LT:
            return -1;
        case WCS_GT:
            return +1;
    }

    return 0;
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
 *@@changed V0.9.16 (2002-02-02) [umoeller]: now sorting alphabetically
 *@@changed V0.9.18 (2002-03-08) [umoeller]: now never adding object buttons
 */

HWND ctrpAddWidgetsMenu(XCenter *somSelf,
                        HWND hwndMenu,
                        SHORT sPosition,        // in: position where to insert (can be MIT_END)
                        BOOL fTrayableOnly)     // in: if TRUE, only classes with WGTF_TRAYABLE will be shown
{
    HWND hwndWidgetsSubmenu;

    PLISTNODE       pClassNode;
    ULONG           ulIndex = 0;

    // linked list of menu items to add so we can sort
    // V0.9.16 (2002-02-02) [umoeller]
    LINKLIST        llClasses;
    PCLASSTOINSERT  pClass2Insert;
    lstInit(&llClasses, TRUE);      // auto-free

    ctrpLoadClasses();

    hwndWidgetsSubmenu =  winhInsertSubmenu(hwndMenu,
                                            sPosition,
                                            (cmnQuerySetting(sulVarMenuOffset) + ID_XFMI_OFS_XWPVIEW),
                                                    // fixed ID for sliding menus
                                                    // V0.9.13 (2001-06-14) [umoeller]
                                            cmnGetString(ID_XSSI_ADDWIDGET),
                                            MIS_TEXT,
                                            0, NULL, 0, 0);

    pClassNode = lstQueryFirstNode(&G_llWidgetClasses);
    while (pClassNode)
    {
        ULONG ulAttr = 0;
        PPRIVATEWIDGETCLASS pClass = (PPRIVATEWIDGETCLASS)pClassNode->pItemData;

        // WGTF_NOUSERCREATE means user cannot add this from
        // the widgets menu (e.g. object buttons) --> skip
        // (we used to disable only, but that's confusing)
        // V0.9.18 (2002-03-08) [umoeller]
        if (!(pClass->Public.ulClassFlags & WGTF_NOUSERCREATE))
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

            if (pClass2Insert = NEW(CLASSTOINSERT))
            {
                pClass2Insert->ulMenuID =    cmnQuerySetting(sulVarMenuOffset)
                                           + ID_XFMI_OFS_VARIABLE
                                           + ulIndex;
                pClass2Insert->pClass = pClass;
                pClass2Insert->ulAttr = ulAttr;

                lstAppendItem(&llClasses,
                              pClass2Insert);
            }
        }

        // raise the index in any case or the offsets are all wrong
        // V0.9.18 (2002-03-24) [umoeller]
        ulIndex++;

        pClassNode = pClassNode->pNext;
    }

    // sort alphabetically V0.9.16 (2002-02-02) [umoeller]
    lstQuickSort(&llClasses,
                 SortClasses,
                 (PVOID)WinQueryAnchorBlock(hwndWidgetsSubmenu));


    // now go insert the sorted list
    for (pClassNode = lstQueryFirstNode(&llClasses);
         pClassNode;
         pClassNode = pClassNode->pNext)
    {
        pClass2Insert = (PCLASSTOINSERT)pClassNode->pItemData;
        winhInsertMenuItem(hwndWidgetsSubmenu,
                           MIT_END,
                           pClass2Insert->ulMenuID,
                           (PSZ)pClass2Insert->pClass->Public.pcszClassTitle,
                           MIS_TEXT,
                           pClass2Insert->ulAttr);
    }

    lstClear(&llClasses);

    ctrpFreeClasses();

    return (hwndWidgetsSubmenu);
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
    ULONG ulOfsVariable =  cmnQuerySetting(sulVarMenuOffset) + ID_XFMI_OFS_VARIABLE;
    if (    (usCmd >=  ulOfsVariable)
         && (usCmd < (   ulOfsVariable
                       + lstCountItems(&G_llWidgetClasses))
            )
       )
    {
        // yes: get the corresponding item
        return (PPRIVATEWIDGETCLASS)lstItemFromIndex(&G_llWidgetClasses,
                                                     usCmd - ulOfsVariable);
    }

    return (NULL);
}

/* ******************************************************************
 *
 *   Widget settings management
 *
 ********************************************************************/

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

/* VOID AddWidgetSetting(XCenter *somSelf,
                      PPRIVATEWIDGETSETTING pSetting,  // in: new setting
                      ULONG ulBeforeIndex,             // in: index to insert before or -1 for end
{
} */

/*
 *@@ ctrpCheckClass:
 *      checks if the given class is valid and,
 *      if (fMustBeTrayable == TRUE), if the given class is
 *      trayable.
 *
 *      Returns:
 *
 *      --  NO_ERROR: class exists and is trayable.
 *
 *      --  XCERR_INVALID_CLASS_NAME: class doesn't exist.
 *
 *      --  XCERR_CLASS_NOT_TRAYABLE: class exists, but is
 *          not trayable.
 *
 *      --  ERROR_INVALID_PARAMETER
 *
 *@@added V0.9.19 (2002-04-25) [umoeller]
 */

APIRET ctrpCheckClass(PCSZ pcszWidgetClass,
                      BOOL fMustBeTrayable)
{
    PCXCENTERWIDGETCLASS    pClass;
    APIRET                  arc = XCERR_INVALID_CLASS_NAME;

    if (!pcszWidgetClass)
        return ERROR_INVALID_PARAMETER;

    if (ctrpLockClasses())
    {
        arc = ctrpFindClass(pcszWidgetClass,
                            fMustBeTrayable,
                            &pClass);

        ctrpUnlockClasses();
    }

    return (arc);
}

/*
 *@@ ctrpCreateWidgetSetting:
 *      creates a new widget setting, which is returned
 *      in *ppNewSetting.
 *
 *      This operates in two modes:
 *
 *      1)  If (pTray != NULL), a subwidget is created
 *          in the specified tray (of a tray widget).
 *          In that case, *pulNewItemCount receives the
 *          new count of subwidgets in the tray, and
 *          *pulNewWidgetIndex the index of the new
 *          widget in the tray.
 *
 *      2)  If (pTray == NULL), a root widget is created
 *          in the given XCenter. In that case, *pulNewItemCount
 *          receives the new count of root widgets, and
 *          *pulNewWidgetIndex the index of the new
 *          widget in the tray.
 *
 *      This does not create a widget window yet.
 *      Neither does this save the XCenter or
 *      tray data.
 *
 *      Returns:
 *
 *      --  NO_ERROR
 *
 *      --  ERROR_NOT_ENOUGH_MEMORY
 *
 *      --  ERROR_INVALID_PARAMETER
 *
 *      --  XCERR_INVALID_CLASS_NAME: class doesn't exist.
 *
 *      --  XCERR_CLASS_NOT_TRAYABLE: class exists, but is
 *          not trayable.
 *
 *@@added V0.9.13 (2001-06-21) [umoeller]
 *@@changed V0.9.16 (2001-10-18) [umoeller]: merged this with old AddWidgetSetting func
 *@@changed V0.9.19 (2002-04-25) [umoeller]: now checking if class is trayable
 */

APIRET ctrpCreateWidgetSetting(XCenter *somSelf,
                               PTRAYSETTING pTray,   // in: tray to create subwidget in or NULL
                               PCSZ pcszWidgetClass, // in: new widget's class (required)
                               PCSZ pcszSetupString, // in: new widget's setup string (can be NULL)
                               ULONG ulBeforeIndex,   // in: index (-1 for rightmost)
                               PPRIVATEWIDGETSETTING *ppNewSetting,  // out: newly created widget setting
                               PULONG pulNewItemCount,   // out: new settings count (ptr can be NULL)
                               PULONG pulNewWidgetIndex) // out: index of new widget (ptr can be NULL)
{
    APIRET arc = NO_ERROR;

    if (    (somSelf)
         && (pcszWidgetClass)       // this is required
         && (!(arc = ctrpCheckClass(pcszWidgetClass,
                                    // fMustBeTrayable:
                                    (pTray != NULL))))
                                // V0.9.19 (2002-04-25) [umoeller]
         && (ppNewSetting)
       )
    {
        PPRIVATEWIDGETSETTING pNew;
        if (pNew = NEW(PRIVATEWIDGETSETTING))
        {
            PLINKLIST pllSettings;

            ZERO(pNew);

            pNew->pOwningTray = pTray;

            if (    (!(arc = strhStore(&pNew->Public.pszWidgetClass,
                                       pcszWidgetClass,
                                       NULL)))
                 && (!(arc = strhStore(&pNew->Public.pszSetupString,
                                       pcszSetupString,
                                       NULL)))
               )
            {
                if (pTray)
                    // this is a subwidget:
                    pllSettings = &pTray->llSubwidgetSettings;
                else
                    // no tray == new root widget:
                    pllSettings = ctrpQuerySettingsList(somSelf);

                if (    (ulBeforeIndex == -1)
                     || (ulBeforeIndex >= lstCountItems(pllSettings))
                   )
                    lstAppendItem(pllSettings,
                                  pNew);
                else
                    lstInsertItemBefore(pllSettings,
                                        pNew,
                                        ulBeforeIndex);

                // output data, if caller wants this
                if (pulNewItemCount)
                    *pulNewItemCount = lstCountItems(pllSettings);
                if (pulNewWidgetIndex)
                    *pulNewWidgetIndex = lstIndexFromItem(pllSettings, pNew);
            }

            if (!arc)
                *ppNewSetting = pNew;
            else
                ctrpFreeSettingData(&pNew);
        }
        else
            arc = ERROR_NOT_ENOUGH_MEMORY;
    }
    else
        arc = ERROR_INVALID_PARAMETER;

    return (arc);
}

/*
 *@@ ctrpFindWidgetSetting:
 *      returns a PRIVATEWIDGETSETTING for the given widget
 *      index.
 *
 *      If a widget setting is found and (ppViewData != NULL),
 *      this also attempts to find the current view for the
 *      widget (see below).
 *
 *      Note that the index parameters work in two modes:
 *
 *      --  If (WIDGETPOSITION.ulTrayWidgetIndex == -1), this
 *          finds a "root" widget, whose index must be specified
 *          in ulWidgetIndex. ulTrayIndex is ignored.
 *
 *      --  If (WIDGETPOSITION.ulTrayWidgetIndex != -1), it is
 *          assumed to contain the index of a "root" tray widget.
 *          In that case, WIDGETPOSITION.ulTrayIndex specifies the
 *          tray in that tray widget and WIDGETPOSITION.ulWidgetIndex
 *          the subwidget index in that tray.
 *
 *      *ppViewData receives a pointer to the currently open
 *      widget view, if any. This will be set to NULL if
 *
 *      --  an error is returned in the first place;
 *
 *      --  the XCenter isn't currently open at all;
 *
 *      --  the widget doesn't currently exist as a view,
 *          e.g. because it is in a tray that is not currently
 *          switched to.
 *
 *      Preconditions:
 *
 *      --  Caller must lock the XCenter.
 *
 *      Returns:
 *
 *      --  NO_ERROR
 *
 *      --  XCERR_INVALID_ROOT_WIDGET_INDEX: the specified
 *          root widget index is too large.
 *
 *      --  XCERR_ROOT_WIDGET_INDEX_IS_NO_TRAY: in "tray" mode,
 *          the specified tray widget index specifies no
 *          tray.
 *
 *      --  XCERR_INVALID_TRAY_INDEX: in "tray" mode, the specified
 *          tray index is too large.
 *
 *      --  XCERR_INVALID_SUBWIDGET_INDEX: in "tray" mode, the
 *          specified subwidget index is too large.
 *
 *@@added V0.9.14 (2001-08-01) [umoeller]
 *@@changed V0.9.16 (2001-10-18) [umoeller]: mostly rewritten
 *@@changed V0.9.16 (2001-12-31) [umoeller]: now using WIDGETPOSITION struct
 */

APIRET ctrpFindWidgetSetting(XCenter *somSelf,
                             PWIDGETPOSITION pPosition,  // in: widget position
                             PPRIVATEWIDGETSETTING *ppSetting, // out: widget setting
                             PXCENTERWIDGET *ppViewData) // out: view data or NULL; ptr can be NULL
{
    APIRET arc = NO_ERROR;

    PLINKLIST   pllWidgets = ctrpQuerySettingsList(somSelf);

    if (ppSetting)
        *ppSetting = NULL;
    if (ppViewData)
        *ppViewData = NULL;

    if (pPosition->ulTrayWidgetIndex == -1)
    {
        // root widget:
        if (!(*ppSetting = lstItemFromIndex(pllWidgets,
                                            pPosition->ulWidgetIndex)))
            arc = XCERR_INVALID_ROOT_WIDGET_INDEX;
        else
        {
            XCenterData *somThis = XCenterGetData(somSelf);
            PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)_pvOpenView;
                                    // can be NULL if we have no open view

            if (    (ppViewData)            // caller wants open view
                 && (pXCenterData)          // and we have an open view:
               )
            {
                *ppViewData = (PXCENTERWIDGET)lstItemFromIndex(&pXCenterData->llWidgets,
                                                               pPosition->ulWidgetIndex);
            }
        }
    }
    else
    {
        PPRIVATEWIDGETSETTING   pTrayWidgetSetting;
        PTRAYSETTING            pTraySetting;
        PXCENTERWIDGET          pTrayWidgetView = NULL;
        if (!(arc = ctrpFindTraySetting(somSelf,
                                        pPosition->ulTrayWidgetIndex,
                                        pPosition->ulTrayIndex,
                                        &pTrayWidgetSetting,
                                        &pTraySetting,
                                        // get widget view if caller wants view data:
                                        (ppViewData)
                                            ? &pTrayWidgetView
                                            // pTrayWidgetView is set to tray widget view
                                            // only if specified tray is currently
                                            // switched to
                                            : NULL)))
        {
            if (!(*ppSetting = lstItemFromIndex(&pTraySetting->llSubwidgetSettings,
                                                 pPosition->ulWidgetIndex)))
                arc = XCERR_INVALID_SUBWIDGET_INDEX;
            else
                // all OK:
                if (    (pTrayWidgetView)       // caller wants view, and we have one,
                                                // and specified tray is currently
                                                // switched to:
                     && (((PPRIVATEWIDGETVIEW)pTrayWidgetView)->pllSubwidgetViews)
                   )
                    *ppViewData = (PXCENTERWIDGET)lstItemFromIndex(((PPRIVATEWIDGETVIEW)pTrayWidgetView)->pllSubwidgetViews,
                                                                   pPosition->ulWidgetIndex);
        }
    }

    return (arc);
}

/*
 *@@ ctrpFreeSettingData:
 *      frees the data in the specified widget setting,
 *      including tray and subwidget definitions,
 *      and also the *ppSetting structure itself.
 *      Afterwards *ppSetting is set to NULL for safety.
 *
 *@@added V0.9.13 (2001-06-21) [umoeller]
 *@@changed V0.9.16 (2001-10-18) [umoeller]: now freeing pSetting too, changed prototype accordingly
 */

VOID ctrpFreeSettingData(PPRIVATEWIDGETSETTING *ppSetting)
{
    PPRIVATEWIDGETSETTING pSetting;

    if (    (ppSetting)
         && (pSetting = *ppSetting)
       )
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
                pSubwidgetNode = lstQueryFirstNode(&pTraySetting->llSubwidgetSettings);
                while (pSubwidgetNode)
                {
                    PPRIVATEWIDGETSETTING pSubwidget = (PPRIVATEWIDGETSETTING)pSubwidgetNode->pItemData;
                    // recurse into that subwidget; this won't have
                    // a linked list of tray again, but we can reuse
                    // the free() code above
                    ctrpFreeSettingData(&pSubwidget);

                    pSubwidgetNode = pSubwidgetNode->pNext;
                }

                free(pTraySetting);

                pTrayNode = pTrayNode->pNext;
            }

            // nuke the trays list (nodes left, item data has been freed)
            lstFree(&pSetting->pllTraySettings);
        }

        free(pSetting);         // V0.9.16 (2001-10-18) [umoeller]
        *ppSetting = NULL;
    }
}

/*
 *@@ ctrpRemoveWidgetSetting:
 *      deletes the specified widget setting,
 *      which can either be a subwidget or a
 *      root widget.
 *
 *      This does not save the widget settings.
 *
 *      Returns:
 *
 *      --  NO_ERROR
 *
 *      --  ERROR_INVALID_PARAMETER
 *
 *      --  XCERR_INTEGRITY
 *
 *      Preconditions:
 *
 *      --  Caller must hold the XCenter's mutex.
 *
 *      --  The (sub)widget's window must have been destroyed first.
 *
 *@@added V0.9.19 (2002-05-04) [umoeller]
 */

APIRET ctrpRemoveWidgetSetting(XCenter *somSelf,
                               PPRIVATEWIDGETSETTING *ppSetting)     // in: widget to delete
{
    APIRET arc = NO_ERROR;
    PPRIVATEWIDGETSETTING pSetting;

    if (!(pSetting = *ppSetting))
        return ERROR_INVALID_PARAMETER;

    if (pSetting->pOwningTray)
    {
        // this is a subwidget:
        if (!lstRemoveItem(&pSetting->pOwningTray->llSubwidgetSettings,
                           pSetting))
            arc = XCERR_INTEGRITY;
    }
    else
    {
        // this is a root widget:
        PLINKLIST   pllWidgetSettings = ctrpQuerySettingsList(somSelf);
        if (!lstRemoveItem(pllWidgetSettings,
                           pSetting))
            arc = XCERR_INTEGRITY;
    }

    if (!arc)
        // now clean up
        ctrpFreeSettingData(ppSetting);

    return (arc);
}

/* ******************************************************************
 *
 *   Tray management
 *
 ********************************************************************/

/*
 *@@ ctrpCreateTraySetting:
 *      creates a new (empty) tray setting with the specified
 *      name in a tray widget. The new tray setting is appended
 *      to the end of the list of the tray widget's
 *      trays.
 *
 *      Does not switch to the tray yet since the model
 *      doesn't care about the tray's current tray.
 *
 *      This does not save the widget settings.
 *
 *@@added V0.9.13 (2001-06-21) [umoeller]
 *@@changed V0.9.19 (2002-04-25) [umoeller]: renamed from ctrpCreateTray
 */

PTRAYSETTING ctrpCreateTraySetting(PPRIVATEWIDGETSETTING ppws, // in: private tray widget setting
                                   PCSZ pcszTrayName,          // in: tray name
                                   PULONG pulIndex)            // out: index of new tray
{
    PTRAYSETTING pNewTray;

    if (pNewTray = NEW(TRAYSETTING))
    {
        ZERO(pNewTray);

        pNewTray->pszTrayName = strhdup(pcszTrayName, NULL);

        lstInit(&pNewTray->llSubwidgetSettings, FALSE);

        // append to widget setup
        lstAppendItem(ppws->pllTraySettings, pNewTray);

        if (pulIndex)
            *pulIndex = lstCountItems(ppws->pllTraySettings) - 1;
    }

    return (pNewTray);
}

/*
 *@@ ctrpFindTraySetting:
 *      returns a TRAYSETTING for the given tray widget
 *      and tray indices.
 *
 *      If a tray setting is found and (ppViewData != NULL),
 *      this also attempts to find the current view for the
 *      tray widget (see below).
 *
 *      On input, ulTrayWidgetIndex is assumed to contain the
 *      index of a "root" tray widget. ulTrayIndex specifies the
 *      tray in that tray widget.
 *
 *      *ppViewData receives a pointer to the currently open
 *      widget view, if any. This will be set to NULL if
 *
 *      --  an error is returned in the first place;
 *
 *      --  the XCenter isn't currently open at all;
 *
 *      --  the specified tray is not currently switched to.
 *
 *      In other words, *ppViewData is set only if the
 *      NO_ERROR is returned _and_ the given tray is currently
 *      showing in the tray.
 *
 *      Returns:
 *
 *      --  NO_ERROR
 *
 *      --  XCERR_INVALID_ROOT_WIDGET_INDEX: the specified
 *          (root) tray widget index is too large.
 *
 *      --  XCERR_ROOT_WIDGET_INDEX_IS_NO_TRAY:
 *          the specified (root) tray widget index specifies
 *          no tray.
 *
 *      --  XCERR_INVALID_TRAY_INDEX: the specified tray
 *          index is too large.
 *
 *@@added V0.9.19 (2002-04-25) [umoeller]
 */

APIRET ctrpFindTraySetting(XCenter *somSelf,
                           ULONG ulTrayWidgetIndex,     // in: root index of tray widget
                           ULONG ulTrayIndex,           // in: tray index in tray widget
                           PPRIVATEWIDGETSETTING *ppTrayWidgetSetting,
                                                        // out: tray widget setting if NO_ERROR
                           PTRAYSETTING *ppTraySetting, // out: tray setting if NO_ERROR
                           PXCENTERWIDGET *ppTrayWidget)  // out: tray widget view data or NULL; ptr can be NULL
{
    APIRET      arc = NO_ERROR;
    PLINKLIST   pllWidgets = ctrpQuerySettingsList(somSelf);

    PPRIVATEWIDGETSETTING pTrayWidgetSetting;
    if (!(pTrayWidgetSetting = lstItemFromIndex(pllWidgets,
                                                ulTrayWidgetIndex)))
        // this doesn't exist at all:
        arc = XCERR_INVALID_ROOT_WIDGET_INDEX;
    else if (!(pTrayWidgetSetting->pllTraySettings))
        // this is no tray widget:
        arc = XCERR_ROOT_WIDGET_INDEX_IS_NO_TRAY;
    else if (!(*ppTraySetting = lstItemFromIndex(pTrayWidgetSetting->pllTraySettings,
                                                 ulTrayIndex)))
        arc = XCERR_INVALID_TRAY_INDEX;
    else
    {
        XCenterData *somThis = XCenterGetData(somSelf);
        PXCENTERWINDATA pXCenterData = (PXCENTERWINDATA)_pvOpenView;
                                // can be NULL if we have no open view

        if (    (ppTrayWidget)            // caller wants view
             && (pXCenterData)          // and we have an open view:
           )
        {
            PPRIVATEWIDGETVIEW pTrayWidgetView;
            if (    (pTrayWidgetView = lstItemFromIndex(&pXCenterData->llWidgets,
                                                        ulTrayWidgetIndex))
                    // is the desired tray active?
                 && (pTrayWidgetSetting->ulCurrentTray == ulTrayIndex)
               )
            {
                *ppTrayWidget = (PXCENTERWIDGET)pTrayWidgetView;
            }
        }
    }

    return arc;
}

/*
 *@@ ctrpDeleteTraySetting:
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
 *@@changed V0.9.19 (2002-05-04) [umoeller]: prototype changed, renamed
 */

BOOL ctrpDeleteTraySetting(XCenter *somSelf,
                           PPRIVATEWIDGETSETTING ppws, // in: private tray widget setting
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
            pSubwidgetNode = lstQueryFirstNode(&pTray->llSubwidgetSettings);
            while (pSubwidgetNode)
            {
                PLISTNODE pNext = pSubwidgetNode->pNext;
                PPRIVATEWIDGETSETTING pSetting = (PPRIVATEWIDGETSETTING)pSubwidgetNode->pItemData;

                ctrpRemoveWidgetSetting(somSelf,
                                        &pSetting);

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

/*
 *@@ ctrpQueryWidgetsCount:
 *      returns the no. of root-level widgets on
 *      the widget list.
 *
 *      The caller must lock the XCenter himself.
 *
 *@@added V0.9.16 (2001-10-15) [umoeller]
 */

ULONG ctrpQueryWidgetsCount(XCenter *somSelf)
{
    PLINKLIST pll;
    if (pll = ctrpQuerySettingsList(somSelf))
        return (lstCountItems(pll));

    return 0;
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
    WPSHLOCKSTRUCT Lock = {0};
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
                    ctrpFreeSettingData(&pSetting);

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

/* ******************************************************************
 *
 *   Settings stuffing
 *
 ********************************************************************/

#define TRAYSETTINGSMARKER "(#@@TRAY&_/)!"        // whatever

/*
 *@@ ctrpStuffSettings:
 *      packs all the settings into a binary array
 *      which can be saved with wpSaveState.
 *
 *      The format is defined as two strings alternating
 *      for each widget:
 *
 *      1)  the PM window class name
 *
 *      2)  then the setup string (or a space if there's none).
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
                    if (pSubwidgetNode = lstQueryFirstNode(&pTray->llSubwidgetSettings))
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

static VOID DecodeSubwidgets(PSZ p,                    // in: entire subwidgets substring (between [])
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

        // _Pmpf((__FUNCTION__ ": decoding widgets for tray %s", pTray->pszTrayName));

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

                /* _Pmpf(("  got subwidget %s, %s",
                        (pNewSubwidget->Public.pszWidgetClass)
                            ? pNewSubwidget->Public.pszWidgetClass
                            : "NULL",
                        (pNewSubwidget->Public.pszSetupString)
                            ? pNewSubwidget->Public.pszSetupString
                            : "NULL")); */

                lstAppendItem(&pTray->llSubwidgetSettings,
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

static VOID DecodeTraySettings(PSZ p,
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

                lstInit(&pNewTray->llSubwidgetSettings, FALSE);

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
 *@@changed V0.9.16 (2001-10-18) [umoeller]: adjusted for new model functions
 *@@changed V0.9.16 (2001-10-18) [umoeller]: fixed bad loop if two trays were next to each other
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
                // _Pmpf((__FUNCTION__ ": found tray settings %s", p));

                if (pPrevSetting)
                    DecodeTraySettings(p + sizeof(TRAYSETTINGSMARKER) - 1,
                                       ulClassLen,
                                       pPrevSetting);

                // go beyond the terribly long tray string
                p += ulClassLen + 1;    // go beyond null byte

                // now we're at the real class name...

                // V0.9.16 (2001-10-18) [umoeller]:
                // this used to continue with the below code, which
                // failed miserably if two trays were right behind
                // each other in the list... so loop again!!
            }
            else
            {
                // no special tray string:
                // create a regular widget setting then
                const char  *pcszWidgetClass = p,
                            *pcszSetupString;

                // after class name, we have the setup string
                p += ulClassLen + 1;    // go beyond null byte

                ulSetupLen = strlen(p);
                // single space means no setup string
                if (ulSetupLen > 1)
                    pcszSetupString = p;
                else
                    pcszSetupString = NULL;

                if (ctrpCreateWidgetSetting(somSelf,
                                            NULL,           // tray
                                            pcszWidgetClass,
                                            pcszSetupString,
                                            -1,             // ulbeforeIndex
                                            &pPrevSetting,  // out: new setting
                                                    // (store this for next loop, in
                                                    // case it's a tray)
                                            NULL,
                                            NULL))
                    // error:
                    break;

                // go for next setting
                p += ulSetupLen + 1;    // go beyond null byte
            }
        } // end while (    (*p)  ...

        // remove the packed settings;
        // from now on we'll use the list
        free(_pszPackedWidgetSettings);
        _pszPackedWidgetSettings = NULL;

        ctrpFreeClasses();
    }

    return (ulrc);
}


