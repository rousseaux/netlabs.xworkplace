
/*
 *@@sourcefile program.c:
 *      various program-related implementation code
 *      (especially WPProgram and WPProgramFile).
 *
 *      Presently, this implements:
 *
 *      -- Starting an executable via WinStartApp
 *         from a program object, with full in-use
 *         management and session handling. See
 *         progOpenProgram.
 *
 *      This file is ALL new with V0.9.6.
 *
 *      Function prefix for this file:
 *      --  prog*
 *
 *@@added V0.9.6 [umoeller]
 *@@header "filesys\program.h"
 */

/*
 *      Copyright (C) 2000 Ulrich M”ller.
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

#define INCL_DOSPROCESS
#define INCL_DOSEXCEPTIONS
#define INCL_DOSSEMAPHORES
#define INCL_DOSERRORS
#define INCL_WINMESSAGEMGR
#define INCL_WINDIALOGS
#define INCL_WINPROGRAMLIST     // needed for wppgm.h
#define INCL_WINENTRYFIELDS
#define INCL_WINERRORS
#include <os2.h>

// C library headers
#include <stdio.h>
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\stringh.h"            // string helper routines
#include "helpers\winh.h"               // PM helper routines
#include "helpers\xstring.h"            // extended string helpers

// SOM headers which don't crash with prec. header files
#include "xfobj.ih"
#include "xfdataf.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel

#include "filesys\object.h"             // XFldObject implementation
#include "filesys\program.h"            // program implementation

// other SOM headers
#pragma hdrstop
#include <wppgm.h>                      // WPProgram
#include <wppgmf.h>                     // WPProgramFile

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

HMTX        G_hmtxRunning = NULLHANDLE;
LINKLIST    G_llRunning;
        // linked list of running programs; contains RUNNINGPROGRAM structs

/* ******************************************************************
 *
 *   Helpers
 *
 ********************************************************************/

/* ******************************************************************
 *
 *   Running programs database
 *
 ********************************************************************/

/*
 *@@ RUNNINGPROGRAM:
 *
 *@@added V0.9.6 (2000-10-16) [umoeller]
 */

typedef struct _RUNNINGPROGRAM
{
    WPObject    *pObjEmphasis;       // object with use item
    PUSEITEM    pUseItemView;        // use item followed by VIEWITEM
    PUSEITEM    pUseItemFile;        // use item followed by VIEWFILE; can be NULL
} RUNNINGPROGRAM, *PRUNNINGPROGRAM;

/*
 *@@ progStoreRunningApp:
 *      registers a useitem for the specified object to give
 *      it "in-use" emphasis and stores the program in
 *      our internal list so that progAppTerminateNotify
 *      can remove it again when WM_APPTERMINATENOTIFY comes
 *      in.
 *
 *      This operates in two modes:
 *
 *      -- If pDataFile is NULL, this adds a USAGE_OPENVIEW (VIEWITEM)
 *         to pProgram, which is assumed to be a WPProgram
 *         or WPProgramFile. In that case, ulMenuID is ignored.
 *
 *      -- If pDataFile is specified, this first adds a
 *         USAGE_OPENVIEW (VIEWITEM) to the data file (!). In
 *         addition, this adds a USAGE_OPENFILE use
 *         item (VIEWFILE) to the data file which is needed
 *         by the WPS to track multiple open associations.
 *         In that case, you must also specify ulMenuID.
 *
 *@@added V0.9.6 (2000-10-16) [umoeller]
 *@@changed V0.9.6 (2000-11-12) [umoeller]: VIEWFILE items were missing, fixed.
 */

