
/*
 *@@sourcefile disk.h:
 *      header file for disk.c (XFldDisk implementation).
 *
 *      This file is ALL new with V0.9.0.
 *
 *@@include #define INCL_DOSDEVIOCTL
 *@@include #include <os2.h>
 *@@include #include "shared\notebook.h"
 *@@include #include "filesys\disk.h"
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

#ifndef DISK_HEADER_INCLUDED
    #define DISK_HEADER_INCLUDED

    /* ******************************************************************
     *
     *   Disk implementation
     *
     ********************************************************************/

    #ifdef SOM_WPFolder_h
        WPFolder* dskCheckDriveReady(WPDisk *somSelf);
    #endif

    #ifdef INCL_DOSDEVIOCTL

        #define DFL_REMOTE                      0x0001
        #define DFL_FIXED                       0x0002
        #define DFL_PARTITIONABLEREMOVEABLE     0x0004
        #define DFL_CDROM                       0x0010

        #define DFL_SUPPORTS_EAS                0x1000
        #define DFL_SUPPORTS_LONGNAMES          0x2000
        #define DFL_BOOTDRIVE                   0x4000

        /*
         *@@ XDISKINFO:
         *
         *@@added V0.9.14 (2001-08-01) [umoeller]
         */

        typedef struct _XDISKINFO
        {
            CHAR        cDriveLetter;           // drive letter
            CHAR        cLogicalDrive;          // logical drive no.

            APIRET      arc;
                            // error code accessing this drive; if
                            // NO_ERROR, the following fields are valid

            ULONG       flType;
                            // any combination of the following:
                            // --  DFL_REMOTE: drive is remote (not local)
                            // --  DFL_FIXED: drive is fixed; otherwise
                            //          it is removeable!
                            // --  DFL_ISCDROM: drive is a CD-ROM. DFL_FIXED
                            //          should not be set in this case.
                            // --  DFL_PARTITIONABLEREMOVEABLE
                            // --  DFL_SUPPORTS_EAS
                            // --  DFL_SUPPORTS_LONGNAMES
                            // --  DFL_BOOTDRIVE

            BIOSPARAMETERBLOCK bpb;

            CHAR        szFileSystem[30];

        } XDISKINFO, *PXDISKINFO;

        BOOL APIENTRY dskQueryInfo(PXDISKINFO paDiskInfos,
                                   ULONG ulLogicalDrive);
        typedef BOOL APIENTRY DSKQUERYINFO(PXDISKINFO paDiskInfos,
                                           ULONG ulLogicalDrive);
        typedef DSKQUERYINFO *PDSKQUERYINFO;

    #endif

    /* ******************************************************************
     *
     *   XFldDisk notebook callbacks (notebook.c)
     *
     ********************************************************************/

    #ifdef NOTEBOOK_HEADER_INCLUDED
        VOID XWPENTRY dskDetailsInitPage(PCREATENOTEBOOKPAGE pcnbp,
                                         ULONG flFlags);

        MRESULT XWPENTRY dskDetailsItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                      ULONG ulItemID,
                                      USHORT usNotifyCode,
                                      ULONG ulExtra);
    #endif

#endif


