/*
    Create XWorkplace installation objects
    English version (049)
    (C) 1998-2000 Ulrich M”ller.
 */
 
/*
    This file gets called by WarpIN after all files have been unpacked.
    This calls CROBJxxx.CMD in turn to have the config folder created.
*/
 
/* change the following language code to your language. */
LanguageCode = "049";
 
/* Here come the titles of the objects to be created.
   Translate these to your language. */
 
XWorkplace          = "eWorkplace";
OS2                 = "eCS"
 
/* Title of the Desktop folder; choose a faily long title,
   or the installation might fail if that object already
   exists */
XFolderMain         = XWorkplace||" Installation";
/* "Introduction" object (SmartGuide) */
XFolderIntro        = "Introduction";
OnlineReference     = XWorkplace||" User Guide";
/* other objects */
WorkplaceShell      = "Workplace Shell";
XWPSetup            = XWorkplace||" Setup";
XWPClassList        = "WPS Class List";
OS2Kernel           = OS2||" Kernel";
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
 
/* create "XWorkplace Setup" (added V0.9.0) */
rc = SysCreateObject("XWPSetup", XWPSetup, "<WP_CONFIG>", "OBJECTID=<XWORKPLACE_SETUP>;", "U");
 
/* create XCenter (V0.9.7) in eCS "Utilities" folder */
rc = SysCreateObject("XCenter", "eCenter", "<WP_TOOLS>", "OBJECTID=<XWP_XCENTER>;", "U");
 
/* "Font folder" ... added with V0.9.7 */
rc = SysCreateObject("XWPFontFolder", FontFolder, "<WP_CONFIG>", "DEFAULTVIEW=DETAILS;DETAILSCLASS=XWPFontObject;SORTCLASS=XWPFontObject;OBJECTID=<XWP_FONTFOLDER>;", "U");
 
/* create "Workplace Shell" */
rc = SysCreateObject("XFldWPS", WorkplaceShell, "<WP_CONFIG>", "OBJECTID=<XWP_WPS>;", "U");
 
/* create "OS/2 Kernel" */
/* rc = SysCreateObject("XFldSystem", OS2Kernel, "<WP_CONFIG>", "OBJECTID=<XWP_KERNEL>;", "U"); */
 
/* create "Screen" (added V0.9.2) */
rc = SysCreateObject("XWPScreen", Screen, "<WP_CONFIG>", "OBJECTID=<XWP_SCREEN>;", "U");
 
/* create "Media" (added V0.9.5) */
/* rc = SysCreateObject("XWPMedia", Media, "<WP_CONFIG>", "OBJECTID=<XWP_MEDIA>;", "U"); */
 
/* create "WPS Class List" (added V0.9.2) */
/* rc = SysCreateObject("XWPClassList", XWPClassList, "<WP_CONFIG>", "OBJECTID=<XWP_CLASSLIST>;", "U"); */
 
/* create "Setup String" template (added V0.9.5) */
/* rc = SysCreateObject("XWPString", String, "<WP_CONFIG>", "TEMPLATE=YES;OBJECTID=<XWP_STRINGTPL>;", "U"); */
 
"@call "idir"crobj"LanguageCode
 
