
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
 *          MINIRECORDCORE.hptrIcon is returned by wpQueryIcon,
 *          if it is != NULLHANDLE. Otherwise wpQueryIcon does
 *          a lot of magic things depending on the object's
 *          class (see below).
 *
 *      2)  The _general_ rule of the WPS is that any object
 *          uses the object of its class, unless it has a
 *          non-default icon (indicated by the OBJSTYLE_NOTDEFAULTICON
 *          object style).
 *
 *          For most classes, the object will use the class icon.
 *          In that case, hptrIcon is set to what wpclsQueryIcon
 *          returns for the object's class. Apparently the class
 *          icon is loaded only once per class and then shared for
 *          objects that use the class icon.
 *
 *          The class icon is _not_ used however if the object
 *          has a custom icon, usually set by the user on the
 *          "Icon" page. In that case, the custom icon data is
 *          stored with the object (either in OS2.INI or in the
 *          .ICON EA), and the OBJSTYLE_NOTDEFAULTICON flag is
 *          set for the object during wpRestoreState somewhere.
 *
 *          Quite simple, and quite logical, and mostly documented.
 *
 *          The problem is of course that all subclasses of
 *          WPFileSystem (including WPFolder, WPDataFile and
 *          again WPProgramFile) plus the abstract WPProgram
 *          modify this standard behavior. And since 99% of all
 *          WPS objects are of those classes, things are different
 *          in 99% of all cases. This is where the problems
 *          start in practice when replacing the WPS standard
 *          icons.
 *
 *      3)  For data file objects, the WPS appears to defer icon
 *          loading until the first call to wpQueryIcon, which
 *          is usually the case when the object first becomes
 *          visible in a container window. This saves the WPS
 *          from loading all icons of a folder if only a small
 *          subset of the icons actually ever becomes visible.
 *          You can notice this when scrolling thru a folder
 *          with many data file objects.
 *
 *          So, as opposed to the standard model explained
 *          under (2), for data file objects the HPOINTER is only
 *          created during wpRestoreState if the object _does_
 *          have an .ICON EA. It is not created if the object
 *          does not have a custom icon and will later receive
 *          the icon of its associated program.
 *
 *          The reason for this is speed; wpPopulate already
 *          retrieves all the EA info from disk, including the
 *          .ICON data, during DosFindFirst/Next, and this is
 *          passed to wpclsMakeAwake and then wpRestoreState in
 *          the "ulReserved" parameter that is not documented
 *          in WPSREF. Really, this is a pointer to the
 *          FILEFINDBUF3 and the FEA2LIST that was filled
 *          by wpPopulate.
 *
 *          Since the data is already present, in that case,
 *          the WPS can quickly build that data already during
 *          object instantiation.
 *
 *      4)  Now, as indicated above, the entry point into the icon mess
 *          is WPObject::wpQueryIcon. I believe this gets called by the
 *          WPS's container owner-draw routines. As a result, the method
 *          only gets called if the icon is actually visible.
 *
 *          wpQueryIcon returns the hptrIcon from the MINIRECORDCORE,
 *          if it was set already during wpRestoreState. If the field
 *          is still NULLHANDLE, the WPS _then_ builds a proper icon.
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
 *              If that fails, a standard executable icon is used.
 *
 *          If all of this fails, or for other classes, the WPS
 *          gets the class's default icon from wpclsQueryIcon.
 *
 *      5)  WPObject::wpSetIcon sets the MINIRECORDCORE.hptrIcon
 *          field. This can be called at any time and will also
 *          update the object's display in all containers where
 *          it is currently inserted.
 *
 *          wpSetIcon does _not_ store the icon persistently.
 *          It only changes the current icon and gets called
 *          from the various wpQueryIcon implementations when
 *          the icon is first built for an object.
 *
 *          I guess this makes it sufficiently clear that the
 *          method naming is quite silly here -- a "query" method
 *          always calls a "set" method?!? This makes it nearly
 *          impossible to override wpSetIcon, too.
 *
 *          Note that if the object had an icon previously (i.e.
 *          hptrIcon was != NULLHANDLE), the old icon is freed if
 *          the object has the OBJSTYLE_NOTDEFAULTICON flag set.
 *
 *          This flag is not changed by the method however. As
 *          a result, _after_ calling wpSetIcon, you should always
 *          call wpModifyStyle to clear or set the flag, depending
 *          on whether the icon should be freed if the object
 *          goes dormant (or will be changed again later).
 *
 *      6)  WPObject::wpQueryIconData does not return an icon,
 *          nor does it always return icon data. (Another misnamed
 *          method.) It only returns _information_ about where
 *          to find the icon; only if the object has a non-default
 *          icon, the actual icon data is returned.
 *
 *          I believe this method only gets called in some very
 *          special situations, such as on the "Icon" page and
 *          during drag'n'drop to build icon copies. Also I
 *          haven't exactly figured out how the memory management
 *          is supposed to work here.
 *
 *      7)  WPObject::wpSetIconData is supposed to change the icon
 *          permanently. Depending on the object's class, this
 *          will store the icon data in the .ICON EA or in OS2.INI.
 *          This will also call wpSetIcon in turn to change the
 *          current icon of the object.
 *
 *      Wheew. Now for the replacements. There are several problems
 *      with the WPS implementation:
 *
 *      1)  We want to replace the default icons. Unfortunately
 *          this requires us to replace the icon management
 *          altogether because for example for executables, the
 *          only public interface is WinLoadFileIcon, which
 *          _always_ builds an icon even if it's a default one.
 *
 *      2)  WinLoadFileIcon takes ages on PE executables.
 *
 *      3)  The WPS is not very good at optimizing memory usage.
 *          Basically each object has its own icon handle unless
 *          it is really the class default icon, which is quite
 *          expensive. I am now trying to at least share icons
 *          that are reused, such as our program default icons.
 *          This has turned out to be quite a mess too because
 *          the WPS sometimes frees icons where it shouldn't.
 *
 *      4)  We have to rewrite the entire icon page too, but
 *          this is a good idea in the first place because we
 *          wanted to get rid of the extra "Object" page for
 *          object hotkeys and such.
 *
 *      So we have to replace the entire executable resource
 *      management, which is in this file (icons.c).
 *      Presently NE and LX icons are supported; see icoLoadExeIcon.
 *
 *      Function prefix for this file:
 *      --  ico*
 *
 *@@added V0.9.V0.9.16 (2001-10-01) [umoeller]
 *@@header "filesys\icons.h"
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
#define INCL_WINERRORS

