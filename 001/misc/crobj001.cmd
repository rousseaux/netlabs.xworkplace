/*
    Create default XWorkplace configuration folder
    English version (001)
    (C) 1998-2000 Ulrich M”ller.
 */

/*
    This file is executed both by INSTLxxx.CMD
    and by XWorkplace directly if the config folder
    is to be (re)created.
*/

/* Here come the titles of the objects to be created.
   Translate these to your language. The "~" character
   determines the character for objects in the XWorkplace
   Configuration Folder which will then be underlined
   in the respective menu items. */

/* Config Folder title */
ConfigFolder        = "XWorkplace Configuration Folder";
/* here come the objects in the config folder */
CommandLines        = "~Command lines";
OS2Win              = "~OS/2 window";
OS2Fullscreen       = "OS/2 ~fullscreen";
DosWin              = "~DOS window";
DOSFullscreen       = "DOS f~ullscreen";
CreateAnother       = "Create a~nother";
Folder              = "Folder";
ProgramObject       = "Program object";
QuickSettings       = "~Quick Settings";
DefaultView         = "~Default view for this folder";
IconView            = "Icon view";
TreeView            = "Tree view";
DetailsView         = "Details view";
SmallIcons          = "Small icons for Icon and Tree views";
NormalIcons         = "Normal icons for Icon and Tree views";
ShowAllInTreeView   = "Show all objects in Tree view";
WorkplaceShell      = "Workplace Shell";
XWPSetup            = "XWorkplace Setup";
OS2Kernel           = "OS/2 Kernel";
Screen              = "Screen";
Mouse               = "Mouse";
Keyboard            = "Keyboard";
Treesize            = "Treesize";
PackTree            = "Pack this tree";

/* DO NOT CHANGE the following */
call RxFuncAdd 'SysLoadFuncs', 'REXXUTIL', 'SysLoadFuncs'
call SysLoadFuncs

parse source dir;
parse var dir x1 x2 dir;
dir = filespec("D", dir)||filespec("P", dir);
pdir = left(dir, length(dir)-8);
idir = dir;
dir = pdir||"bin\";

rc = SysCreateObject("WPFolder", ConfigFolder, "<XWP_MAINFLDR>", "ICONVIEW=NONFLOWED,MINI;DEFAULTVIEW=ICON;SHOWALLINTREEVIEW=YES;ALWAYSSORT=NO;OBJECTID=<XWP_CONFIG>", "U");
if (\rc) then
    rc = SysCreateObject("WPFolder", ConfigFolder, "<WP_DESKTOP>", "ICONVIEW=NONFLOWED,MINI;DEFAULTVIEW=ICON;SHOWALLINTREEVIEW=YES;ALWAYSSORT=NO;OBJECTID=<XWP_CONFIG>", "U");

