
/*
 *@@sourcefile sec32_contexts.c:
 *      security contexts implementation.
 *
 *      See strat_init_base.c for an introduction.
 */

/*
 *      Copyright (C) 2000-2003 Ulrich M�ller.
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

#include "xwpsec32.sys\xwpsec_callbacks.h"

#include "security\ring0api.h"

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

extern HVDHSEM      G_hmtx;             // in strat_ioctl.c

extern PLOGBUF  G_pLogFirst = NULL,     // ptr to first log buffer in linklist
                G_pLogLast = NULL;      // ptr to last log buffer in linklist

extern ULONG    G_cLogBufs = 0;         // no. of log buffers currently allocated

extern ULONG    G_idLogBufNext = 0,     // global log buf ID (counter)
                G_idEventNext = 0;      // global event ID (counter)

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
 *      Logging works as follows:
 *
 *      1)  After the driver has been opened successfully
 *          by the shell, it starts a logging thread,
 *          which calls DosDevIOCtl into the driver with
 *          an XWPSECIO_GETLOGBUF function code. This
 *          ends up in ctxtFillLogBuf() where the thread
 *          is blocked until logging data becomes available.
 *
 *      2)  Whenever ctxtLogEvent is called (e.g. from the
 *          OPEN_PRE callout), it receives the size of the
 *          data that is to go into the log buffers.
 *
 *          If no log buffer has yet been allocated, or if
 *          there's one, but there's not enough room left
 *          for the new data, ctxtLogEvent allocates one.
 *          The driver maintains a linked list of logging
 *          buffers and always appends to the last one
 *          (while the ring-3 thread always takes off the
 *          first buffer from the list).
 *
 *          Otherwise we append to the current logging buffer.
 *
 *          In any case, we then append a new EVENTLOGENTRY
 *          to the LOGBUF (either the old one or the new one).
 *
 *      3)  We then check if the ring-3 thread is currently
 *          blocked. If so, we unblock it; otherwise it's
 *          still busy processing previous data and we do
 *          nothing.
 *
 *      Whenever the ring-3 thread gets unblocked,
 *      ctxtFillLogBuf() takes the first buffer off the list
 *      and copies it into ring-3 memory. The buffer from
 *      the list is then freed again.
 *
 *      Sample event flow:
 *
 +          ring 3 logging thread     � ring 0 (task time          � ring 0 (other task)
 +                                    � of logging thread,         �
 +                                    � GETLOGBUF implement'n)     �
 +                                    �                            �
 +      ��������������������������������������������������������������������������������������������
 +                                    �                            �
 +          XWPShell starts logging   �                            �
 +          thread,                   �                            �
 +                                    �                            �
 +          calls GETLOGBUF ioctl     �                            �
 +                                    �                            �
 +                                    � ctxtFillLogBuf() sees      �
 +                                    � we have no LOGBUF data,    �
 +                                    � so ProcBlock() logging     �
 +                                    � thread                     �
 +                                    �                            �
 +      (*) (blocked)                 �                            � OPEN_PRE calls ctxtLogEvent
 +          �                         �                            �
 +          �                         �                            � 1) append LOGBUF data
 +          �                         �                            �
 +          �                         �                            � 2) see that log thread is blocked:
 +          �                         �                            �    ProcRun() logging thread,
 +          �� (unblocked)            �                            �    return
 +                                    �                            �
 +                                    � ProcBlock() returns in     �
 +                                    � ctxtFillLogBuf():          �
 +                                    �                            �
 +                                    � memcpy to ring-3 LOGBUF    �
 +                                    � return from ioctl          �
 +                                    �                            �
 +          process logbufs           �                            �
 +          �                         �                            �
 +          �                         �                            �
 +          �                         �                            � OPEN_PRE calls ctxtLogEvent
 +          �                         �                            �
 +          �                         �                            � 1) append LOGBUF data
 +          �                         �                            �
 +          �                         �                            � 2) see that log thread is busy:
 +          �                         �                            �    do nothing
 +          �                         �                            �
 +          �                         �                            �
 +          �                         �                            � OPEN_PRE calls ctxtLogEvent
 +          �                         �                            �
 +          �                         �                            � 1) append LOGBUF data
 +          �                         �                            �
 +          �                         �                            � 2) see that log thread is busy:
 +          �                         �                            �    do nothing
 +          �                         �                            �
 +          �                         �                            �
 +          � calls GETLOGBUF ioctl   �                            �
 +                                    �                            �
 +                                    � ctxtFillLogBuf()           �
 +                                    � sees that we have data:    �
 +                                    � do not block               �
 *                                    �                            �
 +                                    � memcpy to ring-3 LOGBUF    �
 +                                    � return from ioctl          �
 +                                    �                            �
 +          process logbufs           �                            � (no events this time)
 +          �                         �                            �
 +          �                         �                            �
 +          � calls GETLOGBUF ioctl   �                            �
 +                                    �                            �
 +                                    � ctxtFillLogBuf() sees      �
 +                                    � we have no LOGBUF data,    �
 +                                    � so ProcBlock() logging     �
 +                                    � thread                     �
 +                                    �                            �
 +                                    � go back to (*) above       �
 +                                    �                            �
 +      ���������������������������������������������������������������������������������������������
 *
 */

