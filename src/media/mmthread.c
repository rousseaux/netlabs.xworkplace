
/*
 *@@sourcefile mmthread.c:
 *      this has the XWorkplace Media thread, which handles
 *      multimedia tasks (playing sounds etc.) in XWorkplace.
 *
 *      This is all new with V0.9.3. Most of this code used
 *      to be in the "Speedy" ("Quick") thread in filesys\xthreads.c
 *      previously.
 *
 *      With V0.9.3, SOUND.DLL is gone also. That DLL was introduced
 *      because if we linked XFLDR.DLL against mmpm2.lib,
 *      XFolder/XWorkplace refused to install on systems without
 *      OS/2 multimedia installed because the DLL imports failed.
 *      Achim HasenmÅller pointed out to me that I could also
 *      dynamically import the MMPM/2 routines into the XFLDR.DLL
 *      itself, so that's what we do here. You'll find function
 *      prototypes for for MMPM/2 functions below, which are
 *      resolved by xmmInit.
 *
 *      Note: Those G_mmio* and G_mci* identifiers are global
 *      variables containing MMPM/2 API entries. Those are
 *      resolved by xmmInit (mmthread.c) and must only be used
 *      after checking xmmQueryStatus.
 *
 *@@added V0.9.3 (2000-04-25) [umoeller]
 *@@header "media\media.h"
 */

/*
 *      Copyright (C) 1997-2000 Ulrich Mîller.
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

#pragma strings(readonly)

/*
 *  Suggested #include order:
 *  1)  os2.h
 *  2)  C library headers
 *  3)  setup.h (code generation and debugging options)
 *  4)  headers in helpers\
 *  5)  at least one SOM implementation header (*.ih)
 *  6)  dlgids.h, headers in shared\ (as needed)
 *  7)  headers in filesys\ (as needed)
 *  8)  #pragma hdrstop and then more SOM headers which crash with precompiled headers
 */

#define INCL_DOSPROCESS
#define INCL_DOSSEMAPHORES
#define INCL_DOSEXCEPTIONS
#define INCL_DOSERRORS

#define INCL_WINMESSAGEMGR

#define INCL_GPI                // required for INCL_MMIO_CODEC
#define INCL_GPIBITMAPS         // required for INCL_MMIO_CODEC
#include <os2.h>

// multimedia includes
#define INCL_MCIOS2
#define INCL_MMIOOS2
#define INCL_MMIO_CODEC
#include <os2me.h>

// C library headers
#include <stdio.h>              // needed for except.h
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h
#include <io.h>                 // access etc.

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\syssound.h"           // system sound helper routines
#include "helpers\threads.h"            // thread helpers
#include "helpers\winh.h"               // PM helper routines

// XWorkplace implementation headers
#include "media\media.h"                // XWorkplace multimedia support

/* ******************************************************************
 *                                                                  *
 *   Global variables                                               *
 *                                                                  *
 ********************************************************************/

HAB         G_habMediaThread = NULLHANDLE;
HMQ         G_hmqMediaThread = NULLHANDLE;
extern HWND G_hwndMediaObject = NULLHANDLE;

// sound data
ULONG       G_ulMMPM2Working = MMSTAT_UNKNOWN;

USHORT      G_usSoundDeviceID = 0;
ULONG       G_ulVolumeTemp = 0;

THREADINFO  G_tiMediaThread = {0};

extern PXMMCDPLAYER G_pPlayer;      // in mmhelp.c

const char *WNDCLASS_MEDIAOBJECT = "XWPMediaThread";

/* ******************************************************************
 *                                                                  *
 *   Function imports                                               *
 *                                                                  *
 ********************************************************************/

