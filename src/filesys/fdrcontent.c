
/*
 *@@sourcefile folder.c:
 *      folder content implementation code. This contains
 *
 *      1)  folder mutex wrappers,
 *
 *      2)  folder content enumeration,
 *
 *      3)  fast binary balanced trees for folder contents,
 *
 *      4)  fdrQueryAwakeFSObject for fast finding of
 *          an awake file-system object,
 *
 *      4)  a replacement populate (fdrPopulate).
 *
 *      Some of this code is used all the time, some only
 *      if "turbo folders" are enabled.
 *
 *      A couple of remarks about the WPS's folder content
 *      management is in order.
 *
 *      1)  You need to differentiate between "folder content"
 *          and "objects in folder views", which are not necessarily
 *          the same, albeit hard to tell apart because of the
 *          ugly WPS implementation and bad documentation.
 *
 *          As soon as an object is made awake, it is added to
 *          the folder's _content_. When it is made dormant,
 *          it is removed again. For this, WPFolder has the
 *          wpAddToContent and wpDeleteFromContent methods,
 *          which _always_ get called on the folder of the
 *          object that is being made awake or dormant.
 *          The standard WPFolder implementation of these
 *          two methods maintain the folder content lists.
 *
 *      2)  Awaking an object does not necessarily mean that the
 *          object is also inserted into open views of the folder.
 *          IBM recommends to override the aforementioned two
 *          methods for supporting add/remove in new folder views.
 *
 *      3)  WPFolder itself does not seem to contain a real
 *          linked list of its objects. Instead, each _object_
 *          has two "next" and "previous" instance variables,
 *          which point to the next and previous object in
 *          its folder.
 *
 *          Apparently, WPFolder::wpQueryContent iterates over
 *          these things.
 *
 *          I wonder who came up with this stupid idea since
 *          this needs SOM methods and slows things down
 *          massively. See fdrResolveContentPtrs.
 *
 *          Starting with V0.9.16, XFolder maintains two binary
 *          balanced trees of the file-system and abstract
 *          objects which have been added to the folder to allow
 *          for speedy lookup of whether an object is already
 *          awake. I believe this new code is way faster than
 *          the WPS way of looking this up.
 *
 *          For this, I have overridden wpAddToContent and
 *          wpDeleteFromContent. In addition, to refresh this
 *          data when objects are renamed, I have also added
 *          XWPFileSystem::wpSetTitleAndRenameFile and
 *          XWPFileSystem::xwpQueryUpperRealName.
 *
 *      4)  At least three additional folder mutexes are involved
 *          in folder content protection. See fdrRequestFolderMutexSem,
 *          fdrRequestFolderWriteMutexSem, and fdrRequestFindMutexSem.
 *
 *      This file is ALL new with V0.9.16 and contains
 *      code formerly in folder.c and wpsh.c.
 *
 *      Function prefix for this file:
 *      --  fdr*
 *
 *@@added V0.9.16 (2001-10-23) [umoeller]
 *@@header "filesys\folder.h"
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

#define INCL_DOSEXCEPTIONS
#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#define INCL_DOSMISC
#define INCL_DOSERRORS

#define INCL_WINWINDOWMGR
#define INCL_WINMESSAGEMGR
#define INCL_WINFRAMEMGR
#define INCL_WININPUT
#define INCL_WINRECTANGLES
#define INCL_WINSYS             // needed for presparams
#define INCL_WINMENUS
#define INCL_WINTIMER
#define INCL_WINDIALOGS
#define INCL_WINSTATICS
#define INCL_WINBUTTONS
#define INCL_WINENTRYFIELDS
#define INCL_WINLISTBOXES
#define INCL_WINSTDCNR
#define INCL_WINSHELLDATA       // Prf* functions
#define INCL_GPILOGCOLORTABLE
#define INCL_GPIPRIMITIVES
#include <os2.h>

// C library headers
#include <stdio.h>              // needed for except.h
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\cnrh.h"               // container helper routines
#include "helpers\comctl.h"             // common controls (window procs)
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\eah.h"                // extended attributes helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\gpih.h"               // GPI helper routines
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\nls.h"                // National Language Support helpers
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\standards.h"          // some standard macros
#include "helpers\stringh.h"            // string helper routines
#include "helpers\syssound.h"           // system sound helper routines
#include "helpers\threads.h"            // thread helpers
#include "helpers\winh.h"               // PM helper routines
#include "helpers\wphandle.h"           // file-system object handles
#include "helpers\threads.h"            // thread helpers
#include "helpers\tree.h"               // red-black binary trees
#include "helpers\xstring.h"            // extended string helpers

// SOM headers which don't crash with prec. header files
#include "xfobj.ih"
#include "xfdisk.ih"
#include "xwpfsys.ih"
#include "xfldr.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\cnrsort.h"             // container sort comparison functions
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\helppanels.h"          // all XWorkplace help panel IDs
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

#include "filesys\fdrmenus.h"           // shared folder menu logic
#include "filesys\fileops.h"            // file operations implementation
#include "filesys\filesys.h"            // various file-system object implementation code
#include "filesys\filetype.h"           // extended file types implementation
#include "filesys\folder.h"             // XFolder implementation
#include "filesys\object.h"             // XFldObject implementation
#include "filesys\statbars.h"           // status bar translation logic
#include "filesys\xthreads.h"           // extra XWorkplace threads

// other SOM headers
#pragma hdrstop                     // VAC++ keeps crashing otherwise

#include <wpclsmgr.h>                   // this includes SOMClassMgr
#include <wpshadow.h>               // WPShadow
#include <wprootf.h>
#include <wpdataf.h>

#include "helpers\undoc.h"              // some undocumented stuff

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

static PGEA2LIST G_StandardGEA2List = NULL;

/* ******************************************************************
 *
 *   Folder semaphores
 *
 ********************************************************************/

/*
 *@@ fdrRequestFolderMutexSem:
 *      calls WPFolder::wpRequestFolderMutexSem.
 *
 *      The folder mutex semaphore is mentioned in the Warp 4
 *      toolkit docs for wpRequestObjectMutexSem, but never
 *      prototyped. This is a real pity because we'll always
 *      hang the WPS if we don't request this properly.
 *
 *      In addition to the regular object mutex, each folder
 *      has associated with it a second mutex to protect the
 *      folder contents. While this semaphore is held, one
 *      can be sure that no objects are removed from or added
 *      to a folder from some other WPS thread. For example,
 *      another populate will be blocked out from the folder.
 *      You should always request this semaphore before using
 *      wpQueryContent and such things because otherwise the
 *      folder contents can be changed behind your back.
 *
 *      Returns:
 *
 *      --  NO_ERROR: semaphore was successfully requested.
 *
 *      --  -1: error resolving method. A log entry should
 *          have been added also.
 *
 *      --  other: error codes from DosRequestMutexSem probably.
 *
 *      WARNINGS:
 *
 *      -- As usual, you better not forget to release the
 *         mutex again. Use fdrReleaseFolderMutexSem.
 *
 *      -- Also as usual, request this mutex only in a
 *         block which is protected by an exception handler.
 *
 *      -- In addition, if you also request the _object_
 *         mutex for the folder (WPObject::wpRequestObjectMutexSem),
 *         you must take great care that the two are released in
 *         exactly reverse order, or you can deadlock the system.
 *
 *         Guideline:
 *
 *         1)  Request the folder mutex.
 *
 *         2)  Request the object mutex.
 *
 *         3)  Do processing.
 *
 *         4)  Release object mutex.
 *
 *         5)  Release folder mutex.
 *
 *      -- See fdrRequestFindMutexSem for additional remarks.
 *
 *@@added V0.9.6 (2000-10-25) [umoeller]
 *@@changed V0.9.16 (2001-10-25) [umoeller]: moved this here from wpsh.c, changed prefix
 *@@changed V0.9.16 (2001-10-25) [umoeller]: now caching method pointer in folder instance data
 */

ULONG fdrRequestFolderMutexSem(WPFolder *somSelf,
                               ULONG ulTimeout)
{
    ULONG ulrc = -1;

    XFolderData *somThis = XFolderGetData(somSelf);

    if (!_pfn_wpRequestFolderMutexSem)
        // first call for this folder:
        _pfn_wpRequestFolderMutexSem
            = (PVOID)wpshResolveFor(somSelf,
                                    NULL, // use somSelf's class
                                    "wpRequestFolderMutexSem");

    if (_pfn_wpRequestFolderMutexSem)
        ulrc = ((xfTD_wpRequestFolderMutexSem)_pfn_wpRequestFolderMutexSem)(somSelf,
                                                                            ulTimeout);

    return (ulrc);
}

/*
 *@@ fdrReleaseFolderMutexSem:
 *      calls WPFolder::wpReleaseFolderMutexSem. This is
 *      the reverse to fdrRequestFolderMutexSem.
 *
 *      Returns:
 *
 *      --  NO_ERROR: semaphore was successfully requested.
 *
 *      --  -1: error resolving method. A log entry should
 *          have been added also.
 *
 *      --  other: error codes from DosReleaseMutexSem probably.
 *
 *@@added V0.9.6 (2000-10-25) [umoeller]
 *@@changed V0.9.16 (2001-10-25) [umoeller]: moved this here from wpsh.c, changed prefix
 *@@changed V0.9.16 (2001-10-25) [umoeller]: now caching method pointer in folder instance data
 */

ULONG fdrReleaseFolderMutexSem(WPFolder *somSelf)
{
    ULONG ulrc = -1;

    XFolderData *somThis = XFolderGetData(somSelf);

    if (!_pfn_wpReleaseFolderMutexSem)
        // first call for this folder:
        _pfn_wpReleaseFolderMutexSem
            = (PVOID)wpshResolveFor(somSelf,
                                    NULL, // use somSelf's class
                                    "wpReleaseFolderMutexSem");

    if (_pfn_wpReleaseFolderMutexSem)
        ulrc = ((xfTD_wpReleaseFolderMutexSem)_pfn_wpReleaseFolderMutexSem)(somSelf);

    return (ulrc);
}

/*
 *@@ GetMonitorObject:
 *      gets the RWMonitor object for the specified
 *      folder. See fdrRequestFolderWriteMutexSem.
 *
 *@@added V0.9.16 (2001-10-25) [umoeller]
 */

SOMAny* GetMonitorObject(WPFolder *somSelf)
{
#ifdef __DEBUG__
    if (_somIsA(somSelf, _WPFolder))
#endif
    {
        XFolderData *somThis = XFolderGetData(somSelf);

        if (!_pMonitor)
        {
            // first call on this folder:
            xfTD_wpQueryRWMonitorObject pwpQueryRWMonitorObject;

            if (pwpQueryRWMonitorObject = (xfTD_wpQueryRWMonitorObject)wpshResolveFor(
                                                         somSelf,
                                                         NULL,
                                                         "wpQueryRWMonitorObject"))
            {
                // get the object and store it in the instance data
                _pMonitor = pwpQueryRWMonitorObject(somSelf);
            }
        }

        return (_pMonitor);
    }

#ifdef __DEBUG__
    cmnLog(__FILE__, __LINE__, __FUNCTION__,
           "Function invoked on non-folder object.");
    return (NULL);
#endif
}

/*
 *@@ fdrRequestFolderWriteMutexSem:
 *      requests the "folder write" mutex for the folder.
 *
 *      Apparently this extra semaphore gets requested
 *      each time before the folder's contents are actually
 *      modified. It gets requested for every single
 *      object that is added or removed from the contents.
 *
 *      Strangely, this semaphore is implemented through
 *      an extra SOM class called RWMonitor, which is not
 *      derived from WPObject, but from SOMObject directly.
 *      You can see that class with the "WPS class list"
 *      object if you enable "Show all SOM classes" in its
 *      settings notebook.
 *
 *      Because that class is completely undocumented and
 *      not prototyped, I have added this helper func
 *      to request the folder write mutex from this object.
 *
 *      Returns:
 *
 *      --  NO_ERROR: semaphore was successfully requested.
 *
 *      --  -1: error resolving methods. A log entry should
 *          have been added also.
 *
 *      --  other: error codes from DosRequestMutexSem probably.
 *
 *@@added V0.9.16 (2001-10-25) [umoeller]
 */

ULONG fdrRequestFolderWriteMutexSem(WPFolder *somSelf)
{
    ULONG ulrc = -1;

    SOMAny *pMonitor;

    if (pMonitor = GetMonitorObject(somSelf))
    {
        // this is never overridden so we can safely use
        // a static variable here
        static xfTD_RequestWrite pRequestWrite = NULL;

        if (!pRequestWrite)
            // first call:
            pRequestWrite = (xfTD_RequestWrite)wpshResolveFor(
                                           pMonitor,
                                           NULL,
                                           "RequestWrite");

        if (pRequestWrite)
            ulrc = pRequestWrite(pMonitor);
    }

    return (ulrc);
}

/*
 *@@ fdrReleaseFolderWriteMutexSem:
 *      releases the "folder write" mutex requested by
 *      fdrRequestFolderWriteMutexSem.
 *
 *@@added V0.9.16 (2001-10-25) [umoeller]
 */

