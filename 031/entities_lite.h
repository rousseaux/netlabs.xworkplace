
/*
 *@@sourcefile entities.h:
 *      string replacements for the INF and HLP files.
 *
 */
/*
 *After consulting Bart van Leeuwen, eCS Program Manager due
 *to text conflicts in the eWPS .hlp the following line changed
 *#define xwp "eComStation"
 *to:
*/
#define xwp "eComStation"
#define os2 "eComStation"
#define warp "OS/2 Warp"
#define warp4 "OS/2 Warp 4"
#define winos2 "Win16"
#define warpcenter "eComCenter"
#define xcenter "eCenter"
#define xbutton "eButton"
#define xshutdown "eShutdown"
#define pgr "ePager"
#define contact-user "xworkplace-user@netlabs.org"
#define contact-dev "xworkplace-dev@netlabs.org"

#define colon   ":"
#define euml    "�"
#define eacute  "�"

#define cfgsys "<CODE>CONFIG.SYS</CODE>"

// V0.9.19: added these two entities. Please replace all occurences
// in your translation with the entity (i.e. replace "System Setup"
// with &syssetup;) to allow for dynamic string replacements.
#define syssetup "Configuratie"
#define cfgfdr "Uitgebreide menu opties map"
#define popmenu "voorgrondmenu"

// V0.9.20: added the following
#define eexe "Tekst Editor"
#define sysfdr "Lokaal Systeem"
#define cntryobj "Locale"
#define minwinv "Vensters als pictogrammen"
#define os2short "eCS"
#define connectfdr "Lokaal Netwerk"
#define winos2setup "Win16 instellingen"
#define link_desktop "<IMG SRC=\"img/desktop_mini.gif\"><A HREF=\"objects/obj_desktop.html\">De Werkplek</A>"
#define link_progobjs "<A HREF=\"objects/obj_pgmobj.html\">programma objecten</A>"
#define link_tempsfdr "<IMG SRC=\"img/os2temps_mini.gif\"><A HREF=\"objects/obj_os2wptemplates.html\">Modellenmap</A>"
#define link_cmdfdr "<IMG SRC=\"img/os2cmdpfdr_mini.gif\"><A HREF=\"objects/obj_os2cmdfdr.html\">Opdrachtaanwijzingen map</A>"
#define link_drivesfdr "<IMG SRC=\"img/os2drives_mini.gif\"><A HREF=\"objects/obj_os2drivfdr.html\">De Stationsmap</A>"
#define link_warpcenter "<A HREF=\"objects/obj_os2warpcenter.html\">&warpcenter;</A>"
#define link_sysfdr "<IMG SRC=\"img/os2system_mini.gif\"><A HREF=\"objects/obj_os2system.html\">&sysfdr;map</A>"
#define link_syssetupfdr "<IMG SRC=\"img/setupfdr_mini.gif\"><A HREF=\"objects/obj_setupfdr.html\">&syssetup;map</A>"
#define link_lookfdr "<IMG SRC=\"img/setup_look_mini.gif\"><A HREF=\"objects/obj_setup_look.html\">Werkplek Uiterlijk map</A>"
#define link_wpnetgrp "<IMG SRC=\"img/os2wpnetgrp_mini.gif\"><A HREF=\"objects/obj_os2wpnetgrp.html\">Bestand en Print Client Hulpbron zoeken</A>"
#define link_internet "<A HREF=\"glossary/gls_internet.html\">Internet</A>"
#define link_tcpip "<A HREF=\"glossary/gls_prot_tcpip.html\">TCP/IP</A>"
#define link_netbios "<A HREF=\"glossary/gls_prot_netbios.html\">NetBIOS</A>"
#define link_peer "<A HREF=\"glossary/gls_peer.html\">Bestand en Print Client</A>"
// for the xwphelp2 dir, replace all occurences of
// <A HREF="objects/obj_wps.html">"Workplace Shell" object</A>
// with
// &link_wpsobj;
// which defines a mini icon with the link to the page
#define link_wpsobj "<IMG SRC=\"img/xfwps_mini.gif\"><A HREF=\"objects/obj_wps.html\">Workplace Shell object</A>"
// for the xwphelp2 dir, replace all occurences of
// <A HREF="objects/obj_fdr_main.html">folders</A>
// (note, plural) with
// &link_folders;
#define link_folders "<A HREF=\"objects/obj_fdr_main.html\">mappen</A>"
// for the xwphelp2 dir, replace all occurences of
// <A HREF="objects/obj_dataf.html">data files</A>
// (note, plural) with
// &link_datafiles;
#define link_datafiles "<A HREF=\"objects/obj_dataf.html\">gegevensbestanden</A>"
