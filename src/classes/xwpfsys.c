
/*
 *@@sourcefile xwpfsys.c:
 *      This file contains SOM code for the following XWorkplace classes:
 *
 *      --  XWPFileSystem (WPFileSystem replacement)
 *
 *      This class replaces the WPFileSystem class.
 *      This is all new with V0.9.5 but was only experimental
 *      at that point.
 *
 *      Starting with V0.9.16, installation of this class is
 *      required for the new folder content hacks to work.
 *
 *@@added V0.9.5 [umoeller]
 *
 *@@somclass XWPFileSystem xfs_
 *@@somclass M_XWPFileSystem xfsM_
 */

/*
 *      Copyright (C) 2000-2001 Ulrich M�ller.
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
 *  This file was generated by the SOM Compiler and Emitter Framework.
 *  Generated using:
 *      SOM Emitter emitctm: 2.41
 */

#ifndef SOM_Module_xwpfsys_Source
#define SOM_Module_xwpfsys_Source
#endif
#define XWPFileSystem_Class_Source
#define M_XWPFileSystem_Class_Source

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

#define INCL_DOSSEMAPHORES
#define INCL_DOSEXCEPTIONS
#define INCL_DOSPROCESS
#define INCL_DOSERRORS
#include <os2.h>

// C library headers
#include <stdio.h>              // needed for except.h
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\except.h"             // exception handling
#include "helpers\nls.h"                // National Language Support helpers
#include "helpers\stringh.h"            // string helper routines

// SOM headers which don't crash with prec. header files
#include "xwpfsys.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

#include "filesys\fhandles.h"           // replacement file-system handles management
#include "filesys\filesys.h"            // various file-system object implementation code
#include "filesys\filetype.h"           // extended file types implementation
#include "filesys\folder.h"             // XFolder implementation

// other SOM headers
#pragma hdrstop

/* ******************************************************************
 *                                                                  *
 *  Global variables                                                *
 *                                                                  *
 ********************************************************************/

static BOOL    G_fReplaceHandles = FALSE;

extern BOOL             G_fTurboSettingsEnabled;
                                // common.c;
                                // this is copied from the global settings
                                // by M_XWPFileSystem::wpclsInitData and
                                // remains FALSE if that class is not
                                // installed

/* ******************************************************************
 *                                                                  *
 *  XWPFileSystem instance methods                                  *
 *                                                                  *
 ********************************************************************/

/*
 *@@ xwpQueryHandle:
 *
 */

SOM_Scope HOBJECT  SOMLINK xfs_xwpQueryHandle(XWPFileSystem *somSelf,
                                              BOOL fCreate)
{
    // XWPFileSystemData *somThis = XWPFileSystemGetData(somSelf);
    XWPFileSystemMethodDebug("XWPFileSystem","xfs_xwpQueryHandle");

    /* Return statement to be customized: */
    return (0);
}

/*
 *@@ xwpQueryUpperRealName:
 *      returns the current real name of the object in
 *      upper case, which can then be used for fast
 *      string comparisons, e.g. in the new folder
 *      content trees (V0.9.16).
 *
 *      Precondition:
 *
 *      --  somSelf must be fully initialized.
 *
 *      --  The caller must hold the folder mutex sem of
 *          somSelf's folder while calling this.
 *
 *@@added V0.9.16 (2001-10-25) [umoeller]
 */

SOM_Scope PSZ  SOMLINK xfs_xwpQueryUpperRealName(XWPFileSystem *somSelf)
{
    XWPFileSystemData *somThis = XWPFileSystemGetData(somSelf);
    XWPFileSystemMethodDebug("XWPFileSystem","xfs_xwpQueryUpperRealName");

    if (!_pszUpperRealName)
    {
        // not queried yet:
        // create a copy
        CHAR sz[CCHMAXPATH];
        if (_wpQueryFilename(somSelf, sz, FALSE))
        {
            ULONG ulLength;
            strhStore(&_pszUpperRealName, sz, &ulLength);
            nlsUpper(_pszUpperRealName, ulLength);
        }
    }

    return (_pszUpperRealName);
}

/*
 *@@ wpInitData:
 *      this WPObject instance method gets called when the
 *      object is being initialized (on wake-up or creation).
 *      We initialize our additional instance data here.
 *      Always call the parent method first.
 */