BOOL progStoreRunningApp(WPObject *pProgram,        // in: started program
                         WPFileSystem *pDataFile,   // in: data file with assoc or NULL
                         HAPP happ,                 // in: app handle from winhStartApp
                         ULONG ulMenuID)            // in: if (pDataFile != NULL), selected menu ID
                                                    // (for VIEWFILE item)
{
    BOOL    brc = FALSE;
    BOOL    fSemOwned = FALSE;

    ULONG   ulNesting = 0;
    DosEnterMustComplete(&ulNesting);

    TRY_LOUD(excpt1)
    {
        if (!G_hmtxRunning)
        {
            // first call:
            APIRET arc = DosCreateMutexSem(NULL,     // unnamed
                                           &G_hmtxRunning,
                                           0,        // unshared
                                           TRUE);    // initially owned!
            if (arc == NO_ERROR)
            {
                fSemOwned = TRUE;
                lstInit(&G_llRunning,
                        TRUE);      // auto-free
            }
        }

        if (!fSemOwned)
            fSemOwned = (WinRequestMutexSem(G_hmtxRunning, 4000) == NO_ERROR);

        if (fSemOwned)
        {
            if (    (happ)
                 && ((pProgram != NULL) || (pDataFile != NULL))
               )
            {
                PRUNNINGPROGRAM pRunning = (PRUNNINGPROGRAM)malloc(sizeof(RUNNINGPROGRAM));
                if (pRunning)
                {
                    // allocate view item
                    PUSEITEM pUseItemView = 0,
                             pUseItemFile = 0;
                    WPObject *pObjEmph = 0;

                    if (pDataFile == NULL)
                        // object to work on is program object
                        pObjEmph = pProgram;
                    else
                        // object to work on is datafile object
                        pObjEmph = pDataFile;

                    if (pObjEmph)
                    {
                        // in any case, add "in-use" emphasis to the object
                        pUseItemView = (PUSEITEM)_wpAllocMem(pObjEmph,
                                                             sizeof(USEITEM) + sizeof(VIEWITEM),
                                                             NULL);
                        if (pUseItemView)
                        {
                            // VIEWITEM is right behind use item
                            PVIEWITEM pViewItem = (PVIEWITEM)(pUseItemView + 1);
                            // set up data
                            memset(pUseItemView, 0, sizeof(USEITEM) + sizeof(VIEWITEM));
                            pUseItemView->type = USAGE_OPENVIEW;
                            pViewItem->view = OPEN_RUNNING;
                            pViewItem->handle = happ;

                            brc = _wpAddToObjUseList(pObjEmph,
                                                     pUseItemView);

                            if (brc)
                            {
                                // success:
                                // for data file associations, add VIEWFILE
                                // structure as well
                                pUseItemFile =  (PUSEITEM)_wpAllocMem(pObjEmph,
                                                                      sizeof(USEITEM) + sizeof(VIEWFILE),
                                                                      NULL);
                                if (pUseItemFile)
                                {
                                    // VIEWFILE item is right behind use item
                                    PVIEWFILE pViewFile = (PVIEWFILE)(pUseItemFile + 1);
                                    // set up data
                                    memset(pUseItemFile, 0, sizeof(USEITEM) + sizeof(VIEWFILE));
                                    pUseItemFile->type = USAGE_OPENFILE;
                                    pViewFile->ulMenuId = ulMenuID;
                                    pViewFile->handle  = happ;

                                    brc = _wpAddToObjUseList(pObjEmph,
                                                             pUseItemFile);
                                }
                                else
                                    brc = FALSE;
                            }
                        }

                        if (brc)
                        {
                            // store this in our internal list
                            // so we can find the object
                            // in progAppTerminateNotify
                            pRunning->pObjEmphasis = pObjEmph;
                            pRunning->pUseItemView = pUseItemView;
                            pRunning->pUseItemFile = pUseItemFile; // can be 0
                            lstAppendItem(&G_llRunning,
                                          pRunning);

                            // set list-notify flag on the object
                            // so that XFldObject will call
                            // progAppTerminateNotifyObj when
                            // the object is destroyed
                            _xwpModifyListNotify(pObjEmph,
                                                 OBJLIST_RUNNINGSTORED,
                                                 OBJLIST_RUNNINGSTORED);
                        }
                    } // end if (pObjEmph)

                    if (!brc)
                        free(pRunning);

                } // end if (pRunning)
            }
        }
    }
    CATCH(excpt1)
    {
        brc = FALSE;
    } END_CATCH();

    if (fSemOwned)
    {
        DosReleaseMutexSem(G_hmtxRunning);
        fSemOwned = FALSE;
    }

    DosExitMustComplete(&ulNesting);

    return (brc);
}

/*
 *@@ progAppTerminateNotify:
 *      the reverse to progStoreRunningApp. This searches
 *      the internal list of programs created by
 *      progStoreRunningApp and removes
 *
 *      --  the use item from the object;
 *
 *      --  the item from the list.
 *
 *      This normally gets called from krn_fnwpThread1Object
 *      when WM_APPTERMINATENOTIFY comes in.
 *
 *@@added V0.9.6 (2000-10-16) [umoeller]
 */