ULONG fdrReleaseFolderWriteMutexSem(WPFolder *somSelf)
{
    ULONG ulrc = -1;

    SOMAny *pMonitor;

    if (pMonitor = GetMonitorObject(somSelf))
    {
        static xfTD_ReleaseWrite pReleaseWrite = NULL;

        if (!pReleaseWrite)
            pReleaseWrite = (xfTD_ReleaseWrite)wpshResolveFor(
                                           pMonitor,
                                           NULL,
                                           "ReleaseWrite");

        if (pReleaseWrite)
            ulrc = pReleaseWrite(pMonitor);
    }

    return (ulrc);
}

/*
 *@@ fdrRequestFindMutexSem:
 *      mutex requested by the populate thread to prevent
 *      multiple populates, apparently. From my testing,
 *      this semaphore is requested any time an object is
 *      being made awake (created in memory).
 *
 *      Returns:
 *
 *      --  NO_ERROR: semaphore was successfully requested.
 *
 *      --  -1: error resolving method. A log entry should
 *          have been added also.
 *
 *      --  other: error codes from DosRequestMutexSem probably.
 *
 *      As with the folder mutex sem, proper serialization
 *      must be applied to avoid mutexes locking each other
 *      out.
 *
 *      Apparently, the WPS uses the following order when
 *      it requests this mutex:
 *
 *      1)  request find mutex
 *
 *      2)  request folder mutex,
 *          while (wpQueryContent, ...)
 *          release folder mutex
 *
 *      3)  awake/create object
 *
 *      4)  release find mutex
 *
 *      Whatever you do, never request the find AFTER the
 *      folder mutex.
 *
 *@@added V0.9.9 (2001-03-11) [umoeller]
 *@@changed V0.9.16 (2001-10-25) [umoeller]: moved this here from wpsh.c, changed prefix
 */

ULONG fdrRequestFindMutexSem(WPFolder *somSelf,
                             ULONG ulTimeout)
{
    ULONG ulrc = -1;

    xfTD_wpRequestFindMutexSem _wpRequestFindMutexSem;

    if (_wpRequestFindMutexSem
            = (xfTD_wpRequestFindMutexSem)wpshResolveFor(somSelf,
                                                         NULL, // use somSelf's class
                                                         "wpRequestFindMutexSem"))
        ulrc = _wpRequestFindMutexSem(somSelf, ulTimeout);

    return (ulrc);
}

/*
 *@@ fdrReleaseFindMutexSem:
 *      calls WPFolder::wpReleaseFindMutexSem. This is
 *      the reverse to fdrRequestFindMutexSem.
 *
 *@@added V0.9.9 (2001-03-11) [umoeller]
 *@@changed V0.9.16 (2001-10-25) [umoeller]: moved this here from wpsh.c, changed prefix
 */

ULONG fdrReleaseFindMutexSem(WPFolder *somSelf)
{
    ULONG ulrc = -1;

    xfTD_wpReleaseFindMutexSem _wpReleaseFindMutexSem;

    if (_wpReleaseFindMutexSem
            = (xfTD_wpReleaseFindMutexSem)wpshResolveFor(somSelf,
                                                         NULL, // use somSelf's class
                                                         "wpReleaseFindMutexSem"))
        ulrc = _wpReleaseFindMutexSem(somSelf);

    return (ulrc);
}

/*
 *@@ fdrFlushNotifications:
 *      invokes WPFolder::wpFlushNotifications, which
 *      is only published with the Warp 4 toolkit.
 *
 *@@added V0.9.6 (2000-10-25) [umoeller]
 *@@changed V0.9.16 (2001-10-25) [umoeller]: moved this here from wpsh.c, changed prefix
 */

ULONG fdrFlushNotifications(WPFolder *somSelf)
{
    ULONG ulrc = 0;

    xfTD_wpFlushNotifications _wpFlushNotifications;

    if (_wpFlushNotifications
        = (xfTD_wpFlushNotifications)wpshResolveFor(somSelf,
                                                    NULL, // use somSelf's class
                                                    "wpFlushNotifications"))
        ulrc = _wpFlushNotifications(somSelf);

    return (ulrc);
}

/*
 *@@ fdrGetNotifySem:
 *      calls M_WPFolder::wpclsGetNotifySem to lock out
 *      the WPS auto-refresh-folder threads.
 *
 *      Note that this requests a system-wide lock.
 *
 *@@added V0.9.6 (2000-10-25) [umoeller]
 *@@changed V0.9.16 (2001-10-25) [umoeller]: moved this here from wpsh.c, changed prefix
 */

BOOL fdrGetNotifySem(ULONG ulTimeout)
{
    BOOL brc = FALSE;

    static xfTD_wpclsGetNotifySem _wpclsGetNotifySem = NULL;

    M_WPFolder *pWPFolder = _WPFolder;
            // THIS RETURNS NULL UNTIL THE FOLDER CLASS IS INITIALIZED

    if (pWPFolder)
    {
        if (!_wpclsGetNotifySem)
        {
            // first call: resolve...
            _wpclsGetNotifySem = (xfTD_wpclsGetNotifySem)wpshResolveFor(
                                                pWPFolder,
                                                NULL,
                                                "wpclsGetNotifySem");
        }

        if (_wpclsGetNotifySem)
            brc = _wpclsGetNotifySem(pWPFolder, ulTimeout);
    }

    return (brc);
}

/*
 *@@ fdrReleaseNotifySem:
 *
 *@@added V0.9.6 (2000-10-25) [umoeller]
 *@@changed V0.9.16 (2001-10-25) [umoeller]: moved this here from wpsh.c, changed prefix
 */

VOID fdrReleaseNotifySem(VOID)
{
    static xfTD_wpclsReleaseNotifySem _wpclsReleaseNotifySem = NULL;

    M_WPFolder *pWPFolder = _WPFolder;
            // THIS RETURNS NULL UNTIL THE FOLDER CLASS IS INITIALIZED

    if (pWPFolder)
    {
        if (!_wpclsReleaseNotifySem)
        {
            // first call: resolve...
            _wpclsReleaseNotifySem = (xfTD_wpclsReleaseNotifySem)wpshResolveFor(
                                                pWPFolder,
                                                NULL,
                                                "wpclsReleaseNotifySem");
        }

        if (_wpclsReleaseNotifySem)
            _wpclsReleaseNotifySem(pWPFolder);
    }
}

/* ******************************************************************
 *
 *   Folder content management
 *
 ********************************************************************/

/*
 *@@ fdrResolveContentPtrs:
 *      resolves the pointers to WPFolder's "first object"
 *      and "last object" pointers. IBM was nice enough
 *      to declare these instance variables as SOM attributes,
 *      so there are exported attribute methods (_getFirstObj
 *      and _getLastObj) which return exactly the addresses
 *      of these two variables.
 *
 *      In order to avoid having to resolve these functions
 *      for every access, we resolve them once and store
 *      the addresses of the two attributes directly in
 *      the XFolder instance variables (_ppFirstObj and
 *      _ppLastObj). They will never change.
 *
 *      Preconditions:
 *
 *      -- the caller must hold the folder mutex. Yes,
 *         this affects XFolder instance variables, so
 *         the object mutex would be the first to think
 *         of... but we put up this requirement instead.
 *
 *@@added V0.9.7 (2001-01-13) [umoeller]
 */

BOOL fdrResolveContentPtrs(WPFolder *somSelf)
{
    BOOL brc = FALSE;
    XFolderData *somThis = XFolderGetData(somSelf);

    if ((!_ppFirstObj) || (!_ppLastObj))
    {
        // method ptrs
        xfTD_get_FirstObj __getFirstObj = NULL;
        xfTD_get_LastObj __getLastObj = NULL;

        // first obj
        __getFirstObj
            = (xfTD_get_FirstObj)wpshResolveFor(somSelf,
                                                _somGetClass(somSelf),
                                                "_get_FirstObj");
        if (__getFirstObj)
            _ppFirstObj = __getFirstObj(somSelf);

        // last obj
        __getLastObj
            = (xfTD_get_LastObj)wpshResolveFor(somSelf,
                                               _somGetClass(somSelf),
                                               "_get_LastObj");
        if (__getLastObj)
            _ppLastObj = __getLastObj(somSelf);

        if ((_ppFirstObj) && (_ppLastObj))
            // both succeeded:
            brc = TRUE;
    }
    else
        brc = TRUE;

    return (brc);
}

/*
 *@@ FindFSFromUpperName:
 *
 *@@added V0.9.16 (2001-10-25) [umoeller]
 */

WPObject* FindFSFromUpperName(WPFolder *pFolder,
                              const char *pcszUpperShortName)
{
    WPObject *pobjReturn = NULL;
    BOOL fFolderLocked = FALSE;

    TRY_LOUD(excpt1)
    {
        if (fFolderLocked = !fdrRequestFolderMutexSem(pFolder, SEM_INDEFINITE_WAIT))
        {
            XFolderData *somThis = XFolderGetData(pFolder);
            PFDRCONTENTITEM pNode;

            if (pNode = (PFDRCONTENTITEM)treeFind(
                                 ((PFDRCONTENTS)_pvFdrContents)->FileSystemsTreeRoot,
                                 (ULONG)pcszUpperShortName,
                                 treeCompareStrings))
            {
                pobjReturn = pNode->pobj;
            }
        }
    }
    CATCH(excpt1)
    {
        pobjReturn = NULL;
    } END_CATCH();

    if (fFolderLocked)
        fdrReleaseFolderMutexSem(pFolder);

    return (pobjReturn);
}

/*
 *@@ fdrFindFSFromName:
 *      goes thru the folder contents to find the first
 *      file-system object with the specified real name
 *      (not title!).
 *
 *      pcszShortName is looked for without respect to
 *      case.
 *
 *@@added V0.9.9 (2001-02-01) [umoeller]
 *@@changed V0.9.16 (2001-10-25) [umoeller]: now using fast content trees
 */

WPObject* fdrFindFSFromName(WPFolder *pFolder,
                            const char *pcszShortName)  // in: short real name to look for
{
    // protect the folder contents (lock out anyone changing them)
    if (    pcszShortName
         && (*pcszShortName)
       )
    {
        // all the following rewritten V0.9.16 (2001-10-25) [umoeller]

        // avoid two strlen's (speed)
        ULONG   ulLength = strlen(pcszShortName);

        // upper-case the short name
        PSZ pszUpperRealName = _alloca(ulLength + 1);
        memcpy(pszUpperRealName, pcszShortName, ulLength + 1);
        nlsUpper(pszUpperRealName, ulLength);

        return (FindFSFromUpperName(pFolder,
                                    pszUpperRealName));

        /* if (fdrResolveContentPtrs(pFolder))
        {
            XFolderData *somThis = XFolderGetData(pFolder);

            WPObject *pobj = *_ppFirstObj;

            while (pobj)
            {
                if (_somIsA(pobj, _WPFileSystem))
                {
                    // check name
                    CHAR szFilename[CCHMAXPATH];
                    if (_wpQueryFilename(pobj, szFilename, FALSE))
                        if (!stricmp(szFilename, pcszShortName))
                        {
                            // got it:
                            pobjReturn = pobj;
                            break;
                        }
                }

                pobj = _xwpQueryNextObj(pobj);
            }
        } */
    }

    return (NULL);
}

/*
 *@@ HackContentPointers:
 *
 *@@added V0.9.16 (2001-10-25) [umoeller]
 */

BOOL HackContentPointers(WPFolder *somSelf,
                         XFolderData *somThis,
                         WPObject *pObject)
{
    BOOL brc = FALSE;

    if (pObject)
    {
        BOOL fSubObjectLocked = FALSE;

        TRY_LOUD(excpt1)        // V0.9.9 (2001-04-01) [umoeller]
        {
            if (fSubObjectLocked = !_wpRequestObjectMutexSem(pObject, SEM_INDEFINITE_WAIT))
            {
                // this strange thing gets called by the original
                // wpAddToContent... it's a flag which shows whether
                // the object is in any container at all. We want
                // to set this to TRUE.
                PULONG pulContainerFlag = _wpQueryContainerFlagPtr(pObject);
                if (!*pulContainerFlag)
                   *pulContainerFlag = TRUE;

                // _Pmpf((__FUNCTION__ ": adding %s to %d",
                //         _wpQueryTitle(pObject),
                //         _wpQueryTitle(somSelf) ));

                // set the new object's "next object" to NULL
                _xwpSetNextObj(pObject, NULL);

                if (fdrResolveContentPtrs(somSelf))
                {
                    // now check our contents...
                    if (*_ppFirstObj)
                    {
                        // we had objects before:
                        // store new object as next object for
                        // previously last object
                        WPObject **ppObjNext = wpshGetNextObjPointer(*_ppLastObj);
                        if (ppObjNext)
                            *ppObjNext = pObject;
                        // store new object as new last object
                        *_ppLastObj = pObject;
                    }
                    else
                    {
                        // no objects yet:
                        *_ppFirstObj = pObject;
                        *_ppLastObj = pObject;
                    }

                    // for each object that was added, lock
                    // the folder...
                    _wpLockObject(somSelf);

                    brc = TRUE;
                }
            }
        }
        CATCH(excpt1)
        {
            brc = FALSE;
        } END_CATCH();

        // release the mutexes in reverse order V0.9.9 (2001-04-01) [umoeller]
        if (fSubObjectLocked)
            _wpReleaseObjectMutexSem(pObject);
    }

    return (brc);
}

