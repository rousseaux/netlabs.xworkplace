
/*
 *@@sourcefile strat_init_complete.c:
 *      PDD "init_complete" strategy routine.
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
#define INCL_DOS
#define INCL_NOPMAPI
#include <os2.h>
// #include <secure.h>

#include "xwpsec32.sys\types.h"
#include "xwpsec32.sys\reqpkt32.h"

#include "xwpsec32.sys\xwpsec_callbacks.h"

/*
 *@@ sec32_init_complete:
 *      this strategy command informs the driver that all PDD's
 *      have been loaded and it's OK to start inter-device-driver
 *      communications (IDC).
 *
 *      This is called from sec32_strategy() since it's stored in
 *      driver_routing_table in sec32_strategy.c.
 */

int sec32_init_complete(PTR16 reqpkt)
{
    /*
     * no special processing yet
     */
    return STDON;
}

