
/*
 *@@sourcefile sound.h:
 *      header file for sound.c (XWPSound implementation).
 *
 *      This file is ALL new with V0.9.0.
 *
 *@@include #include <os2.h>
 *@@include #include "shared\notebook.h"
 *@@include #include "setup\sound.h"
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

#ifndef SOUND_HEADER_INCLUDED
    #define SOUND_HEADER_INCLUDED

    /* ******************************************************************
     *                                                                  *
     *   XFldDisk notebook callbacks (notebook.c)                       *
     *                                                                  *
     ********************************************************************/

    #ifdef NOTEBOOK_HEADER_INCLUDED
        VOID sndSoundsInitPage(PCREATENOTEBOOKPAGE pcnbp,
                                ULONG flFlags);

        MRESULT sndSoundsItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                      USHORT usItemID, USHORT usNotifyCode,
                                      ULONG ulExtra);
    #else
        #error "shared\notebook.h needs to be included before including disk.h".
    #endif
#endif