/*
 *@@ fdrAddToContent:
 *      implementation for the XFolder::wpAddToContent override.
 *
 *@@changed V0.9.9 (2001-04-02) [umoeller]: fixed mutex release order
 *@@changed V0.9.9 (2001-04-02) [umoeller]: removed object mutex request on folder
 *@@changed V0.9.16 (2001-10-25) [umoeller]: moved old code to HackContentPointers, added tree maintenance
 */

BOOL fdrAddToContent(WPFolder *somSelf,
                     WPObject *pObject,
                     BOOL *pfCallParent)        // out: call parent method
{
    BOOL    brc = TRUE,
            fFolderLocked = FALSE,
            fLog = FALSE;

    *pfCallParent = TRUE;

    TRY_LOUD(excpt1)
    {
        if (fFolderLocked = !fdrRequestFolderWriteMutexSem(somSelf))
        {
            XFolderData *somThis = XFolderGetData(somSelf);

            if (_fDisableAutoCnrAdd)
            {
                // do not call the parent!!
                *pfCallParent = FALSE;
                // call our own implementation instead
                brc = HackContentPointers(somSelf, somThis, pObject);
            }

            if (brc)
            {
                // raise total objects count
                PFDRCONTENTITEM pNew;

                _cObjects++;

                // add to contents tree
                if (_somIsA(pObject, _WPFileSystem))
                {
                    // WPFileSystem added:
                    // add a new tree node and sort it according
                    // to the object's upper-case real name
                    PSZ pszUpperRealName;
                    if (    (pszUpperRealName = _xwpQueryUpperRealName(pObject))
                         && (pNew = NEW(FDRCONTENTITEM))
                       )
                    {
                        pNew->Tree.ulKey = (ULONG)pszUpperRealName;
                        pNew->pobj = pObject;

                        if (treeInsert(&((PFDRCONTENTS)_pvFdrContents)->FileSystemsTreeRoot,
                                       &((PFDRCONTENTS)_pvFdrContents)->cFileSystems,
                                       (TREE*)pNew,
                                       treeCompareStrings))
                            fLog = TRUE;
                    }
                }
                else if (_somIsA(pObject, _WPAbstract))
                {
                    // WPAbstract added:
                    // add a new tree node and sort it according
                    // to the object's 32-bit handle (this is safe
                    // because abstracts _always_ have a handle)
                    HOBJECT hobj;
                    if (    (hobj = _wpQueryHandle(pObject))
                         && (pNew = NEW(FDRCONTENTITEM))
                       )
                    {
                        // upper case!
                        pNew->Tree.ulKey = hobj;
                        pNew->pobj = pObject;

                        if (treeInsert(&((PFDRCONTENTS)_pvFdrContents)->AbstractsTreeRoot,
                                       &((PFDRCONTENTS)_pvFdrContents)->cAbstracts,
                                       (TREE*)pNew,
                                       treeCompareKeys))
                            fLog = TRUE;
                    }
                }
            }
        }
    }
    CATCH(excpt1)
    {
        brc = FALSE;
    } END_CATCH();

    if (fFolderLocked)
        fdrReleaseFolderWriteMutexSem(somSelf);

    if (fLog)
        cmnLog(__FILE__, __LINE__, __FUNCTION__,
               "treeInsert failed for %s",
               _wpQueryTitle(pObject));

    return (brc);
}

/*
 *@@ fdrRealNameChanged:
 *      called by XWPFileSystem::wpSetTitleAndRenameFile
 *      when an object's real name has changed. We then
 *      need to update our tree of FS objects which is
 *      sorted by real names.
 *
 *      This also sets a new upper-case real name in
 *      the file-system object's instance data.
 *
 *      Preconditions:
 *
 *      --  caller must hold the folder write mutex sem.
 *
 *@@added V0.9.16 (2001-10-25) [umoeller]
 */

BOOL fdrRealNameChanged(WPFolder *somSelf,          // in: folder of pFSObject
                        WPObject *pFSObject)        // in: FS object who is changing
{
    BOOL    brc = FALSE,
            fFailed = 0;

    PSZ     pszOldRealName;

    if (pszOldRealName = _xwpQueryUpperRealName(pFSObject))
    {
        CHAR szNewUpperRealName[CCHMAXPATH];
        XFolderData *somThis = XFolderGetData(somSelf);
        PFDRCONTENTITEM pNode;

        _wpQueryFilename(pFSObject, szNewUpperRealName, FALSE);
        nlsUpper(szNewUpperRealName, 0);

        if (strcmp(pszOldRealName, szNewUpperRealName))
        {
            // find the old real name in the tree
            if (pNode = (PFDRCONTENTITEM)treeFind(
                             ((PFDRCONTENTS)_pvFdrContents)->FileSystemsTreeRoot,
                             (ULONG)pszOldRealName,
                             treeCompareStrings))
            {
                // 1) remove that node from the tree
                if (!treeDelete(&((PFDRCONTENTS)_pvFdrContents)->FileSystemsTreeRoot,
                                &((PFDRCONTENTS)_pvFdrContents)->cFileSystems,
                                (TREE*)pNode))
                {
                    // 2) set the new real name on the fs object
                    XWPFileSystemData *somThat = XWPFileSystemGetData(pFSObject);

                    // update the fs object's instance data
                    free(somThat->pszUpperRealName);
                    somThat->pszUpperRealName = strdup(szNewUpperRealName);
                    // refresh the tree node to point to the new buffer
                    pNode->Tree.ulKey = (ULONG)somThat->pszUpperRealName;

                    // 3) re-insert
                    if (!treeInsert(&((PFDRCONTENTS)_pvFdrContents)->FileSystemsTreeRoot,
                                    &((PFDRCONTENTS)_pvFdrContents)->cFileSystems,
                                    (TREE*)pNode,
                                    treeCompareStrings))
                    {
                        brc = TRUE;
                    }
                    else
                        fFailed = 3;
                }
                else
                    fFailed = 2;
            }
            else
                fFailed = 1;

            if (!brc)
                cmnLog(__FILE__, __LINE__, __FUNCTION__,
                       "tree funcs failed (%d), for old \"%s\", new \"%s\"",
                       fFailed,
                       pszOldRealName,
                       szNewUpperRealName);
        } // end if (strcmp(pszOldRealName, szNewUpperRealName))
    }

    return (brc);
}

/*
 *@@ fdrDeleteFromContent:
 *      implementation for the XFolder::wpDeleteFromContent override.
 *
 *@@added V0.9.16 (2001-10-25) [umoeller]
 */

BOOL fdrDeleteFromContent(WPFolder *somSelf,
                          WPObject* pObject)
{
    BOOL    brc = TRUE,
            fFolderLocked = FALSE;

    TRY_LOUD(excpt1)
    {
        if (fFolderLocked = !fdrRequestFolderWriteMutexSem(somSelf))
        {
            XFolderData *somThis = XFolderGetData(somSelf);
            PFDRCONTENTITEM pNode;

            _cObjects--;

            // remove from contents tree
            if (_somIsA(pObject, _WPFileSystem))
            {
                // removing WPFileSystem:
                PCSZ pcszUpperRealName;
                if (pNode = (PFDRCONTENTITEM)treeFind(
                                     ((PFDRCONTENTS)_pvFdrContents)->FileSystemsTreeRoot,
                                     (ULONG)_xwpQueryUpperRealName(pObject),
                                     treeCompareStrings))
                {
                    if (!treeDelete(&((PFDRCONTENTS)_pvFdrContents)->FileSystemsTreeRoot,
                                    &((PFDRCONTENTS)_pvFdrContents)->cFileSystems,
                                    (TREE*)pNode))
                        brc = TRUE;
                }
            }
            else if (_somIsA(pObject, _WPAbstract))
            {
                // removing WPAbstract:
                if (pNode = (PFDRCONTENTITEM)treeFind(
                                     ((PFDRCONTENTS)_pvFdrContents)->AbstractsTreeRoot,
                                     (ULONG)_wpQueryHandle(pObject),
                                     treeCompareKeys))
                {
                    if (!treeDelete(&((PFDRCONTENTS)_pvFdrContents)->AbstractsTreeRoot,
                                    &((PFDRCONTENTS)_pvFdrContents)->cAbstracts,
                                    (TREE*)pNode))
                        brc = TRUE;
                }
            }
        }
    }
    CATCH(excpt1)
    {
        brc = FALSE;
    } END_CATCH();

    if (fFolderLocked)
        fdrReleaseFolderWriteMutexSem(somSelf);

    if (!brc)
        cmnLog(__FILE__, __LINE__, __FUNCTION__,
               "tree funcs failed for %s",
               _wpQueryTitle(pObject));

    return (brc);
}

/*
 *@@ fdrQueryContent:
 *      implementation for the XFolder::wpQueryContent override.
 *
 *      NOTE: The caller must lock the folder contents BEFORE
 *      the call. We can't this here because we can't guarantee
 *      that the folder's content list will remain valid after
 *      the call, unless the caller's processing is also protected.
 *
 *      The original implementation (WPFolder::wpQueryContent)
 *      appears to call folder mutex methods. I have not done
 *      this here. So far, it works.
 *
 *      This code ONLY gets called if XFolder::xwpSetDisableCnrAdd
 *      was called.
 */

WPObject* fdrQueryContent(WPFolder *somSelf,
                          WPObject *pobjFind,
                          ULONG ulOption)
{
    WPObject *pobjReturn = NULL;

    TRY_LOUD(excpt1)
    {
        if (fdrResolveContentPtrs(somSelf))
        {
            XFolderData *somThis = XFolderGetData(somSelf);

            switch (ulOption)
            {
                case QC_FIRST:
                    // that's easy
                    pobjReturn = *_ppFirstObj;
                break;

                case QC_NEXT:
                    if (pobjFind)
                    {
                        WPObject **ppObjNext = wpshGetNextObjPointer(pobjFind);
                        if (ppObjNext)
                            pobjReturn = *ppObjNext;
                    }
                break;

                case QC_LAST:
                    pobjReturn = *_ppLastObj;
                break;
            }
        }
    }
    CATCH(excpt1)
    {
        pobjReturn = NULL;
    } END_CATCH();

    return (pobjReturn);
}

/*
 *@@ fdrQueryContentArray:
 *      returns an array of WPObject* pointers representing
 *      the folder contents. This does NOT populate the
 *      folder, but will only return the objects that are
 *      presently awake.
 *
 *      Returns NULL if the folder is empty. Otherwise
 *      *pulItems receives the array item count (NOT the
 *      array size).
 *
 *      If (flFilter & QCAFL_FILTERINSERTED), this checks each
 *      object and returns only those which have not been
 *      inserted into any container yet. Useful for
 *      wpclsInsertMultipleObjects, which will simply fail if
 *      any object has already been inserted.
 *
 *      Use free() to release the memory allocated here.
 *
 *      NOTE: The caller must lock the folder contents BEFORE
 *      the call. We can't this here because we can't guarantee
 *      that the array will remain valid after the call, unless
 *      the caller's processing is also protected.
 *
 *@@added V0.9.7 (2001-01-13) [umoeller]
 *@@changed V0.9.9 (2001-04-02) [umoeller]: added flFilter
 */

WPObject** fdrQueryContentArray(WPFolder *pFolder,
                                ULONG flFilter,           // in: filter flags
                                PULONG pulItems)
{
    WPObject** paObjects = NULL;

    TRY_LOUD(excpt1)
    {
        XFolderData *somThis = XFolderGetData(pFolder);
        if (_cObjects)
        {
            paObjects = (WPObject**)malloc(sizeof(WPObject*) * _cObjects);
            if (paObjects)
            {
                WPObject **ppThis = paObjects;
                WPObject *pObject;

                ULONG ul = 0;
                // somTD_WPFolder_wpQueryContent rslv_wpQueryContent
                        // = (somTD_WPFolder_wpQueryContent)wpshResolveFor(pFolder, NULL, "wpQueryContent");

                // V0.9.16 (2001-11-01) [umoeller]: now using wpshGetNextObjPointer
                for (   pObject = _wpQueryContent(pFolder, NULL, QC_FIRST);
                        pObject;
                        pObject = *wpshGetNextObjPointer(pObject))
                {
                    // add object if either filter is off,
                    // or if no RECORDITEM exists yet
                    if (    (!(flFilter & QCAFL_FILTERINSERTED))
                         || (!(_wpFindUseItem(pObject, USAGE_RECORD, NULL)))
                       )
                    {
                        // store obj
                        *ppThis = pObject;
                        // advance ptr
                        ppThis++;
                        // raise count
                        ul++;
                    }

                    if (ul >= _cObjects)
                        // shouldn't happen, but we don't want to
                        // crash the array
                        break;
                }

                *pulItems = ul;
            }
        }
    }
    CATCH(excpt1)
    {
        if (paObjects)
        {
            free(paObjects);
            paObjects = NULL;
        }
    } END_CATCH();

    return (paObjects);
}