SOM_Scope void  SOMLINK xfs_wpInitData(XWPFileSystem *somSelf)
{
    XWPFileSystemData *somThis = XWPFileSystemGetData(somSelf);
    XWPFileSystemMethodDebug("XWPFileSystem","xfs_wpInitData");

    XWPFileSystem_parent_WPFileSystem_wpInitData(somSelf);

    _ulHandle = 0;
    _pszUpperRealName = NULL;
    _ulCnrRefresh = -1;
}

/*
 *@@ UnInitData:
 *      this WPObject instance method is called when the object
 *      is destroyed as a SOM object, either because it's being
 *      made dormant or being deleted. All allocated resources
 *      should be freed here.
 *      The parent method must always be called last.
 */

SOM_Scope void  SOMLINK xfs_wpUnInitData(XWPFileSystem *somSelf)
{
    XWPFileSystemData *somThis = XWPFileSystemGetData(somSelf);
    XWPFileSystemMethodDebug("XWPFileSystem","xfs_wpUnInitData");

    strhStore(&_pszUpperRealName, NULL, NULL);

    XWPFileSystem_parent_WPFileSystem_wpUnInitData(somSelf);
}

/*
 *@@ wpObjectReady:
 *      this WPObject notification method gets called by the
 *      WPS when object instantiation is complete, for any reason.
 *      ulCode and refObject signify why and where from the
 *      object was created.
 *      The parent method must be called first.
 *
 *      See XFldObject::wpObjectReady for remarks about using
 *      this method as a copy constructor.
 */

SOM_Scope void  SOMLINK xfs_wpObjectReady(XWPFileSystem *somSelf,
                                          ULONG ulCode, WPObject* refObject)
{
    // XWPFileSystemData *somThis = XWPFileSystemGetData(somSelf);
    XWPFileSystemMethodDebug("XWPFileSystem","xfs_wpObjectReady");

    XWPFileSystem_parent_WPFileSystem_wpObjectReady(somSelf,
                                                    ulCode, refObject);
}

/*
 *@@ wpOpen:
 *      this WPObject instance method gets called when
 *      a new view needs to be opened. Normally, this
 *      gets called after wpViewObject has scanned the
 *      object's USEITEMs and has determined that a new
 *      view is needed.
 *
 *      This _normally_ runs on thread 1 of the WPS, but
 *      this is not always the case. If this gets called
 *      in response to a menu selection from the "Open"
 *      submenu or a double-click in the folder, this runs
 *      on the thread of the folder (which _normally_ is
 *      thread 1). However, if this results from WinOpenObject
 *      or an OPEN setup string, this will not be on thread 1.
 *
 *      We check for OPEN_SETTINGS here because
 *      WPFileSystem::wpOpen evilly refreshes the file-system
 *      data under our back and keeps nuking our replacement
 *      icons. As a result, we call our refresh replacement
 *      instead.
 *
 *@@added V0.9.16 (2001-12-08) [umoeller]
 */

SOM_Scope HWND  SOMLINK xfs_wpOpen(XWPFileSystem *somSelf, HWND hwndCnr,
                                   ULONG ulView, ULONG param)
{
    XWPFileSystemData *somThis = XWPFileSystemGetData(somSelf);
    XWPFileSystemMethodDebug("XWPFileSystem","xfs_wpOpen");

    if (ulView == OPEN_SETTINGS)
    {
        somTD_WPObject_wpOpen pwpOpen;

        // now call the WPObject method by skipping the WPFileSystem
        // implementation
        if (pwpOpen = (somTD_WPObject_wpOpen)wpshResolveFor(
                                     somSelf,
                                     _WPObject,     // class to resolve for
                                     "wpOpen"))
        {
            // this allows us to replace the method, dammit
            _wpRefresh(somSelf,
                       NULLHANDLE,      // view
                       NULL);           // reserved

            return pwpOpen(somSelf, hwndCnr, ulView, param);
        }
    }

    return (XWPFileSystem_parent_WPFileSystem_wpOpen(somSelf,
                                                     hwndCnr,
                                                     ulView,
                                                     param));
}

