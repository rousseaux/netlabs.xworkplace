
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
    PUSEITEM    pUseItem;           // use item; has HAPP
} RUNNINGPROGRAM, *PRUNNINGPROGRAM;

/*
 *@@ progStoreRunningApp:
 *      registers a "running" view useitem for the specified
 *      object.
 *
 *      The object can be a WPProgram, WPProgramFile, or
 *      WPDataFile, depending on whether a program was
 *      started directly or via a data file association.
 *
 *      In any case, this adds an OPEN_RUNNING useitem to
 *      the specified object to give it "in use" emphasis.
 *
 *@@added V0.9.6 (2000-10-16) [umoeller]
 */

BOOL progStoreRunningApp(HAPP happ,
                         WPObject *pObjEmphasis)
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
            if ((happ) && (pObjEmphasis))
            {
                PRUNNINGPROGRAM pRunning = (PRUNNINGPROGRAM)malloc(sizeof(RUNNINGPROGRAM));
                if (pRunning)
                {
                    // allocate view item
                    PUSEITEM pUseItem = (PUSEITEM)_wpAllocMem(pObjEmphasis,
                                                              sizeof(USEITEM) + sizeof(VIEWITEM),
                                                              NULL);
                    if (pUseItem)
                    {
                        // view item is right behind use item
                        PVIEWITEM pViewItem = (PVIEWITEM)(pUseItem + 1);

                        // setup up data
                        memset(pUseItem, 0, sizeof(USEITEM) + sizeof(VIEWITEM));
                        pUseItem->type = USAGE_OPENVIEW;
                        pViewItem->view = OPEN_RUNNING;     // even for data files
                        pViewItem->handle = happ;

                        // register this view; this adds emphasis to the object
                        brc = _wpAddToObjUseList(pObjEmphasis,
                                                 pUseItem);

                        if (brc)
                        {
                            // store this in our internal list
                            // so we can find the object
                            // in progAppTerminateNotify
                            pRunning->pObjEmphasis = pObjEmphasis;
                            pRunning->pUseItem = pUseItem;
                            lstAppendItem(&G_llRunning,
                                          pRunning);

                            // set list-notify flag on the object
                            // so that XFldObject will call
                            // progAppTerminateNotifyObj when
                            // the object is destroyed
                            _xwpModifyListNotify(pObjEmphasis,
                                                 OBJLIST_RUNNINGSTORED,
                                                 OBJLIST_RUNNINGSTORED);
                        }
                    }
                    else
                        free(pRunning);
                }
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
                if (pRunning->pUseItem)
                {
                    PVIEWITEM pViewItem = (PVIEWITEM)((pRunning->pUseItem) + 1);
                    if (pViewItem->handle == happ)
                    {
                        // yes, this is ours:

                        // remove this from the object's use list
                        _wpDeleteFromObjUseList(pRunning->pObjEmphasis,
                                                pRunning->pUseItem);

                        _wpFreeMem(pRunning->pObjEmphasis,
                                   (PBYTE)pRunning->pUseItem);

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
 *@@added V0.9.6 (2000-10-23) [umoeller]
 */

BOOL progRunningAppDestroyed(WPObject *pObjEmphasis)
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
                            // since the memory for the use-item has been
                            // allocated using wpAllocMem, this will be
                            // freed automatically...

                    brc = TRUE;

                    // stop searching
                    break;
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
 *
 *      This function must either return FALSE or it MUST
 *      replace the [] thing in the buffer, or the caller
 *      will run in an endless loop.
 *
 *@@added V0.9.6 (2000-10-16) [umoeller]
 */

BOOL progDisplayParamsPrompt(PSZ *ppszParamsNew,
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
                    WinQueryDlgItemText(hwndDlg, ID_XSDI_FT_ENTRYFIELD,
                                        sizeof(szNew), szNew);
                    xstrrpl(ppszParamsNew,
                            0,
                            pszBrackets,
                            szNew,
                            0);
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
 */

BOOL progSetupArgs(const char *pcszParams,
                   WPFileSystem *pFile,         // in: file or NULL
                   PSZ *ppszParamsNew)          // out: new params
{
    BOOL    brc = TRUE;
    PSZ     pszParamsNew = 0;
    BOOL    fAppendDataFilename = TRUE;

    CHAR    szDataFilename[CCHMAXPATH] = "";
    if (pFile)
        // we have a data file as argument:
        // get fully q'fied data file name
        _wpQueryFilename(pFile, szDataFilename, TRUE);
    // else no file: szDataFilename is empty

    if (pcszParams)
    {
        // program object has standard parameters:
        // note that this is frequently ""

        PSZ     p = 0;
        BOOL    fFirstLoop = FALSE;
        CHAR    szTemp[CCHMAXPATH];

        // copy existing params into new buffer
        // so we can search and replace
        pszParamsNew = strdup(pcszParams);

        //  "%**P": drive and path information without the last backslash (\).
        fFirstLoop = TRUE;
        while (p = strstr(pszParamsNew, "%**P"))
        {
            if (fFirstLoop)
            {
                // first time: copy drive and path
                PSZ p2 = strrchr(szDataFilename, '\\');
                szTemp[0] = 0;
                if (p2)
                    strhncpy0(szTemp, szDataFilename, p2 - szDataFilename);
                fFirstLoop = FALSE;
            }
            xstrrpl(&pszParamsNew, 0, "%**P", szTemp, NULL);

            // disable appending the full path to the end
            fAppendDataFilename = FALSE;
        }

        //  "%**D": drive with ':' or UNC name.
        fFirstLoop = TRUE;
        while (p = strstr(pszParamsNew, "%**D"))
        {
            if (fFirstLoop)
            {
                PSZ p2 = strchr(szDataFilename + 2, '\\');
                    // works on "C:\blah" or "\\unc\blah"
                szTemp[0] = 0;
                if (p2)
                    strhncpy0(szTemp, szDataFilename, p2 - szDataFilename);
                fFirstLoop = FALSE;
            }
            xstrrpl(&pszParamsNew, 0, "%**D", szTemp, NULL);

            // disable appending the full path to the end
            fAppendDataFilename = FALSE;
        }

        //  "%**N": file name without extension.
        fFirstLoop = TRUE;
        while (p = strstr(pszParamsNew, "%**N"))
        {
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
            xstrrpl(&pszParamsNew, 0, "%**N", szTemp, NULL);

            // disable appending the full path to the end
            fAppendDataFilename = FALSE;
        }

        // "%**F": file name with extension.
        fFirstLoop = TRUE;
        while (p = strstr(pszParamsNew, "%**F"))
        {
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
            xstrrpl(&pszParamsNew, 0, "%**F", szTemp, NULL);

            // disable appending the full path to the end
            fAppendDataFilename = FALSE;
        }

        //  "%**E": extension without leading dot.
        // In HPFS, the extension always comes after the last dot.
        fFirstLoop = TRUE;
        {
            PSZ pszExt = "";
            while (p = strstr(pszParamsNew, "%**E"))
            {
                if (fFirstLoop)
                {
                    pszExt = doshGetExtension(szDataFilename);
                    if (!pszExt)
                        pszExt = "";
                    fFirstLoop = FALSE;
                }
                xstrrpl(&pszParamsNew, 0, "%**E", pszExt, NULL);

                // disable appending the full path to the end
                fAppendDataFilename = FALSE;
            }
        }

        // "%*": full path of data file, if pArgDataFile
        while (p = strstr(pszParamsNew, "%*"))
        {
            xstrrpl(&pszParamsNew, 0, "%*", szDataFilename, NULL);

            // disable appending the full path to the end
            fAppendDataFilename = FALSE;
        }

        // "%": disable passing full path
        while (p = strstr(pszParamsNew, "%"))
        {
            xstrrpl(&pszParamsNew, 0, "%", "", NULL);

            // disable appending the full path to the end
            fAppendDataFilename = FALSE;
        }

        // now go for prompts...
        while (p = strchr(pszParamsNew, '['))
        {
            PSZ pStart = p,
                pEnd;
            if (pEnd = strchr(pStart, ']'))
            {
                brc = progDisplayParamsPrompt(&pszParamsNew,
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
        // append to the end
        if ((pszParamsNew) && (strlen(pszParamsNew)))
            if (*(pszParamsNew + strlen(pszParamsNew) - 1) != ' ')
                xstrcat(&pszParamsNew,
                        " ");

        xstrcat(&pszParamsNew,
                szDataFilename);
    }

    if (brc)
        *ppszParamsNew = pszParamsNew;
    else
        // cancelled or error:
        if (pszParamsNew)
            free(pszParamsNew);

    return (brc);
}

/*
 *@@ progOpenProgram:
 *      this opens the specified program object, which
 *      must be of the WPProgram or WPProgramFile class.
 *      This implements workarounds to WinStartApp to
 *      mimic the typical WPS behavior on program objects.
 *      Even better, this can handle data files as arguments
 *      to program objects.
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
                     WPFileSystem *pArgDataFile)  // in: data file as arg or NULL
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
                                WPObject *pObj2Store = pProgObject;
                                if (pArgDataFile)
                                    pObj2Store = pArgDataFile;
                                progStoreRunningApp(happ,
                                                    pObj2Store);
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

