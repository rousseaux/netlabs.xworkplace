
/*
 *@@sourcefile refresh.c:
 *      new code for folder auto-refresh support, if the
 *      XWP replacement is enabled.
 *
 *      If the user has replaced folder auto-refresh, on
 *      WPS startup, krnReplaceWheelWatcher stops the WPS
 *      "wheel watcher" thread, which usually processes
 *      the Dos*ChangeNotify notifications.
 *
 *      Since these APIs can only be used once on the
 *      entire system, we can't compete with that thread.
 *      This is also why enabling the replacement requires
 *      a WPS restart, because we can only stop that thread
 *      if it has not done any processing yet.
 *
 *      Instead, the following three (!) threads are responsible
 *      for processing folder auto-refresh:
 *
 *      1)  The "Sentinel" thread (refr_fntSentinel). This is
 *          our replacement for the WPS wheel watcher.
 *
 *          This gets started from krnReplaceWheelWatcher.
 *          It does not have a PM message queue and now
 *          handles the Dos*ChangeNotify APIs.
 *
 *      2)  The "Notify worker" thread gets started by the
 *          sentinel thread and processes the notification
 *          in more detail. Since the sentinel must get the
 *          notifications processed as quickly as possible
 *          (because these come directly from the kernel),
 *          it does not check them but posts them to the
 *          notify worker in an XWPNOTIFY struct.
 *
 *          For each XWPNOTIFY, the find-notify worker checks
 *          if the notification is for a folder that is already
 *          awake. If so, it adds the notification to the
 *          folder.
 *
 *      3)  The "Pump thread" is then responsible for actually
 *          refreshing the folders after a reasonable delay.
 *          This thread sorts out notifications that cancel
 *          each other out or need not be processed at all.
 *          This is what the WPS "Ager" thread normally does.
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
#include "helpers\threads.h"            // thread helpers

// SOM headers which don't crash with prec. header files
#include "xfldr.ih"

// XWorkplace implementation headers
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

#include "filesys\folder.h"             // XFolder implementation
#include "filesys\refresh.h"            // folder auto-refresh

// other SOM headers
#pragma hdrstop                 // VAC++ keeps crashing otherwise

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

THREADINFO      G_tiNotifyWorker = {0};
HWND            G_hwndNotifyWorker = NULLHANDLE;
HEV             G_hevNotifyWorkerReady = NULLHANDLE;

THREADINFO      G_tiPumpThread = {0};
HEV             G_hwndNotificationPump = NULLHANDLE;

LINKLIST        G_llAllNotifications;
ULONG           G_ulMSLastOverflow = 0;

BOOL            G_fExitAllRefreshThreads = FALSE;

#define PUMP_AGER_MILLISECONDS      4000

/* ******************************************************************
 *
 *   Notifications list API
 *
 ********************************************************************/

/*
 *@@ refrAddNotification:
 *
 *      Preconditions:
 *
 *      -- The caller must have the global WPS notify
 *         mutex.
 *
 *@@added V0.9.9 (2001-02-04) [umoeller]
 */

VOID refrAddNotification(PXWPNOTIFY pNotify)
{
    XFolderData *somThat = XFolderGetData(pNotify->pFolder);
    PLINKLIST pllFolder = (PLINKLIST)somThat->pvllNotifications;

    if (!pllFolder)
    {
        // folder doesn't have a list yet:
        pllFolder = lstCreate(FALSE);       // no auto-free;
                                            // auto-free is for the
                                            // global list instead
        somThat->pvllNotifications = pllFolder;
    }

    if (pllFolder)
    {
        // append to the folder's list (no auto-free)
        lstAppendItem(pllFolder, pNotify);
        // append to the global list (auto-free)
        lstAppendItem(&G_llAllNotifications, pNotify);
    }
}

