
/*
 *@@sourcefile xwpsec32.h:
 *      declarations for xwpsec32.sys.
 *
 */

// #define MK_FP( seg, ofs )   (void*)( (unsigned long)((void*)(seg)) + (unsigned long)((void*)(ofs)) )

#define VA_START(ap, last) ap = ((va_list)__StackToFlat(&last)) + __nextword(last)

// typedef unsigned short  SEL;
typedef char            HLOCK[12];
typedef unsigned long   LINADDR;
typedef unsigned long   PHYSADDR;

// stuff in sec32_data.c
extern PVOID    G_pSecIOShared;
extern HEV      G_hevCallback;
extern ULONG    G_hmtxBufferLocked;
extern SEL      G_aGDTSels[2];
extern int      G_open_count;
extern ULONG    G_pidRing3Daemon;

// stuff elsewhere
// extern CHAR     G_szLogFile[CCHMAXPATH];
extern CHAR     G_szScratchBuf[1000];
// extern ULONG    G_ulLogSFN;
// extern struct   InfoSegGDT *G_pGDT;
// extern struct   InfoSegLDT *G_pLDT;

