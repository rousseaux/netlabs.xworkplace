
/*
 *@@sourcefile w_diskfree.c:
 *      XCenter "Disk Usage" widget.
 *
 *      This is all new with V0.9.9. Thanks to fonz for the
 *      contribution.
 *
 *@@added V0.9.9 (2001-02-28) [umoeller]
 *@@header "shared\center.h"
 */

#define INCL_DOSMODULEMGR
#define INCL_DOSERRORS

#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WINDIALOGS
#define INCL_WININPUT
#define INCL_WINSWITCHLIST
#define INCL_WINRECTANGLES
#define INCL_WINPOINTERS
#define INCL_WINSYS
#define INCL_WINLISTBOXES
#define INCL_WINENTRYFIELDS

#define INCL_GPIPRIMITIVES
#define INCL_GPILOGCOLORTABLE
#include <os2.h>

// C library headers
#include <stdio.h>
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\gpih.h"               // GPI helper routines
#include "helpers\timer.h"
#include "helpers\prfh.h"               // INI file helper routines;
                                        // this include is required for some
                                        // of the structures in shared\center.h
#include "helpers\winh.h"               // PM helper routines
#include "helpers\xstring.h"            // extended string helpers

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\center.h"              // public XCenter interfaces
#include "shared\common.h"              // the majestic XWorkplace include file

//#include "pmprintf.h"

#pragma hdrstop                     // VAC++ keeps crashing otherwise

/* ******************************************************************
 *
 *   Private definitions
 *
 ********************************************************************/

// None currently.

/* ******************************************************************
 *
 *   XCenter widget class definition
 *
 ********************************************************************/

/*
 *      This contains the name of the PM window class and
 *      the XCENTERWIDGETCLASS definition(s) for the widget
 *      class(es) in this DLL.
 */

#define WNDCLASS_WIDGET_SAMPLE "XWPCenterDiskfreeWidget"

static XCENTERWIDGETCLASS G_WidgetClasses[]
    = {
        WNDCLASS_WIDGET_SAMPLE,     // PM window class name
        0,                          // additional flag, not used here
        "DiskfreeWidget",           // internal widget class name
        "Disk Usage",               // widget class name displayed to user
        WGTF_UNIQUEPERXCENTER|WGTF_SIZEABLE,      // widget class flags
        NULL                        // no settings dialog
      };

/* ******************************************************************
 *
 *   Function imports from XFLDR.DLL
 *
 ********************************************************************/

/*
 *      To reduce the size of the widget DLL, it can
 *      be compiled with the VAC subsystem libraries.
 *      In addition, instead of linking frequently
 *      used helpers against the DLL again, you can
 *      import them from XFLDR.DLL, whose module handle
 *      is given to you in the INITMODULE export.
 *
 *      Note that importing functions from XFLDR.DLL
 *      is _not_ a requirement. We only do this to
 *      avoid duplicate code.
 *
 *      For each funtion that you need, add a global
 *      function pointer variable and an entry to
 *      the G_aImports array. These better match.
 *
 *      The actual imports are then made by WgtInitModule.
 */

// resolved function pointers from XFLDR.DLL
PCMNQUERYDEFAULTFONT pcmnQueryDefaultFont = NULL;
PCMNQUERYMAINRESMODULEHANDLE pcmnQueryMainResModuleHandle = NULL;

PTMRSTARTXTIMER ptmrStartXTimer = NULL;
PTMRSTOPXTIMER ptmrStopXTimer = NULL;

PCTRDISPLAYHELP pctrDisplayHelp = NULL;
PCTRFREESETUPVALUE pctrFreeSetupValue = NULL;
PCTRPARSECOLORSTRING pctrParseColorString = NULL;
PCTRSCANSETUPSTRING pctrScanSetupString = NULL;
PCTRSETSETUPSTRING pctrSetSetupString = NULL;

PGPIHDRAW3DFRAME pgpihDraw3DFrame = NULL;
PGPIHSWITCHTORGB pgpihSwitchToRGB = NULL;

