
/*
 *@@sourcefile folder.h:
 *      header file for folder.c (XFolder implementation).
 *
 *      This file is ALL new with V0.9.0.
 *
 *@@include #include <os2.h>
 *@@include #include "helpers\linklist.h"  // for some structures
 *@@include #include "helpers\tree.h"  // for some structures
 *@@include #include <wpfolder.h>
 *@@include #include "shared\notebook.h"   // for notebook callback prototypes
 *@@include #include "filesys\folder.h"
 */

/*
 *      Copyright (C) 1997-2002 Ulrich M”ller.
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

#ifndef FOLDER_HEADER_INCLUDED
    #define FOLDER_HEADER_INCLUDED

    /* ******************************************************************
     *
     *   IBM folder instance data
     *
     ********************************************************************/

    #pragma pack(1)                 // SOM packs structures, apparently

    /*
     *@@ IBMFOLDERDATA:
     *      WPFolder instance data structure, as far as I
     *      have been able to decode it. See
     *      XFldObject::wpInitData where we get a pointer
     *      to this.
     *
     *      WARNING: This is the result of the testing done
     *      on eComStation, i.e. the MCP1 code level of the
     *      WPS. I have not tested whether the struct ordering
     *      is the same on all older versions of OS/2, nor can
     *      I guarantee that the ordering will stay the same
     *      in the future (even though it is unlikely that
     *      anyone at IBM is capable of changing this structure
     *      any more in the first place).
     *
     *      There are many more fields coming after this, but
     *      right now I only need those.
     *
     *@@added V0.9.20 (2002-07-25) [umoeller]
     */

    typedef struct _IBMFOLDERDATA
    {
        // all these are also SOM readonly attributes and appear
        // as _get_XXX in the WPS class list method table
        WPObject        *FirstObj,              // first object of contents linked list;
                                                // each object has a pobjNext attribute
                        *LastObj;               // last object of contents linked list
        ULONG           hmtxOneFindAtATime;     // whatever this is
        ULONG           retaddrFindSemOwner;    // whatever this is
        ULONG           hevFillFolder;          // whatever this is

        // many more fields following apparently, not decoded yet

    } IBMFOLDERDATA, *PIBMFOLDERDATA;

    #pragma pack()

    /* ******************************************************************
     *
     *   Additional declarations for xfldr.c
     *
     ********************************************************************/
