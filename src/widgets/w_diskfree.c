
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

#define INCL_WIN
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
#define DONT_REPLACE_MALLOC         // in case mem debug is enabled
#include "setup.h"                      // code generation and debugging options

// disable wrappers, because we're not linking statically
#ifdef WINH_STANDARDWRAPPERS
    #undef WINH_STANDARDWRAPPERS
#endif

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

#define DISKFREE_SHOW_FS        0x01

#define WNDCLASS_WIDGET_SAMPLE "XWPCenterDiskfreeWidget"

void EXPENTRY WgtShowSettingsDlg(PWIDGETSETTINGSDLGDATA pData);

static XCENTERWIDGETCLASS G_WidgetClasses[]
    = {
          WNDCLASS_WIDGET_SAMPLE,     // PM window class name
          0,                          // additional flag, not used here
          "DiskfreeWidget",           // internal widget class name
          "Diskfree",                 // widget class name displayed to user
          WGTF_TRAYABLE,
          WgtShowSettingsDlg          // our settings dialog
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
PCMNQUERYNLSMODULEHANDLE pcmnQueryNLSModuleHandle = NULL;
PCMNSETCONTROLSFONT pcmnSetControlsFont = NULL;

PDOSHENUMDRIVES pdoshEnumDrives = NULL;
PDOSHQUERYDISKSIZE pdoshQueryDiskSize = NULL;
PDOSHQUERYDISKFREE pdoshQueryDiskFree = NULL;
PDOSHQUERYDISKFSTYPE pdoshQueryDiskFSType = NULL;

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
PWINHCENTERWINDOW pwinhCenterWindow = NULL;

PXSTRCAT pxstrcat = NULL;
PXSTRCLEAR pxstrClear = NULL;
PXSTRINIT pxstrInit = NULL;

RESOLVEFUNCTION G_aImports[] =
    {
        "cmnQueryDefaultFont", (PFN*)&pcmnQueryDefaultFont,
        "cmnQueryMainResModuleHandle", (PFN*)&pcmnQueryMainResModuleHandle,
        "cmnQueryNLSModuleHandle", (PFN*)&pcmnQueryNLSModuleHandle,
        "cmnSetControlsFont", (PFN*)&pcmnSetControlsFont,

        "doshEnumDrives", (PFN*)&pdoshEnumDrives,
        "doshQueryDiskSize", (PFN*)&pdoshQueryDiskSize,
        "doshQueryDiskFree", (PFN*)&pdoshQueryDiskFree,
        "doshQueryDiskFSType", (PFN*)&pdoshQueryDiskFSType,

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
        "winhCenterWindow", (PFN*)&pwinhCenterWindow,

        "xstrcat", (PFN*)&pxstrcat,
        "xstrClear", (PFN*)&pxstrClear,
        "xstrInit", (PFN*)&pxstrInit
    };

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
    long        lcolBackground,         // background color
                lcolForeground;         // foreground color (for text)

    PSZ         pszFont;
            // if != NULL, non-default font (in "8.Helv" format);
            // this has been allocated using local malloc()!

    char        chDrive;      // if ch=='*' we are in 'multi-view'
    long        lShow;        // eg FILETYPE
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


    HPOINTER hptrHand;
    HPOINTER hptrDrive;
    HPOINTER hptrDrives[3];

    char     szDrives[27];
    BYTE     bFSIcon;
    long     lCX;

    char     chAktDrive;
    char     szAktDriveType[12];
    double   dAktDriveFree;
    double   dAktDriveSize;

    ULONG    ulTimerID;

} DISKFREEPRIVATE, *PDISKFREEPRIVATE;


// not defined in the toolkit-headers
BOOL APIENTRY WinStretchPointer(HPS hps,
                                long lX,
                                long ly,
                                long lcy,
                                long lcx,
                                HPOINTER hptr,
                                ULONG ulHalf);

// prototypes
void ScanSwitchList(PDISKFREEPRIVATE pPrivate);

BOOL GetDriveInfo(PDISKFREEPRIVATE pPrivate);

void GetDrive(HWND hwnd,
              PXCENTERWIDGET pWidget,
              BOOL fNext); // if fNext=FALSE, ut returns the prev. drive

