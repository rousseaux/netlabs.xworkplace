
/*
 *@@sourcefile object.c:
 *      implementation code for XFldObject class.
 *
 *      This file is ALL new with V0.9.0.
 *
 *      This looks like a good place for general explanations
 *      about how the WPS maintains Desktop objects as SOM objects
 *      in memory.
 *
 *      Basically, there are two situations where objects are
 *      created and destroyed.
 *
 *      1)  Scenario 1 involves creating and destroying objects
 *          in memory ONLY without affecting the physical
 *          storage of the object. For abstract objects,
 *          "physical storage" is the OS2.INI file; for file-system
 *          objects, the file system. WPTransients have no physical
 *          storage at all.
 *
 *          The WPS calls this "make awake" and "make dormant".
 *          Objects are most frequently made awake when a folder
 *          is populated (mostly on folder open). Of course this
 *          does not physically create objects... they are only
 *          instantiated in memory then from their physical
 *          storage.
 *
 *          There are many ways to awake objects. To awake a
 *          single object, call M_WPObject::wpclsMakeAwake.
 *          This is a bit difficult to manage, so
 *          WPFileSystem::wpclsQueryObjectFromPath is easier
 *          for waking up FS objects.
 *
 *          WPFolder::wpPopulate calls these in turn somehow.
 *
 *          Even though this isn't documented anywhere, the
 *          WPS also supports the reverse concept of making
 *          the object dormant again. This will destroy the
 *          SOM object in memory, but not the physical representation.
 *
 *          I suspect this was not documented because you can
 *          never know whether some code still needs the
 *          SOM pointer to the object somehow. Anyway, the
 *          WPS _does_ make objects dormant again, e.g. when
 *          their folders are closed and they are not referenced
 *          from anywhere else. You can prevent the WPS from doing
 *          this by calling WPObject::wpLock.
 *
 *          The interesting thing is that there is an undocumented
 *          method for destroying the SOM object.
 *          WPObject::wpMakeDormant does exactly this.
 *          Actually, this does a lot of things:
 *
 *          --  It removes the object from all containers,
 *
 *          --  closes all views (wpClose),
 *
 *          --  frees all associated memory allocated thru wpAllocMem.
 *
 *          --  In addition, if the object has called _wpSaveDeferred
 *              and a _wpSaveImmediate is thus pending, _wpSaveImmediate
 *              also gets called. (See XFldObject::wpSaveDeferred
 *              for more about this.)
 *
 *          --  Finally, wpMakeDormant calls wpUnInitData, which
 *              should clean up the object.
 *
 *      2)  Scenario 2 means that an object is physically created
 *          or destroyed through the WPS.
 *
 *          An object is created through either M_WPObject::wpclsNew
 *          or WPObject::wpCopyObject or WPObject::wpCreateAnother.
 *          These are the "object factories" of the WPS. Depending
 *          on which class the method is invoked on, the new object
 *          will be of that class.
 *
 *          Depending on the object's class, wpclsNew will create
 *          a physical representation (e.g. file, folder) of the
 *          object _and_ a SOM object.
 *
 *          Deleting an object can really be done in two ways:
 *
 *          --  WPObject::wpDelete looks like the most natural
 *              way. However this really only displays a
 *              confirmation and then invokes WPObject::wpFree.
 *
 *          --  WPObject::wpFree is the most direct way to
 *              delete an object. This does not display any
 *              more confirmations (in theory), but deletes
 *              the object right away.
 *
 *              Interestingly, wpFree in turn calls another
 *              undocumented method -- WPObject::wpDestroyObject.
 *              From my testing this is responsible for destroying
 *              the physical representation (file, folder, INI data).
 *
 *              After that, wpFree also calls wpMakeDormant
 *              to free the SOM object.
 *
 *      wpDestroyObject is a bit obscure. I believe it is this
 *      method which was supposed to do the object cleanup.
 *      It is introduced by WPObject and overridden by the
 *      following classes:
 *
 *      --  WPFileSystem: apparently, this then does DosDelete.
 *          Unfortunately, this one has a real nasty bug... it
 *          displays a message box if deleting the object fails.
 *          This is really annoying when calling wpFree in a loop
 *          on a bunch of objects. That's why we have
 *          XFldObject::xwpNukePhysical now.
 *
 *      --  WPAbstract: this probably removes the INI entries
 *          associated with the abstract object.
 *
 *      --  WPProgram.
 *
 *      --  WPProgramFile.
 *
 *      --  WPTransient.
 *
 *      If folder auto-refresh is replaced by XWP, we must override
 *      wpFree in order to suppress calling this method. The message
 *      box bug is not acceptable for file-system objects, so we have
 *      introduced XFldObject::xwpNukePhysical instead.
 *
 *      The destruction call sequence thus is:
 *
 +          wpDelete (display confirmation, if applicable)
 +             |
 +             +-- wpFree (really delete the object now)
 +                   |
 +                   +-- wpDestroyObject (delete physical storage)
 +                   |
 +                   +-- wpMakeDormant (delete SOM object in memory)
 +                         |
 +                         +-- (lots of cleanup: wpClose, etc.)
 +                         |
 +                         +-- wpSaveImmediate (if "dirty")
 +                         |
 +                         +-- wpUnInitData
 *
 *      Function prefix for this file:
 *      --  obj*
 *
 *@@added V0.9.0 [umoeller]
 *@@header "filesys\object.h"
 */

/*
 *      Copyright (C) 1997-2001 Ulrich M”ller.
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
#define INCL_DOSEXCEPTIONS
#define INCL_DOSSEMAPHORES
#define INCL_DOSRESOURCES
#define INCL_DOSMISC
#define INCL_DOSERRORS

#define INCL_WININPUT
#define INCL_WINWINDOWMGR
#define INCL_WINMESSAGEMGR
#define INCL_WINPOINTERS
#define INCL_WINSYS
#define INCL_WINTIMER
#define INCL_WINMENUS
#define INCL_WINDIALOGS
#define INCL_WINSTATICS
#define INCL_WINBUTTONS
#define INCL_WINENTRYFIELDS
#define INCL_WINMLE
#define INCL_WINSTDCNR
#define INCL_WINSTDBOOK
#define INCL_WINPROGRAMLIST     // needed for wppgm.h
#define INCL_WINSHELLDATA
#include <os2.h>

// C library headers
#include <stdio.h>
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\apps.h"               // application helpers
// #include "helpers\comctl.h"             // common controls (window procs)
// #include "helpers\cnrh.h"               // container helper routines
// #include "helpers\dialog.h"             // dialog helpers
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\except.h"             // exception handling
// #include "helpers\gpih.h"               // GPI helper routines
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\standards.h"          // some standard macros
#include "helpers\stringh.h"            // string helper routines
#include "helpers\tree.h"               // red-black binary trees
#include "helpers\winh.h"               // PM helper routines
// #include "helpers\wphandle.h"           // file-system object handles
#include "helpers\xstring.h"            // extended string helpers

// SOM headers which don't crash with prec. header files
#include "xfldr.ih"
#include "xfobj.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\cnrsort.h"             // container sort comparison functions
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\helppanels.h"          // all XWorkplace help panel IDs
#include "shared\notebook.h"            // generic XWorkplace notebook handling
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

// headers in /hook
#include "hook\xwphook.h"

#include "filesys\filesys.h"            // various file-system object implementation code
#include "filesys\object.h"             // XFldObject implementation

#include "config\hookintf.h"            // daemon/hook interface

// other SOM headers
#pragma hdrstop
// #include <wpdesk.h>                     // WPDesktop; includes WPFolder also
// #include <wppgm.h>                      // WPProgram
// #include <wppgmf.h>                     // WPProgramFile
// #include <wpshadow.h>                   // WPShadow
#include <wptrans.h>                    // WPTransient
#include <wpclsmgr.h>                   // WPClassMgr
// #include "filesys\folder.h"             // XFolder implementation
// #include "filesys\program.h"            // program implementation

#include "helpers\undoc.h"              // some undocumented stuff

/* ******************************************************************
 *
 *   Private declarations
 *
 ********************************************************************/

/*
 *@@ OBJTREENODE:
 *      tree node structure for object handles cache.
 *
 *@@added V0.9.9 (2001-04-02) [umoeller]
 */

typedef struct _OBJTREENODE
{
    TREE        Tree;
    WPObject    *pObject;
    ULONG       ulReferenced;       // system uptime count when last referenced
} OBJTREENODE, *POBJTREENODE;

#define CACHE_ITEM_LIMIT    200

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

// mutex semaphores for object lists (favorite folders, quick-open)
static HMTX        G_hmtxObjectsLists = NULLHANDLE;

// object handles cache
static TREE        *G_HandlesCache;
static HMTX        G_hmtxHandlesCache = NULLHANDLE;
static LONG        G_lHandlesCacheItemsCount = 0;

// dirty objects list
static TREE        *G_DirtyList;
static HMTX        G_hmtxDirtyList = NULLHANDLE;
static LONG        G_lDirtyListItemsCount = 0;

/* ******************************************************************
 *
 *   Object creation/destruction
 *
 ********************************************************************/

