
/*
 *@@sourcefile wpsh.c:
 *      this file contains many WPS helper functions.
 *
 *      Usage: All WPS classes.
 *
 *      These are "pseudo SOM methods" in that most of
 *      these take some WPS object(s) as parameters without
 *      having been declared as SOM methods in the .IDL
 *      files. This is done for the following reasons:
 *
 *      a)  SOM method resolution takes at least three
 *          times as long as calling a regular C function;
 *
 *      b)  if we declare these functions as SOM methods,
 *          this requires the owning XWorkplace WPS replacement
 *          class to be installed, which can lead to
 *          crashes if it is not.
 *
 *      These functions should work even if XWorkplace is not
 *      installed.
 *
 *      This file was new with V0.84, called "xwps.c". Most of
 *      these functions used to have the cmn* prefix and were
 *      spread across all over the .C source files, which
 *      was not very lucid. As a result, I have put these
 *      together in a separate file.
 *
 *      The code has been made independent of XWorkplace with V0.9.0,
 *      moved to the shared\ directory, and renamed to "wpsh.c".
 *
 *      All the functions in this file have the wpsh* prefix (changed
 *      with V0.9.0).
 *
 *@@header "shared\wpsh.h"
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

/*
 *  Suggested #include order:
 *  1)  os2.h
 *  2)  C library headers
 *  3)  SOM headers which work with precompiled header files
 *  4)  headers in /helpers
 *  5)  headers in /main with dlgids.h and common.h first
 *  6)  #pragma hdrstop to prevent VAC++ crashes
 *  7)  other needed SOM headers
 *  8)  for non-SOM-class files: corresponding header (e.g. classlst.h)
 */

#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_WIN
#include <os2.h>

// C library headers
#include <stdio.h>              // needed for except.h
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\cnrh.h"               // container helper routines
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\winh.h"               // PM helper routines
#include "helpers\except.h"             // exception handling

// SOM headers which don't crash with prec. header files

// other SOM headers
#pragma hdrstop                 // VAC++ keeps crashing otherwise
#include <wpfolder.h>
#include <wpdisk.h>
#include <wpshadow.h>

#include "shared\wpsh.h"

/*
 *@@ wpshCheckObject:
 *      since somIsObj doesn't seem to be working right,
 *      here is a new function which checks if pObject
 *      points to a valid WPS object. This is done by
 *      temporarily installing yet another xcpt handler,
 *      so if the object ain't valid, calling this function
 *      doesn't crash, but returns FALSE only.
 *
 *      Even though the SOM reference says that somIsObj
 *      should be "failsafe", it is not. I've read somewhere
 *      that that function only checks if the object pointer
 *      points to something that looks like a method table,
 *      which doesn't look very failsafe to me, so use this
 *      one instead.
 *
 *      Note: this should not be called very often,
 *      because a SOM method is called upon the given
 *      object, which takes a little bit of time. Also, if
 *      the object is not valid, an exception is generated
 *      internally.
 *
 *      So call this only if you're unsure whether an object
 *      is valid.
 *
 *@@changed V0.9.0 [umoeller]: now using TRY_xxx macros
 */

BOOL wpshCheckObject(WPObject *pObject)
{
    BOOL                  brc = TRUE;

    if (pObject)
    {
        TRY_QUIET(excpt1, NULL)
        {
            // call an object method; if this doesn't fail,
            // TRUE is returned, otherwise an xcpt occurs
            _wpQueryTitle(pObject);
            brc = TRUE;
        }
        CATCH(excpt1)
        {
            // the thread exception handler puts us here if an exception
            // occured, i.e. the object was not valid:
            brc = FALSE;
            #ifdef DEBUG_ORDEREDLIST
                DosBeep(10000, 10);
                _Pmpf(("wpshCheckObject: Invalid object found."));
            #endif
        } END_CATCH();
    }

    return (brc);
}

/*
 *@@ wpshQueryObjectFromID:
 *      this returns an object for an object ID
 *      (those things in angle brackets) or NULL
 *      if not found.
 *
 *      If NULL is returned and (pulErrorCode != NULL),
 *      *pulErrorCode will be set to the following:
 *      --  1:  ID does not even exist.
 *      --  2:  ID does exist, but cannot be resolved
 *              (because object handle is invalid).
 *
 *      This method was added because apparently, IBM
 *      is unable to keep object ID resolution working
 *      with all OS/2 versions, and WinQueryObject causes
 *      a process switch. At least I was reported
 *      that this keeps failing with certain fixpak
 *      levels, so we resolve these things directly
 *      from OS2.INI now.
 *
 *@@added V0.9.0 [umoeller]
 */

WPObject* wpshQueryObjectFromID(const PSZ pszObjectID,
                                PULONG pulErrorCode)
{
    ULONG       ulHandle,
                cbHandle;
    WPObject*   pObject = NULL;

    // the WPS stores all the handles as plain ULONGs
    // (four bytes)
    cbHandle = sizeof(ulHandle);
    if (PrfQueryProfileData(HINI_USER,
                            "PM_Workplace:Location",
                            pszObjectID,                  // key
                            &ulHandle,
                            &cbHandle))
    {
        // cool, handle found: resolve object
        pObject = _wpclsQueryObject(_WPObject, ulHandle);
        if (!pObject)
            if (pulErrorCode)
                *pulErrorCode = 2;
    }
    else
        if (pulErrorCode)
            *pulErrorCode = 1;      // ID not found

    return (pObject);
}

/*
 *@@ wpshQueryRootFolder:
 *      just like wpQueryRootFolder, but this one
 *      avoids "Drive not ready" popups if the drive
 *      isn't ready. In this case, *parc contains
 *      the DOS error code (NO_ERROR otherwise), and
 *      NULL is returned.
 *      If you're not interested in the return code,
 *      you may pass parc as NULL.
 */

