
/*
 *@@sourcefile callb_exec.c:
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

/* ******************************************************************
 *
 *   Callouts
 *
 ********************************************************************/

/*
 *@@ LOADEROPEN:
 *      SES kernel hook for LOADEROPEN.
 *
 *      As with all our hooks, this is stored in G_SecurityHooks
 *      (sec32_callbacks.c) force the OS/2 kernel to call us for
 *      each such event.
 *
 *      This is a "pre" event. Required privileges:
 *
 *      --  XWPACCESS_EXEC on the executable.
 *
 *      LOADEROPEN always receives a fully qualified pathname.
 *
 *      --  For EXE files, we get this sequence for the EXE file,
 *          running in the context of the thread that is running
 *          DosExecPgm:
 *
 *          1)  OPEN_PRE, OPEN_POST
 *          2)  LOADEROPEN
 *          3)  GETMODULE
 *          4)  EXEC_PRE, EXEC_POST (which returns the new PID)
 *
 *      --  For unqualified DLL names that have not yet been loaded,
 *          we get:
 *
 *          1)  GETMODULE short name
 *          2)  OPEN_PRE, OPEN_POST with the long name for every
 *              directory along the LIBPATH
 *          3)  LOADEROPEN with the long name that was found
 *
 *      --  For unqualified DLL names that are already loaded, we get:
 *
 *          1)  GETMODULE short name
 *
 *          only. @@todo How do we authenticate this? We can
 *          presume that this module is already loaded, and we
 *          must check the full file's permissions!
 *
 *      --  For fully qualified DLL names, we get:
 *
 *          1)  GETMODULE full name
 *          2)  OPEN_PRE, OPEN_POST with the full name
 *          3)  LOADEROPEN
 *
 *      Context: Possibly any ring-3 thread on the system.
 */

ULONG LOADEROPEN(PSZ pszPath,
                 ULONG SFN)
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
            PEVENTBUF_LOADEROPEN pBuf;
            ULONG   ulPathLen = strlen(pszPath);

            if (pBuf = ctxtLogEvent(EVENT_LOADEROPEN,
                                    sizeof(EVENTBUF_LOADEROPEN) + ulPathLen))
            {
                pBuf->SFN = SFN;
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
 *@@ GETMODULE:
 *      SES kernel hook for GETMODULE.
 *
 *      As with all our hooks, this is stored in G_SecurityHooks
 *      (sec32_callbacks.c) force the OS/2 kernel to call us for
 *      each such event.
 *
 *      This is a "pre" event. Required privileges:
 *
 *      --  XWPACCESS_EXEC on the executable.
 *
 *      This can come in with "pure" (unqualified) module names.
 *      See LOADEROPEN for details.
 *
 *      However if the module is already in memory, it is not
 *      followed by any other callout. We should then check if
 *      the current task is allowed to see the module. @@todo
 *      How do we figure out the access rights if the module
 *      name is _not_ fully qualified?
 *
 *      Context: Possibly any ring-3 thread on the system.
 */

ULONG GETMODULE(PSZ pszPath)
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
            PEVENTBUF_FILENAME pBuf;
            ULONG   ulPathLen = strlen(pszPath);

            if (pBuf = ctxtLogEvent(EVENT_GETMODULE,
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

    return rc;
}

/*
 *@@ EXECPGM:
 *      SES kernel hook for EXECPGM.
 *
 *      As with all our hooks, this is stored in G_SecurityHooks
 *      (sec32_callbacks.c) force the OS/2 kernel to call us for
 *      each such event.
 *
 *      Comes in for EXE files only.
 *
 *      Required privileges:
 *
 *      --  XWPACCESS_EXEC on the executable.
 *
 *      Context: Possibly any ring-3 thread on the system.
 */

ULONG EXECPGM(PSZ pszPath,
              PCHAR pchArgs)
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
            PEVENTBUF_FILENAME pBuf;
            ULONG   ulPathLen = strlen(pszPath);

            if (pBuf = ctxtLogEvent(EVENT_EXECPGM_PRE,
                                    sizeof(EVENTBUF_FILENAME) + ulPathLen))
            {
                pBuf->rc = rc;
                pBuf->ulPathLen = ulPathLen;
                memcpy(pBuf->szPath,
                       pszPath,
                       ulPathLen + 1);

                // log arguments in a second buffer because
                // this can be up to 64K in itself @@todo
                // EVENT_EXECPGM_ARGS
            }
        }
    }

    return rc;
}

/*
 *@@ EXECPGM_POST:
 *      SES kernel hook for EXECPGM_POST.
 *
 *      As with all our hooks, this is stored in G_SecurityHooks
 *      (sec32_callbacks.c) force the OS/2 kernel to call us for
 *      each such event.
 *
 *      Context: Possibly any ring-3 thread on the system.
 */

VOID EXECPGM_POST(PSZ pszPath,
                  PCHAR pchArgs,
                  ULONG NewPID)
{
    if (    (G_pidShell)
         && (!DevHlp32_GetInfoSegs(&G_pGDT,
                                   &G_pLDT))
       )
    {
        if (G_bLog == LOG_ACTIVE)
        {
            PEVENTBUF_FILENAME pBuf;
            ULONG   ulPathLen = strlen(pszPath);

            if (pBuf = ctxtLogEvent(EVENT_EXECPGM_POST,
                                    sizeof(EVENTBUF_FILENAME) + ulPathLen))
            {
                pBuf->rc = NewPID;
                pBuf->ulPathLen = ulPathLen;
                memcpy(pBuf->szPath,
                       pszPath,
                       ulPathLen + 1);
            }
        }
    }
}


