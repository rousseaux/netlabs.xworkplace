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

rc = SysCreateObject("WPFolder", ConfigFolder, "<XFOLDER_MAIN>", "ICONVIEW=NONFLOWED,MINI;DEFAULTVIEW=ICON;SHOWALLINTREEVIEW=YES;ALWAYSSORT=NO;OBJECTID=<XFOLDER_CONFIG>", "U");
if (\rc) then
    rc = SysCreateObject("WPFolder", ConfigFolder, "<WP_DESKTOP>", "ICONVIEW=NONFLOWED,MINI;DEFAULTVIEW=ICON;SHOWALLINTREEVIEW=YES;ALWAYSSORT=NO;OBJECTID=<XFOLDER_CONFIG>", "U");

/* create config folders */
rc = SysCreateObject("WPFolder", CommandLines, "<XFOLDER_CONFIG>", "DEFAULTVIEW=ICON;ICONVIEW=NONFLOWED,MINI;SHOWALLINTREEVIEW=YES;ALWAYSSORT=NO;OBJECTID=<XFOLDER_CFG1>", "U");
    if (SysSearchPath("PATH", "CMDSHL.CMD") \= "") then
        rc = SysCreateObject("WPProgram", OS2Win||" (CmdShl)", "<XFOLDER_CFG1>", "EXENAME=cmdshl.cmd;PARAMETERS=%;PROGTYPE=WINDOWABLEVIO;CCVIEW=YES;OBJECTID=<XFOLDER_CMDSHL>;", "R");
    if (SysSearchPath("PATH", "BASH.EXE") \= "") then
        rc = SysCreateObject("WPProgram", OS2Win||" (bash)", "<XFOLDER_CFG1>", "EXENAME=bash.exe;PARAMETERS=%;PROGTYPE=WINDOWABLEVIO;CCVIEW=YES;OBJECTID=<XFOLDER_BASH>;", "R");
    if (SysSearchPath("PATH", "KSH.EXE") \= "") then
        rc = SysCreateObject("WPProgram", OS2Win||" (ksh)", "<XFOLDER_CFG1>", "EXENAME=ksh.exe;PARAMETERS=%;PROGTYPE=WINDOWABLEVIO;CCVIEW=YES;OBJECTID=<XFOLDER_KSH>;", "R");
    rc = SysCreateObject("WPProgram", OS2Win, "<XFOLDER_CFG1>", "EXENAME=*;PARAMETERS=%;PROGTYPE=WINDOWABLEVIO;CCVIEW=YES;OBJECTID=<XFOLDER_OS2WIN>;", "R");
    rc = SysCreateObject("WPProgram", OS2Fullscreen, "<XFOLDER_CFG1>", "EXENAME=*;PARAMETERS=%;PROGTYPE=FULLSCREEN;CCVIEW=YES;OBJECTID=<XFOLDER_OS2FULL>;", "R");
    rc = SysCreateObject("WPProgram", DosWin, "<XFOLDER_CFG1>", "EXENAME=*;PARAMETERS=%;PROGTYPE=WINDOWEDVDM;CCVIEW=YES;OBJECTID=<XFOLDER_DOSWIN>;", "R");
    rc = SysCreateObject("WPProgram", DosFullscreen, "<XFOLDER_CFG1>", "EXENAME=*;PARAMETERS=%;PROGTYPE=VDM;CCVIEW=YES;OBJECTID=<XFOLDER_DOSFULL>;", "R");

rc = SysCreateObject("WPFolder", CreateAnother, "<XFOLDER_CONFIG>", "DEFAULTVIEW=ICON;ICONVIEW=NONFLOWED,MINI;SHOWALLINTREEVIEW=YES;ALWAYSSORT=NO;OBJECTID=<XFOLDER_CFG2>", "U");
    rc = SysCreateObject("WPFolder", Folder, "<XFOLDER_CFG2>", "TEMPLATE=YES;OBJECTID=<XFOLDER_FOLDERTEMPLATE>;", "R");
    rc = SysCreateObject("WPProgram", ProgramObject, "<XFOLDER_CFG2>", "TEMPLATE=YES;OBJECTID=<XFOLDER_PROGRAMTEMPLATE>;", "R");

