
#ifndef WPAMPTR_RCH
#define WPAMPTR_RCH

#include "pointers\title.h"

// --- diverse res ids

#define IDDLG_UNUSED                      -1
#define IDTAB_NBPAGE                      0x00F0
#define IDDLG_GB_NBPAGE                   0x00F1

#define IDDLG_NBANIMATION                 0x00F2
#define IDDLG_NBHIDE                      0x00F3
#define IDDLG_NBDRAGDROP                  0x00F4
#define IDDLG_NBINIT                      0x00F5

#define IDTAB_NBANIMATION                 0x00F6
#define IDTAB_NBHIDE                      0x00F7
#define IDTAB_NBDRAGDROP                  0x00F8
#define IDTAB_NBINIT                      0x00F9

// --- pushbuttons

#define IDDLG_PB_OK                       0x0100
#define IDDLG_PB_CANCEL                   0x0101
#define IDDLG_PB_EDIT                     0x0102
#define IDDLG_PB_FIND                     0x0103
#define IDDLG_PB_LOAD                     0x0104
#define IDDLG_PB_UNDO                     0x0105
#define IDDLG_PB_DEFAULT                  0x0106
#define IDDLG_PB_HELP                     0x0107
#define IDDLG_PB_CLOSE                    0x0108
#define IDDLG_PB_RETURN                   0x0109


// --- notebook page dialog

#define IDDLG_DLG_ANIMATEDWAITPOINTER_230 0x0110
#define IDDLG_DLG_ANIMATEDWAITPOINTER     0x0111
#define IDDLG_CN_POINTERSET               0x0101


// --- about dialog

#define IDDLG_DLG_ABOUT                   0x0120


// --- settings dialog

#define IDDLG_DLG_CNRSETTINGS_230         0x0130
#define IDDLG_DLG_CNRSETTINGS             0x0131
#define IDDLG_EF_ANIMATIONPATH            0x0132
#define IDDLG_SB_FRAMELENGTH              0x0133
#define IDDLG_CB_USEFORALL                0x0134
#define IDDLG_CB_ANIMATEONLOAD            0x0135
#define IDDLG_EF_DRAGPTRTYPE              0x0136
#define IDDLG_EF_DRAGSETTYPE              0x0137
#define IDDLG_CB_HIDEPOINTER              0x0138
#define IDDLG_SB_HIDEPOINTERDELAY         0x0139
#define IDDLG_SB_INITDELAY                0x013A


// --- load set dialog

#define IDDLG_DLG_LOADSET                 0x0140
#define IDDLG_CN_FOUNDSETS                0x0141
#define IDDLG_CO_FILTER                   0x0142


// --- animation page for file classes properties notebook
#define IDDLG_DLG_ANIMATIONPAGE1_230      0x0150
#define IDDLG_DLG_ANIMATIONPAGE1          0x0151
#define IDDLG_DLG_ANIMATIONPAGE2_230      0x0152
#define IDDLG_DLG_ANIMATIONPAGE2          0x0153
#define IDDLG_DLG_ANIMATIONPAGE3_230      0x0154
#define IDDLG_DLG_ANIMATIONPAGE3          0x0155

#define IDTAB_NBGENERAL_230               0x015E
#define IDTAB_NBGENERAL                   0x015F

#define IDDLG_EF_INFONAME                 0x0160
#define IDDLG_EF_INFOARTIST               0x0161
#define IDDLG_EF_FRAMES                   0x0162
#define IDDLG_EF_PHYSFRAMES               0x0163
#define IDDLG_EF_DEFTIMEOUT               0x0164

// --- icon page for file classes properties notebook
#define IDDLG_DLG_ICONPAGE_230            0x0160
#define IDDLG_DLG_ICONPAGE                0x0161

#define IDDLG_ME_TITLE                    0x0162
#define IDDLG_CB_TEMPLATE                 0x0161
#define IDDLG_CB_LOCKPOS                  0x0163
#define IDDLG_GB_CONTENTS                 0x0164
#define IDDLG_IC_CONTENTS                 0x0165

// fine IDTAB_NBANIMATION                 bereits definiert


// --- animation editor dialog

