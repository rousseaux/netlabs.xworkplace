/* justinstall.cmd
   set flag in OS2.INI that will make XFolder
   say hello at the next WPS bootup
   (for debugging)
   */

call RxFuncAdd 'SysLoadFuncs', 'REXXUTIL', 'SysLoadFuncs'
call SysLoadFuncs

rc = SysINI('USER', "XWorkplace", "JustInstalled", '1'||'0'x);