#define INCL_GPIPRIMITIVES
#define INCL_GPIBITMAPS
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
#include "helpers\exeh.h"               // executable helpers
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
#include "filesys\program.h"            // program implementation; WARNING: this redefines macros

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
 *      calls the undocumented WinBuildPtrHandle API which
 *      buils a new HPOINTER from an icon data buffer.
 *      This is faster and more convenient than
 *      WinCreatePointerIndirect where we'd have to build
 *      all the bitmaps ourselves first.
 *
 *      The API is resolved from PMMERGE.DLL on the first call.
 *
 *      An "icon data buffer" is simply the bunch of bitmap
 *      headers, color tables and bitmap data as described
 *      in PMREF; I believe it has the same format as an .ICO
 *      file and the .ICON EA. It appears that the WPS also
 *      uses this format when it stores icons in OS2.INI for
 *      abstract objects.
 *
 *      Returns:
 *
 *      --  NO_ERROR: *phptr has received a new HPOINTER. Use
 *          WinFreeFileIcon to free the icon. If you're using
 *          the icon on an object via wpSetIcon, set the
 *          OBJSTYLE_NOTDEFAULTICON style on the object.
 *
 *      --  ICONERR_BUILDPTR_FAILED: WinBuildPtrHandle failed
 *          for some unknown reason. Probably the data buffer
 *          is invalid.
 *
 *      --  ERROR_PROTECTION_VIOLATION
 *
 *      plus the return codes of DosQueryModuleHandle,
 *      DosQueryProcAddr, DosLoadModule.
 *
 *@@added V0.9.16 (2001-12-08) [umoeller]
 *@@changed V0.9.16 (2001-12-17) [lafaix]: fixed error handling and added ERROR_INVALID_HANDLE support
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
                {
                    if ((arc = DosQueryProcAddr(hmod,
                                                5117,        // WinBuildPtrHandle (32-bit)
                                                NULL,
                                                (PFN*)&WinBuildPtrHandle)))
                    {
                        // the CP programming guide and reference says use
                        // DosLoadModule if DosQueryProcAddr fails with this error

                        if (arc == ERROR_INVALID_HANDLE)
                        {
                            if (!(arc = DosLoadModule(NULL, 0, "PMMERGE", &hmod)))
                            {
                                if ((arc = DosQueryProcAddr(hmod,
                                                            5117,
                                                            NULL,
                                                            (PFN*)&WinBuildPtrHandle)))
                                    cmnLog(__FILE__, __LINE__, __FUNCTION__,
                                           "Error %d resolving WinBuildPtrHandle.", arc);
                            }
                            else
                                cmnLog(__FILE__, __LINE__, __FUNCTION__,
                                       "Error %d loading module PMMERGE.", arc);
                        }
                        else
                            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                                   "Error %d resolving WinBuildPtrHandle.", arc);
                    }
                }
                else
                    cmnLog(__FILE__, __LINE__, __FUNCTION__,
                           "Error %d querying module handle for PMMERGE.", arc);
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
 *      attempts to load the specified .ICO file.
 *
 *      To support both "query icon" and "query
 *      icon data" interfaces, this supports three
 *      output formats:
 *
 *      --  If (phptr != NULL), the icon data
 *          is loaded and turned into a new
 *          HPOINTER by calling icoBuildPtrHandle.
 *          Use WinFreeFileIcon to free that pointer.
 *
 *      --  If (pcbIconData != NULL), *pcbIconData
 *          is set to the size that is required for
 *          holding the icon data. This allows for
 *          calling the method twice to allocate
 *          a sufficient buffer dynamically.
 *
 *      --  If (pbIconData != NULL), the icon
 *          data is copied into that buffer. The
 *          caller is responsible for allocating
 *          and freeing that buffer.
 *
 *          In that case, *pcbIconData must also
 *          contain the size of the buffer on input.
 *
 *      The above is only valid if NO_ERROR is
 *      returned.
 *
 *      Otherwise this returns:
 *
 *      --  ERROR_FILE_NOT_FOUND
 *      --  ERROR_PATH_NOT_FOUND
 *      --  ERROR_NOT_ENOUGH_MEMORY
 *
 *      --  ERROR_INVALID_PARAMETER: pcszFilename is
 *          invalid, or pcbIconData was NULL while
 *          pbIconData was != NULL.
 *
 *      --  ERROR_BUFFER_OVERFLOW: *pcbIconData is too
 *          small a size of pbIconData.
 *
 *      plus the error codes of icoBuildPtrHandle.
 *
 *@@added V0.9.16 (2001-12-08) [umoeller]
 */

APIRET icoLoadICOFile(PCSZ pcszFilename,
                      HPOINTER *phptr,          // out: if != NULL, newly built HPOINTER
                      PULONG pcbIconData,       // in/out: if != NULL, size of buffer required
                      PBYTE pbIconData)         // out: if != NULL, icon data that was loaded
{
    APIRET  arc = NO_ERROR;
    PXFILE  pxf = NULL;

    PBYTE   pbData = NULL;
    ULONG   cbData = 0;

    TRY_LOUD(excpt1)
    {
        if (!(arc = doshOpen(pcszFilename,
                             XOPEN_READ_EXISTING | XOPEN_BINARY,
                             &cbData,
                             &pxf)))
        {
            if (!cbData)
                arc = ERROR_NO_DATA;
            else
            {
                if (!(pbData = malloc(cbData)))
                    arc = ERROR_NOT_ENOUGH_MEMORY;
                else
                {
                    ULONG ulDummy;
                    arc = DosRead(pxf->hf,
                                  pbData,
                                  cbData,
                                  &ulDummy);
                }
            }
        }

        // output data
        if (!arc)
        {
            if (phptr)
                arc = icoBuildPtrHandle(pbData,
                                        phptr);

            if (pbIconData)
                if (!pcbIconData)
                    arc = ERROR_INVALID_PARAMETER;
                else if (*pcbIconData < cbData)
                    arc = ERROR_BUFFER_OVERFLOW;
                else
                    memcpy(pbIconData,
                           pbData,
                           cbData);

            if (pcbIconData)
                *pcbIconData = cbData;
        }
    }
    CATCH(excpt1)
    {
        arc = ERROR_PROTECTION_VIOLATION;
    } END_CATCH();

    if (pbData)
        free(pbData);

    doshClose(&pxf);

    return (arc);
}

/*
 *@@ icoBuildPtrFromFEA2List:
 *      builds a pointer from the .ICON data in
 *      the given FEA2LIST, if there's some.
 *
 *      See icoLoadICOFile for the meaning of
 *      the phptr, pcbIconData, and pbIconData
 *      parameters.
 *
 *      Returns:
 *
 *      --  NO_ERROR: output data was set. See
 *          icoLoadICOFile.
 *
 *      --  ERROR_NO_DATA: no .ICON EA found,
 *          or it has a bad format.
 *
 *      --  ERROR_INVALID_PARAMETER: pFEA2List is
 *          invalid, or pcbIconData was NULL while
 *          pbIconData was != NULL.
 *
 *      --  ERROR_BUFFER_OVERFLOW: *pcbIconData is too
 *          small a size of pbIconData.
 *
 *      plus the error codes of icoBuildPtrHandle.
 *
 *@@added V0.9.16 (2001-12-08) [umoeller]
 */

APIRET icoBuildPtrFromFEA2List(PFEA2LIST pFEA2List,     // in: FEA2LIST to check for .ICON EA
                               HPOINTER *phptr,         // out: if != NULL, newly built HPOINTER
                               PULONG pcbIconData,      // in/out: if != NULL, size of buffer required
                               PBYTE pbIconData)        // out: if != NULL, icon data that was loaded
{
    APIRET arc = NO_ERROR;

    if (!pFEA2List)
        arc = ERROR_INVALID_PARAMETER;
    else
    {
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
                USHORT cbData;
                if (cbData = *pcbValue)
                {
                    PBYTE pbData = (PBYTE)(pcbValue + 1);

                    // output data
                    if (phptr)
                        arc = icoBuildPtrHandle(pbData,
                                                phptr);

                    if (pbIconData)
                        if (!pcbIconData)
                            arc = ERROR_INVALID_PARAMETER;
                        else if (*pcbIconData < cbData)
                            arc = ERROR_BUFFER_OVERFLOW;
                        else
                            memcpy(pbIconData,
                                   pbData,
                                   cbData);

                    if (pcbIconData)
                        *pcbIconData = cbData;
                }
                else
                    arc = ERROR_NO_DATA;
            }
            else
                arc = ERROR_NO_DATA;
        }
        else
            arc = ERROR_NO_DATA;
    }

    return (arc);
}

#pragma pack(1)
typedef struct _WIN16DIBINFO
{
    ULONG       cbFix;
    ULONG       cx;
    ULONG       cy;
    USHORT      cPlanes;
    USHORT      cBitCount;
    ULONG       cCompression;           // unused in icons
    ULONG       ulImageSize;
    ULONG       ulXPelsPerMeter;        // unused in icons
    ULONG       ulYPelsPerMeter;        // unused in icons
    ULONG       cColorsUsed;            // unused in icons
    ULONG       cColorsImportant;       // unused in icons
} WIN16DIBINFO, *PWIN16DIBINFO;
#pragma pack()

/*
 *@@ G_DefaultIconHeader:
 *      sick structure for building an icon
 *      data buffer so we can use memcpy
 *      for most fields instead of having
 *      to set everything explicitly.
 *
 *      Note that we use the old-style
 *      bitmap headers (i.e. BITMAPINFOHEADER
 *      instead of BITMAPINFOHEADER2, etc.).
 *
 *@@added V0.9.16 (2001-12-18) [umoeller]
 */

