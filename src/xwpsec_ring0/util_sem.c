
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
#include "xwpsec32.sys\SecHlp.h"

#include "xwpsec32.sys\xwpsec_types.h"
#include "xwpsec32.sys\xwpsec_callbacks.h"

/*
 *@@ utilSemRequest:
 *      implements a mutex semaphore mechanism, using
 *      ProcBlock and ProcRun.
 *
 *      This uses a plain ULONG as the semaphore. Pass
 *      the pointer to that ULONG as the parameter,
 *      which is also used as the block ID for ProcBlock.
 *
 *      This is for task time only.
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
        DevHlp32_Beep(1000, 10);
        rc = DevHlp32_ProcBlock((ULONG)pulMutex,
                                ulTimeout,
                                0);     // interruptible
        // thread 2 unblocks: check mutex again
        _disable();
    }
    if (rc == NO_ERROR)
        (*pulMutex)++;
    _enable();

    return (rc);
}

/*
 *@@ utilSemRelease:
 *      the reverse to utilSemRequest. See remarks there.
 */

VOID utilSemRelease(PULONG pulMutex)
{
    (*pulMutex)--;
    DevHlp32_ProcRun((ULONG)pulMutex);

    // DevHlp32_SemClearRam1();
}




