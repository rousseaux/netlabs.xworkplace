/* $Id$ */

/* createdoc.cmd:
    this calls xdoc\xdoc.exe on all the *.c files in \main and \helpers
    to have HTML reference documentation created.
    After this has been executed, open HTML\index.html to view the
    documentation. */

'@echo off'
'call xdoc "-iinclude;K:\projects\cvs\warpin\include\helpers;include\filesys;include\shared;include\hook;include\setup" src\classes\*.c K:\projects\cvs\warpin\src\helpers\*.c src\hook\*.c src\filesys\*.c src\shared\*.c'

