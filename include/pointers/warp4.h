
#ifndef WARP4_H
#define WARP4_H

//
//    Definitions of WARP 4 Toolkit not included in WARP 3 Toolkit
//


#ifndef CCS_MINIICONS
/* Style to have container support mini icons with the minirecord. */
#define CCS_MINIICONS             0x00000800L
#endif

#ifndef BS_NOTEBOOKBUTTON
#define BS_NOTEBOOKBUTTON          8L
#endif

#ifndef WinShowControl
#define WinShowControl(hwndDlg, id, fShow) \
WinShowWindow(WinWindowFromID(hwndDlg, id), fShow)
#endif

#ifndef OBJSTYLE_LOCKEDINPLACE
#define OBJSTYLE_LOCKEDINPLACE  0x00020000
#endif

#endif // WARP4_H

