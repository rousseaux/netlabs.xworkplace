
/*
 *@@sourcefile fonts.c:
 *      XWorkplace font folder implementation.
 *
 *      This file is ALL new with V0.9.7.
 *
 *@@added V0.9.7 (2001-01-12) [umoeller]
 *@@header "config\fonts.h"
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

#define INCL_DOSEXCEPTIONS
#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#define INCL_DOSERRORS

#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WININPUT
#define INCL_WINMENUS
#define INCL_WINMLE
#define INCL_WINCOUNTRY
#define INCL_WINSHELLDATA
#define INCL_WINERRORS
#define INCL_WINSYS

#define INCL_GPIREGIONS
#define INCL_GPIPRIMITIVES
#define INCL_GPILCIDS
#define INCL_GPILOGCOLORTABLE
#include <os2.h>

// C library headers
#include <stdio.h>              // needed for except.h
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\gpih.h"               // GPI helper routines
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\threads.h"            // thread helpers
#include "helpers\winh.h"               // PM helper routines

// SOM headers which don't crash with prec. header files
#include "xfont.ih"
#include "xfontfile.ih"
#include "xfontobj.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\errors.h"              // private XWorkplace error codes
#include "shared\helppanels.h"          // all XWorkplace help panel IDs
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\notebook.h"            // generic XWorkplace notebook handling
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

#include "config\fonts.h"               // font folder implementation

#include "filesys\fileops.h"            // file operations implementation
#include "filesys\folder.h"             // XFolder implementation

// other SOM headers
#pragma hdrstop

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

// static const char   *G_pcszFontsApp = "PM_Fonts";

// static BOOL         G_fCreateFontObjectsThreadRunning = FALSE;
// static THREADINFO   G_ptiCreateFontObjects = {0};

#define WC_XWPFONTOBJ_SAMPLE     "XWPFontSampleClient"
static BOOL         G_fFontSampleClassRegistered = FALSE;

ULONG               G_ulFontSampleHints = 0;
                                    /* | HINTS_MAX_ASCENDER_DESCENDER_GRAYRECT
                                    | HINTS_INTERNALLEADING_GRAYRECT
                                    | HINTS_BASELINE_REDLINE
                                    | HINTS_LOWERCASEASCENT_REDRECT; */
                            // exported in fonts.h

typedef struct _HINTMENUITEM
{
    // const char  *pcszItemText;
    ULONG       ulStringID;
    ULONG       ulFlag;
} HINTMENUITEM, *PHINTMENUITEM;

static HINTMENUITEM G_aHintMenuItems[] =
        {
            ID_XSSI_FONT_BASELINE, // "Show baseline (red line)",
                                        HINTS_BASELINE_REDLINE,
            ID_XSSI_FONT_MAXASCENDER, // "Show maximum ascender/descender (gray rectangles on left)",
                                        HINTS_MAX_ASCENDER_DESCENDER_GRAYRECT,
            ID_XSSI_FONT_INTERNALLEADING, // "Show internal leading (gray rectangle)",
                                        HINTS_INTERNALLEADING_GRAYRECT,
            ID_XSSI_FONT_LOWERCASEASCENT, // "Show lower case ascent/descent (red rectangles)",
                                        HINTS_LOWERCASEASCENT_REDRECT
        };

// list of open views
static LINKLIST     G_llFontSampleViews;
                        // list of plain HWND's with the frame windows
                        // of all currently open sample views... this
                        // is not auto-free, of course.
static BOOL         G_fInitialized = FALSE;
                        // TRUE after first call... we must initialize the
                        // list.

/* ******************************************************************
 *
 *   General font management
 *
 ********************************************************************/

/*
 *@@ fonGetFontDescription:
 *      returns the font description (family and face)
 *      for the specified font filename. This calls
 *      GpiQueryFontFileDescriptions on the font file.
 *      (Yes, that function does work with fonts other
 *      than FON files... even though it's awful to use.)
 *
 *      NOTE: There's another API (GpiQueryFullFontFileDesc)
 *      which appears to be a bit more comfortable, but this
 *      doesn't work with TrueType fonts if FreeType/2 is
 *      installed.
 *
 *      We also cannot simply use GpiQueryFonts because that
 *      doesn't give us the font filenames.
 *
 *      If this returns NO_ERROR, pszFamily and pszFace
 *      receive the font's family and face name, respectively.
 *
 *      If the font file is invalid, this returns some OS/2
 *      error code (e.g. ERROR_FILE_NOT_FOUNT).
 *      This can be the error code from DosQueryPathInfo.
 *
 *      If the file does exist, but GPI is unable to return
 *      font information for it (e.g. because its format is
 *      not understood), this returns ERROR_BAD_FORMAT. If
 *      the file format is initially understood, but GPI
 *      still failed to return the info, ERROR_INVALID_DATA
 *      is returned (shouldn't happen).
 */

APIRET fonGetFontDescription(HAB hab,
                             PCSZ pcszFilename,  // in: font file (fully q'fied)
                             PSZ pszFamily,             // out: font's family
                             PSZ pszFace)               // out: font's face name
{
    LONG    cFonts = 0;
            // lBufLen = 0;
    PFFDESCS pffd = NULL;

    APIRET arc = doshQueryPathAttr(pcszFilename, NULL);
    if (arc == NO_ERROR)
    {
        // no error:
        // get size of mem block we need
        cFonts = GpiQueryFontFileDescriptions(hab,
                                              (PSZ)pcszFilename,
                                              &cFonts,
                                              NULL);
        if ((cFonts) && (cFonts != GPI_ALTERROR) )
        {
            ULONG cb = cFonts * sizeof(FFDESCS)
                            + 100; // for some reason, this crashes otherwise
            if (pffd = malloc(cb))
            {
                // ZERO the memory block... or we get garbage later
                memset(pffd, 0, cb);
                // get the font descriptions
                if (GpiQueryFontFileDescriptions(hab,
                                                 (PSZ)pcszFilename,
                                                 &cFonts,
                                                 pffd)
                        == 0)
                {
                    // for each font, GpiQueryFontFileDescriptions
                    // returns two CHAR arrays: the first one is the
                    // family, the second one is the face name, if any.

                    /* For TT fonts, we get something like this:
                        0000:            5A 61 70 66 43 61 6C 6C   ZapfCall
                        0008:            69 67 72 20 42 54 00 00   igr BT..
                        0010:            00 00 00 00 00 00 00 00   ........
                        0018:            00 00 00 00 00 00 00 00   ........
                        0020:            5A 61 70 66 20 43 61 6C   Zapf Cal
                        0028:            6C 69 67 72 61 70 68 69   ligraphi
                        0030:            63 20 38 30 31 20 49 74   c 801 It
                        0038:            61 6C 69 63 20 42 54 00   alic BT.
                        0040:            00 00 00 00 00 00 00 00   ........
                                ... rest is zeroed

                    Similarly, for Type1 fonts:

                        0000:            41 6D 65 72 69 63 61 6E   American
                        0008:            20 47 61 72 61 6D 6F 6E    Garamon
                        0010:            64 00 00 00 00 00 00 00   d.......
                        0018:            00 00 00 00 00 00 00 00   ........
                        0020:            41 6D 65 72 69 63 61 6E   American
                        0028:            20 47 61 72 61 6D 6F 6E    Garamon
                        0030:            64 00 00 00 00 00 00 00   d.......
                                ... rest is zeroed

                    By contrast, for FON files (bitmap fonts), this function
                    fills in plenty of garbage even AFTER the first two items.
                    We better just ignore that.
                    */

                    strcpy(pszFamily, (PSZ)pffd);

                    strcpy(pszFace, (PSZ)pffd + 0x20);

                }
                else
                    arc = ERROR_INVALID_DATA;

                free(pffd);

            } // end if (pffd)
            else
                // malloc failed:
                arc = ERROR_NOT_ENOUGH_MEMORY;
        } // end if ((cFonts) && (cFonts != GPI_ALTERROR) )
        else
            // error getting font descriptions:
            arc = ERROR_BAD_FORMAT;
    }

    return arc;
}