WPFolder* wpshQueryRootFolder(WPDisk* somSelf, // in: disk to check
                              APIRET *parc)    // out: DOS error code; may be NULL
{
    WPFolder *pReturnFolder = NULL;
    APIRET   arc = NO_ERROR;

    ULONG ulLogicalDrive = _wpQueryLogicalDrive(somSelf);
    arc = doshAssertDrive(ulLogicalDrive);
    if (arc == NO_ERROR)
        pReturnFolder = _wpQueryRootFolder(somSelf);

    if (parc)
        *parc = arc;
    return (pReturnFolder);
}

/*
 *@@ wpshPopulateTree:
 *      this will populate a given folder and all of
 *      its subfolders; used by the Worker thread to
 *      populate the Config folders in the background
 *      after the desktop is ready.
 *
 *      Be warned, this recurses in really all the
 *      folders, so in extreme cases this can populate
 *      a whole drive, which will take ages.
 */

BOOL wpshPopulateTree(WPFolder *somSelf)
{
    BOOL brc = FALSE;
    WPObject    *pObject;

    if (somSelf)
    {
        if (wpshCheckIfPopulated(somSelf))
            brc = TRUE;

        for (   pObject = _wpQueryContent(somSelf, NULL, (ULONG)QC_FIRST);
                (pObject);
                pObject = _wpQueryContent(somSelf, pObject, (ULONG)QC_NEXT)
            )
        {
            if (_somIsA(pObject, _WPFolder))
                if (wpshPopulateTree(pObject)) // recurse
                    brc = TRUE;
        }
    }

    return (brc);
}

/*
 *@@ wpshCheckIfPopulated:
 *      this populates a folder if it's not fully populated yet.
 *      Saves you from querying the full path and all that.
 *      Returns TRUE only if the folder was successfully populated;
 *      returns FALSE if either the folder was populated already
 *      or populating failed.
 */

BOOL wpshCheckIfPopulated(WPFolder *somSelf)
{
    BOOL        brc = FALSE;
    CHAR        szRealName[CCHMAXPATH];

    if ((_wpQueryFldrFlags(somSelf) & FOI_POPULATEDWITHALL) == 0)
    {
        _wpQueryFilename(somSelf, szRealName, TRUE);
        brc = _wpPopulate(somSelf, 0, szRealName, FALSE);
    }
    return (brc);
}

/*
 *@@ wpshQueryDiskFreeFromFolder:
 *      returns the free space on the drive where a
 *      given folder resides (in bytes).
 *
 *@@changed V0.9.0 [umoeller]: fixed another > 4 GB bug (thanks to Rdiger Ihle)
 */

double wpshQueryDiskFreeFromFolder(WPFolder *somSelf)
{
    if (somSelf)
    {
        CHAR        szRealName[CCHMAXPATH];
        ULONG       ulDrive;
        FSALLOCATE  fsa;

        _wpQueryFilename(somSelf, szRealName, TRUE);
        ulDrive = (szRealName[0] - 'A' + 1); // = 1 for "A", 2 for "B" etc.
        DosQueryFSInfo(ulDrive, FSIL_ALLOC, &fsa, sizeof(fsa));
        return ((double)fsa.cSectorUnit * fsa.cbSector * fsa.cUnitAvail);
    }
    else
        return (0);
}

/*
 *@@ wpshResidesBelow:
 *      returns TRUE if pChild resides either in
 *      or somewhere below the folder hierarchy
 *      of pFolder.
 */

BOOL wpshResidesBelow(WPObject *pChild,
                      WPFolder *pFolder)
{
    BOOL        rc = FALSE;
    WPObject    *pObj;
    if ( (pFolder) && (pChild))
    {
        pObj = pChild;
        while (pObj)
        {
            if (pFolder == pObj)
            {
                rc = TRUE;
                break;
            }
            else
                pObj = _wpQueryFolder(pObj);
        }
    }
    return (rc);
}

/*
 *@@ wpshContainsFile:
 *      this returns a file-system object if the folder contains the
 *      file pszRealName.
 *
 *      This does _not_ use wpPopulate, but DosFindFirst to find the
 *      file, including a subfolder of the same name. Abstract objects
 *      are not found. If such an object does not exist, NULL is
 *      returned.
 */

WPFileSystem* wpshContainsFile(WPFolder *pFolder,   // in: folder to examine
                               PSZ pszRealName)     // in: file-name (w/out path)
{
    CHAR        szRealName[2*CCHMAXPATH];
    ULONG       cbRealName = sizeof(szRealName);
    WPObject    *prc = NULL;

    if (_wpQueryRealName(pFolder, szRealName, &cbRealName, TRUE))
    {
        HDIR          hdirFindHandle = HDIR_SYSTEM;
        FILEFINDBUF3  FindBuffer     = {0};      // Returned from FindFirst/Next
        ULONG         ulResultBufLen = sizeof(FILEFINDBUF3);
        ULONG         ulFindCount    = 1;        // Look for 1 file at a time
        APIRET        rc             = NO_ERROR; // Return code

        szRealName[cbRealName] =  '\\';
        strcpy(szRealName+cbRealName+1, pszRealName);

        rc = DosFindFirst(szRealName,
                          &hdirFindHandle,
                          FILE_DIRECTORY
                              | FILE_SYSTEM
                              | FILE_ARCHIVED
                              | FILE_HIDDEN
                              | FILE_READONLY,
                          // FILE_NORMAL,
                          &FindBuffer,
                          ulResultBufLen,
                          &ulFindCount,
                          FIL_STANDARD);
        DosFindClose(hdirFindHandle);
        if (rc == NO_ERROR)
            prc = _wpclsQueryObjectFromPath(_WPFileSystem, szRealName);
    }
    return (prc);
}