/*
 *@@ refrRemoveNotification:
 *
 *      Preconditions:
 *
 *      -- The caller must have the global WPS notify
 *         mutex.
 *
 *@@added V0.9.9 (2001-02-01) [umoeller]
 */

VOID refrRemoveNotification(PLINKLIST pllFolder,
                            PXWPNOTIFY pNotify,
                            PLISTNODE pGlobalNode)
{
    // remove from folder list
    lstRemoveItem(pllFolder,
                  pNotify);   // no auto-free, search node

    if (!pGlobalNode)
        // not specified: search it then
        pGlobalNode = lstNodeFromItem(&G_llAllNotifications,
                                      pNotify);
    if (pGlobalNode)
    {
        // remove from global list
        lstRemoveNode(&G_llAllNotifications,
                      pGlobalNode);        // auto-free
    }
}

/*
 *@@ refrClearFolderNotifications:
 *      removes all pending notifications for the
 *      specified folder without processing them.
 *
 *      This does NOT free the list in the instance
 *      data itself.
 *
 *      Gets called from XFolder::wpUnInitData.
 *
 *      Preconditions:
 *
 *      -- The caller must have the global WPS notify
 *         mutex.
 *
 *@@added V0.9.9 (2001-02-01) [umoeller]
 */

VOID refrClearFolderNotifications(WPFolder *pFolder)
{
    XFolderData *somThis = XFolderGetData(pFolder);
    if (_pvllNotifications)
    {
        PLINKLIST pllFolder = (PLINKLIST)_pvllNotifications;
        PLISTNODE pNode = lstQueryFirstNode(pllFolder);
        while (pNode)
        {
            PXWPNOTIFY pNotify = (PXWPNOTIFY)pNode->pItemData;
            // get next node NOW because we're deleting
            PLISTNODE pNode2 = pNode;
            pNode = pNode->pNext;

            // remove in folder (no auto-free)
            lstRemoveNode(pllFolder, pNode2);
            // remove in global list (auto-free XWPNOTIFY)
            lstRemoveItem(&G_llAllNotifications, pNotify);
        }
    }
}

/* ******************************************************************
 *
 *   Pump thread
 *
 ********************************************************************/

/*
 *@@ PumpAgedNotification:
 *
 *@@added V0.9.9 (2001-02-04) [umoeller]
 */

VOID PumpAgedNotification(PXWPNOTIFY pNotify,
                          PLISTNODE pGlobalNode)
{
    BOOL    fRemoveThis = TRUE;

    // Note that we hack the folder's instance
    // data directly. Yes, this isn't exactly what
    // you would call encapsulation, but we need
    // speed here.
    XFolderData *somThat = XFolderGetData(pNotify->pFolder);
    PLINKLIST pllFolder = (PLINKLIST)somThat->pvllNotifications;
    if (pllFolder)
    {
        // _Pmpf((__FUNCTION__ ": processing notification %s", pNotify->pShortName));

        switch (pNotify->CNInfo.bAction)
        {
            case RCNF_FILE_ADDED:
            case RCNF_DIR_ADDED:
            {
                _wpclsQueryObjectFromPath(_WPFileSystem,
                                          pNotify->CNInfo.szName);
                        // this will wake the object up
                        // if it hasn't been awakened yet
            break; }

            case RCNF_FILE_DELETED:
            case RCNF_DIR_DELETED:
            {
                // loop thru the folder contents (without refreshing)
                // to check if we have a WPFileSystem with this name
                // already
                WPFileSystem *pobj = fdrFindFSFromName(pNotify->pFolder,
                                                       pNotify->pShortName);
                if (pobj)
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

                            // lock the folder first
                            WPFolder *pFolder = pNotify->pFolder;
                            WPSHLOCKSTRUCT Lock;
                            if (wpshLockObject(&Lock, pFolder))
                            {
                                ULONG flFolder = _wpQueryFldrFlags(pFolder);
                                _wpModifyFldrFlags(pFolder,
                                                   FOI_POPULATEDWITHALL | FOI_POPULATEDWITHFOLDERS,
                                                   0);
                                _wpFree(pobj);
                                _wpSetFldrFlags(pFolder, flFolder);
                            }
                            wpshUnlockObject(&Lock);
                        break; }

                        // case NO_ERROR: the file has reappeared.
                        // DO NOT FREE IT.
                        // ### refresh the FSobject instead
                    }
                }
                // else object not in folder: no problem there
            break; }

            case RCNF_MOVED_IN:
            case RCNF_MOVED_OUT:

            case RCNF_CHANGED:
            case RCNF_OLDNAME:
            case RCNF_NEWNAME:

            break;

            case RCNF_DEVICE_ATTACHED:
            case RCNF_DEVICE_DETACHED:

            break;

            case RCNF_XWP_FULLREFRESH:
            {
                _wpRefresh(pNotify->pFolder, NULLHANDLE, NULL);

                refrClearFolderNotifications(pNotify->pFolder);
                fRemoveThis = FALSE;
            break; }
        }
    }

    if (fRemoveThis)
        refrRemoveNotification(pllFolder,
                               pNotify,
                               pGlobalNode);
}