/*
 *@@ fonCreateFontObject:
 *      creates a new font object in the specified
 *      font folder.
 *
 *      If (pInsert != NULL), the new record is
 *      inserted at the specified position.
 */

XWPFontObject* fonCreateFontObject(XWPFontFolder *pFolder,
                                   PCSZ pcszFamily,   // in: family name
                                   PCSZ pcszFace,     // in: face name
                                   PCSZ pcszFilename, // in: fully q'fied file
                                   PCSZ pcszSetup,    // in: extra setup string or NULL
                                   BOOL fInsert)    // in: if != NULL, where to insert object
{
    XWPFontObject *pNew = NULL;

    if ((pFolder) && (_somIsA(pFolder, _XWPFontFolder)))
    {
        CHAR    szSetup[500];
        sprintf(szSetup,
                "FONTFILE=%s;FONTFAMILY=%s;%s",
                pcszFilename,
                pcszFamily,
                pcszSetup);     // extra setup

        pNew = _wpclsNew(_XWPFontObject,
                         (PSZ)pcszFace,          // object title
                         szSetup,
                         pFolder,
                         TRUE);             // lock
        if (pNew)
        {
            // raise status count... this is for the status bar
            WPSHLOCKSTRUCT Lock = {0};
            TRY_LOUD(excpt1)
            {
                if (LOCK_OBJECT(Lock, pFolder))
                {
                    XWPFontFolderData *somThis = XWPFontFolderGetData(pFolder);

                    _ulFontsCurrent++;
                    stbUpdate(pFolder);

                    if (fInsert)
                        fdrCnrInsertObject(pNew);
                }
            }
            CATCH(excpt1) {} END_CATCH();

            if (Lock.fLocked)
                _wpReleaseObjectMutexSem(Lock.pObject);
        }
    }

    return (pNew);
}

/*
 *@@ fonInstallFont:
 *      installs a new font on the system. The font is
 *      specified thru the XWPFontFile that it represents
 *      (i.e. the WPDataFile subclass).
 *
 *      This returns either a standard OS/2 error code
 *      or one of the new file engine error codes defined
 *      in fileops.h, which can be:
 *
 *      --  ERROR_BAD_FORMAT: font format not understood by OS/2.
 *
 *      --  FOPSERR_FONT_ALREADY_INSTALLED: font is already installed.
 *
 *      If pFontFolder has already been filled with font objects,
 *      this creates a new font object as well.
 */

APIRET fonInstallFont(HAB hab,
                      XWPFontFolder *pFontFolder,   // in: font folder to create font obj in
                      XWPFontFile *pNewFontFile,    // in: font file to install
                      WPObject **ppNewFontObj)      // out: new XWPFontObject created
                                                    // if NO_ERROR is returned; ptr can be NULL
{
    APIRET arc = NO_ERROR;

    if (    (pFontFolder)
         && (pNewFontFile)
         && (_somIsA(pFontFolder, _XWPFontFolder))
         && (_somIsA(pNewFontFile, _XWPFontFile))
       )
    {
        // OK, at least the parameters are valid.
        CHAR    szFilename[CCHMAXPATH];
        if (!_wpQueryFilename(pNewFontFile,
                              szFilename,
                              TRUE))
            arc = ERROR_INVALID_PARAMETER;
        else
        {
            // we got the filename in szFilename:
            // check if that font is already installed
            if (__get_fFontInstalled(pNewFontFile))
                arc = FOPSERR_FONT_ALREADY_INSTALLED;
            else
            {
                CHAR    szFamily[100],
                        szFace[100];
                // OK, get font information.
                arc = fonGetFontDescription(hab,
                                            szFilename,
                                            szFamily,
                                            szFace);
                if (arc == NO_ERROR)
                {
                    // LOAD THE FONT INTO THE SYSTEM
                    if (!GpiLoadPublicFonts(hab,
                                            szFilename))
                        arc = ERROR_BAD_FORMAT;
                    else
                    {
                        // font loaded:
                        // this remains there for the rest of this
                        // OS/2 boot (i.e. PM lifetime...)
                        // put it into OS2.INI for the next reboot
                        PSZ pLastBackslash = 0;

                        // OS2.INI wants it in upper case
                        WinUpper(hab,
                                 0,         // process cp
                                 0,         // standard country
                                 szFilename);
                        // extract short filename
                        pLastBackslash = strrchr(szFilename, '\\');
                        if (!pLastBackslash)
                            arc = ERROR_INVALID_NAME;
                        else
                        {
                            // write!
                            if (!PrfWriteProfileString(HINI_USER,
                                                       (PSZ)PMINIAPP_FONTS, // "PM_Fonts";
                                                       // key:
                                                       pLastBackslash + 1,
                                                       // data: full path spec
                                                       szFilename))
                                arc = ERROR_DEVICE_IN_USE;
                            else
                            {
                                // OK, we're done so far...

                                // if the font folder has been populated
                                // already, create XWPFontObject
                                if (__get_fFilledWithFonts(pFontFolder))
                                {
                                    // yes, already filled:
                                    XWPFontObject *pNew
                                        = fonCreateFontObject(pFontFolder,
                                                              szFamily,
                                                              szFace,
                                                              szFilename,
                                                              NULL,        // no extra
                                                              TRUE);        // insert
                                    if (ppNewFontObj)
                                        // caller wants font object:
                                        *ppNewFontObj = pNew;
                                }

                                // update font file's install state
                                __set_fFontInstalled(pNewFontFile, TRUE);
                            }
                        }
                    }
                }
            }
        }
    }
    else
        arc = ERROR_INVALID_PARAMETER;

    return arc;
}

/*
 *@@ fonDeInstallFont:
 *      deinstalls a font from the system. The font is
 *      specified thru the XWPFontObject that it represents
 *      (i.e. the WPTransient subclass).
 *
 *      This returns either a standard OS/2 error code
 *      or one of the new file engine error codes defined
 *      in fileops.h.
 *
 *      If this returns FOPSERR_FONT_STILL_IN_USE, the
 *      font has still been deleted. It will simply not
 *      be freed immediately.
 *
 *      Note that this does NOT destroy the font object.
 *      The caller should destroy this if this returns
 *      either NO_ERROR or FOPSERR_FONT_STILL_IN_USE.
 */