/*
 *@@ fdrNukeContents:
 *      deletes all the folder contents without any
 *      confirmations by invoking wpFree on each awake
 *      object. This does NOT populate the folder.
 *
 *      Note that this is not a polite way of cleaning
 *      a folder. This is ONLY used by
 *      XWPFontFolder::wpDeleteContents and
 *      XWPTrashCan::wpDeleteContents to nuke all the
 *      transient objects before those special folders
 *      get deleted themselves. This avoids the stupid
 *      "cannot delete object" messages the WPS would
 *      otherwise produce for each transient object.
 *
 *      DO NOT INVOKE THIS FUNCTION ON REGULAR FOLDERS.
 *      THERE'S NO WAY TO INTERRUPT THIS PROCESSING.
 *      THIS WOULD ALSO DELETE ALL FILES IN THE FOLDER
 *      AND ALL SUBFOLDERS.
 *
 *      Returns FALSE if killing one of the objects
 *      failed.
 *
 *@@added V0.9.9 (2001-02-08) [umoeller]
 *@@changed V0.9.12 (2001-04-29) [umoeller]: removed wpQueryContent calls
 */

BOOL fdrNukeContents(WPFolder *pFolder)
{
    BOOL        brc = FALSE,
                fFolderLocked = FALSE;

    TRY_LOUD(excpt1)
    {
        if (fFolderLocked = !fdrRequestFolderMutexSem(pFolder, SEM_INDEFINITE_WAIT))
        {
            ULONG   cObjects = 0,
                    ul;
            // now querying content array... we can't use wpQueryContent
            // while we're deleting the objects! V0.9.12 (2001-04-29) [umoeller]
            WPObject** papObjects = fdrQueryContentArray(pFolder,
                                                         0,     // no filter
                                                         &cObjects);
            brc = TRUE;

            for (ul = 0;
                 ul < cObjects;
                 ul++)
            {
                WPObject *pObject = papObjects[ul];
                if (!_wpFree(pObject))
                {
                    // error:
                    brc = FALSE;
                    // and stop
                    break;
                }
            }
        }
    }
    CATCH(excpt1)
    {
        brc = FALSE;
    } END_CATCH();

    if (fFolderLocked)
        fdrReleaseFolderMutexSem(pFolder);

    return (brc);
}

/* ******************************************************************
 *
 *   Awake-objects test
 *
 ********************************************************************/

static LINKLIST     G_llRootFolders;        // linked list of root folders,
                                            // holding plain WPFolder pointers
                                            // (no auto-free, of course)
static HMTX         G_hmtxRootFolders = NULLHANDLE;

// last folder cache
extern WPFolder     *G_pLastQueryAwakeFolder = NULL;
                        // this must be exported because if this goes
                        // dormant, XFolder::wpUnInitData must null this
static CHAR         G_szLastQueryAwakeFolderPath[CCHMAXPATH];

/*
 *@@ LockRootFolders:
 *      locks G_hmtxRootFolders. Creates the mutex on
 *      the first call.
 *
 *      Returns TRUE if the mutex was obtained.
 *
 *@@added V0.9.16 (2001-10-25) [umoeller]
 */

BOOL LockRootFolders(VOID)
{
    if (G_hmtxRootFolders)
        return (!WinRequestMutexSem(G_hmtxRootFolders, SEM_INDEFINITE_WAIT));
            // WinRequestMutexSem works even if the thread has no message queue

    if (!DosCreateMutexSem(NULL,
                           &G_hmtxRootFolders,
                           0,
                           TRUE))      // request!
    {
        lstInit(&G_llRootFolders,
                FALSE);     // no auto-free
        return (TRUE);
    }

    return (FALSE);
}

/*
 *@@ UnlockRootFolders:
 *      the reverse to LockRootFolders.
 *
 *@@added V0.9.16 (2001-10-25) [umoeller]
 */

VOID UnlockRootFolders(VOID)
{
    DosReleaseMutexSem(G_hmtxRootFolders);
}

/*
 *@@ fdrRegisterAwakeRootFolder:
 *
 *@@added V0.9.16 (2001-10-25) [umoeller]
 */

BOOL fdrRegisterAwakeRootFolder(WPFolder *somSelf)
{
    BOOL brc = FALSE,
         fLocked = FALSE;

    TRY_LOUD(excpt1)
    {
        if (fLocked = LockRootFolders())
        {
            CHAR sz[CCHMAXPATH] = "???";
            _wpQueryFilename(somSelf, sz, TRUE);
                // this gives us "M:", not "M:\" !

            lstAppendItem(&G_llRootFolders,
                          somSelf);

            _xwpModifyListNotify(somSelf,
                                 OBJLIST_QUERYAWAKEFSOBJECT,
                                 OBJLIST_QUERYAWAKEFSOBJECT);

            brc = TRUE;
        }
    }
    CATCH(excpt1)
    {
        brc = FALSE;
    } END_CATCH();

    if (fLocked)
        UnlockRootFolders();

    return (brc);
}

/*
 *@@ fdrRemoveAwakeRootFolder:
 *      called from XFldObject::wpUnInitData when the
 *      root folder goes dormant again. Note that
 *      this also gets called for any folder that
 *      was in the "awake folder" cache.
 *
 *@@added V0.9.16 (2001-10-25) [umoeller]
 */

BOOL fdrRemoveAwakeRootFolder(WPFolder *somSelf)
{
    BOOL brc = FALSE,
         fLocked = FALSE;

    TRY_LOUD(excpt1)
    {
        if (fLocked = LockRootFolders())
        {
            lstRemoveItem(&G_llRootFolders,
                          somSelf);
                    // can fail if this wasn't really a root folder
                    // but only touched by the cache

            // invalidate cache
            G_pLastQueryAwakeFolder = NULL;

            brc = TRUE;
        }
    }
    CATCH(excpt1)
    {
        brc = FALSE;
    } END_CATCH();

    if (fLocked)
        UnlockRootFolders();

    return (brc);
}

/*
 *@@ ProcessParticles:
 *
 *      Preconditions:
 *
 *      --  pStartOfParticle must point to the
 *          first particle name. For example,
 *          with C:\FOLDER\FILE.TXT, this must
 *          point to the FOLDER particle.
 *
 *      --  The path must be in upper case.
 *
 *      --  Caller must hold root folder mutex.
 *
 *      --  The path must not end in '\\'.
 *
 *      Postconditions:
 *
 *      --  This trashes the string buffer.
 *
 *@@added V0.9.16 (2001-10-25) [umoeller]
 */

WPFileSystem* ProcessParticles(WPFolder *pCurrentFolder,
                               PSZ pStartOfParticle)
{
    // 0123456789012
    // C:\FOLDER\XXX
    //    ^ start of particle

    BOOL fQuit = FALSE;

    while (    (pStartOfParticle)
            && (*pStartOfParticle)
            && (!fQuit)
          )
    {
        PSZ pEndOfParticle;
        ULONG ulLengthThis;
        if (pEndOfParticle = strchr(pStartOfParticle, '\\'))
        {
            // 0123456789012
            // C:\FOLDER\XXX
            //    |     ^ end of particle
            //    ^ start of particle
            ulLengthThis =   pEndOfParticle
                           - pStartOfParticle;
            // overwrite backslash
            *pEndOfParticle = '\0';
        }
        else
        {
            // no more backslashes:
            ulLengthThis = strlen(pStartOfParticle);
            fQuit = TRUE;
        }

        if (ulLengthThis)
        {
            // ask the folder if this file name is awake
            WPFileSystem *pFound;
            // _Pmpf(("Scanning particle %s", pStartOfParticle));
            if (pFound = FindFSFromUpperName(pCurrentFolder,
                                             pStartOfParticle))
            {
                // found something:
                if (fQuit)
                {
                    // we're at the last node already:
                    // update the "last folder" cache
                    if (pCurrentFolder != G_pLastQueryAwakeFolder)
                    {
                        G_pLastQueryAwakeFolder = pCurrentFolder;
                        _wpQueryFilename(pCurrentFolder,
                                         G_szLastQueryAwakeFolderPath,
                                         TRUE);
                        nlsUpper(G_szLastQueryAwakeFolderPath, 0);

                        // set the flag in the instance data
                        // so the cache ptr is invalidated
                        // once this thing goes dormant
                        _xwpModifyListNotify(pCurrentFolder,
                                             OBJLIST_QUERYAWAKEFSOBJECT,
                                             OBJLIST_QUERYAWAKEFSOBJECT);
                    }
                    // return this
                    return (pFound);
                }
                else
                {
                    pCurrentFolder = pFound;
                    // search on after that backslash
                    // C:\FOLDER\XXX
                    //          ^ end of particle
                    pStartOfParticle = pEndOfParticle + 1;
                    // C:\FOLDER\XXX
                    //           ^ new start of particle
                }
            }
            else
                // not awake:
                fQuit = TRUE;
        }
        else
            fQuit = TRUE;
    } // end while (    (pStartOfParticle)
           //         && (!fQuit)

    return (NULL);
}

/*
 *@@ fdrQueryAwakeFSObject:
 *      returns the WPFileSystem for the specified
 *      full path if it is already awake, or NULL
 *      if it is not.
 *
 *      As opposed to M_WPFileSystem::wpclsQueryAwakeObject,
 *      this uses the fast XFolder content trees.
 *
 *      This compares without respect to case, but cannot
 *      handle UNC names. In other words, pcszFQPath must
 *      be something like C:\folder\file.txt.
 *
 *@@added V0.9.16 (2001-10-25) [umoeller]
 */

WPFileSystem* fdrQueryAwakeFSObject(PCSZ pcszFQPath)
{
    WPFileSystem *pReturn = NULL;

    BOOL    fLocked = FALSE;
    PSZ     pszUpperFQPath;

    TRY_LOUD(excpt1)
    {
        ULONG       ulFQPathLen;
        if (    (pcszFQPath)
             && (ulFQPathLen = strlen(pcszFQPath))
             && (ulFQPathLen > 1)
             && (pszUpperFQPath = malloc(ulFQPathLen + 1))
           )
        {
            memcpy(pszUpperFQPath, pcszFQPath, ulFQPathLen + 1);
            nlsUpper(pszUpperFQPath, ulFQPathLen);

            // if the string terminates with '\\',
            // remove that (the below code cannot handle that)
            if (pszUpperFQPath[ulFQPathLen - 1] == '\\')
                pszUpperFQPath[--ulFQPathLen] = '\0';

            // check if this is a local object; we can't support
            // UNC at this point
            if (pcszFQPath[1] == ':')
            {
                // this is a fully qualified path like C:\folder\file.txt:

                // now get the root folder from the drive letter
                if (fLocked = LockRootFolders())
                {
                    PLISTNODE pNode;
                    WPFolder *pRoot = NULL;

                    // we are not in the root folder:
                    // now, since this routine probably gets
                    // called frequently for files in the same
                    // folder, we should cache the last folder
                    // found...
                    PSZ     pLastBackslash;
                    BOOL    fSkip = FALSE;

                    if (    // cache only non-root folders
                            (ulFQPathLen > 4)
                            // not first call?
                         && G_pLastQueryAwakeFolder
                         && (pLastBackslash = strrchr(pszUpperFQPath, '\\'))
                       )
                    {
                        *pLastBackslash = '\0';
                        if (!strcmp(G_szLastQueryAwakeFolderPath,
                                    pszUpperFQPath))
                        {
                            // same folder as last time:
                            // we don't need to run thru the
                            // particles then
                            // _Pmpf(("Scanning cache folder %s",
                               //     G_szLastQueryAwakeFolderPath));

                            pReturn = FindFSFromUpperName(G_pLastQueryAwakeFolder,
                                                          pLastBackslash + 1);
                            // even if this returned NULL,
                            // skip the query
                            fSkip = TRUE;
                        }

                        // restore last backslash
                        *pLastBackslash = '\\';
                    }

                    if (!fSkip)
                    {
                        // cache wasn't used:
                        for (pNode = lstQueryFirstNode(&G_llRootFolders);
                             pNode;
                             pNode = pNode->pNext)
                        {
                            CHAR szRootThis[CCHMAXPATH];
                            WPFolder *pThis;
                            if (    (pThis = (WPFolder*)pNode->pItemData)
                                 && (_wpQueryFilename(pThis,
                                                      szRootThis,
                                                      TRUE))
                               )
                            {
                                // _Pmpf(("Scanning root folder %s", szRootThis));
                                if (szRootThis[0] == pszUpperFQPath[0])
                                {
                                    pRoot = pThis;
                                    break;
                                }
                            }
                        }

                        if (pRoot)
                        {
                            // we got the root folder:
                            if (ulFQPathLen == 2)
                            {
                                // caller wants root dir only:
                                // we can stop here
                                pReturn = pRoot;
                            }
                            else if (pcszFQPath[2] == '\\')
                            {
                                // go thru the path particles and,
                                // for each particle, query the parent
                                // folder for whether the component
                                // exists (this updates the cache)
                                pReturn = ProcessParticles(pRoot,
                                                           // first particle:
                                                           pszUpperFQPath + 3);
                            }
                        }
                        // else: we didn't even have the root folder,
                        // no need to search on... the others can't
                        // be awake either
                    }
                }
            }

            free(pszUpperFQPath);
        }
    }
    CATCH(excpt1)
    {
        pReturn = NULL;
    } END_CATCH();

    if (fLocked)
        UnlockRootFolders();

    return (pReturn);
}

