
/*
 *@@sourcefile wpsh.h:
 *      header file for wpsh.c ("pseudo SOM methods").
 *      See remarks there.
 *
 *      Note: before #include'ing this header, you must
 *      have at least wpobject.h #include'd. Functions which
 *      operate on other WPS classes will only be declared
 *      if the necessary header has been #include'd already.
 *
 *@@include #define INCL_WINWINDOWMGR
 *@@include #include <os2.h>
 *@@include #include <wpdisk.h>     // only for some funcs
 *@@include #include <wpfolder.h>   // only for some funcs
 *@@include #include <wpobject.h>   // only if no other WPS header is included
 *@@include #include "wpsh.h"
 */

/*
 *      Copyright (C) 1997-2000 Ulrich M”ller.
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

#ifndef XWPS_HEADER_INCLUDED
    #define XWPS_HEADER_INCLUDED

    PVOID wpshResolveFor(SOMObject *somSelf,
                         SOMClass *pClass,
                         const char *pcszMethodName);

    PVOID wpshParentNumResolve(SOMClass *pClass,
                               somMethodTabs parentMTab,
                               const char *pcszMethodName);

    BOOL wpshCheckObject(WPObject *pObject);

    WPObject* wpshQueryObjectFromID(const PSZ pszObjectID,
                                    PULONG pulErrorCode);

    #ifdef SOM_WPDisk_h
        WPFolder* wpshQueryRootFolder(WPDisk* somSelf, BOOL bForceMap,
                                      APIRET *parc);
    #endif

    #ifdef SOM_WPFolder_h
        BOOL wpshPopulateTree(WPFolder *somSelf);

        BOOL wpshCheckIfPopulated(WPFolder *somSelf,
                                  BOOL fFoldersOnly);

        double wpshQueryDiskFreeFromFolder(WPFolder *somSelf);

        BOOL wpshResidesBelow(WPObject *pChild, WPFolder *pFolder);

        WPFileSystem*  wpshContainsFile(WPFolder *pFolder, PSZ pszRealName);

        WPObject* wpshCreateFromTemplate(WPObject *pTemplate,
                                    WPFolder* pFolder,
                                    HWND hwndFrame,
                                    USHORT usOpenSettings,
                                    BOOL fReposition,
                                    POINTL* pptlMenuMousePos);

        HWND wpshQueryFrameFromView(WPFolder *somSelf, ULONG ulView);

    #endif

    ULONG wpshQueryView(WPObject* somSelf,
                        HWND hwndFrame);

    BOOL wpshIsViewCnr(WPObject *somSelf,
                       HWND hwndCnr);

    WPObject* wpshQuerySourceObject(WPFolder *somSelf,
                                    HWND hwndCnr,
                                    BOOL fKeyboardMode,
                                    PULONG pulSelection);

    WPObject* wpshQueryNextSourceObject(HWND hwndCnr,
                                        WPObject *pObject);

    BOOL wpshCloseAllViews(WPObject *pObject);

    ULONG wpshQueryLogicalDriveNumber(WPObject *somSelf);

    BOOL wpshCopyObjectFileName(WPObject *somSelf, HWND hwndCnr, BOOL fFullPath);

    /*
     * wpshQueryCnrFromFrame:
     *      this returns the window handle of the container in
     *      a given folder frame. Since this container _always_
     *      has the ID 0x8008, we can safely define this macro.
     */

    #define wpshQueryCnrFromFrame(hwndFrame) \
                WinWindowFromID(hwndFrame, 0x8008)

    ULONG wpshQueryDraggedObject(PDRAGITEM pdrgItem,
                                 WPObject **ppObjectFound);

    MRESULT wpshQueryDraggedObjectCnr(PCNRDRAGINFO pcdi,
                                      HOBJECT *phObject);

    #ifdef __DEBUG__
        PSZ wpshIdentifyRestoreID(PSZ pszClass,
                                  ULONG ulKey);

        VOID wpshDumpTaskRec(WPObject *somSelf,
                             PSZ pszMethodName,
                             PTASKREC pTaskRec);
    #else
        #define wpshIdentifyRestoreID(psz, ul) ""
        #define wpshDumpTaskRec(somSelf, pszMethodName, pTaskRec)
    #endif

    /* ******************************************************************
     *
     *   Object locks
     *
     ********************************************************************/

    #ifdef EXCEPT_HEADER_INCLUDED

        /*
         *@@ WPSHLOCKSTRUCT:
         *      structure used with wpshLockObject. This
         *      must be on the function's stack. See
         *      wpshLockObject for usage.
         *
         *@@added V0.9.7 (2000-12-08) [umoeller]
         */

        typedef struct _WPSHLOCKSTRUCT
        {
            EXCEPTSTRUCT    ExceptStruct;       // exception struct from helpers\except.h
            BOOL            fLocked;            // TRUE if object was locked
            ULONG           ulNesting;          // for DosEnter/ExitMustComplete
        } WPSHLOCKSTRUCT, *PWPSHLOCKSTRUCT;

        BOOL wpshLockObject(PWPSHLOCKSTRUCT pLock,
                            WPObject *somSelf);

        BOOL wpshUnlockObject(PWPSHLOCKSTRUCT pLock);

    #endif

    /* ******************************************************************
     *
     *   Additional WPObject method prototypes
     *
     ********************************************************************/

    /*
     *  For each method, we declare xfTP_methodname for the
     *  the prototype and xfTD_methodname for a function pointer.
     *
     *  IMPORTANT NOTE: Make sure these things have the _System
     *  calling convention. Normally, SOM uses #pragma linkage
     *  in the headers to ensure this, but we must do this manually.
     *  The usual SOMLINK does _not_ suffice.
     */

    /*
     * xfTP_wpMakeDormant:
     *      prototype for WPObject::wpMakeDormant.
     *
     *      _wpMakeDormant is undocumented. It destroys only
     *      the SOM object which represents the persistent
     *      form of a WPS object. This gets called whenever
     *      the WPS puts an object back to sleep (e.g. because
     *      its folder was closed and the "sleepy time" has
     *      elapsed on the object). This is described in the
     *      WPSGUIDE, "WPS Processes and Threads", "Sleepy
     *      Time Thread".
     *
     *      This also gets called in turn by _wpFree (which,
     *      in addition, destroys the persistent form) after
     *      the object has been deleted.
     *
     *      WPObject::wpMakeDormant apparently does the
     *      following:
     *
     *      1)  changes the folder flags of the owning
     *          folder, if that folder was populated
     *          (apparently to force a re-populate at
     *          the next open);
     *
     *      2)  closes all open views (by calling _wpClose);
     *
     *      3)  calls _wpSaveImmediate, if (fSaveState == TRUE);
     *
     *      4)  cleans up the object's useitems list (remove
     *          the object from all containers, cleans up
     *          memory, etc.);
     *
     *      5)  calls _somUninit, which in turn calls _wpUnInitData.
     *
     *      For example, _wpMakeDormant on a data file would
     *      only destroy the SOM representation of the data
     *      file, without deleting the file itself.
     *
     *      By contrast, _wpFree on a data file would first
     *      delete the file and then call _wpMakeDormant in
     *      turn to have the SOM object destroyed as well.
     *
     *      In other words, wpMakeDormant is the reverse to
     *      wpclsMakeAwake. By contrast, wpFree is the reverse
     *      to wpclsNew.
     */

    typedef BOOL32 _System xfTP_wpMakeDormant(WPObject *somSelf,
                                              BOOL32 fSaveState);
    typedef xfTP_wpMakeDormant *xfTD_wpMakeDormant;

    /*
     * xfTP_wpModifyMenu:
     *      prototype for WPObject::wpModifyMenu.
     *
     *      See the Warp 4 Toolkit documentation for details.
     *
     *      From my testing (after overriding _all_ WPDataFile methods...),
     *      I found out that wpDisplayMenu apparently calls the following
     *      methods in this order:
     *
     *      --  wpFilterMenu (Warp-4-specific);
     *      --  wpFilterPopupMenu;
     *      --  wpModifyPopupMenu;
     *      --  wpModifyMenu (Warp-4-specific).
     */

    typedef BOOL _System xfTP_wpModifyMenu(WPObject*,
                                           HWND,
                                           HWND,
                                           ULONG,
                                           ULONG,
                                           ULONG,
                                           ULONG);
    typedef xfTP_wpModifyMenu *xfTD_wpModifyMenu;

    /* ******************************************************************
     *
     *   Folder content hacks
     *
     ********************************************************************/

    /*
     *@@ xfTP_get_pobjNext:
     *      prototype for WPObject::_get_pobjNext (note the
     *      extra underscore).
     *
     *      Each WPObject apparently has a "pobjNext" member,
     *      which points to the "next" object if that object
     *      has been inserted in a folder. This is how the
     *      WPFolder::wpQueryContent method works... it keeps
     *      calling _get_pobjNext on the previous object.
     *
     *      Apparently, pobjNext is a SOM "attribute", which
     *      is an instance variable for which SOM automatically
     *      generates "get" methods so that they can be accessed.
     *      The nice thing about this is that this also allows
     *      us to get a direct pointer to the address of the
     *      attribute (i.e. the instance variable).
     *
     *      See the SOM Programming guide for more about attributes.
     *
     *      As a consequence, calling _get_pobjNext gives us
     *      the address of the "pobjNext" instance variable,
     *      which is a WPObject pointer.
     *
     *      Note that WPFolder also defines the _get_FirstObj
     *      and _get_LastObj attributes, which define the head
     *      and the tail of the folder contents list.
     *
     *@@added V0.9.7 (2001-01-13) [umoeller]
     */

    typedef WPObject** _System xfTP_get_pobjNext(WPObject*);
    typedef xfTP_get_pobjNext *xfTD_get_pobjNext;

    /*
     *@@ xfTP_get_FirstObj:
     *      prototype for WPFolder::_get_FirstObj (note the
     *      extra underscore).
     *
     *      See xfTP_get_pobjNext for explanations.
     *
     *@@added V0.9.7 (2001-01-13) [umoeller]
     */

    typedef WPObject** _System xfTP_get_FirstObj(WPFolder*);
    typedef xfTP_get_FirstObj *xfTD_get_FirstObj;

    /*
     *@@ xfTP_get_LastObj:
     *      prototype for WPFolder::_get_LastObj (note the
     *      extra underscore).
     *
     *      See xfTP_get_pobjNext for explanations.
     *
     *@@added V0.9.7 (2001-01-13) [umoeller]
     */

    typedef WPObject** _System xfTP_get_LastObj(WPFolder*);
    typedef xfTP_get_LastObj *xfTD_get_LastObj;

    WPObject** wpshGetNextObjPointer(WPObject *somSelf);

    /* ******************************************************************
     *
     *   Additional WPFolder method prototypes
     *
     ********************************************************************/

    #ifdef SOM_WPFolder_h

    /*
     * xfTP_wpRequestFolderMutexSem:
     *      prototype for WPFolder::wpRequestFolderMutexSem.
     *
     *      In addition to the regular object mutex, each folder
     *      has associated with it a second mutex to protect the
     *      folder contents. While this semaphore is held, one
     *      can be sure that no objects are removed from or added
     *      to a folder from some other WPS thread.
     *      You should always request this semaphore before using
     *      wpQueryContent and such things.
     *
     *      This semaphore is mentioned in the Warp 4 toolkit docs
     *      for wpRequestObjectMutexSem, but never prototyped.
     *
     *      This returns 0 if the semaphore was successfully obtained.
     */

    typedef ULONG _System xfTP_wpRequestFolderMutexSem(WPFolder *somSelf,
                                                       ULONG ulTimeout);
    typedef xfTP_wpRequestFolderMutexSem *xfTD_wpRequestFolderMutexSem;

    /*
     * xfTP_wpReleaseFolderMutexSem:
     *      prototype for WPFolder::wpReleaseFolderMutexSem.
     *
     *      This is the reverse to WPFolder::wpRequestFolderMutexSem.
     */

    typedef ULONG _System xfTP_wpReleaseFolderMutexSem(WPFolder *somSelf);
    typedef xfTP_wpReleaseFolderMutexSem *xfTD_wpReleaseFolderMutexSem;

    // wrappers
    ULONG wpshRequestFolderMutexSem(WPFolder *somSelf,
                                    ULONG ulTimeout);
    ULONG wpshReleaseFolderMutexSem(WPFolder *somSelf);

    /*
     * xfTP_wpFlushNotifications:
     *      prototype for WPFolder::wpFlushNotifications.
     *
     *      See the Warp 4 Toolkit documentation for details.
     */

    typedef BOOL _System xfTP_wpFlushNotifications(WPFolder *somSelf);
    typedef xfTP_wpFlushNotifications *xfTD_wpFlushNotifications;

    // wrapper
    ULONG wpshFlushNotifications(WPFolder *somSelf);

    /*
     * xfTP_wpclsGetNotifySem:
     *      prototype for M_WPFolder::wpclsGetNotifySem.
     *
     *      This "notify mutex" is used before the background
     *      threads in the WPS attempt to update folder contents
     *      for auto-refreshing folders. By requesting this
     *      semaphore, any other WPS thread which does file
     *      operations can therefore keep these background
     *      threads from interfering.
     */

    typedef BOOL _System xfTP_wpclsGetNotifySem(M_WPFolder *somSelf,
                                                ULONG ulTimeout);
    typedef xfTP_wpclsGetNotifySem *xfTD_wpclsGetNotifySem;

    /*
     * xfTP_wpclsReleaseNotifySem:
     *      prototype for M_WPFolder::wpclsReleaseNotifySem.
     *
     *      This is the reverse to xfTP_wpclsGetNotifySem.
     */

    typedef VOID _System xfTP_wpclsReleaseNotifySem(M_WPFolder *somSelf);
    typedef xfTP_wpclsReleaseNotifySem *xfTD_wpclsReleaseNotifySem;

    // wrappers
    BOOL wpshGetNotifySem(ULONG ulTimeout);
    VOID wpshReleaseNotifySem(VOID);

    #endif // SOM_WPFolder_h

#endif