BOOL progAppTerminateNotify(HAPP happ)        // in: application handle
{
    BOOL    brc = FALSE;
    BOOL    fSemOwned = FALSE;

    ULONG   ulNesting = 0;
    DosEnterMustComplete(&ulNesting);

    TRY_LOUD(excpt1)
    {
        fSemOwned = (WinRequestMutexSem(G_hmtxRunning, 4000) == NO_ERROR);

        if (fSemOwned)
        {
            // go thru list
            PLISTNODE pNode = lstQueryFirstNode(&G_llRunning);
            while (pNode)
            {
                PRUNNINGPROGRAM pRunning = (PRUNNINGPROGRAM)pNode->pItemData;
                if (pRunning->pUseItemView)
                {
                    // VIEWITEM is right behind use item;
                    // this exists for both program and data file objects
                    PVIEWITEM pViewItem = (PVIEWITEM)(pRunning->pUseItemView + 1);

                    if (pViewItem->handle == happ)
                    {
                        // yes, this is ours:

                        // check if we also have a VIEWFILE item
                        if (pRunning->pUseItemFile)
                        {
                            // yes:
                            _wpDeleteFromObjUseList(pRunning->pObjEmphasis,
                                                    pRunning->pUseItemFile);
                            _wpFreeMem(pRunning->pObjEmphasis,
                                       (PBYTE)pRunning->pUseItemFile);
                        }

                        // now remove "view" useitem from the object's use list
                        // (this always exists)
                        _wpDeleteFromObjUseList(pRunning->pObjEmphasis,
                                                pRunning->pUseItemView);
                        _wpFreeMem(pRunning->pObjEmphasis,
                                   (PBYTE)pRunning->pUseItemView);

                        // remove this thing (auto-free!)
                        lstRemoveNode(&G_llRunning, pNode);

                        brc = TRUE;

                        // stop searching
                        break;
                    }
                }

                pNode = pNode->pNext;
            }
        }
    }
    CATCH(excpt1)
    {
        brc = FALSE;
    } END_CATCH();

    if (fSemOwned)
    {
        DosReleaseMutexSem(G_hmtxRunning);
        fSemOwned = FALSE;
    }

    DosExitMustComplete(&ulNesting);

    return (brc);
}

/*
 *@@ progRunningAppDestroyed:
 *      this gets called from XFldObject::wpUnInitData
 *      when an object is being destroyed which is stored
 *      on the list maintained by progStoreRunningApp.
 *
 *      We must then remove that item from the list because
 *      otherwise we get a trap when progAppTerminateNotify
 *      attempts to remove in-use emphasis on the object.
 *
 *      Note that this does not remove the use items, nor
 *      does it free the memory allocated for the use items.
 *      Since this is done by the WPS wpUnInitData method,
 *      this is not necessary anyway.
 *
 *@@added V0.9.6 (2000-10-23) [umoeller]
 */

BOOL progRunningAppDestroyed(WPObject *pObjEmphasis)    // in: destroyed object
{
    BOOL    brc = FALSE;
    BOOL    fSemOwned = FALSE;

    ULONG   ulNesting = 0;
    DosEnterMustComplete(&ulNesting);

    TRY_LOUD(excpt1)
    {
        fSemOwned = (WinRequestMutexSem(G_hmtxRunning, 4000) == NO_ERROR);

        if (fSemOwned)
        {
            // go thru list
            PLISTNODE pNode = lstQueryFirstNode(&G_llRunning);
            while (pNode)
            {
                PRUNNINGPROGRAM pRunning = (PRUNNINGPROGRAM)pNode->pItemData;
                if (pRunning->pObjEmphasis == pObjEmphasis)
                {
                    // found:
                    // remove this thing (auto-free!)
                    lstRemoveNode(&G_llRunning, pNode);
                            // since the memory for the useitems has been
                            // allocated using wpAllocMem, this will be
                            // freed automatically...

                    brc = TRUE;

                    // stop searching
                            // break;
                    // no, continue, because we can have more than one item
                }

                pNode = pNode->pNext;
            }
        }
    }
    CATCH(excpt1)
    {
        brc = FALSE;
    } END_CATCH();

    if (fSemOwned)
    {
        DosReleaseMutexSem(G_hmtxRunning);
        fSemOwned = FALSE;
    }

    DosExitMustComplete(&ulNesting);

    return (brc);
}

/* ******************************************************************
 *
 *   Run programs
 *
 ********************************************************************/

/*
 *@@ progDisplayParamsPrompt:
 *      this gets called from progSetupArgs for each of
 *      those "[prompt]" strings that is found in the
 *      arguments of a program object.
 *
 *      On input, pstrPrompt is set to the string in
 *      between the "[]" characters. For "[prompt]",
 *      pstrPrompt would thus contain "prompt".
 *
 *      This then displays a dialog.
 *
 *      If this function returns TRUE, the caller assumes
 *      that pstrPrompt has been replaced with whatever
 *      the user entered. This function may clear pstrPrompt
 *      if the user entered nothing; in that case, the
 *      "[prompt]" is simply removed from the parameters.
 *
 *      If this function returns FALSE, the caller will
 *      assume the user has pressed "Cancel" and abort
 *      processing.
 *
 *@@added V0.9.6 (2000-10-16) [umoeller]
 */

