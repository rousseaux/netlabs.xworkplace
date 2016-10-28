/**/
wpistem = 'xwp-1-0-11'
filetime = '01:11:00'
/**/
call RxFuncAdd 'SysLoadFuncs', 'RexxUtil', 'SysLoadFuncs'
call SysLoadFuncs
parse arg reldir countrycode .
if reldir = '' then do
  say 'Usage: makewpi <release directory> [<country code>]'
  say
  say 'Country code defaults to 001 if omitted or invalid'
  return
end
if datatype(countrycode) \= 'NUM' | countrycode < 0 | countrycode > 999 then
  countrycode = 1

wisfile = right(countrycode, 3, '0')'\xwp'right(countrycode, 3, '0')'.wis'

select
  when countrycode = 1 then
    wpistem = wpistem'-en'
  when countrycode = 31 then
    wpistem = wpistem'-nl'
  when countrycode = 39 then
    wpistem = wpistem'-it'
  when countrycode = 49 then
    wpistem = wpistem'-de'
  when countrycode = 81 then
    wpistem = wpistem'-ja'
  otherwise do
    wpistem = wpistem'-'right(countrycode, 3, '0')
    wisfile = 'xwp.wis'
    end
end

packages.0 = 3
packages.1.id = 1
packages.1.dir = 'kernel'
packages.2.id = 1000 + countrycode
packages.2.dir = right(countrycode, 3, '0')
packages.3.id = 2000 + countrycode
packages.3.dir = 'inf'right(countrycode, 3, '0')
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
wic3 = '-u -s' makedir'\'wisfile
call SysFileDelete reldir'\'wpistem'.exe'
wic1 wic2 wic3
call directory makedir
return

setfiletime: procedure expose filetime makedir
  parse arg pkgdir
  call SysFileTree pkgdir'\*', 'stem', 'FOS'
  filedate = date('S')
  filedate = left(filedate,4)'-'substr(filedate,5,2)'-'substr(filedate,7,2)
  do i = 1 to stem.0
    call SysSetFileDateTime stem.i, filedate, filetime
    '@'makedir'\tools\setftime' stem.i
  end
  return