/* ******************************************************************
 *
 *   Folder populate with file-system objects
 *
 ********************************************************************/

/*
 *@@ fdrCreateStandardGEAList:
 *      sets up the global pointer to the GEA2LIST which
 *      describes the EA names to look for during
 *      file-system populate. Since we always use
 *      the same list, we create this on
 *      M_XFolder::wpclsInitData and reuse that list
 *      forever.
 *
 *@@added V0.9.16 (2001-10-25) [umoeller]
 */

VOID fdrCreateStandardGEAList(VOID)
{
    /* Entries in the GEA2 list
    must be aligned on a doubleword boundary. Each oNextEntryOffset
    field must contain the number of bytes from the beginning of the
    current entry to the beginning of the next entry. */

    /* typedef struct _GEA2LIST {
        ULONG     cbList;   // Total bytes of structure including full list.
        GEA2      list[1];  // Variable-length GEA2 structures.
    } GEA2LIST;

    /* typedef struct _GEA2 {
        ULONG     oNextEntryOffset;  // Offset to next entry.
        BYTE      cbName;            // Name length not including NULL.
        CHAR      szName[1];         // Attribute name.
      } GEA2;
    */

    if (!G_StandardGEA2List)
    {
        // first call:

        PCSZ apcszEANames[] =
            {
                ".CLASSINFO",
                ".LONGNAME",
                ".TYPE",
                ".ICON"
            };

        // check how much memory we need:
        ULONG   cbList = sizeof(ULONG),       // GEA2LIST.cbList
                ul;

        for (ul = 0;
             ul < ARRAYITEMCOUNT(apcszEANames);
             ul++)
        {
            cbList +=   sizeof(ULONG)         // GEA2.oNextEntryOffset
                      + sizeof(BYTE)          // GEA2.cbName
                      + strlen(apcszEANames[ul])
                      + 1;                    // null terminator

            // add padding, each entry must be dword-aligned
            cbList += 4;
        }

        if (G_StandardGEA2List = (PGEA2LIST)malloc(cbList))
        {
            PGEA2 pThis, pLast;

            G_StandardGEA2List->cbList = cbList;
            pThis = G_StandardGEA2List->list;

            for (ul = 0;
                 ul < ARRAYITEMCOUNT(apcszEANames);
                 ul++)
            {
                pThis->cbName = strlen(apcszEANames[ul]);
                memcpy(pThis->szName,
                       apcszEANames[ul],
                       pThis->cbName + 1);

                pThis->oNextEntryOffset =   sizeof(ULONG)
                                          + sizeof(BYTE)
                                          + pThis->cbName
                                          + 1;

                pThis->oNextEntryOffset += 3 - ((pThis->oNextEntryOffset + 3) & 0x03);
                            // 1:   1 + 3 = 4  = 0      should be 3
                            // 2:   2 + 3 = 5  = 1      should be 2
                            // 3:   3 + 3 = 6  = 2      should be 1
                            // 4:   4 + 3 = 7  = 3      should be 0

                pLast = pThis;
                pThis = (PGEA2)(((PBYTE)pThis) + pThis->oNextEntryOffset);
            }

            pLast->oNextEntryOffset = 0;
        }
    }
}

#ifdef __DEBUG__

/*
 *@@ fdrDebugDumpFolderFlags:
 *
 *@@added V0.9.16 (2001-10-28) [umoeller]
 */

VOID fdrDebugDumpFolderFlags(WPFolder *somSelf)
{
    ULONG fl = _wpQueryFldrFlags(somSelf);
    _Pmpf((__FUNCTION__ ": flags for folder %s", _wpQueryTitle(somSelf)));
    if (fl & FOI_POPULATEINPROGRESS)
        _Pmpf(("    FOI_POPULATEINPROGRESS"));
    if (fl & FOI_REFRESHINPROGRESS)
        _Pmpf(("    FOI_REFRESHINPROGRESS"));
    if (fl & FOI_ASYNCREFRESHONOPEN)
        _Pmpf(("    FOI_ASYNCREFRESHONOPEN"));
    if (fl & FOI_POPULATEDWITHFOLDERS)
        _Pmpf(("    FOI_POPULATEDWITHFOLDERS"));
    if (fl & FOI_POPULATEDWITHALL)
        _Pmpf(("    FOI_POPULATEDWITHALL"));
}

#endif

// buffer size for DosFindFirst
#define FINDBUFSIZE 0x10000     // 64K

/*
 *@@ CreateFindFirstBuffer:
 *      called from PopulateWithFileSystems to
 *      allocate and set up a buffer for DosFindFirst/Next.
 *
 *@@added V0.9.16 (2001-10-28) [umoeller]
 */

APIRET CreateFindFirstBuffer(PEAOP2 *pp)
{
    APIRET arc;

    if (!(arc = DosAllocMem((PVOID*)pp,
                            FINDBUFSIZE,
                            OBJ_TILE | PAG_COMMIT | PAG_READ | PAG_WRITE)))
    {
        // set up the EAs list... sigh

        /*  CPREF: On input, pfindbuf contains an EAOP2 data structure. */
        PEAOP2      peaop2 = *pp;
                    /*  typedef struct _EAOP2 {
                          PGEA2LIST     fpGEA2List;
                          PFEA2LIST     fpFEA2List;
                          ULONG         oError;
                        } EAOP2; */

        /*  CPREF: fpGEA2List contains a pointer to a GEA2 list, which
            defines the EA names whose values are to be returned. */

        // since we try the same EAs for the objects, we
        // create a GEA2LIST only once and reuse that forever:
        peaop2->fpGEA2List = G_StandardGEA2List;

        // set up FEA2LIST output buffer: right after the leading EAOP2
        peaop2->fpFEA2List          = (PFEA2LIST)(peaop2 + 1);
        peaop2->fpFEA2List->cbList  = FINDBUFSIZE - sizeof(EAOP2);
        peaop2->oError              = 0;
    }

    return (arc);
}

/*
 *@@ FindEAValue:
 *      returns the pointer to the EA value
 *      if the EA with the given name exists
 *      in the given FEA2LIST.
 *
 *      Within the FEA structure
 *
 +          typedef struct _FEA2 {
 +              ULONG      oNextEntryOffset;  // Offset to next entry.
 +              BYTE       fEA;               // Extended attributes flag.
 +              BYTE       cbName;            // Length of szName, not including NULL.
 +              USHORT     cbValue;           // Value length.
 +              CHAR       szName[1];         // Extended attribute name.
 +          } FEA2;
 *
 *      the EA value starts right after szName (plus its null
 *      terminator). The first USHORT of the value should
 *      normally signify the type of the EA, e.g. EAT_ASCII.
 *      This returns a pointer to that type USHORT.
 *
 *@@added V0.9.16 (2001-10-25) [umoeller]
 */

PBYTE FindEAValue(PFEA2LIST pFEA2List2,      // in: file EA list
                  PCSZ pcszEAName,           // in: EA name to search for (e.g. ".LONGNAME")
                  PUSHORT pcbValue)          // out: length of value (ptr can be NULL)
{
    ULONG ulEANameLen;

    /*
    typedef struct _FEA2LIST {
        ULONG     cbList;   // Total bytes of structure including full list.
                            // Apparently, if EAs aren't supported, this
                            // is == sizeof(ULONG).
        FEA2      list[1];  // Variable-length FEA2 structures.
    } FEA2LIST;

    typedef struct _FEA2 {
        ULONG      oNextEntryOffset;  // Offset to next entry.
        BYTE       fEA;               // Extended attributes flag.
        BYTE       cbName;            // Length of szName, not including NULL.
        USHORT     cbValue;           // Value length.
        CHAR       szName[1];         // Extended attribute name.
    } FEA2;
    */

    if (    (pFEA2List2->cbList > sizeof(ULONG))
                    // FAT32 and CDFS return 4 for anything here, so
                    // we better not mess with anything else; I assume
                    // any FS which doesn't support EAs will do so then
         && (pcszEAName)
         && (ulEANameLen = strlen(pcszEAName))
       )
    {
        PFEA2 pThis = &pFEA2List2->list[0];
        // maintain a current offset so we will never
        // go beyond the end of the buffer accidentally...
        // who knows what these stupid EA routines return!
        ULONG ulOfsThis = sizeof(ULONG),
              ul = 0;

        do
        {
            if (    (ulEANameLen == pThis->cbName)
                 && (!memcmp(pThis->szName,
                             pcszEAName,
                             ulEANameLen))
               )
            {
                if (pThis->cbValue)
                {
                    PBYTE pbValue =   (PBYTE)pThis
                                    + sizeof(FEA2)
                                    + pThis->cbName;
                    if (pcbValue)
                        *pcbValue = pThis->cbValue;
                    return (pbValue);
                }
                else
                    // no value:
                    return NULL;
            }

            if (!pThis->oNextEntryOffset)
                return (NULL);

            ulOfsThis += pThis->oNextEntryOffset;
            if (ulOfsThis >= pFEA2List2->cbList)
                return (NULL);

            pThis = (PFEA2)(((PBYTE)pThis) + pThis->oNextEntryOffset);
            ul++;

        } while (TRUE);
    }

    return (NULL);
}

/*
 *@@ DecodeLongname:
 *
 *@@added V0.9.16 (2001-10-25) [umoeller]
 */

BOOL DecodeLongname(PFEA2LIST pFEA2List2,
                    PSZ pszLongname)          // out: .LONGNAME if TRUE is returned
{
    PBYTE pbValue;

    if (pbValue = FindEAValue(pFEA2List2,
                              ".LONGNAME",
                              NULL))
    {
        PUSHORT pusType = (PUSHORT)pbValue;
        if (*pusType == EAT_ASCII)
        {
            // CPREF: first word after EAT_ASCII specifies length
            PUSHORT pusStringLength = pusType + 1;      // pbValue + 2
            if (*pusStringLength)
            {
                ULONG cb = _min(*pusStringLength, CCHMAXPATH - 1);
                memcpy(pszLongname,
                       pbValue + 4,
                       cb);
                pszLongname[cb] = '\0';
                return (TRUE);
            }
        }
    }

    return (FALSE);
}

/*
 *@@ CLASSINFO:
 *
 *@@added V0.9.16 (2001-10-28) [umoeller]
 */

typedef struct _FSCLASSINFO
{
   ULONG    whatever;
   ULONG    cbObjData;          // if != 0, length of OBJDATA after szClassName
   CHAR     szClassName[1];
} FSCLASSINFO, *PFSCLASSINFO;

/*
 *@@ DecodeClassInfo:
 *      decodes the .CLASSINFO EA, if present.
 *      If so, it returns the name of the class
 *      to be used for the folder or file.
 *      Otherwise NULL is returned.
 *
 *      Note that this also sets the given POBJDATA
 *      pointer to the OBJDATA structure found in
 *      the classinfo. This ptr will always be set,
 *      either to NULL or the OBJDATA structure.
 *
 *      If this returns NULL, the caller is responsible
 *      for finding out the real folder or data file
 *      class to be used.
 *
 *@@added V0.9.16 (2001-10-28) [umoeller]
 */

PCSZ DecodeClassInfo(PFEA2LIST pFEA2List2,
                     PULONG pulClassNameLen,    // out: strlen of the return value
                     POBJDATA *ppObjData)       // out: OBJDATA for _wpclsMakeAwake
{
    PCSZ        pcszClassName = NULL;
    ULONG       ulClassNameLen = 0;

    PBYTE pbValue;

    *ppObjData = NULL;

    if (pbValue = FindEAValue(pFEA2List2,
                              ".CLASSINFO",
                              NULL))
    {
        PUSHORT pusType = (PUSHORT)pbValue;
        if (*pusType == EAT_BINARY)
        {
            // CPREF: first word after EAT_BINARY specifies length
            PUSHORT pusDataLength = pusType + 1;      // pbValue + 2

            PFSCLASSINFO pInfo = (PFSCLASSINFO)(pusDataLength + 1); // pbValue + 4

            if (ulClassNameLen = strlen(pInfo->szClassName))
            {
                if (pInfo->cbObjData)
                {
                    // we have OBJDATA after szClassName:
                    *ppObjData = (POBJDATA)(   pInfo->szClassName
                                             + ulClassNameLen
                                             + 1);              // null terminator
                }

                pcszClassName = pInfo->szClassName;
            }
        }
    }

    *pulClassNameLen = ulClassNameLen;

    return (pcszClassName);
}

/*
 *@@ FindBestDataFileClass:
 *      gets called for all instances of WPDataFile
 *      to find the real data file class to be used
 *      for instantiating the object.
 *
 *      Returns either the name of WPDataFile subclass,
 *      if wpclsQueryInstanceType/Filter of a class
 *      requested ownership of this object, or NULL
 *      for the default "WPDataFile".
 *
 *@@added V0.9.16 (2001-10-28) [umoeller]
 */

