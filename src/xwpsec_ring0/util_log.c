
/*
 *@@sourcefile audit.c:
 *      various audit code.
 */

/*
 *      Copyright (C) 2000 Ulrich M”ller.
 *      Based on the MWDD32.SYS example sources,
 *      Copyright (C) 1995, 1996, 1997  Matthieu Willm (willm@ibm.net).
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, in version 2 as it comes in the COPYING
 *      file of the XWorkplace main distribution.
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 */

#ifdef __IBMC__
#pragma strings(readonly)
#endif

#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_NOPMAPI
#include <os2.h>

#include <string.h>
#include <stdarg.h>

#include "xwpsec32.sys\types.h"
#include "xwpsec32.sys\StackToFlat.h"
#include "xwpsec32.sys\DevHlp32.h"
#include "xwpsec32.sys\reqpkt32.h"
#include "xwpsec32.sys\SecHlp.h"

#include "xwpsec32.sys\xwpsec_types.h"
#include "xwpsec32.sys\xwpsec_callbacks.h"

/* ******************************************************************
 *                                                                  *
 *   Global variables                                               *
 *                                                                  *
 ********************************************************************/

extern CHAR     G_szLogFile[CCHMAXPATH] = "";
        // file name of log file. This is copied from the driver
        // command line by sec32_init_base(). If the first byte
        // is zero (that is, no log file was specified), no
        // logging will be done by the audit functions.

extern CHAR     G_szScratchBuf[1000] = "";
        // generic temporary buffer for composing strings etc.

extern ULONG    G_ulLogSFN = 0;
        // system file number (SFN) of log file, which is opened
        // by utilOpenLog().

ULONG           G_ulOffset = 0;
        // write offset into log file; initially 0, raised with
        // each write. We need to store this because SecHlpWrite
        // doesn't maintain a current file position.

// two global pointers to GDT and LDT info segs which can be used
// with DevHlp32_GetInfoSegs to avoid stack thunking.
extern struct InfoSegGDT *G_pGDT = 0;      // OS/2 global infoseg
extern struct InfoSegLDT *G_pLDT = 0;      // OS/2 local  infoseg

/*
    struct InfoSegGDT {

    // Time (offset 0x00)

    unsigned long   SIS_BigTime;    // Time from 1-1-1970 in seconds
    unsigned long   SIS_MsCount;    // Freerunning milliseconds counter
    unsigned char   SIS_HrsTime;    // Hours
    unsigned char   SIS_MinTime;    // Minutes
    unsigned char   SIS_SecTime;    // Seconds
    unsigned char   SIS_HunTime;    // Hundredths of seconds
    unsigned short  SIS_TimeZone;   // Timezone in min from GMT (Set to EST)
    unsigned short  SIS_ClkIntrvl;  // Timer interval (units=0.0001 secs)

    // Date (offset 0x10)

    unsigned char   SIS_DayDate;    // Day-of-month (1-31)
    unsigned char   SIS_MonDate;    // Month (1-12)
    unsigned short  SIS_YrsDate;    // Year (>= 1980)
    unsigned char   SIS_DOWDate;    // Day-of-week (1-1-80 = Tues = 3)

    // Version (offset 0x15)

    unsigned char   SIS_VerMajor;   // Major version number
    unsigned char   SIS_VerMinor;   // Minor version number
    unsigned char   SIS_RevLettr;   // Revision letter

    // System Status (offset 0x18)

    // XLATOFF
    #ifdef  OLDVER
    unsigned char   CurScrnGrp;     // Fgnd screen group #
    #else
    // XLATON
    unsigned char   SIS_CurScrnGrp; // Fgnd screen group #
    // XLATOFF
    #endif
    // XLATON
    unsigned char   SIS_MaxScrnGrp; // Maximum number of screen groups
    unsigned char   SIS_HugeShfCnt; // Shift count for huge segments
    unsigned char   SIS_ProtMdOnly; // Protect-mode-only indicator
    unsigned short  SIS_FgndPID;    // Foreground process ID

    // Scheduler Parms (offset 0x1E)

    unsigned char   SIS_Dynamic;    // Dynamic variation flag (1=enabled)
    unsigned char   SIS_MaxWait;    // Maxwait (seconds)
    unsigned short  SIS_MinSlice;   // Minimum timeslice (milliseconds)
    unsigned short  SIS_MaxSlice;   // Maximum timeslice (milliseconds)

    // Boot Drive (offset 0x24)

    unsigned short  SIS_BootDrv;    // Drive from which system was booted

    // RAS Major Event Code Table (offset 0x26)

    unsigned char   SIS_mec_table[32]; // Table of RAS Major Event Codes (MECs)

    // Additional Session Data (offset 0x46)

    unsigned char   SIS_MaxVioWinSG;  // Max. no. of VIO windowable SG's
    unsigned char   SIS_MaxPresMgrSG; // Max. no. of Presentation Manager SG's

    // Error logging Information (offset 0x48)

    unsigned short  SIS_SysLog;     // Error Logging Status

    // Additional RAS Information (offset 0x4A)

    unsigned short  SIS_MMIOBase;   // Memory mapped I/O selector
    unsigned long   SIS_MMIOAddr;   // Memory mapped I/O address

    // Additional 2.0 Data (offset 0x50)

    unsigned char   SIS_MaxVDMs;      // Max. no. of Virtual DOS machines
    unsigned char   SIS_Reserved;
    };
*/

