
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
#define INCL_DOSERRORS

#define INCL_WINWINDOWMGR
#define INCL_WINMENUS
#define INCL_WINCOUNTRY
#define INCL_WINSHELLDATA
#define INCL_WINERRORS

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
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

#include "config\fonts.h"               // font folder implementation

#include "filesys\fileops.h"            // file operations implementation
#include "filesys\folder.h"             // XFolder implementation

// other SOM headers
#pragma hdrstop

/* ******************************************************************
 *
 *   Declarations
 *
 ********************************************************************/

const char      *G_pcszFontsApp = "PM_Fonts";

BOOL            G_fCreateFontObjectsThreadRunning = FALSE;
THREADINFO      G_ptiCreateFontObjects = {0};

#define WC_XWPFONTOBJ_SAMPLE     "XWPFontSampleClient"
BOOL            G_fFontSampleClassRegistered = FALSE;

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
                             const char *pcszFilename,  // in: font file (fully q'fied)
                             PSZ pszFamily,             // out: font's family
                             PSZ pszFace)               // out: font's face name
{
    LONG    cFonts = 0,
            lBufLen = 0;
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
            pffd = malloc(cb);
            if (pffd)
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

    return (arc);
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
                                   const char *pcszFamily,   // in: family name
                                   const char *pcszFace,     // in: face name
                                   const char *pcszFilename, // in: fully q'fied file
                                   const char *pcszSetup,    // in: extra setup string or NULL
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
            WPSHLOCKSTRUCT Lock;
            if (wpshLockObject(&Lock, pFolder))
            {
                XWPFontFolderData *somThis = XWPFontFolderGetData(pFolder);

                _ulFontsCurrent++;
                fdrUpdateStatusBars(pFolder);

                if (fInsert)
                    fdrCnrInsertObject(pNew);
            }
            wpshUnlockObject(&Lock);
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
                                                       (PSZ)G_pcszFontsApp, // "PM_Fonts";
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

    return (arc);
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
                                              (PSZ)G_pcszFontsApp, // "PM_Fonts"
                                              pLastBackslash + 1,
                                              &cb))
                     || (cb == 0)
                   )
                    arc = FOPSERR_FONT_ALREADY_DELETED;
                else
                {
                    // remove the font from OS2.INI
                    PrfWriteProfileString(HINI_USER,
                                          (PSZ)G_pcszFontsApp, // "PM_Fonts"
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

    return (arc);
}

/* ******************************************************************
 *
 *   Font folder implementation
 *
 ********************************************************************/

/*
 *@@ FONTTHREADDATA:
 *
 */

typedef struct _FONTTHREADDATA
{
    XWPFontFolder       *pFontFolder;
} FONTTHREADDATA, *PFONTTHREADDATA;

/*
 *@@ fnt_fonCreateFontObjects:
 *
 */

VOID fnt_fonCreateFontObjects(PTHREADINFO ptiMyself)
{
    PFONTTHREADDATA pFontThreadData = (PFONTTHREADDATA)ptiMyself->ulData;
    BOOL fFolderLocked = FALSE;

    if (pFontThreadData)
    {
        TRY_LOUD(excpt1)
        {
            XWPFontFolder *pFolder = pFontThreadData->pFontFolder;
            XWPFontFolderData *somThis = XWPFontFolderGetData(pFolder);
            ULONG ulIndex = 0;
            PSZ pszFontKeys = NULL;

            fFolderLocked = !wpshRequestFolderMutexSem(pFolder, SEM_INDEFINITE_WAIT);
            if (fFolderLocked)
            {
                // reset counts
                _ulFontsMax = 0;
                _ulFontsCurrent = 0;

                // wait while folder is still populating
                while (_wpQueryFldrFlags(pFolder)
                            & (FOI_POPULATEINPROGRESS | FOI_REFRESHINPROGRESS))
                    DosSleep(100);

                pszFontKeys = prfhQueryKeysForApp(HINI_USER,
                                                  G_pcszFontsApp); // "PM_Fonts"
                if (pszFontKeys)
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
                        PSZ pszFilename = prfhQueryProfileData(HINI_USER,
                                                               G_pcszFontsApp, // "PM_Fonts",
                                                               pKey2,
                                                               NULL);
                        if (pszFilename)
                        {
                            // always retrieve first font only...
                            CHAR    szFamily[100] = "unknown";
                            CHAR    szTitle[200] = "unknown";
                            CHAR    szStatus[100] = "";       // font is OK

                            APIRET arc = fonGetFontDescription(ptiMyself->hab,
                                                               pszFilename,
                                                               szFamily,
                                                               szTitle);    // face

                            if (arc != NO_ERROR)
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

                    _wpModifyFldrFlags(pFolder,
                                       FOI_POPULATEINPROGRESS | FOI_REFRESHINPROGRESS,
                                       0);

                    // now insert all objects in one flush...
                    if (_ulFontsCurrent)
                    {
                        ULONG       cObjects = 0;
                        WPObject    **papObjects = fdrQueryContentArray(pFolder,
                                                                        &cObjects);

                        if (papObjects)
                        {
                            PVIEWITEM   pViewItem;
                            for (pViewItem = _wpFindViewItem(pFolder, VIEW_ANY, NULL);
                                 pViewItem;
                                 pViewItem = _wpFindViewItem(pFolder, VIEW_ANY, pViewItem))
                            {
                                switch (pViewItem->view)
                                {
                                    case OPEN_CONTENTS:
                                    case OPEN_TREE:
                                    case OPEN_DETAILS:
                                    {
                                        HWND hwndCnr = wpshQueryCnrFromFrame(pViewItem->handle);
                                        POINTL ptlIcon = {0, 0};
                                        if (hwndCnr)
                                            _wpclsInsertMultipleObjects(_somGetClass(pFolder),
                                                                        hwndCnr,
                                                                        &ptlIcon,
                                                                        (PVOID*)papObjects,
                                                                        NULL,   // parentrecord
                                                                        cObjects);
                                    }
                                }
                            }

                            free(papObjects);
                        }
                    }
                }
            } // end if (fFolderLocked)
        }
        CATCH(excpt1)
        {
        } END_CATCH();

        if (fFolderLocked)
            wpshReleaseFolderMutexSem(pFontThreadData->pFontFolder);

        free(pFontThreadData);
    } // end if (pFontThreadData)
}

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

BOOL fonFillWithFontObjects(XWPFontFolder *pFontFolder)
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

    return (brc);
}

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
    return (MRFROM2SHORT(usDrop, usDefaultOp));
}

