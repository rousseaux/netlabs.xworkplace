
#ifndef AMPTRENG_RCH
#define AMPTRENG_RCH

#define __TITLE__                                "Animated Mouse Pointer for OS/2"

//
//      Text for dialog controls
//

//
// Here come some strings that are needed also without a "~" in the online help.
// The defines are also used below in order to avoid duplicates.
// NOTE: String-concatenation does not work for Strings with a leading "~"
//       so sometimes duplicates with/without "~" cannot be avoided

#define IDT_IDTXT_NBPAGE                         "Pointers"
#define IDT_IDTXT_GROUPBOX                       __TITLE__
#define IDT_IDTXT_FIND                           "Find"
#define IDT_IDTXT_LOADSET                        "Load Set"
#define IDT_IDTXT_USEFORALL                      "Use for all pointers"
#define IDT_IDTXT_ANIMATEONLOAD                  "Activate animation on Load"
#define IDT_IDTXT_ANIMATE                        "Animate"
#define IDT_IDTXT_DEMO                           "Demo"
#define IDT_IDTXT_SETTINGS                       "Settings/Properties"

//
// strings for the "Pointers" page
// Please refer to the layout of the original Pointers page
// of your OS/2. The page tab and all buttons except the "Load Group"
// button should be named like on the original page.
//

#define IDT_IDTAB_NBPAGE                         "~Pointers"
#define IDT_IDDLG_PB_OK                          "OK"
#define IDT_IDDLG_PB_CANCEL                      "Cancel"
#define IDT_IDDLG_PB_EDIT                        "Edit..."
#define IDT_IDDLG_PB_FIND                        IDT_IDTXT_FIND "..."
#define IDT_IDDLG_PB_LOAD                        IDT_IDTXT_LOADSET "..."
#define IDT_IDDLG_PB_UNDO                        "~Undo"
#define IDT_IDDLG_PB_DEFAULT                     "~Default"
#define IDT_IDDLG_PB_HELP                        "Help"
#define IDT_IDDLG_PB_CLOSE                       "~Close"
#define IDT_IDDLG_PB_RETURN                      "~Return"

// In WARP 4 english "settings" are called "properties".
// Please check wether they are called different also
// between WARP 3 and WARP 4 version of your language.
// If not, make both strings the same.
#define IDT_IDDLG_DLG_CNRSETTINGS_230            "Animation Settings"
#define IDT_IDDLG_DLG_CNRSETTINGS                "Animation Properties"

#define IDT_IDDLG_GB_ANIMATIONPATH               "Animation File Path"
#define IDT_IDDLG_GB_FRAMELENGTH                 "Default Frame Length Value"
#define IDT_IDDLG_ST_VALUE_MS                    "Value in ms.:"
#define IDT_IDDLG_CB_USEFORALL                   "~" IDT_IDTXT_USEFORALL
#define IDT_IDDLG_GB_LOAD                        "Load"
#define IDT_IDDLG_CB_ANIMATEONLOAD               "~" IDT_IDTXT_ANIMATEONLOAD
#define IDT_IDDLG_GB_HIDEPOINTER                 "Hide pointer"
#define IDT_IDDLG_CB_HIDEPOINTER                 "Hide pointer"
#define IDT_IDDLG_GB_HIDEPOINTERDELAY            "Delay for hiding the pointer"
#define IDT_IDDLG_GB_DRAGPTRTYPE                 "Default filetype for pointer"
#define IDT_IDDLG_GB_DRAGSETTYPE                 "Default filetype for pointer set"
#define IDT_IDDLG_GB_INITDELAY                   "Animation initialization Delay"
#define IDT_IDDLG_ST_VALUE_S                     "Value in s.:"

//
// strings for the container settings notebook page tabs
//

#define IDT_IDTAB_NBANIMATION                    "~Animation"
#define IDT_IDTAB_NBHIDE                         "~Hide"
#define IDT_IDTAB_NBDRAGDROP                     "~Drag&Drop"
#define IDT_IDTAB_NBINIT                         "~Initialization"

//
// strings for the about box.
// Enter your name for IDT_IDDLG_DLG_ABOUT_TRANSLATOR
//

