
/*
 *@@sourcefile apm.c:
 *      this contains the XFolder APM interface for automatically
 *      turning the computer off after shutdown has completed.
 *      This is mostly used in the context of XShutdown (shutdown.c).
 *
 *      Massive thanks go out to ARAKAWA Atsushi (arakaw@ibm.net)
 *      for filling this in, and to Roman Stangl (rstangl@vnet.ibm.com)
 *      for finding out all the APM stuff in the first place.
 *
 *@@header "startshut\apm.h"
 */

/*
 *      Copyright (C) 1998 ARAKAWA Atsushi.
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

/*
 *@@todo:
 *
 */

/*
 *  Suggested #include order:
 *  1)  os2.h
 *  2)  C library headers
 *  3)  setup.h (code generation and debugging options)
 *  4)  headers in helpers\
 *  5)  at least one SOM implementation header (*.ih)
 *  6)  dlgids.h, headers in shared\ (as needed)
 *  7)  headers in implementation dirs (e.g. filesys\, as needed)
 *  8)  #pragma hdrstop and then more SOM headers which crash with precompiled headers
 */

#define INCL_DOSPROCESS
#define INCL_DOSERRORS
#define INCL_DOSDEVICES
#define INCL_DOSDEVIOCTL
#include <os2.h>

// C library headers
#include <stdio.h>
#include <string.h>

// generic headers
#include "setup.h"                      // code generation and debugging options

// XWorkplace implementation headers
#include "startshut\apm.h"            // APM power-off for XShutdown

static HFILE           G_hfAPMSys = NULLHANDLE;
static ULONG           G_ulAPMStat = APM_UNKNOWN;
static USHORT          G_usAPMVersion = 0;
static CHAR            G_szAPMVersion[10];

/*
 *@@  apmQueryVersion:
 *      This function returns a PSZ to a static APM
 *      version string, e.g. "1.2".
 *
 *      <B>Usage:</B> any time, esp. on the shutdown
 *      notebook page.
 *
 */

PSZ apmQueryVersion(VOID)
{
    if (G_ulAPMStat == APM_UNKNOWN)
        apmPowerOffSupported();
    sprintf(G_szAPMVersion, "%d.%d", G_usAPMVersion>>8, G_usAPMVersion & 0xff);
    return (G_szAPMVersion);
}

/*
 *@@  apmPowerOffSupported:
 *      This function returns TRUE if the computer
 *      supports the APM power-off function.
 *
 *      <B>Usage:</B> any time, especially
 *      by the "XDesktop" page in the Desktop's
 *      settings notebook to determine if the "APM power
 *      off" checkbox will be enabled or not (i.e. cannot
 *      be selected).
 *
 */

