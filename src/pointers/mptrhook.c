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

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : SetHookData                                                ³
 *³ Kommentar : initialisiert Hook-Daten                                   ³
 *³ Autor     : C.Langanke                                                 ³
 *³ Datum     : 10.10.1996                                                 ³
 *³ Žnderung  : 10.10.1996                                                 ³
 *³ aufgerufen: ###                                                        ³
 *³ ruft auf  : -                                                          ³
 *³ Eingabe   : PHOOKDATA - Zeiger auf Hook-Daten                          ³
 *³ Aufgaben  : - Daten bernehmen                                         ³
 *³ Rckgabe  : APIRET - OS/2 Fehlercode                                   ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
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
            (strcmp(__VERSION__, pszVersion) != 0))
        {
            rc = ERROR_INVALID_PARAMETER;
            break;
        }

        // Zeiger bernehmen
        memcpy(&hookdataGlobal, phookdata, sizeof(HOOKDATA));

    }
    while (FALSE);

    return rc;
}

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : InputHook                                                  ³
 *³ Comment   :                                                            ³
 *³ Author    : C.Langanke                                                 ³
 *³ Date      : 06.12.1995                                                 ³
 *³ Update    : 06.12.1995                                                 ³
 *³ called by : PM message queue                                           ³
 *³ calls     : Win*, Dos*                                                 ³
 *³ Input     : HAB, PQMSG, USHORT - parms of input hook                   ³
 *³ Tasks     : - intercepts trigger message and performs specified action ³
 *³               on specified pushbutton.                                 ³
 *³ returns   : BOOL - remove flag                                         ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
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
