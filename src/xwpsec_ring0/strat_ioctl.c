
/*
 *@@sourcefile strat_ioctl.c:
 *      strategy routines for PDD "open", "ioctl", and "close"
 *      commands, plus the implementation for the various
 *      IOCtl commands supported by the driver.
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

static ULONG    G_open_count = 0;

PPROCESSLIST    G_pplTrusted = NULL;           // global list of trusted processes (fixed mem)

/* ******************************************************************
 *
 *   IOCtl implementation
 *
 ********************************************************************/

/*
 *@@ RegisterDaemon:
 *      implementation for XWPSECIO_REGISTER in sec32_ioctl.
 *
 *      If this returns NO_ERROR, access control is ENABLED
 *      globally.
 *
 *@@added V1.0.1 (2003-01-10) [umoeller]
 */

APIRET RegisterDaemon(struct reqpkt_ioctl *pRequest)   // flat ptr to request packet
{
    PPROCESSLIST pplShell;

    // get flat pointer to data packet, which is PROCESSLIST
    if (DevHlp32_VirtToLin(pRequest->data,
                           __StackToFlat(&pplShell)))
        return ERROR_I24_INVALID_PARAMETER;

    // make a copy of the shell's list of trusted processes
    if (DevHlp32_VMAlloc(pplShell->cbStruct,   // Length
                         VMDHA_NOPHYSADDR,     // PhysAddr == -1
                         VMDHA_FIXED,          // Flags
                         (PVOID*)&G_pplTrusted))
        return ERROR_I24_GEN_FAILURE;

    _memcpy(G_pplTrusted,
            pplShell,
            pplShell->cbStruct);

    // now get the caller's (shell's) PID  and store that,
    // because we need that for extra privileges of the shell;
    // also once G_pidShell is != null,
    // ACCESS CONTROL IS ENABLED GLOBALLY
    if (!(G_pidShell = utilGetTaskPID()))
        // error:
        return ERROR_I24_GEN_FAILURE;

    // alright from now on we authenticate!

    return NO_ERROR;
}

/*
 *@@ DeregisterDaemon:
 *      implementation for XWPSECIO_DEREGISTER in sec32_ioctl.
 *
 *      After this, access control is DISABLED.
 *
 *@@added V1.0.1 (2003-01-10) [umoeller]
 */

VOID DeregisterDaemon(VOID)
{
    if (G_pplTrusted)
    {
        DevHlp32_VMFree(G_pplTrusted);
        G_pplTrusted = NULL;
    }

    G_pidShell = 0;
}

/* ******************************************************************
 *
 *   Strategy commands
 *
 ********************************************************************/

/*
 *@@ sec32_open:
 *      this strategy command opens the device as a result of
 *      a valid DosOpen call on the driver.
 *
 *      This is called from sec32_strategy() since it's stored in
 *      driver_routing_table in sec32_strategy.c.
 *
 *      The System File Number is a unique number associated with
 *      an open request. The physical device driver must perform
 *      the following actions:
 *
 *      --  Perform the requested function.
 *
 *      --  Set the status WORD in the request header.
 *
 *      Character device drivers can use Open/Close requests
 *      to correlate using their devices with application activity.
 *      For instance, the physical device driver can
 *      use the OPEN as an indicator to send an initialization
 *      string to its device. The physical device driver can then
 *      increase a reference count for every OPEN and decrease the
 *      reference count for every CLOSE.  When the count goes to 0,
 *      the physical device driver can flush its buffers.
 */

int sec32_open(PTR16 reqpkt)
{
    // no need for the request packet here, it only has
    // the SFN number for us...

    // we allow only one open at a time, which should be the
    // ring-3 control program, so check if we've been opened
    // already
    if (G_open_count)
        return STDON | STERR | ERROR_I24_DEVICE_IN_USE;

    // success:
    // set open count to one (will be decreased by "close"
    // strategy command again)
    ++G_open_count;

    return STDON;
}

/*
 *@@ sec32_ioctl:
 *      implementation for the "IOCtl" strategy call, which is
 *      the interface for the ring-3 shell.
 *      Gets called from sec32_strategy() since it's stored in
 *      driver_routing_table in sec32_strategy.c.
 *
 *      Since only one process may open the driver, we can
 *      be sure it's the ring-3 daemon which does the IOCtl
 *      here. This makes sure that no other process can
 *      attempt to implement a second access control mechanism
 *      (which would be a security hole).
 *
 *      The IOCtl request packet is:
 *
 +      struct reqpkt_ioctl {
 +             struct reqhdr  header;
 +             unsigned char  cat;      // category
 +             unsigned char  func;     // function
 +             PTR16          parm;
 +             PTR16          data;
 +             unsigned short sfn;
 +             unsigned short parmlen;
 +             unsigned short datalen;
 +         };
 */

int sec32_ioctl(PTR16 reqpkt)
{
    APIRET  rc;
    struct reqpkt_ioctl *pRequest;

    if (DevHlp32_VirtToLin(reqpkt,
                           __StackToFlat(&pRequest)))
        // could not thunk reqpkt:
        return STDON | STERR | ERROR_I24_INVALID_PARAMETER;

    // check category
    if (pRequest->cat != IOCTL_XWPSEC)      // our category
        return STDON | STERR | ERROR_I24_BAD_COMMAND;

    // check function
    switch (pRequest->func)
    {
        case XWPSECIO_REGISTER:
            if (rc = RegisterDaemon(pRequest))
                return STDON | STERR | rc;
        break;

        case XWPSECIO_DEREGISTER:
            DeregisterDaemon();
        break;

        default:
            return STDON | STERR | ERROR_I24_BAD_COMMAND;
    }

    return STDON;
}

/*
 *@@ sec32_close:
 *      implementation for the "close" strategy call, which is
 *      the interface for the ring-3 shell.
 *      Gets called from sec32_strategy() since it's stored in
 *      driver_routing_table in sec32_strategy.c.
 */

int sec32_close(PTR16 reqpkt)
{
    // no need for the request packet here, it only has
    // the SFN number for us...

    // to be safe, call deregister daemon first,
    // just in case the shell crashed and didn't
    // call the ioctl properly
    DeregisterDaemon();

    --G_open_count;

    return STDON;
}

