
/*
 *@@sourcefile filesys.c:
 *      various implementation code related to file-system objects.
 *      This is mostly for code which is shared between folders
 *      and data files.
 *      So this code gets interfaced from XFolder, XFldDataFile,
 *      and XWPProgramFile.
 *
 *      This file is ALL new with V0.9.0.
 *
 *      Function prefix for this file:
 *      --  fsys*
 *
 *@@added V0.9.0 [umoeller]
 *@@header "filesys\filesys.h"
 */

/*
 *      Copyright (C) 1997-2002 Ulrich M”ller.
 *      This file is part of the XWorkplace source package.
 *      XWorkplace is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published
 *      by the Free Software Foundation, in version 2 as it comes in the
 *      "COPYING" file of the XWorkplace main distribution.
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 */

#pragma strings(readonly)

/*
 *  Suggested #include order:
 *  1)  os2.h
 *  2)  C library headers
 *  3)  setup.h (code generation and debugging options)
 *  4)  headers in helpers\
 *  5)  at least one SOM implementation header (*.ih)
 *  6)  dlgids.h, headers in shared\ (as needed)
 *  7)  headers in implementation dirs (e.g. filesys\, as needed)
 *  8)  #pragma hdrstop and then more SOM headers which crash with precompiled headers
 */

#define INCL_DOSSEMAPHORES
#define INCL_DOSPROCESS
#define INCL_DOSEXCEPTIONS
#define INCL_DOSRESOURCES
#define INCL_DOSERRORS

#define INCL_WINMESSAGEMGR
#define INCL_WINPOINTERS
#define INCL_WINDIALOGS
#define INCL_WINSTATICS
#define INCL_WINBUTTONS
#define INCL_WINENTRYFIELDS
#define INCL_WINLISTBOXES
#define INCL_WINMENUS
#define INCL_WINMLE
#define INCL_WINSTDCNR
#define INCL_WINPROGRAMLIST     // needed for PROGDETAILS, wppgm.h
#include <os2.h>

// C library headers
#include <stdio.h>
#include <setjmp.h>

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\apps.h"               // application helpers
#include "helpers\dialog.h"             // dialog helpers
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\eah.h"                // extended attributes helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\nls.h"                // National Language Support helpers
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\standards.h"          // some standard macros
#include "helpers\stringh.h"            // string helper routines
#include "helpers\threads.h"            // thread helpers
#include "helpers\winh.h"               // PM helper routines
#include "helpers\xstring.h"            // extended string helpers

// SOM headers which don't crash with prec. header files
#include "xfldr.ih"
#include "xwppgmf.ih"
#include "xwpfsys.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\classtest.h"           // some cheap funcs for WPS class checks
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\helppanels.h"          // all XWorkplace help panel IDs
#include "shared\notebook.h"            // generic XWorkplace notebook handling
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

#include "filesys\icons.h"              // icons handling
#include "filesys\filesys.h"            // various file-system object implementation code
#include "filesys\filetype.h"           // extended file types implementation
#include "filesys\folder.h"             // XFolder implementation

// other SOM headers
#pragma hdrstop                 // VAC++ keeps crashing otherwise

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

static PGEA2LIST    G_StandardGEA2List = NULL;
static ULONG        G_cbStandardGEA2List = 0;

/* ******************************************************************
 *
 *   File system information implementation
 *
 ********************************************************************/

/*
 *@@ fsysQueryEASubject:
 *      returns the contents of the .SUBJECT extended
 *      attribute in a new buffer, which must be free()'d
 *      by the caller.
 *
 *      Returns NULL on errors or if the EA doesn't exist.
 *
 *      The .SUBJECT extended attribute is a plain PSZ
 *      without line breaks.
 *
 *@@added V0.9.7 (2000-11-30) [umoeller]
 */

PSZ fsysQueryEASubject(WPFileSystem *somSelf)
{
    PSZ     psz = 0;
    CHAR    szFilename[CCHMAXPATH];
    if (_wpQueryFilename(somSelf, szFilename, TRUE))
    {
        PEABINDING  peab;
        if (peab = eaPathReadOneByName(szFilename, ".SUBJECT"))
        {
            psz = eaCreatePSZFromBinding(peab);
            eaFreeBinding(peab);
        }
    }

    return psz;
}

/*
 *@@ fsysQueryEAComments:
 *      returns the contents of the .COMMENTS extended
 *      attribute in a new buffer, which must be free()'d
 *      by the caller.
 *
 *      Returns NULL on errors or if the EA doesn't exist.
 *
 *      The .COMMENTS EA is multi-value multi-type, but all
 *      of the sub-types are EAT_ASCII. We convert all the
 *      sub-items into one string and separate the items
 *      with CR/LF.
 *
 *@@added V0.9.7 (2000-11-30) [umoeller]
 */

PSZ fsysQueryEAComments(WPFileSystem *somSelf)
{
    PSZ     psz = 0;
    CHAR    szFilename[CCHMAXPATH];
    if (_wpQueryFilename(somSelf, szFilename, TRUE))
    {
        PEABINDING  peab;
        if (peab = eaPathReadOneByName(szFilename, ".COMMENTS"))
        {
            psz = eaCreatePSZFromMVBinding(peab,
                                           "\r\n", // separator string
                                           NULL);  // codepage (not needed)
            eaFreeBinding(peab);
        }
    }

    return psz;
}

/*
 *@@ fsysQueryEAKeyphrases:
 *      returns the contents of the .KEYPHRASES extended
 *      attribute in a new buffer, which must be free()'d
 *      by the caller.
 *
 *      Returns NULL on errors or if the EA doesn't exist.
 *
 *      The .KEYPHRASES EA is multi-value multi-type, but all
 *      of the sub-types are EAT_ASCII. We convert all the
 *      sub-items into one string and separate the items
 *      with CR/LF.
 *
 *@@added V0.9.7 (2000-11-30) [umoeller]
 */

PSZ fsysQueryEAKeyphrases(WPFileSystem *somSelf)
{
    PSZ     psz = 0;
    CHAR    szFilename[CCHMAXPATH];
    if (_wpQueryFilename(somSelf, szFilename, TRUE))
    {
        PEABINDING  peab;
        if (peab = eaPathReadOneByName(szFilename, ".KEYPHRASES"))
        {
            psz = eaCreatePSZFromMVBinding(peab,
                                           "\r\n", // separator string
                                           NULL);  // codepage (not needed)

            eaFreeBinding(peab);
        }
    }

    return psz;
}

/*
 *@@ fsysSetEASubject:
 *      sets a new value for the .SUBJECT extended
 *      attribute.
 *
 *      If (psz == NULL), the EA is deleted.
 *
 *      This EA expects a plain PSZ string without
 *      line breaks.
 *
 *@@added V0.9.7 (2000-11-30) [umoeller]
 */

BOOL fsysSetEASubject(WPFileSystem *somSelf, PCSZ psz)
{
    BOOL brc = FALSE;
    CHAR    szFilename[CCHMAXPATH];

    if (_wpQueryFilename(somSelf, szFilename, TRUE))
    {
        PCSZ pcszEA = ".SUBJECT";
        if (psz)
        {
            PEABINDING  peab;
            if (peab = eaCreateBindingFromPSZ(pcszEA, psz))
            {
                brc = (NO_ERROR == eaPathWriteOne(szFilename, peab));
                eaFreeBinding(peab);
            }
        }
        else
            brc = (NO_ERROR == eaPathDeleteOne(szFilename, pcszEA));
    }

    return brc;
}

/*
 *@@ fsysSetEAComments:
 *      sets a new value for the .COMMENTS extended
 *      attribute.
 *
 *      If (psz == NULL), the EA is deleted.
 *
 *      This EA is multi-value multi-type, but all of
 *      the sub-types are EAT_ASCII. This function
 *      expects a string where several lines are
 *      separated with CR/LF, which is then converted
 *      into the multi-value EA.
 *
 *@@added V0.9.7 (2000-11-30) [umoeller]
 */

BOOL fsysSetEAComments(WPFileSystem *somSelf, PCSZ psz)
{
    BOOL brc = FALSE;
    CHAR    szFilename[CCHMAXPATH];

    if (_wpQueryFilename(somSelf, szFilename, TRUE))
    {
        PCSZ pcszEA = ".COMMENTS";
        if (psz)
        {
            PEABINDING  peab;
            if (peab = eaCreateMVBindingFromPSZ(pcszEA,
                                                psz,
                                                "\r\n",     // separator
                                                0))         // codepage
            {
                brc = (NO_ERROR == eaPathWriteOne(szFilename, peab));
                eaFreeBinding(peab);
            }
        }
        else
            brc = (NO_ERROR == eaPathDeleteOne(szFilename, pcszEA));
    }

    return brc;
}

/*
 *@@ fsysSetEAKeyphrases:
 *      sets a new value for the .KEYPHRASES extended
 *      attribute.
 *
 *      If (psz == NULL), the EA is deleted.
 *
 *      This EA is multi-value multi-type, but all of
 *      the sub-types are EAT_ASCII. This function
 *      expects a string where several lines are
 *      separated with CR/LF, which is then converted
 *      into the multi-value EA.
 *
 *@@added V0.9.7 (2000-11-30) [umoeller]
 */

BOOL fsysSetEAKeyphrases(WPFileSystem *somSelf, PCSZ psz)
{
    BOOL brc = FALSE;
    CHAR    szFilename[CCHMAXPATH];

    if (_wpQueryFilename(somSelf, szFilename, TRUE))
    {
        PCSZ pcszEA = ".KEYPHRASES";
        if (psz)
        {
            PEABINDING  peab;
            if (peab = eaCreateMVBindingFromPSZ(pcszEA,
                                                psz,
                                                "\r\n",     // separator
                                                0))         // codepage
            {
                brc = (NO_ERROR == eaPathWriteOne(szFilename, peab));
                eaFreeBinding(peab);
            }
        }
        else
            brc = (NO_ERROR == eaPathDeleteOne(szFilename, pcszEA));
    }

    return brc;
}

/* ******************************************************************
 *
 *   Populate / refresh
 *
 ********************************************************************/

/*
 *@@ fsysCreateStandardGEAList:
 *      sets up a pointer to the GEA2LIST which
 *      describes the EA names to look for during
 *      file-system populate. Since we always use
 *      the same list, we create this on
 *      M_XFolder::wpclsInitData and reuse that list
 *      forever.
 *
 *      GetGEAList then makes a copy of that for
 *      fdrPopulate. See remarks there.
 *
 *@@added V0.9.16 (2001-10-25) [umoeller]
 */

VOID fsysCreateStandardGEAList(VOID)
{
    /*
        Entries in the GEA2 list
        must be aligned on a doubleword boundary. Each oNextEntryOffset
        field must contain the number of bytes from the beginning of the
        current entry to the beginning of the next entry.

        typedef struct _GEA2LIST {
            ULONG     cbList;   // Total bytes of structure including full list.
            GEA2      list[1];  // Variable-length GEA2 structures.
        } GEA2LIST;

        typedef struct _GEA2 {
            ULONG     oNextEntryOffset;  // Offset to next entry.
            BYTE      cbName;            // Name length not including NULL.
            CHAR      szName[1];         // Attribute name.
        } GEA2;
    */

    if (!G_StandardGEA2List)
    {
        // first call:

        static PCSZ apcszEANames[] =
            {
                ".CLASSINFO",
                ".LONGNAME",
                ".TYPE",
                ".ICON"
            };

        // check how much memory we need:
        ULONG   ul;

        G_cbStandardGEA2List = sizeof(ULONG);       // GEA2LIST.cbList

        for (ul = 0;
             ul < ARRAYITEMCOUNT(apcszEANames);
             ul++)
        {
            G_cbStandardGEA2List +=   sizeof(ULONG)         // GEA2.oNextEntryOffset
                                    + sizeof(BYTE)          // GEA2.cbName
                                    + strlen(apcszEANames[ul])
                                    + 1;                    // null terminator

            // add padding, each entry must be dword-aligned
            G_cbStandardGEA2List += 4;
        }

        if (G_StandardGEA2List = (PGEA2LIST)malloc(G_cbStandardGEA2List))
        {
            PGEA2 pThis, pLast;

            G_StandardGEA2List->cbList = G_cbStandardGEA2List;
            pThis = G_StandardGEA2List->list;

            for (ul = 0;
                 ul < ARRAYITEMCOUNT(apcszEANames);
                 ul++)
            {
                pThis->cbName = strlen(apcszEANames[ul]);
                memcpy(pThis->szName,
                       apcszEANames[ul],
                       pThis->cbName + 1);

                pThis->oNextEntryOffset =   sizeof(ULONG)
                                          + sizeof(BYTE)
                                          + pThis->cbName
                                          + 1;

                // add padding, each entry must be dword-aligned
                pThis->oNextEntryOffset += 3 - ((pThis->oNextEntryOffset + 3) & 0x03);
                            // 1:   1 + 3 = 4  = 0      should be 3
                            // 2:   2 + 3 = 5  = 1      should be 2
                            // 3:   3 + 3 = 6  = 2      should be 1
                            // 4:   4 + 3 = 7  = 3      should be 0

                pLast = pThis;
                pThis = (PGEA2)(((PBYTE)pThis) + pThis->oNextEntryOffset);
            }

            pLast->oNextEntryOffset = 0;
        }
    }
}

