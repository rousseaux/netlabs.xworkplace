
/*
 *@@sourcefile strat_ioctl.c:
 *      PDD "ioctl" strategy routine.
 *
 *
          Sam Detweiler:

          A better way.. daemon allocates buffer, and semaphore,
          ioctls in (passing both to driver).. driver locks memory
          opens semaphore..
          dd returns thread to r3.. now you have thread at
          r3 that waits on semaphore, looks in buffer,
          does I/O request, puts response back, and posts semaphore
          which driver was waiting on (task time right!!)..
          now you have no Ioctl, no kernel serialization..

    -- that is:

    1.  Daemon allocates memory and creates an event semphoare (hevWork2To).

    2.  Daemon calls IOCtl into driver, passing memory and hevWork2To.

    3.  In IOCtl, Driver locks memory, opens hevWork2To and returns
        to ring-3 thread.

    4.  Daemon waits on hevWork2To.

    5.  When ring-0 needs something from daemon, it does the following:

        a)  Initialize a RAM semaphore to 0.

        b)  Put the address of a RAM semaphore into the daemon's buffer.

        c)  Posts hevWork2To, which unblocks the daemon thread.

        d)  Ring-0 (which is at task time) blocks on RAM semaphore
            (DevHlp_SemRequest).

    6.  Ring-3 thread unblocks, looks into data buffer, processes
        data, writes result back, releases RAM semaphore.

    7.  Driver thread unblocks.

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
#include <builtin.h>

// #include <secure.h>

#include "xwpsec32.sys\types.h"
#include "xwpsec32.sys\StackToFlat.h"
#include "xwpsec32.sys\DevHlp32.h"
#include "xwpsec32.sys\reqpkt32.h"

#include "security\ring0api.h"

#include "xwpsec32.sys\xwpsec_types.h"
#include "xwpsec32.sys\xwpsec_callbacks.h"

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

extern ULONG    G_blockidCmdPending,
                G_cCmdPending;
                    // in util_daemon.c

/* ******************************************************************
 *
 *   IOCtl implementation
 *
 ********************************************************************/

/*
 *@@ BlockForCmdPending:
 *
 *@@added V0.9.19 (2002-04-02) [umoeller]
 */

int BlockForCmdPending(VOID)
{
    int rc;

    G_cCmdPending++;
    _disable();
    rc = DevHlp32_ProcBlock((ULONG)&G_blockidCmdPending,
                            -1,     // wait forever
                            0);     // interruptible
            // corresponding ProcRun is in utilDaemonRequest

    return rc;
}

/*
 *@@ RegisterDaemon:
 *      prepares the stuff we need to interface with
 *      the ring-3 control program (CP).
 *
 *      Returns NO_ERROR or error code.
 */