#define IDT_IDDLG_DLG_ABOUT                      "Product Information"
#define IDT_IDDLG_DLG_ABOUT_TITLE                __TITLE__
#define IDT_IDDLG_DLG_ABOUT_VERSION              "Version " BLDLEVEL_VERSION
#define IDT_IDDLG_DLG_ABOUT_AUTHOR               "by __AUTHOR__  __YEAR__"
#define IDT_IDDLG_DLG_ABOUT_RIGHTS               "All rights reserved."
#define IDT_IDDLG_DLG_ABOUT_NLS                  "National Language Support"
#define IDT_IDDLG_DLG_ABOUT_TRANSLATOR           "by Christian Langanke"
#define IDT_IDDLG_DLG_ABOUT_SENDTO               "Send email to:"
#define IDT_IDDLG_DLG_ABOUT_EMAILADR             "email"

//
// Strings for the "find" results dialog
//

#define IDT_IDDLG_DLG_LOADSET                    "Please make a selection from the following list:"
#define IDT_IDDLG_ST_FILTER                      "Select resource types to be displayed:"

//
// Strings for the mouse pointer container
//

#define IDT_IDMEN_ITEM_SETTINGS_230              "Se~ttings"
#define IDT_IDMEN_ITEM_SETTINGS                  "~Properties"
#define IDT_IDMEN_ITEM_HELP                      "~Help"
#define IDT_IDMEN_ITEM_HELP_INDEX                "Help ~index"
#define IDT_IDMEN_ITEM_HELP_GENERAL              "~General help"
#define IDT_IDMEN_ITEM_HELP_USING                "~Using help"
#define IDT_IDMEN_ITEM_HELP_KEYS                 "~Keys help"
#define IDT_IDMEN_ITEM_HELP_ABOUT                "~Product information"
#define IDT_IDMEN_ITEM_EDIT                      "~Edit..."
#define IDT_IDMEN_ITEM_FIND                      "~Find..."
#define IDT_IDMEN_ITEM_SAVEAS                    "~Save As..."
#define IDT_IDMEN_ITEM_DEFAULT                   "~Default"
#define IDT_IDMEN_ITEM_ANIMATE                   "~Animate"

#define IDT_IDMEN_FOLDER_SETTINGS_230            IDT_IDMEN_ITEM_SETTINGS_230
#define IDT_IDMEN_FOLDER_SETTINGS                IDT_IDMEN_ITEM_SETTINGS
#define IDT_IDMEN_FOLDER_VIEW                    "~View"
#define IDT_IDMEN_FOLDER_VIEW_ICON               "~Icon view"
#define IDT_IDMEN_FOLDER_VIEW_DETAIL             "~Details view"
#define IDT_IDMEN_FOLDER_HELP                    IDT_IDMEN_ITEM_HELP
#define IDT_IDMEN_FOLDER_HELP_INDEX              IDT_IDMEN_ITEM_HELP_INDEX
#define IDT_IDMEN_FOLDER_HELP_GENERAL            IDT_IDMEN_ITEM_HELP_GENERAL
#define IDT_IDMEN_FOLDER_HELP_USING              IDT_IDMEN_ITEM_HELP_USING
#define IDT_IDMEN_FOLDER_HELP_KEYS               IDT_IDMEN_ITEM_HELP_KEYS
#define IDT_IDMEN_FOLDER_HELP_ABOUT              IDT_IDMEN_ITEM_HELP_ABOUT
#define IDT_IDMEN_FOLDER_FIND                    "~Load Set..."
#define IDT_IDMEN_FOLDER_SAVEAS                  "~Save As..."
#define IDT_IDMEN_FOLDER_DEFAULT                 IDT_IDMEN_ITEM_DEFAULT
#define IDT_IDMEN_FOLDER_HIDEPOINTER             "~Hide Pointer"
#define IDT_IDMEN_FOLDER_BLACKWHITE              "~Black&White"
#define IDT_IDMEN_FOLDER_DEMO                    "De~mo"
#define IDT_IDMEN_FOLDER_ANIMATE                 IDT_IDMEN_ITEM_ANIMATE

#define IDT_IDSTR_FILETYPE_DEFAULT               "Default"
#define IDT_IDSTR_FILETYPE_POINTER               "OS/2 Pointer"
#define IDT_IDSTR_FILETYPE_POINTERSET            "OS/2 Pointer Set"
#define IDT_IDSTR_FILETYPE_CURSOR                "Win Cursor"
#define IDT_IDSTR_FILETYPE_CURSORSET             "Win Cursor Set"
#define IDT_IDSTR_FILETYPE_ANIMOUSE              "AniMouse"
#define IDT_IDSTR_FILETYPE_ANIMOUSESET           "AniMouse Set"
#define IDT_IDSTR_FILETYPE_ANMFILE               "AniMouse scriptfile"
#define IDT_IDSTR_FILETYPE_WINANIMATION          "Win Animation"
#define IDT_IDSTR_FILETYPE_WINANIMATIONSET       "Win Animation Set"
#define IDT_IDSTR_FILETYPE_ANIMATIONSETDIR       "Animation Set Directory"
#define IDT_IDSTR_FILETYPE_ALL                   "<All Files>"