/*
 *@@ GetGEA2List:
 *      returns a copy of the GEA2 list created by
 *      fsysCreateStandardGEAList. CPREF says that
 *      this buffer is modified internally by
 *      DosFindFirst/Next, so we need a separate
 *      buffer for each populate.
 *
 *      The caller is responsible for free()ing
 *      the buffer.
 *
 *@@added V0.9.16 (2001-12-08) [umoeller]
 */

STATIC PGEA2LIST GetGEA2List(VOID)
{
    PGEA2LIST pList;
    if (pList = (PGEA2LIST)malloc(G_cbStandardGEA2List))
        memcpy(pList, G_StandardGEA2List, G_cbStandardGEA2List);

    return pList;
}

/*
 *@@ fsysCreateFindBuffer:
 *      called from PopulateWithFileSystems to
 *      allocate and set up a buffer for DosFindFirst/Next
 *      or DosQueryPathInfo.
 *
 *      Use fsysFreeFindBuffer to free the buf again.
 *
 *@@added V0.9.16 (2001-10-28) [umoeller]
 */

APIRET fsysCreateFindBuffer(PEAOP2 *pp)
{
    APIRET arc;

    if (!(arc = DosAllocMem((PVOID*)pp,
                            FINDBUFSIZE, // 64K
                            OBJ_TILE | PAG_COMMIT | PAG_READ | PAG_WRITE)))
    {
        // set up the EAs list... sigh

        /*  CPREF: On input, pfindbuf contains an EAOP2 data structure. */
        PEAOP2      peaop2 = *pp;
                    /*  typedef struct _EAOP2 {
                          PGEA2LIST     fpGEA2List;
                          PFEA2LIST     fpFEA2List;
                          ULONG         oError;
                        } EAOP2; */

        /*  CPREF: fpGEA2List contains a pointer to a GEA2 list, which
            defines the EA names whose values are to be returned. */

        // since we try the same EAs for the objects, we
        // create a GEA2LIST only once and reuse that forever:
        peaop2->fpGEA2List = GetGEA2List();     // freed by fsysFreeFindBuffer

        // set up FEA2LIST output buffer: right after the leading EAOP2
        peaop2->fpFEA2List          = (PFEA2LIST)(peaop2 + 1);
        peaop2->fpFEA2List->cbList  =    FINDBUFSIZE   // 64K
                                       - sizeof(EAOP2);
        peaop2->oError              = 0;
    }

    return arc;
}

/*
 *@@ fsysFillFindBuffer:
 *      fills a FILEFINDBUF3 for the given file system
 *      object, including all the EAs.
 *
 *      This calls fsysCreateFindBuffer internally,
 *      so the caller is responsible for calling
 *      fsysFreeFindBuffer(peaop) when done.
 *
 *      For convenience, *ppfb3 is set to the address
 *      of the FILEFINDBUF3 within the EAOP2 buffer.
 *
 *@@added V0.9.18 (2002-03-19) [umoeller]
 */

APIRET fsysFillFindBuffer(PCSZ pcszFilename,       // in: fully q'fied filename to check
                          PFILEFINDBUF3 *ppfb3,    // out: ptr into *ppeaop
                          PEAOP2 *ppeaop)          // out: buffer to be freed
{
    APIRET arc;
    if (!(arc = fsysCreateFindBuffer(ppeaop)))
                // freed at bottom
    {
        HDIR        hdirFindHandle = HDIR_CREATE;
        ULONG       ulFindCount = 1;
        if (!(arc = DosFindFirst((PSZ)pcszFilename,
                                 &hdirFindHandle,
                                 FILE_DIRECTORY
                                    | FILE_ARCHIVED | FILE_HIDDEN | FILE_SYSTEM | FILE_READONLY,
                                 *ppeaop,     // buffer
                                 FINDBUFSIZE, // 64K
                                 &ulFindCount,
                                 FIL_QUERYEASFROMLIST)))
        {
            *ppfb3 = (PFILEFINDBUF3)(((PBYTE)*ppeaop) + sizeof(EAOP2));
        }

        DosFindClose(hdirFindHandle);
    }

    return arc;
}

/*
 *@@ fsysFreeFindBuffer:
 *
 *@@added V0.9.16 (2002-01-01) [umoeller]
 */

VOID fsysFreeFindBuffer(PEAOP2 *pp)
{
    PEAOP2 peaop;
    if (    (pp)
         && (peaop = *pp)
       )
    {
        FREE(peaop->fpGEA2List);
        DosFreeMem(peaop);

        *pp = NULL;
    }
}

/*
 *@@ FindEAValue:
 *      returns the pointer to the EA value
 *      if the EA with the given name exists
 *      in the given FEA2LIST.
 *
 *      Within the FEA structure
 *
 +          typedef struct _FEA2 {
 +              ULONG      oNextEntryOffset;  // Offset to next entry.
 +              BYTE       fEA;               // Extended attributes flag.
 +              BYTE       cbName;            // Length of szName, not including NULL.
 +              USHORT     cbValue;           // Value length.
 +              CHAR       szName[1];         // Extended attribute name.
 +          } FEA2;
 *
 *      the EA value starts right after szName (plus its null
 *      terminator). The first USHORT of the value should
 *      normally signify the type of the EA, e.g. EAT_ASCII.
 *      This returns a pointer to that type USHORT.
 *
 *@@added V0.9.16 (2001-10-25) [umoeller]
 */

PBYTE fsysFindEAValue(PFEA2LIST pFEA2List2,      // in: file EA list
                      PCSZ pcszEAName,           // in: EA name to search for (e.g. ".LONGNAME")
                      PUSHORT pcbValue)          // out: length of value (ptr can be NULL)
{
    ULONG ulEANameLen;

    /*
    typedef struct _FEA2LIST {
        ULONG     cbList;   // Total bytes of structure including full list.
                            // Apparently, if EAs aren't supported, this
                            // is == sizeof(ULONG).
        FEA2      list[1];  // Variable-length FEA2 structures.
    } FEA2LIST;

    typedef struct _FEA2 {
        ULONG      oNextEntryOffset;  // Offset to next entry.
        BYTE       fEA;               // Extended attributes flag.
        BYTE       cbName;            // Length of szName, not including NULL.
        USHORT     cbValue;           // Value length.
        CHAR       szName[1];         // Extended attribute name.
    } FEA2;
    */

    if (!pFEA2List2)
        return NULL;

    if (    (pFEA2List2->cbList > sizeof(ULONG))
                    // FAT32 and CDFS return 4 for anything here, so
                    // we better not mess with anything else; I assume
                    // any FS which doesn't support EAs will do so then
         && (pcszEAName)
         && (ulEANameLen = strlen(pcszEAName))
       )
    {
        PFEA2 pThis = &pFEA2List2->list[0];
        // maintain a current offset so we will never
        // go beyond the end of the buffer accidentally...
        // who knows what these stupid EA routines return!
        ULONG ulOfsThis = sizeof(ULONG),
              ul = 0;

        while (ulOfsThis < pFEA2List2->cbList)
        {
            if (    (ulEANameLen == pThis->cbName)
                 && (!memcmp(pThis->szName,
                             pcszEAName,
                             ulEANameLen))
               )
            {
                if (pThis->cbValue)
                {
                    PBYTE pbValue =   (PBYTE)pThis
                                    + sizeof(FEA2)
                                    + pThis->cbName;
                    if (pcbValue)
                        *pcbValue = pThis->cbValue;
                    return pbValue;
                }
                else
                    // no value:
                    return NULL;
            }

            if (!pThis->oNextEntryOffset)
                // this was the last entry:
                return NULL;

            ulOfsThis += pThis->oNextEntryOffset;

            pThis = (PFEA2)(((PBYTE)pThis) + pThis->oNextEntryOffset);
            ul++;
        } // end while
    } // end if (    (pFEA2List2->cbList > sizeof(ULONG)) ...

    return NULL;
}

// find this many files at a time
#define FIND_COUNT              300
            // doesn't make a whole lot of difference whether
            // I set this to 1 or 300 on my system, but maybe
            // on SMP systems this helps?!?

/*
 *@@ DecodeLongname:
 *
 *@@added V0.9.16 (2001-10-25) [umoeller]
 */

STATIC BOOL DecodeLongname(PFEA2LIST pFEA2List2,
                           PSZ pszLongname,          // out: .LONGNAME if TRUE is returned
                           PULONG pulNameLen)        // out: length of .LONGNAME string
{
    PBYTE pbValue;

    if (pbValue = fsysFindEAValue(pFEA2List2,
                                  ".LONGNAME",
                                  NULL))
    {
        PUSHORT pusType = (PUSHORT)pbValue;
        if (*pusType == EAT_ASCII)
        {
            // CPREF: first word after EAT_ASCII specifies length
            PUSHORT pusStringLength = pusType + 1;      // pbValue + 2
            if (*pusStringLength)
            {
                ULONG cb = _min(*pusStringLength, CCHMAXPATH - 1);
                memcpy(pszLongname,
                       pbValue + 4,
                       cb);
                pszLongname[cb] = '\0';
                *pulNameLen = cb;
                return TRUE;
            }
        }
    }

    return FALSE;
}

/*
 *@@ CLASSINFO:
 *
 *@@added V0.9.16 (2001-10-28) [umoeller]
 */

typedef struct _FSCLASSINFO
{
    ULONG   whatever;
    ULONG   cbObjData;          // if != 0, length of object data after szClassName
    CHAR    szClassName[1];
} FSCLASSINFO, *PFSCLASSINFO;

/*
 *@@ DecodeClassInfo:
 *      decodes the .CLASSINFO EA, if present.
 *      If so, it returns the name of the class
 *      to be used for the folder or file.
 *      Otherwise NULL is returned.
 *
 *      Note that this also sets the given POBJDATA
 *      pointer to the OBJDATA structure found in
 *      the classinfo. This ptr will always be set,
 *      either to NULL or the OBJDATA structure.
 *
 *      If this returns NULL, the caller is responsible
 *      for finding out the real folder or data file
 *      class to be used.
 *
 *@@added V0.9.16 (2001-10-28) [umoeller]
 */

STATIC PCSZ DecodeClassInfo(PFEA2LIST pFEA2List2,
                            PULONG pulClassNameLen,    // out: strlen of the return value
                            POBJDATA *ppObjData)       // out: OBJDATA for _wpclsMakeAwake
{
    PCSZ        pcszClassName = NULL;
    ULONG       ulClassNameLen = 0;

    PBYTE pbValue;

    *ppObjData = NULL;

    if (pbValue = fsysFindEAValue(pFEA2List2,
                                  ".CLASSINFO",
                                  NULL))
    {
        PUSHORT pusType = (PUSHORT)pbValue;
        if (*pusType == EAT_BINARY)
        {
            // CPREF: first word after EAT_BINARY specifies length
            PUSHORT pusDataLength = pusType + 1;      // pbValue + 2

            PFSCLASSINFO pInfo = (PFSCLASSINFO)(pusDataLength + 1); // pbValue + 4

            if (ulClassNameLen = strlen(pInfo->szClassName))
            {
                if (pInfo->cbObjData)
                {
                    // we have OBJDATA after szClassName:
                    *ppObjData = (POBJDATA)(   pInfo->szClassName
                                             + ulClassNameLen
                                             + 1);              // null terminator
                }

                pcszClassName = pInfo->szClassName;
            }
        }
    }

    *pulClassNameLen = ulClassNameLen;

    return pcszClassName;
}

/*
 *@@ FindBestDataFileClass:
 *      gets called for all instances of WPDataFile
 *      to find the real data file class to be used
 *      for instantiating the object.
 *
 *      Returns either the name of WPDataFile subclass,
 *      if wpclsQueryInstanceType/Filter of a class
 *      requested ownership of this object, or NULL
 *      for the default "WPDataFile".
 *
 *@@added V0.9.16 (2001-10-28) [umoeller]
 */

