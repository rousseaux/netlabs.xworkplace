
/*
 *@@sourcefile dlgids.h:
 *      this file declares all the dialog id's common to
 *      all XWorkplace components, but only those which are needed
 *      by both the XFolder code and the NLS resource DLLs.
 *
 *      This file is #include'd by the XFolder C code itself
 *      and all the .RC and .DLG files.
 *
 *      Changes for V0.9.0:
 *      -- lots of new IDs, of course, for all the new dialogs.
 *         Note: even the existing IDs have mostly been redefined.
 *      -- greatly rearranged this file, because I was finally
 *         unable to find anything in here any more.
 *      -- dialog ID's for Treesize and NetscapeDDE have been moved
 *         into this file to allow for NLS for these two programs.
 *
 *      Since these dialog ID's are shared across all of XWorkplace,
 *      we need to have "number spaces" for the different developers.
 *      If you add resources to XFLDRxxx.DLL, add your "number space"
 *      to the list below. Do not add resources which are part of
 *      another developer's number space, or we'll get into trouble.
 *
 *      NOTE: The ID's in this file are limited to 12000. All resource
 *            ID's must be below that number because numbers above that
 *            are reserved for other XWorkplace parts. For example,
 *            the animated mouse pointers use ID's 0x7000 (28672) and
 *            above. V0.9.4 (2000-06-15) [umoeller]
 *
 *      Current number spaces:
 *      -- Ulrich M”ller:  200-12000
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

#ifndef DLGIDS_HEADER_INCLUDED
    #define DLGIDS_HEADER_INCLUDED

/* Naming conventions:
   All ID's (= def's for integers) begin with "ID_",
   then comes an abbreviation of the context of the ID:
   -    XF   for XFolder,
   -    XS   for XWorkplace settings (XFldSystem, XFldWPS, Desktop),
   -    OS   for "OS/2 Kernel" settings
   -    XC   for XWPSetup object ("XWorkplace Setup", new with V0.9.0),
   -    XL   for XWPClassList object ("WPS Class List", new with V0.9.0)
   -    CR   for XCenter object (V0.9.7 (2000-11-39) [umoeller])
   -    SD   for XShutdown,
   -    XT   for XWPTrashCan,
   -    FN   for fonts,
   -    WP   for previously undocumented WPS ID's,
   -    TS   for Treesize;
   then comes an abbreviation of the ID type:
   -    M    for a menu template,
   -    MI   for a menu item,
   -    D    for a dialog template,
   -    DI   for a dialog item;
   -    H    for a help panel res id.

   Example:
        ID_SDDI_SKIPAPP is a dlg item ID for use with XShutdown
                        (the "Skip" button in the status window).
*/

// XFolder version flags; since this file is
// #include'd in the NLS DLLs also, this string
// will be readable as a resource in the NLS DLL
#define XFOLDER_VERSION        "0.9.8"

// this sets the minimum version number for NLS DLLS
// which XFolder will accept
#define MINIMUM_NLS_VERSION    "0.9.8"

// icons / bitmaps
#define ID_ICON1               100
#define ID_ICON2               101
#define ID_ICONDLG             102
#define ID_ICONSHUTDOWN        103

#define ID_STARTICON1          104
#define ID_STARTICON2          105
#define ID_SHUTICON1           106
#define ID_SHUTICON2           107

#define ID_ICONSYS             108
#define ID_ICONWPS             109

#define ID_ICONSDANIM1         110
#define ID_ICONSDANIM2         111
#define ID_ICONSDANIM3         112
#define ID_ICONSDANIM4         113
#define ID_ICONSDANIM5         114

// #define ID_ICONMENUARROW4      115 removed V0.9.7 (2001-01-26) [umoeller]
// #define ID_ICONMENUARROW3      116

#define ID_ICONXWPCONFG        117
#define ID_ICONXWPLIST         118
#define ID_ICONXWPSOUND        119
#define ID_ICONXWPSCREEN       120

#define ID_ICONXWPTRASHEMPTY   121
#define ID_ICONXWPTRASHFILLED  122

#define ID_ICONXWPMEDIA        123
#define ID_ICONXWPSTRING       124
#define ID_ICONXMMCDPLAY       125
#define ID_ICONXMMVOLUME       126
#define ID_ICONXCENTER         127
#define ID_ICONXMINI           128

#define ID_XFLDRBITMAP         130

#define ID_XWPBIGLOGO          131

#define ID_ICON_CDEJECT        140
#define ID_ICON_CDNEXT         141
#define ID_ICON_CDPAUSE        142
#define ID_ICON_CDPLAY         143
#define ID_ICON_CDPREV         144
#define ID_ICON_CDSTOP         145

#define ID_ICONXWPFONTCLOSED   150
#define ID_ICONXWPFONTOPEN     151
#define ID_ICONXWPFONTOBJ      152

/******************************************
 * generics                        < 100  *
 ******************************************/

/* notebook buttons; these must be < 100
   so that they can be moved for Warp 4
   notebooks */
#define DID_APPLY              98
#define DID_HELP               97
#define DID_UNDO               96
#define DID_DEFAULT            95
#define DID_OPTIMIZE           94
#define DID_REFRESH            93
#define DID_SHOWTOOLTIPS       92           // always used for tooltip controls, V0.9.0
#define DID_TOOLTIP            91           // always used for tooltip controls, V0.9.0

/* DID_OK and DID_CANCEL are def'd somewhere in os2.h */

/* Notebook pages */
// #define ID_XSD_SET5INTERNALS        409  // removed (V0.9.0)
// #define ID_XSD_SETTINGS_DTP2        407  // removed (V0.9.0)
// #define ID_XCD_FILEOPS              412  // removed (V0.9.0)

/******************************************
 *  "real" dialogs                >= 200  *
 ******************************************/

// for product info
#define ID_XFD_PRODINFO                 200
#define ID_XFD_PRODLOGO                 201
#define ID_XFDI_XFLDVERSION             202

// "Select by name" dlg items
#define ID_XFD_SELECTSOME               210
#define ID_XFDI_SOME_ENTRYFIELD         211
#define ID_XFDI_SOME_SELECT             212
#define ID_XFDI_SOME_DESELECT           213
#define ID_XFDI_SOME_SELECTALL          214
#define ID_XFDI_SOME_DESELECTALL        215

// generic dlg text
#define ID_XFD_GENERICDLG               220
#define ID_XFDI_GENERICDLGTEXT          221
#define ID_XFDI_GENERICDLGICON          222

// "Title clash" dlg
#define ID_XFD_TITLECLASH               230
#define ID_XFDI_CLASH_TXT1              231
#define ID_XFDI_CLASH_RENAMENEW         232
#define ID_XFDI_CLASH_RENAMENEWTXT      233
#define ID_XFDI_CLASH_REPLACE           234
#define ID_XFDI_CLASH_APPEND            235
#define ID_XFDI_CLASH_RENAMEOLD         236
#define ID_XFDI_CLASH_RENAMEOLDTXT      237
#define ID_XFDI_CLASH_DATEOLD           238
#define ID_XFDI_CLASH_TIMEOLD           239
#define ID_XFDI_CLASH_SIZEOLD           240
#define ID_XFDI_CLASH_DATENEW           241
#define ID_XFDI_CLASH_TIMENEW           242
#define ID_XFDI_CLASH_SIZENEW           243

// bootup status
#define ID_XFD_BOOTUPSTATUS             250
#define ID_XFD_ARCHIVINGSTATUS          251
#define ID_XFDI_BOOTUPSTATUSTEXT        252

// "Startup panic" dialog, V0.9.0
#define ID_XFD_STARTUPPANIC             260
#define ID_XFDI_PANIC_SKIPBOOTLOGO      261
#define ID_XFDI_PANIC_SKIPXFLDSTARTUP   262
#define ID_XFDI_PANIC_SKIPQUICKOPEN     263
#define ID_XFDI_PANIC_NOARCHIVING       264
#define ID_XFDI_PANIC_DISABLEFEATURES   265
#define ID_XFDI_PANIC_DISABLEREPLICONS  266
#define ID_XFDI_PANIC_REMOVEHOTKEYS     267
#define ID_XFDI_PANIC_DISABLEPAGEMAGE   268
#define ID_XFDI_PANIC_DISABLEMULTIMEDIA 269

/* Dialog box templates */
#define ID_XFD_NOCONFIG                 270
#define ID_XFD_NOOBJECT                 271
#define ID_XFD_LIMITREACHED             272
#define ID_XFD_WRONGVERSION             273

#define ID_XFD_NOTEXTCLIP               280
// #define ID_XFD_NOICONVIEW               281      removed V0.9.2 (2000-03-04) [umoeller]

#define ID_XFD_WELCOME                  290
#define ID_XFD_CREATINGCONFIG           291

#define ID_XFD_STARTUPSTATUS            292

#define ID_XFD_FILEOPSSTATUS            293
#define ID_XSDI_SOURCEFOLDER            294
#define ID_XSDI_SOURCEOBJECT            295
#define ID_XSDI_SUBOBJECT               296
#define ID_XSDI_TARGETFOLDER            297
#define ID_XSDI_TARGETFOLDER_TXT        298

// generic container page
#define ID_XFD_CONTAINERPAGE            300
#define ID_XFDI_CNR_CNR                 301
#define ID_XFDI_CNR_GROUPTITLE          302

/******************************************
 * "Workplace Shell" (XFldWPS)     >= 500 *
 ******************************************/

// "View" page (added V0.9.0)
#define ID_XSD_FOLDERVIEWS              500
// #define ID_XSDI_ADDINTERNALS            501  // removed (V0.9.0)
// #define ID_XSDI_REPLICONS               501  // removed (V0.9.0)
#define ID_XSDI_FULLPATH                502
#define ID_XSDI_KEEPTITLE               503
#define ID_XSDI_MAXPATHCHARS            504
#define ID_XSDI_MAXPATHCHARS_TX1        505
#define ID_XSDI_MAXPATHCHARS_TX2        506
#define ID_XSDI_TREEVIEWAUTOSCROLL      507
#define ID_XSDI_FDRDEFAULTDOC           508
#define ID_XSDI_FDRDEFAULTDOCVIEW       509

// "Remove menu items" page
#define ID_XSD_SET2REMOVEMENUS          510
#define ID_XSDI_FIND                    511
#define ID_XSDI_SORT                    512
#define ID_XSDI_SELECT                  513
#define ID_XSDI_WARP4DISPLAY            514
#define ID_XSDI_ARRANGE                 515
#define ID_XSDI_INSERT                  516
#define ID_XSDI_CHECKDISK               517
#define ID_XSDI_FORMATDISK              518
#define ID_XSDI_HELP                    519
#define ID_XSDI_CRANOTHER               520
#define ID_XSDI_COPY                    521
#define ID_XSDI_MOVE                    522
#define ID_XSDI_SHADOW                  523
#define ID_XSDI_DELETE                  524
#define ID_XSDI_PICKUP                  525
#define ID_XSDI_LOCKINPLACE             526
#define ID_XSDI_PRINT                   527
#define ID_XSDI_LOCKINPLACE_NOSUB       528 // V0.9.7 (2000-12-10) [umoeller]

// new menu items
#define ID_XSD_SET25ADDMENUS            530
#define ID_XSDI_FILEATTRIBS             531
#define ID_XSDI_COPYFILENAME            532
#define ID_XSDI_MOVE4REFRESH            533
#define ID_XSDI_SELECTSOME              534
#define ID_XSDI_FLDRVIEWS               535
#define ID_XSDI_FOLDERCONTENT           536
#define ID_XSDI_FC_SHOWICONS            537

// "XFolder Internals": removed with V0.9.0

// "Config folder menu items" page
#define ID_XSD_SET26CONFIGMENUS         540
#define ID_XSDI_CASCADE                 541
#define ID_XSDI_REMOVEX                 542
#define ID_XSDI_APPDPARAM               543

#define ID_XSDI_TPL_DONOTHING           544
#define ID_XSDI_TPL_EDITTITLE           545
#define ID_XSDI_TPL_OPENSETTINGS        546
#define ID_XSDI_TPL_POSITION            547

// "snap to grid" page
#define ID_XSD_SET3SNAPTOGRID           550
#define ID_XSDI_SNAPTOGRID              511
#define ID_XSDI_GRID_X                  512
#define ID_XSDI_GRID_Y                  513
#define ID_XSDI_GRID_CX                 514
#define ID_XSDI_GRID_CY                 515

