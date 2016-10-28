/* */

/* createchangelog.cmd:

    this calls xdoc.exe on all the *.c files in \main and \frontend
    to have a plain-text CHANGELOG file generated from the sources.

    xdoc.exe must be in ..\xwphelpers (which is true if you have
    checked out the xwphelpers repository).
*/

'echo on'
cmd = '..\xwphelpers-1-0\xdoc -C "-iinclude;..\xwphelpers-1-0\include"'
cmd = cmd' src\classes\*.c src\config\*.c src\Daemon\*.c src\filesys\*.c src\hook\*.c ';
cmd = cmd'src\media\*.c src\shared\*.c src\startshut\*.c src\widgets\*.c src\xcenter\*.c ';
cmd = cmd'..\xwphelpers-1-0\src\helpers\*.c';

cmd;


