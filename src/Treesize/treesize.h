
/*
 * treesize.h:
 *      header file for treesize.h.
 *
 *      Starting with XWorkplace 0.9.0, this file no longer contains
 *      the ID's for Treesize's resources. These have been moved
 *      to include\dlgids.h to allow for NLS.
 *
 *      Copyright (C) 1997-2000 Ulrich M”ller.
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, in version 2 as it comes in the COPYING
 *      file of the XWorkplace main distribution.
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 */

#define SV_SIZE             1
#define SV_EASIZE           2
#define SV_NAME             3
#define SV_FILESCOUNT       4

#define SD_BYTES            1
#define SD_KBYTES           2
#define SD_MBYTES           3

typedef struct _DIRINFO
{
    struct _DIRINFO     *pParent;
    CHAR                szFullPath[CCHMAXPATH]; // "F:\OS2"
    CHAR                szThis[CCHMAXPATH];     // "OS2"
    CHAR                szRecordText[CCHMAXPATH+50];          // what appears in the cnr
    ULONG               ulFiles;
    double              dTotalSize;
    double              dTotalEASize;
    ULONG               ulRecursionLevel;       // 1 for root level
    PRECORDCORE         precc;      // PSIZERECORDCORE actually
} DIRINFO, *PDIRINFO;

/* extended RECORDCORE structure */
typedef struct _SIZERECORDCORE {
    RECORDCORE     recc;
    PDIRINFO       pdi;
    BOOL           fDisplayValid;        // TRUE only if display is valid
} SIZERECORDCORE, *PSIZERECORDCORE;

#define TSM_START            (WM_USER)
#define TSM_BEGINDIRECTORY   (WM_USER+1)
#define TSM_DONEDIRECTORY    (WM_USER+2)
#define TSM_DONEWITHALL      (WM_USER+3)