PWINHFREE pwinhFree = NULL;
PWINHQUERYPRESCOLOR pwinhQueryPresColor = NULL;
PWINHQUERYWINDOWFONT pwinhQueryWindowFont = NULL;
PWINHSETWINDOWFONT pwinhSetWindowFont = NULL;

PXSTRCAT pxstrcat = NULL;
PXSTRCLEAR pxstrClear = NULL;
PXSTRINIT pxstrInit = NULL;

RESOLVEFUNCTION G_aImports[] =
    {
        "cmnQueryDefaultFont", (PFN*)&pcmnQueryDefaultFont,
        "cmnQueryMainResModuleHandle", (PFN*)&pcmnQueryMainResModuleHandle,

        "tmrStartXTimer", (PFN*)&ptmrStartXTimer,
        "tmrStopXTimer", (PFN*)&ptmrStopXTimer,

        "ctrDisplayHelp", (PFN*)&pctrDisplayHelp,
        "ctrFreeSetupValue", (PFN*)&pctrFreeSetupValue,
        "ctrParseColorString", (PFN*)&pctrParseColorString,
        "ctrScanSetupString", (PFN*)&pctrScanSetupString,
        "ctrSetSetupString", (PFN*)&pctrSetSetupString,

        "gpihDraw3DFrame", (PFN*)&pgpihDraw3DFrame,
        "gpihSwitchToRGB", (PFN*)&pgpihSwitchToRGB,

        "winhFree", (PFN*)&pwinhFree,
        "winhQueryPresColor", (PFN*)&pwinhQueryPresColor,
        "winhQueryWindowFont", (PFN*)&pwinhQueryWindowFont,
        "winhSetWindowFont", (PFN*)&pwinhSetWindowFont,

        "xstrcat", (PFN*)&pxstrcat,
        "xstrClear", (PFN*)&pxstrClear,
        "xstrInit", (PFN*)&pxstrInit
    };

/* ******************************************************************
 *
 *   Other global variables
 *
 ********************************************************************/

HPOINTER G_hptrHand = NULLHANDLE;
HPOINTER G_hptrDrive = NULLHANDLE;

/* ******************************************************************
 *
 *   Private widget instance data
 *
 ********************************************************************/

/*
 *@@ SAMPLESETUP:
 *      instance data to which setup strings correspond.
 *      This is also a member of DISKFREEPRIVATE.
 *
 *      Putting these settings into a separate structure
 *      is no requirement, but comes in handy if you
 *      want to use the same setup string routines on
 *      both the open widget window and a settings dialog.
 */

typedef struct _SAMPLESETUP
{
    LONG        lcolBackground,         // background color
                lcolForeground;         // foreground color (for text)

    PSZ         pszFont;
            // if != NULL, non-default font (in "8.Helv" format);
            // this has been allocated using local malloc()!
} SAMPLESETUP, *PSAMPLESETUP;

/*
 *@@ DISKFREEPRIVATE:
 *      more window data for the widget.
 *
 *      An instance of this is created on WM_CREATE in
 *      fnwpSampleWidget and stored in XCENTERWIDGET.pUser.
 */

typedef struct _DISKFREEPRIVATE
{
    PXCENTERWIDGET pWidget;
            // reverse ptr to general widget data ptr; we need
            // that all the time and don't want to pass it on
            // the stack with each function call

    SAMPLESETUP Setup;
            // widget settings that correspond to a setup string

    char     szDrives[27];

    char     *pchAktDrive;
    char     szAktDriveType[12];
    double   dAktDriveFree;
    double   dAktDriveSize;

    ULONG    ulTimerID;

} DISKFREEPRIVATE, *PDISKFREEPRIVATE;


// not defined in the toolkit-headers
BOOL APIENTRY WinStretchPointer(HPS hps, long lX, long ly, long lcy, long lcx, HPOINTER hptr, ULONG ulHalf);

// prototypes
VOID ScanSwitchList(PDISKFREEPRIVATE pPrivate);

BOOL GetDriveInfo(PDISKFREEPRIVATE pPrivate);

