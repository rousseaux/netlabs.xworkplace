
#ifndef MPTRHOOK_H
#define MPTRHOOK_H

// external timer id
#define MOUSEMOVED_TIMER_ID   300

// dll name
#define HOOK_DLLNAME     "wpamptrh"
#define HOOK_HOOKPROC    "InputHook"
#define HOOK_SETDATAPROC "SetHookData"

// hook data
typedef struct _HOOKDATA
   {
         HWND           hwndNotify;
   } HOOKDATA, *PHOOKDATA;

// Prototypes
APIRET SetHookData( PSZ pszVersion, PHOOKDATA phookdata);

#endif // MPTRHOOK_H

