
/*
 * netscdde.c:
 *      this is the main (and only) C file for the
 *      Netscape DDE interface. This code is much more
 *      messy than XWorkplace's. It's a rather quick hack
 *      done in about two days with DDE code stolen from
 *      various places.
 *
 *      Use the undocumented "-D" parameter on the command
 *      line to start NetscDDE in "debug" mode, which will
 *      display a frame window with a menu where you may
 *      debug the DDE messages. This window is invisible
 *      when "-D" is not used. (Ugly, huh.)
 *
 *      Netscape's DDE topics are (horribly) documented
 *      for all Netscape versions at
 *      http://developer.netscape.com/library/documentation/communicator/DDE
 *
 *      Copyright (C) 1997-2000 Ulrich M”ller.
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, in version 2 as it comes in the COPYING
 *      file of the XWorkplace main distribution.
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 */

#define  INCL_WIN
#define  INCL_DOS

#include <os2.h>
#include <stdio.h>
#include <string.h>

#include "setup.h"

#include "netscdde.h"
#include "dlgids.h"

void            ShowMessage(PSZ);
MRESULT EXPENTRY fnwpMain(HWND, ULONG, MPARAM, MPARAM);

HAB             hab;
HWND            hwndDebug, hwndListbox, hServerWnd = NULLHANDLE;
PFNWP           SysWndProc;

// NLS Resource DLL
HMODULE     hmodNLS = NULLHANDLE;

CHAR            szURL[400] = "";

ULONG           idTimer = 0;

CONVCONTEXT     context;

CHAR            szDDENetscape[] = "NETSCAPE",   // DDE server name
                szNetscapeApp[CCHMAXPATH] = "NETSCAPE.EXE",     // default program to start
                                                        // if not running
                szNetscapeParams[CCHMAXPATH] = "";  // space for params

PSZ             szOpenURLTopic = "WWW_OpenURL";     // open URL DDE topic

                                                        // (see Netscape docs)

// options flags, modified by command line interface
BOOL            optNewWindow = FALSE,
                optDebug = FALSE,
                optExecute = TRUE,
                optConfirmStart = TRUE,
                optMinimized = FALSE,
                optHidden = FALSE,
                optQuiet = FALSE;           // "-q", don't show status windows

BOOL            NetscapeFound = FALSE;

// status window handle
HWND            hwndContacting = NULLHANDLE;

/*
 * CenterWindow:
 *      centers a window within its parent window.
 *      The window should not be visible to avoid flickering.
 */

void CenterWindow(HWND hwnd)
{
    RECTL           rclParent;
    RECTL           rclWindow;

    WinQueryWindowRect(hwnd, &rclWindow);
    WinQueryWindowRect(WinQueryWindow(hwnd, QW_PARENT), &rclParent);

    rclWindow.xLeft = (rclParent.xRight - rclWindow.xRight) / 2;
    rclWindow.yBottom = (rclParent.yTop - rclWindow.yTop) / 2;

    WinSetWindowPos(hwnd, NULLHANDLE, rclWindow.xLeft, rclWindow.yBottom,
                    0, 0, SWP_MOVE | SWP_SHOW);
}

/*
 * WinCenteredDlgBox:
 *      just like WinDlgBox, but the window is centered
 */

ULONG WinCenteredDlgBox(HWND hwndParent,
                        HWND hwndOwner,
                        PFNWP pfnDlgProc,
                        HMODULE hmod,
                        ULONG idDlg,
                        PVOID pCreateParams)
{
    ULONG           ulReply = DID_CANCEL;
    HWND            hwndDlg = WinLoadDlg(hwndParent, hwndOwner, pfnDlgProc,
                                         hmod, idDlg, pCreateParams);

    if (hwndDlg)
    {
        CenterWindow(hwndDlg);
        ulReply = WinProcessDlg(hwndDlg);
        WinDestroyWindow(hwndDlg);
    }
    else
        WinMessageBox(HWND_DESKTOP, HWND_DESKTOP,
                        "Error loading dlg", "NetscDDE",
                        0, MB_OK | MB_MOVEABLE);

    return (ulReply);
}

/*
 * ExplainParams:
 *      this displays the dlg box which explains
 *      NetscDDE's usage in brief; called when the
 *      parameters on the cmd line don't seem to
 *      be complete
 */

VOID ExplainParams(VOID)
{
    WinCenteredDlgBox(HWND_DESKTOP, HWND_DESKTOP,
                      WinDefDlgProc,
                      hmodNLS,
                      ID_NDD_EXPLAIN,
                      NULL);
}

