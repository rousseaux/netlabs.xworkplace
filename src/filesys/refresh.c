
/*
 *@@sourcefile refresh.c:
 *      code for the folder auto-refresh replacement, if
 *      the replacement is enabled.
 *
 *      If the user has replaced folder auto-refresh in
 *      XWPSetup, on Desktop startup, krnReplaceWheelWatcher
 *      prevents the WPS "wheel watcher" thread from starting,
 *      which usually processes the Dos*ChangeNotify notifications.
 *
 *      Since these APIs can only be used ONCE on the
 *      entire system, we can't compete with that thread.
 *      This is also why enabling the replacement requires
 *      a Desktop restart, because we can only stop that thread
 *      if it has not done any processing yet.
 *
 *      Instead, with XWP, the following three (!) threads are
 *      responsible for processing folder auto-refresh:
 *
 *      1)  The "Sentinel" thread (refr_fntSentinel). This is
 *          our replacement for the WPS wheel watcher.
 *
 *          This gets started on Desktop startup from
 *          krnReplaceWheelWatcher.
 *          It does not have a PM message queue and now
 *          handles the Dos*ChangeNotify APIs.
 *
 *      2)  The "Find Folder" thread gets started by the
 *          Sentinel thread and processes the notification
 *          in more detail. Since the sentinel must get the
 *          notifications processed as quickly as possible
 *          (because these come directly from the kernel),
 *          it does not check them but posts them to the
 *          "find folder" thread in an XWPNOTIFY struct.
 *
 *          For each XWPNOTIFY, the "find folder" thread checks
 *          if the notification is for a folder that is already
 *          awake. If so, it adds the notification to the
 *          folder's notification queue.
 *
 *      3)  The "Pump thread" is then responsible for actually
 *          refreshing the folders after a reasonable delay.
 *          This thread sorts out notifications that cancel
 *          each other out or need not be processed at all.
 *          This is what the WPS "Ager" thread normally does.
 *
 *      Note that all these three threads are started on WPS
 *      startup if folder auto-refresh has been replaced in
 *      XWPSetup. By contrast, the "Folder auto-refresh" setting
 *      in "Workplace shell" (which is in the global settings) is only
 *      respected by the Sentinel thread. Disabling that setting
 *      does not stop the threads from running... it will only
 *      disable posting messages from the Sentinel to the
 *      "find folder" thread, thus effectively disabling refresh.
 *
 *      This file is ALL new with V0.9.9 (2001-01-29) [umoeller]
 *
 *      Function prefix for this file:
 *      --  refr*
 *
 *@@added V0.9.9 (2001-01-31) [umoeller]
 *@@header "filesys\refresh.h"
 */

/*
 *      Copyright (C) 2001-2002 Ulrich M�ller.
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

#define INCL_DOSSEMAPHORES
#define INCL_DOSPROCESS
#define INCL_DOSEXCEPTIONS
#define INCL_DOSRESOURCES
#define INCL_DOSMISC
#define INCL_DOSERRORS

#define INCL_WINWINDOWMGR
#define INCL_WINMESSAGEMGR
#include <os2.h>

// C library headers
#include <stdio.h>
#include <setjmp.h>

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\threads.h"            // thread helpers

// SOM headers which don't crash with prec. header files
#include "xfldr.ih"

// XWorkplace implementation headers
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

#include "filesys\filesys.h"            // various file-system object implementation code
#include "filesys\folder.h"             // XFolder implementation
#include "filesys\refresh.h"            // folder auto-refresh

// other SOM headers
#pragma hdrstop                 // VAC++ keeps crashing otherwise

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

// find folder thread
static THREADINFO      G_tiFindFolder = {0};
static HWND            G_hwndFindFolder = NULLHANDLE;
static HEV             G_hevFindFolderReady = NULLHANDLE;

// pump thread
static THREADINFO      G_tiPumpThread = {0};
static HEV             G_hevNotificationPump = NULLHANDLE;

// global list of all notifications (auto-free)
static LINKLIST        G_llAllNotifications;

static BOOL            G_fExitAllRefreshThreads = FALSE;
            // this is set to TRUE as an emergency exit if
            // one of the refresh threads crashed. All three
            // threads will then terminate.

// delay after which notifications are pumped into the folders
#define PUMP_AGER_MILLISECONDS      4000

/* ******************************************************************
 *
 *   Notifications list API
 *
 ********************************************************************/

/*
 *@@ refrAddNotification:
 *      adds the specified notification to the linked
 *      list of the folder that is specified in the
 *      notification.
 *
 *      Called on the find-folder thread.
 *
 *      Preconditions:
 *
 *      -- The caller must have the global WPS notify
 *         mutex.
 *
 *@@added V0.9.9 (2001-02-04) [umoeller]
 *@@changed V0.9.12 (2001-05-22) [umoeller]: fixed synchronization with folder instance data
 *@@changed V0.9.16 (2002-01-09) [umoeller]: fixed missing event sem post
 */

VOID refrAddNotification(PXWPNOTIFY pNotify)
{
    XFolderData *somThis = XFolderGetData(pNotify->pFolder);
    (_cNotificationsPending)++;

    // store current system time in pNotify
    pNotify->ulMS = doshQuerySysUptime();

    // append to the global list (auto-free)
    lstAppendItem(&G_llAllNotifications, pNotify);

    // post the event sem for the pump thread
    // V0.9.16 (2002-01-09) [umoeller]: moved this here,
    // this was missing for full refresh
    DosPostEventSem(G_hevNotificationPump);
}

/*
 *@@ refrRemoveNotification:
 *      removes the specified notification.
 *
 *      If you know the list node of the notification,
 *      you can specify it in pGlobalNode to save
 *      this function from having to search the
 *      entire list for it.
 *
 *      WARNING: pNotify->pFolder might be in the
 *      process of being destroyed, so DO NOT MAKE
 *      ANY METHOD CALLS on the folder.
 *
 *      Preconditions:
 *
 *      -- The caller must have the global WPS notify
 *         mutex.
 *
 *@@added V0.9.9 (2001-02-01) [umoeller]
 *@@changed V0.9.12 (2001-05-22) [umoeller]: fixed synchronization with folder instance data
 */

