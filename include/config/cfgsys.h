
/*
 *@@sourcefile cfgsys.h:
 *      header file for cfgsys.c (XFldSystem CONFIG.SYS pages implementation).
 *
 *      This file is ALL new with V0.9.0.
 *
 *@@include #include <os2.h>
 *@@include #include "shared\notebook.h"
 *@@include #include "config\cfgsys.h"
 */

/*
 *      Copyright (C) 1997-2000 Ulrich M”ller.
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

    /* ******************************************************************
     *
     *   External APIs
     *
     ********************************************************************/

    BOOL XWPENTRY cfgParseSwapPath(const char *pcszConfigSys,
                                   PSZ pszSwapPath,
                                   PULONG pulMinFree,
                                   PULONG pulMinSize);
    typedef BOOL XWPENTRY CFGPARSESWAPPATH(const char *pcszConfigSys,
                                           PSZ pszSwapPath,
                                           PULONG pulMinFree,
                                           PULONG pulMinSize);
    typedef CFGPARSESWAPPATH *PCFGPARSESWAPPATH;

    ULONG XWPENTRY cfgQuerySwapperSize(VOID);
    typedef ULONG XWPENTRY CFGQUERYSWAPPERSIZE(VOID);
    typedef CFGQUERYSWAPPERSIZE *PCFGQUERYSWAPPERSIZE;

    /* ******************************************************************
     *
     *   XFldDisk notebook callbacks (notebook.c)
     *
     ********************************************************************/

    #ifdef NOTEBOOK_HEADER_INCLUDED
        VOID cfgConfigInitPage(PCREATENOTEBOOKPAGE pcnbp,
                               ULONG flFlags);

        MRESULT cfgConfigItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                     USHORT usItemID,
                                     USHORT usNotifyCode,
                                     ULONG ulExtra);

        VOID cfgConfigTimer(PCREATENOTEBOOKPAGE pcnbp,
                            ULONG ulTimer);

        extern MPARAM *G_pampDriversPage;
        extern ULONG G_cDriversPage;

        VOID cfgDriversInitPage(PCREATENOTEBOOKPAGE pcnbp,
                                ULONG flFlags);

        MRESULT cfgDriversItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                      USHORT usItemID,
                                      USHORT usNotifyCode,
                                      ULONG ulExtra);

        VOID cfgSyslevelInitPage(PCREATENOTEBOOKPAGE pcnbp,
                                 ULONG flFlags);

        MRESULT cfgSyslevelItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                       USHORT usItemID,
                                       USHORT usNotifyCode,
                                       ULONG ulExtra);
    #endif
#endif


