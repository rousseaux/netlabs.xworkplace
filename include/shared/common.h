
/*
 *@@sourcefile common.h:
 *      header file for common.c.
 *
 *      This prototypes functions that are common to all
 *      parts of XWorkplace.
 *
 *      This file also declares all kinds of structures, id's,
 *      flags, strings, commands etc. which are used by
 *      all XWorkplace components.
 *
 *      As opposed to the declarations in dlgids.h, these
 *      declarations are NOT used by the NLS resource DLLs.
 *      The declarations have been separated to avoid
 *      unnecessary recompiles.
 *
 *      Note that with V0.9.0, all the debugging #define's have
 *      been moved to include\setup.h.
 *
 *@@include #define INCL_WINWINDOWMGR
 *@@include #define INCL_DOSMODULEMGR
 *@@include #include <os2.h>
 *@@include #include "helpers\xstring.h"    // only for setup sets
 *@@include #include <wpfolder.h>           // only for some features
 *@@include #include "shared\common.h"
 */

/*
 *      Copyright (C) 1997-2000 Ulrich M”ller.
 *      This file is part of the XWorkplace source package.
 *      XWorkplace is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published
 *      by the Free Software Foundation, in version 2 as it comes in the
 *      "COPYING" file of the XWorkplace main distribution.
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 */

