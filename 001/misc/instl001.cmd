/* create XFolder installation objects
   English version (001)
   (W) (C) Ulrich M”ller. All rights reserved.
 */

/* This file is executed only by the XFolder installation script. */

/* change the following language code to your language. */
LanguageCode = "001";

/* Here come the titles of the objects to be created.
   Translate these to your language. */

/* Title of the Desktop folder; choose a faily long title,
   or the installation might fail if that object already
   exists */
XFolderMain         = "XFolder Installation";
/* "Introduction" object (SmartGuide) */
XFolderIntro        = "Introduction";
OnlineReference     = "XFolder Online Reference";
/* other objects */
WorkplaceShell      = "Workplace Shell";
XWPSetup            = "XWorkplace Setup";
OS2Kernel           = "OS/2 Kernel";
XShutdown           = "eXtended Shutdown...";

/* DO NOT CHANGE the following */
call RxFuncAdd 'SysLoadFuncs', 'REXXUTIL', 'SysLoadFuncs'
call SysLoadFuncs

parse source dir;
parse var dir x1 x2 dir;
dir = filespec("D", dir)||filespec("P", dir);
pdir = left(dir, length(dir)-8);
idir = dir;
dir = pdir||"bin\";

OnlineReferenceFile = "xfldr"LanguageCode".inf";
XFolderIntroFile    = "xfldr"LanguageCode".sgs";

rc = SysCreateObject("WPFolder", XFolderMain, "<WP_DESKTOP>", "DEFAULTVIEW=ICON;SHOWALLINTREEVIEW=YES;ALWAYSSORT=NO;OBJECTID=<XFOLDER_MAIN>", "U");
if (SysSearchPath("PATH", "sguide.exe") \= "") then
    rc = SysCreateObject("WPProgram", XFolderIntro, "<XFOLDER_MAIN>", "EXENAME=sguide.exe;PARAMETERS="XFolderIntroFile";STARTUPDIR="dir";ICONFILE="idir"xfolder.ico;OBJECTID=<XFOLDER_INTRO>", "U");
rc = SysCreateObject("WPProgram", OnlineReference, "<XFOLDER_MAIN>", "EXENAME=view.exe;PARAMETERS="pdir||OnlineReferenceFile";OBJECTID=<XFOLDER_REF>", "U");
rc = SysCreateObject("WPProgram", XShutdown, "<XFOLDER_MAIN>", "EXENAME="dir"xshutdwn.exe;OBJECTID=<XFOLDER_XSHUTDOWN>", "U");

/* create "XWorkplace Setup" (added V1.00) */
rc = SysCreateObject("XWPSetup", XWPSetup, "<WP_CONFIG>", "OBJECTID=<XWORKPLACE_SETUP>;", "U");
if (\rc) then
    rc = SysCreateObject("XWPSetup", XWPSetup, "<WP_DESKTOP>", "OBJECTID=<XWORKPLACE_SETUP>;", "U");
rc = SysCreateObject("WPShadow", XWPSetup, "<XFOLDER_MAIN>", "SHADOWID=<XWORKPLACE_SETUP>;OBJECTID=<XWORKPLACE_SETUPSHADOW>;", "U");

/* create "OS/2 Kernel" */
rc = SysCreateObject("XFldSystem", OS2Kernel, "<WP_CONFIG>", "OBJECTID=<XFOLDER_KERNEL>;", "U");
if (\rc) then
    /* fixed with V0.85: changed following <XFOLDER_WPS> to <XFOLDER_KERNEL> */
    rc = SysCreateObject("XFldSystem", OS2Kernel, "<WP_DESKTOP>", "OBJECTID=<XFOLDER_KERNEL>;", "U");
rc = SysCreateObject("WPShadow", OS2Kernel, "<XFOLDER_MAIN>", "SHADOWID=<XFOLDER_KERNEL>;OBJECTID=<XFOLDER_KERNELSHADOW>;", "U");

/* create "Workplace Shell" */
rc = SysCreateObject("XFldWPS", WorkplaceShell, "<WP_CONFIG>", "OBJECTID=<XFOLDER_WPS>", "U");
if (\rc) then
    rc = SysCreateObject("XFldWPS", WorkplaceShell, "<WP_DESKTOP>", "OBJECTID=<XFOLDER_WPS>", "U");
rc = SysCreateObject("WPShadow", WorkplaceShell, "<XFOLDER_MAIN>", "SHADOWID=<XFOLDER_WPS>;OBJECTID=<XFOLDER_WPSSHADOW>;", "U");

"@call "idir"crobj"LanguageCode

