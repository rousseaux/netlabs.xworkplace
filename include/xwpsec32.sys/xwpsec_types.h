
/*
 *@@sourcefile xwpsec32.h:
 *      declarations for xwpsec32.sys.
 *
 */

#define VA_START(ap, last) ap = ((va_list)__StackToFlat(&last)) + __nextword(last)

typedef ULONG LINADDR;
typedef ULONG PHYSADDR;

extern ULONG    G_pidShell;         // sec32_data.c

extern struct InfoSegGDT
                    *G_pGDT;        // sec32_contexts.c
extern struct InfoSegLDT
                    *G_pLDT;        // sec32_contexts.c

extern BYTE     G_bLog;             // sec32_contexts.c
            #define LOG_INACTIVE        0
            #define LOG_ACTIVE          1
            #define LOG_ERROR           2

// stuff elsewhere
extern CHAR     G_szScratchBuf[];


