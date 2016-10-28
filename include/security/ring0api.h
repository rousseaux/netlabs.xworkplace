
/*
 *@@sourcefile ring0api.h:
 *      public definitions for interfacing XWPSEC32.SYS.
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
     *   Structures
     *
     ********************************************************************/

    #pragma pack(1)

    // event codes

    #define SECEVENT_GROUPMASK          0xFFFFFFF0

    #define EVENTGROUP(ev) ((ev) & SECEVENT_GROUPMASK)
                                                        // implementation status:
                                                        //    struct ring0 ring3
    // "Open/Close" group (0x00000010)
    #define SECEVENTGROUP_OPENCLOSE     0x00000010
    #define SECEVENT_OPEN_PRE           0x00000011      //      +      +     +
    #define SECEVENT_OPEN_POST          0x00000012      //      +      -     -
    #define SECEVENT_CLOSE              0x00000013      //      +      -     -

    // "Read/Write" group (0x00000020)
    #define SECEVENTGROUP_READWRITE     0x00000020
    #define SECEVENT_READ_PRE           0x00000021      //      -      -     -
    #define SECEVENT_READ_POST          0x00000022      //      -      -     -
    #define SECEVENT_WRITE_PRE          0x00000023      //      -      -     -
    #define SECEVENT_WRITE_POST         0x00000024      //      -      -     -

    // "Directories" group (0x00000040)
    #define SECEVENTGROUP_DIRS          0x00000040
    #define SECEVENT_MAKEDIR            0x00000041      //      +      +     +
    #define SECEVENT_CHANGEDIR          0x00000042      //      +      +     +
    #define SECEVENT_REMOVEDIR          0x00000043      //      +      +     +

    // "Delete"/"Move" group (0x00000080)
    #define SECEVENTGROUP_DELETEMOVE    0x00000080
    #define SECEVENT_DELETE_PRE         0x00000081      //      +      +     +
    #define SECEVENT_DELETE_POST        0x00000082      //      +      +     +
    #define SECEVENT_MOVE_PRE           0x00000083      //      +      -     -
    #define SECEVENT_MOVE_POST          0x00000084      //      +      -     -

    // "Execute" group (0x00000100)
    #define SECEVENTGROUP_EXEC          0x00000100
    #define SECEVENT_LOADEROPEN         0x00000101      //      +      +     +
    #define SECEVENT_GETMODULE          0x00000102      //      +      +     +
    #define SECEVENT_EXECPGM            0x00000103      //      +      +     +
    #define SECEVENT_EXECPGM_POST       0x00000104      //      +      +     +
    #define SECEVENT_CREATEVDM          0x00000105      //      -      -     -
    #define SECEVENT_CREATEVDM_POST     0x00000106      //      -      -     -

    // "File find" group (0x00000200)
    #define SECEVENTGROUP_FIND          0x00000200
    #define SECEVENT_FINDFIRST          0x00000201      //      +      -     -
    #define SECEVENT_FINDNEXT           0x00000202      //      -      -     -
    #define SECEVENT_FINDCLOSE          0x00000203      //      -      -     -
    #define SECEVENT_FINDFIRST3X        0x00000204      //      -      -     -
    #define SECEVENT_FINDFIRSTNEXT3X    0x00000205      //      -      -     -
    #define SECEVENT_FINDCLOSE3X        0x00000206      //      -      -     -

    // "File info" group (0x00000400)
    #define SECEVENTGROUP_FILEINFO      0x00000400
    #define SECEVENT_SETFILEINFO        0x00000401      //      -      -     -
    #define SECEVENT_SETPATHINFO        0x00000402      //      -      -     -
    #define SECEVENT_SETFILESIZE        0x00000403      //      -      -     -
    #define SECEVENT_SETFILEMODE        0x00000404      //      -      -     -

    // "File pointer" group (0x00000800)
    #define SECEVENTGROUP_FILEPTR       0x00000800
    #define SECEVENT_CHGFILEPTR         0x00000801      //      -      -     -
    #define SECEVENT_QUERYFILEINFO_POST 0x00000802      //      -      -     -

    // "System Date/Time"
    #define SECEVENTGROUP_DATETIME      0x00010000
    #define SECEVENT_SETDATETIME        0x00010001      //      -      -     -

    // "DevIOCtl"
    #define SECEVENTGROUP_DEVIOCTL      0x00020000
    #define SECEVENT_DEVIOCTL           0x00020001      //      -      -     -

    // "Trusted Path Control"
    #define SECEVENTGROUP_TRUSTEDPATH   0x00040000
    #define SECEVENT_TRUSTEDPATHCONTROL 0x00040001      //      -      -     -

    // "Callgate" group (0x00060000)
    #define SECEVENTGROUP_CALLGATE      0x00080000
    #define SECEVENT_CALLGATE16         0x00080001      //      -      -     -
    #define SECEVENT_CALLGATE32         0x00080002      //      -      -     -

    /*
     *@@ XWPSECEVENTDATA_FILEONLY:
     *      authorization data for a file or directory.
     *
     *      Used with the following events:
     *
     +      SECEVENT_DELETE_PRE         0x00000002:
     +          Required privileges:
     +          -- XWPACCESS_DELETE on the file.
     +
     +      SECEVENT_GETMODULE          0x00000102
     +          GETMODULE gets called for each DosLoadModule
     +          call, apparently, and the path is not always
     +          fully initialized (if an application wants
     +          to search the path).
     +          See XWPSECEVENTDATA_EXECPGM for more
     +          information on executable callouts.
     +          Required privileges:
     +          -- XWPACCESS_EXEC on the executable.
     +
     +      SECEVENT_MAKEDIR            0x00000008
     +          Required privileges:
     +          -- XWPACCESS_CREATE on the directory.
     +
     +      SECEVENT_CHANGEDIR          0x00000010
     +          Required privileges:
     +          -- XWPACCESS_EXEC on the directory.
     +
     +      SECEVENT_REMOVEDIR          0x00000020
     +          Required privileges:
     +          -- XWPACCESS_DELETE on the directory.
     *
     *      Member of the XWPSECEVENTDATA union.
     */

    typedef struct _XWPSECEVENTDATA_FILEONLY
    {
        CHAR        szPath[2*CCHMAXPATH];
    } XWPSECEVENTDATA_FILEONLY, *PXWPSECEVENTDATA_FILEONLY;

    /*
     *@@ XWPSECEVENTDATA_CLOSE:
     *      notification event.
     *      No privileges required.
     *
     *      Member of the XWPSECEVENTDATA union.
     */

    typedef struct _XWPSECEVENTDATA_CLOSE
    {
        ULONG       SFN;
    } XWPSECEVENTDATA_CLOSE, *PXWPSECEVENTDATA_CLOSE;

    /*
     *@@ XWPSECEVENTDATA_DELETE_POST:
     *      notification event.
     *      No privileges required.
     *
     *      ACLDB must be notified so that it can delete
     *      ACL entries for this resource.
     *
     *      Member of the XWPSECEVENTDATA union.
     */

    typedef struct _XWPSECEVENTDATA_DELETE_POST
    {
        CHAR        szPath[2*CCHMAXPATH];
        ULONG       rc;
    } XWPSECEVENTDATA_DELETE_POST, *PXWPSECEVENTDATA_DELETE_POST;

    /*
     *@@ XWPSECEVENTDATA_FINDFIRST:
     *      authorization event for listing the contents
     *      of a directory (DosFindFirst).
     *
     *      Used with the following events:
     *
     +      SECEVENT_FINDFIRST          0x00000201
     +          Required privileges:
     +          -- XWPACCESS_READ on the directory.
     *
     *      Member of the XWPSECEVENTDATA union.
     */

    typedef struct _XWPSECEVENTDATA_FINDFIRST
    {
        CHAR        szPath[2*CCHMAXPATH];
                    // fully qualified file mask, e.g. "F:\*.txt"
        ULONG       ulHandle;     // search handle
        ULONG       rc;           // rc user got from findfirst
        // USHORT      usResultCnt;  // count of found files
        USHORT      usReqCnt;     // count user requested
        USHORT      usLevel;      // search level
        USHORT      usBufSize;    // user buffer size
        USHORT      fPosition;    // use position information?
        // PCHAR  pcBuffer;     // ptr to user buffer
        ULONG       ulPosition;   // position for restarting search
        // PSZ         pszPosition;  // file to restart search with
    } XWPSECEVENTDATA_FINDFIRST, *PXWPSECEVENTDATA_FINDFIRST;

    /*
     *@@ XWPSECEVENTDATA_LOADEROPEN:
     *      authorization event for an executable file.
     *
     *      Used with the following events:
     *
     +      SECEVENT_LOADEROPEN         0x00000040
     +          Required privileges:
     +          -- XWPACCESS_EXEC on the executable.
     *
     *      See XWPSECEVENTDATA_EXECPGM for more
     *      information on executable callouts.
     *
     *      Member of the XWPSECEVENTDATA union.
     */

    typedef struct _XWPSECEVENTDATA_LOADEROPEN
    {
        CHAR        szFileName[2*CCHMAXPATH];
        ULONG       SFN;
    } XWPSECEVENTDATA_LOADEROPEN, *PXWPSECEVENTDATA_LOADEROPEN;

    /*
     *@@ XWPSECEVENTDATA_EXECPGM:
     *      authorization event for DosExecPgm.
     *
     *      Used with the following events:
     *
     +      SECEVENT_EXECPGM            0x00000103
     +          Required privileges:
     +          -- XWPACCESS_EXEC on the executable.
     *
     *      Note: When DosExecPgm is called, we receive
     *      callouts in the following order:
     *
     *      1) OPEN_PRE for the executable.
     *
     *      2) LOADEROPEN for the executable.
     *
     *      3) GETMODULE for the executable (full path).
     *
     *      4) EXECPGM for the executable.
     *
     *      5) We then get GETMODULE for each DLL which
     *         needs to be imported.
     *
     *      Member of the XWPSECEVENTDATA union.
     */

    typedef struct _XWPSECEVENTDATA_EXECPGM
    {
        CHAR        szFileName[2*CCHMAXPATH];
        CHAR        szArgs[1024];
    } XWPSECEVENTDATA_EXECPGM, *PXWPSECEVENTDATA_EXECPGM;

    /*
     *@@ XWPSECEVENTDATA_EXECPGM_POST:
     *      notification event.
     *      No privileges required.
     *
     *      Member of the XWPSECEVENTDATA union.
     */

    typedef struct _XWPSECEVENTDATA_EXECPGM_POST
    {
        CHAR        szFileName[2*CCHMAXPATH];
        CHAR        szArgs[1024];
        ULONG       ulNewPID;
    } XWPSECEVENTDATA_EXECPGM_POST, *PXWPSECEVENTDATA_EXECPGM_POST;

    /*
     *@@ XWPSECEVENTDATA_MOVE_PRE:
     *      authorization event for moving/renaming
     *      a file or directory.
     *
     *      This only gets called when moving on the
     *      same volume. No matter if the file is
     *      being moved or renamed, both paths are
     *      always fully qualified.
     *
     *      Example:
     +          F:\test> ren foo.txt bar.txt
     *
     *      results in "F:\test\bar.txt" (new) and
     *      "F:\test\foo.txt". This applies to
     *      directories as well.
     *
     *      Used with the following events:
     *
     +      SECEVENT_MOVE_PRE           0x00000083
     +          Required privileges:
     +          -- for source directory: XWPACCESS_DELETE.
     +          -- for target directory: XWPACCESS_WRITE.
     +          -- for file: XWPACCESS_WRITE.
     *
     *      Member of the XWPSECEVENTDATA union.
     */

    typedef struct _XWPSECEVENTDATA_MOVE_PRE
    {
        CHAR        szNewPath[2*CCHMAXPATH];
        CHAR        szOldPath[2*CCHMAXPATH];
    } XWPSECEVENTDATA_MOVE_PRE, *PXWPSECEVENTDATA_MOVE_PRE;

    /*
     *@@ XWPSECEVENTDATA_MOVE_POST:
     *      notification event.
     *      No privileges required.
     *
     *      ACLDB must be notified so that it can update
     *      ACL entries for this resource.
     *
     *      Member of the XWPSECEVENTDATA union.
     */

    typedef struct _XWPSECEVENTDATA_MOVE_POST
    {
        CHAR        szNewPath[2*CCHMAXPATH];
        CHAR        szOldPath[2*CCHMAXPATH];
        ULONG       rc;
    } XWPSECEVENTDATA_MOVE_POST, *PXWPSECEVENTDATA_MOVE_POST;

    /*
     *@@ XWPSECEVENTDATA_OPEN_PRE:
     *      authorization event for opening a file (DosOpen).
     *      Note: This also gets called for devices
     *      In that case, szFileName starts out with "\DEV\".
     *
     *      Used with the following events:
     *
     +      SECEVENT_OPEN_PRE           0x00000011
     +          Required privileges:
     +
     +          -- If (fsOpenFlags & OPEN_ACTION_CREATE_IF_NEW):
     +              XWPACCESS_CREATE for file's directory.
     +          -- If (fsOpenFlags & OPEN_ACTION_OPEN_IF_EXISTS):
     +              XWPACCESS_READ for file.
     +          -- If (fsOpenFlags & OPEN_ACTION_REPLACE_IF_EXISTS):
     +              XWPACCESS_WRITE for file plus
     +              XWPACCESS_CREATE for file's directory.
     +
     +          -- If (fsOpenMode & OPEN_FLAGS_DASD) (open drive):
     +              XWPACCESS_WRITE and XWPACCESS_DELETE and XWPACCESS_CREATE
     +              for the entire drive (root directory).
     +          -- If (fsOpenMode & OPEN_ACCESS_READONLY):
     +              XWPACCESS_READ for file
     +              plus XWPACCESS_READ for file's directory.
     +          -- If (fsOpenMode & OPEN_ACCESS_WRITEONLY):
     +              XWPACCESS_WRITE for file
     +              plus XWPACCESS_WRITE for file's directory.
     +          -- If (fsOpenMode & OPEN_ACCESS_READWRITE):
     +              XWPACCESS_READ and XWPACCESS_WRITE for file.
     +              plus XWPACCESS_READ | XWPACCESS_WRITE for file's directory.
     *
     *      Member of the XWPSECEVENTDATA union.
     */

    typedef struct _XWPSECEVENTDATA_OPEN_PRE
    {
        CHAR        szFileName[2*CCHMAXPATH];
        ULONG       fsOpenFlags;
        ULONG       fsOpenMode;
        ULONG       SFN;
    } XWPSECEVENTDATA_OPEN_PRE, *PXWPSECEVENTDATA_OPEN_PRE;

    /*
     *@@ XWPSECEVENTDATA_OPEN_POST:
     *      notification event.
     *      No privileges required.
     *
     *      Member of the XWPSECEVENTDATA union.
     */

    typedef struct _XWPSECEVENTDATA_OPEN_POST
    {
        CHAR        szFileName[2*CCHMAXPATH];
        ULONG       fsOpenFlags;
        ULONG       fsOpenMode;
        ULONG       SFN;
        ULONG       ulActionTaken;
        ULONG       rc;
    } XWPSECEVENTDATA_OPEN_POST, *PXWPSECEVENTDATA_OPEN_POST;

    /*
     *@@ XWPSECEVENTDATA_AC1024:
     *      structure for passing any data.
     *
     *      Member of the XWPSECEVENTDATA union.
     */

    typedef struct _XWPSECEVENTDATA_AC1024
    {
        CHAR        acData[1000];
    } XWPSECEVENTDATA_AC1024, *PXWPSECEVENTDATA_AC1024;

    /*
     *@@ XWPSECEVENTDATA:
     *      union which combines the event-specific
     *      data.
     *
     *      Member of SECIOSHARED.
     */

    typedef union _XWPSECEVENTDATA
    {
        // SECEVENT_DELETE_PRE
        // SECEVENT_GETMODULE
        // SECEVENT_MAKEDIR
        // SECEVENT_CHANGEDIR
        // SECEVENT_REMOVEDIR
        XWPSECEVENTDATA_FILEONLY    FileOnly;

        // SECEVENT_CLOSE
        XWPSECEVENTDATA_CLOSE       Close;

        // SECEVENT_DELETE_POST
        XWPSECEVENTDATA_DELETE_POST DeletePost;

        // SECEVENT_FINDFIRST
        XWPSECEVENTDATA_FINDFIRST   FindFirst;

        // SECEVENT_LOADEROPEN
        XWPSECEVENTDATA_LOADEROPEN  LoaderOpen;

        // SECEVENT_EXECPGM
        XWPSECEVENTDATA_EXECPGM     ExecPgm;

        // SECEVENT_EXECPGM_POST
        XWPSECEVENTDATA_EXECPGM_POST ExecPgmPost;

        // SECEVENT_MOVE_PRE
        XWPSECEVENTDATA_MOVE_PRE    MovePre;

        // SECEVENT_MOVE_POST
        XWPSECEVENTDATA_MOVE_POST   MovePost;

        // SECEVENT_OPEN_PRE
        XWPSECEVENTDATA_OPEN_PRE    OpenPre;

        // SECEVENT_OPEN_POST
        XWPSECEVENTDATA_OPEN_POST   OpenPost;

        // array of char:
        XWPSECEVENTDATA_AC1024      ac1024;

    } XWPSECEVENTDATA, *PXWPSECEVENTDATA;

    /*
     *@@ SECIOSHARED:
     *      data structure shared between the
     *      ring-0 driver (XWPSEC32.SYS) and
     *      the ring-3 daemon.
     *
     *      When the driver unblocks the daemon
     *      thread, it fills ulEventCode, ulCallerPID,
     *      and EventData. The daemon must return
     *      the access code in arc (either NO_ERROR
     *      or ERROR_ACCESS_DENIED).
     */

    typedef struct _SECIOSHARED
    {
        ULONG   ulEventCode;
                // in: event code

        // process/thread data of caller who did the request (e.g. DosOpen)
        ULONG   ulCallerPID;            // process ID
        ULONG   ulParentPID;            // parent process ID
        ULONG   ulCallerTID;            // thread ID

        APIRET  arc;
                // out: error code returned from daemon
                // for this operation

        XWPSECEVENTDATA EventData;
                // in: event-specific data
    } SECIOSHARED, *PSECIOSHARED;

    /* ******************************************************************
     *
     *   IOCtl
     *
     ********************************************************************/

    // category for IOCtl's to xwpsec32.sys
    #define IOCTL_XWPSEC            0x8F

    // IOCtl functions

    /*
     *@@ SECIOREGISTER:
     *      structure to be passed with the
     *      XWPSECIO_REGISTER DosDevIOCtl
     *      function code to XWPSEC32.SYS.
     *
     *      <B>XWPSec security protocol:</B>
     *
     *      1.  The ring-3 daemon must call DosDevIOCtl
     *          with the XWPSECIO_REGISTER function code,
     *          passing this structure as the data packet.
     *
     *          On input, the following must be done:
     *
     *          a) The daemon must allocate a SECIOSHARED
     *             structure in pSecIOShared. This must
     *             be allocated using DosAllocMem, since
     *             this memory must be on a page boundary.
     *             Besides, it must have PAGE_COMMIT | PAGE_READ
     *             | PAGE_WRITE flags.
     *
     *          b) The daemon must create a SHARED 32-bit event
     *             semaphore and pass it in hevCallback.
     *
     *          c) The daemon calls DosDevIOCtl on the driver
     *             with IOCTL_XWPSEC and XWPSECIO_REGISTER.
     *
     *             If NO_ERROR is returned, access control
     *             is ENABLED on the system!
     *
     *      2.  The daemon must then block on hevCallback
     *          (DosWaitEventSem(hevCallback).
     *
     *          XWPSEC32.SYS posts this semaphore whenever
     *          access needs to be verified for some other
     *          process. The driver takes care of serialization
     *          so that the daemon only gets one access
     *          request at a time.
     *
     *      3.  When hevCallback is posted, the daemon needs
     *          to do access verification based on the data
     *          in SECIOSHARED. See SECIOSHARED for details.
     *
     *      4.  The daemon then needs to signal the driver
     *          that it's done with the access request.
     *
     *          Presently, this is done using another IOCtl
     *          named XWPSECIO_JOBDONE which takes no
     *          parameters or data.
     *
     *          NOTE: After the daemon has unblocked from
     *          the event semaphore (step 3), all other
     *          threads from any other application on the
     *          system which have called a function where
     *          access needs to be verified are BLOCKED.
     *          The daemon must process the access request
     *          as quickly as possible, and it MUST signal
     *          to the driver that it's done. Otherwise
     *          all other applications on the system will
     *          starve.
     *
     *      5.  Go back to 2.
     */

    /* typedef struct _SECIOREGISTER
    {
        HEV             hevCallback;
                // event semaphore ring-3 is waiting on;
                // this must be shared, otherwise driver
                // cannot find it

        HMTX            hmtxBufferBusy;

        PSECIOSHARED    pSecIOShared;
                // pointer to SECIOSHARED structure

    } SECIOREGISTER, *PSECIOREGISTER; */

    #define XWPSECIO_REGISTER           0x50

    #define XWPSECIO_AUTHORIZED_NEXT    0x54

    #define XWPSECIO_DEREGISTER         0x55

    #pragma pack()

#endif

#if __cplusplus
}
#endif

