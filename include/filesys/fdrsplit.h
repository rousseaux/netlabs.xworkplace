
/*
 *@@sourcefile fdrsplit.h:
 *      header file for fdrsplit.c.
 *
 *      This file is ALL new with V0.9.21
 *
 *@@include #include <os2.h>
 *@@include #include "helpers\linklist.h"
 *@@include #include "helpers\tree.h"
 *@@include #include <wpfolder.h>
 *@@include #include "filesys\folder.h"
 *@@include #include "filesys\fdrsubclass.h"
 *@@include #include "filesys\fdrsplit.h"
 */

/*
 *      Copyright (C) 2001-2002 Ulrich M”ller.
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

#ifndef FDRSPLIT_HEADER_INCLUDED
    #define FDRSPLIT_HEADER_INCLUDED

    /* ******************************************************************
     *
     *   Folder split views (fdrsplit.c)
     *
     ********************************************************************/

    #define ID_TREEFRAME            1001
    #define ID_FILESFRAME           1002

    #define FM_FILLFOLDER           (WM_USER + 1)
        #define FFL_FOLDERSONLY             0x0001
        #define FFL_SCROLLTO                0x0002
        #define FFL_EXPAND                  0x0004
        #define FFL_SETBACKGROUND           0x0008
        #define FFL_UNLOCKOBJECTS           0x1000

    #define FM_POPULATED_FILLTREE   (WM_USER + 2)
    #define FM_POPULATED_SCROLLTO   (WM_USER + 3)
    #define FM_POPULATED_FILLFILES  (WM_USER + 4)
    #define FM_UPDATEPOINTER        (WM_USER + 5)
    #define FM_DELETINGFDR          (WM_USER + 6)

    #define FM2_POPULATE            (WM_USER + 10)
    #define FM2_ADDFIRSTCHILD_BEGIN (WM_USER + 11)
    #define FM2_ADDFIRSTCHILD_NEXT  (WM_USER + 12)
    #define FM2_ADDFIRSTCHILD_DONE  (WM_USER + 13)

    #define SPLIT_ANIMATE           0x0001
    #define SPLIT_FDRSTYLES         0x0002
    #define SPLIT_MULTIPLESEL       0x0004
    #define SPLIT_STATUSBAR         0x0008
    #define SPLIT_MENUBAR           0x0010
    #define SPLIT_NOAUTOPOSITION    0x0020
    #define SPLIT_NOAUTOPOPOPULATE  0x0040

    // WM_CONTROL notifications sent by the controller
    // to its owner; SHORT1FROMMP(mp1) is always FID_CLIENT

    #define SN_FOLDERCHANGING       0x1000
                // sent when a new folder is populated and the
                // files cnr is to be filled (that is, FFL_FOLDERSONLY
                // is _not_ set);
                // mp2 is WPFolder that is now populating. If a
                // disk is being populated, this is the root folder.
                // Return code is ignored.

    #define SN_FOLDERCHANGED        0x1001
                // after SN_FOLDERCHANGING, sent when populate is done;
                // mp2 is the same folder as with the previous
                // SN_FOLDERCHANGING.
                // Return code is ignored.

    #define SN_OBJECTSELECTED       0x1002
                // object was selected in the files cnr. This only
                // comes in if the user explicitly clicks on an
                // object, not if an object gets selected by the
                // container automatically.
                // mp2 is the record of the object, which might
                // be a shadow.

    #define SN_OBJECTENTER          0x1003
                // object was double-clicked upon in the files cnr.
                // mp2 is the record of the object, which might
                // be a shadow.
                // If this returns FALSE (or is not handled), the
                // split view performs a default action. If a
                // non-zero value is returned, the split view does
                // nothing.

    #ifdef THREADS_HEADER_INCLUDED
    #ifdef FDRSUBCLASS_HEADER_INCLUDED

        /*
         *@@ FDRSPLITVIEW:
         *
         */

        typedef struct _FDRSPLITVIEW
        {
            USHORT          cbStruct;

            LONG            lSplitBarPos;   // initial split bar position in percent

            WPObject        *pRootObject;   // root object; normally a folder, but
                                            // be a disk object to, for which we
                                            // then query the root folder
            WPFolder        *pRootsFolder;  // unless this is a disk, == pRootObject

            ULONG           flSplit;
                                // current split operation mode
                                // (SPLIT_* flags)

            // window hierarchy
            HWND            hwndMainFrame,
                            hwndMainControl;    // child of hwndMainFrame

            HWND            hwndSplitWindow,    // child of hwndMainControl
                            hwndTreeFrame,      // child of hwndSplitWindow
                            hwndTreeCnr,        // child of hwndTreeFrame
                            hwndFilesFrame,     // child of hwndSplitWindow
                            hwndFilesCnr;       // child of hwndFilesFrame

            HWND            hwndStatusBar;      // if present, or NULLHANDLE

            HAB             habGUI;             // anchor block of GUI thread

            // data for tree view (left)
            PSUBCLFOLDERVIEW psfvTree;
                                // XFolder subclassed view data (needed
                                // for cnr owner subclassing with XFolder's
                                // cooperation);
                                // created in fdlgFileDlg only once

            PMINIRECORDCORE precFolderToPopulate,
                                // gets set to the folder to populate when
                                // the user selects a record in the tree
                                // on the left; we then start a timer to
                                // delay the actual populate in case the
                                // user runs through a lot of folders;
                                // reset to NULL when the timer has elapsed
                            precFolderPopulating,
                                // while populate is running, this holds
                                // the folder that is populating to avoid
                                // duplicate populate posts
                            precTreeSelected,
                                // record that is currently selected
                                // in the tree on the left
                            precTreeExpanded,
                                // != NULL if the user not only selected
                                // a record, but expanded it as well
                            precFilesShowing;
                                // record whose contents are currently
                                // showing in the files cnr on the right;
                                // this is == precTreeSelected after the
                                // timer has elapsed and populate is done

            WPObject        *pobjUseList;
                                // object whose use list the following was
                                // added to
            USEITEM         uiDisplaying;
            VIEWITEM        viDisplaying;

            // data for files view (right)
            PSUBCLFOLDERVIEW psfvFiles;
                                // XFolder subclassed view data (see above)

            PCSZ            pcszFileMask;
                    // if this is != NULL, it must point to a CHAR
                    // array in the user code, and we will then
                    // populate the files cnr with file system objects
                    // only that match this file mask (even if it is "*").
                    // If it is NULL, this uses the folder filter.

            BOOL            fUnlockOnClear;
                    // set to TRUE by FM_POPULATED_FILLFILES when we
                    // received notification that we did a full populate
                    // and objects therefore should be unlocked when
                    // clearing out the container
                    // V0.9.21 (2002-09-13) [umoeller]

            BOOL            fSplitViewReady;
                    // while this is FALSE (during the initial setup),
                    // the split view refuses to react any changes in
                    // the containers; this must be set to TRUE explicitly
                    // by the user code

            ULONG           cThreadsRunning;
                    // if > 0, STPR_WAIT is used for the pointer

            // populate thread
            THREADINFO      tiSplitPopulate;
            volatile TID    tidSplitPopulate;
            HWND            hwndSplitPopulate;

            // LINKLIST        llTreeObjectsInserted;
                                // linked list of plain WPObject* pointers
                                // currently inserted in the left tree;
                                // no auto-free, needed for cleanup
            // LINKLIST        llFileObjectsInserted;
                                // linked list of plain WPObject* pointers
                                // currently inserted in the files cnr;
                                // no auto-free, needed for cleanup

        } FDRSPLITVIEW, *PFDRSPLITVIEW;

        #define INSERT_UNLOCKFILTERED       0x10000000

        VOID fdrInsertContents(WPFolder *pFolder,
                               HWND hwndCnr,
                               PMINIRECORDCORE precParent,
                               ULONG flInsert,
                               HWND hwndAddFirstChild,
                               PCSZ pcszFileMask);

        HPOINTER fdrSplitQueryPointer(PFDRSPLITVIEW psv);

        VOID fdrSplitPopulate(PFDRSPLITVIEW psv,
                              PMINIRECORDCORE prec,
                              ULONG fl);

        VOID fdrPostFillFolder(PFDRSPLITVIEW psv,
                               PMINIRECORDCORE prec,
                               ULONG fl);

        HWND fdrCreateFrameWithCnr(ULONG ulFrameID,
                                   HWND hwndParentOwner,
                                   ULONG flCnrStyle,
                                   HWND *phwndClient);

        MPARAM fdrSetupSplitView(HWND hwnd,
                                 PFDRSPLITVIEW psv);

        VOID fdrCleanupSplitView(PFDRSPLITVIEW psv);

        BOOL fdrSplitCreateFrame(WPObject *pRootObject,
                                 WPFolder *pRootsFolder,
                                 PFDRSPLITVIEW psv,
                                 ULONG flFrame,
                                 PCSZ pcszTitle,
                                 ULONG flSplit,
                                 PCSZ pcszFileMask,
                                 LONG lSplitBarPos);

    #endif
    #endif

    HWND fdrCreateSplitView(WPObject *pRootObject,
                            WPFolder *pRootsFolder,
                            ULONG ulView);

#endif


