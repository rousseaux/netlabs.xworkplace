
#ifndef MPTRPPL_H
#define MPTRPPL_H

#define  MAX_COUNT_POINTERS            192
#define  DEMO_TIMER_ID                 128
#define  DEMO_TIMER_TIMEOUT            DEFAULT_ANIMATION_TIMEOUT
#define  INIT_TIMER_ID                 129
#define  INIT_TIMER_TIMEOUT            1000L

// data structures

typedef struct _POINTERLIST                              // data to be copied
   {
         // --- static animation resource data
         ULONG          ulPtrId;                         // system id for pointer
         ULONG          ulResourceType;                  // resource file type
         // --- dynamic animation thread data
         BOOL           fReplaceActive;                  // currently animated
         ULONG          ulPtrIndex;                      // current ptr in animation
         ULONG          ulPtrIndexCnr;                   // current ptr in cnr
         // --- dynamic resource data
         PSZ            pszInfoName;                     // name of animation
         PSZ            pszInfoArtist;                   // name of artist
         CHAR           szAnimationName[ _MAX_PATH];     // file or directory name or zero string
         CHAR           szAnimationFile[ _MAX_PATH];     // first file of animation set
         BOOL           fModifyNameOnSave;               // shall name be modified on save
         BOOL           fAnimate;                        // animate flag
         ULONG          ulPtrCount;                      // counter of currently available pointers
         HMODULE        hmodResource;                    // module handle for ptrs in resource dlls
         ULONG          ulDefaultTimeout;                // default timeout for this pointer
         ULONG          aulTimer[ MAX_COUNT_POINTERS ];  // timeout values for each pointer
         BOOL           fStaticPointerCopied;            // static pointer is a copy
         HPOINTER       hptrStatic;                      // handle of optional static pointer
         ICONINFO       iconinfoStatic;                  // iconinfo for optional static pointer
         HPOINTER       hptr[ MAX_COUNT_POINTERS ];      // handle of the loaded pointer
         ICONINFO       iconinfo[ MAX_COUNT_POINTERS ];  // iconinfo for each pointer
   } POINTERLIST, *PPOINTERLIST;


// prototypes & access macros for mutx sem handle for data access
HMTX QueryDataAccessSem(VOID);
BOOL SetDataAccessSem( HMTX hmtx);
#define REQUEST_DATA_ACCESS_TIMEOUT(i) DosRequestMutexSem( QueryDataAccessSem(), i)
#define REQUEST_DATA_ACCESS()          DosRequestMutexSem( QueryDataAccessSem(), SEM_INDEFINITE_WAIT)
#define RELEASE_DATA_ACCESS()          DosReleaseMutexSem( QueryDataAccessSem())

// prototypes
APIRET ResetAnimation( PPOINTERLIST ppl, BOOL fSetPointer);

APIRET LoadAnimouseModule( PSZ pszModuleName, PHMODULE phmodResource);
BOOL LoadPointerAnimation( ULONG ulPtrIndex, PPOINTERLIST ppl, PSZ pszName,
                           BOOL fLoadSet, BOOL fEnableIfAnimation, BOOL fRefresh);
BOOL CopyPointerlist( PPOINTERLIST pplTarget, PPOINTERLIST pplSource, BOOL fCopyResources);

BOOL IsPointerlistInitialized( VOID);
APIRET InitializePointerlist( HAB hab, HMODULE hmodResource);
APIRET DeinitializePointerlist( VOID);
PPOINTERLIST QueryPointerlist( ULONG ulIndex);
HMTX QueryDataAccessSem(VOID);

BOOL SetAnimationPriority( ULONG ulPriorityClass, TID tid);
BOOL EnableAnimation( PPOINTERLIST ppl, BOOL fEnable);
BOOL SetNextAnimatedPointer( HAB hab, PPOINTERLIST ppl, BOOL fForceSet);


BOOL QueryDemo(VOID);
BOOL QueryAnimate( ULONG ulPointerIndex, BOOL fQueryAll);
BOOL ToggleDemo( HWND hwnd, PRECORDCORE pcnrrec, BOOL fRefresh, PBOOL pfEnable);
BOOL ProceedWithDemo( HWND hwnd, PRECORDCORE pcnrrec, BOOL fRefresh, BOOL fReset);
BOOL IsPointerActive( ULONG ulPtrId, HPOINTER *phptrCurrent);

#endif // MPTRPPL_H