STATIC PCSZ FindBestDataFileClass(PFEA2LIST pFEA2List2,
                                  PCSZ pcszObjectTitle,
                                  ULONG ulTitleLen)      // in: length of title string (req.)
{
    PCSZ pcszClassName = NULL;

    PBYTE pbValue;

    if (pbValue = fsysFindEAValue(pFEA2List2,
                                  ".TYPE",
                                  NULL))
    {
        // file has .TYPE EA:
        PUSHORT pusType = (PUSHORT)pbValue;
        if (*pusType == EAT_MVMT)
        {
            // layout of EAT_MVMT:
            // 0    WORD     EAT_MVMT        (pusType)
            // 2    WORD     usCodepage      (pusType + 1, pbValue + 2)
            //               if 0, system default codepage
            // 4    WORD     cEntries        (pusType + 2, pbValue + 4)
            // 6    type 0   WORD    EAT_ASCII
            //               WORD    length
            //               CHAR[]  achType   (not null-terminated)
            //      type 1   WORD    EAT_ASCII
            //               WORD    length
            //               CHAR[]  achType   (not null-terminated)

            PUSHORT pusCodepage = pusType + 1;      // pbValue + 2
            PUSHORT pcEntries = pusCodepage + 1;    // pbValue + 4

            PBYTE   pbEAThis = (PBYTE)(pcEntries + 1);  // pbValue + 6

            ULONG ul;
            for (ul = 0;
                 ul < *pcEntries;
                 ul++)
            {
                PUSHORT pusTypeThis = (PUSHORT)pbEAThis;
                if (*pusTypeThis == EAT_ASCII)
                {
                    PUSHORT pusLengthThis = pusTypeThis + 1;
                    // next sub-EA:
                    PSZ pszType  =   pbEAThis
                                   + sizeof(USHORT)      // EAT_ASCII
                                   + sizeof(USHORT);     // usLength
                    PBYTE pbNext =   pszType
                                   + (*pusLengthThis);   // skip string

                    // null-terminate the type string
                    CHAR c = *pbNext;
                    *pbNext = '\0';
                    // pszType now has the null-terminated type string:
                    // try to find the class
                    if (pcszClassName = ftypFindClassFromInstanceType(pszType))
                        // we can stop here
                        break;

                    *pbNext = c;
                    pbEAThis = pbNext;
                }
                else
                    // non-ASCII: we cannot handle this!
                    break;
            }
        }
    }

    if (!pcszClassName)
        // instance types didn't help: then go for the
        // instance filters
        pcszClassName = ftypFindClassFromInstanceFilter(pcszObjectTitle,
                                                        ulTitleLen);

    return pcszClassName;     // can be NULL
}

/*
 *@@ RefreshOrAwake:
 *      called by PopulateWithFileSystems for each file
 *      or directory returned by DosFindFirst/Next.
 *
 *      On input, we get the sick FILEFINDBUF3 returned
 *      from DosFindFirst/Next, which contains both
 *      the EAs for the object and its real name.
 *
 *      This checks if the object is already awake in
 *      the folder. If so, it is refreshed if necessary.
 *
 *      If the object is not awake, it is awakened by
 *      a call to wpclsMakeAwake with the correct class
 *      object.
 *
 *      In any case (refresh or awakening), the object
 *      is locked once in this call.
 *
 *@@added V0.9.16 (2001-10-25) [umoeller]
 *@@changed V0.9.18 (2002-02-06) [umoeller]: fixed duplicate awakes and "treeInsert failed"
 *@@changed V0.9.19 (2002-04-14) [umoeller]: fixed missing FOUNDBIT after awake
 */

STATIC WPFileSystem* RefreshOrAwake(WPFolder *pFolder,
                                    PFILEFINDBUF3 pfb3)
{
    WPFileSystem *pAwake = NULL;

    BOOL fFolderLocked = FALSE;

    // Alright, the caller has given us a pointer
    // into the return buffer from DosFindFirst;
    // we have declared this to be a FILEFINDBUF3
    // here, but according to CPREF, the struct is
    // _without_ the cchName and achName fields...

    // My lord, who came up with this garbage?!?

    // the FEA2LIST with the EAs comes after the
    // truncated FILEFINDBUF3, that is, at the
    // address where FILEFINDBUF3.cchName would
    // normally be
    PFEA2LIST pFEA2List2 = (PFEA2LIST)(   ((PBYTE)pfb3)
                                        + FIELDOFFSET(FILEFINDBUF3,
                                                      cchName));

    // next comes a UCHAR with the name length
    PUCHAR puchNameLen = (PUCHAR)(((PBYTE)pFEA2List2) + pFEA2List2->cbList);

    // finally, the (real) name of the object
    PSZ pszRealName = ((PBYTE)puchNameLen) + sizeof(UCHAR);

    static ULONG s_ulWPDataFileLen = 0;

    // _PmpfF(("processing %s", pszRealName));

    // now, ignore "." and ".." which we don't want to see
    // in the folder, of course
    if (    (pszRealName[0] == '.')
         && (    (*puchNameLen == 1)
              || (    (*puchNameLen == 2)
                   && (pszRealName[1] == '.')
                 )
            )
       )
        return NULL;

    if (!s_ulWPDataFileLen)
        // on first call, cache length of "WPDataFile" string
        s_ulWPDataFileLen = strlen(G_pcszWPDataFile);

    TRY_LOUD(excpt1)
    {
        // sem was missing, this produced "treeInsertFailed",
        // and duplicate awakes for the same object sometimes
        // V0.9.18 (2002-02-06) [umoeller]
        if (fFolderLocked = !_wpRequestFolderMutexSem(pFolder, SEM_INDEFINITE_WAIT))
        {
            // alright, apparently we got something:
            // check if it is already awake (using the
            // fast content tree functions)
            if (pAwake = fdrFastFindFSFromName(pFolder,
                                               pszRealName))
            {
                FILEFINDBUF4        ffb4;

                _wpLockObject(pAwake);

                // now set the refresh flags... since wpPopulate gets in turn
                // called by wpRefresh, we are responsible for setting the
                // "dirty" and "found" bits here, or the object will disappear
                // from the folder on refresh.
                // For about how this works, the Warp 4 WPSREF says:

                //    1. Loop through all of the objects in the folder and turn on the DIRTYBIT
                //       and turn off the FOUNDBIT for all of your objects.
                //    2. Loop through the database. For every entry in the database, find the
                //       corresponding object.
                //         a. If the object exists, turn on the FOUNDBIT for the object.
                //         b. If the object does not exist, create a new object with the
                //            FOUNDBIT turned on and the DIRTYBIT turned off.
                //    3. Loop through the objects in the folder again. For any object that has
                //       the FOUNDBIT turned off, delete the object (since there is no longer a
                //       corresponding entry in the database). For any object that has the
                //       DIRTYBIT turned on, update the view with the current contents of the
                //       object and turn its DIRTYBIT off.

                // Note, these two wpSet/QueryRefreshFlags methods have always been in
                // the toolkit headers, but are only documented with the Warp 4 toolkit.
                // We used to have wrappers around them, but this wasn't necessary and
                // has thus been removed V0.9.20 (2002-07-25) [umoeller].

                // Now, since the objects disappear on refresh, I assume
                // we need to set the FOUNDBIT to on; since we are refreshing
                // here already, we can set DIRTYBIT to off as well.
                _wpSetRefreshFlags(pAwake,
                                   (_wpQueryRefreshFlags(pAwake)
                                        & ~DIRTYBIT)
                                        | FOUNDBIT);

                /* _wpQueryLastWrite(pAwake, &fdateLastWrite, &ftimeLastWrite);
                _wpQueryLastAccess(pAwake, &fdateLastAccess, &ftimeLastAccess);
                if (    (memcmp(&fdateLastWrite, &pfb3->fdateLastWrite, sizeof(FDATE)))
                     || (memcmp(&ftimeLastWrite, &pfb3->ftimeLastWrite, sizeof(FTIME)))
                     || (memcmp(&fdateLastAccess, &pfb3->fdateLastAccess, sizeof(FDATE)))
                     || (memcmp(&ftimeLastAccess, &pfb3->ftimeLastAccess, sizeof(FTIME)))
                   )
                */

                // this is way faster, I believe V0.9.16 (2001-12-18) [umoeller]
                _wpQueryDateInfo(pAwake, &ffb4);

                // in both ffb3 and ffb4, fdateCreation is the first date/time field;
                // FDATE and FTIME are a USHORT each, and the decl in the toolkit
                // has #pragma pack(2), so this should work
                if (memcmp(&pfb3->fdateCreation,
                           &ffb4.fdateCreation,
                           3 * (sizeof(FDATE) + sizeof(FTIME))))
                {
                    // object changed: go refresh it
                    _wpRefreshFSInfo(pAwake, NULLHANDLE, pfb3, TRUE);
                            // safe to call this method now since we have managed
                            // to override it V0.9.20 (2002-07-25) [umoeller]
                }
            }
            else
            {
                // no: wake it up then... this is terribly complicated:
                POBJDATA        pObjData = NULL;

                CHAR            szLongname[CCHMAXPATH];
                PSZ             pszTitle;
                ULONG           ulTitleLen;

                PCSZ            pcszClassName = NULL;
                ULONG           ulClassNameLen;
                somId           somidClassName;
                SOMClass        *pClassObject;

                // for the title of the new object, use the real
                // name, unless we also find a .LONGNAME attribute,
                // so decode the EA buffer
                if (DecodeLongname(pFEA2List2, szLongname, &ulTitleLen))
                    // got .LONGNAME:
                    pszTitle = szLongname;
                else
                {
                    // no .LONGNAME:
                    pszTitle = pszRealName;
                    ulTitleLen = *puchNameLen;
                }

                // NOTE about the class management:
                // At this point, we operate on class _names_
                // only and do not mess with class objects yet. This
                // is because we must take class replacements into
                // account; that is, if the object says "i wanna be
                // WPDataFile", it should really be XFldDataFile
                // or whatever other class replacements are installed.
                // While the _WPDataFile macro will not always correctly
                // resolve (since apparently this code gets called
                // too early to properly initialize the static variables
                // hidden in the macro code), somFindClass _will_
                // return the proper replacement classes.

                // _PmpfF(("checking %s", pszTitle));

                // decode the .CLASSINFO EA, which may give us a
                // class name and the OBJDATA buffer
                if (!(pcszClassName = DecodeClassInfo(pFEA2List2,
                                                      &ulClassNameLen,
                                                      &pObjData)))
                {
                    // no .CLASSINFO: use default class...
                    // if this is a directory, use _WPFolder
                    if (pfb3->attrFile & FILE_DIRECTORY)
                        pcszClassName = G_pcszWPFolder;
                    // else for WPDataFile, keep NULL so we
                    // can determine the proper class name below
                }
                else
                {
                    // we found a class name:
                    // if this is "WPDataFile", return NULL instead so we
                    // can still check for the default data file subclasses

                    // _Pmpf(("  got .CLASSINFO %s", pcszClassName));

                    if (    (s_ulWPDataFileLen == ulClassNameLen)
                         && (!memcmp(G_pcszWPDataFile, pcszClassName, s_ulWPDataFileLen))
                       )
                        pcszClassName = NULL;
                }

                if (!pcszClassName)
                {
                    // still NULL: this means we have no .CLASSINFO,
                    // or the .CLASSINFO specified "WPDataFile"
                    // (folders were ruled out before, so we do have
                    // a data file now)...
                    // for WPDataFile, we must run through the
                    // wpclsQueryInstanceType/Filter methods to
                    // find if any WPDataFile subclass wants this
                    // object to be its own (for example, .EXE files
                    // should be WPProgramFile instead)
                    pcszClassName = FindBestDataFileClass(pFEA2List2,
                                                          // title (.LONGNAME or realname)
                                                          pszTitle,
                                                          ulTitleLen);
                            // this returns either NULL or the
                            // class object of a subclass

                    // _Pmpf(("  FindBestDataFileClass = %s", pcszClassName));
                }

                if (!pcszClassName)
                    // still nothing:
                    pcszClassName = G_pcszWPDataFile;

                // now go load the class
                if (somidClassName = somIdFromString((PSZ)pcszClassName))
                {
                    if (!(pClassObject = _somFindClass(SOMClassMgrObject,
                                                       somidClassName,
                                                       0,
                                                       0)))
                    {
                        // this class is not installed:
                        // this can easily happen with multiple OS/2
                        // installations accessing the same partitions...
                        // to be on the safe side, use either
                        // WPDataFile or WPFolder then
                        if (pfb3->attrFile & FILE_DIRECTORY)
                            pcszClassName = G_pcszWPFolder;
                        else
                            pcszClassName = G_pcszWPDataFile;

                        SOMFree(somidClassName);
                        if (somidClassName = somIdFromString((PSZ)pcszClassName))
                            pClassObject = _somFindClass(SOMClassMgrObject,
                                                         somidClassName,
                                                         0,
                                                         0);
                    }
                }

                if (pClassObject)
                {
                    MAKEAWAKEFS   awfs;

                    // alright, now go make the thing AWAKE
                    awfs.pszRealName        = pszRealName;

                    memcpy(&awfs.Creation, &pfb3->fdateCreation, sizeof(FDATETIME));
                    memcpy(&awfs.LastAccess, &pfb3->fdateLastAccess, sizeof(FDATETIME));
                    memcpy(&awfs.LastWrite, &pfb3->fdateLastWrite, sizeof(FDATETIME));

                    awfs.attrFile           = pfb3->attrFile;
                    awfs.cbFile             = pfb3->cbFile;
                    awfs.cbList             = pFEA2List2->cbList;
                    awfs.pFea2List          = pFEA2List2;

                    if (pAwake = _wpclsMakeAwake(pClassObject,
                                                 pszTitle,
                                                 0,                 // style
                                                 NULLHANDLE,        // icon
                                                 pObjData,          // null if no .CLASSINFO found
                                                 pFolder,           // folder
                                                 (ULONG)&awfs))
                    {
                        #ifdef __DEBUG__
                            ULONG fl = _wpQueryRefreshFlags(pAwake);
                            PMPF_TURBOFOLDERS(("refresh flags for new \"%s\": 0x%lX (%s%s)",
                                pszRealName,
                                fl,
                                (fl & FOUNDBIT) ? "FOUNDBIT" : "",
                                (fl & DIRTYBIT) ? "DIRTYBIT" : ""));
                        #endif

                        // refresh flags are 0 always after creation,
                        // so turn on the FOUNDBIT but leave DIRTYBIT
                        // off
                        // V0.9.19 (2002-04-14) [umoeller]
                        _wpSetRefreshFlags(pAwake, FOUNDBIT);
                    }
                }

                if (somidClassName)
                    SOMFree(somidClassName);
            }
        }
    }
    CATCH(excpt1)
    {
    } END_CATCH();

    if (fFolderLocked)
        _wpReleaseFolderMutexSem(pFolder);

    return pAwake;
}

