
/*
 *@@sourcefile helppanels.h:
 *      shared include file for XWorkplace help panel IDs.
 *
 *      This is now also used by h2i.exe to finally get
 *      rid of the stupid hand calculations for getting
 *      the help panels right. However, this must be
 *      in a separate file because h2i isn't good at
 *      fully parsing C syntax. So the help panel IDs
 *      have been extracted from common.h into this file.
 *
 *      To reference (show) a help panel from the XWorkplace
 *      sources, do the following:
 *
 *      --  For each new help panel, add a unique resid
 *          definition below. The only requirement is that
 *          this be < 10000 because h2i.exe will assign
 *          automatic resids with values 10000 and higher.
 *
 *      --  Write a new HTML file in 001\xwphelp.
 *
 *      --  Add a link to the new help file from
 *          001\xwphelp\xfldr001.html. Otherwise your file
 *          won't be translated by h2i.exe.
 *
 *      --  To the top <HTML> tag in your new help file,
 *          add a "RESID=&resid;" attribute to explicitly
 *          assign the resid for that file (with resid
 *          being the identifier used in the #define below).
 *
 *          Example:
 *
 +              <HTML RESID=&ID_XFH_BORED;>
 *
 *      --  In the C sources, call cmnDisplayHelp(ID_XFH_BORED)
 *          to display the help panel (or specify ID_XFH_BORED
 *          in the callbacks to ntbInsertPage).
 *
 *      For historical reasons, the resids below are defined
 *      in ascending order. Since some external scripts rely
 *      on certain resids, do not change the existing ones.
 *      However, there is no requirement for new resids to
 *      be in ascending order any more, as long as they are
 *      unique.
 */

