
/*
 *@@sourcefile icons.c:
 *      implementation code for object icon handling.
 *
 *      This file is ALL new with V0.9.16.
 *
 *      The WPS icon methods are quite a mess. Basically,
 *      for almost each icon method, there are two
 *      versions (query and set), and within each version,
 *      there is a "query" method and a "query data"
 *      method, respectively.
 *
 *      What these methods do isn't clearly explained
 *      anywhere. Here is what I found out, and what I
 *      think the typical flow of execution is when
 *      objects are made awake by the WPS (usually during
 *      folder populate).
 *
 *      1)  Each object stores its icon in its MINIRECORDCODE
 *          (as returned by WPObject::wpQueryCoreRecord), in
 *          the hptrIcon field. When the object is created,
 *          this field is initially set to NULLHANDLE.
 *
 *      2)  Whether the icon handle is resolved during object
 *          instantion, usually during wpRestoreState, depends
 *          on the object's class.
 *
 *          --  For file-system objects, the HPOINTER is only
 *              created during wpRestoreState if the object
 *              has an .ICON EA. The reason for this is speed;
 *              wpPopulate already retrieves the .ICON EA data
 *              during DosFindFirst/Next, and this is passed
 *              to wpclsMakeAwake and then wpRestoreState in
 *              the "ulReserved" parameter that is not documented
 *              in WPSREF. Really, this is a pointer to the
 *              FILEFINDBUF3 and the FEA2LIST that was filled
 *              by wpPopulate.
 *
 *              Since the data is already present, in that case,
 *              the WPS builds that data already during object
 *              instantiation.
 *
 *          --  In all other cases (non-file-system objects or
 *              or no .ICON EA present), I believe the hptrIcon
 *              field is still NULLHANDLE.
 *
 *      3)  Now, the entry point into the icon mess is
 *          WPObject::wpQueryIcon. This gets called by the WPS's
 *          container owner-draw routines. As a result, the method
 *          only gets called if the icon is actually visible; for
 *          example, in a crowded folder where many objects are
 *          initially outside the visible viewport rectangle,
 *          wpQueryIcon doesn't get called until the container is
 *          scrolled. (This is why you get a noticeable delay when
 *          first scrolling through a folder with many file-system
 *          objects.)
 *
 *          wpQueryIcon returns the hptrIcon from the MINIRECORDCORE,
 *          if it was set. If the field is still NULLHANDLE (because
 *          the data wasn't built during wpRestoreState, see above),
 *          the WPS _then_ builds a proper icon.
 *
 *          What happens then again depends on the object's class:
 *
 *          --  For WPDataFile, the WPS calls
 *              WPDataFile::wpSetAssociatedFileIcon to have the
 *              icon of the associated program object set for
 *              the data file too.
 *
 *          --  For WPDisk, WPDisk::wpSetCorrectDiskIcon is called
 *              to set the icon to one of the hard disk, CD-ROM
 *              or floppy icons.
 *
 *          --  For WPProgramFile, WPProgramFile::wpSetProgIcon
 *              attempts to load an icon from the executable file.
 *
 *          If all of this fails, or for other classes, the WPS
 *          gets the class's default icon from wpclsQueryIcon.
 *
 *      Function prefix for this file:
 *      --  ico*
 *
 *@@added V0.9.V0.9.16 (2001-10-01) [umoeller]
 *@@header "filesys\icons.h"
 */

/*
 *      Copyright (C) 2001 Ulrich M”ller.
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

#pragma strings(readonly)

/*
 *  Suggested #include order:
 *  1)  os2.h
 *  2)  C library headers
 *  3)  setup.h (code generation and debugging options)
 *  4)  headers in helpers\
 *  5)  at least one SOM implementation header (*.ih)
 *  6)  dlgids.h, headers in shared\ (as needed)
 *  7)  headers in implementation dirs (e.g. filesys\, as needed)
 *  8)  #pragma hdrstop and then more SOM headers which crash with precompiled headers
 */

#define INCL_DOSPROCESS
#define INCL_DOSEXCEPTIONS
#define INCL_DOSSEMAPHORES
#define INCL_DOSRESOURCES
#define INCL_DOSMODULEMGR
#define INCL_DOSMISC
#define INCL_DOSERRORS

#define INCL_WININPUT
#define INCL_WINWINDOWMGR
#define INCL_WINMESSAGEMGR
#define INCL_WINPOINTERS
#define INCL_WINSYS
#define INCL_WINTIMER
#define INCL_WINMENUS
#define INCL_WINDIALOGS
#define INCL_WINSTATICS
#define INCL_WINBUTTONS
#define INCL_WINENTRYFIELDS
#define INCL_WINMLE
#define INCL_WINSTDCNR
#define INCL_WINSTDBOOK
#define INCL_WINPROGRAMLIST     // needed for wppgm.h
#define INCL_WINSHELLDATA
#include <os2.h>

// C library headers
#include <stdio.h>
// #include <malloc.h>
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\apps.h"               // application helpers
#include "helpers\comctl.h"             // common controls (window procs)
#include "helpers\dialog.h"             // dialog helpers
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\gpih.h"               // GPI helper routines
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\standards.h"          // some standard macros
#include "helpers\stringh.h"            // string helper routines
#include "helpers\tree.h"               // red-black binary trees
#include "helpers\winh.h"               // PM helper routines
#include "helpers\xstring.h"            // extended string helpers

// SOM headers which don't crash with prec. header files
#include "xfldr.ih"
#include "xfobj.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\cnrsort.h"             // container sort comparison functions
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\helppanels.h"          // all XWorkplace help panel IDs
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

// headers in /hook
#include "hook\xwphook.h"

#include "filesys\filesys.h"            // various file-system object implementation code
#include "filesys\icons.h"              // icons handling
#include "filesys\object.h"             // XFldObject implementation

#include "config\hookintf.h"            // daemon/hook interface

// other SOM headers
#include "helpers\undoc.h"              // some undocumented stuff
#pragma hdrstop
#include <wpshadow.h>                   // WPShadow

/* ******************************************************************
 *
 *   Icon data handling
 *
 ********************************************************************/

typedef HPOINTER APIENTRY WINBUILDPTRHANDLE(PBYTE pbData);
typedef WINBUILDPTRHANDLE *PWINBUILDPTRHANDLE;

/*
 *@@ icoBuildPtrHandle:
 *
 *@@added V0.9.16 (2001-12-08) [umoeller]
 */

APIRET icoBuildPtrHandle(PBYTE pbData,
                         HPOINTER *phptr)
{
    APIRET      arc = NO_ERROR;
    HPOINTER    hptr = NULLHANDLE;

    static  PWINBUILDPTRHANDLE WinBuildPtrHandle = NULL;
    BOOL    fLocked = FALSE;

    TRY_LOUD(excpt1)
    {
        if (fLocked = krnLock(__FILE__, __LINE__, __FUNCTION__))
        {
            if (!WinBuildPtrHandle)
            {
                // first call:
                HMODULE hmod;
                if (!(arc = DosQueryModuleHandle("PMMERGE",
                                                 &hmod)))
                    arc = DosQueryProcAddr(hmod,
                                           5117,        // WinBuildPtrHandle (32-bit)
                                           NULL,
                                           (PFN*)&WinBuildPtrHandle);

                if (arc)
                    cmnLog(__FILE__, __LINE__, __FUNCTION__,
                           "Error %d resolving WinBuildPtrHandle.");
            }

            krnUnlock();
            fLocked = FALSE;
        }

        if ( (!arc) && (!WinBuildPtrHandle))
            arc = ERROR_INVALID_ORDINAL;

        if (!arc)
            if (!(*phptr = WinBuildPtrHandle(pbData)))
                arc = ICONERR_BUILDPTR_FAILED;
    }
    CATCH(excpt1)
    {
        arc = ERROR_PROTECTION_VIOLATION;
    } END_CATCH();

    if (fLocked)
        krnUnlock();

    return (arc);
}

/*
 *@@ icoLoadICOFile:
 *      attempts to load the specified .ICO file
 *      and turn it into a new HPOINTER.
 *
 *      If NO_ERROR is returned, *hptr receives
 *      a newly allocated pointer handle.
 *
 *      Otherwise this returns:
 *
 *      --
 *
 *      plus the error codes of DosQueryModuleHandle,
 *      DosQueryProcAddr.
 *
 *@@added V0.9.16 (2001-12-08) [umoeller]
 */

APIRET icoLoadICOFile(PCSZ pcszFilename,
                      HPOINTER *phptr)
{
    APIRET  arc = NO_ERROR;
    PXFILE  pxf = NULL;
    ULONG   cbFile = 0;

    PBYTE   pbData = NULL;

    TRY_LOUD(excpt1)
    {
        if (!(arc = doshOpen(pcszFilename,
                             XOPEN_READ_EXISTING | XOPEN_BINARY,
                             &cbFile,
                             &pxf)))
        {
            if (!cbFile)
                arc = ERROR_NO_DATA;
            else
            {
                if (!(pbData = malloc(cbFile)))
                    arc = ERROR_NOT_ENOUGH_MEMORY;
                else
                {
                    ULONG ulDummy;
                    if (!(arc = DosRead(pxf->hf,
                                        pbData,
                                        cbFile,
                                        &ulDummy)))
                    {
                        // use this undocumented PM API to build the pointer
                        // from the icon data directly, which is the
                        // BITMAPARRAYHEADER normally
                        if (!(arc = icoBuildPtrHandle(pbData,
                                                      phptr)))
                            /* _Pmpf((__FUNCTION__ ": built hptr %lX from %s",
                                   *phptr,
                                   pcszFilename))*/ ;
                    }
                }
            }
        }
    }
    CATCH(excpt1)
    {
        arc = ERROR_PROTECTION_VIOLATION;
    } END_CATCH();

    if (pbData)
        free(pbData);

    doshClose(&pxf);

    /* if (arc)
        _Pmpf((__FUNCTION__ ": returning %d for %s",
               arc,
               pcszFilename));
       */
    return (arc);
}

/*
 *@@ icoBuildPtrFromFEA2List:
 *      builds a pointer from the .ICON data in
 *      the given FEA2LIST, if there's some.
 *
 *      Returns:
 *
 *      --  NO_ERROR: ptr was built.
 *
 *      --  ERROR_NO_DATA: no .ICON EA found.
 *
 *@@added V0.9.16 (2001-12-08) [umoeller]
 */

APIRET icoBuildPtrFromFEA2List(PFEA2LIST pFEA2List,
                               HPOINTER *phptr)
{
    APIRET arc = ERROR_NO_DATA;

    PBYTE pbValue;
    if (pbValue = fsysFindEAValue(pFEA2List,
                                  ".ICON",
                                  NULL))
    {
        // got something:
        PUSHORT pusType = (PUSHORT)pbValue;
        if (*pusType == EAT_ICON)
        {
            // next ushort has data length
            PUSHORT pcbValue = pusType + 1;
            if (*pcbValue)
            {
                PBYTE pbIconData = (PBYTE)(pcbValue + 1);
                arc = icoBuildPtrHandle(pbIconData,
                                        phptr);
            }
        }
    }

    return (arc);
}

// page flags (OBJECTPAGETABLEENTRY.o32_pageflags)
#define VALID           0x0000                // Valid Physical Page in .EXE
#define ITERDATA        0x0001                // Iterated Data Page
#define INVALID         0x0002                // Invalid Page
#define ZEROED          0x0003                // Zero Filled Page
#define RANGE           0x0004                // Range of pages
#define ITERDATA2       0x0005                // Iterated Data Page Type II

/*
 *@@ ExpandIterdata1:
 *      expands a page compressed with the old exepack
 *      method introduced with OS/2 2.0 (plain /EXEPACK).
 *
 *      (C) Knut Stange Osmundsen. Used with permission.
 *
 *@@added V0.9.16 (2001-12-08) [umoeller]
 */

APIRET ExpandIterdata1(char *pabTarget,         // out: page data (pagesize as in lx spec)
                       int cbTarget,            // in: sizeof *pabTarget (pagesize as in lx spec)
                       const char *pabSource,   // in: compressed source data in EXEPACK:1 format
                       int cbSource)            // in: sizeof *pabSource
{
    PLXITER             pIter = (PLXITER)pabSource;
    // store the pointer for boundary checking
    char                *pabTargetOriginal = pabTarget;

    // validate size of data
    if (cbSource >= cbTarget - 2)
        return (ICONERR_EXPANDPAGE1_TOOSMALL);

    // expand the page
    while (    (pIter->LX_nIter)
            && (cbSource > 0)
          )
    {
        // check if we're out of bound
        if (    pabTarget - pabTargetOriginal + pIter->LX_nIter * pIter->LX_nBytes > cbTarget
             || cbSource <= 0
           )
            return ICONERR_EXPANDPAGE1_OUTOFBOUND;

        if (pIter->LX_nBytes == 1)
        {
            // one databyte
            memset(pabTarget, pIter->LX_Iterdata, pIter->LX_nIter);
            pabTarget += pIter->LX_nIter;
            cbSource -= 4 + 1;
            pIter++;
        }
        else
        {
            int i;
            for (i = pIter->LX_nIter;
                 i > 0;
                 i--, pabTarget += pIter->LX_nBytes)
                memcpy(pabTarget, &pIter->LX_Iterdata, pIter->LX_nBytes);
            cbSource -= 4 + pIter->LX_nBytes;
            pIter   = (PLXITER)((char*)pIter + 4 + pIter->LX_nBytes);
        }
    }

    // zero remaining part of the page
    if (pabTarget - pabTargetOriginal < cbTarget)
        memset(pabTarget, 0, cbTarget - (pabTarget - pabTargetOriginal));

    return NO_ERROR;
}

/*
 *@@ memcpyw:
 *      a special memcpy for expandPage2 which performs a
 *      word based copy. The difference between this, memmove
 *      and memcpy is that we'll allways read words.
 *
 *      (C) Knut Stange Osmundsen. Used with permission.
 *
 *@@added V0.9.16 (2001-12-08) [umoeller]
 */

