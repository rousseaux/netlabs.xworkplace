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
Media               = "Multimedia";
String              = "Setup string";
Mouse               = "Mouse";
Keyboard            = "Keyboard";
Sound               = "Sound";
XShutdown           = "eXtended Shutdown...";
/* font folder added with V0.9.7 */
FontFolder          = "Fonts";

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

HelpLibrary = "HELPLIBRARY="pdir||"\help\xfldr"LanguageCode".hlp;"

/* main folder */
rc = SysCreateObject("WPFolder", XFolderMain, "<WP_DESKTOP>", "DEFAULTVIEW=ICON;SHOWALLINTREEVIEW=YES;ALWAYSSORT=NO;ICONFILE="pdir||"\install\xwp.ico;ICONNFILE=1,"pdir||"\install\xwp_o.ico;"HelpLibrary"HELPPANEL=84;OBJECTID=<XWP_MAINFLDR>;", "U");

/* "SmartGuide" introduction: removed V0.9.7 (2000-12-10) [umoeller] */
/* if (SysSearchPath("PATH", "sguide.exe") \= "") then
    rc = SysCreateObject("WPProgram", XFolderIntro, "<XWP_MAINFLDR>", "EXENAME=sguide.exe;PARAMETERS="XFolderIntroFile";STARTUPDIR="dir";ICONFILE="idir"xfolder.ico;OBJECTID=<XWP_INTRO>;", "U"); */

/* User Guide */
rc = SysCreateObject("WPProgram", OnlineReference, "<XWP_MAINFLDR>", "EXENAME=view.exe;PARAMETERS="pdir||OnlineReferenceFile";OBJECTID=<XWP_REF>;", "U");

/* "Readme" shadow (added V0.9.2) */
rc = SysCreateObject("WPShadow", XWPSetup, "<XWP_MAINFLDR>", "SHADOWID="pdir||"README;OBJECTID=<XWP_READMEMAINSHADOW>;", "U");

/* create "XWorkplace Setup" (added V0.9.0) */
rc = SysCreateObject("XWPSetup", XWPSetup, "<WP_CONFIG>", "OBJECTID=<XWORKPLACE_SETUP>;", "U");
if (\rc) then
    rc = SysCreateObject("XWPSetup", XWPSetup, "<WP_DESKTOP>", "OBJECTID=<XWORKPLACE_SETUP>;", "U");
rc = SysCreateObject("WPShadow", XWPSetup, "<XWP_MAINFLDR>", "SHADOWID=<XWORKPLACE_SETUP>;OBJECTID=<XWP_SETUPMAINSHADOW>;", "U");

/* create XCenter (V0.9.7) */
rc = SysCreateObject("XCenter", "XCenter", "<XWP_MAINFLDR>", "OBJECTID=<XWP_XCENTER>;", "U");

/* "Font folder" ... added with V0.9.7 */
rc = SysCreateObject("XWPFontFolder", FontFolder, "<WP_CONFIG>", "DEFAULTVIEW=DETAILS;DETAILSCLASS=XWPFontObject;SORTCLASS=XWPFontObject;OBJECTID=<XWP_FONTFOLDER>;", "U");
if (\rc) then
    rc = SysCreateObject("XWPFontFolder", FontFolder, "<WP_DESKTOP>", "DEFAULTVIEW=DETAILS;DETAILSCLASS=XWPFontObject;OBJECTID=<XWP_FONTFOLDER>;", "U");
rc = SysCreateObject("WPShadow", FontFolder, "<XWP_MAINFLDR>", "SHADOWID=<XWP_FONTFOLDER>;OBJECTID=<XWP_FONTFOLDERSHADOW>;", "U");

/* create "Workplace Shell" */
rc = SysCreateObject("XFldWPS", WorkplaceShell, "<WP_CONFIG>", "OBJECTID=<XWP_WPS>;", "U");
if (\rc) then
    rc = SysCreateObject("XFldWPS", WorkplaceShell, "<WP_DESKTOP>", "OBJECTID=<XWP_WPS>;", "U");
