
/*
 *@@sourcefile xsetup.h:
 *      header file for xsetup.c (XWPSetup implementation).
 *
 *@@include #include <os2.h>
 *@@include #include "shared\notebook.h"
 *@@include #include "shared\xsetup.h"
 */

/*
 *      Copyright (C) 1997-2002 Ulrich M”ller.
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

    BOOL setCreateStandardObject(HWND hwndOwner,
                                 USHORT usMenuID,
                                 BOOL fConfirm,
                                 BOOL fXWPObject);

    /* ******************************************************************
     *
     *   XWPSetup "Status" page notebook callbacks (notebook.c)
     *
     ********************************************************************/

    #ifdef NOTEBOOK_HEADER_INCLUDED
        VOID XWPENTRY setLogoInitPage(PNOTEBOOKPAGE pnbp,
                                      ULONG flFlags);

        BOOL setLogoMessages(PNOTEBOOKPAGE pnbp,
                             ULONG msg, MPARAM mp1, MPARAM mp2,
                             MRESULT *pmrc);

        VOID XWPENTRY setThreadsInitPage(PNOTEBOOKPAGE pnbp,
                                         ULONG flFlags);

        VOID XWPENTRY setStatusInitPage(PNOTEBOOKPAGE pnbp,
                                        ULONG flFlags);

        MRESULT XWPENTRY setStatusItemChanged(PNOTEBOOKPAGE pnbp,
                                     ULONG ulItemID, USHORT usNotifyCode,
                                     ULONG ulExtra);

        VOID setStatusTimer(PNOTEBOOKPAGE pnbp,
                            ULONG ulTimer);

        VOID XWPENTRY setFeaturesInitPage(PNOTEBOOKPAGE pnbp,
                                          ULONG flFlags);

        MRESULT XWPENTRY setFeaturesItemChanged(PNOTEBOOKPAGE pnbp,
                                       ULONG ulItemID, USHORT usNotifyCode,
                                       ULONG ulExtra);

        BOOL setFeaturesMessages(PNOTEBOOKPAGE pnbp,
                                 ULONG msg, MPARAM mp1, MPARAM mp2,
                                 MRESULT *pmrc);

        VOID XWPENTRY setObjectsInitPage(PNOTEBOOKPAGE pnbp,
                                         ULONG flFlags);

        MRESULT XWPENTRY setObjectsItemChanged(PNOTEBOOKPAGE pnbp,
                                      ULONG ulItemID, USHORT usNotifyCode,
                                      ULONG ulExtra);

        VOID XWPENTRY setParanoiaInitPage(PNOTEBOOKPAGE pnbp,
                                          ULONG flFlags);

        MRESULT XWPENTRY setParanoiaItemChanged(PNOTEBOOKPAGE pnbp,
                                       ULONG ulItemID, USHORT usNotifyCode,
                                       ULONG ulExtra);

    #endif
#endif