/* create config folders */
rc = SysCreateObject("WPFolder", CommandLines, "<XWP_CONFIG>", "DEFAULTVIEW=ICON;ICONVIEW=NONFLOWED,MINI;SHOWALLINTREEVIEW=YES;ALWAYSSORT=NO;OBJECTID=<XWP_CFG1>", "U");
    if (SysSearchPath("PATH", "CMDSHL.CMD") \= "") then
        rc = SysCreateObject("WPProgram", OS2Win||" (CmdShl)", "<XWP_CFG1>", "EXENAME=cmdshl.cmd;PARAMETERS=%;PROGTYPE=WINDOWABLEVIO;CCVIEW=YES;OBJECTID=<XWP_CMDSHL>;", "R");
    if (SysSearchPath("PATH", "BASH.EXE") \= "") then
        rc = SysCreateObject("WPProgram", OS2Win||" (bash)", "<XWP_CFG1>", "EXENAME=bash.exe;PARAMETERS=%;PROGTYPE=WINDOWABLEVIO;CCVIEW=YES;OBJECTID=<XWP_BASH>;", "R");
    if (SysSearchPath("PATH", "KSH.EXE") \= "") then
        rc = SysCreateObject("WPProgram", OS2Win||" (ksh)", "<XWP_CFG1>", "EXENAME=ksh.exe;PARAMETERS=%;PROGTYPE=WINDOWABLEVIO;CCVIEW=YES;OBJECTID=<XWP_KSH>;", "R");
    rc = SysCreateObject("WPProgram", OS2Win, "<XWP_CFG1>", "EXENAME=*;PARAMETERS=%;PROGTYPE=WINDOWABLEVIO;CCVIEW=YES;OBJECTID=<XWP_OS2WIN>;", "R");
    rc = SysCreateObject("WPProgram", OS2Fullscreen, "<XWP_CFG1>", "EXENAME=*;PARAMETERS=%;PROGTYPE=FULLSCREEN;CCVIEW=YES;OBJECTID=<XWP_OS2FULL>;", "R");
    rc = SysCreateObject("WPProgram", DosWin, "<XWP_CFG1>", "EXENAME=*;PARAMETERS=%;PROGTYPE=WINDOWEDVDM;CCVIEW=YES;OBJECTID=<XWP_DOSWIN>;", "R");
    rc = SysCreateObject("WPProgram", DosFullscreen, "<XWP_CFG1>", "EXENAME=*;PARAMETERS=%;PROGTYPE=VDM;CCVIEW=YES;OBJECTID=<XWP_DOSFULL>;", "R");

rc = SysCreateObject("WPFolder", CreateAnother, "<XWP_CONFIG>", "DEFAULTVIEW=ICON;ICONVIEW=NONFLOWED,MINI;SHOWALLINTREEVIEW=YES;ALWAYSSORT=NO;OBJECTID=<XWP_CFG2>", "U");
    rc = SysCreateObject("WPFolder", Folder, "<XWP_CFG2>", "TEMPLATE=YES;OBJECTID=<XWP_FOLDERTEMPLATE>;", "R");
    rc = SysCreateObject("WPProgram", ProgramObject, "<XWP_CFG2>", "TEMPLATE=YES;OBJECTID=<XWP_PROGRAMTEMPLATE>;", "R");

rc = SysCreateObject("WPFolder", QuickSettings, "<XWP_CONFIG>", "DEFAULTVIEW=ICON;ICONVIEW=NONFLOWED,MINI;SHOWALLINTREEVIEW=YES;ALWAYSSORT=NO;OBJECTID=<XWP_CFG3>", "U");
    rc = SysCreateObject("WPFolder", DefaultView, "<XWP_CFG3>", "DEFAULTVIEW=ICON;ICONVIEW=NONFLOWED,MINI;SHOWALLINTREEVIEW=YES;ALWAYSSORT=NO;OBJECTID=<XWP_CFG3_1>", "U");
        rc = SysCreateObject("WPProgram", IconView, "<XWP_CFG3_1>", "EXENAME="dir'deficon.cmd;CCVIEW=YES;MINIMIZED=YES;NOAUTOCLOSE=NO;OBJECTID=<XWP_DEFICON>;', "R");
        rc = SysCreateObject("WPProgram", TreeView, "<XWP_CFG3_1>", "EXENAME="dir'deftree.cmd;CCVIEW=YES;MINIMIZED=YES;NOAUTOCLOSE=NO;OBJECTID=<XWP_DEFTREE>;', "R");
        rc = SysCreateObject("WPProgram", DetailsView, "<XWP_CFG3_1>", "EXENAME="dir'defdetls.cmd;CCVIEW=YES;MINIMIZED=YES;NOAUTOCLOSE=NO;OBJECTID=<XWP_DEFDETLS>;', "R");
    rc = SysCreateObject("WPProgram", SmallIcons, "<XWP_CFG3>", "EXENAME="dir'icosmall.cmd;CCVIEW=YES;MINIMIZED=YES;NOAUTOCLOSE=NO;OBJECTID=<XWP_ICOSMALL>;', "R");
    rc = SysCreateObject("WPProgram", NormalIcons, "<XWP_CFG3>", "EXENAME="dir'iconorm.cmd;CCVIEW=YES;MINIMIZED=YES;NOAUTOCLOSE=NO;OBJECTID=<XWP_ICONORM>;', "R");
    rc = SysCreateObject("WPProgram", ShowAllInTreeView, "<XWP_CFG3>", "EXENAME="dir'showall.cmd;CCVIEW=YES;MINIMIZED=YES;NOAUTOCLOSE=NO;OBJECTID=<XWP_SHOWALL>;', "R");

