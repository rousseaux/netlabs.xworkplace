/* $Id$ */

/* XWorkplace installation program
   (C) 1998-99 Ulrich M”ller */

/* NOTE: this is just an updated version of the XFolder
   install script. Future versions will be installed
   using WarpIN. */

/* Optional parameters: three-digit language code
   to start installation with a certain language
   without being prompted. */

/* This script is language-independent. It relies on
   the files in the ./INSTALL subdirectory which should
   contain the necessary language-dependent strings. */

'@echo off'
signal on halt; trace off

call RxFuncAdd 'SysLoadFuncs', 'REXXUTIL', 'SysLoadFuncs'
call SysLoadFuncs

/* get the directory from where we're started */
parse source mydir;
parse var mydir x1 x2 mydir;
mydir = filespec("D", mydir)||filespec("P", mydir);
if (right(mydir, 1) = "\") then
    mydir = left(mydir, length(mydir)-1);

nl = '0a0d'x;
IniKey = "XFolder";
XFolderVersion = "1.00";

foundList.0 = 0

Say "XWorkplace "XFolderVersion" installation"

/* parse parameters: */
parse arg args

LanguageCodeValid = 0;

if (args \= "") then
    LanguageCode = args;
else do
    /* no parameters given: search ./INSTALL for language files
       and display them */
    do
        MsgFiles.0 = 0;
        Say ""
        rc = SysFileTree(mydir"\install\*.msg", "MsgFiles", "FO");
        if (msgFiles.0 = 0) then do
            /* none found: error */
            Say ""
            Say "The XWorkplace install program was unable to find any of its language"
            Say "files. This can have the following reasons:"
            Say "1) You have not downloaded any National Language Support (NLS) files."
            Say "   Starting with V0.83, NLS is no longer included in the base package,"
            Say "   so you must download at least one XWorkplace NLS package."
            Say "   See the README.1ST file for details."
            Say "2) The ZIP files you downloaded have been damaged."
            Say "3) You did not unzip the files properly. There must be a several"
            Say "   subdirectories under the directory where INSTALL.CMD resides."
            Say "   Place all the zip files in the same directory."
            Say "   With InfoZIP, use:           unzip <file>.zip"
            Say "   With PkZIP, use:             pkzip /extract /directories <file>.zip"
            "pause > nul"
            exit
        end
        else if (MsgFiles.0 = 1) then do
            /* only one set of language files: don't ask */
            fname = filespec("NAME", MsgFiles.1);
            LanguageCode = substr(fname, 5, 3)
            signal begin
        end
        else do while (LanguageCodeValid = 0)
            do i = 1 to MsgFiles.0
                /* for each language found, display the welcome
                   string from the msg file ("Enter 001 to install in English...") */
                fname = filespec("NAME", MsgFiles.i);
                "call bin\xhelp -F install\"fname" IdentifyMsg"
            end

            /* have the user enter the language code */
            Say ""
            call charout , ">>"
            parse pull LanguageCode

            if (stream("install\inst"LanguageCode".msg", "c", "QUERY EXISTS") = "") then do
                say "This is not a valid language code.";
            end
            else
                /* exit loop */
                LanguageCodeValid = 1;
        end
    end;
end;

/* now let's go */
begin:
/* this is a shortcut for displaying language-dependent stuff */
callxhelpStr = "call bin\xhelp -F install\inst"LanguageCode".msg"

cls

Say "XWorkplace "XFolderVersion" installation"

callxhelpStr "welcomeMsg"
call pause

callxhelpStr "licenceMsg"
call pause

loop:
/* ask for what we should do: register, deregister etc. */
callxhelpStr "queryActionMsg"
key = ''
do until (pos(key,"RDX") > 0)
   key = translate(SysGetKey("NOECHO"))
end /* do */
Say key

select
    when (key = "R") then do

        /* REGISTER */

        /* delete all INI keys of previous XFolder versions
           because the XFolder INI data changes with every
           release */
        rc = SysINI('USER', "XFolder_0_40", "DELETE:");
        rc = SysINI('USER', "XFolder_0_41", "DELETE:");
        rc = SysINI('USER', "XFolder_0_42", "DELETE:");
        rc = SysINI('USER', "XFolder 0.5x", "DELETE:");
        rc = SysINI('USER', "XFolder07", "DELETE:");
        rc = SysINI('USER', "XFolder7", "DELETE:");

        /* set INI data for this version */
        rc = SysINI('USER', IniKey, "XFolderPath", mydir||'0'x);
        rc = SysINI('USER', IniKey, "Language", LanguageCode||'0'x);
        rc = SysINI('USER', IniKey, "JustInstalled", '1'||'0'x);
        rc = SysINI('USER', IniKey, "Version", XFolderVersion||'0'x);

        /* remove WPS classes that might already be registered */
        callxhelpStr "unregisteringOldMsg";
        call pause

        say "";
        call charout , "Deregistering XWPClassList: "
        if (SysDeregisterObjectClass("XWPClassList")) then
            Say "OK";
            else Say "not found";
        call charout , "Deregistering XWPTrashCan: "
        if (SysDeregisterObjectClass("XWPTrashCan")) then
            Say "OK";
            else Say "not found";
        call charout , "Deregistering XWPTrashObject: "
        if (SysDeregisterObjectClass("XWPTrashObject")) then
            Say "OK";
            else Say "not found";
        call charout , "Deregistering XWPSetup: "
        if (SysDeregisterObjectClass("XWPSetup")) then
            Say "OK";
            else Say "not found";
        call charout , "Deregistering XFldWPS: "
        if (SysDeregisterObjectClass("XFldWPS")) then
            Say "OK";
            else Say "not found";
        call charout , "Deregistering XFldStartup: "
        if (SysDeregisterObjectClass("XFldStartup")) then
            Say "OK";
            else Say "not found";
        call charout , "Deregistering XFldShutdown: "
        if (SysDeregisterObjectClass("XFldShutdown")) then
            Say "OK";
            else Say "not found";
        "bin\repclass.exe WPSystem XFldSystem"
        "bin\repclass.exe WPProgram XFldProgram"
        "bin\repclass.exe WPDisk XFldDisk"
        "bin\repclass.exe WPDataFile XFldDataFile"
        "bin\repclass.exe WPDesktop XFldDesktop"
        "bin\repclass.exe WPFolder XFolder"
        "bin\repclass.exe WPFileSystem XFldFileSystem"
        "bin\repclass.exe WPObject XFldObject"
        "bin\repclass.exe WPProgramFile XFldProgramFile"
        "bin\repclass.exe WPSound XWPSound"
        "bin\repclass.exe WPMouse XWPMouse"
        "bin\repclass.exe WPKeyboard XWPKeyboard"

        /* install classes for this version */
        say "";
        callxhelpStr "registeringNewMsg";
        call pause

        say "";
        "bin\repclass.exe WPFolder XFolder" mydir||"\bin\xfldr.dll";
        "bin\repclass.exe WPObject XFldObject" mydir||"\bin\xfldr.dll";
        /* "bin\repclass.exe WPFileSystem XFldFileSystem" mydir||"\bin\xfldr.dll"; */
        "bin\repclass.exe WPDataFile XFldDataFile" mydir||"\bin\xfldr.dll";
        "bin\repclass.exe WPDisk XFldDisk" mydir||"\bin\xfldr.dll";
        "bin\repclass.exe WPDesktop XFldDesktop" mydir||"\bin\xfldr.dll";
        "bin\repclass.exe WPProgramFile XFldProgramFile" mydir||"\bin\xfldr.dll";
        "bin\repclass.exe WPSound XWPSound" mydir||"\bin\xfldr.dll";
        "bin\repclass.exe WPMouse XWPMouse" mydir||"\bin\xfldr.dll";
        "bin\repclass.exe WPKeyboard XWPKeyboard" mydir||"\bin\xfldr.dll";
        say "";
        call charout , "Registering XFldSystem: "
        if (SysRegisterObjectClass("XFldSystem", mydir||"\bin\xfldr.dll")) then
            Say "OK";
            else Say "failed";
        call charout , "Registering XFldWPS: "
        if (SysRegisterObjectClass("XFldWPS", mydir||"\bin\xfldr.dll")) then
            Say "OK";
            else Say "failed";
        call charout , "Registering XFldStartup: "
        if (SysRegisterObjectClass("XFldStartup", mydir||"\bin\xfldr.dll")) then
            Say "OK";
            else Say "failed";
        call charout , "Registering XFldShutdown: "
        if (SysRegisterObjectClass("XFldShutdown", mydir||"\bin\xfldr.dll")) then
            Say "OK";
            else Say "failed";
        call charout , "Registering XWPSetup: "
        if (SysRegisterObjectClass("XWPSetup", mydir||"\bin\xfldr.dll")) then
            Say "OK";
            else Say "failed";
        call charout , "Registering XWPTrashCan: "
        if (SysRegisterObjectClass("XWPTrashCan", mydir||"\bin\xfldr.dll")) then
            Say "OK";
            else Say "failed";
        call charout , "Registering XWPTrashObject: "
        if (SysRegisterObjectClass("XWPTrashObject", mydir||"\bin\xfldr.dll")) then
            Say "OK";
            else Say "failed";
        call charout , "Registering XWPClassList: "
        if (SysRegisterObjectClass("XWPClassList", mydir||"\bin\xfldr.dll")) then
            Say "OK";
            else Say "failed";

        /* check for Object Desktop TSEnhFolder */
        ClassList.0 = 0;
        call SysQueryClassList "ClassList.";
        if (ClassList.0 > 0) then
            do i = 1 to ClassList.0
                parse var ClassList.i Class DLL
                if (Class = "TSEnhFolder") then do
                    callxhelpStr "QueryODMsg";
                    key = ''
                    do until (pos(key,"YN") > 0)
                       key = translate(SysGetKey("NOECHO"))
                    end;
                    say key;
                    say "";
                    if (key = "Y") then do
                        /* remove TSEnhFolder and undo replacement */
                        "bin\repclass.exe WPFolder TSEnhFolder"
                        /* and register and replace again;
                           OBJDEFLD.DLL should be on the LIBPATH */
                        "bin\repclass.exe WPFolder TSEnhFolder OBJDEFLD"
                        callxhelpStr "ODDeregdMsg";
                        "pause >NUL"
                    end;
                end;
            end

        /* restart WPS? */
        callxhelpStr "shutdownMsg";
        key = ''
        do until (pos(key,"YN") > 0)
           key = translate(SysGetKey("NOECHO"))
        end /* do */
        Say key
        if (key = "Y") then
            "bin\wpsreset -D"
        else do
            callxhelpStr "howtoMsg";
            "pause >NUL"
        end
        exit
    end

    when (key = "D") then do
        /* UNREGISTER */

        callxhelpStr "unregisteringOldMsg";

        say "";
        call charout , "Deregistering XWPClassList: "
        if (SysDeregisterObjectClass("XWPClassList")) then
            Say "OK";
            else Say "not found";
        call charout , "Deregistering XWPTrashCan: "
        if (SysDeregisterObjectClass("XWPTrashCan")) then
            Say "OK";
            else Say "not found";
        call charout , "Deregistering XWPTrashObject: "
        if (SysDeregisterObjectClass("XWPTrashObject")) then
            Say "OK";
            else Say "not found";
        call charout , "Deregistering XWPSetup: "
        if (SysDeregisterObjectClass("XWPSetup")) then
            Say "OK";
            else Say "not found";
        call charout , "Deregistering XFldWPS: "
        if (SysDeregisterObjectClass("XFldWPS")) then
            Say "OK";
            else Say "not found";
        call charout , "Deregistering XFldStartup: "
        if (SysDeregisterObjectClass("XFldStartup")) then
            Say "OK";
            else Say "not found";
        call charout , "Deregistering XFldShutdown: "
        if (SysDeregisterObjectClass("XFldShutdown")) then
            Say "OK";
            else Say "not found";
        "bin\repclass.exe WPSystem XFldSystem"
        "bin\repclass.exe WPProgram XFldProgram"
        "bin\repclass.exe WPDisk XFldDisk"
        "bin\repclass.exe WPDataFile XFldDataFile"
        "bin\repclass.exe WPDesktop XFldDesktop"
        "bin\repclass.exe WPFolder XFolder"
        "bin\repclass.exe WPFileSystem XFldFileSystem"
        "bin\repclass.exe WPObject XFldObject"
        "bin\repclass.exe WPProgramFile XFldProgramFile"
        "bin\repclass.exe WPSound XWPSound"
        "bin\repclass.exe WPMouse XWPMouse"
        "bin\repclass.exe WPKeyboard XWPKeyboard"

        /* clean up INIs too? */
        say "";
        callxhelpStr "removeIniMsg";
        key = ''
        do until (pos(key,"YN") > 0)
           key = translate(SysGetKey("NOECHO"))
        end /* do */
        Say key
        if (key = "Y") then do
            /* XFolder keys in OS2.INI */
            rc = SysINI('USER', IniKey, "DELETE:");
            /* XFolder sounds in MMPM.INI */
            MMINI = GetBootDrive()||"\MMOS2\MMPM.INI";
            rc = SysINI(MMINI, "MMPM2_AlarmSounds", "555", "DELETE:");
            rc = SysINI(MMINI, "MMPM2_AlarmSounds", "556", "DELETE:");
            rc = SysINI(MMINI, "MMPM2_AlarmSounds", "558", "DELETE:");
            rc = SysINI(MMINI, "MMPM2_AlarmSounds", "559", "DELETE:");
            rc = SysINI(MMINI, "MMPM2_AlarmSounds", "560", "DELETE:");
        end;

        /* restart WPS? */
        say "";
        callxhelpStr "shutdownMsg";
        key = ''
        do until (pos(key,"YN") > 0)
           key = translate(SysGetKey("NOECHO"))
        end /* do */
        Say key
        if (key = "Y") then
            "bin\wpsreset -D"
        exit
    end;

    when (key = "X") then
        exit;
end /* select */

/* wrong key pressed: keep looping */
say ""
signal loop;

halt:
say '0d0a'x'Installation aborted!'
exit

createObjects:
        callxhelpStr "creatingObjMsg"
        "call install\crobj"LanguageCode".cmd"
return;

GetBootDrive: procedure
    parse upper value VALUE( "PATH",, "OS2ENVIRONMENT" ) with "\OS2\SYSTEM" -2 boot_drive +2
return boot_drive

pause:  procedure
key = ""
do while (key = "")
   key = SysGetKey("NOECHO")
end /* do */
say ""
return

strReplace: procedure
    /* syntax: result = strReplace(str, old, new) */
    /* will replace a with b in oldstr */
    parse arg str, old, new
    p = pos(old, str)
    if (p > 0) then
        str = left(str, p-1)||new||substr(str,p+length(old))
return str

