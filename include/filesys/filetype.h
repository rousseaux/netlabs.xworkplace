
/*
 *@@sourcefile filesys.h:
 *      header file for filetype.c (extended file types implementation).
 *
 *      This file is ALL new with V0.9.0.
 *
 *@@include #include <os2.h>
 *@@include #include "shared\notebook.h"
 *@@include #include "filesys\filetype.h"
 *@@include #include "classes\xfdataf.h"            // for method implementations
 */

/*
 *      Copyright (C) 1997-99 Ulrich M”ller.
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

#ifndef FILETYPE_HEADER_INCLUDED
    #define FILETYPE_HEADER_INCLUDED

    /* ******************************************************************
     *                                                                  *
     *   Notebook callbacks (notebook.c) for XFldWPS "File types" page  *
     *                                                                  *
     ********************************************************************/

    #ifdef NOTEBOOK_HEADER_INCLUDED
        VOID ftypFileTypesInitPage(PCREATENOTEBOOKPAGE pcnbp,
                                      ULONG flFlags);

        MRESULT ftypFileTypesItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                            USHORT usItemID,
                                            USHORT usNotifyCode,
                                            ULONG ulExtra);
    #else
        #error "shared\notebook.h needs to be included before including filetype.h".
    #endif

    /* ******************************************************************
     *                                                                  *
     *   XFldDataFile extended associations                             *
     *                                                                  *
     ********************************************************************/

    #ifdef SOM_XFldDataFile_h
        /* PSZ ftypQueryType(WPDataFile *somSelf,
                          PSZ pszOriginalTypes,
                          PCGLOBALSETTINGS pGlobalSettings); */

        ULONG ftypBuildAssocsList(XFldDataFile *somSelf,
                                  BOOL fRefresh);

        WPObject* ftypQueryAssociatedProgram(XFldDataFile *somSelf,
                                             ULONG ulView,
                                             PULONG pulHowMatched,
                                             PSZ pszMatchString,
                                             ULONG cbMatchString,
                                             PSZ pszDefaultType);

        BOOL ftypModifyDataFileOpenSubmenu(WPDataFile *somSelf,
                                           HWND hwndMenu);
    #endif

#endif