APIRET fonDeInstallFont(HAB hab,
                        XWPFontFolder *pFontFolder,
                        XWPFontObject *pFontObject)
{
    APIRET arc = NO_ERROR;

    if (    (pFontFolder)
         && (pFontObject)
         && (_somIsA(pFontFolder, _XWPFontFolder))
         && (_somIsA(pFontObject, _XWPFontObject))
       )
    {
        CHAR    szFilename[CCHMAXPATH];

        // close all open view of the font object...
        // otherwise we can't unload the font below
        _wpClose(pFontObject);

        if (!_xwpQueryFontFile(pFontObject,
                               szFilename))
            arc = ERROR_INVALID_PARAMETER;
        else
        {
            // extract short filename
            PSZ pLastBackslash = strrchr(szFilename, '\\');
            if (!pLastBackslash)
                arc = ERROR_INVALID_NAME;
            else
            {
                // check if the font is still in OS2.INI
                ULONG cb = 0;
                if (    (!PrfQueryProfileSize(HINI_USER,
                                              (PSZ)PMINIAPP_FONTS, // "PM_Fonts"
                                              pLastBackslash + 1,
                                              &cb))
                     || (cb == 0)
                   )
                    arc = FOPSERR_FONT_ALREADY_DELETED;
                else
                {
                    // remove the font from OS2.INI
                    PrfWriteProfileString(HINI_USER,
                                          (PSZ)PMINIAPP_FONTS, // "PM_Fonts"
                                          pLastBackslash + 1,
                                          // remove key:
                                          NULL);

                    if (_xwpQueryFontFileError(pFontObject) == NO_ERROR)
                    {
                        // font was successfully loaded:
                        // attempt to unload the font...
                        // this might fail if the font is still used
                        // somewhere, but will help after a reboot
                        if (!GpiUnloadPublicFonts(hab, szFilename))
                            arc = FOPSERR_FONT_STILL_IN_USE;
                    }
                    // else: the font probably isn't loaded in the
                    // first place, so return NO_ERROR
                }
            }
        }
    }

    return arc;
}

/* ******************************************************************
 *
 *   Font folder implementation
 *
 ********************************************************************/

/*
 *@@ fonPopulateFirstTime:
 *      implementation for XWPFontFolder::wpPopulate, which
 *      only gets called on the first populate though.
 *
 *      This was moved back to the populate thread with V0.9.9.
 *
 *@@added V0.9.9 (2001-02-06) [umoeller]
 */

VOID fonPopulateFirstTime(XWPFontFolder *pFolder)
{
    BOOL fFolderLocked = FALSE;

    TRY_LOUD(excpt1)
    {
        XWPFontFolderData *somThis = XWPFontFolderGetData(pFolder);
        // ULONG ulIndex = 0;
        HAB hab = WinQueryAnchorBlock(cmnQueryActiveDesktopHWND());
                            // how can we get the HAB of the populate thread?!?

        if (fFolderLocked = !fdrRequestFolderMutexSem(pFolder, SEM_INDEFINITE_WAIT))
        {
            APIRET arc = NO_ERROR;
            PSZ pszFontKeys = NULL;

            // reset counts
            _ulFontsMax = 0;
            _ulFontsCurrent = 0;

            if (!(arc = prfhQueryKeysForApp(HINI_USER,
                                            PMINIAPP_FONTS, // "PM_Fonts"
                                            &pszFontKeys)))
            {
                PSZ     pKey2 = pszFontKeys;

                // prevent the WPS from interfering here...
                _wpModifyFldrFlags(pFolder,
                                   FOI_POPULATEINPROGRESS | FOI_REFRESHINPROGRESS,
                                   FOI_POPULATEINPROGRESS | FOI_REFRESHINPROGRESS);

                // first count all the keys... we need this for the status bar
                while (*pKey2 != 0)
                {
                    _ulFontsMax++;
                    pKey2 += strlen(pKey2)+1;
                }

                // start over
                pKey2 = pszFontKeys;
                while (*pKey2 != 0)
                {
                    PSZ pszFilename;
                    if (pszFilename = prfhQueryProfileData(HINI_USER,
                                                           PMINIAPP_FONTS, // "PM_Fonts",
                                                           pKey2,
                                                           NULL))
                    {
                        // always retrieve first font only...
                        CHAR    szFamily[100] = "unknown";
                        CHAR    szTitle[200] = "unknown";
                        CHAR    szStatus[100] = "";       // font is OK

                        if (arc = fonGetFontDescription(hab,
                                                        pszFilename,
                                                        szFamily,
                                                        szTitle))    // face
                            // file doesn't exist:
                            // pass APIRET to object creation
                            sprintf(szStatus, "FONTFILEERROR=%d;", arc);

                        fonCreateFontObject(pFolder,
                                            szFamily,       // family
                                            szTitle,        // face
                                            pszFilename,     // file
                                            szStatus,      // extra setup string
                                            FALSE);        // no insert yet

                        free(pszFilename);
                    } // end if (pszFilename)

                    // next font
                    pKey2 += strlen(pKey2)+1;
                } // while (*pKey2 != 0)

                // done:
                _ulFontsMax = _ulFontsCurrent;

                free(pszFontKeys);

                // now insert all objects in one flush...
                if (_ulFontsCurrent)
                {
                    fdrInsertAllContents(pFolder);
                }

                _wpModifyFldrFlags(pFolder,
                                   FOI_POPULATEINPROGRESS | FOI_REFRESHINPROGRESS,
                                   0);
            }
        } // end if (fFolderLocked)
    }
    CATCH(excpt1)
    {
    } END_CATCH();

    if (fFolderLocked)
        fdrReleaseFolderMutexSem(pFolder);
}

/*
 *@@ FONTTHREADDATA:
 *
 */

/* typedef struct _FONTTHREADDATA
{
    XWPFontFolder       *pFontFolder;
} FONTTHREADDATA, *PFONTTHREADDATA; */

/*
 *@@ fnt_fonCreateFontObjects:
 *
 */

/* VOID fnt_fonCreateFontObjects(PTHREADINFO ptiMyself)
{
    PFONTTHREADDATA pFontThreadData = (PFONTTHREADDATA)ptiMyself->ulData;
    if (pFontThreadData)
    {

        free(pFontThreadData);
    } // end if (pFontThreadData)
} */

/*
 *@@ fonFillWithFontObjects:
 *      implementation for XWPFontFolder::wpPopulate.
 *      However, this only gets called when the font folder
 *      is populated for the first time.
 *
 *      This creates all the XWPFontObject instances according
 *      to the fonts which are installed.
 *
 *      To get the installed fonts, we go thru the "PM_Fonts"
 *      application in OS2.INI and call fonGetFontDescription
 */

/* BOOL fonFillWithFontObjects(XWPFontFolder *pFontFolder)
{
    BOOL brc = FALSE;

    PFONTTHREADDATA pFontThreadData = malloc(sizeof(FONTTHREADDATA));
    pFontThreadData->pFontFolder = pFontFolder;

    if (!G_fCreateFontObjectsThreadRunning)
        thrCreate(&G_ptiCreateFontObjects,
                  fnt_fonCreateFontObjects,
                  &G_fCreateFontObjectsThreadRunning,
                  THRF_PMMSGQUEUE | THRF_WAIT,
                  (ULONG)pFontThreadData);

    return brc;
} */

/*
 *@@ fonDragOver:
 *      implementation for XWPFontFolder::wpDragOver.
 *
 *      This only accepts instances of XWPFontFile over
 *      the font folder. As a result, we can easily
 *      check whether we accept the files being dragged.
 */