/*
 *@@ SYNCHPOPULATETHREADS:
 *      structure for communication between
 *      PopulateWithFileSystems and fntFindFiles.
 *
 *@@added V0.9.16 (2001-10-28) [umoeller]
 */

typedef struct _SYNCHPOPULATETHREADS
{
    // input parameters copied from PopulateWithFileSystems
    // so fntFindFiles knows what to look for
    PCSZ            pcszFolderFullPath;     // wpQueryFilename(somSelf, TRUE)
    PCSZ            pcszFileMask;           // NULL or file mask to look for
    BOOL            fFoldersOnly;           // TRUE if folders only

    // two 64K buffers allocated by PopulateWithFileSystems
    // for use with DosFindFirst/Next;
    // after DosFindFirst has found something in fntFindFiles,
    // PopulateWithFileSystems() can process that buffer,
    // while fntFindFiles can already run DosFindNext on the
    // second buffer.
    PEAOP2          pBuf1,      // fpGEA2List has GetGEA2List buffer to be freed
                    pBuf2;

    // current buffer to work on for PopulateWithFileSystems;
    // set by fntFindFiles after each DosFindFirst/Next.
    // This points into either pBuf1 or pBuf2 (after the
    // EAOP2 structure).
    // This must only be read or set by the owner of hmtxBuffer.
    // As a special rule, if fntFindFiles sets this to NULL,
    // it is done with DosFindFirst/Next.
    PFILEFINDBUF3   pfb3;
    ULONG           ulFindCount;        // find count from DosFindFirst/Next

    // synchronization semaphores:
    // 1) current owner of the buffer
    //    RULE: only the owner of this mutex may post or
    //    reset any of the event semaphores
    HMTX            hmtxBuffer;
    // 2) "buffer taken" event sem; posted by PopulateWithFileSystems
    //    after it has copied the pfb3 pointer (before it starts
    //    processing the buffer); fntFindFiles blocks on this before
    //    switching the buffer again
    HEV             hevBufTaken;
    // 3) "buffer changed" event sem; posted by fntFindFiles
    //    after it has switched the buffer so that
    //    PopulateWithFileSystems knows new data is available
    //    (or DosFindFirst/Next is done);
    //    PopulateWithFileSystems blocks on this before processing
    //    the buffer
    HEV             hevBufPtrChanged;

    // return code from fntFindFiles, valid only after exit
    APIRET          arcReturn;

} SYNCHPOPULATETHREADS, *PSYNCHPOPULATETHREADS;

/*
 *@@ fntFindFiles:
 *      find-files thread started by PopulateWithFileSystems.
 *      This actually does the DosFindFirst/Next loop and
 *      fills a result buffer for PopulateWithFileSystems
 *      to create objects from.
 *
 *      This allows us to get better CPU utilization since
 *      DosFindFirst/Next produce a lot of idle time (waiting
 *      for a disk transaction to complete), especially with
 *      the tons of EAs we are trying to read here.
 *      We can use this idle time to do all the CPU-intensive
 *      object creation instead of doing "find file" and
 *      "create object" synchronously.
 *
 *      Note that this requires a lot of evil synchronization
 *      between the two threads. A SYNCHPOPULATETHREADS structure
 *      is created on   PopulateWithFileSystems's stack to organize
 *      this. See remarks there for details.
 *
 *      This thread does _not_ have a message queue.
 *
 *@@added V0.9.16 (2001-10-28) [umoeller]
 *@@changed V0.9.20 (2002-07-25) [umoeller]: lowered priority
 */

STATIC void _Optlink fntFindFiles(PTHREADINFO ptiMyself)
{
    PSYNCHPOPULATETHREADS pspt = (PSYNCHPOPULATETHREADS)ptiMyself->ulData;
    HDIR            hdirFindHandle = HDIR_CREATE;

    BOOL            fSemBuffer = FALSE;
    APIRET          arc;

    TRY_LOUD(excpt1)
    {
        if (    (!(arc = DosCreateEventSem(NULL,    // unnamed
                                           &pspt->hevBufTaken,
                                           0,       // unshared
                                           FALSE))) // not posted
             && (!(arc = DosCreateEventSem(NULL,    // unnamed
                                           &pspt->hevBufPtrChanged,
                                           0,       // unshared
                                           FALSE))) // not posted
             && (!(arc = DosCreateMutexSem(NULL,
                                           &pspt->hmtxBuffer,
                                           0,
                                           TRUE)))      // request! this blocks out the
                                                        // second thread
             && (fSemBuffer = TRUE)
           )
        {
            CHAR            szFullMask[2*CCHMAXPATH];
            ULONG           attrFind;
            LONG            cb;

            ULONG           ulFindCount;

            PBYTE           pbCurrentBuffer;

            PCSZ            pcszFileMask = pspt->pcszFileMask;

            // crank up the priority of this thread so
            // that we get the CPU as soon as there's new
            // data from DosFindFirst/Next; since we are
            // blocked most of the time, this ensures
            // that the CPU is used most optimally
            DosSetPriority(PRTYS_THREAD,
                           PRTYC_TIMECRITICAL,
                           +31,
                           0);      // current thread

            // post thread semaphore so that thrCreate returns
            DosPostEventSem(ptiMyself->hevRunning);

            if (!pcszFileMask)
                pcszFileMask = "*";

            sprintf(szFullMask,
                    "%s\\%s",
                    pspt->pcszFolderFullPath,
                    pcszFileMask);

            if (pspt->fFoldersOnly)
                attrFind =   MUST_HAVE_DIRECTORY
                           | FILE_ARCHIVED | FILE_HIDDEN | FILE_SYSTEM | FILE_READONLY;
            else
                attrFind =   FILE_DIRECTORY
                           | FILE_ARCHIVED | FILE_HIDDEN | FILE_SYSTEM | FILE_READONLY;

            // on the first call, use buffer 1
            pbCurrentBuffer = (PBYTE)pspt->pBuf1;

            ulFindCount = FIND_COUNT;
            arc = DosFindFirst(szFullMask,
                               &hdirFindHandle,
                               attrFind,
                               pbCurrentBuffer,     // buffer
                               FINDBUFSIZE,
                               &ulFindCount,
                               FIL_QUERYEASFROMLIST);

            // start looping...
            while (    (arc == NO_ERROR)
                    || (arc == ERROR_BUFFER_OVERFLOW)
                  )
            {
                // go process this file or directory
                ULONG ulPosted;

                // On output from DosFindFirst/Next, the buffer
                // has the EAOP2 struct first, which we no
                // longer care about... after that comes
                // a truncated FILEFINDBUF3 with all the
                // data we need, so give this to the populate
                // thread, which calls RefreshOrAwake.

                // Note that at this point, we _always_ own
                // hmtxBuffer; on the first loop because it
                // was initially requested, and later on
                // because of the explicit request below.

                // 1) set buffer pointer for populate thread
                pspt->pfb3 = (PFILEFINDBUF3)(pbCurrentBuffer + sizeof(EAOP2));
                pspt->ulFindCount = ulFindCount;        // items found
                // 2) unset "buffer taken" event sem
                DosResetEventSem(pspt->hevBufTaken, &ulPosted);
                // 3) tell second thread we're going for DosFindNext
                // now, which will block on this
                // _PmpfF(("posting hevBufPtrChanged"));
                DosPostEventSem(pspt->hevBufPtrChanged);

                if (!ptiMyself->fExit)
                {
                    // 4) release buffer mutex; the second thread
                    //    is blocked on this and will then run off
                    // _PmpfF(("releasing hmtxBuffer"));
                    DosReleaseMutexSem(pspt->hmtxBuffer);
                    fSemBuffer = FALSE;

                    // _PmpfF(("blocking on hevBufTaken"));
                    if (    (!ptiMyself->fExit)
                         && (!(arc = DosWaitEventSem(pspt->hevBufTaken,
                                                     SEM_INDEFINITE_WAIT)))
                            // check again, second thread might be exiting now
                         && (!ptiMyself->fExit)
                       )
                    {
                        // alright, we got something else:
                        // request the buffer mutex again
                        // and re-loop; above, we will block
                        // again until the second thread has
                        // taken the new buffer
                        // _PmpfF(("blocking on hmtxBuffer"));
                        if (!(arc = DosRequestMutexSem(pspt->hmtxBuffer,
                                                       SEM_INDEFINITE_WAIT)))
                        {
                            fSemBuffer = TRUE;
                            // switch the buffer so we can load next file
                            // while second thread is working on the
                            // previous one
                            if (pbCurrentBuffer == (PBYTE)pspt->pBuf1)
                                pbCurrentBuffer = (PBYTE)pspt->pBuf2;
                            else
                                pbCurrentBuffer = (PBYTE)pspt->pBuf1;

                            // find next:
                            // _PmpfF(("DosFindNext"));
                            ulFindCount = FIND_COUNT;
                            arc = DosFindNext(hdirFindHandle,
                                              pbCurrentBuffer,
                                              FINDBUFSIZE,
                                              &ulFindCount);
                        }
                    } // end if !DosWaitEventSem(pspt->hevBufTaken)

                } // if (!ptiMyself->fExit)

                if (ptiMyself->fExit)
                    // we must exit for some reason:
                    break;

            } // while (arc == NO_ERROR)

            if (arc == ERROR_NO_MORE_FILES)
            {
                // nothing found is not an error
                arc = NO_ERROR;
            }
        }
    }
    CATCH(excpt1)
    {
        arc = ERROR_PROTECTION_VIOLATION;
    } END_CATCH();

    // post thread semaphore so that thrCreate returns,
    // in case we haven't even gotten to the above call
    DosPostEventSem(ptiMyself->hevRunning);

    // cleanup:

    DosFindClose(hdirFindHandle);

    if (!fSemBuffer)
        if (!DosRequestMutexSem(pspt->hmtxBuffer,
                                SEM_INDEFINITE_WAIT))
            fSemBuffer = TRUE;

    // tell populate thread we're done
    if (fSemBuffer)
    {
        // buffer == NULL means no more data
        pspt->pfb3 = NULL;

        // post "buf changed" because populate
        // blocks on this
        DosPostEventSem(pspt->hevBufPtrChanged);
        DosReleaseMutexSem(pspt->hmtxBuffer);
    }

    // return what we have
    pspt->arcReturn = arc;

    // _PmpfF(("exiting"));
}

