
/*
 *@@sourcefile object.c:
 *      implementation code for XFldObject class.
 *
 *      This file is ALL new with V0.9.0.
 *
 *      This looks like a good place for general explanations
 *      about how the WPS maintains Desktop objects as SOM objects
 *      in memory.
 *
 *      The basic object life cycle is like this:
 *
 +        wpclsNew
 +        wpCopyObject
 +        wpCreateFromTemplate
 +              ³
 +              ³      ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿
 +              ³      ³ awake object ³
 +              ÀÄÄÄÄ> ³ (physical    ³ <ÄÄ wpclsMakeAwake ÄÄÄ¿
 +                     ³ and memory)  ³                       ³
 +                     ÀÄÄÄÄÄÂÄÂÄÄÄÄÄÄÙ                       ³
 +                           ³ ³                              ³
 +          \ /              ³ ³                   ÚÄÄÄÄÄÄÄÄÄÄÁÄÄÄÄÄ¿
 +           X   <ÄÄ wpFree ÄÙ ÀÄ wpMakeDormant Ä> ³ dormant object ³
 +          / \                                    ³ (physical only)³
 +                                                 ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ
 *
 *      The basic rule is that there is the concept of a
 *      "dormant object". This is an object that currently
 *      exists only in storage, but not as a SOM object in
 *      memory. "Storage" is disk for file-system objects
 *      and OS2.INI for abstract objects.
 *
 *      By contrast, an object is said to be "awake" if it
 *      also exists as a SOM object in memory. Per definition,
 *      an awake object also has a physical representation.
 *      Only WPTransients have no physical storage at all;
 *      as a consequence, there is really no such thing as
 *      a dormant transient object.
 *
 *      1)  Objects are most frequently made awake when a folder
 *          is populated (mostly on folder open). Of course this
 *          does not physically create objects... they are only
 *          instantiated in memory then from their physical
 *          storage.
 *
 *          There are many ways to awake objects. To awake a
 *          single object, call M_WPObject::wpclsMakeAwake.
 *          This is a bit difficult to manage, so
 *          WPFileSystem::wpclsQueryObjectFromPath is easier
 *          for waking up FS objects (which calls wpclsMakeAwake
 *          internally). wpclsMakeAwake also gets called from
 *          within WPFolder::wpPopulate.
 *
 *      2)  Even though this isn't documented anywhere, the
 *          WPS also supports the reverse concept of making
 *          the object dormant again. This will destroy the
 *          SOM object in memory, but not the physical representation.
 *
 *          I suspect this was not documented because you can
 *          never know whether some code still needs the
 *          SOM pointer to the object somehow. Anyway, the
 *          WPS _does_ make objects dormant again, e.g. when
 *          their folders are closed and they are not referenced
 *          from anywhere else. You can prevent the WPS from doing
 *          this by calling WPObject::wpLock.
 *
 *          The interesting thing is that there is an undocumented
 *          method for destroying the SOM object.
 *          WPObject::wpMakeDormant does exactly this.
 *          Actually, this does a lot of things:
 *
 *          --  It removes the object from all containers,
 *
 *          --  closes all views (wpClose),
 *
 *          --  frees all associated memory allocated thru wpAllocMem.
 *
 *          --  In addition, if the object has called _wpSaveDeferred
 *              and a _wpSaveImmediate is thus pending, _wpSaveImmediate
 *              also gets called. (See XFldObject::wpSaveDeferred
 *              for more about this.)
 *
 *          --  Finally, wpMakeDormant calls wpUnInitData, which
 *              should clean up the object.
 *
 *      3)  An object is physically created through either
 *          M_WPObject::wpclsNew or WPObject::wpCopyObject or
 *          WPObject::wpCreateAnother or WPObject::wpCreateFromTemplate.
 *          These are the "object factories" of the WPS. Depending
 *          on which class the method is invoked on, the new object
 *          will be of that class.
 *
 *          Depending on the object's class, wpclsNew will create
 *          a physical representation (e.g. file, folder) of the
 *          object _and_ a SOM object. So calling, for example,
 *          M_WPFolder::wpclsNew will create a new folder on disk
 *          and return a SOM object that represents it.
 *
 *      4)  Deleting an object can really be done in two ways:
 *
 *          --  WPObject::wpDelete looks like the most natural
 *              way. However this really only displays a
 *              confirmation and then invokes WPObject::wpFree.
 *
 *          --  WPObject::wpFree is the most direct way to
 *              delete an object. This does not display any
 *              more confirmations (in theory), but deletes
 *              the object right away.
 *
 *              Interestingly, wpFree in turn calls another
 *              undocumented method -- WPObject::wpDestroyObject.
 *              From my testing this is responsible for destroying
 *              the physical representation (file, folder, INI data).
 *
 *              After that, wpFree also calls wpMakeDormant
 *              to free the SOM object.
 *
 *      wpDestroyObject is a bit obscure. I believe it is this
 *      method which was supposed to do the object cleanup.
 *      It is introduced by WPObject and overridden by the
 *      following classes:
 *
 *      --  WPFileSystem: apparently, this then does DosDelete.
 *          Unfortunately, this one has a real nasty bug... it
 *          displays a message box if deleting the object fails.
 *          This is really annoying when calling wpFree in a loop
 *          on a bunch of objects. That's why we override this
 *          method in WPDataFile::wpDestroyObject and similar
 *          methods (V0.9.20).
 *
 *      --  WPAbstract: this probably removes the INI entries
 *          associated with the abstract object.
 *
 *      --  WPProgram.
 *
 *      --  WPProgramFile.
 *
 *      --  WPTransient.
 *
 *      If folder auto-refresh is replaced by XWP, we must override
 *      wpFree in order to suppress calling this method. The message
 *      box bug is not acceptable for file-system objects, so we have
 *      introduced WPDataFile::wpDestroyObject instead.
 *
 *      The destruction call sequence thus is:
 *
 +          wpDelete (display confirmation, if applicable)
 +             |
 +             +-- wpFree (really delete the object now)
 +                   |
 +                   +-- wpDestroyObject (delete physical storage)
 +                   |
 +                   +-- wpMakeDormant (delete SOM object in memory)
 +                         |
 +                         +-- (lots of cleanup: wpClose, etc.)
 +                         |
 +                         +-- wpSaveImmediate (if "dirty")
 +                         |
 +                         +-- wpUnInitData
 *
 *      Function prefix for this file:
 *      --  obj*
 *
 *@@added V0.9.0 [umoeller]
 *@@header "filesys\object.h"
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

#define INCL_DOSPROCESS
#define INCL_DOSEXCEPTIONS
#define INCL_DOSSEMAPHORES
#define INCL_DOSRESOURCES
#define INCL_DOSMISC
#define INCL_DOSERRORS

#define INCL_WININPUT
#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WINMESSAGEMGR
#define INCL_WINPOINTERS
#define INCL_WINSYS
#define INCL_WINTIMER
#define INCL_WINMENUS
#define INCL_WINDIALOGS
#define INCL_WINSTATICS
#define INCL_WINBUTTONS
#define INCL_WINENTRYFIELDS
#define INCL_WINMLE
#define INCL_WINSTDCNR
#define INCL_WINSTDBOOK
#define INCL_WINPROGRAMLIST     // needed for wppgm.h
#define INCL_WINSWITCHLIST
#define INCL_WINSHELLDATA
#define INCL_WINCLIPBOARD
#include <os2.h>

// C library headers
#include <stdio.h>
#include <setjmp.h>             // needed for except.h
#include <assert.h>             // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers
#include "helpers\apps.h"               // application helpers
#include "helpers\cnrh.h"               // container helper routines
#include "helpers\dialog.h"             // dialog helpers
#include "helpers\dosh.h"               // Control Program helper routines
#include "helpers\except.h"             // exception handling
#include "helpers\linklist.h"           // linked list helper routines
#include "helpers\prfh.h"               // INI file helper routines
#include "helpers\standards.h"          // some standard macros
#include "helpers\stringh.h"            // string helper routines
#include "helpers\tree.h"               // red-black binary trees
#include "helpers\winh.h"               // PM helper routines
#include "helpers\wphandle.h"           // file-system object handles
#include "helpers\xstring.h"            // extended string helpers

// SOM headers which don't crash with prec. header files
#include "xfldr.ih"
#include "xfobj.ih"

// XWorkplace implementation headers
#include "dlgids.h"                     // all the IDs that are shared with NLS
#include "shared\classtest.h"           // some cheap funcs for WPS class checks
#include "shared\cnrsort.h"             // container sort comparison functions
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\helppanels.h"          // all XWorkplace help panel IDs
#include "shared\notebook.h"            // generic XWorkplace notebook handling
#include "shared\wpsh.h"                // some pseudo-SOM functions (WPS helper routines)

// headers in /hook
#include "hook\xwphook.h"

#include "filesys\fdrmenus.h"           // shared folder menu logic
#include "filesys\filesys.h"            // various file-system object implementation code
#include "filesys\folder.h"             // XFolder implementation
#include "filesys\icons.h"              // icons handling
#include "filesys\object.h"             // XFldObject implementation
#include "filesys\program.h"            // program implementation; WARNING: this redefines macros
#include "filesys\xthreads.h"           // extra XWorkplace threads

#include "config\hookintf.h"            // daemon/hook interface

// other SOM headers
#pragma hdrstop
// #include <wptrans.h>                    // WPTransient
#include <wpclsmgr.h>                   // WPClassMgr
#include <wpshadow.h>

#include "helpers\undoc.h"              // some undocumented stuff

/* ******************************************************************
 *
 *   Private declarations
 *
 ********************************************************************/

/*
 *@@ OBJTREENODE:
 *      tree node structure for object handles cache.
 *
 *@@added V0.9.9 (2001-04-02) [umoeller]
 */

typedef struct _OBJTREENODE
{
    TREE        Tree;
    WPObject    *pObject;
    ULONG       ulReferenced;       // system uptime count when last referenced
} OBJTREENODE, *POBJTREENODE;

#define CACHE_ITEM_LIMIT    200

/* ******************************************************************
 *
 *   Global variables
 *
 ********************************************************************/

// mutex semaphores for object lists (favorite folders, quick-open)
static HMTX         G_hmtxObjectsLists = NULLHANDLE;

// object handles cache
static TREE         *G_HandlesCacheRoot;
static HOBJECT      G_hobjLast = NULLHANDLE;        // V0.9.20 (2002-07-25) [umoeller]
static WPObject     *G_pobjLast = NULL;
static HMTX         G_hmtxHandlesCache = NULLHANDLE;
static LONG         G_lHandlesCacheItemsCount = 0;

// dirty objects list
static TREE        *G_DirtyList;
static HMTX        G_hmtxDirtyList = NULLHANDLE;
static LONG        G_lDirtyListItemsCount = 0;

/* ******************************************************************
 *
 *   Object internals
 *
 ********************************************************************/

/*
 *@@ objIsAShadow:
 *      returns TRUE if somSelf is a shadow.
 *
 *@@added V0.9.20 (2002-07-31) [umoeller]
 */

