
/*
 *@@sourcefile mmhelp.c:
 *      this has generic multimedia helpers which offer
 *      frequently used functions in a more convinent
 *      interface than the ugly mciSendCommand, where
 *      type-safety is somewhat difficult.
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
#define INCL_MMIOOS2
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
#include "shared\common.h"              // the majestic XWorkplace include file

#include "media\media.h"                // XWorkplace multimedia support

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

// list of all open MMPM/2 devices: this list
// holds the plain USHORT device ID's
HMTX        G_hmtxOpenDevices = NULLHANDLE;
LINKLIST    G_lstOpenDevices;

/* ******************************************************************
 *                                                                  *
 *   Device manager                                                 *
 *                                                                  *
 ********************************************************************/

/*
 *@@ xmmLockDevicesList:
 *      internal helper to make the device functions
 *      thread safe. This requests a mutex semaphore.
 *      xmmUnlockDevicesList must be called to release it.
 *
 *@@added V0.9.3 (2000-04-29) [umoeller]
 */

BOOL xmmLockDevicesList(VOID)
{
    if (G_hmtxOpenDevices == NULLHANDLE)
    {
        // first call:
        DosCreateMutexSem(NULL,
                          &G_hmtxOpenDevices,
                          0,
                          FALSE);
        lstInit(&G_lstOpenDevices,
                FALSE);     // we have USHORT's on the list, so no free
    }

    return (WinRequestMutexSem(G_hmtxOpenDevices, 5000) == 0);  // APIRET NO_ERROR
}

/*
 *@@ xmmUnlockDevicesList:
 *      releases the mutex semaphore requested
 *      by xmmLockDevicesList.
 *
 *@@added V0.9.3 (2000-04-29) [umoeller]
 */

BOOL xmmUnlockDevicesList(VOID)
{
    return (DosReleaseMutexSem(G_hmtxOpenDevices) == 0); // APIRET NO_ERROR
}

/*
 *@@ xmmOpenDevice:
 *      opens any multimedia device.
 *      If *pusMMDeviceID is != 0, this simply
 *      returns TRUE. Otherwise, the device is
 *      opened from the specified parameters
 *      and *pusMMDeviceID receives the device ID.
 *      As a result, if TRUE is returned, you can
 *      always be sure that the device is open.
 *
 *      Use xmmCloseDevice to close the device again.
 *
 *      This keeps a list of all open devices so that
 *      xmmCleanup can clean up on shutdown.
 *
 *@@added V0.9.3 (2000-04-29) [umoeller]
 */

BOOL xmmOpenDevice(USHORT usDeviceTypeID,   // in: MCI_DEVTYPE_* ID
                   USHORT usDeviceIndex,    // in: device index (0 for default)
                   PUSHORT pusMMDeviceID)   // in/out: MMPM/2 device ID
{
    BOOL brc = FALSE;
    if (*pusMMDeviceID == 0)
    {
        // device not opened yet:
        ULONG ulrc;
        MCI_OPEN_PARMS  mop;
        memset(&mop, 0, sizeof(mop));

        // set device type (MCI_OPEN_TYPE_ID):
        // low word is standard device ID,
        // high word is index; if 0, default device will be opened.
        mop.pszDeviceType = (PSZ)(MPFROM2SHORT(usDeviceTypeID,
                                               usDeviceIndex));

        ulrc = G_mciSendCommand(0,  // device ID, ignored on MCI_OPEN
                                MCI_OPEN,
                                MCI_WAIT
                                    // | MCI_OPEN_ELEMENT
                                    // | MCI_READONLY
                                    | MCI_OPEN_TYPE_ID // pszDeviceType is valid
                                    | MCI_OPEN_SHAREABLE,
                                &mop,
                                0);                              // No user parm

        if (LOUSHORT(ulrc) == MCIERR_SUCCESS)
        {
            // successful: go on
            *pusMMDeviceID = mop.usDeviceID;     // remember device ID

            if (xmmLockDevicesList())
            {
                lstAppendItem(&G_lstOpenDevices,
                              (PVOID)mop.usDeviceID);
                xmmUnlockDevicesList();
            }

            brc = TRUE;
        }
        else
        {
            // no success:
            // we need to close the device again!
            // Even if MCI_OPEN fails, MMPM/2 has allocated resources.
            MCI_GENERIC_PARMS mgp = {0};
            memset(&mgp, 0, sizeof(mgp));
            G_mciSendCommand(mop.usDeviceID,
                             MCI_CLOSE,
                             MCI_WAIT,
                             &mgp,
                             0);
        }
    }
    else
        // device already open:
        brc = TRUE;

    return (brc);
}

