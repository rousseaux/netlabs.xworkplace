
/*
 *@@sourcefile util_sem.c:
 *      security contexts implementation.
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

// #ifdef __IBMC__
// #pragma strings(readonly)
// #endif

#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_NOPMAPI
#include <os2.h>

#include <builtin.h>
#include <string.h>

#include "xwpsec32.sys\types.h"
#include "xwpsec32.sys\StackToFlat.h"
#include "xwpsec32.sys\DevHlp32.h"
#include "xwpsec32.sys\reqpkt32.h"

#include "xwpsec32.sys\xwpsec_types.h"
#include "xwpsec32.sys\xwpsec_callbacks.h"

#include "security\ring0api.h"

/* ******************************************************************
 *
 *   Private definitions
 *
 ********************************************************************/

/*
 *@@ LOGBUF:
 *      ring 0 logging buffer.
 *
 *      Logging works as follows:
 *
 *      1)  After the driver has been opened successfully
 *          by the shell, it starts a logging thread,
 *          which calls DosDevIOCtl into the driver with
 *          an XWPSECIO_GETLOGBUF function code. The thread
 *          is then blocked in the driver until logging
 *          data becomes available.
 *
 *      2)  Whenever ctxtLogEvent is called (e.g. from the
 *          OPEN_PRE callout), it receives the size of the
 *          data that is to go into the log buffers.
 *
 *          If no log buffer has yet been allocated (first
 *          call), ctxtLogEvent allocates one.
 *
 *          If we have a log buffer already, this means that
 *          the logging thread is currently unblocked and
 *          busy in ring 3. We then check if there is still
 *          sufficient memory available in it to store a new
 *          EVENTLOGENTRY.
 *
 *          If not, we allocate a new LOGBUF and append
 *          it to the previously last one.
 *
 *          In any case, we then append a new EVENTLOGENTRY
 *          to the LOGBUF (either the old one or the new one).
 *
 *      3)  If the ring-3 logging thread is currently blocked
 *          in the driver, we detach all logging buffers that
 *          have piled up so far and make them visible to
 *          the shell by calling VMGlobalToProcess. The ring-3
 *          thread is then unblocked so it can process the
 *          buffers in ring 3.
 *
 *      Event flow:
 *
 +          ring 3 logging thread       ring 0 (task time           ring 0 (other task)
 +                                      of logging thread,
 +                                      GETLOGBUF implement'n)
 +
 +          XWPShell starts logging
 +          thread,
 +
 +          calls IOCtl with GETLOGBUF
 +
 +                                      ctxtFillLogBuf() sees
 +                                      we have no LOGBUF data,
 +                                      so ProcBlock() logging
 +                                      thread
 +
 +      (*) (blocked)                                               OPEN_PRE calls ctxtLogEvent
 +          ³
 +          ³                                                       1) append LOGBUF data
 +          ³
 +          ³                                                       2) see that log thread is blocked:
 +          ³                                                          ProcRun() logging thread,
 +          ÀÄ (unblocked)                                             return
 +
 +                                      ProcBlock() returns in
 +                                      ctxtFillLogBuf():
 +                                      memcpy to ring-3 LOGBUF
 +
 +                                      return from ioctl
 +
 +          process logbufs
 +          ³
 +          ³
 +          ³                                                       OPEN_PRE calls ctxtLogEvent
 +          ³
 +          ³                                                       1) append LOGBUF data
 +          ³
 +          ³                                                       2) see that log thread is busy:
 +          ³                                                          do nothing
 +          ³
 +          ³
 +          ³                                                       OPEN_PRE calls ctxtLogEvent
 +          ³
 +          ³                                                       1) append LOGBUF data
 +          ³
 +          ³                                                       2) see that log thread is busy:
 +          ³                                                          do nothing
 +          ³
 +          ³
 +          À call IOCtl with GETLOGBUF
 +
 +                                      ctxtFillLogBuf()
 +                                      sees that we have data:
 +                                      memcpy to ring-3 LOGBUF
 +
 +                                      return from ioctl
 +
 +          process logbufs                                         (no events this time)
 +          ³
 +          ³
 +          À call IOCtl with GETLOGBUF
 +
 +                                      ctxtFillLogBuf() sees
 +                                      we have no LOGBUF data,
 +                                      so ProcBlock() logging
 +                                      thread
 +
 +                                      go back to (*) above
 *
 *@@added V1.0.1 (2003-01-10) [umoeller]
 */

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

extern struct InfoSegGDT
                    *G_pGDT = 0;        // OS/2 global infoseg
