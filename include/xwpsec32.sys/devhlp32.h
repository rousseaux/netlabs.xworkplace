//
// $Header$
//

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

#ifndef __DevHlp32_h
#define __DevHlp32_h


#ifndef __infoseg_h
#define __infoseg_h
#include <infoseg.h>
#endif

#ifdef __IBMC__
#include <stdarg.h>
#else
typedef char *va_list;
#endif

#pragma pack(1)
struct PageList {
    unsigned long physaddr;
    unsigned long size;
};
#pragma pack()

#pragma pack(1)
struct ddtable {
    USHORT reserved[3];
    PTR16  idc_entry;
    USHORT idc_ds;
};
#pragma pack()

/* ******************************************************************
 *
 *   Prototypes
 *
 ********************************************************************/

extern int DH32ENTRY DevHlp32_AllocGDTSelector(
                                               unsigned short *psel,
                                               int count
                                              );

extern int DH32ENTRY DevHlp32_AttachDD(char *ddname, struct ddtable *table);

#define VERIFY_READONLY 0
#define VERIFY_READWRITE 1

extern int DH32ENTRY2 DevHlp32_Beep(
                                ULONG           ulFreq,       /* ebp + 8  */
                                ULONG           ulDuration    /* ebp + 12 */
                                  );

extern int DH32ENTRY DevHlp32_CloseEventSem(
                                  unsigned long handle        /* ebp + 8  */
                                     );

extern int DH32ENTRY2 DevHlp32_EOI(
                           unsigned short interrupt_level   /* ax */
                                  );

extern int DH32ENTRY2 DevHlp32_FreeGDTSelector(
                                               unsigned short sel      /* ax */
                                              );

#define DHGETDOSV_SYSINFOSEG            1
#define DHGETDOSV_LOCINFOSEG            2
#define DHGETDOSV_VECTORSDF             4
#define DHGETDOSV_VECTORREBOOT          5
#define DHGETDOSV_YIELDFLAG             7                            /*@V76282*/
#define DHGETDOSV_TCYIELDFLAG           8                            /*@V76282*/
#define DHGETDOSV_DOSCODEPAGE           11                           /*@V76282*/
#define DHGETDOSV_INTERRUPTLEV          13
#define DHGETDOSV_DEVICECLASSTABLE      14                           /*@V76282*/
#define DHGETDOSV_DMQSSELECTOR          15                           /*@V76282*/
#define DHGETDOSV_APMINFO               16                           /*@V76282*/

extern int DH32ENTRY DevHlp32_GetDosVar(
                                        int     index,      /* ebp + 8  */
                                        PTR16  *value,      /* ebp + 12 */
                                        int     member      /* ebp + 16 */
                                       );

extern int DH32ENTRY DevHlp32_GetInfoSegs(
                    struct InfoSegGDT **ppSysInfoSeg,  /* ebp + 8  */
                    struct InfoSegLDT **ppLocInfoSeg   /* ebp + 12 */
                   );

extern void DH32ENTRY DevHlp32_InternalError(
                                 char *msg,     /* ebp + 8  */
                                 int   len      /* ebp + 12 */
                                    );

extern int DH32ENTRY DevHlp32_LinToPageList(
                                void            *lin,         /* ebp + 8  */
                                unsigned long    size,        /* ebp + 12 */
                                struct PageList *pages,        /* ebp + 16 */
                                unsigned long   *nr_pages    /* ebp + 20 */
                               );

extern int DH32ENTRY DevHlp32_OpenEventSem(
                                  unsigned long handle        /* ebp + 8  */
                                     );

extern int DH32ENTRY DevHlp32_PageListToLin(
                                            unsigned long     size,        /* ebp + 8  */
                                            struct PageList  *pPageList,   /* ebp + 12 */
                                            void            **pLin         /* ebp + 16 */
                                           );

extern int DH32ENTRY DevHlp32_PostEventSem(
                                  unsigned long handle        /* ebp + 8  */
                                     );

#define WAIT_IS_INTERRUPTABLE      0
#define WAIT_IS_NOT_INTERRUPTABLE  1

#define WAIT_INTERRUPTED           0x8003
#define WAIT_TIMED_OUT             0x8001

extern int DH32ENTRY DevHlp32_ProcBlock(
                          unsigned long  eventid,       /* bp + 8  */
                          long           timeout,       /* bp + 12 */
                          short          interruptible  /* bp + 16 */
                         );

extern int DH32ENTRY DevHlp32_ProcRun(
                                  unsigned long eventid        /* ebp + 8  */
                                     );

extern int DH32ENTRY DevHlp32_ResetEventSem(
                                  unsigned long handle,       /* ebp + 8  */
                                  int *nposts                 /* ebp + 12 */
                                     );

