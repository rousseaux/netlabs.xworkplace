/* $Id$ */

/* ICONORM.CMD
 *
 * Switches the icon size for both Icon and Tree views to small icons.
 *
 * See ALWSSORT.CMD for comments. This file is basically the same.
 *
 * (w) (c) 1998 Ulrich M”ller. All rights reserved.
 */

'@echo off'
call RxFuncAdd 'SysLoadFuncs', 'RexxUtil', 'SysLoadFuncs'
call SysLoadFuncs

call SysSetObjectData directory(), 'ICONVIEW=FLOWED,MINI;ICONFONT=9.WarpSans;TREEVIEW=LINES,MINI;'