// resolved function addresses...
FNTD_MCISENDCOMMAND         *G_mciSendCommand = NULL;
FNTD_MCIGETERRORSTRING      *G_mciGetErrorString = NULL;
FNTD_MMIOINIFILECODEC       *G_mmioIniFileCODEC = NULL;
FNTD_MMIOQUERYCODECNAMELENGTH *G_mmioQueryCODECNameLength;
FNTD_MMIOQUERYCODECNAME     *G_mmioQueryCODECName = NULL;
FNTD_MMIOINIFILEHANDLER     *G_mmioIniFileHandler = NULL;
FNTD_MMIOQUERYFORMATCOUNT   *G_mmioQueryFormatCount = NULL;
FNTD_MMIOGETFORMATS         *G_mmioGetFormats = NULL;
FNTD_MMIOGETFORMATNAME      *G_mmioGetFormatName = NULL;

/*
 * G_aResolveFromMDM:
 *      functions imported from MDM.DLL.
 *      Used with doshResolveImports.
 */

RESOLVEFUNCTION G_aResolveFromMDM[] =
        {
                "mciSendCommand", (PFN*)&G_mciSendCommand,
                "mciGetErrorString", (PFN*)&G_mciGetErrorString
        };

/*
 * G_aResolveFromMMIO:
 *      functions resolved from MMIO.DLL.
 *      Used with doshResolveImports.
 */

RESOLVEFUNCTION G_aResolveFromMMIO[] =
        {
                "mmioIniFileCODEC", (PFN*)&G_mmioIniFileCODEC,
                "mmioQueryCODECNameLength", (PFN*)&G_mmioQueryCODECNameLength,
                "mmioQueryCODECName", (PFN*)&G_mmioQueryCODECName,
                "mmioIniFileHandler", (PFN*)&G_mmioIniFileHandler,
                "mmioQueryFormatCount", (PFN*)&G_mmioQueryFormatCount,
                "mmioGetFormats", (PFN*)&G_mmioGetFormats,
                "mmioGetFormatName", (PFN*)&G_mmioGetFormatName,
        };

/* ******************************************************************
 *                                                                  *
 *   Media thread                                                   *
 *                                                                  *
 ********************************************************************/

/*
 *@@ xmm_fnwpMediaObject:
 *      window procedure for the Media thread
 *      (xmm_fntMediaThread) object window.
 *
 *@@added V0.9.3 (2000-04-25) [umoeller]
 *@@changed V0.9.7 (2000-12-20) [umoeller]: removed XMM_CDPLAYER
 */