/*
 *@@ wpshCreateFromTemplate:
 *      enhanced wpCreateFromTemplate, which can automatically
 *      make the title of the new object editable and reposition
 *      the newly created object to the mouse position.
 *      This is _not_ a replacement of wpCreateFromTemplate and
 *      _not_ a SOM method, but only used when XFolder creates objects.
 *      This returns the new object.
 *
 *@@changed V0.9.0 [umoeller]: changed function prototype to be XWorkplace-independent
 */

WPObject* wpshCreateFromTemplate(WPObject *pTemplate,
                                    // the template to create from
                                 WPFolder* pFolder,
                                    // the folder to create the new object in
                                 HWND hwndFrame,
                                    // the frame wnd in which the object
                                    // should be manipulated
                                 USHORT usOpenSettings,
                                    // 0: do nothing after creation
                                    // 1: open settings notebook
                                    // 2: make title editable
                                 BOOL fReposition,
                                    // in: if TRUE, the new object will be repositioned
                                 POINTL* pptlMenuMousePos)
                                    // for Icon views: position to create
                                    // object at or NULL for default pos
{
    HPOINTER            hptrOld;
    WPObject            *pNewObject = NULL;
    BOOL                fIconPos = FALSE,
                        fFolderSemOwned = FALSE,
                        fNewObjSemOwned = FALSE;

    TRY_LOUD(excpt1, NULL)
    {
        // change the mouse pointer to "wait" state
        hptrOld = winhSetWaitPointer();

        if ((pFolder) && (pTemplate) && (hwndFrame))
        {
            HWND            hwndCnr = wpshQueryCnrFromFrame(hwndFrame);
            CNRINFO         CnrInfo;
            // WPFileSystem    *pNowhereFolder;

            if (hwndCnr)
                cnrhQueryCnrInfo(hwndCnr, &CnrInfo);

            // position newly created object in window?
            if ((hwndCnr) && (fReposition) && (pptlMenuMousePos))
            {
                // only do this in "true" Icon views (not Tree, not Name, not Text)
                if (    ((CnrInfo.flWindowAttr & (CV_ICON | CV_TREE)) == CV_ICON)
                        // and only if "Always sort" is off
                     && (CnrInfo.pSortRecord == NULL)
                   )
                {
                    SWP swp;
                    HWND hwndFrame2 = // wpshQueryFrameFromView(pFolder, OPEN_CONTENTS);
                            WinQueryWindow(hwndCnr, QW_PARENT);

                    if (hwndFrame2)
                    {
                        // the passed mouse coords are relative to screen,
                        // so subtract frame wnd coords
                        WinQueryWindowPos(hwndFrame2, &swp);
                        pptlMenuMousePos->x -= swp.x;
                        pptlMenuMousePos->y -= swp.y;

                        // subtract cnr coords also
                        WinQueryWindowPos(hwndCnr, &swp);
                        pptlMenuMousePos->x -= swp.x;
                        pptlMenuMousePos->y -= swp.y;

                        // add cnr work area offset
                        pptlMenuMousePos->x += CnrInfo.ptlOrigin.x;
                        pptlMenuMousePos->y += CnrInfo.ptlOrigin.y;

                        fIconPos = TRUE;
                    }
                }
            }

            wpshCheckIfPopulated(pFolder);
            pNewObject = _wpCreateFromTemplate(pTemplate,
                                               pFolder,
                                               TRUE);

            fFolderSemOwned = !_wpRequestObjectMutexSem(pFolder, 3000);
            if (    (fFolderSemOwned)
                &&  (pNewObject)
               )
            {
                PMINIRECORDCORE pmrc;

                fNewObjSemOwned = !_wpRequestObjectMutexSem(pNewObject, 3000);
                if (fNewObjSemOwned)
                {
                    pmrc = _wpQueryCoreRecord(pNewObject);

                    if ((hwndCnr) && (pmrc))
                    {
                        // move new object to mouse pos, if allowed;
                        // we must do this "manually" by manipulating the
                        // cnr itself, because the WPS methods for setting
                        // icon positions simply don't work (I think this
                        // broke with Warp 3)

                        if (fIconPos)       // valid-data flag set above
                        {
                            /* _wpCnrInsertObject(pNewObject,
                                        hwndCnr,
                                        pptlMenuMousePos,
                                        pmrc,
                                        NULL); */

                            // the WPS shares records among views, so we need
                            // to update the record core info first
                            WinSendMsg(hwndCnr,
                                       CM_QUERYRECORDINFO,
                                       (MPARAM)&pmrc,
                                       (MPARAM)1);         // one record only
                            // un-display the new object at the old (default) location
                            WinSendMsg(hwndCnr,
                                       CM_ERASERECORD,
                                           // this only changes the visibility of the
                                           // record without changing the recordcore;
                                           // this msg is intended for drag'n'drop and such
                                       (MPARAM)pmrc,
                                       NULL);

                            // move object
                            pmrc->ptlIcon.x = pptlMenuMousePos->x;
                            pmrc->ptlIcon.y = pptlMenuMousePos->y;

                            // repaint at new position
                            WinSendMsg(hwndCnr,
                                       CM_INVALIDATERECORD,
                                       (MPARAM)&pmrc,
                                       MPFROM2SHORT(1,     // one record only
                                           CMA_REPOSITION | CMA_ERASE));

                            // scroll cnr work area to make the new object visible
                            cnrhScrollToRecord(hwndCnr,
                                               (PRECORDCORE)pmrc,
                                               CMA_TEXT,
                                               FALSE);
                        }

                    } // end if ((hwndCnr) && (pmrc))

                    _wpReleaseObjectMutexSem(pNewObject);
                    fNewObjSemOwned = FALSE;
                } // end if (fNewObjSemOwned)

                // the object is now created; depending on the
                // Global settings, we will now either open
                // the settings notebook of it or make its title
                // editable

                if (usOpenSettings == 1)
                {
                    // open settings of the newly created object
                    _wpViewObject(pNewObject,
                                  NULLHANDLE,
                                  OPEN_SETTINGS,
                                  0L);
                }
                else if (usOpenSettings == 2)
                {
                    // make the title of the newly created object
                    // editable (container "direct editing"), if
                    // the settings allow it and the folder is open
                    if (hwndCnr)
                    {
                        CNREDITDATA     CnrEditData = {0};

                        // first check if the folder window whose
                        // context menu was used is in Details view
                        // or other
                        if (CnrInfo.flWindowAttr & CV_DETAIL)
                        {
                            PFIELDINFO      pFieldInfo = 0;

                            // Details view: now, this is wicked; we
                            // need to find out the "Title" column of
                            // the container which we need to pass to
                            // the container for enabling direct editing
                            pFieldInfo = (PFIELDINFO)WinSendMsg(hwndCnr,
                                                                CM_QUERYDETAILFIELDINFO,
                                                                MPNULL,
                                                                (MPARAM)CMA_FIRST);

                            // pFieldInfo now points to the first Details
                            // column; now we go through all the Details
                            // columns until we find one which is not
                            // read-only (which should be the title); we
                            // cannot assume "column two" or anything like
                            // this, because this folder might have
                            // Details settings which are different from
                            // the defaults
                            while ((pFieldInfo) && ((LONG)pFieldInfo != -1))
                            {
                                if (pFieldInfo->flData & CFA_FIREADONLY)
                                    break; // while

                                // else get next column
                                pFieldInfo = (PFIELDINFO)WinSendMsg(hwndCnr,
                                                                    CM_QUERYDETAILFIELDINFO,
                                                                    pFieldInfo,
                                                                    (MPARAM)CMA_NEXT);
                            }

                            // in Details view, direct editing needs the
                            // column info plus a fixed constant
                            CnrEditData.pFieldInfo = pFieldInfo;
                            CnrEditData.id = CID_LEFTDVWND;
                        }
                        else
                        {
                            // other than Details view: that's easy,
                            // we only need the container ID
                            CnrEditData.pFieldInfo = NULL;
                            CnrEditData.id = WinQueryWindowUShort(hwndCnr, QWS_ID);
                        }

                        CnrEditData.cb = sizeof(CnrEditData);
                        CnrEditData.hwndCnr = hwndCnr;
                        // pass the MINIRECORDCORE of the new object
                        CnrEditData.pRecord = (PRECORDCORE)pmrc;
                        // use existing (template default) title
                        CnrEditData.ppszText = NULL;
                        CnrEditData.cbText = 0;

                        // finally, this message switches to
                        // direct editing of the title
                        WinPostMsg(hwndCnr,
                                   CM_OPENEDIT,
                                   (MPARAM)&CnrEditData,
                                   MPNULL);
                    } // end if (hwndCnr)
                } // end else if (usOpenSettings == 2)

            } // end if (pNewObject);
            else
                WinEnableWindowUpdate(hwndCnr, TRUE);

            // wpQueryNextIconPos
            /*
            // now create new object from template
            pNewObject = _wpCreateFromTemplate(
                    pTemplate,   // template
                    pFolder,     // folder to create object in
                    TRUE);       // keep new object awake

            if (pNewObject) {

                DosSleep(200);

            */

            if (fFolderSemOwned)
            {
                _wpReleaseObjectMutexSem(pFolder);
                fFolderSemOwned = FALSE;
            }

        } // end if ((pFolder) && (pTemplate) && (hwndFrame))

        // after all this, reset the mouse pointer
        WinSetPointer(HWND_DESKTOP, hptrOld);
    }
    CATCH(excpt1)
    {
    } END_CATCH();

    if (fNewObjSemOwned)
    {
        _wpReleaseObjectMutexSem(pNewObject);
        fNewObjSemOwned = FALSE;
    }
    if (fFolderSemOwned)
    {
        _wpReleaseObjectMutexSem(pFolder);
        fFolderSemOwned = FALSE;
    }

    return (pNewObject);
}

