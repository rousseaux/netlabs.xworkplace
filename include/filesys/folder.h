
/*
 *@@sourcefile folder.h:
 *      header file for folder.c (XFolder implementation).
 *
 *      This file is ALL new with V0.9.0.
 *
 *@@include #include <os2.h>
 *@@include #include "helpers\linklist.h"   // for some structures
 *@@include #include "helpers\tree.h"       // for some structures
 *@@include #include <wpfolder.h>
 *@@include #include "shared\notebook.h"    // for notebook callback prototypes
 *@@include #include "filesys\folder.h"
 */

/*
 *      Copyright (C) 1997-2002 Ulrich M�ller.
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

    #ifdef SOM_XFolder_h            // we access structs from xfldr.idl here

        #pragma pack(1)                 // SOM packs structures, apparently

        /*
         *@@ IBMSORTINFO:
         *      structure used internally by the WPS
         *      for sorting. This is undocumented and
         *      has been provided by Chris Wohlgemuth.
         *
         *      This is what wpQuerySortInfo really returns.
         *
         *      We intercept the address of this structure
         *      in the WPFolder instance data in
         *      XFolder::wpRestoreData and store the pointer
         *      in the XFolder instance data.
         *
         *@@added V0.9.12 (2001-05-18) [umoeller]
         */

        typedef struct _IBMSORTINFO
        {
            LONG            lDefaultSort;     // default sort column index
            BOOL            fAlwaysSort;      // "always maintain sort order"
            LONG            lCurrentSort;     // current sort column index
            PFNCOMPARE      pfnCompare;       // WPS comparison func called by fnCompareDetailsColumn
            ULONG           ulFieldOffset;    // field offset to compare
            M_WPObject      *Class;           // sort class
        } IBMSORTINFO, *PIBMSORTINFO;

        /*
         *@@ FDRBKGNDSTORE:
         *      WPS structure used for storing and restoring
         *      a folder's background settings. This corresponds
         *      to the IDKEY_FDRBACKGROUND key in wpRestoreState.
         *
         *      This information was originally from Henk Kelder.
         *
         *      Moved this here from xfldr.idl.
         *
         *@@added V1.0.0 (2002-08-24) [umoeller]
         */

        typedef struct _IBMFDRBKGNDSTORE
        {
            // 0x00 (0)
            PSZ     pszBitmapFile;
            // 0x04 (4)
            LONG    lcolBackground;
            // 0x08 (8)
            USHORT  usColorOrBitmap;        // 0x127 == color only
                                            // 0x128 == bitmap
                        #define BKGND_COLORONLY     0x127
                        #define BKGND_BITMAP        0x128

            // 0x0A (10)
            USHORT  usTiledOrScaled;        // 0x132 == normal
                                            // 0x133 == tiled
                                            // 0x134 == scaled
                        #define BKGND_NORMAL        0x132
                                // "normal" means, center the bitmap once
                                // in the window and use lcolBackground for
                                // the rest
                        #define BKGND_TILED         0x133
                                // "tiled" means, paint the bitmap as many
                                // times as it fits in the window and never
                                // paint the background color
                        #define BKGND_SCALED        0x134
                                // "scaled" means, enlarge the bitmap so
                                // that it is displayed
                                // usScaleFactor x usScaleFactor times in
                                // the window; never paint the background
                                // color either

            // 0x0C (12)
            USHORT  usScaleFactor;          // normally 1

            // 0x0E (14) bytes altogether

        } IBMFDRBKGNDSTORE, *PIBMFDRBKGNDSTORE;

        /*
         *@@ IBMFDRBKGND:
         *      transient structure used by the WPS internally
         *      to handle folder backgrounds.
         *
         *@@added V1.0.0 (2002-08-24) [umoeller]
         */

        typedef struct _IBMFDRBKGND
        {
            WPObject            *pobjImage;     // WPImageFile apparently
            IBMFDRBKGNDSTORE    BkgndStore;     // persistent data
        } IBMFDRBKGND, *PIBMFDRBKGND;

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
         *@@added V0.9.20 (2002-07-25) [umoeller]
         *@@changed V1.0.0 (2002-08-24) [umoeller]: greatly extended
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

            // these are non-attributes
            SOMObject       *pRWMonitor;            // as returned by wpQueryRWMonitorObject
            ULONG           ulUnknown1_1,
                            ulUnknown1_2,
                            ulUnknown1_3,
                            ulUnknown1_4;

            PIBMFDRBKGND    pCurrentBackground;
                        // always contains ptr to current folder background, either
                        // the default folder background struct or an instance one...
                        // this ptr gets returned by wpQueryBkgnd (now defined in
                        // idl\wps\wpfolder.idl)
            IBMFDRBKGND     Background;
                        // instance folder background data; apparently
                        // wpRestoreState writes into the BkgndStore member
                        // of this struct

            ULONG           ulUnknown2_1,
                            ulUnknown2_2;
            WPObject        *pFilter;               // as returned by _wpQueryFldrFilter
            ULONG           ulUnknown3;

            // sort info structure
            // (needed by us for extended folder sorting)
            IBMSORTINFO     SortInfo;

            ULONG           ulUnknown4_1,
                            ulUnknown4_2,
                            ulUnknown4_3,
                            ulUnknown4_4,
                            ulUnknown4_5,
                            ulUnknown4_6,
                            ulUnknown4_7,
                            ulUnknown4_8,
                            ulUnknown4_9,
                            ulUnknown4_10,
                            ulUnknown4_11,
                            ulUnknown4_12,
                            ulUnknown4_13,
                            ulUnknown4_14,
                            ulUnknown4_15,
                            ulUnknown4_16;

            // here comes the "folder long array" that is declared
            // in xfldr.idl and stored as such with wpSaveState
            FDRLONGARRAY    LongArray;

            // since FDRLONGARRAY has been extended with Warp 4, at least
            // the following fields are completely broken on Warp 3,
            // so watch out with their use

            BOOL            fShowAllInTreeView;
                                // FALSE == folders only, TRUE == all objects;
                                // Warp 4 only

            // folder string array as used in wpSaveState; there's
            // nothing in here we couldn't get through documented
            // instance methods, so no need to use these!
            FDRSTRINGARRAY  StringArray;

        } IBMFOLDERDATA, *PIBMFOLDERDATA;

        #pragma pack()

    #endif // SOM_XFolder_h

    /* ******************************************************************
     *
     *   Drive data
     *
     ********************************************************************/

    #pragma pack(1)

    /*
     *@@ IBMDRIVEDATA:
     *
     *      Here's a table of values from the testing on my system
     *      (V1.0.0 (2002-08-31) [umoeller]):
     *
     +      �����������������������������������������������������������������������������������Ŀ
     +      � fs             � bFileSystem       �fRemote�fFixed � fZIP  � bDiskType            �
     +      �����������������������������������������������������������������������������������Ĵ
     +      � H: unformatted � 0xF (?!?!?)       � FALSE � TRUE  � FALSE � DRVTYPE_HARDDISK (0) �
     +      �����������������������������������������������������������������������������������Ĵ
     +      � D: FAT16 part. � FSYS_FAT (1)      � FALSE � TRUE  � FALSE � DRVTYPE_HARDDISK (0) �
     +      �����������������������������������������������������������������������������������Ĵ
     +      � C: FAT32 part. � FSYS_REMOTE (10)  � FALSE � TRUE  � FALSE � DRVTYPE_HARDDISK (0) �
     +      �����������������������������������������������������������������������������������Ĵ
     +      � I: HPFS part.  � FSYS_HPFS_JFS (2) � FALSE � TRUE  � FALSE � DRVTYPE_HARDDISK (0) �
     +      �����������������������������������������������������������������������������������Ĵ
     +      � N: JFS part.   � FSYS_HPFS_JFS (2) � FALSE � TRUE  � FALSE � DRVTYPE_HARDDISK (0) �
     +      �����������������������������������������������������������������������������������Ĵ
     +      � R: NetBIOS     � 0x19 (?!?!?)      � TRUE  � TRUE  � FALSE � DRVTYPE_UNKNOWN (255)�
     +      �����������������������������������������������������������������������������������Ĵ
     +      � X: CD w/media  � FSYS_CDFS (3)     � FALSE � FALSE � FALSE � DRVTYPE_CDROM (4)    �
     +      �����������������������������������������������������������������������������������Ĵ
     +      � Y: CD w/o med. � FSYS_CDFS (3)     � FALSE � FALSE � FALSE � DRVTYPE_CDROM (4)    �
     +      �����������������������������������������������������������������������������������Ĵ
     +      � Z: RAMFS virt. � 0x21 (?!?!?)      � TRUE  � FALSE � FALSE � DRVTYPE_LAN (5)      �
     +      �������������������������������������������������������������������������������������
     +
     *      From all I've seen, ulSerial IS ALWAYS NULL.
     *
     *      szSerial NEVER seems to be set either.
     *
     *@@added V0.9.16 (2002-01-01) [umoeller]
     */

    typedef struct _IBMDRIVEDATA
    {
        BYTE        bFileSystem;    // one of the FSYS_* values (dosh.h)
        BOOL        fNotLocal;      // TRUE if drive is not local
        BOOL        fFixedDisk;     // FALSE if drive is floppy or CD-ROM
        BOOL        fZIP;           // TRUE if drive is ZIP or something
        BOOL        fUnknown;
        BYTE        bUnknown;
        BYTE        bDiskType;      // one of the DRVTYPE_* values (dosh.h)
        WPObject    *pDisk;         // WPDisk object
        WPFolder    *pRootFolder;   // WPRootFolder or NULL
        ULONG       ulFreeKB;
        ULONG       ulTotalKB;
        BOOL        fCanLongname;
        BOOL        fCanEAs;
        ULONG       ulSerial;       // volume serial number
        BYTE        cbVolLabel;     // volume label length
        CHAR        szVolLabel[12]; // volume label
    } IBMDRIVEDATA, *PIBMDRIVEDATA;

    #pragma pack()

    // global variable to WPS-internal drive data array, set in init.c
    extern PIBMDRIVEDATA    G_paDriveData;

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

    typedef BOOL _Optlink FNFOREACHVIEWCALLBACK(WPFolder *somSelf,
                                                HWND hwndView,
                                                ULONG ulView,
                                                ULONG ulUser);
    typedef FNFOREACHVIEWCALLBACK *PFNFOREACHVIEWCALLBACK;

    BOOL fdrForEachOpenInstanceView(WPFolder *somSelf,
                                    PFNFOREACHVIEWCALLBACK pfnCallback,
                                    ULONG ulUser);

    BOOL fdrForEachOpenGlobalView(PFNFOREACHVIEWCALLBACK pfnCallback,
                                  ULONG ulUser);

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
     *   Folder refresh
     *
     ********************************************************************/

    BOOL fdrForceRefresh(WPFolder *pFolder);

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

    BOOL _Optlink fdrSortAllViews(WPFolder *somSelf,
                                  HWND hwndView,
                                  ULONG ulView,
                                  ULONG ulSort);

    BOOL fdrSortViewOnce(WPFolder *somSelf,
                         HWND hwndFrame,
                         long lSort);

    VOID fdrSetFldrCnrSort(WPFolder *somSelf,
                           HWND hwndCnr,
                           BOOL fForce);

    BOOL _Optlink fdrUpdateFolderSorts(WPFolder *somSelf,
                                       HWND hwndView,
                                       ULONG ulView,
                                       ULONG fForce);

    /* ******************************************************************
     *
     *   XFolder window procedures
     *
     ********************************************************************/

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

    ULONG fdrCheckIfPopulated(WPFolder *somSelf,
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