VOID refrRemoveNotification(PXWPNOTIFY pNotify,
                            PLISTNODE pGlobalNode)      // or NULL if unknown (slower)
{
    XFolderData *somThis = XFolderGetData(pNotify->pFolder);
    (_cNotificationsPending)--;

    if (!pGlobalNode)
        // not specified: search it then
        pGlobalNode = lstNodeFromItem(&G_llAllNotifications,
                                      pNotify);

    if (pGlobalNode)
        // remove from global list
        lstRemoveNode(&G_llAllNotifications,
                      pGlobalNode);        // auto-free
}

/*
 *@@ refrClearFolderNotifications:
 *      removes all pending notifications for the
 *      specified folder without processing them.
 *
 *      Gets called
 *
 *      --  from XFolder::wpUnInitData on whatever
 *          thread that method gets called on;
 *
 *      --  from PumpAgedNotification on
 *          RCNF_XWP_FULLREFRESH before we run a
 *          full refresh on the folder because then
 *          all the notifications don't make much
 *          sense any more.
 *
 *      WARNING: pNotify->pFolder might be in the
 *      process of being destroyed, so DO NOT MAKE
 *      ANY METHOD CALLS on the folder.
 *
 *      Preconditions:
 *
 *      -- The caller must have the global WPS notify
 *         mutex.
 *
 *@@added V0.9.9 (2001-02-01) [umoeller]
 *@@changed V0.9.12 (2001-05-22) [umoeller]: fixed synchronization with folder instance data
 */

VOID refrClearFolderNotifications(WPFolder *pFolder)
{
    // go thru the entire global list
    // and remove all notify nodes which
    // point to this folder

    PLISTNODE pNode = lstQueryFirstNode(&G_llAllNotifications);
    while (pNode)
    {
        PLISTNODE pNext = pNode->pNext;
        PXWPNOTIFY pNotify = (PXWPNOTIFY)pNode->pItemData;
        if (pNotify->pFolder == pFolder)
            // kick this out
            refrRemoveNotification(pNotify,
                                   pNode);
        pNode = pNext;
    }
}

/* ******************************************************************
 *
 *   Refresh thread
 *
 ********************************************************************/

/*
 *@@ fntOverflowRefresh:
 *      transient thread which refreshes a folder after
 *      an overflow. Gets started from PumpAgedNotification.
 *
 *      This thread is created with a PM message queue.
 *
 *@@added V0.9.12 (2001-05-19) [umoeller]
 */

static VOID _Optlink fntOverflowRefresh(PTHREADINFO ptiMyself)
{
    TRY_LOUD(excpt1)
    {
        WPFolder *pFolder = (WPFolder*)ptiMyself->ulData;

        _wpRefresh(pFolder, NULLHANDLE, NULL);
    }
    CATCH(excpt1) {} END_CATCH();
}

/* ******************************************************************
 *
 *   Pump thread
 *
 ********************************************************************/

/*
 *@@ Refresh:
 *
 *@@added V0.9.16 (2002-01-04) [umoeller]
 */

static VOID Refresh(WPObject *pobj)
{
    if (_somIsA(pobj, _WPFolder))
        // is folder: do not call _wpRefresh, this
        // goes into the subfolders
        fsysRefreshFSInfo(pobj, NULL);
    else
        // regular fs object: call wpRefresh directly,
        // which we might have replaced if icon replacements
        // are on
        _wpRefresh(pobj, NULLHANDLE, NULL);
}

#define REMOVE_NOTHING  0
#define REMOVE_NODE     1
#define REMOVE_FOLDER   2

/*
 *@@ PumpAgedNotification:
 *      called from PumpNotifications for each notification
 *      which has aged enough to be added to the folder
 *      specified in the XWPNOTIFY struct.
 *
 *      This function must return one of:
 *
 *      -- REMOVE_NOTHING: leave the notification on the
 *         list.
 *
 *      -- REMOVE_NODE: caller should remove this one node.
 *
 *      -- REMOVE_FOLDER: _this_ function has removed all
 *         notifications for the folder specified in pNotify.
 *         This is to notify the caller that the notifications
 *         list might have changed totally and internal
 *         pointers are no longer valid.
 *
 *      Preconditions:
 *
 *      -- The caller must hold the WPS notify mutex.
 *
 *      Postconditions:
 *
 *      -- Caller must free pNotify according to the return
 *         value.
 *
 *@@added V0.9.9 (2001-02-04) [umoeller]
 *@@changed V0.9.12 (2001-05-18) [umoeller]: added full refresh on overflow
 *@@changed V0.9.12 (2001-05-22) [umoeller]: fixed synchronization with folder instance data
 *@@changed V0.9.12 (2001-05-31) [umoeller]: added REMOVE_* return value for caller
 *@@changed V0.9.16 (2001-10-28) [umoeller]: added support for RCNF_CHANGED
 *@@changed V0.9.16 (2002-01-09) [umoeller]: folder refresh wasn't always working, fixed
 */