BOOL objIsAShadow(WPObject *somSelf)
{
    XFldObjectData *somThis = XFldObjectGetData(somSelf);
    return (0 != (_flObject & OBJFL_WPSHADOW));
}

/*
 *@@ objResolveIfShadow:
 *      resolves a shadow if somSelf is one.
 *
 *      If somSelf is a WPShadow object, this returns
 *      the target object or NULL if the shadow is
 *      broken.
 *
 *      Otherwise it returns somSelf.
 *
 *@@added V0.9.18 (2002-03-23) [umoeller]
 */

WPObject* objResolveIfShadow(WPObject *somSelf)
{
    if (somSelf)
    {
        XFldObjectData *somThis = XFldObjectGetData(somSelf);
        if (_flObject & OBJFL_WPSHADOW)
            return _wpQueryShadowedObject(somSelf, TRUE);
    }

    return somSelf;
}

/*
 *@@ objQueryFlags:
 *
 *@@added V0.9.19 (2002-04-24) [umoeller]
 */

ULONG objQueryFlags(WPObject *somSelf)
{
    XFldObjectData *somThis = XFldObjectGetData(somSelf);
    return _flObject;
}

/*
 *@@ objIsAnAbstract:
 *      returns TRUE if somSelf is an abstract.
 *
 *@@added V0.9.19 (2002-04-24) [umoeller]
 */

BOOL objIsAnAbstract(WPObject *somSelf)
{
    XFldObjectData *somThis = XFldObjectGetData(somSelf);
    return (0 != (_flObject & OBJFL_WPABSTRACT));
}

/*
 *@@ objIsAFolder:
 *      returns TRUE if somSelf is a folder.
 *
 *@@added V0.9.18 (2002-03-23) [umoeller]
 */

BOOL objIsAFolder(WPObject *somSelf)
{
    XFldObjectData *somThis = XFldObjectGetData(somSelf);
    return (0 != (_flObject & OBJFL_WPFOLDER));
}

/*
 *@@ objIsADataFile:
 *      returns TRUE if somSelf is a data file.
 *
 *@@added V0.9.20 (2002-07-31) [umoeller]
 */

BOOL objIsADataFile(WPObject *somSelf)
{
    XFldObjectData *somThis = XFldObjectGetData(somSelf);
    return (0 != (_flObject & OBJFL_WPDATAFILE));
}

/*
 *@@ objIsObjectInitialized:
 *      the same as WPObject::wpIsObjectInitialized,
 *      but faster.
 *
 *@@added V0.9.19 (2002-04-17) [umoeller]
 */

BOOL objIsObjectInitialized(WPObject *somSelf)
{
    XFldObjectData *somThis = XFldObjectGetData(somSelf);
    return (0 != (_flObject & OBJFL_INITIALIZED));
}

/* ******************************************************************
 *
 *   Object setup
 *
 ********************************************************************/

/*
 *@@ objSetup:
 *      implementation for XFldObject::wpSetup.
 *
 *@@added V0.9.9 (2001-04-06) [umoeller]
 */

BOOL objSetup(WPObject *somSelf,
              PSZ pszSetupString)
{
    ULONG   cb = 0;
    BOOL    brc = TRUE;

    if (_wpScanSetupString(somSelf,
                           pszSetupString,
                           "WRITEREXXSETUP",
                           NULL,            // get size
                           &cb))
    {
        PSZ pszRexxFile;
        if (pszRexxFile = malloc(cb))
        {
            if (_wpScanSetupString(somSelf,
                                   pszSetupString,
                                   "WRITEREXXSETUP",
                                   pszRexxFile,
                                   &cb))
            {
                brc = !objCreateObjectScript(somSelf,
                                             pszRexxFile,
                                             NULL,
                                             SCRFL_RECURSE);
            }

            free(pszRexxFile);
        }
    }

    return brc;
}

/*
 *@@ objQuerySetup:
 *      implementation of XFldObject::xwpQuerySetup.
 *      See remarks there.
 *
 *      This returns the length of the XFldObject
 *      setup string part only.
 *
 *@@added V0.9.1 (2000-01-16) [umoeller]
 *@@changed V0.9.4 (2000-08-02) [umoeller]: added NOCOPY, NODELETE etc.
 *@@changed V0.9.5 (2000-08-26) [umoeller]: added DEFAULTVIEW=RUNNING; fixed class default view
 *@@changed V0.9.7 (2000-12-10) [umoeller]: added LOCKEDINPLACE
 *@@todo some strings missing
 *@@changed V0.9.18 (2002-03-23) [umoeller]: optimized
 *@@changed V0.9.19 (2002-07-01) [umoeller]: adapted to use IBMOBJECTDATA
 *@@changed V0.9.20 (2002-07-12) [umoeller]: added HELPLIBRARY
 */

BOOL objQuerySetup(WPObject *somSelf,
                   PVOID pstrSetup)
{
    ULONG   ulValue = 0,
            ulStyle = 0,
            ulClassStyle = 0;
    PSZ     pszValue = 0;

    XFldObjectData  *somThis = XFldObjectGetData(somSelf);
    SOMClass        *pMyClass = _somGetClass(somSelf);


    #define APPEND(str) xstrcat(pstrSetup, (str), sizeof(str) - 1)

    // CCVIEW
    ulValue = _wpQueryConcurrentView(somSelf);
    switch (ulValue)
    {
        case CCVIEW_ON:
            APPEND("CCVIEW=YES;");
        break;

        case CCVIEW_OFF:
            APPEND("CCVIEW=NO;");
        break;
        // ignore CCVIEW_DEFAULT
    }

    // DEFAULTVIEW
    // modified to use IBMOBJECTDATA V0.9.19 (2002-07-01) [umoeller]
    if (_pvWPObjectData)
    {
        ULONG ulClassDefaultView = _wpclsQueryDefaultView(pMyClass);
        ULONG ulDefaultView = ((PIBMOBJECTDATA)_pvWPObjectData)->ulDefaultView;

        if (    (ulDefaultView != 0x67)      // default view for folders
             && (ulDefaultView != 0x1000)    // default view for data files
             && (ulDefaultView != -1)        // OPEN_DEFAULT
             && (ulDefaultView != ulClassDefaultView) // OPEN_DEFAULT
           )
        {
            switch (ulDefaultView)
            {
                case OPEN_SETTINGS:
                    APPEND("DEFAULTVIEW=SETTINGS;");
                break;

                case OPEN_CONTENTS:
                    APPEND("DEFAULTVIEW=ICON;");
                break;

                case OPEN_TREE:
                    APPEND("DEFAULTVIEW=TREE;");
                break;

                case OPEN_DETAILS:
                    APPEND("DEFAULTVIEW=DETAILS;");
                break;

                case OPEN_RUNNING:
                    APPEND("DEFAULTVIEW=RUNNING;");
                break;

                case OPEN_DEFAULT:
                    // ignore
                break;

                default:
                {
                    // any other: that's user defined, add decimal ID
                    CHAR szTemp[30];
                    sprintf(szTemp, "DEFAULTVIEW=%d;", ulDefaultView);
                    xstrcat(pstrSetup, szTemp, 0);
                }
                break;
            }
        }
    }

    if (_pvWPObjectData)
    {
        // HELPLIBRARY
        // added V0.9.20 (2002-07-12) [umoeller]
        PSZ p;
        if (    (p = (((PIBMOBJECTDATA)_pvWPObjectData)->pszHelpLibrary))
             && (*p)
           )
        {
            APPEND("HELPLIBRARY=");
            xstrcat(pstrSetup, p, 0);
            xstrcatc(pstrSetup, ';');
        }

        // HELPPANEL
        // modified to use IBMOBJECTDATA V0.9.19 (2002-07-01) [umoeller]

        if (((PIBMOBJECTDATA)_pvWPObjectData)->ulHelpPanelID)
        {
            CHAR szTemp[40];
            sprintf(szTemp, "HELPPANEL=%d;", ((PIBMOBJECTDATA)_pvWPObjectData)->ulHelpPanelID);
            xstrcat(pstrSetup, szTemp, 0);
        }
    }

    // HIDEBUTTON
    ulValue = _wpQueryButtonAppearance(somSelf);
    switch (ulValue)
    {
        case HIDEBUTTON:
            APPEND("HIDEBUTTON=YES;");
        break;

        case MINBUTTON:
            APPEND("HIDEBUTTON=NO;");
        break;

        // ignore DEFAULTBUTTON
    }

    // ICONFILE: cannot be queried!
    // ICONRESOURCE: cannot be queried!

    // ICONPOS: x, y in percentage of folder coordinates

    // MENUS: Warp 4 only

    // MINWIN
    ulValue = _wpQueryMinWindow(somSelf);
    switch (ulValue)
    {
        case MINWIN_HIDDEN:
            APPEND("MINWIN=HIDE;");
        break;

        case MINWIN_VIEWER:
            APPEND("MINWIN=VIEWER;");
        break;

        case MINWIN_DESKTOP:
            APPEND("MINWIN=DESKTOP;");
        break;

        // ignore MINWIN_DEFAULT
    }

    // compare wpQueryStyle with clsStyle
    // V0.9.18 (2002-03-23) [umoeller]: rewritten
    ulStyle = _wpQueryStyle(somSelf);
    ulClassStyle = _wpclsQueryStyle(pMyClass);

    {
        static const struct
        {
            ULONG   fl;
            PCSZ    pcsz;
        } aStyles[] =
            {
                OBJSTYLE_NOMOVE, "NOMOVE=",
                    // OBJSTYLE_NOMOVE == CLSSTYLE_NEVERMOVE == 0x00000002; see wpobject.h
                OBJSTYLE_NOLINK, "NOLINK=",
                    // OBJSTYLE_NOLINK == CLSSTYLE_NEVERLINK == 0x00000004; see wpobject.h
                OBJSTYLE_NOCOPY, "NOCOPY=",
                    // OBJSTYLE_NOCOPY == CLSSTYLE_NEVERCOPY == 0x00000008; see wpobject.h
                OBJSTYLE_NODELETE, "NODELETE=",
                    // OBJSTYLE_NODELETE == CLSSTYLE_NEVERDELETE == 0x00000040; see wpobject.h
                OBJSTYLE_NOPRINT, "NOPRINT=",
                    // OBJSTYLE_NOPRINT == CLSSTYLE_NEVERPRINT == 0x00000080; see wpobject.h
                OBJSTYLE_NODRAG, "NODRAG=",
                    // OBJSTYLE_NODRAG == CLSSTYLE_NEVERDRAG == 0x00000100; see wpobject.h
                OBJSTYLE_NOTVISIBLE, "NOTVISIBLE=",
                    // OBJSTYLE_NOTVISIBLE == CLSSTYLE_NEVERVISIBLE == 0x00000200; see wpobject.h
                OBJSTYLE_NOSETTINGS, "NOSETTINGS=",
                    // OBJSTYLE_NOSETTINGS == CLSSTYLE_NEVERSETTINGS == 0x00000400; see wpobject.h
                OBJSTYLE_NORENAME, "NORENAME=",
                    // OBJSTYLE_NORENAME == CLSSTYLE_NEVERRENAME == 0x00000800; see wpobject.h
                // NODROP is obscure: wpobject.h writes:
                /*
                  #define OBJSTYLE_NODROP         0x00001000
                  #define OBJSTYLE_NODROPON       0x00002000   // Use instead of OBJSTYLE_NODROP,
                                                          because OBJSTYLE_NODROP and
                                                          CLSSTYLE_PRIVATE have the same
                                                          value (DD 86093F)  */
                OBJSTYLE_NODROPON, "NODROP="
                    // OBJSTYLE_NODROPON == CLSSTYLE_NEVERDROPON == 0x00002000; see wpobject.h
            };

        ULONG ul;
        for (ul = 0;
             ul < ARRAYITEMCOUNT(aStyles);
             ul++)
        {
            ULONG flThis = aStyles[ul].fl;
            if ((ulStyle & flThis) != (ulClassStyle & flThis))
            {
                // object style is different from class style:
                xstrcat(pstrSetup, aStyles[ul].pcsz, 0);
                if (ulStyle & flThis)
                    xstrcat(pstrSetup, "YES;", 4);
                else
                    xstrcat(pstrSetup, "NO;", 3);

            }
        }
    }

    if (ulStyle & OBJSTYLE_TEMPLATE)
        APPEND("TEMPLATE=YES;");

    // LOCKEDINPLACE: Warp 4 only
    if (G_fIsWarp4)
        if (ulStyle & OBJSTYLE_LOCKEDINPLACE)
            APPEND("LOCKEDINPLACE=YES;");

    // TITLE
    /* pszValue = _wpQueryTitle(somSelf);
    {
        xstrcat(pstrSetup, "TITLE=");
            xstrcat(pstrSetup, pszValue);
            xstrcat(pstrSetup, ";");
    } */

    // OBJECTID: always append this LAST!
    if (    (pszValue = _wpQueryObjectID(somSelf))
         && (ulValue = strlen(pszValue))
       )
    {
        APPEND("OBJECTID=");
        xstrcat(pstrSetup, pszValue, ulValue);
        xstrcatc(pstrSetup, ';');
    }

    return TRUE;
}