/*
 *@@ LoadNLS:
 *      NetscDDE NLS interface.
 *
 *@@added V0.9.1 (99-12-19) [umoeller]
 */

BOOL LoadNLS(VOID)
{
    CHAR        szNLSDLL[2*CCHMAXPATH];
    BOOL Proceed = TRUE;

    if (PrfQueryProfileString(HINI_USER,
                              "XWorkplace",
                              "XFolderPath",
                              "",
                              szNLSDLL, sizeof(szNLSDLL)) < 3)

    {
        WinMessageBox(HWND_DESKTOP, HWND_DESKTOP,
                      "NetscapeDDE was unable to determine the location of the "
                      "XWorkplace National Language Support DLL, which is "
                      "required for operation. The OS2.INI file does not contain "
                      "this information. "
                      "NetscapeDDE cannot proceed. Please re-install XWorkplace.",
                      "NetscapeDDE: Fatal Error",
                      0, MB_OK | MB_MOVEABLE);
        Proceed = FALSE;
    }
    else
    {
        CHAR    szLanguageCode[50] = "";

        // now compose module name from language code
        PrfQueryProfileString(HINI_USERPROFILE,
                              "XWorkplace", "Language",
                              "001",
                              (PVOID)szLanguageCode,
                              sizeof(szLanguageCode));
        strcat(szNLSDLL, "\\bin\\xfldr");
        strcat(szNLSDLL, szLanguageCode);
        strcat(szNLSDLL, ".dll");

        // try to load the module
        if (DosLoadModule(NULL,
                          0,
                          szNLSDLL,
                          &hmodNLS))
        {
            CHAR    szMessage[2000];
            sprintf(szMessage,
                    "NetscapeDDE was unable to load \"%s\", "
                    "the National Language DLL which "
                    "is specified for XWorkplace in OS2.INI.",
                    szNLSDLL);
            WinMessageBox(HWND_DESKTOP, HWND_DESKTOP,
                          szMessage,
                          "NetscapeDDE: Fatal Error",
                          0, MB_OK | MB_MOVEABLE);
            Proceed = FALSE;
        }

        _Pmpf(("Loaded %s --> hmodNLS 0x%lX", szNLSDLL, hmodNLS));
    }

    return (Proceed);
}

/*
 * main:
 *      program entry point; accepts URLs on the command line.
 *
 *@@changed V0.9.1 (2000-02-07) [umoeller]: added "-q" option
 */

