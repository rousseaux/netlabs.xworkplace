
#ifndef MPTRPROP_H
#define MPTRPROP_H

// structs
typedef struct _SETTINGSHANDLERDATA
   {
         HMODULE        hmodResource;
         BOOL           fRefreshView;
         PVOID          somSelf;
         BOOL           fToggleHidePointer;
         BOOL           fHidePointerDelayChanged;
   } SETTINGSHANDLERDATA, *PSETTINGSHANDLERDATA;

// prototypes
BOOL ExecPropertiesNotebook( HWND hwnd, HMODULE hmodResource, PSETTINGSHANDLERDATA pshd);

// used by exe loader only
HWND CreateNotebookPage ( HWND hwndNotebook, HMODULE hmodResource, 
                          ULONG ulResIdDialog, ULONG ulResIdMajorTabText, 
                          ULONG ulResIdMinorTabText, PFNWP DlgWindowProc, 
                          PVOID pvParams);

#endif // MPTRPROP_H

