
/*
 *@@sourcefile strat_open.c:
 *      PDD "open" strategy routine.
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
// #include <secure.h>

#include "xwpsec32.sys\types.h"
#include "xwpsec32.sys\StackToFlat.h"
#include "xwpsec32.sys\DevHlp32.h"
#include "xwpsec32.sys\reqpkt32.h"

#include "xwpsec32.sys\xwpsec_types.h"
#include "xwpsec32.sys\xwpsec_callbacks.h"

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
    int             status = 0;
    int             rc = 0;
    struct reqpkt_open *pRequest;

    if (    (rc = DevHlp32_VirtToLin(reqpkt, __StackToFlat(&pRequest)))
            != NO_ERROR)
        // could not thunk reqpkt:
        status |= STERR + ERROR_I24_INVALID_PARAMETER;
    else
    {
        // we now have a valid flat pointer to the "open"
        // request packet:

        // we allow only one open at a time, which should be the
        // ring-3 control program, so check if we've been opened
        // already
        if (G_open_count)
            // yes: return "device busy"
            status |= STERR + ERROR_I24_DEVICE_IN_USE;
        else
        {
            // get caller's PID out of the local info segment
            // and store that, because we need that for extra
            // privileges of the caller
            G_pidRing3Daemon = utilGetTaskPID();

            if (!G_pidRing3Daemon)
                // error:
                status |= STERR + ERROR_I24_GEN_FAILURE;
            else
            {
                // success:
                // set open count to one (will be decreased by "close"
                // strategy command again)
                G_open_count++;

                // audit log file specified?
                /* if (G_szLogFile[0])
                {
                    if (G_ulLogSFN == 0)
                        // log file not opened yet:
                        utilOpenLog();

                    // utilWriteLog("sec32_open:\r\n");
                    utilWriteLogInfo();
                } */
            }
        }
    }

    return (status | STDON);
}

