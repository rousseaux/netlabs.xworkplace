
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

    PVOID wpshParentResolve(SOMObject *somSelf,
                            SOMClass *pClass,
                            const char *pcszMethodName);

    PVOID wpshParentNumResolve(SOMClass *pClass,
                               somMethodTabs parentMTab,
                               const char *pcszMethodName);

    BOOL wpshCheckObject(WPObject *pObject);

    WPObject* wpshQueryObjectFromID(const PSZ pszObjectID,
                                    PULONG pulErrorCode);

    #ifdef SOM_WPDisk_h
        WPFolder* wpshQueryRootFolder(WPDisk* somSelf, APIRET *parc);
    #endif

    #ifdef SOM_WPFolder_h
        BOOL wpshPopulateTree(WPFolder *somSelf);

        BOOL wpshCheckIfPopulated(WPFolder *somSelf);

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
     *      only remove the SOM representation of the data
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
     *   Additional WPFolder method prototypes
     *
     ********************************************************************/

    #ifdef SOM_WPFolder_h

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
     * M_WPFolder:
     *      prototype for M_WPFolder::wpclsReleaseNotifySem.
     *
     *      This is the reverse to xfTP_wpclsGetNotifySem.
     */

    typedef VOID _System xfTP_wpclsReleaseNotifySem(M_WPFolder *somSelf);
    typedef xfTP_wpclsReleaseNotifySem *xfTD_wpclsReleaseNotifySem;

    #endif // SOM_WPFolder_h

#endif
