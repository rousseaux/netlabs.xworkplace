
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
 *@@include #include <wpfolder.h>       // only for some features
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
    #define INIAPP_XWORKPLACE       "XWorkplace"

    // INI key used by XFolder and XWorkplace 0.9.0;
    // this is checked for if INIAPP_XWORKPLACE is not
    // found and converted
    #define INIAPP_OLDXFOLDER       "XFolder"

    /*
     * XWorkplace keys:
     *      Add the keys you are using for storing your data here.
     *      Note: If anything has been marked as "removed" here,
     *      do not use that string, because it might still exist
     *      in a user's OS2.INI file.
     */

    // #define INIKEY_DEFAULTTITLE     "DefaultTitle"       removed V0.9.0
    #define INIKEY_GLOBALSETTINGS   "GlobalSettings"
    // #define INIKEY_XFOLDERPATH      "XFolderPath"        removed V0.81 (I think)
    #define INIKEY_ACCELERATORS     "Accelerators"
    #define INIKEY_LANGUAGECODE     "Language"
    #define INIKEY_JUSTINSTALLED    "JustInstalled"
    // #define INIKEY_DONTDOSTARTUP    "DontDoStartup"      removed V0.84 (I think)
    // #define INIKEY_LASTPID          "LastPID"            removed V0.84 (I think)
    #define INIKEY_FAVORITEFOLDERS  "FavoriteFolders"
    #define INIKEY_QUICKOPENFOLDERS "QuickOpenFolders"

    #define INIKEY_WNDPOSSTARTUP    "WndPosStartup"
    #define INIKEY_WNDPOSNAMECLASH  "WndPosNameClash"
    #define INIKEY_NAMECLASHFOCUS   "NameClashLastFocus"

    #define INIKEY_STATUSBARFONT    "SB_Font"
    #define INIKEY_SBTEXTNONESEL    "SB_NoneSelected"
    #define INIKEY_SBTEXT_WPOBJECT  "SB_WPObject"
    #define INIKEY_SBTEXT_WPPROGRAM "SB_WPProgram"
    #define INIKEY_SBTEXT_WPFILESYSTEM "SB_WPDataFile"
    #define INIKEY_SBTEXT_WPURL        "SB_WPUrl"
    #define INIKEY_SBTEXT_WPDISK    "SB_WPDisk"
    #define INIKEY_SBTEXT_WPFOLDER  "SB_WPFolder"
    #define INIKEY_SBTEXTMULTISEL   "SB_MultiSelected"
    #define INIKEY_SB_LASTCLASS     "SB_LastClass"
    #define INIKEY_DLGFONT          "DialogFont"

    #define INIKEY_BOOTMGR          "RebootTo"
    #define INIKEY_AUTOCLOSE        "AutoClose"

    #define DEFAULT_LANGUAGECODE    "001"

    // window position of "WPS Class list" window (V0.9.0)
    #define INIKEY_WNDPOSCLASSINFO  "WndPosClassInfo"

    // last directory used on "Sound" replacement page (V0.9.0)
    #define INIKEY_XWPSOUNDLASTDIR  "XWPSound:LastDir"
    // last sound scheme selected (V0.9.0)
    #define INIKEY_XWPSOUNDSCHEME   "XWPSound:Scheme"

    // boot logo .BMP file (V0.9.0)
    #define INIKEY_BOOTLOGOFILE     "BootLogoFile"

    // last ten selections in "Select some" (V0.9.0)
    #define INIKEY_LAST10SELECTSOME "SelectSome"

    // supported drives in XWPTrashCan (V0.9.1 (99-12-14) [umoeller])
    #define INIKEY_TRASHCANDRIVES   "TrashCan::Drives"

    // window pos of file operations status window V0.9.1 (2000-01-30) [umoeller]
    #define INIKEY_FILEOPSPOS       "WndPosFileOpsStatus"

    // window pos of "Partitions" view V0.9.2 (2000-02-29) [umoeller]
    #define INIKEY_WNDPOSPARTITIONS "WndPosPartitions"

    /*
     * file type hierarchies:
     *
     */

    // application for file type hierarchies
    #define INIAPP_XWPFILETYPES     "XWorkplace:FileTypes"   // added V0.9.0
    #define INIAPP_XWPFILEFILTERS   "XWorkplace:FileFilters" // added V0.9.0

    /*
     * some default WPS INI keys:
     *
     */

    #define WPINIAPP_LOCATION       "PM_Workplace:Location"
    #define WPINIAPP_FOLDERPOS      "PM_Workplace:FolderPos"
    // #define WPINIAPP_TASKLISTPOS    "PM_Workplace:WindowListPos"
    #define WPINIAPP_ASSOCTYPE      "PMWP_ASSOC_TYPE"
    #define WPINIAPP_ASSOCFILTER    "PMWP_ASSOC_FILTER"
    // #define WPINIKEY_TASKLISTPOS    "SavePos"

    /********************************************************************
     *
     *   XWorkplace object IDs
     *
     ********************************************************************/

    // all of these have been redone with V0.9.2

    // folders
    #define XFOLDER_MAINID          "<XWP_MAINFLDR>"
    #define XFOLDER_CONFIGID        "<XWP_CONFIG>"

    #define XFOLDER_STARTUPID       "<XWP_STARTUP>"
    #define XFOLDER_SHUTDOWNID      "<XWP_SHUTDOWN>"

    #define XFOLDER_WPSID           "<XWP_WPS>"
    #define XFOLDER_KERNELID        "<XWP_KERNEL>"
    #define XFOLDER_SCREENID        "<XWP_SCREEN>"
    #define XFOLDER_MEDIAID         "<XWP_MEDIA>"

    #define XFOLDER_CLASSLISTID     "<XWP_CLASSLIST>"
    #define XFOLDER_TRASHCANID      "<XWP_TRASHCAN>"

    #define XFOLDER_INTROID         "<XWP_INTRO>"
    #define XFOLDER_USERGUIDE       "<XWP_REF>"

    #define XWORKPLACE_ARCHIVE_MARKER   "xwparchv.tmp"
                // archive marker file in Desktop directory V0.9.4 (2000-08-03) [umoeller]

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
    #define WNDCLASS_WORKEROBJECT         "XWPWorkerObject"
    #define WNDCLASS_QUICKOBJECT          "XWPQuickObject"
    #define WNDCLASS_FILEOBJECT           "XWPFileObject"

    #define WNDCLASS_THREAD1OBJECT        "XWPThread1Object"
    #define WNDCLASS_SUPPLOBJECT          "XWPSupplFolderObject"

    /********************************************************************
     *
     *   Help panels in XFDLRxxx.HLP
     *
     ********************************************************************/

    // The following are constant (I hope) help panel IDs
    // for the various XWorkplace menu items, settings pages,
    // and dialog items therein.

    // All help panel IDs have been changed with V0.9.3 (2000-05-04) [umoeller]
    // because I've rearranged the help HTML files.

    // help panel IDs for various dlg boxes
    #define ID_XFH_BORED            2
    #define ID_XFH_NOCONFIG         (ID_XFH_BORED+1)
    #define ID_XFH_NOOBJECT         (ID_XFH_BORED+2)
    #define ID_XFH_LIMITREACHED     (ID_XFH_BORED+3)
    #define ID_XFH_NOTEXTCLIP       (ID_XFH_BORED+4)
    #define ID_XFH_REBOOTEXT        (ID_XFH_BORED+5)
    #define ID_XFH_AUTOCLOSEDETAILS (ID_XFH_BORED+6)
    #define ID_XFH_SELECTCLASS      (ID_XFH_BORED+7)
    #define ID_XFH_REGISTERCLASS    (ID_XFH_BORED+8)
    #define ID_XFH_TITLECLASH       (ID_XFH_BORED+9)

    #define ID_XMH_VARIABLE         12
    #define ID_XMH_CONFIGFOLDER     13
    #define ID_XMH_REFRESH          14
    #define ID_XMH_SNAPTOGRID       15
    #define ID_XMH_COPYFILENAME     16
    #define ID_XMH_XSHUTDOWN        17
    #define ID_XMH_RESTARTWPS       18

    #define ID_XSH_SETTINGS1                 19
    #define ID_XSH_SETTINGS_REMOVEMENUS      20
    #define ID_XSH_SETTINGS_ADDMENUS         21
    #define ID_XSH_SETTINGS_PARANOIA         25
    #define ID_XSH_SETTINGS_DTP_MENUITEMS    26
    #define ID_XSH_SETTINGS_DTP_SHUTDOWN     27     // XShutdown
    #define ID_XSH_SETTINGS_FLDRSORT         28
    #define ID_XSH_SETTINGS_FLDR1            29
    #define ID_XSH_SETTINGS_SB2              30
    #define ID_XSH_SETTINGS_WPSCLASSES       31
    #define ID_XSH_SETTINGS_OBJINTERNALS     32
    #define ID_XSH_SETTINGS_KERNEL1          33
    #define ID_XSH_SETTINGS_KERNEL2          34
    #define ID_XMH_STARTUPSHUTDOWN           35
    #define ID_XMH_FOLDERCONTENT             36
    #define ID_XSH_SETTINGS_HPFS             37
    #define ID_XSH_SETTINGS_FAT              37     // same page as HPFS
    #define ID_XSH_SETTINGS_CFGM             38     // "Config folder menu items"
    #define ID_XSH_SETTINGS_FILEOPS          39     // "File operations"
    #define ID_XSH_SETTINGS_WPS              40
    #define ID_XSH_SETTINGS_ERRORS           41
    #define ID_XSH_SETTINGS_SYSPATHS         42     // V0.9.0: "System paths"
    #define ID_XSH_SETTINGS_FILETYPES        43     // V0.9.0: "File Types"
    #define ID_XSH_SETTINGS_TRASHCAN         46     // V0.9.0: XWPTrashCan (two pages)
    #define ID_XSH_SETTINGS_XC_INFO          48     // V0.9.0: XWPSetup "Status" page
    #define ID_XSH_SETTINGS_XC_FEATURES      49     // V0.9.0: XWPSetup "Features" page
    #define ID_XSH_SETTINGS_FILEPAGE1        50     // V0.9.0: file-system objects "File" page replacment
    #define ID_XSH_SETTINGS_FILEPAGE2        51     // V0.9.1 (2000-01-22) [umoeller]
    #define ID_XSH_SETTINGS_DISKDETAILS      52     // V0.9.0: disk "Details" replacement
    #define ID_XSH_SETTINGS_DTP_STARTUP      53     // V0.9.0: disk "Details" replacement
    #define ID_XSH_SETTINGS_DTP_ARCHIVES     54     // V0.9.0: disk "Details" replacement
    #define ID_XSH_SETTINGS_XFLDSTARTUP      55     // V0.9.0: startup folder settings page
    #define ID_XSH_SETTINGS_PGMFILE_MODULE   56     // V0.9.0: progfile "Module" page
    #define ID_XSH_SETTINGS_CLASSLIST        57     // V0.9.0: XWPClassList settings page
    #define ID_XSH_XFLDWPS                   58     // V0.9.0: default help for XFldWPS
    #define ID_XSH_XFLDSYSTEM                59     // V0.9.0: default help for XFldSystem
    #define ID_XSH_XWPSETUP                  60     // V0.9.0: default help for XWPSetup
    #define ID_XSH_SETTINGS_OBJECTS          61     // V0.9.0: XWPSetup "Objects" page
    #define ID_XSH_SETTINGS_DRIVERS          62     // V0.9.0: XFldSystem "Drivers" page
    #define ID_XSH_DRIVER_HPFS               63     // V0.9.0: XFldSystem "Drivers" page
    #define ID_XSH_DRIVER_CDFS               64     // V0.9.0: XFldSystem "Drivers" page
    #define ID_XSH_DRIVER_S506               65     // V0.9.0: XFldSystem "Drivers" page
    #define ID_XSH_KEYB_OBJHOTKEYS           66     // V0.9.0: XWPKeyboard "Object hotkeys" page
    #define ID_XSH_MOUSE_MOVEMENT            67     // V0.9.0: XWPMouse "Movement" page 1
    #define ID_XSH_MOUSEMAPPINGS2            68     // V0.9.1: XWPMouse "Mappings" page 2
    #define ID_XSH_XWPSCREEN                 69     // V0.9.2: default help for XWPScreen
    #define ID_XSH_MOUSE_CORNERS             70     // V0.9.2: XWPMouse "Movement" page 2
    #define ID_XSH_SETTINGS_TRASH_DRIVES     71     // V0.9.2: XWPTrashCan "Drives" page
    #define ID_XSH_SETTINGS_SYSLEVEL         72     // V0.9.3: XFldSystem "Syslevel" page
    #define ID_XSH_SETTINGS_PAGEMAGE_GENERAL 73     // V0.9.3: XWPScreen "PageMage General" page
    #define ID_XSH_SETTINGS_PAGEMAGE_COLORS  74     // V0.9.3: XWPScreen "PageMage Colors" page
                // 75 is for "Mouse actions" descriptions linked to from the above pages
    #define ID_XSH_SETTINGS_FUNCTIONKEYS     76     // V0.9.3: XWPKeyboard "Function keys" page
                // 77 is for the "edit function key" dlg
    #define ID_XSH_SETTINGS_XWPSTRING_MAIN   78     // V0.9.3: XWPString main help
    #define ID_XSH_SETTINGS_XWPSTRING_PAGE   79     // V0.9.3: XWPString settings page
    #define ID_XSH_MEDIA_DEVICES             80     // V0.9.3: XWPMedia "Devices" page
    #define ID_XSH_MEDIA_CODECS              81     // V0.9.3: XWPMedia "Codecs" page
    #define ID_XSH_MEDIA_IOPROCS             82     // V0.9.3: XWPMedia "IOProcs" page
    #define ID_XSH_SETTINGS_TRASHCAN_DRIVES  83     // V0.9.4: XWPTrashCan "Drives" page
    #define ID_XSH_SETTINGS_PAGEMAGE_STICKY  85     // V0.9.3: XWPScreen "PageMage Sticky" page;
                                                    // three pages
    #define ID_XSH_SETTINGS_TRASHCAN_ICON    88     // V0.9.4: XWPTrashCan "Icon" page
    #define ID_XSH_XSHUTDOWN_CONFIRM         89     // V0.9.4: shutdown confirm dlg
    #define ID_XFH_SELECTSOME                90     // V0.9.4: changed this to have it assigned a fixed no. finally
    #define ID_XFH_VIEW_MENU_ITEMS           91     // V0.9.4: added for XFolder "View" submenu items
    #define ID_XSH_DRIVER_HPFS386            92     // V0.9.5: HPFS386 driver dialog help

    #define ID_XSH_XWPMEDIA                  93     // V0.9.5: XWPMedia main panel

    #define ID_XSH_XWP_CLASSESDLG            94     // V0.9.5: XWP "Classes" dlg

    #define ID_XFH_CLOSEVIO                  95     // V0.9.6: was missing

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

    // common dlg msgs for settings notebook dlg funcs
    #define XM_SETTINGS2DLG         (WM_USER+90)    // set controls
    #define XM_DLG2SETTINGS         (WM_USER+91)    // read controls
    #define XM_ENABLEITEMS          (WM_USER+92)    // enable/disable controls

    // misc
    #define XM_UPDATE               (WM_USER+93) // in dlgs
    #define XM_SETLONGTEXT          (WM_USER+94) // for cmnMessageBox
    #define XM_CRASH                (WM_USER+95) // test exception handlers

    // common value for indicating that a Global Setting
    // is to be used instead of an instance's one
    #define SET_DEFAULT             255

    // flags for xfSet/QueryStatusBarVisibility
    #define STATUSBAR_ON            1
    #define STATUSBAR_OFF           0
    #define STATUSBAR_DEFAULT       255

    // flags for XFolder sort items
    #define SV_NAME                 0
    #define SV_TYPE                 1
    #define SV_CLASS                2
    #define SV_REALNAME             3
    #define SV_SIZE                 4
    #define SV_LASTWRITEDATE        5
    #define SV_LASTACCESSDATE       6
    #define SV_CREATIONDATE         7
    #define SV_EXT                  8
    #define SV_FOLDERSFIRST         9
    #define SV_LAST                 9

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
    #define SP_FILETYPES            13      // new with V0.9.0

    // 2) in "OS/2 Kernel"
    #define SP_SCHEDULER            20
    #define SP_MEMORY               21
    #define SP_HPFS                 22
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

    // 12) XWPClassList
    #define SP_CLASSLIST            110     // new with V0.9.0

    // 13) XWPKeyboard
    #define SP_KEYB_OBJHOTKEYS      120     // new with V0.9.0
    #define SP_KEYB_FUNCTIONKEYS    121     // new with V0.9.3 (2000-04-18) [umoeller]

    // 13) XWPMouse
    #define SP_MOUSE_MOVEMENT       130     // new with V0.9.2 (2000-02-26) [umoeller]
    #define SP_MOUSE_CORNERS        131     // new with V0.9.2 (2000-02-26) [umoeller]
    #define SP_MOUSE_MAPPINGS2      132     // new with V0.9.1

    // 14) XWPScreen
    #define SP_PAGEMAGE1            140     // new with V0.9.3 (2000-04-09) [umoeller]
    #define SP_PAGEMAGE2            141     // new with V0.9.3 (2000-04-09) [umoeller]

    // 15) XWPString
    #define SP_XWPSTRING            150     // new with V0.9.3 (2000-04-27) [umoeller]

    // 16) XWPMedia
    #define SP_MEDIA_DEVICES        160     // new with V0.9.3 (2000-04-29) [umoeller]
    #define SP_MEDIA_CODECS         161     // new with V0.9.3 (2000-04-29) [umoeller]
    #define SP_MEDIA_IOPROCS        162     // new with V0.9.3 (2000-04-29) [umoeller]

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
                    DefaultSort,
                        // default sort criterion
                    CleanupINIs,
                        // XWPSetup "File operations" page

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
#ifdef __EXTASSOCS__
                    fExtAssocs,
                        // XFldDataFile/XFldWPS: extended associations