/* ******************************************************************
 *
 *   Object scripts
 *
 ********************************************************************/

/*
 *@@ WriteOutObjectSetup:
 *
 *@@added V0.9.9 (2001-04-06) [umoeller]
 *@@changed V0.9.18 (2002-02-24) [pr]: was freeing setup string twice
 */

static ULONG WriteOutObjectSetup(FILE *RexxFile,
                                 WPObject *pobj,
                                 ULONG ulRecursion,        // in: recursion level, initially 0
                                 BOOL fRecurse)
{
    ULONG       ulrc = 0,
                ul;

    PSZ         pszSetupString;
    ULONG       ulSetupStringLen = 0;

    if (pszSetupString = _xwpQuerySetup(pobj, &ulSetupStringLen))
    {
        PSZ         pszTrueClassName = _wpGetTrueClassName(SOMClassMgrObject, pobj);

        CHAR        szFolderName[CCHMAXPATH];
        XSTRING     strTitle;
        ULONG       ulOfs;
        CHAR        cQuote = '\"';

        BOOL        fIsDisk = !strcmp(pszTrueClassName, G_pcszWPDisk);

        // get folder ID or name
        WPFolder    *pOwningFolder = _wpQueryFolder(pobj);
        PSZ         pszOwningFolderID = _wpQueryObjectID(pOwningFolder);
        if (pszOwningFolderID)
            strcpy(szFolderName, pszOwningFolderID);
        else
            _wpQueryFilename(pOwningFolder, szFolderName, TRUE);

        // special hack for line breaks in titles: "^"
        xstrInitCopy(&strTitle, _wpQueryTitle(pobj), 0);
        ulOfs = 0;
        while (xstrFindReplaceC(&strTitle, &ulOfs, "\r\n", "^"))
            ;
        ulOfs = 0;
        while (xstrFindReplaceC(&strTitle, &ulOfs, "\r", "^"))
            ;
        ulOfs = 0;
        while (xstrFindReplaceC(&strTitle, &ulOfs, "\n", "^"))
            ;

        // if we have a quote:
        if (strchr(strTitle.psz, '\"'))
            cQuote = '\'';

        // indent
        for (ul = 0; ul < ulRecursion; ul++)
            fprintf(RexxFile, "  ");

        if (fIsDisk)
            fprintf(RexxFile, "/* ");

        // write out object
        if (ulSetupStringLen)
            // we got setup:
            fprintf(RexxFile,
                    "rc = SysCreateObject(\"%s\", %c%s%c, \"%s\", \"%s\");",
                    pszTrueClassName,
                    cQuote,
                    strTitle.psz,
                    cQuote,
                    szFolderName,
                    pszSetupString);
        else
            // no setup string:
            fprintf(RexxFile,
                    "rc = SysCreateObject(\"%s\", %c%s%c, \"%s\");",
                    pszTrueClassName,
                    cQuote,
                    strTitle.psz,
                    cQuote,
                    szFolderName);

        if (fIsDisk)
            fprintf(RexxFile, " */ \n");
        else
            fprintf(RexxFile, "\n");

        ulrc++;

        _xwpFreeSetupBuffer(pobj, pszSetupString);
        xstrClear(&strTitle);

        // recurse for folders
        if (    (fRecurse)
             && (_somIsA(pobj, _WPFolder))
           )
        {
            BOOL    fFolderLocked = FALSE;
            TRY_LOUD(excpt1)
            {
                // rule out certain stupid special folder classes
                if (    strcmp(pszTrueClassName, G_pcszXWPFontFolder)
                     && strcmp(pszTrueClassName, G_pcszXWPTrashCan)
                     && strcmp(pszTrueClassName, "WPMinWinViewer")
                     && strcmp(pszTrueClassName, "WPHwManager")
                     && strcmp(pszTrueClassName, "WPTemplates")
                     && strcmp(pszTrueClassName, "WPNetgrp")
                   )
                {
                    if (fdrCheckIfPopulated(pobj, FALSE))
                    {
                        if (fFolderLocked = !_wpRequestFolderMutexSem(pobj, 5000))
                        {
                            WPObject *pSubObj = 0;
                            // V0.9.20 (2002-07-31) [umoeller]: now using get_pobjNext SOM attribute
                            for (   pSubObj = _wpQueryContent(pobj, NULL, QC_FIRST);
                                    pSubObj;
                                    pSubObj = *__get_pobjNext(pSubObj)
                                )
                            {
                                ulrc += WriteOutObjectSetup(RexxFile,
                                                            pSubObj,
                                                            ulRecursion + 1,
                                                            fRecurse);
                            }
                        }
                    }
                }
            }
            CATCH(excpt1)
            {
            }
            END_CATCH();

            if (fFolderLocked)
                _wpReleaseFolderMutexSem(pobj);
        }
    }

    // return total object count
    return ulrc;
}

/*
 *@@ fdrCreateObjectScript:
 *      creates an object package.
 *
 *      pllObjects is expected to contain plain WPObject*
 *      pointers of all objects to put into the package.
 *
 *      pcszRexxFile must be the fully qualified path name
 *      of the REXX .CMD file to be created.
 *
 *      flCreate can be any combination of:
 *
 *      --  SCRFL_RECURSE: recurse into subfolders.
 *
 *      This returns an OS/2 error code.
 *
 *@@added V0.9.9 (2001-04-04) [umoeller]
 */

APIRET objCreateObjectScript(WPObject *pObject,          // in: object to start with
                             PCSZ pcszRexxFile,   // in: file name of output REXX file
                             WPFolder *pFolderForFiles,  // in: if != NULL, icons etc. are put here
                             ULONG flCreate)             // in: flags
{
    APIRET arc = NO_ERROR;

    if (!pObject || !pcszRexxFile)
        arc = ERROR_INVALID_PARAMETER;
    else
    {
        FILE *RexxFile = fopen(pcszRexxFile,
                               "w");            // replace @@@

        if (!RexxFile)
            arc = ERROR_CANNOT_MAKE;
        else
        {
            WriteOutObjectSetup(RexxFile,
                                pObject,
                                0,
                                ((flCreate & SCRFL_RECURSE) != 0));
                    // this will recurse for folders

            fclose(RexxFile);
        }
    }

    return arc;
}

/* ******************************************************************
 *
 *   Object creation/destruction
 *
 ********************************************************************/

/*
 *@@ objReady:
 *      implementation for XFldObject::wpObjectReady.
 *
 *      Since apparently the dumbass IBM WPFolder::wpObjectReady
 *      does not call its parent, I have isolated this implementation
 *      into this function, which gets called from both
 *      XFldObject::wpObjectReady and XFoldser::wpObjectReady.
 *      We do check whether this has been called before though
 *      since maybe this WPS bug is not present with all
 *      implementations.
 *
 *      Preconditions: Call the parent method first. (As if that
 *      helped, since even IBM can't read their own docs.)
 *
 *@@added V0.9.19 (2002-04-02) [umoeller]
 *@@changed V0.9.20 (2002-07-25) [umoeller]: fixed excessive object ID creation
 */

VOID objReady(WPObject *somSelf,
              ULONG ulCode,
              WPObject* refObject)
{
    XFldObjectData *somThis = XFldObjectGetData(somSelf);

    // avoid doing this twice
    if (!(_flObject & OBJFL_INITIALIZED))
    {
        #if defined(DEBUG_SOMMETHODS) || defined(DEBUG_AWAKEOBJECTS)
            _Pmpf(("xo_wpObjectReady for %s (class %s), ulCode: %s",
                    _wpQueryTitle(somSelf),
                    _somGetName(_somGetClass(somSelf)),
                    (ulCode == OR_AWAKE) ? "OR_AWAKE"
                    : (ulCode == OR_FROMTEMPLATE) ? "OR_FROMTEMPLATE"
                    : (ulCode == OR_FROMCOPY) ? "OR_FROMCOPY"
                    : (ulCode == OR_NEW) ? "OR_NEW"
                    : (ulCode == OR_SHADOW) ? "OR_SHADOW"
                    : (ulCode == OR_REFERENCE) ? "OR_REFERENCE"
                    : "unknown code"
                 ));
         #endif

        // set flag
        _flObject |= OBJFL_INITIALIZED;

        // on my Warp 4 FP 10, this method does not get
        // called for WPFolder instances, so we override
        // WPFolder::wpObjectReady also; but we don't know
        // if this is so with all Warp versions, so we
        // better check (the worker thread checks for
        // duplicates, so there's no problem in posting
        // this twice)
        // V0.9.19 (2002-04-02) [umoeller] still the case
        // for eComStation 1.0, but this now gets called
        // explicitly, so kick out the check
        // if (!(_flFlags & OBJFL_WPFOLDER))
        // xthrPostWorkerMsg(WOM_ADDAWAKEOBJECT,
        //                   (MPARAM)somSelf,
        //                   MPNULL);
                // removed V0.9.20 (2002-07-25) [umoeller]

        // if this is a template, don't let the WPS make
        // it go dormant
        if (_wpQueryStyle(somSelf) & OBJSTYLE_TEMPLATE)
            _wpLockObject(somSelf);

        // if we were copied, nuke our backup object ID
        // because this got copied from the refobject...
        // and even though wpCopyObject apparently does
        // call wpSetObjectID(NULL), it does that before
        // the objReady call so that XFldObject::wpSetObjectID
        // does not kill the backup! This resulted in the
        // backup ID being stored for all objects created
        // from the "Create new" folder, for example, but
        // probably from every other copy or template on
        // the system as well. CHECKINI then reported tons
        // of wrong object IDs because the backup ID was
        // taken again for the real object ID after wakeup.
        // V0.9.20 (2002-07-25) [umoeller]
        if (    (ulCode == OR_FROMCOPY)
             || (ulCode == OR_FROMTEMPLATE)
           )
            wpshStore(somSelf,
                      &_pWszOriginalObjectID,
                      NULL,
                      NULL);
    }
}