/*  struct InfoSegLDT
    {
        unsigned short  LIS_CurProcID;  // Current process ID
        unsigned short  LIS_ParProcID;  // Process ID of parent
        unsigned short  LIS_CurThrdPri; // Current thread priority
        unsigned short  LIS_CurThrdID;  // Current thread ID
        unsigned short  LIS_CurScrnGrp; // Screengroup
        unsigned char   LIS_ProcStatus; // Process status bits
        unsigned char   LIS_fillbyte1;  // filler byte
        unsigned short  LIS_Fgnd;       // Current process is in foreground
        unsigned char   LIS_ProcType;   // Current process type
        unsigned char   LIS_fillbyte2;  // filler byte

        unsigned short  LIS_AX;         // @@V1 Environment selector
        unsigned short  LIS_BX;         // @@V1 Offset of command line start
        unsigned short  LIS_CX;         // @@V1 Length of Data Segment
        unsigned short  LIS_DX;         // @@V1 STACKSIZE from the .EXE file
        unsigned short  LIS_SI;         // @@V1 HEAPSIZE  from the .EXE file
        unsigned short  LIS_DI;         // @@V1 Module handle of the application
        unsigned short  LIS_DS;         // @@V1 Data Segment Handle of application

        unsigned short  LIS_PackSel;    // First tiled selector in this EXE
        unsigned short  LIS_PackShrSel; // First selector above shared arena
        unsigned short  LIS_PackPckSel; // First selector above packed arena
    }; */

/* ******************************************************************
 *                                                                  *
 *   Log file helpers                                               *
 *                                                                  *
 ********************************************************************/

/*
 *@@ _sprintf:
 *      sorta like sprintf, but prints into the
 *      global G_szScratchBuf buffer, which is
 *      1000 bytes in size.
 */

void _sprintf(const char *pcszFormat, ...)
{
    ULONG   cb = 0,
            cbWritten = 0;
    APIRET  arc = NO_ERROR;

    va_list arg_ptr;
    VA_START(arg_ptr, pcszFormat);
    __vsprintf(G_szScratchBuf, pcszFormat, arg_ptr);
    va_end(arg_ptr);
}

/*
 *@@ utilOpenLog:
 *      opens the log file specified in G_szLogFile (which
 *      has been set from the driver command line in
 *      sec32_init_base()) and stores the system file number
 *      in G_ulLogSFN.
 *
 *      This can then be used with SecHlpWrite.
 *
 *      Returns NO_ERROR or error code.
 */

int utilOpenLog(VOID)
{
    APIRET arc = NO_ERROR;

    if (G_szLogFile[0])
    {
        // log file enabled:
        arc = SecHlpOpen(G_szLogFile,
                         &G_ulLogSFN,
                         // open flags:
                         OPEN_ACTION_CREATE_IF_NEW
                            | OPEN_ACTION_REPLACE_IF_EXISTS,
                         // open mode:
                         OPEN_FLAGS_WRITE_THROUGH
                            | OPEN_FLAGS_FAIL_ON_ERROR
                            | OPEN_FLAGS_NO_CACHE
                            | OPEN_SHARE_DENYWRITE
                            | OPEN_ACCESS_READWRITE);
    }

    return (arc);
}

