/*
    Create XWorkplace installation objects
    English version (001)
    (C) 1998-2000 Ulrich M”ller.
 */

/*
    This file gets called by WarpIN after all files have been unpacked.
    This calls CROBJxxx.CMD in turn to have the config folder created.
*/

/* change the following language code to your language. */
LanguageCode = "001";

/* Here come the titles of the objects to be created.
   Translate these to your language. */

/* Title of the Desktop folder; choose a faily long title,
   or the installation might fail if that object already
   exists */
XFolderMain         = "XWorkplace Installation";
/* "Introduction" object (SmartGuide) */
XFolderIntro        = "Introduction";
OnlineReference     = "XWorkplace User Guide";
/* other objects */
WorkplaceShell      = "Workplace Shell";
XWPSetup            = "XWorkplace Setup";
XWPClassList        = "WPS Class List";
OS2Kernel           = "OS/2 Kernel";
Screen              = "Screen";
Mouse               = "Mouse";
Keyboard            = "Keyboard";
Sound               = "Sound";
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

rc = SysCreateObject("WPFolder", XFolderMain, "<WP_DESKTOP>", "DEFAULTVIEW=ICON;SHOWALLINTREEVIEW=YES;ALWAYSSORT=NO;ICONFILE="pdir||"\install\xwp.ico;ICONNFILE=1,"pdir||"\install\xwp_o.ico;OBJECTID=<XWP_MAINFLDR>", "R");
if (SysSearchPath("PATH", "sguide.exe") \= "") then
    rc = SysCreateObject("WPProgram", XFolderIntro, "<XWP_MAINFLDR>", "EXENAME=sguide.exe;PARAMETERS="XFolderIntroFile";STARTUPDIR="dir";ICONFILE="idir"xfolder.ico;OBJECTID=<XWP_INTRO>", "R");
/* "Readme" shadow (added V0.9.2) */
rc = SysCreateObject("WPShadow", XWPSetup, "<XWP_MAINFLDR>", "SHADOWID="pdir||"README;OBJECTID=<XWP_READMEMAINSHADOW>;", "R");

/* User Guide */
rc = SysCreateObject("WPProgram", OnlineReference, "<XWP_MAINFLDR>", "EXENAME=view.exe;PARAMETERS="pdir||OnlineReferenceFile";OBJECTID=<XWP_REF>", "R");

/* XSHUTDWN.EXE */
rc = SysCreateObject("WPProgram", XShutdown, "<XWP_MAINFLDR>", "EXENAME="dir"xshutdwn.exe;OBJECTID=<XWP_XSHUTDOWN>", "R");

/* create "XWorkplace Setup" (added V0.9.0) */
rc = SysCreateObject("XWPSetup", XWPSetup, "<WP_CONFIG>", "OBJECTID=<XWORKPLACE_SETUP>;", "R");
if (\rc) then
    rc = SysCreateObject("XWPSetup", XWPSetup, "<WP_DESKTOP>", "OBJECTID=<XWORKPLACE_SETUP>;", "R");
rc = SysCreateObject("WPShadow", XWPSetup, "<XWP_MAINFLDR>", "SHADOWID=<XWORKPLACE_SETUP>;OBJECTID=<XWP_SETUPMAINSHADOW>;", "R");

/* create "WPS Class List" (added V0.9.2) */
rc = SysCreateObject("XWPClassList", XWPClassList, "<WP_CONFIG>", "OBJECTID=<XWP_CLASSLIST>;", "R");
if (\rc) then
    rc = SysCreateObject("XWPClassList", XWPClassList, "<WP_DESKTOP>", "OBJECTID=<XWP_CLASSLIST>;", "R");
rc = SysCreateObject("WPShadow", XWPClassList, "<XWP_MAINFLDR>", "SHADOWID=<XWP_CLASSLIST>;OBJECTID=<XWP_CLASSLISTMAINSHADOW>;", "R");

/* create "OS/2 Kernel" */
rc = SysCreateObject("XFldSystem", OS2Kernel, "<WP_CONFIG>", "OBJECTID=<XWP_KERNEL>;", "R");
if (\rc) then
    /* fixed with V0.85: changed following <XWP_WPS> to <XWP_KERNEL> */
    rc = SysCreateObject("XFldSystem", OS2Kernel, "<WP_DESKTOP>", "OBJECTID=<XWP_KERNEL>;", "R");
rc = SysCreateObject("WPShadow", OS2Kernel, "<XWP_MAINFLDR>", "SHADOWID=<XWP_KERNEL>;OBJECTID=<XWP_KERNELMAINSHADOW>;", "R");

/* create "Workplace Shell" */
rc = SysCreateObject("XFldWPS", WorkplaceShell, "<WP_CONFIG>", "OBJECTID=<XWP_WPS>", "R");
if (\rc) then
    rc = SysCreateObject("XFldWPS", WorkplaceShell, "<WP_DESKTOP>", "OBJECTID=<XWP_WPS>", "R");
rc = SysCreateObject("WPShadow", WorkplaceShell, "<XWP_MAINFLDR>", "SHADOWID=<XWP_WPS>;OBJECTID=<XWP_WPSMAINSHADOW>;", "R");

/* create "Screen" (added V0.9.2) */
rc = SysCreateObject("XWPScreen", Screen, "<WP_CONFIG>", "OBJECTID=<XWP_SCREEN>", "R");
if (\rc) then
    rc = SysCreateObject("XWPScreen", Screen, "<WP_DESKTOP>", "OBJECTID=<XWP_SCREEN>", "R");
rc = SysCreateObject("WPShadow", Screen, "<XWP_MAINFLDR>", "SHADOWID=<XWP_SCREEN>;OBJECTID=<XWP_SCREENMAINSHADOW>;", "R");

/* "Mouse" shadow (added V0.9.2) */
rc = SysCreateObject("WPShadow", Mouse, "<XWP_MAINFLDR>", "SHADOWID=<WP_MOUSE>;OBJECTID=<XWP_MOUSEMAINSHADOW>;", "R");
/* "Keyboard" shadow (added V0.9.2) */
rc = SysCreateObject("WPShadow", Keyboard, "<XWP_MAINFLDR>", "SHADOWID=<WP_KEYB>;OBJECTID=<XWP_KEYBMAINSHADOW>;", "R");
/* "Sound" shadow (added V0.9.2) */
rc = SysCreateObject("WPShadow", Sound, "<XWP_MAINFLDR>", "SHADOWID=<WP_SOUND>;OBJECTID=<XWP_SOUNDMAINSHADOW>;", "R");

"@call "idir"crobj"LanguageCode