void memcpyw(char *pch1, const char *pch2, size_t cch)
{
    /*
     * Use memcpy if possible.
     */
    if ((pch2 > pch1 ? pch2 - pch1 : pch1 - pch2) >= 4)
    {
        memcpy(pch1, pch2, cch);        /* BUGBUG! ASSUMES that memcpy move NO more than 4 bytes at the time! */
        return;
    }

    /*
     * Difference is less than 3 bytes.
     */
    if (cch & 1)
        *pch1++ = *pch2++;

    for (cch >>= 1;
         cch > 0;
         cch--, pch1 += 2, pch2 += 2)
        *(PUSHORT)pch1 = *(PUSHORT)pch2;
}

/*
 *@@ memcpyb:
 *      a special memcpy for expandPage2 which performs a memmove
 *      operation. The difference between this and memmove is that
 *      this one works.
 *
 *      (C) Knut Stange Osmundsen. Used with permission.
 *
 *@@added V0.9.16 (2001-12-08) [umoeller]
 */

void memcpyb(char *pch1, const char *pch2, size_t cch)
{
    /*
     * Use memcpy if possible.
     */
    if ((pch2 > pch1 ? pch2 - pch1 : pch1 - pch2) >= 4)
    {
        memcpy(pch1, pch2, cch);        /* BUGBUG! ASSUMES that memcpy move NO more than 4 bytes at the time! */
        return;
    }

    /*
     * Difference is less than 3 bytes.
     */
    while(cch--)
        *pch1++ = *pch2++;
}

/*
 *@@ ExpandIterdata2:
 *      expands a page compressed with the new exepack
 *      method introduced with OS/2 Warp 3.0 (/EXEPACK:2).
 *
 *      (C) Knut Stange Osmundsen. Used with permission.
 *
 *@@added V0.9.16 (2001-12-08) [umoeller]
 */

int ExpandIterdata2(char *pachPage,
                    int cchPage,
                    const char *pachSrcPage,
                    int cchSrcPage)
{
    char *          pachDestPage = pachPage; /* Store the pointer for boundrary checking. */

    while (cchSrcPage > 0)
    {
        /*
         * Bit 0 and 1 is the encoding type.
         */

        char cSrc = *pachSrcPage;

        switch (cSrc & 0x03)
        {
            /*
             *
             *  0  1  2  3  4  5  6  7
             *  type  |              |
             *        ----------------
             *             cch        <cch bytes of data>
             *
             * Bits 2-7 is, if not zero, the length of an uncompressed run
             *   starting at the following byte.
             *
             *  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23
             *  type  |              |  |                    | |                     |
             *        ----------------  ---------------------- -----------------------
             *             zero                 cch                char to multiply
             *
             * If the bits are zero, the following two bytes describes a
             *   1 byte interation run. First byte is count, second is the byte to copy.
             *   A count of zero is means end of data, and we simply stops. In that case
             *   the rest of the data should be zero.
             */

            case 0:
            {
                if (cSrc)
                {
                    int cch = cSrc >> 2;
                    if (!(    (cchPage  < cch)
                           || (cchSrcPage < cch + 1)
                         )
                       )
                    {
                        memcpy(pachPage, pachSrcPage+1, cch);
                        pachPage += cch, cchPage -= cch;
                        pachSrcPage += cch + 1, cchSrcPage -= cch + 1;
                        break;
                    }
                    return ICONERR_EXPANDPAGE2_BADDATA;
                }

                if (cchSrcPage >= 2)
                {
                    int cch;
                    if (cch = pachSrcPage[1])
                    {
                        if (!(    (cchSrcPage < 3)
                               || (cchPage < cch)
                             )
                           )
                        {
                            memset(pachPage, pachSrcPage[2], cch);
                            pachPage += cch, cchPage -= cch;
                            pachSrcPage += 3, cchSrcPage -= 3;
                            break;
                        }
                    }
                    else
                        goto endloop;
                }

                return ICONERR_EXPANDPAGE2_BADDATA;
            }


            /*
             *  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
             *  type  |  |  |     |  |                       |
             *        ----  -------  -------------------------
             *        cch1  cch2 - 3          offset            <cch1 bytes of data>
             *
             *  Two bytes layed out as described above, followed by cch1 bytes of data to be copied.
             *  The cch2(+3) and offset describes an amount of data to be copied from the expanded
             *    data relative to the current position. The data copied as you would expect it to be.
             */

            case 1:
            {
                if (cchSrcPage >= 2)
                {
                    int off = *(PUSHORT)pachSrcPage >> 7;
                    int cch1 = cSrc >> 2 & 3;
                    int cch2 = (cSrc >> 4 & 7) + 3;
                    pachSrcPage += 2, cchSrcPage -= 2;
                    if (!(    (cchSrcPage < cch1)
                           || (cchPage < cch1 + cch2)
                           || (pachPage + cch1 - off < pachDestPage)
                         )
                       )
                    {
                        memcpy(pachPage, pachSrcPage, cch1);
                        pachPage += cch1, cchPage -= cch1;
                        pachSrcPage += cch1, cchSrcPage -= cch1;
                        memcpyb(pachPage, pachPage - off, cch2); //memmove doesn't do a good job here for some stupid reason.
                        pachPage += cch2, cchPage -= cch2;
                        break;
                    }
                }
                return ICONERR_EXPANDPAGE2_BADDATA;
            }

            /*
             *  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
             *  type  |  |  |                                |
             *        ----  ----------------------------------
             *       cch-3              offset
             *
             *  Two bytes layed out as described above.
             *  The cch(+3) and offset describes an amount of data to be copied from the expanded
             *  data relative to the current position.
             *
             *  If offset == 1 the data is not copied as expected, but in the memcpyw manner.
             */

            case 2:
            {
                if (cchSrcPage >= 2)
                {
                    int off = *(PUSHORT)pachSrcPage >> 4;
                    int cch = (cSrc >> 2 & 3) + 3;
                    pachSrcPage += 2, cchSrcPage -= 2;
                    if (!(    (cchPage < cch)
                           || (pachPage - off < pachDestPage)
                         )
                       )
                    {
                        memcpyw(pachPage, pachPage - off, cch);
                        pachPage += cch, cchPage -= cch;
                        break;
                    }
                }
                return ICONERR_EXPANDPAGE2_BADDATA;
            }


            /*
             *  0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 19 20 21 22 23
             *  type  |        |  |              |  |                                |
             *        ----------  ----------------  ----------------------------------
             *           cch1           cch2                     offset                <cch1 bytes of data>
             *
             *  Three bytes layed out as described above, followed by cch1 bytes of data to be copied.
             *  The cch2 and offset describes an amount of data to be copied from the expanded
             *  data relative to the current position.
             *
             *  If offset == 1 the data is not copied as expected, but in the memcpyw manner.
             */

            case 3:
            {
                if (cchSrcPage >= 3)
                {
                    int cch1 = cSrc >> 2 & 0x000f;
                    int cch2 = *(PUSHORT)pachSrcPage >> 6 & 0x003f;
                    int off  = *(PUSHORT)(pachSrcPage+1) >> 4;
                    pachSrcPage += 3, cchSrcPage -= 3;
                    if (!(    (cchSrcPage < cch1)
                           || (cchPage < cch1 + cch2)
                           || (pachPage - off + cch1 < pachDestPage)
                         )
                       )
                    {
                        memcpy(pachPage, pachSrcPage, cch1);
                        pachPage += cch1, cchPage -= cch1;
                        pachSrcPage += cch1, cchSrcPage -= cch1;
                        memcpyw(pachPage, pachPage - off, cch2);
                        pachPage += cch2, cchPage -= cch2;
                        break;
                    }
                }
                return ICONERR_EXPANDPAGE2_BADDATA;
            }
        }
    }

endloop:;


    /*
     * Zero the rest of the page.
     */
    if (cchPage > 0)
        memset(pachPage, 0, cchPage);

    return 0;
}

/*
 *@@ GetOfsFromPageTableIndex:
 *
 *@@added V0.9.16 (2001-12-08) [umoeller]
 */

APIRET GetOfsFromPageTableIndex(PEXECUTABLE pExec,     // in: executable from doshExecOpen
                                ULONG ulObjPageTblIndexThis,
                                PULONG pulFlags,
                                PULONG pulSize,
                                PULONG pulPageOfs)      // out: page ofs (add pLXHeader->ulDataPagesOfs to this)
{
    OBJECTPAGETABLEENTRY *pObjPageTblEntry;

    PLXHEADER       pLXHeader = pExec->pLXHeader;

    if (ulObjPageTblIndexThis - 1 >= pLXHeader->ulPageCount)
        return ICONERR_INVALID_OFFSET;

    pObjPageTblEntry = &pExec->pObjPageTbl[ulObjPageTblIndexThis - 1];

    // page offset: shift left by what was specified in LX header
    *pulPageOfs     =    pObjPageTblEntry->o32_pagedataoffset
                      << pLXHeader->ulPageLeftShift;
    *pulFlags       = pObjPageTblEntry->o32_pageflags;
    *pulSize        = pObjPageTblEntry->o32_pagesize;

    return NO_ERROR;
}


/*
 *@@ LoadCompressedResourcePages:
 *
 *@@added V0.9.16 (2001-12-08) [umoeller]
 */

APIRET LoadCompressedResourcePages(PEXECUTABLE pExec,     // in: executable from doshExecOpen
                                   RESOURCETABLEENTRY *pRsEntry,  // in: resource table entry for this res
                                   ULONG ulType,          // in: page type of first page
                                   PBYTE *ppbResData)    // out: icon data (use DosFreeMem)
{
    APIRET  arc = NO_ERROR;
    PBYTE   pabCompressed = NULL;

    #define PAGESHIFT  12

    TRY_LOUD(excpt1)
    {
        PLXHEADER       pLXHeader = pExec->pLXHeader;

        // alloc buffer for compressed data from disk
        if (!(pabCompressed = malloc(pLXHeader->ulPageSize + 4)))
            arc = ERROR_NOT_ENOUGH_MEMORY;
        else
        {
            ULONG   ul,
                    cPages,
                    cbAlloc;

            OBJECTTABLEENTRY *pObjTblEntry = &pExec->pObjTbl[pRsEntry->obj - 1];

            ULONG   ulObjPageTblIndex =   pObjTblEntry->o32_pagemap
                                        + (pRsEntry->offset >> PAGESHIFT);

            cPages =   (pRsEntry->cb + pLXHeader->ulPageSize - 1)
                     / pLXHeader->ulPageSize;

            if (!(arc = doshAllocArray(cPages,
                                       pLXHeader->ulPageSize,
                                       ppbResData,
                                       &cbAlloc)))
            {
                // current output pointer: start with head of
                // buffer, this is advanced by the page size
                // for each page that was decompressed
                PBYTE pbCurrent = *ppbResData;

                for (ul = 0;
                     ul < cPages;
                     ul++)
                {
                    ULONG   ulFlags2,       // page flags... we ignore this and use
                                            // the flags from the first page always
                                            // (given to us in ulType)
                            ulSize,         // size of this page (and bytes read)
                            ulOffset;       // offset of this page in exe

                    if (!(arc = GetOfsFromPageTableIndex(pExec,
                                                         ulObjPageTblIndex + ul,
                                                         &ulFlags2,
                                                         &ulSize,
                                                         &ulOffset)))
                    {
                        // resources are in data pages
                        ulOffset += pLXHeader->ulDataPagesOfs;

                        // now go read this compressed page
                        if (!(arc = doshReadAt(pExec->hfExe,
                                               ulOffset,
                                               FILE_BEGIN,
                                               &ulSize,
                                               pabCompressed)))
                        {
                            // terminate the buf for decompress
                            memset(pabCompressed + ulSize,
                                   0,
                                   4);

                            if (ulType == ITERDATA)
                                // OS/2 2.x:
                                arc = ExpandIterdata1(pbCurrent,
                                                      pLXHeader->ulPageSize,
                                                      pabCompressed,
                                                      ulSize);            // this page's size
                            else
                                // Warp 3:
                                arc = ExpandIterdata2(pbCurrent,
                                                      pLXHeader->ulPageSize,
                                                      pabCompressed,
                                                      ulSize);            // this page's size

                            if (!arc)
                                pbCurrent += pLXHeader->ulPageSize;
                            else
                                break;
                        }
                        else
                            break;
                    }
                    else
                        break;

                } // end for

                if (arc)
                    free(*ppbResData);

            } // end if (!(*ppbResData = malloc(...
        }
    }
    CATCH(excpt1)
    {
        arc = ERROR_PROTECTION_VIOLATION;
    } END_CATCH();

    _Pmpf((__FUNCTION__ ": returning %d", arc));

    if (pabCompressed)
        free(pabCompressed);

    return (arc);
}

/*
 *@@ LoadLXResource:
 *      attempts to load the data of the resource
 *      with the specified type and id from an LX
 *      executable.
 *
 *      If idResource == 0, the first resource of
 *      the specified type is loaded.
 *
 *      If NO_ERROR is returned, *ppbResData receives
 *      a new buffer with the icon data. The caller
 *      must free() that buffer.
 *
 *      Otherwise this returns:
 *
 *      --  ERROR_NO_DATA: resource not found.
 *
 *      --  ERROR_BAD_FORMAT: cannot handle resource format.
 *
 *@@added V0.9.16 (2001-12-08) [umoeller]
 */