/*
 *@@ objRefreshUseItems:
 *      refresh all views of the object with a new
 *      title. Part of the implementation for
 *      XFldObject::wpSetTitle and XFldObject::wpSetIconHandle.
 *
 *      This operates in two modes:
 *
 *      --  If pszNewTitleCopy != NULL, this refreshes the
 *          _titles_ of the object views.
 *
 *      --  If hptrNewIcon != NULL, this refreshes the
 *          _icons_ of the object views.
 *
 *      Preconditions:
 *
 *      --  Caller must hold the object semaphore.
 *
 *@@added V0.9.16 (2002-01-04) [umoeller]
 *@@changed V0.9.20 (2002-07-31) [umoeller]: added icon support
 */

VOID objRefreshUseItems(WPObject *somSelf,
                        PSZ pszNewTitleCopy,        // in: new title or NULL
                        HPOINTER hptrNewIcon)       // in: new icon or NULLHANDLE
{
    PUSEITEM    pUseItem;

    USHORT      flInvalidate;

    if (pszNewTitleCopy)
        // if we are refreshing titles, invalidate those
        flInvalidate  = CMA_TEXTCHANGED;
    else if (hptrNewIcon)
        flInvalidate =    CMA_ERASE             // repaint icon background
                        | CMA_NOREPOSITION      // CMA_REPOSITION is not needed for icons
                        | CMA_NOTEXTCHANGED;

    for (pUseItem = _wpFindUseItem(somSelf, USAGE_RECORD, NULL);
         pUseItem;
         pUseItem = _wpFindUseItem(somSelf, USAGE_RECORD, pUseItem))
    {
        // USAGE_RECORD specifies where this object is
        // currently inserted
        PRECORDITEM pRecordItem = (PRECORDITEM)(pUseItem + 1);

        // refresh the record in the useitem;
        // the WPS shares records between containers
        WinSendMsg(pRecordItem->hwndCnr,
                   CM_QUERYRECORDINFO,
                   (MPARAM)&pRecordItem->pRecord,
                   (MPARAM)1);

        // and refresh the view
        WinSendMsg(pRecordItem->hwndCnr,
                   CM_INVALIDATERECORD,
                   (MPARAM)&pRecordItem->pRecord,
                   MPFROM2SHORT(1,
                                flInvalidate));     // set above V0.9.20 (2002-07-31) [umoeller]

        // @@todo resort the folder
    }

    // refresh open views of this object
    for (pUseItem = _wpFindUseItem(somSelf, USAGE_OPENVIEW, NULL);
         pUseItem;
         pUseItem = _wpFindUseItem(somSelf, USAGE_OPENVIEW, pUseItem))
    {
        PVIEWITEM   pViewItem = (PVIEWITEM)(pUseItem + 1);
        HWND        hwndView;
        // this can also be a HAPP, so check if this really
        // is a window
        if (    (hwndView = pViewItem->handle)
             && (WinIsWindow(0, // G_habThread1,
                             hwndView))
           )
        {
            if (pszNewTitleCopy)
            {
                HWND hwndTitleBar;
                HSWITCH hsw;
                // in "text changed" mode, refresh the title bar
                if (    (hwndTitleBar = WinWindowFromID(hwndView,
                                                        FID_TITLEBAR))
                     && (WinSetWindowText(hwndTitleBar,
                                          pszNewTitleCopy))
                        // update the switch handle also, if we have one
                     && (hsw = WinQuerySwitchHandle(hwndView, 0))
                   )
                {
                    SWCNTRL swc;
                    WinQuerySwitchEntry(hsw, &swc);
                    strhncpy0(swc.szSwtitle,
                              pszNewTitleCopy,
                              sizeof(swc.szSwtitle));
                    WinChangeSwitchEntry(hsw, &swc);
                }
            } // end if (pszNewTitleCopy)
            else if (hptrNewIcon)
            {
                HWND hwndSysMenu;
                if (hwndSysMenu = WinWindowFromID(hwndView,
                                                  FID_SYSMENU))
                    WinSendMsg(hwndView,
                               WM_SETICON,
                               (MPARAM)hptrNewIcon,
                               0);
            }
        }
    }

    // refresh awake shadows pointing to us
    for (pUseItem = _wpFindUseItem(somSelf, USAGE_LINK, NULL);
         pUseItem;
         pUseItem = _wpFindUseItem(somSelf, USAGE_LINK, pUseItem))
    {
        PLINKITEM pLinkItem = (PLINKITEM)(pUseItem + 1);
        WPShadow *pShadow;

        if (pShadow = pLinkItem->LinkObj)
        {
            if (pszNewTitleCopy)
                _wpSetShadowTitle(pShadow,
                                  pszNewTitleCopy);
            else if (hptrNewIcon)
                _wpSetIcon(pShadow,
                           hptrNewIcon);
        }
    }
}

/* ******************************************************************
 *
 *   Object linked lists
 *
 ********************************************************************/

/*
 *@@ LockObjectsList:
 *      locks G_hmtxFolderLists. Creates the mutex on
 *      the first call.
 *
 *      Returns TRUE if the mutex was obtained.
 *
 *@@added V0.9.7 (2001-01-18) [umoeller]
 */

static BOOL LockObjectsList(VOID)
{
    if (G_hmtxObjectsLists)
        return !DosRequestMutexSem(G_hmtxObjectsLists, SEM_INDEFINITE_WAIT);

    // first call:
    return !DosCreateMutexSem(NULL,
                              &G_hmtxObjectsLists,
                              0,
                              TRUE);
}

/*
 *@@ UnlockObjectsList:
 *      the reverse to LockObjectsLists.
 *
 *@@added V0.9.7 (2001-01-18) [umoeller]
 */

static VOID UnlockObjectsList(VOID)
{
    DosReleaseMutexSem(G_hmtxObjectsLists);
}

/*
 *@@ WriteObjectsList:
 *
 *      This creates a handle for each object on the
 *      list.
 *
 *      Preconditions: caller must lock the list.
 *
 *@@added V0.9.7 (2001-01-18) [umoeller]
 *@@changed V0.9.9 (2001-03-27) [umoeller]: added OBJECTLIST encapsulation
 */

static BOOL WriteObjectsList(POBJECTLIST pll,
                             PCSZ pcszIniKey)
{
    BOOL brc = FALSE;

    if (pll->fLoaded)
    {
        PLISTNODE pNode;
        if (pNode = lstQueryFirstNode(&pll->ll))
        {
            // list is not empty: recompose string
            XSTRING strTemp;
            xstrInit(&strTemp, 100);

            while (pNode)
            {
                CHAR szHandle[30];
                WPObject *pobj = (WPObject*)pNode->pItemData;
                sprintf(szHandle,
                        "%lX ",         // note the space
                        _wpQueryHandle(pobj));

                xstrcat(&strTemp, szHandle, 0);

                pNode = pNode->pNext;
            }

            brc = PrfWriteProfileString(HINI_USERPROFILE,
                                        (PSZ)INIAPP_XWORKPLACE, (PSZ)pcszIniKey,
                                        strTemp.psz);

            xstrClear(&strTemp);
        }
        else
        {
            // list is empty: remove
            brc = PrfWriteProfileData(HINI_USERPROFILE,
                                      (PSZ)INIAPP_XWORKPLACE, (PSZ)pcszIniKey,
                                      NULL, 0);
        }
    }

    return brc;
}

/*
 *@@ LoadObjectsList:
 *      adds WPObject pointers to pll according to the
 *      specified INI key.
 *
 *      If (ulListFlag != 0), this invokes
 *      WPObject::xwpModifyFlags to set that flag
 *      on each object that was added to the list.
 *
 *      Preconditions: caller must lock the list.
 *
 *@@added V0.9.7 (2001-01-18) [umoeller]
 *@@changed V0.9.9 (2001-03-19) [pr]: tidied up
 *@@changed V0.9.9 (2001-03-27) [umoeller]: added OBJECTLIST encapsulation
 *@@changed V0.9.16 (2001-10-11) [umoeller]: rewritten
 *@@changed V0.9.16 (2001-10-24) [umoeller]: added fixes for duplicate objects on list, which lead to endlessly looping startup folders
 */

static BOOL LoadObjectsList(POBJECTLIST pll,
                            ULONG ulListFlag,          // in: list flag for xwpModifyFlags
                            PCSZ pcszIniKey)
{
    BOOL        brc = FALSE;
    PSZ         pszHandles = NULL;

    if (pll->fLoaded)
        return FALSE;  // V0.9.16 (2001-10-24) [umoeller]

    TRY_LOUD(excpt1)
    {
        BOOL fReSave = FALSE;

        if (pszHandles = prfhQueryProfileData(HINI_USERPROFILE,
                                              INIAPP_XWORKPLACE,
                                              pcszIniKey,
                                              NULL))
        {
            PCSZ pTemp = pszHandles;
            ULONG ulCharsLeft;

            while (     (pTemp)
                     && (*pTemp)
                  )
            {
                HOBJECT          hObject;
                if (hObject = strtol(pTemp,
                                     NULL,
                                     16))
                {
                    PCSZ        p;
                    WPObject    *pobj;
                    if (    (pobj = _wpclsQueryObject(_WPObject, hObject))
                         && (wpshCheckObject(pobj))
                       )
                    {
                        // make sure the object is NOT on the list
                        // yet!! for some reason, we end up with objects
                        // being on the list twice or something.
                        // V0.9.16 (2001-10-24) [umoeller]

                        if (-1 == lstIndexFromItem(&pll->ll, pobj))
                        {
                            // not on list yet:
                            // add it now

                            // object is already locked
                            // _Pmpf(("    got new obj %lX = %s", pobj, _wpQueryTitle(pobj)));
                            lstAppendItem(&pll->ll,
                                          pobj);
                            if (ulListFlag)
                                // set list notify flag
                                _xwpModifyFlags(pobj,
                                                     ulListFlag,        // set
                                                     ulListFlag);       // mask
                        }
                        else
                        {
                            // we have duplicate objects on the list:
                            // save the fixed list back to OS2.INI
                            // V0.9.16 (2001-10-24) [umoeller]
                            fReSave = TRUE;
                            // _Pmpf(("   got duplicate obj %lX = %s", pobj, _wpQueryTitle(pobj)));
                        }
                    }

                    // find next space
                    if (p = strchr(pTemp, ' '))
                    {
                        // skip spaces
                        while (*p && *p == ' ')
                            p++;
                        pTemp = p;          // points to either next non-space
                                            // or null char now
                    }
                    else
                        // no more spaces:
                        break;
                }
                else
                    // cannot get object string:
                    break;
            };
        }

        pll->fLoaded = TRUE;

        if (fReSave)        // V0.9.16 (2001-10-24) [umoeller]
        {
            // _Pmpf(("  RESAVE, saving modified list back to OS2.INI"));
            WriteObjectsList(pll,
                             pcszIniKey);
        }
    }
    CATCH(excpt1)
    {
        brc = FALSE;
    } END_CATCH();

    if (pszHandles)
        free(pszHandles);

    return brc;
}