MRESULT EXPENTRY xmm_fnwpMediaObject(HWND hwndObject, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = NULL;

    switch (msg)
    {
        /*
         *@@ XMM_PLAYSYSTEMSOUND:
         *      plays system sound specified in MMPM.INI.
         *      This is posted by xthrPostMediaMsg.
         *      (USHORT)mp1 must be the MMPM.INI index (see
         *      sndQuerySystemSound in common.c for a list).
         */

        case XMM_PLAYSYSTEMSOUND:
        {
            CHAR    szDescr[CCHMAXPATH];
            ULONG   ulVolume;
            // allocate mem for sound file; this will
            // be freed in QM_PLAYSOUND below
            PSZ     pszFile = malloc(CCHMAXPATH);

            #ifdef DEBUG_SOUNDS
                _Pmpf(( "XMM_PLAYSYSTEMSOUND index %d", mp1));
            #endif

            // get system sound from MMPM.INI
            if (sndQuerySystemSound(G_habMediaThread,
                                    (USHORT)mp1,
                                    szDescr,
                                    pszFile,
                                    &ulVolume))
            {
                // OK, sound file found in MMPM.INI:
                #ifdef DEBUG_SOUNDS
                    _Pmpf(( "  posting Sound %d == %s, %s", mp1, szDescr, pszFile ));
                #endif

                // play!
                WinPostMsg(hwndObject,
                           XMM_PLAYSOUND,
                           (MPARAM)pszFile,
                           (MPARAM)ulVolume);
            }
            else
                // any error: do nothing
                free(pszFile);
        break; }

        /*
         *@@ XMM_PLAYSOUND:
         *      plays the sound file specified in
         *      (PSZ)mp1; this PSZ is assumed to have
         *      been allocated using malloc() and will
         *      be freed afterwards.
         *
         *      (ULONG)mp2 must be the volume (0-100).
         *
         *      This message is only posted by
         *      XMM_PLAYSYSTEMSOUND (above) if a system
         *      sound was queried successfully.
         *
         *      Playing sounds is a three-step process:
         *
         *      1)  We call xmmOpenSound in SOUND.DLL first
         *          to open a waveform device for this sound
         *          file as a _shareable_ device and then
         *          stop for the moment.
         *
         *      2)  If this device is accessible, MMPM/2 then
         *          posts us MM_MCIPASSDEVICE (below) so we can
         *          play the sound.
         *
         *      3)  We close the device if we're either losing
         *          it (because another app needs it -- that's
         *          MM_MCIPASSDEVICE with MCI_LOSING_USE set)
         *          or if MMPM/2 is done with our sound (that's
         *          MM_MCINOTIFY below).
         */

        case XMM_PLAYSOUND:
        {
            if (mp1)
            {
                // check for whether that sound file really exists
                if (access(mp1, 0) == 0)
                    xmmOpenSound(hwndObject,
                                 &G_usSoundDeviceID,
                                 (PSZ)mp1);
                        // this will post MM_MCIPASSDEVICE
                        // if the device is available

                // free the PSZ passed to us
                free((PSZ)mp1);
                G_ulVolumeTemp = (ULONG)mp2;
            }
        break; }

        /*
         * MM_MCIPASSDEVICE:
         *      MMPM/2 posts this msg for shareable devices
         *      to allow multimedia applications to behave
         *      politely when several applications use the
         *      same device. This is posted to us in two cases:
         *      1)  opening the device above was successful
         *          and the device is available (that is, no
         *          other application needs exclusive access
         *          to that device); in this case, mp2 has the
         *          MCI_GAINING_USE flag set, and we can call
         *          xmmPlaySound in SOUND.DLL to actually
         *          play the sound.
         *          The device is _not_ available, for example,
         *          when a Win-OS/2 session is running which
         *          uses sounds.
         *      2)  While we are playing, another application
         *          is trying to get access to the device; in
         *          this case, mp2 has the MCI_LOSING_USE flag
         *          set, and we call xmmStopSound to stop
         *          playing our sound.
         */

        #define MM_MCINOTIFY                        0x0500
        #define MM_MCIPASSDEVICE                    0x0501
        #define MCI_LOSING_USE                      0x00000001L
        #define MCI_GAINING_USE                     0x00000002L
        #define MCI_NOTIFY_SUCCESSFUL               0x0000

        case MM_MCIPASSDEVICE:
        {
            BOOL fGainingUse = (SHORT1FROMMP(mp2) == MCI_GAINING_USE);

            #ifdef DEBUG_SOUNDS
                _Pmpf(( "MM_MCIPASSDEVICE: mp1 = 0x%lX, mp2 = 0x%lX", mp1, mp2 ));
                _Pmpf(( "    %s use", (fGainingUse) ? "Gaining" : "Losing" ));
            #endif

            if (fGainingUse)
            {
                // we're gaining the device (1): play sound
                xmmPlaySound(hwndObject,
                             &G_usSoundDeviceID,
                             G_ulVolumeTemp);
            }
            else
                // we're losing the device (2): stop sound
                xmmStopSound(&G_usSoundDeviceID);
        break; }

        /*
         * MM_MCINOTIFY:
         *      this is the general notification msg of MMPM/2.
         *      We need this message to know when MMPM/2 is done
         *      playing our sound; we will then close the device.
         */

        case MM_MCINOTIFY:
        {
            USHORT  usNotifyCode = SHORT1FROMMP(mp1),
                    usDeviceID = SHORT1FROMMP(mp2),
                    usMessage = SHORT2FROMMP(mp2);

            if ((G_usSoundDeviceID) && (usDeviceID == G_usSoundDeviceID))
            {
                if (usNotifyCode == MCI_NOTIFY_SUCCESSFUL)
                    xmmStopSound(&G_usSoundDeviceID);
            }
        break; }

        case MM_MCIPOSITIONCHANGE:
        {
            if (xmmLockDevicesList())
            {
                if (G_pPlayer)
                {
                    USHORT  usDeviceID = SHORT2FROMMP(mp1);

                    if (    (usDeviceID == G_pPlayer->usDeviceID)
                         && (G_pPlayer->hwndNotify)
                       )
                    {
                        ULONG   ulMMTime = (ULONG)mp2;

                        ULONG   ulSeconds;
                        ULONG   ulTrack = xmmCDCalcTrack(G_pPlayer,
                                                         ulMMTime,
                                                         &ulSeconds);
                        MPARAM  mp1Post = 0,
                                mp2Post = 0;
                        if (ulTrack != G_pPlayer->ulTrack)
                        {
                            G_pPlayer->ulTrack = ulTrack;
                            mp1Post = (MPARAM)ulTrack;
                        }
                        if (ulSeconds != G_pPlayer->ulSecondsInTrack)
                        {
                            G_pPlayer->ulSecondsInTrack = ulSeconds;
                            mp2Post = (MPARAM)ulSeconds;
                        }

                        WinPostMsg(G_pPlayer->hwndNotify,
                                   G_pPlayer->ulNotifyMsg,
                                   mp1Post,
                                   mp2Post);
                    }
                }
                xmmUnlockDevicesList();
            } // end if (xmmLockDevicesList())
        break; }

        default:
            mrc = WinDefWindowProc(hwndObject, msg, mp1, mp2);
    }

    return (mrc);
}