static struct _DefaultIconHeader
{
        // bitmap array file header;
        // includes first BITMAPFILEHEADER for AND and XOR masks
        BITMAPARRAYFILEHEADER ah;
        // first RGB color table for monochrome bitmap
        RGB MonochromeTable[2];
        // second BITMAPFILEHEADER for color pointer
        BITMAPFILEHEADER fh2;
} G_DefaultIconHeader =
    {
        {
            // bitmap array file header
            /*pah->usType*/ BFT_BITMAPARRAY, //
            /*pah->cbSize*/ 40, //
            /*pah->offNext*/ 0, //       // no next array header
            /*pah->cxDisplay*/ 0, //
            /*pah->cyDisplay*/ 0, //

            // first BITMAPFILEHEADER for AND and XOR masks
            /*pah->bfh.usType*/ BFT_COLORICON, //
            /*pah->bfh.cbSize*/ 0x1A, //    // size of file header
            /*pah->bfh.xHotspot*/ 0, //         // tbr
            /*pah->bfh.yHotspot*/ 0, //     // tbr
            /* pah->bfh.offBits = */ 0, // tbr
            /*pah->bfh.bmp.cbFix*/ sizeof(BITMAPINFOHEADER), //
            /*pah->bfh.bmp.cx*/ 0, //          // tbr
            /*pah->bfh.bmp.cy*/ 0, //          // tbr
            /*pah->bfh.bmp.cPlanes*/ 1, //
            /*pah->bfh.bmp.cBitCount*/ 1, //          // monochrome
        },
        // first RGB color table for monochrome bitmap;
        // set first entry to black, second to white
        {
            0, 0, 0, 0xFF, 0xFF, 0xFF
        },
        // second BITMAPFILEHEADER for color pointer
        {
            /*pfh->usType*/ BFT_COLORICON, //
            /*pfh->cbSize*/ 0x1A, //    // size of file header
            /*pfh->xHotspot*/ 0, // tbr
            /*pfh->yHotspot*/ 0, // tbr
            /*pfh->offBits*/  0, // tbr
            /*pfh->bmp.cbFix*/ sizeof(BITMAPINFOHEADER), //
            /*pfh->bmp.cx*/ 0, // tbr
            /*pfh->bmp.cy*/ 0, // tbr
            /*pfh->bmp.cPlanes*/ 1, //
            /*pfh->bmp.cBitCount*/ 0, // tbr
        }
    };

/*
 *@@ ConvertWinIcon:
 *      converts a Windows icon to an OS/2 icon buffer.
 *
 *      At this point, we handle only 32x32 Windows icons
 *      with 16 colors.
 *
 *      As opposed to the OS/2 code, if the system icon
 *      size is 40x40, we try to center the win icon
 *      instead of scaling it (which looks quite ugly).
 *      At this point we center only vertically though
 *      because of the weird bit offsets that have to
 *      be handled.
 *
 *      Returns:
 *
 *      --  NO_ERROR: icon data was successfully converted,
 *          and *ppbResData has received the OS/2-format
 *          icon data.
 *
 *      --  ERROR_NOT_ENOUGH_MEMORY
 *
 *      --  ERROR_BAD_FORMAT: icon format not recognized.
 *          Currently we handle only 32x32 in 16 colors.
 *
 *@@added V0.9.16 (2001-12-18) [umoeller]
 */