/*
 *@@ PumpNotifications:
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
 */

BOOL PumpNotifications(VOID)
{
    BOOL        fNotificationsLeft = FALSE;
    ULONG       ulrc = 100*1000;
    PLISTNODE   pGlobalNodeThis;

    // go thru ALL notifications on the list
    // and check if they have aged enough to
    // be processed
    pGlobalNodeThis = lstQueryFirstNode(&G_llAllNotifications);

    if (pGlobalNodeThis)
    {
        // get current milliseconds
        ULONG       ulMSNow = 0;
        DosQuerySysInfo(QSV_MS_COUNT,
                        QSV_MS_COUNT,
                        &ulMSNow,
                        sizeof(ulMSNow));

        // _Pmpf((__FUNCTION__ ": %d notifications to be pumped.", lstCountItems(&G_llAllNotifications)));

        while (pGlobalNodeThis)
        {
            PXWPNOTIFY  pNotify = (PXWPNOTIFY)pGlobalNodeThis->pItemData;

            // advance to next node before processing...
            // the node might get deleted
            PLISTNODE   pGlobalNodeSaved = pGlobalNodeThis;
            pGlobalNodeThis = pGlobalNodeThis->pNext;

            // pNotify->ulMS holds the milliseconds offset
            // at the time the notification was added.
            // If that offset plus PUMP_AGER_MILLISECONDS
            // is smaller than ulMSNow, the notification
            // has aged enough.
            if (pNotify->ulMS + PUMP_AGER_MILLISECONDS <= ulMSNow)
            {
                // aged enough:

                // check if we had an overflow AFTER this notification
                if (    (G_ulMSLastOverflow)
                     && (G_ulMSLastOverflow > pNotify->ulMS)
                   )
                {
                    // yes:
                    PXWPNOTIFY pNew;

                    // and add a new notification for "full refresh" later
                    pNew = (PXWPNOTIFY)malloc(sizeof(XWPNOTIFY));
                    memset(pNew, 0, sizeof(XWPNOTIFY));
                    pNew->pFolder = pNotify->pFolder;
                    pNew->CNInfo.bAction = RCNF_XWP_FULLREFRESH;

                    DosQuerySysInfo(QSV_MS_COUNT,
                                    QSV_MS_COUNT,
                                    &pNew->ulMS,
                                    sizeof(pNew->ulMS));

                    // clear all notifications for this folder
                    refrClearFolderNotifications(pNotify->pFolder);

                    refrAddNotification(pNew);
                }
                else
                    // no overflow: just process
                    PumpAgedNotification(pNotify,
                                         pGlobalNodeSaved);

            } // end if (pNotify->ulMS + PUMP_AGER_MILLISECONDS < ulMSNow)
            else
            {
                // not yet aged enough:
                ULONG ulMStoGoThis = pNotify->ulMS + PUMP_AGER_MILLISECONDS - ulMSNow;
                if (ulMStoGoThis < ulrc)
                    ulrc = ulMStoGoThis;
                fNotificationsLeft = TRUE;
            }
        }
    }

    if (!fNotificationsLeft)
        ulrc = 0;

    return (ulrc);
}

