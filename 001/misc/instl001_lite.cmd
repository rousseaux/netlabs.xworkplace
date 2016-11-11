/*
    Create XWorkplace-lite installation objects
    English version (001)
    (C) 1998-2005 Ulrich M”ller.
 */

/* change the following language code to your language. */
LanguageCode = "001";

/* Here come the titles of the objects to be created.
   Translate these to your language. */

XWorkplace          = "XWorkplace";
OS2                 = "OS/2"

WorkplaceShell      = "Workplace Shell";
Screen              = "Screen";
String              = "Setup String";
FontFolder          = "Fonts";
TrashCan            = "Trash Can";

Lockup              = "Lockup now";
FindObjects         = "Find objects";
Shutdown            = "Shut down";
XCenter             = "XCenter";

/*********************************************
 *
 *  NLS-independent portion
 *
 *  Note: All of this was rewritten with V0.9.19,
 *  but the NLS part above is unchanged. Just copy
 *  the entire chunk below from instl001.cmd to your
 *  translated file, and it should still work.
 *
 ********************************************/

call RxFuncAdd 'SysLoadFuncs', 'REXXUTIL', 'SysLoadFuncs'
call SysLoadFuncs

parse source dir;
parse var dir x1 x2 dir;
dir = filespec("D", dir)||filespec("P", dir);
pdir = left(dir, length(dir)-8);
idir = dir;
dir = pdir||"bin\";

HelpLibrary = "HELPLIBRARY="pdir||"\help\xfldr"LanguageCode".hlp;"

/* uninstall XWPSetup and XWPMedia if present */
call RemoveUnsupportedFeatures

/* "Fonts" folder  */
class = "XWPFontFolder";
title = FontFolder;
setup = "DEFAULTVIEW=DETAILS;DETAILSCLASS=XWPFontObject;SORTCLASS=XWPFontObject;"
id = "<XWP_FONTFOLDER>"
target = "<WP_CONFIG>";
call CreateObject;

/* create "Workplace Shell" */
class = "XFldWPS";
title = WorkplaceShell;
setup = "";
id = "<XWP_WPS>";
target = "<WP_CONFIG>";
call CreateObject;

/* create "Screen" */
class = "XWPScreen";
title = Screen;
setup = "";
id = "<XWP_SCREEN>";
target = "<WP_CONFIG>";
call CreateObject;

/* create "Setup String" template in Templates folder */
class = "XWPString";
title = String;
setup = "TEMPLATE=YES;"
id = "<XWP_STRINGTPL>";
target = "<WP_TEMPS>";
call CreateObject;

/* create trash can on desktop */
class = "XWPTrashCan";
title = TrashCan;
setup = "DEFAULTVIEW=DETAILS;ALWAYSSORT=YES;DETAILSCLASS=XWPTrashObject;SORTCLASS=XWPTrashObject;"
id = "<XWP_TRASHCAN>";
target = "<WP_DESKTOP>";
call CreateObject;

/* the following three added with V0.9.19
   and redone with V0.9.20 */
commonXWPString = "DEFAULTOBJECT=<WP_DESKTOP>;CONFIRMINVOCATION=NO;NOSTRINGPAGE=YES;HELPLIBRARY=WPHELP.HLP;HELPPANEL"

/* create "Lockup" setup string object */
class = "XWPString";
title = Lockup;
setup = "SETUPSTRING=MENUITEMSELECTED%3D705%3B;"commonXWPString"=8004;ICONRESOURCE=78,PMWP;"
id = "<XWP_LOCKUPSTR>"
target = "<WP_NOWHERE>";
call CreateObject;

/* create "Find objects" setup string object */
class = "XWPString";
title = FindObjects;
setup = "SETUPSTRING=MENUITEMSELECTED%3D8%3B;"commonXWPString"=1205;ICONRESOURCE=79,PMWP;";
id = "<XWP_FINDSTR>";
target = "<WP_NOWHERE>";
call CreateObject;

/* create "Shutdown" setup string object */
/* setup string modified to use POSTSHUTDOWN=YES V0.9.20 */
class = "XWPString";
title = Shutdown;
setup = "SETUPSTRING=POSTSHUTDOWN%3DYES%3B;"commonXWPString"=4001;ICONRESOURCE=80,PMWP;"
id = "<XWP_SHUTDOWNSTR>";
target = "<WP_NOWHERE>";
call CreateObject;

/* create XCenter in the "Utilities" folder */
class = "XCenter";
title = XCenter;
setup = "";
id = "<XWP_XCENTER>"
target = "<WP_TOOLS>";
call CreateObject;

"@call "idir"crobj"LanguageCode

exit;


CreateObject:
    len = length(id);
    if (len == 0) then do
        Say 'Error with object "'title'": object ID not given.';
        exit;
    end

    if (left(id, 1) \= '<') then do
        Say 'Error with object "'title'": object ID does not start with "<".';
        exit;
    end

    if (right(id, 1) \= '>') then do
        Say 'Error with object "'title'": object ID does not end with ">".';
        exit;
    end

    len = length(setup);
    if ((len > 0) & (right(setup, 1) \= ';')) then do
        Say 'Error with object "'title'": Setup string "'setup'" does not end in semicolon.';
        exit;
    end

    call charout , 'Creating "'title'" of class "'class'", setup "'setup'"... '
    rc = SysIni("USER", "PM_InstallObject", title||";"||class||";"||target||";RELOCATE", setup||"TITLE="||title||";OBJECTID="||id||";");
    if (rc <> "") then do
        Say 'Warning: object "'title'" of class "'class'" could not be created.'
    end
    else do
        Say "OK"
    end

    id = "";

    return;

/*
 * RemoveUnsupportedFeatures:
 *      This removes features that are explicitly unsupported by
 *      XWP-Lite from systems that previously had XWP-Full installed.
 */

RemoveUnsupportedFeatures:

rc = Destroy("<XWORKPLACE_SETUPCFGSHADOW>");
rc = Destroy("<XWP_SETUPMAINSHADOW>");
rc = Destroy("<XWORKPLACE_SETUP>");

rc = Destroy("<XWP_MEDIAMAINSHADOW>");
rc = Destroy("<XWP_MEDIA>");

rc = SysDeregisterObjectClass("XWPSetup");
rc = SysDeregisterObjectClass("XWPMedia");

/*
 * Destroy:
 *      sneaky little subproc which first sets the NODELETE=NO style
 *      to make sure we can really delete the object and then does a
 *      SysDestroyObject() on it.
 */

Destroy:
parse arg objid

call charout , 'Killing object ID "'objid'"...'

rc = SysSetObjectData(objid, "NODELETE=NO;");
if (rc \= 0) then do
    /* got that: */
    rc = SysDestroyObject(objid);
end

if (rc \= 0) then say " OK"; else say " failed.";

return rc;

