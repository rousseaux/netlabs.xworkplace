
#ifndef INFO_H
#define INFO_H

#include "pointers\mptrptr.h"

#define FI_SOURCEINFO    1
#define FI_DETAILINFO 1000

typedef struct _SOURCEINFO
   {
         PSZ            pszInfoName;
         PSZ            pszInfoArtist;
   } SOURCEINFO, *PSOURCEINFO;

typedef struct _PTRSOURCEINFO
   {
         SOURCEINFO     sourceinfo;
         BOOL           fColor;
         BYTE           XHotspot;
         BYTE           YHotspot;
         ULONG          ulColorCount;
         ULONG          ulSequence;
         ULONG          ulTimeout;
   } PTSOURCEINFO, *PPTRSOURCEINFO;

typedef struct _ANIMSOURCEINFO
   {
         SOURCEINFO     sourceinfo;
         ULONG          ulPhysFrames;
         ULONG          ulFrames;
         ULONG          ulDefaultTimeout;
         PPTRSOURCEINFO ppsi;
   } ANIMSOURCEINFO, *PANIMSOURCEINFO;

typedef struct _ANIMSOURCEINFOLIST
   {
         BOOL           fSingleAnimation;
         ANIMSOURCEINFO aasi[ NUM_OF_SYSCURSORS];
   } ANIMSOURCEINFOLIST, *PANIMSOURCEINFOLIST;

#endif // INFO_H

