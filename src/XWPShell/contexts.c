
/*
 *@@sourcefile contexts.c:
 *      security contexts management for XWorkplace Security.
 *
 *      A security context is defined in XWPSECURITYCONTEXT.
 *      One security context is created for each process on
 *      the system and contains the subject handles representing
 *      the process's access rights.
 *
 *@@added V0.9.5 [umoeller]
 *@@header "security\xwpsecty.h"
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

#define  INCL_DOS
#define  INCL_DOSERRORS
#include <os2.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <io.h>

#include "setup.h"

#include "helpers\stringh.h"
#include "helpers\tree.h"               // red-black binary trees

#include "bldlevel.h"

#include "security\xwpsecty.h"

/* ******************************************************************
 *
 *   Private Definitions
 *
 ********************************************************************/

/*
 *@@ CONTEXTTREENODE:
 *
 */

typedef struct _CONTEXTTREENODE
{
    TREE        Tree;               // tree item (required for tree* to work);
                                    // ID has the PID
    XWPSECURITYCONTEXT Context;     // security context
} CONTEXTTREENODE, *PCONTEXTTREENODE;

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

/* ******************************************************************
 *
 *   Initialization
 *
 ********************************************************************/

/*
 *@@ scxtInit:
 *      initializes XWPSecurity.
 */

APIRET scxtInit(VOID)
{
    return NO_ERROR;
}

/* ******************************************************************
 *
 *   Security Contexts APIs
 *
 ********************************************************************/

/*
 *@@ scxtCreateSecurityContext:
 *      this creates a new security context
 *      for the specified process ID.
 */

APIRET scxtCreateSecurityContext(ULONG ulPID,
                                 ULONG cSubjects,           // in: no. of subjects in array
                                 HXSUBJECT *paSubjects)     // in: subjects array
{
    // @@todo call the driver

    return NO_ERROR;
}

/*
 *@@ scxtDeleteSecurityContext:
 *      deletes the security context for the
 *      specified process ID.
 *
 *      Returns:
 *      -- NO_ERROR: security context was deleted.
 *
 *      -- XWPSEC_INVALID_PID: security context doesn't exist.
 */

APIRET scxtDeleteSecurityContext(ULONG ulPID)
{
    // @@todo call the driver

    return NO_ERROR;
}

/*
 *@@ scxtFindSecurityContext:
 *      this attempts to find a security context
 *      from a process ID.
 *
 *      On input, specify XWPSECURITYCONTEXT.ulPID.
 *
 *      This returns:
 *
 *      -- NO_ERROR: rest of XWPSECURITYCONTEXT was
 *         filled with context info.
 *
 *      -- XWPSEC_INVALID_PID: security context doesn't exist.
 */

APIRET scxtFindSecurityContext(PXWPSECURITYCONTEXT pContext)
{
    // @@todo call the driver

    return NO_ERROR;
}

/*
 *@@ scxtEnumSecurityContexts:
 *      enumerates all security contexts for the
 *      specified user subject handle.
 *
 *      If hsubjUser is 0, this enumerates all
 *      security contexts.
 *
 *      This allocates memory for the contexts.
 *      Pass paContexts to scxtFreeSecurityContexts
 *      when you're done with the data.
 *
 *      *pulCount receives the count of XWPSECURITYCONTEXT
 *      structures which were allocated and returned.
 *
 *      Returns:
 *
 *      -- NO_ERROR: paContexts allocated.
 *
 *      -- XWPSEC_NO_CONTEXTS: no contexts found.
 */

APIRET scxtEnumSecurityContexts(HXSUBJECT hsubjUser,
                                PXWPSECURITYCONTEXT *ppaContexts,
                                PULONG pulCount)
{
    // @@todo call the driver

    return NO_ERROR;
}

/*
 *@@ scxtFreeSecurityContexts:
 *      frees resources allocated by scxtEnumSecurityContexts.
 */

APIRET scxtFreeSecurityContexts(PXWPSECURITYCONTEXT paContexts)
{
    if (paContexts)
        free(paContexts);

    return NO_ERROR;
}

/*
 *@@ scxtVerifyAuthority:
 *      returns NO_ERROR only if the specified process
 *      has sufficient authority to perform the
 *      given action.
 *
 *      Otherwise this returns XWPSEC_INSUFFICIENT_AUTHORITY.
 *
 *@@added V0.9.19 (2002-04-02) [umoeller]
 */

APIRET scxtVerifyAuthority(PXWPSECURITYCONTEXT pContext,
                           ULONG flActions)
{
    if (!pContext || !flActions)
        return ERROR_INVALID_PARAMETER;

    // @@todo call the driver

    return NO_ERROR;
}