/*
 *@@ xmmCloseDevice:
 *      closes a device opened by xmmOpenDevice and sets
 *      *pusMMDeviceID to null.
 *
 *@@added V0.9.3 (2000-04-29) [umoeller]
 */

BOOL xmmCloseDevice(PUSHORT pusMMDeviceID)
{
    BOOL brc;
    ULONG ulrc;
    MCI_GENERIC_PARMS mgp = {0};
    ulrc = G_mciSendCommand(*pusMMDeviceID,
                            MCI_CLOSE,
                            MCI_WAIT,
                            &mgp,
                            0);
    if (LOUSHORT(ulrc) == MCIERR_SUCCESS)
    {
        if (xmmLockDevicesList())
        {
            lstRemoveItem(&G_lstOpenDevices,
                          (PVOID)*pusMMDeviceID);
            xmmUnlockDevicesList();
            brc = TRUE;
        }

        *pusMMDeviceID = 0;
    }

    return brc;
}

/*
 *@@ xmmCleanup:
 *      closes all leftover open devices. This is
 *      used during XShutdown because otherwise
 *      the WPS hangs after the WPS restart. So
 *      much for exit-list cleanup, IBM.
 *
 *@@added V0.9.3 (2000-04-29) [umoeller]
 */

VOID xmmCleanup(VOID)
{
    if (xmmLockDevicesList())
    {
        // always delete the first node, because
        // xmmCloseDevice modifies the list
        PLISTNODE pNode;
        while (pNode = lstQueryFirstNode(&G_lstOpenDevices))
        {
            USHORT usDeviceID = (USHORT)(pNode->pItemData);
            DosBeep(1000, 100);
            xmmCloseDevice(&usDeviceID);
        }

        xmmUnlockDevicesList();
    }
}

/* ******************************************************************
 *                                                                  *
 *   Sound helpers                                                  *
 *                                                                  *
 ********************************************************************/

/*
 *@@ xmmOpenSound:
 *      this is called by the Media thread (xmm_fnwpMediaObject)
 *      when it receives QM_PLAYSOUND to start playing a sound.
 *      In order to store the current status of
 *      the sound device, this func needs and modifies a MMPM/2
 *      device ID. "Device" in this context means an individual
 *      sound file.
 *
 *      We also need the object window of the calling
 *      thread (which is xmm_fnwpMediaObject)
 *      to inform MMPM/2 where to post notification
 *      messages.
 *
 *      We will only attempt to open the sound file here
 *      and then return; we will _not_ yet play the sound
 *      because we will need to wait for the MM_MCIPASSDEVICE
 *      message (which will be posted to xmm_fnwpMediaObject, the Media
 *      thread object window) for checking whether the device
 *      is actually available.
 *
 *@@changed V0.9.3 (2000-04-26) [umoeller]: this was in SOUND.DLL previously
 */