BOOL apmPowerOffSupported(VOID)
{
    USHORT          usBIOSVersion, usDriverVersion;
    APIRET          arc = NO_ERROR;
    ULONG           ulAction = 0;
    GETPOWERINFO    getpowerinfo;
    ULONG           ulAPMRc;
    ULONG           ulPacketSize;
    ULONG           ulDataSize;

    if (G_ulAPMStat == APM_UNKNOWN)
    {
        // open APM.SYS
        ulAction = 0;
        arc = DosOpen("\\DEV\\APM$", &G_hfAPMSys, &ulAction, 0, FILE_NORMAL,
                      OPEN_ACTION_OPEN_IF_EXISTS,
                      OPEN_FLAGS_FAIL_ON_ERROR | OPEN_SHARE_DENYNONE | OPEN_ACCESS_READWRITE,
                      NULL);
        if (arc == NO_ERROR)
        {
            // query version of APM-BIOS and APM driver
            memset(&getpowerinfo, 0, sizeof(getpowerinfo));
            getpowerinfo.usParmLength = sizeof(getpowerinfo);
            ulPacketSize = sizeof(getpowerinfo);
            ulAPMRc = 0;
            ulDataSize = sizeof(ulAPMRc);
            arc = DosDevIOCtl(G_hfAPMSys, IOCTL_POWER,
                              POWER_GETPOWERINFO,
                              &getpowerinfo, ulPacketSize, &ulPacketSize,
                              &ulAPMRc, ulDataSize, &ulDataSize);
            if ((arc == NO_ERROR) && (ulAPMRc == NO_ERROR))
            {
                // swap lower-byte(major vers.) to higher-byte(minor vers.)
                usBIOSVersion = (getpowerinfo.usBIOSVersion & 0xff) <<8 |
                    (getpowerinfo.usBIOSVersion >>8);
                usDriverVersion = (getpowerinfo.usDriverVersion & 0xff) <<8 |
                    (getpowerinfo.usDriverVersion >>8);
                // set general APM version to lower
                G_usAPMVersion = (usBIOSVersion < usDriverVersion)
                    ? usBIOSVersion : usDriverVersion;

                // check APM version whether power-off is supported
                if (G_usAPMVersion >= 0x102)  // version 1.2 or above
                    G_ulAPMStat = APM_OK;
                else
                    G_ulAPMStat = APM_IGNORE;

            }
            else
            {
                // DosDevIOCtl failed
                G_ulAPMStat = APM_IGNORE;
                G_usAPMVersion = 0;
            }
            DosClose(G_hfAPMSys);
        }
        else
        {
            // DosOpen failed; APM.SYS is not loaded
            G_ulAPMStat = APM_IGNORE;
            G_usAPMVersion = 0;
        }
    }
    return (G_ulAPMStat == APM_OK);
}

/*
 *@@ apmPreparePowerOff:
 *      This function is called _once_ by XShutdown while
 *      the system is being shut down, if apmPowerOffSupported
 *      above returned TRUE. This call happens
 *      after all windows have been closed, but
 *      before DosShutdown is called, so you may
 *      open the APM.SYS device driver here and
 *      do other preparations you might need.
 *
 *      After this, you must return _one_ of the
 *      following flags:
 *      --  APM_OK:  go ahead with XShutdown and call
 *                   apmDoPowerOff later.
 *      --  APM_IGNORE: go ahead with XShutdown and do
 *                   _not_ call apmDoPowerOff later;
 *                   this will lead to the normal
 *                   "Press Ctrl+Alt+Del" window.
 *      --  APM_CANCEL: cancel shutdown and present the
 *                   error message which you must
 *                   then copy to pszError, which
 *                   points to a buffer 500 bytes in
 *                   size.
 *
 *      ORed with _one_ or _none_ of the following:
 *      --  APM_DOSSHUTDOWN_0:
 *                   set this flag if XFolder should
 *                   call DosShutdown(0); this is _not_
 *                   recommended, because this would block
 *                   any subsequent DosDevIOCtl calls.
 *      --  APM_DOSSHUTDOWN_1:
 *                   the same for DosShutdown(1), which is
 *                   recommended.
 *
 *      If you return APM_OK only without either
 *      APM_DOSSHUTDOWN_0 or APM_DOSSHUTDOWN_1,
 *      XFolder will call apmDoPowerOff later,
 *      but without having called DosShutdown.
 *      You MUST do this yourself then.
 *      This will however prevent XFolder from
 *      presenting the proper informational windows
 *      to the user.
 *
 *      The buffer that pszError points to is only
 *      evaluated if you return APM_CANCEL.
 */