/*
 *@@ objFree:
 *      implementation for XFldObject::wpFree.
 *
 *      If folder auto-refresh has been replaced by XWP, we override
 *      the entire WPObject::wpFree method without calling the parent.
 *      This is necessary for a number of reasons:
 *
 *      --  wpFree in turn calls the undocumented wpDestroyObject
 *          method, which has a number of bugs but cannot be overridden
 *          (because it's not exported in the IDL file).
 *
 *          So in order to fix those bugs, wpFree had to be rewritten,
 *          which now calls the new XFldObject::xwpNukePhysical method
 *          (which is our reimplementation of wpDestroyObject).
 *
 *      --  wpFree is not very good at cleaning up object data in the
 *          INI files. This needs to be fixed finally.
 *
 *      In other words, when wpFree is now invoked on an object,
 *      the following happens:
 *
 *      1) XFldObject::wpFree calls this implementation (objFree).
 *
 *      2) objFree cleans up some object data and then calls
 *         the xwpNukePhysical method, using SOM name-lookup
 *         resolution. This allows subclasses (such as XFolder
 *         and XFldDataFile) to override the method, even though
 *         SOM doesn't know that XFldObject is actually a parent
 *         class of those subclasses (since the IDL files do not
 *         reflect that WPObject has been replaced with XFldObject).
 *
 *      3) XFldObject::xwpNukePhysical is the standard implementation,
 *         which invokes the standard undocumented wpDestroyObject
 *         method.
 *
 *      In summary, for classes which have not overridden xwpNukeObject,
 *      the behavior is EXACTLY as with the standard WPObject::wpFree.
 *
 *      HOWEVER, this way we can override xwpNukeObject in
 *      XFldDataFile and XFolder to fix the annoying message box
 *      bugs.
 *
 *@@added V0.9.9 (2001-02-04) [umoeller]
 */

BOOL objFree(WPObject *somSelf)
{
    BOOL    brc = FALSE;

    ULONG   ulStyle = _wpQueryStyle(somSelf);
    PSZ     pszID = _wpQueryObjectID(somSelf);
    somTD_XFldObject_xwpNukePhysical pxwpNukePhysical = NULL;
    xfTD_wpDeleteWindowPosKeys _wpDeleteWindowPosKeys = NULL;
    xfTD_wpMakeDormant _wpMakeDormant = NULL;

    // if the object has an object ID assigned, remove this...
    // this should clean the INI entry
    if (pszID && strlen(pszID))
        _wpSetObjectID(somSelf, NULL);

    // if the object is a template, unset that bit...
    // this should clean the INI entry as well
    if (ulStyle & OBJSTYLE_TEMPLATE)
        _wpModifyStyle(somSelf, OBJSTYLE_TEMPLATE, 0);

    // OK, here comes the fun stuff.
    // The WPS normally calls the "wpDestroyObject" method,
    // which is responsible for killing the physical representation
    // of the object. Unfortunately, we cannot override that method
    // because IBM wasn't kind enough to make it public. Our way
    // around this (without breaking compatibility) is to introduce
    // a new method in XFldObject, which calls wpDestroyObject per
    // default.

    // We resolve the method by name because it is overridden
    // in some XWorkplace classes. If it is not overridden,
    // XFldObject::xwpNukePhysical calls wpDestroyObject.
    if (pxwpNukePhysical = (somTD_XFldObject_xwpNukePhysical)somResolveByName(
                              somSelf,
                              "xwpNukePhysical"))
        pxwpNukePhysical(somSelf);

    // the WPS then calls wpSaveImmediate just in case the object
    // has called wpSaveDeferred. I'm not sure this is a good idea...
    // this will add another entry to the INI file. This should be
    // moved up.
    _wpSaveImmediate(somSelf);

    // then there's another undocumented method call... i'm unsure
    // what this does, but what the heck. We need to resolve this
    // manually.
    if (_wpDeleteWindowPosKeys = (xfTD_wpDeleteWindowPosKeys)wpshResolveFor(
                                            somSelf,
                                            NULL,
                                            "wpDeleteWindowPosKeys"))
        _wpDeleteWindowPosKeys(somSelf);

    // finally, this calls wpMakeDormant, which destroys the SOM object
    if (_wpMakeDormant = (xfTD_wpMakeDormant)wpshResolveFor(
                                            somSelf,
                                            NULL,
                                            "wpMakeDormant"))
        brc = _wpMakeDormant(somSelf, 0);

    return (brc);
}

/* ******************************************************************
 *
 *   Object linked lists
 *
 ********************************************************************/

/*
 *@@ LockObjectsList:
 *      locks G_hmtxFolderLists. Creates the mutex on
 *      the first call.
 *
 *      Returns TRUE if the mutex was obtained.
 *
 *@@added V0.9.7 (2001-01-18) [umoeller]
 */

BOOL LockObjectsList(VOID)
{
    BOOL brc = FALSE;

    if (G_hmtxObjectsLists == NULLHANDLE)
        brc = !DosCreateMutexSem(NULL,
                                 &G_hmtxObjectsLists,
                                 0,
                                 TRUE);
    else
        brc = !WinRequestMutexSem(G_hmtxObjectsLists, SEM_INDEFINITE_WAIT);

    return (brc);
}

/*
 *@@ UnlockObjectsList:
 *      the reverse to LockObjectsLists.
 *
 *@@added V0.9.7 (2001-01-18) [umoeller]
 */

VOID UnlockObjectsList(VOID)
{
    DosReleaseMutexSem(G_hmtxObjectsLists);
}

/*
 *@@ LoadObjectsList:
 *      adds WPObject pointers to pll according to the
 *      specified INI key.
 *
 *      If (ulListFlag != 0), this invokes
 *      WPObject::xwpModifyListNotify to set that flag
 *      on each object that was added to the list.
 *
 *      Preconditions: caller must lock the list.
 *
 *@@added V0.9.7 (2001-01-18) [umoeller]
 *@@changed V0.9.9 (2001-03-19) [pr]: tidied up
 *@@changed V0.9.9 (2001-03-27) [umoeller]: added OBJECTLIST encapsulation
 *@@changed V0.9.16 (2001-10-11) [umoeller]: rewritten
 */

BOOL LoadObjectsList(POBJECTLIST pll,
                     ULONG ulListFlag,          // in: list flag for xwpModifyListNotify
                     const char *pcszIniKey)
{
    BOOL        brc = FALSE;
    // ULONG       ulSize;
    PSZ         pszHandles = NULL;

    TRY_LOUD(excpt1)
    {
        if (pszHandles = prfhQueryProfileData(HINI_USERPROFILE,
                                              INIAPP_XWORKPLACE,
                                              pcszIniKey,
                                              NULL))
        {
            PCSZ pTemp = pszHandles;
            ULONG ulCharsLeft;

            while (     (pTemp)
                     && (*pTemp)
                  )
            {
                HOBJECT          hObject;
                if (hObject = strtol(pTemp,
                                     NULL,
                                     16))
                {
                    PCSZ        p;
                    WPObject    *pobj;
                    if (    (pobj = _wpclsQueryObject(_WPFolder, hObject))
                         && (wpshCheckObject(pobj))
                       )
                    {
                        // object is already locked
                        lstAppendItem(&pll->ll,
                                      pobj);
                        if (ulListFlag)
                        {
                            // set list notify flag
                            _xwpModifyListNotify(pobj,
                                                 ulListFlag,        // set
                                                 ulListFlag);       // mask
                        }
                    }

                    // find next space
                    if (p = strchr(pTemp, ' '))
                    {
                        // skip spaces
                        while (*p && *p == ' ')
                            p++;
                        pTemp = p;          // points to either next non-space
                                            // or null char now
                    }
                    else
                        // no more spaces:
                        break;
                }
                else
                    // cannot get object string:
                    break;
            };
        }

        pll->fLoaded = TRUE;
    }
    CATCH(excpt1)
    {
        brc = FALSE;
    } END_CATCH();

    if (pszHandles)
        free(pszHandles);

    return (brc);
}

/*
 *@@ WriteObjectsList:
 *
 *      This creates a handle for each object on the
 *      list.
 *
 *      Preconditions: caller must lock the list.
 *
 *@@added V0.9.7 (2001-01-18) [umoeller]
 *@@changed V0.9.9 (2001-03-27) [umoeller]: added OBJECTLIST encapsulation
 */

BOOL WriteObjectsList(POBJECTLIST pll,
                      const char *pcszIniKey)
{
    BOOL brc = FALSE;

    if (pll->fLoaded)
    {
        PLISTNODE pNode = lstQueryFirstNode(&pll->ll);
        if (pNode)
        {
            // list is not empty: recompose string
            XSTRING strTemp;
            xstrInit(&strTemp, 100);

            while (pNode)
            {
                CHAR szHandle[30];
                WPObject *pobj = (WPObject*)pNode->pItemData;
                sprintf(szHandle,
                        "%lX ",         // note the space
                        _wpQueryHandle(pobj));

                xstrcat(&strTemp, szHandle, 0);

                pNode = pNode->pNext;
            }

            brc = PrfWriteProfileString(HINI_USERPROFILE,
                                        (PSZ)INIAPP_XWORKPLACE, (PSZ)pcszIniKey,
                                        strTemp.psz);

            xstrClear(&strTemp);
        }
        else
        {
            // list is empty: remove
            brc = PrfWriteProfileData(HINI_USERPROFILE,
                                      (PSZ)INIAPP_XWORKPLACE, (PSZ)pcszIniKey,
                                      NULL, 0);
        }
    }

    return (brc);
}