// "folder hotkeys" page
#define ID_XSD_SET4ACCELS               560
#define ID_XSDI_ACCELERATORS            561
#define ID_XSDI_LISTBOX                 562
#define ID_XSDI_DESCRIPTION             563
#define ID_XSDI_DESCRIPTION_TX1         564     // text
#define ID_XSDI_CLEARACCEL              565
#define ID_XSDI_SHOWINMENUS             566     // V0.9.2 (2000-03-08) [umoeller]

// "status bars" page 1
#define ID_XSD_SET27STATUSBARS          570
#define ID_XSDI_ENABLESTATUSBAR         571
#define ID_XSDI_SBFORICONVIEWS          572
#define ID_XSDI_SBFORTREEVIEWS          573
#define ID_XSDI_SBFORDETAILSVIEWS       574
#define ID_XSDI_SBSTYLE_3RAISED         575
#define ID_XSDI_SBSTYLE_3SUNKEN         576
#define ID_XSDI_SBSTYLE_4RECT           577
#define ID_XSDI_SBSTYLE_4MENU           578

// "status bars" page 2
#define ID_XSD_SET28STATUSBARS2         580
#define ID_XSDI_SBTEXTNONESEL           581
#define ID_XSDI_SBCURCLASS              582
#define ID_XSDI_SBSELECTCLASS           583
#define ID_XSDI_SBTEXT1SEL              584
#define ID_XSDI_SBTEXTMULTISEL          585
#define ID_XSDI_DEREFSHADOWS_SINGLE     586     // new V0.9.5 (2000-10-07) [umoeller]
#define ID_XSDI_DEREFSHADOWS_MULTIPLE   587     // new V0.9.5 (2000-10-07) [umoeller]

// extended "sort" page
#define ID_XSD_SETTINGS_FLDRSORT        590
#define ID_XSDI_ALWAYSSORT              591
#define ID_XSDI_SORTLISTBOX             592
//  #define ID_XSDI_REPLACESORT      675    // removed (V0.9.0)
#define ID_XSDI_SORTTEXT                593

// "File types" page (new with V0.9.0)
#define ID_XSD_FILETYPES                600     // "File types" page in XFldWPS (V0.9.0)
#define ID_XSDI_FT_GROUP                601
#define ID_XSDI_FT_CONTAINER            602
#define ID_XSDI_FT_FILTERS_TXT          603
#define ID_XSDI_FT_FILTERSCNR           604
#define ID_XSDI_FT_ASSOCS_TXT           605
#define ID_XSDI_FT_ASSOCSCNR            606

// "Import WPS Filters" dialog (V0.9.0
#define ID_XSD_IMPORTWPS                610     // "Import WPS filter" dlg (V0.9.0)
#define ID_XSDI_FT_TYPE                 611
#define ID_XSDI_FT_FILTERLIST           612
#define ID_XSDI_FT_NEW                  613
#define ID_XSDI_FT_SELALL               614
#define ID_XSDI_FT_DESELALL             615

// "New file type" dialog (V0.9.0)
#define ID_XSD_NEWFILETYPE              620     // "New File type" dlg (V0.9.0)
#define ID_XSD_NEWFILTER                621     // "New Filter" dlg (V0.9.0)
#define ID_XSDI_FT_ENTRYFIELD           622
#define ID_XSDI_FT_TITLE                623

/******************************************
 * Instance settings notebooks     >=700  *
 ******************************************/

// XFolder page in folder notebook
// (also uses some ID's def'd above)
#define ID_XSD_SETTINGS_FLDR1           710
#define ID_XSDI_FAVORITEFOLDER          711
#define ID_XSDI_QUICKOPEN               712

// "Internals" page in all object notebooks
#define ID_XSD_OBJECTDETAILS            720
#define ID_XSDI_DTL_CNR                 721
#define ID_XSDI_DTL_HOTKEY              722
#define ID_XSDI_DTL_HOTKEY_TXT          723
#define ID_XSDI_DTL_CLEAR               724
#define ID_XSDI_DTL_SETUP_ENTRY         725
#define ID_XSDI_DTL_SET                 726

// "File" page replacement (XFolder, XFldDataFile, V0.9.0)
#define ID_XSD_FILESPAGE1               730
#define ID_XSDI_FILES_REALNAME          731
#define ID_XSDI_FILES_CREATIONDATE      732
#define ID_XSDI_FILES_CREATIONTIME      733
#define ID_XSDI_FILES_LASTWRITEDATE     734
#define ID_XSDI_FILES_LASTWRITETIME     735
#define ID_XSDI_FILES_LASTACCESSDATE    736
#define ID_XSDI_FILES_LASTACCESSTIME    737
#define ID_XSDI_FILES_ATTR_ARCHIVED     738
#define ID_XSDI_FILES_ATTR_READONLY     739
#define ID_XSDI_FILES_ATTR_HIDDEN       740
#define ID_XSDI_FILES_ATTR_SYSTEM       741
#define ID_XSDI_FILES_SUBJECT           742
#define ID_XSDI_FILES_COMMENTS          743
#define ID_XSDI_FILES_KEYPHRASES        744
#define ID_XSDI_FILES_VERSION           745
#define ID_XSDI_FILES_FILESIZE          746
#define ID_XSDI_FILES_EASIZE            747
#define ID_XSDI_FILES_WORKAREA          748     // added V0.9.1 (99-12-20) [umoeller]

#define ID_XSD_FILESPAGE2               760
#define ID_XSDI_FILES_EALIST            761
#define ID_XSDI_FILES_EAINFO            762
#define ID_XSDI_FILES_EACONTENTS        763

// "Details" page replacement (XFldDisk, V0.9.0)
#define ID_XSD_DISK_DETAILS             770
#define ID_XSDI_DISK_LABEL              771
#define ID_XSDI_DISK_FILESYSTEM         772
#define ID_XSDI_DISK_SECTORSIZE         775
#define ID_XSDI_DISK_TOTAL_SECTORS      776
#define ID_XSDI_DISK_TOTAL_BYTES        777
#define ID_XSDI_DISK_ALLOCATED_SECTORS  778
#define ID_XSDI_DISK_ALLOCATED_BYTES    779
#define ID_XSDI_DISK_AVAILABLE_SECTORS  780
#define ID_XSDI_DISK_AVAILABLE_BYTES    781
#define ID_XSDI_DISK_CHART              782

// "Module" page (XFldProgramFile, V0.9.0)
#define ID_XSD_PGMFILE_MODULE           789
#define ID_XSDI_PROG_FILENAME           781
#define ID_XSDI_PROG_PARAMETERS         782
#define ID_XSDI_PROG_WORKINGDIR         783
#define ID_XSDI_PROG_EXEFORMAT          784
#define ID_XSDI_PROG_TARGETOS           785
#define ID_XSDI_PROG_VENDOR             786
#define ID_XSDI_PROG_VERSION            787
#define ID_XSDI_PROG_DESCRIPTION        788

// "Resources" page (XFldProgramFile, V0.9.7)
#define ID_XSD_PGMFILE_RESOURCES        790
#define ID_XSDI_PROG_RESOURCES          791

/******************************************
 * XWPSound (V0.9.0)              >= 1000  *
 ******************************************/

#define ID_XSD_XWPSOUND                 1000    // new "Sounds" page (XWPSound, V0.9.0)
#define ID_XSD_NEWSOUNDSCHEME           1001    // "New Sound Scheme" dlg, V0.9.0

#define ID_XSDI_SOUND_ENABLE            1002
#define ID_XSDI_SOUND_SCHEMES_DROPDOWN  1003
#define ID_XSDI_SOUND_SCHEMES_SAVEAS    1004
#define ID_XSDI_SOUND_SCHEMES_DELETE    1005
#define ID_XSDI_SOUND_EVENTSLISTBOX     1006
#define ID_XSDI_SOUND_FILE              1007
#define ID_XSDI_SOUND_BROWSE            1008
#define ID_XSDI_SOUND_PLAY              1009
#define ID_XSDI_SOUND_COMMONVOLUME      1010
#define ID_XSDI_SOUND_VOLUMELEVER       1011

/******************************************
 * XWPSetup (V0.9.0)             >= 1100   *
 ******************************************/

// XWPSetup info page (V0.9.0)
#define ID_XCD_STATUS                   1100
#define ID_XCDI_INFO_KERNEL_RELEASE     1101
// #define ID_XCDI_INFO_KERNEL_BUILD       1102 removed V0.9.2 (2000-02-20) [umoeller]
// #define ID_XCDI_INFO_KERNEL_LOCALE      1103
#define ID_XCDI_INFO_AWAKEOBJECTS       1103
#define ID_XCDI_INFO_WPSTHREADS         1104
#define ID_XCDI_INFO_WPSRESTARTS        1105
#define ID_XCDI_INFO_WORKERSTATUS       1106
#define ID_XCDI_INFO_FILESTATUS         1107
#define ID_XCDI_INFO_QUICKSTATUS        1108
#define ID_XCDI_INFO_SOUNDSTATUS        1109
#define ID_XCDI_INFO_HOOKSTATUS         1110
#define ID_XCDI_INFO_LANGUAGE           1111
#define ID_XCDI_INFO_NLS_RELEASE        1112
#define ID_XCDI_INFO_NLS_AUTHOR         1113

// XWPSetup "Features" page (V0.9.0)
#define ID_XCD_FEATURES                 1120
#define ID_XCDI_CONTAINER               1140
#define ID_XCDI_SETUP                   1141

// XWPSetup first page V0.9.6 (2000-11-04) [umoeller]
#define ID_XCD_FIRST                    1150


// XWPSetup "File operations" page (V0.9.0)
// #define ID_XCD_FILEOPS                  1150
// #define ID_XCDI_EXTASSOCS               1151
// #define ID_XCDI_IGNOREFILTERS           1152
// #define ID_XCDI_CLEANUPINIS             1153
// #define ID_XCDI_REPLFILEEXISTS          1154
// #define ID_XCDI_REPLDRIVENOTREADY       1155

// XWPSetup "Paranoia" page (V0.9.0)
#define ID_XCD_PARANOIA                 1160
#define ID_XCDI_VARMENUOFFSET           1161
#define ID_XCDI_NOFREAKYMENUS           1162
#define ID_XCDI_NOSUBCLASSING           1163
#define ID_XCDI_NOWORKERTHREAD          1164
#define ID_XCDI_USE8HELVFONT            1165
#define ID_XCDI_NOEXCPTBEEPS            1166
#define ID_XCDI_WORKERPRTY_SLIDER       1167
#define ID_XCDI_WORKERPRTY_BEEP         1168
// the following two are for the descriptive texts;
// they must have higher IDs, or the help panels won't work
#define ID_XCDI_WORKERPRTY_TEXT1        1169
#define ID_XCDI_WORKERPRTY_TEXT2        1170

// XWPSetup "Objects" page (V0.9.0)
#define ID_XCD_OBJECTS                  1180
#define ID_XCD_OBJECTS_SYSTEM           1181
#define ID_XCD_OBJECTS_XWORKPLACE       1182
#define ID_XCD_OBJECTS_CONFIGFOLDER     1183

// logo window
#define ID_XFDI_LOGOBITMAP              1190

// "Installed XWorkplace Classes" dlg
#define ID_XCD_XWPINSTALLEDCLASSES      1250

#define ID_XCDI_XWPCLS_XFLDOBJECT       1251
#define ID_XCDI_XWPCLS_XFOLDER          1252
#define ID_XCDI_XWPCLS_XFLDDISK         1253
#define ID_XCDI_XWPCLS_XFLDDESKTOP      1254
#define ID_XCDI_XWPCLS_XFLDDATAFILE     1255
#define ID_XCDI_XWPCLS_XFLDPROGRAMFILE  1256
#define ID_XCDI_XWPCLS_XWPSOUND         1257
#define ID_XCDI_XWPCLS_XWPMOUSE         1258
#define ID_XCDI_XWPCLS_XWPKEYBOARD      1259

#define ID_XCDI_XWPCLS_XWPSETUP         1260
#define ID_XCDI_XWPCLS_XFLDSYSTEM       1261
#define ID_XCDI_XWPCLS_XFLDWPS          1262
#define ID_XCDI_XWPCLS_XFLDSTARTUP      1263
#define ID_XCDI_XWPCLS_XFLDSHUTDOWN     1264
#define ID_XCDI_XWPCLS_XWPCLASSLIST     1265
#define ID_XCDI_XWPCLS_XWPTRASHCAN      1266

// new classes with V0.9.3
#define ID_XCDI_XWPCLS_XWPSCREEN        1267
#define ID_XCDI_XWPCLS_XWPSTRING        1268

// new classes with V0.9.4
#define ID_XCDI_XWPCLS_XWPMEDIA         1269