/*
 *      Copyright (C) 1997-2001 Ulrich M”ller.
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

#ifndef HELPPANELS_HEADER_INCLUDED
    #define HELPPANELS_HEADER_INCLUDED

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
    #define ID_XFH_NOCONFIG         3
    #define ID_XFH_NOOBJECT         4
    #define ID_XFH_LIMITREACHED     5
    #define ID_XFH_NOTEXTCLIP       6
    #define ID_XFH_REBOOTEXT        7
    #define ID_XFH_AUTOCLOSEDETAILS 8
    #define ID_XFH_SELECTCLASS      9
    #define ID_XFH_REGISTERCLASS    10
    #define ID_XFH_TITLECLASH       11

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
    #define ID_XSH_SETTINGS_STATUSBARS1      22
    #define ID_XSH_SETTINGS_SNAPTOGRID       23
    #define ID_XSH_SETTINGS_FOLDERHOTKEYS    24
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
    // #define ID_XSH_SETTINGS_FILEOPS          39     // "File operations" no longer used
    #define ID_XSH_SETTINGS_WPS              40
    #define ID_XSH_SETTINGS_ERRORS           41
    #define ID_XSH_SETTINGS_SYSPATHS         42     // V0.9.0: "System paths"
    #define ID_XSH_SETTINGS_FILETYPES        43     // V0.9.0: "File Types"
    #define ID_XSH_SETTINGS_FILET_MERGE      44
    #define ID_XSH_SETTINGS_SOUND            45
    #define ID_XSH_SETTINGS_TRASHCAN         46     // V0.9.0: XWPTrashCan (two pages)
    #define ID_XSH_SETTINGS_TRASHCAN2        47     // V0.9.0: XWPTrashCan (two pages)
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
    #define ID_XSH_SETTINGS_PAGEMAGE_MOUSE   75
    #define ID_XSH_SETTINGS_FUNCTIONKEYS     76     // V0.9.3: XWPKeyboard "Function keys" page
    #define ID_XSH_SETTINGS_FUNCTIONKEYSDLG  77
    #define ID_XSH_SETTINGS_XWPSTRING_MAIN   78     // V0.9.3: XWPString main help
    #define ID_XSH_SETTINGS_XWPSTRING_PAGE   79     // V0.9.3: XWPString settings page
    #define ID_XSH_MEDIA_DEVICES             80     // V0.9.3: XWPMedia "Devices" page
    #define ID_XSH_MEDIA_CODECS              81     // V0.9.3: XWPMedia "Codecs" page
    #define ID_XSH_MEDIA_IOPROCS             82     // V0.9.3: XWPMedia "IOProcs" page
    #define ID_XSH_SETTINGS_TRASHCAN_DRIVES  83     // V0.9.4: XWPTrashCan "Drives" page
    #define ID_XSH_XWP_INSTALL_FOLDER        84     // installation folder default help;
                                                    // set by install script, do not change
    #define ID_XSH_SETTINGS_PAGEMAGE_STICKY  85     // V0.9.3: XWPScreen "PageMage Sticky" page;
    #define ID_XSH_SETTINGS_PAGEMAGE_STICKY2 86     // V0.9.3: XWPScreen "PageMage Sticky" page;
    #define ID_XSH_SETTINGS_PAGEMAGE_STICKY3 87     // V0.9.3: XWPScreen "PageMage Sticky" page;
    #define ID_XSH_SETTINGS_TRASHCAN_ICON    88     // V0.9.4: XWPTrashCan "Icon" page
    #define ID_XSH_XSHUTDOWN_CONFIRM         89     // V0.9.4: shutdown confirm dlg
    #define ID_XFH_SELECTSOME                90     // V0.9.4: changed this to have it assigned a fixed no. finally
    #define ID_XFH_VIEW_MENU_ITEMS           91     // V0.9.4: added for XFolder "View" submenu items
    #define ID_XSH_DRIVER_HPFS386            92     // V0.9.5: HPFS386 driver dialog help

    #define ID_XSH_XWPMEDIA                  93     // V0.9.5: XWPMedia main panel

    #define ID_XSH_XWP_CLASSESDLG            94     // V0.9.5: XWP "Classes" dlg

    #define ID_XFH_CLOSEVIO                  95     // V0.9.6: was missing

    #define ID_XSH_SETTINGS_PGMFILE_RESOURCES 96    // V0.9.7: progfile "Resources" page

    #define ID_XSH_XCENTER_MAIN              97     // V0.9.7: XCenter default help
    #define ID_XSH_XCENTER_VIEW1             98     // V0.9.7: XCenter "View" page
    #define ID_XSH_XCENTER_VIEW2             99     // V0.9.7: XCenter "View" page
    #define ID_XSH_XCENTER_WIDGETS          100     // V0.9.7: XCenter "Widgets" page
    #define ID_XSH_XCENTER_CLASSES          101     // V0.9.9: XCenter "Classes" page

    #define ID_XSH_WIDGET_CLOCK_MAIN        102     // V0.9.7: Winlist widget main help
    #define ID_XSH_WIDGET_MEMORY_MAIN       103     // V0.9.7: Memory widget main help
    #define ID_XSH_WIDGET_OBJBUTTON_MAIN    104     // V0.9.7: Object button widget main help
    #define ID_XSH_WIDGET_PULSE_MAIN        105     // V0.9.7: Pulse widget main help
    #define ID_XSH_WIDGET_SWAP_MAIN         106     // V0.9.7: Swapper widget main help
    #define ID_XSH_WIDGET_WINLIST_MAIN      107     // V0.9.7: Winlist widget main help
    #define ID_XSH_WIDGET_WINLIST_SETTINGS  108     // V0.9.7: Winlist widget properties
    #define ID_XSH_WIDGET_XBUTTON_MAIN      109     // V0.9.7: X-Button widget main help
    #define ID_XSH_WIDGET_SENTINEL_MAIN     110     // V0.9.9: Sentinal widget main help
    #define ID_XSH_WIDGET_HEALTH_MAIN       111     // V0.9.9: Health widget main help
    #define ID_XSH_WIDGET_HEALTH_SETTINGS   112     // V0.9.9: Health widget main help

    #define ID_XSH_FONTFOLDER               113
    #define ID_XSH_FONTFOLDER_TEXT          114     // V0.9.9 (2001-03-27) [umoeller]
    #define ID_XSH_FONTFILE                 115
    #define ID_XSH_FONTOBJECT               116
    #define ID_XSH_FONTSAMPLEVIEW           117
    #define ID_XSH_FONTSAMPLEHINTS          118

    #define ID_XSH_XFIX_INTRO               119

    #define ID_XSH_RUN                      120

    #define ID_XSH_SETTINGS_PGM_ASSOCIATIONS 121
    #define ID_XSH_SETTINGS_XC_THREADS      122
    #define ID_XSH_SETTINGS_PGMFILE_MODULE1 123
    #define ID_XSH_SETTINGS_PGMFILE_MODULE2 124

    #define ID_XSH_SETTINGS_PAGEMAGE_WINDOW 125     // V0.9.9: XWPScreen "PageMage" page V0.9.9 (2001-03-15) [lafaix]

    #define ID_XSH_DATAFILE_TYPES           126     // V0.9.9: data file types page

    #define ID_XSH_ADMIN_USER               127     // V0.9.11: XWPAdmin "Users" page

    #define ID_XSH_PAGEMAGE_INTRO           128     // V0.9.11: XWPAdmin "Users" page

    #define ID_XSH_SORTPAGE                 129     // V0.9.12: sort page (instance or global)

    #define ID_XSH_WIDGET_POWER_MAIN        130     // V0.9.12 (2001-05-26) [umoeller]

    #define ID_XSH_WIDGET_TRAY              131     // V0.9.13 (2001-06-21) [umoeller]

#endif