APIRET LoadLXResource(PEXECUTABLE pExec,     // in: executable from doshExecOpen
                      ULONG ulType,          // in: RT_* type (e.g. RT_POINTER)
                      ULONG idResource,      // in: resource ID or 0 for first
                      PBYTE *ppbResData)     // out: icon data (to be free()'d)
{
    APIRET          arc = NO_ERROR;
    ULONG           cResources = 0;

    ULONG           ulNewHeaderOfs = 0; // V0.9.12 (2001-05-03) [umoeller]

    PLXHEADER       pLXHeader;

    if (!(pLXHeader = pExec->pLXHeader))
        return (ERROR_INVALID_EXE_SIGNATURE);

    if (pExec->pDosExeHeader)
        // executable has DOS stub: V0.9.12 (2001-05-03) [umoeller]
        ulNewHeaderOfs = pExec->pDosExeHeader->ulNewHeaderOfs;

    if (!(cResources = pLXHeader->ulResTblCnt))
        // no resources at all:
        return (ERROR_NO_DATA);

    if (!pExec->fLXMapsLoaded)
        arc = doshLoadLXMaps(pExec);

    if (!arc)
    {
        // alright, we're in:

        // run thru the resources
        BOOL fPtrFound = FALSE;

        ULONG i;
        for (i = 0;
             i < cResources;
             i++)
        {
            // ptr to resource table entry
            RESOURCETABLEENTRY *pRsEntry = &pExec->pRsTbl[i];

            // check resource type and ID
            if (    (pRsEntry->type == ulType)
                 && (    (idResource == 0)
                      || (idResource == pRsEntry->name)
                    )
               )
            {
                // hooray

                // here comes the sick part...
                if (pRsEntry->obj - 1 >= pLXHeader->ulObjCount)
                    arc = ICONERR_INVALID_OFFSET;
                else
                {
                    // 1) get the object table entry from the object no. in
                    //    the resource tbl entry
                    OBJECTTABLEENTRY *pObjTblEntry = &pExec->pObjTbl[pRsEntry->obj - 1];
                    // 2) from that, get the object _page_ table index
                    ULONG ulObjPageTblIndex = pObjTblEntry->o32_pagemap;
                    ULONG ulFlags,
                          ulSize,
                          ulOffset;

                    if (!(arc = GetOfsFromPageTableIndex(pExec,
                                                         ulObjPageTblIndex,
                                                         &ulFlags,
                                                         &ulSize,
                                                         &ulOffset)))
                    {
                        ulOffset +=   pLXHeader->ulDataPagesOfs
                                    + pRsEntry->offset;

                        _Pmpf(("  found RT_POINTER %d, ofs %d, type %s",
                                pRsEntry->name,
                                ulOffset,
                                (ulFlags == 0x0001) ? "ITERDATA"
                                : (ulFlags == 0x0005) ? "ITERDATA2"
                                : "uncompressed"));

                        switch (ulFlags)
                        {
                            case ITERDATA:
                                // compressed in OS/2 2.x format:
                            case ITERDATA2:
                                if (!(arc = LoadCompressedResourcePages(pExec,
                                                                        pRsEntry,
                                                                        ulFlags,
                                                                        ppbResData)))
                                    fPtrFound = TRUE;
                            break;

                            case VALID:
                            {
                                // uncompressed
                                ULONG cb = pRsEntry->cb;        // resource size
                                PBYTE pb;
                                if (!(*ppbResData = malloc(cb)))
                                    arc = ERROR_NOT_ENOUGH_MEMORY;
                                else
                                {
                                    if (!(arc = doshReadAt(pExec->hfExe,
                                                           ulOffset,
                                                           FILE_BEGIN,
                                                           &cb,
                                                           *ppbResData)))
                                        fPtrFound = TRUE;
                                    else
                                        // error reading:
                                        free(*ppbResData);
                                }
                            }
                            break;
                        }
                    }
                }
            }

            if (fPtrFound || arc)
                break;

        } // end for

        if ((!fPtrFound) && (!arc))
            arc = ERROR_NO_DATA;
    }

    return (arc);
}

/*
 *@@ LoadOS2NEResource:
 *      attempts to load the data of the resource
 *      with the specified type and id from an OS/2
 *      NE executable.
 *
 *      Note that NE executables with resources in
 *      OS/2 format are very, very rare. The
 *      only OS/2 NE executables with resources I
 *      could find at this point were in an old 1.3
 *      Toolkit, but with them, this code works.
 *
 *      If idResource == 0, the first resource of
 *      the specified type is loaded.
 *
 *      If NO_ERROR is returned, *ppbResData receives
 *      a new buffer with the icon data. The caller
 *      must free() that buffer.
 *
 *      Otherwise this returns:
 *
 *      --  ERROR_NO_DATA: resource not found.
 *
 *      --  ERROR_BAD_FORMAT: cannot handle resource format.
 *
 *@@added V0.9.16 (2001-12-08) [umoeller]
 */

APIRET LoadOS2NEResource(PEXECUTABLE pExec,     // in: executable from doshExecOpen
                         ULONG ulType,          // in: RT_* type (e.g. RT_POINTER)
                         ULONG idResource,      // in: resource ID or 0 for first
                         PBYTE *ppbResData)     // out: icon data (to be free()'d)
{
    APIRET          arc = NO_ERROR;
    ULONG           cResources = 0;

    ULONG           ulNewHeaderOfs = 0; // V0.9.12 (2001-05-03) [umoeller]

    PNEHEADER       pNEHeader;

    if (!(pNEHeader = pExec->pNEHeader))
        return (ERROR_INVALID_EXE_SIGNATURE);

    if (pExec->pDosExeHeader)
        // executable has DOS stub: V0.9.12 (2001-05-03) [umoeller]
        ulNewHeaderOfs = pExec->pDosExeHeader->ulNewHeaderOfs;

    _Pmpf((__FUNCTION__ ": entering, checking %d resources", pNEHeader->usResSegmCount));

    if (!(cResources = pNEHeader->usResSegmCount))
        // no resources at all:
        return (ERROR_NO_DATA);

    if (!pExec->fOS2NEMapsLoaded)
        arc = doshLoadOS2NEMaps(pExec);

    if (!arc)
    {
        // alright, we're in:

        // run thru the resources
        BOOL fPtrFound = FALSE;

        ULONG i;
        POS2NERESTBLENTRY pResTblEntryThis = pExec->paOS2NEResTblEntry;
        POS2NESEGMENT pSegThis = pExec->paOS2NESegments;
        for (i = 0;
             i < cResources;
             i++, pResTblEntryThis++, pSegThis++)
        {
            // check resource type and ID
            if (    (pResTblEntryThis->usType == ulType)
                 && (    (idResource == 0)
                      || (idResource == pResTblEntryThis->usID)
                    )
               )
            {
                // hooray, we found the resource...

                // look up the corresponding segment

                ULONG ulOffset = (    (ULONG)pSegThis->ns_sector
                                   << pNEHeader->usLogicalSectShift
                                 );

                ULONG cb = pSegThis->ns_cbseg;        // resource size
                PBYTE pb;
                if (!(*ppbResData = malloc(cb)))
                    arc = ERROR_NOT_ENOUGH_MEMORY;
                else
                {
                    if (!(arc = doshReadAt(pExec->hfExe,
                                           ulOffset,
                                           FILE_BEGIN,
                                           &cb,
                                           *ppbResData)))
                        fPtrFound = TRUE;
                    else
                        // error reading:
                        free(*ppbResData);
                }
            }

            if (fPtrFound || arc)
                break;

        } // end for

        if ((!fPtrFound) && (!arc))
            arc = ERROR_NO_DATA;
    }
    else
        _Pmpf(("doshLoadOS2NEMaps returned %d"));

    return (arc);
}

/*

Resource Table

The resource table describes and identifies the location of each resource in the
executable file.

Following are the members in the resource table:

                       struct new_rsrc
                           {
                           unsigned short rs_align;    // alignment shift count for resources
                           struct rsrc_typeinfo rs_typeinfo;
                           };

rscAlignShift   The alignment shift count for resource data. When the
                shift count is used as an exponent of 2, the resulting value
                specifies the factor, in bytes, for computing the location of
                a resource in the executable file.
rscTypes        An array of TTYPEINFO structures containing information
                about resource types. There must be one TTYPEINFO
                structure for each type of resource in the executable file.

                Following are the members in the TTYPEINFO structure:

                                         // Resource type information block
                                         struct rsrc_typeinfo
                                             {
                                             unsigned short rt_id;
                                             unsigned short rt_nres;
                                             long rt_proc;
                                             };

                rtTypeID    The type identifier of the resource. This integer value is either
                            a resource-type value or an offset to a resource-type name. If
                            the high bit in this member is set (0x8000), the value is one of
                            the following resource-type values:

                            Value   Resource type

                            RT_ACCELERATOR      Accelerator table
                            RT_BITMAP           Bitmap
                            RT_CURSOR           Cursor
                            RT_DIALOG           Dialog box
                            RT_FONT             Font component
                            RT_FONTDIR          Font directory
                            RT_GROUP_CURSOR     Cursor directory
                            RT_GROUP_ICON       Icon directory
                            RT_ICON             Icon
                            RT_MENU             Menu
                            RT_RCDATA           Resource data
                            RT_STRING           String table

                            If the high bit of the value in this member is not set, the value
                            represents an offset, in bytes relative to the beginning of the
                            resource table, to a name in the rscResourceNames
                            member.

                rtResourceCount The number of resources of this type in the executable file.
                rtReserved  Reserved.
                rtNameInfo  An array of TNAMEINFO structures containing information
                            about individual resources. The rtResourceCount member
                            specifies the number of structures in the array.

                Following are the members in the TNAMEINFO structure:

                                       // Resource name information block
                                       struct rsrc_nameinfo
                                           {
                                           // The following two fields must be shifted left by the value of
                                           // the rs_align field to compute their actual value.  This allows
                                           // resources to be larger than 64k, but they do not need to be
                                           // aligned on 512 byte boundaries, the way segments are
                                           unsigned short rn_offset;   // file offset to resource data
                                           unsigned short rn_length;   // length of resource data
                                           unsigned short rn_flags;    // resource flags
                                           unsigned short rn_id;       // resource name id
                                           unsigned short rn_handle;   // If loaded, then global handle
                                           unsigned short rn_usage;    // Initially zero.  Number of times
                                                                       // the handle for this resource has
                                                                       // been given out
                                           };

                rnOffset    An offset to the contents of the resource data (relative to the beginning
                            of the file). The offset is in terms of alignment units specified by the
                            rscAlignShift member at the beginning of the resource table.
                rnLength    The resource length, in bytes.
                rnFlags     Whether the resource is fixed, preloaded, or shareable. This member
                            can be one or more of the following values:

                            Value   Meaning

                            0x0010  Resource is movable (MOVEABLE). Otherwise, it is fixed.
                            0x0020  Resource can be shared (PURE).
                            0x0040  Resource is preloaded (PRELOAD). Otherwise, it is loaded
                                    on demand.

                rnID        Specifies or points to the resource identifier. If the identifier is an
                            integer, the high bit is set (8000h). Otherwise, it is an offset to a
                            resource string, relative to the beginning of the resource table.
                rnHandle    Reserved.
                rnUsage     Reserved.

rscEndTypes     The end of the resource type definitions. This member
                must be zero.
rscResourceNames    The names (if any) associated with the resources in this
                table. Each name is stored as consecutive bytes; the first
                byte specifies the number of characters in the name.
rscEndNames     The end of the resource names and the end of the
                resource table. This member must be zero.

*/

#define WINRT_ACCELERATOR          9
#define WINRT_BITMAP               2
#define WINRT_CURSOR               1
#define WINRT_DIALOG               5
#define WINRT_FONT                 8
#define WINRT_FONTDIR              7
#define WINRT_ICON                 3
#define WINRT_MENU                 4
#define WINRT_RCDATA               10
#define WINRT_STRING               6

PCSZ GetWinResourceTypeName(ULONG ulTypeThis)
{
    switch (ulTypeThis)
    {
        case WINRT_ACCELERATOR: return "WINRT_ACCELERATOR";
        case WINRT_BITMAP: return "WINRT_BITMAP";
        case WINRT_CURSOR: return "WINRT_CURSOR";
        case WINRT_DIALOG: return "WINRT_DIALOG";
        case WINRT_FONT: return "WINRT_FONT";
        case WINRT_FONTDIR: return "WINRT_FONTDIR";
        case WINRT_ICON: return "WINRT_ICON";
        case WINRT_MENU: return "WINRT_MENU";
        case WINRT_RCDATA: return "WINRT_RCDATA";
        case WINRT_STRING: return "WINRT_STRING";
    }

    return ("unknown");
}

/*
 *@@ LoadWinNEResource:
 *      attempts to load the data of the resource
 *      with the specified type and id from a Win16
 *      NE executable.
 *
 *      If idResource == 0, the first resource of
 *      the specified type is loaded.
 *
 *      If NO_ERROR is returned, *ppbResData receives
 *      a new buffer with the icon data. The caller
 *      must free() that buffer.
 *
 *      Otherwise this returns:
 *
 *      --  ERROR_NO_DATA: resource not found.
 *
 *      --  ERROR_BAD_FORMAT: cannot handle resource format.
 *
 *@@added V0.9.16 (2001-12-08) [umoeller]
 */