/*
 *@@ wpSetTitleAndRenameFile:
 *      this WPFileSystem method is responsible for setting
 *      the real name _and_ the title of a file-system object.
 *
 *      WPFileSystem::wpSetTitle only calls this method and
 *      does nothing else. In other words, this method is
 *      the implementation for WPFileSystem::wpSetTitle;
 *      fConfirmations is then set to what _wpQueryConfirmations
 *      returns.
 *      Apparently, is the only place in the WPS which
 *      actually renames a file-system object on disk.
 *
 *      If "turbo folders" are enabled, we must update the
 *      content tree of our folder.
 *
 *@@added V0.9.16 (2001-10-25) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xfs_wpSetTitleAndRenameFile(XWPFileSystem *somSelf,
                                                    PSZ pszNewTitle,
                                                    ULONG fConfirmations)
{
    BOOL        brc;
    BOOL        fFolderLocked = FALSE;
    WPFolder    *pMyFolder;
    CHAR        szFolder[CCHMAXPATH];

    XWPFileSystemMethodDebug("XWPFileSystem","xfs_wpSetTitleAndRenameFile");

    if (    (cmnIsFeatureEnabled(TurboFolders))
            // we only need to handle the case where the object's name
            // is being changed, so skip if we aren't even initialized yet
         && (_wpIsObjectInitialized(somSelf))
            // we can't handle UNC yet
         && (pMyFolder = _wpQueryFolder(somSelf))
         && (_wpQueryFilename(pMyFolder, szFolder, TRUE))
         && (szFolder[0] != '\\')
         && (szFolder[1] != '\\')
       )
    {
        TRY_LOUD(excpt1)
        {
            if (    (brc = XWPFileSystem_parent_WPFileSystem_wpSetTitleAndRenameFile(somSelf,
                                                                                     pszNewTitle,
                                                                                     fConfirmations))
                 && (fFolderLocked = !fdrRequestFolderWriteMutexSem(pMyFolder))
               )
            {
                // update the folder's contents tree
                // (this also changes our instance data!)
                fdrRealNameChanged(pMyFolder,
                                   somSelf);
            }
        }
        CATCH(excpt1)
        {
            brc = FALSE;
        } END_CATCH();

        if (fFolderLocked)
            fdrReleaseFolderWriteMutexSem(pMyFolder);

        return (brc);
    } // end if (cmnIsFeatureEnabled(TurboFolders))

    return (XWPFileSystem_parent_WPFileSystem_wpSetTitleAndRenameFile(somSelf,
                                                                      pszNewTitle,
                                                                      fConfirmations));
}

/*
 *@@ wpQueryIcon:
 *      this WPObject instance method returns the HPOINTER
 *      with the current icon of the object. For some WPS
 *      classes, icon loading is deferred until the first
 *      call to this method.
 *      See icons.c for an introduction.
 *
 *      WPDataFile overrides this method to defer icon loading
 *      for data files until the icon is first needed.
 *      See XFldDataFile::wpQueryIcon for more about that.
 *
 *      However, WPFolder does _not_ override this method, so
 *      for folders, the WPFileSystem implementation gets called.
 *      Unfortunately, that implementation of this method is
 *      very expensive. Even though (as with all file-system
 *      objects) the icon data from the .ICON EA is passed
 *      to WPFolder::wpRestoreState, the WPS apparently
 *      doesn't even check the buffer but checks the icon
 *      EAs _again_ in this method, which isn't exactly
 *      speedy. What we can do here safely is check if
 *      an icon was set in our XFolder::wpRestoreState override
 *      (which does parse the FEA2LIST) and if not, load
 *      a default icon here.
 *
 *      I'd love to have shared this implementation with
 *      XFldDataFile, but since WPDataFile overrides this
 *      method, we have to override that override again
 *      for XFldDataFile.
 *
 *@@added V0.9.16 (2002-01-04) [umoeller]
 */

