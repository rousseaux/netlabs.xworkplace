
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
#include <secure.h>

#include <string.h>

#include "xwpsec32.sys\types.h"
#include "xwpsec32.sys\StackToFlat.h"
#include "xwpsec32.sys\devhlp32.h"

#include "security\ring0api.h"

#include "xwpsec32.sys\xwpsec_types.h"
#include "xwpsec32.sys\xwpsec_callbacks.h"

/*
 *@@ FINDFIRST:
 *      SES kernel hook for FINDFIRST.
 *      This gets called from the OS/2 kernel to give
 *      the ISS a chance to authorize this event.
 *
 *      This callback is stored in SecurityImports in
 *      sec32_callbacks.c to hook the kernel.
 */

ULONG FINDFIRST(PFINDPARMS pParms)
{
    /*          typedef struct {
                 PSZ    pszPath;      // well formed path
                 ULONG  ulHandle;     // search handle
                 ULONG  rc;           // rc user got from findfirst
                 PUSHORT pResultCnt;  // count of found files
                 USHORT usReqCnt;     // count user requested
                 USHORT usLevel;      // search level
                 USHORT usBufSize;    // user buffer size
                 USHORT fPosition;    // use position information?
                 PCHAR  pcBuffer;     // ptr to user buffer
                 ULONG  Position;     // Position for restarting search
                 PSZ    pszPosition;  // file to restart search with
             } FINDPARMS, *PFINDPARMS;
    */

    int rc = NO_ERROR;

    if (utilNeedsVerify())
    {
        // access control enabled, and not call from daemon itself:
        if (    (rc = utilSemRequest(&G_hmtxBufferLocked, -1))
                    == NO_ERROR)
        {
            // daemon buffers locked
            // (we have exclusive access):

            utilWriteLog("FINDFIRST for \"%s\"\r\n", pParms->pszPath);
            utilWriteLogInfo();

            strcpy( ((PSECIOSHARED)G_pSecIOShared)->EventData.FindFirst.szPath,
                    pParms->pszPath);
                      // sizeof(EventData.FindFirst.szPath));
            /* EventData.FindFirst.ulHandle = pParms->ulHandle;
            EventData.FindFirst.rc = pParms->rc;
            // EventData.FindFirst.usResultCnt = pParms->pResultCnt;
            EventData.FindFirst.usReqCnt = pParms->usReqCnt;
            EventData.FindFirst.usLevel = pParms->usLevel;
            EventData.FindFirst.usBufSize = pParms->usBufSize;
            EventData.FindFirst.fPosition = pParms->fPosition;
            EventData.FindFirst.ulPosition = pParms->Position; */

            // have this request authorized by daemon
            rc = utilDaemonRequest(SECEVENT_FINDFIRST);
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


