
/*
 *@@sourcefile sounddll.h:
 *      header file for sound support. This is used by both sounddll.c
 *      (for SOUND.DLL) and various source files for the main DLL.
 *
 *      This file was called sound.h before V0.9.0.
 *
 *      See sounddll.c for details.
 *
 *@@include #define INCL_WINWINDOWMGR       // needed for WM_USER
 *@@include #include <os2.h>
 *@@include #include "filesys\sounddll.h"
 */

/*
 *      Copyright (C) 1997-99 Ulrich M”ller.
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

#ifndef SOUND_HEADER_INCLUDED
    #define SOUND_HEADER_INCLUDED

    // Quick thread msgs (QM_xxx) for xfdesk.c
    #define QM_BOOTUPSTATUS             WM_USER+140
    #define QM_PLAYSOUND                WM_USER+141
    #define QM_PLAYSYSTEMSOUND          WM_USER+142

    // function prototypes for SOUND.DLL imports;
    // APIENTRY has been added to the following prototypes with V0.9.0:
    typedef VOID APIENTRY (FN_SNDOPENSOUND)(HWND, PUSHORT, PSZ);
    typedef FN_SNDOPENSOUND *PFN_SNDOPENSOUND;

    typedef VOID APIENTRY (FN_SNDPLAYSOUND)(HWND, PUSHORT, ULONG);
    typedef FN_SNDPLAYSOUND *PFN_SNDPLAYSOUND;

    typedef VOID APIENTRY (FN_SNDSTOPSOUND)(PUSHORT);
    typedef FN_SNDSTOPSOUND *PFN_SNDSTOPSOUND;

#endif
