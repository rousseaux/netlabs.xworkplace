/* $Id$ */

/* ALWSSORT.CMD:
 *
 * Switches on "Always maintain sort order" for the
 * current directory.
 * You can take this file as an example for changing
 * WPS folder settings. If you're not familiar with
 * REXX, note the following:
 * -- The first line of this file MUST be a comment
 *    (as can be seen above). You may change it to
 *    something else (if you don't like my name),
 *    but you may not remove it. Otherwise OS/2 will
 *    consider this file a plain batch file (as with
 *    DOS), and produce errors only.
 * -- Check the XFolder Online Reference for valid
 *    folder setup strings. BE CAREFUL with them, or
 *    you might mess up things big time. DO NOT try
 *    out any actions on crucial OS/2 folders, such
 *    as the desktop. Create a "test" folder instead.
 *
 *  (W) (C) 1998 Ulrich M”ller. All rights reserved.
 */

'@echo off'
call RxFuncAdd 'SysLoadFuncs', 'RexxUtil', 'SysLoadFuncs'
call SysLoadFuncs
/* this loads the REXXUTIL functions into memory, one
 * of which is the SysSetObjectData function below */

call SysSetObjectData directory(), 'ALWAYSSORT=YES;'
/* directory() returns the current directory, which XFolder
 * should have switched to upon starting this script;
 * 'ALWAYSSORT=YES' is a folder setup string, as described
 * in the Online Reference. If you wish to change several
 * settings at once, separate them with semicola (";"). */