// new classes with V0.9.7
#define ID_XCDI_XWPCLS_XCENTER          1270

/******************************************
 * XWPScreen ("Screen", PageMage) >= 1400 *
 ******************************************/

#define ID_SCD_PAGEMAGE_GENERAL         1400
#define ID_SCDI_PGMG1_X_SLIDER          1401
#define ID_SCDI_PGMG1_X_TEXT2           1402
#define ID_SCDI_PGMG1_Y_SLIDER          1403
#define ID_SCDI_PGMG1_Y_TEXT2           1404
#define ID_SCDI_PGMG1_SHOWWINDOWS       1405
#define ID_SCDI_PGMG1_SHOWWINTITLES     1406
#define ID_SCDI_PGMG1_CLICK2ACTIVATE    1407
#define ID_SCDI_PGMG1_TITLEBAR          1408
#define ID_SCDI_PGMG1_PRESERVEPROPS     1409
#define ID_SCDI_PGMG1_STAYONTOP         1410
#define ID_SCDI_PGMG1_FLASHTOTOP        1411
#define ID_SCDI_PGMG1_FLASH_TXT1        1412
#define ID_SCDI_PGMG1_FLASH_SPIN        1413
#define ID_SCDI_PGMG1_FLASH_TXT2        1414
#define ID_SCDI_PGMG1_ARROWHOTKEYS      1415
#define ID_SCDI_PGMG1_HOTKEYS_CTRL      1416
#define ID_SCDI_PGMG1_HOTKEYS_SHIFT     1417
#define ID_SCDI_PGMG1_HOTKEYS_ALT       1418

#define ID_SCD_PAGEMAGE_COLORS          1430
#define ID_SCDI_PGMG2_DTP_INACTIVE      1431
#define ID_SCDI_PGMG2_DTP_ACTIVE        1432
#define ID_SCDI_PGMG2_DTP_BORDER        1433
#define ID_SCDI_PGMG2_WIN_INACTIVE      1434
#define ID_SCDI_PGMG2_WIN_ACTIVE        1435
#define ID_SCDI_PGMG2_WIN_BORDER        1436
#define ID_SCDI_PGMG2_TXT_INACTIVE      1437
#define ID_SCDI_PGMG2_TXT_ACTIVE        1438

#define ID_SCD_PAGEMAGE_STICKY          1450
#define ID_SCDI_PGMG_STICKY_CNR         1451

#define ID_SCD_PAGEMAGE_NEWSTICKY       1460
#define ID_SCD_PAGEMAGE_COMBO_STICKIES  1461

/******************************************
 * XFldSystem ("OS/2 Kernel")    >= 1500  *
 ******************************************/

#define ID_OSD_SETTINGS_KERNEL1         1500
#define ID_OSD_SETTINGS_KERNEL2         1501
#define ID_OSDI_CURRENTTHREADS          1502
#define ID_OSDI_MAXTHREADS              1503
#define ID_OSDI_MAXWAIT                 1504
#define ID_OSDI_PRIORITYDISKIO          1505
#define ID_OSDI_CURRENTSWAPSIZE         1506
#define ID_OSDI_PHYSICALMEMORY          1507
#define ID_OSDI_MINSWAPSIZE             1508
#define ID_OSDI_MINSWAPFREE             1509
#define ID_OSDI_SWAPPATH                1510

// FAT page; this is also used by the driver dialogs
#define ID_OSD_SETTINGS_FAT             1521
#define ID_OSDI_FSINSTALLED             1522
#define ID_OSDI_CACHESIZE               1523
#define ID_OSDI_CACHESIZE_TXT           1524
#define ID_OSDI_CACHESIZE_AUTO          1525
#define ID_OSDI_CACHE_THRESHOLD         1526
#define ID_OSDI_CACHE_THRESHOLD_TXT     1527
#define ID_OSDI_CACHE_LAZYWRITE         1528
#define ID_OSDI_CACHE_MAXAGE            1529
#define ID_OSDI_CACHE_BUFFERIDLE        1530
#define ID_OSDI_CACHE_DISKIDLE          1531
#define ID_OSDI_AUTOCHECK               1532
#define ID_OSDI_AUTOCHECK_PROPOSE       1533
#define ID_OSDI_HPFS386INI_GROUP        1534
#define ID_OSDI_HPFS386INI_CNR          1535

#define ID_OSD_SETTINGS_ERRORS          1540
#define ID_OSDI_AUTOFAIL                1541
#define ID_OSDI_SUPRESSPOPUPS           1542
#define ID_OSDI_SUPRESSP_DRIVE          1543
#define ID_OSDI_REIPL                   1544

#define ID_OSD_SETTINGS_WPS             1550
#define ID_OSDI_AUTO_PROGRAMS           1551
#define ID_OSDI_AUTO_TASKLIST           1552
#define ID_OSDI_AUTO_CONNECTIONS        1553
#define ID_OSDI_AUTO_LAUNCHPAD          1554
#define ID_OSDI_AUTO_WARPCENTER         1555
#define ID_OSDI_RESTART_YES             1556
#define ID_OSDI_RESTART_NO              1557
#define ID_OSDI_RESTART_FOLDERS         1558
#define ID_OSDI_RESTART_REBOOT          1559
#define ID_OSDI_AUTOREFRESHFOLDERS      1560

// "System paths" page (V0.9.0)
#define ID_OSD_SETTINGS_SYSPATHS        1570
#define ID_OSD_NEWSYSPATH               1571   // "New System Path" dlg
#define ID_OSDI_PATHDROPDOWN            1572
#define ID_OSDI_PATHLISTBOX             1573
#define ID_OSDI_PATHNEW                 1574
#define ID_OSDI_PATHDELETE              1575
#define ID_OSDI_PATHUP                  1576
#define ID_OSDI_PATHDOWN                1577
#define ID_OSDI_VALIDATE                1578
#define ID_OSDI_PATHINFOTXT             1579
#define ID_OSDI_DOUBLEFILES             1580

#define ID_OSD_FILELIST                 1581
#define ID_OSDI_FILELISTSYSPATH1        1582
#define ID_OSDI_FILELISTSYSPATH2        1583
#define ID_OSDI_FILELISTCNR             1584

// "Drivers" page (V0.9.0)
#define ID_OSD_SETTINGS_DRIVERS         1585
#define ID_OSDI_DRIVR_CNR               1586
#define ID_OSDI_DRIVR_STATICDATA        1587
#define ID_OSDI_DRIVR_PARAMS            1588
#define ID_OSDI_DRIVR_CONFIGURE         1589
#define ID_OSDI_DRIVR_APPLYTHIS         1590 // "Apply", non-notebook button
#define ID_OSDI_DRIVR_GROUP1            1591
#define ID_OSDI_DRIVR_GROUP2            1592
#define ID_OSDI_DRIVR_PARAMS_TXT        1593

#define ID_OSD_DRIVER_HPFS386           1597
#define ID_OSD_DRIVER_FAT               1598
#define ID_OSD_DRIVER_HPFS              1599

#define ID_OSD_DRIVER_CDFS              1600
#define ID_OSDI_CDFS_JOLIET             1601
#define ID_OSDI_CDFS_KANJI              1602
#define ID_OSDI_CDFS_CACHESLIDER        1603
#define ID_OSDI_CDFS_CACHETXT           1604
#define ID_OSDI_CDFS_SECTORSSLIDER      1605
#define ID_OSDI_CDFS_SECTORSTXT         1606
#define ID_OSDI_CDFS_INITDEFAULT        1607
#define ID_OSDI_CDFS_INITQUIET          1608
#define ID_OSDI_CDFS_INITVERBOSE        1609

// "IBM1S506" dialog; do not modify these IDs, because
// the TMF file uses these too
#define ID_OSD_DRIVER_IBM1S506          1620
#define ID_OSDI_S506_INITQUIET          1621
#define ID_OSDI_S506_INITVERBOSE        1622
#define ID_OSDI_S506_INITWAIT           1623
#define ID_OSDI_S506_DSG                1624
#define ID_OSDI_S506_ADAPTER0           1625
#define ID_OSDI_S506_ADAPTER1           1626
#define ID_OSDI_S506_A_IGNORE           1627
#define ID_OSDI_S506_A_RESET            1628
#define ID_OSDI_S506_A_BASEADDR_CHECK   1629
#define ID_OSDI_S506_A_BASEADDR_ENTRY   1630
#define ID_OSDI_S506_A_IRQ_CHECK        1631
#define ID_OSDI_S506_A_IRQ_SLIDER       1632
#define ID_OSDI_S506_A_IRQ_TXT          1633
#define ID_OSDI_S506_A_DMA_CHECK        1634
#define ID_OSDI_S506_A_DMA_SPIN         1635
#define ID_OSDI_S506_A_DSGADDR_CHECK    1636
#define ID_OSDI_S506_A_DSGADDR_ENTRY    1637
#define ID_OSDI_S506_A_BUSMASTER        1638
#define ID_OSDI_S506_UNIT0              1639
#define ID_OSDI_S506_UNIT1              1640
#define ID_OSDI_S506_UNIT2              1641
#define ID_OSDI_S506_UNIT3              1642
#define ID_OSDI_S506_U_BUSMASTER        1643
#define ID_OSDI_S506_U_RECOVERY_CHECK   1644
#define ID_OSDI_S506_U_RECOVERY_SLIDER  1645
#define ID_OSDI_S506_U_RECOVERY_TXT     1646
#define ID_OSDI_S506_U_GEO_CHECK        1647
#define ID_OSDI_S506_U_GEO_ENTRY        1648
#define ID_OSDI_S506_U_SMS              1652
#define ID_OSDI_S506_U_LBA              1653
#define ID_OSDI_S506_U_DASD             1654
#define ID_OSDI_S506_U_FORCE            1655
#define ID_OSDI_S506_U_ATAPI            1656
#define ID_OSDI_S506_NEWPARAMS          1657

#define ID_OSDI_DANIS506_CLOCK_CHECK        1660
#define ID_OSDI_DANIS506_CLOCK_SLIDER       1661
#define ID_OSDI_DANIS506_CLOCK_TXT          1662
#define ID_OSDI_DANIS506_GBM                1663
#define ID_OSDI_DANIS506_FORCEGBM           1664
#define ID_OSDI_DANIS506_MGAFIX             1665
#define ID_OSDI_DANIS506_U_TIMEOUT_CHECK    1666
#define ID_OSDI_DANIS506_U_TIMEOUT_SPIN     1667
#define ID_OSDI_DANIS506_U_RATE_CHECK       1668
#define ID_OSDI_DANIS506_U_RATE_UDMA_TXT    1669
#define ID_OSDI_DANIS506_U_RATE_UDMA_SPIN   1670
#define ID_OSDI_DANIS506_U_RATE_MWDMA_TXT   1671
#define ID_OSDI_DANIS506_U_RATE_MWDMA_SPIN  1672
#define ID_OSDI_DANIS506_U_RATE_PIO_TXT     1673
#define ID_OSDI_DANIS506_U_RATE_PIO_SPIN    1674
#define ID_OSDI_DANIS506_U_REMOVEABLE       1675

// syslevel page V0.9.2 (2000-03-08) [umoeller]
            // uses generic cnr page
// #define ID_OSD_SETTINGS_SYSLEVEL            1900
// #define ID_OSDI_SYSLEVEL_CNR                1901

/******************************************
 * XWPClassList (V0.9.0)                   *
 ******************************************/

// class list dialog (left part of split view)
#define ID_XLD_CLASSLIST                2000

// class info dialog (top right part of split view)
#define ID_XLD_CLASSINFO                2001

// method info dialog (bottom right part of split view)
#define ID_XLD_METHODINFO               2002

// other dlgs
#define ID_XLD_SELECTCLASS              2003
#define ID_XLD_REGISTERCLASS            2004

// class list dlg items
#define ID_XLDI_INTROTEXT               2010
#define ID_XLDI_CNR                     2011
#define ID_XLDI_TEXT2                   2012
#define ID_XLDI_CLASSICON               2014
#define ID_XLDI_CLASSNAME               2015
#define ID_XLDI_REPLACEDBY              2016
#define ID_XLDI_CLASSTITLE              2017
#define ID_XLDI_CLASSMODULE             2018
#define ID_XLDI_CLASSNAMETXT            2019
#define ID_XLDI_REPLACEDBYTXT           2020
#define ID_XLDI_CLASSTITLETXT           2021
#define ID_XLDI_CLASSMODULETXT          2022
#define ID_XLDI_DLL                     2023
#define ID_XLDI_BROWSE                  2024
#define ID_XLDI_ICON                    2025
#define ID_XLDI_ICONTXT                 2026
#define ID_XLDI_RADIO_CLASSMETHODS      2027
#define ID_XLDI_RADIO_INSTANCEMETHODS   2028