#define IDT_IDSTR_POINTER_ARROW                  "Arrow"
#define IDT_IDSTR_POINTER_TEXT                   "Text"
#define IDT_IDSTR_POINTER_WAIT                   "Wait"
#define IDT_IDSTR_POINTER_SIZENWSE               "Size NWSE"
#define IDT_IDSTR_POINTER_SIZEWE                 "Size WE"
#define IDT_IDSTR_POINTER_MOVE                   "Move"
#define IDT_IDSTR_POINTER_SIZENESW               "Size NESW"
#define IDT_IDSTR_POINTER_SIZENS                 "Size NS"
#define IDT_IDSTR_POINTER_ILLEGAL                "Illegal"

#define IDT_IDSTR_TITLE_ICON                     "Icon"
#define IDT_IDSTR_TITLE_NAME                     "Name"
#define IDT_IDSTR_TITLE_STATUS                   "Status"
#define IDT_IDSTR_TITLE_ANIMATIONTYPE            "Animation Type"
#define IDT_IDSTR_TITLE_POINTER                  "Pointer"
#define IDT_IDSTR_TITLE_ANIMATIONNAME            "Animation Name"
#define IDT_IDSTR_TITLE_FRAMERATE                "Framelength"
#define IDT_IDSTR_TITLE_INFONAME                 "Name of Animation"
#define IDT_IDSTR_TITLE_INFOARTIST               "Creator of Animation"
#define IDT_IDSTR_STATUS_ON                      "on"
#define IDT_IDSTR_STATUS_OFF                     "off"

//
// text for message boxes
//

#define IDT_IDMSG_TITLE_ERROR                    "Error"
#define IDT_IDMSG_ANIMATIONPATH_NOT_FOUND        "The Animation File Path specified does not exist."
#define IDT_IDMSG_TITLENOTFOUND                  "Find Results"
#define IDT_IDMSG_MSGNOTFOUND                    "The " IDT_IDDLG_GB_ANIMATIONPATH " does not contain animation ressources."

//
// some system related text
//

// take titles of the objects from ?:\os2\ini.rc
#define IDT_IDTXT_INFORMATION                    "Information"
#define IDT_IDTXT_MOUSE_OBJECT                   "Mouse object"
#define IDT_IDTXT_SYSTEM_SETUP                   "System Setup"
#define IDT_IDTXT_ICON_EDITOR                    "Icon Editor"

// call ?:\os2\install\install.exe to obtain the appropriate strings
#define IDT_IDTXT_SELECTIVE_INSTALL              "Selective Install"
#define IDT_IDTXT_OPTIONAL_SYSTEM_UTILITIES      "Optional System Utilities"
#define IDT_IDTXT_LINK_OBJECT_MODULES            "Link Object Modules"

//
// strings for the notebook pages of the file classes
//

// animation pages
#define IDT_IDDLG_GB_ANIMINFO                    "Animation Info"
#define IDT_IDDLG_ST_ANIMNAME                    IDT_IDSTR_TITLE_INFONAME ":"
#define IDT_IDDLG_ST_ANIMARTIST                  IDT_IDSTR_TITLE_INFOARTIST ":"
#define IDT_IDDLG_GB_ANIMDETAILS                 "Animation Details"
#define IDT_IDDLG_ST_ANIMFRAMES                  "Number of physical / total frames:"
#define IDT_IDDLG_ST_ANIMTIMEOUT                 "Default Frame Length Value in ms.:"

//
// General/Icon page replacement (see General/Icon page of any Desktop object of your OS/2 !)
//

// In english this page is called:
// - WARP 3: "General"
// - WARP 4: "Icon"
#define IDT_IDTAB_NBGENERAL_230                  "~General"
#define IDT_IDTAB_NBGENERAL                      "~Icon"
#define IDT_IDDLG_ST_TITLE                       "Title:"
#define IDT_IDDLG_GB_CONTENTS                    "Current icon"
#define IDT_IDDLG_GB_CONTENTS_ANIMATION          "Current animation"
#define IDT_IDDLG_CB_TEMPLATE                    "~Template"
#define IDT_IDDLG_CB_LOCKPOS                     "~Lock position"

//
// Strings for the context menus of fileclasses
//

#define IDT_IDMEN_CONVERT_SUBMENU                "Convert to..."