PCSZ FindBestDataFileClass(PFEA2LIST pFEA2List2,
                           PCSZ pcszObjectTitle)
{
    PCSZ pcszClassName = NULL;

    PBYTE pbValue;

    if (pbValue = FindEAValue(pFEA2List2,
                              ".TYPE",
                              NULL))
    {
        // file has .TYPE EA:
        PUSHORT pusType = (PUSHORT)pbValue;
        if (*pusType == EAT_MVMT)
        {
            // layout of EAT_MVMT:
            // 0    WORD     EAT_MVMT        (pusType)
            // 2    WORD     usCodepage      (pusType + 1, pbValue + 2)
            //               if 0, system default codepage
            // 4    WORD     cEntries        (pusType + 2, pbValue + 4)
            // 6    type 0   WORD    EAT_ASCII
            //               WORD    length
            //               CHAR[]  achType   (not null-terminated)
            //      type 1   WORD    EAT_ASCII
            //               WORD    length
            //               CHAR[]  achType   (not null-terminated)

            PUSHORT pusCodepage = pusType + 1;      // pbValue + 2
            PUSHORT pcEntries = pusCodepage + 1;    // pbValue + 4

            PBYTE   pbEAThis = (PBYTE)(pcEntries + 1);  // pbValue + 6

            ULONG ul;
            for (ul = 0;
                 ul < *pcEntries;
                 ul++)
            {
                PUSHORT pusTypeThis = (PUSHORT)pbEAThis;
                if (*pusTypeThis == EAT_ASCII)
                {
                    PUSHORT pusLengthThis = pusTypeThis + 1;
                    // next sub-EA:
                    PSZ pszType  =   pbEAThis
                                   + sizeof(USHORT)      // EAT_ASCII
                                   + sizeof(USHORT);     // usLength
                    PBYTE pbNext =   pszType
                                   + (*pusLengthThis);   // skip string

                    // null-terminate the type string
                    CHAR c = *pbNext;
                    *pbNext = '\0';
                    // pszType now has the null-terminated type string:
                    // try to find the class
                    if (pcszClassName = ftypFindClassFromInstanceType(pszType))
                        // we can stop here
                        break;

                    *pbNext = c;
                    pbEAThis = pbNext;
                }
                else
                    // non-ASCII: we cannot handle this!
                    break;
            }
        }
    }

    if (!pcszClassName)
        // instance types didn't help: then go for the
        // instance filters
        pcszClassName = ftypFindClassFromInstanceFilter(pcszObjectTitle);

    return (pcszClassName);     // can be NULL
}

/*
 *@@ FDATETIME:
 *
 *@@added V0.9.16 (2001-10-28) [umoeller]
 */

typedef struct _FDATETIME
{
    FDATE       Date;
    FTIME       Time;
} FDATETIME, *PFDATETIME;

/*
 *@@ MAKEAWAKEFS:
 *      structure used with M_WPFileSystem::wpclsMakeAwake.
 *      Note that this is undocumented and may not work
 *      with every OS/2 version, although it works here
 *      with eCS.
 *
 *      This mostly has data from the FILEFINDBUF3 that
 *      we processed, although for some strange reason
 *      the fields have a different ordering here.
 *
 *@@added V0.9.16 (2001-10-25) [umoeller]
 */

typedef struct _MAKEAWAKEFS
{
    PSZ         pszRealName;    // real name
    FDATETIME   Creation;
    FDATETIME   LastWrite;
    FDATETIME   LastAccess;
    ULONG       attrFile;
    ULONG       cbFile;         // file size
    ULONG       cbList;         // size of FEA2LIST
    PFEA2LIST   pFea2List;      // EAs
} MAKEAWAKEFS, *PMAKEAWAKEFS;

/*
 *@@ RefreshOrAwake:
 *      called by PopulateWithFileSystems for each file
 *      or directory returned by DosFindFirst/Next.
 *
 *      On input, we get the sick FILEFINDBUF3 returned
 *      from DosFindFirst/Next, which contains both
 *      the EAs for the object and its real name.
 *
 *@@added V0.9.16 (2001-10-25) [umoeller]
 */

WPFileSystem* RefreshOrAwake(WPFolder *pFolder,
                             PFILEFINDBUF3 pfb3)
{
    WPFileSystem *pAwake = NULL;

    // Alright, the caller has given us a pointer
    // into the return buffer from DosFindFirst;
    // we have declared this to be a FILEFINDBUF3
    // here, but ccording to CPREF, the struct is
    // _without_ the cchName and achName fields...

    // My lord, who came up with this garbage?!?

    // the FEA2LIST with the EAs comes after the
    // truncated FILEFINDBUF3, that is, at the
    // address where FILEFINDBUF3.cchName would
    // normally be
    PFEA2LIST pFEA2List2 = (PFEA2LIST)(   ((PBYTE)pfb3)
                                        + FIELDOFFSET(FILEFINDBUF3,
                                                      cchName));

    // next comes a UCHAR with the name length
    PUCHAR puchNameLen = (PUCHAR)(((PBYTE)pFEA2List2) + pFEA2List2->cbList);

    // finally, the (real) name of the object
    PSZ pszRealName = ((PBYTE)puchNameLen) + sizeof(UCHAR);

    static ULONG s_ulWPDataFileLen = 0;

    // _Pmpf((__FUNCTION__ ": processing %s", pszRealName));

    // now, ignore "." and ".." which we don't want to see
    // in the folder, of course
    if (    (pszRealName[0] == '.')
         && (    (*puchNameLen == 1)
              || (    (*puchNameLen == 2)
                   && (pszRealName[1] == '.')
                 )
            )
       )
        return (NULL);

    if (!s_ulWPDataFileLen)
        // on first call, cache length of "WPDataFile" string
        s_ulWPDataFileLen = strlen(G_pcszWPDataFile);

    // alright, apparently we got something:
    // check if it is already awake (using the
    // fast content tree functions)
    if (pAwake = fdrFindFSFromName(pFolder,
                                   pszRealName))
    {
        FDATE   fdateLastWrite,
                fdateLastAccess;
        FTIME   ftimeLastWrite,
                ftimeLastAccess;

        _wpLockObject(pAwake);

        // now set the refresh flags... since wpPopulate gets in turn
        // called by wpRefresh, we are responsible for setting the
        // "dirty" and "found" bits here, or the object will disappear
        // from the folder on refresh.
        // For about how this works, the Warp 4 WPSREF says:

        //    1. Loop through all of the objects in the folder and turn on the DIRTYBIT
        //       and turn off the FOUNDBIT for all of your objects.
        //    2. Loop through the database. For every entry in the database, find the
        //       corresponding object.
        //         a. If the object exists, turn on the FOUNDBIT for the object.
        //         b. If the object does not exist, create a new object with the
        //            FOUNDBIT turned on and the DIRTYBIT turned off.
        //    3. Loop through the objects in the folder again. For any object that has
        //       the FOUNDBIT turned off, delete the object (since there is no longer a
        //       corresponding entry in the database). For any object that has the
        //       DIRTYBIT turned on, update the view with the current contents of the
        //       object and turn its DIRTYBIT off.

        // Now, since the objects disappear on refresh, I assume
        // we need to set the FOUNDBIT to on; since we are refreshing
        // here already, we can set DIRTYBIT to off as well.
        fsysSetRefreshFlags(pAwake,
                            (fsysQueryRefreshFlags(pAwake)
                                & ~DIRTYBIT)
                                | FOUNDBIT);

        _wpQueryLastWrite(pAwake, &fdateLastWrite, &ftimeLastWrite);
        _wpQueryLastAccess(pAwake, &fdateLastAccess, &ftimeLastAccess);
        if (    (memcmp(&fdateLastWrite, &pfb3->fdateLastWrite, sizeof(FDATE)))
             || (memcmp(&ftimeLastWrite, &pfb3->ftimeLastWrite, sizeof(FTIME)))
             || (memcmp(&fdateLastAccess, &pfb3->fdateLastAccess, sizeof(FDATE)))
             || (memcmp(&ftimeLastAccess, &pfb3->ftimeLastAccess, sizeof(FTIME)))
           )
        {
            // object changed: go refresh it
            fsysRefreshFSInfo(pAwake, pfb3);
        }
    }
    else
    {
        // no: wake it up then... this is terribly
        // complicated as well...
        POBJDATA        pObjData = NULL;

        CHAR            szLongname[CCHMAXPATH];
        PSZ             pszTitle;

        PCSZ            pcszClassName = NULL;
        ULONG           ulClassNameLen;
        somId           somidClassName;
        SOMClass        *pClassObject;

        // for the title of the new object, use the real
        // name, unless we also find a .LONGNAME attribute,
        // so decode the EA buffer
        if (DecodeLongname(pFEA2List2, szLongname))
            // got .LONGNAME:
            pszTitle = szLongname;
        else
            // no .LONGNAME:
            pszTitle = pszRealName;

        // NOTE about the class management:
        // At this point, we operate on class _names_
        // only and do not mess with class objects yet. This
        // is because we must take class replacements into
        // account; that is, if the object says "i wanna be
        // WPDataFile", it should really be XFldDataFile
        // or whatever other class replacements are installed.
        // While the _WPDataFile macro will not always correctly
        // resolve (since apparently this code gets called
        // too early to properly initialize the static variables
        // hidden in the macro code), somFindClass _will_
        // return the proper replacement classes.

        _Pmpf((__FUNCTION__ ": checking %s", pszTitle));

        // decode the .CLASSINFO EA, which may give us a
        // class name and the OBJDATA buffer
        if (!(pcszClassName = DecodeClassInfo(pFEA2List2,
                                              &ulClassNameLen,
                                              &pObjData)))
        {
            // no .CLASSINFO:
            // if this is a directory, use _WPFolder
            if (pfb3->attrFile & FILE_DIRECTORY)
                pcszClassName = G_pcszWPFolder;
            // else for WPDataFile, keep NULL so we
            // can determine the proper class name below
        }
        else
        {
            // we found a class name:
            // if this is "WPDataFile", return NULL instead so we
            // can still check for the default data file subclasses

            _Pmpf(("  got .CLASSINFO %s", pcszClassName));

            if (    (s_ulWPDataFileLen == ulClassNameLen)
                 && (!memcmp(G_pcszWPDataFile, pcszClassName, s_ulWPDataFileLen))
               )
                pcszClassName = NULL;
        }

        if (!pcszClassName)
        {
            // still NULL: this means we have no .CLASSINFO,
            // or the .CLASSINFO specified "WPDataFile"...
            // for WPDataFile, we must run through the
            // wpclsQueryInstanceType/Filter methods to
            // find if any WPDataFile subclass wants this
            // object to be its own (for example, .EXE files
            // should be WPProgramFile instead)
            pcszClassName = FindBestDataFileClass(pFEA2List2,
                                                  // title (.LONGNAME or realname)
                                                  pszTitle);
                    // this returns either WPDataFile or the
                    // class object of a subclass

            _Pmpf(("  FindBestDataFileClass = %s", pcszClassName));
        }

        if (!pcszClassName)
            // still nothing:
            pcszClassName = G_pcszWPDataFile;

        // now go load the class
        if (somidClassName = somIdFromString((PSZ)pcszClassName))
        {
            if (!(pClassObject = _somFindClass(SOMClassMgrObject,
                                               somidClassName,
                                               0,
                                               0)))
            {
                // this class is not installed:
                // this can easily happen with multiple OS/2
                // installations accessing the same partitions...
                // to be on the safe side, use either
                // WPDataFile or WPFolder then
                if (pfb3->attrFile & FILE_DIRECTORY)
                    pcszClassName = G_pcszWPFolder;
                else
                    pcszClassName = G_pcszWPDataFile;

                SOMFree(somidClassName);
                if (somidClassName = somIdFromString((PSZ)pcszClassName))
                    pClassObject = _somFindClass(SOMClassMgrObject,
                                                 somidClassName,
                                                 0,
                                                 0);
            }
        }

        if (pClassObject)
        {
            MAKEAWAKEFS   awfs;

            // alright, now go make the thing AWAKE
            awfs.pszRealName        = pszRealName;
            memcpy(&awfs.Creation, &pfb3->fdateCreation, sizeof(FDATETIME));
            memcpy(&awfs.LastAccess, &pfb3->fdateLastAccess, sizeof(FDATETIME));
            memcpy(&awfs.LastWrite, &pfb3->fdateLastWrite, sizeof(FDATETIME));
            awfs.attrFile           = pfb3->attrFile;
            awfs.cbFile             = pfb3->cbFile;
            awfs.cbList             = pFEA2List2->cbList;
            awfs.pFea2List          = pFEA2List2;

            pAwake = _wpclsMakeAwake(pClassObject,
                                     pszTitle,
                                     0,                 // style
                                     NULLHANDLE,        // icon
                                     pObjData,          // null if no .CLASSINFO found
                                     pFolder,           // folder
                                     (ULONG)&awfs);
        }

        if (somidClassName)
            SOMFree(somidClassName);
    }

    return (pAwake);
}

/*
 *@@ SYNCHPOPULATETHREADS:
 *      structure for communication between
 *      PopulateWithFileSystems and fntFindFiles.
 *
 *@@added V0.9.16 (2001-10-28) [umoeller]
 */

