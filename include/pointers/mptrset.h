
#ifndef MPTRSET_H
#define MPTRSET_H

#include "pointers\mptrptr.h"

#define  DEFAULT_OVERRIDETIMEOUT       FALSE
#define  DEFAULT_ACTIVATEONLOAD        TRUE
#define  DEFAULT_ANIMATIONPATH         "?:\\OS2\\POINTERS"
#define  DEFAULT_DEMO                  0
#define  DEFAULT_INITDELAY             0
#define  DEFAULT_USEMOUSESETUP         FALSE
#define  DEFAULT_BLACKWHITE            FALSE
#define  DEFAULT_HIDEPOINTER           FALSE
#define  DEFAULT_HIDEPOINTERDELAY      20
#define  DEFAULT_DRAGPTRTYPE           RESFILETYPE_POINTER
#define  DEFAULT_DRAGSETTYPE           RESFILETYPE_ANIMOUSE

#define INITDELAY_MIN                  0
#define INITDELAY_MAX                  (20 * 60)

#define HIDEPONTERDELAY_MIN            1
#define HIDEPONTERDELAY_MAX            3600

// prototypes

APIRET ScanSetupString( HWND hwnd, PRECORDCORE pcnrrec, PSZ pszSetup, BOOL fModify, BOOL fCallAsync);
APIRET QueryCurrentSettings( PSZ *ppszSettings);

BOOL  IsSettingsInitialized(VOID);

// access functions for settings
// access to DEMO, ANIMATION and POINTER implemented elsewhere

// ACTIVATEONLOAD=YES|NO
VOID  setActivateOnLoad( BOOL fActivate);
BOOL  getActivateOnLoad(VOID);

// ANIMATIONINITDELAY=n
ULONG getDefaultAnimationInitDelay(VOID);
ULONG getAnimationInitDelay(VOID);
VOID  setAnimationInitDelay(ULONG ulNewInitDelay);

// ANIMATIONPATH=...
BOOL  setAnimationPath( PSZ pszNewPath);
PCSZ  getAnimationPath(VOID);

// BLACKWHITE=YES|NO
VOID  setBlackWhite( BOOL fNewBlackWhite);
BOOL  getBlackWhite(VOID);

// DRAGPTRTYPE=PTR,CUR,ANI,AND
// use QueryResFileExt() to convert
VOID setDragPtrType( ULONG ulResFileType);
ULONG getDragPtrType(VOID);

// DRAGSETTYPE=PTR,CUR,ANI,AND
// use QueryResFileExt() to convert
VOID setDragSetType( ULONG ulResFileType);
ULONG getDragSetType(VOID);

// FRAMELENGTH=xxx,ALL|UNDEFINED
VOID  setDefaultTimeout( ULONG ulTimeout);
ULONG getDefaultTimeout(VOID);
VOID  setOverrideTimeout( BOOL fOverride);
BOOL  getOverrideTimeout(VOID);

// HIDEPOINTER=ON|OFF
VOID setHidePointer( BOOL fNewHidePointer);
BOOL getHidePointer(VOID);
VOID overrideSetHidePointer( BOOL fNewHidePointer); // use only in case of failure

// HIDEPOINTERDELAY=n
VOID  setHidePointerDelay( ULONG ulNewHidePointerDelay);
ULONG  getHidePointerDelay(VOID);

// USEMOUSESETUP=YES|NO
VOID  setUseMouseSetup( BOOL fNewUseMouseSetup);
BOOL  getUseMouseSetup(VOID);

#endif // MPTRPPL_H