/*
#define VIEW_CONTENTS      0x00000001
#define VIEW_SETTINGS      0x00000002
#define VIEW_HELP          0x00000004
#define VIEW_RUNNING       0x00000008
#define VIEW_DETAILS       0x00000010
#define VIEW_TREE          0x00000020
#define VIEW_ANY           0xFFFFFFFF
*/

    #define VIEW_SPLIT         0x00002000

    #ifdef LINKLIST_HEADER_INCLUDED
        /*
         *@@ ENUMCONTENT:
         *      this is the structure which represents
         *      an enumeration handle for XFolder::xfBeginEnumContent
         *      etc.
         */

        typedef struct _ENUMCONTENT
        {
            PLINKLIST   pllOrderedContent;  // created by XFolder::xfBeginEnumContent
            PLISTNODE   pnodeLastQueried;   // initially NULL;
                                            // updated by XFolder::xfEnumNext
        } ENUMCONTENT, *PENUMCONTENT;
    #endif

    /*
     *@@ ORDEREDLISTITEM:
     *      linked list structure for the ordered list
     *      of objects in a folder
     *      (XFolder::xwpBeginEnumContent).
     */

    typedef struct _ORDEREDLISTITEM
    {
        WPObject                *pObj;
        CHAR                    szIdentity[CCHMAXPATH];
    } ORDEREDLISTITEM, *PORDEREDLISTITEM;

    /*
     *@@ SORTBYICONPOS:
     *      structure for GetICONPOS.
     */

    typedef struct _SORTBYICONPOS
    {
        CHAR    szRealName[CCHMAXPATH];
        PBYTE   pICONPOS;
        USHORT  usICONPOSSize;
    } SORTBYICONPOS, *PSORTBYICONPOS;

    // prototype for wpSetMenuBarVisibility;
    // this is resolved by name (fdrmenus.c)

    typedef BOOL _System xfTP_wpSetMenuBarVisibility(WPFolder *somSelf,
                                                     ULONG ulVisibility);
    typedef xfTP_wpSetMenuBarVisibility *xfTD_wpSetMenuBarVisibility;

    /* ******************************************************************
     *
     *   Setup strings
     *
     ********************************************************************/

    BOOL fdrHasShowAllInTreeView(WPFolder *somSelf);

    BOOL fdrSetup(WPFolder *somSelf,
                  PCSZ pszSetupString);

    BOOL fdrQuerySetup(WPObject *somSelf,
                       PVOID pstrSetup);

    /* ******************************************************************
     *
     *   Folder view helpers
     *
     ********************************************************************/

    BOOL fdrForEachOpenInstanceView(WPFolder *somSelf,
                                    ULONG ulMsg,
                                    PFNWP pfnwpCallback);

    BOOL fdrForEachOpenGlobalView(ULONG ulMsg,
                                  PFNWP pfnwpCallback);

    VOID stbUpdate(WPFolder *pFolder);

    /* ******************************************************************
     *
     *   Full path in title
     *
     ********************************************************************/

    BOOL fdrSetOneFrameWndTitle(WPFolder *somSelf, HWND hwndFrame);

    #define FDRUPDATE_TITLE         0
    #define FDRUPDATE_REPAINT       1

    BOOL fdrUpdateAllFrameWindows(WPFolder *somSelf,
                                    ULONG ulAction);

    /* ******************************************************************
     *
     *   Quick Open
     *
     ********************************************************************/

    typedef BOOL _Optlink FNCBQUICKOPEN(WPFolder *pFolder,
                                        WPObject *pObject,
                                        ULONG ulNow,
                                        ULONG ulMax,
                                        ULONG ulCallbackParam);
    typedef FNCBQUICKOPEN *PFNCBQUICKOPEN;

    BOOL fdrQuickOpen(WPFolder *pFolder,
                      PFNCBQUICKOPEN pfnCallback,
                      ULONG ulCallbackParam);

#ifndef __NOSNAPTOGRID__
    /* ******************************************************************
     *
     *   Snap To Grid
     *
     ********************************************************************/

    BOOL fdrSnapToGrid(WPFolder *somSelf,
                       BOOL fNotify);