SOM_Scope HPOINTER  SOMLINK xfs_wpQueryIcon(XWPFileSystem *somSelf)
{
    HPOINTER hptrReturn = NULLHANDLE;
    // XWPFileSystemData *somThis = XWPFileSystemGetData(somSelf);
    XWPFileSystemMethodDebug("XWPFileSystem","xfs_wpQueryIcon");

    if (cmnIsFeatureEnabled(TurboFolders))
    {
        PMINIRECORDCORE prec = _wpQueryCoreRecord(somSelf);
        if (!(hptrReturn = prec->hptrIcon))
        {
            // first call, and icon wasn't set in wpRestoreState:
            // use class default icon
            // (again, note that we override this for XFldDataFile
            // so this really only affects folders)
            if (hptrReturn = _wpclsQueryIcon(_somGetClass(somSelf)))
            {
                _wpSetIcon(somSelf, hptrReturn);
                _wpModifyStyle(somSelf,
                               OBJSTYLE_NOTDEFAULTICON,
                               0);
            }
        }
    }

    if (!hptrReturn)
        hptrReturn = XWPFileSystem_parent_WPFileSystem_wpQueryIcon(somSelf);

    return (hptrReturn);
}

/*
 *@@ wpRefresh:
 *      this WPFileSystem method compares the internal
 *      object data with the data on disk and refreshes
 *      the object, if necessary.
 *
 *      The original WPFileSystem implementation is truly
 *      evil. It keeps nuking icons behind our back, for
 *      example. As a result, this has now been rewritten
 *      with V0.9.16.
 *
 *@@added V0.9.16 (2001-12-08) [umoeller]
 */

SOM_Scope BOOL  SOMLINK xfs_wpRefresh(XWPFileSystem *somSelf,
                                      ULONG ulView, PVOID pReserved)
{
    XWPFileSystemData *somThis = XWPFileSystemGetData(somSelf);
    XWPFileSystemMethodDebug("XWPFileSystem","xfs_wpRefresh");

    if (cmnIsFeatureEnabled(TurboFolders))
    {
        return !fsysRefresh(somSelf, pReserved);
    }

    return (XWPFileSystem_parent_WPFileSystem_wpRefresh(somSelf,
                                                        ulView,
                                                        pReserved));
}

/*
 *@@ wpCnrRefreshDetails:
 *      this WPObject instance method causes all currently visible
 *      RECORDCORE structures to  be refreshed with the current
 *      object details. Unfortunately, this gets called way too
 *      often during wpRefresh, so we hack this a little bit
 *      to avoid flickering details views.
 *
 *      The way this works is that our wpInitData sets the instance
 *      variable _ulCnrRefresh to -1, meaning that we should always
 *      call the parent. If that is != -1, we simply do not call
 *      the parent, but raise the variable by 1;
 *      this way we can count pending changes during wpRefresh and
 *      then reset the variable to -1 and call this explicitly.
 *
 *@@added V0.9.16 (2001-12-08) [umoeller]
 */

SOM_Scope void  SOMLINK xfs_wpCnrRefreshDetails(XWPFileSystem *somSelf)
{
    XWPFileSystemData *somThis = XWPFileSystemGetData(somSelf);
    XWPFileSystemMethodDebug("XWPFileSystem","xfs_wpCnrRefreshDetails");

    if (_ulCnrRefresh == -1)
        XWPFileSystem_parent_WPFileSystem_wpCnrRefreshDetails(somSelf);
    else
        // just count, we're in wpRefresh
        (_ulCnrRefresh)++;

}


/* ******************************************************************
 *                                                                  *
 *  M_XWPFileSystem class methods                                   *
 *                                                                  *
 ********************************************************************/

/*
 *@@ wpclsInitData:
 *      this WPObject class method gets called when a class
 *      is loaded by the WPS (probably from within a
 *      somFindClass call) and allows the class to initialize
 *      itself.
 */

