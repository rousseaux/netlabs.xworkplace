
/*
 *@@sourcefile callb_exec.c:
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
#include <secure.h>

#include <string.h>

#include "xwpsec32.sys\types.h"
#include "xwpsec32.sys\StackToFlat.h"
#include "xwpsec32.sys\devhlp32.h"

#include "security\ring0api.h"

#include "xwpsec32.sys\xwpsec_types.h"
#include "xwpsec32.sys\xwpsec_callbacks.h"

/*
 *@@ LOADEROPEN:
 *      SES kernel hook for LOADEROPEN.
 *      This gets called from the OS/2 kernel to give
 *      the ISS a chance to authorize this event.
 *
 *      This callback is stored in SecurityImports in
 *      sec32_callbacks.c to hook the kernel.
 */

ULONG LOADEROPEN(PSZ pszPath,
                 ULONG SFN)
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

            PXWPSECEVENTDATA_LOADEROPEN pLoaderOpen
                = &((PSECIOSHARED)G_pSecIOShared)->EventData.LoaderOpen;

            utilWriteLog("LOADEROPEN for \"%s\"\r\n", pszPath);
            utilWriteLogInfo();

            // prepare data for daemon notify
            strcpy( pLoaderOpen->szFileName,
                    pszPath);
                      // sizeof(EventData.LoaderOpen.szFileName));
            pLoaderOpen->SFN = SFN;

            // have this request authorized by daemon
            rc = utilDaemonRequest(SECEVENT_LOADEROPEN);
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
 *@@ GETMODULE:
 *      SES kernel hook for GETMODULE.
 *      This gets called from the OS/2 kernel to give
 *      the ISS a chance to authorize this event.
 *
 *      This callback is stored in SecurityImports in
 *      sec32_callbacks.c to hook the kernel.
 */

ULONG GETMODULE(PSZ pszPath)
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

            utilWriteLog("GETMODULE for \"%s\"\r\n", pszPath);
            utilWriteLogInfo();

            // prepare data for daemon notify
            strcpy( ((PSECIOSHARED)G_pSecIOShared)->EventData.FileOnly.szPath,
                    pszPath);
                      // sizeof(EventData.GetModule.szFileName));

            // have this request authorized by daemon
            rc = utilDaemonRequest(SECEVENT_GETMODULE);
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
 *@@ EXECPGM:
 *      SES kernel hook for EXECPGM.
 *      This gets called from the OS/2 kernel to give
 *      the ISS a chance to authorize this event.
 *
 *      This callback is stored in SecurityImports in
 *      sec32_callbacks.c to hook the kernel.
 */

ULONG EXECPGM(PSZ pszPath,
              PCHAR pchArgs)
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

            utilWriteLog("EXECPGM for \"%s\"\r\n", pszPath);
            utilWriteLogInfo();

            // prepare data for daemon notify
            strcpy( ((PSECIOSHARED)G_pSecIOShared)->EventData.ExecPgm.szFileName,
                    pszPath);
                      // sizeof(EventData.ExecPgm.szFileName));
            /* strcpy(__StackToFlat(&EventData.ExecPgm.szArgs),
                   pchArgs); */
                      // sizeof(EventData.ExecPgm.szArgs));

            // have this request authorized by daemon
            rc = utilDaemonRequest(SECEVENT_EXECPGM);
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
 *@@ EXECPGM_POST:
 *      SES kernel hook for EXECPGM_POST.
 *      This gets called from the OS/2 kernel to notify
 *      the ISS of this event.
 *
 *      This callback is stored in SecurityImports in
 *      sec32_callbacks.c to hook the kernel.
 */

VOID EXECPGM_POST(PSZ pszPath,
                  PCHAR pchArgs,
                  ULONG NewPID)
{
    int rc = NO_ERROR;

    if (G_hevCallback)
    {
        // access control enabled:
        // note that this time, we don't use utilNeedsVerify()
        // because we also need to track calls from the daemon itself...

        // AS A CONSEQUENCE, THE DAEMON RING-3 THREAD MUST NOT
        // START PROCESSES!!! THIS WOULD CAUSE A DEADLOCK!!!

        if (    (rc = utilSemRequest(&G_hmtxBufferLocked, -1))
                    == NO_ERROR)
        {
            // daemon buffers locked
            // (we have exclusive access):

            PXWPSECEVENTDATA_EXECPGM_POST pExecPgmPost
                = &((PSECIOSHARED)G_pSecIOShared)->EventData.ExecPgmPost;

            utilWriteLog("EXECPGM_POST for \"%s\", new PID: 0x%lX\r\n", pszPath, NewPID);
            utilWriteLogInfo();

            // prepare data for daemon notify
            strcpy( pExecPgmPost->szFileName,
                    pszPath);
            pExecPgmPost->ulNewPID = NewPID;

            // have this request authorized by daemon
            utilDaemonRequest(SECEVENT_EXECPGM_POST);
            // utilDaemonRequest blocks until the daemon has
            // processed this request... no return code here

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

}


