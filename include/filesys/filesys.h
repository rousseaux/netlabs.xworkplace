
/*
 *@@sourcefile filesys.h:
 *      header file for filesys.c (extended file types implementation).
 *
 *      This file is ALL new with V0.9.0.
 *
 *@@include #include <os2.h>
 *@@include #include "shared\notebook.h"
 *@@include #include "filesys\filesys.h"
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

#ifndef FILESYS_HEADER_INCLUDED
    #define FILESYS_HEADER_INCLUDED

    /* ******************************************************************
     *
     *   File system information implementation
     *
     ********************************************************************/

    #ifdef SOM_WPFileSystem_h

        PSZ fsysQueryEASubject(WPFileSystem *somSelf);

        PSZ fsysQueryEAComments(WPFileSystem *somSelf);

        PSZ fsysQueryEAKeyphrases(WPFileSystem *somSelf);

        BOOL fsysSetEASubject(WPFileSystem *somSelf, const char *psz);

        BOOL fsysSetEAComments(WPFileSystem *somSelf, const char *psz);

        BOOL fsysSetEAKeyphrases(WPFileSystem *somSelf, const char *psz);

        /*
         * xfTP_wpQueryRefreshFlags:
         *      prototype for WPFileSystem::wpQueryRefreshFlags.
         *
         *      See the Warp 4 toolkit docs.
         */

        typedef ULONG _System xfTP_wpQueryRefreshFlags(WPFileSystem *somSelf);
        typedef xfTP_wpQueryRefreshFlags *xfTD_wpQueryRefreshFlags;

        ULONG fsysQueryRefreshFlags(WPFileSystem *somSelf);

        /*
         * xfTP_wpSetRefreshFlags:
         *      prototype for WPFileSystem::wpSetRefreshFlags.
         *
         *      See the Warp 4 toolkit docs.
         */

        typedef BOOL _System xfTP_wpSetRefreshFlags(WPFileSystem *somSelf,
                                                    ULONG ulRefreshFlags);
        typedef xfTP_wpSetRefreshFlags *xfTD_wpSetRefreshFlags;

        BOOL fsysSetRefreshFlags(WPFileSystem *somSelf, ULONG ulRefreshFlags);

        /*
         * xfTP_wpRefreshFSInfo:
         *      prototype for WPFileSystem::wpRefreshFSInfo.
         *
         */

        typedef BOOL _System xfTP_wpRefreshFSInfo(WPFileSystem *somSelf,
                                                  ULONG ulUnknown,
                                                  PVOID pvFileBuf,
                                                  BOOL fRefreshIcon);
        typedef xfTP_wpRefreshFSInfo *xfTD_wpRefreshFSInfo;

        BOOL fsysRefreshFSInfo(WPFileSystem *somSelf, PFILEFINDBUF3 pfb3);

        #ifndef DIRTYBIT
            #define DIRTYBIT    0x80000000
        #endif
        #ifndef FOUNDBIT
            #define FOUNDBIT    0x40000000
        #endif

    /* ******************************************************************
     *
     *   Drive data
     *
     ********************************************************************/

        #pragma pack(1)

        #define FSYS_FAT            1
        #define FSYS_HPFS_JFS       2
        #define FSYS_CDFS           3
        #define FSYS_REMOTE         10

        #define DISKTYPE_HARDDISK   0
        #define DISKTYPE_FLOPPY     1
        #define DISKTYPE_TAPE       2
        #define DISKTYPE_VDISK      3
        #define DISKTYPE_CDROM      4
        #define DISKTYPE_LAN        5
        #define DISKTYPE_UNKNOWN    255

        /*
         *@@ WPSDRIVEDATA:
         *
         *      --  LAN drives: fNotLocal == TRUE, fFixed == TRUE
         *
         *@@added V0.9.16 (2002-01-01) [umoeller]
         */

        typedef struct _WPSDRIVEDATA
        {
            BYTE    bFileSystem;    // one of the FSYS_* values
            BOOL    fNotLocal;      // TRUE if drive is not local
            BOOL    fFixedDisk;     // FALSE if drive is floppy or CD-ROM
            BOOL    fZIP;           // TRUE if drive is ZIP or something
            BOOL    fUnknown;
            BYTE    bUnknown;
            BYTE    bDiskType;      // one of the DISKTYPE_* values
            PVOID   pDisk;          // WPDisk object
            PVOID   pRootFolder;    // WPRootFolder or NULL
            ULONG   ulFreeKB;
            ULONG   ulTotalKB;
            BOOL    fCanLongname;
            BOOL    fCanEAs;
            ULONG   ulSerial;       // volume serial number
            BYTE    cbVolLabel;     // volume label length
            CHAR    szVolLabel[12]; // volume label
        } WPSDRIVEDATA, *PWPSDRIVEDATA;

        #pragma pack()

        typedef PWPSDRIVEDATA _System xfTP_wpQueryDriveData(WPFileSystem *somSelf);
        typedef xfTP_wpQueryDriveData *xfTD_wpQueryDriveData;

        PWPSDRIVEDATA fsysQueryDriveData(WPFileSystem *somSelf);

    #endif

    /* ******************************************************************
     *
     *   Populate / refresh
     *
     ********************************************************************/

    /*
     *@@ FDATETIME:
     *
     *@@added V0.9.16 (2001-10-28) [umoeller]
     */

    #pragma pack(2)
    typedef struct _FDATETIME
    {
        FDATE       Date;
        FTIME       Time;
    } FDATETIME, *PFDATETIME;
    #pragma pack()

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
     *      This also gets passed to WPFileSystem::wpRestoreState
     *      in the ulReserved parameter. ;-)
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

    VOID fsysCreateStandardGEAList(VOID);

    // buffer size for DosFindFirst
    #define FINDBUFSIZE             0x10000     // 64K

    APIRET fsysCreateFindBuffer(PEAOP2 *pp);

    PBYTE fsysFindEAValue(PFEA2LIST pFEA2List2,
                          PCSZ pcszEAName,
                          PUSHORT pcbValue);

    #ifdef SOM_WPFolder_h
        BOOL fsysPopulateWithFSObjects(WPFolder *somSelf,
                                       PCSZ pcszFolderFullPath,
                                       BOOL fFoldersOnly,
                                       PCSZ pcszFileMask,
                                       PBOOL pfExit);
    #endif

    APIRET fsysRefresh(WPFileSystem *somSelf,
                       PVOID pvReserved);

    /* ******************************************************************
     *
     *   "File" pages replacement in WPDataFile/WPFolder
     *
     ********************************************************************/

    #ifdef NOTEBOOK_HEADER_INCLUDED
        VOID XWPENTRY fsysFile1InitPage(PCREATENOTEBOOKPAGE pcnbp,
                                        ULONG flFlags);

        MRESULT XWPENTRY fsysFile1ItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                     ULONG ulItemID,
                                     USHORT usNotifyCode,
                                     ULONG ulExtra);

#ifndef __NOFILEPAGE2__
        VOID XWPENTRY fsysFile2InitPage(PCREATENOTEBOOKPAGE pcnbp,
                                        ULONG flFlags);

        MRESULT XWPENTRY fsysFile2ItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                     ULONG ulItemID,
                                     USHORT usNotifyCode,
                                     ULONG ulExtra);
#endif

        ULONG fsysInsertFilePages(WPObject *somSelf,
                                  HWND hwndNotebook);

    #endif

#endif