MRESULT fonDragOver(XWPFontFolder *pFontFolder,
                    PDRAGINFO pdrgInfo)
{
    USHORT      usDrop = DOR_NODROP,
                usDefaultOp = DO_LINK;
    ULONG       ulItemNow = 0;

    if (    (pdrgInfo->usOperation != DO_LINK)
         && (pdrgInfo->usOperation != DO_DEFAULT)
       )
    {
        usDrop = DOR_NODROPOP;      // invalid drop operation, but send msg again
    }
    else
    {
        // valid operation: set flag to drag OK first
        usDrop = DOR_DROP;

        // now go thru objects being dragged
        for (ulItemNow = 0;
             ulItemNow < pdrgInfo->cditem;
             ulItemNow++)
        {
            BOOL fThisValid = FALSE;
            DRAGITEM    drgItem;
            if (DrgQueryDragitem(pdrgInfo,
                                 sizeof(drgItem),
                                 &drgItem,
                                 ulItemNow))
            {
                WPObject *pObjDragged = NULL;
                if (wpshQueryDraggedObject(&drgItem,
                                           &pObjDragged))
                {
                    // we got the object:
                    // check if it's an XWPFontFile
                    if (_somIsA(pObjDragged, _XWPFontFile))
                    {
                        // yes:
                        // allow drag for now... we'll check the
                        // install state during drop, when we collect
                        // the objects. Disallowing drag here would
                        // be confusing to the user.
                        fThisValid = TRUE;
                    }
                }

                if (!fThisValid)
                {
                    // not acceptable:
                    usDrop = DOR_NEVERDROP; // do not send msg again
                    // and stop processing
                    break;
                }
            }
        }
    }

    // compose 2SHORT return value
    return MRFROM2SHORT(usDrop, usDefaultOp);
}

/*
 *@@ fonDrop:
 *      implementation for XWPFontFolder::wpDrop.
 *
 *
 *@@changed V0.9.19 (2002-06-12) [umoeller]: added error handling
 */

MRESULT fonDrop(XWPFontFolder *pFontFolder,
                PDRAGINFO pdrgInfo)
{
    MRESULT     mrc = (MRESULT)RC_DROP_ERROR;
    ULONG       cItems = 0;
    BOOL        fStartInstall = TRUE;
    ULONG       ulItemNow = 0;
    PLINKLIST   pllDroppedObjects = lstCreate(FALSE);   // no auto-free

    for (ulItemNow = 0;
         ulItemNow < pdrgInfo->cditem;
         ++ulItemNow)
    {
        DRAGITEM    drgItem;
        if (DrgQueryDragitem(pdrgInfo,
                             sizeof(drgItem),
                             &drgItem,
                             ulItemNow))
        {
            BOOL    fThisValid = FALSE;
            WPObject *pobjDropped = NULL;

            if (wpshQueryDraggedObject(&drgItem,
                                       &pobjDropped))
            {
                // we got the object:
                // again, check if it's an XWPFontFile
                // (we've done this in wpDragOver, but who knows)
                if (_somIsA(pobjDropped, _XWPFontFile))
                {
                    // OK so far: append to list
                    lstAppendItem(pllDroppedObjects,
                                  pobjDropped);
                    cItems++;
                    fThisValid = TRUE;
                }
            }

            // notify source of the success of
            // this operation (target rendering)
            WinSendMsg(drgItem.hwndItem,        // source
                       DM_ENDCONVERSATION,
                       (MPARAM)(drgItem.ulItemID),
                       (MPARAM)((fThisValid)
                            ? DMFL_TARGETSUCCESSFUL
                            : DMFL_TARGETFAIL));

            if (!fThisValid)
                fStartInstall = FALSE;
        }
    }

    if ((cItems) && (fStartInstall))
    {
        // OK:
        // start "move to trashcan" with the new list
        FOPSRET frc;
        _PmpfF(("starting XFT_INSTALLFONTS (%d items)", cItems));
        if (frc = fopsStartTaskFromList(XFT_INSTALLFONTS,
                                        NULLHANDLE,       // no anchor block, asynchronously
                                        NULL,             // source folder: not needed
                                        pFontFolder,      // target folder: font folder
                                        pllDroppedObjects))
            // added error msg V0.9.19 (2002-06-12) [umoeller]
            cmnErrorMsgBox(NULLHANDLE,
                           frc,
                           0,
                           MB_CANCEL,
                           TRUE);

        mrc = (MRESULT)RC_DROP_DROPCOMPLETE;
                // means: _all_ items have been processed,
                // and wpDrop should _not_ be called again
                // by the WPS for the next items, if any
    }

    // in any case, free the list
    lstFree(&pllDroppedObjects);

    return mrc;
}

/*
 *@@ fonProcessObjectCommand:
 *      implementation for XWPTrashCan::xwpProcessObjectCommand.
 */

BOOL fonProcessObjectCommand(WPFolder *somSelf,
                             USHORT usCommand,
                             HWND hwndCnr,
                             WPObject* pFirstObject,
                             ULONG ulSelectionFlags)
{
    BOOL brc = TRUE;        // default: processed

    LONG lMenuID2 = usCommand - cmnQuerySetting(sulVarMenuOffset);

    switch (lMenuID2)
    {
        case ID_XFMI_OFS_FONT_DEINSTALL:
            fopsStartFontDeinstallFromCnr(NULLHANDLE,  // no anchor block, asynchronously
                                          somSelf,  // source: font folder
                                          pFirstObject, // first source object
                                          ulSelectionFlags,
                                          hwndCnr,
                                          TRUE);        // confirm
        break;

        default:
        {
            // other: call parent method to find out
            // whether default processing should occur

            // manually resolve parent method
            somTD_XWPFontFolder_xwpProcessObjectCommand pxwpProcessObjectCommand;

            if (pxwpProcessObjectCommand
                = (somTD_XWPFontFolder_xwpProcessObjectCommand)wpshResolveFor(
                                                       somSelf,
                                                       _somGetParent(_XWPFontFolder),
                                                       "xwpProcessObjectCommand"))
                // let parent method return TRUE or FALSE
                brc = pxwpProcessObjectCommand(somSelf,
                                               usCommand,
                                               hwndCnr,
                                               pFirstObject,
                                               ulSelectionFlags);
        }
    }

    return brc;
}

/*
 *@@ fonSampleTextInitPage:
 *
 *@@added V0.9.9 (2001-03-27) [umoeller]
 */

VOID fonSampleTextInitPage(PNOTEBOOKPAGE pnbp,
                           ULONG flFlags)
{
    if (flFlags & CBI_INIT)
    {
        // backup old string for "Undo"; if not defined,
        // this returns NULL
        pnbp->pUser = prfhQueryProfileData(HINI_USER,
                                            INIAPP_XWORKPLACE,
                                            INIKEY_FONTSAMPLESTRING,
                                            NULL);
    }

    if (flFlags & CBI_SET)
    {
        PSZ psz = prfhQueryProfileData(HINI_USER,
                                       INIAPP_XWORKPLACE,
                                       INIKEY_FONTSAMPLESTRING,
                                       NULL);
        WinSetDlgItemText(pnbp->hwndDlgPage,
                          ID_FNDI_SAMPLETEXT_MLE,
                          ((psz) && strlen(psz))
                            ? psz
                            : "The Quick Brown Fox Jumps Over The Lazy Dog.");
        if (psz)
            free(psz);
    }
}

/*
 *@@ fonSampleTextItemChanged:
 *
 *@@added V0.9.9 (2001-03-27) [umoeller]
 */

MRESULT fonSampleTextItemChanged(PNOTEBOOKPAGE pnbp,
                                 ULONG ulItemID,
                                 USHORT usNotifyCode,
                                 ULONG ulExtra)
{
    MRESULT mrc = 0;

    switch (ulItemID)
    {
        case ID_FNDI_SAMPLETEXT_MLE:
            if (usNotifyCode == MLN_KILLFOCUS)
            {
                PSZ psz = winhQueryWindowText(pnbp->hwndControl);

                PrfWriteProfileString(HINI_USER,
                                      (PSZ)INIAPP_XWORKPLACE,
                                      (PSZ)INIKEY_FONTSAMPLESTRING,
                                      psz);

                if (psz)
                    free(psz);
            }
    }

    return mrc;
}

/* ******************************************************************
 *
 *   Font object implementation
 *
 ********************************************************************/