/*
 *@@ wpshQueryFrameFromView:
 *       this routine gets the frame window handle of the
 *       specified object view (OPEN_* flag, e.g. OPEN_CONTENTS
 *       or OPEN_SETTINGS), as found in the view items list
 *       of somSelf (wpFindViewItem).
 *
 *       Returns NULLHANDLE if the specified view is not
 *       currently open.
 */

HWND wpshQueryFrameFromView(WPFolder *somSelf,  // in: folder to examine
                            ULONG ulView)       // in: OPEN_CONTENTS etc.
{
    PVIEWITEM           pViewItem;
    // PUSEITEM            pUseFile;
    HWND                hwndFrame = 0;

    if (_wpFindUseItem(somSelf, USAGE_OPENVIEW, NULL))
    { // folder has an open view
        // now we go search the open views of the folder and get the
        // frame handle of the desired view (ulView)
        for (pViewItem = _wpFindViewItem(somSelf, VIEW_ANY, NULL);
             pViewItem;
             pViewItem = _wpFindViewItem(somSelf, VIEW_ANY, pViewItem))
        {
            if (pViewItem->view == ulView)
                 hwndFrame = pViewItem->handle;
        } // end for
    } // end if
    return (hwndFrame);
}

/*
 *@@ wpshQueryView:
 *      kinda reverse to wpshQueryFrameFromView, this
 *      returns the OPEN_* flag which represents the
 *      specified view.
 *
 *      For example, pass the hwndFrame of a folder's
 *      Details view to this func, and you'll get
 *      OPEN_DETAILS back.
 *
 *      Returns 0 upon errors.
 */