/*
 *@@ fsysPopulateWithFSObjects:
 *      called from fdrPopulate to get the file-system
 *      objects.
 *
 *      This starts off a second thread which does
 *      the DosFindFirst/Next loop. See fntFindFiles.
 *
 *      --  For file-system objects,
 *          fsysPopulateWithFSObjects() starts a second
 *          thread which does the actual DosFindFirst/Next
 *          processing (see fntFindFiles). This allows
 *          us to use the idle time produced by
 *          DosFindFirst/Next for the SOM processing
 *          which is quite expensive.
 *
 *      --  Since we can use our fast folder content
 *          trees (see fdrSafeFindFSFromName), this
 *          is a _lot_ faster for folders with many
 *          file system objects because we can check
 *          much more quickly if an object is already
 *          awake.
 *
 *      Benchmarks (pure populate with the
 *      QUICKOPEN=IMMEDIATE setup string, so no container
 *      management involved):
 *
 +      +--------------------+-------------+-------------+-------------+
 +      |                    | turbo on    | turbo on    |  turbo off  |
 +      |                    | findcnt 1   | findcnt 300 |             |
 +      +--------------------+-------------+-------------+-------------+
 +      |   JFS folder with  |     53 s    |             |      160 s  |
 +      |   10000 files      |             |             |             |
 +      +--------------------+-------------+-------------+-------------+
 +      |   JFS folder with  |     60 s    |     60 s    |      211 s  |
 +      |   13000 files      |             |             |             |
 +      +--------------------+-------------+-------------+-------------+
 +      |   HPFS folder with |     56 s    |             |             |
 +      |   10.000 files     |             |             |             |
 +      +--------------------+-------------+-------------+-------------+
 *
 *      Obviously, the time that the default WPS populate
 *      takes increases exponentially with the no. of objects
 *      in the folder. As a result, the fuller a folder is,
 *      the better this replacement becomes in comparison.
 *
 *      Two bottlenecks remain for folder populating...
 *      one is DosFindFirst/Next, which is terminally
 *      slow (and which I cannot fix), the other is the
 *      record management in the containers.
 *
 *@@added V0.9.16 (2001-10-25) [umoeller]
 */

BOOL fsysPopulateWithFSObjects(WPFolder *somSelf,
                               HWND hwndReserved,
                               PMINIRECORDCORE pMyRecord,
                               PCSZ pcszFolderFullPath,  // in: wpQueryFilename(somSelf, TRUE)
                               BOOL fFoldersOnly,
                               PCSZ pcszFileMask,     // in: file mask or NULL for "*" (ignored if fFoldersOnly)
                               PBOOL pfExit)          // in: exit flag
{
    APIRET      arc;

    THREADINFO  tiFindFiles;
    volatile TID tidFindFiles = 0;

    // structure on stack to synchronize our two threads
    SYNCHPOPULATETHREADS spt;

    BOOL        fBufSem = FALSE;

    memset(&spt, 0, sizeof(spt));
    spt.pcszFileMask = pcszFileMask;
    spt.pcszFolderFullPath = pcszFolderFullPath;
    spt.fFoldersOnly = fFoldersOnly;

            // allocate two 64K buffers
    if (    (!(arc = fsysCreateFindBuffer(&spt.pBuf1)))
         && (!(arc = fsysCreateFindBuffer(&spt.pBuf2)))
            // create the find-files thread
         && (thrCreate(&tiFindFiles,
                       fntFindFiles,
                       &tidFindFiles,
                       "FindFiles",
                       THRF_WAIT_EXPLICIT,      // no PM msg queue!
                       (ULONG)&spt))
       )
    {
        TRY_LOUD(excpt1)
        {
            while (!arc)
            {
                // go block until find-files has set the buf ptr
                // _PmpfF(("blocking on hevBufPtrChanged"));
                if (!(arc = DosWaitEventSem(spt.hevBufPtrChanged,
                                            SEM_INDEFINITE_WAIT)))
                {
                    // _PmpfF(("blocking on hmtxBuffer"));
                    if (!(arc = DosRequestMutexSem(spt.hmtxBuffer,
                                                   SEM_INDEFINITE_WAIT)))
                    {
                        // OK, find-files released that sem:
                        // we either have data now or we're done
                        PFILEFINDBUF3   pfb3;
                        ULONG           ulFindCount;
                        ULONG           ulPosted;

                        fBufSem = TRUE;

                        DosResetEventSem(spt.hevBufPtrChanged, &ulPosted);

                        // take the buffer pointer and the find count
                        pfb3 = spt.pfb3;
                        ulFindCount = spt.ulFindCount;

                        // tell find-files we've taken that buffer
                        DosPostEventSem(spt.hevBufTaken);
                        // release buffer mutex, on which
                        // find-files may have blocked
                        DosReleaseMutexSem(spt.hmtxBuffer);
                        fBufSem = FALSE;

                        if (pfb3)
                        {
                            // we have more data:
                            // run thru the buffer array
                            ULONG ul;
                            for (ul = 0;
                                 ul < ulFindCount;
                                 ul++)
                            {
                                // process this item
                                RefreshOrAwake(somSelf,     // folder
                                               pfb3);       // file
                                // _PmpfF(("done with RefreshOrAwake"));

                                // next item in buffer
                                if (pfb3->oNextEntryOffset)
                                    pfb3 = (PFILEFINDBUF3)(   (PBYTE)pfb3
                                                            + pfb3->oNextEntryOffset
                                                          );
                            }

                            if (hwndReserved)
                            {
                                WinPostMsg(hwndReserved,
                                           0x0405,
                                           (MPARAM)-1,
                                           (MPARAM)pMyRecord);
                                // do this only once, or the folder
                                // chokes on the number of objects inserted
                                hwndReserved = NULLHANDLE;
                            }
                        }
                        else
                            // no more data, exit now!
                            break;
                    }
                }

                if (*pfExit)
                    arc = -1;

            } // end while (!arc)
        }
        CATCH(excpt1)
        {
            arc = ERROR_PROTECTION_VIOLATION;
        } END_CATCH();

        // tell find-files to exit too
        tiFindFiles.fExit = TRUE;

        // in case find-files is still blocked on this
        DosPostEventSem(spt.hevBufTaken);

        if (fBufSem)
            DosReleaseMutexSem(spt.hmtxBuffer);

        // wait for thread to terminate
        // before freeing the buffers!!
        while (tidFindFiles)
        {
            // _PmpfF(("tidFindFiles %lX is %d",
               //  &tidFindFiles,
                // tidFindFiles));
            DosSleep(0);
        }
    }

    fsysFreeFindBuffer(&spt.pBuf1);
    fsysFreeFindBuffer(&spt.pBuf2);

    if (spt.hevBufTaken)
        DosCloseEventSem(spt.hevBufTaken);
    if (spt.hevBufPtrChanged)
        DosCloseEventSem(spt.hevBufPtrChanged);
    if (spt.hmtxBuffer)
        DosCloseMutexSem(spt.hmtxBuffer);

    if (!arc)
        arc = spt.arcReturn;

    // return TRUE if no error
    return !arc;
}

/*
 *@@ fsysRefresh:
 *      implementation for our replacement XWPFileSystem::wpRefresh.
 *
 *@@added V0.9.16 (2001-12-08) [umoeller]
 */

APIRET fsysRefresh(WPFileSystem *somSelf,
                   PVOID pvReserved)
{
    APIRET          arc = NO_ERROR;

    PFILEFINDBUF3   pfb3 = NULL;
    PEAOP2          peaop = NULL;

    CHAR            szFilename[CCHMAXPATH];

    if (pvReserved)
        // caller gave us data (probably from wpPopulate):
        // use that
        pfb3 = (PFILEFINDBUF3)pvReserved;
    else
    {
        // no info given: get it then
        if (!_wpQueryFilename(somSelf, szFilename, TRUE))
            arc = ERROR_FILE_NOT_FOUND;
        else
            arc = fsysFillFindBuffer(szFilename, &pfb3, &peaop);
    }

    if (!arc)
    {
        // alright, we got valid information, either from the
        // caller, or we got it ourselves:

        WPObject *pobjLock = NULL;

        TRY_LOUD(excpt1)
        {
            if (pobjLock = cmnLockObject(somSelf))
            {
                XWPFileSystemData *somThis = XWPFileSystemGetData(somSelf);

                ULONG flRefresh = _wpQueryRefreshFlags(somSelf);

                PFEA2LIST pFEA2List2 = (PFEA2LIST)(   ((PBYTE)pfb3)
                                                    + FIELDOFFSET(FILEFINDBUF3,
                                                                  cchName));

                // next comes a UCHAR with the name length
                PUCHAR puchNameLen = (PUCHAR)(((PBYTE)pFEA2List2) + pFEA2List2->cbList);

                // finally, the (real) name of the object
                PSZ pszRealName = ((PBYTE)puchNameLen) + sizeof(UCHAR);

                PMINIRECORDCORE prec = _wpQueryCoreRecord(somSelf);
                HPOINTER hptrNew = NULLHANDLE;

                if (flRefresh & 0x20000000)
                    _wpSetRefreshFlags(somSelf, flRefresh & ~0x20000000);

                // set the instance variable for wpCnrRefreshDetails to
                // 0 so that we can count pending changes... see
                // XWPFileSystem::wpCnrRefreshDetails
                _ulCnrRefresh = 0;

                // refresh various file-system info
                _wpSetAttr(somSelf, pfb3->attrFile);
                        // this calls _wpCnrRefreshDetails

                // this is funny: we can pass in pFEA2List2, but only
                // if we also set pszTypes to -1... so much for lucid APIs
                _wpSetType(somSelf,
                           (PSZ)-1,
                           pFEA2List2);
                        // this does not call _wpCnrRefreshDetails
                _wpSetDateInfo(somSelf, (PFILEFINDBUF4)pfb3);
                        // this does not call _wpCnrRefreshDetails
                _wpSetFileSizeInfo(somSelf,
                                   pfb3->cbFile,        // file size
                                   pFEA2List2->cbList);       // EA size
                        // this calls _wpCnrRefreshDetails

                // refresh the icon... if prec->hptrIcon is still
                // NULLHANDLE, the WPS hasn't loaded the icon yet,
                // so there's no need to refresh
                if (prec->hptrIcon)
                {
                    // alright, we had an icon previously:
                    ULONG flNewStyle = 0;

                    // check if we have an .ICON EA... in that case,
                    // always set the icon data
                    if (!(arc = icoBuildPtrFromFEA2List(pFEA2List2,
                                                        &hptrNew,
                                                        NULL,
                                                        NULL)))
                    {
                        _wpSetIcon(somSelf, hptrNew);
                        flNewStyle = OBJSTYLE_NOTDEFAULTICON;
                        (_ulCnrRefresh)++;
                    }

                    // if we didn't get an icon, do some default stuff
                    if (!hptrNew)
                    {
                        if (_somIsA(somSelf, _WPProgramFile))
                            // try to refresh the program icon,
                            // in case somebody just ran the resource
                            // compiler or something
                            _wpSetProgIcon(somSelf, NULL);
                        else if (    (ctsIsIcon(somSelf))
                                  || (ctsIsPointer(somSelf))
                                )
                        {
                            _wpQueryFilename(somSelf, szFilename, TRUE);
                            if (!icoLoadICOFile(szFilename,
                                                &hptrNew,
                                                NULL,
                                                NULL))
                            {
                                _wpSetIcon(somSelf, hptrNew);
                                flNewStyle = OBJSTYLE_NOTDEFAULTICON;
                            }
                        }
                        else if (_somIsA(somSelf, _WPDataFile))
                            // data file other than program file:
                            // set the association icon
                            _wpSetAssociatedFileIcon(somSelf);
                    }

                    _wpModifyStyle(somSelf,
                                   OBJSTYLE_NOTDEFAULTICON,
                                   flNewStyle);
                }

                if (_ulCnrRefresh)
                {
                    // something changed:
                    _ulCnrRefresh = -1;
                    _wpCnrRefreshDetails(somSelf);
                }
                else
                    _ulCnrRefresh = -1;
            }
        }
        CATCH(excpt1)
        {
            arc = ERROR_PROTECTION_VIOLATION;
        } END_CATCH();

        if (pobjLock)
            _wpReleaseObjectMutexSem(pobjLock);
    }

    fsysFreeFindBuffer(&peaop);

    return arc;
}

/* ******************************************************************
 *
 *   "File" page 1 replacement in WPDataFile/WPFolder
 *
 ********************************************************************/

#define INIT_DATE_TBR     "00.00.0000  "
#define INIT_TIME_TBR     "00:00:00"

#define LEFT_COLUMN_WIDTH           50

#define DATETIME_TABLE_WIDTH        125
            // specified with the datetime group; dialog.c adds spacing around that
#define DATETIME_ACTUAL_WIDTH   (DATETIME_TABLE_WIDTH + (2 * COMMON_SPACING) + (2 * GROUP_INNER_SPACING_X))
            // this is the actual size produced by the dialog formatter

#define ATTR_TABLE_WIDTH            65
            // specified with the attribs group; dialog.c adds spacing around that
#define ATTR_ACTUAL_WIDTH       (ATTR_TABLE_WIDTH + (2 * COMMON_SPACING) + (2 * GROUP_INNER_SPACING_X))
            // this is the actual size produced by the dialog formatter