APIRET ConvertWinIcon(PBYTE pbBuffer,       // in: windows icon data
                      ULONG cbBuffer,       // in: size of *pbBuffer
                      PBYTE *ppbResData,     // out: converted icon data (to be free()'d)
                      PULONG pcbResdata)   // out: size of converted data (ptr can be NULL)
{
    PWIN16DIBINFO pInfo = (PWIN16DIBINFO)pbBuffer;
    APIRET arc = ERROR_BAD_FORMAT;

    ULONG cbScanLineSrc = 0;

    ULONG cbBitmapSrc = 0;
    ULONG cbMaskLineSrc = 0;
    ULONG cbEachMaskSrc = 0;

    ULONG cColors = 1 << pInfo->cBitCount;
    ULONG cyRealSrc = pInfo->cy / 2;             // cy is doubled always

    _Pmpf(("    cbFix %d", pInfo->cbFix));
    _Pmpf(("    cx %d", pInfo->cx));
    _Pmpf(("    cy %d", pInfo->cy));
    _Pmpf(("    cPlanes %d", pInfo->cPlanes));
    _Pmpf(("    cBitCount %d, cColors %d", pInfo->cBitCount, cColors));
    _Pmpf(("    cColorsUsed %d", pInfo->cColorsUsed));

    /*
     * check source format:
     *
     */

    if (    (pInfo->cx == 32)
         && (cyRealSrc == 32)
         && (cColors == 16)
       )
    {
        // 32x32, 16 colors:
        cbScanLineSrc  = ((pInfo->cx + 1) / 2);    // two pixels per byte
        cbBitmapSrc    = ( cbScanLineSrc
                           * cyRealSrc
                         );         // should be 512

        cbMaskLineSrc = ((pInfo->cx + 7) / 8);      // not dword-aligned!

        cbEachMaskSrc  = cbMaskLineSrc * cyRealSrc;        // should be 128
    }

    if (cbBitmapSrc)
    {
        // after bitmap header comes color table
        // with cColors entries
        RGB2 *paColors = (RGB2*)(pbBuffer + sizeof(WIN16DIBINFO));
        ULONG cbColors = (sizeof(RGB2) * cColors);
        // next comes XOR mask in bitmap format
        // (i.e. with 16 colors, two colors in one byte)
        PBYTE pbWinXorBitmap = (PBYTE)paColors + cbColors;
        // next comes AND mask;
        // this is monochrome always
        PBYTE pbWinAndMask = (PBYTE)pbWinXorBitmap + cbBitmapSrc;

        // make sure we won't overflow
        if (pbWinAndMask + cbEachMaskSrc > pbBuffer + cbBuffer)
            arc = ERROR_BAD_FORMAT;
        else
        {
            /*
             * create OS/2 target buffer:
             *
             */

            // target icon size == system icon size (32 or 40)
            ULONG cxDest = WinQuerySysValue(HWND_DESKTOP, SV_CXICON);
            ULONG cyRealDest = cxDest;

            // size of one color scan line: with 16 colors,
            // that's two pixels per byte
            ULONG cbScanLineDest  = ((cxDest + 1) / 2);
            // total size of target bitmap
            ULONG cbBitmapDest    = (   cbScanLineDest
                                      * cyRealDest
                                    );         // should be 512

            // size of one monochrome scan line:
            // that's eight pixels per byte
            ULONG cbMaskLineDest2  = ((cxDest + 7) / 8);
            // note, with OS/2, this must be dword-aligned:
            ULONG cbMaskLineDest =  cbMaskLineDest2 + 3 - ((cbMaskLineDest2 + 3) & 0x03);
            // total size of a monochrome bitmap
            ULONG cbEachMaskDest  = cbMaskLineDest * cyRealDest;        // should be 128

            // total size of the OS/2 icon buffer that's needed:
            ULONG cbDataDest =   sizeof(BITMAPARRAYFILEHEADER)   // includes one BITMAPFILEHEADER
                               + 2 * sizeof(RGB)
                               + sizeof(BITMAPFILEHEADER)
                                    // the above is in G_DefaultIconHeader
                               + cColors * sizeof(RGB)   // 3
                               + 2 * cbEachMaskDest          // one XOR, one AND mask
                               + cbBitmapDest;

            // go create that buffer
            if (!(*ppbResData = malloc(cbDataDest)))
                arc = ERROR_NOT_ENOUGH_MEMORY;
            else
            {

                /*
                 * fill OS/2 target buffer:
                 *
                 */

                ULONG   ul;

                PBYTE   pbDest,
                        pbAndMaskDest,
                        pbXorMaskDest,
                        pbBitmapDest;

                struct _DefaultIconHeader *pHeaderDest;


                arc = NO_ERROR;

                if (pcbResdata)
                    *pcbResdata = cbDataDest;

                // start at front of buffer
                pbDest = *ppbResData;

                pHeaderDest = (PVOID)pbDest;

                // copy default header for icon from static memory
                memcpy(pHeaderDest,
                       &G_DefaultIconHeader,
                       sizeof(G_DefaultIconHeader));

                // fix the variable fields
                pHeaderDest->ah.bfh.xHotspot
                    = pHeaderDest->fh2.xHotspot
                    = cxDest / 2;
                pHeaderDest->ah.bfh.yHotspot
                    = pHeaderDest->fh2.yHotspot
                    = cyRealDest / 2;
                pHeaderDest->ah.bfh.offBits =
                              sizeof(BITMAPARRAYFILEHEADER)
                            + sizeof(RGB) * 2
                            + sizeof(BITMAPFILEHEADER)
                            + sizeof(RGB) * cColors;
                pHeaderDest->ah.bfh.bmp.cx
                    = pHeaderDest->fh2.bmp.cx
                    = cxDest;
                pHeaderDest->ah.bfh.bmp.cy = 2 * cyRealDest;         // AND and XOR masks in one bitmap

                pHeaderDest->fh2.offBits =
                              pHeaderDest->ah.bfh.offBits
                            + 2 * cbEachMaskDest;
                pHeaderDest->fh2.bmp.cy = cyRealDest;               // single size this time
                pHeaderDest->fh2.bmp.cBitCount = pInfo->cBitCount;      // same as in source

                // alright, now we have
                // -- the BITMAPARRAYFILEHEADER,
                // -- which includes the first BITMAPFILEHEADER,
                // -- the monochrome color table,
                // -- the second BITMAPFILEHEADER
                pbDest += sizeof(G_DefaultIconHeader);

                // after this comes second color table:
                // this is for the color pointer, so copy
                // from win icon, but use only three bytes
                // (Win uses four)
                for (ul = 0; ul < cColors; ul++)
                {
                    // memcpy(pbDest, &paColors[ul], 3);
                    *(PULONG)pbDest = *(PULONG)&paColors[ul];
                    pbDest += 3;
                }

                /*
                 * convert bitmap data:
                 *
                 */

                // new monochrome AND mask;
                // this we fill below... if a bit
                // is cleared here, it will be transparent
                pbAndMaskDest = pbDest;
                // clear all AND masks first; the source icon
                // might be smaller than the target icon,
                // and we want the extra space to be transparent
                memset(pbAndMaskDest, 0x00, cbEachMaskDest);
                pbDest += cbEachMaskDest;

                // new monochrome XOR mask; for this we
                // just copy the monochrome Win AND mask,
                // because with windoze, a pixel is drawn
                // only if its AND bit is set, and to draw
                // a pixel in OS/2, the XOR bit must be set too
                pbXorMaskDest = pbDest;
                memset(pbXorMaskDest, 0xFF, cbEachMaskDest);
                pbDest += cbEachMaskDest;

                // new 16-color bitmap:
                // will receive the win 16-color xor bitmap
                pbBitmapDest = pbDest;
                memset(pbBitmapDest, 0x00, cbBitmapDest);

                if (pbDest + cbBitmapDest > *ppbResData + cbDataDest)
                    arc = ERROR_BAD_FORMAT;
                else
                {
                    // line offset to convert from 32x32 to 40x40;
                    // this centers the icon vertically
                    ULONG   ulLineOfs = (cyRealDest - cyRealSrc) / 2;

                        // note that presently the below code cannot
                        // center horizontally yet because the AND
                        // and XOR masks are monochrome; with 32
                        // pixels, the AND and XOR masks take
                        // four bytes, while with 40 pixels they
                        // take five, so we'd have to distribute
                        // the four bytes onto five, which I didn't
                        // quite feel the urge for yet. ;-)

                    // set up the bunch of pointers which point
                    // to data that is needed for this scan line;
                    // all these pointers are incremented by the
                    // respective scan line size with each line
                    // loop below
                    PBYTE   pbBitmapSrcThis  =   pbWinXorBitmap;
                    PBYTE   pbBitmapDestThis =   pbBitmapDest
                                               + ulLineOfs * cbScanLineDest;

                    PBYTE   pbAndMaskSrcThis =   pbWinAndMask;
                    PBYTE   pbAndMaskDestThis =  pbAndMaskDest
                                               + ulLineOfs * cbMaskLineDest;
                    PBYTE   pbXorMaskDestThis =  pbXorMaskDest
                                               + ulLineOfs * cbMaskLineDest;

                    ULONG   x, y;

                    // run thru the source lines and set up
                    // the target lines according to the
                    // source bitmap data
                    for (y = 0;
                         y < cyRealSrc;
                         y++)
                    {
                        PBYTE   // pbBmpTest = pbBitmapSrcThis,
                                pbAndTest = pbAndMaskSrcThis,
                                pbAndSet = pbAndMaskDestThis;

                        // bit to check for AND and XOR masks;
                        // this is shifted right with every X
                        // in the loop below
                        UCHAR   ucAndMask = 0x80;     // shifted right
                        UCHAR   ucBmpMask = 0xF0;     // toggles between 0xF0 and 0x0F

                        // copy color bitmap for this line
                        memcpy(pbBitmapDestThis,
                               pbBitmapSrcThis,
                               cbScanLineSrc);

                        // copy XOR mask from Win AND mask
                        memcpy(pbXorMaskDestThis,
                               pbAndMaskSrcThis,
                               cbMaskLineSrc);

                        for (x = 0;
                             x < pInfo->cx;
                             x++)
                        {
                            // now with windoze, a pixel is drawn if its
                            // AND bit (now in the xor mask) is originally
                            // set and the pixel color is != 0; in that case,
                            // the OS/2 AND bit must be set to 1 to make
                            // it visible (non-transparent)
                            if (
                                    // check windoze AND bit; ucAndMask
                                    // is initially 0x80 and shifted right
                                    // with every X loop
                                    ((*pbAndTest) & ucAndMask)
                                    // check windoze color nibble; ucBmp
                                    // mask is initially 0xF0 and toggled
                                    // between 0x0F and 0xF0 with every
                                    // X loop
                                 && ((pbBitmapSrcThis[x / 2]) & ucBmpMask)
                               )
                            {
                                // alright, windoze pixel is visible:
                                // set the OS/2 AND bit too
                                *pbAndSet |= ucAndMask;
                            }

                            // toggle between 0xF0 and 0x0F
                            ucBmpMask ^= 0xFF;

                            // shift right the AND mask bit
                            if (!(ucAndMask >>= 1))
                            {
                                // every eight X's, reset the
                                // AND mask
                                ucAndMask = 0x80;

                                // and go for next monochrome byte
                                pbAndTest++;
                                pbAndSet++;
                            }
                        } // end for (x = 0...

                        pbBitmapSrcThis += cbScanLineSrc;
                        pbBitmapDestThis += cbScanLineDest;
                        pbAndMaskSrcThis += cbMaskLineSrc;
                        pbAndMaskDestThis += cbMaskLineDest;
                        pbXorMaskDestThis += cbMaskLineDest;
                    } // end for (y = 0;
                } // end else if (pbDest + cbBitmapDest > *ppbResData + cbDataDest)

                if (arc)
                {
                    FREE(*ppbResData);
                }
            } // end else if (!(*ppbResData = malloc(cbDataDest)))
        } // end else if (pbWinAndMask + cbEachMaskSrc > pbBuffer + cbBuffer)
    } // end if if (cbBitmapSrc)

    return (arc);
}

/*
 *@@ LoadWinNEResource:
 *      attempts to load the data of the resource
 *      with the specified type and id from a Win16
 *      NE executable.
 *
 *      Note that Windows uses different resource
 *      type flags than OS/2; use the WINRT_* flags
 *      from dosh.h instead of the OS/2 RT_* flags.
 *
 *      If idResource == 0, the first resource of
 *      the specified type is loaded.
 *
 *      From my testing, this code is quite efficient.
 *      Using doshReadAt, we usually get by with only
 *      two actual DosRead's from disk, including
 *      reading the actual resource data.
 *
 *      If NO_ERROR is returned, *ppbResData receives
 *      a new buffer with the resource data. The caller
 *      must free() that buffer.
 *
 *      If the resource type is WINRT_ICON, the icon
 *      data is converted to the OS/2 format, if
 *      possible. See ConvertWinIcon().
 *
 *      Otherwise this returns:
 *
 *      --  ERROR_INVALID_EXE_SIGNATURE: pExec is not NE
 *          or not Win16.
 *
 *      --  ERROR_NO_DATA: resource not found.
 *
 *      --  ERROR_BAD_FORMAT: cannot handle resource format.
 *
 *@@added V0.9.16 (2001-12-08) [umoeller]
 */

