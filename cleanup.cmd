/* Cleanup XWorkplace */
signal on halt
'@echo off'

/* Compiled XWorkplace files */
say 'Do you wish to delete all .OBJ, .RES, .EXE, .DLL, .MAP, .SYM files in'
call charout , 'the source directories [Y/N] ? '
parse upper linein yn .
if yn = 'Y' then do
  call deletefiles 'bin\*.obj'
  call deletefiles 'bin\*.res'
  call deletefiles 'bin\exe_mt\*.obj'
  call deletefiles 'bin\exe_mt\*.res'
  call deletefiles 'bin\exe_st\*.obj'
  call deletefiles 'bin\exe_st\*.res'
  call deletefiles 'bin\modules\*.exe'
  call deletefiles 'bin\modules\*.dll'
  call deletefiles 'bin\modules\*.map'
  call deletefiles 'bin\modules\*.sym'
  call deletefiles 'bin\widgets\*.obj'
end

/* LIB files */
call charout , 'Do you wish to delete the LIB files [Y/N] ? '
parse upper linein yn .
if yn = 'Y' then do
  call deletefiles 'bin\*.lib'
  call deletefiles 'bin\exe_mt\*.lib'
end

/* INF/HLP files */
call charout , 'Do you wish to delete the INF/HLP files [Y/N] ? '
parse upper linein yn .
if yn = 'Y' then do
  call deletefiles '001\inf.001\xfldr001.inf'
  call deletefiles '001\xwphelp\xfldr001.hlp'
  call deletefiles '049_de\inf.049\xfldr049.inf'
end

/* IPF source files */
call charout , 'Do you wish to delete the IPF source files [Y/N] ? '
parse upper linein yn .
if yn = 'Y' then do
  call deletefiles '001\inf.001\xfldr001.ipf'
  call deletefiles '001\inf.001\*.bmp'
  call deletefiles '001\xwphelp\xfldr001.ipf'
  call deletefiles '001\xwphelp\000img\*.bmp'
  call deletefiles '049_de\inf.049\xfldr049.ipf'
  call deletefiles '049_de\inf.049\*.bmp'
end

/* SOM headers */
call charout , 'Do you wish to delete all the SC-created .DEF, .IH and .H files [Y/N] ? '
parse upper linein yn .
if yn = 'Y' then do
  call deletefiles 'idl\*.def'
  call deletefiles 'include\classes\*.h'
  call deletefiles 'include\classes\*.ih'
end
return

halt:
  say
  return

deletefiles: procedure
  arg name
  'if exist' name 'del' name
  return