BOOL DisplayParamsPrompt(PXSTRING pstrPrompt)   // in: prompt string,
                                                // out: what user entered
{
    BOOL brc = FALSE;

    HWND hwndDlg = WinLoadDlg(HWND_DESKTOP,     // parent
                              NULLHANDLE,       // owner
                              WinDefDlgProc,
                              cmnQueryNLSModuleHandle(FALSE),
                              ID_XSD_NEWFILETYPE,   // "New File Type" dlg
                              NULL);            // pCreateParams
    if (hwndDlg)
    {
        PNLSSTRINGS pNLSStrings = cmnQueryNLSStrings();
        HWND hwndEntryField = WinWindowFromID(hwndDlg, ID_XSDI_FT_ENTRYFIELD);
        WinSetWindowText(hwndDlg, pNLSStrings->pszParameters);
        WinSetDlgItemText(hwndDlg,
                          ID_XSDI_FT_TITLE,
                          pstrPrompt->psz);
        winhSetEntryFieldLimit(hwndEntryField, 255);
        if (WinProcessDlg(hwndDlg) == DID_OK)
        {
            CHAR    szNew[300] = "";
            ULONG   ulOfs = 0;
            WinQueryWindowText(hwndEntryField,
                               sizeof(szNew),
                               szNew);
            // replace output buffer
            xstrcpy(pstrPrompt, szNew, 0);
            // return "OK pressed"
            brc = TRUE;
        }
        // else: user pressed "Cancel":

        WinDestroyWindow(hwndDlg);
    }

    return (brc);
}

/*
 *@@ FixSpacesInFilename:
 *      checks if psz contains spaces and, if so,
 *      encloses the string in psz in quotes.
 *      It is assumes that there is enough room
 *      in psz to hold two more characters.
 *
 *      Otherwise psz is not changed.
 *
 *@@added V0.9.7 (2000-12-10) [umoeller]
 */

VOID FixSpacesInFilename(PSZ psz)
{
    if (psz)
    {
        if (strchr(psz, ' '))
        {
            // we have spaces:
            // compose temporary
            XSTRING strTemp;
            xstrInit(&strTemp,
                     strlen(psz) + 3);  // preallocate: length of original
                                        // plus two quotes plus null terminator
            xstrcpy(&strTemp, "\"", 0);
            xstrcat(&strTemp, psz, 0);
            xstrcatc(&strTemp, '"');

            // copy back
            strcpy(psz, strTemp.psz);
            xstrClear(&strTemp);
        }
    }
}

/*
 *@@ HandlePlaceholder:
 *      checks *p if it is a WPS placeholder for file name
 *      components. Returns TRUE if *p is a valid placeholder
 *      or FALSE if it is not.
 *
 *      Gets called with *p pointing to the "%" char.
 *
 *      If this returns TRUE, it must:
 *
 *      -- set pxstr to the replacement string (e.g. a filename).
 *         pxstr can be set to an empty string if the placeholder
 *         is to be deleted.
 *
 *      -- set pcReplace to the no. of characters that should be
 *         skipped in the source string. This must be the length
 *         of the placeholder, excluding any null terminator
 *         (e.g. 4 for "%**F").
 *
 *      -- set *pfAppendDataFilename to FALSE if the caller should
 *         _not_ append the data file name at the end of the params
 *         string. Otherwise it should leave this flag alone.
 *
 *         From my testing, the WPS always appends the full path
 *         specification (%*) of the data file to the end of the
 *         program's arguments list, UNLESS "%*" or one of the
 *         "%**X" parameters are specified at least once.
 *
 *         As a result, this function sets *pfAppendDataFilename
 *         to FALSE for all "%*" and "%**X" placeholders, but
 *         not for parameter strings.
 *
 *      If this returns FALSE, *p is simply left alone (copied).
 *      All output parameters are then ignored.
 *
 *      Preconditions: pstrTemp has been initialized (xstrInit).
 *
 *      Postconditions: pstrTemp is freed by the caller.
 *
 *@@added V0.9.7 (2000-12-10) [umoeller]
 */