VOID xmmOpenSound(HWND hwndObject,       // in: Media thread object wnd
                  PUSHORT pusDeviceID,   // in/out: "device" ID (= sound file).
                         // This is != 0 if we're already playing something
                  PSZ pszFile)           // in: sound file to play
{
    ULONG           ulrc;
    MCI_OPEN_PARMS  mop;
    MCI_GENERIC_PARMS mgp;

    if (*pusDeviceID)
    {
        // if the device ID is != 0, that means
        // that we're already playing: abort
        // current job and close device first
        memset(&mgp, 0, sizeof(mgp));
        mgp.hwndCallback = hwndObject;
        G_mciSendCommand(*pusDeviceID,
                         MCI_STOP,
                         MCI_WAIT,
                         &mgp,
                         0);
        G_mciSendCommand(*pusDeviceID,
                         MCI_CLOSE,
                         MCI_WAIT,
                         &mgp,
                         0);
        #ifdef DEBUG_SOUNDS
            _Pmpf(("SOUND.DLL: Stopped old sound"));
        #endif
    }

    #ifdef DEBUG_SOUNDS
        _Pmpf(("SOUND.DLL: Opening sound file"));
    #endif

    // open new device
    memset(&mop, 0, sizeof(mop));
    mop.hwndCallback = hwndObject;       // callback hwnd
    // mop.usDeviceID = 0;                  // we don't have one yet
    // mop.pszDeviceType = NULL;            // using default device type
    mop.pszElementName = pszFile;        // file name to open

    // now we open the file; note the flags:
    // --  MCI_READONLY is important for two reasons:
    //     1)   otherwise, if the file doesn't exist, MMPM/2
    //          will create a temporary file for writing into,
    //          which costs too much time;
    //     2)   if opening fails, the file would be blocked for
    //          the rest of this session.
    // --  MCI_OPEN_SHAREABLE allows us to share the device
    //     with other applications and get MM_MCIPASSDEVICE msgs.
    // From now on, the sound file will be our "device".
    ulrc = G_mciSendCommand(0,
                            MCI_OPEN,
                            MCI_WAIT | MCI_OPEN_ELEMENT | MCI_READONLY | MCI_OPEN_SHAREABLE,
                            &mop,
                            0);                              // No user parm

    if (LOUSHORT(ulrc) == MCIERR_SUCCESS)
    {
        // successful: go on
        *pusDeviceID = mop.usDeviceID;     // remember device ID

        #ifdef DEBUG_SOUNDS
            _Pmpf(("  SOUND.DLL: Device opened"));
        #endif
    }
    else
    {
        // no success:
        // we need to close the device again then, because
        // otherwise we won't be able to play that sound again
        // (remember: "device" is the sound file here)
        memset(&mgp, 0, sizeof(mgp));
        mgp.hwndCallback = hwndObject;
        G_mciSendCommand(mop.usDeviceID,
                         MCI_CLOSE,
                         MCI_WAIT,
                         &mgp,
                         0);
        #ifdef DEBUG_SOUNDS
        {
            CHAR szError[1000];
            mciGetErrorString(ulrc, szError, sizeof(szError));
            _Pmpf(("  DeviceID %d has error %d (\"%s\")",
                   mop.usDeviceID, ulrc, szError));
        }
        #endif

        *pusDeviceID = 0;
    }
}

/*
 *@@ xmmPlaySound:
 *      this is called by the Media thread (xmm_fnwpMediaObject)
 *      when it receives MM_MCIPASSDEVICE with MCI_GAINING_USE
 *      set, i.e. the device is ready to play. So playing
 *      is what we'll do here.
 *
 *@@changed V0.9.3 (2000-04-26) [umoeller]: this was in SOUND.DLL previously
 */

VOID xmmPlaySound(HWND hwndObject,     // in: Media thread object wnd
                  PUSHORT pusDeviceID, // in: "device" ID (= sound file)
                  ULONG ulVolume)      // in: volume for sound (0-100)
{
    MCI_PLAY_PARMS  mpp;
    MCI_SET_PARMS   msp;

    #ifdef DEBUG_SOUNDS
        _Pmpf(("  SOUND.DLL: Playing sound"));
    #endif

    // set the volume for this sound
    msp.ulLevel = ulVolume;
    msp.ulAudio = MCI_SET_AUDIO_ALL;
    G_mciSendCommand(*pusDeviceID,
                     MCI_SET,
                     MCI_WAIT | MCI_SET_AUDIO |
                     MCI_SET_VOLUME,
                     &msp, 0);

    // play and request MM_MCINOTIFY msg to
    // the Media thread object window
    mpp.hwndCallback = (HWND)hwndObject;
    G_mciSendCommand(*pusDeviceID,
                     MCI_PLAY,
                     MCI_NOTIFY,
                     (PVOID)&mpp,
                     0);
}

