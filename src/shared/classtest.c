
/*
 *@@sourcefile classtest.c:
 *      just a stupid collection of functions which test
 *      if an object is of a certain class so we won't
 *      have to include all the WPS headers all the time.
 *
 *      There are many places in the code which simply
 *      do a _somIsA(somSelf, _XXX) check, and it's just
 *      a waste of compilation time to include a full
 *      WPS header just for having the class object
 *      definition.
 *
 *      Function prefix for this file:
 *      --  cts*
 *
 *@@header "shared\classtest.h"
 *@@added V0.9.19 (2002-06-15) [umoeller]
 */

/*
 *      Copyright (C) 2002 Ulrich M”ller.
 *      This file is part of the XWorkplace source package.
 *      XWorkplace is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published
 *      by the Free Software Foundation, in version 2 as it comes in the
 *      "COPYING" file of the XWorkplace main distribution.
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 */

#pragma strings(readonly)

/*
 *  Suggested #include order:
 *  1)  os2.h
 *  2)  C library headers
 *  3)  setup.h (code generation and debugging options)
 *  4)  headers in helpers\
 *  5)  at least one SOM implementation header (*.ih)
 *  6)  dlgids.h, headers in shared\ (as needed)
 *  7)  headers in implementation dirs (e.g. filesys\, as needed)
 *  8)  #pragma hdrstop and then more SOM headers which crash with precompiled headers
 */

#define INCL_WINPROGRAMLIST     // needed for PROGDETAILS, wppgm.h
#include <os2.h>

// C library headers
#include <stdio.h>
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers

// SOM headers which don't crash with prec. header files

// XWorkplace implementation headers
#pragma hdrstop                     // VAC++ keeps crashing otherwise
#include <wpshadow.h>                   // WPShadow
#include <wptrans.h>                    // WPTransient
#include <wpicon.h>                     // WPIcon
#include <wpptr.h>                      // WPPointer
#include <wpcmdf.h>                     // WPCommandFile
#include <wprootf.h>                    // WPRootFolder
#include <wpshdir.h>                    // WPSharedDir

#include "shared\classtest.h"           // some cheap funcs for WPS class checks

/*
 *@@ ctsIsAbstract:
 *
 */

BOOL ctsIsAbstract(WPObject *somSelf)
{
    return _somIsA(somSelf, _WPAbstract);
}

/*
 *@@ ctsIsShadow:
 *
 */

BOOL ctsIsShadow(WPObject *somSelf)
{
    return _somIsA(somSelf, _WPShadow);
}

/*
 *@@ ctsIsTransient:
 *      returns TRUE if somSelf is a WPTransient.
 */

BOOL ctsIsTransient(WPObject *somSelf)
{
    return _somIsA(somSelf, _WPTransient);
}

/*
 *@@ ctsIsMinWin:
 *      returns TRUE if somSelf is a WPMinWindow,
 *      a transient mini-window in the minimized
 *      window viewer.
 *
 *@@added V0.9.20 (2002-07-12) [umoeller]
 */

BOOL ctsIsMinWin(WPObject *somSelf)
{
    // WPMinWindow is undocumented, so check class name
    return (!strcmp(_somGetClassName(somSelf), "WPMinWindow"));
}

/*
 *@@ ctsIsIcon:
 *
 */

BOOL ctsIsIcon(WPObject *somSelf)
{
    return _somIsA(somSelf, _WPIcon);
}

/*
 *@@ ctsIsPointer:
 *
 */

BOOL ctsIsPointer(WPObject *somSelf)
{
    return _somIsA(somSelf, _WPPointer);
}

/*
 *@@ ctsIsCommandFile:
 *
 */

BOOL ctsIsCommandFile(WPObject *somSelf)
{
    return _somIsA(somSelf, _WPCommandFile);
}

/*
 *@@ ctsIsRootFolder:
 *
 */

BOOL ctsIsRootFolder(WPObject *somSelf)
{
    return _somIsA(somSelf, _WPRootFolder);
}

/*
 *@@ ctsIsSharedDir:
 *
 */

BOOL ctsIsSharedDir(WPObject *somSelf)
{
    return _somIsA(somSelf, _WPSharedDir);
}

/*
 *@@ ctsDescendedFromSharedDir:
 *
 */

BOOL ctsDescendedFromSharedDir(SOMClass *pClassObject)
{
    return _somDescendedFrom(pClassObject, _WPSharedDir);
}