// class list notebook settings page
#define ID_XLD_SETTINGS                 2030
#define ID_XLDI_SHOWSOMOBJECT           2031    // corresponds to IDL instance setting
#define ID_XLDI_SHOWMETHODS             2032    // corresponds to IDL instance setting

/******************************************
 * XFldDesktop                    >=2100  *
 ******************************************/

#define ID_XSD_STARTUPFOLDER        442     // "Startup" page in XFldStartup;
                                            // new with V0.9.0

// "Menu items" page (V0.9.0)
#define ID_XSD_DTP_MENUITEMS            2100
#define ID_XSDI_DTP_SORT                2101
#define ID_XSDI_DTP_ARRANGE             2102
#define ID_XSDI_DTP_SYSTEMSETUP         2103
#define ID_XSDI_DTP_LOCKUP              2104
#define ID_XSDI_DTP_SHUTDOWN            2105
#define ID_XSDI_DTP_SHUTDOWNMENU        2106
#define ID_XSDI_DTP_LOGOFFNETWORKNOW    2107 // V0.9.7 (2000-12-13) [umoeller]

// XFldDesktop "Startup" page  (V0.9.0)
#define ID_XSD_DTP_STARTUP              2110
#define ID_XSDI_DTP_BOOTLOGO            2111
#define ID_XSDI_DTP_LOGO_TRANSPARENT    2112
#define ID_XSDI_DTP_LOGO_BLOWUP         2113
#define ID_XSDI_DTP_LOGOFRAME           2114
#define ID_XSDI_DTP_LOGOBITMAP          2115
#define ID_XSDI_DTP_TESTLOGO            2116
#define ID_XSDI_DTP_CREATESTARTUPFLDR   2117
#define ID_XSDI_DTP_LOGOFILETXT         2118
#define ID_XSDI_DTP_LOGOFILE            2119
#define ID_XSDI_DTP_LOGO_BROWSE         2120
#define ID_XSDI_DTP_BOOTUPSTATUS        2121
#define ID_XSDI_DTP_NUMLOCKON           2122  // added V0.9.1 (2000-02-09) [umoeller]

// XFldDesktop "Shutdown" page (V0.9.0)
#define ID_XSD_DTP_SHUTDOWN             2125
#define ID_SDDI_REBOOT                  2126
#define ID_SDDI_ANIMATE_SHUTDOWN        2127
#define ID_SDDI_ANIMATE_REBOOT          2128
#define ID_SDDI_APMPOWEROFF             2129
#define ID_SDDI_DELAY                   2130
#define ID_SDDI_CONFIRM                 2131
#define ID_SDDI_WARPCENTERFIRST         2132
#define ID_SDDI_AUTOCLOSEVIO            2133
#define ID_SDDI_LOG                     2134

#define ID_SDDI_CREATESHUTDOWNFLDR      2135
#define ID_SDDI_SHOWSTARTUPPROGRESS     2136

#define ID_SDDI_STARTUP_INITDELAY_TXT1  2137
#define ID_SDDI_STARTUP_INITDELAY_SLID  2138
#define ID_SDDI_STARTUP_INITDELAY_TXT2  2139
#define ID_SDDI_STARTUP_OBJDELAY_TXT1   2140
#define ID_SDDI_STARTUP_OBJDELAY_SLID   2141
#define ID_SDDI_STARTUP_OBJDELAY_TXT2   2142

#define ID_SDDI_REBOOTEXT               2143
#define ID_SDDI_AUTOCLOSEDETAILS        2144
#define ID_SDDI_APMVERSION              2145
#define ID_SDDI_APMVERSION_TXT          2146
#define ID_SDDI_APMSYS                  2147
#define ID_SDDI_APMSYS_TXT              2148

#define ID_SDDI_SAVEINIS_TXT            2149
#define ID_SDDI_SAVEINIS_LIST           2150

// XFldDesktop "Archives" replacement page (V0.9.0)
#define ID_XSD_DTP_ARCHIVES             2155
#define ID_XSDI_ARC_ENABLE              2156
#define ID_XSDI_ARC_ALWAYS              2157
#define ID_XSDI_ARC_NEXT                2158
#define ID_XSDI_ARC_INI                 2159
#define ID_XSDI_ARC_INI_SPIN            2160
#define ID_XSDI_ARC_INI_SPINTXT1        2161
#define ID_XSDI_ARC_DAYS                2162
#define ID_XSDI_ARC_DAYS_SPIN           2163
#define ID_XSDI_ARC_DAYS_SPINTXT1       2164
#define ID_XSDI_ARC_SHOWSTATUS          2165
#define ID_XSDI_ARC_ARCHIVES_SPIN       2166

// "extended reboot" / "auto-close" dlg items
#define ID_XSD_REBOOTEXT                2170
#define ID_XSDI_XRB_LISTBOX             2171
#define ID_XSDI_XRB_NEW                 2172
#define ID_XSDI_XRB_DELETE              2173
#define ID_XSDI_XRB_UP                  2174
#define ID_XSDI_XRB_DOWN                2175
#define ID_XSDI_XRB_ITEMNAME            2176
#define ID_XSDI_XRB_COMMAND             2177
#define ID_XSDI_XRB_PARTITIONS          2178     // added  (V0.9.0)

#define ID_XSD_AUTOCLOSE                2180
#define ID_XSDI_ACL_WMCLOSE             2181
#define ID_XSDI_ACL_CTRL_C              2182
#define ID_XSDI_ACL_KILLSESSION         2183
#define ID_XSDI_ACL_SKIP                2184

#define ID_XSDI_ACL_STORE               2185    // added V0.9.1 (99-12-10)

#define ID_XSDI_PARTITIONSFIRST         2190    // menu item of first submenu on "Partitions" button (V0.9.0)

/******************************************
 * XWPTrashCan (V0.9.0)                   *
 ******************************************/

#define ID_XTD_SETTINGS                 3000
// #define ID_XTDI_DELETE                  3001
// #define ID_XTDI_EMPTYSTARTUP            3002
// #define ID_XTDI_EMPTYSHUTDOWN           3003
#define ID_XTDI_CONFIRMEMPTY            3004
#define ID_XTDI_CONFIRMDESTROY          3005

#define ID_XTD_DRIVES                   3020
#define ID_XTDI_UNSUPPORTED_LB          3021
#define ID_XTDI_SUPPORTED_LB            3022
#define ID_XTDI_ADD_SUPPORTED           3023
#define ID_XTDI_REMOVE_SUPPORTED        3024

#define ID_XTD_ICONPAGE                 3030
#define ID_XTDI_ICON_TITLEMLE           3031


/******************************************
 * XWPKeyboard (V0.9.0)          >= 3200   *
 ******************************************/

// #define ID_XSD_KEYB_OBJHOTKEYS          3200
// #define ID_XSDI_HOTK_CNR                3201

// #define ID_XSD_KEYB_FUNCTIONKEYS        3220
// #define ID_XSDI_FUNCK_CNR               3221

#define ID_XSD_KEYB_EDITFUNCTIONKEY     3230
#define ID_XSDI_FUNCK_DESCRIPTION_EF    3231
#define ID_XSDI_FUNCK_SCANCODE_EF       3232
#define ID_XSDI_FUNCK_MODIFIER          3233

/******************************************
 * XWPMouse    (V0.9.0)          >= 3400   *
 ******************************************/

#define ID_XSD_MOUSE_MOVEMENT           3400
#define ID_XSDI_MOUSE_SLIDINGFOCUS      3401
#define ID_XSDI_MOUSE_FOCUSDELAY_TXT1   3402
#define ID_XSDI_MOUSE_FOCUSDELAY_SLIDER 3403
#define ID_XSDI_MOUSE_FOCUSDELAY_TXT2   3404
#define ID_XSDI_MOUSE_BRING2TOP         3405
#define ID_XSDI_MOUSE_IGNORESEAMLESS    3406
#define ID_XSDI_MOUSE_IGNOREDESKTOP     3407
#define ID_XSDI_MOUSE_IGNOREPAGEMAGE    3408
#define ID_XSDI_MOUSE_IGNOREXCENTER     3409    // V0.9.7 (2000-12-08) [umoeller]
#define ID_XSDI_MOUSE_AUTOHIDE_CHECK    3410
#define ID_XSDI_MOUSE_AUTOHIDE_TXT1     3411
#define ID_XSDI_MOUSE_AUTOHIDE_SLIDER   3412
#define ID_XSDI_MOUSE_AUTOHIDE_TXT2     3413
#define ID_XSDI_MOUSE_SLIDINGMENU       3414
#define ID_XSDI_MOUSE_MENUDELAY_TXT1    3415
#define ID_XSDI_MOUSE_MENUDELAY_SLIDER  3416
#define ID_XSDI_MOUSE_MENUDELAY_TXT2    3417
#define ID_XSDI_MOUSE_MENUHILITE        3418
#define ID_XSDI_MOUSE_CONDCASCADE       3419    // V0.9.6 (2000-10-27) [umoeller]

#define ID_XSD_MOUSE_CORNERS            3430
#define ID_XSDI_MOUSE_RADIO_TOPLEFT     3431
#define ID_XSDI_MOUSE_RADIO_TOPRIGHT    3432
#define ID_XSDI_MOUSE_RADIO_BOTTOMLEFT  3433
#define ID_XSDI_MOUSE_RADIO_BOTTOMRIGHT 3434
#define ID_XSDI_MOUSE_RADIO_TOP         3435
#define ID_XSDI_MOUSE_RADIO_LEFT        3436
#define ID_XSDI_MOUSE_RADIO_RIGHT       3437
#define ID_XSDI_MOUSE_RADIO_BOTTOM      3438
#define ID_XSDI_MOUSE_INACTIVEOBJ       3439
#define ID_XSDI_MOUSE_SPECIAL_CHECK     3040
#define ID_XSDI_MOUSE_SPECIAL_DROP      3441
#define ID_XSDI_MOUSE_OPEN_CHECK        3442
#define ID_XSDI_MOUSE_OPEN_CNR          3443

#define ID_XSD_MOUSEMAPPINGS2           3450
#define ID_XSDI_MOUSE_CHORDWINLIST      3451
#define ID_XSDI_MOUSE_SYSMENUMB2        3452
#define ID_XSDI_MOUSE_MB3SCROLL         3453
#define ID_XSDI_MOUSE_MB3PIXELS_TXT1    3454
#define ID_XSDI_MOUSE_MB3PIXELS_SLIDER  3455
#define ID_XSDI_MOUSE_MB3PIXELS_TXT2    3456
#define ID_XSDI_MOUSE_MB3LINEWISE       3457
#define ID_XSDI_MOUSE_MB3AMPLIFIED      3458
#define ID_XSDI_MOUSE_MB3AMP_TXT1       3459
#define ID_XSDI_MOUSE_MB3AMP_SLIDER     3460
#define ID_XSDI_MOUSE_MB3AMP_TXT2       3461
#define ID_XSDI_MOUSE_MB3SCROLLREVERSE  3462
#define ID_XSDI_MOUSE_MB3CLK2MB1DBLCLK  3463

/******************************************
 * XCenter (V0.9.7)         >= 3600       *
 ******************************************/

#define ID_CRD_SETTINGS_VIEW            3600
#define ID_CRDI_VIEW_TOPOFSCREEN        3601
#define ID_CRDI_VIEW_BOTTOMOFSCREEN     3602
#define ID_CRDI_VIEW_ALWAYSONTOP        3603
#define ID_CRDI_VIEW_ANIMATE            3604
#define ID_CRDI_VIEW_AUTOHIDE           3605
#define ID_CRDI_VIEW_PRTY_SLIDER        3608
#define ID_CRDI_VIEW_PRTY_TEXT          3609
#define ID_CRDI_VIEW_REDUCEWORKAREA     3610

#define ID_CRD_SETTINGS_VIEW2           3650
#define ID_CRDI_VIEW2_3DBORDER_SLIDER   3651
#define ID_CRDI_VIEW2_3DBORDER_TEXT     3652
#define ID_CRDI_VIEW2_BDRSPACE_SLIDER   3653
#define ID_CRDI_VIEW2_BDRSPACE_TEXT     3654
#define ID_CRDI_VIEW2_WGTSPACE_SLIDER   3655
#define ID_CRDI_VIEW2_WGTSPACE_TEXT     3656
#define ID_CRDI_VIEW2_FLATBUTTONS       3657
#define ID_CRDI_VIEW2_SUNKBORDERS       3658
#define ID_CRDI_VIEW2_ALL3DBORDERS      3659
#define ID_CRDI_VIEW2_SIZINGBARS        3660

