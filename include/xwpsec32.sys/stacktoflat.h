
// 32 bits OS/2 device driver and IFS support. Provides 32 bits kernel
// services (DevHelp) and utility functions to 32 bits OS/2 ring 0 code
// (device drivers and installable file system drivers).
// Copyright (C) 1995, 1996, 1997  Matthieu WILLM (willm@ibm.net)
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#ifndef __StackToFlat_h
#define __StackToFlat_h

#ifndef USE_VDH_HELP
/*
 * This points to the kernel stack to FLAT conversion value for the
 * current thread. FLAT:ESP = SP + *TKSSBase
 * Its pointer is obtained from the kernel at INIT time in the DosTable.
 */
extern void *TKSSBase;

/*
 * This routine is used to convert a ring 0 stack based address to its
 * FLAT equivalent. This is to avoid SS!=DS problems when passing such
 * addresses to subroutines. It is A LOT faster than using VirtToLin
 * to do the conversion.
 */
INLINE void *__StackToFlat(void *addr) {
    return (void *)((unsigned long)addr + *(unsigned long *)TKSSBase);
}

#else

/*
 * The other solution is to use _TKSSBase as exported in MVDM.DLL (Virtual device
 * driver helpers); not I don't know if it is always available (PROTECTONLY=YES may
 * disable it in some cases ???)
 */
#pragma library("vdh.lib")

/*
 * _TKSSBase is exported in MVDM.DLL
 */
extern void *_TKSSBase;

INLINE void *__StackToFlat(void *addr) {
    return (void *)((unsigned long)addr + (unsigned long)_TKSSBase);
}
#endif


#endif
