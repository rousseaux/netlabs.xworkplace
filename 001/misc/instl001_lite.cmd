/*
    Create eWorkplace installation objects
    English version (001)
    (C) 1998-2002 Ulrich M”ller.
 */

/* change the following language code to your language. */
LanguageCode = "001";

/* Here come the titles of the objects to be created.
   Translate these to your language. */

XWorkplace          = "eCS";
OS2                 = "eCS"

WorkplaceShell      = "Workplace Shell";
Screen              = "Screen";
String              = "Setup String";
FontFolder          = "Fonts";
TrashCan            = "Trash Can";

/* DO NOT CHANGE the following */
call RxFuncAdd 'SysLoadFuncs', 'REXXUTIL', 'SysLoadFuncs'
call SysLoadFuncs

parse source dir;
parse var dir x1 x2 dir;
dir = filespec("D", dir)||filespec("P", dir);
pdir = left(dir, length(dir)-8);
idir = dir;
dir = pdir||"bin\";

HelpLibrary = "HELPLIBRARY="pdir||"\help\xfldr"LanguageCode".hlp;"

/* create eCenter in eCS "Utilities" folder */
rc = SysCreateObject("XCenter", "eCenter", "<WP_TOOLS>", "OBJECTID=<XWP_XCENTER>;", "U");

/* "Fonts" folder  */
rc = SysCreateObject("XWPFontFolder", FontFolder, "<WP_CONFIG>", "DEFAULTVIEW=DETAILS;DETAILSCLASS=XWPFontObject;SORTCLASS=XWPFontObject;OBJECTID=<XWP_FONTFOLDER>;", "U");

/* create "Workplace Shell" */
rc = SysCreateObject("XFldWPS", WorkplaceShell, "<WP_CONFIG>", "OBJECTID=<XWP_WPS>;", "U");

/* create "Screen" */
rc = SysCreateObject("XWPScreen", Screen, "<WP_CONFIG>", "OBJECTID=<XWP_SCREEN>;", "U");

/* create "Setup String" template in Templates folder */
rc = SysCreateObject("XWPString", String, "<WP_TEMPS>", "TEMPLATE=YES;OBJECTID=<XWP_STRINGTPL>;", "U");

/* create trash can on desktop */
rc = SysCreateObject("XWPTrashCan", TrashCan, "<WP_DESKTOP>", "DEFAULTVIEW=DETAILS;ALWAYSSORT=YES;DETAILSCLASS=XWPTrashObject;SORTCLASS=XWPTrashObject;", "U");

"@call "idir"crobj"LanguageCode