#endif

    /* ******************************************************************
     *
     *   Extended Folder Sort (fdrsort.c)
     *
     ********************************************************************/

    BOOL fdrModifySortMenu(WPFolder *somSelf,
                           HWND hwndSortMenu);

    BOOL fdrSortMenuItemSelected(WPFolder *somSelf,
                                 HWND hwndFrame,
                                 HWND hwndMenu,
                                 ULONG ulMenuId,
                                 PBOOL pbDismiss);

    PFN fdrQuerySortFunc(WPFolder *somSelf,
                         LONG lSort);

    BOOL fdrHasAlwaysSort(WPFolder *somSelf);

    MRESULT EXPENTRY fdrSortAllViews(HWND hwndView,
                                     ULONG ulSort,
                                     MPARAM mpView,
                                     MPARAM mpFolder);

    BOOL fdrSortViewOnce(WPFolder *somSelf,
                         HWND hwndFrame,
                         long lSort);

    VOID fdrSetFldrCnrSort(WPFolder *somSelf,
                           HWND hwndCnr,
                           BOOL fForce);

    MRESULT EXPENTRY fdrUpdateFolderSorts(HWND hwndView,
                                          ULONG ulDummy,
                                          MPARAM mpView,
                                          MPARAM mpFolder);

    /********************************************************************
     *
     *   Folder frame window subclassing
     *
     ********************************************************************/

    #define BARPULL_FOLDER      0       // never set
    #define BARPULL_EDIT        2
    #define BARPULL_VIEW        3
    #define BARPULL_HELP        4

    /*
     *@@ SUBCLFOLDERVIEW:
     *      linked list structure used with folder frame
     *      window subclassing. One of these structures
     *      is created for each folder view (window) which
     *      is subclassed by fdrSubclassFolderView and
     *      stored in a global linked list.
     *
     *      This is one of the most important kludges which
     *      XFolder uses to hook itself into the WPS.
     *      Most importantly, this structure stores the
     *      original frame window procedure before the
     *      folder frame window was subclassed, but we also
     *      use this to store various other data for status
     *      bars etc.
     *
     *      We need this additional structure because all
     *      the data in here is _view_-specific, not
     *      folder-specific, so we cannot store this in
     *      the instance data.
     *
     *@@changed V0.9.1 (2000-01-29) [umoeller]: added pSourceObject and ulSelection fields
     *@@changed V0.9.2 (2000-03-06) [umoeller]: removed ulView, because this might change
     *@@changed V0.9.3 (2000-04-07) [umoeller]: renamed from SUBCLASSEDLISTITEM
     *@@changed V0.9.9 (2001-03-10) [umoeller]: added ulWindowWordOffset
     *@@changed V0.9.19 (2002-04-17) [umoeller]: renamed from SUBCLASSEDFOLDERVIEW
     */

    typedef struct _SUBCLFOLDERVIEW
    {
        HWND        hwndFrame;          // folder view frame window
        WPFolder    *somSelf;           // folder object
        WPObject    *pRealObject;       // "real" object; this is == somSelf
                                        // for folders, but the corresponding
                                        // disk object for WPRootFolders
        PFNWP       pfnwpOriginal;      // orig. frame wnd proc before subclassing
                                        // (WPS folder proc)
        ULONG       ulWindowWordOffset; // as passed to fdrSubclassFolderView

        HWND        hwndStatusBar,      // status bar window; NULL if there's no
                                        // status bar for this view
                    hwndCnr,            // cnr window (child of hwndFrame)
                    hwndSupplObject;    // supplementary object wnd
                                        // (fdr_fnwpSupplFolderObject)

        BOOL        fNeedCnrScroll;     // scroll container after adding status bar?
        BOOL        fRemoveSourceEmphasis; // flag for whether XFolder has added
                                        // container source emphasis

        ULONG       ulLastSelMenuItem;  // last selected menu item ID
        WPObject    *pSourceObject;     // object whose record core has source
                                        // emphasis;
                                        // this field is valid only between
                                        // WM_INITMENU and WM_COMMAND; if this
                                        // is NULL, the entire folder whitespace
                                        // has source emphasis
        ULONG       ulSelection;        // SEL_* flags;
                                        // this field is valid only between
                                        // WM_INITMENU and WM_COMMAND
    } SUBCLFOLDERVIEW, *PSUBCLFOLDERVIEW;

    PSUBCLFOLDERVIEW fdrCreateSFV(HWND hwndFrame,
                                  HWND hwndCnr,
                                  ULONG ulWindowWordOffset,
                                  WPFolder *somSelf,
                                  WPObject *pRealObject);

    PSUBCLFOLDERVIEW fdrSubclassFolderView(HWND hwndFrame,
                                           HWND hwndCnr,
                                           WPFolder *somSelf,
                                           WPObject *pRealObject);

    PSUBCLFOLDERVIEW fdrQuerySFV(HWND hwndFrame,
                                 PULONG pulIndex);

    VOID fdrManipulateNewView(WPFolder *somSelf,
                              HWND hwndNewFrame,
                              ULONG ulView);

    VOID fdrRemoveSFV(PSUBCLFOLDERVIEW psfv);

    BOOL fdrProcessObjectCommand(WPFolder *somSelf,
                                 USHORT usCommand,
                                 HWND hwndCnr,
                                 WPObject* pFirstObject,
                                 ULONG ulSelectionFlags);

    MRESULT fdrProcessFolderMsgs(HWND hwndFrame,
                                 ULONG msg,
                                 MPARAM mp1,
                                 MPARAM mp2,
                                 PSUBCLFOLDERVIEW psfv,
                                 PFNWP pfnwpOriginal);

    // Supplementary object window msgs (for each
    // subclassed folder frame, xfldr.c)
    #define SOM_ACTIVATESTATUSBAR       (WM_USER+100)
    // #define SOM_CREATEFROMTEMPLATE      (WM_USER+101)
                // removed V0.9.9 (2001-03-27) [umoeller]

    MRESULT EXPENTRY fdr_fnwpSupplFolderObject(HWND hwndObject,
                                               ULONG msg,
                                               MPARAM mp1,
                                               MPARAM mp2);

    /* ******************************************************************
     *
     *   XFolder window procedures
     *
     ********************************************************************/

    MRESULT EXPENTRY fdr_fnwpStatusBar(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);

    MRESULT EXPENTRY fdr_fnwpSubclFolderContentMenu(HWND hwndMenu, ULONG msg, MPARAM mp1, MPARAM mp2);

    SHORT XWPENTRY fdrSortByICONPOS(PVOID pItem1, PVOID pItem2, PVOID psip);

    /* ******************************************************************
     *
     *   Folder edit dialogs
     *
     ********************************************************************/

    VOID fdrShowSelectSome(HWND hwndFrame);

    VOID fdrShowBatchRename(HWND hwndFrame);

    #ifdef SOM_WPFolder_h
        VOID fdrShowPasteDlg(WPFolder *pFolder,
                             HWND hwndFrame);
    #endif

    /* ******************************************************************
     *
     *   Folder split views (fdrsplit.c)
     *
     ********************************************************************/

    #ifdef LINKLIST_HEADER_INCLUDED
    #ifdef THREADS_HEADER_INCLUDED

        WPFileSystem* fdrGetFSFromRecord(PMINIRECORDCORE precc,
                                         BOOL fFoldersOnly);

        BOOL fdrIsInsertable(WPObject *pObject,
                             BOOL ulFoldersOnly,
                             PCSZ pcszFileMask);

        BOOL fdrIsObjectInCnr(WPObject *pObject,
                              HWND hwndCnr);

        VOID fdrInsertContents(WPFolder *pFolder,
                               HWND hwndCnr,
                               PMINIRECORDCORE precParent,
                               ULONG ulFoldersOnly,
                               HWND hwndAddFirstChild,
                               PCSZ pcszFileMask,
                               PLINKLIST pllObjects);

        ULONG fdrClearContainer(HWND hwndCnr,
                                PLINKLIST pllObjects);

        #define ID_TREEFRAME            1001
        #define ID_FILESFRAME           1002

        #define FM_FILLFOLDER           (WM_USER + 1)
            #define FFL_FOLDERSONLY         0x0001
            #define FFL_SCROLLTO            0x0002
            #define FFL_EXPAND              0x0004

        #define FM_POPULATED_FILLTREE   (WM_USER + 2)
        #define FM_POPULATED_SCROLLTO   (WM_USER + 3)
        #define FM_POPULATED_FILLFILES  (WM_USER + 4)
        #define FM_UPDATEPOINTER        (WM_USER + 5)

        #define FM2_POPULATE            (WM_USER + 6)
        #define FM2_ADDFIRSTCHILD_BEGIN (WM_USER + 7)
        #define FM2_ADDFIRSTCHILD_NEXT  (WM_USER + 8)
        #define FM2_ADDFIRSTCHILD_DONE  (WM_USER + 9)

        /*
         *@@ FDRSPLITVIEW:
         *
         */

        typedef struct _FDRSPLITVIEW
        {
            // window hierarchy
            HWND            hwndMainFrame,
                            hwndMainControl;    // child of hwndMainFrame

            HWND            hwndSplitWindow,    // child of hwndMainControl
                            hwndTreeFrame,      // child of hwndSplitWindow
                            hwndTreeCnr,        // child of hwndTreeFrame
                            hwndFilesFrame,     // child of hwndSplitWindow
                            hwndFilesCnr;       // child of hwndFilesFrame

            // data for drives view (left)
            PSUBCLFOLDERVIEW psfvTree;          // XFolder subclassed view data (needed
                                                // for cnr owner subclassing with XFolder's
                                                // cooperation);
                                                // created in fdlgFileDlg only once

            WPFolder        *pRootFolder;       // root folder to populate, whose contents
                                                // appear in left tree (constant)

            PMINIRECORDCORE precFolderContentsShowing;   // currently selected record

            // data for files view (right)
            PSUBCLFOLDERVIEW psfvFiles;         // XFolder subclassed view data (see above)
            BOOL            fFilesFrameSubclassed;  // TRUE after first insert

            BOOL            fFileDlgReady;
                    // while this is FALSE (during the initial setup),
                    // the dialog doesn't react to any changes in the containers

            ULONG           cThreadsRunning;
                    // if > 0, STPR_WAIT is used for the pointer

            // populate thread
            THREADINFO      tiSplitPopulate;
            volatile TID    tidSplitPopulate;
            HWND            hwndSplitPopulate;

            LINKLIST        llTreeObjectsInserted; // linked list of plain WPObject* pointers
                                                // inserted, no auto-free; needed for cleanup
            LINKLIST        llFileObjectsInserted;

        } FDRSPLITVIEW, *PFDRSPLITVIEW;

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
                                 BOOL fMultipleSel,
                                 PFDRSPLITVIEW psv);

        VOID fdrCleanupSplitView(PFDRSPLITVIEW psv);

    #endif
    #endif

    HWND fdrCreateSplitView(WPFolder *somSelf,
                            ULONG ulView);

    /* ******************************************************************
     *
     *   Folder semaphores
     *
     ********************************************************************/

    #ifdef SOM_WPFolder_h

        typedef ULONG _System xfTP_RequestWrite(SOMAny *somSelf);
        typedef xfTP_RequestWrite *xfTD_RequestWrite;

        typedef ULONG _System xfTP_ReleaseWrite(SOMAny *somSelf);
        typedef xfTP_ReleaseWrite *xfTD_ReleaseWrite;

        /*
         * xfTP_wpFlushNotifications:
         *      prototype for WPFolder::wpFlushNotifications.
         *
         *      See the Warp 4 Toolkit documentation for details.
         */

        typedef BOOL _System xfTP_wpFlushNotifications(WPFolder *somSelf);
        typedef xfTP_wpFlushNotifications *xfTD_wpFlushNotifications;

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
    #endif

    // wrappers
    ULONG fdrRequestFolderWriteMutexSem(WPFolder *somSelf);

    ULONG fdrReleaseFolderWriteMutexSem(WPFolder *somSelf);

    ULONG fdrFlushNotifications(WPFolder *somSelf);

    BOOL fdrGetNotifySem(ULONG ulTimeout);

    VOID fdrReleaseNotifySem(VOID);

    /* ******************************************************************
     *
     *   Folder content management
     *
     ********************************************************************/

    #ifdef XWPTREE_INCLUDED

        typedef struct _FDRCONTENTITEM
        {
            TREE        Tree;
                    // -- for file-system objects, ulKey is
                    //    a PSZ with the object's short real name
                    //    which _must_ be upper-cased.
                    //    WARNING: This PSZ points into XWPFileSystem's
                    //    instance data!!
                    // -- for abstracts, ulKey has the 32-bit
                    //    object handle (_wpQueryHandle)
            WPObject    *pobj;
                    // object pointer
        } FDRCONTENTITEM, *PFDRCONTENTITEM;

    #endif

    WPObject* fdrFastFindFSFromName(WPFolder *pFolder,
                                    const char *pcszShortName);

    WPObject* fdrSafeFindFSFromName(WPFolder *pFolder,
                                    const char *pcszShortName);

    BOOL fdrAddToContent(WPFolder *somSelf,
                         WPObject *pObject,
                         BOOL *pfCallParent);

    BOOL fdrRealNameChanged(WPFolder *somSelf,
                            WPObject *pFSObject);

    BOOL fdrDeleteFromContent(WPFolder *somSelf,
                              WPObject *pObject);

    /*
     *@@ xfTP_wpMatchesFilter:
     *      this WPFilter instance method returns TRUE if
     *      pObject matches the filter and should therefore
     *      not be visible.
     *
     *      somSelf must be a WPFilter object really, but
     *      since that class isn't documented, we use
     *      WPObject (since WPFilter is derived from
     *      WPTransient).
     *
     *@@added V0.9.16 (2002-01-05) [umoeller]
     */

    typedef BOOL _System xfTP_wpMatchesFilter(WPObject *pFilter, WPObject *pObject);
    typedef xfTP_wpMatchesFilter *xfTD_wpMatchesFilter;

    BOOL fdrIsObjectFiltered(WPFolder *pFolder,
                             WPObject *pObject);

    WPObject* fdrQueryContent(WPFolder *somSelf,
                              WPObject *pobjFind,
                              ULONG ulOption);

    #define QCAFL_FILTERINSERTED        0x0001

    WPObject** fdrQueryContentArray(WPFolder *pFolder,
                                    ULONG flFilter,
                                    PULONG pulItems);

    BOOL fdrNukeContents(WPFolder *pFolder);

    /* ******************************************************************
     *
     *   Folder population
     *
     ********************************************************************/

    #ifdef __DEBUG__
        VOID fdrDebugDumpFolderFlags(WPFolder *somSelf);
    #else
        #define fdrDebugDumpFolderFlags(x)
    #endif

    BOOL fdrPopulate(WPFolder *somSelf,
                     PCSZ pcszFolderFullPath,
                     HWND hwndReserved,
                     BOOL fFoldersOnly,
                     PBOOL pfExit);

    BOOL fdrCheckIfPopulated(WPFolder *somSelf,
                             BOOL fFoldersOnly);

    /* ******************************************************************
     *
     *   Awake-objects test
     *
     ********************************************************************/

    BOOL fdrRegisterAwakeRootFolder(WPFolder *somSelf);

    BOOL fdrRemoveAwakeRootFolder(WPFolder *somSelf);

    #ifdef SOM_WPFileSystem_h
        WPFileSystem* fdrQueryAwakeFSObject(PCSZ pcszFQPath);
    #endif

    /* ******************************************************************
     *
     *   Object insertion
     *
     ********************************************************************/

    BOOL fdrCnrInsertObject(WPObject *pObject);

    ULONG fdrInsertAllContents(WPFolder *pFolder);

    /* ******************************************************************
     *
     *   Notebook callbacks (notebook.c) for XFldWPS  "View" page
     *
     ********************************************************************/

    #ifdef NOTEBOOK_HEADER_INCLUDED
        VOID XWPENTRY fdrViewInitPage(PNOTEBOOKPAGE pnbp,
                                      ULONG flFlags);

        MRESULT XWPENTRY fdrViewItemChanged(PNOTEBOOKPAGE pnbp,
                                   ULONG ulItemID, USHORT usNotifyCode,
                                   ULONG ulExtra);

    /* ******************************************************************
     *
     *   Notebook callbacks (notebook.c) for XFldWPS"Grid" page
     *
     ********************************************************************/

