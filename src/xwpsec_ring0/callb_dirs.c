
/*
 *@@sourcefile callb_move.c:
 *      SES kernel hook code.
 */

/*
 *      Copyright (C) 2000 Ulrich M”ller.
 *      Based on the MWDD32.SYS example sources,
 *      Copyright (C) 1995, 1996, 1997  Matthieu Willm (willm@ibm.net).
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, in version 2 as it comes in the COPYING
 *      file of the XWorkplace main distribution.
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 */

#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_NOPMAPI
#include <os2.h>
// #include <secure.h>

#include <string.h>

#include "xwpsec32.sys\types.h"
#include "xwpsec32.sys\StackToFlat.h"
#include "xwpsec32.sys\devhlp32.h"

#include "security\ring0api.h"

#include "xwpsec32.sys\xwpsec_types.h"
#include "xwpsec32.sys\xwpsec_callbacks.h"

/*
 *@@ MAKEDIR:
 *      SES kernel hook for MAKEDIR.
 *      This gets called from the OS/2 kernel to give
 *      the ISS a chance to authorize this event.
 *
 *      This callback is stored in G_SecurityHooks in
 *      sec32_callbacks.c to hook the kernel.
 */

ULONG MAKEDIR(PSZ pszPath)
{
    int rc = NO_ERROR;

    if (utilNeedsVerify())
    {
        // access control enabled, and not call from daemon itself:
        if (    (rc = utilSemRequest(&G_hmtxBufferLocked, -1))
                    == NO_ERROR)
        {
            // daemon buffers locked
            // (we have exclusive access):

            utilWriteLog("MAKEDIR for \"%s\"\r\n", pszPath);
            utilWriteLogInfo();

            // prepare data for daemon notify
            strcpy( ((PSECIOSHARED)G_pSecIOShared)->EventData.FileOnly.szPath,
                    pszPath);
                      // sizeof(EventData.Directory.szPath));

            // have this request authorized by daemon
            rc = utilDaemonRequest(SECEVENT_MAKEDIR);
            // utilDaemonRequest properly serializes all requests
            // to the daemon;
            // utilDaemonRequest blocks until the daemon has either
            // authorized or turned down this request.

            // Return code is either an error in the driver
            // or NO_ERROR if daemon has authorized the request
            // or ERROR_ACCESS_DENIED or some other error code
            // if the daemon denied the request.

            // now release buffers mutex;
            // this unblocks other application threads
            // which are waiting on an access verification
            // in this function (after we return from ring-0,
            // I guess)
            utilSemRelease(&G_hmtxBufferLocked);
        }
    }

    if (    (rc != NO_ERROR)
         && (rc != ERROR_ACCESS_DENIED)
       )
    {
        // kernel panic
        // _sprintf("XWPSEC32.SYS: OPEN_PRE returned %d.", rc);
        // DevHlp32_InternalError(G_szScratchBuf, strlen(G_szScratchBuf) + 1);
        utilWriteLog("      ------ WARNING rc is %d\r\n", rc);
        rc = ERROR_ACCESS_DENIED;
    }

    return (rc);
}

/*
 *@@ CHANGEDIR:
 *      SES kernel hook for CHANGEDIR.
 *      This gets called from the OS/2 kernel to give
 *      the ISS a chance to authorize this event.
 *
 *      This callback is stored in G_SecurityHooks in
 *      sec32_callbacks.c to hook the kernel.
 */

ULONG CHANGEDIR(PSZ pszPath)
{
    int rc = NO_ERROR;

    if (utilNeedsVerify())
    {
        // access control enabled, and not call from daemon itself:
        if (    (rc = utilSemRequest(&G_hmtxBufferLocked, -1))
                    == NO_ERROR)
        {
            // daemon buffers locked
            // (we have exclusive access):

            utilWriteLog("CHANGEDIR for \"%s\"\r\n", pszPath);
            utilWriteLogInfo();

            // prepare data for daemon notify
            strcpy( ((PSECIOSHARED)G_pSecIOShared)->EventData.FileOnly.szPath,
                    pszPath);
                      // sizeof(EventData.Directory.szPath));

            // have this request authorized by daemon
            rc = utilDaemonRequest(SECEVENT_CHANGEDIR);
            // utilDaemonRequest properly serializes all requests
            // to the daemon;
            // utilDaemonRequest blocks until the daemon has either
            // authorized or turned down this request.

            // Return code is either an error in the driver
            // or NO_ERROR if daemon has authorized the request
            // or ERROR_ACCESS_DENIED or some other error code
            // if the daemon denied the request.

            // now release buffers mutex;
            // this unblocks other application threads
            // which are waiting on an access verification
            // in this function (after we return from ring-0,
            // I guess)
            utilSemRelease(&G_hmtxBufferLocked);
        }
    }

    if (    (rc != NO_ERROR)
         && (rc != ERROR_ACCESS_DENIED)
       )
    {
        // kernel panic
        // _sprintf("XWPSEC32.SYS: OPEN_PRE returned %d.", rc);
        // DevHlp32_InternalError(G_szScratchBuf, strlen(G_szScratchBuf) + 1);
        utilWriteLog("      ------ WARNING rc is %d\r\n", rc);
        rc = ERROR_ACCESS_DENIED;
    }

    return (rc);
}

/*
 *@@ REMOVEDIR:
 *      SES kernel hook for REMOVEDIR.
 *      This gets called from the OS/2 kernel to give
 *      the ISS a chance to authorize this event.
 *
 *      Um, we have a problem here. There is no POST call
 *      for REMOVEDIR so we cannot find out whether REMOVEDIR
 *      failed and if not, remove the ACLDB entry.
 *
 *      This callback is stored in G_SecurityHooks in
 *      sec32_callbacks.c to hook the kernel.
 */

ULONG REMOVEDIR(PSZ pszPath)
{
    int rc = NO_ERROR;

    if (utilNeedsVerify())
    {
        // access control enabled, and not call from daemon itself:
        if (    (rc = utilSemRequest(&G_hmtxBufferLocked, -1))
                    == NO_ERROR)
        {
            // daemon buffers locked
            // (we have exclusive access):

            utilWriteLog("REMOVEDIR for \"%s\"\r\n", pszPath);
            utilWriteLogInfo();

            // prepare data for daemon notify
            strcpy( ((PSECIOSHARED)G_pSecIOShared)->EventData.FileOnly.szPath,
                    pszPath);
                      // sizeof(EventData.Directory.szPath));

            // have this request authorized by daemon
            rc = utilDaemonRequest(SECEVENT_REMOVEDIR);
            // utilDaemonRequest properly serializes all requests
            // to the daemon;
            // utilDaemonRequest blocks until the daemon has either
            // authorized or turned down this request.

            // Return code is either an error in the driver
            // or NO_ERROR if daemon has authorized the request
            // or ERROR_ACCESS_DENIED or some other error code
            // if the daemon denied the request.

            // now release buffers mutex;
            // this unblocks other application threads
            // which are waiting on an access verification
            // in this function (after we return from ring-0,
            // I guess)
            utilSemRelease(&G_hmtxBufferLocked);
        }
    }

    if (    (rc != NO_ERROR)
         && (rc != ERROR_ACCESS_DENIED)
       )
    {
        // kernel panic
        // _sprintf("XWPSEC32.SYS: OPEN_PRE returned %d.", rc);
        // DevHlp32_InternalError(G_szScratchBuf, strlen(G_szScratchBuf) + 1);
        utilWriteLog("      ------ WARNING rc is %d\r\n", rc);
        rc = ERROR_ACCESS_DENIED;
    }

    return (rc);
}