ULONG wpshQueryView(WPObject* somSelf,      // in: object to examine
                    HWND hwndFrame)         // in: frame window of open view of somSelf
{
    PUSEITEM    pUseItem = NULL;
    ULONG       ulView = 0;

    for (pUseItem = _wpFindUseItem(somSelf, USAGE_OPENVIEW, NULL);
         pUseItem;
         pUseItem = _wpFindUseItem(somSelf, USAGE_OPENVIEW, pUseItem))
    {
        PVIEWITEM pViewItem = (PVIEWITEM)(pUseItem+1);
        if (pViewItem->handle == hwndFrame)
        {
            ulView = pViewItem->view;
            break;
        }
    }

    return (ulView);
}

/*
 *@@ wpshQuerySourceObject:
 *      this helper function evaluates a given container
 *      to find out which objects have been selected while
 *      a context menu is open. The WPS gives the items on
 *      which the menu action should be performed upon
 *      container "source" emphasis, so this is what we
 *      evaluate.
 *
 *      This function only works when a context menu is open,
 *      because otherwise WPS cnr items don't have source emphasis.
 *
 *      However, if (fKeyboardMode == TRUE), this function
 *      does not check for source emphasis, but selection
 *      emphasis instead. Only in that case this function
 *      can be used even when no context menu is open on
 *      the container. This is useful for processing hotkeys
 *      on objects, because the WPS makes those work on
 *      selected objects only.
 *
 *      The result of this evaluation is stored in
 *      *pulSelection, which can be:
 *
 *      --   SEL_WHITESPACE the context menu was opened on the
 *                          whitespace of the container;
 *                          this func then returns the folder itself.
 *                          This is only possible if (fKeyboard ==
 *                          FALSE).
 *
 *      --   SEL_SINGLESEL  the context menu was opened for a
 *                          single selected object (that is,
 *                          exactly one object is selected and
 *                          the menu was opened above that object):
 *                          this func then returns that object.
 *
 *      --   SEL_MULTISEL   the context menu was opened on one
 *                          of a multitude of selected objects;
 *                          this func then returns the first of the
 *                          selected objects. Only in that case,
 *                          keep calling wpshQueryNextSourceObject
 *                          to get the other selected objects.
 *
 *      --   SEL_SINGLEOTHER the context menu was opened for a
 *                          single object _other_ than the selected
 *                          objects:
 *                          this func then returns that object.
 *                          This is only possible if (fKeyboard ==
 *                          FALSE).
 *
 *      --   SEL_NONEATALL: no object is selected. This is only
 *                          possible if (fKeyboard == TRUE); only
 *                          in that case, NULL is returned also.
 *
 *      Note that these flags are defined in include\helpers\cnrh.h.
 *
 *      Keep in mind that if this function returns something other
 *      than the folder of the container (SEL_WHITESPACE), the
 *      returned object might be a shadow, which you might need to
 *      dereference before working on it.
 *
 *@@changed V0.9.1 (2000-01-29) [umoeller]: moved this here from menus.c; changed prefix
 *@@changed V0.9.1 (2000-01-31) [umoeller]: added fKeyboardMode support
 */

WPObject* wpshQuerySourceObject(WPFolder *somSelf,     // in: folder with open menu
                                HWND hwndCnr,          // in: cnr
                                BOOL fKeyboardMode,    // in: if TRUE, check selected instead of source only
                                PULONG pulSelection)   // out: selection flags
{
    WPObject        *pObject = NULL;

    do
    {
        PMINIRECORDCORE pmrcSource = 0,
                        pmrcSelected = 0;
        if (!fKeyboardMode)
        {
            // not keyboard, but mouse mode:
            // get first object with source emphasis
            pmrcSource = (PMINIRECORDCORE)WinSendMsg(hwndCnr,
                                                     CM_QUERYRECORDEMPHASIS,
                                                     (MPARAM)CMA_FIRST,
                                                     (MPARAM)CRA_SOURCE);

            if (pmrcSource == NULL)
            {
                // if CM_QUERYRECORDEMPHASIS returns NULL
                // for source emphasis (CRA_SOUCE),
                // this means the whole container has source
                // emphasis --> context menu on folder whitespace
                pObject = somSelf;   // folder
                *pulSelection = SEL_WHITESPACE;
                // we're done
                break;
            }
            else if (((LONG)pmrcSource) == -1)
                // error:
                break;
            // else: we have at least one object with source emphasis
        }

        // get first _selected_ now
        pmrcSelected = (PMINIRECORDCORE)WinSendMsg(hwndCnr,
                                                   CM_QUERYRECORDEMPHASIS,
                                                   (MPARAM)CMA_FIRST,
                                                   (MPARAM)CRA_SELECTED);
        if (((LONG)pmrcSelected) == -1)
            // error:
            break;

        if (!fKeyboardMode)
        {
            // not keyboard, but mouse mode:
            // get the object with source emphasis
            // (this is != NULL at this point)
            pObject = OBJECT_FROM_PREC(pmrcSource);

            // check if first source object is equal to
            // first selected object, i.e. the menu was
            // opened on one or several selected objects
            if (pmrcSelected != pmrcSource)
            {
                // no:
                // only one object, but not one of
                // the selected ones
                *pulSelection = SEL_SINGLEOTHER;
                // we're done
                break;
            }
        }
        else
        {
            // keyboard mode:
            if (pmrcSelected)
                pObject = OBJECT_FROM_PREC(pmrcSelected);
            else
            {
                // no selected object: that's no object
                // at all
                *pulSelection = SEL_NONEATALL;
                // we're done
                break;
            }
        }

        // we're still going if
        // a)   we're in menu mode and the first
        //      source object equals the first selected
        //      object or
        // b)   we're in keyboard mode and any
        //      selected object was found.
        // Now, are several objects selected?
        pmrcSelected = (PMINIRECORDCORE)WinSendMsg(hwndCnr,
                                                   CM_QUERYRECORDEMPHASIS,
                                                   (MPARAM)pmrcSelected,
                                                        // get second obj
                                                   (MPARAM)CRA_SELECTED);
        if (pmrcSelected)
            // several objects:
            *pulSelection = SEL_MULTISEL;
        else
            // only one object:
            *pulSelection = SEL_SINGLESEL;
    } while (FALSE);

    // note that we have _not_ dereferenced shadows
    // here, because this will lead to confusion for
    // finding other selected objects in the same
    // folder; dereferencing shadows is therefore
    // the responsibility of the caller
    return (pObject);       // can be NULL
}