/*
 *@@ fonModifyFontPopupMenu:
 *      implementation for XWPFontObject::wpModifyPopupMenu.
 *
 *@@changed V0.9.19 (2002-05-07) [umoeller]: added NLS for menu item strings
 */

VOID fonModifyFontPopupMenu(XWPFontObject *somSelf,
                            HWND hwndMenu)
{
    XWPFontObjectData *somThis = XWPFontObjectGetData(somSelf);
    MENUITEM mi;
    ULONG ulOfs = cmnQuerySetting(sulVarMenuOffset);

    // get handle to the "Open" submenu in the
    // the popup menu
    if (winhQueryMenuItem(hwndMenu,
                          WPMENUID_OPEN,
                          TRUE,
                          &mi))
    {
        // mi.hwndSubMenu now contains "Open" submenu handle,
        // which we add items to now
        winhInsertMenuItem(mi.hwndSubMenu, MIT_END,
                           ulOfs + ID_XFMI_OFS_XWPVIEW,
                           cmnGetString(ID_XSSI_FONTSAMPLEVIEW),  // pszFontSampleView
                           MIS_TEXT, 0);
    }

    // insert separator
    winhInsertMenuSeparator(hwndMenu, MIT_END,
                            ulOfs+ ID_XFMI_OFS_SEPARATOR);

    // add "Deinstall..."
    winhInsertMenuItem(hwndMenu, MIT_END,
                       ulOfs + ID_XFMI_OFS_FONT_DEINSTALL,
                       cmnGetString(ID_XSSI_FONTDEINSTALL),  // pszFontDeinstall
                       MIS_TEXT, 0);

    if (_fShowingOpenViewMenu)
    {
        ULONG ul = 0;
        // insert separator
        winhInsertMenuSeparator(hwndMenu, MIT_END,
                                ulOfs + ID_XFMI_OFS_SEPARATOR);

        // context menu for open view:
        // add view hints submenu
        for (ul = 0;
             ul < sizeof(G_aHintMenuItems) / sizeof(G_aHintMenuItems[0]);
             ul++)
        {
            winhInsertMenuItem(hwndMenu,
                               MIT_END,
                               WPMENUID_USER + ul,
                               cmnGetString(G_aHintMenuItems[ul].ulStringID),
                                    // V0.9.19 (2002-05-07) [umoeller]
                               MIS_TEXT,
                               // set checked if flag is currently set
                               (G_ulFontSampleHints & G_aHintMenuItems[ul].ulFlag)
                                    ? MIA_CHECKED
                                    : 0
                              );
        }
    }
}

/*
 *@@ fonMenuItemSelected:
 *      implementation for XWPFontObject::wpMenuItemSelected.
 */

BOOL fonMenuItemSelected(XWPFontObject *somSelf,
                         ULONG ulMenuId)
{
    BOOL brc = FALSE;

    if (ulMenuId == cmnQuerySetting(sulVarMenuOffset) + ID_XFMI_OFS_XWPVIEW)
    {
        _wpViewObject(somSelf,
                      NULLHANDLE,
                      ulMenuId,
                      0);
        brc = TRUE;
    }
    else
        if (    (ulMenuId >= WPMENUID_USER)
             && (ulMenuId < WPMENUID_USER
                            + sizeof(G_aHintMenuItems) / sizeof(G_aHintMenuItems[0])
                )
           )
        {
            // hint menu item selected:
            // toggle flag
            ULONG ulFlag = G_aHintMenuItems[ulMenuId - WPMENUID_USER].ulFlag,
                  ulNewFlags = G_ulFontSampleHints;
            if (G_ulFontSampleHints & ulFlag)
                // flag currently set: unset
                ulNewFlags &= ~ulFlag;
            else
                // flag currently not set: set now
                ulNewFlags |= ulFlag;

            _xwpclsSetFontSampleHints(_XWPFontObject,
                                      ulNewFlags);
        }
    return brc;
}

/*
 *@@ fonMenuItemHelpSelected:
 *      implementation for XWPFontObject::wpMenuItemHelpSelected.
 */

BOOL fonMenuItemHelpSelected(XWPFontObject *somSelf,
                             ULONG ulMenuId)
{
    BOOL brc = FALSE;

    if (    (ulMenuId >= WPMENUID_USER)
         && (ulMenuId < WPMENUID_USER
                        + sizeof(G_aHintMenuItems) / sizeof(G_aHintMenuItems[0])
            )
       )
    {
        _wpDisplayHelp(somSelf,
                       ID_XSH_FONTSAMPLEHINTS,
                       (PSZ)cmnQueryHelpLibrary());
        brc = TRUE;
    }

    return brc;
}

/* ******************************************************************
 *
 *   Font object sample view
 *
 ********************************************************************/

/*
 *@@ FONTSAMPLEDATA:
 *      window data for open sample view. Stored in
 *      QWL_USER of the client.
 */

typedef struct _FONTSAMPLEDATA
{
    XWPFontObject       *somSelf;

    USEITEM             UseItem;            // use item; immediately followed by view item
    VIEWITEM            ViewItem;           // view item; ViewItem.handle has the frame HWND

    HPS                 hps;                // micro PS allocated at WM_CREATE
    LONG                lcidFont;           // font LCID we're representing here

    FONTMETRICS         FontMetrics;

    HWND                hwndContextMenu;

    PFNWP               pfnwpFrameOriginal; // orig frame wnd proc before subclassing

    ULONG               cxViewport,         // extensions of viewport (for scrollbars)
                        cyViewport;
    ULONG               xOfs,               // current ofs of win in viewport
                        yOfs;

    PSZ                 pszSampleText;
} FONTSAMPLEDATA, *PFONTSAMPLEDATA;

/*
 *@@ UpdateScrollBars:
 *      called on FontSamplePaint and WM_SIZE to update
 *      the client's scroll bars.
 */

static VOID UpdateScrollBars(PFONTSAMPLEDATA pWinData,
                             ULONG ulWinCX,
                             ULONG ulWinCY)
{
    // vertical
    winhUpdateScrollBar(WinWindowFromID(pWinData->ViewItem.handle,  // frame
                                        FID_VERTSCROLL),
                        ulWinCY,            // ulWinPels
                        pWinData->cyViewport,
                        pWinData->yOfs,     // ofs: top
                        FALSE);             // no auto-hide
    // horizontal
    winhUpdateScrollBar(WinWindowFromID(pWinData->ViewItem.handle,  // frame
                                        FID_HORZSCROLL),
                        ulWinCX,            // ulWinPels
                        pWinData->cxViewport,
                        pWinData->xOfs,     // ofs: left
                        FALSE);             // no auto-hide
}

/*
 *@@ FontSamplePaint:
 *      implementation for WM_PAINT.
 *
 *@@changed V0.9.16 (2001-09-29) [umoeller]: now painting small sizes first
 */