void GetDrive(HWND hwnd,
              PXCENTERWIDGET pWidget,
              BOOL fNext); // if fNext=FALSE, ut returns the prev. drive

/* ******************************************************************
 *
 *   Widget setup management
 *
 ********************************************************************/

/*
 *      This section contains shared code to manage the
 *      widget's settings. This can translate a widget
 *      setup string into the fields of a binary setup
 *      structure and vice versa. This code is used by
 *      an open widget window, but could be shared with
 *      a settings dialog, if you implement one.
 */

/*
 *@@ WgtClearSetup:
 *      cleans up the data in the specified setup
 *      structure, but does not free the structure
 *      itself.
 */

VOID WgtClearSetup(PSAMPLESETUP pSetup)
{
    if (pSetup)
    {
        if (pSetup->pszFont)
        {
            free(pSetup->pszFont);
            pSetup->pszFont = NULL;
        }
    }
}

/*
 *@@ WgtScanSetup:
 *      scans the given setup string and translates
 *      its data into the specified binary setup
 *      structure.
 *
 *      NOTE: It is assumed that pSetup is zeroed
 *      out. We do not clean up previous data here.
 */

VOID WgtScanSetup(const char *pcszSetupString,
                  PSAMPLESETUP pSetup)
{
    PSZ p;

    // background color
    p = pctrScanSetupString(pcszSetupString,
                            "BGNDCOL");
    if (p)
    {
        pSetup->lcolBackground = pctrParseColorString(p);
        pctrFreeSetupValue(p);
    }
    else
        // default color:
        pSetup->lcolBackground = WinQuerySysColor(HWND_DESKTOP, SYSCLR_DIALOGBACKGROUND, 0);

    // text color:
    p = pctrScanSetupString(pcszSetupString,
                            "TEXTCOL");
    if (p)
    {
        pSetup->lcolForeground = pctrParseColorString(p);
        pctrFreeSetupValue(p);
    }
    else
        pSetup->lcolForeground = WinQuerySysColor(HWND_DESKTOP, SYSCLR_WINDOWSTATICTEXT, 0);

    // font:
    // we set the font presparam, which automatically
    // affects the cached presentation spaces
    p = pctrScanSetupString(pcszSetupString,
                            "FONT");
    if (p)
    {
        pSetup->pszFont = strdup(p);
        pctrFreeSetupValue(p);
    }
    // else: leave this field null
}

/*
 *@@ WgtSaveSetup:
 *      composes a new setup string.
 *      The caller must invoke xstrClear on the
 *      string after use.
 */

VOID WgtSaveSetup(PXSTRING pstrSetup,       // out: setup string (is cleared first)
                  PSAMPLESETUP pSetup)
{
    CHAR    szTemp[100];
    PSZ     psz = 0;
    pxstrInit(pstrSetup, 100);

    sprintf(szTemp, "BGNDCOL=%06lX;",
            pSetup->lcolBackground);
    pxstrcat(pstrSetup, szTemp, 0);

    sprintf(szTemp, "TEXTCOL=%06lX;",
            pSetup->lcolForeground);
    pxstrcat(pstrSetup, szTemp, 0);

    if (pSetup->pszFont)
    {
        // non-default font:
        sprintf(szTemp, "FONT=%s;",
                pSetup->pszFont);
        pxstrcat(pstrSetup, szTemp, 0);
    }
}

/* ******************************************************************
 *
 *   Widget settings dialog
 *
 ********************************************************************/

// None currently. To see how a setup dialog can be done,
// see the window list widget (w_winlist.c).

/* ******************************************************************
 *
 *   Callbacks stored in XCENTERWIDGETCLASS
 *
 ********************************************************************/

// If you implement a settings dialog, you must write a
// "show settings dlg" function and store its function pointer
// in XCENTERWIDGETCLASS.

/* ******************************************************************
 *
 *   PM window class implementation
 *
 ********************************************************************/

/*
 *      This code has the actual PM window class.
 *
 */

/*
 *@@ WgtCreate:
 *      implementation for WM_CREATE.
 */

