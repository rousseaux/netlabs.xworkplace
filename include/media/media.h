
/*
 *@@sourcefile media.h:
 *      shared header file for XWorkplace multimedia support.
 *
 *@@include #include <wpobject.h>           // for SOM support functions
 *@@include #include "media\media.h"
 */

/*
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

#ifndef MTHREAD_HEADER_INCLUDED
    #define MTHREAD_HEADER_INCLUDED

    /********************************************************************
     *
     *   MMPM/2 imports
     *
     ********************************************************************/

    // prototypes:

    #ifdef INCL_MCIOS2
        // mciSendCommand
        typedef ULONG APIENTRY FNTD_MCISENDCOMMAND(USHORT,
                                                   USHORT,
                                                   ULONG,
                                                   PVOID,
                                                   USHORT);
        // mciGetErrorString
        typedef ULONG APIENTRY FNTD_MCIGETERRORSTRING(ULONG,
                                                      PSZ,
                                                      USHORT);
        #ifdef INCL_MMIOOS2
            #ifdef INCL_MMIO_CODEC

                // mmioIniFileCODEC
                typedef ULONG APIENTRY FNTD_MMIOINIFILECODEC(PCODECINIFILEINFO,
                                                             ULONG);
                typedef ULONG APIENTRY FNTD_MMIOQUERYCODECNAMELENGTH(PCODECINIFILEINFO,
                                                                     PULONG);
                typedef ULONG APIENTRY FNTD_MMIOQUERYCODECNAME(PCODECINIFILEINFO,
                                                               PSZ,
                                                               PULONG);

                typedef ULONG APIENTRY FNTD_MMIOINIFILEHANDLER(PMMINIFILEINFO,
                                                               ULONG);


                typedef ULONG APIENTRY FNTD_MMIOQUERYFORMATCOUNT(PMMFORMATINFO,
                                                                 PLONG,
                                                                 ULONG,
                                                                 ULONG);
                typedef ULONG APIENTRY FNTD_MMIOGETFORMATS(PMMFORMATINFO,
                                                           LONG,
                                                           PVOID,
                                                           PLONG,
                                                           ULONG,
                                                           ULONG);
                typedef ULONG APIENTRY FNTD_MMIOGETFORMATNAME(PMMFORMATINFO,
                                                              PSZ,
                                                              PLONG,
                                                              ULONG,
                                                              ULONG);

                // global variables with resolved functions;
                // use these ONLY if xmmQueryStatus returns MMSTAT_RUNNING!!!
                extern FNTD_MCISENDCOMMAND          *G_mciSendCommand;
                extern FNTD_MCIGETERRORSTRING       *G_mciGetErrorString;

                extern FNTD_MMIOINIFILECODEC        *G_mmioIniFileCODEC;
                extern FNTD_MMIOQUERYCODECNAMELENGTH *G_mmioQueryCODECNameLength;
                extern FNTD_MMIOQUERYCODECNAME      *G_mmioQueryCODECName;
                extern FNTD_MMIOINIFILEHANDLER      *G_mmioIniFileHandler;
                extern FNTD_MMIOQUERYFORMATCOUNT    *G_mmioQueryFormatCount;
                extern FNTD_MMIOGETFORMATS          *G_mmioGetFormats;
                extern FNTD_MMIOGETFORMATNAME       *G_mmioGetFormatName;
            #endif
        #endif
    #endif

    /********************************************************************
     *
     *   Media thread messages
     *
     ********************************************************************/

    // flags for XMM_CDPLAYER mp1
    #define XMMCD_PLAY                  1
    #define XMMCD_STOP                  2
    #define XMMCD_PAUSE                 3
    #define XMMCD_TOGGLEPLAY            4
    #define XMMCD_NEXTTRACK             5
    #define XMMCD_PREVTRACK             6
    #define XMMCD_EJECT                 7

    #define XMM_PLAYSOUND                (WM_USER+251)

    #define XMM_PLAYSYSTEMSOUND          (WM_USER+252)

    #define XMM_CDPLAYER                 (WM_USER+253)

    // MMPM/2 status flags in KERNELGLOBALS.ulMMPM2Working;
    // these reflect the status of SOUND.DLL.
    // If this is anything other than MMSTAT_WORKING, sounds
    // are disabled.
    #define MMSTAT_UNKNOWN             0        // initial value
    #define MMSTAT_WORKING             1        // SOUND.DLL is working
    #define MMSTAT_MMDIRNOTFOUND       2        // MMPM/2 directory not found
    #define MMSTAT_DLLNOTFOUND         3        // MMPM/2 DLLs not found
    #define MMSTAT_IMPORTSFAILED       4        // MMPM/2 imports failed
    #define MMSTAT_CRASHED             5        // SOUND.DLL crashed, sounds disabled
    #define MMSTAT_DISABLED            6        // explicitly disabled in startup panic dlg

    /* ******************************************************************
     *
     *   Device manager
     *
     ********************************************************************/

    BOOL xmmLockDevicesList(VOID);

    BOOL xmmUnlockDevicesList(VOID);

    BOOL xmmOpenDevice(USHORT usDeviceTypeID,
                       USHORT usDeviceIndex,
                       PUSHORT pusMMDeviceID);

    BOOL xmmCloseDevice(PUSHORT pusMMDeviceID);

    VOID xmmCleanup(VOID);

    /* ******************************************************************
     *
     *   Sound helpers
     *
     ********************************************************************/

    VOID xmmOpenSound(HWND hwndObject,
                      PUSHORT pusDeviceID,
                      PSZ pszFile);

    VOID xmmPlaySound(HWND hwndObject,
                      PUSHORT pusDeviceID,
                      ULONG ulVolume);

    VOID xmmStopSound(PUSHORT pusDeviceID);

    /* ******************************************************************
     *
     *   CD player helpers
     *
     ********************************************************************/

    BOOL xmmCDOpenDevice(PUSHORT pusMMDeviceID);

    ULONG xmmCDQueryStatus(USHORT usDeviceID);

    ULONG xmmCDQueryLastTrack(USHORT usDeviceID);

    ULONG xmmCDQueryCurrentTrack(USHORT usDeviceID);

    BOOL xmmCDPlay(USHORT usDeviceID);

    BOOL xmmCDPlayTrack(USHORT usDeviceID,
                        USHORT usTrack);

    BOOL xmmCDPause(USHORT usDeviceID);

    /* ******************************************************************
     *
     *   Media thread interface
     *
     ********************************************************************/

    BOOL xmmInit(VOID);

    VOID xmmDisable(VOID);

    ULONG xmmQueryStatus(VOID);

    BOOL xmmPostMediaMsg(ULONG msg, MPARAM mp1, MPARAM mp2);

    BOOL xmmIsBusy(VOID);

    /* ******************************************************************
     *
     *   MMPM/2 configuration queries
     *
     ********************************************************************/

    #ifndef MCI_DEVTYPE_FILTER
        #define MCI_DEVTYPE_FILTER              18
    #endif
    #ifndef MCI_DEVTYPE_TTS
        #define MCI_DEVTYPE_TTS                 19
    #endif

    /*
     *@@ XMMDEVICE:
     *      describes a single MMPM/2 device returned
     *      by xmmQueryDevices.
     *
     *@@added V0.9.3 (2000-04-29) [umoeller]
     */

    typedef struct _XMMDEVICE
    {
        ULONG   ulDeviceType;       // MCI_DEVTYPE_* identifier
        PSZ     pszDeviceType;      // NLS description (composed by xmmQueryDevices)
        ULONG   ulDeviceIndex;      // index (1 or higher)
        CHAR    szInfo[200];        // return value of szInfo
    } XMMDEVICE, *PXMMDEVICE;

    PXMMDEVICE xmmQueryDevices(PULONG pcDevices);

    BOOL xmmFreeDevices(PXMMDEVICE paDevices);

    /* ******************************************************************
     *
     *   Volume Control (mmvolume.c)
     *
     ********************************************************************/

    #ifdef SOM_WPObject_h
        HWND xmmCreateVolumeView(WPObject *somSelf,
                                 HWND hwndCnr,
                                 ULONG ulView);
    #endif

#endif