/*
 *@@ AllocLogBuffer:
 *      allocates and initializes a new LOGBUF
 *      as fixed kernel memory.
 *
 *      Preconditions: Caller must hold the big lock
 *      cos VMAlloc may block!
 *
 *      Context: Possibly any ring-3 thread on the system.
 */

PLOGBUF AllocLogBuffer(VOID)
{
    PLOGBUF pBuf;

    if (pBuf = utilAllocFixed(LOGBUFSIZE))    // 64K
    {
        pBuf->cbUsed = sizeof(LOGBUF);
        pBuf->pNext = NULL;
        pBuf->idLogBuf = G_idLogBufNext++;
        pBuf->cLogEntries = 0;

        // update global ring-0 status
        ++(G_R0Status.cLogBufs);

        if (G_R0Status.cLogBufs > G_R0Status.cMaxLogBufs)
            G_R0Status.cMaxLogBufs = G_R0Status.cLogBufs;
    }

    return pBuf;
}

/*
 *@@ ctxtLogEvent:
 *      appends a new EVENTLOGENTRY to the global logging
 *      data and returns a pointer to where event-specific
 *      data can be copied.
 *
 *      Runs at task time of any thread in the system
 *      that calls a system API, including XWPShell
 *      itself. For example, we end up here whenever
 *      someone calls DosOpen through the OPEN_PRE
 *      callout.
 *
 *      Each log entry consists of a fixed-size
 *      EVENTLOGENTRY struct with information like
 *      date, time, pid, and tid of the event and
 *      the event code. This data is filled in by
 *      this function.
 *
 *      This function returns a pointer to the first
 *      byte after that structure where the caller must
 *      copy the event-specific data to. This is to avoid
 *      multiple memcpy's so the logger can memcpy directly
 *      into the buffer. However, the cbData passed in to
 *      this function needs to be the size of the entire
 *      _variable_ data so we can reserve memory correctly
 *      already in here.
 *
 *      Preconditions:
 *
 *      --  Call this only if G_bLog == LOG_ACTIVE or
 *          we'll leak memory.
 *
 *      --  DevHlp32_GetInfoSegs _must_ have been called
 *          beforehand so that the global "local infoseg"
 *          ptr is valid.
 *
 *      --  This possibly allocates a new logging buffer
 *          via VMAlloc, so I guess this can block. As
 *          a result, the caller must be reentrant and
 *          not rely on static data before and after this
 *          call.
 *
 *      Context: Possibly any ring-3 thread on the system.
 */

PVOID ctxtLogEvent(ULONG ulEventCode,      // in: EVENT_* code
                   ULONG cbData)           // in: size of that buffer
{
    PVOID   pvReturn = NULL;
    // determine size we need:
    ULONG   cbLogEntry =   sizeof(EVENTLOGENTRY)    // fixed-size struct first
                         + cbData;                  // event-specific data next

    if (!VDHRequestMutexSem(G_hmtx, -1))
        ctxtStopLogging();
    else
    {
        // do we have a log buffer allocated currently?
        if (!G_pLogLast)
        {
            // no: that's easy, allocate one log buffer and
            // set it as the first and last
            G_pLogFirst
            = G_pLogLast
            = AllocLogBuffer();
        }
        else
        {
            // check if we have enough room left in this
            // log buffer
            if (G_pLogLast->cbUsed + cbLogEntry >= LOGBUFSIZE)      // 64K
            {
                // no: allocate a new one
                G_pLogLast->pNext = AllocLogBuffer();
                // and set this as the last buffer
                G_pLogLast = G_pLogLast->pNext;

                // G_pLogFirst remains the same
            }
        }

        if (!G_pLogLast)
            // error allocating memory:
            // stop logging globally!
            ctxtStopLogging();
        else
        {
            // determine target address of new EVENTLOGENTRY in LOGBUF
            PEVENTLOGENTRY pEntry = (PEVENTLOGENTRY)((PBYTE)G_pLogLast + G_pLogLast->cbUsed);

            // 1) fill fixed EVENTLOGENTRY struct

            pEntry->cbStruct = cbLogEntry;
            pEntry->ulEventCode = ulEventCode;
            pEntry->idEvent = G_idEventNext++;      // global counter

            // copy the first 16 bytes from local infoseg
            // into log entry (these match our CONTEXTINFO
            // declaration in ring0api.h)
            memcpy(&pEntry->ctxt,
                   G_pLDT,
                   sizeof(CONTEXTINFO));

            // copy the first 20 bytes from global infoseg
            // into log entry (these match our TIMESTAMP
            // declaration in ring0api.h)
            memcpy(&pEntry->stamp,
                   G_pGDT,
                   sizeof(TIMESTAMP));

            // 2) return ptr to event-specific data
            pvReturn = (PBYTE)pEntry + sizeof(EVENTLOGENTRY);

            // update the current LOGBUF
            G_pLogLast->cbUsed += cbLogEntry;
            ++(G_pLogLast->cLogEntries);

            // update global ring-0 status
            ++(G_R0Status.cLogged);

            // unblock ring-3 thread, if blocked
            if (G_idLogBlock)
                DevHlp32_ProcRun(G_idLogBlock);
        }

        VDHReleaseMutexSem(G_hmtx);
    }

    return pvReturn;
}

