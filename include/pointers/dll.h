
#ifndef DLL_H
#define DLL_H

#include "pointers\mptrppl.h"
#include "pointers\mptrptr.h"
#include "pointers\info.h"

typedef struct _FRAMEINFO
   {
         USHORT        usResId;            //     Resource Id
         ULONG         ulResSize;          //     Resource size
         ULONG         ulPtrIndex;         //     index to system pointer
         ULONG         ulResOfs;           //     Offset within file
         ULONG         ulTimeout;          //     timeout for this frame
         PVOID         pvDuplicate;        //     pointer to duplicate frame
   } FRAMEINFO, *PFRAMEINFO;

typedef struct _ANDFILEINFO
   {
         ULONG          aulFrameCount[ NUM_OF_SYSCURSORS];     // !  count of frames in animation
         PFRAMEINFO     apframeinfoStatic[ NUM_OF_SYSCURSORS]; // !  pointer to first frame for syspointer
         PFRAMEINFO     apframeinfo1st[ NUM_OF_SYSCURSORS];    // !  pointer to first frame for syspointer
         ULONG          ulPointerCount;                        // !  count of all pointer images
         PFRAMEINFO     apframeinfo;                           // !  pointer to frame table
   } ANDFILEINFO, *PANDFILEINFO;

// Prototypes
BOOL LoadPointerFromAnimouseFile( HMODULE hmod, ULONG ulPointerIndex,
                                  PHPOINTER pahptr, PICONINFO paiconinfo,
                                  PHPOINTER phptrStatic, PICONINFO piconinfoStatic,
                                  PULONG paulTimeout, PULONG pulEntries);

BOOL LoadFirstPointerFromAnimouseFile( PSZ pszAnimationFile,
                                       PHPOINTER phptr,
                                       PICONINFO piconinfo);

BOOL LoadFirstAnimationFromAnimouseFile( PSZ pszAnimationFile,
                                         PHPOINTER pahpointer,
                                         PULONG    pulTimeout,
                                         PULONG pulEntries);

APIRET WriteAnimouseDllFile( PSZ pszAnimationFileName,
                             BOOL fSaveAll, PPOINTERLIST papl,
                             PSZ pszInfoName, PSZ pszInfoArtist);

APIRET QueryAnimouseFileDetails( PSZ pszAnimationFileName, ULONG ulInfoLevel,
                                 PVOID pvData, ULONG ulBuflen, PULONG pulSize);

APIRET SetAnimouseFileInfo( PSZ pszAnimationFileName, PSOURCEINFO psourceinfo);


// Helpers for reading ressources
ULONG GetAnimouseStringId( HMODULE hmodResource, ULONG ulPtrResourceId);
ULONG GetAnimouseBasePointerId( ULONG ulPointerIndex);
ULONG GetSystemPointerIndexFromAnimouseId( ULONG ulPointerId);
BOOL  GetAnimousePointerResource( HMODULE hmodResource, ULONG ulResourceId, PICONINFO piconinfo);

#endif // DLL_H

