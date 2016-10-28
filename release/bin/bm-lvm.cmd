/* Add BootManager choices to XDesktop shutdown actions */
/* This will modify the XFolder INI keys in OS2.INI to  */
/* contain all Bootmanager partitions as user reboot    */
/* options.                                             */
/* (W) (C) 1998 Duane A. Chamblee <duanec@ibm.net>      */
/* Part of the XFolder package.                         */


/**********************************************************
 Modified by
   Nenad Milenkovic (nenad_milenkovic@softhome.net)

 Nov 1999:
   - Improved to support Logical Volume Manager output
     from Warp Server for e-business (Aurora) in addition
     to standard FDISK output from Warp 4

 Apr 2000:
   - Updated to work with XWorkplace instead XFolder
   - Improved to support bootable partitions hidden
     from OS/2 (those without drive letter assigned,
     Linux for example)

 Donated to XWorkplace authors to be used and released
 with XWorkplace in any form and under any lincense
 they wish.
**********************************************************/

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
       if POS('Bootable', oneline)\=41 then iterate
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
   call SysINI 'USER','XWorkplace', 'RebootTo', inistring||'00'x||'00'x

'@call RXQUEUE.EXE /CLEAR'
