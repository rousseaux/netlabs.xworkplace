
/*
 *@@sourcefile xwpsec32.h:
 *      declarations for xwpsec32.sys.
 *
 */

#define VA_START(ap, last) ap = ((va_list)__StackToFlat(&last)) + __nextword(last)

typedef unsigned long   LINADDR;
typedef unsigned long   PHYSADDR;

extern ULONG    G_pidShell;         // sec32_data.c

// stuff elsewhere
extern CHAR     G_szScratchBuf[];


