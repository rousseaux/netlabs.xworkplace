/* $Id$ */

/* AufrufPfad = C:\os2tools\xfolder\install> */

/* DO NOT CHANGE the following */
call RxFuncAdd 'SysLoadFuncs', 'REXXUTIL', 'SysLoadFuncs'
call SysLoadFuncs

/* aus crobj001.cmd */
parse source dir;
say "parse source dir";
say dir;
parse var dir x1 x2 dir;
say "parse var dir x1 x2 dir";
say "  "dir x1 x2;
dir = filespec("D", dir)||filespec("P", dir);
say "dir = filespec("D", dir)||filespec("P", dir)";
say "  "dir;
pdir = left(dir, length(dir)-8);
say "pdir = left(dir, length(dir)-8)";
say "  pdir: "pdir;
idir = dir;
say "idir = dir";
say "  dir: "dir;
dir = pdir||"\bin\";
say "dir = pdir||\bin\";
say "  dir: "dir;

/* aus install.cmd */
parse source mydir;
parse var mydir x1 x2 mydir;
mydir = filespec("D", mydir)||filespec("P", mydir);
if (right(mydir, 1) = "\") then
    mydir = left(mydir, length(mydir)-1);
say mydir;