rc = SysCreateObject("WPFolder", QuickSettings, "<XFOLDER_CONFIG>", "DEFAULTVIEW=ICON;ICONVIEW=NONFLOWED,MINI;SHOWALLINTREEVIEW=YES;ALWAYSSORT=NO;OBJECTID=<XFOLDER_CFG3>", "U");
    rc = SysCreateObject("WPFolder", DefaultView, "<XFOLDER_CFG3>", "DEFAULTVIEW=ICON;ICONVIEW=NONFLOWED,MINI;SHOWALLINTREEVIEW=YES;ALWAYSSORT=NO;OBJECTID=<XFOLDER_CFG3_1>", "U");
        rc = SysCreateObject("WPProgram", IconView, "<XFOLDER_CFG3_1>", "EXENAME="dir'deficon.cmd;CCVIEW=YES;MINIMIZED=YES;NOAUTOCLOSE=NO;OBJECTID=<XFOLDER_DEFICON>;', "R");
        rc = SysCreateObject("WPProgram", TreeView, "<XFOLDER_CFG3_1>", "EXENAME="dir'deftree.cmd;CCVIEW=YES;MINIMIZED=YES;NOAUTOCLOSE=NO;OBJECTID=<XFOLDER_DEFTREE>;', "R");
        rc = SysCreateObject("WPProgram", DetailsView, "<XFOLDER_CFG3_1>", "EXENAME="dir'defdetls.cmd;CCVIEW=YES;MINIMIZED=YES;NOAUTOCLOSE=NO;OBJECTID=<XFOLDER_DEFDETLS>;', "R");
    rc = SysCreateObject("WPProgram", SmallIcons, "<XFOLDER_CFG3>", "EXENAME="dir'icosmall.cmd;CCVIEW=YES;MINIMIZED=YES;NOAUTOCLOSE=NO;OBJECTID=<XFOLDER_ICOSMALL>;', "R");
    rc = SysCreateObject("WPProgram", NormalIcons, "<XFOLDER_CFG3>", "EXENAME="dir'iconorm.cmd;CCVIEW=YES;MINIMIZED=YES;NOAUTOCLOSE=NO;OBJECTID=<XFOLDER_ICONORM>;', "R");
    rc = SysCreateObject("WPProgram", ShowAllInTreeView, "<XFOLDER_CFG3>", "EXENAME="dir'showall.cmd;CCVIEW=YES;MINIMIZED=YES;NOAUTOCLOSE=NO;OBJECTID=<XFOLDER_SHOWALL>;', "R");

if (SysSearchPath("PATH", "NETSCAPE.EXE") \= "") then
    rc = SysCreateObject("WPProgram", "Netscape (DDE)", "<XFOLDER_CONFIG>", "EXENAME="dir"netscdde.exe;OBJECTID=<XFOLDER_NETSCAPE>;", "R");

rc = SysCreateObject("WPProgram", Treesize, "<XFOLDER_CONFIG>", "EXENAME="dir"treesize.exe;CCVIEW=YES;OBJECTID=<XFOLDER_TREESIZE>;", "R");

if (SysSearchPath("PATH", "ZIP.EXE") \= "") then
    rc = SysCreateObject("WPProgram", PackTree, "<XFOLDER_CONFIG>", "EXENAME="dir"packtree.cmd;CCVIEW=YES;OBJECTID=<XFOLDER_PACKTREE>;", "R");

rc = SysCreateObject("WPFolder", "~XWorkplace", "<XFOLDER_CONFIG>", "DEFAULTVIEW=ICON;ICONVIEW=NONFLOWED,MINI;SHOWALLINTREEVIEW=YES;ALWAYSSORT=NO;OBJECTID=<XFOLDER_CFG4>", "U");
    rc = SysCreateObject("WPShadow", "Online Reference", "<XFOLDER_CFG4>", "SHADOWID=<XFOLDER_REF>;OBJECTID=<XFOLDER_REFSHADOW>;", "R");
    rc = SysCreateObject("WPShadow", ConfigFolder, "<XFOLDER_CFG4>", "SHADOWID=<XFOLDER_CONFIG>;OBJECTID=<XFOLDER_CONFIGSHADOW>;", "R");
    rc = SysCreateObject("WPShadow", WorkplaceShell, "<XFOLDER_CFG4>", "SHADOWID=<XFOLDER_WPS>;OBJECTID=<XFOLDER_WPSCFGSHADOW>;", "R");
    rc = SysCreateObject("WPShadow", "OS/2 Kernel", "<XFOLDER_CFG4>", "SHADOWID=<XFOLDER_KERNEL>;OBJECTID=<XFOLDER_KERNELCFGSHADOW>;", "R");
    rc = SysCreateObject("WPShadow", XWPSetup, "<XFOLDER_CFG4>", "SHADOWID=<XWORKPLACE_SETUP>;OBJECTID=<XWORKPLACE_SETUPCFGSHADOW>;", "R");

rc = SysCreateObject("WPProgram", "---", "<XFOLDER_CONFIG>", "OBJECTID=<XFOLDER_SEP1>;", "R");


