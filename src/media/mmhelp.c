
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
#define INCL_WINPOINTERS

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
#include "dlgids.h"                     // all the IDs that are shared with NLS
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

extern HWND G_hwndMediaObject;      // in mmthread.c

extern PXMMCDPLAYER G_pPlayer = NULL;      // points to application ptr!

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
 *      If *pusDeviceID is != 0, this simply
 *      returns TRUE. Otherwise, the device is
 *      opened from the specified parameters
 *      and *pusDeviceID receives the device ID.
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
                   PUSHORT pusDeviceID)   // in/out: MMPM/2 device ID
{
    BOOL brc = FALSE;
    if (*pusDeviceID == 0)
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
            *pusDeviceID = mop.usDeviceID;     // remember device ID

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
 *      *pusDeviceID to null.
 *
 *@@added V0.9.3 (2000-04-29) [umoeller]
 */

BOOL xmmCloseDevice(PUSHORT pusDeviceID)
{
    BOOL brc;
    ULONG ulrc;
    MCI_GENERIC_PARMS mgp = {0};
    ulrc = G_mciSendCommand(*pusDeviceID,
                            MCI_CLOSE,
                            MCI_WAIT,
                            &mgp,
                            0);
    if (LOUSHORT(ulrc) == MCIERR_SUCCESS)
    {
        if (xmmLockDevicesList())
        {
            lstRemoveItem(&G_lstOpenDevices,
                          (PVOID)*pusDeviceID);
            xmmUnlockDevicesList();
            brc = TRUE;
        }

        *pusDeviceID = 0;
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
            #ifdef __DEBUG__
                DosBeep(1000, 100);
            #endif
            xmmCloseDevice(&usDeviceID);
            lstRemoveNode(&G_lstOpenDevices, pNode);       // V0.9.7 (2000-12-21) [umoeller]
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
 *      This expects a static XMMCDPLAYER structure
 *      which should better be valid at all times
 *      because the XWP media thread will also
 *      access it to update status information.
 *
 *      Use xmmCDCloseDevice to close the device
 *      again.
 *
 *@@added V0.9.3 (2000-04-25) [umoeller]
 */

BOOL xmmCDOpenDevice(PXMMCDPLAYER pPlayer,
                     ULONG ulDeviceIndex)       // 0 for default
{
    BOOL brc = FALSE;
    if (xmmLockDevicesList())
    {
        if (pPlayer)
        {
            brc = xmmOpenDevice(MCI_DEVTYPE_CD_AUDIO,
                                ulDeviceIndex,
                                &pPlayer->usDeviceID);
            if (brc)
                G_pPlayer = pPlayer;
        }

        xmmUnlockDevicesList();
    }

    return (brc);
}

/*
 *@@ xmmCDCloseDevice:
 *      closes the CD player again.
 *
 *@@added V0.9.7 (2000-12-21) [umoeller]
 */

VOID xmmCDCloseDevice(PXMMCDPLAYER pPlayer)
{
    if (xmmLockDevicesList())
    {
        if (pPlayer)
        {
            xmmCloseDevice(&pPlayer->usDeviceID);

            if (pPlayer->aTocEntries)
                free(pPlayer->aTocEntries);
            pPlayer->aTocEntries = NULL;
            pPlayer->cTocEntries = 0;
        }

        G_pPlayer = NULL;

        xmmUnlockDevicesList();
    }
}

/*
 *@@ xmmCDGetTOC:
 *      retrieves the CD's table of contents.
 *      This operates synchronously.
 *
 *      This sets cTracks, aTocEntries and
 *      cTocEntries in the XMMCDPLAYER.
 *
 *      The MCI_TOC_REC structure is defined
 *      as follows:
 *
 +      typedef struct _MCI_TOC_REC {
 +          BYTE       TrackNum;     //  Track number.
 +          ULONG      ulStartAddr;  //  Starting address in MMTIME.
 +          ULONG      ulEndAddr;    //  Ending address in MMTIME.
 +          BYTE       Control;      //  Track control info.
 +          USHORT     usCountry;    //  Country.
 +          ULONG      ulOwner;      //  Owner.
 +          ULONG      ulSerialNum;  //  Serial number.
 +      } MCI_TOC_REC;
 +      typedef MCI_TOC_REC *PTOCREC;
 *
 *@@added V0.9.7 (2000-12-20) [umoeller]
 */

BOOL xmmCDGetTOC(PXMMCDPLAYER pPlayer)
{
    BOOL brc = FALSE;
    ULONG ulrc = 0;

    if (xmmLockDevicesList())
    {
        if ((pPlayer) && (pPlayer->usDeviceID))
        {
            MCI_TOC_PARMS mtp = {0};
            MCI_STATUS_PARMS msp = {0};

            // clear old toc
            if (pPlayer->aTocEntries)
                free(pPlayer->aTocEntries);
            pPlayer->aTocEntries = NULL;
            pPlayer->cTocEntries = 0;

            // get no. of tracks
            msp.ulItem = MCI_STATUS_NUMBER_OF_TRACKS;

            ulrc = G_mciSendCommand(pPlayer->usDeviceID,
                                    MCI_STATUS,
                                    MCI_WAIT
                                        | MCI_STATUS_ITEM, // msp.ulItem is valid
                                    &msp,
                                    0);                              // No user parm
            if (LOUSHORT(ulrc) == MCIERR_SUCCESS)
            {
                pPlayer->cTracks = LOUSHORT(msp.ulReturn);

                // now get the toc
                ulrc = G_mciSendCommand(pPlayer->usDeviceID,
                                        MCI_GETTOC,
                                        MCI_WAIT,
                                        &mtp,
                                        0);                              // No user parm
                if (mtp.ulBufSize)
                {
                    mtp.pBuf = malloc(mtp.ulBufSize);
                    ulrc = G_mciSendCommand(pPlayer->usDeviceID,
                                            MCI_GETTOC,
                                            MCI_WAIT,
                                            &mtp,
                                            0);                              // No user parm
                    if (LOUSHORT(ulrc) == MCIERR_SUCCESS)
                    {
                        pPlayer->aTocEntries = mtp.pBuf;
                        pPlayer->cTocEntries = mtp.ulBufSize / sizeof(MCI_TOC_REC);
                        brc = TRUE;
                    }
                    else
                    {
                        CHAR szError[1000];
                        G_mciGetErrorString(ulrc, szError, sizeof(szError));
                        _Pmpf((__FUNCTION__ ": DeviceID %d has error %d (\"%s\")",
                               pPlayer->usDeviceID, LOUSHORT(ulrc), szError));

                        free(mtp.pBuf);
                    }
                }
            }
            else
            {
                CHAR szError[1000];
                G_mciGetErrorString(ulrc, szError, sizeof(szError));
                _Pmpf((__FUNCTION__ ": DeviceID %d has error %d (\"%s\")",
                       pPlayer->usDeviceID, LOUSHORT(ulrc), szError));
            }
        }
        xmmUnlockDevicesList();
    }

    return (brc);
}

/*
 *@@ xmmCDQueryStatus:
 *      returns the status of a CD device opened
 *      with xmmCDOpenDevice.
 *
 *      This operates synchronously.
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

    if (xmmLockDevicesList())
    {
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
                ulReturn = LOUSHORT(msp.ulReturn);
            else
            {
                CHAR szError[1000];
                G_mciGetErrorString(ulrc, szError, sizeof(szError));
                _Pmpf((__FUNCTION__ ": DeviceID %d has error %d (\"%s\")",
                       usDeviceID, LOUSHORT(ulrc), szError));
            }
        }
        xmmUnlockDevicesList();
    }

    return (ulReturn);
}

/*
 *@@ xmmCDQueryCurrentTrack:
 *      returns the current track of the CD,
 *      no matter whether the CD is playing
 *      or not.
 *
 *      Returns 0 on errors.
 *
 *@@added V0.9.3 (2000-04-25) [umoeller]
 */

ULONG xmmCDQueryCurrentTrack(USHORT usDeviceID)
{
    ULONG ulReturn = 0,
          ulrc;

    if (xmmLockDevicesList())
    {
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
                ulReturn = LOUSHORT(msp.ulReturn);
            else
            {
                CHAR szError[1000];
                G_mciGetErrorString(ulrc, szError, sizeof(szError));
                _Pmpf((__FUNCTION__ ": DeviceID %d has error %d (\"%s\")",
                       usDeviceID, LOUSHORT(ulrc), szError));
            }
        }
        xmmUnlockDevicesList();
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

BOOL xmmCDPlay(PXMMCDPLAYER pPlayer,
               BOOL fShowWaitPointer)
{
    BOOL brc = FALSE;

    HPOINTER hptrOld = NULLHANDLE;
    if (fShowWaitPointer)
        hptrOld = winhSetWaitPointer();

    if (xmmLockDevicesList())
    {
        if ((pPlayer) && (pPlayer->usDeviceID))
        {
            ULONG ulrc = -1;

            ULONG ulNewStatus = 0,
                  ulNewTrack = 0;

            if (    (pPlayer->ulStatus != MCI_MODE_PAUSE)
                 && (pPlayer->ulStatus != MCI_MODE_PLAY)
               )
            {
                // device stopped:
                // start at beginning
                brc = xmmCDPlayTrack(pPlayer,
                                     1,
                                     fShowWaitPointer);
                                  // this also gets the TOC

                /* MCI_PLAY_PARMS mpp = {0};
                ULONG fl = 0;
                ulrc = G_mciSendCommand(pPlayer->usDeviceID,
                                        MCI_PLAY,
                                        MCI_WAIT,
                                        &mpp,      // from and to are ignored,
                                                   // since we've not set the flags
                                        0);
                ulNewStatus = MCI_MODE_PLAY;
                ulNewTrack = 1; */
            }
            else if (pPlayer->ulStatus == MCI_MODE_PAUSE)
            {
                // device paused:
                // resume
                MCI_GENERIC_PARMS mgp = {0};
                mgp.hwndCallback = G_hwndMediaObject;
                ulrc = G_mciSendCommand(pPlayer->usDeviceID,
                                        MCI_RESUME,
                                        MCI_WAIT,
                                        &mgp,
                                        0);
                ulNewStatus = MCI_MODE_PLAY;

                if (LOUSHORT(ulrc) == MCIERR_SUCCESS)
                {
                    pPlayer->ulStatus = ulNewStatus;
                    brc = TRUE;
                }
                else
                {
                    CHAR szError[1000];
                    G_mciGetErrorString(ulrc, szError, sizeof(szError));
                    _Pmpf((__FUNCTION__ ": DeviceID %d has error %d (\"%s\")",
                           pPlayer->usDeviceID, LOUSHORT(ulrc), szError));
                }
            }
        }
        xmmUnlockDevicesList();
    }

    if (hptrOld)
        WinSetPointer(HWND_DESKTOP, hptrOld);

    return (brc);
}

/*
 *@@ xmmCDPlayTrack:
 *      starts playing at the specified track.
 *
 *@@added V0.9.3 (2000-04-25) [umoeller]
 */

BOOL xmmCDPlayTrack(PXMMCDPLAYER pPlayer,
                    USHORT usTrack,
                    BOOL fShowWaitPointer)
{
    BOOL brc = FALSE;
    HPOINTER hptrOld = NULLHANDLE;
    if (fShowWaitPointer)
        hptrOld = winhSetWaitPointer();

    if (xmmLockDevicesList())
    {
        if ((pPlayer) && (pPlayer->usDeviceID))
        {
            ULONG ulrc = 0;

            if (pPlayer->aTocEntries == 0)
            {
                // ain't got no toc yet:
                // go get it
                xmmCDGetTOC(pPlayer);
            }

            if (pPlayer->aTocEntries)
            {
                // switch time format to tracks
                MCI_SET_PARMS msetp = {0};
                msetp.ulTimeFormat = MCI_FORMAT_TMSF;
                ulrc = G_mciSendCommand(pPlayer->usDeviceID,
                                        MCI_SET,
                                        MCI_WAIT
                                            | MCI_SET_TIME_FORMAT,
                                        &msetp,
                                        0);

                if (LOUSHORT(ulrc) == MCIERR_SUCCESS)
                {
                    MCI_PLAY_PARMS mpp = {0};

                    if (usTrack < 0)
                        usTrack = 1;
                    if (usTrack > pPlayer->cTracks)
                        return (FALSE);

                    TMSF_TRACK(mpp.ulFrom) = usTrack;
                    TMSF_MINUTE(mpp.ulFrom) = 0;
                    TMSF_SECOND(mpp.ulFrom) = 0;
                    TMSF_FRAME(mpp.ulFrom) = 0;
                    ulrc = G_mciSendCommand(pPlayer->usDeviceID,
                                            MCI_PLAY,
                                            MCI_FROM,
                                                // do not MCI_WAIT here,
                                                // this will never return
                                            &mpp,
                                            0);

                    _Pmpf(("NEXTTRACK MCI_SEEK ulrc: 0x%lX", ulrc));

                    if (LOUSHORT(ulrc) == MCIERR_SUCCESS)
                    {
                        pPlayer->ulStatus = MCI_MODE_PLAY;
                        pPlayer->usCurrentTrack = usTrack;
                        brc = TRUE;
                    }
                    else
                    {
                        CHAR szError[1000];
                        G_mciGetErrorString(ulrc, szError, sizeof(szError));
                        _Pmpf((__FUNCTION__ ": DeviceID %d has error %d (\"%s\")",
                               pPlayer->usDeviceID, LOUSHORT(ulrc), szError));
                    }
                }
                else
                {
                    CHAR szError[1000];
                    G_mciGetErrorString(ulrc, szError, sizeof(szError));
                    _Pmpf((__FUNCTION__ ": DeviceID %d has error %d (\"%s\")",
                           pPlayer->usDeviceID, LOUSHORT(ulrc), szError));
                }
            }
        }
        xmmUnlockDevicesList();
    }

    if (hptrOld)
        WinSetPointer(HWND_DESKTOP, hptrOld);

    return (brc);
}

/*
 *@@ xmmCDCalcTrack:
 *      calculates the track and the currently elapsed
 *      seconds in the track from the given ulMMTime.
 *
 *      The track (1-99) is returned.
 *
 *      Returns 0 if the track could not be found.
 *
 *      The caller must lock pPlayer first.
 */

ULONG xmmCDCalcTrack(PXMMCDPLAYER pPlayer,
                     ULONG ulMMTime,
                     PULONG pulSecondsInTrack)
{
    ULONG           ul;
    ULONG           ulTrack = 0,
                    ulMSInTrack = 0;
    MCI_TOC_REC     *pTocEntryFound = NULL;

    for (ul = 0;
         ul < pPlayer->cTocEntries;
         ul++)
    {
        MCI_TOC_REC     *pTocEntryThis = &pPlayer->aTocEntries[ul];
        if (    (pTocEntryThis->ulStartAddr < ulMMTime)
             && (pTocEntryThis->ulEndAddr > ulMMTime)
           )
        {
            pTocEntryFound = pTocEntryThis;
            break;
        }
    }

    if (pTocEntryFound)
    {
        ulTrack = pTocEntryFound->TrackNum;
        ulMSInTrack = MSECFROMMM(ulMMTime - pTocEntryFound->ulStartAddr);
    }

    *pulSecondsInTrack = ulMSInTrack / 1000;

    return (ulTrack);
}

/*
 *@@ xmmCDPositionAdvise:
 *      intializes position advise messages on the media
 *      thread. Gets called automatically from xmmCDPlayTrack
 *      and xmmCDPlay.
 *
 *      If hwndNotify is NULLHANDLE, notifiations are stopped.
 *
 *@@added V0.9.7 (2000-12-20) [umoeller]
 */

BOOL xmmCDPositionAdvise(PXMMCDPLAYER pPlayer,
                         HWND hwndNotify,
                         ULONG ulNotifyMsg)
{
    BOOL brc = FALSE;
    if (xmmLockDevicesList())
    {
        if ((pPlayer) && (pPlayer->usDeviceID))
        {
            ULONG ulrc = 0,
                  fl = MCI_WAIT;
            MCI_POSITION_PARMS mpp = {0};
            MCI_SET_PARMS msetp = {0};

            if (!hwndNotify)
                pPlayer->hwndNotify = NULLHANDLE;

            // switch time format to milliseconds
            msetp.ulTimeFormat = MCI_FORMAT_MMTIME;
            ulrc = G_mciSendCommand(pPlayer->usDeviceID,
                                    MCI_SET,
                                    MCI_WAIT
                                        | MCI_SET_TIME_FORMAT,
                                    &msetp,
                                    0);

            mpp.hwndCallback = G_hwndMediaObject;
            mpp.ulUnits = MSECTOMM(200);

            if (hwndNotify)
                fl |= MCI_SET_POSITION_ADVISE_ON;
            else
                fl |= MCI_SET_POSITION_ADVISE_OFF;

            ulrc = G_mciSendCommand(pPlayer->usDeviceID,
                                    MCI_SET_POSITION_ADVISE,
                                    fl,
                                    &mpp,
                                    0);

            if (LOUSHORT(ulrc) == MCIERR_SUCCESS)
            {
                pPlayer->fPositionAdvising = (hwndNotify != NULLHANDLE);
                pPlayer->hwndNotify = hwndNotify;
                pPlayer->ulNotifyMsg = ulNotifyMsg;
            }
            else
            {
                CHAR szError[1000];
                G_mciGetErrorString(ulrc, szError, sizeof(szError));
                _Pmpf((__FUNCTION__ ": DeviceID %d has error %d (\"%s\")",
                       pPlayer->usDeviceID, LOUSHORT(ulrc), szError));
                pPlayer->hwndNotify = hwndNotify;
            }
        }
        xmmUnlockDevicesList();
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

BOOL xmmCDPause(PXMMCDPLAYER pPlayer)
{
    BOOL brc = FALSE;

    if (xmmLockDevicesList())
    {
        if ((pPlayer) && (pPlayer->usDeviceID))
        {
            ULONG ulrc;
            MCI_GENERIC_PARMS mgp = {0};
            mgp.hwndCallback = G_hwndMediaObject;
            ulrc = G_mciSendCommand(pPlayer->usDeviceID,
                                    MCI_PAUSE,
                                    MCI_WAIT,
                                    &mgp,
                                    0);
            if (LOUSHORT(ulrc) == MCIERR_SUCCESS)
                brc = TRUE;
        }
        xmmUnlockDevicesList();
    }

    return (brc);
}

/*
 *@@ xmmCDStop:
 *      stops the CD playing and closes the device
 *      by calling xmmCDCloseDevice.
 *
 *@@added V0.9.7 (2000-12-20) [umoeller]
 */

BOOL xmmCDStop(PXMMCDPLAYER pPlayer)
{
    BOOL brc = FALSE;

    if (xmmLockDevicesList())
    {
        if (pPlayer)
        {
            if (pPlayer->usDeviceID)
            {
                xmmCDPositionAdvise(pPlayer, NULLHANDLE, 0);

                if (xmmCDQueryStatus(pPlayer->usDeviceID) == MCI_MODE_PLAY)
                {
                    MCI_GENERIC_PARMS mgp = {0};
                    ULONG ulrc;
                    mgp.hwndCallback = G_hwndMediaObject;
                    ulrc = G_mciSendCommand(pPlayer->usDeviceID,
                                            MCI_STOP,
                                            MCI_WAIT,
                                            &mgp,
                                            0);
                    if (LOUSHORT(ulrc) == MCIERR_SUCCESS)
                        brc = TRUE;
                    else
                    {
                        CHAR szError[1000];
                        G_mciGetErrorString(ulrc, szError, sizeof(szError));
                        _Pmpf((__FUNCTION__ ": DeviceID %d has error %d (\"%s\")",
                               pPlayer->usDeviceID, LOUSHORT(ulrc), szError));
                    }
                }
            }

            xmmCDCloseDevice(pPlayer);
        }
        xmmUnlockDevicesList();
    }

    return (brc);
}

/*
 *@@ xmmCDEject:
 *      ejects the CD and closes the device
 *      by calling xmmCDCloseDevice.
 *
 *@@added V0.9.7 (2000-12-20) [umoeller]
 */

BOOL xmmCDEject(PXMMCDPLAYER pPlayer)
{
    BOOL brc = FALSE;

    if (xmmLockDevicesList())
    {
        if (pPlayer)
        {
            if (pPlayer->usDeviceID)
            {
                ULONG ulrc;
                MCI_SET_PARMS msetp = {0};
                xmmCDPositionAdvise(pPlayer, NULLHANDLE, 0);

                ulrc = G_mciSendCommand(pPlayer->usDeviceID,
                                        MCI_SET,
                                        MCI_SET_DOOR_OPEN,
                                        &msetp,
                                        0);
                if (LOUSHORT(ulrc) == MCIERR_SUCCESS)
                    brc = TRUE;
                else
                {
                    CHAR szError[1000];
                    G_mciGetErrorString(ulrc, szError, sizeof(szError));
                    _Pmpf((__FUNCTION__ ": DeviceID %d has error %d (\"%s\")",
                           pPlayer->usDeviceID, LOUSHORT(ulrc), szError));
                }
            }

            xmmCDCloseDevice(pPlayer);
        }
        xmmUnlockDevicesList();
    }

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
            MCI_DEVTYPE_VIDEOTAPE, "Video tape",
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
    ULONG   ulStringID = 0;
    // const char *prc = "Unknown";

    switch (ulDeviceType)
    {
        case MCI_DEVTYPE_VIDEOTAPE:
            ulStringID = ID_MMSI_DEVTYPE_VIDEOTAPE;  // pszDevTypeVideotape
        break;

        case MCI_DEVTYPE_VIDEODISC:
            ulStringID = ID_MMSI_DEVTYPE_VIDEODISC;  // pszDevTypeVideodisc
        break;

        case MCI_DEVTYPE_CD_AUDIO:
            ulStringID = ID_MMSI_DEVTYPE_CD_AUDIO;  // pszDevTypeCDAudio
        break;

        case MCI_DEVTYPE_DAT:
            ulStringID = ID_MMSI_DEVTYPE_DAT;  // pszDevTypeDAT
        break;

        case MCI_DEVTYPE_AUDIO_TAPE:
            ulStringID = ID_MMSI_DEVTYPE_AUDIO_TAPE;  // pszDevTypeAudioTape
        break;

        case MCI_DEVTYPE_OTHER:
            ulStringID = ID_MMSI_DEVTYPE_OTHER;  // pszDevTypeOther
        break;

        case MCI_DEVTYPE_WAVEFORM_AUDIO:
            ulStringID = ID_MMSI_DEVTYPE_WAVEFORM_AUDIO;  // pszDevTypeWaveformAudio
        break;

        case MCI_DEVTYPE_SEQUENCER:
            ulStringID = ID_MMSI_DEVTYPE_SEQUENCER;  // pszDevTypeSequencer
        break;

        case MCI_DEVTYPE_AUDIO_AMPMIX:
            ulStringID = ID_MMSI_DEVTYPE_AUDIO_AMPMIX;  // pszDevTypeAudioAmpmix
        break;

        case MCI_DEVTYPE_OVERLAY:
            ulStringID = ID_MMSI_DEVTYPE_OVERLAY;  // pszDevTypeOverlay
        break;

        case MCI_DEVTYPE_ANIMATION:
            ulStringID = ID_MMSI_DEVTYPE_ANIMATION;  // pszDevTypeAnimation
        break;

        case MCI_DEVTYPE_DIGITAL_VIDEO:
            ulStringID = ID_MMSI_DEVTYPE_DIGITAL_VIDEO;  // pszDevTypeDigitalVideo
        break;

        case MCI_DEVTYPE_SPEAKER:
            ulStringID = ID_MMSI_DEVTYPE_SPEAKER;  // pszDevTypeSpeaker
        break;

        case MCI_DEVTYPE_HEADPHONE:
            ulStringID = ID_MMSI_DEVTYPE_HEADPHONE;  // pszDevTypeHeadphone
        break;

        case MCI_DEVTYPE_MICROPHONE:
            ulStringID = ID_MMSI_DEVTYPE_MICROPHONE;  // pszDevTypeMicrophone
        break;

        case MCI_DEVTYPE_MONITOR:
            ulStringID = ID_MMSI_DEVTYPE_MONITOR;  // pszDevTypeMonitor
        break;

        case MCI_DEVTYPE_CDXA:
            ulStringID = ID_MMSI_DEVTYPE_CDXA;  // pszDevTypeCDXA
        break;

        case MCI_DEVTYPE_FILTER:
            ulStringID = ID_MMSI_DEVTYPE_FILTER;  // pszDevTypeFilter
        break;

        case MCI_DEVTYPE_TTS:
            ulStringID = ID_MMSI_DEVTYPE_TTS;  // pszDevTypeTTS
        break;
    }

    if (ulStringID)
        return (cmnGetString(ulStringID));

    return ("Unknown");
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