/*
 *@@ refr_fntPumpThread:
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
 *      but has no object window.
 *
 *@@added V0.9.9 (2001-02-01) [umoeller]
 */

VOID _Optlink refr_fntPumpThread(PTHREADINFO ptiMyself)
{
    QMSG qmsg;
    ULONG   ulWaitTime = 1000;

    WinCancelShutdown(ptiMyself->hmq, TRUE);

    while (!G_fExitAllRefreshThreads)
    {
        BOOL    fSemOwned = FALSE;
        ULONG   ulPostCount = 0;

        // _Pmpf((__FUNCTION__ ": pump thread blocking on HEV."));

        WinWaitEventSem(G_hwndNotificationPump, ulWaitTime);

        // _Pmpf((__FUNCTION__ ": pump event posted."));

        TRY_LOUD(excpt1)
        {
            fSemOwned = wpshGetNotifySem(5000);
            if (fSemOwned)
            {
                // only if we got the mutex, reset the event
                DosResetEventSem(G_hwndNotificationPump, &ulPostCount);
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
            WinPostMsg(G_hwndNotifyWorker, WM_QUIT, 0, 0);
        } END_CATCH();

        if (fSemOwned)
        {
            wpshReleaseNotifySem();
            fSemOwned = FALSE;
        }
    }
}

/* ******************************************************************
 *
 *   Notify worker thread
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
 */

BOOL AddNotifyIfNotRedundant(PXWPNOTIFY pNotify)
{
    BOOL    fAddThis = FALSE;

    if (pNotify)
    {
        // hack the folder's instance data directly...
        // note that we do NOT request the folder's
        // object mutex here. Per our definition, the
        // folder's notify list may only be accessed
        // under the protection of the global WPS
        // notify mutex, so we're safe here. If we
        // additionally requested the folder object
        // mutex, we are running into the danger of
        // global deadlocks.
        XFolderData *somThat = XFolderGetData(pNotify->pFolder);

        BYTE        bActionThis = pNotify->CNInfo.bAction;

        // let's say: add this one now
        fAddThis = TRUE;

        // does the folder have a notification list at
        // all yet?
        if (somThat->pvllNotifications)
        {
            /*
             *  (1) drop redundant processing:
             *
             */

            BYTE    bOpposite = 0;

            // for FILE_DELETED, drop if we have a FILE_ADDED (temp file)
            // for DIR_DELETED, drop if we have a DIR_ADDED
            if (bActionThis == RCNF_FILE_DELETED)
                bOpposite = RCNF_FILE_ADDED;
            else if (bActionThis == RCNF_DIR_DELETED)
                bOpposite = RCNF_DIR_ADDED;

            if (bOpposite)
            {
                // yes:
                PLINKLIST pllFolder = (PLINKLIST)somThat->pvllNotifications;
                // search the folder's list for whether we already
                // have an opposite notification... in that
                // case we can safely drop this

                PLISTNODE pFolderNode = lstQueryFirstNode(pllFolder);
                while (pFolderNode)
                {
                    PXWPNOTIFY  pNotifyThat = (PXWPNOTIFY)pFolderNode->pItemData;
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
                            refrRemoveNotification(pllFolder,
                                                   pNotifyThat,
                                                   NULL);       // find node

                            // _Pmpf((__FUNCTION__ ": dropping %s", pNotify->pShortName));
                           break;
                        }
                    }

                    pFolderNode = pFolderNode->pNext;
                } // while (pFolderNode)
            } // end if (somThat->pvllNotifications)
        } // end if (bOpposite)

        /*
         *  (2) append the notification:
         *
         */

        if (fAddThis)
        {
            refrAddNotification(pNotify);
        }
    } // end if (pNotify)

    return (fAddThis);
}