CHAR ValidateDrive(CHAR chDrive);

MRESULT EXPENTRY fnwpSettingsDlg(HWND hwnd,
                                 ULONG msg,
                                 MPARAM mp1,
                                 MPARAM mp2);

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

void WgtClearSetup(PSAMPLESETUP pSetup)
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

void WgtScanSetup(const char *pcszSetupString,
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



    ////////////////////////////////////////////////////////////////

    // view:  *..multi-view | otherwise..single-view where setup-string is drive-letter
    p = pctrScanSetupString(pcszSetupString,
                            "VIEW");
    if(p)
    {
        pSetup->chDrive = ValidateDrive(*p);  // V0.9.11 (2001-04-19) [pr]: Validate drive letter
        pctrFreeSetupValue(p);
    }
    else
        pSetup->chDrive = '*';


    // different 'show-styles'
    p = pctrScanSetupString(pcszSetupString,
                            "SHOW");
    if(p)
    {
        pSetup->lShow = atol(p);
        pctrFreeSetupValue(p);
    }
    else
        pSetup->lShow = 0;
}

/*
 *@@ WgtSaveSetup:
 *      composes a new setup string.
 *      The caller must invoke xstrClear on the
 *      string after use.
 */

void WgtSaveSetup(PXSTRING pstrSetup,       // out: setup string (is cleared first)
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


   ////////////////////////////////////////////////////////////////

   // view:  *..multi-view | otherwise..single-view where setup-string is drive-letter
   sprintf(szTemp, "VIEW=%c;",
           pSetup->chDrive);
   pxstrcat(pstrSetup, szTemp, 0);

   // different 'show-styles'
   sprintf(szTemp, "SHOW=%02d;",
           pSetup->lShow);
   pxstrcat(pstrSetup, szTemp, 0);
}

/* ******************************************************************
 *
 *   Widget settings dialog
 *
 ********************************************************************/

#define WMXINT_SETUP    WM_USER+1805

/*
 *@@ fnwpSettingsDlg:
 *      dialog proc for the winlist settings dialog.
 */

