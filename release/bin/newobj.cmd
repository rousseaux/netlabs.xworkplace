/* NEWOBJ.CMD
 *
 * See the XFolder Online Reference for a description of what this
 * file is doing.
 *
 * (W) (C) 1998 Ulrich M”ller. All rights reserved.
 * Contains enhancements suggested by John Buckley.
 */

'@echo off'
call RxFuncAdd 'SysLoadFuncs', 'RexxUtil', 'SysLoadFuncs'
call SysLoadFuncs

parse arg string

/* Extract Class name and object name from string */

/* Extracts all words from 'string' beginning at word 2 and passes them to 'OName' */
OName = subword(string,2)

/* Deletes all words in 'string', beginning at word 2 and passes the result to 'Class' */
Class = delword(string,2)

/* Strips any trailing space characters from the string in Class */
Class = strip(Class,'T')

if (OName = "") then do
    Say "Usage: newobj <wpsclass> <name> [<destdir>]"
    exit
end

Dest = directory()

if SysCreateObject(Class, OName, Dest) = 0 then do
    x = 1
    do while x < 10
        Name = OName||x
        if SysCreateObject(Class, Name, Dest) = 1 then
            x = 10
        else x = x+1
    end
end