/*
 *@@ fonDrop:
 *      implementation for XWPFontFolder::wpDragOver.
 *
 *
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
         ulItemNow++)
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
        fopsStartTaskFromList(XFT_INSTALLFONTS,
                              NULLHANDLE,       // no anchor block, asynchronously
                              NULL,             // source folder: not needed
                              pFontFolder,      // target folder: font folder
                              pllDroppedObjects);

        mrc = (MRESULT)RC_DROP_DROPCOMPLETE;
                // means: _all_ items have been processed,
                // and wpDrop should _not_ be called again
                // by the WPS for the next items, if any
    }

    // in any case, free the list
    lstFree(pllDroppedObjects);

    return (mrc);
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

    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    LONG lMenuID2 = usCommand - pGlobalSettings->VarMenuOffset;

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
            somTD_XWPFontFolder_xwpProcessObjectCommand pxwpProcessObjectCommand
                = (somTD_XWPFontFolder_xwpProcessObjectCommand)wpshResolveFor(
                                                       somSelf,
                                                       _somGetParent(_XWPFontFolder),
                                                       "xwpProcessObjectCommand");
            if (pxwpProcessObjectCommand)
            {
                // let parent method return TRUE or FALSE
                brc = pxwpProcessObjectCommand(somSelf,
                                               usCommand,
                                               hwndCnr,
                                               pFirstObject,
                                               ulSelectionFlags);
            }
        }
    }

    return (brc);
}

/* ******************************************************************
 *
 *   Font object implementation
 *
 ********************************************************************/

/*
 *@@ fonModifyFontPopupMenu:
 *      implementation for XWPFontObject::wpModifyPopupMenu.
 */

VOID fonModifyFontPopupMenu(XWPFontObject *somSelf,
                            HWND hwndMenu)
{
    PCGLOBALSETTINGS pGlobalSettings = cmnQueryGlobalSettings();
    MENUITEM mi;
    // get handle to the "Open" submenu in the
    // the popup menu
    if (WinSendMsg(hwndMenu,
                   MM_QUERYITEM,
                   MPFROM2SHORT(WPMENUID_OPEN, TRUE),
                   (MPARAM)&mi))
    {
        // mi.hwndSubMenu now contains "Open" submenu handle,
        // which we add items to now
        PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();
        winhInsertMenuItem(mi.hwndSubMenu, MIT_END,
                           (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_XWPVIEW),
                           "Sample",        // ###
                           MIS_TEXT, 0);
    }

    // insert separator
    winhInsertMenuSeparator(hwndMenu, MIT_END,
                            (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_SEPARATOR));

    // add "Deinstall..."
    winhInsertMenuItem(hwndMenu, MIT_END,
                       (pGlobalSettings->VarMenuOffset + ID_XFMI_OFS_FONT_DEINSTALL),
                       "~Deinstall...",        // ###
                       MIS_TEXT, 0);
}

/*
 *@@ FONTSAMPLEDATA:
 *
 */

typedef struct _FONTSAMPLEDATA
{
    XWPFontObject       *somSelf;

    USEITEM             UseItem;            // use item; immediately followed by view item
    VIEWITEM            ViewItem;           // view item

    HPS                 hps;                // micro PS allocated at WM_CREATE
    LONG                lcidFont;           // font LCID we're representing here

    FONTMETRICS         FontMetrics;
} FONTSAMPLEDATA, *PFONTSAMPLEDATA;

/*
 *@@ fon_fnwpFontSampleClient:
 *      window proc for the font "Sample" view client.
 */