/*
 *@@ objAddToList:
 *      appends or removes the given object to or from
 *      the given object list.
 *
 *      An "object list" is an abstract concept of a list
 *      of object which is stored in OS2.INI (in the
 *      "XWorkplace" section under the pcszIniKey key).
 *
 *      If fInsert == TRUE, somSelf is added to the list.
 *      If fInsert == FALSE, somSelf is removed from the list.
 *
 *      Returns TRUE if the list was changed. In that case,
 *      the list is rewritten to the specified INI key.
 *
 *      The list is protected by a global mutex semaphore,
 *      so this is thread-safe.
 *
 *      WARNING: If an object is added to such a list, the
 *      caller must make sure that the object gets removed
 *      when the object goes dormant (e.g. when it's deleted).
 *
 *      Also, because of the way the "query" funcs work here,
 *      every object can only be added to a list once.
 *
 *      This is used from the XFolder methods for
 *      "favorite" and "quick-open" folders. The linked
 *      list roots are stored in classes\xfldr.c.
 *
 *@@added V0.9.0 (99-11-16) [umoeller]
 *@@changed V0.9.7 (2001-01-18) [umoeller]: made this generic for all objs... renamed from fdr*
 *@@changed V0.9.7 (2001-01-18) [umoeller]: removed mallocs(), this wasn't needed
 *@@changed V0.9.9 (2001-01-29) [lafaix]: wrong object set, fixed
 *@@changed V0.9.9 (2001-03-19) [pr]: lock/unlock objects on the lists
 *@@changed V0.9.9 (2001-03-27) [umoeller]: added OBJECTLIST encapsulation
 *@@changed V0.9.14 (2001-07-31) [pr]: fixed confusing code
 */

BOOL objAddToList(WPObject *somSelf,    // in: object to add or remove
                  POBJECTLIST pll,      // in: linked list of WPObject* pointers
                  BOOL fInsert,         // in: add or remove?
                  PCSZ pcszIniKey,      // in: INI key for saving the list
                  ULONG ulListFlag)     // in: list flag for xwpModifyFlags
{
    BOOL    brc = FALSE,
            fLocked = FALSE;

    TRY_LOUD(excpt1)
    {
        if (fLocked = LockObjectsList())
        {
            PLISTNODE   pNode = 0,
                        pNodeFound = 0;
            WPObject    *pobj;

            if (!pll->fLoaded)
                // if the list has not yet been built
                // yet, we will do this now
                LoadObjectsList(pll,
                                ulListFlag,
                                pcszIniKey);

            // find object on list
            pNode = lstQueryFirstNode(&pll->ll);
            while (pNode)
            {
                pobj = pNode->pItemData;

                if (pobj == somSelf)
                {
                    pNodeFound = pNode;
                    break;
                }

                pNode = pNode->pNext;
            }

            if (fInsert)
            {
                // insert mode:
                if (!pNodeFound)
                {
                    // lock object to stop it going dormant
                    _wpLockObject(somSelf);
                    // not on list yet: append
                    lstAppendItem(&pll->ll,
                                  somSelf);

                    if (ulListFlag)
                        // set list notify flag
                        _xwpModifyFlags(somSelf, // V0.9.9 (2001-01-29) [lafaix]
                                             ulListFlag,        // set
                                             ulListFlag);       // mask

                    brc = TRUE;
                }
            }
            else
            {
                // remove mode:
                if (pNodeFound)
                {
                    lstRemoveNode(&pll->ll,
                                  pNodeFound);
                    // unlock object to allow it to go dormant
                    _wpUnlockObject(somSelf);

                    if (ulListFlag)
                        // unset list notify flag
                        _xwpModifyFlags(somSelf,
                                             0,                 // clear
                                             ulListFlag);       // mask

                    brc = TRUE;
                }
            }

            if (brc)
            {
                // list changed:
                // write list to INI as a string of handles
                WriteObjectsList(pll, pcszIniKey);
            }
        }
        else
            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                   "hmtxFolderLists request failed.");
    }
    CATCH(excpt1)
    {
        brc = FALSE;
    } END_CATCH();

    if (fLocked)
        UnlockObjectsList();

    return brc;
}

/*
 *@@ objIsOnList:
 *      returns TRUE if the given object is on the
 *      given object list.
 *
 *      The list is protected by a global mutex semaphore,
 *      so this is thread-safe.
 *
 *      This is used from the XFolder methods for
 *      "favorite" and "quick-open" folders. The linked
 *      list roots are stored in classes\xfldr.c.
 *
 *@@added V0.9.0 (99-11-16) [umoeller]
 *@@changed V0.9.7 (2001-01-18) [umoeller]: made this generic for all objs... renamed from fdr*
 *@@changed V0.9.7 (2001-01-18) [umoeller]: removed mallocs(), this wasn't needed
 *@@changed V0.9.9 (2001-03-27) [umoeller]: added OBJECTLIST encapsulation
 */

BOOL objIsOnList(WPObject *somSelf,
                 POBJECTLIST pll)     // in: linked list of WPObject* pointers
{
    BOOL                 rc = FALSE,
                         fLocked = FALSE;

    TRY_LOUD(excpt1)
    {
        if (fLocked = LockObjectsList())
        {
            PLISTNODE pNode = lstQueryFirstNode(&pll->ll);
            while (pNode)
            {
                WPObject *pobj = (WPObject*)pNode->pItemData;
                if (pobj == somSelf)
                {
                    rc = TRUE;
                    break;
                }
                else
                    pNode = pNode->pNext;
            }
        }
        else
            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                       "hmtxFolderLists request failed.");
    }
    CATCH(excpt1)
    {
        rc = FALSE;
    } END_CATCH();

    if (fLocked)
        UnlockObjectsList();

    return (rc);
}

/*
 *@@ objEnumList:
 *      this enumerates objects on the given list.
 *
 *      If (pObject == NULL), the first object on the list
 *      is returned, otherwise the object which comes
 *      after pObject in the list. If no (next) object
 *      is found, NULL is returned.
 *
 *      The list is protected by a global mutex semaphore,
 *      so this is thread-safe.
 *
 *      This is used from the XFolder methods for
 *      "favorite" and "quick-open" folders. The linked
 *      list roots are stored in classes\xfldr.c.
 *
 *@@added V0.9.0 (99-11-16) [umoeller]
 *@@changed V0.9.7 (2001-01-18) [umoeller]: made this generic for all objs... renamed from fdr*
 *@@changed V0.9.7 (2001-01-18) [umoeller]: removed mallocs(), this wasn't needed
 *@@changed V0.9.9 (2001-03-27) [umoeller]: added OBJECTLIST encapsulation
 */

WPObject* objEnumList(POBJECTLIST pll,        // in: linked list of WPObject* pointers
                      WPObject *pObjectFind,
                      PCSZ pcszIniKey,
                      ULONG ulListFlag)     // in: list flag for xwpModifyFlags
{
    WPObject    *pObjectFound = NULL;
    BOOL        fLocked = FALSE;

    // _PmpfF(("entering, pObjectFind = %s",
       //          (pObjectFind) ? _wpQueryTitle(pObjectFind) : "NULL"));

    TRY_LOUD(excpt1)
    {
        if (fLocked = LockObjectsList())
        {
            PLISTNODE       pNode = NULL;

            if (!pll->fLoaded)
                // if the list of favorite folders has not yet been built
                // yet, we will do this now
                LoadObjectsList(pll,
                                ulListFlag,
                                pcszIniKey);

            // OK, we should now have a valid list
            pNode = lstQueryFirstNode(&pll->ll);

            if (pObjectFind)
            {
                // obj given as param: look for this obj
                // and return the following in the list
                while (pNode)
                {
                    WPObject *pobj = (WPObject*)pNode->pItemData;

                    // _Pmpf(("     looping, pobj = %s",
                       //          (pobj) ? _wpQueryTitle(pobj) : "NULL"));

                    if (pobj == pObjectFind)
                    {
                        // return node after this
                        pNode = pNode->pNext;
                        break;
                    }

                    pNode = pNode->pNext;
                }
            }
            // else pObject == NULL: pNode still points to first

            if (pNode)
                // found something (either first or next):
                pObjectFound = (WPObject*)pNode->pItemData;
        }
        else
            cmnLog(__FILE__, __LINE__, __FUNCTION__,
                       "hmtxFolderLists request failed.");
    }
    CATCH(excpt1)
    {
        pObjectFound = NULL;
    } END_CATCH();

    if (fLocked)
        UnlockObjectsList();

    return (pObjectFound);
}

/* ******************************************************************
 *
 *   Object handles cache
 *
 ********************************************************************/

/*
 *@@ LockHandlesCache:
 *
 *@@added V0.9.9 (2001-04-02) [umoeller]
 */

static BOOL LockHandlesCache(VOID)
{
    if (G_hmtxHandlesCache)
        return !DosRequestMutexSem(G_hmtxHandlesCache, SEM_INDEFINITE_WAIT);

    // first call:
    if (!DosCreateMutexSem(NULL,
                           &G_hmtxHandlesCache,
                           0,
                           TRUE))
    {
        // initialize tree
        treeInit(&G_HandlesCacheRoot,
                 &G_lHandlesCacheItemsCount);

        return TRUE;
    }

    return FALSE;
}

/*
 *@@ UnlockHandlesCache:
 *
 *@@added V0.9.9 (2001-04-02) [umoeller]
 */

static VOID UnlockHandlesCache(VOID)
{
    DosReleaseMutexSem(G_hmtxHandlesCache);
}

/*
 *@@ CheckShrinkCache:
 *      checks if the cache has too many objects
 *      and shrinks it if needed. Returns the
 *      no. of objects removed.
 *
 *      Since the cache maintains a reference
 *      item per node, it can delete the oldest
 *      objects.
 *
 *      Preconditions:
 *
 *      --  Caller must have locked the cache.
 *
 *@@added V0.9.9 (2001-04-02) [umoeller]
 *@@changed V0.9.20 (2002-07-25) [umoeller]: this deleted the wrong object, fixed
 */

