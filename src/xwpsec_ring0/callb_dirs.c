
/*
 *@@sourcefile callb_move.c:
 *      SES kernel hook code.
 *
 *      See strat_init_base.c for an introduction.
 */

/*
 *      Copyright (C) 2000-2003 Ulrich M”ller.
 *      Based on the MWDD32.SYS example sources,
 *      Copyright (C) 1995, 1996, 1997  Matthieu Willm (willm@ibm.net).
 *
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

#include "xwpsec32.sys\xwpsec_callbacks.h"

/*
 *@@ MAKEDIR:
 *      SES kernel hook for MAKEDIR.
 *
 *      As with all our hooks, this is stored in G_SecurityHooks
 *      (sec32_callbacks.c) force the OS/2 kernel to call us for
 *      each such event.
 *
 *      Required privileges:
 *
 *      --  XWPACCESS_CREATE on the directory.
 *
 *      Context: Possibly any ring-3 thread on the system.
 */

ULONG MAKEDIR(PSZ pszPath)
{
    APIRET  rc = NO_ERROR;

    if (    (G_pidShell)
         && (!DevHlp32_GetInfoSegs(&G_pGDT,
                                   &G_pLDT))
       )
    {
        if (G_bLog == LOG_ACTIVE)
        {
            PEVENTBUF_FILENAME pBuf;
            ULONG   ulPathLen = strlen(pszPath);

            if (pBuf = ctxtLogEvent(EVENT_MAKEDIR,
                                    sizeof(EVENTBUF_FILENAME) + ulPathLen))
            {
                pBuf->rc = rc;
                pBuf->ulPathLen = ulPathLen;
                memcpy(pBuf->szPath,
                       pszPath,
                       ulPathLen + 1);
            }
        }
    }

    return NO_ERROR;
}

/*
 *@@ CHANGEDIR:
 *      SES kernel hook for CHANGEDIR.
 *
 *      As with all our hooks, this is stored in G_SecurityHooks
 *      (sec32_callbacks.c) force the OS/2 kernel to call us for
 *      each such event.
 *
 *      Required privileges:
 *
 *      --  XWPACCESS_EXEC on the directory.
 *
 *      Context: Possibly any ring-3 thread on the system.
 */

ULONG CHANGEDIR(PSZ pszPath)
{
    APIRET  rc = NO_ERROR;

    if (    (G_pidShell)
         && (!DevHlp32_GetInfoSegs(&G_pGDT,
                                   &G_pLDT))
       )
    {
        if (G_bLog == LOG_ACTIVE)
        {
            PEVENTBUF_FILENAME pBuf;
            ULONG   ulPathLen = strlen(pszPath);

            if (pBuf = ctxtLogEvent(EVENT_CHANGEDIR,
                                    sizeof(EVENTBUF_FILENAME) + ulPathLen))
            {
                pBuf->rc = rc;
                pBuf->ulPathLen = ulPathLen;
                memcpy(pBuf->szPath,
                       pszPath,
                       ulPathLen + 1);
            }
        }
    }

    return NO_ERROR;
}

/*
 *@@ REMOVEDIR:
 *      SES kernel hook for REMOVEDIR.
 *
 *      As with all our hooks, this is stored in G_SecurityHooks
 *      (sec32_callbacks.c) force the OS/2 kernel to call us for
 *      each such event.
 *
 *      Required privileges:
 *
 *      --  XWPACCESS_DELETE on the directory.
 *
 *      Context: Possibly any ring-3 thread on the system.
 */

ULONG REMOVEDIR(PSZ pszPath)
{
    APIRET  rc = NO_ERROR;

    if (    (G_pidShell)
         && (!DevHlp32_GetInfoSegs(&G_pGDT,
                                   &G_pLDT))
       )
    {
        if (G_bLog == LOG_ACTIVE)
        {
            PEVENTBUF_FILENAME pBuf;
            ULONG   ulPathLen = strlen(pszPath);

            if (pBuf = ctxtLogEvent(EVENT_REMOVEDIR,
                                    sizeof(EVENTBUF_FILENAME) + ulPathLen))
            {
                pBuf->rc = rc;
                pBuf->ulPathLen = ulPathLen;
                memcpy(pBuf->szPath,
                       pszPath,
                       ulPathLen + 1);
            }
        }
    }

    return NO_ERROR;
}