APIRET LoadWinNEResource(PEXECUTABLE pExec,     // in: executable from exehOpen
                         ULONG ulType,          // in: RT_* type (e.g. RT_POINTER)
                         ULONG idResource,      // in: resource ID or 0 for first
                         PBYTE *ppbResData,     // out: converted resource data (to be free()'d)
                         PULONG pcbResData)     // out: size of converted data (ptr can be NULL)
{
    APIRET          arc = NO_ERROR;
    ULONG           cbRead;

    ULONG           ulNewHeaderOfs = 0; // V0.9.12 (2001-05-03) [umoeller]

    PNEHEADER       pNEHeader;

    USHORT          usAlignShift;
    PXFILE          pFile;

    if (!(pNEHeader = pExec->pNEHeader))
        return (ERROR_INVALID_EXE_SIGNATURE);

    if (pExec->pDosExeHeader)
        // executable has DOS stub: V0.9.12 (2001-05-03) [umoeller]
        ulNewHeaderOfs = pExec->pDosExeHeader->ulNewHeaderOfs;

    // 1) res tbl starts with align leftshift
    pFile = pExec->pFile;
    cbRead = sizeof(usAlignShift);
    if (!(arc = doshReadAt(pFile,
                           // start of res table
                           pNEHeader->usResTblOfs
                             + ulNewHeaderOfs,
                           &cbRead,
                           (PBYTE)&usAlignShift,
                           DRFL_FAILIFLESS)))
    {
        // run thru the resources
        BOOL fPtrFound = FALSE;

        // current offset: since we want to use doshReadAt
        // for caching, we need to maintain this
        ULONG ulCurrentOfs =   pNEHeader->usResTblOfs
                             + ulNewHeaderOfs
                             + cbRead;      // should be sizeof(usAlignShift)

        while (!arc)
        {
            #pragma pack(1)
            struct WIN16_RESTYPEINFO
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
            cbRead = sizeof(typeinfo);
            if (!(arc = doshReadAt(pFile,
                                   ulCurrentOfs,
                                   &cbRead,
                                   (PBYTE)&typeinfo,
                                   0)))
            {
                // advance our private file pointer
                ulCurrentOfs += cbRead;

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
                    typedef struct WIN16_RESNAMEINFO
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
                        // this is not our type, so we can simply
                        // skip the entire table for speed
#ifdef _PMPRINTF_
                        CHAR szBuf[100];
                        _Pmpf((__FUNCTION__ ": skipping type %d (%s), %d entries",
                                      ulTypeThis,
                                      progGetWinResourceTypeName(szBuf, ulTypeThis),
                                      typeinfo.rt_nres));
#endif
                        ulCurrentOfs += typeinfo.rt_nres * sizeof(nameinfo);
                    }
                    else
                    {
                        // this is our type:
                        nameinfo *paNameInfos = NULL;
                        ULONG cbNameInfos;

#ifdef _PMPRINTF_
                        CHAR szBuf[100];
                        _Pmpf((__FUNCTION__ ": entering type %d (%s), %d entries",
                                      ulTypeThis,
                                      progGetWinResourceTypeName(szBuf, ulTypeThis),
                                      typeinfo.rt_nres));
#endif

                        if (    (!(arc = doshAllocArray(typeinfo.rt_nres,
                                                        sizeof(nameinfo),
                                                        (PBYTE*)&paNameInfos,
                                                        &cbNameInfos)))
                             && (cbRead = cbNameInfos)
                             && (!(arc = doshReadAt(pFile,
                                                    ulCurrentOfs,
                                                    &cbRead,
                                                    (PBYTE)paNameInfos,
                                                    DRFL_FAILIFLESS)))
                           )
                        {
                            ULONG ul;
                            nameinfo *pThis = paNameInfos;

                            ulCurrentOfs += cbRead;

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
                                    if (!(pb = malloc(cbThis)))
                                        arc = ERROR_NOT_ENOUGH_MEMORY;
                                    else
                                    {
                                        if (!(arc = doshReadAt(pFile,
                                                               ulOffset,
                                                               &cbThis,
                                                               pb,
                                                               DRFL_FAILIFLESS)))
                                        {
                                            if (ulType == WINRT_ICON)
                                            {
                                                ULONG cbConverted = 0;
                                                if (!ConvertWinIcon(pb,
                                                                    cbThis,
                                                                    ppbResData,
                                                                    &cbConverted))
                                                {
                                                    if (pcbResData)
                                                        *pcbResData = cbConverted;
                                                    fPtrFound = TRUE;
                                                }
                                                // else unknown format: keep looking

                                                // but always free the buffer,
                                                // since ConvertWinIcon has created one
                                                free(pb);
                                            }
                                            else
                                            {
                                                // not icon: just return this
                                                *ppbResData = pb;
                                                if (pcbResData)
                                                    *pcbResData = cbThis;
                                                fPtrFound = TRUE;
                                            }
                                        }
                                        else
                                        {
                                            free(pb);
                                            // stop reading, we have a problem
                                            break;
                                        }
                                    }
                                }

                                if (fPtrFound || arc)
                                    break;

                            } // end for
                        }

                        if (paNameInfos)
                            free(paNameInfos);

                        // since this was the type we were looking
                        // for, stop in any case
                        break;  // while

                    } // end if our type
                }
            } // end if (!(arc = DosRead(pExec->hfExe,
        } // end while (!arc)

        if ((!fPtrFound) && (!arc))
            arc = ERROR_NO_DATA;
    }

    return (arc);
}

typedef unsigned short  WORD;
typedef unsigned long   DWORD;

typedef CHAR           *LPSTR;
typedef const CHAR     *LPCSTR;

// typedef unsigned short  WCHAR;
typedef WCHAR          *LPWSTR;
typedef WCHAR          *PWSTR;
typedef const WCHAR    *LPCWSTR;
typedef const WCHAR    *PCWSTR;

// #define LOBYTE(w)              ((BYTE)(WORD)(w))
// #define HIBYTE(w)              ((BYTE)((WORD)(w) >> 8))

#define LOWORD(l)              ((WORD)(DWORD)(l))
#define HIWORD(l)              ((WORD)((DWORD)(l) >> 16))

#define SLOWORD(l)             ((INT16)(LONG)(l))
#define SHIWORD(l)             ((INT16)((LONG)(l) >> 16))

// #define MAKEWORD(low,high)     ((WORD)(((BYTE)(low)) | ((WORD)((BYTE)(high))) << 8))
// #define MAKELONG(low,high)     ((LONG)(((WORD)(low)) | (((DWORD)((WORD)(high))) << 16)))
// #define MAKELPARAM(low,high)   ((LPARAM)MAKELONG(low,high))
// #define MAKEWPARAM(low,high)   ((WPARAM)MAKELONG(low,high))
// #define MAKELRESULT(low,high)  ((LRESULT)MAKELONG(low,high))
// #define MAKEINTATOM(atom)      ((LPCSTR)MAKELONG((atom),0))

/*
 *@@ GetResDirEntryW:
 *      helper function, goes down one level of PE resource
 *      tree.
 *
 *@@added V0.9.16 (2002-01-09) [umoeller]
 */

