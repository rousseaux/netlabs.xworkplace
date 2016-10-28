
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

#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_NOPMAPI
#include <os2.h>
#include <secure.h>

#include <string.h>

#include "xwpsec32.sys\types.h"
#include "xwpsec32.sys\StackToFlat.h"
#include "xwpsec32.sys\DevHlp32.h"
#include "xwpsec32.sys\reqpkt32.h"
#include "xwpsec32.sys\SecHlp.h"

#include "xwpsec32.sys\xwpsec_types.h"
#include "xwpsec32.sys\xwpsec_callbacks.h"

/* ******************************************************************
 *
 *   Log file helpers
 *
 ********************************************************************/

/*
 *@@ WriteEvent:
 *      writes event information into the log file.
 */

VOID WriteEvent(ULONG ulEvent)      // in: event no. (e.g. SES_EVENT_LOGON)
{
/* typedef union _SESEVENTDATA
   {
      SESLOGON                Logon;
      SESUNLOCK               Unlock;
      SESCHANGEPASSWORD       ChangePassword;
      SESPROFILE              Profile;
      SESIA                   IA;
      SESSENDSECURITYCONTEXT  SendSecurityContext;
      SESPROCESSCREATION      ProcessCreation;
      SESCREATEHANDLENOTIFY   CreateHandleNotify;
      SESDELETEHANDLENOTIFY   DeleteHandleNotify;
      SESREQUESTSCS           RequestSCS;

   } SESEVENTDATA, *PSESEVENTDATA;
*/

    if (G_ulLogSFN)
    {
        utilWriteLog("  SES event code: 0x%lX",
                           ulEvent);

        switch (ulEvent & 0x00FFFFFF)     // filter out phase flags
        {
            case SES_EVENT_INIT:
                utilWriteLog(" (SES_EVENT_INIT)");
            break;
            case SES_EVENT_DISPLAY_NOTEBOOK:
                utilWriteLog(" (SES_EVENT_DISPLAY_NOTEBOOK)");
            break;
            case SES_EVENT_LOGON:
                utilWriteLog(" (SES_EVENT_LOGON)");
                /* Logon:
                       typedef struct _SESLOGON
                       {
                          ULONG       UserNameLen;
                          CHAR        UserName[MAX_USER_NAME];
                          ULONG       UserTokenLen;
                          CHAR        UserToken[MAX_TOKEN];
                          ULONG       CUHandle;
                          ULONG       AuthoritySource;
                       } SESLOGON , *PSESLOGON;
                */
                /* utilWriteLog("\r\n    SESLOGON.CUHandle: %d",
                                   pEventData->Logon.CUHandle);
                utilWriteLog("\r\n    SESLOGON.AuthoritySource: %d",
                                   pEventData->Logon.AuthoritySource); */
            break;
            case SES_EVENT_LOGOFF:
                utilWriteLog(" (SES_EVENT_LOGOFF)");
            break;
            case SES_EVENT_SHUTDOWN:
                utilWriteLog(" (SES_EVENT_SHUTDOWN)");
            break;
            case SES_EVENT_LOCK:
                utilWriteLog(" (SES_EVENT_LOCK)");
            break;
            case SES_EVENT_UNLOCK:
                utilWriteLog(" (SES_EVENT_UNLOCK)");
            break;
            case SES_EVENT_CHANGE_PASSWORD:
                utilWriteLog(" (SES_EVENT_CHANGE_PASSWORD)");
            break;
            case SES_EVENT_CREATE_PROFILE:
                utilWriteLog(" (SES_EVENT_CREATE_PROFILE)");
            break;
            case SES_EVENT_DELETE_PROFILE:
                utilWriteLog(" (SES_EVENT_DELETE_PROFILE)");
            break;
            case SES_EVENT_IA:
                utilWriteLog(" (SES_EVENT_IA)");
            break;
            case SES_EVENT_CREATE_HANDLE_NOTIFY:
                utilWriteLog(" (SES_EVENT_CREATE_HANDLE_NOTIFY)");
            break;
            case SES_EVENT_DELETE_HANDLE_NOTIFY:
                utilWriteLog(" (SES_EVENT_DELETE_HANDLE_NOTIFY)");
            break;
            case SES_EVENT_SEND_SECURITY_CONTEXT:
                utilWriteLog(" (SES_EVENT_SEND_SECURITY_CONTEXT)");
            break;
            // case SES_EVENT_SET_CONTEXT_STATUS:
            // break;
            case SES_EVENT_PROCESS_CREATION:
                utilWriteLog(" (SES_EVENT_PROCESS_CREATION or SET_CONTEXT_STATUS)");
            break;

        }

        utilWriteLog("\r\n");
    }
}

/*
 *@@ WriteEventStatus:
 *
 */

VOID WriteEventStatus(ULONG ulEventStatus)
{
    if (G_ulLogSFN)
    {
        utilWriteLog("  Event status: 0x%lX (%d, loword: %d)\r\n",
                           ulEventStatus,
                           ulEventStatus,
                           ulEventStatus & 0xFFFF);
    }
}