/*
 *@@ ctxtStopLogging:
 *      gets called from ioctlDeregisterDaemon to clean up
 *      when the driver is closed. Aside from freeing all
 *      remaining buffers, we must explicitly unblock the
 *      ring-3 thread or we'll end up with a zombie shell.
 *
 *      Context: "close" request packet from XWPShell,
 *      or XWPShell ring-3 logging thread.
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

    G_pLogFirst
    = G_pLogLast
    = NULL;

    // stop logging until next open
    G_bLog = LOG_ERROR;

    // force running the logger thread
    // or we'll have a zombie XWPSHELL;
    // we MUST check G_idLogBlock because
    // we also get called from ctxtFillLogBuf()
    // on errors, when G_idLogBlock is already null
    if (G_idLogBlock)
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
 *      Context: XWPSECIO_GETLOGBUF ioctl request packet
 *      from XWPShell only, that is, the ring-3 logging
 *      thread.
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
                    // keep rechecking G_bLog too because we get unblocked
                    // also when the driver is closed, and we'll end up
                    // with a zombie XWPShell in that case otherwise
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
            break;
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
    if (    (!rc)
         && (G_pLogFirst)
         && (G_bLog == LOG_ACTIVE)
       )
    {
        PLOGBUF     pFirst;

        // buffer status:
        //                                   � case 1: only one buf   � case 2: two bufs       � case 3: four bufs
        //                                   �         G_pLogFirst    �         G_pLogFirst    �         G_pLogFirst
        //                                   �         �   G_pLogLast �         �   G_pLogLast �         �   G_pLogLast
        //                                   �         �   �   pFirst �         �   �   pFirst �         �   �   pFirst
        //                                   �         �   �   �      �         �   �   �      �         �   �   �
        //                                   �         B1  B1  ?      �         B1  B2  ?      �         B1  B4  ?
        // we have logging data:             �         �   �   �      �         �   �   �      �         �   �   �
        memcpy(pLogBufR3,              //    �         �   �   �      �         �   �   �      �         �   �   �
               G_pLogFirst,            //    �         �   �   �      �         �   �   �      �         �   �   �
               G_pLogFirst->cbUsed);   //    �         �   �   �      �         �   �   �      �         �   �   �
                                       //    �         �   �   �      �         �   �   �      �         �   �   �
        // unlink this record                �         �   �   �      �         �   �   �      �         �   �   �
        pFirst = G_pLogFirst;          //    �         �   �   B1     �         �   �   B1     �         �   �   B1
                                       //    �         �   �          �         �   �          �         �   �
        if (pFirst == G_pLogLast)      //    � yes:    �   �          � no      �   �          � no      �   �
            // we only had one buffer:       �         �   �          �         �   �          �         �   �
            // unset last                    �         �   �          �         �   �          �         �   �
            G_pLogLast = NULL;         //    �         �   NUL        �         �   �          �         �   �
                                       //    �         �   �          �         �   �          �         �   �
        G_pLogFirst = G_pLogFirst->pNext; // �         NUL �          �         �   �          �         �   �
            // will be NULL if this was last �         �   �          �         B2  �          �         B2  �
                                       //    �         �   �          �         �   �          �         �   �
        utilFreeFixed(pFirst,          //    �         �   �          �         �   �          �         �   �
                      LOGBUFSIZE);  // 64K   �         �   �          �         �   �          �         �   �
        --(G_R0Status.cLogBufs);       //    �         �   �          �         �   �          �         �   �
    }                                  // final:       NUL NUL        �         B2  B2         �         B2  B4
    else
    {
        // to be safe, free all buffers
        // if we had an error (this won't call
        // ProcRun since the blockid is null)
        ctxtStopLogging();

        rc = ERROR_I24_CHAR_CALL_INTERRUPTED;
    }

    return rc;
}

/* ******************************************************************
 *
 *   Security contexts implementation
 *
 ********************************************************************/