MRESULT WgtCreate(HWND hwnd,
                  PXCENTERWIDGET pWidget)
{
    MRESULT mrc = 0;
    PSZ p;
    PDISKFREEPRIVATE pPrivate = (PDISKFREEPRIVATE)malloc(sizeof(DISKFREEPRIVATE));
    memset(pPrivate, 0, sizeof(DISKFREEPRIVATE));
    // link the two together
    pWidget->pUser = pPrivate;
    pPrivate->pWidget = pWidget;

    // initialize binary setup structure from setup string
    WgtScanSetup(pWidget->pcszSetupString,
                 &pPrivate->Setup);

    // set window font (this affects all the cached presentation
    // spaces we use)
    pwinhSetWindowFont(hwnd,
                       (pPrivate->Setup.pszFont)
                        ? pPrivate->Setup.pszFont
                        // default font: use the same as in the rest of XWorkplace:
                        : pcmnQueryDefaultFont());

    // if you want the context menu help to be enabled,
    // add your help library here; if these fields are
    // left NULL, the "Help" context menu item is disabled

    // pWidget->pcszHelpLibrary = pcmnQueryHelpLibrary();
    // pWidget->ulHelpPanelID = ID_XSH_WIDGET_WINLIST_MAIN;

    doshEnumDrives((PSZ)pPrivate->szDrives,
                   NULL,
                   TRUE);

    pPrivate->pchAktDrive=(char *)pPrivate->szDrives;
    GetDriveInfo(pPrivate);


    // start update timer
    pPrivate->ulTimerID = ptmrStartXTimer(pWidget->pGlobals->pvXTimerSet,
                                          hwnd,
                                          1,
                                          5000);

    return (mrc);
}

/*
 *@@ MwgtControl:
 *      implementation for WM_CONTROL.
 *
 *      The XCenter communicates with widgets thru
 *      WM_CONTROL messages. At the very least, the
 *      widget should respond to XN_QUERYSIZE because
 *      otherwise it will be given some dumb default
 *      size.
 *
 *@@added V0.9.7 (2000-12-14) [umoeller]
 */

BOOL WgtControl(HWND hwnd, MPARAM mp1, MPARAM mp2)
{
    BOOL brc = FALSE;

    // get widget data from QWL_USER (stored there by WM_CREATE)
    PXCENTERWIDGET pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER);
    if (pWidget)
    {
        // get private data from that widget data
        PDISKFREEPRIVATE pPrivate = (PDISKFREEPRIVATE)pWidget->pUser;
        if (pPrivate)
        {
            USHORT  usID = SHORT1FROMMP(mp1),
                    usNotifyCode = SHORT2FROMMP(mp1);

            // is this from the XCenter client?
            if (usID == ID_XCENTER_CLIENT)
            {
                // yes:

                switch (usNotifyCode)
                {
                    /*
                     * XN_QUERYSIZE:
                     *      XCenter wants to know our size.
                     */

                    case XN_QUERYSIZE:
                    {
                        PSIZEL pszl = (PSIZEL)mp2;
                        pszl->cx = 210;      // desired width
                        pszl->cy = 20;      // desired minimum height
                        brc = TRUE;
                    break; }

                    /*
                     * XN_SETUPCHANGED:
                     *      XCenter has a new setup string for
                     *      us in mp2.
                     *
                     *      NOTE: This only comes in with settings
                     *      dialogs. Since we don't have one, this
                     *      really isn't necessary.
                     */

                    case XN_SETUPCHANGED:
                    {
                        const char *pcszNewSetupString = (const char*)mp2;

                        // reinitialize the setup data
                        WgtClearSetup(&pPrivate->Setup);
                        WgtScanSetup(pcszNewSetupString, &pPrivate->Setup);

                        WinInvalidateRect(pWidget->hwndWidget, NULL, FALSE);
                    break; }
                }
            }
        } // end if (pPrivate)
    } // end if (pWidget)

    return (brc);
}

/*
 *@@ WgtPaint:
 *      implementation for WM_PAINT.
 *
 *      This really does nothing, except painting a
 *      3D rectangle and printing a question mark.
 */