BOOL HandlePlaceholder(const char *p,           // in: placeholder (starting with "%")
                       const char *pcszFilename, // in: data file name;
                                                 // ptr is always valid, but can point to ""
                       PXSTRING pstrTemp,       // out: replacement string (e.g. filename)
                       PULONG pcReplace,        // out: no. of chars to replace (w/o null terminator)
                       PBOOL pfAppendDataFilename)  // out: if FALSE, do not append full filename
{
    BOOL brc = FALSE;

    // p points to "%" char; check next char
    switch (*(p+1))
    {
        case ' ':
        case '\0':
            // "%": disable passing full path
            *pfAppendDataFilename = FALSE;
            *pcReplace = 1;     // % only
            brc = TRUE;
        break;

        case '*':
        {
            CHAR szTemp[2*CCHMAXPATH] = "";

            if ( (*(p+2)) == '*' )      // "%**" ?
            {
                // we have a placeholder:
                // check next char (P, D, N, F, E)
                switch (*(p+3))
                {
                    case 'P':
                    {
                        // "%**P": drive and path information without the
                        // last backslash (\)
                        PSZ pLastBackslash = strrchr(pcszFilename, '\\');
                        if (pLastBackslash)
                            strhncpy0(szTemp,
                                      pcszFilename,
                                      pLastBackslash - pcszFilename);
                        FixSpacesInFilename(szTemp); // V0.9.7 (2000-12-10) [umoeller]

                        *pfAppendDataFilename = FALSE;
                        *pcReplace = 4;     // "%**?"
                        brc = TRUE;
                    break; }

                    case 'D':
                    {
                        //  "%**D": drive with ':' or UNC name
                        PSZ pFirstBackslash = strchr(pcszFilename + 2, '\\');
                            // works on "C:\blah" or "\\unc\blah"
                        if (pFirstBackslash)
                            strhncpy0(szTemp,
                                      pcszFilename,
                                      pFirstBackslash - pcszFilename);
                        FixSpacesInFilename(szTemp); // V0.9.7 (2000-12-10) [umoeller]

                        *pfAppendDataFilename = FALSE;
                        *pcReplace = 4;     // "%**?"
                        brc = TRUE;
                    break; }

                    case 'N':
                    {
                        //  "%**N": file name without extension.
                        // first find filename
                        PSZ pLastBackslash = strrchr(pcszFilename + 2, '\\');
                            // works on "C:\blah" or "\\unc\blah"
                        if (pLastBackslash)
                        {
                            // find last dot in filename
                            PSZ pLastDot = strrchr(pLastBackslash + 1, '.');
                            if (pLastDot)
                            {
                                // extension found:
                                // copy from pLastBackslash + 1 to pLastDot
                                strhncpy0(szTemp,
                                          pLastBackslash + 1,
                                          pLastDot - (pLastBackslash + 1));
                            }
                            else
                                // no extension:
                                // copy entire name
                                strcpy(szTemp, pLastBackslash + 1);
                        }
                        FixSpacesInFilename(szTemp); // V0.9.7 (2000-12-10) [umoeller]

                        *pfAppendDataFilename = FALSE;
                        *pcReplace = 4;     // "%**?"
                        brc = TRUE;
                    break; }

                    case 'F':
                    {
                        // "%**F": file name with extension.
                        // find filename
                        PSZ pLastBackslash = strrchr(pcszFilename + 2, '\\');
                            // works on "C:\blah" or "\\unc\blah"
                        if (pLastBackslash)
                            strcpy(szTemp, pLastBackslash + 1);
                        FixSpacesInFilename(szTemp); // V0.9.7 (2000-12-10) [umoeller]

                        *pfAppendDataFilename = FALSE;
                        *pcReplace = 4;     // "%**?"
                        brc = TRUE;
                    break; }

                    case 'E':
                    {
                        //  "%**E": extension without leading dot.
                        // In HPFS, the extension always comes after the last dot.
                        PSZ pExt = doshGetExtension(pcszFilename);
                        if (pExt)
                        {
                            strcpy(szTemp, pExt);
                            FixSpacesInFilename(szTemp);
                                    // improbable, but possible
                        }

                        *pfAppendDataFilename = FALSE;
                        *pcReplace = 4;     // "%**?"
                        brc = TRUE;
                    break; }
                }
            } // end of all those "%**?" cases
            else
            {
                // third character is not '*':
                // we then assume it's "%*" only...

                // "%*": full path of data file
                strcpy(szTemp, pcszFilename);
                FixSpacesInFilename(szTemp); // V0.9.7 (2000-12-10) [umoeller]

                *pfAppendDataFilename = FALSE;
                *pcReplace = 2;     // "%**?"
                brc = TRUE;
            }

            if (szTemp[0])
                // something was copied to replace:
                xstrcpy(pstrTemp, szTemp, 0);
            // else: pstrTemp has been zeroed by caller, leave it

        break; } // case '*': (second character)
    } // end switch (*(p+1))

    return (brc);
}

/*
 *@@ progSetupArgs:
 *      sets up a new "parameters" string from the specified
 *      program parameters string and data file.
 *
 *      This returns a new buffer which must be free()'d after
 *      use.
 *
 *      All the WPS argument placeholders are supported, which
 *      are:
 *
 *      --  "%": do not pass any arguments at all. In that
 *          case, pArgDataFile is always ignored.
 *
 *      --  [string]: create a popup dialog for variable
 *          data. This does not prevent the WPS from appending
 *          the data file name.
 *
 *      --  "%*": full path of data file.
 *
 *      --  "%**P": drive and path information without the
 *          last backslash (\).
 *
 *      --  "%**D": drive with ':' or UNC name.
 *
 *      --  "%**N": file name without extension.
 *
 *      --  "%**F": file name with extension.
 *
 *      --  "%**E": extension without leading dot.  In HPFS,
 *          the extension always comes after the last dot.
 *
 *      The WPS doesn't care for spaces. These placeholders
 *      are even replaced if not surrounded by spaces.
 *
 *      NOTE: Since this program can possibly wait for a prompt
 *      dialog to be filled by the user, this requires a message
 *      queue. Also, this will not return until the dialog has
 *      been displayed.
 *
 *      This returns TRUE if all parameters were successfully
 *      converted. This returns FALSE only on fatal internal
 *      errors or if a "prompt" dlg was displayed and the user
 *      pressed "Cancel".
 *
 *@@added V0.9.6 (2000-10-16) [umoeller]
 *@@changed V0.9.6 (2000-11-04) [umoeller]: fixed null string search
 *@@changed V0.9.7 (2000-12-10) [umoeller]: fixed spaces-in-filename problem (new implementation)
 *@@changed V0.9.7 (2000-12-10) [umoeller]: fixed params prompt hangs (new implementation)
 *@@changed V0.9.7 (2000-12-10) [umoeller]: extracted HandlePlaceholder
 */

