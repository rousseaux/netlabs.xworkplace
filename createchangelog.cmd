/* createdoc.cmd:
    this calls xdoc\xdoc.exe on all the *.c files in \main and \helpers
    to have HTML reference documentation created.
    After this has been executed, open HTML\index.html to view the
    documentation. */

'echo on'
'..\xwphelpers\xdoc -C "-iinclude;..\xwphelpers\include" src\classes\*.c src\config\*.c src\Daemon\*.c src\filesys\*.c src\hook\*.c src\media\*.c src\shared\*.c src\startshut\*.c src\xcenter\*.c ..\xwphelpers\src\helpers\*.c'