/*
 *@@ FindFolderForNotification:
 *      this does the hard work of finding a folder
 *      etc. for every notification.
 *
 *@@added V0.9.9 (2001-01-31) [umoeller]
 */

VOID FindFolderForNotification(PXWPNOTIFY pNotify)
{
    BOOL fStored = FALSE;

    // get current milliseconds for this notification
    ULONG   ulMSNow = 0;
    DosQuerySysInfo(QSV_MS_COUNT,
                    QSV_MS_COUNT,
                    &ulMSNow,
                    sizeof(ulMSNow));

    switch (pNotify->CNInfo.bAction)
    {
        case RCNF_FILE_ADDED:
        case RCNF_FILE_DELETED:
        case RCNF_DIR_ADDED:
        case RCNF_DIR_DELETED:
        case RCNF_MOVED_IN:
        case RCNF_MOVED_OUT:
        case RCNF_CHANGED:
        case RCNF_OLDNAME:
        case RCNF_NEWNAME:
        {
            // for all these, find the folder first
            PSZ pLastBackslash = strrchr(pNotify->CNInfo.szName, '\\');

            _Pmpf((__FUNCTION__ ": got bAction %d for %s", pNotify->CNInfo.bAction, pNotify->CNInfo.szName));

            if (pLastBackslash)
            {
                // check if the folder object is already awake
                BOOL    fSemOwned = FALSE;

                // store ptr to short name
                pNotify->pShortName = pLastBackslash + 1;

                // terminate path name so we get the folder path
                *pLastBackslash = '\0';

                // request the WPS notify semaphore
                TRY_LOUD(excpt2)
                {
                    // ignore all notifications for folders which
                    // aren't even awake... so first ask the WPS
                    // if it has awakened that folder already
                    pNotify->pFolder = _wpclsQueryAwakeObject(_WPFileSystem,
                                                              pNotify->CNInfo.szName);
                                                               // up to backslash now
                    if (    (pNotify->pFolder)
                         && (_somIsA(pNotify->pFolder, _WPFolder))
                       )
                    {
                        // OK, folder is awake:

                        BOOL    fRefreshFolderOnOpen = FALSE;

                        // request the global WPS notify sem;
                        // we'll wait ten seconds for this -- if we can't
                        // get the mutex in that time, we'll just drop
                        // the notification
                        fSemOwned = wpshGetNotifySem(10 * 1000);
                        if (fSemOwned)
                        {
                            ULONG flFolder = _wpQueryFldrFlags(pNotify->pFolder);

                            _Pmpf(("    --> folder %s, flags: 0x%lX",
                                        _wpQueryTitle(pNotify->pFolder),
                                        flFolder));
            /*
        #define FOI_POPULATEDWITHALL      0x0001
        #define FOI_POPULATEDWITHFOLDERS  0x0002
        #define FOI_WORKAREA              0x0004
        #define FOI_CHANGEFONT            0x0008        // anti-recursion flag
        #define FOI_WAMINIMIZED           0x0020
        #define FOI_WASTARTONRESTORE      0x0040
        #define FOI_NOREFRESHVIEWS        0x0080
        #define FOI_ASYNCREFRESHONOPEN    0x0100
        #define FOI_TREEPOPULATED         0x0200
        #define FOI_POPULATEINPROGRESS    0x0400
        #define FOI_REFRESHINPROGRESS     0x0800
        #define FOI_FIRSTPOPULATE         0x1000  // folder has no iconposdata

        #define FOI_WAMCRINPROGRESS       0x2000  // Minimize, close, restore in progress
        #define FOI_CNRBKGNDOLDFORMAT     0x4000  // CnrBkgnd saved in old format
        #define FOI_CHANGEICONBGNDCOLOR   0x8000  // Icon Text Background Color changing
        #define FOI_CHANGEICONTEXTCOLOR   0x00010000 // Icon Text Color changing
        #define FOI_DELETEINPROGRESS      0x00020000 // To prevent wpFree from repopulating
            */
                            // ignore if delete or populate or refresh is in progress
                            if (0 == (flFolder & (  FOI_DELETEINPROGRESS
                                                  | FOI_POPULATEINPROGRESS
                                                  | FOI_REFRESHINPROGRESS)
                               ))
                            {
                                // see if it's populated; we can safely
                                // drop the notification if the folder
                                // isn't even fully populated because
                                // then the WPS will collect all the file-system
                                // information on populate anyway...

                                // keep the notification?
                                if (    (flFolder & FOI_POPULATEDWITHALL)
                                     || (    (flFolder & FOI_POPULATEDWITHFOLDERS)
                                          && (    (pNotify->CNInfo.bAction == RCNF_DIR_ADDED)
                                               || (pNotify->CNInfo.bAction == RCNF_DIR_DELETED)
                                             )
                                        )
                                   )
                                {
                                    // OK, this is worth storing:

                                    // fill the fields that are missing
                                    pNotify->ulMS = ulMSNow;

                                    // restore the path name that we truncated
                                    // above
                                    *pLastBackslash = '\\';

                                    fStored = AddNotifyIfNotRedundant(pNotify);
                                            // if this returns FALSE, the
                                            // notification is freed below.
                                    _Pmpf(("    AddNotifyIfNotRedundant returned %d.", fStored));


                                }
                                else
                                    // folder isn't even populated:
                                    // mark the folder for refresh on next open
                                    fRefreshFolderOnOpen = TRUE;

                            } // end if delete or refresh in progress
                        } // end if (fSemOwned)
                        else
                            // can't get the semaphore in time:
                            // mark the folder for refresh
                            fRefreshFolderOnOpen = TRUE;

                        if (fRefreshFolderOnOpen)
                            _wpModifyFldrFlags(pNotify->pFolder,
                                               FOI_ASYNCREFRESHONOPEN,
                                               FOI_ASYNCREFRESHONOPEN);
                    }
                    // else: folder isn't even awake... then it can't be visible
                    // in a tree view either, so don't bother
                }
                CATCH(excpt2)
                {
                    // if we crashed, stop all refresh threads. This
                    // can become very annoying otherwise.
                    G_fExitAllRefreshThreads = TRUE;
                    WinPostMsg(G_hwndNotifyWorker, WM_QUIT, 0, 0);
                } END_CATCH();

                if (fSemOwned)
                    wpshReleaseNotifySem();
            } // if (pLastBackslash)
        break; }

        case RCNF_DEVICE_ATTACHED:
        case RCNF_DEVICE_DETACHED:
        break;
    }

    if (fStored)
        // we had a notification:
        // post the event sem for the pump thread
        DosPostEventSem(G_hwndNotificationPump);
    else
        // we have not handled the notification:
        free(pNotify);
}

