
/*
 *@@sourcefile notebook.h:
 *      header file for notebook.c. See notes there. New with V0.82.
 *
 *      All the functions in this file have the ntb* prefix.
 *
 *@@include #define INCL_DOSMODULEMGR
 *@@include #define INCL_WINWINDOWMGR
 *@@include #define INCL_WINSTDCNR
 *@@include #include <os2.h>
 *@@include #include <wpobject.h>
 *@@include #include "shared\notebook.h"
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

#ifndef NOTEBOOK_HEADER_INCLUDED
    #define NOTEBOOK_HEADER_INCLUDED

    /********************************************************************
     *
     *   Declarations
     *
     ********************************************************************/

    // resize information for ID_XFD_CONTAINERPAGE, which is used
    // by many settings pages
    extern MPARAM *G_pampGenericCnrPage;
    extern ULONG G_cGenericCnrPage;

    // forward-declare the NOTEBOOKPAGE types, because
    // these are needed by the function prototypes below
    typedef struct _NOTEBOOKPAGE *PNOTEBOOKPAGE;

    // some callback function prototypes:

    // 1)  init-page callback
    typedef VOID XWPENTRY FNCBACTION(PNOTEBOOKPAGE, ULONG);
    typedef FNCBACTION *PFNCBACTION;

    // 2)  item-changed callback
    typedef MRESULT XWPENTRY FNCBITEMCHANGED(PNOTEBOOKPAGE,
                                      ULONG,    // ulItemID
                                            // V0.9.9 (2001-03-27) [umoeller]: turned USHORT into ULONG
                                      USHORT,   // usNotifyCode
                                      ULONG);   // ulExtra
    typedef FNCBITEMCHANGED *PFNCBITEMCHANGED;

    // 3)  message callback
    typedef BOOL XWPENTRY FNCBMESSAGE(PNOTEBOOKPAGE, ULONG, MPARAM, MPARAM, MRESULT*);
    typedef FNCBMESSAGE *PFNCBMESSAGE;

    /*
     *  here come the ORed flags which are passed to
     *  the INIT callback
     */

    #define CBI_INIT        0x01        // only set on first call (WM_INITDLG)
    #define CBI_SET         0x02        // controls should be set
    #define CBI_ENABLE      0x04        // controls should be en/disabled
    #define CBI_DESTROY     0x08        // notebook page is destroyed
    #define CBI_SHOW        0x10        // notebook page is turned to
    #define CBI_HIDE        0x20        // notebook page is turned away from

    // #define XNTBM_UPDATE     (WM_USER)  // update
                // moved to common.h

    #ifndef BKA_MAJOR
       #define BKA_MAJOR                0x0040
    #endif

    #ifndef BKA_MINOR
       #define BKA_MINOR                0x0080
    #endif

    #pragma pack(1)

    /*
     *@@ INSERTNOTEBOOKPAGE:
     *      this structure must be passed to ntbInsertPage
     *      and specifies lots of data according to which
     *      fnwpPageCommon will react.
     *
     *      Always zero the entire structure and then fill
     *      in only the fields that you need. The top fields
     *      listed below are required, all the others are
     *      optional and have safe defaults.
     *
     *      See ntbInsertPage for how to use this.
     *
     *@@changed V0.9.0 [umoeller]: typedef was missing, thanks Rdiger Ihle
     *@@changed V0.9.4 (2000-07-11) [umoeller]: added fPassCnrHelp
     *@@changed V0.9.18 (2002-02-23) [umoeller]: renamed from CREATENOTEBOOKPAGE, removed non-input data
     */

    typedef struct _INSERTNOTEBOOKPAGE
    {
        // 1) REQUIRED input to ntbInsertPage
        HWND        hwndNotebook;   // hwnd of Notebook control; set this to the
                                    // (misnamed) hwndDlg parameter in the
                                    // _wpAddSettingsPages method
        WPObject    *somSelf;       // object whose Settings notebook is opened;
                                    // set this to somSelf of _wpAddSettingsPages
        HMODULE     hmod;           // module of dlg resource
        ULONG       ulDlgID;        // ID of dlg resource (in hmod)
        PCSZ        pcszName;       // title of page (in notebook tab)

        // 2) OPTIONAL input to ntbInsertPage; all of these can be null
        USHORT      ulPageID;       // the page identifier, which should be set to
                                    // uniquely identify the notebook page (e.g. for
                                    // ntbQueryOpenPages); XWorkplace uses the SP_*
                                    // IDs def'd in common.h.
        USHORT      usPageStyleFlags; // any combination or none of the following:
                                    // -- BKA_MAJOR
                                    // -- BKA_MINOR
                                    // BKA_STATUSTEXTON will always be added.
        BOOL        fEnumerate;     // if TRUE: add "page 1 of 3"-like thingies
        PCSZ        pcszMinorName;  // if != NULL, subtitle to add to notebook context
                                    // menu V0.9.16 (2001-10-23) [umoeller]
                                    // (useful with fEnumerate)
        BOOL        fPassCnrHelp;   // if TRUE: CN_HELP is not intercepted, but sent
                                    // to "item changed" callback;
                                    // if FALSE: CN_HELP is processed like WM_HELP
                                    // V0.9.4 (2000-07-11) [umoeller]
        USHORT      ulDefaultHelpPanel; // default help panel ID for the whole page
                                    // in the XFolder help file;
                                    // this will be displayed when WM_HELP comes in
                                    // and if no subpanel could be found
        USHORT      ulTimer;        // if !=0, a timer will be started and pfncbTimer
                                    // will be called with this frequency (in ms)
        MPARAM      *pampControlFlags; // if != NULL, winhAdjustControls will be
                                    // called when the notebook gets resized with
                                    // the array of MPARAM's specified here; this
                                    // allows for automatic resizing of notebook
                                    // pages
        ULONG       cControlFlags;  // if (pampControlFlags != NULL), specify the
                                    // array item count here

        ULONG       ulCnrOwnerDraw; // CODFL_* flags for container owner draw,
                                    // if CA_OWNERDRAW is set for a container.
                                    // If this is != 0, cnrhOwnerDrawRecord is
                                    // called for painting records with these
                                    // flags; see cnrhOwnerDrawRecord for valid
                                    // values.
                                    // V0.9.16 (2001-09-29) [umoeller]

        // 3)  Here follow the callback functions. If any of these is NULL,
        //     it will not be called. As a result, you may selectively install
        //     callbacks, depending on how much functionality you need.

        PFNCBACTION pfncbInitPage;
                // callback function for initializing the page.
                // This is required and gets called (at least) when
                // the page is initialized (WM_INITDLG comes in).
                // See ntbInsertPage for details.

        PFNCBITEMCHANGED pfncbItemChanged;
                // callback function if an item on the page has changed; you
                // should update your data in memory then.
                // See ntbInsertPage for details.

        PFNCBACTION pfncbTimer;
                // optional callback function if INSERTNOTEBOOKPAGE.ulTimer != 0;
                // this callback gets called every INSERTNOTEBOOKPAGE.ulTimer
                // milliseconds then.

        PFNCBMESSAGE pfncbMessage;
                // optional callback function thru which all dialog messages are going.
                // You can use this if you need additional handling which the above
                // callbacks do not provide for. This gets really all the messages
                // which go thru fnwpPageCommon.
                //
                // This callback gets called _after_ all other message processing
                // (i.e. the "item changed" and "timer" callbacks).
                //
                // Parameters:
                //     PNOTEBOOKPAGE pcnbp      notebook info struct
                //     msg, mp1, mp2            usual message parameters.
                //     MRESULT* pmrc            return value, if TRUE is returned.
                //
                // If the callback returns TRUE, *pmrc is returned from the
                // common notebook page proc.

    } INSERTNOTEBOOKPAGE, *PINSERTNOTEBOOKPAGE;

    /*
     *@@ NOTEBOOKPAGE:
     *
     *@@added V0.9.18 (2002-02-23) [umoeller]
     */

    typedef struct _NOTEBOOKPAGE
    {
        INSERTNOTEBOOKPAGE  inbp;

        PVOID       pUser,
                    pUser2;         // user data; since you can access this structure
                // from the "pcnbp" parameter which is always passed to the notebook
                // callbacks, you can use this for backing up data for the "Undo" button
                // in the INIT callback, or for whatever other data you might need.
                // Simply allocate memory using malloc() and store it here.
                // When the notebook page is destroyed, both pointers are checked and
                // will automatically be free()'d if != NULL.

        // 4) The following fields are not intended for _input_ to ntbInsertPage.
        //    Instead, these contain additional data which can be evaluated from
        //    the callbacks. These fields are only set by fnwpPageCommon
        //    _after_ the page has been initialized.
        ULONG       ulNotebookPageID; // the PM notebook page ID, as returned by
                                      // wpInsertSettingsPage
        ULONG       flPage;           // any combination of the following:
                                      // V0.9.19 (2002-04-24) [umoeller]
                          #define NBFL_PAGE_INITED          0x0001
                                    // TRUE after the INIT callback has been called
                          #define NBFL_PAGE_SHOWING         0x0002
                                    // TRUE if the page is currently turned to
                                    // in the notebook
        HWND        hwndDlgPage;      // hwnd of dlg page in notebook; this
                                      // is especially useful to get control HWND's:
                                      // use WinWindowFromID(pnbp->hwndDlgPage, YOUR_ID).
        HWND        hwndFrame;        // frame window (to which hwndNotebook belongs);
                                      // use this as the owner for subdialogs to lock
                                      // the notebook
        HWND        hwndControl;      // this always has the current control window handle
                                      // when the "item changed" callback is called.
                                      // In the callback, this is equivalent to
                                      // calling WinWindowFromID(pnbp->hwndDlgPage, usControlID).
        HWND        hwndSourceCnr;    // see next
        PRECORDCORE preccSource;      // this can be set to a container record
                                      // core in hwndSourceCnr which will be removed
                                      // source emphasis from when WM_MENUEND
                                      // is received; useful for CN_CONTEXTMENU.
                                      // This gets initialized to -1, because
                                      // NULL means container whitespace.
        POINTL      ptlMenuMousePos;  // for CN_CONTEXTMENU, this has the
                                      // last mouse position (in Desktop coords).
        BOOL        fShowWaitPointer; // while TRUE, fnwpPageCommon shows the "Wait" pointer;
                                      // only meaningful if another thread is preparing data
        HWND        hwndTooltip;      // if this is != NULL, this window gets destroyed
                                      // automatically when the notebook page is destroyed.
                                      // Useful with tooltip controls, which are not destroyed
                                      // automatically otherwise.
        // 5) Internal use only, do not mess with these.
        PVOID       pnbli;
        PRECORDCORE preccLastSelected;
        PRECORDCORE preccExpanded;      // for tree-view auto scroll
        HWND        hwndExpandedCnr;    // for tree-view auto scroll
        PVOID       pxac;               // ptr to XADJUSTCTRLS if (pampControlFlags != NULL)

    } NOTEBOOKPAGE;

    #pragma pack()

    /*
     *@@ NOTEBOOKPAGELISTITEM:
     *      list item structure (linklist.c) for maintaining
     *      a list of currently open notebook pages.
     */

    typedef struct _NOTEBOOKPAGELISTITEM
    {
        ULONG                   ulSize;
        PNOTEBOOKPAGE           pnbp;
    } NOTEBOOKPAGELISTITEM, *PNOTEBOOKPAGELISTITEM;

    /*
     *@@ SUBCLNOTEBOOKLISTITEM:
     *      list item structure (linklist.c) for
     *      maintaining subclassed notebooks controls.
     *
     *@@added V0.9.1 (99-12-06) [umoeller]
     */

    typedef struct _SUBCLNOTEBOOKLISTITEM
    {
        HWND            hwndNotebook;
        PFNWP           pfnwpNotebookOrig;
    } SUBCLNOTEBOOKLISTITEM, *PSUBCLNOTEBOOKLISTITEM;

    /********************************************************************
     *
     *   Prototypes
     *
     ********************************************************************/

    ULONG ntbInsertPage(PINSERTNOTEBOOKPAGE pinbp);

    #ifdef DIALOG_HEADER_INCLUDED
        APIRET ntbFormatPage(HWND hwndDlg,
                             PCDLGHITEM paDlgItems,
                             ULONG cDlgItems);
    #endif

    PNOTEBOOKPAGE ntbQueryOpenPages(PNOTEBOOKPAGE pnbp);

    ULONG ntbUpdateVisiblePage(WPObject *somSelf, ULONG ulPageID);

    BOOL ntbTurnToPage(HWND hwndNotebook,
                       ULONG ulPageID);

    BOOL ntbOpenSettingsPage(PCSZ pcszObjectID,
                             ULONG ulPageID);

#endif