ULONG apmPreparePowerOff(PSZ pszError)      // in: error message
{
    APIRET          arc = NO_ERROR;
    ULONG           ulAction = 0;
    SENDPOWEREVENT  sendpowerevent;
    ULONG           ulAPMRc;
    ULONG           ulPacketSize;
    ULONG           ulDataSize;

    // open APM.SYS
    ulAction = 0;
    arc = DosOpen("\\DEV\\APM$",
                  &G_hfAPMSys,
                  &ulAction,
                  0,
                  FILE_NORMAL,
                  OPEN_ACTION_OPEN_IF_EXISTS,
                  OPEN_FLAGS_FAIL_ON_ERROR | OPEN_SHARE_DENYNONE | OPEN_ACCESS_READWRITE,
                  NULL);
    if (arc != NO_ERROR)
    {
        strcpy(pszError, "Cannot open APM driver.");
        return (APM_CANCEL);
    }
    // enable APM feature
    memset(&sendpowerevent, 0, sizeof(sendpowerevent));
    ulAPMRc = 0;
    sendpowerevent.usSubID = POWER_SUBID_ENABLE_APM;
    ulPacketSize = sizeof(sendpowerevent);
    ulDataSize = sizeof(ulAPMRc);
    arc = DosDevIOCtl(G_hfAPMSys,
                      IOCTL_POWER,
                      POWER_SENDPOWEREVENT,
                      &sendpowerevent, ulPacketSize, &ulPacketSize,
                      &ulAPMRc, ulDataSize, &ulDataSize);
    if (    (arc != NO_ERROR)
         || (ulAPMRc != NO_ERROR)
       )
    {
        strcpy(pszError, "Cannot enable APM.");
        return (APM_CANCEL);
    }

    return (APM_OK | APM_DOSSHUTDOWN_1); // APM_DOSSHUTDOWN_1 never works, hangs the system
}

/*
 *@@ apmDoPowerOff:
 *      If apmPreparePowerOff returned with the APM_OK flag set,
 *      XFolder calls this function after it is
 *      done with XShutdown. In this function,
 *      you should call the APM function which
 *      turns off the computer's power. If you
 *      have not specified one of the APM_DOSSHUTDOWN
 *      flags in apmPreparePowerOff, you must call DosShutdown
 *      yourself here.
 *
 *      <P><B>Usage:</B>
 *      only once after XShutdown.
 *      We do not return from this function, but stay in an infinite loop.
 *
 *      <P><B>Parameters:</B>
 *      none.
 *      </LL>
 *
 *@@changed V0.9.2 (2000-03-04) [umoeller]: added "APM delay" support
 */

VOID apmDoPowerOff(BOOL fDelay)
{
    APIRET          arc = NO_ERROR;
    SENDPOWEREVENT  sendpowerevent;
    USHORT          usAPMRc;
    ULONG           ulPacketSize;
    ULONG           ulDataSize;
    ULONG           ul;

    memset(&sendpowerevent, 0, sizeof(sendpowerevent));
    usAPMRc = 0;
    sendpowerevent.usSubID = POWER_SUBID_SET_POWER_STATE;
    sendpowerevent.usData1 = POWER_DEVID_ALL_DEVICES;
    sendpowerevent.usData2 = POWER_STATE_OFF;
    ulPacketSize = sizeof(sendpowerevent);
    ulDataSize = sizeof(usAPMRc);

    if (fDelay)
    {
        // try another pause of 3 seconds; maybe DosShutdown is
        // still running V0.9.2 (2000-02-29) [umoeller]
        for (ul = 0;
             ul < 3;
             ul++)
        {
            DosBeep(4000, 10);
            DosSleep(1000);
        }
        DosBeep(8000, 10);
    }

    // this initiates the APM power-off
    arc = DosDevIOCtl(G_hfAPMSys,
                      IOCTL_POWER,
                      POWER_SENDPOWEREVENT,
                      &sendpowerevent, ulPacketSize, &ulPacketSize,
                      &usAPMRc, ulDataSize, &ulDataSize);
    DosClose(G_hfAPMSys);

    // OS/2 _does_ return from that function, but in the
    // background APM power-off is now being executed.
    // So we must now loop forever until power-off has
    // completed, when the computer will just stop
    // executing anything.

    // We must _not_ return to the XFolder shutdown routines,
    // because XFolder would then enter a critical section,
    // which would keep power-off from working.
    while (TRUE)
        DosSleep(10000);
}

