/* Make XWorkplace */
"@echo off"

/*
    This will call nmake to (re)build XWorkplace completely.

    Note that there are some directory checks below which might
    not apply to your system, because I am also using this script
    to update my "private" XWorkplace parts which are not included
    in the source distribution. This applies mostly to all the
    different NLS sources.

    However, this script is smart enough though to check for the
    existence of those directories.

    Please check the below variables and adjust them to your
    system before using this file.

    Also check the "setup.in" file, which has a few more options.
*/

/*
    Change the following if you need some cmd file
    which sets compiler environent variables; otherwise
    comment this out. This is only because I don't like
    all the VAC++ settings in CONFIG.SYS.
*/

"call envicc.cmd"

/*  Set other variables to override the defaults (which
    apply to my system only) in SETUP.IN.

    See README.TXT for details. */

/* XWPRUNNING (current XFolder/XWorkplace installation
   from where WPS classes are registered; the executables
   in there will be unlocked by the makefiles) */
/* SET XWPRUNNING= */

/* XWPRELEASE (target for composing all new tree for releases;
   this is only useful for creating a complete tree to create
   a new .WPI file (for WarpIN); this is only used with
   "nmake release") */
/* SET XWPRELEASE= */

"SET PROJECT_BASE_DIR="directory()

/* *** go! */

/* reset timer */
call time("E")
mydir = directory();

Say "***********************************************"
Say "*  Making XWorkplace main module (./MAIN/)... *"
Say "***********************************************"
"nmake -nologo"

Say "***********************************************"
Say "*  Making NLS files...                        *"
Say "***********************************************"
Say "   Making /001/XFLDR001.DLL"
"cd 001\dll"
"nmake /nologo"

Say "   Making /001/INF.001/XFLDR001.INF..."
"cd ..\inf.001"
"nmake /nologo"

Say "   Making /001/HELP.001/XFLDR001.HLP..."
"cd ..\help.001"
"nmake /nologo"
"cd ..\.."

/* if you want to create new NLS files, */
/* change the following "049"'s to your */
/* language code */

/* this is for German */
LanguageDir  = "049_de"
LanguageCode = "049"

if (stream(LanguageDir"\dll\makefile", "C", "QUERY EXISTS") \= "") then
do
    Say "  Making "LanguageDir"/XFLDR"LanguageCode".DLL..."
    "cd "LanguageDir
    "copy ..\001\inf.001\vers_2history.html .\inf."LanguageCode" > NUL"
    "copy ..\001\inf.001\notices_41thanks.html .\inf."LanguageCode" > NUL"
    "copy ..\001\inf.001\notices_42credits.html .\inf."LanguageCode" > NUL"
    /* "copy ..\001\inf.001\further*.html .\inf.049 > NUL" */
    "cd dll"
    "nmake /nologo"
    Say "  Making "LanguageDir"/XFLDR"LanguageCode".INF..."
    "cd ..\inf."LanguageCode
    "nmake /nologo"
    Say "  Making "LanguageDir"/XFLDR"LanguageCode".HLP..."
    "cd ..\help."LanguageCode
    "nmake /nologo"
    "cd "mydir
end;

/* this is for Spanish */

LanguageDir  = "034_es"
LanguageCode = "034"

if (stream(LanguageDir"\dll\makefile", "C", "QUERY EXISTS") \= "") then
do
    Say "  Making "LanguageDir"/XFLDR"LanguageCode".DLL..."
    "cd "LanguageDir
/*     "copy ..\001\inf.001\vers_2history.html .\inf."LanguageCode" > NUL" */
/*     "copy ..\001\inf.001\notices_41thanks.html .\inf."LanguageCode" > NUL" */
/*     "copy ..\001\inf.001\notices_42credits.html .\inf."LanguageCode" > NUL" */
/*     "copy ..\001\inf.001\further*.html .\inf."LanguageCode" > NUL" */
    "nmake /nologo xfldr"LanguageCode".mak"
/*    Say "  Making "LanguageDir"/XFLDR"LanguageCode".INF..."
     "cd inf."LanguageCode
    "nmake /nologo inf"LanguageCode".mak"
    Say "  Making "LanguageDir"/XFLDR"LanguageCode".HLP..."
    "cd ..\help."LanguageCode
    "nmake /nologo help"LanguageCode".mak" */
    "cd "mydir
end;

/* this is for Swedish */

LanguageDir  = "046_se"
LanguageCode = "046"

if (stream(LanguageDir"\dll\makefile", "C", "QUERY EXISTS") \= "") then
do
    Say "  Making "LanguageDir"/XFLDR"LanguageCode".DLL..."
    "cd "LanguageDir
/*     "copy ..\001\inf.001\vers_2history.html .\inf."LanguageCode" > NUL" */
/*     "copy ..\001\inf.001\notices_41thanks.html .\inf."LanguageCode" > NUL" */
/*     "copy ..\001\inf.001\notices_42credits.html .\inf."LanguageCode" > NUL" */
/*     "copy ..\001\inf.001\further*.html .\inf."LanguageCode" > NUL" */
    "nmake /nologo xfldr"LanguageCode".mak"
    Say "  Making "LanguageDir"/XFLDR"LanguageCode".INF..."
    "cd inf."LanguageCode
    "nmake /nologo inf"LanguageCode".mak"
    Say "  Making "LanguageDir"/XFLDR"LanguageCode".HLP..."
    "cd ..\help."LanguageCode
    "nmake /nologo help"LanguageCode".mak"
    "cd "mydir
end;

/* this is for Czech */

LanguageDir  = "421_cz"
LanguageCode = "421"

if (stream(LanguageDir"\dll\makefile", "C", "QUERY EXISTS") \= "") then
do
    Say "  Making "LanguageDir"/XFLDR"LanguageCode".DLL..."
    "cd "LanguageDir
/*     "copy ..\001\inf.001\vers_2history.html .\inf."LanguageCode" > NUL" */
/*     "copy ..\001\inf.001\notices_41thanks.html .\inf."LanguageCode" > NUL" */
/*     "copy ..\001\inf.001\notices_42credits.html .\inf."LanguageCode" > NUL" */
/*     "copy ..\001\inf.001\further*.html .\inf."LanguageCode" > NUL" */
    "nmake /nologo xfldr"LanguageCode".mak"
/*    Say "  Making "LanguageDir"/XFLDR"LanguageCode".INF..."
     "cd inf."LanguageCode
    "nmake /nologo inf"LanguageCode".mak"
    Say "  Making "LanguageDir"/XFLDR"LanguageCode".HLP..."
    "cd ..\help."LanguageCode
    "nmake /nologo help"LanguageCode".mak" */
    "cd "mydir
end;

/* XWorkplace Programming Guide and Reference; this
   only exists on my harddisk ;-) */
if (stream("..\progref\progref.mak", "C", "QUERY EXISTS") \= "") then do
    Say "  Making ..\progref\xfsrc.mak"
    "cd ..\progref"
    "copy ..\xwpsource\001\inf.001\notices_1licence.html"
    "nmake /nologo progref.mak"
    "cd "mydir
end

/* show elapsed time */
seconds = time("e"); /* in seconds */
minutes = trunc(seconds/60);
seconds2 = trunc(seconds - (minutes*60));
Say;
Say "Done!"
Say "Elapsed time: "minutes" minutes, "seconds2" seconds."
"pause"


