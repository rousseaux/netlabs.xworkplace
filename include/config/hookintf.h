
/*
 *@@sourcefile hookintf.h:
 *      header file for hookintf.c (daemon/hook interface).
 *
 *@@include #include <os2.h>
 *@@include #include "shared\notebook.h"
 *@@include #include "hook\xwphook.h"
 *@@include #include "setup\hookintf.h"
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

#ifndef HOOKINTF_HEADER_INCLUDED
    #define HOOKINTF_HEADER_INCLUDED

    /********************************************************************
     *                                                                  *
     *   Declarations                                                   *
     *                                                                  *
     ********************************************************************/

    #ifdef XWPHOOK_HEADER_INCLUDED
        /*
         *@@ HOTKEYRECORD:
         *      extended record core used for
         *      hotkey definitions in "Hotkeys"
         *      settings page.
         */

        typedef struct _HOTKEYRECORD
        {
            RECORDCORE  recc;

            ULONG       ulIndex;

            // object handle
            PSZ         pszHandle;          // points to szHandle
            CHAR        szHandle[10];       // string representation of GLOBALHOTKEY handle member

            // folder of object
            PSZ         pszFolderPath;      // points to szFolderPath
            CHAR        szFolderPath[CCHMAXPATH];

            // hotkey description
            PSZ         pszHotkey;          // points to szHotkey
            CHAR        szHotkey[200];

            // hotkey data; this contains the object handle
            GLOBALHOTKEY Hotkey;

            // object ptr
            WPObject    *pObject;
        } HOTKEYRECORD, *PHOTKEYRECORD;
    #endif

    /********************************************************************
     *                                                                  *
     *   XWorkplace daemon/hook interface                               *
     *                                                                  *
     ********************************************************************/

    BOOL hifEnableHook(BOOL fEnable);

    BOOL hifXWPHookReady(VOID);

    BOOL hifEnablePageMage(BOOL fEnable);

    BOOL hifObjectHotkeysEnabled(VOID);

    VOID hifEnableObjectHotkeys(BOOL fEnable);

    PVOID hifQueryObjectHotkeys(PULONG pcHotkeys);

    VOID hifFreeObjectHotkeys(PVOID pvHotkeys);

    BOOL hifSetObjectHotkeys(PVOID pvHotkeys, ULONG cHotkeys);

    BOOL hifHookConfigChanged(PVOID pvdc);

    #ifdef NOTEBOOK_HEADER_INCLUDED

        /* ******************************************************************
         *                                                                  *
         *   XWPKeyboard notebook callbacks (notebook.c)                    *
         *                                                                  *
         ********************************************************************/

        VOID hifKeybdHotkeysInitPage(PCREATENOTEBOOKPAGE pcnbp,
                                     ULONG flFlags);

        MRESULT hifKeybdHotkeysItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                           USHORT usItemID, USHORT usNotifyCode,
                                           ULONG ulExtra);

        /* ******************************************************************
         *                                                                  *
         *   XWPMouse notebook callbacks (notebook.c)                       *
         *                                                                  *
         ********************************************************************/

        VOID hifMouseMappings2InitPage(PCREATENOTEBOOKPAGE pcnbp,
                                       ULONG flFlags);

        MRESULT hifMouseMappings2ItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                             USHORT usItemID, USHORT usNotifyCode,
                                             ULONG ulExtra);

        VOID hifMouseCornersInitPage(PCREATENOTEBOOKPAGE pcnbp,
                                      ULONG flFlags);

        MRESULT hifMouseCornersItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                            USHORT usItemID, USHORT usNotifyCode,
                                            ULONG ulExtra);

        VOID hifMouseMovementInitPage(PCREATENOTEBOOKPAGE pcnbp,
                                      ULONG flFlags);

        MRESULT hifMouseMovementItemChanged(PCREATENOTEBOOKPAGE pcnbp,
                                            USHORT usItemID, USHORT usNotifyCode,
                                            ULONG ulExtra);
    #endif

#endif