extern struct InfoSegLDT
                    *G_pLDT = 0;        // OS/2 local  infoseg

extern RING0STATUS  G_R0Status;         // in strat_ioctl.c

extern PLOGBUF  G_pLogFirst = NULL,     // ptr to first log buffer in linklist
                G_pLogLast = NULL;      // ptr to last log buffer in linklist

extern ULONG    G_cLogBufs = 0;         // no. of log buffers currently allocated

extern BYTE     G_bLog = LOG_INACTIVE;

extern ULONG    G_idLogBlock = 0;       // if != 0, the blockid that the logging thread
                                        // is currently blocked on;
                                        // if 0, the logging thread is either busy
                                        // in ring-3 or ready to run after a ProcRun()

/* ******************************************************************
 *
 *   Audit logging
 *
 ********************************************************************/

/*
 *@@ CreateLogBuffer:
 *
 *@@added V1.0.1 (2003-01-10) [umoeller]
 */

PLOGBUF CreateLogBuffer(VOID)
{
    PLOGBUF pBuf = utilAllocFixed(LOGBUFSIZE);    // 64K

    pBuf->cbUsed = sizeof(LOGBUF);
    pBuf->pNext = NULL;
    pBuf->cLogEntries = 0;

    // update global ring-0 status
    ++(G_R0Status.cLogBufs);

    if (G_R0Status.cLogBufs > G_R0Status.cMaxLogBufs)
        G_R0Status.cMaxLogBufs = G_R0Status.cLogBufs;

    return pBuf;
}

/*
 *@@ RunLoggerThread:
 *
 *@@added V1.0.1 (2003-01-10) [umoeller]
 */

VOID RunLoggerThread(VOID)
{
    // unblock ring-3 thread, if blocked
    if (G_idLogBlock)
    {
        DevHlp32_ProcRun(G_idLogBlock);

        // do this only once; we may end up with
        // another event to log even before the
        // logger thread gets a chance to run,
        // but it will run eventually, and will
        // take all the data that has piled up
        // until then
        // G_idLogBlock = 0;
    }
}

/*
 *@@ ctxtLogEvent:
 *      appends a new log entry to the global logging
 *      data.
 *
 *      Runs at task time of any thread in the system
 *      that calls a system API, including XWPShell
 *      itself. For example, we end up here whenever
 *      someone calls DosOpen through the OPEN_PRE
 *      callout.
 *
 *      Preconditions:
 *
 *      --  DevHlp32_GetInfoSegs _must_ have been called
 *          beforehand so that the global "local infoseg"
 *          ptr is valid.
 *
 *@@added V1.0.1 (2003-01-10) [umoeller]
 */

VOID ctxtLogEvent(ULONG ulEventCode,      // in: EVENT_* code
                  PVOID pvData,           // in: event-specific data
                  ULONG cbData)           // in: size of that buffer
{
    ULONG   cbLogEntry =   sizeof(EVENTLOGENTRY)
                         + cbData;

    // do we have a log buffer allocated currently?
    if (!G_pLogLast)
    {
        // no: that's easy, allocate one log buffer and
        // set it as the first and last
        G_pLogFirst
        = G_pLogLast
        = CreateLogBuffer();
    }
    else
    {
        // check if we have enough room left in this
        // log buffer
        if (G_pLogLast->cbUsed + cbLogEntry >= LOGBUFSIZE)      // 64K
        {
            // no: allocate a new one
            G_pLogLast->pNext = CreateLogBuffer();
            // and set this as the last buffer
            G_pLogLast = G_pLogLast->pNext;

            // G_pLogFirst remains the same
        }
    }

    if (!G_pLogLast)
        // error allocating memory:
        // stop logging globally!
        G_bLog = LOG_ERROR;
    else
    {
        // determine where to copy stuff to in LOGBUF
        PEVENTLOGENTRY pEntry = (PEVENTLOGENTRY)((PBYTE)G_pLogLast + G_pLogLast->cbUsed);
        // event-specific data follows right after
        PBYTE pbCopyTo = (PBYTE)pEntry + sizeof(EVENTLOGENTRY);

        pEntry->cbStruct = cbLogEntry;
        pEntry->ulEventCode = ulEventCode;

        // copy the first 16 bytes from local infoseg
        // into log buffer (these match our CONTEXTINFO
        // declaration in ring0api.h)
        memcpy(&pEntry->ctxt,
               G_pLDT,
               sizeof(CONTEXTINFO));

        // copy the first 20 bytes from global infoseg
        // into log buffer (these match our TIMESTAMP
        // declaration in ring0api.h)
        memcpy(&pEntry->stamp,
               G_pGDT,
               sizeof(TIMESTAMP));

        // copy event-specific data
        memcpy(pbCopyTo,
               pvData,
               cbData);

        // update the current LOGBUF
        G_pLogLast->cbUsed += cbLogEntry;
        ++(G_pLogLast->cLogEntries);

        // update global ring-0 status
        ++(G_R0Status.cLogged);

        // unblock ring-3 thread if necessary
        RunLoggerThread();
    }
}

