/* Add BootManager choices to XDesktop shutdown actions */
/* This will modify the XFolder INI keys in OS2.INI to  */
/* contain all Bootmanager partitions as user reboot    */
/* options.                                             */
/* (W) (C) 1998 Duane A. Chamblee <duanec@ibm.net>      */
/* Part of the XFolder package.                         */

/****************************************************
  Modified to support Logical Volume Manager output
     from Warp Server for e-business (Aurora) by
    Nenad Milenkovic (nenad@softhome.net) Nov '99
****************************************************/

call RxFuncAdd 'SysLoadFuncs', 'RexxUtil', 'SysLoadFuncs'
call SysLoadFuncs

'@call RXQUEUE.EXE /CLEAR'

if SysOS2ver()='2.45' then
  do
    '@call lvm /query:bootable,all 2>&1 | RXQUEUE.EXE'
    ver='wseb'
  end
else 
  do
    '@call fdisk /query /bootable:1 2>&1 | RXQUEUE.EXE'
    ver='warp'
  end

inicurrent=SysINI('USER','XFolder', 'RebootTo')
if inicurrent='ERROR:' then
   inistring=''
else
   inistring=LEFT(inicurrent,LENGTH(inicurrent)-2)

Do until LINES("QUEUE:")=0
   parse value linein("QUEUE:") with oneline
   if STRIP(oneline)='' then iterate

   if ver='wseb' then
     do
       parse var oneline drive 4 name 20 .
       if POS(':', drive)\=2 then iterate     
     end

   if ver='warp' then
     do
       parse var oneline drive 7 name 15 .
       if TRANSLATE(drive)='DRIVE' then iterate
     end

   if POS(name,inistring)>0 then iterate
   name = strip(name)
   say 'adding: 'name
   inistring=inistring||name||'00'x||'setboot /iba:"'||name||'"'||'00'x
end

if inistring<>'' then
   call SysINI 'USER','XFolder', 'RebootTo', inistring||'00'x||'00'x

'@call RXQUEUE.EXE /CLEAR'