typedef struct _SYNCHPOPULATETHREADS
{
    // input parameters copied from PopulateWithFileSystems
    // so fntFindFiles knows what to look for
    PCSZ            pcszFolderFullPath;     // wpQueryFilename(somSelf, TRUE)
    PCSZ            pcszFileMask;           // NULL or file mask to look for
    BOOL            fFoldersOnly;           // TRUE if folders only

    // two 64K buffers allocated by PopulateWithFileSystems
    // for use with DosFindFirst/Next;
    // after DosFindFirst has found something in fntFindFiles,
    // PopulateWithFileSystems() can process that buffer,
    // while fntFindFiles can already run DosFindNext on the
    // second buffer.
    PEAOP2          pBuf1,
                    pBuf2;

    // current buffer to work on for PopulateWithFileSystems;
    // set by fntFindFiles after each DosFindFirst/Next.
    // This points into either pBuf1 or pBuf2 (after the
    // EAOP2 structure).
    // This must only be read or set by the owner of hmtxBuffer.
    // As a special rule, if fntFindFiles sets this to NULL,
    // it is done with DosFindFirst/Next.
    PFILEFINDBUF3   pfb3;

    // synchronization semaphores:
    // 1) current owner of the buffer
    //    RULE: only the owner of this mutex may post or
    //    reset any of the event semaphores
    HMTX            hmtxBuffer;
    // 2) "buffer taken" event sem; posted by PopulateWithFileSystems
    //    after it has copied the pfb3 pointer (before it starts
    //    processing the buffer); fntFindFiles blocks on this before
    //    switching the buffer again
    HEV             hevBufTaken;
    // 3) "buffer changed" event sem; posted by fntFindFiles
    //    after it has switched the buffer so that
    //    PopulateWithFileSystems knows new data is available
    //    (or DosFindFirst/Next is done);
    //    PopulateWithFileSystems blocks on this before processing
    //    the buffer
    HEV             hevBufPtrChanged;

    // return code from fntFindFiles, valid only after exit
    APIRET          arcReturn;

} SYNCHPOPULATETHREADS, *PSYNCHPOPULATETHREADS;

/*
 *@@ fntFindFiles:
 *      find-files thread started by PopulateWithFileSystems.
 *      This actually does the DosFindFirst/Next loop and
 *      fills a result buffer for PopulateWithFileSystems
 *      to create objects from.
 *
 *      This allows us to get better CPU utilization since
 *      DosFindFirst/Next produce a lot of idle time (waiting
 *      for a disk transaction to complete), especially with
 *      JFS. We can use this idle time to do all the
 *      CPU-intensive object creation instead of doing
 *      "find file" and "create object" synchronously.
 *
 *      Note that this requires a lot of evil synchronization
 *      between the two threads. A SYNCHPOPULATETHREADS structure
 *      is created on   PopulateWithFileSystems's stack to organize
 *      this. See remarks there for details.
 *
 *      This thread does _not_ have a message queue.
 *
 *@@added V0.9.16 (2001-10-28) [umoeller]
 */

void _Optlink fntFindFiles(PTHREADINFO ptiMyself)
{
    PSYNCHPOPULATETHREADS pspt = (PSYNCHPOPULATETHREADS)ptiMyself->ulData;
    HDIR            hdirFindHandle = HDIR_CREATE;

    BOOL            fSemBuffer = FALSE;
    APIRET          arc;

    TRY_LOUD(excpt1)
    {
        if (    (!(arc = DosCreateEventSem(NULL,    // unnamed
                                           &pspt->hevBufTaken,
                                           0,       // unshared
                                           FALSE))) // not posted
             && (!(arc = DosCreateEventSem(NULL,    // unnamed
                                           &pspt->hevBufPtrChanged,
                                           0,       // unshared
                                           FALSE))) // not posted
             && (!(arc = DosCreateMutexSem(NULL,
                                           &pspt->hmtxBuffer,
                                           0,
                                           TRUE)))      // request! this blocks out the
                                                        // second thread
             && (fSemBuffer = TRUE)
           )
        {
            CHAR            szFullMask[2*CCHMAXPATH];
            ULONG           attrFind;
            LONG            cb;

            ULONG           ulFindCount;

            PBYTE           pbCurrentBuffer;

            PCSZ            pcszFileMask = pspt->pcszFileMask;

            // crank up the priority of this thread so
            // that we get the CPU as soon as there's new
            // data from DosFindFirst/Next; since we are
            // blocked most of the time, this ensures
            // that the CPU is used most optimally
            DosSetPriority(PRTYS_THREAD,
                           PRTYC_TIMECRITICAL,
                           2,
                           0);      // current thread

            // post thread semaphore so that thrCreate returns
            DosPostEventSem(ptiMyself->hevRunning);

            if (!pcszFileMask)
                pcszFileMask = "*";

            sprintf(szFullMask,
                    "%s\\%s",
                    pspt->pcszFolderFullPath,
                    pcszFileMask);

            if (pspt->fFoldersOnly)
                attrFind =   MUST_HAVE_DIRECTORY
                           | FILE_ARCHIVED | FILE_HIDDEN | FILE_SYSTEM | FILE_READONLY;
            else
                attrFind =   FILE_DIRECTORY
                           | FILE_ARCHIVED | FILE_HIDDEN | FILE_SYSTEM | FILE_READONLY;

            // on the first call, use buffer 1
            pbCurrentBuffer = (PBYTE)pspt->pBuf1;

            ulFindCount = 1;
            arc = DosFindFirst(szFullMask,
                               &hdirFindHandle,
                               attrFind,
                               pbCurrentBuffer,     // buffer
                               FINDBUFSIZE,
                               &ulFindCount,
                               FIL_QUERYEASFROMLIST);

            // start looping...
            while (arc == NO_ERROR)
            {
                // go process this file or directory
                ULONG ulPosted;

                // On output from DosFindFirst/Next, the buffer
                // has the EAOP2 struct first, which we no
                // longer care about... after that comes
                // a truncated FILEFINDBUF3 with all the
                // data we need, so give this to the populate
                // thread, which calls RefreshOrAwake.

                // Note that at this point, we _always_ own
                // hmtxBuffer; on the first loop because it
                // was initially requested, and later on
                // because of the explicit request below.

                // 1) set buffer pointer for populate thread
                pspt->pfb3 = (PFILEFINDBUF3)(pbCurrentBuffer + sizeof(EAOP2));
                // 2) unset "buffer taken" event sem
                DosResetEventSem(pspt->hevBufTaken, &ulPosted);
                // 3) tell second thread we're going for DosFindNext
                // now, which will block on this
                // _Pmpf((__FUNCTION__ ": posting hevBufPtrChanged"));
                DosPostEventSem(pspt->hevBufPtrChanged);

                if (!ptiMyself->fExit)
                {
                    // 4) release buffer mutex; the second thread
                    //    is blocked on this and will then run off
                    // _Pmpf((__FUNCTION__ ": releasing hmtxBuffer"));
                    DosReleaseMutexSem(pspt->hmtxBuffer);
                    fSemBuffer = FALSE;

                    // _Pmpf((__FUNCTION__ ": blocking on hevBufTaken"));
                    if (    (!ptiMyself->fExit)
                         && (!(arc = DosWaitEventSem(pspt->hevBufTaken,
                                                     SEM_INDEFINITE_WAIT)))
                            // check again, second thread might be exiting now
                         && (!ptiMyself->fExit)
                       )
                    {
                        // alright, we got something else:
                        // request the buffer mutex again
                        // and re-loop; above, we will block
                        // again until the second thread has
                        // taken the new buffer
                        // _Pmpf((__FUNCTION__ ": blocking on hmtxBuffer"));
                        if (    (!(arc = DosRequestMutexSem(pspt->hmtxBuffer,
                                                            SEM_INDEFINITE_WAIT)))
                           )
                        {
                            fSemBuffer = TRUE;
                            // switch the buffer so we can load next file
                            // while second thread is working on the
                            // previous one
                            if (pbCurrentBuffer == (PBYTE)pspt->pBuf1)
                                pbCurrentBuffer = (PBYTE)pspt->pBuf2;
                            else
                                pbCurrentBuffer = (PBYTE)pspt->pBuf1;

                            // find next:
                            // _Pmpf((__FUNCTION__ ": DosFindNext"));
                            ulFindCount = 1;
                            arc = DosFindNext(hdirFindHandle,
                                              pbCurrentBuffer,
                                              FINDBUFSIZE,
                                              &ulFindCount);
                        }
                    } // end if !DosWaitEventSem(pspt->hevBufTaken)

                } // if (!ptiMyself->fExit)

                if (ptiMyself->fExit)
                    // we must exit for some reason:
                    break;

            } // while (arc == NO_ERROR)

            if (arc == ERROR_NO_MORE_FILES)
            {
                // nothing found is not an error
                arc = NO_ERROR;
            }
        }
    }
    CATCH(excpt1)
    {
        arc = ERROR_PROTECTION_VIOLATION;
    } END_CATCH();

    // post thread semaphore so that thrCreate returns,
    // in case we haven't even gotten to the above call
    DosPostEventSem(ptiMyself->hevRunning);

    // cleanup:

    DosFindClose(hdirFindHandle);

    if (!fSemBuffer)
        if (!DosRequestMutexSem(pspt->hmtxBuffer,
                                SEM_INDEFINITE_WAIT))
            fSemBuffer = TRUE;

    // tell populate thread we're done
    if (fSemBuffer)
    {
        // buffer == NULL means no more data
        pspt->pfb3 = NULL;

        // post "buf changed" because populate
        // blocks on this
        DosPostEventSem(pspt->hevBufPtrChanged);
        DosReleaseMutexSem(pspt->hmtxBuffer);
    }

    // return what we have
    pspt->arcReturn = arc;

    // _Pmpf((__FUNCTION__ ": exiting"));
}

/*
 *@@ PopulateWithFileSystems:
 *      called from fdrPopulate to get the file-system
 *      objects.
 *
 *      This starts off a second thread which does
 *      the DosFindFirst/Next loop. See fntFindFiles.
 *
 *@@added V0.9.16 (2001-10-25) [umoeller]
 */

BOOL PopulateWithFileSystems(WPFolder *somSelf,
                             PCSZ pcszFolderFullPath,  // in: wpQueryFilename(somSelf, TRUE)
                             BOOL fFoldersOnly,
                             PCSZ pcszFileMask,     // in: file mask or NULL for "*" (ignored if fFoldersOnly)
                             PBOOL pfExit)          // in: exit flag
{
    APIRET      arc;

    THREADINFO  tiFindFiles;
    volatile TID tidFindFiles = 0;
    SYNCHPOPULATETHREADS spt;

    BOOL        fBufSem = FALSE;

    memset(&spt, 0, sizeof(spt));
    spt.pcszFileMask = pcszFileMask;
    spt.pcszFolderFullPath = pcszFolderFullPath;
    spt.fFoldersOnly = fFoldersOnly;

            // allocate two 64K buffers
    if (    (!(arc = CreateFindFirstBuffer(&spt.pBuf1)))
         && (!(arc = CreateFindFirstBuffer(&spt.pBuf2)))
            // create the find-files thread
         && (thrCreate(&tiFindFiles,
                       fntFindFiles,
                       &tidFindFiles,
                       "FindFiles",
                       THRF_WAIT_EXPLICIT,      // no PM msg queue!
                       (ULONG)&spt))
       )
    {
        TRY_LOUD(excpt1)
        {
            while (!arc)
            {
                // go block until find-files has set the buf ptr
                // _Pmpf((__FUNCTION__ ": blocking on hevBufPtrChanged"));
                if (!(arc = WinWaitEventSem(spt.hevBufPtrChanged,
                                            SEM_INDEFINITE_WAIT)))
                {
                    // _Pmpf((__FUNCTION__ ": blocking on hmtxBuffer"));
                    if (!(arc = WinRequestMutexSem(spt.hmtxBuffer,
                                                   SEM_INDEFINITE_WAIT)))
                    {
                        // OK, find-files released that sem:
                        // we either have data now or we're done
                        PFILEFINDBUF3   pfb3;
                        ULONG           ulPosted;

                        fBufSem = TRUE;

                        DosResetEventSem(spt.hevBufPtrChanged, &ulPosted);

                        pfb3 = spt.pfb3;

                        // tell find-files we've taken that buffer
                        // _Pmpf((__FUNCTION__ ": got 0x%lX, posting hevBufTaken",
                           //      pfb3));
                        DosPostEventSem(spt.hevBufTaken);
                        // release buffer mutex, on which
                        // find-files may have blocked
                        DosReleaseMutexSem(spt.hmtxBuffer);
                        fBufSem = FALSE;

                        if (pfb3)
                        {
                            // we have more data:
                            /* if (tidFindFiles)
                                // before processing, give away timeslice
                                // so find-files can go for DosFindNext, which
                                // will probably block on file-system activity
                                DosSleep(0); */

                            // process this item
                            RefreshOrAwake(somSelf,
                                           pfb3);
                            // _Pmpf((__FUNCTION__ ": done with RefreshOrAwake"));
                        }
                        else
                            // no more data, exit now!
                            break;
                    }
                }

                if (*pfExit)
                    arc = -1;

            } // end while (!arc)
        }
        CATCH(excpt1)
        {
            arc = ERROR_PROTECTION_VIOLATION;
        } END_CATCH();

        // tell find-files to exit too
        tiFindFiles.fExit = TRUE;

        // in case find-files is still blocked on this
        DosPostEventSem(spt.hevBufTaken);

        if (fBufSem)
            DosReleaseMutexSem(spt.hmtxBuffer);

        // wait for thread to terminate
        // before freeing the buffers!!
        while (tidFindFiles)
        {
            // _Pmpf((__FUNCTION__ ": tidFindFiles %lX is %d",
               //  &tidFindFiles,
                // tidFindFiles));
            DosSleep(0);
        }
    }

    if (spt.pBuf1)
        DosFreeMem(spt.pBuf1);
    if (spt.pBuf2)
        DosFreeMem(spt.pBuf2);
    if (spt.hevBufTaken)
        DosCloseEventSem(spt.hevBufTaken);
    if (spt.hevBufPtrChanged)
        DosCloseEventSem(spt.hevBufPtrChanged);
    if (spt.hmtxBuffer)
        DosCloseMutexSem(spt.hmtxBuffer);

    if (!arc)
        arc = spt.arcReturn;

    // return TRUE if no error
    return (!arc);
}

