
/*
 *@@sourcefile pg_settings.c:
 *      XPager configuration stuff.
 *
 *      See pg_control.c for an introduction to XPager.
 */

/*
 *      Copyright (C) 2000 Ulrich M”ller.
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, in version 2 as it comes in the COPYING
 *      file of the XWorkplace main distribution.
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 */

#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#define INCL_DOSEXCEPTIONS
#define INCL_DOSERRORS

#define INCL_WININPUT
#define INCL_WINSYS
#define INCL_WINSHELLDATA
#include <os2.h>

#include <stdio.h>
#include <process.h>

#define DONT_REPLACE_MALLOC         // in case mem debug is enabled
#include "setup.h"                      // code generation and debugging options

#include "helpers\gpih.h"               // GPI helper routines
#include "helpers\regexp.h"             // extended regular expressions

#include "xwpapi.h"                     // public XWorkplace definitions

#include "hook\xwphook.h"
#include "hook\hook_private.h"
#include "hook\xwpdaemn.h"              // XPager and daemon declarations