static ULONG CheckShrinkCache(VOID)
{
    ULONG   ulDeleted = 0;
    LONG    lObjectsToDelete = G_lHandlesCacheItemsCount - CACHE_ITEM_LIMIT;

    if (lObjectsToDelete > 0)
    {
        while (lObjectsToDelete--)
        {
            POBJTREENODE    pOldest = (POBJTREENODE)treeFirst(G_HandlesCacheRoot),
                            pNode = pOldest;
            while (pNode)
            {
                if (pNode->ulReferenced < pOldest->ulReferenced)
                    // this node is older:
                    pOldest = pNode;

                pNode = (POBJTREENODE)treeNext((TREE*)pNode);
            }

            // now we know the oldest node;
            // delete it
            if (pOldest)
            {
                if (pOldest->pObject == G_pobjLast)
                {
                    // V0.9.20 (2002-07-25) [umoeller]
                    G_pobjLast = NULL;
                    G_hobjLast = NULLHANDLE;
                }

                treeDelete(&G_HandlesCacheRoot,
                           &G_lHandlesCacheItemsCount,
                           (TREE*)pOldest); // fixed pNode); V0.9.20 (2002-07-25) [umoeller]
                // unset list notify flag
                _xwpModifyFlags(pOldest->pObject,      // V0.9.20 (2002-07-25) [umoeller]
                                     OBJLIST_HANDLESCACHE,
                                     0);
                free(pOldest);
            }
        }
    }

    return (ulDeleted);
}

/*
 *@@ objFindObjFromHandle:
 *      fast find-object function, which implements a
 *      cache for frequently used objects.
 *
 *      This is way faster than running _wpclsQueryObject
 *      and is therefore used with the extended
 *      file associations.
 *
 *      This uses a red-black balanced binary tree for
 *      finding the object from the HOBJECT. If the object
 *      is not found, _wpclsQueryObject is invoked, and
 *      the object is added to the tree.
 *
 *      As a result, this is especially helpful if you
 *      need the same few objects many times, as with
 *      the extended file assocs. They query the same
 *      maybe 20 HOBJECTs all the time, and possibly for
 *      a thousand data files.
 *
 *      Since I implemented this, folder populating has
 *      become a blitz. Extended assocs are now even
 *      faster than the standard WPS assocs. Quite
 *      unbelievable what 30 lines of code here and
 *      there could do to some other WPS internals...
 *
 *@@added V0.9.9 (2001-04-02) [umoeller]
 */

WPObject* objFindObjFromHandle(HOBJECT hobj)
{
    WPObject    *pobjReturn = NULL;
    BOOL        fLocked = FALSE;

    // lock the cache
    if (fLocked = LockHandlesCache())
    {
        POBJTREENODE pNode;

        // we frequently get here for the same object many times,
        // so this is faster than going through the tree
        // V0.9.20 (2002-07-25) [umoeller]
        if (    (G_hobjLast)
             && (G_hobjLast == hobj)
           )
        {
            pobjReturn = G_pobjLast;
        }
        else if (pNode = (POBJTREENODE)treeFind(G_HandlesCacheRoot,
                                                hobj,
                                                treeCompareKeys))
        {
            // was in cache:
            G_hobjLast = hobj;
            G_pobjLast = pobjReturn = pNode->pObject;
            // store system uptime as last reference
            pNode->ulReferenced = doshQuerySysUptime();
        }
        else
        {
            // was not in cache:
            // run wpclsQueryObject
            static M_XFldObject *pObjectClass = NULL;

            if (!pObjectClass)
                pObjectClass = _WPObject;

            // do not keep the cache locked while we're calling
            // wpclsQueryObject... this is a very expensive
            // operation and might cause deadlocks
            // V0.9.20 (2002-07-25) [umoeller]
            UnlockHandlesCache();
            fLocked = FALSE;

            if (    (pobjReturn = _wpclsQueryObject(pObjectClass, hobj))
                 && (fLocked = LockHandlesCache())
               )
            {
                // valid handle:

                // check if the cache needs to be shrunk
                CheckShrinkCache();

                // add new obj to cache
                if (pNode = NEW(OBJTREENODE))
                {
                    pNode->Tree.ulKey = hobj;
                    pNode->pObject = pobjReturn;
                    // store system uptime as last reference
                    pNode->ulReferenced = doshQuerySysUptime();

                    treeInsert(&G_HandlesCacheRoot,
                               &G_lHandlesCacheItemsCount,
                               (TREE*)pNode,
                               treeCompareKeys);

                    // set list-notify flag so we can
                    // kill this node, should the obj get deleted
                    // (objRemoveFromHandlesCache)
                    _xwpModifyFlags(pobjReturn,
                                         OBJLIST_HANDLESCACHE,
                                         OBJLIST_HANDLESCACHE);
                }

                G_hobjLast = hobj;
                G_pobjLast = pobjReturn;
            }
        }

        if (fLocked)
            UnlockHandlesCache();
    }

    return (pobjReturn);
}

/*
 *@@ objRemoveFromHandlesCache:
 *      removes the specified object from the
 *      handles cache. Called from WPObject::wpUnInitData
 *      only when an object from the cache goes dormant.
 *
 *@@added V0.9.9 (2001-04-02) [umoeller]
 */

VOID objRemoveFromHandlesCache(WPObject *somSelf)
{
    // lock the cache
    if (LockHandlesCache())
    {
        // this is terminally slow, but what the heck...
        // this rarely gets called
        POBJTREENODE pNode = (POBJTREENODE)treeFirst(G_HandlesCacheRoot);
        while (pNode)
        {
            if (pNode->pObject == somSelf)
            {
                if (pNode->pObject == G_pobjLast)
                {
                    // V0.9.20 (2002-07-25) [umoeller]
                    G_hobjLast = NULLHANDLE;
                    G_pobjLast = NULL;
                }

                treeDelete(&G_HandlesCacheRoot,
                           &G_lHandlesCacheItemsCount,
                           (TREE*)pNode);
                free(pNode);
                break;
            }

            pNode = (POBJTREENODE)treeNext((TREE*)pNode);
        }

        UnlockHandlesCache();
    }
}

/* ******************************************************************
 *
 *   Dirty objects list
 *
 ********************************************************************/

/*
 *@@ LockDirtyList:
 *
 *@@added V0.9.9 (2001-04-02) [umoeller]
 */

static BOOL LockDirtyList(VOID)
{
    if (G_hmtxDirtyList)
        return !DosRequestMutexSem(G_hmtxDirtyList, SEM_INDEFINITE_WAIT);

    // first call:
    if (!DosCreateMutexSem(NULL,
                           &G_hmtxDirtyList,
                           0,
                           TRUE))
    {
        // initialize tree
        treeInit(&G_DirtyList,
                 &G_lDirtyListItemsCount);
        return TRUE;
    }

    return FALSE;
}

/*
 *@@ UnlockDirtyList:
 *
 *@@added V0.9.9 (2001-04-02) [umoeller]
 */

static VOID UnlockDirtyList(VOID)
{
    DosReleaseMutexSem(G_hmtxDirtyList);
}

/*
 *@@ objAddToDirtyList:
 *      adds the given object to the "dirty" list, which
 *      is really a binary tree for speed.
 *
 *      This gets called from XFldObject::wpSaveDeferred.
 *      See remarks there.
 *
 *      Returns TRUE if the object was added or FALSE if
 *      not, e.g. because the object was already on the
 *      list.
 *
 *@@added V0.9.9 (2001-04-04) [umoeller]
 *@@changed V0.9.11 (2001-04-18) [umoeller]: added OBJLIST_DIRTYLIST list flag
 *@@changed V0.9.14 (2001-08-01) [umoeller]: fixed memory leak
 */

BOOL objAddToDirtyList(WPObject *pobj)
{
    BOOL    brc = FALSE;

    BOOL    fLocked = FALSE;

    TRY_LOUD(excpt1)
    {
        if (!ctsIsTransient(pobj))
        {
            if (fLocked = LockDirtyList())
            {
                // add new obj to cache;
                // we can use a plain TREE node (no special struct
                // definition needed) and just use the "id" field
                // for the pointer
                TREE *pNode;
                if (pNode = NEW(TREE))
                {
                    pNode->ulKey = (ULONG)pobj;

                    if (brc = (!treeInsert(&G_DirtyList,
                                           &G_lDirtyListItemsCount,
                                           pNode,
                                           treeCompareKeys)))
                    {
                        /*
                        _PmpfF(("added obj 0x%lX (%s, class %s)",
                                            pobj,
                                            _wpQueryTitle(pobj),
                                            _somGetClassName(pobj) ));
                        _Pmpf(("  now %d objs on list", G_lDirtyListItemsCount ));
                          */

                        // note that we do not need an object list flag
                        // here because the WPS automatically invokes
                        // wpSaveImmediate on "dirty" objects during
                        // wpMakeDormant processing; as a result,
                        // objRemoveFromDirtyList will also get called
                        // automatically when the object goes dormant

                        // WRONG... this is not true for WPAbstracts, and
                        // we get tons of exceptions on XShutdown save-objects
                        // then. V0.9.11 (2001-04-18) [umoeller]
                        // so set list-notify flag so we can
                        // kill this node, should the obj get deleted
                        _xwpModifyFlags(pobj,
                                             OBJLIST_DIRTYLIST,
                                             OBJLIST_DIRTYLIST);
                    }
                    else
                        free(pNode);        // V0.9.14 (2001-08-01) [umoeller]

                    // else
                        // already on list:
                        // _PmpfF(("DID NOT ADD obj 0x%lX (%s)", pobj, _wpQueryTitle(pobj) ));
                }
            }
        }
    }
    CATCH(excpt1)
    {
        brc = FALSE;
    } END_CATCH();

    if (fLocked)
        UnlockDirtyList();

    return brc;
}

/*
 *@@ objRemoveFromDirtyList:
 *      removes the specified object from the "dirty"
 *      list. See objAddToDirtyList.
 *
 *      This gets called from XFldObject::wpSaveImmediate.
 *      See remarks there. Since that method doesn't always
 *      get called for WPAbstracts, we have some objects
 *      on this list which will always remain there... but
 *      never mind, it shouldn't hurt if we save those on
 *      shutdown. It's still better than saving all awake
 *      objects.
 *
 *      Returns TRUE if the object was found and removed.
 *
 *@@added V0.9.9 (2001-04-04) [umoeller]
 *@@changed V0.9.11 (2001-04-18) [umoeller]: added OBJLIST_DIRTYLIST list flag
 */

BOOL objRemoveFromDirtyList(WPObject *pobj)
{
    BOOL    brc = FALSE;

    BOOL    fLocked = FALSE;

    TRY_LOUD(excpt1)
    {
        if (fLocked = LockDirtyList())
        {
            TREE *pNode;
            if (pNode = treeFind(G_DirtyList,
                                 (ULONG)pobj,
                                 treeCompareKeys))
            {
                // was on list:
                treeDelete(&G_DirtyList,
                           &G_lDirtyListItemsCount,
                           pNode);
                free(pNode);

                /* _PmpfF(("removed obj 0x%lX (%s, class %s), %d remaining",
                                    pobj,
                                    _wpQueryTitle(pobj),
                                    _somGetClassName(pobj),
                                    G_lDirtyListItemsCount ));
                   */

                // unset object's "dirty" list flag
                _xwpModifyFlags(pobj,
                                     OBJLIST_DIRTYLIST,
                                     0);

                brc = TRUE;
            }
        }
    }
    CATCH(excpt1)
    {
        brc = FALSE;
    } END_CATCH();

    if (fLocked)
        UnlockDirtyList();

    return brc;
}