#ifndef __NOSNAPTOGRID__
        VOID XWPENTRY fdrGridInitPage(PNOTEBOOKPAGE pnbp,
                                      ULONG flFlags);

        MRESULT XWPENTRY fdrGridItemChanged(PNOTEBOOKPAGE pnbp,
                                   ULONG ulItemID,
                                   USHORT usNotifyCode,
                                   ULONG ulExtra);
#endif

    /* ******************************************************************
     *
     *   Notebook callbacks (notebook.c) for "XFolder" instance page
     *
     ********************************************************************/


        VOID XWPENTRY fdrXFolderInitPage(PNOTEBOOKPAGE pnbp,
                                         ULONG flFlags);

        MRESULT XWPENTRY fdrXFolderItemChanged(PNOTEBOOKPAGE pnbp,
                                      ULONG ulItemID,
                                      USHORT usNotifyCode,
                                      ULONG ulExtra);

        VOID XWPENTRY fdrSortInitPage(PNOTEBOOKPAGE pnbp, ULONG flFlags);

        MRESULT XWPENTRY fdrSortItemChanged(PNOTEBOOKPAGE pnbp,
                                   ULONG ulItemID,
                                   USHORT usNotifyCode,
                                   ULONG ulExtra);

    /* ******************************************************************
     *
     *   XFldStartup notebook callbacks (notebook.c)
     *
     ********************************************************************/

        VOID XWPENTRY fdrStartupFolderInitPage(PNOTEBOOKPAGE pnbp,
                                               ULONG flFlags);

        MRESULT XWPENTRY fdrStartupFolderItemChanged(PNOTEBOOKPAGE pnbp,
                        ULONG ulItemID, USHORT usNotifyCode,
                        ULONG ulExtra);
    #endif

    /********************************************************************
     *
     *   Folder hotkey functions (fdrhotky.c)
     *
     ********************************************************************/

    // maximum no. of folder hotkeys
    #define FLDRHOTKEYCOUNT (ID_XSSI_LB_LAST-ID_XSSI_LB_FIRST+1)

    // maximum length of folder hotkey descriptions
    #define MAXLBENTRYLENGTH 50

    /*
     *@@ XFLDHOTKEY:
     *      XFolder folder hotkey definition.
     *      A static array of these exists in folder.c.
     */

    typedef struct _XFLDHOTKEY
    {
        USHORT  usFlags;     //  Keyboard control codes
        USHORT  usKeyCode;   //  Hardware scan code
        USHORT  usCommand;   //  corresponding menu item id to send to container
    } XFLDHOTKEY, *PXFLDHOTKEY;

    #define FLDRHOTKEYSSIZE sizeof(XFLDHOTKEY)*FLDRHOTKEYCOUNT

    PXFLDHOTKEY fdrQueryFldrHotkeys(VOID);

    void fdrLoadDefaultFldrHotkeys(VOID);

    void fdrLoadFolderHotkeys(VOID);

    void fdrStoreFldrHotkeys(VOID);

    BOOL fdrFindHotkey(USHORT usCommand,
                       PUSHORT pusFlags,
                       PUSHORT pusKeyCode);

    BOOL fdrProcessFldrHotkey(WPFolder *somSelf,
                              HWND hwndFrame,
                              USHORT usFlags,
                              USHORT usch,
                              USHORT usvk);

    VOID fdrAddHotkeysToPulldown(HWND hwndPulldown,
                                 const ULONG *paulMenuIDs,
                                 ULONG cMenuIDs);

    VOID fdrAddHotkeysToMenu(WPObject *somSelf,
                             HWND hwndCnr,
                             HWND hwndMenu);

    #ifdef NOTEBOOK_HEADER_INCLUDED
        VOID XWPENTRY fdrHotkeysInitPage(PNOTEBOOKPAGE pnbp,
                                         ULONG flFlags);

        MRESULT XWPENTRY fdrHotkeysItemChanged(PNOTEBOOKPAGE pnbp,
                                      ULONG ulItemID,
                                      USHORT usNotifyCode,
                                      ULONG ulExtra);
    #endif

    /********************************************************************
     *
     *   Folder messaging (fdrsubclass.c)
     *
     ********************************************************************/

    #ifdef INCL_WINHOOKS
        VOID EXPENTRY fdr_SendMsgHook(HAB hab,
                                      PSMHSTRUCT psmh,
                                      BOOL fInterTask);
    #endif

    /* ******************************************************************
     *
     *   Start folder contents
     *
     ********************************************************************/

    ULONG fdrStartFolderContents(WPFolder *pFolder,
                                 ULONG ulTiming);

#endif