#ifndef COMMON_HEADER_INCLUDED
    #define COMMON_HEADER_INCLUDED

    /*
     *  All these constants are declared as "extern" in
     *  common.h. They all used to be #define's in common.h,
     *  which put a lot of duplicates of them into the .obj
     *  files (and also stress on the compiler, since it had
     *  to do comparisons on them... and didn't even know that
     *  they were really constant).
     *
     *  These have been moved here with V0.9.7 (2001-01-17) [umoeller]
     *  and converted to DECLARE_STRING macros with
     *  V0.9.14 V0.9.14 (2001-07-31) [umoeller].
     */

    // DECLARE_STRING is a handy macro which saves us from
    // keeping two string lists in both common.h and common.c.
    // If this include file is included from common.c,
    // INCLUDE_COMMON_PRIVATE is #define'd, and the actual
    // string is defined as a global variable. Otherwise
    // it is only declared as "extern" so other files can
    // see it.

    #ifdef INCLUDE_COMMON_PRIVATE
        #define DECLARE_STRING(str, def) const char *str = def
    #else
        #define DECLARE_STRING(str, def) extern const char *str;
    #endif

    /********************************************************************
     *
     *   INI keys
     *
     ********************************************************************/

    /*
     * XWorkplace application:
     *
     */

    // INI key used with V0.9.1 and above
    DECLARE_STRING(INIAPP_XWORKPLACE, "XWorkplace");

    // INI key used by XFolder and XWorkplace 0.9.0;
    // this is checked for if INIAPP_XWORKPLACE is not
    // found and converted
    // DECLARE_STRING(INIAPP_OLDXFOLDER, "XFolder");
    DECLARE_STRING(INIAPP_OLDXFOLDER, "XFolder");

    /*
     * XWorkplace keys:
     *      Add the keys you are using for storing your data here.
     *      Note: If anything has been marked as "removed" here,
     *      do not use that string, because it might still exist
     *      in a user's OS2.INI file.
     */

    // DECLARE_STRING(INIKEY_DEFAULTTITLE, "DefaultTitle");       removed V0.9.0
    DECLARE_STRING(INIKEY_GLOBALSETTINGS, "GlobalSettings");
    // DECLARE_STRING(INIKEY_XFOLDERPATH, "XFolderPath");        removed V0.81 (I think)
    DECLARE_STRING(INIKEY_ACCELERATORS, "Accelerators");
    DECLARE_STRING(INIKEY_LANGUAGECODE, "Language");
    DECLARE_STRING(INIKEY_JUSTINSTALLED, "JustInstalled");
    // DECLARE_STRING(INIKEY_DONTDOSTARTUP, "DontDoStartup");      removed V0.84 (I think)
    // DECLARE_STRING(INIKEY_LASTPID, "LastPID");            removed V0.84 (I think)
    DECLARE_STRING(INIKEY_FAVORITEFOLDERS, "FavoriteFolders");
    DECLARE_STRING(INIKEY_QUICKOPENFOLDERS, "QuickOpenFolders");

    DECLARE_STRING(INIKEY_WNDPOSSTARTUP, "WndPosStartup");
    DECLARE_STRING(INIKEY_WNDPOSNAMECLASH, "WndPosNameClash");
    DECLARE_STRING(INIKEY_NAMECLASHFOCUS, "NameClashLastFocus");

    DECLARE_STRING(INIKEY_STATUSBARFONT, "SB_Font");
    DECLARE_STRING(INIKEY_SBTEXTNONESEL, "SB_NoneSelected");
    DECLARE_STRING(INIKEY_SBTEXT_WPOBJECT, "SB_WPObject");
    DECLARE_STRING(INIKEY_SBTEXT_WPPROGRAM, "SB_WPProgram");
    DECLARE_STRING(INIKEY_SBTEXT_WPFILESYSTEM, "SB_WPDataFile");
    DECLARE_STRING(INIKEY_SBTEXT_WPURL, "SB_WPUrl");
    DECLARE_STRING(INIKEY_SBTEXT_WPDISK, "SB_WPDisk");
    DECLARE_STRING(INIKEY_SBTEXT_WPFOLDER, "SB_WPFolder");
    DECLARE_STRING(INIKEY_SBTEXTMULTISEL, "SB_MultiSelected");
    DECLARE_STRING(INIKEY_SB_LASTCLASS, "SB_LastClass");
    DECLARE_STRING(INIKEY_DLGFONT, "DialogFont");

    DECLARE_STRING(INIKEY_BOOTMGR, "RebootTo");
    DECLARE_STRING(INIKEY_AUTOCLOSE, "AutoClose");

    DECLARE_STRING(DEFAULT_LANGUAGECODE, "001");

    // window position of "WPS Class list" window (V0.9.0)
    DECLARE_STRING(INIKEY_WNDPOSCLASSINFO, "WndPosClassInfo");

    // last directory used on "Sound" replacement page (V0.9.0)
    DECLARE_STRING(INIKEY_XWPSOUNDLASTDIR, "XWPSound:LastDir");
    // last sound scheme selected (V0.9.0)
    DECLARE_STRING(INIKEY_XWPSOUNDSCHEME, "XWPSound:Scheme");

    // boot logo .BMP file (V0.9.0)
    DECLARE_STRING(INIKEY_BOOTLOGOFILE, "BootLogoFile");

    // last ten selections in "Select some" (V0.9.0)
    DECLARE_STRING(INIKEY_LAST10SELECTSOME, "SelectSome");

    // supported drives in XWPTrashCan (V0.9.1 (99-12-14) [umoeller])
    DECLARE_STRING(INIKEY_TRASHCANDRIVES, "TrashCan::Drives");

    // window pos of file operations status window V0.9.1 (2000-01-30) [umoeller]
    DECLARE_STRING(INIKEY_FILEOPSPOS, "WndPosFileOpsStatus");

    // window pos of "Partitions" view V0.9.2 (2000-02-29) [umoeller]
    DECLARE_STRING(INIKEY_WNDPOSPARTITIONS, "WndPosPartitions");

    // window position of XMMVolume control V0.9.6 (2000-11-09) [umoeller]
    DECLARE_STRING(INIKEY_WNDPOSXMMVOLUME, "WndPosXMMVolume");

    // window position of XMMCDPlayer V0.9.7 (2000-12-20) [umoeller]
    DECLARE_STRING(INIKEY_WNDPOSXMMCDPLAY, "WndPosXMMCDPlayer::");
                    // object handle appended

    // font samples (XWPFontObject) V0.9.7 (2001-01-17) [umoeller]
    DECLARE_STRING(INIKEY_FONTSAMPLEWNDPOS, "WndPosFontSample");
    DECLARE_STRING(INIKEY_FONTSAMPLESTRING, "FontSampleString");
    DECLARE_STRING(INIKEY_FONTSAMPLEHINTS, "FontSampleHints");

    // XFldStartup V0.9.9 (2001-03-19) [pr]
    DECLARE_STRING(INIKEY_XSTARTUPFOLDERS, "XStartupFolders");
    DECLARE_STRING(INIKEY_XSAVEDSTARTUPFOLDERS, "XSavedStartupFolders");

    // file dialog V0.9.11 (2001-04-18) [umoeller]
    DECLARE_STRING(INIKEY_WNDPOSFILEDLG, "WndPosFileDlg");
    DECLARE_STRING(INIKEY_FILEDLGSETTINGS, "FileDlgSettings");

    /*
     * file type hierarchies:
     *
     */

    // application for file type hierarchies
    DECLARE_STRING(INIAPP_XWPFILETYPES, "XWorkplace:FileTypes");   // added V0.9.0
    DECLARE_STRING(INIAPP_XWPFILEFILTERS, "XWorkplace:FileFilters"); // added V0.9.0

    DECLARE_STRING(INIAPP_REPLACEFOLDERREFRESH, "ReplaceFolderRefresh");
                                        // V0.9.9 (2001-01-31) [umoeller]

    /*
     * some default WPS INI keys:
     *
     */

    DECLARE_STRING(WPINIAPP_LOCATION, "PM_Workplace:Location");
    DECLARE_STRING(WPINIAPP_FOLDERPOS, "PM_Workplace:FolderPos");
    DECLARE_STRING(WPINIAPP_ASSOCTYPE, "PMWP_ASSOC_TYPE");
    DECLARE_STRING(WPINIAPP_ASSOCFILTER, "PMWP_ASSOC_FILTER");

    /********************************************************************
     *
     *   Standard WPS object IDs
     *
     ********************************************************************/

    DECLARE_STRING(WPOBJID_DESKTOP, "<WP_DESKTOP>");

    DECLARE_STRING(WPOBJID_KEYB, "<WP_KEYB>");
    DECLARE_STRING(WPOBJID_MOUSE, "<WP_MOUSE>");
    DECLARE_STRING(WPOBJID_CNTRY, "<WP_CNTRY>");
    DECLARE_STRING(WPOBJID_SOUND, "<WP_SOUND>");
    DECLARE_STRING(WPOBJID_SYSTEM, "<WP_SYSTEM>"); // V0.9.9
    DECLARE_STRING(WPOBJID_POWER, "<WP_POWER>");
    DECLARE_STRING(WPOBJID_WINCFG, "<WP_WINCFG>");

    DECLARE_STRING(WPOBJID_HIRESCLRPAL, "<WP_HIRESCLRPAL>");
    DECLARE_STRING(WPOBJID_LORESCLRPAL, "<WP_LORESCLRPAL>");
    DECLARE_STRING(WPOBJID_FNTPAL, "<WP_FNTPAL>");
    DECLARE_STRING(WPOBJID_SCHPAL96, "<WP_SCHPAL96>");

    DECLARE_STRING(WPOBJID_LAUNCHPAD, "<WP_LAUNCHPAD>");
    DECLARE_STRING(WPOBJID_WARPCENTER, "<WP_WARPCENTER>");

    DECLARE_STRING(WPOBJID_SPOOL, "<WP_SPOOL>");
    DECLARE_STRING(WPOBJID_VIEWER, "<WP_VIEWER>");
    DECLARE_STRING(WPOBJID_SHRED, "<WP_SHRED>");
    DECLARE_STRING(WPOBJID_CLOCK, "<WP_CLOCK>");

    DECLARE_STRING(WPOBJID_START, "<WP_START>");
    DECLARE_STRING(WPOBJID_TEMPS, "<WP_TEMPS>");
    DECLARE_STRING(WPOBJID_DRIVES, "<WP_DRIVES>");

    /********************************************************************
     *
     *   XWorkplace object IDs
     *
     ********************************************************************/

    // all of these have been redone with V0.9.2

    // folders
    DECLARE_STRING(XFOLDER_MAINID, "<XWP_MAINFLDR>");
    DECLARE_STRING(XFOLDER_CONFIGID, "<XWP_CONFIG>");

    DECLARE_STRING(XFOLDER_STARTUPID, "<XWP_STARTUP>");
    DECLARE_STRING(XFOLDER_SHUTDOWNID, "<XWP_SHUTDOWN>");
    DECLARE_STRING(XFOLDER_FONTFOLDERID, "<XWP_FONTFOLDER>");

    DECLARE_STRING(XFOLDER_WPSID, "<XWP_WPS>");
    DECLARE_STRING(XFOLDER_KERNELID, "<XWP_KERNEL>");
    DECLARE_STRING(XFOLDER_SCREENID, "<XWP_SCREEN>");
    DECLARE_STRING(XFOLDER_MEDIAID, "<XWP_MEDIA>");

    DECLARE_STRING(XFOLDER_CLASSLISTID, "<XWP_CLASSLIST>");
    DECLARE_STRING(XFOLDER_TRASHCANID, "<XWP_TRASHCAN>");
    DECLARE_STRING(XFOLDER_XCENTERID, "<XWP_XCENTER>");
    DECLARE_STRING(XFOLDER_STRINGTPLID, "<XWP_STRINGTPL>"); // V0.9.9

    DECLARE_STRING(XFOLDER_INTROID, "<XWP_INTRO>");
    DECLARE_STRING(XFOLDER_USERGUIDE, "<XWP_REF>");

    // DECLARE_STRING(XWORKPLACE_ARCHIVE_MARKER, "xwparchv.tmp");
                // archive marker file in Desktop directory V0.9.4 (2000-08-03) [umoeller]
                // removed V0.9.13 (2001-06-14) [umoeller]

    /********************************************************************
     *
     *   WPS class names V0.9.14 (2001-07-31) [umoeller]
     *
     ********************************************************************/

    DECLARE_STRING(G_pcszXFldObject, "XFldObject");
    DECLARE_STRING(G_pcszXFolder, "XFolder");
    DECLARE_STRING(G_pcszXFldDisk, "XFldDisk");
    DECLARE_STRING(G_pcszXFldDesktop, "XFldDesktop");
    DECLARE_STRING(G_pcszXFldDataFile, "XFldDataFile");
    DECLARE_STRING(G_pcszXFldProgramFile, "XFldProgramFile");
    DECLARE_STRING(G_pcszXWPSound, "XWPSound");
    DECLARE_STRING(G_pcszXWPMouse, "XWPMouse");
    DECLARE_STRING(G_pcszXWPKeyboard, "XWPKeyboard");

    DECLARE_STRING(G_pcszXWPSetup, "XWPSetup");
    DECLARE_STRING(G_pcszXFldSystem, "XFldSystem");
    DECLARE_STRING(G_pcszXFldWPS, "XFldWPS");
    DECLARE_STRING(G_pcszXWPScreen, "XWPScreen");
    DECLARE_STRING(G_pcszXWPMedia, "XWPMedia");
    DECLARE_STRING(G_pcszXFldStartup, "XFldStartup");
    DECLARE_STRING(G_pcszXFldShutdown, "XFldShutdown");
    DECLARE_STRING(G_pcszXWPClassList, "XWPClassList");
    DECLARE_STRING(G_pcszXWPTrashCan, "XWPTrashCan");
    DECLARE_STRING(G_pcszXWPTrashObject, "XWPTrashObject");
    DECLARE_STRING(G_pcszXWPString, "XWPString");
    DECLARE_STRING(G_pcszXCenter, "XCenter");
    DECLARE_STRING(G_pcszXWPFontFolder, "XWPFontFolder");
    DECLARE_STRING(G_pcszXWPFontFile, "XWPFontFile");
    DECLARE_STRING(G_pcszXWPFontObject, "XWPFontObject");

    // @@todo
    DECLARE_STRING(G_pcszXWPProgram, "XWPProgram");

    DECLARE_STRING(G_pcszWPObject, "WPObject");
    DECLARE_STRING(G_pcszWPFolder, "WPFolder");
    DECLARE_STRING(G_pcszWPDisk, "WPDisk");
    DECLARE_STRING(G_pcszWPDesktop, "WPDesktop");
    DECLARE_STRING(G_pcszWPDataFile, "WPDataFile");
    DECLARE_STRING(G_pcszWPProgram, "WPProgram");
    DECLARE_STRING(G_pcszWPProgramFile, "WPProgramFile");
    DECLARE_STRING(G_pcszWPKeyboard, "WPKeyboard");
    DECLARE_STRING(G_pcszWPMouse, "WPMouse");
    DECLARE_STRING(G_pcszWPCountry, "WPCountry");
    DECLARE_STRING(G_pcszWPSound, "WPSound");
    DECLARE_STRING(G_pcszWPSystem, "WPSystem");
    DECLARE_STRING(G_pcszWPPower, "WPPower");
    DECLARE_STRING(G_pcszWPWinConfig, "WPWinConfig");
    DECLARE_STRING(G_pcszWPColorPalette, "WPColorPalette");
    DECLARE_STRING(G_pcszWPFontPalette, "WPFontPalette");
    DECLARE_STRING(G_pcszWPSchemePalette, "WPSchemePalette");

    DECLARE_STRING(G_pcszWPLaunchPad, "WPLaunchPad");
    DECLARE_STRING(G_pcszSmartCenter, "SmartCenter");

    DECLARE_STRING(G_pcszWPSpool, "WPSpool");
    DECLARE_STRING(G_pcszWPMinWinViewer, "WPMinWinViewer");
    DECLARE_STRING(G_pcszWPShredder, "WPShredder");
    DECLARE_STRING(G_pcszWPClock, "WPClock");

    DECLARE_STRING(G_pcszWPStartup, "WPStartup");
    DECLARE_STRING(G_pcszWPTemplates, "WPTemplates");
    DECLARE_STRING(G_pcszWPDrives, "WPDrives");

    /********************************************************************
     *
     *   Thread object windows
     *
     ********************************************************************/

    // object window class names (added V0.9.0)
    DECLARE_STRING(WNDCLASS_WORKEROBJECT, "XWPWorkerObject");
    DECLARE_STRING(WNDCLASS_QUICKOBJECT, "XWPQuickObject");
    DECLARE_STRING(WNDCLASS_FILEOBJECT, "XWPFileObject");

    DECLARE_STRING(WNDCLASS_THREAD1OBJECT, "XWPThread1Object");
    DECLARE_STRING(WNDCLASS_SUPPLOBJECT, "XWPSupplFolderObject");
    DECLARE_STRING(WNDCLASS_APIOBJECT, "XWPAPIObject");

    /********************************************************************
     *
     *   Constants
     *
     ********************************************************************/

    /*
     *  All these constants have been made "const char*" pointers
     *  with V0.9.7 (they used to be #define's before). The actual
     *  character constants are now on top of common.c.
     */

    /* extern const char *INIAPP_XWORKPLACE;
    extern const char *INIAPP_OLDXFOLDER;

    extern const char *INIKEY_GLOBALSETTINGS;
    extern const char *INIKEY_ACCELERATORS;
    extern const char *INIKEY_LANGUAGECODE;
    extern const char *INIKEY_JUSTINSTALLED;
    extern const char *INIKEY_FAVORITEFOLDERS;
    extern const char *INIKEY_QUICKOPENFOLDERS;

    extern const char *INIKEY_WNDPOSSTARTUP;
    extern const char *INIKEY_WNDPOSNAMECLASH;
    extern const char *INIKEY_NAMECLASHFOCUS;

    extern const char *INIKEY_STATUSBARFONT;
    extern const char *INIKEY_SBTEXTNONESEL;
    extern const char *INIKEY_SBTEXT_WPOBJECT;
    extern const char *INIKEY_SBTEXT_WPPROGRAM;
    extern const char *INIKEY_SBTEXT_WPFILESYSTEM;
    extern const char *INIKEY_SBTEXT_WPURL;
    extern const char *INIKEY_SBTEXT_WPDISK;
    extern const char *INIKEY_SBTEXT_WPFOLDER;
    extern const char *INIKEY_SBTEXTMULTISEL;
    extern const char *INIKEY_SB_LASTCLASS;
    extern const char *INIKEY_DLGFONT;

    extern const char *INIKEY_BOOTMGR;
    extern const char *INIKEY_AUTOCLOSE;

    extern const char *DEFAULT_LANGUAGECODE;

    extern const char *INIKEY_WNDPOSCLASSINFO;

    extern const char *INIKEY_XWPSOUNDLASTDIR;
    extern const char *INIKEY_XWPSOUNDSCHEME;

    extern const char *INIKEY_BOOTLOGOFILE;

    extern const char *INIKEY_LAST10SELECTSOME;

    extern const char *INIKEY_TRASHCANDRIVES;

    extern const char *INIKEY_FILEOPSPOS;

    extern const char *INIKEY_WNDPOSPARTITIONS;

    extern const char *INIKEY_WNDPOSXMMVOLUME;

    extern const char *INIKEY_WNDPOSXMMCDPLAY;

    extern const char *INIKEY_FONTSAMPLEWNDPOS;
    extern const char *INIKEY_FONTSAMPLESTRING;
    extern const char *INIKEY_FONTSAMPLEHINTS;

    // added V0.9.9 (2001-03-19) [pr]
    extern const char *INIKEY_XSTARTUPFOLDERS;
    extern const char *INIKEY_XSAVEDSTARTUPFOLDERS;

    // added V0.9.11 (2001-04-18) [umoeller]
    extern const char *INIKEY_WNDPOSFILEDLG;
    extern const char *INIKEY_FILEDLGSETTINGS;

    extern const char *INIAPP_XWPFILETYPES;   // added V0.9.0
    extern const char *INIAPP_XWPFILEFILTERS; // added V0.9.0

    extern const char *INIAPP_REPLACEFOLDERREFRESH; // added V0.9.9 (2001-01-31) [umoeller]

    extern const char *WPINIAPP_LOCATION;
    extern const char *WPINIAPP_FOLDERPOS;
    extern const char *WPINIAPP_ASSOCTYPE;
    extern const char *WPINIAPP_ASSOCFILTER; */

    /********************************************************************
     *
     *   Standard WPS object IDs
     *
     ********************************************************************/

    /* extern const char *WPOBJID_DESKTOP;

    extern const char *WPOBJID_KEYB;
    extern const char *WPOBJID_MOUSE;
    extern const char *WPOBJID_CNTRY;
    extern const char *WPOBJID_SOUND;
    extern const char *WPOBJID_SYSTEM;
    extern const char *WPOBJID_POWER;
    extern const char *WPOBJID_WINCFG;

    extern const char *WPOBJID_HIRESCLRPAL;
    extern const char *WPOBJID_LORESCLRPAL;
    extern const char *WPOBJID_FNTPAL;
    extern const char *WPOBJID_SCHPAL96;

    extern const char *WPOBJID_LAUNCHPAD;
    extern const char *WPOBJID_WARPCENTER;

    extern const char *WPOBJID_SPOOL;
    extern const char *WPOBJID_VIEWER;
    extern const char *WPOBJID_SHRED;
    extern const char *WPOBJID_CLOCK;

    extern const char *WPOBJID_START;
    extern const char *WPOBJID_TEMPS;
    extern const char *WPOBJID_DRIVES; */

    /********************************************************************
     *
     *   XWorkplace object IDs
     *
     ********************************************************************/

    // all of these have been redone with V0.9.2

    // folders
    /* extern const char *XFOLDER_MAINID;
    extern const char *XFOLDER_CONFIGID;

    extern const char *XFOLDER_STARTUPID;
    extern const char *XFOLDER_SHUTDOWNID;
    extern const char *XFOLDER_FONTFOLDERID;

    extern const char *XFOLDER_WPSID;
    extern const char *XFOLDER_KERNELID;
    extern const char *XFOLDER_SCREENID;
    extern const char *XFOLDER_MEDIAID;

    extern const char *XFOLDER_CLASSLISTID;
    extern const char *XFOLDER_TRASHCANID;
    extern const char *XFOLDER_XCENTERID;
    extern const char *XFOLDER_STRINGTPLID;

    extern const char *XFOLDER_INTROID;
    extern const char *XFOLDER_USERGUIDE;

    // extern const char *XWORKPLACE_ARCHIVE_MARKER;
        // removed V0.9.13 (2001-06-14) [umoeller]
       */

    /********************************************************************
     *
     *   Thread object windows
     *
     ********************************************************************/

    // ID's of XWorkplace object windows (added V0.9.0)
    #define ID_THREAD1OBJECT        0x1234
    #define ID_WORKEROBJECT         0x1235
    #define ID_QUICKOBJECT          0x1236
    #define ID_FILEOBJECT           0x1237

    // object window class names (added V0.9.0)
    /* extern const char *WNDCLASS_WORKEROBJECT;
    extern const char *WNDCLASS_QUICKOBJECT;
    extern const char *WNDCLASS_FILEOBJECT;

    extern const char *WNDCLASS_THREAD1OBJECT;
    extern const char *WNDCLASS_SUPPLOBJECT;
    extern const char *WNDCLASS_APIOBJECT; */

    /********************************************************************
     *
     *   Various other identifiers/flag declarations
     *
     ********************************************************************/

    // offset by which the controls should be moved
    // when converting buttons to Warp 4 notebook style
    // (using winhAssertWarp4Notebook); this is in
    // "dialog units"
    #define WARP4_NOTEBOOK_OFFSET   14

    // miscellaneae
    #define LANGUAGECODELENGTH      30

    /*
     *  XWorkplace WM_USER message spaces:
     *      Even though the various object windows could use the
     *      same WM_USER messages since they only have to be
     *      unique for each object window, just to be sure, we
     *      make sure each message has a unique value throughout
     *      the system. This avoids problems in case someone sends
     *      a message to the wrong object window.
     *
     *                up to WM_USER+100:    common.h
     *      WM_USER+100 ... WM_USER+149:    folder.h, statbars.h
     *      WM_USER+150 ... WM_USER+179:    Worker thread, xthreads.h
     *      WM_USER+180 ... WM_USER+199:    Quick thread, xthreads.h
     *      WM_USER+200 ... WM_USER+249:    File thread, xthreads.h
     *      WM_USER+250 ... WM_USER+269:    media thread, media.h
     *      WM_USER+270 ... WM_USER+299:    thread-1 obj wnd, kernel.h
     *      WM_USER+300 ... WM_USER+499:    hook, Daemon and PageMage
     */

    // common dlg msgs for settings notebook dlg funcs
    #define XM_SETTINGS2DLG         (WM_USER+90)    // set controls
    #define XM_DLG2SETTINGS         (WM_USER+91)    // read controls
    #define XM_ENABLEITEMS          (WM_USER+92)    // enable/disable controls

    // misc
    #define XM_UPDATE               (WM_USER+93) // in dlgs
    // #define XM_SETLONGTEXT          (WM_USER+94) // for cmnMessageBox
            // removed V0.9.13 (2001-06-23) [umoeller]
    #define XM_CRASH                (WM_USER+95) // test exception handlers

    // fill container; used with class list dialogs
    #define WM_FILLCNR              (WM_USER+96)
                // value changed; moved this here from classes.h
                // (thanks Martin Lafaix)
                // V0.9.6 (2000-11-07) [umoeller]

    // notebook.c messages: moved here V0.9.6 (2000-11-07) [umoeller]
    #define XNTBM_UPDATE            (WM_USER+97)  // update

    // common value for indicating that a Global Setting
    // is to be used instead of an instance's one
    #define SET_DEFAULT             255

    // flags for xfSet/QueryStatusBarVisibility
    #define STATUSBAR_ON            1
    #define STATUSBAR_OFF           0
    #define STATUSBAR_DEFAULT       255

    // new XWorkplace system sounds indices
    // (in addition to those def'd by helpers\syssound.h)
    #define MMSOUND_XFLD_SHUTDOWN   555     // shutdown
    #define MMSOUND_XFLD_RESTARTWPS 556     // restart WPS
    #define MMSOUND_XFLD_CTXTOPEN   558     // context (sub)menu opened
    #define MMSOUND_XFLD_CTXTSELECT 559     // menu item selected
    #define MMSOUND_XFLD_CNRDBLCLK  560     // folder container double-click
    #define MMSOUND_XFLD_HOTKEYPRSD 561     // XWP global hotkey pressed
                                            // added V0.9.3 (2000-04-20) [umoeller]

    // default style used for XWorkplace tooltip controls
    #ifdef COMCTL_HEADER_INCLUDED
        #define XWP_TOOLTIP_STYLE (TTS_SHADOW /* | TTS_ROUNDED */ | TTF_SHYMOUSE | TTS_ALWAYSTIP)
    #endif

    /********************************************************************
     *
     *   Notebook settings page IDs (notebook.c)
     *
     ********************************************************************/

    // XWorkplace settings page IDs; these are used by
    // the following:
    // --  the notebook.c functions to identify open
    //     pages;
    // --  the settings functions in common.c to
    //     identify the default settings to be set.

    // If you add a settings page using notebook.c, define a new
    // ID here. Use any ULONG you like.

    // Groups of settings pages:
    // 1) in "Workplace Shell"
    #define SP_1GENERIC             1
    #define SP_2REMOVEITEMS         2
    #define SP_25ADDITEMS           3
    #define SP_26CONFIGITEMS        4
    #define SP_27STATUSBAR          5
    #define SP_3SNAPTOGRID          6
    #define SP_4ACCELERATORS        7
    // #define SP_5INTERNALS           8    // removed (V0.9.0)
    // #define SP_DTP2                 10   // removed (V0.9.0)
    #define SP_28STATUSBAR2         11
    // #define SP_FILEOPS              12   // removed (V0.9.0)
    #define SP_FILETYPES            13      // new with V0.9.0 (XFldWPS)

    // 2) in "OS/2 Kernel"
    #define SP_SCHEDULER            20
    #define SP_MEMORY               21
    // #define SP_HPFS                 22   this is dead! we now have a settings dlg
    #define SP_FAT                  23
    #define SP_ERRORS               24
    #define SP_WPS                  25
    #define SP_SYSPATHS             26      // new with V0.9.0
    #define SP_DRIVERS              27      // new with V0.9.0
    #define SP_SYSLEVEL             28      // new with V0.9.2 (2000-03-08) [umoeller]

    // 3) in "XWorkplace Setup"
    #define SP_SETUP_INFO           30      // new with V0.9.0
    #define SP_SETUP_FEATURES       31      // new with V0.9.0
    #define SP_SETUP_PARANOIA       32      // new with V0.9.0
    #define SP_SETUP_OBJECTS        33      // new with V0.9.0
    #define SP_SETUP_XWPLOGO        34      // new with V0.9.6 (2000-11-04) [umoeller]
    #define SP_SETUP_THREADS        35      // new with V0.9.9 (2001-03-07) [umoeller]

    // 4) "Sort" pages both in folder notebooks and
    //    "Workplace Shell"
    #define SP_FLDRSORT_FLDR        40
    #define SP_FLDRSORT_GLOBAL      41

    // 5) "XFolder" page in folder notebooks
    #define SP_XFOLDER_FLDR         45      // fixed V0.9.1 (99-12-06)

    // 6) "Startup" page in XFldStartup notebook
    #define SP_STARTUPFOLDER        50      // new with V0.9.0

    // 7) "File" page in XFldDataFile/XFolder
    #define SP_FILE1                60      // new with V0.9.0
    #define SP_FILE2                61      // new with V0.9.1 (2000-01-22) [umoeller]
    #define SP_DATAFILE_TYPES       62      // XFldDataFile "Types" page V0.9.9 (2001-03-27) [umoeller]

    // 8) "Sounds" page in XWPSound
    #define SP_SOUNDS               70

    // 9) pages in XFldDesktop
    #define SP_DTP_MENUITEMS        80      // new with V0.9.0
    #define SP_DTP_STARTUP          81      // new with V0.9.0
    #define SP_DTP_SHUTDOWN         82      // new with V0.9.0
    #define SP_DTP_ARCHIVES         83      // new with V0.9.0

    // 10) pages for XWPTrashCan
    #define SP_TRASHCAN_SETTINGS    90      // new with V0.9.0; renamed V0.9.1 (99-12-12)
    #define SP_TRASHCAN_DRIVES      91      // new with V0.9.1 (99-12-12)
    #define SP_TRASHCAN_ICON        92      // new with V0.9.4 (2000-08-03) [umoeller]

    // 11) "Details" pages
    #define SP_DISK_DETAILS         100     // new with V0.9.0
    #define SP_PROG_DETAILS         101     // new with V0.9.0
    #define SP_PROG_RESOURCES       102     // new with V0.9.7 (2000-12-17) [lafaix]
    #define SP_PROG_DETAILS1        103
    #define SP_PROG_DETAILS2        104

    // 12) XWPClassList
    #define SP_CLASSLIST            110     // new with V0.9.0

    // 13) XWPKeyboard
    #define SP_KEYB_OBJHOTKEYS      120     // new with V0.9.0
    #define SP_KEYB_FUNCTIONKEYS    121     // new with V0.9.3 (2000-04-18) [umoeller]

    // 13) XWPMouse
    #define SP_MOUSE_MOVEMENT       130     // new with V0.9.2 (2000-02-26) [umoeller]
    #define SP_MOUSE_CORNERS        131     // new with V0.9.2 (2000-02-26) [umoeller]
    #define SP_MOUSE_MAPPINGS2      132     // new with V0.9.1
    #define SP_MOUSE_MOVEMENT2      133     // new with V0.9.14 (2001-08-02) [lafaix]

    // 14) XWPScreen
    #define SP_PAGEMAGE_MAIN        140     // new with V0.9.3 (2000-04-09) [umoeller]
    #define SP_PAGEMAGE_WINDOW      141     // new with V0.9.9 (2001-03-27) [umoeller]
    #define SP_PAGEMAGE_STICKY      142     // new with V0.9.3 (2000-04-09) [umoeller]
    #define SP_PAGEMAGE_COLORS      143     // new with V0.9.3 (2000-04-09) [umoeller]

    // 15) XWPString
    #define SP_XWPSTRING            150     // new with V0.9.3 (2000-04-27) [umoeller]

    // 16) XWPMedia
    #define SP_MEDIA_DEVICES        160     // new with V0.9.3 (2000-04-29) [umoeller]
    #define SP_MEDIA_CODECS         161     // new with V0.9.3 (2000-04-29) [umoeller]
    #define SP_MEDIA_IOPROCS        162     // new with V0.9.3 (2000-04-29) [umoeller]

    // 17) XCenter
    #define SP_XCENTER_VIEW1        170     // new with V0.9.7 (2000-12-05) [umoeller]
    #define SP_XCENTER_VIEW2        171     // new with V0.9.7 (2001-01-18) [umoeller]
    #define SP_XCENTER_WIDGETS      172     // new with V0.9.7 (2000-12-05) [umoeller]
    #define SP_XCENTER_CLASSES      173     // new with V0.9.9 (2001-03-09) [umoeller]

    // 18) WPProgram/WPProgramFile
    #define SP_PGM_ASSOCS           180     // new with V0.9.9 (2001-03-07) [umoeller]
    #define SP_PGMFILE_ASSOCS       181     // new with V0.9.9 (2001-03-07) [umoeller]

    // 19) XWPFontFolder
    #define SP_FONT_SAMPLETEXT      190     // new with V0.9.9 (2001-03-27) [umoeller]

    // 20) XWPAdmin
    #define SP_ADMIN_USER           200     // new with V0.9.11 (2001-04-22) [umoeller]

    /********************************************************************
     *
     *   Global structures
     *
     ********************************************************************/

    // shutdown settings bits: composed by the
    // Shutdown settings pages, stored in
    // pGlobalSettings->XShutdown,
    // passed to xfInitiateShutdown

    /* the following removed with V0.9.0;
       there are new flags in GLOBALSETTINGS for this
    #define XSD_DTM_SYSTEMSETUP     0x00001
    #define XSD_DTM_SHUTDOWN        0x00002
    #define XSD_DTM_LOCKUP          0x00004 */

    // #define XSD_ENABLED             0x00010
    #define XSD_CONFIRM             0x000020
    #define XSD_REBOOT              0x000040
    // #define XSD_RESTARTWPS          0x00100
    #define XSD_DEBUG               0x001000
    #define XSD_AUTOCLOSEVIO        0x002000
    #define XSD_WPS_CLOSEWINDOWS    0x004000
    #define XSD_LOG                 0x008000
    #define XSD_ANIMATE_SHUTDOWN    0x010000     // renamed V0.9.3 (2000-05-22) [umoeller]
    #define XSD_APMPOWEROFF         0x020000
    #define XSD_APM_DELAY           0x040000     // added V0.9.2 (2000-03-04) [umoeller]
    #define XSD_ANIMATE_REBOOT      0x080000     // added V0.9.3 (2000-05-22) [umoeller]
    #define XSD_EMPTY_TRASH         0x100000     // added V0.9.4 (2000-08-03) [umoeller]
    #define XSD_WARPCENTERFIRST     0x200000     // added V0.9.7 (2000-12-08) [umoeller]

    // flags for GLOBALSETTINGS.ulIntroHelpShown
    #define HLPS_CLASSLIST          0x00000001

    // flags for GLOBALSETTINGS.ulConfirmEmpty
    #define TRSHCONF_EMPTYTRASH     0x00000001
    #define TRSHCONF_DESTROYOBJ     0x00000002

    // flags for GLOBALSETTINGS.bDereferenceShadows
    #define STBF_DEREFSHADOWS_SINGLE        0x01
    #define STBF_DEREFSHADOWS_MULTIPLE      0x02

    #pragma pack(4)     // just to make sure;
                        // the following is stored as binary in OS2.INI

    /*
     *@@ GLOBALSETTINGS:
     *      this is the massive global settings structure
     *      used by all parts of XWorkplace. This holds
     *      all kinds of configuration data.
     *
     *      This structure is stored in OS2.INI, in the
     *      key specified by the INIAPP_XWORKPLACE / INIKEY_GLOBALSETTINGS
     *      strings above. This is loaded once at WPS
     *      startup (in M_XFldObject::wpclsInitData)
     *      and remains in a global variable in common.c
     *      while the WPS is running.
     *
     *      Only use cmnQueryGlobalSettings to get a pointer
     *      to that global variable.
     *
     *      The items in this structure are modified by
     *      the various XWorkplace settings pages. Whenever
     *      a settings page changes anything, it should
     *      write the settings back to OS2.INI by calling
     *      cmnStoreGlobalSettings.
     *
     *      Usage notes:
     *
     *      a) Do not change this structure definition. This has
     *         existed since XFolder 0.20 or something, and
     *         has always been extended to the bottom only.
     *         Since this gets loaded in one flush from OS2.INI,
     *         users can preserve their settings when upgrading
     *         from an old XFolder version. If anything is removed
     *         from this structure, this would break compatibility
     *         and probably lead to really strange behavior, since
     *         the settings could then mean anything.
     *
     *         If you need to store your own global settings,
     *         use some other INI key, and maintain your settings
     *         yourself. This structure has only been moved
     *         to the shared\ directories so you can _read_
     *         certain settings if you need them.
     *
     *      b) Never load this structure from the INIs yourself.
     *         Always use cmnQueryGlobalSettings to get a "const"
     *         pointer to the up-to-date version, because
     *         this thing is saved to the INIs asynchronously
     *         by the File thread (xthreads.c).
     */

    typedef struct _GLOBALSETTINGS
    {
        ULONG       fReplaceIcons,     // V0.9.0, was: ReplIcons,
                    MenuCascadeMode,
                    FullPath,
                        // enable "full path in title"
                    KeepTitle,
                        // "full path in title": keep existing title
                    RemoveX,
                    AppdParam,
                    MoveRefreshNow;
                        // move "Refresh now" to main context menu

        ULONG       MaxPathChars,
                        // maximum no. of chars for "full path in title"
                    DefaultMenuItems;
                        // ready-made CTXT_* flags for wpFilterPopupMenu

        LONG        VarMenuOffset;
                        // variable menu offset, "Paranoia" page

        ULONG       fAddSnapToGridDefault;
                        // V0.9.0, was: AddSnapToGridItem
                        // default setting for adding "Snap to grid";
                        // can be overridden in XFolder instance settings

        // "snap to grid" values
        LONG        GridX,
                    GridY,
                    GridCX,
                    GridCY;

        ULONG       fFolderHotkeysDefault,
                        // V0.9.0, was: FolderHotkeysDefault
                        // default setting for enabling folder hotkeys;
                        // can be overridden in XFolder instance settings
                    TemplatesOpenSettings;
                        // open settings after creating from template;
                        // 0: do nothing after creation
                        // 1: open settings notebook
                        // 2: make title editable
    /* XFolder 0.52 */
        ULONG       RemoveLockInPlaceItem,
                        // XFldObject, Warp 4 only
                    RemoveFormatDiskItem,
                        // XFldDisk
                    RemoveCheckDiskItem,
                        // XFldDisk
                    RemoveViewMenu,
                        // XFolder, Warp 4 only
                    RemovePasteItem,
                        // XFldObject, Warp 4 only

                    ulRemoved1,             // was: DebugMode,
                    AddCopyFilenameItem;
                        // default setting for "Copy filename" (XFldDataFile)
                        // can be overridden in XFolder instance settings
        ULONG       ulXShutdownFlags;
                        // XSD_* shutdown settings
        ULONG       NoWorkerThread,
                        // "Paranoia" page
                    fNoSubclassing,
                        // "Paranoia" page
                    TreeViewAutoScroll,
                        // XFolder
                    ShowStartupProgress;
                        // XFldStartup
        ULONG       ulStartupObjectDelay;
                        // was: ulStartupDelay;
                        // there's a new ulStartupInitialDelay with V0.9.4 (bottom)
                        // XFldStartup

    /* XFolder 0.70 */
        ULONG       AddFolderContentItem,
                        // general "Folder content" submenu; this does
                        // not affect favorite folders, which are set
                        // for each folder individually
                    FCShowIcons,
                        // show icons in folder content menus (both
                        // "folder content" and favorite folders)
                    fDefaultStatusBarVisibility,
                        // V0.9.0, was: StatusBar;
                        // default visibility of status bars (XFldWPS),
                        // can be overridden in XFolder instance settings
                        // (unlike fEnableStatusBars below, XWPSetup)
                    SBStyle;
                        // status bar style
        LONG        lSBBgndColor,
                    lSBTextColor;
                        // status bar colors; can be changed via drag'n'drop
        ULONG       TemplatesReposition;
                        // reposition new objects after creating from template
        USHORT      usLastRebootExt;
                        // XShutdown: last extended reboot item
        ULONG       AddSelectSomeItem,
                        // XFolder: enable "Select by name"
                    ExtFolderSort,
                        // V0.9.0, was: ReplaceSort;
                        // enable XFolder extended sorting (XWPSetup)
                    AlwaysSort,
                        // default "always sort" flag (BOOL)
                    _removed1, // DefaultSort,
                        // default sort criterion
                        // moved this down V0.9.12 (2001-05-18) [umoeller]
                    __disabled1, // CleanupINIs,
                        // disabled for now V0.9.12 (2001-05-15) [umoeller]

    /* XFolder 0.80  */
                    ShowBootupStatus,
                    ulRemoved3;
                        // V0.9.0, was: WpsShowClassInfo;
        ULONG       SBForViews,
                        // XFldWPS: SBV_xxx flags
                    fReplFileExists,
                        // V0.9.0, was: ReplConfirms;
                        // XFldWPS, replace "File Exists" dialog
                    BootLogo,
                        // V0.9.0, was: ShowXFolderAnim
                        // XFldDesktop "Startup" page: show boot logo
                    FileAttribs,
                        // XFldDataFile: show "File attributes" submenu
                    AddObjectPage,
                        // V0.9.0, was: ShowInternals
                        // XFldObject: add "Object" page to all settings notebooks
                    ExtendFldrViewMenu;
                        // XFolder: extend Warp 4 "View" submenu

    /* XWorkplace 0.9.0 */
        BOOL        fNoExcptBeeps,
                        // XWPSetup "Paranoia": disable exception beeps
                    fUse8HelvFont,
                        // XWPSetup "Paranoia": use "8.Helv" font for dialogs;
                        // on Warp 3, this is enabled per default
                    fReplaceFilePage,
                        // XFolder/XFldDataFile: replace three "File" pages
                        // into one
                    fExtAssocs,
                        // XFldDataFile/XFldWPS: extended associations

                    // Desktop menu items
                    fDTMSort,
                    fDTMArrange,
                    fDTMSystemSetup,
                    fDTMLockup,
                    fDTMShutdown,
                    fDTMShutdownMenu,

                    _ulRemoved4, // fIgnoreFilters,
                        // XFldDataFile/XFldWPS: extended associations
                    fMonitorCDRoms,
                    fRestartWPS,
                        // XWPSetup: enable "Restart WPS"
                    fXShutdown,
                        // XWPSetup: enable XShutdown

                    fEnableStatusBars,
                        // XWPSetup: whether to enable the status bars at all;
                        // unlike fDefaultStatusBarVisibility above
                    fEnableSnap2Grid,
                        // XWPSetup: whether to enable "Snap to grid" at all;
                        // unlike fAddSnapToGridDefault above
                    fEnableFolderHotkeys;
                        // XWPSetup: whether to enable folder hotkeys at all;
                        // unlike fFolderHotkeysDefault above

        BYTE        bDefaultWorkerThreadPriority,
                        // XWPSetup "Paranoia": default priority of Worker thread:
                        //      0: idle +/-0
                        //      1: idle +31
                        //      2: regular +/-0

                    fXSystemSounds,
                        // XWPSetup: enable extended system sounds
                    fWorkerPriorityBeep,
                        // XWPSetup "Paranoia": beep on priority change

                    bBootLogoStyle,
                        // XFldDesktop "Startup" page:
                        // boot logo style:
                        //      0 = transparent
                        //      1 = blow-up

                    bDereferenceShadows,
                        // XFldWPS "Status bars" page 2:
                        // deference shadows flag
                        // changed V0.9.5 (2000-10-07) [umoeller]: now bit flags...
                        // -- STBF_DEREFSHADOWS_SINGLE        0x01
                        // -- STBF_DEREFSHADOWS_MULTIPLE      0x02

        // trashcan settings
                    fTrashDelete,
                    __fRemoved1, // fTrashEmptyStartup,
                    __fRemoved2; // fTrashEmptyShutdown;
        ULONG       ulTrashConfirmEmpty;
                        // TRSHEMPTY_* flags

        BYTE        fReplDriveNotReady;
                        // XWPSetup: replace "Drive not ready" dialog

        ULONG       ulIntroHelpShown;
                        // HLPS_* flags for various classes, whether
                        // an introductory help page has been shown
                        // the first time it's been opened

        BYTE        fEnableXWPHook;
                        // XWPSetup: enable hook (enables object hotkeys,
                        // mouse movement etc.)

        BYTE        fReplaceArchiving;
                        // XWPSetup: enable WPS archiving replacement

        BYTE        fAniMouse;
                        // XWPSetup: enable "animated mouse pointers" page in XWPMouse

        BYTE        fNumLockStartup;
                        // XFldDesktop "Startup": set NumLock to ON on WPS startup

        BYTE        fEnablePageMage;
                        // XWPSetup "PageMage virtual desktops"; this will cause
                        // XDM_STARTSTOPPAGEMAGE to be sent to the daemon

        BYTE        fShowHotkeysInMenus;
                        // on XFldWPS "Hotkeys" page

    /* XWorkplace 0.9.3 */

        BYTE        fNoFreakyMenus;
                        // on XWPSetup "Paranoia" page

        BYTE        fReplaceTrueDelete;
                        // replace "true delete" also?
                        // on XWPSetup "Features" page

    /* XWorkplace 0.9.4 */
        BYTE        fFdrDefaultDoc,
                        // folder default documents enabled?
                        // "Workplace Shell" "View" page
                    fFdrDefaultDocView;
                        // "default doc = folder default view"
                        // "Workplace Shell" "View" page

        BYTE        fResizeSettingsPages;
                        // XWPSetup: allow resizing of WPS notebook pages?

        ULONG       ulStartupInitialDelay;
                        // XFldStartup: initial delay

    /* XWorkplace 0.9.5 */

#ifdef __REPLHANDLES__
        BYTE        fReplaceHandles;
                        // XWPSetup: replace handles management?
#else
        BYTE        fDisabled2;
#endif
        BYTE        bSaveINIS;
                        // XShutdown: save-INIs method:
                        // -- 0: new method (xprf* APIs)
                        // -- 1: old method (Prf* APIs)
                        // -- 2: do not save

    /* XWorkplace 0.9.7 */
        BYTE        fFixLockInPlace;
                        // "Workplace Shell" menus p3: submenu, checkmark
        BYTE        fDTMLogoffNetwork;
                        // "Logoff network now" desktop menu item (XFldDesktop)

    /* XWorkplace 0.9.9 */
        BYTE        fFdrAutoRefreshDisabled;
                        // "Folder auto-refresh" on "Workplace Shell" "View" page;
                        // this only has an effect if folder auto-refresh has
                        // been replaced in XWPSetup in the first place
    /* XWOrkplace V0.9.12 */
        BYTE        bDefaultFolderView;
                        // "default folder view" on XFldWPS "View" page:
                        // -- 0: inherit from parent (default, standard WPS)
                        // -- OPEN_CONTENTS (1): icon view
                        // -- OPEN_TREE (101): tree view
                        // -- OPEN_DETAILS (102): details view

        BYTE        fFoldersFirst;
                        // global sort setting for "folders first"
                        // (TRUE or FALSE)
        LONG        lDefSortCrit;
                        // new global sort criterion (moved this down here
                        // because the value is incompatible with the earlier
                        // setting above, which has been disabled);
                        // this is a LONG because it can have negative values
                        // (see XFolder::xwpSetFldrSort)

        BYTE        fFixClassTitles;
                        // XWPSetup: override wpclsQueryTitle?

        BYTE        fExtendCloseMenu;
                        // XFldWPS "View" page

    } GLOBALSETTINGS;

    typedef const GLOBALSETTINGS* PCGLOBALSETTINGS;

    #pragma pack()

    #ifdef SOM_WPObject_h

        /*
         *@@ ORDEREDLISTITEM:
         *      linked list structure for the ordered list
         *      of objects in a folder
         *      (XFolder::xwpBeginEnumContent).
         */

        typedef struct _ORDEREDLISTITEM
        {
            WPObject                *pObj;
            CHAR                    szIdentity[CCHMAXPATH];
        } ORDEREDLISTITEM, *PORDEREDLISTITEM;

        /*
         * PROCESSCONTENTINFO:
         *      structure needed for processing the ordered content
         *      of a folder (Startup, Shutdown folders).
         *
         *changed V0.9.0 [umoeller]: updated for new XFolder::xwpBeginEnumContent
         *  removed V0.9.12 (2001-04-29) [umoeller]
         */

        /* typedef struct _PROCESSCONTENTINFO
        {
            ULONG          henum;     // xwpBeginEnumContent handle
            WPObject       *pObject;  // current object in this folder
            HWND           hwndView;  // hwnd of this object, if opened succfly.
            ULONG          ulObjectNow,
                           ulObjectMax;
            ULONG          ulTiming;
            ULONG          ulFirstTime;
            PFNWP          pfnwpCallback;
            ULONG          ulCallbackParam;
            BOOL           fStartAll;
            WPFolder        *pFolder;
            BOOL           fCancelled;
            ULONG          tid;         // tid of poc threaed V0.9.12 (2001-04-29) [umoeller]
        } PROCESSCONTENTINFO, *PPROCESSCONTENTINFO;
        */
    #endif

    #ifdef SOM_WPFolder_h
        /*
         *@@ CONTENTMENULISTITEM:
         *      additional linked list item for
         *      "folder content" menus
         */

        typedef struct _CONTENTMENULISTITEM
        {
            WPFolder                    *pFolder;
            SHORT                       sMenuId;
        } CONTENTMENULISTITEM, *PCONTENTMENULISTITEM;
    #endif

    /* ******************************************************************
     *
     *   Main module handling (XFLDR.DLL)
     *
     ********************************************************************/

    HMODULE XWPENTRY cmnQueryMainCodeModuleHandle(VOID);

    #define cmnQueryMainModuleHandle #error Func prototype has changed.

    const char* XWPENTRY cmnQueryMainModuleFilename(VOID);

    HMODULE XWPENTRY cmnQueryMainResModuleHandle(VOID);
    typedef HMODULE XWPENTRY CMNQUERYMAINRESMODULEHANDLE(VOID);
    typedef CMNQUERYMAINRESMODULEHANDLE *PCMNQUERYMAINRESMODULEHANDLE;

    /* ******************************************************************
     *
     *   Error logging
     *
     ********************************************************************/

    VOID XWPENTRY cmnLog(const char *pcszSourceFile,
                         ULONG ulLine,
                         const char *pcszFunction,
                         const char *pcszFormat,
                         ...);

    /* ******************************************************************
     *
     *   XWorkplace National Language Support (NLS)
     *
     ********************************************************************/

    BOOL XWPENTRY cmnQueryXWPBasePath(PSZ pszPath);

    const char* XWPENTRY cmnQueryLanguageCode(VOID);

    BOOL XWPENTRY cmnSetLanguageCode(PSZ pszLanguage);

    const char* XWPENTRY cmnQueryHelpLibrary(VOID);
    typedef const char* XWPENTRY CMNQUERYHELPLIBRARY(VOID);
    typedef CMNQUERYHELPLIBRARY *PCMNQUERYHELPLIBRARY;

    #ifdef SOM_WPObject_h
        BOOL XWPENTRY cmnDisplayHelp(WPObject *somSelf,
                                     ULONG ulPanelID);
    #endif

    const char* XWPENTRY cmnQueryMessageFile(VOID);

    HMODULE XWPENTRY cmnQueryIconsDLL(VOID);

    PSZ XWPENTRY cmnQueryBootLogoFile(VOID);

    HMODULE XWPENTRY cmnQueryNLSModuleHandle(BOOL fEnforceReload);
    typedef HMODULE XWPENTRY CMNQUERYNLSMODULEHANDLE(BOOL fEnforceReload);
    typedef CMNQUERYNLSMODULEHANDLE *PCMNQUERYNLSMODULEHANDLE;

    // PNLSSTRINGS cmnQueryNLSStrings(VOID);        removed V0.9.9 (2001-04-04) [umoeller]

    #ifdef PRFH_HEADER_INCLUDED
        PCOUNTRYSETTINGS XWPENTRY cmnQueryCountrySettings(BOOL fReload);
    #endif

    CHAR XWPENTRY cmnQueryThousandsSeparator(VOID);

    BOOL XWPENTRY cmnIsValidHotkey(USHORT usFlags,
                                   USHORT usKeyCode);

    BOOL XWPENTRY cmnDescribeKey(PSZ pszBuf,
                                 USHORT usFlags,
                                 USHORT usKeyCode);

    BOOL XWPENTRY cmnAddProductInfoMenuItem(HWND hwndMenu);

    VOID XWPENTRY cmnAddCloseMenuItem(HWND hwndMenu);

    /* ******************************************************************
     *
     *   NLS strings
     *
     ********************************************************************/

    void XWPENTRY cmnLoadString(HAB habDesktop, HMODULE hmodResource, ULONG ulID, PSZ *ppsz);

    PSZ XWPENTRY cmnGetString(ULONG ulStringID);
    typedef PSZ XWPENTRY CMNGETSTRING(ULONG ulStringID);
    typedef CMNGETSTRING *PCMNGETSTRING;

    /********************************************************************
     *
     *   XFolder Global Settings
     *
     ********************************************************************/

    const char* XWPENTRY cmnQueryStatusBarSetting(USHORT usSetting);

    BOOL XWPENTRY cmnSetStatusBarSetting(USHORT usSetting, PSZ pszSetting);

    ULONG XWPENTRY cmnQueryStatusBarHeight(VOID);

    PCGLOBALSETTINGS XWPENTRY cmnLoadGlobalSettings(BOOL fResetDefaults);

    const GLOBALSETTINGS* XWPENTRY cmnQueryGlobalSettings(VOID);

    GLOBALSETTINGS* XWPENTRY cmnLockGlobalSettings(const char *pcszSourceFile,
                                                   ULONG ulLine,
                                                   const char *pcszFunction);

    VOID XWPENTRY cmnUnlockGlobalSettings(VOID);

    BOOL XWPENTRY cmnStoreGlobalSettings(VOID);

    BOOL XWPENTRY cmnSetDefaultSettings(USHORT usSettingsPage);

    /* ******************************************************************
     *
     *   Object setup sets V0.9.9 (2001-01-29) [umoeller]
     *
     ********************************************************************/

    // settings types for XWPSETUPENTRY.ulType
    #define     STG_LONG        1
    #define     STG_BOOL        2
    #define     STG_BITFLAG     3
    #define     STG_PSZ         4       // V0.9.9 (2001-03-07) [umoeller]
    #define     STG_PSZARRAY    5
    #define     STG_BINARY      6       // V0.9.12 (2001-05-24) [umoeller]

    /*
     *@@ XWPSETUPENTRY:
     *      describes an entry in an object's setup set.
     *
     *      A "setup set" is an array of XWPSETUPENTRY
     *      structures, each of which represents an object
     *      instance variable together with its setup
     *      string, variable type, default value, and
     *      value limits.
     *
     *      A setup set can be quickly
     *
     *      -- initialized to the default values in
     *         wpInitData (see cmnSetupInitData);
     *
     *      -- built a setup string from during xwpQuerySetup
     *         (see cmnSetupBuildString);
     *
     *      -- updated from a setup string during wpSetup
     *         (see cmnSetupScanString);
     *
     *      -- stored during wpSaveState (see cmnSetupSave);
     *
     *      -- restored during wpRestoreState (see cmnSetupRestore).
     *
     *      Setup sets have been introduced because when new
     *      instance variables are added to a WPS class, one
     *      always has to go through the same dull procedure
     *      of adding that instance variable to all these
     *      methods. So there is always the danger that a
     *      variable is not safely initialized, saved, or
     *      restored, or that default values get messed up
     *      somewhere. The cmnSetup* functions are intended
     *      to aid in getting that synchronized.
     *
     *      To use these, set up a "setup set" (an array of
     *      XWPSETUPENTRY structs) for your class as a global
     *      variable with your class implementation and use the
     *      cmnSetup* function calls in your method overrides.
     *
     *      In order to support any type of variable, ulOfsOfData
     *      does not specify the absolute address of the variable,
     *      but the offset in bytes within a structure which is
     *      then passed with the somThis pointer to the cmnSetup*
     *      functions. While this _can_ be a "true" somThis pointer
     *      from a SOM object, it can really be any structure.
     *
     *@@added V0.9.9 (2001-01-29) [umoeller]
     */

    typedef struct _XWPSETUPENTRY
    {
        ULONG       ulType;
                        // describes the type of the variable specified
                        // by ulOfsOfData. One of:

                        // -- STG_LONG: LONG value; in that case,
                        //      ulMin and ulMax apply and the setup
                        //      string gets the long value appended

                        // -- STG_BOOL: BOOL value; in that case,
                        //      the setup string gets either YES or NO

                        // -- STG_BITFLAG: a bitflag value; in that case,
                        //      the data is assumed to be a ULONG and
                        //      ulBitflag applies; the setup string
                        //      gets either YES or NO also for each entry.
                        //      NOTE: For bitfields, always set them up
                        //      as follows:
                        //      1) a STG_LONG entry for the entire bit
                        //         field with the default value and a
                        //         save/restore key, but no setup string;
                        //      2) for each bit flag, a STG_BITFLAG
                        //         entry afterwards with each flag's
                        //         default value and the setup string,
                        //         but NO save/restore key.
                        //      This ensures that on save/restore, the
                        //      bit field is flushed once only and that
                        //      the bit field is initialized on cmnInitData,
                        //      but each flag can be set/cleared individually
                        //      with a setup string.

                        // -- STG_PSZ: a null-terminated string. Note
                        //      that this requires memory management.
                        //      In that case, ulOfsOfData must point to
                        //      a PSZ pointer, and lDefault must also
                        //      be a PSZ to the default value.

                        // -- STG_PSZARRAY: an array of null-terminated
                        //      strings, where the last string is terminated
                        //      with two zero bytes. Note that this requires
                        //      memory management.
                        //      In that case, ulOfsOfData must point to
                        //      a PSZ pointer. The default value will be
                        //      a NULL string.

                        // -- STG_BINARY: a binary structure. There is
                        //      no setup string support, of course, and
                        //      lMax must specify the size of the
                        //      structure.

        // build/scan setup string values:

        const char  *pcszSetupString;
                        // setup string keyword; e.g. "CCVIEW";
                        // if this is NULL, no setup string is supported
                        // for this entry, and it is not scanned/built.

        // data description:

        ULONG       ulOfsOfData;
                        // offset of the data in an object's instance
                        // data (this is added to the somThis pointer);
                        // the size of the data depends on the setting
                        // type (usually a ULONG).
                        // You can use the FIELDOFFSET macro to determine
                        // this value, e.g. FIELDOFFSET(somThis, ulVariable).

        // save/restore values:
        ULONG       ulKey;
                        // key to be used with wpSaveState/wpRestoreState;
                        // if 0, value is not saved/restored.
                        // NOTE: For STG_BITFLAG, set this to 0 always.
                        // Define a preceding STG_LONG for the bitflag
                        // instead.

        // defaults/limits:
        LONG        lDefault;   // default value; a setup string is only
                                // built if the value is different from
                                // this. This is also used for cmnSetupInitData.

        ULONG       ulExtra;
                        // -- with STG_BITFLAG, the mask for the
                        // ULONG data pointed to by ulOfsOfData
                        // -- with STG_BINARY, the size of the structure

        LONG        lMin,       // only with STG_LONG, the min and max
                    lMax;       // values allowed

    } XWPSETUPENTRY, *PXWPSETUPENTRY;

    VOID XWPENTRY cmnSetupInitData(PXWPSETUPENTRY paSettings,
                                   ULONG cSettings,
                                   PVOID somThis);

    #ifdef XSTRING_HEADER_INCLUDED
        VOID XWPENTRY cmnSetupBuildString(PXWPSETUPENTRY paSettings,
                                          ULONG cSettings,
                                          PVOID somThis,
                                          PXSTRING pstr);
    #endif

    #ifdef SOM_WPObject_h
        BOOL XWPENTRY cmnSetupScanString(WPObject *somSelf,
                                         PXWPSETUPENTRY paSettings,
                                         ULONG cSettings,
                                         PVOID somThis,
                                         PSZ pszSetupString,
                                         PULONG pcSuccess);

        BOOL XWPENTRY cmnSetupSave(WPObject *somSelf,
                                   PXWPSETUPENTRY paSettings,
                                   ULONG cSettings,
                                   const char *pcszClassName,
                                   PVOID somThis);

        BOOL XWPENTRY cmnSetupRestore(WPObject *somSelf,
                                      PXWPSETUPENTRY paSettings,
                                      ULONG cSettings,
                                      const char *pcszClassName,
                                      PVOID somThis);
    #endif

    ULONG XWPENTRY cmnSetupSetDefaults(PXWPSETUPENTRY paSettings,
                                       ULONG cSettings,
                                       PULONG paulOffsets,
                                       ULONG cOffsets,
                                       PVOID somThis);

    ULONG XWPENTRY cmnSetupRestoreBackup(PULONG paulOffsets,
                                         ULONG cOffsets,
                                         PVOID somThis,
                                         PVOID pBackup);

    /* ******************************************************************
     *
     *   Trash can setup
     *
     ********************************************************************/

    BOOL XWPENTRY cmnTrashCanReady(VOID);

    BOOL XWPENTRY cmnEnableTrashCan(HWND hwndOwner,
                                    BOOL fEnable);

    #ifdef SOM_WPObject_h
        BOOL XWPENTRY cmnDeleteIntoDefTrashCan(WPObject *pObject);
    #endif

    BOOL XWPENTRY cmnEmptyDefTrashCan(HAB hab,
                                      PULONG pulDeleted,
                                      HWND hwndConfirmOwner);

    /********************************************************************
     *
     *   Miscellaneae
     *
     ********************************************************************/

    #ifdef SOM_WPObject_h
        BOOL XWPENTRY cmnRegisterView(WPObject *somSelf,
                                      PUSEITEM pUseItem,
                                      ULONG ulViewID,
                                      HWND hwndFrame,
                                      const char *pcszViewTitle);
    #endif

    BOOL XWPENTRY cmnPlaySystemSound(USHORT usIndex);

    #ifdef SOM_WPObject_h
        BOOL cmnIsADesktop(WPObject *somSelf);

        WPObject* XWPENTRY cmnQueryActiveDesktop(VOID);

        HWND XWPENTRY cmnQueryActiveDesktopHWND(VOID);
    #endif

    VOID XWPENTRY cmnShowProductInfo(HWND hwndOwner, ULONG ulSound);

    HAPP XWPENTRY cmnRunCommandLine(HWND hwndOwner,
                                    const char *pcszStartupDir);

    const char* XWPENTRY cmnQueryDefaultFont(VOID);
    typedef const char* XWPENTRY CMNQUERYDEFAULTFONT(VOID);
    typedef CMNQUERYDEFAULTFONT *PCMNQUERYDEFAULTFONT;

    VOID XWPENTRY cmnSetControlsFont(HWND hwnd, SHORT usIDMin, SHORT usIDMax);
    typedef VOID XWPENTRY CMNSETCONTROLSFONT(HWND hwnd, SHORT usIDMin, SHORT usIDMax);
    typedef CMNSETCONTROLSFONT *PCMNSETCONTROLSFONT;

    ULONG XWPENTRY cmnMessageBox(HWND hwndOwner,
                                 const char *pcszTitle,
                                 const char *pcszMessage,
                                 ULONG flStyle);

    APIRET XWPENTRY cmnGetMessageExt(PCHAR *pTable,
                                     ULONG ulTable,
                                     PSZ pszBuf,
                                     ULONG cbBuf,
                                     PSZ pszMsgID);

    APIRET XWPENTRY cmnGetMessage(PCHAR *pTable,
                                  ULONG ulTable,
                                  PSZ pszBuf,
                                  ULONG cbBuf,
                                  ULONG ulMsgNumber);

    ULONG XWPENTRY cmnMessageBoxMsg(HWND hwndOwner,
                                    ULONG ulTitle,
                                    ULONG ulMessage,
                                    ULONG flStyle);

    ULONG XWPENTRY cmnMessageBoxMsgExt(HWND hwndOwner,
                                       ULONG ulTitle,
                                       PCHAR *pTable,
                                       ULONG ulTable,
                                       ULONG ulMessage,
                                       ULONG flStyle);

    ULONG XWPENTRY cmnDosErrorMsgBox(HWND hwndOwner,
                                     CHAR cDrive,
                                     PSZ pszTitle,
                                     APIRET arc,
                                     ULONG ulFlags,
                                     BOOL fShowExplanation);

    #define TEBF_REMOVETILDE            0x0001
    #define TEBF_REMOVEELLIPSE          0x0002

    PSZ XWPENTRY cmnTextEntryBox(HWND hwndOwner,
                                 const char *pcszTitle,
                                 const char *pcszDescription,
                                 const char *pcszDefault,
                                 ULONG ulMaxLen,
                                 ULONG fl);

    VOID XWPENTRY cmnSetDlgHelpPanel(ULONG ulHelpPanel);

    MRESULT EXPENTRY cmn_fnwpDlgWithHelp(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);

    BOOL XWPENTRY cmnFileDlg(HWND hwndOwner,
                             PSZ pszFile,
                             ULONG flFlags,
                             HINI hini,
                             const char *pcszApplication,
                             const char *pcszKey);
#endif