static ULONG PumpAgedNotification(PXWPNOTIFY pNotify)
{
    // per default, remove the node V0.9.12 (2001-05-29) [umoeller]
    ULONG ulrc = REMOVE_NODE;

    switch (pNotify->CNInfo.bAction)
    {
        case RCNF_FILE_ADDED:
        case RCNF_DIR_ADDED:
            _wpclsQueryObjectFromPath(_WPFileSystem,
                                      pNotify->CNInfo.szName);
                    // this will wake the object up
                    // if it hasn't been awakened yet
        break;

        case RCNF_CHANGED:      // V0.9.16 (2001-10-28) [umoeller]
        {
            WPFileSystem *pobj;
            if (pobj = _wpclsFileSysExists(_WPFileSystem,
                                           pNotify->pFolder,
                                           pNotify->pShortName,
                                           // attFile: this is probably no
                                           // directory, or is it?
                                           0))
            {
                Refresh(pobj);
            }
        }
        break;

        case RCNF_FILE_DELETED:
        case RCNF_DIR_DELETED:
        {
            // loop thru the folder contents (without refreshing)
            // to check if we have a WPFileSystem with this name
            // already
            WPFileSystem *pobj;
            if (pobj = _wpclsFileSysExists(_WPFileSystem,
                                           pNotify->pFolder,
                                           pNotify->pShortName,
                                           // attrFile: either directory or file
                                           (pNotify->CNInfo.bAction == RCNF_DIR_DELETED)
                                                ? FILE_DIRECTORY
                                                : 0))
            {
                // yes, we have an FS object of that name:
                // check if the file still physically exists...
                // we better not kill it if it does
                FILESTATUS3 fs3;
                APIRET arc = DosQueryPathInfo(pNotify->CNInfo.szName,
                                              FIL_STANDARD,
                                              &fs3,
                                              sizeof(fs3));

                // _Pmpf((__FUNCTION__ ": DosQueryPathInfo ret'd %d", arc));

                switch (arc)
                {
                    case ERROR_FILE_NOT_FOUND:
                            // as opposed to what CPREF says,
                            // DosQueryPathInfo does return
                            // ERROR_FILE_NOT_FOUND... sigh
                    case ERROR_PATH_NOT_FOUND:
                    {
                        // ok, free the object... this needs some hacks
                        // too because otherwise the WPS modifies the
                        // folder flags internally during this processing.
                        // I believe this happens during wpMakeDormant,
                        // because the FOI_POPULATED* flags are turned off
                        // even though we have replaced wpFree.

                        ULONG flFolder = _wpQueryFldrFlags(pNotify->pFolder);
                        _wpModifyFldrFlags(pNotify->pFolder,
                                           FOI_POPULATEDWITHALL | FOI_POPULATEDWITHFOLDERS,
                                           0);
                        _wpFree(pobj);
                        _wpSetFldrFlags(pNotify->pFolder, flFolder);
                    }
                    break;

                    case NO_ERROR:
                        // the file has reappeared.
                        // DO NOT FREE IT, or we would delete a
                        // valid file; instead, we refresh the
                        // FS object
                        // V0.9.16 (2001-12-06) [umoeller]
                        Refresh(pobj);
                }
            }
            // else object not in folder: no problem there
        }
        break;

        /*
         * RCNF_XWP_FULLREFRESH:
         *      special XWP notify code if we
         *      encountered an overflow and
         *      should do a full refresh on the
         *      folder.
         */

        case RCNF_XWP_FULLREFRESH:
        {
            // get the folder pointer first because
            // we're deleting the node
            WPFolder *pFolder = pNotify->pFolder;
            ULONG flFolder = _wpQueryFldrFlags(pNotify->pFolder);

            // kick out all pending notifications
            // for this folder
            refrClearFolderNotifications(pFolder);

            // tell caller that we nuked the list
            ulrc = REMOVE_FOLDER;

            // refresh the folder if it's not currently refreshing
            if (0 == (flFolder & (FOI_POPULATEINPROGRESS | FOI_REFRESHINPROGRESS)))
            {
                // we need a full refresh on the folder...
                // set FOI_ASYNCREFRESHONOPEN, clear
                // FOI_POPULATEDWITHFOLDERS | FOI_POPULATEDWITHALL,
                // which will cause the folder contents to be refreshed
                // on open
                // fixed flags V0.9.16 (2002-01-09) [umoeller]
                _wpModifyFldrFlags(pFolder,
                                   FOI_ASYNCREFRESHONOPEN | FOI_POPULATEDWITHFOLDERS | FOI_POPULATEDWITHALL,
                                   FOI_ASYNCREFRESHONOPEN);

                // if the folder is currently open, do a
                // full refresh NOW
                if (_wpFindViewItem(pFolder,
                                    VIEW_ANY,
                                    NULL))
                {
                    // alright, refresh NOW.... however, we can't
                    // do this on this thread while we're holding
                    // the WPS notify mutex, so start a transient
                    // thread just for refreshing
                    thrCreate(NULL,
                              fntOverflowRefresh,
                              NULL,
                              "OverflowRefresh",
                              THRF_PMMSGQUEUE | THRF_TRANSIENT,
                              // thread param: folder pointer
                              (ULONG)pFolder);
                }
            }
        }
        break;
    }

    return (ulrc);
}

/*
 *@@ PumpNotifications:
 *      called from the Pump thread if any notifications
 *      appear to have aged enough to be pumped into
 *      the respective folders.
 *
 *      This calls PumpAgedNotification in turn.
 *
 *      Returns 0 if there are no notifications left.
 *      Otherwise it returns the milliseconds from now
 *      when the next notification is to be pumped.
 *
 *      Preconditions:
 *
 *      -- The caller must hold the WPS notify mutex.
 *
 *@@added V0.9.9 (2001-02-01) [umoeller]
 *@@changed V0.9.9 (2001-04-07) [umoeller]: disabled overflow handling for now, which kept crashing
 *@@changed V0.9.12 (2001-05-31) [umoeller]: fixed more list crashes on overflow
 */

static BOOL PumpNotifications(VOID)
{
    BOOL        fNotificationsLeft = FALSE;
    ULONG       ulrc = 100*1000;
    PLISTNODE   pGlobalNodeThis;

    // go thru ALL notifications on the list
    // and check if they have aged enough to
    // be processed
    if (pGlobalNodeThis = lstQueryFirstNode(&G_llAllNotifications))
    {
        // get current milliseconds
        ULONG       ulMSNow = doshQuerySysUptime();

        // _Pmpf((__FUNCTION__ ": %d notifications to be pumped.", lstCountItems(&G_llAllNotifications)));

        while (pGlobalNodeThis)
        {
            PXWPNOTIFY  pNotify = (PXWPNOTIFY)pGlobalNodeThis->pItemData;

            // advance to next node before processing...
            // the node might get deleted
            PLISTNODE   pNext = pGlobalNodeThis->pNext;

            // pNotify->ulMS holds the milliseconds offset
            // at the time the notification was added.
            // If that offset plus PUMP_AGER_MILLISECONDS
            // is smaller than ulMSNow, the notification
            // has aged enough.
            if (pNotify->ulMS + PUMP_AGER_MILLISECONDS <= ulMSNow)
            {
                // aged enough:
                // have it processed, and evaluate
                // the return value (our list might
                // have been modified)
                // V0.9.12 (2001-05-31) [umoeller]
                switch (PumpAgedNotification(pNotify))
                {
                    case REMOVE_NODE:
                        // ok, remove the node:
                        lstRemoveNode(&G_llAllNotifications,
                                      pGlobalNodeThis);
                    break;

                    case REMOVE_FOLDER:
                        // function has nuked all
                        // notifications for the folder itself:
                        // start over with the list, pNext
                        // can be invalid!
                        pNext = lstQueryFirstNode(&G_llAllNotifications);
                                // can be NULL if nothing left
                        // and recount
                        fNotificationsLeft = FALSE;
                    break;
                }
            }
            else
            {
                // not yet aged enough:
                ULONG ulMStoGoThis = pNotify->ulMS + PUMP_AGER_MILLISECONDS - ulMSNow;
                if (ulMStoGoThis < ulrc)
                    ulrc = ulMStoGoThis;
                fNotificationsLeft = TRUE;
            }

            pGlobalNodeThis = pNext;
                    // pNext is the next node, unless REMOVE_FOLDER
                    // was returned above; can be NULL in any case
                    // V0.9.12 (2001-05-31) [umoeller]
        }
    }

    if (!fNotificationsLeft)
        ulrc = 0;

    return (ulrc);
}