/*
 *@@ objAddToList:
 *      appends or removes the given folder to or from
 *      the given object list.
 *
 *      An "object list" is an abstract concept of a list
 *      of object which is stored in OS2.INI (in the
 *      "XWorkplace" section under the pcszIniKey key).
 *
 *      If fInsert == TRUE, somSelf is added to the list.
 *      If fInsert == FALSE, somSelf is removed from the list.
 *
 *      Returns TRUE if the list was changed. In that case,
 *      the list is rewritten to the specified INI key.
 *
 *      The list is protected by a global mutex semaphore,
 *      so this is thread-safe.
 *
 *      WARNING: If an object is added to such a list, the
 *      caller must make sure that the object gets removed
 *      when the object goes dormant (e.g. when it's deleted).
 *
 *      This is used from the XFolder methods for
 *      "favorite" and "quick-open" folders. The linked
 *      list roots are stored in classes\xfldr.c.
 *
 *@@added V0.9.0 (99-11-16) [umoeller]
 *@@changed V0.9.7 (2001-01-18) [umoeller]: made this generic for all objs... renamed from fdr*
 *@@changed V0.9.7 (2001-01-18) [umoeller]: removed mallocs(), this wasn't needed
 *@@changed V0.9.9 (2001-01-29) [lafaix]: wrong object set, fixed
 *@@changed V0.9.9 (2001-03-19) [pr]: lock/unlock objects on the lists
 *@@changed V0.9.9 (2001-03-27) [umoeller]: added OBJECTLIST encapsulation
 *@@changed V0.9.14 (2001-07-31) [pr]: fixed confusing code
 */

BOOL objAddToList(WPObject *somSelf,
                  POBJECTLIST pll,        // in: linked list of WPObject* pointers
                  BOOL fInsert,
                  const char *pcszIniKey,
                  ULONG ulListFlag)     // in: list flag for xwpModifyListNotify
{
    BOOL    brc = FALSE,
            fSemOwned = FALSE;

    TRY_LOUD(excpt1)
    {
        fSemOwned = LockObjectsList();
        if (fSemOwned)
        {
            PLISTNODE   pNode = 0,
                        pNodeFound = 0;
            WPObject    *pobj;

            if (!pll->fLoaded)
                // if the list has not yet been built
                // yet, we will do this now
                LoadObjectsList(pll,
                                ulListFlag,
                                pcszIniKey);

            // find folder on list
            pNode = lstQueryFirstNode(&pll->ll);
            while (pNode)
            {
                pobj = pNode->pItemData;

                if (pobj == somSelf)
                {
                    pNodeFound = pNode;
                    break;
                }

                pNode = pNode->pNext;
            }

            if (fInsert)
            {
                // insert mode:
                if (!pNodeFound)
                {
                    // lock object to stop it going dormant
                    _wpLockObject(somSelf);
                    // not on list yet: append
                    lstAppendItem(&pll->ll,
                                  somSelf);

                    if (ulListFlag)
                        // set list notify flag
                        _xwpModifyListNotify(somSelf, // V0.9.9 (2001-01-29) [lafaix]
                                             ulListFlag,        // set
                                             ulListFlag);       // mask

                    brc = TRUE;
                }
            }
            else
            {
                // remove mode:
                if (pNodeFound)
                {
                    lstRemoveNode(&pll->ll,
                                  pNodeFound);
                    // unlock object to allow it to go dormant
                    _wpUnlockObject(somSelf);

                    if (ulListFlag)
                        // unset list notify flag
                        _xwpModifyListNotify(somSelf,
                                             0,                 // clear
                                             ulListFlag);       // mask

                    brc = TRUE;
                }
            }

            if (brc)
            {
                // list changed:
                // write list to INI as a string of handles
                WriteObjectsList(pll, pcszIniKey);
            }
        }
        else
            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                   "hmtxFolderLists request failed.");
    }
    CATCH(excpt1)
    {
        brc = FALSE;
    } END_CATCH();

    if (fSemOwned)
    {
        UnlockObjectsList();
        fSemOwned = FALSE;
    }

    return (brc);
}

/*
 *@@ objIsOnList:
 *      returns TRUE if the given object is on the
 *      given list of CONTENTMENULISTITEM's.
 *
 *      The list is protected by a global mutex semaphore,
 *      so this is thread-safe.
 *
 *      This is used from the XFolder methods for
 *      "favorite" and "quick-open" folders. The linked
 *      list roots are stored in classes\xfldr.c.
 *
 *@@added V0.9.0 (99-11-16) [umoeller]
 *@@changed V0.9.7 (2001-01-18) [umoeller]: made this generic for all objs... renamed from fdr*
 *@@changed V0.9.7 (2001-01-18) [umoeller]: removed mallocs(), this wasn't needed
 *@@changed V0.9.9 (2001-03-27) [umoeller]: added OBJECTLIST encapsulation
 */

BOOL objIsOnList(WPObject *somSelf,
                 POBJECTLIST pll)     // in: linked list of WPObject* pointers
{
    BOOL                 rc = FALSE,
                         fSemOwned = FALSE;

    TRY_LOUD(excpt1)
    {
        fSemOwned = LockObjectsList();
        if (fSemOwned)
        {
            PLISTNODE pNode = lstQueryFirstNode(&pll->ll);
            while (pNode)
            {
                WPObject *pobj = (WPObject*)pNode->pItemData;
                if (pobj == somSelf)
                {
                    rc = TRUE;
                    break;
                }
                else
                    pNode = pNode->pNext;
            }
        }
        else
            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                       "hmtxFolderLists request failed.");
    }
    CATCH(excpt1)
    {
        rc = FALSE;
    } END_CATCH();

    if (fSemOwned)
    {
        UnlockObjectsList();
        fSemOwned = FALSE;
    }

    return (rc);
}

/*
 *@@ objEnumList:
 *      this enumerates objects on the given list.
 *
 *      If (pObject == NULL), the first object on the list
 *      is returned, otherwise the object which comes
 *      after pObject in the list. If no (next) object
 *      is found, NULL is returned.
 *
 *      The list is protected by a global mutex semaphore,
 *      so this is thread-safe.
 *
 *      This is used from the XFolder methods for
 *      "favorite" and "quick-open" folders. The linked
 *      list roots are stored in classes\xfldr.c.
 *
 *@@added V0.9.0 (99-11-16) [umoeller]
 *@@changed V0.9.7 (2001-01-18) [umoeller]: made this generic for all objs... renamed from fdr*
 *@@changed V0.9.7 (2001-01-18) [umoeller]: removed mallocs(), this wasn't needed
 *@@changed V0.9.9 (2001-03-27) [umoeller]: added OBJECTLIST encapsulation
 */

WPObject* objEnumList(POBJECTLIST pll,        // in: linked list of WPObject* pointers
                      WPObject *pObjectFind,
                      const char *pcszIniKey,
                      ULONG ulListFlag)     // in: list flag for xwpModifyListNotify
{
    WPObject    *pObjectFound = NULL;
    BOOL        fSemOwned = FALSE;

    TRY_LOUD(excpt1)
    {
        fSemOwned = LockObjectsList();
        if (fSemOwned)
        {
            PLISTNODE       pNode = NULL;

            if (!pll->fLoaded)
                // if the list of favorite folders has not yet been built
                // yet, we will do this now
                LoadObjectsList(pll,
                                ulListFlag,
                                pcszIniKey);

            // OK, we should now have a valid list
            pNode = lstQueryFirstNode(&pll->ll);

            if (pObjectFind)
            {
                // obj given as param: look for this obj
                // and return the following in the list
                while (pNode)
                {
                    WPObject *pobj = (WPObject*)pNode->pItemData;
                    if (pobj == pObjectFind)
                    {
                        // return node after this
                        pNode = pNode->pNext;
                        break;
                    }

                    pNode = pNode->pNext;
                }
            }
            // else pObject == NULL: pNode still points to first

            if (pNode)
                // found something (either first or next):
                pObjectFound = (WPObject*)pNode->pItemData;
        }
        else
            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                       "hmtxFolderLists request failed.");
    }
    CATCH(excpt1)
    {
        pObjectFound = NULL;
    } END_CATCH();

    if (fSemOwned)
    {
        UnlockObjectsList();
        fSemOwned = FALSE;
    }

    return (pObjectFound);
}

/* ******************************************************************
 *
 *   Object handles cache
 *
 ********************************************************************/

/*
 *@@ LockHandlesCache:
 *
 *@@added V0.9.9 (2001-04-02) [umoeller]
 */