int main(int argc,
         char *argv[])
{
    HMQ             hmq;
    FRAMECDATA      fcd;
    QMSG            qmsg;
    BOOL            Proceed = TRUE;

    if (!(hab = WinInitialize(0)))
        return (1);

    if (!(hmq = WinCreateMsgQueue(hab, 0)))
        return (1);

    // now attempt to find the XWorkplace NLS resource DLL,
    // which we need for all resources (new with XWP 0.9.0)
    Proceed = LoadNLS();

    if (Proceed)
    {
        // parse parameters on cmd line
        if (argc > 1)
        {
            SHORT           i = 0;

            while (i++ < argc - 1)
            {
                if (argv[i][0] == '-')
                {
                    SHORT           i2;

                    for (i2 = 1; i2 < strlen(argv[i]); i2++)
                    {
                        switch (argv[i][i2])
                        {
                            case 'n':
                                optNewWindow = TRUE;
                                break;

                            case 'x':
                                optExecute = FALSE;
                                break;

                            case 'm':
                                optMinimized = TRUE;
                                break;

                            case 'h':
                                optHidden = TRUE;
                                break;

                            case 'X':
                                optConfirmStart = FALSE;
                                break;

                            case 'p':   // netscape path

                                if (i < argc)
                                {
                                    strcpy(szNetscapeApp, argv[i + 1]);
                                    i++;
                                    i2 = 1000;
                                }
                                else
                                {
                                    ExplainParams();
                                    Proceed = FALSE;
                                }
                                break;

                            case 'P':   // netscape parameters

                                if (i < argc)
                                {
                                    strcpy(szNetscapeParams, argv[i + 1]);
                                    i++;
                                    i2 = 1000;
                                }
                                else
                                {
                                    ExplainParams();
                                    Proceed = FALSE;
                                }
                                break;

                            case 'D':   // debug, show list box window w/ DDE msgs

                                optDebug = TRUE;
                                break;

                            case 'q': // added V0.9.1 (2000-02-07) [umoeller]
                                optQuiet = TRUE;
                                break;

                            default:    // unknown parameter
                                ExplainParams();
                                Proceed = FALSE;
                                break;
                        }
                    }
                }
                else
                {
                    // no option ("-"): seems to be URL
                    if (strchr(argv[i], ' '))
                    {
                        // if the URL contains spaces, we enclose it in quotes
                        sprintf(szURL, "\"%s\"", argv[i]);
                    }
                    else
                        strcpy(szURL, argv[i]);
                }
            }
        }

        if (strlen(szURL) == 0)
        {
            // no URL given: explain
            ExplainParams();
            Proceed = FALSE;
        }

        if (Proceed)
        {
            // OK, parameters seemed to be correct:
            // create the main window, which is only
            // visible in Debug mode ("-D" param). But
            // even if not in debug mode, this window is
            // used for DDE message processing.
            fcd.cb = sizeof(FRAMECDATA);
            fcd.flCreateFlags = FCF_TITLEBAR |
                                   FCF_SYSMENU |
                                   FCF_MENU |
                                   FCF_SIZEBORDER |
                                   FCF_SHELLPOSITION |
                                   FCF_MINMAX |
                                   FCF_TASKLIST;

            fcd.hmodResources = NULLHANDLE;
            // set our resource key (so PM can find menus, icons, etc).
            fcd.idResources = DDEC;
            // create the frame
            hwndDebug = WinCreateWindow(HWND_DESKTOP,
                                        WC_FRAME,
                                        "Netscape DDE",
                                        0, 0, 0, 0, 0,
                                        NULLHANDLE,
                                        HWND_TOP,
                                        DDEC,
                                        &fcd,
                                        NULL);

            if (!hwndDebug)
                return (1);

            // set the NetscDDE icon for the frame window
            WinSendMsg(hwndDebug,
                       WM_SETICON,
                       (MPARAM)WinLoadPointer(HWND_DESKTOP, hmodNLS,
                                              ID_ND_ICON),
                       NULL);

            // create a list window child
            hwndListbox = WinCreateWindow(hwndDebug,
                                          WC_LISTBOX,
                                          NULL,
                                          LS_HORZSCROLL,
                                          0, 0, 0, 0,
                                          hwndDebug,
                                          HWND_BOTTOM,
                                          FID_CLIENT,
                                          NULL,
                                          NULL);

            // we must intercept the frame window's messages;
            // we save the return value (the current WndProc),
            // so we can pass it all the other messages the frame gets.
            SysWndProc = WinSubclassWindow(hwndDebug, (PFNWP) fnwpMain);

            // the window we just created is normally invisible; we
            // will only display it if the (undocumented) "-D" option
            // was given on the command line.
            if (optDebug)
                WinShowWindow(hwndDebug, TRUE);

            // now show "Contacting Netscape"
            if (!optQuiet)
            {
                hwndContacting = WinLoadDlg(HWND_DESKTOP, hwndDebug,
                                            WinDefDlgProc,
                                            hmodNLS, ID_NDD_CONTACTING,
                                            0);
                WinShowWindow(hwndContacting, TRUE);
            }

            // now post msg to main window to initiate DDE
            WinPostMsg(hwndDebug, WM_COMMAND, MPFROM2SHORT(IDM_INITIATE, 0), 0);

            //  standard PM message loop
            while (WinGetMsg(hab, &qmsg, NULLHANDLE, 0, 0))
            {
                WinDispatchMsg(hab, &qmsg);
            }
        }                           // end if (proceed)

        // clean up on the way out
        if (hwndContacting)
            WinDestroyWindow(hwndContacting);
    }

    WinDestroyMsgQueue(hmq);
    WinTerminate(hab);

    return (0);
}

/*
 * DDERequest:
 *      this routine tries to post a DDE request to Netscape
 *      and returns TRUE only if this was successful.
 */