/*
 *@@ fnwpNotifyWorker:
 *      object window proc for the "notify worker" thread.
 *
 *@@added V0.9.9 (2001-01-29) [umoeller]
 */

MRESULT EXPENTRY fnwpNotifyWorker(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;

    switch (msg)
    {
        /*
         *@@ NM_NOTIFICATION:
         *      this has an XWPNOTIFY in mp1.
         */

        case NM_NOTIFICATION:
            if (mp1)
            {
                TRY_LOUD(excpt1)
                {
                    FindFolderForNotification((PXWPNOTIFY)mp1);
                }
                CATCH(excpt1) {} END_CATCH();
            }
        break;

        /*
         *@@ NM_OVERFLOW:
         *      posted by the sentinel when it gets a
         *      buffer overflow. We have no data in this
         *      case, so we'll just set a global variable
         *      to the current time... this will cause
         *      a complete refresh in the pump thread.
         */

        case NM_OVERFLOW:
            // get current milliseconds
            DosQuerySysInfo(QSV_MS_COUNT,
                            QSV_MS_COUNT,
                            &G_ulMSLastOverflow,
                            sizeof(G_ulMSLastOverflow));
        break;

        default:
            mrc = WinDefWindowProc(hwnd, msg, mp1, mp2);
    }

    return (mrc);
}

