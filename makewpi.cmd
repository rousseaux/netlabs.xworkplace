/**/
wpistem = 'xwp-1-1-0'
filetime = '01:10:00'
wisfile = 'xwp.wis'
packages.0 = 3
packages.stub = '-ustub.exe'
packages.1.id = 1
packages.1.dir = 'kernel'
packages.2.id = 2
packages.2.dir = '001'
packages.3.id = 3
packages.3.dir = 'inf001'
/**/
call RxFuncAdd 'SysLoadFuncs', 'RexxUtil', 'SysLoadFuncs'
call SysLoadFuncs
parse arg reldir
if reldir = '' then do
  say 'Usage: makewpi <release directory>'
  return
end
makedir = directory()
reldir = directory(reldir)
if reldir = '' then do
  say 'Invalid release directory'
  return
end
warpindir = SysIni(USER, 'WarpIN', 'Path')
if warpindir = 'ERROR:' | warpindir = '' then do
  say 'WarpIN is not installed correctly'
  return
end
call directory warpindir
wic1 = '@wic' reldir'\'wpistem '-a'
wic2 = ''
do i = 1 to packages.0
  wic2 = wic2 packages.i.id '-r -c'reldir'\'packages.i.dir '*'
  call setfiletime reldir'\'packages.i.dir
end
wic3 = packages.stub '-s' makedir'\'wisfile
call SysFileDelete reldir'\'wpistem'.exe'
wic1 wic2 wic3
call directory makedir
return

setfiletime: procedure expose filetime
  parse arg pkgdir
  call SysFileTree pkgdir'\*', 'stem', 'FOS'
  filedate = date('S')
  filedate = left(filedate,4)'-'substr(filedate,5,2)'-'substr(filedate,7,2)
  do i = 1 to stem.0
    call SysSetFileDateTime stem.i, filedate, filetime
  end
  return