BOOL DDERequest(HWND hwndClient,
                PSZ pszItemString)
{
    ULONG           mem;
    PID             pid;
    TID             tid;
    PDDESTRUCT      pddeStruct;
    PSZ             pszDDEItemName;

    // get some sharable memory
    DosAllocSharedMem((PVOID) & mem,
                      NULL,
                      sizeof(DDESTRUCT) + 1000,
                      PAG_COMMIT |
                      PAG_READ |
                      PAG_WRITE |
                      OBJ_GIVEABLE);

    // get the server's ID and give it access
    // to the shared memory
    WinQueryWindowProcess(hServerWnd, &pid, &tid);
    DosGiveSharedMem(&mem, pid, PAG_READ | PAG_WRITE);

    /* here is definition for DDESTRUCT, for further reference:
     * typedef struct _DDESTRUCT {
     * ULONG    cbData;
     * This is the length of data that occurs after the offabData parameter. If no
     * data exists, this field should contain a zero (0).
     * USHORT   fsStatus;       /  Status of the data exchange.
     * DDE_FACK
     * Positive acknowledgement
     * DDE_FBUSY
     * Application is busy
     * DDE_FNODATA
     * No data transfer for advise
     * DDE_FACKREQ
     * Acknowledgements are requested
     * DDE_FRESPONSE
     * Response to WM_DDE_REQUEST
     * DDE_NOTPROCESSED
     * DDE message not understood
     * DDE_FAPPSTATUS
     * A 1-byte field of bits that are reserved for application-specific returns.
     * USHORT   usFormat;       /  Data format.
     * USHORT   offszItemName;  /  Offset to item name.
     * This is the offset to the item name from the start of this structure. Item
     * name is a null (0x00) terminated string. If no item name exists, there must
     * be a single null (0x00) character in this position. (That is, ItemName is
     * ALWAYS a null terminated string.)
     *
     * USHORT   offabData;      /  Offset to beginning of data.
     * This is the offset to the data, from the start of this structure. This field
     * should be calculated regardless of the presence of data. If no data exists,
     * cbData must be zero (0).
     *
     * For compatibility reasons, this data should not contain embedded pointers.
     * Offsets should be used instead.
     *
     * --  CHAR     szItemName[]    /  offset: offszItemName
     * --  BYTE     abData[]        /  offset: offabData
     * } DDESTRUCT; */

    // setup DDE data structures
    pddeStruct = (PDDESTRUCT) mem;
    pddeStruct->fsStatus = 0;   // DDE_FACKREQ;            // Status

    pddeStruct->usFormat = DDEFMT_TEXT;     // Text format

    // go past end of data structure for the item name
    pddeStruct->offszItemName = sizeof(DDESTRUCT);

    pszDDEItemName = ((BYTE *) pddeStruct) + (pddeStruct->offszItemName);
    strcpy(pszDDEItemName, pszItemString);

    // go past end of data structure
    // (plus past the name) for the data
    pddeStruct->offabData = strlen(pszDDEItemName) + 1;
    // offset to BEGINNING of data
    pddeStruct->cbData = 500;
    // length of the data

    // post our request to the server program
    NetscapeFound = (WinDdePostMsg(hServerWnd,
                                   hwndClient,
                                   WM_DDE_REQUEST,
                                   pddeStruct,
                                   0));

    return (NetscapeFound);
}

/*
 * fnwpMain:
 *      window procedure for the main NetscDDE window, which
 *      is only visible in Debug mode ("-D" option), mostly
 *      processing DDE messages. If we're in debug mode, this
 *      routine waits for certain menu selections, otherwise
 *      the corresponding messages will be posted automatically.
 *
 *@@changed V0.9.4 (2000-07-10) [umoeller]: added DDE conflicts fix by Rousseau de Pantalon
 */