BOOL LockHandlesCache(VOID)
{
    BOOL brc = FALSE;

    if (G_hmtxHandlesCache == NULLHANDLE)
    {
        // first call:
        brc = !DosCreateMutexSem(NULL,
                                 &G_hmtxHandlesCache,
                                 0,
                                 TRUE);
        // initialize tree
        treeInit(&G_HandlesCache,
                 &G_lHandlesCacheItemsCount);
    }
    else
        brc = !WinRequestMutexSem(G_hmtxHandlesCache, SEM_INDEFINITE_WAIT);

    return (brc);
}

/*
 *@@ UnlockHandlesCache:
 *
 *@@added V0.9.9 (2001-04-02) [umoeller]
 */

VOID UnlockHandlesCache(VOID)
{
    DosReleaseMutexSem(G_hmtxHandlesCache);
}

/*
 *@@ CheckShrinkCache:
 *      checks if the cache has too many objects
 *      and shrinks it if needed. Returns the
 *      no. of objects removed.
 *
 *      Since the cache maintains a reference
 *      item per node, it can delete the oldest
 *      objects.
 *
 *      Preconditions:
 *
 *      --  Caller must have locked the cache.
 *
 *@@added V0.9.9 (2001-04-02) [umoeller]
 */

ULONG CheckShrinkCache(VOID)
{
    ULONG   ulDeleted = 0;
    LONG    lObjectsToDelete = G_lHandlesCacheItemsCount - CACHE_ITEM_LIMIT;

    if (lObjectsToDelete > 0)
    {
        while (lObjectsToDelete--)
        {
            POBJTREENODE    pOldest = (POBJTREENODE)treeFirst(G_HandlesCache),
                            pNode = pOldest;
            while (pNode)
            {
                if (pNode->ulReferenced < pOldest->ulReferenced)
                    // this node is older:
                    pOldest = pNode;

                pNode = (POBJTREENODE)treeNext((TREE*)pNode);
            }

            // now we know the oldest node;
            // delete it
            if (pOldest)
            {
                treeDelete(&G_HandlesCache,
                           &G_lHandlesCacheItemsCount,
                           (TREE*)pNode);
                // unset list notify flag
                _xwpModifyListNotify(pNode->pObject,
                                     OBJLIST_HANDLESCACHE,
                                     0);
                free(pNode);
            }
        }
    }

    return (ulDeleted);
}

/*
 *@@ objFindObjFromHandle:
 *      fast find-object function, which implements a
 *      cache for frequently used objects.
 *
 *      This is way faster than running _wpclsQueryObject
 *      and is therefore used with the extended
 *      file associations.
 *
 *      This uses a red-black balanced binary tree for
 *      finding the object from the HOBJECT. If the object
 *      is not found, _wpclsQueryObject is invoked, and
 *      the object is added to the tree.
 *
 *      As a result, this is especially helpful if you
 *      need the same few objects many times, as with
 *      the extended file assocs. They query the same
 *      maybe 20 HOBJECTs all the time, and possibly for
 *      a thousand data files.
 *
 *      Since I implemented this, folder populating has
 *      become a blitz. Extended assocs are now even
 *      faster than the standard WPS assocs. Quite
 *      unbelievable what 30 lines of code here and
 *      there could do to some other WPS internals...
 *
 *@@added V0.9.9 (2001-04-02) [umoeller]
 */

WPObject* objFindObjFromHandle(HOBJECT hobj)
{
    WPObject *pobjReturn = NULL;

    // lock the cache
    if (LockHandlesCache())
    {
        POBJTREENODE pNode = (POBJTREENODE)treeFind(G_HandlesCache,
                                                    hobj,
                                                    treeCompareKeys);
        if (pNode)
        {
            // was in cache:
            pobjReturn = pNode->pObject;
            // store system uptime as last reference
            pNode->ulReferenced = doshQuerySysUptime();
        }
        else
        {
            // was not in cache:
            // run wpclsQueryObject
            static M_XFldObject *pObjectClass = NULL;

            if (!pObjectClass)
                pObjectClass = _WPObject;

            if (pobjReturn = _wpclsQueryObject(pObjectClass, hobj))
            {
                // valid handle:

                // check if the cache needs to be shrunk
                CheckShrinkCache();

                // add new obj to cache
                if (pNode = NEW(OBJTREENODE))
                {
                    pNode->Tree.ulKey = hobj;
                    pNode->pObject = pobjReturn;
                    // store system uptime as last reference
                    pNode->ulReferenced = doshQuerySysUptime();

                    treeInsert(&G_HandlesCache,
                               &G_lHandlesCacheItemsCount,
                               (TREE*)pNode,
                               treeCompareKeys);

                    // set list-notify flag so we can
                    // kill this node, should the obj get deleted
                    // (objRemoveFromHandlesCache)
                    _xwpModifyListNotify(pobjReturn,
                                         OBJLIST_HANDLESCACHE,
                                         OBJLIST_HANDLESCACHE);
                }
            }
        }

        UnlockHandlesCache();
    }

    return (pobjReturn);
}

/*
 *@@ objRemoveFromHandlesCache:
 *      removes the specified object from the
 *      handles cache. Called from WPObject::wpUnInitData
 *      only when an object from the cache goes dormant.
 *
 *@@added V0.9.9 (2001-04-02) [umoeller]
 */

VOID objRemoveFromHandlesCache(WPObject *somSelf)
{
    // lock the cache
    if (LockHandlesCache())
    {
        // this is terminally slow, but what the heck...
        // this rarely gets called
        POBJTREENODE pNode = (POBJTREENODE)treeFirst(G_HandlesCache);
        while (pNode)
        {
            if (pNode->pObject == somSelf)
            {
                treeDelete(&G_HandlesCache,
                           &G_lHandlesCacheItemsCount,
                           (TREE*)pNode);
                free(pNode);
                break;
            }

            pNode = (POBJTREENODE)treeNext((TREE*)pNode);
        }

        UnlockHandlesCache();
    }
}

/* ******************************************************************
 *
 *   Dirty objects list
 *
 ********************************************************************/

/*
 *@@ LockDirtyList:
 *
 *@@added V0.9.9 (2001-04-02) [umoeller]
 */

BOOL LockDirtyList(VOID)
{
    BOOL brc = FALSE;

    if (G_hmtxDirtyList == NULLHANDLE)
    {
        // first call:
        brc = !DosCreateMutexSem(NULL,
                                 &G_hmtxDirtyList,
                                 0,
                                 TRUE);
        // initialize tree
        treeInit(&G_DirtyList,
                 &G_lDirtyListItemsCount);
    }
    else
        brc = !WinRequestMutexSem(G_hmtxDirtyList, SEM_INDEFINITE_WAIT);

    return (brc);
}

/*
 *@@ UnlockDirtyList:
 *
 *@@added V0.9.9 (2001-04-02) [umoeller]
 */

VOID UnlockDirtyList(VOID)
{
    DosReleaseMutexSem(G_hmtxDirtyList);
}

/*
 *@@ objAddToDirtyList:
 *      adds the given object to the "dirty" list, which
 *      is really a binary tree for speed.
 *
 *      This gets called from XFldObject::wpSaveDeferred.
 *      See remarks there.
 *
 *      Returns TRUE if the object was added or FALSE if
 *      not, e.g. because the object was already on the
 *      list.
 *
 *@@added V0.9.9 (2001-04-04) [umoeller]
 *@@changed V0.9.11 (2001-04-18) [umoeller]: added OBJLIST_DIRTYLIST list flag
 *@@changed V0.9.14 (2001-08-01) [umoeller]: fixed memory leak
 */

BOOL objAddToDirtyList(WPObject *pobj)
{
    BOOL    brc = FALSE;

    BOOL    fLocked = FALSE;

    TRY_LOUD(excpt1)
    {
        if (!_somIsA(pobj, _WPTransient))
        {
            if (fLocked = LockDirtyList())
            {
                // add new obj to cache;
                // we can use a plain TREE node (no special struct
                // definition needed) and just use the "id" field
                // for the pointer
                TREE *pNode = NEW(TREE);
                if (pNode)
                {
                    pNode->ulKey = (ULONG)pobj;

                    brc = (!treeInsert(&G_DirtyList,
                                       &G_lDirtyListItemsCount,
                                       pNode,
                                       treeCompareKeys));        // no duplicates
                    if (brc)
                    {
                        // _Pmpf((__FUNCTION__ ": added obj 0x%lX (%s)", pobj, _wpQueryTitle(pobj) ));
                        // _Pmpf(("  now %d objs on list", G_lDirtyListItemsCount ));

                        // note that we do not need an object list flag
                        // here because the WPS automatically invokes
                        // wpSaveImmediate on "dirty" objects during
                        // wpMakeDormant processing; as a result,
                        // objRemoveFromDirtyList will also get called
                        // automatically when the object goes dormant

                        // WRONG... this is not true for WPAbstracts, and
                        // we get tons of exceptions on XShutdown save-objects
                        // then. V0.9.11 (2001-04-18) [umoeller]
                        // so set list-notify flag so we can
                        // kill this node, should the obj get deleted
                        _xwpModifyListNotify(pobj,
                                             OBJLIST_DIRTYLIST,
                                             OBJLIST_DIRTYLIST);
                    }
                    else
                        free(pNode);        // V0.9.14 (2001-08-01) [umoeller]

                    // else
                        // already on list:
                        // _Pmpf((__FUNCTION__ ": DID NOT ADD obj 0x%lX (%s)", pobj, _wpQueryTitle(pobj) ));
                }
            }
        }
    }
    CATCH(excpt1)
    {
        brc = FALSE;
    } END_CATCH();

    if (fLocked)
        UnlockDirtyList();

    return (brc);
}