#define ID_CRD_WINLISTWGT_SETTINGS      3700
#define ID_CRDI_FILTERS_CURRENTLB       3701
#define ID_CRDI_FILTERS_REMOVE          3702
#define ID_CRDI_FILTERS_NEWCOMBO        3703
#define ID_CRDI_FILTERS_ADD             3704

/******************************************
 * XWPString (V0.9.3)       >= 3800       *
 ******************************************/

#define ID_XSD_XWPSTRING_PAGE           3800
#define ID_XSD_XWPSTRING_STRING_MLE     3801
#define ID_XSD_XWPSTRING_OBJ_CNR        3802
#define ID_XSD_XWPSTRING_OBJ_CLEAR      3803
#define ID_XSD_XWPSTRING_CONFIRM        3804


/******************************************
 *          Shutdown defs                 *
 ******************************************/

#define ID_SDICON                       4100   // shutdown icon

/* dlg templates */
#define ID_SDD_MAIN                     4200
#define ID_SDD_STATUS                   4201
#define ID_SDD_CONFIRM                  4202
#define ID_SDD_CAD                      4203
#define ID_SDD_CLOSEVIO                 4204
#define ID_SDD_CONFIRMWPS               4205
#define ID_SDD_BOOTMGR                  4206

/* dlg items */
#define ID_SDDI_LISTBOX                 4301
#define ID_SDDI_BEGINSHUTDOWN           4302
#define ID_SDDI_CANCELSHUTDOWN          4303
#define ID_SDDI_TEXTSHUTTING            4304
#define ID_SDDI_PROGRESSBAR             4305
#define ID_SDDI_STATUS                  4306
#define ID_SDDI_SKIPAPP                 4307
#define ID_SDDI_PERCENT                 4309
#define ID_SDDI_MESSAGEAGAIN            4310
#define ID_SDDI_VDMAPPTEXT              4311
#define ID_SDDI_WPS_CLOSEWINDOWS        4312
#define ID_SDDI_ICON                    4313
#define ID_SDDI_BOOTMGR                 4314
#define ID_SDDI_WPS_STARTUPFOLDER       4315
#define ID_SDDI_SHUTDOWNONLY            4316
#define ID_SDDI_STANDARDREBOOT          4317
#define ID_SDDI_REBOOTTO                4318
#define ID_SDDI_EMPTYTRASHCAN           4319
#define ID_SDDI_CONFIRM_TEXT            4320  // V0.9.5 (2000-08-10) [umoeller]

/* command defs (used in the Shutdown wnd proc) */
#define ID_SDMI_CLOSEITEM               4400
#define ID_SDMI_UPDATESHUTLIST          4402
#define ID_SDMI_UPDATEPROGRESSBAR       4403
#define ID_SDMI_FLUSHBUFFERS            4404
#define ID_SDMI_CLOSEVIO                4405
#define ID_SDMI_PREPARESAVEWPS          4406
#define ID_SDMI_SAVEWPSITEM             4407
#define ID_SDMI_BEGINCLOSINGITEMS       4408
#define ID_SDMI_CLEANUPANDQUIT          4409

/******************************************
 *          Menu IDs                      *
 ******************************************/

// generic help menu item in various menus
#define ID_XFMI_HELP                    (WPMENUID_USER+1000) // added V0.9.0

// context menu in "WPS Classes" container;
// all the identifiers have changed with V0.9.0
#define ID_XLM_CLASS_SEL                (WPMENUID_USER+1001)
// #define ID_XLM_CLASS_NOSEL              (WPMENUID_USER+1002)
#define ID_XLMI_REGISTER                (WPMENUID_USER+1003)
#define ID_XLMI_DEREGISTER              (WPMENUID_USER+1004)
#define ID_XLMI_REPLACE                 (WPMENUID_USER+1005)
#define ID_XLMI_UNREPLACE               (WPMENUID_USER+1006)
#define ID_XLMI_CREATEOBJECT            (WPMENUID_USER+1007)
#define ID_XLM_METHOD_SEL               (WPMENUID_USER+1010)
#define ID_XLM_METHOD_NOSEL             (WPMENUID_USER+1011)
#define ID_XLM_METHOD_SORT              (WPMENUID_USER+1012)
#define ID_XLMI_METHOD_SORT_INDEX       (WPMENUID_USER+1013)
#define ID_XLMI_METHOD_SORT_NAME        (WPMENUID_USER+1014)
#define ID_XLMI_METHOD_SORT_INTRO       (WPMENUID_USER+1015)
#define ID_XLMI_METHOD_SORT_OVERRIDE    (WPMENUID_USER+1016)
#define ID_XLMI_REFRESH_VIEW            (WPMENUID_USER+1017) // V0.9.6 (2000-11-12) [umoeller]

// "File types" container
#define ID_XSM_FILETYPES_SEL            (WPMENUID_USER+1020)
#define ID_XSM_FILETYPES_NOSEL          (WPMENUID_USER+1021)
#define ID_XSMI_FILETYPES_DELETE        (WPMENUID_USER+1022)
#define ID_XSMI_FILETYPES_NEW           (WPMENUID_USER+1023)
#define ID_XSMI_FILETYPES_PICKUP        (WPMENUID_USER+1024)
#define ID_XSMI_FILETYPES_DROP          (WPMENUID_USER+1025)
#define ID_XSMI_FILETYPES_CANCELDRAG    (WPMENUID_USER+1026)

// "Filters" container on "File types" page
#define ID_XSM_FILEFILTER_SEL           (WPMENUID_USER+1030)
#define ID_XSM_FILEFILTER_NOSEL         (WPMENUID_USER+1031)
#define ID_XSMI_FILEFILTER_DELETE       (WPMENUID_USER+1035)
#define ID_XSMI_FILEFILTER_NEW          (WPMENUID_USER+1036)
#define ID_XSMI_FILEFILTER_IMPORTWPS    (WPMENUID_USER+1037)

// "Associations" container on "File types" page
#define ID_XSM_FILEASSOC_SEL            (WPMENUID_USER+1040)
#define ID_XSM_FILEASSOC_NOSEL          (WPMENUID_USER+1041)
#define ID_XSMI_FILEASSOC_DELETE        (WPMENUID_USER+1042)

// "Objects" menus on XWPSetup "Objects" page
#define ID_XSM_OBJECTS_SYSTEM           (WPMENUID_USER+1050)
#define ID_XSM_OBJECTS_XWORKPLACE       (WPMENUID_USER+1051)

// "Driver" menus on XFldSystem "Drivers" page
#define ID_XSM_DRIVERS_SEL              (WPMENUID_USER+1100)
#define ID_XSM_DRIVERS_NOSEL            (WPMENUID_USER+1101)
#define ID_XSMI_DRIVERS_CMDREFHELP      (WPMENUID_USER+1102)

// "Hotkey" menus on XWPKeyboard "Hotkeys" page
#define ID_XSM_HOTKEYS_SEL              (WPMENUID_USER+1120)
#define ID_XSMI_HOTKEYS_PROPERTIES      (WPMENUID_USER+1121)
#define ID_XSMI_HOTKEYS_OPENFOLDER      (WPMENUID_USER+1122)
#define ID_XSMI_HOTKEYS_REMOVE          (WPMENUID_USER+1123)

// "Function keys" menus on XWPKeyboard "Function keys" page
// V0.9.3 (2000-04-18) [umoeller]
#define ID_XSM_FUNCTIONKEYS_SEL         (WPMENUID_USER+1130)
#define ID_XSMI_FUNCK_EDIT              (WPMENUID_USER+1131)
#define ID_XSMI_FUNCK_DELETE            (WPMENUID_USER+1132)

#define ID_XSM_FUNCTIONKEYS_NOSEL       (WPMENUID_USER+1133)
#define ID_XSMI_FUNCK_NEW               (WPMENUID_USER+1134)

// "Sticky windows" context menus on XWPScreen "PageMage Sticky" page
// V0.9.4 (2000-07-10) [umoeller]
#define ID_XSM_STICKY_NOSEL             (WPMENUID_USER+1140)
#define ID_XSMI_STICKY_NEW              (WPMENUID_USER+1141)

#define ID_XSM_STICKY_SEL               (WPMENUID_USER+1142)
#define ID_XSMI_STICKY_DELETE           (WPMENUID_USER+1143)

/* XCenter main button menu V0.9.7 (2000-11-30) [umoeller] */
#define ID_CRM_XCENTERBUTTON            (WPMENUID_USER+1144)
// menu items; these only need to be unique within the menu
#define ID_CRMI_SEP1                    100
#define ID_CRMI_SEP2                    101
#define ID_CRMI_SUSPEND                 102
#define ID_CRMI_LOGOFF                  103
#define ID_CRMI_RESTARTWPS              104
#define ID_CRMI_SHUTDOWN                105

#define ID_CRM_WIDGET                   200
#define ID_CRMI_PROPERTIES              201
#define ID_CRMI_HELP                    202
#define ID_CRMI_REMOVEWGT               203

#define ID_FNM_SAMPLE                   (WPMENUID_USER+1145)

// The following menu ID's (with _OFS_ in their names)
// are _variable_ menu ID's: XFolder will add the value
// on the "XFolder Internals" page ("menu item id offset")
// to them to avoid ID conflicts. This value is stored
// internally in the GlobalSettings structure.
#define ID_XFMI_OFS_SEPARATOR           (WPMENUID_USER+1)
#define ID_XFMI_OFS_PRODINFO            (WPMENUID_USER+2)
#define ID_XFMI_OFS_REFRESH             (WPMENUID_USER+3)
#define ID_XFMI_OFS_SNAPTOGRID          (WPMENUID_USER+4)
#define ID_XFMI_OFS_OPENPARENT          (WPMENUID_USER+5)
#define ID_XFMI_OFS_OPENPARENTANDCLOSE  (WPMENUID_USER+6)
#define ID_XFMI_OFS_CLOSE               (WPMENUID_USER+7)
#define ID_XFM_OFS_SHUTDOWNMENU         (WPMENUID_USER+9)       // new with V0.9.0
// #define ID_XFMI_OFS_XSHUTDOWN           (WPMENUID_USER+10)      // new with V0.9.0
                                                    // removed V0.9.3 (2000-04-26) [umoeller]
#define ID_XFMI_OFS_OS2_SHUTDOWN        (WPMENUID_USER+10)      // new with V0.9.3 (2000-04-26) [umoeller]
#define ID_XFMI_OFS_RESTARTWPS          (WPMENUID_USER+11)

#define ID_XFMI_OFS_FLDRCONTENT         (WPMENUID_USER+12)
#define ID_XFMI_OFS_DUMMY               (WPMENUID_USER+13)
#define ID_XFMI_OFS_COPYFILENAME_MENU   (WPMENUID_USER+14)      // menu item
#define ID_XFMI_OFS_COPYFILENAME_SHORT  (WPMENUID_USER+15)      // folder hotkeys
#define ID_XFMI_OFS_COPYFILENAME_FULL   (WPMENUID_USER+16)      // folder hotkyes
#define ID_XFMI_OFS_BORED               (WPMENUID_USER+17)
#define ID_XFMI_OFS_SELECTSOME          (WPMENUID_USER+18)
#define ID_XFMI_OFS_PROCESSCONTENT      (WPMENUID_USER+19)

#define ID_XFMI_OFS_CONTEXTMENU         (WPMENUID_USER+20)

#define ID_XFMI_OFS_SORTBYCLASS         (WPMENUID_USER+21)
#define ID_XFMI_OFS_SORTBYEXT           (WPMENUID_USER+27)
#define ID_XFMI_OFS_SORTFOLDERSFIRST    (WPMENUID_USER+28)
#define ID_XFMI_OFS_ALWAYSSORT          (WPMENUID_USER+29)

#define ID_XFM_OFS_ATTRIBUTES           (WPMENUID_USER+31)
#define ID_XFMI_OFS_ATTR_ARCHIVED       (WPMENUID_USER+32)
#define ID_XFMI_OFS_ATTR_SYSTEM         (WPMENUID_USER+33)
#define ID_XFMI_OFS_ATTR_HIDDEN         (WPMENUID_USER+34)
#define ID_XFMI_OFS_ATTR_READONLY       (WPMENUID_USER+35)

#define ID_XFM_OFS_WARP3FLDRVIEW        (WPMENUID_USER+36)
#define ID_XFMI_OFS_SMALLICONS          (WPMENUID_USER+37)
#define ID_XFMI_OFS_FLOWED              (WPMENUID_USER+38)
#define ID_XFMI_OFS_NONFLOWED           (WPMENUID_USER+39)
#define ID_XFMI_OFS_NOGRID              (WPMENUID_USER+40)

