
/*
 * wpsreset.c:
 *      this resets the WPS using PrfReset(). This API was originally
 *      intended to change the user INI file (OS2.INI) and will then
 *      restart the WPS. But this also works if you call it with the
 *      current user INI file. ;-)
 *
 *      I suppose the WPS reacts to the PL_ALTERED msg which is
 *      broadcast to all msg queues on the system by terminating
 *      itself. The first instance of PMSHELL.EXE will then restart
 *      the WPS.
 *
 *      WPSRESET only works if you pass it the "-D" parameter on the
 *      command line in order to prevent accidental Desktop restarts.
 *      I don't remember what "-D" stands for. Maybe "dumb".
 *
 *      Copyright (C) 1997-2000 Ulrich M”ller.
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, in version 2 as it comes in the COPYING
 *      file of the XWorkplace main distribution.
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 */

#include <stdlib.h>
#define INCL_WIN
#include <os2.h>
#include <string.h>
#include <stdio.h>

#include "bldlevel.h"

void Error(PSZ psz)
{
    printf("wpsreset: %s\n", psz);
}

int main(int argc, char *argv[])
{
    BOOL    fContinue = FALSE;
    HAB     habShutdown = WinInitialize(0);
    HMQ     hmq = WinCreateMsgQueue(habShutdown, 0);

    if (argc == 2)
        // check for "-D" parameter
        if (strcmp(argv[1], "-D") == 0)
            fContinue = TRUE;

    if (fContinue)
    {
        // find out current profile names
        PRFPROFILE Profiles;
        Profiles.cchUserName = Profiles.cchSysName = 0;
        // first query their file name lengths
        if (PrfQueryProfile(habShutdown, &Profiles))
        {
            // allocate memory for filenames
            Profiles.pszUserName  = malloc(Profiles.cchUserName);
            Profiles.pszSysName  = malloc(Profiles.cchSysName);

            if (Profiles.pszSysName)
            {
                // get filenames
                if (PrfQueryProfile(habShutdown, &Profiles))
                {

                    // "change" INIs to these filenames:
                    // THIS WILL RESET THE WPS
                    if (PrfReset(habShutdown, &Profiles) == FALSE)
                        Error("Unable to reset profiles.");
                    free(Profiles.pszSysName);
                    free(Profiles.pszUserName);
                }
                else
                    Error("Unable to query profile file names.");
            }
            else
                Error("Out of memory.");
        }
        else
            Error("Unable to query profile file name sizes.");
    }
    else
    {
        printf("wpsreset V"BLDLEVEL_VERSION" ("__DATE__") (C) 1998-2000 Ulrich M”ller\n");
        printf("Restarts the Workplace Shell process.\n");
        printf("Usage: wpsreset -D\n");
    }


    WinDestroyMsgQueue(hmq);
    WinTerminate(habShutdown);

    return (0);
}