#define IDDLG_DLG_EDITANIMATION           0x0180
#define IDDLG_GB_SELECTEDFRAME            0x0181
#define IDDLG_CN_FRAMESET                 0x0182
#define IDDLG_ST_SHOWFOR                  0x0183
//      IDDLG_SB_FRAMELENGTH              ------
#define IDDLG_ST_UNIT                     0x0184
#define IDDLG_GB_PREVIEW                  0x0185
#define IDDLG_ST_PREVIEW                  0x0186
//      IDDLG_EF_INFONAME
//      IDDLG_EF_INFOARTIST

#define IDDLG_BMP_STOP                    0x0187
#define IDDLG_BMP_STOPP                   0x0188
#define IDDLG_BMP_START                   0x0189
#define IDDLG_BMP_STARTP                  0x018A

// --- animation editor menu

#define IDMEN_AE_FILE                     0x0300
#define IDMEN_AE_FILE_NEW                 0x0301
#define IDMEN_AE_FILE_OPEN                0x0302
#define IDMEN_AE_FILE_SAVE                0x0303
#define IDMEN_AE_FILE_SAVEAS              0x0304
#define IDMEN_AE_FILE_IMPORT              0x0305
#define IDMEN_AE_FILE_EXIT                0x0306

#define IDMEN_AE_EDIT                     0x0310
#define IDMEN_AE_EDIT_COPY                0x0311
#define IDMEN_AE_EDIT_CUT                 0x0312
#define IDMEN_AE_EDIT_PASTE               0x0313
#define IDMEN_AE_EDIT_DELETE              0x0314
#define IDMEN_AE_EDIT_SELECTALL           0x0315
#define IDMEN_AE_EDIT_DESELECTALL         0x0316

#define IDMEN_AE_PALETTE                  0x0320
#define IDMEN_AE_PALETTE_OPEN             0x0321
#define IDMEN_AE_PALETTE_SAVEAS           0x0322
#define IDMEN_AE_PALETTE_COPY             0x0323
#define IDMEN_AE_PALETTE_PASTE            0x0324

#define IDMEN_AE_OPTION                   0x0331
#define IDMEN_AE_OPTION_UNIT              0x0332
#define IDMEN_AE_OPTION_UNIT_MS           0x0333
#define IDMEN_AE_OPTION_UNIT_JIF          0x0334

#define IDMEN_AE_HELP                     0x0340
#define IDMEN_AE_HELP_INDEX               0x0341
#define IDMEN_AE_HELP_GENERAL             0x0342
#define IDMEN_AE_HELP_USING               0x0343
#define IDMEN_AE_HELP_KEYS                0x0344
#define IDMEN_AE_HELP_ABOUT               0x0345

// --- animation editor item menu

#define IDMEN_EDITITEM                    0x0400

// --- res ids for container item popup menu

#define IDMEN_ITEM                        0x0200
#define IDMEN_ITEM_HELP                   0x0201
#define IDMEN_ITEM_HELP_INDEX             0x0202
#define IDMEN_ITEM_HELP_GENERAL           0x0203
#define IDMEN_ITEM_HELP_USING             0x0204
#define IDMEN_ITEM_HELP_KEYS              0x0205
#define IDMEN_ITEM_HELP_ABOUT             0x0206
#define IDMEN_ITEM_EDIT                   0x0207
#define IDMEN_ITEM_FIND                   0x0208
#define IDMEN_ITEM_SAVEAS                 0x0209
#define IDMEN_ITEM_DEFAULT                0x020A
#define IDMEN_ITEM_ANIMATE                0x020B


// --- res ids for container popup menu