int RegisterDaemon(struct reqpkt_ioctl *pRequest)   // flat ptr to request packet
{
    APIRET  rc = NO_ERROR;

    HEV     hevCallback2Store = 0;

    /* struct reqpkt_ioctl {
           struct reqhdr  header;
           unsigned char  cat;      // category
           unsigned char  func;     // function
           PTR16          parm;     // --- not used here
           PTR16          data;     // ptr to SECIOSHARED structure
           unsigned short sfn;
           unsigned short parmlen;
           unsigned short datalen;
       }; */

    if (G_fDaemonReady)
        // daemon already attached:
        rc = ERROR_I24_INVALID_PARAMETER;
    else
    {
        ULONG   pidDaemon;
        if (!(pidDaemon = utilGetTaskPID()))
            // error:
            rc = 2;     // @@todo
        else
        {
            PSECIOSHARED pSecIOSharedProcess;

            // get flat pointer to data packet, which is PSECIOSHARED
            if ((rc = DevHlp32_VirtToLin(pRequest->data,
                                         __StackToFlat(&pSecIOSharedProcess))))
                // could not thunk reqpkt:
            {
                // utilWriteLog("RegisterDaemon: DevHlp32_VirtToLin returned %d.\r\n", rc);
                rc = ERROR_I24_INVALID_PARAMETER;
            }
            else
            {
                // OK, we got pointer to PSECIOSHARED in process memory:

                // access memory buffer allocated by ring-3 daemon
                // this is in the process address space (the daemon
                // has used DosAllocMem)

                // we need to map this address from the process
                // address space to the system arena so we can
                // use this pointer later from any other process

                if (    (rc = DevHlp32_VMProcessToGlobal(VMDHPG_WRITE,   // writeable
                                                         pSecIOSharedProcess,
                                                         sizeof(SECIOSHARED),
                                                         &G_pSecIOShared))
                               != NO_ERROR)
                {
                    // utilWriteLog("RegisterDaemon: DevHlp32_VMProcessToGlobal returned %d.\r\n", rc);
                    rc = ERROR_I24_INVALID_PARAMETER;
                }
                else
                {
                    // READY!!

                    G_fDaemonReady = TRUE;

                    rc = BlockForCmdPending();
                            // calls ProcBlock, waits for some app task thread
                            // to call ProcRun; returns NO_ERROR if there's
                            // another job to be authorized
                }
            }
        }
    }

    return (rc);

    // if this is NO_ERROR, daemon will then authorize the job
    // in SECIOSHARED and call XWPSECIO_AUTHORIZED_NEXT
}

/*
 *@@ DeregisterDaemon:
 *
 */

int DeregisterDaemon(VOID)
{
    int rc = NO_ERROR;

    G_fDaemonReady = FALSE;

    if (G_pSecIOShared)
    {
        // reverse global memory
        DevHlp32_VMFree(G_pSecIOShared);
        G_pSecIOShared = NULL;
    }

    return (rc);
}

/*
 *@@ sec32_ioctl:
 *      DosDevIOCtl interface for the ring-3 access control
 *      daemon.
 *
 *      Since only one process may open the driver, we can
 *      be sure it's the ring-3 daemon which does the IOCtl
 *      here. This makes sure that no other process can
 *      attempt to implement a second access control mechanism
 *      (which would be a security hole).
 *
 *      This is called from sec32_strategy() since it's stored in
 *      driver_routing_table in sec32_strategy.c.
 *
 *      The control flow is the following:
 *
 +          Ring-3 daemon           Ring-0                  Ring-0
 +                                  daemon task time        any application task time
 +
 +      1)  calls XWPSECIO_REGISTER,
 +          passes SECIOSHARED to
 +          ring 0
 +
 +
 +      2)                          ProcBlock(G_blockidCmdPending)
 +                                  for first authorization request
 +                                  (BlockForCmdPending)
 +
 +      3)                                                  DosOpen ends up
 +                                                          in OPEN_PRE;
 +                                                          1) utilSemRequest
 +                                                          2) utilDaemonRequest
 +                                                             fills SECIOSHARED,
 +                                                             ProcRun(G_blockidCmdPending)
 +                                                             to unblock daemon,
 +                                                             ProcBlock(G_blockidCmdComplete)
 +                                                             until daemon is done
 +                                                             with authorization
 +
 +      4)                          returns from
 +                                  XWPSECIO_REGISTER
 +
 +      5)  authorizes request,
 +          stores result in
 +          SECIOSHARED.arc
 +          calls XWPSECIO_AUTHORIZED_NEXT,
 +          passes same SECIOSHARED to
 +          ring 0
 +
 +      6)                          ProcRun(G_blockidCmdComplete),
 +                                  ProcBlock(G_blockidCmdPending)
 +                                  for next authorization request
 +                                  (BlockForCmdPending)
 +
 +      7)
 +                                                          utilDaemonRequest returns;
 +                                                          OPEN_PRE calls utilSemRelease
 +                                                          and returns NO_ERROR or
 +                                                          ERROR_ACCESS_DENIED;
 +                                                          go back to 3)
 */