VOID WgtPaint(HWND hwnd,
              PXCENTERWIDGET pWidget)
{
    HPS hps = WinBeginPaint(hwnd, NULLHANDLE, NULL);
    if (hps)
    {
        PDISKFREEPRIVATE pPrivate = (PDISKFREEPRIVATE)pWidget->pUser;
        if (pPrivate)
        {
            RECTL       rclWin;
            char        szText[64];
            double      dPercentFree=0;


            // now paint frame
            WinQueryWindowRect(hwnd,
                               &rclWin);        // exclusive
            pgpihSwitchToRGB(hps);

            // make this rectangle inclusive
            WinFillRect(hps,
                        &rclWin,                // exclusive
                        pPrivate->Setup.lcolBackground);


            // draw drive-icon
            WinStretchPointer(hps,
                              rclWin.xLeft+3,
                              (rclWin.yTop-rclWin.yBottom-11)/2+1,
                              21,
                              11,
                              G_hptrDrive,
                              DP_NORMAL);
                                                         // pPrivate->dAktDriveSize/1024/1024...100%
            // print drive-data                             pPrivate->dAktDriveFree/1024/1024...x%
            dPercentFree=pPrivate->dAktDriveFree*100/pPrivate->dAktDriveSize;

            sprintf(szText, "%c: (%s)  %.fMB (%.f%%) free", *pPrivate->pchAktDrive,
                                                            pPrivate->szAktDriveType,
                                                            pPrivate->dAktDriveFree/1024/1024,
                                                            dPercentFree);

            rclWin.xLeft+=30;
            WinDrawText(hps,
                        strlen(szText),
                        szText,
                        &rclWin,                // exclusive
                        pPrivate->Setup.lcolForeground,
                        pPrivate->Setup.lcolBackground,
                        DT_LEFT| DT_VCENTER);
        }
        WinEndPaint(hps);
    }
}

/*
 *@@ WgtPresParamChanged:
 *      implementation for WM_PRESPARAMCHANGED.
 *
 *      While this isn't exactly required, it's a nice
 *      thing for a widget to react to colors and fonts
 *      dropped on it. While we're at it, we also save
 *      these colors and fonts in our setup string data.
 */

VOID WgtPresParamChanged(HWND hwnd,
                         ULONG ulAttrChanged,
                         PXCENTERWIDGET pWidget)
{
    PDISKFREEPRIVATE pPrivate = (PDISKFREEPRIVATE)pWidget->pUser;
    if (pPrivate)
    {
        BOOL fInvalidate = TRUE;
        switch (ulAttrChanged)
        {
            case 0:     // layout palette thing dropped
            case PP_BACKGROUNDCOLOR:    // background color (no ctrl pressed)
            case PP_FOREGROUNDCOLOR:    // foreground color (ctrl pressed)
                // update our setup data; the presparam has already
                // been changed, so we can just query it
                pPrivate->Setup.lcolBackground
                    = pwinhQueryPresColor(hwnd,
                                          PP_BACKGROUNDCOLOR,
                                          FALSE,
                                          SYSCLR_DIALOGBACKGROUND);
                pPrivate->Setup.lcolForeground
                    = pwinhQueryPresColor(hwnd,
                                          PP_FOREGROUNDCOLOR,
                                          FALSE,
                                          SYSCLR_WINDOWSTATICTEXT);
            break;

            case PP_FONTNAMESIZE:       // font dropped:
            {
                PSZ pszFont = 0;
                if (pPrivate->Setup.pszFont)
                {
                    free(pPrivate->Setup.pszFont);
                    pPrivate->Setup.pszFont = NULL;
                }

                pszFont = pwinhQueryWindowFont(hwnd);
                if (pszFont)
                {
                    // we must use local malloc() for the font;
                    // the winh* code uses a different C runtime
                    pPrivate->Setup.pszFont = strdup(pszFont);
                    pwinhFree(pszFont);
                }
            break; }

            default:
                fInvalidate = FALSE;
        }

        if (fInvalidate)
        {
            // something has changed:
            XSTRING strSetup;

            // repaint
            WinInvalidateRect(hwnd, NULL, FALSE);

            // recompose our setup string
            WgtSaveSetup(&strSetup,
                         &pPrivate->Setup);
            if (strSetup.ulLength)
                // we have a setup string:
                // tell the XCenter to save it with the XCenter data
                WinSendMsg(pPrivate->pWidget->pGlobals->hwndClient,
                           XCM_SAVESETUP,
                           (MPARAM)hwnd,
                           (MPARAM)strSetup.psz);
            pxstrClear(&strSetup);
        }
    } // end if (pPrivate)
}

