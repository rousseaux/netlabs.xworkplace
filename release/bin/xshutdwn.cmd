/* XSHUTDWN.CMD
 *
 * Invokes a setup string on the desktop to start shutdown now.
 *
 * (w) (c) 2001 Ulrich M”ller. All rights reserved.
 */

'@echo off'
call RxFuncAdd 'SysLoadFuncs', 'RexxUtil', 'SysLoadFuncs'
call SysLoadFuncs

call SysSetObjectData '<WP_DESKTOP>', 'XSHUTDOWNNOW=DEFAULT;'


