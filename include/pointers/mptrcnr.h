
#ifndef MPTRCNR_H
#define MPTRCNR_H

#include "pointers\mptrppl.h"

#define NEWLINE            "\n"
#define MAX_RES_STRLEN     32
#define MAX_RES_MSGLEN     256

typedef struct _HANDLERDATA
         {
         HMODULE        hmodResource;         // input fields
         PVOID          somSelf;              //   "      "
         PRECORDCORE *  ppcnrrec;             //   "      "
         PFNWP          pfnwpOriginal;
         PVOID          pvReserved;           // handler storage data
         } HANDLERDATA,  *PHANDLERDATA;

typedef struct _MYRECORDCORE
   {
         RECORDCORE     rec;
         ULONG          ulPointerIndex;
         PSZ            pszAnimationName;
         PSZ            pszAnimate;
         PSZ            pszAnimationType;
         PSZ            pszInfoName;
         PSZ            pszInfoArtist;
         PSZ            pszFrameRate;
         ULONG          ulPtrCount;
         PPOINTERLIST   ppl;
   } MYRECORDCORE, *PMYRECORDCORE;

#define RECORD_EXTRAMEMORY (sizeof( MYRECORDCORE) - sizeof( RECORDCORE))

// Prototypes

VOID InitStringResources( HAB hab, HMODULE hmodResource);
MRESULT EXPENTRY DialogHandlerProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);

BOOL QueryContainerView( HWND hwnd, PULONG pulViewStyle);
BOOL QueryItemSet( PRECORDCORE prec);
BOOL QueryItemLoaded( PRECORDCORE prec);
BOOL QueryItemAnimate( PRECORDCORE prec);


#endif // MPTRCNR_H

