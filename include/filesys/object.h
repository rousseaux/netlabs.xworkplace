
/*
 *@@sourcefile object.h:
 *      header file for object.c (XFldObject implementation).
 *
 *      This file is ALL new with V0.9.0.
 *
 *@@include #include <os2.h>
 *@@include #include <wpobject.h>   // or any other WPS SOM header
 *@@include #include "hook\xwphook.h"
 *@@include #include "filesys\object.h"
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

#ifndef OBJECT_HEADER_INCLUDED
    #define OBJECT_HEADER_INCLUDED

    /* ******************************************************************
     *
     *   Object flags
     *
     ********************************************************************/

    #define OBJLIST_RUNNINGSTORED           0x0001

    /* ******************************************************************
     *
     *   Object "Internals" page
     *
     ********************************************************************/

    MRESULT EXPENTRY obj_fnwpSettingsObjDetails(HWND hwndDlg, ULONG msg, MPARAM mp1, MPARAM mp2);

    /* ******************************************************************
     *
     *   Object hotkeys
     *
     ********************************************************************/

    #ifdef XWPHOOK_HEADER_INCLUDED
    PGLOBALHOTKEY objFindHotkey(PGLOBALHOTKEY pHotkeys,
                                ULONG cHotkeys,
                                HOBJECT hobj);
    #endif

    BOOL objQueryObjectHotkey(WPObject *somSelf,
                              PUSHORT pusFlags,
                              PUCHAR  pucScanCode,
                              PUSHORT pusKeyCode);

    BOOL objSetObjectHotkey(WPObject *somSelf,
                            USHORT usFlags,
                            UCHAR ucScanCode,
                            USHORT usKeyCode);

    BOOL objRemoveObjectHotkey(HOBJECT hobj);

    /* ******************************************************************
     *
     *   Object setup strings
     *
     ********************************************************************/

    ULONG objQuerySetup(WPObject *somSelf,
                        PSZ pszSetupString,
                        ULONG cbSetupString);
#endif