static VOID FontSamplePaint(HWND hwnd,
                            PFONTSAMPLEDATA pWinData)
{
    HPS  hps = NULLHANDLE;
    HRGN hrgnUpdate,
         hrgnOld;

    hrgnUpdate = GpiCreateRegion(pWinData->hps,
                                 0,
                                 NULL);
    if (hrgnUpdate)
        if (RGN_NULL != WinQueryUpdateRegion(hwnd, hrgnUpdate))
            hps = pWinData->hps;

    // since we're not using WinBeginPaint,
    // we must always validate the update region,
    // or we'll get bombed with WM_PAINT msgs
    WinValidateRect(hwnd,
                    NULL,
                    FALSE);

    if (hps)
    {
        // we got something to paint:
        RECTL       rclClient;
        static ULONG aulSizes[] =
                {
                    // paint small sizes first V0.9.16 (2001-09-29) [umoeller]
                    6,
                    8,
                    10,
                    12,
                    16,
                    24,
                    30,
                    36,
                    48,
                    72,
                    144
                };
        POINTL      ptlCurrent;
        ULONG       ul = 0;
        CHAR        szTemp[100];

        ULONG       ulMaxCX = 0,        // x extension
                    ulMaxCY = 0;        // y extension

        // set clip region to update rectangle we queried above...
        // speeds up painting, since we're not using WinBeginPaint
        GpiSetClipRegion(hps, hrgnUpdate, &hrgnOld);

        WinQueryWindowRect(hwnd, &rclClient);

        WinFillRect(hps,
                    &rclClient,
                    RGBCOL_WHITE);

        // set charset to lcid created in WM_CREATE
        GpiSetCharSet(hps, pWinData->lcidFont);

        // start at top... this is changed below according to font size
        ptlCurrent.y = rclClient.yTop;
        // add y ofs from scrollbar; we paint over the top of the window
        // if the scroller is down
        ptlCurrent.y += pWinData->yOfs;

        for (ul = 0;
             ul < sizeof(aulSizes) / sizeof(aulSizes[0]);
             ul++)
        {
            POINTL      ptl2;
            RECTL       rcl;
            FONTMETRICS fm;
            // set current point size from array
            gpihSetPointSize(hps, aulSizes[ul]);

            // get font metrics...
            GpiQueryFontMetrics(hps, sizeof(fm), &fm);

/*
        ptlCurrent.y now points to the top of the char box.

           ÉÍ ________________________________________________
           º
           º lExternalLeading, according to font designer. Apparently, this is always 0.
        ÉÍÍÈÍ ________________________________________________  Í»
        º                                                        º
        º    lInternalLeading (1)                                º
        ÈÍ                                                       º
       ÉÍÍÍÍ  ______________________                             º
       º                            ##                           º
 lLowerCaseAscent (2)                #    ##     ##              º lMaxAscender (1)
       º   ÉÍ _______________        #    # #   # #              º
       º   º                 ####    #    #  # #  #              º
       º   º                #    #   #    #   #   #              º
       º   º  lXHeight      #    #   #    #       #              º
       º   º                #    #   #    #       #              º
       º   º                #    #   #    #       #              º
       º   º  _______________#####__###___#_______#___ baseline Í»
       ÈÍÍ ÈÍ                    #                               º
                                 #                               º lMaxDescender
              ______________ ####______________________________ Í¼


        Annotations:

        (1) lMaxAscender is actually quite a bit larger than the highest ascender
        that is really used by font characters. The ascender that is really used
        should be calculated by subtracting lInternalLeading from lMaxAscender.

        (2) lLowerCaseAscent is unreliable for TrueType fonts, at least with the
        FreeType/2 engine.
*/

            // move down by external leading
            ptlCurrent.y -= fm.lExternalLeading;
            ulMaxCY += fm.lExternalLeading;
                            // apparently, this is 0 always

            // go to baseline now
            ptlCurrent.y -= fm.lMaxAscender;        // max above baseline
            ulMaxCY += fm.lMaxAscender;

            if (G_ulFontSampleHints & HINTS_MAX_ASCENDER_DESCENDER_GRAYRECT)
            {
                // paint gray rect for max ascender

                // restore solid pattern
                GpiSetPattern(hps,
                              PATSYM_DEFAULT);

                rcl.xLeft = 0 - pWinData->xOfs;
                rcl.xRight = 50 - pWinData->xOfs;
                rcl.yBottom = ptlCurrent.y;
                rcl.yTop = rcl.yBottom + fm.lMaxAscender;
                WinFillRect(hps,
                            &rcl,               // exclusive
                            0xE0E0E0);

                // paint gray rect for max descender
                rcl.yTop = rcl.yBottom;         // not included
                rcl.yBottom = rcl.yTop - fm.lMaxDescender;
                WinFillRect(hps,
                            &rcl,               // exclusive
                            0xC0C0C0);
            }

            GpiSetColor(hps, RGBCOL_RED);

            if (G_ulFontSampleHints & HINTS_BASELINE_REDLINE)
            {
                // draw red line for baseline
                ptl2.x = 0;                 // always use client, scroller doesn't matter
                ptl2.y = ptlCurrent.y;
                GpiMove(hps, &ptl2);
                ptl2.x = rclClient.xRight;
                GpiLine(hps, &ptl2);
            }

            if (G_ulFontSampleHints & HINTS_INTERNALLEADING_GRAYRECT)
            {
                // internal leading:
                GpiSetColor(hps,
                            0xC0C0C0);
                GpiSetPattern(hps,
                              PATSYM_VERT);
                rcl.xLeft = 0;              // always use client, scroller doesn't matter
                rcl.yBottom =   ptlCurrent.y        // baseline
                              + fm.lMaxAscender
                              - fm.lInternalLeading;
                rcl.xRight = rclClient.xRight - 1;
                rcl.yTop = rcl.yBottom + fm.lInternalLeading - 1;
                gpihBox(hps,
                        DRO_FILL,
                        &rcl);                  // inclusive
            }

            if (G_ulFontSampleHints & HINTS_LOWERCASEASCENT_REDRECT)
            {
                GpiSetColor(hps,
                            RGBCOL_RED);

                // lower case ascent:
                GpiSetPattern(hps,
                              PATSYM_DIAG1);
                rcl.xLeft = 0;              // always use client, scroller doesn't matter
                rcl.yBottom = ptlCurrent.y;         // baseline
                rcl.xRight = rclClient.xRight - 1;
                rcl.yTop = rcl.yBottom + fm.lLowerCaseAscent - 1;
                gpihBox(hps,
                        DRO_FILL,
                        &rcl);              // inclusive

                // lower case descent:
                GpiSetPattern(hps,
                              PATSYM_DIAG3);
                rcl.xLeft = 0;              // always use client, scroller doesn't matter
                rcl.yBottom =   ptlCurrent.y         // baseline
                              - fm.lLowerCaseDescent;
                rcl.xRight = rclClient.xRight - 1;
                rcl.yTop = ptlCurrent.y - 1;
                gpihBox(hps,
                        DRO_FILL,
                        &rcl);              // inclusive
            }

            // draw font sample at that baseline
            GpiSetColor(hps, RGBCOL_BLACK);
            ptlCurrent.x = 5 - pWinData->xOfs;
            GpiMove(hps, &ptlCurrent);
            sprintf(szTemp,
                    "%d pt: %s",
                    aulSizes[ul],
                    pWinData->pszSampleText);
            GpiCharString(hps, strlen(szTemp), (PSZ)szTemp);

            // current position is now at the right where the
            // next character would be drawn...
            // get that
            GpiQueryCurrentPosition(hps, &ptl2);
            if (ptl2.x + pWinData->xOfs > ulMaxCX)
                ulMaxCX = ptl2.x + pWinData->xOfs;

            // move down by max descender
            ptlCurrent.y -= fm.lMaxDescender;
            ulMaxCY += fm.lMaxDescender;

            // next loop will move down by external leading of
            // NEXT font
        }

        // print extra info in default font
        /* GpiSetCharSet(hps, LCID_DEFAULT);
        // move down
        ptlCurrent.x = 10;
        ptlCurrent.y = 10;
        // ptlCurrent.y -= 12 * 1.3;
        GpiMove(hps, &ptlCurrent);
        sprintf(szTemp, "xOfs: %d, yOfs: %d", pWinData->xOfs, pWinData->yOfs);
        GpiCharString(hps, strlen(szTemp), szTemp); */

        if (    (ulMaxCY != pWinData->cyViewport)
             || (ulMaxCX != pWinData->cxViewport)
           )
        {
            pWinData->cyViewport = ulMaxCY;
            pWinData->cxViewport = ulMaxCX;

            UpdateScrollBars(pWinData,
                             rclClient.xRight,
                             rclClient.yTop);
        }
    }

    if (hrgnUpdate)
    {
        GpiSetClipRegion(hps, NULLHANDLE, &hrgnOld);
        GpiDestroyRegion(hps, hrgnUpdate);
    }
}

