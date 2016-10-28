/* DEFDETLS.CMD
 *
 * Change current directory's default view to Details view.
 * See ALWSSORT.CMD for comments. This file is basically the same.
 *
 * (w) (c) 1998 Ulrich M”ller. All rights reserved.
 */

'@echo off'
call RxFuncAdd 'SysLoadFuncs', 'RexxUtil', 'SysLoadFuncs'
call SysLoadFuncs

call SysSetObjectData directory(), 'DEFAULTVIEW=DETAILS';

