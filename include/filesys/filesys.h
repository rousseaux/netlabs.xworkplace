
/*
 *@@sourcefile filesys.h:
 *      header file for filesys.c (extended file types implementation).
 *
 *      This file is ALL new with V0.9.0.
 *
 *@@include #include <os2.h>
 *@@include #include "shared\notebook.h"
 *@@include #include "filesys\filesys.h"
 */

/*
 *      Copyright (C) 1997-99 Ulrich M”ller.
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

#ifndef FILESYS_HEADER_INCLUDED
    #define FILESYS_HEADER_INCLUDED

    /* ******************************************************************
     *                                                                  *
     *   "File" pages replacement in WPDataFile/WPFolder                *
     *                                                                  *
     ********************************************************************/

    #ifdef NOTEBOOK_HEADER_INCLUDED
        VOID fsysFileInitPage(PCREATENOTEBOOKPAGE pcnbp,
                              ULONG flFlags);

        MRESULT fsysFileItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                    USHORT usItemID,
                                    USHORT usNotifyCode,
                                    ULONG ulExtra);

    /* ******************************************************************
     *                                                                  *
     *   XFldProgramFile notebook callbacks (notebook.c)                *
     *                                                                  *
     ********************************************************************/

        VOID fsysProgramInitPage(PCREATENOTEBOOKPAGE pcnbp,
                                 ULONG flFlags);
    #else
        #error "shared\notebook.h needs to be included before including filesys.h".
    #endif

#endif


