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
        DECLARE_CMN_STRING(ENTITY_OS2, "eComStation");
        DECLARE_CMN_STRING(ENTITY_WINOS2, "Win16");
        DECLARE_CMN_STRING(ENTITY_WARPCENTER, "eComCenter");
        DECLARE_CMN_STRING(ENTITY_XCENTER, "eCenter");
        DECLARE_CMN_STRING(ENTITY_XBUTTON, "eButton");
        DECLARE_CMN_STRING(ENTITY_XSHUTDOWN, "eShutdown");
        DECLARE_CMN_STRING(ENTITY_PAGER, "ePager");
    #endif

    #define XWORKPLACE_STRING       "eComStation"

// from include\shared\kernel.h
    #define XFOLDER_CRASHLOG        "ewptrap.log"
    #define XFOLDER_SHUTDOWNLOG     "eshutdwn.log"
    #define XFOLDER_LOGLOG          "ewplog.log"
    #define XFOLDER_DMNCRASHLOG     "edmntrap.log"

// from src/classes/xfsys.c
    #define XWORKPLACE_KERNEL       "eComStation Kernel"

// from src/shared/common.c
    #define XWORKPLACE_ERROR        "eComStation Workplace Shell error"

// from src/shared/xwpres.rc

// this defines the pathnames for the graphics packaged by xwpres.rc;
// to replace a default graphic with a customized version, replace the
// default pathname with [__GRAFX__"\\newname"] and place the replacement
// file in your 'grafx' directory
#ifdef RC_INVOKED
    #define XFLDRDLG_ICO        __GRAFX__"\\ecs.ico"
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
    #define XCENTER_ICO         __GRAFX__"\\xcenter.ico"
    #define XTRASH1_ICO         "grafx\\xtrash1.ico"
    #define XTRASH2_ICO         "grafx\\xtrash2.ico"
    #define XWPSTRING_ICO       "grafx\\xwpstring.ico"
    #define VCARD_ICO           "grafx\\vcard.ico"
    #define SHADOW_ICO          __GRAFX__"\\shadow.ico"
    #define HAND_PTR            "grafx\\hand.ptr"
    #define XFSHUT1_ICO         "grafx\\xfshut1.ico"
    #define XFSHUT2_ICO         "grafx\\xfshut2.ico"
    #define XFSHUT3_ICO         "grafx\\xfshut3.ico"
    #define XFSHUT4_ICO         "grafx\\xfshut4.ico"
    #define XFSHUT5_ICO         "grafx\\xfshut5.ico"
    #define FONTFDR0_ICO        __GRAFX__"\\fontfdr0.ico"
    #define FONTFDR1_ICO        __GRAFX__"\\fontfdr1.ico"
    #define FONTOBJ_ICO         __GRAFX__"\\fontobj.ico"
    #define FONTOBJ_ICO         __GRAFX__"\\fontobj.ico"
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
    #define X-MINI_ICO          "grafx\\x-mini.ico"
    #define XWPMINILOGO_BMP     __GRAFX__"\\ecsminilogo.bmp"
    #define XWPLOGO_BMP         __GRAFX__"\\ecslogo.bmp"
    #define TRAY_BMP            "grafx\\tray.bmp"
#endif

#endif