APIRET LoadWinNEResource(PEXECUTABLE pExec,     // in: executable from doshExecOpen
                         ULONG ulType,          // in: RT_* type (e.g. RT_POINTER)
                         ULONG idResource,      // in: resource ID or 0 for first
                         PBYTE *ppbResData)     // out: icon data (to be free()'d)
{
    APIRET          arc = NO_ERROR;
    ULONG           cb;

    ULONG           ulNewHeaderOfs = 0; // V0.9.12 (2001-05-03) [umoeller]

    PNEHEADER       pNEHeader;

    USHORT          usAlignShift;

    if (!(pNEHeader = pExec->pNEHeader))
        return (ERROR_INVALID_EXE_SIGNATURE);

    if (pExec->pDosExeHeader)
        // executable has DOS stub: V0.9.12 (2001-05-03) [umoeller]
        ulNewHeaderOfs = pExec->pDosExeHeader->ulNewHeaderOfs;

    // 1) res tbl starts with align leftshift
    cb = sizeof(usAlignShift);
    if (!(arc = doshReadAt(pExec->hfExe,
                           // start of res table
                           pNEHeader->usResTblOfs
                             + ulNewHeaderOfs,
                           FILE_BEGIN,
                           &cb,
                           (PBYTE)&usAlignShift)))
    {
        // run thru the resources
        BOOL fPtrFound = FALSE;

        while (!arc)
        {
            ULONG cbRead;

            #pragma pack(1)
            struct rsrc_typeinfo
            {
                unsigned short  rt_id;
                unsigned short  rt_nres;
                long            reserved;
            } typeinfo;
            #pragma pack()

            //    then array of typeinfo structures, one for each res type
            //    in the file; last one has byte 0 first
            //          rtTypeID (RT_* value for this table; if 0, end of table)
            //          count of res's of this type
            //          reserved
            //          array of nameinfo structures
            //              offset rel. to beginning of file (leftshift!)
            //              length
            //              flags
            //              id (int if 0x8000 set), otherwise offset to string
            //              reserved
            //              reserved
            if (!(arc = DosRead(pExec->hfExe,
                                &typeinfo,
                                sizeof(typeinfo),
                                &cbRead)))
            {
                if (    (cbRead < sizeof(typeinfo))
                     || (typeinfo.rt_id == 0)
                   )
                    // this was the last array item:
                    break;
                else
                {
                    // next comes array of nameinfo structures
                    // we know how many array items follow,
                    // so we can read them in one flush, or
                    // skip them if it's not what we're looking for

                    #pragma pack(1)
                    typedef struct rsrc_nameinfo
                    {
                        // The following two fields must be shifted left by the value of
                        // the rs_align field to compute their actual value.  This allows
                        // resources to be larger than 64k, but they do not need to be
                        // aligned on 512 byte boundaries, the way segments are
                        unsigned short rn_offset;   // file offset to resource data
                        unsigned short rn_length;   // length of resource data
                        unsigned short rn_flags;    // resource flags
                        unsigned short rn_id;       // resource name id
                        unsigned short rn_handle;   // If loaded, then global handle
                        unsigned short rn_usage;    // Initially zero.  Number of times
                                                    // the handle for this resource has
                                                    // been given out
                    } nameinfo;
                    #pragma pack()

                    ULONG ulTypeThis = typeinfo.rt_id & ~0x8000;

                    if (    (!(typeinfo.rt_id & 0x8000))
                         || (ulTypeThis != ulType)
                       )
                    {
                        _Pmpf((__FUNCTION__ ": skipping type %d (%s), %d entries",
                                      ulTypeThis,
                                      GetWinResourceTypeName(ulTypeThis),
                                      typeinfo.rt_nres));
                        arc = DosSetFilePtr(pExec->hfExe,
                                            typeinfo.rt_nres * sizeof(nameinfo),
                                            FILE_CURRENT,
                                            &cbRead);
                    }
                    else
                    {
                        // this is our type:
                        nameinfo *paNameInfos = NULL;
                        ULONG cbNameInfos;

                        _Pmpf((__FUNCTION__ ": entering type %d (%s), %d entries",
                                      ulTypeThis,
                                      GetWinResourceTypeName(ulTypeThis),
                                      typeinfo.rt_nres));

                        if (    (!(arc = doshAllocArray(typeinfo.rt_nres,
                                                        sizeof(nameinfo),
                                                        (PBYTE*)&paNameInfos,
                                                        &cbNameInfos)))
                             && (!(arc = DosRead(pExec->hfExe,
                                                 paNameInfos,
                                                 cbNameInfos,
                                                 &cbRead)))
                           )
                        {
                            ULONG ul;
                            nameinfo *pThis = paNameInfos;

                            if (cbRead < cbNameInfos)
                                arc = ERROR_BAD_FORMAT;
                            else
                            {
                                for (ul = 0;
                                     ul < typeinfo.rt_nres;
                                     ul++, pThis++)
                                {
                                    ULONG ulIDThis = pThis->rn_id;
                                    ULONG ulOffset = pThis->rn_offset << usAlignShift;
                                    ULONG cbThis =   pThis->rn_length << usAlignShift;
                                    _Pmpf(("   found res type %d, id %d, length %d",
                                                ulTypeThis,
                                                ulIDThis & ~0x8000,
                                                cbThis));

                                    if (    (!idResource)
                                         || (    (ulIDThis & 0x8000)
                                              && ((ulIDThis & ~0x8000) == idResource)
                                            )
                                       )
                                    {
                                        // found:
                                        PBYTE pb;
                                        if (!(*ppbResData = malloc(cbThis)))
                                            arc = ERROR_NOT_ENOUGH_MEMORY;
                                        else
                                        {
                                            if (!(arc = doshReadAt(pExec->hfExe,
                                                                   ulOffset,
                                                                   FILE_BEGIN,
                                                                   &cbThis,
                                                                   *ppbResData)))
                                                fPtrFound = TRUE;
                                            else
                                                // error reading:
                                                free(*ppbResData);
                                        }

                                        break;
                                    }
                                } // end for
                            }

                            if (arc)
                                break;
                        }

                        if (paNameInfos)
                            free(paNameInfos);

                        break;  // while
                    } // end if our type
                }
            }
        } // end while (!arc)

        if ((!fPtrFound) && (!arc))
            arc = ERROR_NO_DATA;
    }

    return (arc);
}

/*
 *@@ icoLoadExeIcon:
 *      smarter replacement for WinLoadFileIcon, which
 *      takes ages on PE executables.
 *
 *      Note that as opposed to WinLoadFileIcon, this
 *      is intended for executables _only_. This does
 *      not check for an .ICO file in the same directory.
 *
 *      Neither will this check for an .ICON EA because
 *      this is intended for XWPFileSystem::wpSetProgIcon
 *      which never gets called in the first place for
 *      that case.
 *
 *      Instead, this will "only" try to find a default
 *      icon in the executable's resources.
 *
 *      Also, if no icon resource could be found in the
 *      executable, this returns ERROR_NO_DATA instead
 *      of a default icon. It is then the responsibility
 *      of the caller to supply a default icon.
 *
 *      Otherwise this returns:
 *
 *      --  ERROR_INVALID_EXE_SIGNATURE: cannot handle
 *          this EXE format.
 *
 *      --  ERROR_NO_DATA: EXE format understood, but
 *          the EXE contains no RT_POINTER resources.
 *
 *      --  ERROR_NOT_ENOUGH_MEMORY
 *
 *      plus the error codes of doshExecOpen.
 *
 *@@added V0.9.16 (2001-12-08) [umoeller]
 */

APIRET icoLoadExeIcon(PEXECUTABLE pExec,
                      HPOINTER *phptr)
{
    APIRET arc;

    TRY_LOUD(excpt1)
    {
        PBYTE pbIconData = NULL;

        if (!pExec)
            return ERROR_INVALID_PARAMETER;

        // check the executable type
        switch (pExec->ulExeFormat)
        {
            case EXEFORMAT_LX:
                // these two we can handle for now
                arc = LoadLXResource(pExec,
                                     RT_POINTER,
                                     0,         // first one found
                                     &pbIconData);
            break;

            case EXEFORMAT_NE:
                switch (pExec->ulOS)
                {
                    case EXEOS_OS2:
                        arc = LoadOS2NEResource(pExec,
                                                RT_POINTER,
                                                0,         // first one found
                                                &pbIconData);
                        if (arc)
                            _Pmpf((__FUNCTION__ ": LoadOS2NEResource returned %d", arc));
                    break;

                    case EXEOS_WIN16:
                    case EXEOS_WIN386:
                        arc = LoadWinNEResource(pExec,
                                                WINRT_ICON,
                                                0,         // first one found
                                                &pbIconData);
                        if (arc)
                            _Pmpf((__FUNCTION__ ": LoadWinNEResource returned %d", arc));
                    break;

                    default:
                        arc = ERROR_INVALID_EXE_SIGNATURE;
                }
            break;

            case EXEFORMAT_PE:          // @@todo later

            case EXEFORMAT_OLDDOS:
            case EXEFORMAT_TEXT_BATCH:
            case EXEFORMAT_TEXT_REXX:

            default:
                arc = ERROR_INVALID_EXE_SIGNATURE;
            break;
        }

        if (!arc && pbIconData)
        {
            if (!(arc = icoBuildPtrHandle(pbIconData,
                                          phptr)))
                _Pmpf(("Built hptr %lX", *phptr));
            else
                _Pmpf(("icoBuildPtrHandle returned %d", arc));
            free(pbIconData);
        }
    }
    CATCH(excpt1)
    {
        arc = ERROR_PROTECTION_VIOLATION;
    } END_CATCH();

    if (arc)
        _Pmpf((__FUNCTION__ ": returning %d", arc));

    return (arc);
}

/* ******************************************************************
 *
 *   Object icon management
 *
 ********************************************************************/

/*
 *@@ icoQueryIconN:
 *      evil helper for calling wpQueryIcon
 *      or wpQueryIconN, which is undocumented.
 *
 *      Returns 0 on errors.
 *
 *@@added V0.9.16 (2001-10-15) [umoeller]
 */

HPOINTER icoQueryIconN(WPObject *pobj,    // in: object
                       ULONG ulIndex)     // in: animation index or 0 for regular icon
{
    if (!ulIndex)
        return (_wpQueryIcon(pobj));

    // index specified: this better be a folder, and we better be Warp 4
    if (    (doshIsWarp4())
         && (_somIsA(pobj, _WPFolder))
       )
    {
        xfTD_wpQueryIconN pwpQueryIconN;

        if (pwpQueryIconN
            = (xfTD_wpQueryIconN)wpshResolveFor(pobj,
                                                NULL, // use somSelf's class
                                                "wpQueryIconN"))
            return (pwpQueryIconN(pobj, ulIndex));
    }

    return (0);
}

/*
 *@@ icoQueryIconDataN:
 *      evil helper for calling wpQueryIconData
 *      or wpQueryIconDataN, which is undocumented.
 *
 *      Returns 0 on errors.
 *
 *      The "query icon data" methods return
 *      an ICONINFO structure. This only sometimes
 *      contains the "icon data" (which is why the
 *      method name is very misleading), but sometimes
 *      instructions for the caller about the icon
 *      format:
 *
 *      --  With ICON_RESOURCE, the caller will
 *          find the icon data in the specified
 *          icon resource.
 *
 *      --  With ICON_FILE, the caller will have
 *          to load an icon file.
 *
 *      --  Only with ICON_DATA, the icon data is
 *          actually returned. This is usually
 *          returned if the object has a user icon
 *          set in OS2.INI (for abstracts) or
 *          in the .ICONx EA (for fs objects)
 *
 *      As a result, icoLoadIconData was added
 *      which actually loads the icon data if
 *      the format isn't ICON_DATA already.
 *
 *@@added V0.9.16 (2001-10-15) [umoeller]
 */

ULONG icoQueryIconDataN(WPObject *pobj,    // in: object
                        ULONG ulIndex,     // in: animation index or 0 for regular icon
                        PICONINFO pData)   // in: icon data buffer or NULL for "query size"
{
    if (!ulIndex)
        return (_wpQueryIconData(pobj, pData));

    // index specified: this better be a folder, and we better be Warp 4
    if (    (doshIsWarp4())
         && (_somIsA(pobj, _WPFolder))
       )
    {
        xfTD_wpQueryIconDataN pwpQueryIconDataN;

        if (pwpQueryIconDataN
            = (xfTD_wpQueryIconDataN)wpshResolveFor(pobj,
                                                    NULL, // use somSelf's class
                                                    "wpQueryIconDataN"))
            return (pwpQueryIconDataN(pobj, pData, ulIndex));
    }

    return (0);
}

/*
 *@@ icoSetIconDataN:
 *      evil helper for calling wpSetIconData
 *      or wpSetIconDataN, which is undocumented.
 *
 *      While wpSetIcon only temporarily changes
 *      an object's icon (which is then returned
 *      by wpQueryIcon), wpSetIconData stores a
 *      new persistent icon as well. For abstracts,
 *      this will go into OS2.INI, for fs objects
 *      into the object's EAs.
 *
 *      Note that as a special hack, wpSetIconData
 *      supports the ICON_CLEAR "format", which
 *      resets the icon data to the class's default
 *      icon (or something else for special classes
 *      like WPDisk, WPDataFile or WPProgram).
 *
 *      Returns 0 on errors.
 *
 *@@added V0.9.16 (2001-10-15) [umoeller]
 */

BOOL icoSetIconDataN(WPObject *pobj,    // in: object
                     ULONG ulIndex,     // in: animation index or 0 for regular icon
                     PICONINFO pData)   // in: icon data to set (requried)
{
    if (!ulIndex)
        return (_wpSetIconData(pobj, pData));

    // index specified: this better be a folder, and we better be Warp 4
    if (    (doshIsWarp4())
         && (_somIsA(pobj, _WPFolder))
       )
    {
        xfTD_wpSetIconDataN pwpSetIconDataN;

        if (pwpSetIconDataN
            = (xfTD_wpSetIconDataN)wpshResolveFor(pobj,
                                                  NULL, // use somSelf's class
                                                  "wpSetIconDataN"))
            return (pwpSetIconDataN(pobj, pData, ulIndex));
    }

    return (0);
}

/*
 *@@ icoClsQueryIconN:
 *      evil helper for calling wpclsQueryIconN,
 *      which is undocumented.
 *
 *      WARNING: This always builds a new HPOINTER,
 *      so the caller should use WinDestroyPointer
 *      to avoid resource leaks.
 *
 *@@added V0.9.16 (2001-10-19) [umoeller]
 */

