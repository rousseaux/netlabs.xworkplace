
#ifndef MPTRFILE_H
#define MPTRFILE_H

#include "pointers\mptrppl.h"

// Prototypes

BOOL SelectFile( HWND hwndOwner, ULONG ulDialogStyle,
                 PSZ pszInitialFilename, PULONG pulTargetFileType,
                 PSZ pszFileName, ULONG  ulBuflen);

BOOL WriteTargetFiles ( PSZ pszFileName, ULONG ulTargetFileType,
                        PPOINTERLIST ppl, BOOL fSaveAllPointers);

#endif // MPTRFILE_H

