
/*
 *@@sourcefile mptrhook.c:
 *
 *      This file is ALL new with V0.9.4.
 *
 *@@added V0.9.4 [umoeller]
 *@@header "pointers\mptrhook.h"
 */

/*
 *      Copyright (C) 1996-2000 Christian Langanke.
 *      Copyright (C) 2000 Ulrich M�ller.
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

// C Runtime
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

// OS/2 Toolkit
#define INCL_ERRORS
#define INCL_PM
#define INCL_WIN
#define INCL_DOS
#define INCL_DOSDEVIOCTL
#define INCL_DOSMISC
#include <os2.h>

// generic headers
#include "setup.h"              // code generation and debugging options

#include "pointers\mptrhook.h"
#include "pointers\title.h"
#include "pointers\debug.h"

// globale variablen
static HOOKDATA hookdataGlobal;

/*������������������������������������������������������������������������Ŀ
 *� Name      : SetHookData                                                �
 *� Kommentar : initialisiert Hook-Daten                                   �
 *� Autor     : C.Langanke                                                 �
 *� Datum     : 10.10.1996                                                 �
 *� �nderung  : 10.10.1996                                                 �
 *� aufgerufen: ###                                                        �
 *� ruft auf  : -                                                          �
 *� Eingabe   : PHOOKDATA - Zeiger auf Hook-Daten                          �
 *� Aufgaben  : - Daten �bernehmen                                         �
 *� R�ckgabe  : APIRET - OS/2 Fehlercode                                   �
 *��������������������������������������������������������������������������
 */

APIRET _Export SetHookData
 (
     PSZ pszVersion,
     PHOOKDATA phookdata
)
{
    APIRET rc = NO_ERROR;

    do
    {
        // check parms
        if ((pszVersion == NULL) ||
            (strcmp(BLDLEVEL_VERSION, pszVersion) != 0))
        {
            rc = ERROR_INVALID_PARAMETER;
            break;
        }

        // Zeiger �bernehmen
        memcpy(&hookdataGlobal, phookdata, sizeof(HOOKDATA));

    }
    while (FALSE);

    return rc;
}

/*������������������������������������������������������������������������Ŀ
 *� Name      : InputHook                                                  �
 *� Comment   :                                                            �
 *� Author    : C.Langanke                                                 �
 *� Date      : 06.12.1995                                                 �
 *� Update    : 06.12.1995                                                 �
 *� called by : PM message queue                                           �
 *� calls     : Win*, Dos*                                                 �
 *� Input     : HAB, PQMSG, USHORT - parms of input hook                   �
 *� Tasks     : - intercepts trigger message and performs specified action �
 *�               on specified pushbutton.                                 �
 *� returns   : BOOL - remove flag                                         �
 *��������������������������������������������������������������������������
 */
BOOL _Export EXPENTRY InputHook
 (
     HAB hab,
     PQMSG pqmsg,
     USHORT usRemove
)

{
    BOOL fRemove = FALSE;
    USHORT x, y;
    static USHORT xold = 0, yold = 0;
    BOOL fMouseMoved = FALSE;


    if ((pqmsg->msg >= WM_BUTTONCLICKFIRST) &&
        (pqmsg->msg <= WM_BUTTONCLICKLAST))
        // send message on any click
        fMouseMoved = TRUE;
    else if (pqmsg->msg == WM_MOUSEMOVE)
    {
        // send message on move
        // check position, because for WARP 3 there is
        // a periodic mousemove message without moving
        x = SHORT1FROMMP(pqmsg->mp1);
        y = SHORT2FROMMP(pqmsg->mp1);
        if ((x != xold) || (y != yold))
            fMouseMoved = TRUE;
        xold = x;
        yold = y;
    }

// post the message
    if (fMouseMoved)
        WinPostMsg(hookdataGlobal.hwndNotify,
                   WM_TIMER,
                   (MPARAM)MOUSEMOVED_TIMER_ID,
                   MPFROMLONG(FALSE));

    return fRemove;
}
