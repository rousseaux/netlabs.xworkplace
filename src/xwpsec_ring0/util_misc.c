
/*
 *@@sourcefile util_sem.c:
 *      semaphore utilities.
 */

/*
 *      Copyright (C) 2000 Ulrich M”ller.
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, in version 2 as it comes in the COPYING
 *      file of the XWorkplace main distribution.
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 */

#ifdef __IBMC__
#pragma strings(readonly)
#endif

#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_NOPMAPI
#include <os2.h>

#include <builtin.h>

#include "xwpsec32.sys\types.h"
#include "xwpsec32.sys\StackToFlat.h"
#include "xwpsec32.sys\DevHlp32.h"
#include "xwpsec32.sys\reqpkt32.h"

#include "xwpsec32.sys\xwpsec_types.h"
#include "xwpsec32.sys\xwpsec_callbacks.h"

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

struct InfoSegGDT *G_pGDT = 0;      // OS/2 global infoseg
struct InfoSegLDT *G_pLDT = 0;      // OS/2 local  infoseg

/* ******************************************************************
 *
 *   Utilities
 *
 ********************************************************************/

/*
 *@@ utilNeedsVerify:
 *      returns TRUE if access control is enabled
 *      AND the caller's PID (task time!) is not
 *      that of the ring-3 daemon.
 */

BOOL utilNeedsVerify(VOID)
{
    return (    (G_pidShell)
             && (G_pidShell != utilGetTaskPID())
           );
}

/*
 *@@ utilGetTaskPID:
 *      returns the PID of the caller's process.
 *      Valid at task time only.
 *
 *      Returns 0 on errors, the PID otherwise.
 *
 *@@added V0.9.4 (2000-07-03) [umoeller]
 */

unsigned long utilGetTaskPID(void)
{
    if (!DevHlp32_GetInfoSegs(&G_pGDT,
                              &G_pLDT))
        return G_pLDT->LIS_CurProcID;

    return 0;
}