int sec32_ioctl(PTR16 reqpkt)
{
    int             status = 0;
    int             rc = 0;
    struct reqpkt_ioctl *pRequest;

    if (rc = DevHlp32_VirtToLin(reqpkt, __StackToFlat(&pRequest)))
        // could not thunk reqpkt:
        status |= STERR + ERROR_I24_INVALID_PARAMETER;
    else
    {
        // pRequest now has the flat pointer to the IOCtl reqpkt,
        // which is:

        /* struct reqpkt_ioctl {
               struct reqhdr  header;
               unsigned char  cat;      // category
               unsigned char  func;     // function
               PTR16          parm;
               PTR16          data;
               unsigned short sfn;
               unsigned short parmlen;
               unsigned short datalen;
           };

         On entry, the request packet has the IOCtl category
         code (cat) and function code (func) set.  Parameter Buffer
         Address and Data Buffer Address are set as virtual
         addresses.  Notice that some IOCtl functions do not
         require data or parameters to be passed.  For these
         IOCtls, the parameter and data buffer addresses can
         contain zeros.  The System File Number is a unique
         number associated with an Open request.

         If the physical device driver indicates in the function
         level of the Device Attribute field of its device header
         that it supports DosDevIOCtl2, the generic IOCtl
         request packets passed to the physical device driver
         will have two additional WORDs containing the lengths
         of the Parameter Buffer and Data Buffer.  If the
         physical device driver indicates through the function
         level that it supports DosDevIOCtl2, but the application
         issues DosDevIOCtl, the Parameter Buffer and Data
         Buffer Length fields will be set to 0.  The physical
         device driver must perform the following actions:

              Perform the requested function
              Set the status WORD in the request header

          The physical device driver is responsible for locking
          the parameter and data buffer segments and
          converting the pointers to 32-bit physical addresses, if
          necessary.  Refer to the OS/2 Control Program
          Programming Reference and the OS/2 Programming
          Guide for more detailed information on the generic
          IOCtl interface for applications (DosDevIOCtl).  Refer
          to Generic IOCtl Commands for a detailed description
          of the generic IOCtl interface for physical device
          drivers. */

        // check category

        if (pRequest->cat != IOCTL_XWPSEC)      // our category
            status |= STERR + ERROR_I24_BAD_COMMAND;
        else
        {
            // valid XWPSEC32 category:

            /* if (G_ulLogSFN == 0)
                // log file not opened yet:
                utilOpenLog(); */

            // check function
            switch (pRequest->func)
            {
                /*
                 *@@ XWPSECIO_REGISTER:
                 *      initializes the driver for the ring-3
                 *      daemon.
                 *
                 *      On input, this expects a SECIOSHARED
                 *      structure in the data packet.
                 *
                 *      Blocks the daemon thread until the
                 *      first authorization request comes in.
                 *
                 *      Before returning, this fills the SECIOSHARD
                 *      with the authorization request.
                 */

                case XWPSECIO_REGISTER:
                    status = RegisterDaemon(pRequest);
                            // this calls ProcBlock
                break;

                /*
                 *@@ XWPSECIO_AUTHORIZED_NEXT:
                 *      comes in
                 *
                 *      --  after the first XWPSECIOREGISTER
                 *          returned with the first authorization
                 *          request, to pass the authorization
                 *          error code to the thread that the
                 *          driver blocked;
                 *
                 *      --  after that, for each subsequent
                 *          request.
                 */

                case XWPSECIO_AUTHORIZED_NEXT:
                    utilDaemonDone();
                    rc = BlockForCmdPending();
                break;

                /*
                 *@@ XWPSECIO_DEREGISTER:
                 *
                 */

                case XWPSECIO_DEREGISTER:
                    // utilWriteLog("sec32_ioctl XWPSECIO_DEREGISTER:\r\n");
                    // utilWriteLogInfo();

                    // just in case we started blocking
                    // while the daemon deregistered
                    // itself:
                    utilDaemonDone();

                    status = DeregisterDaemon();
                break;
            }
        }
    }

    return (status | STDON);
}


