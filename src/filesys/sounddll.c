
/*
 *@@sourcefile sounddll.c:
 *      this is the source code for SOUND.DLL which is used by
 *      XWorkplace to play the new system sounds introduced with V0.70.
 *
 *      This file was called sound.c before V0.9.0.
 *
 *      Unfortunately, with V0.82 I found out that the reason that
 *      XWorkplace wouldn't install on some systems was that the main
 *      XFLDR.DLL imported a few MMPM/2 library functions, and the
 *      OS/2 DLL loader then would refuse to load XWorkplace if MMPM/2
 *      was not installed. As a result, XWorkplace class registration
 *      failed (naturally, without any meaningful error messages.
 *      FALSE isn't that meaningful, in my view.)
 *
 *      So this file is new with V0.82. The system sounds are still
 *      played by fnwpQuickObject (kernel.c), but all the function
 *      calls to MMPM/2 have been moved into this file so that the main
 *      DLL does not have to be linked against the MMPM/2 libraries.
 *
 *      At WPS bootup, XWorkplace attempts to load this DLL and to
 *      resolve the exports. See krnInitializeXWorkplace.
 *
 *      The smart thing about this DLL is that if any error occurs
 *      loading the DLL, XWorkplace will automatically disable the new
 *      system sounds. That is, if MMPM/2 is not installed (loading
 *      of SOUND.DLL then fails because the imports to MMPM/2 cannot
 *      be resolved) or if SOUND.DLL is simply not found. As a
 *      result, you can simply delete SOUND.DLL to disable sounds.
 *
 *      Notes:
 *
 *      1) The functions in this file are not intended to be called
 *         directly to play sounds because access to multimedia
 *         devices needs to be serialized using MMPM/2 messages.
 *         This is what the Quick thread does. Use xthrPostQuickMsg
 *         instead, which will post QM_PLAYSOUND to the quick thread.
 *
 *      2) THIS LIBRARY IS NOT THREAD-SAFE. The makefile (main.mak)
 *         compiles this source code as a subsystem library to reduce
 *         the size of the DLL. As a result, SOUND.DLL must only
 *         be called by the Quick thread. See the VAC++ docs for more
 *         information on subsystem libraries.
 *
 *      For these reasons, simply use xthrPostQuickMsg to play
 *      a system sound, which is bomb-proof.
 *
 *      With V0.82, I have also fixed some problems WRT multiple
 *      applications playing sounds at the same time. Error codes
 *      are now properly checked for and if another application
 *      requests use of the audio device, XWorkplace will stop its
 *      own playing and release the device.
 *
 *@@header "filesys\sounddll.h"
 */

/*
 *      Copyright (C) 1997-99 Ulrich Mîller.
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

// basic OS/2 includes
#include <os2.h>

// multimedia includes
#define INCL_MCIOS2
// #define INCL_OS2MM -- commented out, V0.9.0
#include <os2me.h>

// generic headers
#include "setup.h"                      // code generation and debugging options

#include <string.h>

/*
 *@@ sndOpenSound:
 *      this is called by the Quick thread (fnwpQuickObject)
 *      when it receives QM_PLAYSOUND to start playing a sound.
 *      In order to store the current status of
 *      the sound device, the Quick thread uses a global
 *      usDeviceID variable in kernel.c, which is modified
 *      by this func. "Device" in this context means an
 *      individual sound file.
 *
 *      We also need the object window of the calling
 *      thread (which is hwndQuickObject from kernel.c)
 *      to inform MMPM/2 where to post notification
 *      messages.
 *
 *      We will only attempt to open the sound file here
 *      and then return; we will _not_ yet play the sound
 *      because we will need to wait for the MM_MCIPASSDEVICE
 *      message (which will be posted to hwndQuickObject, the Quick
 *      thread object window) for checking whether the device
 *      is actually available.
 *
 *      Note: This function is _not_ prototyped in sound.h
 *      because its address is resolved dynamically by
 *      krnInitializeXWorkplace.
 *
 *@@changed V0.9.0 [umoeller]: added EXPENTRY to the function header (thanks, RÅdiger Ihle)
 */

VOID EXPENTRY sndOpenSound(HWND hwndObject,       // in: Quick thread object wnd
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
        mgp.hwndCallback = hwndObject;
        mciSendCommand(*pusDeviceID,
                    MCI_STOP,
                    MCI_WAIT,
                    &mgp,
                    0);
        mciSendCommand(*pusDeviceID,
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
    ulrc = mciSendCommand(0,
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
        mciSendCommand(mop.usDeviceID,
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
 *@@ sndPlaySound:
 *      this is called by the Quick thread (fnwpQuickObject)
 *      when it receives MM_MCIPASSDEVICE with MCI_GAINING_USE
 *      set, i.e. the device is ready to play. So that's
 *      what we'll do here.
 *
 *      Note: This function is _not_ prototyped in sound.h
 *      because its address is resolved dynamically by
 *      krnInitializeXWorkplace.
 *
 *@@changed V0.9.0 [umoeller]: added EXPENTRY to the function header (thanks, RÅdiger Ihle)
 */

VOID EXPENTRY sndPlaySound(HWND hwndObject,     // in: Quick thread object wnd
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
    mciSendCommand(*pusDeviceID,
                   MCI_SET,
                   MCI_WAIT | MCI_SET_AUDIO |
                   MCI_SET_VOLUME,
                   &msp, 0);

    // play and request MM_MCINOTIFY msg to
    // the Quick thread object window
    mpp.hwndCallback = (HWND)hwndObject;
    mciSendCommand(*pusDeviceID,
                    MCI_PLAY,
                    MCI_NOTIFY,
                    (PVOID)&mpp,
                    0);
}

/*
 *@@ sndStopSound:
 *      this is called by the Quick thread (fnwpQuickObject)
 *      in two situations:
 *      1)  MMPM/2 is done playing our sound, i.e.
 *          upon receiving MM_MCINOTIFY;
 *      2)  another application requests the waveform
 *          device for playing, i.e. upon receiving
 *          MM_MCIPASSDEVICE with MCI_LOSING_USE set.
 *
 *      In both situations, we need to close our
 *      device.
 *
 *      Note: This function is _not_ prototyped in sound.h
 *      because its address is resolved dynamically by
 *      krnInitializeXWorkplace.
 *
 *@@changed V0.9.0 [umoeller]: added EXPENTRY to the function header (thanks, RÅdiger Ihle)
 */

VOID EXPENTRY sndStopSound(PUSHORT pusDeviceID)
{
    MCI_GENERIC_PARMS mgp;
    // stop playing the sound (this will probably do
    // nothing if the sound is already done with)
    mciSendCommand(*pusDeviceID,
                MCI_STOP,
                MCI_WAIT,
                &mgp,
                0);
    // close the device
    mciSendCommand(*pusDeviceID,
                MCI_CLOSE,
                MCI_WAIT,
                &mgp,
                0);
    // set the Quick thread's device ID to 0 so
    // we know that we're not currently playing
    // anything
    *pusDeviceID = 0;
}