#else
                    _ulDisabled1,
#endif
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

    } GLOBALSETTINGS;

    typedef const GLOBALSETTINGS* PCGLOBALSETTINGS;

    #pragma pack()

    /*
     *@@ NLSSTRINGS:
     *      structure filled with all the XWorkplace NLS
     *      strings. Each item in here corresponds to
     *      an item in the NLS .RC file.
     *
     *      All fields are initialized at once when the
     *      NLS DLL is first loaded (cmnQueryNLSModuleHandle).
     *
     *      Use cmnQueryNLSStrings to get access to this
     *      structure.
     */

    typedef struct _NLSSTRINGS
    {
        PSZ     pszNotDefined,
                pszProductInfo,
                pszRefreshNow,
                pszSnapToGrid,

                pszFldrContent,
                pszCopyFilename,
                pszBored,
                pszFldrEmpty,

                pszSelectSome,
                pszQuickStatus,

                // various key descriptions
                pszCtrl,
                pszAlt,
                pszShift,
                pszBackspace,
                pszTab,
                pszBacktab,
                pszEnter,
                pszEsc,
                pszSpace,
                pszPageup,
                pszPagedown,
                pszEnd,
                pszHome,
                pszLeft,
                pszUp,
                pszRight,
                pszDown,
                pszPrintscrn,
                pszInsert,
                pszDelete,
                pszScrlLock,
                pszNumLock,
                pszWinLeft,
                pszWinRight,
                pszWinMenu,

                // shutdown strings
                pszSDFlushing,
                pszSDCAD,
                pszSDRebooting,
                pszSDRestartingWPS,
                pszSDClosing,
                pszShutdown,
                pszRestartWPS,
                pszSDSavingDesktop,
                pszSDSavingProfiles,

                pszStarting,

                // various notebook page titles
                psz1Generic,
                psz2RemoveItems,
                psz25AddItems,
                psz26ConfigFolderMenus,
                psz27StatusBar,
                psz3SnapToGrid,
                psz4Accelerators,
                psz5Internals,
                pszFileOps,

                pszInternals,

                pszScheduler,
                pszMemory,
                pszErrors,
                pszWPS,

                // sort strings
                pszSortByName,
                pszSortByType,
                pszSortByClass,
                pszSortByRealName,
                pszSortBySize,
                pszSortByWriteDate,
                pszSortByAccessDate,
                pszSortByCreationDate,
                pszSortByExt,
                pszSortFoldersFirst,

                pszSort,
                pszAlwaysSort,

                // status bars
                pszSBClassMnemonics,
                pszSBClassNotSupported,
                pszPopulating,

                // XWPClassList strings
                pszWpsClasses,
                pszWpsClassLoaded,
                pszWpsClassLoadingFailed,
                pszWpsClassReplacedBy,
                pszWpsClassOrphans,
                pszWpsClassOrphansInfo,

                pszProcessContent,

                // context menu strings
                pszSettings,
                pszSettingsNotebook,
                pszAttributes,
                pszAttrArchive,
                pszAttrSystem,
                pszAttrHidden,
                pszAttrReadOnly,

                pszWarp3FldrView,
                pszSmallIcons,
                pszFlowed,
                pszNonFlowed,
                pszNoGrid,
                pszWarp4MenuBar,    // added V0.9.0
                pszShowStatusBar,

                // more settings pages:
                // the following are new with V0.9.0
                pszSysPaths,        // title of "System Paths" page in "OS/2 Kernel"
                pszDrivers,         // title of "Drivers" page in "OS/2 Kernel"
                pszDriverCategories,  // root record in container on that page
                pszXWPStatus,       // title of "XWP Status" page in XWPSetup
                pszFeatures,        // title of "Features" page in XWPSetup
                pszParanoia,        // title of "Paranoia" page in XWPSetup
                pszObjects,         // title of "Objects" page in XWPSetup
                pszFilePage,        // title of WPFileSystem "File" page replacement
                pszDetailsPage,     // title of WPDisk "Details" page replacement
                pszXShutdownPage,   // title of XFldDesktop "XShutdown" page
                pszStartupPage,     // title of XFldDesktop "Startup" page
                pszDtpMenuPage,     // title of XFldDesktop "Menu" page
                pszFileTypesPage,   // title of "File types" page (XFldWPS)
                pszSoundsPage,      // title of "Sounds" page (XWPSound)
                pszViewPage,        // title of "View" page (XFldWPS)
                pszArchivesPage,    // title of WPDesktop "Archives" page replacement
                pszModulePage,      // title of XFldProgramFile "Module" page
                pszObjectHotkeysPage, // title of XWPKeyboard "Hotkeys" page
                pszFunctionKeysPage, // title of XWPKeyboard "Function keys " page V0.9.3 (2000-04-18) [umoeller]
                pszMouseHookPage,   // title of XWPMouse "Mouse hook" page
                pszMappingsPage,    // title of XWPKeyboard/XWPMouse "Mappings" pages (V0.9.1)

                pszOpenClassList,   // "WPS Class List" (XWPClassList, new with V0.9.0)
                pszXWPClassList,    // XWPClassList default title
                pszRegisterClass,   // XWPClassList: "Register new class" V0.9.1 (99-12-28) [umoeller]

                pszSoundSchemeNone, // XWPSound
                pszItemsSelected,   // "x items selected" on System Paths page

                // trash can (XWPTrashCan, XWPTrashObject, new with V0.9.0)
                pszTrashEmpty,
                pszTrashRestore,
                pszTrashDestroy,

                pszTrashCan,
                pszTrashObject,

                pszTrashSettingsPage,
                pszTrashDrivesPage,

                // trash can Details view columns
                pszOrigFolder,
                pszDelDate,
                pszDelTime,
                pszSize,
                pszOrigClass,

                // trash can status bar strings; V0.9.1 (2000-02-04) [umoeller]
                pszStbPopulating,
                pszStbObjCount,

                // Details view columns on XWPKeyboard "Hotkeys" page; V0.9.1 (99-12-03)
                pszHotkeyTitle,
                pszHotkeyFolder,
                pszHotkeyHandle,
                pszHotkeyHotkey,

                // Method info columns for XWPClassList; V0.9.1 (99-12-03)
                pszClsListIndex,
                pszClsListMethod,
                pszClsListAddress,
                pszClsListClass,
                pszClsListOverriddenBy,

                // "Special functions" on XWPMouse "Movement" page
                pszSpecialWindowList,
                pszSpecialDesktopPopup,

                // default title of XWPScreen class V0.9.2 (2000-02-23) [umoeller]
                pszXWPScreenTitle,

                // "Partitions" item in WPDrives "open" menu V0.9.2 (2000-02-29) [umoeller]
                pszOpenPartitions,

                // "Syslevel" page title in "OS/2 kernel"
                pszSyslevelPage,

                pszCalculating,

                pszDeviceType,
                pszDeviceIndex,
                pszDeviceInfo,

                pszTypeImage,
                pszTypeAudio,
                pszTypeMIDI,
                pszTypeCompound,
                pszTypeOther,
                pszTypeUnknown,
                pszTypeVideo,
                pszTypeAnimation,
                pszTypeMovie,

                pszTypeStorage,
                pszTypeFile,
                pszTypeData,

                pszDevTypeVideotape,
                pszDevTypeVideodisc,
                pszDevTypeCDAudio,
                pszDevTypeDAT,
                pszDevTypeAudioTape,
                pszDevTypeOther,
                pszDevTypeWaveformAudio,
                pszDevTypeSequencer,
                pszDevTypeAudioAmpmix,
                pszDevTypeOverlay,
                pszDevTypeAnimation,
                pszDevTypeDigitalVideo,
                pszDevTypeSpeaker,
                pszDevTypeHeadphone,
                pszDevTypeMicrophone,
                pszDevTypeMonitor,
                pszDevTypeCDXA,
                pszDevTypeFilter,
                pszDevTypeTTS,

                pszColmnFourCC,
                pszColmnName,
                pszColmnIOProcType,
                pszColmnMediaType,
                pszColmnExtension,
                pszColmnDLL,
                pszColmnProcedure,

                pszPagetitleDevices,
                pszPagetitleIOProcs,
                pszPagetitleCodecs,

                pszPagetitlePageMage,

                pszXWPStringPage,
                pszXWPStringOpenMenu,

                pszSyslevelComponent,
                pszSyslevelFile,
                pszSyslevelVersion,
                pszSyslevelLevel,
                pszSyslevelPrevious,

                pszDriversVersion,
                pszDriversVendor,

                pszFuncKeyDescription,
                pszFuncKeyScanCode,
                pszFuncKeyModifier,

                // default documents V0.9.4 (2000-06-09) [umoeller]
                pszDataFileDefaultDoc,
                pszFdrDefaultDoc,

                // XCenter V0.9.4 (2000-06-10) [umoeller]
                pszXCenterPage1,

                // file operations V0.9.4 (2000-07-27) [umoeller]
                pszFopsMove2TrashCan,
                pszFopsRestoreFromTrashCan,
                pszFopsTrueDelete,
                pszFopsEmptyingTrashCan,

                pszIconPage,        // added V0.9.4 (2000-08-03) [umoeller]

                // XShutdown INI save strings V0.9.5 (2000-08-16) [umoeller]
                pszXSDSaveInisNew,
                pszXSDSaveInisOld,
                pszXSDSaveInisNone,

                // logoff V0.9.5 (2000-09-28) [umoeller]
                pszXSDLogoff,
                pszXSDConfirmLogoffMsg;
    } NLSSTRINGS;

    typedef const NLSSTRINGS* PNLSSTRINGS;

    #ifdef SOM_WPObject_h

        /*
         *@@ VARMENULISTITEM:
         *     linked list item for variable menu items
         *     inserted into folder context menus; these
         *     list items are created for both config folder
         *     and folder content items
         */

        typedef struct _VARMENULISTITEM
        {
            WPObject                    *pObject;
            CHAR                        szTitle[100];
            USHORT                      usObjType;
        } VARMENULISTITEM, *PVARMENULISTITEM;

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
         *@@ PROCESSCONTENTINFO:
         *      structure needed for processing the ordered content
         *      of a folder (Startup, Shutdown folders).
         *
         *@@changed V0.9.0 [umoeller]: updated for new XFolder::xwpBeginEnumContent
         */

        typedef struct _PROCESSCONTENTINFO
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
            BOOL           fCancelled;
        } PROCESSCONTENTINFO, *PPROCESSCONTENTINFO;
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

    HMODULE cmnQueryMainModuleHandle(VOID);

    const char* cmnQueryMainModuleFilename(VOID);

    /* ******************************************************************
     *
     *   Error logging
     *
     ********************************************************************/

    VOID cmnLog(const char *pcszSourceFile,
                ULONG ulLine,
                const char *pcszFunction,
                const char *pcszFormat,
                ...);

    /* ******************************************************************
     *
     *   XWorkplace National Language Support (NLS)
     *
     ********************************************************************/

    BOOL cmnQueryXFolderBasePath(PSZ pszPath);

    const char* cmnQueryLanguageCode(VOID);

    BOOL cmnSetLanguageCode(PSZ pszLanguage);

    const char* cmnQueryHelpLibrary(VOID);

    #ifdef SOM_WPObject_h
        BOOL cmnDisplayHelp(WPObject *somSelf,
                            ULONG ulPanelID);
    #endif

    const char* cmnQueryMessageFile(VOID);

    HMODULE cmnQueryIconsDLL(VOID);

    PSZ cmnQueryBootLogoFile(VOID);

    void cmnLoadString(HAB habDesktop, HMODULE hmodResource, ULONG ulID, PSZ *ppsz);

    HMODULE cmnQueryNLSModuleHandle(BOOL fEnforceReload);

    PNLSSTRINGS cmnQueryNLSStrings(VOID);

    CHAR cmnQueryThousandsSeparator(VOID);

    BOOL cmnDescribeKey(PSZ pszBuf,
                        USHORT usFlags,
                        USHORT usKeyCode);

    /********************************************************************
     *
     *   XFolder Global Settings
     *
     ********************************************************************/

    const char* cmnQueryStatusBarSetting(USHORT usSetting);

    BOOL cmnSetStatusBarSetting(USHORT usSetting, PSZ pszSetting);

    ULONG cmnQueryStatusBarHeight(VOID);

    PCGLOBALSETTINGS cmnLoadGlobalSettings(BOOL fResetDefaults);

    const GLOBALSETTINGS* cmnQueryGlobalSettings(VOID);

    GLOBALSETTINGS* cmnLockGlobalSettings(ULONG ulTimeout);

    VOID cmnUnlockGlobalSettings(VOID);

    BOOL cmnStoreGlobalSettings(VOID);

    BOOL cmnSetDefaultSettings(USHORT usSettingsPage);

    /* ******************************************************************
     *
     *   Trash can setup
     *
     ********************************************************************/

    BOOL cmnTrashCanReady(VOID);

    BOOL cmnEnableTrashCan(HWND hwndOwner,
                           BOOL fEnable);

    #ifdef SOM_WPObject_h
        BOOL cmnMove2DefTrashCan(WPObject *pObject);
    #endif

    BOOL cmnEmptyDefTrashCan(HAB hab,
                             PULONG pulDeleted,
                             HWND hwndConfirmOwner);

    /********************************************************************
     *
     *   Miscellaneae
     *
     ********************************************************************/

    BOOL cmnPlaySystemSound(USHORT usIndex);

    #ifdef SOM_WPObject_h
        WPObject* cmnQueryActiveDesktop(VOID);

        HWND cmnQueryActiveDesktopHWND(VOID);
    #endif

    VOID cmnShowProductInfo(ULONG ulSound);

    const char* cmnQueryDefaultFont(VOID);

    VOID cmnSetControlsFont(HWND hwnd,
                            SHORT usIDMin,
                            SHORT usIDMax);

    /*
    #define MB_OK                      0x0000
    #define MB_OKCANCEL                0x0001
    #define MB_RETRYCANCEL             0x0002
    #define MB_ABORTRETRYIGNORE        0x0003
    #define MB_YESNO                   0x0004
    #define MB_YESNOCANCEL             0x0005
    #define MB_CANCEL                  0x0006
    #define MB_ENTER                   0x0007
    #define MB_ENTERCANCEL             0x0008 */
    #define MB_YES_YES2ALL_NO          0x0009

    /*
    #define MBID_OK                    1
    #define MBID_CANCEL                2
    #define MBID_ABORT                 3
    #define MBID_RETRY                 4
    #define MBID_IGNORE                5
    #define MBID_YES                   6
    #define MBID_NO                    7
    #define MBID_HELP                  8
    #define MBID_ENTER                 9 */
    #define MBID_YES2ALL               10

    ULONG cmnMessageBox(HWND hwndOwner,
                        PSZ pszTitle,
                        PSZ pszMessage,
                        ULONG flStyle);

    APIRET cmnGetMessageExt(PCHAR *pTable,
                            ULONG ulTable,
                            PSZ pszBuf,
                            ULONG cbBuf,
                            PSZ pszMsgID);

    APIRET cmnGetMessage(PCHAR *pTable,
                         ULONG ulTable,
                         PSZ pszBuf,
                         ULONG cbBuf,
                         ULONG ulMsgNumber);

    ULONG cmnMessageBoxMsg(HWND hwndOwner,
                           ULONG ulTitle,
                           ULONG ulMessage,
                           ULONG flStyle);

    ULONG cmnMessageBoxMsgExt(HWND hwndOwner,
                              ULONG ulTitle,
                              PCHAR *pTable,
                              ULONG ulTable,
                              ULONG ulMessage,
                              ULONG flStyle);

    ULONG cmnDosErrorMsgBox(HWND hwndOwner,
                            CHAR cDrive,
                            PSZ pszTitle,
                            APIRET arc,
                            ULONG ulFlags,
                            BOOL fShowExplanation);

    VOID cmnSetDlgHelpPanel(ULONG ulHelpPanel);

    MRESULT EXPENTRY cmn_fnwpDlgWithHelp(HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2);

#endif

