
/*
 *@@sourcefile program.h:
 *      header file for program.c (program implementation).
 *
 *      This file is ALL new with V0.9.6.
 *
 *@@include #define INCL_WINPROGRAMLIST     // for progStartApp
 *@@include #include <os2.h>
 *@@include #include <wpfsys.h>             // for progOpenProgram
 *@@include #include "filesys\program.h"
 */

/*
 *      Copyright (C) 2000 Ulrich M”ller.
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

#ifndef PROGRAM_HEADER_INCLUDED
    #define PROGRAM_HEADER_INCLUDED

    /* ******************************************************************
     *
     *   Running programs database
     *
     ********************************************************************/

    BOOL progStoreRunningApp(HAPP happ,
                             WPObject *pObjEmphasis);

    BOOL progAppTerminateNotify(HAPP happ);

    BOOL progRunningAppDestroyed(WPObject *pObjEmphasis);

    /* ******************************************************************
     *
     *   Run programs
     *
     ********************************************************************/

    #ifdef INCL_WINPROGRAMLIST
    HAPP progStartApp(const PROGDETAILS *pcProgDetails,
                      const char *pcszParameters,
                      const char *pcszStartupDir);
    #endif

    #ifdef SOM_WPDataFile_h
    HAPP progOpenProgram(WPObject *pProgObject,
                         WPFileSystem *pArgDataFile);
    #endif
#endif
