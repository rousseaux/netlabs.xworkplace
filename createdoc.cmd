/* createdoc.cmd:
    this calls xdoc\xdoc.exe on all the *.c files in \main and \helpers
    to have HTML reference documentation created.
    After this has been executed, open HTML\index.html to view the
    documentation. */

'SET BEGINLIBPATH=K:\projects\cvs\warpin'
'@echo off'
'call xdoc "-iinclude;K:\projects\cvs\warpin\include" src\classes\*.c src\config\*.c src\filesys\*.c src\hook\*.c src\media\*.c src\shared\*.c src\startshut\*.c K:\projects\cvs\warpin\src\helpers\*.c K:\projects\cvs\warpin\src\helpers\*.cpp'

