
/*
 *@@sourcefile xwpshadow.c:
 *      This file contains SOM code for the following XWorkplace classes:
 *
 *      --  XWPShadow: a replacement for WPShadow.
 *
 *@@added V0.9.9 (2001-02-08) [umoeller]
 *@@somclass XWPShadow xsh_
 *@@somclass M_XWPShadow xshM_
 */

/*
 *      Copyright (C) 2001 Ulrich M�ller.
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
 *@@todo:
 *
 */

/*
 *  This file was generated by the SOM Compiler and Emitter Framework.
 *  Generated using:
 *      SOM Emitter emitctm: 2.41
 */

#ifndef SOM_Module_xwpshadow_Source
#define SOM_Module_xwpshadow_Source
#endif
#define XWPShadow_Class_Source
#define M_XWPShadow_Class_Source

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
#define INCL_DOSERRORS

#define INCL_GPILOGCOLORTABLE
#define INCL_GPIPRIMITIVES
#include <os2.h>

// C library headers
#include <stdio.h>              // needed for except.h

// generic headers
#include "setup.h"                      // code generation and debugging options

// headers in /helpers

// SOM headers which don't crash with prec. header files
#include "xwpshadow.ih"

// XWorkplace implementation headers
#include "shared\common.h"              // the majestic XWorkplace include file
#include "shared\kernel.h"              // XWorkplace Kernel

// other SOM headers

#pragma hdrstop

/* ******************************************************************
 *
 *   XWPShadow instance methods
 *
 ********************************************************************/

/*
 *@@ wpInitData:
 *      this WPObject instance method gets called when the
 *      object is being initialized (on wake-up or creation).
 *      We initialize our additional instance data here.
 *      Always call the parent method first.
 */

SOM_Scope void  SOMLINK xsh_wpInitData(XWPShadow *somSelf)
{
    /* XWPShadowData *somThis = XWPShadowGetData(somSelf); */
    XWPShadowMethodDebug("XWPShadow","xsh_wpInitData");

    XWPShadow_parent_WPShadow_wpInitData(somSelf);
}

/*
 *@@ wpRestoreState:
 *      this WPObject instance method gets called during object
 *      initialization (after wpInitData) to restore the data
 *      which was stored with wpSaveState.
 */

SOM_Scope BOOL  SOMLINK xsh_wpRestoreState(XWPShadow *somSelf,
                                           ULONG ulReserved)
{
    /* XWPShadowData *somThis = XWPShadowGetData(somSelf); */
    XWPShadowMethodDebug("XWPShadow","xsh_wpRestoreState");

    return (XWPShadow_parent_WPShadow_wpRestoreState(somSelf,
                                                     ulReserved));
}

/*
 *@@ wpSaveState:
 *      this WPObject instance method saves an object's state
 *      persistently so that it can later be re-initialized
 *      with wpRestoreState. This gets called during wpClose,
 *      wpSaveImmediate or wpSaveDeferred processing.
 *      All persistent instance variables should be stored here.
 */

SOM_Scope BOOL  SOMLINK xsh_wpSaveState(XWPShadow *somSelf)
{
    /* XWPShadowData *somThis = XWPShadowGetData(somSelf); */
    XWPShadowMethodDebug("XWPShadow","xsh_wpSaveState");

    return (XWPShadow_parent_WPShadow_wpSaveState(somSelf));
}

/*
 *@@ wpMenuItemSelected:
 *      this WPObject method processes menu selections.
 *      This must be overridden to support new menu
 *      items which have been added in wpModifyPopupMenu.
 *
 *      See XFldObject::wpMenuItemSelected for additional
 *      remarks.
 */

SOM_Scope BOOL  SOMLINK xsh_wpMenuItemSelected(XWPShadow *somSelf,
                                               HWND hwndFrame,
                                               ULONG ulMenuId)
{
    /* XWPShadowData *somThis = XWPShadowGetData(somSelf); */
    XWPShadowMethodDebug("XWPShadow","xsh_wpMenuItemSelected");

    return (XWPShadow_parent_WPShadow_wpMenuItemSelected(somSelf,
                                                         hwndFrame,
                                                         ulMenuId));
}

/*
 *@@ wpViewObject:
 *      this WPObject method either opens a new view of the
 *      object (by calling wpOpen) or resurfaces an already
 *      open view, if one exists already and "concurrent views"
 *      are enabled. This gets called every single time when
 *      an object is to be opened... e.g. on double-clicks.
 */

SOM_Scope HWND  SOMLINK xsh_wpViewObject(XWPShadow *somSelf,
                                         HWND hwndCnr, ULONG ulView,
                                         ULONG param)
{
    /* XWPShadowData *somThis = XWPShadowGetData(somSelf); */
    XWPShadowMethodDebug("XWPShadow","xsh_wpViewObject");

    return (XWPShadow_parent_WPShadow_wpViewObject(somSelf, hwndCnr,
                                                   ulView, param));
}

/*
 *@@ wpOpen:
 *      this WPObject instance method gets called when
 *      a new view needs to be opened. Normally, this
 *      gets called after wpViewObject has scanned the
 *      object's USEITEMs and has determined that a new
 *      view is needed.
 */

SOM_Scope HWND  SOMLINK xsh_wpOpen(XWPShadow *somSelf, HWND hwndCnr,
                                   ULONG ulView, ULONG param)
{
    /* XWPShadowData *somThis = XWPShadowGetData(somSelf); */
    XWPShadowMethodDebug("XWPShadow","xsh_wpOpen");

    return (XWPShadow_parent_WPShadow_wpOpen(somSelf, hwndCnr,
                                             ulView, param));
}


/* ******************************************************************
 *
 *   XWPShadow class methods
 *
 ********************************************************************/

/*
 *@@ wpclsQueryIconData:
 *      this WPObject class method builds the default
 *      icon for objects of a class (i.e. the icon which
 *      is shown if no instance icon is assigned). This
 *      apparently gets called from some of the other
 *      icon instance methods if no instance icon was
 *      found for an object. The exact mechanism of how
 *      this works is not documented.
 */

SOM_Scope ULONG  SOMLINK xshM_wpclsQueryIconData(M_XWPShadow *somSelf,
                                                 PICONINFO pIconInfo)
{
    /* M_XWPShadowData *somThis = M_XWPShadowGetData(somSelf); */
    M_XWPShadowMethodDebug("M_XWPShadow","xshM_wpclsQueryIconData");

    return (M_XWPShadow_parent_M_WPShadow_wpclsQueryIconData(somSelf,
                                                             pIconInfo));
}

