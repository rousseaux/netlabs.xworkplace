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

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS

#include "pointers\expire.h"
#include "pointers\debug.h"
#include "pointers\r_amptreng.h"

#define NEWLINE "\n"

/*ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 *³ Name      : CheckExpiration                                            ³
 *³ Comment   :                                                            ³
 *³ Author    : C.Langanke                                                 ³
 *³ Date      : 25.08.1996                                                 ³
 *³ Update    : 25.08.1996                                                 ³
 *³ called by : diverse                                                    ³
 *³ calls     : -                                                          ³
 *³ Input     : HMODULE - resource lib handle                              ³
 *³             PSZ     - Titel der Fehler-Messagebox                      ³
 *³             PSZ     - Text der Fehler-Messagebox                       ³
 *³ Tasks     : - Zeit prfen und ggfs. Fehlermeldung ausgeben             ³
 *³ returns   : APIRET - OS/2 error code                                   ³
 *ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 */


APIRET _Export CheckExpiration
 (
     HMODULE hmodResource
)

{

    APIRET rc = NO_ERROR;
    PVOID pvResource;
    PSZ pszExpirationDate = NULL;
    ULONG ulExpirationDate;
    ULONG ulYear, ulMonth;
    ULONG ulResId = IDRCD_EXPIRATIONDATE;
    DATETIME dt;

    do
    {
        // Ablaufdatum holen
        rc = DosGetResource(hmodResource, RT_RCDATA, ulResId, &pvResource);
        if (rc != NO_ERROR)
            break;

        // Daten lesen
        ulYear = *((PUSHORT) pvResource);
        ulMonth = *((PUSHORT) pvResource + 1);

        // check date here
        DosGetDateTime(&dt);
        if (ulYear < (ULONG) dt.year)
        {
            rc = ERROR_ACCESS_DENIED;
            break;
        }

        if ((ulYear == (ULONG) dt.year) && (ulMonth <= (ULONG) dt.month))
        {
            rc = ERROR_ACCESS_DENIED;
            break;
        }

    }
    while (FALSE);

// cleanup
    if (pvResource)
        DosFreeResource(pvResource);

// Fehlermeldung ausgeben
    /* if (rc != NO_ERROR)
    {
        WinMessageBox(HWND_DESKTOP, HWND_DESKTOP,
         "Beta version has expired. Contact " __EMAIL__ " for a new version.",
                      __TITLE__, 0, MB_CANCEL);
    } */

    return rc;

}