/*
 *@@ fntPumpThread:
 *      third refresh thread which finally does the
 *      automatic folder update.
 *
 *      This is called "pump" thread because it permanently
 *      works in the background to age the notifications
 *      which come in and then squeezes them into open
 *      folders.
 *
 *      This thread does have a PM message queue because
 *      we are inserting stuff into folder containers,
 *      but it has no object window.
 *
 *@@added V0.9.9 (2001-02-01) [umoeller]
 */

static VOID _Optlink fntPumpThread(PTHREADINFO ptiMyself)
{
    // QMSG qmsg;
    ULONG   ulWaitTime = 1000;

    WinCancelShutdown(ptiMyself->hmq, TRUE);

    while (!G_fExitAllRefreshThreads)
    {
        BOOL    fSemOwned = FALSE;
        ULONG   ulPostCount = 0;

        // _Pmpf((__FUNCTION__ ": pump thread blocking on HEV."));

        WinWaitEventSem(G_hevNotificationPump, ulWaitTime);

        // _Pmpf((__FUNCTION__ ": pump event posted."));

        TRY_LOUD(excpt1)
        {
            if (fSemOwned = fdrGetNotifySem(SEM_INDEFINITE_WAIT))
            {
                // only if we got the mutex, reset the event
                DosResetEventSem(G_hevNotificationPump, &ulPostCount);
                ulWaitTime = PumpNotifications();
                if (ulWaitTime == 0)
                    // no notifications left now:
                    ulWaitTime = SEM_INDEFINITE_WAIT;
            }
            else
                // we couldn't get the semaphore:
                // block for a second then
                ulWaitTime = 1000;
        }
        CATCH(excpt1)
        {
            // if we crashed, stop all refresh threads. This
            // can become very annoying otherwise.
            G_fExitAllRefreshThreads = TRUE;
            WinPostMsg(G_hwndFindFolder, WM_QUIT, 0, 0);
        } END_CATCH();

        if (fSemOwned)
        {
            fdrReleaseNotifySem();
            fSemOwned = FALSE;
        }
    }
}

/* ******************************************************************
 *
 *   Find Folder thread
 *
 ********************************************************************/

/*
 *@@ AddNotifyIfNotRedundant:
 *      adds the specified notification to both the global
 *      notifications list and the folder's notifications
 *      list (specified by pNotify->pFolder), if it is
 *      considered "important".
 *
 *      A notification is "important" if there is no
 *      opposite notification on the folder's list already.
 *      For example, if we get a "file delete" for C:\temp\12345.tmp
 *      and there is already a "file add" for the same file on the
 *      list, we can simply ignore the new notification (and delete
 *      the old one as well).
 *
 *      Returns TRUE if the notification was important and
 *      added to the global list and the respective folder.
 *      In that case, the caller MUST NOT FREE the notification.
 *
 *      Otherwise the notification should be freed.
 *
 *      Preconditions:
 *
 *      -- pNotify->pFolder has been set and is awake.
 *
 *      -- pNotify must have been malloc()'d.
 *
 *      -- The caller must hold the WPS notify mutex.
 *         This is essential because a folder might be in
 *         the process of being deleted or whatever when
 *         the notifications come in. The WPS flushes all
 *         notifications for a folder when a folder is
 *         deleted under the protection of the notify
 *         mutex, so we're safe then.
 *
 *@@added V0.9.9 (2001-01-29) [umoeller]
 *@@changed V0.9.16 (2001-10-28) [umoeller]: added support for RCNF_CHANGED
 */

static BOOL AddNotifyIfNotRedundant(PXWPNOTIFY pNotify)
{
    BOOL    fAddThis = FALSE;

    if (pNotify)
    {
        // hack the folder's instance data directly...
        // XFolderData *somThat = XFolderGetData(pNotify->pFolder);

        BYTE        bActionThis = pNotify->CNInfo.bAction;
        BYTE        bOpposite = 0;

        // let's say: add this one now
        fAddThis = TRUE;

        // does the folder have a notification list at
        // all yet?

        /*
         *  (1) drop redundant processing:
         *
         */

        // for FILE_DELETED, drop previous FILE_ADDED (temp file)
        if (bActionThis == RCNF_FILE_DELETED)
            bOpposite = RCNF_FILE_ADDED;
        // for DIR_DELETED, drop previous DIR_ADDED
        else if (bActionThis == RCNF_DIR_DELETED)
            bOpposite = RCNF_DIR_ADDED;

        // besides, drop RCNF_CHANGED if we have a
        // RCNF_FILE_ADDED in the queue already
        if (    (bOpposite)
             || (bActionThis == RCNF_CHANGED)
           )
        {
            // yes, check redundancy:
            PLISTNODE pNode = lstQueryFirstNode(&G_llAllNotifications);
            while (pNode)
            {
                PLISTNODE pNext = pNode->pNext;
                PXWPNOTIFY  pNotifyThat = (PXWPNOTIFY)pNode->pItemData;
                BYTE        bActionThat = pNotifyThat->CNInfo.bAction;
                if (bActionThat == bOpposite)
                {
                    // we have an opposite action for that folder:
                    // compare the short names
                    if (!stricmp(pNotify->pShortName,
                                 pNotifyThat->pShortName))
                    {
                        // same file name:
                        // drop it, it's redundant
                        fAddThis = FALSE;

                        // and remove the old notification as well
                        refrRemoveNotification(pNotifyThat,
                                               pNode);

                        break;
                    }
                }
                // V0.9.16 (2001-10-28) [umoeller]
                else if (    // do not add RCNF_CHANGED if we already have RCNF_CHANGED
                             (    (bActionThis == RCNF_CHANGED)
                               && (bActionThat == RCNF_FILE_ADDED)
                             )
                             // and do not add any duplicate notifications at all
                          || (bActionThis == bActionThat)
                        )
                {
                    if (!stricmp(pNotify->pShortName,
                                 pNotifyThat->pShortName))
                    {
                        fAddThis = FALSE;
                        break;
                    }
                }

                pNode = pNext;
            } // while (pFolderNode)
        } // end if (somThat->pvllNotifications)

        /*
         *  (2) append the notification:
         *
         */

        if (fAddThis)
            refrAddNotification(pNotify);
    } // end if (pNotify)

    return (fAddThis);
}