rc = SysCreateObject("WPShadow", WorkplaceShell, "<XWP_MAINFLDR>", "SHADOWID=<XWP_WPS>;OBJECTID=<XWP_WPSMAINSHADOW>;", "U");

/* create "OS/2 Kernel" */
rc = SysCreateObject("XFldSystem", OS2Kernel, "<WP_CONFIG>", "OBJECTID=<XWP_KERNEL>;", "U");
if (\rc) then
    /* fixed with V0.85: changed following <XWP_WPS> to <XWP_KERNEL> */
    rc = SysCreateObject("XFldSystem", OS2Kernel, "<WP_DESKTOP>", "OBJECTID=<XWP_KERNEL>;", "U");
rc = SysCreateObject("WPShadow", OS2Kernel, "<XWP_MAINFLDR>", "SHADOWID=<XWP_KERNEL>;OBJECTID=<XWP_KERNELMAINSHADOW>;", "U");

/* create "Screen" (added V0.9.2) */
rc = SysCreateObject("XWPScreen", Screen, "<WP_CONFIG>", "OBJECTID=<XWP_SCREEN>;", "U");
if (\rc) then
    rc = SysCreateObject("XWPScreen", Screen, "<WP_DESKTOP>", "OBJECTID=<XWP_SCREEN>;", "U");
rc = SysCreateObject("WPShadow", Screen, "<XWP_MAINFLDR>", "SHADOWID=<XWP_SCREEN>;OBJECTID=<XWP_SCREENMAINSHADOW>;", "U");

/* create "Media" (added V0.9.5) */
rc = SysCreateObject("XWPMedia", Media, "<WP_CONFIG>", "OBJECTID=<XWP_MEDIA>;", "U");
if (\rc) then
    rc = SysCreateObject("XWPMedia", Media, "<WP_DESKTOP>", "OBJECTID=<XWP_MEDIA>;", "U");
rc = SysCreateObject("WPShadow", Screen, "<XWP_MAINFLDR>", "SHADOWID=<XWP_MEDIA>;OBJECTID=<XWP_MEDIAMAINSHADOW>;", "U");

/* "Mouse" shadow (added V0.9.2) */
rc = SysCreateObject("WPShadow", Mouse, "<XWP_MAINFLDR>", "SHADOWID=<WP_MOUSE>;OBJECTID=<XWP_MOUSEMAINSHADOW>;", "U");
/* "Keyboard" shadow (added V0.9.2) */
rc = SysCreateObject("WPShadow", Keyboard, "<XWP_MAINFLDR>", "SHADOWID=<WP_KEYB>;OBJECTID=<XWP_KEYBMAINSHADOW>;", "U");
/* "Sound" shadow (added V0.9.2) */
rc = SysCreateObject("WPShadow", Sound, "<XWP_MAINFLDR>", "SHADOWID=<WP_SOUND>;OBJECTID=<XWP_SOUNDMAINSHADOW>;", "U");

/* create "WPS Class List" (added V0.9.2) */
rc = SysCreateObject("XWPClassList", XWPClassList, "<WP_CONFIG>", "OBJECTID=<XWP_CLASSLIST>;", "U");
if (\rc) then
    rc = SysCreateObject("XWPClassList", XWPClassList, "<WP_DESKTOP>", "OBJECTID=<XWP_CLASSLIST>;", "U");
rc = SysCreateObject("WPShadow", XWPClassList, "<XWP_MAINFLDR>", "SHADOWID=<XWP_CLASSLIST>;OBJECTID=<XWP_CLASSLISTMAINSHADOW>;", "U");

/* create "Setup String" template (added V0.9.5) */
rc = SysCreateObject("XWPString", String, "<XWP_MAINFLDR>", "TEMPLATE=YES;OBJECTID=<XWP_STRINGTPL>;", "U");

/* XShutdown... changed with V0.9.7 */
rc = SysCreateObject("WPProgram", XShutdown, "<XWP_MAINFLDR>", "EXENAME="dir"xshutdwn.cmd;MINIMIZED=YES;"HelpLibrary"HELPPANEL=17;OBJECTID=<XWP_XSHUTDOWN>;", "U");

"@call "idir"crobj"LanguageCode

