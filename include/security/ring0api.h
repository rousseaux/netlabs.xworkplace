
/*
 *@@sourcefile ring0api.h:
 *      public definitions for interfacing XWPSEC32.SYS.
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

#if __cplusplus
extern "C" {
#endif

#ifndef RING0API_HEADER_INCLUDED
    #define RING0API_HEADER_INCLUDED

    /* ******************************************************************
     *
     *   IOCtl
     *
     ********************************************************************/

    // category for IOCtl's to xwpsec32.sys
    #define IOCTL_XWPSEC            0x8F

    // IOCtl functions

    /*
     *@@ SECIOREGISTER:
     *      structure to be passed with the XWPSECIO_REGISTER
     *      DosDevIOCtl function code to XWPSEC32.SYS.
     *
     *      With this structure, the ring-3 shell must
     *      pass down an array of process IDs that were
     *      running already at the time the shell was
     *      started. These PIDs will then be treated as
     *      trusted processes by the driver.
     */

    typedef struct _PROCESSLIST
    {
        ULONG           cbStruct;
        ULONG           cTrusted;
        ULONG           apidTrusted[1];
    } PROCESSLIST, *PPROCESSLIST;

    #define XWPSECIO_REGISTER           0x50

    #define XWPSECIO_DEREGISTER         0x51

    #pragma pack()

#endif

#if __cplusplus
}
#endif

