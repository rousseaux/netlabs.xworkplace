
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
        #define FFL_FOLDERSONLY         0x0001
        #define FFL_SCROLLTO            0x0002
        #define FFL_EXPAND              0x0004
        #define FFL_SETBACKGROUND       0x0008

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

    #ifdef LINKLIST_HEADER_INCLUDED
    #ifdef THREADS_HEADER_INCLUDED

        /*
         *@@ FDRSPLITVIEW:
         *
         */

        typedef struct _FDRSPLITVIEW
        {
            USHORT          cbStruct;

            LONG            lSplitBarPos;       // initial split bar position in percent

            WPFolder        *pRootFolder;       // root folder to populate, whose contents
                                                // appear in left tree (constant)

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

            LINKLIST        llTreeObjectsInserted;
                                // linked list of plain WPObject* pointers
                                // currently inserted in the left tree;
                                // no auto-free, needed for cleanup
            LINKLIST        llFileObjectsInserted;
                                // linked list of plain WPObject* pointers
                                // currently inserted in the files cnr;
                                // no auto-free, needed for cleanup

        } FDRSPLITVIEW, *PFDRSPLITVIEW;

        VOID fdrInsertContents(WPFolder *pFolder,
                               HWND hwndCnr,
                               PMINIRECORDCORE precParent,
                               ULONG ulFoldersOnly,
                               HWND hwndAddFirstChild,
                               PCSZ pcszFileMask,
                               PLINKLIST pllObjects);

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

        BOOL fdrSplitCreateFrame(WPFolder *pRootFolder,
                                 PFDRSPLITVIEW psv,
                                 ULONG flFrame,
                                 PCSZ pcszTitle,
                                 ULONG flSplit,
                                 PCSZ pcszFileMask,
                                 LONG lSplitBarPos);

    #endif
    #endif

    HWND fdrCreateSplitView(WPFolder *somSelf,
                            ULONG ulView);

#endif