PIMAGE_RESOURCE_DIRECTORY GetResDirEntryW(PIMAGE_RESOURCE_DIRECTORY resdirptr,
                                          LPCWSTR name,
                                          ULONG root,
                                          BOOL allowdefault)
{
    int entrynum;
    PIMAGE_RESOURCE_DIRECTORY_ENTRY entryTable;
    int namelen;

    if (HIWORD(name))
    {
        // to make this work we'd have to copy the entire
        // Odin codepage management which I do _not_ want
        /*
        if (name[0] == '#')
        {
            char    buf[10];

            lstrcpynWtoA(buf,
                         name + 1,
                         10);
            return GetResDirEntryW(resdirptr,
                                   (LPCWSTR)atoi(buf),
                                   root,
                                   allowdefault);
        }
        entryTable = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)(   (BYTE*)resdirptr
                                                        + sizeof(IMAGE_RESOURCE_DIRECTORY));
        namelen = lstrlenW(name);
        for (entrynum = 0;
             entrynum < resdirptr->NumberOfNamedEntries;
             entrynum++)
        {
            PIMAGE_RESOURCE_DIR_STRING_U str
                = (PIMAGE_RESOURCE_DIR_STRING_U)(   root
                                                  + entryTable[entrynum].u1.s.NameOffset);
            if (namelen != str->Length)
                continue;
            if (lstrncmpiW(name,
                           str->NameString,
                           str->Length)==0)
                    return (PIMAGE_RESOURCE_DIRECTORY)(   root
                                                        + entryTable[entrynum].u2.s.OffsetToDirectory);
        }
        */
    }
    else
    {
        entryTable
            = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)
                    (
                        (BYTE*)resdirptr
                      + sizeof(IMAGE_RESOURCE_DIRECTORY)
                      +   resdirptr->NumberOfNamedEntries
                        * sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY)
                    );
        for (entrynum = 0;
             entrynum < resdirptr->NumberOfIdEntries;
             entrynum++)
        {
            if ((DWORD)entryTable[entrynum].u1.Name == (DWORD)name)
                return (PIMAGE_RESOURCE_DIRECTORY)(   root
                                                    + entryTable[entrynum].u2.s.OffsetToDirectory);
        }
        // just use first entry if no default can be found
        if (allowdefault && !name && resdirptr->NumberOfIdEntries)
            return (PIMAGE_RESOURCE_DIRECTORY)(   root
                                                + entryTable[0].u2.s.OffsetToDirectory);
    }

    return NULL;
}

/*
 *@@ LoadRootResDirectory:
 *
 *@@added V0.9.16 (2002-01-09) [umoeller]
 */

APIRET LoadRootResDirectory(PEXECUTABLE pExec,
                            PIMAGE_SECTION_HEADER paSections,
                            PIMAGE_RESOURCE_DIRECTORY *ppResDir,    // out: new directory
                            PULONG pcbResDir)           // out: size
{
    APIRET arc = ERROR_NO_DATA;     // unless found

    PPEHEADER pPEHeader = pExec->pPEHeader;

    // address to find is address specified in resource data directory
    ULONG ulAddressFind = pPEHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].VirtualAddress;

    int i;

    _Pmpf((__FUNCTION__ ": entering, %d sections, looking for 0x%lX",
                pPEHeader->FileHeader.usNumberOfSections,
                ulAddressFind));

    for (i = 0;
         i < pPEHeader->FileHeader.usNumberOfSections;
         i++)
    {
        PIMAGE_SECTION_HEADER pThis = &paSections[i];

        _Pmpf(("    %d (%s): virtual address 0x%lX",
                i,
                pThis->Name,
                pThis->VirtualAddress));

        if (pThis->flCharacteristics & IMAGE_SCN_CNT_UNINITIALIZED_DATA)
            continue;

        // FIXME: doesn't work when the resources are not in a seperate section
        if (pThis->VirtualAddress == ulAddressFind)
        {
            // rootresdir = (PIMAGE_RESOURCE_DIRECTORY)(   (char*)pExec->pDosExeHeader
               //                                        + pThis->ulPointerToRawData);

            ULONG cb = pThis->ulSizeOfRawData; // sizeof(IMAGE_RESOURCE_DIRECTORY);
            PIMAGE_RESOURCE_DIRECTORY pResDir;

            _Pmpf((__FUNCTION__ ": raw data size %d, ptr %d",
                    pThis->ulSizeOfRawData,
                    pThis->ulPointerToRawData,
                    sizeof(IMAGE_RESOURCE_DIRECTORY)));

            if (!(pResDir = malloc(cb)))
                arc = ERROR_NOT_ENOUGH_MEMORY;
            else
            {
                // load that
                if (!(arc = doshReadAt(pExec->pFile,
                                       // offset:
                                       pThis->ulPointerToRawData,
                                       &cb,
                                       (PBYTE)pResDir,
                                       DRFL_FAILIFLESS)))
                {
                    *ppResDir = pResDir;
                    *pcbResDir = cb;
                }
                else
                    free(pResDir);
            }

            break;
        }
    }

    _Pmpf((__FUNCTION__": returning %d", arc));

    return (arc);
}

#define MAKEINTRESOURCEW(i) (LPWSTR)((DWORD)((WORD)(i)))
#define RT_ICONW            MAKEINTRESOURCEW(3)
#define RT_GROUP_ICONW      MAKEINTRESOURCEW(14)

/*
 *@@ LoadResData:
 *
 *@@added V0.9.16 (2002-01-09) [umoeller]
 */

APIRET LoadResData(PPEHEADER pPEHeader,
                   PIMAGE_SECTION_HEADER paSections,
                   PIMAGE_RESOURCE_DIRECTORY pRootResDir,
                   PIMAGE_RESOURCE_DIRECTORY icongroupresdir,
                   int iInIconDir,               // current index
                   int iconDirCount,
                   int idResource)
{
    APIRET          arc = NO_ERROR;

    /* PBYTE           idata,
                    igdata; */

    return (arc);
}

/*
 *@@ LoadWinPEResource:
 *
 *@@added V0.9.16 (2002-01-09) [umoeller]
 */

