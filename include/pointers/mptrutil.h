
#ifndef MPTRUTIL_H
#define MPTRUTIL_H


// Prototypes
ULONG GetLoaderType(VOID);
APIRET GetModuleInfo( PFN pfn, PHMODULE phmod, PSZ pszBuffer, ULONG ulBufLen);
APIRET GetBaseName( PFN pfn, PSZ pszBuffer, ULONG ulBufLen);
APIRET LoadResourceLib( PHMODULE phmodResource);
APIRET ReoadClassLib( PHMODULE phmodResource);
APIRET GetHelpLibName( PSZ pszBuffer, ULONG ulMaxLen);
APIRET IsRemoveableMedia( PSZ pszDeviceName, PBOOL pfIsRemoveableMedia);
APIRET IsLocalDrive( PSZ pszDeviceName, PBOOL pfIsLocalDrive);
APIRET OpenTmpFile( PSZ pszBaseName, PSZ pszExtension, PHFILE phfile, PSZ pszFileName, ULONG ulMaxlen);
PSZ LoadStringResource( ULONG ulResourceType, HAB hab, HLIB hlibResource, ULONG ulResId, PSZ pszBuffer, ULONG ulBufLen);
BOOL IsWARP3(VOID);
BOOL SetPriority( ULONG ulPriorityClass);

APIRET GetNextFile( PSZ pszFileMask, PHDIR phdir, PSZ pszNextFile, ULONG ulBufLen);

BOOL FileExist( PSZ pszName, BOOL fCheckDirectory);
BOOL FileMaskExists( PSZ pszFilename, PSZ pszFirstFile, ULONG ulBuflen);
#define CHECK_DIRECTORY TRUE
#define CHECK_FILE      FALSE


BOOL IsFilenameValid( PSZ pszFilename, ULONG ulCheckType, PULONG pulValue);
#define FILENAME_CONTAINSNUMERATION  1
#define FILENAME_CONTAINSPOINTERNAME 2
#define FILENAME_CONTAINSWILDCARDS   3
#define FILENAME_CONTAINSFULLNAME    4

// --------------------------------------------------------------------------------

APIRET ChangeFilename( PSZ pszFileName, ULONG ulChangeType, PSZ pszNewName, ULONG ulBufLen,
                       PSZ pszNewNamePart, ULONG ulPointerIndex, ULONG ulNumeration);

// Styles with trailing _ are not yet implemented
#define CHANGE__MASK             0x000f
#define CHANGE_DRIVE_            0x0001
#define CHANGE_PATH_             0x0002
#define CHANGE_DRIVEPATH         0x0003
#define CHANGE_NAME_             0x0004
#define CHANGE_EXTENSION         0x0005
#define CHANGE_USEPOINTERNAME    0x0006
#define CHANGE_USEDLLNAME        0x0007
// ----
#define CHANGE_DELETE__MASK      0x00f0
#define CHANGE_DELETEDRIVE_      0x0010
#define CHANGE_DELETEPATH_       0x0020
#define CHANGE_DELETEDRIVEPATH_  0x0030
#define CHANGE_DELETEFILENAME    0x0040
#define CHANGE_DELETEEXTENSION_  0x0050
// ----
#define CHANGE_NUMERATION__MASK  0x0f00
#define CHANGE_ADDNUMERATION     0x0100
#define CHANGE_SETNUMERATION     0x0200
#define CHANGE_DELNUMERATION     0x0300

// --------------------------------------------------------------------------------

APIRET CreateTmpFile( PSZ pszMainFile, PHFILE phfile, PSZ pszBuffer, ULONG ulBufferlen);

// --------------------------------------------------------------------------------

PSZ Filespec ( PSZ pszFilename, ULONG ulPart);
#define FILESPEC_PATHNAME  1
#define FILESPEC_NAME      2
#define FILESPEC_EXTENSION 3

// --------------------------------------------------------------------------------

APIRET CopyFromFileToFile( HFILE hfileTarget, HFILE hfileSource, ULONG ulDataLen);

// --------------------------------------------------------------------------------

BOOL IsFileModified( PSZ pszFilename, PFILESTATUS3 pfs3);

// --------------------------------------------------------------------------------

MRESULT EXPENTRY SubclassStaticWindowProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);

APIRET ConvertFile(PSZ pszFilename, ULONG ulTargetFiletype);

#endif // MPTRUTIL_H

