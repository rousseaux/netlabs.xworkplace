
/*
 *@@sourcefile init.h:
 *      header file for init.c. See remarks there.
 *
 *@@include #include <os2.h>
 *@@include #include "shared\init.h"
 */

/*
 *      Copyright (C) 1997-2001 Ulrich M”ller.
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, in version 2 as it comes in the COPYING
 *      file of the XWorkplace main distribution.
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 */

#ifndef INIT_HEADER_INCLUDED
    #define INIT_HEADER_INCLUDED

    /* ******************************************************************
     *
     *   XWorkplace initialization
     *
     ********************************************************************/

    VOID initMain(VOID);

    BOOL initRepairDesktopIfBroken(VOID);

    VOID initDesktopPopulated(VOID);

#endif
