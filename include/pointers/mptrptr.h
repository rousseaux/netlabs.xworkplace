
#ifndef MPTRPTR_H
#define MPTRPTR_H

#include "pointers\mptrppl.h"

#define POINTER_BASEPATH   "?:\\OS2\\POINTERS"

#ifndef PHPOINTER
typedef HPOINTER *PHPOINTER;
#endif

#define NUM_OF_SYSCURSORS                        9

// resource file types
#define RESFILETYPE_DEFAULT                      0
#define RESFILETYPE_POINTER                      1
#define RESFILETYPE_POINTERSET                   2
#define RESFILETYPE_CURSOR                       3
#define RESFILETYPE_CURSORSET                    4
#define RESFILETYPE_ANIMOUSE                     5
#define RESFILETYPE_ANIMOUSESET                  6
#define RESFILETYPE_WINANIMATION                 7
#define RESFILETYPE_WINANIMATIONSET              8
#define RESFILETYPE_ANMFILE                      9
#define RESFILETYPE__LAST                        9

// Prototypes
ULONG QueryPointerSysId( ULONG ulPointerIndex);
PSZ QueryResFileExtension( ULONG ulResFileType);
BOOL QueryResFileTypeFromExt( PSZ pszFileExtension, PULONG pulResFileType);

PSZ QueryPointerName( ULONG ulPtrIndex);
BOOL QueryPointerIndexFromName ( PSZ pszPointerName, PULONG pulPointerNameIndex);
BOOL QueryResFileType( PSZ pszName, PULONG pulFileType, PSZ pszFullName, ULONG ulMaxLen);

HPOINTER CreatePtrFromIconInfo( PICONINFO piconinfo);

APIRET WritePointerToPointerFile(  PSZ pszName, ULONG ulTimeout, PICONINFO pIconInfo, PSZ pszInfoName, PSZ pszInfoArtist);
APIRET WritePointerToCursorFile(  PSZ pszName, ULONG ulTimeout, PICONINFO pIconInfo, PSZ pszInfoName, PSZ pszInfoArtist);

APIRET WritePointerlistToPointerFile( PSZ pszName, PPOINTERLIST ppl, PSZ pszError, ULONG ulBuflen);
APIRET WritePointerlistToCursorFile( PSZ pszName, PPOINTERLIST ppl, PSZ pszError, ULONG ulBuflen);
APIRET WritePointerlistToWinAnimationFile( PSZ pszName, PPOINTERLIST ppl);
APIRET WritePointerlistToAnimouseFile( PSZ pszName, PPOINTERLIST ppl);

APIRET WriteAnimationSetToPointerFile( PSZ pszName, PPOINTERLIST papl, PSZ pszError, ULONG ulBuflen);
APIRET WriteAnimationSetToCursorFile( PSZ pszName, PPOINTERLIST papl, PSZ pszError, ULONG ulBuflen);
APIRET WriteAnimationSetToWinAnimationFile( PSZ pszName, PPOINTERLIST papl, PSZ pszError, ULONG ulBuflen);
APIRET WriteAnimationSetToAnimouseFile( PSZ pszName, PPOINTERLIST papl);

#endif // MPTRPTR_H