/*
 *@@ utilWriteLog:
 *      writes a string to the log file.
 *      Make sure to use __StackToFlat when passing the
 *      string.
 *
 *      Returns NO_ERROR or error code.
 */

int utilWriteLog(const char *pcszFormat, ...)
{
    ULONG   cb = 0,
            cbWritten = 0;
    APIRET  arc = NO_ERROR;

    if (G_ulLogSFN)
    {
        // compose string in G_szScratchBuf
        va_list arg_ptr;
        VA_START(arg_ptr, pcszFormat);
        __vsprintf(G_szScratchBuf, pcszFormat, arg_ptr);
        va_end(arg_ptr);

        cb = strlen(G_szScratchBuf);

        // ULONG SecHlpWrite(ULONG SFN,
        //                   PULONG pcbBytes,
        //                   PUCHAR pBuffer,
        //                   ULONG p16Addr,
        //                   ULONG Offset)
        arc = SecHlpWrite(G_ulLogSFN,       // log system file number
                          __StackToFlat(&cb),
                          G_szScratchBuf,   // string
                          0,
                          G_ulOffset);      // write offset, initially 0
        if (arc == NO_ERROR)
            // OK:
            // raise write offset by bytes which have been
            // written for next time
            G_ulOffset += cb;
    }
    return (arc);
}

/*
 *@@ utilWriteLogInfo:
 *      writes information about the current
 *      process/thread into log file.
 */

VOID utilWriteLogInfo(VOID)
{
    APIRET              arc = NO_ERROR;

    if (G_ulLogSFN)
    {
        arc = DevHlp32_GetInfoSegs(&G_pGDT, &G_pLDT);
        if (arc == NO_ERROR)
        {
            // PID/TID
            utilWriteLog("        PID 0x%lX PPID 0x%lX TID 0x%lX prty 0x%lX\r\n",
                               G_pLDT->LIS_CurProcID,
                               G_pLDT->LIS_ParProcID,
                               G_pLDT->LIS_CurThrdID,
                               G_pLDT->LIS_CurThrdPri);
            // date/time
            utilWriteLog("        %04d-%02d-%02d %02d:%02d:%02d:%03d\r\n",
                               G_pGDT->SIS_YrsDate, G_pGDT->SIS_MonDate, G_pGDT->SIS_DayDate,
                               G_pGDT->SIS_HrsTime, G_pGDT->SIS_MinTime, G_pGDT->SIS_SecTime, G_pGDT->SIS_HunTime);
        }
        else
            utilWriteLog("        Couldn't get info segs. DevHlp32_GetInfoSegs returned %d.\r\n",
                               arc);
    }
}

/*
 *@@ utilGetTaskPID:
 *      returns the PID of the caller's process.
 *      Valid at task time only.
 *
 *      Returns 0 on errors, the PID otherwise.
 *
 *@@added V0.9.4 (2000-07-03) [umoeller]
 */

unsigned long utilGetTaskPID(void)
{
    APIRET              arc = NO_ERROR;

    arc = DevHlp32_GetInfoSegs(&G_pGDT, &G_pLDT);
    if (arc == NO_ERROR)
        return (G_pLDT->LIS_CurProcID);
    else
        return (0);

    /* PTR16           p16Temp;

    if (DevHlp32_GetDosVar(DHGETDOSV_LOCINFOSEG,
                           __StackToFlat(&p16Temp),
                           0)
        == NO_ERROR)
    {
        // GetDosVar succeeded:
        // p16Temp now is a 16:16 pointer, which we need to
        // convert to flat first
        struct InfoSegLDT *pInfoSecLDT = 0;
        if (DevHlp32_VirtToLin(p16Temp, __StackToFlat(&pInfoSecLDT))
                == NO_ERROR)
            return (pInfoSecLDT->LIS_CurProcID);
    }

    return 0; */
}