/* ******************************************************************
 *
 *   Folder populate with abstract objects
 *
 ********************************************************************/

/*
 *@@ PopulateWithAbstracts:
 *      called from fdrPopulate to get the abstract
 *      objects.
 *
 *      This still needs to be rewritten. For now we
 *      use the slow wpclsFind* methods.
 *
 *@@added V0.9.16 (2001-10-28) [umoeller]
 */

BOOL PopulateWithAbstracts(WPFolder *somSelf,
                           HWND hwndReserved,
                           PMINIRECORDCORE pMyRecord,
                           PBOOL pfExit)          // in: exit flag
{
    BOOL        fSuccess = FALSE;
    HFIND       hFind = 0;

    TRY_LOUD(excpt1)
    {
        CLASS       Classes2Find[2];
        WPObject    *aObjectsFound[500];
        ULONG       cObjects;
        BOOL        brcFind;
        ULONG       ulrc;

        Classes2Find[0] = _WPAbstract;
        Classes2Find[1] = NULL;

        _wpclsSetError(_WPObject,0);
        cObjects = ARRAYITEMCOUNT(aObjectsFound);
        brcFind = _wpclsFindObjectFirst(_WPObject,
                                        Classes2Find,    // _WPAbstract
                                        &hFind,
                                        NULL,            // all titles
                                        somSelf,         // folder
                                        FALSE,           // no subfolders
                                        NULL,            // no extended criteria
                                        aObjectsFound,   // out: objects
                                        &cObjects);      // in: size of buffer, out: objs found

        do
        {
            ulrc = _wpclsQueryError(_WPObject);

            if (ulrc == WPERR_OBJECT_NOT_FOUND)
            {
                // nothing found is not an error
                fSuccess = TRUE;
                break;
            }

            if (    (brcFind)      // no error: we're done
                 || (ulrc == WPERR_BUFFER_OVERFLOW) // more objects to go
               )
            {
                // process objects here
                if (hwndReserved)
                    WinPostMsg(hwndReserved,
                               0x0405,
                               (MPARAM)-1,
                               (MPARAM)pMyRecord);

                if (brcFind)
                {
                    // we're done:
                    fSuccess = TRUE;
                    break;
                }
                else if (ulrc == WPERR_BUFFER_OVERFLOW)
                {
                    // go for next chunk
                    cObjects = ARRAYITEMCOUNT(aObjectsFound);
                    brcFind = _wpclsFindObjectNext(_WPObject,
                                                   hFind,
                                                   aObjectsFound,
                                                   &cObjects);
                }
            }
            else
            {
                // bad error:
                // get out of here
                break;
            }

        } while (!*pfExit);
    }
    CATCH(excpt1)
    {
        fSuccess = FALSE;
    } END_CATCH();

    // clean up
    if (hFind)
        _wpclsFindObjectEnd(_WPObject, hFind);

    return (fSuccess);
}

/* ******************************************************************
 *
 *   Folder populate (main)
 *
 ********************************************************************/

/*
 *@@ fdrPopulate:
 *      implementation for WPFolder::wpPopulate if
 *      "turbo folders" are enabled.
 *
 *      This presently does _not_ get called
 *
 *      --  for desktops (don't want to mess with this
 *          yet);
 *
 *      --  for remote folders cos we can't cache UNC
 *          folders yet.
 *
 *      This is a lot faster than WPFolder::wpPopulate:
 *
 *      --  Since we can use our fast folder content
 *          trees (see fdrFindFSFromName), this
 *          is a _lot_ faster for folders with many
 *          file system objects.
 *
 *      --  For file-system objects,
 *          PopulateWithFileSystems() starts a second
 *          thread which does the actual DosFindFirst/Next
 *          processing (see fntFindFiles).
 *
 *      Two bottlenecks remain for folder populating...
 *      one is DosFindFirst/Next, which is terminally
 *      slow (and which I cannot fix), the other is the
 *      record management in the containers.
 *
 *      Benchmarks (pure populate with the
 *      QUICKOPEN=IMMEDIATE setup string, so no container
 *      management involved):
 *
 +      +--------------------+-------------+-------------+
 +      |                    | turbo on    |  turbo off  |
 +      |                    | DosSleep(0) |             |
 +      +--------------------+-------------+-------------+
 +      |   JFS folder with  |     53 s    |      160 s  |
 +      |   10.000 files     |             |             |
 +      +--------------------+-------------+-------------+
 +      |   HPFS folder with |     56 s    |             |
 +      |   10.000 files     |             |             |
 +      +--------------------+-------------+-------------+
 *
 *      In addition, this supports an exit flag which, when
 *      set to TRUE, will cancel populate. This
 *      is useful to do populate on a transient thread
 *      while still being able to cancel populate,
 *      which is simply impossible with the standard wpPopulate.
 *
 *      If you call this with the address of a
 *      THREADINFO.fExit, you can even use the
 *      thread functions to cancel (see thrClose).
 *
 *@@added V0.9.16 (2001-10-25) [umoeller]
 */

BOOL fdrPopulate(WPFolder *somSelf,
                 PCSZ pcszFolderFullPath, // in: wpQueryFilename(somSelf, TRUE)
                 HWND hwndReserved,
                 BOOL fFoldersOnly,
                 PBOOL pfExit)          // in: exit flag
{
    BOOL    fSuccess = FALSE;
    BOOL    fFindSem = FALSE;

    ULONG   flFolderNew = 0;            // new folder flags

    PMINIRECORDCORE pMyRecord = _wpQueryCoreRecord(somSelf);

    TRY_LOUD(excpt1)
    {
        _Pmpf((__FUNCTION__ ": POPULATING %s", pcszFolderFullPath));

        // there can only be one populate at a time
        if (fFindSem = !fdrRequestFindMutexSem(somSelf, SEM_INDEFINITE_WAIT))
        {
            // tell everyone that we're populating
            _wpModifyFldrFlags(somSelf,
                               FOI_POPULATEINPROGRESS,
                               FOI_POPULATEINPROGRESS);

            fdrDebugDumpFolderFlags(somSelf);

            if (hwndReserved)
                WinPostMsg(hwndReserved,
                           0x0405,
                           (MPARAM)-1,
                           (MPARAM)pMyRecord);

            // in any case, populate FS objects; this
            // will use folders only if the flag is set
            if (fSuccess = PopulateWithFileSystems(somSelf,
                                                   pcszFolderFullPath,
                                                   fFoldersOnly,
                                                   NULL,        // all objects
                                                   pfExit))
            {
                // set folder flags to be set on exit:
                // we got at least "folders only" at this point
                flFolderNew = FOI_POPULATEDWITHFOLDERS;

                if (!fFoldersOnly)
                {
                    // if we have something other than folders,
                    // we need to populate with abstracts too;
                    // this hasn't been rewritten yet, so use
                    // wpclsFindFirst etc.
                    if (fSuccess = PopulateWithAbstracts(somSelf,
                                                         hwndReserved,
                                                         pMyRecord,
                                                         pfExit))
                        // got this too:
                        flFolderNew |= FOI_POPULATEDWITHALL;
                }
            }
        }
    }
    CATCH(excpt1)
    {
        fSuccess = FALSE;
    } END_CATCH();

    // in any case, even if we crashed,
    // set new folder flags:
    //      clear FOI_POPULATEINPROGRESS
    //      clear FOI_ASYNCREFRESHONOPEN
    //      set FOI_POPULATEDWITHFOLDERS, if no error
    //      set FOI_POPULATEDWITHALL, if no error
    _wpModifyFldrFlags(somSelf,
                       FOI_POPULATEINPROGRESS | FOI_ASYNCREFRESHONOPEN | flFolderNew,
                       flFolderNew);

    if (hwndReserved)
        WinPostMsg(hwndReserved,
                   0x0405,
                   (MPARAM)1,
                   (MPARAM)pMyRecord);

    fdrDebugDumpFolderFlags(somSelf);

    if (fFindSem)
        fdrReleaseFindMutexSem(somSelf);

    _Pmpf((__FUNCTION__ ": returning %d", fSuccess));

    return (fSuccess);
}

/* ******************************************************************
 *
 *   Object insertion
 *
 ********************************************************************/

/*
 *@@ fdrCnrInsertObject:
 *      inserts an object into all currently open views,
 *      wherever this may be.
 *
 *      As a precondition, the object must already
 *      _reside_ in a folder. It is assumed that the
 *      object only hasn't been inserted yet.
 *
 *      This inserts the object into:
 *
 *      -- icon views;
 *
 *      -- details views;
 *
 *      -- tree views.
 *
 *@@added V0.9.7 (2001-01-13) [umoeller]
 */

BOOL fdrCnrInsertObject(WPObject *pObject)
{
    BOOL        brc = FALSE;
    WPObject    *pFolder;

    if (    (pObject)
         && (pFolder = _wpQueryFolder(pObject))
       )
    {
        WPSHLOCKSTRUCT Lock = {0};
        TRY_LOUD(excpt1)
        {
            // if pFolder is a root folder, we should really
            // insert the object below the corresponding disk object
            if (_somIsA(pFolder, _WPRootFolder))
                pFolder = _wpQueryDisk(pFolder);

            if (    (pFolder)
                 && (LOCK_OBJECT(Lock, pFolder))
               )
            {
                PVIEWITEM   pViewItem;
                for (pViewItem = _wpFindViewItem(pFolder, VIEW_ANY, NULL);
                     pViewItem;
                     pViewItem = _wpFindViewItem(pFolder, VIEW_ANY, pViewItem))
                {
                    switch (pViewItem->view)
                    {
                        case OPEN_CONTENTS:
                        // case OPEN_TREE:
                        case OPEN_DETAILS:
                        {
                            HWND hwndCnr;
                            if (hwndCnr = wpshQueryCnrFromFrame(pViewItem->handle))
                            {
                                PPOINTL pptlIcon = _wpQueryNextIconPos(pFolder);
                                if (_wpCnrInsertObject(pObject,
                                                       hwndCnr,
                                                       pptlIcon,
                                                       NULL,     // parent record
                                                       NULL))     // RECORDINSERT, next pos.
                                    brc = TRUE;
                            }
                        }
                    }
                }
            }
        }
        CATCH(excpt1) {} END_CATCH();

        if (Lock.fLocked)
            _wpReleaseObjectMutexSem(Lock.pObject);
    }

    return (brc);
}

/*
 *@@ fdrInsertAllContents:
 *      inserts the contents of pFolder into all currently
 *      open views of pFolder.
 *
 *      Preconditions:
 *
 *      --  The folder is assumed to be fully populated.
 *
 *      --  The caller must hold the folder mutex since we're
 *          going over the folder contents here.
 *
 *@@added V0.9.9 (2001-04-01) [umoeller]
 */

ULONG fdrInsertAllContents(WPFolder *pFolder)
{
     ULONG       cObjects = 0;
     WPObject    **papObjects;

     if (papObjects = fdrQueryContentArray(pFolder,
                                           QCAFL_FILTERINSERTED,
                                           &cObjects))
     {
         PVIEWITEM   pViewItem;
         for (pViewItem = _wpFindViewItem(pFolder, VIEW_ANY, NULL);
              pViewItem;
              pViewItem = _wpFindViewItem(pFolder, VIEW_ANY, pViewItem))
         {
             switch (pViewItem->view)
             {
                 case OPEN_CONTENTS:
                 case OPEN_TREE:
                 case OPEN_DETAILS:
                 {
                     HWND hwndCnr = wpshQueryCnrFromFrame(pViewItem->handle);
                     POINTL ptlIcon = {0, 0};
                     if (hwndCnr)
                         _wpclsInsertMultipleObjects(_somGetClass(pFolder),
                                                     hwndCnr,
                                                     &ptlIcon,
                                                     (PVOID*)papObjects,
                                                     NULL,   // parentrecord
                                                     cObjects);
                 }
             }
         }

         free(papObjects);
     }

    return (cObjects);
}


