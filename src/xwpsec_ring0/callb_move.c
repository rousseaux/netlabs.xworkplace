
/*
 *@@sourcefile callb_move.c:
 *      SES kernel hook code.
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

#include <string.h>

#include "xwpsec32.sys\types.h"
#include "xwpsec32.sys\StackToFlat.h"
#include "xwpsec32.sys\devhlp32.h"

#include "security\ring0api.h"

#include "xwpsec32.sys\xwpsec_types.h"
#include "xwpsec32.sys\xwpsec_callbacks.h"

/*
 *@@ MOVE_PRE:
 *      SES kernel hook for MOVE_PRE (move or rename).
 *      This gets called from the OS/2 kernel to give
 *      the ISS a chance to authorize this event.
 *
 *      As with DosMove, this will only get called when
 *      source and dest are on same volume
 *
 *      This callback is stored in G_SecurityHooks in
 *      sec32_callbacks.c to hook the kernel.
 *
 *@@added V0.9.2 (2000-03-13) [umoeller]
 */

ULONG CallType MOVE_PRE(PSZ pszNewPath,
                        PSZ pszOldPath)
{
    return NO_ERROR;
}

/*
 *@@ MOVE_POST:
 *      SES kernel hook for MOVE_POST.
 *      This gets called from the OS/2 kernel to notify
 *      the ISS of this event. We need this so that
 *      entries in the ACLDB can be updated.
 *
 *      This callback is stored in G_SecurityHooks in
 *      sec32_callbacks.c to hook the kernel.
 *
 *      Currently disabled. @@todo
 *
 *@@added V0.9.2 (2000-03-13) [umoeller]
 */

VOID CallType MOVE_POST(PSZ pszNewPath,
                        PSZ pszOldPath,
                        ULONG RC)
{
}