/*
 *@@ xmmStopSound:
 *      this is called by the Media thread (xmm_fnwpMediaObject)
 *      in two situations:
 *
 *      1)  MMPM/2 is done playing our sound, i.e.
 *          upon receiving MM_MCINOTIFY;
 *
 *      2)  another application requests the waveform
 *          device for playing, i.e. upon receiving
 *          MM_MCIPASSDEVICE with MCI_LOSING_USE set.
 *
 *      In both situations, we need to close our
 *      device.
 *
 *@@changed V0.9.3 (2000-04-26) [umoeller]: this was in SOUND.DLL previously
 */

VOID xmmStopSound(PUSHORT pusDeviceID)
{
    MCI_GENERIC_PARMS mgp;
    // stop playing the sound (this will probably do
    // nothing if the sound is already done with)
    memset(&mgp, 0, sizeof(mgp));
    G_mciSendCommand(*pusDeviceID,
                     MCI_STOP,
                     MCI_WAIT,
                     &mgp,
                     0);
    // close the device
    memset(&mgp, 0, sizeof(mgp));
    G_mciSendCommand(*pusDeviceID,
                     MCI_CLOSE,
                     MCI_WAIT,
                     &mgp,
                     0);
    // set the Media thread's device ID to 0 so
    // we know that we're not currently playing
    // anything
    *pusDeviceID = 0;
}

/* ******************************************************************
 *                                                                  *
 *   CD player helpers                                              *
 *                                                                  *
 ********************************************************************/

/*
 *@@ xmmCDOpenDevice:
 *      opens the CD device by calling xmmOpenDevice
 *      and prepares it for playing.
 *
 *      Returns TRUE if the CD device is open,
 *      either because the device has already been
 *      opened or has been opened by this call.
 *
 *      This function has no counterpart. Use
 *      the generic xmmCloseDevice to close the
 *      device again.
 *
 *@@added V0.9.3 (2000-04-25) [umoeller]
 */

BOOL xmmCDOpenDevice(PUSHORT pusMMDeviceID) // in/out: CD device ID
{
    BOOL brc = FALSE;
    ULONG ulrc;

    if (*pusMMDeviceID == 0)
    {
        // device not opened yet:
        if (xmmOpenDevice(MCI_DEVTYPE_CD_AUDIO,
                          0,    // default device
                          pusMMDeviceID))
        {
            // successfully opened:
            // switch time format to tracks
            MCI_SET_PARMS msetp = {0};
            msetp.ulTimeFormat = MCI_FORMAT_TMSF;
            ulrc = G_mciSendCommand(*pusMMDeviceID,
                                    MCI_SET,
                                    MCI_WAIT
                                        | MCI_SET_TIME_FORMAT,
                                    &msetp,
                                    0);

            _Pmpf(("NEXTTRACK MCI_SET ulrc: 0x%lX", ulrc));

            brc = TRUE;
        }
    }
    else
        // device already open:
        brc = TRUE;

    return (brc);
}

/*
 *@@ xmmCDQueryStatus:
 *      returns the status of a CD device opened
 *      with xmmCDOpenDevice.
 *
 *      Returns one of the following:
 *      -- 0: invalid device.
 *      -- MCI_MODE_NOT_READY
 *      -- MCI_MODE_PAUSE
 *      -- MCI_MODE_PLAY
 *      -- MCI_MODE_STOP
 *
 *@@added V0.9.3 (2000-04-25) [umoeller]
 */