// now calculate the size of the "information" table; we must specify
// the size of the table, not of the PM group control, so calc reversely
// 1) actual width of the group is the same as the above two actual width
#define INFO_ACTUAL_WIDTH        (DATETIME_ACTUAL_WIDTH + ATTR_ACTUAL_WIDTH)
// 2) inner table width (to be specified) is that without the group spacings
#define INFO_TABLE_WIDTH        (INFO_ACTUAL_WIDTH - (4 * COMMON_SPACING) - (2 * GROUP_INNER_SPACING_X))

#define MLE_WIDTH               ((INFO_TABLE_WIDTH - 2 * COMMON_SPACING) / 2)
#define MLE_HEIGHT              25

#define REAL_NAME_WIDTH         (INFO_ACTUAL_WIDTH - LEFT_COLUMN_WIDTH - 2 * COMMON_SPACING)

static const CONTROLDEF
    RealNameTxt = CONTROLDEF_TEXT(
                            LOAD_STRING,
                            ID_XSDI_FILES_REALNAME_TXT,
                            LEFT_COLUMN_WIDTH,
                            -1),
    RealNameData = CONTROLDEF_TEXT(
                            "W",
                            ID_XSDI_FILES_REALNAME,
                            REAL_NAME_WIDTH,
                            -1),
    SizeTxt = CONTROLDEF_TEXT(
                            LOAD_STRING,
                            ID_XSDI_FILES_FILESIZE_TXT,
                            LEFT_COLUMN_WIDTH,
                            -1),
    SizeData = CONTROLDEF_TEXT(
                            LOAD_STRING,
                            ID_XSDI_FILES_FILESIZE,
                            -1,
                            -1),
    WorkAreaCB = LOADDEF_AUTOCHECKBOX(ID_XSDI_FILES_WORKAREA),
    DateTimeGroup = CONTROLDEF_GROUP(
                            LOAD_STRING,
                            ID_XSDI_FILES_DATETIME_GROUP,
                            DATETIME_TABLE_WIDTH,
                            -1),
    CreationTxt = CONTROLDEF_TEXT(
                            LOAD_STRING,
                            ID_XSDI_FILES_CREATIONDATE_TXT,
                            LEFT_COLUMN_WIDTH,
                            -1),
    CreationDate = CONTROLDEF_TEXT(
                            INIT_DATE_TBR,
                            ID_XSDI_FILES_CREATIONDATE,
                            -1,
                            -1),
    CreationTime = CONTROLDEF_TEXT(
                            INIT_TIME_TBR,
                            ID_XSDI_FILES_CREATIONTIME,
                            -1,
                            -1),
    LastWriteTxt = CONTROLDEF_TEXT(
                            LOAD_STRING,
                            ID_XSDI_FILES_LASTWRITEDATE_TXT,
                            LEFT_COLUMN_WIDTH,
                            -1),
    LastWriteDate = CONTROLDEF_TEXT(
                            INIT_DATE_TBR,
                            ID_XSDI_FILES_LASTWRITEDATE,
                            -1,
                            -1),
    LastWriteTime = CONTROLDEF_TEXT(
                            INIT_TIME_TBR,
                            ID_XSDI_FILES_LASTWRITETIME,
                            -1,
                            -1),
    LastAccessTxt = CONTROLDEF_TEXT(
                            LOAD_STRING,
                            ID_XSDI_FILES_LASTACCESSDATE_TXT,
                            LEFT_COLUMN_WIDTH,
                            -1),
    LastAccessDate = CONTROLDEF_TEXT(
                            INIT_DATE_TBR,
                            ID_XSDI_FILES_LASTACCESSDATE,
                            -1,
                            -1),
    LastAccessTime = CONTROLDEF_TEXT(
                            INIT_TIME_TBR,
                            ID_XSDI_FILES_LASTACCESSTIME,
                            -1,
                            -1),
    AttrGroup = CONTROLDEF_GROUP(
                            LOAD_STRING,
                            ID_XSDI_FILES_ATTR_GROUP,
                            ATTR_TABLE_WIDTH,
                            -1),
    AttrArchivedCB = LOADDEF_AUTOCHECKBOX(ID_XSDI_FILES_ATTR_ARCHIVED),
    AttrReadOnlyCB = LOADDEF_AUTOCHECKBOX(ID_XSDI_FILES_ATTR_READONLY),
    AttrSystemCB = LOADDEF_AUTOCHECKBOX(ID_XSDI_FILES_ATTR_SYSTEM),
    AttrHiddenCB = LOADDEF_AUTOCHECKBOX(ID_XSDI_FILES_ATTR_HIDDEN),
    InfoGroup = CONTROLDEF_GROUP(
                            LOAD_STRING,
                            ID_XSDI_FILES_INFO_GROUP,
                            INFO_TABLE_WIDTH,
                            -1),
    SubjectTxt = CONTROLDEF_TEXT(
                            LOAD_STRING,
                            ID_XSDI_FILES_SUBJECT_TXT,
                            LEFT_COLUMN_WIDTH,
                            -1),
    SubjectEF = CONTROLDEF_ENTRYFIELD(
                            NULL,
                            ID_XSDI_FILES_SUBJECT,
                            INFO_TABLE_WIDTH - LEFT_COLUMN_WIDTH - 2 * COMMON_SPACING,
                            -1),
    CommentsTxt = CONTROLDEF_TEXT(
                            LOAD_STRING,
                            ID_XSDI_FILES_COMMENTS_TXT,
                            -1,
                            -1),
    CommentsMLE = CONTROLDEF_MLE(
                            NULL,
                            ID_XSDI_FILES_COMMENTS,
                            MLE_WIDTH,
                            MLE_HEIGHT),
    KeyphrasesTxt = CONTROLDEF_TEXT(
                            LOAD_STRING,
                            ID_XSDI_FILES_KEYPHRASES_TXT,
                            -1,
                            -1),
    KeyphrasesMLE = CONTROLDEF_MLE(
                            NULL,
                            ID_XSDI_FILES_KEYPHRASES,
                            MLE_WIDTH,
                            MLE_HEIGHT);

static const DLGHITEM dlgFile1[] =
    {
        START_TABLE,            // root table, required
            START_ROW(0),
                CONTROL_DEF(&RealNameTxt),
                CONTROL_DEF(&RealNameData),
            START_ROW(0),
                CONTROL_DEF(&SizeTxt),
                CONTROL_DEF(&SizeData),
            START_ROW(0),
                START_TABLE,
                    START_ROW(0),
                        CONTROL_DEF(&WorkAreaCB),
                    START_ROW(0),
                        START_GROUP_TABLE(&DateTimeGroup),
                            START_ROW(ROW_VALIGN_CENTER),
                                CONTROL_DEF(&CreationTxt),
                                CONTROL_DEF(&CreationDate),
                                CONTROL_DEF(&CreationTime),
                            START_ROW(ROW_VALIGN_CENTER),
                                CONTROL_DEF(&LastWriteTxt),
                                CONTROL_DEF(&LastWriteDate),
                                CONTROL_DEF(&LastWriteTime),
                            START_ROW(ROW_VALIGN_CENTER),
                                CONTROL_DEF(&LastAccessTxt),
                                CONTROL_DEF(&LastAccessDate),
                                CONTROL_DEF(&LastAccessTime),
                        END_TABLE,
                END_TABLE,
                START_GROUP_TABLE(&AttrGroup),
                    START_ROW(0),
                        CONTROL_DEF(&AttrArchivedCB),
                    START_ROW(0),
                        CONTROL_DEF(&AttrReadOnlyCB),
                    START_ROW(0),
                        CONTROL_DEF(&AttrSystemCB),
                    START_ROW(0),
                        CONTROL_DEF(&AttrHiddenCB),
                END_TABLE,
            START_ROW(0),
                START_GROUP_TABLE(&InfoGroup),
                    START_ROW(ROW_VALIGN_CENTER),
                        CONTROL_DEF(&SubjectTxt),
                        CONTROL_DEF(&SubjectEF),
                    START_ROW(0),
                        START_TABLE,
                            START_ROW(0),
                                CONTROL_DEF(&CommentsTxt),
                            START_ROW(0),
                                CONTROL_DEF(&CommentsMLE),
                        END_TABLE,
                        START_TABLE,
                            START_ROW(0),
                                CONTROL_DEF(&KeyphrasesTxt),
                            START_ROW(0),
                                CONTROL_DEF(&KeyphrasesMLE),
                        END_TABLE,
                END_TABLE,
            START_ROW(0),
                CONTROL_DEF(&G_UndoButton),         // common.c
                CONTROL_DEF(&G_DefaultButton),      // common.c
                CONTROL_DEF(&G_HelpButton),         // common.c
        END_TABLE
    };

static MPARAM G_ampFile1Page[] =
    {
        MPFROM2SHORT(ID_XSDI_FILES_REALNAME_TXT, XAC_MOVEY),
        MPFROM2SHORT(ID_XSDI_FILES_REALNAME, XAC_MOVEY | XAC_SIZEX),
        MPFROM2SHORT(ID_XSDI_FILES_FILESIZE_TXT, XAC_MOVEY),
        MPFROM2SHORT(ID_XSDI_FILES_FILESIZE, XAC_MOVEY | XAC_SIZEX),
        MPFROM2SHORT(ID_XSDI_FILES_WORKAREA, XAC_MOVEY),
        MPFROM2SHORT(ID_XSDI_FILES_DATETIME_GROUP, XAC_MOVEY | XAC_SIZEX),
        MPFROM2SHORT(ID_XSDI_FILES_CREATIONDATE_TXT, XAC_MOVEY | XAC_SIZEX),
        MPFROM2SHORT(ID_XSDI_FILES_CREATIONDATE, XAC_MOVEY | XAC_MOVEX),
        MPFROM2SHORT(ID_XSDI_FILES_CREATIONTIME, XAC_MOVEY | XAC_MOVEX),
        MPFROM2SHORT(ID_XSDI_FILES_LASTWRITEDATE_TXT, XAC_MOVEY | XAC_SIZEX),
        MPFROM2SHORT(ID_XSDI_FILES_LASTWRITEDATE, XAC_MOVEY | XAC_MOVEX),
        MPFROM2SHORT(ID_XSDI_FILES_LASTWRITETIME, XAC_MOVEY | XAC_MOVEX),
        MPFROM2SHORT(ID_XSDI_FILES_LASTACCESSDATE_TXT, XAC_MOVEY | XAC_SIZEX),
        MPFROM2SHORT(ID_XSDI_FILES_LASTACCESSDATE, XAC_MOVEY | XAC_MOVEX),
        MPFROM2SHORT(ID_XSDI_FILES_LASTACCESSTIME, XAC_MOVEY | XAC_MOVEX),
        MPFROM2SHORT(ID_XSDI_FILES_ATTR_GROUP, XAC_MOVEX | XAC_MOVEY),
        MPFROM2SHORT(ID_XSDI_FILES_ATTR_ARCHIVED, XAC_MOVEX | XAC_MOVEY),
        MPFROM2SHORT(ID_XSDI_FILES_ATTR_READONLY, XAC_MOVEX | XAC_MOVEY),
        MPFROM2SHORT(ID_XSDI_FILES_ATTR_SYSTEM, XAC_MOVEX | XAC_MOVEY),
        MPFROM2SHORT(ID_XSDI_FILES_ATTR_HIDDEN, XAC_MOVEX | XAC_MOVEY),
        MPFROM2SHORT(ID_XSDI_FILES_INFO_GROUP, XAC_SIZEX | XAC_SIZEY),
        MPFROM2SHORT(ID_XSDI_FILES_SUBJECT_TXT, XAC_MOVEY),
        MPFROM2SHORT(ID_XSDI_FILES_SUBJECT, XAC_SIZEX | XAC_MOVEY),
        MPFROM2SHORT(ID_XSDI_FILES_COMMENTS_TXT, XAC_MOVEY),
        MPFROM2SHORT(ID_XSDI_FILES_COMMENTS, XAC_SIZEX | XAC_SIZEY),
        MPFROM2SHORT(ID_XSDI_FILES_KEYPHRASES_TXT, XAC_MOVEX | XAC_MOVEY),
        MPFROM2SHORT(ID_XSDI_FILES_KEYPHRASES, XAC_MOVEX | XAC_SIZEY)
    };

STATIC ULONG G_cFile1Page = ARRAYITEMCOUNT(G_ampFile1Page);

/*
 *@@ FILEPAGEDATA:
 *      structure used for backing up file page data
 *      (Undo button on "File" page).
 */

typedef struct _FILEPAGEDATA
{
    // file attributes backup
    ULONG       ulAttr;
    // EA backups
    PSZ         pszSubject,
                pszComments,
                pszKeyphrases;
} FILEPAGEDATA, *PFILEPAGEDATA;

/*
 *@@ SetDlgDateTime:
 *
 *@@added V0.9.18 (2002-02-06) [umoeller]
 */