HPOINTER icoClsQueryIconN(SOMClass *pClassObject,
                          ULONG ulIndex)
{
    if (!ulIndex)
        return (_wpclsQueryIcon(pClassObject));

    // index specified: this better be a folder, and we better be Warp 4
    if (    (doshIsWarp4())
         && (_somDescendedFrom(pClassObject, _WPFolder))
       )
    {
        xfTD_wpclsQueryIconN pwpclsQueryIconN;

        if (pwpclsQueryIconN
            = (xfTD_wpclsQueryIconN)wpshResolveFor(pClassObject,
                                                   NULL, // use somSelf's class
                                                   "wpclsQueryIconN"))
            return (pwpclsQueryIconN(pClassObject, ulIndex));
    }

    return (0);
}

/*
 *@@ LoadIconData:
 *      helper for icoLoadIconData.
 *
 *@@added V0.9.16 (2001-10-19) [umoeller]
 */

APIRET LoadIconData(WPObject *pobj,             // in: object whose icon to query
                    ULONG ulIndex,              // in: animation index or 0 for regular icon
                    PICONINFO *ppIconInfo,      // out: ICONINFO allocated via _wpAllocMem
                    BOOL fMayRecurse)           // in: if TRUE, this may recurse
{
    APIRET arc = NO_ERROR;
    PICONINFO pData = NULL;
    ULONG cbIconInfo;

    arc = ERROR_NO_DATA;     // whatever, this shouldn't fail

    // find out how much memory the object needs for this
    if (    (cbIconInfo = icoQueryIconDataN(pobj,
                                            ulIndex,
                                            NULL))        // query size
                                // if this fails, arc is still ERROR_NO_DATA
            // allocate the memory
         && (pData = doshMalloc(cbIconInfo, &arc))
       )
    {
        // ask the object again
        if (icoQueryIconDataN(pobj,
                              ulIndex,
                              pData))
        {
            // get the icon data depending on the format
            switch (pData->fFormat)
            {
                case ICON_RESOURCE:
                {
                    ULONG   cbResource;
                    PVOID   pvResourceTemp;
                    // object has specified icon resource:
                    // load resource data...
                    if (    (!(arc = DosQueryResourceSize(pData->hmod,
                                                          RT_POINTER,
                                                          pData->resid,
                                                          &cbResource)))
                       )
                    {
                        if (!cbResource)
                            arc = ERROR_NO_DATA;
                        else if (!(arc = DosGetResource(pData->hmod,
                                                        RT_POINTER,
                                                        pData->resid,
                                                        &pvResourceTemp)))
                        {
                            // loaded OK:
                            // return a new ICONINFO then
                            PICONINFO pData2;
                            ULONG     cb2 = sizeof(ICONINFO) + cbResource;
                            if (pData2 = doshMalloc(cb2, &arc))
                            {
                                // point icon data to after ICONINFO struct
                                PBYTE pbIconData = (PBYTE)pData2 + sizeof(ICONINFO);
                                pData2->cb = sizeof(ICONINFO);
                                pData2->fFormat = ICON_DATA;
                                pData2->cbIconData = cbResource;
                                pData2->pIconData = pbIconData;
                                // copy icon data there
                                memcpy(pbIconData, pvResourceTemp, cbResource);

                                // and return the new struct
                                *ppIconInfo = pData2;
                            }

                            // in any case, free the original
                            free(pData);
                            pData = NULL;       // do not free again below

                            DosFreeResource(pvResourceTemp);
                        }
                    }
                }
                break;

                case ICON_DATA:
                    // this is OK, no conversion needed
                    *ppIconInfo = pData;
                break;

                case ICON_FILE:
                    if (fMayRecurse)
                    {
                        WPFileSystem *pfs;
                        if (    (pData->pszFileName)
                             && (pfs = _wpclsQueryObjectFromPath(_WPFileSystem,
                                                                 pData->pszFileName))
                           )
                        {
                            // recurse
                            PICONINFO pData2;
                            if (!(arc = LoadIconData(pfs,
                                                     ulIndex,
                                                     &pData2,
                                                     FALSE))) // may _not_ recurse
                            {
                                // got the data:
                                // return the new struct
                                *ppIconInfo = pData2;

                                free(pData);
                                pData = NULL;       // do not free again below
                            }
                        }
                        else
                            arc = ERROR_FILE_NOT_FOUND;
                    }
                    else
                        // invalid recursion:
                        arc = ERROR_NESTING_NOT_ALLOWED;
                break;

                default:
                    // any other format:
                    arc = ERROR_INVALID_DATA;
            } // end switch (pData->Format)
        } // end if (_wpQueryIconData(pobj, pData))
        else
            arc = ERROR_NO_DATA;

        if (arc && pData)
            free(pData);
    }

    return (arc);
}

/*
 *@@ icoLoadIconData:
 *      retrieves the ICONINFO for the specified
 *      object and animation index in a new buffer.
 *
 *      If ulIndex == 0, this retrieves the standard
 *      icon. Otherwise this returns the animation icon.
 *      Even though the WPS always uses this stupid
 *      index with the icon method calls, I don't think
 *      any index besides 1 is actually supported.
 *
 *      If this returns NO_ERROR, the given PICONINFO*
 *      will receive a pointer to a newly allocated
 *      ICONINFO buffer whose format is always ICON_DATA.
 *      This will properly load an icon resource if the
 *      object has the icon format set to ICON_RESOURCE
 *      or ICON_FILE.
 *
 *      If NO_ERROR is returned, the caller must free()
 *      this pointer.
 *
 *      Otherwise this might return the following errors:
 *
 *      --  ERROR_NO_DATA: icon format not understood.
 *
 *      --  ERROR_FILE_NOT_FOUND: icon file doesn't exist.
 *
 *      --  ERROR_NESTING_NOT_ALLOWED: invalid internal recursion.
 *
 *      plus those of _wpAllocMem and DosGetResource, such as
 *      ERROR_NOT_ENOUGH_MEMORY.
 *
 *      This is ICONINFO:
 *
 +      typedef struct _ICONINFO {
 +        ULONG       cb;           // Length of the ICONINFO structure.
 +        ULONG       fFormat;      // Indicates where the icon resides.
 +        PSZ         pszFileName;  // Name of the file containing icon data (ICON_FILE)
 +        HMODULE     hmod;         // Module containing the icon resource (ICON_RESOURCE)
 +        ULONG       resid;        // Identity of the icon resource (ICON_RESOURCE)
 +        ULONG       cbIconData;   // Length of the icon data in bytes (ICON_DATA)
 +        PVOID       pIconData;    // Pointer to the buffer containing icon data (ICON_DATA)
 +      } ICONINFO;
 *
 *@@added V0.9.16 (2001-10-15) [umoeller]
 */

APIRET icoLoadIconData(WPObject *pobj,             // in: object whose icon to query
                       ULONG ulIndex,              // in: animation index or 0 for regular icon
                       PICONINFO *ppIconInfo)      // out: ICONINFO allocated via _wpAllocMem
{
    *ppIconInfo = NULL;
    return (LoadIconData(pobj,
                         ulIndex,
                         ppIconInfo,
                         TRUE));            // may recurse
}

/*
 *@@ icoCopyIconFromObject:
 *      sets a new persistent icon for somSelf
 *      by copying the icon data from pobjSource.
 *
 *      Preconditions:
 *
 *      --  somSelf better not be a shadow.
 *
 *@@added V0.9.16 (2001-10-15) [umoeller]
 */

APIRET icoCopyIconFromObject(WPObject *somSelf,       // in: target
                             WPObject *pobjSource,    // in: source
                             ULONG ulIndex)           // in: animation index or 0 for regular icon
{
    APIRET arc = NO_ERROR;

    if ((pobjSource) && (_somIsA(pobjSource, _WPShadow)))
        pobjSource = _wpQueryShadowedObject(pobjSource, TRUE);

    if (pobjSource)
    {
        PICONINFO pData;
        if (arc = icoLoadIconData(pobjSource, ulIndex, &pData))
            // error loading that icon:
            // if we're trying to load an animation icon
            // (i.e. ulIndex != 0), try loading index 0 and
            // set that on the animation icon... the user
            // might be trying to drag a regular icon on
            // the animation page
            // V0.9.16 (2001-12-08) [umoeller]
            arc = icoLoadIconData(pobjSource, 0, &pData);

        if (!arc)
        {
            // now set this icon for the target object
            icoSetIconDataN(somSelf, ulIndex, pData);
            free(pData);


            // the standard WPS behavior is that
            // if a folder icon is copied onto the
            // standard icon of a folder, the animation
            // icon is copied too... so check:
            if (    (_somIsA(somSelf, _WPFolder))
                 && (_somIsA(pobjSource, _WPFolder))
                 // but don't do this if we're setting the
                 // animation icon explicitly
                 && (!ulIndex)
               )
            {
                // alright, copy animation icon too,
                // but if this fails, don't return an error
                if (!icoLoadIconData(pobjSource, 1, &pData))
                {
                    icoSetIconDataN(somSelf, 1, pData);
                    free(pData);
                }
            }
        }
    }
    else
        arc = ERROR_FILE_NOT_FOUND;

    return (arc);
}

/*
 *@@ objResetIcon:
 *      resets an object's icon to its default.
 *
 *      See the explanations about ICON_CLEAR
 *      with icoSetIconDataN.
 *
 *@@added V0.9.16 (2001-10-15) [umoeller]
 */

VOID objResetIcon(WPObject *somSelf,
                  ULONG ulIndex)
{
    ICONINFO Data;
    memset(&Data, 0, sizeof(Data));
    Data.fFormat = ICON_CLEAR;
    icoSetIconDataN(somSelf,
                    ulIndex,
                    &Data);
}

/*
 *@@ icoIsUsingDefaultIcon:
 *      returns TRUE if the object is using
 *      a default icon for the specified
 *      animation index.
 *
 *      This call is potentially expensive so
 *      don't use it excessively.
 *
 *@@added V0.9.16 (2001-10-19) [umoeller]
 */

BOOL icoIsUsingDefaultIcon(WPObject *pobj,
                           ULONG ulAnimationIndex)
{
    if (ulAnimationIndex)
    {
        // caller wants animation icon checked:
        // compare this object's icon to the class's
        // default animation icon
        BOOL brc = FALSE;
        HPOINTER hptrClass = icoClsQueryIconN(_somGetClass(pobj),
                                              ulAnimationIndex);
        if (    hptrClass
             && (icoQueryIconN(pobj,
                               ulAnimationIndex)
                 == hptrClass)
           )
            brc = TRUE;

        WinDestroyPointer(hptrClass);

        return (brc);
    }

    // caller wants regular icon checked:
    // do NOT compare it to the class default icon
    // but check the object style instead (there are
    // many default icons in the WPS which are _not_
    // class default icons, e.g. WPProgram and WPDataFile
    // association icons)
    return ((0 == (_wpQueryStyle(pobj)
                            & OBJSTYLE_NOTDEFAULTICON)));
}

/* ******************************************************************
 *
 *   Replacement object "Icon" page
 *
 ********************************************************************/

CONTROLDEF
    TitleText = CONTROLDEF_TEXT(
                            LOAD_STRING,
                            ID_XSDI_ICON_TITLE_TEXT,
                            -1,
                            -1),
    TitleEF = CONTROLDEF_MLE(
                            NULL,
                            ID_XSDI_ICON_TITLE_EF,
                            300,
                            50),
    IconGroup = CONTROLDEF_GROUP(
                            LOAD_STRING,
                            ID_XSDI_ICON_GROUP),
    IconStatic =
        {
            WC_STATIC,
            NULL,
            WS_VISIBLE | SS_TEXT | DT_LEFT | DT_VCENTER | DT_MNEMONIC,
            ID_XSDI_ICON_STATIC,
            CTL_COMMON_FONT,
            0,
            {80, 80},
            COMMON_SPACING
        },
    IconExplanationText = CONTROLDEF_TEXT_WORDBREAK(
                            LOAD_STRING,
                            ID_XSDI_ICON_EXPLANATION_TXT,
                            200),
    IconEditButton = CONTROLDEF_PUSHBUTTON(
                            LOAD_STRING,
                            ID_XSDI_ICON_EDIT_BUTTON,
                            -1,
                            30),
    IconBrowseButton = CONTROLDEF_PUSHBUTTON(
                            LOAD_STRING,
                            DID_BROWSE,
                            -1,
                            30),
    IconResetButton = CONTROLDEF_PUSHBUTTON(
                            LOAD_STRING,
                            ID_XSDI_ICON_RESET_BUTTON,
                            -1,
                            30),
    ExtrasGroup = CONTROLDEF_GROUP(
                            LOAD_STRING,
                            ID_XSDI_ICON_EXTRAS_GROUP),
    HotkeyText = CONTROLDEF_TEXT(
                            LOAD_STRING,
                            ID_XSDI_ICON_HOTKEY_TEXT,
                            -1,
                            -1),
    HotkeyEF = CONTROLDEF_ENTRYFIELD(
                            NULL,
                            ID_XSDI_ICON_HOTKEY_EF,
                            100,
                            -1),
    HotkeyClearButton = CONTROLDEF_PUSHBUTTON(
                            LOAD_STRING,
                            ID_XSDI_ICON_HOTKEY_CLEAR,
                            -1,
                            30),
    HotkeySetButton = CONTROLDEF_PUSHBUTTON(
                            LOAD_STRING,
                            ID_XSDI_ICON_HOTKEY_SET,
                            -1,
                            30),
    LockPositionCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,
                            ID_XSDI_ICON_LOCKPOSITION_CB,
                            -1,
                            -1),
    TemplateCB = CONTROLDEF_AUTOCHECKBOX(
                            LOAD_STRING,
                            ID_XSDI_ICON_TEMPLATE_CB,
                            -1,
                            -1),
    DetailsButton = CONTROLDEF_PUSHBUTTON(
                            LOAD_STRING,
                            DID_DETAILS,
                            -1,
                            30);

DLGHITEM dlgObjIconFront[] =
    {
        START_TABLE,            // root table, required
    };

