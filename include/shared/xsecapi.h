
/*
 *@@sourcefile xsecapi.h:
 *      header file for xsecapi.c. See remarks there.
 *
 *@@include #include <os2.h>
 *@@include #include "security\xwpsecty.h"
 *@@include #include "shared\xsecapi.h"
 */

/*
 *      Copyright (C) 2001-2002 Ulrich M”ller.
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, in version 2 as it comes in the COPYING
 *      file of the XWorkplace main distribution.
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 */

#ifndef XSECAPI_HEADER_INCLUDED
    #define XSECAPI_HEADER_INCLUDED

    APIRET xsecQueryLocalLoggedOn(PXWPLOGGEDON pLoggedOn);

    APIRET xsecQueryUsers(PULONG pcUsers,
                          PXWPUSERDBENTRY *ppaUsers);

    APIRET xsecQueryGroups(PULONG pcGroups,
                           PXWPGROUPDBENTRY *ppaGroups);

    APIRET xsecQueryProcessOwner(ULONG ulPID,
                                 XWPSECID *puid);

#endif