STATIC VOID SetDlgDateTime(HWND hwndDlg,           // in: dialog
                           ULONG idDate,           // in: dialog item ID for date string
                           ULONG idTime,           // in: dialog item ID for time string
                           PFDATE pfDate,          // in: file info
                           PFTIME pfTime,
                           PCOUNTRYSETTINGS pcs)   // in: country settings
{
    CHAR    szTemp[100];

    nlsFileDate(szTemp,
                pfDate,
                pcs->ulDateFormat,
                pcs->cDateSep);
    WinSetDlgItemText(hwndDlg,
                      idDate,
                      szTemp);

    nlsFileTime(szTemp,
                pfTime,
                pcs->ulTimeFormat,
                pcs->cTimeSep);
    WinSetDlgItemText(hwndDlg,
                      idTime,
                      szTemp);
}

/*
 *@@ fsysFile1InitPage:
 *      first "File" page notebook callback function (notebook.c).
 *      Sets the controls on the page according to a file's
 *      filesystem characteristics.
 *
 *      This is used by both XFolder and XFldDataFile.
 *
 *      Notes about the EAs which are parsed here (also see the
 *      "Extended Attributes" section in CPREF):
 *
 *      --  The "Subject" field on the page corresponds to
 *          the .SUBJECT EA. This is a single-type EAT_ASCII EA.
 *
 *      --  The "Comments" field corresponds to the .COMMENTS EA.
 *          This is multi-value, multi-type (EAT_MVMT), but all the
 *          subvalues are of EAT_ASCII. All lines in the default
 *          WPS "File" multi-line entry field terminated by CR/LF
 *          are put in one of those subvalues.
 *
 *      --  The "Keyphrases" field corresponds to .KEYPHRASES.
 *          This is also EAT_MVMT and used like .COMMENTS.
 *
 *@@changed V0.9.1 (2000-01-22) [umoeller]: renamed from fsysFileInitPage
 *@@changed V0.9.18 (2002-03-19) [umoeller]: now refreshing page when turned back to
 *@@changed V0.9.19 (2002-04-13) [umoeller]: now using dialog formatter, made page resizeable
 */

VOID fsysFile1InitPage(PNOTEBOOKPAGE pnbp,    // notebook info struct
                       ULONG flFlags)                // CBI_* flags (notebook.h)
{
    if (flFlags & CBI_INIT)
    {
        // first call: backup instance data for "Undo" button;
        // this memory will be freed automatically by the
        // common notebook window function (notebook.c) when
        // the notebook page is destroyed
        CHAR            szFilename[CCHMAXPATH];
        PFILEPAGEDATA   pfpd = (PFILEPAGEDATA)malloc(sizeof(FILEPAGEDATA));
        memset(pfpd, 0, sizeof(FILEPAGEDATA));
        pnbp->pUser = pfpd;
        pfpd->ulAttr = _wpQueryAttr(pnbp->inbp.somSelf);
        _wpQueryFilename(pnbp->inbp.somSelf, szFilename, TRUE);
        pfpd->pszSubject = fsysQueryEASubject(pnbp->inbp.somSelf);
        pfpd->pszComments = fsysQueryEAComments(pnbp->inbp.somSelf);
        pfpd->pszKeyphrases = fsysQueryEAKeyphrases(pnbp->inbp.somSelf);

        // insert the controls using the dialog formatter
        // V0.9.19 (2002-04-14) [umoeller]
        ntbFormatPage(pnbp->hwndDlgPage,
                      dlgFile1,
                      ARRAYITEMCOUNT(dlgFile1));

        if (doshIsFileOnFAT(szFilename))
        {
            // on FAT: hide fields
            winhShowDlgItem(pnbp->hwndDlgPage, ID_XSDI_FILES_CREATIONDATE,
                            FALSE);
            winhShowDlgItem(pnbp->hwndDlgPage, ID_XSDI_FILES_LASTACCESSDATE,
                            FALSE);
        }

        if (!_somIsA(pnbp->inbp.somSelf, _WPFolder))
        {
            // this page is not for a folder, but
            // a data file:
            // hide "Work area" item
            winhShowDlgItem(pnbp->hwndDlgPage, ID_XSDI_FILES_WORKAREA, FALSE);
        }
        else if (cmnIsADesktop(pnbp->inbp.somSelf))
            // for the Desktop, disable work area;
            // this must not be changed
            WinEnableControl(pnbp->hwndDlgPage, ID_XSDI_FILES_WORKAREA,
                              FALSE);

        // Even though CPREF says that the .SUBJECT EA was limited to
        // 40 chars altogether, this is wrong apparently, as many users
        // have said after V0.9.1; so limit the entry field to 260 chars
        WinSendDlgItemMsg(pnbp->hwndDlgPage, ID_XSDI_FILES_SUBJECT,
                          EM_SETTEXTLIMIT,
                          (MPARAM)(260), MPNULL);
    }

    if (flFlags & (CBI_SET | CBI_SHOW))     // refresh page when turned back to
                                            // V0.9.18 (2002-03-19) [umoeller]
    {
        // prepare file date/time etc. for display in window
        CHAR    szFilename[CCHMAXPATH];
        CHAR    szTemp[100];
        PSZ     pszString = NULL;
        ULONG   ulAttr;
        FILESTATUS3 fs3;

        // on network drives, this can take a second or so
        // V0.9.19 (2002-04-24) [umoeller]
        HPOINTER hptrOld = winhSetWaitPointer();

        PCOUNTRYSETTINGS pcs = cmnQueryCountrySettings(FALSE);

        // get file-system object information
        // (we don't use the WPS method because the data
        // in there is frequently outdated)
        _wpQueryFilename(pnbp->inbp.somSelf, szFilename, TRUE);
        DosQueryPathInfo(szFilename,
                        FIL_STANDARD,
                        &fs3, sizeof(fs3));

        // real name
        WinSetDlgItemText(pnbp->hwndDlgPage, ID_XSDI_FILES_REALNAME,
                        szFilename);

        // file size
        nlsThousandsULong(szTemp, fs3.cbFile, pcs->cThousands);
        WinSetDlgItemText(pnbp->hwndDlgPage, ID_XSDI_FILES_FILESIZE, szTemp);

        // for folders: set work-area flag
        if (_somIsA(pnbp->inbp.somSelf, _WPFolder))
            // this page is not for a folder, but
            // a data file:
            // hide "Work area" item
            winhSetDlgItemChecked(pnbp->hwndDlgPage, ID_XSDI_FILES_WORKAREA,
                                  ((_wpQueryFldrFlags(pnbp->inbp.somSelf) & FOI_WORKAREA) != 0));

        // creation date/time
        SetDlgDateTime(pnbp->hwndDlgPage,
                       ID_XSDI_FILES_CREATIONDATE,
                       ID_XSDI_FILES_CREATIONTIME,
                       &fs3.fdateCreation,
                       &fs3.ftimeCreation,
                       pcs);
        // last write date/time
        SetDlgDateTime(pnbp->hwndDlgPage,
                       ID_XSDI_FILES_LASTWRITEDATE,
                       ID_XSDI_FILES_LASTWRITETIME,
                       &fs3.fdateLastWrite,
                       &fs3.ftimeLastWrite,
                       pcs);
        // last access date/time
        SetDlgDateTime(pnbp->hwndDlgPage,
                       ID_XSDI_FILES_LASTACCESSDATE,
                       ID_XSDI_FILES_LASTACCESSTIME,
                       &fs3.fdateLastAccess,
                       &fs3.ftimeLastAccess,
                       pcs);

        // attributes
        ulAttr = _wpQueryAttr(pnbp->inbp.somSelf);
        winhSetDlgItemChecked(pnbp->hwndDlgPage,
                              ID_XSDI_FILES_ATTR_ARCHIVED,
                              ((ulAttr & FILE_ARCHIVED) != 0));
        winhSetDlgItemChecked(pnbp->hwndDlgPage,
                              ID_XSDI_FILES_ATTR_READONLY,
                              ((ulAttr & FILE_READONLY) != 0));
        winhSetDlgItemChecked(pnbp->hwndDlgPage,
                              ID_XSDI_FILES_ATTR_HIDDEN,
                              ((ulAttr & FILE_HIDDEN) != 0));
        winhSetDlgItemChecked(pnbp->hwndDlgPage,
                              ID_XSDI_FILES_ATTR_SYSTEM,
                              ((ulAttr & FILE_SYSTEM) != 0));

        // .SUBJECT EA; this is plain text
        pszString = fsysQueryEASubject(pnbp->inbp.somSelf);
        WinSetDlgItemText(pnbp->hwndDlgPage, ID_XSDI_FILES_SUBJECT,
                          pszString);
        if (pszString)
            free(pszString);

        // .COMMENTS EA; this is multi-value multi-type, but all
        // of the sub-types are EAT_ASCII. We need to convert
        // the sub-items into one string and separate the items
        // with CR/LF.
        pszString = fsysQueryEAComments(pnbp->inbp.somSelf);
        WinSetDlgItemText(pnbp->hwndDlgPage, ID_XSDI_FILES_COMMENTS,
                          pszString);
        if (pszString)
            free(pszString);

        // .KEYPHRASES EA; this is multi-value multi-type, but all
        // of the sub-types are EAT_ASCII. We need to convert
        // the sub-items into one string and separate the items
        // with CR/LF.
        pszString = fsysQueryEAKeyphrases(pnbp->inbp.somSelf);
        WinSetDlgItemText(pnbp->hwndDlgPage, ID_XSDI_FILES_KEYPHRASES,
                        pszString);
        if (pszString)
            free(pszString);

        WinSetPointer(HWND_DESKTOP, hptrOld);
    }

    if (flFlags & CBI_DESTROY)
    {
        // notebook page is being destroyed:
        // free the backup EAs we created before
        PFILEPAGEDATA   pfpd = pnbp->pUser;

        if (pfpd->pszSubject)
            free(pfpd->pszSubject);
        if (pfpd->pszComments)
            free(pfpd->pszComments);
        if (pfpd->pszKeyphrases)
            free(pfpd->pszKeyphrases);

        // the pnbp->pUser field itself is free()'d automatically
    }
}

/*
 *@@ fsysFile1ItemChanged:
 *      first "File" page notebook callback function (notebook.c).
 *      Reacts to changes of any of the dialog controls.
 *
 *      This is used by both XFolder and XFldDataFile.
 *
 *@@changed V0.9.1 (2000-01-22) [umoeller]: renamed from fsysFile1InitPage
 *@@changed V0.9.19 (2002-04-13) [umoeller]: now using dialog formatter, made page resizeable
 */