/*
 *@@ wpshQueryNextSourceObject:
 *      if wpshQuerySourceObject above returns SEL_MULTISEL,
 *      you can keep calling this helper func to get the
 *      other objects with source emphasis until this function
 *      returns NULL.
 *
 *      This will return the next object after pObject which
 *      is selected or NULL if it's the last.
 *
 *@@changed V0.9.1 (2000-01-29) [umoeller]: moved this here from menus.c; changed prefix
 */

WPObject* wpshQueryNextSourceObject(HWND hwndCnr,
                                    WPObject *pObject)
{
    WPObject *pObject2 = NULL;
    PMINIRECORDCORE pmrcCurrent = _wpQueryCoreRecord(pObject);
    if (pmrcCurrent)
    {
        PMINIRECORDCORE pmrcNext
            = (PMINIRECORDCORE)WinSendMsg(hwndCnr,
                                          CM_QUERYRECORDEMPHASIS,
                                          (MPARAM)pmrcCurrent,
                                          (MPARAM)CRA_SELECTED);
        if (    (pmrcNext)
             && ((LONG)pmrcNext != -1)
           )
            pObject2 = OBJECT_FROM_PREC(pmrcNext);
    }

    return (pObject2);
}

/*
 *@@ wpshQueryLogicalDriveNumber:
 *      as opposed to wpQueryDisk, of which I really don't
 *      know what it returns, this returns the logical drive
 *      number (1 = A, 2 = B, etc.) on which the WPObject
 *      resides. This works also for objects which are not
 *      file-system based; for these, their folder is examined
 *      instead.
 */

ULONG wpshQueryLogicalDriveNumber(WPObject *somSelf)
{
    ULONG ulDrive = 0;
    WPFileSystem *pFSObj = NULL;
    if (somSelf)
    {
        CHAR        szRealName[CCHMAXPATH];

        if (!_somIsA(somSelf, _WPFileSystem))
            pFSObj = _wpQueryFolder(somSelf);
        else
            pFSObj = somSelf;

        if (pFSObj)
        {
            _wpQueryFilename(pFSObj, szRealName, TRUE);
            ulDrive = (szRealName[0] - 'A' + 1); // = 1 for "A", 2 for "B" etc.
        }
    }
    return (ulDrive);
}

/*
 *@@ wpshCopyObjectFileName:
 *      copy object filename(s) to clipboard. This method is
 *      called from several overrides of wpMenuItemSelected.
 *
 *      If somSelf does not have CRA_SELECTED emphasis in the
 *      container, its filename is copied. If it does have
 *      CRA_SELECTED emphasis, all filenames which have CRA_SELECTED
 *      emphasis are copied, separated by spaces.
 *
 *      Note that somSelf might not neccessarily be a file-system
 *      object. It can also be a shadow to one, so we might need
 *      to dereference that.
 *
 *@@changed V0.9.0 [umoeller]: fixed a minor bug when memory allocation failed
 */

BOOL wpshCopyObjectFileName(WPObject *somSelf, // in: the object which was passed to
                                // wpMenuItemSelected
                            HWND hwndCnr, // in: the container of the hwmdFrame
                                // of wpMenuItemSelected
                            BOOL fFullPath) // in: if TRUE, the full path will be
                                // copied; otherwise the filename only
{
    PSZ     pszDest = NULL;
    BOOL    fSuccess = FALSE,
            fSingleMode = TRUE;
    CHAR    szToClipboard[3000] = "";
    ULONG   ulLength;
    HAB     hab = WinQueryAnchorBlock(hwndCnr);

    // get the record core of somSelf
    PMINIRECORDCORE pmrcSelf = _wpQueryCoreRecord(somSelf);

    // now we go through all the selected records in the container
    // and check if pmrcSelf is among these selected records;
    // if so, this means that we want to copy the filenames
    // of all the selected records.
    // However, if pmrcSelf is not among these, this means that
    // either the context menu of the _folder_ has been selected
    // or the menu of an object which is not selected; we will
    // then only copy somSelf's filename.

    PMINIRECORDCORE pmrcSelected = (PMINIRECORDCORE)CMA_FIRST;

    do
    {
        // get the first or the next _selected_ item
        pmrcSelected =
            (PMINIRECORDCORE)WinSendMsg(hwndCnr,
                    CM_QUERYRECORDEMPHASIS,
                    (MPARAM)pmrcSelected,
                    (MPARAM)CRA_SELECTED);

        if ((pmrcSelected != 0) && (((ULONG)pmrcSelected) != -1))
        {
            // first or next record core found:
            CHAR       szRealName[CCHMAXPATH];

            // get SOM object pointer
            WPObject *pObject = (WPObject*)OBJECT_FROM_PREC(pmrcSelected);

            // dereference shadows
            if (pObject)
                if (_somIsA(pObject, _WPShadow))
                    pObject = _wpQueryShadowedObject(pObject, TRUE);

            // check if it's a file-system object
            if (pObject)
                if (_somIsA(pObject, _WPFileSystem))
                    if (_wpQueryFilename(pObject, szRealName, fFullPath))
                        sprintf(szToClipboard+strlen(szToClipboard), "%s ", szRealName);

            // compare the selection with pmrcSelf
            if (pmrcSelected == pmrcSelf)
                fSingleMode = FALSE;
        }
    } while ((pmrcSelected != 0) && (((ULONG)pmrcSelected) != -1));

    if (fSingleMode)
    {
        // if somSelf's record core does NOT have the "selected"
        // emphasis: this means that the user has requested a
        // context menu for an object other than the selected
        // objects in the folder, or the folder's context menu has
        // been opened: we will only copy somSelf then.

        CHAR       szRealName[CCHMAXPATH];

        WPObject *pObject = somSelf;
        if (_somIsA(pObject, _WPShadow))
            pObject = _wpQueryShadowedObject(pObject, TRUE);

        if (pObject)
            if (_somIsA(pObject, _WPFileSystem))
                if (_wpQueryFilename(pObject, szRealName, fFullPath))
                    sprintf(szToClipboard, "%s ", szRealName);
    }

    ulLength = strlen(szToClipboard);
    if (ulLength)
    {
        // something was copied:
        szToClipboard[ulLength-1] = '\0'; // remove last space

        // copy to clipboard (stolen from PMREF)
        if (WinOpenClipbrd(hab))
        {
            if (0 == DosAllocSharedMem((PVOID*)(&pszDest),       // pointer to shared memory object
                                                NULL,            // use unnamed shared memory
                                                ulLength,        // include 0 byte (used to be last space)
                                                PAG_WRITE  |     // allow write access
                                                PAG_COMMIT |     // commit the shared memory
                                                OBJ_GIVEABLE))   // make pointer giveable
            {
                strcpy(pszDest, szToClipboard);
                WinEmptyClipbrd(hab);
                fSuccess = WinSetClipbrdData(hab,       // anchor-block handle
                                             (ULONG)pszDest, // pointer to text data
                                             CF_TEXT,        // data is in text format
                                             CFI_POINTER);   // passing a pointer
            }
            WinCloseClipbrd(hab);   // moved V0.9.0
        }
    }

    return (fSuccess);
}