APIRET _doshQueryDiskSize(ULONG ulLogicalDrive, // in: 1 for A:, 2 for B:, 3 for C:, ...
                          double *pdSize)
{
    APIRET      arc = NO_ERROR;
    FSALLOCATE  fsa;
    double      dbl = -1;

    arc = DosQueryFSInfo(ulLogicalDrive, FSIL_ALLOC, &fsa, sizeof(fsa));
    if (arc == NO_ERROR)
        *pdSize = ((double)fsa.cSectorUnit * fsa.cbSector * fsa.cUnit);

    return (arc);
}

/*
 *@@ GetDriveInfo:
 *
 */

BOOL GetDriveInfo(PDISKFREEPRIVATE pPrivate)
{
    double dOldDriveFree=pPrivate->dAktDriveFree;


    doshQueryDiskFSType(*pPrivate->pchAktDrive-64,
                        (PSZ)pPrivate->szAktDriveType,
                        sizeof(pPrivate->szAktDriveType));


    doshQueryDiskFree(*pPrivate->pchAktDrive-64,
                      &pPrivate->dAktDriveFree);

    _doshQueryDiskSize(*pPrivate->pchAktDrive-64,
                       &pPrivate->dAktDriveSize);

    return((BOOL)pPrivate->dAktDriveFree!=dOldDriveFree);
}

/*
 *@@ GetDrive:
 *
 */

void GetDrive(HWND hwnd,
              PXCENTERWIDGET pWidget,
              BOOL fNext)
{
    PDISKFREEPRIVATE pPrivate = (PDISKFREEPRIVATE)pWidget->pUser;

    if (pPrivate)
    {
        if (fNext)
        {
            // return the next drive
            pPrivate->pchAktDrive++;
            if (*pPrivate->pchAktDrive==0)
                pPrivate->pchAktDrive=(char *)pPrivate->szDrives;
        }
        else
        {
            // return the prev drive
            if (pPrivate->pchAktDrive==(char *)pPrivate->szDrives)
                pPrivate->pchAktDrive=(char *)pPrivate->szDrives+strlen(pPrivate->szDrives)-1    ;
            else
                pPrivate->pchAktDrive--;
        }


        GetDriveInfo(pPrivate);

        WinInvalidateRect(hwnd,
                          NULLHANDLE,
                          TRUE);
     }
}

/*
 *@@ WgtDestroy:
 *      implementation for WM_DESTROY.
 *
 *      This must clean up all allocated resources.
 */

VOID WgtDestroy(HWND hwnd,
                PXCENTERWIDGET pWidget)
{
    PDISKFREEPRIVATE pPrivate = (PDISKFREEPRIVATE)pWidget->pUser;
    if (pPrivate)
    {
        if (pPrivate->ulTimerID)
           ptmrStopXTimer(pWidget->pGlobals->pvXTimerSet,
                          hwnd,
                          pPrivate->ulTimerID);

        WgtClearSetup(&pPrivate->Setup);

        free(pPrivate);
                // pWidget is cleaned up by DestroyWidgets
    }
}

/*
 *@@ fnwpSampleWidget:
 *      window procedure for the winlist widget class.
 *
 *      There are a few rules which widget window procs
 *      must follow. See ctrDefWidgetProc in src\shared\center.c
 *      for details.
 *
 *      Other than that, this is a regular window procedure
 *      which follows the basic rules for a PM window class.
 */