ULONG xmmCDQueryStatus(USHORT usDeviceID)
{
    ULONG ulReturn = 0,
          ulrc;

    if (usDeviceID)
    {
        MCI_STATUS_PARMS msp = {0};

        msp.ulItem = MCI_STATUS_MODE;

        ulrc = G_mciSendCommand(usDeviceID,
                                MCI_STATUS,
                                MCI_WAIT
                                    | MCI_STATUS_ITEM, // msp.ulItem is valid
                                &msp,
                                0);                              // No user parm
        if (LOUSHORT(ulrc) == MCIERR_SUCCESS)
        {
            ulReturn = LOUSHORT(msp.ulReturn);
        }
    }

    return (ulReturn);
}

/*
 *@@ xmmCDQueryLastTrack:
 *      returns the no. of the last track of the CD
 *      which is in the drive (equals the no. of
 *      tracks on the CD).
 *
 *@@added V0.9.3 (2000-04-25) [umoeller]
 */

ULONG xmmCDQueryLastTrack(USHORT usDeviceID)
{
    ULONG ulReturn = 0,
          ulrc;

    if (usDeviceID)
    {
        MCI_STATUS_PARMS msp = {0};

        msp.ulItem = MCI_STATUS_NUMBER_OF_TRACKS;

        ulrc = G_mciSendCommand(usDeviceID,
                                MCI_STATUS,
                                MCI_WAIT
                                    | MCI_STATUS_ITEM, // msp.ulItem is valid
                                &msp,
                                0);                              // No user parm
        if (LOUSHORT(ulrc) == MCIERR_SUCCESS)
        {
            ulReturn = LOUSHORT(msp.ulReturn);
        }
    }

    return (ulReturn);
}

/*
 *@@ xmmCDQueryCurrentTrack:
 *      returns the current track of the CD,
 *      no matter whether the CD is playing
 *      or not.
 *
 *@@added V0.9.3 (2000-04-25) [umoeller]
 */

ULONG xmmCDQueryCurrentTrack(USHORT usDeviceID)
{
    ULONG ulReturn = 0,
          ulrc;

    if (usDeviceID)
    {
        MCI_STATUS_PARMS msp = {0};

        msp.ulItem = MCI_STATUS_CURRENT_TRACK;

        ulrc = G_mciSendCommand(usDeviceID,
                                MCI_STATUS,
                                MCI_WAIT
                                    | MCI_STATUS_ITEM, // msp.ulItem is valid
                                &msp,
                                0);                              // No user parm
        if (LOUSHORT(ulrc) == MCIERR_SUCCESS)
        {
            ulReturn = LOUSHORT(msp.ulReturn);
        }
    }

    return (ulReturn);
}

/*
 *@@ xmmCDPlay:
 *      starts playing if the CD is currently
 *      stopped or paused. Otherwise, this does
 *      nothing and returns FALSE.
 *
 *@@added V0.9.3 (2000-04-27) [umoeller]
 */

BOOL xmmCDPlay(USHORT usDeviceID)
{
    BOOL brc = FALSE;
    ULONG ulStatus = xmmCDQueryStatus(usDeviceID);
    ULONG ulrc = -1;

    if (ulStatus == MCI_MODE_STOP)
    {
        // device stopped:
        // start at beginning
        MCI_PLAY_PARMS mpp = {0};
        ulrc = G_mciSendCommand(usDeviceID,
                                MCI_PLAY,
                                0, // MCI_WAIT,
                                &mpp,      // from and to are ignored,
                                           // since we've not set the flags
                                0);
    }
    else if (ulStatus == MCI_MODE_PAUSE)
    {
        // device paused:
        // resume
        MCI_GENERIC_PARMS mgp = {0};
        ulrc = G_mciSendCommand(usDeviceID,
                                MCI_RESUME,
                                0, // MCI_WAIT,
                                &mgp,
                                0);
    }

    if (LOUSHORT(ulrc) == MCIERR_SUCCESS)
        brc = TRUE;

    return (brc);
}