/*
 *@@ FindFolderForNotification:
 *      this does the hard work of finding a folder
 *      etc. for every notification.
 *
 *      Gets called when fnwpFindFolder receives
 *      NM_NOTIFICATION. pNotify points to the
 *      XWPNOTIFY structure which was allocated by
 *      refr_fntSentinel.
 *
 *      It is the responsibility of this function to
 *      find the folder for the notification and add
 *      the XWPNOTIFY to the folder's queue (and the
 *      global ager queue as well).
 *
 *      If the notify doesn't get added, this func
 *      must free pNotify.
 *
 *@@added V0.9.9 (2001-01-31) [umoeller]
 *@@changed V0.9.12 (2001-05-18) [umoeller]: added "rename" support
 *@@changed V0.9.12 (2001-05-18) [umoeller]: added full refresh on overflow
 *@@changed V0.9.16 (2001-10-28) [umoeller]: added support for RCNF_CHANGED
 *@@changed V0.9.16 (2002-01-09) [umoeller]: added RCNF_DEVICE_ATTACHED, RCNF_DEVICE_DETACHED support
 */

static VOID FindFolderForNotification(PXWPNOTIFY pNotify,
                                      WPFolder **ppLastValidFolder)    // out: last valid folder
{
    static M_WPFileSystem *pclsWPFileSystem = NULL;

    BOOL    fStored = FALSE;
    BOOL    fSemOwned = FALSE;

    TRY_LOUD(excpt2)
    {
        if (!pclsWPFileSystem)
            pclsWPFileSystem = _WPFileSystem;

        // reset parent's last valid folder; this is
        // only set to something if we find a valid
        // folder below
        *ppLastValidFolder = NULL;

        /*
         *  check the action code
         *
         *  NOTE: the sentinel already filters out codes
         *  that don't apply here. If you add "case" checks
         *  here, add them to the sentinel too.
         */

        switch (pNotify->CNInfo.bAction)
        {
            /*
             * RCNF_MOVED_IN:
             * RCNF_MOVED_OUT:
             *      never seen this.
             */

            /* case RCNF_MOVED_IN:             Ŀ
            case RCNF_MOVED_OUT:                �  filtered out in sentinel already
            break;                             ��
                */

            /*
             * RCNF_FILE_ADDED:
             * RCNF_FILE_DELETED:
             *      file added to or removed from directory.
             *
             *      NOTE: a move from one dir to another results
             *      in FILE_ADDED first, then FILE_DELETED, so
             *      there's nothing special to handle here.
             *
             *  RCNF_DIR_ADDED:
             *  RCNF_DIR_DELETED:
             *      directory added or removed.
             *
             * RCNF_CHANGED:
             *      comes in if a file has changed, i.e.
             *      it already existed and was worked on
             *      (e.g. its size changed).
             *
             * RCNF_OLDNAME:
             * RCNF_NEWNAME:
             *      these two come in for a "rename" sequence.
             *
             *      When a file gets "moved" (in the DosMove sense,
             *      i.e. can be a simple rename too), these two
             *      notifications come in with the full old name
             *      and then the full new name. This needs some
             *      special care... for now, we'll just convert this
             *      to "file deleted" and "file added".
             */

            case RCNF_FILE_ADDED:
            case RCNF_FILE_DELETED:
            case RCNF_DIR_ADDED:
            case RCNF_DIR_DELETED:
            case RCNF_CHANGED:              // added V0.9.16 (2001-10-28) [umoeller]
            case RCNF_OLDNAME:
            case RCNF_NEWNAME:
            {
                // for all these, find the folder first
                PSZ pLastBackslash;
                if (pLastBackslash = strrchr(pNotify->CNInfo.szName, '\\'))
                {
                    BOOL    fRefreshFolderOnOpen = FALSE;

                    /* _Pmpf((__FUNCTION__ ": %s \"%s\"",
                            (pNotify->CNInfo.bAction == RCNF_FILE_ADDED) ? "RCNF_FILE_ADDED"
                                : (pNotify->CNInfo.bAction == RCNF_FILE_DELETED) ? "RCNF_FILE_DELETED"
                                : (pNotify->CNInfo.bAction == RCNF_DIR_ADDED) ? "RCNF_DIR_ADDED"
                                : (pNotify->CNInfo.bAction == RCNF_DIR_DELETED) ? "RCNF_DIR_DELETED"
                                : (pNotify->CNInfo.bAction == RCNF_CHANGED) ? "RCNF_CHANGED"
                                : (pNotify->CNInfo.bAction == RCNF_OLDNAME) ? "RCNF_OLDNAME"
                                : (pNotify->CNInfo.bAction == RCNF_NEWNAME) ? "RCNF_NEWNAME"
                                : "unknown code",
                            pNotify->CNInfo.szName));
                       */

                    // first of all, special handling for the rename sequence...
                    // we could use wpSetTitle on the object, but this would
                    // require extra synchronization, so we'll just dump the
                    // old object and re-awake it with the new name. This will
                    // also get the .LONGNAME stuff right then.
                    if (pNotify->CNInfo.bAction == RCNF_OLDNAME)
                        pNotify->CNInfo.bAction = RCNF_FILE_DELETED;
                    else if (pNotify->CNInfo.bAction == RCNF_NEWNAME)
                        pNotify->CNInfo.bAction = RCNF_FILE_ADDED;

                    // store ptr to short name
                    pNotify->pShortName = pLastBackslash + 1;

                    // terminate path name so we get the folder path
                    *pLastBackslash = '\0';

                    // ignore all notifications for folders which
                    // aren't even awake... so first ask the WPS
                    // if it has awakened that folder already
                    pNotify->pFolder = _wpclsQueryAwakeObject(pclsWPFileSystem,
                                                              pNotify->CNInfo.szName);
                                                               // up to backslash now
                    if (    (pNotify->pFolder)
                         && (_somIsA(pNotify->pFolder, _WPFolder))
                       )
                    {
                        ULONG flFolder = _wpQueryFldrFlags(pNotify->pFolder);

                        /* _Pmpf(("    --> folder %s, flags: 0x%lX",
                                    _wpQueryTitle(pNotify->pFolder),
                                    flFolder)); */

                        // ignore if delete or populate or refresh is in progress
                        if (0 == (flFolder & (  FOI_DELETEINPROGRESS
                                              | FOI_POPULATEINPROGRESS
                                              | FOI_REFRESHINPROGRESS)
                           ))
                        {
                            // request the global WPS notify sem;
                            // we'll wait ten seconds for this -- if we can't
                            // get the mutex in that time, we'll just drop
                            // the notification
                            if (fSemOwned = fdrGetNotifySem(10 * 1000))
                            {
                                // see if it's populated; we can safely
                                // drop the notification if the folder
                                // isn't even fully populated because
                                // then the WPS will collect all the file-system
                                // information on populate anyway...

                                PULONG pulFlag = _wpQueryContainerFlagPtr(pNotify->pFolder);

                                // keep the notification?
                                if (    (flFolder & (FOI_POPULATEDWITHALL | FOI_POPULATEDWITHFOLDERS))
                                     || ( (pulFlag) && (*pulFlag) )
                                   )
                                {
                                    // OK, this is worth storing:

                                    // restore the path name that we truncated
                                    // above
                                    *pLastBackslash = '\\';

                                    fStored = AddNotifyIfNotRedundant(pNotify);
                                            // if this returns FALSE, the
                                            // notification is freed below.
                                    /*  _Pmpf(("    AddNotifyIfNotRedundant returned %d.",
                                            fStored)); */

                                    if (    (fStored)
                                         && (pNotify->CNInfo.bAction != RCNF_DIR_DELETED)
                                       )
                                        *ppLastValidFolder = pNotify->pFolder;
                                }
                                else
                                    // folder isn't even populated:
                                    // mark the folder for refresh on next open
                                    fRefreshFolderOnOpen = TRUE;

                            } // end if (fSemOwned)
                            else
                                // can't get the semaphore in time:
                                // mark the folder for refresh
                                fRefreshFolderOnOpen = TRUE;
                        } // end if delete or refresh in progress

                        if (fRefreshFolderOnOpen)
                            // fixed flags V0.9.16 (2002-01-09) [umoeller]
                            _wpModifyFldrFlags(pNotify->pFolder,
                                               FOI_ASYNCREFRESHONOPEN | FOI_POPULATEDWITHFOLDERS | FOI_POPULATEDWITHALL,
                                               FOI_ASYNCREFRESHONOPEN);
                    }
                    // else: folder isn't even awake... then it can't be visible
                    // in a tree view either, so don't bother
                } // if (pLastBackslash)
            }
            break;

            /*
             * RCNF_DEVICE_ATTACHED:
             * RCNF_DEVICE_DETACHED:
             *      these two come in if drive letters change,
             *      e.g. after a net use command or something;
             *      we just refresh the drives folder, I won't
             *      bother with the details of creating drive
             *      objects here.
             *
             *      Support was added with V0.9.16 (2002-01-09) [umoeller].
             */

            case RCNF_DEVICE_ATTACHED:
            case RCNF_DEVICE_DETACHED:
            {
                WPFolder *pDrivesFolder;

                if (pDrivesFolder = _wpclsQueryFolder(_WPFolder,
                                                      (PSZ)WPOBJID_DRIVES,
                                                      TRUE))
                {
                    if (fSemOwned = fdrGetNotifySem(10 * 1000))
                    {
                        // change this notification into a
                        // full refresh of the drives folder,
                        // as if it had overflowed; this will
                        // be properly handled by the pump thread
                        // pNotify->ulMS = ulMSNow;
                        pNotify->pFolder = pDrivesFolder;
                        pNotify->CNInfo.bAction = RCNF_XWP_FULLREFRESH;
                        refrAddNotification(pNotify);
                                // this works
                        fStored = TRUE;
                    }
                }
            }
            break;

        }
    }
    CATCH(excpt2)
    {
        // if we crashed, stop all refresh threads. This
        // can become very annoying otherwise.
        G_fExitAllRefreshThreads = TRUE;
        WinPostMsg(G_hwndFindFolder, WM_QUIT, 0, 0);
    } END_CATCH();

    if (fSemOwned)
        fdrReleaseNotifySem();

    if (!fStored)
        // we have not handled the notification:
        free(pNotify);
}

