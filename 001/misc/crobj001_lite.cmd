/*
    Create default eWorkplace configuration folder
    English version (001)
    (C) 1998-2002 Ulrich M”ller.
 */

/* Here come the titles of the objects to be created.
   Translate these to your language. The "~" character
   determines the character for objects in the XWorkplace
   Configuration Folder which will then be underlined
   in the respective menu items. */

XWorkplace          = "eCS"
OS2                 = "eCS"

/* Config Folder title */
ConfigFolder        = "Menu Configuration Folder";
/* here come the objects in the config folder */
CommandLines        = "~Command prompts";
OS2Win              = "~"||OS2||" window";
OS2Fullscreen       = OS2||" ~fullscreen";
DosWin              = "~DOS window";
DOSFullscreen       = "DOS f~ullscreen";
CreateAnother       = "Create ~new";
Folder              = "Folder";
    /* the following three were added with V0.9.16 */
URLFolder           = "URL Folder";
DataFile            = "Data file";
ProgramObject       = "Program object";

Treesize            = "Treesize";

/* where to create the config folder: */
TargetLocation      = "<WP_CONFIG>"

/* DO NOT CHANGE the following */
call RxFuncAdd 'SysLoadFuncs', 'REXXUTIL', 'SysLoadFuncs'
call SysLoadFuncs

parse source dir;
parse var dir x1 x2 dir;
dir = filespec("D", dir)||filespec("P", dir);
pdir = left(dir, length(dir)-8);
idir = dir;
dir = pdir||"bin\";

rc = SysCreateObject("WPFolder", ConfigFolder, TargetLocation, "ICONVIEW=NONFLOWED,MINI;DEFAULTVIEW=ICON;SHOWALLINTREEVIEW=YES;ALWAYSSORT=NO;OBJECTID=<XWP_CONFIG>", "U");
if (\rc) then
    rc = SysCreateObject("WPFolder", ConfigFolder, "<WP_DESKTOP>", "ICONVIEW=NONFLOWED,MINI;DEFAULTVIEW=ICON;SHOWALLINTREEVIEW=YES;ALWAYSSORT=NO;OBJECTID=<XWP_CONFIG>", "U");

rc = SysCreateObject("WPFolder", CommandLines, "<XWP_CONFIG>", "DEFAULTVIEW=ICON;ICONVIEW=NONFLOWED,MINI;SHOWALLINTREEVIEW=YES;ALWAYSSORT=NO;OBJECTID=<XWP_CFG1>", "U");
    if (SysSearchPath("PATH", "CMDSHL.CMD") \= "") then
        rc = SysCreateObject("WPProgram", OS2Win||" (CmdShl)", "<XWP_CFG1>", "EXENAME=cmdshl.cmd;PARAMETERS=%;PROGTYPE=WINDOWABLEVIO;CCVIEW=YES;OBJECTID=<XWP_CMDSHL>;", "U");
    if (SysSearchPath("PATH", "BASH.EXE") \= "") then
        rc = SysCreateObject("WPProgram", OS2Win||" (bash)", "<XWP_CFG1>", "EXENAME=bash.exe;PARAMETERS=%;PROGTYPE=WINDOWABLEVIO;CCVIEW=YES;OBJECTID=<XWP_BASH>;", "U");
    if (SysSearchPath("PATH", "KSH.EXE") \= "") then
        rc = SysCreateObject("WPProgram", OS2Win||" (ksh)", "<XWP_CFG1>", "EXENAME=ksh.exe;PARAMETERS=%;PROGTYPE=WINDOWABLEVIO;CCVIEW=YES;OBJECTID=<XWP_KSH>;", "U");
    rc = SysCreateObject("WPProgram", OS2Win, "<XWP_CFG1>", "EXENAME=*;PARAMETERS=%;PROGTYPE=WINDOWABLEVIO;CCVIEW=YES;OBJECTID=<XWP_OS2WIN>;", "U");
    rc = SysCreateObject("WPProgram", OS2Fullscreen, "<XWP_CFG1>", "EXENAME=*;PARAMETERS=%;PROGTYPE=FULLSCREEN;CCVIEW=YES;OBJECTID=<XWP_OS2FULL>;", "U");
    rc = SysCreateObject("WPProgram", DosWin, "<XWP_CFG1>", "EXENAME=*;PARAMETERS=%;PROGTYPE=WINDOWEDVDM;CCVIEW=YES;OBJECTID=<XWP_DOSWIN>;", "U");
    rc = SysCreateObject("WPProgram", DosFullscreen, "<XWP_CFG1>", "EXENAME=*;PARAMETERS=%;PROGTYPE=VDM;CCVIEW=YES;OBJECTID=<XWP_DOSFULL>;", "U");

rc = SysCreateObject("WPFolder", CreateAnother, "<XWP_CONFIG>", "DEFAULTVIEW=ICON;ICONVIEW=NONFLOWED,MINI;SHOWALLINTREEVIEW=YES;ALWAYSSORT=NO;OBJECTID=<XWP_CFG2>", "U");
    rc = SysCreateObject("WPFolder", Folder, "<XWP_CFG2>", "TEMPLATE=YES;OBJECTID=<XWP_FOLDERTEMPLATE>;", "U");
    rc = SysCreateObject("WPUrlFolder", URLFolder, "<XWP_CFG2>", "TEMPLATE=YES;OBJECTID=<XWP_URLFOLDERTEMPLATE>;", "U");
    rc = SysCreateObject("WPDataFile", DataFile, "<XWP_CFG2>", "TEMPLATE=YES;OBJECTID=<XWP_DATAFILETEMPLATE>;", "U");
    rc = SysCreateObject("WPProgram", ProgramObject, "<XWP_CFG2>", "TEMPLATE=YES;OBJECTID=<XWP_PROGRAMTEMPLATE>;", "U");

rc = SysCreateObject("WPProgram", "---", "<XWP_CONFIG>", "OBJECTID=<XWP_SEP1>;", "U");

rc = SysCreateObject("WPProgram", Treesize, "<XWP_CONFIG>", "EXENAME="dir"treesize.exe;CCVIEW=YES;OBJECTID=<XWP_TREESIZE>;", "U");