DLGHITEM dlgObjIconTitle[] =
    {
            START_ROW(ROW_VALIGN_TOP),       // row 1 in the root table, required
                CONTROL_DEF(&TitleText),
                CONTROL_DEF(&TitleEF)
    };

DLGHITEM dlgObjIconIcon[] =
    {
            START_ROW(ROW_VALIGN_CENTER),
                START_GROUP_TABLE(&IconGroup),
                    START_ROW(0),
                        CONTROL_DEF(&IconStatic),
                    START_TABLE,
                        START_ROW(0),
                            CONTROL_DEF(&IconExplanationText),
                        START_ROW(0),
                            CONTROL_DEF(&IconEditButton),
                            CONTROL_DEF(&IconBrowseButton),
                            CONTROL_DEF(&IconResetButton),
                    END_TABLE,
                END_TABLE,
    };

DLGHITEM dlgObjIconExtrasFront[] =
    {
            START_ROW(ROW_VALIGN_CENTER),
                START_GROUP_TABLE(&ExtrasGroup),
                    START_ROW(0)
    };

DLGHITEM dlgObjIconHotkey[] =
    {
                        START_ROW(ROW_VALIGN_CENTER),
                            CONTROL_DEF(&HotkeyText),
                            CONTROL_DEF(&HotkeyEF),
                            CONTROL_DEF(&HotkeySetButton),
                            CONTROL_DEF(&HotkeyClearButton),
                        START_ROW(ROW_VALIGN_CENTER)
    };

DLGHITEM dlgObjIconTemplate[] =
    {
                            CONTROL_DEF(&TemplateCB)
    };

DLGHITEM dlgObjIconLockedInPlace[] =
    {
                            CONTROL_DEF(&LockPositionCB)
    };

DLGHITEM dlgObjIconExtrasTail[] =
    {
                END_TABLE
    };

DLGHITEM dlgObjIconDetails[] =
    {
                CONTROL_DEF(&DetailsButton)
    };

DLGHITEM dlgObjIconTail[] =
    {
            START_ROW(0),
                CONTROL_DEF(&G_UndoButton),         // notebook.c
                CONTROL_DEF(&G_DefaultButton),      // notebook.c
                CONTROL_DEF(&G_HelpButton),         // notebook.c
        END_TABLE
    };

/*
 *@@ icoFormatIconPage:
 *      calls ntbFormatPage for the given empty dialog
 *      frame to insert the formatted controls for
 *      the "Icon" page.
 *
 *      flFlags determines the special controls to be added.
 *      This can be any combination of the following:
 *
 *      --  ICONFL_TITLE:       add the "Title" MLE.
 *
 *      --  ICONFL_ICON:        add the "Icon" controls.
 *
 *      --  ICONFL_TEMPLATE:    add the "Template" checkbox.
 *
 *      --  ICONFL_LOCKEDINPLACE: add the "Locked in place" checkbox.
 *
 *      --  ICONFL_HOTKEY:      add the "Hotkey" entry field.
 *
 *      --  ICONFL_DETAILS:     add the "Details" pushbutton.
 *
 *      The "Template" checkbox is automatically added only
 *      if the object's class doesn't have the CLSSTYLE_NEVERTEMPLATE
 *      class style flag set.
 *
 *@@added V0.9.16 (2001-10-15) [umoeller]
 */

VOID icoFormatIconPage(PCREATENOTEBOOKPAGE pcnbp,
                       ULONG flFlags)
{
    APIRET arc;

    PDLGARRAY pArrayIcon = NULL;
    if (!(arc = dlghCreateArray(   ARRAYITEMCOUNT(dlgObjIconFront)
                                 + ARRAYITEMCOUNT(dlgObjIconTitle)
                                 + ARRAYITEMCOUNT(dlgObjIconIcon)
                                 + ARRAYITEMCOUNT(dlgObjIconExtrasFront)
                                 + ARRAYITEMCOUNT(dlgObjIconHotkey)
                                 + ARRAYITEMCOUNT(dlgObjIconTemplate)
                                 + ARRAYITEMCOUNT(dlgObjIconLockedInPlace)
                                 + ARRAYITEMCOUNT(dlgObjIconDetails)
                                 + ARRAYITEMCOUNT(dlgObjIconExtrasTail)
                                 + ARRAYITEMCOUNT(dlgObjIconTail),
                                &pArrayIcon)))
    {
        if (!(arc = dlghAppendToArray(pArrayIcon,
                                      dlgObjIconFront,
                                      ARRAYITEMCOUNT(dlgObjIconFront))))
        {
            // "icon title" fields
            if (flFlags & ICONFL_TITLE)
                arc = dlghAppendToArray(pArrayIcon,
                                        dlgObjIconTitle,
                                        ARRAYITEMCOUNT(dlgObjIconTitle));

            // icon control fields
            if ( (!arc) && (flFlags & ICONFL_ICON) )
                arc = dlghAppendToArray(pArrayIcon,
                                        dlgObjIconIcon,
                                        ARRAYITEMCOUNT(dlgObjIconIcon));

            if (    (!arc)
                 && (flFlags & (  ICONFL_TEMPLATE
                                | ICONFL_LOCKEDINPLACE
                                | ICONFL_HOTKEY))
               )
            {
                arc = dlghAppendToArray(pArrayIcon,
                                        dlgObjIconExtrasFront,
                                        ARRAYITEMCOUNT(dlgObjIconExtrasFront));

                // "Template" checkbox
                if ( (!arc) && (flFlags & ICONFL_TEMPLATE) )
                    arc = dlghAppendToArray(pArrayIcon,
                                            dlgObjIconTemplate,
                                            ARRAYITEMCOUNT(dlgObjIconTemplate));

                // "Lock in place" checkbox
                if ( (!arc) && (flFlags & ICONFL_LOCKEDINPLACE) )
                    arc = dlghAppendToArray(pArrayIcon,
                                            dlgObjIconLockedInPlace,
                                            ARRAYITEMCOUNT(dlgObjIconLockedInPlace));

                // "hotkey" group
                if ( (!arc) && (flFlags & ICONFL_HOTKEY) )
                    arc = dlghAppendToArray(pArrayIcon,
                                            dlgObjIconHotkey,
                                            ARRAYITEMCOUNT(dlgObjIconHotkey));

                if (!arc)
                    arc = dlghAppendToArray(pArrayIcon,
                                            dlgObjIconExtrasTail,
                                            ARRAYITEMCOUNT(dlgObjIconExtrasTail));
            }

            // "Details" button
            if ( (!arc) && (flFlags & ICONFL_DETAILS) )
                arc = dlghAppendToArray(pArrayIcon,
                                        dlgObjIconDetails,
                                        ARRAYITEMCOUNT(dlgObjIconDetails));

            // main tail
            if (    (!arc)
                 && (!(arc = dlghAppendToArray(pArrayIcon,
                                               dlgObjIconTail,
                                               ARRAYITEMCOUNT(dlgObjIconTail))))
               )
            {
                ntbFormatPage(pcnbp->hwndDlgPage,
                              pArrayIcon->paDlgItems,
                              pArrayIcon->cDlgItemsNow);
            }
        }

        if (arc)
            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                   "dlghAppendToArray returned %d",
                   arc);

        dlghFreeArray(&pArrayIcon);
    }
}

/*
 *@@ OBJICONPAGEDATA:
 *
 *@@added V0.9.16 (2001-10-15) [umoeller]
 */

typedef struct _OBJICONPAGEDATA
{
    ULONG       flIconPageFlags;            // flags for icoFormatIconPage so we
                                            // know which fields are available

    BOOL        fIsFolder;                  // TRUE if this is for a folder page

    HWND        hwndIconStatic,
                hwndHotkeyEF;

    ULONG       ulAnimationIndex;           // 0 if main icon page
                                            // 1 if folder animation page

    // backup for "Undo"
    PICONINFO   pIconDataBackup;            // backup of icon data... if this is NULL,
                                            // the object was using a default icon
    PSZ         pszTitleBackup;             // backup of object title
    BOOL        fNoRename,
                fTemplateBackup,
                fLockedInPlaceBackup;

    // data for subclassed icon
    PCREATENOTEBOOKPAGE pcnbp;              // reverse linkage for subclassed icon
    PFNWP           pfnwpIconOriginal;
    WPObject        *pobjDragged;

    // data for subclassed hotkey EF
    // function keys
    PFUNCTIONKEY    paFuncKeys;
    ULONG           cFuncKeys;

    BOOL            fHotkeysWorking,
                    fHasHotkey,
                    fHotkeyPending;

    // object hotkey
    XFldObject_OBJECTHOTKEY Hotkey;
    BOOL            fInitiallyHadHotkey;
    XFldObject_OBJECTHOTKEY HotkeyBackup;   // if (fInitiallyHadHotkey)

} OBJICONPAGEDATA, *POBJICONPAGEDATA;

/*
 *@@ PaintIcon:
 *
 *@@added V0.9.16 (2001-10-15) [umoeller]
 */

VOID PaintIcon(POBJICONPAGEDATA pData,
               HWND hwndStatic,
               HPS hps)
{
    RECTL       rclStatic;
    LONG        cxIcon = WinQuerySysValue(HWND_DESKTOP, SV_CXICON);
    LONG        lcolBackground = winhQueryPresColor(hwndStatic,
                                                    PP_BACKGROUNDCOLOR,
                                                    TRUE,
                                                    SYSCLR_DIALOGBACKGROUND);
    HPOINTER    hptr = icoQueryIconN(pData->pcnbp->somSelf,
                                     pData->ulAnimationIndex);

    gpihSwitchToRGB(hps);
    WinQueryWindowRect(hwndStatic, &rclStatic);
    WinFillRect(hps,
                &rclStatic,
                lcolBackground);

    WinDrawPointer(hps,
                   (rclStatic.xRight - rclStatic.xLeft - cxIcon) / 2,
                   (rclStatic.yTop - rclStatic.yBottom - cxIcon) / 2,
                   hptr,
                   DP_NORMAL);
}

/*
 *@@ RemoveTargetEmphasis:
 *
 *@@added V0.9.16 (2001-10-15) [umoeller]
 */

VOID RemoveTargetEmphasis(POBJICONPAGEDATA pData,
                          HWND hwndStatic)
{
    HPS hps = DrgGetPS(hwndStatic);
    PaintIcon(pData, hwndStatic, hps);
    DrgReleasePS(hps);
}

/*
 *@@ ReportError:
 *
 *@@added V0.9.16 (2001-10-19) [umoeller]
 */

VOID ReportError(PCREATENOTEBOOKPAGE pcnbp,
                 APIRET arc)
{
    if (arc)
        cmnDosErrorMsgBox(pcnbp->hwndDlgPage,
                          '?',
                          _wpQueryTitle(pcnbp->somSelf),
                          NULL,
                          arc,
                          NULL,
                          MB_CANCEL,
                          TRUE);
}

/*
 *@@ EditIcon:
 *
 *@@added V0.9.16 (2001-10-19) [umoeller]
 */

VOID EditIcon(POBJICONPAGEDATA pData)
{
    APIRET arc;

    HPOINTER hptrOld = winhSetWaitPointer();

    PICONINFO pIconInfo;
    if (!(arc = icoLoadIconData(pData->pcnbp->somSelf,
                                pData->ulAnimationIndex,
                                &pIconInfo)))
    {
        CHAR szTempFile[CCHMAXPATH];
        if (!(arc = doshCreateTempFileName(szTempFile,
                                           NULL,        // use TEMP
                                           "WP!",       // prefix
                                           "ICO")))     // extension
        {
            PXFILE pFile;
            ULONG cb = pIconInfo->cbIconData;
            if (!cb)
                arc = ERROR_NO_DATA;     // @@todo this shouldn't happen!!
            else if (!(arc = doshOpen(szTempFile,
                                      XOPEN_READWRITE_NEW | XOPEN_BINARY,
                                      &cb,
                                      &pFile)))
            {
                arc = doshWrite(pFile,
                                pIconInfo->pIconData,
                                pIconInfo->cbIconData);
                doshClose(&pFile);
            }

            if (!arc)
            {
                CHAR szIconEdit[CCHMAXPATH];
                FILESTATUS3 fs3Old, fs3New;

                // get the date/time of the file so we
                // can check whether the file was changed
                // by iconedit
                if (    (!(arc = DosQueryPathInfo(szTempFile,
                                                  FIL_STANDARD,
                                                  &fs3Old,
                                                  sizeof(fs3Old))))

                        // find ICONEDIT
                     && (!(arc = doshSearchPath("PATH",
                                                "ICONEDIT.EXE",
                                                szIconEdit,
                                                sizeof(szIconEdit))))
                   )
                {
                    // open the icon editor with this new icon file
                    HAPP happ;
                    ULONG ulExitCode;
                    if (happ = appQuickStartApp(szIconEdit,
                                                PROG_DEFAULT,
                                                szTempFile,
                                                &ulExitCode))
                    {
                        // has the file changed?
                        if (    (!(arc = DosQueryPathInfo(szTempFile,
                                                          FIL_STANDARD,
                                                          &fs3New,
                                                          sizeof(fs3New))))
                             && (memcmp(&fs3New.ftimeLastWrite,
                                        &fs3Old.ftimeLastWrite,
                                        sizeof(FTIME)))
                           )
                        {
                            // alright, file changed:
                            // set the new icon then
                            ICONINFO NewIcon;
                            NewIcon.cb = sizeof(ICONINFO);
                            NewIcon.fFormat = ICON_FILE;
                            NewIcon.pszFileName = szTempFile;
                            if (!icoSetIconDataN(pData->pcnbp->somSelf,
                                                 pData->ulAnimationIndex,
                                                 &NewIcon))
                                arc = ERROR_FILE_NOT_FOUND;

                            // repaint icon
                            WinInvalidateRect(pData->hwndIconStatic, NULL, FALSE);
                        }
                    }
                    else
                        arc = ERROR_INVALID_EXE_SIGNATURE;
                }

                arc = DosForceDelete(szTempFile);
            }
        }

        free(pIconInfo);
    }

    WinSetPointer(HWND_DESKTOP, hptrOld);

    ReportError(pData->pcnbp, arc);
}