/*
 *@@ ctxtStopLogging:
 *      gets called from ioctlDeregisterDaemon to clean up
 *      when the driver is closed.
 *
 *@@added V1.0.1 (2003-01-10) [umoeller]
 */

VOID ctxtStopLogging(VOID)
{
    // free all memory
    PLOGBUF pThis = G_pLogFirst;
    while (pThis)
    {
        PLOGBUF pNext = pThis->pNext;
        utilFreeFixed(pThis,
                      LOGBUFSIZE);     // 64K
        --(G_R0Status.cLogBufs);
        pThis = pNext;
    }

    // stop logging until next open
    G_bLog = LOG_ERROR;

    // force running the logger thread
    // or we'll have a zombie
    DevHlp32_ProcRun(G_idLogBlock);
}

/*
 *@@ ctxtFillLogBuf:
 *      implementation for XWPSECIO_GETLOGBUF in sec32_ioctl().
 *
 *      This gets called at task time of the ring-3 logging
 *      thread only. That thread keeps calling this IOCtl
 *      function and assumes to be blocked until data is
 *      available.
 *
 *      See LOGBUF for the general event flow.
 *
 *      This function must:
 *
 *      1)  Check if logging data is available.
 *
 *          --  If we have data, go to 2).
 *
 *          --  If not, we block. We will get unblocked by
 *              ctxtLogEvent as soon as data comes in.
 *
 *      2)  Copy first logging buffer to ring-3 memory.
 *
 *      3)  In any case, we return with logging data.
 *
 *@@added V1.0.1 (2003-01-10) [umoeller]
 */

IOCTLRET ctxtFillLogBuf(PLOGBUF pLogBufR3,              // in: flat pointer to ring-3 mem from ioctl
                        ULONG blockid)                  // in: 16:16 pointer of reqpkt for ProcBlock id
{
    IOCTLRET    rc = NO_ERROR;

    switch (G_bLog)
    {
        case LOG_INACTIVE:
            // very first call:
            // activate logging from now on
            G_bLog = LOG_ACTIVE;
        break;

        case LOG_ERROR:
            // we had an error previously:
            // never do anything then
            return ERROR_I24_GEN_FAILURE;
    }

    _disable();
    while (    (!G_pLogFirst)
            && (G_bLog == LOG_ACTIVE)
          )
    {
        // no logging data yet:
        // block on the 16-bit request packet address
        G_idLogBlock = blockid;
        if (DevHlp32_ProcBlock(G_idLogBlock,
                               -1,     // wait forever
                               0))     // interruptible
        {
            // probably thread died or something
            rc = ERROR_I24_CHAR_CALL_INTERRUPTED;
        }

        _disable();
    }
    _enable();

    // in any case, thread is no longer blocked,
    // so reset global blockid
    G_idLogBlock = 0;

    // the logging memory _might_ have been freed
    // if the driver was closed from XWPShell via
    // ctxtStopLogging(), so check again if we
    // really have memory
    if (    (!G_pLogFirst)
         || (G_bLog != LOG_ACTIVE)
       )
        rc = ERROR_I24_CHAR_CALL_INTERRUPTED;
    else if (!rc)
    {
        // we have logging data:
        // backup "first" pointer
        PLOGBUF pFirst = G_pLogFirst;

        memcpy(pLogBufR3,
               G_pLogFirst,
               G_pLogFirst->cbUsed);

        // unlink this record
        if (G_pLogFirst == G_pLogLast)
            // we only had one buffer: unset last
            G_pLogLast = NULL;

        G_pLogFirst = G_pLogFirst->pNext;       // will be NULL if this was last

        utilFreeFixed(pFirst,
                      LOGBUFSIZE);     // 64K
        --(G_R0Status.cLogBufs);

        G_idLogBlock = 0;
    }

    return rc;
}

/* ******************************************************************
 *
 *   Security contexts implementation
 *
 ********************************************************************/

/* ******************************************************************
 *
 *   Security contexts implementation
 *
 ********************************************************************/