#define ID_XFMI_OFS_WARP4MENUBAR        (WPMENUID_USER+41)  // added V0.9.0
#define ID_XFMI_OFS_SHOWSTATUSBAR       (WPMENUID_USER+42)

// new view item in "Open" submenu... this is used for XWPClassList, Partitions,
// XCenter, XWPFontObject, ...
#define ID_XFMI_OFS_XWPVIEW             (WPMENUID_USER+43)

// Trash can (XWPTrashCan, XWPTrashObject, new with V0.9.0)
#define ID_XFMI_OFS_TRASHEMPTY          (WPMENUID_USER+44)
#define ID_XFMI_OFS_TRASHRESTORE        (WPMENUID_USER+45)
#define ID_XFMI_OFS_TRASHDESTROY        (WPMENUID_USER+46)

// "Deinstall" menu item in XWPFontObject V0.9.7 (2001-01-13) [umoeller]
#define ID_XFMI_OFS_FONT_DEINSTALL      (WPMENUID_USER+47)

// "Default document" item in WPFolder "Open" and WPDataFile main context menu V0.9.4 (2000-06-09) [umoeller]
#define ID_XFMI_OFS_FDRDEFAULTDOC       (WPMENUID_USER+48)

// "Logoff" menu item in XFldDesktop V0.9.5 (2000-08-10) [umoeller]
#define ID_XFMI_OFS_LOGOFF              (WPMENUID_USER+50)

// this is the value used for variable menu items, i.e.
// those inserted according to the config folder and by
// the "folder content" functions; XFolder will use this
// value (i.e. WPMENUID_USER + "menu item id offset"
// + FIRST_VARIABLE) and increment it until 0x8000 is reached
#define FIRST_VARIABLE                  60      // raised V0.9.4 (2000-06-10) [umoeller]
#define ID_XFMI_OFS_VARIABLE            (WPMENUID_USER+FIRST_VARIABLE)

/******************************************
 *          String IDs                    *
 ******************************************/

// XFolder uses the following ID's for language-
// dependent strings. These correspond to the
// string definitions in the NLS .RC file.
// Note that XFolder also uses a message file
// (.MSG) for messages which might turn out to
// be longer than 256 characters.

// These IDs should not have "gaps" in the
// numbers, because OS/2 loads string resources
// in blocks of 16 strings, which does not
// work if the IDs don't have following numbers.

// Note: All string IDs have been raised with V0.9.0.

#define ID_XSS_MAIN               5000
#define ID_XSSI_NOTDEFINED        5001
#define ID_XSSI_PRODUCTINFO       5002
#define ID_XSSI_REFRESHNOW        5003
#define ID_XSSI_SNAPTOGRID        5004
#define ID_XSSI_DLLLANGUAGE       5005
#define ID_XSSI_XFOLDERVERSION    5006

#define ID_XSSI_FLDRCONTENT       5007
#define ID_XSSI_COPYFILENAME      5008
#define ID_XSSI_BORED             5009
#define ID_XSSI_FLDREMPTY         5010
#define ID_XSSI_SELECTSOME        5011

// startup/shutdown folder context menu
#define ID_XSSI_PROCESSCONTENT    5012

#define ID_XFSI_QUICKSTATUS       5013

#define ID_XSSI_POPULATING        5014
#define ID_XSSI_SBTEXTNONESEL     5015
#define ID_XSSI_SBTEXTMULTISEL    5016
#define ID_XSSI_SBTEXTWPDATAFILE  5017
#define ID_XSSI_SBTEXTWPDISK      5018
#define ID_XSSI_SBTEXTWPPROGRAM   5019
#define ID_XSSI_SBTEXTWPOBJECT    5020

#define ID_XSSI_NLS_AUTHOR        5021      // new with V0.9.0
// #define ID_XSSI_KERNEL_BUILD      5022      // new with V0.9.0; for /main/xfldr.rc
        // removed V0.9.7 (2000-12-14) [umoeller]

// folder hotkeys: key descriptions
#define ID_XSSI_KEY_CTRL             5201
#define ID_XSSI_KEY_Alt              5202
#define ID_XSSI_KEY_SHIFT            5203

#define ID_XSSI_KEY_BACKSPACE        5204
#define ID_XSSI_KEY_TAB              5205
#define ID_XSSI_KEY_BACKTABTAB       5206
#define ID_XSSI_KEY_ENTER            5207
#define ID_XSSI_KEY_ESC              5208
#define ID_XSSI_KEY_SPACE            5209
#define ID_XSSI_KEY_PAGEUP           5210
#define ID_XSSI_KEY_PAGEDOWN         5211
#define ID_XSSI_KEY_END              5212
#define ID_XSSI_KEY_HOME             5213
#define ID_XSSI_KEY_LEFT             5214
#define ID_XSSI_KEY_UP               5215
#define ID_XSSI_KEY_RIGHT            5216
#define ID_XSSI_KEY_DOWN             5217
#define ID_XSSI_KEY_PRINTSCRN        5218
#define ID_XSSI_KEY_INSERT           5219
#define ID_XSSI_KEY_DELETE           5220
#define ID_XSSI_KEY_SCRLLOCK         5221
#define ID_XSSI_KEY_NUMLOCK          5222

#define ID_XSSI_KEY_WINLEFT          5223
#define ID_XSSI_KEY_WINRIGHT         5224
#define ID_XSSI_KEY_WINMENU          5225

// folder hotkeys: action descriptions
#define ID_XSSI_LB_REFRESHNOW           5500
#define ID_XSSI_LB_SNAPTOGRID           5501
#define ID_XSSI_LB_SELECTALL            5502
#define ID_XSSI_LB_OPENPARENTFOLDER     5503

#define ID_XSSI_LB_OPENSETTINGSNOTEBOOK 5504
#define ID_XSSI_LB_OPENNEWDETAILSVIEW   5505
#define ID_XSSI_LB_OPENNEWICONVIEW      5506
#define ID_XSSI_LB_DESELECTALL          5507
#define ID_XSSI_LB_OPENNEWTREEVIEW      5508

#define ID_XSSI_LB_FIND                 5509

#define ID_XSSI_LB_PICKUP               5510
#define ID_XSSI_LB_PICKUPCANCELDRAG     5511

#define ID_XSSI_LB_SORTBYNAME           5512
#define ID_XSSI_LB_SORTBYSIZE           5513
#define ID_XSSI_LB_SORTBYTYPE           5514
#define ID_XSSI_LB_SORTBYREALNAME       5515
#define ID_XSSI_LB_SORTBYWRITEDATE      5516
#define ID_XSSI_LB_SORTBYACCESSDATE     5517
#define ID_XSSI_LB_SORTBYCREATIONDATE   5518

#define ID_XSSI_LB_SWITCHTOICONVIEW     5519
#define ID_XSSI_LB_SWITCHTODETAILSVIEW  5520
#define ID_XSSI_LB_SWITCHTOTREEVIEW     5521

#define ID_XSSI_LB_ARRANGEDEFAULT       5522
#define ID_XSSI_LB_ARRANGEFROMTOP       5523
#define ID_XSSI_LB_ARRANGEFROMLEFT      5524
#define ID_XSSI_LB_ARRANGEFROMRIGHT     5525
#define ID_XSSI_LB_ARRANGEFROMBOTTOM    5526
#define ID_XSSI_LB_ARRANGEPERIMETER     5527
#define ID_XSSI_LB_ARRANGEHORIZONTALLY  5528
#define ID_XSSI_LB_ARRANGEVERTICALLY    5529

#define ID_XSSI_LB_INSERT               5530

#define ID_XSSI_LB_SORTBYEXTENSION      5531
#define ID_XSSI_LB_SORTFOLDERSFIRST     5532
#define ID_XSSI_LB_SORTBYCLASS          5533
#define ID_XSSI_LB_OPENPARENTFOLDERANDCLOSE     5534
#define ID_XSSI_LB_SELECTSOME           5535

#define ID_XSSI_LB_CLOSEWINDOW          5536

#define ID_XSSI_LB_CONTEXTMENU          5537
#define ID_XSSI_LB_TASKLIST             5538

#define ID_XSSI_LB_COPYFILENAME_SHORT   5539
#define ID_XSSI_LB_COPYFILENAME_FULL    5540

// FIRST and LAST are used be the notebook func to
// calculate corresponding items
#define ID_XSSI_LB_FIRST                5500
#define ID_XSSI_LB_LAST                 5540

// shutdown strings
#define ID_SDSI_FLUSHING                5600
#define ID_SDSI_CAD                     5601
#define ID_SDSI_REBOOTING               5602
#define ID_SDSI_CLOSING                 5603
#define ID_SDSI_SHUTDOWN                5604
#define ID_SDSI_RESTARTWPS              5605
#define ID_SDSI_RESTARTINGWPS           5606
#define ID_SDSI_SAVINGDESKTOP           5607
#define ID_SDSI_SAVINGPROFILES          5608
#define ID_SDSI_STARTING                5609
#define ID_SDSI_DEFAULT                 5610

// settings page titles (for notebook tabs)
#define ID_XSSI_1GENERIC                5620
#define ID_XSSI_2REMOVEITEMS            5621
#define ID_XSSI_25ADDITEMS              5622
#define ID_XSSI_26CONFIGITEMS           5623
#define ID_XSSI_27STATUSBAR             5624
#define ID_XSSI_3SNAPTOGRID             5625
#define ID_XSSI_4ACCELERATORS           5626
#define ID_XSSI_5INTERNALS              5627
#define ID_XSSI_FILEOPS                 5628
#define ID_XSSI_SORT                    5629
#define ID_XSSI_INTERNALS               5630
#define ID_XSSI_WPSCLASSES              5631
#define ID_XSSI_XWPSTATUS               5632
#define ID_XSSI_FEATURES                5633
#define ID_XSSI_PARANOIA                5634
#define ID_XSSI_OBJECTS                 5635
#define ID_XSSI_FILEPAGE                5636
#define ID_XSSI_DETAILSPAGE             5637
#define ID_XSSI_XSHUTDOWNPAGE           5638
#define ID_XSSI_STARTUPPAGE             5639
#define ID_XSSI_DTPMENUPAGE             5640
#define ID_XSSI_FILETYPESPAGE           5641
#define ID_XSSI_SOUNDSPAGE              5642
#define ID_XSSI_VIEWPAGE                5643
#define ID_XSSI_ARCHIVESPAGE            5644
#define ID_XSSI_PGMFILE_MODULE          5645
#define ID_XSSI_OBJECTHOTKEYSPAGE       5646
#define ID_XSSI_FUNCTIONKEYSPAGE        5647
#define ID_XSSI_MOUSEHOOKPAGE           5648
#define ID_XSSI_MAPPINGSPAGE            5649

// sort criteria
#define ID_XSSI_SV_NAME                 5650
#define ID_XSSI_SV_TYPE                 5651
#define ID_XSSI_SV_CLASS                5652
#define ID_XSSI_SV_REALNAME             5653
#define ID_XSSI_SV_SIZE                 5654
#define ID_XSSI_SV_WRITEDATE            5655
#define ID_XSSI_SV_ACCESSDATE           5656
#define ID_XSSI_SV_CREATIONDATE         5657
#define ID_XSSI_SV_EXT                  5658
#define ID_XSSI_SV_FOLDERSFIRST         5659

#define ID_XSSI_SV_ALWAYSSORT           5660

// message box strings
// #define ID_XSSI_DLG_CONFIRMCONFIGSYS1   4000
// #define ID_XSSI_DLG_CONFIRMCONFIGSYS2   4001

// "Yes", "no", etc.
#define ID_XSSI_DLG_YES                 5800
#define ID_XSSI_DLG_YES2ALL             5801
#define ID_XSSI_DLG_NO                  5802
#define ID_XSSI_DLG_OK                  5803
#define ID_XSSI_DLG_CANCEL              5804
#define ID_XSSI_DLG_ABORT               5805
#define ID_XSSI_DLG_RETRY               5806
#define ID_XSSI_DLG_IGNORE              5807

// "status bars" page
#define ID_XSSI_SB_CLASSMNEMONICS       5808
#define ID_XSSI_SB_CLASSNOTSUPPORTED    5809

// "WPS Classes" page
#define ID_XSSI_WPSCLASSLOADED          5810
#define ID_XSSI_WPSCLASSLOADINGFAILED   5811
#define ID_XSSI_WPSCLASSREPLACEDBY      5812
#define ID_XSSI_WPSCLASSORPHANS         5813
#define ID_XSSI_WPSCLASSORPHANSINFO     5814

