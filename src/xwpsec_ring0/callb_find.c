
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

#include "xwpsec32.sys\xwpsec_callbacks.h"

/*
 *@@ FINDFIRST:
 *      SES kernel hook for FINDFIRST.
 *
 *      As with all our hooks, this is stored in G_SecurityHooks
 *      (sec32_callbacks.c) force the OS/2 kernel to call us for
 *      each such event.
 *
 *      Required privileges:
 *
 *      --  XWPACCESS_READ on the directory.
 */

ULONG FINDFIRST(PFINDPARMS pParms)
{
    return NO_ERROR;
}