/*
 *@@ xmm_fntMediaThread:
 *      thread func for the Media thread, which creates
 *      an object window (xmm_fnwpMediaObject). This
 *      is responsible for playing sounds and such.
 *
 *@@added V0.9.3 (2000-04-25) [umoeller]
 */

void _Optlink xmm_fntMediaThread(PTHREADINFO pti)
{
    QMSG                  qmsg;
    PSZ                   pszErrMsg = NULL;
    BOOL                  fTrapped = FALSE;

    TRY_LOUD(excpt1)
    {
        if (G_habMediaThread = WinInitialize(0))
        {
            if (G_hmqMediaThread = WinCreateMsgQueue(G_habMediaThread, 3000))
            {
                WinCancelShutdown(G_hmqMediaThread, TRUE);

                WinRegisterClass(G_habMediaThread,
                                 (PSZ)WNDCLASS_MEDIAOBJECT,    // class name
                                 (PFNWP)xmm_fnwpMediaObject,    // Window procedure
                                 0,                  // class style
                                 0);                 // extra window words

                // set ourselves to higher regular priority
                DosSetPriority(PRTYS_THREAD,
                               PRTYC_REGULAR,
                               +31, // priority delta
                               0);

                // create object window
                G_hwndMediaObject
                    = winhCreateObjectWindow(WNDCLASS_MEDIAOBJECT, NULL);

                if (!G_hwndMediaObject)
                    winhDebugBox(HWND_DESKTOP,
                             "XFolder: Error",
                             "XFolder failed to create the Media thread object window.");

                // now enter the message loop
                while (WinGetMsg(G_habMediaThread, &qmsg, NULLHANDLE, 0, 0))
                    WinDispatchMsg(G_habMediaThread, &qmsg);
                                // loop until WM_QUIT
            }
        }
    }
    CATCH(excpt1)
    {
        // disable sounds
        fTrapped = TRUE;
    } END_CATCH();

    WinDestroyWindow(G_hwndMediaObject);
    G_hwndMediaObject = NULLHANDLE;
    WinDestroyMsgQueue(G_hmqMediaThread);
    G_hmqMediaThread = NULLHANDLE;
    WinTerminate(G_habMediaThread);
    G_habMediaThread = NULLHANDLE;

    if (fTrapped)
        G_ulMMPM2Working = MMSTAT_CRASHED;
    else
        G_ulMMPM2Working = MMSTAT_UNKNOWN;
}

/* ******************************************************************
 *                                                                  *
 *   Media thread interface                                         *
 *                                                                  *
 ********************************************************************/

/*
 *@@ xmmInit:
 *      initializes the XWorkplace Media environment
 *      and resolves the MMPM/2 APIs.
 *      Gets called by krnInitializeXWorkplace on
 *      WPS startup.
 *
 *@@added V0.9.3 (2000-04-25) [umoeller]
 */