// CONFIG.SYS pages
#define ID_XSSI_SCHEDULER               5815
#define ID_XSSI_MEMORY                  5816
#define ID_XSSI_ERRORS                  5817
#define ID_XSSI_WPS                     5818
#define ID_XSSI_SYSPATHS                5819    // new with V0.9.0
#define ID_XSSI_DRIVERS                 5820    // new with V0.9.0
#define ID_XSSI_DRIVERCATEGORIES        5821    // new with V0.9.0

// settings submenu strings
#define ID_XFSI_SETTINGS                5831
#define ID_XFSI_SETTINGSNOTEBOOK        5832
#define ID_XFSI_ATTRIBUTES              5833
#define ID_XFSI_ATTR_ARCHIVE            5834
#define ID_XFSI_ATTR_SYSTEM             5835
#define ID_XFSI_ATTR_HIDDEN             5836
#define ID_XFSI_ATTR_READONLY           5837

#define ID_XFSI_FLDRSETTINGS            5838
#define ID_XFSI_SMALLICONS              5839
#define ID_XFSI_FLOWED                  5840
#define ID_XFSI_NONFLOWED               5841
#define ID_XFSI_NOGRID                  5842

#define ID_XFSI_WARP4MENUBAR            5843
#define ID_XFSI_SHOWSTATUSBAR           5844

// "WPS Class List" (XWPClassList, new with V0.9.0)
#define ID_XFSI_OPENCLASSLIST           5845
#define ID_XFSI_XWPCLASSLIST            5846
#define ID_XFSI_REGISTERCLASS           5847

// XWPSound (new with V0.9.0)
#define ID_XSSI_SOUNDSCHEMENONE         5849
// "System paths" page
#define ID_XSSI_ITEMSSELECTED           5850    // new with V0.9.0

// Trash can (XWPTrashCan, XWPTrashObject, new with V0.9.0)
#define ID_XTSI_TRASHEMPTY              5851
#define ID_XTSI_TRASHRESTORE            5852
#define ID_XTSI_TRASHDESTROY            5853

#define ID_XTSI_TRASHCAN                5854
#define ID_XTSI_TRASHOBJECT             5855

#define ID_XTSI_TRASHSETTINGSPAGE       5856
#define ID_XTSI_TRASHDRIVESPAGE         5857

#define ID_XTSI_ORIGFOLDER              5858
#define ID_XTSI_DELDATE                 5859
#define ID_XTSI_DELTIME                 5860
#define ID_XTSI_SIZE                    5861
#define ID_XTSI_ORIGCLASS               5862

#define ID_XTSI_STB_POPULATING          5863
#define ID_XTSI_STB_OBJCOUNT            5864

// Details view columns on XWPKeyboard "Hotkeys" page; V0.9.1 (99-12-03)
#define ID_XSSI_HOTKEY_TITLE            5865
#define ID_XSSI_HOTKEY_FOLDER           5866
#define ID_XSSI_HOTKEY_HANDLE           5867
#define ID_XSSI_HOTKEY_HOTKEY           5868

// Method info columns for XWPClassList; V0.9.1 (99-12-03)
#define ID_XSSI_CLSLIST_INDEX           5869
#define ID_XSSI_CLSLIST_METHOD          5870
#define ID_XSSI_CLSLIST_ADDRESS         5871
#define ID_XSSI_CLSLIST_CLASS           5872
#define ID_XSSI_CLSLIST_OVERRIDDENBY    5873

// "Special functions" on XWPMouse "Movement" page
#define ID_XSSI_SPECIAL_WINDOWLIST      5903
#define ID_XSSI_SPECIAL_DESKTOPPOPUP    5904

// default title of XWPScreen class V0.9.2 (2000-02-23) [umoeller]
#define ID_XSSI_XWPSCREENTITLE          5905

// "Partitions" item in WPDrives "open" menu V0.9.2 (2000-02-29) [umoeller]
#define ID_XSSI_OPENPARTITIONS          5906

// "Syslevel" page title in "OS/2 kernel" V0.9.3 (2000-04-01) [umoeller]
#define ID_XSSI_SYSLEVELPAGE            5907

#define ID_XTSI_CALCULATING             5908

#define ID_MMSI_DEVICETYPE              5909
#define ID_MMSI_DEVICEINDEX             5910
#define ID_MMSI_DEVICEINFO              5911

#define ID_MMSI_TYPE_IMAGE              5912
#define ID_MMSI_TYPE_AUDIO              5913
#define ID_MMSI_TYPE_MIDI               5914
#define ID_MMSI_TYPE_COMPOUND           5915
#define ID_MMSI_TYPE_OTHER              5916
#define ID_MMSI_TYPE_UNKNOWN            5917
#define ID_MMSI_TYPE_VIDEO              5918
#define ID_MMSI_TYPE_ANIMATION          5919
#define ID_MMSI_TYPE_MOVIE              5920

#define ID_MMSI_TYPE_STORAGE            5921
#define ID_MMSI_TYPE_FILE               5922
#define ID_MMSI_TYPE_DATA               5923

#define ID_MMSI_DEVTYPE_VIDEOTAPE       5924
#define ID_MMSI_DEVTYPE_VIDEODISC       5925
#define ID_MMSI_DEVTYPE_CD_AUDIO        5926
#define ID_MMSI_DEVTYPE_DAT             5927
#define ID_MMSI_DEVTYPE_AUDIO_TAPE      5928
#define ID_MMSI_DEVTYPE_OTHER           5929
#define ID_MMSI_DEVTYPE_WAVEFORM_AUDIO  5930
#define ID_MMSI_DEVTYPE_SEQUENCER       5931
#define ID_MMSI_DEVTYPE_AUDIO_AMPMIX    5932
#define ID_MMSI_DEVTYPE_OVERLAY         5933
#define ID_MMSI_DEVTYPE_ANIMATION       5934
#define ID_MMSI_DEVTYPE_DIGITAL_VIDEO   5935
#define ID_MMSI_DEVTYPE_SPEAKER         5936
#define ID_MMSI_DEVTYPE_HEADPHONE       5937
#define ID_MMSI_DEVTYPE_MICROPHONE      5938
#define ID_MMSI_DEVTYPE_MONITOR         5939
#define ID_MMSI_DEVTYPE_CDXA            5940
#define ID_MMSI_DEVTYPE_FILTER          5941
#define ID_MMSI_DEVTYPE_TTS             5942

#define ID_MMSI_COLMN_FOURCC            5943
#define ID_MMSI_COLMN_NAME              5944
#define ID_MMSI_COLMN_IOPROC_TYPE       5945
#define ID_MMSI_COLMN_MEDIA_TYPE        5946
#define ID_MMSI_COLMN_EXTENSION         5947
#define ID_MMSI_COLMN_DLL               5948
#define ID_MMSI_COLMN_PROCEDURE         5949

#define ID_MMSI_PAGETITLE_DEVICES       5950
#define ID_MMSI_PAGETITLE_IOPROCS       5951
#define ID_MMSI_PAGETITLE_CODECS        5952

#define ID_XSSI_PAGETITLE_PAGEMAGE      5953

#define ID_XSSI_XWPSTRING_PAGE          5954
#define ID_XSSI_XWPSTRING_OPENMENU      5955

#define ID_XSSI_COLMN_SYSL_COMPONENT    5956
#define ID_XSSI_COLMN_SYSL_FILE         5957
#define ID_XSSI_COLMN_SYSL_VERSION      5958
#define ID_XSSI_COLMN_SYSL_LEVEL        5959
#define ID_XSSI_COLMN_SYSL_PREVIOUS     5960

#define ID_XSSI_DRIVERS_VERSION         5961
#define ID_XSSI_DRIVERS_VENDOR          5962

#define ID_XSSI_FUNCKEY_DESCRIPTION     5963
#define ID_XSSI_FUNCKEY_SCANCODE        5964
#define ID_XSSI_FUNCKEY_MODIFIER        5965

// default documents V0.9.4 (2000-06-09) [umoeller]
#define ID_XSSI_DATAFILEDEFAULTDOC      5966
#define ID_XSSI_FDRDEFAULTDOC           5967

// XCenter V0.9.4 (2000-06-10) [umoeller]
#define ID_XSSI_XCENTERPAGE1            5968

// file operations V0.9.4 (2000-07-27) [umoeller]
#define ID_XSSI_FOPS_MOVE2TRASHCAN      5969
#define ID_XSSI_FOPS_RESTOREFROMTRASHCAN 5970
#define ID_XSSI_FOPS_TRUEDELETE         5971
#define ID_XSSI_FOPS_EMPTYINGTRASHCAN   5972

#define ID_XSSI_ICONPAGE                5973

// XShutdown INI save strings V0.9.5 (2000-08-16) [umoeller]
#define ID_XSSI_XSD_SAVEINIS_NEW        5974
#define ID_XSSI_XSD_SAVEINIS_OLD        5975
#define ID_XSSI_XSD_SAVEINIS_NONE       5976

// logoff V0.9.5 (2000-09-28) [umoeller]
#define ID_XSSI_XSD_LOGOFF              5977
#define ID_XSSI_XSD_CONFIRMLOGOFFMSG    5978

// "bytes" strings for status bars V0.9.6 (2000-11-23) [umoeller]
#define ID_XSSI_BYTE                    5979
#define ID_XSSI_BYTES                   5980

// title of "Resources" page V0.9.7 (2000-12-20) [umoeller]
#define ID_XSSI_PGMFILE_RESOURCES       5981

/******************************************
 *  Features on XWPSetup "Features" page  *
 ******************************************/

// The following identifiers are used BOTH
// for loading string resources and for
// identifying check box container record
// cores (helpers/cnrh.c).

#define ID_XCSI_GENERALFEATURES         5999
#define ID_XCSI_REPLACEICONS            6000
#define ID_XCSI_RESIZESETTINGSPAGES     6001
#define ID_XCSI_ADDOBJECTPAGE           6002
#define ID_XCSI_REPLACEFILEPAGE         6003
#define ID_XCSI_XSYSTEMSOUNDS           6004

#define ID_XCSI_FOLDERFEATURES          6010
#define ID_XCSI_ENABLESTATUSBARS        6011
#define ID_XCSI_ENABLESNAP2GRID         6012
#define ID_XCSI_ENABLEFOLDERHOTKEYS     6013
#define ID_XCSI_EXTFOLDERSORT           6014

#define ID_XCSI_STARTSHUTFEATURES       6020
#define ID_XCSI_ARCHIVING               6021
#define ID_XCSI_RESTARTWPS              6022
#define ID_XCSI_XSHUTDOWN               6023

#define ID_XCSI_FILEOPERATIONS          6040
#define ID_XCSI_EXTASSOCS               6041
#define ID_XCSI_CLEANUPINIS             6042
#define ID_XCSI_REPLFILEEXISTS          6043
#define ID_XCSI_REPLDRIVENOTREADY       6044
#define ID_XCSI_XWPTRASHCAN             6045
#define ID_XCSI_REPLACEDELETE           6046
#define ID_XCSI_REPLHANDLES             6047

#define ID_XCSI_MOUSEKEYBOARDFEATURES   6050
#define ID_XCSI_XWPHOOK                 6051
#define ID_XCSI_ANIMOUSE                6052
#define ID_XCSI_GLOBALHOTKEYS           6053
#define ID_XCSI_PAGEMAGE                6054

/******************************************
 *          Treesize                      *
 ******************************************/

#define ID_TSD_MAIN                     10001
#define ID_TSDI_CNR                     10002
#define ID_TSDI_TEXT1                   10003
#define ID_TSDI_TEXT2                   10004
#define ID_TSDI_ICON                    10005
#define DID_CLEAR                       10006

#define ID_TSD_PRODINFO                 10007

#define ID_TSM_CONTEXT                  10008
#define ID_TSM_SORT                     10009
#define ID_TSMI_SORTBYNAME              10010
#define ID_TSMI_SORTBYSIZE              10011
#define ID_TSMI_SORTBYEASIZE            10012
#define ID_TSMI_SORTBYFILESCOUNT        10013
#define ID_TSMI_COLLECTEAS              10014
#define ID_TSMI_LOWPRTY                 10015
#define ID_TSM_SIZES                    10016
#define ID_TSMI_SIZE_BYTES              10017
#define ID_TSMI_SIZE_KBYTES             10018
#define ID_TSMI_SIZE_MBYTES             10019

#define ID_TSMI_PRODINFO                10020

#define ID_TS_ICON                      10021

