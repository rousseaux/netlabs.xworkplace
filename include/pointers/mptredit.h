
#ifndef MPTREDIT_H
#define MPTREDIT_H

#include "pointers\mptrppl.h"

// Prototypes
BOOL BeginEditPointer( HWND hwnd, HMODULE hmodResource, PPOINTERLIST ppl);
BOOL EndEditPointer( HWND hwnd, HAPP happ, ULONG ulReturncode, PPOINTERLIST ppl);
BOOL QueryEditPending(VOID);


#endif // MPTREDIT_H

