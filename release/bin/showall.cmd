/* $Id$ */

/* SHOWALL.CMD
 *
 * This file makes Tree views of folders show all objects of the folder, instead
 * of just subfolders. This setting works on OS/2 Warp 4 only (unfortunately).
 *
 * See ALWSSORT.CMD for comments. This file is basically the same.
 *
 * (w) (c) 1998 Ulrich M”ller. All rights reserved.
 */

'@echo off'
call RxFuncAdd 'SysLoadFuncs', 'RexxUtil', 'SysLoadFuncs'
call SysLoadFuncs

call SysSetObjectData directory(), 'SHOWALLINTREEVIEW=YES';

