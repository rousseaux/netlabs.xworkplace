
#ifndef MACROS_H
#define MACROS_H

#include "pointers\fmacros.h"
#include "pointers\tmacros.h"
#include "pointers\mmacros.h"

// some string macros
static CHAR STRINGLONG[20];
#define STRING(s)       ((s) ? s : "N/A")
#define STRLONG(l)      _ltoa(l , STRINGLONG, 10)
#define STRBOOL(b)      ((b) ? "TRUE" : "FALSE")
#define NEXTSTR(s)      (s+strlen(s) + 1)
#define ENDSTRING(s)    (s+strlen(s))

// some calc macros
#define MAX(a,b)        (a > b ? a : b)
#define MIN(a,b)        (a < b ? a : b)

// Prototyp undokumentiertes API
APIRET APIENTRY
DosQueryModFromEIP (HMODULE *phModule, ULONG *pulObjectNumber,
                    ULONG ulBufferLength, PCHAR pchBuffer,
                    ULONG *pulOffset, PVOID pvAddress);

// query pointer visibility
#define WinIsPointerVisible(hwnd) (WinQuerySysValue( hwnd, SV_POINTERLEVEL) == 0)

// enable controls
#define ENABLECONTROL(hwnd,id,flag)  (WinEnableWindow( WinWindowFromID( hwnd, id), flag))

// Pushbutton macros
#define PBSETDEFAULT(hwnd,id,flag)    (WinSendDlgItemMsg( hwnd, id, BM_SETDEFAULT, MPFROMLONG(flag), 0))
#define SETFOCUS(hwnd,id)             (WinSetFocus( HWND_DESKTOP, WinWindowFromID( hwnd, id)))

// read strings and messages
#define LOADMESSAGE(id,buf)                 (LoadStringResource( RT_MESSAGE, hab, hmodResource, id, (PSZ)buf, sizeof(buf)))
#define LOADMESSAGE2(id,buf,len)            (LoadStringResource( RT_MESSAGE, hab, hmodResource, id, (PSZ)buf, len))
#define LOADSTRING(id,buf)                  (LoadStringResource( RT_STRING,  hab, hmodResource, id, (PSZ)buf, sizeof(buf)))
#define LOADSTRING2(id,buf,len)             (LoadStringResource( RT_STRING,  hab, hmodResource, id, (PSZ)buf, len))

// manipulate entry field
#define DLGQUERYSTRING(hwnd,id,buf)         (WinQueryDlgItemText(hwnd, id, sizeof(buf), buf))
#define DLGSETSTRING(hwnd,id,str)           (WinSetDlgItemText(hwnd, id, str))
#define DLGSETSTRINGTEXTLIMIT(hwnd,id,max)  ((USHORT) WinSendDlgItemMsg( hwnd, id, EM_SETTEXTLIMIT, (MPARAM)max, 0L))
#define DLGSELECTSTRING(hwnd,id)            ((USHORT) WinSendDlgItemMsg( hwnd, id, EM_SETSEL, 0, -1))

#define DLGQUERYCHANGED(hwnd,id)            ((BOOL) WinSendDlgItemMsg( hwnd, id, EM_QUERYCHANGED, 0, 0))
#define MLEQUERYCHANGED(hwnd,id)            ((BOOL) WinSendDlgItemMsg( hwnd, id, MLM_QUERYCHANGED, 0, 0))


// manipulate checkbox
#define DLGQUERYCHECK(hwnd,id)              ((USHORT) WinSendDlgItemMsg( hwnd, id, BM_QUERYCHECK, 0L, 0L))
#define DLGSETCHECK(hwnd,id,check)          ((USHORT) WinSendDlgItemMsg( hwnd, id, BM_SETCHECK, (MPARAM) check, 0L))

// manipulate spin button
#define DLGSETSPIN(hwnd,id,value)           (WinSendDlgItemMsg( hwnd, id, SPBM_SETCURRENTVALUE, MPFROMLONG(value), 0L))
#define DLGINITSPIN(hwnd,id,high,low)       (WinSendDlgItemMsg( hwnd, id, SPBM_SETLIMITS, MPFROMLONG(high), MPFROMLONG(low)))


// simple listbox macros
#define INSERTITEM(hwnd,id,text)            ((ULONG) WinSendDlgItemMsg( hwnd, id, LM_INSERTITEM,      MPFROMSHORT( LIT_END), (MPARAM) text))
#define SETSELECTION(hwnd,id,item)          (        WinSendDlgItemMsg( hwnd, id, LM_SELECTITEM,      MPFROMSHORT(item),MPFROMSHORT(TRUE)))
#define QUERYSELECTION(hwnd,id,item) \
    ((ULONG) WinSendDlgItemMsg( hwnd, id, LM_QUERYSELECTION,  MPFROMSHORT( item), 0L))

// setup notebook styles
#define SETUPNOTEBOOK(hwnd, cxTab, cyTab, cPB)             \
 WinSendMsg( hwndNotebook, BKM_SETNOTEBOOKCOLORS,          \
             MPFROMLONG( SYSCLR_DIALOGBACKGROUND),         \
             MPFROMLONG( BKA_BACKGROUNDPAGECOLORINDEX));   \
 WinSendMsg( hwndNotebook, BKM_SETNOTEBOOKCOLORS,          \
             MPFROMLONG( SYSCLR_DIALOGBACKGROUND),         \
             MPFROMLONG( BKA_BACKGROUNDMAJORCOLORINDEX));  \
 WinSendMsg( hwndNotebook, BKM_SETNOTEBOOKCOLORS,          \
             MPFROMLONG( SYSCLR_DIALOGBACKGROUND),         \
             MPFROMLONG( BKA_BACKGROUNDMINORCOLORINDEX));  \
 WinSendMsg( hwndNotebook, BKM_SETDIMENSIONS,              \
             MPFROM2SHORT( cxTab, cyTab),                  \
             MPFROMSHORT( BKA_MAJORTAB));                  \
 WinSendMsg( hwndNotebook, BKM_SETDIMENSIONS,              \
             MPFROM2SHORT( cPB, cPB),                      \
             MPFROMSHORT( BKA_PAGEBUTTON));                \

#endif // MACROS_H