/*
 *@@ fnwpSubclassedIconStatic:
 *
 *@@added V0.9.16 (2001-10-15) [umoeller]
 */

MRESULT EXPENTRY fnwpSubclassedIconStatic(HWND hwndStatic, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    POBJICONPAGEDATA  pData = (POBJICONPAGEDATA)WinQueryWindowPtr(hwndStatic, QWL_USER);
    MRESULT         mrc = FALSE;

    switch(msg)
    {
        case WM_PAINT:
        {
            HPS     hps = WinBeginPaint(hwndStatic, NULLHANDLE, NULL);
            PaintIcon(pData, hwndStatic, hps);
            WinEndPaint(hps);
        }
        break;

        case DM_DRAGOVER:
        {
            USHORT      usDrop = DOR_NEVERDROP,
                        usDefaultOp = DO_LINK;
            ULONG       ulItemNow = 0;

            PDRAGINFO   pdrgInfo = (PDRAGINFO)mp1;

            if (DrgAccessDraginfo(pdrgInfo))
            {
                // invalidate valid drag object
                pData->pobjDragged = NULL;

                if (    (pdrgInfo->usOperation != DO_LINK)
                     && (pdrgInfo->usOperation != DO_DEFAULT)
                   )
                {
                    usDrop = DOR_NODROPOP;      // invalid drop operation, but send msg again
                }
                else
                {
                    // we can handle only one object
                    if (pdrgInfo->cditem == 1)
                    {
                        DRAGITEM    drgItem;
                        if (DrgQueryDragitem(pdrgInfo,
                                             sizeof(drgItem),
                                             &drgItem,
                                             0))        // first item
                        {
                            if (wpshQueryDraggedObject(&drgItem,
                                                       &pData->pobjDragged))
                            {
                                HPS hps = DrgGetPS(hwndStatic);
                                RECTL rclStatic;
                                POINTL ptl;
                                WinQueryWindowRect(hwndStatic, &rclStatic);
                                // draw target emphasis (is stricly rectangular
                                // with OS/2)
                                GpiSetColor(hps,
                                            CLR_BLACK);       // no RGB here
                                ptl.x = rclStatic.xLeft;
                                ptl.y = rclStatic.yBottom;
                                GpiMove(hps,
                                        &ptl);
                                ptl.x = rclStatic.xRight - 1;
                                ptl.y = rclStatic.yTop - 1;
                                GpiBox(hps,
                                       DRO_OUTLINE,
                                       &ptl,
                                       0,
                                       0);
                                DrgReleasePS(hps);

                                usDrop = DOR_DROP;
                            }
                        }
                    }
                }

                DrgFreeDraginfo(pdrgInfo);
            }

            // compose 2SHORT return value
            mrc = (MRFROM2SHORT(usDrop, usDefaultOp));
        }
        break;

        case DM_DRAGLEAVE:
            RemoveTargetEmphasis(pData, hwndStatic);
        break;

        case DM_DROP:
        {
            // dragover was valid above:
            APIRET arc;
            if (arc = icoCopyIconFromObject(pData->pcnbp->somSelf,
                                            pData->pobjDragged,
                                            pData->ulAnimationIndex))
                // do not display a msg box during drop,
                // this nukes PM
                WinPostMsg(hwndStatic,
                           XM_DISPLAYERROR,
                           (MPARAM)arc,
                           0);
            // and repaint
            RemoveTargetEmphasis(pData, hwndStatic);
            // re-enable controls
            pData->pcnbp->pfncbInitPage(pData->pcnbp, CBI_ENABLE);
        }
        break;

        case XM_DISPLAYERROR:
            ReportError(pData->pcnbp,
                        (APIRET)mp1);
        break;

        default:
            mrc = pData->pfnwpIconOriginal(hwndStatic, msg, mp1, mp2);
    }

    return (mrc);
}

/*
 *@@ icoIcon1InitPage:
 *      "Icon" page notebook callback function (notebook.c).
 *      Sets the controls on the page according to the
 *      object's instance settings.
 *
 *@@added V0.9.16 (2001-10-15) [umoeller]
 */

VOID XWPENTRY icoIcon1InitPage(PCREATENOTEBOOKPAGE pcnbp,
                               ULONG flFlags)
{
    POBJICONPAGEDATA pData = (POBJICONPAGEDATA)pcnbp->pUser;
    ULONG flIconPageFlags = 0;
    if (pData)
        flIconPageFlags = pData->flIconPageFlags;

    if (flFlags & CBI_INIT)
    {
        if (    (pcnbp->pUser == NULL)
             // backup data for "undo"
             && (pData = NEW(OBJICONPAGEDATA))
           )
        {
            ULONG   ulStyle = _wpQueryStyle(pcnbp->somSelf);

            ZERO(pData);

            pData->fIsFolder = _somIsA(pcnbp->somSelf, _WPFolder);

            pData->pcnbp = pcnbp;

            // store this in notebook page data
            pcnbp->pUser = pData;

            switch (pcnbp->ulPageID)
            {
                case SP_OBJECT_ICONPAGE2:
                    // folder animation icon page 2:
                    // display icon only
                    pData->flIconPageFlags = ICONFL_ICON;
                            // and nothing else!

                    // make all method calls use animation index 1
                    pData->ulAnimationIndex = 1;
                break;

                case SP_TRASHCAN_ICON:
                    // trash can icon page:
                    pData->flIconPageFlags =    ICONFL_TITLE
                                              | ICONFL_HOTKEY
                                              | ICONFL_DETAILS
                                              | ICONFL_LOCKEDINPLACE;
                break;

                default:
                    // SP_OBJECT_ICONPAGE1
                    // standard object page flags:
                    pData->flIconPageFlags =    ICONFL_TITLE
                                              | ICONFL_ICON
                                              | ICONFL_HOTKEY
                                              | ICONFL_DETAILS
                                              | ICONFL_TEMPLATE
                                              | ICONFL_LOCKEDINPLACE;
                break;
            }

            // now rule out invalid flags:

            // disable "lock in place" on Warp 3
            if (!doshIsWarp4())
                pData->flIconPageFlags &= ~ICONFL_LOCKEDINPLACE;
            else
                // backup old "locked in place" for "undo"
                pData->fLockedInPlaceBackup = !!(ulStyle & OBJSTYLE_LOCKEDINPLACE);

            flIconPageFlags = pData->flIconPageFlags;

            // insert the controls using the dialog formatter
            icoFormatIconPage(pcnbp,
                              flIconPageFlags);

            // set up controls some more

            if (flIconPageFlags & ICONFL_TITLE)
            {
                HWND hwndMLE = WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_ICON_TITLE_EF);

                // backup for undo
                pData->pszTitleBackup = strhdup(_wpQueryTitle(pcnbp->somSelf), NULL);

                WinSendMsg(hwndMLE,
                           MLM_SETTEXTLIMIT,
                           (MPARAM)255,
                           0);

                // if the object must not be renamed, set
                // the MLE read-only
                if (pData->fNoRename = !!(_wpQueryStyle(pcnbp->somSelf)
                                                & OBJSTYLE_NORENAME))
                    WinSendMsg(hwndMLE,
                               MLM_SETREADONLY,
                               (MPARAM)TRUE,
                               0);
            }

            if (flIconPageFlags & ICONFL_ICON)
            {
                BOOL fUsingDefaultIcon = icoIsUsingDefaultIcon(pcnbp->somSelf,
                                                               pData->ulAnimationIndex);

                // go subclass the icon static control
                pData->hwndIconStatic = WinWindowFromID(pcnbp->hwndDlgPage, ID_XSDI_ICON_STATIC);
                winhSetPresColor(pData->hwndIconStatic,
                                 // PP_BACKGROUNDCOLOR
                                 3L,
                                 RGBCOL_WHITE);       // white

                WinSetWindowPtr(pData->hwndIconStatic,
                                QWL_USER,
                                pData);
                pData->pfnwpIconOriginal = WinSubclassWindow(pData->hwndIconStatic,
                                                             fnwpSubclassedIconStatic);

                if (!fUsingDefaultIcon)
                    // not default icon: load a copy for "undo"
                    icoLoadIconData(pcnbp->somSelf,
                                    pData->ulAnimationIndex,
                                    &pData->pIconDataBackup);
            }

            if (flIconPageFlags & ICONFL_HOTKEY)
            {
                pData->hwndHotkeyEF = WinWindowFromID(pcnbp->hwndDlgPage,
                                                      ID_XSDI_ICON_HOTKEY_EF);

                if (pData->fHotkeysWorking = hifXWPHookReady())
                {
                    // load function keys
                    pData->paFuncKeys = hifQueryFunctionKeys(&pData->cFuncKeys);

                    // subclass entry field for hotkeys
                    ctlMakeHotkeyEntryField(pData->hwndHotkeyEF);

                    // backup original hotkey
                    if (_xwpQueryObjectHotkey(pcnbp->somSelf,
                                              &pData->HotkeyBackup))
                        pData->fInitiallyHadHotkey = TRUE;
                }
            }

            // disable template checkbox if the class style
            // has NEVERTEMPLATE
            if (    (flIconPageFlags & ICONFL_TEMPLATE)
                 && (_wpclsQueryStyle(_somGetClass(pcnbp->somSelf))
                                    & CLSSTYLE_NEVERTEMPLATE)
               )
            {
                // keep the "Template" checkbox, but disable it
                WinEnableControl(pcnbp->hwndDlgPage,
                                 ID_XSDI_ICON_TEMPLATE_CB,
                                 FALSE);
                // unset the flag so the code below won't play
                // with the template setting
                pData->flIconPageFlags &= ~ICONFL_TEMPLATE;
            }
            else
                // backup old template flag for "undo"
                pData->fTemplateBackup = !!(ulStyle & OBJSTYLE_TEMPLATE);
        }
    }

    if (flFlags & CBI_SET)
    {
        ULONG ulStyle = _wpQueryStyle(pcnbp->somSelf);

        if (flIconPageFlags & ICONFL_TITLE)
            WinSetDlgItemText(pcnbp->hwndDlgPage, ID_XSDI_ICON_TITLE_EF,
                              _wpQueryTitle(pcnbp->somSelf));

        // no need to set icon handle, this is subclassed and can
        // do this itself; just have it repaint itself to be sure
        if (flIconPageFlags & ICONFL_ICON)
            WinInvalidateRect(pData->hwndIconStatic, NULL, FALSE);

        if (flIconPageFlags & ICONFL_TEMPLATE)
            winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_ICON_TEMPLATE_CB,
                                  (!!(ulStyle & OBJSTYLE_TEMPLATE)));

        if (flIconPageFlags & ICONFL_LOCKEDINPLACE)
            winhSetDlgItemChecked(pcnbp->hwndDlgPage, ID_XSDI_ICON_LOCKPOSITION_CB,
                                  (!!(ulStyle & OBJSTYLE_LOCKEDINPLACE)));

        if (flIconPageFlags & ICONFL_HOTKEY)
        {
            USHORT      usFlags,
                        usKeyCode;
            UCHAR       ucScanCode;

            XFldObject_OBJECTHOTKEY Hotkey;

            if (_xwpQueryObjectHotkey(pcnbp->somSelf,
                                      &Hotkey))
            {
                CHAR    szKeyName[200];
                // check if maybe this is a function key
                // V0.9.3 (2000-04-19) [umoeller]
                PFUNCTIONKEY pFuncKey = hifFindFunctionKey(pData->paFuncKeys,
                                                           pData->cFuncKeys,
                                                           Hotkey.ucScanCode);
                if (pFuncKey)
                {
                    // it's a function key:
                    sprintf(szKeyName,
                            "\"%s\"",
                            pFuncKey->szDescription);
                }
                else
                    cmnDescribeKey(szKeyName, Hotkey.usFlags, Hotkey.usKeyCode);

                // set entry field
                WinSetWindowText(pData->hwndHotkeyEF, szKeyName);

                pData->fHasHotkey = TRUE;
            }
            else
            {
                WinSetWindowText(pData->hwndHotkeyEF,
                                 cmnGetString(ID_XSSI_NOTDEFINED));
                            // (cmnQueryNLSStrings())->pszNotDefined);

                pData->fHasHotkey = FALSE;
            }
        }
    }

    if (flFlags & CBI_ENABLE)
    {
        if (flIconPageFlags & ICONFL_ICON)
        {
            // disable "Reset icon" button if
            // the object doesn't have a non-default icon anyway
            WinEnableControl(pcnbp->hwndDlgPage,
                             ID_XSDI_ICON_RESET_BUTTON,
                             !icoIsUsingDefaultIcon(pcnbp->somSelf,
                                                    pData->ulAnimationIndex));
        }

        if (flIconPageFlags & ICONFL_HOTKEY)
        {
            // disable entry field if hotkeys are not working
            WinEnableWindow(pData->hwndHotkeyEF,
                            pData->fHotkeysWorking);
            WinEnableControl(pcnbp->hwndDlgPage,
                             ID_XSDI_ICON_HOTKEY_TEXT,
                             pData->fHotkeysWorking);
            WinEnableControl(pcnbp->hwndDlgPage,
                             ID_XSDI_ICON_HOTKEY_CLEAR,
                                (pData->fHotkeysWorking)
                             && (pData->fHasHotkey));

            WinEnableControl(pcnbp->hwndDlgPage,
                             ID_XSDI_ICON_HOTKEY_SET,
                                (pData->fHotkeysWorking)
                             && (pData->fHotkeyPending));
        }
    }

    if (flFlags & CBI_DESTROY)
    {
        if (pData)
        {
            if (pData->pszTitleBackup)
                free(pData->pszTitleBackup);

            if (pData->pIconDataBackup)
                free(pData->pIconDataBackup);

            if (pData->paFuncKeys)
                hifFreeFunctionKeys(pData->paFuncKeys);

            // pData itself is automatically freed
        }
    }
}