MRESULT EXPENTRY fnwpMain(HWND hwndFrame, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    PSZ             szData;
    PDDESTRUCT      pddeStruct;
    ULONG           mem;

    CHAR            szBuffer[200];

    switch (msg)
    {

        // all answers to the WinDDEInitate call arrive here
        case WM_DDE_INITIATEACK:
        {
            PDDEINIT        pddeInit;
            PSZ             szInApp, szInTopic;
            static BOOL bNetscapeAnswered = FALSE;

            pddeInit = (PDDEINIT) mp2;
            szInApp = pddeInit->pszAppName;
            szInTopic = pddeInit->pszTopic;
            ShowMessage("!! Netscape answered.");
            hServerWnd = (HWND) mp1;

            // RDP 2000-07-07 07:24:18
            // There was no check on which application responded.
            // This made NETSCDDE fail when another DDE-aware application,
            // like EPM, was running.
            // Now the handle from mp1 is only assigned if the application
            // responding is Netscape.
            // If the app is not Netscape then the handle is nullified.
            // I don't know if assigning 0 to the handle is correct but
            // is seems to solve the problem.
            if (!strcmp(pddeInit->pszAppName, "NETSCAPE"))
            {
                ShowMessage("!! Netscape answered.");
                hServerWnd = (HWND)mp1;
                bNetscapeAnswered = TRUE;
            }
            else
            {
                ShowMessage("!! Other application aswered.");
                hServerWnd = (HWND)0;
            }

            // Show the application name and the topic in the debug-window.
            ShowMessage(pddeInit->pszAppName);
            ShowMessage(pddeInit->pszTopic);
        break; }

        // all answers to DDE requests arrive here
        case WM_DDE_DATA:
        {
            ShowMessage("!! Received data from Netscape: ");
            pddeStruct = (PDDESTRUCT) mp2;
            DosGetSharedMem(pddeStruct, PAG_READ | PAG_WRITE);
            szData = (BYTE *) (pddeStruct + (pddeStruct->offabData));
            ShowMessage(szData);
            break;
        }

        // menu item processing (in debug mode, otherwise these
        // WM_COMMAND msgs have been posted automatically)
        case WM_COMMAND:
            switch (SHORT1FROMMP(mp1))
            {
                // start DDE conversation: this was posted
                // by "main" before the PM loop was entered
                case IDM_INITIATE:
                    WinPostMsg(hwndListbox, LM_DELETEALL, 0, 0);
                    ShowMessage("--- Initiating DDE... Topic:");
                    ShowMessage(szOpenURLTopic);
                    context.cb = sizeof(CONVCONTEXT);
                    context.fsContext = 0;
                    WinDdeInitiate(hwndFrame, szDDENetscape,
                                   szOpenURLTopic, &context);
                    if (!optDebug)
                        // if we're not in debug mode, post subsequent
                        // menu commands automatically
                        WinPostMsg(hwndFrame, WM_COMMAND, MPFROM2SHORT(IDM_CHAIN2, 0), 0);
                break;

                // "Open URL": request data from server
                case IDM_OPENURL:
                {
                    ShowMessage("--- URL is:");
                    ShowMessage(szURL);
                    strcpy(szBuffer, szURL);
                    strcat(szBuffer, ",,0xFFFFFFFF,0x0");
                    ShowMessage("Sending request:");
                    ShowMessage(szBuffer);

                    if (DDERequest(hwndFrame, szBuffer))
                        ShowMessage("DDE Message sent.");
                    else
                        ShowMessage("DDE Message sending failed.");
                    break;
                }

                // "Open URL in new window": request data from server,
                // but with different parameters
                case IDM_OPENURLNEW:
                {
                    ShowMessage("--- URL is:");
                    ShowMessage(szURL);
                    strcpy(szBuffer, szURL);
                    strcat(szBuffer, ",,0x0,0x0");
                    ShowMessage("Sending request:");
                    ShowMessage(szBuffer);
                    if (DDERequest(hwndFrame, szBuffer))
                        ShowMessage("DDE Message sent.");
                    else
                        ShowMessage("DDE Message sending failed.");
                    break;
                }

                /*
                 * IDM_CHAIN2:
                 *      this is posted after DDE_INITIATE was
                 *      successful
                 */

                case IDM_CHAIN2:
                {
                    if (optNewWindow)
                        WinPostMsg(hwndDebug, WM_COMMAND, MPFROM2SHORT(IDM_OPENURLNEW, 0), 0);
                    else
                        WinPostMsg(hwndDebug, WM_COMMAND, MPFROM2SHORT(IDM_OPENURL, 0), 0);
                    WinPostMsg(hwndDebug, WM_COMMAND, MPFROM2SHORT(IDM_CHAIN3, 0), 0);
                }
                break;

                /*
                 * IDM_CHAIN3:
                 *      this is posted to close the whole thing; we just need
                 *      another msg before going for IDM_CLOSE, or some DDE
                 *      msgs might get lost
                 */

                case IDM_CHAIN3:
                    WinPostMsg(hwndDebug, WM_COMMAND, MPFROM2SHORT(IDM_CLOSE, 0), 0);
                break;

                /*
                 * IDM_CLOSE:
                 *      this is posted to close the whole thing
                 */

                case IDM_CLOSE:
                    WinDdePostMsg(hServerWnd,
                                  hwndFrame,
                                  WM_DDE_TERMINATE,
                                  NULL,
                                  DDEPM_RETRY);
                    ShowMessage("DDE connection closed.");

                    if (!optDebug)
                        WinPostMsg(hwndFrame, WM_COMMAND, MPFROM2SHORT(IDM_DELAYEXIT, 0), 0);
                break;

                /*
                 * IDM_DELAYEXIT:
                 *      this is posted after IDM_CLOSE; we will now
                 *      check for whether the DDE conversation with
                 *      Netscape was successful and, if not, start
                 *      a new instance of Netscape according to the
                 *      command line parameters
                 */

                case IDM_DELAYEXIT:
                {
                    if (    (!NetscapeFound)
                         && (optExecute)
                       )
                    {
                        // confirm start netscape
                        if (    (!optConfirmStart)
                             || (WinCenteredDlgBox(HWND_DESKTOP,
                                                   hwndDebug,
                                                   WinDefDlgProc,
                                                   hmodNLS,
                                                   ID_NDD_QUERYSTART,
                                                   NULL)
                                      == DID_OK)
                           )
                        {
                            STARTDATA       SData =
                            {0};
                            APIRET          rc = 0;
                            PID             pid = 0;    // PID returned

                            ULONG           ulSessID = 0;   // session ID returned

                            UCHAR           achObjBuf[256] =
                            {0};    // Error data if DosStart fails

                            CHAR            szArgs[CCHMAXPATH];
                            CHAR            szMsg[100];     // message

                            HWND            hwndNotify = HWND_DESKTOP;
                            PROGDETAILS     Details;
                            HAPP            happ;

                            // destroy "Contacting", create "Starting Netscape"
                            // window
                            if (hwndContacting)
                                WinDestroyWindow(hwndContacting);
                            if (!optQuiet)
                            {
                                hwndContacting = WinLoadDlg(HWND_DESKTOP, hwndDebug,
                                                            WinDefDlgProc,
                                                            hmodNLS, ID_NDD_STARTING,
                                                            0);
                                WinShowWindow(hwndContacting, TRUE);
                            }

                            // now start session
                            strcpy(szArgs, szNetscapeParams);
                            strcat(szArgs, " ");
                            strcat(szArgs, szURL);

                            SData.Length = sizeof(STARTDATA);
                            SData.Related = SSF_RELATED_INDEPENDENT;
                            SData.FgBg = SSF_FGBG_FORE;
                            SData.TraceOpt = SSF_TRACEOPT_NONE;

                            SData.PgmTitle = "Netscape";
                            SData.PgmName = szNetscapeApp;
                            SData.PgmInputs = szArgs;

                            SData.TermQ = 0;
                            SData.Environment = 0;
                            SData.InheritOpt = SSF_INHERTOPT_SHELL;
                            SData.SessionType = SSF_TYPE_DEFAULT;
                            SData.IconFile = 0;
                            SData.PgmHandle = 0;

                            SData.PgmControl = (optMinimized)
                                ? (SSF_CONTROL_MINIMIZE |
                                   ((optHidden) ? SSF_CONTROL_INVISIBLE : SSF_CONTROL_VISIBLE)
                                )
                                : SSF_CONTROL_VISIBLE;
                            SData.InitXPos = 30;
                            SData.InitYPos = 40;
                            SData.InitXSize = 200;
                            SData.InitYSize = 140;
                            SData.Reserved = 0;
                            SData.ObjectBuffer = achObjBuf;
                            SData.ObjectBuffLen = (ULONG) sizeof(achObjBuf);

                            rc = DosStartSession(&SData, &ulSessID, &pid);
                        }
                    }
                    // keep "Contacting" / "Starting" window visible for two seconds
                    idTimer = WinStartTimer(hab, hwndFrame, 1, 2000);
                    break;
                }

                // User closes the window
                case IDM_EXIT:
                    WinPostMsg(hwndFrame, WM_CLOSE, 0, 0);
                break;
            }
            break;

        case WM_TIMER:
            // after two seconds, close status window
            WinStopTimer(hab, hwndFrame, idTimer);
            WinPostMsg(hwndFrame, WM_CLOSE, 0, 0);
        break;

        // Send the message to the usual WC_FRAME WndProc
        default:
            return (*SysWndProc) (hwndFrame, msg, mp1, mp2);
    }

    return FALSE;
}

/*
 * ShowMessage:
 *      add a string to the listbox.
 */

void ShowMessage(PSZ szText)
{
    WinPostMsg(hwndListbox,
               LM_INSERTITEM,
               MPFROMSHORT(LIT_END),
               szText);
}