/*
 *@@ fnwpFindFolder:
 *      object window proc for the "find folder" thread.
 *
 *@@added V0.9.9 (2001-01-29) [umoeller]
 *@@changed V0.9.12 (2001-05-18) [umoeller]: added full refresh on overflow
 */

static MRESULT EXPENTRY fnwpFindFolder(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    static WPFolder *pLastValidFolder = NULL;

    BOOL fSemOwned = FALSE;

    TRY_LOUD(excpt1)
    {
        switch (msg)
        {
            /*
             *@@ NM_NOTIFICATION:
             *      this has an XWPNOTIFY in a newly
             *      allocated buffer in mp1.
             */

            case NM_NOTIFICATION:
                if (mp1)
                    FindFolderForNotification((PXWPNOTIFY)mp1,
                                              &pLastValidFolder);
                            // this sets pLastValidFolder so we
                            // can process overflows
            break;

            /*
             *@@ NM_OVERFLOW:
             *      posted by the sentinel when it gets a
             *      buffer overflow. We have no data in this
             *      case, so we'll just use the last valid
             *      folder we had before (if any) and add
             *      a special notification, which will cause
             *      a full refresh in the pump thread.
             */

            case NM_OVERFLOW:
            {
                if (pLastValidFolder)
                {
                    if (fSemOwned = fdrGetNotifySem(10 * 1000))
                    {
                        // create an XWPNOTIFY with the special "full refresh" flag
                        PXWPNOTIFY pNew = (PXWPNOTIFY)malloc(sizeof(XWPNOTIFY));
                        memset(pNew, 0, sizeof(XWPNOTIFY));
                        pNew->pFolder = pLastValidFolder;
                        pNew->CNInfo.bAction = RCNF_XWP_FULLREFRESH;

                        // clear all notifications for this folder
                        refrClearFolderNotifications(pLastValidFolder);

                        // add "full refresh" notification
                        refrAddNotification(pNew);
                    }

                    // but do this only ONCE
                    pLastValidFolder = NULL;
                }
            }
            break;

            default:
                mrc = WinDefWindowProc(hwnd, msg, mp1, mp2);
        }
    }
    CATCH(excpt1)
    {
        // if we crashed, stop all refresh threads. This
        // can become very annoying otherwise.
        G_fExitAllRefreshThreads = TRUE;
        WinPostMsg(G_hwndFindFolder, WM_QUIT, 0, 0);

        pLastValidFolder = NULL;

    } END_CATCH();

    if (fSemOwned)
        fdrReleaseNotifySem();

    return (mrc);
}

