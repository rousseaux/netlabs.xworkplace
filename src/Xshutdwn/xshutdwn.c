
/*
 * xshutdwn.c:
 *      this is the XShutdown command-line interface. It does NOT
 *      contain any real shutdown code, but only posts a message
 *      to the XWorkplace Object window in XFLDR.DLL which will then
 *      do the rest.
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

#define  INCL_WIN
#define  INCL_DOS
#define  INCL_GPI

#include <os2.h>
#include <stdio.h>
#include <string.h>

#include "helpers\winh.h"
#include "shared\common.h"
#include "shared\kernel.h"
#include "startshut\shutdown.h"

void Explain(void)
{
    DosBeep(100, 500);
}

int main(int argc, char *argv[])
{
    HAB         hab;
    HMQ         hmq;
    HWND        hwndXWorkplaceObject;
    CHAR        szBlah[1000];
    MRESULT     mrVersion = 0;
    PSHUTDOWNPARAMS psdp;
    BOOL        fProceed = TRUE;

    if (DosAllocSharedMem((PVOID*)&psdp,
                    NULL,       // unnamed
                    sizeof(SHUTDOWNPARAMS),
                    PAG_COMMIT | OBJ_GETTABLE | PAG_READ | PAG_WRITE
        ) == 0)
    {
        // defaults
        psdp->optReboot = FALSE;
        psdp->optDebug = FALSE;
        psdp->optRestartWPS = FALSE;
        psdp->optWPSCloseWindows = FALSE;
        psdp->optAutoCloseVIO = FALSE;
        psdp->optLog = FALSE;
        psdp->optAnimate = FALSE;
        psdp->optConfirm = TRUE;
        strcpy(psdp->szRebootCommand, "");

        // evaluate command line
        if (argc > 1)
        {
            SHORT i = 0;
            while (i++ < argc-1)
            {
                if (argv[i][0] == '-')
                {
                    SHORT i2;
                    for (i2 = 1; i2 < strlen(argv[i]); i2++)
                    {
                        switch (argv[i][i2])
                        {
                            case 'r':
                                psdp->optReboot = TRUE;
                            break;

                            case 'R':
                                psdp->optReboot = TRUE;
                                if (i < argc-1)
                                {
                                    strcpy(psdp->szRebootCommand, argv[i+1]);
                                    i++;
                                    i2 = 1000;
                                }
                                else
                                {
                                    Explain();
                                    fProceed = FALSE;
                                }
                            break;

                            case 'D':
                                psdp->optDebug = TRUE;
                            break;

                            case 'l':
                                psdp->optLog = TRUE;
                            break;

                            case 'v':
                                psdp->optAutoCloseVIO = TRUE;
                            break;

                            case 'a':
                                psdp->optAnimate = TRUE;
                            break;

                            case 'f':
                                psdp->optConfirm = FALSE;
                            break;

                            case 'w':
                                psdp->optRestartWPS = TRUE;
                                psdp->optWPSCloseWindows = FALSE;
                            break;

                            case 'W':
                                psdp->optRestartWPS = TRUE;
                                psdp->optWPSCloseWindows = TRUE;
                            break;

                            default:  // unknown parameter
                                Explain();
                                fProceed = FALSE;
                            break;
                        }
                    }
                }
                else
                {
                    // no option ("-"): explain
                    Explain();
                }
            }
        }

        if (fProceed)
        {
            if (!(hab = WinInitialize(0)))
                return FALSE;

            if (!(hmq = WinCreateMsgQueue(hab, 0)))
                return FALSE;

            // find the XWorkplace object window
            hwndXWorkplaceObject = WinWindowFromID(HWND_OBJECT, ID_THREAD1OBJECT);

            // check if this window understands the
            // "query version" message
            if (hwndXWorkplaceObject)
            {
                SHORT   sMajor,
                        sMinor;
                mrVersion = WinSendMsg(hwndXWorkplaceObject,
                                       T1M_QUERYXFOLDERVERSION,
                                       0, 0);
                sMajor = SHORT1FROMMR(mrVersion);
                sMinor = SHORT2FROMMR(mrVersion);
                // error:
                if (    (    (sMajor == 0)
                          && (sMinor < 9)
                        )
                   )
                    DebugBox(0,
                             "XShutdown: Error",
                             "The external XShutdown interface could not be "
                             "accessed. Either XWorkplace is not properly installed, "
                             "or the WPS is not currently running, "
                             "or the installed XWorkplace version is too old to support "
                             "calling XShutdown from the command line.");
                else
                {
                    // XWorkplace version supports command line: go on
                    if (!WinSendMsg(hwndXWorkplaceObject,
                                    T1M_EXTERNALSHUTDOWN,
                                    (MPARAM)psdp,
                                    (MPARAM)0))
                        DebugBox(0,
                                 "XShutdown: Error",
                                 "XWorkplace reported an error processing the "
                                 "external shutdown request. "
                                 "XShutdown was not initiated. Please shut down using the Desktop's "
                                 "context menu.");
                }
            }

            WinDestroyMsgQueue(hmq);
            WinTerminate(hab);
        }
    }
    else
            DebugBox(0,
                     "XShutdown: Error",
                     "XShutdown failed allocating shared memory.");

    return TRUE;
}