MRESULT EXPENTRY fnwpSampleWidget(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
    MRESULT mrc = 0;
    // get widget data from QWL_USER (stored there by WM_CREATE)
    PXCENTERWIDGET pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER);
                    // this ptr is valid after WM_CREATE

    switch (msg)
    {
        /*
         * WM_CREATE:
         *      as with all widgets, we receive a pointer to the
         *      XCENTERWIDGET in mp1, which was created for us.
         *
         *      The first thing the widget MUST do on WM_CREATE
         *      is to store the XCENTERWIDGET pointer (from mp1)
         *      in the QWL_USER window word by calling:
         *
         *          WinSetWindowPtr(hwnd, QWL_USER, mp1);
         *
         *      We use XCENTERWIDGET.pUser for allocating
         *      DISKFREEPRIVATE for our own stuff.
         */

        case WM_CREATE:
            WinSetWindowPtr(hwnd, QWL_USER, mp1);
            pWidget = (PXCENTERWIDGET)mp1;
            if ((pWidget) && (pWidget->pfnwpDefWidgetProc))
                mrc = WgtCreate(hwnd, pWidget);
            else
                // stop window creation!!
                mrc = (MPARAM)TRUE;
        break;

        /*
         * WM_CONTROL:
         *      process notifications/queries from the XCenter.
         */

        case WM_CONTROL:
            mrc = (MPARAM)WgtControl(hwnd, mp1, mp2);
        break;

        case WM_MOUSEMOVE:
            WinSetPointer(HWND_DESKTOP, G_hptrHand);
        break;

        case WM_BUTTON1UP:
            if ((long)WinGetKeyState(HWND_DESKTOP, VK_CTRL) & 0x8000)
                GetDrive(hwnd, pWidget, FALSE);
            else
                GetDrive(hwnd, pWidget, TRUE);
        break;

        /*
         * WM_PAINT:
         *
         */

        case WM_PAINT:
            WgtPaint(hwnd, pWidget);
        break;

        case WM_TIMER:
        {
            if(pWidget)
            {
                // get private data from that widget data
                PDISKFREEPRIVATE pPrivate = (PDISKFREEPRIVATE)pWidget->pUser;

                if (GetDriveInfo(pPrivate)) // if values have changed update, display
                    WinInvalidateRect(hwnd, NULLHANDLE, TRUE);
            }
        break; }

        /*
         * WM_PRESPARAMCHANGED:
         *
         */

        case WM_PRESPARAMCHANGED:
            if (pWidget)
                // this gets sent before this is set!
                WgtPresParamChanged(hwnd, (ULONG)mp1, pWidget);
        break;

        /*
         * WM_DESTROY:
         *      clean up. This _must_ be passed on to
         *      ctrDefWidgetProc.
         */

        case WM_DESTROY:
            WgtDestroy(hwnd, pWidget);
            // we _MUST_ pass this on, or the default widget proc
            // cannot clean up.
            mrc = pWidget->pfnwpDefWidgetProc(hwnd, msg, mp1, mp2);
        break;

        default:
            mrc = pWidget->pfnwpDefWidgetProc(hwnd, msg, mp1, mp2);
    } // end switch(msg)

    return (mrc);
}

/* ******************************************************************
 *
 *   Exported procedures
 *
 ********************************************************************/

