
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
     *   Helpers
     *
     ********************************************************************/

    BOOL progQuerySetup(WPObject *somSelf,
                        PVOID pstrSetup);

    ULONG progIsProgramOrProgramFile(WPObject *somSelf);

    #ifdef INCL_WINPROGRAMLIST
        PPROGDETAILS progQueryDetails(WPObject *pProgObject);
    #endif

    /* ******************************************************************
     *
     *   Running programs database
     *
     ********************************************************************/

    #ifdef SOM_WPDataFile_h
    BOOL progStoreRunningApp(WPObject *pProgram,
                             WPFileSystem *pArgDataFile,
                             HAPP happ,
                             ULONG ulMenuID);
    #endif

    BOOL progAppTerminateNotify(HAPP happ);

    BOOL progRunningAppDestroyed(WPObject *pObjEmphasis);

    /* ******************************************************************
     *
     *   Run programs
     *
     ********************************************************************/

    APIRET progOpenProgramThread1(PVOID pvData);

    #ifdef SOM_WPDataFile_h
    APIRET progOpenProgram(WPObject *pProgObject,
                           WPFileSystem *pArgDataFile,
                           ULONG ulMenuID,
                           HAPP *phapp);
    #endif
#endif