rc = SysCreateObject("WPFolder", "~XWorkplace", "<XWP_CONFIG>", "DEFAULTVIEW=ICON;ICONVIEW=NONFLOWED,MINI;SHOWALLINTREEVIEW=YES;ALWAYSSORT=NO;OBJECTID=<XWP_CFG4>", "U");
    rc = SysCreateObject("WPShadow", XWPSetup, "<XWP_CFG4>", "SHADOWID=<XWORKPLACE_SETUP>;OBJECTID=<XWORKPLACE_SETUPCFGSHADOW>;", "R");
    rc = SysCreateObject("WPShadow", WorkplaceShell, "<XWP_CFG4>", "SHADOWID=<XWP_WPS>;OBJECTID=<XWP_WPSCFGSHADOW>;", "R");
    rc = SysCreateObject("WPShadow", Mouse, "<XWP_CFG4>", "SHADOWID=<WP_MOUSE>;OBJECTID=<XWP_WPMOUSECFGSHADOW>;", "R");
    rc = SysCreateObject("WPShadow", Keyboard, "<XWP_CFG4>", "SHADOWID=<WP_KEYB>;OBJECTID=<XWP_WPKEYBCFGSHADOW>;", "R");
    rc = SysCreateObject("WPShadow", Screen, "<XWP_CFG4>", "SHADOWID=<XWP_SCREEN>;OBJECTID=<XWP_SCREENCFGSHADOW>;", "R");
    rc = SysCreateObject("WPShadow", "OS/2 Kernel", "<XWP_CFG4>", "SHADOWID=<XWP_KERNEL>;OBJECTID=<XWP_KERNELCFGSHADOW>;", "R");
    rc = SysCreateObject("WPProgram", "---", "<XWP_CFG4>", "OBJECTID=<XWP_SEP41>;", "R");
    rc = SysCreateObject("WPShadow", "User Guide", "<XWP_CFG4>", "SHADOWID=<XWP_REF>;OBJECTID=<XWP_REFCFGSHADOW>;", "R");
    rc = SysCreateObject("WPProgram", "---", "<XWP_CFG4>", "OBJECTID=<XWP_SEP42>;", "R");
    rc = SysCreateObject("WPShadow", ConfigFolder, "<XWP_CFG4>", "SHADOWID=<XWP_CONFIG>;OBJECTID=<XWP_CONFIGCFGSHADOW>;", "R");

rc = SysCreateObject("WPProgram", "---", "<XWP_CONFIG>", "OBJECTID=<XWP_SEP1>;", "R");

if (SysSearchPath("PATH", "NETSCAPE.EXE") \= "") then
    rc = SysCreateObject("WPProgram", "Netscape (DDE)", "<XWP_CONFIG>", "EXENAME="dir"netscdde.exe;OBJECTID=<XWP_NETSCAPE>;", "R");

rc = SysCreateObject("WPProgram", Treesize, "<XWP_CONFIG>", "EXENAME="dir"treesize.exe;CCVIEW=YES;OBJECTID=<XWP_TREESIZE>;", "R");

if (SysSearchPath("PATH", "ZIP.EXE") \= "") then
    rc = SysCreateObject("WPProgram", PackTree, "<XWP_CONFIG>", "EXENAME="dir"packtree.cmd;CCVIEW=YES;OBJECTID=<XWP_PACKTREE>;", "R");