MRESULT EXPENTRY fon_fnwpFontSampleClient(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
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
                GpiSetCharSet(pWinData->hps, pWinData->lcidFont);
            }
            else
                // error:
                mrc = (MPARAM)TRUE;
        break; }

        case WM_PAINT:
        {
            RECTL rclClient;
            ULONG aulSizes[] =
                    {
                        72,
                        48,
                        36,
                        24,
                        16,
                        12,
                        10,
                        8,
                        6
                    };
            POINTL ptlCurrent;
            ULONG ul = 0;
            const char *pcszText = "The Quick Brown Fox Jumps Over The Lazy Dog.";

            // since we're not using WinBeginPaint,
            // we must validate the update region,
            // or we'll get bombed with WM_PAINT msgs
            WinValidateRect(hwnd,
                            NULL,
                            FALSE);

            WinQueryWindowRect(hwnd, &rclClient);

            WinFillRect(pWinData->hps,
                        &rclClient,
                        RGBCOL_WHITE);

            ptlCurrent.x = 5;
            ptlCurrent.y = rclClient.yTop - (aulSizes[0] * 1.2);

            for (ul = 0;
                 ul < sizeof(aulSizes) / sizeof(aulSizes[0]);
                 ul++)
            {
                gpihSetPointSize(pWinData->hps, aulSizes[ul]);
                GpiMove(pWinData->hps, &ptlCurrent);
                GpiCharString(pWinData->hps, strlen(pcszText), (PSZ)pcszText);
                ptlCurrent.y -= aulSizes[ul] * 1.2;
            }
        break; }

        case WM_CLOSE:
            // destroy the frame, which in turn destroys us
            WinDestroyWindow(WinQueryWindow(hwnd, QW_OWNER));
        break;

        case WM_DESTROY:
            if (pWinData)
            {
                if (pWinData->hps)
                {
                    if (pWinData->lcidFont)
                    {
                        _Pmpf((__FUNCTION__ ": GpiDeleteSetId"));
                        GpiSetCharSet(pWinData->hps, LCID_DEFAULT);
                        if (!GpiDeleteSetId(pWinData->hps, pWinData->lcidFont))
                            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                                   "GpiDeleteSetId failed with %d",
                                   WinGetLastError(WinQueryAnchorBlock(hwnd)));
                    }
                    GpiAssociate(pWinData->hps, NULLHANDLE);
                    _Pmpf((__FUNCTION__ ": GpiDestroyPS"));
                    if (!GpiDestroyPS(pWinData->hps))
                        cmnLog(__FILE__, __LINE__, __FUNCTION__,
                               "GpiDestroyPS failed with %d",
                               WinGetLastError(WinQueryAnchorBlock(hwnd)));
                }

                // remove this window from the object's use list
                _wpDeleteFromObjUseList(pWinData->somSelf,
                                        &pWinData->UseItem);

                _wpFreeMem(pWinData->somSelf,
                           (PBYTE)pWinData);
            }
        break;

        default:
            mrc = WinDefWindowProc(hwnd, msg, mp1, mp2);
    }

    return (mrc);
}

/*
 *@@ fonCreateFontSampleView:
 *      implementation for XWPFontObject::wpOpen. This
 *      creates the "Sample" view for the font object.
 */

HWND fonCreateFontSampleView(XWPFontObject *somSelf,
                             HAB hab,
                             ULONG ulView)
{
    HWND hwndNewView = NULLHANDLE;

    TRY_LOUD(excpt1)
    {
        XWPFontObjectData *somThis = XWPFontObjectGetData(somSelf);

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

                swpFrame.x = 10;
                swpFrame.y = 10;
                swpFrame.cx = 200;
                swpFrame.cy = 200;
                swpFrame.hwndInsertBehind = HWND_TOP;
                swpFrame.fl = SWP_SIZE | SWP_MOVE | SWP_ZORDER;

                hwndNewView
                   = winhCreateStdWindow(HWND_DESKTOP,  // frame's parent
                                         &swpFrame,
                                         FCF_NOBYTEALIGN
                                            | FCF_TITLEBAR
                                            | FCF_SYSMENU
                                            | FCF_MINMAX
                                            | FCF_SIZEBORDER,
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
                    // add the use list item to the object's use list;
                    // this struct has been zeroed above
                    pData->UseItem.type    = USAGE_OPENVIEW;
                    pData->ViewItem.view   = ulView;
                    pData->ViewItem.handle = hwndNewView;
                    if (!_wpAddToObjUseList(somSelf,
                                            &pData->UseItem))
                        cmnLog(__FILE__, __LINE__, __FUNCTION__,
                               "_wpAddToObjUseList failed.");

                    _wpRegisterView(somSelf,
                                    hwndNewView,
                                    "Sample"); // view title ###

                    WinSetWindowPos(hwndNewView,
                                    HWND_TOP,
                                    10, 10, 300, 300,
                                    SWP_SHOW | SWP_ZORDER | SWP_MOVE | SWP_SIZE | SWP_ACTIVATE);
                }
                else
                    free(pData);
            }
        }
    }
    CATCH(excpt1) {} END_CATCH();


    return (hwndNewView);
}