/*
 *@@ wpshQueryDraggedObject:
 *      this helper function can be used with wpDragOver
 *      and/or wpDrop to resolve a DRAGITEM to a WPS object.
 *      This supports both DRM_OBJECT and DRM_OS2FILE
 *      rendering mechanisms.
 *
 *      If (ppObject != NULL), the object is resolved and
 *      written into that pointer as a SOM pointer to the
 *      object.
 *
 *      Returns:
 *      -- 0: invalid object or mechanism not supported.
 *      -- 1: DRM_OBJECT mechanism, *ppObject is valid.
 *      -- 2: DRM_OS2FILE mechanism, *ppObject is valid.
 *
 *@@added V0.9.1 (2000-02-01) [umoeller]
 */

ULONG wpshQueryDraggedObject(PDRAGITEM pdrgItem,
                             WPObject **ppObjectFound)
{
    ULONG   ulrc = 0;

    // check DRM_OBJECT
    if (DrgVerifyRMF(pdrgItem,
                     "DRM_OBJECT",      // mechanism
                     NULL))             // any format
    {
        // get the object pointer:
        // the WPS stores the MINIRECORDCORE in drgItem.ulItemID
        WPObject *pObject = OBJECT_FROM_PREC(pdrgItem->ulItemID);
        if (pObject)
        {
            ulrc = 1;
            if (ppObjectFound)
                *ppObjectFound = pObject;
        }
    }
    // check DRM_FILE (used by other PM applications)
    else if (DrgVerifyRMF(pdrgItem,
                          "DRM_OS2FILE",       // mechanism
                          NULL))            // any format
    {
        CHAR    szFullFile[2*CCHMAXPATH];
        ULONG   cbFullFile;
        // get source directory; this always ends in "\"
        cbFullFile = DrgQueryStrName(pdrgItem->hstrContainerName, // source container
                                     sizeof(szFullFile),
                                     szFullFile);
        if (cbFullFile)
        {
            // append file name to source directory
            if (DrgQueryStrName(pdrgItem->hstrSourceName,
                                sizeof(szFullFile) - cbFullFile,
                                szFullFile + cbFullFile))
            {
                ulrc = 2;
                if (ppObjectFound)
                    *ppObjectFound = _wpclsQueryObjectFromPath(_WPFileSystem,
                                                               szFullFile);
            }
        }
    }

    return (ulrc);
}