/*
 *@@ WriteEventInfo:
 *
 *      Calls WriteEvent and WriteEventStatus in turn.
 */

VOID WriteEventInfo(PSESEVENT pSESEventInfo)
{
    if (G_ulLogSFN)
    {
        /* typedef struct _SESEVENT
           {
              ULONG                DaemonID;      // Dmon ID assigned  -In
              ULONG                ReqPID;        // PID /SESStartEvent caller -Out
              ULONG                ReqTID;        // TID /SESStartEvent caller -Out
              SESSecurityContext   SCH;           // SecurityContext of " caller
              ULONG                EventID;       // Unique Event ID  -Out
              ULONG                Event;         // Event code In/Out -
              ULONG                EventStatus;   // Event status In/Out
              SESEVENTDATA         EventData;     // Union.. data depends on event

           } SESEVENT, *PSESEVENT;
        */

        utilWriteLog("  Daemon ID: 0x%lX\r\n",
                           pSESEventInfo->DaemonID);
        utilWriteLog("  ReqPID: 0x%lX\r\n",
                           pSESEventInfo->ReqPID);
        utilWriteLog("  ReqTID: 0x%lX\r\n",
                           pSESEventInfo->ReqTID);
        utilWriteLog("  EventID: 0x%lX\r\n",
                           pSESEventInfo->EventID);
        WriteEvent(pSESEventInfo->Event);
        WriteEventStatus(pSESEventInfo->EventStatus);
    }
}

/* ******************************************************************
 *
 *   Audit callbacks
 *
 ********************************************************************/

/*
 *@@ AUDIT_STARTEVENT:
 *      Note that this is only called when SESStartEvent
 *      returns to the caller.
 */

VOID CallType AUDIT_STARTEVENT(ULONG AuditRC,
                               PSESSTARTEVENT pSESStartEvent)
{
    // audit log file specified?
    if (G_szLogFile[0])
    {
        if (G_ulLogSFN == 0)
            // log file not opened yet:
            utilOpenLog();

        /* lssapi.h:
            typedef struct _SESSTARTEVENT
            {
                ULONG                Event;         // Event code (see Event Codes)
                ULONG                EventStatus;   // Event status (see Event Status)
                SESEVENTDATA         EventData;     // Union.. data depends on event
            } SESSTARTEVENT, *PSESSTARTEVENT;
        */

        utilWriteLog("AUDIT_STARTEVENT:\r\n");
        utilWriteLogInfo();

        WriteEvent(pSESStartEvent->Event);
        WriteEventStatus(pSESStartEvent->EventStatus);
    }
}

/*
 *@@ AUDIT_WAITEVENT:
 *
 */

VOID CallType AUDIT_WAITEVENT(ULONG AuditRC,
                              PSESEVENT pSESEventInfo,
                              ULONG ulTimeout)
{
    // audit log file specified?
    if (G_szLogFile[0])
    {
        if (G_ulLogSFN == 0)
            // log file not opened yet:
            utilOpenLog();

        utilWriteLog("AUDIT_WAITEVENT:\r\n");
        utilWriteLogInfo();

        WriteEventInfo(pSESEventInfo);
    }
}

/*
 *@@ AUDIT_RETURNEVENTSTATUS:
 *
 */

VOID CallType AUDIT_RETURNEVENTSTATUS(ULONG AuditRC,
                                      PSESEVENT pSESEventInfo)
{
    // audit log file specified?
    if (G_szLogFile[0])
    {
        if (G_ulLogSFN == 0)
            // log file not opened yet:
            utilOpenLog();


        utilWriteLog("AUDIT_RETURNEVENTSTATUS:\r\n");
        utilWriteLogInfo();

        WriteEventInfo(pSESEventInfo);
    }
}

/*
 *@@ AUDIT_REGISTERDAEMON:
 *
 */

VOID CallType AUDIT_REGISTERDAEMON(ULONG AuditRC,
                                   ULONG ulDaemonID,
                                   ULONG ulEventList)
{
    // audit log file specified?
    if (G_szLogFile[0])
    {
        if (G_ulLogSFN == 0)
            // log file not opened yet:
            utilOpenLog();

        utilWriteLog("AUDIT_REGISTERDAEMON:\r\n");
        utilWriteLogInfo();

        utilWriteLog("  Daemon ID: 0x%lX\r\n",
                           ulDaemonID);

        utilWriteLog("  Event list: 0x%lX\r\n", ulEventList);
    }
}

/*
 *@@ AUDIT_RETURNWAITEVENT:
 *
 */

VOID CallType AUDIT_RETURNWAITEVENT(ULONG AuditRC,
                                    PSESEVENT pSESEventInfo,
                                    ULONG ulTimeout)
{
    // audit log file specified?
    if (G_szLogFile[0])
    {
        if (G_ulLogSFN == 0)
            // log file not opened yet:
            utilOpenLog();

        utilWriteLog("AUDIT_RETURNWAITEVENT:\r\n");
        utilWriteLogInfo();

        WriteEventInfo(pSESEventInfo);
    }
}

