
/*
 * xwpshell.h:
 *      header file for xwpshell.c.
 *
 *      Copyright (C) 2000 Ulrich M”ller.
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, in version 2 as it comes in the COPYING
 *      file of the XWorkplace main distribution.
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 */

/* ******************************************************************
 *
 *   Constants
 *
 ********************************************************************/

#define     WC_SHELL_OBJECT     "XWPShellObject"

/* ******************************************************************
 *
 *   Messages
 *
 ********************************************************************/

#define     XM_LOGON                (WM_USER + 1)
#define     XM_LOGOFF               (WM_USER + 2)
#define     XM_ERROR                (WM_USER + 3)
#define     XM_MESSAGE              (WM_USER + 4)

/* ******************************************************************
 *
 *   Dialog ID's
 *
 ********************************************************************/

#define IDD_LOGON               200
#define IDDI_USERENTRY          201
#define IDDI_PASSWORDENTRY      202
#define IDDI_USERTEXT           204
#define IDDI_PASSWORDTEXT       205