MRESULT EXPENTRY fnwpSettingsDlg(HWND hwnd,
                                 ULONG msg,
                                 MPARAM mp1,
                                 MPARAM mp2)
{
    MRESULT mrc = 0;
    static PWIDGETSETTINGSDLGDATA pData;


    switch(msg)
    {
        case WM_INITDLG:
        {
            pData=(PWIDGETSETTINGSDLGDATA)mp2;
            WinPostMsg(hwnd, WMXINT_SETUP, (MPARAM)0, (MPARAM)0); // otherwise all auto(radio)controls are resetted??
        break; }

        case WMXINT_SETUP:
        {
            // setup-string-handling
            PSAMPLESETUP pSetup=(PSAMPLESETUP)malloc(sizeof(SAMPLESETUP));

            // set max.length of entryfield to 1
            WinSendDlgItemMsg(hwnd, 106,
                              EM_SETTEXTLIMIT,
                              MPFROMSHORT(1),
                              (MPARAM)0);

            if(pSetup)
            {
                memset(pSetup, 0, sizeof(SAMPLESETUP));
                // store this in WIDGETSETTINGSDLGDATA
                pData->pUser = pSetup;

                WgtScanSetup(pData->pcszSetupString, pSetup);

                if(pSetup->chDrive=='*')
                    WinSendDlgItemMsg(hwnd, 101,
                                      BM_CLICK,
                                      MPFROMSHORT(TRUE),
                                      (MPARAM)0);
                else
                {
                    char sz[2];

                    WinSendDlgItemMsg(hwnd, 102,
                                      BM_CLICK,
                                      MPFROMSHORT(TRUE),
                                      (MPARAM)0);

                    sz[0]=pSetup->chDrive;
                    sz[1]='\0';
                    WinSetDlgItemText(hwnd, 106, sz);
                }

                if(pSetup->lShow & DISKFREE_SHOW_FS)
                    WinCheckButton(hwnd, 107, 1);
                else
                    WinCheckButton(hwnd, 107, 0);
            }
        break; }

        case WM_DESTROY:
        {
            if(pData)
            {
                PSAMPLESETUP pSetup = (PSAMPLESETUP)pData->pUser;
                if(pSetup)
                {
                    WgtClearSetup(pSetup);
                    free(pSetup);
                } // end if (pSetup)
             } // end if (pData)
        break; }


        case WM_CONTROL:
        {
            if(SHORT2FROMMP(mp1)==BN_CLICKED)
            {
                if(SHORT1FROMMP(mp1)==101) // multi-view
                {
                    // disable groupbox+children
                    WinEnableWindow(WinWindowFromID(hwnd, 104), FALSE);
                    WinEnableWindow(WinWindowFromID(hwnd, 105), FALSE);
                    WinEnableWindow(WinWindowFromID(hwnd, 106), FALSE);
                }
                else if(SHORT1FROMMP(mp1)==102) // single-view
                {
                    WinEnableWindow(WinWindowFromID(hwnd, 104), TRUE);
                    WinEnableWindow(WinWindowFromID(hwnd, 105), TRUE);
                    WinEnableWindow(WinWindowFromID(hwnd, 106), TRUE);
                }
            }
        break; }

        case WM_COMMAND:
        {
            switch(SHORT1FROMMP(mp1))
            {
                case 110: // ok-button
                {
                    XSTRING strSetup;
                    PSAMPLESETUP pSetup=(PSAMPLESETUP)pData->pUser;
                    // 'store' settings in pSetup
                    if(0==(long)WinSendDlgItemMsg(hwnd, 101,
                                                  BM_QUERYCHECKINDEX,
                                                  (MPARAM)0,
                                                  (MPARAM)0))
                        // radiobutton 1 is checked -> multi-view
                        pSetup->chDrive = '*';
                    else
                    {
                        // radiobutton 2 is checked -> single-view
                        char sz[2]={0};
                        WinQueryDlgItemText(hwnd, 106, 2, (PSZ)sz);
                        pSetup->chDrive = ValidateDrive(sz[0]);  // V0.9.11 (2001-04-19) [pr]: Validate drive letter
                    }

                    // 'show-styles'
                    pSetup->lShow=0;
                    if (WinQueryButtonCheckstate(hwnd, 107))
                        pSetup->lShow |= DISKFREE_SHOW_FS;

                    // something has changed:
                    WgtSaveSetup(&strSetup,
                                 pSetup);
                    pData->pctrSetSetupString(pData->hSettings,
                                              strSetup.psz);
                    pxstrClear(&strSetup);

                    WinDismissDlg(hwnd, TRUE);
                break; }
            }
        break; }

        default:
            mrc=WinDefDlgProc(hwnd, msg, mp1, mp2);
    }

    return(mrc);
}


/*
 *@@ WwgtShowSettingsDlg:
 *      this displays the winlist widget's settings
 *      dialog.
 *
 *      This procedure's address is stored in
 *      XCENTERWIDGET so that the XCenter knows that
 *      we can do this.
 *
 *      When calling this function, the XCenter expects
 *      it to display a modal dialog and not return
 *      until the dialog is destroyed. While the dialog
 *      is displaying, it would be nice to have the
 *      widget dynamically update itself.
 *
 *@@changed V0.9.11 (2001-04-18) [umoeller]: moved dialog to XFLDR001.DLL
 */

void EXPENTRY WgtShowSettingsDlg(PWIDGETSETTINGSDLGDATA pData)
{
    HWND hwnd = WinLoadDlg(HWND_DESKTOP,         // parent
                           pData->hwndOwner,
                           fnwpSettingsDlg,
                           // hmod,
                           pcmnQueryNLSModuleHandle(FALSE), // V0.9.11 (2001-04-18) [umoeller]
                           // 1200,
                           ID_CRD_DISKFREEWGT_SETTINGS, // V0.9.11 (2001-04-18) [umoeller]
                           // pass original setup string with WM_INITDLG
                           (PVOID)pData);

    if(hwnd)
    {
        pcmnSetControlsFont(hwnd,
                            1,
                            10000);

        pwinhCenterWindow(hwnd);         // invisibly

        // go!!
        WinProcessDlg(hwnd);

        WinDestroyWindow(hwnd);
    }
}

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
 *
 *@@changed V0.9.11 (2001-04-18) [umoeller]: moved icons to XWPRES.DLL
 */

