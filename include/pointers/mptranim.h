
#ifndef MPTRAMIM_H
#define MPTRAMIM_H

// some defaults
#define DEFAULT_ANIMATION_TIMEOUT                150L
#define TIMEOUT_MIN                               50L
#define TIMEOUT_MAX                             2000L
#define TIMEOUT_STEP                              50L

// prototypes

VOID _Optlink AnimationThread(PVOID pvParams); // ULONG ulParms);
MRESULT EXPENTRY ObjectWindowProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);

HAB  QueryAnimationHab(VOID);
HWND QueryAnimationHwnd(VOID);

#endif // MPTRAMIM_H