/*
 *@@ HandleENHotkey:
 *
 *@@added V0.9.16 (2001-12-08) [umoeller]
 */

MRESULT HandleENHotkey(POBJICONPAGEDATA pData,
                       ULONG ulExtra)
{
    PCREATENOTEBOOKPAGE pcnbp = pData->pcnbp;

    ULONG           flReturn = 0;
    PHOTKEYNOTIFY   phkn = (PHOTKEYNOTIFY)ulExtra;
    BOOL            fStore = FALSE;
    USHORT          usFlags = phkn->usFlags;

    // check if maybe this is a function key
    // V0.9.3 (2000-04-19) [umoeller]
    PFUNCTIONKEY pFuncKey;
    if (pFuncKey = hifFindFunctionKey(pData->paFuncKeys,
                                      pData->cFuncKeys,
                                      phkn->ucScanCode))
    {
        // key code is one of the XWorkplace user-defined
        // function keys:
        // add KC_INVALIDCOMP flag (used by us for
        // user-defined function keys)
        usFlags |= KC_INVALIDCOMP;
        sprintf(phkn->szDescription,
                "\"%s\"",
                pFuncKey->szDescription);
        flReturn = HEFL_SETTEXT;
        fStore = TRUE;
    }
    else
    {
        // no function key:

        // check if this is one of the mnemonics of
        // the "Set" or "Clear" buttons; we better
        // not store those, or the user won't be
        // able to use this page with the keyboard

        if (    (    (phkn->usFlags & KC_VIRTUALKEY)
                  && (    (phkn->usvk == VK_TAB)
                       || (phkn->usvk == VK_BACKTAB)
                     )
                )
             || (    ((usFlags & (KC_CTRL | KC_SHIFT | KC_ALT)) == KC_ALT)
                  && (   (WinSendDlgItemMsg(pcnbp->hwndDlgPage,
                                            ID_XSDI_ICON_HOTKEY_SET,
                                            WM_MATCHMNEMONIC,
                                            (MPARAM)phkn->usch,
                                            0))
                      || (WinSendDlgItemMsg(pcnbp->hwndDlgPage,
                                            ID_XSDI_ICON_HOTKEY_CLEAR,
                                            WM_MATCHMNEMONIC,
                                            (MPARAM)phkn->usch,
                                            0))
                     )
                )
           )
            // pass those to owner
            flReturn = HEFL_FORWARD2OWNER;
        else
        {
            // have this key combo checked if this thing
            // makes up a valid hotkey just yet:
            // if KC_VIRTUALKEY is down,
            // we must filter out the sole CTRL, ALT, and
            // SHIFT keys, because these are valid only
            // when pressed with some other key
            if (cmnIsValidHotkey(phkn->usFlags,
                                 phkn->usKeyCode))
            {
                // valid hotkey:
                cmnDescribeKey(phkn->szDescription,
                               phkn->usFlags,
                               phkn->usKeyCode);
                flReturn = HEFL_SETTEXT;

                fStore = TRUE;
            }
        }
    }

    if (fStore)
    {
        // store hotkey for object,
        // which can then be set using the "Set" button
        // we'll now pass the scan code, which is
        // used by the hook
        pData->Hotkey.ucScanCode = phkn->ucScanCode;
        pData->Hotkey.usFlags = usFlags;
        pData->Hotkey.usKeyCode = phkn->usKeyCode;

        pData->fHotkeyPending = TRUE;

        WinEnableControl(pcnbp->hwndDlgPage,
                         ID_XSDI_ICON_HOTKEY_SET,
                         TRUE);

        // and have entry field display that (comctl.c)
    }

    return ((MPARAM)flReturn);
}

/*
 *@@ icoIcon1ItemChanged:
 *      "Icon" page notebook callback function (notebook.c).
 *      Reacts to changes of any of the dialog controls.
 *
 *@@added V0.9.16 (2001-10-15) [umoeller]
 *@@changed V0.9.16 (2001-12-08) [umoeller]: now disabling hotkeys while entryfield has the focus
 */

MRESULT XWPENTRY icoIcon1ItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                     ULONG ulItemID, USHORT usNotifyCode,
                                     ULONG ulExtra)
{
    MRESULT mrc = (MRESULT)0;

    POBJICONPAGEDATA pData;

    ULONG   flIconPageFlags;

    ULONG   ulStyleFlags = 0,       // style flags to be affected
            ulStyleMask = 0;        // style bits to be set or cleared;
                                    // to set a bit, set it in both flags and mask
                                    // to clear a bit set it in flags only
    BOOL    fRefresh = TRUE;

    APIRET  arc;

    if (    (pData = (POBJICONPAGEDATA)pcnbp->pUser)
         && (flIconPageFlags = pData->flIconPageFlags)
       )
    {
        switch (ulItemID)
        {
            case ID_XSDI_ICON_TITLE_EF:
                fRefresh = FALSE;

                if (    (usNotifyCode == MLN_KILLFOCUS)
                     // title controls available?
                     && (flIconPageFlags & ICONFL_TITLE)
                     && (!pData->fNoRename)
                   )
                {
                    PSZ pszNewTitle;
                    if (!(pszNewTitle = winhQueryWindowText(pcnbp->hwndControl)))
                    {
                        // no title: restore old
                        cmnMessageBoxMsg(pcnbp->hwndDlgPage,
                                         104,   // error
                                         187,   // old name restored
                                         MB_OK);
                    }
                    else
                        _wpSetTitle(pcnbp->somSelf, pszNewTitle);

                    free(pszNewTitle);

                    fRefresh = TRUE;
                }
            break;

            // no need for static icon control here,
            // this is subclassed and handles all the drag'n'drop

            case ID_XSDI_ICON_EDIT_BUTTON:
                EditIcon(pData);
            break;

            case DID_BROWSE:
            {
                CHAR szFilemask[CCHMAXPATH] = "*.ico";
                if (cmnFileDlg(pcnbp->hwndDlgPage,
                               szFilemask,
                               WINH_FOD_INILOADDIR | WINH_FOD_INISAVEDIR,
                               HINI_USER,
                               INIAPP_XWORKPLACE,
                               INIKEY_ICONPAGE_LASTDIR))
                {
                    WPFileSystem *pfs;
                    if (!(pfs = _wpclsQueryObjectFromPath(_WPFileSystem,
                                                          szFilemask)))
                        arc = ERROR_FILE_NOT_FOUND;
                    else
                    {
                        arc = icoCopyIconFromObject(pcnbp->somSelf,
                                                    pfs,
                                                    pData->ulAnimationIndex);
                        WinInvalidateRect(pData->hwndIconStatic, NULL, FALSE);
                    }

                    ReportError(pcnbp, arc);
                }
            }
            break;

            case ID_XSDI_ICON_RESET_BUTTON:
                if (pData->flIconPageFlags & ICONFL_ICON)
                    objResetIcon(pcnbp->somSelf, pData->ulAnimationIndex);
            break;

            case ID_XSDI_ICON_TEMPLATE_CB:
                if (flIconPageFlags & ICONFL_TEMPLATE)
                {
                    ulStyleFlags |= OBJSTYLE_TEMPLATE;
                    if (ulExtra)
                        ulStyleMask |= OBJSTYLE_TEMPLATE;
                }
            break;

            case ID_XSDI_ICON_LOCKPOSITION_CB:
                if (flIconPageFlags & ICONFL_LOCKEDINPLACE)
                {
                    ulStyleFlags |= OBJSTYLE_LOCKEDINPLACE;
                    if (ulExtra)
                        ulStyleMask |= OBJSTYLE_LOCKEDINPLACE;
                }
            break;

            /*
             * ID_XSDI_ICON_HOTKEY_EF:
             *      subclassed "hotkey" entry field;
             *      this sends EN_HOTKEY when a new
             *      hotkey has been entered
             */

            case ID_XSDI_ICON_HOTKEY_EF:
                switch (usNotifyCode)
                {
                    case EN_SETFOCUS:
                    case EN_KILLFOCUS:
                        // if we're getting the focus, disable
                        // hotkeys; if we're losing the focus,
                        // enable hotkeys again (otherwise the
                        // user can't set any hotkeys that are
                        // already occupied)
                        // V0.9.16 (2001-12-06) [umoeller]
                        krnPostDaemonMsg(XDM_DISABLEHOTKEYSTEMP,
                                         (MPARAM)(usNotifyCode == EN_SETFOCUS),
                                             // TRUE: disable (on set focus)
                                             // FALSE: re-enable (on kill focus)
                                         0);
                    break;

                    /*
                     * EN_HOTKEY:
                     *      new hotkey has been entered:
                     */

                    case EN_HOTKEY:
                        mrc = HandleENHotkey(pData,
                                             ulExtra);
                    break;
                }

                fRefresh = FALSE;
                        // or we'll hang

            break;

            /*
             * ID_XSDI_ICON_HOTKEY_SET:
             *      "Set hotkey" button
             */

            case ID_XSDI_ICON_HOTKEY_SET:
                // set hotkey
                _xwpSetObjectHotkey(pcnbp->somSelf,
                                    &pData->Hotkey);
                pData->fHotkeyPending = FALSE;
                pData->fHasHotkey = TRUE;
            break;

            /*
             * ID_XSDI_ICON_HOTKEY_CLEAR:
             *      "Clear hotkey" button
             */

            case ID_XSDI_ICON_HOTKEY_CLEAR:
                // remove hotkey
                _xwpSetObjectHotkey(pcnbp->somSelf,
                                    NULL);   // remove
                WinSetWindowText(pData->hwndHotkeyEF,
                                 cmnGetString(ID_XSSI_NOTDEFINED)); // (cmnQueryNLSStrings())->pszNotDefined);
                pData->fHotkeyPending = FALSE;
                pData->fHasHotkey = FALSE;
            break;

            /*
             * DID_DETAILS:
             *      display object details.
             */

            case DID_DETAILS:
                objShowObjectDetails(pcnbp->hwndNotebook,
                                     pcnbp->somSelf);
            break;

            /*
             * DID_UNDO:
             *
             */

            case DID_UNDO:
                if (    (flIconPageFlags & ICONFL_TITLE)
                     && (pData->pszTitleBackup)
                     && (!pData->fNoRename)
                   )
                    // set backed-up title
                    _wpSetTitle(pcnbp->somSelf, pData->pszTitleBackup);

                if (flIconPageFlags & ICONFL_ICON)
                {
                    // restore icon backup
                    if (pData->pIconDataBackup)
                        // was using non-default icon:
                        icoSetIconDataN(pcnbp->somSelf,
                                        pData->ulAnimationIndex,
                                        pData->pIconDataBackup);
                    else
                        // was using default icon:
                        objResetIcon(pcnbp->somSelf, pData->ulAnimationIndex);
                }

                if (flIconPageFlags & ICONFL_TEMPLATE)
                {
                    ulStyleFlags |= OBJSTYLE_TEMPLATE;
                    if (pData->fTemplateBackup)
                        ulStyleMask |= OBJSTYLE_TEMPLATE;
                    // else: bit is still zero
                }

                if (flIconPageFlags & ICONFL_LOCKEDINPLACE)
                {
                    ulStyleFlags |= OBJSTYLE_LOCKEDINPLACE;
                    if (pData->fLockedInPlaceBackup)
                        ulStyleMask |= OBJSTYLE_LOCKEDINPLACE;
                    // else: bit is still zero
                }

                if (flIconPageFlags & ICONFL_HOTKEY)
                {
                    if (pData->fInitiallyHadHotkey)
                    {
                        _xwpSetObjectHotkey(pcnbp->somSelf,
                                            &pData->HotkeyBackup);
                        pData->fHasHotkey = TRUE;
                    }
                    else
                    {
                        // no hotkey: delete
                        _xwpSetObjectHotkey(pcnbp->somSelf,
                                            NULL);
                        pData->fHasHotkey = FALSE;
                    }
                    pData->fHotkeyPending = FALSE;
                }
            break;

            /*
             * DID_DEFAULT:
             *
             */

            case DID_DEFAULT:
                if (    (flIconPageFlags & ICONFL_TITLE)
                     && (!pData->fNoRename)
                   )
                    // set class default title
                    _wpSetTitle(pcnbp->somSelf,
                                _wpclsQueryTitle(_somGetClass(pcnbp->somSelf)));

                if (flIconPageFlags & ICONFL_ICON)
                    // reset standard icon
                    objResetIcon(pcnbp->somSelf, pData->ulAnimationIndex);

                if (flIconPageFlags & ICONFL_TEMPLATE)
                    // clear template bit
                    ulStyleFlags |= OBJSTYLE_TEMPLATE;

                if (flIconPageFlags & ICONFL_LOCKEDINPLACE)
                    // clear locked-in-place bit
                    ulStyleFlags |= OBJSTYLE_LOCKEDINPLACE;

                if (flIconPageFlags & ICONFL_HOTKEY)
                {
                    // delete hotkey
                    _xwpSetObjectHotkey(pcnbp->somSelf,
                                        NULL);
                    pData->fHasHotkey = FALSE;
                    pData->fHotkeyPending = FALSE;
                }
            break;

            default:
                fRefresh = FALSE;
        }
    }

    if (ulStyleFlags)
    {
        // object style flags are to be changed:
        _wpModifyStyle(pcnbp->somSelf,
                       ulStyleFlags,        // affected flags
                       ulStyleMask);        // bits to be set or cleared
        fRefresh = TRUE;
    }

    if (fRefresh)
    {
        // update the display by calling the INIT callback
        pcnbp->pfncbInitPage(pcnbp, CBI_SET | CBI_ENABLE);
        // and save the object (to be on the safe side)
        _wpSaveDeferred(pcnbp->somSelf);
    }

    return (mrc);
}