#define IDT_IDMEN_CONVERT_POINTER                "OS/2 Pointer"
#define IDT_IDMEN_CONVERT_CURSOR                 "Win Cursor"
#define IDT_IDMEN_CONVERT_WINANIMATION           "Win Animation"
#define IDT_IDMEN_CONVERT_ANIMOUSE               "Animouse Resource DLL"

//
// strings for the Edit Animation dialog
//

#define IDT_IDDLG_DLG_EDITANIMATION              "Animation Editor"
#define IDT_IDDLG_ST_SHOWFOR                     "Show frame for"
#define IDT_IDDLG_GB_PREVIEW                     "Preview"
#define IDT_IDDLG_GB_ANIMINFO                    "Animation Info"
//      IDT_IDDLG_ST_ANIMNAME
//      IDT_IDDLG_ST_ANIMARTIST

//
// Strings for the frame set container
//

#define IDT_IDSTR_TITLE_IMAGE                    "Image"
//      IDT_IDSTR_TITLE_FRAMERATE
#define IDT_IDSTR_SELECTED_NONE                  " - No frame selected -"
#define IDT_IDSTR_SELECTED_FRAME                 " Frame %u of %u selected"
#define IDT_IDSTR_SELECTED_FRAMES                " %u Frames selected"
#define IDT_IDSTR_UNIT_MILLISECONDS              "ms."
#define IDT_IDSTR_UNIT_JIFFIES                   "Jiffies"

#define IDT_IDMEN_AE_FILE                        "~File"
#define IDT_IDMEN_AE_FILE_NEW                    "~New"
#define IDT_IDMEN_AE_FILE_OPEN                   "~Open..."
#define IDT_IDMEN_AE_FILE_SAVE                   "~Save"
#define IDT_IDMEN_AE_FILE_SAVEAS                 "Save ~as..."
#define IDT_IDMEN_AE_FILE_IMPORT                 "~Import frame..."
#define IDT_IDMEN_AE_FILE_EXIT                   "E~xit"
#define IDT_IDMEN_AE_EDIT                        "~Edit"
#define IDT_IDMEN_AE_EDIT_COPY                   "~Copy"
#define IDT_IDMEN_AE_EDIT_CUT                    "Cu~t"
#define IDT_IDMEN_AE_EDIT_PASTE                  "Pa~ste"
#define IDT_IDMEN_AE_EDIT_DELETE                 "~Delete"
#define IDT_IDMEN_AE_EDIT_SELECTALL              "Select ~all"
#define IDT_IDMEN_AE_EDIT_DESELECTALL            "D~eselect all"
#define IDT_IDMEN_AE_PALETTE                     "~Palette"
#define IDT_IDMEN_AE_PALETTE_OPEN                "~Open"
#define IDT_IDMEN_AE_PALETTE_SAVE                "~Save"
#define IDT_IDMEN_AE_PALETTE_SAVEAS              "Save ~as..."
#define IDT_IDMEN_AE_PALETTE_COPY                "~Copy"
#define IDT_IDMEN_AE_PALETTE_PASTE               "~Paste"
#define IDT_IDMEN_AE_OPTION                      "~Options"
#define IDT_IDMEN_AE_OPTION_UNIT                 "Change Unit"
#define IDT_IDMEN_AE_OPTION_UNIT_MS              "milliseconds"
#define IDT_IDMEN_AE_OPTION_UNIT_JIF             "Jiffies"
#define IDT_IDMEN_AE_HELP                        IDT_IDMEN_ITEM_HELP
#define IDT_IDMEN_AE_HELP_INDEX                  IDT_IDMEN_ITEM_HELP_INDEX
#define IDT_IDMEN_AE_HELP_GENERAL                IDT_IDMEN_ITEM_HELP_GENERAL
#define IDT_IDMEN_AE_HELP_USING                  IDT_IDMEN_ITEM_HELP_USING
#define IDT_IDMEN_AE_HELP_KEYS                   IDT_IDMEN_ITEM_HELP_KEYS
#define IDT_IDMEN_AE_ABOUT                       IDT_IDMEN_ITEM_HELP_ABOUT

//
// text for online help panel titles
//

#define IDH_IDPNL_MAIN                           __TITLE__

#define IDH_IDPNL_OVERVIEW                       "Overview"

#define IDH_IDPNL_REVIEWS                        "Reviews"

#define IDH_IDPNL_PREREQUISITES                  "Prerequisites"