extern int DH32ENTRY DevHlp32_SaveMessage(
                              char *msg,        /* ebp + 8  */
                              int   len     /* ebp + 12 */
                                 );

// #define DHSEC_GETEXPORT  0x48a78df8
// #define DHSEC_SETIMPORT  0x73ae3627
// #define DHSEC_GETINFO    0x33528882

extern int DH32ENTRY DevHlp32_Security(
                       unsigned long   func,     /* ebp + 8  */
                                       void           *ptr       /* ebp + 12 */
                                      );

/* extern int DH32ENTRY DevHlp32_SemClearRAM(PULONG pulSemaphore);

extern int DH32ENTRY DevHlp32_SemRequestRAM(PULONG pulSemaphore,
                                            ULONG ulSemTimeout); */

/* extern int DH32ENTRY DevHlp32_ofs( ULONG ulPtrToDATA16
                                ); */

extern int DH32ENTRY DevHlp32_SemClearRam1(VOID);

extern int DH32ENTRY DevHlp32_SemRequestRam1(ULONG ulSemTimeout);

extern int DH32ENTRY DevHlp32_setIRQ(
                                     unsigned short offset_irq,     /* ebp + 8  */
                             unsigned short interrupt_level,    /* ebp + 12 */
                             unsigned short sharing_flag,   /* ebp + 16 */
                             unsigned short data16_segment  /* ebp + 20 */
                                    );

extern int DH32ENTRY DevHlp32_UnSetIRQ(
                                       unsigned short interrupt_level,  /* ebp + 8  */
                               unsigned short data16_segment    /* ebp + 12 */
                                      );

extern int DH32ENTRY DevHlp32_VerifyAccess( PTR16 address,        /* ebp + 8 */
                                            unsigned short size,  /* ebp + 12 */
                                            int flags                 /* ebp + 16  */
                                          );

extern int DH32ENTRY DevHlp32_VirtToLin(
                                        PTR16  virt, // [ebp + 8]
                                        void **plin  // [ebp + 12]
                                       );

#define VMDHA_16M               0x0001
#define VMDHA_FIXED             0x0002
#define VMDHA_SWAP              0x0004
#define VMDHA_CONTIG            0x0008
#define VMDHA_PHYS              0x0010
#define VMDHA_PROCESS           0x0020
#define VMDHA_SGSCONT           0x0040
#define VMDHA_RESERVE           0x0100
#define VMDHA_USEHIGHMEM        0x0800

#define VMDHA_NOPHYSADDR        0xFFFFFFFF

extern int DH32ENTRY DevHlp32_VMAlloc(
                                      unsigned long  Length,      /* ebp + 8  */
                                      unsigned long  PhysAddr,    /* ebp + 12 */
                                      unsigned long  Flags,       /* ebp + 16 */
                                      void         **LinAddr      /* ebp + 20 */
                                     );

extern int _Optlink DevHlp32_VMFree(
                                    void *addr        /* eax */
                                   );

#define VMDHL_WRITE             0x0008
#define VMDHL_LONG              0x0010
#define VMDHL_VERIFY            0x0020

extern int DH32ENTRY DevHlp32_VMLock(
            unsigned long   flags,
            void           *addr,
            unsigned long   length,
            void           *pPageList,
            void           *pLockHandle,
            unsigned long *pPageListCount
           );

#define VMDHPG_READONLY         0x0000                               /*@V76282*/
#define VMDHPG_WRITE            0x0001                               /*@V76282*/

extern int DH32ENTRY DevHlp32_VMProcessToGlobal(ULONG ulFlags,
                                                PVOID plinProcess,
                                                ULONG Length,
                                                PVOID *pplinGlobal);

extern int DH32ENTRY DevHlp32_VMUnlock(
                                       void *pLockHandle   /* ebp + 8 */
                                      );

extern void DH32ENTRY DevHlp32_Yield(void);

extern int DH32ENTRY sec32_attach_ses (void *SecHlp);

extern int DH32ENTRY __vsprintf(char *buf, const char *fmt, va_list args);
extern unsigned long DH32ENTRY __strtoul (const char *string, char **end_ptr, int radix);
extern long DH32ENTRY __strtol (const char *string, char **end_ptr, int radix);
extern long DH32ENTRY __atol (const char *string);
extern char * DH32ENTRY __strupr (char *string);
extern char * DH32ENTRY __strpbrk (const char *string1, const char *string2);
extern int DH32ENTRY __fnmatch (const char *mask, const char *name, int flags);

extern void * _Pascal VDHQueryLin(PTR16);

#endif

