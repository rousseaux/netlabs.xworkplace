
/*
 *@@sourcefile wpsh.h:
 *      header file for wpsh.c ("pseudo SOM methods"). See remarks there.
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
#endif