BOOL progSetupArgs(const char *pcszParams,
                   WPFileSystem *pFile,         // in: file or NULL
                   PSZ *ppszParamsNew)          // out: new params
{
    BOOL    brc = TRUE;

    XSTRING strParamsNew;
    BOOL    fAppendDataFilename = TRUE;
    CHAR    szDataFilename[2*CCHMAXPATH] = "";

    if (pFile)
    {
        // we have a data file as argument:
        // get fully q'fied data file name
        _wpQueryFilename(pFile,
                         szDataFilename,
                         TRUE);
    }
    // else no file: szDataFilename is empty

    xstrInit(&strParamsNew, 500);

    if (pcszParams)
    {
        // we have a params string in the program object:
        // copy it character per character and replace
        // special keywords
        const char *p = pcszParams;

        while (    (*p)  // not end of string
                && (brc == TRUE)        // no error, not cancelled
              )
        {
            switch (*p)
            {
                /*
                 *   '%':  handle filename placeholder
                 *
                 */

                case '%':
                {
                    XSTRING strTemp;
                    ULONG cReplace = 0;
                    xstrInit(&strTemp, 0);
                    // might be a placeholder, so check
                    if (    (HandlePlaceholder(p,
                                               szDataFilename,
                                               &strTemp,
                                               &cReplace,
                                               &fAppendDataFilename))
                         && (cReplace)
                       )
                    {
                        // handled:
                        // append replacement
                        xstrcat(&strParamsNew, strTemp.psz, strTemp.ulLength);
                        // skip placeholder (cReplace has been set by HandlePlaceholder)
                        p += cReplace;
                        // disable appending the full filename
                        // at the end
                        // fAppendDataFilename = FALSE;
                    }
                    else
                    {
                        // not handled:
                        // just append
                        xstrcatc(&strParamsNew, *p);
                        p++;
                    }
                    xstrClear(&strTemp);
                break; }

                /*
                 *   '[': handle prompt placeholder
                 *
                 */

                case '[':
                {
                    const char *pEnd = strchr(p + 1, ']');
                    if (pEnd)
                    {
                        XSTRING strPrompt;
                        // copy stuff between brackets
                        PSZ pPrompt = strhSubstr(p + 1, pEnd);
                        xstrInitSet(&strPrompt, pPrompt);
                        if (DisplayParamsPrompt(&strPrompt))
                        {
                            // user pressed "OK":
                            xstrcat(&strParamsNew, strPrompt.psz, strPrompt.ulLength);
                            // next character to copy is
                            // character after closing bracket
                            p = pEnd + 1;
                        }
                        else
                            // user pressed "Cancel":
                            // get outta here!!
                            brc = FALSE;

                        xstrClear(&strPrompt);
                    }
                    else
                        // no closing bracket: just copy
                        p++;
                break; }

                default:
                    // any other character: append
                    xstrcatc(&strParamsNew, *p);
                    p++;
            } // end switch (*p)
        } // end while ( (*p) && (brc == TRUE) )
    } // end if (pcszParams)

    if (brc)
    {
        // no error, not cancelled:

        // we are now either done with copying the existing params string,
        // or no params string existed in the first place...

        // now check if we should still add the filename

        if (fAppendDataFilename)
        {
            // this is still TRUE if none of the "%" placeholders have
            // been found;
            // append filename to the end

            // if we have parameters already, append space
            // if the last char isn't a space yet
            if (strParamsNew.ulLength)
                // space as last character?
                if (    *(strParamsNew.psz + strParamsNew.ulLength - 1)
                     != ' ')
                    xstrcatc(&strParamsNew, ' ');

            FixSpacesInFilename(szDataFilename); // V0.9.7 (2000-12-10) [umoeller]
            xstrcat(&strParamsNew, szDataFilename, 0);
        }

        // return new params to caller
        *ppszParamsNew = strParamsNew.psz;

    } // end if (brc)
    else
        // cancelled or error:
        xstrClear(&strParamsNew);

    return (brc);
}

