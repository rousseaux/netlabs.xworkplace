
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

    TRY_LOUD(excpt1, NULL)
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

    TRY_LOUD(excpt1, NULL)
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

    TRY_LOUD(excpt1, NULL)
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
 *      This function must either return FALSE or it MUST
 *      replace the [] thing in the buffer, or the caller
 *      will run in an endless loop.
 *
 *@@added V0.9.6 (2000-10-16) [umoeller]
 */

BOOL DisplayParamsPrompt(PXSTRING pstrParamsNew,
                         PSZ pStart,
                         PSZ pEnd)
{
    BOOL brc = TRUE;

    // prompt found: p now contains a new buffer
    // extract complete [] part
    PSZ     pszBrackets = strhSubstr(pStart, pEnd + 1);
    if (pszBrackets)
    {
        PSZ     pszString = strhSubstr(pStart + 1, pEnd);
        if (pszString)
        {
            HWND hwndDlg = WinLoadDlg(HWND_DESKTOP,     // parent
                                      NULLHANDLE,       // owner
                                      WinDefDlgProc,
                                      cmnQueryNLSModuleHandle(FALSE),
                                      ID_XSD_NEWFILETYPE,   // "New File Type" dlg
                                      NULL);            // pCreateParams
            if (hwndDlg)
            {
                WinSetWindowText(hwndDlg, "");
                WinSetDlgItemText(hwndDlg,
                                  ID_XSDI_FT_TITLE,
                                  pszString);
                WinSendDlgItemMsg(hwndDlg, ID_XSDI_FT_ENTRYFIELD,
                                  EM_SETTEXTLIMIT,
                                  (MPARAM)255,
                                  0);
                if (WinProcessDlg(hwndDlg) == DID_OK)
                {
                    CHAR    szNew[300];
                    /* XSTRING strFind,
                            strReplace; */
                    ULONG   ulOfs = 0;
                    // xstrInit(&strFind, 0);
                    // xstrset(&strFind, pszBrackets);     // must not be freed here!
                    // xstrInit(&strReplace, 0);

                    WinQueryDlgItemText(hwndDlg, ID_XSDI_FT_ENTRYFIELD,
                                        sizeof(szNew), szNew);
                    // xstrset(&strReplace, szNew);        // must not be freed here!
                    xstrcrpl(pstrParamsNew,
                             &ulOfs,
                             pszBrackets, // &strFind,
                             szNew); // &strReplace);
                }
                else
                    // user pressed "Cancel":
                    brc = FALSE;

                WinDestroyWindow(hwndDlg);
            }
            else
                brc = FALSE;

            free(pszString);
        } // end if (pString)
        else
            brc = FALSE;

        free(pszBrackets);
    } // end if (pszBrackets)
    else
        // error:
        brc = FALSE;

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
 *      From my testing, the WPS always appends the full path
 *      specification (%*) of the data file to the end of the
 *      program's arguments list, UNLESS "%*" or one of the
 *      "%**X" parameters are specified at least once.
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
 */

BOOL progSetupArgs(const char *pcszParams,
                   WPFileSystem *pFile,         // in: file or NULL
                   PSZ *ppszParamsNew)          // out: new params
{
    BOOL    brc = TRUE;
    XSTRING strParamsNew;
    BOOL    fAppendDataFilename = TRUE;

    CHAR    szDataFilename[CCHMAXPATH] = "";
    if (pFile)
        // we have a data file as argument:
        // get fully q'fied data file name
        _wpQueryFilename(pFile, szDataFilename, TRUE);
    // else no file: szDataFilename is empty

    xstrInit(&strParamsNew, 200);

    // copy existing params into new buffer
    // so we can search and replace;
    // note that this is frequently ""
    xstrcpy(&strParamsNew, pcszParams);

    if (strParamsNew.ulLength) // fixed V0.9.6 (2000-11-04) [umoeller]
    {
        // program object has standard parameters:

        PSZ     p = 0;
        BOOL    fFirstLoop = FALSE;
        CHAR    szTemp[CCHMAXPATH];

        //  "%**P": drive and path information without the last backslash (\).
        fFirstLoop = TRUE;
        while (p = strstr(strParamsNew.psz, "%**P"))
        {
            ULONG ulOfs = 0;
            if (fFirstLoop)
            {
                // first time: copy drive and path
                PSZ p2 = strrchr(szDataFilename, '\\');
                szTemp[0] = 0;
                if (p2)
                    strhncpy0(szTemp, szDataFilename, p2 - szDataFilename);
                fFirstLoop = FALSE;
            }
            xstrcrpl(&strParamsNew, &ulOfs, "%**P", szTemp);

            // disable appending the full path to the end
            fAppendDataFilename = FALSE;
        }

        //  "%**D": drive with ':' or UNC name.
        fFirstLoop = TRUE;
        while (p = strstr(strParamsNew.psz, "%**D"))
        {
            ULONG ulOfs = 0;
            if (fFirstLoop)
            {
                PSZ p2 = strchr(szDataFilename + 2, '\\');
                    // works on "C:\blah" or "\\unc\blah"
                szTemp[0] = 0;
                if (p2)
                    strhncpy0(szTemp, szDataFilename, p2 - szDataFilename);
                fFirstLoop = FALSE;
            }
            xstrcrpl(&strParamsNew, &ulOfs, "%**D", szTemp);

            // disable appending the full path to the end
            fAppendDataFilename = FALSE;
        }

        //  "%**N": file name without extension.
        fFirstLoop = TRUE;
        while (p = strstr(strParamsNew.psz, "%**N"))
        {
            ULONG ulOfs = 0;
            if (fFirstLoop)
            {
                // first find filename
                PSZ p2 = strrchr(szDataFilename + 2, '\\');
                    // works on "C:\blah" or "\\unc\blah"
                szTemp[0] = 0;
                if (p2)
                {
                    // find last dot in filename
                    PSZ p3 = strrchr(p2 + 1, '.');
                    if (p3)
                    {
                        // extension found:
                        // copy from p2+1 to p3
                        strhncpy0(szTemp, p2+1, p3 - (p2 + 1));
                    }
                    else
                        // no extension:
                        strcpy(szTemp, p2 + 1);
                }
                fFirstLoop = FALSE;
            }
            xstrcrpl(&strParamsNew, &ulOfs, "%**N", szTemp);

            // disable appending the full path to the end
            fAppendDataFilename = FALSE;
        }

        // "%**F": file name with extension.
        fFirstLoop = TRUE;
        while (p = strstr(strParamsNew.psz, "%**F"))
        {
            ULONG ulOfs = 0;
            if (fFirstLoop)
            {
                // find filename
                PSZ p2 = strrchr(szDataFilename + 2, '\\');
                    // works on "C:\blah" or "\\unc\blah"
                szTemp[0] = 0;
                if (p2)
                    strcpy(szTemp, p2 + 1);
                fFirstLoop = FALSE;
            }
            xstrcrpl(&strParamsNew, &ulOfs, "%**F", szTemp);

            // disable appending the full path to the end
            fAppendDataFilename = FALSE;
        }

        //  "%**E": extension without leading dot.
        // In HPFS, the extension always comes after the last dot.
        fFirstLoop = TRUE;
        {
            ULONG ulOfs = 0;
            PSZ pszExt = "";
            while (p = strstr(strParamsNew.psz, "%**E"))
            {
                if (fFirstLoop)
                {
                    pszExt = doshGetExtension(szDataFilename);
                    if (!pszExt)
                        pszExt = "";
                    fFirstLoop = FALSE;
                }
                xstrcrpl(&strParamsNew, &ulOfs, "%**E", pszExt);

                // disable appending the full path to the end
                fAppendDataFilename = FALSE;
            }
        }

        // "%*": full path of data file, if pArgDataFile
        while (p = strstr(strParamsNew.psz, "%*"))
        {
            ULONG ulOfs = 0;
            xstrcrpl(&strParamsNew, &ulOfs, "%*", szDataFilename);

            // disable appending the full path to the end
            fAppendDataFilename = FALSE;
        }

        // "%": disable passing full path
        while (p = strstr(strParamsNew.psz, "%"))
        {
            ULONG ulOfs = 0;
            xstrcrpl(&strParamsNew, &ulOfs, "%", "");

            // disable appending the full path to the end
            fAppendDataFilename = FALSE;
        }

        // now go for prompts...
        while (p = strchr(strParamsNew.psz, '['))
        {
            PSZ pStart = p,
                pEnd;
            if (pEnd = strchr(pStart, ']'))
            {
                brc = DisplayParamsPrompt(&strParamsNew,
                                          pStart,
                                          pEnd);

            } // if (pEnd = strchr(pStart, ']'))
            else
                break;

            if (!brc)
                break;

        } // end while (p = strchr(p, '['))

    } // end if (pcszParams)

    if (fAppendDataFilename)
    {
        // none of the above worked:
        // append filename to the end
        if (strParamsNew.ulLength)
            // space as last character?
            if (    *(strParamsNew.psz + strParamsNew.ulLength - 1)
                 != ' ')
                xstrcat(&strParamsNew,
                        " ");

        xstrcat(&strParamsNew,
                szDataFilename);
    }

    if (brc)
        *ppszParamsNew = strParamsNew.psz;
    else
        // cancelled or error:
        xstrClear(&strParamsNew);

    return (brc);
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
 *@@added V0.9.6 (2000-10-16) [umoeller]
 */

HAPP progOpenProgram(WPObject *pProgObject,     // in: WPProgram or WPProgramFile
                     WPFileSystem *pArgDataFile,  // in: data file as arg or NULL
                     ULONG ulMenuID)            // in: with data files, menu ID that was used
{
    HAPP happ = 0;

    TRY_LOUD(excpt1, NULL)
    {
        PSZ     pszProgTitle = _wpQueryTitle(pProgObject);

        _Pmpf((__FUNCTION__ ": Entering with \"%s\"", pszProgTitle));

        // make sure we really have a program
        if (    (_somIsA(pProgObject, _WPProgram))
             || (_somIsA(pProgObject, _WPProgramFile))
           )
        {
            // OK:

            // get program data
            // (wpQueryProgDetails is supported on both WPprogram and WPProgramFile)
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
                        PSZ     pszParams = 0;
                        if (progSetupArgs(pProgDetails->pszParameters,
                                          pArgDataFile,
                                          &pszParams))
                        {
                            PCKERNELGLOBALS pKernelGlobals = krnQueryGlobals();

                            PSZ     pszStartupDir = pProgDetails->pszStartupDir;
                            CHAR    szDatafileDir[CCHMAXPATH];
                            _Pmpf((__FUNCTION__ ": calling winhStartApp \"%s\" with args \"%s\"",
                                        pProgDetails->pszExecutable,
                                        (pszParams) ? pszParams : "NULL"));

                            // if startup dir is empty, but
                            // data file is given, use data file's directory
                            // as startup dir...
                            if ((!pszStartupDir) || (*pszStartupDir == 0))
                                if (pArgDataFile)
                                {
                                    PSZ p = 0;
                                    _wpQueryFilename(pArgDataFile, szDatafileDir, TRUE);
                                    p = strrchr(szDatafileDir, '\\');
                                    if (p)
                                    {
                                        *p = 0;
                                        pszStartupDir = szDatafileDir;
                                    }
                                }

                            // start the application...
                            pProgDetails->pszParameters = pszParams;
                            pProgDetails->pszStartupDir = pszStartupDir;
                            happ = winhStartApp(pKernelGlobals->hwndThread1Object,
                                                pProgDetails);
                            if (happ)
                            {
                                // set in-use emphasis on either
                                // the data file or the program object
                                progStoreRunningApp(pProgObject,
                                                    pArgDataFile,
                                                    happ,
                                                    ulMenuID);

                            }
                        }

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

    return (happ);
}

