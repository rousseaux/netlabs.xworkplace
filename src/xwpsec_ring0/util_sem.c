
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
// #include "xwpsec32.sys\SecHlp.h"

#include "xwpsec32.sys\xwpsec_types.h"
#include "xwpsec32.sys\xwpsec_callbacks.h"

extern CHAR     G_szScratchBuf[1000] = "";
        // generic temporary buffer for composing strings etc.

// two global pointers to GDT and LDT info segs which can be used
// with DevHlp32_GetInfoSegs to avoid stack thunking.
extern struct InfoSegGDT *G_pGDT = 0;      // OS/2 global infoseg
extern struct InfoSegLDT *G_pLDT = 0;      // OS/2 local  infoseg

/*
 *@@ utilSemRequest:
 *      implements a mutex semaphore mechanism, using
 *      ProcBlock and ProcRun.
 *
 *      This uses a plain ULONG as the semaphore. Pass
 *      the pointer to that ULONG as the parameter,
 *      which is also used as the block ID for ProcBlock.
 *
 *      Preconditions:
 *
 *      1. The ULONG must be in the system arena to
 *         form a valid block ID (which must be unique
 *         for the entire system). Use a ULONG in the
 *         global data segment of the driver.
 *
 *      2. This works at task time only.
 *
 *      Usage:
 *
 +          ULONG ulMutex = 0;
 +
 +          ...
 +
 +          rc = utilSemRequest(&ulMutex, -1);
 +          if (rc == NO_ERROR)
 +          {
 +              // .. resource protected
 +
 +              utilSemRelease(&ulMutex);
 +                  // unblocks other threads
 +          }
 *
 */

/* int utilSemRequest(PULONG pulMutex,
                   ULONG ulTimeout)     // -1 for indefinite wait
{
    int rc = NO_ERROR;
    rc = DevHlp32_SemRequestRam1(ulTimeout);
    return (rc);
} */

int utilSemRequest(PULONG pulMutex,
                   ULONG ulTimeout)     // -1 for indefinite wait
{
    int rc = NO_ERROR;

    // 1)   On the first call (say, thread 1), *pulMutex
    //      is 0, and we can just continue.
    //      We then raise *pulMutex below to block
    //      out other threads while we're in here.

    // 2)   If some other thread (2) comes in here while the
    //      buffers are locked, it is blocked by the ProcBlock
    //      call below. When thread 1 is done, it unblocks all
    //      threads waiting on the buffers.
    //      Thread 2 then checks the buffer lock again.
    //      If several threads were blocked on the semaphore,
    //      the first one to run will receive the semaphore;
    //      all others will check the sem and block again.

    _disable();
    while (*pulMutex)
    {
        // mutex busy: block this thread...
        // it will be unlocked by the other call to
        // this function (below)
        rc = DevHlp32_ProcBlock((ULONG)pulMutex,
                                ulTimeout,
                                0);     // interruptible
        // thread 2 unblocks: check mutex again
        _disable();
    }
    // if (rc == NO_ERROR) // V0.9.6 (2000-11-27) [umoeller]
        (*pulMutex)++;
    _enable();

    // return (rc);
    return (NO_ERROR);
}

/*
 *@@ utilSemRelease:
 *      the reverse to utilSemRequest. See remarks there.
 */

VOID utilSemRelease(PULONG pulMutex)
{
    _disable();
    (*pulMutex)--;
    DevHlp32_ProcRun((ULONG)pulMutex);
    _enable();

    // DevHlp32_SemClearRam1();
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
    APIRET              arc = NO_ERROR;

    arc = DevHlp32_GetInfoSegs(&G_pGDT, &G_pLDT);
    if (arc == NO_ERROR)
        return (G_pLDT->LIS_CurProcID);
    else
        return (0);

    /* PTR16           p16Temp;

    if (DevHlp32_GetDosVar(DHGETDOSV_LOCINFOSEG,
                           __StackToFlat(&p16Temp),
                           0)
        == NO_ERROR)
    {
        // GetDosVar succeeded:
        // p16Temp now is a 16:16 pointer, which we need to
        // convert to flat first
        struct InfoSegLDT *pInfoSecLDT = 0;
        if (DevHlp32_VirtToLin(p16Temp, __StackToFlat(&pInfoSecLDT))
                == NO_ERROR)
            return (pInfoSecLDT->LIS_CurProcID);
    }

    return 0; */
}