/*
 *@@ progSetupEnv:
 *      sets up the environment for progOpenProgram.
 *
 *      The caller must free the PSZ which is returned.
 *
 *@@added V0.9.7 (2000-12-17) [umoeller]
 */

PSZ progSetupEnv(WPObject *pProgObject,        // in: WPProgram or WPProgramFile
                 const char *pcszEnv,          // in: its environment string (or NULL)
                 WPFileSystem *pFile)          // in: file or NULL
{
    PSZ             pszNewEnv = NULL;
    APIRET          arc = NO_ERROR;
    DOSENVIRONMENT  Env = {0};

    // _Pmpf((__FUNCTION__ ": pcszEnv is %s", (pcszEnv) ? pcszEnv : "NULL"));

    if (pcszEnv)
        // environment specified:
        arc = doshParseEnvironment(pcszEnv,
                                   &Env);
    else
        // no environment specified:
        // get the one from the WPS process
        arc = doshGetEnvironment(&Env);

    if (arc == NO_ERROR)
    {
        // we got the environment:
        PSZ         *pp = 0;
        CHAR        szTemp[100];
        HOBJECT     hobjProgram = _wpQueryHandle(pProgObject);

        // 1) change WORKPLACE_PROCESS=YES to WORKPLACE__PROCESS=NO

        pp = doshFindEnvironmentVar(&Env, "WORKPLACE_PROCESS");
        if (pp)
        {
            // _Pmpf(("  found %s", *pp));
            // variable was set (should always be the case, since
            // this is set for the second PMSHELL.EXE):
            // we can simply overwrite the string in there,
            // because WORKPLACE_PROCESS=YES has the same length
            // as      WORKPLACE__PROCESS=NO
            strcpy(*pp, "WORKPLACE__PROCESS=NO");
        }

        // 2) set WP_OBJHANDLE

        if (pFile)
        {
            // file as argument: use WP_OBJHANDLE=xxx,yyy with
            // the handle of the file _and_ the program
            sprintf(szTemp,
                    "WP_OBJHANDLE=%d,%d",
                    _wpQueryHandle(pFile),
                    hobjProgram);
        }
        else
        {
            // no file specified: use WP_OBJHANDLE=xxx with
            // the handle of the program only
            sprintf(szTemp, "WP_OBJHANDLE=%d", hobjProgram);
        }

        arc = doshSetEnvironmentVar(&Env,
                                    szTemp,
                                    TRUE);      // add as first entry

        if (arc == NO_ERROR)
        {
            // rebuild environment
            arc = doshConvertEnvironment(&Env,
                                         &pszNewEnv,
                                         NULL);
            if (arc != NO_ERROR)
                if (pszNewEnv)
                    free(pszNewEnv);
        }

        doshFreeEnvironment(&Env);
    }

    return (pszNewEnv);
}

/*
 *@@ progOpenProgram:
 *      this opens the specified program object, which
 *      must be of the WPProgram or WPProgramFile class.
 *      This implements workarounds to WinStartApp to
 *      mimic the typical WPS behavior on program objects.
 *      Even better, this can handle data files as arguments
 *      to program objects, with full support for the various
 *      parameter placeholders.
 *
 *      This is a complete implementation of running
 *      programs in the WPS. Since IBM was not kind
 *      enough to export a method interface for doing
 *      this, and since there is no way of starting a
 *      program object with a data file as a parameter,
 *      I had to rewrite all this.
 *
 *      Presently, this gets called from:
 *
 *      -- XFldDataFile::wpOpen to open a program (file)
 *         object which has been associated with a data
 *         file. In that case, pArgDataFile is set to
 *         the data file whose association is to be started
 *         with the data file as an argument.
 *
 *      Features:
 *
 *      1)  This handles special executable flags as with
 *          WPProgram (e.g. "*" for a command line). See
 *          winhStartApp for details, which gets called from
 *          here to call WinStartApp in turn.
 *
 *      2)  If pArgDataFile is != NULL, this function handles
 *          data files as arguments to a program file. In that
 *          case, we also evaluate placeholders in the program
 *          object "parameters" data field (see wpQueryProgDetails).
 *          See progSetupArgs for supported placeholders, which
 *          gets called from here.
 *
 *          In addition, if the associated program does not have
 *          a startup directory, this is set to the data file's
 *          directory.
 *
 *      3)  This creates a use item to take care of "in use"
 *          emphasis on the icon exactly as the WPS does it.
 *          If pArgDataFile is specified, "in use" emphasis
 *          is added to the data file's icon. Otherwise it is
 *          added to the program object icon. See progStoreRunningApp,
 *          which gets called from here.
 *
 *          Note: The XWorkplace thread-1 object window
 *          (krn_fnwpThread1Object) is used as the notify
 *          window to WinStartApp to receive WM_APPTERMINATENOTIFY
 *          so we can properly remove source emphasis later.
 *          krn_fnwpThread1Object then calls progAppTerminateNotify.
 *
 *      Since this calls progSetupArgs, this might display modal
 *      dialogs before returning. As a result (and because this
 *      calls winhStartApp in turn), the calling thread must
 *      have a message queue.
 *
 *@@added V0.9.6 (2000-10-16) [umoeller]
 *@@changed V0.9.7 (2000-12-10) [umoeller]: fixed startup dir with data files
 *@@changed V0.9.7 (2000-12-17) [umoeller]: now building environment correctly
 */

