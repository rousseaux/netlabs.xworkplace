
/*
 *@@sourcefile callb_open.c:
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

#include <string.h>

#include "xwpsec32.sys\types.h"
#include "xwpsec32.sys\StackToFlat.h"
#include "xwpsec32.sys\devhlp32.h"

#include "security\ring0api.h"

#include "xwpsec32.sys\xwpsec_callbacks.h"

/* ******************************************************************
 *
 *   Callouts
 *
 ********************************************************************/

/*
 *@@ OPEN_PRE:
 *      SES kernel hook for OPEN_PRE.
 *
 *      As with all our hooks, this is stored in G_SecurityHooks
 *      (sec32_callbacks.c) force the OS/2 kernel to call us for
 *      each such event.
 *
 *      This is a "pre" event. Required privileges:
 *
 *      --  If (fsOpenFlags & OPEN_ACTION_CREATE_IF_NEW):
 *          XWPACCESS_CREATE
 *
 *      --  If (fsOpenFlags & OPEN_ACTION_OPEN_IF_EXISTS):
 *          XWPACCESS_READ
 *
 *      --  If (fsOpenFlags & OPEN_ACTION_REPLACE_IF_EXISTS):
 *          XWPACCESS_CREATE
 *
 *      --  If (fsOpenMode & OPEN_FLAGS_DASD) (open drive):
 *          XWPACCESS_WRITE and XWPACCESS_DELETE and XWPACCESS_CREATE
 *          for the entire drive (root directory).
 *
 *      --  If (fsOpenMode & OPEN_ACCESS_READONLY):
 *          XWPACCESS_READ
 *
 *      --  If (fsOpenMode & OPEN_ACCESS_WRITEONLY):
 *          XWPACCESS_WRITE
 *
 *      --  If (fsOpenMode & OPEN_ACCESS_READWRITE):
 *          XWPACCESS_READ and XWPACCESS_WRITE
 */

ULONG CallType OPEN_PRE(PSZ pszPath,        // in: full path of file
                        ULONG fsOpenFlags,  // in: open flags
                        ULONG fsOpenMode,   // in: open mode
                        ULONG SFN)          // in: system file number
{
    APIRET  rc = NO_ERROR;

    if (    (G_pidShell)
         && (!DevHlp32_GetInfoSegs(&G_pGDT,
                                   &G_pLDT))
       )
    {
        // authorize event if it is not from XWPShell
        if (G_pidShell != G_pLDT->LIS_CurProcID)
        {
        }

        if (G_bLog == LOG_ACTIVE)
        {
            PEVENTBUF_OPEN pBuf;
            ULONG   ulPathLen = strlen(pszPath);

            if (pBuf = ctxtLogEvent(EVENT_OPEN_PRE,
                                    sizeof(EVENTBUF_OPEN) + ulPathLen))
            {
                pBuf->fsOpenFlags = fsOpenFlags;
                pBuf->fsOpenMode = fsOpenMode;
                pBuf->SFN = SFN;
                pBuf->Action = 0;           // not used with OPEN_PRE
                pBuf->rc = rc;
                pBuf->ulPathLen = ulPathLen;
                memcpy(pBuf->szPath,
                       pszPath,
                       ulPathLen + 1);
            }
        }
    }

    return rc;
}

/*
 *@@ OPEN_POST:
 *      security callback for OPEN_POST.
 *
 *      As with all our hooks, this is stored in G_SecurityHooks
 *      (sec32_callbacks.c) force the OS/2 kernel to call us for
 *      each such event.
 */

ULONG CallType OPEN_POST(PSZ pszPath,
                         ULONG fsOpenFlags,
                         ULONG fsOpenMode,
                         ULONG SFN,
                         ULONG Action,
                         ULONG RC)
{
    if (    (G_pidShell)
         && (!DevHlp32_GetInfoSegs(&G_pGDT,
                                   &G_pLDT))
       )
    {
        if (G_bLog == LOG_ACTIVE)
        {
            PEVENTBUF_OPEN pBuf;
            ULONG   ulPathLen = strlen(pszPath);

            if (pBuf = ctxtLogEvent(EVENT_OPEN_POST,
                                    sizeof(EVENTBUF_OPEN) + ulPathLen))
            {
                pBuf->fsOpenFlags = fsOpenFlags;
                pBuf->fsOpenMode = fsOpenMode;
                pBuf->SFN = SFN;
                pBuf->Action = Action;
                pBuf->rc = RC;
                pBuf->ulPathLen = ulPathLen;
                memcpy(pBuf->szPath,
                       pszPath,
                       ulPathLen + 1);
            }
        }
    }

    return NO_ERROR;
}