/*
 *@@ objRemoveFromDirtyList:
 *      removes the specified object from the "dirty"
 *      list. See objAddToDirtyList.
 *
 *      This gets called from XFldObject::wpSaveImmediate.
 *      See remarks there. Since that method doesn't always
 *      get called for WPAbstracts, we have some objects
 *      on this list which will always remain there... but
 *      never mind, it shouldn't hurt if we save those on
 *      shutdown. It's still better than saving all awake
 *      objects.
 *
 *      Returns TRUE if the object was found and removed.
 *
 *@@added V0.9.9 (2001-04-04) [umoeller]
 *@@changed V0.9.11 (2001-04-18) [umoeller]: added OBJLIST_DIRTYLIST list flag
 */

BOOL objRemoveFromDirtyList(WPObject *pobj)
{
    BOOL    brc = FALSE;

    BOOL    fLocked = FALSE;

    TRY_LOUD(excpt1)
    {
        if (fLocked = LockDirtyList())
        {
            TREE *pNode = treeFind(G_DirtyList,
                                   (ULONG)pobj,
                                   treeCompareKeys);
            if (pNode)
            {
                // was on list:
                treeDelete(&G_DirtyList,
                           &G_lDirtyListItemsCount,
                           pNode);
                free(pNode);

                /* _Pmpf((__FUNCTION__ ": removed obj 0x%lX, %d remaining",
                       pobj,
                       G_lDirtyListItemsCount )); */

                // unset object's "dirty" list flag
                _xwpModifyListNotify(pobj,
                                     OBJLIST_DIRTYLIST,
                                     0);

                brc = TRUE;
            }
        }
    }
    CATCH(excpt1)
    {
        brc = FALSE;
    } END_CATCH();

    if (fLocked)
        UnlockDirtyList();

    return (brc);
}

/*
 *@@ objQueryDirtyObjectsCount:
 *      returns the no. of currently dirty objects.
 *
 *@@added V0.9.9 (2001-04-04) [umoeller]
 */

ULONG objQueryDirtyObjectsCount(VOID)
{
    ULONG ulrc = 0;

    if (LockDirtyList())
    {
        ulrc = G_lDirtyListItemsCount;

        UnlockDirtyList();
    }

    return (ulrc);
}

/*
 *@@ objSaveAllDirtyObjects:
 *      invokes the specified callback on all objects on
 *      the "dirty" list. Starting with V0.9.9, this is
 *      used during XShutdown to save all dirty objects.
 *
 *      The callback must have the following prototype:
 *
 +      BOOL _Optlink fnCallback(WPObject *pobjThis,
 +                               ULONG ulIndex,
 +                               ULONG cObjects,
 +                               PVOID pvUser);
 +
 *      It will receive:
 *
 *      --  pobjThis: current object.
 *
 *      --  ulIndex: list index of current object, starting from 0.
 *
 *      --  cObjects: total objects on the list.
 *
 +      --  pvUser: what was passed to this function.
 *
 *      It is safe to call wpSaveImmediate from the callback
 *      (which will remove the object from the "dirty" list
 *      being processed!) because we build a copy of the dirty
 *      list internally before running the callback on the list.
 *
 *      WARNING: Do not play around with threads in the callback.
 *      The "dirty" list is locked while the callback is running,
 *      so only the callback thread may invoke wpSaveImmediate.
 *      Keep in mind that WinSendMsg can cause a thread switch.
 *
 *      Returns the no. of objects for which the callback
 *      returned TRUE.
 *
 *@@added V0.9.9 (2001-04-04) [umoeller]
 *@@changed V0.9.9 (2001-04-05) [umoeller]: now using treeBuildArray
 */

ULONG objForAllDirtyObjects(FNFORALLDIRTIESCALLBACK *pCallback,  // in: callback function
                            PVOID pvUserForCallback)    // in: user param for callback
{
    ULONG   ulrc = 0;

    BOOL    fLocked = FALSE;

    TRY_LOUD(excpt1)
    {
        if (fLocked = LockDirtyList())
        {
            // since wpSaveImmediate will call objRemoveFromDirtyList
            // from our XFldObject::wpSaveImmediate override, we cannot
            // simply run through the "dirty" tree and go for treeNext,
            // since the tree will be rebalanced with every save...
            // build an array instead and run the callback on the array.

            LONG        cObjects = G_lDirtyListItemsCount;
            TREE        **papNodes = treeBuildArray(G_DirtyList, // V0.9.9 (2001-04-05) [umoeller]
                                                    &cObjects);
            if (papNodes)
            {
                if (cObjects == G_lDirtyListItemsCount)
                {
                    ULONG   ul;
                    for (ul = 0;
                         ul < cObjects;
                         ul++)
                    {
                        if (pCallback((WPObject*)(papNodes[ul]->ulKey),    // object ptr
                                      ul,
                                      cObjects,
                                      pvUserForCallback))
                                // if this calls wpSaveImmediate, this might kill
                                // the tree node thru objRemoveFromDirtyList!
                            ulrc++;
                    }
                }
                else
                    cmnLog(__FILE__, __LINE__, __FUNCTION__,
                           "Tree node count mismatch. G_lDirtyListItemsCount is %d, treeBuildArray found %d.",
                           G_lDirtyListItemsCount, cObjects);

                free(papNodes);
            }
        }
    }
    CATCH(excpt1) { } END_CATCH();

    if (fLocked)
        UnlockDirtyList();

    return (ulrc);
}

/* ******************************************************************
 *
 *   Object hotkeys
 *
 ********************************************************************/

/*
 *@@ objFindHotkey:
 *      this searches an array of GLOBALHOTKEY structures
 *      for the given object handle (as returned by
 *      hifQueryObjectHotkeys) and returns a pointer
 *      to the array item or NULL if not found.
 *
 *      Used by objQueryObjectHotkey and objSetObjectHotkey.
 */

PGLOBALHOTKEY objFindHotkey(PGLOBALHOTKEY pHotkeys, // in: array returned by hifQueryObjectHotkeys
                            ULONG cHotkeys,        // in: array item count (_not_ array size!)
                            HOBJECT hobj)
{
    PGLOBALHOTKEY   prc = NULL;
    ULONG   ul = 0;
    // go thru array of hotkeys and find matching item
    PGLOBALHOTKEY pHotkeyThis = pHotkeys;
    for (ul = 0;
         ul < cHotkeys;
         ul++)
    {
        if (pHotkeyThis->ulHandle == hobj)
        {
            prc = pHotkeyThis;
            break;
        }
        pHotkeyThis++;
    }

    return (prc);
}

/*
 *@@ objQueryObjectHotkey:
 *      implementation for XFldObject::xwpQueryObjectHotkey.
 */

BOOL objQueryObjectHotkey(WPObject *somSelf,
                          XFldObject_POBJECTHOTKEY pHotkey)
{
    PGLOBALHOTKEY   pHotkeys;
    ULONG           cHotkeys = 0;
    BOOL            brc = FALSE;

    // XFldObjectData *somThis = XFldObjectGetData(somSelf);

    if (    (pHotkey)
         && (pHotkeys = hifQueryObjectHotkeys(&cHotkeys))
       )
    {
        PGLOBALHOTKEY pHotkeyThis = objFindHotkey(pHotkeys,
                                                  cHotkeys,
                                                  _wpQueryHandle(somSelf));
        if (pHotkeyThis)
        {
            // found:
            pHotkey->usFlags = pHotkeyThis->usFlags;
            pHotkey->ucScanCode = pHotkeyThis->ucScanCode;
            pHotkey->usKeyCode = pHotkeyThis->usKeyCode;
            brc = TRUE;     // success
        }

        hifFreeObjectHotkeys(pHotkeys);
    }

    return (brc);
}

/*
 *@@ objSetObjectHotkey:
 *      implementation for XFldObject::xwpSetObjectHotkey.
 *
 *@@changed V0.9.5 (2000-08-20) [umoeller]: fixed "set first hotkey" bug, which hung the system
 *@@changed V0.9.5 (2000-08-20) [umoeller]: added more error checking
 */