APIRET LoadWinPEResource(PEXECUTABLE pExec,     // in: executable from exehOpen
                         ULONG ulType,          // in: RT_* type (e.g. RT_POINTER)
                         int idResource,      // in: resource ID or 0 for first
                         PBYTE *ppbResData,     // out: converted resource data (to be free()'d)
                         PULONG pcbResData)     // out: size of converted data (ptr can be NULL)
{
    APIRET          arc = NO_ERROR;
    ULONG           cbRead;

    ULONG           ulNewHeaderOfs = 0; // V0.9.12 (2001-05-03) [umoeller]

    PPEHEADER       pPEHeader;

    PXFILE          pFile;

    PIMAGE_SECTION_HEADER
                    paSections = NULL;
    ULONG           cbSections;

    PIMAGE_RESOURCE_DIRECTORY
                    pRootResDir = NULL;
    ULONG           cbRootResDir;
    PIMAGE_RESOURCE_DIRECTORY
                    icongroupresdir = NULL;

    if (!(pPEHeader = pExec->pPEHeader))
        return (ERROR_INVALID_EXE_SIGNATURE);
    if (    (pExec->ulOS != EXEOS_WIN32_GUI)
         && (pExec->ulOS != EXEOS_WIN32_CLI)
       )
        return (ERROR_INVALID_EXE_SIGNATURE);

    if (pExec->pDosExeHeader)
        // executable has DOS stub: V0.9.12 (2001-05-03) [umoeller]
        ulNewHeaderOfs = pExec->pDosExeHeader->ulNewHeaderOfs;

    pFile = pExec->pFile;

    // read in section headers right after PE header
    if (    (!(arc = doshAllocArray(pPEHeader->FileHeader.usNumberOfSections,
                                    sizeof(IMAGE_SECTION_HEADER),
                                    (PBYTE*)&paSections,
                                    &cbSections)))
         && (!(arc = doshReadAt(pFile,
                                // right after PE header
                                ulNewHeaderOfs + sizeof(PEHEADER), // pExec->cbPEHeader,
                                &cbSections,
                                (PBYTE)paSections,
                                DRFL_FAILIFLESS)))
        // paSections = (PIMAGE_SECTION_HEADER)(   ((char*)pPEHeader)
           //                             + sizeof(*pPEHeader));
                              // probably makes problems with short PE headers...
         && (!(arc = LoadRootResDirectory(pExec,
                                          paSections,
                                          &pRootResDir,
                                          &cbRootResDir)))
       )
    {
        // search the group icon dir
        if (!(icongroupresdir = GetResDirEntryW(pRootResDir,
                                                RT_GROUP_ICONW,
                                                (ULONG)pRootResDir,
                                                FALSE)))
            arc = ERROR_NO_DATA; // WARN("No Icongroupresourcedirectory!\n");
        else
        {
            // alright, found the icon group dir:
            ULONG iconDirCount =   icongroupresdir->NumberOfNamedEntries
                                 + icongroupresdir->NumberOfIdEntries;

            int     ulIconGroup = 0;        // n

            // res dir for icon groups follows
            PIMAGE_RESOURCE_DIRECTORY_ENTRY
                    pResDirEntryThis = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)(icongroupresdir + 1);

            BOOL fPtrFound = FALSE;

            _Pmpf(("  found RT_GROUP_ICONW, %d names, %d ids",
                    icongroupresdir->NumberOfNamedEntries,
                    icongroupresdir->NumberOfIdEntries));

            // go thru icon group
            for (ulIconGroup = 0;
                 ((ulIconGroup < iconDirCount) && (pResDirEntryThis)) && (!arc);
                 ulIconGroup++, pResDirEntryThis++)
            {
                _Pmpf(("  %d: idThis: %d, ofs to data 0x%lX",
                        ulIconGroup,
                        pResDirEntryThis->u1.Id,
                        pResDirEntryThis->u2.OffsetToData));

                if (    (idResource == 0)       // first one found
                     || (pResDirEntryThis->u1.Id == idResource)
                   )
                {
                    if (ulIconGroup >= iconDirCount) // idResource %d is larger than iconDirCount %d\n",idResource,iconDirCount);
                        arc = ERROR_NO_DATA;
                    else
                    {
                        // found icon:
                        PIMAGE_RESOURCE_DATA_ENTRY
                                        idataent,
                                        igdataent;
                        PIMAGE_RESOURCE_DIRECTORY_ENTRY
                                        pXResDirEntry;
                        int             i,
                                        j;

                        pXResDirEntry = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)(icongroupresdir + 1);

                        if (ulIconGroup <= iconDirCount - idResource)   // assure we don't get too much ...
                        {
                            pXResDirEntry = pXResDirEntry + idResource;     // starting from specified index ...

                            _Pmpf(("    found icondir for id %d", pResDirEntryThis->u1.Id));

                            for (i = 0;
                                 i < ulIconGroup;
                                 i++, pXResDirEntry++)
                            {
                                PIMAGE_RESOURCE_DIRECTORY   resdir;

                                // go down this resource entry, name
                                resdir = (PIMAGE_RESOURCE_DIRECTORY)(  (ULONG)pRootResDir
                                                                      +(pXResDirEntry->u2.s.OffsetToDirectory));

                                // default language (0)
                                resdir = GetResDirEntryW(resdir,
                                                         0,
                                                         (ULONG)pRootResDir,
                                                         TRUE);
                                igdataent = (PIMAGE_RESOURCE_DATA_ENTRY)resdir;

                                // lookup address in mapped image for virtual address
                                // igdata = NULL;

                                for (j = 0;
                                     j < pPEHeader->FileHeader.usNumberOfSections;
                                     j++)
                                {
                                    ULONG ulOfs;
                                    PIMAGE_RESOURCE_DIRECTORY iconresdir = NULL;

                                    if (igdataent->OffsetToData < paSections[j].VirtualAddress)
                                        continue;
                                    if (   igdataent->OffsetToData + igdataent->Size
                                         >   paSections[j].VirtualAddress
                                           + paSections[j].ulSizeOfRawData)
                                        continue;

                                    ulOfs = //  (PBYTE)pExec->pDosExeHeader +
                                            (   igdataent->OffsetToData
                                              - paSections[j].VirtualAddress
                                              + paSections[j].ulPointerToRawData
                                            );

                                    _Pmpf(("    data of this icon group is at 0x%lX", ulOfs));

                                    /* RetPtr[i] = (HICON)pLookupIconIdFromDirectoryEx(igdata,
                                                                                    TRUE,
                                                                                    cxDesired,
                                                                                    cyDesired,
                                                                                    LR_DEFAULTCOLOR);
                                    */

                                    /*
                                    if (iconresdir = GetResDirEntryW(pRootResDir,
                                                                     RT_ICONW,
                                                                     (ULONG)pRootResDir,
                                                                     FALSE))
                                    {
                                        for (i2 = 0;
                                             (i2 < ulIconGroup) && (!arc);
                                             i2++)
                                        {
                                            PIMAGE_RESOURCE_DIRECTORY   xresdir;
                                            xresdir = GetResDirEntryW(iconresdir,
                                                                      (LPWSTR)(ULONG)RetPtr[i2],
                                                                      (ULONG)pRootResDir,
                                                                      FALSE);
                                            xresdir = GetResDirEntryW(xresdir,
                                                                      (LPWSTR)0,
                                                                      (ULONG)pRootResDir,
                                                                      TRUE);
                                            idataent = (PIMAGE_RESOURCE_DATA_ENTRY)xresdir;

                                            // map virtual to address in image
                                            for (j2 = 0;
                                                 j2 < pPEHeader->FileHeader.usNumberOfSections;
                                                 j2++)
                                            {
                                                ULONG ulDataOfs;
                                                if (idataent->OffsetToData < paSections[j].VirtualAddress)
                                                    continue;
                                                if (idataent->OffsetToData+idataent->Size > paSections[j].VirtualAddress+paSections[j].SizeOfRawData)
                                                    continue;
                                                ulDataOfs =
                                                          (   idataent->OffsetToData
                                                            - paSections[j].VirtualAddress
                                                            + paSections[j].ulPointerToRawData
                                                          );
                                                RetPtr[i] = (HICON)pCreateIconFromResourceEx(idata,
                                                                                             idataent->Size,
                                                                                             TRUE,
                                                                                             0x00030000,
                                                                                             cxDesired,
                                                                                             cyDesired,
                                                                                             LR_DEFAULTCOLOR);
                                            }
                                        } // for i = 0
                                    }

                                    hRet = RetPtr[0]; // return first icon
                                    break;
                                    */

                                    break;
                                }
                            }
                        }
                    } // else if (n >= iconDirCount)

                    if (fPtrFound)
                        break;

                } // if idResource

            } // while
        }

        if (paSections)
            free(paSections);
        if (pRootResDir)
            free(pRootResDir);
    }

    arc = ERROR_NO_DATA;

    return (arc);
}

/*
 *@@ icoLoadExeIcon:
 *      smarter replacement for WinLoadFileIcon.
 *      In conjunction with the exeh* functions,
 *      this is a full rewrite, including all the
 *      executable parsing.
 *
 *      Essentially, this function allows you to
 *      get an icon resource from an executable
 *      file without having to run DosLoadModule.
 *      This is used in XWorkplace for getting the
 *      icons for executable files.
 *
 *      Differences to WinLoadFileIcon:
 *
 *      --  WinLoadFileIcon can take ages on PE files.
 *
 *      --  WinLoadFileIcon _always_ returns an icon,
 *          which makes it unsuitable for our
 *          wpSetProgIcon replacements because we'd
 *          rather replace the default icons and we
 *          can't find out using WinLoadFileIcon.
 *
 *      --  This is intended for executables _only_.
 *          It does not check for an .ICO file in the
 *          same directory, nor will this check for
 *          .ICON EAs.
 *
 *      --  This takes an EXECUTABLE from exehOpen
 *          as input. As a result, only executables
 *          supported by exehOpen are supported.
 *
 *      Presently the following executable and icon
 *      resource formats are understood:
 *
 *      1)  OS/2 LX and NE (via exehLoadLXResource
 *          and exehLoadOS2NEResource);
 *
 *      2)  Win16 NE, but only 32x32 icons in 16 colors.
 *
 *      See icoLoadICOFile for the meaning of
 *      the phptr, pcbIconData, and pbIconData
 *      parameters.
 *
 *      This returns:
 *
 *      --  NO_ERROR: output data was set. See
 *          icoLoadICOFile.
 *
 *      --  ERROR_INVALID_EXE_SIGNATURE: cannot handle
 *          this EXE format.
 *
 *      --  ERROR_NO_DATA: EXE format understood, but
 *          the EXE contains no icon resources.
 *
 *      --  ERROR_NOT_ENOUGH_MEMORY
 *
 *      --  ERROR_INVALID_PARAMETER: pExec is
 *          invalid, or pcbIconData was NULL while
 *          pbIconData was != NULL.
 *
 *      --  ERROR_BUFFER_OVERFLOW: *pcbIconData is too
 *          small a size of pbIconData.
 *
 *      plus the error codes of exehOpen and icoBuildPtrHandle.
 *
 *@@added V0.9.16 (2001-12-08) [umoeller]
 */