#define IDMEN_FOLDER                      0x0210
#define IDMEN_FOLDER_SETTINGS_230         0x0211
#define IDMEN_FOLDER_SETTINGS             0x0212
#define IDMEN_FOLDER_SETTINGS_NEW         0x0712
#define IDMEN_FOLDER_VIEW                 0x0213
#define IDMEN_FOLDER_VIEW_ICON            0x0214
#define IDMEN_FOLDER_VIEW_DETAIL          0x0215
#define IDMEN_FOLDER_HELP                 0x0216
#define IDMEN_FOLDER_HELP_INDEX           0x0217
#define IDMEN_FOLDER_HELP_GENERAL         0x0218
#define IDMEN_FOLDER_HELP_USING           0x0219
#define IDMEN_FOLDER_HELP_KEYS            0x021A
#define IDMEN_FOLDER_HELP_ABOUT           0x021B
#define IDMEN_FOLDER_FIND                 0x021C
#define IDMEN_FOLDER_SAVEAS               0x021D
#define IDMEN_FOLDER_DEFAULT              0x021E
#define IDMEN_FOLDER_DEMO                 0x021F
#define IDMEN_FOLDER_LEFTHANDED           0x0220
#define IDMEN_FOLDER_HIDEPOINTER          0x0221
#define IDMEN_FOLDER_BLACKWHITE           0x0222
#define IDMEN_FOLDER_ANIMATE              0x0223

// --- res id for strings

#define IDSTR_VERSION                     0x4000
#define IDSTR_FILETYPE_DEFAULT            0x0001
#define IDSTR_FILETYPE_POINTER            0x0002
#define IDSTR_FILETYPE_POINTERSET         0x0003
#define IDSTR_FILETYPE_CURSOR             0x0004
#define IDSTR_FILETYPE_CURSORSET          0x0005
#define IDSTR_FILETYPE_ANIMOUSE           0x0006
#define IDSTR_FILETYPE_ANIMOUSESET        0x0007
#define IDSTR_FILETYPE_WINANIMATION       0x0008
#define IDSTR_FILETYPE_WINANIMATIONSET    0x0009
#define IDSTR_FILETYPE_ANIMATIONSETDIR    0x000A
#define IDSTR_FILETYPE_ALL                0x000B

#define IDSTR_POINTER_ARROW               0x0010
#define IDSTR_POINTER_TEXT                0x0011
#define IDSTR_POINTER_WAIT                0x0012
#define IDSTR_POINTER_SIZENWSE            0x0013
#define IDSTR_POINTER_SIZEWE              0x0014
#define IDSTR_POINTER_MOVE                0x0015
#define IDSTR_POINTER_SIZENESW            0x0016
#define IDSTR_POINTER_SIZENS              0x0017
#define IDSTR_POINTER_ILLEGAL             0x0018

#define IDSTR_TITLE_ICON                  0x0020
#define IDSTR_TITLE_NAME                  0x0021
#define IDSTR_TITLE_STATUS                0x0022
#define IDSTR_TITLE_ANIMATIONTYPE         0x0023
#define IDSTR_TITLE_POINTER               0x0024
#define IDSTR_TITLE_ANIMATIONNAME         0x0025
#define IDSTR_TITLE_FRAMERATE             0x0026
#define IDSTR_TITLE_INFONAME              0x0027
#define IDSTR_TITLE_INFOARTIST            0x0028
#define IDSTR_STATUS_ON                   0x0029
#define IDSTR_STATUS_OFF                  0x002A

#define IDSTR_TITLE_IMAGE                 0x0030
#define IDSTR_SELECTED_NONE               0x0031
#define IDSTR_SELECTED_FRAME              0x0032
#define IDSTR_SELECTED_FRAMES             0x0033
#define IDSTR_UNIT_MILLISECONDS           0x0034
#define IDSTR_UNIT_JIFFIES                0x0035


// --- res id for messages

#define IDMSG_TITLE_ERROR                 0x0001
#define IDMSG_ANIMATIONPATH_NOT_FOUND     0x0002
#define IDMSG_TITLENOTFOUND               0x0003
#define IDMSG_MSGNOTFOUND                 0x0004

// online Help Id for DisplayHelp
#define IDPNL_MAIN                     1
#define IDPNL_USAGE_NBPAGE             2
#define IDPNL_USAGE_NBPAGE_CNRSETTINGS 3
#define IDPNL_USAGE_NBPAGE_CNRLOADSET  4

// --- res ids for container item popup menu

// define submenu ids for submenu of every resource WPS class
#define IDMEN_CONVERT_MENU                0x7111

// define menuitem ids
#define IDMEN_CONVERT_POINTER             0x7120
#define IDMEN_CONVERT_CURSOR              0x7121
#define IDMEN_CONVERT_WINANIMATION        0x7122
#define IDMEN_CONVERT_ANIMOUSE            0x7123

#endif // WPAMPTR_RCH