BOOL objSetObjectHotkey(WPObject *somSelf,
                        XFldObject_POBJECTHOTKEY pHotkey)
{
    BOOL            brc = FALSE;
    HOBJECT         hobjSelf;

    // XFldObjectData *somThis = XFldObjectGetData(somSelf);

    if (hobjSelf = _wpQueryHandle(somSelf))
    {
        TRY_LOUD(excpt1)
        {
            PGLOBALHOTKEY   pHotkeys;
            ULONG           cHotkeys = 0;
            #ifdef DEBUG_KEYS
                _Pmpf(("Entering xwpSetObjectHotkey usFlags = 0x%lX, usKeyCode = 0x%lX",
                        usFlags, usKeyCode));
            #endif

            if (pHotkeys = hifQueryObjectHotkeys(&cHotkeys))
            {
                // hotkeys list exists:

                PGLOBALHOTKEY pHotkeyThis = NULL;

                #ifdef DEBUG_KEYS
                    _Pmpf(("  Checking for existence hobj 0x%lX", hobjSelf));
                #endif
                // check if we have a hotkey for this object already
                pHotkeyThis = objFindHotkey(pHotkeys,
                                            cHotkeys,
                                            hobjSelf);

                #ifdef DEBUG_KEYS
                    _Pmpf(("  objFindHotkey returned PGLOBALHOTKEY 0x%lX", pHotkeyThis));
                #endif

                // what does the caller want?
                if (!pHotkey)
                {
                    #ifdef DEBUG_KEYS
                        _Pmpf(("  'delete hotkey' mode:"));
                    #endif

                    // "delete hotkey" mode:
                    if (pHotkeyThis)
                    {
                        // found (already exists): delete
                        // by copying the following item(s)
                        // in the array over the current one
                        ULONG   ulpofs = 0,
                                uliofs = 0;
                        ulpofs = ((PBYTE)pHotkeyThis - (PBYTE)pHotkeys);

                        #ifdef DEBUG_KEYS
                            _Pmpf(("  pHotkeyThis - pHotkeys: 0x%lX", ulpofs));
                        #endif

                        uliofs = (ulpofs / sizeof(GLOBALHOTKEY));
                                    // 0 for first, 1 for second, ...

                        #ifdef DEBUG_KEYS
                            _Pmpf(("  Deleting existing hotkey @ ofs %d", uliofs));
                        #endif

                        if (uliofs < (cHotkeys - 1))
                        {
                            // not last item:

                            ULONG cb = (cHotkeys - uliofs - 1) * sizeof(GLOBALHOTKEY);

                            #ifdef DEBUG_KEYS
                                _Pmpf(("  Copying 0x%lX to 0x%lX, %d bytes (%d per item)",
                                        pHotkeyThis + 1, pHotkeyThis,
                                        cb, sizeof(GLOBALHOTKEY)));
                            #endif

                            memcpy(pHotkeyThis,
                                   pHotkeyThis + 1,
                                   cb);
                        }

                        brc = hifSetObjectHotkeys(pHotkeys, cHotkeys - 1);
                    }
                    // else: does not exist, so it can't be deleted either
                }
                else
                {
                    // "set hotkey" mode:

                    #ifdef DEBUG_KEYS
                        _Pmpf(("  'set hotkey' mode:"));
                    #endif

                    if (pHotkeyThis)
                    {
                        // found (already exists): overwrite
                        #ifdef DEBUG_KEYS
                            _Pmpf(("  Overwriting existing hotkey"));
                        #endif

                        if (    (pHotkeyThis->usFlags != pHotkey->usFlags)
                             || (pHotkeyThis->usKeyCode != pHotkey->usKeyCode)
                             || (pHotkeyThis->ucScanCode != pHotkey->ucScanCode)
                           )
                        {
                            pHotkeyThis->usFlags = pHotkey->usFlags;
                            pHotkeyThis->ucScanCode = pHotkey->ucScanCode;
                            pHotkeyThis->usKeyCode = pHotkey->usKeyCode;
                            pHotkeyThis->ulHandle = hobjSelf;
                            // set new objects list, which is the modified old list
                            brc = hifSetObjectHotkeys(pHotkeys, cHotkeys);
                        }
                    }
                    else
                    {
                        // not found: append new item after copying
                        // the entire list
                        PGLOBALHOTKEY pHotkeysNew = (PGLOBALHOTKEY)malloc(sizeof(GLOBALHOTKEY)
                                                                            * (cHotkeys+1));
                        #ifdef DEBUG_KEYS
                            _Pmpf(("  Appending new hotkey"));
                        #endif

                        if (pHotkeysNew)
                        {
                            PGLOBALHOTKEY pNewItem = pHotkeysNew + cHotkeys;
                            // copy old array
                            memcpy(pHotkeysNew, pHotkeys, sizeof(GLOBALHOTKEY) * cHotkeys);
                            // append new item
                            pNewItem->usFlags = pHotkey->usFlags;
                            pNewItem->ucScanCode = pHotkey->ucScanCode;
                            pNewItem->usKeyCode = pHotkey->usKeyCode;
                            pNewItem->ulHandle = hobjSelf;
                            brc = hifSetObjectHotkeys(pHotkeysNew, cHotkeys + 1);
                            free(pHotkeysNew);
                        }
                    }
                }

                hifFreeObjectHotkeys(pHotkeys);
            } // end if (pHotkeys)
            else
            {
                // hotkey list doesn't exist yet:
                if (pHotkey)
                {
                    // "set hotkey" mode:
                    GLOBALHOTKEY HotkeyNew = {0};

                    #ifdef DEBUG_KEYS
                        _Pmpf(("  Creating single new hotkey"));
                    #endif

                    HotkeyNew.usFlags = pHotkey->usFlags;
                    HotkeyNew.ucScanCode = pHotkey->ucScanCode; // V0.9.5 (2000-08-20) [umoeller]
                    HotkeyNew.usKeyCode = pHotkey->usKeyCode;
                    HotkeyNew.ulHandle = hobjSelf;
                    brc = hifSetObjectHotkeys(&HotkeyNew,
                                              1);     // one item only
                }
                // else "delete hotkey" mode: do nothing
            }
        }
        CATCH(excpt1)
        {
            brc = FALSE;
        } END_CATCH();

        if (brc)
            // updated: update the "Hotkeys" settings page
            // in XWPKeyboard, if it's open
            ntbUpdateVisiblePage(NULL,      // any somSelf
                                 SP_KEYB_OBJHOTKEYS);
    } // end if (hobjSelf)

    #ifdef DEBUG_KEYS
        _Pmpf(("Leaving xwpSetObjectHotkey"));
    #endif

    return (brc);
}

/*
 *@@ objRemoveObjectHotkey:
 *      implementation for M_XFldObject::xwpclsRemoveObjectHotkey.
 *
 *@@added V0.9.0 (99-11-12) [umoeller]
 */

BOOL objRemoveObjectHotkey(HOBJECT hobj)
{
    PGLOBALHOTKEY   pHotkeys;
    ULONG           cHotkeys = 0;
    BOOL            brc = FALSE;

    pHotkeys = hifQueryObjectHotkeys(&cHotkeys);

    if (pHotkeys)
    {
        // hotkeys list exists:
        PGLOBALHOTKEY pHotkeyThis = objFindHotkey(pHotkeys,
                                                  cHotkeys,
                                                  hobj);

        if (pHotkeyThis)
        {
            // found (already exists): delete
            // by copying the following item(s)
            // in the array over the current one
            ULONG   ulpofs = 0,
                    uliofs = 0;
            ulpofs = ((PBYTE)pHotkeyThis - (PBYTE)pHotkeys);

            uliofs = (ulpofs / sizeof(GLOBALHOTKEY));
                        // 0 for first, 1 for second, ...
            if (uliofs < (cHotkeys - 1))
            {
                ULONG cb = (cHotkeys - uliofs - 1) * sizeof(GLOBALHOTKEY);

                #ifdef DEBUG_KEYS
                    _Pmpf(("  Copying 0x%lX to 0x%lX, %d bytes (%d per item)",
                            pHotkeyThis + 1, pHotkeyThis,
                            cb, sizeof(GLOBALHOTKEY)));
                #endif

                // not last item:
                memcpy(pHotkeyThis,
                       pHotkeyThis + 1,
                       cb);
            }

            brc = hifSetObjectHotkeys(pHotkeys, cHotkeys-1);
        }
        // else: does not exist, so it can't be deleted either

        hifFreeObjectHotkeys(pHotkeys);
    }

    if (brc)
        // updated: update the "Hotkeys" settings page
        // in XWPKeyboard, if it's open
        ntbUpdateVisiblePage(NULL,      // any somSelf
                             SP_KEYB_OBJHOTKEYS);

    return (brc);
}

/* ******************************************************************
 *
 *   Object setup strings
 *
 ********************************************************************/

/*
 * CheckStyle:
 *      called by objQuerySetup to check an instance
 *      style against a class default style and add
 *      a string if necessary.
 */

VOID CheckStyle(PXSTRING pxstr,       // in: string for xstrcat
                ULONG ul1,        // in: style 1
                ULONG ul2,        // in: style 2
                ULONG ulMask,     // in: mask for style 1/2
                PSZ pszName)    // in: setup string prefix (e.g. "NOMOVE=")
{
    if ((ul1 & ulMask) != (ul2 & ulMask))
    {
        xstrcat(pxstr, pszName, 0);
        if (ul1 & ulMask)
            xstrcat(pxstr, "YES;", 0);
        else
            xstrcat(pxstr, "NO;", 0);
    }
}

/*
 *@@ objModifyPopupMenu:
 *      implementation for XFldObject::wpModifyPopupMenu.
 *      This now implements "lock in place" hacks.
 *
 *@@added V0.9.7 (2000-12-10) [umoeller]
 */

