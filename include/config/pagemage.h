
/*
 *@@sourcefile pagemage.h:
 *      header file for pagemage.c (PageMage XWPScreen interface).
 *
 *      This file is ALL new with V0.9.3.
 *
 *@@added V0.9.3 (2000-04-08) [umoeller]
 *@@include #include <os2.h>
 *@@include #include "shared\notebook.h"
 *@@include #include "config\pagemage.h"
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

#ifndef CONFIG_HEADER_INCLUDED
    #define CONFIG_HEADER_INCLUDED

// #ifdef __PAGEMAGE__

    /* ******************************************************************
     *                                                                  *
     *   PageMage (XWPScreen) notebook functions (notebook.c)           *
     *                                                                  *
     ********************************************************************/

    #ifdef NOTEBOOK_HEADER_INCLUDED
        VOID XWPENTRY pgmiPageMageGeneralInitPage(PCREATENOTEBOOKPAGE pcnbp,
                                                  ULONG flFlags);

        MRESULT XWPENTRY pgmiPageMageGeneralItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                               ULONG ulItemID, USHORT usNotifyCode,
                                               ULONG ulExtra);

        VOID XWPENTRY pgmiPageMageStickyInitPage(PCREATENOTEBOOKPAGE pcnbp,
                                                 ULONG flFlags);

        MRESULT XWPENTRY pgmiPageMageStickyItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                              ULONG ulItemID, USHORT usNotifyCode,
                                              ULONG ulExtra);

        VOID XWPENTRY pgmiPageMageColorsInitPage(PCREATENOTEBOOKPAGE pcnbp,
                                                 ULONG flFlags);

        MRESULT XWPENTRY pgmiPageMageColorsItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                              ULONG ulItemID, USHORT usNotifyCode,
                                              ULONG ulExtra);

        VOID XWPENTRY pgmiPageMageWindowInitPage(PCREATENOTEBOOKPAGE pcnbp,
                                                 ULONG flFlags);

        MRESULT XWPENTRY pgmiPageMageWindowItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                              ULONG ulItemID, USHORT usNotifyCode,
                                              ULONG ulExtra);
    #else
        #error "shared\notebook.h needs to be included before including pagemage.h".
    #endif

// #endif // __PAGEMAGE__

#endif