HAPP progOpenProgram(WPObject *pProgObject,     // in: WPProgram or WPProgramFile
                     WPFileSystem *pArgDataFile,  // in: data file as arg or NULL
                     ULONG ulMenuID)            // in: with data files, menu ID that was used
{
    HAPP happ = 0;

    ULONG   ulNesting = 0;
    DosEnterMustComplete(&ulNesting);

    TRY_LOUD(excpt1)
    {
        PSZ     pszProgTitle = _wpQueryTitle(pProgObject);

        // _Pmpf((__FUNCTION__ ": Entering with \"%s\"", pszProgTitle));

        // make sure we really have a program
        if (    (_somIsA(pProgObject, _WPProgram))
             || (_somIsA(pProgObject, _WPProgramFile))
           )
        {
            // OK:

            // get program data
            // (wpQueryProgDetails is supported on both WPProgram and WPProgramFile)
            ULONG   ulSize = 0;
            if ((_wpQueryProgDetails(pProgObject, (PPROGDETAILS)NULL, &ulSize)))
            {
                PPROGDETAILS    pProgDetails = 0;
                if ((pProgDetails = (PPROGDETAILS)malloc(ulSize)) != NULL)
                {
                    if ((_wpQueryProgDetails(pProgObject, pProgDetails, &ulSize)))
                    {
                        // OK, now we got the program object data....
                        /*
                        typedef struct _PROGDETAILS {
                          ULONG        Length;          //  Length of structure.
                          PROGTYPE     progt;           //  Program type.
                          PSZ          pszTitle;        //  Title.
                          PSZ          pszExecutable;   //  Executable file name (program name).
                          PSZ          pszParameters;   //  Parameter string.
                          PSZ          pszStartupDir;   //  Start-up directory.
                          PSZ          pszIcon;         //  Icon-file name.
                          PSZ          pszEnvironment;  //  Environment string.
                          SWP          swpInitial;      //  Initial window position and size.
                        } PROGDETAILS; */

                        // fix parameters (placeholders etc.)
                        PSZ             pszParams = 0;

                        if (    (progSetupArgs(pProgDetails->pszParameters,
                                               pArgDataFile,
                                               &pszParams))
                           )
                        {
                            PCKERNELGLOBALS pKernelGlobals = krnQueryGlobals();

                            PSZ     pszStartupDir = pProgDetails->pszStartupDir;
                            CHAR    szDatafileDir[CCHMAXPATH] = "";

                            // if startup dir is empty, but data file is given,
                            // use data file's folder as startup dir...
                            if ((!pszStartupDir) || (*pszStartupDir == 0))
                                if (pArgDataFile)
                                {
                                    // V0.9.7 (2000-12-10) [umoeller]: better to query folder's dir
                                    WPFolder *pFilesFolder = _wpQueryFolder(pArgDataFile);
                                    if (pFilesFolder)
                                        if (_wpQueryFilename(pFilesFolder, szDatafileDir, TRUE))
                                            pszStartupDir = szDatafileDir;
                                }

                            // set the new params and startup dir
                            pProgDetails->pszParameters = pszParams;
                            pProgDetails->pszStartupDir = pszStartupDir;

                            // build the new environment V0.9.7 (2000-12-17) [umoeller]
                            pProgDetails->pszEnvironment
                                = progSetupEnv(pProgObject,
                                               pProgDetails->pszEnvironment,
                                               pArgDataFile);

                            // start the app (more hacks in winhStartApp,
                            // which calls WinStartApp in turn)
                            happ = winhStartApp(pKernelGlobals->hwndThread1Object,
                                                pProgDetails);

                            if (happ)
                            {
                                // app started OK:
                                // set in-use emphasis on either
                                // the data file or the program object
                                progStoreRunningApp(pProgObject,
                                                    pArgDataFile,
                                                    happ,
                                                    ulMenuID);

                            }

                            if (pProgDetails->pszEnvironment)
                                free(pProgDetails->pszEnvironment);

                        } // end if progSetupArgs

                        if (pszParams)
                            free(pszParams);
                    }

                    free(pProgDetails);
                }
            }
        }
    }
    CATCH(excpt1)
    {
        happ = 0;
    } END_CATCH();

    DosExitMustComplete(&ulNesting);

    return (happ);
}