/*
 *@@ fntFindFolder:
 *      second thread for folder auto-refresh. This
 *      has a PM message queue and an object window
 *      (fnwpFindFolder) which does the hard work
 *      for folder auto-refresh.
 *
 *      This receives an NM_NOTIFICATION message
 *      from the Sentinel thread (refr_fntSentinel)
 *      for each notification that comes in.
 *
 *@@added V0.9.9 (2001-01-29) [umoeller]
 */

static VOID _Optlink fntFindFolder(PTHREADINFO ptiMyself)
{
    QMSG qmsg;

    WinCancelShutdown(ptiMyself->hmq, TRUE);

    G_hwndFindFolder = WinCreateWindow(HWND_OBJECT,
                                       WC_STATIC,
                                       "XWPFindFolder",
                                       0,
                                       0, 0, 0, 0,
                                       NULLHANDLE,
                                       HWND_BOTTOM,
                                       0,
                                       NULL,
                                       NULL);
    WinSubclassWindow(G_hwndFindFolder,
                      fnwpFindFolder);

    // tell the sentinel that we're ready
    DosPostEventSem(G_hevFindFolderReady);

    // now enter the message loop
    while (WinGetMsg(ptiMyself->hab, &qmsg, NULLHANDLE, 0, 0))
        WinDispatchMsg(ptiMyself->hab, &qmsg);
                    // loop until WM_QUIT

    WinDestroyWindow(G_hwndFindFolder);
    G_hwndFindFolder = NULLHANDLE;
}

/* ******************************************************************
 *
 *   Sentinel thread
 *
 ********************************************************************/

/*
 *@@ PostXWPNotify:
 *      called from refr_fntSentinel for each
 *      notification to be posted to fnwpFindFolder.
 *
 *      This takes a kernel CNINFO structure and creates
 *      an XWPNOTIFY from it, which is then posted to the
 *      find-folder msg queue.
 *
 *@@added V0.9.12 (2001-05-18) [umoeller]
 */

static VOID PostXWPNotify(PCNINFO pCNInfo)
{
    // create an XWPNOTIFY struct which has
    // a CNINFO in it... the find-folder worker
    // will set up the other fields then
    ULONG   cbThis =   sizeof(XWPNOTIFY)
                     + pCNInfo->cbName
                     + 1;
    PXWPNOTIFY pInfo2;
    if (pInfo2 = (PXWPNOTIFY)malloc(cbThis))
    {
        // copy string and make it zero-terminated
        memcpy(&pInfo2->CNInfo,
               pCNInfo,
               sizeof(CNINFO) + pCNInfo->cbName - 1);
        pInfo2->CNInfo.szName[pCNInfo->cbName] = '\0';

        // notify worker
        if (!WinPostMsg(G_hwndFindFolder,
                        NM_NOTIFICATION,
                        (MPARAM)pInfo2,
                        0))
        {
            // error posting:
            // either find-folder has crashed,
            // or queue is full... in any case,
            // we discard the notification then
            free(pInfo2);
        }
    }
}

/*
 *@@ refr_fntSentinel:
 *      "sentinel" thread function.
 *
 *      This replaces the WPS's "WheelWatcher" thread
 *      and processes the DosFindNotify* functions.
 *
 *      On startup, this creates the "find folder"
 *      and "pump" threads in turn.
 *
 *      This thread does NOT have a PM message queue.
 *
 *      If folder auto-refresh has been replaced in
 *      XWPSetup, this thread gets created from
 *      krnReplaceWheelWatcher and starts the other
 *      two threads (fntFindFolder, fntPumpThread)
 *      in turn.
 *
 *      These three threads get created even if folder
 *      auto-refresh has been disabled on the "View" page
 *      in "Workplace shell". That setting will only disable
 *      posting messages from the Sentinel to the "find folder"
 *      thread.
 *
 *@@added V0.9.9 (2001-01-31) [umoeller]
 *@@changed V0.9.12 (2001-05-18) [umoeller]: multiple notifications in the same buffer were missing, fixed
 *@@changed V0.9.12 (2001-05-18) [umoeller]: sped up allocation, added filter
 *@@changed V0.9.12 (2001-05-20) [umoeller]: fixed stupid pointer error which caused a trap D on DosResetChangeNotify
 *@@changed V0.9.16 (2002-01-09) [umoeller]: added RCNF_DEVICE_ATTACHED, RCNF_DEVICE_DETACHED support
 */