/*
 *@@ objQueryDirtyObjectsCount:
 *      returns the no. of currently dirty objects.
 *
 *@@added V0.9.9 (2001-04-04) [umoeller]
 */

ULONG objQueryDirtyObjectsCount(VOID)
{
    ULONG ulrc = 0;

    if (LockDirtyList())
    {
        ulrc = G_lDirtyListItemsCount;

        UnlockDirtyList();
    }

    return (ulrc);
}

/*
 *@@ objForAllDirtyObjects:
 *      invokes the specified callback on all objects on
 *      the "dirty" list. Starting with V0.9.9, this is
 *      used during XShutdown to save all dirty objects.
 *
 *      The callback must have the following prototype:
 *
 +      BOOL _Optlink fnCallback(WPObject *pobjThis,
 +                               ULONG ulIndex,
 +                               ULONG cObjects,
 +                               PVOID pvUser);
 +
 *      It will receive:
 *
 *      --  pobjThis: current object.
 *
 *      --  ulIndex: list index of current object, starting from 0.
 *
 *      --  cObjects: total objects on the list.
 *
 +      --  pvUser: what was passed to this function.
 *
 *      It is safe to call wpSaveImmediate from the callback
 *      (which will remove the object from the "dirty" list
 *      being processed!) because we build a copy of the dirty
 *      list internally before running the callback on the list.
 *
 *      WARNING: Do not play around with threads in the callback.
 *      The "dirty" list is locked while the callback is running,
 *      so only the callback thread may invoke wpSaveImmediate.
 *      Keep in mind that WinSendMsg can cause a thread switch.
 *
 *      Returns the no. of objects for which the callback
 *      returned TRUE.
 *
 *@@added V0.9.9 (2001-04-04) [umoeller]
 *@@changed V0.9.9 (2001-04-05) [umoeller]: now using treeBuildArray
 */

ULONG objForAllDirtyObjects(FNFORALLDIRTIESCALLBACK *pCallback,  // in: callback function
                            PVOID pvUserForCallback)    // in: user param for callback
{
    ULONG   ulrc = 0;

    BOOL    fLocked = FALSE;

    TRY_LOUD(excpt1)
    {
        if (fLocked = LockDirtyList())
        {
            // since wpSaveImmediate will call objRemoveFromDirtyList
            // from our XFldObject::wpSaveImmediate override, we cannot
            // simply run through the "dirty" tree and go for treeNext,
            // since the tree will be rebalanced with every save...
            // build an array instead and run the callback on the array.

            LONG        cObjects = G_lDirtyListItemsCount;
            TREE        **papNodes;
            if (papNodes = treeBuildArray(G_DirtyList, // V0.9.9 (2001-04-05) [umoeller]
                                          &cObjects))
            {
                if (cObjects == G_lDirtyListItemsCount)
                {
                    ULONG   ul;
                    for (ul = 0;
                         ul < cObjects;
                         ul++)
                    {
                        if (pCallback((WPObject*)(papNodes[ul]->ulKey),    // object ptr
                                      ul,
                                      cObjects,
                                      pvUserForCallback))
                                // if this calls wpSaveImmediate, this might kill
                                // the tree node thru objRemoveFromDirtyList!
                            ulrc++;
                    }
                }
                else
                    cmnLog(__FILE__, __LINE__, __FUNCTION__,
                           "Tree node count mismatch. G_lDirtyListItemsCount is %d, treeBuildArray found %d.",
                           G_lDirtyListItemsCount, cObjects);

                free(papNodes);
            }
        }
    }
    CATCH(excpt1) { } END_CATCH();

    if (fLocked)
        UnlockDirtyList();

    return (ulrc);
}

/* ******************************************************************
 *
 *   Object hotkeys
 *
 ********************************************************************/

/*
 *@@ objFindHotkey:
 *      this searches an array of GLOBALHOTKEY structures
 *      for the given object handle (as returned by
 *      hifQueryObjectHotkeys) and returns a pointer
 *      to the array item or NULL if not found.
 *
 *      Used by objQueryObjectHotkey and objSetObjectHotkey.
 */

PGLOBALHOTKEY objFindHotkey(PGLOBALHOTKEY pHotkeys, // in: array returned by hifQueryObjectHotkeys
                            ULONG cHotkeys,        // in: array item count (_not_ array size!)
                            HOBJECT hobj)
{
    PGLOBALHOTKEY   prc = NULL;
    ULONG   ul = 0;
    // go thru array of hotkeys and find matching item
    PGLOBALHOTKEY pHotkeyThis = pHotkeys;
    for (ul = 0;
         ul < cHotkeys;
         ul++)
    {
        if (pHotkeyThis->ulHandle == hobj)
        {
            prc = pHotkeyThis;
            break;
        }
        pHotkeyThis++;
    }

    return (prc);
}

/*
 *@@ objQueryObjectHotkey:
 *      implementation for XFldObject::xwpQueryObjectHotkey.
 */

BOOL objQueryObjectHotkey(WPObject *somSelf,
                          XFldObject_POBJECTHOTKEY pHotkey)
{
    PGLOBALHOTKEY   pHotkeys;
    ULONG           cHotkeys = 0;
    BOOL            brc = FALSE;

    // XFldObjectData *somThis = XFldObjectGetData(somSelf);

    if (    (pHotkey)
         && (pHotkeys = hifQueryObjectHotkeys(&cHotkeys))
       )
    {
        PGLOBALHOTKEY pHotkeyThis;
        if (pHotkeyThis = objFindHotkey(pHotkeys,
                                        cHotkeys,
                                        _wpQueryHandle(somSelf)))
        {
            // found:
            pHotkey->usFlags = pHotkeyThis->usFlags;
            pHotkey->ucScanCode = pHotkeyThis->ucScanCode;
            pHotkey->usKeyCode = pHotkeyThis->usKeyCode;
            brc = TRUE;     // success
        }

        hifFreeObjectHotkeys(pHotkeys);
    }

    return brc;
}

/*
 *@@ objSetObjectHotkey:
 *      implementation for XFldObject::xwpSetObjectHotkey.
 *
 *@@changed V0.9.5 (2000-08-20) [umoeller]: fixed "set first hotkey" bug, which hung the system
 *@@changed V0.9.5 (2000-08-20) [umoeller]: added more error checking
 */

BOOL objSetObjectHotkey(WPObject *somSelf,
                        XFldObject_POBJECTHOTKEY pHotkey)
{
    BOOL            brc = FALSE;
    HOBJECT         hobjSelf;

    // XFldObjectData *somThis = XFldObjectGetData(somSelf);

    if (hobjSelf = _wpQueryHandle(somSelf))
    {
        TRY_LOUD(excpt1)
        {
            PGLOBALHOTKEY   pHotkeys;
            ULONG           cHotkeys = 0;
            #ifdef DEBUG_KEYS
                _Pmpf(("Entering xwpSetObjectHotkey usFlags = 0x%lX, usKeyCode = 0x%lX",
                        usFlags, usKeyCode));
            #endif

            if (pHotkeys = hifQueryObjectHotkeys(&cHotkeys))
            {
                // hotkeys list exists:

                PGLOBALHOTKEY pHotkeyThis = NULL;

                #ifdef DEBUG_KEYS
                    _Pmpf(("  Checking for existence hobj 0x%lX", hobjSelf));
                #endif
                // check if we have a hotkey for this object already
                pHotkeyThis = objFindHotkey(pHotkeys,
                                            cHotkeys,
                                            hobjSelf);

                #ifdef DEBUG_KEYS
                    _Pmpf(("  objFindHotkey returned PGLOBALHOTKEY 0x%lX", pHotkeyThis));
                #endif

                // what does the caller want?
                if (!pHotkey)
                {
                    #ifdef DEBUG_KEYS
                        _Pmpf(("  'delete hotkey' mode:"));
                    #endif

                    // "delete hotkey" mode:
                    if (pHotkeyThis)
                    {
                        // found (already exists): delete
                        // by copying the following item(s)
                        // in the array over the current one
                        ULONG   ulpofs = 0,
                                uliofs = 0;
                        ulpofs = ((PBYTE)pHotkeyThis - (PBYTE)pHotkeys);

                        #ifdef DEBUG_KEYS
                            _Pmpf(("  pHotkeyThis - pHotkeys: 0x%lX", ulpofs));
                        #endif

                        uliofs = (ulpofs / sizeof(GLOBALHOTKEY));
                                    // 0 for first, 1 for second, ...

                        #ifdef DEBUG_KEYS
                            _Pmpf(("  Deleting existing hotkey @ ofs %d", uliofs));
                        #endif

                        if (uliofs < (cHotkeys - 1))
                        {
                            // not last item:

                            ULONG cb = (cHotkeys - uliofs - 1) * sizeof(GLOBALHOTKEY);

                            #ifdef DEBUG_KEYS
                                _Pmpf(("  Copying 0x%lX to 0x%lX, %d bytes (%d per item)",
                                        pHotkeyThis + 1, pHotkeyThis,
                                        cb, sizeof(GLOBALHOTKEY)));
                            #endif

                            memcpy(pHotkeyThis,
                                   pHotkeyThis + 1,
                                   cb);
                        }

                        brc = hifSetObjectHotkeys(pHotkeys, cHotkeys - 1);
                    }
                    // else: does not exist, so it can't be deleted either
                }
                else
                {
                    // "set hotkey" mode:

                    #ifdef DEBUG_KEYS
                        _Pmpf(("  'set hotkey' mode:"));
                    #endif

                    if (pHotkeyThis)
                    {
                        // found (already exists): overwrite
                        #ifdef DEBUG_KEYS
                            _Pmpf(("  Overwriting existing hotkey"));
                        #endif

                        if (    (pHotkeyThis->usFlags != pHotkey->usFlags)
                             || (pHotkeyThis->usKeyCode != pHotkey->usKeyCode)
                             || (pHotkeyThis->ucScanCode != pHotkey->ucScanCode)
                           )
                        {
                            pHotkeyThis->usFlags = pHotkey->usFlags;
                            pHotkeyThis->ucScanCode = pHotkey->ucScanCode;
                            pHotkeyThis->usKeyCode = pHotkey->usKeyCode;
                            pHotkeyThis->ulHandle = hobjSelf;
                            // set new objects list, which is the modified old list
                            brc = hifSetObjectHotkeys(pHotkeys, cHotkeys);
                        }
                    }
                    else
                    {
                        // not found: append new item after copying
                        // the entire list
                        PGLOBALHOTKEY pHotkeysNew = (PGLOBALHOTKEY)malloc(sizeof(GLOBALHOTKEY)
                                                                            * (cHotkeys+1));
                        #ifdef DEBUG_KEYS
                            _Pmpf(("  Appending new hotkey"));
                        #endif

                        if (pHotkeysNew)
                        {
                            PGLOBALHOTKEY pNewItem = pHotkeysNew + cHotkeys;
                            // copy old array
                            memcpy(pHotkeysNew, pHotkeys, sizeof(GLOBALHOTKEY) * cHotkeys);
                            // append new item
                            pNewItem->usFlags = pHotkey->usFlags;
                            pNewItem->ucScanCode = pHotkey->ucScanCode;
                            pNewItem->usKeyCode = pHotkey->usKeyCode;
                            pNewItem->ulHandle = hobjSelf;
                            brc = hifSetObjectHotkeys(pHotkeysNew, cHotkeys + 1);
                            free(pHotkeysNew);
                        }
                    }
                }

                hifFreeObjectHotkeys(pHotkeys);
            } // end if (pHotkeys)
            else
            {
                // hotkey list doesn't exist yet:
                if (pHotkey)
                {
                    // "set hotkey" mode:
                    GLOBALHOTKEY HotkeyNew = {0};

                    #ifdef DEBUG_KEYS
                        _Pmpf(("  Creating single new hotkey"));
                    #endif

                    HotkeyNew.usFlags = pHotkey->usFlags;
                    HotkeyNew.ucScanCode = pHotkey->ucScanCode; // V0.9.5 (2000-08-20) [umoeller]
                    HotkeyNew.usKeyCode = pHotkey->usKeyCode;
                    HotkeyNew.ulHandle = hobjSelf;
                    brc = hifSetObjectHotkeys(&HotkeyNew,
                                              1);     // one item only
                }
                // else "delete hotkey" mode: do nothing
            }
        }
        CATCH(excpt1)
        {
            brc = FALSE;
        } END_CATCH();

        if (brc)
            // updated: update the "Hotkeys" settings page
            // in XWPKeyboard, if it's open
            ntbUpdateVisiblePage(NULL,      // any somSelf
                                 SP_KEYB_OBJHOTKEYS);
    } // end if (hobjSelf)

    #ifdef DEBUG_KEYS
        _Pmpf(("Leaving xwpSetObjectHotkey"));
    #endif

    return brc;
}