/*
 *@@ HandleContextMenu:
 *      handles WM_CONTEXTMENU and WM_MENUEND for open
 *      views of Desktop objects.
 *
 *      On WM_CONTEXTMENU, this calls _wpDisplayMenu.
 *      Pass this in from your client window proc
 *      with hwnd being your client window handle.
 *
 *      On WM_MENUEND, this clears phwndContextMenu
 *      again if that msg was for the context menu.
 *      Note that WM_MENUEND will not come into
 *      your client window proc, but only to the frame...
 *      so you have to subclass your frame window proc
 *      as well and call this function from there.
 *
 *      Returns a proper MRESULT for that message, as
 *      defined in PMREF.
 */

static MRESULT HandleContextMenu(WPObject *somSelf,            // in: object with view
                                 HWND hwndClient,              // in: client window handle of view
                                                               // (must be FID_CLIENT)
                                 ULONG msg,                    // in: WM_CONTEXTMENU or WM_MENUEND
                                 MPARAM mp1,                   // in: message param 1 (just pass this in)
                                 MPARAM mp2,                   // in: message param 2 (just pass this in)
                                 BOOL *pfShowingOpenViewMenu,  // in/out: set to TRUE on context menu show
                                 HWND *phwndContextMenu)       // out: context menu window
{
    MRESULT mrc = 0;

    switch (msg)
    {
        case WM_CONTEXTMENU:
        {
            POINTL  ptlPopup;
            ptlPopup.x = SHORT1FROMMP(mp1);
            ptlPopup.y = SHORT2FROMMP(mp1);

            *pfShowingOpenViewMenu = TRUE;
            *phwndContextMenu
                    = _wpDisplayMenu(somSelf,
                                     // owner: must be the frame,
                                     // or the "open view" items are not added... sigh
                                     WinQueryWindow(hwndClient, QW_OWNER),
                                     // client: our client
                                     hwndClient,
                                     &ptlPopup,
                                     MENU_OPENVIEWPOPUP,
                                     0);
            mrc = (MPARAM)TRUE;     // processed
        }
        break;

        case WM_MENUEND:
            if (    (*phwndContextMenu)
                 && ( (HWND)mp2 == *phwndContextMenu)
               )
            {
                // context menu ending:
                *phwndContextMenu = NULLHANDLE;
                *pfShowingOpenViewMenu = FALSE;
            }
        break;
    }

    return mrc;
}

/*
 *@@ fon_fnwpFontSampleFrame:
 *      window proc for the subclassed frame.
 */

static MRESULT EXPENTRY fon_fnwpFontSampleFrame(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT             mrc = 0;
    PFONTSAMPLEDATA     pWinData = (PFONTSAMPLEDATA)WinQueryWindowPtr(hwnd, QWL_USER);

    switch (msg)
    {
        case WM_MENUEND:
        {
            XWPFontObjectData *somThis = XWPFontObjectGetData(pWinData->somSelf);
            mrc = HandleContextMenu(pWinData->somSelf,
                                    hwnd,
                                    msg, mp1, mp2,
                                    &_fShowingOpenViewMenu,
                                    &pWinData->hwndContextMenu);
        }
        break;

        default:
            mrc = pWinData->pfnwpFrameOriginal(hwnd, msg, mp1, mp2);
    }

    return mrc;
}

/*
 *@@ fon_fnwpFontSampleClient:
 *      window proc for the font "Sample" view client.
 */

static MRESULT EXPENTRY fon_fnwpFontSampleClient(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT             mrc = 0;
    PFONTSAMPLEDATA     pWinData = (PFONTSAMPLEDATA)WinQueryWindowPtr(hwnd, QWL_USER);

    switch (msg)
    {
        /*
         * WM_CREATE:
         *      mp1 has PFONTSAMPLEDATA pointer.
         */

        case WM_CREATE:
        {
            SIZEL   szlPage = {0, 0};
            HDC hdc = WinOpenWindowDC(hwnd);
            pWinData = (PFONTSAMPLEDATA)mp1;
            WinSetWindowPtr(hwnd, QWL_USER, mp1);

            pWinData->hps = GpiCreatePS(WinQueryAnchorBlock(hwnd),
                                        hdc,
                                        &szlPage, // use same page size as device
                                        PU_PELS | GPIT_MICRO | GPIA_ASSOC);
            if (pWinData->hps)
            {
                gpihSwitchToRGB(pWinData->hps);

                pWinData->lcidFont = gpihFindFont(pWinData->hps,
                                                  36,
                                                  FALSE,    // font face
                                                  _wpQueryTitle(pWinData->somSelf),
                                                  0,        // no format
                                                  &pWinData->FontMetrics);
                // GpiSetCharSet(pWinData->hps, pWinData->lcidFont);
            }
            else
                // error:
                mrc = (MPARAM)TRUE;
        }
        break;

        /*
         * WM_PAINT:
         *
         */

        case WM_PAINT:
            FontSamplePaint(hwnd, pWinData);
        break;

        case WM_CONTEXTMENU:
        case WM_MENUEND:
        {
            XWPFontObjectData *somThis = XWPFontObjectGetData(pWinData->somSelf);
            mrc = HandleContextMenu(pWinData->somSelf,
                                    hwnd,
                                    msg, mp1, mp2,
                                    &_fShowingOpenViewMenu,
                                    &pWinData->hwndContextMenu);
        }
        break;

        case WM_SIZE:
            UpdateScrollBars(pWinData,
                             SHORT1FROMMP(mp2),  // new cx
                             SHORT2FROMMP(mp2)   // new cy
                             );
        break;

        case WM_VSCROLL:
        case WM_HSCROLL:
        {
            RECTL rcl;
            ULONG id, ulExt;
            PULONG pulOfs;

            WinQueryWindowRect(hwnd, &rcl);

            if (msg == WM_VSCROLL)
            {
                id = FID_VERTSCROLL;
                pulOfs = &pWinData->yOfs;
                ulExt = pWinData->cyViewport;
            }
            else
            {
                id = FID_HORZSCROLL;
                pulOfs = &pWinData->xOfs;
                ulExt = pWinData->cxViewport;
            }

            winhHandleScrollMsg(hwnd,               // client to scroll
                                WinWindowFromID(pWinData->ViewItem.handle, // frame
                                                id),
                                pulOfs,
                                &rcl,
                                ulExt,
                                8,              // line steps
                                msg,
                                mp2);
        }
        break;

        /*
         * WM_CLOSE:
         *
         */

        case WM_CLOSE:
        {
            // destroy the frame, which in turn destroys us
            HWND hwndFrame = WinQueryWindow(hwnd, QW_OWNER);
            winhSaveWindowPos(hwndFrame,
                              HINI_USER,
                              INIAPP_XWORKPLACE,
                              INIKEY_FONTSAMPLEWNDPOS);
            WinDestroyWindow(hwndFrame);
        }
        break;

        /*
         * WM_HELP:
         *      display "view" help directly, instead
         *      of the "font object" default help.
         */

        case WM_HELP:
            _wpDisplayHelp(pWinData->somSelf,
                           ID_XSH_FONTSAMPLEVIEW,
                           (PSZ)cmnQueryHelpLibrary());
        break;

        /*
         * WM_DESTROY:
         *
         */

        case WM_DESTROY:
            if (pWinData)
            {
                if (pWinData->hps)
                {
                    if (pWinData->lcidFont)
                    {
                        _PmpfF(("GpiDeleteSetId"));
                        GpiSetCharSet(pWinData->hps, LCID_DEFAULT);
                        if (!GpiDeleteSetId(pWinData->hps, pWinData->lcidFont))
                            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                                   "GpiDeleteSetId failed with %d",
                                   WinGetLastError(WinQueryAnchorBlock(hwnd)));
                    }
                    GpiAssociate(pWinData->hps, NULLHANDLE);
                    _PmpfF(("GpiDestroyPS"));
                    if (!GpiDestroyPS(pWinData->hps))
                        cmnLog(__FILE__, __LINE__, __FUNCTION__,
                               "GpiDestroyPS failed with %d",
                               WinGetLastError(WinQueryAnchorBlock(hwnd)));
                }

                // remove ourselves from the global list
                if (krnLock(__FILE__, __LINE__, __FUNCTION__))
                {
                    lstRemoveItem(&G_llFontSampleViews,
                                  // item is the frame HWND
                                  (PVOID)(pWinData->ViewItem.handle));

                    krnUnlock();
                }

                // remove this window from the object's use list
                _wpDeleteFromObjUseList(pWinData->somSelf,
                                        &pWinData->UseItem);

                free(pWinData->pszSampleText);

                _wpFreeMem(pWinData->somSelf,
                           (PBYTE)pWinData);
            }
        break;

        default:
            mrc = WinDefWindowProc(hwnd, msg, mp1, mp2);
    }

    return mrc;
}

