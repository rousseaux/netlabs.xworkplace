
/*
 *@@sourcefile hk_misc.c:
 *
 *@@added V0.9.12 (2001-05-27) [umoeller]
 *@@header "hook\hook_private.h"
 */

/*
 *      Copyright (C) 1999-2001 Ulrich M�ller.
 *      Copyright (C) 1993-1999 Roman Stangl.
 *      Copyright (C) 1995-1999 Carlos Ugarte.
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, in version 2 as it comes in the COPYING
 *      file of the XWorkplace main distribution.
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 */

#define INCL_WINWINDOWMGR
#define INCL_WINFRAMEMGR
#define INCL_WINMESSAGEMGR
#define INCL_WININPUT
#define INCL_WINPOINTERS
#define INCL_WINMENUS
#define INCL_WINSCROLLBARS
#define INCL_WINSYS
#define INCL_WINTIMER
#define INCL_WINHOOKS
#define INCL_DOSSEMAPHORES
#define INCL_DOSERRORS
#define INCL_DOSMODULEMGR
#include <os2.h>

#include <stdio.h>

// PMPRINTF in hooks is a tricky issue;
// avoid this unless this is really needed.
// If enabled, NEVER give the PMPRINTF window
// the focus, or your system will hang solidly...
#define DONTDEBUGATALL
#define DONT_REPLACE_MALLOC         // in case mem debug is enabled
#include "setup.h"

#include "helpers\undoc.h"

#include "hook\xwphook.h"
#include "hook\hook_private.h"          // private hook and daemon definitions

#pragma hdrstop

/******************************************************************
 *
 *  Input hook -- miscellaneous processing
 *
 ******************************************************************/

/*
 *@@ WMButton_SystemMenuContext:
 *      this gets called from hookInputHook upon
 *      MB2 clicks to copy a window's system menu
 *      and display it as a popup menu on the
 *      title bar.
 *
 *      Based on code from WarpEnhancer, (C) Achim Hasenm�ller.
 */

VOID WMButton_SystemMenuContext(HWND hwnd)     // of WM_BUTTON2CLICK
{
    POINTL      ptlMouse; // mouse coordinates
    HWND        hwndFrame; // handle of the frame window (parent)

    // get mouse coordinates (absolute coordinates)
    WinQueryPointerPos(HWND_DESKTOP, &ptlMouse);
    // query parent of title bar window (frame window)
    if (hwndFrame = WinQueryWindow(hwnd, QW_PARENT))
    {
        // query handle of system menu icon (action bar style)
        HWND hwndSysMenuIcon;
        if (hwndSysMenuIcon = WinWindowFromID(hwndFrame, FID_SYSMENU))
        {
            HWND        hNewMenu; // handle of our copied menu
            HWND        SysMenuHandle;

            // query item id of action bar system menu
            SHORT id = (SHORT)(USHORT)(ULONG)WinSendMsg(hwndSysMenuIcon,
                                                        MM_ITEMIDFROMPOSITION,
                                                        MPFROMSHORT(0), 0);
            // query item id of action bar system menu
            MENUITEM    mi = {0};
            CHAR        szItemText[100]; // buffer for menu text
            WinSendMsg(hwndSysMenuIcon, MM_QUERYITEM,
                       MPFROM2SHORT(id, FALSE),
                       MPFROMP(&mi));
            // submenu is our system menu
            SysMenuHandle = mi.hwndSubMenu;

            // create a new empty menu
            if (hNewMenu = WinCreateMenu(HWND_OBJECT, NULL))
            {
                // query how menu entries the original system menu has
                SHORT SysMenuItems = (SHORT)WinSendMsg(SysMenuHandle,
                                                       MM_QUERYITEMCOUNT,
                                                       0, 0);
                ULONG i;
                // loop through all entries in the original system menu
                for (i = 0; i < SysMenuItems; i++)
                {
                    id = (SHORT)(USHORT)(ULONG)WinSendMsg(SysMenuHandle,
                                                          MM_ITEMIDFROMPOSITION,
                                                          MPFROMSHORT(i),
                                                          0);
                    // get this menu item into mi buffer
                    WinSendMsg(SysMenuHandle,
                               MM_QUERYITEM,
                               MPFROM2SHORT(id, FALSE),
                               MPFROMP(&mi));
                    // query text of this menu entry into our buffer
                    WinSendMsg(SysMenuHandle,
                               MM_QUERYITEMTEXT,
                               MPFROM2SHORT(id, sizeof(szItemText)-1),
                               MPFROMP(szItemText));
                    // add this entry to our new menu
                    WinSendMsg(hNewMenu,
                               MM_INSERTITEM,
                               MPFROMP(&mi),
                               MPFROMP(szItemText));
                }

                // display popup menu
                WinPopupMenu(HWND_DESKTOP, hwndFrame, hNewMenu,
                             ptlMouse.x, ptlMouse.y, 0x8007, PU_HCONSTRAIN |
                             PU_VCONSTRAIN | PU_MOUSEBUTTON1 |
                             PU_MOUSEBUTTON2 | PU_KEYBOARD);
            }
        }
    }
}

/*
 *@@ WMChord_WinList:
 *      this displays the window list at the current
 *      mouse position when WM_CHORD comes in.
 *
 *      Based on code from WarpEnhancer, (C) Achim Hasenm�ller.
 */

VOID WMChord_WinList(VOID)
{
    POINTL  ptlMouse;       // mouse coordinates
    SWP     WinListPos;     // position of window list window
    LONG WinListX, WinListY; // new ordinates of window list window
    // LONG DesktopCX, DesktopCY; // width and height of screen
    // get mouse coordinates (absolute coordinates)
    WinQueryPointerPos(HWND_DESKTOP, &ptlMouse);
    // get position of window list window
    WinQueryWindowPos(G_HookData.hwndSwitchList,
                      &WinListPos);
    // calculate window list position (mouse pointer is center)
    WinListX = ptlMouse.x - (WinListPos.cx / 2);
    if (WinListX < 0)
        WinListX = 0;
    WinListY = ptlMouse.y - (WinListPos.cy / 2);
    if (WinListY < 0)
        WinListY = 0;
    if (WinListX + WinListPos.cx > G_HookData.lCXScreen)
        WinListX = G_HookData.lCXScreen - WinListPos.cx;
    if (WinListY + WinListPos.cy > G_HookData.lCYScreen)
        WinListY = G_HookData.lCYScreen - WinListPos.cy;
    // set window list window to calculated position
    WinSetWindowPos(G_HookData.hwndSwitchList, HWND_TOP,
                    WinListX, WinListY, 0, 0,
                    SWP_MOVE | SWP_SHOW | SWP_ZORDER);
    // now make it the active window
    WinSetActiveWindow(HWND_DESKTOP,
                       G_HookData.hwndSwitchList);
}