#ifdef __DEBUG__
    /*
     *@@ wpshIdentifyRestoreID:
     *      this returns a string to identify the
     *      "restore ID" used in wpRestoreString,
     *      wpRestoreData, wpRestoreLong.
     *
     *      This is useful for debugging all those
     *      keys that are undocumented.
     *
     *      This returns a static PSZ, so do not
     *      free it.
     *
     *@@added V0.9.1 (2000-01-17) [umoeller]
     */

    PSZ wpshIdentifyRestoreID(PSZ pszClass,     // in: class name (as in wpRestore*)
                              ULONG ulKey)      // in: value ID (as in wpRestore*)
    {
        if (strcmp(pszClass, "WPObject") == 0)
        {
            switch (ulKey)
            {
                case 1:
                    return ("IDKEY_OBJID");
                case 2:
                    return ("IDKEY_OBJHELPPANEL");
                case 6:
                    return ("IDKEY_OBJSZID");
                case 7:
                    return ("IDKEY_OBJSTYLE");
                case 8:
                    return ("IDKEY_OBJMINWIN");
                case 9:
                    return ("IDKEY_OBJCONCURRENT");
                case 10:
                    return ("IDKEY_OBJVIEWBUTTON");
                case 11:
                    return ("IDKEY_OBJDATA");
                case 12:
                    return ("IDKEY_OBJSTRINGS");
            }
        }
        else if (strcmp(pszClass, "WPFileSystem") == 0)
        {
            switch (ulKey)
            {
                case 4:
                    return ("IDKEY_FSYSMENUCOUNT");
                case 3:
                    return ("IDKEY_FSYSMENUARRAY");
            }
        }
        else if (strcmp(pszClass, "WPFolder") == 0)
        {
            switch (ulKey)
            {
                case IDKEY_FDRCONTENTATTR    : // 2900
                    return ("IDKEY_FDRCONTENTATTR");
                case IDKEY_FDRTREEATTR       : // 2901
                    return ("IDKEY_FDRTREEATTR");
                case IDKEY_FDRCVLFONT        : // 2902
                    return ("IDKEY_FDRCVLFONT");
                case IDKEY_FDRCVNFONT        : // 2903
                    return ("IDKEY_FDRCVNFONT");
                case IDKEY_FDRCVIFONT        : // 2904
                    return ("IDKEY_FDRCVIFONT");
                case IDKEY_FDRTVLFONT        : // 2905
                    return ("IDKEY_FDRTVLFONT");
                case IDKEY_FDRTVNFONT        : // 2906
                    return ("IDKEY_FDRTVNFONT");
                case IDKEY_FDRDETAILSATTR    : // 2907
                    return ("IDKEY_FDRDETAILSATTR");
                case IDKEY_FDRDVFONT         : // 2908
                    return ("IDKEY_FDRDVFONT");
                case IDKEY_FDRDETAILSCLASS   : // 2909
                    return ("IDKEY_FDRDETAILSCLASS");
                case IDKEY_FDRICONPOS        : // 2910
                    return ("IDKEY_FDRICONPOS");
                case IDKEY_FDRINVISCOLUMNS   : // 2914
                    return ("IDKEY_FDRINVISCOLUMNS");
                case IDKEY_FDRINCCLASS       : // 2920
                    return ("IDKEY_FDRINCCLASS");
                case IDKEY_FDRINCNAME        : // 2921
                    return ("IDKEY_FDRINCNAME");
                case IDKEY_FDRFSYSSEARCHINFO : // 2922
                    return ("IDKEY_FDRFSYSSEARCHINFO");
                case IDKEY_FILTERCONTENT     : // 2923
                    return ("IDKEY_FILTERCONTENT");
                case IDKEY_CNRBACKGROUND     : // 2924
                    return ("IDKEY_CNRBACKGROUND");
                case IDKEY_FDRINCCRITERIA    : // 2925
                    return ("IDKEY_FDRINCCRITERIA");
                case IDKEY_FDRICONVIEWPOS    : // 2926
                    return ("IDKEY_FDRICONVIEWPOS");
                case IDKEY_FDRSORTCLASS      : // 2927
                    return ("IDKEY_FDRSORTCLASS");
                case IDKEY_FDRSORTATTRIBS    : // 2928
                    return ("IDKEY_FDRSORTATTRIBS");
                case IDKEY_FDRSORTINFO       : // 2929
                    return ("IDKEY_FDRSORTINFO");
                case IDKEY_FDRSNEAKYCOUNT    : // 2930
                    return ("IDKEY_FDRSNEAKYCOUNT");
                case IDKEY_FDRLONGARRAY      : // 2931
                    return ("IDKEY_FDRLONGARRAY");
                case IDKEY_FDRSTRARRAY       : // 2932
                    return ("IDKEY_FDRSTRARRAY");
                case IDKEY_FDRCNRBACKGROUND  : // 2933
                    return ("IDKEY_FDRCNRBACKGROUND");
                case IDKEY_FDRBKGNDIMAGEFILE : // 2934
                    return ("IDKEY_FDRBKGNDIMAGEFILE");
                case IDKEY_FDRBACKGROUND     : // 2935
                    return ("IDKEY_FDRBACKGROUND");
                case IDKEY_FDRSELFCLOSE      : // 2936
                    return ("IDKEY_FDRSELFCLOSE");

                case 2937:
                    return ("IDKEY_FDRODMENUBARON");
                case 2938:
                    return ("IDKEY_FDRGRIDINFO");
                case 2939:
                    return ("IDKEY_FDRTREEVIEWCONTENTS");
            }
        }

        return ("unknown");
    }

    /*
     *@@ wpshDumpTaskRec:
     *
     *@@added V0.9.1 (2000-02-01) [umoeller]
     */

    VOID wpshDumpTaskRec(WPObject *somSelf,
                         PSZ pszMethodName,
                         PTASKREC pTaskRec)
    {
        _Pmpf(("%s: dumping task rec for %s", pszMethodName, _wpQueryTitle(somSelf) ));

        if (pTaskRec)
        {
            ULONG   ul = 0;

            while (pTaskRec)
            {
                _Pmpf(("Index: %d", ul));
                _Pmpf(("    useCount: %d", pTaskRec->useCount));
                _Pmpf(("    pStdDlg: 0x%lX", pTaskRec->pStdDlg));
                _Pmpf(("    folder: %s", _wpQueryTitle(pTaskRec->folder) ));
                _Pmpf(("    xOrigin: %d", pTaskRec->xOrigin));
                _Pmpf(("    yOrigin: %d", pTaskRec->yOrigin));
                _Pmpf(("    pszTitle: %s", pTaskRec->pszTitle));
                _Pmpf(("    posAfterRecord: 0x%lX", pTaskRec->positionAfterRecord));
                _Pmpf(("    keepAssocs: %d", pTaskRec->fKeepAssociations));
                _Pmpf(("    pReserved: 0x%lX", pTaskRec->pReserved));

                pTaskRec = pTaskRec->next;
                ul++;
            }
        }
        else
            _Pmpf(("    pTaskRec is NULL"));
    }
#endif