/*
 *@@ refr_fntNotifyWorker:
 *      second thread for folder auto-refresh. This
 *      has a PM message queue and an object window
 *      (fnwpNotifyWorker) which does the hard work
 *      for folder auto-refresh.
 *
 *      This receives an NM_NOTIFICATION message
 *      from the Sentinel thread (refr_fntSentinel)
 *      for each notification that comes in.
 *
 *@@added V0.9.9 (2001-01-29) [umoeller]
 */

VOID _Optlink refr_fntNotifyWorker(PTHREADINFO ptiMyself)
{
    QMSG qmsg;

    WinCancelShutdown(ptiMyself->hmq, TRUE);

    G_hwndNotifyWorker = WinCreateWindow(HWND_OBJECT,
                                         WC_STATIC,
                                         "XWPNotifyWorker",
                                         0,
                                         0, 0, 0, 0,
                                         NULLHANDLE,
                                         HWND_BOTTOM,
                                         0,
                                         NULL,
                                         NULL);
    WinSubclassWindow(G_hwndNotifyWorker,
                      fnwpNotifyWorker);

    // tell the sentinel that we're ready
    DosPostEventSem(G_hevNotifyWorkerReady);

    // now enter the message loop
    while (WinGetMsg(ptiMyself->hab, &qmsg, NULLHANDLE, 0, 0))
        WinDispatchMsg(ptiMyself->hab, &qmsg);
                    // loop until WM_QUIT

    WinDestroyWindow(G_hwndNotifyWorker);
    G_hwndNotifyWorker = NULLHANDLE;
}

/* ******************************************************************
 *
 *   Sentinel thread
 *
 ********************************************************************/