SOM_Scope void  SOMLINK xfsM_wpclsInitData(M_XWPFileSystem *somSelf)
{
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();

    /* M_XWPFileSystemData *somThis = M_XWPFileSystemGetData(somSelf); */
    M_XWPFileSystemMethodDebug("M_XWPFileSystem","xfsM_wpclsInitData");

    if (krnClassInitialized(G_pcszXWPFileSystem))
    {
        // make a copy of the "turbo folders" setting _now_, which
        // will then be returned by cmnIsFeatureEnabled for the rest
        // of this WPS session; doing this here ensures that the
        // setting is only enabled if XWPFileSystem is installed
        G_fTurboSettingsEnabled = pGlobalSettings->__fTurboFolders;
    }

#ifdef __REPLHANDLES__
    // query once at system startup whether handles
    // management has been replaced
    G_fReplaceHandles = pGlobalSettings->fReplaceHandles;
    if (G_fReplaceHandles)
        // enabled:
        // initialize handles
        fhdlLoadHandles();
#endif

    M_XWPFileSystem_parent_M_WPFileSystem_wpclsInitData(somSelf);

    // go register instance types and filters
    if (ftypRegisterInstanceTypesAndFilters(somSelf))
        // got any:
        // do not allow this class to be unloaded!!
        _wpclsIncUsage(somSelf);

    /*
     *  Manually patch method tables of this class...
     *
     */

    // this gets called for subclasses too, so patch
    // this only for the parent class... descendant
    // classes will inherit this anyway
    /* _Pmpf((__FUNCTION__ ": attempting to override wpDestroyObject"));
    if (somSelf == _XWPFileSystem)
    {
        // override the undocumented wpDestroyObject method
        _Pmpf((__FUNCTION__ ": overriding"));
        wpshOverrideStaticMethod(somSelf,
                                 "wpDestroyObject",
                                 (somMethodPtr)xfs_wpDestroyObject);
    } */
}

/*
 *@@ wpclsQueryAwakeObject:
 *      this WPFileSystem class method determines whether a
 *      a file-system object is already awake.
 *
 *      From my testing, this method is never actually
 *      called in the WPS, but my own code calls it in
 *      some places. If "turbo folders" are on, we use
 *      our own high-speed functions instead of calling
 *      the parent.
 *
 *@@added V0.9.16 (2001-10-25) [umoeller]
 */

SOM_Scope WPObject*  SOMLINK xfsM_wpclsQueryAwakeObject(M_XWPFileSystem *somSelf,
                                                        PSZ pszInputPath)
{
    /* M_XWPFileSystemData *somThis = M_XWPFileSystemGetData(somSelf); */
    M_XWPFileSystemMethodDebug("M_XWPFileSystem","xfsM_wpclsQueryAwakeObject");

    if (    (cmnIsFeatureEnabled(TurboFolders))
            // we can't handle UNC yet
         && (pszInputPath[0] != '\\')
         && (pszInputPath[1] != '\\')
       )
        return (fdrQueryAwakeFSObject(pszInputPath));

    return (M_XWPFileSystem_parent_M_WPFileSystem_wpclsQueryAwakeObject(somSelf,
                                                                        pszInputPath));
}


/*
 *@@ wpclsFileSysExists:
 *      tests "Folder" whether a file-system object with
 *      the real name pszFilename is already awake in the
 *      folder. If so, it is returned.
 *
 *      We replace this method also if "turbo folders"
 *      are on.
 *
 *@@added V0.9.16 (2002-01-04) [umoeller]
 */

SOM_Scope WPObject*  SOMLINK xfsM_wpclsFileSysExists(M_XWPFileSystem *somSelf,
                                                     WPFolder* Folder,
                                                     PSZ pszFilename,
                                                     ULONG attrFile)
{
    WPObject *pAwake;
    CHAR    szFolder[CCHMAXPATH];
    /* M_XWPFileSystemData *somThis = M_XWPFileSystemGetData(somSelf); */
    M_XWPFileSystemMethodDebug("M_XWPFileSystem","xfsM_wpclsFileSysExists");

    if (    (cmnIsFeatureEnabled(TurboFolders))
            // we can't handle UNC yet
         && (_wpQueryFilename(Folder, szFolder, TRUE))
         && (szFolder[0] != '\\')
         && (szFolder[1] != '\\')
       )
    {
        // alright, apparently we got something:
        // check if it is already awake (using the
        // fast content tree functions)
        if (pAwake = fdrFindFSFromName(Folder,
                                       pszFilename))
        {
            if ((_wpQueryAttr(pAwake) & FILE_DIRECTORY) == (attrFile & FILE_DIRECTORY))
                fsysSetRefreshFlags(pAwake,
                                    (fsysQueryRefreshFlags(pAwake)
                                        & ~DIRTYBIT)
                                        | FOUNDBIT);
            else
                pAwake = NULL;
        }
    }
    else
        pAwake = M_XWPFileSystem_parent_M_WPFileSystem_wpclsFileSysExists(somSelf,
                                                                          Folder,
                                                                          pszFilename,
                                                                          attrFile);

    return (pAwake);
}

