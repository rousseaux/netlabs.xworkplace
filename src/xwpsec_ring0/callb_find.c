
/*
 *@@sourcefile callb_move.c:
 *      SES kernel hook code.
 *
 *      See strat_init_base.c for an introduction to the driver
 *      structure in general.
 */

/*
 *      Copyright (C) 2000-2003 Ulrich M�ller.
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

#include "helpers\tree.h"
#include "helpers\xwpsecty.h"

#include "xwpsec32.sys\types.h"
#include "xwpsec32.sys\StackToFlat.h"
#include "xwpsec32.sys\devhlp32.h"

#include "security\ring0api.h"

#include "xwpsec32.sys\xwpsec_callbacks.h"

/*
 *@@ FindFirstWorker:
 *      common worker for FINDFIRST and FINDFIRST3X.
 *
 *@@added V1.0.2 (2003-11-13) [umoeller]
 */

ULONG FindFirstWorker(PCSZ pcszPath,
                      ULONG ulEventCode,
                      APIRET rc)
{
    if (    (G_pidShell)
         && (!DevHlp32_GetInfoSegs(&G_pGDT,
                                   &G_pLDT))
       )
    {
        // authorize event if it is not from XWPShell;
        // we can also save ourselves the trouble of
        // authorization if the call failed already
        // in the kernel
        PXWPSECURITYCONTEXT pThisContext;
        USHORT  fsGranted = 0;
        ULONG   ulPathLen = strlen(pcszPath);

        if (    (G_pidShell != G_pLDT->LIS_CurProcID)
             && (!rc)
           )
        {
            if (!(pThisContext = ctxtFind(G_pLDT->LIS_CurProcID)))
                rc = G_rcUnknownContext;
            else
            {
                fsGranted = ctxtQueryPermissions(pcszPath,
                                                 ulPathLen,
                                                 pThisContext->ctxt.cSubjects,
                                                 pThisContext->ctxt.aSubjects);

                // all bits of fsRequired must be set in fsGranted
                if ((fsGranted & XWPACCESS_READ) != XWPACCESS_READ)
                    rc = ERROR_ACCESS_DENIED;
            }
        }
        else
            pThisContext = NULL;

        if (G_bLog == LOG_ACTIVE)
        {
            PEVENTBUF_FILENAME pBuf;

            if (pBuf = ctxtLogEvent(pThisContext,
                                    ulEventCode,
                                    sizeof(EVENTBUF_FILENAME) + ulPathLen))
            {
                pBuf->rc = rc;
                pBuf->fsRequired = XWPACCESS_READ;
                pBuf->fsGranted = fsGranted;
                pBuf->ulPathLen = ulPathLen;
                memcpy(pBuf->szPath,
                       pcszPath,
                       ulPathLen + 1);
            }
        }
    }

    return rc;
}

/*
 *@@ FINDFIRST:
 *      SES kernel hook for FINDFIRST, the worker for DosFindFirst.
 *      This thing gets called with a specially set up FINDPARMS
 *      structure, after the kernel has already completed the
 *      actual find-first processing. I am not sure whether we
 *      even end up here if the find did not succeed with some
 *      error unrelated to authorization; to be sure, we check
 *      anyway.
 *
 *      As with all our hooks, this is stored in G_SecurityHooks
 *      (sec32_callbacks.c) force the OS/2 kernel to call us for
 *      each such event.
 *
 *      Required privileges:
 *
 *      --  XWPACCESS_READ on the directory.
 *
 *      Context: Possibly any ring-3 thread on the system.
 */

ULONG FINDFIRST(PFINDPARMS pParms)
{
    return FindFirstWorker(pParms->pszPath,
                           EVENT_FINDFIRST,
                           pParms->rc);
}

/*
 *@@ FINDFIRST3X:
 *      SES kernel hook for FINDFIRST3X (the DOS box version of
 *      find-first). From testing I get this from typing "DIR"
 *      at a COMMAND.COM prompt, for example.
 *
 *      As with all our hooks, this is stored in G_SecurityHooks
 *      (sec32_callbacks.c) force the OS/2 kernel to call us for
 *      each such event.
 *
 *      Required privileges:
 *
 *      --  XWPACCESS_READ on the directory.
 *
 *      Context: Possibly any ring-3 thread on the system.
 *
 *@@added V1.0.2 (2003-11-13) [umoeller]
 */

ULONG FINDFIRST3X(ULONG ulSrchHandle,
                  PSZ pszPath)
{
    return FindFirstWorker(pszPath,
                           EVENT_FINDFIRST3X,
                           NO_ERROR);
}

