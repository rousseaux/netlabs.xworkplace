
/*
 *@@sourcefile xsetup.h:
 *      header file for xsetup.c (XWPSetup implementation).
 *
 *@@include #include <os2.h>
 *@@include #include "shared\notebook.h"
 *@@include #include "shared\xsetup.h"
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

#ifndef XSETUP_HEADER_INCLUDED
    #define XSETUP_HEADER_INCLUDED

    /* ******************************************************************
     *                                                                  *
     *   XWPSetup "Status" page notebook callbacks (notebook.c)         *
     *                                                                  *
     ********************************************************************/

    #ifdef NOTEBOOK_HEADER_INCLUDED
        VOID setStatusInitPage(PCREATENOTEBOOKPAGE pcnbp,
                               ULONG flFlags);

        MRESULT setStatusItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                     USHORT usItemID, USHORT usNotifyCode,
                                     ULONG ulExtra);

        VOID setStatusTimer(PCREATENOTEBOOKPAGE pcnbp,
                            ULONG ulTimer);

        VOID setFeaturesInitPage(PCREATENOTEBOOKPAGE pcnbp,
                                 ULONG flFlags);

        MRESULT setFeaturesItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                       USHORT usItemID, USHORT usNotifyCode,
                                       ULONG ulExtra);

        BOOL setFeaturesMessages(PCREATENOTEBOOKPAGE pcnbp,
                                 ULONG msg, MPARAM mp1, MPARAM mp2,
                                 MRESULT *pmrc);

        VOID setObjectsInitPage(PCREATENOTEBOOKPAGE pcnbp,
                                ULONG flFlags);

        MRESULT setObjectsItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                      USHORT usItemID, USHORT usNotifyCode,
                                      ULONG ulExtra);

        VOID setParanoiaInitPage(PCREATENOTEBOOKPAGE pcnbp,
                                 ULONG flFlags);

        MRESULT setParanoiaItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                       USHORT usItemID, USHORT usNotifyCode,
                                       ULONG ulExtra);
    #else
        #error "shared\notebook.h needs to be included before including xsetup.h".
    #endif
#endif