/*
 *@@ refr_fntSentinel:
 *      "sentinel" thread function.
 *
 *      This replaces the WPS's "WheelWatcher" thread
 *      and processes the DosFindNotify* functions.
 *
 *      On startup, this creates the "notify worker"
 *      and "pump" threads in turn.
 *
 *      This thread does NOT have a PM message queue.
 *
 *@@added V0.9.9 (2001-01-31) [umoeller]
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
    DosCreateEventSem(NULL, &G_hevNotifyWorkerReady, 0, FALSE);
    thrCreate(&G_tiNotifyWorker,
              refr_fntNotifyWorker,
              NULL,
              THRF_PMMSGQUEUE,
              0);
    // wait for the object window to be created
    DosWaitEventSem(G_hevNotifyWorkerReady, 20*1000);
    DosResetEventSem(G_hevNotifyWorkerReady, &ulPostCount);
    DosCloseEventSem(G_hevNotifyWorkerReady);

    // and create the "Pump" thread for aging and
    // inserting the notifications into the folder
    DosCreateEventSem(NULL, &G_hwndNotificationPump, 0, FALSE);
    thrCreate(&G_tiPumpThread,
              refr_fntPumpThread,
              NULL,
              THRF_PMMSGQUEUE,
              0);

    TRY_LOUD(excpt1)
    {
        // now loop forever... at least while the other threads
        // haven't stopped. If the notify worker terminates for
        // whatever reason, it resets G_hwndNotifyWorker to NULLHANDLE.
        while (G_hwndNotifyWorker)
        {
            // allocate a block of tiled memory...
            // apparently this is a 16-bit API and
            // needs this.
            ULONG       cb = sizeof(CNINFO) + (2 * CCHMAXPATH);
                                    // we can't use more, this will hang the system
            PCNINFO     pCNInfo = NULL;
            APIRET      arc = DosAllocMem((PVOID*)&pCNInfo,
                                          cb,
                                          PAG_COMMIT
                                            | OBJ_TILE
                                            | PAG_READ
                                            | PAG_WRITE);

            if ((arc == NO_ERROR) && (pCNInfo))
            {
                arc = DosOpenChangeNotify(NULL,
                                          0,
                                          &hdir,
                                          0);

                while (    (G_hwndNotifyWorker)
                        && (arc == NO_ERROR)
                      )
                {
                    TRY_QUIET(excpt2)
                    {
                        ULONG cLogs = 0;
                        arc = DosResetChangeNotify(pCNInfo,
                                                   cb,
                                                   &cLogs,
                                                   hdir);
                                // from my checking, cLogs is ALWAYS 0 here

                        if (    (G_hwndNotifyWorker)
                             && (arc == NO_ERROR)
                           )
                        {
                            // we got a notification:
                            // we won't bother with the details
                            // since we better get out of the processing
                            // as quickly as possible.
                            // Instead, we create a copy of this notification
                            // and have the "notify worker" thread do the
                            // work of finding the folder etc.

                            if (pCNInfo->cbName)
                            {
                                // create an XWPNOTIFY struct which has
                                // a CNINFO in it... the find-notify worker
                                // will set up the other fields then
                                ULONG   cbThis =   sizeof(XWPNOTIFY)
                                                 + pCNInfo->cbName;
                                PXWPNOTIFY pInfo2 = (PXWPNOTIFY)malloc(cbThis);
                                if (pInfo2)
                                {
                                    // copy string and make it zero-terminated
                                    memcpy(&pInfo2->CNInfo,
                                           pCNInfo,
                                           sizeof(CNINFO) + pCNInfo->cbName - 1);
                                    *(pInfo2->CNInfo.szName + pCNInfo->cbName) = '\0';

                                    // notify worker
                                    if (!WinPostMsg(G_hwndNotifyWorker,
                                                    NM_NOTIFICATION,
                                                    (MPARAM)pInfo2,
                                                    0))
                                    {
                                        // error posting:
                                        // either notify-worker has crashed,
                                        // or queue is full... in any case,
                                        // we discard the notification then
                                        free(pInfo2);
                                        _Pmpf((__FUNCTION__ ": !!! queue full !!!"));
                                    }
                                }
                            }
                        }
                        // else: DosResetChangeNotify returned an error...
                        // close the dir handle below
                    }
                    CATCH(excpt2)
                    {
                        // in case of an exception, set an error code
                        // to get out of the while() loop
                        arc = 1;
                    } END_CATCH();

                } // while (arc == NO_ERROR)

                _Pmpf((__FUNCTION__ ": !!! arc == %d !!!", arc));

                // we got an error:

                // in addition, check if we get error 111
                // (ERROR_BUFFER_OVERFLOW); this comes in frequently
                // when there's too many notifications flooding in,
                // e.g. when hundreds of files are unzipped into a
                // folder
                if (arc == ERROR_BUFFER_OVERFLOW)
                    // tell the notify worker to refresh the
                    // entire folder then... we have no valid
                    // data now, so we'll use the previous notification
                    // then
                    WinPostMsg(G_hwndNotifyWorker,
                               NM_OVERFLOW,
                               0, 0);

                // close the directory handle
                DosCloseChangeNotify(hdir);
                hdir = NULLHANDLE;

                DosFreeMem(pCNInfo);
            } // if (arc == NO_ERROR && pCNInfo)

            // and start over
        } // end while (G_hwndNotifyWorker)

    }
    CATCH(excpt1)
    {
    } END_CATCH();

    if (hdir)
    {
        DosCloseChangeNotify(hdir);
    }
}


