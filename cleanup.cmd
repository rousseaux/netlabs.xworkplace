@echo off
REM cleanup all compiled XFolder files

ECHO This will delete all .DLL, .RES, .OBJ, .MSG, .MAP, .SYM files in
ECHO the source directories. Press any key to continue or Ctrl+C to stop.
PAUSE

del src\helpers\*.obj
del src\NetscapeDDE\*.obj
del src\NetscapeDDE\*.exe
del src\Treesize\*.obj
del src\Treesize\*.exe

del bin\*.exe
del bin\*.dll
del bin\*.obj
del bin\*.res
del bin\*.map
del bin\*.sym

del tools\repclass\*.exe
del tools\repclass\*.obj
del tools\repclass\*.map
del tools\wpsreset\*.exe
del tools\wpsreset\*.obj
del tools\wpsreset\*.map

REM INF/HLP files

ECHO Do you also wish to delete the INF/HLP files?
ECHO Press any key to continue or Ctrl+C to stop.
PAUSE

del 001\help.001\xfldr001.hlp
del 001\inf.001\xfldr001.inf

del 049_de\help.049\xfldr049.hlp
del 049_de\inf.049\xfldr049.inf

REM SOM headers

ECHO Do you also wish to delete all the SC-created .DEF, .IH, .H files?
ECHO Press any key to continue or Ctrl+C to stop.
PAUSE

del idl\*.def
del include\classes\*.h
del include\classes\*.ih

REM IPF source files

ECHO Do you also wish to delete all the IPF source files?
ECHO Press any key to continue or Ctrl+C to stop.
PAUSE

del 001\inf.001\xfldr001.ipf
del 001\inf.001\*.bmp
del 001\help.001\xfldr001.ipf
del 001\help.001\*.bmp

del 049_de\inf.049\xfldr049.ipf
del 049_de\inf.049\*.bmp
del 049_de\help.049\xfldr049.ipf
del 049_de\help.049\*.bmp