/*
 *@@ xmmCDPlayTrack:
 *      starts playing at the specified track.
 *
 *@@added V0.9.3 (2000-04-25) [umoeller]
 */

BOOL xmmCDPlayTrack(USHORT usDeviceID,
                    USHORT usTrack)
{
    BOOL brc = FALSE;
    ULONG ulrc = 0;
    MCI_PLAY_PARMS mpp = {0};

    if (usTrack < 0)
        usTrack = 1;
    if (usTrack > xmmCDQueryLastTrack(usDeviceID))
        return (FALSE);

    TMSF_TRACK(mpp.ulFrom) = usTrack;
    TMSF_MINUTE(mpp.ulFrom) = 0;
    TMSF_SECOND(mpp.ulFrom) = 0;
    TMSF_FRAME(mpp.ulFrom) = 0;
    ulrc = G_mciSendCommand(usDeviceID,
                            MCI_PLAY,
                            MCI_FROM,
                            &mpp,
                            0);

    _Pmpf(("NEXTTRACK MCI_SEEK ulrc: 0x%lX", ulrc));

    if (LOUSHORT(ulrc) == MCIERR_SUCCESS)
        brc = TRUE;
    else
    {
        CHAR szError[1000];
        G_mciGetErrorString(ulrc, szError, sizeof(szError));
        _Pmpf(("  DeviceID %d has error %d (\"%s\")",
               usDeviceID, ulrc, szError));
    }

    return (brc);
}

/*
 *@@ xmmCDPause:
 *      pauses the CD, which should be currently
 *      playing.
 *
 *@@added V0.9.3 (2000-04-27) [umoeller]
 */

BOOL xmmCDPause(USHORT usDeviceID)
{
    BOOL brc = FALSE;
    MCI_GENERIC_PARMS mgp = {0};
    ULONG ulrc = G_mciSendCommand(usDeviceID,
                                  MCI_PAUSE,
                                  MCI_WAIT,
                                  &mgp,
                                  0);
    if (LOUSHORT(ulrc) == MCIERR_SUCCESS)
        brc = TRUE;
    return (brc);
}

/* ******************************************************************
 *                                                                  *
 *   MMPM/2 configuration queries                                   *
 *                                                                  *
 ********************************************************************/

/* typedef struct _DEVICETYPE
{
    ULONG   ulDeviceTypeID;
    PSZ     pszDeviceType;
} DEVICETYPE, *PDEVICETYPE; */

/* DEVICETYPE aDeviceTypes[] =
        {
            MCI_DEVTYPE_VIDEOTAPE, "Video tape",        // ### NLS!
            MCI_DEVTYPE_VIDEODISC, "Video disc",
            MCI_DEVTYPE_CD_AUDIO, "CD Audio",
            MCI_DEVTYPE_DAT, "DAT",
            MCI_DEVTYPE_AUDIO_TAPE, "Audio tape",
            MCI_DEVTYPE_OTHER, "Other",
            MCI_DEVTYPE_WAVEFORM_AUDIO, "Wave audio",
            MCI_DEVTYPE_SEQUENCER, "Sequencer",
            MCI_DEVTYPE_AUDIO_AMPMIX, "Ampmix",
            MCI_DEVTYPE_OVERLAY, "Overlay",
            MCI_DEVTYPE_ANIMATION, "Animation",
            MCI_DEVTYPE_DIGITAL_VIDEO, "Digital video",
            MCI_DEVTYPE_SPEAKER, "Speaker",
            MCI_DEVTYPE_HEADPHONE, "Headphone",
            MCI_DEVTYPE_MICROPHONE, "Microphone",
            MCI_DEVTYPE_MONITOR, "Monitor",
            MCI_DEVTYPE_CDXA, "CDXA",
            MCI_DEVTYPE_FILTER, "Filter",       // Warp 4 only
            MCI_DEVTYPE_TTS, "Text-to-speech"
        }; */

