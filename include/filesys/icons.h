
/*
 *@@sourcefile icons.h:
 *      header file for icons.c.
 *
 *      This file is ALL new with V0.9.16.
 *
 *@@include #include <os2.h>
 *@@include #include <wpobject.h>   // or any other WPS SOM header
 *@@include #include "shared\notebook.h"    // for notebook headers
 *@@include #include "filesys\icon.h"
 */

/*
 *      Copyright (C) 2001 Ulrich M”ller.
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

#ifndef ICONS_HEADER_INCLUDED
    #define ICONS_HEADER_INCLUDED

    /* ******************************************************************
     *
     *   Notebook callbacks (notebook.c) for new XFldObject "Icon" page
     *
     ********************************************************************/

    #ifdef NOTEBOOK_HEADER_INCLUDED

        #define ICONFL_TITLE            0x0001
        #define ICONFL_ICON             0x0002
        #define ICONFL_TEMPLATE         0x0004
        #define ICONFL_LOCKEDINPLACE    0x0008
        #define ICONFL_HOTKEY           0x0010
        #define ICONFL_DETAILS          0x0020

        VOID icoFormatIconPage(PCREATENOTEBOOKPAGE pcnbp,
                               ULONG flFlags);

        VOID XWPENTRY icoIcon1InitPage(PCREATENOTEBOOKPAGE pcnbp,
                                       ULONG flFlags);

        MRESULT XWPENTRY icoIcon1ItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                             ULONG ulItemID, USHORT usNotifyCode,
                                             ULONG ulExtra);
    #endif

    /* ******************************************************************
     *
     *   Object details dialog
     *
     ********************************************************************/

    VOID icoShowObjectDetails(HWND hwndOwner,
                              WPObject *pobj);


#endif