VOID _Optlink refr_fntSentinel(PTHREADINFO ptiMyself)
{
    ULONG   ulPostCount = 0;
    HDIR    hdir = NULLHANDLE;

    // give ourselves higher priority... we must get
    // the notifications off the queue quickly
    DosSetPriority(PRTYS_THREAD,
                   PRTYC_REGULAR,
                   PRTYD_MAXIMUM,
                   0);      // current thread

    lstInit(&G_llAllNotifications,
            TRUE);          // auto-free

    // create a second thread with a PM object window
    // which can process our notifications asynchronously
    DosCreateEventSem(NULL, &G_hevFindFolderReady, 0, FALSE);
    thrCreate(&G_tiFindFolder,
              fntFindFolder,
              NULL,
              "RefreshFindFolder",
              THRF_PMMSGQUEUE,
              0);
    // wait for the object window to be created
    DosWaitEventSem(G_hevFindFolderReady, 20*1000);
    DosResetEventSem(G_hevFindFolderReady, &ulPostCount);
    DosCloseEventSem(G_hevFindFolderReady);

    // and create the "Pump" thread for aging and
    // inserting the notifications into the folder
    DosCreateEventSem(NULL, &G_hevNotificationPump, 0, FALSE);
    thrCreate(&G_tiPumpThread,
              fntPumpThread,
              NULL,
              "RefreshNotificationPump",
              THRF_PMMSGQUEUE,
              0);

    TRY_LOUD(excpt1)
    {
        PCKERNELGLOBALS      pKernelGlobals = krnQueryGlobals();

        // allocate a block of memory... we can't use malloc,
        // or the Dos*Notify APIs will trap
        ULONG       cb = 2 * (sizeof(CNINFO) + CCHMAXPATH);
                                // we can't use more, this will hang the system
        PCNINFO     pBuffer = NULL;
        APIRET      arc = DosAllocMem((PVOID*)&pBuffer,
                                      cb,
                                      PAG_COMMIT
                                        | OBJ_TILE
                                        | PAG_READ
                                        | PAG_WRITE);
                            // ^^^ moved this up, we don't have to reallocate
                            // on every loop

        if ((arc == NO_ERROR) && (pBuffer))
        {
            // now loop forever... at least while the other threads
            // haven't stopped. If the "find folder" thread terminates for
            // whatever reason, it resets G_hwndFindFolder to NULLHANDLE.
            while (G_hwndFindFolder)
            {
                arc = DosOpenChangeNotify(NULL,
                                          0,
                                          &hdir,
                                          0);

                while (    (G_hwndFindFolder)
                        && (arc == NO_ERROR)
                      )
                {
                    TRY_LOUD(excpt2)
                    {
                        ULONG cLogs = 0;
                        arc = DosResetChangeNotify(pBuffer,
                                                   cb,
                                                   &cLogs,
                                                   hdir);
                            // normally we get only one info,
                            // but for the "rename" sequence
                            // there are usually two

                        if (    (G_hwndFindFolder)
                             && (pKernelGlobals->fDesktopPopulated)
                             && (arc == NO_ERROR)
                           )
                        {
                            // we got a notification:
                            // we won't bother with the details
                            // since we better get out of the processing
                            // as quickly as possible.
                            // Instead, we create a copy of this notification
                            // and have the "find folder" thread do the
                            // work of finding the folder etc.
                            PCNINFO pcniThis = pBuffer;
                            while (    (pcniThis)
                                    && (!cmnQuerySetting(sfFdrAutoRefreshDisabled))
                                  )
                            {
                                if (pcniThis->cbName)
                                    // filter out the items which the
                                    // "find folder" thrad doesn't understand
                                    // anyway, this saves us a malloc() call
                                    // V0.9.12 (2001-05-18) [umoeller]
                                    switch (pcniThis->bAction)
                                    {
                                        case RCNF_FILE_ADDED:
                                        case RCNF_FILE_DELETED:
                                        case RCNF_DIR_ADDED:
                                        case RCNF_DIR_DELETED:
                                        // RCNF_MOVED_IN
                                        // RCNF_MOVED_OUT
                                        case RCNF_CHANGED:      // V0.9.16 (2001-10-28) [umoeller]
                                        case RCNF_OLDNAME:
                                        case RCNF_NEWNAME:

                                        // RCNF_DEVICE_ATTACHED and RCNF_DEVICE_DETACHED
                                        // come in for net use command, i.e. a new
                                        // network drive was added
                                        // V0.9.16 (2002-01-09) [umoeller]
                                        case RCNF_DEVICE_ATTACHED:
                                        case RCNF_DEVICE_DETACHED:
                                            PostXWPNotify(pcniThis);
                                        break;

                                    }

                                // go for next entry in the buffer
                                if (pcniThis->oNextEntryOffset)
                                    pcniThis = (PCNINFO)(   (PBYTE)pcniThis
                                                          + pcniThis->oNextEntryOffset
                                                        );
                                else
                                    // no next entry:
                                    break;
                            }
                        }
                        // else: DosResetChangeNotify returned an error...
                        // close the dir handle below
                    }
                    CATCH(excpt2)
                    {
                        // in case of an exception, set an error code
                        // to get out of the while() loop
                        arc = ERROR_PROTECTION_VIOLATION;
                    } END_CATCH();

                } // while (arc == NO_ERROR)

                // _Pmpf((__FUNCTION__ ": !!! arc == %d !!!", arc));

                // we got an error:

                // close the directory handle
                DosCloseChangeNotify(hdir);
                hdir = NULLHANDLE;

                // in addition, check if we get error 111
                // (ERROR_BUFFER_OVERFLOW); this comes in frequently
                // when there's too many notifications flooding in,
                // e.g. when hundreds of files are unzipped into a
                // folder
                if (    (arc == ERROR_BUFFER_OVERFLOW)
                     && (G_hwndFindFolder)
                     && (!cmnQuerySetting(sfFdrAutoRefreshDisabled))
                   )
                {
                    // tell the "find folder" thread to refresh the
                    // entire folder then... we have no valid
                    // data now, so we'll use the previous notification
                    // then
                    WinPostMsg(G_hwndFindFolder,
                               NM_OVERFLOW,
                               0, 0);

                    DosSleep(1000);
                }

                // and start over

            } // end while (G_hwndFindFolder)

            DosFreeMem(pBuffer);

        } // end if ((arc == NO_ERROR) && (pCNInfo))
    }
    CATCH(excpt1)
    {
    } END_CATCH();

    if (hdir)
    {
        DosCloseChangeNotify(hdir);
    }
}