ULONG aulDeviceTypes[] =
        {
            MCI_DEVTYPE_VIDEOTAPE,
            MCI_DEVTYPE_VIDEODISC,
            MCI_DEVTYPE_CD_AUDIO,
            MCI_DEVTYPE_DAT,
            MCI_DEVTYPE_AUDIO_TAPE,
            MCI_DEVTYPE_OTHER,
            MCI_DEVTYPE_WAVEFORM_AUDIO,
            MCI_DEVTYPE_SEQUENCER,
            MCI_DEVTYPE_AUDIO_AMPMIX,
            MCI_DEVTYPE_OVERLAY,
            MCI_DEVTYPE_ANIMATION,
            MCI_DEVTYPE_DIGITAL_VIDEO,
            MCI_DEVTYPE_SPEAKER,
            MCI_DEVTYPE_HEADPHONE,
            MCI_DEVTYPE_MICROPHONE,
            MCI_DEVTYPE_MONITOR,
            MCI_DEVTYPE_CDXA,
            MCI_DEVTYPE_FILTER, // Warp 4 only
            MCI_DEVTYPE_TTS
        };

/*
 *@@ GetDeviceTypeName:
 *      returns an NLS string describing the
 *      input MCI_DEVTYPE_* device type.
 *
 *@@added V0.9.7 (2000-11-30) [umoeller]
 */

const char* GetDeviceTypeName(ULONG ulDeviceType)
{
    const char *prc = "Unknown";
    PNLSSTRINGS     pNLSStrings = cmnQueryNLSStrings();
    switch (ulDeviceType)
    {
        case MCI_DEVTYPE_VIDEOTAPE:
            prc = pNLSStrings->pszDevTypeVideotape;
        break;

        case MCI_DEVTYPE_VIDEODISC:
            prc = pNLSStrings->pszDevTypeVideodisc;
        break;

        case MCI_DEVTYPE_CD_AUDIO:
            prc = pNLSStrings->pszDevTypeCDAudio;
        break;

        case MCI_DEVTYPE_DAT:
            prc = pNLSStrings->pszDevTypeDAT;
        break;

        case MCI_DEVTYPE_AUDIO_TAPE:
            prc = pNLSStrings->pszDevTypeAudioTape;
        break;

        case MCI_DEVTYPE_OTHER:
            prc = pNLSStrings->pszDevTypeOther;
        break;

        case MCI_DEVTYPE_WAVEFORM_AUDIO:
            prc = pNLSStrings->pszDevTypeWaveformAudio;
        break;

        case MCI_DEVTYPE_SEQUENCER:
            prc = pNLSStrings->pszDevTypeSequencer;
        break;

        case MCI_DEVTYPE_AUDIO_AMPMIX:
            prc = pNLSStrings->pszDevTypeAudioAmpmix;
        break;

        case MCI_DEVTYPE_OVERLAY:
            prc = pNLSStrings->pszDevTypeOverlay;
        break;

        case MCI_DEVTYPE_ANIMATION:
            prc = pNLSStrings->pszDevTypeAnimation;
        break;

        case MCI_DEVTYPE_DIGITAL_VIDEO:
            prc = pNLSStrings->pszDevTypeDigitalVideo;
        break;

        case MCI_DEVTYPE_SPEAKER:
            prc = pNLSStrings->pszDevTypeSpeaker;
        break;

        case MCI_DEVTYPE_HEADPHONE:
            prc = pNLSStrings->pszDevTypeHeadphone;
        break;

        case MCI_DEVTYPE_MICROPHONE:
            prc = pNLSStrings->pszDevTypeMicrophone;
        break;

        case MCI_DEVTYPE_MONITOR:
            prc = pNLSStrings->pszDevTypeMonitor;
        break;

        case MCI_DEVTYPE_CDXA:
            prc = pNLSStrings->pszDevTypeCDXA;
        break;

        case MCI_DEVTYPE_FILTER:
            prc = pNLSStrings->pszDevTypeFilter;
        break;

        case MCI_DEVTYPE_TTS:
            prc = pNLSStrings->pszDevTypeTTS;
        break;
    }

    return (prc);
}