VOID objModifyPopupMenu(WPObject* somSelf,
                        HWND hwndMenu)
{
    if (doshIsWarp4())
    {
        PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
        if (pGlobalSettings->RemoveLockInPlaceItem)
            // remove WPObject's "Lock in place" submenu
            winhDeleteMenuItem(hwndMenu, ID_WPM_LOCKINPLACE);
        else if (pGlobalSettings->fFixLockInPlace) // V0.9.7 (2000-12-10) [umoeller]
        {
            // get text first... this saves us our own NLS resource
            PSZ pszLockInPlace = winhQueryMenuItemText(hwndMenu, ID_WPM_LOCKINPLACE);
            if (pszLockInPlace)
            {
                MENUITEM mi;
                if (winhQueryMenuItem(hwndMenu,
                                      ID_WPM_LOCKINPLACE,
                                      FALSE,
                                      &mi))
                {
                    // delete old (incl. submenu)
                    winhDeleteMenuItem(hwndMenu, ID_WPM_LOCKINPLACE);
                    // insert new
                    winhInsertMenuItem(hwndMenu,
                                       mi.iPosition,        // at old position
                                       ID_WPM_LOCKINPLACE,
                                       pszLockInPlace,
                                       MIS_TEXT,
                                       (_wpQueryStyle(somSelf) & OBJSTYLE_LOCKEDINPLACE)
                                          ? MIA_CHECKED
                                          : 0);
                }
                free(pszLockInPlace);
            }
        }
    }
}

/* ******************************************************************
 *
 *   Object setup strings
 *
 ********************************************************************/

/*
 *@@ objSetup:
 *      implementation for XFldObject::wpSetup.
 *
 *@@added V0.9.9 (2001-04-06) [umoeller]
 */

BOOL objSetup(WPObject *somSelf,
              PSZ pszSetupString)
{
    ULONG   cb = 0;
    BOOL    brc = TRUE;

    if (_wpScanSetupString(somSelf,
                           pszSetupString,
                           "WRITEREXXSETUP",
                           NULL,            // get size
                           &cb))
    {
        PSZ pszRexxFile = malloc(cb);
        if (pszRexxFile)
        {
            if (_wpScanSetupString(somSelf,
                                   pszSetupString,
                                   "WRITEREXXSETUP",
                                   pszRexxFile,
                                   &cb))
            {
                brc = !objCreateObjectScript(somSelf,
                                             pszRexxFile,
                                             NULL,
                                             SCRFL_RECURSE);
            }

            free(pszRexxFile);
        }
    }

    return (brc);
}

/*
 *@@ objQuerySetup:
 *      implementation of XFldObject::xwpQuerySetup.
 *      See remarks there.
 *
 *      This returns the length of the XFldObject
 *      setup string part only.
 *
 *@@added V0.9.1 (2000-01-16) [umoeller]
 *@@changed V0.9.4 (2000-08-02) [umoeller]: added NOCOPY, NODELETE etc.
 *@@changed V0.9.5 (2000-08-26) [umoeller]: added DEFAULTVIEW=RUNNING; fixed class default view
 *@@changed V0.9.7 (2000-12-10) [umoeller]: added LOCKEDINPLACE
 *@@todo some strings missing
 */

BOOL objQuerySetup(WPObject *somSelf,
                   PVOID pstrSetup)
{
    ULONG   ulValue = 0,
            ulStyle = 0,
            ulClassStyle = 0;
    PSZ     pszValue = 0;

    XFldObjectData *somThis = XFldObjectGetData(somSelf);

    /* if (_somIsA(somSelf, _WPProgram))
    {
        fsysQueryProgramSetup(somSelf,
                              pstrSetup);
    } */        // removed this V0.9.9 (2001-04-02) [umoeller]
                // because we have now a proper method in XWPProgram

    // CCVIEW
    ulValue = _wpQueryConcurrentView(somSelf);
    switch (ulValue)
    {
        case CCVIEW_ON:
            xstrcat(pstrSetup, "CCVIEW=YES;", 0);
        break;

        case CCVIEW_OFF:
            xstrcat(pstrSetup, "CCVIEW=NO;", 0);
        break;
        // ignore CCVIEW_DEFAULT
    }

    // DEFAULTVIEW
    if (_pWPObjectData)
    {
        ULONG ulClassDefaultView = _wpclsQueryDefaultView(_somGetClass(somSelf));

        if (    (_pWPObjectData->lDefaultView != 0x67)      // default view for folders
             && (_pWPObjectData->lDefaultView != 0x1000)    // default view for data files
             && (_pWPObjectData->lDefaultView != -1)        // OPEN_DEFAULT
             && (_pWPObjectData->lDefaultView != ulClassDefaultView) // OPEN_DEFAULT
           )
        {
            switch (_pWPObjectData->lDefaultView)
            {
                case OPEN_SETTINGS:
                    xstrcat(pstrSetup, "DEFAULTVIEW=SETTINGS;", 0);
                break;

                case OPEN_CONTENTS:
                    xstrcat(pstrSetup, "DEFAULTVIEW=ICON;", 0);
                break;

                case OPEN_TREE:
                    xstrcat(pstrSetup, "DEFAULTVIEW=TREE;", 0);
                break;

                case OPEN_DETAILS:
                    xstrcat(pstrSetup, "DEFAULTVIEW=DETAILS;", 0);
                break;

                case OPEN_RUNNING:
                    xstrcat(pstrSetup, "DEFAULTVIEW=RUNNING;", 0);
                break;

                case OPEN_DEFAULT:
                    // ignore
                break;

                default:
                {
                    // any other: that's user defined, add decimal ID
                    CHAR szTemp[30];
                    sprintf(szTemp, "DEFAULTVIEW=%d;", _pWPObjectData->lDefaultView);
                    xstrcat(pstrSetup, szTemp, 0);
                break; }
            }
        }
    }

    // HELPLIBRARY  @@todo

    // HELPPANEL
    if (_pWPObjectData)
        if (_pWPObjectData->ulHelpPanel)
        {
            CHAR szTemp[40];
            sprintf(szTemp, "HELPPANEL=%d;", _pWPObjectData->ulHelpPanel);
            xstrcat(pstrSetup, szTemp, 0);
        }

    // HIDEBUTTON
    ulValue = _wpQueryButtonAppearance(somSelf);
    switch (ulValue)
    {
        case HIDEBUTTON:
            xstrcat(pstrSetup, "HIDEBUTTON=YES;", 0);
        break;

        case MINBUTTON:
            xstrcat(pstrSetup, "HIDEBUTTON=NO;", 0);
        break;

        // ignore DEFAULTBUTTON
    }

    // ICONFILE: cannot be queried!
    // ICONRESOURCE: cannot be queried!

    // ICONPOS: x, y in percentage of folder coordinates

    // MENUS: Warp 4 only

    // MINWIN
    ulValue = _wpQueryMinWindow(somSelf);
    switch (ulValue)
    {
        case MINWIN_HIDDEN:
            xstrcat(pstrSetup, "MINWIN=HIDE;", 0);
        break;

        case MINWIN_VIEWER:
            xstrcat(pstrSetup, "MINWIN=VIEWER;", 0);
        break;

        case MINWIN_DESKTOP:
            xstrcat(pstrSetup, "MINWIN=DESKTOP;", 0);
        break;

        // ignore MINWIN_DEFAULT
    }

    // compare wpQueryStyle with clsStyle
    ulStyle = _wpQueryStyle(somSelf);
    ulClassStyle = _wpclsQueryStyle(_somGetClass(somSelf));

    // NOMOVE:
    CheckStyle(pstrSetup, ulStyle, ulClassStyle, OBJSTYLE_NOMOVE, "NOMOVE=");
        // OBJSTYLE_NOMOVE == CLSSTYLE_NEVERMOVE == 0x00000002; see wpobject.h
    // NOLINK:
    // NOSHADOW:
    CheckStyle(pstrSetup, ulStyle, ulClassStyle, OBJSTYLE_NOLINK, "NOLINK=");
        // OBJSTYLE_NOLINK == CLSSTYLE_NEVERLINK == 0x00000004; see wpobject.h
    // NOCOPY:
    CheckStyle(pstrSetup, ulStyle, ulClassStyle, OBJSTYLE_NOCOPY, "NOCOPY=");
        // OBJSTYLE_NOCOPY == CLSSTYLE_NEVERCOPY == 0x00000008; see wpobject.h
    // NODELETE:
    CheckStyle(pstrSetup, ulStyle, ulClassStyle, OBJSTYLE_NODELETE, "NODELETE=");
        // OBJSTYLE_NODELETE == CLSSTYLE_NEVERDELETE == 0x00000040; see wpobject.h
    // NOPRINT:
    CheckStyle(pstrSetup, ulStyle, ulClassStyle, OBJSTYLE_NOPRINT, "NOPRINT=");
        // OBJSTYLE_NOPRINT == CLSSTYLE_NEVERPRINT == 0x00000080; see wpobject.h
    // NODRAG:
    CheckStyle(pstrSetup, ulStyle, ulClassStyle, OBJSTYLE_NODRAG, "NODRAG=");
        // OBJSTYLE_NODRAG == CLSSTYLE_NEVERDRAG == 0x00000100; see wpobject.h
    // NOTVISIBLE:
    CheckStyle(pstrSetup, ulStyle, ulClassStyle, OBJSTYLE_NOTVISIBLE, "NOTVISIBLE=");
        // OBJSTYLE_NOTVISIBLE == CLSSTYLE_NEVERVISIBLE == 0x00000200; see wpobject.h
    // NOSETTINGS:
    CheckStyle(pstrSetup, ulStyle, ulClassStyle, OBJSTYLE_NOSETTINGS, "NOSETTINGS=");
        // OBJSTYLE_NOSETTINGS == CLSSTYLE_NEVERSETTINGS == 0x00000400; see wpobject.h
    // NORENAME:
    CheckStyle(pstrSetup, ulStyle, ulClassStyle, OBJSTYLE_NORENAME, "NORENAME=");
        // OBJSTYLE_NORENAME == CLSSTYLE_NEVERRENAME == 0x00000800; see wpobject.h

    // NODROP:
    // NODROP is obscure: wpobject.h writes:
    /*
      #define OBJSTYLE_NODROP         0x00001000
      #define OBJSTYLE_NODROPON       0x00002000   // Use instead of OBJSTYLE_NODROP,
                                              because OBJSTYLE_NODROP and
                                              CLSSTYLE_PRIVATE have the same
                                              value (DD 86093F)  */
    // However, according to WPSREF, the setup string is still NODROP, not NODROPON.
    CheckStyle(pstrSetup, ulStyle, ulClassStyle, OBJSTYLE_NODROPON, "NODROP=");
        // OBJSTYLE_NODROPON == CLSSTYLE_NEVERDROPON == 0x00002000; see wpobject.h

    if (ulStyle & OBJSTYLE_TEMPLATE)
        xstrcat(pstrSetup, "TEMPLATE=YES;", 0);

    // LOCKEDINPLACE: Warp 4 only
    if (doshIsWarp4())
        if (ulStyle & OBJSTYLE_LOCKEDINPLACE)
            xstrcat(pstrSetup, "LOCKEDINPLACE=YES;", 0);

    // TITLE
    /* pszValue = _wpQueryTitle(somSelf);
    {
        xstrcat(pstrSetup, "TITLE=");
            xstrcat(pstrSetup, pszValue);
            xstrcat(pstrSetup, ";");
    } */

    // OBJECTID: always append this LAST!
    if (pszValue = _wpQueryObjectID(somSelf))
        if (strlen(pszValue))
        {
            xstrcat(pstrSetup, "OBJECTID=", 0);
            xstrcat(pstrSetup, pszValue, 0);
            xstrcatc(pstrSetup, ';');
        }

    return (TRUE);
}

