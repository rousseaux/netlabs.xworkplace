@echo off

REM This will rename lots of HTML source files for the
REM HLP file and update the links in the
REM HTML sources also.
REM For this to work, tools/strrpl.exe must be on your PATH.

REM change the following command to the directory where this
REM script should change to (containing all the INF HTML sources)
SET SUBDIR=inf.001

echo Warning: This script will rename a lot of files in the %SUBDIR% directory.
echo Please read the comments in this script to find out more.
echo Press Ctrl+C to abort now or any other key to continue.
PAUSE > NUL

CD %SUBDIR%

REM This command is for deleting all .LONGNAME EAs. If you
REM don't have my CommandPak utilities, you can delete this
REM line. But make sure the .LONGNAMEs are delete in some
REM other way, or the WPS might still display the old filenames.
call xren.cmd -dL *

ren 066bootlogo.htm xsd_03bootlogo.html
strrpl *.html 066bootlogo.htm xsd_03bootlogo.html









