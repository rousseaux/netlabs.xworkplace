/* Add BootManager choices to XDesktop shutdown actions */
/* This will modify the XFolder INI keys in OS2.INI to  */
/* contain all Bootmanager partitions as user reboot    */
/* options.                                             */
/* (W) (C) 1998 Duane A. Chamblee <duanec@ibm.net>      */
/* Part of the XFolder package.                         */

/* Updated with V1.00 to work with Aurora.              */

call RxFuncAdd 'SysLoadFuncs', 'RexxUtil', 'SysLoadFuncs'
call SysLoadFuncs

'@call RXQUEUE.EXE /CLEAR'


inicurrent=SysINI('USER','XFolder', 'RebootTo')
if inicurrent='ERROR:' then
   inistring=''
else
   inistring=LEFT(inicurrent,LENGTH(inicurrent)-2)
if sysOS2Ver()>='2.45' then
   'call lvm /query:bootable 2>&1 | RXQUEUE.EXE'
else
   'call fdisk /query /bootable:1 2>&1 | RXQUEUE.EXE'

/* Parse LVM output */
if sysOS2Ver()>='2.45' then do until LINES("QUEUE:")=0
   parse value linein("QUEUE:") with oneline
   if STRIP(oneline)='' then iterate
   parse var oneline drive': ' name 15 .
   if name='' then iterate
   if TRANSLATE(drive)='DRIVE' then iterate
   if POS(name,inistring)>0 then iterate
   name = strip(name)
   say 'adding: 'name
   inistring=inistring||name||'00'x||'setboot /iba:"'||name||'"'||'00'x
end
/* Parse FDISK output */
else Do until LINES("QUEUE:")=0
   parse value linein("QUEUE:") with oneline
   if STRIP(oneline)='' then iterate
   parse var oneline drive 7 name 15 .
   if TRANSLATE(drive)='DRIVE' then iterate
   if POS(name,inistring)>0 then iterate
   name = strip(name)
   say 'adding: 'name
   inistring=inistring||name||'00'x||'setboot /iba:"'||name||'"'||'00'x
end
if inistring<>'' then
   call SysINI 'USER','XFolder', 'RebootTo', inistring||'00'x||'00'x
'@call RXQUEUE.EXE /CLEAR'


