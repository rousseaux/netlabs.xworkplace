/*
    Create eWorkplace installation objects
    Dutch version (031)
    (C) 1998-2002 Ulrich M�ller.
 */

/* change the following language code to your language. */
LanguageCode = "031";

/* Here come the titles of the objects to be created.
   Translate these to your language. */

XWorkplace          = "eCS";
OS2                 = "eCS"

WorkplaceShell      = "Workplace Shell";
Screen              = "Beeldscherm";
String              = "Instellingen reeks";
FontFolder          = "Lettertypen";
TrashCan            = "Prullebak";

Lockup              = "Nu vergrendelen";
FindObjects         = "Objecten zoeken";
Shutdown            = "Afsluiten";
XCenter             = "eCenter";

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

/* create eCenter in eCS "Utilities" folder */
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
        Say 'Fout in object "'title'": object ID niet opgegeven.';
        exit;
    end

    if (left(id, 1) \= '<') then do
        Say 'Fout in object "'title'": object ID start niet met "<".';
        exit;
    end

    if (right(id, 1) \= '>') then do
        Say 'Fout in object "'title'": object ID eindigt niet met ">".';
        exit;
    end

    len = length(setup);
    if ((len > 0) & (right(setup, 1) \= ';')) then do
        Say 'Fout in object "'title'": Instellingen reeks "'setup'" eindigt niet met een puntkomma.';
        exit;
    end
    call charout , 'Maken van "'title'" van class "'class'", instellingen "'setup'"... '
    rc = SysCreateObject(class, title, target, setup"TITLE="title";OBJECTID="id";", "U");
    if (\rc) then do
        rc = SysCreateObject(class, title, "<WP_DESKTOP>", setup"TITLE="title";OBJECTID="id";", "U");
    end;
    if (\rc) then do
        Say 'Waarschuwing: object "'title'" van class "'class'" kan nite worden gemaakt.'
    end
    else do
        Say "OK"
    end

    id = "";

    return;

CreateObjectWithShadow:
    idOld = id;
    call CreateObject;

    class = "WPShadow";
    setup = "SHADOWID="idOld";"
    id = idOfShadow;
    target = "<XWP_MAINFLDR>";

    call CreateObject;

    return;
