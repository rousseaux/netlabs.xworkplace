
/*
 *@@sourcefile util_daemon.c:
 *
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

#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_NOPMAPI
#include <os2.h>

#include <builtin.h>        // interrupts: _disable etc.

#include "xwpsec32.sys\types.h"
#include "xwpsec32.sys\StackToFlat.h"
#include "xwpsec32.sys\DevHlp32.h"
#include "xwpsec32.sys\reqpkt32.h"
#include "xwpsec32.sys\SecHlp.h"

#include "security\ring0api.h"

#include "xwpsec32.sys\xwpsec_types.h"
#include "xwpsec32.sys\xwpsec_callbacks.h"

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

ULONG           G_ulBufferLockCount = 0;

extern ULONG    G_blockidCmdComplete = 0,
                G_cCmdComplete = 0,
                G_blockidCmdPending = 0,
                G_cCmdPending = 0;

/* ******************************************************************
 *
 *   Daemon communication
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
    if (G_fDaemonReady)
    {
        // access control enabled by ring-3 daemon:
        ULONG   pid;
        if (    (pid = utilGetTaskPID())
                != 0)
        {
            // pid contains caller's PID:
            if (pid != G_pidRing3Daemon)
                // not daemon process:
                return (TRUE);
        }
    }

    return (FALSE);
}

/*
 *@@ utilDaemonRequest:
 *      called from the various security kernel hooks
 *      when the daemon needs to verify access for something.
 *
 *      This runs at task time of the process whose API
 *      call has been hooked. For example, if an application
 *      calls DosOpen, its thread is arriving here.
 *      This blocks the thread until the daemon has authorized
 *      the request.
 *
 *      If the daemon is currently busy processing a
 *      previous request, the current thread is blocked
 *      here on G_rsemBuffersLocked until the daemon is
 *      done.
 *
 *      This returns either NO_ERROR or some error code
 *      from either the daemon or from this function.
 *      The caller should decide whether it's cool to
 *      pass all these codes back to the kernel callout.
 *
 *      Preconditions:
 *
 *      -- The caller MUST call
 +              utilSemRequest(&G_hmtxBufferLocked, -1)
 *         before calling this function, and
 +              utilSemRelease(&G_hmtxBufferLocked)
 *         after return.
 *
 *@@added V [umoeller]
 */

int utilDaemonRequest(ULONG ulEventCode)    // in: event code
{
    int rc = NO_ERROR;

    struct InfoSegGDT *pGDT = 0;      // OS/2 global infoseg
    struct InfoSegLDT *pLDT = 0;      // OS/2 local  infoseg

    // store event code
    ((PSECIOSHARED)G_pSecIOShared)->ulEventCode = ulEventCode;

    // store PID of process and TID of thread
    // (we're running at task time of thread who
    // requested access to resource!)

    if (!(rc = DevHlp32_GetInfoSegs(__StackToFlat(&pGDT),
                                    __StackToFlat(&pLDT))))
    {
        // PID
        ((PSECIOSHARED)G_pSecIOShared)->ulCallerPID = pLDT->LIS_CurProcID;
        // PPID
        ((PSECIOSHARED)G_pSecIOShared)->ulParentPID = pLDT->LIS_ParProcID;
        // TID
        ((PSECIOSHARED)G_pSecIOShared)->ulCallerTID = pLDT->LIS_CurThrdID;

        // unblock the daemon
        DevHlp32_ProcRun((ULONG)&G_blockidCmdPending);

        // we now need to block until daemon is done.
        // This isn't that trivial because we don't
        // have a DevHlp32_WaitEventSem, unfortunately.
        // So we need to do a ProcBlock until the
        // driver uses DevIOCtl to signal that it's
        // done with the request.

        // For the block Id we use G_blockidCmdComplete.
        // This is safe because only one thread on
        // the entire system can ever block on that
        // ID, since we are serialized by the
        // G_rsemBuffersLocked mutex in this section.

        // utilWriteLog("    Blocking... G_cCmdComplete: %d\r\n", G_cCmdComplete);

        G_cCmdComplete++;
        _disable();
        rc = DevHlp32_ProcBlock((ULONG)&G_blockidCmdComplete,
                                -1,     // wait forever
                                0);     // interruptible
               // the corresponding ProcRun() is in utilDaemonDone(),
               // which gets called from the ioctl

        if (rc == NO_ERROR)
        {
            // daemon has returned:
            // it has then put the result code (NO_ERROR
            // or ERROR_ACCESS_DENID or whatever) into
            // G_pSecIOShared->arc

            rc = ((PSECIOSHARED)G_pSecIOShared)->arc;
                // this is then returned from the API call, e.g. DosOpen
        }
    }

    return (rc);
}

/*
 *@@ utilDaemonDone:
 *      unblocks the application thread blocked
 *      in utilDaemonRequest waiting for access
 *      verification.
 *
 *      This always runs at task time of the ring-3
 *      daemon thread (during its IOCtl).
 *
 *      This returns the no. of threads awakened, which
 *      should be 1.
 */

VOID utilDaemonDone(VOID)
{
    if (G_cCmdComplete)
    {
        // application thread currently blocked:
        G_cCmdComplete--;
        DevHlp32_ProcRun((ULONG)&G_blockidCmdComplete);
    }
}
