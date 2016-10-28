
/*
 *@@sourcefile fdrviews.h:
 *      header file for fdrviews.c.
 *
 *      This file is ALL new with V1.0.0
 *
 *@@include #include <os2.h>
 *@@include #include "helpers\linklist.h"
 *@@include #include <wpfolder.h>
 *@@include #include "filesys\fdrviews.h"
 */

/*
 *      Copyright (C) 2001-2002 Ulrich M�ller.
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

#ifndef FDRVIEWS_HEADER_INCLUDED
    #define FDRVIEWS_HEADER_INCLUDED

    /* ******************************************************************
     *
     *   Image file cache
     *
     ********************************************************************/

    BOOL fdrvRemoveFromImageCache(WPObject *pobjImage);

    /* ******************************************************************
     *
     *   Container view setup
     *
     ********************************************************************/

    BOOL fdrvMakeCnrPaint(HWND hwndCnr);

    VOID fdrvSetCnrLayout(HWND hwndCnr,
                          WPFolder *pFolder,
                          ULONG ulView);

    /* ******************************************************************
     *
     *   View populate management
     *
     ********************************************************************/

    WPFileSystem* fdrvGetFSFromRecord(PMINIRECORDCORE precc,
                                      BOOL fFoldersOnly);

    #define INSERT_ALL              0
    #define INSERT_FILESYSTEMS      1
    #define INSERT_FOLDERSONLY      2
    #define INSERT_FOLDERSANDDISKS  3

    BOOL fdrvIsInsertable(WPObject *pObject,
                          BOOL ulFoldersOnly,
                          PCSZ pcszFileMask);

    BOOL fdrvIsObjectInCnr(WPObject *pObject,
                           HWND hwndCnr);

    #define CLEARFL_TREEVIEW        0x0001
    #define CLEARFL_UNLOCKOBJECTS   0x0002

    ULONG fdrvClearContainer(HWND hwndCnr,
                             ULONG flClear);

#endif