/* ******************************************************************
 *
 *   Object Scripts
 *
 ********************************************************************/

/*
 *@@ WriteOutObjectSetup:
 *
 *@@added V0.9.9 (2001-04-06) [umoeller]
 */

ULONG WriteOutObjectSetup(FILE *RexxFile,
                          WPObject *pobj,
                          ULONG ulRecursion,        // in: recursion level, initially 0
                          BOOL fRecurse)
{
    ULONG       ulrc = 0,
                ul;

    PSZ         pszSetupString;

    if (pszSetupString = _xwpQuerySetup(pobj, NULL))
    {
        PSZ         pszTrueClassName = _wpGetTrueClassName(SOMClassMgrObject, pobj);

        CHAR        szFolderName[CCHMAXPATH];
        XSTRING     strTitle;
        ULONG       ulOfs;
        CHAR        cQuote = '\"';

        BOOL        fIsDisk = !strcmp(pszTrueClassName, "WPDisk");

        // get folder ID or name
        WPFolder    *pOwningFolder = _wpQueryFolder(pobj);
        PSZ         pszOwningFolderID = _wpQueryObjectID(pOwningFolder);
        if (pszOwningFolderID)
            strcpy(szFolderName, pszOwningFolderID);
        else
            _wpQueryFilename(pOwningFolder, szFolderName, TRUE);

        // special hack for line breaks in titles: "^"
        xstrInitCopy(&strTitle, _wpQueryTitle(pobj), 0);
        ulOfs = 0;
        while (xstrFindReplaceC(&strTitle, &ulOfs, "\r\n", "^"))
            ;
        ulOfs = 0;
        while (xstrFindReplaceC(&strTitle, &ulOfs, "\r", "^"))
            ;
        ulOfs = 0;
        while (xstrFindReplaceC(&strTitle, &ulOfs, "\n", "^"))
            ;

        // if we have a quote:
        if (strchr(strTitle.psz, '\"'))
            cQuote = '\'';

        // indent
        for (ul = 0; ul < ulRecursion; ul++)
            fprintf(RexxFile, "  ");

        if (fIsDisk)
            fprintf(RexxFile, "/* ");

        // write out object
        if (pszSetupString && strlen(pszSetupString))
            // we got setup:
            fprintf(RexxFile,
                    "rc = SysCreateObject(\"%s\", %c%s%c, \"%s\", \"%s\");",
                    pszTrueClassName,
                    cQuote,
                    strTitle.psz,
                    cQuote,
                    szFolderName,
                    pszSetupString);
        else
            // no setup string:
            fprintf(RexxFile,
                    "rc = SysCreateObject(\"%s\", %c%s%c, \"%s\");",
                    pszTrueClassName,
                    cQuote,
                    strTitle.psz,
                    cQuote,
                    szFolderName);

        if (fIsDisk)
            fprintf(RexxFile, " */ \n");
        else
            fprintf(RexxFile, "\n");

        ulrc++;

        if (pszSetupString)
            free(pszSetupString);
        xstrClear(&strTitle);

        // recurse for folders
        if (    (fRecurse)
             && (_somIsA(pobj, _WPFolder))
           )
        {
            BOOL    fSem = FALSE;
            TRY_LOUD(excpt1)
            {
                // rule out certain stupid special folder classes
                if (    strcmp(pszTrueClassName, G_pcszXWPFontFolder)
                     && strcmp(pszTrueClassName, G_pcszXWPTrashCan)
                     && strcmp(pszTrueClassName, "WPMinWinViewer")
                     && strcmp(pszTrueClassName, "WPHwManager")
                     && strcmp(pszTrueClassName, "WPTemplates")
                     && strcmp(pszTrueClassName, "WPNetgrp")
                   )
                {
                    if (wpshCheckIfPopulated(pobj, FALSE))
                    {
                        if (fSem = !wpshRequestFolderMutexSem(pobj, 5000))
                        {
                            WPObject *pSubObj = 0;

                            somTD_WPFolder_wpQueryContent rslv_wpQueryContent
                                    = (somTD_WPFolder_wpQueryContent)wpshResolveFor(pobj,
                                                                                    NULL,
                                                                                    "wpQueryContent");

                            for (   pSubObj = rslv_wpQueryContent(pobj,
                                                                  NULL,
                                                                  QC_FIRST);
                                    pSubObj;
                                    pSubObj = rslv_wpQueryContent(pobj,
                                                                  pSubObj,
                                                                  QC_NEXT)
                                )
                            {
                                ulrc += WriteOutObjectSetup(RexxFile,
                                                            pSubObj,
                                                            ulRecursion + 1,
                                                            fRecurse);
                            }
                        }
                    }
                }
            }
            CATCH(excpt1)
            {
            }
            END_CATCH();

            if (fSem)
                wpshReleaseFolderMutexSem(pobj);
        }

        _xwpFreeSetupBuffer(pobj, pszSetupString);
    }

    // return total object count
    return (ulrc);
}

/*
 *@@ fdrCreateObjectScript:
 *      creates an object package.
 *
 *      pllObjects is expected to contain plain WPObject*
 *      pointers of all objects to put into the package.
 *
 *      pcszRexxFile must be the fully qualified path name
 *      of the REXX .CMD file to be created.
 *
 *      flCreate can be any combination of:
 *
 *      --  SCRFL_RECURSE: recurse into subfolders.
 *
 *      This returns an OS/2 error code.
 *
 *@@added V0.9.9 (2001-04-04) [umoeller]
 */

APIRET objCreateObjectScript(WPObject *pObject,          // in: object to start with
                             const char *pcszRexxFile,   // in: file name of output REXX file
                             WPFolder *pFolderForFiles,  // in: if != NULL, icons etc. are put here
                             ULONG flCreate)             // in: flags
{
    APIRET arc = NO_ERROR;

    if (!pObject || !pcszRexxFile)
        arc = ERROR_INVALID_PARAMETER;
    else
    {
        FILE *RexxFile = fopen(pcszRexxFile,
                               "w");            // replace @@@

        if (!RexxFile)
            arc = ERROR_CANNOT_MAKE;
        else
        {
            WriteOutObjectSetup(RexxFile,
                                pObject,
                                0,
                                ((flCreate & SCRFL_RECURSE) != 0));
                    // this will recurse for folders

            fclose(RexxFile);
        }
    }

    return (arc);
}