APIRET icoLoadExeIcon(PEXECUTABLE pExec,        // in: EXECUTABLE from exehOpen
                      ULONG idResource,         // in: resource ID or 0 for first
                      HPOINTER *phptr,          // out: if != NULL, newly built HPOINTER
                      PULONG pcbIconData,       // in/out: if != NULL, size of buffer required
                      PBYTE pbIconData)         // out: if != NULL, icon data that was loaded
{
    APIRET  arc;
    PBYTE   pbData = NULL;
    ULONG   cbData = 0;

    static  s_fCrashed = FALSE;

    if (s_fCrashed)
        return ERROR_PROTECTION_VIOLATION;

    TRY_LOUD(excpt1)
    {
        if (!pExec)
            return ERROR_INVALID_PARAMETER;

        // check the executable type
        switch (pExec->ulExeFormat)
        {
            case EXEFORMAT_LX:
                // these two we can handle for now
                arc = exehLoadLXResource(pExec,
                                         RT_POINTER,
                                         idResource,
                                         &pbData,
                                         &cbData);
            break;

            case EXEFORMAT_NE:
                switch (pExec->ulOS)
                {
                    case EXEOS_OS2:
                        arc = exehLoadOS2NEResource(pExec,
                                                    RT_POINTER,
                                                    idResource,
                                                    &pbData,
                                                    &cbData);
                        if (arc)
                            _Pmpf((__FUNCTION__ ": LoadOS2NEResource returned %d", arc));
                    break;

                    case EXEOS_WIN16:
                    case EXEOS_WIN386:
                        arc = LoadWinNEResource(pExec,
                                                WINRT_ICON,
                                                idResource,
                                                &pbData,
                                                &cbData);
                        if (arc)
                            _Pmpf((__FUNCTION__ ": LoadWinNEResource returned %d", arc));
                    break;

                    default:
                        arc = ERROR_INVALID_EXE_SIGNATURE;
                }
            break;

            case EXEFORMAT_PE:
                arc = LoadWinPEResource(pExec,
                                        WINRT_ICON,
                                        idResource,
                                        &pbData,
                                        &cbData);
                _Pmpf((__FUNCTION__ ": LoadWinPEResource returned %d", arc));
            break;

            default:        // includes COM, BAT, CMD
                arc = ERROR_INVALID_EXE_SIGNATURE;
            break;
        }

        // output data
        if (!arc)
        {
            if (pbData && cbData)
            {
                if (phptr)
                    arc = icoBuildPtrHandle(pbData,
                                            phptr);

                if (pbIconData)
                    if (!pcbIconData)
                        arc = ERROR_INVALID_PARAMETER;
                    else if (*pcbIconData < cbData)
                        arc = ERROR_BUFFER_OVERFLOW;
                    else
                        memcpy(pbIconData,
                               pbData,
                               cbData);

                if (pcbIconData)
                    *pcbIconData = cbData;
            }
            else
                arc = ERROR_NO_DATA;
        }
    }
    CATCH(excpt1)
    {
        s_fCrashed = TRUE;      // never let this code do anything again
        arc = ERROR_PROTECTION_VIOLATION;
    } END_CATCH();

    if (pbData)
        free(pbData);

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
 *      plus those of doshMalloc and DosGetResource, such as
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

static CONTROLDEF
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

static const DLGHITEM dlgObjIconFront[] =
    {
        START_TABLE,            // root table, required
    };

static const DLGHITEM dlgObjIconTitle[] =
    {
            START_ROW(ROW_VALIGN_TOP),       // row 1 in the root table, required
                CONTROL_DEF(&TitleText),
                CONTROL_DEF(&TitleEF)
    };

static const DLGHITEM dlgObjIconIcon[] =
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

static const DLGHITEM dlgObjIconExtrasFront[] =
    {
            START_ROW(ROW_VALIGN_CENTER),
                START_GROUP_TABLE(&ExtrasGroup),
                    START_ROW(0)
    };

static const DLGHITEM dlgObjIconHotkey[] =
    {
                        START_ROW(ROW_VALIGN_CENTER),
                            CONTROL_DEF(&HotkeyText),
                            CONTROL_DEF(&HotkeyEF),
                            CONTROL_DEF(&HotkeySetButton),
                            CONTROL_DEF(&HotkeyClearButton),
                        START_ROW(ROW_VALIGN_CENTER)
    };

static const DLGHITEM dlgObjIconTemplate[] =
    {
                            CONTROL_DEF(&TemplateCB)
    };

static const DLGHITEM dlgObjIconLockedInPlace[] =
    {
                            CONTROL_DEF(&LockPositionCB)
    };

static const DLGHITEM dlgObjIconExtrasTail[] =
    {
                END_TABLE
    };

static const DLGHITEM dlgObjIconDetails[] =
    {
                CONTROL_DEF(&DetailsButton)
    };

static const DLGHITEM dlgObjIconTail[] =
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
                 APIRET arc,
                 PCSZ pcszContext)
{
    if (arc)
        cmnDosErrorMsgBox(pcnbp->hwndDlgPage,
                          NULL,
                          _wpQueryTitle(pcnbp->somSelf),
                          NULL,
                          arc,
                          pcszContext,
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

    PICONINFO pIconInfo = NULL;
    HPOINTER hptrOld = winhSetWaitPointer();

    PCSZ pcszContext = NULL;

    TRY_LOUD(excpt1)
    {
        pcszContext = "Context: loading icon data";
        if (!(arc = icoLoadIconData(pData->pcnbp->somSelf,
                                    pData->ulAnimationIndex,
                                    &pIconInfo)))
        {
            CHAR szTempFile[CCHMAXPATH];
            pcszContext = "Context: creating temp file name";
            if (!(arc = doshCreateTempFileName(szTempFile,
                                               NULL,        // use TEMP
                                               "WP!",       // prefix
                                               "ICO")))     // extension
            {
                PXFILE pFile;
                ULONG cb = pIconInfo->cbIconData;
                pcszContext = "Context: opening temp file for writing";
                if (!cb)
                    arc = ERROR_NO_DATA;
                else if (!(arc = doshOpen(szTempFile,
                                          XOPEN_READWRITE_NEW | XOPEN_BINARY,
                                          &cb,
                                          &pFile)))
                {
                    arc = doshWrite(pFile,
                                    pIconInfo->cbIconData,
                                    pIconInfo->pIconData);
                    doshClose(&pFile);
                }

                if (!arc)
                {
                    CHAR szIconEdit[CCHMAXPATH];
                    FILESTATUS3 fs3Old, fs3New;

                    // get the date/time of the file so we
                    // can check whether the file was changed
                    // by iconedit
                    pcszContext = "Context: finding ICONEDIT.EXE";
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
                        pcszContext = "Context: starting ICONEDIT.EXE";
                        if (happ = appQuickStartApp(szIconEdit,
                                                    PROG_DEFAULT,
                                                    szTempFile,
                                                    &ulExitCode))
                        {
                            pcszContext = "Context: setting new icon data";
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

            if (pIconInfo)
                free(pIconInfo);
        }
    }
    CATCH(excpt1)
    {
        arc = ERROR_PROTECTION_VIOLATION;
    } END_CATCH();

    WinSetPointer(HWND_DESKTOP, hptrOld);

    ReportError(pData->pcnbp, arc, pcszContext);
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
                        (APIRET)mp1,
                        NULL);
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

                    ReportError(pcnbp, arc, NULL);
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


