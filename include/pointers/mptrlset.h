
#ifndef MPTRLSET_H
#define MPTRLSET_H

// Prototypes
APIRET FindFiles( PVOID m_wpobject,
                  PVOID wpobject,
                  HWND hwndParent,
                  HWND hwndOwner,
                  HMODULE hmodResource,
                  PSZ pszDirectory,
                  PSZ pszFullName,
                  ULONG ulMaxLen,
                  BOOL fFindSet);

#endif // MPTRLSET_H

