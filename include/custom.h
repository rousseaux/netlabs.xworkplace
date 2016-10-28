/*
 * custom.h
 *
 * 'custom.h' is included by the files noted below to supply
 * strings and filenames that may vary in custom builds.
 *
 *@@added V1.0.11 (2016-09-29) [rwalsh]
 */

#ifndef CUSTOM_HEADER_INCLUDED
    #define CUSTOM_HEADER_INCLUDED

// from include\shared\common.h
    #ifdef COMMON_HEADER_INCLUDED
        DECLARE_CMN_STRING(ENTITY_OS2, "OS/2");
        DECLARE_CMN_STRING(ENTITY_WINOS2, "Win-OS/2");
        DECLARE_CMN_STRING(ENTITY_WARPCENTER, "WarpCenter");
        DECLARE_CMN_STRING(ENTITY_XCENTER, "XCenter");
        DECLARE_CMN_STRING(ENTITY_XBUTTON, "X-Button");
        DECLARE_CMN_STRING(ENTITY_XSHUTDOWN, "XShutdown");
        DECLARE_CMN_STRING(ENTITY_PAGER, "XPager");
    #endif

    #define XWORKPLACE_STRING       "XWorkplace"

// from include\shared\kernel.h
    #define XFOLDER_CRASHLOG        "xwptrap.log"
    #define XFOLDER_SHUTDOWNLOG     "xshutdwn.log"
    #define XFOLDER_LOGLOG          "xwplog.log"
    #define XFOLDER_DMNCRASHLOG     "xdmntrap.log"

// from src/classes/xfsys.c
    #define XWORKPLACE_KERNEL       "OS/2 Kernel"

// from src/shared/common.c
    #define XWORKPLACE_ERROR        "XWorkplace error"

// from src/shared/xwpres.rc

// these are the default pathnames for the graphics packaged by xwpres.rc;
// a custom build can replace any or all of them by changing the pathname
// in its own version of this file (see ecs\include\custom.h for an example).
#ifdef RC_INVOKED
    #define XFLDRDLG_ICO        "grafx\\xfldrdlg.ico"
    #define XFLDSTART1_ICO      "grafx\\xfldstart1.ico"
    #define XFLDSTART2_ICO      "grafx\\xfldstart2.ico"
    #define XFLDSHUT1_ICO       "grafx\\xfldshut1.ico"
    #define XFLDSHUT2_ICO       "grafx\\xfldshut2.ico"
    #define XFSYS_ICO           "grafx\\xfsys.ico"
    #define XFWPS_ICO           "grafx\\xfwps.ico"
    #define XWPCONFG_ICO        "grafx\\xwpconfg.ico"
    #define XCLSLIST_ICO        "grafx\\xclslist.ico"
    #define XWPSOUND_ICO        "grafx\\xwpsound.ico"
    #define XWPSCREEN_ICO       "grafx\\xwpscreen.ico"
    #define MMVOLUME_ICO        "grafx\\mmvolume.ico"
    #define XCENTER_ICO         "grafx\\xcenter.ico"
    #define XTRASH1_ICO         "grafx\\xtrash1.ico"
    #define XTRASH2_ICO         "grafx\\xtrash2.ico"
    #define XWPSTRING_ICO       "grafx\\xwpstring.ico"
    #define VCARD_ICO           "grafx\\vcard.ico"
    #define SHADOW_ICO          "grafx\\shadow.ico"
    #define HAND_PTR            "grafx\\hand.ptr"
    #define XFSHUT1_ICO         "grafx\\xfshut1.ico"
    #define XFSHUT2_ICO         "grafx\\xfshut2.ico"
    #define XFSHUT3_ICO         "grafx\\xfshut3.ico"
    #define XFSHUT4_ICO         "grafx\\xfshut4.ico"
    #define XFSHUT5_ICO         "grafx\\xfshut5.ico"
    #define FONTFDR0_ICO        "grafx\\fontfdr0.ico"
    #define FONTFDR1_ICO        "grafx\\fontfdr1.ico"
    #define FONTOBJ_ICO         "grafx\\fontobj.ico"
    #define FONTOBJ_ICO         "grafx\\fontobj.ico"
    #define DRIVE_ICO           "grafx\\drive.ico"
    #define DRV_CD_ICO          "grafx\\drv_cd.ico"
    #define DRV_LAN_ICO         "grafx\\drv_lan.ico"
    #define DRV_NORMAL_ICO      "grafx\\drv_normal.ico"
    #define PWR_AC_ICO          "grafx\\pwr_ac.ico"
    #define PWR_BATTERY_ICO     "grafx\\pwr_battery.ico"
    #define XWPADMIN_ICO        "grafx\\xwpadmin.ico"
    #define MMCDPLAY_ICO        "grafx\\mmcdplay.ico"
    #define XWPMEDIA_ICO        "grafx\\xwpmedia.ico"
    #define CD_EJECT_ICO        "grafx\\cd_eject.ico"
    #define CD_NEXT_ICO         "grafx\\cd_next.ico"
    #define CD_PAUSE_ICO        "grafx\\cd_pause.ico"
    #define CD_PLAY_ICO         "grafx\\cd_play.ico"
    #define CD_PREV_ICO         "grafx\\cd_prev.ico"
    #define CD_STOP_ICO         "grafx\\cd_stop.ico"
    #define X_MINI_ICO          "grafx\\x_mini.ico"
    #define XWPMINILOGO_BMP     "grafx\\xwpminilogo.bmp"
    #define XWPLOGO_BMP         "grafx\\xwplogo.bmp"
    #define TRAY_BMP            "grafx\\tray.bmp"
#endif

#endif