MRESULT fsysFile1ItemChanged(PNOTEBOOKPAGE pnbp,    // notebook info struct
                             ULONG ulItemID,
                             USHORT usNotifyCode,
                             ULONG ulExtra)      // for checkboxes: contains new state
{
    BOOL fUpdate = TRUE;

    switch (ulItemID)
    {
        case ID_XSDI_FILES_WORKAREA:
            if (_somIsA(pnbp->inbp.somSelf, _WPFolder))
            {
                ULONG ulFlags = _wpQueryFldrFlags(pnbp->inbp.somSelf);
                if (ulExtra)
                    // checked:
                    ulFlags |= FOI_WORKAREA;
                else
                    ulFlags &= ~FOI_WORKAREA;
                _wpSetFldrFlags(pnbp->inbp.somSelf, ulFlags);
            }
        break;

        /*
         * ID_XSDI_FILES_ATTR_ARCHIVED:
         * ID_XSDI_FILES_ATTR_READONLY:
         * ID_XSDI_FILES_ATTR_HIDDEN:
         * ID_XSDI_FILES_ATTR_SYSTEM:
         *      any "Attributes" flag clicked:
         *      change file attributes
         */

        case ID_XSDI_FILES_ATTR_ARCHIVED:
        case ID_XSDI_FILES_ATTR_READONLY:
        case ID_XSDI_FILES_ATTR_HIDDEN:
        case ID_XSDI_FILES_ATTR_SYSTEM:
        {
            ULONG       ulFileAttr;

            ulFileAttr = _wpQueryAttr(pnbp->inbp.somSelf);

            // toggle file attribute
            ulFileAttr ^= // XOR flag depending on item checked
                      (ulItemID == ID_XSDI_FILES_ATTR_ARCHIVED) ? FILE_ARCHIVED
                    : (ulItemID == ID_XSDI_FILES_ATTR_SYSTEM  ) ? FILE_SYSTEM
                    : (ulItemID == ID_XSDI_FILES_ATTR_HIDDEN  ) ? FILE_HIDDEN
                    : FILE_READONLY;

            _wpSetAttr(pnbp->inbp.somSelf, ulFileAttr);
        }
        break;

        /*
         * ID_XSDI_FILES_SUBJECT:
         *      when focus leaves .SUBJECT entry field,
         *      rewrite plain EAT_ASCII EA
         */

        case ID_XSDI_FILES_SUBJECT:
            if (usNotifyCode == EN_KILLFOCUS)
            {
                PSZ         pszSubject = NULL;
                pszSubject = winhQueryWindowText(WinWindowFromID(pnbp->hwndDlgPage,
                                                                 ID_XSDI_FILES_SUBJECT));
                fsysSetEASubject(pnbp->inbp.somSelf, pszSubject);
                if (pszSubject)
                    free(pszSubject);
            }
        break;

        /*
         * ID_XSDI_FILES_COMMENTS:
         *      when focus leaves .COMMENTS MLE,
         *      rewrite EAT_MVMT EA
         */

        case ID_XSDI_FILES_COMMENTS:
            if (usNotifyCode == MLN_KILLFOCUS)
            {
                HWND    hwndMLE = WinWindowFromID(pnbp->hwndDlgPage, ulItemID);
                PSZ     pszText = winhQueryWindowText(hwndMLE);
                fsysSetEAComments(pnbp->inbp.somSelf, pszText);
                if (pszText)
                    free(pszText);
            }
        break;

        /*
         * ID_XSDI_FILES_KEYPHRASES:
         *      when focus leaves .KEYPHRASES MLE,
         *      rewrite EAT_MVMT EA
         */

        case ID_XSDI_FILES_KEYPHRASES:
            if (usNotifyCode == MLN_KILLFOCUS)
            {
                HWND    hwndMLE = WinWindowFromID(pnbp->hwndDlgPage, ulItemID);
                PSZ     pszText = winhQueryWindowText(hwndMLE);
                fsysSetEAKeyphrases(pnbp->inbp.somSelf, pszText);
                if (pszText)
                    free(pszText);
            }
        break;

        case DID_UNDO:
            if (pnbp->pUser)
            {
                // restore the file's data from the backup data
                PFILEPAGEDATA   pfpd = (PFILEPAGEDATA)pnbp->pUser;

                // reset attributes
                _wpSetAttr(pnbp->inbp.somSelf, pfpd->ulAttr);

                // reset EAs
                fsysSetEASubject(pnbp->inbp.somSelf, pfpd->pszSubject); // can be NULL
                fsysSetEAComments(pnbp->inbp.somSelf, pfpd->pszComments); // can be NULL
                fsysSetEAKeyphrases(pnbp->inbp.somSelf, pfpd->pszKeyphrases); // can be NULL

                // have the page updated by calling the callback above
                fsysFile1InitPage(pnbp, CBI_SET | CBI_ENABLE);
            }
        break;

        case DID_DEFAULT:
        {
            // "Default" button:
            ULONG           ulAttr = 0;
            // EABINDING       eab;
            if (_somIsA(pnbp->inbp.somSelf, _WPFolder))
                ulAttr = FILE_DIRECTORY;
            // reset attributes
            _wpSetAttr(pnbp->inbp.somSelf, ulAttr);

            // delete EAs
            fsysSetEASubject(pnbp->inbp.somSelf, NULL);
            fsysSetEAComments(pnbp->inbp.somSelf, NULL);
            fsysSetEAKeyphrases(pnbp->inbp.somSelf, NULL);

            // have the page updated by calling the callback above
            fsysFile1InitPage(pnbp, CBI_SET | CBI_ENABLE);
        }
        break;

        default:
            fUpdate = FALSE;
        break;
    }

    if (fUpdate)
        _wpSaveDeferred(pnbp->inbp.somSelf);

    return (MPARAM)-1;
}

/* ******************************************************************
 *
 *   "File" page 2 replacement in WPDataFile/WPFolder
 *
 ********************************************************************/

#ifndef __NOFILEPAGE2__

/*
 *@@ fsysFile2InitPage:
 *      second "File" page notebook callback function (notebook.c).
 *      Sets the controls on the page according to a file's
 *      filesystem characteristics.
 *
 *      This is used by both XFolder and XFldDataFile.
 *
 *@@added V0.9.1 (2000-01-22) [umoeller]
 *@@changed V0.9.18 (2002-03-19) [umoeller]: now refreshing page when turned back to
 */

VOID fsysFile2InitPage(PNOTEBOOKPAGE pnbp,    // notebook info struct
                       ULONG flFlags)                // CBI_* flags (notebook.h)
{
    if (flFlags & CBI_INIT)
    {
        HWND hwndContents = WinWindowFromID(pnbp->hwndDlgPage, ID_XSDI_FILES_EACONTENTS);
        winhSetWindowFont(hwndContents, "8.Courier");
        WinSendMsg(hwndContents, MLM_SETREADONLY, (MPARAM)TRUE, 0);
    }

    if (flFlags & (CBI_SET | CBI_SHOW))     // refresh page when turned back to
                                            // V0.9.18 (2002-03-19) [umoeller]
    {
        CHAR szFilename[CCHMAXPATH];
        if (_wpQueryFilename(pnbp->inbp.somSelf, szFilename, TRUE))
        {
            PEALIST peal = eaPathReadAll(szFilename),
                    pealThis = peal;
            HWND hwndEAList = WinWindowFromID(pnbp->hwndDlgPage, ID_XSDI_FILES_EALIST);
            winhDeleteAllItems(hwndEAList);         // V0.9.18 (2002-03-19) [umoeller]
            while (pealThis)
            {
                PEABINDING peabThis = pealThis->peab;
                if (peabThis)
                {
                    WinInsertLboxItem(hwndEAList,
                                      LIT_END,
                                      peabThis->pszName);
                }

                pealThis = pealThis->next;
            }

            eaFreeList(peal);

            // clear info fields, this might be a refresh
            // V0.9.18 (2002-03-19) [umoeller]
            WinSetDlgItemText(pnbp->hwndDlgPage,
                              ID_XSDI_FILES_EAINFO,
                              NULL);
            WinSetDlgItemText(pnbp->hwndDlgPage,
                              ID_XSDI_FILES_EACONTENTS,
                              NULL);
        }
    }
}

/*
 *@@ fsysFile2ItemChanged:
 *      second "File" page notebook callback function (notebook.c).
 *      Reacts to changes of any of the dialog controls.
 *
 *      This is used by both XFolder and XFldDataFile.
 *
 *@@added V0.9.1 (2000-01-22) [umoeller]
 */

MRESULT fsysFile2ItemChanged(PNOTEBOOKPAGE pnbp,    // notebook info struct
                             ULONG ulItemID,
                             USHORT usNotifyCode,
                             ULONG ulExtra)      // for checkboxes: contains new state
{
    switch (ulItemID)
    {
        /*
         * ID_XSDI_FILES_EALIST:
         *      EAs list box.
         */

        case ID_XSDI_FILES_EALIST:
            if (usNotifyCode == LN_SELECT)
            {
                CHAR szFilename[CCHMAXPATH];
                HWND hwndEAList = WinWindowFromID(pnbp->hwndDlgPage, ID_XSDI_FILES_EALIST);
                ULONG ulSelection = (ULONG)WinSendMsg(hwndEAList,
                                                      LM_QUERYSELECTION,
                                                      MPNULL,
                                                      MPNULL);
                if (    (ulSelection != LIT_NONE)
                     && (_wpQueryFilename(pnbp->inbp.somSelf, szFilename, TRUE))
                   )
                {
                    PSZ pszEAName;
                    if (pszEAName = winhQueryLboxItemText(hwndEAList,
                                                          ulSelection))
                    {
                        PEABINDING peab;
                        if (peab = eaPathReadOneByName(szFilename,
                                                       pszEAName))
                        {
                            PSZ     pszContents = NULL;
                            XSTRING strInfo;
                            USHORT  usEAType = eaQueryEAType(peab);
                            CHAR    szTemp[100];
                            BOOL    fDumpBinary = TRUE;

                            xstrInit(&strInfo, 200);
                            xstrcpy(&strInfo, pszEAName, 0);

                            switch (usEAType)
                            {
                                case EAT_BINARY:
                                    xstrcat(&strInfo, " (EAT_BINARY", 0);
                                break;

                                case EAT_ASCII:
                                    xstrcat(&strInfo, " (EAT_ASCII", 0);
                                    pszContents = eaCreatePSZFromBinding(peab);
                                    fDumpBinary = FALSE;
                                break;

                                case EAT_BITMAP:
                                    xstrcat(&strInfo, " (EAT_BITMAP", 0);
                                break;

                                case EAT_METAFILE:
                                    xstrcat(&strInfo, " (EAT_METAFILE", 0);
                                break;

                                case EAT_ICON:
                                    xstrcat(&strInfo, " (EAT_ICON", 0);
                                break;

                                case EAT_EA:
                                    xstrcat(&strInfo, " (EAT_EA", 0);
                                break;

                                case EAT_MVMT:
                                    xstrcat(&strInfo, " (EAT_MVMT", 0);
                                break;

                                case EAT_MVST:
                                    xstrcat(&strInfo, " (EAT_MVST", 0);
                                break;

                                case EAT_ASN1:
                                    xstrcat(&strInfo, " (EAT_ASN1", 0);
                                break;

                                default:
                                {
                                    sprintf(szTemp, " (type 0x%lX", usEAType);
                                    xstrcat(&strInfo, szTemp, 0);
                                }
                            }

                            sprintf(szTemp, ", %d bytes)", peab->usValueLength);
                            xstrcat(&strInfo, szTemp, 0);

                            if (fDumpBinary)
                            {
                                pszContents = strhCreateDump(peab->pszValue,
                                                             peab->usValueLength,
                                                             0);
                            }

                            // set static above MLE
                            WinSetDlgItemText(pnbp->hwndDlgPage,
                                              ID_XSDI_FILES_EAINFO,
                                              strInfo.psz);

                            // set MLE; this might be empty
                            WinSetDlgItemText(pnbp->hwndDlgPage,
                                              ID_XSDI_FILES_EACONTENTS,
                                              pszContents);

                            eaFreeBinding(peab);
                            xstrClear(&strInfo);
                            if (pszContents)
                                free(pszContents);
                        }
                        free(pszEAName);
                    }
                }
            }
        break;
    }

    return 0;
}

#endif

/*
 *@@ fsysInsertFilePages:
 *      shared code between XFldDataFile and XFolder
 *      to insert the new "File" pages if this is
 *      enabled. This gets called from the respective
 *      wpAddFile1Page methods.
 *
 *@@added V0.9.5 (2000-08-14) [umoeller]
 */

ULONG fsysInsertFilePages(WPObject *somSelf,    // in: must be a WPFileSystem, really
                          HWND hwndNotebook)    // in: from wpAddFile1Page
{
    INSERTNOTEBOOKPAGE inbp;

#ifndef __NOFILEPAGE2__
    // page 2
    memset(&inbp, 0, sizeof(INSERTNOTEBOOKPAGE));
    inbp.somSelf = somSelf;
    inbp.hwndNotebook = hwndNotebook;
    inbp.hmod = cmnQueryNLSModuleHandle(FALSE);
    inbp.ulDlgID = ID_XSD_FILESPAGE2;
    inbp.ulPageID = SP_FILE2;
    inbp.pcszName = cmnGetString(ID_XSSI_FILEPAGE);  // pszFilePage
    inbp.fEnumerate = TRUE;
    inbp.ulDefaultHelpPanel  = ID_XSH_SETTINGS_FILEPAGE2;
    inbp.pfncbInitPage    = (PFNCBACTION)fsysFile2InitPage;
    inbp.pfncbItemChanged = (PFNCBITEMCHANGED)fsysFile2ItemChanged;

    ntbInsertPage(&inbp);
#endif

    // page 1
    memset(&inbp, 0, sizeof(INSERTNOTEBOOKPAGE));
    inbp.somSelf = somSelf;
    inbp.hwndNotebook = hwndNotebook;
    inbp.hmod = cmnQueryNLSModuleHandle(FALSE);
    inbp.ulDlgID = ID_XFD_EMPTYDLG; // ID_XSD_FILESPAGE1; V0.9.19 (2002-04-13) [umoeller]
    inbp.ulPageID = SP_FILE1;
    inbp.usPageStyleFlags = BKA_MAJOR;
    inbp.pcszName = cmnGetString(ID_XSSI_FILEPAGE);  // pszFilePage
    inbp.fEnumerate = TRUE;
    inbp.ulDefaultHelpPanel  = ID_XSH_SETTINGS_FILEPAGE1;
    inbp.pfncbInitPage    = (PFNCBACTION)fsysFile1InitPage;
    inbp.pfncbItemChanged = (PFNCBITEMCHANGED)fsysFile1ItemChanged;
    // make this page sizeable V0.9.19 (2002-04-13) [umoeller]
    inbp.pampControlFlags = G_ampFile1Page;
    inbp.cControlFlags = G_cFile1Page;

    return ntbInsertPage(&inbp);
}