/*
 *@@ xmmQueryDevices:
 *      returns an array of XMMDEVICE structures describing
 *      all available MMPM/2 devices on your system.
 *      *pcDevices receives the no. of items in the array
 *      (not the array size!). Use xmmFreeDevices to clean up.
 *
 *@@added V0.9.3 (2000-04-29) [umoeller]
 *@@changed V0.9.7 (2000-11-30) [umoeller]: added NLS for device types
 */

PXMMDEVICE xmmQueryDevices(PULONG pcDevices)
{
    #define XMM_QDEV_ALLOC_DELTA    50

    ULONG   cDevices = 0,
            ulDevicesAllocated = XMM_QDEV_ALLOC_DELTA;
    PXMMDEVICE paDevices = malloc(sizeof(XMMDEVICE) * XMM_QDEV_ALLOC_DELTA);

    ULONG   ulDevTypeThis = 0;
    for (ulDevTypeThis = 0;
         ulDevTypeThis < sizeof(aulDeviceTypes) / sizeof(aulDeviceTypes[0]); // array item count
         ulDevTypeThis++)
    {
        ULONG   ulrc,
                ulCurrentDeviceIndex = 0;
        BOOL    fContinue = FALSE;

        // _Pmpf(("Opening type %d", aDeviceTypes[ulDevTypeThis].ulDeviceTypeID));

        // now, for this device type, enumerate
        // devices; start with device "1", because
        // "0" means default device
        ulCurrentDeviceIndex = 1;
        fContinue = TRUE;
        while (fContinue)
        {
            USHORT  usDeviceID = 0;

            if (xmmOpenDevice(aulDeviceTypes[ulDevTypeThis],    // device type
                              ulCurrentDeviceIndex,
                              &usDeviceID))
            {
                // get info
                MCI_INFO_PARMS minfop = {0};
                minfop.pszReturn = paDevices[cDevices].szInfo;
                minfop.ulRetSize = sizeof(paDevices[cDevices].szInfo);
                ulrc = G_mciSendCommand(usDeviceID,
                                        MCI_INFO,
                                        MCI_WAIT | MCI_INFO_PRODUCT,
                                        &minfop,
                                        0);
                if (LOUSHORT(ulrc) != MCIERR_SUCCESS)
                    paDevices[cDevices].szInfo[0] = 0;

                _Pmpf(("  Opened!"));
                if (cDevices >= ulDevicesAllocated)
                {
                    // if we had space for 10 devices and current device is 10,
                    // allocate another 10
                    paDevices = realloc(paDevices,
                                        (   sizeof(XMMDEVICE)
                                          * (ulDevicesAllocated + XMM_QDEV_ALLOC_DELTA)
                                       ));
                    ulDevicesAllocated += XMM_QDEV_ALLOC_DELTA;
                }

                paDevices[cDevices].ulDeviceType = aulDeviceTypes[ulDevTypeThis];
                paDevices[cDevices].pcszDeviceType = GetDeviceTypeName(aulDeviceTypes[ulDevTypeThis]);
                paDevices[cDevices].ulDeviceIndex = ulCurrentDeviceIndex;

                cDevices++;

                // next device index for this device type
                ulCurrentDeviceIndex++;
            }
            else
                // stop looping for this device type
                fContinue = FALSE;

            // even if open failed, we need to close the
            // device again
            xmmCloseDevice(&usDeviceID);

        } // while (TRUE);
    } // end for (ulDevTypeThis = 0;

    *pcDevices = cDevices;
    return (paDevices);
}

/*
 *@@ xmmFreeDevices:
 *      frees resources allocated by xmmQueryDevices.
 *
 *@@added V0.9.3 (2000-04-29) [umoeller]
 */

BOOL xmmFreeDevices(PXMMDEVICE paDevices)
{
    free(paDevices);
    return (TRUE);
}