/*
 *@@ fonCreateFontSampleView:
 *      implementation for XWPFontObject::wpOpen. This
 *      creates the "Sample" view for the font object.
 *
 *@@changed V0.9.13 (2001-06-17) [umoeller]: fixed default window size
 *@@changed V0.9.16 (2001-10-30) [umoeller]: now giving the client focus
 */

HWND fonCreateFontSampleView(XWPFontObject *somSelf,
                             HAB hab,
                             ULONG ulView)
{
    HWND hwndNewView = NULLHANDLE;

    TRY_LOUD(excpt1)
    {
        // XWPFontObjectData *somThis = XWPFontObjectGetData(somSelf);

        if (!G_fFontSampleClassRegistered)
        {
            if (WinRegisterClass(hab,
                                 WC_XWPFONTOBJ_SAMPLE,
                                 fon_fnwpFontSampleClient,
                                 /* CS_SIZEREDRAW | CS_SYNCPAINT */0,
                                 sizeof(PFONTSAMPLEDATA)))
                G_fFontSampleClassRegistered = TRUE;
        }

        if (G_fFontSampleClassRegistered)
        {
            SWP swpFrame;
            HWND hwndClient = NULLHANDLE;
            PFONTSAMPLEDATA pData
                = (PFONTSAMPLEDATA)_wpAllocMem(somSelf,
                                               sizeof(FONTSAMPLEDATA),
                                               NULL);
            if (pData)
            {
                memset(pData, 0, sizeof(*pData));

                pData->somSelf = somSelf;

                if (!(pData->pszSampleText = prfhQueryProfileData(HINI_USER,
                                                            INIAPP_XWORKPLACE,
                                                            INIKEY_FONTSAMPLESTRING,
                                                            NULL)))
                    // no user data given: use a default here
                    pData->pszSampleText = strdup("The Quick Brown Fox Jumps Over The Lazy Dog.");

                swpFrame.x = 10;
                swpFrame.y = 10;
                swpFrame.cx = winhQueryScreenCX() - 20;     // fixed V0.9.13 (2001-06-17) [umoeller]
                swpFrame.cy = winhQueryScreenCY() - 20;     // fixed V0.9.13 (2001-06-17) [umoeller]
                swpFrame.hwndInsertBehind = HWND_TOP;
                swpFrame.fl = SWP_SIZE | SWP_MOVE | SWP_ZORDER;

                hwndNewView
                   = winhCreateStdWindow(HWND_DESKTOP,  // frame's parent
                                         &swpFrame,
                                         FCF_NOBYTEALIGN
                                            | FCF_TITLEBAR
                                            | FCF_SYSMENU
                                            | FCF_MINMAX
                                            | FCF_SIZEBORDER
                                            | FCF_VERTSCROLL
                                            | FCF_HORZSCROLL,
                                         WS_ANIMATE,    // frame style
                                         _wpQueryTitle(somSelf),
                                         0,             // resid
                                         WC_XWPFONTOBJ_SAMPLE, // client class
                                         WS_VISIBLE,    // client style
                                         0,             // frame ID
                                         pData,         // client control data
                                         &hwndClient);
                                               // out: client window
                if (hwndNewView)
                {
                    // PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();
                    // store win data in QWL_USER of the frame too
                    WinSetWindowPtr(hwndNewView, QWL_USER, pData);

                    // subclass frame
                    pData->pfnwpFrameOriginal = WinSubclassWindow(hwndNewView,
                                                                  fon_fnwpFontSampleFrame);

                    // add the use list item to the object's use list;
                    // this struct has been zeroed above

                    cmnRegisterView(somSelf,
                                    &pData->UseItem,
                                    ulView,
                                    hwndNewView,
                                    cmnGetString(ID_XSSI_FONTSAMPLEVIEW));

                    winhRestoreWindowPos(hwndNewView,
                                         HINI_USER,
                                         INIAPP_XWORKPLACE,
                                         INIKEY_FONTSAMPLEWNDPOS,
                                         SWP_SHOW | SWP_ZORDER | SWP_MOVE | SWP_SIZE | SWP_ACTIVATE);

                    // give focus to the client
                    // V0.9.16 (2001-10-30) [umoeller]
                    WinSetFocus(HWND_DESKTOP, hwndClient);

                    // add to global views list
                    if (krnLock(__FILE__, __LINE__, __FUNCTION__))
                    {
                        if (!G_fInitialized)
                        {
                            lstInit(&G_llFontSampleViews, FALSE);
                            G_fInitialized = TRUE;
                        }

                        lstAppendItem(&G_llFontSampleViews,
                                      (PVOID)hwndNewView);

                        krnUnlock();
                    }
                }
                else
                    free(pData);
            }
        }
    }
    CATCH(excpt1) {} END_CATCH();


    return (hwndNewView);
}

/*
 *@@ fonInvalidateAllOpenSampleViews:
 *      repaints all open sample views.
 *
 *      Returns the no. of samples repainted.
 */

ULONG fonInvalidateAllOpenSampleViews(VOID)
{
    ULONG ulrc = 0;

    if (krnLock(__FILE__, __LINE__, __FUNCTION__))
    {
        PLISTNODE pNode = lstQueryFirstNode(&G_llFontSampleViews);
        while (pNode)
        {
            HWND hwndFrame = (HWND)pNode->pItemData;
            HWND hwndClient = WinWindowFromID(hwndFrame, FID_CLIENT);
            if (hwndClient)
            {
                WinInvalidateRect(hwndClient, NULL, FALSE);
                ulrc++;
            }

            pNode = pNode->pNext;
        }

        krnUnlock();
    }

    return (ulrc);
}