BOOL xmmInit(FILE* DumpFile)
{
    HMODULE hmodMDM = NULLHANDLE,
            hmodMMIO = NULLHANDLE;

    if (DumpFile)
        fprintf(DumpFile,
                "\nEntering " __FUNCTION__":\n");

    G_ulMMPM2Working = MMSTAT_WORKING;

    if (doshResolveImports("MDM.DLL",
                           &hmodMDM,
                           G_aResolveFromMDM,
                           sizeof(G_aResolveFromMDM) / sizeof(G_aResolveFromMDM[0]))
            != NO_ERROR)
        G_ulMMPM2Working = MMSTAT_IMPORTSFAILED;
    else
        if (doshResolveImports("MMIO.DLL",
                               &hmodMMIO,
                               G_aResolveFromMMIO,
                               sizeof(G_aResolveFromMMIO) / sizeof(G_aResolveFromMMIO[0]))
                != NO_ERROR)
            G_ulMMPM2Working = MMSTAT_IMPORTSFAILED;

    if (DumpFile)
        fprintf(DumpFile,
                "  Resolved MMPM/2 imports, new XWP media status: %d\n",
                G_ulMMPM2Working);

    if (G_ulMMPM2Working == MMSTAT_WORKING)
    {
        thrCreate(&G_tiMediaThread,
                  xmm_fntMediaThread,
                  NULL, // running flag
                  "Media",
                  0,    // no msgq
                  0);
        if (DumpFile)
            fprintf(DumpFile,
                    "  Started XWP Media thread, TID: %d\n",
                    G_tiMediaThread.tid);
    }

    return (G_ulMMPM2Working == MMSTAT_WORKING);
}

/*
 *@@ xmmDisable:
 *      disables multimedia completely.
 *      Called from krnInitializeXWorkplace if
 *      the flag in the panic dialog has been set.
 *
 *@@added V0.9.3 (2000-04-30) [umoeller]
 */

VOID xmmDisable(VOID)
{
    G_ulMMPM2Working = MMSTAT_DISABLED;
}

/*
 *@@ xmmQueryStatus:
 *      returns the status of the XWorkplace media
 *      engine, which is one of the following:
 *
 +      --  MMSTAT_UNKNOWN: initial value after startup.
 +      --  MMSTAT_WORKING: media is working.
 +      --  MMSTAT_MMDIRNOTFOUND: MMPM/2 directory not found.
 +      --  MMSTAT_DLLNOTFOUND: MMPM/2 DLLs not found.
 +      --  MMSTAT_IMPORTSFAILED: MMPM/2 imports failed.
 +      --  MMSTAT_CRASHED: Media thread crashed, sounds disabled.
 +      --  MMSTAT_DISABLED: media explicitly disabled in startup panic dlg.
 *
 *      You should check this value when using XWorkplace media
 *      and use the media functions only when MMSTAT_WORKING is
 *      returned.
 *
 *@@added V0.9.3 (2000-04-25) [umoeller]
 */

ULONG xmmQueryStatus(VOID)
{
    return (G_ulMMPM2Working);
}

/*
 *@@ xmmPostMediaMsg:
 *      posts a message to xmm_fnwpMediaObject with
 *      error checking.
 *
 *@@added V0.9.3 (2000-04-25) [umoeller]
 */

BOOL xmmPostMediaMsg(ULONG msg, MPARAM mp1, MPARAM mp2)
{
    BOOL rc = FALSE;
    if (thrQueryID(&G_tiMediaThread))
    {
        if (G_hwndMediaObject)
            if (G_ulMMPM2Working == MMSTAT_WORKING)
                rc = WinPostMsg(G_hwndMediaObject, msg, mp1, mp2);
    }
    return (rc);
}

/*
 *@@ xmmIsPlayingSystemSound:
 *      returns TRUE if the Media thread is
 *      currently playing a system sound.
 *      This is useful for waiting until it's done.
 */

BOOL xmmIsBusy(VOID)
{
    return (G_usSoundDeviceID != 0);
}


