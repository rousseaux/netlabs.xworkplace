/* ICONORM.CMD
 *
 * Switches the icon size for both Icon and Tree views to normal icons.
 *
 * See ALWSSORT.CMD for comments. This file is basically the same.
 *
 * (w) (c) 1998 Ulrich M”ller. All rights reserved.
 */

'@echo off'
call RxFuncAdd 'SysLoadFuncs', 'RexxUtil', 'SysLoadFuncs'
call SysLoadFuncs

call SysSetObjectData directory(), 'ICONVIEW=NOGRID,NORMAL;REMOVEFONTS=YES;TREEVIEW=LINES,NORMAL;'