MRESULT WgtCreate(HWND hwnd,
                  PXCENTERWIDGET pWidget)
{
    MRESULT mrc = 0;
    HMODULE hmodRes = pcmnQueryMainResModuleHandle(); // V0.9.11 (2001-04-18) [umoeller]
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

    pPrivate->hptrDrive = WinLoadPointer(HWND_DESKTOP,
                                         hmodRes,
                                         ID_ICON_DRIVE);

    pPrivate->hptrHand  = WinLoadPointer(HWND_DESKTOP,
                                         hmodRes,
                                         ID_POINTER_HAND);


    pPrivate->hptrDrives[0] = WinLoadPointer(HWND_DESKTOP,
                                             hmodRes,
                                             ID_ICON_DRIVE_NORMAL);

    pPrivate->hptrDrives[1] = WinLoadPointer(HWND_DESKTOP,
                                             hmodRes,
                                             ID_ICON_DRIVE_LAN);

    pPrivate->hptrDrives[2] = WinLoadPointer(HWND_DESKTOP,
                                             hmodRes,
                                             ID_ICON_DRIVE_CD);

    pdoshEnumDrives((PSZ)pPrivate->szDrives,
                    NULL,
                    TRUE);

    pPrivate->lCX = 10;          // we'll resize ourselves later

    if(pPrivate->Setup.chDrive=='*')
        pPrivate->chAktDrive = *pPrivate->szDrives;
    else
        pPrivate->chAktDrive = pPrivate->Setup.chDrive;

    GetDriveInfo(pPrivate);

    // start update timer
    pPrivate->ulTimerID = ptmrStartXTimer((PXTIMERSET)pPrivate->pWidget->pGlobals->pvXTimerSet,
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
                        pszl->cx = pPrivate->lCX;      // desired width
                        pszl->cy = 20;                 // desired minimum height
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

                        // V0.9.11 (2001-04-19) [pr]: Don't change drive when selecting multi-view
                        if(pPrivate->Setup.chDrive != '*')
                            pPrivate->chAktDrive=pPrivate->Setup.chDrive;


                        GetDriveInfo(pPrivate);

                        pPrivate->lCX=10;
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
 */

void WgtPaint(HWND hwnd,
              PXCENTERWIDGET pWidget)
{
    HPS hps = WinBeginPaint(hwnd, NULLHANDLE, NULL);
    if (hps)
    {
        PDISKFREEPRIVATE pPrivate = (PDISKFREEPRIVATE)pWidget->pUser;
        if (pPrivate)
        {
            RECTL       rclWin;
            POINTL      aptlText[TXTBOX_COUNT];
            BYTE        bxCorr;
            char        szText[64];
            double      dPercentFree=0;


            // now paint frame
            WinQueryWindowRect(hwnd,
                               &rclWin);
            pgpihSwitchToRGB(hps);

            WinFillRect(hps,
                        &rclWin,                // exclusive
                        pPrivate->Setup.lcolBackground);


            // draw border
            if(pPrivate->pWidget->pGlobals->flDisplayStyle & XCS_SUNKBORDERS)
             {
              ULONG ulBorder=1;
              RECTL rcl2;


              memcpy(&rcl2, &rclWin, sizeof(RECTL));
              rcl2.xRight--;
              rcl2.yTop--;

              pgpihDraw3DFrame(hps,
                               &rcl2,
                               ulBorder,
                               pPrivate->pWidget->pGlobals->lcol3DDark,
                               pPrivate->pWidget->pGlobals->lcol3DLight);

              rclWin.xLeft += ulBorder;
              rclWin.yBottom += ulBorder;
              rclWin.xRight -= ulBorder;
              rclWin.yTop -= ulBorder;
             }



            // calculate percent
            dPercentFree=pPrivate->dAktDriveFree*100/pPrivate->dAktDriveSize;

            if(pPrivate->Setup.chDrive=='*') // == multi-view-clickable
             {
              // draw drive-icon
              WinStretchPointer(hps,
                                rclWin.xLeft+3,
                                (rclWin.yTop-rclWin.yBottom-11)/2+1,
                                21,
                                11,
                                pPrivate->hptrDrives[pPrivate->bFSIcon],
                                DP_NORMAL);
                                                           // pPrivate->dAktDriveSize/1024/1024...100%
              // print drive-data                             pPrivate->dAktDriveFree/1024/1024...x%
              // V0.9.11 (2001-04-19) [pr]: Fixed show drive type
              if(pPrivate->Setup.lShow & DISKFREE_SHOW_FS)
                sprintf(szText, "%c: (%s)  %.fMB (%.f%%) free", pPrivate->chAktDrive,
                                                                pPrivate->szAktDriveType,
                                                                pPrivate->dAktDriveFree/1024/1024,
                                                                dPercentFree);
              else
                sprintf(szText, "%c:  %.fMB (%.f%%) free", pPrivate->chAktDrive,
                                                           pPrivate->dAktDriveFree/1024/1024,
                                                           dPercentFree);

              bxCorr=30;
             }
            else
             {
              // draw drive-icon
              WinStretchPointer(hps,
                                rclWin.xLeft,
                                (rclWin.yTop-rclWin.yBottom-11)/2+1,
                                21,
                                11,
                                pPrivate->hptrDrives[pPrivate->bFSIcon],
                                DP_NORMAL);

              if(pPrivate->Setup.lShow & DISKFREE_SHOW_FS)
                sprintf(szText, "%c: (%s)  %.fMB", pPrivate->chAktDrive,
                                                   pPrivate->szAktDriveType,
                                                   pPrivate->dAktDriveFree/1024/1024);
              else
                sprintf(szText, "%c:  %.fMB", pPrivate->chAktDrive,
                                              pPrivate->dAktDriveFree/1024/1024);

              bxCorr=24;

              //rclWin.xLeft+=24;
             }



            // now check if we have enough space
            GpiQueryTextBox(hps,
                            strlen(szText),
                            szText,
                            TXTBOX_COUNT,
                            aptlText);

            if(((aptlText[TXTBOX_TOPRIGHT].x+bxCorr) > (rclWin.xRight+2)) || pPrivate->lCX==10)
             {
              // we need more space: tell XCenter client
              pPrivate->lCX = (aptlText[TXTBOX_TOPRIGHT].x + bxCorr+6);

              WinPostMsg(WinQueryWindow(hwnd, QW_PARENT),
                         XCM_SETWIDGETSIZE,
                         (MPARAM)hwnd,
                         (MPARAM)pPrivate->lCX);
             }
            else
             {
              // sufficient space:
              rclWin.xLeft+=bxCorr;

              WinDrawText(hps,
                          strlen(szText),
                          szText,
                          &rclWin,
                          pPrivate->Setup.lcolForeground,
                          pPrivate->Setup.lcolBackground,
                          DT_LEFT| DT_VCENTER);
             }
        }
        WinEndPaint(hps);  // V0.9.11 (2001-04-19) [pr]: Moved to correct place
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
 *
 *@@changed V0.9.13 (2001-06-21) [umoeller]: changed XCM_SAVESETUP call for tray support
 */

void WgtPresParamChanged(HWND hwnd,
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
                // changed V0.9.13 (2001-06-21) [umoeller]:
                // post it to parent instead of fixed XCenter client
                // to make this trayable
                WinSendMsg(WinQueryWindow(hwnd, QW_PARENT), // pPrivate->pWidget->pGlobals->hwndClient,
                           XCM_SAVESETUP,
                           (MPARAM)hwnd,
                           (MPARAM)strSetup.psz);
            pxstrClear(&strSetup);
        }
    } // end if (pPrivate)
}

/*
 *@@ GetDriveInfo:
 *      Returns TRUE if the drive info has changed
 *      and the display should therefore be updated.
 *
 *@@changed V0.9.11 (2001-04-25) [umoeller]: added error checking
 */

BOOL GetDriveInfo(PDISKFREEPRIVATE pPrivate)
{
    double dOldDriveFree = pPrivate->dAktDriveFree;

    APIRET arc = pdoshQueryDiskFSType(pPrivate->chAktDrive-64,
                                      (PSZ)pPrivate->szAktDriveType,
                                      sizeof(pPrivate->szAktDriveType));
    if (!arc)
    {
        if (!strcmp("LAN", pPrivate->szAktDriveType))
            pPrivate->bFSIcon = 1;
        else if(!strcmp("CDFS", pPrivate->szAktDriveType))
            pPrivate->bFSIcon = 2;
        else
            pPrivate->bFSIcon = 0;

        if (    (!(arc = pdoshQueryDiskFree(pPrivate->chAktDrive-64,
                                            &pPrivate->dAktDriveFree)))
             && (!(arc = pdoshQueryDiskSize(pPrivate->chAktDrive-64,
                                            &pPrivate->dAktDriveSize)))
           )
            return((BOOL)pPrivate->dAktDriveFree != dOldDriveFree);
    }

    return (FALSE);
}

void GetDrive(HWND hwnd,
              PXCENTERWIDGET pWidget,
              BOOL fNext)
{
    PDISKFREEPRIVATE pPrivate = (PDISKFREEPRIVATE)pWidget->pUser;
    if(pPrivate)
    {
         // V0.9.11 (2001-04-19) [pr]: Rewrite to use drive char. rather than pointers
         CHAR *pCurrent = strchr(pPrivate->szDrives, pPrivate->chAktDrive);

         if (pCurrent == NULL)
             pPrivate->chAktDrive = *pPrivate->szDrives;
         else
         {
            if(fNext)
            {
                // return the next drive
                if(pCurrent >= pPrivate->szDrives+strlen(pPrivate->szDrives)-1)
                    pPrivate->chAktDrive = *pPrivate->szDrives;
                else
                    pPrivate->chAktDrive = *(pCurrent+1);
            }
            else
            {
                // return the prev drive
                if(pCurrent == pPrivate->szDrives)
                    pPrivate->chAktDrive = *(pPrivate->szDrives+strlen(pPrivate->szDrives)-1);
                else
                    pPrivate->chAktDrive = *(pCurrent-1);
            }
        }

        GetDriveInfo(pPrivate);

        pPrivate->lCX=10;
        WinInvalidateRect(hwnd,
                          NULLHANDLE,
                          TRUE);
    }
}

/*
 *@@ ValidateDrive:
 *      checks for valid drive letter or *, converts lower to upper case.
 *
 *@@added V0.9.11 (2001-04-19) [pr]
 */

CHAR ValidateDrive(CHAR chDrive)
{
    if(   (chDrive >= 'a')
       && (chDrive <= 'z')
      )
        chDrive &= ~0x20;

    if(   (chDrive != '*')
       && (   (chDrive < 'A')
           || (chDrive > 'Z')
          )
      )
        chDrive = 'C';

    return(chDrive);
}


/*
 *@@ WgtDestroy:
 *      implementation for WM_DESTROY.
 *
 *      This must clean up all allocated resources.
 */

void WgtDestroy(HWND hwnd,
                PXCENTERWIDGET pWidget)
{
    PDISKFREEPRIVATE pPrivate = (PDISKFREEPRIVATE)pWidget->pUser;
    if (pPrivate)
    {
        if(pPrivate->ulTimerID)
           ptmrStopXTimer((PXTIMERSET)pPrivate->pWidget->pGlobals->pvXTimerSet,
                          hwnd,
                          pPrivate->ulTimerID);


        WinDestroyPointer(pPrivate->hptrDrive);
        WinDestroyPointer(pPrivate->hptrHand);

        WinDestroyPointer(pPrivate->hptrDrives[0]);
        WinDestroyPointer(pPrivate->hptrDrives[1]);
        WinDestroyPointer(pPrivate->hptrDrives[2]);


        WgtClearSetup(&pPrivate->Setup);

        free(pPrivate);
                // pWidget is cleaned up by DestroyWidgets
    }
}

/*
 *@@ fnwpSampleWidget:
 *      window procedure for the Diskfree widget class.
 *
 *      There are a few rules which widget window procs
 *      must follow. See XCENTERWIDGETCLASS in center.h
 *      for details.
 *
 *      Other than that, this is a regular window procedure
 *      which follows the basic rules for a PM window class.
 *
 *@@changed V0.9.11 (2001-04-18) [umoeller]: couple of fixes for the winproc.
 */

MRESULT EXPENTRY fnwpSampleWidget(HWND hwnd,
                                  ULONG msg,
                                  MPARAM mp1,
                                  MPARAM mp2)
{
    MRESULT mrc = 0;

    // get widget data from QWL_USER (stored there by WM_CREATE)
    PXCENTERWIDGET pWidget = (PXCENTERWIDGET)WinQueryWindowPtr(hwnd, QWL_USER); // this ptr is valid after WM_CREATE

    switch(msg)
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
                mrc = (MRESULT)TRUE;
        break;

        case WM_CONTROL:
            mrc = (MRESULT)WgtControl(hwnd, mp1, mp2);
        break;

        case WM_MOUSEMOVE:
        {
            // This was wrong. You must call the default window procedure unless you
            // handle all cases.  V0.9.11 (2001-04-27) [pr]
            if(pWidget)
            {
                PDISKFREEPRIVATE pPrivate = (PDISKFREEPRIVATE)pWidget->pUser;

                if(pPrivate->Setup.chDrive=='*')
                {
                    WinSetPointer(HWND_DESKTOP, pPrivate->hptrHand);
                    mrc = (MRESULT)TRUE;        // V0.9.11 (2001-04-18) [umoeller]
                }
            }
            if (!mrc)
                mrc = pWidget->pfnwpDefWidgetProc(hwnd, msg, mp1, mp2);

        break; }

        case WM_BUTTON1CLICK:
        case WM_BUTTON1DBLCLK:  // V0.9.11 (2001-04-19) [pr]
        {
            if(pWidget)
            {
                PDISKFREEPRIVATE pPrivate = (PDISKFREEPRIVATE)pWidget->pUser;
                if(pPrivate->Setup.chDrive=='*')
                {
                    if ((long)WinGetKeyState(HWND_DESKTOP, VK_CTRL) & 0x8000)
                        GetDrive(hwnd, pWidget, FALSE);
                    else
                        GetDrive(hwnd, pWidget, TRUE);
                }
            }

            mrc = (MRESULT)TRUE;        // V0.9.11 (2001-04-18) [umoeller]
                                        // you processed the msg, so return TRUE
        break; }


        case WM_PAINT:
            WgtPaint(hwnd, pWidget);
        break;

        case WM_TIMER:
        {
            if(pWidget)
            {
                // get private data from that widget data
                PDISKFREEPRIVATE pPrivate = (PDISKFREEPRIVATE)pWidget->pUser;
                // V0.9.11 (2001-04-19) [pr]: Update drive list
                /* pdoshEnumDrives(pPrivate->szDrives,
                                NULL,
                                TRUE);      // skip removeables
                */
                // V0.9.11 (2001-04-25) [umoeller]: removed this again...
                // this is outright dangerous.
                // If this fails, e.g. because a CHKDSK is in progress, this
                // is dangerous, because the user gets the white error box
                // on each timer tick, making it almost impossible to close the
                // XCenter. So if this failed for any reason, stop the timer.
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
 *
 *@@changed V0.9.11 (2001-04-18) [umoeller]: added more imports from dosh.c
 */

ULONG EXPENTRY WgtInitModule(HAB hab,         // XCenter's anchor block
                             HMODULE hmodPlugin, // module handle of the widget DLL
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

void EXPENTRY WgtUnInitModule(void)
{
}


/*
 *@@ MwgtQueryVersion:
 *      this new export with ordinal 3 can return the
 *      XWorkplace version number which is required
 *      for this widget to run. For example, if this
 *      returns 0.9.10, this widget will not run on
 *      earlier XWorkplace versions.
 *
 *      NOTE: This export was mainly added because the
 *      prototype for the "Init" export was changed
 *      with V0.9.9. If this returns 0.9.9, it is
 *      assumed that the INIT export understands
 *      the new FNWGTINITMODULE_099 format (see center.h).
 *
 *@@added V0.9.9 (2001-02-06) [umoeller]
 *@@changed V0.9.11 (2001-04-18) [umoeller]: now reporting 0.9.11 because we need the newer imports
 */

void EXPENTRY WgtQueryVersion(PULONG pulMajor,
                              PULONG pulMinor,
                              PULONG pulRevision)
{
    *pulMajor = 0;
    *pulMinor = 9;
    *pulRevision = 13;
}


