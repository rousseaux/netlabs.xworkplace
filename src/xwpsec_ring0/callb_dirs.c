
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
 *@@ MAKEDIR:
 *      SES kernel hook for MAKEDIR.
 *      This gets called from the OS/2 kernel to give
 *      the ISS a chance to authorize this event.
 *
 *      This callback is stored in G_SecurityHooks in
 *      sec32_callbacks.c to hook the kernel.
 */

ULONG MAKEDIR(PSZ pszPath)
{
    int rc = NO_ERROR;

    if (utilNeedsVerify())
    {
    }

    return (rc);
}

/*
 *@@ CHANGEDIR:
 *      SES kernel hook for CHANGEDIR.
 *      This gets called from the OS/2 kernel to give
 *      the ISS a chance to authorize this event.
 *
 *      This callback is stored in G_SecurityHooks in
 *      sec32_callbacks.c to hook the kernel.
 */

ULONG CHANGEDIR(PSZ pszPath)
{
    int rc = NO_ERROR;

    if (utilNeedsVerify())
    {
    }

    return (rc);
}

/*
 *@@ REMOVEDIR:
 *      SES kernel hook for REMOVEDIR.
 *      This gets called from the OS/2 kernel to give
 *      the ISS a chance to authorize this event.
 *
 *      Um, we have a problem here. There is no POST call
 *      for REMOVEDIR so we cannot find out whether REMOVEDIR
 *      failed and if not, remove the ACLDB entry.
 *
 *      This callback is stored in G_SecurityHooks in
 *      sec32_callbacks.c to hook the kernel.
 */

ULONG REMOVEDIR(PSZ pszPath)
{
    int rc = NO_ERROR;

    if (utilNeedsVerify())
    {
    }

    return (rc);
}


