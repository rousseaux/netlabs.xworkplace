
/*
 *@@sourcefile xview.h:
 *      header file for xview.c.
 *
 *      This file is new with V1.0.11
 *
 */

/*
 *      Copyright (C) 2001-2016 Ulrich M”ller.
 *      Copyright (C) 2016 Rich Walsh.
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

#ifndef XVIEW_HEADER_INCLUDED
    #define XVIEW_HEADER_INCLUDED

    HWND xvwCreateXview(WPObject *pRootObject,
                        WPFolder *pRootsFolder,
                        ULONG ulView);

    // pv is actually a PFDRSPLITVIEW
    void xvwInitXview(void* pv);

#ifdef XVIEW_SRC

    /*
     *@@ XVIEWDATA:
     *
     */
    
    typedef struct _XVIEWDATA
    {
        USHORT              cb;
    
        USEITEM             ui;
        VIEWITEM            vi;
    
        CHAR                szFolderPosKey[10];
    
        FDRSPLITVIEW        sv;         // pRootFolder == somSelf
    
        PSUBCLFOLDERVIEW    psfvBar;    // SUBCLFOLDERVIEW of either the tree
                                        // or the files frame for menu bar support
    
        SIZEF               szfCharBox; // an unfortunate hack needed to ensure menu
                                        // text isn't mis-sized after drawing the
                                        // Options submenu's blank separators
                                        // (it's here rather than in FDRSPLITVIEW
                                        // to avoid modifying filedlg.c)
    } XVIEWDATA, *PXVIEWDATA;
    
    /*
     *@@ XVIEWPOS:
     *
     */
    
    typedef struct _XVIEWPOS
    {
        LONG        x,
                    y,
                    cx,
                    cy;

        LONG        lSplitBarPos;

        ULONG       flState;

        ULONG       flIconAttr;

        ULONG       flColumns;

        LONG        lCnrSplitPos;

    } XVIEWPOS, *PXVIEWPOS;

    MRESULT EXPENTRY fnwpXviewFrame(HWND hwndFrame, ULONG msg,
                                    MPARAM mp1, MPARAM mp2);

#endif
#endif

