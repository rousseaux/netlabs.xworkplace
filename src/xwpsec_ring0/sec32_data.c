
/*
 *@@sourcefile sec32_data.c:
 *      global variables for the data segment.
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

#define INCL_DOSERRORS
#define INCL_NOPMAPI
#include <os2.h>

#include "xwpsec32.sys\types.h"
#include "xwpsec32.sys\xwpsec_types.h"

int             _dllentry = 0;

extern PVOID    G_pSecIOShared = 0;

extern HEV      G_hevCallback = 0;
    // event semaphore created by ring-3 daemon; if this is != 0,
    // the daemon has contacted the DD via IOCtl, and access control
    // callbacks are enabled

extern ULONG    G_hmtxBufferLocked = 0;
    // pseudo-mutex semaphore (for utilSemRequest) for serializing
    // access to our buffers...

extern SEL      G_aGDTSels[2];

extern int      G_open_count = 0;

extern ULONG    G_pidRing3Daemon = 0;