/*
 *@@ objRemoveObjectHotkey:
 *      implementation for M_XFldObject::xwpclsRemoveObjectHotkey.
 *
 *@@added V0.9.0 (99-11-12) [umoeller]
 */

BOOL objRemoveObjectHotkey(HOBJECT hobj)
{
    PGLOBALHOTKEY   pHotkeys;
    ULONG           cHotkeys = 0;
    BOOL            brc = FALSE;

    pHotkeys = hifQueryObjectHotkeys(&cHotkeys);

    if (pHotkeys)
    {
        // hotkeys list exists:
        PGLOBALHOTKEY pHotkeyThis = objFindHotkey(pHotkeys,
                                                  cHotkeys,
                                                  hobj);

        if (pHotkeyThis)
        {
            // found (already exists): delete
            // by copying the following item(s)
            // in the array over the current one
            ULONG   ulpofs = 0,
                    uliofs = 0;
            ulpofs = ((PBYTE)pHotkeyThis - (PBYTE)pHotkeys);

            uliofs = (ulpofs / sizeof(GLOBALHOTKEY));
                        // 0 for first, 1 for second, ...
            if (uliofs < (cHotkeys - 1))
            {
                ULONG cb = (cHotkeys - uliofs - 1) * sizeof(GLOBALHOTKEY);

                #ifdef DEBUG_KEYS
                    _Pmpf(("  Copying 0x%lX to 0x%lX, %d bytes (%d per item)",
                            pHotkeyThis + 1, pHotkeyThis,
                            cb, sizeof(GLOBALHOTKEY)));
                #endif

                // not last item:
                memcpy(pHotkeyThis,
                       pHotkeyThis + 1,
                       cb);
            }

            brc = hifSetObjectHotkeys(pHotkeys, cHotkeys-1);
        }
        // else: does not exist, so it can't be deleted either

        hifFreeObjectHotkeys(pHotkeys);
    }

    if (brc)
        // updated: update the "Hotkeys" settings page
        // in XWPKeyboard, if it's open
        ntbUpdateVisiblePage(NULL,      // any somSelf
                             SP_KEYB_OBJHOTKEYS);

    return brc;
}

/* ******************************************************************
 *
 *   Object menus
 *
 ********************************************************************/

/*
 *@@ objModifyPopupMenu:
 *      implementation for XFldObject::wpModifyPopupMenu.
 *      This now implements "lock in place" hacks.
 *
 *@@added V0.9.7 (2000-12-10) [umoeller]
 *@@changed V0.9.19 (2002-04-17) [umoeller]: adjusted for new menu handling
 */

VOID objModifyPopupMenu(WPObject* somSelf,
                        HWND hwndMenu)
{
    if (G_fIsWarp4)
    {
        if (cmnQuerySetting(mnuQueryMenuXWPSetting(somSelf)) & XWPCTXT_LOCKEDINPLACE)
            // remove WPObject's "Lock in place" submenu
            winhDeleteMenuItem(hwndMenu, WPMENUID_LOCKEDINPLACE); // ID_WPM_LOCKINPLACE);
        else if (cmnQuerySetting(sfFixLockInPlace)) // V0.9.7 (2000-12-10) [umoeller]
        {
            // get text first... this saves us our own NLS resource
            PSZ pszLockInPlace;
            if (pszLockInPlace = winhQueryMenuItemText(hwndMenu, WPMENUID_LOCKEDINPLACE)) // ID_WPM_LOCKINPLACE))
            {
                MENUITEM mi;
                if (winhQueryMenuItem(hwndMenu,
                                      WPMENUID_LOCKEDINPLACE,
                                      FALSE,
                                      &mi))
                {
                    // delete old (incl. submenu)
                    winhDeleteMenuItem(hwndMenu, WPMENUID_LOCKEDINPLACE);
                    // insert new
                    winhInsertMenuItem(hwndMenu,
                                       mi.iPosition,        // at old position
                                       WPMENUID_LOCKEDINPLACE,
                                       pszLockInPlace,
                                       MIS_TEXT,
                                       (_wpQueryStyle(somSelf) & OBJSTYLE_LOCKEDINPLACE)
                                          ? MIA_CHECKED
                                          : 0);
                }
                free(pszLockInPlace);
            }
        }
    }
}

/*
 *@@ CopyOneObject:
 *
 *@@added V0.9.19 (2002-06-02) [umoeller]
 */

static VOID CopyOneObject(PXSTRING pstr,
                          WPObject *pObject,
                          BOOL fFullPath)
{
    CHAR szRealName[CCHMAXPATH];
    if (    (pObject = objResolveIfShadow(pObject))
         && (_somIsA(pObject, _WPFileSystem))
         && (_wpQueryFilename(pObject, szRealName, fFullPath))
       )
    {
        xstrcat(pstr, szRealName, 0);
        xstrcatc(pstr, ' ');
    }
}

/*
 *@@ objCopyObjectFileName:
 *      copy object filename(s) to clipboard. This method is
 *      called from several overrides of wpMenuItemSelected.
 *
 *      If somSelf does not have CRA_SELECTED emphasis in the
 *      container, its filename is copied. If it does have
 *      CRA_SELECTED emphasis, all filenames which have CRA_SELECTED
 *      emphasis are copied, separated by spaces.
 *
 *      Note that somSelf might not neccessarily be a file-system
 *      object. It can also be a shadow to one, so we might need
 *      to dereference that.
 *
 *@@changed V0.9.0 [umoeller]: fixed a minor bug when memory allocation failed
 *@@changed V0.9.19 (2002-06-02) [umoeller]: fixed buffer overflow with many objects
 *@@changed V0.9.19 (2002-06-02) [umoeller]: renamed from wpshCopyObjectFileName, moved here
 */

BOOL objCopyObjectFileName(WPObject *somSelf, // in: the object which was passed to
                                // wpMenuItemSelected
                           HWND hwndCnr, // in: the container of the hwmdFrame
                                // of wpMenuItemSelected
                           BOOL fFullPath) // in: if TRUE, the full path will be
                                // copied; otherwise the filename only
{
    BOOL        fSuccess = FALSE,
                fSingleMode = TRUE;
    XSTRING     strFilenames;

    // get the record core of somSelf
    PMINIRECORDCORE pmrcSelf = _wpQueryCoreRecord(somSelf);

    // now we go through all the selected records in the container
    // and check if pmrcSelf is among these selected records;
    // if so, this means that we want to copy the filenames
    // of all the selected records.
    // However, if pmrcSelf is not among these, this means that
    // either the context menu of the _folder_ has been selected
    // or the menu of an object which is not selected; we will
    // then only copy somSelf's filename.

    PMINIRECORDCORE pmrcSelected = (PMINIRECORDCORE)CMA_FIRST;

    xstrInit(&strFilenames, 1000);

    do
    {
        // get the first or the next _selected_ item
        pmrcSelected = (PMINIRECORDCORE)WinSendMsg(hwndCnr,
                                                   CM_QUERYRECORDEMPHASIS,
                                                   (MPARAM)pmrcSelected,
                                                   (MPARAM)CRA_SELECTED);

        if ((pmrcSelected != 0) && (((ULONG)pmrcSelected) != -1))
        {
            // first or next record core found:
            // copy filename to buffer
            CopyOneObject(&strFilenames,
                          OBJECT_FROM_PREC(pmrcSelected),
                          fFullPath);

            // compare the selection with pmrcSelf
            if (pmrcSelected == pmrcSelf)
                fSingleMode = FALSE;
        }
    } while ((pmrcSelected != 0) && (((ULONG)pmrcSelected) != -1));

    if (fSingleMode)
        // if somSelf's record core does NOT have the "selected"
        // emphasis: this means that the user has requested a
        // context menu for an object other than the selected
        // objects in the folder, or the folder's context menu has
        // been opened: we will only copy somSelf then.
        CopyOneObject(&strFilenames, somSelf, fFullPath);

    if (strFilenames.ulLength)
    {
        // something was copied:
        HAB     hab = WinQueryAnchorBlock(hwndCnr);

        // remove last space
        strFilenames.psz[--(strFilenames.ulLength)] = '\0';

        // copy to clipboard
        if (WinOpenClipbrd(hab))
        {
            PSZ pszDest;
            if (!DosAllocSharedMem((PVOID*)&pszDest,
                                   NULL,
                                   strFilenames.ulLength + 1,
                                   PAG_WRITE | PAG_COMMIT | OBJ_GIVEABLE))
            {
                memcpy(pszDest,
                       strFilenames.psz,
                       strFilenames.ulLength + 1);

                WinEmptyClipbrd(hab);

                fSuccess = WinSetClipbrdData(hab,       // anchor-block handle
                                             (ULONG)pszDest, // pointer to text data
                                             CF_TEXT,        // data is in text format
                                             CFI_POINTER);   // passing a pointer

                // PMREF says (implicitly) it is not necessary to call
                // DosFreeMem. I hope that is correct.
                // V0.9.19 (2002-06-02) [umoeller]
            }
            WinCloseClipbrd(hab);
        }
    }

    xstrClear(&strFilenames);

    return fSuccess;
}