#define IDH_IDPNL_LEGAL                          "Copyright &amp. Co."
#define IDH_IDPNL_LEGAL_COPYRIGHT                "Copyright"
#define IDH_IDPNL_LEGAL_LICENSE                  "freeware license"
#define IDH_IDPNL_LEGAL_PROMOTEOS2               "About OS/2"
#define IDH_IDPNL_LEGAL_DISCLAIMER               "Disclaimer"
#define IDH_IDPNL_LEGAL_AUTHOR                   "The author"

#define IDH_IDPNL_USAGE                          "Usage"
#define IDH_IDPNL_USAGE_OS2POINTER               "OS/2 mouse pointer"
#define IDH_IDPNL_USAGE_ANIMATION                "mouse pointer animations"
#define IDH_IDPNL_USAGE_RESTYPES                 "Supported resource types"
#define IDH_IDPNL_USAGE_RESTYPES_POINTER         "OS/2 pointer file"
#define IDH_IDPNL_USAGE_RESTYPES_CURSOR          "Win* cursor file"
#define IDH_IDPNL_USAGE_RESTYPES_WINANIMATION    "Win* Animation file"
#define IDH_IDPNL_USAGE_RESTYPES_ANIMOUSE        "AniMouse resource DLL file"
#define IDH_IDPNL_USAGE_RESTYPES_ANMFILE         "AniMouse scriptfile"
#define IDH_IDPNL_USAGE_RESTYPES_SET             "animation set"
#define IDH_IDPNL_USAGE_RESTYPES_SET_POINTER     "OS/2 pointer set"
#define IDH_IDPNL_USAGE_RESTYPES_SET_CURSOR      "Win cursor set"
#define IDH_IDPNL_USAGE_RESTYPES_SET_WINANIMATION "Win Animation set"
#define IDH_IDPNL_USAGE_RESTYPES_SET_ANIMOUSE    "AniMouse Animation set"
#define IDH_IDPNL_USAGE_STATICPTR                "Static Pointer"
#define IDH_IDPNL_USAGE_NBPAGE                   "&IDT_IDTXT_NBPAGE. page"
#define IDH_IDPNL_USAGE_NBPAGE_CNRSETTINGS       "Animation settings/properties"
#define IDH_IDPNL_USAGE_NBPAGE_CNRLOADSET        "Load Animation Set"
#define IDH_IDPNL_USAGE_SETTINGS_STRINGS         "Mouse object settings strings"
#define IDH_IDPNL_USAGE_REXX_SAMPLES             "REXX sample programs"
#define IDH_IDPNL_USAGE_ENVIRONMENT_VARIABLES    "environment variables"
#define IDH_IDPNL_USAGE_MAKEAND                  "Create AniMouse resource DLL files"
#define IDH_IDPNL_USAGE_MAKEAND_SCRIPT           "AniMouse scriptfile format"
#define IDH_IDPNL_USAGE_SUPPLIEDANIMATIONS       "Supplied Animations"
#define IDH_IDPNL_USAGE_WHEREANIMATIONS          "Where to get more/new animations"

#define IDH_IDPNL_HOW                            "How can I ... ?"
#define IDH_IDPNL_HOW__BASETEXT                  "How can I "
#define IDH_IDPNL_HOW__BASEDOT                   "... "
#define IDH_IDPNL_HOW_LOADSET                    "load an animation resource ?"
#define IDH_IDPNL_HOW_ACTIVATE                   "(de-)activate an animation ?"
#define IDH_IDPNL_HOW_DEMO                       "(de-)activate the demo function ?"
#define IDH_IDPNL_HOW_SETTINGS                   "change animation settings ?"
#define IDH_IDPNL_HOW_DELAYINITANIMATION         "delay the initialization of the animation ?"
#define IDH_IDPNL_HOW_USEREXX                    "use REXX to configure &__TITLE__. ?"
#define IDH_IDPNL_HOW_MAKEAND                    "create an AniMouse resource DLL ?"

#define IDH_IDPNL_LIMITATIONS                    "Limitations"

#define IDH_IDPNL_REVISIONS                      "Revision history"
#define IDH_IDPNL_REVISION_HISTORY_100           "Version 1.00"
#define IDH_IDPNL_REVISION_HISTORY_101           "Version 1.01"
#define IDH_IDPNL_FILE_ID_DIZ                    "file_id.diz"

#define IDH_IDPNL_CREDITS                        "Credits"
#define IDH_IDPNL_CREDITS_GENERAL                "General credits"
#define IDH_IDPNL_CREDITS_TRANSLATORS            "Translators"
#define IDH_IDPNL_CREDITS_ANIMATIONS             "Suppliers of animations"

#define IDH_IDPNL_TRADEMARKS                     "Trademarks"

#endif // AMPTRENG_RCH

