
/*
 *@@sourcefile ring0api.h:
 *      public definitions for interfacing XWPSEC32.SYS, shared between
 *      ring 3 and ring 0.
 */

/*
 *      Copyright (C) 2000 Ulrich M”ller.
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, in version 2 as it comes in the COPYING
 *      file of the XWorkplace main distribution.
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 */

#if __cplusplus
extern "C" {
#endif

#ifndef RING0API_HEADER_INCLUDED
    #define RING0API_HEADER_INCLUDED

    /* ******************************************************************
     *
     *   IOCtl
     *
     ********************************************************************/

    #pragma pack(1)

    // category for IOCtl's to xwpsec32.sys
    #define IOCTL_XWPSEC            0x8F

    // IOCtl functions

    /*
     *@@ SECIOREGISTER:
     *      structure to be passed with the XWPSECIO_REGISTER
     *      DosDevIOCtl function code to XWPSEC32.SYS.
     *
     *      With this structure, the ring-3 shell must
     *      pass down an array of process IDs that were
     *      running already at the time the shell was
     *      started. These PIDs will then be treated as
     *      trusted processes by the driver.
     */

    typedef struct _PROCESSLIST
    {
        ULONG       cbStruct;
        USHORT      cTrusted;
        USHORT      apidTrusted[1];
    } PROCESSLIST, *PPROCESSLIST;

    #define XWPSECIO_REGISTER           0x50

    #define XWPSECIO_DEREGISTER         0x51

    /*
     *@@ TIMESTAMP:
     *      matches the first 20 bytes from the global infoseg
     *      declaration.
     *
     *@@added V1.0.1 (2003-01-10) [umoeller]
     */

    typedef struct _TIMESTAMP
    {
        ULONG   SIS_BigTime;    // Time from 1-1-1970 in seconds    4
        ULONG   SIS_MsCount;    // Freerunning milliseconds counter 8
        UCHAR   hours;          // Hours                            9
        UCHAR   minutes;        // Minutes                          10
        UCHAR   seconds;        // Seconds                          11
        UCHAR   SIS_HunTime;    // Hundredths of seconds            12
        USHORT  SIS_TimeZone;   // Timezone in min from GMT (Set to EST)  14
        USHORT  SIS_ClkIntrvl;  // Timer interval (units=0.0001 secs)     16
        UCHAR   day;            // Day-of-month (1-31)                    17
        UCHAR   month;          // Month (1-12)                           18
        USHORT  year;           // Year (>= 1980)                         20
    } TIMESTAMP, *PTIMESTAMP;

    /*
     *@@ CONTEXTINFO:
     *      matches the first 16 bytes from the local infoseg
     *      declaration.
     *
     *@@added V [umoeller]
     */

    typedef struct _CONTEXTINFO
    {
        USHORT  pid;            // Current process ID
        USHORT  ppid;           // Process ID of parent
        USHORT  prty;           // Current thread priority
        USHORT  tid;            // Current thread ID
        USHORT  LIS_CurScrnGrp; // Screengroup
        UCHAR   LIS_ProcStatus; // Process status bits
        UCHAR   LIS_fillbyte1;  // filler byte
        USHORT  LIS_Fgnd;       // Current process is in foreground
        UCHAR   LIS_ProcType;   // Current process type
        UCHAR   LIS_fillbyte2;  // filler byte
    } CONTEXTINFO, *PCONTEXTINFO;

    /*
     *@@ EVENTLOGENTRY:
     *
     *@@added V1.0.1 (2003-01-10) [umoeller]
     */

    typedef struct _EVENTLOGENTRY
    {
        USHORT      cbStruct;       // total size of EVENTLOGENTRY plus data that follows
        USHORT      ulEventCode;    // EVENT_* code

        CONTEXTINFO ctxt;           // 16 bytes

        TIMESTAMP   stamp;          // 20 bytes

        // event-specific data follows right afterwards;
        // as a convention, each data buffer has another
        // ULONG cbStruct as the first entry
    } EVENTLOGENTRY, *PEVENTLOGENTRY;

    #define LOGBUFSIZE          0xFFFF

    /*
     *@@ LOGBUF:
     *      logging buffer used in two contexts:
     *
     *      --  as the data packet used with the
     *          XWPSECIO_GETLOGBUF ioctl, which
     *          must be allocated LOGBUFSIZE (64K)
     *          in size by the ring-3 logging thread;
     *
     *      --  as a linked list of logging buffers
     *          in the driver.
     *
     *@@added V1.0.1 (2003-01-10) [umoeller]
     */

    typedef struct _LOGBUF
    {
        ULONG           cbUsed;             // total bytes used in the buffer
        struct _LOGBUF  *pNext;             // used only at ring 0 for linking bufs
        ULONG           cLogEntries;        // no. of EVENTLOGENTRY structs that follow

        // next follow cLogEntries EVENTLOGENTRY structs

    } LOGBUF, *PLOGBUF;

    /*
     *@@ XWPSECIO_GETLOGBUF:
     *      ioctl function code for the ring-3 logging
     *      thread.
     *
     *      The shell can optionally start a thread to
     *      retrieve logging data from ring 0. This
     *      thread must do nothing but keep calling
     *      this ioctl function code with a pointer
     *      to a LOGBUFSIZE (64K) LOGBUF structure as
     *      the data packet.
     *
     *      On each ioctl, ring 0 will block the thread
     *      until logging data is available.
     *
     *      The buffer contains data only if NO_ERROR
     *      is returned, and LOGBUF.cbUsed bytes will
     *      have been filled. The pNext buffer contains
     *      a ring-0 pointer and must not be followed
     *      by the thread.
     *
     *      Ring 0 may return the following errors:
     *
     *      --  ERROR_I24_INVALID_PARAMETER
     *
     *      --  ERROR_I24_GEN_FAILURE: probably out of
     *          memory.
     *
     *      --  ERROR_I24_CHAR_CALL_INTERRUPTED: ProcBlock
     *          returned with an error.
     */

    #define XWPSECIO_GETLOGBUF          0x52

    /*
     *@@ RING0STATUS:
     *
     *@@added V1.0.1 (2003-01-10) [umoeller]
     */

    typedef struct _RING0STATUS
    {
        ULONG       cbAllocated;        // fixed memory currently allocated in ring 0
        ULONG       cAllocations,       // no. of allocations made since startup
                    cFrees;             // no. of frees made since startup
        USHORT      cLogBufs,           // current 64K log buffers in use
                    cMaxLogBufs;        // max 64K log buffers that were ever in use
        ULONG       cLogged;            // no. of syscalls that were logged
        ULONG       cGranted,           // no. of syscalls where access was granted
                    cDenied;            // ... and denied
    } RING0STATUS, *PRING0STATUS;

    /*
     *@@ XWPSECIO_QUERYSTATUS:
     *      returns the current status of the driver.
     *
     *      Data packet must be a fixed-size RING0STATUS
     *      structure.
     */

    #define XWPSECIO_QUERYSTATUS        0x53

    /* ******************************************************************
     *
     *   Event logging codes
     *
     ********************************************************************/

    /*
     *@@ EVENT_OPEN_PRE:
     *      uses EVENTBUF_OPEN.
     */

    #define EVENT_OPEN_PRE              1

    /*
     *@@ EVENT_OPEN_POST:
     *      uses EVENTBUF_OPEN.
     */

    #define EVENT_OPEN_POST             2

    /*
     *@@ EVENT_LOADEROPEN:
     *      uses EVENTBUF_LOADEROPEN.
     */

    #define EVENT_LOADEROPEN            3

    /*
     *@@ EVENT_GETMODULE:
     *      uses EVENTBUF_FILENAME.
     */

    #define EVENT_GETMODULE             4

    /*
     *@@ EVENT_EXECPGM_PRE:
     *      uses EVENTBUF_EXECPGM.
     */

    #define EVENT_EXECPGM_PRE           5

    /*
     *@@ EVENT_EXECPGM_ARGS:
     *      @@todo
     */

    #define EVENT_EXECPGM_ARGS          6

    /*
     *@@ EVENT_EXECPGM_PRE:
     *      uses EVENTBUF_EXECPGM.
     */

    #define EVENT_EXECPGM_POST          7

    /*
     *@@ EVENT_CLOSE:
     *      uses EVENTBUF_CLOSE.
     */

    #define EVENT_CLOSE                 8

    /*
     *@@ EVENT_DELETE_PRE:
     *      uses EVENTBUF_FILENAME.
     */

    #define EVENT_DELETE_PRE            9

    /*
     *@@ EVENT_DELETE_POST:
     *      uses EVENTBUF_FILENAME.
     */

    #define EVENT_DELETE_POST           10

    /* ******************************************************************
     *
     *   Event-specific logging structures
     *
     ********************************************************************/

    /*
     *@@ EVENTBUF_FILENAME:
     *      event buffer used with EVENT_GETMODULE,
     *      EVENT_DELETE_PRE, EVENT_DELETE_POST,
     *      EVENT_EXECPGM_PRE, EVENT_EXECPGM_ARGS,
     *      and EVENT_EXECPGM_POST.
     *
     *@@added V1.0.1 (2003-01-13) [umoeller]
     */

    typedef struct _EVENTBUF_FILENAME
    {
        ULONG   rc;             // -- EVENT_LOADEROPEN, EVENT_GETMODULE, EVENT_DELETE_PRE:
                                //    authorization returned from callout,
                                //    either NO_ERROR or ERROR_ACCESS_DENIED
                                // -- EVENT_DELETE_POST: RC parameter passed in
                                // -- EVENT_EXECPGM_PRE: authorization returned from callout,
                                //    either NO_ERROR or ERROR_ACCESS_DENIED
                                // -- EVENT_EXECPGM_ARGS: always 0
                                // -- EXECPGM_POST: newly created process ID
        ULONG   ulPathLen;      // length of szPath, excluding null terminator
        CHAR    szPath[1];      // filename
    } EVENTBUF_FILENAME, *PEVENTBUF_FILENAME;

    /*
     *@@ EVENTBUF_LOADEROPEN:
     *      event buffer used with EVENT_LOADEROPEN.
     *
     *@@added V1.0.1 (2003-01-13) [umoeller]
     */

    typedef struct _EVENTBUF_LOADEROPEN
    {
        ULONG   SFN;            // -- EVENT_LOADEROPEN: system file number;
                                // -- EVENT_GETMODULE, EVENT_DELETE_PRE, EVENT_DELETE_POST: not used
        ULONG   rc;             // -- EVENT_LOADEROPEN, EVENT_GETMODULE, EVENT_DELETE_PRE:
                                //    authorization returned from callout,
                                //    either NO_ERROR or ERROR_ACCESS_DENIED
                                // -- EVENT_DELETE_POST: RC parameter passed in
        ULONG   ulPathLen;      // length of szPath, excluding null terminator
        CHAR    szPath[1];      // filename
    } EVENTBUF_LOADEROPEN, *PEVENTBUF_LOADEROPEN;

    /*
     *@@ EVENTBUF_OPEN:
     *      event buffer used with EVENT_OPEN_PRE
     *      and EVENT_OPEN_POST.
     *
     *@@added V1.0.1 (2003-01-10) [umoeller]
     */

    typedef struct _EVENTBUF_OPEN
    {
        ULONG   fsOpenFlags;    // open flags
        ULONG   fsOpenMode;     // open mode
        ULONG   SFN;            // system file number
        ULONG   Action;         // OPEN_POST only: action taken
        APIRET  rc;             // -- with OPEN_PRE: authorization returned from callout,
                                //    either NO_ERROR or ERROR_ACCESS_DENIED
                                // -- with OPEN_POST: RC parameter passed in
        ULONG   ulPathLen;      // length of szPath, excluding null terminator
        CHAR    szPath[1];      // filename
    } EVENTBUF_OPEN, *PEVENTBUF_OPEN;

    /*
     *@@ EVENTBUF_CLOSE:
     *      event buffer used with EVENT_CLOSE.
     *
     *@@added V1.0.1 (2003-01-10) [umoeller]
     */

    typedef struct _EVENTBUF_CLOSE
    {
        ULONG   SFN;            // system file number
    } EVENTBUF_CLOSE, *PEVENTBUF_CLOSE;

    #pragma pack()

#endif

#if __cplusplus
}
#endif