/*
 *@@ WgtInitModule:
 *      required export with ordinal 1, which must tell
 *      the XCenter how many widgets this DLL provides,
 *      and give the XCenter an array of XCENTERWIDGETCLASS
 *      structures describing the widgets.
 *
 *      With this call, you are given the module handle of
 *      XFLDR.DLL. For convenience, you may resolve imports
 *      for some useful functions which are exported thru
 *      src\shared\xwp.def. See the code below.
 *
 *      This function must also register the PM window classes
 *      which are specified in the XCENTERWIDGETCLASS array
 *      entries. For this, you are given a HAB which you
 *      should pass to WinRegisterClass. For the window
 *      class style (4th param to WinRegisterClass),
 *      you should specify
 *
 +          CS_PARENTCLIP | CS_SIZEREDRAW | CS_SYNCPAINT
 *
 *      Your widget window _will_ be resized, even if you're
 *      not planning it to be.
 *
 *      This function only gets called _once_ when the widget
 *      DLL has been successfully loaded by the XCenter. If
 *      there are several instances of a widget running (in
 *      the same or in several XCenters), this function does
 *      not get called again. However, since the XCenter unloads
 *      the widget DLLs again if they are no longer referenced
 *      by any XCenter, this might get called again when the
 *      DLL is re-loaded.
 *
 *      There will ever be only one load occurence of the DLL.
 *      The XCenter manages sharing the DLL between several
 *      XCenters. As a result, it doesn't matter if the DLL
 *      has INITINSTANCE etc. set or not.
 *
 *      If this returns 0, this is considered an error, and the
 *      DLL will be unloaded again immediately.
 *
 *      If this returns any value > 0, *ppaClasses must be
 *      set to a static array (best placed in the DLL's
 *      global data) of XCENTERWIDGETCLASS structures,
 *      which must have as many entries as the return value.
 */

ULONG EXPENTRY WgtInitModule(HAB hab,         // XCenter's anchor block
                             HMODULE hmodXFLDR,    // XFLDR.DLL module handle
                             PXCENTERWIDGETCLASS *ppaClasses,
                             PSZ pszErrorMsg)  // if 0 is returned, 500 bytes of error msg
{
    ULONG   ulrc = 0,
            ul = 0;
    BOOL    fImportsFailed = FALSE;

    // resolve imports from XFLDR.DLL (this is basically
    // a copy of the doshResolveImports code, but we can't
    // use that before resolving...)
    for (ul = 0;
         ul < sizeof(G_aImports) / sizeof(G_aImports[0]);
         ul++)
    {
        if (DosQueryProcAddr(hmodXFLDR,
                             0,               // ordinal, ignored
                             (PSZ)G_aImports[ul].pcszFunctionName,
                             G_aImports[ul].ppFuncAddress)
                    != NO_ERROR)
        {
            sprintf(pszErrorMsg,
                    "Import %s failed.",
                    G_aImports[ul].pcszFunctionName);
            fImportsFailed = TRUE;
            break;
        }
    }

    if (!fImportsFailed)
    {
        HMODULE hmodRes = pcmnQueryMainResModuleHandle();
        G_hptrDrive = WinLoadPointer(HWND_DESKTOP,
                                     hmodRes,
                                     ID_ICON_DRIVE);
        G_hptrHand  = WinLoadPointer(HWND_DESKTOP,
                                     hmodRes,
                                     ID_POINTER_HAND);

        if (!G_hptrDrive || !G_hptrHand)
            strcpy(pszErrorMsg, "Cannot load icons from XWPRES.DLL.");
        else
        {
            // all imports OK:
            // register our PM window class
            if (!WinRegisterClass(hab,
                                  WNDCLASS_WIDGET_SAMPLE,
                                  fnwpSampleWidget,
                                  CS_PARENTCLIP | CS_SIZEREDRAW | CS_SYNCPAINT,
                                  sizeof(PDISKFREEPRIVATE))
                                        // extra memory to reserve for QWL_USER
                                 )
                strcpy(pszErrorMsg, "WinRegisterClass failed.");
            else
            {
                // no error:
                // return widget classes
                *ppaClasses = G_WidgetClasses;

                // return no. of classes in this DLL (one here):
                ulrc = sizeof(G_WidgetClasses) / sizeof(G_WidgetClasses[0]);
            }
        }
    }

    return (ulrc);
}

/*
 *@@ WgtUnInitModule:
 *      optional export with ordinal 2, which can clean
 *      up global widget class data.
 *
 *      This gets called by the XCenter right before
 *      a widget DLL gets unloaded. Note that this
 *      gets called even if the "init module" export
 *      returned 0 (meaning an error) and the DLL
 *      gets unloaded right away.
 */

VOID EXPENTRY WgtUnInitModule(VOID)
{
    WinDestroyPointer(G_hptrDrive);
    WinDestroyPointer(G_hptrHand);
}