/******************************************
 *          NetscapeDDE                   *
 ******************************************/

#define ID_NDD_EXPLAIN                  10100
#define ID_NDD_QUERYSTART               10101
#define ID_NDD_CONTACTING               10102
#define ID_NDD_STARTING                 10103

#define ID_ND_ICON                      10110

/********************************************
 * Animated mouse pointers       >= 0x6000  *
 ********************************************/

// --- diverse res ids

#define IDTAB_NBPAGE                      0x60F0
#define IDDLG_GB_NBPAGE                   0x60F1

#define IDDLG_NBANIMATION                 0x60F2
#define IDDLG_NBHIDE                      0x60F3
#define IDDLG_NBDRAGDROP                  0x60F4
#define IDDLG_NBINIT                      0x60F5

#define IDTAB_NBANIMATION                 0x60F6
#define IDTAB_NBHIDE                      0x60F7
#define IDTAB_NBDRAGDROP                  0x60F8
#define IDTAB_NBINIT                      0x60F9

// --- pushbuttons

#define IDDLG_PB_OK                       0x6100
#define IDDLG_PB_CANCEL                   0x6101
#define IDDLG_PB_EDIT                     0x6102
#define IDDLG_PB_FIND                     0x6103
#define IDDLG_PB_LOAD                     0x6104
#define IDDLG_PB_UNDO                     0x6105
#define IDDLG_PB_DEFAULT                  0x6106
#define IDDLG_PB_HELP                     0x6107
#define IDDLG_PB_CLOSE                    0x6108
#define IDDLG_PB_RETURN                   0x6109


// --- notebook page dialog

#define IDDLG_DLG_ANIMATEDWAITPOINTER_230 0x6110
#define IDDLG_DLG_ANIMATEDWAITPOINTER     0x6111
#define IDDLG_CN_POINTERSET               0x6101


// --- about dialog

#define IDDLG_DLG_ABOUT                   0x6120


// --- settings dialog

#define IDDLG_DLG_CNRSETTINGS_230         0x6130
#define IDDLG_DLG_CNRSETTINGS             0x6131
#define IDDLG_EF_ANIMATIONPATH            0x6132
#define IDDLG_SB_FRAMELENGTH              0x6133
#define IDDLG_CB_USEFORALL                0x6134
#define IDDLG_CB_ANIMATEONLOAD            0x6135
#define IDDLG_EF_DRAGPTRTYPE              0x6136
#define IDDLG_EF_DRAGSETTYPE              0x6137
#define IDDLG_CB_HIDEPOINTER              0x6138
#define IDDLG_SB_HIDEPOINTERDELAY         0x6139
#define IDDLG_SB_INITDELAY                0x613A


// --- load set dialog

#define IDDLG_DLG_LOADSET                 0x6140
#define IDDLG_CN_FOUNDSETS                0x6141
#define IDDLG_CO_FILTER                   0x6142


// --- animation page for file classes properties notebook
#define IDDLG_DLG_ANIMATIONPAGE1_230      0x6150
#define IDDLG_DLG_ANIMATIONPAGE1          0x6151
#define IDDLG_DLG_ANIMATIONPAGE2_230      0x6152
#define IDDLG_DLG_ANIMATIONPAGE2          0x6153
#define IDDLG_DLG_ANIMATIONPAGE3_230      0x6154
#define IDDLG_DLG_ANIMATIONPAGE3          0x6155

#define IDTAB_NBGENERAL_230               0x615E
#define IDTAB_NBGENERAL                   0x615F

#define IDDLG_EF_INFONAME                 0x6160
#define IDDLG_EF_INFOARTIST               0x6161
#define IDDLG_EF_FRAMES                   0x6162
#define IDDLG_EF_PHYSFRAMES               0x6163
#define IDDLG_EF_DEFTIMEOUT               0x6164

// --- icon page for file classes properties notebook
#define IDDLG_DLG_ICONPAGE_230            0x6160
#define IDDLG_DLG_ICONPAGE                0x6161

#define IDDLG_ME_TITLE                    0x6162
#define IDDLG_CB_TEMPLATE                 0x6161
#define IDDLG_CB_LOCKPOS                  0x6163
#define IDDLG_GB_CONTENTS                 0x6164
#define IDDLG_IC_CONTENTS                 0x6165

// fine IDTAB_NBANIMATION                 bereits definiert


// --- animation editor dialog

#define IDDLG_DLG_EDITANIMATION           0x6180
#define IDDLG_GB_SELECTEDFRAME            0x6181
#define IDDLG_CN_FRAMESET                 0x6182
#define IDDLG_ST_SHOWFOR                  0x6183
//      IDDLG_SB_FRAMELENGTH              ------
#define IDDLG_ST_UNIT                     0x6184
#define IDDLG_GB_PREVIEW                  0x6185
#define IDDLG_ST_PREVIEW                  0x6186
//      IDDLG_EF_INFONAME
//      IDDLG_EF_INFOARTIST

#define IDDLG_BMP_STOP                    0x6187
#define IDDLG_BMP_STOPP                   0x6188
#define IDDLG_BMP_START                   0x6189
#define IDDLG_BMP_STARTP                  0x618A

// --- animation editor menu

#define IDMEN_AE_FILE                     0x6300
#define IDMEN_AE_FILE_NEW                 0x6301
#define IDMEN_AE_FILE_OPEN                0x6302
#define IDMEN_AE_FILE_SAVE                0x6303
#define IDMEN_AE_FILE_SAVEAS              0x6304
#define IDMEN_AE_FILE_IMPORT              0x6305
#define IDMEN_AE_FILE_EXIT                0x6306

#define IDMEN_AE_EDIT                     0x6310
#define IDMEN_AE_EDIT_COPY                0x6311
#define IDMEN_AE_EDIT_CUT                 0x6312
#define IDMEN_AE_EDIT_PASTE               0x6313
#define IDMEN_AE_EDIT_DELETE              0x6314
#define IDMEN_AE_EDIT_SELECTALL           0x6315
#define IDMEN_AE_EDIT_DESELECTALL         0x6316

#define IDMEN_AE_PALETTE                  0x6320
#define IDMEN_AE_PALETTE_OPEN             0x6321
#define IDMEN_AE_PALETTE_SAVEAS           0x6322
#define IDMEN_AE_PALETTE_COPY             0x6323
#define IDMEN_AE_PALETTE_PASTE            0x6324

#define IDMEN_AE_OPTION                   0x6331
#define IDMEN_AE_OPTION_UNIT              0x6332
#define IDMEN_AE_OPTION_UNIT_MS           0x6333
#define IDMEN_AE_OPTION_UNIT_JIF          0x6334

#define IDMEN_AE_HELP                     0x6340
#define IDMEN_AE_HELP_INDEX               0x6341
#define IDMEN_AE_HELP_GENERAL             0x6342
#define IDMEN_AE_HELP_USING               0x6343
#define IDMEN_AE_HELP_KEYS                0x6344
#define IDMEN_AE_HELP_ABOUT               0x6345

// --- animation editor item menu

#define IDMEN_EDITITEM                    0x6400

// --- res ids for container item popup menu

#define IDMEN_ITEM                        0x6200
#define IDMEN_ITEM_HELP                   0x6201
#define IDMEN_ITEM_HELP_INDEX             0x6202
#define IDMEN_ITEM_HELP_GENERAL           0x6203
#define IDMEN_ITEM_HELP_USING             0x6204
#define IDMEN_ITEM_HELP_KEYS              0x6205
#define IDMEN_ITEM_HELP_ABOUT             0x6206
#define IDMEN_ITEM_EDIT                   0x6207
#define IDMEN_ITEM_FIND                   0x6208
#define IDMEN_ITEM_SAVEAS                 0x6209
#define IDMEN_ITEM_DEFAULT                0x620A
#define IDMEN_ITEM_ANIMATE                0x620B


// --- res ids for container popup menu

#define IDMEN_FOLDER                      0x6210
#define IDMEN_FOLDER_SETTINGS_230         0x6211
#define IDMEN_FOLDER_SETTINGS             0x6212
#define IDMEN_FOLDER_SETTINGS_NEW         0x6712
#define IDMEN_FOLDER_VIEW                 0x6213
#define IDMEN_FOLDER_VIEW_ICON            0x6214
#define IDMEN_FOLDER_VIEW_DETAIL          0x6215
#define IDMEN_FOLDER_HELP                 0x6216
#define IDMEN_FOLDER_HELP_INDEX           0x6217
#define IDMEN_FOLDER_HELP_GENERAL         0x6218
#define IDMEN_FOLDER_HELP_USING           0x6219
#define IDMEN_FOLDER_HELP_KEYS            0x621A
#define IDMEN_FOLDER_HELP_ABOUT           0x621B
#define IDMEN_FOLDER_FIND                 0x621C
#define IDMEN_FOLDER_SAVEAS               0x621D
#define IDMEN_FOLDER_DEFAULT              0x621E
#define IDMEN_FOLDER_DEMO                 0x621F
#define IDMEN_FOLDER_LEFTHANDED           0x6220
#define IDMEN_FOLDER_HIDEPOINTER          0x6221
#define IDMEN_FOLDER_BLACKWHITE           0x6222
#define IDMEN_FOLDER_ANIMATE              0x6223

// --- res id for strings

#define IDSTR_VERSION                     0x6000
#define IDSTR_FILETYPE_DEFAULT            0x6001
#define IDSTR_FILETYPE_POINTER            0x6002
#define IDSTR_FILETYPE_POINTERSET         0x6003
#define IDSTR_FILETYPE_CURSOR             0x6004
#define IDSTR_FILETYPE_CURSORSET          0x6005
#define IDSTR_FILETYPE_ANIMOUSE           0x6006
#define IDSTR_FILETYPE_ANIMOUSESET        0x6007
#define IDSTR_FILETYPE_WINANIMATION       0x6008
#define IDSTR_FILETYPE_WINANIMATIONSET    0x6009
#define IDSTR_FILETYPE_ANIMATIONSETDIR    0x600A
#define IDSTR_FILETYPE_ALL                0x600B

#define IDSTR_POINTER_ARROW               0x6010
#define IDSTR_POINTER_TEXT                0x6011
#define IDSTR_POINTER_WAIT                0x6012
#define IDSTR_POINTER_SIZENWSE            0x6013
#define IDSTR_POINTER_SIZEWE              0x6014
#define IDSTR_POINTER_MOVE                0x6015
#define IDSTR_POINTER_SIZENESW            0x6016
#define IDSTR_POINTER_SIZENS              0x6017
#define IDSTR_POINTER_ILLEGAL             0x6018

#define IDSTR_TITLE_ICON                  0x6020
#define IDSTR_TITLE_NAME                  0x6021
#define IDSTR_TITLE_STATUS                0x6022
#define IDSTR_TITLE_ANIMATIONTYPE         0x6023
#define IDSTR_TITLE_POINTER               0x6024
#define IDSTR_TITLE_ANIMATIONNAME         0x6025
#define IDSTR_TITLE_FRAMERATE             0x6026
#define IDSTR_TITLE_INFONAME              0x6027
#define IDSTR_TITLE_INFOARTIST            0x6028
#define IDSTR_STATUS_ON                   0x6029
#define IDSTR_STATUS_OFF                  0x602A

#define IDSTR_TITLE_IMAGE                 0x6030
#define IDSTR_SELECTED_NONE               0x6031
#define IDSTR_SELECTED_FRAME              0x6032
#define IDSTR_SELECTED_FRAMES             0x6033
#define IDSTR_UNIT_MILLISECONDS           0x6034
#define IDSTR_UNIT_JIFFIES                0x6035


// --- res id for messages

#define IDMSG_TITLE_ERROR                 0x6F01
#define IDMSG_ANIMATIONPATH_NOT_FOUND     0x6F02
#define IDMSG_TITLENOTFOUND               0x6F03
#define IDMSG_MSGNOTFOUND                 0x6F04

// online Help Id for DisplayHelp
/* #define IDPNL_MAIN                     1
#define IDPNL_USAGE_NBPAGE             2
#define IDPNL_USAGE_NBPAGE_CNRSETTINGS 3
#define IDPNL_USAGE_NBPAGE_CNRLOADSET  4 */

// --- res ids for container item popup menu

// define submenu ids for submenu of every resource WPS class
#define IDMEN_CONVERT_MENU                0x7111

// define menuitem ids
#define IDMEN_CONVERT_POINTER             0x7120
#define IDMEN_CONVERT_CURSOR              0x7121
#define IDMEN_CONVERT_WINANIMATION        0x7122
#define IDMEN_CONVERT_ANIMOUSE            0x7123

#endif


