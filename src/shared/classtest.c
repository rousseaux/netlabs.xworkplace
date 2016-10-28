
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
 *      Copyright (C) 2002-2008 Ulrich M”ller.
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
#include <wppgm.h>                      // WPProgram
#include <wptrans.h>                    // WPTransient
#include <wpicon.h>                     // WPIcon
#include <wpptr.h>                      // WPPointer
#include <wpcmdf.h>                     // WPCommandFile
#include <wprootf.h>                    // WPRootFolder
#include <wpserver.h>                   // WPServer
#include <wpshdir.h>                    // WPSharedDir
#include <wpdisk.h>                     // WPDisk
#include <wpnetgrp.h>                   // WPNetgrp

#include "shared\classtest.h"           // some cheap funcs for WPS class checks
#include "filesys\object.h"             // XFldObject implementation

/*
 *@@ ctsSetClassFlags:
 *      called from XFldObject::wpObjectReady to set
 *      flags in the object instance data for speedier
 *      testing later.
 *
 *@@added V0.9.20 (2002-08-04) [umoeller]
 */

VOID ctsSetClassFlags(WPObject *somSelf,
                      PULONG pfl)
{
    if (_somIsA(somSelf, _WPFileSystem))
    {
        *pfl = OBJFL_WPFILESYSTEM;
        if (_somIsA(somSelf, _WPFolder))
            *pfl |= OBJFL_WPFOLDER;
        else if (_somIsA(somSelf, _WPDataFile))
            *pfl |= OBJFL_WPDATAFILE;
    }
    else if (_somIsA(somSelf, _WPAbstract))
    {
        *pfl = OBJFL_WPABSTRACT;
        if (ctsIsShadow(somSelf))
            *pfl |= OBJFL_WPSHADOW;
        else if (_somIsA(somSelf, _WPProgram))
            *pfl |= OBJFL_WPPROGRAM;
    }
}

/*
 *@@ ctsIsAbstract:
 *      returns TRUE if somSelf is abstract.
 */

BOOL ctsIsAbstract(WPObject *somSelf)
{
    return _somIsA(somSelf, _WPAbstract);
}

/*
 *@@ ctsIsShadow:
 *      returns TRUE if somSelf is a shadow
 *      (of the WPShadow class).
 */

BOOL ctsIsShadow(WPObject *somSelf)
{
    return _somIsA(somSelf, _WPShadow);
}

/*
 *@@ ctsIsTransient:
 *      returns TRUE if somSelf is transient
 *      (of the WPTransient class).
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
 *      returns TRUE if somSelf is an icon data
 *      file (of the WPIcon class).
 */

BOOL ctsIsIcon(WPObject *somSelf)
{
    return _somIsA(somSelf, _WPIcon);
}

/*
 *@@ ctsIsPointer:
 *      returns TRUE if somSelf is an pointer data
 *      file (of the WPPointer class).
 */

BOOL ctsIsPointer(WPObject *somSelf)
{
    return _somIsA(somSelf, _WPPointer);
}

/*
 *@@ ctsIsCommandFile:
 *      returns TRUE if somSelf is a command file
 *      (of the WPCommandFile class, which in turn
 *      descends from WPProgramFile).
 */

BOOL ctsIsCommandFile(WPObject *somSelf)
{
    return _somIsA(somSelf, _WPCommandFile);
}

/*
 *@@ ctsIsRootFolder:
 *      returns TRUE if somSelf is a root folder
 *      (of the WPRootFolder class).
 */

BOOL ctsIsRootFolder(WPObject *somSelf)
{
    return _somIsA(somSelf, _WPRootFolder);
}

/*
 *@@ ctsIsSharedDir:
 *      returns TRUE if somSelf is a "shared directory"
 *      (of the WPSharedDir class).
 *
 *      WPSharedDir objects function as root folders for
 *      remote folders and appear below WPServer objects.
 */

BOOL ctsIsSharedDir(WPObject *somSelf)
{
    return _somIsA(somSelf, _WPSharedDir);
}

/*
 *@@ ctsIsServer:
 *      returns TRUE if somSelf is a server object
 *      (of the WPServer class).
 *
 *      WPServer objects function like WPDisks for
 *      remote objects and appear only in WPNetGroup.
 *
 *@@added V0.9.20 (2002-07-31) [umoeller]
 */

BOOL ctsIsServer(WPObject *somSelf)
{
    return _somIsA(somSelf, _WPServer);
}

/*
 *@@ ctsIsNetgrp:
 *      returns TRUE if somSelf is a network group object
 *      (of the WPNetgrp class).
 *
 *      A WPNetgrp object contains WPServer objects
 *      and appears in the WPNetwork folder.
 *
 *@@added V1.0.2 (2003-10-14) [pr]: @@fixes 514
 */

BOOL ctsIsNetgrp(WPObject *somSelf)
{
    return _somIsA(somSelf, _WPNetgrp);
}

/*
 *@@ ctsDescendedFromSharedDir:
 *
 */

BOOL ctsDescendedFromSharedDir(SOMClass *pClassObject)
{
    return _somDescendedFrom(pClassObject, _WPSharedDir);
}

/*
 *@@ ctsIsDisk:
 *      returns TRUE if somSelf is a Disk
 *      (of the WPDisk class).
 *
 *      WPDisk objects function as root folders.
 *
 *@@added V1.0.0 (2002-09-23) [pr]
 */

BOOL ctsIsDisk(WPObject *somSelf)
{
    return _somIsA(somSelf, _WPDisk);
}

/*
 *@@ ctsIsPrinter:
 *      returns TRUE if somSelf is a Printer (of the WPPrinter class).
 *
 *      Do NOT use _WPPrinter anywhere in XWP. This causes a reference
 *      to WPPRINT.DLL to be linked into XFLDR.DLL.
 *      Due to more horrendous IBM bugs, WPPRINT.DLL does a significant
 *      amount of processing in its DLL_InitTerm function which results
 *      in SOM calling its instance methods.
 *      Because XWP overrides some of these, code in XFLDR.DLL gets
 *      called before its own DLL_InitTerm function has been called and
 *      the C RTL is therefore not initialised.
 *      This results in crashes in the DLL_InitTerm of WPPRINT.DLL and
 *      the PMSHELL exception handler can't cope with that. The process
 *      just hangs.
 *      Resolving the class object as below prevents this reference to
 *      WPPRINT.DLL and everything stays working.
 *      Jeez, how many hours and reboots did it take to work this out?
 *
 *@@added V1.0.6 (2006-09-24) [pr]
 */

BOOL ctsIsPrinter(WPObject *somSelf)
{
    somId       WPPrinterId = somIdFromString("WPPrinter");
    SOMClass    *WPPrinterClass = _somClassFromId(SOMClassMgrObject,
                                                  WPPrinterId);

    SOMFree(WPPrinterId);
    if (WPPrinterClass)
        return _somIsA(somSelf, WPPrinterClass);
    else
        return FALSE;
}

/*
 *@@ ctsIsTrashable:
 *      returns FALSE if somSelf is a Transient, a Printer, a Network folder
 *      or a Server otherwise returns TRUE.
 *
 *@@added V1.0.8 (2008-01-05) [pr]
 */

BOOL ctsIsTrashable(WPObject *somSelf)
{
    if (   ctsIsTransient(somSelf)
        || ctsIsPrinter(somSelf)
        || ctsIsNetgrp(somSelf)
        || ctsIsServer(somSelf)
       )
        return FALSE;
    else
        return TRUE;
}

